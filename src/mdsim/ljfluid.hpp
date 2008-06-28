/* Lennard-Jones fluid simulation using CUDA
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

#ifndef MDSIM_LJFLUID_HPP
#define MDSIM_LJFLUID_HPP

#include <algorithm>
#include <boost/array.hpp>
#include <boost/foreach.hpp>
#include <cmath>
#include <cuda_wrapper.hpp>
#include <stdint.h>
#include "H5param.hpp"
#include "accumulator.hpp"
#include "perf.hpp"
#include "exception.hpp"
#include "log.hpp"
#include "statistics.hpp"
#include "rand48.hpp"
#include "gpu/ljfluid_glue.hpp"


#define foreach BOOST_FOREACH

namespace mdsim
{

/**
 * Lennard-Jones fluid simulation using CUDA
 */
template <unsigned dimension, typename T, typename U>
class ljfluid
{
public:
    /** initialize fixed simulation parameters */
    ljfluid();
    /** set number of particles in system */
    void particles(unsigned int value);
    /** set particle density */
    void density(float value);
    /** set periodic box length */
    void box(float value);
#ifdef USE_CELL
    /** set desired average cell occupancy */
    void cell_occupancy(float value);
#endif
    /** set number of CUDA execution threads */
    void threads(unsigned int value);
    /** restore system state from phase space sample */
    template <typename V> void restore(V visitor);

    /** seed random number generator */
    void rng(unsigned int seed);
    /** restore random number generator from state */
    void rng(mdsim::rand48::state_type const& state);
    /** place particles on a face-centered cubic (fcc) lattice */
    void lattice();
    /** set system temperature according to Maxwell-Boltzmann distribution */
    void temperature(float temp);
    /** set simulation timestep */
    void timestep(float value);

    /** get number of particles */
    unsigned int const& particles() const;
#ifdef USE_CELL
    /** get number of cells per dimension */
    unsigned int const& cells() const;
    /** get total number of cell placeholders */
    unsigned int const& placeholders() const;
    /** get cell length */
    float const& cell_length() const;
    /** get effective average cell occupancy */
    float const& cell_occupancy() const;
    /** get number of placeholders per cell */
    unsigned int const& cell_size() const;
#endif
    /** get number of CUDA execution blocks */
    unsigned int blocks() const;
    /** get number of CUDA execution threads */
    unsigned int threads() const;
    /** get particle density */
    float const& density() const;
    /** get periodic box length */
    float const& box() const;
    /** get simulation timestep */
    float const& timestep() const;
    /** get potential cutoff distance */
    float const& cutoff_distance() const;

    /** stream MD simulation step on GPU */
    void mdstep();
    /** synchronize MD simulation step on GPU */
    void synchronize();
    /** copy MD simulation step results from GPU to host */
    void sample();
    /** sample trajectory */
    template <typename V> void sample(V visitor) const;
    /** get CUDA execution time statistics */
    perf_type const& times() const;

private:
    /** number of particles in system */
    unsigned int npart;
#ifdef USE_CELL
    /** number of cells per dimension */
    unsigned int ncell;
    /** total number of cell placeholders */
    unsigned int nplace;
    /** cell length */
    float cell_length_;
    /** effective average cell occupancy */
    float cell_occupancy_;
    /** number of placeholders per cell */
    unsigned int cell_size_;
#endif
    /** particle density */
    float density_;
    /** periodic box length */
    float box_;
    /** simulation timestep */
    float timestep_;
    /** cutoff distance for shifted Lennard-Jones potential */
    float r_cut;

    /** system state in page-locked host memory */
    struct {
	/** periodically reduced particle positions */
	cuda::host::vector<U> r;
	/** periodically extended particle positions */
	cuda::host::vector<U> R;
	/** particle velocities */
	cuda::host::vector<U> v;
#ifndef USE_CELL
	/** potential energies per particle */
	cuda::host::vector<float> en;
	/** virial equation sums per particle */
	cuda::host::vector<float> virial;
#endif
    } h_part;

