/*
 * Copyright © 2008-2011  Peter Colberg and Felix Höfling
 *
 * This file is part of HALMD.
 *
 * HALMD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <cmath>
#include <string>

#include <halmd/algorithm/gpu/reduce.hpp>
#include <halmd/io/logger.hpp>
#include <halmd/mdsim/gpu/integrators/verlet_nvt_hoover.hpp>
#include <halmd/utility/lua_wrapper/lua_wrapper.hpp>
#include <halmd/utility/scoped_timer.hpp>
#include <halmd/utility/timer.hpp>

using namespace boost;
using namespace halmd::algorithm::gpu;
using namespace std;
using boost::fusion::at_key;

namespace halmd
{
namespace mdsim { namespace gpu { namespace integrators
{

template <int dimension, typename float_type>
verlet_nvt_hoover<dimension, float_type>::
verlet_nvt_hoover(
    shared_ptr<particle_type> particle
  , shared_ptr<box_type> box
  , float_type timestep, float_type temperature
  , fixed_vector<float_type, 2>  const& mass
)
  // dependency injection
  : particle(particle)
  , box(box)
  // member initialisation
  , xi(0)
  , v_xi(0)
  , mass_xi_(mass)
{
    this->timestep(timestep);
    this->temperature(temperature);
    LOG("`mass' of heat bath variables: " << mass_xi_);

#ifdef USE_VERLET_DSFUN
    //
    // Double-single precision requires two single precision
    // "words" per coordinate. We use the first part of a GPU
    // vector for the higher (most significant) words of all
    // particle positions or velocities, and the second part for
    // the lower (least significant) words.
    //
    // The additional memory is allocated using reserve(), which
    // increases the capacity() without changing the size().
    //
    // Take care to pass capacity() as an argument to cuda::copy
    // or cuda::memset calls if needed, as the lower words will
    // be ignored in the operation.
    //
    LOG("using velocity-Verlet integration in double-single precision");
    particle->g_r.reserve(2 * particle->dim.threads());
    // particle images remain in single precision as they
    // contain integer values (and otherwise would not matter
    // for the long-time stability of the Verlet integrator)
    particle->g_v.reserve(2 * particle->dim.threads());
#else
    LOG_WARNING("using velocity-Verlet integration in single precision");
#endif

    // copy parameters to CUDA device
    try {
        cuda::copy(
            static_cast<fixed_vector<float, dimension> >(box->length())
          , wrapper_type::kernel.box_length
        );
    }
    catch (cuda::error const& e) {
        LOG_ERROR(e.what());
        throw runtime_error("failed to initialize Verlet integrator symbols");
    }
}

/**
 * register module runtime accumulators
 */
template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::
register_runtimes(profiler_type& profiler)
{
    profiler.register_map(runtime_);
}

/**
 * set integration time-step
 */
template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::
timestep(double timestep)
{
    timestep_ = static_cast<float_type>(timestep);
    timestep_half_ = timestep_ / 2;
    timestep_4_ = timestep_ / 4;
    timestep_8_ = timestep_ / 8;

    try {
        cuda::copy(timestep_, wrapper_type::kernel.timestep);
    }
    catch (cuda::error const& e) {
        LOG_ERROR(e.what());
        throw runtime_error("failed to initialize Verlet integrator symbols");
    }

    LOG("integration timestep: " << timestep_);
}

template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::
temperature(double temperature)
{
  temperature_ = static_cast<float_type>(temperature);
  en_kin_target_2_ = dimension * particle->nbox * temperature_;

  LOG("temperature of heat bath: " << temperature_);
  LOG("target kinetic energy: " << en_kin_target_2_ / particle->nbox);
}

/**
 * First leapfrog half-step of velocity-Verlet algorithm
 */
template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::
integrate()
{
    float_type scale = propagate_chain();

    try {
        scoped_timer<timer> timer_(at_key<integrate_>(runtime_));
        cuda::configure(particle->dim.grid, particle->dim.block);
        wrapper_type::kernel.integrate(
            particle->g_r, particle->g_image, particle->g_v, particle->g_f, scale
        );
        cuda::thread::synchronize();
    }
    catch (cuda::error const& e) {
        LOG_ERROR("CUDA: " << e.what());
        throw std::runtime_error("failed to stream first leapfrog step on GPU");
    }
}

/**
 * Second leapfrog half-step of velocity-Verlet algorithm
 */
