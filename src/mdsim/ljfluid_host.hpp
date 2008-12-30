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

#ifndef MDSIM_LJFLUID_HOST_HPP
#define MDSIM_LJFLUID_HOST_HPP

#include <H5Cpp.h>
#include <boost/array.hpp>
#include <boost/function.hpp>
#include <boost/multi_array.hpp>
#include <list>
#include <vector>
#include "config.hpp"
#include "gsl_rng.hpp"
#include "perf.hpp"
#include "sample.hpp"

namespace mdsim
{

/**
 * MD simulation particle
 */
struct particle
{
    /** particle position */
    hvector r;
    /** particle velocity */
    hvector v;
    /** particle number tag */
    unsigned int n;

    /** particle force */
    hvector f;
    /** particle neighbours list */
    std::vector<particle*> neighbour;

    particle(hvector const& r, unsigned int n) : r(r), n(n) {}
    particle(hvector const& r, hvector const& v, unsigned int n) : r(r), v(v), n(n) {}
    particle() {}
};

/**
 * Lennard-Jones fluid simulation with cell lists and Verlet neighbour lists
 */
class ljfluid
{
public:
    typedef std::list<particle> cell_list;
    typedef cell_list::iterator cell_list_iterator;
    typedef cell_list::const_iterator cell_list_const_iterator;
    typedef boost::array<int, dimension> cell_index;

    /** trajectory sample visitor type */
    typedef boost::function<void (std::vector<hvector>&, std::vector<hvector>&)> trajectory_sample_visitor;

public:
    /** set number of particles */
    void particles(unsigned int value);
    /** set particle density */
    void density(double value);
    /** set periodic box length */
    void box(double value);
    /** set potential cutoff radius */
    void cutoff_radius(double value);
    /** initialize cell lists */
    void init_cell();
    /** set simulation timestep */
    void timestep(double value);

    /** set system state from phase space sample */
    void restore(trajectory_sample_visitor visitor);
    /** initialize random number generator with seed */
    void rng(unsigned int seed);
    /** initialize random number generator from state */
    void rng(rng::gsl::gfsr4::state_type const& state);
    /** place particles on a face-centered cubic (fcc) lattice */
    void lattice();
    /** set system temperature according to Maxwell-Boltzmann distribution */
    void temperature(double value);

    /** returns number of particles */
    unsigned int const& particles() const { return npart; }
    /** returns number of cells per dimension */
    unsigned int const& cells() const { return ncell; }
    /** returns particle density */
    double const& density() const { return density_; }
    /** returns periodic box length */
    double const& box() const { return box_; }
    /** returns potential cutoff radius */
    double const& cutoff_radius() const { return r_cut; }
    /** returns cell length */
    double const& cell_length() { return cell_length_; }
    /** returns simulation timestep */
    double const& timestep() const { return timestep_; }
    /** returns and resets CPU tick statistics */
    perf_counters times();

    /** write parameters to HDF5 parameter group */
    void attrs(H5::Group const& param) const;

    /** MD simulation step */
    void mdstep();
    /** get trajectory sample */
    trajectory_sample const& trajectory() const { return h_sample; }

private:
    /** update cell lists */
    void update_cells();
    /** returns cell list which a particle belongs to */
    cell_list& compute_cell(hvector const& r);
    /** update neighbour lists */
    void update_neighbours();
    /** update neighbour list of particle */
    template <bool same_cell> void compute_cell_neighbours(particle& p, cell_list& c);
    /** compute Lennard-Jones forces */
    void compute_forces();
    /** first leapfrog step of integration of equations of motion */
    void leapfrog_half();
    /** second leapfrog step of integration of equations of motion */
    void leapfrog_full();

private:
    /** number of particles */
    unsigned int npart;
    /** particle density */
    double density_;
    /** periodic box length */
    double box_;
    /** number of cells per dimension */
    unsigned int ncell;
    /** cell length */
    double cell_length_;
    /** simulation timestep */
    double timestep_;
    /** cutoff radius for shifted Lennard-Jones potential */
    double r_cut;
    /** neighbour list skin */
    double r_skin;
    /** cutoff radius with neighbour list skin */
    double r_cut_skin;

    /** trajectory sample */
    trajectory_sample h_sample;

    /** cell lists */
    boost::multi_array<cell_list, dimension> cell;

    /** random number generator */
    rng::gsl::gfsr4 rng_;
    /** squared cutoff radius */
    double rr_cut;
    /** potential energy at cutoff radius */
    double en_cut;
    /** squared cutoff radius with neighbour list skin */
    double rr_cut_skin;
    /** sum over maximum velocity magnitudes since last neighbour lists update */
    double v_max_sum;

    /** CPU tick statistics */
    perf_counters m_times;
};

} // namespace mdsim

#endif /* ! MDSIM_LJFLUID_HOST_HPP */
