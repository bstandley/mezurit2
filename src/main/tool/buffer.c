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

#include "buffer.h"

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/str.h>
#include <lib/util/num.h>
#include <lib/util/fs.h>
#include <lib/hardware/timing.h>
#include <control/server.h>

static long save_buffer (Buffer *buffer, const char *filename, bool always_append, bool overwrite);
static bool would_be_similar (VSP a, VSP b);

#include "buffer_callback.c"

void buffer_init (Buffer *buffer, GtkWidget *parent)
{
	f_start(F_INIT);

	GtkWidget *hbox = pack_start(set_margins(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4), 2, 4, 10, 10), 0, parent);

	buffer->tzero_button = pack_start(gtk_button_new_with_label("T=0"),                                0, hbox);
	buffer->clear_button = pack_start(gtk_button_new_with_label("CLEAR"),                              0, hbox);
	buffer->add_button   = pack_start(gtk_button_new_from_icon_name("list-add", GTK_ICON_SIZE_BUTTON), 0, hbox);
	GtkWidget *file_hbox = pack_start(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0),                      1, hbox);
	buffer->save_button  = pack_start(gtk_button_new_with_label("SAVE"),                               0, hbox);

	set_padding(file_hbox, 8);
	buffer->file_entry  = pack_start(new_entry(0, -1),                                                      1, file_hbox);
	buffer->file_button = pack_start(gtk_button_new_from_icon_name("text-x-generic", GTK_ICON_SIZE_BUTTON), 0, file_hbox);

	gtk_widget_set_tooltip_text(buffer->add_button,  "Start new dataset");
	gtk_widget_set_tooltip_text(buffer->file_button, "Select filename");
	buffer->adjusted = 0;

	mt_mutex_init(&buffer->mutex);
	mt_mutex_init(&buffer->confirming);

	buffer->svs = NULL;
	buffer->timer = timer_new();
	buffer->do_time_reset = 1;
	buffer->percent = 0;

	buffer->filename = NULL;
	buffer->locked = 0;

	buffer->displayed_empty = 0;
	buffer->displayed_filling = 1;
}

void buffer_update (Buffer *buffer, ChanSet *chanset)
{
	f_start(F_UPDATE);

	buffer->locked = 0;

	if (buffer->svs == NULL || total_pts(buffer->svs) == 0) clear_buffer(buffer, chanset, 0, 0);
	else
	{
		// check if channel setup is compatible with old one:
		VSP test_vs = prepare_vset(chanset);
		bool compatible = would_be_similar(test_vs, buffer->svs->data[0]);
		free_vset(test_vs);

		if (compatible)
		{
			for (int k = 0; k < buffer->svs->N_set; k++)
				for (int i = 0; i < test_vs->N_col; i++)
					set_colsave(buffer->svs->data[k], i, get_colsave(test_vs, i));  // apply new save channel settings to old data
		}
		else
		{
			char *text _strfree_ = cat3("The current set of channels is not compatible with the current data buffer.\n",
			                            "Would you like to clear the buffer now?\n",
			                            "If not, no additional data can be recorded until the buffer is manually cleared.");

			if (!run_yes_no_dialog(text) || !clear_buffer(buffer, chanset, 1, 0)) buffer->locked = 1;
		}
	}
}

void buffer_final (Buffer *buffer)
{
	f_start(F_INIT);

	mt_mutex_clear(&buffer->mutex);
	mt_mutex_clear(&buffer->confirming);
	free_svset(buffer->svs);
	timer_destroy(buffer->timer);
}

