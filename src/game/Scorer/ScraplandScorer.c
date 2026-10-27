/*
 * ScraplandScorer.cpp of the game (Menu/): cScraplandMainMenuScorer and cScraplandSubMenuScorer, the animated
 * logos of the menus. The draws (rva 0x176130 and 0x176d10) are not decompiled. See include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/scoreritems.h"

#define SCRAPLAND_WHITE "2D/white.tga"
/* the double constants of the random times */
#define K(rva) GAME_DATA(double, rva)

/* a random time: rand() / 32767 * range + add, as a float */
static float ScraplandScorer_Rand(double range, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * g_ScorerRandScale * range + add);
}

cScraplandMainMenuScorer *__fastcall cScraplandMainMenuScorer_ctor(cScraplandMainMenuScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CSCRAPLANDMAINMENUSCORER_VFTABLE);
	self->highRes = 0;
	self->time = 0.0f;
	self->now = 0.0f;
	self->logo = NULL;
	self->glow = NULL;
	self->neon = NULL;
	self->lighting = NULL;
	self->logo = ScorerImg_Replace(NULL, SCRAPLAND_WHITE, 3, 0x20);
	self->glow = ScorerImg_Replace(self->glow, SCRAPLAND_WHITE, 2, 0x20);
	self->neon = ScorerImg_Replace(self->neon, SCRAPLAND_WHITE, 2, 0x20);
	cScraplandMainMenuScorer_SetLighting(self, 0x20, SCRAPLAND_WHITE);
	cScraplandMainMenuScorer_Init(self);
	return self;
}

cScraplandSubMenuScorer *__fastcall cScraplandSubMenuScorer_ctor(cScraplandSubMenuScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CSCRAPLANDSUBMENUSCORER_VFTABLE);
	cString_ctor(&self->file);
	self->highRes = 0;
	self->scale = 1.0f;
	self->time = 0.0f;
	self->now = 0.0f;
	self->logo = NULL;
	self->glow = NULL;
	self->lighting = NULL;
	self->ringMask = NULL;
	self->ringReflection = NULL;
	self->ringBorder = NULL;
	self->logo = ScorerImg_Replace(NULL, SCRAPLAND_WHITE, 3, 0x40020);
	self->glow = ScorerImg_Replace(self->glow, SCRAPLAND_WHITE, 2, 0x40020);
	cScraplandSubMenuScorer_SetLighting(self, 0x20, SCRAPLAND_WHITE);
	self->ringMask = ScorerImg_Replace(self->ringMask, SCRAPLAND_WHITE, 2, 0x41020);
	self->ringReflection = ScorerImg_Replace(self->ringReflection, SCRAPLAND_WHITE, 2, 0x42030);
	self->ringBorder = ScorerImg_Replace(self->ringBorder, SCRAPLAND_WHITE, 3, 0x40020);
	self->base.x = 320.0f;
	self->base.y = 240.0f;
	cScraplandSubMenuScorer_Init(self);
	return self;
}

/* the image deleted and its slot cleared */
static void ScraplandScorer_Drop(CPsaImage **slot) {
	if (*slot) {
		ScorerImg_Free(*slot);
		*slot = NULL;
	}
}

void __fastcall cScraplandMainMenuScorer_dtor(cScraplandMainMenuScorer *self) {
	self->base.vftable = Game_Ptr(CSCRAPLANDMAINMENUSCORER_VFTABLE);
	ScraplandScorer_Drop(&self->logo);
	ScraplandScorer_Drop(&self->glow);
	ScraplandScorer_Drop(&self->neon);
	ScraplandScorer_Drop(&self->lighting);
	cScorerItem_dtor(&self->base);
}

void __fastcall cScraplandSubMenuScorer_dtor(cScraplandSubMenuScorer *self) {
	self->base.vftable = Game_Ptr(CSCRAPLANDSUBMENUSCORER_VFTABLE);
	ScraplandScorer_Drop(&self->logo);
	ScraplandScorer_Drop(&self->glow);
	ScraplandScorer_Drop(&self->lighting);
	ScraplandScorer_Drop(&self->ringMask);
	ScraplandScorer_Drop(&self->ringReflection);
	ScraplandScorer_Drop(&self->ringBorder);
	cString_dtor(&self->file);
	cScorerItem_dtor(&self->base);
}

cScraplandMainMenuScorer *__thiscall cScraplandMainMenuScorer_Delete(cScraplandMainMenuScorer *self, byte flags) {
	cScraplandMainMenuScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cScraplandMainMenuScorer));
	}
	return self;
}

cScraplandSubMenuScorer *__thiscall cScraplandSubMenuScorer_Delete(cScraplandSubMenuScorer *self, byte flags) {
	cScraplandSubMenuScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cScraplandSubMenuScorer));
	}
	return self;
}

