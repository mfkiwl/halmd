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

#include <algorithm>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <cmath>

#include <halmd/algorithm/gpu/radix_sort.hpp>
#include <halmd/io/logger.hpp>
#include <halmd/mdsim/gpu/sorts/hilbert.hpp>
#include <halmd/utility/lua/lua.hpp>
#include <halmd/utility/scoped_timer.hpp>
#include <halmd/utility/timer.hpp>

using namespace boost;
using namespace halmd::algorithm::gpu;
using namespace std;

namespace halmd
{
namespace mdsim { namespace gpu { namespace sorts
{

template <int dimension, typename float_type>
hilbert<dimension, float_type>::hilbert(
    shared_ptr<particle_type> particle
  , shared_ptr<box_type> box
)
  // dependency injection
  : particle(particle)
{
    // FIXME set Hilbert space-filling curve recursion depth
    float_type max_length = *max_element(box->length().begin(), box->length().end());
    depth_ = static_cast<unsigned int>(std::ceil(std::log(max_length) / M_LN2));
    // 32-bit integer for 2D/3D Hilbert code allows a maximum of 16/10 levels
    depth_ = std::min((dimension == 3) ? 10U : 16U, depth_);

    LOG("[hilbert] vertex recursion depth: " << depth_);

    try {
        cuda::copy(static_cast<vector_type>(box->length()), wrapper_type::kernel.box_length);
        cuda::copy(depth_, wrapper_type::kernel.depth);
    }
    catch (cuda::error const&) {
        LOG_ERROR("[hilbert] failed to copy parameters to device");
        throw;
    }
}

/**
 * register module runtime accumulators
 */
template <int dimension, typename float_type>
void hilbert<dimension, float_type>::register_runtimes(profiler_type& profiler)
{
    profiler.register_runtime(runtime_.map, "map", "map particles to Hilbert curve");
    profiler.register_runtime(runtime_.permutation, "permutation", "generate permutation");
    profiler.register_runtime(runtime_.order, "order", "order particles by permutation");
}

/**
 * Order particles after Hilbert space-filling curve
 */
template <int dimension, typename float_type>
void hilbert<dimension, float_type>::order()
{
    LOG_TRACE("[hilbert] order particles");
    {
        cuda::vector<unsigned int> g_map(particle->nbox);
        g_map.reserve(particle->g_r.capacity());
        this->map(g_map);
        this->permutation(g_map, particle->g_index);
    }
    this->order(particle->g_index);
    on_order_();
}

/**
 * map particles to Hilbert curve
 */
template <int dimension, typename float_type>
void hilbert<dimension, float_type>::map(cuda::vector<unsigned int>& g_map)
{
    scoped_timer<timer> timer_(runtime_.map);
    cuda::configure(particle->dim.grid, particle->dim.block);
    wrapper_type::kernel.map(particle->g_r, g_map);
}

/**
 * generate permutation
 */
template <int dimension, typename float_type>
void hilbert<dimension, float_type>::permutation(cuda::vector<unsigned int>& g_map, cuda::vector<unsigned int>& g_index)
{
    scoped_timer<timer> timer_(runtime_.permutation);
    cuda::configure(particle->dim.grid, particle->dim.block);
    wrapper_type::kernel.gen_index(g_index);
    radix_sort<unsigned int> sort(particle->nbox, particle->dim.threads_per_block());
    sort(g_map, g_index);
}

/**
 * order particles by permutation
 */
template <int dimension, typename float_type>
void hilbert<dimension, float_type>::order(cuda::vector<unsigned int> const& g_index)
{
    scoped_timer<timer> timer_(runtime_.order);

    cuda::vector<float4> g_r(particle->g_r.size());
    cuda::vector<gpu_vector_type> g_image(particle->g_image.size());
    cuda::vector<float4> g_v(particle->g_v.size());

    g_r.reserve(particle->g_r.capacity());
    g_image.reserve(particle->g_image.capacity());
    g_v.reserve(particle->g_v.capacity());

    cuda::configure(particle->dim.grid, particle->dim.block);
    wrapper_type::kernel.r.bind(particle->g_r);
    wrapper_type::kernel.image.bind(particle->g_image);
    wrapper_type::kernel.v.bind(particle->g_v);
    wrapper_type::kernel.order_particles(g_index, g_r, g_image, g_v);

    g_r.swap(particle->g_r);
    g_image.swap(particle->g_image);
    g_v.swap(particle->g_v);
}

template <int dimension, typename float_type>
static char const* module_name_wrapper(hilbert<dimension, float_type> const&)
{
    return hilbert<dimension, float_type>::module_name();
}

template <int dimension, typename float_type>
void hilbert<dimension, float_type>::luaopen(lua_State* L)
{
    using namespace luabind;
    static string class_name("hilbert_" + lexical_cast<string>(dimension) + "_");
    module(L, "libhalmd")
    [
        namespace_("mdsim")
        [
            namespace_("gpu")
            [
                namespace_("sorts")
                [
                    class_<hilbert, shared_ptr<_Base>, _Base>(class_name.c_str())
                        .def(constructor<
                            shared_ptr<particle_type>
                          , shared_ptr<box_type>
                        >())
                        .property("module_name", &module_name_wrapper<dimension, float_type>)
                        .def("register_runtimes", &hilbert::register_runtimes)
                ]
            ]
        ]
    ];
}

HALMD_LUA_API int luaopen_libhalmd_mdsim_gpu_sorts_hilbert(lua_State* L)
{
    hilbert<3, float>::luaopen(L);
    hilbert<2, float>::luaopen(L);
    return 0;
}

// explicit instantiation
template class hilbert<3, float>;
template class hilbert<2, float>;

}}} // namespace mdsim::gpu::sorts

} // namespace halmd