template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::
finalize()
{
    scoped_timer<timer> timer_(at_key<finalize_>(runtime_));

    // TODO: possibly a performance critical issue:
    // the old implementation had this loop included in update_forces(),
    // which saves one additional read of the forces plus the additional kernel execution
    // and scheduling
    try {
        cuda::configure(particle->dim.grid, particle->dim.block);
        wrapper_type::kernel.finalize(particle->g_v, particle->g_f);
        cuda::thread::synchronize();

        float_type scale = propagate_chain();

        // rescale velocities
        cuda::configure(particle->dim.grid, particle->dim.block);
        wrapper_type::kernel.rescale(particle->g_v, scale);
        cuda::thread::synchronize();
    }
    catch (cuda::error const& e) {
        LOG_ERROR("CUDA: " << e.what());
        throw std::runtime_error("failed to stream second leapfrog step on GPU");
    }
}

/**
 * propagate Nosé-Hoover chain
 */
template <int dimension, typename float_type>
float_type verlet_nvt_hoover<dimension, float_type>::propagate_chain()
{
    float_type en_kin_2 = compute_en_kin_2();

    // head of the chain
    v_xi[1] += (mass_xi_[0] * v_xi[0] * v_xi[0] - temperature_) * timestep_4_;
    float_type t = exp(-v_xi[1] * timestep_8_);
    v_xi[0] *= t;
    v_xi[0] += (en_kin_2 - en_kin_target_2_) / mass_xi_[0] * timestep_4_;
    v_xi[0] *= t;

    // propagate heat bath variables
    for (unsigned int i = 0; i < 2; ++i ) {
        xi[i] += v_xi[i] * timestep_half_;
    }

    // rescale velocities and kinetic energy
    // we only compute the factor, the rescaling is done elsewhere
    float_type s = exp(-v_xi[0] * timestep_half_);
    en_kin_2 *= s * s;

    // tail of the chain, (almost) mirrors the head
    v_xi[0] *= t;
    v_xi[0] += (en_kin_2 - en_kin_target_2_) / mass_xi_[0] * timestep_4_;
    v_xi[0] *= t;
    v_xi[1] += (mass_xi_[0] * v_xi[0] * v_xi[0] - temperature_) / mass_xi_[1] * timestep_4_;

    // return scaling factor for CUDA kernels
    return s;
}

/**
 * compute actual value of total kinetic energy (multiplied by 2)
 */
template <int dimension, typename float_type>
float_type verlet_nvt_hoover<dimension, float_type>::compute_en_kin_2() const
{
    // compute total kinetic energy (multiplied by 2)
    return reduce<
        sum_                                    // reduce_transform
      , fixed_vector<float, dimension>          // input_type
      , float4                                  // coalesced_input_type
      , dsfloat                                 // output_type
      , dsfloat                                 // coalesced_output_type
      , float_type                              // host_output_type
      , square_                                 // input_transform
    >()(particle->g_v);
}

template <int dimension, typename float_type>
static char const* module_name_wrapper(verlet_nvt_hoover<dimension, float_type> const&)
{
    return verlet_nvt_hoover<dimension, float_type>::module_name();
}

template <int dimension, typename float_type>
void verlet_nvt_hoover<dimension, float_type>::
luaopen(lua_State* L)
{
    typedef typename _Base::_Base _Base_Base;
    using namespace luabind;
    static string class_name(module_name() + ("_" + lexical_cast<string>(dimension) + "_"));
    module(L)
    [
        namespace_("halmd_wrapper")
        [
            namespace_("mdsim")
            [
                namespace_("gpu")
                [
                    namespace_("integrators")
                    [
                        class_<
                            verlet_nvt_hoover
                          , shared_ptr<_Base_Base>
                          , bases<_Base_Base, _Base>
                        >(class_name.c_str())
                            .def(constructor<
                                shared_ptr<particle_type>
                              , shared_ptr<box_type>
                              , float_type, float_type
                              , fixed_vector<float_type, 2> const&
                            >())
                            .def("register_runtimes", &verlet_nvt_hoover::register_runtimes)
                            .property("mass", &verlet_nvt_hoover::mass)
                            .property("module_name", &module_name_wrapper<dimension, float_type>)
                    ]
                ]
            ]
        ]
    ];
}

static __attribute__((constructor)) void register_lua()
{
    lua_wrapper::register_(2) //< distance of derived to base class
    [
#ifdef USE_VERLET_DSFUN
        &verlet_nvt_hoover<3, double>::luaopen
#else
        &verlet_nvt_hoover<3, float>::luaopen
#endif
    ]
    [
#ifdef USE_VERLET_DSFUN
        &verlet_nvt_hoover<2, double>::luaopen
#else
        &verlet_nvt_hoover<2, float>::luaopen
#endif
    ];
}

// explicit instantiation
#ifdef USE_VERLET_DSFUN
template class verlet_nvt_hoover<3, double>;
template class verlet_nvt_hoover<2, double>;
#else
template class verlet_nvt_hoover<3, float>;
template class verlet_nvt_hoover<2, float>;
#endif

}}} // namespace mdsim::gpu::integrators

} // namespace halmd
