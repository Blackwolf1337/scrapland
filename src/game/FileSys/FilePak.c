/*
 * The .packed archives (include/scrap/filepak.h). Original source: FileSys/FilePak.cpp, with its instances of
 * Global/HashTable.h and Global/DList.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/filepak.h"
#include "scrap/hashtable.h"
#include "scrap/mempool.h"
#include "scrap/scrap.h"
#include "scrap/platform.h"
#include "scrap/actions.h"
#include "scrap/staticinit.h"

#define g_PackExclude (GVar_BuildPackExclude.value.s)	/* ';' separated patterns not to pack */
#define g_PackInclude (GVar_BuildPackInclude.value.s)	/* ';' separated patterns to pack, all if empty */
#define g_PackBuildAsk (GVar_BuildPackPrompt.value.i)	/* ask before quitting after Pack_Build */
#define CFILE2_VFTABLE (&cFile2_vftable)
#define CFILEPACK_VFTABLE (&cFilePack_vftable)
static const cAbstractFileVtbl cFilePack_vftable;
static const cAbstractFileVtbl cFile2_vftable;
static cGVar GVar_BuildPackPrompt;
static cGVar GVar_BuildPackInclude;
static cGVar GVar_BuildPackExclude;

cStringNode *cStringList_NewNode(cStringList *list);

void Loading_Step(int percent);				/* -1: one step of the loading bar */
void Console_Execute(const char *cmd);			/* a line of the engine console */

enum { CRT_SEEK_SET = 0 };

/* lower case with '/' as separator, the form of the names in the index */
static void PackName_Normalize(char *s) {
	crt_strlwr(s);
	for (; *s; s++) {
		if (*s == '\\') {
			*s = '/';
		}
	}
}

/* cFile2: a pack file on disk with its position */

static cFile2 *cFile2_ctor(cFile2 *self) {
	self->file.base.name = NULL;
	self->file.base.mode = 0;
	self->file.f = NULL;
	self->file.base.vftable = CFILE2_VFTABLE;
	self->curPos = 0;
	return self;
}

static void cFile2_dtor(cFile2 *self) {
	self->file.base.vftable = CFILE2_VFTABLE;
	cFile_dtor(&self->file);
}

static cAbstractFile *cFile2_Destroy(cAbstractFile *elem, int flags) {
	cFile2 *self = (cFile2 *)elem;
	self->file.base.vftable = CFILE2_VFTABLE;
	cFile_dtor(&self->file);
	if (flags & 1) {
		operator_delete(self);
	}
	return elem;
}

static int cFile2_Open(cAbstractFile *base, const char *name, uint32_t mode) {
	cFile2 *self = (cFile2 *)base;
	self->curPos = 0;
	return cFile_Open(&self->file.base, name, mode);
}

static int cFile2_Read(cAbstractFile *base, void *dst, int size) {
	cFile2 *self = (cFile2 *)base;
	int n = cFile_Read(&self->file.base, dst, size);
	self->curPos += n;
	return n;
}

static int cFile2_Write(cAbstractFile *base, const void *src, int size) {
	cFile2 *self = (cFile2 *)base;
	int n = cFile_Write(&self->file.base, src, size);
	self->curPos += n;
	return n;
}

static int cFile2_Seek(cAbstractFile *base, int pos) {
	cFile2 *self = (cFile2 *)base;
	self->curPos = cFile_Seek(&self->file.base, pos);
	CHECK(self->curPos != -1);
	return self->curPos;
}

static int cFile2_Tell(cAbstractFile *base) {
	cFile2 *self = (cFile2 *)base;
	CHECK(self->curPos == cFile_Tell(&self->file.base));
	return self->curPos;
}

/* reads up to pos when it is close (32 bytes), else seeks */
static void cFile2_SkipTo(cFile2 *self, int pos, const char *name) {
	byte skip[0x20];
	uint32_t d = (uint32_t)(pos - self->curPos);

	if (d > sizeof(skip)) {
		Con_Verbosef("PackFile seek: %s: %d\n", name, d);
		VFile_Seek(self, pos);
		return;
	}
	if (d) {
		VFile_Read(self, skip, d);
	}
}

static cPackLocation g_PackFound;

static HashTable g_PackIndex = { .numSlots = 457 };

/* the instance of cNameItem (value int), the HashTable.h parts in src/game/Global/HashTable.c */

HASHTABLE_STR(NameTable, cNameItem)

/* the instance of cPackIndexItem (value cPackLocation), (&g_PackIndex) */
HASHTABLE_STR(PackIndexTable, cPackIndexItem)

