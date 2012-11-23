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

#ifndef _LIB_PILE_H
#define _LIB_PILE_H 1

#include <stddef.h>

#define PILE_CALLBACK(f) ((PileCallback) (f))

typedef void ( *PileCallback) (void *);

typedef struct
{
	void **data;
	size_t size, occupied, cur;

} Pile;

void   pile_init  (Pile *pile);
void * pile_add   (Pile *pile, void *ptr);
void * pile_item  (Pile *pile, size_t i);
void * pile_first (Pile *pile);
void * pile_inc   (Pile *pile);
void   pile_gc    (Pile *pile, PileCallback cb);
void   pile_final (Pile *pile, PileCallback cb);

// Note: only _pile_first() and _pile_inc() modify pile.cur

#endif
