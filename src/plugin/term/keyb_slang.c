/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <slang.h>
#include "emu.h"
#include "timers.h"
#include "keymaps.h"
#include "keyb_clients.h"
#include "keyboard.h"
#include "utilities.h"
#include "video.h"
#include "env_term.h"
#include "translate.h"

#ifndef VOID
#  define VOID void
#endif

#define KBBUF_SIZE 80

static struct keyboard_state
{
	int kbd_fd;

	int kbcount;
	Bit8u kbbuf[KBBUF_SIZE];
	Bit8u *kbp;

	int save_kbd_flags; /* saved flags for STDIN before our fcntl */
	struct termios save_termios;

	int pc_scancode_mode; /* By default we are not in pc_scancode_mode */
	SLKeyMap_List_Type *The_Normal_KeyMap;

	unsigned char erasekey;
	unsigned char Esc_Char;
	int KeyNot_Ready;	 /* a flag */
	int Keystr_Len;
	unsigned long Shift_Flags;
	
	struct char_set_state translate_state;
} keyb_state;


#ifndef SLANG_VERSION
# define SLANG_VERSION 1
#endif

/*
 * The goal of the routines here is simple: to allow SHIFT, ALT, and
 * CONTROL keys to be used with a remote terminal.  The way this is
 * accomplished is simple:  There are 4 keymaps: normal, shift, control,
 * and alt.  The user must a key or keysequence that invokes on of these
 * keymaps.  The default keymap is the normal one.  When one of the other
 * keymaps is invoked via the special key defined for it, the new keymap
 * is only in effect for the next character at which point the normal one
 * becomes active again.
 * 
 * The normal keymap is fairly robust.  It contains mappings for all the
 * ascii characters (0-26, 28-256).  The escape character itself (27) is
 * included only in the sence that it is the prefix character for arrow
 * keys, function keys, etc...
 */

/*
 * The keymaps are simply a mapping from a character sequence to
 * scan/ascii codes.
 */

typedef struct {
  unsigned char keystr[10];
  unsigned long scan_code;
}
Keymap_Scan_Type;

#define SHIFT_MASK			0x00010000
#define CTRL_MASK			0x00020000
#define ALT_MASK			0x00040000
#define ALTGR_MASK			0x00080000
#define STICKY_SHIFT_MASK		0x00100000
#define STICKY_CTRL_MASK		0x00200000
#define STICKY_ALT_MASK			0x00400000
#define STICKY_ALTGR_MASK		0x00800000
#define KEYPAD_MASK			0x01000000
#define MOVE_MASK			0x02000000

#define ALT_KEY_SCAN_CODE		0x80000000
#define STICKY_ALT_KEY_SCAN_CODE	0x80000001
#define SHIFT_KEY_SCAN_CODE		0x80000002
#define STICKY_SHIFT_KEY_SCAN_CODE	0x80000003
#define CTRL_KEY_SCAN_CODE		0x80000004
#define STICKY_CTRL_KEY_SCAN_CODE	0x80000005
#define ALTGR_KEY_SCAN_CODE		0x80000006
#define STICKY_ALTGR_KEY_SCAN_CODE	0x80000007

#define SCROLL_UP_SCAN_CODE		0x80000020
#define SCROLL_DOWN_SCAN_CODE		0x80000021
#define REDRAW_SCAN_CODE		0x80000022
#define SUSPEND_SCAN_CODE		0x80000023
#define HELP_SCAN_CODE			0x80000024
#define RESET_SCAN_CODE			0x80000025
#define SET_MONO_SCAN_CODE		0x80000026
#define KEYPAD_KEY_SCAN_CODE		0x80000027

/* Keyboard maps for non-terminfo keys */

static Keymap_Scan_Type Dosemu_defined_fkeys[] =
{
/* These are the F1 - F12 keys for terminals without them. */
  {"^@1", KEY_F1 },			/* F1 */
  {"^@2", KEY_F2 },			/* F2 */
  {"^@3", KEY_F3 },			/* F3 */
  {"^@4", KEY_F4 },			/* F4 */
  {"^@5", KEY_F5 },			/* F5 */
  {"^@6", KEY_F6 },			/* F6 */
  {"^@7", KEY_F7 },			/* F7 */
  {"^@8", KEY_F8 },			/* F8 */
  {"^@9", KEY_F9 },			/* F9 */
  {"^@0", KEY_F10 },			/* F10 */
  {"^@-", KEY_F11 },			/* F11 */
  {"^@=", KEY_F12 },			/* F12 */
  {"\033\033", KEY_ESC },		/* ESC */
  {"", 0}
};

static Keymap_Scan_Type Generic_backspace[] =
{
  {"^H",   KEY_BKSP },			/* Backspace */
  {"\177", KEY_BKSP },
  {"", 0}
};

#if 0
static Keymap_Scan_Type Meta_ALT[] =
{
/* Alt keys (high bit set) */
  {"\233", KEY_ESC         | ALT_MASK },	/* Alt Esc */
#if 0
  {"\377", KEY_BKSP        | ALT_MASK },	/* Alt Backspace */
#endif
  {"\361", KEY_Q           | ALT_MASK },	/* Alt Q */
  {"\367", KEY_W           | ALT_MASK },	/* Alt W */
  {"\345", KEY_E           | ALT_MASK },	/* Alt E */
  {"\362", KEY_R           | ALT_MASK },	/* Alt R */
  {"\364", KEY_T           | ALT_MASK },	/* Alt T */
  {"\371", KEY_Y           | ALT_MASK },	/* Alt Y */
  {"\365", KEY_U           | ALT_MASK },	/* Alt U */
  {"\351", KEY_I           | ALT_MASK },	/* Alt I */
  {"\357", KEY_O           | ALT_MASK },	/* Alt O */
  {"\360", KEY_P           | ALT_MASK },	/* Alt P */
  {"\333", KEY_LBRACK      | ALT_MASK },	/* Alt [ */
  {"\335", KEY_RBRACK      | ALT_MASK },	/* Alt ] */
  {"\215", KEY_RETURN      | ALT_MASK },	/* Alt Enter */
  {"\341", KEY_A           | ALT_MASK },	/* Alt A */
  {"\363", KEY_S           | ALT_MASK },	/* Alt S */
/* {"\344", KEY_D           | ALT_MASK }, */	/* Alt D *//* This is not (always) true */
  {"\346", KEY_F           | ALT_MASK },	/* Alt F */
  {"\347", KEY_G           | ALT_MASK },	/* Alt G */
  {"\350", KEY_H           | ALT_MASK },	/* Alt H */
  {"\352", KEY_J           | ALT_MASK },	/* Alt J */
  {"\353", KEY_K           | ALT_MASK },	/* Alt K */
  {"\354", KEY_L           | ALT_MASK },	/* Alt L */
  {"\273", KEY_SEMICOLON   | ALT_MASK },	/* Alt ; */
  {"\247", KEY_APOSTROPHE  | ALT_MASK },	/* Alt ' */
  {"\340", KEY_GRAVE       | ALT_MASK },	/* Alt ` */
/* {"\334", KEY_BACKSLASH   | ALT_MASK }, *//* Alt \ *//* This is not (always) true */
  {"\372", KEY_Z           | ALT_MASK },	/* Alt Z */
  {"\370", KEY_X           | ALT_MASK },	/* Alt X */
  {"\343", KEY_C           | ALT_MASK },	/* Alt C */
/* {"\366", KEY_V           | ALT_MASK }, */	/* Alt V *//* This is not (always) true */
  {"\342", KEY_B           | ALT_MASK },	/* Alt B */
  {"\356", KEY_N           | ALT_MASK },	/* Alt N */
  {"\355", KEY_M           | ALT_MASK },	/* Alt M */
  {"\254", KEY_COMMA       | ALT_MASK },	/* Alt , */
  {"\256", KEY_PERIOD      | ALT_MASK },	/* Alt . */
  {"\257", KEY_SLASH       | ALT_MASK },	/* Alt / */
  {"\261", KEY_1           | ALT_MASK },	/* Alt 1 */
  {"\262", KEY_2           | ALT_MASK },	/* Alt 2 */
  {"\263", KEY_3           | ALT_MASK },	/* Alt 3 */
  {"\264", KEY_4           | ALT_MASK },	/* Alt 4 */
  {"\265", KEY_5           | ALT_MASK },	/* Alt 5 */
  {"\266", KEY_6           | ALT_MASK },	/* Alt 6 */
  {"\267", KEY_7           | ALT_MASK },	/* Alt 7 */
  {"\270", KEY_8           | ALT_MASK },	/* Alt 8 */
  {"\271", KEY_9           | ALT_MASK },	/* Alt 9 */
  {"\260", KEY_0           | ALT_MASK },	/* Alt 0 */
  {"\255", KEY_DASH        | ALT_MASK },	/* Alt - */
  {"\275", KEY_EQUALS      | ALT_MASK },	/* Alt = */
  {"\211", KEY_TAB         | ALT_MASK },	/* Alt Tab */
  {"", 0}
};
#endif