/* the location of a file in the packs (in g_PackFound, valid until the next call), NULL if not there */
static cPackLocation *PackIndex_Find(const char *name) {
	char key[0x104];

	Str_Copy(key, name, sizeof(key));
	PackName_Normalize(key);
	cPackIndexItem *it = PackIndexTable_Find((&g_PackIndex), key, HashTable_Slot((&g_PackIndex), key));
	if (!it) {
		return NULL;
	}
	g_PackFound.offset = it->value.offset;
	g_PackFound.size = it->value.size;
	g_PackFound.pack = it->value.pack;
	return &g_PackFound;
}

byte Pack_FileExists(const char *name) {
	return PackIndex_Find(name) != NULL;
}

/* Global/DList.h instances: new nodes go in front of head, the callers step head on to append */

static void PackEntries_Add(cPackEntryList *list, const cPackEntry *e) {
	cPackEntryNode *node = DList_NewNode(list, sizeof(*node));
	node->e = *e;
	DList_LinkFirst(list, node);
}

static void PackEntries_Clear(cPackEntryList *list) {
	DList_Clear(list);
}

static void PackNames_Add(cStringList *list, const cString *str) {
	list->iter = NULL;
	cStringNode *node = cStringList_NewNode(list);
	cString_Assign(&node->str, str);
	DList_LinkFirst(list, node);
}

static cStringList g_PackNames;

/* the cString of the pack being recorded with that name */
static char **PackNames_Find(const char *name) {
	cStringList *list = (&g_PackNames);
	list->iter = list->head;
	if (!list->head) {
		return NULL;
	}
	for (;;) {
		char **str = &list->iter->str;
		if (crt_stricmp(*str, name) == 0) {
			return str;
		}
		list->iter = list->iter->next;
		if (list->iter == list->head) {
			return NULL;
		}
	}
}

static cPackEntryList g_PackEntries;
static uint32_t g_PackNumEntries;

/* the entry of the build list with that name; the iterator of the list stays on it */
static cPackEntry *PackEntry_Find(const char *name) {
	char key[0x104];
	cPackEntryList *list = (&g_PackEntries);

	list->iter = list->head;
	cPackEntry *pf = list->head ? &list->head->e : NULL;
	Str_Copy(key, name, sizeof(key));
	PackName_Normalize(key);
	for (uint32_t i = 0; i < g_PackNumEntries; i++) {
		if (strcmp(pf->name, key) == 0) {
			return pf;
		}
		list->iter = list->iter->next;
		pf = list->iter == list->head ? NULL : &list->iter->e;
	}
	CHECK(pf == NULL);
	return pf;
}

/* the iterator of the build list on to the next entry, NULL at the end */
static cPackEntry *PackEntries_Next(cPackEntryList *list) {
	list->iter = list->iter->next;
	return list->iter == list->head ? NULL : &list->iter->e;
}

/* the build list in front of the walk: appends behind the last entry */
static void PackEntries_Append(const cPackEntry *e) {
	PackEntries_Add((&g_PackEntries), e);
	if (g_PackEntries.head) {
		g_PackEntries.head = g_PackEntries.head->next;
	}
	g_PackNumEntries++;
}

/* the exclusion and inclusion lists of the build */
static byte Pack_IsIncluded(const char *name) {
	if (g_PackExclude && *g_PackExclude && StrList_Match(g_PackExclude, name, ";")) {
		Con_Debugf("Excluded  :'%s'  <file in exclusion list>\n", name);
		return 0;
	}
	if (g_PackInclude && *g_PackInclude && !StrList_Match(g_PackInclude, name, ";")) {
		Con_Debugf("Excluded  :'%s'  <file not in inclusion list>\n", name);
		return 0;
	}
	return 1;
}

/* cFilePack */

cFilePack *cFilePack_ctor(cFilePack *self) {
	self->file.base.name = NULL;
	self->file.base.mode = 0;
	self->file.f = NULL;
	self->file.base.vftable = CFILEPACK_VFTABLE;
	return self;
}

void cFilePack_dtor(cFilePack *self) {
	self->file.base.vftable = CFILEPACK_VFTABLE;
	cFile_dtor(&self->file);
}

static cAbstractFile *cFilePack_Destroy(cAbstractFile *elem, int flags) {
	cFilePack *self = (cFilePack *)elem;
	self->file.base.vftable = CFILEPACK_VFTABLE;
	cFile_dtor(&self->file);
	if (flags & 1) {
		operator_delete(self);
	}
	return elem;
}

static int g_NumPackFiles;
static cFile2 g_PackFiles[128];
static int g_PackLocked;
static void *g_LstFile;

#define PACK_MEMORY -2				/* cFilePack.pack of the memory file */
static char g_MemName[0x104];
static const uint8_t *g_MemData;
static int g_MemSize, g_MemPos;

