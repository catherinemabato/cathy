/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef __LOWMEM_H
#define __LOWMEM_H

int lowmem_init(void);
void *lowmem_alloc(int size);
void * lowmem_alloc_aligned(int align, int size);
void lowmem_free(void *p);
void lowmem_reset(void);
extern unsigned char *dosemu_lmheap_base;
#define DOSEMU_LMHEAP_OFFS_OF(s) \
  (((unsigned char *)(s) - dosemu_lmheap_base) + DOSEMU_LMHEAP_OFF)

int get_rm_stack(Bit16u *ss_p, Bit16u *sp_p, uint64_t cookie);
uint16_t put_rm_stack(uint64_t *cookie);
void get_rm_stack_regs(struct vm86_regs *regs, struct vm86_regs *saved_regs);
void rm_stack_enter(void);
void rm_stack_leave(void);

#endif
