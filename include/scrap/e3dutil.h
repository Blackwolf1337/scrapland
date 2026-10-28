/*
 * The Util/ library of the engine3d (Spanish names in the original: ruta = path, unidad = drive, comodin =
 * wildcard, cadena = string). Paths are DOS style: drive "C:", directory with '\\' or '/' at the end, name and the
 * extension with its dot. MAX_RUTA is the buffer size the functions assume for a path.
 */
#ifndef SCRAP_E3DUTIL_H
#define SCRAP_E3DUTIL_H

#include <stdarg.h>

#include "scrap/game.h"
#include "scrap/render.h"

/* g_pD3DApp, the D3D application of the engine3d (CD3DApplication of the D3D framework): its window, its device
 * (NULL before it is made) and the RasterCaps of the device */
#define g_Motor GAME_GLOBAL(byte *, g_Motor, 0x4b1c04)
#define MOTOR_HWND(m) (*(void **)((m) + 0x2a3cc))
#define MOTOR_DEV(m) (*(void **)((m) + 0x2a3d8))
#define MOTOR_RASTERCAPS(m) (*(uint32_t *)((m) + 0x2a400))
#define MOTOR_CAPS(m) ((const uint32_t *)((m) + 0x2a3dc))	/* D3DCAPS8, 0x35 words */
#define MOTOR_ADAPTER(m) (*(int *)((m) + 0x2a380))
#define MOTOR_ADAPTERS(m) ((m) + 4)			/* 0x438c each, the identifier first */
#define MOTOR_MODE(m) ((const uint32_t *)((m) + 0x2a4b0))	/* 8 words: format, ..., w, h */
#define MOTOR_ZBUF(m) ((const uint32_t *)((m) + 0x2a4d0))
#define MOTOR_DEVID(m) (*(int *)((m) + 0x2a5a8))
#define MOTOR_DEVNAME(m) ((const char *)((m) + 0x2a524))
#define g_MotorDev MOTOR_DEV(g_Motor)
/* the frames drawn by the engine3d */
#define g_FrameCount GAME_GLOBAL(int, g_FrameCount, 0x4a8ec8)
/* the time of the last frame (the clock of the engine, also g_FrameDelta of Motor.c) */
#define g_FrameMs GAME_GLOBAL(float, g_FrameMs, 0x4a8ed4)
/* the time of the engine in ms */
#define g_TimeMs GAME_GLOBAL(uint32_t, g_TimeMs, 0x4a8ef0)
/* video var DiffuseMod4x: colors divided by 4 */
#define g_DiffuseMod4x GAME_GLOBAL(int, g_DiffuseMod4x, 0x431be8)

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
/* a GDI line of width pixels on the engine window, color 0xRRGGBB */
void Debug_Line(int x0, int y0, int x1, int y1, uint32_t color, int width);
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

/*
 * Util/Paletas.cpp (engine3d/util/paletas.h): palettes of 4 bytes a color (r g b and a flag, 5 when loaded) and
 * 16 bit palettes for a pixel format given by its masks. A 16 bit palette is a Mem_Alloc block of 0x200 bytes more
 * than the colors need, the colors start at the first 512 byte boundary after its start.
 */
typedef struct Pal16Shift {
	byte rShr, rShl;		/* 0x0 the 8 bit value shifted right, then left into its place */
	byte gShr, gShl;		/* 0x2 */
	byte bShr, bShl;		/* 0x4 */
} Pal16Shift;
#define g_Pal16Shift GAME_GLOBAL(Pal16Shift, g_Pal16Shift, 0x4e6e18)
#define PAL16_COLORS(pal) ((uint16_t *)(((uintptr_t)(pal) + 0x200) & ~(uintptr_t)0x1ff))

