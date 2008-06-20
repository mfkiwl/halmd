/* Performance data
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

#ifndef MDSIM_PERF_HPP
#define MDSIM_PERF_HPP

#include <H5Cpp.h>
#include <iomanip>
#include <map>
#include <string>
#include "H5param.hpp"
#include "accumulator.hpp"
#include "log.hpp"
#include "statistics.hpp"


namespace mdsim
{

/**
 * performance class accumulators
 */
typedef std::map<std::string, std::map<std::string, accumulator<float> > > perf_type;

/**
 * performance data
 */
template <unsigned dimension, typename T, typename U>
class perf
{
public:
    perf();

    /** create HDF5 performance data output file */
    void open(std::string const& filename);
    /** dump global simulation parameters to HDF5 file */
    perf<dimension, T, U>& operator<<(H5param const& param);
    /** write performance data to HDF5 file */
    void write(perf_type const& times);
    /** close HDF5 file */
    void close();

private:
    /** HDF5 performance data output file */
    H5::H5File file_;
};


template <unsigned dimension, typename T, typename U>
perf<dimension, T, U>::perf()
{
#ifdef NDEBUG
    // turns off the automatic error printing from the HDF5 library
    H5::Exception::dontPrint();
#endif
}

/**
 * create HDF5 performance data output file
 */
template <unsigned dimension, typename T, typename U>
void perf<dimension, T, U>::open(std::string const& filename)
{
    LOG("write performance data to file: " << filename);
    try {
	// truncate existing file
	file_ = H5::H5File(filename, H5F_ACC_TRUNC);
    }
    catch (H5::FileIException const& e) {
	throw exception("failed to create performance data file");
    }
}

/**
 * dump global simulation parameters to HDF5 file
 */
template <unsigned dimension, typename T, typename U>
perf<dimension, T, U>& perf<dimension, T, U>::operator<<(H5param const& param)
{
    param.write(file_.createGroup("/parameters"));
    return *this;
}

/**
 * write performance data to HDF5 file
 */
template <unsigned dimension, typename T, typename U>
void perf<dimension, T, U>::write(perf_type const& times)
{
    hsize_t dim[1] = { 3 };
    H5::DataSpace ds(1, dim);
    H5::DataType tid(H5::PredType::NATIVE_FLOAT);

    try {
	H5::Group root(file_.createGroup("/times"));

	for (typename perf_type::const_iterator it = times.begin(); it != times.end(); ++it) {
	    // create group for performance class
	    H5::Group group(root.createGroup(it->first));

	    for (typename perf_type::mapped_type::const_iterator acc = it->second.begin(); acc != it->second.end(); ++acc) {
		// create dataset
		H5::DataSet dataset(group.createDataSet(acc->first, tid, ds));

		// write dataset
		float data[] = {
		    // average time in milliseconds
		    acc->second.mean(),
		    // standard deviation in milliseconds
		    acc->second.std(),
		    // number of calls
		    acc->second.count(),
		};
		dataset.write(data, tid, ds, ds);

		if (acc->second.count() > 1) {
		    LOG(it->first << "/" << acc->first << " average time: " << std::setprecision(4) << data[0] << " ms (" << std::setprecision(4) << data[1] << " ms)");
		}
		else {
		    LOG(it->first << "/" << acc->first << " time: " << std::setprecision(4) << data[0] << " ms");
		}
	    }
	}
    }
    catch (H5::FileIException const& e) {
	throw exception("failed to write performance data to HDF5 file");
    }
}

/**
 * close HDF5 file
 */
template <unsigned dimension, typename T, typename U>
void perf<dimension, T, U>::close()
{
    try {
	file_.close();
    }
    catch (H5::Exception const& e) {
	throw exception("failed to close performance data file");
    }
}

} // namespace mdsim

#endif /* ! MDSIM_PERF_HPP */
