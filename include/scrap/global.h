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

/* the engine's operator new and sized delete (rva 0x1334d0, 0x313634; the plain delete is 0x1334e0), with its memory statistics */
void *operator_new(uint32_t size);
void operator_delete_sized(void *p, uint32_t size);

/* Global/StrUtils.cpp: copy with CHECK(dst && src && bytes_dst>0), always terminated (rva 0x1365c0) */
char *Str_Copy(char *dst, const char *src, int size);
/* snprintf that logs "String size error" when it cuts (rva 0x136610) */
int Str_Printf(char *dst, int size, const char *fmt, ...);

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
	byte pad_8;
	byte caseSensitive;		/* 0x9 */
	byte pad_a[2];
} HashTable;
SD_OFFSET(HashTable, caseSensitive, 0x9);

uint32_t __thiscall HashTable_Hash(HashTable *table, const char *key);
HashNode *__thiscall HashTable_Find(HashTable *table, const char *key, uint32_t slot);

#endif
