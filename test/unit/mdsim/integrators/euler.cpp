/*
 * Copyright © 2011  Michael Kopp and Felix Höfling
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

#define BOOST_TEST_MODULE euler
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <boost/assign.hpp>
#include <boost/make_shared.hpp>
#include <functional>
#include <limits>

#include <halmd/algorithm/gpu/apply_kernel.hpp>
#include <halmd/mdsim/box.hpp>                          // dependency of euler module
#include <halmd/mdsim/clock.hpp>                        // dependency of phase_space module
#include <halmd/mdsim/host/integrators/euler.hpp>       // module to be tested
#include <halmd/mdsim/host/positions/lattice.hpp>       // for initialisation
#include <halmd/mdsim/host/velocities/boltzmann.hpp>    // for initialisation
#include <halmd/observables/host/phase_space.hpp>       // for unified reading of positions and velocities
#include <halmd/observables/host/samples/particle_group.hpp>
#include <halmd/random/host/random.hpp>                 // dependency of modules position, velocity
#ifdef WITH_CUDA
# include <halmd/mdsim/gpu/integrators/euler.hpp>
# include <halmd/mdsim/gpu/positions/lattice.hpp>
# include <halmd/mdsim/gpu/velocities/boltzmann.hpp>
# include <halmd/observables/gpu/phase_space.hpp>
# include <halmd/observables/gpu/samples/particle_group.hpp>
# include <halmd/random/gpu/random.hpp>
# include <halmd/utility/gpu/device.hpp>
# include <include/cuda_wrapper/cuda_wrapper.hpp>
#endif

using namespace boost;
using namespace boost::assign; // list_of
using namespace halmd;
using namespace std;

/** test Euler integrator: ordinary differential equations of 1st order
 *
 * Two differential equations are solved using the Euler integrator,
 * @f\[
 *   \dot r = v = \mathit{const} \quad \text{and} \quad  \dot r = -r \, .
 * @f\]
 * The results are then compared to the algebraic solutions of the
 * numerical scheme to properly account for discretisation errors,
 * @f\[
 *   r(n) = r_0 + v \, dt \, n
 *   \quad \text{and} \quad
 *   r(n) = (1 - dt)^n \, r_0 ,
 * @f\]
 * with \f$n\f$ the number of steps taken.
 *
 */

template <typename modules_type>
struct test_euler
{
    typedef typename modules_type::box_type box_type;
    typedef typename modules_type::integrator_type integrator_type;
    typedef typename modules_type::particle_group_type particle_group_type;
    typedef typename particle_group_type::particle_type particle_type;
    typedef typename modules_type::position_type position_type;
    typedef typename modules_type::random_type random_type;
    typedef typename modules_type::velocity_type velocity_type;
    typedef mdsim::clock clock_type;
    typedef typename modules_type::sample_type sample_type;
    typedef typename modules_type::phase_space_type phase_space_type;
    typedef typename modules_type::sample_type::sample_vector sample_vector_type; // positions/velocities

    typedef typename particle_type::vector_type vector_type;
    typedef typename vector_type::value_type float_type;
    static unsigned int const dimension = vector_type::static_size;

    typedef typename modules_type::numeric_limits numeric_limits;
    static bool const gpu = modules_type::gpu;

    size_t steps;
    double density;
    double temp;
    double timestep;
    unsigned int npart;
    fixed_vector<double, dimension> box_ratios;
    fixed_vector<double, dimension> slab;

    shared_ptr<box_type> box;
    shared_ptr<particle_type> particle;
    shared_ptr<integrator_type> integrator;
    shared_ptr<random_type> random;
    shared_ptr<position_type> position;
    shared_ptr<velocity_type> velocity;
    shared_ptr<clock_type> clock;
    shared_ptr<sample_type> sample;
    shared_ptr<phase_space_type> phase_space;

    test_euler();
    void linear_motion();
    void overdamped_motion();
};

/** solve the differential equation @f$ \dot r = v = const @f$ */
template <typename modules_type>
void test_euler<modules_type>::linear_motion()
{
    // store initial positions and velocities
    phase_space->acquire();                      // copy data from particle to host sample
    sample_type initial_sample = *sample;        // only copy internal boost::shared_ptr
    sample->reset();                             // detach shared_ptrs and reset sample's time stamp

    // perform integration
    BOOST_TEST_MESSAGE("running Euler integration for linear motion over " << steps << " steps");
    for (size_t i = 0; i < steps; ++i) {
        integrator->integrate();
        integrator->finalize();
    }

    // acquire sample with final positions and velocities
    phase_space->acquire();

    // particlewise comparison with analytic solution
    // the absolute error should be relative to the maximum value, i.e., the box length
    float_type tolerance = 4 * steps * numeric_limits::epsilon() * norm_inf(box->length());
    float_type duration = steps * integrator->timestep();
    float_type max_deviation = 0;
    for (size_t i = 0; i < npart; ++i) {
        vector_type const& r0 = (*initial_sample.r)[i];
        vector_type const& v0 = (*initial_sample.v)[i];
        vector_type const& r_final = (*sample->r)[i];

        vector_type r_analytic = r0 + duration * v0;

        // check that maximum deviation of a vector component is less than the tolerance
        BOOST_CHECK_SMALL(norm_inf(r_final - r_analytic), tolerance);
        max_deviation = std::max(norm_inf(r_final - r_analytic), max_deviation);
    }
    BOOST_TEST_MESSAGE("Maximum deviation: " << max_deviation << ", tolerance: " << tolerance);
    BOOST_CHECK_LT(max_deviation, tolerance);
}

