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
SD_SIZE(FntGlyph, 0x10);

typedef struct CFont3 {
	float x, y;			/* 0x00 the end of the last string */
	float width;			/* 0x08 the advance of a monospaced font */
	float wave;			/* 0x0c 1, the scale of FNT_WAVE */
	uint32_t color;			/* 0x10 Font_SetColor */
	uint32_t flags;			/* 0x14 Font_SetFlags, FNT_SHADOW */
	int unused_18;			/* 0x18 */
	byte round;			/* 0x1c positions rounded to pixels */
	byte mono;			/* 0x1d monospaced */
	byte pad_1e[2];
	float uniScale[2];		/* 0x20 of the unicode characters, not set by the constructor */
	float uniOffset[2];		/* 0x28 */
	CGraphic2D *graphic;		/* 0x30 owned */
	CTable glyphs;			/* 0x34 FntGlyph of first..last */
	byte first, last;			/* 0x3c */
	byte pad_3e[2];
	int bmpWidth;			/* 0x40 the size of the texture */
	int bmpHeight;			/* 0x44 */
} CFont3;
SD_SIZE(CFont3, 0x48);

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
	CGraphic2D graphic;			/* 0x00 */
	byte first, last;			/* 0x44 */
	byte pad_46[2];
	int cellWidth;			/* 0x48 pixels of a 256 wide texture */
	int cellHeight;			/* 0x4c */
	int unused_50;			/* 0x50 */
	int unused_54;			/* 0x54 */
	float space;			/* 0x58 the advance */
} CFontGrid;
SD_SIZE(CFontGrid, 0x5c);

typedef struct CDigits {
	CGraphic2D digit[10];		/* 0x000 bmp/digitos/digito0001.tga .. 0010 */
	int unused_2a8;			/* 0x2a8 */
	int unused_2ac;			/* 0x2ac */
	float unused_2b0;			/* 0x2b0 24 */
} CDigits;
SD_SIZE(CDigits, 0x2b4);

typedef struct CUnifont {
	uint16_t first, last;		/* 0x0 the unicode range of map */
	uint16_t *map;			/* 0x4 Mem_Alloc, the sprite of each character */
	CGraphic2D *graphic;		/* 0x8 */
	CMultiSprite *sprites;		/* 0xc */
} CUnifont;
SD_SIZE(CUnifont, 0x10);

#define g_Unifont (GAME_GLOBAL(CUnifont, g_Unifont, 0x4d6de0))
#define g_FontsDir GAME_GLOBAL(const char *, g_FontsDir, 0x431b50)	/* "2d/fonts-drm/" */
#define g_FntComic14 GAME_GLOBAL(CFont3 *, g_FntComic14, 0x4d6dc8)
#define g_FntTerm8 GAME_GLOBAL(CFont3 *, g_FntTerm8, 0x4d6dcc)
/* the materials of the outline of CFont3_PrintOutline (the font and the unicode font), made again after each start
 * of the device (g_EngineInits) or for another texture */
#define g_FntMatOutline GAME_GLOBAL(CMaterial *, g_FntMatOutline, 0x4d6dd0)
#define g_FntMatOutlineInit GAME_GLOBAL(int, g_FntMatOutlineInit, 0x4d6dd4)
#define g_FntMatOutlineUni GAME_GLOBAL(CMaterial *, g_FntMatOutlineUni, 0x4d6dd8)
#define g_FntMatOutlineUniInit GAME_GLOBAL(int, g_FntMatOutlineUniInit, 0x4d6ddc)
#define g_EngineInits GAME_GLOBAL(int, g_EngineInits, 0x4a8f74)	/* the starts of the device objects of the engine */

CFontGrid *__thiscall CFontGrid_ctor(CFontGrid *self, const char *tex, const char *opac, byte first, byte last,
	int cellWidth, int cellHeight, float space);
void __thiscall CFontGrid_Print(CFontGrid *self, float x, float y, const char *s);
CDigits *__fastcall CDigits_ctor(CDigits *self);
void __fastcall CDigits_dtor(CDigits *self);

/* the font of tex (opac its opacity map), glyphs first..last. generate: no sprite of its own, the glyphs packed into a
 * new alpha texture instead; unused not read */
CFont3 *__thiscall CFont3_ctor(CFont3 *self, const char *tex, const char *opac, byte first, byte last, float width,
	byte generate, uint32_t flags, float unused);
void __fastcall CFont3_dtor(CFont3 *self);
/* the size of s (dim) and the position of its top left corner for the corner at x, y (pos, by align) */
void __thiscall CFont3_Measure(const CFont3 *self, float x, float y, uint16_t align, const char *s, float *pos,
	float *dim);
float __thiscall CFont3_GlyphWidth(const CFont3 *self, const FntGlyph *g);
float __thiscall CFont3_GlyphHeight(const CFont3 *self, const FntGlyph *g);
/* the scale of the glyphs */
void __thiscall CFont3_SetScale(CFont3 *self, float x, float y);
uint32_t __thiscall Font_SetColor(CFont3 *self, uint32_t color);	/* returns 2 */
uint32_t __thiscall Font_SetFlags(CFont3 *self, uint32_t flags);	/* returns 8 */
/* s at x, y (top left) in the color of the sprite; x, y become the end */
void __thiscall CFont3_Print(CFont3 *self, float x, float y, const char *s);
void __thiscall CFont3_PrintColor(CFont3 *self, float x, float y, uint32_t color, const char *s);
/* flags: the alignment, FNT_WAVE, FNT_OUTLINE */
void __thiscall CFont3_PrintAlign(CFont3 *self, float x, float y, uint32_t flags, uint32_t color, const char *s);
void __thiscall CFont3_PrintWave(CFont3 *self, float x, float y, uint32_t color, float wave, const char *s);
/* a black outline (a pixel to each corner), the text, then a glow fading out to 3 pixels in its color */
void __thiscall CFont3_PrintOutline(CFont3 *self, float x, float y, uint32_t color, const char *s);
void Font_PrintfXY(CFont3 *font, float x, float y, const char *fmt, ...);
/* flags FNT_PRINT_* */
void Font_Printf(CFont3 *font, float x, float y, byte flags, const char *fmt, ...);
/* centered at x, y with a black shadow */
void Font_PrintfCenter(CFont3 *font, float x, float y, const char *fmt, ...);
/* a text with a black shadow at the screen position of pos moved by x, y (2D screen pixels) */
void Debug_Text3D(CFont3 *font, float x, float y, const vec3_t *pos, uint32_t color, const char *fmt, ...);
/* the pixels from the left of row y in the color of the border (the alpha not compared) */
int Bmp_RowBorder(const CBmp *b, int y, uint32_t border);
void Fonts_Init(void);
void Fonts_Free(void);

void __fastcall CUnifont_Load(CUnifont *self);
void __fastcall CUnifont_Free(CUnifont *self);
/* the atexit destructor of g_Unifont */
void Unifont_Exit(void);
/* the "\1XXXX" at s: 1 and the sprite in *out (0 out of the range), 0 if s is not one */
byte __thiscall CUnifont_Parse(const CUnifont *self, const char *s, uint16_t *out);
/* the width of sprite i at the height h, 0 if there is none */
float __thiscall CUnifont_Width(const CUnifont *self, uint16_t i, float h);
/* sprite i of size w by h on the sprite of the font, 0 if there is none */
byte __thiscall CUnifont_SetSprite(const CUnifont *self, uint16_t i, float w, float h);

#endif
