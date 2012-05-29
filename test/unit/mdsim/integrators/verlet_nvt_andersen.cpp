/*
 * Copyright © 2010-2012  Felix Höfling
 * Copyright © 2010-2011  Peter Colberg
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

#include <halmd/config.hpp>

#define BOOST_TEST_MODULE verlet_nvt_andersen
#include <boost/test/unit_test.hpp>

#include <boost/assign.hpp>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/numeric/ublas/banded.hpp>
#include <cmath>
#include <numeric>

#include <halmd/mdsim/box.hpp>
#include <halmd/mdsim/clock.hpp>
#include <halmd/mdsim/core.hpp>
#include <halmd/mdsim/host/integrators/verlet_nvt_andersen.hpp>
#include <halmd/mdsim/host/particle.hpp>
#include <halmd/mdsim/host/positions/lattice.hpp>
#include <halmd/mdsim/host/velocities/boltzmann.hpp>
#include <halmd/mdsim/particle_groups/from_range.hpp>
#include <halmd/numeric/accumulator.hpp>
#include <halmd/observables/host/thermodynamics.hpp>
#include <halmd/random/host/random.hpp>
#ifdef HALMD_WITH_GPU
# include <halmd/mdsim/gpu/integrators/verlet_nvt_andersen.hpp>
# include <halmd/mdsim/gpu/particle.hpp>
# include <halmd/mdsim/gpu/positions/lattice.hpp>
# include <halmd/mdsim/gpu/velocities/boltzmann.hpp>
# include <halmd/observables/gpu/thermodynamics.hpp>
# include <halmd/random/gpu/random.hpp>
# include <halmd/utility/gpu/device.hpp>
#endif
#include <test/tools/ctest.hpp>

using namespace boost;
using namespace boost::assign; // list_of
using namespace halmd;
using namespace std;

/**
 * test NVT Verlet integrator with stochastic Andersen thermostat
 */
template <typename modules_type>
struct verlet_nvt_andersen
{
    typedef typename modules_type::box_type box_type;
    typedef typename modules_type::integrator_type integrator_type;
    typedef typename modules_type::particle_type particle_type;
    typedef halmd::mdsim::particle_groups::from_range<particle_type> particle_group_type;
    typedef typename modules_type::position_type position_type;
    typedef typename modules_type::random_type random_type;
    typedef typename modules_type::thermodynamics_type thermodynamics_type;
    typedef typename modules_type::velocity_type velocity_type;
    static bool const gpu = modules_type::gpu;

    typedef typename particle_type::vector_type vector_type;
    typedef typename vector_type::value_type float_type;
    static unsigned int const dimension = vector_type::static_size;
    typedef mdsim::clock clock_type;
    typedef typename clock_type::time_type time_type;
    typedef typename clock_type::step_type step_type;
    typedef mdsim::core core_type;

    time_type timestep;
    float density;
    float temp;
    double coll_rate;
    unsigned int npart;
    fixed_vector<double, dimension> box_ratios;
    fixed_vector<double, dimension> slab;

    boost::shared_ptr<box_type> box;
    boost::shared_ptr<clock_type> clock;
    boost::shared_ptr<core_type> core;
    boost::shared_ptr<integrator_type> integrator;
    boost::shared_ptr<particle_type> particle;
    boost::shared_ptr<position_type> position;
    boost::shared_ptr<random_type> random;
    boost::shared_ptr<thermodynamics_type> thermodynamics;
    boost::shared_ptr<velocity_type> velocity;

    void test();
    verlet_nvt_andersen();
    void connect();
};

