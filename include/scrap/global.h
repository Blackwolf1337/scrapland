/*
 * Engine basics of the original Global/ folder: the CHECK macro and the string hash table.
 */
#ifndef SCRAP_GLOBAL_H
#define SCRAP_GLOBAL_H

#include <stdarg.h>

#include "scrap/crt.h"
#include "scrap/shared.h"

/* "FATAL ERROR: Check failed!" report of the engine, logs expression, file, date and line */
void CheckFailed(const char *expr, const char *file, const char *date, int line);

/* the original macro passes __FILE__ and __TIMESTAMP__ of the .cpp file, ours those of the .c file; SD_CODECMP
   (tools/codecmp.sh) leaves out the expression text too, so a rename does not change the binary */
#ifdef SD_CODECMP
#define CHECK_EXPR(expr) ""
#else
#define CHECK_EXPR(expr) #expr
#endif
#define CHECK(expr) \
	do { \
		if (!(expr)) { \
			CheckFailed(CHECK_EXPR(expr), __FILE__, __TIMESTAMP__, __LINE__); \
		} \
	} while (0)

/* memory statistics: of the whole game (g_MemStats) and of the context set in g_MemStatsContext */
typedef struct cMemStats {
	int bytes;			/* 0x00 */
	int peakBytes;			/* 0x04 */
	int blocks;			/* 0x08 */
	int peakBlocks;			/* 0x0c */
	int allocs;			/* 0x10 */
	int frees;			/* 0x14 */
} cMemStats;
SD_SIZE(cMemStats, 0x18);

#define g_MemStats (&GAME_GLOBAL(cMemStats, g_MemStats))
#define g_MemStatsContext GAME_GLOBAL(cMemStats *, g_MemStatsContext)

/* the engine's operator new and sized delete (rva 0x1334d0, 0x313634; the plain delete is 0x1334e0), with its memory statistics */
void *operator_new(uint32_t size);
void operator_delete_sized(void *p, uint32_t size);
void *operator_new_array(uint32_t size);
void operator_delete(void *p);
void operator_delete_array(void *p);
/* the CRT loops of new[] and delete[] for objects: ctor or dtor (thiscall) on each */
void __stdcall crt_eh_vector_ctor(void *p, uint32_t size, uint32_t count, void *ctor, void *dtor);
void __stdcall crt_eh_vector_dtor(void *p, uint32_t size, uint32_t count, void *dtor);
/* new T[count] of the exe: the count in the 4 bytes before the objects, the constructor (thiscall, GAME_FUNC) on
 * each; NULL if the allocation fails. Delete_Objects is the delete[] of it. (src/game/Global/Mem.c) */
void *New_Objects(uint32_t count, uint32_t size, void *ctor, void *dtor);
void Delete_Objects(void *p, uint32_t size, void *dtor);
/* the array part of a vector deleting destructor (flags 2): the dtor on each object, the block freed with flags 1;
 * returns the block */
void *Delete_VectorPart(void *p, uint32_t flags, uint32_t size, void *dtor);
/* the shared stub of the empty constructors (src/game/Global/Stubs.c) */
void *__thiscall Stub_ReturnSelf(void *self);
/* new[] of count items as the compiler makes it: a size that overflows is -1, which the allocator refuses */
static inline void *New_Array(uint32_t count, uint32_t size) {
	uint64_t bytes = (uint64_t)count * size;
	return operator_new_array(bytes >> 32 ? 0xffffffffu : (uint32_t)bytes);
}
/* one character of the string hash of the engine: 4 bit shift, folded to 28 bits */
static inline uint32_t Hash_Step(uint32_t h, byte c) {
	h = h * 16 + c;
	if (h & 0xf0000000) {
		h = (h ^ ((h >> 24) & 0xf0)) & 0x0fffffff;
	}
	return h;
}
/* the engine's malloc with the same statistics */
void *Mem_Alloc(uint32_t size);
void *Mem_Calloc(uint32_t size);
void *Mem_Free(void *p);	/* NULL */
void *Mem_Realloc(void *p, uint32_t size);

/* Global/StrUtils.cpp: copy with CHECK(dst && src && bytes_dst>0), always terminated */
char *Str_Copy(char *dst, const char *src, int size);
/* UTF-8 to the font encoding, in a static buffer of 1 KB (src/game/Global/StrUtils.c) */
char *Str_FromUTF8(const char *s);
/* directory, base name and extension of a path, NULL for a part not wanted */
void Path_Split(const char *path, char *name, char *dir, char *ext);
/* snprintf that logs "String size error" when it cuts */
int Str_Printf(char *dst, int size, const char *fmt, ...);
/* the same without the log line */
int Str_PrintfQuiet(char *dst, int size, const char *fmt, ...);
int Str_VPrintf(char *dst, int size, const char *fmt, va_list ap);

/* Global/HashTable.h: chained hash of string keys, case-insensitive unless caseSensitive */
typedef struct HashNode {
	void *value;			/* 0x0 */
	const char *key;		/* 0x4 */
	struct HashNode *next;		/* 0x8 */
} HashNode;

typedef struct HashTable {
	uint32_t numSlots;		/* 0x0 */
	HashNode **slots;		/* 0x4 */
	byte locked;			/* 0x8 no changes while set */
	byte caseSensitive;		/* 0x9 */
	byte pad_a[2];
} HashTable;
SD_OFFSET(HashTable, caseSensitive, 0x9);

/* the instance functions with a pointer value (src/game/Global/HashTable.c, include/scrap/hashtable.h) */
uint32_t __thiscall HashTable_Slot(HashTable *self, const char *key);
struct cPtrItem *__thiscall PtrTable_Find(HashTable *self, const char *key, uint32_t slot);
#define HashTable_Hash(t, key) HashTable_Slot((t), (key))
#define HashTable_Find(t, key, slot) ((HashNode *)PtrTable_Find((t), (key), (slot)))

#endif
