# Makefile for Linux DOSEMU
#
# $Date: 1995/01/14 15:27:32 $
# $Source: /home/src/dosemu0.60/RCS/Makefile,v $
# $Revision: 2.38 $
# $State: Exp $
#
# You should do a "make doeverything" or a "make most" (excludes TeX)
# if you are doing the first compile.
#

# Want to try SLANG?
USE_SLANG=-DUSE_SLANG
ifdef USE_SLANG
TCNTRL=-lslang
export USE_SLANG
else
TCNTRL=-lncurses
endif

# Eliminate to avoid X
USE_X=1
ifdef USE_X
# Autodetecting the installation of X11. Looks weird, but works...
ifeq (/usr/include/X11/X.h,$(wildcard /usr/include/X11/X.h))
  ifeq (/usr/X11R6/lib/libX11.sa,$(wildcard /usr/X11R6/lib/libX11.sa))
    X11ROOTDIR  = /usr/X11R6
  else
    ifeq (/usr/X386/lib/libX11.sa,$(wildcard /usr/X386/lib/libX11.sa))
      X11ROOTDIR  = /usr/X386
    else
      ifeq (/usr/lib/libX11.sa,$(wildcard /usr/lib/libX11.sa))
        X11ROOTDIR  = /usr/X386
      endif
    endif
  endif
  X11LIBDIR = $(X11ROOTDIR)/lib
  X11INCDIR = $(X11ROOTDIR)/include
endif

endif

#Change the following line if the right kernel includes reside elsewhere
LINUX_KERNEL = /usr/src/linux
LINUX_INCLUDE = $(LINUX_KERNEL)/include
export LINUX_KERNEL
export LINUX_INCLUDE  

#Change the following line to point to your ncurses include
NCURSES_INC = /usr/include/ncurses
export NCURSES_INC

#Change the following line to point to your loadable modules directory
BOOTDIR = /boot/modules

# The following sets up the X windows support for DOSEMU.
ifdef X11LIBDIR
X_SUPPORT  = 1
X2_SUPPORT = 1
#the -u forces the X11 shared library to be linked into ./dos
XLIBS   = -L$(X11LIBDIR) -lX11 -u _XOpenDisplay
XDEFS   = -DX_SUPPORT
endif

ifdef X2_SUPPORT
X2CFILES = x2dos.c
X2CEXE = x2dos xtermdos xinstallvgafont
X2DEFS   = -DX_SUPPORT
endif

export X_SUPPORT
export XDEFS

#  The next lines are for testing the new pic code.  You must do a
#  make clean, make config if you change these lines.
# Uncomment the next line to try new pic code on keyboard and timer only.
NEW_PIC = -DNEW_PIC=1
# Uncomment the next line to try new pic code on keyboard, timer, and serial.
# NOTE:  The serial pic code is known to have bugs.
# NEW_PIC = -DNEW_PIC=2
ifdef NEW_PIC
PICOBJS = libtimer.a
export NEW_PIC
export PICOBJS
endif

# enable this target to make a different way
# do_DEBUG=true
export CC         = gcc  # I use gcc-specific features (var-arg macros, fr'instance)
export LD         = gcc
ifdef do_DEBUG
COPTFLAGS	=  -g
endif

OBJS	= dos.o 
DEPENDS = dos.d emu.d

# set if you want one excutable
# STATIC=1

# dosemu version
EMUVER  =   0.53
export EMUVER
VERNUM  =   0x53
PATCHL  =   40
LIBDOSEMU = libdosemu$(EMUVER)pl$(PATCHL)

# DON'T CHANGE THIS: this makes libdosemu start high enough to be safe. 
# should be okay at...0x20000000 for .5 GB mark.
LIBSTART = 0x20000000

ENDOFDOSMEM = 0x110000     # 1024+64 Kilobytes

CFILES=emu.c dos.c $(X2CFILES)

# For testing the internal IPX code
# IPX = ipxutils

# Change USING_NET to 0 and set NET to nothing to remove all net code.
export USING_NET = 1
export NET = net

# SYNC_ALOT
#   uncomment this if the emulator is crashing your machine and some debug info #   isn't being sync'd to the debug file (stdout). shouldn't happen. :-) #SYNC_ALOT = -DSYNC_ALOT=1 
#SYNC_ALOT = -DSYNC_ALOT=1