uint32_t Pal16_FromRGB(byte r, byte g, byte b);	/* 16 bits, the upper ones 0 */
void Pal16_ToRGB(byte *r, byte *g, byte *b, uint16_t c);
/* the color first..last nearest to r g b (sum of the differences), the first with distance 0 at once */
int Pal_Nearest(const byte *pal, int first, int last, int r, int g, int b);
/* the 768 bytes of a 6 bit palette file into 256 colors */
void Pal_Load(const char *name, byte *dest);
/* the same as a new 16 bit palette; CHECK(dest && !*dest) */
void Pal16_Load(const char *name, void **dest);
/* num colors of src from i2 to dst at i1; CHECK(num > 0 && i1 > 0 && i2 > 0) */
void Pal_Copy(byte *dst, int i1, const byte *src, int i2, int num);
void *Pal_ToPal16(const byte *pal, int num);
/* each component scaled and clamped to 0..255 first */
void *Pal_ToPal16Scaled(const byte *pal, int num, float r, float g, float b);
/* the shifts of the 16 bit format from its masks; 8/0 for each without all three masks */
void Pal16_SetMasks(uint32_t r, uint32_t g, uint32_t b);
/* r g b towards r2 g2 b2 by t (-32..32, 64ths from -32), r2 g2 b2 from 32 on, unchanged up to -32 */
void RGB_Blend(int *r, int *g, int *b, int r2, int g2, int b2, int t);
/* the num colors of a 16 bit palette scaled */
void Pal16_Scale(void *pal16, int num, float r, float g, float b);

/* Util/Pcx.cpp: 8 bit PCX images, run length coded; lx and ly in the header are the size (from the window when 0) */
typedef struct PcxHeader {
	byte manuf;			/* 0x00 10 */
	byte version;			/* 0x01 5 */
	byte encoding;			/* 0x02 1 */
	byte bpp;			/* 0x03 8 */
	int16_t xmin, ymin;		/* 0x04 */
	int16_t xmax, ymax;		/* 0x08 */
	byte pad_0c[0x35];
	byte planes;			/* 0x41 */
	uint16_t bpl;			/* 0x42 bytes a line of a plane */
	uint16_t palInfo;		/* 0x44 */
	uint16_t lx;			/* 0x46 */
	uint16_t ly;			/* 0x48 */
	byte pad_4a[0x36];
} PcxHeader;
SD_OFFSET(PcxHeader, planes, 0x41);
SD_OFFSET(PcxHeader, lx, 0x46);
SD_SIZE(PcxHeader, 0x80);

/* the error code of a header that is no 8 bit PCX (errno) */
#define g_ErrPcxFormat GAME_GLOBAL(int, g_ErrPcxFormat, 0x3de07c)

/* the pixels from src into dest (planes interleaved), the end of the data read */
const byte *Pcx_Decode(byte *dest, const byte *src, const PcxHeader *hdr);
/* the same from a FILE of the exe's CRT: 0, -1 at the end of the file */
int Pcx_DecodeFile(byte *dest, void *f, const PcxHeader *hdr);
/* a 24 bit image (3 bytes a pixel) to f as 3 planes: 0, -1 when a write fails */
int Pcx_Write24(void *f, const byte *mem, int lx, int ly);
/* an 8 bit image with the palette after it (pal 768 6 bit bytes, or 256 colors of 4 bytes with pal4): 0 or -1 */
int Pcx_Write8(void *f, const byte *mem, int lx, int ly, const byte *pal, int pal4);
/* 0, -1 with errno */
int Pcx_ReadHeader(void *f, PcxHeader *hdr);
/* the palette after the image (12, then num colors of 3 bytes) into 4 byte colors; 0 also without one, -1 */
int Pcx_ReadPal(byte *dest, void *f, int num);
/* 0 for an 8 bit PCX (lx and ly set), else -1 with errno g_ErrPcxFormat */
int Pcx_CheckHeader(PcxHeader *hdr);

typedef struct procrange_t {
	int lo, hi;
} procrange_t;

/* converts the identifier name of a type into the text of its number */
typedef int (*proctype_t)(char *dest, const char *name, const void *data, int size);

typedef struct cProcType {
	const char *name;		/* 0x0 */
	const void *data;		/* 0x4 the "IDENT value" list of Proc_TypeLookup */
	proctype_t convert;		/* 0x8 NULL: Proc_TypeLookup */
} cProcType;
SD_SIZE(cProcType, 0xc);

#endif