    /** mean potential energy per particle */
    float en_pot_;
    /** mean virial equation sum per particle */
    float virial_;

#ifdef USE_CELL
    /** cell placeholders in page-locked host memory */
    struct {
	/** periodically reduced particle positions */
	cuda::host::vector<U> r;
	/** periodically extended particle positions */
	cuda::host::vector<U> R;
	/** particle velocities */
	cuda::host::vector<U> v;
	/** particle number tags */
	cuda::host::vector<int> n;
	/** potential energies per particle */
	cuda::host::vector<float> en;
	/** virial equation sums per particle */
	cuda::host::vector<float> virial;
    } h_cell;
#endif

#ifdef USE_CELL
    /** system state in global device memory */
    struct {
	/** periodically reduced particle positions */
	cuda::vector<U> r;
	/** periodically extended particle positions */
	cuda::vector<U> R;
	/** particle velocities */
	cuda::vector<U> v;
	/** particle number tags */
	cuda::vector<int> n;
	/** particle forces */
	cuda::vector<U> f;
	/** potential energies per particle */
	cuda::vector<float> en;
	/** virial equation sums per particle */
	cuda::vector<float> virial;
    } g_cell;

    /** system state double buffer in global device memory */
    struct {
	/** periodically reduced particle positions */
	cuda::vector<U> r;
	/** periodically extended particle positions */
	cuda::vector<U> R;
	/** particle velocities */
	cuda::vector<U> v;
	/** particle number tags */
	cuda::vector<int> n;
    } g_cell2;
#else
    /** system state in global device memory */
    struct {
	/** periodically reduced particle positions */
	cuda::vector<U> r;
	/** periodically extended particle positions */
	cuda::vector<U> R;
	/** particle velocities */
	cuda::vector<U> v;
	/** particle forces */
	cuda::vector<U> f;
	/** potential energies per particle */
	cuda::vector<float> en;
	/** virial equation sums per particle */
	cuda::vector<float> virial;
    } g_part;
#endif

    /** random number generator */
    mdsim::rand48 rng_;
    /** CUDA execution dimensions */
    cuda::config dim_;
#ifdef USE_CELL
    /** CUDA execution dimensions for cell-specific kernels */
    cuda::config dim_cell_;
#endif
    /** CUDA asynchronous execution */
    cuda::stream stream_;
    /** CUDA events for kernel timing */
#ifdef USE_CELL
    boost::array<cuda::event, 5> event_;
#else
    boost::array<cuda::event, 3> event_;
#endif
    /** CUDA execution time statistics */
    perf_type times_;
};


/**
 * initialize fixed simulation parameters
 */
template <unsigned dimension, typename T, typename U>
ljfluid<dimension, T, U>::ljfluid()
{
    // suppress attractive tail of Lennard-Jones potential
    r_cut = std::pow(2., 1. / 6.);
    LOG("potential cutoff distance: " << r_cut);

    // squared cutoff distance
    float rr_cut = r_cut * r_cut;
    // potential energy at cutoff distance
    float rri_cut = 1. / rr_cut;
    float r6i_cut = rri_cut * rri_cut * rri_cut;
    float en_cut = 2. * r6i_cut * (r6i_cut - 1.);

    try {
	cuda::copy(rr_cut, gpu::ljfluid::rr_cut);
	cuda::copy(en_cut, gpu::ljfluid::en_cut);
    }
    catch (cuda::error const& e) {
	throw exception("failed to copy cutoff parameters to device symbols");
    }
}

