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

#ifndef _LIB_HARDWARE_DAQ_H
#define _LIB_HARDWARE_DAQ_H 1

#include <config.h>

typedef struct
{
	// input:
	int N_chan, phys_chan[M2_DAQ_MAX_CHAN];

	// input/output:
	double total_time;
	double rate_kHz;

	// output:
	int status;
	long N_pt;
	double read_interval;

} Scan;

// configuration

void daq_init  (void);
void daq_final (void);

int    daq_board_connect (int id, const char *node);
char * daq_board_info    (int id, const char *info);

int daq_AO_valid (int id, int chan);
int daq_AI_valid (int id, int chan);

// point I/O

int daq_AI_read  (int id, int chan, double *voltage);  // if unknown, add to multi setup
int daq_AO_read  (int id, int chan, double *voltage);
int daq_AO_write (int id, int chan, double  voltage);

int  daq_multi_tick  (int id);
void daq_multi_reset (int id);

// scans

void   daq_SCAN_prepare (int id, Scan *userscan);
int    daq_SCAN_start   (int id);
double daq_SCAN_elapsed (int id);
long   daq_SCAN_read    (int id);
long   daq_SCAN_stop    (int id);

int daq_AI_convert (int id, int chan, long pt, double *voltage);
int daq_AO_convert (int id, int chan, long pt, double *voltage);

#endif
