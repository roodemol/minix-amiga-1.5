#if (MACHINE == AMIGA)
/*
 * The video driver for the Amiga
 *
 * The main difference with the st version is that I dropped color and
 * attributes and thereby gained considerable simplicity, the reason for
 * this is that it was not portable between the st and pc versions,
 * if I had implemented it in the amiga version there would have been another
 * version with another set of graphics primitives.
 * The routines for color graphics on the atari and amiga would NOT have
 * been the same, the amiga uses bitplanes, the atari uses 2 consecutive
 * bits per pixel (4 pixels per byte of video ram)
 *
 * notes:
 * this driver assumes memory for videoram at about $07c180 - $07ffff
 *
 * This driver does not deal with multiple consoles and parameter passing
 * through tp. Also the tty struct fields row and column are not maintained
 */

#include "kernel.h"
#include <sgtty.h>
#include "amhardware.h"
#include <minix/amtransfer.h>
#include "tty.h"

#define RGB_BLACK	0x0000
#define RGB_RED		0x0f00
#define RGB_GREEN	0x00f0
#define RGB_YELLOW	0x0ff0
#define RGB_BLUE	0x000f
#define RGB_MAGENTA	0x0f0f
#define RGB_CYAN	0x00ff
#define RGB_WHITE	0x0fff
#define RGB_LGREY	0x0ccc
#define RGB_DGREY	0x0444

#define VIDEORAM        16000L  /* size of video ram */
#define NCOL            80      /* characters on a row */
#define PIX_LIN         640     /* pixels per video line */
#define PIX_CHR         8       /* pixels in char (width) */
#define LIN_SCR         200     /* video lines on screen */
#define BYT_LIN         80      /* bytes in video line */
#define NROW            25      /* 25 lines of text onscreen */
#define BYTR            640     /* bytes per text line */
u_long  VRAM=0;                 /* start of video ram */
#define FONTH           8       /* 8 videolines per text line */

PRIVATE struct vduinfo {
        ptr_t	curs;           /* cursor position in video RAM */
        int	ccol;           /* current char column */
        int     crow;           /* current char row */
        char    attr;           /* current attribute byte */
        char    savattr;        /* saved attribute byte */
        int     savccol;        /* saved char column */
        int     savcrow;        /* saved char row */
        char    vbuf[20];       /* partial escape sequence */
        char    *next;          /* next char in vbuf[] */
} vduinfo;
PRIVATE ptr_t fontbase;           /* start of fontdata in rom */
PRIVATE long debuglvl;          /* debugging level you told to the loader */
PRIVATE u_long spritemem[SPRITESZ+1]; /* cursorsprite data */
PRIVATE u_long nospritemem[2];        /* data for other sprites */
PRIVATE u_long coplistmem[32];        /* copperlist */
PUBLIC  int  clipright;         /* boolean, clip text at right edge, see moveto */


/*===========================================================================*
 *				flush					     *
 *===========================================================================*/
EXTERN void flush(tp)
register struct tty_struct *tp;
{
  register char *rq;

  if (tp->tty_rwords == 0)
	return;
  rq = (char *)tp->tty_ramqueue;
  do {
	if (tp->tty_inhibited == TRUE) {
		vducursor();
		return;
	}
	out_char(tp, *rq++);	/* write 1 byte to terminal */
	tp->tty_phys++;		/* advance physical data pointer */
	tp->tty_cum++;		/* number of characters printed */
  } while (--tp->tty_rwords != 0);
  vducursor();
  return;
}

/*===========================================================================*
 *				console					     *
 *===========================================================================*/
