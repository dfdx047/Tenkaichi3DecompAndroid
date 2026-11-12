	.file	1 "test.c"
	.section .mdebug.eabi64
	.previous
gcc2_compiled.:
__gnu_compiled_c:
	.text
	.align	2
	.ent	putc_
putc_:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, extra= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	sll	$4,$4,24
	sra	$4,$4,24
	li	$2,268435456			# 0x10000000
	ori	$2,$2,0xf180
	#.set	volatile
	sb	$4,0($2)
	#.set	novolatile
	j	$31
	.end	putc_
	.rdata
	.align	3
$LC0:
	.ascii	"0123456789abcdef\000"
	.text
	.align	2
	.ent	hex
hex:
	.frame	$sp,32,$31		# vars= 0, regs= 4/0, args= 0, extra= 0
	.mask	0x80070000,-8
	.fmask	0x00000000,0
	subu	$sp,$sp,32
	sd	$16,0($sp)
	sd	$17,8($sp)
	sd	$18,16($sp)
	sd	$31,24($sp)
	move	$17,$4
	li	$16,28			# 0x1c
	lui	$2,%hi($LC0) # high
	addiu	$18,$2,%lo($LC0) # low
	srl	$2,$17,$16
$L9:
	andi	$2,$2,0xf
	addu	$2,$2,$18
	.set	noreorder
	.set	nomacro
	jal	putc_
	lb	$4,0($2)
	.set	macro
	.set	reorder

	addu	$16,$16,-4
	.set	noreorder
	.set	nomacro
	bgez	$16,$L9
	srl	$2,$17,$16
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	putc_
	li	$4,32			# 0x20
	.set	macro
	.set	reorder

	ld	$16,0($sp)
	ld	$17,8($sp)
	ld	$18,16($sp)
	ld	$31,24($sp)
	#nop
	.set	noreorder
	.set	nomacro
	j	$31
	addu	$sp,$sp,32
	.set	macro
	.set	reorder

	.end	hex
	.align	2
	.ent	str
str:
	.frame	$sp,16,$31		# vars= 0, regs= 2/0, args= 0, extra= 0
	.mask	0x80010000,-8
	.fmask	0x00000000,0
	subu	$sp,$sp,16
	sd	$16,0($sp)
	sd	$31,8($sp)
	move	$16,$4
	lbu	$2,0($16)
	#nop
	.set	noreorder
	.set	nomacro
	beq	$2,$0,$L12
	move	$4,$2
	.set	macro
	.set	reorder

$L13:
	sll	$4,$4,24
	addu	$16,$16,1
	.set	noreorder
	.set	nomacro
	jal	putc_
	sra	$4,$4,24
	.set	macro
	.set	reorder

	lb	$2,0($16)
	#nop
	.set	noreorder
	.set	nomacro
	bne	$2,$0,$L13
	move	$4,$2
	.set	macro
	.set	reorder

$L12:
	ld	$16,0($sp)
	ld	$31,8($sp)
	#nop
	.set	noreorder
	.set	nomacro
	j	$31
	addu	$sp,$sp,16
	.set	macro
	.set	reorder

	.end	str
	.data
	.align	2
	.type	 seed,@object
	.size	 seed,4
seed:
	.word	12345
	.text
	.align	2
	.ent	rnd
rnd:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, extra= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	lui	$4,%hi(seed) # high
	addiu	$4,$4,%lo(seed) # low
	lw	$2,0($4)
	li	$3,1638400			# 0x190000
	ori	$3,$3,0x660d
	mult	$2,$2,$3
	la $2,1013904223($2)
	.set	noreorder
	.set	nomacro
	j	$31
	sw	$2,0($4)
	.set	macro
	.set	reorder

	.end	rnd
	.align	2
	.ent	f
f:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, extra= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	mtc1	$4,$f0
	j	$31
	.end	f
	.align	2
	.ent	u
u:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, extra= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	mfc1	$2,$f12
	j	$31
	.end	u
	.rdata
	.align	3
$LC1:
	.ascii	"\n"
	.ascii	"PS2FLOAT BEGIN\n\000"
	.align	3
$LC2:
	.ascii	"PS2FLOAT END\n\000"
	.text
	.align	2
	.globl	main
	.ent	main
