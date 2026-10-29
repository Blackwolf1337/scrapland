/*
 * ButtonScorer.cpp of the game (Menu/): cButtonScorer, the menu buttons, and the Global/Table.h functions of its
 * cString and byte tables. See include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/scoreritems.h"

/* the frames of a Complex box: 3x3 corners, edges and middle, the second row mirrored */
static const int s_BoxFrames[18] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 2, 1, 0, 5, 4, 3, 8, 7, 6};
/* the four corners of the highlight frame, the second row mirrored */
static const int s_FocusCorners[8] = {0, 1, 2, 3, 1, 0, 3, 2};

static const char *const s_TextColors[4] = {"TextColorRed", "TextColorGreen", "TextColorBlue", "TextColorAlpha"};
static const char *const s_BorderColors[3] = {"BorderColorRed", "BorderColorGreen", "BorderColorBlue"};
static const char *const s_HighLightColors[3] = {"HighLightColorRed", "HighLightColorGreen", "HighLightColorBlue"};

/* ---- Global/Table.h ---- */

/* also rva 0x1613b0 */
void __fastcall StrTable_Free(cStrTable *self) {
	if (self->items) {
		Delete_Objects(self->items, sizeof(cString), GAME_FUNC(cString_dtor, CSTRING_DTOR));
		self->items = NULL;
		self->count = 0;
	}
}

/* also rva 0x161430 */
void __fastcall ByteTable_Free(cByteTable *self) {
	if (self->items) {
		operator_delete_array(self->items);
		self->items = NULL;
		self->count = 0;
	}
}

char **__thiscall StrTable_At(cStrTable *self, uint32_t i) {
	return TABLE_AT(*self, i);
}

void __thiscall StrTable_Init(cStrTable *self, int num) {
	CHECK(!self->items && num > 0);
	self->count = num;
	self->items = New_Objects(num, sizeof(cString), GAME_FUNC(cString_ctor, CSTRING_CTOR), GAME_FUNC(cString_dtor, CSTRING_DTOR));
	CHECK(self->items);
}

cString *__thiscall cString_DeleteVec(cString *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(cString), *mem, GAME_FUNC(cString_dtor, CSTRING_DTOR));
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return (cString *)mem;
	}
	cString_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cString));
	}
	return self;
}

/* ---- construction ---- */

cButtonScorer *__fastcall cButtonScorer_ctor(cButtonScorer *self) {
	cSpriteScorer_ctor(&self->base);
	self->base.base.vftable = CBUTTONSCORER_VFTABLE;
	self->lines.items = NULL;
	self->lines.count = 0;
	self->isValue.items = NULL;
	self->isValue.count = 0;
	self->xAlignLeft = 0.0f;
	self->xAlignRight = 0.0f;
	self->xAlignCenter = 0.0f;
	self->highlight = 0;
	self->glow = 0;
	self->focusSpriteIndex = 1;
	self->borderSpriteIndex = -1;
	self->width = 1;
	self->height = 0;
	self->maxLineSize = -1;
	self->hasText = 0;
	self->textAlign = BUTTON_CENTER;
	self->marker = 0;
	self->isNameValuePair = 1;
	self->renderAlphaChannel = 0;
	self->markerOffset = 0.0f;
	self->highlightOffset = 0.0f;
	self->lineHeight = 0.0f;
	self->scaleSmoothWidth = 0.0f;
	self->scaleSmoothHeight = 0.0f;
	self->scaleSmoothTime = 0.0f;
	self->textColor = 0xffffffff;
	self->textWidth = -1.0f;
	self->textHeight = -1.0f;
	self->complex = 1;
	self->minX = -1.0f;
	self->maxX = -1.0f;
	self->focusImg = NULL;
	self->base.isMultiSprite = 1;
	self->font = g_FontHoratio;
	self->origScaleX = self->base.img->scaleX;
	self->origScaleY = self->base.img->scaleY;
	self->borderColor = 0xffffffff;
	self->highLightColor = 0xffffffff;
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	self->alphaImg = img ? CPsaImage_ctorTex(img, "2D/white.tga", 0, 4) : NULL;
	img = operator_new(sizeof(CPsaImage));
	self->focusImg = img ? CPsaImage_ctorTex(img, "2D/white.tga", 0, 4) : NULL;
	self->highLightBlend = 3;
	return self;
}

