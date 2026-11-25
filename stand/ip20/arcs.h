/*
 * arcs.h
 *	The parts of the SGI ARCS firmware interface that ip20mon uses.
 *	Layouts follow NetBSD sys/dev/arcbios/arcbios.h (32-bit, sgimips).
 */

#ifndef ARCS_H
#define ARCS_H

#define ARCS_SPB_PHYS	0x1000		/* System Parameter Block */
#define ARCS_SIG	0x53435241	/* "ARCS", both byte orders seen */
#define ARCS_SIG_SWAP	0x41524353

struct arcs_spb {
	unsigned long	signature;
	unsigned long	length;
	unsigned short	version;
	unsigned short	revision;
	long		restart_block;
	long		debug_block;
	long		ge_vector;
	long		utlb_miss_vector;
	unsigned long	fv_length;	/* firmware vector length, bytes */
	long		firmware_vector;
	unsigned long	pv_length;
	long		private_vector;
	unsigned long	adapter_count;
	unsigned long	adapter_type;
	unsigned long	av_length;
	long		adapter_vector;
};

/* Firmware vector slots (each is a 32-bit function address) */
#define FV_RESTART		5
#define FV_REBOOT		6
#define FV_ENTERINTERACTIVE	7
#define FV_GETCHILD		10
#define FV_GETMEMDESC		18
#define FV_GETRELTIME		21
#define FV_READ			25
#define FV_GETREADSTATUS	26
#define FV_WRITE		27
#define FV_GETENV		30

#define ARCS_STDIN	0
#define ARCS_STDOUT	1

struct arcs_memdesc {
	unsigned long	type;
	unsigned long	base_page;	/* physical, 4K pages */
	unsigned long	page_count;
};

#define ARCS_PAGESHIFT	12

/* SGI's memory type numbering, which differs from the ARC standard */
#define ARCS_MEM_EXCEPTION	0
#define ARCS_MEM_SPB		1
#define ARCS_MEM_FREECONTIG	2
#define ARCS_MEM_FREE		3
#define ARCS_MEM_BAD		4
#define ARCS_MEM_PROGRAM	5
#define ARCS_MEM_FWTEMP		6
#define ARCS_MEM_FWPERM		7
#define ARCS_MEM_NTYPES		8

struct arcs_component {
	unsigned long	class;
	unsigned long	type;
	unsigned long	flags;
	unsigned short	version;
	unsigned short	revision;
	unsigned long	key;
	unsigned long	affinity;
	unsigned long	config_size;
	unsigned long	id_length;
	long		identifier;	/* char * */
};

int	arcs_init(void);
int	arcs_present(void);
int	arcs_avail(int slot);
struct arcs_spb	*arcs_spb(void);

long	arcs_write(const char *buf, unsigned long n);
int	arcs_getc(void);
char	*arcs_getenv(const char *name);
struct arcs_memdesc	*arcs_getmemdesc(struct arcs_memdesc *prev);
struct arcs_component	*arcs_getchild(struct arcs_component *parent);
unsigned long	arcs_reltime(void);
void	arcs_reboot(void);

#endif /* ARCS_H */
