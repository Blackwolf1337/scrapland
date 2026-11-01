/*
 * The C runtime of include/scrap/crt.h in ScrapC.exe (stage 2b of docs/standalone.md): the SD_IMPORT_CRT functions
 * of src/imports.inc. The original links the static MSVC 14 CRT, which is UCRT based; this exe links the UCRT DLL
 * through mingw, so most functions are the same code there and only pass through. One allocator (the UCRT heap,
 * which the original's static CRT also took from GetProcessHeap) and one set of FILE objects, errno and locale serve
 * the C and the Python library. The entries of the original's CRT that only the code of the original reached
 * (_alloca_probe, _EH_prolog, _setjmp3 ...) went with the room in stage 3c2.
 *
 * Only the exact math functions come from this UCRT; the others are Microsoft's ucrtbase (src/standalone/crtmath.c),
 * Wine's libm differs in the last bits. The printf and scanf cores, strtod, atof and strerror are C of their own
 * (src/standalone/crtfmt.c and crtscan.c) that gives what the original's static UCRT gives; Wine's format and read
 * denormals, NaNs and some exponents otherwise and has other error texts. The compiler support (the vector
 * constructor and destructor iterators, the thread-safe statics, atexit and the startup) is src/standalone/crtinit.c.
 */
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <io.h>
#include <locale.h>
#include <malloc.h>
#include <math.h>
#include <mbstring.h>
#include <process.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <windows.h>

#include "crtnum.h"
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

size_t crt_msize(void *p) {
	return _msize(p);
}

char *crt_strdup(const char *s) {
	return _strdup(s);
}

/* ---- stdio ---- */

void *crt_iob_func(unsigned index) {
	return __acrt_iob_func(index);
}

void *crt_fopen(const char *name, const char *mode) {
	return fopen(name, mode);
}

void *crt_fsopen(const char *name, const char *mode, int share) {
	return _fsopen(name, mode, share);
}

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

int crt_fileno(void *f) {
	return _fileno(f);
}

/*
 * The number formatting and parsing are C of their own (src/standalone/crtfmt.c and crtscan.c), the same as the
 * original's static UCRT. A locale object would be one of the linked UCRT, so calls with one stay there.
 */
int crt_stdio_common_vsprintf(uint64_t options, char *buf, size_t count, const char *fmt, void *locale, va_list ap) {
	if (!locale) {
		return Crt_VFormat(options, buf, count, fmt, ap);
	}
	return __stdio_common_vsprintf(options, buf, count, fmt, locale, ap);
}

/* the text goes to the FILE of the linked UCRT */
int crt_stdio_common_vfprintf(uint64_t options, void *f, const char *fmt, void *locale, va_list ap) {
	if (!locale) {
		return Crt_VFormatFile(options, f, fmt, ap);
	}
	return __stdio_common_vfprintf(options, f, fmt, locale, ap);
}

int crt_stdio_common_vsscanf(uint64_t options, const char *buf, size_t count, const char *fmt, void *locale, va_list ap) {
	if (!locale) {
		return Crt_VScan(options, buf, count, fmt, ap);
	}
	return __stdio_common_vsscanf(options, buf, count, fmt, locale, ap);
}

int crt_remove(const char *name) {
	return remove(name);
}

int crt_rename(const char *from, const char *to) {
	return rename(from, to);
}

/* ---- low level io and the file system ---- */