static void Button_FreeImage(CPsaImage **img) {
	if (*img) {
		CPsaImage_dtor(*img);
		operator_delete_sized(*img, sizeof(CPsaImage));
		*img = NULL;
	}
}

void __fastcall cButtonScorer_dtor(cButtonScorer *self) {
	self->base.base.vftable = CBUTTONSCORER_VFTABLE;
	Button_FreeImage(&self->alphaImg);
	Button_FreeImage(&self->focusImg);
	ByteTable_Free(&self->isValue);
	StrTable_Free(&self->lines);
	cSpriteScorer_dtor(&self->base);
}

cButtonScorer *__thiscall cButtonScorer_Delete(cButtonScorer *self, byte flags) {
	cButtonScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cButtonScorer));
	}
	return self;
}

/* ---- the text ---- */

int __thiscall cButtonScorer_WrapLines(cButtonScorer *self, char **out, const char *text) {
	char buf[0x500];
	float rect[2], dim[2];
	const char *start = text, *cur = text, *sp;
	int count = 0;
	byte done;

	memset(buf, 0, sizeof(buf));
	do {
		sp = strchr(cur, ' ');
		if (!sp) {
			sp = strchr(cur, 0);
		}
		size_t n = (size_t)(sp - start);
		if (n >= sizeof(buf)) {
			crt_report_rangecheckfailure();
		}
		strncpy(buf, start, n);
		buf[n] = 0;
		Font_TextSize(self->font, rect, dim, self->base.base.x, self->base.base.y, "%s", buf);
		done = 0;
		if (!*sp) {
			/* the last word: a line of everything, or of the words before it if it does not fit */
			done = 1;
			if (!((float)(self->maxLineSize + 75) >= dim[0])) {
				done = 0;
			} else {
				cur = sp;
			}
		}
		if (dim[0] > (float)(self->maxLineSize + 75) || done) {
			strncat(*out, start, (size_t)(cur - start));
			char *end = *out + strlen(*out);
			end[0] = '\n';
			end[1] = 0;
			count++;
			start = cur;
		}
		cur = sp + 1;
	} while (!done && sp);
	return count;
}

void __thiscall cButtonScorer_SetFile(cButtonScorer *self, uint32_t flags, const char *name) {
	if (self->focusImg) {
		CPsaImage_dtor(self->focusImg);
		operator_delete_sized(self->focusImg, sizeof(CPsaImage));
	}
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctorFlags(img, name, 0, self->highLightBlend,
			(self->base.discardable ? 0x40000 : 0) | flags) : NULL;
	self->focusImg = img;
	img->color = 0xffffffff;
}

/* a copy of n characters of src as line i of the button */
static void Button_SetLine(cButtonScorer *self, uint32_t i, const char *src, size_t n, byte value) {
	char *tmp = operator_new_array(n + 1);
	strncpy(tmp, src, n);
	tmp[n] = 0;
	cString_Set(TABLE_AT(self->lines, i), tmp);
	*TABLE_AT(self->isValue, i) = value;
	operator_delete_array(tmp);
}

