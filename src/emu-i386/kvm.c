/*
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 *  Emulate vm86() using KVM
 *  Started 2015, Bart Oldeman
 *  References: http://lwn.net/Articles/658511/
 *  plus example at http://lwn.net/Articles/658512/
 */

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>

#include "kvm.h"
#include "emu.h"
#include "emu-ldt.h"
#include "mapping.h"
#include "dpmi.h"
#include "../dosext/dpmi/dpmisel.h"

#define SAFE_MASK (X86_EFLAGS_CF|X86_EFLAGS_PF| \
                   X86_EFLAGS_AF|X86_EFLAGS_ZF|X86_EFLAGS_SF| \
                   X86_EFLAGS_TF|X86_EFLAGS_DF|X86_EFLAGS_OF| \
                   X86_EFLAGS_NT|X86_EFLAGS_AC|X86_EFLAGS_ID) // 0x244dd5
#define RETURN_MASK (SAFE_MASK | 0x28 | X86_EFLAGS_FIXED) // 0x244dff

extern char kvm_mon_start[];
extern char kvm_mon_hlt[];
extern char kvm_mon_end[];

/* V86 monitor structure to run code in V86 mode with VME enabled inside KVM
   This contains:
   1. a TSS with
     a. ss0:esp0 set to a stack at the top of the monitor structure
        This stack contains a copy of the vm86_regs struct.
     b. An interrupt redirect bitmap copied from info->int_revectored
     c. I/O bitmap, for now set to trap all ints. Todo: sync with ioperm()
   2. A GDT with 3 entries
     a. 0 entry
     b. selector 8: flat CS
     c. selector 0x10: flat SS
   3. An IDT with 33 (0x21) entries:
     a. 0x20 entries for all CPU exceptions
     b. a special entry at index 0x20 to interrupt the VM
   4. The stack (from 1a) above
   5. The code pointed to by the IDT entries, from kvmmon.S, on a new page
      This just pushes the exception number, error code, and all registers
      to the stack and executes the HLT instruction which is then trapped
      by KVM.
 */

#define TSS_IOPB_SIZE (65536 / 8)
#define GDT_ENTRIES 3
#undef IDT_ENTRIES
#define IDT_ENTRIES 0x21

#define PG_PRESENT 1
#define PG_RW 2
#define PG_USER 4

#ifndef __x86_64__
#undef MAP_32BIT
#define MAP_32BIT 0
#endif

static struct monitor {
    Task tss;                                /* 0000 */
    /* tss.esp0                                 0004 */
    /* tss.ss0                                  0008 */
    /* tss.IOmapbaseT (word)                    0066 */
    struct revectored_struct int_revectored; /* 0068 */
    unsigned char io_bitmap[TSS_IOPB_SIZE+1];/* 0088 */
    /* TSS last byte (limit)                    2088 */
    unsigned char padding0[0x2100-sizeof(Task)
		-sizeof(struct revectored_struct)
		-(TSS_IOPB_SIZE+1)];
    Descriptor gdt[GDT_ENTRIES];             /* 2100 */
    unsigned char padding1[0x2200-0x2100
	-GDT_ENTRIES*sizeof(Descriptor)];    /* 2118 */
    Gatedesc idt[IDT_ENTRIES];               /* 2200 */
    unsigned char padding2[0x3000-0x2200
	-IDT_ENTRIES*sizeof(Gatedesc)
	-sizeof(unsigned int)
	-sizeof(struct vm86_regs)];          /* 2308 */
    unsigned int cr2;         /* Fault stack at 2FA8 */
    struct vm86_regs regs;
    /* 3000: page directory, 4000: page table */
    unsigned int pde[PAGE_SIZE/sizeof(unsigned int)];
    unsigned int pte[PAGE_SIZE/sizeof(unsigned int)*
		     PAGE_SIZE/sizeof(unsigned int)];
    unsigned char code[PAGE_SIZE];         /* 405000 */
    /* 5000 IDT exception 0 code start
       500A IDT exception 1 code start
       .... ....
       5140 IDT exception 0x21 code start
       514A IDT common code start
       5164 IDT common code end
    */
} *monitor;

