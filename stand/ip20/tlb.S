/*
 * tlb.S
 *	R4000 TLB primitives for ip20mon.  tlbr, tlbwi, tlbwr and tlbp read
 *	or write the CP0 registers Index (0), EntryLo0 (2), EntryLo1 (3),
 *	PageMask (5) and EntryHi (10), and the R4000 doesn't interlock on
 *	them, so each step is padded with nops (HAZ).  Note that tlbr also
 *	overwrites EntryHi (and so the current ASID) and PageMask; callers
 *	that care save and restore them.
 */

#include "ip20.h"

#define HAZ	nop; nop; nop; nop

	.set	noreorder
	.text

/* void tlb_read(int index, struct tlbent *e) */
	.globl	tlb_read
	.ent	tlb_read
tlb_read:
	mtc0	$a0, $0
	HAZ
	tlbr
	HAZ
	mfc0	$t0, $10
	mfc0	$t1, $2
	mfc0	$t2, $3
	mfc0	$t3, $5
	sw	$t0, 0($a1)
	sw	$t1, 4($a1)
	sw	$t2, 8($a1)
	jr	$ra
	sw	$t3, 12($a1)
	.end	tlb_read

/* void tlb_write(int index, const struct tlbent *e) */
	.globl	tlb_write
	.ent	tlb_write
tlb_write:
	lw	$t0, 0($a1)
	lw	$t1, 4($a1)
	lw	$t2, 8($a1)
	lw	$t3, 12($a1)
	mtc0	$a0, $0
	mtc0	$t0, $10
	mtc0	$t1, $2
	mtc0	$t2, $3
	mtc0	$t3, $5
	HAZ
	tlbwi
	HAZ
	jr	$ra
	nop
	.end	tlb_write

/* int tlb_probe(unsigned long entryhi): the matching index, or -1 */
	.globl	tlb_probe
	.ent	tlb_probe
tlb_probe:
	mtc0	$a0, $10
	HAZ
	tlbp
	HAZ
	mfc0	$v0, $0
	nop
	bltz	$v0, 1f			/* Index.P set: no match */
	nop
	jr	$ra
	nop
1:	jr	$ra
	li	$v0, -1
	.end	tlb_probe

/*
 * void tlb_update(unsigned long hi, unsigned long lo0, unsigned long lo1)
 * Replace the entry matching hi, or write a new one at a random
 * (non-wired) index.  Always uses 4K pages.
 */
	.globl	tlb_update
	.ent	tlb_update
tlb_update:
	mtc0	$a0, $10
	mtc0	$zero, $5
	HAZ
	tlbp
	HAZ
	mfc0	$t0, $0
	mtc0	$a1, $2
	mtc0	$a2, $3
	HAZ
	bltz	$t0, 1f
	nop
	tlbwi
	b	2f
	nop
1:	tlbwr
2:	HAZ
	jr	$ra
	nop
	.end	tlb_update

/* CP0 accessors */
#define GETSET(get, set, reg)	\
	.globl	get, set;	\
	.ent	get;		\
get:	mfc0	$v0, reg;	\
	nop;			\
	jr	$ra;		\
	nop;			\
	.end	get;		\
	.ent	set;		\
set:	mtc0	$a0, reg;	\
	HAZ;			\
	jr	$ra;		\
	nop;			\
	.end	set

	GETSET(get_entryhi, set_entryhi, $10)
	GETSET(get_pagemask, set_pagemask, $5)
	GETSET(get_wired, set_wired, $6)
