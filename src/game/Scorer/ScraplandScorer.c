/*
 * ScraplandScorer.cpp of the game (Menu/): cScraplandMainMenuScorer and cScraplandSubMenuScorer, the animated
 * logos of the menus. See include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/scoreritems.h"

#define SCRAPLAND_WHITE "2D/white.tga"
/* a random time: rand() / 32767 * range + add, as a float */
static float ScraplandScorer_Rand(double range, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * range + add);
}

cScraplandMainMenuScorer *cScraplandMainMenuScorer_ctor(cScraplandMainMenuScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CSCRAPLANDMAINMENUSCORER_VFTABLE;
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

cScraplandSubMenuScorer *cScraplandSubMenuScorer_ctor(cScraplandSubMenuScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CSCRAPLANDSUBMENUSCORER_VFTABLE;
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

void cScraplandMainMenuScorer_dtor(cScraplandMainMenuScorer *self) {
	self->base.vftable = CSCRAPLANDMAINMENUSCORER_VFTABLE;
	ScraplandScorer_Drop(&self->logo);
	ScraplandScorer_Drop(&self->glow);
	ScraplandScorer_Drop(&self->neon);
	ScraplandScorer_Drop(&self->lighting);
	cScorerItem_dtor(&self->base);
}

void cScraplandSubMenuScorer_dtor(cScraplandSubMenuScorer *self) {
	self->base.vftable = CSCRAPLANDSUBMENUSCORER_VFTABLE;
	ScraplandScorer_Drop(&self->logo);
	ScraplandScorer_Drop(&self->glow);
	ScraplandScorer_Drop(&self->lighting);
	ScraplandScorer_Drop(&self->ringMask);
	ScraplandScorer_Drop(&self->ringReflection);
	ScraplandScorer_Drop(&self->ringBorder);
	cString_dtor(&self->file);
	cScorerItem_dtor(&self->base);
}

cScorerItem *cScraplandMainMenuScorer_Delete(cScorerItem *base, byte flags) {
	cScraplandMainMenuScorer *self = (cScraplandMainMenuScorer *)base;
	cScraplandMainMenuScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cScraplandMainMenuScorer));
	}
	return base;
}

cScorerItem *cScraplandSubMenuScorer_Delete(cScorerItem *base, byte flags) {
	cScraplandSubMenuScorer *self = (cScraplandSubMenuScorer *)base;
	cScraplandSubMenuScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cScraplandSubMenuScorer));
	}
	return base;
}

void cScraplandMainMenuScorer_Init(cScraplandMainMenuScorer *self) {
	self->time = (float)g_pApp->vftable->GetGameTime(g_pApp);
	if (g_MainMenuAnimInit) {
		return;
	}
	g_MainMenuAnimInit = 1;
	for (int k = 0; k < 9; k++) {
		g_MainMenuWait[k] = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3c4118), GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3c4108));
		float len = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Init_3c40b8), GAME_GLOBAL(double, cScraplandMainMenuScorer_Init_3aeb60));
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
		g_MainMenuSparkWait[k] = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3b11a0), 0.0);
		int r = crt_rand() & 0x7fff;
		g_MainMenuSparkTime[k] = 0.0f;
		g_MainMenuSparkLength[k] = (float)((double)r * RAND_SCALE * GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3b9748) + GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3c40e8));
		g_MainMenuSparkFrame[k] = crt_rand() % 14;
		g_MainMenuSparkDir[k] = (byte)crt_rand() & 1;
	}
	for (int k = 0; k < 21; k++) {
		if (crt_rand() & 1) {
			g_MainMenuNeonWait[k] = ScraplandScorer_Rand(GAME_GLOBAL(double, SubMenu_RandWait_3ae240), GAME_GLOBAL(double, cScraplandMainMenuScorer_Init_3ae440));
		} else {
			g_MainMenuNeonWait[k] = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Init_3ae440), GAME_GLOBAL(double, SubMenu_RandWait_3abbc0));
		}
	}
	g_MainMenuNeonGroup[0] = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Init_3b1f08), GAME_GLOBAL(double, cScraplandMainMenuScorer_Init_3aeba0));
	g_MainMenuNeonGroup[1] = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Init_3b1f08), GAME_GLOBAL(double, cScraplandMainMenuScorer_Init_3ae260));
	g_MainMenuNeonGroup[2] = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Init_3b1f08), GAME_GLOBAL(double, cScraplandMainMenuScorer_Init_3b1f08));
	g_MainMenuNeonNext = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3b11a0), GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3afd68));
}

