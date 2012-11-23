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

#include "timing.h"

#include <lib/status.h>
#include <lib/util/str.h>

#ifdef MINGW
#include <windows.h>

static double counter_frequency;
#endif

void timing_init (void)
{
#ifdef MINGW
	status_add(0, cat2("Setting system timer resolution to 1 ms ... ", timeBeginPeriod(1) == TIMERR_NOERROR ? "Success\n" : "Failed\n"));

	LARGE_INTEGER freq_LI;
	QueryPerformanceFrequency(&freq_LI);
	counter_frequency = (double) freq_LI.LowPart;
#endif
}

void timing_final (void)
{
#ifdef MINGW
	timeEndPeriod(1);
#endif
}

void xleep (double dt)
{
#ifndef MINGW
	g_usleep((gulong) (1e6 * dt));
#else
	Timer *timer = timer_new();

	for (double rt = dt; rt > 0; rt = dt - timer_elapsed(timer))
		Sleep(rt > 1e-3 ? (DWORD) (rt * 1e3) : 0);

	timer_destroy(timer);
#endif
}

bool wait_and_reset (Timer *timer, double target)
{
	double deficit = target - timer_elapsed(timer);
	if (deficit > 0) xleep(deficit);
	timer_reset(timer);
	return (deficit > 0);
}

bool overtime_then_reset (Timer *timer, double target)
{
	if (timer_elapsed(timer) > target)
	{
		timer_reset(timer);
		return 1;
	}
	else return 0;
}

void _timer_clean (Timer **timer)
{
	timer_destroy(*timer);
}

#ifdef MINGW
Timer * timer_new (void)
{
	LARGE_INTEGER count_LI;
	QueryPerformanceCounter(&count_LI);

	Timer *timer = malloc(sizeof(Timer));
	*timer = count_LI.QuadPart;
	return timer;
}

void timer_reset (Timer *timer)
{
	LARGE_INTEGER count_LI;
	QueryPerformanceCounter(&count_LI);

	*timer = count_LI.QuadPart;
}

double timer_elapsed (Timer *timer)
{
	LARGE_INTEGER count_LI;
	QueryPerformanceCounter(&count_LI);

	return (double) (count_LI.QuadPart - *timer) / counter_frequency;
}

void timer_destroy (Timer *timer)
{
	free(timer);
}
#endif
