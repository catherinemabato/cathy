/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* dos emulator, Matthias Lautner
 * Extensions by Robert Sanders, 1992-93
 *
 * floppy disks, dos partitions or their images (files) (maximum 8 heads)
 */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#ifdef __linux__
  #include <linux/hdreg.h>
  #include <linux/fd.h>
  #include "kversion.h"
  #if (LX_KERNEL_VERSION < 2003099)
    #include <linux/fs.h>
  #else
    #if (GLIBC_VERSION_CODE >= 2000)
      #include <sys/mount.h>
    #else
      #ifndef BLKGETSIZE
			/* Note: _IO() being defined in <sys/ioctl.h> */
        #define BLKGETSIZE _IO(0x12,96) /* return device size */
      #endif
    #endif
  #endif
#endif
#include <sys/stat.h>
#include <sys/time.h>

#include "emu.h"
#include "disks.h"
#include "timers.h"
#include "utilities.h"
#include "priv.h"
#include "dosemu_select.h"
#include "int.h"
#include "fatfs.h"
#include "utilities.h"

#ifdef NEED_LLSEEK_PROTOTYPE
  /* well, if we don't have llseek prototype,
   * we most likely won't have __loff_t too, hence using long long
   */
  extern long long libless_llseek(int fd, long long offset, int origin);
  #define llseek libless_llseek
#endif

static int disks_initiated = 0;

#define FDISKS config.fdisks
#define HDISKS config.hdisks

inline void disk_close(void);

#define USE_FSYNC 1

#if 1
#  ifdef USE_FSYNC
#     define FLUSHDISK(dp) if (dp->removeable && !config.fastfloppy) fsync(dp->fdesc);
#  else
#     define FLUSHDISK(dp) if (dp->removeable && !config.fastfloppy) disk_close();
#  endif
#else
#define FLUSHDISK(dp) if (dp->removeable && !config.fastfloppy) \
  ioctl(dp->fdesc, FDFLUSH, 0)
#endif

/* NOTE: the "header" element in the structure above can (and will) be
 * negative. This facilitates treating partitions as disks (i.e. using
 * /dev/hda1 with a simulated partition table) by adjusting out the
 * simulated partition table offset...
 */

struct disk_fptr {
  void (*autosense) (struct disk *);
  void (*setup) (struct disk *);
};

static struct disk_fptr disk_fptrs[NUM_DTYPES] =
{
  {image_auto, image_setup},
  {hdisk_auto, hdisk_setup},
  {floppy_auto, floppy_setup},
  {partition_auto, partition_setup},
  {dir_auto, dir_setup}
};


static void dump_disk_blks(unsigned char *tb, int count, int ssiz)
{
  static unsigned char buf[80], cbuf[20];
  int a,i,j;
  unsigned char *p,*q;

  q=tb; a=0;
  while (count--) {
    for (i=0; i < (ssiz>>4); i++) {
      p=buf;
      p+=sprintf(p,"%04x: ",a);
      for (j=0; j<16; j++) {
        p+=sprintf(p,"%02x ",*q);
        cbuf[j] = (isprint(*q)? *q:'.');
        q++;
      }
      cbuf[16]=0; a+=16;
      d_printf("%s  %s\n",buf,cbuf);
    }
    d_printf("\n");
  }
}

/* read_sectors
 *
 * okay, here's the purpose of this: to handle reads orthogonally across
 * all disk types.  The tricky one is PARTITION, which looks like this:
 *
 *  |||..........,,,######################################
 *
 *   |   adds up to 1 sector (512 bytes) of Master Boot Record (MBR)
 *       THIS IS KEPT IN MEMORY, and is the file 
 *       /var/lib/dosemu/partition.<partition>.
 *
 *   .   is any number of sectors before the desired partition start address.
 *       THESE SECTORS ARE NOT EMULATED, BUT MERELY SKIPPED
 *
 *   ,   is some number of sectors "before" the partition.  These sectors
 *       occur after the partition start address.
 *       THESE SECTORS ARE NOT EMULATED, BUT MERELY SKIPPED
 *       I might not need to skip these, but it seems to work...
 *
 * read_sectors should be able to handle a read which crosses any number
 * of boundaries by memcpy()ing, skipping, and read()ing the appropriate
 * combination.
 */