CONFIG_FILE = -DCONFIG_FILE=\"/etc/dosemu.conf\" 
DOSEMU_USERS_FILE = -DDOSEMU_USERS_FILE=\"/etc/dosemu.users\"

###################################################################

# Uncomment for DPMI support
# it is for the makefile and also for the C compiler
DPMI=-DDPMI
ifndef NEW_PIC	# I need PIC for DPMI
DPMI=
endif

###################################################################
#
#  Section for Client areas (why not?)
#
###################################################################

CLIENTSSUB=clients

OPTIONALSUBDIRS =examples v-net syscallmgr emumod

LIBSUBDIRS=dosemu timer mfs video init keyboard mouse $(NET) $(IPX) drivers
ifdef DPMI
LIBSUBDIRS+=dpmi
endif

SUBDIRS= periph include boot \
	$(CLIENTSSUB) kernel

REQUIRED=tools bios periph commands

# call all libraries the name of the directory
LIBS=$(LIBSUBDIRS)


DOCS= doc


OFILES= Makefile Makefile.common ChangeLog dosconfig.c QuickStart \
	DOSEMU-HOWTO.txt DOSEMU-HOWTO.ps DOSEMU-HOWTO.sgml \
	NOVELL-HOWTO.txt BOGUS-Notes \
	README.ncurses vga.pcf vga.bdf xtermdos.sh xinstallvgafont.sh README.X \
	README.CDROM README.video Configure DANG_CONFIG README.HOGTHRESHOLD

BFILES=

F_DOC=dosemu.texinfo Makefile dos.1 wp50
F_DRIVERS=emufs.S emufs.sys
F_COMMANDS=exitemu.S exitemu.com vgaon.S vgaon.com vgaoff.S vgaoff.com \
            lredir.exe lredir.c makefile.mak dosdbg.exe dosdbg.c
F_EXAMPLES=config.dist
F_PERIPH=debugobj.S getrom hdinfo.c mkhdimage.c mkpartition putrom.c 
 

###################################################################

LIBPATH=lib


OPTIONAL   = # -DDANGEROUS_CMOS=1
CONFIGS    = $(CONFIG_FILE) $(DOSEMU_USERS_FILE)
DEBUG      = $(SYNC_ALOT)
CONFIGINFO = $(CONFIGS) $(OPTIONAL) $(DEBUG) \
	     -DLIBSTART=$(LIBSTART) -DVERNUM=$(VERNUM) -DVERSTR=\"$(EMUVER)\" \
	     -DPATCHSTR=\"$(PATCHL)\"

 
# does this work if you do make -C <some dir>
TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
INCDIR     = -I$(TOPDIR)/include  -I$(LINUX_INCLUDE)
ifndef USE_SLANG
INCDIR  := $(INCDIR) -I$(NCURSES_INC)
endif
 
ifdef X11LIBDIR
INCDIR  := $(INCDIR) -I$(X11INCDIR)
endif
export INCDIR



# if NEWPIC is there, use it
# if DPMI is there, use it
export CFLAGS     = -N -s -O2 -DUSING_NET=$(USING_NET) $(NEW_PIC) $(DPMI) $(XDEFS) $(CDEBUGOPTS) $(COPTFLAGS) $(INCDIR)
EMU_CFLAGS=-Idosemu $(CFLAGS)
export ASFLAGS    = $(NEW_PIC)
ifdef STATIC
CFLAGS+=-DSTATIC
endif


LDFLAGS    = $(LNKOPTS) # exclude symbol information
AS86 = as86
#LD86 = ld86 -s -0
LD86 = ld86 -0

DISTBASE=/tmp
DISTNAME=dosemu$(EMUVER)pl$(PATCHL)
DISTPATH=$(DISTBASE)/$(DISTNAME)
ifdef RELEASE
DISTFILE=$(DISTBASE)/$(DISTNAME).tgz
else
DISTFILE=$(DISTBASE)/pre$(EMUVER)_$(PATCHL).tgz
endif
export DISTBASE DISTNAME DISTPATH DISTFILE


ifdef do_DEBUG
# first target for debugging build
ifneq (include/config.h,$(wildcard include/config.h))
firstsimple:	include/config.h dep simple
endif

