# Makefile for Linux DOS emulator
#
# $Date: 1994/08/02 00:31:45 $
# $Source: /home/src/dosemu0.60/RCS/Makefile,v $
# $Revision: 2.19 $
# $State: Exp $
#

# You should do a make config, make dep, make clean if you're doing
# the first compile. 

#Change the following line if the right kernel includes reside elsewhere
LINUX_INCLUDE = /usr/src/linux/include

#Change the following line to point to your ncurses include
NCURSES_INC = /usr/include/ncurses

#ifdef DEBUG
#STATIC=1
#DOSOBJS=$(OBJS)
#SHLIBOBJS=
#DOSLNK=-lncurses -lipc
#CDEBUGOPTS=-g -DSTATIC=1
#LNKOPTS=
#else
STATIC=0
DOSOBJS=
SHLIBOBJS=$(OBJS)
DOSLNK=
#LNKOPTS=-s
#MAGIC=-zmagic
#endif

X_SUPPORT = 1
export X_SUPPORT

ifdef X_SUPPORT
XCFILES = 
XOBJS   =
#the -u forces the X11 shared library to be linked into ./dos
XLIBS   = -lX11 -u _XOpenDisplay
XDEFS   = -DX_SUPPORT
endif

# dosemu version
EMUVER  =   0.53
VERNUM  =   0x53
PATCHL  =   1

# DON'T CHANGE THIS: this makes libdosemu start high enough to be safe. 
# should be okay at...0x20000000 for .5 GB mark.
LIBSTART = 0x20000000

ENDOFDOSMEM = 0x110000     # 1024+64 Kilobytes

DPMIOBJS = dpmi/dpmi.o dpmi/call.o

# For testing the internal IPX code
# IPX = ipxutils

#
# SYNC_ALOT
#  uncomment this if the emulator is crashing your machine and some debug info
# isn't being sync'd to the debug file (stdout). shouldn't happen. :-)
# SYNC_ALOT = -DSYNC_ALOT=1

CONFIG_FILE = -DCONFIG_FILE=\"/etc/dosemu.conf\"

###################################################################

ifdef DPMIOBJS
DPMISUB= dpmi
else
DPMISUB=
endif

###################################################################
#
#  Section for Client areas (why not?)
#
###################################################################

CLIENTSSUB=clients

SUBDIRS= periph video mouse include boot commands drivers \
	$(DPMISUB) $(CLIENTSSUB) timer init net $(IPX) kernel \
	examples

DOCS= doc

CFILES=cmos.c dos.c emu.c termio.c xms.c disks.c keymaps.c mutex.c \
	timers.c dosio.c cpu.c  mfs.c bios_emm.c lpt.c \
        serial.c dyndeb.c sigsegv.c detach.c

HFILES=cmos.h emu.h termio.h timers.h xms.h dosio.h \
        cpu.h mfs.h disks.h memory.h machcompat.h lpt.h \
        serial.h mutex.h int.h int10.h ports.h

SFILES=bios.S

OFILES= Makefile ChangeLog dosconfig.c QuickStart \
	DOSEMU-HOWTO.txt DOSEMU-HOWTO.ps DOSEMU-HOWTO.sgml \
	README.ncurses vga.pcf xdosemu xinstallvgafont README.X

BFILES=

F_DOC=dosemu.texinfo Makefile dos.1 wp50
F_DRIVERS=emufs.S emufs.sys
F_COMMANDS=exitemu.S exitemu.com vgaon.S vgaon.com vgaoff.S vgaoff.com \
            lredir.exe lredir.c makefile.mak dosdbg.exe dosdbg.c
F_EXAMPLES=config.dist
F_PERIPH=debugobj.S getrom hdinfo.c mkhdimage.c mkpartition putrom.c 
 

###################################################################

OBJS=emu.o termio.o disks.o keymaps.o timers.o cmos.o mouse.o \
     dosio.o cpu.o xms.o mfs.o bios_emm.o lpt.o \
     serial.o dyndeb.o sigsegv.o video.o bios.o init.o net.o detach.o $(XOBJS)

