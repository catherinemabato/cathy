/* 
 * (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 
 * DANG_BEGIN_MODULE
 * 
 * Description: Keyboard backend - interface to the DOS side
 * 
 * Maintainer: Eric Biederman
 * 
 * REMARK
 * This module handles interfacing to the DOS side both on int9/port60h level, 
 * or on the bios buffer level.
 * Keycodes are buffered in a queue, which, however, has limited depth, so it
 * shouldn't be used for pasting.
 *
 * More information about this module is in doc/README.newkbd
 *
 * /REMARK
 * DANG_END_MODULE
 *
 */

#include <string.h>
#include <stdlib.h>

#include "emu.h"
#include "types.h"
#include "keyboard.h"
#include "keyb_server.h"
#include "keyb_clients.h"
#include "bios.h"
#include "pic.h"
#include "cpu.h"
#include "keystate.h"

/* If this is set to 1, the server will check whether the BIOS keyboard buffer is
 * full.
 * This is somewhat inaccurate emulation, as the state of BIOS variables really
 * shouldn't affect 'hardware' behaviour, but this seems the only way of knowing how
 * fast DOS is processing keystrokes, in particular for pasting.
 */
#define KEYBUF_HACK 1

/********** QUEUE ***********/

/*
 * This is the dosemu keyboard queue.
 * Each queue entry holds a data structure corresponding to (mostly)
 * one keypress or release event. [The exception are the braindead
 * 0xe02a / 0xe0aa shift key emulation codes the keyboard processor
 * 'decorates' some kinds of keyboard events, which for convenience
 * are treated as seperate events.]
 * Each queue entry holds a up to 4 bytes of raw keycodes for the
 * port 60h emulation, along with a 2-byte translated int16h keycode
 * and the shift state after this event was processed.
 * Note that the bios_key field can be empty (=0), e.g. for shift keys,
 * while the raw field should always contain something.
 */

struct keyboard_queue keyb_queue = {
0, 0, 0, 0
};

static inline Boolean queue_empty(struct keyboard_queue *q) 
{
	return (q->head == q->tail);
}


int queue_level(struct keyboard_queue *q) 
{
	int n;
	/* q->tail is the first item to pop
	 * q->head is the place to write the next item
	 */
	n = q->head - q->tail;
	return (n < 0) ? n + q->size : n;
}

static inline Boolean queue_full(struct keyboard_queue *q) 
{
	return (q->size == 0) || (queue_level(q) == (q->size - 1));
}

/*
 * this has to work even if the variables are uninitailized!
 */
void clear_queue(struct keyboard_queue *q) 
{
	q->head = q->tail = 0;
	k_printf("KBD: clear_queue() queuelevel=0\n");
}

void write_queue(struct keyboard_queue *q, t_rawkeycode raw) 
{
	int qh;

	k_printf("KBD: writing to queue: scan=%08x\n",
		(unsigned int)raw);

	if (queue_full(q)) {
		/* If the queue is full grow it */
		t_rawkeycode *new;
		int sweep1, sweep2;
		new = malloc(q->size + KEYB_QUEUE_LENGTH);
		if (!new) {
			k_printf("KBD: queue overflow!\n");
			return;
		}
		k_printf("KBD: resize queue %d->%d head=%d tail=%d level=%d\n",
			 q->size, q->size + KEYB_QUEUE_LENGTH, q->head, q->tail, queue_level(q));
		if (q->tail <= q->head) {
			sweep1 = q->head - q->tail;
			sweep2 = 0;
		} else {
			sweep1 = q->size - q->tail;
			sweep2 = q->head;
			
		}
		memcpy(new, q->queue + q->tail, sweep1);
		memcpy(new + sweep1, q->queue, sweep2);
		
		free(q->queue);
		q->tail = 0;
		q->head = sweep1 + sweep2;
		q->size += KEYB_QUEUE_LENGTH;
		q->queue = new;
	}
	qh = q->head;
	if (++qh == q->size) 
		qh = 0;
	if (qh == q->tail) {
		k_printf("KBD: queue overflow!\n");
		return;
	}
	q->queue[q->head] = raw;
	q->head = qh;
	k_printf("KBD: queuelevel=%d\n", queue_level(q));
}



t_rawkeycode read_queue(struct keyboard_queue *q) 
{
	t_rawkeycode *qp;
	t_rawkeycode raw = 0;
	
	if (!queue_empty(q)) {
		qp = &q->queue[q->tail];
		
		raw = *qp;
		if (++q->tail == q->size) q->tail = 0;
	}
	return raw;
}

/****************** END QUEUE *******************/




/********************** (BIOS) mode backend ***************/

/*
 *    Interface to DOS (BIOS keyboard buffer/shiftstate flags)
 */

void clear_bios_keybuf() 
{
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_START,0x001e);
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_END,  0x003e);
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_HEAD, 0x001e);
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_TAIL, 0x001e);
   MEMSET_DOS(BIOS_KEYBOARD_BUFFER,0,32);
}

static inline Boolean bios_keybuf_full(void) 
{
   int start,end,head,tail;

   start = READ_WORD(BIOS_KEYBOARD_BUFFER_START);
   end   = READ_WORD(BIOS_KEYBOARD_BUFFER_END);
   head  = READ_WORD(BIOS_KEYBOARD_BUFFER_HEAD);
   tail  = READ_WORD(BIOS_KEYBOARD_BUFFER_TAIL);
   
   tail+=2;
   if (tail==end) tail=start;
   return (tail==head);
}

