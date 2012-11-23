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

#ifndef _MAIN_SETUP_CHANNEL_H
#define _MAIN_SETUP_CHANNEL_H 1

#include <config.h>
#include <lib/entry.h>
#include <lib/varset/varset.h>
#include <main/setup/compute.h>
#include <main/section.h>

typedef struct
{
	// private:

		char *name, *unit, *expr;  // threads: GUI only, parsed by build_chanset()
		int prefix;                // threads: GUI only
		bool save;                 // threads: GUI only, applied to Varset before daq_thread_starts

		GtkWidget *name_entry, *unit_entry, *expr_entry;
		GtkWidget *label, *prefix_combo, *save_widget;
		NumericEntry *binsize_entry;
		int block_prefix_cb;

		// The following sub-channel vars are written in a CSF which runs in
		// the DAQ thread, but read in a CSF which runs in the GUI thread.
		// As they are only accessed by CSFs, they are implicitly protected
		// by the control server's internal mutex. The actual configuration
		// is stored inside the ComputeFunction, so these are not used during
		// normal acquisition and sweeping.

		int sub_vc;
		char *sub_expr;

	// public:

		char *full_unit, *type, *desc, *desc_long;  // GUI only

		double binsize;  // threads: shared, but copied before daq_thread starts
		ComputeFunc cf;  // threads: shared, but updated before daq_thread starts

} Channel;

typedef struct
{
	// private:

		int scope_ai_count[M2_DAQ_MAX_BRD][M2_DAQ_MAX_CHAN];  // Use reduce_chanset() to determine the active physical
		                                                      // channels on a specific physical board. ADCs which
		                                                      // are mentioned only as part of a non-scannable (ADCs
		                                                      // on different boards) channels are not included.

	// public:

		int N_total_chan, N_inv_chan, N_gpib;

		int vci_by_vc [M2_MAX_CHAN];  // used by ch_cfunc() and set_dac_csf()
		int ici_by_vc [M2_MAX_CHAN];  // used by sweep callbacks

		int           vc_by_vci [M2_MAX_CHAN];  // used to access channel_array[], or to build strings like "X_(vc}"
		Channel *channel_by_vci [M2_MAX_CHAN];  // shortcut pointers, i.e. &channel_array[vc_by_vci[vci]]

		int vci_by_ici [M2_MAX_CHAN];  // used by sweep_array_update()

} ChanSet;

void channel_array_init     (Channel *channel_array, Section *sect, GtkWidget **apt);
void channel_array_register (Channel *channel_array, Section *sect, GtkWidget **apt);
void channel_array_final    (Channel *channel_array);

void build_chanset  (ChanSet *chanset, Channel *channel_array);
int  reduce_chanset (ChanSet *chanset, int brd_id, int *scan_ai_chan);  // returns N_chan
VSP  prepare_vset   (ChanSet *chanset);

/*

Index                            Definition                              Domain
---------------------------------------------------------------------------------------------------------------
vc  = channel index              line # on the setup page i.e. X_(vc)    0 to (M2_MAX_CHAN - 1)

vci = virtual channel index      column # in a varset holding the data   0 to (N_total_chan - 1), or -1 for N/A
                                 (includes non-saved channels)

ici = invertible channel index   index of available DAC/GPIB channels    0 to (N_inv_chan - 1), or -1 for N/A
                                 which are linear functions of a
                                 single compute function

brd_id = board index             the id chosen when calling              0 to M2_NUM_DAQ or M2_NUM_GPIB
                                 daq_board_connect(), for example

chan = DAC or ADC port number    physical port # on the breakout box     0 to ?? (depends on the device)

EXAMPLE:

Symbol  Expr               vc  vci_by_vc[]  ici_by_vc[]
------------------------------------------------------
X0      time()             0   0            -1
X1      DAC(0,0)           1   1            0
X2                         2   -1           -1
X3      ADC(0,2)           3   2            -1
X4      ADC(0,3)-ADC(1,3)  4   3            -1
X5      ADC(1,3)           5   4            -1
X6      ADC(1,4)           6   5            -1
X7                         7   -1           -1
X8      A8648_Freq(0,18)   8   6            1
------------------------------------------------------

vci  vc_by_vci[]  channel_by_vci[]      ici  vci_by_ici
-----------------------------------     ---------------
0    0            &panel.channel[0]     0    1
1    1            &panel.channel[1]     1    6
2    3            &panel.channel[3]     ---------------
3    4            &panel.channel[4]     N_inv_chan = 2
4    5            &panel.channel[5]
5    6            &panel.channel[6]
6    8            &panel.channel[8]
-----------------------------------
N_total_chan = 7

                          0   1   2   < brd_id
                        -------------
                      0 | 0   0   0 |
scope_ai_count[][]:   1 | 0   0   0 |
                      2 | 1   0   0 |
                      3 | 0   1   0 |
                      4 | 0   1   0 |
                        -------------
                 chan ^

Note: scope_ai_count[0,3] == 0 because X4 *is not* scannable.
      scope_ai_count[1,3] == 1 because X5 *is* scannable.

int array[5];
reduce_chanset(chanset, 0, array) == 1, array: [2, ?, ?, ?, ?]
reduce_chanset(chanset, 1, array) == 2, array: [3, 4, ?, ?, ?]
reduce_chanset(chanset, 2, array) == 0, array: [?, ?, ?, ?, ?]

*/

#endif
