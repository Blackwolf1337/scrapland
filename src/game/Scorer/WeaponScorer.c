/*
 * WeaponScorer.cpp of the game (Menu/): cWeaponScorer, the weapon of the ship of the camera with its ammo, its
 * upgrades and the weapon slots. See include/scrap/scorerhud.h.
 */
#include "scrap/scorerhud.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"

static CPsaImage *Weapon_NewWhite(byte color) {
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x20) : NULL;
	if (color) {
		img->color = 0xffffffff;
	}
	return img;
}

cWeaponScorer *__fastcall cWeaponScorer_ctor(cWeaponScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CWEAPONSCORER_VFTABLE;
	self->base.x = 320.0f;
	self->base.y = 240.0f;
	self->ammoFont = g_FontHoratio;
	self->ammoTextColor = 0xffffffff;
	self->img = Weapon_NewWhite(1);
	self->img3D = Weapon_NewWhite(1);
	self->changeTime = 0.0f;
	self->lastWeapon = -100;
	self->lastUpgrade = -100;
	self->slotsTime = 0.0f;
	self->backColor = 0x5cffcc00;
	self->textBackColor = 0xffffffff;
	/* the color stays the one of the image */
	self->tab = Weapon_NewWhite(0);
	self->tabSpriteIndex = 0;
	self->tabBlend = 0;
	return self;
}

void __fastcall cWeaponScorer_dtor(cWeaponScorer *self) {
	self->base.vftable = CWEAPONSCORER_VFTABLE;
	ScorerImg_Free(self->img);
	ScorerImg_Free(self->img3D);
	ScorerImg_Free(self->tab);
	cScorerItem_dtor(&self->base);
}

cWeaponScorer *__thiscall cWeaponScorer_Delete(cWeaponScorer *self, byte flags) {
	cWeaponScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cWeaponScorer));
	}
	return self;
}

#define WV(name, kind, field) {name, kind, offsetof(cWeaponScorer, field)}
static const ScorerVarDef s_WeaponVars1[] = {
	WV("BackIndex", SCORERVAR_INT, backIndex),
	WV("BackOffX", SCORERVAR_FLOAT, backOffX),
	WV("BackOffY", SCORERVAR_FLOAT, backOffY),
	WV("WeaponIconOffX", SCORERVAR_FLOAT, weaponIconOffX),
	WV("WeaponIconOffY", SCORERVAR_FLOAT, weaponIconOffY),
	WV("WeaponIconShadowOffX", SCORERVAR_FLOAT, shadowOffX),
	WV("WeaponIconShadowOffY", SCORERVAR_FLOAT, shadowOffY),
	WV("WeaponIconAc", SCORERVAR_INT, weaponIconAc),
};
static const ScorerVarDef s_WeaponVars2[] = {
	WV("FontBackIndex", SCORERVAR_INT, fontBackIndex),
	WV("FontBackOffX", SCORERVAR_FLOAT, fontBackOffX),
	WV("FontBackOffY", SCORERVAR_FLOAT, fontBackOffY),
	WV("AmmoBackIndex", SCORERVAR_INT, ammoBackIndex),
	WV("AmmoBackOffX", SCORERVAR_FLOAT, ammoBackOffX),
	WV("AmmoBackOffY", SCORERVAR_FLOAT, ammoBackOffY),
	WV("AmmoIconAc", SCORERVAR_INT, ammoIconAc),
};
static const ScorerVarDef s_WeaponVars3[] = {
	WV("AmmoIconOffX", SCORERVAR_FLOAT, ammoIconOffX),
	WV("AmmoIconOffY", SCORERVAR_FLOAT, ammoIconOffY),
	WV("AmmoBarIndex", SCORERVAR_INT, ammoBarIndex),
	WV("AmmoBarOffX", SCORERVAR_FLOAT, ammoBarOffX),
	WV("AmmoBarOffY", SCORERVAR_FLOAT, ammoBarOffY),
	WV("AmmoBarRed", SCORERVAR_RED, ammoBarColor),
	WV("AmmoBarGreen", SCORERVAR_GREEN, ammoBarColor),
	WV("AmmoBarBlue", SCORERVAR_BLUE, ammoBarColor),
	WV("AmmoBarAlpha", SCORERVAR_ALPHA, ammoBarColor),
	WV("AmmoTextOffX", SCORERVAR_FLOAT, ammoTextOffX),
	WV("AmmoTextOffY", SCORERVAR_FLOAT, ammoTextOffY),
};
static const ScorerVarDef s_WeaponVars4[] = {
	WV("AmmoTextRed", SCORERVAR_RED, ammoTextColor),
	WV("AmmoTextGreen", SCORERVAR_GREEN, ammoTextColor),
	WV("AmmoTextBlue", SCORERVAR_BLUE, ammoTextColor),
	WV("AmmoTextAlpha", SCORERVAR_ALPHA, ammoTextColor),
	WV("UpgradeBarIndex", SCORERVAR_INT, upgradeBarIndex),
	WV("UpgradeBarOffX", SCORERVAR_FLOAT, upgradeBarOffX),
	WV("UpgradeBarOffY", SCORERVAR_FLOAT, upgradeBarOffY),
	WV("UpgradeBarRed", SCORERVAR_RED, upgradeBarColor),
	WV("UpgradeBarGreen", SCORERVAR_GREEN, upgradeBarColor),
	WV("UpgradeBarBlue", SCORERVAR_BLUE, upgradeBarColor),
	WV("UpgradeBarAlpha", SCORERVAR_ALPHA, upgradeBarColor),
	WV("SlotsOffX", SCORERVAR_FLOAT, slotsOffX),
	WV("SlotsOffY", SCORERVAR_FLOAT, slotsOffY),
	WV("SlotsIndex0", SCORERVAR_INT, slotsIndex[0]),
	WV("SlotsIndex1", SCORERVAR_INT, slotsIndex[1]),
};
#undef WV

