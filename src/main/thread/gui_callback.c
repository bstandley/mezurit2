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

static void clear_cb (GtkWidget *widget, ThreadVars *tv, Buffer *buffer, Plot *plot);
static void record_cb (GtkWidget *widget, ThreadVars *tv);
static void scan_cb (GtkWidget *widget, ThreadVars *tv);
static gboolean gpib_pause_cb (GtkWidget *widget, GdkEvent *event, ThreadVars *tv);
static char * read_channel_csf (gchar **argv, ChanSet *chanset, double *data, bool *known);
static char * get_sweep_id_csf (gchar **argv, ChanSet *chanset);
static char * catch_sweep_csf (gchar **argv, ThreadVars *tv);
static char * catch_signal_csf (gchar **argv, ThreadVars *tv);
static char * catch_scan_csf (gchar **argv);
static char * clear_csf (gchar **argv, ThreadVars *tv, Buffer *buffer, Plot *plot);
static char * gpib_send_recv_csf (gchar **argv, ThreadVars *tv);
static char * gpib_pause_csf (gchar **argv, ThreadVars *tv, Logger *logger);

char * read_channel_csf (gchar **argv, ChanSet *chanset, double *data, bool *known)
{
	f_start(F_CONTROL);

	int vc;
	if (scan_arg_int(argv[1], "channel", &vc) && vc >= 0 && vc < chanset->N_total_chan)
	{
		int vci = chanset->vci_by_vc[vc];
		if (vci != -1) return supercat("%s;value|%f;known|%d", argv[0], data[vci], known[vci] ? 1 : 0);
	}

	return cat1("argument_error");
}

char * get_sweep_id_csf (gchar **argv, ChanSet *chanset)
{
	f_start(F_CONTROL);

	int vc;
	if (scan_arg_int(argv[1], "channel", &vc) && vc >= 0 && vc < M2_MAX_CHAN)
	{
		int ici = chanset->ici_by_vc[vc];
		return supercat("%s;id|%d", argv[0], ici);  // may be -1 if vc is not invertible
	}

	return cat1("argument_error");
}

char * catch_sweep_csf (gchar **argv, ThreadVars *tv)
{
	f_start(F_CONTROL);

	int vc;
	if (scan_arg_int(argv[1], "channel", &vc) && vc >= 0 && vc < M2_MAX_CHAN)
	{
		int ici = tv->chanset->ici_by_vc[vc];
		if (ici != -1)
		{
			tv->catch_sweep_ici = ici;
			return NULL;  // reply later
		}
	}

	return cat1("argument_error");
}

char * catch_signal_csf (gchar **argv, ThreadVars *tv)
{
	f_start(F_CONTROL);

	char *name _strfree_ = NULL;
	if (scan_arg_string(argv[1], "name", &name))
	{
		replace(tv->catch_signal, cat1(name));
		return NULL;  // reply later (when signal is emitted via emit_signal_csf())
	}

	return cat1("argument_error");
}

char * catch_scan_csf (gchar **argv)
{
	f_start(F_CONTROL);

	return NULL;  // reply later
}

void record_cb (GtkWidget *widget, ThreadVars *tv)
{
	f_start(F_CALLBACK);

	set_recording(tv, RL_TOGGLE);
}

gboolean gpib_pause_cb (GtkWidget *widget, GdkEvent *event, ThreadVars *tv)
{
	f_start(F_CALLBACK);

	bool was_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

	mt_mutex_lock(&tv->gpib_mutex);
	tv->gpib_paused = !was_active;
	mt_mutex_unlock(&tv->gpib_mutex);

	return 0;
}

char * gpib_send_recv_csf (gchar **argv, ThreadVars *tv)
{
	f_start(F_CONTROL);

	int id, pad, eos;
	char *msg;
	bool expect_reply;

	if (scan_arg_int   (argv[1], "id",           &id)  &&
	    scan_arg_int   (argv[2], "pad",          &pad) &&
	    scan_arg_int   (argv[3], "eos",          &eos) &&
	    scan_arg_string(argv[4], "msg",          &msg) &&
	    scan_arg_bool  (argv[5], "expect_reply", &expect_reply))
	{
		if (tv->pid == -1)
		{
			if (eos) gpib_device_set_eos(id, pad, eos);

			char *msg_out = gpib_string_query(id, pad, msg, str_length(msg), expect_reply);
			return expect_reply ? cat3(get_cmd(M2_TS_ID), ";msg|", msg_out) : cat1(get_cmd(M2_TS_ID));
		}
		else
		{
			mt_mutex_lock(&tv->gpib_mutex);
			tv->gpib_id = id;
			tv->gpib_pad = pad;
			tv->gpib_eos = eos;
			tv->gpib_msg = msg;
			tv->gpib_expect_reply = expect_reply;
			mt_mutex_unlock(&tv->gpib_mutex);

			return NULL;  // reply later, in GPIB thread
		}
	}
	else return cat1("argument_error");
}

char * gpib_pause_csf (gchar **argv, ThreadVars *tv, Logger *logger)
{
	f_start(F_CONTROL);

	bool paused;
	if (scan_arg_bool(argv[1], "paused", &paused))
	{
		mt_mutex_lock(&tv->gpib_mutex);
		tv->gpib_paused = paused;
		mt_mutex_unlock(&tv->gpib_mutex);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(logger->gpib_button), paused);
		return cat1(argv[0]);
	}
	else return cat1("argument_error");
}

void scan_cb (GtkWidget *widget, ThreadVars *tv)
{
	f_start(F_CALLBACK);

	bool on = set_scanning(tv, RL_TOGGLE);
	if (on) set_scan_callback_mode(tv, 1);
}

void clear_cb (GtkWidget *widget, ThreadVars *tv, Buffer *buffer, Plot *plot)
{
	f_start(F_CALLBACK);

	if (clear_buffer(buffer, tv->chanset, 1, *buffer->link_tzero))  // confirm = 1, tzero = link_tzero
	{
		plot_update_channels(plot, tv->chanset->vc_by_vci, buffer->svs);
		plot_build(plot);

		set_recording(tv, RL_NO_HOLD);
		set_scanning(tv, RL_NO_HOLD);
	}
}

char * clear_csf (gchar **argv, ThreadVars *tv, Buffer *buffer, Plot *plot)
{
	f_start(F_CONTROL);

	bool confirm, tzero;
	if (scan_arg_bool(argv[1], "confirm", &confirm) && scan_arg_bool(argv[2], "tzero", &tzero))
	{
		bool cleared = clear_buffer(buffer, tv->chanset, confirm, tzero);
		if (cleared)
		{
			plot_update_channels(plot, tv->chanset->vc_by_vci, buffer->svs);
			plot_build(plot);

			set_recording(tv, RL_NO_HOLD);
			set_scanning(tv, RL_NO_HOLD);
		}

		return supercat("%s;buffer_cleared|%d", argv[0], cleared ? 1 : 0);
	}
	else return cat1("argument_error");
}
