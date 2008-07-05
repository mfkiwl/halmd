/* MD simulation trajectory writer
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

#ifndef MDSIM_TRAJECTORY_HPP
#define MDSIM_TRAJECTORY_HPP

#include <H5Cpp.h>
#include <algorithm>
#include <assert.h>
#include <boost/array.hpp>
#include <cuda_wrapper.hpp>
#include <string>
#include "H5param.hpp"
#include "exception.hpp"
#include "log.hpp"


namespace mdsim {

template <unsigned dimension, typename T, typename U, bool writer = true>
class trajectory;

/**
 * trajectory file writer
 */
template <unsigned dimension, typename T, typename U>
class trajectory<dimension, T, U, true>
{
public:
    /** vector sample vector in page-locked host memory */
    typedef cuda::host::vector<U> vector_type;

public:
    /** create HDF5 trajectory output file */
    void open(std::string const& filename, unsigned int const& npart);
    /** close HDF5 trajectory output file */
    void close();
    /** returns HDF5 parameter group */
    H5param attrs();
    /** write phase space sample */
    void sample(vector_type const& r, vector_type const& R, vector_type const& v, float const& time);

private:
    /** HDF5 trajectory output file */
    H5::H5File m_file;
    /** trajectory datasets for particle coordinates and velocities */
    boost::array<H5::DataSet, 4> m_dataset;
    /** memory dataspace for a single coordinates or velocities sample */
    H5::DataSpace m_ds_mem;
    /** file dataspace for a single coordinates or velocities sample */
    H5::DataSpace m_ds_file;
    /** file dataspace for simulation time */
    H5::DataSpace m_ds_scalar;
};

/**
 * create HDF5 trajectory output file
 */
template <unsigned dimension, typename T, typename U>
void trajectory<dimension, T, U, true>::open(std::string const& filename, unsigned int const& npart)
{
    // create trajectory output file
    LOG("write trajectories to file: " << filename);
    try {
	// truncate existing file
	m_file = H5::H5File(filename, H5F_ACC_TRUNC);
    }
    catch (H5::FileIException const& e) {
	throw exception("failed to create trajectory output file");
    }
    // create parameter group
    m_file.createGroup("param");

    // modify dataset creation properties to enable chunking
    H5::DSetCreatPropList cparms;
    hsize_t chunk_dim[3] = { 1, npart, dimension };
    cparms.setChunk(3, chunk_dim);

    // create datasets
    hsize_t dim[3] = { 0, npart, dimension };
    hsize_t max_dim[3] = { H5S_UNLIMITED, npart, dimension };
    m_ds_file = H5::DataSpace(3, dim, max_dim);
    H5::Group root(m_file.createGroup("trajectory"));
    m_dataset[0] = root.createDataSet("r", H5::PredType::NATIVE_FLOAT, m_ds_file, cparms);
    m_dataset[1] = root.createDataSet("R", H5::PredType::NATIVE_FLOAT, m_ds_file, cparms);
    m_dataset[2] = root.createDataSet("v", H5::PredType::NATIVE_FLOAT, m_ds_file, cparms);

    hsize_t dim_mem[2] = { npart, sizeof(U) / sizeof(float) };
    m_ds_mem = H5::DataSpace(2, dim_mem);

    hsize_t count_mem[2]  = { npart, 1 };
    hsize_t start_mem[2]  = { 0, 0 };
    hsize_t stride_mem[2] = { 1, 1 };
    hsize_t block_mem[2]  = { 1, dimension };
    m_ds_mem.selectHyperslab(H5S_SELECT_SET, count_mem, start_mem, stride_mem, block_mem);

    hsize_t chunk_scalar[1] = { 1 };
    cparms.setChunk(1, chunk_scalar);

    hsize_t dim_scalar[1] = { 0 };
    hsize_t max_dim_scalar[1] = { H5S_UNLIMITED };
    m_ds_scalar = H5::DataSpace(1, dim_scalar, max_dim_scalar);
    m_dataset[3] = root.createDataSet("t", H5::PredType::NATIVE_FLOAT, m_ds_scalar, cparms);
}

