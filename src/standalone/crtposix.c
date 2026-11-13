/*
 * The part of the C runtime of include/scrap/crt.h for systems other than Windows: the MSVC names of the file system,
 * path, string and 32 bit time functions on POSIX, with what the game expects from Windows. The file system shows as
 * drive Z: with '\\' (as Wine does: getcwd gives "Z:\\home\\..."); paths take '\\' or '/', a drive letter is
 * dropped, and a part of a path that does not exist is looked up without case (the files of the game are named in
 * any case). A file opened for reading in text mode drops the '\r' of its line ends. stat fills
 * the layout of struct _stat32, which the callers read by offset.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <math.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "scrap/crt.h"
#include "scrap/game.h"

#define CRT_PATH 1024

extern char **environ;

/* ---- paths ---- */

/*
 * The path of the game on this system into out: '/' for '\\', no drive letter, and each part that does not exist
 * replaced by the entry of its directory with the same name in another case (the rest is kept as it is when none
 * has it, so a new file gets the name it was given)
 */
static const char *Crt_Path(const char *in, char *out) {
	char tmp[CRT_PATH];
	size_t n = 0;

	if (isalpha((unsigned char)in[0]) && in[1] == ':') {
		in += 2;
	}
	for (; *in && n < sizeof(tmp) - 1; in++) {
		tmp[n++] = *in == '\\' ? '/' : *in;
	}
	tmp[n] = 0;
	if (!tmp[0] || access(tmp, F_OK) == 0) {
		memcpy(out, tmp, n + 1);
		return out;
	}
	size_t o = 0;
	const char *p = tmp;
	if (*p == '/') {
		out[o++] = '/';
		p++;
	}
	out[o] = 0;
	while (*p) {
		const char *e = strchr(p, '/');
		size_t len = e ? (size_t)(e - p) : strlen(p);
		if (o + len + 2 >= CRT_PATH) {
			break;
		}
		memcpy(out + o, p, len);
		out[o + len] = 0;
		if (len && access(out, F_OK) != 0) {
			/* the same name in another case in this directory */
			char dir[CRT_PATH];
			memcpy(dir, out, o);
			dir[o] = 0;
			DIR *d = opendir(o ? dir : ".");
			if (d) {
				struct dirent *de;
				while ((de = readdir(d))) {
					if (strlen(de->d_name) == len && !strncasecmp(de->d_name, p, len)) {
						memcpy(out + o, de->d_name, len);
						break;
					}
				}
				closedir(d);
			}
		}
		o += len;
		if (!e) {
			break;
		}
		out[o++] = '/';
		out[o] = 0;
		p = e + 1;
	}
	out[o] = 0;
	return out;
}

char *crt_syspath(const char *path, char *out, int size) {
	char tmp[CRT_PATH];
	snprintf(out, (size_t)size, "%s", Crt_Path(path, tmp));
	return out;
}

char *crt_gamepath(const char *path, char *out, int size) {
	snprintf(out, (size_t)size, "%s%s", path[0] == '/' ? "Z:" : "", path);
	for (char *c = out; *c; c++) {
		if (*c == '/') {
			*c = '\\';
		}
	}
	return out;
}

/* ---- heap ---- */

size_t crt_msize(void *p) {
	return malloc_usable_size(p);
}

char *crt_strdup(const char *s) {
	size_t n = strlen(s) + 1;
	char *d = crt_malloc(n);
	if (d) {
		memcpy(d, s, n);
	}
	return d;
}

/* ---- stdio ---- */

void *crt_iob_func(unsigned index) {
	return index == 0 ? stdin : index == 1 ? stdout : stderr;
}

/* a text mode file read: "\r\n" as "\n", as the MSVC runtime gives it */
static ssize_t Crt_TextRead(void *cookie, char *buf, size_t size) {
	FILE *f = cookie;
	size_t k = 0;

	while (k < size) {
		int c = getc(f);
		if (c == EOF) {
			break;
		}
		if (c == '\r') {
			int next = getc(f);
			if (next == '\n') {
				c = '\n';
			} else if (next != EOF) {
				ungetc(next, f);
			}
		}
		buf[k++] = (char)c;
	}
	return (ssize_t)k;
}

static int Crt_TextSeek(void *cookie, off64_t *pos, int whence) {
	FILE *f = cookie;

	if (fseeko(f, (off_t)*pos, whence)) {
		return -1;
	}
	*pos = ftello(f);
	return 0;
}

static int Crt_TextClose(void *cookie) {
	return fclose(cookie);
}

