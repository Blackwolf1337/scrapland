/*
 * AbstractTextScorer.cpp of the game (Menu/): the text items cAbstractTextScorer, cCreditsScorer, cHintScorer,
 * cLabelScorer and cMsgScorer, the label and message texts of the scripts. See include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/net.h"
#include "scrap/scoreritems.h"
#include "scrap/staticinit.h"

#define CABSTRACTTEXTSCORER_VFTABLE (&cAbstractTextScorer_vftable)
#define CMSGSCORER_VFTABLE (&cMsgScorer_vftable)
#define CLABELSCORER_VFTABLE (&cLabelScorer_vftable)
#define CHINTSCORER_VFTABLE (&cHintScorer_vftable)
#define CCREDITSSCORER_VFTABLE (&cCreditsScorer_vftable)
#define g_HintAppearsTime (GVar_HintAppearsTime.value.f)	/* HintAppearsTime */
#define g_MissionScorerName (GVar_MissionScorerName.value.s)	/* MissionScorerName, the item hiding the label */
static const cScorerItemVtbl cMsgScorer_vftable;
static const cScorerItemVtbl cLabelScorer_vftable;
static const cScorerItemVtbl cHintScorer_vftable;
static const cScorerItemVtbl cCreditsScorer_vftable;
static const cScorerItemVtbl cAbstractTextScorer_vftable;
static cGVar GVar_MissionScorerName;
static cGVar GVar_HintAppearsTime;

/* the strings Var returns (those of the exe) */
#define STR_LEFT "Left"
#define STR_RIGHT "Right"
#define STR_CENTER "Center"
#define STR_NONE "None"
#define STR_OSCILE "Oscile"
#define STR_SHADOW "Shadow"
#define STR_OUTLINE "Outline"

static const char *const s_NetStates[] = {"Connecting...", NULL, "Waiting server response", "Server timeout",
	"Disconnected from server"};

/* ---- construction ---- */

cAbstractTextScorer *cAbstractTextScorer_ctor(cAbstractTextScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CABSTRACTTEXTSCORER_VFTABLE;
	cString_ctor(&self->fontName);
	cString_ctor(&self->text);
	self->font = Font_Get("Horatio");
	cString_Set(&self->fontName, "Horatio");
	cString_Set(&self->text, "");
	self->color = 0xffffffff;
	self->align = TEXTSCORER_LEFT;
	self->effect = 0;
	return self;
}

cCreditsScorer *cCreditsScorer_ctor(cCreditsScorer *self) {
	cAbstractTextScorer_ctor(&self->base);
	self->base.base.vftable = CCREDITSSCORER_VFTABLE;
	self->font = Font_Get("ScrapMedium");
	self->titleFont = Font_Get("Horatio");
	self->time = 0.0f;
	for (int i = 0; i < CREDITS_LINES; i++) {
		self->lines[i][0] = 0;
	}
	g_CreditsAlpha = 0;
	return self;
}

/* the frame sheet of the hint and the label */
static cHintScorer *TabText_ctor(cHintScorer *self, const cScorerItemVtbl *vftable) {
	cAbstractTextScorer_ctor(&self->base);
	self->base.base.vftable = vftable;
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	self->tab = img ? CPsaImage_ctor(img, "2D/white.tga", 0x20) : NULL;
	self->tabIndex = 0;
	self->tabBlend = 0;
	return self;
}

cHintScorer *cHintScorer_ctor(cHintScorer *self) {
	return TabText_ctor(self, CHINTSCORER_VFTABLE);
}

cLabelScorer *cLabelScorer_ctor(cLabelScorer *self) {
	return TabText_ctor(self, CLABELSCORER_VFTABLE);
}

cMsgScorer *cMsgScorer_ctor(cMsgScorer *self) {
	float dim[2];

	cAbstractTextScorer_ctor(&self->base);
	self->base.base.vftable = CMSGSCORER_VFTABLE;
	Font_TextSize(self->base.font, NULL, dim, 0.0f, 0.0f, "W");
	self->lineHeight = dim[1];
	self->base.align = TEXTSCORER_CENTER;
	self->base.color = 0xffffff00;
	return self;
}

