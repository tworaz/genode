#include <util/string.h>

extern "C" {

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
	unsigned long r = 0;
	Genode::size_t s = Genode::ascii_to_unsigned(nptr, r, base);
	if (endptr) *endptr = (char*)nptr + s;
	return r;
}


size_t strlen(const char *s) { return Genode::strlen(s); }


size_t strnlen(const char *s, size_t maxlen)
{
	size_t c;
	for (c = 0; c <maxlen; c++)
		if (!s[c])
			return c;

	return maxlen;
}


int memcmp(const void *p0, const void *p1, size_t size) {
	return Genode::memcmp(p0, p1, size); }


char *strchr(const char *p, int ch)
{
	char c;
	c = ch;
	for (;; ++p) {
		if (*p == c)
			return ((char *)p);
		if (*p == '\0')
			break;
	}

	return 0;
}


char *strrchr(const char *p, int ch)
{
	size_t n = strlen(p) + 1;
	ch = (unsigned char)ch;
	while (n--) {
	    if (p[n] == ch) {
		return (char *)(p + n);
	    }
	}
	return NULL;
}


void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = (unsigned char *)s;
	while (n--) {
		if ((unsigned char)c == *p++) {
			return (void *)(p - 1);
		}
	}
	return NULL;
}

} // extern "C"
