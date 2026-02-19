#if (MACHINE == AMIGA)
/*
 * The keyboard driver for the Amiga
 */

#include "kernel.h"
#include <minix/com.h>
#include <sgtty.h>
#include "amhardware.h"
#include <minix/amtransfer.h>
#include "tty.h"

#define THRESHOLD                 20	/* # chars to accumulate before msg */
#undef  BYTE
typedef unsigned char BYTE;


EXTERN	int     shift1, shift2, capslock, control;
PRIVATE	int	alt1, alt2;
                                /* keep track of key status */
PRIVATE char    kbdbuf[2*MAX_OVERRUN+2];
                                /* driver collects chars here */
PRIVATE message kbdmes;         /* message used for console input chars */
PRIVATE BYTE    repeatkey;      /* the last raw keycode */
PRIVATE char    asciikey[80];   /* the ascii translation of the last key */
PRIVATE int     repeattic;      /* time to next repeat */
PRIVATE int     repeat_speed;   /* initial repeattic */
PRIVATE struct minixkeymap kmap; /* the minix keymap */
#define SP_SEND 0
#define SP_REC  1
PRIVATE int     sp_sr=SP_REC;   /* am i receiving from or sending to keyboard ? */

/*===========================================================================*
 *				kbdint					     *
 *===========================================================================*/
PUBLIC void kbdint()
{
  register short code, make, k;
  register int s;

  if (sp_sr == SP_SEND) { /* sending to (handshaking with) keyboard ? */
      sp_sr = SP_REC;     /* back to receiving */
      *CRAA = 0x80;
      return;
  }
  sp_sr = SP_SEND;        /* going to send in a 'minute' */

  s = lock();
  k = tty_buf_count(tty_driver_buf);

  code = *SDRA;
  *CRAA = 0xD1;
  *SDRA = 0x00;         /* shake hands */

  /*
   * The keyboard interrupts twice per key,
   * once when depressed, once when released.
   * Filter out the latter, ignoring all but
   * the shift-type keys.
   */
  make = code & 1;     /* 1=depressed, 0=released */
  code = ~(code >> 1) & 0x7F;
  switch (code) {
  case 0x60:      /* shift key on left */
          shift1 = make; break;
  case 0x61:      /* shift key on right */
          shift2 = make; break;
  case 0x63:      /* control */
          control = make; break;
  case 0x64:      /* left alt key */
          alt1 = make; break;
  case 0x65:      /* right alt key */
          alt2 = make; break;
  case 0x62:      /* caps lock */
          capslock = make; break;
  default:        /* normal key */
          if (make == 0)
                  repeattic = 0;
          else {
                  repeatkey = code;
                  repeattic = 18;  /* delay: 18 / HZ == 0.3 sec */
                  kbdkey(code);
          }
  }

  if (tty_buf_count(tty_driver_buf) < THRESHOLD) {
	/* Don't send message.  Just accumulate.  Let clock do it. */
	INT_CTL_ENABLE;
	flush_flag++;
  } else rs_flush();			/* send TTY task a message */
  restore(s);
}


/*==========================================================================*
 *                             kbdkey                                       *
 *==========================================================================*/
