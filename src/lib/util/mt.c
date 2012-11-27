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
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 32
	g_static_mutex_init(mutex);
#else
	g_mutex_init(mutex);
#endif
}

void mt_mutex_clear (MtMutex *mutex)
{
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 32
	g_static_mutex_free(mutex);  // for completeness (not required because no MtMutexes in Mezurit2 are part of dynamically-allocated structures)
#else
	g_mutex_clear(mutex);
#endif
}
