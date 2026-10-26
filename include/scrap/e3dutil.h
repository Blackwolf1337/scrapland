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

/* Util/Errores.cpp: an engine error; fatal through Con_SysErrorf, else a message box (rva 0x27cea0) */
void Err_Printf(int fatal, const char *fmt, ...);

#endif
