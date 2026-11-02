/*
 * The C runtime of include/scrap/crt.h: thin wrappers of the platform's libc. One allocator and one set of FILE
 * objects, errno and locale serve the C and the Python library. The functions whose libc differs between the systems
 * (files and paths, the MSVC string extras, the 32 bit time) are in crtwin.c (mingw-w64 with the UCRT) and
 * crtposix.c; the compiler support (the vector constructor and destructor iterators, the thread-safe statics,
 * atexit and the startup) is src/standalone/crtinit.c.
 */
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "scrap/crt.h"
#include "scrap/game.h"

/* ---- heap ---- */

void *crt_malloc(size_t size) {
	return malloc(size);
}

void crt_free(void *p) {
	free(p);
}

void *crt_realloc(void *p, size_t size) {
	return realloc(p, size);
}

/* ---- stdio ---- */

int crt_fclose(void *f) {
	return fclose(f);
}

size_t crt_fread(void *dst, size_t size, size_t count, void *f) {
	return fread(dst, size, count, f);
}

size_t crt_fwrite(const void *src, size_t size, size_t count, void *f) {
	return fwrite(src, size, count, f);
}

int crt_fputc(int c, void *f) {
	return fputc(c, f);
}

int crt_fputs(const char *s, void *f) {
	return fputs(s, f);
}

int crt_fgetc(void *f) {
	return fgetc(f);
}

int crt_ungetc(int c, void *f) {
	return ungetc(c, f);
}

char *crt_fgets(char *buf, int size, void *f) {
	return fgets(buf, size, f);
}

int crt_fflush(void *f) {
	return fflush(f);
}

int crt_fseek(void *f, long off, int origin) {
	return fseek(f, off, origin);
}

long crt_ftell(void *f) {
	return ftell(f);
}

int crt_feof(void *f) {
	return feof(f);
}

int crt_ferror(void *f) {
	return ferror(f);
}

void crt_clearerr(void *f) {
	clearerr(f);
}

int crt_setvbuf(void *f, char *buf, int mode, size_t size) {
	return setvbuf(f, buf, mode, size);
}

/* the formatting of the platform's libc; sprintf has no size like the original's callers (bounded later) */
int crt_vsprintf(char *buf, const char *fmt, va_list ap) {
	return vsprintf(buf, fmt, ap);
}

int crt_sprintf(char *buf, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	int r = vsprintf(buf, fmt, ap);
	va_end(ap);
	return r;
}

int crt_vfprintf(void *f, const char *fmt, va_list ap) {
	return vfprintf(f, fmt, ap);
}

int crt_fprintf(void *f, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	int r = vfprintf(f, fmt, ap);
	va_end(ap);
	return r;
}

int crt_sscanf(const char *s, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	int r = vsscanf(s, fmt, ap);
	va_end(ap);
	return r;
}

/* ---- strings, conversion, ctype ---- */

int crt_strncmp(const char *a, const char *b, size_t n) {
	return strncmp(a, b, n);
}

char *crt_strncat(char *dst, const char *src, size_t n) {
	return strncat(dst, src, n);
}

size_t crt_strcspn(const char *s, const char *set) {
	return strcspn(s, set);
}

char *crt_strtok(char *s, const char *sep) {
	return strtok(s, sep);
}

int crt_strcoll(const char *a, const char *b) {
	return strcoll(a, b);
}

size_t crt_strxfrm(char *dst, const char *src, size_t n) {
	return strxfrm(dst, src, n);
}

char *crt_strerror(int code) {
	return strerror(code);
}

int crt_atoi(const char *s) {
	return atoi(s);
}

double crt_atof(const char *s) {
	return atof(s);
}

double crt_strtod(const char *s, char **end) {
	return strtod(s, end);
}

void crt_qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *)) {
	qsort(base, n, size, cmp);
}

int crt_isalpha(int c) {
	return isalpha(c);
}

int crt_isdigit(int c) {
	return isdigit(c);
}

int crt_isalnum(int c) {
	return isalnum(c);
}

int crt_isxdigit(int c) {
	return isxdigit(c);
}

int crt_islower(int c) {
	return islower(c);
}

int crt_isupper(int c) {
	return isupper(c);
}

int crt_isspace(int c) {
	return isspace(c);
}

int crt_toupper(int c) {
	return toupper(c);
}

int crt_tolower(int c) {
	return tolower(c);
}

/* ---- time, locale, environment ---- */

char *crt_asctime(const void *tm) {
	return asctime(tm);
}

size_t crt_strftime(char *buf, size_t size, const char *fmt, const void *tm) {
	return strftime(buf, size, fmt, tm);
}

char *crt_setlocale(int category, const char *locale) {
	return setlocale(category, locale);
}

void *crt_localeconv(void) {
	return localeconv();
}

char *crt_getenv(const char *name) {
	return getenv(name);
}

int crt_rand(void) {
	return rand();
}

void crt_srand(unsigned int seed) {
	srand(seed);
}

void (*crt_signal(int sig, void (*handler)(int)))(int) {
	return signal(sig, handler);
}

[[noreturn]] void crt_abort(void) {
	abort();
}

/* ---- entries the tests and the vtables reach (the memcpy and memset of the original, its _purecall) ---- */

/* memcpy of the MSVC runtime also copies overlapping blocks like memmove */
void *crt_memcpy(void *dst, const void *src, size_t n) {
	return memmove(dst, src, n);
}

void *crt_memset(void *dst, int c, size_t n) {
	return memset(dst, c, n);
}

/* _purecall: a pure virtual method was called, the runtime ends the program */
[[noreturn]] void crt_purecall(void) {
	Com_Printf("crt: pure virtual function call\n");
	abort();
}
