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

#ifndef _LIB_UTIL_NUM_H
#define _LIB_UTIL_NUM_H 1

#define U_PI 3.14159265358979323846

#define array_set(_A, _I, _J, _x)          \
{                                          \
    for (size_t _i = 0; _i < _I; _i++)     \
        for (size_t _j = 0; _j < _J; _j++) \
            _A[_i][_j] = _x;               \
}

#define array_sum(_A, _I, _J, _P)          \
{                                          \
    *_P = 0;                               \
    for (size_t _i = 0; _i < _I; _i++)     \
        for (size_t _j = 0; _j < _J; _j++) \
            *_P += _A[_i][_j];             \
}

int window_int (int x, int x_min, int x_max);

int  min_int  (int a,  int b);
int  max_int  (int a,  int b);
long min_long (long a, long b);
long max_long (long a, long b);

double min_double (double a, double b);
double max_double (double a, double b);

int  floor_int  (double x);
int  ceil_int   (double x);
long floor_long (double x);
long ceil_long  (double x);

double window_double (double x, double x_min, double x_max);
double round_down_double (double x, double d);

#endif