/**
 * set number of particles in system
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::particles(unsigned int value)
{
    // validate particle number
    if (value < 1) {
	throw exception("invalid number of particles");
    }
    // set particle number
    npart = value;
    LOG("number of particles: " << npart);
    // copy particle number to device symbol
    try {
	cuda::copy(npart, gpu::ljfluid::npart);
    }
    catch (cuda::error const& e) {
	throw exception("failed to copy particle number to device symbol");
    }

#ifndef USE_CELL
    // allocate global device memory for system state
    try {
	g_part.r.resize(npart);
	g_part.R.resize(npart);
	g_part.v.resize(npart);
	g_part.f.resize(npart);
	g_part.en.resize(npart);
	g_part.virial.resize(npart);
    }
    catch (cuda::error const& e) {
	throw exception("failed to allocate global device memory for system state");
    }
#endif

    // allocate page-locked host memory for system state
    try {
	h_part.r.resize(npart);
	h_part.R.resize(npart);
	h_part.v.resize(npart);
	// particle forces reside only in GPU memory
#ifndef USE_CELL
	h_part.en.resize(npart);
	h_part.virial.resize(npart);
#endif
    }
    catch (cuda::error const& e) {
	throw exception("failed to allocate page-locked host memory for system state");
    }
}

/**
 * set particle density
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::density(float value)
{
    // set particle density
    density_ = value;
    LOG("particle density: " << density_);

    // compute periodic box length
    box_ = powf(npart / density_, 1. / dimension);
    LOG("periodic simulation box length: " << box_);
    // copy periodic box length to device symbol
    try {
	cuda::copy(box_, gpu::ljfluid::box);
    }
    catch (cuda::error const& e) {
	throw exception("failed to copy periodic box length to device symbol");
    }
}

/**
 * set periodic box length
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::box(float value)
{
    // set periodic box length
    box_ = value;
    LOG("periodic simulation box length: " << box_);
    // copy periodic box length to device symbol
    try {
	cuda::copy(box_, gpu::ljfluid::box);
    }
    catch (cuda::error const& e) {
	throw exception("failed to copy periodic box length to device symbol");
    }

    // compute particle density
    density_ = npart / powf(box_, dimension);
    LOG("particle density: " << density_);
}

#ifdef USE_CELL
/**
 * set desired average cell occupancy
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::cell_occupancy(float value)
{
    LOG("desired average cell occupancy: " << value);

    // fixed cell size due to fixed number of CUDA execution threads per block
    cell_size_ = CELL_SIZE;

    // optimal number of cells with given cell occupancy as upper boundary
    ncell = std::ceil(std::pow(npart / (value * cell_size_), 1.f / dimension));

    // set number of cells per dimension, respecting cutoff distance
    ncell = std::min(ncell, uint(box_ / r_cut));
    LOG("number of cells per dimension: " << ncell);

    if (ncell < 3) {
	throw exception("number of cells per dimension must be at least 3");
    }

    // derive cell length from number of cells
    cell_length_ = box_ / ncell;
    LOG("cell length: " << cell_length_);

    // set total number of cell placeholders
    nplace = pow(ncell, dimension) * cell_size_;
    LOG("number of cell placeholders: " << nplace);

    // set effective average cell occupancy
    cell_occupancy_ = npart * 1. / nplace;
    LOG("effective average cell occupancy: " << cell_occupancy_);

    if (cell_occupancy_ > 1.) {
	throw exception("average cell occupancy must not be larger than 1.0");
    }
    else if (cell_occupancy_ > 0.5) {
	LOG_WARNING("average cell occupancy is larger than 0.5");
    }

    // copy cell parameters to device symbols
    try {
	cuda::copy(ncell, gpu::ljfluid::ncell);
    }
    catch (cuda::error const& e) {
	throw exception("failed to copy cell parameters to device symbols");
    }
}
#endif /* USE_CELL */

