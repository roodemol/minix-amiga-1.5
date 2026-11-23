	.sect	.text

! Convert a binary 512-byte block (binbuf) to MFM-coded data and compute
! the Cyclic Redundancy Check (CRC) as is normally done by a hardware
! Floppy Disk Controller (FDC).
! The algorithms are:
! - To transform binary bytes into MFM-word insert a 0 between two bits
!   if one of its neighbours are a 1, or insert a 0 if both its neighbours
!   are 0. The binary byte "00110010" transforms into "?010010100100100",
!   where the first bit depends on the preceding word.
!   The inserted bits are called TAG-bits (T), the others DATA-bits (D).
! - To generate the CRC it is first initalized to -1 (0xFFFF).
!   For all following bits (high bit first) we use the following steps:
!   - XOR the new bit into bit 15 (msb) of the CRC
!   - If bit 15 of the CRC is now set XOR the CRC with 0x0810
!   - Rotate the CRC one bit left, putting bit 15 into bit 0
! Because this should all be done in software (bit by bit), this routine
! is written in assembly language, and both algorithms are combined as one.
	.extern	_b2r
_b2r:
	movem.l	a2-a3/d1-d7,-(sp)	! 9 regs = 36 bytes
	move.l	40(sp),a3
	move.l	44(sp),a2
				! d1 = BYTE from binbuf
				! d2 = counter for 512 bytes
				! d3 = the CRC (after 0xA1 0xA1 0xA1 0xFB)
	move.l	#511,d2		! d4 = bit count (7..0)
				! d5 = MFM-WORD
	move.w	#0x8000,d6	! d6,d7 = constants
	move.w	#0x0810,d7
	move.l	#0xE295,d3	! a2 = address of next MFM-WORD
				! a3 = address of next bin-BYTE
	move.w	(a2)+,d5	! get previous MFM-WORD (=DATA_ID)
.M4:
	move.l	#7,d4
	move.b	(a3)+,d1	! get a BYTE
.M5:
	lsl.w	#2,d5		! make room for T&D bits.
	lsl.b	#1,d1		! bit to X/C
	bcc	.M3
	add.w	#1,d5		!  T=0,D=1
	eor.w	d6,d3		! invert bit #15 (=bchg #15,d3)
	bra	.M1
.M3:
	btst	#2,d5		! was previous bit also 0?
	bne	.M6		!  T=0,D=0
	add.w	#2,d5		!  T=1,D=0
.M6:
	tst.w	d3		! bit #15 in crc set?
.M1:
	bpl	.M2
	eor.w	d7,d3		! yes, crc^=0x0810
.M2:	rol.w	#1,d3		! CRC: 0>1,1>2..14>15,15>0
	dbf	d4,.M5		! next bit in BYTE
	move.w	d5,(a2)+	! store MFM-WORD
	dbf	d2,.M4		! next BYTE
	move.l	d3,d0		! return crc-value
	movem.l	(sp)+,d1-d7/a2-a3
	rts


! As in "bin2raw" here we also have to convert from MFM to binary, and
! check the CRC. The only difference is that this time we don't have to
! worry about what bit to insert. We just remove all the extra bits, and
! what's left is the binary equivalent. Because we still have to generate
! the CRC we can't avoid assembly-language to prevent the whole system
! from being slowed down by every floppy-access.
! For the used algorithms see the comments in "bin2raw".
	.extern	_r2b
_r2b:
	movem.l	a2-a3/d1-d7,-(sp)	! 9 regs = 36 bytes
	move.l	40(sp),a3
	move.l	44(sp),a2
	move.w	#0x8000,d6	! consts in registers, for speed
	move.w	#0x0810,d7
	move.l	#511,d2
	move.l	#0xE295,d3
.N4:	move.w	(a2)+,d5
	move.l	#7,d4
.N3:	lsl.w	#2,d5
	bcc	.N1		! bit == 0 ?
	eor.w	d6,d3
.N1:	roxl.b	#1,d1
	tst.w	d3
	bpl	.N2
	eor.w	d7,d3
.N2:	rol.w	#1,d3
	dbf	d4,.N3		! next bit
	move.b	d1,(a3)+
	dbf	d2,.N4		! next byte
	move.l	d3,d0		! return crc
	movem.l	(sp)+,d1-d7/a2-a3
	rts

	.extern _long_copy
_long_copy:
	move.l	4(sp),a0
	move.l	8(sp),a1
	move.l	12(sp),d0
	move.l	d0,d1
	lsr.l	#3,d1
	and.l	#7,d0

	dbf	d1,2f
	bra	1f
2:	movem.l d2-d7/a2-a3,-(sp)
lloop:	movem.l (a0)+,d2-d7/a2-a3
	movem.l d2-d7/a2-a3,(a1)
	add.l   #32,a1
	dbf	d1,lloop
	movem.l (sp)+,d2-d7/a2-a3
	
1:	dbf	d0,loop
	rts
loop:	move.l	(a0)+,(a1)+
	dbf	d0,loop
	rts


	.extern _long_clear
_long_clear:
	move.l 	4(sp),a0
	move.l 	8(sp),d0
	sub.l   #1,d0
lc:
	clr.l   (a0)+
	dbf	d0,lc
	rts
