/*
 * MissionScorer.cpp of the game (Menu/): cMissionScorer, the mission status icons and the next station text.
 * See include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/scoreritems.h"
#include "scrap/staticinit.h"

#define CMISSIONSCORER_VFTABLE (&cMissionScorer_vftable.base)
#define g_ScorerMarginH (GVar_ScorerMarginH.value.i)		/* ScorerMarginH */
#define g_NextMissionLine1XBox (GVar_NextMissionFirstLineOffsetXBox.value.f)	/* NextMissionFirstLineOffsetXBox */
#define g_NextMissionLine2XBox (GVar_NextMissionSecondLineOffsetXBox.value.f)	/* NextMissionSecondLineOffsetXBox */
#define g_NextMissionLine1PC (GVar_NextMissionFirstLineOffsetPC.value.f)	/* NextMissionFirstLineOffsetPC */
#define g_NextMissionLine2PC (GVar_NextMissionSecondLineOffsetPC.value.f)	/* NextMissionSecondLineOffsetPC */
static const cSpriteScorerVtbl cMissionScorer_vftable;
static cGVar GVar_ScorerMarginH;
static cGVar GVar_NextMissionSecondLineOffsetXBox;
static cGVar GVar_NextMissionSecondLineOffsetPC;
static cGVar GVar_NextMissionFirstLineOffsetXBox;
static cGVar GVar_NextMissionFirstLineOffsetPC;

static const char *const s_TabColors[4] = {"TabRed", "TabGreen", "TabBlue", "TabAlpha"};
static const char *const s_TextColors[3] = {"TextRed", "TextGreen", "TextBlue"};

cMissionScorer *cMissionScorer_ctor(cMissionScorer *self) {
	cSpriteScorer_ctor(&self->base);
	self->base.base.vftable = CMISSIONSCORER_VFTABLE;
	cString_ctorStr(&self->forceMap, "");
	self->font = g_FontHoratio;
	self->nextText = Lang_GetStr("NextMissionMapText");
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	self->tab = img ? CPsaImage_ctor(img, "2D/white.tga", 0x20) : NULL;
	self->tabIndex = 0;
	self->tabBlend = 0;
	return self;
}

static void cMissionScorer_dtor(cMissionScorer *self) {
	CPsaImage *img = self->tab;
	self->base.base.vftable = CMISSIONSCORER_VFTABLE;
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, sizeof(CPsaImage));
	}
	cString_dtor(&self->forceMap);
	cSpriteScorer_dtor(&self->base);
}

static cScorerItem *cMissionScorer_Delete(cScorerItem *base, byte flags) {
	cMissionScorer *self = (cMissionScorer *)base;
	cMissionScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cMissionScorer));
	}
	return base;
}

static const char *g_MissionMapName;

/* Status0 of the scripts: the station text of the mission map, if it is not this level or the orbit */
static void Mission_SetStation(cMissionScorer *self) {
	char buf[0x20];

	memset(buf, 0, sizeof(buf));
	const char *level = g_pApp->levelPath + 7;	/* after "Levels/" */
	const char *map = self->forceMap && *self->forceMap ? self->forceMap : SaveVar_Get("Mission.Map");
	if (!map || !level || !strcmp(map, level) || !strcmp(map, "Orbit")) {
		return;
	}
	size_t n = strlen(map);
	CHECK(n < 24);
	if (n > sizeof(buf) - 9) {
		n = sizeof(buf) - 9;
	}
	memcpy(buf, "Station_", 9);
	memcpy(buf + 8, map, n);
	buf[8 + n] = 0;
	g_MissionMapName = Lang_GetStr(buf);
}

static int g_MissionStatus[3];
static float g_MissionStatusTime[3];

/* Status0..2: the scorer time is taken on every set */
static int Mission_VarStatus(cMissionScorer *self, int k, byte set, int *i) {
	if (!set) {
		*i = g_MissionStatus[k];
		return 3;
	}
	g_MissionStatus[k] = *i;
	g_MissionStatusTime[k] = Scorer_Get(self->base.base.player)->time;
	if (k == 0) {
		if (!g_MissionStatus[0]) {
			g_MissionMapName = NULL;
		} else {
			Mission_SetStation(self);
		}
	}
	return 3;
}

static int g_MissionBlinkOn;
static float g_MissionBlinkTime;
static float g_MissionBlinkLast;
float g_MissionSolidTime;

