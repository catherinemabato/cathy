/*
 * Extensions by Robert Sanders, 1992-93
 * 
 * DANG_BEGIN_MODULE
 * 
 * Here is where DOSEMU gets booted. From emu.c external calls are made to
 * the specific I/O systems (video/keyboard/serial/etc...) to initialize
 * them. Memory is cleared/set up and the boot sector is read from the
 * boot drive. Many SIGNALS are set so that DOSEMU can exploit things like
 * timers, I/O signals, illegal instructions, etc... When every system
 * gives the green light, vm86() is called to switch into vm86 mode and
 * start executing i86 code.
 * 
 * The vm86() function will return to DOSEMU when certain `exceptions` occur
 * as when some interrupt instructions occur (0xcd).
 * 
 * The top level function emulate() is called from dos.c by way of a dll
 * entry point.
 * 
 * DANG_END_MODULE
 * 
 * DANG_BEGIN_CHANGELOG 
 * $Date: 1995/05/06 16:25:14 $ $Source: /home/src/dosemu0.60/RCS/emu.c,v $ $Revision: 2.38 $ $State: Exp $
 * 
 * $Log: emu.c,v $
 * 
 * DANG_END_CHANGELOG
 */


/*
 * DANG_BEGIN_REMARK 
 * DOSEMU must not work within the 1 meg DOS limit, so
 * start of code is loaded at a higher address, at some time this could
 * conflict with other shared libs. If DOSEMU is compiled statically
 * (without shared libs), and org instruction is used to provide the jump
 * above 1 meg. 
 * DANG_END_REMARK
 */

#ifdef __NetBSD__
#define __ELF__				/* simluated with a.out mmap-ing stuff.
					   use _main entrypoint. */
#define EDEADLOCK EDEADLK
#endif

#ifndef __ELF__
/*
 * DANG_BEGIN_FUNCTION jmp_emulate
 * 
 * description: This function allows the startup program `dos` to know how to
 * call the emulate function by way of the dll headers. Always make sure
 * that this line is the first of emu.c and link emu.o as the first object
 * file to the lib
 * 
 * DANG_END_FUNCTION
 */
__asm__("___START___: jmp _emulate\n");
#endif

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#ifndef EDEADLOCK
  #define EDEADLOCK EDEADLK
#endif
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#ifndef __NetBSD__
#include <getopt.h>
#endif
#include <assert.h>

#ifdef __NetBSD__
#include <signal.h>
#include <machine/pcvt_ioctl.h>
#include "netbsd_vm86.h"
#endif
#ifdef __linux__
#include <linux/vt.h>
#include <linux/fd.h>
#include <linux/hdreg.h>
#include <sys/vm86.h>
#include <syscall.h>
#endif

#include "config.h"
#include "memory.h"

#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif

#ifdef REQUIRES_EMUMODULE
  /* Please folks, don't remove this, it's required for emusys.h (within emu.h) */
  #define __EMUSYS_parent
#endif
#include "emu.h"

#include "bios.h"
#include "termio.h"
#include "video.h"
#include "timers.h"
#include "cmos.h"
#include "mouse.h"
#include "dosio.h"
#include "disks.h"
#include "xms.h"
#include "hgc.h"
#include "ipx.h"		/* TRB - add support for ipx */
#include "serial.h"
#include "keymaps.h"
#include "int.h"
#include "bitops.h"
#include "pic.h"
#include "dpmi.h"
#ifdef __NetBSD__
#include <setjmp.h>
#endif

extern void     stdio_init(void);
extern void     time_setting_init(void);
extern void     tmpdir_init(void);
extern void     low_mem_init(void);

extern void     shared_memory_exit(void);
extern void     restore_vt(u_short);
extern void     disallocate_vt(void);
extern void     keyboard_close(void);
extern void     vm86_GP_fault();
extern void     config_init(int argc, char **argv);
extern void	timer_int_engine(void);
extern void	disk_open(struct disk *dp);

extern void io_select_init(void);

static int      special_nowait = 0;

