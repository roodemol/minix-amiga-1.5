#if (MACHINE == AMIGA)
/* "amfloppy.c"					Author: Raymond Michiels
 *
 * This file contains a floppy disk controller which reads raw tracks using
 * the Amiga hardware and then converts these to binary data according to
 * the IBM-PC 720Kb/360Kb (double and single sided), 80 cylinder 'standard'.
 * The driver supports two operations: read a block and write a block.
 * It accepts two messages, one for reading and one for writing, both using
 * message format m2 and with the same parameters:
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADDRESS
 * ----------------------------------------------------------------
 * |  DISK_READ | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DISK_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |SCATTERED_IO| device  | proc nr | requests|         | iov ptr |
 * ----------------------------------------------------------------
 *
 * The file contains only one major entry point:
 *
 *   floppy_task:	main entry when system is brought up
 *
 * Two minor entry points are:
 *
 *   dskblk_int:	interrupt handler for DMA_READY.
 *   index_int:		interrupt handler for INDEX_FOUND.
 *
 *
 * The device numbers are allocated as follows:
 *
 *   number,  name,  description.
 *
 *   0...3   fd0..fd3  single sided diskette in drive 0..3
 *  (4...7   df0..df3  reserved for future use on different formats)
 *   8..11   dd0..dd3  double sided diskette in drive 0..3
 *
 * Although there are no AMIGAs that can't read double sided disks the
 * capability to use single sided disks is included to provide full
 * compatibility with the Atari-ST version of minix.
 */

#include "kernel.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/amtransfer.h>
#include "proc.h"
#include "amhardware.h"

#undef ABS
#define ABS(x) ((x)<0 ? -(x) : (x))
#define NR_SECTORS	 9	/* 720 Kb IBM disk */
#define NR_CYLINDERS    80
#define NR_SIDES	 2
#define NR_DRIVES	 4	/* maximum # of supported drives */
#define SECTOR_SIZE    512

#define MOTOR_RUNNING	 1	/* internal messages */
#define TIMED_OUT	 2
#define INDEX_FOUND	 4
#define DMA_READY	 8
#define SEEK_READY	16
#define DO_FLUSH	32

#undef OK
#define OK		 0	/* Error return values */
#define E_NO_DRIVE	-1
#define E_BAD_ARGS	-2
#define E_SYNC		-3
#define E_DISK_DMA	-4
#define E_NO_DISK	-5
#define E_WRONG_CYL	-6
#define E_WR_PROT	-7
#define E_CRC		-8
#define E_BAD_DISK	-9
#define E_TIME_OUT	-10
#define E_BAD_SEEK	-11

#define H_ID		 0	/* Offsets in the HEADER_FIELD */
#define H_CYLINDER	 1
#define H_SIDE		 2
#define H_SECTOR	 3
#define H_LENGTH	 4
#define H_CRC		 5
#define H_SIZE		 7

#define D_ID		 0	/* Offsets in the DATA_FIELD */
#define D_DATA		 1
#define D_CRC          513
#define D_SIZE         515

#define H_ID_MFM    0x5554	/* uncoded: 0xFE */
#define H_ID_BIN      0xFE

#define D_ID_MFM    0x5545	/* uncoded: 0xFB */
#define D_ID_BIN      0xFB

/* track= (GAP1 GAP2 SYNC HEADER GAP3 GAP4 SYNC DATA)+ */
/* sizes:   70 + 12 +  3 +   7  + 22 + 12 +  3 + 515  = 644 */

#define GAP1_SIZE	70
#define GAP2_SIZE	12
#define GAP3_SIZE	22
#define GAP4_SIZE	12
#define SYNC_SIZE	 3

#define H_OFFSET   (GAP1_SIZE + GAP2_SIZE + SYNC_SIZE)
#define D_OFFSET   (H_OFFSET + H_SIZE + GAP3_SIZE + GAP4_SIZE + SYNC_SIZE)

#define RAW_S_SIZE (GAP1_SIZE + GAP2_SIZE + SYNC_SIZE + H_SIZE + \
			GAP3_SIZE + GAP4_SIZE + SYNC_SIZE + D_SIZE)

#define RAW_D_SIZE (SYNC_SIZE + H_SIZE + GAP3_SIZE + GAP4_SIZE + \
			SYNC_SIZE + D_SIZE)