int __thiscall cButtonScorer_SetText(cButtonScorer *self, const char *text) {
	size_t len = strlen(text);
	CHECK(len < 1024 * 5);
	char *buf = operator_new_array(0x1400);
	*buf = 0;
	if (self->maxLineSize > -1) {
		cButtonScorer_WrapLines(self, &buf, text);
	} else {
		if (len >= 0x1400) {
			len = 0x13ff;
		}
		memcpy(buf, text, len);
		buf[len] = 0;
	}
	uint32_t numLines = 1, numColons = 0;
	for (const char *p = strchr(buf, '\n'); p; p = strchr(p + 1, '\n')) {
		numLines++;
	}
	const char *p;
	if (self->isNameValuePair && (p = strchr(buf, ':'))) {
		do {
			numLines++;
			numColons++;
			p = strchr(p + 1, ':');
		} while (p);
		if (numColons) {
			CHECK((float)(double)numLines * 0.5f == (float)(double)numColons);
		}
	}
	self->textWidth = -1.0f;
	self->textHeight = -1.0f;
	int result = 0;
	StrTable_Init(&self->lines, numLines);
	CHECK(!self->isValue.items && (int)numLines > 0);
	self->isValue.count = numLines;
	self->isValue.items = operator_new_array(numLines);
	CHECK(self->isValue.items);

	const char *line = buf;
	float rect[2], dim[2], dim2[2];
	for (uint32_t k = 0; k < numLines; k++) {
		const char *nl = strchr(line, '\n');
		if (!nl) {
			nl = strchr(line, 0);
		}
		float w;
		const char *colon;
		if (self->isNameValuePair && (colon = strchr(line, ':'))) {
			Button_SetLine(self, k, line, (size_t)(colon - line) + 1, 0);
			Font_TextSize(self->font, rect, dim, self->base.base.x, self->base.base.y, "%s",
				*TABLE_AT(self->lines, k));
			k++;
			Button_SetLine(self, k, colon + 1, (size_t)(nl - colon) - 1, 1);
			Font_TextSize(self->font, rect, dim2, self->base.base.x, self->base.base.y, "%s",
				*TABLE_AT(self->lines, k));
			w = dim2[0] + dim[0];
		} else {
			Button_SetLine(self, k, line, (size_t)(nl - line), 0);
			Font_TextSize(self->font, rect, dim, self->base.base.x, self->base.base.y, "%s",
				*TABLE_AT(self->lines, k));
			w = dim[0];
		}
		if (w > self->textWidth) {
			self->textWidth = w;
			self->textHeight = dim[1];
		}
		result++;
		line = nl + 1;
	}
	operator_delete_sized(buf, 1);
	return result;
}

/* ---- Var ---- */

