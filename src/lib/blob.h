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

#ifndef _LIB_BLOB_H
#define _LIB_BLOB_H 1

#include <stdarg.h>

#include <config.h>

#define cast_3(_type) , _type, _type, _type
#define cast_2(_type) , _type, _type
#define cast_1(_type) , _type
#define cast_0(_type) 

#define call_3(_var) , blob->_var[0], blob->_var[1], blob->_var[2]
#define call_2(_var) , blob->_var[0], blob->_var[1]
#define call_1(_var) , blob->_var[0]
#define call_0(_var) 

#define blob_switch_inner(_ptr_cast, _ptr_call)                                                                       \
                                                                                                                      \
    switch (blob->sig & 0x0F)                                                                                         \
    {                                                                                                                 \
        case 0x02 : _ACTION ((_TYPE (*)(_CAST _ptr_cast cast_2(int))) blob->cb)(_CALL _ptr_call call_2(num));  break; \
        case 0x01 : _ACTION ((_TYPE (*)(_CAST _ptr_cast cast_1(int))) blob->cb)(_CALL _ptr_call call_1(num));  break; \
        case 0x00 : _ACTION ((_TYPE (*)(_CAST _ptr_cast cast_0(int))) blob->cb)(_CALL _ptr_call call_0(num));  break; \
        default   :                                                                                            break; \
    }                                                                                                                 \

#define blob_switch()                                                      \
                                                                           \
    switch (blob->sig & 0xF0)                                              \
    {                                                                      \
        case 0x30 : blob_switch_inner(cast_3(void*), call_3(ptr));  break; \
        case 0x20 : blob_switch_inner(cast_2(void*), call_2(ptr));  break; \
        case 0x10 : blob_switch_inner(cast_1(void*), call_1(ptr));  break; \
        case 0x00 : blob_switch_inner(cast_0(void*), call_0(ptr));  break; \
        default   :                                                 break; \
    }

#define fill_blob(_blob, _cb, _sig)      \
{                                        \
	va_list _vl;                         \
	va_start(_vl, _sig);                 \
	fill_blob_vl(_blob, _cb, _sig, _vl); \
	va_end(_vl);                         \
}

#define BLOB_CALLBACK(_F) ((BlobCallback) (_F))
typedef void (*BlobCallback) (void);

typedef struct
{
	BlobCallback cb;
	int sig;

	void *ptr [M2_BLOB_MAX_PTR];
	int   num [M2_BLOB_MAX_NUM];

} Blob;

void fill_blob_vl (Blob *blob, BlobCallback, int sig, va_list vl);

#endif
