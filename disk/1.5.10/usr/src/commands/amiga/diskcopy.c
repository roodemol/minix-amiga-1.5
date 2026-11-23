/* Diskcopy1.1		Uses zone-map info.		By Raymond Michiels
 */

#include <stdio.h>
#include <errno.h>

#define NR_BLKS 720
#define BLK_SIZE 1024

char *buffer[720], *malloc();
int map[512], offset, valid_fs, nr_bufs, fs_size;
extern int errno;

int init_bufs()
{
  nr_bufs = 0;

  while (nr_bufs < NR_BLKS)		/* We don't have enough yet */
	if (!(buffer[nr_bufs++] = malloc(BLK_SIZE))) {
		--nr_bufs;
		break;			/* Alloc-ed everything we could */
	}
  return(nr_bufs);
}

int init_map()
{
  int fd, r;

  printf ("Insert source disk in drive 0 and press <return>\n");
  getlf ();

  fd = open("/dev/dd0",0);

  lseek(fd, 1024L, 0);
  if ((r = read(fd, map, 1024)) != 1024)	/* Actually super block */
	return (r);

  if (map[5] || map[8] != 0x137F) {
	valid_fs = 0;
	return -1;
  }

  valid_fs = 1;
  fs_size = map[1];
  offset = map[4];

  lseek(fd, 3072L, 0);
  if ((r = read(fd, map, 1024)) != 1024)
	return (r);

  close(fd);
  return(0);
}

int in_use(blk)
int blk;
{
  int i = blk - offset + 1;

  if (!valid_fs || i < 0)
	return (1);
  if (blk >= fs_size)
	return (0);
  return (map[i>>4] >> (i&15) & 1);
}

getlf()
{
  while (getchar() != '\n')
	;
}

int read_buf(from, to)		/* Read as many blocks as possible */
int from, to;			/* from 'from' upto 'to'-1 */
{
  int blk = from, fd, bufp = 0, r;

  if (from) {			/* Don't ask first time */
	printf ("Insert source disk in drive 0 and press <return>\n");
	getlf ();
  }
  fd = open ("/dev/dd0", 0);
  while (bufp < nr_bufs && blk < to) {
	if (in_use (blk)) {
		lseek(fd, (long)blk*BLK_SIZE, 0);
		if ((r=read(fd, buffer[bufp++], BLK_SIZE)) != BLK_SIZE)
			printf ("Read error (%d) on block %d, skipped.\n",
					r, blk);
	}
	++blk;
  }
  printf ("Read %d blocks in the range %d to %d\n", bufp, from, blk);
  close (fd);
  sync ();
  return (blk);		/* next block to be read */
}

int write_buf(from, to)
int from, to;
{
  int blk = from, fd, bufp = 0, r;

  printf ("Insert target disk in drive 0 and press <return>\n");
  getlf ();
  fd = open ("/dev/dd0", 1);
  while (bufp < nr_bufs && blk < to) {
	if (in_use (blk)) {
		lseek(fd, (long)blk*BLK_SIZE, 0);
		if ((r=write(fd, buffer[bufp++], BLK_SIZE)) != BLK_SIZE)
			printf ("Write error (%d) on block %d.\n", r, blk);
	}
	++blk;
	sync();		/* Keep file system from caching */
  }
  close (fd);
  sync ();
  return (to);
}

main(argc, argv)
int argc;
char *argv[];
{
  int r, nr_bufs, fd, blk = 0, from;

  perprintf(stdout);		/* flush after each printf */
  if (umount("/dev/dd0") && errno == EBUSY) {
	printf("Couldn't umount /dev/dd0. Device busy.\n");
	exit(1);
  }
  if ((r = init_map()) > 0) {
	printf ("Couldn't read zone-map.\n");
	exit(r);
  }
  if ((nr_bufs = init_bufs()) == 0) {
	printf ("Couldn't alloc any buffers!\n");
	exit(1);
  }

  while (blk < NR_BLKS) {
	from = blk;
	blk = read_buf (from, NR_BLKS);
	write_buf (from, blk);
  }
  printf ("Disk copied.\n");
}