#define WEAPON_TABLE(t) ScorerVar_Table(self, (t), sizeof(t) / sizeof((t)[0]), name, set, d, i)

/* the icon of the selected index; the index is not checked */
static int Weapon_IconVar(int *icons, int ac, byte set, int *i) {
	return ScorerVar_Int(icons + ac, set, i);
}

int __thiscall cWeaponScorer_Var(cWeaponScorer *self, const char *name, byte set, char **s, double *d, int *i) {
	static const char *const tabColors[4] = {"TabRed", "TabGreen", "TabBlue", "TabAlpha"};
	int r;

	if (s) {
		if (!crt_stricmp(name, "Sprite") && set) {
			self->img = ScorerImg_ReplaceSimple(self->img, *s, 0x10060);
			return 1;
		}
		if (!crt_stricmp(name, "Weapons3DSprite") && set) {
			self->img3D = ScorerImg_ReplaceSimple(self->img3D, *s, 0x10060);
			return 1;
		}
	}
	if ((r = WEAPON_TABLE(s_WeaponVars1))) {
		return r;
	}
	if (i) {
		if (!crt_stricmp(name, "WeaponIconIndex")) {
			return Weapon_IconVar(self->weaponIcons, self->weaponIconAc, set, i);
		}
		if (!crt_stricmp(name, "UpgradeIconAc")) {
			return ScorerVar_Int(&self->upgradeIconAc, set, i);
		}
		if (!crt_stricmp(name, "UpgradeIconIndex")) {
			return Weapon_IconVar(self->upgradeIcons, self->upgradeIconAc, set, i);
		}
	}
	if ((r = WEAPON_TABLE(s_WeaponVars2))) {
		return r;
	}
	if (i && !crt_stricmp(name, "AmmoIconIndex")) {
		return Weapon_IconVar(self->ammoIcons, self->ammoIconAc, set, i);
	}
	if ((r = WEAPON_TABLE(s_WeaponVars3))) {
		return r;
	}
	if (set && s && !crt_stricmp(name, "AmmoFont")) {
		return ScorerVar_Font(&self->ammoFont, *s);
	}
	if ((r = WEAPON_TABLE(s_WeaponVars4))) {
		return r;
	}
	if (s && !crt_stricmp(name, "TabSprite") && set) {
		self->tab = ScorerImg_Replace(self->tab, *s, self->tabBlend, 0x10060);
		return 1;
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
		if ((r = ScorerVar_Color(name, tabColors, &self->tab->color, set, i))) {
			return r;
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

#define g_WeaponChangeFadeTime GAME_DATA(float, 0x462440)	/* WeaponChangeFadeTime */
#define g_WeaponChangeShowTime GAME_DATA(float, 0x462458)	/* WeaponChangeShowTime: the slots after a change */
/* the slot row of the weapons, shared by all items: per weapon 0 none, 1 with ammo, 2 without; its place in the row */
#define g_WeaponSlotState ((int *)Game_Ptr(0x462460))		/* [WEAPONSCORER_WEAPONS] */
#define g_WeaponSlotPlace ((int *)Game_Ptr(0x46247c))		/* [WEAPONSCORER_WEAPONS] */
#define g_WeaponSlotCount GAME_DATA(int, 0x462498)

#define WEAPON_UAMMO_MAX 200.0f		/* the ammo of an upgrade */
#define WEAPON_AMMO_CHARGED 3		/* the ammo kind with a bar of its charge instead of a number */
#define WEAPON_LASER_WEAPON 6		/* drawn from the 2D sheet, not the 3D one */
#define WEAPON_SLOT_SIZE 38		/* the slot row: the size of a slot, centred at x 339 */
#define WEAPON_SLOT_X 0x153
#define WEAPON_CHANGE_TIME 0.2f

/* the icon of weapon w with upgrade u, the upgrade icon when it has one */
static int Weapon_Icon(cWeaponScorer *self, int w, int u) {
	return u > 0 ? self->upgradeIcons[u] : self->weaponIcons[w];
}

static void Weapon_DrawAt(CPsaImage *img, int frame, float x, float y) {
	if (frame > 0) {
		CPsaImage_DrawFrame(img, frame, x, y);
	}
}

/* the charge of the charged ammo: the icon and a bar, colored from yellow (empty) to white */
static void Weapon_DrawCharge(cWeaponScorer *self, cWeaponManager *wm, int w) {
	float uv0[2], uv1[2];
	CPsaImage *img = self->img;
	float x = self->base.x, y = self->base.y;

	cAmmoInfo *a = TABLE_AT(g_pWorld->ammo, WEAPON_AMMO_CHARGED);
	cAmmoInfo *b = TABLE_AT(g_pWorld->ammo, WEAPON_AMMO_CHARGED);
	float left = Weap_X87Sub(a->max, cWeaponManager_GetWeapAmmo(wm, (uint32_t)w));
	float f = left / b->max;
	if (self->ammoIcons[WEAPON_AMMO_CHARGED]) {
		float e = 1.0f - f;
		img->color = ((uint32_t)(int)(e * 241.0f) | 0xffff00) << 8 | (uint32_t)(int)(e * 171.0f);
		CPsaImage_DrawFrame(img, self->ammoIcons[WEAPON_AMMO_CHARGED], self->ammoIconOffX + x + 4.5f,
			self->ammoIconOffY + y + 1.0f);
	}
	CPsaImage_GetFrameUV(img, self->ammoBarIndex, uv0, uv1);
	float e = 1.0f - f;
	img->color = ((uint32_t)(int)(e * 241.0f) | 0xc0ff00) << 8 | (uint32_t)(int)(e * 171.0f);
	Menu_DrawBar(img, self->ammoBarOffX + x, self->ammoBarOffY + y, f, 5, 0.0f, uv0[0], uv0[1], uv1[0], uv1[1],
		0.0f, 0.0f, 0.0f, 0.0f, 0, 0);
	img->color = 0xffffffff;
}

/* the ammo icon and the ammo count (blinking at 0) */
static void Weapon_DrawAmmo(cWeaponScorer *self, cWeaponManager *wm, int w, int kind, float time) {
	CPsaImage *img = self->img;
	float x = self->base.x, y = self->base.y;

	if (self->ammoIcons[kind]) {
		img->color = 0xffffffff;
		CPsaImage_DrawFrame(img, self->ammoIcons[kind], self->ammoIconOffX + x, self->ammoIconOffY + y);
	}
	if (self->fontBackIndex) {
		img->color = self->backColor;
		CPsaImage_DrawFrameFlip(img, self->fontBackIndex, self->fontBackOffX + x, self->fontBackOffY + y, 0, 1);
	}
	img->flip = 0;
	float am = cWeaponManager_GetWeapAmmo(wm, (uint32_t)w);
	int n = (int)am;
	if (n > 0 || ((int)(time * 10.0f) & 1)) {
		img->color = self->textBackColor;
		Menu_DrawStretch(img, 0x20, self->ammoTextOffX + x - 4.0f, self->ammoTextOffY + y + 1.0f, 50.0f, 1.0f);
		CFont_PrintfAlignColor(self->ammoFont, self->ammoTextOffX + self->base.x, self->ammoTextOffY + self->base.y,
			0, self->ammoTextColor, "%d", n);
	}
}

/* the row of the weapon slots after a change: the weapons of the ship in their order, the selected one framed */
static void Weapon_DrawSlots(cWeaponScorer *self, cWeaponManager *wm, int player, int sel, float frameTime) {
	float t = self->slotsTime - frameTime;
	if (t >= 0.0f) {
		self->slotsTime = t;
	} else {
		self->slotsTime = 0.0f;
	}
	float a = self->slotsTime;
	float fade = g_WeaponChangeFadeTime;
	if (a > fade) {
		a = 1.0f;
	} else {
		a = a / fade;
	}
	int alpha = (int)(a * 192.0f);
	float x0 = (float)(WEAPON_SLOT_X - g_WeaponSlotCount * WEAPON_SLOT_SIZE / 2);
	float y0 = 300.0f;
	if (g_SplitScreenGame) {
		y0 = player == 0 ? 180.0f : 420.0f;
	}
	for (int i = 0; i < WEAPONSCORER_WEAPONS; i++) {
		int st = g_WeaponSlotState[i];
		if (st <= 0) {
			continue;
		}
		float y = y0 + 0.0f;
		uint32_t a24 = (uint32_t)alpha << 24;
		float x = (float)(g_WeaponSlotPlace[i] * WEAPON_SLOT_SIZE) + x0;
		uint32_t color = a24 | (st == 1 ? 0xffffff : 0x404040);
		if (i == sel) {
			self->tab->color = (self->tab->color & 0xffffff) | a24;
			Menu_DrawFrame(self->tab, self->tabSpriteIndex, x - 19.0f, y - 19.0f, 38.0f, 38.0f);
		}
		CPsaImage *img;
		if (i == WEAPON_LASER_WEAPON) {
			img = self->img;
			img->color = color;
			img->scaleX = 0.75f;
			img->scaleY = 0.75f;
			img->pivotX = 0.5f;
			img->pivotY = 0.5f;
			Weapon_DrawAt(img, self->weaponIcons[WEAPON_LASER_WEAPON], x, y);
		} else {
			int u = cWeaponManager_GetUpgrade(wm, i);
			img = self->img3D;
			img->color = color;
			img->scaleX = 0.75f;
			img->scaleY = 0.75f;
			img->pivotX = 0.5f;
			img->pivotY = 0.5f;
			Weapon_DrawAt(img, Weapon_Icon(self, i, u), x, y);
		}
		img->scaleX = 1.0f;
		img->scaleY = 1.0f;
		img->pivotY = 0.0f;
		img->pivotX = 0.0f;
	}
}

/* vf00: the weapon of the ship of the camera with its upgrade bar, its ammo, the slots mounted, the row of the
 * weapons after a change and the fade between the old and the new weapon icon */
void __thiscall cWeaponScorer_Draw(cWeaponScorer *self, int player) {
	float uv0[2], uv1[2], sv0[2], sv1[2];
	cEntity *ship;

	float time = Scorer_Get(player)->time;
	float frameTime = Scorer_Get(player)->frameTime;
	if (!ScorerHud_CamShip(player, &ship)) {
		return;
	}
	cVehicleEntity *v = Veh(ship);
	cWeaponManager *wm = Entity_TargetLock(ship);
	int w = wm->curWeap;
	int kind = TABLE_AT(g_pWorld->weapons, w)->ammo;
	int upg = cWeaponManager_GetUpgrade(wm, w);
	int sel = Controls_GetWeapon(v->controls);
	if (self->lastWeapon == -100) {
		self->lastWeapon = w;
		self->lastUpgrade = upg;
	}
	CPsaImage *img = self->img;
	img->color = 0xffffffff;
	CPsaImage_DrawFrame(img, self->backIndex, self->backOffX + self->base.x, self->backOffY + self->base.y);
	if (upg) {
		float f = Weap_X87Div(cWeaponManager_GetUAmmo(wm, w), WEAPON_UAMMO_MAX);
		CPsaImage_GetFrameUV(img, self->upgradeBarIndex, uv0, uv1);
		img->color = self->upgradeBarColor;
		Menu_DrawBar(img, self->upgradeBarOffX + self->base.x, self->upgradeBarOffY + self->base.y, f, 5, 0.0f, uv0[0],
			uv0[1], uv1[0], uv1[1], 0.99f, 0.99f, 0.99f, 0.99f, 0, 0);
	}
	if (self->ammoBackIndex) {
		img->color = self->backColor;
		CPsaImage_DrawFrame(img, self->ammoBackIndex, self->ammoBackOffX + self->base.x,
			self->ammoBackOffY + self->base.y);
	}
	if (kind == WEAPON_AMMO_CHARGED) {
		Weapon_DrawCharge(self, wm, w);
	} else {
		Weapon_DrawAmmo(self, wm, w, kind, time);
	}
	img->color = 0xffffffff;
	/* the slots of the weapon mounted, a bar of quarters */
	int n = 0;
	for (int s = 0; s < 4; s++) {
		if (cTargetLock_HasSlot(wm, w, s)) {
			n++;
		}
	}
	float f = (float)n * 0.25f;
	if (f > 0.0f && 1.0f > f) {
		f = f - 0.05f;
	}
	CPsaImage_GetFrameUV(img, self->slotsIndex[1], uv0, uv1);
	CPsaImage_GetFrameUV(img, self->slotsIndex[0], sv0, sv1);
	Menu_DrawBar(img, self->slotsOffX + self->base.x, self->slotsOffY + self->base.y, f, 0, 0.0f, uv0[0], uv0[1], uv1[0],
		uv1[1], sv0[0], sv0[1], sv1[0], sv1[1], 0, 0);
	/* the row of the slots shows again after a change of the weapon or of the selection */
	float show = g_WeaponChangeShowTime;
	if ((v->life > 0 && show > 0.0f && w != sel) || w != self->lastWeapon) {
		self->slotsTime = show;
		g_WeaponSlotCount = 0;
		int k = WEAPON_LASER_WEAPON;
		for (int i = 0; i < WEAPONSCORER_WEAPONS; i++) {
			g_WeaponSlotPlace[k] = g_WeaponSlotCount;
			if (!cWeaponManager_GetSlots(wm, k)) {
				g_WeaponSlotState[k] = 0;
			} else {
				g_WeaponSlotCount++;
				g_WeaponSlotState[k] = (byte)cWeaponManager_HasMinAmmo(wm, (uint32_t)k) ? 1 : 2;
			}
			k = TABLE_AT(g_pWorld->weapons, k)->next;
		}
	}
	if (self->slotsTime > 0.0f) {
		Weapon_DrawSlots(self, wm, player, sel, frameTime);
	}
	/* the old icon fades out, the new one in */
	int fadeOld = 0, fadeNew = 0xff;
	if (w != self->lastWeapon || upg != self->lastUpgrade) {
		float c = self->changeTime, k = 0.0f;
		if (c > 0.0f) {
			k = c / WEAPON_CHANGE_TIME;
			self->changeTime = c - frameTime;
		} else {
			self->changeTime = WEAPON_CHANGE_TIME;
			self->lastWeapon = w;
			self->lastUpgrade = upg;
		}
		fadeOld = (int)(k * 255.0f);
		fadeNew = (int)((1.0f - k) * 255.0f);
	}
	if (w != 2 && w != 4 && w != WEAPON_LASER_WEAPON) {
		/* the shadow */
		self->img3D->color = ((uint32_t)fadeNew & ~1u) << 23 | 0x80d40;
		Weapon_DrawAt(self->img3D, Weapon_Icon(self, w, upg), self->base.x + 3.0f + self->weaponIconOffX,
			self->base.y + 3.0f + self->weaponIconOffY);
	}
	if (fadeOld > 0) {
		uint32_t c = (uint32_t)fadeOld << 24 | 0xffffff;
		if (self->lastWeapon == WEAPON_LASER_WEAPON) {
			self->img->color = c;
			Weapon_DrawAt(self->img, self->weaponIcons[self->lastWeapon], self->weaponIconOffX + self->base.x,
				self->weaponIconOffY + self->base.y);
		} else {
			self->img3D->color = c;
			Weapon_DrawAt(self->img3D, Weapon_Icon(self, self->lastWeapon, self->lastUpgrade),
				self->weaponIconOffX + self->base.x, self->weaponIconOffY + self->base.y);
		}
	}
	uint32_t c = (uint32_t)fadeNew << 24 | 0xffffff;
	if (w == WEAPON_LASER_WEAPON) {
		self->img->color = c;
		Weapon_DrawAt(self->img, self->weaponIcons[WEAPON_LASER_WEAPON], self->weaponIconOffX + self->base.x,
			self->weaponIconOffY + self->base.y);
	} else {
		self->img3D->color = c;
		Weapon_DrawAt(self->img3D, Weapon_Icon(self, w, upg), self->weaponIconOffX + self->base.x,
			self->weaponIconOffY + self->base.y);
	}
}