static Keymap_Scan_Type Esc_ALT[] =
{
/* Another form of alt keys */
  {"\033q", KEY_Q          | ALT_MASK },	/* Alt Q */
  {"\033w", KEY_W          | ALT_MASK },	/* Alt W */
  {"\033e", KEY_E          | ALT_MASK },	/* Alt E */
  {"\033r", KEY_R          | ALT_MASK },	/* Alt R */
  {"\033t", KEY_T          | ALT_MASK },	/* Alt T */
  {"\033y", KEY_Y          | ALT_MASK },	/* Alt Y */
  {"\033u", KEY_U          | ALT_MASK },	/* Alt U */
  {"\033i", KEY_I          | ALT_MASK },	/* Alt I */
  {"\033o", KEY_O          | ALT_MASK },	/* Alt O */
  {"\033p", KEY_P          | ALT_MASK },	/* Alt P */
  {"\033\015", KEY_RETURN  | ALT_MASK },	/* Alt Enter */
  {"\033a", KEY_A          | ALT_MASK },	/* Alt A */
  {"\033s", KEY_S          | ALT_MASK },	/* Alt S */
  {"\033d", KEY_D          | ALT_MASK },	/* Alt D */
  {"\033f", KEY_F          | ALT_MASK },	/* Alt F */
  {"\033g", KEY_G          | ALT_MASK },	/* Alt G */
  {"\033h", KEY_H          | ALT_MASK },	/* Alt H */
  {"\033j", KEY_J          | ALT_MASK },	/* Alt J */
  {"\033k", KEY_K          | ALT_MASK },	/* Alt K */
  {"\033l", KEY_L          | ALT_MASK },	/* Alt L */
  {"\033;", KEY_SEMICOLON  | ALT_MASK },	/* Alt ; */
  {"\033'", KEY_APOSTROPHE | ALT_MASK },	/* Alt ' */
  {"\033`", KEY_GRAVE      | ALT_MASK },	/* Alt ` */
  {"\033\\", KEY_BACKSLASH | ALT_MASK },	/* Alt \ */
  {"\033z", KEY_Z          | ALT_MASK },	/* Alt Z */
  {"\033x", KEY_X          | ALT_MASK },	/* Alt X */
  {"\033c", KEY_C          | ALT_MASK },	/* Alt C */
  {"\033v", KEY_V          | ALT_MASK },	/* Alt V */
  {"\033b", KEY_B          | ALT_MASK },	/* Alt B */
  {"\033n", KEY_N          | ALT_MASK },	/* Alt N */
  {"\033m", KEY_M          | ALT_MASK },	/* Alt M */
  {"\033,", KEY_COMMA      | ALT_MASK },	/* Alt , */
  {"\033.", KEY_PERIOD     | ALT_MASK },	/* Alt . */
  {"\033/", KEY_SLASH      | ALT_MASK },	/* Alt / */
  {"\0331", KEY_1          | ALT_MASK },	/* Alt 1 */
  {"\0332", KEY_2          | ALT_MASK },	/* Alt 2 */
  {"\0333", KEY_3          | ALT_MASK },	/* Alt 3 */
  {"\0334", KEY_4          | ALT_MASK },	/* Alt 4 */
  {"\0335", KEY_5          | ALT_MASK },	/* Alt 5 */
  {"\0336", KEY_6          | ALT_MASK },	/* Alt 6 */
  {"\0337", KEY_7          | ALT_MASK },	/* Alt 7 */
  {"\0338", KEY_8          | ALT_MASK },	/* Alt 8 */
  {"\0339", KEY_9          | ALT_MASK },	/* Alt 9 */
  {"\0330", KEY_0          | ALT_MASK },	/* Alt 0 */
  {"\033-", KEY_DASH       | ALT_MASK },	/* Alt - */
  {"\033=", KEY_EQUALS     | ALT_MASK },	/* Alt = */
  {"\033\011", KEY_TAB     | ALT_MASK },	/* Alt Tab */
  {"", 0}
};

static Keymap_Scan_Type Linux_Keypad[] =
{
/* Keypad keys */
#if 0
  {"\033OP", KEY_NUMLOCK },		/* Keypad Numlock */
#endif
  {"\033OQ", KEY_PAD_SLASH },		/* Keypad / */
  {"\033OR", KEY_PAD_AST },		/* Keypad * */
  {"\033OS", KEY_PAD_MINUS },		/* Keypad - */
  {"", 0}
};

static Keymap_Scan_Type vtxxx_Keypad[] =
{
  {"\033Ow", KEY_PAD_7 },		/* Keypad 7 */
  {"\033Ox", KEY_PAD_8 },		/* Keypad 8 */
  {"\033Oy", KEY_PAD_9 },		/* Keypad 9 */
  {"\033Ot", KEY_PAD_4 },		/* Keypad 4 */
  {"\033Ou", KEY_PAD_5 },		/* Keypad 5 */
  {"\033Ov", KEY_PAD_6 },		/* Keypad 6 */
  {"\033Ol", KEY_PAD_PLUS },		/* Keypad + */
  {"\033Oq", KEY_PAD_1 },		/* Keypad 1 */
  {"\033Or", KEY_PAD_2 },		/* Keypad 2 */
  {"\033Os", KEY_PAD_3 },		/* Keypad 3 */
  {"\033Op", KEY_PAD_0 },		/* Keypad 0 */
  {"\033On", KEY_PAD_DECIMAL },		/* Keypad . */
  {"\033OM", KEY_PAD_ENTER },		/* Keypad Enter */
  {"", 0}
};

static Keymap_Scan_Type Linux_Xkeys[] =
{
  {"\033[2~",  KEY_INS | MOVE_MASK },	/* Ins */
  {"\033[3~",  KEY_DEL },		/* Del    Another keyscan is 0x007F */
  {"\033[1~",  KEY_HOME },		/* Ho     Another keyscan is 0x5c00 */
  {"\033[4~",  KEY_END },		/* End    Another keyscan is 0x6100 */
  {"\033[5~",  KEY_PGUP },		/* PgUp */
  {"\033[6~",  KEY_PGDN },		/* PgDn */
  {"\033[A",   KEY_UP },		/* Up */
  {"\033[B",   KEY_DOWN },		/* Dn */
  {"\033[C",   KEY_RIGHT },		/* Ri */
  {"\033[D",   KEY_LEFT },		/* Le */
  {"", 0}
};

static Keymap_Scan_Type Xterm_Xkeys[] =
{
  {"\033[2~",  KEY_INS | MOVE_MASK },	/* Ins */
#if 0
  {"\177",     KEY_DEL },		/* Del  Same as backspace! */
#endif
  {"\033[H",   KEY_HOME },		/* Ho     (rxvt)  */
#if 0
  {"\033[^@",  KEY_HOME },		/* Ho     (xterm) Hmm, Would this work. */
#endif
  {"\033Ow",   KEY_END },		/* End    (rxvt) */
  {"\033[e",   KEY_END },		/* End    (color_xterm) */
  {"\033[K",   KEY_END },		/* End  - Where does this come from ? */
  {"\033[5~",  KEY_PGUP },		/* PgUp */
  {"\033[6~",  KEY_PGDN },		/* PgDn */

  {"\033[7~",  KEY_HOME },		/* Ho     (xterm) */
  {"\033[8~",  KEY_END },		/* End    (xterm) */

  {"\033[A",   KEY_UP },		/* Up */
  {"\033[B",   KEY_DOWN },		/* Dn */
  {"\033[C",   KEY_RIGHT },		/* Ri */
  {"\033[D",   KEY_LEFT },		/* Le */
  {"", 0}
};

