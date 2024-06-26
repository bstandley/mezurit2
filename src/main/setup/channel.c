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

#include "channel.h"

#include <lib/status.h>
#include <lib/pile.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/fs.h>
#include <lib/util/str.h>
#include <lib/util/num.h>
#include <control/server.h>

enum
{
	PREFIX_giga = 0,
	PREFIX_mega,
	PREFIX_kilo,
	PREFIX_none,
	PREFIX_milli,
	PREFIX_micro,
	PREFIX_nano,
	PREFIX_pico
};

static void show_followers (Channel *channel_array, bool mention_none);
static char * build_type (ComputeFunc *cf, bool use_all);
static char * lookup_SI_symbol (int code);
static double lookup_SI_factor (int code);

#include "channel_chanset.c"
#include "channel_callback.c"

void channel_array_init (Channel *channel_array, Section *sect, GtkWidget **apt)
{
	f_start(F_INIT);

	section_init(sect, atg(sharepath("pixmaps/tool_channel.png")), "Channels", apt);
	sect->expand_fill = 1;

	GtkWidget *table = container_add(set_margins(new_table(0, 2), 4, 4, 8, 4),
	                   pack_start(new_scrolled_window(GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC), 1, sect->box));

	table_attach(new_label("Name",       0, 0.0), 1, 0, table);
	table_attach(new_label("Prefix",     0, 0.0), 2, 0, table);
	table_attach(new_label("Unit",       0, 0.0), 3, 0, table);
	table_attach(new_label("Bin Size",   0, 0.0), 4, 0, table);
	table_attach(new_label("Expression", 0, 0.0), 5, 0, table);
	table_attach(new_label("Save",       0, 0.0), 6, 0, table);

	for (int vc = 0; vc < M2_MAX_CHAN; vc++)
	{
		Channel *channel = &channel_array[vc];

		channel->binsize_entry = new_numeric_entry(1, 6, 7);
		set_entry_min(channel->binsize_entry, 0.0);

		// widgets:
		channel->label        = table_attach(new_label(atg(supercat("X<sub>%d</sub> ", vc)), 1, 0.0), 0, vc + 1, table);
		channel->name_entry   = table_attach(new_entry(0, 10),                                        1, vc + 1, table);
		channel->prefix_combo = table_attach(gtk_combo_box_text_new(),                                2, vc + 1, table);
		channel->unit_entry   = table_attach(new_entry(0, 5),                                         3, vc + 1, table);
		/**/                    table_attach(channel->binsize_entry->widget,                          4, vc + 1, table);
		channel->expr_entry   = table_attach(new_entry(0, -1),                                        5, vc + 1, table);
		channel->save_widget  = table_attach(gtk_check_button_new(),                                  6, vc + 1, table);

		gtk_widget_set_hexpand(channel->expr_entry, 1);

		// prefixes:
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(channel->prefix_combo), atg(lookup_SI_symbol(PREFIX_giga)));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(channel->prefix_combo), atg(lookup_SI_symbol(PREFIX_mega)));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(channel->prefix_combo), atg(lookup_SI_symbol(PREFIX_kilo)));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(channel->prefix_combo), "-");
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(channel->prefix_combo), atg(lookup_SI_symbol(PREFIX_milli)));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(channel->prefix_combo), atg(lookup_SI_symbol(PREFIX_micro)));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(channel->prefix_combo), atg(lookup_SI_symbol(PREFIX_nano)));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(channel->prefix_combo), atg(lookup_SI_symbol(PREFIX_pico)));

		// initialize:
		channel->name = NULL;
		channel->unit = NULL;
		channel->expr = NULL;
		channel->full_unit = NULL;
		channel->type = NULL;
		channel->desc = NULL;
		channel->desc_long = NULL;

		channel->sub_vc = -1;
		channel->sub_expr = NULL;

		channel->block_prefix_cb = 0;

		compute_func_init(&channel->cf);
	}

	gtk_widget_show(sect->box);
}

