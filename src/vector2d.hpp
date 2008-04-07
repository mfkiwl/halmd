/* 2-dimensional floating-point vector
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

#ifndef MDSIM_VECTOR2D_HPP
#define MDSIM_VECTOR2D_HPP

#include <cmath>


/**
 * 2-dimensional floating-point vector
 */
template <typename T>
class vector2d
{
public:
    T x, y;

public:
    vector2d()
    {
    }

    /**
     * initialization by vector
     */
    vector2d(const vector2d<T>& v) : x(v.x), y(v.y)
    {
    }

    /**
     * initialization by scalar
     */
    vector2d(const T& s) : x(s), y(s)
    {
    }

    /**
     * initialization by scalar components
     */
    vector2d(const T& x, const T& y) : x(x), y(y)
    {
    }

    /**
     * dimension of vector space
     */
    unsigned int dim() const
    {
	return 2;
    }

    /**
     * assignment by vector
     */
    vector2d<T>& operator=(const vector2d<T>& v)
    {
	x = v.x;
	y = v.y;
	return *this;
    }

    /**
     * assignment by scalar
     */
    vector2d<T>& operator=(const T& s)
    {
	x = s;
	y = s;
	return *this;
    }

    /**
     * assignment by componentwise vector addition
     */
    vector2d<T>& operator+=(const vector2d<T>& v)
    {
	x += v.x;
	y += v.y;
	return *this;
    }

    /**
     * assignment by componentwise vector subtraction
     */
    vector2d<T>& operator-=(const vector2d<T>& v)
    {
	x -= v.x;
	y -= v.y;
	return *this;
    }

    /**
     * assignment by scalar multiplication
     */
    vector2d<T>& operator*=(const T& s)
    {
	x *= s;
	y *= s;
	return *this;
    }

    /**
     * assignment by scalar division
     */
    vector2d<T>& operator/=(const T& s)
    {
	x /= s;
	y /= s;
	return *this;
    }

    /**
     * componentwise vector addition
     */
    vector2d<T> operator+(const vector2d<T>& v) const
    {
	return vector2d<T>(x + v.x, y + v.y);
    }

    /**
     * componentwise vector subtraction
     */
    vector2d<T> operator-(const vector2d<T>& v) const
    {
	return vector2d<T>(x - v.x, y - v.y);
    }

    /**
     * scalar product
     */
    T operator*(const vector2d<T>& v) const
    {
	return x * v.x + y * v.y;
    }

    /**
     * scalar multiplication
     */
    vector2d<T> operator*(const T& s) const
    {
	return vector2d<T>(x * s, y * s);
    }

    /**
     * scalar division
     */
    vector2d<T> operator/(const T& s) const
    {
	return vector2d<T>(x / s, y / s);
    }

    /**
     * componentwise round to nearest integer
     */
    friend vector2d<T> rint(const vector2d<T>& v)
    {
	return vector2d<T>(rint(v.x), rint(v.y));
    }

    /**
     * componentwise round to nearest integer, away from zero
     */
    friend vector2d<T> round(const vector2d<T>& v)
    {
	return vector2d<T>(round(v.x), round(v.y));
    }

    /**
     * componentwise round to nearest integer not greater than argument
     */
    friend vector2d<T> floor(const vector2d<T>& v)
    {
	return vector2d<T>(floor(v.x), floor(v.y));
    }

    /**
     * componentwise round to nearest integer not less argument
     */
    friend vector2d<T> ceil(const vector2d<T>& v)
    {
	return vector2d<T>(ceil(v.x), ceil(v.y));
    }
};

#endif /* ! MDSIM_VECTOR2D_HPP */