void FilePack_SetMemory(const char *name, const void *data, int size) {
	Str_Copy(g_MemName, name && data ? name : "", sizeof(g_MemName));
	g_MemData = data;
	g_MemSize = data ? size : 0;
}

static byte Pack_IsMemory(const char *name) {
	return g_MemName[0] && !crt_stricmp(name, g_MemName);
}

/*
 * name is relative to the open pack (g_PackName). A name in the index is read from its pack, else from disk; while
 * a file list is recorded every file is read from disk and listed. Always 1.
 */
int cFilePack_Open(cAbstractFile *elem, const char *name, uint32_t mode) {
	cFilePack *self = (cFilePack *)elem;
	char path[0x104];

	CHECK(mode == FILE_MODE_READ || g_LstFile);
	CHECK(g_NumPackFiles > 0 || g_LstFile);
	CHECK(g_PackLocked == 0);
	g_PackLocked = 1;
	Loading_Step(-1);
	Str_Printf(path, sizeof(path), "%s%s", g_PackNameStr, name);
	CHECK(strstr(name, "../") == NULL);
	CHECK(strstr(name, "..\\") == NULL);
	self->start = 0;
	self->size = 0;
	self->pack = -1;
	if (Pack_IsMemory(name) && mode == FILE_MODE_READ) {
		self->start = -1;
		self->pack = PACK_MEMORY;
		self->size = g_MemSize;
		g_MemPos = 0;
		return 1;
	}
	if (!g_LstFile) {
		cPackLocation *loc = PackIndex_Find(path);
		if (loc) {
			cFile2_SkipTo(&g_PackFiles[loc->pack], loc->offset, path);
			self->start = loc->offset;
			self->pack = loc->pack;
			self->size = loc->size;
			return cAbstractFile_Open(&self->file.base, path, mode);
		}
		Con_Warnf("WARNING at cFilePack::Open : File not in pak!\n  Trying with '%s'.\n", path);
	} else {
		crt_fputs(g_PackNameStr, g_LstFile);
		crt_fputs(name, g_LstFile);
		crt_fputs("\n", g_LstFile);
	}
	return cFile_Open(&self->file.base, path, mode);
}

/* only one cFilePack is open at a time (g_PackLocked) */
int cFilePack_Close(cAbstractFile *base) {
	cFilePack *self = (cFilePack *)base;
	CHECK(g_PackLocked != 0);
	g_PackLocked = 0;
	if (self->pack == PACK_MEMORY) {
		return 1;
	}
	if (!self->start) {
		return cFile_Close(&self->file.base);
	}
	return 1;
}

int cFilePack_Exists(cAbstractFile *base, const char *name) {
	cFilePack *self = (cFilePack *)base;
	char path[0x104];

	if (Pack_IsMemory(name)) {
		return 1;
	}
	if (!g_LstFile) {
		if (PackIndex_Find(name)) {
			return 1;
		}
		Str_Printf(path, sizeof(path), "%s%s", g_PackNameStr, name);
		Con_Warnf("WARNING at cFilePack::Exist : File not in pak!\n  Trying with '%s'.\n", path);
		name = path;
	}
	return cFile_Exists(&self->file.base, name);
}

int cFilePack_Read(cAbstractFile *elem, void *dst, int size) {
	cFilePack *self = (cFilePack *)elem;
	if (self->pack == PACK_MEMORY) {
		int n = size < g_MemSize - g_MemPos ? size : g_MemSize - g_MemPos;
		memcpy(dst, g_MemData + g_MemPos, (size_t)n);
		g_MemPos += n;
		return n;
	}
	if (!self->start) {
		return cFile_Read(&self->file.base, dst, size);
	}
	int pos = VFile_Tell(self);
	CHECK(pos >= 0 && pos + size <= self->size);
	cFile2 *pf = &g_PackFiles[self->pack];
	int n = cFile_Read(&pf->file.base, dst, size);
	pf->curPos += n;
	return n;
}

int cFilePack_Write(cAbstractFile *base, const void *src, int size) {
	cFilePack *self = (cFilePack *)base;
	CHECK(self->start == 0);
	return cFile_Write(&self->file.base, src, size);
}

static int cFilePack_Seek(cAbstractFile *base, int pos) {
	cFilePack *self = (cFilePack *)base;
	if (self->pack == PACK_MEMORY) {
		g_MemPos = pos < 0 ? 0 : pos > g_MemSize ? g_MemSize : pos;
		return g_MemPos;
	}
	if (!self->start) {
		return cFile_Seek(&self->file.base, pos);
	}
	cFile2 *pf = &g_PackFiles[self->pack];
	pf->curPos = cFile_Seek(&pf->file.base, self->start + pos);
	CHECK(pf->curPos != -1);
	return pf->curPos;
}

