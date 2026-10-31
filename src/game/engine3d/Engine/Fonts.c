/*
 * Engine/fuentes.cpp of the engine3d: the bitmap fonts, see include/scrap/fonts.h.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/filepak.h"
#include "scrap/fonts.h"
#include "scrap/render.h"
#include "scrap/misc.h"

CFontGrid *__thiscall CFontGrid_ctor(CFontGrid *self, const char *tex, const char *opac, byte first, byte last,
	int cellWidth, int cellHeight, float space) {
	CGraphic2D_ctor(&self->graphic);
	CGraphic2D_SetTexture(&self->graphic, tex, opac, 0);
	self->graphic.screen = 1;
	self->graphic.screenSize = 1;
	self->i54 = 0;
	self->i50 = 0;
	CHECK(first <= last);
	self->cellWidth = cellWidth;
	self->cellHeight = cellHeight;
	self->first = first;
	self->last = last;
	self->space = space;
	return self;
}

void __thiscall CFontGrid_Print(CFontGrid *self, float x, float y, const char *s) {
	int perRow = 0x100 / self->cellWidth;
	float cw = (float)self->cellWidth * (1.0f / 256.0f);
	float ch = (float)self->cellHeight * (1.0f / 256.0f);
	for (const byte *c = (const byte *)s; *c; c++) {
		if (*c >= self->first && *c <= self->last) {
			int i = *c - self->first;
			self->graphic.v0 = (float)(i / perRow) * ch;
			self->graphic.v1 = self->graphic.v0 + ch;
			self->graphic.u0 = (float)(i % perRow) * cw;
			self->graphic.u1 = self->graphic.u0 + cw;
			self->graphic.x = x;
			self->graphic.y = y;
			CGraphic2D_Render(&self->graphic);
		}
		x = self->space + x;
	}
}

CDigits *__fastcall CDigits_ctor(CDigits *self) {
	char tex[128], opac[128];

	for (int i = 0; i < 10; i++) {
		CGraphic2D_ctor(&self->digit[i]);
	}
	for (int i = 0; i < 10; i++) {
		snprintf(tex, sizeof(tex), "bmp/digitos/digito%04d.tga", i + 1);
		snprintf(opac, sizeof(opac), "bmp/digitos/digito%04do.tga", i + 1);
		CGraphic2D_SetTexture(&self->digit[i], tex, opac, 0);
		self->digit[i].screen = 1;
		self->digit[i].screenSize = 1;
	}
	self->i2ac = 0;
	self->i2a8 = 0;
	self->f2b0 = 24.0f;
	return self;
}

void __fastcall CDigits_dtor(CDigits *self) {
	for (int i = 9; i >= 0; i--) {
		CGraphic2D_dtor(&self->digit[i]);
	}
}

static const FntGlyph *CFont3_Glyph(const CFont3 *self, uint32_t i) {
	TABLE_CHECK(self->glyphs.p, i, self->glyphs.n);
	return (const FntGlyph *)self->glyphs.p + i;
}

static void CFont3_SetGlyphRect(CFont3 *self, byte letter, float u0, float v0, float u1, float v1) {
	FntGlyph *g = (FntGlyph *)CFont3_Glyph(self, (uint32_t)(letter - self->first));
	g->u0 = u0;
	g->v0 = v0;
	g->u1 = u1;
	g->v1 = v1;
}

static byte Pixel_IsBorder(const CBmp *b, int x, int y, uint32_t border) {
	return ((*(const uint32_t *)Img_Pixel(b, (uint32_t)x, (uint32_t)y, 4) ^ border) & 0xffffff) == 0;
}

/* the glyphs between the border color (the pixel 0, 0): rows of glyphs between border rows, in each row the glyphs
 * between border pixels; the last letter found */
