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
	int(__thiscall *Open)(cAbstractFile *self, const char *name, uint32_t mode);	/* 0x00 */
	int(__thiscall *Exists)(cAbstractFile *self, const char *name);		/* 0x04 */
	int(__thiscall *Close)(cAbstractFile *self);					/* 0x08 */
	int(__thiscall *Read)(cAbstractFile *self, void *dst, int size);		/* 0x0c */
	int(__thiscall *Write)(cAbstractFile *self, const void *src, int size);	/* 0x10 */
	int(__thiscall *Seek)(cAbstractFile *self, int pos);				/* 0x14 -1 if not possible */
	int(__thiscall *Tell)(cAbstractFile *self);					/* 0x18 */
	int(__thiscall *Size)(cAbstractFile *self);					/* 0x1c */
	cAbstractFile *(__thiscall *Destroy)(cAbstractFile *self, int flags);	/* 0x20 flags 1 frees */
} cAbstractFileVtbl;
SD_OFFSET(cAbstractFileVtbl, Destroy, 0x20);

struct cAbstractFile {
	const cAbstractFileVtbl *vftable;	/* 0x00 */
	char *name;			/* 0x04 Mem_Alloc copy while open */
	uint32_t mode;			/* 0x08 FILE_MODE_ */
};
SD_SIZE(cAbstractFile, 0xc);

typedef struct cFile {
	cAbstractFile base;
	void *f;			/* 0x0c FILE of the exe's CRT */
} cFile;
SD_SIZE(cFile, 0x10);

typedef struct cFileMem {
	cAbstractFile base;
	byte *buffer;			/* 0x0c */
	byte *pos;			/* 0x10 */
	int size;			/* 0x14 */
	int maxSize;			/* 0x18 */
	byte ownsBuffer;		/* 0x1c */
	byte pad_1d[3];
} cFileMem;
SD_OFFSET(cFileMem, buffer, 0xc);
SD_OFFSET(cFileMem, ownsBuffer, 0x1c);
SD_SIZE(cFileMem, 0x20);

#define cAbstractFile_vftable ((const cAbstractFileVtbl *)Game_Ptr(0x3ba488))
#define cFile_vftable ((const cAbstractFileVtbl *)Game_Ptr(0x3ba51c))
#define cFileMem_vftable ((const cAbstractFileVtbl *)Game_Ptr(0x3ba6d8))

void __fastcall cAbstractFile_dtor(cAbstractFile *self);
int __thiscall cAbstractFile_Open(cAbstractFile *self, const char *name, uint32_t mode);
int __thiscall cAbstractFile_Close(cAbstractFile *self);
/* a string with an int length into buf of size bytes */
int __thiscall cAbstractFile_ReadString(cAbstractFile *self, char *buf, int size);
/* the length (int) and the characters without the terminator */
int __thiscall cAbstractFile_WriteString(cAbstractFile *self, const char *s);

void __fastcall cFile_dtor(cFile *self);
int __thiscall cFile_Open(cFile *self, const char *name, uint32_t mode);
int __thiscall cFile_Exists(cFile *self, const char *name);
int __thiscall cFile_Close(cFile *self);
int __thiscall cFile_Read(cFile *self, void *dst, int size);
int __thiscall cFile_Write(cFile *self, const void *src, int size);
int __thiscall cFile_Seek(cFile *self, int pos);
int __thiscall cFile_Tell(cFile *self);
int __thiscall cFile_Size(cFile *self);

cFileMem *__thiscall cFileMem_ctor(cFileMem *self, void *buffer, int maxSize, byte ownsBuffer);

/*
 * FileSys/FileBlock.cpp: a file of nested blocks, each a 4 character id, the size of its data (int) and the data.
 * Reading goes through a cFilePack when only reading, else through a cFile.
 */
typedef struct cFileBlockNode {
	uint32_t id;			/* 0x0 the characters of the id, first in the low byte */
	int pos;			/* 0x4 offset of the size field while writing */
	struct cFileBlockNode *prev;	/* 0x8 */
} cFileBlockNode;
SD_SIZE(cFileBlockNode, 0xc);

typedef struct cFileBlock {
	cAbstractFile *f;		/* 0x0 */
	cFileBlockNode *block;		/* 0x4 the open block */
} cFileBlock;
SD_SIZE(cFileBlock, 0x8);

/* Open flags: bit 0 write (with bit 1 still a cFilePack), 0x10 only if the file exists */
#define FILEBLOCK_WRITE 0x1
#define FILEBLOCK_READ 0x2
#define FILEBLOCK_MUST_EXIST 0x10

/* fopen relative to the game directory, "El path debe ser relativo" for drive paths (rva 0x21bc10) */
void *File_OpenRel(const char *name, const char *mode);
/* 0 if the file is there (rva 0x21bb60) */
int File_Access(const char *name, int mode);

/* the exe's CRT for its FILE objects */
void *crt_fopen(const char *name, const char *mode);
int crt_fclose(void *f);
size_t crt_fread(void *dst, size_t size, size_t count, void *f);
size_t crt_fwrite(const void *src, size_t size, size_t count, void *f);
int crt_fseek(void *f, long off, int origin);
long crt_ftell(void *f);

#endif