int __thiscall cButtonScorer_Var(cButtonScorer *self, const char *name, byte set, char **s, double *d, int *i) {
	cScorerItem *it = &self->base.base;
	int r;

	if (d) {
		if (!crt_stricmp(name, "ScaleSmoothWidth")) {
			if (set) {
				float v = (float)*d;
				self->scaleSmoothWidth = v;
				CHECK(v > self->origScaleX);
			} else {
				*d = (double)self->scaleSmoothWidth;
			}
			return 2;
		}
		if (!crt_stricmp(name, "ScaleSmoothHeight")) {
			if (set) {
				float v = (float)*d;
				self->scaleSmoothHeight = v;
				CHECK(v > self->origScaleY);
			} else {
				*d = (double)self->scaleSmoothHeight;
			}
			return 2;
		}
		if (!crt_stricmp(name, "ScaleSmoothTime")) {
			return ScorerVar_Float(&self->scaleSmoothTime, set, d);
		}
		if (!crt_stricmp(name, "XAlignRight")) {
			ScorerVar_Float(&self->xAlignRight, set, d);
			CHECK(self->hasText);
			it->x = self->xAlignRight - it->w;
			return 2;
		}
		if (!crt_stricmp(name, "XAlignLeft")) {
			ScorerVar_Float(&self->xAlignLeft, set, d);
			CHECK(self->hasText);
			it->x = self->xAlignLeft;
			return 2;
		}
		if (!crt_stricmp(name, "XAlignCenter")) {
			ScorerVar_Float(&self->xAlignCenter, set, d);
			CHECK(self->hasText);
			it->x = self->xAlignCenter - it->w * 0.5f;
			return 2;
		}
	}
	if (i) {
		if (!crt_stricmp(name, "Highlight")) {
			if (set) {
				self->highlight = *i;
				if (self->highlight) {
					self->glow = 0;
				}
			} else {
				*i = self->highlight;
			}
			return 3;
		}
		if (!crt_stricmp(name, "Glow")) {
			if (set) {
				self->glow = *i;
				if (self->glow) {
					self->highlight = 0;
				}
			} else {
				*i = self->glow;
			}
			return 3;
		}
	}
	if (d && !crt_stricmp(name, "HighlightOffset")) {
		return ScorerVar_Float(&self->highlightOffset, set, d);
	}
	if (i && !crt_stricmp(name, "Marker")) {
		return ScorerVar_Int(&self->marker, set, i);
	}
	if (d && !crt_stricmp(name, "MarkerOffset")) {
		return ScorerVar_Float(&self->markerOffset, set, d);
	}
	if (set && s && !crt_stricmp(name, "Font")) {
		CFont *font = Font_Get(*s);
		if (font) {
			self->font = font;
			return 0;
		}
		return 1;
	}
	if (i) {
		if (!crt_stricmp(name, "IsNameValuePair")) {
			return ScorerVar_Int(&self->isNameValuePair, set, i);
		}
		if (!crt_stricmp(name, "MaxLineSize")) {
			return ScorerVar_Int(&self->maxLineSize, set, i);
		}
	}
	if (s && !crt_stricmp(name, "Text")) {
		if (set) {
			StrTable_Free(&self->lines);
			ByteTable_Free(&self->isValue);
			int n = cButtonScorer_SetText(self, *s);
			float cells = (self->textWidth - 20.0f) / 10.0f;
			self->hasText = 1;
			int cw = (int)crt_ceil((double)cells);
			self->width = cw + 1;
			float rows = ((float)(double)(uint32_t)n * self->textHeight - 20.0f) / 10.0f;
			int ch = (int)crt_ceil((double)rows);
			float lh = self->textHeight;
			self->height = ch;
			self->boxH = (ch + 2) * 10;
			self->textH = ((int)lh + 2) * n - 2;
			if (self->boxH <= self->textH) {
				self->height = ch + 1;
				self->boxH = ((ch + 1) * 5 + 10) * 2;
			}
			self->lineHeight = lh;
			it->w = (float)((self->width + 2) * 10);
			it->h = (float)((self->height + 2) * 10);
			return 1;
		}
		/* the lines joined by spaces; the original returns the buffer of the destroyed string */
		cString all;
		cString_ctor(&all);
		cString_Assign(&all, StrTable_At(&self->lines, 0));
		int n = self->lines.count;
		for (int k = 1; k < n; k++) {
			cString_AppendStr(&all, " ");
			cString_AppendStr(&all, *StrTable_At(&self->lines, k));
		}
		*s = all;
		cString_dtor(&all);
		return 1;
	}
	if (i) {
		if (!crt_stricmp(name, "ForceWidth")) {
			self->width = *i / 10 - 2;
			return 3;
		}
		if (!crt_stricmp(name, "ForceHeight")) {
			self->height = *i / 10 - 2;
			return 3;
		}
	}
	if (s && !crt_stricmp(name, "TextAlign")) {
		if (set) {
			if (!crt_stricmp(*s, "Left")) {
				self->textAlign = BUTTON_LEFT;
			} else if (!crt_stricmp(*s, "Center")) {
				self->textAlign = BUTTON_CENTER;
			} else if (!crt_stricmp(*s, "Right")) {
				self->textAlign = BUTTON_RIGHT;
			}
		}
		return 1;
	}
	if (i) {
		if ((r = ScorerVar_Color(name, s_TextColors, &self->textColor, set, i))) {
			return r;
		}
		if (!crt_stricmp(name, "FocusSpriteIndex")) {
			return ScorerVar_Int(&self->focusSpriteIndex, set, i);
		}
		if (!crt_stricmp(name, "RenderAlphaChannel")) {
			return ScorerVar_Int(&self->renderAlphaChannel, set, i);
		}
		if (!crt_stricmp(name, "BorderSpriteIndex")) {
			return ScorerVar_Int(&self->borderSpriteIndex, set, i);
		}
		for (int k = 0; k < 3; k++) {
			if (!crt_stricmp(name, s_BorderColors[k])) {
				return ScorerVar_ColorByte(&self->borderColor, 16 - k * 8, set, i);
			}
		}
		if (!crt_stricmp(name, "Complex")) {
			return ScorerVar_Int(&self->complex, set, i);
		}
		for (int k = 0; k < 3; k++) {
			if (!crt_stricmp(name, s_HighLightColors[k])) {
				return ScorerVar_ColorByte(&self->highLightColor, 16 - k * 8, set, i);
			}
		}
	}
	if (d) {
		if (!crt_stricmp(name, "MinX")) {
			return ScorerVar_Float(&self->minX, set, d);
		}
		if (!crt_stricmp(name, "MaxX")) {
			return ScorerVar_Float(&self->maxX, set, d);
		}
	}
	/* the next two go on to the base class */
	if (i && !crt_stricmp(name, "HighLightBlendingMode") && set) {
		self->highLightBlend = *i;
	}
	if (s && !crt_stricmp(name, "File") && set) {
		uint32_t flags = self->base.highRes ? 0x10020 : 0x20;
		if (self->base.isMultiSprite) {
			flags |= 0x40;
		}
		cButtonScorer_SetFile(self, flags, *s);
	}
	if (d) {
		if (!crt_stricmp(name, "SizeX") && !set) {
			if (!self->isNameValuePair) {
				*d = (double)self->textWidth;
			} else {
				*d = (double)((self->maxX - it->x) - (float)(self->boxH - self->textH));
			}
			return 2;
		}
		if (!crt_stricmp(name, "SizeY") && !set) {
			*d = (double)((self->lineHeight + 2.0f) * (float)self->lines.count);
			return 2;
		}
	}
	return cSpriteScorer_Var(&self->base, name, set, s, d, i);
}

