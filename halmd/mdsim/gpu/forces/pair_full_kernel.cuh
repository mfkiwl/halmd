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

#ifndef HALMD_MDSIM_GPU_FORCES_PAIR_FULL_KERNEL_CUH
#define HALMD_MDSIM_GPU_FORCES_PAIR_FULL_KERNEL_CUH

#include <halmd/mdsim/force_kernel.hpp>
#include <halmd/mdsim/gpu/box_kernel.cuh>
#include <halmd/mdsim/gpu/forces/pair_full_kernel.hpp>
#include <halmd/mdsim/gpu/particle_kernel.cuh>
#include <halmd/mdsim/type_traits.hpp>
#include <halmd/numeric/blas/blas.hpp>
#include <halmd/numeric/mp/dsfloat.hpp>
#include <halmd/utility/gpu/thread.cuh>

using namespace halmd::mdsim::gpu::particle_kernel;

namespace halmd {
namespace mdsim {
namespace gpu {
namespace forces {
namespace pair_full_kernel {

/** total number of particles */
static __constant__ unsigned int npart_;

/**
 * Compute pair forces, potential energy, and stress tensor for all particles
 */
template <
    bool do_aux               //< compute auxiliary variables in addition to force
  , typename vector_type
  , typename potential_type
  , typename gpu_vector_type
  , typename stress_tensor_first_type
  , typename stress_tensor_second_type
>
__global__ void compute(
    gpu_vector_type* g_f
  , float4 const* g_r
  , float* g_en_pot
  , stress_tensor_first_type* g_stress_pot_first
  , stress_tensor_second_type* g_stress_pot_second
  , float* g_hypervirial
  , vector_type box_length
)
{
    enum { dimension = vector_type::static_size };
    typedef typename vector_type::value_type value_type;
    unsigned int const i = GTID;

    // load particle associated with this thread
    unsigned int type1;
    vector_type r1;
    tie(r1, type1) = untagged<vector_type>(g_r[i]);

    // contribution to potential energy and hypervirial
    float en_pot_ = 0;
    float hypervirial_ = 0;
    // contribution to stress tensor
    typename type_traits<dimension, float>::stress_tensor_type stress_pot = 0;
#ifdef USE_FORCE_DSFUN
    // force sum
    fixed_vector<dsfloat, dimension> f = 0;
#else
    vector_type f = 0;
#endif

    for (unsigned int j = 0; j < npart_; ++j) {
        // load particle
        unsigned int type2;
        vector_type r2;
        tie(r2, type2) = untagged<vector_type>(g_r[j]);
        // pair potential
        potential_type const potential(type1, type2);

        // particle distance vector
        vector_type r = r1 - r2;
        // enforce periodic boundary conditions
        box_kernel::reduce_periodic(r, box_length);
        // squared particle distance
        value_type rr = inner_prod(r, r);

        // skip self-interaction (branch only here to keep memory reads together)
        if (i == j) {
            continue;
        }

        value_type fval, en_pot, hvir;
        tie(fval, en_pot, hvir) = potential(rr);

        // force from other particle acting on this particle
        f += fval * r;
        if (do_aux) {
            // potential energy contribution of this particle
            en_pot_ += 0.5f * en_pot;
            // contribution to stress tensor from this particle
            stress_pot += 0.5f * fval * make_stress_tensor(r);
            // contribution to hypervirial
            hypervirial_ += 0.5f * hvir / (dimension * dimension);
        }
    }

    // write results to global memory
    g_f[i] = static_cast<vector_type>(f);
    if (do_aux) {
        g_en_pot[i] = en_pot_;
        tie(g_stress_pot_first[i], g_stress_pot_second[i]) = split(stress_pot);
        g_hypervirial[i] = hypervirial_;
    }
}

} // namespace pair_full_kernel

template <int dimension, typename potential_type>
pair_full_wrapper<dimension, potential_type> const
pair_full_wrapper<dimension, potential_type>::kernel = {
    pair_full_kernel::compute<false, fixed_vector<float, dimension>, potential_type>
  , pair_full_kernel::compute<true, fixed_vector<float, dimension>, potential_type>
  , pair_full_kernel::npart_
};

} // namespace mdsim
} // namespace gpu
} // namespace forces
} // namespace halmd

#endif /* ! HALMD_MDSIM_GPU_FORCES_PAIR_FULL_KERNEL_CUH */
