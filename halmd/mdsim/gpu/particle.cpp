/*
 * Copyright © 2016      Daniel Kirchner
 * Copyright © 2010-2012 Felix Höfling
 * Copyright © 2013      Nicolas Höft
 * Copyright © 2008-2012 Peter Colberg
 *
 * This file is part of HALMD.
 *
 * HALMD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <halmd/config.hpp>

#include <halmd/algorithm/gpu/iota.hpp>
#include <halmd/algorithm/gpu/radix_sort.hpp>
#include <halmd/io/logger.hpp>
#include <halmd/mdsim/gpu/particle.hpp>
#include <halmd/mdsim/gpu/particle_kernel.hpp>
#include <halmd/mdsim/gpu/velocity.hpp>
#include <halmd/utility/gpu/device.hpp>
#include <halmd/utility/lua/lua.hpp>
#include <halmd/utility/signal.hpp>

#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <luaponte/out_value_policy.hpp>

#include <algorithm>
#include <exception>
#include <iterator>
#include <numeric>

namespace halmd {
namespace mdsim {
namespace gpu {

/**
 * Allocate microscopic system state.
 *
 * @param particles number of particles per type or species
 */
template <int dimension, typename float_type>
particle<dimension, float_type>::particle(size_type nparticle, unsigned int nspecies)
  : // allocate global device memory
    nparticle_(nparticle)
  , array_size_((nparticle + 128 - 1) & ~(128-1))
  , nspecies_(std::max(nspecies, 1u))
  // FIXME default CUDA kernel execution dimensions
  , dim_(device::validate(cuda::config(array_size_/128, 128)))
  // enable auxiliary variables by default to allow sampling of initial state
  , force_zero_(true)
  , force_dirty_(true)
  , aux_dirty_(true)
  , aux_enabled_(true)
{
    // register particle arrays
    auto position_array = register_data<gpu_position_type>("g_position");
    auto image_array = register_data<gpu_image_type>("g_image");
    auto velocity_array = register_data<gpu_velocity_type>("g_velocity");
    auto id_array = register_data<gpu_id_type>("g_id");
    auto reverse_id_array = register_data<gpu_reverse_id_type>("g_reverse_id");
    auto force_array = register_data<gpu_force_type>("g_force", [this]() { this->update_force_(); });
    auto en_pot_array = register_data<gpu_en_pot_type>("g_en_pot", [this]() { this->update_force_(true); });
    auto stress_pot_array = register_data<gpu_stress_pot_type>("g_stress_pot", [this]() { this->update_force_(true); });

    // register host data wrappers for packed data
    register_packed_data_wrapper<tuple<position_type, species_type>, 0>("position", position_array);
    register_packed_data_wrapper<tuple<position_type, species_type>, 1>("species", position_array);
    register_packed_data_wrapper<tuple<velocity_type, mass_type>, 0>("velocity", velocity_array);
    register_packed_data_wrapper<tuple<velocity_type, mass_type>, 1>("mass", velocity_array);

    // register host wrappers for other data
    register_host_data_wrapper<force_type>("force", force_array);
    register_host_data_wrapper<image_type>("image", image_array);
    register_host_data_wrapper<id_type>("id", id_array);
    register_host_data_wrapper<reverse_id_type>("reverse_id", reverse_id_array);
    register_host_data_wrapper<en_pot_type>("en_pot", en_pot_array);
    register_host_data_wrapper<stress_pot_type>("stress_pot", stress_pot_array);

    // create alias for potential energy
    data_["potential_energy"] = data_["en_pot"];

    // get access to the underlying cuda vectors for initialization
    auto g_position = make_cache_mutable(position_array->mutable_data());
    auto g_image = make_cache_mutable(image_array->mutable_data());
    auto g_velocity = make_cache_mutable(velocity_array->mutable_data());
    auto g_id = make_cache_mutable(id_array->mutable_data());
    auto g_reverse_id = make_cache_mutable(reverse_id_array->mutable_data());
    auto g_force = make_cache_mutable(force_array->mutable_data());
    auto g_en_pot = make_cache_mutable(en_pot_array->mutable_data());
    auto g_stress_pot = make_cache_mutable(stress_pot_array->mutable_data());

    //
    // The GPU stores the stress tensor elements in column-major order to
    // optimise access patterns for coalescable access. Increase capacity of
    // GPU array such that there are 4 (6) in 2D (3D) elements per particle
    // available, although stress_pot_->size() still returns the number of
    // particles.
    //
    g_stress_pot->reserve(stress_pot_type::static_size * array_size_);

    LOG_DEBUG("number of CUDA execution blocks: " << dim_.blocks_per_grid());
    LOG_DEBUG("number of CUDA execution threads per block: " << dim_.threads_per_block());

    if (typeid(float_type) == typeid(float)) {
        LOG_WARNING("integrate using single precision");
    }

    // initialise 'ghost' particles to zero and sets their species to -1U
    // this avoids potential nonsense computations resulting in denormalised numbers
    cuda::configure(dim_.grid, dim_.block);
    get_particle_kernel<dimension, float_type>().initialize(g_position->data(), g_velocity->data(), nparticle_);
    cuda::memset(g_image->begin(), g_image->begin() + g_image->capacity(), 0);
    iota(g_id->begin(), g_id->begin() + g_id->capacity(), 0);
    iota(g_reverse_id->begin(), g_reverse_id->begin() + g_reverse_id->capacity(), 0);
    cuda::memset(g_force->begin(), g_force->begin() + g_force->capacity(), 0);
    cuda::memset(g_en_pot->begin(), g_en_pot->begin() + g_en_pot->capacity(), 0);
    cuda::memset(g_stress_pot->begin(), g_stress_pot->begin() + g_stress_pot->capacity(), 0);

    try {
        cuda::copy(nparticle_, get_particle_kernel<dimension, float_type>().nbox);
        cuda::copy(nspecies_, get_particle_kernel<dimension, float_type>().ntype);
    }
    catch (cuda::error const&) {
        LOG_ERROR("failed to copy particle parameters to device symbols");
        throw;
    }

    LOG("number of particles: " << nparticle_);
    LOG("number of particle placeholders: " << array_size_);
    LOG("number of particle species: " << nspecies_);
}

