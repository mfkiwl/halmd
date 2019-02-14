/*
 * Copyright © 2008-2011  Peter Colberg and Felix Höfling
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

#include <halmd/mdsim/gpu/forces/pair_full_kernel.cuh>
#include <halmd/mdsim/gpu/forces/pair_trunc_kernel.cuh>
#include <halmd/mdsim/gpu/potentials/pair/lennard_jones_simple_kernel.hpp>
#include <halmd/mdsim/gpu/potentials/pair/truncations/truncations.cuh>
#include <halmd/numeric/blas/blas.hpp>
#include <halmd/utility/tuple.hpp>

namespace halmd {
namespace mdsim {
namespace gpu {
namespace potentials {
namespace pair {
namespace lennard_jones_simple_kernel {

/**
 * Lennard-Jones interaction for a simple fluid of a single species.
 */
class lennard_jones_simple
{
public:
    /**
     * Construct Lennard-Jones pair interaction potential.
     *
     * @param type1 type of first interacting particle
     * @param type2 type of second interacting particle
     */
    HALMD_GPU_ENABLED lennard_jones_simple(
        unsigned int type1, unsigned int type2
      , unsigned int ntype1, unsigned int ntype2
    )
    {}

    /**
     * Compute force and potential for interaction.
     *
     * @param rr squared distance between particles
     * @returns tuple of unit "force" @f$ -U'(r)/r @f$ and potential @f$ U(r) @f$
     */
    template <typename float_type>
    HALMD_GPU_ENABLED tuple<float_type, float_type> operator()(float_type rr) const
    {
        return lennard_jones_simple_kernel::compute(rr);
    }
};

} // namespace lennard_jones_simple_kernel

HALMD_MDSIM_GPU_POTENTIALS_PAIR_TRUNCATIONS_INSTANTIATE_WRAPPERS(lennard_jones_simple_kernel::lennard_jones_simple);

} // namespace pair
} // namespace potentials

// explicit instantiation of force kernels
namespace forces {

using namespace halmd::mdsim::gpu::potentials::pair::lennard_jones_simple_kernel;

template class pair_full_wrapper<3, lennard_jones_simple>;
template class pair_full_wrapper<2, lennard_jones_simple>;
HALMD_MDSIM_GPU_POTENTIALS_PAIR_TRUNCATIONS_INSTANTIATE_FORCE_KERNELS(lennard_jones_simple);

} // namespace forces

} // namespace gpu
} // namespace mdsim
} // namespace halmd
