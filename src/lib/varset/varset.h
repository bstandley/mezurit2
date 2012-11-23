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

#ifndef _LIB_VARSET_VARSET_H
#define _LIB_VARSET_VARSET_H 1

#include <stdbool.h>

typedef struct
{
	char *name;
	long N_pt, chunks;
	bool data_saved;

	int N_col;
	char **colname, **colunit;
	bool *colsave;

	double *data;

} VarSet;

typedef VarSet (* VSP);

VSP  new_vset     (int N_col);
VSP  clone_vset   (VSP vs, long N_pt);  // pass N_pt = -1 to copy all points
void append_point (VSP vs, double *pt);
void free_vset    (VSP vs);

bool set_name    (VSP vs, const char *str);
bool set_colname (VSP vs, int i, const char *name);
bool set_colunit (VSP vs, int i, const char *unit);
bool set_colsave (VSP vs, int i, bool save);

char * get_name    (VSP vs);
char * get_colname (VSP vs, int i);
char * get_colunit (VSP vs, int i);
bool   get_colsave (VSP vs, int i);  // returns 1 if out-of-range

// Note: get_name(), get_colname(), get_colunit() return internal strings (do not free).

double * vs_ref (VSP vs, long row, int col);
#define vs_value(VS, ROW, COL) (*vs_ref(VS, ROW, COL))

VSP read_vset_range (const char *filename, long skip, long total);
long write_vset_custom (VSP vs, int *vci, int vci_len, const char *filename, bool save_col_names, bool append, bool overwrite);

#endif
