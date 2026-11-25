/*
 * ip20.h
 *	Hardware definitions for ip20mon, the standalone bring-up program for
 *	the IP20 (Indigo R4000/R4400).  Addresses are physical; use PHYS_TO_K0
 *	for cached access to memory and PHYS_TO_K1 for device registers.
 *
 *	Where these come from:
 *	  HPC1.5, Z85C30, MC	NetBSD sys/arch/sgimips (hpc/hpcreg.h, dev/zs.c,
 *				dev/imcreg.h, sgimips/machdep.c) and Linux
 *				arch/mips/include/asm/sgi/mc.h
 *	  CP0 registers		MIPS R4000 Microprocessor User's Manual
 */

#ifndef IP20_H
#define IP20_H

#define K0BASE		0x80000000	/* cached, unmapped */
#define K1BASE		0xa0000000	/* uncached, unmapped */
#define PHYS_TO_K0(pa)	((unsigned long)(pa) | K0BASE)
#define PHYS_TO_K1(pa)	((unsigned long)(pa) | K1BASE)

#define REG8(pa)	(*(volatile unsigned char *)PHYS_TO_K1(pa))
#define REG32(pa)	(*(volatile unsigned long *)PHYS_TO_K1(pa))

/*
 * MC memory controller.  The MEMCFG decode is the one Linux uses for the
 * same chip on IP22; it has not been checked against an IP20.
 */
#define MC_BASE		0x1fa00000
#define MC_SYSID	(MC_BASE + 0x1c)
#define MC_MEMCFG0	(MC_BASE + 0xc4)	/* banks 0 (high half), 1 (low) */
#define MC_MEMCFG1	(MC_BASE + 0xcc)	/* banks 2 (high half), 3 (low) */

/* Bus-error latches; write 0 to the status registers to clear them. */
#define MC_CPU_ERRADDR	(MC_BASE + 0xe4)
#define MC_CPU_ERRSTAT	(MC_BASE + 0xec)
#define MC_GIO_ERRADDR	(MC_BASE + 0xf4)
#define MC_GIO_ERRSTAT	(MC_BASE + 0xfc)

#define MC_SYSID_REV	0x0000000f
#define MC_MEMCFG_BASE	0x000000ff
#define MC_MEMCFG_SIZE	0x00001f00
#define MC_MEMCFG_VALID	0x00002000

/* IP20 board revision, bits 14:12 (from NetBSD sgimips machdep.c) */
#define IP20_BOARDREV		0x1fbd0000
#define IP20_BOARDREV_MASK	0x00007000
#define IP20_BOARDREV_SHIFT	12

/*
 * HPC1.5 peripheral controller and the Z85C30 serial chips behind it.
 * Each chip has channel B at +0 and channel A at +8; within a channel
 * the control register is byte 3 and the data register byte 7.
 * The rear serial ports are on the chip at ZS1: port 1 is channel B,
 * port 2 is channel A.  The chip at ZS0 runs the keyboard and mouse.
 */
#define HPC_BASE	0x1fb80000
#define ZS0_BASE	(HPC_BASE + 0x0d00)
#define ZS1_BASE	(HPC_BASE + 0x0d10)
#define ZS_CHAN_B	0
#define ZS_CHAN_A	8
#define ZS_CSR		3
#define ZS_DATA		7

#define ZSRR0_RX_READY	0x01
#define ZSRR0_TX_READY	0x04

/* CP0 PRId */
#define PRID_IMP(p)	(((p) >> 8) & 0xff)
#define PRID_REV(p)	((p) & 0xff)
#define PRID_IMP_R4000	0x04		/* R4000 and R4400 */
#define PRID_REV_R4400	0x40		/* revision 4.0 and up is an R4400 */

/* CP0 Config */
#define CFG_K0_MASK	0x00000007	/* kseg0 cache algorithm */
#define CFG_DB		0x00000010	/* D-cache lines are 32 bytes, not 16 */
#define CFG_IB		0x00000020	/* I-cache lines are 32 bytes, not 16 */
#define CFG_DC_SHIFT	6		/* D-cache is 4K << DC */
#define CFG_IC_SHIFT	9		/* I-cache is 4K << IC */
#define CFG_CSIZE_MASK	0x7
#define CFG_BE		0x00008000	/* big-endian */
#define CFG_SC		0x00020000	/* set when there is NO secondary cache */
#define CFG_SB_SHIFT	22		/* secondary line is 16 bytes << SB */
#define CFG_SB_MASK	0x3

/* CP0 Status */
#define SR_IE		0x00000001
#define SR_EXL		0x00000002
#define SR_ERL		0x00000004
#define SR_KSU_SHIFT	3
#define SR_KSU_MASK	0x3
#define SR_UX		0x00000020
#define SR_SX		0x00000040
#define SR_KX		0x00000080
#define SR_IMASK	0x0000ff00
#define SR_BEV		0x00400000
#define SR_FR		0x04000000
#define SR_CU0		0x10000000
#define SR_CU1		0x20000000

/* CP0 Cause */
#define CAUSE_BD	0x80000000	/* exception in a branch delay slot */
#define CAUSE_CE_SHIFT	28		/* coprocessor, for CpU */
#define CAUSE_CE_MASK	0x3
#define CAUSE_IP_MASK	0x0000ff00
#define CAUSE_EXC_SHIFT	2
#define CAUSE_EXC_MASK	0x1f

