/*
 * SpriteScorer.cpp of the game (Menu/): cSpriteScorer, a sprite of a sheet, and cGradientScorer, a sprite filled by
 * four corner colors. Also the table helper of the item variables of include/scrap/scorerhud.h.
 */
#include "scrap/scorerhud.h"

int ScorerVar_Table(void *item, const ScorerVarDef *defs, int n, const char *name, byte set, double *d, int *i) {
	static const int shifts[4] = {16, 8, 0, 24};

	for (int k = 0; k < n; k++) {
		const ScorerVarDef *v = &defs[k];
		void *field = (byte *)item + v->ofs;

		if (v->kind == SCORERVAR_FLOAT ? !d : !i) {
			continue;
		}
		if (crt_stricmp(name, v->name)) {
			continue;
		}
		if (v->kind == SCORERVAR_FLOAT) {
			return ScorerVar_Float(field, set, d);
		}
		if (v->kind == SCORERVAR_INT) {
			return ScorerVar_Int(field, set, i);
		}
		return ScorerVar_ColorByte(field, shifts[v->kind - SCORERVAR_RED], set, i);
	}
	return 0;
}

cSpriteScorer *__fastcall cSpriteScorer_ctor(cSpriteScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CSPRITESCORER_VFTABLE);
	self->numSubSprites = 1;
	self->spriteIndex = 1;
	self->isMultiSprite = 0;
	self->highRes = 0;
	self->fixPosition = 0;
	self->blendingMode = 3;
	self->rotate = 0;
	self->mirror = 0;
	self->discardable = 0;
	self->img = NULL;
	cString_ctor(&self->file);
	cSpriteScorer_SetFile(self, 0x20, "2D/white.tga");
	self->base.x = 320.0f;
	self->base.y = 240.0f;
	self->jumpFX = 0.0f;
	return self;
}

/* also the destructor of cGradientScorer (rva 0x17b2d0, the same code) */
void __fastcall cSpriteScorer_dtor(cSpriteScorer *self) {
	CPsaImage *img = self->img;
	self->base.vftable = Game_Ptr(CSPRITESCORER_VFTABLE);
	if (img) {
		ScorerImg_Free(img);
		self->img = NULL;
	}
	cString_dtor(&self->file);
	cScorerItem_dtor(&self->base);
}

cSpriteScorer *__thiscall cSpriteScorer_Delete(cSpriteScorer *self, byte flags) {
	cSpriteScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cSpriteScorer));
	}
	return self;
}

void __thiscall cSpriteScorer_SetFile(cSpriteScorer *self, uint32_t flags, const char *name) {
	self->img = ScorerImg_Replace(self->img, name, self->blendingMode, (self->discardable ? 0x40000 : 0) | flags);
}

