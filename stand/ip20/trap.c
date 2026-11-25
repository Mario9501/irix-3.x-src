/*
 * trap.c
 *	Exception handling for ip20mon.  exc_install() points the R4000's
 *	exception vectors at our handlers, keeping the PROM's to put back,
 *	and trap() reports each exception.  Breakpoints and syscalls
 *	resume at the next instruction; anything else abandons the current
 *	command and returns to the ip20> prompt.
 */

#include "ip20.h"
#include "arcs.h"

struct trapframe {
	unsigned long	regs[32];
	unsigned long	hi, lo, epc, cause, status, badvaddr, vector, pad;
};

/* exc.S hard-codes these offsets */
typedef char tf_size_ok[sizeof (struct trapframe) == TF_SIZE ? 1 : -1];
typedef char tf_epc_ok[__builtin_offsetof(struct trapframe, epc) == TF_EPC ?
    1 : -1];

/* Used by exc.S */
unsigned long	exc_area[EXC_LEVELS][EXC_AREA / 4] __attribute__((aligned(8)));
volatile unsigned long	exc_depth;
volatile unsigned long	exc_vecnum;

jmp_buf		mon_jb;
int		mon_jb_set;

static struct trapframe	exc_last;
static int		have_last;

extern char vec_tlb[], vec_tlb_end[], vec_xtlb[], vec_xtlb_end[];
extern char vec_general[], vec_general_end[];

#define STUB_MAX	8		/* words saved per vector */

static struct vector {
	const char	*name;
	unsigned long	addr;
	char		*stub, *stub_end;
	unsigned long	saved[STUB_MAX];
} vectors[] = {
	{ "TLB refill",  VEC_TLB,     vec_tlb,     vec_tlb_end },
	{ "XTLB refill", VEC_XTLB,    vec_xtlb,    vec_xtlb_end },
	{ "general",     VEC_GENERAL, vec_general, vec_general_end },
};
#define NVECTORS	(sizeof vectors / sizeof vectors[0])

static unsigned long	saved_status;
static int		installed;

/*
 * Cache maintenance.  The stubs are stored through the write-back
 * D-cache, but instruction fetch reads through the I-cache from the
 * secondary cache or memory, so write the lines back out of the
 * D-cache (and the secondary, if there is one) and drop any stale
 * copies from the I-cache.
 */
#define HIT_INVALIDATE_I	0x10
#define HIT_WRITEBACK_INV_D	0x15
#define HIT_WRITEBACK_INV_SD	0x17

#define CACHE_OP(op, addr) \
	__asm__ volatile ("cache %0, 0(%1)" : : "i" (op), "r" (addr) : "memory")

static unsigned long	dline, iline, sline;	/* sline 0: no secondary */

static void
cache_lines(void)
{
	unsigned long cfg = get_config();

	dline = cfg & CFG_DB ? 32 : 16;
	iline = cfg & CFG_IB ? 32 : 16;
	sline = cfg & CFG_SC ? 0 : 16 << ((cfg >> CFG_SB_SHIFT) & CFG_SB_MASK);
}

static void
sync_icache(unsigned long start, unsigned long len)
{
	unsigned long a, end = start + len;

	for (a = start & ~(dline - 1); a < end; a += dline)
		CACHE_OP(HIT_WRITEBACK_INV_D, a);
	if (sline != 0)
		for (a = start & ~(sline - 1); a < end; a += sline)
			CACHE_OP(HIT_WRITEBACK_INV_SD, a);
	for (a = start & ~(iline - 1); a < end; a += iline)
		CACHE_OP(HIT_INVALIDATE_I, a);
}

static unsigned long
stub_len(struct vector *v)
{
	return v->stub_end - v->stub;
}

void
exc_install(void)
{
	struct vector *v;
	unsigned long n, *mem;
	int i, bad = 0;

	if (installed)
		return;
	saved_status = get_status();
	if (saved_status & SR_BEV) {
		printf("exceptions: Status.BEV is set, so the vectors are in the "
		    "PROM; not installing\n");
		return;
	}
	cache_lines();

	/* Interrupts off while we own the vectors; the PROM's console is polled. */
	set_status(saved_status & ~SR_IE);
	for (v = vectors; v < vectors + NVECTORS; v++) {
		n = stub_len(v);
		memcpy(v->saved, (void *)v->addr, n);
		memcpy((void *)v->addr, v->stub, n);
		sync_icache(v->addr, n);
	}
	installed = 1;

	/* Check that memory itself (read uncached) now holds the stubs. */
	for (v = vectors; v < vectors + NVECTORS; v++) {
		mem = (unsigned long *)(v->addr | K1BASE);
		for (i = 0; i < stub_len(v) / 4; i++)
			if (mem[i] != ((unsigned long *)v->stub)[i])
				bad = 1;
	}
	if (bad) {
		printf("exceptions: stubs didn't reach memory; putting the "
		    "PROM's vectors back\n");
		exc_uninstall();
		return;
	}
	printf("exceptions: handlers installed at %08x (TLB refill), %08x, "
	    "%08x (general)\n", VEC_TLB, VEC_XTLB, VEC_GENERAL);
}