#define RAW_T_SIZE  0x1D00	/* room for a full track */
#define WIPESIZE    0x0400	/* area to wipe before writeing */
#define BUFSIZE     (WIPESIZE + (RAW_S_SIZE) * NR_SECTORS + 2)
#define GAP1_DATA   0x9254	/* uncoded: 0x4E */
#define GAP2_DATA   0xAAAA	/* uncoded: 0x00 */
#define GAP3_DATA   0x9254
#define GAP4_DATA   0xAAAA
#define SYNC_DATA   0x4489	/* uncoded: 0xA1 */

#define MAX_RETRIES      7	/* max # of retries on disk access */

#define MOTORON_DELAY  (HZ / 2)	/* wait 500ms to get motor turning */
#define MOTOROFF_DELAY (3 * HZ)	/* wait 3 sec before turning motor off */
#define ROTATION_DELAY (2 * HZ)	/* disk-dma should be finished within */
		/* a single rotation (200 ms), so 2 secs should be enough */
/* this may seem a bit exaggerated but accounts for dumping proctables 
 * which occurs at a higher interrupt level
 */

PRIVATE struct {
	long recal, seek_rate, Hcrc, Scrc;
} stat;

PRIVATE struct disk {
	int	cyl, side, dirty, valid, checked, delay,
		wr_prot, num;
	u_char	sel;
	u_short	*buf;
} disk[NR_DRIVES];

typedef struct disk *d_ptr;

PRIVATE u_short *readbuf, *dummybuf;		/* CHIPMEM-buffer for read */

#ifdef NEED_INDEX
PRIVATE u_short dsklen_val = 0;
#endif

PRIVATE int to_flush = 0;
PRIVATE int stepdelay;
PRIVATE struct transferdata *transdat;
PRIVATE int seek_offset, seek_delay;
PRIVATE long debug, clock_freq;
PRIVATE d_ptr seek_dp;
PRIVATE void portBout();
PRIVATE int adjust_buffer();
PRIVATE int last_msg = 0;
PRIVATE void build_track();
PRIVATE void clock_mess();
PRIVATE void dperror();
PRIVATE void my_receive();

PUBLIC  void clock_start_motor(); 
PUBLIC  void motor_off();
PUBLIC  void release_int();

/*========================================================================*
 *				floppy_task				  *
 *========================================================================*/

PUBLIC void floppy_task()
{
/* Main program of the floppy disk driver task */

  int caller, proc_nr, i, r;
  d_ptr dp;
  static message req;
  u_short *sys_alloc();

  transdat = *(struct transferdata **)0x0000;
  debug = transdat->args['d'-'a'];
  stepdelay = transdat->args['r'-'a'];
  readbuf = sys_alloc(RAW_T_SIZE * 2L);
  dummybuf = sys_alloc( (long) SECTOR_SIZE);
  enable_int(EXTER);

  for (i = 0; i < NR_DRIVES; i++) {
	dp = &disk[i];
	dp->num = i;
	dp->sel = ((DSK_SEL0) << i);
	dp->dirty = 0;
	dp->valid = FALSE;
	dp->checked = 0;
	dp->cyl = NR_CYLINDERS;
	if (connected(dp)) {
		dp->buf = sys_alloc(BUFSIZE * 2L);
		build_track(dp);
		motor_off(dp);
		portBout(BSET, dp->sel);
	} else {
		dp->buf = (u_short *)0;
	}
  }

  /* Initialize timer B of CIA B for stepping */

  clock_freq = transdat->args['t'-'a'];
  stat.seek_rate = clock_freq /1000L * transdat->args['r'-'a'] / 10000L;
  *TBLOB = stat.seek_rate & 0xFF;
  *TBHIB = (stat.seek_rate >> 8) & 0xFF;
 
/* Here is the main loop of the disk task.  It waits for a message,
 * carries it out, and sends a reply.
 */

  while (TRUE) {
/* first wait for a request to read or write a disk block. */
	last_msg = 0;
	receive(ANY, &req);	/* get a request to do some work */

	caller = req.m_source;
	proc_nr = req.PROC_NR;

/*	if ((caller < 0) && !(last_msg & DO_FLUSH))
 *		panic("disk task got message from", req.m_source);
 *
 *	we expect spurious interrupts. see also code in fd_timer
 */
	/* Now carry out the work. */
	switch(req.m_type) {
		case DISK_READ:
		case DISK_WRITE:r = do_rdwt(&req);	break;
		case SCATTERED_IO:r = do_vrdwt(&req, do_rdwt); break;
	/*	case HARD_INT:	r = do_flush();		break; */
		default:	r = EINVAL;		break;
	}

	if (req.m_type != HARD_INT) {
		/* Finally, prepare and send the reply message. */
		req.m_type = TASK_REPLY;
		req.REP_PROC_NR = proc_nr;
		req.REP_STATUS = r;	/* # bytes transferred or error code */
		send(caller, &req);	/* send reply to caller */
	}
	if (to_flush)
		if (r = do_flush())
			dperror(dp, "flush error: %d\n", r);
  }
}


