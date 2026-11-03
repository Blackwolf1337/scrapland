/*
 * The HUD and menu items of Menu/ from SpriteScorer.cpp to WindFxScorer.cpp (the classes over cScorerItem of
 * include/scrap/scorer.h, cSpriteScorer and cAbstractTextScorer of include/scrap/scoreritems.h), the menu drawing
 * helpers and the native script callbacks of the menus between them. Draw(player) at vf00, Var at vf02 and the
 * deleting destructor at vf03.
 */
#ifndef SCRAP_SCORERHUD_H
#define SCRAP_SCORERHUD_H

#include "scrap/gvar.h"
#include "scrap/app.h"
#include "scrap/scoreritems.h"
#include "scrap/world.h"

/* the number of frames of the sheet (CHECKs a sheet) */
int CPsaImage_GetNumFrames(CPsaImage *self);

/* a script variable of an item by a table: the kinds below, the field at the offset from the item */
#define SCORERVAR_FLOAT 0	/* double, 2 */
#define SCORERVAR_INT 1		/* int, 3 */
#define SCORERVAR_RED 2		/* the color bytes of an 0xAARRGGBB field, 3 */
#define SCORERVAR_GREEN 3
#define SCORERVAR_BLUE 4
#define SCORERVAR_ALPHA 5
typedef struct ScorerVarDef {
	const char *name;
	byte kind;
	uint16_t ofs;
} ScorerVarDef;

/* the variables of defs[0..n) tried in this order (a float only with d, the others only with i); 0 if none */
int ScorerVar_Table(void *item, const ScorerVarDef *defs, int n, const char *name, byte set, double *d, int *i);
/* a font variable: set only, 0 when the font exists (and is taken), 1 when not */
static inline int ScorerVar_Font(CFont **font, const char *name) {
	CFont *f = Font_Get(name);
	if (!f) {
		return 1;
	}
	*font = f;
	return 0;
}

/* the target of the camera of player (an entity of the type followed by an "FCam"), NULL without a camera entity;
 * 0 when the draw has to stop (the camera is no FCam, it follows nothing or nothing of the type) */
static inline byte ScorerHud_CamTarget(int player, const char *type, cEntity **target) {
	cEntity *cam = World_FindEntity(cWorld_GetCamName(g_pWorld, player));

	*target = NULL;
	if (cam) {
		if (!Entity_IsType(cam, "FCam")) {
			return 0;
		}
		*target = cCamEntity_GetTarget(cam);
		if (!*target || !Entity_IsType(*target, type)) {
			return 0;
		}
	}
	return 1;
}

/* the ship of the camera of player (a "Car") */
static inline byte ScorerHud_CamShip(int player, cEntity **ship) {
	return ScorerHud_CamTarget(player, TYEN_CAR, ship);
}

/* the rectangle mark around a screen point: the corner frame of img four times, turned, d away from scr */
static inline void ScorerHud_DrawCorners(CPsaImage *img, int frame, const float *scr, float d) {
	float uv0[2], uv1[2], uv[14], pos[8];

	CPsaImage_GetFrameUV(img, frame, uv0, uv1);
	float u0 = uv0[0], v0 = uv0[1], u1 = uv1[0], v1 = uv1[1];
	/* the texture coordinates of the four turns, overlapping: (u0 v0) (u1 v0) (u1 v1) (u0 v1) from 0, 2, 4 and 6 */
	const float t[14] = {u0, v0, u1, v0, u1, v1, u0, v1, u0, v0, u1, v0, u1, v1};
	for (int k = 0; k < 14; k++) {
		uv[k] = t[k];
	}
	pos[0] = u0 - u0;
	pos[1] = v0 - v0;
	pos[2] = u1 - u0;
	pos[3] = v0 - v0;
	pos[4] = u1 - u0;
	pos[5] = v1 - v0;
	pos[6] = u0 - u0;
	pos[7] = v1 - v0;
	img->pivotX = (u1 - u0) * 0.5f;
	img->pivotY = (v1 - v0) * 0.5f;
	img->x = scr[0] - d;
	img->y = scr[1] - d;
	CPsaImage_DrawPoly(img, pos, &uv[2], NULL, 4, 6);
	img->x = scr[0] - d;
	img->y = scr[1] + d;
	CPsaImage_DrawPoly(img, pos, &uv[4], NULL, 4, 6);
	img->x = scr[0] + d;
	img->y = scr[1] + d;
	CPsaImage_DrawPoly(img, pos, &uv[6], NULL, 4, 6);
	img->x = scr[0] + d;
	img->y = scr[1] - d;
	CPsaImage_DrawPoly(img, pos, &uv[0], NULL, 4, 6);
}

