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