/* ---- drawing ---- */

/* the box of cells around the text: corners, edges and the middle (Complex) */
static void Button_DrawBox(cButtonScorer *self) {
	cSpriteScorer *sp = &self->base;
	const int *t = &s_BoxFrames[(sp->mirror & 1) * 9];
	float x = sp->base.x, y = sp->base.y;

	CPsaImage_DrawFrame(sp->img, sp->spriteIndex + t[0], x, y);
	CPsaImage_DrawFrame(sp->img, sp->spriteIndex + t[2], (float)((self->width + 1) * 10) + sp->base.x, sp->base.y);
	CPsaImage_DrawFrame(sp->img, sp->spriteIndex + t[6], sp->base.x, (float)((self->height + 1) * 10) + sp->base.y);
	CPsaImage_DrawFrame(sp->img, sp->spriteIndex + t[8], (float)((self->width + 1) * 10) + sp->base.x,
		(float)((self->height + 1) * 10) + sp->base.y);
	for (int k = 0, off = 0; k < self->height; k++, off += 10) {
		CPsaImage_DrawFrame(sp->img, sp->spriteIndex + t[3], sp->base.x, (sp->base.y + 10.0f) + (float)off);
		CPsaImage_DrawFrame(sp->img, sp->spriteIndex + t[5], ((float)(self->width * 10) + sp->base.x) + 10.0f,
			(sp->base.y + 10.0f) + (float)off);
	}
	for (int k = 0, off = 0; k < self->width; k++, off += 10) {
		CPsaImage_DrawFrame(sp->img, sp->spriteIndex + t[1], (sp->base.x + 10.0f) + (float)off, sp->base.y);
		CPsaImage_DrawFrame(sp->img, sp->spriteIndex + t[7], (sp->base.x + 10.0f) + (float)off,
			(float)((self->height + 1) * 10) + sp->base.y);
	}
	if (self->height > 0) {
		float fx = sp->base.x + 10.0f;
		float endX = (float)(self->width * 10) + fx;
		float endY = (float)((self->height + 1) * 10) + sp->base.y;
		if (endX > fx) {
			do {
				float fy = sp->base.y + 10.0f;
				while (endY > fy) {
					CPsaImage_DrawFrame(sp->img, sp->spriteIndex + t[4], fx, fy);
					fy = fy + 10.0f;
				}
				fx = fx + 10.0f;
			} while (endX > fx);
		}
	}
}

