/*
 * Engine/ArchIO.cpp of the engine3d: file access relative to the game directory with a log, files through the
 * packs (a cFilePack), and CArchIO, a binary file of nested blocks (4 character id, int size, data) as the model
 * and scene files use it. Blocks are written with a size placeholder that EndBlockWrite fills in. File_OpenRel,
 * File_Access and File_MkDirRel of this file are declared in include/scrap/filesys.h.
 */
#ifndef SCRAP_ARCHIO_H
#define SCRAP_ARCHIO_H

#include "scrap/filepak.h"

typedef struct cArchBlq {
	uint32_t id;			/* 0x0 IdToDword */
	int pos;			/* 0x4 offset of the size field */
	struct cArchBlq *prev;		/* 0x8 */
} cArchBlq;
SD_SIZE(cArchBlq, 0xc);

typedef struct CArchIO {
	byte error;			/* 0x0 every call fails once set */
	byte pad_1[3];
	int err;			/* 0x4 errno of the failure */
	cAbstractFile *f;		/* 0x8 a cFilePack */
	cArchBlq *blq;			/* 0xc the open block */
} CArchIO;
SD_SIZE(CArchIO, 0x10);

/* the log of the file accesses ("FILE OPEN: ...") */
#define g_FileLog ((void *)Game_Ptr(0x4ab914))
void E3D_Log(void *log, const char *fmt, ...);	/* rva 0x237ca0 */

CArchIO *__thiscall CArchIO_ctor(CArchIO *self, const char *name, const char *mode);
void __thiscall CArchIO_dtor(CArchIO *self);
void Ptr_Delete(void **p);
/* the whole file in a new[] buffer; a missing file is fatal */
void *ArchIO_ReadAll(const char *name, void **buf, uint32_t *size);
int __thiscall CArchIO_Finish(CArchIO *self);
void File_RelPath(char *dest, const char *path);
int __thiscall CArchIO_Close(CArchIO *self);
int __thiscall CArchIO_Write(CArchIO *self, const void *buf, int n);
/* the length with the terminator (int), then the characters; 0 for NULL */
int __thiscall CArchIO_WriteString(CArchIO *self, const char *s);
int __thiscall CArchIO_EndBlockWrite(CArchIO *self, const char *id);
int __thiscall CArchIO_WriteInt(CArchIO *self, int32_t v);	/* two copies in the exe (int, float) */
int __thiscall CArchIO_BeginBlockWrite(CArchIO *self, const char *id);
int ArchIO_Close(void *f);	/* 0 or -1 */
char ArchIO_Getc(void *f);
char *ArchIO_Gets(char *buf, int size, void *f);	/* keeps the '\n' */
int __thiscall CArchIO_Unsupported(CArchIO *self);
int ArchIO_Read(void *buf, int size, int count, void *f);	/* count */
int ArchIO_Seek(void *f, int off, int origin);	/* only SEEK_SET, 0 or -1 */
int ArchIO_Size(void *f);
int ArchIO_Tell(void *f);
int ArchIO_Write(const void *buf, int size, int count, void *f);
int File_AccessPack(const char *name, int mode);	/* the packs while they are in use */
uint32_t IdToDword(const char *id);	/* up to 4 characters, the first in the low byte */
int File_Stat(const char *name, void *st);
void *ArchIO_Open(const char *name, const char *mode);	/* a cFilePack, "rb" or "wb" */
int File_StatPack(const char *name, void *st);
byte __thiscall CArchIO_ReadBool(CArchIO *self);
int __thiscall CArchIO_ReadInt(CArchIO *self);
int __thiscall CArchIO_Read(CArchIO *self, void *buf, int n);
int __thiscall CArchIO_ReadString(CArchIO *self, char *buf, uint32_t size);
/* like CArchIO_ReadString, *isNull 1 for a NULL string */
int __thiscall CArchIO_ReadStringNull(CArchIO *self, char *buf, uint32_t size, byte *isNull);
int __thiscall CArchIO_Error(CArchIO *self, const char *what);	/* what: a label of the caller, unused */
int __thiscall CArchIO_Read4(CArchIO *self, void *out);	/* two copies in the exe */
int __thiscall CArchIO_BeginBlock(CArchIO *self, const char *id);
int __thiscall CArchIO_BeginBlockSize(CArchIO *self, const char *id, int *size);
void __thiscall CArchIO_PushBlock(CArchIO *self, const char *id, int pos);
int __thiscall CArchIO_SetError(CArchIO *self, const char *what);
void __thiscall CArchIO_PopBlock(CArchIO *self, const char *id);
/* access or stat of a name relative to the game directory, logged; pack: in the packs while they are in use */
int File_Check(const char *name, int mode, byte pack);
int File_StatX(const char *name, void *st, byte pack);

#endif