main:
	.frame	$sp,96,$31		# vars= 0, regs= 8/4, args= 0, extra= 0
	.mask	0x807f0000,-40
	.fmask	0x00f00000,-8
	subu	$sp,$sp,96
	sd	$16,0($sp)
	sd	$17,8($sp)
	sd	$18,16($sp)
	sd	$19,24($sp)
	sd	$20,32($sp)
	sd	$21,40($sp)
	sd	$22,48($sp)
	sd	$31,56($sp)
	s.s	$f23,88($sp)
	s.s	$f22,80($sp)
	s.s	$f21,72($sp)
	.set	noreorder
	.set	nomacro
	jal	__main
	s.s	$f20,64($sp)
	.set	macro
	.set	reorder

	lui	$4,%hi($LC1) # high
	.set	noreorder
	.set	nomacro
	jal	str
	addiu	$4,$4,%lo($LC1) # low
	.set	macro
	.set	reorder

	move	$20,$0
	li	$21,56			# 0x38
	li	$22,2155806720			# 0x807f0000
	ori	$22,$22,0xffff
$L22:
	jal	rnd
	.set	noreorder
	.set	nomacro
	jal	rnd
	move	$18,$2
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	rnd
	move	$17,$2
	.set	macro
	.set	reorder

	srl	$2,$2,8
	divu	$0,$2,$21
	mfhi	$2
	#nop
	.set	noreorder
	beql	$21,$0,1f
	break	7
1:
	.set	reorder
	addu	$19,$2,100
	li	$2,3			# 0x3
	div	$0,$20,$2
	mfhi	$3
	#nop
	.set	noreorder
	beql	$2,$0,1f
	break	7
1:
	.set	reorder
	bne	$3,$0,$L23
	jal	rnd
	srl	$2,$2,8
	li	$3,30			# 0x1e
	divu	$0,$2,$3
	mfhi	$2
	#nop
	.set	noreorder
	beql	$3,$0,1f
	break	7
1:
	.set	reorder
	.set	noreorder
	.set	nomacro
	b	$L24
	subu	$4,$19,$2
	.set	macro
	.set	reorder

$L23:
	jal	rnd
	srl	$2,$2,8
	divu	$0,$2,$21
	mfhi	$2
	#nop
	.set	noreorder
	beql	$21,$0,1f
	break	7
1:
	.set	reorder
	addu	$4,$2,100
$L24:
	and	$3,$18,$22
	sll	$2,$19,23
	or	$18,$3,$2
	and	$3,$17,$22
	sll	$2,$4,23
	or	$17,$3,$2
	li	$2,7			# 0x7
	div	$0,$20,$2
	mfhi	$3
	#nop
	.set	noreorder
	beql	$2,$0,1f
	break	7
1:
	.set	reorder
	bne	$3,$0,$L25
	jal	rnd
	li	$3,-16			# 0xfffffffffffffff0
	and	$3,$18,$3
	srl	$2,$2,8
	andi	$2,$2,0xf
	xor	$17,$3,$2
$L25:
	.set	noreorder
	.set	nomacro
	jal	f
	move	$4,$18
	.set	macro
	.set	reorder

	mov.s	$f20,$f0
	mov.s	$f23,$f20
	.set	noreorder
	.set	nomacro
	jal	f
	move	$4,$17
	.set	macro
	.set	reorder

	mov.s	$f21,$f0
	.set	noreorder
	.set	nomacro
	jal	rnd
	mov.s	$f22,$f21
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	rnd
	move	$16,$2
	.set	macro
	.set	reorder

	srl	$2,$2,8
	li	$3,24			# 0x18
	divu	$0,$2,$3
	mfhi	$2
	#nop
	.set	noreorder
	beql	$3,$0,1f
	break	7
