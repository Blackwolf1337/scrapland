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
/* the same in other colors and log levels (rva 0x237be0, 0x237d80, 0x238180) */
void E3D_LogBlue(void *log, const char *fmt, ...);
void E3D_LogDebug(void *log, const char *fmt, ...);
void E3D_LogInfo(void *log, const char *fmt, ...);

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
int ArchIO_Getc(void *f);	/* 0..255 */
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

/* Engine/ArchINI.cpp (src/game/engine3d/Engine/ArchINI.c): an ini file, the getters take a default; the setters
 * return 1 if the value changed */
typedef struct cArchDatos cArchDatos;

typedef struct CArchINI {
	cArchDatos *dat;		/* 0x0 */
} CArchINI;
SD_SIZE(CArchINI, 4);

CArchINI *__thiscall CArchINI_ctorLoad(CArchINI *self, const char *name, const char *dir);
CArchINI *__thiscall CArchINI_ctor(CArchINI *self);
void __thiscall CArchINI_dtor(CArchINI *self);
void ArchINI_Read(CArchIO *f, CArchINI **ini);	/* a flag, then the block; a new CArchINI */
void ArchINI_Write(CArchIO *f, CArchINI *ini);
int __thiscall CArchINI_FindKey(CArchINI *self, const char *key, char *value, int size, int from);
int __thiscall CArchINI_FindSection(CArchINI *self, const char *name, int from);
byte __thiscall CArchINI_GetBool(CArchINI *self, const char *sect, const char *key, byte def);
float __thiscall CArchINI_GetFloat(CArchINI *self, const char *sect, const char *key, float def);
int __thiscall CArchINI_GetInt(CArchINI *self, const char *sect, const char *key, int def);
int __thiscall CArchINI_Count(CArchINI *self);
uint32_t __thiscall CArchINI_GetColor(CArchINI *self, const char *sect, const char *key, uint32_t def);	/* "R G B A" */
byte __thiscall CArchINI_GetSectionName(CArchINI *self, int idx, char *dest, int size);
char *__thiscall CArchINI_GetText(CArchINI *self, const char *sect, const char *key, char *dest, int size,
	const char *def);
float *__thiscall CArchINI_GetVec2(CArchINI *self, float *out, const char *sect, const char *key, const float *def);
vec3_t *__thiscall CArchINI_GetVec3(CArchINI *self, vec3_t *out, const char *sect, const char *key, const vec3_t *def);
void __thiscall CArchINI_ReadBlock(CArchINI *self, CArchIO *f);
void __thiscall CArchINI_Clear(CArchINI *self);
void __thiscall CArchINI_WriteBlock(CArchINI *self, CArchIO *f);
int __thiscall CArchINI_SetYesNo(CArchINI *self, const char *sect, const char *key, byte v);	/* "YES" / "NO" */
int __thiscall CArchINI_SetFloat(CArchINI *self, const char *sect, const char *key, float v, int prec);
int __thiscall CArchINI_SetInt(CArchINI *self, const char *sect, const char *key, int v);
int __thiscall CArchINI_SetStr(CArchINI *self, const char *sect, const char *key, const char *v);
int __thiscall CArchINI_Load(CArchINI *self, const char *name, const char *dir, byte verbose);
void __thiscall CArchINI_AddLine(CArchINI *self, const char *text, int idx);
void __thiscall CArchINI_DeleteLine(CArchINI *self, int idx);
int __thiscall CArchINI_SetBool(CArchINI *self, const char *sect, const char *key, int v);	/* "SI" / "NO" */
int __thiscall CArchINI_Save(CArchINI *self, const char *name, const char *dir, int unused);
int __thiscall CArchINI_ReadBool(CArchINI *self, const char *sect, const char *key, int *out);
int __thiscall CArchINI_ReadFloat(CArchINI *self, const char *sect, const char *key, float *out);
int __thiscall CArchINI_ReadInt(CArchINI *self, const char *sect, const char *key, int *out);
int __thiscall CArchINI_ReadText(CArchINI *self, const char *sect, const char *key, char *dest, int size);
const char *__thiscall CArchINI_LineText(CArchINI *self, int idx);
int __thiscall CArchINI_LineType(CArchINI *self, int idx);

#endif
