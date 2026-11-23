/* Read or Write. (minix version)	Author: Raymond Michiels
 *
 * Reads and Writes are done on a special (simple) format:
 * 	Block 0 = (long) size + name + '\0'
 *	Blocks >1 are consecutively used to store the program
 *
 * This program should be called by:
 * 	write <filename>	= transfer -w <file>
 * 	read			= transfer -r
 *
 * NOTE: lseeks are done rather inefficiently...
 */

#include <stdio.h>

#define BLK_SIZE 512

unsigned char buffer[BLK_SIZE];
int r, fd;
long size;
char name[32];
FILE *file;

pexit(s,n)
char *s;
int n;
{
  disk_quit();
  if (n) printf("%s Error:%d\n",s,n);
  else puts(s);
  exit(n);
}

do_read()
{
  register int blk = 0, bp = BLK_SIZE;
  register long pos;
  printf("Insert special disk in drive 0 and press return.\n");
  getchar();
  disk_init();
  if (r=disk_read(&buffer[0] ,0 ,0))
  	pexit("diskette unreadable.",r);
  size = *(long *)(&buffer[0]);
  strcpy(name, &buffer[4]);
  printf("Reading file: %s (size=%ld)\n",name,size);
  if (!(file=fopen(name,"w")))
  	pexit("Couldn't open file for writing.",r);
  for (pos = 0; pos < size; pos ++) {
  	if (bp == BLK_SIZE) {
		bp = 0;
		if (r=disk_read(buffer, 0, ++blk))
			pexit("Error while reading file.",r);
	}
	fputc(buffer[bp++], file);
  }
  fclose(file);
  pexit("ok.",0);
}

do_write(name)
char *name;
{
  register int c,bp = 0;
  char *p, *rindex();

  printf ("Insert special disk in drive 0 and press return.\n");
  getchar();
  disk_init();
  if (!(file=fopen(name,"r")))
  	pexit("Couldn't open file for reading.",r);
  if ((p = rindex(name,'/')) != 0)
	name = p+1;
  printf("writing: %s\n",name);
  size = 0;
  while ( (c = fgetc(file)) != EOF ) {
	buffer[bp++] = c;
	if (bp == BLK_SIZE) {
		if (r=disk_write(buffer, 0, (int) (size / BLK_SIZE) + 1))
			pexit("Error while writing.",r);
		size += BLK_SIZE;
		bp = 0;
		}
  }
  if (r=disk_write(buffer,0,(int) (size / BLK_SIZE) + 1))
	pexit("Error while writing.",r);
  size += bp;
  *(long *)(&buffer[0]) = size;

  strcpy(&buffer[4], name);
  if (r=disk_write(buffer,0, 0))
	pexit("Error while writing.",r);
  printf("(size = %ld)\n",size);

  fclose(file);
  pexit("ok.",0);
}

disk_init()
{
  if ((fd = open("/dev/dd0", 2)) <= 0)
	pexit("couldn't open /dev/dd0", fd);
  return 0;
}

disk_quit()
{
  close (fd);
}

disk_read(addr, dr, blk)
char *addr;
int dr, blk;
{
  if (dr != 0)
	pexit("sorry, can only access drive #0");
  lseek(fd, (long)blk*BLK_SIZE, 0);
  if ((r = read(fd, addr, BLK_SIZE)) != BLK_SIZE)
	pexit("read error", r);
  return 0;
}

disk_write(addr, dr, blk)
char *addr;
int dr, blk;
{
  if (dr != 0)
	pexit("sorry, can only access drive #0");
  lseek(fd, (long)blk*BLK_SIZE, 0);
  if ((r = write(fd, addr, BLK_SIZE)) != BLK_SIZE)
	pexit("write error", r);
  return 0;
}

main(argc,argv)
int argc;
char **argv;
{
  if ((argc > 1) && (argv[1][0]=='-'))
	switch (argv[1][1]) {
	case 'r' :
		if (argc == 2) do_read();
	case 'w' :
		if (argc == 3) do_write(argv[2]);
	}
  else if (!strcmp(argv[0],"read") && argc == 1)
  	do_read();
  else if (!strcmp(argv[0],"write") && argc == 2)
	do_write(argv[1]);

  puts("Usage: \"read\" or \"write <file>\" or use -r or -w flag");
}
