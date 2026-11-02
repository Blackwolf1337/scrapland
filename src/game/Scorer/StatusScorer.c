/*
 * StatusScorer.cpp of the game (Menu/): cStatusScorer, the life, lives, money and boost of the ship of the camera.
 * See include/scrap/scorerhud.h.
 */
#include "scrap/gvar.h"
#include "scrap/crt.h"
#include "scrap/logic.h"
#include "scrap/scorerhud.h"
#include "scrap/scrap.h"
#include "scrap/sound.h"
#include "scrap/vehicleentity.h"
#include "scrap/withlifeentity.h"

/* a byte of the ship regeneration state of the world (SLogic.SetShipToRegenerate): the siren plays while set */

static CPsaImage *Status_NewWhite(void) {
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x20) : NULL;
	img->color = 0xffffffff;
	return img;
}

cStatusScorer *cStatusScorer_ctor(cStatusScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CSTATUSSCORER_VFTABLE;
	cSound_Init(&self->lowLife);
	cSound_Init(&self->alarm);
	self->numLives = 0;
	self->base.x = 320.0f;
	self->base.y = 240.0f;
	self->lifeFont = g_FontHoratio;
	self->lifeTextColor = 0xffffffff;
	self->livesFont = g_FontHoratio;
	self->livesTextColor = 0xffffffff;
	self->moneyFont = g_FontHoratio;
	self->moneyTextColor = 0xffffffff;
	self->img = Status_NewWhite();
	self->glow = Status_NewWhite();
	self->flag = Status_NewWhite();
	cSound_SetSound(&self->lowLife, g_StatusLowLifeSound, 0);
	self->lowLife.desc.loop = 1;
	cSound_SetSound(&self->alarm, g_StatusAlarmSound, 0);
	self->alarm.desc.loop = 1;
	self->alarmOn = g_pWorld->logic.shipRegen.noLoss;
	self->lifeIconIndex = 0;
	self->hullIconIndex = 0;
	self->moneyIconIndex = 0;
	self->money = g_pWorld->money;
	self->lastMoney = g_pWorld->money;
	self->moneyBlink = 0.0f;
	self->boostIconIndex = 0;
	self->boostTurboIndex = 0;
	self->boostTime = 1.0f;
	self->backColor = 0x5cffcc00;
	self->textBackColor = 0xffffffff;
	return self;
}

void cStatusScorer_dtor(cStatusScorer *self) {
	self->base.vftable = CSTATUSSCORER_VFTABLE;
	ScorerImg_Free(self->img);
	ScorerImg_Free(self->glow);
	ScorerImg_Free(self->flag);
	cSound_Stop(&self->alarm);
	cSound_Stop(&self->lowLife);
	cScorerItem_dtor(&self->base);
}

cScorerItem *cStatusScorer_Delete(cScorerItem *base, byte flags) {
	cStatusScorer *self = (cStatusScorer *)base;
	cStatusScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cStatusScorer));
	}
	return base;
}