PRIVATE console(tp)
register struct tty_struct *tp;	/* tells which terminal is to be used */
{
/* Copy as much data as possible to the output queue, then start I/O.  On
 * memory-mapped terminals, such as the IBM console, the I/O will also be
 * finished, and the counts updated.  Keep repeating until all I/O done.
 */

  int count = 0;
  char c;
#if (CHIP == M68000)
  char *charptr = (char *)tp->tty_phys;
#else
  extern char get_byte();
  unsigned segment, offset, offset1;

  /* Loop over the user bytes one at a time, outputting each one. */
  segment = (tp->tty_phys >> 4) & WORD_MASK;
  offset = tp->tty_phys & OFF_MASK;
  offset1 = offset;
#endif

  while (tp->tty_outleft > 0 && tp->tty_inhibited == RUNNING) {
#if (CHIP == M68000)
	out_char(tp, *charptr++);	/* write 1 byte to terminal */
	count++;
#else
	c = get_byte(segment, offset);	/* fetch 1 byte from user space */
	out_char(tp, c);	/* write 1 byte to terminal */
	offset++;		/* advance one character in user buffer */
#endif
	tp->tty_outleft--;	/* decrement count */
  }
#if (CHIP == M68000)
  vducursor();
#endif
  flush(tp);			/* clear out the pending characters */

  /* Update terminal data structure. */
#if (CHIP != M68000)
  count = offset - offset1;	/* # characters printed */
#endif
  tp->tty_phys += count;	/* advance physical data pointer */
  tp->tty_cum += count;		/* number of characters printed */

  /* If all data has been copied to the terminal, send the reply. */
  if (tp->tty_outleft == 0) finish(tp, tp->tty_cum);
}

/*===========================================================================*
 *				out_char				     *
 *===========================================================================*/
/*
 * send character to VDU, collecting escape sequences
 */
PUBLIC void out_char(tp, c)
struct tty_struct *tp;	/* not yet used */
register char c;		/* character to be output */
{
  register struct vduinfo *v = &vduinfo;

  if (c == 0x7F)
	return;
  if ((c & 0340) == 0) {
	/*
	 * control character
	 */
	vductrl(c);
	return;
  }
  if (v->next == 0) {
	/*
	 * normal character
	 */
	paint(c);
	moveto(v->crow, v->ccol + 1);
	return;
  }
  if (v->next == v->vbuf && c == '[') {
	/*
	 * start of ANSI sequence
	 */
	*v->next++ = (char)c;
	return;
  }
  if (c >= 060 && (v->next == v->vbuf || v->vbuf[0] != '[')) {
	/*
	 * end of non-ANSI escape sequence
	 */
	vduesc(c);
	v->next = 0;
	return;
  }
  if (c >= 0100) {
	/*
	 * end of ANSI sequence
	 */
	vduansi(c);
	v->next = 0;
	return;
  }
  /*
   * part of escape sequence
   */
  *v->next = (char)c;
  if (v->next < &v->vbuf[sizeof(v->vbuf)])
	v->next++;
}

/*
 * control character
 */
PRIVATE vductrl(c)
{
  register struct vduinfo *v = &vduinfo;
  register i;
  register struct tty_struct *tp = &tty_struct[CONSOLE];
  
  switch (c) {
  case 007: /* BEL */
	beepon(); return;
  case 010: /* BS */
	moveto(v->crow, v->ccol - 1); return;
  case 011: /* HT */
	do
		if ((tp->tty_mode & XTABS) == XTABS)
			out_char(tp, ' ');
		else
			moveto(v->crow, v->ccol + 1);
	while (v->ccol & TAB_MASK);
	return;
  case 012: /* LF */
	if (tp->tty_mode & CRMOD)
		out_char(tp, '\r');
  case 013: /* VT */
  case 014: /* FF */
        while ( !(*PRAA & (1<<FIR0)));
                                /* Left mouse button disables scrolling */
        if (v->crow == NROW - 1) {
               /*for (i = 0; i < NROW-1; i++)
                        cpyline(i + 1, i);*/
                long_copy((long *)(VRAM+BYTR), (long *)VRAM,
                 (u_long)((NROW-1)*BYTR/4));
                clrline(NROW-1);
        } else
                moveto(v->crow + 1, v->ccol);
        return;
  case 015: /* CR */
	moveto(v->crow, 0);
	return;
  case 030: /* CAN */
  case 032: /* SUB */
	v->next = 0;
	/* no error char */
	return;
  case 033: /* ESC */
	v->next = v->vbuf;
	return;
  default:
	return;
  }
}

