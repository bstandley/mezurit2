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

static void config_cb (GtkWidget *widget, Hardware *hw_array, Panel **tv_panel, OldVars *oldvars, int load, int mode);
static char * config_csf (gchar **argv, Hardware *hw_array, Panel **tv_panel, OldVars *oldvars);
static char * var_csf (gchar **argv, Panel **tv_panel);

static void panel_lock (Panel *panel);
static void panel_unlock (Panel *panel);

void panel_lock (Panel *panel)
{
	g_static_mutex_lock(&panel->logger.mutex);
	g_static_mutex_lock(&panel->scope.mutex);
	g_static_mutex_lock(&panel->sweep_mutex);
	g_static_mutex_lock(&panel->trigger_mutex);
}

void panel_unlock (Panel *panel)
{
	g_static_mutex_unlock(&panel->logger.mutex);
	g_static_mutex_unlock(&panel->scope.mutex);
	g_static_mutex_unlock(&panel->sweep_mutex);
	g_static_mutex_unlock(&panel->trigger_mutex);
}

void config_cb (GtkWidget *widget, Hardware *hw_array, Panel **tv_panel, OldVars *oldvars, int load, int mode)  // if load == 0, then save
{
	f_start(F_CALLBACK);

	// Note: The load actions only occur in "setup" mode because those menu items are set insensitive in "panel" mode.
	//       Setup actions can occur in either mode.

	hardware_array_block(hw_array);  // only applicable to loading, but is a lightweight operation so might as well do it either way

	if (load && mode == MENU_INTERNAL_DEFAULT) mcf_load_defaults("setup");
	else
	{
		char *filename = (mode == MENU_USER_DEFAULT) ? atg(configpath("default.mcf"))                                                                  :
		                 (mode == MENU_SYS_DEFAULT)  ? atg(sharepath("default.mcf"))                                                                   :
		                 (mode == MENU_LAST)         ? atg(configpath("last.mcf"))                                                                     :
		                 (mode == MENU_FILE)         ? atg(run_file_chooser(atg(cat2(load ? "Load" : "Save", " configuration")),
		                                                                    load ? GTK_FILE_CHOOSER_ACTION_OPEN : GTK_FILE_CHOOSER_ACTION_SAVE,
		                                                                    load ? GTK_STOCK_OPEN               : GTK_STOCK_SAVE, NULL))               :
		                 (mode == MENU_EXAMPLE_FILE) ? atg(run_file_chooser("Load example configuration",
		                                                                    GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_OPEN, atg(sharepath("examples")))) : NULL;

		if (filename != NULL)
		{
			if (load)
			{
				oldvars_reset(oldvars);
				mcf_read_file(filename, "setup");
				oldvars_mention(oldvars);
			}
			else
			{
				if (*tv_panel != NULL) panel_lock(*tv_panel);
				mcf_write_file(filename);
				if (*tv_panel != NULL) panel_unlock(*tv_panel);
			}
		}
	}

	hardware_array_unblock(hw_array);  // only applicable to loading, but is a lightweight operation so might as well do it either way
}

char * config_csf (gchar **argv, Hardware *hw_array, Panel **tv_panel, OldVars *oldvars)
{
	f_start(F_CONTROL);

	char *mode _strfree_ = NULL;
	if (scan_arg_string(argv[1], "mode", &mode))
	{
		hardware_array_block(hw_array);  // only applicable to loading, but is a lightweight operation so might as well do it either way

		bool load = str_equal(argv[0], "load_config");

		if (load && str_equal(mode, "internal_default"))
		{
			mcf_load_defaults("setup");
			return supercat("%s;success|1");
		}
		else
		{
			char *filename _strfree_ = NULL;
			if      (str_equal(mode, "user_default"))        filename = configpath("default.mcf");
			else if (str_equal(mode, "sys_default") && load) filename = sharepath("default.mcf");
			else if (str_equal(mode, "last")        && load) filename = configpath("last.mcf");
			else if (str_equal(mode, "file"))                scan_arg_string(argv[2], "filename", &filename);

			if (filename != NULL)
			{
				bool rv;
				if (load)
				{
					oldvars_reset(oldvars);
					rv = mcf_read_file(filename, "setup") > 0;
					oldvars_mention(oldvars);
				}
				else
				{
					if (*tv_panel != NULL) panel_lock(*tv_panel);
					rv = mcf_write_file(filename);
					if (*tv_panel != NULL) panel_unlock(*tv_panel);
				}
				return supercat("%s;success|%d", argv[0], rv ? 1 : 0);
			}
		}

		hardware_array_unblock(hw_array);  // only applicable to loading, but is a lightweight operation so might as well do it either way
	}

	return cat1("argument_error");
}

char * var_csf (gchar **argv, Panel **tv_panel)
{
	f_start(F_CONTROL);

	char *line _strfree_ = NULL;
	if (scan_arg_string(argv[1], "line", &line))
	{
		if (str_equal(argv[0], "set_var"))
		{
			bool rv = mcf_process_string(line, *tv_panel == NULL ? "setup" : "panel");
			return supercat("%s;ok|%d", argv[0], rv ? 1 : 0);
		}
		else  // "get_var"
		{
			char *value _strfree_ = mcf_lookup(line);  // take our chances w.r.t. locking
			return (value != NULL) ? supercat("%s;ok|1;value|%s", argv[0], value) :
			                         supercat("%s;ok|0",          argv[0]);
		}
	}

	return cat1("argument_error");
}