OPTIONAL   = # -DDANGEROUS_CMOS=1
CONFIGS    = $(CONFIG_FILE)
DEBUG      = $(SYNC_ALOT)
CONFIGINFO = $(CONFIGS) $(OPTIONAL) $(DEBUG) \
	     -DLIBSTART=$(LIBSTART) -DVERNUM=$(VERNUM) -DVERSTR=\"$(EMUVER)\" \
	     -DPATCHSTR=\"$(PATCHL)\"

CC         =   gcc # I use gcc-specific features (var-arg macros, fr'instance)
COPTFLAGS  = -N -s -O2 -funroll-loops

# -Wall -fomit-frame-pointer # -ansi -pedantic -Wmissing-prototypes -Wstrict-prototypes
 
ifdef DPMIOBJS
DPMI = -DDPMI
else
DPMI = 
endif

TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
INCDIR     = -I$(TOPDIR)/include -I$(TOPDIR) -I$(LINUX_INCLUDE) -I$(NCURSES_INC)
export INCDIR
CFLAGS     = $(DPMI) $(XDEFS) $(CDEBUGOPTS) $(COPTFLAGS) $(INCDIR)
LDFLAGS    = $(LNKOPTS) # exclude symbol information
AS86 = as86
#LD86 = ld86 -s -0
LD86 = ld86 -0

DISTBASE=/tmp
DISTNAME=dosemu$(EMUVER)
DISTPATH=$(DISTBASE)/$(DISTNAME)
DISTFILE=$(DISTBASE)/$(DISTNAME).tgz

warning: warning2
	@echo "To compile DOSEMU, type 'make doeverything'"
	@echo ""
	
warning2: 
	@echo ""
	@echo "IMPORTANT: Please read the new 'QuickStart' file before compiling DOSEMU!"
	@echo "The location and format of DOSEMU files have changed since 0.50pl1 release!"
	@echo "You need gcc 2.4.5, lib 4.4.4, linux 1.1.12 (or patched linux) and at least"
	@echo "16MB total RAM+swap to compile DOSEMU."
	@echo ""
	@sleep 10

warning3:
	@echo ""
	@echo "Be patient...This may take a while to complete, especially for 'mfs.c'."
	@echo "Hopefully you have at least 16MB RAM+swap available during this compile."
	@echo ""

doeverything: warning2 config dep installnew docsubdirs
most: config dep installnew

all:	warnconf dos dossubdirs warning3 libdosemu

.EXPORT_ALL_VARIABLES:

debug:
	rm dos
	make dos

config.h: Makefile
ifeq (config.h,$(wildcard config.h))
	@echo "WARNING: Your Makefile has changed since config.h was generated."
	@echo "         Consider doing a 'make config' to be safe."
else
	@echo "WARNING: You have no config.h file in the current directory."
	@echo "         Generating config.h..."
	make config
endif

warnconf: config.h

dos.o: config.h dos.c
	$(CC) -DSTATIC=$(STATIC) -c dos.c

dos:	dos.c $(DOSOBJS)
	@echo "Including dos.o " $(DOSOBJS)
	$(CC) $(DOSLNK) -DSTATIC=$(STATIC) $(LDFLAGS) -N -o $@ $< $(DOSOBJS) \
              $(XLIBS)

libdosemu:	$(SHLIBOBJS) $(DPMIOBJS)
	ld $(LDFLAGS) $(MAGIC) -T $(LIBSTART) -o $@ \
	   $(SHLIBOBJS) $(DPMIOBJS) $(SHLIBS) $(XLIBS) -lncurses -lc

dossubdirs: dummy
	@for i in $(SUBDIRS); do \
	    (cd $$i && echo $$i && $(MAKE)) || exit; \
	done

docsubdirs: dummy
	@for i in $(DOCS); do \
	    (cd $$i && echo $$i && $(MAKE)) || exit; \
	done

