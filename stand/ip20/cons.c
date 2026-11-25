/*
 * cons.c
 *	Console I/O for ip20mon: through the PROM's ARCS vector when there
 *	is one, otherwise straight to the Z85C30 (zs.c).  printf() handles
 *	%c %s %d %u %x and %%, with an optional '-' or '0' flag, a width,
 *	and a 'l' that is ignored (long and int are both 32 bits).
 */

#include <stdarg.h>
#include "ip20.h"
#include "arcs.h"

static int cons_arcs;

void
cons_init(void)
{
	cons_arcs = arcs_present();
}

static void
rawputc(int c)
{
	char ch = c;

	if (cons_arcs)
		arcs_write(&ch, 1);
	else
		zs_putc(c);
}

int
putchar(int c)
{
	if (c == '\n')
		rawputc('\r');
	rawputc(c);
	return c;
}

int
getchar(void)
{
	return cons_arcs ? arcs_getc() : zs_getc();
}

static void
putnum(unsigned long v, unsigned base, int width, int zero, int neg)
{
	char buf[12];
	int n = 0;

	do {
		buf[n++] = "0123456789abcdef"[v % base];
		v /= base;
	} while (v != 0);
	if (neg)
		width--;
	if (neg && zero)
		putchar('-');
	for (; width > n; width--)
		putchar(zero ? '0' : ' ');
	if (neg && !zero)
		putchar('-');
	while (n > 0)
		putchar(buf[--n]);
}

int
printf(const char *fmt, ...)
{
	va_list ap;
	const char *s;
	long d;
	int width, zero, left, len;

	va_start(ap, fmt);
	for (; *fmt != '\0'; fmt++) {
		if (*fmt != '%') {
			putchar(*fmt);
			continue;
		}
		fmt++;
		zero = left = width = 0;
		if (*fmt == '-') {
			left = 1;
			fmt++;
		} else if (*fmt == '0') {
			zero = 1;
			fmt++;
		}
		while (*fmt >= '0' && *fmt <= '9')
			width = width * 10 + *fmt++ - '0';
		if (*fmt == 'l')
			fmt++;

		switch (*fmt) {
		case 'c':
			putchar(va_arg(ap, int));
			break;
		case 's':
			s = va_arg(ap, const char *);
			if (s == 0)
				s = "(null)";
			len = strlen(s);
			if (!left)
				for (; width > len; width--)
					putchar(' ');
			while (*s != '\0')
				putchar(*s++);
			if (left)
				for (; width > len; width--)
					putchar(' ');
			break;
		case 'd':
			d = va_arg(ap, long);
			putnum(d < 0 ? 0UL - (unsigned long)d : (unsigned long)d,
			    10, width, zero, d < 0);
			break;
		case 'u':
			putnum(va_arg(ap, unsigned long), 10, width, zero, 0);
			break;
		case 'x':
			putnum(va_arg(ap, unsigned long), 16, width, zero, 0);
			break;
		case '%':
			putchar('%');
			break;
		case '\0':
			fmt--;		/* let the loop see the end */
			break;
		default:
			putchar('%');
			putchar(*fmt);
			break;
		}
	}
	va_end(ap);
	return 0;
}
