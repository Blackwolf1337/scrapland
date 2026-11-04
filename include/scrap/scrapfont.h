/*
 * ScrapEngFont.cpp and Fonts.cpp of the game: CFont, a font of the game over a CFont3 of the engine with its cursor,
 * alignment, color and effects (wave, shadow, outline), and the seven fonts of 2d/fonts-drm/Fonts.fnt (an ini, a
 * section per font: texture, opacity, first, last, incx for a monospaced advance, bres the screen width the
 * texture is made for, Unifont_Scale and Unifont_Offset).
 */
#ifndef SCRAP_SCRAPFONT_H
#define SCRAP_SCRAPFONT_H

#include <stdarg.h>

#include "scrap/archio.h"
#include "scrap/fonts.h"

typedef struct CFont {
	float x, y;			/* the cursor, the end of the last text */
	float incx;			/* the advance of a monospaced font */
	int unused_0c;
	uint32_t align;			/* FNT_ALIGN_* with CFONT_ALIGN */
	uint32_t color;			/* with CFONT_COLOR, else white */
	float f18, f1c;			/* 1 */
	uint32_t effects;		/* with CFONT_EFFECTS: FNT_WAVE, CFONT_SHADOW, FNT_OUTLINE */
	float wave;			/* 1 */
	float shadow[2];		/* 1, the offset of the shadow */
	CFont3 *Font;
} CFont;

/* the flags of CFont_PrintfV */
#define CFONT_ALIGN 0x1
#define CFONT_COLOR 0x2
#define CFONT_EFFECTS 0x8
/* CFont.effects */
#define CFONT_SHADOW 0x20000		/* a black copy at shadow first */

/* the fonts by name (Font_Get) */
extern CFont *g_FontTerminal8;
extern CFont *g_FontBigNumbers;
extern CFont *g_FontScrapBig;
extern CFont *g_FontScrapMedium;
extern CFont *g_FontHoratio;
extern CFont *g_FontHoratioSmall;

/* CFont_Load(name, unused, NULL): only a NULL name works */
CFont *CFont_ctor(CFont *self, const char *name, void *unused);
void CFont_dtor(CFont *self);
/* the font name of ini (none for NULL) */
void CFont_Load(CFont *self, const char *name, void *unused, CIniFile *ini);
/* the CFont3 of the texture name of the height size (generating mode) made and deleted again; self not used */
byte CFont_Build(CFont *self, const char *name, int size);
void Font_TextSize(CFont *font, float *pos, float *dim, float x, float y, const char *fmt, ...);
void Font_TextSizeAlign(CFont *font, float *pos, float *dim, float x, float y, byte flags, const char *fmt, ...);
void CFont_PrintfXY(CFont *font, float x, float y, const char *fmt, ...);
void CFont_PrintfFlags(CFont *font, float x, float y, uint32_t flags, const char *fmt, ...);
void CFont_PrintfAlign(CFont *font, float x, float y, uint32_t align, const char *fmt, ...);
void CFont_PrintfAlignColor(CFont *font, float x, float y, uint32_t align, uint32_t color, const char *fmt, ...);
void CFont_PrintfAlignColorShadow(CFont *font, float x, float y, uint32_t align, uint32_t color, const char *fmt,
	...);
void CFont_PrintfColor(CFont *font, float x, float y, uint32_t color, const char *fmt, ...);
void CFont_PrintfColorShadow(CFont *font, float x, float y, uint32_t color, const char *fmt, ...);
void CFont_PrintfShadow(CFont *font, float x, float y, const char *fmt, ...);
/* at the screen position of a world point, nothing off the screen */
void CFont_Printf3D(CFont *font, const vec3_t *pos, uint32_t flags, const char *fmt, ...);
void CFont_ResetEffects(CFont *self);

/* Fonts.cpp */
void Fonts_Load(void);
void Fonts_Unload(void);
CFont *Font_Get(const char *name);
/* builds the alpha texture of the font texture name for the height size (the Python Build function) */
byte Font_Build(const char *name, int size);
byte Fonts_Unifont(void);

#endif
