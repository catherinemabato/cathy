#ifdef DOSEMU
#include "emu.h"
#include "serial.h"
#include "utilities.h"
#else
#define _XOPEN_SOURCE
#endif
#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>	/*(printf,fprintf)*/
#include <ctype.h>	/*isprint*/
#include <arpa/telnet.h>/*IAC,DO,DONT,...*/
#include <sys/time.h>	/*fd_set,FD_ZERO*/
#include <sys/stat.h>
#include <fcntl.h>	/*O_RDWR*/
#include <errno.h>	/*EINTR*/
#include <string.h>

#ifdef __GLIBC__
#include <pty.h>
#endif

#include "defs.h"	/*uchar*/
#include "sock.h"	/*sock*/
#include "sockbuf.h"	/*sockBufR,sockBufW*/
#include "ttybuf.h"	/*tty*/
#include "stty.h"	/*(setTty)*/
#include "cmdlex.h"	/*Cmdstat*/
#include "telopt.h"	/*telOptSummary*/
#include "atcmd.h"	/*CHAR_CR*/
#include "timeval.h"	/*(timeval...)*/
#include "commx.h"	/*(commxForkExec)*/
#include "cmdarg.h"	/*cmdarg*/


enum ModemMode { NOMODE, CMDMODE, DIAL, ONLINE };
static enum ModemMode mmode;

/* socket input processing loop */

static void
sockReadLoop(void)
{
    static enum {
	SRL_NORM, SRL_IAC, SRL_CMD,
	SRL_SB, SRL_SBC, SRL_SBS, SRL_SBI
    } state /*= SRL_NORM*/;
    static int cmd;
    static int opt;
    int c;

    if (atcmd.pr) {
	while ((c = getSock1()) >= 0) putTty1(c);
    } else {
	while ((c = getSock1()) >= 0) {
	    switch (state) {
	    case SRL_IAC:
		switch (c) {
		case WILL:
		case WONT:
		case DO:
		case DONT:
		    cmd = c;
		    state = SRL_CMD;
		    break;
		case IAC:
		    /*if (telOpt.binrecv)*/ {
			putTty1(c);
			state = SRL_NORM;
		    }
		    break;
		case SB:
		    state = SRL_SB;
		    break;
		default:
		    state = SRL_NORM;
		    telOptPrintCmd("<", c);
		}
		break;
	    case SRL_CMD:
		if (telOptHandle(cmd, c)) sock.alive = 0;
		state = SRL_NORM;
		break;
	    case SRL_SB:
		opt = c;
		state = SRL_SBC;
		break;
	    case SRL_SBC:
		state = (c == TELQUAL_SEND)? SRL_SBS : SRL_NORM;
		break;
	    case SRL_SBS:
		state = (c == IAC)? SRL_SBI : SRL_NORM;
		break;
	    case SRL_SBI:
		telOptSBHandle(opt);
		state = SRL_NORM;
		break;
	    default:
		if (c == IAC) {
		    state = SRL_IAC;
		} else {
		    /*putTty1(telOpt.binrecv? c : (c & 0x7f));*/
		    putTty1(c);
		}
	    }
	}
    }
}


/* TTY input processing loop */

enum { ESH_NORM, ESH_P1, ESH_P2, ESH_P3 };
static struct {
    int state;
    struct timeval plus1T; /* the time 1st '+' input */
    int checkSilence; /* Recognized silence,"+++" sequence.
			 Now prepare for the 2nd silence.. */
    struct timeval expireT; /* keep silence until the time */
} escSeq;

#define escSeqReset() { escSeq.state = ESH_NORM; }
#define checkTtySilence() (escSeq.checkSilence)

/* t1 - t2 > S12? */
static int
s12timePassed(const struct timeval *t1p, const struct timeval *t2p)
{
    struct timeval t;

    timevalSet10ms(&t, atcmd.s[12] * 2);
    timevalAdd(&t, t2p);
    return (timevalCmp(t1p, &t) > 0);
}

static void
escSeqHandle(int c)
{
    switch (escSeq.state) {
    case ESH_P1:
	if (c == CHAR_ESC
	    && ! s12timePassed(&ttyBufR.newT, &escSeq.plus1T)) {
	    escSeq.state = ESH_P2;
	} else escSeq.state = ESH_NORM;
	break;
    case ESH_P2:
	if (c == CHAR_ESC
	    && ! s12timePassed(&ttyBufR.newT, &escSeq.plus1T)) {
	    escSeq.checkSilence = 1;
	    timevalSet10ms(&escSeq.expireT, atcmd.s[12] * 2);
	    timevalAdd(&escSeq.expireT, &ttyBufR.newT);
	    escSeq.state = ESH_P3;
	} else escSeq.state = ESH_NORM;
	break;
    case ESH_P3:
	escSeq.checkSilence = 0;
	escSeq.state = ESH_NORM;
	/*break;*/
    case ESH_NORM:
	if (c == CHAR_ESC
	    && s12timePassed(&ttyBufR.newT, &ttyBufR.prevT)) {
	    escSeq.plus1T = ttyBufR.newT;
	    escSeq.state = ESH_P1;
	}
    }
}