/*
 * execute ANSI escape sequence
 */
PRIVATE vduansi(c)
{
  register struct vduinfo *v = &vduinfo;
  register i;
  register j;

  if (v->next >= &v->vbuf[sizeof(v->vbuf)])
	return;
  *v->next = 0;
  v->next = &v->vbuf[1];
  j = vduparam();
  if ((i = j) <= 0)
	i = 1;
  switch (c) {
  case 'A': /* CUU: cursor up */
	if ((i = v->crow - i) < 0)
		i = 0;
	moveto(i, v->ccol);
	return;
  case 'B': /* CUD: cursor down */
	if ((i += v->crow) >= NROW)
		i = NROW-1;
	moveto(i, v->ccol);
	return;
  case 'C': /* CUF: cursor forward */
	if ((i += v->ccol) >= NCOL)
		i = NCOL-1;
	moveto(v->crow, i);
	return;
  case 'D': /* CUB: cursor backward */
	if ((i = v->ccol - i) < 0)
		i = 0;
	moveto(v->crow, i);
	return;
  case 'H': /* CUP: cursor position */
  case 'f': /* HVP: horizontal and vertical position */
	j = vduparam();
	if (j <= 0)
		j = 1;
	if (i > NROW)
		i = NROW;
	if (j > NCOL)
		j = NCOL;
	moveto(i - 1, j - 1);
	return;
  case 'J': /* ED: erase in display */
	if (j <= 0)
		clrarea(v->crow, v->ccol, NROW-1, NCOL-1);
	else if (j == 1)
		clrarea(0, 0, v->crow, v->ccol);
	else if (j == 2)
		clrarea(0, 0, NROW-1, NCOL-1);
	return;
  case 'K': /* EL: erase in line */
	if (j <= 0)
		clrarea(v->crow, v->ccol, v->crow, NCOL-1);
	else if (j == 1)
		clrarea(v->crow, 0, v->crow, v->ccol);
	else if (j == 2)
		clrarea(v->crow, 0, v->crow, NCOL-1);
	return;
#if (CHIP != M68000)
#ifdef NEEDED
  case 'n': /* DSR: device status report */
	if (i == 6)
		kbdinput("\033[%d;%dR", v->crow+1, v->ccol+1);
	return;
#endif /* NEEDED */
#endif
  case 'm': /* SGR: set graphic rendition */
	do {
		static colors[] = {
			RGB_BLACK,
			RGB_RED,
			RGB_GREEN,
			RGB_YELLOW,
			RGB_BLUE,
			RGB_MAGENTA,
			RGB_CYAN,
			RGB_WHITE,
			RGB_LGREY,
			RGB_DGREY,
		};

		if (j <= 0)
			v->attr &= ~3;
		else if (j == 4 || j == 7)
			v->attr |= 3;
		else if (j >= 30 && j <= 39)
			*COLOR01 = colors[j - 30];
		else if (j >= 40 && j <= 49)
			*COLOR00 = colors[j - 40];
	} while ((j = vduparam()) >= 0);
	return;
  case 'L': /* IL: insert line */
	if (i > NROW - v->crow)
		i = NROW - v->crow;
	for (j = NROW-1; j >= v->crow+i; j--)
		cpyline(j - i, j);
	while (--i >= 0)
		clrline(j--);
	return;
  case 'M': /* DL: delete line */
	if (i > NROW - v->crow)
		i = NROW - v->crow;
	for (j = v->crow; j < NROW-i; j++)
		cpyline(j + i, j);
	while (--i >= 0)
		clrline(j++);
	return;
  case '@': /* ICH: insert char */
	j = NCOL - v->ccol;
	if (i > j)
		i = j;
	j -= i;
	while (--j >= 0)
		cpychar(v->crow, v->ccol + j, v->crow, v->ccol + j + i);
	clrarea(v->crow, v->ccol, v->crow, v->ccol + i - 1);
	return;
  case 'P': /* DCH: delete char */
	j = NCOL - v->ccol;
	if (i > j)
		i = j;
	j -= i;
	while (--j >= 0)
		cpychar(v->crow, NCOL-1 - j, v->crow, NCOL-1 - j - i);
	clrarea(v->crow, NCOL - i, v->crow, NCOL-1);
	return;
  case 'l': /* RM: reset mode */
  case 'h': /* SM: set mode */
	if (v->next[0] == '?') { /* DEC private modes */
		if (v->next[1] == '5') /* DECSCNM */
			*COLOR00 = c == 'l' ? RGB_BLACK : RGB_WHITE;
		else if (v->next[1] == '1')	  /* DECCKM */
			/* app_mode = c == 'l' ? TRUE : FALSE */ ;
	}
	return;
  case '~': /* Minix-ST specific escape sequence, here for compatibility */
	/*
	 * Handle the following escape sequence:
	 *   ESC [ l;a;m;r;g;b '~'
	 * where
	 *   if l present:    (ignored in amiga-minix)
	 *	0: 25 lines if mono
	 *	1: 50 lines if mono
	 *   if a present:
	 *	low 4 bits are attribute byte value (see paint())
	 *   if m present:
	 *	interpret r;g;b as colors for map register m
	 *	only assign color if r, g or b present
	 */
	j = vduparam();
	if (j >= 0)
		v->attr = j & 0x0F;
	j = vduparam();
	if (j >= 0) {
                register unsigned short *w;
                register unsigned short tmp_color;

                w = COLOR00 + (j & 0x1f);
                j = vduparam();
                if (j >= 0)
                        tmp_color = (j & 0xf) << 8;
                j = vduparam();
                if (j >= 0)
                        tmp_color |= (j & 0xf) << 4;
                j = vduparam();
                if (j >= 0)
                        tmp_color |= j & 0xf;
                *w = tmp_color;
	}
	return;
  default:
	return;
  }
}