PRIVATE void portBout(setclr, mask)
u_short setclr;
u_char mask;
{  if (setclr) *PRBB |= mask;
   else *PRBB &= ~mask;
}

/*========================================================================*
 *	start_motor, stop_motor, clock_start_motor, motor_off		  *
 *========================================================================*/

/* Controlling the disk motors is a big pain.  The first problem is
 * that after turning the motor on, you have to wait about half a second
 * for the motor to be on full speed.  The second problem, however, is much
 * more troublesome.  Turning to motor off right after each read or write
 * request would introduce a 500 millisec delay for each next disk access,
 * so it would be better to keep the motor on all the time. Unfortunately,
 * this approach will surely wear out your diskette.
 * The best solution is to wait a while after each disk access, to
 * see if there will be a second access.  If this access does not arrive
 * within 3 seconds for example, the drive is turned off.
 * Because we know (or rather: suppose) that the drive won't be used for
 * some while this is probably also the best time to flush the current
 * track buffer.
 */

PRIVATE void start_motor(dp, start_delay)
d_ptr dp;
int start_delay;
{
  lock();
  portBout(BSET, dp->sel);
  portBout(BCLR, DSK_MOTOR);
  portBout(BCLR, dp->sel);
  if ((dp->delay < 0) && (start_delay > 0)) {
	dp->delay = 0;
	unlock();
	clock_mess(start_delay, clock_start_motor);
	my_receive(HARDWARE, MOTOR_RUNNING);
  } else {
	dp->delay = 0;
	unlock();
  }
}

PUBLIC void clock_start_motor()
{
  last_msg |= MOTOR_RUNNING;
  interrupt(FLOPPY);		/* signal FLOPPY that the motor is running */
}

PRIVATE void stop_motor(dp)
d_ptr dp;
{
  if (dp->delay < 0)
	dperror(dp, "stop_motor: motor already off\n");
  lock();
  portBout(BSET, dp->sel);
  dp->delay = MOTOROFF_DELAY / 6;	/* HACK; depends on SCHED_RATE */
  unlock();
}

PUBLIC void motor_off(dp)
d_ptr dp;
{
  int s;

  s = lock();
	portBout(BSET, DSK_MOTOR | dp->sel);
	portBout(BCLR, dp->sel);
	portBout(BSET, dp->sel);
	dp->delay = -1;
  restore(s);
}

PUBLIC void fd_timer()
{
/* This routine is called every clock tick, to check if there
 * are any motors to be turned off.
 */
  register int i;
  register d_ptr dp;

  for (i = 0; i < NR_DRIVES; i++) {
	dp = &disk[i];
	if ((dp->delay > 0) && (--dp->delay == 0))
		if (!dp->dirty) {
			motor_off(dp);
		} else {
			to_flush |= (1<<i);
			if (!(last_msg & DO_FLUSH)) {
				last_msg |= DO_FLUSH;
				interrupt(FLOPPY);
			}
			/* motor will be turned off later */
		}
  }
}

PRIVATE void movehead(dp, dir)
d_ptr dp;
int dir;
{
/* Move the head of the current drive one cylinder up (dir > 0) or down
 * (dir <= 0).
 */
  portBout(BCLR, dp->sel);
  portBout((dir > 0) ? BCLR : BSET, DSK_DIREC);
  portBout(BCLR, DSK_STEP);
  portBout(BSET, DSK_STEP);	/* give an active low pulse */

}

PRIVATE int writeprotect()
{
/* Check whether the current drive is write-protected */

  return ((*PRAA & DSK_WPROT) == 0);
}

