/*
 * Engine/fuentes.cpp of the engine3d: the bitmap fonts, see include/scrap/fuentes.h.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/filepak.h"
#include "scrap/fuentes.h"
#include "scrap/render.h"
#include "scrap/varios.h"

CFuenteGrid *__thiscall CFuenteGrid_ctor(CFuenteGrid *self, const char *tex, const char *opac, byte pri, byte ult,
	int celdaAncho, int celdaAlto, float espacio) {
	CGrafico2D_ctor(&self->grf);
	CGrafico2D_SetTexture(&self->grf, tex, opac, 0);
	self->grf.pantalla = 1;
	self->grf.tamPantalla = 1;
	self->i54 = 0;
	self->i50 = 0;
	CHECK(pri <= ult);
	self->celdaAncho = celdaAncho;
	self->celdaAlto = celdaAlto;
	self->pri = pri;
	self->ult = ult;
	self->espacio = espacio;
	return self;
}

void __thiscall CFuenteGrid_Print(CFuenteGrid *self, float x, float y, const char *s) {
	int porFila = 0x100 / self->celdaAncho;
	float cw = (float)self->celdaAncho * (1.0f / 256.0f);
	float ch = (float)self->celdaAlto * (1.0f / 256.0f);
	for (const byte *c = (const byte *)s; *c; c++) {
		if (*c >= self->pri && *c <= self->ult) {
			int i = *c - self->pri;
			self->grf.v0 = (float)(i / porFila) * ch;
			self->grf.v1 = self->grf.v0 + ch;
			self->grf.u0 = (float)(i % porFila) * cw;
			self->grf.u1 = self->grf.u0 + cw;
			self->grf.x = x;
			self->grf.y = y;
			CGrafico2D_Render(&self->grf);
		}
		x = self->espacio + x;
	}
}

CDigitos *__fastcall CDigitos_ctor(CDigitos *self) {
	char tex[128], opac[128];

	for (int i = 0; i < 10; i++) {
		CGrafico2D_ctor(&self->digito[i]);
	}
	for (int i = 0; i < 10; i++) {
		snprintf(tex, sizeof(tex), "bmp/digitos/digito%04d.tga", i + 1);
		snprintf(opac, sizeof(opac), "bmp/digitos/digito%04do.tga", i + 1);
		CGrafico2D_SetTexture(&self->digito[i], tex, opac, 0);
		self->digito[i].pantalla = 1;
		self->digito[i].tamPantalla = 1;
	}
	self->i2ac = 0;
	self->i2a8 = 0;
	self->f2b0 = 24.0f;
	return self;
}

void __fastcall CDigitos_dtor(CDigitos *self) {
	for (int i = 9; i >= 0; i--) {
		CGrafico2D_dtor(&self->digito[i]);
	}
}

void __fastcall CFuente3_dtor(CFuente3 *self) {
	if (self->grf) {
		CGrafico2D_dtor(self->grf);
		operator_delete_sized(self->grf, sizeof(CGrafico2D));
		self->grf = NULL;
	}
	Tabla_Free(&self->glifos);
}

static const FntGlifo *CFuente3_Glifo(const CFuente3 *self, uint32_t i) {
	TABLA_CHECK(self->glifos.p, i, self->glifos.n);
	return (const FntGlifo *)self->glifos.p + i;
}

float __thiscall CFuente3_GlifoAncho(const CFuente3 *self, const FntGlifo *g) {
	if (self->mono) {
		return self->grf->escala2[0] * self->ancho;
	}
	return (g->u1 - g->u0) * (float)self->bmpAncho * self->grf->escala2[0];
}

float __thiscall CFuente3_GlifoAlto(const CFuente3 *self, const FntGlifo *g) {
	return (g->v1 - g->v0) * (float)self->bmpAlto * self->grf->escala2[1];
}

/* the height of the unicode characters: the glyph pri */
static float CFuente3_UniAlto(const CFuente3 *self) {
	return CFuente3_GlifoAlto(self, CFuente3_Glifo(self, self->pri)) * self->uniEscala[1];
}