#define EXC_INT		0
#define EXC_MOD		1		/* store to a clean (D=0) page */
#define EXC_TLBL	2
#define EXC_TLBS	3
#define EXC_IBE		6
#define EXC_DBE		7
#define EXC_SYS		8
#define EXC_BP		9
#define EXC_RI		10
#define EXC_CPU		11
#define EXC_OV		12
#define EXC_TR		13

/* Exception vectors when Status.BEV is clear */
#define VEC_TLB		0x80000000	/* TLB refill, 32-bit addressing */
#define VEC_XTLB	0x80000080	/* TLB refill, 64-bit addressing */
#define VEC_GENERAL	0x80000180	/* everything else */

/*
 * Trap frame layout, shared by exc.S and trap.c.  Each nesting level
 * gets an EXC_AREA-byte area: the frame at the bottom, stack above it.
 */
#define TF_REG(n)	((n) * 4)
#define TF_HI		128
#define TF_LO		132
#define TF_EPC		136
#define TF_CAUSE	140
#define TF_STATUS	144
#define TF_BADVADDR	148
#define TF_VECTOR	152
#define TF_SIZE		160

#define EXC_AREA_SHIFT	12
#define EXC_AREA	(1 << EXC_AREA_SHIFT)
#define EXC_LEVELS	2

/*
 * TLB.  The R4000 has 48 entries, each mapping an even/odd pair of
 * pages: EntryHi holds the virtual page-pair number (VPN2) and ASID,
 * EntryLo0/1 the physical frame and flags for each half.
 */
#define TLB_ENTRIES	48
#define PAGE_SHIFT	12
#define PAGE_SIZE	(1 << PAGE_SHIFT)

#define EL_G		0x00000001	/* global: ignore ASID (set in both) */
#define EL_V		0x00000002	/* valid */
#define EL_D		0x00000004	/* dirty: writes allowed */
#define EL_C_SHIFT	3
#define EL_C_UNCACHED	(2 << EL_C_SHIFT)
#define EL_C_CACHED	(3 << EL_C_SHIFT)	/* noncoherent write-back */
#define EL_PFN_SHIFT	6
#define EL_PFN_MASK	0x00ffffff
#define ENTRYLO(pa, flags) \
	((((pa) >> PAGE_SHIFT) << EL_PFN_SHIFT) | (flags))
#define ENTRYLO_PA(lo) \
	((((lo) >> EL_PFN_SHIFT) & EL_PFN_MASK) << PAGE_SHIFT)

#define EH_VPN2_MASK	0xffffe000
#define EH_ASID_MASK	0x000000ff
#define INDEX_P		0x80000000	/* tlbp found no match */

/*
 * ip20mon's page table: one EntryLo pair (8 bytes) per 8K of kuseg,
 * covering the low 64 MB.  The refill handler in exc.S indexes it
 * directly; misses above that go to trap().
 */
#define PT_SPAN_SHIFT	26
#define PT_PAIRS	(1 << (PT_SPAN_SHIFT - PAGE_SHIFT - 1))

#ifndef __ASSEMBLER__

/* start.S */
unsigned long	get_prid(void);
unsigned long	get_config(void);
unsigned long	get_status(void);
unsigned long	get_count(void);

/* exc.S */
typedef unsigned long jmp_buf[12];
int	setjmp(jmp_buf) __attribute__((returns_twice));
void	longjmp(jmp_buf, int) __attribute__((noreturn));
unsigned long	get_cause(void);
void	set_status(unsigned long);
void	trap_break(void);
void	trap_syscall(void);
void	trap_ri(void);
void	trap_overflow(void);

/* tlb.S */
struct tlbent {
	unsigned long	hi, lo0, lo1, mask;	/* tlb.S uses these offsets */
};
void	tlb_read(int index, struct tlbent *);
void	tlb_write(int index, const struct tlbent *);
void	tlb_update(unsigned long hi, unsigned long lo0, unsigned long lo1);
int	tlb_probe(unsigned long hi);
unsigned long	get_entryhi(void);
void	set_entryhi(unsigned long);
unsigned long	get_pagemask(void);
void	set_pagemask(unsigned long);
unsigned long	get_wired(void);
void	set_wired(unsigned long);

/* trap.c */
extern jmp_buf	mon_jb;
extern int	mon_jb_set;
extern unsigned	badaddr_code;
void	exc_install(void);
void	exc_uninstall(void);
void	exc_vectors(void);
void	exc_regs(void);
void	trap_test(const char *kind);
void	cache_wbinv(unsigned long start, unsigned long len);
int	badaddr(unsigned long addr, int write, unsigned long *val);

/* vm.c */
void	vm_save(void);
void	vm_restore(void);
int	vm_fault(unsigned long va, int write);
int	vm_modfault(unsigned long va);
void	tlb_dump(void);
void	vm_test(void);

/* main.c */
int	ip20_hw_ok(void);

/* cons.c */
void	cons_init(void);
int	putchar(int);
int	getchar(void);
int	printf(const char *, ...);

/* zs.c */
void	zs_putc(int);
int	zs_getc(void);

/* cpu.c */
void	cpu_info(void);
void	clock_info(void);

/* mem.c */
void	mem_info(int probe_mc);
unsigned long	mc_mem_end(void);

/* subr.c */
void	*memset(void *, int, unsigned long);
void	*memcpy(void *, const void *, unsigned long);
unsigned long	strlen(const char *);
int	strcmp(const char *, const char *);

#endif /* __ASSEMBLER__ */
#endif /* IP20_H */
