/*
 * The effects of animated maps of the engine3d, original source Engine/fxtextura.cpp (include/scrap/fxtextura.h).
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/flare.h"
#include "scrap/fxtextura.h"
#include "scrap/rendertarget.h"
#include "scrap/varios.h"

#define g_TimeMsInt GAME_DATA(int, 0x4a8ee0)	/* the engine time in ms as an int */
#define FXTEX_MASK (FXTEX_SIZE - 1)

static float Float_FromBits(uint32_t bits) {
	float f;

	memcpy(&f, &bits, sizeof(f));
	return f;
}

/* the waves of the lava: angle steps per pixel and per second, the amplitude in pixels */
#define LAVA_STEP Float_FromBits(0x3d48b439)		/* 0.049 */
#define LAVA_ANGLE Float_FromBits(0x4222f983)		/* 256 / (2 pi) */
#define LAVA_SPEEDY Float_FromBits(0xbf3ae148)		/* -0.73 */
#define LAVA_AMPL 4.0f
#define LAVA_SCROLL 7.0f
#define LAVA_COS_STEP Float_FromBits(0x3cc90fdb)	/* 2 pi / 256 */

void __fastcall IFXTextura_dtor(IFXTextura *self) {
	self->vftable = Game_Ptr(IFXTEXTURA_VFTABLE);
}