static Keymap_Scan_Type vtxxx_fkeys[] =
{
  {"\033[17~", KEY_F6 },		/* F6 */
  {"\033[18~", KEY_F7 },		/* F7 */
  {"\033[19~", KEY_F8 },		/* F8 */
  {"\033[20~", KEY_F9 },		/* F9 */
  {"\033[21~", KEY_F10 },		/* F10 */
  {"\033[23~", KEY_F1  | SHIFT_MASK },	/* Shift F1  (F11 acts like
					 			* Shift-F1) */
  {"\033[24~", KEY_F2  | SHIFT_MASK },	/* Shift F2  (F12 acts like
								 * Shift-F2) */
  {"\033[25~", KEY_F3  | SHIFT_MASK },	/* Shift F3 */
  {"\033[26~", KEY_F4  | SHIFT_MASK },	/* Shift F4 */
  {"\033[28~", KEY_F5  | SHIFT_MASK },	/* Shift F5 */
  {"\033[29~", KEY_F6  | SHIFT_MASK },	/* Shift F6 */
  {"\033[31~", KEY_F7  | SHIFT_MASK },	/* Shift F7 */
  {"\033[32~", KEY_F8  | SHIFT_MASK },	/* Shift F8 */
  {"\033[33~", KEY_F9  | SHIFT_MASK },	/* Shift F9 */
  {"\033[34~", KEY_F10 | SHIFT_MASK },	/* Shift F10 */
  {"", 0}
};

static Keymap_Scan_Type Xterm_fkeys[] =
{
  {"\033[11~", KEY_F1 },		/* F1 */
  {"\033[12~", KEY_F2 },		/* F2 */
  {"\033[13~", KEY_F3 },		/* F3 */
  {"\033[14~", KEY_F4 },		/* F4 */
  {"\033[15~", KEY_F5 },		/* F5 */
  {"", 0}
};

static Keymap_Scan_Type Linux_fkeys[] =
{
  {"\033[[A",  KEY_F1 },		/* F1 */
  {"\033[[B",  KEY_F2 },		/* F2 */
  {"\033[[C",  KEY_F3 },		/* F3 */
  {"\033[[D",  KEY_F4 },		/* F4 */
  {"\033[[E",  KEY_F5 },		/* F5 */
  {"", 0}
};

static Keymap_Scan_Type vtxxx_xkeys[] =
{
/* Who knows which mode it'll be in */
  {"\033OA",   KEY_UP },		/* Up */
  {"\033OB",   KEY_DOWN },		/* Dn */
  {"\033OC",   KEY_RIGHT },		/* Ri */
  {"\033OD",   KEY_LEFT },		/* Le */
  {"\033[A",   KEY_UP },		/* Up */
  {"\033[B",   KEY_DOWN },		/* Dn */
  {"\033[C",   KEY_RIGHT },		/* Ri */
  {"\033[D",   KEY_LEFT },		/* Le */
  {"", 0}
};

static Keymap_Scan_Type rxvt_alt_keys[] =
{
  {"\033\033[11~", ALT_MASK | KEY_F1 },		/* F1 */
  {"\033\033[12~", ALT_MASK | KEY_F2 },		/* F2 */
  {"\033\033[13~", ALT_MASK | KEY_F3 },		/* F3 */
  {"\033\033[14~", ALT_MASK | KEY_F4 },		/* F4 */
  {"\033\033[15~", ALT_MASK | KEY_F5 },		/* F5 */
  {"\033\033[17~", ALT_MASK | KEY_F6 },		/* F6 */
  {"\033\033[18~", ALT_MASK | KEY_F7 },		/* F7 */
  {"\033\033[19~", ALT_MASK | KEY_F8 },		/* F8 */
  {"\033\033[20~", ALT_MASK | KEY_F9 },		/* F9 */
  {"\033\033[21~", ALT_MASK | KEY_F10 },		/* F10 */
  {"\033\033[23~", ALT_MASK | KEY_F1  | SHIFT_MASK },	/* Shift F1  (F11 acts like
					 			* Shift-F1) */
  {"\033\033[24~", ALT_MASK | KEY_F2  | SHIFT_MASK },	/* Shift F2  (F12 acts like
								 * Shift-F2) */
  {"\033\033[25~", ALT_MASK | KEY_F3  | SHIFT_MASK },	/* Shift F3 */
  {"\033\033[26~", ALT_MASK | KEY_F4  | SHIFT_MASK },	/* Shift F4 */
  {"\033\033[28~", ALT_MASK | KEY_F5  | SHIFT_MASK },	/* Shift F5 */
  {"\033\033[29~", ALT_MASK | KEY_F6  | SHIFT_MASK },	/* Shift F6 */
  {"\033\033[31~", ALT_MASK | KEY_F7  | SHIFT_MASK },	/* Shift F7 */
  {"\033\033[32~", ALT_MASK | KEY_F8  | SHIFT_MASK },	/* Shift F8 */
  {"\033\033[33~", ALT_MASK | KEY_F9  | SHIFT_MASK },	/* Shift F9 */
  {"\033\033[34~", ALT_MASK | KEY_F10 | SHIFT_MASK },	/* Shift F10 */

  {"\033\033[2~",  ALT_MASK | MOVE_MASK | KEY_INS },	/* Ins */
  {"\033\177",     ALT_MASK | KEY_DEL },		/* Del */
  {"\033\033[H",   ALT_MASK | KEY_HOME },		/* Ho     (rxvt)  */
  {"\033\033Ow",   ALT_MASK | KEY_END },		/* End    (rxvt) */
  {"\033\033[5~",  ALT_MASK | KEY_PGUP },		/* PgUp */
  {"\033\033[6~",  ALT_MASK | KEY_PGDN },		/* PgDn */
  {"\033\033[A",   ALT_MASK | KEY_UP },			/* Up */
  {"\033\033[B",   ALT_MASK | KEY_DOWN },		/* Dn */
  {"\033\033[C",   ALT_MASK | KEY_RIGHT },		/* Ri */
  {"\033\033[D",   ALT_MASK | KEY_LEFT },		/* Le */
  {"", 0}
};

static Keymap_Scan_Type vtxxx_pfkey[] =
{
/* Keypad keys */
  {"\033OP", KEY_F1 },		/* PF1 */
  {"\033OQ", KEY_F2 },		/* PF2 */
  {"\033OR", KEY_F3 },		/* PF3 */
  {"\033OS", KEY_F4 },		/* PF4 */
  {"", 0}
};