void __fastcall cScraplandMainMenuScorer_Init(cScraplandMainMenuScorer *self) {
	self->time = (float)g_pApp->vftable->GetGameTime(g_pApp);
	if (g_MainMenuAnimInit) {
		return;
	}
	g_MainMenuAnimInit = 1;
	for (int k = 0; k < 9; k++) {
		g_MainMenuWait[k] = ScraplandScorer_Rand(K(0x3c4118), K(0x3c4108));
		float len = ScraplandScorer_Rand(K(0x3c40b8), K(0x3aeb60));
		g_MainMenuLength[k] = len;
		switch (k) {
		case 0:
		case 1:
			g_MainMenuLength[k] = len * 1.6f;
			break;
		case 2:
		case 3:
			g_MainMenuLength[k] = len * 1.5f;
			break;
		case 4:
		case 8:
			g_MainMenuLength[k] = len * 1.9f;
			break;
		case 5:
			g_MainMenuLength[k] = len * 1.3f;
			break;
		case 7:
			g_MainMenuLength[k] = len;
			break;
		}
		g_MainMenuTime[k] = 0.0f;
	}
	for (int k = 0; k < 19; k++) {
		g_MainMenuSparkWait[k] = ScraplandScorer_Rand(K(0x3b11a0), 0.0);
		int r = crt_rand() & 0x7fff;
		g_MainMenuSparkTime[k] = 0.0f;
		g_MainMenuSparkLength[k] = (float)((double)r * g_ScorerRandScale * K(0x3b9748) + K(0x3c40e8));
		g_MainMenuSparkFrame[k] = crt_rand() % 14;
		g_MainMenuSparkDir[k] = (byte)crt_rand() & 1;
	}
	for (int k = 0; k < 21; k++) {
		if (crt_rand() & 1) {
			g_MainMenuNeonWait[k] = ScraplandScorer_Rand(K(0x3ae240), K(0x3ae440));
		} else {
			g_MainMenuNeonWait[k] = ScraplandScorer_Rand(K(0x3ae440), K(0x3abbc0));
		}
	}
	g_MainMenuNeonGroup[0] = ScraplandScorer_Rand(K(0x3b1f08), K(0x3aeba0));
	g_MainMenuNeonGroup[1] = ScraplandScorer_Rand(K(0x3b1f08), K(0x3ae260));
	g_MainMenuNeonGroup[2] = ScraplandScorer_Rand(K(0x3b1f08), K(0x3b1f08));
	g_MainMenuNeonNext = ScraplandScorer_Rand(K(0x3b11a0), K(0x3afd68));
}

void __fastcall cScraplandSubMenuScorer_Init(cScraplandSubMenuScorer *self) {
	float v;

	self->time = (float)g_pApp->vftable->GetGameTime(g_pApp);
	if (g_SubMenuAnimInit) {
		return;
	}
	g_SubMenuAnimInit = 1;
	if (crt_rand() % 4 == 0) {
		v = ScraplandScorer_Rand(K(0x3c4100), K(0x3aeb88));
	} else if (crt_rand() % 4 == 0) {
		v = ScraplandScorer_Rand(K(0x3ae240), K(0x3abbc0));
	} else {
		v = ScraplandScorer_Rand(K(0x3c40f0), K(0x3aeb68));
	}
	g_SubMenuWait = v;
	if (crt_rand() & 1) {
		v = ScraplandScorer_Rand(K(0x3aeb50), K(0x3c40e0));
	} else {
		v = ScraplandScorer_Rand(K(0x3c40c0), K(0x3c40d8));
	}
	g_SubMenuRingTime = 0.0f;
	g_SubMenuRingWait = v;
}

/* the flags of a lighting file: the white texture without 0x40 (and above bit 15), any other with it */
static uint32_t ScraplandScorer_LightingFlags(uint32_t flags, const char *file) {
	if (!strcmp(file, SCRAPLAND_WHITE)) {
		return flags & 0xffbf;
	}
	return flags | 0x40;
}

void __thiscall cScraplandMainMenuScorer_SetGlow(cScraplandMainMenuScorer *self, uint32_t flags, const char *file) {
	self->glow = ScorerImg_Replace(self->glow, file, 2, flags);
}

void __thiscall cScraplandSubMenuScorer_SetGlow(cScraplandSubMenuScorer *self, uint32_t flags, const char *file) {
	self->glow = ScorerImg_Replace(self->glow, file, 2, flags | 0x40000);
}