/*#define LINEBUF_SIZE 256 =>defs.h*/

static struct {
    uchar buf[LINEBUF_SIZE];
    uchar *ptr;
    /*int eol;*/
} lineBuf;

#define lineBufReset() { lineBuf.ptr = lineBuf.buf; /*lineBuf.eol = 0;*/ }
#define putLine1(c) \
{ \
    if (lineBuf.ptr < lineBuf.buf + LINEBUF_SIZE) *lineBuf.ptr++ = (c); \
}
#define lineBufBS() \
{ \
    if (lineBuf.ptr > lineBuf.buf) lineBuf.ptr--; \
}

static void
ttyReadLoop(void)
{
    int c;

    if (atcmd.pr) {
	while ((c = getTty1()) >= 0) {
	    putSock1(c);
	    escSeqHandle(c);
	}
    } else if (telOpt.sgasend) {
	while ((c = getTty1()) >= 0) {
	    /*if (telOpt.binsend)*/ {
		if (c == IAC) putSock1(IAC);
		putSock1(c);
	    } /*else putSock1(c & 0x7f);*/
	    escSeqHandle(c);
	}
    } else {
	/* !sgasend == local echo mode, which cannot be true binmode */
	while ((c = getTty1()) >= 0) {
	    putTty1(c);
	    if (c == CHAR_CR) {
		putTty1(CHAR_LF);
		putSockN(lineBuf.buf, lineBuf.ptr - lineBuf.buf);
		putSock1('\r'); /* EOL = CRLF */
		putSock1('\n');
		lineBufReset();
	    } else if (c == CHAR_LF) {
		/* ignore LFs. CR is the EOL char for modems */
	    } else if (c == CHAR_BS) {
		lineBufBS();
	    } else {
		/*if (telOpt.binsend)*/ {
		    if (c == IAC) putLine1(IAC);
		    putLine1(c);
		} /*else putLine1(c & 0x7f);*/
	    }
	    escSeqHandle(c);
	}
    }
}

/* online mode main loop */

static int
onlineMode(void)
{
    fd_set rfds,wfds;
    int max_fd, selrt;
    struct timeval t = {};

    while (1) {
	if (!sockIsAlive()) {
	    sockShutdown();
	    return 0;
	}

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

	if (ttyBufWReady()) FD_SET(sock.fd, &rfds); /*flow control*/
	if (sockBufWHasData()) FD_SET(sock.fd, &wfds);
	if (sockBufWReady()) FD_SET(tty.rfd, &rfds); /*flow control*/
	if (ttyBufWHasData()) FD_SET(tty.wfd, &wfds);

	if (escSeq.checkSilence) {
	    struct timeval tt;
	    gettimeofday(&tt, NULL);
	    if (timevalCmp(&tt, &escSeq.expireT) >= 0) {
		escSeq.checkSilence = 0;
		sockShutdown();
		return 1;
	    }
	}

#ifndef DOSEMU
#define _max(a, b) (((a) > (b)) ? (a) : (b))
#endif
	max_fd = _max(tty.rfd, tty.wfd);
	max_fd = _max(max_fd, sock.fd);
	selrt = select(max_fd + 1, &rfds, &wfds, NULL, &t);
	switch (selrt) {
	case -1:
	    if (errno != EINTR) {
		perror("select()");
		return 0;
	    }
	    /* fall thru */
	case 0:
	    return 2;
	}

	if (FD_ISSET(sock.fd, &wfds)) {
	    sockBufWrite();
	}
	if (FD_ISSET(tty.wfd, &wfds)) {
	    ttyBufWrite();
	}
	if (FD_ISSET(sock.fd, &rfds)) {
	    sockBufRead();
	    sockReadLoop();
	}
	if (FD_ISSET(tty.rfd, &rfds)) {
	    ttyBufRead();
	    ttyReadLoop();
	}
    }
    return 0;
}


/* command mode input processing loop */

/*#define CMDBUF_MAX 255 =>defs.h*/

