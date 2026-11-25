/*
 * subr.c
 *	The few C library routines ip20mon needs.  The compiler may also
 *	emit calls to memset and memcpy on its own, so they must exist.
 */

#include "ip20.h"

void *
memset(void *dst, int c, unsigned long n)
{
	unsigned char *p = dst;

	while (n-- > 0)
		*p++ = c;
	return dst;
}

void *
memcpy(void *dst, const void *src, unsigned long n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;

	while (n-- > 0)
		*d++ = *s++;
	return dst;
}

unsigned long
strlen(const char *s)
{
	const char *p = s;

	while (*p != '\0')
		p++;
	return p - s;
}

int
strcmp(const char *a, const char *b)
{
	while (*a != '\0' && *a == *b) {
		a++;
		b++;
	}
	return (unsigned char)*a - (unsigned char)*b;
}