/* also the destructor of cMsgScorer */
void cAbstractTextScorer_dtor(cAbstractTextScorer *self) {
	self->base.vftable = CABSTRACTTEXTSCORER_VFTABLE;
	cString_dtor(&self->text);
	cString_dtor(&self->fontName);
	cScorerItem_dtor(&self->base);
}

static void cCreditsScorer_dtor(cCreditsScorer *self) {
	g_CreditsAlpha = 0;
	cAbstractTextScorer_dtor(&self->base);
}

static void TabText_dtor(cHintScorer *self, const cScorerItemVtbl *vftable) {
	CPsaImage *img = self->tab;
	self->base.base.vftable = vftable;
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, sizeof(CPsaImage));
	}
	cAbstractTextScorer_dtor(&self->base);
}

static void cHintScorer_dtor(cHintScorer *self) {
	TabText_dtor(self, CHINTSCORER_VFTABLE);
}

static void cLabelScorer_dtor(cLabelScorer *self) {
	TabText_dtor(self, CLABELSCORER_VFTABLE);
}

static cScorerItem *cAbstractTextScorer_Delete(cScorerItem *base, byte flags) {
	cAbstractTextScorer *self = (cAbstractTextScorer *)base;
	cAbstractTextScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cAbstractTextScorer));
	}
	return base;
}

static cScorerItem *cCreditsScorer_Delete(cScorerItem *base, byte flags) {
	cCreditsScorer *self = (cCreditsScorer *)base;
	cCreditsScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cCreditsScorer));
	}
	return base;
}

static cScorerItem *cHintScorer_Delete(cScorerItem *base, byte flags) {
	cHintScorer *self = (cHintScorer *)base;
	cHintScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cHintScorer));
	}
	return base;
}

static cScorerItem *cLabelScorer_Delete(cScorerItem *base, byte flags) {
	cLabelScorer *self = (cLabelScorer *)base;
	cLabelScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cLabelScorer));
	}
	return base;
}

static cScorerItem *cMsgScorer_Delete(cScorerItem *base, byte flags) {
	cMsgScorer *self = (cMsgScorer *)base;
	cAbstractTextScorer_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cMsgScorer));
	}
	return base;
}

/* vf01 of cScorerItem, kept in this file: no parameters */
void cScorerItem_SetParam(cScorerItem *self, const char *name, const char *value) {
	(void)self;
	(void)name;
	(void)value;
}

/* ---- Var ---- */

static const char *const s_TextColors[4] = {"Red", "Green", "Blue", "Alpha"};
static const char *const s_TabColors[4] = {"TabRed", "TabGreen", "TabBlue", "TabAlpha"};

int cAbstractTextScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cAbstractTextScorer *self = (cAbstractTextScorer *)base;
	int r;

	if (s && !crt_stricmp(name, "Text")) {
		if (set) {
			cString_Set(&self->text, *s);
		} else {
			*s = self->text;
		}
		return 1;
	}
	if (i && (r = ScorerVar_Color(name, s_TextColors, &self->color, set, i))) {
		return r;
	}
	if (set) {
		if (s && !crt_stricmp(name, "Font")) {
			CFont *font = Font_Get(*s);
			if (!font) {
				return 0;
			}
			self->font = font;
			cString_Set(&self->fontName, *s);
			return 1;
		}
		if (i) {
			int central = !crt_stricmp(name, "CentralText");
			if (central || !crt_stricmp(name, "GetExtens")) {
				if (*i == 1) {
					self->font->align = central ? TEXTSCORER_CENTER : (uint32_t)self->align;
					Font_TextSizeAlign(self->font, &self->base.x, &self->base.w, self->base.x, self->base.y,
						CFONT_ALIGN, "%s", self->text);
				}
				return 3;
			}
		}
	}
	if (s) {
		if (!crt_stricmp(name, "Align")) {
			if (set) {
				if (!crt_stricmp(*s, "Left")) {
					self->align = TEXTSCORER_LEFT;
					return 1;
				}
				if (!crt_stricmp(*s, "Right")) {
					self->align = TEXTSCORER_RIGHT;
					return 1;
				}
				if (!crt_stricmp(*s, "Center")) {
					self->align = TEXTSCORER_CENTER;
					return 1;
				}
			} else if (self->align == TEXTSCORER_LEFT) {
				*s = STR_LEFT;
				return 1;
			} else if (self->align == TEXTSCORER_RIGHT) {
				*s = STR_RIGHT;
				return 1;
			} else if (self->align == TEXTSCORER_CENTER) {
				*s = STR_CENTER;
				return 1;
			}
			return 0;
		}
		if (!crt_stricmp(name, "Effect")) {
			if (set) {
				if (!crt_stricmp(*s, "None")) {
					self->effect = 0;
					return 1;
				}
				if (!crt_stricmp(*s, "Oscile")) {
					self->effect = TEXTSCORER_OSCILE;
					return 1;
				}
				if (!crt_stricmp(*s, "Shadow")) {
					self->effect = TEXTSCORER_SHADOW;
					return 1;
				}
				if (!crt_stricmp(*s, "Outline")) {
					self->effect = TEXTSCORER_OUTLINE;
					return 1;
				}
			} else if (self->effect == TEXTSCORER_SHADOW) {
				*s = STR_SHADOW;
				return 1;
			} else if (self->effect == 0) {
				*s = STR_NONE;
				return 1;
			} else if (self->effect == TEXTSCORER_OSCILE) {
				*s = STR_OSCILE;
				return 1;
			} else if (self->effect == TEXTSCORER_OUTLINE) {
				*s = STR_OUTLINE;
				return 1;
			}
			return 0;
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

/* Time, Life, FadeIn, FadeOut; set only: Text0..Text19 */
static int cCreditsScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cCreditsScorer *self = (cCreditsScorer *)base;
	if (d) {
		if (!crt_stricmp(name, "Time")) {
			return ScorerVar_Float(&self->time, set, d);
		}
		if (!crt_stricmp(name, "Life")) {
			return ScorerVar_Float(&self->life, set, d);
		}
		if (!crt_stricmp(name, "FadeIn")) {
			return ScorerVar_Float(&self->fadeIn, set, d);
		}
		if (!crt_stricmp(name, "FadeOut")) {
			return ScorerVar_Float(&self->fadeOut, set, d);
		}
	}
	if (set && s && !crt_strncmp(name, "Text", 4)) {
		/* Text0..Text19 */
		int n = name[4] - '0';
		if (name[5]) {
			n = name[5] + (n * 5 - 0x18) * 2;
		}
		CHECK(n >= 0 && n < CREDITS_LINES);
		Str_Copy(self->lines[n], *s, CREDITS_LINE_SIZE);
		return 1;
	}
	return cAbstractTextScorer_Var(&self->base.base, name, set, s, d, i);
}

/* also cLabelScorer */
static int cHintScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cHintScorer *self = (cHintScorer *)base;
	int r;

	if (s && !crt_stricmp(name, "TabSprite") && set) {
		if (self->tab) {
			CPsaImage_dtor(self->tab);
			operator_delete_sized(self->tab, sizeof(CPsaImage));
		}
		CPsaImage *img = operator_new(sizeof(CPsaImage));
		img = img ? CPsaImage_ctorFlags(img, *s, 0, self->tabBlend, 0x10060) : NULL;
		self->tab = img;
		img->color = 0xffffffff;
		return 1;
	}
	if (i) {
		if (!crt_stricmp(name, "TabBlendingMode")) {
			int v = *i;
			CHECK(v >= 0 && v <= 6);
			if (set) {
				self->tabBlend = v;
			}
			return 3;
		}
		if (!crt_stricmp(name, "TabSpriteIndex")) {
			return ScorerVar_Int(&self->tabIndex, set, i);
		}
		if ((r = ScorerVar_Color(name, s_TabColors, &self->tab->color, set, i))) {
			return r;
		}
	}
	return cAbstractTextScorer_Var(&self->base.base, name, set, s, d, i);
}

/* ---- drawing ---- */