void *crt_fopen(const char *name, const char *mode) {
	char path[CRT_PATH], m[8];
	size_t n = 0;

	/* the mode without 't', which only the MSVC runtime knows */
	for (const char *c = mode; *c && n < sizeof(m) - 1; c++) {
		if (*c != 't') {
			m[n++] = *c;
		}
	}
	m[n] = 0;
	FILE *f = fopen(Crt_Path(name, path), m);
	if (!f || m[0] != 'r' || strchr(m, 'b') || strchr(m, '+')) {
		return f;
	}
	static const cookie_io_functions_t text = { Crt_TextRead, NULL, Crt_TextSeek, Crt_TextClose };
	FILE *t = fopencookie(f, "r", text);
	if (!t) {
		fclose(f);
	}
	return t;
}

void *crt_fsopen(const char *name, const char *mode, int share) {
	(void)share;
	return crt_fopen(name, mode);
}

int crt_fileno(void *f) {
	return fileno(f);
}

int crt_remove(const char *name) {
	char path[CRT_PATH];
	return remove(Crt_Path(name, path));
}

int crt_rename(const char *from, const char *to) {
	char a[CRT_PATH], b[CRT_PATH];
	return rename(Crt_Path(from, a), Crt_Path(to, b));
}

/* ---- low level io and the file system ---- */

/* the flags of the MSVC runtime: _O_WRONLY 1, _O_RDWR 2, _O_APPEND 8, _O_CREAT 0x100, _O_TRUNC 0x200,
 * _O_EXCL 0x400 (_O_TEXT 0x4000 and _O_BINARY 0x8000 mean nothing here) */
int crt_open(const char *name, int flags, ...) {
	char path[CRT_PATH];
	va_list ap;
	int mode = 0;
	int f = (flags & 3) == 1 ? O_WRONLY : (flags & 3) == 2 ? O_RDWR : O_RDONLY;

	if (flags & 0x8) {
		f |= O_APPEND;
	}
	if (flags & 0x200) {
		f |= O_TRUNC;
	}
	if (flags & 0x400) {
		f |= O_EXCL;
	}
	if (flags & 0x100) {
		f |= O_CREAT;
		va_start(ap, flags);
		mode = va_arg(ap, int) & 0x80 ? 0644 : 0444;	/* _S_IWRITE */
		va_end(ap);
	}
	return open(Crt_Path(name, path), f, mode);
}

int crt_read(int fd, void *buf, unsigned int size) {
	return (int)read(fd, buf, size);
}

int crt_close(int fd) {
	return close(fd);
}

long crt_lseek(int fd, long off, int origin) {
	return (long)lseek(fd, off, origin);
}

long crt_filelength(int fd) {
	struct stat st;
	return fstat(fd, &st) ? -1 : (long)st.st_size;
}

int crt_chsize(int fd, long size) {
	return ftruncate(fd, size);
}

int crt_isatty(int fd) {
	return isatty(fd);
}

/* struct _stat32 of the MSVC runtime: the callers read st_mtime at 0x1c, st_size at 0x14 */
typedef struct CrtStat32 {
	uint32_t dev;
	uint16_t ino, mode;
	int16_t nlink, uid, gid;
	uint16_t pad;
	uint32_t rdev;
	int32_t size, atime, mtime, ctime;
} CrtStat32;

static void Crt_Stat32(const struct stat *s, void *out) {
	/* the drive number (Z: is 25) in st_dev and st_rdev and no inode, as the runtime of Windows gives them */
	CrtStat32 st = {
		.dev = 25,
		.ino = 0,
		.mode = (uint16_t)((s->st_mode & S_IFDIR ? 0x4000 : 0x8000) | (s->st_mode & 0700)),
		.nlink = (int16_t)s->st_nlink,
		.rdev = 25,
		.size = (int32_t)s->st_size,
		.atime = (int32_t)s->st_atime,
		.mtime = (int32_t)s->st_mtime,
		.ctime = (int32_t)s->st_ctime,
	};
	memcpy(out, &st, sizeof(st));
}

int crt_stat32(const char *name, void *st) {
	char path[CRT_PATH];
	struct stat s;

	if (stat(Crt_Path(name, path), &s)) {
		return -1;
	}
	Crt_Stat32(&s, st);
	return 0;
}

int crt_fstat32(int fd, void *st) {
	struct stat s;

	if (fstat(fd, &s)) {
		return -1;
	}
	Crt_Stat32(&s, st);
	return 0;
}

