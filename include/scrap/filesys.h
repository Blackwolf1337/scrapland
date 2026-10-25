/*
 * Files of the engine (original source: FileSys/). cFile is the base class with a vtable, cFileMem a file in a
 * memory buffer (FileSys/FileMem.cpp).
 */
#ifndef SCRAP_FILESYS_H
#define SCRAP_FILESYS_H

#include "scrap/shared.h"

typedef struct cFile cFile;

typedef struct cFileVtbl {
	byte(__thiscall *Open)(cFile *self, const char *name, uint32_t mode);	/* 0x00 */
	void *vf01;
	byte(__thiscall *Close)(cFile *self);				/* 0x08 */
	int(__thiscall *Read)(cFile *self, void *dst, int size);		/* 0x0c */
	int(__thiscall *Write)(cFile *self, const void *src, int size);	/* 0x10 */
	int(__thiscall *Seek)(cFile *self, int pos);			/* 0x14 */
	int(__thiscall *Tell)(cFile *self);				/* 0x18 */
	int(__thiscall *Size)(cFile *self);				/* 0x1c */
	cFile *(__thiscall *Destroy)(cFile *self, int flags);		/* 0x20 deleting destructor, flags 1 frees */
} cFileVtbl;
SD_OFFSET(cFileVtbl, Destroy, 0x20);

struct cFile {
	const cFileVtbl *vftable;	/* 0x00 */
	byte pad_04[0x4];
	uint32_t mode;			/* 0x08 */
};

typedef struct cFileMem {
	cFile base;
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

/* constructor on memory of operator_new (rva 0x12e050) */
cFileMem *__thiscall cFileMem_ctor(void *mem, void *buffer, int maxSize, byte ownsBuffer);
/* writes the length (int) and the characters without the terminator (rva 0x12d250) */
int __thiscall cFile_WriteString(cFile *self, const char *s);

#endif
