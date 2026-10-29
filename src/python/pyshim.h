/*
 * Force-included into every source of the Python 1.5.2 library (CMakeLists.txt), before anything else. The library
 * must behave like the Python of Scrap.exe, so it uses what that one used, and the vendored sources stay unchanged:
 * - malloc, realloc and free are the engine's Python pool (src/game/PyInterface/pymempool.c), as in the exe
 * - the inline stdio functions of the UCRT headers (sprintf, printf, ...) are the ones of the C code
 *   (src/game/Global/StdioInline.c), the rest of the libc that is not pure string work is the C runtime of the
 *   original (include/scrap/crt.h): FILE objects, errno, locale, ctype, strtod and the float formatting are the exe's
 * - math: calls go to the SSE2 entries the MSVC compiler calls (src/core/libm.c), the address of a function (the math
 *   module's tables) is the cdecl function of the runtime. "#define sin crt_sin" with a function-like crt_sin does
 *   both: sin(x) becomes Game_Sin(x), a bare sin stays crt_sin.
 * The system headers come first, so their prototypes keep the plain names.
 */
#ifndef SCRAP_PYSHIM_H
#define SCRAP_PYSHIM_H

/* ours first: PC/dl_nt.c includes "config.h" from its own folder, the MSVC one, which has the same guard */
#include "config.h"

#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <io.h>
#include <locale.h>
#include <malloc.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "scrap/crt.h"

/* C code of the engine, declared here because its headers are C23 (python.h, mathlib.h, e3dutil.h) */
void *PyPool_Malloc(size_t size);
void *PyPool_Realloc(void *p, size_t size);
void PyPool_Free(void *p);
void DataFile_Printf(const char *fmt, ...);	/* the inline printf of the CRT headers, folded with it in the exe */
double Game_Sin(double x);
double Game_Cos(double x);
double Game_Atan(double x);
double Game_Atan2(double y, double x);
double Game_Fmod(double x, double y);
double Game_Exp(double x);
double Game_Log(double x);
double Game_Pow(double x, double y);

/* crt_fclose with the FILE * of the sources, which declare fclose themselves and pass it as a pointer */
int crt_fclose_file(FILE *f) __asm__("_crt_fclose");

/* _stat32 of the exe's CRT; "#define stat crt_stat32" also renames the struct tag of "struct stat" */
struct crt_stat32 {
	unsigned int st_dev;
	unsigned short st_ino;
	unsigned short st_mode;
	short st_nlink;
	short st_uid;
	short st_gid;
	unsigned int st_rdev;
	long st_size;
	long st_atime;
	long st_mtime;
	long st_ctime;
};

#define malloc PyPool_Malloc
#define realloc PyPool_Realloc
#define free PyPool_Free
#define strdup crt_strdup

#define sprintf crt_sprintf
#define vsprintf crt_vsprintf
#define fprintf crt_fprintf
#define vfprintf crt_vfprintf
#define printf DataFile_Printf
#define sscanf crt_sscanf

#undef stdin
#undef stdout
#undef stderr
#define stdin ((FILE *)crt_iob_func(0))
#define stdout ((FILE *)crt_iob_func(1))
#define stderr ((FILE *)crt_iob_func(2))
#define fopen(name, mode) ((FILE *)crt_fopen(name, mode))
#define fclose crt_fclose_file
#define fread crt_fread
#define fwrite crt_fwrite
#define fputs crt_fputs
#undef putc
#undef getc
#define fputc crt_fputc
#define putc crt_fputc
#define fgetc crt_fgetc
#define getc crt_fgetc
#define ungetc crt_ungetc
#define fgets crt_fgets
#define fflush crt_fflush
#define fseek crt_fseek
#define ftell crt_ftell
#define ferror crt_ferror
#define feof crt_feof
#define clearerr crt_clearerr
#define setvbuf crt_setvbuf
#define fileno crt_fileno
#define _fileno crt_fileno	/* fileobject.c renames fileno and ftruncate for MS_WIN32 */
#define _chsize crt_chsize
#define isatty crt_isatty
#define lseek crt_lseek
#define chsize crt_chsize
#define unlink crt_remove
#define stat crt_stat32
#define fstat crt_fstat32
#define getcwd crt_getcwd
#define _getcwd crt_getcwd
#define strnicmp crt_strnicmp
#define _strnicmp crt_strnicmp

#undef errno
#define errno (*crt_errno())
#define strerror crt_strerror
#define getenv crt_getenv
#define abort crt_abort
#define exit crt_exit
#define signal crt_signal
#define setlocale crt_setlocale
#define localeconv crt_localeconv
#define strcoll crt_strcoll
#define strxfrm crt_strxfrm
#define strtod crt_strtod
#define atof crt_atof

#undef isalnum
#undef isalpha
#undef isdigit
#undef islower
#undef isspace
#undef isupper
#undef isxdigit
#undef tolower
#undef toupper
#define isalnum crt_isalnum
#define isalpha crt_isalpha
#define isdigit crt_isdigit
#define islower crt_islower
#define isspace crt_isspace
#define isupper crt_isupper
#define isxdigit crt_isxdigit
#define tolower crt_tolower
#define toupper crt_toupper

#define acos crt_acos
#define asin crt_asin
#define atan crt_atan
#define atan2 crt_atan2
#define ceil crt_ceil
#define cos crt_cos
#define cosh crt_cosh
#define exp crt_exp
#define fabs crt_fabs
#define floor crt_floor
#define fmod crt_fmod
#define frexp crt_frexp
#define hypot crt_hypot
#define ldexp crt_ldexp
#define log crt_log
#define log10 crt_log10
#define modf crt_modf
#define pow crt_pow
#define sin crt_sin
#define sinh crt_sinh
#define sqrt crt_sqrt
#define tan crt_tan
#define tanh crt_tanh
#define crt_atan(x) Game_Atan(x)
#define crt_atan2(y, x) Game_Atan2(y, x)
#define crt_cos(x) Game_Cos(x)
#define crt_exp(x) Game_Exp(x)
#define crt_fmod(x, y) Game_Fmod(x, y)
#define crt_log(x) Game_Log(x)
#define crt_pow(x, y) Game_Pow(x, y)
#define crt_sin(x) Game_Sin(x)

#endif
