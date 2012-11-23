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

#ifndef _LIB_UTIL_FS_H
#define _LIB_UTIL_FS_H 1

void remember_argv0 (const char *str);

char * catg (char *gstr);

char * extract_dir  (const char *filename);
char * extract_base (const char *filename);

char * libpath     (const char *filename);
char * libpath_sys (const char *filename);
char * sharepath   (const char *filename);
char * configpath  (const char *filename);

void verify_config_dir  (void);
void verify_config_file (const char *name, const char *text);

#endif