void channel_array_register (Channel *channel_array, Section *sect, GtkWidget **apt)
{
	f_start(F_INIT);

	mcf_register(NULL, "# Channels",   MCF_W);
	mcf_register(NULL, "num_channels", MCF_INT);  // obsolete

	section_register(sect, "channels_", SECTION_LEFT, apt);

	for (int vc = 0; vc < M2_MAX_CHAN; vc++)
	{
		Channel *channel = &channel_array[vc];
		char *prefix = atg(supercat("ch%d_", vc));

		int name_var    = mcf_register(&channel->name,    atg(cat2(prefix, "name")),     MCF_STRING | MCF_W | MCF_DEFAULT, "");
		int unit_var    = mcf_register(&channel->unit,    atg(cat2(prefix, "unit")),     MCF_STRING | MCF_W | MCF_DEFAULT, "");
		int prefix_var  = mcf_register(&channel->prefix,  atg(cat2(prefix, "prefix")),   MCF_INT    | MCF_W | MCF_DEFAULT, PREFIX_none);
		int expr_var    = mcf_register(&channel->expr,    atg(cat2(prefix, "expr")),     MCF_STRING | MCF_W | MCF_DEFAULT, "");
		int save_var    = mcf_register(&channel->save,    atg(cat2(prefix, "save")),     MCF_BOOL   | MCF_W | MCF_DEFAULT, 1);
		int binsize_var = mcf_register(&channel->binsize, atg(cat2(prefix, "bin_size")), MCF_DOUBLE | MCF_W | MCF_DEFAULT, 0.01);

		mcf_connect(name_var,    "setup", BLOB_CALLBACK(channel_string_mcf),  0x10, channel->name_entry);
		mcf_connect(prefix_var,  "setup", BLOB_CALLBACK(channel_prefix_mcf),  0x10, channel);
		mcf_connect(unit_var,    "setup", BLOB_CALLBACK(channel_string_mcf),  0x10, channel->unit_entry);
		mcf_connect(expr_var,    "setup", BLOB_CALLBACK(channel_string_mcf),  0x10, channel->expr_entry);
		mcf_connect(save_var,    "setup", BLOB_CALLBACK(channel_save_mcf),    0x10, channel);
		mcf_connect(binsize_var, "setup", BLOB_CALLBACK(channel_binsize_mcf), 0x10, channel);

		snazzy_connect(channel->name_entry,            "key-press-event, focus-out-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(channel_string_cb),  0x10, &channel->name);
		snazzy_connect(channel->prefix_combo,          "changed",                          SNAZZY_VOID_VOID, BLOB_CALLBACK(channel_prefix_cb),  0x10, channel);
		snazzy_connect(channel->unit_entry,            "key-press-event, focus-out-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(channel_string_cb),  0x10, &channel->unit);
		snazzy_connect(channel->binsize_entry->widget, "key-press-event, focus-out-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(channel_binsize_cb), 0x10, channel);
		snazzy_connect(channel->expr_entry,            "key-press-event, focus-out-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(channel_string_cb),  0x10, &channel->expr);
		snazzy_connect(channel->save_widget,           "button-release-event",             SNAZZY_BOOL_PTR,  BLOB_CALLBACK(channel_save_cb),    0x10, channel);
	}

	control_server_connect(M2_TS_ID, "set_follower",   M2_CODE_SETUP | all_pid(M2_CODE_DAQ), BLOB_CALLBACK(set_follower_csf),   0x10, channel_array);
	control_server_connect(M2_TS_ID, "show_followers", M2_CODE_SETUP | all_pid(M2_CODE_GUI), BLOB_CALLBACK(show_followers_csf), 0x10, channel_array);
}

void channel_array_final (Channel *channel_array)
{
	f_start(F_INIT);

	for (int vc = 0; vc < M2_MAX_CHAN; vc++)
	{
		replace(channel_array[vc].name,      NULL);
		replace(channel_array[vc].unit,      NULL);
		replace(channel_array[vc].expr,      NULL);
		replace(channel_array[vc].full_unit, NULL);
		replace(channel_array[vc].type,      NULL);
		replace(channel_array[vc].desc,      NULL);
		replace(channel_array[vc].desc_long, NULL);
		replace(channel_array[vc].cf.info,   NULL);

		destroy_entry(channel_array[vc].binsize_entry);
	}
}

void show_followers (Channel *channel_array, bool mention_none)
{
	f_start(F_VERBOSE);

	bool some = 0;
	char *lines[M2_MAX_CHAN];

	for (int vc = 0; vc < M2_MAX_CHAN; vc++)
	{
		Channel *channel = &channel_array[vc];
		if (channel->sub_vc != -1)
		{
			lines[vc] = atg(supercat("X%d = f(X%d), f(x) = %s, Valid? %s\n", channel->sub_vc, vc, channel->sub_expr, channel->cf.sub_cf != NULL ? "Y" : "N"));
			some = 1;
		}
		else lines[vc] = NULL;
	}

	if      (some)         status_add(1, join_lines(lines, "", M2_MAX_CHAN));
	else if (mention_none) status_add(1, cat1("There are no follower channels.\n"));
}
