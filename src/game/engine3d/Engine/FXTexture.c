/*
 * The effects of animated maps of the engine3d, original source Engine/fxtextura.cpp (include/scrap/fxtexture.h).
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/flare.h"
#include "scrap/fxtexture.h"
#include "scrap/rendertarget.h"
#include "scrap/misc.h"

extern int g_TimeMsInt;	/* the engine time in ms as an int */
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

void IFXTexture_dtor(IFXTexture *self) {
	self->vftable = IFXTEXTURE_VFTABLE;
}

IFXTexture *IFXTexture_Delete(IFXTexture *self, byte flags) {
	self->vftable = IFXTEXTURE_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void *IFXTexture_GetTexture(IFXTexture *self) {
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

CFXTexLava *CFXTexLava_ctor(CFXTexLava *self, const char *file) {
	self->base.vftable = CFXTEXLAVA_VFTABLE;
	self->lastTime = 0;
	FXTex_LoadBmps(&self->bmp, &self->orig, file);
	CHECK(self->bmp->width == FXTEX_SIZE && self->bmp->height == FXTEX_SIZE);
	if (!g_LavaCosInit) {
		g_LavaCosInit = 1;
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

void CFXTexLava_dtor(CFXTexLava *self) {
	self->base.vftable = CFXTEXLAVA_VFTABLE;
	FXTex_FreeBmp(&self->bmp);
	FXTex_FreeBmp(&self->orig);
	IFXTexture_dtor(&self->base);
}

CFXTexLava *CFXTexLava_Delete(CFXTexLava *self, byte flags) {
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

int CFXTexLava_Update(CFXTexLava *self, CMap *map) {
	if (!FXTex_Due(&self->lastTime)) {
		return 0;
	}
	float t = g_TimeMsF * 0.001f;
	float ty = t * LAVA_SPEEDY;
	float tx = -t;
	int lx = self->bmp->width;
	int ly = self->bmp->height;
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
	CMap_CopyBmp(map, 0, 0, self->bmp, 0, 0, self->bmp->width, self->bmp->height);
	return 0;
}

CFXTexScroll *CFXTexScroll_ctor(CFXTexScroll *self, const char *file) {
	self->base.vftable = CFXTEXSCROLL_VFTABLE;
	self->lastTime = 0;
	self->offset = 0;
	self->unused_14 = 0;
	FXTex_LoadBmps(&self->bmp, &self->orig, file);
	CHECK(self->bmp->width == FXTEX_SIZE && self->bmp->height == FXTEX_SIZE);
	return self;
}

void CFXTexScroll_dtor(CFXTexScroll *self) {
	self->base.vftable = CFXTEXSCROLL_VFTABLE;
	FXTex_FreeBmp(&self->bmp);
	FXTex_FreeBmp(&self->orig);
	IFXTexture_dtor(&self->base);
}

CFXTexScroll *CFXTexScroll_Delete(CFXTexScroll *self, byte flags) {
	CFXTexScroll_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

int CFXTexScroll_Update(CFXTexScroll *self, CMap *map) {
	if (!FXTex_Due(&self->lastTime)) {
		return 0;
	}
	int lx = self->bmp->width;
	int ly = self->bmp->height;
	int off = self->offset++;
	for (int y = 0; y < ly; y++) {
		uint32_t *row = CBmp_Pixel(self->bmp, 0, (uint32_t)y);
		for (int x = 0; x < lx; x++) {
			const uint32_t *src = self->orig->img;
			row[x] = FXTex_Mix(row[x], src[((x - off) & FXTEX_MASK) + (y & FXTEX_MASK) * FXTEX_SIZE]);
		}
	}
	CMap_CopyBmp(map, 0, 0, self->bmp, 0, 0, self->bmp->width, self->bmp->height);
	return 0;
}

CFXNewsPanel *CFXNewsPanel_ctor(CFXNewsPanel *self) {
	self->base.vftable = CFXNEWSPANEL_VFTABLE;
	self->time = 0;
	return self;
}

CFXNewsPanel *CFXNewsPanel_Delete(CFXNewsPanel *self, byte flags) {
	IFXTexture_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

int CFXNewsPanel_Update(CFXNewsPanel *self, CMap *map) {
	(void)map;
	self->time = g_TimeMsInt;
	return 0;
}

void *CFXNewsPanel_GetTexture(CFXNewsPanel *self) {
	(void)self;
	return NewsPanel_GetTarget();
}

void *FXTexture_Create(byte kind, CMap *map) {
	switch (kind) {
	case FXTEX_LAVA: {
		CFXTexLava *p = operator_new(sizeof(*p));
		return p ? CFXTexLava_ctor(p, map->path) : NULL;
	}
	case FXTEX_SCROLL: {
		CFXTexScroll *p = operator_new(sizeof(*p));
		return p ? CFXTexScroll_ctor(p, map->path) : NULL;
	}
	case FXTEX_NEWSPANEL: {
		CFXNewsPanel *p = operator_new(sizeof(*p));
		return p ? CFXNewsPanel_ctor(p) : NULL;
	}
	}
	E3D_LogBlue(g_FileLog, "Texture FX not found: %s", map->Name);
	return NULL;
}

byte FXTexture_Kind(const char *name) {
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
