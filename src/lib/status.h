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

#ifndef _LIB_STATUS_H
#define _LIB_STATUS_H 1

#include <lib/hardware/timing.h>

enum
{
	F_NONE     = 0x0,
	F_ERROR    = 0x1 << 0,
	F_WARNING  = 0x1 << 1,
	F_INIT     = 0x1 << 2,
	F_UPDATE   = 0x1 << 3,
	F_RUN      = 0x1 << 4,
	F_CALLBACK = 0x1 << 5,
	F_MCF      = 0x1 << 6,
	F_CONTROL  = 0x1 << 7,
	F_VERBOSE  = 0x1 << 8,
	F_BENCH    = 0x1 << 9,
	F_PROFILE  = 0x1 << 10
};

#define f_start(_mode)                                       \
                                                             \
    FContext _f_context __attribute__((cleanup(_f_return))); \
    _f_trace(&_f_context, _mode, __func__);

#define f_verify(_test, _msg, _action)           \
                                                 \
    if (!(_test))                                \
    {                                            \
        if (_msg != NULL)                        \
            _f_print(F_WARNING, __func__, _msg); \
                                                 \
        _action;                                 \
    }

#define atg(_ptr) _f_add (&_f_context, _ptr)
#define f_gc()    _f_gc  (&_f_context)

#define f_print(_mode, ...) _f_print(_mode, __func__, __VA_ARGS__)

typedef struct
{
	void *pile;
	Timer *timer;

} FContext;

void f_init (const int mode);

void   _f_trace  (FContext *f_context, const int mode, const char *func);
void   _f_return (FContext *f_context);
void * _f_add    (FContext *f_context, void *ptr);
void   _f_gc     (FContext *f_context);
void   _f_print  (const int mode, const char *func, const char *fmt, ...);

void   status_init (void);
void   status_final (void);
void   status_add (bool timestamp, char *msg_in);  // thread-safe
bool   status_regenerate (void);                   // GUI thread only
char * status_get_text (void);                     // GUI thread only
char * status_get_last_line (void);                // GUI thread only

#endif
