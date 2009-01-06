/* Lennard-Jones fluid kernel
 *
 * Copyright (C) 2008  Peter Colberg
 *
 * This program is free software: you can redistribute it and/or modify
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

#include <float.h>
#include "ljfluid_nbr_glue.hpp"
#include "ljfluid_base.cu"
#include "algorithm.h"
#include "cutil.h"
#include "dsfun.h"
#include "vector2d.h"
#include "vector3d.h"

/** number of cells per dimension */
static __constant__ unsigned int ncell;
/** neighbour list length */
static __constant__ unsigned int nbl_size;
/** neighbour list stride */
static __constant__ unsigned int nbl_stride;
/** potential cutoff radius with cell skin */
static __constant__ float r_cell;
/** squared potential radius distance with cell skin */
static __constant__ float rr_cell;
#ifdef DIM_3D
/** texture reference to periodic particle positions */
struct texture<float4, 1, cudaReadModeElementType> t_r;
/** texture reference to extended particle positions */
struct texture<float4, 1, cudaReadModeElementType> t_R;
/** texture reference to particle velocities */
struct texture<float4, 1, cudaReadModeElementType> t_v;
#else
/** texture reference to periodic particle positions */
struct texture<float2, 1, cudaReadModeElementType> t_r;
/** texture reference to extended particle positions */
struct texture<float2, 1, cudaReadModeElementType> t_R;
/** texture reference to particle velocities */
struct texture<float2, 1, cudaReadModeElementType> t_v;
#endif
/** texture reference to particle tags */
struct texture<int, 1, cudaReadModeElementType> t_tag;


/**
 * n-dimensional MD simulation step
 */
template <typename T, typename U>
__global__ void mdstep(U const* g_r, U* g_v, U* g_f, int const* g_nbl, float* g_en, float* g_virial)
{
    // load particle associated with this thread
    const T r = unpack(g_r[GTID]);
    T v = unpack(g_v[GTID]);

    // potential energy contribution
    float en = 0;
    // virial equation sum contribution
    float virial = 0;

#ifdef DIM_3D
    dfloat3 f(make_float3(0, 0, 0));
#else
    dfloat2 f(make_float2(0, 0));
#endif

    for (unsigned int i = 0; i < nbl_size; ++i) {
	// coalesced read from neighbour list
	const int n = g_nbl[i * nbl_stride + GTID];
	// skip placeholder particles
	if (!IS_REAL_PARTICLE(n))
	    break;
	// accumulate force between particles
	compute_force(r, unpack(tex1Dfetch(t_r, n)), f, en, virial);
    }

    // second leapfrog step as part of integration of equations of motion
    leapfrog_full_step(v, f.f0);

    // store particle associated with this thread
    g_v[GTID] = pack(v);
    g_f[GTID] = pack(f.f0);
    g_en[GTID] = en;
    g_virial[GTID] = virial;
}

/**
 * blockwise maximum velocity magnitude
 */
template <typename T, typename U>
__global__ void maximum_velocity(U const* g_v, float* g_vmax)
{
    extern __shared__ float s_vv[];

    // load particles from global device memory
    float vv = 0;
    for (unsigned int i = GTID; i < npart; i += GTDIM) {
	T v = unpack(g_v[i]);
	vv = fmaxf(vv, v * v);
    }
    // maximum velocity for this thread
    s_vv[TID] = vv;
    __syncthreads();

    // compute maximum velocity for all threads in block
    if (TID < 256) {
	vv = fmaxf(vv, s_vv[TID + 256]);
	s_vv[TID] = vv;
    }
    __syncthreads();
    if (TID < 128) {
	vv = fmaxf(vv, s_vv[TID + 128]);
	s_vv[TID] = vv;
    }
    __syncthreads();
    if (TID < 64) {
	vv = fmaxf(vv, s_vv[TID + 64]);
	s_vv[TID] = vv;
    }
    __syncthreads();
    if (TID < 32) {
	vv = fmaxf(vv, s_vv[TID + 32]);
	s_vv[TID] = vv;
    }
    // no further syncs needed within execution warp of 32 threads
    if (TID < 16) {
	vv = fmaxf(vv, s_vv[TID + 16]);
	s_vv[TID] = vv;
    }
    if (TID < 8) {
	vv = fmaxf(vv, s_vv[TID + 8]);
	s_vv[TID] = vv;
    }
    if (TID < 4) {
	vv = fmaxf(vv, s_vv[TID + 4]);
	s_vv[TID] = vv;
    }
    if (TID < 2) {
	vv = fmaxf(vv, s_vv[TID + 2]);
	s_vv[TID] = vv;
    }
    if (TID < 1) {
	vv = fmaxf(vv, s_vv[TID + 1]);
	// store maximum block velocity in global memory
	g_vmax[blockIdx.x] = sqrtf(vv);
    }
}