template <int dimension, typename float_type>
void particle<dimension, float_type>::aux_enable()
{
    LOG_TRACE("enable computation of auxiliary variables");
    aux_enabled_ = true;
}

/**
 * rearrange particles by permutation
 */
template <int dimension, typename float_type>
void particle<dimension, float_type>::rearrange(cuda::vector<unsigned int> const& g_index)
{
    auto g_position = make_cache_mutable(mutable_data<gpu_position_type>("g_position"));
    auto g_image = make_cache_mutable(mutable_data<gpu_image_type>("g_image"));
    auto g_velocity = make_cache_mutable(mutable_data<gpu_velocity_type>("g_velocity"));
    auto g_id = make_cache_mutable(mutable_data<gpu_id_type>("g_id"));
    auto g_reverse_id = make_cache_mutable(mutable_data<gpu_reverse_id_type>("g_reverse_id"));

    scoped_timer_type timer(runtime_.rearrange);

    position_array_type position(array_size_);
    image_array_type image(array_size_);
    velocity_array_type velocity(array_size_);
    id_array_type id(array_size_);

    cuda::configure(dim_.grid, dim_.block);
    get_particle_kernel<dimension, float_type>().r.bind(*g_position);
    get_particle_kernel<dimension, float_type>().image.bind(*g_image);
    get_particle_kernel<dimension, float_type>().v.bind(*g_velocity);
    get_particle_kernel<dimension, float_type>().id.bind(*g_id);
    get_particle_kernel<dimension, float_type>().rearrange(g_index, position, image, velocity, id, nparticle_);

    position.swap(*g_position);
    image.swap(*g_image);
    velocity.swap(*g_velocity);
    cuda::copy(id.begin(), id.begin() + id.capacity(), g_id->begin());

    iota(g_reverse_id->begin(), g_reverse_id->begin() + g_reverse_id->capacity(), 0);
    radix_sort(id.begin(), id.begin() + nparticle_, g_reverse_id->begin());
}