ifdef STATIC
simple:	dossubdirs dos
else
simple:	dossubdirs libdosemu dos
endif
endif


warning: warning2
	@echo "To compile DOSEMU, type 'make doeverything'"
	@echo "To compile DOSEMU if you dont want to use TeX, type 'make most'"
	@echo ""
	
warning2: 
	@echo ""
	@echo "IMPORTANT: "
	@echo "  -> Please read the new 'QuickStart' file before compiling DOSEMU!"
	@echo "  -> The location and format of DOSEMU files have changed since 0.50pl1 release!"
	@echo "  -> This package requires at least the following:"
	@echo "     gcc 2.4.5, lib 4.4.4, Linux 1.1.12 (or patch to Linux 1.0.9),"
	@echo "     and 16MB total swap+RAM.  (you may actually need up to 20MB total)"
	@if [ "1" = "$(X_SUPPORT)" ]; then \
		echo "  -> I guess, you'll compile DOSEMU with X11-support." ; \
		echo "     The X11-libs reside in $(X11LIBDIR)"; \
	else \
		echo "  -> I didn't find the X11-development-system here." ; \
		echo "     DOSEMU will be compiled without X11-support." ; \
	fi
	@echo "  -> Type 'make most' instead of 'make doeverything' if you don't have TeX."
	@echo "  -> Hit Ctrl-C now to abort if you forgot something!"
	@echo ""
	@echo -n "Hit Enter to continue..."
	@read

#	@echo "  -> You need to edit XWINDOWS SUPPORT accordingly in Makefile if you"
#	@echo "     don't have Xwindows installed!" 

warning3:
	@echo ""
	@echo "Be patient...This may take a while to complete, especially for 'mfs.c'."
	@echo "Hopefully you have at least 16MB swap+RAM available during this compile."
	@echo ""

doeverything: warning2 config dep $(DOCS) installnew

itall: warning2 config dep optionalsubdirs $(DOCS) installnew

most: warning2 config dep installnew

all:	warnconf warning3 dos $(LIBDOSEMU) $(X2CEXE)


debug:
	rm dos
	$(MAKE) dos

include/config.h: Makefile 
ifeq (include/config.h,$(wildcard include/config.h))
	@echo "WARNING: Your Makefile has changed since config.h was generated."
	@echo "         Consider doing a 'make config' to be safe."
else
	@echo "WARNING: You have no config.h file in the current directory."
	@echo "         Generating config.h..."
	$(MAKE) dosconfig
	./dosconfig $(CONFIGINFO) > include/config.h
endif

warnconf: include/config.h

dos.o: include/config.h

x2dos.o: include/config.h x2dos.c
	$(CC) $(CFLAGS) -I/usr/openwin/include -c x2dos.c

ifdef STATIC
dos::	dos.o emu.o bios/bios.o
	$(LD) $(LDFLAGS) -o $@ $^ bios/bios.o $(addprefix -L,$(LIBPATH)) -L. \
		$(addprefix -l, $(LIBS)) $(TCNTRL) $(XLIBS)
else
dos:	dos.o
	$(LD) $(LDFLAGS) -N -o $@ $^ $(addprefix -L,$(LIBPATH)) -L. \
		$(TCNTRL) $(XLIBS)
endif

x2dos: x2dos.o
	@echo "Including x2dos.o "
	$(CC) $(LDFLAGS) \
	  -o $@ $< -L$(X11LIBDIR) -lXaw -lXt -lX11

xtermdos:	xtermdos.sh
	@echo "#!/bin/sh" > xtermdos
	@echo >> xtermdos
	@echo X11ROOTDIR=$(X11ROOTDIR) >> xtermdos
	@echo >> xtermdos
	@cat xtermdos.sh >> xtermdos

xinstallvgafont:	xinstallvgafont.sh
	@echo "#!/bin/sh" > xinstallvgafont
	@echo >> xinstallvgafont
	@echo X11ROOTDIR=$(X11ROOTDIR) >> xinstallvgafont
	@echo >> xinstallvgafont
	@cat xinstallvgafont.sh >> xinstallvgafont



$(LIBDOSEMU): 	emu.o dossubdirs
	$(LD) $(LDFLAGS) $(MAGIC) -Ttext $(LIBSTART) -o $(LIBDOSEMU) \
	   -nostdlib $< $(addprefix -L,$(LIBPATH)) -L. $(SHLIBS) \
	    $(addprefix -l, $(LIBS)) bios/bios.o $(XLIBS) $(TCNTRL) -lc