static int cFilePack_Tell(cAbstractFile *base) {
	cFilePack *self = (cFilePack *)base;
	if (self->pack == PACK_MEMORY) {
		return g_MemPos;
	}
	if (!self->start) {
		return cFile_Tell(&self->file.base);
	}
	return g_PackFiles[self->pack].curPos - self->start;
}

int cFilePack_Size(cAbstractFile *base) {
	cFilePack *self = (cFilePack *)base;
	if (!self->start) {
		return cFile_Size(&self->file.base);
	}
	return self->size;
}

/* the packs */

int PackLog_Closed(void) {
	return g_LstFile == NULL;
}

/* adds the open pack and name to the file list being recorded; 0 if none is */
int PackLog_Add(const char *name) {
	if (!g_LstFile) {
		return 0;
	}
	crt_fputs(g_PackNameStr, g_LstFile);
	crt_fputs(name, g_LstFile);
	crt_fputs("\n", g_LstFile);
	return 1;
}

/* pack name, offset and size of a file in the packs; 0 if not there or while a list is recorded */
byte Pack_GetFileInfo(const char *name, char *packName, int *offset, int *size) {
	if (g_LstFile) {
		return 0;
	}
	cPackLocation *loc = PackIndex_Find(name);
	if (!loc) {
		return 0;
	}
	if (packName) {
		Str_Printf(packName, 0x40, "%s", g_PackFiles[loc->pack].file.base.name);
	}
	if (offset) {
		*offset = loc->offset;
	}
	if (size) {
		*size = loc->size;
	}
	return 1;
}

/* the file name of the packs (up to max bytes) into dst; its size, -1 when it is not there or longer */
int Pack_ReadFile(const char *name, void *dst, int max) {
	if (g_LstFile) {
		return -1;
	}
	cPackLocation *loc = PackIndex_Find(name);
	if (!loc || loc->size > max) {
		return -1;
	}
	cFile2 *f = &g_PackFiles[loc->pack];
	cFile2_SkipTo(f, loc->offset, name);
	return VFile_Read(f, dst, loc->size) == loc->size ? loc->size : -1;
}

/*
 * mounts name.packed and adds its files to the index (later packs win); 1 if it did. Without the pack and without other packs
 * a file list is recorded instead: dir/filelist.2Bpack (name\filelist.2Bpack without dir), appended when the pack
 * is already being recorded.
 */
