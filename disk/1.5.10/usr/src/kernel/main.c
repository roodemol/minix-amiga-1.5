#if (MACHINE == AMIGA)
/* This file contains the main program of MINIX for the Amiga
 * The routine main() initializes the system and starts the ball
 * rolling by setting up the proc table, interrupt vectors, and
 * scheduling each task to run to initialize itself.
 * 
 * The entries into this file are:
 *   main:		MINIX main program
 *   trap:		called for an unexpected trap (synchronous)
 *   panic:		abort MINIX due to a fatal error
 */

#include "kernel.h"
#include <signal.h>
#include <minix/config.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include "proc.h"

static void amigainit();
void panic();

unsigned char ciaa_mask = 0, ciab_mask = 0;
long debug;

#define SIZES              8	/* sizes array has 8 entries */

/*===========================================================================*
 *                                   main                                    * 
 *===========================================================================*/
PUBLIC void main()
{
/* Start the ball rolling. */

  register struct proc *rp;
  register int t;
  register vir_clicks size;
  register phys_clicks base;
  reg_t ktsb;

  amigainit();

  /* Clear the process table.
   * Set up mappings for proc_addr() and proc_number() macros.
   */
  for (rp = BEG_PROC_ADDR, t = -NR_TASKS; rp < END_PROC_ADDR; ++rp, ++t) {
        rp->p_flags = P_SLOT_FREE;
        rp->p_nr = t;           /* proc number from ptr */
        (pproc_addr + NR_TASKS)[t] = rp;        /* proc ptr from number */
  }
 
  size = sizes[0] + sizes[1];	/* kernel text + data size */
  base = size;			/* end of kernel */

  ktsb = ((reg_t) t_stack + (ALIGNMENT - 1)) & ~((reg_t) ALIGNMENT - 1);
  for (t = -NR_TASKS; t < 0; t++) {	/* for all drivers */
	rp = proc_addr(t);
	rp->p_flags = 0;
	ktsb += tasktab[t+NR_TASKS].stksize;
	rp->p_reg.sp = ktsb;
	rp->p_splow = rp->p_reg.sp;
	rp->p_reg.pc = (reg_t) tasktab[t + NR_TASKS].initial_pc;
	if (!isidlehardware(t)) {
		lock_ready(rp);	/* IDLE, HARDWARE neveready */
		rp->p_reg.psw = 0x2000;	/* S-BIT */
	} else {
		rp->p_reg.psw = 0x0000;
	}

	rp->p_map[T].mem_len  = sizes[0];
/*	rp->p_map[T].mem_phys = 0; */
	rp->p_map[D].mem_len  = sizes[1]; 
	rp->p_map[D].mem_phys = sizes[0];
/*	rp->p_map[S].mem_len  = 0; */
	rp->p_map[S].mem_phys = size;
/*	rp->p_map[T].mem_vir  = rp->p_map[T].mem_phys; */
	rp->p_map[D].mem_vir  = rp->p_map[D].mem_phys;
	rp->p_map[S].mem_vir  = rp->p_map[S].mem_phys;
  }

  rp = proc_addr(HARDWARE);
  rp->p_map[D].mem_len  = ~0;	/* maximum size */
  rp->p_map[D].mem_phys = 0;
  rp->p_map[D].mem_vir  = 0;

  for (t = 0; t <= LOW_USER; t++) {
	rp = proc_addr(t);
	rp->p_flags = 0;
	lock_ready(rp);
	rp->p_reg.psw = (reg_t)0x0000;
	rp->p_reg.pc = (reg_t) ((long)base << CLICK_SHIFT);
	size = sizes[2*t + 2];
	rp->p_map[T].mem_len  = size;
	rp->p_map[T].mem_phys = base;
	base += size;
	size = sizes[2*t + 3];
	rp->p_map[D].mem_len  = size;
	rp->p_map[D].mem_phys = base;
	base += size;
	rp->p_map[S].mem_len  = 0;
	rp->p_map[S].mem_phys = base;
	rp->p_map[T].mem_vir  = rp->p_map[T].mem_phys;
	rp->p_map[D].mem_vir  = rp->p_map[D].mem_phys;
	rp->p_map[S].mem_vir  = rp->p_map[S].mem_phys;
  }

  bill_ptr = proc_addr(HARDWARE);	/* it has to point somewhere */
  lock_pick_proc();

  /* go back to assembly code to start running the current process. */
}


/*===========================================================================*
 *                                trap                                       * 
 *===========================================================================*/