template <typename modules_type>
void verlet_nvt_andersen<modules_type>::test()
{
    // run for Δt*=500
    step_type steps = static_cast<step_type>(ceil(500 / timestep));
    // ensure that sampling period is sufficiently large such that
    // the samples can be considered independent
    step_type period = static_cast<step_type>(round(3. / (coll_rate * timestep)));
    accumulator<double> temp_;
    boost::array<accumulator<double>, dimension> v_cm;   //< accumulate velocity component-wise

    core->setup();
    BOOST_TEST_MESSAGE("run NVT integrator over " << steps << " steps");
    clock->set_timestep(integrator->timestep());
    for (step_type i = 0; i < steps; ++i) {
        clock->advance();
        core->mdstep();
        if(i % period == 0) {
            temp_(thermodynamics->temp());
            fixed_vector<double, dimension> v(thermodynamics->v_cm());
            for (unsigned int i = 0; i < dimension; ++i) {
                v_cm[i](v[i]);
            }
        }
    }

    //
    // test velocity distribution of final state
    //
    // centre-of-mass velocity ⇒ mean of velocity distribution
    // each particle is an independent "measurement",
    // tolerance is 4.5σ, σ = √(<v_x²> / (N - 1)) where <v_x²> = k T,
    // with this choice, a single test passes with 99.999% probability
    double vcm_tolerance = 4.5 * sqrt(temp / (npart - 1));
    BOOST_TEST_MESSAGE("Absolute tolerance on instantaneous centre-of-mass velocity: " << vcm_tolerance);
    BOOST_CHECK_SMALL(norm_inf(thermodynamics->v_cm()), vcm_tolerance);  //< norm_inf tests the max. value

    // temperature ⇒ variance of velocity distribution
    // we have only one measurement of the variance,
    // tolerance is 4.5σ, σ = √<ΔT²> where <ΔT²> / T² = 2 / (dimension × N)
    double rel_temp_tolerance = 4.5 * sqrt(2. / (dimension * npart)) / temp;
    BOOST_TEST_MESSAGE("Relative tolerance on instantaneous temperature: " << rel_temp_tolerance);
    BOOST_CHECK_CLOSE_FRACTION(thermodynamics->temp(), temp, rel_temp_tolerance);

    //
    // test velocity distribution averaged over the whole simulation run
    //
    // centre-of-mass velocity ⇒ mean of velocity distribution
    // #measurements = #particles × #samples,
    // tolerance is 4.5σ, σ = √(<v_x²> / (N × C - 1)) where <v_x²> = k T
    vcm_tolerance = 4.5 * sqrt(temp / (npart * count(v_cm[0]) - 1));
    BOOST_TEST_MESSAGE("Absolute tolerance on centre-of-mass velocity: " << vcm_tolerance);
    for (unsigned int i = 0; i < dimension; ++i) {
        BOOST_CHECK_SMALL(mean(v_cm[i]), vcm_tolerance);
        BOOST_CHECK_SMALL(error_of_mean(v_cm[i]), vcm_tolerance);
    }

    // mean temperature ⇒ variance of velocity distribution
    // each sample should constitute an independent measurement,
    // tolerance is 4.5σ, σ = √(<ΔT²> / (C - 1)) where <ΔT²> / T² = 2 / (dimension × N)
    rel_temp_tolerance = 4.5 * sqrt(2. / (dimension * npart * (count(temp_) - 1))) / temp;
    BOOST_TEST_MESSAGE("Relative tolerance on temperature: " << rel_temp_tolerance);
    BOOST_CHECK_CLOSE_FRACTION(mean(temp_), temp, rel_temp_tolerance);

    // specific heat per particle ⇒ temperature fluctuations
    // c_V = k × (dimension × N / 2)² <ΔT²> / T² / N = k × dimension / 2
    // where we have used <ΔT²> / T² = 2 / (dimension × N),
    // tolerance is 4.5σ, with the approximation
    // σ² = Var[ΔE² / (k T²)] / C → (dimension / 2) × (dimension + 6 / N) / C
    // (one measurement only from the average over C samples)
    double cv = pow(.5 * dimension, 2.) * npart * variance(temp_);
    double cv_variance=  (.5 * dimension) * (dimension + 6. / npart) / count(temp_);
    double rel_cv_tolerance = 4.5 * sqrt(cv_variance) / (.5 * dimension);
    BOOST_TEST_MESSAGE("Relative tolerance on specific heat: " << rel_cv_tolerance);
    BOOST_CHECK_CLOSE_FRACTION(cv, .5 * dimension, rel_cv_tolerance);
}

