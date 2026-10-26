/*
 * The engine's allocator on the CRT heap of the exe, with statistics of the whole game and of the current
 * context (g_MemStatsContext, e.g. the Python pool). The original source file is not known (Global/).
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/game.h"

static void MemStats_Alloc(cMemStats *st, int size) {
	st->blocks++;
	st->bytes += size;
	st->allocs++;
	if (st->peakBlocks < st->blocks) {
		st->peakBlocks = st->blocks;
	}
	if (st->peakBytes < st->bytes) {
		st->peakBytes = st->bytes;
	}
}

static void MemStats_Free(cMemStats *st, int size) {
	st->frees++;
	st->blocks--;
	st->bytes -= size;
}

/* counts an allocation (freed 0) or a free of size bytes made elsewhere */
void Mem_Count(int freed, int size) {
	if (!freed) {
		MemStats_Alloc(g_MemStats, size);
	} else {
		MemStats_Free(g_MemStats, size);
	}
	if (g_MemStatsContext) {
		if (!freed) {
			MemStats_Alloc(g_MemStatsContext, size);
		} else {
			MemStats_Free(g_MemStatsContext, size);
		}
	}
}

void Mem_OutOfMemory(uint32_t size) {
	Con_FatalErrorf("Not enough memory (queried: %d, used: %d)", size, g_MemStats->bytes);
}

/* NULL and "Not enough memory" in the log if the heap has no room */
void *Mem_Alloc(uint32_t size) {
	void *p = crt_malloc(size);
	if (!p) {
		Mem_OutOfMemory(size);
		return NULL;
	}
	Mem_Count(0, (int)crt_msize(p));
	return p;
}

/* NULL, callers in the exe do p = Mem_Free(p) */
void *Mem_Free(void *p) {
	if (p) {
		Mem_Count(1, (int)crt_msize(p));
		crt_free(p);
	}
	return NULL;
}

void *operator_new(uint32_t size) {
	return Mem_Alloc(size);
}

void operator_delete(void *p) {
	Mem_Free(p);
}

void *Mem_Calloc(uint32_t size) {
	void *p = Mem_Alloc(size);
	memset(p, 0, size);
	return p;
}

uint32_t Mem_Size(void *p) {
	return p ? (uint32_t)crt_msize(p) : 0;
}

void *Mem_Realloc(void *p, uint32_t size) {
	if (p) {
		Mem_Count(1, (int)crt_msize(p));
	}
	void *np = crt_realloc(p, size);
	if (!np) {
		if (size) {
			Mem_OutOfMemory(size);
		}
		return NULL;
	}
	Mem_Count(0, (int)crt_msize(np));
	return np;
}