void
exc_uninstall(void)
{
	struct vector *v;

	if (!installed)
		return;
	for (v = vectors; v < vectors + NVECTORS; v++) {
		memcpy((void *)v->addr, v->saved, stub_len(v));
		sync_icache(v->addr, stub_len(v));
	}
	set_status(saved_status);
	installed = 0;
}

void
exc_vectors(void)
{
	struct vector *v;
	struct arcs_spb *spb = arcs_spb();
	unsigned long i;

	printf("exception vectors: %s\n", installed ?
	    "ip20mon's handlers (the PROM's are saved)" : "the PROM's");
	for (v = vectors; v < vectors + NVECTORS; v++) {
		printf("  %-12s %08x  PROM's code:", v->name, v->addr);
		for (i = 0; i < stub_len(v) / 4; i++)
			printf(" %08x", installed ? v->saved[i] :
			    ((unsigned long *)v->addr)[i]);
		printf("\n");
	}
	if (spb != 0)
		printf("  ARCS GEVector %08x, UTLBMissVector %08x\n",
		    spb->ge_vector, spb->utlb_miss_vector);
	printf("  cache lines: D %u, I %u, secondary %u bytes\n",
	    dline, iline, sline);
}

/* ---------------------------------------------------------------- */

static const char *const excnames[32] = {
	[0] = "interrupt",
	[1] = "TLB modified",
	[2] = "TLB miss on load or fetch",
	[3] = "TLB miss on store",
	[4] = "address error on load or fetch",
	[5] = "address error on store",
	[6] = "bus error on instruction fetch",
	[7] = "bus error on data access",
	[8] = "syscall",
	[9] = "breakpoint",
	[10] = "reserved instruction",
	[11] = "coprocessor unusable",
	[12] = "integer overflow",
	[13] = "trap instruction",
	[14] = "virtual coherency (instruction)",
	[15] = "floating point",
	[23] = "watchpoint",
	[31] = "virtual coherency (data)",
};

static const char *const regnames[32] = {
	"zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
	"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra",
};

static const char *const vecnames[3] = {
	"TLB refill", "XTLB refill", "general",
};

static void
print_frame(struct trapframe *tf)
{
	unsigned code = (tf->cause >> CAUSE_EXC_SHIFT) & CAUSE_EXC_MASK;
	int i;

	printf("    epc %08x  badvaddr %08x  cause %08x  status %08x\n",
	    tf->epc, tf->badvaddr, tf->cause, tf->status);
	printf("    vector: %s%s",
	    tf->vector < 3 ? vecnames[tf->vector] : "?",
	    tf->cause & CAUSE_BD ? ", in a branch delay slot" : "");
	if (code == EXC_CPU)
		printf(", coprocessor %u",
		    (tf->cause >> CAUSE_CE_SHIFT) & CAUSE_CE_MASK);
	if (code == EXC_INT)
		printf(", pending IP %02x", (tf->cause & CAUSE_IP_MASK) >> 8);
	printf("\n");
	for (i = 1; i < 32; i++) {
		if (i == 26 || i == 27)
			printf("    %4s --------", regnames[i]);
		else
			printf("    %4s %08x", regnames[i], tf->regs[i]);
		if (i % 4 == 3)
			printf("\n");
	}
	printf("      hi %08x      lo %08x\n", tf->hi, tf->lo);
}

/* Report and clear the MC's latched bus error, if any. */
static void
mc_buserr(void)
{
	if (!ip20_hw_ok())
		return;
	printf("    MC: CPU error addr %08x status %08x, GIO error addr %08x "
	    "status %08x\n", REG32(MC_CPU_ERRADDR), REG32(MC_CPU_ERRSTAT),
	    REG32(MC_GIO_ERRADDR), REG32(MC_GIO_ERRSTAT));
	REG32(MC_CPU_ERRSTAT) = 0;
	REG32(MC_GIO_ERRSTAT) = 0;
}

