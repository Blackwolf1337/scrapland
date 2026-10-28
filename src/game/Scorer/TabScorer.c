/*
 * TabScorer.cpp of the game (Menu/): cTabScorer, a frame of cells of a sprite sheet with a tab. See
 * include/scrap/scorerhud.h.
 */
#include "scrap/scorerhud.h"
#include "scrap/scrapmain.h"

cTabScorer *__fastcall cTabScorer_ctor(cTabScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CTABSCORER_VFTABLE);
	self->highRes = 0;
	self->blend = 3;
	self->filled = 1;
	self->img = NULL;
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctorFlags(img, "2D/white.tga", 0, self->blend, 0x20) : NULL;
	self->img = img;
	img->color = 0xffffffff;
	self->base.x = 320.0f;
	self->base.y = 240.0f;
	self->tabInit = 0;
	self->tabEnd = 0;
	self->sizeTabQuad = 1;
	self->type = 0;
	/* the width of the back text, measured by the first tab */
	if (-1.0f >= g_TabBackTextWidth && g_FontScrapBig) {
		float dim[2];
		const char *text = Lang_GetStr(g_IsXbox ? "XBox_Menu_Back" : "Menu_Back");
		Font_TextSize(g_FontScrapBig, NULL, dim, 0.0f, 0.0f, text);
		g_TabBackTextWidth = dim[0];
	}
	return self;
}

void __fastcall cTabScorer_dtor(cTabScorer *self) {
	CPsaImage *img = self->img;
	self->base.vftable = Game_Ptr(CTABSCORER_VFTABLE);
	if (img) {
		ScorerImg_Free(img);
		self->img = NULL;
	}
	cScorerItem_dtor(&self->base);
}

cTabScorer *__thiscall cTabScorer_Delete(cTabScorer *self, byte flags) {
	cTabScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cTabScorer));
	}
	return self;
}

void __thiscall cTabScorer_SetFile(cTabScorer *self, uint32_t flags, const char *name) {
	self->img = ScorerImg_Replace(self->img, name, self->blend, flags);
}

void __fastcall cTabScorer_UpdateSize(cTabScorer *self) {
	int cell;

	self->cell = self->highRes ? 7 : 14;
	self->sizeW = self->sizeX;
	self->sizeH = self->sizeY;
	cell = self->cell;
	self->cellsX = (int)(self->sizeW / (float)cell);
	self->cellsY = (int)(self->sizeH / (float)cell);
	self->sizeW = (float)(self->cellsX * cell);
	self->sizeH = (float)(self->cellsY * cell);
	if (!self->tabInit && !self->tabEnd) {
		self->tabInit = 0;
		self->tabEnd = self->cellsX;
		self->tabMax = self->cellsX;
	}
}

static int Tab_SetInt(cTabScorer *self, int *v, byte set, int *i) {
	if (set) {
		*v = *i;
		cTabScorer_UpdateSize(self);
	} else {
		*i = *v;
	}
	return 3;
}