byte Pack_Open(const char *name, const char *dir, size_t bufSize) {
	char path[0x104];
	char fname[0x104];

	if (g_NumPackFiles >= PACK_MAX_FILES) {
		Con_FatalErrorf("Too many packs opened.");
	}
	CHECK(g_PackFiles[g_NumPackFiles].file.base.name == NULL);
	if (g_NumPackFiles < 1) {
		g_PackEntries.iter = g_PackEntries.head;
		CHECK(g_NumPackFiles > 0 || g_PackEntries.head == NULL);
		if (g_NumPackFiles < 1) {
			for (int i = 0; i < (int)g_PackIndex.numSlots; i++) {
				if (g_PackIndex.slots[i]) {
					CHECK(g_NumPackFiles > 0 || g_PackIndex.slots[i] == NULL);
					break;
				}
			}
		}
	}
	CHECK(g_LstFile == NULL);
	CHECK(g_PackLocked == 0);
	Str_Printf(path, sizeof(path), "%s.%s", name, PACK_EXT);
	cFile2 *pf = &g_PackFiles[g_NumPackFiles];
	if (!cFile_Exists(&pf->file.base, path)) {
		if (g_NumPackFiles >= 1) {
			Con_Verbosef("Pack: %s not found\n", path);
			return 0;
		}
		g_PackNumEntries = 0;
		if (dir) {
			Str_Printf(path, sizeof(path), "%s%s", dir, PACK_LIST);
		} else {
			Str_Printf(path, sizeof(path), "%s\\%s", name, PACK_LIST);
		}
		if (!PackNames_Find(name)) {
			Con_Verbosef("Pack not found: creating  '%s'\n", path);
			g_LstFile = File_OpenRel(path, "wt");
			char *str;
			cString_ctorStr(&str, name);
			PackNames_Add((&g_PackNames), &str);
			cString_dtor(&str);
		} else {
			Con_Verbosef("Pack not found: appending '%s'\n", path);
			g_LstFile = File_OpenRel(path, "a+t");
		}
		if (!g_LstFile) {
			Con_Print(0xff, "#######################\n####", 0);
			Con_Print(0x8f008f, " W A R N I N G ", 0);
			Con_Print(0xff, "####\n#######################\n", 0);
			Con_Print(0x8f008f, "Can not create or append '", 0);
			Con_Print(0xff, path, 0);
			Con_Print(0x8f008f, "'\n", 0);
			g_LstFile = File_OpenRel(path, "rt");
			CHECK(g_LstFile);
		}
		return 0;
	}
	pf->curPos = 0;
	cFile_Open(&pf->file.base, path, FILE_MODE_READ);
	if (bufSize) {
		crt_setvbuf(pf->file.f, NULL, 0, bufSize);
	}
	int magic, version;
	uint32_t count;
	VFile_Read(pf, &magic, 4);
	VFile_Read(pf, &version, 4);
	if (magic != PACK_MAGIC) {
		Con_FatalErrorf("Wrong format in file \"%s\"", path);
	}
	if (version != PACK_VERSION) {
		Con_FatalErrorf("Wrong version in file \"%s\"", path);
	}
	VFile_Read(pf, &count, 4);
	g_PackNumEntries += count;
	Con_Printf("Pack: %s\n", path);
	for (uint32_t i = 0; i < count; i++) {
		cAbstractFile *f = &g_PackFiles[g_NumPackFiles].file.base;
		int size, offset;
		cAbstractFile_ReadString(f, fname, sizeof(fname));
		VFile_Read(f, &size, 4);
		VFile_Read(f, &offset, 4);
		cPackLocation loc = { offset, size, (byte)g_NumPackFiles, { 0 } };
		/* the index takes the name as it is in the pack; a later pack replaces the location */
		CHECK(!g_PackIndex.locked);
		uint32_t slot = HashTable_Slot((&g_PackIndex), fname);
		if (!PackIndexTable_Find((&g_PackIndex), fname, slot)) {
			PackIndexTable_Insert((&g_PackIndex), fname, slot, &loc);
		} else {
			PackIndexTable_Set((&g_PackIndex), fname, &loc);
		}
	}
	g_NumPackFiles++;
	return 1;
}

/* closes every pack, or the file list being recorded */
void Pack_CloseAll(void) {
	CHECK(g_PackLocked == 0);
	if (g_NumPackFiles) {
		PackEntries_Clear((&g_PackEntries));
		PackIndexTable_Clear((&g_PackIndex));
		for (int i = 0; i < g_NumPackFiles; i++) {
			cFile_Close(&g_PackFiles[i].file.base);
		}
		g_NumPackFiles = 0;
		return;
	}
	CHECK(g_LstFile);
	crt_fclose(g_LstFile);
	g_LstFile = NULL;
}

/* a .packed file found by Pack_OpenDir */
static void Pack_OpenFound(const char *file) {
	char name[0x104];

	Str_Printf(name, sizeof(name), "%s", file);
	char *ext = strrchr(name, '.');
	CHECK(ext && !crt_stricmp(ext + 1, PACK_EXT));
	*ext = 0;
	Pack_Open(name, NULL, 0x4000);
}

/* mounts every .packed below dir */
void Pack_OpenDir(const char *dir) {
	char path[0x104];

	int len = (int)strlen(dir);
	if (len - 1 < 0) {
		return;
	}
	byte sep = dir[len - 1] == '\\' || dir[len - 1] == '/';
	Str_Printf(path, sizeof(path), "%s%s", dir, sep ? "" : "/");
	Dir_Walk(path, PACK_EXT, Pack_OpenFound);
}

/*
 * ExtractPack(dir): extracts dir + "Data".packed into dir; the packs mounted before are closed, and the pack too at
 * the end. The names come from the build list (from its last entry backwards), not from the index; Pack_Open does
 * not fill that list, so this only works after a build in the same run.
 */
