/*
 *  Copyright (C) 2012 California Institute of Technology
 *
 *  This file is part of Mezurit2, written by Brian Standley <brian@brianstandley.com>.
 *
 *  Mezurit2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Foundation,
 *  either version 3 of the License, or (at your option) any later version.
 *
 *  Mezurit2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE. See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this
 *  program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "num.h"

#include <math.h>

int window_int (int x, int x_min, int x_max)
{
	x = (x < x_min) ? x_min : x;
	x = (x > x_max) ? x_max : x;
	return x;
}

int min_int (int a, int b)
{
	return (a < b) ? a : b;
}

int max_int (int a, int b)
{
	return (a > b) ? a : b;
}

long min_long (long a, long b)
{
	return (a < b) ? a : b;
}

long max_long (long a, long b)
{
	return (a > b) ? a : b;
}

double min_double (double a, double b)
{
	return (a < b) ? a : b;
}

double max_double (double a, double b)
{
	return (a > b) ? a : b;
}

int floor_int (double x)
{
	return (int) floor(x);
}

int ceil_int (double x)
{
	return (int) ceil(x);
}

long floor_long (double x)
{
	return (long) floor(x);
}

long ceil_long (double x)
{
	return (long) ceil(x);
}

double round_down_double (double x, double d)
{
	return (d > 0.0) ? floor(x / d) * d : x;
}
