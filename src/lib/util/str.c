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

#include "str.h"

#include <stdlib.h>  // malloc(), free()
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef STATUS
#include <lib/status.h>
#else
#define f_print(_mode, ...) printf(__VA_ARGS__)
#endif

void _free_ptr (void *ptr)
{
	free(ptr);
}

void _str_clean (char **str)
{
	free(*str);
}

char * new_str (int len)
{
	char *str = malloc(sizeof(char) * (size_t) (len + 1));  // +1 for NULL termination
	str[0] = '\0';

	return str;
}

char * cat1 (const char *a)
{
	char *str = new_str(str_length(a));
	strcpy(str, a != NULL ? a : "");

	return str;
}

char * cat2 (const char *a, const char *b)
{
	char *str = new_str(str_length(a) + str_length(b));
	sprintf(str, "%s%s", a != NULL ? a : "", b != NULL ? b : "");

	return str;
}

char * cat3 (const char *a, const char *b, const char *c)
{
	char *str = new_str(str_length(a) + str_length(b) + str_length(c));
	sprintf(str, "%s%s%s", a != NULL ? a : "", b != NULL ? b : "", c != NULL ? c : "");

	return str;
}

char * join_lines (char **lines, const char *sep, int N)
{
	int len = str_length(sep) * N;  // may be slightly over when N > 1
	for (int i = 0; i < N; i++)
		len += str_length(lines[i]);

	char *str = new_str(len);
	int spot = 0;
	for (int i = 0; i < N; i++)
		if (lines[i] != NULL) spot += (spot == 0) ? sprintf(&str[spot], "%s", lines[i]) :
		                                            sprintf(&str[spot], "%s%s", sep, lines[i]);
	return str;
}

char * supercat (const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);

	char str0[64];
	int len = vsnprintf(str0, 64, fmt, vl);
	va_end(vl);

	char *str1 = new_str(len);
	if (len > 63) 
	{
		va_start(vl, fmt);
		vsprintf(str1, fmt, vl);
		va_end(vl);
	}
	else strcpy(str1, str0);

	return str1;
}

int str_to_int (const char *str)
{
	int x;
	if (sscanf(str, "%d", &x) == 1) return x;
	else
	{
		f_print(F_WARNING, "Warning: Not an integer: %s\n", str);
		return -1;
	}
}

bool scan_hex (const char *str, int *x_ptr)
{
	unsigned int x;
	if (sscanf(str, "%x", &x) == 1)
	{
		*x_ptr = (int) x;
		return 1;
	}
	else return 0;
}

char * str_sub (const char *str, int l, int r)
{
	int len = r - l + 1;
	char *sub = new_str(len);

	for (int i = 0; i < len; i++) sub[i] = str[l + i];
	sub[len] = '\0';

	return sub;
}

char * str_strip_end (const char *str, const char *unwanted)
{
	int len = str_length(str);

	char *fixed = cat1(str);
	for (int i = 1; i < len; i++)
	{
		bool matched = 0;
		for (int j = 0; j < str_length(unwanted); j++)
			if (fixed[len - i] == unwanted[j])
			{
				fixed[len - i] = '\0';
				matched = 1;
				break;
			}

		if (!matched) break;
	}

	return fixed;
}

bool str_equal (const char *a, const char *b)
{
	return (a != NULL && b != NULL) ? (strcmp(a, b) == 0) :
	       (a == NULL && b == NULL) ? 1 : 0;
}

int str_length (const char *str)
{
	return (str != NULL) ? (int) strlen(str) : 0;
}

int count_char (const char *str, char c)
{
	int N = 0;
	for (size_t i = 0; str[i] != '\0'; i++) if (str[i] == c) N++;
	return N;
}
