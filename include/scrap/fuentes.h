/*
 * Engine/fuentes.cpp of the engine3d: the bitmap fonts. CFuente3 draws a string glyph by glyph through one CGrafico2D:
 * the glyphs pri..ult are rectangles of its texture found from the borders of the bitmap (or a .MST next to it),
 * advanced by their width or a fixed one. "\1XXXX" in a string (XXXX hex) is a character of the unicode font
 * (g_Unifont, the Unicode.alpha files of g_FontsDir) drawn at the height of the glyph pri. The debug fonts
 * gFntComic14 and gFntTerm8 are made at the start. CFuenteGrid (glyphs in a grid of cells) and CDigitos (ten digit
 * sprites) are not used by the game.
 */
#ifndef SCRAP_FUENTES_H
#define SCRAP_FUENTES_H

#include "scrap/bmp.h"
#include "scrap/grafico2d.h"
#include "scrap/tabla.h"
#include "scrap/textura.h"

/* the rectangle of a glyph in the texture, 0..1 */
typedef struct FntGlifo {
	float u0, v0, u1, v1;
} FntGlifo;
SD_SIZE(FntGlifo, 0x10);

typedef struct CFuente3 {
	float x, y;			/* 0x00 the end of the last string */
	float ancho;			/* 0x08 the advance of a monospaced font */
	float onda;			/* 0x0c 1, the scale of FNT_ONDA */
	uint32_t color;			/* 0x10 Font_SetColor */
	uint32_t flags;			/* 0x14 Font_SetFlags, FNT_SOMBRA */
	int i18;			/* 0x18 */
	byte redondear;			/* 0x1c positions rounded to pixels */
	byte mono;			/* 0x1d monospaced */
	byte pad_1e[2];
	float uniEscala[2];		/* 0x20 of the unicode characters, not set by the constructor */
	float uniOffset[2];		/* 0x28 */
	CGrafico2D *grf;		/* 0x30 owned */
	CTabla glifos;			/* 0x34 FntGlifo of pri..ult */
	byte pri, ult;			/* 0x3c */
	byte pad_3e[2];
	int bmpAncho;			/* 0x40 the size of the texture */
	int bmpAlto;			/* 0x44 */
} CFuente3;
SD_SIZE(CFuente3, 0x48);

/* CFuente3.flags */
#define FNT_SOMBRA 0x20000		/* Font_Printf with FNT_PRINT_SOMBRA draws a shadow */
/* the flags of Font_Printf */
#define FNT_PRINT_COLOR 0x2		/* in the color of the font, else white */
#define FNT_PRINT_SOMBRA 0x8
/* the alignment of CFuente3_Medir (the position is that corner of the string) and the flags of CFuente3_PrintAlign */
enum {
	FNT_ALIGN_NINGUNO = 0,
	FNT_ALIGN_DER = 1,
	FNT_ALIGN_ABAJO = 2,
	FNT_ALIGN_DER_ABAJO = 3,
	FNT_ALIGN_CENTRO = 4,
};
#define FNT_ONDA 0x10000		/* every glyph scaled by onda, the odd ones the other way */
#define FNT_CONTORNO 0x40000		/* with an outline (CFuente3_PrintContorno) */

typedef struct CFuenteGrid {
	CGrafico2D grf;			/* 0x00 */
	byte pri, ult;			/* 0x44 */
	byte pad_46[2];
	int celdaAncho;			/* 0x48 pixels of a 256 wide texture */
	int celdaAlto;			/* 0x4c */
	int i50;			/* 0x50 */
	int i54;			/* 0x54 */
	float espacio;			/* 0x58 the advance */
} CFuenteGrid;
SD_SIZE(CFuenteGrid, 0x5c);

typedef struct CDigitos {
	CGrafico2D digito[10];		/* 0x000 bmp/digitos/digito0001.tga .. 0010 */
	int i2a8;			/* 0x2a8 */
	int i2ac;			/* 0x2ac */
	float f2b0;			/* 0x2b0 24 */
} CDigitos;
SD_SIZE(CDigitos, 0x2b4);

typedef struct CUnifont {
	uint16_t pri, ult;		/* 0x0 the unicode range of mapa */
	uint16_t *mapa;			/* 0x4 Mem_Alloc, the sprite of each character */
	CGrafico2D *grf;		/* 0x8 */
	CMultiSprite *sprites;		/* 0xc */
} CUnifont;
SD_SIZE(CUnifont, 0x10);

