/*
 * Copyright © 2012  Peter Colberg
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

#include <halmd/numeric/blas/fixed_vector.hpp>
#include <halmd/utility/gpu/thread.cuh>
#include <test/unit/numeric/blas/fixed_vector_cuda_vector_converter_kernel.hpp>

using namespace halmd;

template <typename U, typename V>
static __global__ void converter_one(float4 const* g_input, float4* g_output, U* g_u, V* g_v)
{
    U u;
    V v;
    tie(u, v) <<= g_input[GTID];
    g_output[GTID] <<= tie(u, v);
    g_u[GTID] = u;
    g_v[GTID] = v;
}

template <typename U, typename V>
static __global__ void converter_two(float4 const* g_input, float4* g_output, U* g_u, V* g_v)
{
    U u;
    V v;
    tie(u, v) <<= tie(g_input[GTID], g_input[GTID + GTDIM]);
    tie(g_output[GTID], g_output[GTID + GTDIM]) <<= tie(u, v);
    g_u[GTID] = u;
    g_v[GTID] = v;
}

template <typename U, typename V>
float_kernel<U, V> const float_kernel<U, V>::kernel = {
    &::converter_one
};

template <typename U, typename V>
double_kernel<U, V> const double_kernel<U, V>::kernel = {
    &::converter_two
};

template class float_kernel<fixed_vector<float, 3>, int>;
template class float_kernel<fixed_vector<float, 2>, int>;
template class float_kernel<fixed_vector<float, 3>, unsigned int>;
template class float_kernel<fixed_vector<float, 2>, unsigned int>;
template class float_kernel<fixed_vector<float, 3>, float>;
template class float_kernel<fixed_vector<float, 2>, float>;

template class double_kernel<fixed_vector<double, 3>, int>;
template class double_kernel<fixed_vector<double, 2>, int>;
template class double_kernel<fixed_vector<double, 3>, unsigned int>;
template class double_kernel<fixed_vector<double, 2>, unsigned int>;
template class double_kernel<fixed_vector<double, 3>, float>;
template class double_kernel<fixed_vector<double, 2>, float>;
template class double_kernel<fixed_vector<double, 3>, double>;
template class double_kernel<fixed_vector<double, 2>, double>;

template class double_kernel<fixed_vector<dsfloat, 3>, int>;
template class double_kernel<fixed_vector<dsfloat, 2>, int>;
template class double_kernel<fixed_vector<dsfloat, 3>, unsigned int>;
template class double_kernel<fixed_vector<dsfloat, 2>, unsigned int>;
template class double_kernel<fixed_vector<dsfloat, 3>, float>;
template class double_kernel<fixed_vector<dsfloat, 2>, float>;
template class double_kernel<fixed_vector<dsfloat, 3>, dsfloat>;
template class double_kernel<fixed_vector<dsfloat, 2>, dsfloat>;