/* ForceMap (set), BackIndex, PrimaryIndex, SecondaryIndex, BlinkTime, SolidTime, Status0..2, LastStatus (get),
 * StatusClear (set), TabSprite (set), TabBlendingMode, TabSpriteIndex, TabRed/Green/Blue/Alpha, TextRed/Green/Blue */
static int cMissionScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cMissionScorer *self = (cMissionScorer *)base;
	int r;

	if (set && s && !crt_strncmp(name, "ForceMap", 8)) {
		cString_Set(&self->forceMap, *s);
		return 1;
	}
	if (i) {
		if (!crt_stricmp(name, "BackIndex")) {
			return ScorerVar_Int(&self->backIndex, set, i);
		}
		if (!crt_stricmp(name, "PrimaryIndex")) {
			return ScorerVar_Int(&self->primaryIndex, set, i);
		}
		if (!crt_stricmp(name, "SecondaryIndex")) {
			return ScorerVar_Int(&self->secondaryIndex, set, i);
		}
	}
	if (d) {
		if (!crt_stricmp(name, "BlinkTime")) {
			if (set) {
				g_MissionBlinkOn = 0;
				float t = (float)*d;
				g_MissionBlinkTime = t;
				g_MissionBlinkLast = t;
			} else {
				*d = (double)g_MissionBlinkTime;
			}
			return 2;
		}
		if (!crt_stricmp(name, "SolidTime")) {
			return ScorerVar_Float(&g_MissionSolidTime, set, d);
		}
	}
	if (i) {
		if (!crt_stricmp(name, "Status0")) {
			return Mission_VarStatus(self, 0, set, i);
		}
		if (!crt_stricmp(name, "Status1")) {
			return Mission_VarStatus(self, 1, set, i);
		}
		if (!crt_stricmp(name, "Status2")) {
			return Mission_VarStatus(self, 2, set, i);
		}
	}
	if (!set) {
		if (i && !crt_stricmp(name, "LastStatus")) {
			/* the status set last */
			int last = 0;
			float t = 0.0f;
			if (g_MissionStatus[2] && g_MissionStatusTime[2] >= t) {
				last = 2;
				t = g_MissionStatusTime[2];
			}
			if (g_MissionStatus[1] && g_MissionStatusTime[1] >= t) {
				last = 1;
				t = g_MissionStatusTime[1];
			}
			if (g_MissionStatus[0] && g_MissionStatusTime[0] >= t) {
				last = 0;
			}
			*i = last;
			return 3;
		}
	} else if (i && !crt_stricmp(name, "StatusClear")) {
		for (int k = 0; k < MISSION_STATUS; k++) {
			g_MissionStatus[k] = 0;
			g_MissionStatusTime[k] = 0.0f;
		}
		return 3;
	}
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
		for (int k = 0; k < 3; k++) {
			if (!crt_stricmp(name, s_TextColors[k])) {
				return ScorerVar_ColorByte(&self->textColor, 16 - k * 8, set, i);
			}
		}
	}
	return cSpriteScorer_Var(&self->base.base, name, set, s, d, i);
}

static uint32_t g_MissionStatusColors[3] = { 0xffff20, 0xff4020, 0x2577ff };

static float g_MissionStatusOfs[3][2] = { { 23.5f, 40.0f }, { 17.0f, 32.5f }, { 31.0f, 32.5f } };

