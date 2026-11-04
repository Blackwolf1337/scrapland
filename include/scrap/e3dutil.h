/*
 * The Util/ library of the engine3d (Spanish names in the original: path = path, unidad = drive, comodin =
 * wildcard, cadena = string). Paths are DOS style: drive "C:", directory with '\\' or '/' at the end, name and the
 * extension with its dot. MAX_FILEPATH is the buffer size the functions assume for a path.
 */
#ifndef SCRAP_E3DUTIL_H
#define SCRAP_E3DUTIL_H

#include <stdarg.h>

#include "scrap/dxutil.h"
#include "scrap/game.h"
#include "scrap/render.h"

/* the frames drawn by the engine3d */
extern int g_FrameCount;
/* the time of the last frame (the clock of the engine, also g_FrameDelta of Engine.c) */
extern float g_FrameMs;
/* the time of the engine in ms */
extern uint32_t g_TimeMs;
/* video var DiffuseMod4x: colors divided by 4 */
extern int g_DiffuseMod4x;

#define MAX_FILEPATH 0x104

/* Path_Part masks */
#define PATH_DRIVE 0x1
#define PATH_DIR 0x2
#define PATH_NAME 0x4
#define PATH_EXT 0x8

int Wild_Match(const char *pattern, const char *s);
/* frmt with &u drive, &d directory, &n name, &e extension of orig, && for '&' */
char *Path_Format(char *dest, const char *orig, const char *frmt);
char *Path_IsDir(char *path);	/* path with a '\\' added if it is a directory, else NULL */
int Path_MakePath(const char *path);	/* the directories of path; 0 or -1 */
/* drive up to 3, the others up to 255 characters; NULL for a part not wanted */
void Path_SplitDrive(const char *path, char *drive, char *dir, char *name, char *ext);
int Path_IsSep(char c);
/* the parts of path1, those it does not have from path2 */
char *Path_Merge(char *dest, const char *path1, const char *path2);
char *Path_Full(char *dest, const char *rel, int size, const char *base);
char *Path_Part(char *dest, const char *orig, uint32_t mask);
char *Path_FileName(const char *path);	/* after the last separator or ':' */

/* Str_Copy with CHECK(dest && orig && dest_nby>0) */
char *String_Copy(char *dst, const char *src, int size);
char *String_Dup(const char *s);	/* Mem_Alloc */

/* Util/Bprintf.cpp: Str_Printf without the log line */
int Bprintf(char *buf, int size, const char *fmt, ...);
int VBprintf(char *buf, int size, const char *fmt, va_list ap);

/* Util/Errores.cpp: error codes 0..199 with texts (50 and up for Err_Register); a report goes to the log when
 * fatal, else into a message box */
/* a line in pixels over the frame, color 0xRRGGBB (width is not used: the original's GDI pen width) */
void Debug_Line(int x0, int y0, int x1, int y1, uint32_t color, int width);
const char *Err_Str(int code);
int Err_Register(const char *text);	/* the new code or -1 */
void Err_Printf(int fatal, const char *fmt, ...);
void Err_Report(int code, int fatal, const char *fmt, ...);

/* Util/Listadin.cpp: a growing array of elemSize byte elements, allocated in blocks of blockSize elements */
#define DYNLIST_ID 0x6e69444c	/* "LDin" */

typedef struct cDynList {
	uint32_t id;			/* DYNLIST_ID */
	byte *mem;			/* Mem_Alloc */
	uint32_t total;			/* elements allocated */
	uint32_t used;
	uint32_t elemSize;
	uint16_t blockSize;
	byte changed;			/* set by every change */
	byte unused17;			/* cleared by the constructor */
} cDynList;

void DynList_Free(cDynList *dyn);
cDynList *DynList_NewBlock(uint16_t elemSize, uint16_t blockSize);
void *DynList_AddOne(cDynList *dyn);

/*
 * Util/Arch_dat.cpp: a data file as a list of text lines, ini style: "[section]", "key = value ; comment",
 * ";comment". Line 0 is a head without text, the lines of the file are 1..count-1. The last line found is cached
 * for the next lookup.
 */
#define DATAFILE_ID 0x74614441	/* "ADat" */

enum {
	LINE_UNKNOWN = 0,		/* not looked at yet (DataFile_Classify) */
	LINE_HEAD = 1,
	LINE_SECTION = 2,
	LINE_KEY = 3,
	LINE_COMMENT = 4,
	LINE_EMPTY = 5,
};

typedef struct cDatLine {
	struct cDatLine *prev;
	struct cDatLine *next;
	int type;			/* LINEA_ */
	char *text;			/* String_Dup */
} cDatLine;

typedef struct cDataFile {
	uint32_t id;			/* DATAFILE_ID */
	int count;			/* lines with the head */
	cDatLine *head;
	cDatLine *cur;			/* the line last found */
	int curIdx;			/* its index, -1 for none */
	int caseSensitive;		/* names of sections and keys */
} cDataFile;