int crt_open(const char *name, int flags, ...) {
	va_list ap;
	int mode = 0;

	if (flags & _O_CREAT) {
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	return _open(name, flags, mode);
}

int crt_read(int fd, void *buf, unsigned int size) {
	return _read(fd, buf, size);
}

int crt_close(int fd) {
	return _close(fd);
}

long crt_lseek(int fd, long off, int origin) {
	return _lseek(fd, off, origin);
}

long crt_filelength(int fd) {
	return _filelength(fd);
}

int crt_chsize(int fd, long size) {
	return _chsize(fd, size);
}

int crt_isatty(int fd) {
	return _isatty(fd);
}

/* struct crt_stat32 of src/python/pyshim.h is struct _stat32 */
int crt_stat32(const char *name, void *st) {
	return _stat32(name, st);
}

int crt_fstat32(int fd, void *st) {
	return _fstat32(fd, st);
}

static_assert(sizeof(crt_finddata32) == sizeof(struct _finddata32_t), "crt_finddata32 is _finddata32_t");

intptr_t crt_findfirst32(const char *spec, crt_finddata32 *fd) {
	return _findfirst32(spec, (struct _finddata32_t *)fd);
}

int crt_findnext32(intptr_t h, crt_finddata32 *fd) {
	return _findnext32(h, (struct _finddata32_t *)fd);
}

int crt_access(const char *name, int mode) {
	return _access(name, mode);
}

int crt_chmod(const char *path, int mode) {
	return _chmod(path, mode);
}

/*
 * _chdir of the MSVC runtime also sets the variable "=X:" of the environment to the new directory (not for a UNC
 * path), which is where SetCurrentDirectory("X:") goes later, so _chdrive comes back to it; Wine's _chdir does not.
 */
int crt_chdir(const char *dir) {
	char cwd[MAX_PATH + 1];

	if (_chdir(dir) != 0) {
		return -1;
	}
	DWORD n = GetCurrentDirectoryA(sizeof(cwd), cwd);
	if (n == 0 || n >= sizeof(cwd)) {
		*_errno() = n ? ERANGE : ENOENT;
		return -1;
	}
	if ((cwd[0] == '\\' || cwd[0] == '/') && cwd[0] == cwd[1]) {
		return 0;
	}
	char name[4] = { '=', (char)toupper((unsigned char)cwd[0]), ':', '\0' };
	if (!SetEnvironmentVariableA(name, cwd)) {
		*_errno() = EACCES;
		return -1;
	}
	return 0;
}

int crt_mkdir(const char *dir) {
	return _mkdir(dir);
}

char *crt_getcwd(char *buf, int size) {
	return _getcwd(buf, size);
}

int crt_getdrive(void) {
	return _getdrive();
}

int crt_chdrive(int drive) {
	return _chdrive(drive);
}

void crt_splitpath(const char *path, char *drive, char *dir, char *name, char *ext) {
	_splitpath(path, drive, dir, name, ext);
}

/* the argument list up to its NULL, as _spawnlp collects it */
int crt_spawnlp(int mode, const char *cmd, const char *arg0, ...) {
	const char *argv[64];
	va_list ap;
	int n = 0;

	va_start(ap, arg0);
	for (const char *a = arg0; a && n < (int)(sizeof(argv) / sizeof(argv[0])) - 1; a = va_arg(ap, const char *)) {
		argv[n++] = a;
	}
	va_end(ap);
	argv[n] = NULL;
	return (int)_spawnvp(mode, cmd, argv);
}

/* ---- strings, conversion, ctype ---- */

int crt_stricmp(const char *a, const char *b) {
	return _stricmp(a, b);
}

int crt_strnicmp(const char *a, const char *b, size_t n) {
	return _strnicmp(a, b, n);
}

int crt_strncmp(const char *a, const char *b, size_t n) {
	return strncmp(a, b, n);
}

int crt_memicmp(const void *a, const void *b, size_t n) {
	return _memicmp(a, b, n);
}

char *crt_strncat(char *dst, const char *src, size_t n) {
	return strncat(dst, src, n);
}

size_t crt_strcspn(const char *s, const char *set) {
	return strcspn(s, set);
}

char *crt_strlwr(char *s) {
	return _strlwr(s);
}

char *crt_strupr(char *s) {
	return _strupr(s);
}

char *crt_strtok(char *s, const char *sep) {
	return strtok(s, sep);
}

char *crt_strtok_s(char *s, const char *sep, char **ctx) {
	return strtok_s(s, sep, ctx);
}

char *crt_mbsrchr(const char *s, unsigned int c) {
	return (char *)_mbsrchr((const unsigned char *)s, c);
}

int crt_strcoll(const char *a, const char *b) {
	return strcoll(a, b);
}

size_t crt_strxfrm(char *dst, const char *src, size_t n) {
	return strxfrm(dst, src, n);
}

/* the text in a buffer of the thread, which a caller may change, as the runtime keeps it */
char *crt_strerror(int code) {
	static thread_local char buf[134];
	const char *text = Crt_ErrorText(code);

	memcpy(buf, text, strlen(text) + 1);
	return buf;
}

int crt_atoi(const char *s) {
	return atoi(s);
}

double crt_atof(const char *s) {
	return Crt_Atof(s);
}

double crt_strtod(const char *s, char **end) {
	return Crt_Strtod(s, end);
}

char *crt_ltoa(long v, char *buf, int radix) {
	return _ltoa(v, buf, radix);
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

int32_t crt_time32(int32_t *t) {
	return _time32((__time32_t *)t);
}

void *crt_gmtime32(const int32_t *t) {
	return _gmtime32((const __time32_t *)t);
}

double crt_difftime32(int32_t t1, int32_t t0) {
	return _difftime32(t1, t0);
}

char *crt_ctime32(const int32_t *t) {
	return _ctime32((const __time32_t *)t);
}

char *crt_asctime(const void *tm) {
	return asctime(tm);
}

size_t crt_strftime(char *buf, size_t size, const char *fmt, const void *tm) {
	return strftime(buf, size, fmt, tm);
}

char *crt_strtime(char *buf) {
	return _strtime(buf);
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

int *crt_errno(void) {
	return _errno();
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

/* ---- error helper of the compiler: the original fails fast ---- */

/* __report_rangecheckfailure: __fastfail(FAST_FAIL_RANGE_CHECK_FAILURE) */
[[noreturn]] void crt_report_rangecheckfailure(void) {
	__asm__ volatile("int $0x29" : : "c"(8));
	TerminateProcess(GetCurrentProcess(), STATUS_STACK_BUFFER_OVERRUN);
	__builtin_unreachable();
}

/* ---- math: the classification; the math functions are the ones of Microsoft's ucrtbase (crtmath.c) ---- */

int crt_fpclass(double x) {
	return _fpclass(x);
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