/* the MSVC wildcards without case: '*' any run, '?' one character; "*.*" matches a name without a dot too */
static int Crt_Match(const char *pat, const char *name) {
	if (!strcmp(pat, "*.*")) {
		return 1;
	}
	for (; *pat; pat++, name++) {
		if (*pat == '*') {
			for (; *name; name++) {
				if (Crt_Match(pat + 1, name)) {
					return 1;
				}
			}
			return Crt_Match(pat + 1, name);
		}
		if (!*name || (*pat != '?' && tolower((unsigned char)*pat) != tolower((unsigned char)*name))) {
			return 0;
		}
	}
	return !*name;
}

/* a search of crt_findfirst32: the directory, its path and the pattern of the names */
typedef struct CrtFind {
	DIR *dir;
	char path[CRT_PATH];
	char pattern[260];
} CrtFind;

/* the next entry that matches into fd; 0, else -1 at the end */
static int Crt_FindNext(CrtFind *s, crt_finddata32 *fd) {
	struct dirent *de;

	while ((de = readdir(s->dir))) {
		if (!Crt_Match(s->pattern, de->d_name)) {
			continue;
		}
		char full[CRT_PATH * 2];
		struct stat st;
		snprintf(full, sizeof(full), "%s/%s", s->path, de->d_name);
		memset(fd, 0, sizeof(*fd));
		if (!stat(full, &st)) {
			fd->attrib = S_ISDIR(st.st_mode) ? CRT_A_SUBDIR : 0;
			fd->timeCreate = (int32_t)st.st_ctime;
			fd->timeAccess = (int32_t)st.st_atime;
			fd->timeWrite = (int32_t)st.st_mtime;
			fd->size = (uint32_t)st.st_size;
		}
		/* the dot files are what hidden files are on Windows */
		if (de->d_name[0] == '.' && strcmp(de->d_name, ".") && strcmp(de->d_name, "..")) {
			fd->attrib |= CRT_A_HIDDEN;
		}
		snprintf(fd->name, sizeof(fd->name), "%s", de->d_name);
		return 0;
	}
	return -1;
}

/* the handle is the search itself, never freed (the game does not close its searches, as in the original) */
intptr_t crt_findfirst32(const char *spec, crt_finddata32 *fd) {
	char path[CRT_PATH];
	CrtFind *s = crt_malloc(sizeof(CrtFind));

	if (!s) {
		return -1;
	}
	Crt_Path(spec, path);
	char *slash = strrchr(path, '/');
	if (slash) {
		*slash = 0;
		snprintf(s->path, sizeof(s->path), "%s", path[0] ? path : "/");
		snprintf(s->pattern, sizeof(s->pattern), "%.259s", slash + 1);
	} else {
		snprintf(s->path, sizeof(s->path), ".");
		snprintf(s->pattern, sizeof(s->pattern), "%.259s", path);
	}
	s->dir = opendir(s->path);
	if (!s->dir || Crt_FindNext(s, fd)) {
		if (s->dir) {
			closedir(s->dir);
		}
		crt_free(s);
		errno = ENOENT;
		return -1;
	}
	return (intptr_t)s;
}

int crt_findnext32(intptr_t h, crt_finddata32 *fd) {
	return Crt_FindNext((CrtFind *)h, fd);
}

int crt_access(const char *name, int mode) {
	char path[CRT_PATH];
	return access(Crt_Path(name, path), mode & 6);
}

int crt_chmod(const char *name, int mode) {
	char path[CRT_PATH];
	return chmod(Crt_Path(name, path), mode & 0x80 ? 0644 : 0444);	/* _S_IWRITE */
}

int crt_chdir(const char *dir) {
	char path[CRT_PATH];
	return chdir(Crt_Path(dir, path));
}

int crt_mkdir(const char *dir) {
	char path[CRT_PATH];
	return mkdir(Crt_Path(dir, path), 0755);
}

/* the working directory as Windows names it: "Z:" and the path with '\\' */
char *crt_getcwd(char *buf, int size) {
	char cwd[CRT_PATH];

	if (!getcwd(cwd, sizeof(cwd)) || (int)strlen(cwd) + 3 > size) {
		errno = ERANGE;
		return NULL;
	}
	snprintf(buf, (size_t)size, "Z:%s", cwd);
	for (char *c = buf; *c; c++) {
		if (*c == '/') {
			*c = '\\';
		}
	}
	return buf;
}

/* one drive, Z: (26), the root of the file system */
int crt_getdrive(void) {
	return 26;
}

int crt_chdrive(int drive) {
	return drive == 26 ? 0 : -1;
}