static struct kvm_run *run;
static int vmfd, vcpufd;
static void *kvm_maps[0x100];

/* switches KVM virtual machine to vm86 mode */
static void enter_vm86(int vmfd, int vcpufd)
{
  int ret, i;
  struct kvm_regs regs;
  struct kvm_sregs sregs;

  /* trap all I/O instructions with GPF */
  memset(monitor->io_bitmap, 0xff, TSS_IOPB_SIZE+1);

  ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
  if (ret == -1) {
    perror("KVM: KVM_GET_SREGS");
    leavedos(99);
  }

  sregs.tr.base = DOSADDR_REL((unsigned char *)monitor);
  sregs.tr.limit = offsetof(struct monitor, io_bitmap) + TSS_IOPB_SIZE;
  sregs.tr.unusable = 0;
  sregs.tr.type = 0xb;
  sregs.tr.s = 0;
  sregs.tr.dpl = 0;
  sregs.tr.present = 1;
  sregs.tr.avl = 0;
  sregs.tr.l = 0;
  sregs.tr.db = 0;
  sregs.tr.g = 0;

  sregs.ldt.base = DOSADDR_REL((unsigned char *)ldt_buffer);
  sregs.ldt.limit = LDT_ENTRIES * LDT_ENTRY_SIZE - 1;
  sregs.ldt.unusable = 0;
  sregs.ldt.type = 0x2;
  sregs.ldt.s = 0;
  sregs.ldt.dpl = 0;
  sregs.ldt.present = 1;
  sregs.ldt.avl = 0;
  sregs.ldt.l = 0;
  sregs.ldt.db = 0;
  sregs.ldt.g = 0;
  LDT = (Descriptor *)ldt_buffer;

  monitor->tss.ss0 = 0x10;
  monitor->tss.IOmapbase = offsetof(struct monitor, io_bitmap);

  // setup GDT
  sregs.gdt.base = sregs.tr.base + offsetof(struct monitor, gdt);
  sregs.gdt.limit = GDT_ENTRIES * sizeof(Descriptor) - 1;
  for (i=1; i<GDT_ENTRIES; i++) {
    monitor->gdt[i].limit_lo = 0xffff;
    monitor->gdt[i].type = 0xa;
    monitor->gdt[i].S = 1;
    monitor->gdt[i].present = 1;
    monitor->gdt[i].limit_hi = 0xf;
    monitor->gdt[i].DB = 1;
    monitor->gdt[i].gran = 1;
  }
  // based data selector (0x10), to avoid the ESP register corruption bug
  monitor->gdt[GDT_ENTRIES-1].type = 2;
  MKBASE(&monitor->gdt[GDT_ENTRIES-1], DOSADDR_REL((unsigned char *)monitor));

  sregs.idt.base = sregs.tr.base + offsetof(struct monitor, idt);
  sregs.idt.limit = IDT_ENTRIES * sizeof(Gatedesc)-1;
  // setup IDT
  for (i=0; i<IDT_ENTRIES; i++) {
    unsigned int offs = sregs.tr.base + offsetof(struct monitor, code) + i * 16;
    monitor->idt[i].offs_lo = offs & 0xffff;
    monitor->idt[i].offs_hi = offs >> 16;
    monitor->idt[i].seg = 0x8; // FLAT_CODE_SEL
    monitor->idt[i].type = 0xe;
    monitor->idt[i].DPL = 0;
    monitor->idt[i].present = 1;
  }
  memcpy(monitor->code, kvm_mon_start, kvm_mon_end - kvm_mon_start);

  /* setup paging */
  sregs.cr3 = sregs.tr.base + offsetof(struct monitor, pde);
  /* Map and protect monitor */
  mmap_kvm(MAPPING_OTHER, monitor, sizeof(*monitor), PROT_READ|PROT_WRITE);
  mprotect_kvm(MAPPING_OTHER, monitor->code, PAGE_SIZE, PROT_READ|PROT_EXEC);
  /* Map guest memory: LDT, and helper code/data */
  mmap_kvm(MAPPING_DPMI, ldt_buffer, LDT_ENTRIES * LDT_ENTRY_SIZE,
	   PROT_READ|PROT_WRITE);
  mmap_kvm(MAPPING_DPMI, DPMI_sel_code_start, DPMI_SEL_OFF(DPMI_sel_code_end),
	   PROT_READ|PROT_EXEC);
  mmap_kvm(MAPPING_DPMI, DPMI_sel_data_start, DPMI_DATA_OFF(DPMI_sel_data_end),
	   PROT_READ|PROT_WRITE);

  sregs.cr0 |= X86_CR0_PE | X86_CR0_PG;
  sregs.cr4 |= X86_CR4_VME;

  /* setup registers to point to VM86 monitor */
  sregs.cs.base = 0;
  sregs.cs.limit = 0xffffffff;
  sregs.cs.selector = 0x8;
  sregs.cs.db = 1;
  sregs.cs.g = 1;

  sregs.ss.base = sregs.tr.base;
  sregs.ss.limit = 0xffffffff;
  sregs.ss.selector = 0x10;
  sregs.ss.db = 1;
  sregs.ss.g = 1;

  ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
  if (ret == -1) {
    perror("KVM: KVM_SET_SREGS");
    leavedos(99);
  }

  /* just after the HLT */
  regs.rip = sregs.tr.base + offsetof(struct monitor, code) +
    (kvm_mon_hlt - kvm_mon_start) + 1;
  regs.rsp = offsetof(struct monitor, cr2);
  regs.rflags = X86_EFLAGS_FIXED;
  ret = ioctl(vcpufd, KVM_SET_REGS, &regs);
  if (ret == -1) {
    perror("KVM: KVM_SET_REGS");
    leavedos(99);
  }
}

