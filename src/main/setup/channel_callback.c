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

static gboolean channel_string_cb (GtkWidget *widget, GdkEvent *event, char **str);
static void channel_prefix_cb (GtkComboBox *combo, Channel *channel);
static gboolean channel_binsize_cb (GtkWidget *widget, GdkEvent *event, Channel *channel);
static gboolean channel_save_cb (GtkWidget *widget, GdkEvent *event, Channel *channel);

static void channel_string_mcf (char **str, const char *signal_name, MValue value, GtkWidget *widget);
static void channel_prefix_mcf (void *ptr, const char *signal_name, MValue value, Channel *channel);
static void channel_binsize_mcf (void *ptr, const char *signal_name, MValue value, Channel *channel);
static void channel_save_mcf (void *ptr, const char *signal_name, MValue value, Channel *channel);

static char * set_follower_csf (gchar **argv, Channel *channel_array);
static char * show_followers_csf (gchar **argv, Channel *channel_array);

void channel_prefix_cb (GtkComboBox *combo, Channel *channel)
{
	if (channel->block_prefix_cb > 0)
	{
		f_print(F_CALLBACK, "Skipping unnecessary callback.\n");
		channel->block_prefix_cb--;
		return;
	}

	f_start(F_CALLBACK);

	switch (gtk_combo_box_get_active(combo))
	{
		case 0: channel->prefix = PREFIX_giga;   break;
		case 1: channel->prefix = PREFIX_mega;   break;
		case 2: channel->prefix = PREFIX_kilo;   break;
		case 3: channel->prefix = PREFIX_none;   break;
		case 4: channel->prefix = PREFIX_milli;  break;
		case 5: channel->prefix = PREFIX_micro;  break;
		case 6: channel->prefix = PREFIX_nano;   break;
		case 7: channel->prefix = PREFIX_pico;   break;
		default: break;
	}
}

void channel_prefix_mcf (void *ptr, const char *signal_name, MValue value, Channel *channel)
{
	f_start(F_MCF);

	channel->prefix = window_int(value.x_int, PREFIX_giga, PREFIX_pico);

	int request;
	switch (channel->prefix)
	{
		case PREFIX_giga:  request = 0;  break;
		case PREFIX_mega:  request = 1;  break;
		case PREFIX_kilo:  request = 2;  break;
		case PREFIX_none:  request = 3;  break;
		case PREFIX_milli: request = 4;  break;
		case PREFIX_micro: request = 5;  break;
		case PREFIX_nano:  request = 6;  break;
		case PREFIX_pico:  request = 7;  break;
		default:           request = -1;
	}

	if (request != gtk_combo_box_get_active(GTK_COMBO_BOX(channel->prefix_combo)))
	{
		channel->block_prefix_cb++;
		gtk_combo_box_set_active(GTK_COMBO_BOX(channel->prefix_combo), request);
	}
}

gboolean channel_save_cb (GtkWidget *widget, GdkEvent *event, Channel *channel)
{
	f_start(F_CALLBACK);

	bool was_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	channel->save = !was_active;

	return 0;
}

void channel_save_mcf (void *ptr, const char *signal_name, MValue value, Channel *channel)
{
	f_start(F_MCF);

	channel->save = value.x_bool;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(channel->save_widget), value.x_bool);
}

gboolean channel_string_cb (GtkWidget *widget, GdkEvent *event, char **str)
{
	if (entry_update_required(event, widget))
	{
		f_start(F_CALLBACK);
		replace(*str, cat1(gtk_entry_get_text(GTK_ENTRY(widget))));
	}

	return 0;
}

void channel_string_mcf (char **str, const char *signal_name, MValue value, GtkWidget *widget)
{
	f_start(F_MCF);

	replace(*str, cat1(value.string));
	gtk_entry_set_text(GTK_ENTRY(widget), value.string);
}

gboolean channel_binsize_cb (GtkWidget *widget, GdkEvent *event, Channel *channel)
{
	if (entry_update_required(event, widget))
	{
		f_start(F_CALLBACK);

		bool ok;
		double value = read_entry(channel->binsize_entry, &ok);

		channel->binsize = (!ok || value < 0) ? -1.0 : value;

		if (channel->binsize < 0) write_blank(channel->binsize_entry);
		else                      write_entry(channel->binsize_entry, channel->binsize);
	}

	return 0;
}

void channel_binsize_mcf (void *ptr, const char *signal_name, MValue value, Channel *channel)
{
	f_start(F_MCF);

	channel->binsize = (value.x_double < 0) ? -1.0 : value.x_double;

	if (channel->binsize < 0) write_blank(channel->binsize_entry);
	else                      write_entry(channel->binsize_entry, channel->binsize);
}

char * set_follower_csf (gchar **argv, Channel *channel_array)
{
	f_start(F_CONTROL);

	int vc_l, vc_f;
	if (scan_arg_int(argv[1], "leader",   &vc_l) && vc_l >=  0 && vc_l < M2_MAX_CHAN &&
	    scan_arg_int(argv[2], "follower", &vc_f) && vc_f >= -1 && vc_f < M2_MAX_CHAN)
	{
		char *expr = NULL;
		if (vc_f == -1 || scan_arg_string(argv[3], "expr", &expr))
		{
			Channel *channel = &channel_array[vc_l];

			channel->sub_vc = vc_f;
			replace(channel->sub_expr, expr);

			compute_sub_define(&channel->cf, vc_f == -1 ? NULL : &channel_array[vc_f].cf, expr);
			return supercat("%s;success|%d", argv[0], channel->cf.sub_cf != NULL ? 1 : 0);
		}
	}

	return cat1("argument_error");
}

char * show_followers_csf (gchar **argv, Channel *channel_array)
{
	f_start(F_CONTROL);

	show_followers(channel_array, 1);

	return cat1(argv[0]);
}
