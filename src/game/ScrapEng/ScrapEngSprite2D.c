/*
 * ScrapEngSprite2D.cpp of the game: CSprite2D (CPsaImage here), the state of a screen image over a CGrafico2D of the
 * engine3d: position, pivot, scale, mirror and frame of a .psa sprite sheet, copied to the CGrafico2D at each draw.
 * The count of the images is g_CountSprite2D.
 */
#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/grafico2d.h"
#include "scrap/scorerhud.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/textura.h"

/* the scale of the images, of SPR2D 0x10000 and of SPR2D 0x20000 (two floats each) */
#define g_Sprite2DScale (GAME_ARRAY(float, g_Sprite2DScale, 0x431b8c))
#define g_Sprite2DScaleA (GAME_ARRAY(float, g_Sprite2DScaleA, 0x431b94))
#define g_Sprite2DScaleB (GAME_ARRAY(float, g_Sprite2DScaleB, 0x431b9c))

#define Sprite_Graf(p) ((CGrafico2D *)(p)->sprite)

static CMultiSprite *Sprite_MultiSprite(const CPsaImage *self) {
	return Sprite_Graf(self)->Mat->mapa[0]->MultiSprite;
}

void __fastcall CPsaImage_Init(CPsaImage *self) {
	g_CountSprite2D++;
	self->x = 0.0f;
	self->y = 0.0f;
	self->pivotX = 0.0f;
	self->pivotY = 0.0f;
	self->scaleX = 1.0f;
	self->scaleY = 1.0f;
	self->f18 = g_Sprite2DScale[0];
	self->f1c = g_Sprite2DScale[1];
	self->pantalla = 0;
	self->tamPantalla = 0;
	self->unit = 0;
	self->uv0[0] = 0.0f;
	self->uv0[1] = 0.0f;
	self->uv1[0] = 1.0f;
	self->uv1[1] = 1.0f;
	self->color = 0xffffff;
	self->sprite = NULL;
	self->frame = 0;
	self->flip = 0;
}

void *__thiscall CPsaImage_ctorTex(void *self, const char *name, int a, int b) {
	CPsaImage_Init(self);
	CPsaImage_Load(self, name, (const char *)a, 3, (uint32_t)b);
	return self;
}

void *__thiscall CPsaImage_ctorFlags(void *self, const char *name, int a, int b, int flags) {
	CPsaImage_Init(self);
	CPsaImage_Load(self, name, (const char *)a, b, (uint32_t)flags);
	return self;
}

CPsaImage *__thiscall CPsaImage_ctor(CPsaImage *self, const char *name, int flags) {
	CPsaImage_Init(self);
	CPsaImage_Load(self, name, NULL, 3, (uint32_t)flags);
	return self;
}

CPsaImage *__fastcall CPsaImage_ctorEmpty(CPsaImage *self) {
	CPsaImage_Init(self);
	return self;
}

void __fastcall CPsaImage_dtor(CPsaImage *self) {
	CGrafico2D *g;

	CHECK(g_CountSprite2D > 0);
	g_CountSprite2D--;
	g = self->sprite;
	if (g) {
		CGrafico2D_dtor(g);
		operator_delete_sized(g, sizeof(*g));
		self->sprite = NULL;
	}
}

int __thiscall CPsaImage_GetNumFrames(CPsaImage *self) {
	CMapa *map;

	CHECK(self->sprite);
	map = Sprite_Graf(self)->Mat->mapa[0];
	CHECK(map->MultiSprite);
	return ((CMultiSprite *)map->MultiSprite)->n;
}

byte __thiscall CPsaImage_GetFrameSize(CPsaImage *self, int frame, int *w, int *h) {
	CHECK(self->sprite);
	return CMultiSprite_GetSize(Sprite_MultiSprite(self), (uint32_t)frame, (uint32_t *)w, (uint32_t *)h);
}

byte __thiscall CPsaImage_GetFrameUV(CPsaImage *self, int frame, float *uv0, float *uv1) {
	CHECK(self->sprite);
	if (frame == 0) {
		uv0[0] = self->uv0[0];
		uv0[1] = self->uv0[1];
		uv1[0] = self->uv1[0];
		uv1[1] = self->uv1[1];
		return 0;
	}
	if (!CMultiSprite_GetUV(Sprite_MultiSprite(self), (uint32_t)frame, uv0, uv1)) {
		CHECK(0);
	}
	return 1;
}

void __thiscall CPsaImage_Draw(CPsaImage *self, float x, float y) {
	self->x = x;
	self->y = y;
	CPsaImage_DrawPoly(self, NULL, NULL, NULL, 0, 0);
}

void __thiscall CPsaImage_DrawScaled(CPsaImage *self, float x, float y, float scale) {
	self->x = x;
	self->y = y;
	self->scaleX = scale;
	self->scaleY = scale;
	CPsaImage_DrawPoly(self, NULL, NULL, NULL, 0, 0);
}

