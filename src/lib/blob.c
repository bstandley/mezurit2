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

#include "blob.h"

#include <lib/status.h>

void fill_blob_vl (Blob *blob, BlobCallback cb, int sig, va_list vl)
{
	blob->cb = cb;
	blob->sig = sig;

	int N_ptr = (sig & 0xF0) >> 4;
	int N_num = (sig & 0x0F);

	if (N_ptr > M2_BLOB_MAX_PTR) f_print(F_ERROR, "Error: A Blob cannot hold %d pointers!\n", N_ptr);
	if (N_num > M2_BLOB_MAX_NUM) f_print(F_ERROR, "Error: A Blob cannot hold %d numbers!\n",  N_num);

	for (int i = 0; i < N_ptr; i++) blob->ptr[i] = va_arg(vl, void *);
	for (int i = 0; i < N_num; i++) blob->num[i] = va_arg(vl, int);
}