/*
 * execute non-ANSI escape sequence
 */
PRIVATE vduesc(c)
{
  register struct vduinfo *v = &vduinfo;
  register i;

  if (v->next >= &v->vbuf[sizeof(v->vbuf)-1])
	return;
  *v->next = (char)c;
  switch (v->vbuf[0]) {
  case '8': /* DECRC: restore cursor */
	v->ccol = v->savccol;
	v->crow = v->savcrow;
	v->attr = v->savattr;
	moveto(v->crow, v->ccol);
	return;
  case '7': /* DECSC: save cursor */
	v->savccol = v->ccol;
	v->savcrow = v->crow;
	v->savattr = v->attr;
	return;
  case '=': /* DECKPAM: keypad application mode */ 
	/* keypad = TRUE; */
	return;
  case '>': /* DECKPNM: keypad numeric mode */ 
	/* keypad = FALSE; */
	return;
  case 'E': /* NEL: next line */
	vductrl(015);
	/* fall through */
  case 'D': /* IND: index */
	vductrl(012);
	return;
  case 'M': /* RI: reverse index */
	if (v->crow == 0) {
		for (i = NROW - 1; i > 0; i--)
			cpyline(i - 1, i);
		clrline(i);
	} else
		moveto(v->crow - 1, v->ccol);
	return;
  case 'c': /* RIS: reset to initial state */
	vduinit();
	/* keypad = FALSE; */
	/* app_mode = FALSE; */
	return;
  default:
	return;
  }
}

/*
 * compute next parameter out of ANSI sequence
 */
