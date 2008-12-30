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

#include <algorithm>
#include <assert.h>
#include "exception.hpp"
#include "log.hpp"
#include "trajectory.hpp"

namespace mdsim {

/**
 * create HDF5 trajectory output file
 */
void trajectory<true>::open(std::string const& filename, unsigned int const& npart)
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
    // GZIP compression
    cparms.setDeflate(6);

    // create datasets
    hsize_t dim[3] = { 0, npart, dimension };
    hsize_t max_dim[3] = { H5S_UNLIMITED, npart, dimension };
    m_ds_file = H5::DataSpace(3, dim, max_dim);
    H5::Group root(m_file.createGroup("trajectory"));
    m_dataset[1] = root.createDataSet("R", H5_NATIVE_FLOAT_TYPE, m_ds_file, cparms);
    m_dataset[2] = root.createDataSet("v", H5_NATIVE_FLOAT_TYPE, m_ds_file, cparms);
#ifdef USE_CUDA
    m_dataset[3] = root.createDataSet("r", H5_NATIVE_FLOAT_TYPE, m_ds_file, cparms);
#endif

    hsize_t dim_mem[2] = { npart, dimension };
    m_ds_mem = H5::DataSpace(2, dim_mem);

    hsize_t chunk_scalar[1] = { 1 };
    cparms.setChunk(1, chunk_scalar);

    hsize_t dim_scalar[1] = { 0 };
    hsize_t max_dim_scalar[1] = { H5S_UNLIMITED };
    m_ds_scalar = H5::DataSpace(1, dim_scalar, max_dim_scalar);
    m_dataset[0] = root.createDataSet("t", H5_NATIVE_FLOAT_TYPE, m_ds_scalar, cparms);
}

/**
 * close HDF5 trajectory output file
 */
void trajectory<true>::close()
{
    try {
	m_file.close();
    }
    catch (H5::Exception const& e) {
	throw exception("failed to close HDF5 trajectory output file");
    }
}

/**
 * flush HDF5 output file to disk
 */
void trajectory<true>::flush()
{
    try {
	m_file.flush(H5F_SCOPE_GLOBAL);
    }
    catch (H5::Exception const& e) {
	throw exception("failed to flush HDF5 trajectory output file");
    }
}

/**
 * returns HDF5 parameter group
 */
H5param trajectory<true>::attrs()
{
    return H5param(m_file.openGroup("param"));
}

/**
 * write phase space sample
 */
void trajectory<true>::sample(trajectory_sample const& sample, float_type const& time)
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

#ifdef USE_CUDA
    assert(sample.r.size() == dim[1]);
#endif
    assert(sample.R.size() == dim[1]);
    assert(sample.v.size() == dim[1]);

#ifdef USE_CUDA
    // write periodically reduced particle coordinates
    m_dataset[3].extend(dim);
    m_dataset[3].write(sample.r.data(), H5_NATIVE_FLOAT_TYPE, m_ds_mem, m_ds_file);
#endif
    // write periodically extended particle coordinates
    m_dataset[1].extend(dim);
    m_dataset[1].write(sample.R.data(), H5_NATIVE_FLOAT_TYPE, m_ds_mem, m_ds_file);
    // write particle velocities
    m_dataset[2].extend(dim);
    m_dataset[2].write(sample.v.data(), H5_NATIVE_FLOAT_TYPE, m_ds_mem, m_ds_file);

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
    m_dataset[0].extend(dim_scalar);
    m_dataset[0].write(&time, H5_NATIVE_FLOAT_TYPE, H5S_SCALAR, m_ds_scalar);
}

/**
 * open HDF5 trajectory input file
 */
void trajectory<false>::open(std::string const& filename)
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
void trajectory<false>::close()
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
void trajectory<false>::read(std::vector<hvector>& r, std::vector<hvector>& v, int64_t index)
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

	r.resize(npart);
	v.resize(npart);

	// read sample from dataset
	hsize_t dim_mem[2] = { npart, dimension };
	H5::DataSpace ds_mem(2, dim_mem);

	hsize_t count[3]  = { 1, npart, 1 };
	hsize_t start[3]  = { index, 0, 0 };
	hsize_t stride[3] = { 1, 1, 1 };
	hsize_t block[3]  = { 1, 1, dimension };

	// read periodically reduced particle positions
	ds_r.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);
	dataset_r.read(r.data(), H5_NATIVE_FLOAT_TYPE, ds_mem, ds_r);
	// read particle velocities
	ds_v.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);
	dataset_v.read(v.data(), H5_NATIVE_FLOAT_TYPE, ds_mem, ds_v);
    }
    catch (H5::Exception const& e) {
	throw exception("failed to read from HDF5 trajectory input file");
    }
}

} // namespace mdsim
