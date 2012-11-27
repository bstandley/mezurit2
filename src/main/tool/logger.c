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

#include "logger.h"

#include <math.h>

#include <lib/status.h>
#include <lib/mcf2.h>
#include <lib/gui.h>
#include <lib/util/fs.h>
#include <lib/util/str.h>
#include <lib/util/num.h>

#include "logger_callback.c"

void logger_init (Logger *logger, GtkWidget **apt)
{
	f_start(F_INIT);

	section_init(&logger->sect, atg(sharepath("pixmaps/tool_logger.png")), "Acquisition", apt);
	add_loc_menu(&logger->sect, SECTION_WAYLEFT, SECTION_LEFT, SECTION_TOP, SECTION_BOTTOM, SECTION_RIGHT, SECTION_WAYRIGHT, -1);
	add_rollup_button(&logger->sect);
	add_orient_button(&logger->sect);

	GtkWidget *table = pack_start(new_table(2, 3, 4, 2), 0, logger->sect.box);

	logger->max_rate_entry = new_numeric_entry(0, 3, 7);
	set_entry_unit(logger->max_rate_entry, "kHz");
	set_entry_min (logger->max_rate_entry, 0.010);

	table_attach(new_label("f<sub>max</sub>", 0.0), 0, 0, GTK_FILL, table);
	table_attach(logger->max_rate_entry->widget,    1, 0, 0,        table);
	table_attach(new_label("N<sub>ave</sub>", 0.0), 0, 1, GTK_FILL, table);

	logger->cbuf_length_widget = pack_start(fix_shadow(gtk_spin_button_new_with_range(1, M2_MAX_CBUF_LENGTH, 1)), 0,
	                             table_attach(gtk_hbox_new(0, 0), 1, 1, GTK_FILL, table));

	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(logger->cbuf_length_widget), 0);
	gtk_entry_set_width_chars(GTK_ENTRY(logger->cbuf_length_widget), 2);

	GtkWidget *lower_vbox = pack_start(gtk_vbox_new(0, 0), 0, logger->sect.box);

	GtkWidget *reader_hbox = container_add(gtk_hbox_new(0, 0),
	                         container_add(fix_shadow(gtk_frame_new(NULL)),
	                         pack_start(name_widget(gtk_event_box_new(), "m2_align"), 0, lower_vbox)));

	logger->reader_labels = pack_start(new_text_view(1, 8), 0, reader_hbox);
	logger->reader_values = pack_start(new_text_view(0, 0), 1, reader_hbox);
	logger->reader_units  = pack_start(new_text_view(0, 0), 0, reader_hbox);
	logger->reader_types  = pack_start(new_text_view(6, 2), 0, reader_hbox);

	gtk_text_view_set_justification(GTK_TEXT_VIEW(logger->reader_values), GTK_JUSTIFY_RIGHT);

	logger->reader_hash = 0;
	logger->reader_str = NULL;
	logger->reader_textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(logger->reader_values));  // shortcut for later

	logger->resizer_queued = 0;
	logger->resizer_timer = timer_new();
	logger->resizer_w0 = logger->resizer_w1 = 0;

	PangoLayout *layout = pango_layout_new(gtk_widget_get_pango_context(logger->reader_values));
	pango_layout_set_text(layout, "–", -1); pango_layout_get_pixel_size(layout, &logger->minus_width,  NULL);
	pango_layout_set_text(layout, ".", -1); pango_layout_get_pixel_size(layout, &logger->period_width, NULL);
	pango_layout_set_text(layout, "5", -1); pango_layout_get_pixel_size(layout, &logger->digit_width,  NULL);
	g_object_unref(layout);

	GtkWidget *control_vbox = gtk_vbox_new(0, 2);
	gtk_table_attach(GTK_TABLE(table), control_vbox, 2, 3, 0, 2, 0, 0, 0, 0);

	logger->button = pack_start(gtk_button_new(), 0, control_vbox);
	logger->image  = pack_start(gtk_image_new(),  0, control_vbox);

	set_draw_on_expose(logger->sect.full, logger->image);

	logger->gpib_button = pack_end(size_widget(gtk_check_button_new_with_label("Pause GPIB"), -1, 22), 0,
	                      pack_end(gtk_hbox_new(0, 0),                                                 0, lower_vbox));

	mt_mutex_init(&logger->mutex);
	logger->block_cbuf_length_cb = 0;
}

