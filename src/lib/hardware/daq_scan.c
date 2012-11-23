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

int daq_SCAN_start (int id)
{
	// not prepared:  do nothing, return 0 (failure)
	// no driver:     do nothing, return 1 (success)
	// dummy:         do nothing, return 1

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD, DAQ_ID_WARNING_MSG,      return 0);
	f_verify(daq_board[id].is_connected,     DAQ_CONNECT_WARNING_MSG, return 0);

	DaqBoard *board = &daq_board[id];

	if (board->scan_prepared)
	{
		timer_reset(board->scan_timer);

		if (board->is_real)
		{
#if COMEDI
			board->scan_buffer = malloc((size_t) (board->scan_total * board->ai.b_sampl));
			return (board->scan_buffer != NULL && comedi_command(board->comedi_dev, &board->scan_cmd) == 0) ? 1 : 0;
#elif NIDAQ
			board->scan_buffer = malloc((size_t) board->scan_total * sizeof(i16));
			if (board->scan_buffer == NULL) return 0;

			SCAN_Setup(board->nidaq_num, (i16) board->scan_N_chan, board->scan_phys_chan, board->scan_phys_gain);
			DAQ_Config(board->nidaq_num, 0, 0);  // internal trigger and clock

			return (SCAN_Start(board->nidaq_num, (i16*) board->scan_buffer, (u32) board->scan_total,
		                       board->scan_tbcode, board->scan_sample_t, board->scan_tbcode, 0) == 0) ? 1 : 0;
#elif NIDAQMX
			mention_mx_error(DAQmxStopTask(board->multi_task));  // pause multi_task, which naturally conflicts with scan_task
			board->scan_buffer = malloc((size_t) board->scan_total * sizeof(float64));
			return (board->scan_buffer != NULL && mention_mx_error(DAQmxStartTask(board->scan_task)) == 0) ? 1 : 0;
#else
			return 1;
#endif
		}
		else
		{
			board->scan_buffer = malloc((size_t) board->scan_total * sizeof(double));
			return 1;
		}
	}
	else return 0;
}

