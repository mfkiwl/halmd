/* Block correlations
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

#ifndef MDSIM_CORRELATION_HPP
#define MDSIM_CORRELATION_HPP

#include "H5param.hpp"
#include "H5xx.hpp"
#include "exception.hpp"
#include "log.hpp"
#include "sample.hpp"
#include "tcf.hpp"
#include <H5Cpp.h>
#include <algorithm>
#include <boost/bind.hpp>
// requires boost 1.37.0 *or* patch from http://svn.boost.org/trac/boost/ticket/1852
#include <boost/circular_buffer.hpp>
#include <boost/foreach.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/multi_array.hpp>
#include <boost/variant.hpp>
#include <cmath>
#include <cmath>
#include <string>
#include <vector>

namespace mdsim {


/**
 * Block correlations
 */
template <typename float_type, int dimension>
class correlation
{
public:
    typedef vector<float_type, dimension> vector_type;
    /** phase space samples block type */
    typedef boost::circular_buffer<correlation_sample<vector_type> > block_type;
    /** correlation function variant type */
    typedef boost::make_variant_over<tcf_types>::type tcf_type;

public:
    /** initialise with all correlation function types */
    correlation();
    /** set total number of simulation steps */
    void steps(uint64_t const& value, float_type const& timestep);
    /** set total simulation time */
    void time(float_type const& value, float_type const& timestep);
    /** set sample rate for lowest block level */
    void sample_rate(unsigned int const& sample_rate);
    /** set block size */
    void block_size(unsigned int const& value);
    /** set maximum number of samples per block */
    void max_samples(uint64_t const& value);
    /** set q-vectors for spatial Fourier transformation */
    void q_values(unsigned int const& n, float_type const& box);

    /** returns total number of simulation steps */
    uint64_t const& steps() const { return m_steps; }
    /** returns total simulation time */
    float_type const& time() const { return m_time; }
    /** returns sample rate for lowest block level */
    unsigned int const& sample_rate() const { return m_sample_rate; }
    /** returns block size */
    unsigned int const& block_size() const { return m_block_size; }
    /** returns block shift */
    unsigned int const& block_shift() const { return m_block_shift; }
    /** returns block count */
    unsigned int const& block_count() const { return m_block_count; }
    /** returns maximum number of samples per block */
    uint64_t const& max_samples() const { return m_max_samples; }
    /** returns number of wave vectors */
    unsigned int q_values() { return m_q_vector.size(); }

    /** create HDF5 correlations output file */
    void open(std::string const& filename);
    /** close HDF5 file */
    void close();
    /** returns HDF5 parameter group */
    H5param attrs();
    /** write parameters to HDF5 parameter group */
    void attrs(H5::Group const& param) const;
    /** check if sample is acquired for given simulation step */
    bool sample(uint64_t const& step) const;
    /** sample time correlation functions */
    template <typename trajectory_sample>
    void sample(trajectory_sample const& sample, uint64_t const& step, bool& flush);
    /** write correlation function results to HDF5 file */
    void flush();

private:
    /** apply correlation functions to block samples */
    void autocorrelate_block(unsigned int n);

private:
    /** phase space sample blocks */
    std::vector<block_type> m_block;
    /** phase sample frequencies for block levels */
    std::vector<uint64_t> m_block_freq;
    /** correlation sample counts for block levels */
    std::vector<uint64_t> m_block_samples;

    /** simulation timestep */
    float_type m_timestep;
    /** sample rate for lowest block level */
    unsigned int m_sample_rate;
    /** total number of simulation steps */
    uint64_t m_steps;
    /** total simulation time */
    float_type m_time;
    /** block size */
    unsigned int m_block_size;
    /** block shift */
    unsigned int m_block_shift;
    /** block count */
    unsigned int m_block_count;
    /** block time intervals */
    boost::multi_array<float_type, 2> m_block_time;
    /** maximum number of correlation samples per block */
    uint64_t m_max_samples;
    /** q-values for spatial Fourier transformation */
    typename correlation_sample<vector_type>::q_value_vector m_q_vector;

    /** correlation functions and results */
    std::vector<tcf_type> m_tcf;
    /** HDF5 output file */
    H5::H5File m_file;
};

/**
 * initialise with all correlation function types
 */
template <typename float_type, int dimension>
correlation<float_type, dimension>::correlation()
{
    boost::mpl::for_each<tcf_types>(boost::bind(&std::vector<tcf_type>::push_back, boost::ref(m_tcf), _1));
}

/**
 * set total number of simulation steps
 */