static byte CFont3_Scan(CFont3 *self, const CBmp *bmp, byte last) {
	byte letter = self->first;
	int w = self->bmpWidth, h = self->bmpHeight;
	uint32_t border = *CBmp_Pixel(bmp, 0, 0);

	for (int y = 0; y < h;) {
		CHECK(Bmp_RowBorder(bmp, y, border) == w);
		while (y < h && Bmp_RowBorder(bmp, y, border) == w) {
			y++;
		}
		int ytop = y;
		if (ytop >= h) {
			break;
		}
		do {
			if (Bmp_RowBorder(bmp, y, border) == w) {
				break;
			}
			y++;
		} while (y < h);
		int ybot = y;
		if (ybot >= h) {
			break;
		}
		for (int x = 0; x < w; x++) {
			if (Pixel_IsBorder(bmp, x, ytop, border)) {
				continue;
			}
			int x1 = x;
			while (x < w && !Pixel_IsBorder(bmp, x, ytop, border)) {
				x++;
			}
			CFont3_SetGlyphRect(self, letter, (float)x1 / (float)w, (float)ytop / (float)h, (float)x / (float)w,
				(float)ybot / (float)h);
			if (letter == last) {
				return letter;
			}
			letter++;
		}
		if (letter == last) {
			break;
		}
	}
	return letter;
}

/* the glyphs packed into a square alpha texture of the size width (256 below 4), written as &u&d&n.Alpha.TGA */
static void CFont3_CreateAlpha(CFont3 *self, CBmp *bmp, char *tmp, byte last, float width) {
	int size = (int)(4.0f > width ? 256.0f : width);
	CBmp alpha = { size, size, operator_new_array((uint32_t)size * (uint32_t)size * 4) };

	CBmp_Fill(&alpha, 0xf5f5f5);
	for (int y = 0; y < self->bmpHeight; y++) {
		uint32_t *row = Img_Pixel(bmp, 0, (uint32_t)y, 4);
		for (int x = 0; x < self->bmpWidth; x++) {
			row[x] = (uint32_t)((const byte *)&row[x])[1] << 24 | 0xffffff;
		}
	}
	int px = 1, py = 1;
	for (byte c = self->first; c <= last; c++) {
		const FntGlyph *g = CFont3_Glyph(self, (uint32_t)(c - self->first));
		int x1 = (int)((double)((float)bmp->width * g->u0) + 0.5);
		int y1 = (int)((double)((float)bmp->height * g->v0) + 0.5);
		int x2 = (int)((double)((float)bmp->width * g->u1) + 0.5);
		int y2 = (int)((double)((float)bmp->height * g->v1) + 0.5);
		CHECK(x1 < x2);
		CHECK(y1 < y2);
		int cw = x2 - x1;
		if (px + cw > alpha.width) {
			px = 1;
			py = py + 1 + (y2 - y1);
			if ((y2 - y1) + py > alpha.height) {
				Err_Printf(1, "'%s' no cabe en %dx%d (letra: %d)", tmp, alpha.width, alpha.height, c);
			}
		}
		CBmp_Copy(&alpha, px, py, bmp, x1, y1, cw, y2 - y1, 0xf);
		px = px + 1 + cw;
		if (c == 0xff) {
			break;
		}
	}
	Path_Format(tmp, tmp, "&u&d&n.Alpha.TGA");
	if (CBmp_Save(&alpha, tmp, 2)) {
		CheckFailed("!bmp2.escr_archivo( tmp, BITMAP_SAVEALPHA )", __FILE__, __TIMESTAMP__, __LINE__);
	}
	Err_Printf(0, "creado: %s", tmp);
	alpha.width = 0;
	alpha.height = 0;
	if (alpha.img) {
		operator_delete_sized(alpha.img, 4);
	}
}

/* the sprite table of the glyphs (sprite i + 1 is glyph first + i) */
static void CFont3_TableMST(const CFont3 *self, CMultiSprite *ms, byte last) {
	ms->width = self->bmpWidth;
	ms->height = self->bmpHeight;
	Table_Alloc8((CTable *)&ms->n, last - self->first + 2);
	for (int i = 1; i <= last - self->first + 1; i++) {
		const FntGlyph *g = CFont3_Glyph(self, (uint32_t)(i - 1));
		float du = g->u1 - g->u0, dv = g->v1 - g->v0;
		MSprite *s = &ms->p[i];
		s->x = (uint16_t)(int)((float)ms->width * g->u0);
		s->y = (uint16_t)(int)((float)ms->height * g->v0);
		s->cx = (uint16_t)(int)((float)ms->width * du);
		s->cy = (uint16_t)(int)((float)ms->height * dv);
	}
}

