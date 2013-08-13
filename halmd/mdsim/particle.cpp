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
#include <boost/array.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <exception>
#include <numeric>

#include <halmd/io/logger.hpp>
#include <halmd/mdsim/particle.hpp>
#include <halmd/utility/lua/lua.hpp>
#include <halmd/utility/signal.hpp>

using namespace boost;
using namespace boost::algorithm;
using namespace std;

namespace halmd {
namespace mdsim {

/**
 * Construct microscopic system state.
 *
 * @param particles number of particles per type or species
 */
template <int dimension>
particle<dimension>::particle(vector<unsigned int> const& particles)
  : nbox(accumulate(particles.begin(), particles.end(), 0))
  , ntype(particles.size())
  , ntypes(particles)
{
    if (*min_element(this->ntypes.begin(), this->ntypes.end()) < 1) {
        throw logic_error("invalid number of particles");
    }

    // workaround for bug in Boost 1.53.0
    //
    // see http://lists.boost.org/boost-users/2013/03/78142.php
    string(*p_lexical_cast)(const unsigned int&) = &boost::lexical_cast<string, unsigned int>;

    vector<string> ntypes_(ntypes.size());
    transform(
        ntypes.begin()
      , ntypes.end()
      , ntypes_.begin()
      , p_lexical_cast
    );

    LOG("number of particles: " << nbox);
    LOG("number of particle types: " << ntype);
    LOG("number of particles per type: " << join(ntypes_, " "));
}

template <typename particle_type>
typename signal<void ()>::slot_function_type
wrap_set(shared_ptr<particle_type> particle)
{
    return bind(&particle_type::set, particle);
}


template <int dimension>
void particle<dimension>::luaopen(lua_State* L)
{
    using namespace luabind;
    static string class_name("particle_" + lexical_cast<string>(dimension) + "_");
    module(L, "libhalmd")
    [
        namespace_("mdsim")
        [
            class_<particle, shared_ptr<particle> >(class_name.c_str())
                .property("set", &wrap_set<particle>)
                .def_readonly("nbox", &particle::nbox)
                .def_readonly("ntype", &particle::ntype)
                .def_readonly("ntypes", &particle::ntypes)
        ]
    ];
}

HALMD_LUA_API int luaopen_libhalmd_mdsim_particle(lua_State* L)
{
    particle<3>::luaopen(L);
    particle<2>::luaopen(L);
    return 0;
}

// explicit instantiation
template class particle<3>;
template class particle<2>;

} // namespace mdsim
} // namespace halmd