void DataFile_AddSection(cDataFile *dat, const char *name, int idx);	/* "[name]" after line idx */
void DataFile_AddLine(cDataFile *dat, const char *text, int idx);
void DataFile_InsertAt(cDataFile *dat, const char *text, int idx);	/* after line idx, clamped */
int DataFile_SectionEnd(cDataFile *dat, int idx);	/* the last key line of the section at idx */
int DataFile_FindSection(cDataFile *dat, const char *name, int from);	/* its line or 0 */
/* the line of key in the section from line from on, 0 if not there; value without blanks and comment */
int DataFile_FindKey(cDataFile *dat, const char *key, int from, char *value, int size);
void DataFile_SectionName(char *dest, const char *header, int size);
void DataFile_Delete(cDataFile *dat, int idx);
int DataFile_Write(cDataFile *dat, void *f);	/* FILE of the exe's CRT; 0 or -1 */
void DataFile_Free(cDataFile *dat);
cDataFile *DataFile_New(void);
int DataFile_Load(cDataFile *dat, const char *name, int idx);	/* the lines of a file after line idx; 0 or -1 */
void DataFile_SetValue(cDataFile *dat, const char *value, int idx);	/* keeps the column of the comment */
int DataFile_Count(cDataFile *dat);
cDatLine *DataFile_Line(cDataFile *dat, int idx);
int DataFile_LineType(cDataFile *dat, int idx);
void DataFile_Printf(const char *fmt, ...);	/* stdout of the exe's CRT */

/* Util/Dat_simp.cpp: typed values of a cDataFile; the getters give 0 or -1 */
int Dat_SetBool(cDataFile *dat, const char *sect, const char *key, int v);	/* "SI" / "NO" */
int Dat_SetInt(cDataFile *dat, const char *sect, const char *key, int v);
int Dat_SetStr(cDataFile *dat, const char *sect, const char *key, const char *value);	/* 0 if unchanged */
int Dat_GetBool(cDataFile *dat, const char *sect, const char *key, int *out);
int Dat_GetFloat(cDataFile *dat, const char *sect, const char *key, float *out);
int Dat_GetLong(cDataFile *dat, const char *sect, const char *key, int *out);	/* two copies in the exe */
int Dat_GetText(cDataFile *dat, const char *sect, const char *key, char *dest, int size);

int Proc_Scan(const char *s, const char *fmt, ...);	/* the number of values read */

/*
 * Util/Paletas.cpp (engine3d/util/paletas.h): palettes of 4 bytes a color (r g b and a flag, 5 when loaded) and
 * 16 bit palettes for a pixel format given by its masks. A 16 bit palette is a Mem_Alloc block of 0x200 bytes more
 * than the colors need, the colors start at the first 512 byte boundary after its start.
 */
typedef struct Pal16Shift {
	byte rShr, rShl;		/* the 8 bit value shifted right, then left into its place */
	byte gShr, gShl;
	byte bShr, bShl;
} Pal16Shift;
extern Pal16Shift g_Pal16Shift;
#define PAL16_COLORS(pal) ((uint16_t *)(((uintptr_t)(pal) + 0x200) & ~(uintptr_t)0x1ff))

/* the shifts of the 16 bit format from its masks; 8/0 for each without all three masks */
void Pal16_SetMasks(uint32_t r, uint32_t g, uint32_t b);

/* Util/Pcx.cpp: 8 bit PCX images, run length coded; lx and ly in the header are the size (from the window when 0) */
typedef struct PcxHeader {
	byte manuf;			/* 10 */
	byte version;			/* 5 */
	byte encoding;			/* 1 */
	byte bpp;			/* 8 */
	int16_t xmin, ymin;
	int16_t xmax, ymax;
	byte pad_0c[0x35];
	byte planes;
	uint16_t bpl;			/* bytes a line of a plane */
	uint16_t palInfo;
	uint16_t lx;
	uint16_t ly;
	byte pad_4a[0x36];
} PcxHeader;

/* the error code of a header that is no 8 bit PCX (errno) */
extern const int g_ErrPcxFormat;

/* a 24 bit image (3 bytes a pixel) to f as 3 planes: 0, -1 when a write fails */
int Pcx_Write24(void *f, const byte *mem, int lx, int ly);

typedef struct procrange_t {
	int lo, hi;
} procrange_t;

/* converts the identifier name of a type into the text of its number */
typedef int (*proctype_t)(char *dest, const char *name, const void *data, int size);

typedef struct cProcType {
	const char *name;
	const void *data;		/* the "IDENT value" list of Proc_TypeLookup */
	proctype_t convert;		/* NULL: Proc_TypeLookup */
} cProcType;

#endif