void __thiscall CPsaImage_DrawFrame(CPsaImage *self, int frame, float x, float y) {
	self->x = x;
	self->y = y;
	self->frame = frame;
	CPsaImage_DrawPoly(self, NULL, NULL, NULL, 0, 0);
	self->frame = 0;
}

void __thiscall CPsaImage_DrawFrameFlip(CPsaImage *self, int frame, float x, float y, int mode, int flip) {
	CHECK(mode >= 0 && mode <= 3);
	CHECK(flip >= 0 && flip <= 3);
	self->flip = flip;
	Sprite_Graf(self)->rot = (byte)mode;
	self->x = x;
	self->y = y;
	self->frame = frame;
	CPsaImage_DrawPoly(self, NULL, NULL, NULL, 0, 0);
	self->frame = 0;
}

void __thiscall CPsaImage_DrawPoly(void *img, const float *pos, const float *uv, const uint32_t *colors, int n,
	int mode) {
	CPsaImage *self = img;
	CGrafico2D *g;
	float scale[2];

	CHECK(self->sprite);
	g = self->sprite;
	g->x = self->x;
	g->y = self->y;
	g = self->sprite;
	g->alignX = self->pivotX;
	g->alignY = self->pivotY;
	g = self->sprite;
	g->escala[0] = self->scaleX;
	g->escala[1] = self->scaleY;
	if (self->unit) {
		scale[1] = 1.0f;
		scale[0] = 1.0f;
	} else {
		scale[0] = self->f18;
		scale[1] = self->f1c;
	}
	g = self->sprite;
	g->escala2[0] = scale[0];
	g->escala2[1] = scale[1];
	Sprite_Graf(self)->pantalla = self->pantalla == 0;
	Sprite_Graf(self)->tamPantalla = self->tamPantalla == 0;
	Sprite_Graf(self)->sinTextura = self->unit;
	Sprite_Graf(self)->color = self->color;
	g = self->sprite;
	CPsaImage_GetFrameUV(self, self->frame, &g->u0, &g->u1);
	if (self->flip > 0) {
		if (self->flip & 1) {
			float t = Sprite_Graf(self)->u0;

			g = self->sprite;
			g->u0 = g->u1;
			Sprite_Graf(self)->u1 = t;
		}
		if (self->flip & 2) {
			float t = Sprite_Graf(self)->v0;

			g = self->sprite;
			g->v0 = g->v1;
			Sprite_Graf(self)->v1 = t;
		}
	}
	if (mode == 0) {
		CGrafico2D_Render(self->sprite);
		return;
	}
	if (mode != 4 && mode != 5 && mode != 6) {
		Con_Bluef("ERROR: CSprite2D: Unsupported primitive type: %d\n", mode);
		CHECK(0);
	}
	CGrafico2D_Draw(self->sprite, pos, uv, colors, (uint32_t)n, mode);
}

/* the CGrafico2D of the sheet name (opac: the opacity map) with the blend mode and the SPR2D flags */
void __thiscall CPsaImage_Load(CPsaImage *self, const char *name, const char *opac, int blend, uint32_t flags) {
	void *mem;
	uint32_t gflags, alpha;
	int src, dst;

	CHECK(!self->sprite);
	if (flags & 1) {
		self->pantalla = 1;
	}
	if (flags & 2) {
		self->tamPantalla = 1;
	}
	if (flags & 4) {
		self->unit = 1;
	}
	if (flags & 8) {
		self->pivotX = 0.5f;
		self->pivotY = 0.5f;
	}
	if (flags & 0x10000) {
		self->f18 = g_Sprite2DScaleA[0];
		self->f1c = g_Sprite2DScaleA[1];
	}
	if (flags & 0x20000) {
		self->f18 = g_Sprite2DScaleB[0];
		self->f1c = g_Sprite2DScaleB[1];
	}
	mem = operator_new(sizeof(CGrafico2D));
	self->sprite = mem ? CGrafico2D_ctor(mem) : NULL;
	gflags = ((((flags >> 1 & 0x20000) | (flags & 0x3000)) >> 3 | (flags & 0x100)) >> 6) | (flags & 0x10) * 2 |
		(flags & 0x40);
	alpha = (byte)((flags & 0xe0) << 2);
	dst = BlendMode_GetDst(blend);
	src = BlendMode_GetSrc(blend);
	CGrafico2D_CreateMat(self->sprite, name, opac, src, dst, (byte)alpha, gflags);
	if (blend == 1 && (flags & 0x1000)) {
		Con_Warnf("CSprite2D: SPR2D_ZMASK: BLENDMODE_ADD_ONE does not use alpha test\n");
	}
}

/* the plain loads of the sheet name, blend mode 3 */
void __thiscall CPsaImage_LoadOpac(CPsaImage *self, const char *name, const char *opac, uint32_t flags) {
	CPsaImage_Load(self, name, opac, 3, flags);
}

void __thiscall CPsaImage_LoadName(CPsaImage *self, const char *name, uint32_t flags) {
	CPsaImage_Load(self, name, NULL, 3, flags);
}