PRIVATE int read_track(dp)
d_ptr dp;
{
/* Read a full track into readbuf. Reading starts immediately after the
 * first sync found. Later the raw material will be examined to see
 * which blocks were read in what order.
 */
  *DSKSYNC = SYNC_DATA;
  *ADKCON = (WCLR | PRECOMPMASK | MSBSYNC);
  *ADKCON = (WSET | (dp->cyl > 39 ? PRECOMP140 : PRECOMP0) |
			MFMPREC | WORDSYNC | FAST);
  *DSKLEN = 0;
  *DMACON = (WSET | DSKEN);

  *DSKPT = readbuf;
  *DSKLEN = *DSKLEN = (0x8000 /* ~DMAEN */ | DMA_READ | RAW_T_SIZE);
  clock_mess(ROTATION_DELAY, release_int);

/* Now wait for disk DMA to terminate. If, however, the current track
 * does not contain any SYNCs disk-dma will never terminate, so a watchdog
 * timer has to be used to prevent the system from hanging.
 */
  enable_int(DSKBLK);
  my_receive(HARDWARE, DMA_READY | TIMED_OUT);
  disable_int(DSKBLK);
  clock_mess(0, (void (*)())0);		/* Disable watchdog timer */
  if (last_msg & DMA_READY) {
	last_msg &= ~DMA_READY;
	return (adjust_buffer(dp));
  }
  if (last_msg & TIMED_OUT)
	return (E_DISK_DMA);
  dperror(dp, "read_track: last_msg = %d\n", last_msg);
  return (E_DISK_DMA);
}


PRIVATE int write_track(dp)
d_ptr dp;
{
/* Write a full track from "Buffer" to the selected drive.  Writing is
 * done in MFM-format (just as on IBM-PCs and Atari-STs) with 2 microsec/bit.
 */
  *ADKCON = (WCLR | PRECOMPMASK | MSBSYNC);
  *ADKCON = (WSET | (dp->cyl > 39 ? PRECOMP140 : PRECOMP0) | MFMPREC | FAST);
  *DMACON = (WSET | DSKEN);

  *DSKLEN = 0;
  *DSKPT = dp->buf;
#ifdef NEED_INDEX
  dsklen_val = (0x8000 /* ~DMAEN */ | DMA_WRITE | BUFSIZE);
  clock_mess(ROTATION_DELAY, release_int);

/* now wait for either an interrupt from the hardware because an index-hole
 * was detected, or an interrupt from the watchdog timer because the index-
 * hole interrupt did not appear. (Which means the disk is malfunctioning.)
 * 
  enable_ciab_int(INDEX);
  my_receive(HARDWARE, TIMED_OUT | INDEX_FOUND);
  disable_ciab_int(INDEX);

  clock_mess(0, void (*)())0);
  if (last_msg & TIMED_OUT) return(E_NO_DRIVE);
 
/* The index_int routine starts DMA, we just wait until it has finished */
#else
  *DSKLEN = *DSKLEN = (0x8000 /* ~DMAEN */ | DMA_WRITE | BUFSIZE);
#endif /* NEED_INDEX */

  enable_int(DSKBLK);
  my_receive(HARDWARE, DMA_READY);
  disable_int(DSKBLK);

  *DSKLEN = 0;			/* disable diskDMA */
  return (OK);
}


PRIVATE u_char MFM2bin(code)
register u_short code;
{
/* Convert an MFM-word ("code") to its binary equivalent.
 */
  register u_char bin = 0;
  register u_short c1, c2;

  for (c1 = 1, c2 = 1; c1 < 255; c1 <<= 1, c2 <<= 2 )
	if (code & c2) bin |= c1;
  return bin;
}

PRIVATE void bin2MFM(dp, offset, byte)
d_ptr dp;
int offset;
u_char byte;
{
/* This routine converts a binary code ("byte") into its MFM equivalent,
 * and is then stored in the track buffer at "offset". We have to know
 * the offset because the transformation depends on the previous MFM-word.
 */
  register u_short byte2, code = 0;
  register int ci, bi, bbi;

  byte2 = byte|(dp->buf[offset-1] << 8);

  for(ci = 1, bi = 1, bbi = 3; bi < 130; ci <<= 2, bi <<= 1, bbi <<= 1) {
	if (byte2 & bi) code |= ci;			/* MFM data-bit */
	if ((byte2 & bbi) == 0) code |= (ci << 1);	/* MFM tag-bit */
  }
  dp->buf[offset] = code;
}