/* the 20 lines at the position of the phase: fade 0 the scorer, 1 the render target */
static void cCreditsScorer_DrawLines(cCreditsScorer *self, byte fade, uint32_t color) {
	byte columns = self->lines[8][0];
	float sx, sy;

	for (int i = 0; i < CREDITS_LINES; i++) {
		CFont *font;
		float rect[2], dim[2], x, y, yoff;
		const char *line = self->lines[i];

		if (i == 0) {
			font = self->titleFont;
			font->effects = TEXTSCORER_SHADOW;
		} else {
			font = self->font;
			font->effects = TEXTSCORER_OUTLINE;
		}
		font->color = color;
		font->align = 0;
		yoff = (float)(i * 25);
		Font_TextSize(font, rect, dim, 0.0f, 0.0f, "%s", line);
		sx = self->base.base.x;
		sy = self->base.base.y;
		if (fade) {
			x = sx;
			y = sy;
			if (!(65.0f > (float)g_CreditsX)) {
				x = ((float)g_CreditsSizeX - x) - dim[0];
			}
		} else if (columns) {
			int col = i % 3;
			x = sx + 320.0f;
			y = sy + 80.0f;
			yoff = (float)((i + 2) / 3 * 35);
			if (i == 0 || col == 2) {
				x -= dim[0] * 0.5f;
			} else if (col == 1) {
				x -= 300.0f;
			} else {
				x = (x + 300.0f) - dim[0];
			}
		} else {
			float cx = (float)g_CreditsX;
			if (65.0f > cx) {
				x = sx + cx;
				y = sy + (float)g_CreditsY;
			} else {
				x = ((((float)g_CreditsSizeX - sx) - dim[0]) + (sx + cx)) - sx;
				y = sy + (float)g_CreditsY;
			}
		}
		CFont_PrintfFlags(font, x, y + yoff, CFONT_ALIGN | CFONT_COLOR | CFONT_EFFECTS, "%s", line);
	}
}

static void cCreditsScorer_Draw(cScorerItem *base, int player) {
	cCreditsScorer *self = (cCreditsScorer *)base;
	cScorer *sc = Scorer_Get(player);
	float old = self->time;
	float dt = sc->frameTime;
	float t = old + dt;

	self->time = t;
	if (self->lines[8][0]) {
		cCreditsScorer_DrawLines(self, 0, self->base.color);
		return;
	}
	if (dt >= t) {
		g_CreditsAlpha = 0xff;
		g_CreditsClear = 1;
		g_CreditsPhase = 0;
		Eng_RenderTargetBegin("Credits");
		Eng_RenderTargetEnd();
		return;
	}
	if (self->fadeIn > t) {
		g_CreditsAlpha = 0;
		g_CreditsClear = 1;
		g_CreditsPhase = 0;
		float in = self->fadeIn - 0.25f;
		float now = self->time;
		if (now > in) {
			g_CreditsAlpha = 0xff - (int)((now - in) * 4.0f * 255.0f);
			cCreditsScorer_DrawLines(self, 0, self->base.color);
		} else {
			g_CreditsAlpha = (int)(now / in * 255.0f);
		}
		if (Eng_RenderTargetBegin("Credits")) {
			cCreditsScorer_DrawLines(self, 1, self->base.color);
		}
		Eng_RenderTargetEnd();
		return;
	}
	if (t > self->life) {
		if (t > self->fadeOut + self->life) {
			g_CreditsAlpha = 0;
			return;
		}
		g_CreditsClear = 0;
		g_CreditsPhase = 1;
		g_CreditsAlpha = (int)(255.0f - (self->time - self->life) / self->fadeOut * 255.0f);
		if (Eng_RenderTargetBegin("Credits") && self->life >= old) {
			cCreditsScorer_DrawLines(self, 1, self->base.color);
		}
		Eng_RenderTargetEnd();
		return;
	}
	g_CreditsAlpha = 0;
	cCreditsScorer_DrawLines(self, 0, self->base.color);
}

/* the hint text and its two lines */
static char *g_HintLines[2];
static char g_HintText[1280];