void __thiscall CFuente3_Medir(const CFuente3 *self, float x, float y, uint16_t align, const char *s, float *pos,
	float *dim) {
	float w = 0.0f, h = 0.0f;

	CHECK(pos || dim);
	if (align || dim) {
		for (const byte *c = (const byte *)s; *c; c++) {
			float gh;
			if (*c < self->pri || *c > self->ult) {
				uint16_t u;
				if (!CUnifont_Parse(&g_Unifont, (const char *)c, &u)) {
					continue;
				}
				c += 4;
				gh = CFuente3_UniAlto(self);
				w += CUnifont_Ancho(&g_Unifont, u, gh) * self->uniEscala[0] + 1.0f;
			} else {
				const FntGlifo *g = CFuente3_Glifo(self, (uint32_t)(*c - self->pri));
				w += CFuente3_GlifoAncho(self, g);
				gh = CFuente3_GlifoAlto(self, g);
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
	case FNT_ALIGN_DER:
		h = 0.0f;
		break;
	case FNT_ALIGN_ABAJO:
		w = 0.0f;
		break;
	case FNT_ALIGN_DER_ABAJO:
		break;
	case FNT_ALIGN_CENTRO:
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

void __thiscall CFuente3_SetEscala(CFuente3 *self, float x, float y) {
	CHECK(self->grf);
	self->grf->escala2[0] = x;
	self->grf->escala2[1] = y;
}

uint32_t __thiscall Font_SetColor(CFuente3 *self, uint32_t color) {
	self->color = color;
	return 2;
}

uint32_t __thiscall Font_SetFlags(CFuente3 *self, uint32_t flags) {
	self->flags = flags;
	return 8;
}

/* the control characters of a font with them are drawn white (keeping the alpha) */
static byte CFuente3_Control(const CFuente3 *self, byte c) {
	return self->pri < 0x20 && c < 0x20 && self->grf->color;
}

static void CFuente3_SetGlifo(CFuente3 *self, const FntGlifo *g, float x, float y) {
	CGrafico2D *grf = self->grf;
	grf->u0 = g->u0;
	grf->v0 = g->v0;
	grf->u1 = g->u1;
	grf->v1 = g->v1;
	grf->x = x;
	grf->y = y;
}

void __thiscall CFuente3_Print(CFuente3 *self, float x, float y, const char *s) {
	if (!s) {
		return;
	}
	if (self->redondear) {
		x = (float)crt_floor((double)x + 0.5);
		y = (float)crt_floor((double)y + 0.5);
	}
	uint32_t color = self->grf->color;
	for (const byte *c = (const byte *)s; *c; c++) {
		byte ch = *c;
		if (CFuente3_Control(self, ch)) {
			self->grf->color = (self->grf->color & 0xff000000) | 0xffffff;
		}
		if (ch < self->pri || ch > self->ult) {
			uint16_t u;
			if (CUnifont_Parse(&g_Unifont, (const char *)c, &u)) {
				float h = CFuente3_UniAlto(self);
				float w = CUnifont_Ancho(&g_Unifont, u, h) * self->uniEscala[0];
				c += 4;
				CGrafico2D *ug = g_Unifont.grf;
				ug->x = self->uniOffset[0] + x;
				ug->y = self->uniOffset[1] + y;
				ug->color = color;
				CUnifont_SetSprite(&g_Unifont, u, w, h);
				CGrafico2D_Render(ug);
				x = w + 1.0f + x;
			}
		} else {
			const FntGlifo *g = CFuente3_Glifo(self, (uint32_t)(ch - self->pri));
			CFuente3_SetGlifo(self, g, x, y);
			CGrafico2D_Render(self->grf);
			x = CFuente3_GlifoAncho(self, g) + x;
		}
		if (CFuente3_Control(self, ch)) {
			self->grf->color = color;
		}
	}
	self->x = x;
	self->y = y;
}

void __thiscall CFuente3_PrintColor(CFuente3 *self, float x, float y, uint32_t color, const char *s) {
	self->grf->color = color;
	CFuente3_Print(self, x, y, s);
	self->grf->color = 0xffffff;
}

void __thiscall CFuente3_PrintAlign(CFuente3 *self, float x, float y, uint32_t flags, uint32_t color, const char *s) {
	float pos[2];

	if (!s) {
		return;
	}
	CFuente3_Medir(self, x, y, (uint16_t)flags, s, pos, NULL);
	if (flags & FNT_ONDA) {
		CFuente3_PrintOnda(self, pos[0], pos[1], color, self->onda, s);
	} else if (flags & FNT_CONTORNO) {
		CFuente3_PrintContorno(self, pos[0], pos[1], color, s);
	} else {
		CFuente3_PrintColor(self, pos[0], pos[1], color, s);
	}
}

void __thiscall CFuente3_PrintOnda(CFuente3 *self, float x, float y, uint32_t color, float onda, const char *s) {
	if (!s) {
		return;
	}
	CGrafico2D *grf = self->grf;
	grf->color = color;
	for (uint32_t i = 0; s[i]; i++) {
		byte ch = (byte)s[i];
		if (ch < self->pri || ch > self->ult) {
			continue;
		}
		const FntGlifo *g = CFuente3_Glifo(self, (uint32_t)(ch - self->pri));
		CFuente3_SetGlifo(self, g, x, y);
		if (onda != 1.0f) {
			if (i & 1) {
				grf->escala[0] = onda;
				grf->escala[1] = 1.0f / onda;
			} else {
				grf->escala[0] = 1.0f / onda;
				grf->escala[1] = onda;
			}
			float w = CFuente3_GlifoAncho(self, g);
			grf->x = grf->x - (grf->escala[0] - 1.0f) * w * 0.5f;
			grf->y = grf->y - CFuente3_GlifoAlto(self, g) * (grf->escala[1] - 1.0f) * 0.5f;
		}
		CGrafico2D_Render(grf);
		x = CFuente3_GlifoAncho(self, g) + x;
	}
	grf->escala[0] = 1.0f;
	grf->escala[1] = 1.0f;
	grf->color = 0xffffff;
}

void Font_PrintfXY(CFuente3 *font, float x, float y, const char *fmt, ...) {
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	CFuente3_Print(font, x, y, buf);
}

/* s with a black shadow 1 right and 2 down */
static void CFuente3_PrintSombra(CFuente3 *font, float x, float y, uint32_t color, const char *s) {
	CFuente3_PrintColor(font, x + 1.0f, y + 2.0f, 0, s);
	CFuente3_PrintColor(font, x, y, color, s);
}

void Debug_Text3D(CFuente3 *font, float x, float y, const vec3_t *pos, uint32_t color, const char *fmt, ...) {
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
	Grafico2D_GetPantalla(&w, &h);
	float sx = (float)w * scr[0] / (float)Render_GetViewportW() + x;
	float sy = (float)h * scr[1] / (float)Render_GetViewportH() + y;
	CFuente3_PrintSombra(font, sx, sy, color, buf);
}

void Font_PrintfCentro(CFuente3 *font, float x, float y, const char *fmt, ...) {
	char buf[1024];
	float pos[2];
	va_list ap;

	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	CFuente3_Medir(font, x + 1.0f, y + 2.0f, FNT_ALIGN_CENTRO, buf, pos, NULL);
	CFuente3_PrintColor(font, pos[0], pos[1], 0, buf);
	CFuente3_Medir(font, x, y, FNT_ALIGN_CENTRO, buf, pos, NULL);
	font->grf->color = 0xffffff;
	CFuente3_Print(font, pos[0], pos[1], buf);
	font->grf->color = 0xffffff;
}

void Font_Printf(CFuente3 *font, float x, float y, byte flags, const char *fmt, ...) {
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (flags & FNT_PRINT_SOMBRA && font->flags & FNT_SOMBRA) {
		CFuente3_PrintColor(font, x + 1.0f, y + 2.0f, 0, buf);
	}
	CFuente3_PrintColor(font, x, y, flags & FNT_PRINT_COLOR ? font->color : 0xffffff, buf);
}

int Bmp_FilaBorde(const CBmp *b, int y, uint32_t borde) {
	int x = 0;
	while (x < b->ancho) {
		uint32_t p = *(const uint32_t *)Img_Pixel(b, (uint32_t)x, (uint32_t)y, 4);
		if ((p & 0xffffff) != (borde & 0xffffff)) {
			break;
		}
		x++;
	}
	return x;
}

static void CFuente3_Delete(CFuente3 *f) {
	CFuente3_dtor(f);
	operator_delete_sized(f, sizeof(CFuente3));
}

void Fonts_Free(void) {
	if (g_FntComic14) {
		CFuente3_Delete(g_FntComic14);
		g_FntComic14 = NULL;
	}
	if (g_FntTerm8) {
		CFuente3_Delete(g_FntTerm8);
		g_FntTerm8 = NULL;
	}
}

void Fonts_Init(void) {
	CHECK(!g_FntComic14);
	CFuente3 *f = operator_new(sizeof(CFuente3));
	f = f ? CFuente3_ctor(f, "bmp/fnt/Comic14.tga", "bmp/fnt/A_Comic14.tga", 0x20, 0xff, 0.0f, 0, 0, 1.0f) : NULL;
	g_FntComic14 = f;
	CFuente3_SetEscala(f, 0.8f, 0.8f);
	CHECK(!g_FntTerm8);
	f = operator_new(sizeof(CFuente3));
	g_FntTerm8 = f ? CFuente3_ctor(f, "bmp/fnt/Term8.alpha.tga", NULL, 0x20, 0xff, 0.0f, 0, 8, 1.0f) : NULL;
	g_FntTerm8->redondear = 1;
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
		cFilePack_Read(&f, &self->pri, 2);
		cFilePack_Read(&f, &self->ult, 2);
		uint32_t size = ((uint32_t)self->ult - self->pri) * 2 + 2;
		self->mapa = Mem_Alloc(size);
		cFilePack_Read(&f, self->mapa, (int)size);
		cFilePack_Close(&f);
		CGrafico2D *g = operator_new(sizeof(CGrafico2D));
		self->grf = g ? CGrafico2D_ctorTex(g, tga, NULL, GRAF2D_PANTALLA | GRAF2D_TAM_PANTALLA) : NULL;
		self->grf->sinTextura = 1;
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
	if (self->grf) {
		CGrafico2D_dtor(self->grf);
		operator_delete_sized(self->grf, sizeof(CGrafico2D));
		self->grf = NULL;
	}
	if (self->mapa) {
		Mem_Free(self->mapa);
		self->mapa = NULL;
	}
	self->pri = 0;
	self->ult = 0;
}

void Unifont_Exit(void) {
	CUnifont_Free(&g_Unifont);
}

byte __thiscall CUnifont_Parse(const CUnifont *self, const char *s, uint16_t *out) {
	*out = 0;
	if (!self->pri || !s || s[0] != 1) {
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
	if (*out >= self->pri && *out <= self->ult) {
		*out = self->mapa[*out - self->pri];
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

float __thiscall CUnifont_Ancho(const CUnifont *self, uint16_t i, float h) {
	const MSprite *s = CUnifont_Sprite(self, i);
	if (!s) {
		return 0.0f;
	}
	return (float)s->cx * h / (float)s->cy;
}

byte __thiscall CUnifont_SetSprite(const CUnifont *self, uint16_t i, float w, float h) {
	if (!self->pri) {
		return 0;
	}
	const MSprite *s = CUnifont_Sprite(self, i);
	if (!s) {
		return 0;
	}
	CGrafico2D *g = self->grf;
	g->u0 = (float)s->x;
	g->v0 = (float)s->y;
	g->u1 = g->u0 + (float)s->cx;
	g->v1 = g->v0 + (float)s->cy;
	float aw = (float)self->sprites->ancho, ah = (float)self->sprites->alto;
	g->u0 /= aw;
	g->v0 /= ah;
	g->u1 /= aw;
	g->v1 /= ah;
	g->escala[0] = w / (g->u1 - g->u0);
	g->escala[1] = h / (g->v1 - g->v0);
	return 1;
}
