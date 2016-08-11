/*
 * Copyright © 2016      Daniel Kirchner
 * Copyright © 2010-2012 Felix Höfling
 * Copyright © 2013      Nicolas Höft
 * Copyright © 2008-2012 Peter Colberg
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

#ifndef HALMD_MDSIM_GPU_PARTICLE_HPP
#define HALMD_MDSIM_GPU_PARTICLE_HPP

#include <halmd/mdsim/type_traits.hpp>
#include <halmd/utility/cache.hpp>
#include <halmd/utility/profiler.hpp>
#include <halmd/utility/signal.hpp>
#include <halmd/mdsim/force_kernel.hpp>
#include <halmd/mdsim/gpu/particle_data.hpp>

#include <cuda_wrapper/cuda_wrapper.hpp>
#include <lua.hpp>

#include <algorithm>
#include <vector>
#include <unordered_map>

namespace halmd {
namespace mdsim {
namespace gpu {

template <int dimension, typename float_type>
class particle
{
public:
    typedef halmd::signal<void ()> signal_type;
    typedef signal_type::slot_function_type slot_function_type;

    typedef typename type_traits<dimension, float_type>::vector_type vector_type;
    typedef typename type_traits<dimension, float>::gpu::coalesced_vector_type gpu_vector_type;

    typedef unsigned int size_type;
    typedef vector_type position_type;
    typedef vector_type image_type;
    typedef vector_type velocity_type;
    typedef unsigned int tag_type;
    typedef unsigned int reverse_tag_type;
    typedef unsigned int species_type;
    typedef float mass_type;
    typedef vector_type force_type;
    typedef float_type en_pot_type;
    typedef stress_tensor_wrapper<typename type_traits<dimension, float_type>::stress_tensor_type> stress_pot_type;

    typedef float4 gpu_position_type;
    typedef float4 gpu_velocity_type;
    typedef gpu_vector_type gpu_image_type;
    typedef tag_type gpu_tag_type;
    typedef reverse_tag_type gpu_reverse_tag_type;
    typedef gpu_vector_type gpu_force_type;
    typedef en_pot_type gpu_en_pot_type;
    typedef typename stress_pot_type::value_type gpu_stress_pot_type;

    typedef cuda::vector<gpu_position_type> position_array_type;
    typedef cuda::vector<gpu_image_type> image_array_type;
    typedef cuda::vector<gpu_velocity_type> velocity_array_type;
    typedef cuda::vector<gpu_tag_type> tag_array_type;
    typedef cuda::vector<gpu_reverse_tag_type> reverse_tag_array_type;
    typedef cuda::vector<gpu_force_type> force_array_type;
    typedef cuda::vector<gpu_en_pot_type> en_pot_array_type;
    typedef cuda::vector<gpu_stress_pot_type> stress_pot_array_type;

    void rearrange(cuda::vector<unsigned int> const& g_index);

    /** grid and block dimensions for CUDA calls */
    cuda::config const dim;

    /**
     * Allocate particle arrays in GPU memory.
     *
     * @param nparticle number of particles
     * @param nspecies number of particle species
     *
     * All particle arrays, except the masses, are initialised to zero.
     * The particle masses are initialised to unit mass.
     */
    particle(size_type nparticle, unsigned int nspecies);

    /**
     * Returns number of particles.
     *
     * Currently the number of particles is fixed at construction of
     * particle. This may change in the future, to allow for chemical
     * reactions that do not conserve the number of particles, or to
     * transfer particles between domains of different processors.
     */
    size_type nparticle() const
    {
        return nparticle_;
    }

    /**
     * Returns number of species.
     */
    unsigned int nspecies() const
    {
        return nspecies_;
    }

    /**
     * get named particle data with iterator
     *
     * @param name identifier of the particle data
     * @param first output iterator
     * @return output iterator
     *
     * throws an exception if the data does not exist or has an invalid type
     */
    template<typename T, typename iterator_type>
    iterator_type get_data(std::string const& name, iterator_type const& first) const
    {
        return particle_data::cast<T>(lookup_data_(name))->get_data(first);
    }
    /**
     * set named particle data with iterator
     *
     * @param name identifier of the particle data
     * @param first input iterator
     * @return input iterator
     *
     * throws an exception if the data does not exist or has an invalid type
     */
    template <typename T, typename iterator_type>
    iterator_type set_data(const std::string &name, iterator_type const& first)
    {
        return particle_data::cast<T>(lookup_data_(name))->set_data(first);
    }

    /**
     * Returns const reference to named particle data.
     *
     * @param name identifier of the particle data
     * @return const reference to the data
     *
     * throws an exception if the data does not exist or has an invalid type
     */
    template<typename T>
    cache<cuda::vector<T>> const &data(const std::string &name) const {
        return particle_data::cast_gpu<T>(lookup_data_(name))->data();
    }

    /**
     * Returns non-const reference to named particle data.
     *
     * @param name identifier of the particle data
     * @return non-const reference to the data
     *
     * throws an exception if the data does not exist or has an invalid type
     */
    template<typename T>
    cache<cuda::vector<T>>& mutable_data(const std::string &name) {
        return particle_data::cast_gpu<T>(lookup_data_(name))->mutable_data();
    }

    /**
     * Returns const reference to particle positions and species.
     */
    cache<position_array_type> const& position() const
    {
        return data<gpu_position_type>("g_position");
    }

    /**
     * Returns non-const reference to particle positions and species.
     */
    cache<position_array_type>& position()
    {
        return mutable_data<gpu_position_type>("g_position");
    }

    /**
     * Returns const reference to particle images.
     */
    cache<image_array_type> const& image() const
    {
        return data<gpu_image_type>("g_image");
    }

    /**
     * Returns non-const reference to particle images.
     */
    cache<image_array_type>& image()
    {
        return mutable_data<gpu_image_type>("g_image");
    }

    /**
     * Returns const reference to particle velocities and masses.
     */
    cache<velocity_array_type> const& velocity() const
    {
        return data<gpu_velocity_type>("g_velocity");
    }

    /**
     * Returns non-const reference to particle velocities and masses.
     */
    cache<velocity_array_type>& velocity()
    {
        return mutable_data<gpu_velocity_type>("g_velocity");
    }

    /**
     * Returns const reference to particle tags.
     */
    cache<tag_array_type> const& tag() const
    {
        return data<gpu_tag_type>("g_tag");
    }

    /**
     * Returns non-const reference to particle tags.
     */
    cache<tag_array_type>& tag()
    {
        return mutable_data<gpu_tag_type>("g_tag");
    }

    /**
     * Returns const reference to particle reverse tags.
     */
    cache<reverse_tag_array_type> const& reverse_tag() const
    {
        return data<gpu_reverse_tag_type>("g_reverse_tag");
    }

    /**
     * Returns non-const reference to particle reverse tags.
     */
    cache<reverse_tag_array_type>& reverse_tag()
    {
        return mutable_data<gpu_reverse_tag_type>("g_reverse_tag");
    }

    /**
     * Returns const reference to particle force.
     */
    cache<force_array_type> const& force()
    {
        return data<gpu_force_type>("g_force");
    }

    /**
     * Returns non-const reference to particle force.
     */
    cache<force_array_type>& mutable_force()
    {
        return mutable_data<gpu_force_type>("g_force");
    }

    /**
     * Returns const reference to potential energy of particles.
     */
    cache<en_pot_array_type> const& potential_energy()
    {
        return data<gpu_en_pot_type>("g_en_pot");
    }

    /**
     * Returns non-const reference to potential energy of particles.
     */
    cache<en_pot_array_type>& mutable_potential_energy()
    {
        return mutable_data<gpu_en_pot_type>("g_en_pot");
    }

    /**
     * Returns const reference to potential part of stress tensor.
     */
    cache<stress_pot_array_type> const& stress_pot()
    {
        return data<gpu_stress_pot_type>("g_stress_pot");
    }
    /**
     * Returns non-const reference to potential part of stress tensor.
     */
    cache<stress_pot_array_type>& mutable_stress_pot()
    {
        return mutable_data<gpu_stress_pot_type>("g_stress_pot");
    }

    /**
     * Enable computation of auxiliary variables.
     *
     * The flag is reset after the next trigger of on_force_().
     */
    void aux_enable();

    /**
     * Returns true if computation of auxiliary variables is enabled.
     */
    bool aux_enabled() const
    {
        return aux_enabled_;
    }

    /**
     * Returns true if the force has to be reset to zero prior to reading.
     */
    bool force_zero()
    {
        return force_zero_;
    }

    /**
     * Disable a reset of the force to zero upon reading.
     *
     * Must be called after computation of the first force contribution.
     */
    void force_zero_disable()
    {
        force_zero_ = false;
    }

    /**
     * Indicate that a force update (ie. triggering on_force_()) is required
     */
    void mark_force_dirty()
    {
        force_dirty_ = true;

    }

    /**
     * Indicate that an auxiliary update (ie. triggering on_force_() with
     * aux_enabled) is required
     */
    void mark_aux_dirty()
    {
        aux_dirty_ = true;
    }

    connection on_prepend_force(slot_function_type const& slot)
    {
        return on_prepend_force_.connect(slot);
    }

    connection on_force(slot_function_type const& slot)
    {
        return on_force_.connect(slot);
    }

    connection on_append_force(slot_function_type const& slot)
    {
        return on_append_force_.connect(slot);
    }
    luaponte::object get_lua (lua_State *L, std::string const& name) {
        return lookup_data_(name)->get_lua(L);
    }

    void set_lua (std::string const& name, luaponte::object object) {
        lookup_data_(name)->set_lua(object);
    }

    /**
     * Bind class to Lua.
     */
    static void luaopen(lua_State* L);

