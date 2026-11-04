/*
 * Engine/fuentes.cpp of the engine3d: the bitmap fonts. CFont3 draws a string glyph by glyph through one CGraphic2D:
 * the glyphs first..last are rectangles of its texture found from the borders of the bitmap (or a .MST next to it),
 * advanced by their width or a fixed one. "\1XXXX" in a string (XXXX hex) is a character of the unicode font
 * (g_Unifont, the Unicode.alpha files of g_FontsDir) drawn at the height of the glyph first. The debug fonts
 * gFntComic14 and gFntTerm8 are made at the start. CFontGrid (glyphs in a grid of cells) and CDigits (ten digit
 * sprites) are not used by the game.
 */
#ifndef SCRAP_FONTS_H
#define SCRAP_FONTS_H

#include "scrap/bmp.h"
#include "scrap/graphic2d.h"
#include "scrap/table.h"
#include "scrap/texture.h"

/* the rectangle of a glyph in the texture, 0..1 */
typedef struct FntGlyph {
	float u0, v0, u1, v1;
} FntGlyph;

typedef struct CFont3 {
	float x, y;			/* the end of the last string */
	float width;			/* the advance of a monospaced font */
	float wave;			/* 1, the scale of FNT_WAVE */
	uint32_t color;			/* Font_SetColor */
	uint32_t flags;			/* Font_SetFlags, FNT_SHADOW */
	int unused_18;
	byte round;			/* positions rounded to pixels */
	byte mono;			/* monospaced */
	byte pad_1e[2];
	float uniScale[2];		/* of the unicode characters, not set by the constructor */
	float uniOffset[2];
	CGraphic2D *graphic;		/* owned */
	CTable glyphs;			/* FntGlyph of first..last */
	byte first, last;
	byte pad_3e[2];
	int bmpWidth;			/* the size of the texture */
	int bmpHeight;
} CFont3;

/* CFont3.flags */
#define FNT_SHADOW 0x20000		/* Font_Printf with FNT_PRINT_SHADOW draws a shadow */
/* the flags of Font_Printf */
#define FNT_PRINT_COLOR 0x2		/* in the color of the font, else white */
#define FNT_PRINT_SHADOW 0x8
/* the alignment of CFont3_Measure (the position is that corner of the string) and the flags of CFont3_PrintAlign */
enum {
	FNT_ALIGN_NONE = 0,
	FNT_ALIGN_RIGHT = 1,
	FNT_ALIGN_BOTTOM = 2,
	FNT_ALIGN_RIGHT_BOTTOM = 3,
	FNT_ALIGN_CENTER = 4,
};
#define FNT_WAVE 0x10000		/* every glyph scaled by wave, the odd ones the other way */
#define FNT_OUTLINE 0x40000		/* with an outline (CFont3_PrintOutline) */

typedef struct CFontGrid {
	CGraphic2D graphic;
	byte first, last;
	byte pad_46[2];
	int cellWidth;			/* pixels of a 256 wide texture */
	int cellHeight;
	int unused_50;
	int unused_54;
	float space;			/* the advance */
} CFontGrid;

typedef struct CDigits {
	CGraphic2D digit[10];		/* bmp/digitos/digito0001.tga .. 0010 */
	int unused_2a8;
	int unused_2ac;
	float unused_2b0;			/* 24 */
} CDigits;

typedef struct CUnifont {
	uint16_t first, last;		/* the unicode range of map */
	uint16_t *map;			/* Mem_Alloc, the sprite of each character */
	CGraphic2D *graphic;
	CMultiSprite *sprites;
} CUnifont;

extern CUnifont g_Unifont;
extern const char *g_FontsDir;	/* "2d/fonts-drm/" */
extern CFont3 *g_FntComic14;
extern CFont3 *g_FntTerm8;
extern int g_EngineInits;	/* the starts of the device objects of the engine */

/* the font of tex (opac its opacity map), glyphs first..last. generate: no sprite of its own, the glyphs packed into a
 * new alpha texture instead; unused not read */
CFont3 *CFont3_ctor(CFont3 *self, const char *tex, const char *opac, byte first, byte last, float width,
	byte generate, uint32_t flags, float unused);
void CFont3_dtor(CFont3 *self);
/* the size of s (dim) and the position of its top left corner for the corner at x, y (pos, by align) */
void CFont3_Measure(const CFont3 *self, float x, float y, uint16_t align, const char *s, float *pos,
	float *dim);
/* the scale of the glyphs */
void CFont3_SetScale(CFont3 *self, float x, float y);
uint32_t Font_SetColor(CFont3 *self, uint32_t color);	/* returns 2 */
/* s at x, y (top left) in the color of the sprite; x, y become the end */
void CFont3_Print(CFont3 *self, float x, float y, const char *s);
/* flags: the alignment, FNT_WAVE, FNT_OUTLINE */
void CFont3_PrintAlign(CFont3 *self, float x, float y, uint32_t flags, uint32_t color, const char *s);
void Font_PrintfXY(CFont3 *font, float x, float y, const char *fmt, ...);
/* flags FNT_PRINT_* */
void Font_Printf(CFont3 *font, float x, float y, byte flags, const char *fmt, ...);
/* a text with a black shadow at the screen position of pos moved by x, y (2D screen pixels) */
void Debug_Text3D(CFont3 *font, float x, float y, const vec3_t *pos, uint32_t color, const char *fmt, ...);
void Fonts_Init(void);
void Fonts_Free(void);

void CUnifont_Load(CUnifont *self);
void CUnifont_Free(CUnifont *self);

#endif