PRIVATE vduparam()
{
  register struct vduinfo *v = &vduinfo;
  register c;
  register i;

  i = -1;
  c = *v->next++;
  if (c >= '0' && c <= '9') {
	i = 0;
	do {
		i *= 10;
		i += (c - '0');
		c = *v->next++;
	} while (c >= '0' && c <= '9');
  }
  if (c != ';')
	v->next--;
  return(i);
}


/*===========================================================================*
 *                              manipulate videoram                          *
 *===========================================================================*/
/*
 * copy line r1 to r2
 */
PRIVATE cpyline(r1, r2)
int r1, r2;
{
  register u_long *src;
  register u_long *dst;
  register u_long i;

  src = (u_long*)(VRAM+r1 * BYTR);
  dst = (u_long*)(VRAM+r2 * BYTR);
  i = BYTR >> 2; 
  long_copy(src, dst, i);
}

/*
 * copy char (r1,c1) to (r2,c2)
 */
PRIVATE cpychar(r1, c1, r2, c2)
{
  register char *src;           /* ptr into video memory */
  register char *dst;           /* ptr into video memory */
  register nl;                  /* scan lines per char */
  register bl;                  /* bytes per scan line */

  bl = BYT_LIN;
  src = (char *)(VRAM+r1 * BYTR + c1);
  dst = (char *)(VRAM+r2 * BYTR + c2);
  nl = FONTH;

  do {
        *dst = *src;
        src += bl;
        dst += bl;
  } while (--nl != 0);
}

/*
 * clear part of screen between two points inclusive
 */
PRIVATE clrarea(r1, c1, r2, c2)
{
  if (++c2 == NCOL) {
        c2 = 0;
        r2++;
  }
  if (c1 > 0 && r1 < r2) {
        do
                clrchar(r1, c1);
        while (++c1 < NCOL);
        c1 = 0;
        r1++;
  }
  while (r1 < r2)
        clrline(r1++);
  while (c1 < c2)
        clrchar(r1, c1++);
}

/*
 * clear line r
 */
PRIVATE clrline(r)
{
  register long *src;
  register long i;

  src = (long *)(VRAM + r*BYTR);
  i = BYTR >> 2; 
  long_clear(src, i);
 }

/*
 * clear char (r,c)
 */
PRIVATE clrchar(r, c)
{
  register char *p;             /* ptr into video memory */
  register nl;                  /* scan lines per char */
  register bl;                  /* bytes per scan line */

  bl = BYT_LIN;
  p = (char *)(VRAM+r * BYTR + c);
  nl = FONTH;
  do {
        *p = 0;
        p += bl;
  } while (--nl != 0);
}

/*===========================================================================*
 *                              moveto                                       *
 *===========================================================================*/
PRIVATE moveto(r, c)
{
  register struct vduinfo *v = &vduinfo;

  if (clipright) { /* the original ST routine, kept for compatibility */
	if (r < 0 || r >= NROW || c < 0 || c > NCOL)
        	return;
  	v->crow = r;
  	v->ccol = c;
  	if (c == NCOL)
        	c--;                    /* show cursor in last column */
  	v->curs = (ptr_t) (VRAM + r * BYTR+ c);
  } else { /* doesn't clip, continues on next line */
	if (r < 0 || c < 0)
		return;  		
	while (c >= NCOL)
		r++, c -= NCOL;
	while (r >= NROW) {
  		long_copy((long *)(VRAM+BYTR), (long *)VRAM, 
		 (u_long)((NROW-1)*BYTR/4));
                clrline(NROW-1);
		r--;
	}
	v->crow = r;
	v->ccol = c;
	v->curs = (ptr_t)(VRAM + r * BYTR + c);
  }
}

/*===========================================================================*
 *                              beep                                         *
 *===========================================================================*/
PRIVATE short beepcount; /* used to count beep length */

/* called by pressing ^g, turns beeping on */
PUBLIC beepon() {
    *DMACON=WSET|AUD0EN; /* going to beep */
    beepcount=15;         /* 15 / HZ == 0.25 sec */
}