PUBLIC void trap()
{
  register t;
  register struct proc *rp;
  static char vecsig[] = {
	0, 0, SIGSEGV, SIGBUS, SIGILL, SIGILL, SIGILL, SIGABRT,
	SIGILL, SIGTRAP, SIGEMT, SIGFPE, SIGSTKFLT
  };

  rp = proc_ptr;
  t = rp->p_trap;
  if (rp->p_reg.psw & 0x2000) panic("trap via vector", t);
  if (t >= 0 && t < sizeof(vecsig)/sizeof(vecsig[0]) && vecsig[t]) {
	t = vecsig[t];
  } else {
	printf("\nUnexpected trap.  Vector = %d\n", t);
	t = SIGILL;
  }
  if (t != SIGSTKFLT) {	/* DEBUG */
	printf("sig=%d to pid=%d at pc=%X\n",
		t, rp->p_pid, rp->p_reg.pc);
	dump();
  }
  cause_sig(proc_number(rp), t);
}

PUBLIC void checksp()
{
  register struct proc *rp;
  register phys_bytes ad;

  rp = proc_ptr;
  /* if a user process is is supervisor mode don't check stack */
  if ((rp->p_nr >= 0) && (rp->p_reg.psw & 0x2000)) return;
  if (rp->p_reg.sp < rp->p_splow)
	rp->p_splow = rp->p_reg.sp;
  if (rp->p_map[S].mem_len == 0)
	return;
  ad = (phys_bytes)rp->p_map[S].mem_phys;
  ad <<= CLICK_SHIFT;
  if ((phys_bytes)rp->p_reg.sp > ad)
	return;
  /*
   * Stack violation.
   */
  ad = (phys_bytes)rp->p_map[D].mem_phys;
  ad += (phys_bytes)rp->p_map[D].mem_len;
  ad <<= CLICK_SHIFT;
  if ((phys_bytes)rp->p_reg.sp < ad + CLICK_SIZE)
	printf("Stack low (pid=%d,pc=%X,sp=%X,end=%X)\n",
		rp->p_pid, (long)rp->p_reg.pc,
		(long)rp->p_reg.sp, (long)ad);
  rp->p_trap = 12;	/* fake trap causing SIGSTKFLT */
  trap();
}

/*===========================================================================*
 *                                   panic                                   * 
 *===========================================================================*/
PUBLIC void panic(s,n)
char *s;
int n; 
{
/* The system has run aground of a fatal error.  Terminate execution.
 * If the panic originated in MM or FS, the string will be empty and the
 * file system already syncked.  If the panic originates in the kernel, we are
 * kind of stuck. 
 */

  if (*s != 0) {
	printf("\nKernel panic: %s",s); 
	if (n != NO_NUM) printf(" %d", n);
	printf("\n");
  }
  dump();
  printf("\nMINIX halted\n\nPlease reset your Amiga.\n");
  for (;;)
	;
}

/*
 * Amiga specific initialization.
 */

#include <minix/amtransfer.h>
#include "amhardware.h"

/*===========================================================================*
 *                               amigainit                                   * 
 *===========================================================================*/

PRIVATE void amigainit()
{
  register unsigned long ad;
  struct transferdata *transdat;

  if (*ICRA);	/* read to clear */
  if (*ICRB);
  disable_int(WALL);
  enable_int(INTEN);
  *TODHA = 0;		/* Stop TOD (=Event counters) */
  *TODHB = 0;
  *CRAA &= 0xFE;	/* Stop ALL CIA timers */
  *CRAB &= 0xFE;
  *CRBA &= 0xFE;
  *CRBB &= 0xFE;
  *DMACON = (WSET | DMAEN);
/* Every task which needs an interrupt, will have to enable it itself. */

  transdat = *(struct transferdata **)0x0000;
  debug = transdat->args['d'-'a'];

}

enable_int(mask)
u_short mask;
{
  *INTENA = (WSET | mask);
}

disable_int(mask)
u_short mask;
{
  *INTENA = (WCLR | mask);
  *INTREQ = (WCLR | mask);
}

PUBLIC void fake_int();

PUBLIC void nmi_int(t)		{fake_int("nmi", t);}	/* Unused interrupts */
PUBLIC void aud_int(t)		{fake_int("aud", t);}
PUBLIC void alarmA_int(t)	{fake_int("alarmA", t);}
PUBLIC void alarmB_int(t)	{fake_int("alarmB", t);}
PUBLIC void timerAA_int(t)	{fake_int("timerAA", t);}
PUBLIC void timerAB_int(t)	{fake_int("timerAB", t);}
PUBLIC void vbl_int(t)		{fake_int("vertical blank", t);}

