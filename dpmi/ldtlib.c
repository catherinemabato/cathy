static char RCSId[] = "$Id: ldtlib.c,v 1.4 1994/03/04 00:03:08 root Exp root $";
static char Copyright[] = "Copyright  Robert J. Amstadt, 1993";

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/unistd.h>
#include <linux/head.h>
#include <linux/ldt.h>

_syscall3(int, modify_ldt, int, func, void *, ptr, unsigned long, bytecount)
     int
      get_ldt(void *buffer)
{
  return modify_ldt(0, buffer, 32 * sizeof(struct modify_ldt_ldt_s));
}

int
set_ldt_entry(int entry, unsigned long base, unsigned int limit,
	      int seg_32bit_flag, int contents, int read_only_flag,
	      int limit_in_pages_flag)
{
  struct modify_ldt_ldt_s ldt_info;

  ldt_info.entry_number = entry;
  ldt_info.base_addr = base;
  ldt_info.limit = limit;
  ldt_info.seg_32bit = seg_32bit_flag;
  ldt_info.contents = contents;
  ldt_info.read_exec_only = read_only_flag;
  ldt_info.limit_in_pages = limit_in_pages_flag;

  return modify_ldt(1, &ldt_info, sizeof(ldt_info));
}