PRIVATE void bin2raw(dp, binbuf, st)
d_ptr dp;
u_char binbuf[];
int st;
{

  u_short c;
  int offset = st * RAW_S_SIZE + D_OFFSET + WIPESIZE;

  c = b2r(&binbuf[0], &dp->buf[offset + D_DATA - 1]);
  bin2MFM(dp, offset + D_CRC, (u_char)(c >> 8));	/* store the CRC */
  bin2MFM(dp, offset + D_CRC + 1, (u_char)(c&0xFF));
  dp->buf[offset + D_CRC + 2] = 0x5254;		/* CRC/MFM bug-fix */
}

PRIVATE int raw2bin(dp, st, binbuf)
d_ptr dp;
int st;
u_char binbuf[];
{
  u_short c, d;
  int offset = st * RAW_S_SIZE + D_OFFSET + WIPESIZE;

  c = r2b(&binbuf[0], &dp->buf[offset + D_DATA]);
  d = (MFM2bin(dp->buf[offset + D_CRC]) << 8) + 
		(MFM2bin(dp->buf[offset + D_CRC + 1]));
  if (c != d) {
	if (debug &(1L<<30))
		dperror(dp, "CRC error in sector %d: 0x%x should be 0x%x\n",
		 st+1, d, c);
	++stat.Scrc;
	return (E_CRC);
  }
  dp->checked |= (1<<st);
  return(OK);
}

PRIVATE int adjust_buffer(dp)
d_ptr dp;
{
/* Copy the raw track in Buffer2 to Buffer block by block, keeping in
 * mind that the sectors are not necessarily in the correct order.
 * (The track in Buffer2 may look like: "4 5 6 7 8 9 x 1 2 3" instead
 * of "1 2 3 4 5 6 7 8 9", so we may have to re-arrange the blocks.)
 */
  int count, p, r, st;
  int tmpoff, offset;
  char check[NR_SECTORS];
  int track_prev, track_now, nwrong;

  count=0;
  offset=0;
  track_now=0;
  track_prev=dp->cyl;
  nwrong=0;

  for (st = 0; st < NR_SECTORS; st++)
	check[st] = 0;
  while (offset < RAW_T_SIZE && count < NR_SECTORS) {
	while (readbuf[offset] != SYNC_DATA) offset++;
	while (readbuf[offset] == SYNC_DATA) offset++;

	if (readbuf[offset] != H_ID_MFM)		/* Tough luck */
		continue;
	tmpoff = offset;	/* Save header offset */

	st = MFM2bin (readbuf[tmpoff + H_SECTOR])-1;
	if (st < 0 || st >= NR_SECTORS)	{	/* should use header CRC's */
		if (debug&(1L<<30))
			dperror(dp, "sector %d found\n", st);
		continue;
	}
	if (MFM2bin (readbuf[tmpoff + H_SIDE]) != dp->side) {
		dperror(dp, "side inconsistency: found %d\n",
			MFM2bin (readbuf[tmpoff + H_SIDE]) );
		continue;
	}	
	offset += H_SIZE + GAP3_SIZE;

	while (readbuf[offset] != SYNC_DATA) offset++;
	while (readbuf[offset] == SYNC_DATA) offset++;

	if (readbuf[offset] != D_ID_MFM)		/* Bad track */
		continue;
	if (check[st]) {
		if (debug&(1L<<30))
			dperror(dp, "sector %d found twice\n", st);
		continue;	/* Sector appeared twice on same track */
	}
	check[st] = 1; count++;
					/* First copy header */
	long_copy (&readbuf[tmpoff],
	&(dp->buf[WIPESIZE + H_OFFSET + st * RAW_S_SIZE]), (H_SIZE+1) / 2L);
					/* Then copy data block */
	long_copy (&readbuf[offset],
	&(dp->buf[WIPESIZE + D_OFFSET + st * RAW_S_SIZE]), (D_SIZE+1) / 2L);

	offset += D_SIZE;
	track_now= MFM2bin (readbuf[tmpoff + H_CYLINDER]);
	if (track_now!=track_prev) {
		++nwrong;
		track_prev=track_now;
	}
  }
  if (count < NR_SECTORS) {
	if (debug&(1L<<30))
		dperror(dp, "%d sectors found\n", count);
	for (st = 0; st < NR_SECTORS; st++)
		if (!check[st])		/* Mark sector as bad */
			dp->buf[WIPESIZE + D_OFFSET + D_DATA + st*RAW_S_SIZE]++;
	return (E_BAD_DISK);
  } else {
	if (nwrong) {
		stat.recal++;
		if (debug & (1L<<30))
			dperror(dp, "recal, n=%d, %d->%d\n",
			 nwrong, track_prev, dp->cyl);
		p = dp->cyl; dp->cyl = track_prev;
		if (r = seek(dp, p, dp->side))
			return (r);
		return(E_WRONG_CYL);    
	}
	return (OK);
  }
}

