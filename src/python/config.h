/*
 * config.h of the Python 1.5.2 library of ScrapC (mingw-w64 or Linux, i686), derived from the MSVC section of
 * third_party/python-1.5.2/PC/config.h with the choices of the build linked into Scrap.exe:
 * - static: no MS_COREDLL (_PyObject_New takes only the type, no sys.dllhandle or sys.winver)
 * - no WITH_THREAD: the exe has no thread module and no interpreter lock (no PyEval_AcquireThread)
 * - COMPILER and PLATFORM as in the exe, so sys.version and sys.platform read the same (the build date and time of
 *   the exe are set for Modules/getbuildinfo.c in CMakeLists.txt)
 * The libc of the library is the C runtime of the original (src/python/pyshim.h), as for the rest of the C code.
 * On Linux the library is the POSIX one, with the same sys.version and sys.platform ("win32") for the scripts.
 */
#ifndef Py_CONFIG_H
#define Py_CONFIG_H

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#define HAVE_UNISTD_H 1
#endif
#include <stdio.h>

#define HAVE_LIMITS_H
#define HAVE_HYPOT
#define DONT_HAVE_SIG_ALARM
#define DONT_HAVE_SIG_PAUSE
#define LONG_BIT 32
#define PREFIX ""
#define EXEC_PREFIX ""

#ifdef _WIN32
#define NT	/* NT is obsolete - please use MS_WIN32 instead */
#define MS_WIN32
#define MS_WINDOWS
#endif

#define COMPILER "[MSC 32 bit (Intel)]"
#define PYTHONPATH ".\\DLLs;.\\lib;.\\lib\\plat-win;.\\lib\\lib-tk"
#ifdef _WIN32
typedef int pid_t;
#endif
#define WORD_BIT 32
#define HAVE_CLOCK
#define HAVE_STRFTIME
#define HAVE_STRERROR

#define DL_IMPORT(RTYPE) RTYPE
#define DL_EXPORT(RTYPE) RTYPE
#define HAVE_LONG_LONG 1
#define LONG_LONG long long

#define PLATFORM "win32"
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_VOID_P 4
#ifdef _WIN32
#define CHECK_IMPORT_CASE
#endif

#define HAVE_TZNAME
#define RETSIGTYPE void
#define STDC_HEADERS 1
/* no BAD_STATIC_FORWARD (MSVC needs it): staticforward is static, as gcc wants it for the later static definition */
#define HAVE_PUTENV
#define HAVE_PROTOTYPES
#define HAVE_FTIME
#define HAVE_GETPEERNAME
#define HAVE_GETPID
#define HAVE_MKTIME
#define HAVE_SETVBUF
#define HAVE_FCNTL_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDARG_PROTOTYPES
#define HAVE_STDDEF_H 1
#define HAVE_STDLIB_H 1
#define HAVE_SYS_UTIME_H 1
#define HAVE_LIBNSL 1
#define HAVE_LIBSOCKET 1

#endif /* !Py_CONFIG_H */