/**
 * initialise particle tags
 */
__global__ void init_tags(int* g_tag)
{
    int tag = VIRTUAL_PARTICLE;
    if (GTID < npart) {
	tag = GTID;
    }
    g_tag[GTID] = tag;
}

/**
 * compute neighbour cell
 */
template <typename I>
__device__ unsigned int compute_neighbour_cell(I const& offset)
{
#ifdef DIM_3D
    const int3 cell = make_int3(blockIdx.x % ncell, (blockIdx.x / ncell) % ncell, blockIdx.x / ncell / ncell);
    const int3 neighbour = make_int3((cell.x + ncell + offset.x) % ncell, (cell.y + ncell + offset.y) % ncell, (cell.z + ncell + offset.z) % ncell);
    return (neighbour.z * ncell + neighbour.y) * ncell + neighbour.x;
#else
    const int2 cell = make_int2(blockIdx.x % ncell, blockIdx.x / ncell);
    const int2 neighbour = make_int2((cell.x + ncell + offset.x) % ncell, (cell.y + ncell + offset.y) % ncell);
    return neighbour.y * ncell + neighbour.x;
#endif
}

/**
 * update neighbour list with particles of given cell
 */
template <unsigned int cell_size, bool same_cell, typename T, typename I>
__device__ void update_cell_neighbours(I const& offset, int const* g_cell, int* g_nbl, T const& r, int const& n, unsigned int& count)
{
    __shared__ int s_n[cell_size];
    __shared__ T s_r[cell_size];

    // compute cell index
    const unsigned int cell = compute_neighbour_cell(offset);

    // load particles in cell
    s_n[threadIdx.x] = g_cell[cell * cell_size + threadIdx.x];
    s_r[threadIdx.x] = unpack(tex1Dfetch(t_r, s_n[threadIdx.x]));
    __syncthreads();

    if (IS_REAL_PARTICLE(n)) {
	for (unsigned int i = 0; i < cell_size; ++i) {
	    // particle number of cell placeholder
	    const int m = s_n[i];
	    // skip placeholder particles
	    if (!IS_REAL_PARTICLE(m))
		break;
	    // skip same particle
	    if (same_cell && i == threadIdx.x)
		continue;

	    // particle distance vector
	    T dr = r - s_r[i];
	    // enforce periodic boundary conditions
	    dr -= rintf(__fdividef(dr, box)) * box;
	    // squared particle distance
	    const float rr = dr * dr;

	    // enforce cutoff length with neighbour list skin
	    if (rr <= rr_cell && count < nbl_size) {
		// scattered write to neighbour list
		g_nbl[count * nbl_stride + n] = m;
		// increment neighbour list particle count
		count++;
	    }
	}
    }
}

/**
 * update neighbour lists
 */