private:
    /** number of particles */
    size_type nparticle_;
    /** number of particle species */
    unsigned int nspecies_;

    /** map of the stored particle data */
    std::unordered_map<std::string, std::shared_ptr<particle_data>> data_;

    /** flag that the force has to be reset to zero prior to reading */
    bool force_zero_;
    /** flag that the force cache is dirty (not up to date) */
    bool force_dirty_;
    /** flag that the caches of the auxiliary variables are dirty (not up to date) */
    bool aux_dirty_;
    /** flag that the computation of auxiliary variables is requested */
    bool aux_enabled_;

    std::shared_ptr<particle_data> const& lookup_data_(std::string const& name) const {
        auto it = data_.find(name);
        if(it == data_.end()) {
            throw std::invalid_argument("particle data for \"" + name + "\" not registered");
        }
        return it->second;
    }

    /**
     * Update all forces and auxiliary variables if needed. The auxiliary
     * variables are guaranteed to be up-to-date upon return if with_aux was
     * set to true.
     *
     * Auxiliary variables are computed only if they are out of date
     * (aux_dirty_ == true) and if either with_aux or aux_enabled_ is true.
     *
     * Emit a warning if the force update would be necessary solely to compute
     * the auxiliary variables, which indicates a performance problem.
     */
    void update_force_(bool with_aux=false);

    typedef utility::profiler::accumulator_type accumulator_type;
    typedef utility::profiler::scoped_timer_type scoped_timer_type;

    struct runtime
    {
        accumulator_type rearrange;
    };

    /** profiling runtime accumulators */
    runtime runtime_;

    signal_type on_prepend_force_;
    signal_type on_force_;
    signal_type on_append_force_;
};

