/*
 * Block pools for small allocations (original source PyInterface/pymempool.cpp, the pools of the interpreter). A pool
 * has 32 classes of 4 to 128 bytes, each a fixed number of blocks with a bit per block, all in one Mem_Alloc block.
 * Larger sizes and a full class go to Mem_Alloc. The script pool (named "Python" in its console lines, as in the exe)
 * is made on first use with the counts of g_ScriptPoolValues.
 */
#ifndef SCRAP_MEMPOOL_H
#define SCRAP_MEMPOOL_H

#include "scrap/game.h"

#define MEMPOOL_CLASSES 32

typedef struct cMemBlocks {
	uint32_t blockSize;
	uint32_t numBlocks;
	uint32_t numWords;		/* words of status */
	uint32_t usedBlocks;
	uint32_t *status;		/* bit set: block in use */
	byte *data;
} cMemBlocks;

typedef struct cMemPool {
	cMemBlocks **blocks;		/* MEMPOOL_CLASSES, class i has blocks of (i + 1) * 4 bytes */
	byte *memory;
	uint32_t fullSize;
	const char *name;
	struct cMemPool *next;		/* g_MemPools */
} cMemPool;

extern cMemStats g_ScriptMemStats;	/* the context of the script pool's allocations */

cMemPool *cMemPool_ctor(cMemPool *self, const char *name, const uint32_t *values);
void cMemPool_dtor(cMemPool *self);
void *cMemPool_Alloc(cMemPool *self, uint32_t size);
void cMemPool_Free(cMemPool *self, void *p);

/* malloc, realloc and free on the script pool (0x193620, 0x1935c0: the allocator of the Python library in the exe),
 * counted in g_ScriptMemStats */
void *ScriptPool_Malloc(size_t size);
void *ScriptPool_Realloc(void *p, size_t size);
void ScriptPool_Free(void *p);
/* deletes the script pool at the end */
void ScriptPool_Delete(void);

#endif
