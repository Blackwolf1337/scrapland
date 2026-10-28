/*
 * SliderScorer.cpp of the game (Menu/): cSliderScorer, a slider of a sprite sheet dragged with the cursor while
 * the accept is held. See include/scrap/scoreritems.h.
 */
#include "scrap/scoreritems.h"

static const char *const s_SliderColors[4] = {"Red", "Green", "Blue", "Alpha"};

cSliderScorer *__fastcall cSliderScorer_ctor(cSliderScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CSLIDERSCORER_VFTABLE);
	self->highRes = 0;
	self->blend = 3;
	cPy_Function_ctor(&self->onChange);
	self->img = NULL;
	self->img = ScorerImg_Replace(NULL, "2D/white.tga", self->blend, 0x20);
	self->base.x = 320.0f;
	self->base.y = 240.0f;
	self->value = 0.0f;
	self->lastValue = 0.0f;
	self->valueStep = 0.0f;
	self->minValue = 0.0f;
	self->maxValue = 0.0f;
	self->marginSize = 0.0f;
	self->maxValueForced = 0.0f;
	self->focusIndex = 0;
	self->accept = 0;
	self->lastAccept = 0;
	self->dragging = 0;
	return self;
}

void __fastcall cSliderScorer_dtor(cSliderScorer *self) {
	self->base.vftable = Game_Ptr(CSLIDERSCORER_VFTABLE);
	if (self->img) {
		ScorerImg_Free(self->img);
		self->img = NULL;
	}
	cPy_Function_dtor(&self->onChange);
	cScorerItem_dtor(&self->base);
}

cSliderScorer *__thiscall cSliderScorer_Delete(cSliderScorer *self, byte flags) {
	cSliderScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cSliderScorer));
	}
	return self;
}

void __thiscall cSliderScorer_SetFile(cSliderScorer *self, uint32_t flags, const char *file) {
	self->img = ScorerImg_Replace(self->img, file, self->blend, flags);
}

int __thiscall cSliderScorer_Var(cSliderScorer *self, const char *name, byte set, char **s, double *d, int *i) {
	int r;

	if (i) {
		if (!crt_stricmp(name, "HighRes")) {
			CHECK(*i == 0 || *i == 1);
			return ScorerVar_Int(&self->highRes, set, i);
		}
		if (!crt_stricmp(name, "BlendingMode")) {
			int v = *i;
			CHECK(v >= 0 && v <= 6);
			/* no read back */
			if (set) {
				self->blend = v;
			}
			return 3;
		}
	}
	if (s && !crt_stricmp(name, "File") && set) {
		cSliderScorer_SetFile(self, self->highRes ? 0x10060 : 0x60, *s);
		return 1;
	}
	if (i) {
		if (!crt_stricmp(name, "BackSpriteIndex")) {
			return ScorerVar_Int(&self->backIndex, set, i);
		}
		if (!crt_stricmp(name, "SliderSpriteIndex")) {
			return ScorerVar_Int(&self->sliderIndex, set, i);
		}
		if (!crt_stricmp(name, "FocusSpriteIndex")) {
			return ScorerVar_Int(&self->focusIndex, set, i);
		}
		if ((r = ScorerVar_Color(name, s_SliderColors, &self->img->color, set, i))) {
			return r;
		}
		if (!crt_stricmp(name, "Unit")) {
			return ScorerVar_Bool(&self->img->unit, set, i);
		}
	}
	if (d) {
		if (!crt_stricmp(name, "Value")) {
			if (set) {
				self->lastValue = self->value;
				float v = (float)*d;
				self->value = v;
				if (self->minValue > v) {
					self->value = self->minValue;
				}
				if (self->value > self->maxValue) {
					self->value = self->maxValue;
				}
			} else {
				*d = (double)self->value;
			}
			return 2;
		}
		if (!crt_stricmp(name, "ValueStep")) {
			return ScorerVar_Float(&self->valueStep, set, d);
		}
		if (!crt_stricmp(name, "MinValue")) {
			return ScorerVar_Float(&self->minValue, set, d);
		}
		if (!crt_stricmp(name, "MaxValue")) {
			return ScorerVar_Float(&self->maxValue, set, d);
		}
	}
	if (set && s && !crt_stricmp(name, "OnChange")) {
		cPy_Function_SetFunc(&self->onChange, *s);
		return 1;
	}
	if (d) {
		if (!crt_stricmp(name, "MarginSize")) {
			return ScorerVar_Float(&self->marginSize, set, d);
		}
		if (!crt_stricmp(name, "MaxValueForced")) {
			return ScorerVar_Float(&self->maxValueForced, set, d);
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

void __thiscall cSliderScorer_Draw(cSliderScorer *self, int player) {
	float uv0[2], uv1[2], suv0[2], suv1[2];
	float frac, bw, bh, sw, sh, m;
	cScorer *sc;
	byte focused, acc;

	CHECK(self->maxValue >= self->minValue);
	CPsaImage_GetFrameUV(self->img, self->backIndex, uv0, uv1);
	frac = (self->value - self->minValue) / (self->maxValue - self->minValue);
	bw = (uv1[0] - uv0[0]) * 256.0f;
	bh = (uv1[1] - uv0[1]) * 64.0f;
	if (self->highRes) {
		bw = bw * 0.5f;
		bh = bh * 0.5f;
	}
	CPsaImage_GetFrameUV(self->img, self->sliderIndex, suv0, suv1);
	sw = (suv1[0] - suv0[0]) * 256.0f;
	sh = (suv1[1] - suv0[1]) * 64.0f;
	if (self->highRes) {
		sw = sw * 0.5f;
		sh = sh * 0.5f;
	}
	sc = Scorer_Get(player);
	focused = !cString_ICmp(&self->base.name, &sc->actual);
	self->lastAccept = self->accept;
	acc = TABLE_AT(TABLE_AT(g_ActionSets, 0)->actions, 10)->value[player] > 0.5f;
	self->accept = acc;
	if (!acc) {
		self->dragging = 0;
	} else if (!self->lastAccept && focused) {
		self->dragging = 1;
	}
	if (focused && acc && self->dragging) {
		m = self->marginSize;
		float t = (sc->cursor->x - (self->base.x + m)) / (bw - (m + m));
		float k = 0.0f > t ? 0.0f : Float_MinSS(1.0f, t);
		self->lastValue = self->value;
		float v = (self->maxValue - self->minValue) * k + self->minValue;
		float mf = self->maxValueForced;
		self->value = v;
		if (mf > 0.0f && v > mf) {
			self->value = mf;
		}
		if (self->value != self->lastValue) {
			cPy_Function_AddParamI(&self->onChange, player);
			cPy_Function_AddParamS(&self->onChange, self->base.name);
			cPy_Function_Execute(&self->onChange, 0);
		}
	}
	CPsaImage_DrawFrame(self->img, self->backIndex, self->base.x, sh * 0.5f + self->base.y - bh * 0.5f);
	m = self->marginSize;
	sw = sw * 0.5f;
	CPsaImage_DrawFrame(self->img, self->sliderIndex, (bw - (m + m)) * frac + (self->base.x + m) - sw,
		self->base.y);
	if (focused && self->focusIndex) {
		uint32_t saved = self->img->color;
		int v = Scorer_Abs((int)((double)sc->time * 680.0) % 255 - 0x7f);
		self->img->color = (uint32_t)(byte)(v + 0x80) << 24 | 0xffffff;
		m = self->marginSize;
		CPsaImage_DrawFrame(self->img, self->focusIndex, (bw - (m + m)) * frac + (self->base.x + m) - sw,
			self->base.y);
		self->img->color = saved;
	}
}
