/*
 * The Util/ library of the engine3d (Spanish names in the original: ruta = path, unidad = drive, comodin =
 * wildcard, cadena = string). Paths are DOS style: drive "C:", directory with '\\' or '/' at the end, name and the
 * extension with its dot. MAX_RUTA is the buffer size the functions assume for a path.
 */
#ifndef SCRAP_E3DUTIL_H
#define SCRAP_E3DUTIL_H

#include <stdarg.h>

#include "scrap/game.h"

#define MAX_RUTA 0x104

/* Ruta_Part masks */
#define RUTA_DRIVE 0x1
#define RUTA_DIR 0x2
#define RUTA_NAME 0x4
#define RUTA_EXT 0x8

/* Util/Comodin.cpp: paths and wildcards ('*', '?', "[a-z]", "[!...]"), Wild_ case-sensitive */
void Ruta_GetDrive(int *drive);
int Ruta_MakeDir(char *ruta);	/* with the missing parents; 0 or -1, may change the current directory */
const char *Wild_MatchSet(const char *pattern, const char *s);
int Wild_Match(const char *pattern, const char *s);
char *Ruta_Make(char *ruta, const char *drive, const char *dir, const char *name, const char *ext);
/* frmt with &u drive, &d directory, &n name, &e extension of orig, && for '&' */
char *Ruta_Format(char *dest, const char *orig, const char *frmt);
char *Ruta_IsDir(char *ruta);	/* ruta with a '\\' added if it is a directory, else NULL */
int Ruta_MakePath(const char *ruta);	/* the directories of ruta; 0 or -1 */
/* drive up to 3, the others up to 255 characters; NULL for a part not wanted */
void Ruta_Split(const char *ruta, char *drive, char *dir, char *name, char *ext);
int Ruta_IsSep(char c);
/* the parts of ruta1, those it does not have from ruta2 */
char *Ruta_Merge(char *dest, const char *ruta1, const char *ruta2);
int Ruta_Match(const char *pattern, const char *ruta);	/* name and extension, case-insensitive */
int Ruta_MatchPath(const char *pattern, const char *ruta, int subdirs);
/* name and extension of src through pattern: '*', '?', "(1 2 -1)" characters by index (n>0 name, n<0 ext) */
void Ruta_Rename(char *dest, const char *src, const char *pattern);
char *Ruta_Full(char *dest, const char *rel, int size, const char *base);
char *Ruta_Part(char *dest, const char *orig, uint32_t mask);
char *Ruta_PartN(char *dest, int size, const char *orig, uint32_t mask);
char *Ruta_FileName(const char *ruta);	/* after the last separator or ':' */
char *Ruta_FirstDir(char *dest, const char *ruta);
char *Ruta_LastDir(char *dest, const char *ruta);
int Ruta_DriveNum(const char *ruta);	/* 1 for "A:", 0 without drive */
int Ruta_GetCwd(int drive, char *dest, int size);	/* 0 or -1 */

/* Util/Cadenas.cpp */
void Cad_InsertChar(char *s, char c, int pos, int len);
/* Str_Copy with CHECK(dest && orig && dest_nby>0) */
char *Cad_Copy(char *dst, const char *src, int size);
char *Cad_Dup(const char *s);	/* Mem_Alloc */
int Cad_Thousands(char *s);	/* separators into the trailing digits, the new length */
char *Cad_FromInt(char *buf, int n);	/* with separators, a static buffer for NULL */

/* Util/Bprintf.cpp: Str_Printf without the log line */
int Bprintf(char *buf, int size, const char *fmt, ...);
int VBprintf(char *buf, int size, const char *fmt, va_list ap);

/* Util/Errores.cpp: error codes 0..199 with texts (50 and up for Err_Register); a report goes to the log when
 * fatal, else into a message box */
void Dbg_FrameRect(int x, int y, int w, int h, uint32_t color);	/* color 0xRRGGBB, on the engine window */
void Dbg_FillRect(int x, int y, int w, int h, uint32_t color);
const char *Err_Unregister(int code);
const char *Err_Str(int code);
int Err_Register(const char *text);	/* the new code or -1 */
void Err_Code(int code, int fatal);
[[noreturn]] void Err_Exit(const char *text);	/* to stderr, then exit(-1) */
void Err_Printf(int fatal, const char *fmt, ...);
void Err_Report(int code, int fatal, const char *fmt, ...);
void Err_VReport(int code, int fatal, const char *fmt, va_list ap);

/* Util/Listadin.cpp: a growing array of elemSize byte elements, allocated in blocks of blockSize elements */
#define LISTADIN_ID 0x6e69444c	/* "LDin" */

typedef struct cListaDin {
	uint32_t id;			/* 0x00 LISTADIN_ID */
	byte *mem;			/* 0x04 Mem_Alloc */
	uint32_t total;			/* 0x08 elements allocated */
	uint32_t used;			/* 0x0c */
	uint32_t elemSize;		/* 0x10 */
	uint16_t blockSize;		/* 0x14 */
	byte changed;			/* 0x16 set by every change */
	byte pad_17;
} cListaDin;
SD_SIZE(cListaDin, 0x18);

void LDin_Free(cListaDin *din);
cListaDin *LDin_New(uint16_t elemSize);	/* blocks of 256 */
cListaDin *LDin_NewBlock(uint16_t elemSize, uint16_t blockSize);
void *LDin_Get(cListaDin *din, uint32_t i);
uint32_t LDin_Count(cListaDin *din);
void *LDin_Last(cListaDin *din);
void *LDin_AddOne(cListaDin *din);
void *LDin_Add(cListaDin *din, int num);	/* the first new element */
void LDin_Copy(cListaDin *din, uint32_t orig, uint32_t dest, int num);	/* memmove of elements */
void LDin_Delete(cListaDin *din, uint32_t i);
void LDin_RemoveLast(cListaDin *din);
void *LDin_Insert(cListaDin *din, uint32_t i);	/* a new element at i */
void LDin_Move(cListaDin *din, int orig, int dest);	/* an element to another place, -1 the last */
void LDin_Clear(cListaDin *din);

#endif
