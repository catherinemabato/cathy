/* xms.c for the DOS emulator
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * DANG_BEGIN_MODULE
 *
 * Currently the XMS 3.0 spec is covered in this file. XMS is fairly simple
 * as it only deals with allocating extended memory and then moving it 
 * around in specific calls. This spec also includes the allocation of UMB's,
 * so they are also included as part of this file. The amount of xms memory
 * returned to DOS programs via the XMS requests, or int15 fnc88 is set in
 * "/etc/dosemu.conf" via the XMS paramter.
 *
 * DANG_END_MODULE
 * DANG_BEGIN_CHANGELOG
 *
 * $Date: 1995/01/14 15:29:17 $
 * $Source: /home/src/dosemu0.60/dosemu/RCS/xms.c,v $
 * $Revision: 2.7 $
 * $State: Exp $
 *
 * $Log: xms.c,v $
 * Revision 2.7  1995/01/14  15:29:17  root
 * New Year checkin.
 *
 * Revision 2.6  1994/11/03  11:43:26  root
 * Checkin Prior to Jochen's Latest.
 *
 * Revision 2.5  1994/10/14  17:58:38  root
 * Prep for pre53_27.tgz
 *
 * Revision 2.4  1994/09/26  23:10:13  root
 * Prep for pre53_22.
 *
 * Revision 2.3  1994/06/27  02:15:58  root
 * Prep for pre53
 *
 * Revision 2.2  1994/06/14  22:00:18  root
 * Alistair's DANG inserted for the first time :-).
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.16  1994/06/10  23:21:21  root
 * prep for pre51_25.
 *
 * Revision 1.15  1994/06/05  21:42:42  root
 * prePrep for pre51_24.
 *
 * Revision 1.14  1994/05/04  21:56:55  root
 * Prior to Alan's mouse patches.
 *
 * Revision 1.13  1994/03/30  22:12:30  root
 * Prep for 0.51 pre 2.
 *
 * Revision 1.12  1994/03/23  23:24:51  root
 * Prepare to split out do_int.
 *
 * Revision 1.11  1994/03/13  01:07:31  root
 * Poor attempts to optimize.
 *
 * Revision 1.10  1994/03/10  02:49:27  root
 * Back to SINGLE Process.
 *
 * Revision 1.9  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.8  1994/01/25  20:02:44  root
 * Exchange stderr <-> stdout.
 *
 * Revision 1.7  1994/01/20  21:14:24  root
 * Indent.
 *
 * Revision 1.6  1994/01/03  22:19:25  root
 * Debugging
 *
 * Revision 1.5  1993/12/07  18:09:09  root
 * Looking for more bugs :-).
 *
 * Revision 1.4  1993/12/05  20:59:03  root
 * Fixed UMB size 0, move XMS edx:ebx problem.
 *
 * Revision 1.3  1993/11/30  22:21:03  root
 * Final Freeze for release pl3
 *
 * Revision 1.2  1993/11/30  21:26:44  root
 * Chips First set of patches, WOW!
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.10  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.9  1993/04/07  21:04:26  root
 * big move
 *
 * Revision 1.8  1993/04/05  17:25:13  root
 * big pre-49 checkit; EMS, new MFS redirector, etc.
 *
 * Revision 1.7  1993/03/04  22:35:12  root
 * put in perfect shared memory, HMA and everything.  added PROPER_STI.
 *
 * Revision 1.6  1993/02/24  11:33:24  root
 * some general cleanups, fixed the key-repeat bug.
 *
 * Revision 1.5  1993/02/18  19:35:58  root
 * just added newline so diff wouldn't barf
 *
 * Revision 1.4  1993/02/18  18:53:41  root
 * this is for 0.48patch1, mainly to fix the XMS bug (moved libemu up to
 * the 1 GB mark), and to try out the faster termcap buffer-compare code.
 *
 * Revision 1.3  1993/02/16  00:21:29  root
 * DOSEMU 0.48 DISTRIBUTION
 *
 * DANG_END_CHANGELOG
 *
 * NOTE: I keep the BYTE size of EMB's in a field called "size" in the EMB
 *    structure.  Most XMS calls specify/expect size in KILOBYTES.
 */


#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "emu.h"
#include "config.h"
#include "memory.h"
#include "xms.h"
#include "dosio.h"
#include "machcompat.h"