/* Initialize KVM and memory mappings */
int init_kvm_cpu(void)
{
  struct kvm_cpuid *cpuid;
  int kvm, ret, mmap_size;

  kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
  if (kvm == -1) {
    warn("KVM: error opening /dev/kvm: %s\n", strerror(errno));
    return 0;
  }

  vmfd = ioctl(kvm, KVM_CREATE_VM, (unsigned long)0);
  if (vmfd == -1) {
    warn("KVM: KVM_CREATE_VM: %s\n", strerror(errno));
    return 0;
  }

  /* this call is only there to shut up the kernel saying
     "KVM_SET_TSS_ADDR need to be called before entering vcpu"
     this is only really needed if the vcpu is started in real mode and
     the kernel needs to emulate that using V86 mode, as is necessary
     on Nehalem and earlier Intel CPUs */
  unsigned char *scratch = mmap(NULL, 3*PAGE_SIZE, PROT_NONE,
				MAP_ANONYMOUS|MAP_PRIVATE|MAP_32BIT, -1, 0);
  ret = ioctl(vmfd, KVM_SET_TSS_ADDR, (unsigned long)DOSADDR_REL(scratch));
  if (ret == -1) {
    warn("KVM: KVM_SET_TSS_ADDR: %s\n", strerror(errno));
    return 0;
  }

  vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long)0);
  if (vcpufd == -1) {
    warn("KVM: KVM_CREATE_VCPU: %s\n", strerror(errno));
    return 0;
  }

  cpuid = malloc(sizeof(*cpuid) + 2*sizeof(cpuid->entries[0]));
  cpuid->nent = 2;
  // Use the same values as in emu-i386/simx86/interp.c
  // (Pentium 133-200MHz, "GenuineIntel")
  cpuid->entries[0] = (struct kvm_cpuid_entry) { .function = 0,
    .eax = 1, .ebx = 0x756e6547, .ecx = 0x6c65746e, .edx = 0x49656e69 };
  // family 5, model 2, stepping 12, fpu vme de pse tsc msr mce cx8
  cpuid->entries[1] = (struct kvm_cpuid_entry) { .function = 1,
    .eax = 0x052c, .ebx = 0, .ecx = 0, .edx = 0x1bf };
  ret = ioctl(vcpufd, KVM_SET_CPUID, cpuid);
  free(cpuid);
  if (ret == -1) {
    warn("KVM: KVM_SET_CPUID: %s\n", strerror(errno));
    return 0;
  }

  mmap_size = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, NULL);
  if (mmap_size == -1) {
    warn("KVM: KVM_GET_VCPU_MMAP_SIZE: %s\n", strerror(errno));
    return 0;
  }
  if (mmap_size < sizeof(*run)) {
    warn("KVM: KVM_GET_VCPU_MMAP_SIZE unexpectedly small\n");
    return 0;
  }
  run = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0);
  if (run == MAP_FAILED) {
    warn("KVM: mmap vcpu: %s\n", strerror(errno));
    return 0;
  }

  /* create monitor structure in memory */
  monitor = mmap(NULL, sizeof(*monitor), PROT_READ | PROT_WRITE,
		 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  return 1;
}

