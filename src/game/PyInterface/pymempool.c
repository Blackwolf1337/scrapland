/*
 * Block pools of the interpreter's allocator. Original source: PyInterface/pymempool.cpp. PyPool_Malloc,
 * PyPool_Realloc and PyPool_Free are the malloc, realloc and free of the Python code in the exe; they count
 * into the Python memory statistics.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/pymempool.h"
#include "scrap/python.h"

/* class of a size: words of 4 bytes, 1 to 32; 0 or larger sizes have none */
static inline uint32_t SizeWords(uint32_t size) {
	return (size >> 2) + ((size & 3) != 0);
}

cMemBlocks *cMemBlocks_ctor(cMemBlocks *self, uint32_t blockSize, uint32_t numBlocks) {
	self->blockSize = blockSize;
	self->numBlocks = numBlocks;
	self->usedBlocks = 0;
	self->numWords = (numBlocks >> 5) + ((numBlocks & 0x1f) != 0);
	self->status = NULL;
	self->data = NULL;
	return self;
}

void cMemBlocks_SetMemory(cMemBlocks *self, uint32_t *status, byte *data) {
	self->status = status;
	memset(status, 0, self->numWords * 4);
	self->data = data;
}

byte cMemBlocks_Owns(cMemBlocks *self, const void *p) {
	return self->usedBlocks && (uintptr_t)self->data <= (uintptr_t)p &&
		(uintptr_t)p <= (uintptr_t)self->data + (self->numBlocks - 1) * self->blockSize;
}

/* a free block; with every block in use the memory comes from Mem_Alloc */
void *cMemBlocks_Alloc(cMemBlocks *self) {
	if (self->usedBlocks == self->numBlocks) {
		return Mem_Alloc(self->blockSize);
	}
	CHECK(self->usedBlocks < self->numBlocks);
	for (uint32_t w = 0; w < self->numWords; w++) {
		if (self->status[w] == 0xffffffff) {
			continue;
		}
		for (uint32_t bit = 0; bit < 32; bit++) {
			uint32_t mask = 1u << bit;
			if (!(self->status[w] & mask) && w * 32 + bit < self->numBlocks) {
				self->status[w] |= mask;
				self->usedBlocks++;
				return self->data + (bit + w * 32) * self->blockSize;
			}
		}
	}
	CHECK(0);
	return NULL;
}

void cMemBlocks_Free(cMemBlocks *self, void *p) {
	if (!cMemBlocks_Owns(self, p)) {
		Mem_Free(p);
		return;
	}
	self->usedBlocks--;
	uint32_t off = (uint32_t)((byte *)p - self->data);
	CHECK(off % self->blockSize == 0);
	uint32_t idx = off / self->blockSize;
	CHECK(self->status[idx / 32] & (1u << (idx % 32)));
	self->status[idx / 32] &= ~(1u << (idx % 32));
}

cMemPool *cMemPool_ctor(cMemPool *self, const char *name, const uint32_t *values) {
	self->blocks = Mem_Alloc(MEMPOOL_CLASSES * sizeof(*self->blocks));
	for (uint32_t i = 0; i < MEMPOOL_CLASSES; i++) {
		cMemBlocks *b = operator_new(sizeof(*b));
		if (b) {
			cMemBlocks_ctor(b, (i + 1) * 4, values[i]);
		}
		self->blocks[i] = b;
	}
	self->fullSize = 0;
	for (uint32_t i = 0; i < MEMPOOL_CLASSES; i++) {
		cMemBlocks *b = self->blocks[i];
		self->fullSize += b->numBlocks * b->blockSize + b->numWords * 4;
	}
	byte *mem = Mem_Alloc(self->fullSize);
	self->memory = mem;
	for (uint32_t i = 0; i < MEMPOOL_CLASSES; i++) {
		cMemBlocks *b = self->blocks[i];
		cMemBlocks_SetMemory(b, (uint32_t *)(mem + b->numBlocks * b->blockSize), mem);
		mem += b->numWords * 4 + b->numBlocks * b->blockSize;
	}
	self->name = name;
	self->next = g_MemPools;
	g_MemPools = self;
	Con_Printf("%s Pools = %d bytes\n", name, self->fullSize);
	return self;
}

/* the pool stays in g_MemPools, like in the original */
void cMemPool_dtor(cMemPool *self) {
	for (uint32_t i = 0; i < MEMPOOL_CLASSES; i++) {
		if (self->blocks[i]) {
			operator_delete_sized(self->blocks[i], sizeof(cMemBlocks));
		}
	}
	Mem_Free(self->blocks);
	Mem_Free(self->memory);
	Con_Printf("%s Pools deleted!\n", self->name);
}