static void cMissionScorer_Draw(cScorerItem *base, int player) {
	cMissionScorer *self = (cMissionScorer *)base;
	cScorerItem *it = &self->base.base;
	float ft = Scorer_Get(player)->frameTime;
	float solid;

	self->base.img->color = 0xffffffff;
	if (g_MissionSolidTime > 0.0f) {
		self->base.img->color |= 0xff000000;
		solid = g_MissionSolidTime - ft;
		g_MissionSolidTime = solid;
	} else {
		((byte *)&self->base.img->color)[3] = 0x80;
		solid = g_MissionSolidTime;
	}
	if (g_MissionBlinkTime > 0.0f) {
		float bt = g_MissionBlinkTime - ft;
		float since = g_MissionBlinkLast - bt;
		g_MissionBlinkTime = bt;
		int on = g_MissionBlinkOn;
		if (since > 0.15f) {
			on ^= 1;
			g_MissionBlinkLast = bt;
			g_MissionBlinkOn = on;
		}
		if (on) {
			return;
		}
	} else {
		g_MissionBlinkOn = 0;
	}
	if (g_MissionStatus[0] && solid > 0.0f && g_MissionMapName && self->nextText) {
		float tx, ty1, ty2;
		if (g_IsXbox == 1) {
			float w = it->w;
			if (self->base.highRes == 1) {
				w *= 0.5f;
			}
			tx = (((float)g_ScorerMarginH + 6.0f) + w) + 5.0f;
			ty1 = it->y + g_NextMissionLine1XBox;
			ty2 = it->y + g_NextMissionLine2XBox;
		} else {
			tx = (float)g_ScorerMarginH + 6.0f;
			ty1 = it->y + g_NextMissionLine1PC;
			ty2 = it->y + g_NextMissionLine2PC;
		}
		if (self->tabIndex > 0) {
			float rect[2], d1[2], d2[2];
			Font_TextSize(self->font, rect, d1, tx, ty1, "%s", self->nextText);
			Font_TextSize(self->font, rect, d2, tx, ty2, "%s", g_MissionMapName);
			float w = Float_MaxSS(d1[0], d2[0]) + 12.0f;
			float h = (d1[1] + d2[1]) + 2.0f;
			Menu_DrawFrame(self->tab, self->tabIndex, tx - 6.0f, ty1 - 2.0f, w, h);
		}
		CFont_PrintfAlignColor(self->font, tx, ty1, 0, self->textColor, self->nextText);
		CFont_PrintfAlignColor(self->font, tx, ty2, 0, self->textColor, g_MissionMapName);
	} else {
		g_MissionMapName = NULL;
	}
	CPsaImage *img = self->base.img;
	img->pivotX = 0.0f;
	img->pivotY = 0.0f;
	if (self->backIndex) {
		CPsaImage_DrawFrame(self->base.img, self->backIndex, it->x, it->y);
	}
	img = self->base.img;
	img->pivotX = 0.5f;
	img->pivotY = 0.5f;
	for (int k = 0; k < MISSION_STATUS; k++) {
		if (!g_MissionStatus[k]) {
			continue;
		}
		int frame = k == 0 ? self->primaryIndex : self->secondaryIndex;
		float ox = g_MissionStatusOfs[k][0], oy = g_MissionStatusOfs[k][1];
		img = self->base.img;
		img->color = (uint32_t)((byte *)&img->color)[3] << 24 | (g_MissionStatusColors[k] & 0xffffff);
		CPsaImage_DrawFrame(self->base.img, frame, it->x + ox, it->y + oy);
	}
	img = self->base.img;
	img->pivotX = 0.5f;
	img->pivotY = 0.5f;
	img = self->base.img;
	img->color = (uint32_t)((byte *)&img->color)[3] << 24 | 0xc8d2ff;
	CPsaImage_DrawFrame(self->base.img, (g_IsXbox != 0) + 4, it->x + 24.0f, it->y + 16.0f);
}

/* the vtable of cMissionScorer */
static const cSpriteScorerVtbl cMissionScorer_vftable = {
	.base.Draw = cMissionScorer_Draw,
	.base.SetParam = cScorerItem_SetParam,
	.base.Var = cMissionScorer_Var,
	.base.Delete = cMissionScorer_Delete,
	.SetMultiSprite = cSpriteScorer_SetMultiSprite,
};

static cGVar GVar_NextMissionSecondLineOffsetXBox;
static cGVar GVar_NextMissionSecondLineOffsetPC;
static cGVar GVar_NextMissionFirstLineOffsetXBox;
static cGVar GVar_NextMissionFirstLineOffsetPC;
static cGVar GVar_ScorerMarginH;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_NextMissionFirstLineOffsetPC, &GVar_NextMissionFirstLineOffsetPC, "NextMissionFirstLineOffsetPC", 0, "Next mission first line offset PC", 60.0f)
INIT_GVAR_FLOAT(Init_GVar_NextMissionSecondLineOffsetPC, &GVar_NextMissionSecondLineOffsetPC, "NextMissionSecondLineOffsetPC", 0, "Next mission second line offset PC", 80.0f)
INIT_GVAR_FLOAT(Init_GVar_NextMissionFirstLineOffsetXBox, &GVar_NextMissionFirstLineOffsetXBox, "NextMissionFirstLineOffsetXBox", 0, "Next mission first line offset XBox", 4.0f)
INIT_GVAR_FLOAT(Init_GVar_NextMissionSecondLineOffsetXBox, &GVar_NextMissionSecondLineOffsetXBox, "NextMissionSecondLineOffsetXBox", 0, "Next mission second line offset XBox", 24.0f)
INIT_GVAR_INT(Init_GVar_ScorerMarginH, &GVar_ScorerMarginH, "ScorerMarginH", 0, "Horizontal Margin for Scorer", 48)