#define g_Unifont (*(CUnifont *)Game_Ptr(0x4d6de0))
#define g_FontsDir GAME_DATA(const char *, 0x431b50)	/* "2d/fonts-drm/" */
#define g_FntComic14 GAME_DATA(CFuente3 *, 0x4d6dc8)
#define g_FntTerm8 GAME_DATA(CFuente3 *, 0x4d6dcc)

CFuenteGrid *__thiscall CFuenteGrid_ctor(CFuenteGrid *self, const char *tex, const char *opac, byte pri, byte ult,
	int celdaAncho, int celdaAlto, float espacio);
void __thiscall CFuenteGrid_Print(CFuenteGrid *self, float x, float y, const char *s);
CDigitos *__fastcall CDigitos_ctor(CDigitos *self);
void __fastcall CDigitos_dtor(CDigitos *self);

/* the font of tex (opac its opacity map), glyphs pri..ult; sinGrafico: no sprite of its own; unused not read */
CFuente3 *__thiscall CFuente3_ctor(CFuente3 *self, const char *tex, const char *opac, byte pri, byte ult, float ancho,
	byte sinGrafico, uint32_t flags, float unused);
void __fastcall CFuente3_dtor(CFuente3 *self);
/* the size of s (dim) and the position of its top left corner for the corner at x, y (pos, by align) */
void __thiscall CFuente3_Medir(const CFuente3 *self, float x, float y, uint16_t align, const char *s, float *pos,
	float *dim);
float __thiscall CFuente3_GlifoAncho(const CFuente3 *self, const FntGlifo *g);
float __thiscall CFuente3_GlifoAlto(const CFuente3 *self, const FntGlifo *g);
/* the scale of the glyphs */
void __thiscall CFuente3_SetEscala(CFuente3 *self, float x, float y);
uint32_t __thiscall Font_SetColor(CFuente3 *self, uint32_t color);	/* returns 2 */
uint32_t __thiscall Font_SetFlags(CFuente3 *self, uint32_t flags);	/* returns 8 */
/* s at x, y (top left) in the color of the sprite; x, y become the end */
void __thiscall CFuente3_Print(CFuente3 *self, float x, float y, const char *s);
void __thiscall CFuente3_PrintColor(CFuente3 *self, float x, float y, uint32_t color, const char *s);
/* flags: the alignment, FNT_ONDA, FNT_CONTORNO */
void __thiscall CFuente3_PrintAlign(CFuente3 *self, float x, float y, uint32_t flags, uint32_t color, const char *s);
void __thiscall CFuente3_PrintOnda(CFuente3 *self, float x, float y, uint32_t color, float onda, const char *s);
void __thiscall CFuente3_PrintContorno(CFuente3 *self, float x, float y, uint32_t color, const char *s);
void Font_PrintfXY(CFuente3 *font, float x, float y, const char *fmt, ...);
/* flags FNT_PRINT_* */
void Font_Printf(CFuente3 *font, float x, float y, byte flags, const char *fmt, ...);
/* centered at x, y with a black shadow */
void Font_PrintfCentro(CFuente3 *font, float x, float y, const char *fmt, ...);
/* a text with a black shadow at the screen position of pos moved by x, y (2D screen pixels) */
void Debug_Text3D(CFuente3 *font, float x, float y, const vec3_t *pos, uint32_t color, const char *fmt, ...);
/* the pixels from the left of row y in the color of the border (the alpha not compared) */
int Bmp_FilaBorde(const CBmp *b, int y, uint32_t borde);
void Fonts_Init(void);
void Fonts_Free(void);

void __fastcall CUnifont_Load(CUnifont *self);
void __fastcall CUnifont_Free(CUnifont *self);
/* the atexit destructor of g_Unifont */
void Unifont_Exit(void);
/* the "\1XXXX" at s: 1 and the sprite in *out (0 out of the range), 0 if s is not one */
byte __thiscall CUnifont_Parse(const CUnifont *self, const char *s, uint16_t *out);
/* the width of sprite i at the height h, 0 if there is none */
float __thiscall CUnifont_Ancho(const CUnifont *self, uint16_t i, float h);
/* sprite i of size w by h on the sprite of the font, 0 if there is none */
byte __thiscall CUnifont_SetSprite(const CUnifont *self, uint16_t i, float w, float h);

#endif