template <typename modules_type>
verlet_nvt_andersen<modules_type>::verlet_nvt_andersen()
{
    BOOST_TEST_MESSAGE("initialise simulation modules");

    // set module parameters
    density = 0.3;
    timestep = 0.01;
    temp = 1;
    coll_rate = 10;
    npart = gpu ? 5000 : 1500;
    box_ratios = (dimension == 3) ? list_of(1.)(2.)(1.01) : list_of(1.)(2.);
    double det = accumulate(box_ratios.begin(), box_ratios.end(), 1., multiplies<double>());
    double volume = npart / density;
    double edge_length = pow(volume / det, 1. / dimension);
    boost::numeric::ublas::diagonal_matrix<typename box_type::matrix_type::value_type> edges(dimension);
    for (unsigned int i = 0; i < dimension; ++i) {
        edges(i, i) = edge_length * box_ratios[i];
    }
    slab = 1;

    // create modules
    particle = boost::make_shared<particle_type>(npart);
    box = boost::make_shared<box_type>(edges);
    random = boost::make_shared<random_type>();
    position = boost::make_shared<position_type>(particle, box, slab);
    velocity = boost::make_shared<velocity_type>(particle, random, temp);
    integrator = boost::make_shared<integrator_type>(particle, box, random, timestep, temp, coll_rate);
    clock = boost::make_shared<clock_type>();
    boost::shared_ptr<particle_group_type> group = boost::make_shared<particle_group_type>(particle, 0, particle->nparticle());
    thermodynamics = boost::make_shared<thermodynamics_type>(group, box, clock);

    // create core and connect module slots to core signals
    this->connect();
}

template <typename modules_type>
void verlet_nvt_andersen<modules_type>::connect()
{
    core = boost::make_shared<core_type>();
    // system preparation
    core->on_prepend_setup( bind(&particle_type::prepare, particle) );
    core->on_setup( bind(&position_type::set, position) );
    core->on_setup( bind(&velocity_type::set, velocity) );
    // integration step
    core->on_integrate( bind(&integrator_type::integrate, integrator) );
    core->on_finalize( bind(&integrator_type::finalize, integrator) );
}

template <int dimension, typename float_type>
struct host_modules
{
    typedef mdsim::box<dimension> box_type;
    typedef mdsim::host::integrators::verlet_nvt_andersen<dimension, float_type> integrator_type;
    typedef mdsim::host::particle<dimension, float_type> particle_type;
    typedef mdsim::host::positions::lattice<dimension, float_type> position_type;
    typedef halmd::random::host::random random_type;
    typedef mdsim::host::velocities::boltzmann<dimension, float_type> velocity_type;
    typedef observables::host::thermodynamics<dimension, float_type> thermodynamics_type;
    static bool const gpu = false;
};

BOOST_AUTO_TEST_CASE( verlet_nvt_andersen_host_2d ) {
    verlet_nvt_andersen<host_modules<2, double> >().test();
}
BOOST_AUTO_TEST_CASE( verlet_nvt_andersen_host_3d ) {
    verlet_nvt_andersen<host_modules<3, double> >().test();
}

#ifdef HALMD_WITH_GPU
template <int dimension, typename float_type>
struct gpu_modules
{
    typedef mdsim::box<dimension> box_type;
    typedef mdsim::gpu::integrators::verlet_nvt_andersen<dimension, float_type, halmd::random::gpu::rand48> integrator_type;
    typedef mdsim::gpu::particle<dimension, float_type> particle_type;
    typedef mdsim::gpu::positions::lattice<dimension, float_type> position_type;
    typedef halmd::random::gpu::random<halmd::random::gpu::rand48> random_type;
    typedef observables::gpu::thermodynamics<dimension, float_type> thermodynamics_type;
    typedef mdsim::gpu::velocities::boltzmann<dimension, float_type, halmd::random::gpu::rand48> velocity_type;
    static bool const gpu = true;
};

BOOST_FIXTURE_TEST_CASE( verlet_nvt_andersen_gpu_2d, device ) {
    verlet_nvt_andersen<gpu_modules<2, float> >().test();
}
BOOST_FIXTURE_TEST_CASE( verlet_nvt_andersen_gpu_3d, device ) {
    verlet_nvt_andersen<gpu_modules<3, float> >().test();
}
#endif // HALMD_WITH_GPU
