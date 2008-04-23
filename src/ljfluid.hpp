/* Lennard-Jones fluid
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

#include <math.h>
#include <cuda_wrapper/cuda_wrapper.hpp>
#include "gpu/ljfluid_glue.hpp"
#include "rand48.hpp"


namespace mdsim
{

/**
 * MD simulation particle
 */
template <typename T>
struct particle
{
    /** n-dimensional particle coordinates */
    cuda::vector<T> pos_gpu;
    cuda::vector<T> pos_old_gpu;
    cuda::host::vector<T> pos;
    /** n-dimensional particle velocity */
    cuda::vector<T> vel_gpu;
    cuda::host::vector<T> vel;
    /** n-dimensional force acting upon particle */
    cuda::vector<T> force_gpu;
    /** potential energy */
    cuda::vector<float> en_gpu;
    cuda::host::vector<float> en;
    /** virial equation sum */
    cuda::vector<float> virial_gpu;
    cuda::host::vector<float> virial;

    particle(size_t n) : pos_gpu(n), pos_old_gpu(n), pos(n), vel_gpu(n), vel(n), force_gpu(n), en_gpu(n), en(n), virial_gpu(n), virial(n)
    {
    }

    /**
     * MD simulation state in global device memory
     */
    mdstep_param<T> data()
    {
	mdstep_param<T> state;

	state.r = pos_gpu.data();
#ifndef USE_LEAPFROG
	state.rm = pos_old_gpu.data();
#endif
	state.v = vel_gpu.data();
	state.f = force_gpu.data();
	state.en = en_gpu.data();
	state.virial = virial_gpu.data();

	return state;
    }
};


/**
 * Simulate a Lennard-Jones fluid with naive N-squared algorithm
 */
template <typename T>
class ljfluid
{
public:
    ljfluid(size_t npart, cuda::config const& dim);

    size_t particles() const;
    float timestep();
    void timestep(float val);
    float density() const;
    void density(float density_);
    float box() const;
    void temperature(float temp, rand48& rng);

    void step(double& en_pot, double& virial, T& vel_cm, double& vel2_sum);
    void trajectories(std::ostream& os) const;

private:
    /** number of particles in periodic box */
    size_t npart;
#ifdef DIM_3D
    /** particles */
    particle<float3> part;
    /** MD simulation state in global device memory */
    const mdstep_param<float3> state_gpu;
#else
    /** particles */
    particle<float2> part;
    /** MD simulation state in global device memory */
    const mdstep_param<float2> state_gpu;
#endif
    /** CUDA execution dimensions */
    cuda::config dim_;

    /** particle density */
    float density_;
    /** periodic box length */
    float box_;
    /** MD simulation timestep */
    float timestep_;
    /** cutoff distance for shifted Lennard-Jones potential */
    float r_cut;
};


/**
 * initialize Lennard-Jones fluid with given particle number
 */
template <typename T>
ljfluid<T>::ljfluid(size_t npart, cuda::config const& dim) : npart(npart), part(npart), state_gpu(part.data()), dim_(dim)
{
    // FIXME do without this requirement
    assert(npart == dim_.threads());

    // fixed cutoff distance for shifted Lennard-Jones potential
    r_cut = 2.5;
    // squared cutoff distance
    float rr_cut = r_cut * r_cut;

    // potential energy at cutoff distance
    float rri_cut = 1. / rr_cut;
    float r6i_cut = rri_cut * rri_cut * rri_cut;
    float en_cut = 4. * r6i_cut * (r6i_cut - 1.);

    gpu::ljfluid::rr_cut = rr_cut;
    gpu::ljfluid::en_cut = en_cut;
}

/**
 * get number of particles in periodic box
 */
template <typename T>
size_t ljfluid<T>::particles() const
{
    return npart;
}

/**
 * get simulation timestep
 */
template <typename T>
float ljfluid<T>::timestep()
{
    return timestep_;
}

/**
 * set simulation timestep
 */
