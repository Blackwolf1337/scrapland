/*
 * The Util/ library of the engine3d (Spanish names in the original: ruta = path, unidad = drive, comodin =
 * wildcard, cadena = string). Paths are DOS style: drive "C:", directory with '\\' or '/' at the end, name and the
 * extension with its dot. MAX_RUTA is the buffer size the functions assume for a path.
 */
#ifndef SCRAP_E3DUTIL_H
#define SCRAP_E3DUTIL_H

#include <stdarg.h>

#include "scrap/game.h"

/* the D3D device of the engine3d (render.cpp), NULL without one */
#define g_pd3dDevice GAME_DATA(void *, 0x4ad6f4)
/* the device of the D3D application (g_pD3DApp->m_pd3dDevice), NULL before it is made */
#define g_MotorDev (*(void **)(GAME_DATA(byte *, 0x4b1c04) + 0x2a3d8))
/* the frames drawn by the engine3d */
#define g_FrameCount GAME_DATA(int, 0x4a8ec8)
/* the time of the engine in ms */
#define g_TimeMs GAME_DATA(uint32_t, 0x4a8ef0)
/* video var DiffuseMod4x: colors divided by 4 */
#define g_DiffuseMod4x GAME_DATA(int, 0x431be8)
/* the render pass of render.cpp */
#define g_RenderPass GAME_DATA(int, 0x4afdb8)
/* the D3DRS_ZENABLE value of the frame and whether the fog is on (render.cpp) */
#define g_ZEnable GAME_DATA(uint32_t, 0x4ad740)
#define g_FogActive GAME_DATA(int, 0x4ad744)
/* the D3DRS_AMBIENT of the scenes */
#define g_AmbientD3D GAME_DATA(uint32_t, 0x4ad748)
/* the debug fonts (their cursor x, y first) */
#define g_DebugFont GAME_DATA(void *, 0x4d6dcc)
#define g_DebugFont2 GAME_DATA(void *, 0x4d6dc8)

/* the getters of render.cpp (rva 0x24c4d0, 0x24cdb0, 0x24cdc0, 0x24cdd0): the projection and the view matrix, the
 * camera turn of the sprites and the camera position */
const matrix_t *Render_GetProj(void);
const matrix_t *Render_GetView(void);
const matrix_t *Render_GetBillboard(void);
const vec3_t *Render_GetEyePos(void);

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

/*
 * Util/Arch_dat.cpp: a data file as a list of text lines, ini style: "[section]", "key = value ; comment",
 * ";comment". Line 0 is a head without text, the lines of the file are 1..count-1. The last line found is cached
 * for the next lookup.
 */
#define ARCHDATOS_ID 0x74614441	/* "ADat" */

enum {
	LINEA_UNKNOWN = 0,		/* not looked at yet (Arch_Classify) */
	LINEA_HEAD = 1,
	LINEA_SECTION = 2,
	LINEA_KEY = 3,
	LINEA_COMMENT = 4,
	LINEA_EMPTY = 5,
};

typedef struct cLineaDat {
	struct cLineaDat *prev;		/* 0x0 */
	struct cLineaDat *next;		/* 0x4 */
	int type;			/* 0x8 LINEA_ */
	char *text;			/* 0xc Cad_Dup */
} cLineaDat;
SD_SIZE(cLineaDat, 0x10);

typedef struct cArchDatos {
	uint32_t id;			/* 0x00 ARCHDATOS_ID */
	int count;			/* 0x04 lines with the head */
	cLineaDat *head;		/* 0x08 */
	cLineaDat *cur;			/* 0x0c the line last found */
	int curIdx;			/* 0x10 its index, -1 for none */
	int caseSensitive;		/* 0x14 names of sections and keys */
} cArchDatos;
SD_SIZE(cArchDatos, 0x18);

void Arch_AddSection(cArchDatos *dat, const char *name, int idx);	/* "[name]" after line idx */
void Arch_AddLine(cArchDatos *dat, const char *text, int idx);
void Arch_InsertAt(cArchDatos *dat, const char *text, int idx);	/* after line idx, clamped */
int Arch_StrnCmp(cArchDatos *dat, const char *a, const char *b, size_t n);
int Arch_SectionEnd(cArchDatos *dat, int idx);	/* the last key line of the section at idx */
int Arch_FindSection(cArchDatos *dat, const char *name, int from);	/* its line or 0 */
/* the line of key in the section from line from on, 0 if not there; value without blanks and comment */
int Arch_FindKey(cArchDatos *dat, const char *key, int from, char *value, int size);
void Arch_SectionName(char *dest, const char *header, int size);
void Arch_Classify(cArchDatos *dat, int idx);
void Arch_Delete(cArchDatos *dat, int idx);
void Arch_Error(int code, int fatal, cArchDatos *dat, int idx);	/* Err_Report with the line */
int Arch_Write(cArchDatos *dat, void *f);	/* FILE of the exe's CRT; 0 or -1 */
void Arch_Free(cArchDatos *dat);
cArchDatos *Arch_New(void);
int Arch_Load(cArchDatos *dat, const char *name, int idx);	/* the lines of a file after line idx; 0 or -1 */
const char *Arch_Value(cArchDatos *dat, int idx, int *len);	/* the value of a key line */
void Arch_SetValue(cArchDatos *dat, const char *value, int idx);	/* keeps the column of the comment */
int Arch_Count(cArchDatos *dat);
void Arch_Dump(cArchDatos *dat, int from, int to);	/* to stdout */
cLineaDat *Arch_Line(cArchDatos *dat, int idx);
int Arch_LineType(cArchDatos *dat, int idx);
void Arch_Printf(const char *fmt, ...);	/* stdout of the exe's CRT */

/* Util/Dat_simp.cpp: typed values of a cArchDatos; the getters give 0 or -1 */
int Dat_SetBool(cArchDatos *dat, const char *sect, const char *key, int v);	/* "SI" / "NO" */
int Dat_SetInt(cArchDatos *dat, const char *sect, const char *key, int v);
int Dat_SetStr(cArchDatos *dat, const char *sect, const char *key, const char *value);	/* 0 if unchanged */
int Dat_GetBool(cArchDatos *dat, const char *sect, const char *key, int *out);
int Dat_GetDouble(cArchDatos *dat, const char *sect, const char *key, double *out);
int Dat_GetFloat(cArchDatos *dat, const char *sect, const char *key, float *out);
int Dat_GetLong(cArchDatos *dat, const char *sect, const char *key, int *out);	/* two copies in the exe */
int Dat_GetText(cArchDatos *dat, const char *sect, const char *key, char *dest, int size);
void Dat_GetList(cArchDatos *dat, const char *sect, const char *key, cArchDatos *dest);	/* every value of key */
int Dat_GetWord(cArchDatos *dat, const char *sect, const char *key, uint16_t *out);
int Dat_GetStr(cArchDatos *dat, const char *sect, const char *key, char *dest, int size);

/* Util/Proc_dat.cpp: sscanf of the engine with its own conversions ("&l", "&f", "&.*t", "&(tipo_si_no)l"), see
 * src/game/engine3d/Util/Proc_dat.c */
int Proc_Note(char *dest, const char *src, const void *tipo, int size);	/* converter of tipo_nota */
int Proc_TypeLookup(char *dest, const char *name, const void *data, int size);
int Proc_ReadField(va_list *ap, const char **psrc, const char **pfmt);	/* 0 or -1 */
int Proc_DefineType(const char *ident, const void *defin);
int Proc_Scan(const char *s, const char *fmt, ...);	/* the number of values read */

#endif