/* Keyboard map for terminfo keys */
static Keymap_Scan_Type terminfo_keys[] =
{
#if SLANG_VERSION > 9934
   {"^(kb)",	KEY_BKSP},	       /* BackSpace */
   {"^(k1)",	KEY_F1},	       /* F1 */
   {"^(k2)",	KEY_F2},	       /* F2 */
   {"^(k3)",	KEY_F3},	       /* F3 */
   {"^(k4)",	KEY_F4},	       /* F4 */
   {"^(k5)",	KEY_F5},	       /* F5 */
   {"^(k6)",	KEY_F6},	       /* F6 */
   {"^(k7)",	KEY_F7},	       /* F7 */
   {"^(k8)",	KEY_F8},	       /* F8 */
   {"^(k9)",	KEY_F9},	       /* F9 */
   {"^(k;)",	KEY_F10},	       /* F10 */
   {"^(F1)",	KEY_F11},	       /* F11 */
   {"^(F2)",	KEY_F12},	       /* F12 */
   {"^(kI)",	KEY_INS|MOVE_MASK},    /* Ins */
   {"^(#3)",	KEY_INS|MOVE_MASK|SHIFT_MASK},   /* Shift Insert */
   {"^(kD)",	KEY_DEL},	       /* Del */
   {"^(*5)",	KEY_DEL|SHIFT_MASK},   /* Shift Del */
   {"^(kh)",	KEY_HOME},	       /* Ho */
   {"^(#2)",	KEY_HOME|SHIFT_MASK},  /* Shift Home */
   {"^(kH)",	KEY_END},	       /* End */
   {"^(@7)",	KEY_END},	       /* End */
   {"^(*7)",	KEY_END|SHIFT_MASK},   /* Shift End */
   {"^(kP)",	KEY_PGUP},	       /* PgUp */
   {"^(kN)",	KEY_PGDN},	       /* PgDn */
   {"^(K1)",	KEY_PAD_7},	       /* Upper Left key on keypad */
   {"^(ku)",	KEY_UP},	       /* Up */
   {"^(K3)",	KEY_PAD_9},	       /* Upper Right key on keypad */
   {"^(kl)",	KEY_LEFT},	       /* Le */
   {"^(#4)",	KEY_LEFT|SHIFT_MASK},  /* Shift Left */
   {"^(K2)",	KEY_PAD_5},	       /* Center key on keypad */
   {"^(kr)",	KEY_RIGHT},	       /* Ri */
   {"^(K4)",	KEY_PAD_1},	       /* Lower Left key on keypad */
   {"^(kd)",	KEY_DOWN},	       /* Dn */
   {"^(K5)",	KEY_PAD_3},	       /* Lower Right key on keypad */
   {"^(%i)",	KEY_RIGHT|SHIFT_MASK}, /* Shift Right */
   {"^(kB)",	KEY_TAB|SHIFT_MASK},   /* Shift Tab -- BackTab */
   {"^(@8)",	KEY_PAD_ENTER}, /* KEY_RETURN? */	/* Enter */

   /* Special keys */
   {"^(&2)",	REDRAW_SCAN_CODE},	/* Refresh */
   {"^(%1)",	HELP_SCAN_CODE},	/* Help */
#endif
   {"", 0}
};

static Keymap_Scan_Type Dosemu_Xkeys[] =
{
/* These keys are laid out like the numbers on the keypad - not too difficult */
  {"^@K0",  KEY_INS|MOVE_MASK}, /* Ins */
  {"^@K1",  KEY_END },		/* End    Another keyscan is 0x6100 */
  {"^@K2",  KEY_DOWN },		/* Dn */
  {"^@K3",  KEY_PGDN },		/* PgDn */
  {"^@K4",  KEY_LEFT },		/* Le */
  {"^@K5",  KEY_PAD_5 },	/* There's no Xkey equlivant */
  {"^@K6",  KEY_RIGHT },	/* Ri */
  {"^@K7",  KEY_HOME },		/* Ho     Another keyscan is 0x5c00 */
  {"^@K8",  KEY_UP },		/* Up */
  {"^@K9",  KEY_PGUP },		/* PgUp */
  {"^@K.",  KEY_DEL },		/* Del    Another keyscan is 0x007F */
  {"^@Kd",  KEY_DEL },		/* Del */

  /* And a few more */
  {"^@Kh",  KEY_PAUSE },	/* Hold or Pause DOS */
  {"^@Kp",  KEY_PRTSCR },	/* Print screen, SysRequest. */
  {"^@Ky",  KEY_SYSRQ },	/* SysRequest. */
  {"^@KS",  KEY_SCROLL },	/* Scroll Lock */
  {"^@KN",  KEY_NUM },		/* Num Lock */

  {"", 0}
};

static Keymap_Scan_Type Dosemu_Ctrl_keys[] =
{
  /* Repair some of the mistakes from 'define_key_from_keymap()' */
  /* REMEMBER, we're pretending this is a us-ascii keyboard */
  {"^C",	KEY_BREAK },
  {"*",		KEY_8 | SHIFT_MASK },
  {"+",		KEY_EQUALS | SHIFT_MASK },

  /* Now setup the shift modifier keys */
  {"^@a",	ALT_KEY_SCAN_CODE },
  {"^@c",	CTRL_KEY_SCAN_CODE },
  {"^@s",	SHIFT_KEY_SCAN_CODE },
  {"^@g",	ALTGR_KEY_SCAN_CODE },

  {"^@A",	STICKY_ALT_KEY_SCAN_CODE },
  {"^@C",	STICKY_CTRL_KEY_SCAN_CODE },
  {"^@S",	STICKY_SHIFT_KEY_SCAN_CODE },
  {"^@G",	STICKY_ALTGR_KEY_SCAN_CODE },

  {"^@k",	KEYPAD_KEY_SCAN_CODE },

  {"^@?",	HELP_SCAN_CODE },
  {"^@h",	HELP_SCAN_CODE },

  {"^@^R",	REDRAW_SCAN_CODE },
  {"^@^L",	REDRAW_SCAN_CODE },
  {"^@^Z",	SUSPEND_SCAN_CODE },
  {"^@ ",	RESET_SCAN_CODE },
  {"^@B",	SET_MONO_SCAN_CODE },

  {"^@\033[A",	SCROLL_UP_SCAN_CODE },
  {"^@\033OA",	SCROLL_UP_SCAN_CODE },
  {"^@U",	SCROLL_UP_SCAN_CODE },

  {"^@\033[B",	SCROLL_DOWN_SCAN_CODE },
  {"^@\033OB",	SCROLL_DOWN_SCAN_CODE },
  {"^@D",	SCROLL_DOWN_SCAN_CODE },

  {"", 0}
};


#if SLANG_VERSION > 9929
# define SLang_define_key1(s,f,t,m) SLkm_define_key((s), (FVOID_STAR)(f), (m))
#endif



static const unsigned char *define_key_keys = 0;
static int define_key_keys_length =0;
static int define_getkey_callback(void)
{
	if (define_key_keys_length == 0) {
		define_key_keys = 0;
	}
	if (!define_key_keys) {
		return 0;
	}
	define_key_keys_length--;
	return *define_key_keys++;
}

/* Note: Later definitions with the same or a conflicting key sequence fail,
 *  and give an error message, but now don't stop the emulator.
 */
static int define_key(const unsigned char *key, unsigned long scan,
		      SLKeyMap_List_Type * m)
{
	unsigned char buf[SLANG_MAX_KEYMAP_KEY_SEQ +1], k1;
	unsigned char buf2[SLANG_MAX_KEYMAP_KEY_SEQ +1];
	int ret;
	const unsigned char *key_str;
	SLang_Key_Type *pre_key;
	int i;

	if (strlen(key) > SLANG_MAX_KEYMAP_KEY_SEQ) {
		k_printf("key string too long %s\n", key); 
		return -1;
	}

	if (SLang_Error) {
		k_printf("Current slang error skipping string %s\n", key);
		return -1;
	}

	if ((*key == '^') && (keyb_state.Esc_Char != '@')) {
		k1 = key[1];
		if (k1 == keyb_state.Esc_Char)
			return 0;		/* ^Esc_Char is not defined here */
		if (k1 == '@') {
			strcpy(buf, key);
			buf[1] = keyb_state.Esc_Char;
			key = buf;
		}
	}

	/* Get the translated keystring, and save a copy */
	key_str = SLang_process_keystring((char *)key);
	memcpy(buf2, key_str, key_str[0]);
	key_str = buf2;

	/* Display in the debug logs what we are defining */
	k_printf("KBD: define ");
	k_printf("'%s'=", strprintable((char *)key));
	for(i = 1; i < key_str[0]; i++) {
		if (i != 1) {
			k_printf(",");
		}
		k_printf("%02x", key_str[i]);
	}
	k_printf(" -> %04lX:%04lX\n", scan >> 16, scan & 0xFFFF);

	if (key_str[0] == 1) {
		k_printf("KBD: no input string skipping\n\n");
		return 0;
	}

	/* Lookup the key to see if we have already defined it */
	define_key_keys = key_str +1;
	define_key_keys_length = key_str[0] -1;
	pre_key = SLang_do_key(m, define_getkey_callback);

	/* Duplicate key definition, warn and ignore it */
	if (pre_key && (pre_key->str[0] == key_str[0]) &&
		(memcmp(pre_key->str, key_str, key_str[0]) == 0)) {
		unsigned long prev_scan;
#if SLANG_VERSION < 9930
		prev_scan = (unsigned long)pre_key->f;
#else
		prev_scan = (unsigned long)pre_key->f.f;
#endif
		
		k_printf("KBD: Previously mapped to: %04lx:%04lx\n\n",
			prev_scan >> 16, prev_scan & 0xFFFF);
		return 0;
	}

	ret = SLang_define_key1((unsigned char *)key, (VOID *) scan, SLKEY_F_INTRINSIC, m);
	if (ret == -2) {  /* Conflicting key error, ignore it */
		k_printf("KBD: Conflicting key: \n\n");
		SLang_Error = 0;
	}
	if (SLang_Error) {
		fprintf(stderr, "Bad key: %s\n", key);
		return -1;
	}
	return 0;
}