config: dosconfig
	@./dosconfig $(CONFIGINFO) > config.h

installnew: dummy
	$(MAKE) install

install: all
	install -c -o root -m 04755 dos /usr/bin
	install -m 0755 xdosemu /usr/bin
	@if [ -f /lib/libemu ]; then rm -f /lib/libemu ; fi
	install -m 0755 libdosemu /usr/lib
	install -d /var/lib/dosemu
	nm libdosemu | grep -v '\(compiled\)\|\(\.o$$\)\|\( a \)' | \
		sort > dosemu.map
	@for i in $(SUBDIRS); do \
	    (cd $$i && echo $$i && $(MAKE) install) || exit; \
	done
	@echo ""
	@echo "Remember to copy examples/config.dist into /etc/dosemu.conf and edit it!"
	@echo ""

converthd: hdimage
	mv hdimage hdimage.preconvert
	periph/mkhdimage -h 4 -s 17 -c 40 | cat - hdimage.preconvert > hdimage
	@echo "Your hdimage is now converted and ready to use with 0.52!"

newhd: periph/bootsect
	periph/mkhdimage -h 4 -s 17 -c 40 | cat - periph/bootsect > newhd
	@echo "You now have a hdimage file called 'newhd'"

checkin:
	-ci $(CFILES) $(HFILES) $(SFILES) $(OFILES)
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) checkin) || exit; done

checkout:
	-co -l $(CFILES) $(HFILES) $(SFILES) $(OFILES)
	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) checkout) || exit; done

dist: $(CFILES) $(HFILES) $(SFILES) $(OFILES) $(BFILES)
	install -d $(DISTPATH)
	install -m 0644 $(CFILES) $(HFILES) $(SFILES) $(OFILES) $(BFILES) .depend $(DISTPATH)
	cp TODO $(DISTPATH)/.todo
	cp TODO.JES $(DISTPATH)/.todo.jes
	cp .indent.pro $(DISTPATH)/.indent.pro
	install -m 0644 hdimages/hdimage.dist $(DISTPATH)/hdimage.dist
ifdef DPMIOBJS
	@for i in $(SUBDIRS) $(DOCS); do \
	    (cd $$i && echo $$i && $(MAKE) dist) || exit; \
	done
else
	@for i in $(SUBDIRS) $(DOCS) dpmi; do \
	    (cd $$i && echo $$i && $(MAKE) dist) || exit; \
	done
endif
ifdef IPX
	@for i in $(SUBDIRS) $(DOCS); do \
	    (cd $$i && echo $$i && $(MAKE) dist) || exit; \
	done
else
	@for i in $(SUBDIRS) $(DOCS) ipxutils; do \
	    (cd $$i && echo $$i && $(MAKE) dist) || exit; \
	done
endif
	(cd $(DISTBASE); tar cf - $(DISTNAME) | gzip -9 >$(DISTFILE))
	rm -rf $(DISTPATH)
	@echo "FINAL .tgz FILE:"
	@ls -l $(DISTFILE) 

clean:
	rm -f $(OBJS) dos libdosemu *.s core config.h .depend \
	      dosconfig dosconfig.o *.tmp
	@for i in $(SUBDIRS); do \
             (cd $$i && echo $$i && $(MAKE) clean) || exit; \
        done


depend dep: 
	$(CPP) -MM $(CFLAGS) *.c > .depend ;echo "bios.o : bios.S" >>.depend
	cd clients;$(CPP) -MM -I../ -I../include $(CFLAGS) *.c > .depend
	cd video; make depend
	cd mouse; make depend
	cd timer; make depend
	cd init; make depend
	cd net; make depend
ifdef IPX
	cd ipxutils; make depend
endif
ifdef DPMIOBJS
	cd dpmi;$(CPP) -MM -I../ -I../include $(CFLAGS) *.c > .depend;echo "call.o : call.S" >>.depend
endif

dummy:
#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