static void Button_DrawText(cButtonScorer *self) {
	cScorerItem *it = &self->base.base;
	int n = self->lines.count;
	int row = 0;
	float boxW = (float)(self->width + 2) * 10.0f;

	for (int k = 0; k < n; k++) {
		float tx, ty;
		uint32_t align;
		int mul;
		switch (self->textAlign) {
		case BUTTON_RIGHT:
			ty = (float)(self->boxH - self->textH) * 0.5f + it->y;
			tx = ((boxW - self->textWidth) * 0.5f + it->x) + self->textWidth;
			align = 1;
			mul = k;
			break;
		case BUTTON_CENTER:
			tx = boxW * 0.5f + it->x;
			ty = ((float)((self->boxH - self->textH) / 2) + it->y) + self->lineHeight * 0.5f;
			align = 4;
			mul = k;
			break;
		case BUTTON_LEFT:
			ty = (float)(self->boxH - self->textH) * 0.5f + it->y;
			if (!*TABLE_AT(self->isValue, k)) {
				if (self->isNameValuePair) {
					tx = self->minX;
					if (!(tx >= 0.0f)) {
						tx = (float)(self->boxH - self->textH) + it->x;
					}
					CFont_PrintfAlignColorShadow(self->font, tx, (self->lineHeight + 2.0f) * (float)row + ty, 0,
						self->textColor, *TABLE_AT(self->lines, k));
					if (!self->isNameValuePair) {
						row++;
					}
					continue;
				}
			} else if (self->isNameValuePair) {
				tx = self->maxX;
				if (!(tx >= 0.0f)) {
					tx = ((boxW - self->textWidth) * 0.5f + it->x) + self->textWidth;
				}
				CFont_PrintfAlignColorShadow(self->font, tx, (self->lineHeight + 2.0f) * (float)row + ty, 1,
					self->textColor, *TABLE_AT(self->lines, k));
				row++;
				continue;
			}
			tx = (boxW - self->textWidth) * 0.5f + it->x;
			CFont_PrintfAlignColorShadow(self->font, tx, (self->lineHeight + 2.0f) * (float)row + ty, 0,
				self->textColor, *TABLE_AT(self->lines, k));
			row++;
			continue;
		default:
			continue;
		}
		CFont_PrintfAlignColorShadow(self->font, tx, (self->lineHeight + 2.0f) * (float)mul + ty, align,
			self->textColor, *TABLE_AT(self->lines, k));
	}
}