static void cHintScorer_Draw(cScorerItem *base, int player) {
	cHintScorer *self = (cHintScorer *)base;
	cScorer *sc = Scorer_Get(player);
	cScorerItem *item = cScorer_FindItem(sc, sc->current);

	if (item && cPy_Function_IsValid(&item->onAccept)) {
		const cString *src;
		if (cPy_Function_IsValid(&sc->onSpecialHint) && cPy_Function_Execute(&sc->onSpecialHint, 0) == 1) {
			src = &sc->specialHint;
		} else {
			src = &item->hint;
		}
		if (strcmp(sc->hint, *src)) {
			cString_Assign(&sc->hint, src);
		}
		sc->hintWidth = item->maxHintLineWidth;
	} else if (*sc->hint) {
		cString_Set(&sc->hint, "");
	}
	const char *hint = sc->hint;
	if (!hint || !*hint || !(sc->time > sc->hintTime + g_HintAppearsTime)) {
		return;
	}
	size_t len = strlen(hint);
	CHECK(len < 0x500);
	if (len >= 0x500) {
		len = 0x4ff;
	}
	memcpy(g_HintText, hint, len);
	g_HintText[len] = 0;

	CFont *font = self->base.font;
	float rect[2], dim[2];
	int n = Str_WrapText(g_HintText, g_HintLines, (float)sc->hintWidth, 2, self->base.fontName);
	Font_TextSize(font, rect, dim, self->base.base.x, self->base.base.y, "%s", g_HintText);
	float lineH = 0.0f, maxW = 0.0f;
	for (int k = 0; k < n; k++) {
		Font_TextSize(font, rect, dim, self->base.base.x, self->base.base.y, "%s", g_HintLines[k]);
		lineH = Float_MaxSS(dim[1], lineH);
		maxW = Float_MaxSS(dim[0], maxW);
	}
	int row = n - 1;
	if (self->tabIndex > 0) {
		float w = maxW + 16.0f;
		float h = (float)n * lineH + 8.0f;
		Menu_DrawFrame(self->tab, self->tabIndex, self->base.base.x - w * 0.5f,
			(self->base.base.y - (float)row * lineH) - 16.0f, w, h);
	}
	for (int k = 0; k < n; k++, row--) {
		CFont_PrintfAlignColorShadow(self->base.font, self->base.base.x, self->base.base.y - (float)row * lineH,
			TEXTSCORER_CENTER, self->base.color, "%s", g_HintLines[k]);
	}
}

/* the label (Scorer_SetLabelText) */
static char g_LabelText[250];
static float g_LabelTime;

static void cLabelScorer_Draw(cScorerItem *base, int player) {
	cLabelScorer *self = (cLabelScorer *)base;
	const char *text;

	if (!g_IsXbox) {
		cScorer *sc = Scorer_Get(player);
		cScorerItem *mission = sc ? cScorer_FindItem(sc, g_MissionScorerName) : NULL;
		if (mission && mission->visible && g_MissionSolidTime >= 0.0f) {
			return;
		}
	}
	if (g_LabelText[0] && (0.0f >= g_LabelTime || g_LabelTime > Timer_GetTime(&g_pWorld->timer))) {
		if (self->tabIndex > 0) {
			float rect[2], dim[2];
			Font_TextSize(self->base.font, rect, dim, self->base.base.x, self->base.base.y, "%s", g_LabelText);
			float w = dim[0] + 12.0f;
			float h = dim[1] + 8.0f;
			Menu_DrawFrame(self->tab, self->tabIndex, self->base.base.x - 6.0f, self->base.base.y - 4.0f, w, h);
		}
		text = g_LabelText;
	} else {
		if (!(g_pApp->flags & APPF_CLIENT)) {
			return;
		}
		uint32_t state = (uint32_t)Net_GetClient()->state;
		if (state > NETCLIENT_DISCONNECTED || !s_NetStates[state]) {
			return;
		}
		text = s_NetStates[state];
	}
	CFont_PrintfAlignColorShadow(self->base.font, self->base.base.x, self->base.base.y, self->base.align,
		self->base.color, "%s", text);
}

static int g_MsgNumLines;
static char *g_MsgLines[20];