static int umb_find_unused(void);
/* 128*1024 is the amount of memory currently reserved in dos.c above
 * the 1 MEG mark.  ugly.  fix this.
 */

static char RCSxms[] = "$Header: /home/src/dosemu0.60/dosemu/RCS/xms.c,v 2.7 1995/01/14 15:29:17 root Exp root $";

#define	 XMS_GET_VERSION		0x00
#define	 XMS_ALLOCATE_HIGH_MEMORY	0x01
#define	 XMS_FREE_HIGH_MEMORY		0x02
#define	 XMS_GLOBAL_ENABLE_A20		0x03
#define	 XMS_GLOBAL_DISABLE_A20		0x04
#define	 XMS_LOCAL_ENABLE_A20		0x05
#define	 XMS_LOCAL_DISABLE_A20		0x06
#define	 XMS_QUERY_A20			0x07
#define	 XMS_QUERY_FREE_EXTENDED_MEMORY	0x08
#define	 XMS_ALLOCATE_EXTENDED_MEMORY	0x09
#define	 XMS_FREE_EXTENDED_MEMORY	0x0a
#define	 XMS_MOVE_EXTENDED_MEMORY_BLOCK	0x0b
#define	 XMS_LOCK_EXTENDED_MEMORY_BLOCK	0x0c
#define	 XMS_UNLOCK_EXTENDED_MEMORY_BLOCK 0x0d
#define	 XMS_GET_EMB_HANDLE_INFORMATION	0x0e
#define	 XMS_RESIZE_EXTENDED_MEMORY_BLOCK 0x0f
#define	 XMS_ALLOCATE_UMB		0x10
#define	 XMS_DEALLOCATE_UMB		0x11

#define	HIGH_MEMORY_IN_USE		0x92
#define	HIGH_MEMORY_NOT_ALLOCATED	0x93
#define XMS_OUT_OF_SPACE		0xa0
#define XMS_INVALID_HANDLE		0xa2


int a20 = 0;
int freeHMA = 1;		/* is HMA free? */

static struct Handle handles[NUM_HANDLES + 1];
static int handle_count = 0;

static int xms_grab_int15again = 0;

struct EMM get_emm(unsigned int, unsigned int);
void show_emm(struct EMM);
static void xms_query_freemem(int), xms_allocate_EMB(int), xms_free_EMB(void),
 xms_move_EMB(void), xms_lock_EMB(int), xms_EMB_info(int), xms_realloc_EMB(int),
 xms_request_UMB(void), xms_release_UMB(void);

static int FindFreeHandle(int);

/* beginning of quote from Mach */

#define UMB_BASE (caddr_t)0xc0000
#define UMB_SIZE 0x40000
#define UMBS (UMB_SIZE/16)
#define UMB_PAGE 4096
#define UMB_NULL -1

#if 0
/* EMS page frame 0xd0000-0xdffff.  DOSEMU uses 0xe0000-0xeffff */
#define IN_EMM_SPACE(addr) (config.ems_size && (int)(addr) >= 0xd0000 \
			    && (int)(addr) <= 0xdffff)
#else
/*  EMS page frame now is configurable via /etc/dosemu.conf */
#define SPACE_64K   0x10000
#define IN_EMM_SPACE(addr) (config.ems_size && (int)(addr) >= EMM_BASE_ADDRESS \
                            && (int)(addr) < (EMM_BASE_ADDRESS+SPACE_64K))

#define IN_HARDWARE_PAGES(addr) ( config.must_spare_hardware_ram\
                                  && ((int)addr >= HARDWARE_RAM_START) \
                                  && ((int)addr < HARDWARE_RAM_STOP) \
                                  && ( config.hardware_pages[((int)addr-HARDWARE_RAM_START) >> 12] ) )
#endif

/* XXX - I have assumed 32k of BIOS... */
#define IN_BIOS_SPACE(addr) ((int)(addr) >= VBIOS_START && \
      (int)(addr) < (VBIOS_START + VBIOS_SIZE) && config.mapped_bios)

#define IN_EMU_SPACE(addr) (((int)(addr) >= (BIOSSEG*16) && (int)(addr) <= \
			    (BIOSSEG*16 + 0xffff)) || IN_BIOS_SPACE(addr))