CFont3 *__thiscall CFont3_ctor(CFont3 *self, const char *tex, const char *opac, byte first, byte last, float width,
	byte generate, uint32_t flags, float unused) {
	char tmp[260], mst[260];
	CMultiSprite ms;

	(void)unused;
	self->glyphs.p = NULL;
	self->glyphs.n = 0;
	if (generate) {
		self->graphic = NULL;
	} else {
		CGraphic2D *g = operator_new(sizeof(CGraphic2D));
		self->graphic = g ? CGraphic2D_ctorTex(g, tex, opac, flags | GRAPHIC2D_SCREEN | GRAPHIC2D_SCREEN_SIZE) : NULL;
	}
	self->y = 0.0f;
	self->x = 0.0f;
	self->wave = 1.0f;
	self->color = 0xffffff;
	self->flags = 0;
	self->i18 = 0;
	self->round = 0;
	self->mono = 0;
	CHECK(first <= last);
	self->last = last;
	self->first = first;
	self->width = width;
	Table_Alloc10(&self->glyphs, last - first + 1);
	Path_Copy(tmp, tex, sizeof(tmp));
	Path_Format(mst, tmp, "&u&d&n.MST");
	CMultiSprite_ctor(&ms);
	if (File_TimeDiff(mst, tex, 0) < 0) {
		E3D_LogBlue(g_FileLog, "WARN: [%s] is older than [%s]", mst, tex);
	} else if (CMultiSprite_Load(&ms, mst)) {
		CHECK(last - first + 2 == ms.n);
		self->bmpWidth = ms.width;
		self->bmpHeight = ms.height;
		for (int i = 0; i <= last - first; i++) {
			float t0[2], t1[2];
			CMultiSprite_GetUV(&ms, (uint32_t)(i + 1), t0, t1);
			CFont3_SetGlyphRect(self, (byte)(first + i), t0[0], t0[1], t1[0], t1[1]);
		}
		CMultiSprite_dtor(&ms);
		return self;
	}
	CBmp bmp = { 0, 0, NULL };
	if (CBmp_Load(&bmp, tmp, 0)) {
		Err_Report(*crt_errno(), 1, "\n'%s'", tmp);
	}
	self->bmpWidth = bmp.width;
	self->bmpHeight = bmp.height;
	byte letter = CFont3_Scan(self, &bmp, last);
	if (!generate) {
		CHECK(letter == last);
	} else {
		if (letter || last) {
			if (!letter) {
				CheckFailed("letra!=0 || ult==0", __FILE__, __TIMESTAMP__, __LINE__);
			}
			if (last != letter) {
				last = (byte)(letter - 1);
			}
		}
		CFont3_CreateAlpha(self, &bmp, tmp, last, width);
	}
	CFont3_TableMST(self, &ms, last);
	if (CMultiSprite_Save(&ms, mst)) {
		Con_Verbosef("CFuente3: SaveMultiSpriteTable: '%s'\n", mst);
	} else {
		Con_Bluef("CFuente3: SaveMultiSpriteTable: Could not write to file: '%s'\n", mst);
	}
	Img_Free(&bmp, 4);
	CMultiSprite_dtor(&ms);
	return self;
}

void __fastcall CFont3_dtor(CFont3 *self) {
	if (self->graphic) {
		CGraphic2D_dtor(self->graphic);
		operator_delete_sized(self->graphic, sizeof(CGraphic2D));
		self->graphic = NULL;
	}
	Table_Free(&self->glyphs);
}

float __thiscall CFont3_GlyphWidth(const CFont3 *self, const FntGlyph *g) {
	if (self->mono) {
		return self->graphic->scale2[0] * self->width;
	}
	return (g->u1 - g->u0) * (float)self->bmpWidth * self->graphic->scale2[0];
}

float __thiscall CFont3_GlyphHeight(const CFont3 *self, const FntGlyph *g) {
	return (g->v1 - g->v0) * (float)self->bmpHeight * self->graphic->scale2[1];
}

/* the height of the unicode characters: the glyph first */
static float CFont3_UniHeight(const CFont3 *self) {
	return CFont3_GlyphHeight(self, CFont3_Glyph(self, self->first)) * self->uniScale[1];
}

