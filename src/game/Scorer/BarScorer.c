/*
 * BarScorer.cpp of the game (Menu/): cBarScorer, a value bar of a sprite sheet. See include/scrap/scoreritems.h.
 */
#include "scrap/app.h"
#include "scrap/scoreritems.h"

static const char *const s_BackColors[4] = {"BackRed", "BackGreen", "BackBlue", "BackAlpha"};
static const char *const s_MainColors[4] = {"MainRed", "MainGreen", "MainBlue", "MainAlpha"};
static const char *const s_BlackColors[4] = {"BlackRed", "BlackGreen", "BlackBlue", "BlackAlpha"};

cBarScorer *cBarScorer_ctor(cBarScorer *self) {
	cSpriteScorer_ctor(&self->base);
	self->base.base.vftable = CBARSCORER_VFTABLE;
	self->maxValue = 0.0f;
	self->blinkTime = 0.0f;
	self->blink = 0;
	self->blinkOff = 0;
	self->mainIndex = 0;
	self->blackIndex = 0;
	self->mainUV0[0] = 0.0f;
	self->mainUV0[1] = 0.0f;
	self->mainUV1[0] = 1.0f;
	self->mainUV1[1] = 1.0f;
	self->blackUV0[0] = 0.0f;
	self->blackUV0[1] = 0.0f;
	self->blackUV1[0] = 1.0f;
	self->blackUV1[1] = 1.0f;
	self->backIndex = 0;
	self->backColor = 0xffffffff;
	self->mainColor = 0xffffffff;
	self->blackColor = 0xffffff;
	return self;
}

void cBarScorer_dtor(cBarScorer *self) {
	self->base.base.vftable = CBARSCORER_VFTABLE;
	cSpriteScorer_dtor(&self->base);
}

cScorerItem *cBarScorer_Delete(cScorerItem *base, byte flags) {
	cBarScorer *self = (cBarScorer *)base;
	cBarScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cBarScorer));
	}
	return base;
}

int cBarScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cBarScorer *self = (cBarScorer *)base;
	int r;

	if (d) {
		if (!crt_stricmp(name, "MaxValue")) {
			return ScorerVar_Float(&self->maxValue, set, d);
		}
		if (!crt_stricmp(name, "Value")) {
			return ScorerVar_Float(&g_BarValue, set, d);
		}
		if (!crt_stricmp(name, "AutoUpdateStopValue")) {
			if (set) {
				float v = (float)*d;
				g_BarStopValue = v;
				if (v > self->maxValue) {
					g_BarStopValue = self->maxValue;
				} else if (0.0f > v) {
					g_BarStopValue = 0.0f;
				}
			} else {
				*d = (double)g_BarStopValue;
			}
			return 2;
		}
		if (!crt_stricmp(name, "AutoUpdateSpeedFactor")) {
			return ScorerVar_Float(&g_BarSpeedFactor, set, d);
		}
	}
	if (i) {
		if (!crt_stricmp(name, "MainBarIndex")) {
			if (set) {
				int w, h;
				self->mainIndex = *i;
				CPsaImage_GetFrameUV(self->base.img, self->mainIndex, self->mainUV0, self->mainUV1);
				CPsaImage_GetFrameSize(self->base.img, self->mainIndex, &w, &h);
				self->base.base.w = (float)w;
				self->base.base.h = (float)h;
			} else {
				*i = self->mainIndex;
			}
			return 3;
		}
		if (!crt_stricmp(name, "BlackBarIndex")) {
			if (set) {
				self->blackIndex = *i;
				CPsaImage_GetFrameUV(self->base.img, self->blackIndex, self->blackUV0, self->blackUV1);
			} else {
				*i = self->blackIndex;
			}
			return 3;
		}
		if (!crt_stricmp(name, "IsAutoUpdatable")) {
			return ScorerVar_Int(&g_BarAutoUpdate, set, i);
		}
		if (!crt_stricmp(name, "Blink")) {
			if (set) {
				self->blink = *i == 1;
			} else {
				*i = self->blink != 0;
			}
			return 3;
		}
		if (!crt_stricmp(name, "BackIndex")) {
			return ScorerVar_Int(&self->backIndex, set, i);
		}
		if ((r = ScorerVar_Color(name, s_BackColors, &self->backColor, set, i))
			|| (r = ScorerVar_Color(name, s_MainColors, &self->mainColor, set, i))
			|| (r = ScorerVar_Color(name, s_BlackColors, &self->blackColor, set, i))) {
			return r;
		}
	}
	return cSpriteScorer_Var(&self->base.base, name, set, s, d, i);
}

void cBarScorer_Draw(cScorerItem *base, int player) {
	cBarScorer *self = (cBarScorer *)base;
	cSpriteScorer *sp = &self->base;
	int mode = g_BarAutoUpdate;
	float frac;

	(void)player;
	if (mode == 1) {
		if (g_BarValue > 0.0f) {
			g_BarValue = (float)(g_BarValue - (cTimer_GetDelta(&g_pWorld->timer)) * g_BarSpeedFactor);
			if (0.0f >= g_BarValue) {
				g_BarValue = 0.0f;
				if (cPy_Function_IsValid(g_BarOnFinish)) {
					cPy_Function_Execute(g_BarOnFinish, 0);
				}
			} else if (g_BarStopValue >= g_BarValue) {
				g_BarValue = g_BarStopValue;
			}
		}
	} else if (mode == 2 && self->maxValue > g_BarValue) {
		g_BarValue = (float)((cTimer_GetDelta(&g_pWorld->timer)) * g_BarSpeedFactor + g_BarValue);
		float max = self->maxValue;
		float v = g_BarValue;
		if (v >= max) {
			g_BarValue = max;
			if (cPy_Function_IsValid(g_BarOnFinish)) {
				cPy_Function_Execute(g_BarOnFinish, 0);
			}
		} else if (v >= g_BarStopValue) {
			g_BarValue = g_BarStopValue;
		}
	}
	if (self->blink) {
		float t = cTimer_GetDelta(&g_pWorld->timer) + self->blinkTime;
		self->blinkTime = t;
		if (t >= g_BarBlinkTime) {
			self->blinkTime = 0.0f;
			self->blinkOff = !self->blinkOff;
		}
	}
	if (self->backIndex) {
		sp->img->color = self->backColor;
		CPsaImage_DrawFrame(sp->img, self->backIndex, sp->base.x - 6.0f, sp->base.y - 6.0f);
	}
	if (self->blink && self->blinkOff) {
		frac = 0.0f;
	} else {
		frac = g_BarValue / self->maxValue;
	}
	Menu_DrawBar(sp->img, sp->base.x, sp->base.y, frac, 0, 0.0f, self->mainUV0[0], self->mainUV0[1],
		self->mainUV1[0], self->mainUV1[1], self->blackUV0[0], self->blackUV0[1], self->blackUV1[0],
		self->blackUV1[1], self->mainColor, self->blackColor);
}

/* the vtable of cBarScorer */
const cSpriteScorerVtbl cBarScorer_vftable = {
	.base.Draw = cBarScorer_Draw,
	.base.SetParam = cScorerItem_SetParam,
	.base.Var = cBarScorer_Var,
	.base.Delete = cBarScorer_Delete,
	.SetMultiSprite = cSpriteScorer_SetMultiSprite,
};

cGVarFunction GVar_OnFinishAutoUpdate;

cGVar GVar_BlinkTime;

cGVar GVar_IsAutoUpdatable;

cGVar GVar_AutoUpdateSpeedFactor;

cGVar GVar_AutoUpdateStopValue;

cGVar GVar_BarValue;