void daq_SCAN_prepare (int id, Scan *userscan)
{
	// no driver:              do nothing,      return 0 (failure)
	// dummy:                  prepare scan,    return 1 (success)
	// rate needs adjustment:  update rate_kHz, return 4 (try again)

	userscan->status = 0;

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD, DAQ_ID_WARNING_MSG,      return);
	f_verify(daq_board[id].is_connected,     DAQ_CONNECT_WARNING_MSG, return);  

	// Note: Will always update userscan->N_pt, userscan->total_time.
	//       Will update userscan->read_interval if successful.

	DaqBoard *board = &daq_board[id];

	replace(board->scan_buffer, NULL);
	board->scan_prepared = 0;

	if (userscan->N_chan == 0) return;

	userscan->N_pt = (long) (userscan->total_time * userscan->rate_kHz * 1e3);    // compute for the first time
	userscan->total_time = (double) userscan->N_pt / (userscan->rate_kHz * 1e3);  // recompute due to possible rounding

	board->scan_N_chan = userscan->N_chan;
	board->scan_total  = userscan->N_chan * userscan->N_pt;
	board->scan_offset = 0;
	board->scan_saved  = 0;
	board->scan_total_time = userscan->total_time;

	for (int chan = 0; chan < M2_DAQ_MAX_CHAN; chan++) board->scan_pci[chan] = -1;
	for (int pci = 0; pci < userscan->N_chan; pci++)
	{
		if (userscan->phys_chan[pci] >= 0 && userscan->phys_chan[pci] < board->ai.N_ch) board->scan_pci[userscan->phys_chan[pci]] = pci;
		else
		{
			f_print(F_WARNING, "Warning: Phys_chan out of range.\n");
			return;
		}
	}

	if (board->is_real)
	{
#if COMEDI
		for (int pci = 0; pci < userscan->N_chan; pci++)
			board->scan_chanlist[pci] = CR_PACK((unsigned int) userscan->phys_chan[pci], board->ai.range, AREF_GROUND);

		board->scan_cmd.subdev         = (unsigned int) board->ai.num;
		board->scan_cmd.flags          = 0;
		board->scan_cmd.chanlist       = board->scan_chanlist;
		board->scan_cmd.chanlist_len   = (unsigned int) userscan->N_chan;
		board->scan_cmd.start_src      = TRIG_NOW;
		board->scan_cmd.start_arg      = 0;
		board->scan_cmd.scan_begin_src = TRIG_TIMER;
		board->scan_cmd.scan_begin_arg = (unsigned int) (1e9 / (userscan->rate_kHz * 1e3));
		board->scan_cmd.convert_src    = TRIG_TIMER;
		board->scan_cmd.convert_arg    = (unsigned int) (1e9 / userscan->N_chan / (userscan->rate_kHz * 1e3));
		board->scan_cmd.scan_end_src   = TRIG_COUNT;
		board->scan_cmd.scan_end_arg   = (unsigned int) userscan->N_chan;
		board->scan_cmd.stop_src       = TRIG_COUNT;
		board->scan_cmd.stop_arg       = (unsigned int) userscan->N_pt;

		int crv = comedi_command_test(board->comedi_dev, &board->scan_cmd);
		if (crv == 0)
		{
			userscan->read_interval = board->ai.buffer_size / (userscan->rate_kHz * 4e3 * (double) (userscan->N_chan * board->ai.b_sampl));  // 4e3 = safety factor * Hz/kHz
			// rate_kHz unchanged...
			board->scan_prepared = 1;
			userscan->status = 1;
		}
		else if (crv == 4)
		{
			userscan->rate_kHz = 1e9 / (userscan->N_chan * (int) board->scan_cmd.convert_arg * 1e3);
			// rate_kHz changed, user should try again...
			userscan->status = 4;
		}
		else
		{
			f_print(F_WARNING, "comedi_command_test() error: %d\n", crv);
			f_print(F_WARNING, "dumping cmd struct:\n");
			f_print(F_WARNING, "\tsubdev %d\n",        board->scan_cmd.subdev);
			f_print(F_WARNING, "\tchanlist_len %d\n",  board->scan_cmd.chanlist_len);
			f_print(F_WARNING, "\tstart %d %d\n",      board->scan_cmd.start_src,      board->scan_cmd.start_arg);
			f_print(F_WARNING, "\tscan_begin %d %d\n", board->scan_cmd.scan_begin_src, board->scan_cmd.scan_begin_arg);
			f_print(F_WARNING, "\tconvert %d %d\n",    board->scan_cmd.convert_src,    board->scan_cmd.convert_arg);
			f_print(F_WARNING, "\tscan_end %d %d\n",   board->scan_cmd.scan_end_src,   board->scan_cmd.scan_end_arg);
			f_print(F_WARNING, "\tstop %d %d\n",       board->scan_cmd.stop_src,       board->scan_cmd.stop_arg);
		}

#elif NIDAQ
		DAQ_Rate(userscan->rate_kHz * userscan->N_chan * 1e3, 0, &board->scan_tbcode, &board->scan_sample_t);
		double timebase = (board->scan_tbcode == -3) ? 0.05e-6 :  // 50  ns, 20  MHz
		                  (board->scan_tbcode == -2) ? 0.1e-6  :  // 100 ns, 10  MHz
		                  (board->scan_tbcode == 1)  ? 1e-6    :  // 1   us, 1   MHz
		                  (board->scan_tbcode == 2)  ? 10e-6   :  // 10  us, 100 kHz
		                  (board->scan_tbcode == 3)  ? 100e-6  :  // 100 us, 10  kHz
		                  (board->scan_tbcode == 4)  ? 1e-3    :  // 1   ms, 1   kHz
		                  (board->scan_tbcode == 5)  ? 10e-3   :  // 10  ms, 10  Hz
		                                               -1;        // error

		if (timebase > 0)
		{
			userscan->rate_kHz = 1 / (userscan->N_chan * timebase * board->scan_sample_t * 1e3);
			userscan->read_interval = 1.0;

			userscan->N_pt = (long) (userscan->total_time * userscan->rate_kHz * 1e3);    // compute for the first time
			userscan->total_time = (double) userscan->N_pt / (userscan->rate_kHz * 1e3);  // recompute due to possible rounding

			for (int pci = 0; pci < userscan->N_chan; pci++)
			{
				board->scan_phys_chan[pci] = (i16) userscan->phys_chan[pci];
				board->scan_phys_gain[pci] = NIDAQ_ADC_GAIN;
			}

			board->scan_prepared = 1;
			userscan->status = 1;
		}
		else status_add(0, supercat("Warning: DAQ_Rate() error on DAQ%d.\n", id));

#elif NIDAQMX
		mention_mx_error(DAQmxClearTask(board->scan_task));

		if (create_mx_scan(&board->scan_task, board->node, userscan->rate_kHz * 1e3, (uInt64) userscan->N_pt, userscan->N_chan, userscan->phys_chan))
		{
		    mention_mx_error(DAQmxCfgInputBuffer(board->scan_task, (uInt32) (userscan->rate_kHz * 4e3)));  // four seconds worth

			userscan->read_interval = 1.0;  // one second (safety factor of 4)
			// rate_kHz unchanged...

			board->scan_prepared = 1;
			userscan->status = 1;
		}
		else status_add(0, cat1("Warning: DAQmx setup error.\n"));
#endif
	}
	else 
	{
		for (int pci = 0; pci < userscan->N_chan; pci++)
		{
			board->scan_dummy_map[pci][0] = userscan->phys_chan[pci] / 3 + 1;
			board->scan_dummy_map[pci][1] = userscan->phys_chan[pci] % 3;
		}

		userscan->read_interval = 0.5;  // half second for smoother testing runs
		// rate_kHz unchanged...

		board->scan_prepared = 1;
		userscan->status = 1;
	}
}

