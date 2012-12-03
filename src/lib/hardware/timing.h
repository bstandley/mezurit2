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

#ifndef _LIB_HARDWARE_TIMING_H
#define _LIB_HARDWARE_TIMING_H 1

#include <stdbool.h>
#define HEADER_SANS_WARNINGS <glib.h>
#include <sans_warnings.h>

#ifndef MINGW
typedef GTimer Timer;
#else
typedef __int64 Timer;
#endif

void timing_init  (void);
void timing_final (void);

void xleep (double dt);

bool wait_and_reset      (Timer *timer, double target);
bool overtime_then_reset (Timer *timer, double target);

#ifndef MINGW
#define timer_new()           g_timer_new()
#define timer_reset(_TIMER)   g_timer_reset(_TIMER)
#define timer_elapsed(_TIMER) g_timer_elapsed(_TIMER, NULL)
#define timer_destroy(_TIMER) g_timer_destroy(_TIMER)
#else
Timer * timer_new     (void);
void    timer_reset   (Timer *timer);
double  timer_elapsed (Timer *timer);
void    timer_destroy (Timer *timer);
#endif

#define _timerfree_ __attribute__((cleanup(_timer_clean)))
void _timer_clean (Timer **timer);

#endif