static struct umb_record {
  vm_address_t addr;
  vm_size_t size;
  boolean_t in_use;
  boolean_t free;
} umbs[UMBS];

#define x_Stub(arg1, s, a...)   x_printf("XMS: "s, ##a)
#define Debug0(args)		x_Stub args
#define Debug1(args)		x_Stub args
#define Debug2(args)		x_Stub args
#define dbg_fd stderr

static boolean_t
umb_memory_empty(addr, size)
     vm_address_t addr;
     int size;
{
  int i;
  int *memory = (int *) addr;

  for (i = 0; i < (size / 4); i++) {
    if ((memory[i] != 0xffffffff) && (memory[i] != 0)) {
      return FALSE;
    }
  }
  Debug0((dbg_fd, "Found free UMB region: %p\n", (void *) addr));
  return (TRUE);
}

static int
umb_setup(void)
{
  int i;
  int umb,tumb;
  vm_address_t addr;

  for (i = 0; i < UMBS; i++) {
    umbs[i].in_use = FALSE;
  }

  if (config.ems_size) {
    /* set up EMS page frame */
    umb = umb_find_unused();

    umbs[umb].in_use = TRUE;
    umbs[umb].free = FALSE;
    umbs[umb].addr = (caddr_t) EMM_BASE_ADDRESS;
    umbs[umb].size = 0x10000;
  }

  /* set up DOSEMU trampolines */
  umb = umb_find_unused();
  tumb=umb;
  umbs[umb].in_use = TRUE;
  umbs[umb].free = FALSE;
  umbs[umb].addr = (caddr_t) (BIOSSEG * 16);
  umbs[umb].size = 0x10000;

  /* set up 32K VGA BIOS mem if necessary */
  if (config.mapped_bios) {
    umb = umb_find_unused();

    umbs[umb].in_use = TRUE;
    umbs[umb].free = FALSE;
    umbs[umb].addr = (caddr_t) VBIOS_START;
    umbs[umb].size = 0x8000;

    if (config.vbios_seg == 0xe000) {
      Debug0((dbg_fd, "Fixing UMB for E000\n"));
      umb = umb_find_unused();
      umbs[umb].in_use = TRUE;
      umbs[umb].free = FALSE;
      umbs[umb].addr = (caddr_t) 0xc0000;
      umbs[umb].size = 0x8000;
    }

  }
  umb=tumb;
  for (addr = UMB_BASE; addr < (vm_address_t) (UMB_BASE + UMB_SIZE);
       addr += UMB_PAGE) {
    if (IN_EMM_SPACE(addr) || IN_EMU_SPACE(addr) || IN_HARDWARE_PAGES(addr)) {
#if 1
      Debug0((dbg_fd, "skipped UMB region: %p EMM-%d EMU=%d hardw=%d\n",(void *) addr, 
               IN_EMM_SPACE(addr),IN_EMU_SPACE(addr),IN_HARDWARE_PAGES(addr)));
#endif
      continue;
    }
    if (umb_memory_empty(addr, UMB_PAGE)) {
      if ((umbs[umb].addr + umbs[umb].size) == addr) {
	umbs[umb].size += UMB_PAGE;
      }
      else {
	umb = umb_find_unused();
	umbs[umb].in_use = TRUE;
	umbs[umb].free = TRUE;
	umbs[umb].addr = addr;
	umbs[umb].size = UMB_PAGE;
	Debug0((dbg_fd, "New UMB region: %p\n",
		(void *) addr));
      }
    }
  }

  for (i = 0; i < UMBS; i++) {
    if (umbs[i].in_use && umbs[i].free) {
      vm_address_t addr = umbs[i].addr;
      vm_size_t size = umbs[i].size;

#if 0
      MACH_CALL((vm_deallocate(mach_task_self(),
			       (vm_address_t) addr,
			       (vm_size_t) size)), "vm_deallocate");
      MACH_CALL((vm_allocate(mach_task_self(), &addr,
			     (vm_size_t) size,
			     FALSE)),
		"vm_allocate of umb block.");
#else
      Debug0((dbg_fd, "umb_setup: addr %p size 0x%04x\n",
	      (void *) addr, size));
#endif
    }
  }
  return 0;
}

static int
umb_find_unused(void)
{
  int i;

  for (i = 0; i < UMBS; i++) {
    if (!umbs[i].in_use)
      return (i);
  }
  return (UMB_NULL);
}