static inline void put_bios_keybuf(Bit16u scancode) 
{
   int start,end,head,tail;

   k_printf("KBD: put_bios_keybuf(%04x)\n",(unsigned int)scancode);

   start = READ_WORD(BIOS_KEYBOARD_BUFFER_START);
   end   = READ_WORD(BIOS_KEYBOARD_BUFFER_END);
   head  = READ_WORD(BIOS_KEYBOARD_BUFFER_HEAD);
   tail  = READ_WORD(BIOS_KEYBOARD_BUFFER_TAIL);
   
   WRITE_WORD(0x400+tail,scancode);
   tail+=2;
   if (tail==end) tail=start;
   if (tail==head) {
      k_printf("KBD: BIOS keyboard buffer overflow\n");
      return;
   }
   
   WRITE_WORD(BIOS_KEYBOARD_BUFFER_TAIL,tail);
}

/*
 * update the seg 0x40 keyboard flags from dosemu's internal 'shiftstate'
 * variable.
 * This is called either from kbd_process() or the get_bios_key() helper. 
 * It is never called if a dos application takes complete
 * control of int9.
 */

void copy_shift_state(t_shiftstate shift) 
{
   Bit8u flags1, flags2, flags3, leds;

#if 0
   k_printf("KBD: copy_shift_state() %04x\n",shift);
#endif
   
   flags1=flags3=leds=0;
   /* preserve pause bit */
   flags2 = READ_BYTE(BIOS_KEYBOARD_FLAGS2) & PAUSE_MASK;
   
   if (shift & INS_LOCK)      flags1 |= 0x80;
   if (shift & CAPS_LOCK)   { flags1 |= 0x40;  leds |= 0x04; }
   if (shift & NUM_LOCK)    { flags1 |= 0x20;  leds |= 0x02; }
   if (shift & SCR_LOCK)    { flags1 |= 0x10;  leds |= 0x01; }
   if (shift & ANY_ALT)       flags1 |= 0x08;
   if (shift & ANY_CTRL)      flags1 |= 0x04;
   if (shift & L_SHIFT)       flags1 |= 0x02;
   if (shift & R_SHIFT)       flags1 |= 0x01;

   if (shift & INS_PRESSED)   flags2 |= 0x80;
   if (shift & CAPS_PRESSED)  flags2 |= 0x40;
   if (shift & NUM_PRESSED)   flags2 |= 0x20;
   if (shift & SCR_PRESSED)   flags2 |= 0x10;
   if (shift & SYSRQ_PRESSED) flags2 |= 0x04;
   if (shift & L_ALT)         flags2 |= 0x02;
   if (shift & L_CTRL)        flags2 |= 0x01;

   flags3 |= 0x10;  /* set MF101/102 keyboard flag */
   if (shift & R_ALT)         flags3 |= 0x08;
   if (shift & R_CTRL)        flags3 |= 0x04;

   WRITE_BYTE(BIOS_KEYBOARD_FLAGS1,flags1);
   WRITE_BYTE(BIOS_KEYBOARD_FLAGS2,flags2);
   WRITE_BYTE(BIOS_KEYBOARD_FLAGS3,flags3);
   WRITE_BYTE(BIOS_KEYBOARD_LEDS,leds);
}


Bit16u get_bios_key(t_rawkeycode raw) 
{
	Boolean make;
	t_keynum key;
	Bit16u bios_key = 0;
	key = compute_keynum(&make, raw, &dos_keyboard_state.raw_state);
	key = compute_functional_keynum(make, key, &dos_keyboard_state.keys_pressed);
	if (key != NUM_VOID) {
		bios_key = translate_key(make, key, &dos_keyboard_state);
		copy_shift_state(dos_keyboard_state.shiftstate);
		keyb_client_set_leds(get_modifiers_r(dos_keyboard_state.shiftstate));
	}
	return bios_key;
}

/****************** KEYBINT MODE BACKEND *******************/

/* run the queue backend in keybint=on mode
 * called either periodically from keyb_server_run or, for faster response,
 * when writing to the queue and after the IRQ1 handler is finished.
 */
void int_check_queue(void) 
{
   t_rawkeycode rawscan;
   
#if 0
   k_printf("KBD: int_check_queue(): queue_empty=%d port60_ready=%d bios_keybuf_full=%d\n",
	    queue_empty(&keyb_queue), port60_ready, bios_keybuf_full());
#endif

   if (queue_empty(&keyb_queue))
      return;
   
   if (int9_running) {
      k_printf("KBD: int9 running\n");
      return;
   }
   
#if 1
   if (port60_ready) {
      k_printf("KBD: port60 still has data\n");
      return;
   }
#endif   

   if (!port60_ready
#if KEYBUF_HACK
       && (!bios_keybuf_full() || 
	   (READ_BYTE(BIOS_KEYBOARD_FLAGS2) & PAUSE_MASK))
#endif
       )
   {
      rawscan = read_queue(&keyb_queue);
      k_printf("KBD: read queue: raw=%02x\n",
               rawscan);
      k_printf("KBD: queuelevel=%d\n",queue_level(&keyb_queue));

      output_byte_8042(rawscan);
   }
}

/******************* GENERAL ********************************/


void backend_run(void) 
{
   static int running = 0;

   /* avoid re-entrance problems */
   if (running) {
      k_printf("KBD: backend_run cancelled\n");
      return;
   }
   running++;
   
   int_check_queue();
   
   running--;
}


void backend_reset() 
{
   clear_queue(&keyb_queue);

/* initialise keyboard-related BIOS variables */

   WRITE_BYTE(BIOS_KEYBOARD_TOKEN,0);  /* buffer for Alt-XXX (not used by emulator) */
   
   clear_bios_keybuf();
   copy_shift_state(dos_keyboard_state.shiftstate);
   keyb_client_set_leds(get_modifiers_r(dos_keyboard_state.shiftstate));
}