void logger_update (Logger *logger, ChanSet *chanset)
{
	f_start(F_UPDATE);

	// update reader labels (OK with N_total_chan = 0)

	char *label_str [M2_MAX_CHAN];
	char *unit_str  [M2_MAX_CHAN];
	char *type_str  [M2_MAX_CHAN];

	for (int vci = 0; vci < chanset->N_total_chan; vci++)
	{
		Channel *channel = chanset->channel_by_vci[vci];

		label_str[vci] = atg(supercat("X%d: %s", chanset->vc_by_vci[vci], channel->name));
		unit_str[vci]  = atg(str_length(channel->full_unit) > 0 ? cat2(" ", channel->full_unit) : cat1(""));
		type_str[vci]  = atg(cat1(channel->type));
	}

	replace(logger->reader_str, new_str(chanset->N_total_chan * (str_length(atg(cat1("–"))) + 32)));  // endash is a multi-byte char (+32 for number and newline)
	gtk_widget_set_size_request(logger->reader_values, -1, -1);

	set_text_view_text(logger->reader_labels, chanset->N_total_chan > 0 ? atg(join_lines(label_str, "\n", chanset->N_total_chan)) : " (none)");
	set_text_view_text(logger->reader_units,  chanset->N_total_chan > 0 ? atg(join_lines(unit_str,  "\n", chanset->N_total_chan)) : " ");
	set_text_view_text(logger->reader_types,  chanset->N_total_chan > 0 ? atg(join_lines(type_str,  "\n", chanset->N_total_chan)) : " ");

	set_visibility(logger->gpib_button, chanset->N_gpib > 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(logger->gpib_button), 0);  // not paused
}

void set_logger_runlevel (Logger *logger, int rl)
{
	f_start(F_RUN);

	gtk_widget_set_sensitive(logger->button, rl != LOGGER_RL_STOP && rl != LOGGER_RL_HOLD);

	gtk_button_set_label(GTK_BUTTON(logger->button), rl == LOGGER_RL_RECORD || rl == LOGGER_RL_WAIT ? "STOP" : "START");
	gtk_image_set_from_pixbuf(GTK_IMAGE(logger->image), lookup_pixbuf(rl == LOGGER_RL_WAIT   ? PIXBUF_RL_WAIT   :
	                                                                  rl == LOGGER_RL_RECORD ? PIXBUF_RL_RECORD :
	                                                                  rl == LOGGER_RL_IDLE   ? PIXBUF_RL_IDLE   :
	                                                                  rl == LOGGER_RL_HOLD   ? PIXBUF_RL_HOLD   :
	                                                                                           PIXBUF_RL_STOP));
}

void set_logger_scanning (Logger *logger, bool scanning)
{
	gtk_widget_set_sensitive(logger->reader_labels, !scanning);
	gtk_widget_set_sensitive(logger->reader_values, !scanning);
	gtk_widget_set_sensitive(logger->reader_types,  !scanning);
}

