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
	float x, y;			/* 0x00 the cursor, the end of the last text */
	float incx;			/* 0x08 the advance of a monospaced font */
	int unused_0c;			/* 0x0c */
	uint32_t align;			/* 0x10 FNT_ALIGN_* with CFONT_ALIGN */
	uint32_t color;			/* 0x14 with CFONT_COLOR, else white */
	float f18, f1c;			/* 0x18 1 */
	uint32_t effects;		/* 0x20 with CFONT_EFFECTS: FNT_WAVE, CFONT_SHADOW, FNT_OUTLINE */
	float wave;			/* 0x24 1 */
	float shadow[2];		/* 0x28 1, the offset of the shadow */
	CFont3 *Font;			/* 0x30 */
} CFont;
SD_SIZE(CFont, 0x34);

/* the flags of CFont_PrintfV */
#define CFONT_ALIGN 0x1
#define CFONT_COLOR 0x2
#define CFONT_EFFECTS 0x8
/* CFont.effects */
#define CFONT_SHADOW 0x20000		/* a black copy at shadow first */

/* the fonts by name (Font_Get) */
#define g_FontTerminal8 GAME_GLOBAL(CFont *, g_FontTerminal8, 0x4a8908)
#define g_FontTerminal8Fixed GAME_GLOBAL(CFont *, g_FontTerminal8Fixed, 0x4a890c)
#define g_FontBigNumbers GAME_GLOBAL(CFont *, g_FontBigNumbers, 0x4a8910)
#define g_FontScrapBig GAME_GLOBAL(CFont *, g_FontScrapBig, 0x4a8914)
#define g_FontScrapMedium GAME_GLOBAL(CFont *, g_FontScrapMedium, 0x4a8918)
#define g_FontHoratio GAME_GLOBAL(CFont *, g_FontHoratio, 0x4a891c)
#define g_FontHoratioSmall GAME_GLOBAL(CFont *, g_FontHoratioSmall, 0x4a8920)

/* CFont_Load(name, unused, NULL): only a NULL name works */
CFont *__thiscall CFont_ctor(CFont *self, const char *name, void *unused);
void __fastcall CFont_dtor(CFont *self);
void __fastcall CFont_ResetX(CFont *self);
/* the font name of ini (none for NULL) */
void __thiscall CFont_Load(CFont *self, const char *name, void *unused, CIniFile *ini);
/* the CFont3 of the texture name of the height size (generating mode) made and deleted again; self not used */
byte __thiscall CFont_Build(CFont *self, const char *name, int size);
/* the size and the aligned position of a printf text (CFont3_Measure) */
void __thiscall CFont_MeasureV(CFont *self, float *pos, float *dim, float x, float y, byte flags, const char *fmt,
	va_list ap);
void Font_TextSize(CFont *font, float *pos, float *dim, float x, float y, const char *fmt, ...);
void Font_TextSizeAlign(CFont *font, float *pos, float *dim, float x, float y, byte flags, const char *fmt, ...);
/* txt at the cursor, the cursor after it */
void __thiscall CFont_Print(CFont *self, const char *txt);
void __thiscall CFont_PrintXY(CFont *self, float x, float y, const char *txt);
void CFont_Printf(CFont *font, const char *fmt, ...);
void __thiscall CFont_PrintfVA(CFont *self, const char *fmt, va_list ap);
void CFont_PrintfXY(CFont *font, float x, float y, const char *fmt, ...);
/* flags CFONT_*, the cursor at the end after */
void __thiscall CFont_PrintfV(CFont *self, float x, float y, uint32_t flags, const char *fmt, va_list ap);
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
void __fastcall CFont_ResetEffects(CFont *self);

/* Fonts.cpp */
void Fonts_Load(void);
void Fonts_Unload(void);
CFont *Font_Get(const char *name);
/* builds the alpha texture of the font texture name for the height size (the Python Build function) */
byte Font_Build(const char *name, int size);
byte Fonts_Unifont(void);

#endif