static int kvm_find_map_slot(void)
{
  int i;
  for (i = 0; i < sizeof(kvm_maps)/sizeof(kvm_maps[0]); i++) {
    if (kvm_maps[i] == NULL)
      return i;
  }
  error("FATAL: ran out of KVM mapping slots\n");
  leavedos(99);
  return -1;
}

void mmap_kvm(int cap, void *addr, size_t mapsize, int protect)
{
  int pg_user, ret;
  unsigned int page;
  size_t pagesize = sysconf(_SC_PAGESIZE);
  uintptr_t alignaddr = (uintptr_t)addr & ~(pagesize-1);
  uintptr_t alignend = ((uintptr_t)addr + mapsize + pagesize-1) & ~(pagesize-1);
  unsigned int start = DOSADDR_REL((unsigned char *)alignaddr) / pagesize;
  unsigned int end = DOSADDR_REL((unsigned char *)alignend) / pagesize;
  unsigned int limit = (LOWMEM_SIZE + HMASIZE) / pagesize;
  struct kvm_userspace_memory_region region = {
    .slot = kvm_find_map_slot(),
    .guest_phys_addr = start * pagesize,
    .memory_size = alignend - alignaddr,
    .userspace_addr = alignaddr,
  };

  if (config.cpu_vm_dpmi != CPUVM_KVM && !(cap & MAPPING_OTHER)) {
    if (start >= limit || monitor == NULL) return;
    if (end > limit) end = limit;
  }

  if (!(cap & (MAPPING_INIT_LOWRAM|MAPPING_LOWMEM|MAPPING_EMS|MAPPING_HMA|
	       MAPPING_DPMI|MAPPING_VGAEMU|MAPPING_OTHER)))
    return;

  if (monitor->pte[start]) {
    mprotect_kvm(cap, addr, mapsize, protect);
    return;
  }

  Q_printf("KVM: mapping %p:%zx to %llx for slot %d with prot %x\n", addr,
	   mapsize, region.guest_phys_addr, region.slot, protect);

  ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
  if (ret == -1) {
    perror("KVM: KVM_SET_USER_MEMORY_REGION");
    leavedos(99);
  }

  /* adjust paging structures in VM */
  pg_user = (cap & MAPPING_OTHER) ? 0 : PG_USER;
  for (page = start; page < end; page++) {
    int pde_entry = page >> 10;
    unsigned int pde0 = DOSADDR_REL((unsigned char *)&monitor->pte)
      | (PG_PRESENT | PG_RW | PG_USER);
    if (monitor->pde[pde_entry] == 0) {
      monitor->pde[pde_entry] = pde0 + pde_entry*PAGE_SIZE;
    }
    monitor->pte[page] = (page * pagesize) | pg_user;
  }
  if (cap & MAPPING_INIT_LOWRAM)
    cap |= MAPPING_LOWMEM;
  mprotect_kvm(cap, addr, mapsize, protect);
  kvm_maps[region.slot] = addr;
}

