/*
 * vm.c
 *	R4000 TLB management for ip20mon: a page table for the low 64 MB of
 *	kuseg that the refill handler in exc.S reads, demand-zero paging,
 *	dirty tracking through TLB-modified faults, and a wired entry, with
 *	the "tlb" and "vm" commands that show and exercise them.
 *
 *	The R4000SC's primary caches are virtually indexed, and the
 *	secondary cache remembers which virtual index each line was used
 *	with.  Using one physical page at two virtual "colors" raises
 *	virtual coherency exceptions, so pages are allocated with the same
 *	color (address bits 14:12) as the virtual address they're mapped
 *	at, and kseg0, which uses the physical address as the virtual
 *	address, then agrees too.
 */

#include "ip20.h"
#include "arcs.h"

/* Read by the refill handler in exc.S */
unsigned long		pagetable[PT_PAIRS][2] __attribute__((aligned(8)));
volatile unsigned long	tlb_refills;

static unsigned long	vm_faults, mod_faults;

/* The PROM's TLB state, put back by vm_restore() */
static struct tlbent	saved_tlb[TLB_ENTRIES];
static unsigned long	saved_wired, saved_entryhi, saved_pagemask;
static int		saved;

/*
 * Demand-paged regions of kuseg, live only while "vm" runs.  Pages
 * read first are mapped clean; the first write takes a TLB-modified
 * fault that marks them dirty, if the region is writable.
 */
#define RW_BASE		0x00400000
#define RW_SIZE		0x00400000
#define RO_BASE		0x00c00000
#define RO_SIZE		0x00010000
#define WIRED_VA	0xc0000000	/* kseg2, for the wired-entry test */

static const struct region {
	unsigned long	start, size;
	int		writable;
} regions[] = {
	{ RW_BASE, RW_SIZE, 1 },
	{ RO_BASE, RO_SIZE, 0 },
};
#define NREGIONS	(sizeof regions / sizeof regions[0])

/* Physical page pool, carved from the PROM's largest free region */
#define POOL_SIZE	0x00200000
#define COLORS		8
#define COLOR(a)	(((a) >> PAGE_SHIFT) & (COLORS - 1))

static unsigned long	pool_base, pool_next[COLORS];
static int		pool_ready;

static int
pool_init(void)
{
	struct arcs_memdesc *m;
	unsigned long base, size, best = 0, bestsize = 0;
	int c, n = 0;

	for (m = arcs_getmemdesc(0); m != 0 && n < 64;
	    m = arcs_getmemdesc(m), n++) {
		if (m->type != ARCS_MEM_FREE && m->type != ARCS_MEM_FREECONTIG)
			continue;
		base = m->base_page << ARCS_PAGESHIFT;
		size = m->page_count << ARCS_PAGESHIFT;
		if (size > bestsize) {
			best = base;
			bestsize = size;
		}
	}
	/* Align to COLORS pages so each color's pages are evenly spaced. */
	base = (best + COLORS * PAGE_SIZE - 1) & ~(COLORS * PAGE_SIZE - 1);
	if (bestsize < POOL_SIZE + COLORS * PAGE_SIZE)
		return 0;
	pool_base = base;
	for (c = 0; c < COLORS; c++)
		pool_next[c] = base + c * PAGE_SIZE;
	pool_ready = 1;
	return 1;
}

/* A free physical page with the same color as va, or 0. */
static unsigned long
page_alloc(unsigned long va)
{
	unsigned long pa = pool_next[COLOR(va)];

	if (pa >= pool_base + POOL_SIZE)
		return 0;
	pool_next[COLOR(va)] += COLORS * PAGE_SIZE;
	return pa;
}

static const struct region *
find_region(unsigned long va)
{
	const struct region *r;

	if (!pool_ready)
		return 0;
	for (r = regions; r < regions + NREGIONS; r++)
		if (va >= r->start && va - r->start < r->size)
			return r;
	return 0;
}

static unsigned long *
pte_of(unsigned long va)
{
	return &pagetable[va >> (PAGE_SHIFT + 1)][(va >> PAGE_SHIFT) & 1];
}

/* Load va's page pair from the page table into the TLB. */
static void
tlb_reload(unsigned long va)
{
	unsigned long *pair = pagetable[va >> (PAGE_SHIFT + 1)];

	tlb_update((va & EH_VPN2_MASK) | (get_entryhi() & EH_ASID_MASK),
	    pair[0], pair[1]);
}

/*
 * TLB miss or invalid entry at va.  Page in a zeroed page if va is in a
 * live region; returns 0 if it isn't ours to fix.
 */
