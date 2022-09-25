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

#ifndef _CONFIG_H
#define _CONFIG_H 1

#ifdef MINGW
#define M2_F_MODE_DEFAULT 0x0
#define M2_F_MODE_DEFAULT_STR "(none)"
#else
#define M2_F_MODE_DEFAULT 0x83
#define M2_F_MODE_DEFAULT_STR "error,warning,control"
#endif

// panel:
#define M2_NUM_DAQ 3
#define M2_VDAQ_ID 2
#define M2_NUM_GPIB 3
#define M2_VGPIB_ID 2
#define M2_NUM_HW 6  // M2_NUM_DAQ + M2_NUM_GPIB
#define M2_NUM_PANEL 2
#define M2_MAX_CHAN 16
#define M2_MAX_TRIG 8
#define M2_MAX_CBUF_LENGTH 64
#define M2_TRIGGER_LINES 9  // 1 "if" plus 8 "then"
#define M2_STATUS_MAX_MSG 768
#define M2_COMPUTE_EPSILON 1e-24
#define M2_OLD_NUM_CHAN 10
#define M2_OLD_NUM_SWEEP 2
#define M2_OLD_NUM_TRIG 2
#define M2_DEFAULT_HEIGHT 500
#define M2_MESSAGE_WIDTH  150
#define M2_MESSAGE_HEIGHT 80
#define M2_HARDWARE_WIDTH 100
#define M2_HALFSPACE 2
#define M2_CHANNEL_FORMAT "<span font_stretch=\"semicondensed\">%s</span>"
#define M2_HEADING_FORMAT "<span font_weight=\"bold\" font_stretch=\"semicondensed\">%s</span>"
#define M2_URL "brianstandley.com/mezurit2"

// threads:
#define M2_SLEEP_MULT 15
#define M2_TERMINAL_POLLING_RATE 400  // Hz
#define M2_DEFAULT_GUI_RATE 120       // Hz
#define M2_BOOST_GUI_RATE 480         // Hz
#define M2_MAX_READER_RATE 73         // Hz
#define M2_MAX_BUFFER_STATUS_RATE 41  // Hz
#define M2_SCOPE_PROGRESS_RATE 11     // Hz
#define M2_MAX_GRADUAL_PTS 800
#define M2_BOOST_THRESHOLD_PTS 100

// plotting:
#define M2_XBORDER 24
#define M2_YBORDER 20
#define M2_XLABEL_OFFSET 12
#define M2_YLABEL_OFFSET 16
#define M2_XTL_OFFSET 8
#define M2_YTL_OFFSET 5
#define M2_WIN32_DOUBLECLICK_TIME 0.4

#define M2_TICK_LENGTH_LARGE 10
#define M2_TICK_LABEL_PAD 5
#define M2_TICK_LABEL_SIZE 14
#define M2_FONT_SIZE 14
#define M2_BUFFER_SYMBOL_WIDTH 18
#define M2_SCOPE_PROGRESS_WIDTH 100
#define M2_MINMAX_LABEL_BORDER 1.5
#define M2_MAX_CAIRO_PTS 4000  // should be under 10000 to avoid Cairo crashes
#define M2_PLOT_POINT_WIDTH 2
#define M2_PLOT_POINT_HALFWIDTH 1
#define M2_PLOT_LINE_MIN_DISPLACEMENT 0.5
#define M2_PLOT_POINT_MIN_DISPLACEMENT 1.0
#define M2_MARKER_LENGTH 6
#define M2_MARKER_ARROW_SIZE 10
#define M2_MARKER_ARROW_OFFSET 4
#define M2_TICK_LENGTH_SMALL 4
#define M2_TICK_LENGTH_MED 7
#define M2_NUM_COLOR 7
#define M2_MAX_HISTORY 32

// control server:
#define M2_CONTROL_MAX_SERVER 2
#define M2_CONTROL_MAX_ARG 255

// hardware:
#define M2_DAQ_MAX_BRD  6
#define M2_DAQ_MAX_CHAN 16
#define M2_DAQ_MAX_SETTLE 50  // extra samples for NIDAQ, units of 10µs otherwise
#define M2_DAQ_VIRTUAL_NUM_DAC 4
#define M2_DAQ_VIRTUAL_NUM_ADC 16
#define M2_DAQ_VIRTUAL_MIN -1e18
#define M2_DAQ_VIRTUAL_MAX  1e18
#define M2_DAQ_COMEDI_BUFFER_SIZE (64*1024)
#define M2_DAQ_EXTRA_SCAN_TIME 800e-3
#define M2_GPIB_MAX_BRD 6
#define M2_GPIB_MAX_PAD 32
#define M2_GPIB_BUF_LENGTH 255

// libs:
#define M2_MCF_LINE_LENGTH 1024
#define M2_BLOB_MAX_PTR 3
#define M2_BLOB_MAX_NUM 2

#endif
