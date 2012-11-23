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

#ifndef _LIB_MCF2_H
#define _LIB_MCF2_H 1

#include <stdbool.h>

#include <lib/blob.h>

enum
{
	MCF_BOOL    = 0x1 << 0,
	MCF_INT     = 0x1 << 1,
	MCF_DOUBLE  = 0x1 << 2,
	MCF_STRING  = 0x1 << 3,
	MCF_W       = 0x1 << 4,
	MCF_DEFAULT = 0x1 << 5
};

typedef union
{
	bool x_bool;
	int x_int;
	double x_double;
	char *string;

} MValue;

void mcf_init (void);
void mcf_final (void);

int mcf_register (void *ptr, const char *name, int flags, ...);
void mcf_connect_with_note (int var, const char *signal_list, const char *signal_note, BlobCallback cb, int sig, ...);
void mcf_connect           (int var, const char *signal_list,                          BlobCallback cb, int sig, ...);

bool mcf_process_string (char *str, const char *signal_name);  // str will be modified
void mcf_load_defaults (const char *signal_name);
int mcf_read_file (const char *filename, const char *signal_name);

char * mcf_lookup (const char *line);
bool mcf_write_file (const char *filename);

void set_int_mcf    (int    *ptr, const char *signal_name, MValue value);
void set_double_mcf (double *ptr, const char *signal_name, MValue value);

#endif