/* the arrow frame of img turned by a around its middle at x, y (flip: the frame upside down) */
static inline void ScorerHud_DrawArrow(CPsaImage *img, int frame, float a, float x, float y, byte flip) {
	float uv0[2], uv1[2], in[2], r[2];
	float pos[4][2], uv[4][2];

	CPsaImage_GetFrameUV(img, frame, uv0, uv1);
	float hv = (uv1[1] - uv0[1]) * 0.5f;
	float hu = (uv1[0] - uv0[0]) * 0.5f;
	float vt = flip ? uv1[1] : uv0[1], vb = flip ? uv0[1] : uv1[1];
	uv[0][0] = uv0[0];
	uv[0][1] = vt;
	uv[1][0] = uv1[0];
	uv[1][1] = vt;
	uv[2][0] = uv1[0];
	uv[2][1] = vb;
	uv[3][0] = uv0[0];
	uv[3][1] = vb;
	float nu = 0.0f - hu, nv = 0.0f - hv;
	in[0] = nu;
	in[1] = nv;
	Vec2_Rotate(r, in, a);
	pos[0][0] = r[0];
	pos[0][1] = r[1];
	in[0] = uv1[0] - uv0[0] - hu;
	in[1] = nv;
	Vec2_Rotate(r, in, a);
	pos[1][0] = r[0];
	pos[1][1] = r[1];
	in[1] = uv1[1] - uv0[1] - hv;
	in[0] = uv1[0] - uv0[0] - hu;
	Vec2_Rotate(r, in, a);
	pos[2][0] = r[0];
	pos[2][1] = r[1];
	in[0] = nu;
	in[1] = uv1[1] - uv0[1] - hv;
	Vec2_Rotate(r, in, a);
	pos[3][0] = r[0];
	pos[3][1] = r[1];
	img->pivotX = 0.0f;
	img->pivotY = 0.0f;
	img->x = x;
	img->y = y;
	CPsaImage_DrawPoly(img, &pos[0][0], &uv[0][0], NULL, 4, 6);
}

/* the atan2 of the screen point sx, sy seen from the middle of the 640x480 screen (x scaled to a square) */
static inline double ScorerHud_ScreenAtan2(float sx, float sy) {
	double dx = ((double)sx - 320.0) * 240.0 / 320.0;
	double dy = (double)sy - 240.0;
	return atan2(dx, dy);
}

/* the angle of an arrow frame pointing up from the middle of the screen towards the screen point sx, sy */
static inline float ScorerHud_ArrowAngle(float sx, float sy) {
	return -(float)(ScorerHud_ScreenAtan2(sx, sy) + 3.141592653589793);
}

/* ---- SpriteScorer.cpp ---- */

/* vf04 */

/* a sprite filled by four colors at its corners (0xd8 bytes) */
typedef struct cGradientScorer {
	cSpriteScorer base;		/* 0x00 */
	int colorSelected;		/* 0xc4 0..3, the corner of Red, Green, Blue, Alpha */
	uint32_t colors[4];		/* 0xc8 0xffffffff */
} cGradientScorer;

cGradientScorer *cGradientScorer_ctor(cGradientScorer *self);

/* ---- StatusScorer.cpp ---- */