double daq_SCAN_elapsed (int id)
{
	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD, DAQ_ID_WARNING_MSG, return 0);
	f_verify(daq_board[id].is_connected,     NULL,               return 0);

	return timer_elapsed(daq_board[id].scan_timer);
}

long daq_SCAN_read (int id)
{
	// no driver:  do nothing,      return 0
	// dummy:      update progress, return # of samples read

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD, DAQ_ID_WARNING_MSG, return 0);
	f_verify(daq_board[id].is_connected,     NULL,               return 0);

	DaqBoard *board = &daq_board[id];

	long s_read = 0;
	if (board->is_real)
	{
#if COMEDI
		ssize_t b_avail = comedi_get_buffer_contents(board->comedi_dev, (unsigned int) board->ai.num);
		ssize_t b_read_1 = 0, b_read_2 = 0;

		if (b_avail <= board->scan_total * board->ai.b_sampl)
		{
			if (board->ai.use_lsampl)
			{
				lsampl_t *buffer = board->scan_buffer;
				b_read_1 = read(comedi_fileno(board->comedi_dev), buffer + board->scan_offset, (size_t) b_avail);
				if (b_read_1 != b_avail)
					b_read_2 = read(comedi_fileno(board->comedi_dev), buffer + board->scan_offset + b_read_1 / board->ai.b_sampl, (size_t) (b_avail - b_read_1));
			}
			else
			{
				sampl_t *buffer = board->scan_buffer;
				b_read_1 = read(comedi_fileno(board->comedi_dev), buffer + board->scan_offset, (size_t) b_avail);
				if (b_read_1 != b_avail)
					b_read_2 = read(comedi_fileno(board->comedi_dev), buffer + board->scan_offset + b_read_1 / board->ai.b_sampl, (size_t) (b_avail - b_read_1));
			}
		}
		else f_print(F_WARNING, "Warning: Device buffer has more bytes than expected.\n");

		s_read = (b_read_1 + b_read_2) / board->ai.b_sampl;  // # of samples read

		f_print(F_RUN, "Info: Read (%d+%d)/%d bytes.\n", b_read_1, b_read_2, b_avail);

#elif NIDAQ
		i16 stopped;
		u32 ret;
		DAQ_Check(board->nidaq_num, &stopped, &ret);  // Note: Just check on progress, since NI-DAQ writes directly to the buffer.

		if (ret <= (u32) board->scan_total) s_read = (ssize_t) ret - board->scan_offset;
		else f_print(F_WARNING, "Warning: Device buffer has more samples than expected.\n");

		f_print(F_RUN, "Info: Read %ld samples.\n", s_read);

#elif NIDAQMX
		int32 spc_read;
		float64 *buffer = board->scan_buffer;
		mention_mx_error(DAQmxReadAnalogF64(daq_board[id].scan_task, -1, -1, DAQmx_Val_GroupByScanNumber, buffer + board->scan_offset, (uInt32) board->scan_total, &spc_read, NULL));
		s_read = spc_read * board->scan_N_chan;

		f_print(F_RUN, "Info: Read %ld/%ld samples.\n", s_read, s_read);
#endif
	}
	else
	{
		long ret = min_long((long) ((double) board->scan_total * timer_elapsed(board->scan_timer) / board->scan_total_time), board->scan_total);

		double *buffer = board->scan_buffer;
		for (long j = board->scan_offset; j < ret; j++)
		{
			int pci = (int) (j % board->scan_N_chan);
			double wt = (double) j / (double) board->scan_total * board->scan_total_time * board->scan_dummy_map[pci][0];
			switch (board->scan_dummy_map[pci][1])
			{
				case 0  : buffer[j] = sin(2 * U_PI * wt);               break;
				case 1  : buffer[j] = (wt - floor(wt) < 0.5) ? 1 : -1;  break;
				default : buffer[j] = 2 * (wt - floor(wt)) - 1;
			}
			s_read++;
		}
	}

	board->scan_offset += s_read;
	board->scan_saved = board->scan_offset / board->scan_N_chan;  // round down
	return s_read;
}

