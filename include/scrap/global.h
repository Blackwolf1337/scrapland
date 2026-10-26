/*
 * Engine basics of the original Global/ folder: the CHECK macro and the string hash table.
 */
#ifndef SCRAP_GLOBAL_H
#define SCRAP_GLOBAL_H

#include "scrap/shared.h"

/* "FATAL ERROR: Check failed!" report of the engine (rva 0xbcdb0), logs expression, file, date and line */
void CheckFailed(const char *expr, const char *file, const char *date, int line);

/* the original macro passes __FILE__ and __TIMESTAMP__ of the .cpp file, ours those of the .c file */
#define CHECK(expr) \
	do { \
		if (!(expr)) { \
			CheckFailed(#expr, __FILE__, __TIMESTAMP__, __LINE__); \
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

#define g_MemStats (&GAME_DATA(cMemStats, 0x44b628))
#define g_MemStatsContext GAME_DATA(cMemStats *, 0x44b640)

/* the engine's operator new and sized delete (rva 0x1334d0, 0x313634; the plain delete is 0x1334e0), with its memory statistics */
void *operator_new(uint32_t size);
void operator_delete_sized(void *p, uint32_t size);
void *operator_new_array(uint32_t size);
void operator_delete(void *p);
void operator_delete_array(void *p);
/* the engine's malloc with the same statistics (rva 0x1335a0) */
void *Mem_Alloc(uint32_t size);
void Mem_Free(void *p);	/* rva 0x133550 */
void *Mem_Realloc(void *p, uint32_t size);	/* rva 0x133690 */
/* __report_rangecheckfailure of the exe's CRT (rva 0x313d15), ends the process */
[[noreturn]] void crt_report_rangecheckfailure(void);

/* Global/StrUtils.cpp: copy with CHECK(dst && src && bytes_dst>0), always terminated (rva 0x1365c0) */
char *Str_Copy(char *dst, const char *src, int size);
/* UTF-8 to the font encoding, in a static buffer of 1 KB (src/game/Global/StrUtils.c) */
char *Str_FromUTF8(const char *s);
/* directory, base name and extension of a path, NULL for a part not wanted */
void Path_Split(const char *path, char *name, char *dir, char *ext);
/* snprintf that logs "String size error" when it cuts (rva 0x136610) */
int Str_Printf(char *dst, int size, const char *fmt, ...);
/* the same without the log line (rva 0x1365a0) */
int Str_PrintfQuiet(char *dst, int size, const char *fmt, ...);

/* engine3d Util/Cadenas.cpp: Str_Copy with CHECK(dest && orig && dest_nby>0) (rva 0x22cf50) */
char *Cad_Copy(char *dst, const char *src, int size);
/* engine3d Util/Comodin.cpp: wildcard match with '*', '?' and "[...]", case-sensitive (rva 0x22b640) */
int Wild_Match(const char *pattern, const char *name);

/* fprintf of the exe's CRT, for FILE pointers the exe hands out (rva 0xbd230) */
int crt_fprintf(void *file, const char *fmt, ...);

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
