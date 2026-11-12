# Prepended to ee-gcc's assembly output so the modern gas produces what Sony's 2000-era
# assembler did. It applies to every C file (and to any asm a C file pulls in with INCLUDE_ASM).
#
# No assembler flag or `.set` mode does the same job: `.set mips4` and up drop the hazard nops
# but also the R5900 instruction set and encodings, `-O1` stops all delay-slot filling (the
# original has most of it), and this binutils fork adds no option for either.
#
# Helper symbols are named .L* so that they stay out of the object's symbol table.

# ---------------------------------------------------------------------------------------------
# Instruction wrappers
#
# The two fixes below have to act right behind particular instructions, or need to know which
# instructions came before a branch. Macros cannot ask gas for either, so the mnemonics the
# compiler emits are wrapped (lists further down): a wrapper removes itself to reach the
# built-in instruction, does its bookkeeping, and comes back.
#
# 1. Hazard nops (__gp_forget). For the R5900 (MIPS III) the modern gas keeps one instruction
#    between an FPU compare and the bc1t/bc1f that tests it, and between mtc1/mfc1/ctc1/cfc1
#    and the next use of the register, inserting a nop where the compiler left none. It does
#    the same at an alignment or data directive after the mtc1 that `li.s` expands to. Sony's
#    assembler knew the R5900 needs none of that. gas adds the nop while assembling whatever
#    comes next, from what it remembers of the previous instruction; an empty data directive
#    makes it settle that early, and under `.set mips4` it settles on "no nop".
#    Side effect: gas no longer knows the instruction, so it cannot move it into the delay slot
#    of an unfilled branch that follows directly. That is what the original has: Sony's
#    assembler never moved these instructions into a delay slot either
#    (`lui $at / mtc1 $at,$f12 / jal f / nop`).
#
# 2. Unfilled branches (.L__gp_state). In reorder mode the assembler fills an empty delay slot
#    by moving the instruction in front of the branch into it. Sony's assembler did not do that
#    when the instruction before THAT one was assembled under `.set noreorder` (its
#    "prev_prev_insn_unreordered" rule), which in compiler output means a delay slot the
#    compiler filled itself. The modern gas has no such rule. Two shapes show up. A return the
#    compiler duplicated, taking a copy of the first instruction of the shared tail for the
#    delay slot:
#            j $31 / move $2,$3      <- filled by the compiler (.set noreorder)
#        $L26:
#            move $2,$3              <- stays in front of the return ...
#            j $31                   <- ... which gets a nop; modern gas: `jr $ra / move`
#    and a call with a float constant right after a call whose slot the compiler filled:
#            jal f / li $4,1         <- filled by the compiler
#            li.s $f12,0.9           <- one instruction (a .lit4 load): stays in front ...
#            jal g                   <- ... of the call, which gets a nop
#    while after an unfilled branch (`jal f / nop / li.s $f12,0.9 / jal g`) the load does move.
#    A label on the branch stops the swap, but it may only go where the rule applies:
#    everywhere else the swap is original too. So the wrappers track
#      .L__gp_state   2 = behind a branch, 3 = behind the instruction after a branch that was
#                     assembled under noreorder (a delay slot the compiler filled), 4 = one
#                     further instruction later, and that one is a single machine instruction
#                     (if it expands to several, the one before the branch's predecessor is
#                     its own and the swap is right), 0 = anything else
#    and a branch reached in state 4 gets the label.
#    Macros cannot ask gas for its reorder mode or for the size of an instruction (using `.`
#    behind an instruction stops the swap all by itself: gas records it as a label), so both
#    are measured by assembling a probe into a scratch section (__gp_scratch).
#    Not covered: loads, stores and `la` in state 3 are not measured (with a symbol operand gas
#    decides their size after the macros have run), so they are still swapped; so is anything
#    behind break or sqrt.s, which are emitted as data. Instructions whose mnemonic is in
#    neither list below are not seen at all.
# ---------------------------------------------------------------------------------------------
.set .L__gp_state, 0

.macro __gp_forget
    .set push
    .set mips64
    .fill 0
    .set pop
.endm