int Pack_Extract(const char *dir) {
	char pack[0x104];
	char out[0x104];
	int errors = 0;
	cPackEntryList *list = (&g_PackEntries);

	Str_Printf(pack, sizeof(pack), "%s%s", dir, "Data");
	Pack_CloseAll();
	Pack_Open(pack, NULL, 0x400000);
	if (g_LstFile) {
		/* the "%'" of the original text goes to the exe's printf as it is */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
		Con_FatalErrorf("ERROR ExtractPack('%s%'): Pack does not exist", pack);
#pragma GCC diagnostic pop
	}
	Path_MakeDirs(dir);
	cPackEntry *name = NULL;
	if (list->head) {
		list->iter = list->head->prev;
		if (list->iter) {
			name = &list->iter->e;
		}
	}
	for (uint32_t i = 0; i < g_PackNumEntries; i++) {
		cFilePack f;

		Str_Printf(out, sizeof(out), "%s%s", dir, name->name);
		Path_MakeDirs(out);
		Con_Printf("%s.%s:(%s) -> %s\n", pack, PACK_EXT, name->name, out);
		f.file.base.name = NULL;
		f.file.base.mode = 0;
		f.file.f = NULL;
		f.file.base.vftable = CFILEPACK_VFTABLE;
		cFilePack_Open(&f.file.base, name->name, FILE_MODE_READ);
		int size = f.start ? f.size : cFile_Size(&f.file.base);
		void *buf = Mem_Alloc(size);
		if (!f.start) {
			cFile_Read(&f.file.base, buf, size);
		} else {
			int pos = VFile_Tell(&f);
			CHECK(pos >= 0 && pos + size <= f.size);
			cFile2 *pf = &g_PackFiles[f.pack];
			pf->curPos += cFile_Read(&pf->file.base, buf, size);
		}
		void *fo = File_OpenRel(out, "wb");
		if (fo) {
			int n = f.start ? f.size : cFile_Size(&f.file.base);
			CHECK((int)crt_fwrite(buf, 1, n, fo) == (f.start ? f.size : cFile_Size(&f.file.base)));
			CHECK(crt_fclose(fo) == 0);
		} else {
			Con_Bluef("ERROR: File could not be created: '%s'\n", out);
			errors++;
		}
		Mem_Free(buf);
		CHECK(g_PackLocked != 0);
		g_PackLocked = 0;
		if (!f.start) {
			cFile_Close(&f.file.base);
		}
		list->iter = list->iter->prev;
		name = list->iter == list->head->prev ? NULL : &list->iter->e;
		f.file.base.vftable = CFILEPACK_VFTABLE;
		cFile_dtor(&f.file);
	}
	CHECK(name == NULL);
	Pack_CloseAll();
	Con_FatalErrorf("INFO: PackFile extraction finished\nTotal: %d\nErrors: %d", g_PackNumEntries, errors);
	return 1;
}

/* building a pack: the files of filelist.2Bpack, then by mode files found below the directory */

static int g_PackDirLen;
static int g_PackBuildMode;

/*
 * a file of the walk for Pack_Build. Mode 2 adds everything (2D/ also as .dds), 3 Python files, 2D/Loading/,
 * Sounds/Stream/ and Music/, 4 Sounds/Voices/; other modes everything but .dds. Saves, models, ini, scc, html and
 * packs are never added.
 */
static void Pack_BuildAddFile(const char *path) {
	/* a whole cPackEntry: PackEntries_Add copies offset and size too, uninitialized like in the original */
	char name[sizeof(cPackEntry)];
	int special = 0;

	Str_Copy(name, path + g_PackDirLen, sizeof(name));
	PackName_Normalize(name);
	char *ext = strrchr(name, '.');
	if (g_PackBuildMode == 2) {
		if (!crt_strnicmp(name, "2D/", 3)) {
			special = 1;
		}
	} else if (g_PackBuildMode == 3) {
		special = ext && !crt_stricmp(ext, ".PY");
		if (!crt_strnicmp(name, "2D/Loading/", 11)) {
			special = 1;
		}
		if (!crt_strnicmp(name, "Sounds/Stream/", 14)) {
			special = 1;
		}
		if (!crt_strnicmp(name, "Music/", 6)) {
			special = 1;
		} else if (!special) {
			return;
		}
	} else if (g_PackBuildMode == 4) {
		if (crt_strnicmp(name, "Sounds/Voices/", 14)) {
			return;
		}
		special = 1;
	}
	if (ext) {
		if (!special && !crt_stricmp(ext, ".DDS")) {
			Con_Verbosef("Skipping2 :'%s'\n", name);
			return;
		}
		static const char *const skip[] = { ".SAV", ".M3D", ".INI", ".SCC", ".HTML", ".2BPACK", ".PACKED" };
		for (size_t i = 0; i < sizeof(skip) / sizeof(skip[0]); i++) {
			if (!crt_stricmp(ext, skip[i])) {
				Con_Verbosef("Skipping  :'%s'\n", name);
				return;
			}
		}
	}
	if (!PackEntry_Find(name) && Pack_IsIncluded(name)) {
		Con_Warnf("Added     :'%s'\n", name);
		PackEntries_Append((const cPackEntry *)name);
	}
}