/**
 * set number of CUDA execution threads
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::threads(unsigned int value)
{
    // query CUDA device properties
    cuda::device::properties prop;
    try {
	prop = cuda::device::properties(cuda::device::get());
    }
    catch (cuda::error const& e) {
	throw exception("failed to query CUDA device properties");
    }

    // validate number of CUDA execution threads
    if (value < 1) {
	throw exception("invalid number of CUDA execution threads");
    }
    if (value > prop.max_threads_per_block()) {
	throw exception("number of CUDA execution threads exceeds maximum number of threads per block");
    }
    if (value & (value - 1)) {
	LOG_WARNING("number of CUDA execution threads not a power of 2");
    }
    if (value % prop.warp_size()) {
	LOG_WARNING("number of CUDA execution threads not a multiple of warp size (" << prop.warp_size() << ")");
    }

    // set CUDA execution dimensions
    dim_ = cuda::config(dim3((npart + value - 1) / value), dim3(value));
    LOG("number of CUDA execution blocks: " << dim_.blocks_per_grid());
    LOG("number of CUDA execution threads: " << dim_.threads_per_block());

    if (dim_.threads() != npart) {
	LOG_WARNING("number of particles (" << npart << ") not a multiple of number of CUDA execution threads (" << dim_.threads() << ")");
    }

#ifdef USE_CELL
    // set CUDA execution dimensions for cell-specific kernels
    dim_cell_ = cuda::config(dim3(powf(ncell, dimension)), dim3(cell_size_));
    LOG("number of cell CUDA execution blocks: " << dim_cell_.blocks_per_grid());
    LOG("number of cell CUDA execution threads: " << dim_cell_.threads_per_block());

    // allocate page-locked host memory for cell placeholders
    try {
	h_cell.r.resize(dim_cell_.threads());
	h_cell.R.resize(dim_cell_.threads());
	h_cell.v.resize(dim_cell_.threads());
	h_cell.n.resize(dim_cell_.threads());
	// particle forces reside only in GPU memory
	h_cell.en.resize(dim_cell_.threads());
	h_cell.virial.resize(dim_cell_.threads());
    }
    catch (cuda::error const& e) {
	throw exception("failed to allocate page-locked host memory cell placeholders");
    }

    // allocate global device memory for cell placeholders
    try {
	g_cell.r.resize(dim_cell_.threads());
	g_cell.R.resize(dim_cell_.threads());
	g_cell.v.resize(dim_cell_.threads());
	g_cell.n.resize(dim_cell_.threads());
	g_cell.f.resize(dim_cell_.threads());
	g_cell.en.resize(dim_cell_.threads());
	g_cell.virial.resize(dim_cell_.threads());
    }
    catch (cuda::error const& e) {
	throw exception("failed to allocate global device memory cell placeholders");
    }

    // allocate global device memory for double buffers
    try {
	g_cell2.r.resize(dim_cell_.threads());
	g_cell2.R.resize(dim_cell_.threads());
	g_cell2.v.resize(dim_cell_.threads());
	g_cell2.n.resize(dim_cell_.threads());
    }
    catch (cuda::error const& e) {
	throw exception("failed to allocate global device memory double buffers");
    }
#else /* USE_CELL */
    // allocate global device memory for placeholder particles
    try {
	g_part.r.reserve(dim_.threads());
	g_part.R.reserve(dim_.threads());
	g_part.v.reserve(dim_.threads());
	g_part.f.reserve(dim_.threads());
	g_part.en.reserve(dim_.threads());
	g_part.virial.reserve(dim_.threads());
    }
    catch (cuda::error const& e) {
	throw exception("failed to allocate global device memory for placeholder particles");
    }
#endif /* USE_CELL */

    // change random number generator dimensions
    try {
	rng_.resize(dim_);
    }
    catch (cuda::error const& e) {
	throw exception("failed to change random number generator dimensions");
    }
}

/**
 * restore system state from phase space sample
 */
template <unsigned dimension, typename T, typename U>
template <typename V>
void ljfluid<dimension, T, U>::restore(V visitor)
{
    // read phase space sample
    visitor(h_part.r, h_part.v);

    try {
#ifdef USE_CELL
	// copy periodically reduced particle positions from host to GPU
	cuda::vector<U> g_r(npart);
	cuda::copy(h_part.r, g_r, stream_);
	// assign particles to cells
	event_[0].record(stream_);
	cuda::configure(dim_cell_.grid, dim_cell_.block, stream_);
	gpu::ljfluid::assign_cells(g_r.data(), g_cell.r.data(), g_cell.n.data());
	event_[1].record(stream_);
	// replicate particle positions to periodically extended positions
	cuda::copy(g_cell.r, g_cell.R, stream_);
	// calculate forces, potential energy and virial equation sum
	cuda::configure(dim_cell_.grid, dim_cell_.block, stream_);
	gpu::ljfluid::mdstep(g_cell.r.data(), g_cell.v.data(), g_cell.f.data(), g_cell.n.data(), g_cell.en.data(), g_cell.virial.data());

	// copy particle number tags from GPU to host
	cuda::copy(g_cell.n, h_cell.n, stream_);
	// wait for CUDA operations to finish
	stream_.synchronize();

	// assign velocities to cell placeholders
	for (unsigned int i = 0; i < nplace; ++i) {
	    if (IS_REAL_PARTICLE(h_cell.n[i])) {
		h_cell.v[i] = h_part.v[h_cell.n[i]];
	    }
	}
	// copy particle velocities from host to GPU (after force calculation!)
	cuda::copy(h_cell.v, g_cell.v, stream_);
#else
	// copy periodically reduced particle positions from host to GPU
	cuda::copy(h_part.r, g_part.r, stream_);
	// replicate to periodically extended particle positions
	cuda::copy(g_part.r, g_part.R, stream_);
	// calculate forces, potential energy and virial equation sum
	cuda::configure(dim_.grid, dim_.block, dim_.threads_per_block() * sizeof(U), stream_);
	gpu::ljfluid::mdstep(g_part.r.data(), g_part.v.data(), g_part.f.data(), g_part.en.data(), g_part.virial.data());

	// copy particle velocities from host to GPU (after force calculation!)
	cuda::copy(h_part.v, g_part.v, stream_);
#endif
	stream_.synchronize();
    }
    catch (cuda::error const& e) {
	throw exception("failed to restore system state from phase space sample");
    }

#ifdef USE_CELL
    times_["gpu"]["assign_cells"] += event_[1] - event_[0];
#endif
}

