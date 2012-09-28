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

#ifndef HALMD_MDSIM_GPU_FORCE_HPP
#define HALMD_MDSIM_GPU_FORCE_HPP

#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>
#include <lua.hpp>

#include <halmd/mdsim/force.hpp>
#include <halmd/mdsim/type_traits.hpp>

namespace halmd {
namespace mdsim {
namespace gpu {

template <int dimension, typename float_type>
class force
  : public mdsim::force<dimension>
{
public:
    typedef mdsim::force<dimension> _Base;
    typedef type_traits<dimension, float_type> _type_traits;
    typedef typename _type_traits::vector_type vector_type;
    typedef typename _type_traits::stress_tensor_type stress_tensor_type;
    typedef typename _type_traits::gpu::stress_tensor_first_type gpu_stress_tensor_first_type;
    typedef typename _type_traits::gpu::stress_tensor_second_type gpu_stress_tensor_second_type;

    typedef boost::tuple<
        cuda::vector<gpu_stress_tensor_first_type> const&
      , cuda::vector<gpu_stress_tensor_second_type> const&
    > gpu_stress_tensor_const_references;

    static void luaopen(lua_State* L);

    force() {}
    virtual cuda::vector<float> const& potential_energy() const = 0;
    virtual gpu_stress_tensor_const_references stress_tensor_pot() const = 0;
    virtual cuda::vector<float> const& hypervirial() const = 0;
};

} // namespace mdsim
} // namespace gpu
} // namespace halmd

#endif /* ! HALMD_MDSIM_GPU_FORCE_HPP */
