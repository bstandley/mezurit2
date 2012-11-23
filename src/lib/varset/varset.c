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

#include "varset.h"

#include <stdlib.h>  // malloc(), realloc(), free()
#include <stdio.h>
#include <string.h>  // strtok()

#ifdef STATUS
#include <lib/status.h>
#else
#define f_print(_mode, ...) printf(__VA_ARGS__)
#endif
#include <lib/util/str.h>
#include <lib/util/num.h>

#define SPEEDYPROC_LINE_LENGTH 1024
#define SPEEDYPROC_VARSET_CHUNK_SIZE 1024

static void alloc_chunk       (VSP vs);
static void parse_point       (VSP vs, char *str);  // Note: str will be modified
static bool parse_heading     (VSP vs, char *str);  // Note: str will be modified
static void append_value      (VSP vs, long new_pt, int c, double value);
static long write_vset_actual (VSP vs, int *vci, int vci_len, FILE *file, bool headings);

double * vs_ref (VSP vs, long row, int col)
{
	return (row < vs->N_pt && col < vs->N_col) ? &vs->data[row * vs->N_col + col] : NULL;
}

VSP new_vset (int N_col)
{
	VSP vs = malloc(sizeof(VarSet));
	
	if (vs == NULL) f_print(F_ERROR, "Error: malloc() failed!\n");
	else
	{
		vs->N_col = N_col;
		vs->N_pt = 0;
		vs->chunks = 0;
		vs->data_saved = 0;
		vs->data = NULL;
		vs->name = cat1("none");

		if (N_col > 0)
		{
			vs->colname = malloc(sizeof(char*) * (size_t) N_col);
			vs->colunit = malloc(sizeof(char*) * (size_t) N_col);
			vs->colsave = malloc(sizeof(bool)  * (size_t) N_col);

			if (vs->colname == NULL || vs->colunit == NULL || vs->colsave == NULL)
			{
				f_print(F_ERROR, "Error: malloc() failed!\n");
				return NULL;
			}
			else
			{
				for (int i = 0; i < vs->N_col; i++)  // default column info
				{
					vs->colname[i] = supercat("col%d", i);
					vs->colunit[i] = cat1("");
					vs->colsave[i] = 1;
				}
			}
		}
		else
		{
			vs->colname = NULL;
			vs->colunit = NULL;
			vs->colsave = NULL;
		}
	}	

	return vs;
}

VSP clone_vset (VSP vs, long N_pt)
{
	VSP cp = new_vset(vs->N_col);

	set_name(cp, vs->name);
	for (int i = 0; i < vs->N_col; i++)
	{
		set_colname(cp, i, vs->colname[i]);
		set_colunit(cp, i, vs->colunit[i]);
		set_colsave(cp, i, vs->colsave[i]);
	}

	N_pt = (N_pt == -1) ? vs->N_pt : min_long(vs->N_pt, N_pt);
	for (long j = 0; j < N_pt; j++)
		append_point(cp, vs_ref(vs, j, 0));

	return cp;
}

void free_vset (VSP vs)
{
	if (vs == NULL) return;

	free(vs->name);

	for (int i = 0; i < vs->N_col; i++)
	{
		free(vs->colname[i]);
		free(vs->colunit[i]);
	}
	free(vs->colname);
	free(vs->colunit);
	free(vs->colsave);

	free(vs->data);
	free(vs);
}

bool set_name (VSP vs, const char *str)
{
	if (vs == NULL) return 0;

	replace(vs->name, cat1(str));
	return 1;
}

bool set_colname (VSP vs, int i, const char *name)
{
	if (vs == NULL || i < 0 || i >= vs->N_col) return 0;

	replace(vs->colname[i], cat1(name));
	return 1;
}

bool set_colunit (VSP vs, int i, const char *unit)
{
	if (vs == NULL || i < 0 || i >= vs->N_col) return 0;

	replace(vs->colunit[i], cat1(unit));
	return 1;
}

bool set_colsave (VSP vs, int i, bool save)
{
	if (vs == NULL || i < 0 || i >= vs->N_col) return 0;

	vs->colsave[i] = save;
	return 1;
}

char * get_name (VSP vs)
{
	return (vs == NULL) ? NULL : vs->name;
}

char * get_colname (VSP vs, int i)
{
	return (vs == NULL || i < 0 || i >= vs->N_col) ? NULL : vs->colname[i];
}

char * get_colunit (VSP vs, int i)
{
	return (vs == NULL || i < 0 || i >= vs->N_col) ? NULL : vs->colunit[i];
}

bool get_colsave (VSP vs, int i)
{
	return (vs == NULL || i < 0 || i >= vs->N_col) ? 1 : vs->colsave[i];
}

void alloc_chunk (VSP vs)
{
	vs->chunks++;
	vs->data = realloc(vs->data, sizeof(double) * (size_t) vs->N_col * SPEEDYPROC_VARSET_CHUNK_SIZE * (size_t) vs->chunks);

	if (vs->data == NULL) f_print(F_ERROR, "Error: malloc() failed!\n");
}

void append_value (VSP vs, long new_pt, int c, double value)
{
	if (new_pt)
	{
		if (vs->N_pt == SPEEDYPROC_VARSET_CHUNK_SIZE * vs->chunks) alloc_chunk(vs);  // might need to allocate another chunk
		vs->N_pt++;
	}

	vs_value(vs, vs->N_pt - 1, c) = value;  // write to the last point
}

