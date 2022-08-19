/*
 *  Copyright (C) 2012 Brian Standley <brian@brianstandley.com>
 *
 *  This file is part of Mezurit2, created by Brian Standley.
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

#include "mt.h"

void mt_mutex_init (MtMutex *mutex)
{
	g_mutex_init(mutex);
}

void mt_mutex_clear (MtMutex *mutex)
{
	g_mutex_clear(mutex);  // for completeness (not required because no MtMutexes in Mezurit2 are part of dynamically-allocated structures)
}

MtThread mt_thread_create (void * (*f) (void *), void *data)
{
	return g_thread_new("mt", f, data);
}

void mt_thread_join (MtThread thread)
{
	g_thread_join(thread);
}

void mt_thread_yield (void)
{
	g_thread_yield();
}