/**
 * seed random number generator
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::rng(unsigned int seed)
{
    LOG("random number generator seed: " << seed);
    try {
	rng_.set(seed);
    }
    catch (cuda::error const& e) {
	throw exception("failed to seed random number generator");
    }
}

/**
 * restore random number generator from state
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::rng(mdsim::rand48::state_type const& state)
{
    try {
	rng_.restore(state);
    }
    catch (cuda::error const& e) {
	throw exception("failed to restore random number generator state");
    }
}

/**
 * place particles on a face-centered cubic (fcc) lattice
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::lattice()
{
    LOG("placing particles on face-centered cubic (fcc) lattice");
    try {
#ifdef USE_CELL
	cuda::vector<U> g_r(npart);
	g_r.reserve(dim_.threads());
	// compute particle lattice positions on GPU
	event_[0].record(stream_);
	cuda::configure(dim_.grid, dim_.block, stream_);
	gpu::ljfluid::lattice(g_r.data());
	event_[1].record(stream_);
	// assign particles to cells
	cuda::configure(dim_cell_.grid, dim_cell_.block, stream_);
	gpu::ljfluid::assign_cells(g_r.data(), g_cell.r.data(), g_cell.n.data());
	event_[2].record(stream_);
	// replicate particle positions to periodically extended positions
	cuda::copy(g_cell.r, g_cell.R, stream_);
	// calculate forces, potential energy and virial equation sum
	cuda::configure(dim_cell_.grid, dim_cell_.block, stream_);
	gpu::ljfluid::mdstep(g_cell.r.data(), g_cell.v.data(), g_cell.f.data(), g_cell.n.data(), g_cell.en.data(), g_cell.virial.data());
#else
	// compute particle lattice positions on GPU
	event_[0].record(stream_);
	cuda::configure(dim_.grid, dim_.block, stream_);
	gpu::ljfluid::lattice(g_part.r.data());
	event_[1].record(stream_);
	// copy particle positions to periodically extended positions
	cuda::copy(g_part.r, g_part.R, stream_);
	// calculate forces, potential energy and virial equation sum
	cuda::configure(dim_.grid, dim_.block, dim_.threads_per_block() * sizeof(U), stream_);
	gpu::ljfluid::mdstep(g_part.r.data(), g_part.v.data(), g_part.f.data(), g_part.en.data(), g_part.virial.data());
#endif

	// wait for CUDA operations to finish
	stream_.synchronize();
    }
    catch (cuda::error const& e) {
	throw exception("failed to compute particle lattice positions on GPU");
    }

    times_["gpu"]["lattice"] += event_[1] - event_[0];
#ifdef USE_CELL
    times_["gpu"]["assign_cells"] += event_[2] - event_[1];
#endif
}

/**
 * set system temperature according to Maxwell-Boltzmann distribution
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::temperature(float temp)
{
    LOG("initializing velocities from Maxwell-Boltzmann distribution at temperature: " << temp);
    try {
#ifdef USE_CELL
	cuda::vector<U> g_v(npart);
	g_v.reserve(dim_.threads());
	// set velocities using Maxwell-Boltzmann distribution at temperature
	event_[0].record(stream_);
	cuda::configure(dim_.grid, dim_.block, stream_);
	gpu::ljfluid::boltzmann(g_v.data(), temp, rng_.data());
	event_[1].record(stream_);
	// copy particle velocities from GPU to host
	cuda::copy(g_v, h_part.v, stream_);
	// copy particle number tags from GPU to host
	cuda::copy(g_cell.n, h_cell.n, stream_);
#else
	// set velocities using Maxwell-Boltzmann distribution at temperature
	event_[0].record(stream_);
	cuda::configure(dim_.grid, dim_.block, stream_);
	gpu::ljfluid::boltzmann(g_part.v.data(), temp, rng_.data());
	event_[1].record(stream_);
	// copy particle velocities from GPU to host
	cuda::copy(g_part.v, h_part.v, stream_);
#endif
	// wait for CUDA operations to finish
	stream_.synchronize();
    }
    catch (cuda::error const& e) {
	throw exception("failed to compute Maxwell-Boltzmann distributed velocities on GPU");
    }

    // accumulate Maxwell-Boltzmann distribution GPU time
    times_["gpu"]["boltzmann"] += event_[1] - event_[0];

    // compute center of mass velocity
    T v_cm = 0;
    for (size_t i = 0; i < h_part.v.size(); ++i) {
	v_cm += (T(h_part.v[i]) - v_cm) / (i + 1);
    }
    // set center of mass velocity to zero
    for (size_t i = 0; i < h_part.v.size(); ++i) {
	h_part.v[i].x -= v_cm[0];
	h_part.v[i].y -= v_cm[1];
#ifdef DIM_3D
	h_part.v[i].z -= v_cm[2];
#endif
    }

    try {
#ifdef USE_CELL
	// assign velocities to cell placeholders
	for (unsigned int i = 0; i < nplace; ++i) {
	    if (IS_REAL_PARTICLE(h_cell.n[i])) {
		h_cell.v[i] = h_part.v[h_cell.n[i]];
	    }
	}
	// copy particle velocities from host to GPU
	cuda::copy(h_cell.v, g_cell.v, stream_);
#else
	// copy particle velocities from host to GPU
	cuda::copy(h_part.v, g_part.v, stream_);
#endif
	stream_.synchronize();
    }
    catch (cuda::error const& e) {
	throw exception("failed to set center of mass velocity to zero");
    }
}

/**
 * set simulation timestep
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::timestep(float value)
{
    // set simulation timestep
    timestep_ = value;
    LOG("simulation timestep: " << timestep_);
    // copy simulation timestep to device symbol
    try {
	cuda::copy(timestep_, gpu::ljfluid::timestep);
    }
    catch (cuda::error const& e) {
	throw exception("failed to copy simulation timestep to device symbol");
    }
}

/**
 * get number of particles
 */
