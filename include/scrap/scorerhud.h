/*
 * The HUD and menu items of Menu/ from SpriteScorer.cpp to WindFxScorer.cpp (the classes over cScorerItem of
 * include/scrap/scorer.h, cSpriteScorer and cAbstractTextScorer of include/scrap/scoreritems.h), the menu drawing
 * helpers and the native script callbacks of the menus between them. Every class has the slots Draw(player), Var
 * and Free.
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


/* a sprite filled by four colors at its corners */
typedef struct cGradientScorer {
	cSpriteScorer base;
	int colorSelected;		/* 0..3, the corner of Red, Green, Blue, Alpha */
	uint32_t colors[4];		/* 0xffffffff */
} cGradientScorer;

cGradientScorer *cGradientScorer_ctor(cGradientScorer *self);

/* ---- StatusScorer.cpp ---- */

/* the status of the ship of the camera: life, lives, money, boost and hull, the alarm and siren sounds (0x1fc) */
typedef struct cStatusScorer {
	cScorerItem base;
	cSound lowLife;			/* the warning under the low life (looped) */
	cSound alarm;			/* while the police alarm of the world is on (looped) */
	byte alarmOn;			/* the alarm of the world in the last frame */
	byte pad_12d[3];
	CPsaImage *img;			/* Sprite */
	CPsaImage *glow;		/* Sprite, blend 2 */
	float backOffX, backOffY;
	int backIndex;
	float lifeIconOffX, lifeIconOffY;
	int lifeIconIndex;
	float lifeTextOffX, lifeTextOffY;
	CFont *lifeFont;		/* Horatio */
	uint32_t lifeTextColor;
	int numLives;			/* Player.NumLives */
	float livesTextOffX, livesTextOffY;
	CFont *livesFont;		/* Horatio */
	uint32_t livesTextColor;
	float livesBackOffX, livesBackOffY;
	int livesBackIndex;
	float moneyIconOffX, moneyIconOffY;
	int moneyIconIndex;
	float moneyTextOffX, moneyTextOffY;
	CFont *moneyFont;		/* Horatio */
	uint32_t moneyTextColor;
	float moneyBackOffX, moneyBackOffY;
	int moneyBackIndex;
	int money;			/* of the world */
	int lastMoney;
	float moneyBlink;		/* 0.5 after a change, down to 0 */
	float hullIconOffX, hullIconOffY;
	int hullIconIndex;
	float fontBackOffX, fontBackOffY;
	int fontBackIndex;
	int boostStatus;		/* 0x40 used up, 0x80 ready, 0xff on */
	float boostRemaining;		/* 0..1 */
	float boostTime;		/* 1 */
	float boostOffX, boostOffY;
	int boostIconIndex;
	int boostTurboIndex;
	CPsaImage *flag;		/* SpriteFlag */
	float flagOffX, flagOffY;
	uint32_t backColor;		/* 0x5cffcc00 */
	uint32_t textBackColor;		/* 0xffffffff */
} cStatusScorer;

cStatusScorer *cStatusScorer_ctor(cStatusScorer *self);

/* ---- TabScorer.cpp ---- */

/* a frame of cells of a sprite sheet with a tab on top */
typedef struct cTabScorer {
	cScorerItem base;
	float sizeW, sizeH;		/* SizeX, SizeY to whole cells */
	float sizeX, sizeY;		/* SizeX, SizeY */
	int cellsX, cellsY;
	int cell;			/* the cell size: 14, 7 for HighRes */
	int highRes;
	int blend;			/* 3 */
	int tabSpriteIndex;		/* the first of the frames */
	int tabInit;			/* the first cell of the tab */
	int tabEnd;
	int tabMax;
	int sizeTabQuad;		/* the height of the tab in cells, 1 */
	int type;			/* 0..4, 4 a plain Menu_DrawFrame */
	int filled;			/* 1 */
	CPsaImage *img;			/* File */
} cTabScorer;

cTabScorer *cTabScorer_ctor(cTabScorer *self);

/* ---- TalkScorer.cpp ---- */

#define TALK_MAX_LINES 40
#define TALK_TEXT_SIZE 0x1900

/* the text of a talk (Scorer_SetSpeechText) above the character and the text of the use of the character in
 * front (0x1a80 bytes) */
struct cTalkScorer;
typedef struct cTalkScorer {
	cScorerItem base;
	CPsaImage *sprite;		/* Sprite */
	int useIndex;			/* UseIndex -1 */
	int nextBackIndex;		/* NextBackIndex -1 */
	int nextIndex;			/* NextIndex -1 */
	char useText[TALK_TEXT_SIZE];
	char *useLines[TALK_MAX_LINES];
	int numUseLines;
	float useTime;			/* the talk skips when the use is held this long */
	CPsaImage *tab;			/* TabSprite */
	int tabSpriteIndex;
	int tabBlend;
	byte tabVisible;
	byte pad_1a71[3];
	CFont *font;			/* Horatio */
	float lineHeight;
	int maxLines;			/* 3, shown at once */
} cTalkScorer;

extern cGVar GVar_RemainTalkTime;
#define g_RemainTalkTime (GVar_RemainTalkTime.value.f)	/* RemainTalkTime */

cTalkScorer *cTalkScorer_ctor(cTalkScorer *self);

/* ---- TargetScorer.cpp ---- */

/* the target of the ship of the camera and the enemies that aim at it */
typedef struct cTargetScorer {
	cScorerItem base;
	float testTime;			/* the visibility tests run when it is <= 0 */
	float view;			/* 1 */
	char target[0x80];		/* the target of the last frame */
	CPsaImage *img;			/* Sprite */
	int targetRectIndex;
	int targetArrowIndex;
	int enemyFireIndex;
	int enemyIndex;			/* 0x31 */
} cTargetScorer;

