/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef DOSEMU_DEBUG_H
#define DOSEMU_DEBUG_H

#include <stdio.h>
#include <stdarg.h>
#include "config.h"
#include "extern.h"

#if GCC_VERSION_CODE >= 2005
# define FORMAT(T,A,B)  __attribute__((format(T,A,B)))
#else
# define FORMAT(T,A,B)
#endif

#if GCC_VERSION_CODE >= 2005
# define NORETURN	__attribute__((noreturn))
#else
# define NORETURN
#endif

/*  DONT_DEBUG_BOOT - if you set that, debug messages are
 *	skipped until the cpuemu is activated by int0xe6, so you can
 *	avoid the long DOS intro when using high debug values.
 *	You have to define it if you want to trace into the DOS boot
 *	phase (i.e. from the loading at 0x7c00 on) */
#if 1
#define DONT_DEBUG_BOOT
#endif

/*  CIRCULAR_LOGBUFFER (utilities.c) - another way to reduce the size of
 *	the debug logfile by keeping only the last n lines (n=~20000)
 */
#if 0
#define	CIRCULAR_LOGBUFFER
#endif

/* this is for cpuemu-like tracing via TF (w/o dosdebug) */
#ifdef X86_EMULATOR	/* for the moment */
#if 0
#define TRACE_DPMI
#endif
#endif

/*
 * The 'struct debug_flags' defines which features may print discrete debug
 * messages. If you want to add one, also adapt the following places in the
 * DOSEMU code:
 *
 *  src/base/init/config.c, parse_debugflags(), allopts[]
 *  src/include/emu.h, 'EXTERN struct debug_flags d INIT({0..})'
 *  src/base/async/dyndeb.c, GetDebugFlagsHelper()
 *
 * Here is an overview of which flags are used and which not, please keep
 * this comment in sync with the reality:
 *
 *   used: aA  cCdDeE  g h iI  k   mMn   pP QrRsStT  v wWxX      and '#'
 *   free:   bB      fF G H  jJ KlL   NoO  q        U V    yYzZ
 */
struct debug_flags {
  unsigned char
   disk,		/* disk msgs         "d" */
   read,		/* disk read         "R" */
   write,		/* disk write        "W" */
   dos,			/* unparsed int 21h  "D" */
   cdrom,               /* cdrom             "C" */
   video,		/* video             "v" */
   X,			/* X support         "X" */
   keyb,		/* keyboard          "k" */
   io,			/* port I/O          "i" */
   io_trace, 		/* I/O trace         "T" */
   serial,		/* serial            "s" */
   mouse,		/* mouse             "m" */
   defint,		/* default ints      "#" */
   printer,		/* printer           "p" */
   general,		/* general           "g" */
   config,		/* configuration     "c" */
   warning,		/* warning           "w" */
   hardware,		/* hardware          "h" */
   IPC,			/* IPC               "I" */
   EMS,			/* EMS               "E" */
   xms,			/* xms               "x" */
   dpmi,		/* dpmi              "M" */
   network,		/* IPX network       "n" */
   pd,			/* Packet driver     "P" */
   request,		/* PIC               "r" */
   sound, 		/* SOUND	     "S" */
   aspi,		/* ASPI interface    "A" */
   mapping,		/* Mapping driver    "Q" */
   pci                  /* PCI               "Z" */
#ifdef X86_EMULATOR
   ,emu			/* CPU emulation     "e" */
#endif
#ifdef TRACE_DPMI
   ,dpmit		/* DPMI emu-debug    "t" */
#endif
   ;
};

#ifdef DONT_DEBUG_BOOT
extern struct debug_flags d_save;
#endif
extern struct debug_flags d;

int log_printf(int, const char *,...) FORMAT(printf, 2, 3);

void p_dos_str(char *,...) FORMAT(printf, 1, 2);

#if 0  /* set this to 1, if you want dosemu to honor the -D flags */
 #define NO_DEBUGPRINT_AT_ALL