1:
	.set	reorder
	sra	$16,$16,$2
	.set	noreorder
	.set	nomacro
	jal	hex
	move	$4,$18
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	hex
	move	$4,$17
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	hex
	move	$4,$16
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	u
	add.s	$f12,$f20,$f21
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	hex
	move	$4,$2
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	u
	sub.s	$f12,$f20,$f21
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	hex
	move	$4,$2
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	u
	mul.s	$f12,$f20,$f21
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	hex
	move	$4,$2
	.set	macro
	.set	reorder

	.set	noreorder
	nop
	nop
	div.s	$f12,$f20,$f21
	.set	reorder
	jal	u
	.set	noreorder
	.set	nomacro
	jal	hex
	move	$4,$2
	.set	macro
	.set	reorder

 #APP
	abs.s $f2, $f20
 sqrt.s $f12, $f2
 #NO_APP
	jal	u
	.set	noreorder
	.set	nomacro
	jal	hex
	move	$4,$2
	.set	macro
	.set	reorder

 #APP
	mtc1 $0, $f2
 adda.s $f2, $f20
 madd.s $f12, $f20, $f21
 #NO_APP
	jal	u
	.set	noreorder
	.set	nomacro
	jal	hex
	move	$4,$2
	.set	macro
	.set	reorder

	mtc1	$16,$f12
	cvt.s.w	$f12,$f12
	jal	u
	.set	noreorder
	.set	nomacro
	jal	hex
	move	$4,$2
	.set	macro
	.set	reorder

	and	$2,$18,$22
	andi	$4,$19,0x1f
	addu	$4,$4,120
	sll	$4,$4,23
	.set	noreorder
	.set	nomacro
	jal	f
	or	$4,$2,$4
	.set	macro
	.set	reorder

	cvt.w.s $f1,$f0
	mfc1	$4,$f1
	jal	hex
 #APP
	mfc1 $8, $f23
 qmtc2 $8, $vf4
 mfc1 $8, $f22
 qmtc2 $8, $vf5
	vadd.x $vf6, $vf4, $vf5
 qmfc2 $8, $vf6
 move $4, $8
 #NO_APP
	jal	hex
 #APP
	vsub.x $vf6, $vf4, $vf5
 qmfc2 $8, $vf6
 move $4, $8
 #NO_APP
	jal	hex
 #APP
	vmul.x $vf6, $vf4, $vf5
 qmfc2 $8, $vf6
 move $4, $8
 #NO_APP
	jal	hex
 #APP
	vdiv $Q, $vf4x, $vf5x
 vwaitq
 vaddq.x $vf6, $vf0, $Q
 qmfc2 $8, $vf6
 move $4, $8
 #NO_APP
	jal	hex
 #APP
	vsqrt $Q, $vf4x
 vwaitq
 vaddq.x $vf6, $vf0, $Q
 qmfc2 $8, $vf6
 move $4, $8
 #NO_APP
	jal	hex
 #APP
	vmula.x $ACC, $vf4, $vf5
 vmadd.x $vf6, $vf4, $vf5
 qmfc2 $8, $vf6
 move $4, $8
 #NO_APP
	jal	hex
 #APP
	move $8, $16
 qmtc2 $8, $vf7
 vitof0.x $vf6, $vf7
 qmfc2 $8, $vf6
 move $4, $8
 #NO_APP
	jal	hex
 #APP
	vftoi0.x $vf6, $vf4
 qmfc2 $8, $vf6
 move $4, $8
 #NO_APP
	.set	noreorder
	.set	nomacro
	jal	hex
	addu	$20,$20,1
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	putc_
	li	$4,10			# 0xa
	.set	macro
	.set	reorder

	slt	$2,$20,1500
	.set	noreorder
	.set	nomacro
	bne	$2,$0,$L22
	lui	$4,%hi($LC2) # high
	.set	macro
	.set	reorder

	.set	noreorder
	.set	nomacro
	jal	str
	addiu	$4,$4,%lo($LC2) # low
	.set	macro
	.set	reorder

	move	$2,$0
	ld	$16,0($sp)
	ld	$17,8($sp)
	ld	$18,16($sp)
	ld	$19,24($sp)
	ld	$20,32($sp)
	ld	$21,40($sp)
	ld	$22,48($sp)
	ld	$31,56($sp)
	l.s	$f23,88($sp)
	l.s	$f22,80($sp)
	l.s	$f21,72($sp)
	l.s	$f20,64($sp)
	#nop
	.set	noreorder
	.set	nomacro
	j	$31
	addu	$sp,$sp,96
	.set	macro
	.set	reorder

	.end	main
	.section	.bss
vu:
	.align	4
	.space	16
	.previous