void munmap_kvm(int cap, void *addr)
{
  int i;

  if (!(cap & (MAPPING_INIT_LOWRAM|MAPPING_LOWMEM|MAPPING_EMS|MAPPING_HMA|
	       MAPPING_DPMI|MAPPING_VGAEMU|MAPPING_OTHER)))
    return;

  for (i = 0; i < sizeof(kvm_maps)/sizeof(kvm_maps[0]); i++) {
    if (kvm_maps[i] == addr) {
      struct kvm_userspace_memory_region region = { .slot = i };
      int ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
      if (ret == -1) {
	perror("KVM: KVM_SET_USER_MEMORY_REGION");
	leavedos(99);
      }
      kvm_maps[i] = NULL;
      break;
    }
  }
}

void mprotect_kvm(int cap, void *addr, size_t mapsize, int protect)
{
  unsigned int page;
  size_t pagesize = sysconf(_SC_PAGESIZE);
  uintptr_t alignaddr = (uintptr_t)addr & ~(pagesize-1);
  uintptr_t alignend = ((uintptr_t)addr + mapsize + pagesize-1) & ~(pagesize-1);
  unsigned int start = DOSADDR_REL((unsigned char *)alignaddr) / pagesize;
  unsigned int end = DOSADDR_REL((unsigned char *)alignend) / pagesize;
  unsigned int limit = (LOWMEM_SIZE + HMASIZE) / pagesize;

  if (config.cpu_vm_dpmi != CPUVM_KVM && !(cap & MAPPING_OTHER)) {
    if (start >= limit || monitor == NULL) return;
    if (end > limit) end = limit;
  }

  if (!(cap & (MAPPING_LOWMEM|MAPPING_EMS|MAPPING_HMA|MAPPING_VGAEMU|
	       MAPPING_DPMI|MAPPING_OTHER)))
    return;

  Q_printf("KVM: protecting %p:%zx to %zx with prot %x\n", addr,
	   mapsize, start * pagesize, protect);

  for (page = start; page < end; page++) {
    monitor->pte[page] &= ~(PG_PRESENT | PG_RW);
    if (protect & PROT_WRITE)
      monitor->pte[page] |= PG_PRESENT | PG_RW;
    else if (protect & PROT_READ)
      monitor->pte[page] |= PG_PRESENT;
  }
}

/* This function works like handle_vm86_fault in the Linux kernel,
   except:
   * since we use VME we only need to handle
     PUSHFD, POPFD, IRETD always
     POPF, IRET only if it sets TF or IF with VIP set
     STI only if VIP is set and VIF was not set
     INT only if it is revectored
   * The Linux kernel splits the CPU flags into on-CPU flags and
     flags (VFLAGS) IOPL, NT, AC, and ID that are kept on the stack.
     Here all those flags are merged into on-CPU flags, with the
     exception of IOPL. IOPL is always set to 0 on the CPU,
     and to 3 on the stack with PUSHF
*/
static int kvm_handle_vm86_fault(struct vm86_regs *regs, unsigned int cpu_type)
{
  unsigned char opcode;
  int data32 = 0, pref_done = 0;
  unsigned int csp = regs->cs << 4;
  unsigned int ssp = regs->ss << 4;
  unsigned short ip = regs->eip & 0xffff;
  unsigned short sp = regs->esp & 0xffff;
  unsigned int orig_flags = regs->eflags;
  int ret = -1;

  do {
    switch (opcode = popb(csp, ip)) {
    case 0x66:      /* 32-bit data */     data32 = 1; break;
    case 0x67:      /* 32-bit address */  break;
    case 0x2e:      /* CS */              break;
    case 0x3e:      /* DS */              break;
    case 0x26:      /* ES */              break;
    case 0x36:      /* SS */              break;
    case 0x65:      /* GS */              break;
    case 0x64:      /* FS */              break;
    case 0xf2:      /* repnz */           break;
    case 0xf3:      /* rep */             break;
    default: pref_done = 1;
    }
  } while (!pref_done);

  switch (opcode) {

  case 0x9c: { /* only pushfd faults with VME */
    unsigned int flags = regs->eflags & RETURN_MASK;
    if (regs->eflags & X86_EFLAGS_VIF)
      flags |= X86_EFLAGS_IF;
    flags |= X86_EFLAGS_IOPL;
    pushl(ssp, sp, flags);
    break;
  }

  case 0xcd: { /* int xx */
    int intno = popb(csp, ip);
    ret = VM86_INTx + (intno << 8);
    break;
  }

  case 0xcf: /* iret */
    if (data32) {
      ip = popl(ssp, sp);
      regs->cs = popl(ssp, sp);
    } else {
      ip = popw(ssp, sp);
      regs->cs = popw(ssp, sp);
    }
    /* fall through into popf */
  case 0x9d: { /* popf */
    unsigned int newflags;
    if (data32) {
      newflags = popl(ssp, sp);
      if (cpu_type >= CPU_286 && cpu_type <= CPU_486) {
	newflags &= ~X86_EFLAGS_ID;
	if (cpu_type < CPU_486)
	  newflags &= ~X86_EFLAGS_AC;
      }
      regs->eflags &= ~SAFE_MASK;
    } else {
      /* must have VIP or TF set in VME, otherwise does not trap */
      newflags = popw(ssp, sp);
      regs->eflags &= ~(SAFE_MASK & 0xffff);
    }
    regs->eflags |= newflags & SAFE_MASK;
    if (newflags & X86_EFLAGS_IF) {
      regs->eflags |= X86_EFLAGS_VIF;
      if (orig_flags & X86_EFLAGS_VIP) ret = VM86_STI;
    } else {
      regs->eflags &= ~X86_EFLAGS_VIF;
    }
    break;
  }

  case 0xfb: /* STI */
    /* must have VIP set in VME, otherwise does not trap */
    regs->eflags |= X86_EFLAGS_VIF;
    ret = VM86_STI;
    break;

  default:
    return VM86_UNKNOWN;
  }

  regs->esp = (regs->esp & 0xffff0000) | sp;
  regs->eip = (regs->eip & 0xffff0000) | ip;
  if (ret != -1)
    return ret;
  if (orig_flags & X86_EFLAGS_TF)
    return VM86_TRAP + (1 << 8);
  return ret;
}