/* the scale of the focused sprite towards ScaleSmoothWidth/Height and back */
static void Button_SmoothScale(cButtonScorer *self, byte focused) {
	CPsaImage *img = self->base.img;

	if (focused && self->scaleSmoothTime > 0.0f) {
		float ft = Scorer_Get(0)->frameTime;
		float a = (self->scaleSmoothWidth - self->origScaleX) / self->scaleSmoothTime;
		float b = (self->scaleSmoothHeight - self->origScaleY) / self->scaleSmoothTime;
		img->scaleX = a * ft + img->scaleX;
		img->scaleY = img->scaleY + b * ft;
		if (img->scaleX >= self->scaleSmoothWidth) {
			img->scaleX = self->scaleSmoothWidth;
		}
		if (img->scaleY >= self->scaleSmoothHeight) {
			img->scaleY = self->scaleSmoothHeight;
		}
	} else if (self->scaleSmoothTime > 0.0f) {
		float ft = Scorer_Get(0)->frameTime;
		float a = (self->origScaleX - self->scaleSmoothWidth) / self->scaleSmoothTime;
		float b = (self->origScaleY - self->scaleSmoothHeight) / self->scaleSmoothTime;
		img->scaleX = a * ft + img->scaleX;
		img->scaleY = img->scaleY + b * ft;
		if (self->origScaleX >= img->scaleX) {
			img->scaleX = self->origScaleX;
		}
		if (self->origScaleY >= img->scaleY) {
			img->scaleY = self->origScaleY;
		}
	}
}

/* the sprite without a text: scaled when focused, a white or black copy behind for the alpha, the border frame */
static void Button_DrawSprite(cButtonScorer *self, int player) {
	cSpriteScorer *sp = &self->base;
	float tx = sp->base.x, ty = sp->base.y;

	if (sp->fixPosition) {
		ty = sp->img->pivotY * sp->base.h + ty;
		tx = sp->img->pivotX * sp->base.w + tx;
	}
	cScorer *sc = Scorer_Get(player);
	Button_SmoothScale(self, !cString_ICmp(&sp->base.name, &sc->current));
	if (self->renderAlphaChannel == 1) {
		self->alphaImg->pivotX = sp->img->pivotX;
		self->alphaImg->pivotY = sp->img->pivotY;
		self->alphaImg->scaleY = sp->img->scaleY * sp->base.h;
		self->alphaImg->scaleX = sp->img->scaleX * sp->base.w;
		self->alphaImg->color = 0xffffffff;
		CPsaImage_Draw(self->alphaImg, tx, ty);
		sp->img->color = 0xff000000;
	} else if (self->renderAlphaChannel == 2) {
		self->alphaImg->pivotX = sp->img->pivotX;
		self->alphaImg->pivotY = sp->img->pivotY;
		self->alphaImg->scaleX = sp->base.w * sp->img->scaleX;
		self->alphaImg->scaleY = sp->img->scaleY * sp->base.h;
		self->alphaImg->color = 0xff000000;
		CPsaImage_Draw(self->alphaImg, tx, ty);
	}
	CPsaImage_DrawFrameFlip(sp->img, sp->spriteIndex, tx, ty, sp->rotate, sp->mirror);
	if (self->borderSpriteIndex != -1) {
		CPsaImage *f = self->focusImg;
		float px = f->pivotX, py = f->pivotY, sx = f->scaleX, sy = f->scaleY;
		uint32_t color = f->color;
		f->color = self->borderColor;
		self->focusImg->pivotX = 0.5f;
		self->focusImg->pivotY = 0.5f;
		self->focusImg->scaleX = sp->img->scaleX;
		self->focusImg->scaleY = sp->img->scaleY;
		CPsaImage_DrawFrameFlip(self->focusImg, self->borderSpriteIndex, tx, ty, sp->rotate, sp->mirror);
		self->focusImg->color = color;
		self->focusImg->pivotX = px;
		self->focusImg->pivotY = py;
		self->focusImg->scaleX = sx;
		self->focusImg->scaleY = sy;
	}
}

/* the alpha of the pulsing highlight and glow */
static uint32_t Button_PulseAlpha(const cScorer *sc) {
	int v = Scorer_Abs((int)((double)sc->time * 680.0) % 255 - 127);
	return (uint32_t)(byte)(v - 0x80) << 24;
}