void run_reader_status (Logger *logger, ChanSet *chanset, bool *known, double *data)
{
	// profiling: using spot hack reduced exec time of "for" loop from 9μs to 7μs

	bool have_minus = 0;
	double max_neg = 1.1;  // always get at least one digit this way
	double max_pos = 1.1;

	int spot = 0;
	for (int vci = 0; vci < chanset->N_total_chan; vci++)
	{
		spot += known[vci] ? sprintf(&logger->reader_str[spot], vci == 0 ? "%s%1.4f" : "\n%s%1.4f", data[vci] < 0 ? "–" : "", fabs(data[vci])) :
		                     sprintf(&logger->reader_str[spot], vci == 0 ? "??     " : "\n??     ");

		if (data[vci] < 0)
		{
			have_minus = 1;
			max_neg = max_double(max_neg, fabs(data[vci]));
		}
		else max_pos = max_double(max_pos, data[vci]);
	}

	bool forced = 0;

	guint hash = g_str_hash(logger->reader_str);
	if (hash != logger->reader_hash)
	{
		logger->reader_hash = hash;
		gtk_text_buffer_set_text(logger->reader_textbuf, logger->reader_str, -1);

		int est_plus  =                                    ceil_int(log10(max_pos) + 4) * logger->digit_width + logger->period_width + 5;
		int est_minus = have_minus ? logger->minus_width + ceil_int(log10(max_neg) + 4) * logger->digit_width + logger->period_width + 5: 0;
		int est_final = max_int(50, max_int(est_plus, est_minus));

		if (est_final < logger->resizer_w0)
		{
			if (!logger->resizer_queued)
			{
				logger->resizer_queued = 1;
				timer_reset(logger->resizer_timer);
				logger->resizer_w1 = est_final;
			}
		}
		else
		{
			logger->resizer_queued = 0;

			if (est_final > logger->resizer_w0)
			{
				forced = 1;
				logger->resizer_w1 = est_final;
			}
		}
	}

	if (forced || (logger->resizer_queued && timer_elapsed(logger->resizer_timer) > logger->resizer_delay))
	{
		gtk_widget_set_size_request(logger->reader_values, logger->resizer_w1, -1);
		logger->resizer_w0 = logger->resizer_w1;
	}
}

void logger_final (Logger *logger)
{
	f_start(F_INIT);

	mt_mutex_clear(&logger->mutex);
	destroy_entry(logger->max_rate_entry);
	timer_destroy(logger->resizer_timer);
	replace(logger->reader_str, NULL);
}

void logger_register (Logger *logger, int pid, GtkWidget **apt)
{
	f_start(F_INIT);

	mcf_register(NULL, "# Acquisition", MCF_W);
	section_register(&logger->sect, atg(supercat("panel%d_acquisition_", pid)), SECTION_LEFT, apt);

	int max_rate_var      = mcf_register(&logger->max_rate,      atg(supercat("panel%d_logger_max_acquisition_rate", pid)), MCF_DOUBLE | MCF_W | MCF_DEFAULT, 0.8);
	int cbuf_length_var   = mcf_register(&logger->cbuf_length,   atg(supercat("panel%d_logger_circle_buffer_length", pid)), MCF_INT    | MCF_W | MCF_DEFAULT, 1);
	int resizer_delay_var = mcf_register(&logger->resizer_delay, atg(supercat("panel%d_logger_reader_resize_delay",  pid)), MCF_DOUBLE | MCF_W | MCF_DEFAULT, 5.0);

	mcf_connect(max_rate_var,      "setup, panel", BLOB_CALLBACK(max_rate_mcf),    0x10, logger);
	mcf_connect(cbuf_length_var,   "setup, panel", BLOB_CALLBACK(cbuf_length_mcf), 0x10, logger);
	mcf_connect(resizer_delay_var, "setup, panel", BLOB_CALLBACK(set_double_mcf),  0x00);

	snazzy_connect(logger->max_rate_entry->widget, "key-press-event, focus-out-event", SNAZZY_BOOL_PTR,  BLOB_CALLBACK(max_rate_cb),    0x10, logger);
	snazzy_connect(logger->cbuf_length_widget,     "value-changed",                    SNAZZY_VOID_VOID, BLOB_CALLBACK(cbuf_length_cb), 0x10, logger);
}

void logger_register_legacy (Logger *logger)
{
	f_start(F_INIT);

	int max_rate_var    = mcf_register(&logger->max_rate,    "max_acquisition_rate", MCF_DOUBLE);
	int cbuf_length_var = mcf_register(&logger->cbuf_length, "circle_buffer_length", MCF_INT);

	mcf_connect_with_note(max_rate_var,    "setup", "Mapped obsolete var onto 'Panel A'.\n", BLOB_CALLBACK(max_rate_mcf),    0x10, logger);
	mcf_connect_with_note(cbuf_length_var, "setup", "Mapped obsolete var onto 'Panel A'.\n", BLOB_CALLBACK(cbuf_length_mcf), 0x10, logger);
}