/* the status of the ship of the camera: life, lives, money, boost and hull, the alarm and siren sounds (0x1fc) */
typedef struct cStatusScorer {
	cScorerItem base;		/* 0x000 */
	cSound lowLife;			/* 0x0ac the warning under the low life (looped) */
	cSound alarm;			/* 0x0ec while the police alarm of the world is on (looped) */
	byte alarmOn;			/* 0x12c the alarm of the world in the last frame */
	byte pad_12d[3];
	CPsaImage *img;			/* 0x130 Sprite */
	CPsaImage *glow;		/* 0x134 Sprite, blend 2 */
	float backOffX, backOffY;	/* 0x138 */
	int backIndex;			/* 0x140 */
	float lifeIconOffX, lifeIconOffY;	/* 0x144 */
	int lifeIconIndex;		/* 0x14c */
	float lifeTextOffX, lifeTextOffY;	/* 0x150 */
	CFont *lifeFont;		/* 0x158 Horatio */
	uint32_t lifeTextColor;		/* 0x15c */
	int numLives;			/* 0x160 Player.NumLives */
	float livesTextOffX, livesTextOffY;	/* 0x164 */
	CFont *livesFont;		/* 0x16c Horatio */
	uint32_t livesTextColor;	/* 0x170 */
	float livesBackOffX, livesBackOffY;	/* 0x174 */
	int livesBackIndex;		/* 0x17c */
	float moneyIconOffX, moneyIconOffY;	/* 0x180 */
	int moneyIconIndex;		/* 0x188 */
	float moneyTextOffX, moneyTextOffY;	/* 0x18c */
	CFont *moneyFont;		/* 0x194 Horatio */
	uint32_t moneyTextColor;	/* 0x198 */
	float moneyBackOffX, moneyBackOffY;	/* 0x19c */
	int moneyBackIndex;		/* 0x1a4 */
	int money;			/* 0x1a8 of the world */
	int lastMoney;			/* 0x1ac */
	float moneyBlink;		/* 0x1b0 0.5 after a change, down to 0 */
	float hullIconOffX, hullIconOffY;	/* 0x1b4 */
	int hullIconIndex;		/* 0x1bc */
	float fontBackOffX, fontBackOffY;	/* 0x1c0 */
	int fontBackIndex;		/* 0x1c8 */
	int boostStatus;		/* 0x1cc 0x40 used up, 0x80 ready, 0xff on */
	float boostRemaining;		/* 0x1d0 0..1 */
	float boostTime;		/* 0x1d4 1 */
	float boostOffX, boostOffY;	/* 0x1d8 */
	int boostIconIndex;		/* 0x1e0 */
	int boostTurboIndex;		/* 0x1e4 */
	CPsaImage *flag;		/* 0x1e8 SpriteFlag */
	float flagOffX, flagOffY;	/* 0x1ec */
	uint32_t backColor;		/* 0x1f4 0x5cffcc00 */
	uint32_t textBackColor;		/* 0x1f8 0xffffffff */
} cStatusScorer;

cStatusScorer *cStatusScorer_ctor(cStatusScorer *self);

/* ---- TabScorer.cpp ---- */

/* a frame of cells of a sprite sheet with a tab on top (0xf0 bytes) */
typedef struct cTabScorer {
	cScorerItem base;		/* 0x00 */
	float sizeW, sizeH;		/* 0xac SizeX, SizeY to whole cells */
	float sizeX, sizeY;		/* 0xb4 SizeX, SizeY */
	int cellsX, cellsY;		/* 0xbc */
	int cell;			/* 0xc4 the cell size: 14, 7 for HighRes */
	int highRes;			/* 0xc8 */
	int blend;			/* 0xcc 3 */
	int tabSpriteIndex;		/* 0xd0 the first of the frames */
	int tabInit;			/* 0xd4 the first cell of the tab */
	int tabEnd;			/* 0xd8 */
	int tabMax;			/* 0xdc */
	int sizeTabQuad;		/* 0xe0 the height of the tab in cells, 1 */
	int type;			/* 0xe4 0..4, 4 a plain Menu_DrawFrame */
	int filled;			/* 0xe8 1 */
	CPsaImage *img;			/* 0xec File */
} cTabScorer;

cTabScorer *cTabScorer_ctor(cTabScorer *self);

/* ---- TalkScorer.cpp ---- */

#define TALK_MAX_LINES 40
#define TALK_TEXT_SIZE 0x1900

/* the text of a talk (Scorer_SetSpeechText) above the character and the text of the use of the character in
 * front (0x1a80 bytes) */
struct cTalkScorer;
typedef struct cTalkScorer {
	cScorerItem base;		/* 0x0000 */
	CPsaImage *sprite;		/* 0x00ac Sprite */
	int useIndex;			/* 0x00b0 UseIndex -1 */
	int nextBackIndex;		/* 0x00b4 NextBackIndex -1 */
	int nextIndex;			/* 0x00b8 NextIndex -1 */
	char useText[TALK_TEXT_SIZE];	/* 0x00bc */
	char *useLines[TALK_MAX_LINES];	/* 0x19bc */
	int numUseLines;		/* 0x1a5c */
	float useTime;			/* 0x1a60 the talk skips when the use is held this long */
	CPsaImage *tab;			/* 0x1a64 TabSprite */
	int tabSpriteIndex;		/* 0x1a68 */
	int tabBlend;			/* 0x1a6c */
	byte tabVisible;		/* 0x1a70 */
	byte pad_1a71[3];
	CFont *font;			/* 0x1a74 Horatio */
	float lineHeight;		/* 0x1a78 */
	int maxLines;			/* 0x1a7c 3, shown at once */
} cTalkScorer;