static struct {
    uchar buf[CMDBUF_MAX+1];
    uchar *ptr;
    int eol;
} cmdBuf;

#define cmdBufReset() { cmdBuf.ptr = cmdBuf.buf; cmdBuf.eol = 0; }
#define putCmd1(c) \
{ \
    if (cmdBuf.ptr < cmdBuf.buf + CMDBUF_MAX) *cmdBuf.ptr++ = (c); \
}
#define cmdBufBS() \
{ \
    if (cmdBuf.ptr > cmdBuf.buf) cmdBuf.ptr--; \
}

static void
cmdReadLoop(void)
{
    int c;

    while ((c = getTty1()) >= 0) {
	putTty1(c);
	if (c == CHAR_CR) {
	    cmdBuf.eol = 1;
	    *cmdBuf.ptr = '\0';
	    return; /* may discard some chars in ttyBufR */
	} else if (c == CHAR_BS) {
	    cmdBufBS();
#if 0
	} else if (c <= ' ' || c == 127) {
	    /* side effect: "a  t" is recognized as "at" */
#else
	} else if (c < ' ' || c == 127) {
#endif
	    /* just ignore them */
	} else {
	    putCmd1(c);
	}
    }
}


/* command mode main loop */

static void
putTtyCmdstat(Cmdstat s)
{
    static const char *cmdstatStr[] = {
	"OK",
	"ERROR",
	"CONNECT",
	"NO CARRIER",
	"",
	"",
	"",
    };

    putTty1(CHAR_CR);
    putTty1(CHAR_LF);
    putTtyN(cmdstatStr[s], strlen(cmdstatStr[s]));
    putTty1(CHAR_CR);
    putTty1(CHAR_LF);
    ttyBufWrite();
}

static Cmdstat
cmdMode(void)
{
    fd_set rfds,wfds;
    struct timeval to = {};
    int selrt;
    Cmdstat stat;

    for (;;) {
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

	if (ttyBufWReady()) FD_SET(tty.rfd, &rfds); /*flow control*/
	if (ttyBufWHasData()) FD_SET(tty.wfd, &wfds);

	selrt = select(tty.wfd+1, &rfds, &wfds, NULL, &to);
	switch (selrt) {
	case -1:
	    if (errno != EINTR) {
		perror("select()");
		return CMDST_ERROR;
	    }
	    return CMDST_OK;
	case 0:
	    return CMDST_OK;
	}

	if (FD_ISSET(tty.wfd, &wfds)) {
	    ttyBufWrite(); /* put CR before dialup */
	    if (cmdBuf.eol) {
		stat = cmdLex((char *)cmdBuf.buf);
		cmdBufReset();
		switch (stat) {
		case CMDST_ATD:
		case CMDST_ATO:
		    return stat;
		case CMDST_OK:
		case CMDST_ERROR:
		    putTtyCmdstat(stat);
		    break;
		default:; /*CMDST_NOCMD*/
		}		
	    }
	}
	if (FD_ISSET(tty.rfd, &rfds)) {
	    ttyBufRead();
	    cmdReadLoop();
	}
    }
}


/* open a pty */
#ifndef DOSEMU
int
openPtyMaster(const char *dev)
{
    int fd;

    fd = open(dev, O_RDWR);
    if (fd < 0) {
	fprintf(stderr, "Pty open error.\n");
	exit(1);
    }
    return fd;
}
#endif

#ifdef DOSEMU
static
#endif
int
getPtyMaster(char **line_return)
{
    int rc;
    char name[12], *line = NULL;
    int pty = -1;
    const char *name1 = "pqrstuvwxyzPQRST", *name2 = "0123456789abcdef";
    const char *p1, *p2;

#ifdef HAVE_GRANTPT
    char *temp_line;
    pty = open("/dev/ptmx", O_RDWR | O_CLOEXEC);
    if(pty < 0)
        goto bsd;

    rc = grantpt(pty);
    if(rc < 0) {
        close(pty);
        goto bsd;
    }

    rc = unlockpt(pty);
    if(rc < 0) {
        close(pty);
        goto bsd;
    }

    temp_line = ptsname(pty);
    if(!temp_line) {
        close(pty);
        goto bsd;
    }
    line = malloc(strlen(temp_line) + 1);
    if(!line) {
        close(pty);
        return -1;
    }
    strcpy(line, temp_line);

    *line_return = line;
    return pty;

  bsd:
#endif /* HAVE_GRANTPT */

    strcpy(name, "/dev/pty??");
    for(p1 = name1; *p1; p1++) {
        name[8] = *p1;
        for(p2 = name2; *p2; p2++) {
            name[9] = *p2;
            pty = open(name, O_RDWR);
            if(pty >= 0)
                goto found;
            if(errno == ENOENT)
                goto bail;
            else
                continue;
        }
    }

    goto bail;

  found:
    line = strdup(name);
    line[5] = 't';
    rc = chown(line, getuid(), getgid());
    if(rc < 0) {
        fprintf(stderr, 
                "Warning: could not change ownership of tty -- "
                "pty is insecure!\n");
    }
    rc = chmod(line, S_IRUSR | S_IWUSR | S_IWGRP);
    if (rc < 0) {
        fprintf(stderr, 
                "Warning: could not change permissions of tty -- "
                "pty is insecure!\n");
    }

    *line_return = line;
    return pty;

  bail:
    if(pty >= 0)
        close(pty);
    if(line)
        free(line);
    return -1;
}

