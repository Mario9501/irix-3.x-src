/*
 * main.c
 *	ip20mon: a standalone bring-up monitor for the IP20 (Indigo R4000).
 *	It is the first step toward running this tree on the Indigo: it
 *	proves we can be loaded by the PROM, talk to the console, identify
 *	the CPU and caches, find memory, and return to the PROM cleanly.
 */

#include "ip20.h"
#include "arcs.h"

#define LINELEN		80

static int	g_argc;
static char	**g_argv;
static int	mach_type;	/* IP number from the PROM, 0 if unknown */

/*
 * Only poke IP20 registers directly when the PROM says this is an IP20,
 * or when it can't say at all (no ARCS) and we take that on trust.
 */
int
ip20_hw_ok(void)
{
	return mach_type == 20 || mach_type == 0;
}

static void
sys_info(void)
{
	struct arcs_spb *spb = arcs_spb();
	struct arcs_component *root;
	const char *id;
	char *env;
	unsigned long i;

	if (spb == 0) {
		printf("firmware: no ARCS parameter block; using serial port 1 "
		    "directly\n");
		return;
	}
	printf("firmware: ARCS %u.%u, firmware vector %u entries\n",
	    spb->version, spb->revision, spb->fv_length / 4);

	root = arcs_getchild(0);
	if (root != 0 && root->identifier != 0) {
		id = (const char *)root->identifier;
		printf("system: ");
		for (i = 0; i < root->id_length && id[i] != '\0'; i++)
			putchar(id[i]);
		printf("\n");
		for (i = 0; i < root->id_length && id[i] != '\0'; i++)
			if (id[i] >= '0' && id[i] <= '9') {
				while (id[i] >= '0' && id[i] <= '9')
					mach_type = mach_type * 10 + id[i++] - '0';
				break;
			}
	}
	if (mach_type != 0 && mach_type != 20)
		printf("  warning: this is an IP%d, not an IP20; skipping "
		    "direct hardware access\n", mach_type);
	if (mach_type == 20)
		printf("  IP20 board revision %u\n",
		    (REG32(IP20_BOARDREV) & IP20_BOARDREV_MASK) >>
		    IP20_BOARDREV_SHIFT);

	if ((env = arcs_getenv("ConsoleOut")) != 0)
		printf("  ConsoleOut=%s\n", env);
	if ((env = arcs_getenv("console")) != 0)
		printf("  console=%s\n", env);
}

static void
cmd_help(const char *arg);

static void
cmd_info(const char *arg)
{
	sys_info();
	cpu_info();
	mem_info(ip20_hw_ok());
}

static void
cmd_cpu(const char *arg)
{
	cpu_info();
}

static void
cmd_mem(const char *arg)
{
	mem_info(ip20_hw_ok());
}

static void
cmd_clock(const char *arg)
{
	clock_info();
}

static void
cmd_env(const char *arg)
{
	char *v;

	if (*arg == '\0') {
		printf("usage: env NAME\n");
		return;
	}
	if (!arcs_avail(FV_GETENV)) {
		printf("the PROM doesn't provide environment variables\n");
		return;
	}
	v = arcs_getenv(arg);
	if (v != 0)
		printf("%s=%s\n", arg, v);
	else
		printf("%s is not set\n", arg);
}

static void
cmd_args(const char *arg)
{
	int i;

	for (i = 0; i < g_argc; i++)
		printf("  argv[%d] = %s\n", i, g_argv[i]);
}

static void
cmd_reboot(const char *arg)
{
	printf("rebooting...\n");
	exc_uninstall();
	arcs_reboot();
	exc_install();
	printf("the PROM has no reboot entry\n");
}

static void
cmd_trap(const char *arg)
{
	trap_test(arg);
}

static void
cmd_vectors(const char *arg)
{
	exc_vectors();
}

static void
cmd_regs(const char *arg)
{
	exc_regs();
}

static void
cmd_tlb(const char *arg)
{
	tlb_dump();
}

static void
cmd_vm(const char *arg)
{
	vm_test();
}

static const struct cmd {
	const char	*name;
	void		(*fn)(const char *);
	const char	*help;
} cmds[] = {
	{ "help",	cmd_help,	"list commands" },
	{ "info",	cmd_info,	"firmware, CPU and memory summary" },
	{ "cpu",	cmd_cpu,	"CPU type, caches and status register" },
	{ "mem",	cmd_mem,	"memory map from the PROM and the MC" },
	{ "clock",	cmd_clock,	"measure the CPU clock (takes ~2 s)" },
	{ "env",	cmd_env,	"env NAME: show a PROM variable" },
	{ "args",	cmd_args,	"show the arguments the PROM passed" },
	{ "trap",	cmd_trap,	"trap KIND: raise an exception on purpose" },
	{ "regs",	cmd_regs,	"registers at the last exception" },
	{ "vectors",	cmd_vectors,	"exception vectors and the PROM's code" },
	{ "tlb",	cmd_tlb,	"show the valid TLB entries" },
	{ "vm",		cmd_vm,		"test TLB refill, paging and wired entries" },
	{ "reboot",	cmd_reboot,	"reset the machine" },
	{ "exit",	0,		"return to the PROM" },
	{ 0 }
};

static void
cmd_help(const char *arg)
{
	const struct cmd *c;

	for (c = cmds; c->name != 0; c++)
		printf("  %-8s %s\n", c->name, c->help);
}

static void
readline(char *buf, int len)
{
	int n = 0, c;

	for (;;) {
		c = getchar();
		if (c < 0 || c == '\r' || c == '\n') {
			printf("\n");
			buf[n] = '\0';
			return;
		}
		if (c == '\b' || c == 0x7f) {
			if (n > 0) {
				n--;
				printf("\b \b");
			}
			continue;
		}
		if (c == 0x15) {		/* ^U: erase line */
			for (; n > 0; n--)
				printf("\b \b");
			continue;
		}
		if (c < ' ' || c > '~' || n >= len - 1)
			continue;
		buf[n++] = c;
		putchar(c);
	}
}

static void
monitor(void)
{
	char line[LINELEN], *p, *arg;
	const struct cmd *c;

	for (;;) {
		/* An exception that abandons a command lands back here. */
		if (setjmp(mon_jb) != 0)
			printf("back in ip20mon\n");
		mon_jb_set = 1;

		printf("ip20> ");
		readline(line, sizeof line);

		for (p = line; *p == ' '; p++)
			;
		if (*p == '\0')
			continue;
		for (arg = p; *arg != '\0' && *arg != ' '; arg++)
			;
		if (*arg != '\0')
			*arg++ = '\0';
		while (*arg == ' ')
			arg++;

		for (c = cmds; c->name != 0; c++)
			if (strcmp(p, c->name) == 0)
				break;
		if (c->name == 0) {
			printf("%s: unknown command; try help\n", p);
			continue;
		}
		if (c->fn == 0)
			return;			/* exit */
		c->fn(arg);
	}
}

int
main(int argc, char **argv, char **envp)
{
	g_argc = argc;
	g_argv = argv;

	arcs_init();
	cons_init();

	printf("\nip20mon: IRIS 3.6 standalone monitor for the IP20 "
	    "(Indigo R4000)\n\n");
	cmd_info("");
	printf("\n");
	exc_install();
	printf("\ntype help for commands, exit to return to the PROM\n");
	monitor();
	exc_uninstall();
	printf("returning to the PROM\n");
	return 0;
}