libdosemu:	$(LIBDOSEMU)

.PHONY:	dossubdirs optionalsubdirs docsubdirs
.PHONY: $(LIBSUBDIRS) $(OPTIONALSUBDIRS) $(DOCS) $(REQUIRED)

# ?
dossubdirs:	$(LIBSUBDIRS) $(REQUIRED)

optionalsubdirs:	$(OPTIONALSUBDIRS)


docsubdirs:	$(DOCS)

$(DOCS) $(OPTIONALSUBDIRS) $(LIBSUBDIRS) $(REQUIRED):
	$(MAKE) -C $@ 


include/kversion.h:
	$(SHELL) ./tools/kversion.sh $(LINUX_KERNEL) ./

config: include/config.h include/kversion.h
#	./dosconfig $(CONFIGINFO) > include/config.h

installnew: 
	$(MAKE) install

install: $(REQUIRED) all
	@install -d /var/lib/dosemu
	@nm $(LIBDOSEMU) | grep -v '\(compiled\)\|\(\.o$$\)\|\( a \)' | \
		sort > dosemu.map
	@if [ -f /lib/libemu ]; then rm -f /lib/libemu ; fi
	@for i in $(SUBDIRS); do \
		(cd $$i && echo $$i && $(MAKE) install) || exit; \
	done
	@install -c -o root -m 04755 dos /usr/bin
	@install -m 0644 $(LIBDOSEMU) /usr/lib
	@(cd /usr/lib; ln -sf $(LIBDOSEMU) libdosemu)
	@if [ -f /usr/bin/xdosemu ]; then \
		install -m 0700 /usr/bin/xdosemu /tmp; \
		rm -f /usr/bin/xdosemu; \
	fi
	@if [ -f $(BOOTDIR)/sillyint.o ]; then rm -f $(BOOTDIR)/sillyint.o ; fi
	@install -m 0755 -d $(BOOTDIR)
	@if [ -f sig/sillyint.o ]; then \
	install -c -o root -g root -m 0750 sig/sillyint.o $(BOOTDIR) ; fi
ifdef X_SUPPORT
	@ln -sf dos xdos
	@install -m 0755 xtermdos /usr/bin
	@if [ ! -e /usr/bin/xdos ]; then ln -s dos /usr/bin/xdos; fi
	@echo ""
	@echo "-> Main DOSEMU files installation done. Installing the Xwindows PC-8 font..."
	@if [ -w $(X11LIBDIR)/X11/fonts/misc ] && [ -d $(X11LIBDIR)/X11/fonts/misc ]; then \
		if [ ! -e $(X11LIBDIR)/X11/fonts/misc/vga.pcf* ]; then \
			install -m 0644 vga.pcf $(X11LIBDIR)/X11/fonts/misc; \
			cd $(X11LIBDIR)/X11/fonts/misc; \
			mkfontdir; \
		fi \
	fi
endif
	@echo ""
	@echo "---------------------------------DONE compiling-------------------------------"
	@echo ""
	@echo "  - You need to configure DOSEMU. Read 'config.dist' in the 'examples' dir."
	@echo "  - Update your /etc/dosemu.conf by editing a copy of './examples/config.dist'"
	@echo "  - Using your old DOSEMU 0.52 configuration file might not work."
	@echo "  - After configuring DOSEMU, you can type 'dos' to run DOSEMU."
	@echo "  - If you have sillyint defined, you must load sillyint.o prior"
	@echo "  - to running DOSEMU (see sig/HowTo)"
ifdef X_SUPPORT
	@echo "  - Use 'xdos' instead of 'dos' to cause DOSEMU to open its own Xwindow."
	@echo "  - Type 'xset fp rehash' before running 'xdos' for the first time."
	@echo "  - To make your backspace and delete key work properly in 'xdos', type:"
	@echo "		xmodmap -e \"keycode 107 = 0xffff\""
	@echo "		xmodmap -e \"keycode 22 = 0xff08\""
	@echo "		xmodmap -e \"key 108 = Return\"  [Return = 0xff0d]"	
	@echo ""
