/*
 * TalkScorer.cpp of the game (Menu/): cTalkScorer, the talk text of Scorer_SetSpeechText scrolling above the
 * character, the use text of the character in front of it, and the talk functions of SScorer. See
 * include/scrap/scorerhud.h.
 */
#include <string.h>

#include "scrap/arrowindicator.h"
#include "scrap/chartype.h"
#include "scrap/logic.h"
#include "scrap/scorerhud.h"
#include "scrap/scrapmain.h"
#include "scrap/staticinit.h"

#define CTALKSCORER_VFTABLE (&cTalkScorer_vftable.base)
#define g_TalkWidth (GVar_MaxTalkLineSizePix.value.f)		/* the width of the lines */
#define g_TalkUseTime (GVar_TimeToAbortChatText.value.f)	/* the hold time of the use that skips a talk */
#define g_TalkTime (GVar_FullTalkTime.value.f)		/* the time of the whole talk */
static const cTalkScorerVtbl cTalkScorer_vftable;
static cGVar GVar_TimeToAbortChatText;
static cGVar GVar_MaxTalkLineSizePix;
static cGVar GVar_FullTalkTime;

/* the use action of the walk action set (3) */
#define TALK_ACTION_SET 3
#define TALK_ACTION_USE 0x11

static int Talk_Wrap(char *text, char **lines) {
	return Str_WrapText(text, lines, g_TalkWidth, TALK_MAX_LINES, "Horatio");
}

/* the talk of Scorer_SetSpeechText */
static char g_TalkText[6400];
static int g_TalkNumLines;
static char *g_TalkLines[40];

cTalkScorer *cTalkScorer_ctor(cTalkScorer *self) {
	float dim[2];

	cScorerItem_ctor(&self->base);
	self->base.vftable = CTALKSCORER_VFTABLE;
	self->sprite = NULL;
	self->font = Font_Get("Horatio");
	self->maxLines = 3;
	Font_TextSize(self->font, NULL, dim, 0.0f, 0.0f, "W");
	self->lineHeight = dim[1];
	self->useText[0] = 0;
	memset(self->useLines, 0, sizeof(self->useLines));
	self->numUseLines = 0;
	if (g_RemainTalkTime > 0.0f && g_TalkText[0]) {
		/* a talk running: its lines joined again and wrapped once more */
		if (g_TalkNumLines > 1) {
			for (int k = 0; k < g_TalkNumLines - 1; k++) {
				CHECK(g_TalkLines[k]);
				char *line = g_TalkLines[k];
				size_t len = strlen(line);
				if (line[len] == 0) {
					line[len] = ' ';
				}
			}
		}
		g_TalkNumLines = Talk_Wrap(g_TalkText, g_TalkLines);
	}
	self->useTime = 0.0f;
	self->useIndex = -1;
	self->nextIndex = -1;
	self->nextBackIndex = -1;
	self->tabVisible = 0;
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	/* the color stays the one of the image */
	self->tab = img ? CPsaImage_ctor(img, "2D/white.tga", 0x20) : NULL;
	self->tabSpriteIndex = 0;
	self->tabBlend = 0;
	return self;
}

static void cTalkScorer_dtor(cTalkScorer *self) {
	self->base.vftable = CTALKSCORER_VFTABLE;
	ScorerImg_Free(self->sprite);
	ScorerImg_Free(self->tab);
	cScorerItem_dtor(&self->base);
}

static cScorerItem *cTalkScorer_Delete(cScorerItem *base, byte flags) {
	cTalkScorer *self = (cTalkScorer *)base;
	cTalkScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cTalkScorer));
	}
	return base;
}