template <unsigned int cell_size, typename T>
__global__ void update_neighbours(int const* g_cell, int* g_nbl)
{
    // load particle from cell placeholder
    const int n = g_cell[GTID];
    const T r = unpack(tex1Dfetch(t_r, n));
    // number of particles in neighbour list
    unsigned int count = 0;

    //
    // The summation of all forces acting on a particle is the most
    // critical part of the simulation concerning longtime accuracy.
    //
    // Naively adding all forces with a single-precision operation is fine
    // with the Lennard-Jones potential using the N-squared algorithm, as
    // the force exhibits both a repulsive and an attractive part, and the
    // particles are more or less in random order. Thus, summing over all
    // forces comprises negative and positive summands in random order.
    //
    // With the WCA potential (Weeks-Chandler-Andersen, purely repulsive
    // part of the shifted Lennard-Jones potential) using the N-squared
    // algorithm, the center of mass velocity effectively stays zero if
    // the initial list of particles arranged on a lattice is randomly
    // permuted before simulation.
    // Using the cell algorithm with the WCA potential however results
    // in a continuously drifting center of mass velocity, independent
    // of the chosen simulation timestep.
    //
    // The reason for this behaviour lies in the disadvantageous summing
    // order: With a purely repulsive potential, the summed forces of a
    // single neighbour cell will more or less have the same direction.
    // Thus, when adding the force sums of all neighbour cells, we add
    // huge force sums which will mostly cancel each other out in an
    // equilibrated system, giving a small and very inaccurate total
    // force due to being limited to single-precision floating-point
    // arithmetic.
    //
    // Besides implementing the summation in double precision arithmetic,
    // choosing the order of summation over cells such that one partial
    // neighbour cell force sum is always followed by the sum of the
    // opposite neighbour cell softens the velocity drift.
    //

#ifdef DIM_3D
    // visit this cell
    update_cell_neighbours<cell_size, true>(make_int3( 0,  0,  0), g_cell, g_nbl, r, n, count);
    // visit 26 neighbour cells, grouped into 13 pairs of mutually opposite cells
    update_cell_neighbours<cell_size, false>(make_int3(-1, -1, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(+1, +1, +1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(-1, -1, +1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(+1, +1, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(-1, +1, +1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(+1, -1, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(+1, -1, +1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(-1, +1, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(-1, -1,  0), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(+1, +1,  0), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(-1, +1,  0), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(+1, -1,  0), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(-1,  0, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(+1,  0, +1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(-1,  0, +1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(+1,  0, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3( 0, -1, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3( 0, +1, +1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3( 0, -1, +1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3( 0, +1, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(-1,  0,  0), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3(+1,  0,  0), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3( 0, -1,  0), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3( 0, +1,  0), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3( 0,  0, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int3( 0,  0, +1), g_cell, g_nbl, r, n, count);
#else
    // visit this cell
    update_cell_neighbours<cell_size, true>(make_int2( 0,  0), g_cell, g_nbl, r, n, count);
    // visit 8 neighbour cells, grouped into 4 pairs of mutually opposite cells
    update_cell_neighbours<cell_size, false>(make_int2(-1, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int2(+1, +1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int2(-1, +1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int2(+1, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int2(-1,  0), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int2(+1,  0), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int2( 0, -1), g_cell, g_nbl, r, n, count);
    update_cell_neighbours<cell_size, false>(make_int2( 0, +1), g_cell, g_nbl, r, n, count);
#endif
}

/**
 * compute cell indices for given particle positions
 */
template <typename T, typename U>
__global__ void compute_cell(U const* g_part, uint* g_cell)
{
    const T r = unpack(g_part[GTID]);

    //
    // Mapping the positional coordinates of a particle to its corresponding
    // cell index is the most delicate part of the cell lists update.
    // The safest way is to combine round-towards-zero with a successive
    // integer modulo operation, which comes with a performance penalty.
    //
    // As an efficient alternative, we transform the coordinates to the
    // half-open unit interval [0.0, 1.0) and multiply with the number
    // of cells per dimension afterwards.
    //
    const T cell = (__saturatef(r / box) * (1.f - FLT_EPSILON)) * ncell;

#ifdef DIM_3D
    g_cell[GTID] = uint(cell.x) + ncell * (uint(cell.y) + ncell * uint(cell.z));
#else
    g_cell[GTID] = uint(cell.x) + ncell * uint(cell.y);
#endif
}

/**
 * compute global cell offsets in particle list
 */
__global__ void find_cell_offset(uint* g_cell, int* g_cell_offset)
{
    const uint j = g_cell[GTID];
    const uint k = (GTID > 0 && GTID < npart) ? g_cell[GTID - 1] : j;

    if (GTID == 0 || k < j) {
	// particle marks the start of a cell
	g_cell_offset[j] = GTID;
    }
}

/**
 * assign particles to cells
 */
template <uint cell_size>
__global__ void assign_cells(uint const* g_cell, int const* g_cell_offset, int const* g_itag, int* g_otag)
{
    __shared__ int s_offset[1];

    if (threadIdx.x == 0) {
	s_offset[0] = g_cell_offset[blockIdx.x];
    }
    __syncthreads();
    // global offset of first particle in this block's cell
    const int offset = s_offset[0];
    // global offset of this thread's particle
    const int n = offset + threadIdx.x;
    // mark as virtual particle
    int tag = -1;
    // mark as real particle if appropriate
    if (offset >= 0 && n < npart && g_cell[n] == blockIdx.x) {
	tag = g_itag[n];
    }
    // store particle in this block's cell
    g_otag[blockIdx.x * cell_size + threadIdx.x] = tag;
}

/**
 * generate ascending index sequence
 */
__global__ void gen_index(int* g_idx)
{
    g_idx[GTID] = (GTID < npart) ? GTID : 0;
}

/**
 * order particles after given permutation
 */
template <typename U>
__global__ void order_particles(const int* g_idx, U* g_or, U* g_oR, U* g_ov, int* g_otag)
{
    // permutation index
    const uint j = g_idx[GTID];
    // permute particle phase space coordinates
    g_or[GTID] = tex1Dfetch(t_r, j);
    g_oR[GTID] = tex1Dfetch(t_R, j);
    g_ov[GTID] = tex1Dfetch(t_v, j);
    // permute particle tracking number
    g_otag[GTID] = tex1Dfetch(t_tag, j);
}


namespace mdsim { namespace gpu { namespace ljfluid_gpu_neighbour
{

cuda::symbol<unsigned int> ncell(::ncell);
cuda::symbol<unsigned int> nbl_size(::nbl_size);
cuda::symbol<unsigned int> nbl_stride(::nbl_stride);
cuda::symbol<float> r_cell(::r_cell);
cuda::symbol<float> rr_cell(::rr_cell);
cuda::texture<int> tag(::t_tag);

#ifdef DIM_3D
cuda::function<void (float4 const*, float4*, float4*, int const*, float*, float*)> mdstep(::mdstep<float3>);
cuda::function<void (float4 const*, float*)> maximum_velocity(::maximum_velocity<float3>);
cuda::function<void (int const*, int*)> update_neighbours(::update_neighbours<CELL_SIZE, float3>);
cuda::function<void (float4 const*, uint*)> compute_cell(::compute_cell<float3>);
cuda::function<void (const int*, float4*, float4*, float4*, int*)> order_particles(::order_particles);
cuda::texture<float4> r(::t_r);
cuda::texture<float4> R(::t_R);
cuda::texture<float4> v(::t_v);
#else
cuda::function<void (float2 const*, float2*, float2*, int const*, float*, float*)> mdstep(::mdstep<float2>);
cuda::function<void (float2 const*, float*)> maximum_velocity(::maximum_velocity<float2>);
cuda::function<void (int const*, int*)> update_neighbours(::update_neighbours<CELL_SIZE, float2>);
cuda::function<void (float2 const*, uint*)> compute_cell(::compute_cell<float2>);
cuda::function<void (const int*, float2*, float2*, float2*, int*)> order_particles(::order_particles);
cuda::texture<float2> r(::t_r);
cuda::texture<float2> R(::t_R);
cuda::texture<float2> v(::t_v);
#endif

cuda::function<void (int*)> init_tags(::init_tags);
cuda::function<void (uint const*, int const*, int const*, int*)> assign_cells(::assign_cells<CELL_SIZE>);
cuda::function<void (uint*, int*)> find_cell_offset(::find_cell_offset);
cuda::function<void (int*)> gen_index(::gen_index);

}}} // namespace mdsim::gpu::ljfluid
