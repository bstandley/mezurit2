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

void oldvars_reset (OldVars *oldvars)
{
	for (int vc = 0; vc < M2_OLD_NUM_CHAN; vc++)
	{
		oldvars->ch_type[vc] = 0;  // none
		oldvars->ch_factor[vc] = 1.0;
		oldvars->ch_offset[vc] = 0.0;
	}

	for (int n = 0; n < M2_OLD_NUM_TRIG; n++)
	{
		oldvars->trig_vc[n] = -1;  // not found
		oldvars->trig_mode[n] = 0;  // rising
		oldvars->trig_level[n] = 1.0;
		oldvars->trig_action[n] = 0;  // none
	}

	oldvars->oldtrig_vc = -1;  // none
	oldvars->oldtrig_mode = 0;  // rising
	oldvars->oldtrig_level = 1.0;
}

void oldvars_register (OldVars *oldvars)
{
	f_start(F_INIT);

	for (int vc = 0; vc < M2_OLD_NUM_CHAN; vc++)
	{
		int type_var   = mcf_register(&oldvars->ch_type[vc],   atg(supercat("ch%d_type",   vc)), MCF_INT);
		int factor_var = mcf_register(&oldvars->ch_factor[vc], atg(supercat("ch%d_factor", vc)), MCF_DOUBLE);
		int offset_var = mcf_register(&oldvars->ch_offset[vc], atg(supercat("ch%d_offset", vc)), MCF_DOUBLE);

		mcf_connect(type_var,   "setup", BLOB_CALLBACK(set_int_mcf),    0x00);
		mcf_connect(factor_var, "setup", BLOB_CALLBACK(set_double_mcf), 0x00);
		mcf_connect(offset_var, "setup", BLOB_CALLBACK(set_double_mcf), 0x00);
	}

	for (int n = 0; n < M2_OLD_NUM_TRIG; n++)
	{
		int vc_var     = mcf_register(&oldvars->trig_vc[n],     atg(supercat("trigger%d_chan",   n)), MCF_INT);
		int mode_var   = mcf_register(&oldvars->trig_mode[n],   atg(supercat("trigger%d_mode",   n)), MCF_INT);
		int level_var  = mcf_register(&oldvars->trig_level[n],  atg(supercat("trigger%d_level",  n)), MCF_DOUBLE);
		int action_var = mcf_register(&oldvars->trig_action[n], atg(supercat("trigger%d_action", n)), MCF_INT);

		mcf_connect(vc_var,     "setup", BLOB_CALLBACK(set_int_mcf),    0x00);
		mcf_connect(mode_var,   "setup", BLOB_CALLBACK(set_int_mcf),    0x00);
		mcf_connect(level_var,  "setup", BLOB_CALLBACK(set_double_mcf), 0x00);
		mcf_connect(action_var, "setup", BLOB_CALLBACK(set_int_mcf),    0x00);
	}

	int old_vc_var    = mcf_register(&oldvars->oldtrig_vc,    "trigger_channel", MCF_INT);
	int old_mode_var  = mcf_register(&oldvars->oldtrig_mode,  "trigger_mode",    MCF_INT);
	int old_level_var = mcf_register(&oldvars->oldtrig_level, "trigger_level",   MCF_DOUBLE);

	mcf_connect(old_vc_var,    "setup", BLOB_CALLBACK(set_int_mcf),    0x00);
	mcf_connect(old_mode_var,  "setup", BLOB_CALLBACK(set_int_mcf),    0x00);
	mcf_connect(old_level_var, "setup", BLOB_CALLBACK(set_double_mcf), 0x00);
}

void oldvars_mention (OldVars *oldvars)
{
	f_start(F_UPDATE);

	for (int vc = 0; vc < M2_OLD_NUM_CHAN; vc++)
	{
		char *suggestion _strfree_ = NULL;
		switch (oldvars->ch_type[vc])
		{
			case  1 : suggestion = supercat("DAC(0, 0) * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			case  2 : suggestion = supercat("DAC(0, 1) * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			case  3 : suggestion = supercat("ADC(0, 0) * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			case  4 : suggestion = supercat("ADC(0, 1) * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			case  5 : suggestion = supercat("ADC(0, 2) * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			case  6 : suggestion = supercat("ADC(0, 3) * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			case  7 : suggestion = supercat("ADC(0, 4) * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			case  8 : suggestion = supercat("ADC(0, 5) * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			case  9 : suggestion = supercat("ADC(0, 6) * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			case 10 : suggestion = supercat("ADC(0, 7) * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			case 11 : suggestion = supercat(   "time() * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			case 12 : suggestion = supercat(       "?? * %f + %f", oldvars->ch_factor[vc], oldvars->ch_offset[vc]); break;
			default :                                                                                               break;
		}

		if (suggestion != NULL)
		{
			status_add(0, cat1("Warning! Found obsolete channel setup.\n"));
			status_add(0, supercat("\tSuggested expression for X%d: \"%s\"\n", vc, suggestion));
		}
	}

	for (int n = 0; n < M2_OLD_NUM_TRIG; n++) if (oldvars->trig_vc[n] != -1)
	{
		status_add(0, cat1("Warning! Found obsolete trigger setup:\n"));

		switch (oldvars->trig_mode[n])
		{
			case 0  : status_add(0, supercat("\tSuggested IF: \"ch(%d) > %f\"\n", oldvars->trig_vc[n], oldvars->trig_level[n]));         break;
			case 1  : status_add(0, supercat("\tSuggested IF: \"ch(%d) < %f\"\n", oldvars->trig_vc[n], oldvars->trig_level[n]));         break;
			case 2  : status_add(0, supercat("\tSuggested THEN: \"set_dac(%d, %f)\" or, if scanning, \"request_pulse(%d, %f)\"\n",
			                                 oldvars->trig_vc[n], oldvars->trig_level[n], oldvars->trig_vc[n], oldvars->trig_level[n])); break;
			default :                                                                                                                    break;
		}

		switch (oldvars->trig_action[n])
		{
			case 1  : status_add(0, cat1(    "\tSuggested THEN: \"set_recording(1)\"\n"));         break;
			case 2  : status_add(0, cat1(    "\tSuggested THEN: \"fire_scope()\"\n"));             break;
			case 3  : status_add(0, supercat("\tSuggested THEN: \"arm_trigger(%d)\"\n",   n + 1)); break;
			case 4  : status_add(0, supercat("\tSuggested THEN: \"force_trigger(%d)\"\n", n + 1)); break;
			default :                                                                              break;
		}
	}

	if (oldvars->oldtrig_vc != -1)
	{
		status_add(0, cat1("Warning! Found obsolete trigger setup:\n"));

		switch (oldvars->oldtrig_mode)
		{
			case 0  : status_add(0, supercat("\tSuggested IF: \"ch(%d) > %f\"\n", oldvars->oldtrig_vc, oldvars->oldtrig_level));
			          status_add(0, cat1(    "\tSuggested THEN: \"fire_scope()\"\n"));                                                        break;
			case 1  : status_add(0, supercat("\tSuggested IF: \"ch(%d) < %f\"\n", oldvars->oldtrig_vc, oldvars->oldtrig_level));
			          status_add(0, cat1(    "\tSuggested THEN: \"fire_scope()\"\n"));                                                        break;
			case 2  : status_add(0, supercat("\tSuggested THEN: \"fire_scope_pulse(%d, %f)\"", oldvars->oldtrig_vc, oldvars->oldtrig_level)); break;
			default :                                                                                                                         break;
		}
	}
}
