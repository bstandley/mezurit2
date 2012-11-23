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

#ifndef _LIB_VARSET_SETVARSET_H
#define _LIB_VARSET_SETVARSET_H 1

#include <lib/varset/varset.h>

typedef struct
{
	int N_set, chunks;
	bool data_saved;
	VSP *data, last_vs;

} SetVarSet;

typedef SetVarSet (* SVSP);

SVSP new_svset   (void);
void free_svset  (SVSP svs);
SVSP append_vset (SVSP svs, VSP vs);

bool unsaved_data (SVSP svs);
long total_pts    (SVSP svs);

long write_svset_custom (SVSP svs, int *vci, int vci_len, const char *filename, bool save_col_names, bool always_append, bool overwrite);

#endif