int
vm_fault(unsigned long va, int write)
{
	const struct region *r = find_region(va);
	unsigned long *pte, pa;

	if (r == 0)
		return 0;
	pte = pte_of(va);
	if ((*pte & EL_V) == 0) {
		if ((pa = page_alloc(va)) == 0)
			return 0;
		memset((void *)PHYS_TO_K0(pa), 0, PAGE_SIZE);
		cache_wbinv(PHYS_TO_K0(pa), PAGE_SIZE);
		*pte = ENTRYLO(pa, EL_C_CACHED | EL_V |
		    (write && r->writable ? EL_D : 0));
		vm_faults++;
	}
	tlb_reload(va);
	return 1;
}

/* Store to a clean page: mark it dirty if the region allows writes. */
int
vm_modfault(unsigned long va)
{
	const struct region *r = find_region(va);
	unsigned long *pte;

	if (r == 0 || !r->writable)
		return 0;
	pte = pte_of(va);
	if ((*pte & EL_V) == 0)
		return 0;
	*pte |= EL_D;
	mod_faults++;
	tlb_reload(va);
	return 1;
}

/* Fill entries first..47 with unique kseg0 tags that can never match. */
static void
tlb_invalidate_from(int first)
{
	struct tlbent e;
	int i;

	for (i = first; i < TLB_ENTRIES; i++) {
		e.hi = K0BASE + (i << (PAGE_SHIFT + 1));
		e.lo0 = e.lo1 = e.mask = 0;
		tlb_write(i, &e);
	}
}

void
vm_save(void)
{
	int i;

	saved_entryhi = get_entryhi();
	saved_pagemask = get_pagemask();
	saved_wired = get_wired();
	for (i = 0; i < TLB_ENTRIES; i++)
		tlb_read(i, &saved_tlb[i]);
	set_entryhi(saved_entryhi);
	set_pagemask(saved_pagemask);
	saved = 1;
}

/* Drop ip20mon's mappings and put the PROM's TLB back. */
void
vm_restore(void)
{
	unsigned long pa;
	int i;

	if (pool_ready) {
		/* Write the pages back and out of the caches, via kseg0. */
		for (i = 0; i < PT_PAIRS * 2; i++)
			if (pagetable[i / 2][i % 2] & EL_V) {
				pa = ENTRYLO_PA(pagetable[i / 2][i % 2]);
				cache_wbinv(PHYS_TO_K0(pa), PAGE_SIZE);
			}
		memset(pagetable, 0, sizeof pagetable);
		pool_ready = 0;
	}
	if (!saved)
		return;
	tlb_invalidate_from(0);
	for (i = 0; i < TLB_ENTRIES; i++)
		tlb_write(i, &saved_tlb[i]);
	set_wired(saved_wired);
	set_pagemask(saved_pagemask);
	set_entryhi(saved_entryhi);
}

/* ---------------------------------------------------------------- */

static void
print_lo(unsigned long lo)
{
	if ((lo & EL_V) == 0) {
		printf("%-20s", "-");
		return;
	}
	printf("pa %08x c%u %c %c   ", ENTRYLO_PA(lo), (lo >> EL_C_SHIFT) & 7,
	    lo & EL_D ? 'D' : '-', lo & EL_G ? 'G' : '-');
}

void
tlb_dump(void)
{
	unsigned long hi = get_entryhi(), mask = get_pagemask();
	struct tlbent e;
	int i, valid = 0;

	printf("TLB: %d entries, Wired %u, ASID %02x, %u refills by "
	    "ip20mon's handler\n", TLB_ENTRIES, get_wired(),
	    hi & EH_ASID_MASK, tlb_refills);
	for (i = 0; i < TLB_ENTRIES; i++) {
		tlb_read(i, &e);
		if (((e.lo0 | e.lo1) & EL_V) == 0)
			continue;
		valid++;
		printf("  %2d  va %08x asid %02x mask %04x  ", i,
		    e.hi & EH_VPN2_MASK, e.hi & EH_ASID_MASK, e.mask >> 13);
		print_lo(e.lo0);
		print_lo(e.lo1);
		printf("\n");
	}
	printf("  %d of %d entries valid\n", valid, TLB_ENTRIES);
	set_entryhi(hi);
	set_pagemask(mask);
}

/* ---------------------------------------------------------------- */

static int passed, failed;

static void
result(int ok, const char *fmt, unsigned long a, unsigned long b)
{
	printf("     ");
	printf(fmt, a, b);
	printf(": %s\n", ok ? "OK" : "FAILED");
	if (ok)
		passed++;
	else
		failed++;
}

/*
 * Loads and stores that may fault.  The fault handler changes the page
 * table and counters behind the compiler's back, and volatile alone
 * doesn't stop other memory accesses moving across the access (gcc
 * hoisted a page-table read above a faulting load), so these are also
 * compiler barriers, like a kernel's user-memory accessors.
 */
