/*
 * exc.S
 *	R4000 exception handling for ip20mon: the stubs trap.c copies to
 *	the exception vectors, the common entry and exit path, setjmp and
 *	longjmp, CP0 accessors, and the instructions the "trap" command
 *	uses to raise exceptions on purpose.
 *
 *	On an exception the CPU sets Status.EXL, which disables interrupts
 *	and forces kernel mode, puts the faulting PC in EPC, and jumps to
 *	the vector.  k0 and k1 are reserved for this code; everything else
 *	is saved in a trap frame before C runs, and restored before eret.
 */

#include "ip20.h"

	.set	noreorder
	.set	noat
	.text

/*
 * Vector stubs.  Each is copied to its vector address, so it must be
 * position independent: load exc_common's absolute address and jump,
 * with the vector number in k1 (set in the jump's delay slot).
 */
	.globl	vec_tlb, vec_tlb_end
vec_tlb:
	lui	$k0, %hi(exc_common)
	addiu	$k0, $k0, %lo(exc_common)
	jr	$k0
	addiu	$k1, $zero, 0
vec_tlb_end:

	.globl	vec_xtlb, vec_xtlb_end
vec_xtlb:
	lui	$k0, %hi(exc_common)
	addiu	$k0, $k0, %lo(exc_common)
	jr	$k0
	addiu	$k1, $zero, 1
vec_xtlb_end:

	.globl	vec_general, vec_general_end
vec_general:
	lui	$k0, %hi(exc_common)
	addiu	$k0, $k0, %lo(exc_common)
	jr	$k0
	addiu	$k1, $zero, 2
vec_general_end:

/*
 * Common entry.  Pick this nesting level's save area, save the state,
 * leave exception level (with interrupts off) and call trap(frame) on
 * that level's stack.  trap() returns the frame to resume from; it may
 * have changed epc and sp to abandon the interrupted code.
 */
	.globl	exc_common
	.ent	exc_common
exc_common:
	lui	$k0, %hi(exc_vecnum)
	sw	$k1, %lo(exc_vecnum)($k0)

	lui	$k0, %hi(exc_depth)
	lw	$k1, %lo(exc_depth)($k0)
	nop
	sltiu	$k1, $k1, EXC_LEVELS
	beqz	$k1, exc_hang			/* nested too deep: stop */
	nop
	lw	$k1, %lo(exc_depth)($k0)
	nop
	addiu	$k1, $k1, 1
	sw	$k1, %lo(exc_depth)($k0)	/* depth++ */
	addiu	$k1, $k1, -1
	sll	$k1, $k1, EXC_AREA_SHIFT
	lui	$k0, %hi(exc_area)
	addiu	$k0, $k0, %lo(exc_area)
	addu	$k0, $k0, $k1			/* k0 = this level's area */

	sw	$1, TF_REG(1)($k0)
	sw	$2, TF_REG(2)($k0)
	sw	$3, TF_REG(3)($k0)
	sw	$4, TF_REG(4)($k0)
	sw	$5, TF_REG(5)($k0)
	sw	$6, TF_REG(6)($k0)
	sw	$7, TF_REG(7)($k0)
	sw	$8, TF_REG(8)($k0)
	sw	$9, TF_REG(9)($k0)
	sw	$10, TF_REG(10)($k0)
	sw	$11, TF_REG(11)($k0)
	sw	$12, TF_REG(12)($k0)
	sw	$13, TF_REG(13)($k0)
	sw	$14, TF_REG(14)($k0)
	sw	$15, TF_REG(15)($k0)
	sw	$16, TF_REG(16)($k0)
	sw	$17, TF_REG(17)($k0)
	sw	$18, TF_REG(18)($k0)
	sw	$19, TF_REG(19)($k0)
	sw	$20, TF_REG(20)($k0)
	sw	$21, TF_REG(21)($k0)
	sw	$22, TF_REG(22)($k0)
	sw	$23, TF_REG(23)($k0)
	sw	$24, TF_REG(24)($k0)
	sw	$25, TF_REG(25)($k0)
	sw	$28, TF_REG(28)($k0)
	sw	$29, TF_REG(29)($k0)
	sw	$30, TF_REG(30)($k0)
	sw	$31, TF_REG(31)($k0)
	mfhi	$t0
	sw	$t0, TF_HI($k0)
	mflo	$t0
	sw	$t0, TF_LO($k0)
	mfc0	$t0, $14			/* EPC */
	nop
	sw	$t0, TF_EPC($k0)
	mfc0	$t0, $13			/* Cause */
	nop
	sw	$t0, TF_CAUSE($k0)
	mfc0	$t0, $12			/* Status */
	nop
	sw	$t0, TF_STATUS($k0)
	mfc0	$t0, $8				/* BadVAddr */
	nop
	sw	$t0, TF_BADVADDR($k0)
	lui	$t1, %hi(exc_vecnum)
	lw	$t0, %lo(exc_vecnum)($t1)
	nop
	sw	$t0, TF_VECTOR($k0)

	/* Leave exception level, keeping interrupts off, and call trap(). */
	move	$a0, $k0
	addiu	$sp, $k0, EXC_AREA - 32
	mfc0	$t0, $12
	nop
	li	$t1, ~(SR_EXL | SR_IE)
	and	$t0, $t0, $t1
	mtc0	$t0, $12
	nop
	nop
	nop
	jal	trap
	nop

	/*
	 * Back to exception level for the restore (the saved Status has
	 * EXL set), so nothing can interrupt us until eret clears it.
	 */
	move	$k0, $v0
	lw	$t0, TF_STATUS($k0)
	nop
	mtc0	$t0, $12
	nop
	nop
	nop
	lw	$t0, TF_EPC($k0)
	nop
	mtc0	$t0, $14
	lw	$t0, TF_HI($k0)
	nop
	mthi	$t0
	lw	$t0, TF_LO($k0)
	nop
	mtlo	$t0
	lui	$t1, %hi(exc_depth)
	lw	$t0, %lo(exc_depth)($t1)
	nop
	addiu	$t0, $t0, -1
	sw	$t0, %lo(exc_depth)($t1)	/* depth-- */

	lw	$1, TF_REG(1)($k0)
	lw	$2, TF_REG(2)($k0)
	lw	$3, TF_REG(3)($k0)
	lw	$4, TF_REG(4)($k0)
	lw	$5, TF_REG(5)($k0)
	lw	$6, TF_REG(6)($k0)
	lw	$7, TF_REG(7)($k0)
	lw	$8, TF_REG(8)($k0)
	lw	$9, TF_REG(9)($k0)
	lw	$10, TF_REG(10)($k0)
	lw	$11, TF_REG(11)($k0)
	lw	$12, TF_REG(12)($k0)
	lw	$13, TF_REG(13)($k0)
	lw	$14, TF_REG(14)($k0)
	lw	$15, TF_REG(15)($k0)
	lw	$16, TF_REG(16)($k0)
	lw	$17, TF_REG(17)($k0)
	lw	$18, TF_REG(18)($k0)
	lw	$19, TF_REG(19)($k0)
	lw	$20, TF_REG(20)($k0)
	lw	$21, TF_REG(21)($k0)
	lw	$22, TF_REG(22)($k0)
	lw	$23, TF_REG(23)($k0)
	lw	$24, TF_REG(24)($k0)
	lw	$25, TF_REG(25)($k0)
	lw	$28, TF_REG(28)($k0)
	lw	$29, TF_REG(29)($k0)
	lw	$30, TF_REG(30)($k0)
	lw	$31, TF_REG(31)($k0)
	eret
	.end	exc_common