int
read_sectors(struct disk *dp, char *buffer, long head, long sector,
	     long track, long count)
{
  loff_t  pos;
  long already = 0;
  long tmpread;

  /* XXX - header hack. dp->header can be negative for type PARTITION */
  pos = DISK_OFFSET(dp, head, sector, track) + dp->header;
  d_printf("DISK: %s: Trying to read %ld sectors at T/S/H %ld/%ld/%ld",
	   dp->dev_name,count,track,sector,head);
#ifdef __linux__
  d_printf("%+Ld at pos %Ld\n", dp->header, pos);
#else
  d_printf(" at pos %ld\n", pos);
#endif

  /* reads beginning before that actual disk/file */
  if (pos < 0 && count > 0) {
    int readsize = count * SECTOR_SIZE;
    int mbroff = -dp->header + pos;
    int mbrread = 0;

    if(!(dp->type == PARTITION || dp->type == DIR_TYPE)) {
      error("negative offset on non-partition disk type\n");
      return -DERR_NOTFOUND;
    }

    readsize = readsize > -pos ? -pos : readsize;

    /* 
     * this will end up pretending to read the requested data, but will
     * only read as much as can be read from the fake MBR in memory
     * and zero the rest
     */

    /* copy the MBR... */
    if(mbroff < dp->part_info.mbr_size) {
      mbrread = dp->part_info.mbr_size - mbroff;
      mbrread = mbrread > readsize ? readsize : mbrread;
      memcpy(buffer, dp->part_info.mbr + mbroff, mbrread);
      d_printf("read 0x%lx bytes from MBR, ofs = 0x%lx (0x%lx bytes left)\n",
        (unsigned long) mbrread, (unsigned long) mbroff, (unsigned long) (readsize - mbrread)
      );
    }

    /* ... and zero the rest */
    if(readsize > mbrread) {
      memset(buffer + mbrread, 0, readsize - mbrread);
      d_printf("emulated reading 0x%lx bytes, ofs = 0x%lx\n",
        (unsigned long) (readsize - mbrread), (unsigned long) (mbroff + mbrread)
      );
    }

    if(readsize == count * SECTOR_SIZE) {
      d_printf("   got entire read done from memory. off:%d, count:%d\n", mbroff, readsize);
      return readsize;
    }

    buffer += readsize;
    pos += readsize;
    already += readsize;
  }

  if(dp->type == DIR_TYPE && dp->fatfs) {
    /* this should not never happen */
    if(pos % SECTOR_SIZE || already % SECTOR_SIZE) {
      error("illegal read offset for %s\n", dp->dev_name);
      return -DERR_NOTFOUND;
    }
    tmpread = fatfs_read(dp->fatfs, buffer, pos / SECTOR_SIZE, count - already / SECTOR_SIZE);
    if(tmpread == -1) return -DERR_NOTFOUND;
    if(tmpread == -2) return -DERR_ECCERR;
    tmpread *= SECTOR_SIZE;
  }
  else {
#ifdef __linux__
    if(pos != llseek(dp->fdesc, pos, SEEK_SET)) {
#else
    if(pos != lseek(dp->fdesc, pos, SEEK_SET)) {
#endif
      error("Sector not found in read_sector, error = %s!\n", strerror(errno));
      return -DERR_NOTFOUND;
    }
    tmpread = RPT_SYSCALL(read(dp->fdesc, buffer, count * SECTOR_SIZE - already));
  }

  if(tmpread != -1) {
    if(d.disk > 8) dump_disk_blks(buffer, count - already / SECTOR_SIZE, SECTOR_SIZE);
    return tmpread + already;
  }
  else {
    return -DERR_ECCERR;
  }
}

int
write_sectors(struct disk *dp, char *buffer, long head, long sector,
	      long track, long count)
{
  loff_t  pos;
  long tmpwrite, already = 0;

  if (dp->rdonly) {
    d_printf("ERROR: write to readonly disk %s\n", dp->dev_name);
#if 0
    LO(ax) = 0;			/* no sectors written */
    CARRY;			/* error */
    HI(ax) = 0xcc;		/* write fault */
#endif
    return -DERR_WRITEFLT;
  }

  /* XXX - header hack */
  pos = DISK_OFFSET(dp, head, sector, track) + dp->header;
  d_printf("DISK: %s: Trying to write %ld sectors T/S/H %ld/%ld/%ld",
	   dp->dev_name,count,track,sector,head);
#ifdef __linux__
  d_printf(" at pos %Ld\n", pos);
#else
  d_printf(" at pos %ld\n", pos);
#endif

  /*
   * writes outside the partition (before the actual disk/file) are ignored
   */
  if (pos < 0 && count > 0) {
    long writesize = count * SECTOR_SIZE;

    if(!(dp->type == PARTITION || dp->type == DIR_TYPE)) {
      error("negative offset on non-partition disk type\n");
      return -DERR_NOTFOUND;
    }

    writesize = writesize > -pos ? -pos : writesize;

    d_printf("emulated writing 0x%lx bytes, ofs = 0x%lx (0x%lx left)\n",
      writesize, (long) (pos - dp->header), count * SECTOR_SIZE - writesize
    );

    if(writesize == count * SECTOR_SIZE) return writesize;

    buffer += writesize;
    pos += writesize;
    already += writesize;
  }

  if(dp->type == DIR_TYPE && dp->fatfs) {
    /* this should not never happen */
    if(pos % SECTOR_SIZE || already % SECTOR_SIZE) {
      error("illegal write offset for %s\n", dp->dev_name);
      return -DERR_NOTFOUND;
    }
    tmpwrite = fatfs_write(dp->fatfs, buffer, pos / SECTOR_SIZE, count - already / SECTOR_SIZE);
    if(tmpwrite == -1) return -DERR_NOTFOUND;
    if(tmpwrite == -1) return -DERR_WRITEFLT;
    tmpwrite *= SECTOR_SIZE;
  }
  else {
#ifdef __linux__
    if(pos != llseek(dp->fdesc, pos, SEEK_SET)) {
#else
    if(pos != lseek(dp->fdesc, pos, SEEK_SET)) {
#endif
      error("Sector not found in write_sector!\n");
      return -DERR_NOTFOUND;
    }
    tmpwrite = RPT_SYSCALL(write(dp->fdesc, buffer, count * SECTOR_SIZE - already));
  }

  /* this should make floppies a little safer...I would as soon use the
   * O_SYNC flag, but we don't have it.  fsync() should be in the kernel
   * soon, I think, thanks to Stephen Tweedie.
   *    Also look into detecting floppy change so we can close/reopen
   *    it.  perhaps the FDFLUSH ioctl()?
   */

  FLUSHDISK(dp);

  return tmpwrite + already;
}

void
image_auto(struct disk *dp)
{
  char header[HEADER_SIZE];

  d_printf("IMAGE auto-sensing\n");

  if (dp->fdesc == -1) {
    warn("WARNING: image filedesc not open\n");
    dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
    /* The next line should only be done in case the open succeeds, 
       but that should be the normal case, and allows somewhat better
       code for the if (how sick can you get, since the open is going to
       take a lot more time anyways :-) )
    */
    dp->rdonly = dp->wantrdonly;
    dp->fdesc = open(dp->dev_name, dp->wantrdonly ? O_RDONLY : O_RDWR, 0);
    if (dp->fdesc == -1) {
      /* We should check whether errno is EROFS, but if not the next open will
         fail again and the following lseek will throw us out of dos. So we win
         a very tiny amount of time in case it works. Also, if for some reason
         this does work (should be impossible), we can at least try to 
         continue. (again how sick can you get :-) )
       */
      dp->fdesc = open(dp->dev_name, O_RDONLY, 0);
      dp->rdonly = 1;
    }
  }

  lseek(dp->fdesc, 0, SEEK_SET);/* Use lseek here, as nobody want
				   > 2^^31 bytes for the image file */
  if (RPT_SYSCALL(read(dp->fdesc, header, HEADER_SIZE)) != HEADER_SIZE) {
    error("could not read full header in image_init\n");
    leavedos(19);
  }

  if (strncmp(header, IMAGE_MAGIC, IMAGE_MAGIC_SIZE)
		&& (*((long *)header) != DEXE_MAGIC) ) {
    error("IMAGE %s header lacks magic string - cannot autosense!\n",
	  dp->dev_name);
    leavedos(20);
  }

  dp->heads = *(long *) &header[7];
  dp->sectors = *(long *) &header[11];
  dp->tracks = *(long *) &header[15];
  dp->header = *(long *) &header[19];

  d_printf("IMAGE auto_info disk %s; h=%d, s=%d, t=%d, off=%ld\n",
	   dp->dev_name, dp->heads, dp->sectors, dp->tracks,
	   (long) dp->header);
}

void
hdisk_auto(struct disk *dp)
{
#ifdef __linux__
  struct hd_geometry geo;

  if (ioctl(dp->fdesc, HDIO_GETGEO, &geo) < 0) {
    error("can't get GEO of %s: %s\n", dp->dev_name,
	  strerror(errno));
    leavedos(21);
  }
  else {
    dp->sectors = geo.sectors;
    dp->heads = geo.heads;
    dp->tracks = geo.cylinders;
    dp->start = geo.start;
    d_printf("HDISK auto_info disk %s; h=%d, s=%d, t=%d, start=%ld\n",
	     dp->dev_name, dp->heads, dp->sectors, dp->tracks, dp->start);
  }
#endif
}

void dir_auto(struct disk *dp)
{
  /*
   * We emulate an entire disk with 1 partition starting at t/h/s 0/1/1.
   * You are free to change the geometry (e.g. to change the partition size).
   */
  dp->sectors = 63;
  dp->heads = 255;
  dp->tracks = 50;

  dp->start = dp->sectors;

  d_printf(
    "DIR auto_info disk %s; h=%d, s=%d, t=%d, start=%ld\n",
    dp->dev_name, dp->heads, dp->sectors, dp->tracks, dp->start
  );
}

void dir_setup(struct disk *dp)
{
  unsigned char *mbr;
  struct partition *pi = &dp->part_info;
  int i = strlen(dp->dev_name);

  while(--i >= 0) if(dp->dev_name[i] == '/') dp->dev_name[i] = 0; else break;

  d_printf("partition setup for directory %s\n", dp->dev_name);

  pi->beg_head = 1;
  pi->beg_sec = 1;
  pi->beg_cyl = 0;
  pi->end_head = dp->heads - 1;
  pi->end_sec = dp->sectors;
  pi->end_cyl = dp->tracks - 1;
  pi->pre_secs = dp->sectors;
  pi->num_secs = (dp->tracks * dp->heads - 1) * dp->sectors;

  dp->header = -(SECTOR_SIZE * (loff_t) (pi->pre_secs));

  pi->mbr_size = SECTOR_SIZE;
  pi->mbr = malloc(pi->mbr_size);
  mbr = pi->mbr;

  memset(mbr, 0, SECTOR_SIZE);
  /*
   * mov ax,0fffeh
   * int 0e6h
   */
  mbr[0x00] = 0xb8;
  mbr[0x01] = 0xfe;
  mbr[0x02] = 0xff;
  mbr[0x03] = 0xcd;
  mbr[0x04] = 0xe6;
  mbr[PART_INFO_START + 0x00] = PART_BOOT;
  mbr[PART_INFO_START + 0x01] = pi->beg_head;
  mbr[PART_INFO_START + 0x02] = pi->beg_sec + ((pi->beg_cyl & 0x300) >> 2);
  mbr[PART_INFO_START + 0x03] = pi->beg_cyl;
  mbr[PART_INFO_START + 0x04] = pi->num_secs < 1 << 16 ? 0x04 : 0x06;
  mbr[PART_INFO_START + 0x05] = pi->end_head;
  mbr[PART_INFO_START + 0x06] = pi->end_sec + ((pi->end_cyl & 0x300) >> 2);
  mbr[PART_INFO_START + 0x07] = pi->end_cyl;
  *(unsigned *) (mbr + PART_INFO_START + 0x08) = pi->pre_secs;
  *(unsigned *) (mbr + PART_INFO_START + 0x0c) = pi->num_secs;
  mbr[SECTOR_SIZE - 2] = 0x55;
  mbr[SECTOR_SIZE - 1] = 0xaa;

  d_printf("partition table entry for device %s is:\n", dp->dev_name);
  d_printf(
    "beg head %d, sec %d, cyl %d = end head %d, sec %d, cyl %d\n",
    pi->beg_head, pi->beg_sec, pi->beg_cyl,
    pi->end_head, pi->end_sec, pi->end_cyl
  );
  d_printf(
    "pre_secs %ld, num_secs %ld = %lx, -dp->header %ld = 0x%lx\n",
    pi->pre_secs, pi->num_secs, pi->num_secs,
    (long) -dp->header, (unsigned long) -dp->header
  );

  dp->fatfs = NULL;
  fatfs_init(dp);

  if(dp->fatfs && !redir_state) set_int21_revectored(redir_state = 1);
}

/* XXX - relies upon a file of SECTOR_SIZE in PARTITION_PATH that which
 *       contains the MBR (first sector) of the drive (i.e. /dev/hda).
 *       only works for /dev/hda1 right now, and only allows one
 *       partition, which must begin at head 1, sec 1, cyl 0 (i.e. linear
 *       sector 2, the second one).
 *       Also, it eats memory proportional to the number of sectors before
 *       the start of the partition.
 */

void
partition_setup(struct disk *dp)
{
  PRIV_SAVE_AREA
  int part_fd, i;
  unsigned char tmp_mbr[SECTOR_SIZE];
  char *hd_name;
  void set_part_ent(struct disk *, char *);

#define PART_BYTE(p,b)  *((unsigned char *)tmp_mbr + PART_INFO_START + \
			  (PART_INFO_LEN * (p-1)) + b)
#define PART_INT(p,b)  *((unsigned int *)(tmp_mbr + PART_INFO_START + \
			  (PART_INFO_LEN * (p-1)) + b))
