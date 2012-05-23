/* hdinfo.c
 *
 * 6/2/93, gt8134b@prism.gatech.edu, Robert Sanders
 *
 * print the partition information for a given disk file (device or image).
 * Print the byte offset from the beginning of the file to the
 * start of the partition, which is useful for mounting the loop
 * device (i.e. mounting a file), which is why I wrote this.
 *
 * It's made slightly more complicated by the header prepended to
 * dosemu hdimage files.  The length of the header is kept in byte
 * 19 of the header as an unsigned long.  The header is guaranteed
 * to always be at least 128 bytes long, but never more than 512.
 *
 * BUGS: on my disk, the second logical partition of the extended partition
 *   is said to start 1 sector earlier than it actually does (and therefore
 *   512 bytes earlier).
 *
 * $Date: 1994/01/20 21:18:35 $
 * $Source: /home/src/dosemu0.50/periph/RCS/hdinfo.c,v $
 * $Revision: 1.3 $
 * $State: Exp $
 *
 * $Log: hdinfo.c,v $
 * Revision 1.3  1994/01/20  21:18:35  root
 * Indent.
 *
 * Revision 1.2  1993/11/30  21:27:27  root
 * Freeze before 0.49pl3
 *
 * Revision 1.1  1993/11/12  12:41:41  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  21:20:03  root
 * Initial revision
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <linux/genhd.h>

#define SECTOR_SIZE	512
#define EXT_MAGIC	5	/* sys_ind for an extended partition */

int fd = -1;
char *decfmt = "%sSector=%-6d   Offset=%-10d   Type=0x%02x%s\n";
char *hexfmt = "%sSector=0x%-6x   Offset=0x%-8x   Type=0x%02x%s\n";
char *fmtstring;

void
usage(void)
{
  fprintf(stderr, "usage: hdinfo [-h] <image file or disk device>\n");
}

void
print_part(struct partition *part, size_t offset, int sect_off, int ext)
{
  char *indent = ext ? "       [" : "";
  char *exdent = ext ? "]" : "";
  int i;

  if (part->sys_ind)
    printf(fmtstring, indent,
	   part->start_sect + sect_off,
	   (part->start_sect + sect_off) * SECTOR_SIZE + offset,
	   part->sys_ind, exdent);

  if (part->sys_ind == EXT_MAGIC && !ext) {
    char extblock[512];

    lseek(fd, part->start_sect * SECTOR_SIZE + offset, SEEK_SET);
    if (read(fd, extblock, 512) < 512) {
      perror("hdinfo: Could not read sector");
      exit(1);
    }

    for (i = 0; i < 4; i++) {
      print_part((struct partition *) (extblock + 0x1be + i * 16), offset,
		 part->start_sect, 1);
    }
  }
}

int
main(int argc, char **argv)
{
  int i, hdimage_off = 0;
  char *filename;
  char mbr[1024];
  struct partition *part;

  fmtstring = decfmt;

  if (argc == 3) {
    if (!strcmp(argv[1], "-h"))
      fmtstring = hexfmt;
    else {
      usage();
      exit(1);
    }
    filename = argv[2];
  }
  else if (argc == 2)
    filename = argv[1];
  else {
    usage();
    exit(1);
  }

  if ((fd = open(filename, O_RDONLY)) < 0) {
    perror("hdinfo: Cannot open file");
    exit(1);
  }

  if (read(fd, mbr, 1024) < 1024) {
    perror("hdinfo: Could not read MBR\n");
    exit(1);
  }

  if (!strncmp(mbr, "DOSEMU", 6))
    hdimage_off = *(u_long *) (mbr + 19);
  else
    hdimage_off = 0;

  printf("Partition info for %s %s\n=================================================\n",
	 filename, hdimage_off ? "(a DOSEMU 0.49 hdimage file)" : "");

  for (i = 0; i < 4; i++)
    print_part((struct partition *) (mbr + 0x1be + hdimage_off + i * 16),
	       hdimage_off, 0, 0);
}