PRIVATE int seek(dp, cl, sd)
d_ptr dp;
int cl, sd;
{
/* Get the drive(s) to be ready for reading or writing on drive dr,
 * cylinder cl and side sd. If the track buffer was dirty it has to
 * be re-written before we can go to another cylinder or drive.
 */
  int r;

  if ((dp->cyl == cl) && (dp->side == sd))
	return (OK);

  if (dp->dirty)
	if (r=rdwt_track(dp, WRITE))
		return (r);

  dp->valid = FALSE;

  seek_offset = cl - dp->cyl;
  seek_delay = 4;
  seek_dp = dp;
  start_motor(dp, (int) (MOTORON_DELAY - (long) ABS(seek_offset + seek_delay) * stepdelay * HZ / 1000000L));

  enable_ciab_int (ICRTB);

  *CRBB = 0x11;			/* start timer */
  my_receive(HARDWARE, SEEK_READY);

  dp->wr_prot = writeprotect();
  dp->cyl = cl;	
  dp->side = sd;
  return (OK);
}

PRIVATE int rdwt_track(dp, acc)
d_ptr dp;
int acc;			/* READ or WRITE */
{
/* Here we (try to) read or write a raw track to disk by calling
 * read_track or write_track respectively.
 */
  register int r = OK, retries = MAX_RETRIES;

  dp->valid = FALSE;

  start_motor(dp, MOTORON_DELAY);
  portBout((dp->side == 0) ? BSET : BCLR, DSK_SIDE);
  do {
	r = (acc == READ ? read_track(dp) : write_track(dp));
  } while (r && retries--);
  stop_motor(dp);
  if (acc == READ) dp->valid = (r == OK);
  dp->dirty = 0;
  return (r);
}

PRIVATE int read_block(dp, st, address)
d_ptr dp;
int st;
u_char *address;
{
/* Try to read a block.  If it's already in cache check its CRC.  If the
 * CRC is wrong or the block wasn't in cache call read the entire track
 */
  register r, retries;

  if (!dp->valid)
	dperror(dp, "read_block: buf not valid\n");
  for (retries = MAX_RETRIES; retries; --retries) {
	r = raw2bin(dp, st, address);
	if (r) r = rdwt_track(dp, READ);
	else break;
  }
  if (r) {
	++stat.Hcrc;
#ifdef DEBUG
	dperror(dp, "Unrecoverable read error\n");
#endif
  }
  return (r);
}

PRIVATE int write_block(dp, st, address)
d_ptr dp;
int st;
u_char *address;
{
/* Write a block to cache.  Be sure all CRC's in chache are valid to prevent
 * loss of data.
 */
  register int r, i, retries;

  if (dp->checked != 511) {
	for (retries = MAX_RETRIES; retries; --retries) {
		for (i = 0; i < NR_SECTORS; i++)
			if (r = raw2bin(dp, i, dummybuf)) {
dperror(dp, "Found bad CRC on write... will be recovered\n");
				break;
			}
		if (r) r = rdwt_track(dp, READ);
		else break;
	}
	if (r) {
	++stat.Hcrc;
dperror(dp, "Found real bad CRC on write... won't be recovered\n");
		return (r);
	}
  }

  bin2raw(dp, address, st);
  dp->dirty |= (1<<st);
  return OK;
}