#define PNUM dp->part_info.number

  /* PNUM is 1-based */

  d_printf("PARTITION SETUP for %s\n", dp->dev_name);

#ifdef __linux__
  hd_name = strdup(dp->dev_name);
  hd_name[8] = '\0';			/* i.e.  /dev/hda6 -> /dev/hda */
#endif

  enter_priv_on();
  part_fd = SILENT_DOS_SYSCALL(open(hd_name, O_RDONLY));
  leave_priv_setting();
  if (part_fd == -1) {
    if (dp->removeable) return;
    error("opening device %s to read MBR for PARTITION %s\n",
	  hd_name, dp->dev_name);
    leavedos(22);
  }
  RPT_SYSCALL(read(part_fd, tmp_mbr, SECTOR_SIZE));
  close(part_fd);
  d_printf("Using MBR from %s for PARTITION %s (part#=%d).\n",
           hd_name, dp->dev_name, PNUM);
  free(hd_name);

  /* check for logical partition, if so simulate as primary part#1 */
  if (PNUM > 4 ) {
    d_printf("LOGICAL PARTITION - will be simulated as physical partition 1\n");
    PNUM = 1;
    set_part_ent(dp, tmp_mbr);
  }
  dp->part_info.beg_head = PART_BYTE(PNUM, 1);
  dp->part_info.beg_sec = PART_BYTE(PNUM, 2) & ~0xc0;
  dp->part_info.beg_cyl = PART_BYTE(PNUM, 3) | ((PART_BYTE(PNUM, 2) << 2) & ~0xff);

  dp->part_info.end_head = PART_BYTE(PNUM, 5);
  dp->part_info.end_sec = PART_BYTE(PNUM, 6) & ~0xc0;
  dp->part_info.end_cyl = PART_BYTE(PNUM, 7) | ((PART_BYTE(PNUM, 6) << 2) & ~0xff);

  dp->part_info.pre_secs = PART_INT(PNUM, 8);
  dp->part_info.num_secs = PART_INT(PNUM, 0xc);

  /* HelpPC is wrong about the location of num_secs; it says 0xb! */

  /* head should be zero-based, but isn't in the partition table.
   * sector should be zero-based, and is.
   */
#if 0
  dp->header = -(DISK_OFFSET(dp, dp->part_info.beg_head - 1,
			     dp->part_info.beg_sec,
			     dp->part_info.beg_cyl) +
		 (SECTOR_SIZE * (dp->part_info.pre_secs - 1)));
#else
  dp->header = -(SECTOR_SIZE * (loff_t) (dp->part_info.pre_secs));