/* Where an abandoned command lands, out of exception level. */
static void
trap_recover(void)
{
	exc_depth = 0;
	if (mon_jb_set)
		longjmp(mon_jb, 1);
	printf("no monitor to return to; halted\n");
	for (;;)
		;
}

/* Called from exc_common with the saved state; returns the frame to resume. */
struct trapframe *
trap(struct trapframe *tf)
{
	unsigned code = (tf->cause >> CAUSE_EXC_SHIFT) & CAUSE_EXC_MASK;
	unsigned long iaddr, insn = 0;
	int have_insn = 0;

	if (exc_depth > 1)
		printf("\n*** exception while handling an exception");
	printf("\n*** %s exception\n",
	    excnames[code] ? excnames[code] : "unknown");

	/*
	 * The instruction is readable for exceptions it raised itself.  In
	 * a branch delay slot EPC points at the branch, one word earlier.
	 */
	iaddr = tf->epc + (tf->cause & CAUSE_BD ? 4 : 0);
	if (code == EXC_SYS || code == EXC_BP || code == EXC_RI ||
	    code == EXC_CPU || code == EXC_OV || code == EXC_TR) {
		insn = *(unsigned long *)iaddr;
		have_insn = 1;
	}
	print_frame(tf);
	if (have_insn) {
		printf("    instruction %08x at %08x", insn, iaddr);
		if (code == EXC_TR && (insn & 0xfc00003f) == 0x00000034 &&
		    ((insn >> 6) & 0x3ff) == 7)
			printf(" (teq ..., 7: integer divide by zero)");
		printf("\n");
	}
	if (code == EXC_IBE || code == EXC_DBE)
		mc_buserr();

	exc_last = *tf;
	have_last = 1;

	if ((code == EXC_BP || code == EXC_SYS) && !(tf->cause & CAUSE_BD)) {
		printf("    resuming after it\n");
		tf->epc += 4;
		return tf;
	}
	printf("    abandoning the command\n");
	tf->epc = (unsigned long)trap_recover;
	tf->regs[29] = (unsigned long)tf + EXC_AREA - 32;
	return tf;
}

void
exc_regs(void)
{
	if (!have_last) {
		printf("no exception taken yet\n");
		return;
	}
	printf("last exception: %s\n", excnames[(exc_last.cause >>
	    CAUSE_EXC_SHIFT) & CAUSE_EXC_MASK]);
	print_frame(&exc_last);
}

/* ---------------------------------------------------------------- */

static void
trap_div0(void)
{
	volatile int a = 1, b = 0;

	a = a / b;
}

static void
trap_unaligned(void)
{
	volatile unsigned long addr = PHYS_TO_K0(0x08002001);

	(void)*(volatile unsigned long *)addr;
}

static void
trap_null(void)
{
	volatile unsigned long addr = 0;

	(void)*(volatile unsigned long *)addr;
}

static void
trap_bus(void)
{
	unsigned long end = mc_mem_end();

	if (!ip20_hw_ok() || end == 0 || end >= 0x1f000000) {
		printf("can't find a safe address past the end of memory\n");
		return;
	}
	printf("reading physical %08x, just past the last memory bank\n", end);
	(void)REG32(end);
}

static const struct trapkind {
	const char	*name;
	void		(*fn)(void);
	const char	*what;
} kinds[] = {
	{ "break",	trap_break,	"break instruction (resumes)" },
	{ "syscall",	trap_syscall,	"syscall instruction (resumes)" },
	{ "ri",		trap_ri,	"reserved instruction" },
	{ "overflow",	trap_overflow,	"signed add that overflows" },
	{ "div0",	trap_div0,	"divide by zero (gcc's teq check)" },
	{ "unaligned",	trap_unaligned,	"word load from an odd address" },
	{ "null",	trap_null,	"load through a null pointer (TLB miss)" },
	{ "bus",	trap_bus,	"load past the end of memory (bus error)" },
	{ 0 }
};

void
trap_test(const char *kind)
{
	const struct trapkind *k;

	for (k = kinds; k->name != 0; k++)
		if (strcmp(kind, k->name) == 0)
			break;
	if (k->name == 0) {
		printf("usage: trap KIND, where KIND is one of:\n");
		for (k = kinds; k->name != 0; k++)
			printf("  %-10s %s\n", k->name, k->what);
		return;
	}
	if (!installed) {
		printf("ip20mon's exception handlers aren't installed\n");
		return;
	}
	k->fn();
	printf("back from the %s test\n", k->name);
}
