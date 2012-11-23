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

static char * record_csf (gchar **argv, ThreadVars *tv);
static char * scan_csf (gchar **argv, ThreadVars *tv);
static char * emit_signal_csf (gchar **argv, ThreadVars *tv);
static char * request_pulse_csf (gchar **argv, ThreadVars *tv);

char * scan_csf (gchar **argv, ThreadVars *tv)
{
	f_start(F_CONTROL);

	bool on;
	if (scan_arg_bool(argv[1], "on", &on))
	{
		bool run = set_scanning(tv, on ? SCOPE_RL_SCAN : SCOPE_RL_READY);
		if (run) set_scan_callback_mode(tv, 1);

		return cat1(argv[0]);
	}
	else return cat1("argument_error");
}

char * record_csf (gchar **argv, ThreadVars *tv)
{
	f_start(F_CONTROL);

	bool on;
	if (scan_arg_bool(argv[1], "on", &on))
	{
		set_recording(tv, on ? LOGGER_RL_RECORD : LOGGER_RL_IDLE);
		return cat1(argv[0]);
	}
	else return cat1("argument_error");
}

char * emit_signal_csf (gchar **argv, ThreadVars *tv)  // connected to only "local" control server (M2_LS_ID)
{
	f_start(F_CONTROL);

	char *name _strfree_ = NULL;
	if (scan_arg_string(argv[1], "name", &name))
	{
		bool matched = 0;

		control_server_lock(M2_TS_ID);
		if (str_equal(tv->catch_signal, name))
		{
			char *reply _strfree_ = cat3(get_cmd(M2_TS_ID), ";name|", name);
			control_server_reply(M2_TS_ID, reply);
			replace(tv->catch_signal, NULL);
			matched = 1;
		}
		control_server_unlock(M2_TS_ID);

		return supercat("%s;matched|%d", argv[0], matched ? 1 : 0);
	}

	return cat1("argument_error");
}

char * request_pulse_csf (gchar **argv, ThreadVars *tv)
{
	f_start(F_CONTROL);

	int vc;
	if (scan_arg_int(argv[1], "channel", &vc) && vc >= -1 && vc < M2_MAX_CHAN)
	{
		if (vc == -1)
		{
			tv->pulse_vci = -1;
			return cat1(argv[0]);
		}
		else
		{
			int vci = tv->chanset->vci_by_vc[vc];
			if (vci != -1)
			{
				double target;
				if (scan_arg_double(argv[2], "target", &target))
				{
					double current;
					if (compute_function_read(&tv->chanset->channel_by_vci[vci]->cf, COMPUTE_MODE_POINT, &current))
					{
						tv->pulse_vci = vci;
						tv->pulse_target = target;
						return cat1(argv[0]);
					}
					else status_add(1, cat1("Warning: Will not set pulse request from unknown current value.\n"));
				}
			}
		}
	}

	return cat1("argument_error");
}
