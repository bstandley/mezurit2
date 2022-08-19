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

#ifndef _LIB_ENTRY_H
#define _LIB_ENTRY_H 1

#include <stdbool.h>
#include <gtk/gtk.h>

#include <config.h>

enum
{
	LOWER = 0,
	UPPER = 1
};

#define sides (int s = 0; s < 2; s++)

typedef struct
{
	char *unit;
	int min_digits, max_digits;

	bool want_min, want_max;
	bool have_min, have_max;
	double min, max;

	GtkWidget *widget;

} NumericEntry;

typedef struct
{
	double value[2];
	bool valid[2];
	NumericEntry *entry[2];
	GtkWidget *label;

} NumericRange;

typedef struct
{
	double history[2][M2_MAX_HISTORY];
	int occupied, current;

} HistoryRange;

NumericEntry * new_numeric_entry (int min_digits, int max_digits, int width);
void destroy_entry (NumericEntry *ne);

void set_entry_unit (NumericEntry *ne, const char *unit);
void set_entry_min  (NumericEntry *ne, double value);
void set_entry_max  (NumericEntry *ne, double value);

double check_entry (NumericEntry *ne, double value);
double read_entry  (NumericEntry *ne, bool *ok);
void   write_entry (NumericEntry *ne, double value);
void   write_blank (NumericEntry *ne);

void set_range        (NumericRange *nr, int side, double value);
void write_range      (NumericRange *nr);
bool rectify_range    (NumericRange *nr);
void invalidate_range (NumericRange *nr, int side);
void set_range_vis    (NumericRange *nr, bool show);

void history_range_append    (HistoryRange *hr, NumericRange *nr);
bool history_range_increment (HistoryRange *hr, NumericRange *nr, int delta);

void entry_init (void);
bool cleanup_dirty_entry (void);
bool entry_update_required (GdkEvent *event, GtkWidget *widget);

#endif