static int
umb_find(vm_address_t segbase)
{
  int i;
  vm_address_t addr = (vm_address_t) ((int) segbase * 16);

  for (i = 0; i < UMBS; i++) {
    if (umbs[i].in_use &&
	((addr >= umbs[i].addr) &&
	 (addr < (umbs[i].addr + umbs[i].size)))) {
      return (i);
    }
  }
  return (UMB_NULL);
}

static vm_address_t
umb_allocate(int size)
{
  int i;

  for (i = 0; i < UMBS; i++) {
    if (umbs[i].in_use && umbs[i].free) {
      if (umbs[i].size > size) {
	int new_umb = umb_find_unused();

	if (new_umb != UMB_NULL) {
	  umbs[new_umb].in_use = TRUE;
	  umbs[new_umb].free = TRUE;
	  umbs[new_umb].addr =
	    umbs[i].addr + size;
	  umbs[new_umb].size =
	    umbs[i].size - size;
	  umbs[i].size = size;
	  umbs[i].free = FALSE;
	  return (umbs[i].addr);
	}
      }
      else
	if (umbs[i].size == size) {
	  umbs[i].free = FALSE;
	  return (umbs[i].addr);
	}
    }
  }
  return ((vm_address_t) 0);
}

static void umb_cleanup(int umb)
{
  vm_address_t umb_top = umbs[umb].addr + umbs[umb].size;
  int i, updated;

  do {
    updated = 0;
    for (i = 0; i < UMBS; i++) {
      if (umbs[i].in_use && umbs[i].free) {
	if (umbs[i].addr == umb_top) {
          umbs[umb].size += umbs[i].size;
          umb_top         = umbs[umb].addr + umbs[umb].size;
          umbs[i].in_use  = 0;
	  updated         = 1;
	  continue;
	}
	if (umbs[i].addr + umbs[i].size == umbs[umb].addr) {
	  umbs[umb].addr  = umbs[i].addr;
	  umbs[umb].size += umbs[i].size;
	  umb_top         = umbs[umb].addr + umbs[umb].size;
	  umbs[i].in_use  = 0;
	  updated         = 1;
	}
      }
    }
  } while (updated);
}
/* why int? */
static int
umb_free(int segbase)
{
  int umb = umb_find((vm_address_t)segbase);

  if (umb != UMB_NULL) {
    umbs[umb].free = TRUE;
    umb_cleanup(umb);
  }
  return (0);
}

static int
umb_query(void)
{
  int i;
  int largest = 0;

  for (i = 0; i < UMBS; i++) {
    if (umbs[i].in_use && umbs[i].free) {
      if (umbs[i].size > largest)
	largest = umbs[i].size;
    }
  }
  Debug0((dbg_fd, "umb_query: largest UMB was %d bytes\n", largest));
  return (largest);
}

/* end of stuff from Mach */

void
xms_init(void)
{
  int i;

  if (!config.xms_size) {
    CARRY;
    REG(eax) = 0x8600;
    return;
  }

  x_printf("XMS: initializing XMS...\nRCS: %s\n%d handles\n\n",
	   RCSxms, NUM_HANDLES);

  freeHMA = 1;

  umb_setup();

  a20 = 0;
  xms_grab_int15 = 0;

  handle_count = 0;
  for (i = 0; i < NUM_HANDLES + 1; i++)
    handles[i].valid = 0;
}

