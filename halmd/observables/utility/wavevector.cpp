/*
 * Copyright © 2011  Felix Höfling
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

#include <boost/bind.hpp>
#include <cmath>
#include <iterator>
#include <sstream>

#include <halmd/algorithm/host/pick_lattice_points.hpp>
#include <halmd/io/logger.hpp>
#include <halmd/observables/utility/wavevector.hpp>
#include <halmd/utility/lua_wrapper/lua_wrapper.hpp>

using namespace boost;
using namespace std;

namespace halmd
{
namespace observables { namespace utility
{

template <int dimension>
wavevector<dimension>::wavevector(
    vector<double> const& wavenumber
  , vector_type const& box_length
  , double tolerance
  , unsigned int max_count
)
  // initialise members
  : wavenumber_(wavenumber)
  , box_length_(box_length)
  , tolerance_(tolerance)
  , max_count_(max_count)
{
    ostringstream s;
    copy(wavenumber_.begin(), wavenumber_.end(), ostream_iterator<double>(s, " "));
    LOG("wavenumber grid: " << s.str());

    init_();
}

template <int dimension>
wavevector<dimension>::wavevector(
    double max_wavenumber
  , vector_type const& box_length
  , double tolerance
  , unsigned int max_count
)
  // initialise members
  : box_length_(box_length)
  , tolerance_(tolerance)
  , max_count_(max_count)
{
    LOG("maximum wavenumber: " << max_wavenumber);

    // set up linearly spaced wavenumber grid
    double q_min = 2 * M_PI / norm_inf(box_length_); // norm_inf returns the maximum coordinate
    for (double q = q_min; q < max_wavenumber; q += q_min) {
        wavenumber_.push_back(q);
    }

    init_();
}

template <int dimension>
void wavevector<dimension>::init_()
{
    LOG("tolerance on wavevector magnitude: " << tolerance_);
    LOG("maximum number of wavevectors per wavenumber: " << max_count_);

    // construct wavevectors and store as key/value pairs (wavenumber, wavevector)
    algorithm::host::pick_lattice_points_from_shell(
        wavenumber_.begin(), wavenumber_.end()
      , back_inserter(wavevector_)
      , element_div(vector_type(2 * M_PI), box_length_)
      , tolerance_
      , max_count_
    );

    // sort wavevector map according to keys (wavenumber)
    stable_sort(
        wavevector_.begin(), wavevector_.end()
      , bind(less<double>(), bind(&map_type::value_type::first, _1), bind(&map_type::value_type::first, _2))
    );

    // remove wavenumbers with no compatible wavevectors
    for (vector<double>::iterator q_it = wavenumber_.begin(); q_it != wavenumber_.end(); ++q_it) {
        // find wavevector q with |q| = *q_it
        typename map_type::const_iterator found = find_if(
            wavevector_.begin(), wavevector_.end()
          , bind(equal_to<double>(), bind(&map_type::value_type::first, _1), *q_it)
        );
        if (found == wavevector_.end()) {
            LOG_WARNING("No wavevector compatible with |q| ≈ " << *q_it << ". Value discarded");
            wavenumber_.erase(q_it--);   // post-decrement iterator, increment at end of loop
        }
    }

    LOG_DEBUG("total number of wavevectors: " << wavevector_.size());
}

template <int dimension>
void wavevector<dimension>::luaopen(lua_State* L)
{
    using namespace luabind;
    static string class_name("wavevector_" + lexical_cast<string>(dimension) + "_");
    module(L)
    [
        namespace_("halmd_wrapper")
        [
            namespace_("observables")
            [
                namespace_("utility")
                [
                    class_<wavevector, shared_ptr<wavevector> >(class_name.c_str())
                        .def(constructor<
                             vector<double> const&
                           , vector_type const&
                           , double, unsigned int
                        >())
                        .def(constructor<
                             double
                           , vector_type const&
                           , double, unsigned int
                        >())
                        .property("wavenumber", &wavevector::wavenumber)
                        .property("value", &wavevector::value)
                        .property("tolerance", &wavevector::tolerance)
                        .property("maximum_count", &wavevector::maximum_count)
                ]
            ]
        ]
    ];
}

namespace // limit symbols to translation unit
{

__attribute__((constructor)) void register_lua()
{
    lua_wrapper::register_(0) //< distance of derived to base class
    [
        &wavevector<3>::luaopen
    ]
    [
        &wavevector<2>::luaopen
    ];
}

} // namespace

// explicit instantiation
template class wavevector<3>;
template class wavevector<2>;

}} // namespace observables::utility

} // namespace halmd
