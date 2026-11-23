CFLAGS	= -O
SRC1	= build.c bootblok.s init.c menu.c
SRC2	= outmix.h fakeunix.c getstruc.c putstruc.c

PARTS	= ../kernel/kernel.mix ../mm/mm.mix ../fs/fs.mix init.mix menu.mix
ALL	= minix.img
LD	= /usr/lib/ld
CV	= /usr/lib/cv
l	= /usr/lib


all:	$(ALL)

cp cmp:	all

clean:
	rm -f *.o

clobber:
	rm -f $(ALL) build minix.img init.mix menu.mix

build:	build.c outmix.h getstruc.c putstruc.c
	cc $(CFLAGS) -DAMIGA build.c -o $@

minix.img:	build $(PARTS)
	./build $(PARTS) $@

init.mix:	init.o $l/head.o $l/libc.a $l/end.o
	$(LD) -c -o init.out $l/head.o init.o $l/libc.a $l/end.o
	$(CV) init.out $@
	rm -f init.out

menu.mix:	menu.o $l/head.o $l/libc.a $l/end.o
	$(LD) -c -o menu.out $l/head.o menu.o $l/libc.a $l/end.o
	$(CV) menu.out $@
	rm -f menu.out