/**
 * Copy particle positions to given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
get_position(particle_type const& particle, iterator_type const& first)
{
    return particle.template get_data<typename particle_type::position_type>("position", first);
}

/**
 * Copy particle positions from given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
set_position(particle_type& particle, iterator_type const& first)
{
    return particle.template set_data<typename particle_type::position_type>("position", first);
}

/**
 * Copy particle species to given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
get_species(particle_type const& particle, iterator_type const& first)
{
    return particle.template get_data<typename particle_type::species_type>("species", first);
}

/**
 * Copy particle species from given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
set_species(particle_type& particle, iterator_type const& first)
{
    return particle.template set_data<typename particle_type::species_type>("species", first);
}

/**
 * Copy particle images to given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
get_image(particle_type const& particle, iterator_type const& first)
{
    return particle.template get_data<typename particle_type::image_type>("image", first);
}

/**
 * Copy particle images from given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
set_image(particle_type& particle, iterator_type const& first)
{
    return particle.template set_data<typename particle_type::image_type>("image", first);
}

/**
 * Copy particle velocities to given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
get_velocity(particle_type const& particle, iterator_type const& first)
{
    return particle.template get_data<typename particle_type::velocity_type>("velocity", first);
}

/**
 * Copy particle velocities from given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
set_velocity(particle_type& particle, iterator_type const& first)
{
    return particle.template set_data<typename particle_type::velocity_type>("velocity", first);
}

/**
 * Copy particle masses to given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
get_mass(particle_type const& particle, iterator_type const& first)
{
    return particle.template get_data<typename particle_type::mass_type>("mass", first);
}

/**
 * Copy particle masses from given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
set_mass(particle_type& particle, iterator_type const& first)
{
    return particle.template set_data<typename particle_type::mass_type>("mass", first);
}

/**
 * Copy particle tags to given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
get_tag(particle_type const& particle, iterator_type const& first)
{
    return particle.template get_data<typename particle_type::tag_type>("tag", first);
}

/**
 * Copy particle tags from given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
set_tag(particle_type& particle, iterator_type const& first)
{
    return particle.template set_data<typename particle_type::tag_type>("tag", first);
}

/**
 * Copy particle reverse tags to given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
get_reverse_tag(particle_type const& particle, iterator_type const& first)
{
    return particle.template get_data<typename particle_type::reverse_tag_type>("reverse_tag", first);
}

/**
 * Copy particle reverse tags from given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
set_reverse_tag(particle_type& particle, iterator_type const& first)
{
    return particle.template set_data<typename particle_type::reverse_tag_type>("reverse_tag", first);
}

/**
 * Copy net force per particle to given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
get_force(particle_type& particle, iterator_type const& first)
{
    return particle.template get_data<typename particle_type::force_type>("force", first);
}

/**
 * Copy potential energy per particle to given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
get_potential_energy(particle_type& particle, iterator_type const& first)
{
    return particle.template get_data<typename particle_type::en_pot_type>("en_pot", first);
}

/**
 * Copy potential part of stress tensor per particle to given array.
 */
template <typename particle_type, typename iterator_type>
inline iterator_type
get_stress_pot(particle_type& particle, iterator_type const& first)
{
    return particle.template get_data<typename particle_type::stress_pot_type>("stress_pot", first);
}

} // namespace gpu
} // namespace mdsim
} // namespace halmd

#endif /* ! HALMD_MDSIM_GPU_PARTICLE_HPP */