extern cGVar GVar_RemainTalkTime;
#define g_RemainTalkTime (GVar_RemainTalkTime.value.f)	/* RemainTalkTime */

cTalkScorer *cTalkScorer_ctor(cTalkScorer *self);

/* ---- TargetScorer.cpp ---- */

/* the target of the ship of the camera and the enemies that aim at it (0x148 bytes) */
typedef struct cTargetScorer {
	cScorerItem base;		/* 0x000 */
	float testTime;			/* 0x0ac the visibility tests run when it is <= 0 */
	float view;			/* 0x0b0 1 */
	char target[0x80];		/* 0x0b4 the target of the last frame */
	CPsaImage *img;			/* 0x134 Sprite */
	int targetRectIndex;		/* 0x138 */
	int targetArrowIndex;		/* 0x13c */
	int enemyFireIndex;		/* 0x140 */
	int enemyIndex;			/* 0x144 0x31 */
} cTargetScorer;

cTargetScorer *cTargetScorer_ctor(cTargetScorer *self);

/* ---- TextScorer.cpp ---- */

/* a text, edited with the virtual keyboard when its OnAccept is the native "edit" function (0xdc bytes) */
/* the vtable of cTextScorer and cTextTypingScorer */
struct cTextScorer;
typedef struct cTextScorerVtbl {
	cScorerItemVtbl base;
	void(*DrawKeyboard)(struct cTextScorer *self, int player);			/* 0x10 the virtual keyboard */
} cTextScorerVtbl;

/* calls through the slots of cTextScorerVtbl */
static inline void TextScorer_DrawKeyboard(void *self, int player) {
	VT(cTextScorerVtbl, self)->DrawKeyboard(self, player);
}

typedef struct cTextScorer {
	cAbstractTextScorer base;	/* 0x00 */
	CPsaImage *img;			/* 0xc4 white */
	float cellSize;			/* 0xc8 26, the keys of the keyboard */
	int maxInput;			/* 0xcc 16 */
	byte editing;			/* 0xd0 */
	byte centerEdit;		/* 0xd1 */
	byte pad_d2[2];
	char *editHint;			/* 0xd4 cString */
	byte isNumeric;			/* 0xd8 */
	byte pad_d9[3];
} cTextScorer;

/* a text typed in letter by letter (0xf4 bytes) */
typedef struct cTextTypingScorer {
	cTextScorer base;		/* 0x00 */
	float lastTime;			/* 0xdc the scorer time of the last draw */
	float typingTime;		/* 0xe0 */
	float typingSpeed;		/* 0xe4 0.04, the time of a letter */
	byte cursorAtInit;		/* 0xe8 1 */
	byte cursorAtEnd;		/* 0xe9 1 */
	byte pad_ea[2];
	int pos;			/* 0xec the letters shown, -1 */
	byte typingSound;		/* 0xf0 1 */
	byte pad_f1[3];
} cTextTypingScorer;


cTextScorer *cTextScorer_ctor(cTextScorer *self);
cTextTypingScorer *cTextTypingScorer_ctor(cTextTypingScorer *self);

/* ---- TimerScorer.cpp ---- */

/* the timer types of TimerType: TimeToCancel, TimeToFinish, TimeInactive, TimeSpecialUse */
#define TIMER_TYPES 4

/* a mission timer text "mm:ss" (0xf0 bytes); the times are engine variables shared by all */
typedef struct cTimerScorer {
	cAbstractTextScorer base;	/* 0x00 */
	int type;			/* 0xc4 0..3 */
	cPy_Function onTimeExpired;	/* 0xc8 */
	cPy_Function onTimeWarning;	/* 0xdc */
} cTimerScorer;

cTimerScorer *cTimerScorer_ctor(cTimerScorer *self);

/* ---- the menu drawing helpers (between TimerScorer.cpp and WaypointScorer.cpp) ---- */

