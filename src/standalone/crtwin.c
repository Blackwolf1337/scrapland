/*
 * The part of the C runtime of include/scrap/crt.h whose libc is the one of Windows (mingw-w64 with the UCRT): the
 * MSVC names of the file system, path, string and 32 bit time functions. crtposix.c has the same for other systems.
 */
#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <io.h>
#include <malloc.h>
#include <mbstring.h>
#include <process.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <windows.h>

#include "scrap/crt.h"
#include "scrap/game.h"

size_t crt_msize(void *p) {
	return _msize(p);
}

char *crt_strdup(const char *s) {
	return _strdup(s);
}

void *crt_iob_func(unsigned index) {
	return __acrt_iob_func(index);
}

void *crt_fopen(const char *name, const char *mode) {
	return fopen(name, mode);
}

void *crt_fsopen(const char *name, const char *mode, int share) {
	return _fsopen(name, mode, share);
}

int crt_fileno(void *f) {
	return _fileno(f);
}

int crt_remove(const char *name) {
	return remove(name);
}

int crt_rename(const char *from, const char *to) {
	return rename(from, to);
}

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

int crt_stricmp(const char *a, const char *b) {
	return _stricmp(a, b);
}

int crt_strnicmp(const char *a, const char *b, size_t n) {
	return _strnicmp(a, b, n);
}

int crt_memicmp(const void *a, const void *b, size_t n) {
	return _memicmp(a, b, n);
}

char *crt_strlwr(char *s) {
	return _strlwr(s);
}

char *crt_strupr(char *s) {
	return _strupr(s);
}

char *crt_strtok_s(char *s, const char *sep, char **ctx) {
	return strtok_s(s, sep, ctx);
}

char *crt_mbsrchr(const char *s, unsigned int c) {
	return (char *)_mbsrchr((const unsigned char *)s, c);
}

char *crt_ltoa(long v, char *buf, int radix) {
	return _ltoa(v, buf, radix);
}

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

char *crt_strtime(char *buf) {
	return _strtime(buf);
}

int *crt_errno(void) {
	return _errno();
}

/* __report_rangecheckfailure: __fastfail(FAST_FAIL_RANGE_CHECK_FAILURE) */
[[noreturn]] void crt_report_rangecheckfailure(void) {
	__asm__ volatile("int $0x29" : : "c"(8));
	TerminateProcess(GetCurrentProcess(), STATUS_STACK_BUFFER_OVERRUN);
	__builtin_unreachable();
}

int crt_fpclass(double x) {
	return _fpclass(x);
}

char *crt_syspath(const char *path, char *out, int size) {
	snprintf(out, (size_t)size, "%s", path);
	return out;
}