/** solve the differential equation @f$ \dot r = - r @f$ */
template <typename modules_type>
void test_euler<modules_type>::overdamped_motion()
{
    // store initial positions and velocities
    phase_space->acquire();                      // copy data from particle to host sample
    sample_type initial_sample = *sample;        // only copy internal boost::shared_ptr
    sample->reset();                             // detach shared_ptrs and reset sample's time stamp

    // reduce number of steps as the test runs much slower
    // and the outcome can't be well represented by float
    steps /= gpu ? 100 : 10;

    // perform integration
    BOOST_TEST_MESSAGE("running Euler integration for overdamped motion over " << steps << " steps");
    for (size_t i = 0; i < steps; ++i) {
        modules_type::set_velocity(particle); // set particle velocity: v = -r
        integrator->integrate();
        integrator->finalize();
    }

    // acquire sample with final positions and velocities
    phase_space->acquire();

    // particlewise comparison with analytic solution
    // r_n = r_0 * (1 - Δt)^n → r_0 * exp(-n Δt)
    float_type factor = pow(1 - integrator->timestep(), static_cast<double>(steps));
    float_type max_deviation = 0;
    for (size_t i = 0; i < npart; ++i) {
        vector_type const& r0 = (*initial_sample.r)[i];
        vector_type const& r_final = (*sample->r)[i];

        vector_type r_analytic = r0 * factor;

        // Check that maximum deviation of a vector component is less than the tolerance
        // the tolerance is computed by summing up all errors:
        // @f$$ E_{total} = \epsilon \, \sum_n x_n = \epsilon (x_0 - x_n) \frac{1-\Delta t}{\Delta t} @f$$
        // and @f$\epsilon @f$ is the relative error for one addition.
        float_type tolerance_step = numeric_limits::epsilon() * norm_inf(r0 - r_final) * (1 - timestep) / timestep;
        tolerance_step = std::max(tolerance_step, numeric_limits::min()); // avoid "0 < 0"
        BOOST_CHECK_SMALL(norm_inf(r_final - r_analytic), tolerance_step);
        max_deviation = std::max(norm_inf(r_final - r_analytic), max_deviation);
    }
    BOOST_TEST_MESSAGE("Maximum deviation: " << max_deviation);
}

/**
 * Initialize integrator and dependencies, set basic parameters.
 */
template <typename modules_type>
test_euler<modules_type>::test_euler()
{
    BOOST_TEST_MESSAGE("initialise simulation modules");

    // set test parameters
    steps = 1000000; // run for as many steps as possible, wrap around the box for about 10 times

    // set module parameters
    density = 1e-6; // a low density implies large values of the position vectors
    temp = 1; // the temperature defines the average velocities
    timestep = 0.001; // small, but typical timestep
    npart = gpu ? 4000 : 108; // optimize filling of fcc lattice, use only few particles on the host
    box_ratios = (dimension == 3) ? list_of<double>(1)(2)(1.01) : list_of<double>(1)(2);
    slab = 1;

    vector<unsigned int> npart_vector = list_of(npart);
    vector<double> mass = list_of(1);

    // create modules
    particle = make_shared<particle_type>(npart_vector, mass);
    box = make_shared<box_type>(npart, density, box_ratios);
    integrator = make_shared<integrator_type>(particle, box, timestep);
    random = make_shared<random_type>();
    position = make_shared<position_type>(particle, box, random, slab);
    velocity = make_shared<velocity_type>(particle, random, temp);
    clock = make_shared<clock_type>(1);
    sample = make_shared<sample_type>(particle->nbox);
    phase_space = make_shared<phase_space_type>(sample, make_shared<particle_group_type>(particle), box, clock);

    // set positions and velocities
    BOOST_TEST_MESSAGE("set particle tags");
    particle->set();
    BOOST_TEST_MESSAGE("position particles on lattice");
    position->set();
    BOOST_TEST_MESSAGE("set particle velocities");
    velocity->set();
}

/**
 * Specify concretely which modules to use: Host modules.
 */