/* the class of p, or -1 when it is not a pool block (CHECK if inside the pool memory) */
static int cMemPool_Find(cMemPool *self, const void *p) {
	if ((uintptr_t)p < (uintptr_t)self->memory || (uintptr_t)p >= (uintptr_t)self->memory + self->fullSize) {
		return -1;
	}
	for (int i = 0; i < MEMPOOL_CLASSES; i++) {
		if (cMemBlocks_Owns(self->blocks[i], p)) {
			return i;
		}
	}
	CHECK(0);
	return -1;
}

void *cMemPool_Alloc(cMemPool *self, uint32_t size) {
	uint32_t words = SizeWords(size);
	if (!size || words > MEMPOOL_CLASSES) {
		return Mem_Alloc(size);
	}
	return cMemBlocks_Alloc(self->blocks[words - 1]);
}

void cMemPool_Free(cMemPool *self, void *p) {
	int i = cMemPool_Find(self, p);
	if (i < 0) {
		Mem_Free(p);
		return;
	}
	cMemBlocks_Free(self->blocks[i], p);
}

/* a pool block moves to the class of the new size (Mem_Alloc if it has none) */
void *cMemPool_Realloc(cMemPool *self, void *p, uint32_t size) {
	int i = cMemPool_Find(self, p);
	if (i < 0) {
		return Mem_Realloc(p, size);
	}
	void *np = cMemPool_Alloc(self, size);
	uint32_t n = self->blocks[i]->blockSize;
	memcpy(np, p, n < size ? n : size);
	cMemBlocks_Free(self->blocks[i], p);
	return np;
}

static void cMemPool_PrintValues(cMemPool *self) {
	Con_Printf("//////////////////////////////////////\n");
	Con_Printf("// values for the memory block manager (%s)\n", self->name);
	Con_Printf("static uint32 MemManagerValues[] = {\n");
	for (uint32_t i = 0; i < MEMPOOL_CLASSES; i++) {
		cMemBlocks *b = self->blocks[i];
		uint32_t usage = b->numBlocks ? b->usedBlocks * 100 / b->numBlocks : 100;
		Con_Printf(i == MEMPOOL_CLASSES - 1 ? "%8d}; // size = %7d   usage = %7d\n" :
			"%8d,  // size = %7d   usage = %7d\n", b->usedBlocks, b->blockSize, usage);
	}
	Con_Printf("// Full Size: %8d \n", self->fullSize);
	Con_Printf("//////////////////////////////////////\n\n\n\n\n");
}

void MemPools_PrintAll(void) {
	for (cMemPool *p = g_MemPools; p; p = p->next) {
		cMemPool_PrintValues(p);
	}
}

static cMemPool *PyPool_Get(void) {
	if (!g_PyPool) {
		cMemPool *mem = operator_new(sizeof(*mem));
		g_PyPool = mem ? cMemPool_ctor(mem, "Python", g_PyPoolValues) : NULL;
	}
	return g_PyPool;
}

void *PyPool_MallocImpl(uint32_t size) {
	return cMemPool_Alloc(PyPool_Get(), size);
}

void *PyPool_ReallocImpl(void *p, uint32_t size) {
	return cMemPool_Realloc(PyPool_Get(), p, size);
}

void PyPool_FreeImpl(void *p) {
	cMemPool_Free(PyPool_Get(), p);
}

void PyPool_Delete(void) {
	cMemPool *pool = g_PyPool;
	if (pool) {
		cMemPool_dtor(pool);
		operator_delete_sized(pool, sizeof(*pool));
		g_PyPool = NULL;
	}
}

/* the entry points set the statistics context only for the call (and leave it NULL) */
void *PyPool_Malloc(size_t size) {
	g_MemStatsContext = g_PyMemStats;
	void *p = PyPool_MallocImpl(size);
	g_MemStatsContext = NULL;
	return p;
}

void *PyPool_Realloc(void *p, size_t size) {
	g_MemStatsContext = g_PyMemStats;
	void *np = PyPool_ReallocImpl(p, size);
	g_MemStatsContext = NULL;
	return np;
}

void PyPool_Free(void *p) {
	g_MemStatsContext = g_PyMemStats;
	PyPool_FreeImpl(p);
	g_MemStatsContext = NULL;
}