static int define_keyset(Keymap_Scan_Type *k, SLKeyMap_List_Type *m)
{
	char *str;
	
	while ((str = k->keystr), (*str != 0)) {
		define_key(str, k->scan_code, m);
		k++;
	}
	return 0;
}

static void define_remaining_characters(SLKeyMap_List_Type *m)
{
	int i;

	for(i = 0; i < 256; i++) {
		unsigned char str[2];
		str[0] = i;
		str[1] = '\0';

		if (define_key(str, KEY_VOID, m) < 0) {
			continue;
		}
	}
}

static int init_slang_keymaps(void)
{
	SLKeyMap_List_Type *m;
	unsigned char buf[5];
	unsigned long esc_scan;
	char * term;
	
	/* Do some sanity checking */
	if (config.term_esc_char >= 32)
		config.term_esc_char = 30;
	
	/* Carriage Return & Escape are not going to be used by any sane person */
	if ((config.term_esc_char == '\x0d') ||
		(config.term_esc_char == '\x1b'))
	{
		config.term_esc_char = 30;
	}
	/* escape characters are identity mapped in unicode. */
	esc_scan = config.term_esc_char;
	esc_scan |= CTRL_MASK;
	
	keyb_state.Esc_Char = config.term_esc_char + '@';
	
	if (keyb_state.The_Normal_KeyMap != NULL)
		return 0;
	
	if (NULL == (m = keyb_state.The_Normal_KeyMap = SLang_create_keymap("Normal", NULL)))
		return -1;
	
	/* Everybody needs these */
	define_keyset(Dosemu_defined_fkeys, m);
	define_keyset(Generic_backspace, m);
	
	/* Keypad in a special way */
	define_keyset(Dosemu_Xkeys, m);
	
	term = getenv("TERM");
	if( term && !strncmp("xterm", term, 5) ) {
		/* Oh no, this is _BAD_, there are so many different things called 'xterm'*/
#if 0
/* This breaks the 8-bit charsets, disable it for now */		
		define_keyset(Meta_ALT, m);		/* For xterms */
#endif
		define_keyset(Esc_ALT, m);		/* For rxvt */
		define_keyset(vtxxx_fkeys, m);
		define_keyset(Xterm_Xkeys, m);
		define_keyset(Xterm_fkeys, m);
		/* The rxvt_alt_keys confilict with ESCESC == ESC in Dosemu_defined_fkeys */
		define_keyset(rxvt_alt_keys, m);	/* For rxvt */
	}
	else if( term && !strncmp("linux", term, 5) ) {
		/* This isn't too nasty */
		
		define_keyset(Esc_ALT, m);
		define_keyset(vtxxx_fkeys, m);
		define_keyset(vtxxx_Keypad, m);
		define_keyset(Linux_Keypad, m);
		define_keyset(Linux_fkeys, m);
		define_keyset(Linux_Xkeys, m);
		
		/* Linux using Meta ALT is _very_ rare, it can only be
		 * changed through an ioctl, which nobody uses (I hope!)
		 */
	}
	else if( term && strcmp("vt52", term) && 
		!strncmp("vt", term, 2) && term[2] >= '1' && term[2] <= '9' ) {
		/* A 'real' VT ... yesss, if you're sure ... */
		
		define_keyset(vtxxx_fkeys, m);
		define_keyset(vtxxx_xkeys, m);
		define_keyset(vtxxx_pfkey, m);
		define_keyset(vtxxx_Keypad, m);
	}
	else {
		/* Ok, the terminfo Must be correct here, but add
		   something to allow for keypad stuff. */
		
#if 0
		/* S-lang codes appears to send ke/ks (Keypad enable/disable) --Eric,  
		 * It might not if you are on the console _and_ you use the
		 * slang keyboard but use a linux console for display, possible?
		 */
		/* NEED This for screen 'cause S-lang doesn't send ke/ks */
		define_keyset(vtxxx_xkeys, m);
#endif
		
#if SLANG_VERSION <= 9934
		/* Then we haven't got terminfo keys - lets get guessing */
		
		define_keyset(vtxxx_xkeys, m);
		define_keyset(vtxxx_fkeys, m);
		define_keyset(vtxxx_pfkey, m);
		define_keyset(vtxxx_Keypad, m);
		define_keyset(Xterm_Xkeys, m);
		define_keyset(Xterm_fkeys, m);
		define_keyset(Linux_Keypad, m);
		define_keyset(Linux_fkeys, m);
		define_keyset(Linux_Xkeys, m);
#endif
	}

	/* Just on the offchance they've done something right! */
	define_keyset(terminfo_keys, m);
	
  /* And more Dosemu keys */
	define_keyset(Dosemu_Ctrl_keys, m);
	
	if (SLang_Error)
		return -1;

	/*
	 * If the erase key (as set by stty) is a reasonably safe one, use it.
	 */
	if( ((keyb_state.erasekey>0 && keyb_state.erasekey<' ')) && 
		keyb_state.erasekey != 27 && keyb_state.erasekey != keyb_state.Esc_Char)
	{
		buf[0] = '^';
		buf[1] = keyb_state.erasekey+'@';
		buf[2] = 0;
		define_key(buf, KEY_BKSP, m);
	} else if (keyb_state.erasekey > '~') {
		buf[0] = keyb_state.erasekey;
		buf[1] = 0;
		define_key(buf, KEY_BKSP, m);
	}
	
	/*
	 * Now add one more for the esc character so that sending it twice sends
	 * it.
	 */
	buf[0] = '^';
	buf[1] = keyb_state.Esc_Char;
	buf[2] = '^';
	buf[3] = keyb_state.Esc_Char;
	buf[4] = 0;
	SLang_define_key1(buf, (VOID *) esc_scan, SLKEY_F_INTRINSIC, m);
	if (SLang_Error)
		return -1;
	
	/* Note: define_keys_by_character comes last or we could never define functions keys. . . */
	define_remaining_characters(m);
	if (SLang_Error)
		return -1;
	
	return 0;
}

/*
 * Global variables this module uses: int kbcount : number of characters
 * in the keyboard buffer to be processed. unsigned char kbbuf[KBBUF_SIZE]
 * : buffer where characters are waiting to be processed unsigned char
 * *kbp : pointer into kbbuf where next char to be processed is.
 */

static int read_some_keys(void)
{
	int cc;
	
	if (keyb_state.kbcount == 0)
		keyb_state.kbp = keyb_state.kbbuf;
	else if (keyb_state.kbp > &keyb_state.kbbuf[(KBBUF_SIZE * 3) / 5]) {
		memmove(keyb_state.kbbuf, keyb_state.kbp, keyb_state.kbcount);
		keyb_state.kbp = keyb_state.kbbuf;
	}
	cc = read(keyb_state.kbd_fd, &keyb_state.kbp[keyb_state.kbcount], KBBUF_SIZE - keyb_state.kbcount - 1);
	k_printf("KBD: cc found %d characters (Xlate)\n", cc);
	if (cc > 0)
		keyb_state.kbcount += cc;
	return cc;
}

/*
 * This function is a callback to read the key.  It returns 0 if one is
 * not ready.
 */


