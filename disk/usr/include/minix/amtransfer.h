/* transfer.h - defines the struct transferdata that is used to        */
/* transport data from the loader to minix                             */

#define MAXKEY     0x60         /* highest 'normal' keycode +1             */
struct minixkeymap {            /* used to convert raw keycodes to ascii   */
    char ascii[4*MAXKEY];       /* from 0*MAXKEY: no qualifiers depressed  */
                                /* from 1*MAXKEY: only shift depressed     */
                                /* from 2*MAXKEY: only alt depressed       */
                                /* from 3*MAXKEY: only shift & alt pressed */
    char *func[20];             /* from  0: only functionkey depressed     */
                                /* from 10: shift and functionkey pressed  */
                                /* these addresses are relative to functext*/
    char functext[400];         /* the text for the functionkeys           */
};

struct transferdata {
    long loadkernel;                /* where the kernel is loaded      */
    long runkernel;                 /* where the kernel will be moved  */
    long runaddress;                /* the actual address to run kernel*/
    long kernelsz;                  /* the kernel image size           */
    char mlroutine[1000];           /* routine that will move kernel   */
    long args[26];                  /* args passed to loader (-a to -z)*/
                                    /* only d, f, r, and t are used    */
#define NUMMEMLIST 128              /* max nr of different mem chunks  */
#define MEMCHUNKSZ 0x040000L        /* size of the chunks in bytes     */
    long transmemlist[NUMMEMLIST];  /* list to store memchunks         */
    long numintranslist;            /* nr of used slots in list        */
    struct minixkeymap mkeymap;     /* to convert scancodes to ascii   */
#define SPRITESZ 0x10
    long spritedata[SPRITESZ+1];    /* sprite 0 data                   */
};

