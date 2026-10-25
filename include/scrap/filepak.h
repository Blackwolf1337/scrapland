/*
 * FileSys/FilePak.cpp: the .packed archives. A pack is "BFPK", version 0, the number of files, then per file its
 * name (int length and characters), size and offset, then the data. Up to 128 packs are open at once; one hash
 * index maps every file name (lower case, '/') to pack, offset and size. cFilePack reads a file from the packs, or
 * from disk when the name is not in them. While a file list (filelist.2Bpack) is recorded every file goes to disk
 * and its name into the list; Pack_Build makes a pack from such a list.
 */
#ifndef SCRAP_FILEPAK_H
#define SCRAP_FILEPAK_H

#include "scrap/filesys.h"
#include "scrap/pymempool.h"

#define PACK_MAGIC 0x4b504642		/* "BFPK" */
#define PACK_VERSION 0
#define PACK_MAX_FILES 128
#define PACK_EXT "packed"
#define PACK_LIST "filelist.2Bpack"

/* a pack on disk; curPos follows the FILE position so reads need no seek */
typedef struct cFile2 {
	cFile file;
	int curPos;			/* 0x10 */
} cFile2;
SD_SIZE(cFile2, 0x14);

typedef struct cFilePack {
	cFile file;			/* used directly for a file on disk */
	int start;			/* 0x10 offset in the pack, 0: a file on disk */
	int size;			/* 0x14 */
	int pack;			/* 0x18 index in g_PackFiles, -1 */
} cFilePack;
SD_SIZE(cFilePack, 0x1c);

/* a value of the index */
typedef struct cPackLocation {
	int offset;			/* 0x0 */
	int size;			/* 0x4 */
	byte pack;			/* 0x8 */
	byte pad_9[3];
} cPackLocation;

/* Global/HashTable.h with a cString key, items from the "HashItem" pool; two instances in this file */
typedef struct cPackIndexItem {
	cPackLocation loc;		/* 0x00 */
	char *key;			/* 0x0c cString */
	struct cPackIndexItem *next;	/* 0x10 */
} cPackIndexItem;
SD_SIZE(cPackIndexItem, 0x14);

typedef struct cNameItem {
	int value;			/* 0x0 */
	char *key;			/* 0x4 cString */
	struct cNameItem *next;		/* 0x8 */
} cNameItem;
SD_SIZE(cNameItem, 0xc);

/* an entry of the build list: name, then offset and size in the new pack */
typedef struct cPackEntry {
	char name[0x104];		/* 0x000 */
	int offset;			/* 0x104 */
	int size;			/* 0x108 */
} cPackEntry;
SD_SIZE(cPackEntry, 0x10c);

/* Global/DList.h: a ring with its first node, an iterator and a free list of nodes */
#define DLIST(node) \
	struct { \
		node *head; \
		node *iter; \
		int count; \
		node *freeNodes; \
		int numFree; \
		int maxFree; \
	}

typedef struct cPackEntryNode {
	struct cPackEntryNode *next;	/* 0x0 */
	struct cPackEntryNode *prev;	/* 0x4 */
	cPackEntry e;			/* 0x8 */
} cPackEntryNode;
SD_SIZE(cPackEntryNode, 0x114);

typedef struct cStringNode {
	struct cStringNode *next;	/* 0x0 */
	struct cStringNode *prev;	/* 0x4 */
	char *str;			/* 0x8 cString */
} cStringNode;
SD_SIZE(cStringNode, 0xc);

typedef DLIST(cPackEntryNode) cPackEntryList;
typedef DLIST(cStringNode) cStringList;

#define g_PackIndex (&GAME_DATA(HashTable, 0x428994))	/* of cPackIndexItem, 457 slots */
#define g_PackEntries (&GAME_DATA(cPackEntryList, 0x44ab8c))	/* build list */
#define g_PackNames (&GAME_DATA(cStringList, 0x44ab44))	/* packs being recorded */
#define g_NumPackFiles GAME_DATA(int, 0x44aba4)
#define g_PackFiles (&GAME_DATA(cFile2, 0x44aba8))		/* PACK_MAX_FILES */
#define g_NameTable GAME_DATA(HashTable *, 0x44ab40)	/* of cNameItem, while Python_CompileAll collects */
#define g_PackExclude GAME_DATA(char *, 0x44b5b8)	/* ';' separated patterns not to pack */
#define g_PackInclude GAME_DATA(char *, 0x44b5d0)	/* ';' separated patterns to pack, all if empty */
#define g_PackLocked GAME_DATA(int, 0x44b5d8)		/* a cFilePack is open (only one at a time) */
#define g_LstFile GAME_DATA(void *, 0x44b5dc)		/* the file list being recorded */
#define g_PackNumEntries GAME_DATA(uint32_t, 0x44b5e0)
#define g_PackDirLen GAME_DATA(int, 0x44b5e4)		/* characters to skip of the walked names */
#define g_PackBuildMode GAME_DATA(int, 0x44b5e8)
#define g_PackFound GAME_DATA(cPackLocation, 0x44b5ec)	/* result of PackIndex_Find */
#define g_HashItemPool GAME_DATA(cMemPool *, 0x44b600)
#define g_HashItemRefs GAME_DATA(int, 0x44b604)
#define g_HashItemValues (&GAME_DATA(uint32_t, 0x4289a0))
#define g_PackNameStr GAME_DATA(char *, 0x44ab6c)	/* chars of g_PackName, the prefix of the open pack */
#define g_PackBuildAsk GAME_DATA(int, 0x44ab84)	/* ask before quitting after Pack_Build */

cFilePack *__thiscall cFilePack_ctor(cFilePack *self);
void __thiscall cFilePack_dtor(cFilePack *self);
int __thiscall cFilePack_Open(cFilePack *self, const char *name, uint32_t mode);
int __thiscall cFilePack_Exists(cFilePack *self, const char *name);
int __thiscall cFilePack_Close(cFilePack *self);
int __thiscall cFilePack_Read(cFilePack *self, void *dst, int size);
int __thiscall cFilePack_Size(cFilePack *self);

/* 1 unless the file list is being recorded */
int PackLog_Closed(void);
int PackLog_Add(const char *name);
/* mounts name.packed (src/game/FileSys/FilePak.c) */
byte Pack_Open(const char *name, const char *dir, size_t bufSize);
int Pack_Extract(const char *dir);

#define cFile2_vftable ((const cAbstractFileVtbl *)Game_Ptr(0x3ba868))
#define cFilePack_vftable ((const cAbstractFileVtbl *)Game_Ptr(0x3ba890))

#endif