void __thiscall CFont3_Measure(const CFont3 *self, float x, float y, uint16_t align, const char *s, float *pos,
	float *dim) {
	float w = 0.0f, h = 0.0f;

	CHECK(pos || dim);
	if (align || dim) {
		for (const byte *c = (const byte *)s; *c; c++) {
			float gh;
			if (*c < self->first || *c > self->last) {
				uint16_t u;
				if (!CUnifont_Parse(&g_Unifont, (const char *)c, &u)) {
					continue;
				}
				c += 4;
				gh = CFont3_UniHeight(self);
				w += CUnifont_Width(&g_Unifont, u, gh) * self->uniScale[0] + 1.0f;
			} else {
				const FntGlyph *g = CFont3_Glyph(self, (uint32_t)(*c - self->first));
				w += CFont3_GlyphWidth(self, g);
				gh = CFont3_GlyphHeight(self, g);
			}
			if (!(h > gh)) {
				h = gh;
			}
		}
		if (dim) {
			dim[0] = w;
			dim[1] = h;
		}
	}
	if (!pos) {
		return;
	}
	switch (align) {
	case FNT_ALIGN_RIGHT:
		h = 0.0f;
		break;
	case FNT_ALIGN_BOTTOM:
		w = 0.0f;
		break;
	case FNT_ALIGN_RIGHT_BOTTOM:
		break;
	case FNT_ALIGN_CENTER:
		w *= 0.5f;
		h *= 0.5f;
		break;
	default:
		w = 0.0f;
		h = 0.0f;
	}
	pos[0] = x - w;
	pos[1] = y - h;
}

void __thiscall CFont3_SetScale(CFont3 *self, float x, float y) {
	CHECK(self->graphic);
	self->graphic->scale2[0] = x;
	self->graphic->scale2[1] = y;
}

uint32_t __thiscall Font_SetColor(CFont3 *self, uint32_t color) {
	self->color = color;
	return 2;
}

uint32_t __thiscall Font_SetFlags(CFont3 *self, uint32_t flags) {
	self->flags = flags;
	return 8;
}

/* the control characters of a font with them are drawn white (keeping the alpha) */
static byte CFont3_Control(const CFont3 *self, byte c) {
	return self->first < 0x20 && c < 0x20 && self->graphic->color;
}

static void CFont3_SetGlyph(CFont3 *self, const FntGlyph *g, float x, float y) {
	CGraphic2D *graphic = self->graphic;
	graphic->u0 = g->u0;
	graphic->v0 = g->v0;
	graphic->u1 = g->u1;
	graphic->v1 = g->v1;
	graphic->x = x;
	graphic->y = y;
}

void __thiscall CFont3_Print(CFont3 *self, float x, float y, const char *s) {
	if (!s) {
		return;
	}
	if (self->round) {
		x = (float)crt_floor((double)x + 0.5);
		y = (float)crt_floor((double)y + 0.5);
	}
	uint32_t color = self->graphic->color;
	for (const byte *c = (const byte *)s; *c; c++) {
		byte ch = *c;
		if (CFont3_Control(self, ch)) {
			self->graphic->color = (self->graphic->color & 0xff000000) | 0xffffff;
		}
		if (ch < self->first || ch > self->last) {
			uint16_t u;
			if (CUnifont_Parse(&g_Unifont, (const char *)c, &u)) {
				float h = CFont3_UniHeight(self);
				float w = CUnifont_Width(&g_Unifont, u, h) * self->uniScale[0];
				c += 4;
				CGraphic2D *ug = g_Unifont.graphic;
				ug->x = self->uniOffset[0] + x;
				ug->y = self->uniOffset[1] + y;
				ug->color = color;
				CUnifont_SetSprite(&g_Unifont, u, w, h);
				CGraphic2D_Render(ug);
				x = w + 1.0f + x;
			}
		} else {
			const FntGlyph *g = CFont3_Glyph(self, (uint32_t)(ch - self->first));
			CFont3_SetGlyph(self, g, x, y);
			CGraphic2D_Render(self->graphic);
			x = CFont3_GlyphWidth(self, g) + x;
		}
		if (CFont3_Control(self, ch)) {
			self->graphic->color = color;
		}
	}
	self->x = x;
	self->y = y;
}

void __thiscall CFont3_PrintColor(CFont3 *self, float x, float y, uint32_t color, const char *s) {
	self->graphic->color = color;
	CFont3_Print(self, x, y, s);
	self->graphic->color = 0xffffff;
}

void __thiscall CFont3_PrintAlign(CFont3 *self, float x, float y, uint32_t flags, uint32_t color, const char *s) {
	float pos[2];

	if (!s) {
		return;
	}
	CFont3_Measure(self, x, y, (uint16_t)flags, s, pos, NULL);
	if (flags & FNT_WAVE) {
		CFont3_PrintWave(self, pos[0], pos[1], color, self->wave, s);
	} else if (flags & FNT_OUTLINE) {
		CFont3_PrintOutline(self, pos[0], pos[1], color, s);
	} else {
		CFont3_PrintColor(self, pos[0], pos[1], color, s);
	}
}

