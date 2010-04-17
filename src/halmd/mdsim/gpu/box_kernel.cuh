/*
 * Copyright © 2008-2010  Peter Colberg
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

#ifndef HALMD_MDSIM_GPU_BOX_KERNEL_CUH
#define HALMD_MDSIM_GPU_BOX_KERNEL_CUH

namespace halmd { namespace mdsim { namespace gpu { namespace box_kernel
{

/**
 * enforce periodic boundary conditions on argument
 *
 * map coordinates to (-L[i]/2, L[i]/2)
 * which is appropriate too for relative vectors
 *
 * return reduction vector in units of box edge lengths
 */
template <typename vector_type, typename vector_type_>
__device__ inline vector_type_ reduce_periodic(
  vector_type& r,
  vector_type_ const& L)
{
    vector_type_ image = rint(__fdivide(static_cast<vector_type_>(r), L));
    r -= element_prod(image, L);
    return image;
}

}}}} // namespace halmd::mdsim::gpu::box_kernel

#endif /* ! HALMD_MDSIM_GPU_BOX_KERNEL_CUH */