void 
boot(void)
{
    char           *buffer;
    struct disk    *dp = NULL;

    switch (config.hdiskboot) {
    case 0:
	if (config.bootdisk)
	    dp = &bootdisk;
	else
	    dp = &disktab[0];
	break;
    case 1:
	dp = &hdisktab[0];
	break;
    case 2:
	dp = &disktab[1];
	break;
    default:
	error("ERROR: unexpected value for config.hdiskboot\n");
	leavedos(15);
    }

    ignore_segv++;

    disk_close();
    disk_open(dp);

    buffer = (char *) 0x7c00;

    if (dp->boot_name) {/* Boot from the specified file */
        int bfd;
        d_printf ("Booting from bootfile=%s...\n",dp->boot_name);
        bfd = open (dp->boot_name, O_RDONLY);
        if (bfd == -1) {/* Abort with error */
            error("ERROR: Boot file %s missing\n",dp->boot_name);
            leavedos(16);
        }
        if (read(bfd, buffer, SECTOR_SIZE) != SECTOR_SIZE) {
            error("ERROR: Failed to read exactly %d bytes from %s\n",
                  SECTOR_SIZE, dp->boot_name);
            leavedos(16);
        }
        close(bfd);
    }
    else
    if (dp->type == PARTITION) {/* we boot partition boot record, not MBR! */
	d_printf("Booting partition boot record from part=%s....\n", dp->dev_name);
	if (RPT_SYSCALL(read(dp->fdesc, buffer, SECTOR_SIZE)) != SECTOR_SIZE) {
	    error("ERROR: reading partition boot sector using partition %s.\n", dp->dev_name);
	    leavedos(16);
	}
    } else if (read_sectors(dp, buffer, 0, 0, 0, 1) != SECTOR_SIZE) {
	error("ERROR: can't boot from %s, using harddisk\n", dp->dev_name);
	dp = hdisktab;
	if (read_sectors(dp, buffer, 0, 0, 0, 1) != SECTOR_SIZE) {
	    error("ERROR: can't boot from hard disk\n");
	    leavedos(16);
	}
    }
    disk_close();
    ignore_segv--;
}

/* Silly Interrupt Generator Initialization/Closedown */

#ifdef SIG
SillyG_t       *SillyG = 0;
static SillyG_t SillyG_[16 + 1];
#endif

/*
 * DANG_BEGIN_FUNCTION SIG_int
 * 
 * description: Allow DOSEMU to be made aware when a hard interrupt occurs
 * Requires the sig/sillyint.o driver loaded (using NEW modules package),
 * or a kernel patch (implementing sig/int.c driver).
 * 
 * The IRQ numbers to monitor are taken from config.sillyint, each bit
 * corresponding to one IRQ. The higher 16 bit are defining the use of
 * SIGIO
 * 
 * DANG_END_FUNCTION
 */