/* called by clock task every tick, checks to see if beeping should
 * be turned off, BEEP-HACK
 */
PUBLIC beepoff() {
    if (!beepcount)
        return;
    --beepcount;
    if (!beepcount)
        *DMACON=WCLR|AUD0EN; /* if counter now zero stop sound */
}


PUBLIC beepinit() {
    static long beepdata=0x7f807f80; /* 127, -128, 127, -128 controls speaker */

    *AUD0VOL = 64; /* volume to maximum */
    *AUD0PER = 1100; /* frequency, 124=max height */
    *AUD0LC = (ptr_t) &beepdata; /* point audiochannel to data */
    *AUD0LEN = 2; /* data length in words */
}


/*===========================================================================*
 *                              paint                                        *
 * attributes:                                                               *
 *   0000xxx1: invert plane 0                                                *
 *===========================================================================*/
/*
 * copy from font memory into video memory
 */
PRIVATE paint(c)
int c;
{
  register char *vp;            /* ptr into video memory */
  register char *fp;            /* ptr into font */
  register int fonth;           /* bytes per char */
  register int dscreen;         /* bytes to skip in screendata */
  register int dfont;           /* bytes to skip in fontdata */

  fp = (char *) ((u_long) fontbase + c - 32); /* data starts with ascii code 32 */
  dscreen = BYT_LIN;
  dfont = 0xc0;
  fonth = FONTH;
  vp = (char *)(vduinfo.curs);
  if (vduinfo.attr & 1) /* reverse-video */
      do {
          *vp = ~(*fp);
          vp += dscreen;
          fp += dfont;
      } while (--fonth);
  else                  /* normal */
      do {
          *vp = *fp;
          vp += dscreen;
          fp += dfont;
      } while (--fonth);
}

/*===========================================================================*
 *                              vducursor                                    *
 *===========================================================================*/
/* updates cursor sprite position to vduinfo->ccol, vduinfo->crow */
PUBLIC void vducursor() {
    register u_long spr0all;      /* will be spr0pos and spr0ctl together*/
    register u_long vstart, vstop, hstart;

    spr0all = 0;
    hstart = ((vduinfo.ccol)<<2)+0x7a;
    vstart = ((vduinfo.crow)<<3)+0x28;
    vstop = vstart+SPRITESZ;
    spr0all |= (vstart&0xff)<<24;
    spr0all |= (hstart&0x1fe)<<15;
    spr0all |= (vstop&0xff)<<8;
    if (vstart&0x100) spr0all |= 4;
    if (vstop&0x100) spr0all |= 2;
    spr0all |= hstart&1;
    *(u_long *)spritemem = spr0all;
}

/*===========================================================================*
 *                              vduinit                                      *
 *===========================================================================*/

PUBLIC void vduinit()
{
  register struct vduinfo       *v = &vduinfo;
  struct transferdata *transdat;
  u_short *sys_alloc();

  if (VRAM == 0) {
      transdat = *(struct transferdata **)0x0000; /* where the loader left it */
      fontbase = (ptr_t) transdat->args['f'-'a'];
      VRAM = (u_long)sys_alloc(VIDEORAM);
      debuglvl = transdat->args['d'-'a'];
      initvducursor(transdat);    /* create sprite                   */
  }
  *BPLCON0=HIRES;             /* compcolor on,1 bitplane,HIRES       */
  *BPLCON1=0;                 /* hor scroll offset to 0              */
  *BPLCON2=0x0000;            /* playfields in front of sprites      */
  *BPL1MOD=0;                 /* odd bitplane mod to 0 (no interlace)*/
  *DDFSTRT=DDFSTRTH;          /* set them magic values for HIRES     */
  *DDFSTOP=DDFSTOPH;
  *DIWSTRT=DIWSTRTH;
  *DIWSTOP=DIWSTOPH;
  initcopper();               /* create copperlist                   */
  tty_struct[CONSOLE].tty_devstart=console;
  moveto(0, 0);               /* move cursor to upper left corner */
  clipright = 1;
  clrarea(0, 0, NROW-1, NCOL-1);
  v->attr = 0;                /* clear the attribute byte */
  v->next = 0;
  vducursor();                /* init the cursor */
  beepinit();                 /* init the sound stuff */
  *DMACON=WSET|BPLEN;         /* enable bitplane dma */
  *DMACON=WSET|COPEN;         /* enable copper dma */
  *DMACON=WSET|SPREN;         /* enable sprite dma */
}