PRIVATE kbdkey(code)
register BYTE code;
{
  register int index; /* in the ascii key block of kmap */
  register BYTE c;
  index=code;
  if (shift1 || shift2 || capslock && /* code is a-z */
   (code>=0x10&&code<=0x19 || code>=0x20&&code<=0x28 || code>=0x31&&code<=0x37))
      index+=MAXKEY;
  if (alt1 || alt2)
      index+=MAXKEY<<1;
  c = kmap.ascii[index];
  if (c) {
      if (control)
          c &= 0x1f;
      /* Check to see if character is XOFF, to stop output. */
      if ((tty_struct[CONSOLE].tty_mode & (RAW|CBREAK))==0 &&
       tty_struct[CONSOLE].tty_xoff==c) {
          tty_struct[CONSOLE].tty_inhibited=STOPPED;
          return;
      }
      asciikey[0]=c;
      asciikey[1]='\0';
      kbdput(asciikey, CONSOLE);
      return;
  }
  if (control && (alt1 || alt2) && code>=0x50 && code<=0x59) {
      /*
       * ctrl & alt & function-key generates a special debugging sequence
       */
      asciikey[0]=code-0x50+1;
      asciikey[1]='\0';
      kbdput(asciikey, OPERATOR);
      return;
  }
  if (code>=0x50 && code<=0x59) { /* function key pressed */
      index=code-0x50;
      if (shift1 || shift2)
          index+=10;
      strcpy(asciikey, (long) kmap.func[index]+ (long) kmap.functext);
      kbdput(asciikey, CONSOLE);
      return;
  }
  switch (code) {
  case 0x5f:    /* Home/Clr (well, eh... actually help) */
      kbdansi('H');
      if (shift1 || shift2)
          kbdansi('J');
      return;
  case 0x4c:    /* Up */
      kbdansi('A'); return;
  case 0x4f:    /* Left */
      kbdansi('D'); return;
  case 0x4e:    /* Right */
      kbdansi('C'); return;
  case 0x4d:    /* Down */
      kbdansi('B'); return;
  default:
      return;
  }
}


/*
 * Store the character(s) in memory
 */
PUBLIC void kbdput(str, line)
char *str;
int line;
{
  register int k;
  register BYTE c;

  /* Store the character in memory so the task can get at it later.
   * tty_driver_buf[0] is the current count, and tty_driver_buf[2] is the
   * maximum allowed to be stored.
   */
  c= *str;
  do {
      if ( (k = tty_buf_count(tty_driver_buf)) >= tty_buf_max(tty_driver_buf)) 
          /*
           * Too many characters have been buffered.
           * Discard excess.
           */
          return;
      /* There is room to store this character; do it. */
      k <<= 1;				/* each entry uses two bytes */
      tty_driver_buf[k+4] = c;		/* store the char code */
      tty_driver_buf[k+5] = line;	/* which line it came from */
      tty_buf_count(tty_driver_buf)++;	/* increment counter */
  } while (c= *(++str));
}


/*
 * Input ANSI escape sequence
 */
PRIVATE kbdansi(c)
{
    asciikey[0]='\033';
    asciikey[1]='[';
    asciikey[2]=c;
    asciikey[3]='\0';
    kbdput(asciikey, CONSOLE);
}

/*===========================================================================*
 *				kb_timer				     *
 *===========================================================================*/
PUBLIC void kb_timer()
{
  register int k, s;

  s = lock();
  if (repeattic == 0) {
	restore(s);
	return;
  }
  if (--repeattic != 0) {
	restore(s);
	return;
  }
  k = tty_buf_count(tty_driver_buf);
  kbdkey(repeatkey);
  if (k != tty_buf_count(tty_driver_buf))
  {
    if (tty_buf_count(tty_driver_buf) < THRESHOLD) {
	/* Don't send message.  Just accumulate.  Let clock do it. */
	INT_CTL_ENABLE;
	flush_flag++;
    }
    else rs_flush();			/* send TTY task a message */
  }
  repeattic = repeat_speed;	        /* HZ/repeat_speed keys per second */
  restore(s);
}

/*===========================================================================*
 *				kbdinit				     	     *
 *===========================================================================*/
PUBLIC void kbdinit()
{
  /* initialize keyboard */
  struct transferdata *transdat;

  transdat= *(struct transferdata **)0x0000;
  repeat_speed = HZ/(transdat->args['q'-'a']);
  kmap=transdat->mkeymap;     /* save the transfer keymap            */
  enable_int(PORTS);
  enable_ciaa_int(ICRSP);
  *TALOA=0x08;                /* set tmraa for handshaking with keyb */
  *TAHIA=0x00;                /* to 8; (8+1)*8*2/7.09e5Hz=203 mcrsec */
  *CRAA=0x80;                 /* 50,spin,02,-,cont,pls,norm,stop     */
}
#endif
