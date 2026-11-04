/*
 * CopMeterScorer.cpp of the game (Menu/): cCopMeterScorer, the police alarm meter. See
 * include/scrap/scoreritems.h.
 */
#include "scrap/app.h"
#include "scrap/scoreritems.h"

#define CCOPMETERSCORER_VFTABLE (&cCopMeterScorer_vftable)
static const cScorerItemVtbl cCopMeterScorer_vftable;

static CPsaImage *CopMeter_NewImage(const char *name, int flagsOnly) {
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	if (img) {
		img = flagsOnly ? CPsaImage_ctor(img, name, 0x10060) : CPsaImage_ctorFlags(img, name, 0, 2, 0x10060);
	}
	img->color = 0xffffffff;
	return img;
}

static void CopMeter_FreeImage(CPsaImage *img) {
	if (img) {
		CPsaImage_dtor(img);
		operator_delete(img);
	}
}

cCopMeterScorer *cCopMeterScorer_ctor(cCopMeterScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CCOPMETERSCORER_VFTABLE;
	self->base.x = 320.0f;
	self->base.y = 240.0f;
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x20) : NULL;
	self->img = img;
	img->color = 0xffffffff;
	img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x20) : NULL;
	self->flash = img;
	img->color = 0xffffffff;
	float level = g_pWorld->alarm.value;
	self->target = level;
	self->percent = level;
	self->on = g_pWorld->alarm.on;
	self->fade = 0.0f;
	self->blink = 0.0f;
	return self;
}

static void cCopMeterScorer_dtor(cCopMeterScorer *self) {
	CPsaImage *img = self->img;
	self->base.vftable = CCOPMETERSCORER_VFTABLE;
	CopMeter_FreeImage(img);
	CopMeter_FreeImage(self->flash);
	cScorerItem_dtor(&self->base);
}

static void cCopMeterScorer_Free(cScorerItem *base) {
	cCopMeterScorer *self = (cCopMeterScorer *)base;
	cCopMeterScorer_dtor(self);
	operator_delete(self);
}

/* a value 0..100 of the scripts as 0..1 */
static int CopMeter_VarPercent(float *v, byte set, double *d) {
	if (set) {
		*v = (float)(*d / 100.0);
	} else {
		*d = (double)(*v * 100.0f);
	}
	return 2;
}