PUBLIC void tbe_int(t)
{
    /* this routine is called by CIA TBE interrupts, which are NOT the 
     * same as serial port TBE interrupts
     */
    fake_int("transmit buffer empty", t);
}


PUBLIC void dsksyn_int(t)
{
    /* this routine is called by all level 5 interrupts */
    if (*INTREQR & RBF) /* actually a rbf_int occured */
        siaint(0);
    else
        fake_int("disk sync found", t);
}

/*===========================================================================*
 *                             CIA interrupts                               *
 *===========================================================================*/

ciaa_int()
{
/* The following interrupts are generated bij CIA-A (= level 2):
 * timerAA_int, clock_handler, alarmA_int, kbdint, flagA_int
 */
  register u_short i;

  i = *ICRA & ciaa_mask;
  *INTREQ = (WCLR | PORTS);

  if (i & 1) timerAA_int();  /* Not used, occupied by kbd */
  if (i & 2) clock_handler();/* Used for clock-task */
  if (i & 4) alarmA_int();   /* Not used, freq dep. on PAL/NTSC */
  if (i & 8) kbdint();       /* Used for keyboard-input */
  if (i & 16) prt_ack_int(); /* Not used, for centronics prt */
}

enable_ciaa_int(mask)
u_char mask;
{
  ciaa_mask |= mask;
  *ICRA = (BSET | mask);
}

enable_ciab_int(mask)
u_char mask;
{
  ciab_mask |= mask;
  *ICRB = (BSET | mask);
}

disable_ciaa_int(mask)
u_char mask;
{
  ciaa_mask &= ~mask;
  *ICRA = (BCLR | mask);
}

disable_ciab_int(mask)
u_char mask;
{
  ciab_mask &= ~mask;
  *ICRB = (BCLR | mask);
}

ciab_int()
{
/* The following interrupts are generated bij CIA-B (= level 6):
 * timerAB_int, timerBB_int, alarmB_int, tbe_int, index_int
 */
  register u_short i;
  i = *ICRB /* & ciab_mask*/;
i &= ciab_mask;
  *INTREQ = (WCLR | EXTER);

  if (i & 1) timerAB_int();  /* Not used, occ. by serial port */
  if (i & 2) flstep_int();   /* Used for stepping the disk motor */
  if (i & 4) alarmB_int();   /* Not used, freq dep. on PAL/NTSC */
  if (i & 8) tbe_int();      /* Not used, for serial-port */
  if (i & 16) index_int();   /* Used by the floppy task */
}

/*===========================================================================*
 *				temporary stuff				     *
 *===========================================================================*/

PUBLIC void fake_task(s)
char *s;
{
  message m, reply;

  /* printf("%s alive\n", s); */
  for (;;) {
	receive(ANY, &m);
	printf("%s received %d from %d\n", s, m.m_type, m.m_source);
	reply.m_type = TASK_REPLY;
	reply.REP_STATUS = EIO;
	reply.REP_PROC_NR = m.PROC_NR;
	send(m.m_source, &reply);
  }
}

PUBLIC void fake_int(s, t)
char *s;
int t;
{
  printf("Fake interrupt handler for %s. trap = %02x\n", s, t);
}

PUBLIC void winchester_task()
{
  fake_task("winchester_task");
}

PUBLIC void idle_task()
{
#if 0
  /* the following code is useful to determine stack sizes */
  static int beenhere = 0;
  int t;
  reg_t ktsb;
  register struct proc *rp;

  if (!beenhere)
  {
	beenhere = 1;
	ktsb = ((reg_t) t_stack + (ALIGNMENT - 1)) & ~((reg_t) ALIGNMENT - 1);
	for (t = 0; t < NR_TASKS; t++)
	{
		ktsb += tasktab[t].stksize;
		printf("task %s, stack start: %lx\n", tasktab[t].name, ktsb);
	}
	for (t = 0; t <= LOW_USER; t++) {
		rp = proc_addr(t);
  		if (rp->p_splow == 0)
		rp->p_splow = rp->p_reg.sp;
	}
  }
#endif

   while (1);
}

PUBLIC unsigned long sys_alloc(size)
unsigned long size;
{
	return (*(unsigned long *)0x0004 -= size);
}
#endif
