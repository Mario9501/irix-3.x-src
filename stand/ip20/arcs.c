/*
 * arcs.c
 *	Calls into the IP20 PROM through its ARCS firmware vector.
 *
 *	The System Parameter Block is read through kseg0 (cached), not
 *	kseg1: the PROM may have written it through the write-back D-cache,
 *	so an uncached read could see stale memory.
 */

#include "ip20.h"
#include "arcs.h"

static struct arcs_spb	*spb;
static long		*fv;		/* firmware vector, or 0 */
static unsigned long	fv_slots;

typedef long (*fv_rw_t)(unsigned long, void *, unsigned long, unsigned long *);
typedef char *(*fv_getenv_t)(const char *);
typedef struct arcs_memdesc *(*fv_memdesc_t)(struct arcs_memdesc *);
typedef struct arcs_component *(*fv_child_t)(struct arcs_component *);
typedef unsigned long (*fv_time_t)(void);
typedef void (*fv_void_t)(void);

int
arcs_init(void)
{
	struct arcs_spb *p = (struct arcs_spb *)PHYS_TO_K0(ARCS_SPB_PHYS);

	if (p->signature != ARCS_SIG && p->signature != ARCS_SIG_SWAP)
		return 0;
	if (p->firmware_vector == 0 || p->fv_length < 4)
		return 0;
	spb = p;
	fv = (long *)p->firmware_vector;
	fv_slots = p->fv_length / sizeof (long);
	return 1;
}

int
arcs_present(void)
{
	return fv != 0;
}

struct arcs_spb *
arcs_spb(void)
{
	return spb;
}

int
arcs_avail(int slot)
{
	return fv != 0 && (unsigned long)slot < fv_slots && fv[slot] != 0;
}

static void *
slot(int n)
{
	return arcs_avail(n) ? (void *)fv[n] : 0;
}

long
arcs_write(const char *buf, unsigned long n)
{
	fv_rw_t fn = (fv_rw_t)slot(FV_WRITE);
	unsigned long count;

	if (fn == 0)
		return -1;
	return fn(ARCS_STDOUT, (void *)buf, n, &count);
}

int
arcs_getc(void)
{
	fv_rw_t fn = (fv_rw_t)slot(FV_READ);
	unsigned long count;
	char c;

	if (fn == 0)
		return -1;
	do {
		if (fn(ARCS_STDIN, &c, 1, &count) != 0)
			return -1;
	} while (count != 1);
	return c & 0xff;
}

char *
arcs_getenv(const char *name)
{
	fv_getenv_t fn = (fv_getenv_t)slot(FV_GETENV);

	return fn ? fn(name) : 0;
}

struct arcs_memdesc *
arcs_getmemdesc(struct arcs_memdesc *prev)
{
	fv_memdesc_t fn = (fv_memdesc_t)slot(FV_GETMEMDESC);

	return fn ? fn(prev) : 0;
}

struct arcs_component *
arcs_getchild(struct arcs_component *parent)
{
	fv_child_t fn = (fv_child_t)slot(FV_GETCHILD);

	return fn ? fn(parent) : 0;
}

unsigned long
arcs_reltime(void)
{
	fv_time_t fn = (fv_time_t)slot(FV_GETRELTIME);

	return fn ? fn() : 0;
}

void
arcs_reboot(void)
{
	fv_void_t fn = (fv_void_t)slot(FV_REBOOT);

	if (fn == 0)
		fn = (fv_void_t)slot(FV_RESTART);
	if (fn)
		fn();
}
