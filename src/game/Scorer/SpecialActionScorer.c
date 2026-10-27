/*
 * SpecialActionScorer.cpp of the game (Menu/): cSpecialActionScorer, the special action of the character in the
 * HUD with the draw handler of each character action (the table at rva 0x428cb8). The target search of the gear
 * (rva 0x17ac90) and its sphere callback (rva 0x17a380) are not decompiled. See include/scrap/scoreritems.h.
 */
#include "scrap/chartype.h"
#include "scrap/scoreritems.h"
#include "scrap/scrapfont.h"

/* the draw handler of each character action: thiscall (item, player) */
#define g_SpecialActionDraw ((void(__thiscall **)(cSpecialActionScorer *, int))Game_Ptr(0x428cb8))

static const char *const s_InactiveColors[4] = {"InactiveColorRed", "InactiveColorGreen", "InactiveColorBlue",
	"InactiveColorAlpha"};
static const char *const s_ActiveColors[4] = {"ActiveColorRed", "ActiveColorGreen", "ActiveColorBlue",
	"ActiveColorAlpha"};
static const char *const s_GlowColors[4] = {"GlowColorRed", "GlowColorGreen", "GlowColorBlue", "GlowColorAlpha"};
static const char *const s_TextColors[4] = {"TextColorRed", "TextColorGreen", "TextColorBlue", "TextColorAlpha"};

cSpecialActionScorer *__fastcall cSpecialActionScorer_ctor(cSpecialActionScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CSPECIALACTIONSCORER_VFTABLE);
	self->charAction = 0;
	self->textFont = g_FontHoratio;
	self->sprite = ScorerImg_ReplaceSimple(NULL, "2D/white.tga", 0x20);
	self->cannon = ScorerImg_ReplaceSimple(NULL, "2D/white.tga", 0x20);
	self->icon[0] = ScorerImg_ReplaceSimple(NULL, "2D/white.tga", 0x20);
	self->icon[1] = ScorerImg_ReplaceSimple(NULL, "2D/white.tga", 0x20);
	self->iconGlow[0] = ScorerImg_ReplaceSimple(NULL, "2D/white.tga", 0x20);
	self->iconGlow[1] = ScorerImg_ReplaceSimple(NULL, "2D/white.tga", 0x20);
	self->iconIndex = 0;
	self->glowIconIndex = 0;
	self->energyBackColor = 0x5cffcc00;
	self->batteryColor = 0xffffffff;
	return self;
}

void __fastcall cSpecialActionScorer_dtor(cSpecialActionScorer *self) {
	self->base.vftable = Game_Ptr(CSPECIALACTIONSCORER_VFTABLE);
	ScorerImg_Free(self->sprite);
	ScorerImg_Free(self->cannon);
	ScorerImg_Free(self->icon[0]);
	ScorerImg_Free(self->icon[1]);
	ScorerImg_Free(self->iconGlow[0]);
	ScorerImg_Free(self->iconGlow[1]);
	cScorerItem_dtor(&self->base);
}

cSpecialActionScorer *__thiscall cSpecialActionScorer_Delete(cSpecialActionScorer *self, byte flags) {
	cSpecialActionScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cSpecialActionScorer));
	}
	return self;
}

/* an int variable of the table, by name */
static int SpecialAction_VarInt(const char *name, const char *want, int *v, byte set, int *i) {
	if (crt_stricmp(name, want)) {
		return 0;
	}
	return ScorerVar_Int(v, set, i);
}

/* a pair of float variables nameX, nameY */
static int SpecialAction_VarOff(const char *name, const char *x, const char *y, float *v, byte set, double *d) {
	if (!crt_stricmp(name, x)) {
		return ScorerVar_Float(&v[0], set, d);
	}
	if (!crt_stricmp(name, y)) {
		return ScorerVar_Float(&v[1], set, d);
	}
	return 0;
}