/* Inner loop for KVM, runs until HLT */
static void kvm_run(struct vm86_regs *regs)
{
  unsigned int exit_reason;

  do {
    int ret = ioctl(vcpufd, KVM_RUN, NULL);

    /* KVM should only exit for three reasons:
       1. KVM_EXIT_HLT: at the hlt in kvmmon.S.
       2. KVM_EXIT_INTR: (with ret==-1) after a signal. In this case we
          re-enter KVM with int 0x20 injected (if possible) so it will fault
          with KMV_EXIT_HLT.
       3. KVM_EXIT_IRQ_WINDOW_OPEN: if it is not possible to inject interrupts
          KVM is re-entered asking it to exit when interrupt injection is
          possible, then it exits with this code. This only happens if a signal
          occurs during execution of the monitor code in kvmmon.S.
    */
    exit_reason = run->exit_reason;
    if (ret == -1 && errno == EINTR) {
      exit_reason = KVM_EXIT_INTR;
    } else if (ret != 0) {
      perror("KVM: KVM_RUN");
      leavedos(99);
    }

    switch (exit_reason) {
    case KVM_EXIT_HLT:
      break;
    case KVM_EXIT_IRQ_WINDOW_OPEN:
    case KVM_EXIT_INTR:
      run->request_interrupt_window = !run->ready_for_interrupt_injection;
      if (run->ready_for_interrupt_injection) {
	struct kvm_interrupt interrupt = (struct kvm_interrupt){.irq = 0x20};
	ret = ioctl(vcpufd, KVM_INTERRUPT, &interrupt);
	if (ret == -1) {
	  perror("KVM: KVM_INTERRUPT");
	  leavedos(99);
	}
      }
      break;
    case KVM_EXIT_FAIL_ENTRY:
      fprintf(stderr,
	      "KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason = 0x%llx\n",
	      (unsigned long long)run->fail_entry.hardware_entry_failure_reason);
      leavedos(99);
    case KVM_EXIT_INTERNAL_ERROR:
      fprintf(stderr,
	      "KVM_EXIT_INTERNAL_ERROR: suberror = 0x%x\n", run->internal.suberror);
      leavedos(99);
    default:
      fprintf(stderr, "KVM: exit_reason = 0x%x\n", exit_reason);
      leavedos(99);
    }
  } while (exit_reason != KVM_EXIT_HLT);
}

