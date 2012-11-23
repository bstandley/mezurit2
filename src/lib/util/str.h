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

#ifndef _LIB_UTIL_STR_H
#define _LIB_UTIL_STR_H 1

#include <stdbool.h>

#define _quote(_tok) #_tok
#define quote(_tok) _quote(_tok)

// (extra step in replace() allows _ptr to be used to compute _value)
#define replace(_ptr, _value) \
{                             \
    void *_tmp = (_ptr);      \
    _ptr = (_value);          \
    _free_ptr(_tmp);          \
}
void _free_ptr (void *ptr);

#define _strfree_ __attribute__((cleanup(_str_clean)))
void _str_clean (char **str);

bool str_equal (const char *a, const char *b);
int str_length (const char *str);
int count_char (const char *str, char c);
int str_to_int (const char *str);
bool scan_hex (const char *str, int *x_ptr);

char * new_str (int len);
char * cat1 (const char *a);
char * cat2 (const char *a, const char *b);
char * cat3 (const char *a, const char *b, const char *c);
char * supercat (const char *fmt, ...);

char * str_sub (const char *str, int l, int r);
char * str_strip_end (const char *str, const char *unwanted);
char * join_lines (char **lines, const char *sep, int N);

#endif