#define SV(name, kind, field) {name, kind, offsetof(cStatusScorer, field)}
static const ScorerVarDef s_StatusVars1[] = {
	SV("BackIndex", SCORERVAR_INT, backIndex),
	SV("BackOffX", SCORERVAR_FLOAT, backOffX),
	SV("BackOffY", SCORERVAR_FLOAT, backOffY),
	SV("LifeIconIndex", SCORERVAR_INT, lifeIconIndex),
	SV("LifeIconOffX", SCORERVAR_FLOAT, lifeIconOffX),
	SV("LifeIconOffY", SCORERVAR_FLOAT, lifeIconOffY),
	SV("LifeTextOffX", SCORERVAR_FLOAT, lifeTextOffX),
	SV("LifeTextOffY", SCORERVAR_FLOAT, lifeTextOffY),
};
static const ScorerVarDef s_StatusVars2[] = {
	SV("LifeTextRed", SCORERVAR_RED, lifeTextColor),
	SV("LifeTextGreen", SCORERVAR_GREEN, lifeTextColor),
	SV("LifeTextBlue", SCORERVAR_BLUE, lifeTextColor),
	SV("LifeTextAlpha", SCORERVAR_ALPHA, lifeTextColor),
	SV("LivesTextOffX", SCORERVAR_FLOAT, livesTextOffX),
	SV("LivesTextOffY", SCORERVAR_FLOAT, livesTextOffY),
};
static const ScorerVarDef s_StatusVars3[] = {
	SV("LivesTextRed", SCORERVAR_RED, livesTextColor),
	SV("LivesTextGreen", SCORERVAR_GREEN, livesTextColor),
	SV("LivesTextBlue", SCORERVAR_BLUE, livesTextColor),
	SV("LivesTextAlpha", SCORERVAR_ALPHA, livesTextColor),
	SV("MoneyBackIndex", SCORERVAR_INT, moneyBackIndex),
	SV("MoneyBackOffX", SCORERVAR_FLOAT, moneyBackOffX),
	SV("MoneyBackOffY", SCORERVAR_FLOAT, moneyBackOffY),
	SV("MoneyIconIndex", SCORERVAR_INT, moneyIconIndex),
	SV("MoneyIconOffX", SCORERVAR_FLOAT, moneyIconOffX),
	SV("MoneyIconOffY", SCORERVAR_FLOAT, moneyIconOffY),
	SV("MoneyTextOffX", SCORERVAR_FLOAT, moneyTextOffX),
	SV("MoneyTextOffY", SCORERVAR_FLOAT, moneyTextOffY),
};
static const ScorerVarDef s_StatusVars4[] = {
	SV("MoneyTextRed", SCORERVAR_RED, moneyTextColor),
	SV("MoneyTextGreen", SCORERVAR_GREEN, moneyTextColor),
	SV("MoneyTextBlue", SCORERVAR_BLUE, moneyTextColor),
	SV("MoneyTextAlpha", SCORERVAR_ALPHA, moneyTextColor),
	SV("BoostStatus", SCORERVAR_INT, boostStatus),
	SV("BoostRemaining", SCORERVAR_FLOAT, boostRemaining),
};
static const ScorerVarDef s_StatusVars5[] = {
	SV("BoostOffX", SCORERVAR_FLOAT, boostOffX),
	SV("BoostOffY", SCORERVAR_FLOAT, boostOffY),
	SV("BoostIconIndex", SCORERVAR_INT, boostIconIndex),
	SV("BoostTurboIndex", SCORERVAR_INT, boostTurboIndex),
	SV("HullIconOffX", SCORERVAR_FLOAT, hullIconOffX),
	SV("HullIconOffY", SCORERVAR_FLOAT, hullIconOffY),
	SV("HullIconIndex", SCORERVAR_INT, hullIconIndex),
	SV("FontBackIndex", SCORERVAR_INT, fontBackIndex),
	SV("FontBackOffX", SCORERVAR_FLOAT, fontBackOffX),
	SV("FontBackOffY", SCORERVAR_FLOAT, fontBackOffY),
	SV("LivesBackIndex", SCORERVAR_INT, livesBackIndex),
	SV("LivesBackOffX", SCORERVAR_FLOAT, livesBackOffX),
	SV("LivesBackOffY", SCORERVAR_FLOAT, livesBackOffY),
};
static const ScorerVarDef s_StatusVars6[] = {
	SV("FlagOffX", SCORERVAR_FLOAT, flagOffX),
	SV("FlagOffY", SCORERVAR_FLOAT, flagOffY),
};
#undef SV

#define STATUS_TABLE(t) ScorerVar_Table(self, (t), sizeof(t) / sizeof((t)[0]), name, set, d, i)

int cStatusScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cStatusScorer *self = (cStatusScorer *)base;
	int r;

	if (s && !crt_stricmp(name, "Sprite") && set) {
		self->img = ScorerImg_ReplaceSimple(self->img, *s, 0x10060);
		self->glow = ScorerImg_Replace(self->glow, *s, 2, 0x10060);
		return 1;
	}
	if ((r = STATUS_TABLE(s_StatusVars1))) {
		return r;
	}
	if (set && s && !crt_stricmp(name, "LifeFont")) {
		return ScorerVar_Font(&self->lifeFont, *s);
	}
	if ((r = STATUS_TABLE(s_StatusVars2))) {
		return r;
	}
	if (set && s && !crt_stricmp(name, "LivesFont")) {
		return ScorerVar_Font(&self->livesFont, *s);
	}
	if ((r = STATUS_TABLE(s_StatusVars3))) {
		return r;
	}
	if (set && s && !crt_stricmp(name, "MoneyFont")) {
		return ScorerVar_Font(&self->moneyFont, *s);
	}
	if ((r = STATUS_TABLE(s_StatusVars4))) {
		return r;
	}
	if (set && s && !crt_stricmp(name, "BoostOnRender")) {
		/* the OnRender of the item, not a field of its own */
		cPy_Function_SetFunc(&self->base.onRender, *s);
		return 1;
	}
	if ((r = STATUS_TABLE(s_StatusVars5))) {
		return r;
	}
	if (s && !crt_stricmp(name, "SpriteFlag") && set) {
		self->flag = ScorerImg_ReplaceSimple(self->flag, *s, 0x10060);
		return 1;
	}
	if ((r = STATUS_TABLE(s_StatusVars6))) {
		return r;
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

/* the life engine variables */
extern cGVar GVar_StdMaxLife;
#define g_StdMaxLife (GVar_StdMaxLife.value.i)			/* the max life of what is no ship */
extern cGVar GVar_DangerLifeLevel;
#define g_DangerLifeLevel (GVar_DangerLifeLevel.value.i)		/* percent: the life blinks red below it */
extern cGVar GVar_WarningLifeLevel;
#define g_WarningLifeLevel (GVar_WarningLifeLevel.value.i)		/* percent: the low life sound fades in below it */
extern cGVar GVar_DangerLifeSoundVol;
#define g_DangerLifeSoundVol (GVar_DangerLifeSoundVol.value.f)
extern cGVar GVar_RegenerationSoundVol;
#define g_RegenerationSoundVol (GVar_RegenerationSoundVol.value.f)
extern cGVar GVar_RegenerationSoundPitchMax;
#define g_RegenerationSoundPitchMax (GVar_RegenerationSoundPitchMax.value.f)
extern cGVar GVar_RegenerationSoundPitchMin;
#define g_RegenerationSoundPitchMin (GVar_RegenerationSoundPitchMin.value.f)
extern cGVar GVar_InitRegenerationSound;
#define g_InitRegenerationSound (GVar_InitRegenerationSound.value.s)
extern cGVar GVar_InitRegenerationSoundVol;
#define g_InitRegenerationSoundVol (GVar_InitRegenerationSoundVol.value.f)
extern cGVar GVar_EndRegenerationSound;
#define g_EndRegenerationSound (GVar_EndRegenerationSound.value.s)
extern cGVar GVar_EndRegenerationSoundVol;
#define g_EndRegenerationSoundVol (GVar_EndRegenerationSoundVol.value.f)

#define STATUS_SEGMENTS 7		/* the boost segments over the life icon */
#define STATUS_LOW_COLOR 0xffff2020	/* the texts at 0 */

/* the y of the boost segment j (7 the top) over the life icon from y */
static float Status_SegmentY(cStatusScorer *self, float y, int off, int j) {
	return y + 4.0f + self->lifeIconOffY + (float)off + (float)(j * j) * 0.3f;
}

/* the boost segments: filling up while the boost runs (0xff), else the part left */
static void Status_DrawBoost(cStatusScorer *self, float frameTime) {
	CPsaImage *img = self->img;
	int bs = self->boostStatus;

	if (bs == 0xff) {
		self->boostTime = frameTime + frameTime + self->boostTime;
	} else {
		self->boostTime = 0.0f;
	}
	uint32_t rgb = bs != 0x80 ? 0xff1003 : 0x808080;
	if (bs == 0xff) {
		float k = 0.0f;
		if (1.0f > self->boostTime) {
			k = 1.0f - self->boostTime;
		}
		int lim = STATUS_SEGMENTS - (int)(k * 7.0f);
		int a = 0x20, i = 0, j = STATUS_SEGMENTS;
		for (int off = 0x1c; off > 0; off -= 4) {
			img->color = (uint32_t)a << 24 | (i < lim ? 0x3eaff : rgb);
			CPsaImage_DrawFrame(img, self->lifeIconIndex, self->base.x + self->lifeIconOffX,
				Status_SegmentY(self, self->base.y, off, j));
			a += 0x17;
			i++;
			j--;
		}
		return;
	}
	float f = (1.0f - self->boostRemaining) * 7.0f;
	int n = (int)f;
	float frac = f - (float)n;
	if (n < 0) {
		return;
	}
	int ai = 0, j = STATUS_SEGMENTS, off = 0x1c;
	uint32_t a = 0x20000000;
	for (int i = 0; i <= n; i++) {
		if (i == n) {
			img->color = (uint32_t)((int)((float)ai * frac) + 0x20) << 24 | rgb;
		} else {
			img->color = rgb | a;
		}
		CPsaImage_DrawFrame(img, self->lifeIconIndex, self->base.x + self->lifeIconOffX,
			Status_SegmentY(self, self->base.y, off, j));
		ai += 0x17;
		a += 0x17000000;
		off -= 4;
		j--;
	}
}

/* the money (blinking after a change) and the lives */
static void Status_DrawMoney(cStatusScorer *self, float time, float frameTime) {
	CPsaImage *img = self->img;

	int m = g_pWorld->money;
	self->money = m;
	if (m != self->lastMoney) {
		self->moneyBlink = 0.5f;
	} else if (self->moneyBlink > 0.0f) {
		self->moneyBlink = self->moneyBlink - frameTime;
	}
	if (self->moneyBackIndex) {
		img->color = self->backColor;
		CPsaImage_DrawFrame(img, self->moneyBackIndex, self->moneyBackOffX + self->base.x,
			self->moneyBackOffY + self->base.y);
	}
	float b = self->moneyBlink;
	if (0.0f >= b || (b > 0.0f && ((int)(time * 10.0f) & 1))) {
		img->color = self->textBackColor;
		Menu_DrawStretch(img, 0x22, self->moneyTextOffX + self->base.x - 86.0f + 10.0f,
			self->base.y + self->moneyTextOffY + 1.0f, 86.0f, 1.0f);
		img->color = self->moneyTextColor;
		CPsaImage_DrawFrame(img, self->moneyIconIndex, self->moneyIconOffX + self->base.x,
			self->moneyIconOffY + self->base.y);
		const char *s = cStrMoney_GetStr((cStrMoney *)g_pWorld->strMoney);
		CFont_PrintfAlignColor(self->moneyFont, self->moneyTextOffX + self->base.x, self->base.y + self->moneyTextOffY,
			1, self->moneyTextColor, s);
	}
	self->lastMoney = self->money;
	if (!self->moneyIconIndex) {
		return;
	}
	const char *lives = SaveVar_Get("Player.NumLives");
	if (lives) {
		self->numLives = crt_atoi(lives);
	}
	if (self->livesBackIndex) {
		img->color = self->backColor;
		CPsaImage_DrawFrame(img, self->livesBackIndex, self->livesBackOffX + self->base.x,
			self->livesBackOffY + self->base.y);
	}
	img->color = self->textBackColor;
	Menu_DrawStretch(img, 0x22, self->base.x + self->livesTextOffX - 4.0f, self->base.y + self->livesTextOffY + 1.0f,
		29.0f, 1.0f);
	uint32_t c = self->numLives > 0 ? self->livesTextColor : STATUS_LOW_COLOR;
	CFont_PrintfAlignColor(self->livesFont, self->base.x + self->livesTextOffX + 11.0f,
		self->base.y + self->livesTextOffY + 12.0f, 4, c, "%d", self->numLives);
}

/* the glow of the boost icon: used up (0x40) it shows the part left, while it runs (0xff) the filling and the
 * blinking turbo */
static void Status_DrawBoostIcon(cStatusScorer *self) {
	CPsaImage *glow = self->glow;
	float x = self->base.x, y = self->base.y;

	if (self->boostStatus == 0x40) {
		float f = (1.0f - self->boostRemaining) * 7.0f;
		int n = (int)f;
		int j = STATUS_SEGMENTS - n;
		glow->color = (uint32_t)(int)((f - (float)n) * 128.0f) << 24 | 0xff1002;
		CPsaImage_DrawFrame(glow, self->boostIconIndex, x - 2.0f + self->lifeIconOffX,
			y - 2.0f + 4.0f + self->lifeIconOffY + (float)(j * 4) + (float)(j * j) * 0.3f);
		return;
	}
	if (self->boostStatus != 0xff) {
		return;
	}
	if (1.0f > self->boostTime) {
		float g = 1.0f - self->boostTime;
		int n = (int)(g * 7.0f);
		glow->color = (uint32_t)(int)((g - (float)n) * 128.0f) << 24 | 0x3eaff;
		CPsaImage_DrawFrame(glow, self->boostIconIndex, x - 3.0f + self->lifeIconOffX,
			y - 3.0f + 4.0f + self->lifeIconOffY + (float)(n * 4) + (float)(n * n) * 0.3f);
	}
	float b = self->boostTime;
	float fr = b - (float)(int)b;
	float a;
	if (fr > 0.5f) {
		float h = fr - 0.5f;
		a = 1.0f - (h + h);
	} else {
		a = fr + fr;
	}
	glow->color = (uint32_t)(int)(a * 255.0f) << 24 | 0x3eaff;
	CPsaImage_DrawFrame(glow, self->boostTurboIndex, self->base.x - 2.0f + self->lifeIconOffX,
		self->base.y + 12.0f + self->lifeIconOffY);
}

/* vf00: the life of the target of the camera (blinking when in danger), the boost segments, the money and lives,
 * the team flag, the hull of a ship; the low life warning and the regeneration siren */
void cStatusScorer_Draw(cScorerItem *base, int player) {
	cStatusScorer *self = (cStatusScorer *)base;
	cEntity *e;

	float time = Scorer_Get(player)->time;
	float frameTime = Scorer_Get(player)->frameTime;
	if (!ScorerHud_CamTarget(player, "Obj", &e)) {
		return;
	}
	byte isCar = 0;
	int life = 0, pct = 100;
	float hull = 1.0f;
	if (e) {
		int stdMax = g_StdMaxLife;
		byte withLife = Entity_IsType(e, TYEN_WITH_LIFE);
		isCar = Entity_IsType(e, TYEN_CAR);
		int lv = 0;
		if (withLife) {
			life = ((cWithLifeEntity *)e)->life;
			lv = life;
		}
		int lo = stdMax, hi = stdMax;
		if (isCar) {
			lo = Veh(e)->minLife;
			hi = Veh(e)->maxLife;
		}
		pct = lv * 100 / hi;
		if (isCar) {
			if (life < lo) {
				hull = 0.0f;
			} else if (life <= hi && hi > lo) {
				hull = (float)(life - lo) / (float)(hi - lo);
			}
		}
	}
	CPsaImage *img = self->img;
	img->color = 0xffffffff;
	CPsaImage_DrawFrame(img, self->backIndex, self->backOffX + self->base.x, self->backOffY + self->base.y);
	if (isCar && self->lifeIconIndex) {
		Status_DrawBoost(self, frameTime);
	}
	if (self->moneyIconIndex) {
		Status_DrawMoney(self, time, frameTime);
	}
	if (!self->moneyIconIndex && e && isCar) {
		int t = ((cWithLifeEntity *)e)->arrow.tgType;
		if (t == 0 || t == 2) {
			CPsaImage_DrawFrame(self->flag, t == 0 ? 2 : 1, self->flagOffX + self->base.x,
				self->flagOffY + self->base.y);
		}
	}
	/* the color of the life and the volume of the warning */
	uint32_t lifeColor;
	float lowVol;
	int danger = g_DangerLifeLevel;
	if (pct <= danger) {
		int k = (int)(time * 1024.0f) % 512;
		int d = 0xff - k;
		lifeColor = ((uint32_t)(d < 0 ? -d : d) | 0xffffff00) << 8;
		lowVol = g_DangerLifeSoundVol;
	} else if (pct <= g_WarningLifeLevel) {
		int warn = g_WarningLifeLevel;
		lifeColor = 0xffffff00;
		lowVol = (float)(warn - pct) * g_DangerLifeSoundVol / (float)(warn - danger);
	} else {
		lifeColor = self->lifeTextColor;
		lowVol = 0.0f;
	}
	if (self->lifeIconIndex) {
		img->color = self->livesTextColor;
		CPsaImage_DrawFrame(img, self->lifeIconIndex, self->lifeIconOffX + self->base.x,
			self->lifeIconOffY + self->base.y);
	}
	if (self->fontBackIndex) {
		img->color = self->backColor;
		CPsaImage_DrawFrame(img, self->fontBackIndex, self->fontBackOffX + self->base.x,
			self->fontBackOffY + self->base.y);
	}
	danger = g_DangerLifeLevel;
	if (pct > danger || pct <= 0 || ((int)(((float)danger * 0.5f / (float)pct + 4.0f) * time) & 1)) {
		img->color = self->textBackColor;
		Menu_DrawStretch(img, 0x20, self->lifeTextOffX + self->base.x - 45.0f + 4.0f,
			self->base.y + self->lifeTextOffY + 1.0f, 45.0f, 1.0f);
		CFont_PrintfAlignColor(self->lifeFont, self->lifeTextOffX + self->base.x, self->base.y + self->lifeTextOffY, 1,
			life > 0 ? lifeColor : STATUS_LOW_COLOR, "%d", life);
	}
	if (!isCar) {
		return;
	}
	if (self->hullIconIndex) {
		img->color = (uint32_t)(int)(hull * 255.0f) << 24 | 0xff00ff;
		CPsaImage_DrawFrame(img, self->hullIconIndex, self->hullIconOffX + self->base.x,
			self->hullIconOffY + self->base.y);
	}
	if (!g_pApp->flags && !g_SplitScreenGame && lowVol != 0.0f && !g_pWorld->logic.shipRegen.noLoss) {
		self->lowLife.desc.volume = Snd_Volume(lowVol);
		self->lowLife.desc.pan = 0;
		cSound_Update(&self->lowLife);
	} else {
		cSound_Stop(&self->lowLife);
	}
	if (g_pWorld->logic.shipRegen.noLoss) {
		self->alarm.desc.volume = Snd_Volume(g_RegenerationSoundVol);
		self->alarm.desc.pan = 0;
		float p = (g_RegenerationSoundPitchMax - g_RegenerationSoundPitchMin) * (float)pct / 100.0f +
			g_RegenerationSoundPitchMin;
		self->alarm.desc.freq = 1.0f;
		cSound_SetPitch(&self->alarm, p);
		cSound_Update(&self->alarm);
	} else {
		cSound_Stop(&self->alarm);
	}
	if (self->alarmOn != g_pWorld->logic.shipRegen.noLoss) {
		if (g_pWorld->logic.shipRegen.noLoss) {
			Scorer_PlaySound(g_InitRegenerationSound, g_InitRegenerationSoundVol);
		} else {
			Scorer_PlaySound(g_EndRegenerationSound, g_EndRegenerationSoundVol);
		}
		self->alarmOn = g_pWorld->logic.shipRegen.noLoss;
	}
	if (self->boostIconIndex) {
		Status_DrawBoostIcon(self);
	}
}

/* the vtable of cStatusScorer */
const cScorerItemVtbl cStatusScorer_vftable = {
	.Draw = cStatusScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cStatusScorer_Var,
	.Delete = cStatusScorer_Delete,
};