/* Emulate vm86() using KVM */
int kvm_vm86(struct vm86_struct *info)
{
  struct vm86_regs *regs;
  int vm86_ret;
  unsigned int trapno;
  static int first = 1;

  if (first) {
    enter_vm86(vmfd, vcpufd);
    warn("Using V86 mode inside KVM\n");
    first = 0;
  }

  regs = &monitor->regs;
  *regs = info->regs;
  monitor->int_revectored = info->int_revectored;
  monitor->tss.esp0 = offsetof(struct monitor, regs) + sizeof(monitor->regs);

  regs->eflags &= (SAFE_MASK | X86_EFLAGS_VIF | X86_EFLAGS_VIP);
  regs->eflags |= X86_EFLAGS_FIXED | X86_EFLAGS_VM | X86_EFLAGS_IF;

  do {
    kvm_run(regs);

    /* high word(orig_eax) = exception number */
    /* low word(orig_eax) = error code */
    trapno = regs->orig_eax >> 16;
    vm86_ret = VM86_SIGNAL;
    if (trapno == 1 || trapno == 3)
      vm86_ret = VM86_TRAP | (trapno << 8);
    else if (trapno == 0xd)
      vm86_ret = kvm_handle_vm86_fault(regs, info->cpu_type);
  } while (vm86_ret == -1);

  info->regs = *regs;
  trapno = regs->orig_eax >> 16;
  if (vm86_ret == VM86_SIGNAL && trapno != 0x20) {
    struct sigcontext sc, *scp = &sc;
    _cr2 = (uintptr_t)MEM_BASE32(monitor->cr2);
    _trapno = trapno;
    _err = regs->orig_eax & 0xffff;
    _dosemu_fault(SIGSEGV, scp);
  }
  return vm86_ret;
}

/* Emulate dpmi_control() using KVM */
int kvm_dpmi(struct sigcontext *scp)
{
  struct vm86_regs *regs;
  int ret;
  unsigned int trapno;

  monitor->tss.esp0 = offsetof(struct monitor, regs) +
    offsetof(struct vm86_regs, es);

  regs = &monitor->regs;
  do {
    regs->eax = _eax;
    regs->ebx = _ebx;
    regs->ecx = _ecx;
    regs->edx = _edx;
    regs->esi = _esi;
    regs->edi = _edi;
    regs->ebp = _ebp;
    regs->esp = _esp;
    regs->eip = _eip;

    regs->cs = _cs;
    regs->__null_ds = _ds;
    regs->__null_es = _es;
    regs->ss = _ss;
    regs->__null_fs = _fs;
    regs->__null_gs = _gs;

    regs->eflags = _eflags;
    regs->eflags &= (SAFE_MASK | X86_EFLAGS_VIF | X86_EFLAGS_VIP);
    regs->eflags |= X86_EFLAGS_FIXED | X86_EFLAGS_IF;

    kvm_run(regs);

    /* orig_eax >> 16 = exception number */
    /* orig_eax & 0xffff = error code */
    trapno = regs->orig_eax >> 16;

    _eax = regs->eax;
    _ebx = regs->ebx;
    _ecx = regs->ecx;
    _edx = regs->edx;
    _esi = regs->esi;
    _edi = regs->edi;
    _ebp = regs->ebp;
    _esp = regs->esp;
    _eip = regs->eip;

    _cs = regs->cs;
    _ds = regs->__null_ds;
    _es = regs->__null_es;
    _ss = regs->ss;
    _fs = regs->__null_fs;
    _gs = regs->__null_gs;

    _eflags = regs->eflags;

    ret = -1; /* mirroring sigio/sigalrm */
    if (trapno != 0x20) {
      _cr2 = (uintptr_t)MEM_BASE32(monitor->cr2);
      _trapno = trapno;
      _err = regs->orig_eax & 0xffff;
      ret = _dosemu_fault(SIGSEGV, scp);
    }
  } while (ret == 0);
  return ret;
}