int __thiscall cTabScorer_Var(cTabScorer *self, const char *name, byte set, char **s, double *d, int *i) {
	static const char *const colors[4] = {"Red", "Green", "Blue", "Alpha"};
	int r;

	if (d) {
		if (!crt_stricmp(name, "ScaleX")) {
			return ScorerVar_Float(&self->img->scaleX, set, d);
		}
		if (!crt_stricmp(name, "ScaleY")) {
			return ScorerVar_Float(&self->img->scaleY, set, d);
		}
	}
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
		cTabScorer_SetFile(self, self->highRes ? 0x10060 : 0x60, *s);
		return 1;
	}
	if (i) {
		if (!crt_stricmp(name, "TabSpriteIndex")) {
			return ScorerVar_Int(&self->tabSpriteIndex, set, i);
		}
		if ((r = ScorerVar_Color(name, colors, &self->img->color, set, i))) {
			return r;
		}
		if (!crt_stricmp(name, "Unit")) {
			return ScorerVar_Bool(&self->img->unit, set, i);
		}
	}
	if (d) {
		if (!crt_stricmp(name, "SizeX")) {
			if (set) {
				self->sizeX = (float)*d;
				cTabScorer_UpdateSize(self);
			} else {
				*d = (double)self->sizeX;
			}
			return 2;
		}
		if (!crt_stricmp(name, "SizeY")) {
			if (set) {
				self->sizeY = (float)*d;
				cTabScorer_UpdateSize(self);
			} else {
				*d = (double)self->sizeY;
			}
			return 2;
		}
	}
	if (i) {
		if (!crt_stricmp(name, "TabInit")) {
			return Tab_SetInt(self, &self->tabInit, set, i);
		}
		if (!crt_stricmp(name, "TabEnd")) {
			return Tab_SetInt(self, &self->tabEnd, set, i);
		}
		if (!crt_stricmp(name, "TabMax")) {
			return Tab_SetInt(self, &self->tabMax, set, i);
		}
		if (!crt_stricmp(name, "SizeTabQuad")) {
			return ScorerVar_Int(&self->sizeTabQuad, set, i);
		}
		if (!crt_stricmp(name, "Type")) {
			return ScorerVar_Int(&self->type, set, i);
		}
		if (!crt_stricmp(name, "Filled")) {
			return ScorerVar_Int(&self->filled, set, i);
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

void __thiscall cTabScorer_DrawRect(cTabScorer *self, CPsaImage *img, int x0, int y0, int x1, int y1, int frame) {
	float uv0[2], uv1[2], pos[8], uv[8];
	float du, dv, w, h;

	img->x = (float)(self->cell * x0) + self->base.x;
	img->y = (float)(self->cell * y0) + self->base.y;
	CPsaImage_GetFrameUV(img, frame, uv0, uv1);
	du = uv1[0] - uv0[0];
	dv = uv1[1] - uv0[1];
	w = (float)(x1 - x0) * du;
	h = (float)(y1 - y0) * dv;
	pos[0] = w * 0.0f;
	pos[1] = h * 0.0f;
	pos[2] = w;
	pos[3] = h * 0.0f;
	pos[4] = w;
	pos[5] = h;
	pos[6] = w * 0.0f;
	pos[7] = h;
	uv[0] = du * 0.0f + uv0[0];
	uv[1] = dv * 0.0f + uv0[1];
	uv[2] = du + uv0[0];
	uv[3] = dv * 0.0f + uv0[1];
	uv[4] = du + uv0[0];
	uv[5] = dv + uv0[1];
	uv[6] = du * 0.0f + uv0[0];
	uv[7] = dv + uv0[1];
	CPsaImage_DrawPoly(img, pos, uv, NULL, 4, 6);
}

void __thiscall cTabScorer_DrawCell(cTabScorer *self, CPsaImage *img, int x, int y, int frame, int rot, int flip) {
	/* the corners of the cell in the drawing order, and mirrored */
	static const float corners[2][8] = {
		{0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f},
		{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f},
	};
	const float *c = corners[flip != 0];
	float uv0[2], uv1[2], pos[8], uv[8];
	float du, dv;

	img->x = (float)(self->cell * x) + self->base.x;
	img->y = (float)(self->cell * y) + self->base.y;
	CPsaImage_GetFrameUV(img, frame, uv0, uv1);
	du = uv1[0] - uv0[0];
	dv = uv1[1] - uv0[1];
	pos[0] = du * 0.0f;
	pos[1] = dv * 0.0f;
	pos[2] = du;
	pos[3] = dv * 0.0f;
	pos[4] = du;
	pos[5] = dv;
	pos[6] = du * 0.0f;
	pos[7] = dv;
	for (int k = 0; k < 4; k++) {
		/* the corner turned by rot (0..7) */
		int n = rot + k < 4 ? rot + k : rot + k - 4;
		uv[k * 2] = c[n * 2] * du + uv0[0];
		uv[k * 2 + 1] = c[n * 2 + 1] * dv + uv0[1];
	}
	CPsaImage_DrawPoly(img, pos, uv, NULL, 4, 6);
}

/* the cells of the frame of the original are inlined copies of cTabScorer_DrawCell (rotations 0, 1, 3 and the
 * mirrored 0), called here */
void __thiscall cTabScorer_Draw(cTabScorer *self, int player) {
	int saveInit, saveEnd, n, lim, k;

	(void)player;
	if (self->type == 4) {
		Menu_DrawFrame(self->img, self->tabSpriteIndex, self->base.x, self->base.y, self->sizeX, self->sizeY);
		return;
	}
	saveInit = self->tabInit;
	saveEnd = self->tabEnd;
	if (self->type == 0 && !(self->sizeTabQuad > 0)) {
		self->tabInit = 0;
		self->tabEnd = self->cellsX;
	}
	/* the tab: its top, the corners, the sides */
	for (k = self->tabInit; k < self->tabEnd - 2; k++) {
		cTabScorer_DrawCell(self, self->img, k + 1, 0, self->tabSpriteIndex + 1, 0, 0);
	}
	cTabScorer_DrawCell(self, self->img, self->tabInit, 0, self->tabSpriteIndex, 0, 0);
	cTabScorer_DrawCell(self, self->img, self->tabEnd - 1, 0, self->tabSpriteIndex, 3, 0);
	for (k = 1; k < self->sizeTabQuad; k++) {
		cTabScorer_DrawCell(self, self->img, self->tabInit, k, self->tabSpriteIndex + 1, 1, 0);
	}
	for (k = 1; k < self->sizeTabQuad; k++) {
		cTabScorer_DrawCell(self, self->img, self->tabEnd - 1, k, self->tabSpriteIndex + 1, 3, 0);
	}
	if (self->filled || self->type == 1) {
		n = self->sizeTabQuad;
		if (self->type != 1) {
			n++;
		}
		cTabScorer_DrawRect(self, self->img, self->tabInit + 1, 1, self->tabEnd - 1, n, self->tabSpriteIndex + 2);
	}
	if (self->sizeTabQuad > 0) {
		if (self->type == 1) {
			cTabScorer_DrawCell(self, self->img, self->tabEnd - 1, self->sizeTabQuad, self->tabSpriteIndex + 6, 0,
				0);
		} else if (self->type != 0 || self->tabEnd < self->cellsX) {
			cTabScorer_DrawCell(self, self->img, self->tabEnd - 1, self->sizeTabQuad, self->tabSpriteIndex + 3, 0,
				0);
		} else {
			cTabScorer_DrawCell(self, self->img, self->tabEnd - 1, self->sizeTabQuad, self->tabSpriteIndex + 1, 3,
				0);
		}
	}
	/* the bottom of the tab: type 1 is only the tab */
	if (self->tabInit > 0) {
		if (self->type == 1) {
			cTabScorer_DrawCell(self, self->img, self->tabInit, self->sizeTabQuad, self->tabSpriteIndex + 6, 0, 1);
		} else {
			cTabScorer_DrawCell(self, self->img, 0, self->sizeTabQuad, self->tabSpriteIndex + 5, 0, 0);
		}
	} else if (self->type == 1) {
		cTabScorer_DrawCell(self, self->img, 0, self->sizeTabQuad, self->tabSpriteIndex + 8, 0, 0);
	}
	if (self->type == 1) {
		for (k = self->tabInit + 1; k < self->tabEnd - 1; k++) {
			cTabScorer_DrawCell(self, self->img, k, self->sizeTabQuad, self->tabSpriteIndex + 7, 0, 0);
		}
		goto done;
	}
	/* the frame below the tab */
	for (k = self->tabEnd; k < self->cellsX - 1; k++) {
		cTabScorer_DrawCell(self, self->img, k, self->sizeTabQuad, self->tabSpriteIndex + 1, 0, 0);
	}
	if (self->tabInit > 0) {
		cTabScorer_DrawCell(self, self->img, self->tabInit, self->sizeTabQuad, self->tabSpriteIndex + 4, 0, 0);
	}
	for (k = 1; k < self->tabInit; k++) {
		cTabScorer_DrawCell(self, self->img, k, self->sizeTabQuad, self->tabSpriteIndex + 1, 0, 0);
	}
	if (self->sizeTabQuad > 0 && (self->type != 0 || self->tabEnd < self->cellsX)) {
		cTabScorer_DrawCell(self, self->img, self->cellsX - 1, self->sizeTabQuad, self->tabSpriteIndex, 3, 0);
	}
	n = self->sizeTabQuad;
	if (n <= 0) {
		n = 1;
	}
	lim = self->type == 3 ? self->cellsY - 5 : self->cellsY - 1;
	for (k = (self->tabInit > 0) + n; k < lim; k++) {
		cTabScorer_DrawCell(self, self->img, 0, k, self->tabSpriteIndex + 1, 1, 0);
	}
	if (self->type != 3) {
		cTabScorer_DrawCell(self, self->img, 0, self->cellsY - 1, self->tabSpriteIndex, 1, 0);
		if (self->type != 3) {
			cTabScorer_DrawCell(self, self->img, self->cellsX - 1, self->cellsY - 1, self->tabSpriteIndex, 2, 0);
		}
	}
	for (k = self->sizeTabQuad + 1; k < lim; k++) {
		cTabScorer_DrawCell(self, self->img, self->cellsX - 1, k, self->tabSpriteIndex + 1, 3, 0);
	}
	if (self->type == 2) {
		/* the bottom with a gap for the back text in the middle */
		int half = self->cellsX / 2;
		int wid = (g_TabBackTextWidth > 0.0f ? (int)(g_TabBackTextWidth / 14.0f) : 4) + 2;
		int a = half - wid;
		int b = wid + half;

		for (k = 1; k < a; k++) {
			cTabScorer_DrawCell(self, self->img, k, self->cellsY - 1, self->tabSpriteIndex + 1, 2, 0);
		}
		cTabScorer_DrawCell(self, self->img, a, self->cellsY - 1, self->tabSpriteIndex, 2, 0);
		cTabScorer_DrawCell(self, self->img, a, self->cellsY - 2, self->tabSpriteIndex + 4, 2, 0);
		for (k = b + 1; k < self->cellsX - 1; k++) {
			cTabScorer_DrawCell(self, self->img, k, self->cellsY - 1, self->tabSpriteIndex + 1, 2, 0);
		}
		cTabScorer_DrawCell(self, self->img, b, self->cellsY - 1, self->tabSpriteIndex, 1, 0);
		cTabScorer_DrawCell(self, self->img, b, self->cellsY - 2, self->tabSpriteIndex + 4, 1, 0);
		for (k = a + 1; k < b; k++) {
			cTabScorer_DrawCell(self, self->img, k, self->cellsY - 2, self->tabSpriteIndex + 1, 2, 0);
		}
		cTabScorer_DrawRect(self, self->img, 1, self->sizeTabQuad + 1, self->cellsX - 1, self->cellsY - 2,
			self->tabSpriteIndex + 2);
		cTabScorer_DrawRect(self, self->img, 1, self->cellsY - 2, a, self->cellsY - 1, self->tabSpriteIndex + 2);
		cTabScorer_DrawRect(self, self->img, b + 1, self->cellsY - 2, self->cellsX - 1, self->cellsY - 1,
			self->tabSpriteIndex + 2);
	} else if (self->type == 3) {
		/* the bottom with the two corner pieces */
		cTabScorer_DrawCell(self, self->img, 0, self->cellsY - 5, self->tabSpriteIndex + 14, 0, 0);
		cTabScorer_DrawCell(self, self->img, self->cellsX - 7, self->cellsY - 5, self->tabSpriteIndex + 14, 0, 1);
		for (k = 7; k < self->cellsX - 7; k++) {
			cTabScorer_DrawCell(self, self->img, k, self->cellsY - 1, self->tabSpriteIndex + 1, 2, 0);
		}
		cTabScorer_DrawRect(self, self->img, 1, self->sizeTabQuad + 1, self->cellsX - 1, self->cellsY - 5,
			self->tabSpriteIndex + 2);
		cTabScorer_DrawRect(self, self->img, 7, self->cellsY - 5, self->cellsX - 7, self->cellsY - 1,
			self->tabSpriteIndex + 2);
	} else {
		for (k = 1; k < self->cellsX - 1; k++) {
			cTabScorer_DrawCell(self, self->img, k, self->cellsY - 1, self->tabSpriteIndex + 1, 2, 0);
		}
		if (self->filled) {
			cTabScorer_DrawRect(self, self->img, 1, self->sizeTabQuad + 1, self->cellsX - 1, self->cellsY - 1,
				self->tabSpriteIndex + 2);
		}
	}
done:
	self->tabInit = saveInit;
	self->tabEnd = saveEnd;
}
