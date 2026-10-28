/*
 * The inline functions of the CRT headers that the compiler put into the game code (legacy_stdio_definitions and
 * corecrt_stdio.h of the UCRT): sprintf, sscanf and fprintf on the __stdio_common functions of the exe's CRT with the
 * option words of the exe, and the vector constructor iterator of the compiler.
 */
#include <stdarg.h>
#include <stdint.h>

#include "scrap/crt.h"
#include "scrap/game.h"

/* the option words of the inline functions (_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS and the scanf ones) */
#define g_PrintfOptions (GAME_ARRAY_IN(uint64_t, g_PrintfOptions, 0, 0x444210))
#define g_ScanfOptions (GAME_ARRAY(uint64_t, g_ScanfOptions, 0x4445c0))

uint64_t *crt_local_printf_options(void) {
	return g_PrintfOptions;
}

uint64_t *crt_local_scanf_options(void) {
	return g_ScanfOptions;
}

/* _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR (1) set, no count: -1 for an error */
static inline int Stdio_VSprintf(char *buf, const char *fmt, va_list ap) {
	int r = crt_stdio_common_vsprintf(*crt_local_printf_options() | 1, buf, (size_t)-1, fmt, NULL, ap);
	return r < 0 ? -1 : r;
}

static inline int Stdio_VFprintf(void *f, const char *fmt, va_list ap) {
	return crt_stdio_common_vfprintf(*crt_local_printf_options(), f, fmt, NULL, ap);
}

int crt_sprintf(char *buf, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	int r = Stdio_VSprintf(buf, fmt, ap);
	va_end(ap);
	return r;
}

/* vsprintf and vfprintf: only inlined in the exe (the Python code), C for the Python library of ScrapC.exe */
int crt_vsprintf(char *buf, const char *fmt, va_list ap) {
	return Stdio_VSprintf(buf, fmt, ap);
}

int crt_vfprintf(void *f, const char *fmt, va_list ap) {
	return Stdio_VFprintf(f, fmt, ap);
}

int crt_sscanf(const char *s, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	int r = crt_stdio_common_vsscanf(*crt_local_scanf_options(), s, (size_t)-1, fmt, NULL, ap);
	va_end(ap);
	return r;
}

int crt_fprintf(void *file, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	int r = Stdio_VFprintf(file, fmt, ap);
	va_end(ap);
	return r;
}

/* `vector constructor iterator': the constructor (thiscall) on count objects of size bytes */
void __stdcall crt_vector_ctor(void *p, uint32_t size, uint32_t count, void(__thiscall *ctor)(void *self)) {
	for (; count; count--) {
		ctor(p);
		p = (char *)p + size;
	}
}
