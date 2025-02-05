/***************************************************************************
 *
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2001 Alberto Vignani, FIAT Research Center
 *				a.vignani@crf.it
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Additional copyright notes:
 *
 * 1. The kernel-level vm86 handling was taken out of the Linux kernel
 *  (linux/arch/i386/kernel/vm86.c). This code originaly was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

#ifndef _EMU86_HOST_H
#define _EMU86_HOST_H

#include "dos2linux.h"
#define read_byte(x) do_read_byte((x), emu_pagefault_handler)
#define read_word(x) do_read_word((x), emu_pagefault_handler)
#define read_dword(x) do_read_dword((x), emu_pagefault_handler)
#define read_qword(x) do_read_qword((x), emu_pagefault_handler)
#define write_byte(x,y) do_write_byte((x), (y), emu_pagefault_handler)
#define write_word(x,y) do_write_word((x), (y), emu_pagefault_handler)
#define write_dword(x,y) do_write_dword((x), (y), emu_pagefault_handler)
#define write_qword(x,y) do_write_qword((x), (y), emu_pagefault_handler)

#if defined(ppc)||defined(__ppc)||defined(__ppc__)
/* NO PAGING! */
/*
 *  $Id$
 */
/* alas, egcs sounds like it has a bug in this code that doesn't use the
   inline asm correctly, and can cause file corruption. */
static __inline__ unsigned short ppc_pswap2(long addr)
{
	unsigned val;
	__asm__ __volatile__ ("lhbrx %0,0,%1" : "=r" (val) :
		 "r" ((unsigned short *)addr), "m" (*(unsigned short *)addr));
	return val;
}

static __inline__ void ppc_dswap2(long addr, unsigned short val)
{
	__asm__ __volatile__ ("sthbrx %1,0,%2" : "=m" (*(unsigned short *)addr) :
		 "r" (val), "r" ((unsigned short *)addr));
}

static __inline__ unsigned long ppc_pswap4(long addr)
{
	unsigned val;
	__asm__ __volatile__ ("lwbrx %0,0,%1" : "=r" (val) :
		 "r" ((unsigned long *)addr), "m" (*(unsigned long *)addr));
	return val;
}

static __inline__ unsigned long long ppc_pswap8(long addr)
{
	union {	unsigned long long lq; struct {unsigned long ll,lh;} lw; } val;
	__asm__ __volatile__ (" \
		lwbrx %0,0,%2\n \
		addi  %2,%2,4\n \
		lwbrx %1,0,%2" \
		: "=r" (val.lw.lh), "=r" (val.lw.ll)
		: "r" ((unsigned long *)addr), "m" (*(unsigned long *)addr) );
	return val.lq;
}

static __inline__ void ppc_dswap4(long addr, unsigned long val)
{
	__asm__ __volatile__ ("stwbrx %1,0,%2" : "=m" (*(unsigned long *)addr) :
		 "r" (val), "r" ((unsigned long *)addr));
}

static __inline__ void ppc_dswap8(long addr, unsigned long long val)
{
	union { unsigned long long lq; struct {unsigned long lh,ll;} lw; } v;
	v.lq = val;
	__asm__ __volatile__ (" \
		stwbrx %1,0,%3\n \
		addi   %3,%3,4\n \
		stwbrx %2,0,%3" \
		: "=m" (*(unsigned long *)addr)
		: "r" (v.lw.ll), "r" (v.lw.lh), "r" ((unsigned long *)addr) );
}

#endif		/* ppc */

/////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOUND
/* `Fetch` is for CODE reads, `Get`/`Put` is for DATA.
 *  WARNING - BOUND uses SIGNED limits!! */
#define Fetch(a)	({ \
	register int p = (int)(a);\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	*((unsigned char *)p); })
#define FetchW(a)	({ \
	register int p = (int)(a)+1;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	*((unsigned short *)(a)); })
#define FetchL(a)	({ \
	register int p = (int)(a)+3;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	*((unsigned int *)(a)); })

#define DataFetchWL_U(m,a)	({ \
	register unsigned f = ((m)&DATA16? 1:3);\
	register int p = (int)(a)+f;\
	register int res;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	__asm__ ("xorl	%0,%0\n\
		shr	$2,%1\n\
		jc	1f\n\
		.byte	0x66\n\
1:		movl	(%2),%0"\
		: "=&r"(res) : "r"(f), "g"(a) : "memory" ); res; })

