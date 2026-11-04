/*
 * Files of the engine (original source: FileSys/). cAbstractFile is the base class (name and mode), cFile a file
 * on disk (FILE of the exe's CRT), cFileMem a file in a memory buffer. All share the vtable layout of
 * cAbstractFile.
 */
#ifndef SCRAP_FILESYS_H
#define SCRAP_FILESYS_H

#include "scrap/game.h"

/* mode bits of Open; the low 3 bits select the access (cFile: 1 "rb", 2 and 6 "wb", 3 "r+b", 7 "w+b") */
#define FILE_MODE_READ 0x1
#define FILE_MODE_WRITE 0x2
#define FILE_MODE_CREATE 0x4		/* cFileMem: starts empty */
#define FILE_MODE_ABSOLUTE 0x8		/* cFile: plain fopen instead of File_OpenRel */

typedef struct cAbstractFile cAbstractFile;

typedef struct cAbstractFileVtbl {
	int(*Open)(cAbstractFile *self, const char *name, uint32_t mode);
	int(*Exists)(cAbstractFile *self, const char *name);
	int(*Close)(cAbstractFile *self);
	int(*Read)(cAbstractFile *self, void *dst, int size);
	int(*Write)(cAbstractFile *self, const void *src, int size);
	int(*Seek)(cAbstractFile *self, int pos);				/* -1 if not possible */
	int(*Tell)(cAbstractFile *self);
	int(*Size)(cAbstractFile *self);
	cAbstractFile *(*Destroy)(cAbstractFile *self, int flags);	/* flags 1 frees */
} cAbstractFileVtbl;

/* calls through the slots of cAbstractFileVtbl */
static inline int VFile_Close(void *self) {
	return VT(cAbstractFileVtbl, self)->Close(self);
}

static inline cAbstractFile *VFile_Destroy(void *self, int flags) {
	return VT(cAbstractFileVtbl, self)->Destroy(self, flags);
}

static inline int VFile_Exists(void *self, const char *name) {
	return VT(cAbstractFileVtbl, self)->Exists(self, name);
}

static inline int VFile_Open(void *self, const char *name, uint32_t mode) {
	return VT(cAbstractFileVtbl, self)->Open(self, name, mode);
}

static inline int VFile_Read(void *self, void *dst, int size) {
	return VT(cAbstractFileVtbl, self)->Read(self, dst, size);
}

static inline int VFile_Seek(void *self, int pos) {
	return VT(cAbstractFileVtbl, self)->Seek(self, pos);
}

static inline int VFile_Size(void *self) {
	return VT(cAbstractFileVtbl, self)->Size(self);
}

static inline int VFile_Tell(void *self) {
	return VT(cAbstractFileVtbl, self)->Tell(self);
}

static inline int VFile_Write(void *self, const void *src, int size) {
	return VT(cAbstractFileVtbl, self)->Write(self, src, size);
}

struct cAbstractFile {
	const cAbstractFileVtbl *vftable;
	char *name;			/* Mem_Alloc copy while open */
	uint32_t mode;			/* FILE_MODE_ */
};

typedef struct cFile {
	cAbstractFile base;
	void *f;			/* FILE of the exe's CRT */
} cFile;

typedef struct cFileMem {
	cAbstractFile base;
	byte *buffer;
	byte *pos;
	int size;
	int maxSize;
	byte ownsBuffer;
	byte pad_1d[3];
} cFileMem;

extern const cAbstractFileVtbl cFile_vftable;
#define CFILE_VFTABLE (&cFile_vftable)

void cAbstractFile_dtor(cAbstractFile *self);
int cAbstractFile_Open(cAbstractFile *self, const char *name, uint32_t mode);
int cAbstractFile_Close(cAbstractFile *self);
/* a string with an int length into buf of size bytes */
int cAbstractFile_ReadString(cAbstractFile *self, char *buf, int size);
/* the length (int) and the characters without the terminator */
int cAbstractFile_WriteString(cAbstractFile *self, const char *s);