#endif

  dp->part_info.mbr_size = SECTOR_SIZE;
  dp->part_info.mbr = malloc(dp->part_info.mbr_size);
  memcpy(dp->part_info.mbr, tmp_mbr, dp->part_info.mbr_size);

  /* want this to be the first & only partition on the virtual disk */
  if (PNUM != 1)
    memcpy(dp->part_info.mbr + PART_INFO_START,
	 dp->part_info.mbr + PART_INFO_START + (PART_INFO_LEN * (PNUM - 1)),
	   PART_INFO_LEN);

  d_printf("beg head %d, sec %d, cyl %d = end head %d, sec %d, cyl %d\n",
	   dp->part_info.beg_head, dp->part_info.beg_sec,
	   dp->part_info.beg_cyl,
	   dp->part_info.end_head, dp->part_info.end_sec,
	   dp->part_info.end_cyl);
  d_printf("pre_secs %ld, num_secs %ld = %lx, -dp->header %ld = 0x%lx\n",
	   dp->part_info.pre_secs, dp->part_info.num_secs,
	   dp->part_info.num_secs,
	   (long) -dp->header, (unsigned long) -dp->header);

  /* XXX - make sure there is only 1 partition by zero'ing out others */
  for (i = 1; i <= 3; i++) {
    d_printf("DESTROYING any traces of partition %d\n", i + 1);
    memset(dp->part_info.mbr + PART_INFO_START + (i * PART_INFO_LEN),
	   0, PART_INFO_LEN);
  }

}

/* XXX - this function constructs a primary partition table entry for the device
 *       dp->dev_name which should be a logical DOS partition. This is done by
 *       knowing the preceding sectors & length in sectors, and the geometry
 *       of the drive. The physical h/s/c start and end are calculated and
 *       put in the dp->part_info.number'th entry in the part table.
 */

void
set_part_ent(struct disk *dp, char *tmp_mbr)
{
  long	length;		/* partition length in sectors		*/
  long	end;		/* last sector number offset		*/
  char	*p;		/* ptr to part table entry to create	*/

#ifdef __linux__
  if (ioctl(dp->fdesc, BLKGETSIZE, &length)) {
    error("calling ioctl BLKGETSIZE for PARTITION %s\n", dp->dev_name);
    leavedos(22);
  }
#endif
#define SECPERCYL	(dp->heads * dp->sectors)
#define CYL(s)		((s)/SECPERCYL)			/* 0-based */
#define HEAD(s)		(((s)%SECPERCYL)/dp->sectors)	/* 0-based */
#define SECT(s)		(((s)%dp->sectors)+1)		/* 1-based */

  d_printf("SET_PART_ENT: making part table entry for device %s,\n",
	dp->dev_name);
  d_printf("Calculated physical start: head=%4ld sect=%4ld cyl=%4ld,\n",
	HEAD(dp->start), SECT(dp->start), CYL(dp->start));
  end = dp->start+length-1;
  d_printf("Calculated physical end:   head=%4ld sect=%4ld cyl=%4ld.\n",
	HEAD(end), SECT(end), CYL(end));

  /* get address of where to put new part table entry */
  p = tmp_mbr + PART_INFO_START + (PART_INFO_LEN * (dp->part_info.number-1)); 

  p[0] = PART_BOOT;						/* bootable  */
  p[1] = HEAD(dp->start);					/* beg head  */
  p[2] = SECT(dp->start) | ((CYL(dp->start) >> 2) & 0xC0);	/* beg sect  */
  p[3] = CYL(dp->start) & 0xFF;					/* beg cyl   */
  p[4] = (length < 32*1024*1024/SECTOR_SIZE)? 0x04 : 0x06;	/* dos sysid */
  p[5] = HEAD(end);						/* end head  */
  p[6] = SECT(end) | ((CYL(end) >> 2) & 0xC0);			/* end sect  */
  p[7] = CYL(end) & 0xFF;					/* end cyl   */
  *((long *)(p+8)) = dp->start;					/* pre sects */
  *((long *)(p+12)) = length;					/* len sects */
}

void
d_nullf(struct disk *dp)
{
  d_printf("NULLF for %s\n", dp->dev_name);
}


unsigned char ATAPI_buf0[512] = { 0 };

inline void
disk_close(void)
{
  struct disk *dp;

  if (!disks_initiated) return;  /* just to be safe */
  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if (dp->removeable && dp->fdesc >= 0) {
      d_printf("DISK: %s: Closing a disk\n",dp->dev_name);
      (void) close(dp->fdesc);
      dp->fdesc = -1;
    }
  }
}


#ifdef __linux__
void
disk_open(struct disk *dp)
{
  PRIV_SAVE_AREA
  struct floppy_struct fl;

  if (dp == NULL || dp->fdesc >= 0)
    return;
    
  enter_priv_on();
  dp->fdesc = SILENT_DOS_SYSCALL(open(dp->type == DIR_TYPE ? "/dev/null" : dp->dev_name, dp->wantrdonly ? O_RDONLY : O_RDWR, 0));
  leave_priv_setting();

  /* FIXME:
   * Why the hell was the below handling restricted to non-removeable disks?
   * This made opening writeprotected floppies impossible :-(
   *                                                  -- Hans, 990112
   */
  if ( /*!dp->removeable &&*/ (dp->fdesc < 0)) {
    if (errno == EROFS || errno == ENODEV) {
      enter_priv_on();
      dp->fdesc = DOS_SYSCALL(open(dp->dev_name, O_RDONLY, 0));
      leave_priv_setting();
      if (dp->fdesc < 0) {
        d_printf("ERROR: (disk) can't open %s for read nor write: %s (you should never see this message)\n", dp->dev_name, strerror(errno));
        /* In case we DO get more clever, we want to share that code */
        goto fail;
      } else {
        dp->rdonly = 1;
        d_printf("(disk) can't open %s for read/write. Readonly used.\n", dp->dev_name);
      }
    } else {
      d_printf("ERROR: (disk) can't open %s: %s\n", dp->dev_name, strerror(errno));
    fail:
#if 0
      /* We really should be more clever here */
      fatalerr = 5;
#endif
      return;
    }
  }
  else dp->rdonly = dp->wantrdonly;

{
#if 1 
  /* NOTE: Starting with linux 1.3.100 the floppy driver has changed
   * so that it no longer returns from the following ioctl without
   * getting interrupted by SIGALARM (-EINTR). Also a retry does not help,
   * because this one gets interrupt again (and again).
   * To overcome this problem we temporary switch off the timer
   * during the ioctl. (well, not what we really like)
   * ( 19 May 1996, Hans Lermen ) */
  int res=0;
  if (dp->default_cmos == ATAPI_FLOPPY) {
	unsigned long tns;
	if (ATAPI_buf0[0] || (read_sectors(dp, ATAPI_buf0, 0, 0, 0, 1) > 0)) {
	      fl.sect = *((unsigned char *)&ATAPI_buf0[0x18]);
	      fl.head = *((unsigned char *)&ATAPI_buf0[0x1a]);
	      tns = *((unsigned short *)&ATAPI_buf0[0x13]);
	      if (tns==0) tns = *((unsigned long *)&ATAPI_buf0[0x20]);
	      fl.track = tns/(fl.sect*fl.head);
	}
	else {	/* no disk available */
	      dp->sectors = 0;
	      dp->heads = 0;
	      dp->tracks = 0;
	      return;
	}
  }
  else {
  sigalarm_onoff(0);
  res = ioctl(dp->fdesc, FDGETPRM, &fl);
  sigalarm_onoff(1);
  }
  if (res == -1) {
#else
  if (ioctl(dp->fdesc, FDGETPRM, &fl) == -1) {
#endif
    if ((dp->fdesc == -1) || (errno == ENODEV)) {	/* no disk available */
      dp->sectors = 0;
      dp->heads = 0;
      dp->tracks = 0;
      return;
    }
    error("can't get floppy parameter of %s (%s)\n", dp->dev_name, sys_errlist[errno]);
    fatalerr = 5;
    return;
  }
}
  d_printf("FLOPPY %s h=%d, s=%d, t=%d\n", dp->dev_name, fl.head, fl.sect, fl.track);
  dp->sectors = fl.sect;
  dp->heads = fl.head;
  dp->tracks = fl.track;
  if (dp->default_cmos != ATAPI_FLOPPY)
    DOS_SYSCALL(ioctl(dp->fdesc, FDMSGOFF, 0));
}
#endif

void
disk_close_all(void)
{
  struct disk *dp;
  
  if (!disks_initiated) return;  /* prevent idiocy */
  if (config.bootdisk && bootdisk.fdesc >= 0) {
    d_printf("Boot disk Closing %x\n", bootdisk.fdesc);
    (void) close(bootdisk.fdesc);
    bootdisk.fdesc = -1;
    d_printf("BOOTDISK Closing\n");
  }
  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    ATAPI_buf0[0] = 0;
    if (dp->fdesc >= 0) {
      d_printf("Floppy disk Closing %x\n", dp->fdesc);
      (void) close(dp->fdesc);
      dp->fdesc = -1;
    }
  }
  for (dp = hdisktab; dp < &hdisktab[HDISKS]; dp++) {
    if(dp->type == DIR_TYPE) fatfs_done(dp);
    if (dp->fdesc >= 0) {
      d_printf("Hard disk Closing %x\n", dp->fdesc);
      (void) close(dp->fdesc);
      dp->fdesc = -1;
    }
  }
  disks_initiated = 0;
}