/* the random wait of the sub menu logo until its next ring: short, middle or (mostly) long */
static float SubMenu_RandWait(void) {
	if (crt_rand() % 4 == 0) {
		return ScraplandScorer_Rand(GAME_GLOBAL(double, SubMenu_RandWait_3c4100), GAME_GLOBAL(double, SubMenu_RandWait_3aeb88));
	}
	if (crt_rand() % 4 == 0) {
		return ScraplandScorer_Rand(GAME_GLOBAL(double, SubMenu_RandWait_3ae240), GAME_GLOBAL(double, SubMenu_RandWait_3abbc0));
	}
	return ScraplandScorer_Rand(GAME_GLOBAL(double, SubMenu_RandWait_3c40f0), GAME_GLOBAL(double, SubMenu_RandWait_3aeb68));
}

/* the random length of a ring */
static float SubMenu_RandRing(void) {
	if (crt_rand() & 1) {
		return ScraplandScorer_Rand(GAME_GLOBAL(double, SubMenu_RandRing_3aeb50), GAME_GLOBAL(double, SubMenu_RandRing_3c40e0));
	}
	return ScraplandScorer_Rand(GAME_GLOBAL(double, SubMenu_RandRing_3c40c0), GAME_GLOBAL(double, SubMenu_RandRing_3c40d8));
}