/* Nested deeper than EXC_LEVELS: nothing sane left to do. */
exc_hang:
	b	exc_hang
	nop

/*
 * setjmp/longjmp: just the callee-saved registers, sp, ra and gp.
 */
	.globl	setjmp
	.ent	setjmp
setjmp:
	sw	$16, 0($a0)
	sw	$17, 4($a0)
	sw	$18, 8($a0)
	sw	$19, 12($a0)
	sw	$20, 16($a0)
	sw	$21, 20($a0)
	sw	$22, 24($a0)
	sw	$23, 28($a0)
	sw	$30, 32($a0)
	sw	$29, 36($a0)
	sw	$31, 40($a0)
	sw	$28, 44($a0)
	jr	$ra
	move	$v0, $zero
	.end	setjmp

	.globl	longjmp
	.ent	longjmp
longjmp:
	lw	$16, 0($a0)
	lw	$17, 4($a0)
	lw	$18, 8($a0)
	lw	$19, 12($a0)
	lw	$20, 16($a0)
	lw	$21, 20($a0)
	lw	$22, 24($a0)
	lw	$23, 28($a0)
	lw	$30, 32($a0)
	lw	$29, 36($a0)
	lw	$31, 40($a0)
	lw	$28, 44($a0)
	bnez	$a1, 1f
	move	$v0, $a1
	li	$v0, 1				/* longjmp(jb, 0) returns 1 */
1:	jr	$ra
	nop
	.end	longjmp

/* CP0 accessors not already in start.S */
	.globl	get_cause
	.ent	get_cause
get_cause:
	mfc0	$v0, $13
	nop
	jr	$ra
	nop
	.end	get_cause

	.globl	set_status
	.ent	set_status
set_status:
	mtc0	$a0, $12
	nop
	nop
	nop
	jr	$ra
	nop
	.end	set_status

/*
 * Exceptions on purpose, for "trap".  break and syscall return
 * normally when trap() resumes them; the others never return.
 */
	.globl	trap_break
	.ent	trap_break
trap_break:
	break	0
	jr	$ra
	nop
	.end	trap_break

	.globl	trap_syscall
	.ent	trap_syscall
trap_syscall:
	syscall
	jr	$ra
	nop
	.end	trap_syscall

	.globl	trap_ri
	.ent	trap_ri
trap_ri:
	.word	0x7c000000			/* opcode 31: reserved on R4000 */
	jr	$ra
	nop
	.end	trap_ri

	.globl	trap_overflow
	.ent	trap_overflow
trap_overflow:
	lui	$v0, 0x7fff
	ori	$v0, $v0, 0xffff
	addi	$v0, $v0, 1			/* 0x7fffffff + 1 traps */
	jr	$ra
	nop
	.end	trap_overflow