# Scratch section for probes: not loaded, no file space (nobits), discarded by the linker
# script. Switching sections makes gas settle the way __gp_forget does, so a probe may only go
# in FRONT of the instruction being wrapped, where forgetting what came before is harmless
# (and under noreorder nothing is settled). The __gp_forget keeps the switch itself from adding
# a hazard nop. Ends with `.popsection`.
.macro __gp_scratch
    __gp_forget
    .pushsection .gcc_prelude_scratch, "", @nobits
    .set .L__gp_dot, .
.endm

# In front of an ordinary instruction. single=1: the instruction is known to be one machine
# instruction, 0: known not to be or not known, 2: the caller measures it.
.macro __gp_note single=0
    .if .L__gp_state == 2
        # Under noreorder a branch gets no nop from gas. bgezall has no wrapper.
        __gp_scratch
        bgezall $0, .L__gp_dot
        .if (. - .L__gp_dot) == 4
            .set .L__gp_state, 3
        .else
            .set .L__gp_state, 0
        .endif
        .popsection
    .else
        .if (.L__gp_state == 3) && (\single != 0)
            .set .L__gp_state, 4
        .else
            .set .L__gp_state, 0
        .endif
    .endif
.endm

# measure=1: in state 3, assemble the instruction into the scratch section first to see
# whether it is a single machine instruction.
.macro __gp_insn mnem, forget=0, measure=1
    .macro \mnem operands:vararg
        .purgem \mnem
        .if \measure
            __gp_note 2
            .if .L__gp_state == 4
                __gp_scratch
                \mnem \operands
                .if (. - .L__gp_dot) != 4
                    .set .L__gp_state, 0
                .endif
                .popsection
            .endif
        .else
            __gp_note 0
        .endif
        \mnem \operands
        .if \forget
            __gp_forget
        .endif
        __gp_insn \mnem, \forget, \measure
    .endm
.endm

# The label is harmless if the branch turns out to be in a noreorder block.
.macro __gp_branch mnem
    .macro \mnem operands:vararg
        .purgem \mnem
        .if .L__gp_state == 4
99770:
        .endif
        \mnem \operands
        .set .L__gp_state, 2
        __gp_branch \mnem
    .endm
.endm

.irp mnem, j, jal, jr, jalr, b, bal, beq, bne, beqz, bnez, blez, bgtz, bltz, bgez, bgt, bge, blt, ble, bgtu, bgeu, bltu, bleu, beql, bnel, beqzl, bnezl, blezl, bgtzl, bltzl, bgezl, bgtl, bgel, bltl, blel, bgtul, bgeul, bltul, bleul, bltzal, bgezal, bc1t, bc1f, bc1tl, bc1fl
    __gp_branch \mnem
.endr

# Not listed: move, break, cvt.w.s and sqrt.s (defined below on top of these) and what only
# hand-written asm uses (COP0, COP2/VU, most of MMI, cache, syscall).
.irp mnem, nop, lui, li, dli, add, addu, addi, addiu, sub, subu, neg, negu, abs, dadd, daddu, daddi, daddiu, dsub, dsubu, dneg, dnegu, dabs, and, andi, or, ori, xor, xori, nor, not, sll, srl, sra, sllv, srlv, srav, dsll, dsrl, dsra, dsll32, dsrl32, dsra32, dsllv, dsrlv, dsrav, slt, slti, sltu, sltiu, seq, sne, sgt, sgtu, sge, sgeu, sle, sleu, movz, movn, mult, multu, mult1, multu1, mul, div, divu, div1, divu1, rem, remu, madd, maddu, madd1, maddu1, mfhi, mflo, mthi, mtlo, mfhi1, mflo1, mthi1, mtlo1, mfsa, mtsa, por, pand, pxor, pnor, pcpyld, pcpyud, add.s, sub.s, mul.s, div.s, abs.s, neg.s, mov.s, cvt.s.w, trunc.w.s, max.s, min.s, madd.s, msub.s, adda.s, suba.s, mula.s, madda.s, msuba.s, rsqrt.s
    __gp_insn \mnem
.endr

# Loads, stores and addresses: not measured (see "Not covered" above).
.irp mnem, la, dla, lb, lbu, lh, lhu, lw, lwu, ld, lwl, lwr, ldl, ldr, lq, sb, sh, sw, sd, swl, swr, sdl, sdr, sq, ulh, ulhu, ulw, uld, ush, usw, usd, lwc1, swc1, l.s, s.s
    __gp_insn \mnem, 0, 0
