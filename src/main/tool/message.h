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

#ifndef _MAIN_TOOL_MESSAGE_H
#define _MAIN_TOOL_MESSAGE_H 1

#include <main/section.h>

typedef struct
{
	// private:

		Section sect;
		GtkWidget *mini_entry, *mini_spacer;
		GtkTextBuffer *textbuf;

		bool minimode;

	// public:

		GtkWidget *scroll;

} Message;

void message_init     (Message *message, GtkWidget **apt);
void message_register (Message *message, int pid, GtkWidget **apt);
void message_update   (Message *message);
void message_final    (Message *message);

#endif
