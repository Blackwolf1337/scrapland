/*
 * ScraplandScorer.cpp of the game (Menu/): cScraplandMainMenuScorer and cScraplandSubMenuScorer, the animated
 * logos of the menus. The draw of the main menu (rva 0x176130) is not decompiled. See include/scrap/scoreritems.h.
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

/* the random wait of the sub menu logo until its next ring: short, middle or (mostly) long */
static float SubMenu_RandWait(void) {
	if (crt_rand() % 4 == 0) {
		return ScraplandScorer_Rand(K(0x3c4100), K(0x3aeb88));
	}
	if (crt_rand() % 4 == 0) {
		return ScraplandScorer_Rand(K(0x3ae240), K(0x3abbc0));
	}
	return ScraplandScorer_Rand(K(0x3c40f0), K(0x3aeb68));
}

/* the random length of a ring */
static float SubMenu_RandRing(void) {
	if (crt_rand() & 1) {
		return ScraplandScorer_Rand(K(0x3aeb50), K(0x3c40e0));
	}
	return ScraplandScorer_Rand(K(0x3c40c0), K(0x3c40d8));
}

void __fastcall cScraplandSubMenuScorer_Init(cScraplandSubMenuScorer *self) {
	self->time = (float)g_pApp->vftable->GetGameTime(g_pApp);
	if (g_SubMenuAnimInit) {
		return;
	}
	g_SubMenuAnimInit = 1;
	g_SubMenuWait = SubMenu_RandWait();
	float v = SubMenu_RandRing();
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

#define SUBMENU_MAX_DT 0.2f
#define SUBMENU_REFLECTION_SPEED 0.15f
#define SUBMENU_REFLECTION_SIZE 0.56f	/* the reflection square turns inside the ring */
#define SUBMENU_REFLECTION_SCALE 4.0f
#define SUBMENU_REFLECTION_OFS 2.0f
#define SUBMENU_REFLECTION_HALF 128.0f	/* the half size of the ring images */
#define SUBMENU_RING_FRAMES 9
#define SUBMENU_RING_STEPS (-8.0f)
#define SUBMENU_LIGHTING_Y 11.0f

static void SubMenu_Draw(CPsaImage *img, float scale, uint32_t color, byte setColor, float x, float y) {
	img->pivotX = 0.5f;
	img->pivotY = 0.5f;
	img->scaleX = scale;
	img->scaleY = scale;
	if (setColor) {
		img->color = color;
	}
	CPsaImage_Draw(img, x, y);
}

/* vf00: the logo, the ring mask, its reflection turning back and forth, the ring border and the flickering glow;
 * after a random wait a ring of lightning frames runs in or out */
void __thiscall cScraplandSubMenuScorer_Draw(cScraplandSubMenuScorer *self, int player) {
	static const float uv[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
	static const float corners[4][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
	float pos[4][2];

	(void)player;
	float t = (float)g_pApp->vftable->GetGameTime(g_pApp);
	self->now = t;
	float dt = t - self->time;
	self->time = t;
	dt = Float_MinSS(SUBMENU_MAX_DT, dt);
	float x = self->base.x, y = self->base.y;
	SubMenu_Draw(self->logo, self->scale, 0, 0, x, y);
	SubMenu_Draw(self->ringMask, self->scale, 0xffffffff, 1, x, y);
	self->ringReflection->color = 0x80ffffff;
	double c = Game_Cos((double)(self->now * SUBMENU_REFLECTION_SPEED)) * K(0x3ae448);	/* -pi */
	float a = (float)(c + c);
	for (int k = 0; k < 4; k++) {
		float r[2];
		Vec2_Rotate(r, corners[k], a);
		pos[k][0] = r[0] * SUBMENU_REFLECTION_SIZE * SUBMENU_REFLECTION_SCALE + SUBMENU_REFLECTION_OFS;
		pos[k][1] = r[1] * SUBMENU_REFLECTION_SIZE * SUBMENU_REFLECTION_SCALE + SUBMENU_REFLECTION_OFS;
	}
	CPsaImage *img = self->ringReflection;
	img->scaleX = self->scale;
	img->scaleY = self->scale;
	float h = self->scale * SUBMENU_REFLECTION_HALF;
	img->x = x - h;
	img->y = y - h;
	CPsaImage_DrawPoly(img, &pos[0][0], &uv[0][0], NULL, 4, 6);
	Eng_ResetViewRect();
	SubMenu_Draw(self->ringBorder, self->scale, 0xffffffff, 1, x, y);
	img = self->glow;
	img->pivotX = 0.5f;
	img->pivotY = 0.5f;
	img->scaleX = self->scale;
	img->scaleY = self->scale;
	img->color = (uint32_t)(crt_rand() % 24 - 0x22) << 24 | 0xffffff;
	CPsaImage_Draw(img, x, y);
	if (g_SubMenuRingTime > 0.0f) {
		g_SubMenuRingTime = g_SubMenuRingTime - dt;
		int r = crt_rand();
		float f = g_SubMenuRingTime / g_SubMenuRingWait * SUBMENU_RING_STEPS;
		uint32_t alpha = 0xff - (uint32_t)(r % 3) * 16;
		int frame = g_SubMenuRingFrame - (int)f;
		if (frame > SUBMENU_RING_FRAMES) {
			frame %= SUBMENU_RING_FRAMES;
		}
		int n = g_SubMenuRingDir ? SUBMENU_RING_FRAMES - frame : frame + 1;
		if ((uint32_t)(n - 1) <= SUBMENU_RING_FRAMES - 1) {
			img = self->lighting;
			img->scaleX = self->scale;
			img->scaleY = self->scale;
			img->color = (img->color & 0xffffff) | alpha << 24;
			img->pivotX = 0.5f;
			img->pivotY = 0.5f;
			CPsaImage_DrawFrame(img, n, self->base.x, self->scale * SUBMENU_LIGHTING_Y + self->base.y);
		}
		return;
	}
	g_SubMenuWait = g_SubMenuWait - dt;
	if (0.0f < g_SubMenuWait) {
		return;
	}
	g_SubMenuWait = SubMenu_RandWait();
	float v = SubMenu_RandRing();
	g_SubMenuRingWait = v;
	g_SubMenuRingTime = v;
	g_SubMenuRingDir = crt_rand() & 1;
	g_SubMenuRingFrame = crt_rand() % SUBMENU_RING_FRAMES;
}