/**
 * close HDF5 trajectory output file
 */
template <unsigned dimension, typename T, typename U>
void trajectory<dimension, T, U, true>::close()
{
    try {
	m_file.close();
    }
    catch (H5::Exception const& e) {
	throw exception("failed to close HDF5 trajectory input file");
    }
}

/**
 * returns HDF5 parameter group
 */
template <unsigned dimension, typename T, typename U>
H5param trajectory<dimension, T, U, true>::attrs()
{
    return H5param(m_file.openGroup("param"));
}

/**
 * write phase space sample
 */
template <unsigned dimension, typename T, typename U>
void trajectory<dimension, T, U, true>::sample(vector_type const& r, vector_type const& R, vector_type const& v, float const& time)
{
    hsize_t dim[3];
    m_ds_file.getSimpleExtentDims(dim);

    hsize_t count[3]  = { 1, 1, 1 };
    hsize_t start[3]  = { dim[0], 0, 0 };
    hsize_t stride[3] = { 1, 1, 1 };
    hsize_t block[3]  = { 1, dim[1], dim[2] };

    dim[0]++;
    m_ds_file.setExtentSimple(3, dim);
    m_ds_file.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);

    assert(r.size() == dim[1]);
    assert(R.size() == dim[1]);
    assert(v.size() == dim[1]);

    // write periodically reduced particle coordinates
    m_dataset[0].extend(dim);
    m_dataset[0].write(r.data(), H5::PredType::NATIVE_FLOAT, m_ds_mem, m_ds_file);
    // write periodically extended particle coordinates
    m_dataset[1].extend(dim);
    m_dataset[1].write(R.data(), H5::PredType::NATIVE_FLOAT, m_ds_mem, m_ds_file);
    // write particle velocities
    m_dataset[2].extend(dim);
    m_dataset[2].write(v.data(), H5::PredType::NATIVE_FLOAT, m_ds_mem, m_ds_file);

    hsize_t dim_scalar[1];
    m_ds_scalar.getSimpleExtentDims(dim_scalar);

    hsize_t count_scalar[1]  = { 1 };
    hsize_t start_scalar[1]  = { dim_scalar[0] };
    hsize_t stride_scalar[1] = { 1 };
    hsize_t block_scalar[1]  = { 1 };

    dim_scalar[0]++;
    m_ds_scalar.setExtentSimple(1, dim_scalar);
    m_ds_scalar.selectHyperslab(H5S_SELECT_SET, count_scalar, start_scalar, stride_scalar, block_scalar);

    // write simulation time
    m_dataset[3].extend(dim_scalar);
    m_dataset[3].write(&time, H5::PredType::NATIVE_FLOAT, H5S_SCALAR, m_ds_scalar);
}

/**
 * trajectory file reader
 */
template <unsigned dimension, typename T, typename U>
class trajectory<dimension, T, U, false>
{
public:
    /** sample vector in page-locked host memory */
    typedef cuda::host::vector<U> vector_type;

public:
    /** open HDF5 trajectory input file */
    void open(std::string const& filename);
    /** close HDF5 trajectory input file */
    void close();
    /** read phase space sample */
    void read(vector_type& r, vector_type& v, int64_t index);

private:
    /** HDF5 trajectory input file */
    H5::H5File file;
};

/**
 * open HDF5 trajectory input file
 */
template <unsigned dimension, typename T, typename U>
void trajectory<dimension, T, U, false>::open(std::string const& filename)
{
    LOG("read trajectory file: " << filename);
    try {
	file = H5::H5File(filename, H5F_ACC_RDONLY);
    }
    catch (H5::Exception const& e) {
	throw exception("failed to open HDF5 trajectory input file");
    }
}