template <typename float_type, int dimension>
void correlation<float_type, dimension>::steps(uint64_t const& value, float_type const& timestep)
{
    // set total number of simulation steps
    m_steps = value;
    LOG("total number of simulation steps: " << m_steps);
    // set simulation timestep
    m_timestep = timestep;
    // derive total simulation time
    m_time = value * m_timestep;
    LOG("total simulation time: " << m_time);
}

/**
 * set total simulation time
 */
template <typename float_type, int dimension>
void correlation<float_type, dimension>::time(float_type const& value, float_type const& timestep)
{
    // set total simulation time
    m_time = value;
    LOG("total simulation time: " << m_time);
    // set simulation timestep
    m_timestep = timestep;
    // derive total number of simulation steps
    m_steps = roundf(m_time / m_timestep);
    LOG("total number of simulation steps: " << m_steps);
}

/**
 * set sample rate for lowest block level
 */
template <typename float_type, int dimension>
void correlation<float_type, dimension>::sample_rate(unsigned int const& sample_rate)
{
    m_sample_rate = sample_rate;
    LOG("sample rate for lowest block level: " << m_sample_rate);
}

/**
 * set block size
 */
template <typename float_type, int dimension>
void correlation<float_type, dimension>::block_size(unsigned int const& value)
{
    // set block size
    m_block_size = value;
    LOG("block size: " << m_block_size);

    // derive block shift from block size
    m_block_shift = std::floor(std::sqrt(m_block_size));
    LOG("block shift: " << m_block_shift);
    if (m_block_shift < 2) {
	throw exception("computed block shift is less than 2, larger block size required");
    }

    // derive block count from block size and block shift
    m_block_count = 0;
    for (uint64_t n = m_block_size; n <= m_steps; n *= m_block_size) {
	m_block_count++;
	if ((n * m_block_shift) > m_steps) {
	    break;
	}
	m_block_count ++;
    }
    LOG("block count: " << m_block_count);
    if (!m_block_count) {
	throw exception("computed block count is zero, more simulations steps required");
    }

    // allocate phase space sample blocks
    try {
	m_block.resize(m_block_count, block_type(m_block_size));
    }
    catch (std::bad_alloc const& e) {
	throw exception("failed to allocate phase space sample blocks");
    }
    m_block_samples.resize(m_block_count, 0);

    // calculate phase sample frequencies
    for (uint64_t i = 0, n = m_sample_rate, m = n * m_block_shift; i < m_block_count; ++i) {
	if (i % 2) {
	    m_block_freq.push_back(m);
	    m *= m_block_size;
	}
	else {
	    m_block_freq.push_back(n);
	    n *= m_block_size;
	}
    }

    // compute block time intervals
    m_block_time.resize(boost::extents[m_block_count][m_block_size]);
    for (unsigned int i = 0; i < m_block_count; ++i) {
	for (unsigned int j = 0; j < m_block_size; ++j) {
	    m_block_time[i][j] = m_timestep * m_sample_rate * std::pow(m_block_size, i / 2) * j;
	    // shifted block
	    if (i % 2) {
		m_block_time[i][j] *= m_block_shift;
	    }
	}
    }
}

/**
 * set maximum number of samples per block
 */
template <typename float_type, int dimension>
void correlation<float_type, dimension>::max_samples(uint64_t const& value)
{
    m_max_samples = value;
    LOG("maximum number of samples per block: " << m_max_samples);
    if (m_max_samples < m_block_size) {
	throw exception("maximum number of samples must not be smaller than block size");
    }
}

/**
 * set q-vectors for spatial Fourier transformation
 */
template <typename float_type, int dimension>
void correlation<float_type, dimension>::q_values(unsigned int const& n, float_type const& box)
{
    // integer multiples of q-value corresponding to periodic box length
    for (unsigned int k = 1; k <= n; ++k) {
	m_q_vector.push_back(k * 2 * M_PI / box);
    }
    // FIXME dynamic structure factor at q ~ 2pi/sigma
    m_q_vector.push_back(floorf(box) * 2 * M_PI / box);
    m_q_vector.push_back(ceilf(box) * 2 * M_PI / box);

    // allocate correlation function results
    try {
	BOOST_FOREACH(tcf_type& tcf, m_tcf) {
	    boost::apply_visitor(tcf_allocate_results(m_block_count, m_block_size, m_q_vector.size()), tcf);
	}
    }
    catch (std::bad_alloc const& e) {
	throw exception("failed to allocate binary correlation functions results");
    }
}

/**
 * create HDF5 correlations output file
 */