static int getkey_callback(void)
{
	if (keyb_state.kbcount == keyb_state.Keystr_Len)
		read_some_keys();
	if (keyb_state.kbcount == keyb_state.Keystr_Len) {
		keyb_state.KeyNot_Ready = 1;
		return 0;
	}
	return (int)*(keyb_state.kbp + keyb_state.Keystr_Len++);
}

/* DANG_BEGIN_COMMENT
 * sltermio_input_pending is called when a key is pressed and the time
 * till next keypress is important in interpreting the meaning of the
 * keystroke.  -- i.e. ESC
 * DANG_END_COMMENT
 */
static int sltermio_input_pending(void)
{
	struct timeval scr_tv;
       hitimer_t t_start, t_dif;
	fd_set fds;
	
#if 0
#define	THE_TIMEOUT 750000L
#else
#define THE_TIMEOUT 250000L
#endif
	FD_ZERO(&fds);
	FD_SET(keyb_state.kbd_fd, &fds);
	scr_tv.tv_sec = 0L;
	scr_tv.tv_usec = THE_TIMEOUT;
	
	t_start = GETusTIME(0);
	errno = 0;
	while ((int)select(keyb_state.kbd_fd + 1, &fds, NULL, NULL, &scr_tv) < (int)1) {
               t_dif = GETusTIME(0) - t_start;
		
		if ((t_dif >= THE_TIMEOUT) || (errno != EINTR))
			return 0;
		errno = 0;
		scr_tv.tv_sec = 0L;
               scr_tv.tv_usec = THE_TIMEOUT - (long)t_dif;
	}
	return 1;
}


/*
 * If the sticky bits are set, then the scan code or the modifier key has
 * already been taken care of.  In this case, the unsticky bit should be
 * ignored.
 */
static void slang_send_scancode(unsigned long ls_flags, unsigned long lscan)
{
	unsigned long flags = 0;
	
	k_printf("KBD: slang_send_scancode(ls_flags=%08lx, lscan=%08lx)\n", 
		ls_flags, lscan);


	if (ls_flags & KEYPAD_MASK) {
		flags |= KEYPAD_MASK;
		switch(lscan)
		{
		case KEY_INS:    lscan = KEY_PAD_INS; break;
		case KEY_END:    lscan = KEY_PAD_END; break;
		case KEY_DOWN:   lscan = KEY_PAD_DOWN; break;
		case KEY_PGDN:   lscan = KEY_PAD_PGDN; break;
		case KEY_LEFT:   lscan = KEY_PAD_LEFT; break;
		case KEY_RIGHT:  lscan = KEY_PAD_RIGHT; break;
		case KEY_HOME:	 lscan = KEY_PAD_HOME; break;
		case KEY_UP:     lscan = KEY_PAD_UP; break;
		case KEY_PGUP:   lscan = KEY_PAD_PGUP; break;
		case KEY_DEL:    lscan = KEY_PAD_DEL; break;

		case KEY_DASH:   lscan = KEY_PAD_MINUS; break;
		case KEY_RETURN: lscan = KEY_PAD_ENTER; break;
			
		case KEY_0:      lscan = KEY_PAD_0; break;
		case KEY_1:	 lscan = KEY_PAD_1; break;
		case KEY_2:	 lscan = KEY_PAD_2; break;
		case KEY_3:      lscan = KEY_PAD_3; break;
		case KEY_4:      lscan = KEY_PAD_4; break;
		case KEY_5:      lscan = KEY_PAD_5; break;
		case KEY_6:      lscan = KEY_PAD_6; break;
		case KEY_7:      lscan = KEY_PAD_7; break;
		case KEY_9:      lscan = KEY_PAD_9; break;
			
		/* This is a special */
		case KEY_8:		
			if ( ls_flags & SHIFT_MASK ) {
				ls_flags &= ~SHIFT_MASK;
				lscan = KEY_PAD_AST;
			}
			else     lscan =  KEY_PAD_8; break;
		
		/* Need to remove the shift flag for this */
		case KEY_EQUALS:	
			if (ls_flags & SHIFT_MASK ) {
				ls_flags &= ~SHIFT_MASK;
				lscan = KEY_PAD_PLUS;
			} /* else It is silly to translate an equals */
			break;
		
		/* This still generates the wrong scancode - should be $E02F */
		case KEY_SLASH:	 lscan = KEY_PAD_SLASH; break;
		}

	}
	else if( (ls_flags & (ALT_MASK|STICKY_ALT_MASK|ALTGR_MASK|STICKY_ALTGR_MASK))
		&& (lscan == KEY_PRTSCR)) {
		lscan = KEY_SYSRQ;
		ls_flags |= MOVE_MASK;
	}
   
	if ((ls_flags & SHIFT_MASK)
		&& ((ls_flags & STICKY_SHIFT_MASK) == 0)) {
		flags |= SHIFT_MASK;
		move_key(PRESS, KEY_L_SHIFT);
	}
	
	if ((ls_flags & CTRL_MASK)
		&& ((ls_flags & STICKY_CTRL_MASK) == 0)) {
		flags |= CTRL_MASK;
		move_key(PRESS, KEY_L_CTRL);
	}
	
	if ((ls_flags & ALT_MASK)
		&& ((ls_flags & STICKY_ALT_MASK) == 0)) {
		flags |= ALT_MASK;
		move_key(PRESS, KEY_L_ALT);
	}
	
	if ((ls_flags & ALTGR_MASK)
		&& ((ls_flags & STICKY_ALTGR_MASK) == 0)) {
		flags |= ALTGR_MASK;
		move_key(PRESS, KEY_R_ALT);
	}
	
	if (!(ls_flags & MOVE_MASK)) {
		/* For any keys we know do not modify the shiftstate
		 * this is the optimal way to go.  As it handles all
		 * of the weird cases. 
		 */ 
		put_modified_symbol(PRESS, get_shiftstate(), lscan);
		put_modified_symbol(RELEASE, get_shiftstate(), lscan);
	} else {
		/* For the few keys that might modify the shiftstate
		 * we just do a straight forward key press and release.
		 */
		move_key(PRESS,   lscan);
		move_key(RELEASE, lscan);
	}
	
	if (flags & SHIFT_MASK) {
		move_key(RELEASE, KEY_L_SHIFT);
		keyb_state.Shift_Flags &= ~SHIFT_MASK;
	}
	if (flags & CTRL_MASK) {
		move_key(RELEASE, KEY_L_CTRL);
		keyb_state.Shift_Flags &= ~CTRL_MASK;
	}
	if (flags & ALT_MASK) {
		move_key(RELEASE, KEY_L_ALT);
		keyb_state.Shift_Flags &= ~ALT_MASK;
	}
	if (flags & ALTGR_MASK) {
		move_key(RELEASE, KEY_R_ALT);
		keyb_state.Shift_Flags &= ~ALTGR_MASK;
	}
	if (flags & KEYPAD_MASK) {
		keyb_state.Shift_Flags &= ~KEYPAD_MASK;
	}
}

void handle_slang_keys(Boolean make, t_keysym key)
{
	if (!make) {
		return;
	}
	switch(key) {
	case KEY_DOSEMU_HELP:
		DOSemu_Slang_Show_Help = 1;
		break;
	case KEY_DOSEMU_REDRAW:
		dos_slang_redraw();
		break;
	case KEY_DOSEMU_SUSPEND:
		dos_slang_suspend();
		break;
	case KEY_DOSEMU_MONO:
		dos_slang_smart_set_mono();
		break;
	case KEY_DOSEMU_PAN_UP:
		DOSemu_Terminal_Scroll = -1;
	case KEY_DOSEMU_PAN_DOWN:
		DOSemu_Terminal_Scroll = 1;
		break;
	case KEY_DOSEMU_PAN_LEFT: 
		/* this should be implemented someday */
		break;
	case KEY_DOSEMU_PAN_RIGHT:
		/* this should be implemented someday */
		break;
	case KEY_DOSEMU_RESET:
		DOSemu_Slang_Show_Help = 0;
		DOSemu_Terminal_Scroll = 0;
		if (keyb_state.Shift_Flags & STICKY_CTRL_MASK) {
			move_key(RELEASE, KEY_L_CTRL);
		}
		if (keyb_state.Shift_Flags & STICKY_SHIFT_MASK) {
			move_key(RELEASE, KEY_L_SHIFT);
		}
		if (keyb_state.Shift_Flags & STICKY_ALT_MASK) {
			move_key(RELEASE, KEY_L_ALT);
		}
		if (keyb_state.Shift_Flags & STICKY_ALTGR_MASK) {
			move_key(RELEASE, KEY_R_ALT);
		}
		
		keyb_state.Shift_Flags = 0;
	}
	return;
}