static inline void 
SIG_init()
{
#ifdef SIG
    /* Get in touch with my Silly Interrupt Driver */
    if (config.sillyint) {
#ifndef REQUIRES_EMUMODULE
	char            devname[20];
#endif
	char            prio_table[] =
	{9, 10, 11, 12, 14, 15, 3, 4, 5, 6, 7};
	int             i,
#ifndef REQUIRES_EMUMODULE
	                fd,
#endif
	                irq;
	SillyG_t       *sg = SillyG_;
	for (i = 0; i < sizeof(prio_table); i++) {
	    irq = prio_table[i];
	    if (config.sillyint & (1 << irq)) {
#ifdef REQUIRES_EMUMODULE
		if (emusyscall(EMUSYS_REQUEST_IRQ, irq) < 0) {
		    g_printf("Not gonna touch IRQ %d you requested!\n", irq);
		} else {
		    g_printf("Gonna monitor the IRQ %d you requested\n", irq);
		    sg->fd = -1;
#else
		sprintf(devname, "/dev/int/%d", irq);
		if ((fd = open(devname, O_RDWR)) < 1) {
		    g_printf("Not gonna touch IRQ %d you requested!\n", irq);
		} else {
		    /* Reset interupt incase it went off already */
		    RPT_SYSCALL(write(fd, NULL, (int) NULL));
		    g_printf("Gonna monitor the IRQ %d you requested, Return=0x%02x\n", irq, fd);
		    if (config.sillyint & (0x10000 << irq)) {
			/* Use SIGIO, this should be faster */
			add_to_io_select(fd, 1);
		    }
		    /*
		     * DANG_BEGIN_REMARK 
		     * At this time we have to use
		     * SIGALRM in addition to SIGIO I don't (yet) know why
		     * the SIGIO signal gets lost sometimes (once per
		     * minute or longer). But if it happens, we can
		     * retrigger this way over SIGALRM. Normally SIGIO
		     * happens before SIGALARM, so nothing hurts. (Hans)
		     * DANG_END_REMARK
		     */
#if 0
		    else
#endif
		    {
			/* use SIGALRM  */
			add_to_io_select(fd, 0);
		    }
		    sg->fd = fd;
#endif				/* NOT REQUIRES_EMUMODULE */
		    sg->irq = irq;
		    g_printf("SIG: IRQ%d, enabling PIC-level %ld\n", irq, pic_irq_list[irq]);
#ifdef REQUIRES_EMUMODULE
		    { extern int SillyG_do_irq(void);
		    pic_seti(pic_irq_list[irq], SillyG_do_irq, 0);
		    }
#else
		    pic_seti(pic_irq_list[irq], do_irq, 0);
#endif
		    pic_unmaski(pic_irq_list[irq]);
		    sg++;
		}
	    }
	}
	sg->fd = 0;
	if (sg != SillyG_)
	    SillyG = SillyG_;
    }
#endif
}

static inline void 
SIG_close()
{
#ifdef SIG
    if (SillyG) {
	SillyG_t       *sg = SillyG;
#ifdef REQUIRES_EMUMODULE
	while (sg->fd) {
	    emusyscall(EMUSYS_FREE_IRQ, sg->irq);
	    sg++;
	}
#else
	while (sg->fd)
	    close((sg++)->fd);
#endif
	g_printf("Closing all IRQ you opened!\n");
    }
#endif
}

static inline void 
emumodule_init(void)
{
#ifdef REQUIRES_EMUMODULE
    resolve_emusyscall();
    if (EMUSYS_AVAILABLE) {
	if (emusyscall(EMUSYS_GETVERSION, 0) >= EMUSYSVERSION) return;
    }
    fprintf(stderr, "emumodule not loaded or wrong version\n\r");
    fflush(stdout);
    fflush(stderr);
    _exit(1);
#endif
}

static inline void
module_init(void)
{
    version_init();		/* Check the OS version */
    emumodule_init();		/* emumodule support */
    SIG_init();			/* silly int generator support */
    memcheck_init();		/* lower 1M memory map support */
    tmpdir_init();		/* create our temporary dir */
}

#ifdef __NetBSD__
#include <machine/segments.h>
/*
 * Switch all segment registers to use well-known GDT entries.
 * (The default process setup uses LDT entries for all segment registers)
 */
static u_short csel = GSEL(GUCODE_SEL, SEL_UPL);
asm(".text");
asm(".align 4");
asm(".globl changesegs_lret");
asm("changesegs_lret:");
asm("popl %eax");
asm("pushl _csel");
asm("pushl %eax");
asm("lret");

void
changesegs()
{
    register u_short dsel = GSEL(GUDATA_SEL, SEL_UPL);
    u_long retaddr;

    asm("pushl %0; popl %%ds" : : "g" (dsel) );
    asm("pushl %0; popl %%es" : : "g" (dsel) );
    asm("movl %0,%%fs" : : "r" (dsel) );
    asm("movl %0,%%gs" : : "r" (dsel) );
    asm("movl %0,%%ss" : : "r" (dsel) );
    asm("call changesegs_lret");
    return;
}
#endif

/*
 * DANG_BEGIN_FUNCTION emulate
 * 
 * arguments: 
 * argc - Argument count. 
 * argv - Arguments.
 * 
 * description: 
 * Emulate gets called from dos.c. It initializes DOSEMU to
 * prepare it for running in vm86 mode. This involves catching signals,
 * preparing memory, calling all the initialization functions for the I/O
 * subsystems (video/serial/etc...), getting the boot sector instructions
 * and calling vm86().
 * 
 * DANG_END_FUNCTION
 * 
 */
#ifdef __ELF__
void 
main(int argc, char **argv)
#else
void 
emulate(int argc, char **argv)
#endif
{
#ifdef __NetBSD__
    changesegs();
#endif
    if (0 == geteuid()) {
	warn("I am root\n");
	i_am_root = 1;
    } else {
	if (getuid() != geteuid()) {
	    fprintf(stderr, "Can't setuid to anyone else but root\n");
	    exit(1);
	}
    }

#ifdef RUN_AS_USER
    /* start running as real, not effective user.  This is moved from */
    /* stdio_init, as that and config_init have now changed places and */
    /* allowing reading arbitrary files while being root is not smart */

    priv_off();	
#endif

    /* the transposal of (config_|stdio_)init allows the addition of -o */
    /* to specify a debug out filename, if you're wondering */

    io_select_init();
    config_init(argc, argv);	/* parse the commands & config file(s) */
    stdio_init();		/* initialize stdio & open debug file */
    module_init();
    low_mem_init();		/* initialize the lower 1Meg */
    time_setting_init();	/* get the startup time */
    signal_init();		/* initialize sig's & sig handlers */
    device_init();		/* initialize keyboard, disk, video, etc. */
    cpu_setup();		/* setup the CPU */
    hardware_setup();		/* setup any hardware */
    memory_init();		/* initialize the memory contents */
    boot();			/* read the boot sector & get moving */
    timer_interrupt_init();	/* start sending int 8h int signals */

    if (not_use_sigio)
	k_printf("Atleast 1 NON-SIGIO file handle in use.\n");
    else
	k_printf("No NON-SIGIO file handles in use.\n");
    g_printf("EMULATE\n");

    fflush(stdout);

#ifdef USE_MHPDBG  
    mhp_debug(DBG_INIT, 0, 0);
#endif

    while (!fatalerr) {
	++pic_vm86_count;
	run_vm86();
#if 0
	timer_int_engine();
#endif
	serial_run();
	/*run_irqs();*/ pic_run();		/* trigger any hardware interrupts
				 * requested */
#if 0
#ifdef USING_NET
	/* check for available packets on the packet driver interface */
	/* (timeout=0, so it immediately returns when none are available) */
	pic_request(16);
#endif
#endif
	int_queue_run();
    }

    error("error exit: (%d,0x%04x) in_sigsegv: %d ignore_segv: %d\n",
	  fatalerr, fatalerr, in_sigsegv, ignore_segv);

    sync();
    fprintf(stderr, "Not a good day to die!!!!!\n");
    leavedos(99);
}

extern void HMA_MAP(int);

void
dos_ctrl_alt_del(void)
{
    dbug_printf("DOS ctrl-alt-del requested.  Rebooting!\n");
    HMA_MAP(1);
    time_setting_init();
    keyboard_flags_init();
    video_config_init();
    serial_init();
    mouse_init();
    printer_init();
    disk_close();
    disk_init();
    scr_state_init();
    clear_screen(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE), 7);
    special_nowait = 0;
    p_dos_str("Rebooting DOS.  Be careful...this is partially implemented\r\n");
    disk_init();
    cpu_setup();
    hardware_setup();
    memcheck_init();		/* lower 1M memory map support */
    memory_init();
    boot();
    timer_interrupt_init();	/* start sending int 8h int signals */
}