/* initvducursor - puts the data for the sprites in the right places    */
initvducursor(transdat)
struct transferdata *transdat; {
    int cnt;
    u_long *src, *dest;

    src=(u_long *)transdat->spritedata;
    dest=spritemem;
    cnt=SPRITESZ+1;             /* position words take an extra u_long   */
    while(cnt--)
        *(dest++) = *(src++);
    dest=nospritemem;
    cnt=2;
    while(cnt--)
        *(dest++)=0;            /* this makes sure they won't show up  */
}

/* puts the copperlist at coplistmem (doesn't that sound simple !)     */
/* for those who don't have the faintest idea what a copper(list) is:  */
/* it is a coprocessor that runs alongside the mc68k, fetching its own */
/* instructions from a program, the copperlist, by using dma.          */
/* it performs tasks that have to be done screen-synchronized every    */
/* time the beam reaches a certain spot on the crt, when a vblank      */
/* occurs it automagically jumps to the start of the copperlist        */
initcopper() {
    u_short *p;

    p=(u_short *)coplistmem;
    *(p++)=0x0120; *(p++)=(u_long)spritemem/0x10000;   /* these instructions */
    *(p++)=0x0122; *(p++)=(u_long)spritemem%0x10000;   /* reset the spritepointers */
    *(p++)=0x0124; *(p++)=(u_long)nospritemem/0x10000; /* to the right spot */
    *(p++)=0x0126; *(p++)=(u_long)nospritemem%0x10000; /* 120,122 are sprite 0 */
    *(p++)=0x0128; *(p++)=(u_long)nospritemem/0x10000;
    *(p++)=0x012a; *(p++)=(u_long)nospritemem%0x10000;
    *(p++)=0x012c; *(p++)=(u_long)nospritemem/0x10000;
    *(p++)=0x012e; *(p++)=(u_long)nospritemem%0x10000;
    *(p++)=0x0130; *(p++)=(u_long)nospritemem/0x10000;
    *(p++)=0x0132; *(p++)=(u_long)nospritemem%0x10000;
    *(p++)=0x0134; *(p++)=(u_long)nospritemem/0x10000;
    *(p++)=0x0136; *(p++)=(u_long)nospritemem%0x10000;
    *(p++)=0x0138; *(p++)=(u_long)nospritemem/0x10000;
    *(p++)=0x013a; *(p++)=(u_long)nospritemem%0x10000;
    *(p++)=0x013c; *(p++)=(u_long)nospritemem/0x10000;
    *(p++)=0x013e; *(p++)=(u_long)nospritemem%0x10000; /* 13c,13e are sprite 7 */
    *(p++)=0x00e0; *(p++)=(u_long)VRAM/0x10000; /* these reset the screen pointer */
    *(p++)=0x00e2; *(p++)=(u_long)VRAM%0x10000;
    *(p++)=0xffff; *(p++)=0xfffe; /* this says: wait till forever (next vblank) */

    *COP1LC=(ptr_t)coplistmem;      /* switching copper to new instructions */
    *COPJMP1=0;                 /* force (strobe) copper to COP1LC     */
}


vdu_put(ch, col)		/* HACK */
char ch;
u_short col;
{
  if (col)
	*COLOR00 = col;
  out_char( &tty_struct[CONSOLE], ch);
  vducursor();
}

#endif /* (MACHINE == AMIGA) */