/* Sprite (set), Percent, Target (0..100), BackIndex, BackOffX/Y, LightIndex, LightOffX/Y, BarOffX/Y, BarIndex */
static int cCopMeterScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cCopMeterScorer *self = (cCopMeterScorer *)base;
	if (s && !crt_stricmp(name, "Sprite") && set) {
		CopMeter_FreeImage(self->img);
		self->img = CopMeter_NewImage(*s, 1);
		CopMeter_FreeImage(self->flash);
		self->flash = CopMeter_NewImage(*s, 0);
		return 1;
	}
	if (d) {
		if (!crt_stricmp(name, "Percent")) {
			return CopMeter_VarPercent(&self->percent, set, d);
		}
		if (!crt_stricmp(name, "Target")) {
			return CopMeter_VarPercent(&self->target, set, d);
		}
	}
	if (i && !crt_stricmp(name, "BackIndex")) {
		return ScorerVar_Int(&self->backIndex, set, i);
	}
	if (d) {
		if (!crt_stricmp(name, "BackOffX")) {
			return ScorerVar_Float(&self->backOffX, set, d);
		}
		if (!crt_stricmp(name, "BackOffY")) {
			return ScorerVar_Float(&self->backOffY, set, d);
		}
	}
	if (i && !crt_stricmp(name, "LightIndex")) {
		return ScorerVar_Int(&self->lightIndex, set, i);
	}
	if (d) {
		if (!crt_stricmp(name, "LightOffX")) {
			return ScorerVar_Float(&self->lightOffX, set, d);
		}
		if (!crt_stricmp(name, "LightOffY")) {
			return ScorerVar_Float(&self->lightOffY, set, d);
		}
		if (!crt_stricmp(name, "BarOffX")) {
			return ScorerVar_Float(&self->barOffX, set, d);
		}
		if (!crt_stricmp(name, "BarOffY")) {
			return ScorerVar_Float(&self->barOffY, set, d);
		}
	}
	if (i && !crt_stricmp(name, "BarIndex")) {
		return ScorerVar_Int(&self->barIndex, set, i);
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

/* the alpha of a light flash: t of its window of 0.1 */
static void CopMeter_Flash(cCopMeterScorer *self, float t, int frame, float x) {
	CPsaImage *f = self->flash;
	f->color = (uint32_t)(int)(t * 10.0f * 255.0f) << 24 | (f->color & 0xffffff);
	CPsaImage_DrawFrame(self->flash, frame, x, (self->lightOffY + self->base.y) + 0.0f);
}

static void cCopMeterScorer_Draw(cScorerItem *base, int player) {
	cCopMeterScorer *self = (cCopMeterScorer *)base;
	float level = g_pWorld->alarm.value;
	float v;

	self->target = level;
	self->on = g_pWorld->alarm.on;
	if (!(0.0f >= self->fade)) {
		if (0.0f >= level) {
			v = self->fade - Scorer_Get(player)->frameTime;
		} else {
			v = Scorer_Get(player)->frameTime * 3.0f + self->fade;
		}
	} else {
		if (0.0f >= level) {
			return;
		}
		v = Scorer_Get(player)->frameTime * 3.0f + self->fade;
	}
	self->fade = v;
	if (v >= 1.0f) {
		self->fade = 1.0f;
	}
	float f = self->fade;
	CPsaImage *img = self->img;
	img->color = (uint32_t)(byte)(int)((3.0f - (f + f)) * (f * f) * 255.0f) << 24 | (img->color & 0xffffff);
	img = self->img;
	img->color = (uint32_t)((byte *)&img->color)[3] << 24 | 0xffffff;
	CPsaImage_DrawFrame(self->img, self->backIndex, self->backOffX + self->base.x, self->backOffY + self->base.y);
	self->img->pivotX = 0.5f;
	self->img->pivotY = 0.5f;
	CPsaImage_DrawFrame(self->img, self->lightIndex + 2, self->lightOffX + self->base.x,
		self->lightOffY + self->base.y);
	self->img->pivotX = 0.0f;
	self->img->pivotY = 0.0f;
	img = self->img;
	img->color = (uint32_t)(int)((float)((byte *)&img->color)[3] * 0.75f) << 24 | 0xff2020;
	if (self->percent > self->target) {
		float ft = Scorer_Get(player)->frameTime;
		v = self->percent - (ft + ft);
		self->percent = v;
		if (self->target > v) {
			self->percent = self->target;
		}
		img = self->img;
		img->color = (uint32_t)((byte *)&img->color)[3] << 24 | 0x2020ff;
	} else if (self->target > self->percent) {
		float ft = Scorer_Get(player)->frameTime;
		v = (float)((double)ft + (double)ft + (double)self->percent);
		self->percent = v;
		if (v > self->target) {
			self->percent = self->target;
		}
		img = self->img;
		img->color = (uint32_t)((byte *)&img->color)[3] << 24 | 0xff2020;
	}
	if (self->percent >= 1.0f) {
		self->percent = 1.0f;
		self->on = 1;
	} else if (0.0f >= self->percent) {
		self->percent = 0.0f;
		self->on = 0;
	}
	float uv0[2], uv1[2];
	CPsaImage_GetFrameUV(self->img, self->barIndex, uv0, uv1);
	Menu_DrawBar(self->img, self->barOffX + self->base.x, self->barOffY + self->base.y, self->percent, 7, 0.05f,
		uv0[0], uv0[1], uv1[0], uv1[1], 0.0f, 0.0f, 0.0f, 0.0f, 0, 0);
	if (!self->on) {
		return;
	}
	float ft = Scorer_Get(player)->frameTime;
	float b = self->blink + (self->percent * 0.5f + 0.5f) * ft;
	self->blink = b;
	if (b > 0.9f) {
		self->blink = 0.0f;
	}
	CPsaImage *fl = self->flash;
	fl->scaleX = 2.0f;
	fl->scaleY = 2.0f;
	fl = self->flash;
	fl->pivotX = 0.5f;
	fl->pivotY = 0.5f;
	self->flash->color = 0xffffffff;
	b = self->blink;
	if (b >= 0.6f && 0.7f > b) {
		CopMeter_Flash(self, b - 0.6f, self->lightIndex, (self->lightOffX + self->base.x) + 2.0f);
	} else if (b >= 0.7f && 0.8f > b) {
		CopMeter_Flash(self, b - 0.7f, self->lightIndex, (self->lightOffX + self->base.x) + 2.0f);
	}
	b = self->blink;
	if (b >= 0.1f && 0.2f > b) {
		CopMeter_Flash(self, b - 0.1f, self->lightIndex + 1, (self->lightOffX + self->base.x) - 2.0f);
	} else if (b >= 0.2f && 0.3f > b) {
		CopMeter_Flash(self, b - 0.2f, self->lightIndex + 1, (self->lightOffX + self->base.x) - 2.0f);
	}
}

/* the vtable of cCopMeterScorer */
static const cScorerItemVtbl cCopMeterScorer_vftable = {
	.Draw = cCopMeterScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cCopMeterScorer_Var,
	.Free = cCopMeterScorer_Free,
};
