/* amhardware.h - defines all constants specific to the Amiga hardware
 * (types, autovectors, custom chips and cia registers)
 *
 * Assumptions:
 * vectors      at 0x000000
 * ciab         at 0xbfd000
 * ciaa         at 0xbfe001
 * custom chips at 0xdff000
 */

typedef unsigned long u_long;
typedef unsigned short u_short;
typedef unsigned char u_char;
typedef void *ptr_t;

/* vectors */

#define RESETSP (ptr_t *) 0x000000  /* reset: begin ssp                      */
#define RESETPC (ptr_t *) 0x000004  /* reset: begin pc                       */
#define BUSTRAP (ptr_t *) 0x000008  /* trap: buserror (nonexist mem)         */
#define ADRTRAP (ptr_t *) 0x00000c  /* trap: addresserror (odd WORD/LONG adr)*/
#define ILOPTRP (ptr_t *) 0x000010  /* trap: illegal opcode                  */
#define DIV0TRP (ptr_t *) 0x000014  /* trap: division by zero                */
#define CHKTRAP (ptr_t *) 0x000018  /* trap: chk error trapped               */
#define TRAPVTR (ptr_t *) 0x00001c  /* trap: trapv overflow trapped          */
#define PRIVITR (ptr_t *) 0x000020  /* trap: priority violation              */
#define TRACETR (ptr_t *) 0x000024  /* trap: trace trapped                   */
#define ILOPATR (ptr_t *) 0x000028  /* trap: unimplemented A (?) opcode      */
#define ILOPFTR (ptr_t *) 0x000030  /* trap: unimplemented F (?) opcode      */
#define ILLEGAL (ptr_t *) 0x000060  /* interrupt: illegal interrupt          */
#define AV1INT  (ptr_t *) 0x000064  /* interrupt: level 1 autovector         */
#define AV2INT  (ptr_t *) 0x000068  /* interrupt: level 2 autovector         */
#define AV3INT  (ptr_t *) 0x00006c  /* interrupt: level 3 autovector         */
#define AV4INT  (ptr_t *) 0x000070  /* interrupt: level 4 autovector         */
#define AV5INT  (ptr_t *) 0x000074  /* interrupt: level 5 autovector         */
#define AV6INT  (ptr_t *) 0x000078  /* interrupt: level 6 autovector         */
#define AV7INT  (ptr_t *) 0x00007c  /* interrupt: level 7 autovector (nmi)   */

/* CIAs: */

#define CRAA    (u_char *)0xbfee01  /* rw timer control register aA          */
#define CRAB    (u_char *)0xbfde00  /* rw timer control register aB          */
#define CRBA    (u_char *)0xbfef01  /* rw timer control register bA          */
#define CRBB    (u_char *)0xbfdf00  /* rw timer control register bB          */
#define CRRUN   (u_char)  0         /* start=1/stop=0 bit in timer cr's      */
#define CRSP    (u_char)  6         /* sp out=1/in=0 bit in timera cr's      */
#define DDRBA   (u_char *)0xbfe301  /* data direction reg. PRBA              */
#define DDRAB   (u_char *)0xbfd200  /* data direction reg. PRAB              */
#define ICRA    (u_char *)0xbfed01  /* rW interrupt control register A       */
#define ICRB    (u_char *)0xbfdd00  /* rW interrupt control register B       */
#define ICRTA   (u_char)  0x01      /* timer a in icr                        */
#define ICRTB   (u_char)  0x02      /* timer b in icr                        */
#define ICRALRM (u_char)  0x04      /* alarm in icr                          */
#define ICRSP   (u_char)  0x08      /* serial port in icr                    */
#define ICRF	(u_char)  0x10	  /* parallel port in icr		   */
#define TODHA	(u_char *)0xbfea01  /* TOD-HI, ciaA			   */
#define TODHB	(u_char *)0xbfda00  /* TOD-HI, ciaB			   */
#define PRAA    (u_char *)0xbfe001  /* peripheral data register aA           */
# define DSK_RDY     (1<<5)
# define DSK_TRACK0  (1<<4)
# define DSK_WPROT   (1<<3)
# define DSK_CHANGE  (1<<2)
# define DSK_POW_LED (1<<1)
# define DSK_OVERLAY (1<<0)
#define LED     (u_char)  1         /* led bit in PRAA, 0=on/1=off           */
#define FIR0    (u_char)  6         /* firebutton 0 bit in PRAA, 0=pressed   */
#define PRAB    (u_char *)0xbfd000  /* peripheral data register aB           */
# define PR_BUSY     (1<<0)       /* Printer busy			   */
# define PR_POUT     (1<<1)       /* Paper out				   */
# define PR_SEL      (1<<2)       /* Printer select			   */
#define PRBA    (u_char *)0xbfe101  /* peripheral data register bA           */
#define PRBB	(u_char *)0xbfd100  /* peripheral data register bB	   */
# define DSK_MOTOR   (1<<7)
# define DSK_SEL3    (1<<6)
# define DSK_SEL2    (1<<5)
# define DSK_SEL1    (1<<4)
# define DSK_SEL0    (1<<3)
# define DSK_SIDE    (1<<2)
# define DSK_DIREC   (1<<1)
# define DSK_STEP    (1<<0)
#define SDRA    (u_char *)0xbfec01  /* serial data register A (keyboard)     */
#define TAHIA   (u_char *)0xbfe501  /* timer hi aA                           */
#define TALOA   (u_char *)0xbfe401  /* timer lo aA                           */
#define TBHIA   (u_char *)0xbfe701  /* timer hi bA                           */
#define TBLOA   (u_char *)0xbfe601  /* timer lo bA                           */
#define TBHIB   (u_char *)0xbfd700  /* timer hi bB                           */
#define TBLOB   (u_char *)0xbfd600  /* timer lo bB                           */
#define BALL    (u_char)  0x7f      /* all bits of a cia funny register      */
#define BCLR    (u_char)  0x00      /* code to clear a cia funny register    */
#define BSET    (u_char)  0x80      /* code to set a cia funny register      */

