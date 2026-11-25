/*
 * cpu.c
 *	Identify the R4000/R4400, decode its caches and status register,
 *	and measure the clock by timing the CP0 Count register against the
 *	PROM's seconds counter.
 */

#include "ip20.h"
#include "arcs.h"

static const char *const k0modes[8] = {
	"reserved", "reserved", "uncached", "cached noncoherent",
	"coherent exclusive", "coherent excl. on write",
	"coherent update on write", "reserved",
};

static const char *const ksumodes[4] = {
	"kernel", "supervisor", "user", "invalid",
};

void
cpu_info(void)
{
	unsigned long prid = get_prid();
	unsigned long sr = get_status();
	unsigned long cfg;
	unsigned imp = PRID_IMP(prid), rev = PRID_REV(prid);

	if (imp != PRID_IMP_R4000) {
		printf("cpu: unknown, PRId %08x (expected an R4000 or R4400)\n",
		    prid);
		return;
	}
	printf("cpu: %s revision %u.%u (PRId %08x)\n",
	    rev >= PRID_REV_R4400 ? "R4400" : "R4000",
	    rev >> 4, rev & 0xf, prid);

	cfg = get_config();
	printf("  primary I-cache %uK, %u-byte lines\n",
	    4 << ((cfg >> CFG_IC_SHIFT) & CFG_CSIZE_MASK),
	    cfg & CFG_IB ? 32 : 16);
	printf("  primary D-cache %uK, %u-byte lines\n",
	    4 << ((cfg >> CFG_DC_SHIFT) & CFG_CSIZE_MASK),
	    cfg & CFG_DB ? 32 : 16);
	if (cfg & CFG_SC)
		printf("  no secondary cache (PC part)\n");
	else
		printf("  secondary cache, %u-byte lines (SC part)\n",
		    16 << ((cfg >> CFG_SB_SHIFT) & CFG_SB_MASK));
	printf("  kseg0 mode %u (%s), %s-endian, Config %08x\n",
	    cfg & CFG_K0_MASK, k0modes[cfg & CFG_K0_MASK],
	    cfg & CFG_BE ? "big" : "little", cfg);

	printf("  Status %08x: %s mode%s%s%s%s%s%s, IM %02x\n", sr,
	    ksumodes[(sr >> SR_KSU_SHIFT) & SR_KSU_MASK],
	    sr & SR_IE ? ", IE" : "",
	    sr & SR_EXL ? ", EXL" : "",
	    sr & SR_ERL ? ", ERL" : "",
	    sr & SR_BEV ? ", BEV" : "",
	    sr & SR_CU1 ? ", CU1" : "",
	    sr & SR_FR ? ", FR" : "",
	    (sr & SR_IMASK) >> 8);
}

/* Give up waiting for the PROM's clock after 2^28 Count ticks (several s). */
#define COUNT_GIVEUP	0x10000000

/* Count ticks per second, or 0 if the PROM can't tell us the time. */
static unsigned long
count_rate(void)
{
	unsigned long t, start;

	if (!arcs_avail(FV_GETRELTIME))
		return 0;

	/* Line up on the edge of a second, then count for one second. */
	start = get_count();
	t = arcs_reltime();
	while (arcs_reltime() == t)
		if (get_count() - start > COUNT_GIVEUP)
			return 0;

	start = get_count();
	t = arcs_reltime();
	while (arcs_reltime() == t)
		if (get_count() - start > COUNT_GIVEUP)
			return 0;
	return get_count() - start;
}

void
clock_info(void)
{
	unsigned long hz;

	printf("clock: measuring against the PROM's clock...\n");
	hz = count_rate();
	if (hz == 0) {
		printf("  can't measure: the PROM's clock isn't available\n");
		return;
	}
	/* Count advances once every two pipeline clocks. */
	printf("  Count runs at %u.%02u MHz, so the CPU runs at about %u MHz\n",
	    hz / 1000000, (hz / 10000) % 100, (hz * 2 + 500000) / 1000000);
}
