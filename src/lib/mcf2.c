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

#include "mcf2.h"

#include <stdlib.h>  // malloc(), free()
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include <lib/pile.h>
#include <lib/status.h>
#include <lib/util/str.h>

typedef struct
{
	char *name, *note;
	Blob blob;

} MSignal;

typedef struct
{
	void *ptr;
	char *name, *pattern;
	int flags;
	MValue default_value;
	Pile cb_signals;

} MVar;

static Pile mcf_pile;

static void free_mvar_cb (MVar *mvar);
static void free_msignal_cb (MSignal *ms);
static bool signal_search (MVar *mvar, const char *signal_name, MValue value);

void free_mvar_cb (MVar *mvar)
{
	free(mvar->name);
	free(mvar->pattern);
	if (mvar->flags & MCF_STRING) free(mvar->default_value.string);

	pile_final(&mvar->cb_signals, PILE_CALLBACK(free_msignal_cb));
}

void free_msignal_cb (MSignal *ms)
{
	free(ms->name);
	free(ms->note);
}

void mcf_init (void)
{
	f_start(F_INIT);

	pile_init(&mcf_pile);
}

void mcf_final (void)
{
	f_start(F_INIT);

	pile_final(&mcf_pile, PILE_CALLBACK(free_mvar_cb));
}

int mcf_register (void *ptr, const char *name, int flags, ...)
{
	MVar *mvar = pile_add(&mcf_pile, malloc(sizeof(MVar)));
	int var = (int) mcf_pile.occupied - 1;

	mvar->ptr = ptr;
	mvar->name = cat1(name);
	mvar->flags = flags;
	pile_init(&mvar->cb_signals);

	va_list vl;
	va_start(vl, flags);

	if (flags & MCF_BOOL)
	{
		mvar->pattern = cat2(name, "=%d");
		if (flags & MCF_DEFAULT) mvar->default_value.x_bool = (va_arg(vl, int) != 0);  // bool is promoted to int
	}
	else if (flags & MCF_INT)
	{
		mvar->pattern = cat2(name, "=%d");
		if (flags & MCF_DEFAULT) mvar->default_value.x_int = va_arg(vl, int);
	}
	else if (flags & MCF_DOUBLE)
	{
		mvar->pattern = cat2(name, "=%lf");
		if (flags & MCF_DEFAULT) mvar->default_value.x_double = va_arg(vl, double);
	}
	else if (flags & MCF_STRING)
	{
		mvar->pattern = cat2(name, "=");
		if (flags & MCF_DEFAULT) mvar->default_value.string = cat1(va_arg(vl, char *));
	}
	else mvar->pattern = cat1("");

	va_end(vl);

	return var;
}

void mcf_connect_with_note (int var, const char *signal_list, const char *signal_note, BlobCallback cb, int sig, ...)
{
	MVar *mvar = pile_item(&mcf_pile, (size_t) var);
	if (mvar == NULL) { f_print(F_ERROR, "Error: MVar id out of range.\n"); }
	else
	{
		char *str _strfree_ = cat1(signal_list);
		for (char *signal_name = strtok(str, ", "); signal_name != NULL; signal_name = strtok(NULL, ", "))
		{
			MSignal *ms = pile_add(&mvar->cb_signals, malloc(sizeof(MSignal)));
			ms->name = cat1(signal_name);
			ms->note = cat1(signal_note);
			fill_blob(&ms->blob, cb, sig);
		}
	}
}

void mcf_connect (int var, const char *signal_list, BlobCallback cb, int sig, ...)
{
	MVar *mvar = pile_item(&mcf_pile, (size_t) var);
	if (mvar == NULL) { f_print(F_ERROR, "Error: MVar id out of range.\n"); }
	else
	{
		char *str _strfree_ = cat1(signal_list);
		for (char *signal_name = strtok(str, ", "); signal_name != NULL; signal_name = strtok(NULL, ", "))
		{
			MSignal *ms = pile_add(&mvar->cb_signals, malloc(sizeof(MSignal)));
			ms->name = cat1(signal_name);
			ms->note = NULL;
			fill_blob(&ms->blob, cb, sig);
		}
	}
}

bool mcf_process_string (char *str, const char *signal_name)  // str will be modified
{
	MVar *mvar = pile_first(&mcf_pile);
	while (mvar != NULL)
	{
		MValue value;

		if (mvar->flags & MCF_STRING)
		{
			int offset = str_length(mvar->name) + 1;
			if (strncmp(str, mvar->pattern, (size_t) offset) == 0)
			{
				int len = str_length(str);
				if (str[len - 1] == '\n')
				{
					str[len - 1] = '\0';  // replace newline with null terminator
					if (len >= 2 && str[len - 2] == '\r') str[len - 2] = '\0';  // replace CR with null terminator
				}
				value.string = cat1(str + offset);
				signal_search(mvar, signal_name, value);
				free(value.string);
				break;
			}
		}
		else if (mvar->flags & MCF_BOOL)
		{
			if (sscanf(str, mvar->pattern, &value.x_int) == 1)
			{
				value.x_bool = (value.x_int != 0);
				signal_search(mvar, signal_name, value);
				break;
			}
		}
		else if (mvar->flags & MCF_INT)    { if (sscanf(str, mvar->pattern, &value.x_int)                  == 1) { signal_search(mvar, signal_name, value);  break; } }
		else if (mvar->flags & MCF_DOUBLE) { if (sscanf(str, mvar->pattern, &value.x_double)               == 1) { signal_search(mvar, signal_name, value);  break; } }
		else                               { if (strncmp(str, mvar->name, (size_t) str_length(mvar->name)) == 0) { signal_search(mvar, signal_name, value);  break; } }

		mvar = pile_inc(&mcf_pile);
	}

	if (mvar == NULL) f_print(F_WARNING, "Warning: Line not matched: %s", str);

	return (mvar != NULL);
}