void
xms_control(void)
{
  /* the int15 get memory size call (& block copy, unimplemented) are
   * supposed to be unrevectored until the first non-version XMS call
   * to "maintain compatibility with existing device drivers."
   * hence this blemish.  if Microsoft could act, say, within 3 years of
   * a problem's first appearance, maybe we wouldn't have these kinds
   * of compatibility hacks.
   */
  if (HI(ax) != 0) {
    if (!xms_grab_int15again)
      xms_grab_int15 = 1;
    xms_grab_int15again = 1;
  }

  switch (HI(ax)) {
  case 0:			/* Get XMS Version Number */
    LWORD(eax) = XMS_VERSION;
    LWORD(ebx) = XMS_DRIVER_VERSION;
    LWORD(edx) = 1;		/* HMA exists */
    x_printf("XMS driver version. XMS Spec: %04x dosemu-XMS: %04x\n",
	     LWORD(eax), LWORD(ebx));
    x_printf(RCSxms);
    break;

  case 1:			/* Request High Memory Area */
    x_printf("XMS request HMA size 0x%04x\n", LWORD(edx));
    if (freeHMA) {
      x_printf("XMS: allocating HMA size 0x%04x\n", LWORD(edx));
      freeHMA = 0;
      LWORD(eax) = 1;
    }
    else {
      x_printf("XMS: HMA already allocated\n");
      LWORD(eax) = 0;
      LO(bx) = 0x91;
    }
    break;

  case 2:			/* Release High Memory Area */
    x_printf("XMS release HMA\n");
    if (freeHMA) {
      x_printf("XMS: HMA already free\n");
      LWORD(eax) = 0;
      LO(bx) = 0x93;
    }
    else {
      x_printf("XMS: freeing HMA\n");
      LWORD(eax) = 1;
      freeHMA = 1;
    }
    break;

  case 3:			/* Global Enable A20 */
    x_printf("XMS global enable A20\n");
    if (!a20)
      set_a20(1);		/* map in HMA */
    a20 = 1;
    LWORD(eax) = 1;
    break;

  case 4:			/* Global Disable A20 */
    x_printf("XMS global disable A20\n");
    if (a20)
      set_a20(0);
    a20 = 0;
    LWORD(eax) = 1;
    break;

  case 5:			/* Local Enable A20 */
    a20++;
    if (a20 == 1)
      set_a20(1);
    LWORD(eax) = 1;
    break;

  case 6:			/* Local Disable A20 */
    x_printf("XMS LOCAL disable A20\n");
    if (a20)
      a20--;
    if (a20 == 0)
      set_a20(0);
    LWORD(eax) = 1;
    break;

    /* DOS, if loaded as "dos=high,umb" will continually poll
	  * the A20 line.
          */
  case 7:			/* Query A20 */
    LWORD(eax) = a20 ? 1 : 0;
    LWORD(ebx) = 0;		/* no error */
    break;

  case 8:			/* Query Free Extended Memory */
    xms_query_freemem(OLDXMS);
    break;

  case 9:			/* Allocate Extended Memory Block */
    xms_allocate_EMB(OLDXMS);
    break;

  case 0xa:			/* Free Extended Memory Block */
    xms_free_EMB();
    break;

  case 0xb:			/* Move Extended Memory Block */
    xms_move_EMB();
    break;

  case 0xc:			/* Lock Extended Memory Block */
    xms_lock_EMB(1);
    break;

  case 0xd:			/* Unlock Extended Memory Block */
    xms_lock_EMB(0);
    break;

  case 0xe:			/* Get EMB Handle Information */
    xms_EMB_info(OLDXMS);
    return;

  case 0xf:			/* Realocate Extended Memory Block */
    xms_realloc_EMB(OLDXMS);
    break;

#define state (&_regs)
  case XMS_ALLOCATE_UMB:
    {
      int size = WORD(state->edx) * 16;
      vm_address_t addr = size ? umb_allocate(size) : (vm_address_t) 0;

      Debug0((dbg_fd, "Allocate UMB memory: 0x%04x\n",
	      (unsigned) WORD(state->edx)));
      if (addr == (vm_address_t) 0) {
	Debug0((dbg_fd, "Allocate UMB Failure\n"));
	SETWORD(&(state->eax), 0);
	SETLOW(&(state->ebx), 0xb0);
	SETWORD(&(state->edx), umb_query() >> 4);
      }
      else {
	Debug0((dbg_fd, "Allocate UMB Success\n"));
	SETWORD(&(state->eax), 1);
	SETWORD(&(state->ebx), (int) addr >> 4);
	SETWORD(&(state->edx), size >> 4);
      }
      Debug0((dbg_fd, "umb_allocated: 0x%04x, 0x%04x\n",
	      (unsigned) WORD(state->ebx),
	      (unsigned) WORD(state->edx)));
      /* retval = UNCHANGED; */
      break;
    }

  case XMS_DEALLOCATE_UMB:
    {
      umb_free(WORD(state->edx));
      SETWORD(&(state->eax), 1);
      Debug0((dbg_fd, "umb_freed: 0x%04x\n",
	      (unsigned) WORD(state->edx)));
      /* retval = UNCHANGED; */
      break;
    }

  case 0x12:
    x_printf("XMS realloc UMB segment 0x%04x size 0x%04x\n",
	     LWORD(edx), LWORD(ebx));
    LWORD(eax) = 0;		/* failure */
    LO(bx) = 0x80;		/* function unimplemented */
    break;

    /* the functions below are the newer, 32-bit XMS 3.0 interface */

  case 0x88:			/* Query Any Free Extended Memory */
    xms_query_freemem(NEWXMS);
    break;

  case 0x89:			/* Allocate Extended Memory */
    xms_allocate_EMB(NEWXMS);
    break;

  case 0x8e:			/* Get EMB Handle Information */
    xms_EMB_info(NEWXMS);
    break;

  case 0x8f:			/* Reallocate EMB */
    xms_realloc_EMB(NEWXMS);
    break;

  default:
    error("Unimplemented XMS function AX=0x%04x", LWORD(eax));
    show_regs(__FILE__, __LINE__);		/* if you delete this, put the \n on the line above */
    LWORD(eax) = 0;		/* failure */
    LO(bx) = 0x80;		/* function not implemented */

  }
}