/* _splitpath: the drive "X:", the directory with its last separator, the name, the extension with its dot */
void crt_splitpath(const char *path, char *drive, char *dir, char *name, char *ext) {
	const char *p = path;

	if (drive) {
		drive[0] = 0;
	}
	if (isalpha((unsigned char)p[0]) && p[1] == ':') {
		if (drive) {
			drive[0] = p[0];
			drive[1] = ':';
			drive[2] = 0;
		}
		p += 2;
	}
	const char *slash = NULL, *dot = NULL;
	for (const char *c = p; *c; c++) {
		if (*c == '/' || *c == '\\') {
			slash = c;
			dot = NULL;
		} else if (*c == '.') {
			dot = c;
		}
	}
	const char *base = slash ? slash + 1 : p;
	const char *end = base + strlen(base);
	if (dir) {
		size_t n = (size_t)(base - p);
		memcpy(dir, p, n < 255 ? n : 255);
		dir[n < 255 ? n : 255] = 0;
	}
	if (!dot) {
		dot = end;
	}
	if (name) {
		size_t n = (size_t)(dot - base);
		memcpy(name, base, n < 255 ? n : 255);
		name[n < 255 ? n : 255] = 0;
	}
	if (ext) {
		snprintf(ext, 256, "%s", dot);
	}
}

/* _spawnlp: mode 0 (_P_WAIT) waits and gives the exit code, 1 (_P_NOWAIT) gives the process id; -1 on failure */
int crt_spawnlp(int mode, const char *cmd, const char *arg0, ...) {
	char *argv[64];
	char path[CRT_PATH];
	va_list ap;
	int n = 0;
	pid_t pid;

	va_start(ap, arg0);
	for (const char *a = arg0; a && n < (int)(sizeof(argv) / sizeof(argv[0])) - 1; a = va_arg(ap, const char *)) {
		argv[n++] = (char *)a;
	}
	va_end(ap);
	argv[n] = NULL;
	if (posix_spawnp(&pid, strchr(cmd, '/') || strchr(cmd, '\\') ? Crt_Path(cmd, path) : cmd, NULL, NULL, argv,
		environ)) {
		return -1;
	}
	if (mode == 0) {
		int status;
		if (waitpid(pid, &status, 0) < 0) {
			return -1;
		}
		return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	}
	return (int)pid;
}

/* ---- strings ---- */

int crt_stricmp(const char *a, const char *b) {
	for (;; a++, b++) {
		int x = tolower((unsigned char)*a), y = tolower((unsigned char)*b);
		if (x != y || !x) {
			return x - y;
		}
	}
}

int crt_strnicmp(const char *a, const char *b, size_t n) {
	for (; n; n--, a++, b++) {
		int x = tolower((unsigned char)*a), y = tolower((unsigned char)*b);
		if (x != y || !x) {
			return x - y;
		}
	}
	return 0;
}

int crt_memicmp(const void *a, const void *b, size_t n) {
	const unsigned char *p = a, *q = b;
	for (; n; n--, p++, q++) {
		int x = tolower(*p), y = tolower(*q);
		if (x != y) {
			return x - y;
		}
	}
	return 0;
}

char *crt_strlwr(char *s) {
	for (char *c = s; *c; c++) {
		*c = (char)tolower((unsigned char)*c);
	}
	return s;
}

char *crt_strupr(char *s) {
	for (char *c = s; *c; c++) {
		*c = (char)toupper((unsigned char)*c);
	}
	return s;
}

char *crt_strtok_s(char *s, const char *sep, char **ctx) {
	return strtok_r(s, sep, ctx);
}

char *crt_mbsrchr(const char *s, unsigned int c) {
	return (char *)strrchr(s, (int)c);
}

char *crt_ltoa(long v, char *buf, int radix) {
	char tmp[40];
	int n = 0;
	unsigned long u = radix == 10 && v < 0 ? (unsigned long)-v : (unsigned long)v;

	do {
		int d = (int)(u % (unsigned long)radix);
		tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
		u /= (unsigned long)radix;
	} while (u);
	char *p = buf;
	if (radix == 10 && v < 0) {
		*p++ = '-';
	}
	while (n) {
		*p++ = tmp[--n];
	}
	*p = 0;
	return buf;
}

/* ---- time ---- */

int32_t crt_time32(int32_t *t) {
	int32_t now = (int32_t)time(NULL);
	if (t) {
		*t = now;
	}
	return now;
}

void *crt_gmtime32(const int32_t *t) {
	time_t v = *t;
	return gmtime(&v);
}

