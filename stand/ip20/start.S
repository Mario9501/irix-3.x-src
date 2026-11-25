/*
 * start.S
 *	Entry point for ip20mon.  The IP20 PROM loads us at 0x88002000 and
 *	jumps here in kernel mode with the caches and TLB already set up,
 *	a0 = argc, a1 = argv, a2 = envp, and ra pointing back into the PROM.
 *	We clear BSS, switch to our own stack, call main(), and return to
 *	the PROM on the PROM's stack when main() returns.
 */

#define STACKSIZE	16384

	.set	noreorder
	.text

	.globl	start
	.ent	start
start:
	/* Remember how to get back to the PROM. */
	la	$t0, prom_sp
	sw	$sp, 0($t0)
	la	$t0, prom_ra
	sw	$ra, 0($t0)

	/*
	 * Clear BSS.  Our stack is in BSS too, so we stay on the PROM's
	 * stack until this is done.  The linker script aligns both ends.
	 */
	la	$t0, _fbss
	la	$t1, _end
1:	sltu	$t2, $t0, $t1
	beqz	$t2, 2f
	nop
	sw	$zero, 0($t0)
	b	1b
	addiu	$t0, $t0, 4
2:
	/* Our own stack, leaving the o32 argument save area. */
	la	$sp, stack + STACKSIZE - 32

	jal	main			/* a0-a2 still hold argc/argv/envp */
	nop

	/* Back to the PROM; main()'s return value is in v0. */
	la	$t0, prom_sp
	lw	$sp, 0($t0)
	la	$t0, prom_ra
	lw	$ra, 0($t0)
	jr	$ra
	nop
	.end	start

/*
 * CP0 accessors.  The nop after each mfc0 covers the R4000's CP0 hazard
 * before the value is used.
 */
	.globl	get_prid
	.ent	get_prid
get_prid:
	mfc0	$v0, $15
	nop
	jr	$ra
	nop
	.end	get_prid

	.globl	get_config
	.ent	get_config
get_config:
	mfc0	$v0, $16
	nop
	jr	$ra
	nop
	.end	get_config

	.globl	get_status
	.ent	get_status
get_status:
	mfc0	$v0, $12
	nop
	jr	$ra
	nop
	.end	get_status

	.globl	get_count
	.ent	get_count
get_count:
	mfc0	$v0, $9
	nop
	jr	$ra
	nop
	.end	get_count

	.data
	.align	2
prom_sp:
	.word	0
prom_ra:
	.word	0

	.bss
	.align	4
stack:
	.space	STACKSIZE