static int
FindFreeHandle(int start)
{
  int i, h = 0;

  /* first free handle is 1 */
  for (i = start; (i <= NUM_HANDLES) && (h == 0); i++) {
    if (!handles[i].valid) {
      x_printf("\nXMS: found free handle: %d\n", i);
      h = i;
      break;
    }
    else
      x_printf("XMS: unfree handle %d ", i);
  }
  return h;
}

static int
ValidHandle(unsigned short h)
{
  if ((h <= NUM_HANDLES) && (handles[h].valid))
    return 1;
  else
    return 0;
}

static void
xms_query_freemem(int api)
{
  unsigned long totalBytes = 0, subtotal;
  int h;

  /* the new XMS api should actually work with the function as it
   * stands...
   */
  if (api == NEWXMS)
    x_printf("XMS: new XMS API query_freemem()!\n");

  /* total XMS mem and largest block under DOSEMU will always be the
   * same, thanks to the wonderful operating environment
   */

  totalBytes = 0;
  for (h = FIRST_HANDLE; h <= NUM_HANDLES; h++) {
    if (ValidHandle(h))
      totalBytes += handles[h].size;
  }

  subtotal = config.xms_size - (totalBytes / 1024);
  /* total free is max allowable XMS - the number of K already allocated */

  if (api == OLDXMS) {
    /* old XMS API uses only AX, while new API uses EAX. make
       * sure the old API sees 64 MB if the count >= 64 MB (instead of seeing
       * just the low 16 bits of some number > 65535 (KBytes) which is
       * probably < 64 MB...this makes sure that old API functions at least
       * know of 64MB of the > 64MB available memory.
       */

    if (subtotal > 65535)
      subtotal = 65535;
    LWORD(eax) = LWORD(edx) = subtotal;
    x_printf("XMS query free memory(old): %dK %dK\n", LWORD(eax),
	     LWORD(edx));
  }
  else {
    REG(eax) = REG(edx) = subtotal;
    x_printf("XMS query free memory(new): %ldK %ldK\n",
	     REG(eax), REG(edx));
  }

  LO(bx) = 0;			/* no error */
}

static void
xms_allocate_EMB(int api)
{
  unsigned long h;

  if (api == OLDXMS)
    x_printf("XMS alloc EMB(old) size 0x%04x\n", LWORD(edx));
  else
    x_printf("XMS alloc EMB(new) size 0x%08lx\n", REG(edx));

  if (!(h = FindFreeHandle(FIRST_HANDLE))) {
    x_printf("XMS: out of handles\n");
    LWORD(eax) = 0;
    LO(bx) = 0xa1;
  }
  else {
    handles[h].num = h;
    handles[h].valid = 1;
    if (api == OLDXMS)
      handles[h].size = (long)(LWORD(edx) * 1024);
    else
      handles[h].size = (long)(REG(edx) * 1024);

    x_printf("XMS: EMB size %l\n", (int) handles[h].size);

    /* I could just rely on the behavior of malloc(0) here, but
       * I'd rather not.  I'm going to interpret the XMS 3.0 spec
       * to mean that reserving a handle of size 0 gives it no address
       */
    if (handles[h].size)
      handles[h].addr = malloc((long)handles[h].size);
    else {
      x_printf("XMS WARNING: allocating 0 size EMB\n");
      handles[h].addr = 0;
    }

    handles[h].lockcount = 0;
    handle_count++;

    x_printf("XMS: allocated EMB %lu at %p\n", h, (void *) handles[h].addr);

    if (api == OLDXMS)
      LWORD(edx) = h;		/* handle # */
    else
      REG(edx) = h;

    LWORD(eax) = 1;		/* success */
  }
}