void cFile_dtor(cFile *self);
int cFile_Open(cAbstractFile *elem, const char *name, uint32_t mode);
int cFile_Exists(cAbstractFile *base, const char *name);
int cFile_Close(cAbstractFile *elem);
int cFile_Read(cAbstractFile *base, void *dst, int size);
int cFile_Write(cAbstractFile *base, const void *src, int size);
int cFile_Seek(cAbstractFile *base, int pos);
int cFile_Tell(cAbstractFile *base);
int cFile_Size(cAbstractFile *base);
cAbstractFile *cFile_Destroy(cAbstractFile *base, int flags);

cFileMem *cFileMem_ctor(cFileMem *self, void *buffer, int maxSize, byte ownsBuffer);
void cFileMem_dtor(cFileMem *self);
/* a new buffer with size bytes of data in it */
void cFileMem_SetBuffer(cFileMem *self, void *buffer, int maxSize, byte ownsBuffer, int size);
int cFileMem_Open(cAbstractFile *elem, const char *name, uint32_t mode);
int cFileMem_Close(cAbstractFile *base);
int cFileMem_Read(cAbstractFile *base, void *dst, int size);
int cFileMem_Write(cAbstractFile *base, const void *src, int size);
int cFileMem_Seek(cAbstractFile *base, int pos);
int cFileMem_Tell(cAbstractFile *base);
int cFileMem_Size(cAbstractFile *base);
/* a string with a length byte into buf of size bytes; one of less than 256 characters with its length byte */
int cAbstractFile_ReadString8(cAbstractFile *self, char *buf, byte size);
int cAbstractFile_WriteString8(cAbstractFile *self, const char *s);
int cAbstractFile_ExistsTrue(cAbstractFile *self, const char *name);

/*
 * FileSys/FileBlock.cpp: a file of nested blocks, each a 4 character id, the size of its data (int) and the data.
 * Reading goes through a cFilePack when only reading, else through a cFile.
 */
typedef struct cFileBlockNode {
	uint32_t id;			/* the characters of the id, first in the low byte */
	int pos;			/* offset of the size field while writing */
	struct cFileBlockNode *prev;
} cFileBlockNode;

typedef struct cFileBlock {
	cAbstractFile *f;
	cFileBlockNode *block;		/* the open block */
} cFileBlock;

/* Open flags: bit 0 write (with bit 1 still a cFilePack), 0x10 only if the file exists */
#define FILEBLOCK_WRITE 0x1
#define FILEBLOCK_READ 0x2
#define FILEBLOCK_MUST_EXIST 0x10

cFileBlock *cFileBlock_ctor(cFileBlock *self);
void cFileBlock_dtor(cFileBlock *self);
/* 0 if FILEBLOCK_MUST_EXIST and the file is not there */
int cFileBlock_Open(cFileBlock *self, const char *filename, uint32_t mode, uint32_t flags);
int cFileBlock_Close(cFileBlock *self);
int cFileBlock_Read(cFileBlock *self, void *mem, int size);
int cFileBlock_Write(cFileBlock *self, const void *mem, int size);
int cFileBlock_ReadInt(cFileBlock *self);
int cFileBlock_WriteInt(cFileBlock *self, int v);
int cFileBlock_OpenBlockRead(cFileBlock *self, const char *id);
int cFileBlock_CloseBlockRead(cFileBlock *self, const char *id);
int cFileBlock_OpenBlockWrite(cFileBlock *self, const char *id);
int cFileBlock_CloseBlockWrite(cFileBlock *self, const char *id);

/* Engine/ArchIO.cpp (src/game/engine3d/Engine/FileIO.c): fopen relative to the game directory, a drive in the path is
 * fatal */
void *File_OpenRel(const char *name, const char *mode);
/* 0 if the file is there */
int File_Access(const char *name, int mode);

/* mkdir relative to the game directory, logs "FILE MKDIR" */
void File_MkDirRel(const char *dir);

/* src/game/Global/FileUtils.c */
void Str_Trim(char *s);					/* drops CR/LF, leading and trailing blanks */
void Path_MakeDirs(const char *path);			/* the directories of path */
/* calls cb with every file below dir (dir ends with '/') whose name ends in "." ext, all files for "" */
void Dir_Walk(const char *dir, const char *ext, void (*cb)(const char *path));
/* chdir to the directory of path and then sub */
byte Dir_Change(const char *path, const char *sub);
/* 1 if name matches one of the sep separated patterns of list */
byte StrList_Match(const char *list, const char *name, const char *sep);

#endif