template <typename float_type, int dimension>
void correlation<float_type, dimension>::open(std::string const& filename)
{
    LOG("write correlations to file: " << filename);
    try {
	// truncate existing file
	m_file = H5::H5File(filename, H5F_ACC_TRUNC);
    }
    catch (H5::FileIException const& e) {
	throw exception("failed to create HDF5 correlations output file");
    }
    // create parameter group
    m_file.createGroup("param");

    // create correlation function datasets
    try {
	BOOST_FOREACH(tcf_type& tcf, m_tcf) {
	    boost::apply_visitor(tcf_create_dataset(m_file), tcf);
	}
    }
    catch (H5::FileIException const& e) {
	throw exception("failed to create correlation function datasets");
    }
}

/**
 * close HDF5 file
 */
template <typename float_type, int dimension>
void correlation<float_type, dimension>::close()
{
    // compute higher block correlations for remaining samples
    for (unsigned int i = 2; i < m_block_count; ++i) {
	while (m_block[i].size() > 2) {
	    m_block[i].pop_front();
	    autocorrelate_block(i);
	}
    }

    // write correlation function results to HDF5 file
    flush();

    try {
	m_file.close();
    }
    catch (H5::Exception const& e) {
	throw exception("failed to close HDF5 correlations output file");
    }
}

/**
 * returns HDF5 parameter group
 */
template <typename float_type, int dimension>
H5param correlation<float_type, dimension>::attrs()
{
    return H5param(m_file.openGroup("param"));
}

/**
 * write parameters to HDF5 parameter group
 */
template <typename float_type, int dimension>
void correlation<float_type, dimension>::attrs(H5::Group const& param) const
{
    H5xx::group node(param.createGroup("correlation"));
    node["steps"] = m_steps;
    node["time"] = m_time;
    node["sample_rate"] = m_sample_rate;
    node["block_size"] = m_block_size;
    node["block_shift"] = m_block_shift;
    node["block_count"] = m_block_count;
    node["max_samples"] = m_max_samples;
    node["q_values"] = m_q_vector.size();
}

/**
 * check if sample is acquired for given simulation step
 */
template <typename float_type, int dimension>
bool correlation<float_type, dimension>::sample(uint64_t const& step) const
{
    for (unsigned int i = 0; i < m_block_count; ++i) {
	if (m_block_samples[i] >= m_max_samples)
	    continue;
	if (step % m_block_freq[i])
	    continue;

	return true;
    }
    return false;
}

/**
 * sample time correlation functions
 */
template <typename float_type, int dimension>
template <typename trajectory_sample>
void correlation<float_type, dimension>::sample(trajectory_sample const& sample, uint64_t const& step, bool& flush)
{
    correlation_sample<vector_type> p(sample.R, sample.v, m_q_vector);

    for (unsigned int i = 0; i < m_block_count; ++i) {
	if (m_block_samples[i] >= m_max_samples)
	    continue;
	if (step % m_block_freq[i])
	    continue;

	m_block[i].push_back(p);

	if (m_block[i].full()) {
	    autocorrelate_block(i);
	    if (i < 2) {
		// sample only full blocks in lowest levels to account for strong correlations
		m_block[i].clear();
	    }
	    m_block_samples[i]++;

	    if (m_max_samples == m_block_samples[i]) {
		LOG("finished sampling on block level " << i << " at step " << step);
		// trigger global write of partial results to disk
		flush = true;
	    }
	}
    }
}

/**
 * apply correlation functions to block samples
 */
template <typename float_type, int dimension>
void correlation<float_type, dimension>::autocorrelate_block(unsigned int n)
{
    BOOST_FOREACH(tcf_type& tcf, m_tcf) {
	boost::apply_visitor(tcf_correlate_block_gen(n, m_block[n], m_q_vector), tcf);
    }
}

/**
 * write correlation function results to HDF5 file
 */
template <typename float_type, int dimension>
void correlation<float_type, dimension>::flush()
{
    // find highest block with adequate number of samples
    unsigned int max_blocks = 0;
    for (; max_blocks < m_block_count; ++max_blocks) {
	if (m_block_samples[max_blocks] < 2)
	    break;
    }
    if (max_blocks < 1)
	return;

    try {
	BOOST_FOREACH(tcf_type& tcf, m_tcf) {
	    boost::apply_visitor(tcf_write_results<float_type>(m_block_time, m_q_vector, max_blocks), tcf);
	}
    }
    catch (H5::FileIException const& e) {
	throw exception("failed to write correlation function results");
    }

    try {
	// flush file contents to disk
	m_file.flush(H5F_SCOPE_GLOBAL);
    }
    catch (H5::FileIException const& e) {
	throw exception("failed to flush HDF5 correlations file");
    }
}

} // namespace mdsim

#endif /* ! MDSIM_CORRELATION_HPP */