/*
 * DANG_BEGIN_FUNCTION disk_init
 *
 * description:
 *  Test by opening all floppies/hardrives configured.
 *
 * DANG_END_FUNCTION
 */
void
disk_init(void)
{
  PRIV_SAVE_AREA
  struct disk *dp=NULL;
  struct stat stbuf;

#ifdef SILLY_GET_GEOMETRY
  int s;
  char buf[512], label[12];

#endif

  disks_initiated = 1;  /* disk_init has been called */
  if (config.bootdisk) {
    enter_priv_on();
    bootdisk.fdesc = open(bootdisk.dev_name,
			  bootdisk.rdonly ? O_RDONLY : O_RDWR, 0);
    leave_priv_setting();
    if (bootdisk.fdesc < 0) {
      if (errno == EROFS) {
        enter_priv_on();
        bootdisk.fdesc = open(bootdisk.dev_name, O_RDONLY, 0);
        leave_priv_setting();
        if (bootdisk.fdesc < 0) {
          error("can't open bootdisk %s for read nor write: %s (you should never see this message)\n", dp->dev_name, strerror(errno));
          leavedos(23);
        } else {
          bootdisk.rdonly = 1;
          d_printf("(disk) can't open bootdisk %s for read/write. Readonly did work though\n", bootdisk.dev_name);
        }
      } else {
        error("can't open bootdisk %s: %s\n", dp->dev_name, strerror(errno));
        leavedos(23);
      }
    }
    else bootdisk.rdonly = bootdisk.wantrdonly;
    bootdisk.removeable = 0;
  }

  /*
   * Open floppy disks
   */
  ATAPI_buf0[0] = 0;
  for (dp = disktab; dp < &disktab[FDISKS]; dp++) {
    if (stat(dp->dev_name, &stbuf) < 0) {
      error("can't stat %s\n", dp->dev_name);
      leavedos(24);
    }
    if (S_ISBLK(stbuf.st_mode))
      d_printf("ISBLK ");
    if (S_ISCHR(stbuf.st_mode))
      d_printf("ISCHR ");
    d_printf("dev %s: %#x\n", dp->dev_name, (unsigned) stbuf.st_rdev);
#ifdef __linux__
    if (S_ISBLK(stbuf.st_mode) && 
    (((stbuf.st_rdev & 0xff00)==0x200) || (dp->default_cmos==ATAPI_FLOPPY))
    ) {
      d_printf("DISK %s removeable\n", dp->dev_name);
      dp->removeable = 1;
      dp->fdesc = -1;
      continue;
    }
#endif
    enter_priv_on();
    dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
    leave_priv_setting();
    if (dp->fdesc < 0) {
      if (errno == EROFS || errno == EACCES) {
        enter_priv_on();
        dp->fdesc = open(dp->dev_name, O_RDONLY, 0);
        leave_priv_setting();
        if (dp->fdesc < 0) {
          error("can't open %s for read nor write: %s (you should never see this message)\n", dp->dev_name, strerror(errno));
          leavedos(25);
        } else {
          dp->rdonly = 1;
          d_printf("(disk) can't open %s for read/write. Readonly did work though\n", dp->dev_name);
        }
      } else {
        error("can't open %s: %s\n", dp->dev_name, strerror(errno));
        /* 'leavedos' here hangs the machine */
        leavedos(25);
      }
    }
    else dp->rdonly = dp->wantrdonly;
  }

  /*
   * Open hard disks
   */
  for (dp = hdisktab; dp < &hdisktab[HDISKS]; dp++) {
    if(dp->type == IMAGE)  {
	if (dp->dexeflags & DISK_DEXE_RDWR) {
	  d_printf("IMAGE: dexe, RDWR access allowed for %s\n",dp->dev_name);
	  enter_priv_on();
	}
	else {
	  d_printf("IMAGE: Using user permissions\n");
	  enter_priv_off();
	}
    }
    else enter_priv_on();
    dp->fdesc = open(dp->type == DIR_TYPE ? "/dev/null" : dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
    leave_priv_setting();
    if (dp->fdesc < 0) {
      if (errno == EROFS || errno == EACCES) {
        if (dp->type == IMAGE) enter_priv_off(); else enter_priv_on();
        dp->fdesc = open(dp->dev_name, O_RDONLY, 0);
        leave_priv_setting();
        if (dp->fdesc < 0) {
          error("can't open %s for read nor write: %s (you should never see this message)\n", dp->dev_name, strerror(errno));
          leavedos(26);
        } else {
          dp->rdonly = 1;
          d_printf("(disk) can't open %s for read/write. Readonly did work though\n", dp->dev_name);
        }
      } else {
        error("can't open %s: #%d - %s\n", dp->dev_name, errno, strerror(errno));
        leavedos(26);
      }
    }
    else dp->rdonly = dp->wantrdonly;
    dp->removeable = 0;

    /* HACK: if unspecified geometry (-1) then try to get it from kernel.
       May only work on WD compatible disks (MFM/RLL/ESDI/IDE). */
    if (dp->sectors == -1)
      disk_fptrs[dp->type].autosense(dp);

    /* do all the necesary dirtiness to get this disk working
     * (mostly for the partition type)
     */
    disk_fptrs[dp->type].setup(dp);

    /* this really doesn't make sense...where the disk geometry
     * is in reality given for the actual disk (i.e. /dev/hda)
     * NOT the partition (i.e. /dev/hda1), the code below returns
     * the values for the partition.
     *
     * don't use this code...it's stupid.
     */
#ifdef SILLY_GET_GEOMETRY
    if (RPT_SYSCALL(read(dp->fdesc, buf, 512)) != 512) {
      error("can't read disk info of %s\n", dp->dev_name);
      leavedos(27);
    }

    dp->sectors = *(us *) & buf[0x18];	/* sectors per track */
    dp->heads = *(us *) & buf[0x1a];	/* heads */

    /* for partitions <= 32MB, the number of sectors is at offset 19.
     * since DOS 4.0, larger partitions have the # of sectors as a long
     * at offset 32, and the number at 19 is set to 0
     */
    s = *(us *) & buf[19];
    if (!s) {
      s = *(unsigned long *) &buf[32];
      d_printf("DISK: zero # secs, so DOS 4 # secs = %d\n", s);
    }
    s += *(us *) & buf[28];	/* + hidden sectors */

    dp->tracks = s / (dp->sectors * dp->heads);

    d_printf("DISK read_info disk %s; h=%d, s=%d, t=%d, #=%d, hid=%d\n",
	     dp->dev_name, dp->heads, dp->sectors, dp->tracks,
	     s, *(us *) & buf[28]);

    /* print serial # and label (DOS 4+ only) */
    memcpy(label, &buf[0x2b], 11);
    label[11] = 0;
    g_printf("VOLUME serial #: 0x%08x, LABEL: %s\n",
	     *(unsigned long *) &buf[39], label);

    if (s % (dp->sectors * dp->heads) != 0) {
      error("incorrect track number of %s\n", dp->dev_name);
      /* leavedos(28); */
    }
#endif
  }
}

int
checkdp(struct disk *disk)
{
  if (disk == NULL) {
    d_printf("DISK: null dp\n");
    return 1;
  }
  else if (disk->fdesc == -1) {
    d_printf("DISK: closed disk\n");
    return 1;
  }
  else
    return 0;
}

void
int13(u_char i)
{
  unsigned int disk, head, sect, track, number;
  int res;
  loff_t  pos;
  char *buffer;
  struct disk *dp;
  int checkdp_val;

  disk = LO(dx);
  if (disk < FDISKS) {
    if (!disk && use_bootdisk)
      dp = &bootdisk;
    else
      dp = &disktab[disk];
    switch (HI(ax)) {
      #define DISKETTE_MOTOR_TIMEOUT (*((unsigned char *)0x440))
      /* NOTE: we don't need this counter, but older games seem to rely
       * on it. We count it down in INT08 (bios.S) --SW, --Hans
       */
      case 0: case 2: case 3: case 5: case 10: case 11:
        DISKETTE_MOTOR_TIMEOUT = 3*18;  /* set timout to 3 seconds */
        break;
    }
  }
  else if (disk >= 0x80 && disk < 0x80 + HDISKS)
    dp = &hdisktab[disk - 0x80];
  else
    dp = NULL;

  /* this is a bad hack to ensure that the cached blocks aren't.
   * Linux only checks disk change on open()
   */

  switch (HI(ax)) {
  case 0:			/* init */
    d_printf("DISK init %d\n", disk);
    HI(ax) = DERR_NOERR;
    NOCARRY;
    break;

  case 1:			/* read error code into AL */
    LO(ax) = DERR_NOERR;
    NOCARRY;
    d_printf("DISK error code\n");
    break;

  case 2:			/* read */
    FLUSHDISK(dp);
    disk_open(dp);
    checkdp_val = checkdp(dp);
    if (!checkdp_val && dp->diskcyl4096 && dp->heads <= 64)
      head = HI(dx) & 0x3f;
    else
      head = HI(dx);
    sect = (REG(ecx) & 0x3f) - 1;
    track = (HI(cx)) |
      ((REG(ecx) & 0xc0) << 2);
    if (!checkdp_val && dp->diskcyl4096 && dp->heads <= 64 && (HI(dx) & 0xc0))
      track |= (HI(dx) & 0xc0) << 4;
    buffer = SEG_ADR((char *), es, bx);
    number = LO(ax);

    if (checkdp_val || head >= dp->heads ||
	sect >= dp->sectors || track >= dp->tracks) {
      d_printf("Sector not found 1!\n");
      d_printf("DISK %d read [h:%d,s:%d,t:%d](%d)->%p\n",
	       disk, head, sect, track, number, (void *) buffer);
      if (dp) {
	  d_printf("DISK dev %s GEOM %d heads %d sects %d trk\n",
		   dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  d_printf("DISK %x undefined.\n", disk);
      }

      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      show_regs(__FILE__, __LINE__);
      break;
    }

    res = read_sectors(dp, buffer, head, sect, track, number);

    if (res < 0) {
      HI(ax) = -res;
      CARRY;
      break;
    }
    else if (res & 511) {	/* must read multiple of 512 bytes */
      error("sector_corrupt 1, return = %d!\n", res);
      HI(ax) = DERR_BADSEC;	/* sector corrrupt */
      CARRY;
      break;
    }

    LWORD(eax) = res >> 9;
    REG(eflags) &= ~CF;
    R_printf("DISK read @%d/%d/%d (%d) -> %p OK.\n",
	     head, track, sect, res >> 9, (void *)buffer);
    break;

  case 3:			/* write */
    FLUSHDISK(dp);
    disk_open(dp);
    checkdp_val = checkdp(dp);
    if (!checkdp_val && dp->diskcyl4096 && dp->heads <= 64)
      head = HI(dx) & 0x3f;
    else
      head = HI(dx);
    sect = (REG(ecx) & 0x3f) - 1;
    track = (HI(cx)) |
      ((REG(ecx) & 0xc0) << 2);
    if (!checkdp_val && dp->diskcyl4096 && dp->heads <= 64 && (HI(dx) & 0xc0))
      track |= (HI(dx) & 0xc0) << 4;
    buffer = SEG_ADR((char *), es, bx);
    number = LO(ax);
    W_printf("DISK write [h:%d,s:%d,t:%d](%d)->%p\n",
	     head, sect, track, number, (void *) buffer);

    if (checkdp_val || head >= dp->heads ||
	sect >= dp->sectors || track >= dp->tracks) {
      error("Sector not found 3!\n");
      show_regs(__FILE__, __LINE__);
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      break;
    }

    if (dp->rdonly) {
      W_printf("write protect!\n");
      show_regs(__FILE__, __LINE__);
      if (dp->removeable)
	HI(ax) = DERR_WP;
      else
	HI(ax) = DERR_WRITEFLT;
      REG(eflags) |= CF;
      break;
    }

    if (dp->rdonly)
      W_printf("CONTINUED!!!!!\n");
    res = write_sectors(dp, buffer, head, sect, track, number);

    if (res < 0) {
      W_printf("DISK write error: %d\n", -res);
      HI(ax) = -res;
      CARRY;
      break;
    }
    else if (res & 511) {	/* must write multiple of 512 bytes */
      error("Write sector corrupt 2 (wrong size)!\n");
      HI(ax) = DERR_BADSEC;
      CARRY;
      break;
    }

    LWORD(eax) = res >> 9;
    REG(eflags) &= ~CF;
    W_printf("DISK write @%d/%d/%d (%d) OK.\n",
	     head, track, sect, res >> 9);
    break;

  case 4:			/* test */
    FLUSHDISK(dp);
    disk_open(dp);
    head = HI(dx);
    sect = (REG(ecx) & 0x3f) - 1;
    track = (HI(cx)) |
      ((REG(ecx) & 0xc0) << 2);
    number = LO(ax);
    d_printf("DISK %d test [h:%d,s:%d,t:%d](%d)\n",
	     disk, head, sect, track, number);

    if (checkdp(dp) || head >= dp->heads ||
	sect >= dp->sectors || track >= dp->tracks) {
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      error("test: sector not found 5\n");
      dbug_printf("hds: %d, sec: %d, tks: %d\n",
		  dp->heads, dp->sectors, dp->tracks);
      break;
    }
#ifdef __linux__
    pos = (long long ) ((track * dp->heads + head) * dp->sectors + sect) << 9;
#else
    pos = (long) ((track * dp->heads + head) * dp->sectors + sect) << 9;
#endif
    /* XXX - header hack */
    pos += dp->header;

#ifdef __linux__
    if (pos != llseek(dp->fdesc, pos, 0)) {
#else
    if (pos != lseek(dp->fdesc, pos, 0)) {
#endif
      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      error("test: sector not found 6\n");
      break;
    }
#if 0
    res = lseek(dp->fdesc, number << 9, 0);
    if (res & 0x1ff) {		/* must read multiple of 512 bytes  and res != -1 */
      HI(ax) = DERR_BADSEC;
      REG(eflags) |= CF;
      error("test: sector corrupt 3\n");
      break;
    }
    LWORD(eax) = res >> 9;
#endif
    REG(eflags) &= ~CF;
    break;

  case 8:			/* get disk drive parameters */
    d_printf("disk get parameters %#x\n", disk);

    if (dp != NULL) {
      /* get CMOS type */
      switch (dp->sectors) {
      case 9:
	LO(bx) = THREE_INCH_720KFLOP;
	break;
      case 15:
	LO(bx) = FIVE_INCH_FLOPPY;
	break;
      case 18:
	LO(bx) = THREE_INCH_FLOPPY;
	break;
      case 0:
	LO(bx) = dp->default_cmos;
	dp->tracks = 80;
	dp->heads = 2;
	if (LO(bx) == FIVE_INCH_FLOPPY)
	  dp->sectors = 15;
	else
	  dp->sectors = 18;
	d_printf("auto type defaulted to CMOS %d, sectors: %d\n", LO(bx), dp->sectors);
	break;
      default:
	LO(bx) = THREE_INCH_FLOPPY;
	d_printf("type det. failed. num_tracks is: %d\n", dp->tracks);
	break;
      }

      /* these numbers are "zero based" */
      HI(dx) = dp->heads - 1;
      HI(cx) = (dp->tracks - 1) & 0xff;

      LO(dx) = (disk < 0x80) ? FDISKS : HDISKS;
      LO(cx) = (dp->sectors & 0x3f) | (((dp->tracks -1) & 0x300) >> 2);
      LO(ax) = 0;
      HI(ax) = DERR_NOERR;
      REG(eflags) &= ~CF;	/* no error */
    }
    else {
      LWORD(edx) = 0;		/* no hard disks */
      LWORD(ecx) = 0;
      LO(bx) = 0;
      HI(ax) = DERR_BADCMD;
      REG(eflags) |= CF;	/* error */
    }
    break;

    /* beginning of Alan's additions */
  case 0x9:			/* initialise drive from bpb */
    CARRY;
    break;

  case 0x0A:			/* We dont have access to ECC info */
  case 0x0B:
    CARRY;
    HI(ax) = DERR_BADCMD;	/* unsupported opn. */
    break;

  case 0x0C:			/* explicit seek heads. - bit hard */
    CARRY;
    HI(ax) = DERR_BADCMD;
    break;

  case 0x0D:			/* Drive reset (hd only) */
    NOCARRY;
    HI(ax) = DERR_NOERR;
    break;

  case 0x0E:			/* XT only funcs */
  case 0x0F:
    CARRY;
    HI(ax) = DERR_NOERR;
    break;

  case 0x10:			/* Test drive is ok */
  case 0x11:			/* Recalibrate */
    disk = LO(dx);
    if (disk < 0x80 || disk >= 0x80 + HDISKS) {
      /* Controller didnt respond */
      HI(ax) = DERR_CONTROLLER;
      CARRY;
      break;
    }
    else {
      HI(ax) = DERR_CONTROLLER;
      NOCARRY;
    }
    break;

  case 0x12:			/* XT diagnostics */
  case 0x13:
    REG(eax) &= 0xFF;
    CARRY;
    break;

  case 0x14:			/* AT diagnostics. Unix keeps the drive happy
		   so report ok if it valid */
    REG(eax) &= 0xFF;
    NOCARRY;
    break;
    /* end of Alan's additions */

  case 0x15:			/* Get type */
    d_printf("disk gettype %#x\n", disk);
    if (dp != NULL && disk >= 0x80) {
      if (dp->removeable) {
	HI(ax) = 1;		/* floppy disk, change detect (1=no, 2=yes) */
	d_printf("disk gettype: floppy\n");
	LWORD(edx) = 0;
	LWORD(ecx) = 0;
      }
      else {
	d_printf("disk gettype: hard disk\n");
	HI(ax) = 3;		/* fixed disk */
	number = dp->tracks * dp->sectors * dp->heads;
	LWORD(ecx) = number >> 16;
	LWORD(edx) = number & 0xffff;
      }
      REG(eflags) &= ~CF;	/* no error */
    }
    else {
      if (dp != NULL) {
	d_printf("gettype on floppy %d\n", disk);
	HI(ax) = 1;		/* floppy, no change detect=1 */
	NOCARRY;
      }
      else {
	error("gettype: no disk %d\n", disk);
	HI(ax) = 0;		/* disk not there */
	REG(eflags) |= CF;	/* error */
      }
    }
    break;

  case 0x16:
    /* get disk change status - hard - by claiming
	 our disks dont have a changed line we are kind of ok */
    warn("int13: CHECK DISKCHANGE LINE\n");
    disk = LO(dx);
    if (disk >= FDISKS || disktab[disk].removeable) {
      d_printf("int13: DISK CHANGED\n");
      CARRY;
      /* REG(eax)&=0xFF;
	     REG(eax)|=0x200; */
      HI(ax) = 1;		/* change occurred */
    }
    else {
      NOCARRY;
      HI(ax) = 00;		/* clear AH */
      d_printf("int13: NO CHANGE\n");
    }
    break;

  case 0x17:
    /* set disk type: should do all the ioctls etc
	 but I'm not feeling that brave yet */
    /* al=type dl=drive */
    CARRY;
    break;
    /* end of Alan's 2nd mods */

  case 0x18:			/* Set media type for format */
    track = HI(cx) + ((LO(cx) & 0xc0) << 2);
    sect = LO(cx) & 0x3f;
    d_printf("disk: set media type %x failed, %d sectors, %d tracks\n", disk, sect, track);
    HI(ax) = DERR_BADCMD;	/* function not avilable */
    break;

  case 0x20:			/* ??? */
    d_printf("weird int13, ah=0x%x\n", LWORD(eax));
    break;
  case 0x28:			/* DRDOS 6.0 call ??? */
    d_printf("int 13h, ax=%04x...DRDOS call\n", LWORD(eax));
    break;

  case 0x41:                    /* IBM/MS Extensions, install check */
    LWORD(ebx) = IMEXT_MAGIC;
    HI(ax) = IMEXT_VER_MAJOR;
    LWORD(ecx) = IMEXT_API_SUPPORT_BITS;
    NOCARRY;
    break;

  case 0x42: {                  /* IBM/MS Extensions, read */
    struct ibm_ms_diskaddr_pkt *diskaddr;

    FLUSHDISK(dp);
    disk_open(dp);
    diskaddr = SEG_ADR((struct ibm_ms_diskaddr_pkt *), ds, si);
    sect = diskaddr->block_lo % dp->sectors;
    head = (diskaddr->block_lo / dp->sectors) % dp->heads;
    track = diskaddr->block_lo / (dp->heads * dp->sectors);
    buffer = (char *)(((unsigned long)diskaddr->buf_seg << 4)+diskaddr->buf_ofs);
    number = diskaddr->blocks;
    diskaddr->blocks = 0;

    if (checkdp(dp) || track >= dp->tracks) {
      error("ERROR: Sector not found, AH=0x42!\n");
      d_printf("DISK %d ext read [h:%d,s:%d,t:%d](%d)->%p\n",
	       disk, head, sect, track, number, (void *) buffer);
      if (dp) {
	  d_printf("DISK dev %s GEOM %d heads %d sects %d trk\n",
		   dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  d_printf("DISK %x undefined.\n", disk);
      }

      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      show_regs(__FILE__, __LINE__);
      break;
    }

    res = read_sectors(dp, buffer, head, sect, track, number);

    if (res < 0) {
      HI(ax) = -res;
      CARRY;
      break;
    }
    else if (res & 511) {	/* must read multiple of 512 bytes */
      error("ERROR: sector_corrupt 1, return = %d!\n", res);
      HI(ax) = DERR_BADSEC;	/* sector corrrupt */
      CARRY;
      break;
    }

    diskaddr->blocks = res >> 9;
    HI(ax) = 0;
    REG(eflags) &= ~CF;
    R_printf("DISK ext read @%d/%d/%d (%d) -> %p OK.\n",
	     head, track, sect, res >> 9, (void *)buffer);
    break;
  }
  
  case 0x43: {                  /* IBM/MS Extensions, write */
    struct ibm_ms_diskaddr_pkt *diskaddr;

    FLUSHDISK(dp);
    disk_open(dp);
    diskaddr = SEG_ADR((struct ibm_ms_diskaddr_pkt *), ds, si);
    sect = diskaddr->block_lo % dp->sectors;
    head = (diskaddr->block_lo / dp->sectors) % dp->heads;
    track = diskaddr->block_lo / (dp->heads * dp->sectors);
    buffer = (char *)(((unsigned long)diskaddr->buf_seg << 4)+diskaddr->buf_ofs);
    number = diskaddr->blocks;
    diskaddr->blocks = 0;

    if (checkdp(dp) || track >= dp->tracks) {
      error("ERROR: Sector not found, AH=0x42!\n");
      d_printf("DISK %d ext write [h:%d,s:%d,t:%d](%d)->%p\n",
	       disk, head, sect, track, number, (void *) buffer);
      if (dp) {
	  d_printf("DISK dev %s GEOM %d heads %d sects %d trk\n",
		   dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  d_printf("DISK %x undefined.\n", disk);
      }

      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      show_regs(__FILE__, __LINE__);
      break;
    }

    if (dp->rdonly) {
      d_printf("DISK is write protected!\n");
      show_regs(__FILE__, __LINE__);
      if (dp->removeable)
	HI(ax) = DERR_WP;
      else
	HI(ax) = DERR_WRITEFLT;
      REG(eflags) |= CF;
      break;
    }

    if (dp->rdonly)
      error("CONTINUED!!!!!\n");
    res = write_sectors(dp, buffer, head, sect, track, number);

    if (res < 0) {
      HI(ax) = -res;
      CARRY;
      break;
    }
    else if (res & 511) {	/* must read multiple of 512 bytes */
      error("ERROR: sector_corrupt 1, return = %d!\n", res);
      HI(ax) = DERR_BADSEC;	/* sector corrrupt */
      CARRY;
      break;
    }

    diskaddr->blocks = res >> 9;
    HI(ax) = 0;
    REG(eflags) &= ~CF;
    R_printf("DISK ext write @%d/%d/%d (%d) -> %p OK.\n",
	     head, track, sect, res >> 9, (void *)buffer);
    break;
  }

  case 0x44:                    /* IBM/MS Extensions, verify */
    /* Always succeeds. Should perhaps check validity of sector address. */
    NOCARRY;
    HI(ax) = 0;
    break;

  case 0x47:                    /* IBM/MS Extensions, extended seek */
    NOCARRY;
    HI(ax) = 0;
    break;

  case 0x48: {                  /* IBM/MS Extensions, get drive parameters */
    struct ibm_ms_drive_params *params;

    FLUSHDISK(dp);
    disk_open(dp);
    params = SEG_ADR((struct ibm_ms_drive_params *), ds, si);

    if (checkdp(dp)) {
      error("ERROR: Invalid drive, AH=0x48!\n");
      if (dp) {
	  d_printf("DISK dev %s GEOM %d heads %d sects %d trk\n",
		   dp->dev_name, dp->heads, dp->sectors, dp->tracks);
      } else {
	  d_printf("DISK %x undefined.\n", disk);
      }

      HI(ax) = DERR_NOTFOUND;
      REG(eflags) |= CF;
      show_regs(__FILE__, __LINE__);
      break;
    }

    params->flags = IMEXT_INFOFLAG_CHSVALID;
    if (dp->removeable)
      params->flags |= IMEXT_INFOFLAG_REMOVABLE;
    params->tracks = dp->tracks;
    params->heads = dp->heads;
    params->sectors = dp->sectors;
    params->total_sectors_lo = dp->tracks*dp->heads*dp->sectors;
    params->total_sectors_hi = 0;
    params->bytes_per_sector = SECTOR_SIZE;
    if (params->len >= 0x1e)
      params->edd_cfg_ofs = params->edd_cfg_seg = 0xffff;
    NOCARRY;
    HI(ax) = 0;
    break;
  }

  case 0x5:			/* format */
    NOCARRY;			/* successful */
    HI(ax) = DERR_NOERR;
    break;
  case 0xdc:
    d_printf("int 13h, ax=%04x...weird windows disk interrupt\n",
	     LWORD(eax));
    break;
  case 0xf9:			/* SWBIOS installation check */
    CARRY;
    break;
  case 0xfe:			/* SWBIOS get extended cyl count */
    if (dp) LWORD(edx) = dp->tracks % 1024;
    NOCARRY;
    break;
  default:
    d_printf("disk error, unknown command: int13, ax=0x%x\n",
	  LWORD(eax));
    show_regs(__FILE__, __LINE__);
    CARRY;
    return;
  }
}

#define FLUSH_DELAY 2

/* flush disks every FLUSH_DELAY seconds
 * XXX - make this configurable later
 */
void
floppy_tick(void)
{
  static int secs = 0;

  if (++secs == config.fastfloppy) {
    disk_close();
    secs = 0;
  }
}