PRIVATE int phys_convert(pos, device, dr, cyl, sd, st)
long pos;
int device, *dr, *cyl, *sd, *st;
{
/* Convert a position on disk to physical parameters as cylinder,
 * side and sector. When xxxxxxxxxxxxxx a 720Kb double sided diskette
 * is assumed, else we assume a 360Kb single sided diskette.
 */
  int blk, nr_sides;

  nr_sides = device>3 ? 2 : 1;
  if (pos%SECTOR_SIZE != 0)
	return (EINVAL);
  blk = pos/SECTOR_SIZE;
  if (blk < 0 || blk >= (NR_CYLINDERS * nr_sides * NR_SECTORS) )
	return(EINVAL);

  *cyl = blk / (nr_sides * NR_SECTORS);
  *st = blk % NR_SECTORS;
  *sd = (blk % (nr_sides * NR_SECTORS)) / NR_SECTORS;
  *dr = device & 3;
  return(OK);
}

PRIVATE void build_track(dp)
d_ptr dp;
{
/* Fill in the raw track buffer with the right gaps and syncs, which will
 * not always be transferred from the disk, but need to be there when
 * the buffer is re-written to disk.
 */
  register int st, i;
  register u_short *p;

  p = dp->buf;
  for (i = 0; i < WIPESIZE ; i++)
	*p++ = GAP1_DATA;

  for (st = 0; st < NR_SECTORS; st++) {
	for (i = 0; i < GAP1_SIZE; i++)
		*p++ = GAP1_DATA;
	for (i = 0; i < GAP2_SIZE; i++)
		*p++ = GAP2_DATA;

	for (i = 0; i < SYNC_SIZE; i++)	/* header-sync */
		*p++ = SYNC_DATA;
	p += H_CRC + 2;

	for (i = 0; i < GAP3_SIZE; i++)	/* gap between header and data */
		*p++ = GAP3_DATA;
	for (i = 0; i < GAP4_SIZE; i++)
		*p++ = GAP4_DATA;

	for (i = 0; i < SYNC_SIZE; i++)	/* data-sync */
		*p++ = SYNC_DATA;
	p += D_CRC + 2;
  }
  while (p < dp->buf + BUFSIZE)
	*p++ = GAP1_DATA;
}

PRIVATE int connected(dp)
d_ptr dp;
{
/* See if drive dp is actually connected */
  int i;
  unsigned long r = 0L;

  if (dp->num == 0)
	return (TRUE);
  portBout(BSET, dp->sel);
  portBout(BCLR, DSK_MOTOR);
  portBout(BCLR, dp->sel);

  portBout(BSET, dp->sel | DSK_MOTOR);
  portBout(BCLR, dp->sel);
  for (i = 0; i < 32 ; i++) {
	portBout(BCLR, dp->sel);
	r <<= 1;
	if (*PRAA & DSK_RDY)
		r |= 1;
	portBout(BSET, dp->sel);
  }
  return (r == 0);
}

PRIVATE int disk_changed(dp)
d_ptr dp;
{
/* Check if drive dp had its disk changed */
  int r;

  lock();
  portBout(BSET, dp->sel);
  portBout(dp->delay > 0 ? BCLR : BSET, DSK_MOTOR);
  portBout(BCLR, dp->sel);
  r = (*PRAA & DSK_CHANGE);
  dp->wr_prot = ((*PRAA & DSK_WPROT) == 0);
  portBout(BSET, dp->sel);
  unlock();
  return (!r);
}

PRIVATE int do_rdwt(m_ptr)
message *m_ptr;
{
/* Carry out a read or write request from the disk. */
  d_ptr dp;
  int cyl, sd, st, dr, r, nbytes = 0;
  phys_bytes address;
  extern phys_bytes umap();
  struct proc *rp;

  rp = proc_addr(m_ptr->PROC_NR);	/* Compute absolute address */
  address = umap(rp, D, (vir_bytes)m_ptr->ADDRESS, (vir_bytes)m_ptr->COUNT);
  if (address == 0)
	return (EINVAL);

  if (m_ptr->COUNT % SECTOR_SIZE != 0)
	return (EINVAL);

  do {
	if (r = phys_convert(m_ptr->POSITION, m_ptr->DEVICE,
		&dr, &cyl, &sd, &st)) return (r);
	dp = &disk[dr];
	if (!(dp->buf)) {
		printf("Floppy: drive %d not connected\n", dr);
		return (EINVAL);
	}
	if (disk_changed(dp)) {		/* Also checks for write protection */
		dp->valid = FALSE;
	}
	if (m_ptr->m_type == DISK_WRITE && dp->wr_prot) {
		dperror(dp, "drive is write-protected\n");
		return (E_WR_PROT);
	}
	if (r = seek(dp, cyl, sd)) return (r);
	if (!dp->valid)
		if (r = rdwt_track(dp, READ)) return (r);
	if (m_ptr->m_type == DISK_READ)
		r = read_block(dp, st, address + nbytes);
	else
		r = write_block(dp, st, address + nbytes);
	m_ptr->POSITION += SECTOR_SIZE;
	nbytes += SECTOR_SIZE;
  } while ( r == OK && (m_ptr->COUNT -= SECTOR_SIZE) > 0 );
  return (r ? r : nbytes);
}

