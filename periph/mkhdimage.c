/* mkhdimage.c, for the Linux DOS emulator
 *
 * cheesy program to create an hdimage header
 *
 * $Date: 1994/06/17 00:14:24 $
 * $Source: /usr/src/dosemu0.52/periph/RCS/mkhdimage.c,v $
 * $Revision: 2.2 $
 * $State: Exp $
 *
 * $Log: mkhdimage.c,v $
 * Revision 2.2  1994/06/17  00:14:24  root
 * Let's wrap it up and call it DOSEMU0.52.
 *
 * Revision 2.1  1994/06/12  23:17:32  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 2.1  1994/06/12  23:17:32  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.3  1994/03/13  01:08:52  root
 * Poor attempt to optimize.
 *
 * Revision 1.2  1994/01/20  21:18:35  root
 * Indent.
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
#include <stdio.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>

int sectors = 17, heads = 4, cylinders = 40, header_size = 128;

void
usage(void)
{
  fprintf(stderr, "mkhdimage [-h <heads>] [-s <sectors>] [-c|-t <cylinders>]\n");
}

void
main(int argc, char **argv)
{
  int c;
  int pos = 0;

  while ((c = getopt(argc, argv, "h:s:t:c:s:")) != EOF) {
    switch (c) {
    case 'h':
      heads = atoi(optarg);
      break;
    case 's':
      sectors = atoi(optarg);
      break;
    case 'c':			/* cylinders */
    case 't':			/* tracks */
      cylinders = atoi(optarg);
      break;
    default:
      fprintf(stderr, "Unknown option '%c'\n", c);
      usage();
      exit(1);
    }
  }

#define WRITE(fd, ptr, size) ({ int w_tmp=0; \
do { w_tmp=write(fd,ptr,size); } while ((w_tmp == -1) && (errno == EINTR)); \
if (w_tmp == -1) \
  fprintf(stderr, "WRITE ERROR: %d, *%s* in file %s, line %d\n", \
	  errno, strerror(errno), __FILE__, __LINE__); \
w_tmp; })

  pos += WRITE(STDOUT_FILENO, "DOSEMU", 7);
  pos += WRITE(STDOUT_FILENO, &heads, 4);
  pos += WRITE(STDOUT_FILENO, &sectors, 4);
  pos += WRITE(STDOUT_FILENO, &cylinders, 4);
  pos += WRITE(STDOUT_FILENO, &header_size, 4);

  {
    char tmp[256];

    bzero(tmp, 256);

    pos += WRITE(STDOUT_FILENO, bzero, header_size - pos);
    fprintf(stderr, "Pos now is %d\n", pos);
  }

}
