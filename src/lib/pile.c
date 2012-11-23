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

#include "pile.h"

#include <stdlib.h>  // free()

void pile_init (Pile *pile)
{
	pile->data = NULL;
	pile->size = 0;
	pile->occupied = 0;
	pile->cur = 0;
}

void * pile_add (Pile *pile, void *ptr)
{
	if (ptr != NULL)
	{
		if (pile->occupied == pile->size)
		{
			pile->size += 8;
			pile->data = realloc(pile->data, sizeof(void *) * pile->size);
		}

		pile->data[pile->occupied] = ptr;
		pile->occupied++;
	}

	return ptr;
}

void * pile_item (Pile *pile, size_t i)
{
	return (i < pile->occupied) ? pile->data[i] : NULL;
}

void * pile_first (Pile *pile)
{
	pile->cur = 0;
	return (pile->cur < pile->occupied) ? pile->data[pile->cur] : NULL;
}

void * pile_inc (Pile *pile)
{
	pile->cur++;
	return (pile->cur < pile->occupied) ? pile->data[pile->cur] : NULL;
}

void pile_gc (Pile *pile, PileCallback cb)
{
	for (size_t i = 0; i < pile->occupied; i++)
	{
		if (cb != NULL) cb(pile->data[i]);
		free(pile->data[i]);
	}

	pile->occupied = 0;
}

void pile_final (Pile *pile, PileCallback cb)
{
	pile_gc(pile, cb);
	free(pile->data);
}
