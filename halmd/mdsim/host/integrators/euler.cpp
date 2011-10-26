/*
 * Copyright © 2011 Michael Kopp
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

#include <halmd/mdsim/host/integrators/euler.hpp>
#include <halmd/utility/lua/lua.hpp>
#include <halmd/utility/scoped_timer.hpp>
#include <halmd/utility/timer.hpp>

using namespace boost;
using namespace std;

namespace halmd {
namespace mdsim {
namespace host {
namespace integrators {

// constructor
template <int dimension, typename float_type>
euler<dimension, float_type>::euler(
    shared_ptr<particle_type> particle
  , shared_ptr<box_type const> box
  , double timestep
  , shared_ptr<logger_type> logger
)
  // dependency injection (initialize public variables)
  : particle_(particle)
  , box_(box)
  , logger_(logger)
{
    this->timestep(timestep);
}

/**
 * Set timestep.
 */
template <int dimension, typename float_type>
void euler<dimension, float_type>::timestep(double timestep)
{
  timestep_ = timestep;

  LOG("integration timestep: " << timestep_);
}

/**
 * Perform Euler integration: if v is set, update r.
 */
template <int dimension, typename float_type>
void euler<dimension, float_type>::integrate()
{
    scoped_timer_type timer(runtime_.integrate);
    for( size_t i = 0 ; i < particle_->nbox; ++i)
    {
      vector_type& asdf = particle_->r[i] += particle_->v[i] * timestep_;
      // enforce periodic boundary conditions
      particle_->image[i] += box_->reduce_periodic(asdf);
    }
}

/**
 * Finalize Euler integration (do nothing). Euler does not need finalisation.
 */
template <int dimension, typename float_type>
void euler<dimension, float_type>::finalize() { }

template <int dimension, typename float_type>
static char const* module_name_wrapper(euler<dimension, float_type> const&)
{
    return euler<dimension, float_type>::module_name();
}

template <int dimension, typename float_type>
void euler<dimension, float_type>::luaopen(lua_State* L)
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
                    class_<euler, shared_ptr<_Base>, _Base>(class_name.c_str())
                        .def(constructor<
                            shared_ptr<particle_type>
                          , shared_ptr<box_type>
                          , double
                          , shared_ptr<logger_type>
                          >()
                        )
                        .property("modul_name", &module_name_wrapper<dimension, float_type>)
                        .scope
                        [
                            class_<runtime>("runtime")
                                .def_readonly("integrate", &runtime::integrate)
                        ]
                        .def_readonly("runtime", &euler::runtime_)
                ]
            ]
        ]
    ];
}

HALMD_LUA_API int luaopen_libhalmd_mdsim_host_integrators_euler(lua_State* L)
{
#ifndef USE_HOST_SINGLE_PRECISION
    euler<3, double>::luaopen(L);
    euler<2, double>::luaopen(L);
#else
    euler<3, float>::luaopen(L);
    euler<2, float>::luaopen(L);
#endif
    return 0;
}

// explicit instantiation
#ifndef USE_HOST_SINGLE_PRECISION
template class euler<3, double>;
template class euler<2, double>;
#else
template class euler<3, float>;
template class euler<2, float>;
#endif

} // namespace integrators
} // namespace host
} // namespace mdsim
} // namespace halmd