/* names, sizes and offsets of the build list; the entry after the last (NULL if the count is right) */
static cPackEntry *Pack_WriteEntries(cAbstractFile *out) {
	uint32_t v;
	cPackEntryList *list = (&g_PackEntries);

	list->iter = list->head;
	cPackEntry *pf = list->head ? &list->head->e : NULL;
	for (uint32_t i = 0; i < g_PackNumEntries; i++) {
		cAbstractFile_WriteString(out, pf->name);
		v = pf->size;
		VFile_Write(out, &v, 4);
		v = pf->offset;
		VFile_Write(out, &v, 4);
		pf = PackEntries_Next(list);
	}
	return pf;
}

/*
 * makes outName.packed (dir.packed without outName) from dir/filelist.2Bpack and, with mode 2 or more, the files
 * of Pack_BuildAddFile. Ends the game when done, after asking if g_PackBuildAsk.
 */
void Pack_Build(const char *dir, const char *outName, int mode) {
	char line[sizeof(cPackEntry)];
	char msg[0x400];
	char base[0x104];
	char path[0x104];
	int onList = 0, added = 0, notFound = 0;
	cPackEntryList *list = (&g_PackEntries);
	cFile2 *out = &g_PackFiles[0];
	uint32_t v;

	CHECK(out->file.base.name == NULL);
	list->iter = list->head;
	CHECK(list->head == NULL);
	CHECK(g_LstFile == NULL);
	if (!outName) {
		outName = dir;
	}
	Str_Printf(path, sizeof(path), "%s/%s", dir, PACK_LIST);
	g_LstFile = crt_fopen(path, "rt");
	CHECK(g_LstFile);
	g_PackNumEntries = 0;
	Con_Printf("Reading '%s'\n", path);
	while (!crt_feof(g_LstFile)) {
		if (!crt_fgets(line, 0x104, g_LstFile)) {
			continue;
		}
		Str_Trim(line);
		PackName_Normalize(line);
		if (!line[0] || PackEntry_Find(line)) {
			continue;
		}
		Str_Printf(path, sizeof(path), "%s/%s", dir, line);
		if (crt_access(path, 0) == 0) {
			if (Pack_IsIncluded(line)) {
				onList++;
				Con_Verbosef("On list   :'%s'\n", line);
				PackEntries_Append((const cPackEntry *)line);
			}
		} else {
			notFound++;
			Con_Warnf("Not found :'%s'\n", line);
		}
	}
	crt_fclose(g_LstFile);
	uint32_t listed = g_PackNumEntries;
	g_LstFile = NULL;
	if (mode >= 2) {
		Str_Printf(base, sizeof(base), "%s/", dir);
		g_PackBuildMode = mode;
		g_PackDirLen = (int)strlen(base);
		Dir_Walk(base, "", Pack_BuildAddFile);
		added = g_PackNumEntries - listed;
	}
	Con_Verbosef("total files: %d\non list: %d  added: %d  not found: %d\n", g_PackNumEntries, onList, added,
		notFound);
	Str_Printf(path, sizeof(path), "%s.%s", outName, PACK_EXT);
	out->curPos = 0;
	cFile_Open(&out->file.base, path, FILE_MODE_WRITE | FILE_MODE_CREATE);
	cFile in = { { CFILE_VFTABLE, NULL, 0 }, NULL };
	Con_Printf("Saving '%s'", path);
	v = PACK_MAGIC;
	VFile_Write(out, &v, 4);
	v = PACK_VERSION;
	VFile_Write(out, &v, 4);
	v = g_PackNumEntries;
	VFile_Write(out, &v, 4);
	/* the entries twice: first for the room, after the data with the offsets */
	if (Pack_WriteEntries(&out->file.base)) {
		CHECK(0);
	}
	list->iter = list->head;
	cPackEntry *pf = list->head ? &list->head->e : NULL;
	Con_Print(0, "\n", 0);
	uint32_t pct = 100;
	for (uint32_t i = 0; i < g_PackNumEntries; i++) {
		Str_Printf(path, sizeof(path), "%s/%s", dir, pf->name);
		cFile_Open(&in.base, path, FILE_MODE_READ);
		pf->size = cFile_Size(&in.base);
		CHECK(out->curPos == cFile_Tell(&out->file.base));
		pf->offset = out->curPos;
		void *buf = Mem_Alloc(pf->size);
		cFile_Read(&in.base, buf, pf->size);
		out->curPos += cFile_Write(&out->file.base, buf, pf->size);
		Mem_Free(buf);
		cFile_Close(&in.base);
		pf = PackEntries_Next(list);
		if (g_PackNumEntries > 20 && (i + 1) % (g_PackNumEntries / 10) == g_PackNumEntries % 10) {
			Con_Verbosef("%d%%\n", pct / g_PackNumEntries);
		}
		pct += 100;
	}
	out->curPos = cFile_Seek(&out->file.base, 8);
	CHECK(out->curPos != -1);
	v = g_PackNumEntries;
	VFile_Write(out, &v, 4);
	Pack_WriteEntries(&out->file.base);
	Con_Print(0, " Ok!\n", 0);
	PackEntries_Clear(list);
	cFile_Close(&out->file.base);
	g_LstFile = NULL;
	Str_Printf(msg, sizeof(msg), "BuildPackFile finished\n\npath: %s.%s\n\ntotal files: %d\n\non list: %d  added: %d  "
		"not found: %d\n\nExit now?", outName, PACK_EXT, g_PackNumEntries, onList, added, notFound);
	if (!g_PackBuildAsk) {
		crt_exit(-1);
	}
	/* MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL through the exe's import of MessageBoxA */
	if (Plat_MessageBox(msg, "Message", 0x2024) == 6) {
		crt_exit(-1);
	}
	cFile_dtor(&in);
}

