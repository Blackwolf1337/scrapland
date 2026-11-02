/*
 * Force-included into every source of the Python 1.5.2 library (CMakeLists.txt), before anything else, so the
 * vendored sources stay unchanged: malloc, realloc and free are the engine's Python pool
 * (src/game/PyInterface/pymempool.c), as in the exe, and printf goes to the engine's output. The rest of the libc is
 * the platform's.
 */
#ifndef SCRAP_PYSHIM_H
#define SCRAP_PYSHIM_H

/* ours first: PC/dl_nt.c includes "config.h" from its own folder, the MSVC one, which has the same guard */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
/* the static getline of Objects/fileobject.c, not the one of POSIX 2008 in stdio.h */
#define getline py152_getline

/* the libm and strtod of the Windows runtime as Python sees them through errno (src/standalone/crtposix.c) */
#include <math.h>
double crt_ms_pow(double x, double y);
double crt_ms_exp(double x);
double crt_ms_ldexp(double x, int e);
double crt_ms_sinh(double x);
double crt_ms_cosh(double x);
double crt_ms_atan2(double y, double x);
double crt_ms_hypot(double x, double y);
double crt_ms_fmod(double x, double y);
double crt_ms_sqrt(double x);
double crt_ms_asin(double x);
double crt_ms_strtod(const char *s, char **end);
#define pow crt_ms_pow
#define exp crt_ms_exp
#define ldexp crt_ms_ldexp
#define sinh crt_ms_sinh
#define cosh crt_ms_cosh
#define atan2 crt_ms_atan2
#define hypot crt_ms_hypot
#define fmod crt_ms_fmod
#define sqrt crt_ms_sqrt
#define asin crt_ms_asin
#define strtod crt_ms_strtod
#endif

/* C code of the engine, declared here because its headers are C23 (python.h, e3dutil.h) */
void *PyPool_Malloc(size_t size);
void *PyPool_Realloc(void *p, size_t size);
void PyPool_Free(void *p);
void DataFile_Printf(const char *fmt, ...);

#define malloc PyPool_Malloc
#define realloc PyPool_Realloc
#define free PyPool_Free
#define printf DataFile_Printf

#endif