static void do_slang_special_keys(unsigned long scan)
{
	static char * keymap_prompts[] = {
		0,
		"[Shift]",
		"[Ctrl]",
		"[Ctrl-Shift]",
		"[Alt]",
		"[Alt-Shift]",
		"[Alt-Ctrl]",
		"[Alt-Ctrl-Shift]",
		"[AltGr]",
		"[AltGr-Shift]",
		"[AltGr-Ctrl]",
		"[AltGr-Ctrl-Shift]",
		"[AltGr-Alt]",
		"[AltGr-Alt-Shift]",
		"[AltGr-Alt-Ctrl]",
		"[AltGr-Alt-Ctrl-Shift]"
	};
	int prompt_no = 0;

	switch (scan) {
	case CTRL_KEY_SCAN_CODE:
		if ( !(keyb_state.Shift_Flags & STICKY_CTRL_MASK))
			keyb_state.Shift_Flags |= CTRL_MASK;
		break;
		
	case STICKY_CTRL_KEY_SCAN_CODE:
		if (keyb_state.Shift_Flags & CTRL_MASK)
			keyb_state.Shift_Flags &= ~CTRL_MASK;
		if (keyb_state.Shift_Flags & STICKY_CTRL_MASK) {
			move_key(RELEASE, KEY_L_CTRL);
			keyb_state.Shift_Flags &= ~STICKY_CTRL_MASK;
		}
		else {
			keyb_state.Shift_Flags |= STICKY_CTRL_MASK;
			move_key(PRESS, KEY_L_CTRL);
		}
		break;
		
	case SHIFT_KEY_SCAN_CODE:
		if ( !(keyb_state.Shift_Flags & STICKY_SHIFT_MASK))
			keyb_state.Shift_Flags |= SHIFT_MASK;
		break;
		
	case STICKY_SHIFT_KEY_SCAN_CODE:
		if (keyb_state.Shift_Flags & SHIFT_MASK)
			keyb_state.Shift_Flags &= ~SHIFT_MASK;
		if (keyb_state.Shift_Flags & STICKY_SHIFT_MASK) {
			move_key(RELEASE, KEY_L_SHIFT);
			keyb_state.Shift_Flags &= ~STICKY_SHIFT_MASK;
		}
		else {
			keyb_state.Shift_Flags |= STICKY_SHIFT_MASK;
			move_key(PRESS, KEY_L_SHIFT);
		}
		break;
		
	case ALT_KEY_SCAN_CODE:
		if ( !(keyb_state.Shift_Flags & STICKY_ALT_MASK))
			keyb_state.Shift_Flags |= ALT_MASK;
		break;
		
	case STICKY_ALT_KEY_SCAN_CODE:
		if (keyb_state.Shift_Flags & ALT_MASK)
			keyb_state.Shift_Flags &= ~ALT_MASK;
		if (keyb_state.Shift_Flags & STICKY_ALT_MASK) {
			move_key(RELEASE, KEY_L_ALT);
			keyb_state.Shift_Flags &= ~STICKY_ALT_MASK;
		}
		else {
			keyb_state.Shift_Flags |= STICKY_ALT_MASK;
			move_key(PRESS, KEY_L_ALT);
		}
		break;
		
	case ALTGR_KEY_SCAN_CODE:
		if ( !(keyb_state.Shift_Flags & STICKY_ALTGR_MASK))
			keyb_state.Shift_Flags |= ALTGR_MASK;
		break;
		
	case STICKY_ALTGR_KEY_SCAN_CODE:
		if (keyb_state.Shift_Flags & ALTGR_MASK)
			keyb_state.Shift_Flags &= ~ALTGR_MASK;
		if (keyb_state.Shift_Flags & STICKY_ALTGR_MASK) {
			move_key(RELEASE, KEY_R_ALT);
			keyb_state.Shift_Flags &= ~STICKY_ALTGR_MASK;
		}
		else {
			keyb_state.Shift_Flags |= STICKY_ALTGR_MASK;
			move_key(PRESS, KEY_R_ALT);
		}
		break;
		
	case KEYPAD_KEY_SCAN_CODE:
		keyb_state.Shift_Flags |= KEYPAD_MASK;
		break;
		
	case SCROLL_DOWN_SCAN_CODE:
		DOSemu_Terminal_Scroll = 1;
		break;
		
	case SCROLL_UP_SCAN_CODE:
		DOSemu_Terminal_Scroll = -1;
		break;
		
	case REDRAW_SCAN_CODE:
		dos_slang_redraw();
		break;
		
	case SUSPEND_SCAN_CODE:
		dos_slang_suspend();
		break;
		
	case HELP_SCAN_CODE:
		DOSemu_Slang_Show_Help = 1;
		break;
		
	case RESET_SCAN_CODE:
		DOSemu_Slang_Show_Help = 0;
		DOSemu_Terminal_Scroll = 0;
		
		if (keyb_state.Shift_Flags & STICKY_CTRL_MASK) {
			move_key(RELEASE, KEY_L_CTRL);
		}
		if (keyb_state.Shift_Flags & STICKY_SHIFT_MASK) {
			move_key(RELEASE, KEY_L_SHIFT);
		}
		if (keyb_state.Shift_Flags & STICKY_ALT_MASK) {
			move_key(RELEASE, KEY_L_ALT);
		}
		if (keyb_state.Shift_Flags & STICKY_ALTGR_MASK) {
			move_key(RELEASE, KEY_R_ALT);
		}
		
		keyb_state.Shift_Flags = 0;
		
		break;
		
	case SET_MONO_SCAN_CODE:
		dos_slang_smart_set_mono();
		break;
	}

	
	if (keyb_state.Shift_Flags & (SHIFT_MASK | STICKY_SHIFT_MASK))	prompt_no += 1;
	if (keyb_state.Shift_Flags & (CTRL_MASK | STICKY_CTRL_MASK)) 	prompt_no += 2;
	if (keyb_state.Shift_Flags & (ALT_MASK | STICKY_ALT_MASK)) 	prompt_no += 4;
	if (keyb_state.Shift_Flags & (ALTGR_MASK | STICKY_ALTGR_MASK))	prompt_no += 8;
	
	DOSemu_Keyboard_Keymap_Prompt = keymap_prompts[prompt_no];
}

