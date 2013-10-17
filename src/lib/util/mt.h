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

#ifndef _LIB_UTIL_MT_H
#define _LIB_UTIL_MT_H 1

#define HEADER_SANS_WARNINGS <glib.h>
#include <sans_warnings.h>

#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 32
	#define mt_mutex_lock(_mt_mutex_ptr)    g_static_mutex_lock(_mt_mutex_ptr)
	#define mt_mutex_trylock(_mt_mutex_ptr) g_static_mutex_trylock(_mt_mutex_ptr)
	#define mt_mutex_unlock(_mt_mutex_ptr)  g_static_mutex_unlock(_mt_mutex_ptr)
#else
	#define mt_mutex_lock(_mt_mutex_ptr)    g_mutex_lock(_mt_mutex_ptr)
	#define mt_mutex_trylock(_mt_mutex_ptr) g_mutex_trylock(_mt_mutex_ptr)
	#define mt_mutex_unlock(_mt_mutex_ptr)  g_mutex_unlock(_mt_mutex_ptr)
#endif

#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 32
typedef GStaticMutex MtMutex;
#else
typedef GMutex MtMutex;
#endif

typedef GThread *MtThread;

void mt_mutex_init  (MtMutex *mutex);
void mt_mutex_clear (MtMutex *mutex);

MtThread mt_thread_create (void * (*f) (void *), void *data);
void mt_thread_join (MtThread thread);
void mt_thread_yield (void);

#endif