template <int dimension, typename float_type>
void particle<dimension, float_type>::update_force_(bool with_aux)
{
    on_prepend_force_();          // ask force modules whether force/aux cache is dirty

    if (force_dirty_ || (with_aux && aux_dirty_)) {
        if (with_aux && aux_dirty_) {
            if (!force_dirty_) {
                LOG_WARNING_ONCE("auxiliary variables inactive in prior force computation, use aux_enable()");
            }
            aux_enabled_ = true;  // turn on computation of aux variables
        }
        LOG_TRACE("request force" << std::string(aux_enabled_ ? " and auxiliary variables" : ""));

        force_zero_ = true;       // tell first force module to reset the force
        on_force_();              // compute forces
        force_dirty_ = false;     // mark force cache as clean
        if (aux_enabled_) {
            aux_dirty_ = false;   // aux cache is clean only if requested
        }
        aux_enabled_ = false;     // disable aux variables for next call
    }
    on_append_force_();
}

template <int dimension, typename float_type>
static int wrap_dimension(particle<dimension, float_type> const&)
{
    return dimension;
}

template <typename particle_type>
static luaponte::object wrap_get(particle_type const& particle, lua_State* L, std::string const& name)
{
    return particle.get_array(name)->get_lua(L);
}

template <typename particle_type>
static void wrap_set(particle_type const& particle, std::string const& name, luaponte::object object)
{
    particle.get_array(name)->set_lua(object);
}

template <typename T>
static bool equal(std::shared_ptr<T const> self, std::shared_ptr<T const> other)
{
    // compare pointers of managed objects. I could not get working
    // owner_equal() or owner_before() with shared_ptr's passed from Lua
    return self == other;
}

template<typename float_type>
struct variant_name;

template<>
struct variant_name<float>
{
    static constexpr const char *name = "float";
};

template<>
struct variant_name<dsfloat>
{
    static constexpr const char *name = "dsfloat";
};

template <int dimension, typename float_type>
void particle<dimension, float_type>::luaopen(lua_State* L)
{
    using namespace luaponte;
    static std::string class_name = "particle_" + std::string(variant_name<float_type>::name) + "_" + std::to_string(dimension);
    module(L, "libhalmd")
    [
        namespace_("mdsim")
        [
            namespace_("gpu")
            [
                class_<particle, std::shared_ptr<particle>>(class_name.c_str())
                    .def(constructor<size_type, unsigned int>())
                    .property("nparticle", &particle::nparticle)
                    .property("array_size", &particle::array_size)
                    .property("nspecies", &particle::nspecies)
                    .def("get", &wrap_get<particle>)
                    .def("set", &wrap_set<particle>)
                    .def("shift_velocity", &shift_velocity<particle>)
                    .def("shift_velocity_group", &shift_velocity_group<particle>)
                    .def("rescale_velocity", &rescale_velocity<particle>)
                    .def("rescale_velocity_group", &rescale_velocity_group<particle>)
                    .def("shift_rescale_velocity", &shift_rescale_velocity<particle>)
                    .def("shift_rescale_velocity_group", &shift_rescale_velocity_group<particle>)
                    .property("dimension", &wrap_dimension<dimension, float_type>)
                    .def("aux_enable", &particle::aux_enable)
                    .def("on_prepend_force", &particle::on_prepend_force)
                    .def("on_force", &particle::on_force)
                    .def("on_append_force", &particle::on_append_force)
                    .def("__eq", &equal<particle>) // operator= in Lua
                    .scope
                    [
                        class_<runtime>("runtime")
                            .def_readonly("rearrange", &runtime::rearrange)
                    ]
                    .def_readonly("runtime", &particle::runtime_)
            ]
        ]
    ];
}

HALMD_LUA_API int luaopen_libhalmd_mdsim_gpu_particle(lua_State* L)
{
    particle<3, float>::luaopen(L);
    particle<2, float>::luaopen(L);
    particle<3, dsfloat>::luaopen(L);
    particle<2, dsfloat>::luaopen(L);
    return 0;
}

// explicit instantiation
template class particle<3, float>;
template class particle<2, float>;
template class particle<3, dsfloat>;
template class particle<2, dsfloat>;

} // namespace gpu
} // namespace mdsim
} // namespace halmd
