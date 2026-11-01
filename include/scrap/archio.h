/*
 * Engine/ArchIO.cpp of the engine3d: file access relative to the game directory with a log, files through the
 * packs (a cFilePack), and CFileIO, a binary file of nested blocks (4 character id, int size, data) as the model
 * and scene files use it. Blocks are written with a size placeholder that EndBlockWrite fills in. File_OpenRel,
 * File_Access and File_MkDirRel of this file are declared in include/scrap/filesys.h.
 */
#ifndef SCRAP_ARCHIO_H
#define SCRAP_ARCHIO_H

#include "scrap/filepak.h"
#include "scrap/misc.h"

typedef struct cBlockFile {
	uint32_t id;			/* 0x0 IdToDword */
	int pos;			/* 0x4 offset of the size field */
	struct cBlockFile *prev;		/* 0x8 */
} cBlockFile;
SD_SIZE(cBlockFile, 0xc);

typedef struct CFileIO {
	byte error;			/* 0x0 every call fails once set */
	byte pad_1[3];
	int err;			/* 0x4 errno of the failure */
	cAbstractFile *f;		/* 0x8 a cFilePack */
	cBlockFile *block;			/* 0xc the open block */
} CFileIO;
SD_SIZE(CFileIO, 0x10);


CFileIO *__thiscall CFileIO_ctor(CFileIO *self, const char *name, const char *mode);
void __thiscall CFileIO_dtor(CFileIO *self);
void Ptr_Delete(void **p);
/* the whole file in a new[] buffer; a missing file is fatal */
void *FileIO_ReadAll(const char *name, void **buf, uint32_t *size);
int __thiscall CFileIO_Finish(CFileIO *self);
void File_RelPath(char *dest, const char *path);
int __thiscall CFileIO_Close(CFileIO *self);
int __thiscall CFileIO_Write(CFileIO *self, const void *buf, int n);
/* the length with the terminator (int), then the characters; 0 for NULL */
int __thiscall CFileIO_WriteString(CFileIO *self, const char *s);
int __thiscall CFileIO_EndBlockWrite(CFileIO *self, const char *id);
int __thiscall CFileIO_WriteInt(CFileIO *self, int32_t v);	/* two copies in the exe (int, float) */
int __thiscall CFileIO_BeginBlockWrite(CFileIO *self, const char *id);
int FileIO_Close(void *f);	/* 0 or -1 */
int FileIO_Getc(void *f);	/* 0..255 */
char *FileIO_Gets(char *buf, int size, void *f);	/* keeps the '\n' */
int __thiscall CFileIO_Unsupported(CFileIO *self);
int FileIO_Read(void *buf, int size, int count, void *f);	/* count */
int FileIO_Size(void *f);
int FileIO_Tell(void *f);
int File_AccessPack(const char *name, int mode);	/* the packs while they are in use */
uint32_t IdToDword(const char *id);	/* up to 4 characters, the first in the low byte */
int File_Stat(const char *name, void *st);
void *FileIO_Open(const char *name, const char *mode);	/* a cFilePack, "rb" or "wb" */
int File_StatPack(const char *name, void *st);
byte __thiscall CFileIO_ReadBool(CFileIO *self);
int __thiscall CFileIO_ReadInt(CFileIO *self);
int __thiscall CFileIO_Read(CFileIO *self, void *buf, int n);
int __thiscall CFileIO_ReadString(CFileIO *self, char *buf, uint32_t size);
/* like CFileIO_ReadString, *isNull 1 for a NULL string */
int __thiscall CFileIO_ReadStringNull(CFileIO *self, char *buf, uint32_t size, byte *isNull);
int __thiscall CFileIO_Error(CFileIO *self, const char *what);	/* what: a label of the caller, unused */
int __thiscall CFileIO_Read4(CFileIO *self, void *out);	/* two copies in the exe */
int __thiscall CFileIO_BeginBlock(CFileIO *self, const char *id);
int __thiscall CFileIO_BeginBlockSize(CFileIO *self, const char *id, int *size);
void __thiscall CFileIO_PushBlock(CFileIO *self, const char *id, int pos);
int __thiscall CFileIO_SetError(CFileIO *self, const char *what);
void __thiscall CFileIO_PopBlock(CFileIO *self, const char *id);
/* access or stat of a name relative to the game directory, logged; pack: in the packs while they are in use */
int File_Check(const char *name, int mode, byte pack);
int File_StatX(const char *name, void *st, byte pack);