void
dos_ctrlc(void)
{
    k_printf("DOS ctrl-c!\n");
    p_dos_str("^C\n\r");	/* print ctrl-c message */
    keybuf_clear();

    do_soft_int(0x23);
}

static void
ign_sigs(int sig)
{
    static int      timerints = 0;
    static int      otherints = 0;

    g_printf("ERROR: signal %d received in leavedos()\n", sig);
    show_regs(__FILE__, __LINE__);
    if (sig == SIG_TIME)
	timerints++;
    else
	otherints++;

#define LEAVEDOS_TIMEOUT (3 * FREQ)
#define LEAVEDOS_SIGOUT  5
    if ((timerints >= LEAVEDOS_TIMEOUT) || (otherints >= LEAVEDOS_SIGOUT)) {
	error("ERROR: timed/signalled out in leavedos()\n");
	exit(1);
    }
}

/* "graceful" shutdown */
void
leavedos(int sig)
{
    struct sigaction sa;
    struct itimerval itv;
    extern int errno;

    static int recurse_check = 0;
    if (recurse_check);
    recurse_check = 1;
    priv_on();
#if 1 /* BUG CATCHER */
    if (in_vm86) {
      g_printf("\nkilled while in vm86(), trying to dump DOS-registers:\n");
      show_regs(__FILE__, __LINE__);
    }
#endif
    in_vm86 = 0;

    /* remove tmpdir */
    rmdir(tmpdir);

    itv.it_interval.tv_sec = itv.it_interval.tv_usec = 0;
    itv.it_value = itv.it_interval;
    if (setitimer(TIMER_TIME, &itv, NULL) == -1) {
	g_printf("can't turn off timer at shutdown: %s\n", strerror(errno));
    }
    SETSIG(SIG_TIME, ign_sigs);
    SETSIG(SIGSEGV, ign_sigs);
    SETSIG(SIGILL, ign_sigs);
    SETSIG(SIGFPE, ign_sigs);
    SETSIG(SIGTRAP, ign_sigs);
    warn("leavedos(%d) called - shutting down\n", sig);

    g_printf("calling close_all_printers\n");
    close_all_printers();

    g_printf("releasing ports and blocked devices\n");
    release_ports();

    g_printf("calling serial_close\n");
    serial_close();
    g_printf("calling mouse_close\n");
    mouse_close();

#ifdef SIG
    g_printf("calling SIG_close\n");
#endif
    SIG_close();

    show_ints(0, 0x33);
    g_printf("calling disk_close_all\n");
    disk_close_all();
    g_printf("calling video_close\n");
    video_close();
    g_printf("calling keyboard_close\n");
    fflush(stderr);
    fflush(stdout);
    keyboard_close();

    g_printf("calling shared memory exit\n");
    shared_memory_exit();
    g_printf("calling HMA exit\n");
    hma_exit();
#ifdef USE_MHPDBG
    g_printf("closing debugger pipes\n");
    mhp_close();
#endif
    if (config.detach) {
	restore_vt(config.detach);
	disallocate_vt();
    }
    _exit(sig);
}

