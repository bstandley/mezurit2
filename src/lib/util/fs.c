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

#include "fs.h"

#define HEADER_SANS_WARNINGS <glib/gstdio.h>
#include <sans_warnings.h>
#ifdef MINGW
#include <windows.h>
#endif

#include <config.h>
#include <lib/status.h>
#include <lib/util/str.h>

#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

static char * argv0;

static char * trypath (const char *dir, const char *filename);
static char * respath (const char *sysdir, const char *filename, bool local_ok);

void remember_argv0 (const char *str)
{
	argv0 = cat1(str);
}

char * catg (char *gstr)
{
	char *cstr = cat1(gstr);
	g_free(gstr);
	return cstr;
}

void verify_config_dir (void)
{
	char *dir _strfree_ = configpath(NULL);
	if (g_access(dir, R_OK | W_OK | X_OK) != 0)
	{
		g_mkdir(dir, 0755);
		status_add(0, supercat("Created directory: %s\n", dir));
	}
	else f_print(F_INIT, "Found directory: %s\n", dir);
}

void verify_config_file (const char *name, const char *text)
{
	char *path _strfree_ = configpath(name);
	if (!g_file_test(path, G_FILE_TEST_EXISTS))
	{
		FILE *file = fopen(path, "w");
		fprintf(file, "# %s: %s user config\n\n", name, quote(PROG2));
		fprintf(file, "# Created by %s, will not be overwritten.\n\n", quote(PROG2));
		fprintf(file, "%s", text);
		fclose(file);

		status_add(0, supercat("Created user config file: %s\n", path));
	}
	else status_add(0, supercat("Found user config file: %s\n", path));
}

char * extract_dir (const char *filename)  // will return an existing dir, or else NULL
{
	if (g_file_test(filename, G_FILE_TEST_IS_DIR)) return cat1(filename);

	gchar *dirname = g_path_get_dirname(filename);
	if (g_file_test(dirname, G_FILE_TEST_IS_DIR)) return catg(dirname);

	g_free(dirname);
	return NULL;
}

char * extract_base (const char *filename)
{
	if (str_length(filename) == 0) return NULL;

	if (g_file_test(filename, G_FILE_TEST_IS_DIR)) return NULL;

	return catg(g_path_get_basename(filename));
}

gchar * trypath (const char *dir, const char *filename)
{
	gchar *path = g_build_filename(dir, filename, NULL);
	if (g_access(path, R_OK) == 0) return catg(path);
	else
	{
		g_free(path);
		return NULL;
	}
}

char * respath (const char *sysdir, const char *filename, bool local_ok)
{
	f_print(F_VERBOSE, "Sysdir: %s, Filename: %s\n", sysdir, filename);

	char *path;
#ifndef MINGW
	gchar **argv0v = g_strsplit(argv0, "/", -1);
	gchar *curdir _strfree_ = catg(g_get_current_dir());
	if (local_ok && argv0v[1] != NULL && !str_equal(curdir, quote(BINPATH)))
	{
		f_print(F_VERBOSE, "Note: Using local resource files.\n");
		path = trypath(argv0v[0], filename);
	}
	else path = trypath(sysdir, filename);
	g_strfreev(argv0v);
#else
	char exe_path[MAX_PATH];
	GetModuleFileName(NULL, exe_path, MAX_PATH);
	char *dir _strfree_ = catg(g_path_get_dirname(exe_path));
	path = trypath(dir, filename);
#endif

	if (path == NULL) f_print(F_WARNING, "Warning: Resource file not found: %s.\n", filename);
	return path;
}

char * libpath (const char *filename)
{
	return respath(quote(LIBPATH), filename, 1);
}

char * libpath_sys (const char *filename)
{
	return respath(quote(LIBPATH), filename, 0);
}

char * sharepath (const char *filename)
{
	return respath(quote(SHAREPATH), filename, 1);
}

char * configpath (const char *filename)
{
#ifndef MINGW
	return catg(g_build_filename(g_get_user_config_dir(), quote(PROG1), filename, NULL));
#else
	return catg(g_build_filename(g_get_user_config_dir(), quote(PROG2), filename, NULL));
#endif
}
