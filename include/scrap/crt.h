/*
 * The C runtime of Scrap.exe (statically linked UCRT), called through src/imports.inc. Used where the C code has
 * to share state with the exe (FILE objects, errno, locale, the current drive) or must behave like it bit for bit;
 * plain string work uses the CRT of the DLL.
 */
#ifndef SCRAP_CRT_H
#define SCRAP_CRT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* _finddata32_t of the exe's CRT */
typedef struct crt_finddata32 {
	uint32_t attrib;		/* 0x00 */
	int32_t timeCreate;		/* 0x04 */
	int32_t timeAccess;		/* 0x08 */
	int32_t timeWrite;		/* 0x0c */
	uint32_t size;			/* 0x10 */
	char name[260];			/* 0x14 */
} crt_finddata32;

enum { CRT_A_HIDDEN = 0x2, CRT_A_SYSTEM = 0x4, CRT_A_SUBDIR = 0x10 };

int crt_rand(void);
void crt_srand(unsigned int seed);
void *crt_malloc(size_t size);
void crt_free(void *p);
void *crt_realloc(void *p, size_t size);
size_t crt_msize(void *p);
int crt_fprintf(void *file, const char *fmt, ...);
int crt_fflush(void *f);
int32_t crt_time32(int32_t *t);
void *crt_gmtime32(const int32_t *t);
size_t crt_strftime(char *buf, size_t size, const char *fmt, const void *tm);
[[noreturn]] void crt_exit_now(int code);
[[noreturn]] void crt_report_rangecheckfailure(void);
void *crt_fopen(const char *name, const char *mode);
void *crt_fsopen(const char *name, const char *mode, int share);
int crt_fclose(void *f);
size_t crt_fread(void *dst, size_t size, size_t count, void *f);
size_t crt_fwrite(const void *src, size_t size, size_t count, void *f);
int crt_fputc(int c, void *f);
char *crt_setlocale(int category, const char *locale);
int crt_remove(const char *name);
int crt_rename(const char *from, const char *to);
int crt_fseek(void *f, long off, int origin);
long crt_ftell(void *f);
int crt_stricmp(const char *a, const char *b);
int crt_strncmp(const char *a, const char *b, size_t n);
char *crt_asctime(const void *tm);
/* _strtime: "hh:mm:ss" into buf (9 bytes) */
char *crt_strtime(char *buf);
void crt_qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));
int crt_atoi(const char *s);
double crt_atof(const char *s);
char *crt_strncat(char *dst, const char *src, size_t n);
int crt_stdio_common_vsprintf(uint64_t options, char *buf, size_t count, const char *fmt, void *locale, va_list ap);
int crt_strnicmp(const char *a, const char *b, size_t n);
char *crt_strlwr(char *s);
int crt_fputs(const char *s, void *f);
int crt_feof(void *f);
char *crt_fgets(char *buf, int size, void *f);
int crt_setvbuf(void *f, char *buf, int mode, size_t size);
int crt_access(const char *name, int mode);
int crt_stat32(const char *name, void *st);
[[noreturn]] void crt_exit(int code);
intptr_t crt_findfirst32(const char *spec, crt_finddata32 *fd);
int crt_findnext32(intptr_t h, crt_finddata32 *fd);
int crt_memicmp(const void *a, const void *b, size_t n);
int crt_chdir(const char *dir);
void crt_splitpath(const char *path, char *drive, char *dir, char *name, char *ext);
char *crt_strtok_s(char *s, const char *sep, char **ctx);
int *crt_errno(void);
int crt_getdrive(void);
int crt_chdrive(int drive);
char *crt_getcwd(char *buf, int size);
int crt_mkdir(const char *dir);
int crt_isalpha(int c);
int crt_isdigit(int c);
int crt_toupper(int c);
int crt_tolower(int c);
char *crt_strupr(char *s);
int crt_sscanf(const char *s, const char *fmt, ...);
int crt_sprintf(char *buf, const char *fmt, ...);
int crt_fgetc(void *f);
int crt_ungetc(int c, void *f);
int crt_isalnum(int c);
int crt_isxdigit(int c);
void *crt_iob_func(unsigned index);
int crt_stdio_common_vfprintf(uint64_t options, void *f, const char *fmt, void *locale, va_list ap);
size_t crt_strcspn(const char *s, const char *set);
int crt_fpclass(double x);
double crt_floor(double x);
double crt_ceil(double x);
char *crt_mbsrchr(const char *s, unsigned int c);
int crt_spawnlp(int mode, const char *cmd, const char *arg0, ...);

#endif