static unsigned long
rd(unsigned long va)
{
	unsigned long v;

	__asm__ volatile ("" : : : "memory");
	v = *(volatile unsigned long *)va;
	__asm__ volatile ("" : : : "memory");
	return v;
}

static void
wr(unsigned long va, unsigned long v)
{
	__asm__ volatile ("" : : : "memory");
	*(volatile unsigned long *)va = v;
	__asm__ volatile ("" : : : "memory");
}

#define NPAGES		128

void
vm_test(void)
{
	unsigned long va, v, sysid, asid, f0, r0, m0;
	struct tlbent e;
	int i, bad, idx;

	if (!arcs_present() || !pool_init()) {
		printf("vm: no free memory for a page pool\n");
		return;
	}
	passed = failed = 0;
	asid = get_entryhi() & EH_ASID_MASK;
	set_pagemask(0);
	tlb_invalidate_from(saved_wired);
	set_entryhi(asid);
	memset(pagetable, 0, sizeof pagetable);
	tlb_refills = vm_faults = mod_faults = 0;
	printf("vm: page pool %08x-%08x, page table for kuseg 00000000-%08x\n",
	    pool_base, pool_base + POOL_SIZE - 1, (1 << PT_SPAN_SHIFT) - 1);

	/* A wired entry first, so the later tests can try to evict it. */
	printf("  1. wired entry %d: %08x -> MC registers, uncached\n",
	    (int)saved_wired, WIRED_VA);
	idx = saved_wired;
	set_wired(idx + 1);
	e.hi = WIRED_VA | asid;
	e.lo0 = ENTRYLO(MC_BASE, EL_C_UNCACHED | EL_V | EL_G);
	e.lo1 = EL_G;
	e.mask = 0;
	tlb_write(idx, &e);
	set_entryhi(asid);
	sysid = REG32(MC_SYSID);
	v = ~sysid;
	i = badaddr(WIRED_VA + (MC_SYSID - MC_BASE), 0, &v);
	result(i == 0 && v == sysid, "MC SYSID through the mapping %08x, "
	    "through kseg1 %08x", v, sysid);

	printf("  2. demand-zero: write one word to each of %d pages at %08x\n",
	    NPAGES, RW_BASE);
	r0 = tlb_refills;
	for (i = 0; i < NPAGES; i++)
		wr(RW_BASE + i * PAGE_SIZE, 0x5a000000 | i);
	result(vm_faults == NPAGES, "%u page faults, %u TLB refills",
	    vm_faults, tlb_refills - r0);
	r0 = tlb_refills;
	f0 = vm_faults;
	bad = 0;
	for (i = 0; i < NPAGES; i++) {
		va = RW_BASE + i * PAGE_SIZE;
		if (rd(va) != (0x5a000000 | i) || rd(va + 4) != 0 ||
		    rd(va + PAGE_SIZE - 4) != 0)
			bad++;
	}
	result(bad == 0 && vm_faults == f0, "read back: %u bad pages, "
	    "%u TLB refills (only 48 entries)", bad, tlb_refills - r0);

	printf("  3. dirty tracking: read a page, then write it\n");
	va = RW_BASE + 0x200000;
	m0 = mod_faults;
	v = rd(va);
	result(v == 0 && (*pte_of(va) & (EL_V | EL_D)) == EL_V, "read %08x, "
	    "mapped valid and clean (EntryLo %08x)", v, *pte_of(va));
	wr(va, 0x12345678);
	result(mod_faults == m0 + 1 && (*pte_of(va) & EL_D) &&
	    rd(va) == 0x12345678, "write took %u TLB-modified fault, "
	    "now dirty (EntryLo %08x)", mod_faults - m0, *pte_of(va));

	printf("  4. faults that should happen, via badaddr()\n");
	v = 1;
	i = badaddr(RO_BASE, 1, &v);
	result(i < 0 && badaddr_code == EXC_MOD, "write to read-only "
	    "%08x: exception %u (TLB modified)", RO_BASE, badaddr_code);
	i = badaddr(0, 0, &v);
	result(i < 0 && badaddr_code == EXC_TLBL, "read of null %08x: "
	    "exception %u (TLB miss)", 0, badaddr_code);
	i = badaddr(0x10000000, 0, &v);
	result(i < 0 && badaddr_code == EXC_TLBL, "read of %08x, above the "
	    "page table: exception %u", 0x10000000, badaddr_code);

	printf("  5. the wired entry survived %u random refills\n",
	    tlb_refills);
	i = tlb_probe(WIRED_VA | asid);
	set_entryhi(asid);
	v = ~sysid;
	badaddr(WIRED_VA + (MC_SYSID - MC_BASE), 0, &v);
	result(i == idx && v == sysid, "still at index %d, still reads "
	    "SYSID %08x", i, v);

	printf("vm: %d passed, %d failed; %u pages used\n", passed, failed,
	    vm_faults);
	vm_restore();
}
