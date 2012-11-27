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

#ifndef _MAIN_PANEL_H
#define _MAIN_PANEL_H 1

#include <control/server.h>
#include <main/setup/hardware.h>
#include <main/tool/message.h>
#include <main/tool/logger.h>
#include <main/tool/sweep.h>
#include <main/tool/scope.h>
#include <main/tool/trigger.h>
#include <main/tool/buffer.h>
#include <main/plot/plot.h>

typedef struct
{
	// private:

		GtkWidget *apt[M2_NUM_APTS];
		GtkWidget *terminal_scroll;
		Section ch_sect;

	// public:

		Message message;
		Hardware hw[M2_NUM_HW];  // layout: [DAQ 0, DAQ 1, VDAQ, GPIB 0, GPIB 1, VGPIB] (assuming M2_NUM_DAQ = 3, M2_NUM_GPIB = 3)
		Channel channel[M2_MAX_CHAN];

} Setup;

typedef struct
{
	// public:

		int pid;
		MtMutex sweep_mutex, trigger_mutex;

		GtkWidget *apt[M2_NUM_APTS];
		GtkWidget *terminal_scroll;
		Section trigger_sect;  // shared between triggers

		Message message;
		Logger logger;
		Scope scope;
		Sweep sweep[M2_MAX_CHAN];
		Trigger trigger[M2_MAX_TRIG];
		Plot plot;
		Buffer buffer;

} Panel;

typedef struct
{
	int    ch_type   [M2_OLD_NUM_CHAN];
	double ch_factor [M2_OLD_NUM_CHAN];
	double ch_offset [M2_OLD_NUM_CHAN];

	int    trig_vc     [M2_OLD_NUM_TRIG];
	int    trig_mode   [M2_OLD_NUM_TRIG];
	double trig_level  [M2_OLD_NUM_TRIG];
	int    trig_action [M2_OLD_NUM_TRIG];

	int oldtrig_vc, oldtrig_mode;
	double oldtrig_level;

} OldVars;

void setup_init     (Setup *setup, GtkWidget *flipbook);
void setup_register (Setup *setup);

void panel_init     (Panel *panel, int pid, GtkWidget *flipbook);
void panel_register (Panel *panel, ChanSet *chanset);
void panel_final    (Panel *panel);

void panel_register_legacy (Panel *panel);

void oldvars_reset    (OldVars *oldvars);
void oldvars_register (OldVars *oldvars);
void oldvars_mention  (OldVars *oldvars);

#endif
