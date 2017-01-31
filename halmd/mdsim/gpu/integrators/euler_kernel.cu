/*
 * Copyright © 2011-2014 Felix Höfling
 * Copyright © 2011-2012 Michael Kopp
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

#include <halmd/mdsim/gpu/box_kernel.cuh>
#include <halmd/mdsim/gpu/integrators/euler_kernel.hpp>
#include <halmd/numeric/mp/dsfloat.hpp>
#include <halmd/utility/gpu/thread.cuh>

namespace halmd {
namespace mdsim {
namespace gpu {
namespace integrators {
namespace euler_kernel {

/**
 * Euler integration: @f$ r(t + \Delta t) = r(t) + v(t) \Delta t @f$
 *
 * @param g_position    positions
 * @param g_image       number of times the particle exceeded the box margin
 * @param g_velocity    velocities
 * @param g_force       forces
 * @param timestep      integration timestep
 * @param box_length    edge lengths of cuboid box
 */
template <typename ptr_type, typename const_ptr_type, int dimension, typename float_type, typename gpu_vector_type>
__global__ void integrate(
    ptr_type g_position
  , gpu_vector_type* g_image
  , const_ptr_type g_velocity
  , float timestep
  , fixed_vector<float, dimension> box_length
)
{
    typedef fixed_vector<float_type, dimension> vector_type;
    typedef fixed_vector<float, dimension> float_vector_type;

    // kernel execution parameters
    unsigned int const thread = GTID;
    unsigned int const nthread = GTDIM;

    // read position, species, velocity, mass, image from global memory
    vector_type r, v;
    unsigned int species;
    float mass;
    tie(r, species) <<= g_position[thread];
    tie(v, mass) <<= g_velocity[thread];

    // Euler integration
    r += v * timestep;
    // enforce periodic boundary conditions
    float_vector_type image = box_kernel::reduce_periodic(r, box_length);

    // store position, species, image in global memory
    g_position[thread] <<= tie(r, species);
    if (!(image == float_vector_type(0))) {
        g_image[thread] = image + static_cast<float_vector_type>(g_image[thread]);
    }
}

} // namespace euler_kernel

template <typename float_type, int dimension>
euler_wrapper<float_type, dimension> const euler_wrapper<float_type, dimension>::kernel = {
    euler_kernel::integrate<ptr_type, const_ptr_type, dimension, float_type>
};

// explicit instantiation
template class euler_wrapper<float, 3>;
template class euler_wrapper<float, 2>;
template class euler_wrapper<dsfloat, 3>;
template class euler_wrapper<dsfloat, 2>;

} // namespace integrators
} // namespace gpu
} // namespace mdsim
} // namespace halmd