double crt_difftime32(int32_t t1, int32_t t0) {
	return (double)t1 - (double)t0;
}

char *crt_ctime32(const int32_t *t) {
	time_t v = *t;
	return ctime(&v);
}

/* _strtime: "HH:MM:SS" of the local time */
char *crt_strtime(char *buf) {
	time_t now = time(NULL);
	struct tm tm;

	localtime_r(&now, &tm);
	strftime(buf, 9, "%H:%M:%S", &tm);
	return buf;
}

int *crt_errno(void) {
	return &errno;
}

/* ---- the fail fast of the compiler and the classification ---- */

[[noreturn]] void crt_report_rangecheckfailure(void) {
	abort();
}

/* _fpclass: SNAN 1, QNAN 2, NINF 4, NN 8, ND 0x10, NZ 0x20, PZ 0x40, PD 0x80, PN 0x100, PINF 0x200 */
int crt_fpclass(double x) {
	int neg = signbit(x) != 0;

	switch (fpclassify(x)) {
	case FP_NAN:
		return 2;
	case FP_INFINITE:
		return neg ? 4 : 0x200;
	case FP_ZERO:
		return neg ? 0x20 : 0x40;
	case FP_SUBNORMAL:
		return neg ? 0x10 : 0x80;
	default:
		return neg ? 8 : 0x100;
	}
}

/* ---- the math and strtod of the MSVC runtime, for the math module of the scripts (src/game/Script/ScriptLib.c) ----
 * The math module of 1.5.2 turns errno after a libm call into an exception. glibc sets ERANGE on every underflow and on the overflow
 * of ldexp, and EDOM for fmod(x, 0) of a finite x; the runtime of Windows does neither (the result is the answer: 0, inf or
 * NaN), but gives EDOM for sqrt and asin of NaN. */

/* ERANGE kept only for an overflow (an infinite result) unless over says the runtime has none there either */
static double Crt_MsRange(double r, int over) {
	if (errno == ERANGE && (isfinite(r) || over)) {
		errno = 0;
	}
	return r;
}

double crt_ms_pow(double x, double y) {
	return Crt_MsRange(pow(x, y), 0);
}

double crt_ms_exp(double x) {
	return Crt_MsRange(exp(x), 0);
}

double crt_ms_ldexp(double x, int e) {
	return Crt_MsRange(ldexp(x, e), 1);
}

double crt_ms_sinh(double x) {
	return Crt_MsRange(sinh(x), 0);
}

double crt_ms_cosh(double x) {
	return Crt_MsRange(cosh(x), 0);
}

double crt_ms_asin(double x) {
	double r = asin(x);
	if (isnan(x)) {
		errno = EDOM;
	}
	return r;
}

double crt_ms_atan2(double y, double x) {
	return Crt_MsRange(atan2(y, x), 0);
}

double crt_ms_hypot(double x, double y) {
	return Crt_MsRange(hypot(x, y), 0);
}

double crt_ms_fmod(double x, double y) {
	double r = fmod(x, y);
	if (errno == EDOM && isfinite(x)) {
		errno = 0;
	}
	return r;
}

double crt_ms_sqrt(double x) {
	double r = sqrt(x);
	if (isnan(x)) {
		errno = EDOM;
	}
	return r;
}

/* strtod as the runtime of Windows reads: 'd' or 'D' as the exponent like 'e', no "nan(...)" with a payload, and
 * no ERANGE for a result that is only denormal */
double crt_ms_strtod(const char *s, char **end) {
	const char *p = s;
	char buf[512];

	while (isspace((unsigned char)*p)) {
		p++;
	}
	if (*p == '+' || *p == '-') {
		p++;
	}
	if (!strncasecmp(p, "nan(", 4)) {
		if (end) {
			*end = (char *)s;
		}
		return 0.0;
	}
	const char *q = p;
	while (isdigit((unsigned char)*q) || *q == '.') {
		q++;
	}
	const char *src = s;
	if ((*q == 'd' || *q == 'D') && (isdigit((unsigned char)q[1]) || ((q[1] == '+' || q[1] == '-')
		&& isdigit((unsigned char)q[2]))) && strlen(s) < sizeof(buf)) {
		snprintf(buf, sizeof(buf), "%s", s);
		buf[q - s] = 'e';
		src = buf;
	}
	char *e;
	double r = strtod(src, &e);
	if (end) {
		*end = (char *)s + (e - src);
	}
	if (errno == ERANGE && r != 0.0 && isfinite(r)) {
		errno = 0;
	}
	return r;
}
