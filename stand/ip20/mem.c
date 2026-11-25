/*
 * mem.c
 *	Report memory two ways: the PROM's ARCS memory descriptors, and the
 *	bank registers in the MC memory controller.  The MC decode is the
 *	one Linux uses for the same chip on IP22 and is unverified on IP20;
 *	the two reports agreeing is the check.
 */

#include "ip20.h"
#include "arcs.h"

#define MAXDESC		64	/* guard against a looping descriptor list */

static const char *const memtypes[ARCS_MEM_NTYPES] = {
	"exception block", "parameter block", "free (contiguous)", "free",
	"bad", "loaded program", "firmware temporary", "firmware permanent",
};

static void
arcs_mem(void)
{
	struct arcs_memdesc *m;
	unsigned long kb[ARCS_MEM_NTYPES], total = 0, base, size;
	int i, n = 0;

	for (i = 0; i < ARCS_MEM_NTYPES; i++)
		kb[i] = 0;

	printf("memory, as reported by the PROM:\n");
	for (m = arcs_getmemdesc(0); m != 0 && n < MAXDESC;
	    m = arcs_getmemdesc(m), n++) {
		if (m->page_count == 0)
			continue;
		base = m->base_page << ARCS_PAGESHIFT;
		size = m->page_count << (ARCS_PAGESHIFT - 10);	/* in K */
		printf("  %08x-%08x %7uK  %s\n", base,
		    base + (m->page_count << ARCS_PAGESHIFT) - 1, size,
		    m->type < ARCS_MEM_NTYPES ? memtypes[m->type] : "unknown");
		if (m->type < ARCS_MEM_NTYPES)
			kb[m->type] += size;
		if (m->type != ARCS_MEM_BAD)
			total += size;
	}
	if (n == 0) {
		printf("  (no descriptors)\n");
		return;
	}
	printf("  total %uK, free %uK, bad %uK\n", total,
	    kb[ARCS_MEM_FREECONTIG] + kb[ARCS_MEM_FREE], kb[ARCS_MEM_BAD]);
}

static void
mc_mem(void)
{
	unsigned long rev = REG32(MC_SYSID) & MC_SYSID_REV;
	unsigned long cfg, half, base, size, total = 0;
	int bank, ashift = rev >= 5 ? 24 : 22, sshift = rev >= 5 ? 16 : 14;

	printf("memory, as configured in the MC (revision %u):\n", rev);
	for (bank = 0; bank < 4; bank++) {
		cfg = REG32(bank < 2 ? MC_MEMCFG0 : MC_MEMCFG1);
		half = (bank & 1) ? cfg & 0xffff : cfg >> 16;
		if ((half & MC_MEMCFG_VALID) == 0) {
			printf("  bank %d: empty\n", bank);
			continue;
		}
		base = (half & MC_MEMCFG_BASE) << ashift;
		size = ((half & MC_MEMCFG_SIZE) + 0x100) << sshift;
		printf("  bank %d: %08x-%08x %7uK\n", bank, base,
		    base + size - 1, size >> 10);
		total += size >> 10;
	}
	printf("  total %uK\n", total);
}

void
mem_info(int probe_mc)
{
	if (arcs_present())
		arcs_mem();
	if (probe_mc)
		mc_mem();
}