bool signal_search (MVar *mvar, const char *signal_name, MValue value)
{
	MSignal *ms = pile_first(&mvar->cb_signals);
	while (ms != NULL)
	{
		if (str_equal(ms->name, signal_name))
		{
			Blob *blob = &ms->blob;
#define _ACTION 
#define _TYPE void
#define _CAST void*, const char*, MValue
#define _CALL mvar->ptr, ms->name, value
			blob_switch();
#undef _ACTION
#undef _TYPE
#undef _CAST
#undef _CALL
			if (ms->note != NULL) status_add(0, cat1(ms->note));
			return 1;
		}

		ms = pile_inc(&mvar->cb_signals);
	}

	return 0;
}

void mcf_load_defaults (const char *signal_name)
{
	f_start(F_UPDATE);

	MVar *mvar = pile_first(&mcf_pile);
	while (mvar != NULL)
	{
		if (mvar->flags & MCF_DEFAULT) signal_search(mvar, signal_name, mvar->default_value);

		mvar = pile_inc(&mcf_pile);
	}

	status_add(0, cat1("Loaded internal defaults.\n"));
}

int mcf_read_file (const char *filename, const char *signal_name)
{
	f_start(F_UPDATE);

	FILE *file = fopen(filename, "r");
	if (file == NULL)
	{
		status_add(0, supercat("Error: Could not read config file: %s.\n", filename));
		return -1;
	}

	int N = 0;
	char buffer [M2_MCF_LINE_LENGTH];
	while (fgets(buffer, M2_MCF_LINE_LENGTH, file) != NULL)
	{
		if (buffer[0] != '\n')
			N += mcf_process_string(buffer, signal_name) ? 1 : 0;
	}

	fclose(file);

	status_add(0, supercat("Read config: %s.\n", filename));
	return N;
}

char * mcf_lookup (const char *line)
{
	MVar *mvar = pile_first(&mcf_pile);
	while (mvar != NULL)
	{
		if (str_equal(mvar->name, line))
		{
			if (mvar->ptr != NULL)
			{
				if      (mvar->flags & MCF_BOOL)   return supercat("%d", (int) *((bool *)   mvar->ptr));
				else if (mvar->flags & MCF_INT)    return supercat("%d",       *((int *)    mvar->ptr));
				else if (mvar->flags & MCF_DOUBLE) return supercat("%f",       *((double *) mvar->ptr));
				else if (mvar->flags & MCF_STRING) return cat1(                *((char **)  mvar->ptr));
			}

			break;
		}

		mvar = pile_inc(&mcf_pile);
	}

	return NULL;
}

bool mcf_write_file (const char *filename)
{
	f_start(F_RUN);

	FILE *file = fopen(filename, "w");
	if (file == NULL)
	{
		status_add(0, supercat("Error: Could not write config file: %s.\n", filename));
		return 0;
	}

	MVar *mvar = pile_first(&mcf_pile);
	while (mvar != NULL)
	{
		if (mvar->flags & MCF_W)
		{
			if      (mvar->flags & MCF_BOOL)   fprintf(file, "%s=%d\n", mvar->name, (int) *((bool *)   mvar->ptr));
			else if (mvar->flags & MCF_INT)    fprintf(file, "%s=%d\n", mvar->name,       *((int *)    mvar->ptr));
			else if (mvar->flags & MCF_DOUBLE) fprintf(file, "%s=%f\n", mvar->name,       *((double *) mvar->ptr));
			else if (mvar->flags & MCF_STRING) fprintf(file, "%s=%s\n", mvar->name,       *((char **)  mvar->ptr));
			else                               fprintf(file, "\n%s\n",  mvar->name);
		}

		mvar = pile_inc(&mcf_pile);
	}
	
	fclose(file);

	status_add(0, supercat("Wrote config: %s.\n", filename));
	return 1;
}

void set_int_mcf (int *ptr, const char *signal_name, MValue value)
{
	f_start(F_MCF);
    if (ptr != NULL) *ptr = value.x_int;
}

void set_double_mcf (double *ptr, const char *signal_name, MValue value)
{
	f_start(F_MCF);
    if (ptr != NULL) *ptr = value.x_double;
}