int __thiscall cSpriteScorer_Var(cSpriteScorer *self, const char *name, byte set, char **s, double *d, int *i) {
	if (i && !crt_stricmp(name, "FixPosition")) {
		if (set) {
			self->fixPosition = (byte)*i;
		} else {
			*i = self->fixPosition;
		}
		return 3;
	}
	if (d) {
		if (!crt_stricmp(name, "JumpFX")) {
			return ScorerVar_Float(&self->jumpFX, set, d);
		}
		if (!crt_stricmp(name, "ScaleX")) {
			return ScorerVar_Float(&self->img->scaleX, set, d);
		}
		if (!crt_stricmp(name, "ScaleY")) {
			return ScorerVar_Float(&self->img->scaleY, set, d);
		}
		if (!crt_stricmp(name, "PivotX")) {
			return ScorerVar_Float(&self->img->pivotX, set, d);
		}
		if (!crt_stricmp(name, "PivotY")) {
			return ScorerVar_Float(&self->img->pivotY, set, d);
		}
	}
	if (i) {
		if (!crt_stricmp(name, "IsMultiSprite")) {
			if (set) {
				self->isMultiSprite = (byte)*i;
			} else {
				*i = self->isMultiSprite;
			}
			return 3;
		}
		if (!crt_stricmp(name, "SpriteIndex")) {
			if (set) {
				CHECK(*i > 0 && *i <= self->numSubSprites);
				self->spriteIndex = (byte)*i;
			} else {
				*i = self->spriteIndex;
			}
			return 3;
		}
		if (!crt_stricmp(name, "HighRes")) {
			CHECK(*i == 0 || *i == 1);
			if (set) {
				self->highRes = (byte)*i;
			} else {
				*i = self->highRes;
			}
			return 3;
		}
		if (!crt_stricmp(name, "BlendingMode")) {
			int v = *i;
			CHECK(v >= 0 && v <= 6);
			/* no read back */
			if (set) {
				self->blendingMode = (byte)v;
			}
			return 3;
		}
	}
	if (s && !crt_stricmp(name, "File")) {
		if (set) {
			uint32_t flags = self->highRes ? 0x10020 : 0x20;
			if (!self->isMultiSprite) {
				cSpriteScorer_SetFile(self, flags, *s);
			} else {
				cSpriteScorer_SetFile(self, flags | 0x40, *s);
				self->numSubSprites = (byte)CPsaImage_GetNumFrames(self->img);
			}
			cString_Set(&self->file, *s);
		} else {
			*s = self->file;
		}
		return 1;
	}
	if (i) {
		static const char *const colors[4] = {"Red", "Green", "Blue", "Alpha"};
		int r;

		if ((r = ScorerVar_Color(name, colors, &self->img->color, set, i))) {
			return r;
		}
		if (!crt_stricmp(name, "Unit")) {
			return ScorerVar_Bool(&self->img->unit, set, i);
		}
		if (!crt_stricmp(name, "Mirror")) {
			if (set) {
				self->mirror = (byte)*i;
			} else {
				*i = self->mirror;
			}
			return 3;
		}
		if (!crt_stricmp(name, "Rotate")) {
			if (set) {
				self->rotate = (byte)*i;
			} else {
				*i = self->rotate;
			}
			return 3;
		}
		if (!crt_stricmp(name, "Discardable")) {
			if (set) {
				self->discardable = *i > 0;
			} else {
				*i = self->discardable != 0;
			}
			return 3;
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

void __thiscall cSpriteScorer_Draw(cSpriteScorer *self, int player) {
	float x = self->base.x;
	float y = self->base.y;

	if (self->fixPosition) {
		x = self->img->pivotX * self->base.w + x;
		y = self->img->pivotY * self->base.h + y;
	}
	if (self->jumpFX != 0.0f) {
		cScorer *sc = Scorer_Get(player);
		float j = (float)Game_Sin((double)(sc->time * 10.0f));
		y = y - self->jumpFX * j;
	}
	if (!self->isMultiSprite) {
		CPsaImage_Draw(self->img, x, y);
		return;
	}
	CPsaImage_DrawFrameFlip(self->img, self->spriteIndex, x, y, self->rotate, self->mirror);
}

void __thiscall cSpriteScorer_SetMultiSprite(cSpriteScorer *self, byte on) {
	self->isMultiSprite = on;
}

cGradientScorer *__fastcall cGradientScorer_ctor(cGradientScorer *self) {
	cSpriteScorer_ctor(&self->base);
	self->base.base.vftable = Game_Ptr(CGRADIENTSCORER_VFTABLE);
	self->colorSelected = 0;
	self->colors[0] = 0xffffffff;
	self->colors[1] = 0xffffffff;
	self->colors[2] = 0xffffffff;
	self->colors[3] = 0xffffffff;
	return self;
}

cGradientScorer *__thiscall cGradientScorer_Delete(cGradientScorer *self, byte flags) {
	cSpriteScorer_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cGradientScorer));
	}
	return self;
}

int __thiscall cGradientScorer_Var(cGradientScorer *self, const char *name, byte set, char **s, double *d, int *i) {
	static const char *const colors[4] = {"Red", "Green", "Blue", "Alpha"};
	int r;

	if (i) {
		if (!crt_stricmp(name, "ColorSelected")) {
			if (set) {
				self->colorSelected = *i;
				CHECK(self->colorSelected >= 0 && self->colorSelected <= 3);
			} else {
				*i = self->colorSelected;
			}
			return 3;
		}
		if ((r = ScorerVar_Color(name, colors, &self->colors[self->colorSelected], set, i))) {
			return r;
		}
	}
	return cSpriteScorer_Var(&self->base, name, set, s, d, i);
}

void __thiscall cGradientScorer_Draw(cGradientScorer *self, int player) {
	/* the unit square, as positions (scaled by the image) and as texture coordinates */
	static const float quad[8] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
	float pos[8], uv[8];
	CPsaImage *img;

	(void)player;
	for (int k = 0; k < 8; k++) {
		pos[k] = quad[k];
		uv[k] = quad[k];
	}
	self->base.img->color = 0xffffffff;
	img = self->base.img;
	img->x = self->base.base.x;
	img->y = self->base.base.y;
	CPsaImage_DrawPoly(img, pos, uv, self->colors, 4, 6);
}