void __thiscall CFont3_PrintWave(CFont3 *self, float x, float y, uint32_t color, float wave, const char *s) {
	if (!s) {
		return;
	}
	CGraphic2D *graphic = self->graphic;
	graphic->color = color;
	for (uint32_t i = 0; s[i]; i++) {
		byte ch = (byte)s[i];
		if (ch < self->first || ch > self->last) {
			continue;
		}
		const FntGlyph *g = CFont3_Glyph(self, (uint32_t)(ch - self->first));
		CFont3_SetGlyph(self, g, x, y);
		if (wave != 1.0f) {
			if (i & 1) {
				graphic->scale[0] = wave;
				graphic->scale[1] = 1.0f / wave;
			} else {
				graphic->scale[0] = 1.0f / wave;
				graphic->scale[1] = wave;
			}
			float w = CFont3_GlyphWidth(self, g);
			graphic->x = graphic->x - (graphic->scale[0] - 1.0f) * w * 0.5f;
			graphic->y = graphic->y - CFont3_GlyphHeight(self, g) * (graphic->scale[1] - 1.0f) * 0.5f;
		}
		CGraphic2D_Render(graphic);
		x = CFont3_GlyphWidth(self, g) + x;
	}
	graphic->scale[0] = 1.0f;
	graphic->scale[1] = 1.0f;
	graphic->color = 0xffffff;
}

/* the outline material of the texture of src: cached in *mat until the device starts again or the texture changes */
static CMaterial *Fnt_MatOutline(CMaterial *src, CMaterial **mat, int *init, const char *name) {
	if (g_EngineInits == *init && *mat) {
		const char *a = CMaterial_GetMapName(src, 0) ? CMaterial_GetMapName(src, 0) : "";
		const char *b = CMaterial_GetMapName(*mat, 0) ? CMaterial_GetMapName(*mat, 0) : "";
		if (!strcmp(b, a)) {
			return *mat;
		}
	}
	*init = g_EngineInits;
	CMaterial m;
	CMaterial_ctor(&m);
	String_Set(&m.Name, name);
	CMaterial_SetMap(&m, 0, CMaterial_GetMapName(src, 0));
	m.params.srcBlend = 5;
	m.params.dstBlend = 6;
	m.params.lighting = 0;
	m.params.transp = 1;
	m.params.zFunc = 8;
	m.params.attrib = 0x40;
	m.params.fog = 0;
	m.params.zWrite = 0;
	*mat = Mat_New(&m, 1);
	CMaterial_dtor(&m);
	return *mat;
}

static byte Unifont_Active(void) {
	return g_Unifont.graphic && g_Unifont.sprites;
}

static void Fnt_SetBlend(CMaterial *mat, CMaterial *uni, int dst) {
	mat->params.srcBlend = 5;
	mat->params.dstBlend = dst;
	if (Unifont_Active()) {
		uni->params.srcBlend = 5;
		uni->params.dstBlend = dst;
	}
}

/* the unicode character at c: its sprite on the unicode sprite, its size; 0 if c is not one */
static byte CFont3_UniGlyph(const CFont3 *self, const char *c, float *w, float *h) {
	uint16_t u;
	if (!CUnifont_Parse(&g_Unifont, c, &u)) {
		return 0;
	}
	*h = CFont3_UniHeight(self);
	*w = CUnifont_Width(&g_Unifont, u, *h) * self->uniScale[0];
	CUnifont_SetSprite(&g_Unifont, u, *w, *h);
	return 1;
}

static void CFont3_UniDraw(const CFont3 *self, float x, float y) {
	CGraphic2D *ug = g_Unifont.graphic;
	ug->x = self->uniOffset[0] + x;
	ug->y = self->uniOffset[1] + y;
	CGraphic2D_Render(ug);
}

static void CFont3_GlyphDraw(CFont3 *self, float x, float y) {
	self->graphic->x = x;
	self->graphic->y = y;
	CGraphic2D_Render(self->graphic);
}