cTargetScorer *cTargetScorer_ctor(cTargetScorer *self);

/* ---- TextScorer.cpp ---- */

/* a text, edited with the virtual keyboard when its OnAccept is the native "edit" function */
/* the vtable of cTextScorer and cTextTypingScorer */
struct cTextScorer;
typedef struct cTextScorerVtbl {
	cScorerItemVtbl base;
	void(*DrawKeyboard)(struct cTextScorer *self, int player);			/* the virtual keyboard */
} cTextScorerVtbl;

/* calls through the slots of cTextScorerVtbl */
static inline void TextScorer_DrawKeyboard(void *self, int player) {
	VT(cTextScorerVtbl, self)->DrawKeyboard(self, player);
}

typedef struct cTextScorer {
	cAbstractTextScorer base;
	CPsaImage *img;			/* white */
	float cellSize;			/* 26, the keys of the keyboard */
	int maxInput;			/* 16 */
	byte editing;
	byte centerEdit;
	byte pad_d2[2];
	char *editHint;			/* cString */
	byte isNumeric;
	byte pad_d9[3];
} cTextScorer;

/* a text typed in letter by letter */
typedef struct cTextTypingScorer {
	cTextScorer base;
	float lastTime;			/* the scorer time of the last draw */
	float typingTime;
	float typingSpeed;		/* 0.04, the time of a letter */
	byte cursorAtInit;		/* 1 */
	byte cursorAtEnd;		/* 1 */
	byte pad_ea[2];
	int pos;			/* the letters shown, -1 */
	byte typingSound;		/* 1 */
	byte pad_f1[3];
} cTextTypingScorer;


cTextScorer *cTextScorer_ctor(cTextScorer *self);
cTextTypingScorer *cTextTypingScorer_ctor(cTextTypingScorer *self);

/* ---- TimerScorer.cpp ---- */

/* the timer types of TimerType: TimeToCancel, TimeToFinish, TimeInactive, TimeSpecialUse */
#define TIMER_TYPES 4

/* a mission timer text "mm:ss"; the times are engine variables shared by all */
typedef struct cTimerScorer {
	cAbstractTextScorer base;
	int type;			/* 0..3 */
	cPy_Function onTimeExpired;
	cPy_Function onTimeWarning;
} cTimerScorer;

cTimerScorer *cTimerScorer_ctor(cTimerScorer *self);

/* ---- the menu drawing helpers (between TimerScorer.cpp and WaypointScorer.cpp) ---- */

/* ---- WaypointScorer.cpp ---- */

/* the arrow or the rectangle of the current waypoint */
typedef struct cWaypointScorer {
	cScorerItem base;
	float testTime;
	byte visible;			/* 1, the waypoint is in sight */
	byte pad_b1[3];
	int rectIndex;			/* WaypointRectIndex */
	int arrowIndex;			/* WaypointArrowIndex */
	int dotIndex;			/* WaypointDotIndex 1 */
	CPsaImage *img;			/* Sprite */
} cWaypointScorer;

cWaypointScorer *cWaypointScorer_ctor(cWaypointScorer *self);

/* ---- WeaponScorer.cpp ---- */

#define WEAPONSCORER_WEAPONS 7
#define WEAPONSCORER_UPGRADES 13
#define WEAPONSCORER_AMMO 5

/* the weapon of the ship of the camera with its ammo, upgrades and the slots */
typedef struct cWeaponScorer {
	cScorerItem base;
	CPsaImage *img;			/* Sprite */
	CPsaImage *img3D;		/* Weapons3DSprite */
	CFont *ammoFont;		/* Horatio */
	uint32_t ammoTextColor;
	int backIndex;
	float backOffX, backOffY;
	float weaponIconOffX, weaponIconOffY;
	float shadowOffX, shadowOffY;	/* WeaponIconShadowOffX/Y */
	int weaponIcons[WEAPONSCORER_WEAPONS];
	int weaponIconAc;		/* the weapon of WeaponIconIndex */
	int upgradeIcons[WEAPONSCORER_UPGRADES];
	int upgradeIconAc;
	float upgradeBarOffX, upgradeBarOffY;
	int upgradeBarIndex;
	uint32_t upgradeBarColor;
	float ammoBackOffX, ammoBackOffY;
	int ammoBackIndex;
	float ammoIconOffX, ammoIconOffY;
	int ammoIcons[WEAPONSCORER_AMMO];
	int ammoIconAc;
	float ammoBarOffX, ammoBarOffY;
	int ammoBarIndex;
	float ammoTextOffX, ammoTextOffY;
	uint32_t ammoBarColor;
	float fontBackOffX, fontBackOffY;
	int fontBackIndex;
	float slotsOffX, slotsOffY;
	int slotsIndex[2];
	float changeTime;		/* the fade of a weapon change */
	int lastWeapon;			/* -100 */
	int lastUpgrade;		/* -100 */
	float slotsTime;
	uint32_t backColor;		/* 0x5cffcc00 */
	uint32_t textBackColor;		/* 0xffffffff */
	CPsaImage *tab;			/* TabSprite */
	int tabSpriteIndex;
	int tabBlend;
} cWeaponScorer;

cWeaponScorer *cWeaponScorer_ctor(cWeaponScorer *self);

/* ---- WindFxScorer.cpp ---- */

/* the wind noise of the ship by the free space around it */
typedef struct cWindFxScorer {
	cScorerItem base;
	CPsaImage *img;			/* white, the debug bars */
	float wind[4];			/* the free space left, right, up, down 0..1 */
	cSound sound;			/* looped */
} cWindFxScorer;

cWindFxScorer *cWindFxScorer_ctor(cWindFxScorer *self);

#endif