.endr

# The instructions gas would put a hazard nop behind. They stay real instructions, in any
# operand syntax; the compares keep the R5900 function codes gas already uses for this CPU.
#
.irp mnem, c.f.s, c.eq.s, c.lt.s, c.le.s, mtc1, mfc1, ctc1, cfc1
    __gp_insn \mnem, 1, 0
.endr

# `li.s`: gas assembles it itself, as lui $at / mtc1 $at when the low or the high 16 bits of
# the constant are zero and as a one-instruction load from .lit4 otherwise (under -G0 it would
# always be lui / ori / mtc1; not seen yet, and treated as the .lit4 form here). Both match
# the original. The two forms need different treatment: the mtc1 form is forgotten like any
# other mtc1 (which also keeps it out of a following branch's delay slot, as in the original),
# the .lit4 form is an ordinary instruction that Sony's assembler did move into the delay slot
# of an unfilled `jal` or `j $31` behind it. Which form it will be is found out by loading the
# same constant into an integer register in the scratch section: one instruction (lui or ori)
# exactly when gas picks the mtc1 form. (The real load cannot be tried out there: it would
# leave a second copy of the constant in .lit4.)
.macro __gp_lis
    .macro li.s reg, value
        .purgem li.s
        __gp_scratch
        li.s $2, \value
        .set .L__gp_lit4, (. - .L__gp_dot) > 4
        .popsection
        __gp_note .L__gp_lit4
        li.s \reg, \value
        .if !.L__gp_lit4
            __gp_forget
        .endif
        __gp_lis
    .endm
.endm
__gp_lis

# ---------------------------------------------------------------------------------------------
# Encodings
# ---------------------------------------------------------------------------------------------

# Register moves were 64-bit.
.macro move dst, src
    daddu \dst, \src, $0
.endm

# `break N` (emitted for division-by-zero checks) put N in the low code field, which is where
# the two-operand form `break 0, N` has it.
.macro break code, code2
    __gp_note 0
    .ifb \code2
        .word (\code << 6) | 0xD
    .else
        .word (\code << 16) | (\code2 << 6) | 0xD
    .endif
.endm

# The R5900's only float-to-int conversion truncates, and Sony's assembler took the name
# `cvt.w.s` for it. The modern gas only knows it as `trunc.w.s` (same encoding).
.macro cvt.w.s dst, src
    trunc.w.s \dst, \src
.endm

# \sym = the number of register operand \reg, written `$N` or `$fN` as the compiler does;
# 99 for anything else. For instructions that have to be emitted as raw words.
.macro __gp_regnum sym, reg
    .set \sym, 99
    .irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
        .ifc \reg,$\n
            .set \sym, \n
        .endif
        .ifc \reg,$f\n
            .set \sym, \n
        .endif
    .endr
.endm

# `sqrt.s fd, ft`: the R5900 takes the operand from the ft field (bits 16-20); the modern gas
# puts it in fs (bits 11-15) even for this CPU.
.macro sqrt.s fd, ft
    __gp_regnum .L__gp_fd, \fd
    __gp_regnum .L__gp_ft, \ft
    .if (.L__gp_fd > 31) || (.L__gp_ft > 31)
        .error "gcc_prelude.inc: sqrt.s \fd, \ft: operands must be written $fN"
    .endif
    __gp_note 0
    .word 0x46000004 | (.L__gp_ft << 16) | (.L__gp_fd << 6)
.endm

# `li.d` into an integer register (a `double` constant handled by the soft-float library, e.g. `pow(10.0, n)` in
# src/menu/menu_za_d.c): gas refuses the mnemonic for the r5900, Sony's assembler encoded it like `dli` of the
# constant's bits (10.0 is `ori $22,$0,0x8048 / dsll32 $22,$22,15`). Assembled as mips3, gas does the same.
# Its size is not measured for the delay-slot rules above.
.macro __gp_lid
    .macro li.d reg, value
        .purgem li.d
        .set push
        .set mips3
        li.d \reg, \value
        .set pop
        __gp_lid
    .endm
.endm
__gp_lid
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