static void do_slang_getkeys(void)
{
	SLang_Key_Type *key;

	k_printf("KBD: do_slang_getkeys()\n");
	if (-1 == read_some_keys())
		return;
	
	k_printf("KBD: do_slang_getkeys() found %d bytes\n", keyb_state.kbcount);
	
	/* Now process the keys that are buffered up */
	while (keyb_state.kbcount) {
		unsigned long scan = 0;
		t_unicode symbol = KEY_VOID;

		keyb_state.Keystr_Len = 0;
		keyb_state.KeyNot_Ready = 0;

		key = SLang_do_key(keyb_state.The_Normal_KeyMap, getkey_callback);
		SLang_Error = 0;
		
		if (keyb_state.KeyNot_Ready) {
			if ((keyb_state.Keystr_Len == 1) && (*keyb_state.kbp == 27)) {
				/*
				 * We have an esc character.  If nothing else is available to be
				 * read after a brief period, assume user really wants esc.
				 */
				k_printf("KBD: got ESC character\n");
				if (sltermio_input_pending())
					return;
				
				k_printf("KBD: slang got single ESC\n");
				symbol = KEY_ESC;
				key = NULL;
				/* drop on through to the return for the undefined key below. */
			}
			else
				break;			/* try again next time */
		} 
		
		if (key) {
#if SLANG_VERSION < 9930
			scan = (unsigned long) key->f;
#else
			scan = (unsigned long) key->f.f;
#endif
			symbol = scan & 0xFFFF;
		} 
		if (symbol == KEY_VOID) {
			size_t result;
			/* rough draft version don't stop here... */
			result = charset_to_unicode(&keyb_state.translate_state,
				&symbol, keyb_state.kbp, keyb_state.kbcount);
			k_printf("KBD: got %08x\n", symbol);
		}

		
		keyb_state.kbcount -= keyb_state.Keystr_Len;	/* update count */
		keyb_state.kbp += keyb_state.Keystr_Len;
		
               if (key == NULL && symbol != KEY_ESC) {
			/* undefined key --- return */
			DOSemu_Slang_Show_Help = 0;
			keyb_state.kbcount = 0;
			break;
		}
		
		if (DOSemu_Slang_Show_Help) {
			DOSemu_Slang_Show_Help = 0;
			continue;
		}
		
		
		k_printf("KBD: scan=%08lx Shift_Flags=%08lx str[0]=%d str='%s' len=%d\n",
                       scan,keyb_state.Shift_Flags,key ? key->str[0] : 27,
                       key ? strprintable(key->str+1): "ESC", keyb_state.Keystr_Len);
		if (!(scan&0x80000000)) {
			slang_send_scancode(keyb_state.Shift_Flags | scan, symbol);
		}
		else {
			do_slang_special_keys(scan);
		}
	}
}

/*
 * DANG_BEGIN_FUNCTION setup_pc_scancode_mode
 * 
 * Initialize the keyboard in pc scancode mode.
 * This functionality is ideal but rarely supported on a terminal.
 * 
 * DANG_END_FUNCTION
 */
static void setup_pc_scancode_mode(void)
{
	k_printf("entering pc scancode mode");
	set_shiftstate(0); /* disable all persistent shift state */

	/* enter pc scancode mode */
	SLtt_write_string(SLtt_tgetstr("S4"));
}

/*
 * DANG_BEGIN_FUNCTION exit_pc_scancode_mode
 * 
 * Set the terminal back to a keyboard mode other
 * programs can understand.
 * 
 * DANG_END_FUNCTION
 */
static void exit_pc_scancode_mode(void)
{
	if (keyb_state.pc_scancode_mode) {
		k_printf("leaving pc scancode mode");
		SLtt_write_string(SLtt_tgetstr("S5"));
		keyb_state.pc_scancode_mode = FALSE;
	}
}

/*
 * DANG_BEGIN_FUNCTION do_pc_scancode_getkeys
 * 
 * Set the terminal back to a keyboard mode other
 * programs can understand.
 * 
 * DANG_END_FUNCTION
 */
static void do_pc_scancode_getkeys(void)
{
	if (-1 == read_some_keys()) {
		return;
	}
	k_printf("KBD: do_pc_scancode_getkeys() found %d bytes\n", keyb_state.kbcount);
	
	/* Now process the keys that are buffered up */
	while(keyb_state.kbcount) {
		unsigned char ch = *(keyb_state.kbp++);
		keyb_state.kbcount--;
		put_rawkey(ch);
	}
}

/*
 * DANG_BEGIN_FUNCTION slang_keyb_init()
 * 
 * Code is called at start up to set up the terminal line for non-raw mode.
 * 
 * DANG_END_FUNCTION
 */
   
static int slang_keyb_init(void) 
{
	struct termios buf;

	k_printf("KBD: slang_keyb_init()\n");

	/* First initialize keyb_state */
	memset(&keyb_state, '\0', sizeof(keyb_state));
	keyb_state.kbd_fd = -1;
	keyb_state.kbcount = 0;
	keyb_state.kbp = &keyb_state.kbbuf[0];
	keyb_state.save_kbd_flags = -1;
	keyb_state.pc_scancode_mode = FALSE;
	keyb_state.The_Normal_KeyMap = (void *)0;
	
	keyb_state.Esc_Char = 0;
	keyb_state.erasekey = 0;
	keyb_state.KeyNot_Ready = TRUE;
	keyb_state.Keystr_Len = 0;
	keyb_state.Shift_Flags = 0;
	init_charset_state(&keyb_state.translate_state, trconfig.keyb_charset);

	term_init();
	
	set_shiftstate(0);

	if (SLtt_tgetstr("S4") && SLtt_tgetstr("S5")) {
		keyb_state.pc_scancode_mode = TRUE;
	}
   
	keyb_state.kbd_fd = STDIN_FILENO;
	kbd_fd = keyb_state.kbd_fd; /* FIXME the kbd_fd global!! */
	keyb_state.save_kbd_flags = fcntl(keyb_state.kbd_fd, F_GETFL);
	fcntl(keyb_state.kbd_fd, F_SETFL, O_RDONLY | O_NONBLOCK);
   
	if (tcgetattr(keyb_state.kbd_fd, &keyb_state.save_termios) < 0) {
		error("slang_keyb_init(): Couldn't tcgetattr(kbd_fd,...) errno=%d\n", errno);
		return FALSE;
	}

	buf = keyb_state.save_termios;
	if (keyb_state.pc_scancode_mode) {
		buf.c_iflag = IGNBRK;
	} else {
		buf.c_iflag &= (ISTRIP | IGNBRK | IXON | IXOFF);
	}
	buf.c_cflag &= ~(CLOCAL | CSIZE | PARENB);  
	buf.c_cflag |= CS8;
	buf.c_lflag &= 0;	/* ISIG */
	buf.c_cc[VMIN] = 1;
	buf.c_cc[VTIME] = 0;
	keyb_state.erasekey = buf.c_cc[VERASE];

	if (tcsetattr(keyb_state.kbd_fd, TCSANOW, &buf) < 0) {
		error("slang_keyb_init(): Couldn't tcsetattr(kbd_fd,TCSANOW,...) !\n");
		return FALSE;
	}

	if (keyb_state.pc_scancode_mode) {
		setup_pc_scancode_mode();
		Keyboard_slang.run = do_pc_scancode_getkeys;
	} else {
		if (-1 == init_slang_keymaps()) {
			error("Unable to initialize S-Lang keymaps.\n");
			return FALSE;
		}
		Keyboard_slang.run = do_slang_getkeys;
	}

	if (!isatty(keyb_state.kbd_fd)) {
		k_printf("KBD: Using SIGIO\n");
		add_to_io_select(keyb_state.kbd_fd, 1, keyb_client_run);
	}
	else {
		k_printf("KBD: Not using SIGIO\n");
		add_to_io_select(keyb_state.kbd_fd, 0, keyb_client_run);
	}
   
	k_printf("KBD: slang_keyb_init() ok\n");
	return TRUE;
}

static void slang_keyb_close(void)  
{
	exit_pc_scancode_mode();
	if (tcsetattr(keyb_state.kbd_fd, TCSAFLUSH, &keyb_state.save_termios) < 0) {
		error("slang_keyb_close(): failed to restore keyboard termios settings!\n");
	}
	if (keyb_state.save_kbd_flags != -1) {
		fcntl(keyb_state.kbd_fd, F_SETFL, keyb_state.save_kbd_flags);
	}
	term_close();
	cleanup_charset_state(&keyb_state.translate_state);
}

/*
 * DANG_BEGIN_FUNCTION slang_keyb_probe()
 * 
 * Code is called at start up to see if we can use the slang keyboard.
 * 
 * DANG_END_FUNCTION
 */

static int slang_keyb_probe(void)
{
	int result = FALSE;
	struct termios buf;
	if (tcgetattr(STDIN_FILENO, &buf) >= 0) {
		result = TRUE;
	}
	return result;
}

struct keyboard_client Keyboard_slang =  {
	"slang",                    /* name */
	slang_keyb_probe,           /* probe */
	slang_keyb_init,            /* init */
	NULL,                       /* reset */
	slang_keyb_close,           /* close */
	do_slang_getkeys,           /* run */
	NULL,                       /* set_leds */
};
   
