/*
 * WeaponScorer.cpp of the game (Menu/): cWeaponScorer, the weapon of the ship of the camera with its ammo, its
 * upgrades and the weapon slots. See include/scrap/scorerhud.h.
 */
#include "scrap/scorerhud.h"

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
	self->base.vftable = Game_Ptr(CWEAPONSCORER_VFTABLE);
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
	self->base.vftable = Game_Ptr(CWEAPONSCORER_VFTABLE);
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