#if 0
/* check the fd for data ready for reading */
int
d_ready(int fd)
{
    int selrtn;
    struct timeval  w_time;
    fd_set          checkset;

    w_time.tv_sec = 0;
    w_time.tv_usec = 200000;

    FD_ZERO(&checkset);
    FD_SET(fd, &checkset);

    do {
        selrtn = select(fd + 1, &checkset, NULL, NULL, &w_time);
    } while(selrtn == -1 && errno == EINTR);

    if (selrtn == 1) {
	if (FD_ISSET(fd, &checkset))
	    return (1);
	else
	    return (0);
    } else
	return (0);
}
#endif

static inline   uid_t
be(uid_t who)
{
    if (getuid() != who)
	return setreuid(geteuid(), getuid());
    else
	return 0;
}

static inline   uid_t
be_me(void)
{
    if (geteuid() == 0) {
	return setreuid(geteuid(), getuid());
	return 0;
    } else
	return geteuid();
}

static inline   uid_t
be_root(void)
{
    if (geteuid() != 0) {
	setreuid(geteuid(), getuid());
	return getuid();
    } else
	return 0;
}






void
activate(int con_num)
{
    if (in_ioctl) {
	k_printf("KBD: can't ioctl for activate, in a signal handler\n");
	do_ioctl(kbd_fd, VT_ACTIVATE, con_num);
    } else
	do_ioctl(kbd_fd, VT_ACTIVATE, con_num);
}

#ifdef __NetBSD__
void
usleep(u_int microsecs)
{
    /* system usleep is ghastly inefficient, using SIGALRM.
       Instead, we use select :) */
    struct timeval tv;
    tv.tv_sec = microsecs / 1000000;
    tv.tv_usec = microsecs % 1000000;
    select(0, 0, 0, 0, &tv);
    /* return early if awoken */
    return;
}



#endif