void Python_CompileAll(void) {
	Con_Errorf("PythonCompileAll: no Python interpreter in this build\n");
}

/* the vtable of cFile */
const cAbstractFileVtbl cFile_vftable = {
	.Open = cFile_Open,
	.Exists = cFile_Exists,
	.Close = cFile_Close,
	.Read = cFile_Read,
	.Write = cFile_Write,
	.Seek = cFile_Seek,
	.Tell = cFile_Tell,
	.Size = cFile_Size,
	.Destroy = cFile_Destroy,
};

/* the vtable of cFile2 */
static const cAbstractFileVtbl cFile2_vftable = {
	.Open = cFile2_Open,
	.Exists = cFile_Exists,
	.Close = cFile_Close,
	.Read = cFile2_Read,
	.Write = cFile2_Write,
	.Seek = cFile2_Seek,
	.Tell = cFile2_Tell,
	.Size = cFile_Size,
	.Destroy = cFile2_Destroy,
};

/* the vtable of cFilePack */
static const cAbstractFileVtbl cFilePack_vftable = {
	.Open = cFilePack_Open,
	.Exists = cFilePack_Exists,
	.Close = cFilePack_Close,
	.Read = cFilePack_Read,
	.Write = cFilePack_Write,
	.Seek = cFilePack_Seek,
	.Tell = cFilePack_Tell,
	.Size = cFilePack_Size,
	.Destroy = cFilePack_Destroy,
};

cGVar GVar_PathInPackFile;
static cGVar GVar_BuildPackPrompt;
static cGVar GVar_BuildPackExclude;
static cGVar GVar_BuildPackInclude;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_PathInPackFile, &GVar_PathInPackFile)
INIT_GVAR_STRING(Init_GVar_PathInPackFile, Fini_GVar_PathInPackFile, &GVar_PathInPackFile, "PathInPackFile", 1, "Used to convert the system to the new pack system...", "")
FINI_GVAR_STRING(Fini_GVar_BuildPackInclude, &GVar_BuildPackInclude)
INIT_GVAR_STRING(Init_GVar_BuildPackInclude, Fini_GVar_BuildPackInclude, &GVar_BuildPackInclude, "BuildPackInclude", 4, "Pack build filelist inclusion pattern", "")
FINI_GVAR_STRING(Fini_GVar_BuildPackExclude, &GVar_BuildPackExclude)
INIT_GVAR_STRING(Init_GVar_BuildPackExclude, Fini_GVar_BuildPackExclude, &GVar_BuildPackExclude, "BuildPackExclude", 4, "Pack build filelist exclusion pattern", "")
INIT_GVAR_INT(Init_GVar_BuildPackPrompt, &GVar_BuildPackPrompt, "BuildPackPrompt", 4, "disgusting final message", 1)
FINI_VECTOR(Fini_cFile2_PackFiles, g_PackFiles, sizeof(cFile2), 128, cFile2_dtor)
INIT_VECTOR(Init_cFile2_PackFiles, Fini_cFile2_PackFiles, g_PackFiles, sizeof(cFile2), 128, cFile2_ctor, cFile2_dtor)
FINI_DLIST(Fini_PackEntries, &g_PackEntries, PackEntries_Clear, 276)
INIT_ATEXIT(Init_PackEntries, Fini_PackEntries)
FINI_CALL2(Fini_cStringList_PackNames, &g_PackNames, cStringList_Clear, &g_PackNames, cStringList_FreePool)
INIT_ATEXIT(Init_cStringList_PackNames, Fini_cStringList_PackNames)
FINI_CALL(Fini_PackIndex, &g_PackIndex, PackIndexTable_Clear)
INIT_HASHTABLE(Init_PackIndex, Fini_PackIndex, &g_PackIndex, 0x1c9, 4, 0x724, 0, 1)