template <unsigned dimension, typename T, typename U>
unsigned int const& ljfluid<dimension, T, U>::particles() const
{
    return npart;
}

#ifdef USE_CELL
/**
 * get number of cells per dimension
 */
template <unsigned dimension, typename T, typename U>
unsigned int const& ljfluid<dimension, T, U>::cells() const
{
    return ncell;
}

/**
 * get total number of cell placeholders
 */
template <unsigned dimension, typename T, typename U>
unsigned int const& ljfluid<dimension, T, U>::placeholders() const
{
    return nplace;
}

/**
 * get cell length
 */
template <unsigned dimension, typename T, typename U>
float const& ljfluid<dimension, T, U>::cell_length() const
{
    return cell_length_;
}

/**
 * get effective average cell occupancy
 */
template <unsigned dimension, typename T, typename U>
float const& ljfluid<dimension, T, U>::cell_occupancy() const
{
    return cell_occupancy_;
}

/**
 * get number of placeholders per cell
 */
template <unsigned dimension, typename T, typename U>
unsigned int const& ljfluid<dimension, T, U>::cell_size() const
{
    return cell_size_;
}
#endif

/**
 * get number of CUDA execution blocks
 */
template <unsigned dimension, typename T, typename U>
unsigned int ljfluid<dimension, T, U>::blocks() const
{
    return dim_.blocks_per_grid();
}

/**
 * get number of particles
 */
template <unsigned dimension, typename T, typename U>
unsigned int ljfluid<dimension, T, U>::threads() const
{
    return dim_.threads_per_block();
}

/**
 * get particle density
 */