void __thiscall cButtonScorer_Draw(cButtonScorer *self, int player) {
	cSpriteScorer *sp = &self->base;

	if (self->hasText) {
		if (sp->isMultiSprite) {
			if (self->complex) {
				Button_DrawBox(self);
			} else {
				CPsaImage_DrawFrameFlip(sp->img, sp->spriteIndex, sp->base.x, sp->base.y, sp->rotate, sp->mirror);
			}
		}
		Button_DrawText(self);
	} else {
		if (!sp->isMultiSprite) {
			return;
		}
		if (self->marker) {
			if (self->complex) {
				float mo;
				CPsaImage_DrawFrame(sp->img, sp->spriteIndex, sp->base.x, sp->base.y);
				CPsaImage_DrawFrame(sp->img, sp->spriteIndex + 1, (sp->base.w + sp->base.x) - self->markerOffset,
					sp->base.y);
				CPsaImage_DrawFrame(sp->img, sp->spriteIndex + 2, sp->base.x,
					(sp->base.h + sp->base.y) - self->markerOffset);
				mo = self->markerOffset;
				CPsaImage_DrawFrame(sp->img, sp->spriteIndex + 3, (sp->base.w + sp->base.x) - mo,
					(sp->base.h + sp->base.y) - mo);
			} else {
				CPsaImage_DrawFrameFlip(sp->img, sp->spriteIndex, sp->base.x, sp->base.y, sp->rotate, sp->mirror);
			}
		} else {
			Button_DrawSprite(self, player);
		}
	}
	if (!sp->isMultiSprite) {
		return;
	}
	cScorer *sc = Scorer_Get(player);
	if (!cString_ICmp(&sp->base.name, &sc->current)) {
		if (!self->highlight) {
			return;
		}
		CPsaImage *f = self->focusImg;
		uint32_t color = f->color;
		f->scaleX = sp->img->scaleX;
		f->scaleY = sp->img->scaleY;
		self->focusImg->color = (self->highLightColor & 0xffffff) | Button_PulseAlpha(sc);
		if (!self->complex) {
			CPsaImage_DrawFrameFlip(self->focusImg, self->focusSpriteIndex, sp->base.x, sp->base.y, sp->rotate,
				sp->mirror);
			self->focusImg->color = color;
			return;
		}
		const int *t = &s_FocusCorners[(sp->mirror & 1) * 4];
		float px = self->focusImg->pivotX, py = self->focusImg->pivotY;
		self->focusImg->pivotX = 0.5f;
		self->focusImg->pivotY = 0.5f;
		float dx = (self->focusImg->scaleX - self->origScaleX) * (sp->base.w * 0.5f) + self->highlightOffset;
		float dy = (self->focusImg->scaleY - self->origScaleY) * (sp->base.h * 0.5f) + self->highlightOffset;
		CPsaImage_DrawFrame(self->focusImg, t[0] + self->focusSpriteIndex, sp->base.x - dx, sp->base.y - dy);
		CPsaImage_DrawFrame(self->focusImg, t[1] + self->focusSpriteIndex, (dx + sp->base.x) + sp->base.w,
			sp->base.y - dy);
		CPsaImage_DrawFrame(self->focusImg, t[2] + self->focusSpriteIndex, sp->base.x - dx,
			(dy + sp->base.y) + sp->base.h);
		CPsaImage_DrawFrame(self->focusImg, t[3] + self->focusSpriteIndex, (dx + sp->base.x) + sp->base.w,
			(dy + sp->base.y) + sp->base.h);
		self->focusImg->pivotX = px;
		self->focusImg->pivotY = py;
		self->focusImg->color = color;
	} else if (self->glow) {
		CPsaImage *img = sp->img;
		uint32_t color = img->color;
		img->color = Button_PulseAlpha(sc) | (self->highLightColor & 0xffffff);
		CPsaImage_DrawFrameFlip(sp->img, self->focusSpriteIndex, sp->base.x, sp->base.y, sp->rotate, sp->mirror);
		img->color = color;
	}
}
