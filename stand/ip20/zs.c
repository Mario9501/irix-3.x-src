/*
 * zs.c
 *	Polled Z85C30 console, used only when the PROM has no ARCS vector.
 *	This is rear serial port 1 ("serial(0)" to the PROM), which SGI
 *	wires to channel B of the chip at ZS1_BASE.  The PROM has already
 *	programmed the baud rate, so we don't touch the chip's setup.
 */

#include "ip20.h"

#define ZS_CONS		(ZS1_BASE + ZS_CHAN_B)

/* The Z85C30 needs a few microseconds between register accesses. */
static void
zs_delay(void)
{
	volatile int i;

	for (i = 0; i < 300; i++)
		;
}

void
zs_putc(int c)
{
	while ((REG8(ZS_CONS + ZS_CSR) & ZSRR0_TX_READY) == 0)
		zs_delay();
	zs_delay();
	REG8(ZS_CONS + ZS_DATA) = c;
	zs_delay();
}

int
zs_getc(void)
{
	int c;

	while ((REG8(ZS_CONS + ZS_CSR) & ZSRR0_RX_READY) == 0)
		zs_delay();
	zs_delay();
	c = REG8(ZS_CONS + ZS_DATA);
	zs_delay();
	return c;
}