template <typename T>
void ljfluid<T>::timestep(float timestep_)
{
    this->timestep_ = timestep_;
    gpu::ljfluid::timestep = timestep_;
}

/**
 * get particle density
 */
template <typename T>
float ljfluid<T>::density() const
{
    return density_;
}

/**
 * set particle density
 */
template <typename T>
void ljfluid<T>::density(float density_)
{
    // particle density
    this->density_ = density_;
    // periodic box length
    box_ = pow(npart / density_, 1.0 / T::dim());
    gpu::ljfluid::box = box_;

    // number of particles along one lattice dimension
    unsigned int n = size_t(ceil(pow(npart, 1.0 / T::dim())));
    // lattice distance
    float a = box_ / n;

    // initialize coordinates
    gpu::ljfluid::init_lattice.configure(dim_);
#ifdef DIM_3D
    gpu::ljfluid::init_lattice(part.pos_gpu.data(), make_float3(a, a, a), n);
#else
    gpu::ljfluid::init_lattice(part.pos_gpu.data(), make_float2(a, a), n);
#endif
    cuda::thread::synchronize();

    part.pos.memcpy(part.pos_gpu);
}

/**
 * get periodic box length
 */
template <typename T>
float ljfluid<T>::box() const
{
    return box_;
}

/**
 * set temperature
 */
template <typename T>
void ljfluid<T>::temperature(float temp, rand48& rng)
{
    // initialize velocities
    gpu::ljfluid::init_vel.configure(dim_);
    gpu::ljfluid::init_vel(state_gpu, temp, rng.data());

    // initialize forces
    gpu::ljfluid::init_forces.configure(dim_);
    gpu::ljfluid::init_forces(part.force_gpu.data());
    cuda::thread::synchronize();

    part.vel.memcpy(part.vel_gpu);
}

/**
 * MD simulation step
 */
template <typename T>
void ljfluid<T>::step(double& en_pot, double& virial, T& vel_cm, double& vel2_sum)
{
#ifdef DIM_3D
    // reserve shared device memory for particle coordinates
    gpu::ljfluid::mdstep.configure(dim_, dim_.threads_per_block() * sizeof(float3));
#else
    gpu::ljfluid::mdstep.configure(dim_, dim_.threads_per_block() * sizeof(float2));
#endif
    gpu::ljfluid::mdstep(state_gpu);
    cuda::thread::synchronize();

    part.pos.memcpy(part.pos_gpu);
    part.vel.memcpy(part.vel_gpu);
    part.en.memcpy(part.en_gpu);
    part.virial.memcpy(part.virial_gpu);

    // compute averages
    en_pot = 0.;
    virial = 0.;
    vel_cm = 0.;
    vel2_sum = 0.;

    for (size_t i = 0; i < npart; ++i) {
	en_pot += part.en[i];
	virial += part.virial[i];
#ifdef DIM_3D
	T vel(part.vel[i].x, part.vel[i].y, part.vel[i].z);
#else
	T vel(part.vel[i].x, part.vel[i].y);
#endif
	vel_cm += vel;
	vel2_sum += vel * vel;
    }

    en_pot /= npart;
    virial /= npart;
    vel_cm /= npart;
    vel2_sum /= npart;
}

/**
 * write particle coordinates and velocities to output stream
 */
template <typename T>
void ljfluid<T>::trajectories(std::ostream& os) const
{
    for (size_t i = 0; i < npart; ++i) {
#ifdef DIM_3D
	os << part.pos[i].x << "\t" << part.pos[i].y << "\t" << part.pos[i].z << "\t";
	os << part.vel[i].x << "\t" << part.vel[i].y << "\t" << part.vel[i].z << std::endl;
#else
	os << part.pos[i].x << "\t" << part.pos[i].y << "\t";
	os << part.vel[i].x << "\t" << part.vel[i].y << std::endl;
#endif
    }
    os << std::endl << std::endl;
}

}

#endif /* ! MDSIM_LJFLUID_HPP */
