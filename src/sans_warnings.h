/*
 *  Copyright (C) 2012 Brian Standley <brian@brianstandley.com>
 *
 *  This file is part of Mezurit2, created by Brian Standley.
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

#ifndef SHOW_HEADER_WARNINGS
#pragma GCC system_header
#endif

#ifndef HEADER_SANS_WARNINGS
#pragma message "Must define HEADER_SANS_WARNINGS before including this file."
#else
#include HEADER_SANS_WARNINGS
#undef HEADER_SANS_WARNINGS
#endif