template <unsigned dimension, typename T, typename U>
float const& ljfluid<dimension, T, U>::density() const
{
    return density_;
}

/**
 * get periodic box length
 */
template <unsigned dimension, typename T, typename U>
float const& ljfluid<dimension, T, U>::box() const
{
    return box_;
}

/**
 * get simulation timestep
 */
template <unsigned dimension, typename T, typename U>
float const& ljfluid<dimension, T, U>::timestep() const
{
    return timestep_;
}

/**
 * get potential cutoff distance
 */
template <unsigned dimension, typename T, typename U>
float const& ljfluid<dimension, T, U>::cutoff_distance() const
{
    return r_cut;
}

/**
 * stream MD simulation step on GPU
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::mdstep()
{
#ifdef USE_CELL
    // first leapfrog step of integration of differential equations of motion
    try {
	event_[1].record(stream_);
	cuda::configure(dim_cell_.grid, dim_cell_.block, stream_);
	gpu::ljfluid::inteq(g_cell.r.data(), g_cell.R.data(), g_cell.v.data(), g_cell.f.data());
	event_[2].record(stream_);
    }
    catch (cuda::error const& e) {
	throw exception("failed to stream first leapfrog step on GPU");
    }

    // update cell lists
    try {
	cuda::configure(dim_cell_.grid, dim_cell_.block, stream_);
	gpu::ljfluid::update_cells(g_cell.r.data(), g_cell.R.data(), g_cell.v.data(), g_cell.n.data(), g_cell2.r.data(), g_cell2.R.data(), g_cell2.v.data(), g_cell2.n.data());
	event_[3].record(stream_);

	cuda::copy(g_cell2.r, g_cell.r, stream_);
	cuda::copy(g_cell2.R, g_cell.R, stream_);
	cuda::copy(g_cell2.v, g_cell.v, stream_);
	cuda::copy(g_cell2.n, g_cell.n, stream_);
	event_[4].record(stream_);
    }
    catch (cuda::error const& e) {
	throw exception("failed to stream cell list update on GPU");
    }

    // Lennard-Jones force calculation
    try {
	cuda::configure(dim_cell_.grid, dim_cell_.block, stream_);
	gpu::ljfluid::mdstep(g_cell.r.data(), g_cell.v.data(), g_cell.f.data(), g_cell.n.data(), g_cell.en.data(), g_cell.virial.data());
	event_[0].record(stream_);
    }
    catch (cuda::error const& e) {
	throw exception("failed to stream force calculation on GPU");
    }
#else
    // first leapfrog step of integration of differential equations of motion
    try {
	event_[1].record(stream_);
	cuda::configure(dim_.grid, dim_.block, stream_);
	gpu::ljfluid::inteq(g_part.r.data(), g_part.R.data(), g_part.v.data(), g_part.f.data());
	event_[2].record(stream_);
    }
    catch (cuda::error const& e) {
	throw exception("failed to stream first leapfrog step on GPU");
    }

    // Lennard-Jones force calculation
    try {
	cuda::configure(dim_.grid, dim_.block, dim_.threads_per_block() * sizeof(U), stream_);
	gpu::ljfluid::mdstep(g_part.r.data(), g_part.v.data(), g_part.f.data(), g_part.en.data(), g_part.virial.data());
	event_[0].record(stream_);
    }
    catch (cuda::error const& e) {
	throw exception("failed to stream force calculation on GPU");
    }
#endif
}

/**
 * synchronize MD simulation step on GPU
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::synchronize()
{
    try {
	// wait for MD simulation step on GPU to finish
	event_[0].synchronize();
    }
    catch (cuda::error const& e) {
	throw exception("MD simulation step on GPU failed");
    }

    // accumulate total MD step GPU time
    times_["gpu"]["mdstep"] += event_[0] - event_[1];
    // accumulate leapfrog step of integration GPU kernel time
    times_["gpu"]["verlet"] += event_[2] - event_[1];
#ifdef USE_CELL
    // accumulate Lennard-Jones force calculation GPU kernel time
    times_["gpu"]["ljforce"] += event_[0] - event_[4];
    // accumulate cell lists update GPU kernel time
    times_["gpu"]["update_cells"] += event_[3] - event_[2];
    // accumulate cell lists update GPU mem time
    times_["memcpy"]["update_cells"] += event_[4] - event_[3];
#else
    // accumulate Lennard-Jones force calculation GPU kernel time
    times_["gpu"]["ljforce"] += event_[0] - event_[2];
#endif
}

/**
 * copy MD simulation step results from GPU to host
 */