/* Engine/ArchINI.cpp (src/game/engine3d/Engine/IniFile.c): an ini file, the getters take a default; the setters
 * return 1 if the value changed */
typedef struct cDataFile cDataFile;

typedef struct CIniFile {
	cDataFile *dat;		/* 0x0 */
} CIniFile;
SD_SIZE(CIniFile, 4);

CIniFile *__thiscall CIniFile_ctorLoad(CIniFile *self, const char *name, const char *dir);
CIniFile *__thiscall CIniFile_ctor(CIniFile *self);
void __thiscall CIniFile_dtor(CIniFile *self);
void IniFile_Read(CFileIO *f, CIniFile **ini);	/* a flag, then the block; a new CIniFile */
void IniFile_Write(CFileIO *f, CIniFile *ini);
int __thiscall CIniFile_FindKey(CIniFile *self, const char *key, char *value, int size, int from);
int __thiscall CIniFile_FindSection(CIniFile *self, const char *name, int from);
byte __thiscall CIniFile_GetBool(CIniFile *self, const char *sect, const char *key, byte def);
float __thiscall CIniFile_GetFloat(CIniFile *self, const char *sect, const char *key, float def);
int __thiscall CIniFile_GetInt(CIniFile *self, const char *sect, const char *key, int def);
int __thiscall CIniFile_Count(CIniFile *self);
uint32_t __thiscall CIniFile_GetColor(CIniFile *self, const char *sect, const char *key, uint32_t def);	/* "R G B A" */
byte __thiscall CIniFile_GetSectionName(CIniFile *self, int idx, char *dest, int size);
char *__thiscall CIniFile_GetText(CIniFile *self, const char *sect, const char *key, char *dest, int size,
	const char *def);
float *__thiscall CIniFile_GetVec2(CIniFile *self, float *out, const char *sect, const char *key, const float *def);
vec3_t *__thiscall CIniFile_GetVec3(CIniFile *self, vec3_t *out, const char *sect, const char *key, const vec3_t *def);
void __thiscall CIniFile_ReadBlock(CIniFile *self, CFileIO *f);
void __thiscall CIniFile_Clear(CIniFile *self);
void __thiscall CIniFile_WriteBlock(CIniFile *self, CFileIO *f);
int __thiscall CIniFile_SetYesNo(CIniFile *self, const char *sect, const char *key, byte v);	/* "YES" / "NO" */
int __thiscall CIniFile_SetFloat(CIniFile *self, const char *sect, const char *key, float v, int prec);
int __thiscall CIniFile_SetInt(CIniFile *self, const char *sect, const char *key, int v);
int __thiscall CIniFile_SetStr(CIniFile *self, const char *sect, const char *key, const char *v);
int __thiscall CIniFile_Load(CIniFile *self, const char *name, const char *dir, byte verbose);
void __thiscall CIniFile_AddLine(CIniFile *self, const char *text, int idx);
void __thiscall CIniFile_DeleteLine(CIniFile *self, int idx);
int __thiscall CIniFile_SetBool(CIniFile *self, const char *sect, const char *key, int v);	/* "SI" / "NO" */
int __thiscall CIniFile_Save(CIniFile *self, const char *name, const char *dir, int unused);
int __thiscall CIniFile_ReadBool(CIniFile *self, const char *sect, const char *key, int *out);
int __thiscall CIniFile_ReadFloat(CIniFile *self, const char *sect, const char *key, float *out);
int __thiscall CIniFile_ReadInt(CIniFile *self, const char *sect, const char *key, int *out);
int __thiscall CIniFile_ReadText(CIniFile *self, const char *sect, const char *key, char *dest, int size);
const char *__thiscall CIniFile_LineText(CIniFile *self, int idx);
int __thiscall CIniFile_LineType(CIniFile *self, int idx);

#endif