template <int dimension, typename float_type>
struct host_modules
{
    typedef mdsim::box<dimension> box_type;
    typedef observables::host::samples::particle_group_all<dimension, float_type> particle_group_type;
    typedef typename particle_group_type::particle_type particle_type;
    typedef mdsim::host::integrators::euler<dimension, float_type> integrator_type;
    typedef halmd::random::host::random random_type;
    typedef mdsim::host::positions::lattice<dimension, float_type> position_type;
    typedef mdsim::host::velocities::boltzmann<dimension, float_type> velocity_type;
    typedef observables::host::samples::phase_space<dimension, float_type> sample_type;
    typedef observables::host::phase_space<dimension, float_type> phase_space_type;

    typedef typename std::numeric_limits<float_type> numeric_limits;

    static bool const gpu = false;

    static void set_velocity(shared_ptr<particle_type> particle);
};

/** host specific helper function: set particle velocity to v = -r */
template <int dimension, typename float_type>
void host_modules<dimension, float_type>::set_velocity(shared_ptr<particle_type> particle)
{
    // copy -r[i] to v[i]
    transform(
        particle->r.begin(), particle->r.end()
      , particle->v.begin()
      , negate<typename particle_type::vector_type>()
    );
}

BOOST_AUTO_TEST_CASE( euler_host_2d_linear ) {
    test_euler<host_modules<2, double> >().linear_motion();
}
BOOST_AUTO_TEST_CASE( euler_host_3d_linear ) {
    test_euler<host_modules<3, double> >().linear_motion();
}

BOOST_AUTO_TEST_CASE( euler_host_2d_overdamped ) {
    test_euler<host_modules<2, double> >().overdamped_motion();
}
BOOST_AUTO_TEST_CASE( euler_host_3d_overdamped ) {
    test_euler<host_modules<3, double> >().overdamped_motion();
}

#ifdef WITH_CUDA
/**
 * Specify concretely which modules to use: Gpu modules.
 */
template <int dimension, typename float_type>
struct gpu_modules
{
    typedef mdsim::box<dimension> box_type;
    typedef observables::gpu::samples::particle_group_all<dimension, float_type> particle_group_type;
    typedef typename particle_group_type::particle_type particle_type;
    typedef mdsim::gpu::integrators::euler<dimension, float_type> integrator_type;
    typedef halmd::random::gpu::random<halmd::random::gpu::rand48> random_type;
    typedef mdsim::gpu::positions::lattice<dimension, float_type, halmd::random::gpu::rand48> position_type;
    typedef mdsim::gpu::velocities::boltzmann<dimension, float_type, halmd::random::gpu::rand48> velocity_type;
    typedef observables::host::samples::phase_space<dimension, float_type> sample_type;
    typedef observables::gpu::phase_space<sample_type> phase_space_type;

    static bool const gpu = true;

#ifndef USE_VERLET_DSFUN
    typedef typename std::numeric_limits<float_type> numeric_limits;
#else
    // FIXME define numeric_limits for dsfloat
    // see, e.g., http://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
    struct numeric_limits {
        static float_type epsilon() { return std::pow(float_type(2), -44); }
        static float_type min() { return std::numeric_limits<float>::min(); }
    };
#endif

    static void set_velocity(shared_ptr<particle_type> particle);
};

/** GPU specific helper function: set particle velocity to v = -r */
template <int dimension, typename float_type>
void gpu_modules<dimension, float_type>::set_velocity(shared_ptr<particle_type> particle)
{
    using namespace algorithm::gpu;
    typedef typename particle_type::vector_type vector_type;
    typedef apply_wrapper<negate_, vector_type, float4, vector_type, float4> apply_negate_wrapper;

    // copy -g_r[i] to g_v[i]
    //
    // The kernel wrapper is declared in algorithm/gpu/apply_kernel.hpp
    // and the CUDA kernel is instantiated in apply_negate.cu.
    //
    // If the arrays are stored as two subsequent float4 arrays for
    // double-single representation, we the negation is applied to both floats
    // independently (in correspondence to the definition of operator- for
    // dsfloat).
    //
    // Caveat: overwrites particle tags in g_v (which are not used anyway)
    try {
        cuda::configure(particle->dim.grid, particle->dim.block);
        apply_negate_wrapper::kernel.apply(particle->g_r, particle->g_v, particle->g_r.capacity());
        cuda::thread::synchronize();
    }
    catch (cuda::error const&) {
        LOG_ERROR("copying negated positions to velocities on GPU failed");
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( euler_gpu_2d_linear, device ) {
    test_euler<gpu_modules<2, float> >().linear_motion();
}
BOOST_FIXTURE_TEST_CASE( euler_gpu_3d_linear, device ) {
    test_euler<gpu_modules<3, float> >().linear_motion();
}

BOOST_FIXTURE_TEST_CASE( euler_gpu_2d_overdamped, device ) {
    test_euler<gpu_modules<2, float> >().overdamped_motion();
}
BOOST_FIXTURE_TEST_CASE( euler_gpu_3d_overdamped, device ) {
    test_euler<gpu_modules<3, float> >().overdamped_motion();
}
#endif // WITH_CUDA