/* ---- WaypointScorer.cpp ---- */

/* the arrow or the rectangle of the current waypoint (0xc4 bytes) */
typedef struct cWaypointScorer {
	cScorerItem base;		/* 0x00 */
	float testTime;			/* 0xac */
	byte visible;			/* 0xb0 1, the waypoint is in sight */
	byte pad_b1[3];
	int rectIndex;			/* 0xb4 WaypointRectIndex */
	int arrowIndex;			/* 0xb8 WaypointArrowIndex */
	int dotIndex;			/* 0xbc WaypointDotIndex 1 */
	CPsaImage *img;			/* 0xc0 Sprite */
} cWaypointScorer;

cWaypointScorer *cWaypointScorer_ctor(cWaypointScorer *self);

/* ---- WeaponScorer.cpp ---- */

#define WEAPONSCORER_WEAPONS 7
#define WEAPONSCORER_UPGRADES 13
#define WEAPONSCORER_AMMO 5

/* the weapon of the ship of the camera with its ammo, upgrades and the slots (0x1c4 bytes) */
typedef struct cWeaponScorer {
	cScorerItem base;		/* 0x000 */
	CPsaImage *img;			/* 0x0ac Sprite */
	CPsaImage *img3D;		/* 0x0b0 Weapons3DSprite */
	CFont *ammoFont;		/* 0x0b4 Horatio */
	uint32_t ammoTextColor;		/* 0x0b8 */
	int backIndex;			/* 0x0bc */
	float backOffX, backOffY;	/* 0x0c0 */
	float weaponIconOffX, weaponIconOffY;	/* 0x0c8 */
	float shadowOffX, shadowOffY;	/* 0x0d0 WeaponIconShadowOffX/Y */
	int weaponIcons[WEAPONSCORER_WEAPONS];	/* 0x0d8 */
	int weaponIconAc;		/* 0x0f4 the weapon of WeaponIconIndex */
	int upgradeIcons[WEAPONSCORER_UPGRADES];	/* 0x0f8 */
	int upgradeIconAc;		/* 0x12c */
	float upgradeBarOffX, upgradeBarOffY;	/* 0x130 */
	int upgradeBarIndex;		/* 0x138 */
	uint32_t upgradeBarColor;	/* 0x13c */
	float ammoBackOffX, ammoBackOffY;	/* 0x140 */
	int ammoBackIndex;		/* 0x148 */
	float ammoIconOffX, ammoIconOffY;	/* 0x14c */
	int ammoIcons[WEAPONSCORER_AMMO];	/* 0x154 */
	int ammoIconAc;			/* 0x168 */
	float ammoBarOffX, ammoBarOffY;	/* 0x16c */
	int ammoBarIndex;		/* 0x174 */
	float ammoTextOffX, ammoTextOffY;	/* 0x178 */
	uint32_t ammoBarColor;		/* 0x180 */
	float fontBackOffX, fontBackOffY;	/* 0x184 */
	int fontBackIndex;		/* 0x18c */
	float slotsOffX, slotsOffY;	/* 0x190 */
	int slotsIndex[2];		/* 0x198 */
	float changeTime;		/* 0x1a0 the fade of a weapon change */
	int lastWeapon;			/* 0x1a4 -100 */
	int lastUpgrade;		/* 0x1a8 -100 */
	float slotsTime;		/* 0x1ac */
	uint32_t backColor;		/* 0x1b0 0x5cffcc00 */
	uint32_t textBackColor;		/* 0x1b4 0xffffffff */
	CPsaImage *tab;			/* 0x1b8 TabSprite */
	int tabSpriteIndex;		/* 0x1bc */
	int tabBlend;			/* 0x1c0 */
} cWeaponScorer;

cWeaponScorer *cWeaponScorer_ctor(cWeaponScorer *self);

/* ---- WindFxScorer.cpp ---- */

/* the wind noise of the ship by the free space around it (0x100 bytes) */
typedef struct cWindFxScorer {
	cScorerItem base;		/* 0x00 */
	CPsaImage *img;			/* 0xac white, the debug bars */
	float wind[4];			/* 0xb0 the free space left, right, up, down 0..1 */
	cSound sound;			/* 0xc0 looped */
} cWindFxScorer;

cWindFxScorer *cWindFxScorer_ctor(cWindFxScorer *self);

#endif