void cScraplandSubMenuScorer_Init(cScraplandSubMenuScorer *self) {
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

void cScraplandMainMenuScorer_SetGlow(cScraplandMainMenuScorer *self, uint32_t flags, const char *file) {
	self->glow = ScorerImg_Replace(self->glow, file, 2, flags);
}

void cScraplandSubMenuScorer_SetGlow(cScraplandSubMenuScorer *self, uint32_t flags, const char *file) {
	self->glow = ScorerImg_Replace(self->glow, file, 2, flags | 0x40000);
}

void cScraplandMainMenuScorer_SetLighting(cScraplandMainMenuScorer *self, uint32_t flags,
	const char *file) {
	CPsaImage *old = self->lighting;
	if (old) {
		ScorerImg_Free(old);
	}
	self->lighting = ScorerImg_Replace(NULL, file, 2, ScraplandScorer_LightingFlags(flags, file));
}

void cScraplandSubMenuScorer_SetLighting(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file) {
	CPsaImage *old = self->lighting;
	if (old) {
		ScorerImg_Free(old);
	}
	self->lighting = ScorerImg_Replace(NULL, file, 2, ScraplandScorer_LightingFlags(flags, file) | 0x40000);
}

void cScraplandMainMenuScorer_SetLogo(cScraplandMainMenuScorer *self, uint32_t flags, const char *file) {
	self->logo = ScorerImg_Replace(self->logo, file, 3, flags);
}

void cScraplandSubMenuScorer_SetLogo(cScraplandSubMenuScorer *self, uint32_t flags, const char *file) {
	self->logo = ScorerImg_Replace(self->logo, file, 3, flags | 0x40000);
}

void cScraplandMainMenuScorer_SetNeon(cScraplandMainMenuScorer *self, uint32_t flags, const char *file) {
	self->neon = ScorerImg_Replace(self->neon, file, 2, flags);
}

void cScraplandSubMenuScorer_SetRingBorder(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file) {
	self->ringBorder = ScorerImg_Replace(self->ringBorder, file, 3, flags | 0x40000);
}

void cScraplandSubMenuScorer_SetRingMask(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file) {
	self->ringMask = ScorerImg_Replace(self->ringMask, file, 2, flags | 0x41000);
}

void cScraplandSubMenuScorer_SetRingReflection(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file) {
	self->ringReflection = ScorerImg_Replace(self->ringReflection, file, 2, flags | 0x42010);
}

int cScraplandMainMenuScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cScraplandMainMenuScorer *self = (cScraplandMainMenuScorer *)base;
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

int cScraplandSubMenuScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cScraplandSubMenuScorer *self = (cScraplandSubMenuScorer *)base;
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
void cScraplandSubMenuScorer_Draw(cScorerItem *base, int player) {
	cScraplandSubMenuScorer *self = (cScraplandSubMenuScorer *)base;
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
	double c = cos((double)(self->now * SUBMENU_REFLECTION_SPEED)) * GAME_GLOBAL(double, cScraplandSubMenuScorer_Draw_3ae448);	/* -pi */
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

/* the main menu logo: the screen positions of the lighting frames and of the sparks */
#define g_MainMenuLightPos (GAME_ARRAY(const float[2], g_MainMenuLightPos))	/* [5] */
#define g_MainMenuSparkPos (GAME_ARRAY(const float[2], g_MainMenuSparkPos))	/* [MAINMENU_SPARKS] */

#define MAINMENU_MAX_DT 0.2f
#define MAINMENU_NEONS 21		/* the neon frames 18.. */
#define MAINMENU_NEON_FIRST 18
#define MAINMENU_LETTERS 9		/* the letters, neon frames 1..9 */
#define MAINMENU_SPARKS 19
#define MAINMENU_SHEET 256.0f		/* the neon sheet: the half size of the logo, the frame offsets of 512 */
#define MAINMENU_ACCEPT_ACTION 10	/* of the menu action set: held on the "play" button flickers letter 0 */

/* the lighting frames of a letter while it runs: base - (int)(time / length * scale) when it is in
 * lo..lo + num - 1, at the position pos */
typedef struct MainMenuLight {
	int letter;
	int base;
	float scale;
	int lo, num;
	int pos;
} MainMenuLight;

static const MainMenuLight MainMenu_Lights[] = {
	{7, 1, -8.0f, 1, 8, 0},
	{8, 8, 8.0f, 1, 8, 0},
	{2, 9, -7.0f, 9, 7, 1},
	{2, 34, 10.0f, 25, 10, 3},
	{4, 17, -8.0f, 17, 8, 2},
	{5, 24, 8.0f, 17, 8, 2},
	{1, 25, -10.0f, 25, 10, 3},
	{5, 37, -4.0f, 37, 5, 4},
	{6, 41, 4.0f, 37, 5, 4},
};

/* the remainder of the original's signed rand() % 2^n */
static int MainMenu_RandMod(int n) {
	return crt_rand() % n;
}

/* a frame of the neon sheet at its place in the logo */
static void MainMenu_DrawNeon(cScraplandMainMenuScorer *self, int frame) {
	float uv0[2], uv1[2];

	CPsaImage_GetFrameUV(self->neon, frame, uv0, uv1);
	CPsaImage_DrawFrame(self->neon, frame, self->base.x - MAINMENU_SHEET + uv0[0] * (MAINMENU_SHEET * 2.0f),
		self->base.y + 0.0f + uv0[1] * MAINMENU_SHEET);
}

/* the neon tubes: each flickers while its wait runs, else it is nearly full on */
static void MainMenu_Neons(cScraplandMainMenuScorer *self, float dt) {
	for (int k = 0; k < MAINMENU_NEONS; k++) {
		float *w = &g_MainMenuNeonWait[k];
		int alpha;
		if (*w > 0.0f) {
			if (MainMenu_RandMod(4) == 0) {
				alpha = MainMenu_RandMod(32) + 0x80;
			} else {
				alpha = MainMenu_RandMod(64) + 0x20;
			}
			*w = *w - dt;
		} else {
			alpha = MainMenu_RandMod(35) + 220;
		}
		self->neon->color = (uint32_t)alpha << 24 | 0xffffff;
		MainMenu_DrawNeon(self, MAINMENU_NEON_FIRST + k);
	}
	if (g_MainMenuNeonNext > 0.0f) {
		g_MainMenuNeonNext = g_MainMenuNeonNext - dt;
	} else {
		g_MainMenuNeonNext = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3b11a0), GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3afd68));
		int k = crt_rand() % MAINMENU_NEONS;
		g_MainMenuNeonWait[k] = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3c40c8), GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3aeb38));
	}
	/* the three groups of tubes flicker together */
	static const int groups[3][2] = {{0, 7}, {8, 12}, {13, 20}};
	for (int g = 0; g < 3; g++) {
		if (g_MainMenuNeonGroup[g] > 0.0f) {
			g_MainMenuNeonGroup[g] = g_MainMenuNeonGroup[g] - dt;
			continue;
		}
		g_MainMenuNeonGroup[g] = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3aeba8), 0.0);
		for (int k = groups[g][0]; k <= groups[g][1]; k++) {
			g_MainMenuNeonWait[k] = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3ae218), GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3ae228));
		}
	}
}