/* the sprite at x, y moved d to each corner */
static void Fnt_Corners(CFont3 *self, byte uni, float x, float y, float d) {
	static const signed char corner[4][2] = { { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } };
	for (int i = 0; i < 4; i++) {
		float px = corner[i][0] < 0 ? x - d : d + x;
		float py = corner[i][1] < 0 ? y - d : d + y;
		if (uni) {
			CFont3_UniDraw(self, px, py);
		} else {
			CFont3_GlyphDraw(self, px, py);
		}
	}
}

/* the glow: the corners at 0.5 to 3 pixels, the alpha of color in 7 steps less 16 per pixel */
static void Fnt_Brightness(CFont3 *self, byte uni, float x, float y, uint32_t color) {
	uint32_t *c = uni ? &g_Unifont.graphic->color : &self->graphic->color;
	float d = 0.5f;
	do {
		int a = (int)(color / 0x7000000) - (int)(d * 16.0f);
		if (a <= 0) {
			break;
		}
		*c = (uint32_t)(a + 6) << 24 | (color & 0xffffff);
		Fnt_Corners(self, uni, x, y, d);
		d += 0.4f;
	} while (3.0f >= d);
}

void __thiscall CFont3_PrintOutline(CFont3 *self, float x, float y, uint32_t color, const char *s) {
	if (!s) {
		return;
	}
	CMaterial *graphicMat = self->graphic->Mat;
	CHECK(graphicMat);
	CMaterial *mat = Fnt_MatOutline(graphicMat, &g_FntMatOutline, &g_FntMatOutlineInit, "Font_FX_Outline");
	CMaterial *uniMat = g_FntMatOutlineUni;
	if (Unifont_Active()) {
		uniMat = Fnt_MatOutline(g_Unifont.graphic->Mat, &g_FntMatOutlineUni, &g_FntMatOutlineUniInit,
			"Font_FX_Outline_Unifont");
	}
	/* the outline */
	Render_ClearForced();
	g_MatFX1 = mat;
	Fnt_SetBlend(mat, uniMat, 6);
	self->graphic->color = 0xdc000000;
	float px = x;
	for (const char *c = s; *c; c++) {
		byte ch = (byte)*c;
		float w, h;
		if (ch >= self->first && ch <= self->last) {
			g_MatFX1 = mat;
			const FntGlyph *g = CFont3_Glyph(self, (uint32_t)(ch - self->first));
			CFont3_SetGlyph(self, g, self->graphic->x, self->graphic->y);
			Fnt_Corners(self, 0, px, y, 1.0f);
			px = CFont3_GlyphWidth(self, g) + px;
		} else if (Unifont_Active()) {
			g_MatFX1 = uniMat;
			if (CFont3_UniGlyph(self, c, &w, &h)) {
				c += 4;
				g_Unifont.graphic->color = self->graphic->color;
				Fnt_Corners(self, 1, px, y, 1.0f);
				px = w + 1.0f + px;
			}
		}
	}
	/* the text */
	Render_ClearForced();
	self->graphic->color = color;
	px = x;
	for (const char *c = s; *c; c++) {
		byte ch = (byte)*c;
		float w, h;
		if (ch >= self->first && ch <= self->last) {
			const FntGlyph *g = CFont3_Glyph(self, (uint32_t)(ch - self->first));
			CFont3_SetGlyph(self, g, px, y);
			CGraphic2D_Render(self->graphic);
			px = CFont3_GlyphWidth(self, g) + px;
		} else if (Unifont_Active()) {
			uint16_t u;
			if (CUnifont_Parse(&g_Unifont, c, &u)) {
				h = CFont3_UniHeight(self);
				w = CUnifont_Width(&g_Unifont, u, h) * self->uniScale[0];
				c += 4;
				CGraphic2D *ug = g_Unifont.graphic;
				ug->x = self->uniOffset[0] + px;
				ug->y = self->uniOffset[1] + y;
				ug->color = self->graphic->color;
				CUnifont_SetSprite(&g_Unifont, u, w, h);
				CGraphic2D_Render(ug);
				px = w + 1.0f + px;
			}
		}
	}
	/* the glow, added */
	Render_ClearForced();
	g_MatFX1 = mat;
	Fnt_SetBlend(mat, uniMat, 2);
	px = x;
	for (const char *c = s; *c; c++) {
		byte ch = (byte)*c;
		float w, h;
		if (ch >= self->first && ch <= self->last) {
			g_MatFX1 = mat;
			const FntGlyph *g = CFont3_Glyph(self, (uint32_t)(ch - self->first));
			CFont3_SetGlyph(self, g, self->graphic->x, self->graphic->y);
			Fnt_Brightness(self, 0, px, y, color);
			px = CFont3_GlyphWidth(self, g) + px;
		} else if (Unifont_Active()) {
			g_MatFX1 = uniMat;
			if (CFont3_UniGlyph(self, c, &w, &h)) {
				c += 4;
				Fnt_Brightness(self, 1, px, y, color);
				px = w + 1.0f + px;
			}
		}
	}
	Render_ClearForced();
	Fnt_SetBlend(mat, uniMat, 6);
	self->graphic->color = 0xffffff;
}

