#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright © 2011  Felix Höfling
#
# This file is part of HALMD.
#
# HALMD is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import argparse
import h5py
from numpy import *
from pylab import *

def main():
    # define and parse command line arguments
    parser = argparse.ArgumentParser(prog='plot_h5md.py')
    parser.add_argument('--range', type=int, nargs=2, help='select range of data points')
    parser.add_argument('--dump', metavar='FILENAME', help='dump plot data to filename')
    parser.add_argument('--no-plot', action='store_true', help='do not produce plots, but do the analysis')
    parser.add_argument('input', metavar='INPUT', help='H5MD input file with data for state variables')
    args = parser.parse_args()

    # evaluate option --range
    range = args.range or [0, -1]

    # open and read data file
    H5 = h5py.File(args.input, 'r')
    H5param = H5["param"]

    # print some details
    print 'Particles: {0:s}'.format(H5param["box"].attrs["particles"])
    print 'Force cutoff: {0:s}'.format(H5param[H5param.attrs["force"]].attrs["cutoff"])
    print 'Box size:',
    for x in H5param["box"].attrs["length"]:
        print ' {0:g}'.format(x),
    print
    print 'Density: {0:g}'.format(float(H5param["box"].attrs["density"]))

    # compute and print some averages
    # the simplest selection of a data set looks like this:
    #     temp, temp_err = compute_average(H5["TEMP"], 'Temperature')
    # full support for slicing (th second pair of brackets) requires conversion to a NumPy array before
    temp, temp_err = compute_average(array(H5["TEMP"])[range[0]:range[1]], 'Temperature')
    pressure, pressure_err = compute_average(array(H5["PRESS"])[range[0]:range[1]], 'Pressure')
    epot, epot_err = compute_average(array(H5["EPOT"])[range[0]:range[1]], 'Potential energy')

    # plot potential energy
    x = array(H5["TIME"])[range[0]:range[1]]
    y = array(H5["EPOT"])[range[0]:range[1]]
    if not args.no_plot:
        plot(x, y, '-b', label=args.input)

    # append plot data to file
    if args.dump:
        f = open(args.dump, 'a')
        print >>f, '# time   E_pot(t)'
        savetxt(f, array((x, y)).T)
        print >>f, '\n'
        f.close()

    # plot mean value for comparison
    if not args.no_plot:
        x = linspace(min(x), max(x), num=50)
        y = zeros_like(x) + epot
        plot(x, y, ':k')

    # add axes labels and finalise plot
    if not args.no_plot:
        axis('tight')
        xlabel(r'Time $t$')
        ylabel(r'Potential energy $E_\mathrm{pot}$')
        legend(loc='best', frameon=False)
        show()

    H5.close()

def compute_average(data, label, nblocks = 10):
    """ compute and print average of data set
        The error is estimated from grouping the data in shorter blocks """

    # group data in blocks, discard excess data
    data = reshape(data[:(nblocks * (data.shape[0] / nblocks))], (nblocks, -1))

    # compute mean and error
    avg = mean(data)
    err = std(mean(data, axis=1)) / sqrt(nblocks - 1)
    print '{0:s}: {1:.4f} ± {2:.2g}'.format(label, avg, err)

    # return as tuple
    return avg, err

if __name__ == '__main__':
    main()
