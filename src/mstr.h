// Minimal string helpers with no library dependency (bare metal: no glibc, no *_chk wrappers)
#ifndef MSTR_H
#define MSTR_H
#include <stddef.h>

static inline size_t m_strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }

static inline int m_strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) { a++; b++; }
	return (unsigned char)*a - (unsigned char)*b;
}

static inline char *m_strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) {} return r; }

static inline char *m_strncpy(char *d, const char *s, size_t n)
{
	size_t i = 0;
	for (; i < n && s[i]; i++) d[i] = s[i];
	for (; i < n; i++) d[i] = 0;
	return d;
}

static inline char *m_strcat(char *d, const char *s) { m_strcpy(d + m_strlen(d), s); return d; }

static inline char *m_strncat(char *d, const char *s, size_t n)
{
	char *e = d + m_strlen(d);
	size_t i = 0;
	for (; i < n && s[i]; i++) e[i] = s[i];
	e[i] = 0;
	return d;
}

static inline char *m_strchr(const char *s, int c)
{
	for (;; s++) { if (*s == (char)c) return (char *)s; if (!*s) return 0; }
}

static inline char *m_strrchr(const char *s, int c)
{
	const char *r = 0;
	for (;; s++) { if (*s == (char)c) r = s; if (!*s) return (char *)r; }
}

#endif
