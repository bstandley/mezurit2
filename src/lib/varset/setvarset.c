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

#include "setvarset.h"

#include <stdlib.h>  // malloc(), free()

#ifdef STATUS
#include <lib/status.h>
#else
#define f_print(_mode, ...) printf(__VA_ARGS__)
#endif

#include <lib/util/str.h>

#define SPEEDYPROC_SETVARSET_CHUNK_SIZE 32

static void svset_alloc_chunk (SVSP svs);

SVSP new_svset (void)
{
	SVSP svs = malloc(sizeof(SetVarSet));

	if (svs != NULL)
	{
		svs->N_set = 0;
		svs->chunks = 0;
		svs->data_saved = 0;
		svs->data = NULL;
		svs->last_vs = NULL;
	}

	return svs;
}

void svset_alloc_chunk (SVSP svs)
{
	svs->chunks++;
	svs->data = realloc(svs->data, sizeof(VSP) * SPEEDYPROC_SETVARSET_CHUNK_SIZE * (size_t) svs->chunks);

	if (svs->data == NULL) f_print(F_ERROR, "Error: realloc() failed!\n");
}

SVSP append_vset (SVSP svs, VSP vs)
{
	if (svs->N_set == svs->chunks * SPEEDYPROC_SETVARSET_CHUNK_SIZE)  // might need to allocate another chunk
		svset_alloc_chunk(svs);

	svs->N_set++;
	svs->data[svs->N_set - 1] = svs->last_vs = vs;  // don't free vs later or else it will disappear here too

	return svs;
}

void free_svset (SVSP svs)
{
	if (svs != NULL)
	{
		if (svs->data != NULL)
			for(int j = 0; j < svs->N_set; j++)
				free_vset(svs->data[j]);

		free(svs->data);
		free(svs);
	}
}

bool unsaved_data (SVSP svs)
{
	if (svs != NULL)
	{
		for (int j = 0; j < svs->N_set; j++)
			if (svs->data[j]->N_pt > 0 && !svs->data[j]->data_saved)
				return 1;
	}

	return 0;
}

long total_pts (SVSP svs)
{
	long total = 0;
	for (int j = 0; j < svs->N_set; j++)
		total += svs->data[j]->N_pt;

	return total;
}

long write_svset_custom (SVSP svs, int *vci, int vci_len, const char *filename, bool save_col_names, bool always_append, bool overwrite)
{
	// will return <0 on error

	long N_written = 0;
	for (int j = 0; j < svs->N_set; j++)
		N_written += write_vset_custom(svs->data[j], vci, vci_len, filename, save_col_names, always_append || j != 0, overwrite);

#ifdef STATUS
	status_add(1, supercat("Wrote %ld total points to \"%s\".\n", N_written, filename));
#else
	printf("Wrote %ld total points to \"%s\".\n", N_written, filename);
#endif
	return N_written;
}
