/*
 * StatusScorer.cpp of the game (Menu/): cStatusScorer, the life, lives, money and boost of the ship of the camera.
 * See include/scrap/scorerhud.h.
 */
#include "scrap/scorerhud.h"
#include "scrap/sound.h"

/* a byte of the ship regeneration state of the world (SLogic.SetShipToRegenerate): the siren plays while set */
#define World_RegenOn() (*((byte *)g_pWorld + 0x2150))

static CPsaImage *Status_NewWhite(void) {
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x20) : NULL;
	img->color = 0xffffffff;
	return img;
}

cStatusScorer *__fastcall cStatusScorer_ctor(cStatusScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CSTATUSSCORER_VFTABLE);
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
	self->alarmOn = World_RegenOn();
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

void __fastcall cStatusScorer_dtor(cStatusScorer *self) {
	self->base.vftable = Game_Ptr(CSTATUSSCORER_VFTABLE);
	ScorerImg_Free(self->img);
	ScorerImg_Free(self->glow);
	ScorerImg_Free(self->flag);
	cSound_Stop(&self->alarm);
	cSound_Stop(&self->lowLife);
	cScorerItem_dtor(&self->base);
}

cStatusScorer *__thiscall cStatusScorer_Delete(cStatusScorer *self, byte flags) {
	cStatusScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cStatusScorer));
	}
	return self;
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

int __thiscall cStatusScorer_Var(cStatusScorer *self, const char *name, byte set, char **s, double *d, int *i) {
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