#endif
#undef DEBUG_LITE

extern FILE *dbg_fd;

EXTERN int shut_debug INIT(0);

#ifndef NO_DEBUGPRINT_AT_ALL
# ifdef DEBUG_LITE
#  define ifprintf(flg,fmt,a...)	do{ if (flg) log_printf(0x10000|__LINE__,fmt,##a); }while(0)
# else
#  define ifprintf(flg,fmt,a...)	do{ if (flg) log_printf(flg,fmt,##a); }while(0)
# endif
#else
# define ifprintf(flg,fmt,a...) 
#endif

/* unconditional message into debug log */
#define dbug_printf(f,a...)	ifprintf(10,f,##a)

/* unconditional message into debug log and stderr */
void error(const char *fmt, ...);
void verror(const char *fmt, va_list args);

#define flush_log()		{ if (dbg_fd) log_printf(-1, "\n"); }

/* "dRWDCvXkiTsm#pgcwhIExMnPrS" */
#define d_printf(f,a...) 	ifprintf(d.disk,f,##a)
#define R_printf(f,a...) 	ifprintf(d.read,f,##a)
#define W_printf(f,a...) 	ifprintf(d.write,f,##a)
#define ds_printf(f,a...)     	ifprintf(d.dos,f,##a)
#define C_printf(f,a...)        ifprintf(d.cdrom,f,##a)
#define v_printf(f,a...) 	ifprintf(d.video,f,##a)
#define X_printf(f,a...)        ifprintf(d.X,f,##a)
#define k_printf(f,a...) 	ifprintf(d.keyb,f,##a)
#define i_printf(f,a...) 	ifprintf(d.io,f,##a)
#define T_printf(f,a...) 	ifprintf(d.io_trace,f,##a)
#define s_printf(f,a...) 	ifprintf(d.serial,f,##a)
#define m_printf(f,a...)	ifprintf(d.mouse,f,##a)
#define di_printf(f,a...)     	ifprintf(d.defint,f,##a)
#define p_printf(f,a...) 	ifprintf(d.printer,f,##a)
#define g_printf(f,a...)	ifprintf(d.general,f,##a)
#define c_printf(f,a...) 	ifprintf(d.config,f,##a)
#define warn(f,a...)     	ifprintf(d.warning,f,##a)
#define h_printf(f,a...) 	ifprintf(d.hardware,f,##a)
#define I_printf(f,a...) 	ifprintf(d.IPC,f,##a)
#define E_printf(f,a...) 	ifprintf(d.EMS,f,##a)
#define x_printf(f,a...)	ifprintf(d.xms,f,##a)
#define D_printf(f,a...)	ifprintf(d.dpmi,f,##a)
#define n_printf(f,a...)        ifprintf(d.network,f,##a)  /* TRB     */
#define pd_printf(f,a...)       ifprintf(d.pd,f,##a)	   /* pktdrvr */
#define r_printf(f,a...)        ifprintf(d.request,f,##a)
#define S_printf(f,a...)     	ifprintf(d.sound,f,##a)
#define A_printf(f,a...)     	ifprintf(d.aspi,f,##a)
#define Q_printf(f,a...)	ifprintf(d.mapping,f,##a)
#define Z_printf(f,a...)        ifprintf(d.pci,f,##a)
#ifdef X86_EMULATOR
#define e_printf(f,a...)     	ifprintf(d.emu,f,##a)
#define t_printf(f,a...)     	ifprintf(d.dpmit,f,##a)
#endif

#ifndef NO_DEBUGPRINT_AT_ALL

#define ALL_DEBUG_ON	memset(&d,9,sizeof(d))
#define ALL_DEBUG_OFF	memset(&d,0,sizeof(d))

#else

#define ALL_DEBUG_ON
#define ALL_DEBUG_OFF

#endif

#endif /* DOSEMU_DEBUG_H */