endif
	@echo "  - Try the ./commands/mouse.com if your INTERNAL mouse won't work"
	@echo "  - Try ./commands/unix.com to run a Unix command under DOSEMU"
	@echo "  - Try the ./garrot02/garrot02.com for better CPU use under Linux"
	@echo ""

converthd: hdimage
	mv hdimage hdimage.preconvert
	periph/mkhdimage -h 4 -s 17 -c 40 | cat - hdimage.preconvert > hdimage
	@echo "Your hdimage is now converted and ready to use with 0.52!"

newhd: periph/bootsect
	periph/mkhdimage -h 4 -s 17 -c 40 | cat - periph/bootsect > newhd
	@echo "You now have a hdimage file called 'newhd'"

include Makefile.common

checkin::
	-ci $(CFILES) $(HFILES) $(SFILES) $(OFILES)
	@for i in $(LIBS) $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) checkin) || exit; done

checkout::
	-co -M -l $(CFILES) $(HFILES) $(SFILES) $(OFILES)
	@for i in $(LIBS) $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE) checkout) || exit; done

dist:: $(CFILES) $(HFILES) $(SFILES) $(OFILES) $(BFILES) include/config.h
	install -d $(DISTPATH)
	install -d $(DISTPATH)/lib
	install -m 0644 dosemu.xpm libslang.a $(CFILES) $(HFILES) $(SFILES) $(OFILES) $(BFILES) $(DISTPATH)
	cp TODO $(DISTPATH)/.todo
	cp TODO.JES $(DISTPATH)/.todo.jes
	cp .indent.pro $(DISTPATH)/.indent.pro
	install -m 0644 hdimages/hdimage.dist $(DISTPATH)/hdimage.dist
	@for i in $(REQUIRED) $(LIBS) $(SUBDIRS) $(DOCS) ipxutils $(OPTIONALSUBDIRS) ipxbridge; do \
	    (cd $$i && echo $$i && $(MAKE) dist) || exit; \
	done
	install -d $(DISTPATH)/garrot02
	install -m 0644 garrot02/* $(DISTPATH)/garrot02
	(cd $(DISTBASE); tar cf - $(DISTNAME) | gzip -9 >$(DISTFILE))
	rm -rf $(DISTPATH)
	@echo "FINAL .tgz FILE:"
	@ls -l $(DISTFILE) 

local_clean:
	-rm -f $(OBJS) $(X2CEXE) x2dos.o dos.o dos libdosemu0.* *.s core \
	  dosconfig dosconfig.o *.tmp dosemu.map emu.o

local_realclean:	
	-rm -f include/config.h include/kversion.h

clean::	local_clean

realclean::   local_realclean local_clean

clean realclean::
	-@for i in $(REQUIRED) $(DOCS) $(LIBS) $(SUBDIRS) $(OPTIONALSUBDIRS); do \
	  $(MAKE) -C $$i $@; \
	done

pristine:	realclean
	-rm lib/*

# DEPENDS=dos.d emu.d 
# this is to do make subdir.depend to make a dependency
DEPENDDIRS=$(addsuffix .depend, $(LIBSUBDIRS) )

depend_local:	$(DEPENDS)

depend dep:  $(DEPENDDIRS) depend_local
	$(CPP) -MM $(CFLAGS) $(CFILES) > .depend
	cd clients;$(CPP) -MM -I../ -I../include $(CFLAGS) *.c > .depend

.PHONY:       size
size:
	size ./dos >>size
	ls -l dos >>size

.PHONY: $(DEPENDDIRS)

$(DEPENDDIRS):
	$(MAKE) -C $(subst .depend,,$@) depend

emu.o:	emu.c
	$(CC) -c $(EMU_CFLAGS) -o $@ $<

emu.d:	emu.c
	$(SHELL) -ec '$(CC) $(TYPE_DEPEND) $(EMU_CFLAGS) $< \
                           | sed '\''s/$*\\.o[ :]*/& $@/g'\'' > $@'

.PHONY: help
help:
	@echo The following targets will do make depends:
	@echo 	$(DEPENDDIRS)
	@echo "Each .c file has a corresponding .d file (the dependencies)"
	@echo
	@echo 
	@echo Making dossubdirs will make the follow targets:
	@echo "    " $(LIBSUBDIRS)
	@echo
	echo "To clean a directory, do make -C <dirname> clean|realclean"