/* custom:                           rw adp, W:MSbit is set/clr,S:strobe   */
#define ADKCON  (u_short *)0xdff09e
#define ADKCONR (u_short *)0xdff010
#define AUD0LC  (ptr_t *) 0xdff0a0   /*  w a   audio #0 dmadata ptr_t, hi 3bits */
#define AUD0LEN (u_short *)0xdff0a4  /*  w   p audio #0 dma data len in words */
#define AUD0PER (u_short *)0xdff0a6  /*  w   p audio #0 dma data rate (freq)  */
#define AUD0VOL (u_short *)0xdff0a8  /*  w   p audio #0 dma data volume,64=max*/
#define BPL1MOD (u_short *)0xdff108  /*  w a   odd planemod,bytes between rows*/
#define BPL1PTH (ptr_t *) 0xdff0e0   /*  w a   bitplane 1 pointer, high 3 bits*/
#define BPLCON0 (u_short *)0xdff100  /*  w ad  bitplane ctrlreg 0, mode bits  */
#define HIRES   (u_short)  0x9200    /* compcolor on,1 bitplane,HIRES         */
#define BPLCON1 (u_short *)0xdff102  /*  w  d  bitplane ctrlreg 1, hor scroll */
#define BPLCON2 (u_short *)0xdff104  /*  w  d  bitplane ctrlreg 2,display pris*/
#define COLOR00 (u_short *)0xdff180  /*  w  d  color register 0, background   */
#define COLOR01 (u_short *)0xdff182  /*  w  d  color register 1, foreground   */
#define COP1LC  (ptr_t *) 0xdff080   /*  w a   copper 1st loc reg, hi 3 bits  */
#define COPJMP1 (u_short *)0xdff088  /*  S a   copper restart at COP1LC       */
#define DDFSTOP (u_short *)0xdff094  /*  w a   data fetch stop, hor end       */
#define DDFSTOPH (u_short) 0x00d4    /* usual value for a HIRES screen        */
#define DDFSTRT (u_short *)0xdff092  /*  w a   data fetch start, hor start    */
#define DDFSTRTH (u_short) 0x003c    /* usual value for a HIRES screen        */
#define DIWSTOP (u_short *)0xdff090  /*  w a   display stop,low right,MSB,LSB=*/
#define DIWSTOPH (u_short) 0xf4c1    /* usual value for a HIRES screen        */
#define DIWSTRT (u_short *)0xdff08e  /*  w a   display start,up left,=vert,hor*/
#define DIWSTRTH (u_short) 0x2c81    /* usual value for a HIRES screen        */
#define DMACON  (u_short *)0xdff096  /*  W adp dma control write              */
#define AUD0EN  (u_short)  1         /* the value to enable audio # 0, 2^0=1  */
#define DSKEN   (u_short)  16        /* the value to enable disk dma,  2^4=16 */
#define SPREN   (u_short)  32        /* the value for sprite dma,      2^5=32 */
#define COPEN   (u_short)  128       /* the value for copper dma,      2^7=128*/
#define BPLEN   (u_short)  256       /* the value for bitplane dma,    2^8=256*/
#define DMAEN   (u_short)  512       /* the value for all dma,         2^9=512*/
#define DMACONR (u_short *)0xdff002  /* r  a p dma control&blitter status rd  */
#define INTENA  (u_short *)0xdff09a  /*  W   p interrupt control write        */
#define INTENAR (u_short *)0xdff01c  /* r    p interrupt control read         */
#define INTREQ  (u_short *)0xdff09c  /*  W   p interrupt request write        */
#define INTREQR (u_short *)0xDFF01E
# define INTEN      (1<<14)
# define EXTER      (1<<13)
# define DSKSYN     (1<<12)
# define RBF        (1<<11)
# define PORTS      (1<<3)
# define DSKBLK     (1<<1)
# define TBE        (1<<0)
#define VPOSR   (u_short *)0xdff004  /* r  a   beam vert most sign bit rd     */
#define SERPER  (u_short *)0xdff032
#define SERDAT  (u_short *)0xdff030
#define SERDATR (u_short *)0xdff018
#define WALL    (u_short)  0x7fff    /* all bits in a custom chip funny reg   */
#define WCLR    (u_short)  0x0000    /* code to clear a custom chip funny reg.*/
#define WSET    (u_short)  0x8000    /* code to set a custom chip funny reg   */
#define WZERO   (u_short)  0x0000    /* 0 value of a custom register          */


# define INDEX       (1<<4)

#define DSKPT     (u_short **) 0xDFF020
#define DSKLEN     (u_short *) 0xDFF024

# define DMA_WRITE  (1<<14)
# define DMA_READ        0

#define DSKSYNC    (u_short *) 0xDFF07E

# define PRECOMPMASK (1<<13)|(1<<14)
# define PRECOMP0        0
# define PRECOMP140 (1<<13)
# define MFMPREC    (1<<12)
# define WORDSYNC   (1<<10)
# define MSBSYNC     (1<<9)
# define FAST        (1<<8)

#define CHIPMEM		0x80000		/* end of chipmem */