void append_point (VSP vs, double *pt)
{
	for (int i = 0; i < vs->N_col; i++)
		append_value(vs, i == 0, i, pt[i]);  // pt better have the right length
}

void parse_point (VSP vs, char *str)  // note: str will be modified
{
	for (int i = 0; i < vs->N_col; i++)
		append_value(vs, i == 0, i, atof(strtok(i == 0 ? str : NULL, "\t")));
}	

bool parse_heading (VSP vs, char *str)
{
	double dummy;
	if (sscanf(str, "%lf", &dummy) != 0) return 0;  // not a heading if it is a number

	if (str[0] == '#') str++;

	char *tmps[vs->N_col];
	for (int i = 0; i < vs->N_col; i++)
		tmps[i] = cat1(strtok(i == 0 ? str : NULL, "\t\n"));

	for (int i = 0; i < vs->N_col; i++)
	{
		int l = -1, r = -1;  // l := rightmost ( or [, r := rightmost ) or ]
		for (int k = 0; k < str_length(tmps[i]); k++)
		{
			char c = tmps[i][k];
			if      (c == '(' || c == '[') l = k;
			else if (c == ')' || c == ']') r = k;
		}

		char *unit   _strfree_ = (l != -1 && l < r) ? str_sub(tmps[i], l + 1, r - 1) : cat1("");
		char *name_x _strfree_ = (l != -1 && l < r) ? str_sub(tmps[i], 0,     l - 1) : cat1(tmps[i]);
		char *name   _strfree_ = str_strip_end(name_x , " \r");

		set_colunit(vs, i, unit);
		set_colname(vs, i, name);
	}

	return 1;
}

VSP read_vset_range (const char *filename, long skip, long total)  // will always read at least one point
{
	FILE *file = fopen(filename, "r");
	if (file == NULL) 
	{
		f_print(F_ERROR, "Error: Unable to open \"%s\".\n", filename);
		return NULL;
	}

	char str[SPEEDYPROC_LINE_LENGTH];
	if (fgets(str, SPEEDYPROC_LINE_LENGTH, file) == NULL)
	{
		f_print(F_ERROR, "Error: Unable to read \"%s\".\n", filename);
		fclose(file);
		return NULL;
	}

	VSP vs = new_vset(count_char(str, '\t') + 1);
	set_name(vs, filename);
	bool read_new_line = parse_heading(vs, str);

	long j_done = max_long(skip, 0) + total;  // avoid skip < 0 from messing up total feature
	for (long j = 0; read_new_line ? fgets(str, SPEEDYPROC_LINE_LENGTH, file) != NULL : 1; j++)  // if read_new_line == 1, we already have a string to feed to parse_point left over from parse_heading
	{
		if (j >= skip)
		{
			if (total >= 0 && j >= j_done) break;  // total < 0 turns off the feature
			parse_point(vs, str);
		}

		read_new_line = 1;
	}

	fclose(file);
	f_print(F_RUN, "Read %ld points from \"%s\".\n", vs->N_pt, filename);
	
	return vs;
}

long write_vset_custom (VSP vs, int *vci, int vci_len, const char *filename, bool save_col_names, bool append, bool overwrite)
{
	// pass NULL for vci and 0 for vci_len to use automatic column setup based on colsave[]
	// will return -1 on error, 0+ for successful write

	FILE *file = fopen(filename, "r");

	bool exists = 0;
	if(file != NULL)
	{
		fclose(file);
		exists = 1;
	}

	bool proceed = 1;
	if (exists && !append)
	{
		if (overwrite) f_print(F_WARNING, "Warning: File exists, overwriting...\n");
		else
		{
			f_print(F_WARNING, "Warning, File exists, doing nothing...\n");
			proceed = 0;
		}
	}

	long N_written = -1;

	if (proceed)
	{
		file = fopen(filename, append ? "a" : "w");

		if (file == NULL) f_print(F_ERROR, "Error: Could not open \"%s\".\n", filename);
		else
		{
			int auto_vci[vs->N_col];
			int auto_vci_len = 0;

			if (vci == NULL || vci_len < 1)
			{
				for (int i = 0; i < vs->N_col; i++)
					if (vs->colsave[i])
					{
						auto_vci[auto_vci_len] = i;
						auto_vci_len++;
					}

				vci = auto_vci;           // overwrite with colinfo[].save based setup
				vci_len = auto_vci_len;
			}
			
			N_written = write_vset_actual(vs, vci, vci_len, file, save_col_names && !(exists && append));  // include exists in case we accidentially append to an empty file

			fclose(file);
			vs->data_saved = 1;
			f_print(F_RUN, "Wrote %ld points to \"%s\".\n", vs->N_pt, filename);
		}
	}

	return N_written;
}

long write_vset_actual (VSP vs, int *vci, int vci_len, FILE *file, bool headings)
{
	if (headings)
	{
		fprintf(file, "#");
		for (int i = 0; i < vci_len; i++)
			fprintf(file, "%s (%s)%c", vs->colname[vci[i]], vs->colunit[vci[i]], i == vci_len - 1 ? '\n' : '\t');
	}

	for (long j = 0; j < vs->N_pt; j++)
		for (int i = 0; i < vci_len; i++)
			fprintf(file, "%1.10f%c", vs_value(vs, j, vci[i]), i == vci_len - 1 ? '\n' : '\t');

	return vs->N_pt;
}