void buffer_register (Buffer *buffer, int pid, ChanSet *chanset)
{
	f_start(F_INIT);

	mcf_register(NULL, "# Buffers", MCF_W);

	snazzy_connect(buffer->tzero_button, "clicked",                          SNAZZY_VOID_VOID, BLOB_CALLBACK(tzero_cb),         0x10, buffer);
	snazzy_connect(buffer->add_button,   "clicked",                          SNAZZY_VOID_VOID, BLOB_CALLBACK(add_set_cb),       0x20, buffer, chanset);
	snazzy_connect(buffer->file_entry,   "key-press-event, focus-out-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(filename_cb),      0x10, buffer);
	snazzy_connect(buffer->file_entry,   "draw",                             SNAZZY_BOOL_PTR,  BLOB_CALLBACK(buffer_expose_cb), 0x10, buffer);
	snazzy_connect(buffer->file_button,  "clicked",                          SNAZZY_VOID_VOID, BLOB_CALLBACK(choose_file_cb),   0x10, buffer);
	snazzy_connect(buffer->save_button,  "clicked",                          SNAZZY_VOID_VOID, BLOB_CALLBACK(save_cb),          0x10, buffer);

	control_server_connect(M2_TS_ID, "tzero",    M2_CODE_DAQ << pid, BLOB_CALLBACK(tzero_csf),    0x10, buffer);
	control_server_connect(M2_TS_ID, "new_set",  M2_CODE_GUI << pid, BLOB_CALLBACK(add_set_csf),  0x20, buffer, chanset);
	control_server_connect(M2_TS_ID, "save",     M2_CODE_GUI << pid, BLOB_CALLBACK(save_csf),     0x10, buffer);
	control_server_connect(M2_TS_ID, "get_time", M2_CODE_DAQ << pid, BLOB_CALLBACK(get_time_csf), 0x10, buffer);

	control_server_connect(M2_LS_ID, "tzero",    M2_CODE_DAQ << pid, BLOB_CALLBACK(tzero_csf),    0x10, buffer);
	control_server_connect(M2_LS_ID, "new_set",  M2_CODE_DAQ << pid, BLOB_CALLBACK(add_set_csf),  0x20, buffer, chanset);
	control_server_connect(M2_LS_ID, "save",     M2_CODE_DAQ << pid, BLOB_CALLBACK(save_csf),     0x10, buffer);
}

VSP active_vsp (Buffer *buffer)
{
	SVSP svs = buffer->svs;
	return (svs != NULL && svs->N_set > 0) ? svs->data[svs->N_set - 1] : NULL;
}

bool would_be_similar (VSP a, VSP b)
{
	f_start(F_UPDATE);

	if (a == NULL || b == NULL) return 0;
	if (a->N_col != b->N_col)   return 0;

	for (int i = 0; i < a->N_col; i++)
	{
		if (!str_equal(get_colname(a, i), get_colname(b, i))) return 0;
		if (!str_equal(get_colunit(a, i), get_colunit(b, i))) return 0;
	}

	return 1;
}

bool clear_buffer (Buffer *buffer, ChanSet *chanset, bool confirm, bool tzero)
{
	f_start(F_RUN);

	bool proceed = 1;

	if (confirm)
	{
		mt_mutex_lock(&buffer->confirming);

		mt_mutex_lock(&buffer->mutex);
		confirm = unsaved_data(buffer->svs);
		mt_mutex_unlock(&buffer->mutex);

		if (confirm) proceed = run_yes_no_dialog("There are unsaved points in the buffer.\nClear buffer anyway?");

		mt_mutex_unlock(&buffer->confirming);
	}

	if (proceed)
	{
		SVSP svs = new_svset();
		VSP vs = prepare_vset(chanset);

		if (svs != NULL && vs != NULL)
		{
			append_vset(svs, vs);

			mt_mutex_lock(&buffer->mutex);
			free_svset(buffer->svs);
			buffer->svs = svs;
			buffer->locked = 0;
			if (tzero) buffer->do_time_reset = 1;
			buffer->percent = 0;
			mt_mutex_unlock(&buffer->mutex);

			status_add(1, supercat("Cleared buffer (Time reset: %s)\n", tzero ? "Y" : "N"));
		}
		else
		{
			free_svset(svs);
			free_vset(vs);
			proceed = 0;
		}
	}

	return proceed;
}

bool add_set (Buffer *buffer, ChanSet *chanset)  // needs outer locks
{
	f_start(F_RUN);

	if (!buffer->locked && buffer->svs->last_vs->N_pt > 0)
	{
		VSP vs = prepare_vset(chanset);
		if (vs != NULL)
		{
			append_vset(buffer->svs, vs);
			return 1;
		}
	}

	return 0;
}

long save_buffer (Buffer *buffer, const char *filename, bool always_append, bool overwrite)
{
	f_start(F_RUN);

	mt_mutex_lock(&buffer->mutex);
	long N_pt = write_svset_custom(buffer->svs, NULL, 0, filename, *buffer->save_header, always_append, overwrite);
	mt_mutex_unlock(&buffer->mutex);

	return N_pt;
}

void set_scan_progress (Buffer *buffer, double frac)
{
	mt_mutex_lock(&buffer->mutex);
	buffer->percent = (frac < 0) ? -1 : min_int((int) (frac * 100), 100);
	mt_mutex_unlock(&buffer->mutex);
}

void set_buffer_buttons (Buffer *buffer, bool empty, bool filling)
{
	if (buffer->displayed_empty != empty)
	{
		gtk_widget_set_sensitive(buffer->clear_button, !empty);
		gtk_widget_set_sensitive(buffer->save_button,  !empty);
		buffer->displayed_empty = empty;
	}

	if (buffer->displayed_filling != filling)
	{
		gtk_widget_set_sensitive(buffer->add_button, filling);
		buffer->displayed_filling = filling;
	}
}