IFXTextura *__thiscall IFXTextura_Delete(IFXTextura *self, byte flags) {
	self->vftable = Game_Ptr(IFXTEXTURA_VFTABLE);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void *__fastcall IFXTextura_GetTexture(IFXTextura *self) {
	(void)self;
	return NULL;
}

/* the file and a copy of it, both 128x128 */
static void FXTex_LoadBmps(CBmp **bmp, CBmp **orig, const char *file) {
	CBmp *b = operator_new(sizeof(CBmp));
	if (b) {
		b->img = NULL;
		CBmp_Load(b, file, 0);
	}
	*bmp = b;
	CBmp *o = operator_new(sizeof(CBmp));
	if (o) {
		o->img = NULL;
		CBmp_Resample(o, *bmp, 0xf);
	}
	*orig = o;
}

static void FXTex_FreeBmp(CBmp **bmp) {
	if (*bmp) {
		Img_Free(*bmp, 4);
		operator_delete_sized(*bmp, sizeof(CBmp));
		*bmp = NULL;
	}
}

CFXTexLava *__thiscall CFXTexLava_ctor(CFXTexLava *self, const char *file) {
	self->base.vftable = Game_Ptr(CFXTEXLAVA_VFTABLE);
	self->lastTime = 0;
	FXTex_LoadBmps(&self->bmp, &self->orig, file);
	CHECK(self->bmp->ancho == FXTEX_SIZE && self->bmp->alto == FXTEX_SIZE);
	if (!g_LavaCosInic) {
		g_LavaCosInic = 1;
		for (int i = 0; i < 256; i += 2) {
			double in[2] = { (double)((float)i * LAVA_COS_STEP), (double)((float)(i + 1) * LAVA_COS_STEP) };
			double out[2];
			Game_CosPd(in, out);
			g_LavaCos[i] = (float)out[0];
			g_LavaCos[i + 1] = (float)out[1];
		}
	}
	return self;
}

void __fastcall CFXTexLava_dtor(CFXTexLava *self) {
	self->base.vftable = Game_Ptr(CFXTEXLAVA_VFTABLE);
	FXTex_FreeBmp(&self->bmp);
	FXTex_FreeBmp(&self->orig);
	IFXTextura_dtor(&self->base);
}

CFXTexLava *__thiscall CFXTexLava_Delete(CFXTexLava *self, byte flags) {
	CFXTexLava_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the time for the next step reached: then the time of now kept */
static int FXTex_Due(uint32_t *lastTime) {
	float now = g_TimeMsF;
	if ((float)(double)(*lastTime + FXTEX_PERIOD) > now) {
		return 0;
	}
	*lastTime = Game_FtoUI(now);
	return 1;
}

/* the average of a pixel and src */
static uint32_t FXTex_Mix(uint32_t dst, uint32_t src) {
	return (src >> 1 & 0x7f7f7f7f) + (dst >> 1 & 0x7f7f7f7f);
}

static int Lava_Fistp(float f) {
	return (int)X87_Fistp((double)f);
}

int __thiscall CFXTexLava_Update(CFXTexLava *self, CMapa *mapa) {
	if (!FXTex_Due(&self->lastTime)) {
		return 0;
	}
	float t = g_TimeMsF * 0.001f;
	float ty = t * LAVA_SPEEDY;
	float tx = -t;
	int lx = self->bmp->ancho;
	int ly = self->bmp->alto;
	CHECK(lx == FXTEX_SIZE);
	for (int x = 0; x < lx; x++) {
		int a = Lava_Fistp(((float)x * LAVA_STEP + tx) * LAVA_ANGLE) & 0xff;
		g_LavaOffset[x] = Lava_Fistp(g_LavaCos[a] * LAVA_AMPL);
	}
	float scroll = t * LAVA_SCROLL;
	for (int y = 0; y < ly; y++) {
		int a = Lava_Fistp(((float)y * LAVA_STEP + ty) * LAVA_ANGLE) & 0xff;
		int dx = Lava_Fistp(g_LavaCos[a] * LAVA_AMPL + scroll);
		uint32_t *row = CBmp_Pixel(self->bmp, 0, (uint32_t)y);
		for (int x = 0; x < lx; x++) {
			const uint32_t *src = self->orig->img;
			row[x] = FXTex_Mix(row[x], src[((g_LavaOffset[x] + y) & FXTEX_MASK) * FXTEX_SIZE + ((dx + x) & FXTEX_MASK)]);
		}
	}
	CMapa_CopyBmp(mapa, 0, 0, self->bmp, 0, 0, self->bmp->ancho, self->bmp->alto);
	return 0;
}

CFXTexScroll *__thiscall CFXTexScroll_ctor(CFXTexScroll *self, const char *file) {
	self->base.vftable = Game_Ptr(CFXTEXSCROLL_VFTABLE);
	self->lastTime = 0;
	self->offset = 0;
	self->i14 = 0;
	FXTex_LoadBmps(&self->bmp, &self->orig, file);
	CHECK(self->bmp->ancho == FXTEX_SIZE && self->bmp->alto == FXTEX_SIZE);
	return self;
}

void __fastcall CFXTexScroll_dtor(CFXTexScroll *self) {
	self->base.vftable = Game_Ptr(CFXTEXSCROLL_VFTABLE);
	FXTex_FreeBmp(&self->bmp);
	FXTex_FreeBmp(&self->orig);
	IFXTextura_dtor(&self->base);
}

CFXTexScroll *__thiscall CFXTexScroll_Delete(CFXTexScroll *self, byte flags) {
	CFXTexScroll_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

int __thiscall CFXTexScroll_Update(CFXTexScroll *self, CMapa *mapa) {
	if (!FXTex_Due(&self->lastTime)) {
		return 0;
	}
	int lx = self->bmp->ancho;
	int ly = self->bmp->alto;
	int off = self->offset++;
	for (int y = 0; y < ly; y++) {
		uint32_t *row = CBmp_Pixel(self->bmp, 0, (uint32_t)y);
		for (int x = 0; x < lx; x++) {
			const uint32_t *src = self->orig->img;
			row[x] = FXTex_Mix(row[x], src[((x - off) & FXTEX_MASK) + (y & FXTEX_MASK) * FXTEX_SIZE]);
		}
	}
	CMapa_CopyBmp(mapa, 0, 0, self->bmp, 0, 0, self->bmp->ancho, self->bmp->alto);
	return 0;
}

CFXNewsPanel *__fastcall CFXNewsPanel_ctor(CFXNewsPanel *self) {
	self->base.vftable = Game_Ptr(CFXNEWSPANEL_VFTABLE);
	self->time = 0;
	return self;
}

CFXNewsPanel *__thiscall CFXNewsPanel_Delete(CFXNewsPanel *self, byte flags) {
	IFXTextura_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

int __thiscall CFXNewsPanel_Update(CFXNewsPanel *self, CMapa *mapa) {
	(void)mapa;
	self->time = g_TimeMsInt;
	return 0;
}

void *__fastcall CFXNewsPanel_GetTexture(CFXNewsPanel *self) {
	(void)self;
	return NewsPanel_GetTarget();
}

void *FXTextura_Create(byte kind, CMapa *mapa) {
	switch (kind) {
	case FXTEX_LAVA: {
		CFXTexLava *p = operator_new(sizeof(*p));
		return p ? CFXTexLava_ctor(p, mapa->ruta) : NULL;
	}
	case FXTEX_SCROLL: {
		CFXTexScroll *p = operator_new(sizeof(*p));
		return p ? CFXTexScroll_ctor(p, mapa->ruta) : NULL;
	}
	case FXTEX_NEWSPANEL: {
		CFXNewsPanel *p = operator_new(sizeof(*p));
		return p ? CFXNewsPanel_ctor(p) : NULL;
	}
	}
	E3D_LogBlue(g_FileLog, "Texture FX not found: %s", mapa->Nombre);
	return NULL;
}

byte FXTextura_Kind(const char *name) {
	for (const char *s = strchr(name, '('); s; s = strchr(s + 1, '(')) {
		if (!crt_strnicmp(s, "(fxlava)", 8)) {
			return FXTEX_LAVA;
		}
		if (!crt_strnicmp(s, "(fxscroll)", 10)) {
			return FXTEX_SCROLL;
		}
		if (!crt_strnicmp(s, "(+fx:NewsPanel)", 15)) {
			return FXTEX_NEWSPANEL;
		}
	}
	return 0;
}
