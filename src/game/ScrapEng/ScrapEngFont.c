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
	self->unused_0c = 0;
	self->align = 0;
	self->color = 0xffffff;
	self->f18 = 1.0f;
	self->f1c = 1.0f;
	self->effects = 0;
	self->Font = NULL;
	self->wave = 1.0f;
	self->shadow[0] = 1.0f;
	self->shadow[1] = 1.0f;
	CFont_Load(self, name, unused, NULL);
	return self;
}

void __fastcall CFont_dtor(CFont *self) {
	if (self->Font) {
		CFont3_dtor(self->Font);
		operator_delete_sized(self->Font, sizeof(CFont3));
		self->Font = NULL;
	}
}

void __fastcall CFont_ResetX(CFont *self) {
	self->x = 0.0f;
}

byte __thiscall CFont_Build(CFont *self, const char *name, int size) {
	(void)self;
	CFont3 *f = operator_new(sizeof(CFont3));
	if (f) {
		f = CFont3_ctor(f, name, NULL, 0, 0xff, (float)size, 1, 0, 1.0f);
		if (f) {
			CFont3_dtor(f);
			operator_delete_sized(f, sizeof(CFont3));
		}
	}
	return 1;
}

void Font_TextSize(CFont *font, float *pos, float *dim, float x, float y, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	CFont_MeasureV(font, pos, dim, x, y, 0, fmt, ap);
	va_end(ap);
}

void Font_TextSizeAlign(CFont *font, float *pos, float *dim, float x, float y, byte flags, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	CFont_MeasureV(font, pos, dim, x, y, flags, fmt, ap);
	va_end(ap);
}

void __thiscall CFont_Load(CFont *self, const char *name, void *unused, CIniFile *ini) {
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
	float scale[2] = { 1.0f, 1.0f }, off[2] = { 0.0f, 0.0f };
	if (!ini) {
		Con_FatalErrorf("CFont: LoadFont error: '%s'", name);
	} else {
		static const float one[2] = { 1.0f, 1.0f }, zero[2] = { 0.0f, 0.0f };
		float t[2];
		Str_Copy(sec, name, sizeof(sec));
		CIniFile_ReadText(ini, sec, "texture", tex, sizeof(tex));
		CIniFile_ReadText(ini, sec, "opacity", opac, sizeof(opac));
		CIniFile_ReadInt(ini, sec, "first", &first);
		CIniFile_ReadInt(ini, sec, "last", &last);
		CIniFile_ReadFloat(ini, sec, "incx", &incx);
		CIniFile_ReadInt(ini, sec, "bres", &bres);
		const float *v = CIniFile_GetVec2(ini, t, sec, "Unifont_Scale", one);
		scale[0] = v[0];
		scale[1] = v[1];
		v = CIniFile_GetVec2(ini, t, sec, "Unifont_Offset", zero);
		off[0] = v[0];
		off[1] = v[1];
	}
	if (first < 0 || last > 0xff || last < first) {
		CheckFailed("fnt_first >= 0 && fnt_last <= 255 && fnt_first <= fnt_last", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!tex[0]) {
		CheckFailed("fnt_texture[0]", __FILE__, __TIMESTAMP__, __LINE__);
	}
	CFont3 *f = operator_new(sizeof(CFont3));
	self->Font = f ? CFont3_ctor(f, tex, opac[0] ? opac : NULL, (byte)first, (byte)last, 0.0f, 0, 0, 1.0f) : NULL;
	float k = 640.0f / (float)bres;
	CFont3_SetScale(self->Font, k, k);
	self->unused_0c = 0;
	self->incx = incx;
	self->Font->mono = incx != 0.0f;
	self->Font->uniScale[0] = scale[0];
	self->Font->uniScale[1] = scale[1];
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
	self->Font->width = self->incx;
	CFont3_Print(self->Font, self->x, self->y, txt);
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

void __thiscall CFont_MeasureV(CFont *self, float *pos, float *dim, float x, float y, byte flags, const char *fmt,
	va_list ap) {
	char buf[CFONT_BUF];

	if (!self->Font) {
		CheckFailed("Font", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!fmt) {
		CheckFailed("frmt", __FILE__, __TIMESTAMP__, __LINE__);
	}
	Str_VPrintf(buf, sizeof(buf), fmt, ap);
	self->Font->width = self->incx;
	CFont3_Measure(self->Font, x, y, (uint16_t)(flags & CFONT_ALIGN ? self->align : 0), buf, pos, dim);
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
	self->Font->width = self->incx;
	uint32_t al = flags & CFONT_ALIGN ? self->align : 0;
	uint32_t color = flags & CFONT_COLOR ? self->color : 0xffffff;
	if (flags & CFONT_EFFECTS) {
		if (self->effects & FNT_WAVE) {
			al |= FNT_WAVE;
			self->Font->wave = self->wave;
		}
		if (self->effects & CFONT_SHADOW) {
			CFont3_PrintAlign(self->Font, self->shadow[0] + self->x, self->shadow[1] + self->y, al, 0, buf);
		}
		if (self->effects & FNT_OUTLINE) {
			al |= FNT_OUTLINE;
		}
	}
	CFont3_PrintAlign(self->Font, self->x, self->y, al, color, buf);
	self->Font->wave = 1.0f;
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

void CFont_PrintfAlignColorShadow(CFont *font, float x, float y, uint32_t align, uint32_t color, const char *fmt,
	...) {
	va_list ap;

	font->align = align;
	font->color = color;
	font->effects = CFONT_SHADOW;
	va_start(ap, fmt);
	CFont_PrintfV(font, x, y, CFONT_ALIGN | CFONT_COLOR | CFONT_EFFECTS, fmt, ap);
	va_end(ap);
}

void CFont_PrintfColor(CFont *font, float x, float y, uint32_t color, const char *fmt, ...) {
	va_list ap;

	font->color = color;
	va_start(ap, fmt);
	CFont_PrintfV(font, x, y, CFONT_COLOR, fmt, ap);
	va_end(ap);
}

void CFont_PrintfColorShadow(CFont *font, float x, float y, uint32_t color, const char *fmt, ...) {
	va_list ap;

	font->color = color;
	font->effects = CFONT_SHADOW;
	va_start(ap, fmt);
	CFont_PrintfV(font, x, y, CFONT_COLOR | CFONT_EFFECTS, fmt, ap);
	va_end(ap);
}

void CFont_PrintfShadow(CFont *font, float x, float y, const char *fmt, ...) {
	va_list ap;

	font->effects = CFONT_SHADOW;
	va_start(ap, fmt);
	CFont_PrintfV(font, x, y, CFONT_EFFECTS, fmt, ap);
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

void __fastcall CFont_ResetEffects(CFont *self) {
	self->wave = 1.0f;
	self->shadow[0] = 1.0f;
	self->shadow[1] = 1.0f;
}
