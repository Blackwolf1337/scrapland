/*
 * FileSys/FilePak.cpp: the .packed archives. A pack is "BFPK", version 0, the number of files, then per file its
 * name (int length and characters), size and offset, then the data. Up to 128 packs are open at once; one hash
 * index maps every file name (lower case, '/') to pack, offset and size. cFilePack reads a file from the packs, or
 * from disk when the name is not in them. While a file list (filelist.2Bpack) is recorded every file goes to disk
 * and its name into the list; Pack_Build makes a pack from such a list.
 */
#ifndef SCRAP_FILEPAK_H
#define SCRAP_FILEPAK_H

#include "scrap/gvar.h"
#include "scrap/dlist.h"
#include "scrap/filesys.h"
#include "scrap/mempool.h"

#define PACK_MAGIC 0x4b504642		/* "BFPK" */
#define PACK_VERSION 0
#define PACK_MAX_FILES 128
#define PACK_EXT "packed"
#define PACK_LIST "filelist.2Bpack"

/* a pack on disk; curPos follows the FILE position so reads need no seek */
typedef struct cFile2 {
	cFile file;
	int curPos;
} cFile2;

typedef struct cFilePack {
	cFile file;			/* used directly for a file on disk */
	int start;			/* offset in the pack, 0: a file on disk */
	int size;
	int pack;			/* index in g_PackFiles, -1 */
} cFilePack;

/* a value of the index */
typedef struct cPackLocation {
	int offset;
	int size;
	byte pack;
	byte pad_9[3];
} cPackLocation;

/* Global/HashTable.h with a cString key, items from the "HashItem" pool; two instances in this file */
typedef struct cPackIndexItem {
	cPackLocation value;
	char *key;			/* cString */
	struct cPackIndexItem *next;
} cPackIndexItem;

typedef struct cNameItem {
	intptr_t value;			/* a number, the bits of a float or a pointer */
	char *key;			/* cString */
	struct cNameItem *next;
} cNameItem;

/* the cNameItem instance of HashTable.h (FilePak.c, also the assigned zones of Logic/Domination.c) */
cNameItem *NameTable_Find(HashTable *self, const char *key, uint32_t slot);
byte NameTable_Add(HashTable *self, const char *key, const intptr_t *v, byte warn);
cNameItem *NameTable_Insert(HashTable *self, const char *key, uint32_t slot, const intptr_t *v);
byte NameTable_Set(HashTable *self, const char *key, const intptr_t *v);
byte NameTable_Delete(HashTable *self, const char *key);
void NameTable_Clear(HashTable *self);

/* an entry of the build list: name, then offset and size in the new pack */
typedef struct cPackEntry {
	char name[0x104];
	int offset;
	int size;
} cPackEntry;

typedef struct cPackEntryNode {
	struct cPackEntryNode *next;
	struct cPackEntryNode *prev;
	cPackEntry e;
} cPackEntryNode;

typedef struct cStringNode {
	struct cStringNode *next;
	struct cStringNode *prev;
	char *str;			/* cString */
} cStringNode;

typedef DLIST(cPackEntryNode) cPackEntryList;
typedef DLIST(cStringNode) cStringList;

extern cGVar GVar_PathInPackFile;
#define g_PackNameStr (GVar_PathInPackFile.value.s)	/* chars of g_PackName, the prefix of the open pack */

void PackIndexTable_Clear(HashTable *self);
cFilePack *cFilePack_ctor(cFilePack *self);
void cFilePack_dtor(cFilePack *self);
int cFilePack_Open(cAbstractFile *elem, const char *name, uint32_t mode);
/* not in the original: one file served from memory under name (case-insensitive, read only) before the packs and
 * the disk; data stays the caller's; NULL removes it (the AI graph a multiplayer instance makes,
 * include/scrap/places.h) */
void FilePack_SetMemory(const char *name, const void *data, int size);
int cFilePack_Exists(cAbstractFile *base, const char *name);
int cFilePack_Close(cAbstractFile *base);
int cFilePack_Read(cAbstractFile *elem, void *dst, int size);
int cFilePack_Write(cAbstractFile *base, const void *src, int size);
int cFilePack_Size(cAbstractFile *base);

/* 1 unless the file list is being recorded */
int PackLog_Closed(void);
byte Pack_FileExists(const char *name);	/* in the index of the packs */
int Pack_ReadFile(const char *name, void *dst, int max);
/* the pack name (0x40), offset and size of a file in the packs; 0 if not there or while a list is recorded */
byte Pack_GetFileInfo(const char *name, char *packName, int *offset, int *size);
int PackLog_Add(const char *name);
/* mounts name.packed (src/game/FileSys/FilePak.c) */
byte Pack_Open(const char *name, const char *dir, size_t bufSize);
int Pack_Extract(const char *dir);

#endif