void __thiscall cScraplandMainMenuScorer_SetLighting(cScraplandMainMenuScorer *self, uint32_t flags,
	const char *file) {
	CPsaImage *old = self->lighting;
	if (old) {
		ScorerImg_Free(old);
	}
	self->lighting = ScorerImg_Replace(NULL, file, 2, ScraplandScorer_LightingFlags(flags, file));
}

void __thiscall cScraplandSubMenuScorer_SetLighting(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file) {
	CPsaImage *old = self->lighting;
	if (old) {
		ScorerImg_Free(old);
	}
	self->lighting = ScorerImg_Replace(NULL, file, 2, ScraplandScorer_LightingFlags(flags, file) | 0x40000);
}

void __thiscall cScraplandMainMenuScorer_SetLogo(cScraplandMainMenuScorer *self, uint32_t flags, const char *file) {
	self->logo = ScorerImg_Replace(self->logo, file, 3, flags);
}

void __thiscall cScraplandSubMenuScorer_SetLogo(cScraplandSubMenuScorer *self, uint32_t flags, const char *file) {
	self->logo = ScorerImg_Replace(self->logo, file, 3, flags | 0x40000);
}

void __thiscall cScraplandMainMenuScorer_SetNeon(cScraplandMainMenuScorer *self, uint32_t flags, const char *file) {
	self->neon = ScorerImg_Replace(self->neon, file, 2, flags);
}

void __thiscall cScraplandSubMenuScorer_SetRingBorder(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file) {
	self->ringBorder = ScorerImg_Replace(self->ringBorder, file, 3, flags | 0x40000);
}

void __thiscall cScraplandSubMenuScorer_SetRingMask(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file) {
	self->ringMask = ScorerImg_Replace(self->ringMask, file, 2, flags | 0x41000);
}

void __thiscall cScraplandSubMenuScorer_SetRingReflection(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file) {
	self->ringReflection = ScorerImg_Replace(self->ringReflection, file, 2, flags | 0x42010);
}

int __thiscall cScraplandMainMenuScorer_Var(cScraplandMainMenuScorer *self, const char *name, byte set, char **s,
	double *d, int *i) {
	if (i && !crt_stricmp(name, "HighRes")) {
		CHECK(*i == 0 || *i == 1);
		return ScorerVar_Int(&self->highRes, set, i);
	}
	if (s) {
		if (!crt_stricmp(name, "FileLogo") && set) {
			cScraplandMainMenuScorer_SetLogo(self, self->highRes ? 0x10020 : 0x20, *s);
			return 1;
		}
		if (!crt_stricmp(name, "FileGlow") && set) {
			cScraplandMainMenuScorer_SetGlow(self, self->highRes ? 0x10020 : 0x20, *s);
			return 1;
		}
		if (!crt_stricmp(name, "FileNeon") && set) {
			cScraplandMainMenuScorer_SetNeon(self, self->highRes ? 0x10060 : 0x60, *s);
			return 1;
		}
		if (!crt_stricmp(name, "FileLighting") && set) {
			cScraplandMainMenuScorer_SetLighting(self, self->highRes ? 0x10020 : 0x20, *s);
			return 1;
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

int __thiscall cScraplandSubMenuScorer_Var(cScraplandSubMenuScorer *self, const char *name, byte set, char **s,
	double *d, int *i) {
	if (s) {
		uint32_t flags = 0;
		int k = -1;
		static const char *const files[6] = {"FileLogo", "FileGlow", "FileLighting", "FileRingMask",
			"FileRingReflection", "FileRingBorder"};
		for (int n = 0; n < 6; n++) {
			if (!crt_stricmp(name, files[n]) && set) {
				k = n;
				break;
			}
		}
		if (k >= 0) {
			flags = self->highRes ? 0x10020 : 0x20;
			switch (k) {
			case 0:
				cScraplandSubMenuScorer_SetLogo(self, flags, *s);
				break;
			case 1:
				cScraplandSubMenuScorer_SetGlow(self, flags, *s);
				break;
			case 2:
				cScraplandSubMenuScorer_SetLighting(self, flags, *s);
				break;
			case 3:
				cScraplandSubMenuScorer_SetRingMask(self, flags, *s);
				break;
			case 4:
				cScraplandSubMenuScorer_SetRingReflection(self, flags, *s);
				break;
			default:
				cScraplandSubMenuScorer_SetRingBorder(self, flags, *s);
				break;
			}
			cString_Set(&self->file, *s);
			return 1;
		}
	}
	if (i && !crt_stricmp(name, "HighRes")) {
		CHECK(*i == 0 || *i == 1);
		return ScorerVar_Int(&self->highRes, set, i);
	}
	if (d && !crt_stricmp(name, "Scale")) {
		return ScorerVar_Float(&self->scale, set, d);
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}