static int start_online(void)
{
    sockBufRReset();
    sockBufWReset();
    ttyBufRReset();
    /*ttyBufWReset();*/
    lineBufReset();
    escSeqReset();

    if (!telOpt.sentReqs && !atcmd.pr) telOptSendReqs();

    return 0;
}

static int start_dial(void)
{
    return sockConnectStart();
}

static int start_cmd(void)
{
    cmdBufReset();
    ttyBufRReset();
    /*ttyBufWReset();*/

    return 0;
}

static int do_mode_switch(enum ModemMode old_mode, enum ModemMode new_mode)
{
    switch (new_mode) {
    case ONLINE:
	return start_online();
    case DIAL:
	return start_dial();
    case CMDMODE:
	return start_cmd();
    case NOMODE:
	break;
    }
    return 0;
}

static enum ModemMode do_modem(enum ModemMode mode)
{
  switch (mode) {
  case NOMODE:
    return CMDMODE;

  case CMDMODE:
    switch (cmdMode()) {
    case CMDST_ATD:
	if (sockIsAlive()) {
	    putTtyCmdstat(CMDST_ERROR);
	    return CMDMODE;
	}
	return DIAL;
    case CMDST_ATO:
	if (!sockIsAlive()) {
	    putTtyCmdstat(CMDST_NOCARRIER);
	    return CMDMODE;
	}
	return ONLINE;
    case CMDST_OK:
	return CMDMODE;
    case CMDST_ERROR:
	return NOMODE;
    default:;
    }

    return NOMODE;

  case DIAL:
    telOptReset(); /* before sockDial(), which may change telOpt.xx */
    switch (sockDial()) {
    case 0: /* connect */
	putTtyCmdstat(CMDST_CONNECT);
	return ONLINE;
    case 1: /* error */
	putTtyCmdstat(CMDST_NOCARRIER);
	return CMDMODE;
    case 2: /* connection in progress */
	return DIAL;
    }

    return NOMODE;

  case ONLINE:
    switch (onlineMode()) {
    case 0: /* connection lost */
	putTtyCmdstat(CMDST_NOCARRIER);
	return CMDMODE;
    case 1: /* +++ */
	putTtyCmdstat(CMDST_OK);
	return CMDMODE;
    case 2:
	return ONLINE;
    }

    return NOMODE;
  }

  return NOMODE;
}

#ifdef DOSEMU
static
#endif
void init_modemu(void)
{
    ttyBufWReset();
    telOptInit();
    atcmdInit(); /* initialize atcmd */

    mmode = NOMODE;
}

#ifdef DOSEMU
static
#endif
int run_modemu(void)
{
    enum ModemMode new_mode;

    new_mode = do_modem(mmode);
    if (new_mode != mmode) {
	int err = do_mode_switch(mmode, new_mode);
	if (err)
	    return 1;
	mmode = new_mode;
    }

    if (new_mode != NOMODE)
	return 2;
    return 0;
}

#ifdef DOSEMU
static int initialized;

void modemu_async_callback(void *arg)
{
    run_modemu();
}

void modemu_update(int num)
{
    if (mmode != DIAL)
	return;
    run_modemu();
}

char *modemu_init(int num)
{
    char *ptyslave = NULL;

    if (initialized) {
	error("Multiple vmodem ports not supported, sorry!\n");
	return NULL;
    }
    initialized++;
    tty.rfd = tty.wfd = getPtyMaster(&ptyslave);
    init_modemu();
    add_to_io_select(tty.rfd, modemu_async_callback, NULL);

    return ptyslave;
}

void modemu_done(int num)
{
    if (!initialized)
	return;
    remove_from_io_select(tty.rfd);
    close(tty.rfd);
    close(tty.wfd);
}

#endif
