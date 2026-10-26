/*
 * ScrapEngFont.cpp of the game: CFont, see include/scrap/scrapfont.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/scorer.h"
#include "scrap/scrapfont.h"

#define CFONT_BUF 1024

CFont *__thiscall CFont_ctor(CFont *self, const char *name, void *unused) {
	self->x = 0.0f;
	self->y = 0.0f;
	self->incx = 0.0f;
	self->i0c = 0;
	self->align = 0;
	self->color = 0xffffff;
	self->f18 = 1.0f;
	self->f1c = 1.0f;
	self->efectos = 0;
	self->Font = NULL;
	self->onda = 1.0f;
	self->sombra[0] = 1.0f;
	self->sombra[1] = 1.0f;
	CFont_Load(self, name, unused, NULL);
	return self;
}

void __fastcall CFont_dtor(CFont *self) {
	if (self->Font) {
		CFuente3_dtor(self->Font);
		operator_delete_sized(self->Font, sizeof(CFuente3));
		self->Font = NULL;
	}
}

void __fastcall CFont_ResetX(CFont *self) {
	self->x = 0.0f;
}

byte __thiscall CFont_Build(CFont *self, const char *name, int size) {
	(void)self;
	CFuente3 *f = operator_new(sizeof(CFuente3));
	if (f) {
		f = CFuente3_ctor(f, name, NULL, 0, 0xff, (float)size, 1, 0, 1.0f);
		if (f) {
			CFuente3_dtor(f);
			operator_delete_sized(f, sizeof(CFuente3));
		}
	}
	return 1;
}

void Font_TextSize(CFont *font, float *pos, float *dim, float x, float y, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	CFont_MedirV(font, pos, dim, x, y, 0, fmt, ap);
	va_end(ap);
}

void Font_TextSizeAlign(CFont *font, float *pos, float *dim, float x, float y, byte flags, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	CFont_MedirV(font, pos, dim, x, y, flags, fmt, ap);
	va_end(ap);
}

void __thiscall CFont_Load(CFont *self, const char *name, void *unused, CArchINI *ini) {
	char tex[260], opac[260], sec[64];
	(void)unused;

	if (self->Font) {
		CheckFailed("Font == NULL", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!name) {
		return;
	}
	memset(tex, 0, sizeof(tex));
	memset(opac, 0, sizeof(opac));
	int first = 0x20, last = 0x7f, bres = 640;
	float incx = 0.0f;
	float esc[2] = { 1.0f, 1.0f }, off[2] = { 0.0f, 0.0f };
	if (!ini) {
		Con_FatalErrorf("CFont: LoadFont error: '%s'", name);
	} else {
		static const float uno[2] = { 1.0f, 1.0f }, cero[2] = { 0.0f, 0.0f };
		float t[2];
		Str_Copy(sec, name, sizeof(sec));
		CArchINI_ReadText(ini, sec, "texture", tex, sizeof(tex));
		CArchINI_ReadText(ini, sec, "opacity", opac, sizeof(opac));
		CArchINI_ReadInt(ini, sec, "first", &first);
		CArchINI_ReadInt(ini, sec, "last", &last);
		CArchINI_ReadFloat(ini, sec, "incx", &incx);
		CArchINI_ReadInt(ini, sec, "bres", &bres);
		const float *v = CArchINI_GetVec2(ini, t, sec, "Unifont_Scale", uno);
		esc[0] = v[0];
		esc[1] = v[1];
		v = CArchINI_GetVec2(ini, t, sec, "Unifont_Offset", cero);
		off[0] = v[0];
		off[1] = v[1];
	}
	if (first < 0 || last > 0xff || last < first) {
		CheckFailed("fnt_first >= 0 && fnt_last <= 255 && fnt_first <= fnt_last", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!tex[0]) {
		CheckFailed("fnt_texture[0]", __FILE__, __TIMESTAMP__, __LINE__);
	}
	CFuente3 *f = operator_new(sizeof(CFuente3));
	self->Font = f ? CFuente3_ctor(f, tex, opac[0] ? opac : NULL, (byte)first, (byte)last, 0.0f, 0, 0, 1.0f) : NULL;
	float k = 640.0f / (float)bres;
	CFuente3_SetEscala(self->Font, k, k);
	self->i0c = 0;
	self->incx = incx;
	self->Font->mono = incx != 0.0f;
	self->Font->uniEscala[0] = esc[0];
	self->Font->uniEscala[1] = esc[1];
	self->Font->uniOffset[0] = off[0];
	self->Font->uniOffset[1] = off[1];
}

void __thiscall CFont_Print(CFont *self, const char *txt) {
	if (!self->Font) {
		CheckFailed("Font", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!txt) {
		CheckFailed("txt", __FILE__, __TIMESTAMP__, __LINE__);
	}
	self->Font->ancho = self->incx;
	CFuente3_Print(self->Font, self->x, self->y, txt);
	self->x = self->Font->x;
	self->y = self->Font->y;
}

void __thiscall CFont_PrintXY(CFont *self, float x, float y, const char *txt) {
	self->x = x;
	self->y = y;
	CFont_Print(self, txt);
}

void __thiscall CFont_PrintfVA(CFont *self, const char *fmt, va_list ap) {
	char buf[CFONT_BUF];

	if (!fmt) {
		CheckFailed("frmt", __FILE__, __TIMESTAMP__, __LINE__);
	}
	Str_VPrintf(buf, sizeof(buf), fmt, ap);
	CFont_Print(self, buf);
}

void CFont_Printf(CFont *font, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	CFont_PrintfVA(font, fmt, ap);
	va_end(ap);
}

void CFont_PrintfXY(CFont *font, float x, float y, const char *fmt, ...) {
	va_list ap;

	font->x = x;
	font->y = y;
	va_start(ap, fmt);
	CFont_PrintfVA(font, fmt, ap);
	va_end(ap);
}

void __thiscall CFont_MedirV(CFont *self, float *pos, float *dim, float x, float y, byte flags, const char *fmt,
	va_list ap) {
	char buf[CFONT_BUF];

	if (!self->Font) {
		CheckFailed("Font", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!fmt) {
		CheckFailed("frmt", __FILE__, __TIMESTAMP__, __LINE__);
	}
	Str_VPrintf(buf, sizeof(buf), fmt, ap);
	self->Font->ancho = self->incx;
	CFuente3_Medir(self->Font, x, y, (uint16_t)(flags & CFONT_ALIGN ? self->align : 0), buf, pos, dim);
}

void __thiscall CFont_PrintfV(CFont *self, float x, float y, uint32_t flags, const char *fmt, va_list ap) {
	char buf[CFONT_BUF];

	if (!self->Font) {
		CheckFailed("Font", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!fmt) {
		CheckFailed("frmt", __FILE__, __TIMESTAMP__, __LINE__);
	}
	Str_VPrintf(buf, sizeof(buf), fmt, ap);
	self->x = x;
	self->y = y;
	self->Font->ancho = self->incx;
	uint32_t al = flags & CFONT_ALIGN ? self->align : 0;
	uint32_t color = flags & CFONT_COLOR ? self->color : 0xffffff;
	if (flags & CFONT_EFECTOS) {
		if (self->efectos & FNT_ONDA) {
			al |= FNT_ONDA;
			self->Font->onda = self->onda;
		}
		if (self->efectos & CFONT_SOMBRA) {
			CFuente3_PrintAlign(self->Font, self->sombra[0] + self->x, self->sombra[1] + self->y, al, 0, buf);
		}
		if (self->efectos & FNT_CONTORNO) {
			al |= FNT_CONTORNO;
		}
	}
	CFuente3_PrintAlign(self->Font, self->x, self->y, al, color, buf);
	self->Font->onda = 1.0f;
	self->x = self->Font->x;
	self->y = self->Font->y;
}

void CFont_PrintfFlags(CFont *font, float x, float y, uint32_t flags, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	CFont_PrintfV(font, x, y, flags, fmt, ap);
	va_end(ap);
}

void CFont_PrintfAlign(CFont *font, float x, float y, uint32_t align, const char *fmt, ...) {
	va_list ap;

	font->align = align;
	va_start(ap, fmt);
	CFont_PrintfV(font, x, y, CFONT_ALIGN, fmt, ap);
	va_end(ap);
}

void CFont_PrintfAlignColor(CFont *font, float x, float y, uint32_t align, uint32_t color, const char *fmt, ...) {
	va_list ap;

	font->align = align;
	font->color = color;
	va_start(ap, fmt);
	CFont_PrintfV(font, x, y, CFONT_ALIGN | CFONT_COLOR, fmt, ap);
	va_end(ap);
}

void CFont_PrintfAlignColorSombra(CFont *font, float x, float y, uint32_t align, uint32_t color, const char *fmt,
	...) {
	va_list ap;

	font->align = align;
	font->color = color;
	font->efectos = CFONT_SOMBRA;
	va_start(ap, fmt);
	CFont_PrintfV(font, x, y, CFONT_ALIGN | CFONT_COLOR | CFONT_EFECTOS, fmt, ap);
	va_end(ap);
}

void CFont_PrintfColor(CFont *font, float x, float y, uint32_t color, const char *fmt, ...) {
	va_list ap;

	font->color = color;
	va_start(ap, fmt);
	CFont_PrintfV(font, x, y, CFONT_COLOR, fmt, ap);
	va_end(ap);
}

void CFont_PrintfColorSombra(CFont *font, float x, float y, uint32_t color, const char *fmt, ...) {
	va_list ap;

	font->color = color;
	font->efectos = CFONT_SOMBRA;
	va_start(ap, fmt);
	CFont_PrintfV(font, x, y, CFONT_COLOR | CFONT_EFECTOS, fmt, ap);
	va_end(ap);
}

void CFont_PrintfSombra(CFont *font, float x, float y, const char *fmt, ...) {
	va_list ap;

	font->efectos = CFONT_SOMBRA;
	va_start(ap, fmt);
	CFont_PrintfV(font, x, y, CFONT_EFECTOS, fmt, ap);
	va_end(ap);
}

void CFont_Printf3D(CFont *font, const vec3_t *pos, uint32_t flags, const char *fmt, ...) {
	float scr[2];
	va_list ap;

	if (!Engine_WorldToScreen(pos, scr, 0)) {
		return;
	}
	va_start(ap, fmt);
	CFont_PrintfV(font, scr[0], scr[1], flags, fmt, ap);
	va_end(ap);
}

void __fastcall CFont_ResetEfectos(CFont *self) {
	self->onda = 1.0f;
	self->sombra[0] = 1.0f;
	self->sombra[1] = 1.0f;
}
