//##########################################################################
//#                                                                        #
//#                            CLOUDCOMPARE                                #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
//#                                                                        #
//##########################################################################

#include "ccTimer.h"

//System
#include <assert.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdio.h>

static uint64_t _start_tick_count = 0;

static uint64_t get_tick_count()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

void ccTimer::Init()
{
	if (_start_tick_count == 0) {
		_start_tick_count = get_tick_count();
	}
}

int ccTimer::Sec()
{
	assert(_start_tick_count != 0);
	return (_start_tick_count ? (get_tick_count() - _start_tick_count) / 1000 : 0);
}

int ccTimer::Msec()
{
	assert(_start_tick_count != 0);
	return (_start_tick_count ? (get_tick_count() - _start_tick_count): 0);
}