template <unsigned dimension, typename T, typename U>
void ljfluid<dimension, T, U>::sample()
{
#ifdef USE_CELL
    // copy MD simulation step results from GPU to host
    try {
	event_[1].record(stream_);
	// copy periodically reduce particles positions
	cuda::copy(g_cell.r, h_cell.r, stream_);
	// copy periodically extended particles positions
	cuda::copy(g_cell.R, h_cell.R, stream_);
	// copy particle velocities
	cuda::copy(g_cell.v, h_cell.v, stream_);
	// copy particle number tags
	cuda::copy(g_cell.n, h_cell.n, stream_);
	// copy potential energies per particle and virial equation sums
	cuda::copy(g_cell.en, h_cell.en, stream_);
	// copy virial equation sums per particle
	cuda::copy(g_cell.virial, h_cell.virial, stream_);
	event_[0].record(stream_);

	// wait for CUDA operations to finish
	event_[0].synchronize();
    }
    catch (cuda::error const& e) {
	throw exception("failed to copy MD simulation step results from GPU to host");
    }

    // mean potential energy per particle
    en_pot_ = 0;
    // mean virial equation sum per particle
    virial_ = 0;
    // number of particles found in cells
    unsigned int count = 0;

    for (unsigned int i = 0; i < nplace; ++i) {
	// check if real particle
	if (IS_REAL_PARTICLE(h_cell.n[i])) {
	    // copy periodically reduced particle positions
	    h_part.r[h_cell.n[i]] = h_cell.r[i];
	    // copy periodically extended particle positions
	    h_part.R[h_cell.n[i]] = h_cell.R[i];
	    // copy particle velocities
	    h_part.v[h_cell.n[i]] = h_cell.v[i];

	    count++;

	    // calculate mean potential energy per particle
	    en_pot_ += (h_cell.en[i] - en_pot_) / count;
	    // calculate mean virial equation sum per particle
	    virial_ += (h_cell.virial[i] - virial_) / count;
	}
    }
    // validate number of particles
    if (count != npart) {
	throw exception("particle loss while updating cell lists");
    }
#else
    // copy MD simulation step results from GPU to host
    try {
	event_[1].record(stream_);
	// copy periodically reduce particles positions
	cuda::copy(g_part.r, h_part.r, stream_);
	// copy periodically extended particles positions
	cuda::copy(g_part.R, h_part.R, stream_);
	// copy particle velocities
	cuda::copy(g_part.v, h_part.v, stream_);
	// copy potential energies per particle and virial equation sums
	cuda::copy(g_part.en, h_part.en, stream_);
	// copy virial equation sums per particle
	cuda::copy(g_part.virial, h_part.virial, stream_);
	event_[0].record(stream_);

	// wait for CUDA operations to finish
	event_[0].synchronize();
    }
    catch (cuda::error const& e) {
	throw exception("failed to copy MD simulation step results from GPU to host");
    }

    // calculate mean potential energy per particle
    en_pot_ = mean(h_part.en.begin(), h_part.en.end());
    // calculate mean virial equation sum per particle
    virial_ = mean(h_part.virial.begin(), h_part.virial.end());
#endif

    // ensure that system is still in valid state after MD step
    if (std::isnan(en_pot_)) {
	throw exception("potential energy diverged due to excessive timestep or density");
    }

    // accumulate MD step GPU to host mem time
    times_["memcpy"]["sample"] += event_[0] - event_[1];
}

/**
 * sample trajectory
 */
template <unsigned dimension, typename T, typename U>
template <typename V>
void ljfluid<dimension, T, U>::sample(V visitor) const
{
    visitor(h_part.r, h_part.R, h_part.v, en_pot_, virial_);
}

/**
 * get CUDA execution time statistics
 */
template <unsigned dimension, typename T, typename U>
perf_type const& ljfluid<dimension, T, U>::times() const
{
    return times_;
}

} // namespace mdsim

#undef foreach

#endif /* ! MDSIM_LJFLUID_HPP */
