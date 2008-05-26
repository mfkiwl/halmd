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
#include <string>
#include "exception.hpp"
#include "options.hpp"


namespace mdsim {

template <typename T>
class phase_space_point
{
public:
    typedef T vector_type;

public:
    /** periodically reduced coordinates of all particles in system */
    T r;
    /** periodically extended coordinates of all particles in system */
    T R;
    /** velocities of all particles in system */
    T v;
};


/**
 * trajectory file reader or writer
 */
template <unsigned dimension, typename T, bool writer = true>
class trajectory;


/**
 * trajectory file writer
 */
template <unsigned dimension, typename T>
class trajectory<dimension, T, true>
{
public:
    trajectory(options const& opts);
    /** write global simulation parameters to trajectory output file */
    void write_param(H5param const& param);
    void sample(phase_space_point<T> const& p, double const&, double const&);

private:
    H5::H5File file_;
    const uint64_t npart_;
    const uint64_t max_samples_;
    uint64_t samples_;
    boost::array<H5::DataSet, 2> dset_;
    H5::DataSpace ds_mem_;
    H5::DataSpace ds_file_;
};


/**
 * initialize HDF5 trajectory output file
 */
template <unsigned dimension, typename T>
trajectory<dimension, T>::trajectory(options const& opts) : npart_(opts.particles().value()), max_samples_(std::min(opts.steps().value(), opts.max_samples().value())), samples_(0)
{
#ifdef NDEBUG
    // turns off the automatic error printing from the HDF5 library
    H5::Exception::dontPrint();
#endif

    try {
	file_ = H5::H5File(opts.output_file_prefix().value() + ".trj", H5F_ACC_TRUNC);
    }
    catch (H5::FileIException const& e) {
	throw exception("failed to create HDF5 trajectory file");
    }

    hsize_t dim[3] = { max_samples_, npart_, dimension };
    ds_file_ = H5::DataSpace(3, dim);
    dset_[0] = file_.createDataSet("trajectory", H5::PredType::NATIVE_DOUBLE, ds_file_);
    dset_[1] = file_.createDataSet("velocity", H5::PredType::NATIVE_DOUBLE, ds_file_);

    hsize_t dim_mem[2] = { npart_, dimension };
    ds_mem_ = H5::DataSpace(2, dim_mem);
}

/**
 * write global simulation parameters to trajectory output file
 */
template <unsigned dimension, typename T>
void trajectory<dimension, T>::write_param(H5param const& param)
{
    param.write(file_.createGroup("/parameters"));
}

/**
 * write phase space sample to HDF5 dataset
 */
template <unsigned dimension, typename T>
void trajectory<dimension, T>::sample(phase_space_point<T> const& p, double const&, double const&)
{
    if (samples_ >= max_samples_)
	return;

    assert(p.r.size() == npart_);
    assert(p.v.size() == npart_);

    hsize_t count[3]  = { 1, npart_, 1 };
    hsize_t start[3]  = { samples_, 0, 0 };
    hsize_t stride[3] = { 1, 1, 1 };
    hsize_t block[3]  = { 1, 1, dimension };

    ds_file_.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);

    // coordinates
    dset_[0].write(p.r.data(), H5::PredType::NATIVE_DOUBLE, ds_mem_, ds_file_);
    // velocities
    dset_[1].write(p.v.data(), H5::PredType::NATIVE_DOUBLE, ds_mem_, ds_file_);

    samples_++;
}


/**
 * trajectory file reader
 */
template <unsigned dimension, typename T>
class trajectory<dimension, T, false>
{
public:
    trajectory();
    /** open HDF5 trajectory input file */
    void open(std::string const& filename);
    /** close HDF5 trajectory input file */
    void close();
    /** read global simulation parameters */
    void read(H5param& param);
    /** read phase space sample */
    void read(phase_space_point<std::vector<T> >& sample, int64_t index);

private:
    /** HDF5 trajectory input file */
    H5::H5File file;
};

template <unsigned dimension, typename T>
trajectory<dimension, T, false>::trajectory()
{
#ifdef NDEBUG
    // turns off the automatic error printing from the HDF5 library
    H5::Exception::dontPrint();
#endif
}

/**
 * open HDF5 trajectory input file
 */
template <unsigned dimension, typename T>
void trajectory<dimension, T, false>::open(std::string const& filename)
{
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
template <unsigned dimension, typename T>
void trajectory<dimension, T, false>::close()
{
    try {
	file.close();
    }
    catch (H5::Exception const& e) {
	throw exception("failed to close HDF5 trajectory input file");
    }
}

/**
 * read global simulation parameters
 */
template <unsigned dimension, typename T>
void trajectory<dimension, T, false>::read(H5param& param)
{
    // read global simulation parameters from file
    param.read(file.openGroup("/parameters"));
}

/**
 * read phase space sample
 */
template <unsigned dimension, typename T>
void trajectory<dimension, T, false>::read(phase_space_point<std::vector<T> >& sample, int64_t index)
{
    try {
	// open phase space coordinates datasets
	H5::DataSet dset_r(file.openDataSet("trajectory"));
	H5::DataSpace ds_r(dset_r.getSpace());
	H5::DataSet dset_v(file.openDataSet("velocity"));
	H5::DataSpace ds_v(dset_v.getSpace());

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

	// allocate memory for sample
	sample.r.resize(npart);
	sample.v.resize(npart);

	// read sample from dataset
	hsize_t dim_mem[2] = { npart, dimension };
	H5::DataSpace ds_mem(2, dim_mem);

	hsize_t count[3]  = { 1, npart, 1 };
	hsize_t start[3]  = { index, 0, 0 };
	hsize_t stride[3] = { 1, 1, 1 };
	hsize_t block[3]  = { 1, 1, dimension };

	// coordinates
	ds_r.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);
	dset_r.read(sample.r.data(), H5::PredType::NATIVE_DOUBLE, ds_mem, ds_r);
	// velocities
	ds_v.selectHyperslab(H5S_SELECT_SET, count, start, stride, block);
	dset_v.read(sample.v.data(), H5::PredType::NATIVE_DOUBLE, ds_mem, ds_v);
    }
    catch (H5::Exception const& e) {
	throw exception("failed to read from HDF5 trajectory input file");
    }
}

} // namespace mdsim

#endif /* ! MDSIM_TRAJECTORY_HPP */