static void
xms_free_EMB(void)
{
  unsigned short int h = LWORD(edx);

  if (!ValidHandle(h)) {
    x_printf("XMS: free EMB bad handle %d\n", h);
    LWORD(eax) = 0;
    LO(bx) = 0xa2;
  }
  else {

    if (handles[h].addr)
      free(handles[h].addr);
    else
      x_printf("XMS WARNING: freeing handle w/no address, size 0x%08lx\n",
	       handles[h].size);
    handles[h].valid = 0;
    handle_count--;

    x_printf("XMS: free'd EMB %d\n", h);
    LWORD(eax) = 1;
  }
}

static void
xms_move_EMB(void)
{
  char *src, *dest;
  struct EMM e = get_emm(REG(ds), LWORD(esi));

  x_printf("XMS move extended memory block\n");
  show_emm(e);
  LWORD(eax) = 1;		/* start with success */

  if (e.SourceHandle == 0) {
    src = (char *) (((e.SourceOffset >> 16) << 4) + \
		    (e.SourceOffset & 0xffff));
  }
  else {
    if (handles[e.SourceHandle].valid == 0)
      error("XMS: invalid source handle\n");
    src = handles[e.SourceHandle].addr + e.SourceOffset;
    if (((unsigned long int)handles[e.SourceHandle].addr + handles[e.SourceHandle].size) <
        ((unsigned long int)src + e.Length) ) {
      e.Length = (unsigned long int)handles[e.SourceHandle].addr + handles[e.SourceHandle].size - (unsigned long int)src;
      LWORD(eax) = 0;
      LO(bx) = 0xa7;		/* invalid Length */
    }
  }

  if (e.DestHandle == 0) {
    dest = (char *) (((e.DestOffset >> 16) << 4) + \
		     (e.DestOffset & 0xffff));
  }
  else {
    if (handles[e.DestHandle].valid == 0)
      error("XMS: invalid dest handle\n");
    dest = handles[e.DestHandle].addr + e.DestOffset;
    if (((unsigned long int)handles[e.DestHandle].addr + handles[e.DestHandle].size) <
        ((unsigned long int)dest + e.Length) ) {
      e.Length = (unsigned long int)handles[e.DestHandle].addr + handles[e.DestHandle].size - (unsigned long int)dest;
      LWORD(eax) = 0;
      LO(bx) = 0xa7;		/* invalid Length */
    }
  }

  x_printf("XMS: block move from %p to %p len 0x%lx\n",
	   (void *) src, (void *) dest, e.Length);

  memmove(dest, src, e.Length);

  x_printf("XMS: block move done\n");

  return;
}

static void
xms_lock_EMB(int flag)
{
  int h = LWORD(edx);

  if (flag)
    x_printf("XMS lock EMB %d\n", h);
  else
    x_printf("XMS unlock EMB %d\n", h);

  if (ValidHandle(h)) {
    /* flag=1 locks, flag=0 unlocks */

    if (flag)
      handles[h].lockcount++;
    else
      if (handles[h].lockcount)
        handles[h].lockcount--;
      else {
        x_printf("XMS: Unlock handle %d already at 0\n", h);
        LWORD(eax) = 0;
        LO(bx) = 0xaa;		/* Block is not locked */
	return;
      }

    LWORD(edx) = (int) handles[h].addr >> 16;
    LWORD(ebx) = (int) handles[h].addr & 0xffff;

    LWORD(eax) = 1;
  }
  else {
    x_printf("XMS: invalid handle %d, can't (un)lock\n", h);
    LWORD(eax) = 0;
    LO(bx) = 0xa2;		/* invalid handle */
  }
}