/* the lighting of a letter after its random wait, the alpha of the letter */
static int MainMenu_Letter(cScraplandMainMenuScorer *self, int k, float dt) {
	if (!(g_MainMenuTime[k] > 0.0f)) {
		float w = g_MainMenuWait[k] - dt;
		g_MainMenuWait[k] = w;
		if (0.0f >= w) {
			g_MainMenuWait[k] = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3c4118), GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3c4108));
			float len = ScraplandScorer_Rand(GAME_GLOBAL(double, SubMenu_RandRing_3c40c0), GAME_GLOBAL(double, SubMenu_RandWait_3aeb68));
			g_MainMenuLength[k] = len;
			switch (k) {
			case 7:
			case 8:
			case 4:
				g_MainMenuLength[k] = len * 1.6f;
				break;
			case 2:
				g_MainMenuLength[k] = len * 1.4f;
				break;
			case 1:
				g_MainMenuLength[k] = len * 1.9f;
				break;
			case 5:
			case 6:
				g_MainMenuLength[k] = len;
				break;
			}
			g_MainMenuTime[k] = g_MainMenuLength[k];
		}
		return crt_rand() % 55 + 200;
	}
	g_MainMenuTime[k] = g_MainMenuTime[k] - dt;
	int alpha = (crt_rand() % 5 + 2) << 5;
	CPsaImage *img = self->lighting;
	img->color = 0xffffffff;
	img->pivotX = 0.0f;
	img->pivotY = 0.0f;
	for (int i = 0; i < (int)(sizeof(MainMenu_Lights) / sizeof(MainMenu_Lights[0])); i++) {
		const MainMenuLight *l = &MainMenu_Lights[i];
		if (l->letter != k) {
			continue;
		}
		int n = l->base - (int)(g_MainMenuTime[k] / g_MainMenuLength[k] * l->scale);
		if ((uint32_t)(n - l->lo) <= (uint32_t)(l->num - 1)) {
			CPsaImage_DrawFrame(self->lighting, n, g_MainMenuLightPos[l->pos][0], g_MainMenuLightPos[l->pos][1]);
		}
	}
	return alpha;
}

/* the sparks: each one runs its frames after a random wait */
static void MainMenu_Sparks(cScraplandMainMenuScorer *self, float dt) {
	for (int k = 0; k < MAINMENU_SPARKS; k++) {
		if (!(g_MainMenuSparkTime[k] > 0.0f)) {
			float w = g_MainMenuSparkWait[k] - dt;
			g_MainMenuSparkWait[k] = w;
			if (0.0f >= w) {
				g_MainMenuSparkWait[k] = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3c4110), GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3c40f8));
				int r = crt_rand() & 0x7fff;
				/* the original reads the letter lengths by the spark index, past them for k >= 9 into the spark waits
				 * that follow them there (a separate C object since stage 3c2) */
				g_MainMenuSparkTime[k] = k < 9 ? g_MainMenuLength[k] : g_MainMenuSparkWait[k - 9];
				g_MainMenuSparkLength[k] = (float)((double)r * RAND_SCALE * GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3b9748) + GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3c40e8));
				g_MainMenuSparkFrame[k] = crt_rand() % 14;
				g_MainMenuSparkDir[k] = (byte)crt_rand() & 1;
			}
			crt_rand();
			continue;
		}
		g_MainMenuSparkTime[k] = g_MainMenuSparkTime[k] - dt;
		int r = MainMenu_RandMod(4);
		float x = g_MainMenuSparkPos[k][0], y = g_MainMenuSparkPos[k][1];
		CPsaImage *img = self->lighting;
		img->color = (img->color & 0xffffff) | (uint32_t)((r + 12) << 4) << 24;
		img->pivotX = 0.5f;
		img->pivotY = 0.5f;
		int n = g_MainMenuSparkFrame[k] - (int)(g_MainMenuSparkTime[k] / g_MainMenuSparkLength[k] * -14.0f);
		int m = n > 14 ? n - 14 : n;
		if ((uint32_t)m <= 14) {
			CPsaImage_DrawFrame(self->lighting, g_MainMenuSparkDir[k] ? 0x4f - m : m + 0x41, x, y);
		}
	}
}

