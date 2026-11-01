/*
 * PyInterface/pymempool.cpp: block pools for the small allocations of the interpreter. A pool has 32 classes
 * of 4 to 128 bytes, each a fixed number of blocks with a bit per block, all in one Mem_Alloc block. Larger
 * sizes and a full class go to Mem_Alloc. The "Python" pool is made on first use with the counts of
 * g_PyPoolValues.
 */
#ifndef SCRAP_PYMEMPOOL_H
#define SCRAP_PYMEMPOOL_H

#include "scrap/game.h"

#define MEMPOOL_CLASSES 32

typedef struct cMemBlocks {
	uint32_t blockSize;		/* 0x00 */
	uint32_t numBlocks;		/* 0x04 */
	uint32_t numWords;		/* 0x08 words of status */
	uint32_t usedBlocks;		/* 0x0c */
	uint32_t *status;		/* 0x10 bit set: block in use */
	byte *data;			/* 0x14 */
} cMemBlocks;
SD_SIZE(cMemBlocks, 0x18);

typedef struct cMemPool {
	cMemBlocks **blocks;		/* 0x00 MEMPOOL_CLASSES, class i has blocks of (i + 1) * 4 bytes */
	byte *memory;			/* 0x04 */
	uint32_t fullSize;		/* 0x08 */
	const char *name;		/* 0x0c */
	struct cMemPool *next;		/* 0x10 g_MemPools */
} cMemPool;
SD_SIZE(cMemPool, 0x14);

#define g_PyPool GAME_GLOBAL(cMemPool *, g_PyPool)
#define g_MemPools GAME_GLOBAL(cMemPool *, g_MemPools)
#define g_PyPoolValues (&GAME_GLOBAL_IN(uint32_t, g_PyPoolValues, 0))	/* MEMPOOL_CLASSES block counts */
#define g_PyMemStats (GAME_ARRAY(cMemStats, g_PyMemStats))	/* the context of the interpreter's allocations */

cMemPool *__thiscall cMemPool_ctor(cMemPool *self, const char *name, const uint32_t *values);
void __fastcall cMemPool_dtor(cMemPool *self);
void *__thiscall cMemPool_Alloc(cMemPool *self, uint32_t size);
void __thiscall cMemPool_Free(cMemPool *self, void *p);

/* deletes the "Python" pool at the end (after Py_Finalize) */
void PyPool_Delete(void);

#endif