#define DataFetchWL_S(m,a)	({ \
	register unsigned f = ((m)&DATA16? 1:3);\
	register int p = (int)(a)+f;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	(f&2? *((int *)(a)):*((short *)(a))); })

#define AddrFetchWL_U(m,a)	({ \
	register unsigned f = ((m)&ADDR16? 1:3);\
	register int p = (int)(a)+f;\
	register int res;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	__asm__ ("xorl	%0,%0\n\
		shr	$2,%1\n\
		jc	1f\n\
		.byte	0x66\n\
1:		movl	(%2),%0"\
		: "=&r"(res) : "r"(f), "g"(a) : "memory" ); res; })

#define AddrFetchWL_S(m,a)	({ \
	register unsigned f = ((m)&ADDR16? 1:3);\
	register int p = (int)(a)+f;\
	__asm__ ("boundl %0,%1" : : "r"(p),"m"(CS_DTR) : "memory" );\
	(f&2? *((int *)(a)):*((short *)(a))); })
#else
#define Fetch(a)	read_byte(a)
#define FetchW(a)	read_word(a)
#define FetchL(a)	read_dword(a)
#define DataFetchWL_U(m,a) ((m)&DATA16? FetchW(a):FetchL(a))
#define DataFetchWL_S(m,a) ((m)&DATA16? (short)FetchW(a):(int)FetchL(a))
#define AddrFetchWL_U(m,a) ((m)&ADDR16? FetchW(a):FetchL(a))
#define AddrFetchWL_S(m,a) ((m)&ADDR16? (short)FetchW(a):(int)FetchL(a))
#endif
#define GetDWord(a)	read_word(a)
#define GetDLong(a)	read_dword(a)
#define DataGetWL_U(m,a) ((m)&DATA16? GetDWord(a):GetDLong(a))
#define DataGetWL_S(m,a) ((m)&DATA16? (short)GetDWord(a):(int)GetDLong(a))

#if 0
#if defined(ppc)||defined(__ppc)||defined(__ppc__)
#define Fetch(a)	*((unsigned char *)(a))
#define FetchW(a)	ppc_pswap2((int)(a))
#define FetchL(a)	ppc_pswap4((int)(a))
#define DataFetchWL_U(m,a) ((m)&DATA16? FetchW(a):FetchL(a))
#define DataFetchWL_S(m,a) ((m)&DATA16? (short)FetchW(a):(int)FetchL(a))
#define AddrFetchWL_U(m,a) ((m)&ADDR16? FetchW(a):FetchL(a))
#define AddrFetchWL_S(m,a) ((m)&ADDR16? (short)FetchW(a):(int)FetchL(a))

#define GetDWord(a)	ppc_pswap2((int)(a))
#define GetDLong(a)	ppc_pswap4((int)(a))
#define DataGetWL_U(m,a) ((m)&DATA16? GetDWord(a):GetDLong(a))
#define DataGetWL_S(m,a) ((m)&DATA16? (short)GetDWord(a):(int)GetDLong(a))
#endif

/* general-purpose */
//static inline unsigned short pswap2(long a) {
//	register unsigned char *p = (unsigned char *)a;
//	return p[0] | (p[1]<<8);
//}
//
//static inline unsigned short dswap2(unsigned short w) {
//	register unsigned char *p = (unsigned char *)&w;
//	return p[0] | (p[1]<<8);
//}
//
//static inline unsigned long pswap4(long a) {
//	register unsigned char *p = (unsigned char *)a;
//	return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
//}
//
//static inline unsigned long dswap4(unsigned long l) {
//	register unsigned char *p = (unsigned char *)&l;
//	return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
//}
//#define GetDWord(a)		pswap2((long)a)
//#define GetDLong(a)		pswap4((long)a)
//#define Fetch(p)		*(p)
#endif

#if defined(HOST_ARCH_X86) && !defined(HAVE___FLOAT80)
typedef long double __float80;
#undef __SIZEOF_FLOAT80__
#define __SIZEOF_FLOAT80__ sizeof(__float80)
#define HAVE___FLOAT80 1
#endif

#if !defined(HOST_ARCH_X86) && !defined(HAVE__FLOAT128)
typedef long double _Float128;
#endif

/////////////////////////////////////////////////////////////////////////////

#endif