static void
xms_EMB_info(int api)
{
  int h = LWORD(edx);

  if (ValidHandle(h)) {
    if (api == OLDXMS) {
      HI(bx) = handles[h].lockcount;
      LO(bx) = NUM_HANDLES - handle_count;
      LWORD(edx) = handles[h].size / 1024;
      LWORD(eax) = 1;
      x_printf("XMS Get EMB info(old) %d\n", h);
    }
    else {			/* api == NEWXMS */
      HI(bx) = handles[h].lockcount;
      LWORD(ecx) = NUM_HANDLES - handle_count;
      REG(edx) = handles[h].size / 1024;
      LWORD(eax) = 1;
      x_printf("XMS Get EMB info(new) %d\n", h);
    }
  }
  else {
    LWORD(eax) = 0;
    LO(bx) = 0xa2;		/* invalid handle */
  }
}

/* untested! if you test it, please tell me! */
static void
xms_realloc_EMB(int api)
{
  int h;
  unsigned long oldsize;
  unsigned char *oldaddr;

  h = LWORD(edx);

  if (!ValidHandle(h)) {
    x_printf("XMS: invalid handle %d, cannot realloc\n", h);

    LO(bx) = 0xa2;
    LWORD(eax) = 0;
    return;
  }
  else if (handles[h].lockcount) {
    x_printf("XMS: handle %d locked (%d), cannot realloc\n",
	     h, handles[h].lockcount);

    LO(bx) = 0xab;
    LWORD(eax) = 0;
    return;
  }

  oldsize = handles[h].size;
  oldaddr = handles[h].addr;

  /* BX is the 16-bit new size in KBytes for the old API, but the new
   * XMS 3.0 32-bit API uses EBX
   */
  if (api == OLDXMS)
    handles[h].size = LWORD(ebx) * 1024;
  else
    handles[h].size = REG(ebx) * 1024;

  x_printf((api == OLDXMS) ? "XMS realloc EMB(old) %d to size 0x%04x\n" :
	   "XMS realloc EMB(new) %d to size 0x%08x\n",
	   h, handles[h].size);

  handles[h].addr = malloc(handles[h].size);

  /* memcpy() the old data into the new block, but only
   * min(new,old) bytes.
   */
  memcpy(handles[h].addr, oldaddr,
	 (oldsize <= handles[h].size) ? oldsize : handles[h].size);

  /* free the EMB's old Linux memory */
  free(oldaddr);

  LWORD(eax) = 1;		/* success */
}

void
xms_int15(void)
{
  if (HI(ax) == 0x88) {
    NOCARRY;
    if (xms_grab_int15) {
      x_printf("XMS int 15 free mem returning 0 to protect HMA\n");
      LWORD(eax) &= ~0xffff;
    }
    else {
      x_printf("XMS early int15 call...%dK free\n", config.xms_size);
      LWORD(eax) = config.xms_size;
    }
  }
  else {			/* AH = 0x87 */
    x_printf("XMS int 15 block move failed AX=0x%04x\n", (u_short)REG(eax));
    LWORD(eax) &= 0xFF;
    HI(ax) = 3;			/* say A20 gate failed */
    CARRY;
  }
}

struct EMM
get_emm(unsigned int seg, unsigned int off)
{
  struct EMM e;
  unsigned char *p;

  p = (char *) ((seg << 4) + off);

#if 0
  e.Length = *(unsigned long *) p;
  p += 4;
  e.SourceHandle = *(unsigned short *) p;
  p += 2;
  e.SourceOffset = *(unsigned long *) p;
  p += 4;
  e.DestHandle = *(unsigned short *) p;
  p += 2;
  e.DestOffset = *(unsigned long *) p;
#else
  e.Length = *(unsigned int *) p;
  p += 4;
  e.SourceHandle = *(unsigned short *) p;
  p += 2;
  e.SourceOffset = *(unsigned int *) p;
  p += 4;
  e.DestHandle = *(unsigned short *) p;
  p += 2;
  e.DestOffset = *(unsigned int *) p;
#endif

  return e;
}

void
show_emm(struct EMM e)
{
  x_printf("XMS show_emm:\n");

  x_printf("L: 0x%08lx\n"
	   "SH: 0x%04x  SO: 0x%08lx\n"
	   "DH: 0x%04x  DO: 0x%08lx\n",
	   e.Length, e.SourceHandle, e.SourceOffset,
	   e.DestHandle, e.DestOffset);
}