static void cMsgScorer_Draw(cScorerItem *base, int player) {
	cMsgScorer *self = (cMsgScorer *)base;
	(void)player;
	if (!(0.0f >= g_MsgTime) && !(g_MsgTime > Timer_GetTime(&g_pWorld->timer))) {
		return;
	}
	for (int k = 0; k < g_MsgNumLines; k++) {
		float lh = self->lineHeight;
		CFont_PrintfAlignColorShadow(self->base.font, self->base.base.x,
			(float)k * lh + (lh * 0.5f + self->base.base.y), self->base.align, self->base.color, "%s", g_MsgLines[k]);
	}
}

/* ---- the texts of the scripts ---- */

void Scorer_SetLabelText(const char *text, float time) {
	Str_Copy(g_LabelText, text, 0xfa);
	g_LabelTime = time;
}

/* the message (Scorer_SetMsgText): the text and its lines */
static char g_MsgText[62500];

void Scorer_SetMsgText(const char *text, float time, const char *font) {
	Str_Copy(g_MsgText, text, 0xf424);
	/* differs from the original on purpose: it allowed 0xfa lines in the 20 slots and wrote over g_LabelText and the
	 * globals after it; ScrapC.exe has no fixed layout, so the lines stop at the slots */
	g_MsgNumLines = Str_WrapText(g_MsgText, g_MsgLines, 260.0f, MSG_LINES, font);
	g_MsgTime = time;
}

/* the vtable of cAbstractTextScorer */
static const cScorerItemVtbl cAbstractTextScorer_vftable = {
	.Draw = (void *)crt_purecall,
	.SetParam = cScorerItem_SetParam,
	.Var = cAbstractTextScorer_Var,
	.Delete = cAbstractTextScorer_Delete,
};

/* the vtable of cMsgScorer */
static const cScorerItemVtbl cMsgScorer_vftable = {
	.Draw = cMsgScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cAbstractTextScorer_Var,
	.Delete = cMsgScorer_Delete,
};

/* the vtable of cLabelScorer */
static const cScorerItemVtbl cLabelScorer_vftable = {
	.Draw = cLabelScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cHintScorer_Var,
	.Delete = cLabelScorer_Delete,
};

/* the vtable of cHintScorer */
static const cScorerItemVtbl cHintScorer_vftable = {
	.Draw = cHintScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cHintScorer_Var,
	.Delete = cHintScorer_Delete,
};

/* the vtable of cCreditsScorer */
static const cScorerItemVtbl cCreditsScorer_vftable = {
	.Draw = cCreditsScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cCreditsScorer_Var,
	.Delete = cCreditsScorer_Delete,
};

static cGVar GVar_MissionScorerName;
cGVar GVar_TimeToMsg;
static cGVar GVar_HintAppearsTime;
cGVar GVar_R_CreditsX;
cGVar GVar_R_CreditsY;
cGVar GVar_R_CreditsClear;
cGVar GVar_R_CreditsAlpha;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_TimeToMsg, &GVar_TimeToMsg, "TimeToMsg", 1, "Tiempo restante hasta que desaparezca el mensaje", 0.0f)
FINI_GVAR_STRING(Fini_GVar_MissionScorerName, &GVar_MissionScorerName)
INIT_GVAR_STRING(Init_GVar_MissionScorerName, Fini_GVar_MissionScorerName, &GVar_MissionScorerName, "MissionScorerName", 1, "Mission scorer name", "Mission")
INIT_GVAR_FLOAT(Init_GVar_HintAppearsTime, &GVar_HintAppearsTime, "HintAppearsTime", 0, "Time to show the hint in the menu", 0.5f)
INIT_GVAR_INT(Init_GVar_R_CreditsX, &GVar_R_CreditsX, "R_CreditsX", 0, "Position X of Credits render target", 0)
INIT_GVAR_INT(Init_GVar_R_CreditsY, &GVar_R_CreditsY, "R_CreditsY", 0, "Position Y of Credits render target", 0)
INIT_GVAR_INT(Init_GVar_R_CreditsClear, &GVar_R_CreditsClear, "R_CreditsClear", 0, "Credits render target clear", 1)
INIT_GVAR_INT(Init_GVar_R_CreditsAlpha, &GVar_R_CreditsAlpha, "R_CreditsAlpha", 0, "Credits render target alpha", 0)
