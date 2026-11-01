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


CFileIO *CFileIO_ctor(CFileIO *self, const char *name, const char *mode);
void CFileIO_dtor(CFileIO *self);
void Ptr_Delete(void **p);
/* the whole file in a new[] buffer; a missing file is fatal */
void *FileIO_ReadAll(const char *name, void **buf, uint32_t *size);
int CFileIO_Finish(CFileIO *self);
void File_RelPath(char *dest, const char *path);
int CFileIO_Close(CFileIO *self);
int CFileIO_Write(CFileIO *self, const void *buf, int n);
/* the length with the terminator (int), then the characters; 0 for NULL */
int CFileIO_WriteString(CFileIO *self, const char *s);
int CFileIO_EndBlockWrite(CFileIO *self, const char *id);
int CFileIO_WriteInt(CFileIO *self, int32_t v);	/* two copies in the exe (int, float) */
int CFileIO_BeginBlockWrite(CFileIO *self, const char *id);
int FileIO_Close(void *f);	/* 0 or -1 */
int FileIO_Getc(void *f);	/* 0..255 */
char *FileIO_Gets(char *buf, int size, void *f);	/* keeps the '\n' */
int CFileIO_Unsupported(CFileIO *self);
int FileIO_Read(void *buf, int size, int count, void *f);	/* count */
int FileIO_Size(void *f);
int FileIO_Tell(void *f);
int File_AccessPack(const char *name, int mode);	/* the packs while they are in use */
uint32_t IdToDword(const char *id);	/* up to 4 characters, the first in the low byte */
int File_Stat(const char *name, void *st);
void *FileIO_Open(const char *name, const char *mode);	/* a cFilePack, "rb" or "wb" */
int File_StatPack(const char *name, void *st);
byte CFileIO_ReadBool(CFileIO *self);
int CFileIO_ReadInt(CFileIO *self);
int CFileIO_Read(CFileIO *self, void *buf, int n);
int CFileIO_ReadString(CFileIO *self, char *buf, uint32_t size);
/* like CFileIO_ReadString, *isNull 1 for a NULL string */
int CFileIO_ReadStringNull(CFileIO *self, char *buf, uint32_t size, byte *isNull);
int CFileIO_Error(CFileIO *self, const char *what);	/* what: a label of the caller, unused */
int CFileIO_Read4(CFileIO *self, void *out);	/* two copies in the exe */
int CFileIO_BeginBlock(CFileIO *self, const char *id);
int CFileIO_BeginBlockSize(CFileIO *self, const char *id, int *size);
void CFileIO_PushBlock(CFileIO *self, const char *id, int pos);
int CFileIO_SetError(CFileIO *self, const char *what);
void CFileIO_PopBlock(CFileIO *self, const char *id);
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

CIniFile *CIniFile_ctorLoad(CIniFile *self, const char *name, const char *dir);
CIniFile *CIniFile_ctor(CIniFile *self);
void CIniFile_dtor(CIniFile *self);
void IniFile_Read(CFileIO *f, CIniFile **ini);	/* a flag, then the block; a new CIniFile */
void IniFile_Write(CFileIO *f, CIniFile *ini);
int CIniFile_FindKey(CIniFile *self, const char *key, char *value, int size, int from);
int CIniFile_FindSection(CIniFile *self, const char *name, int from);
byte CIniFile_GetBool(CIniFile *self, const char *sect, const char *key, byte def);
float CIniFile_GetFloat(CIniFile *self, const char *sect, const char *key, float def);
int CIniFile_GetInt(CIniFile *self, const char *sect, const char *key, int def);
int CIniFile_Count(CIniFile *self);
uint32_t CIniFile_GetColor(CIniFile *self, const char *sect, const char *key, uint32_t def);	/* "R G B A" */
byte CIniFile_GetSectionName(CIniFile *self, int idx, char *dest, int size);
char *CIniFile_GetText(CIniFile *self, const char *sect, const char *key, char *dest, int size,
	const char *def);
float *CIniFile_GetVec2(CIniFile *self, float *out, const char *sect, const char *key, const float *def);
vec3_t *CIniFile_GetVec3(CIniFile *self, vec3_t *out, const char *sect, const char *key, const vec3_t *def);
void CIniFile_ReadBlock(CIniFile *self, CFileIO *f);
void CIniFile_Clear(CIniFile *self);
void CIniFile_WriteBlock(CIniFile *self, CFileIO *f);
int CIniFile_SetYesNo(CIniFile *self, const char *sect, const char *key, byte v);	/* "YES" / "NO" */
int CIniFile_SetFloat(CIniFile *self, const char *sect, const char *key, float v, int prec);
int CIniFile_SetInt(CIniFile *self, const char *sect, const char *key, int v);
int CIniFile_SetStr(CIniFile *self, const char *sect, const char *key, const char *v);
int CIniFile_Load(CIniFile *self, const char *name, const char *dir, byte verbose);
void CIniFile_AddLine(CIniFile *self, const char *text, int idx);
void CIniFile_DeleteLine(CIniFile *self, int idx);
int CIniFile_SetBool(CIniFile *self, const char *sect, const char *key, int v);	/* "SI" / "NO" */
int CIniFile_Save(CIniFile *self, const char *name, const char *dir, int unused);
int CIniFile_ReadBool(CIniFile *self, const char *sect, const char *key, int *out);
int CIniFile_ReadFloat(CIniFile *self, const char *sect, const char *key, float *out);
int CIniFile_ReadInt(CIniFile *self, const char *sect, const char *key, int *out);
int CIniFile_ReadText(CIniFile *self, const char *sect, const char *key, char *dest, int size);
const char *CIniFile_LineText(CIniFile *self, int idx);
int CIniFile_LineType(CIniFile *self, int idx);

#endif
