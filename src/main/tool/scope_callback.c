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

static gboolean timescale_cb (GtkWidget *widget, GdkEvent *event, Scope *scope, NumericEntry *entry, double *var);
static void timescale_mcf (double *var, const char *signal_name, MValue value, Scope *scope, NumericEntry *entry);
static void old_daq_time_mcf (double *var, const char *signal_name, MValue value, Scope *scope);

gboolean timescale_cb (GtkWidget *widget, GdkEvent *event, Scope *scope, NumericEntry *entry, double *var)
{
	if (entry_update_required(event, widget))
	{
		f_start(F_CALLBACK);

		bool ok;
		double target = read_entry(entry, &ok);

		if (ok)
		{
			g_static_mutex_lock(&scope->mutex);
			bool scanning = scope->scanning;
			if (!scanning)
			{
				*var = target;
				verify_timescale(scope);
			}
			g_static_mutex_unlock(&scope->mutex);

			if (!scanning) update_readout(scope);
		}

		write_entry(entry, *var);
	}

	return 0;
}

void timescale_mcf (double *var, const char *signal_name, MValue value, Scope *scope, NumericEntry *entry)
{
	f_start(F_MCF);

	double target = check_entry(entry, value.x_double);

	g_static_mutex_lock(&scope->mutex);
	bool scanning = scope->scanning;
	if (!scanning)
	{
		*var = target;
		if (str_equal(signal_name, "panel")) verify_timescale(scope);
	}
	g_static_mutex_unlock(&scope->mutex);

	if (str_equal(signal_name, "panel") && !scanning) update_readout(scope);
	write_entry(entry, *var);
}

void old_daq_time_mcf (double *var, const char *signal_name, MValue value, Scope *scope)
{
	f_start(F_MCF);

	if (str_equal(signal_name, "setup"))  // extra check
	{
		*var = check_entry(scope->time_entry, value.x_double * 1e3);
		write_entry(scope->time_entry, *var);
	}
}