void Font_PrintfXY(CFont3 *font, float x, float y, const char *fmt, ...) {
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	CFont3_Print(font, x, y, buf);
}

/* s with a black shadow 1 right and 2 down */
static void CFont3_PrintShadow(CFont3 *font, float x, float y, uint32_t color, const char *s) {
	CFont3_PrintColor(font, x + 1.0f, y + 2.0f, 0, s);
	CFont3_PrintColor(font, x, y, color, s);
}

void Debug_Text3D(CFont3 *font, float x, float y, const vec3_t *pos, uint32_t color, const char *fmt, ...) {
	char buf[1024];
	float scr[2];
	int w, h;
	va_list ap;

	if (Render_ToScreen(pos, scr)) {
		return;
	}
	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	Graphic2D_GetScreen(&w, &h);
	float sx = (float)w * scr[0] / (float)Render_GetViewportW() + x;
	float sy = (float)h * scr[1] / (float)Render_GetViewportH() + y;
	CFont3_PrintShadow(font, sx, sy, color, buf);
}

void Font_PrintfCenter(CFont3 *font, float x, float y, const char *fmt, ...) {
	char buf[1024];
	float pos[2];
	va_list ap;

	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	CFont3_Measure(font, x + 1.0f, y + 2.0f, FNT_ALIGN_CENTER, buf, pos, NULL);
	CFont3_PrintColor(font, pos[0], pos[1], 0, buf);
	CFont3_Measure(font, x, y, FNT_ALIGN_CENTER, buf, pos, NULL);
	font->graphic->color = 0xffffff;
	CFont3_Print(font, pos[0], pos[1], buf);
	font->graphic->color = 0xffffff;
}

void Font_Printf(CFont3 *font, float x, float y, byte flags, const char *fmt, ...) {
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (flags & FNT_PRINT_SHADOW && font->flags & FNT_SHADOW) {
		CFont3_PrintColor(font, x + 1.0f, y + 2.0f, 0, buf);
	}
	CFont3_PrintColor(font, x, y, flags & FNT_PRINT_COLOR ? font->color : 0xffffff, buf);
}

int Bmp_RowBorder(const CBmp *b, int y, uint32_t border) {
	int x = 0;
	while (x < b->width) {
		uint32_t p = *(const uint32_t *)Img_Pixel(b, (uint32_t)x, (uint32_t)y, 4);
		if ((p & 0xffffff) != (border & 0xffffff)) {
			break;
		}
		x++;
	}
	return x;
}

static void CFont3_Delete(CFont3 *f) {
	CFont3_dtor(f);
	operator_delete_sized(f, sizeof(CFont3));
}

void Fonts_Free(void) {
	if (g_FntComic14) {
		CFont3_Delete(g_FntComic14);
		g_FntComic14 = NULL;
	}
	if (g_FntTerm8) {
		CFont3_Delete(g_FntTerm8);
		g_FntTerm8 = NULL;
	}
}

void Fonts_Init(void) {
	CHECK(!g_FntComic14);
	CFont3 *f = operator_new(sizeof(CFont3));
	f = f ? CFont3_ctor(f, "bmp/fnt/Comic14.tga", "bmp/fnt/A_Comic14.tga", 0x20, 0xff, 0.0f, 0, 0, 1.0f) : NULL;
	g_FntComic14 = f;
	CFont3_SetScale(f, 0.8f, 0.8f);
	CHECK(!g_FntTerm8);
	f = operator_new(sizeof(CFont3));
	g_FntTerm8 = f ? CFont3_ctor(f, "bmp/fnt/Term8.alpha.tga", NULL, 0x20, 0xff, 0.0f, 0, 8, 1.0f) : NULL;
	g_FntTerm8->round = 1;
}

