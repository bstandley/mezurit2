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

#include "entry.h"

#include <stdlib.h>  // malloc()

#include <lib/status.h>
#include <lib/gui.h>
#include <lib/util/str.h>
#include <lib/util/num.h>

static GtkWidget *dirty_entry_widget;

void offer_min (NumericEntry *ne, double value);
void offer_max (NumericEntry *ne, double value);

NumericEntry * new_numeric_entry (int min_digits, int max_digits, int width)
{
	NumericEntry *ne = malloc(sizeof(NumericEntry));

	ne->widget = new_entry(0, width);
	g_object_ref(G_OBJECT(ne->widget));

	ne->unit = cat1("");

	ne->min_digits = min_digits;
	ne->max_digits = max_digits;

	ne->want_min = ne->want_max = 1;
	ne->have_min = ne->have_max = 0;

	return ne;
}

void destroy_entry (NumericEntry *ne)
{
	g_object_unref(G_OBJECT(ne->widget));
	replace(ne->unit, NULL);
	free(ne);
}

void set_entry_unit (NumericEntry *ne, const char *unit)
{
	replace(ne->unit, cat1(unit != NULL ? unit : ""));
}

void set_entry_min (NumericEntry *ne, double value)
{
	ne->have_min = 1;
	ne->min = value;
}

void set_entry_max (NumericEntry *ne, double value)
{
	ne->have_max = 1;
	ne->max = value;
}

void offer_min (NumericEntry *ne, double value)
{
	if (ne->want_min) set_entry_min(ne, value);
}

void offer_max (NumericEntry *ne, double value)
{
	if (ne->want_max) set_entry_max(ne, value);
}

double check_entry (NumericEntry *ne, double value)
{
	if (ne->have_min && value < ne->min) value = ne->min;
	if (ne->have_max && value > ne->max) value = ne->max;

	return value;
}

double read_entry (NumericEntry *ne, bool *ok)
{
	const gchar *str = gtk_entry_get_text(GTK_ENTRY(ne->widget));
	double value = 0;
	*ok = (sscanf(str, "%lf", &value) == 1);

	if (!(*ok)) f_print(F_VERBOSE, "\"%s\" not recognizable as a number.\n", str);

	return check_entry(ne, value);
}

void write_entry (NumericEntry *ne, double value)
{
	int digits = 12;
	char *fmt0 _strfree_ = supercat("%%1.%df", digits);
	char *str0 _strfree_ = supercat(fmt0, value);

	int len = str_length(str0);
	for (int i = 1; str0[len - i] == '0'; i++) digits--;

	if      (digits < ne->min_digits) digits = ne->min_digits;
	else if (digits > ne->max_digits) digits = ne->max_digits;

	char *fmt1 _strfree_ = supercat("%%1.%df%%s%%s", digits);
	char *str1 _strfree_ = supercat(fmt1, value, str_length(ne->unit) > 0 ? " " : "", ne->unit);

	gtk_entry_set_text(GTK_ENTRY(ne->widget), str1);
}

void write_blank (NumericEntry *ne)
{
	gtk_entry_set_text(GTK_ENTRY(ne->widget), "—");
}

void set_range (NumericRange *nr, int side, double value)
{
	nr->value[side] = value;
	nr->valid[side] = 1;

	if      (side == LOWER) offer_min(nr->entry[UPPER], value);
	else if (side == UPPER) offer_max(nr->entry[LOWER], value);
}

void write_range (NumericRange *nr)
{
	for sides
	{
		if (nr->valid[s]) write_entry(nr->entry[s], nr->value[s]);
		else              write_blank(nr->entry[s]);
	}
}

bool rectify_range (NumericRange *nr)
{
	double value[2];
	for sides { value[s] = nr->value[s]; }

	if (nr->valid[LOWER] && nr->valid[UPPER] && value[LOWER] > value[UPPER])
	{
		for sides { set_range(nr, s, value[1 - s]); }
		return 1;
	}
	else return 0;
}

void invalidate_range (NumericRange *nr, int side)
{
	if (side == UPPER)
	{
		nr->valid[UPPER] = 0;
		nr->entry[LOWER]->have_max = 0;
	}
	else if (side == LOWER)
	{
		nr->valid[LOWER] = 0;
		nr->entry[UPPER]->have_min = 0;
	}
}

void history_range_append (HistoryRange *hr, NumericRange *nr)
{
	hr->occupied = hr->current + 1;  // cut off future values now that we are starting a new "branch"

	if (hr->occupied == M2_MAX_HISTORY)  // shift values one slot into the past, if full
	{
		for (int i = 0; i < M2_MAX_HISTORY - 1; i++)
			for sides { hr->history[s][i] = hr->history[s][i + 1]; }
	}
	else hr->occupied++;

	hr->current = hr->occupied - 1;
	for sides { hr->history[s][hr->current] = nr->value[s]; }
}

bool history_range_increment (HistoryRange *hr, NumericRange *nr, int delta)
{
	int previous = hr->current;
	if (hr->occupied > 0) hr->current = window_int(hr->current + delta, 0, hr->occupied - 1);

	if (hr->current != previous)
	{
		for sides { set_range(nr, s, hr->history[s][hr->current]); }
		return 1;
	}
	else return 0;
}

void set_range_vis (NumericRange *nr, bool show)
{
	if (nr->label != NULL) set_visibility(nr->label, show);
	set_visibility(nr->entry[LOWER]->widget, show);
	set_visibility(nr->entry[UPPER]->widget, show);
}

void entry_init (void)
{
	dirty_entry_widget = NULL;
}

bool entry_update_required (GdkEvent *event, GtkWidget *widget)
{
	bool yes = 0;
	if (event->type == GDK_KEY_PRESS)
	{
		GdkEventKey *keyevent = (GdkEventKey *) event;
		yes = (keyevent->keyval == 65293 || keyevent->keyval == 65421);
	}
	else yes = (event->type == GDK_FOCUS_CHANGE);

	dirty_entry_widget = yes ? NULL : widget;
	return yes;
}

bool cleanup_dirty_entry (void)
{
	if (dirty_entry_widget != NULL)
	{
		GdkEvent event;
		event.type = GDK_FOCUS_CHANGE;
		gboolean dummy;
		g_signal_emit_by_name(G_OBJECT(dirty_entry_widget), "focus-out-event", &event, &dummy);

		dirty_entry_widget = NULL;
		return 1;
	}
	else return 0;
}