/* vf00: the logo and its glow, the flickering neon tubes, the letters with their lighting, the sparks and the
 * pulsing "Remastered" */
void cScraplandMainMenuScorer_Draw(cScorerItem *base, int player) {
	cScraplandMainMenuScorer *self = (cScraplandMainMenuScorer *)base;
	static const char remastered[] = "Remastered";
	float pos[2], dim[2];

	float t = (float)g_pApp->vftable->GetGameTime(g_pApp);
	self->now = t;
	float d = t - self->time;
	self->time = t;
	self->logo->pivotX = 0.5f;
	self->logo->pivotY = 0.0f;
	float dt = Float_MinSS(MAINMENU_MAX_DT, d);
	float off = 0.0f;
	CPsaImage_Draw(self->logo, self->base.x, self->base.y);
	self->glow->color = 0xffffffff;
	self->glow->pivotX = 0.5f;
	self->glow->pivotY = 0.0f;
	CPsaImage_Draw(self->glow, self->base.x, self->base.y);
	MainMenu_Neons(self, dt);
	byte accept = 0;
	cScorer *sc = Scorer_Get(player);
	if (sc && sc->cursor) {
		float cx = sc->cursor->x, cy = sc->cursor->y;
		if (cx >= 102.0f && 108.0f >= cx && cy >= 167.0f && 173.0f >= cy) {
			cActionSet *set = TABLE_AT(g_ActionSets, 0);
			accept = TABLE_AT(set->actions, MAINMENU_ACCEPT_ACTION)->value[player] > 0.5f;
		}
	}
	for (int k = 0; k < MAINMENU_LETTERS; k++) {
		int alpha = MainMenu_Letter(self, k, dt);
		if (accept && k == 0) {
			if (MainMenu_RandMod(4) == 0) {
				alpha = MainMenu_RandMod(16) + 0x5c;
			} else {
				alpha = MainMenu_RandMod(32) + 0x20;
			}
		}
		self->neon->color = (self->neon->color & 0xffffff) | (uint32_t)alpha << 24;
		MainMenu_DrawNeon(self, k + 1);
	}
	MainMenu_Sparks(self, dt);
	CFont *font = g_FontScrapMedium;
	for (int i = 0; i < 10; i++) {
		float r = ScraplandScorer_Rand(GAME_GLOBAL(double, cScraplandMainMenuScorer_Draw_3ae270), 0.0);
		double s = sin((double)(self->now * 8.0f - (float)i)) + 1.0;
		font->effects = 0x40000;
		font->align = 0;
		font->color = (uint32_t)(int)(s * 90.0 + (double)r) << 24 | 0xb1eaff;
		Font_TextSize(font, pos, dim, 0.0f, 0.0f, "%c", remastered[i]);
		CFont_PrintfFlags(font, self->base.x + 110.0f + off, self->base.y + 170.0f, 0xb, "%c", remastered[i]);
		off = dim[0] + off;
	}
}

/* the vtable of cScraplandMainMenuScorer */
const cScorerItemVtbl cScraplandMainMenuScorer_vftable = {
	.Draw = cScraplandMainMenuScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cScraplandMainMenuScorer_Var,
	.Delete = cScraplandMainMenuScorer_Delete,
};

/* the vtable of cScraplandSubMenuScorer */
const cScorerItemVtbl cScraplandSubMenuScorer_vftable = {
	.Draw = cScraplandSubMenuScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cScraplandSubMenuScorer_Var,
	.Delete = cScraplandSubMenuScorer_Delete,
};