long daq_SCAN_stop (int id)
{
	// no driver:  do nothing,    return 0
	// dummy:      work normally, return # of samples read

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD, DAQ_ID_WARNING_MSG, return 0);
	f_verify(daq_board[id].is_connected,     NULL,               return 0);

	DaqBoard *board = &daq_board[id];

	long s_read = daq_SCAN_read(id);  // attempt to grab last of the data
	for (int i = 0; i < 8 && board->scan_offset != board->scan_total; i++)
	{
		f_print(F_WARNING, "Warning: Waiting to obtain the last few(?) samples.\n");
		xleep(M2_DAQ_EXTRA_SCAN_TIME / 8);
		s_read += daq_SCAN_read(id);  // attempt to grab last of the data, again
	}

	if (board->is_real)
	{
#if COMEDI
		comedi_cancel(board->comedi_dev, (unsigned int) board->ai.num);
#elif NIDAQ
		DAQ_Clear(board->nidaq_num);
#elif NIDAQMX
		mention_mx_error(DAQmxClearTask(board->scan_task));
		mention_mx_error(DAQmxStartTask(board->multi_task));  // restart multi_task so daq_multi_tick() can proceed as normal
#endif
	}

	return s_read;
}

int daq_AI_convert (int id, int chan, long pt, double *voltage)
{
	// absent chan:  do nothing,                 return 0 (failure)
	// no driver:    do nothing,                 return 1 (success)
	// dummy:        set voltage to a sine wave, return 1

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD,            DAQ_ID_WARNING_MSG, return 0);
	f_verify(daq_board[id].is_connected,                NULL,               return 2);  // (unknown)
	f_verify(chan >= 0 && chan < daq_board[id].ai.N_ch, NULL,               return 0);

	DaqBoard *board = &daq_board[id];

	int pci = board->scan_pci[chan];
	if (pci < 0) return 0;

	if (pt < board->scan_saved)  // implies that buffer subscript is OK (< daq_board[id].scan_offset)
	{
		if (board->is_real)
		{
#if COMEDI
			if (board->ai.use_lsampl)
			{
				lsampl_t *buffer = board->scan_buffer;
				*voltage = comedi_to_phys(buffer[pci + board->scan_N_chan * pt], board->ai.ch[chan].crange, board->ai.ch[chan].maxdata);
			}
			else
			{
				sampl_t *buffer = board->scan_buffer;
				*voltage = comedi_to_phys(buffer[pci + board->scan_N_chan * pt], board->ai.ch[chan].crange, board->ai.ch[chan].maxdata);
			}
#elif NIDAQ
			i16 *buffer = board->scan_buffer;
			AI_VScale(daq_board[id].nidaq_num, (i16) chan, NIDAQ_ADC_GAIN, NIDAQ_ADC_GAIN_ADJUST, NIDAQ_ADC_OFFSET, buffer[pci + board->scan_N_chan * pt], voltage);
#elif NIDAQMX
			float64 *buffer = board->scan_buffer;
			*voltage = buffer[pci + board->scan_N_chan * pt];  // already scaled
#endif
		}
		else
		{
			double *buffer = board->scan_buffer;
			*voltage = buffer[pci + board->scan_N_chan * pt];
		}
	}
	else *voltage = 0;  // for interrupted scans

	return 1;
}

int daq_AO_convert (int id, int chan, long pt, double *voltage)
{
	// no driver:  work normally, return ?
	// dummy:      work normally, return ?

	f_verify(id >= 0 && id < M2_DAQ_MAX_BRD,            DAQ_ID_WARNING_MSG, return 0);  // (failure)
	f_verify(daq_board[id].is_connected,                NULL,               return 2);  // (unknown)
	f_verify(chan >= 0 && chan < daq_board[id].ao.N_ch, NULL,               return 0);

	*voltage = (pt < daq_board[id].scan_saved) ? daq_board[id].ao.ch[chan].voltage : 0.0;
	return daq_board[id].ao.ch[chan].known;
}
