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
