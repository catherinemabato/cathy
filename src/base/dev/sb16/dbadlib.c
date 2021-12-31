/*
 *  Copyright (C) 2002-2011  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* This file contains mostly converted and stripped down code from
   DOSBOX src/hardware/adlib.cpp, hence DOSBOX copyright applies.
   It contains code that glues dbopl.c to DOSEMU adlib.c.
   The only code really added for DOSEMU is the mono->stereo conversion.
*/

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include "emu.h"
#include "timers.h"
#include "opl.h"
#include "sound/oplplug.h"
#include "dbadlib.h"

typedef struct _AdlibTimer AdlibTimer;

struct _AdlibTimer {
	long long start;
	long long delay;
	bool enabled, overflow, masked;
	Bit8u counter;
};

static void AdlibTimer__AdlibTimer(AdlibTimer *self) {
	self->masked = false;
	self->overflow = false;
	self->enabled = false;
	self->counter = 0;
	self->delay = 0;
}

//Call update before making any further changes
static void AdlibTimer__Update(AdlibTimer *self, long long time) {
	long long deltaStart;
	if ( !self->enabled ) 
		return;
	deltaStart = time - self->start;
	//Only set the overflow flag when not masked
	if ( deltaStart >= 0 && !self->masked ) {
		self->overflow = 1;
	}
}

//On a reset make sure the start is in sync with the next cycle
static void AdlibTimer__Reset(AdlibTimer *self, const long long time) {
	long long delta, rem;
	self->overflow = false;
	if ( !self->enabled )
		return;
	delta = time - self->start;
	if ( self->delay )
		rem = self->delay - delta % self->delay;
	else
		rem = 0;
	self->start = time + rem;
}

static void AdlibTimer__Stop(AdlibTimer *self) {
	self->enabled = false;
}

static void AdlibTimer__Start(AdlibTimer *self, const long long time, Bits scale) {
	//Don't enable again
	if ( self->enabled ) {
		return;
	}
	self->enabled = true;
	self->delay = (255 - self->counter ) * scale;
	self->start = time + self->delay;
}

static AdlibTimer opl3_timers[2];

static uint8_t dbadlib_PortRead(void *impl, uint16_t port);
static void dbadlib_PortWrite(void *impl, uint16_t port, uint8_t val );
static void *dbadlib_create(int opl3_rate);
static void dbadlib_generate(int total, int16_t output[][2]);

//Check for it being a write to the timer
static bool AdlibChip__WriteTimer(AdlibTimer *timer, Bit32u reg, Bit8u val) {
	switch ( reg ) {
	case 0x02:
		timer[0].counter = val;
		return true;
	case 0x03:
		timer[1].counter = val;
		return true;
	case 0x04: {
		long long time = GETusTIME(0);
		if ( val & 0x80 ) {
			AdlibTimer__Reset(&timer[0], time);
			AdlibTimer__Reset(&timer[1], time);
		} else {
			AdlibTimer__Update(&timer[0], time);
			AdlibTimer__Update(&timer[1], time);
			if ( val & 0x1 ) {
				AdlibTimer__Start(&timer[0], time, 80);
				S_printf("Adlib: timer 0 set to %lluus\n",
						timer[0].delay);
			} else {
				AdlibTimer__Stop(&timer[0]);
			}
			timer[0].masked = (val & 0x40) > 0;
			if ( timer[0].masked )
				timer[0].overflow = false;
			if ( val & 0x2 ) {
				AdlibTimer__Start(&timer[1], time, 320);
				S_printf("Adlib: timer 1 set to %lluus\n",
						timer[1].delay);
			} else {
				AdlibTimer__Stop(&timer[1]);
			}
			timer[1].masked = (val & 0x20) > 0;
			if ( timer[1].masked )
				timer[1].overflow = false;
		}
		return true; }
	}
	return false;
}


//Read the current timer state, will use current double
static Bit8u AdlibChip__Read(AdlibTimer *timer) {
	long long time = GETusTIME(0);
	AdlibTimer__Update(&timer[0], time);
	AdlibTimer__Update(&timer[1], time);
	Bit8u ret = 0;
	//Overflow won't be set if a channel is masked
	if ( timer[0].overflow ) {
		ret |= 0x40;
		ret |= 0x80;
	}
	if ( timer[1].overflow ) {
		ret |= 0x20;
		ret |= 0x80;
	}
	return ret;

}

static void AdlibChip__AdlibChip(AdlibTimer *timer) {
	AdlibTimer__AdlibTimer(&timer[0]);
	AdlibTimer__AdlibTimer(&timer[1]);
}

// stripped down from DOSBOX adlib.h: class Adlib::Module
static struct {
	//Last selected address in the chip for the different modes
	Bit32u normal;
} reg;

// stripped down from DOSBOX adlib.cpp: Adlib::Module::PortWrite
static void dbadlib_PortWrite(void *impl, uint16_t port, uint8_t val ) {
	AdlibTimer *timer = impl;
	if ( port&1 ) {
		if ( !AdlibChip__WriteTimer( timer, reg.normal, val ) ) {
			opl_write( reg.normal, val );
		}
	} else {
		opl_write_index( port, val );
		reg.normal = val & 0x1ff;
	}
}


// stripped down from DOSBOX adlib.cpp: Adlib::Module::PortRead
static uint8_t dbadlib_PortRead(void *impl, uint16_t port) {
	AdlibTimer *timer = impl;
	//We allocated 4 ports, so just return -1 for the higher ones
	if ( !(port & 3 ) ) {
		return AdlibChip__Read(timer);
	} else {
		return 0xff;
	}
}

// converted from DOSBOX dbopl.cpp: DBOPL::Handler::Init
static void *dbadlib_create(int opl3_rate)
{
	AdlibChip__AdlibChip(opl3_timers);
	opl_init(opl3_rate);
	return opl3_timers;
}

// converted from DOSBOX dbopl.cpp: DBOPL::Handler::Generate
static void dbadlib_generate(int total, int16_t output[][2])
{
	opl_getsample((Bit16s *)output, total);
}

struct opl_ops dbadlib_ops = {
    .PortRead = dbadlib_PortRead,
    .PortWrite = dbadlib_PortWrite,
    .Create = dbadlib_create,
    .Generate = dbadlib_generate,
};
