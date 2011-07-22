/*
 * Copyright © 2008-2011  Peter Colberg
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

#include <halmd/mdsim/host/integrators/verlet.hpp>
#include <halmd/utility/lua/lua.hpp>

using namespace boost;
using namespace std;

namespace halmd {
namespace mdsim {
namespace host {
namespace integrators {

template <int dimension, typename float_type>
verlet<dimension, float_type>::verlet(
    shared_ptr<particle_type> particle
  , shared_ptr<box_type const> box
  , double timestep
  , shared_ptr<logger_type> logger
)
  // dependency injection
  : particle_(particle)
  , box_(box)
  , logger_(logger)
{
    this->timestep(timestep);
}

template <int dimension, typename float_type>
void verlet<dimension, float_type>::timestep(double timestep)
{
  timestep_ = timestep;
  timestep_half_ = 0.5 * timestep;

  LOG("integration timestep: " << timestep_);
}

/**
 * First leapfrog half-step of velocity-Verlet algorithm
 */
template <int dimension, typename float_type>
void verlet<dimension, float_type>::integrate()
{
    scoped_timer_type timer(runtime_.integrate);
    for (size_t i = 0; i < particle_->nbox; ++i) {
        vector_type& v = particle_->v[i] += particle_->f[i] * timestep_half_;
        vector_type& r = particle_->r[i] += v * timestep_;
        // enforce periodic boundary conditions
        // TODO: reduction is now to (-L/2, L/2) instead of (0, L) as before
        // check that this is OK
        particle_->image[i] += box_->reduce_periodic(r);
    }
}

/**
 * Second leapfrog half-step of velocity-Verlet algorithm
 */
template <int dimension, typename float_type>
void verlet<dimension, float_type>::finalize()
{
    scoped_timer_type timer(runtime_.finalize);
    for (size_t i = 0; i < particle_->nbox; ++i) {
        particle_->v[i] += particle_->f[i] * timestep_half_;
    }
}

template <int dimension, typename float_type>
static char const* module_name_wrapper(verlet<dimension, float_type> const&)
{
    return verlet<dimension, float_type>::module_name();
}

template <int dimension, typename float_type>
void verlet<dimension, float_type>::luaopen(lua_State* L)
{
    using namespace luabind;
    static string class_name(module_name() + ("_" + lexical_cast<string>(dimension) + "_"));
    module(L, "libhalmd")
    [
        namespace_("mdsim")
        [
            namespace_("host")
            [
                namespace_("integrators")
                [
                    class_<verlet, shared_ptr<_Base>, _Base>(class_name.c_str())
                        .def(constructor<
                            shared_ptr<particle_type>
                          , shared_ptr<box_type const>
                          , double
                          , shared_ptr<logger_type>
                        >())
                        .property("module_name", &module_name_wrapper<dimension, float_type>)
                        .scope
                        [
                            class_<runtime>("runtime")
                                .def_readonly("integrate", &runtime::integrate)
                                .def_readonly("finalize", &runtime::finalize)
                        ]
                        .def_readonly("runtime", &verlet::runtime_)
                ]
            ]
        ]
    ];
}

HALMD_LUA_API int luaopen_libhalmd_mdsim_host_integrators_verlet(lua_State* L)
{
#ifndef USE_HOST_SINGLE_PRECISION
    verlet<3, double>::luaopen(L);
    verlet<2, double>::luaopen(L);
#else
    verlet<3, float>::luaopen(L);
    verlet<2, float>::luaopen(L);
#endif
    return 0;
}

// explicit instantiation
#ifndef USE_HOST_SINGLE_PRECISION
template class verlet<3, double>;
template class verlet<2, double>;
#else
template class verlet<3, float>;
template class verlet<2, float>;
#endif

} // namespace mdsim
} // namespace host
} // namespace integrators
} // namespace halmd
