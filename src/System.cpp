/*************************************************************************
** System.cpp                                                           **
**                                                                      **
** This file is part of dvisvgm -- the DVI to SVG converter             **
** Copyright (C) 2005-2013 Martin Gieseking <martin.gieseking@uos.de>   **
**                                                                      **
** This program is free software; you can redistribute it and/or        **
** modify it under the terms of the GNU General Public License as       **
** published by the Free Software Foundation; either version 3 of       **
** the License, or (at your option) any later version.                  **
**                                                                      **
** This program is distributed in the hope that it will be useful, but  **
** WITHOUT ANY WARRANTY; without even the implied warranty of           **
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the         **
** GNU General Public License for more details.                         **
**                                                                      **
** You should have received a copy of the GNU General Public License    **
** along with this program; if not, see <http://www.gnu.org/licenses/>. **
*************************************************************************/

#include <ctime>
#include "System.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined (HAVE_SYS_TIME_H)
#include <sys/time.h>
#elif defined (HAVE_SYS_TIMEB_H)
#include <sys/timeb.h>
#endif


using namespace std;


/** Returns timestamp (wall time) in seconds. */
double System::time () {
#if defined (HAVE_SYS_TIME_H)
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec/1000000.0;
#elif defined (HAVE_SYS_TIMEB_H)
	struct timeb tb;
	ftime(&tb);
	return tb.time + tb.millitm/1000.0;
#else
	clock_t myclock = clock();
	return double(myclock)/CLOCKS_PER_SEC;
#endif
}