int __thiscall cSpecialActionScorer_Var(cSpecialActionScorer *self, const char *name, byte set, char **s, double *d,
	int *i) {
	int r;

	if (i && (r = SpecialAction_VarInt(name, "CharAction", &self->charAction, set, i))) {
		return r;
	}
	if (s && !crt_stricmp(name, "CannonSprite") && set) {
		self->cannon = ScorerImg_ReplaceSimple(self->cannon, *s, 0x10064);
		return 1;
	}
	if (i && ((r = SpecialAction_VarInt(name, "CannonIndex", &self->cannonIndex, set, i))
		|| (r = SpecialAction_VarInt(name, "CrossHairIndex", &self->crossHairIndex, set, i)))) {
		return r;
	}
	if (s && !crt_stricmp(name, "Sprite") && set) {
		self->sprite = ScorerImg_ReplaceSimple(self->sprite, *s, 0x10060);
		return 1;
	}
	if (i && (r = SpecialAction_VarInt(name, "EnergyBackIndex", &self->energyBackIndex, set, i))) {
		return r;
	}
	if (d && (r = SpecialAction_VarOff(name, "EnergyBackOffX", "EnergyBackOffY", &self->energyBackOffX, set, d))) {
		return r;
	}
	if (i && (r = SpecialAction_VarInt(name, "EnergyBarIndex", &self->energyBarIndex, set, i))) {
		return r;
	}
	if (d && (r = SpecialAction_VarOff(name, "EnergyBarOffX", "EnergyBarOffY", &self->energyBarOffX, set, d))) {
		return r;
	}
	if (i && (r = SpecialAction_VarInt(name, "BatteryBarIndex", &self->batteryBarIndex, set, i))) {
		return r;
	}
	if (d && (r = SpecialAction_VarOff(name, "BatteryBarOffX", "BatteryBarOffY", &self->batteryBarOffX, set, d))) {
		return r;
	}
	if (i && (r = SpecialAction_VarInt(name, "BackIndex", &self->backIndex, set, i))) {
		return r;
	}
	if (d && (r = SpecialAction_VarOff(name, "BackOffX", "BackOffY", &self->backOffX, set, d))) {
		return r;
	}
	if (s) {
		for (int k = 0; k < 2; k++) {
			if (!crt_stricmp(name, k ? "IconSprite2" : "IconSprite1") && set) {
				self->icon[k] = ScorerImg_ReplaceSimple(self->icon[k], *s, 0x10060);
				self->iconGlow[k] = ScorerImg_Replace(self->iconGlow[k], *s, 2, 0x10060);
				return 1;
			}
		}
	}
	if (d && (r = SpecialAction_VarOff(name, "IconOffX", "IconOffY", &self->iconOffX, set, d))) {
		return r;
	}
	if (i && ((r = SpecialAction_VarInt(name, "IconIndexSprite0", &self->iconIndexSprite0, set, i))
		|| (r = SpecialAction_VarInt(name, "IconIndex", &self->iconIndex, set, i))
		|| (r = SpecialAction_VarInt(name, "GlowIconIndex", &self->glowIconIndex, set, i))
		|| (r = ScorerVar_Color(name, s_InactiveColors, &self->inactiveColor, set, i))
		|| (r = ScorerVar_Color(name, s_ActiveColors, &self->activeColor, set, i))
		|| (r = ScorerVar_Color(name, s_GlowColors, &self->glowColor, set, i)))) {
		return r;
	}
	if (d && (r = SpecialAction_VarOff(name, "TextOffX", "TextOffY", &self->textOffX, set, d))) {
		return r;
	}
	if (i && (r = ScorerVar_Color(name, s_TextColors, &self->textColor, set, i))) {
		return r;
	}
	if (set && s && !crt_stricmp(name, "TextFont")) {
		CFont *f = Font_Get(*s);
		if (!f) {
			return 1;
		}
		self->textFont = f;
		return 0;
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

void __thiscall cSpecialActionScorer_Draw(cSpecialActionScorer *self, int player) {
	if (!g_SpecialActionActive) {
		return;
	}
	CHECK(self->charAction < SPECIALACTION_MAX_ACTIONS);
	CPsaImage_DrawFrame(self->sprite, self->backIndex, self->backOffX + self->base.x, self->backOffY + self->base.y);
	g_SpecialActionDraw[self->charAction](self, player);
}

/* ---- the draws of the character actions ---- */

#define g_SpecialActionState GAME_DATA(int, 0x428cf4)	/* the icon state drawn last: 0 off, 1 on, 2 active */
#define g_SpecialActionFade GAME_DATA(float, 0x428cf8)	/* counts down 0.2 s after a state change */
#define g_GearAimSpreadX GAME_DATA(float, 0x428d10)	/* the aim cone of the gear target search */
#define g_GearAimSpreadY GAME_DATA(float, 0x428d14)
#define g_GearAimMode GAME_DATA(int, 0x4606c8)		/* 1..3: which targets the search takes */
#define SPECIALACTION_FADE 0.2f
#define TYEN_ACTION "Action"
#define TYEN_ACTIONINIT "ActionInit"
#define TYEN_ACTIONEND "ActionEnd"

/* the character of player slot 0 (World.h: CHECK(UsrEntities[i]->IsType("WithLife")) inlined) */
static cEntity *SpecialAction_User(void) {
	cWorld *w = g_pWorld;
	cEntity *e = w->owners[0];

	CHECK(!e || e->vftable->IsType(e, TYEN_WITH_LIFE));
	return w->owners[0];
}

static byte SpecialAction_IsActive(cEntity *e, const char *action) {
	return cActCtrl_IsActive(WithLife_Vtbl(e)->GetActCtrl(e), action);
}

/* the icon of the action in state (0 off, 1 on, 2 active), its color faded over 0.2 s after a change, and the
 * blinking glow while active */
void __thiscall cSpecialActionScorer_DrawIcon(cSpecialActionScorer *self, int player, int state) {
	float t, f, g, a;
	int which = 0;
	uint32_t color;

	if (self->iconIndex <= 0) {
		return;
	}
	t = Scorer_Get(player)->time;
	if (self->iconIndex > 0 && self->iconIndex >= self->iconIndexSprite0) {
		which = 1;
	}
	f = 1.0f;
	if (g_SpecialActionState != state) {
		float fade = g_SpecialActionFade;
		if (fade > 0.0f) {
			f = 1.0f - fade / SPECIALACTION_FADE;
			g_SpecialActionFade = g_SpecialActionFade - Scorer_Get(player)->frameTime;
		} else {
			g_SpecialActionFade = SPECIALACTION_FADE;
			g_SpecialActionState = state;
		}
	}
	if (state == 0) {
		if (g_SpecialActionState) {
			color = Scorer_ColorLerp(self->inactiveColor, self->activeColor, 1.0f - f);
		} else {
			color = self->inactiveColor;
		}
		self->icon[which]->color = color;
	} else if ((uint32_t)(state - 1) <= 1) {
		if (g_SpecialActionState == state || g_SpecialActionState) {
			color = self->activeColor;
		} else {
			color = Scorer_ColorLerp(self->inactiveColor, self->activeColor, f);
		}
		self->icon[which]->color = color;
	}
	if (self->iconIndex > 0) {
		CPsaImage_DrawFrame(self->icon[which], self->iconIndex, self->iconOffX + self->base.x,
			self->iconOffY + self->base.y);
	}
	if (self->glowIconIndex > 0 && (state == 2 || g_SpecialActionState == 2)) {
		g = (float)((int)(t * 1024.0f) % 512) * 0.001953125f;
		if (0.5f > g) {
			g = g + g;
		} else {
			g = g - 0.5f;
			g = 1.0f - (g + g);
		}
		a = f;
		if (state != 2 && g_SpecialActionState == 2) {
			a = 1.0f - f;
		}
		self->iconGlow[which]->color = (uint32_t)(int)(g * a * 192.0f) << 24 | (self->glowColor & 0xffffff);
		CPsaImage_DrawFrame(self->iconGlow[which], self->glowIconIndex, self->iconOffX + self->base.x,
			self->iconOffY + self->base.y);
	}
}

/* the energy bar and the batteries of the character (EnergyBarActive), blinking while SpecialActionActive is 1 */
void __fastcall cSpecialActionScorer_DrawEnergy(cSpecialActionScorer *self) {
	float t = Scorer_Get(self->base.player)->time;
	float uv0[2], uv1[2];
	cEntity *e;
	int n;

	if (!g_EnergyBarActive) {
		return;
	}
	if (g_SpecialActionActive == 1 && ((int)(t * 8.0f) & 1)) {
		return;
	}
	if (self->energyBackIndex) {
		self->sprite->color = self->energyBackColor;
		CPsaImage_DrawFrameFlip(self->sprite, self->energyBackIndex, self->energyBackOffX + self->base.x,
			self->energyBackOffY + self->base.y, 0, 1);
		self->sprite->flip = 0;
	}
	e = SpecialAction_User();
	if (!e) {
		return;
	}
	self->sprite->color = 0xc0f87b00;
	CPsaImage_GetFrameUV(self->sprite, self->energyBarIndex, uv0, uv1);
	float x = self->energyBarOffX + self->base.x;
	float y = self->energyBarOffY + self->base.y;
	float frac = cWalkCharEntity_GetEnergyFrac(e);
	Menu_DrawBar(self->sprite, x, y, frac, 5, 0.0f, uv0[0], uv0[1], uv1[0], uv1[1], 0.0f, 0.0f, 0.0f, 0.0f, 0, 0);
	n = cWalkCharEntity_GetBatteries(e);
	if (n > 0) {
		if (n > 4) {
			n = 4;
		}
		self->sprite->color = self->batteryColor;
		Menu_DrawStretch(self->sprite, 0x20, self->batteryBarOffX + self->base.x - 5.0f,
			self->batteryBarOffY + self->base.y - 5.0f, 79.0f, 1.0f);
		self->sprite->color = 0xffffffff;
		for (int k = 0; n > 0; k += 18, n--) {
			CPsaImage_DrawFrame(self->sprite, self->batteryBarIndex,
				self->batteryBarOffX + self->base.x + (float)k, self->batteryBarOffY + self->base.y - 1.0f);
		}
	}
	self->sprite->color = 0xffffffff;
}

/* the cross hair of the gear aim in the middle of the screen */
void __thiscall cSpecialActionScorer_DrawCrossHair(cSpecialActionScorer *self, int player) {
	static const float pos[8] = {-0.25f, -0.25f, 0.25f, -0.25f, 0.25f, 0.25f, -0.25f, 0.25f};
	float uv0[2], uv1[2], uv[8], pv[8];
	CPsaImage *img;

	(void)player;
	if (!self->crossHairIndex) {
		return;
	}
	img = self->cannon;
	img->scaleX = 32.0f;
	img->scaleY = 32.0f;
	self->cannon->color = 0x80ffffff;
	CPsaImage_GetFrameUV(self->cannon, self->crossHairIndex, uv0, uv1);
	uv[0] = uv0[0];
	uv[1] = uv0[1];
	uv[2] = uv1[0];
	uv[3] = uv0[1];
	uv[4] = uv1[0];
	uv[5] = uv1[1];
	uv[6] = uv0[0];
	uv[7] = uv1[1];
	for (int k = 0; k < 8; k++) {
		pv[k] = pos[k];
	}
	img = self->cannon;
	img->x = 320.0f;
	img->y = 240.0f;
	CPsaImage_DrawPoly(self->cannon, pv, uv, NULL, 4, 6);
}

/* the handlers of the table at rva 0x428cb8 by character action */

/* 0: the weapon energy above 0.9 */
void __thiscall SpecialAction_DrawDefault(cSpecialActionScorer *self, int player) {
	cEntity *e = SpecialAction_User();
	int state;

	if (!e) {
		return;
	}
	if (0.9f > 1.0f - WChar_EnergyUsed(e)) {
		state = 0;
	} else {
		state = SpecialAction_IsActive(e, TYEN_ACTION) ? 2 : 1;
	}
	cSpecialActionScorer_DrawIcon(self, player, state);
	cSpecialActionScorer_DrawEnergy(self);
}

/* 1: the victim of the banker and its money */
void __thiscall SpecialAction_DrawBanker(cSpecialActionScorer *self, int player) {
	char buf[8];
	cEntity *e, *v;

	cSpecialActionScorer_DrawEnergy(self);
	e = SpecialAction_User();
	if (!e) {
		return;
	}
	v = ChType_BankerVictim(e);
	if (!v) {
		cSpecialActionScorer_DrawIcon(self, player, 0);
		return;
	}
	cSpecialActionScorer_DrawIcon(self, player, 1);
	self->sprite->color = self->batteryColor;
	Menu_DrawStretch(self->sprite, 0x22, self->base.x + self->textOffX - 4.0f, self->base.y + self->textOffY + 1.0f,
		60.0f, 1.0f);
	crt_ltoa(WChar_Money(v), buf, 10);
	CFont_PrintfAlignColor(self->textFont, self->base.x + self->textOffX, self->base.y + self->textOffY,
		TEXTSCORER_LEFT, self->textColor, buf);
}

/* 2: a target in reach or the action running */
void __thiscall SpecialAction_DrawTarget(cSpecialActionScorer *self, int player) {
	cEntity *e = SpecialAction_User();
	int target;
	byte flag;
	int out;

	if (!e) {
		return;
	}
	cSpecialActionScorer_DrawEnergy(self);
	target = 0;
	if (!ChType_ActionTarget(e, &target, &flag, &out) && !SpecialAction_IsActive(e, TYEN_ACTION)
		&& !SpecialAction_IsActive(e, TYEN_ACTIONINIT)) {
		cSpecialActionScorer_DrawIcon(self, player, 0);
		return;
	}
	cSpecialActionScorer_DrawIcon(self, player, 1);
}

/* 3, 11, 12: the gear aim with its cone and mode */
void __thiscall SpecialAction_DrawGear(cSpecialActionScorer *self, int player) {
	g_GearAimSpreadX = 1.5707964f;
	g_GearAimSpreadY = 1.5707964f;
	g_GearAimMode = 1;
	cSpecialActionScorer_DrawGearAim(self, player);
}

void __thiscall SpecialAction_DrawGear2(cSpecialActionScorer *self, int player) {
	g_GearAimSpreadX = 0.5235988f;
	g_GearAimSpreadY = 0.5235988f;
	g_GearAimMode = 2;
	cSpecialActionScorer_DrawGearAim(self, player);
}

void __thiscall SpecialAction_DrawGear3(cSpecialActionScorer *self, int player) {
	g_GearAimSpreadX = 0.5235988f;
	g_GearAimSpreadY = 0.5235988f;
	g_GearAimMode = 3;
	cSpecialActionScorer_DrawGearAim(self, player);
}

/* 4: the victim of a walking character */
void __thiscall SpecialAction_DrawVictim(cSpecialActionScorer *self, int player) {
	cEntity *e = SpecialAction_User();
	int state;

	cSpecialActionScorer_DrawEnergy(self);
	if (!ChType_WalkCharVictim(e)) {
		cSpecialActionScorer_DrawIcon(self, player, 0);
		return;
	}
	if (SpecialAction_IsActive(e, TYEN_ACTIONINIT) || SpecialAction_IsActive(e, TYEN_ACTION)) {
		state = 2;
	} else {
		state = 1;
	}
	cSpecialActionScorer_DrawIcon(self, player, state);
}

/* 5, 6, 7, 13, 14: always on */
void __thiscall SpecialAction_DrawOn(cSpecialActionScorer *self, int player) {
	cSpecialActionScorer_DrawEnergy(self);
	cSpecialActionScorer_DrawIcon(self, player, 1);
}

/* 8: the weapon energy above GearLowWeaponEnergy, active through the three action steps */
void __thiscall SpecialAction_DrawLowEnergy(cSpecialActionScorer *self, int player) {
	cEntity *e = SpecialAction_User();
	int state;

	if (!e) {
		return;
	}
	if (g_GearLowWeaponEnergy > 1.0f - WChar_EnergyUsed(e)) {
		state = 0;
	} else if (SpecialAction_IsActive(e, TYEN_ACTION) || SpecialAction_IsActive(e, TYEN_ACTIONINIT)
		|| SpecialAction_IsActive(e, TYEN_ACTIONEND)) {
		state = 2;
	} else {
		state = 1;
	}
	cSpecialActionScorer_DrawIcon(self, player, state);
	cSpecialActionScorer_DrawEnergy(self);
}

/* 9: the functionary, active while its timer at 0x4b4 runs */
void __thiscall SpecialAction_DrawFunctionary(cSpecialActionScorer *self, int player) {
	cEntity *e = SpecialAction_User();
	int state;

	if (!e) {
		return;
	}
	if (g_FunctionarySpecialActionLow > 1.0f - WChar_EnergyUsed(e)) {
		state = 0;
	} else {
		state = *(float *)((byte *)e + 0x4b4) == 0.0f ? 1 : 2;
	}
	cSpecialActionScorer_DrawIcon(self, player, state);
	cSpecialActionScorer_DrawEnergy(self);
}

/* 10: on above 0.9 */
void __thiscall SpecialAction_DrawEnergyOnly(cSpecialActionScorer *self, int player) {
	cEntity *e = SpecialAction_User();

	if (!e) {
		return;
	}
	cSpecialActionScorer_DrawIcon(self, player, !(0.9f > 1.0f - WChar_EnergyUsed(e)));
	cSpecialActionScorer_DrawEnergy(self);
}

/* the three blades of the cannon aim at pos (screen), size the sprite size, turned by ang */
void __thiscall cSpecialActionScorer_DrawCannon(cSpecialActionScorer *self, int player, const float *pos,
	const float *size, float ang, uint32_t color) {
	static const float corners[8] = {-0.5f, -0.375f, 0.5f, -0.375f, 0.5f, 0.375f, -0.5f, 0.375f};
	float uv0[2], uv1[2], uv[8], p[8], c[2], off[2], r[2];

	(void)player;
	if (!self->cannonIndex) {
		return;
	}
	self->cannon->color = color;
	CPsaImage *img = self->cannon;
	img->scaleX = size[0] * 0.05f * 24.0f;
	img->scaleY = size[1] * 0.05f * 24.0f;
	CPsaImage_GetFrameUV(self->cannon, self->cannonIndex, uv0, uv1);
	for (int k = 0; k < 3; k++) {
		uv[0] = uv0[0];
		uv[1] = uv0[1];
		uv[2] = uv1[0];
		uv[3] = uv0[1];
		uv[4] = uv1[0];
		uv[5] = uv1[1];
		uv[6] = uv0[0];
		uv[7] = uv1[1];
		float a = (float)k * 2.0943952f + ang;
		for (int n = 0; n < 4; n++) {
			c[0] = corners[2 * n];
			c[1] = corners[2 * n + 1];
			float *q = Vec2_Rotate(r, c, a);
			p[2 * n] = q[0];
			p[2 * n + 1] = q[1];
		}
		off[1] = size[1] * 0.05f * 12.0f;
		off[0] = size[0] * 0.05f * 0.0f;
		float *q = Vec2_Rotate(r, off, a);
		img = self->cannon;
		img->x = q[0] + pos[0];
		img->y = q[1] + pos[1];
		CPsaImage_DrawPoly(self->cannon, p, uv, NULL, 4, 6);
	}
}