/**
 * close HDF5 trajectory input file
 */
template <unsigned dimension, typename T, typename U>
void trajectory<dimension, T, U, false>::close()
{
    try {
	file.close();
    }
    catch (H5::Exception const& e) {
	throw exception("failed to close HDF5 trajectory input file");
    }
}

/**
 * read phase space sample
 */
template <unsigned dimension, typename T, typename U>
void trajectory<dimension, T, U, false>::read(vector_type& r, vector_type& v, int64_t index)
{
    try {
	// open phase space coordinates datasets
	H5::Group root(file.openGroup("trajectory"));
	H5::DataSet dataset_r(root.openDataSet("r"));
	H5::DataSpace ds_r(dataset_r.getSpace());
	H5::DataSet dataset_v(root.openDataSet("v"));
	H5::DataSpace ds_v(dataset_v.getSpace());

	// validate dataspace extents
	if (!ds_r.isSimple()) {
	    throw exception("trajectory dataspace is not a simple dataspace");
	}
	if (!ds_v.isSimple()) {
	    throw exception("velocity dataspace is not a simple dataspace");
	}
	if (ds_r.getSimpleExtentNdims() != 3) {
	    throw exception("trajectory dataspace has invalid dimensionality");
	}
	if (ds_v.getSimpleExtentNdims() != 3) {
	    throw exception("velocity dataspace has invalid dimensionality");
	}

	// retrieve dataspace dimensions
	hsize_t dim_r[3];
	ds_r.getSimpleExtentDims(dim_r);
	hsize_t dim_v[3];
	ds_v.getSimpleExtentDims(dim_v);

	if (!std::equal(dim_r, dim_r + 3, dim_v)) {
	    throw exception("trajectory and velocity dataspace dimensions differ");
	}

	// number of samples
	int64_t len = dim_r[0];
	// number of particles
	unsigned int npart = dim_r[1];

	// validate dataspace dimensions
	if (len < 1) {
	    throw exception("trajectory input file has invalid number of samples");
	}
	if (npart < 1) {
	    throw exception("trajectory input file has invalid number of particles");
	}
	if (dimension != dim_r[2]) {
	    throw exception("trajectory input file has invalid coordinate dimension");
	}

	// check if sample number is within bounds
	if ((index >= len) || ((-index) > len)) {
	    throw exception("trajectory input sample number out of bounds");
	}
	index = (index < 0) ? (index + len) : index;

	LOG("resuming from trajectory sample at offset: " << index);

	assert(r.size() == npart);
	assert(v.size() == npart);

	// read sample from dataset
	hsize_t dim_mem[2] = { npart, sizeof(U) / sizeof(float) };
	H5::DataSpace ds_mem(2, dim_mem);

	hsize_t count_mem[2]  = { npart, 1 };
	hsize_t start_mem[2]  = { 0, 0 };
	hsize_t stride_mem[2] = { 1, 1 };
	hsize_t block_mem[2]  = { 1, dimension };
	ds_mem.selectHyperslab(H5S_SELECT_SET, count_mem, start_mem, stride_mem, block_mem);

	hsize_t count[3]  = { 1, npart, 1 };
	hsize_t start[3]  = { index, 0, 0 };
	hsize_t stride[3] = { 1, 1, 1 };
	hsize_t block[3]  = { 1, 1, dimension };

	// read periodically reduced particle positions
	ds_r.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);
	dataset_r.read(r.data(), H5::PredType::NATIVE_FLOAT, ds_mem, ds_r);
	// read particle velocities
	ds_v.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);
	dataset_v.read(v.data(), H5::PredType::NATIVE_FLOAT, ds_mem, ds_v);
    }
    catch (H5::Exception const& e) {
	throw exception("failed to read from HDF5 trajectory input file");
    }
}

} // namespace mdsim

#endif /* ! MDSIM_TRAJECTORY_HPP */