void __fastcall CUnifont_Load(CUnifont *self) {
	cFilePack f;
	char ust[128], tga[128], uct[128];

	cFilePack_ctor(&f);
	snprintf(uct, sizeof(uct), "%sUnicode.alpha.uct", g_FontsDir);
	snprintf(tga, sizeof(tga), "%sUnicode.alpha.tga", g_FontsDir);
	snprintf(ust, sizeof(ust), "%sUnicode.alpha.ust", g_FontsDir);
	if (cFilePack_Exists(&f, uct)) {
		cFilePack_Open(&f, uct, 1);
		cFilePack_Read(&f, &self->first, 2);
		cFilePack_Read(&f, &self->last, 2);
		uint32_t size = ((uint32_t)self->last - self->first) * 2 + 2;
		self->map = Mem_Alloc(size);
		cFilePack_Read(&f, self->map, (int)size);
		cFilePack_Close(&f);
		CGraphic2D *g = operator_new(sizeof(CGraphic2D));
		self->graphic = g ? CGraphic2D_ctorTex(g, tga, NULL, GRAPHIC2D_SCREEN | GRAPHIC2D_SCREEN_SIZE) : NULL;
		self->graphic->noTexture = 1;
		CMultiSprite *ms = operator_new(sizeof(CMultiSprite));
		self->sprites = ms ? CMultiSprite_ctor(ms) : NULL;
		CMultiSprite_Load(self->sprites, ust);
	}
	cFilePack_dtor(&f);
}

void __fastcall CUnifont_Free(CUnifont *self) {
	if (self->sprites) {
		CMultiSprite_dtor(self->sprites);
		operator_delete_sized(self->sprites, sizeof(CMultiSprite));
		self->sprites = NULL;
	}
	if (self->graphic) {
		CGraphic2D_dtor(self->graphic);
		operator_delete_sized(self->graphic, sizeof(CGraphic2D));
		self->graphic = NULL;
	}
	if (self->map) {
		Mem_Free(self->map);
		self->map = NULL;
	}
	self->first = 0;
	self->last = 0;
}

void Unifont_Exit(void) {
	CUnifont_Free(&g_Unifont);
}

byte __thiscall CUnifont_Parse(const CUnifont *self, const char *s, uint16_t *out) {
	*out = 0;
	if (!self->first || !s || s[0] != 1) {
		return 0;
	}
	for (int i = 1; i < 5; i++) {
		signed char c = (signed char)s[i];
		uint16_t v = (uint16_t)(*out * 16);
		*out = v;
		if (c >= '0' && c <= '9') {
			v += c - '0';
		} else if (c >= 'A' && c <= 'F') {
			v += c - 'A' + 10;
		} else if (c >= 'a' && c <= 'f') {
			v += c - 'a' + 10;
		} else {
			return 0;
		}
		*out = v;
	}
	if (*out >= self->first && *out <= self->last) {
		*out = self->map[*out - self->first];
	} else {
		*out = 0;
	}
	return 1;
}

static const MSprite *CUnifont_Sprite(const CUnifont *self, uint16_t i) {
	const CMultiSprite *ms = self->sprites;
	if (ms->n <= i) {
		return NULL;
	}
	if (!ms->p || i >= (uint32_t)ms->n) {
		__builtin_trap();
	}
	return &ms->p[i];
}

float __thiscall CUnifont_Width(const CUnifont *self, uint16_t i, float h) {
	const MSprite *s = CUnifont_Sprite(self, i);
	if (!s) {
		return 0.0f;
	}
	return (float)s->cx * h / (float)s->cy;
}

byte __thiscall CUnifont_SetSprite(const CUnifont *self, uint16_t i, float w, float h) {
	if (!self->first) {
		return 0;
	}
	const MSprite *s = CUnifont_Sprite(self, i);
	if (!s) {
		return 0;
	}
	CGraphic2D *g = self->graphic;
	g->u0 = (float)s->x;
	g->v0 = (float)s->y;
	g->u1 = g->u0 + (float)s->cx;
	g->v1 = g->v0 + (float)s->cy;
	float aw = (float)self->sprites->width, ah = (float)self->sprites->height;
	g->u0 /= aw;
	g->v0 /= ah;
	g->u1 /= aw;
	g->v1 /= ah;
	g->scale[0] = w / (g->u1 - g->u0);
	g->scale[1] = h / (g->v1 - g->v0);
	return 1;
}