PRIVATE int do_flush()
{
  int tmp, dr, r;
  d_ptr dp;

  if (!(last_msg & DO_FLUSH))
	return (OK);
  lock();
	tmp = to_flush;
	to_flush = 0;
	last_msg &= ~DO_FLUSH;
  unlock();
  for (dr = 0; dr < NR_DRIVES; dr++)
	if (tmp & (1<<dr)) {
		dp = &disk[dr];
		if ((r = rdwt_track(dp, WRITE)) != OK)
			dperror(dp, "flush: couldn't flush (error:%d)\n", r);
		dp->delay = 1;	/* Turn motor off on next fd_timer call */
	}
  return(r);
}

PUBLIC void flstep_int()
{
/* Step timer has gone off.  Step in the direction as indicated by the
 * global variable seek_offset
 */
  if (--seek_delay > 0)
	return;
  else if (seek_offset > 0)
	movehead(seek_dp, seek_offset--);
  else if (seek_offset < 0)
	movehead(seek_dp, seek_offset++);
  else {
	last_msg |= SEEK_READY;		/* We've reached the right track */
	*CRBB = 0;			/* stop timer */
	disable_ciab_int (ICRTB);
	interrupt(FLOPPY);
  }
}

PUBLIC void dskblk_int()
{
/* this routine is called by every level 1 interrupt, this
 * includes diskdma_ready and serialport_tbe
 */

  if (*INTREQR & TBE)
	siaint(2);
  if (*INTREQR & DSKBLK) {	/* Disk-DMA has finished; signal FLOPPY. */
        *INTREQ = (WCLR | DSKBLK);
	last_msg |= DMA_READY;
	interrupt(FLOPPY);
  }
}

PUBLIC void index_int()
{
#ifdef NEED_INDEX
/* An index-hole has just been seen; start DMA and signal FLOPPY. */

  *DSKLEN = *DSKLEN = dsklen_val;	/* start DMA */
  last_msg |= INDEX_FOUND;
  interrupt(FLOPPY);
#else
  panic("Unexpected index_int\n");
#endif /* NEED_INDEX */
}

PRIVATE void clock_mess(ticks, func)
int ticks;			/* how many clock ticks to wait */
int (*func)();			/* function to call upon time out */
{
/* send the clock task a message. */
  static message mess;

  mess.m_type = SET_ALARM;
  mess.CLOCK_PROC_NR = FLOPPY;
  mess.DELTA_TICKS = ticks;
  mess.FUNC_TO_CALL = (void (*)())func;
  sendrec(CLOCK, &mess);
}

PUBLIC void release_int()
{
/*  This routine is called whenever something times out that wasn't
 *  supposed to do so. (Disk-DMA for example.)
 */
#ifdef DEBUG
  printf("Floppy:timed out.\n");
#endif
  last_msg |= TIMED_OUT;
  interrupt(FLOPPY);
}

PRIVATE void my_receive(task, mask)
int task;
unsigned int mask;
{
  static message dummy_mess;

  last_msg &= ~mask;
  do {
	receive(task, &dummy_mess);
  } while (!(last_msg & mask));
  if (last_msg & DO_FLUSH) {
	to_flush = 1;
	last_msg &= ~DO_FLUSH;
  }
}

PRIVATE void dperror(dp, a,b,c,d,e)
d_ptr dp;
long a,b,c,d,e;
{
/* Usage: dperror (dp, "CRC-error. was %04x, should be %04x\n", x, y);
 */
  printf("Floppy(dr:%d,cyl:%d,sd:%d): ",
	dp->num, dp->cyl, dp->side);
  printf(a,b,c,d,e);
}
#endif
