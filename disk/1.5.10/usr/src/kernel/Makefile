AS	= cc -c -DACK
LD	= /usr/lib/ld
CV	= /usr/lib/cv
l	= /usr/lib
h	= /usr/include/minix
i	= /usr/include

CFLAGS	= -O -DACK

OBJ	= mpx.o main.o proc.o system.o shadow.o \
	  tty.o clock.o memory.o \
	  con.o kbd.o vdu.o print.o  \
 	  table.o dmp.o copy68k.o misc.o \
	  rs232.o floppy.o floppy_conv.o

HDR	= $h/callnr.h $h/com.h $h/const.h $i/sgtty.h $h/type.h \
	  const.h glo.h proc.h tty.h type.h kernel.h amhardware.h

ALL	= kernel.mix

all:	$(ALL)

cp cmp:	all

clean:
	rm -f $(ALL) $(OBJ)

kernel.mix:	$(OBJ) $l/libc.a $l/end.o
	$(LD) -c -o kernel.out $(OBJ) $l/libc.a $l/end.o
	$(CV) kernel.out $@
	rm -f kernel.out

$(OBJ):	$(HDR)
