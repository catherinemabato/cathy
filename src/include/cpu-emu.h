/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef DOSEMU_CPUEMU_H
#define DOSEMU_CPUEMU_H

#include "kversion.h"
#include "bitops.h"

/*
 * Size of io_bitmap in longwords: 32 is ports 0-0x3ff.
 */
#define IO_BITMAP_SIZE	32

extern unsigned long	io_bitmap[IO_BITMAP_SIZE+1];
extern void e_priv_iopl(int);
#define test_ioperm(a)	((a)>0x3ff? 0:test_bit((a),io_bitmap))

/* ------------------------------------------------------------------------
 * if this is defined, cpuemu will only start when explicitly activated
 * by int0xe6. Technically: if we want to trace DOS boot we undefine this
 * and config.cpuemu will be set at 3 from the start - else it is set at 1
 */
#if 1
#define DONT_START_EMU
#endif

/* define this to skip emulation through video BIOS. This speeds up a lot
 * the cpuemu and avoids possible time-dependent code in the VBIOS. It has
 * a drawback: when we jump into REAL vm86 in the VBIOS, we never know
 * where we will exit back to the cpuemu. Oh, well, speed is what matters...
 */
#if 1
#define	SKIP_EMU_VBIOS
#endif

/*  DONT_DEBUG_BOOT (in emu.h) - if you set that, debug messages are
 *	skipped until the cpuemu is activated by int0xe6, so you can
 *	avoid the long DOS intro when using high debug values.
 *  CIRCULAR_LOGBUFFER (utilities.c) - another way to reduce the size of
 *	the debug logfile by keeping only the last n lines (n=~20000)
 */
#if 0
#define	CIRCULAR_LOGBUFFER
#endif

/* if defined, trace instructions (with d.emu>3) only in protected
 * mode code. This is useful to skip timer interrupts and/or better
 * follow the instruction flow */
#if 0
#define SKIP_VM86_TRACE
#endif

/* this is for cpuemu-like tracing via TF (w/o dosdebug) */
#if 1
#define TRACE_DPMI
#endif

/* ----------------------------------------------------------------------- */

/* Cpuemu status register - pack as much info as possible here, so to
 * use a single test to check if we have to go further or not */
#define CeS_SIGPEND	0x01	/* signal pending mask */
#define CeS_SIGACT	0x02	/* signal active mask */
#define CeS_RPIC	0x04	/* pic asks for interruption */
#define CeS_STI		0x08	/* IF active was popped */
#define CeS_LOCK	0x800	/* cycle lock (pop ss; pop sp et sim.) */
#define CeS_TRAP	0x1000	/* INT01 Sstep active */
#define CeS_DRTRAP	0x2000	/* Debug Registers active */

extern int CEmuStat;

/* called from dpmi.c */
void emu_mhp_SetTypebyte (unsigned short selector, int typebyte);
unsigned short emu_do_LAR (unsigned short selector);

#endif	/*DOSEMU_CPUEMU_H*/
