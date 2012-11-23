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

static gboolean max_rate_cb (GtkWidget *widget, GdkEvent *event, Logger *logger);
static void cbuf_length_cb  (GtkWidget *widget, Logger *logger);

static void max_rate_mcf (void *ptr, const char *signal_name, MValue value, Logger *logger);
static void cbuf_length_mcf (void *ptr, const char *signal_name, MValue value, Logger *logger);

gboolean max_rate_cb (GtkWidget *widget, GdkEvent *event, Logger *logger)
{
	if (entry_update_required(event, widget))
	{
		f_start(F_CALLBACK);

		bool ok;
		double rate = read_entry(logger->max_rate_entry, &ok);

		if (ok)
		{
			g_static_mutex_lock(&logger->mutex);
			logger->max_rate = rate;
			logger->max_rate_dirty = 1;  // tell DAQ thread to recalculate time interval
			g_static_mutex_unlock(&logger->mutex);
		}

		write_entry(logger->max_rate_entry, logger->max_rate);
	}

	return 0;
}

void max_rate_mcf (void *ptr, const char *signal_name, MValue value, Logger *logger)
{
	f_start(F_MCF);

	double rate = check_entry(logger->max_rate_entry, value.x_double);

	g_static_mutex_lock(&logger->mutex);
	logger->max_rate = rate;
	logger->max_rate_dirty = 1;  // tell DAQ thread to recalculate time interval
	g_static_mutex_unlock(&logger->mutex);

	write_entry(logger->max_rate_entry, rate);
}

void cbuf_length_cb (GtkWidget *widget, Logger *logger)
{
	if (logger->block_cbuf_length_cb > 0)
	{
		f_print(F_CALLBACK, "Skipping unnecessary callback.\n");
		logger->block_cbuf_length_cb--;
		return;
	}

	f_start(F_CALLBACK);

	int request = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
	int length = window_int(request, 1, M2_MAX_CBUF_LENGTH);

	if (length != request)
	{
		logger->block_cbuf_length_cb++;
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), length);
	}

	g_static_mutex_lock(&logger->mutex);
	logger->cbuf_length = length;
	logger->cbuf_dirty = 1;  // tell DAQ thread to reset the circle buffer
	g_static_mutex_unlock(&logger->mutex);
}

void cbuf_length_mcf (void *ptr, const char *signal_name, MValue value, Logger *logger)
{
	f_start(F_MCF);

	int length = window_int(value.x_int, 1, M2_MAX_CBUF_LENGTH);

	if (length != gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(logger->cbuf_length_widget)))
	{
		logger->block_cbuf_length_cb++;
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(logger->cbuf_length_widget), length);
	}

	g_static_mutex_lock(&logger->mutex);
	logger->cbuf_length = length;
	logger->cbuf_dirty = 1;  // tell DAQ thread to reset the circle buffer
	g_static_mutex_unlock(&logger->mutex);
}