/* the character of the player in front of its character (the use target), NULL if none */
static cEntity *Talk_GetUseTarget(int player) {
	if (Input_GetActionSet() != TALK_ACTION_SET) {
		return NULL;
	}
	cEntity *cam = World_FindEntity(cWorld_GetCamName(g_pWorld, player));
	if (!cam || !cam->active || !Entity_IsType(cam, TYEN_FCAM)) {
		return NULL;
	}
	cEntity *ch = cCamEntity_GetTarget(cam);
	if (!ch || !Entity_IsType(ch, TYEN_WALKCHAR) || !WChar(ch)->listenOn) {
		return NULL;
	}
	cEntity *t = WChar_Listen(ch);
	if (!t || !Entity_IsType(t, TYEN_WITH_LIFE)) {
		return NULL;
	}
	return WChar(t)->wl.arrow.canUse ? t : NULL;
}

static int cTalkScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cTalkScorer *self = (cTalkScorer *)base;
	static const char *const colors[4] = {"TabRed", "TabGreen", "TabBlue", "TabAlpha"};
	int r;

	if (i) {
		if (!crt_stricmp(name, "NextIndex")) {
			return ScorerVar_Int(&self->nextIndex, set, i);
		}
		if (!crt_stricmp(name, "NextBackIndex")) {
			return ScorerVar_Int(&self->nextBackIndex, set, i);
		}
		if (!crt_stricmp(name, "UseIndex")) {
			return ScorerVar_Int(&self->useIndex, set, i);
		}
	}
	if (s) {
		if (!crt_stricmp(name, "Sprite") && set) {
			self->sprite = ScorerImg_ReplaceSimple(self->sprite, *s, 0x10060);
			return 1;
		}
		if (!crt_stricmp(name, "TabSprite") && set) {
			self->tab = ScorerImg_Replace(self->tab, *s, self->tabBlend, 0x10060);
			return 1;
		}
	}
	if (i) {
		if (!crt_stricmp(name, "TabBlendingMode")) {
			int v = *i;
			CHECK(v >= 0 && v <= 6);
			/* no read back */
			if (set) {
				self->tabBlend = v;
			}
			return 3;
		}
		if (!crt_stricmp(name, "TabSpriteIndex")) {
			return ScorerVar_Int(&self->tabSpriteIndex, set, i);
		}
		if ((r = ScorerVar_Color(name, colors, &self->tab->color, set, i))) {
			return r;
		}
		if (!crt_stricmp(name, "TabVisible")) {
			return ScorerVar_Bool(&self->tabVisible, set, i);
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

/* the use text (vf05) and the talk (vf04) up from the item position */
static void cTalkScorer_Draw(cScorerItem *base, int player) {
	cTalkScorer *self = (cTalkScorer *)base;
	float pos[2];

	pos[0] = self->base.x;
	pos[1] = self->base.y;
	TalkScorer_DrawUse(self, player, pos);
	TalkScorer_DrawTalk(self, player, pos);
}

static cPy_Function g_TalkCallback;

static int g_TalkColor[3] = { 174, 229, 251 };

static void cTalkScorer_DrawTalk(cTalkScorer *self, int player, float *pos) {
	float dt, scroll, w, lh, x, y, hgt;
	int n, maxLines;

	if (!(g_RemainTalkTime > 0.01f)) {
		g_RemainTalkTime = 0.0f;
		self->useTime = g_TalkUseTime;
		if (cPy_Function_IsValid((&g_TalkCallback))) {
			cPy_Function_Execute((&g_TalkCallback), 1);
		}
		return;
	}
	dt = Scorer_Get(player)->frameTime;
	g_RemainTalkTime = g_RemainTalkTime - dt;
	n = g_TalkNumLines;
	maxLines = self->maxLines;
	if (n > maxLines) {
		/* the lines scroll up over the time of the talk */
		float fn = (float)n;
		float fm = (float)maxLines;
		scroll = Float_MaxSS(0.0f, (fn * g_RemainTalkTime) / g_TalkTime - fm * 0.5f);
		if (scroll >= fn - fm) {
			scroll = (float)(n - maxLines);
		}
	} else {
		scroll = 0.0f;
	}
	w = g_TalkWidth;
	lh = self->lineHeight;
	x = pos[0] - w * 0.5f;
	hgt = (float)maxLines * lh;
	y = pos[1] - hgt;
	if (self->tabVisible) {
		float fh = (float)(n > maxLines ? maxLines : n) * lh;
		Menu_DrawFrame(self->tab, self->tabSpriteIndex, x - 10.0f, (pos[1] - fh) - 10.0f, w + 20.0f, fh + 20.0f);
	}
	Eng_SetViewRect(x, y, w, hgt);
	for (int k = 0; k < g_TalkNumLines; k++) {
		uint32_t color = (uint32_t)((g_TalkColor[0] << 8 | g_TalkColor[1]) << 8 | g_TalkColor[2]);
		float l = self->lineHeight;
		CFont_PrintfAlignColor(self->font, pos[0], ((float)(k - g_TalkNumLines) + scroll) * l + (l * 0.5f + pos[1]),
			4, color, "%s", g_TalkLines[k]);
	}
	Eng_ResetViewRect();
	/* holding the use skips the talk */
	if (TABLE_AT(TABLE_AT(g_ActionSets, TALK_ACTION_SET)->actions, TALK_ACTION_USE)->value[player] > 0.5f) {
		float t = self->useTime - dt;
		self->useTime = t;
		if (!(0.0f < t)) {
			g_RemainTalkTime = 0.0f;
		}
		return;
	}
	self->useTime = g_TalkUseTime;
}

static void cTalkScorer_DrawUse(cTalkScorer *self, int player, float *pos) {
	cEntity *t = Talk_GetUseTarget(player);
	cArrowIndicator *arrow;
	const char *opt, *p;
	float x, y, w, h;
	int n, v;

	if (!t) {
		return;
	}
	arrow = &WLife(t)->arrow;
	opt = cArrowIndicator_GetOpt(arrow, arrow->iOpt);
	if (!opt || !(p = strchr(opt, ':'))) {
		return;
	}
	Str_Copy(self->useText, p + 1, TALK_TEXT_SIZE);
	n = Talk_Wrap(self->useText, self->useLines);
	self->numUseLines = n;
	if (n > self->maxLines) {
		n = self->maxLines;
	}
	x = (pos[0] - g_TalkWidth * 0.5f) - 10.0f;
	w = g_TalkWidth + 20.0f;
	h = (float)n * self->lineHeight;
	y = (pos[1] - h) - 10.0f;
	h = h + 20.0f;
	if (self->tabVisible) {
		uint32_t c = self->tab->color;
		self->tab->color = ((c >> 24) - 0x40) << 24 | (c & 0xffffff);
		Menu_DrawFrame(self->tab, self->tabSpriteIndex, x, y, w, h);
		self->tab->color = c;
	}
	v = Scorer_Abs((int)(Scorer_Get(player)->time * 1024.0f) % 0x1ff - 0xff);
	for (int k = 0; k < self->numUseLines; k++) {
		float l = self->lineHeight;
		CFont_PrintfAlignColorShadow(self->font, pos[0], (float)(k - self->numUseLines) * l + (l * 0.5f + pos[1]), 4,
			0xdcebff, "%s", self->useLines[k]);
	}
	if (self->sprite) {
		self->sprite->pivotX = 0.5f;
		self->sprite->pivotY = 0.5f;
		if (arrow->nOpt > 1) {
			/* the arrow to the next option, blinking */
			float ny = h * 0.5f + y;
			float nx = (w + x) - 2.0f;
			self->sprite->color = 0xffffffff;
			CPsaImage_DrawFrame(self->sprite, self->nextBackIndex, nx, ny);
			self->sprite->color = (uint32_t)(v / 2 + 0x7f) << 24 | 0xfbf1ae;
			CPsaImage_DrawFrame(self->sprite, self->nextIndex, nx, ny);
		}
		self->sprite->color = 0x80000000;
		CPsaImage_DrawFrame(self->sprite, self->useIndex, x + 10.0f, y + 8.0f);
		self->sprite->color = 0xfffbf1ae;
		CPsaImage_DrawFrame(self->sprite, self->useIndex, x + 8.0f, y + 6.0f);
	}
	pos[1] = pos[1] - ((float)self->numUseLines * self->lineHeight + 20.0f);
}

void Scorer_CancelSpeech(float time) {
	g_RemainTalkTime = time;
}

static void Talk_AddCallbackParams(PyObject *list);
void Scorer_SetSpeechCallback(const char *func, PyObject *arg) {
	cPy_Function_SetFunc((&g_TalkCallback), func);
	if (arg && arg->ob_type == &PyList_Type) {
		Talk_AddCallbackParams(arg);
	}
}

/* the items of a Python list as the parameters of the talk callback */
static void Talk_AddCallbackParams(PyObject *list) {
	for (int k = 0; k < PyList_Size(list); k++) {
		PyObject *item = PyList_GetItem(list, k);
		PyTypeObject *type = item->ob_type;

		if (type == &PyString_Type) {
			cPy_Function_AddParamS((&g_TalkCallback), PyString_AsString(item));
		} else if (type == &PyFloat_Type) {
			cPy_Function_AddParamF((&g_TalkCallback), (float)PyFloat_AsDouble(item));
		} else if (type == &PyInt_Type) {
			cPy_Function_AddParamI((&g_TalkCallback), (int)PyInt_AsLong(item));
		}
	}
}

void Scorer_SetSpeechText(const char *text, float time, int r, int g, int b) {
	g_RemainTalkTime = 0.0f;
	Taunts_UpdateEndTime(&g_pWorld->logic);
	Str_Copy(g_TalkText, text, TALK_TEXT_SIZE);
	g_TalkNumLines = Talk_Wrap(g_TalkText, g_TalkLines);
	g_TalkColor[0] = r;
	g_TalkColor[1] = g;
	g_RemainTalkTime = time;
	g_TalkTime = time;
	g_TalkColor[2] = b;
}

int Talk_IsShown(void) {
	return g_RemainTalkTime > 0.01f;
}

/* the vtable of cTalkScorer */
static const cTalkScorerVtbl cTalkScorer_vftable = {
	.base.Draw = cTalkScorer_Draw,
	.base.SetParam = cScorerItem_SetParam,
	.base.Var = cTalkScorer_Var,
	.base.Delete = cTalkScorer_Delete,
	.DrawTalk = cTalkScorer_DrawTalk,
	.DrawUse = cTalkScorer_DrawUse,
};

static cGVar GVar_MaxTalkLineSizePix;
static cGVar GVar_TimeToAbortChatText;
cGVar GVar_RemainTalkTime;
static cGVar GVar_FullTalkTime;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_TimeToAbortChatText, &GVar_TimeToAbortChatText, "TimeToAbortChatText", 0, "This time must be keeped press the action button to abort the current chat.", 0.5f)
INIT_GVAR_FLOAT(Init_GVar_MaxTalkLineSizePix, &GVar_MaxTalkLineSizePix, "MaxTalkLineSizePix", 0, "Size at chat window with the text....", 300.0f)
INIT_GVAR_FLOAT(Init_GVar_RemainTalkTime, &GVar_RemainTalkTime, "RemainTalkTime", 0, "Time remain to dispear the tex showed...", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_FullTalkTime, &GVar_FullTalkTime, "FullTalkTime", 0, "Time remain to dispear the tex showed...", 0.0f)
FINI_CALL(Fini_cPy_Function_TalkCallback, &g_TalkCallback, cPy_Function_dtor)
INIT_OBJECT(Init_cPy_Function_TalkCallback, Fini_cPy_Function_TalkCallback, &g_TalkCallback, cPy_Function_ctor)
