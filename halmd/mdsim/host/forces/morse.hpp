/*
 * Copyright © 2008-2010  Peter Colberg and Felix Höfling
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

#ifndef HALMD_MDSIM_HOST_FORCES_MORSE_HPP
#define HALMD_MDSIM_HOST_FORCES_MORSE_HPP

#include <boost/assign.hpp>
#include <boost/numeric/ublas/symmetric.hpp>
#include <boost/shared_ptr.hpp>
#include <lua.hpp>
#include <utility>

#include <halmd/mdsim/host/forces/pair_trunc.hpp>
#include <halmd/mdsim/host/forces/smooth.hpp>

namespace halmd
{
namespace mdsim { namespace host { namespace forces
{

/**
 * define Morse potential and parameters
 */
template <typename float_type>
class morse
{
public:
    typedef boost::numeric::ublas::symmetric_matrix<float_type, boost::numeric::ublas::lower> matrix_type;

    static char const* name() { return "Morse"; }
    static char const* module_name() { return "morse"; }

    static void luaopen(lua_State* L);
    morse(
        unsigned ntype
      , boost::array<float, 3> const& cutoff
      , boost::array<float, 3> const& epsilon
      , boost::array<float, 3> const& sigma
      , boost::array<float, 3> const& r_min
    );

    /**
     * Compute force and potential for interaction.
     *
     * @param rr squared distance between particles
     * @param type1 type of first interacting particle
     * @param type2 type of second interacting particle
     * @returns tuple of unit "force" @f$ -U'(r)/r @f$ and potential @f$ U(r) @f$
     */
    std::pair<float_type, float_type> operator() (float_type rr, unsigned a, unsigned b)
    {
        float_type delta_r_sigma = (r_min_(a, b) - sqrt(rr)) / sigma_(a, b);
        float_type exp_dr = exp(delta_r_sigma);
        float_type fval = -2 * epsilon_(a, b) / sigma_(a, b) * (exp_dr - 1) * exp_dr;
        float_type en_pot = epsilon_(a, b) * (exp_dr - 2) * exp_dr - en_cut_(a, b);

        return std::make_pair(fval, en_pot);
    }

    matrix_type const& r_cut() const
    {
        return r_cut_;
    }

    float_type r_cut(unsigned a, unsigned b) const
    {
        return r_cut_(a, b);
    }

    float_type rr_cut(unsigned a, unsigned b) const
    {
        return rr_cut_(a, b);
    }

    matrix_type const& r_cut_sigma() const
    {
        return r_cut_sigma_;
    }

    matrix_type const& epsilon() const
    {
        return epsilon_;
    }

    matrix_type const& sigma() const
    {
        return sigma_;
    }

    matrix_type const& r_min_sigma() const
    {
        return r_min_sigma_;
    }

private:
    /** depths of potential well in MD units */
    matrix_type epsilon_;
    /** width of potential well in MD units */
    matrix_type sigma_;
    /** position of potential well in MD units */
    matrix_type r_min_;
    /** position of potential well in units of sigma */
    matrix_type r_min_sigma_;
    /** potential energy at cutoff length in MD units */
    matrix_type en_cut_;
    /** cutoff radius in MD units */
    matrix_type r_cut_;
    /** cutoff radius in units of sigma */
    matrix_type r_cut_sigma_;
    /** square of cutoff radius */
    matrix_type rr_cut_;
};

}}} // namespace mdsim::host::forces

} // namespace halmd

#endif /* ! HALMD_MDSIM_HOST_FORCES_MORSE_HPP */
