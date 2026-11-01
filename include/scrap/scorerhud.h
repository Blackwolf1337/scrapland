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

/* the vtables of the exe */
#define CSPRITESCORER_VFTABLE GAME_VFTABLE(cSpriteScorer_vftable)
#define CGRADIENTSCORER_VFTABLE GAME_VFTABLE(cGradientScorer_vftable)
#define CSTATUSSCORER_VFTABLE GAME_VFTABLE(cStatusScorer_vftable)
#define CTABSCORER_VFTABLE GAME_VFTABLE(cTabScorer_vftable)
#define CTALKSCORER_VFTABLE GAME_VFTABLE(cTalkScorer_vftable)
#define CTARGETSCORER_VFTABLE GAME_VFTABLE(cTargetScorer_vftable)
#define CTEXTSCORER_VFTABLE GAME_VFTABLE(cTextScorer_vftable)
#define CTEXTTYPINGSCORER_VFTABLE GAME_VFTABLE(cTextTypingScorer_vftable)
#define CTIMERSCORER_VFTABLE GAME_VFTABLE(cTimerScorer_vftable)
#define CWAYPOINTSCORER_VFTABLE GAME_VFTABLE(cWaypointScorer_vftable)
#define CWEAPONSCORER_VFTABLE GAME_VFTABLE(cWeaponScorer_vftable)
#define CWINDFXSCORER_VFTABLE GAME_VFTABLE(cWindFxScorer_vftable)

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
		if (!cam->vftable->IsType(cam, "FCam")) {
			return 0;
		}
		*target = cCamEntity_GetTarget(cam);
		if (!*target || !(*target)->vftable->IsType(*target, type)) {
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
	return Game_Atan2(dx, dy);
}

/* the angle of an arrow frame pointing up from the middle of the screen towards the screen point sx, sy */
static inline float ScorerHud_ArrowAngle(float sx, float sy) {
	return -(float)(ScorerHud_ScreenAtan2(sx, sy) + 3.141592653589793);
}

/* ---- SpriteScorer.cpp ---- */

cSpriteScorer *cSpriteScorer_Delete(cSpriteScorer *self, byte flags);
/* the image of the file with flags (the 0x40000 flag when Discardable) */
void cSpriteScorer_SetFile(cSpriteScorer *self, uint32_t flags, const char *name);
void cSpriteScorer_Draw(cSpriteScorer *self, int player);
/* vf04 */
void cSpriteScorer_SetMultiSprite(cSpriteScorer *self, byte on);

/* a sprite filled by four colors at its corners (0xd8 bytes) */
typedef struct cGradientScorer {
	cSpriteScorer base;		/* 0x00 */
	int colorSelected;		/* 0xc4 0..3, the corner of Red, Green, Blue, Alpha */
	uint32_t colors[4];		/* 0xc8 0xffffffff */
} cGradientScorer;
SD_SIZE(cGradientScorer, 0xd8);

cGradientScorer *cGradientScorer_ctor(cGradientScorer *self);
cGradientScorer *cGradientScorer_Delete(cGradientScorer *self, byte flags);
/* ColorSelected, Red, Green, Blue, Alpha of the selected corner, then the sprite variables */
int cGradientScorer_Var(cGradientScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cGradientScorer_Draw(cGradientScorer *self, int player);

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
SD_SIZE(cStatusScorer, 0x1fc);
SD_OFFSET(cStatusScorer, img, 0x130);
SD_OFFSET(cStatusScorer, numLives, 0x160);
SD_OFFSET(cStatusScorer, moneyBlink, 0x1b0);
SD_OFFSET(cStatusScorer, flag, 0x1e8);

extern cGVar GVar_DangerLifeSound;
#define g_StatusLowLifeSound (GVar_DangerLifeSound.value.s)	/* the sound of the low life */
extern cGVar GVar_RegenerationSound;
#define g_StatusAlarmSound (GVar_RegenerationSound.value.s)	/* the siren of the alarm */

cStatusScorer *cStatusScorer_ctor(cStatusScorer *self);
void cStatusScorer_dtor(cStatusScorer *self);
cStatusScorer *cStatusScorer_Delete(cStatusScorer *self, byte flags);
/* BoostOnRender sets the OnRender function of the item */
int cStatusScorer_Var(cStatusScorer *self, const char *name, byte set, char **s, double *d, int *i);
/* vf00: the life, lives, money, hull and boost of the target of the camera, the low life and regeneration sounds */
void cStatusScorer_Draw(cStatusScorer *self, int player);

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
SD_SIZE(cTabScorer, 0xf0);

extern float g_TabBackTextWidth;	/* the width of the back text, once */

cTabScorer *cTabScorer_ctor(cTabScorer *self);
void cTabScorer_dtor(cTabScorer *self);
cTabScorer *cTabScorer_Delete(cTabScorer *self, byte flags);
void cTabScorer_SetFile(cTabScorer *self, uint32_t flags, const char *name);
int cTabScorer_Var(cTabScorer *self, const char *name, byte set, char **s, double *d, int *i);
/* the cells of the size, the tab over all of them when none is set */
void cTabScorer_UpdateSize(cTabScorer *self);
/* the frame stretched over the cells x0, y0 to x1, y1 */
void cTabScorer_DrawRect(cTabScorer *self, CPsaImage *img, int x0, int y0, int x1, int y1, int frame);
/* the frame in the cell x, y turned by rot quarters (0..7), flip mirrored */
void cTabScorer_DrawCell(cTabScorer *self, CPsaImage *img, int x, int y, int frame, int rot, int flip);
/* type 4 a plain Menu_DrawFrame, else the tab of tabSpriteIndex.. over the frame (types 2, 3: the bottom kinds) */
void cTabScorer_Draw(cTabScorer *self, int player);

/* ---- TalkScorer.cpp ---- */

#define TALK_MAX_LINES 40
#define TALK_TEXT_SIZE 0x1900

/* the text of a talk (Scorer_SetSpeechText) above the character and the text of the use of the character in
 * front (0x1a80 bytes) */
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
SD_SIZE(cTalkScorer, 0x1a80);
SD_OFFSET(cTalkScorer, useLines, 0x19bc);
SD_OFFSET(cTalkScorer, tab, 0x1a64);

/* the talk of Scorer_SetSpeechText */
extern char g_TalkText[6400];		/* TALK_TEXT_SIZE */
extern cGVar GVar_MaxTalkLineSizePix;
#define g_TalkWidth (GVar_MaxTalkLineSizePix.value.f)		/* the width of the lines */
extern cGVar GVar_TimeToAbortChatText;
#define g_TalkUseTime (GVar_TimeToAbortChatText.value.f)	/* the hold time of the use that skips a talk */
extern cGVar GVar_RemainTalkTime;
#define g_RemainTalkTime (GVar_RemainTalkTime.value.f)	/* RemainTalkTime */
extern cGVar GVar_FullTalkTime;
#define g_TalkTime (GVar_FullTalkTime.value.f)		/* the time of the whole talk */
#define g_TalkCallback (&GAME_GLOBAL(cPy_Function, g_TalkCallback))	/* run at the end of the talk */
extern int g_TalkNumLines;
extern char * g_TalkLines[40];	/* TALK_MAX_LINES */
extern int g_TalkColor[3];		/* r, g, b */

cTalkScorer *cTalkScorer_ctor(cTalkScorer *self);
void cTalkScorer_dtor(cTalkScorer *self);
cTalkScorer *cTalkScorer_Delete(cTalkScorer *self, byte flags);
/* the character of the player in front of its character (the use target), NULL if none */
cEntity *Talk_GetUseTarget(int player);
int cTalkScorer_Var(cTalkScorer *self, const char *name, byte set, char **s, double *d, int *i);
/* the use text (vf05) and the talk (vf04) up from the item position */
void cTalkScorer_Draw(cTalkScorer *self, int player);
void cTalkScorer_DrawTalk(cTalkScorer *self, int player, float *pos);
void cTalkScorer_DrawUse(cTalkScorer *self, int player, float *pos);
/* the items of a Python list as the parameters of the talk callback */
void Talk_AddCallbackParams(PyObject *list);

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
SD_SIZE(cTargetScorer, 0x148);

cTargetScorer *cTargetScorer_ctor(cTargetScorer *self);
void cTargetScorer_dtor(cTargetScorer *self);
cTargetScorer *cTargetScorer_Delete(cTargetScorer *self, byte flags);
/* the mark of the target of the ship and the arrow to its last attacker; the marks of the enemies and friends
 * around it */
void cTargetScorer_DrawTarget(cTargetScorer *self, cEntity *ship);
void cTargetScorer_DrawEnemies(cTargetScorer *self, cEntity *ship);
int cTargetScorer_Var(cTargetScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cTargetScorer_Draw(cTargetScorer *self, int player);

/* ---- TextScorer.cpp ---- */

/* a text, edited with the virtual keyboard when its OnAccept is the native "edit" function (0xdc bytes) */
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
SD_SIZE(cTextScorer, 0xdc);

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
SD_SIZE(cTextTypingScorer, 0xf4);

/* the text edit of each player */
extern cTextScorer * g_TextEditItem[4];	/* [4] */
extern cScorerItem * g_TextEditCursor[4];	/* [4] the cursor item before the edit */
extern cGVar GVar_TextTypingSound;
#define g_TypingSoundName (GVar_TextTypingSound.value.s)	/* "keyboard%d" for the key sounds */
extern cGVar GVar_TextTypingSoundVol;
#define g_TypingSoundVol (GVar_TextTypingSoundVol.value.f)
#define TEXTEDIT_NATIVE_FUNC 0x184120			/* the OnAccept that starts the edit */

cTextScorer *cTextScorer_ctor(cTextScorer *self);
cTextTypingScorer *cTextTypingScorer_ctor(cTextTypingScorer *self);
void cTextScorer_dtor(cTextScorer *self);
cTextScorer *cTextScorer_Delete(cTextScorer *self, byte flags);
cTextTypingScorer *cTextTypingScorer_Delete(cTextTypingScorer *self, byte flags);
/* EditHint, CenterEdit, IsNumeric, MaxInput (the size of that many "W"; 0 above 255), then the text variables */
int cTextScorer_Var(cTextScorer *self, const char *name, byte set, char **s, double *d, int *i);
/* TypingSpeed, TypingTime (return 3), CursorAtInit, CursorAtEnd, TypingSound, then cTextScorer_Var */
int cTextTypingScorer_Var(cTextTypingScorer *self, const char *name, byte set, char **s, double *d,
	int *i);
void cTextScorer_Draw(cTextScorer *self, int player);
/* vf04: the text being typed and the virtual keyboard */
void cTextScorer_DrawKeyboard(cTextScorer *self, int player);
void cTextTypingScorer_Draw(cTextTypingScorer *self, int player);
/* the edit of the text item of player ends: the cursor item back, the hint of the item */
void TextEdit_End(int player);
/* onDone of the player input: the typed text into the item, then TextEdit_End */
void TextEdit_Done(int player, const char *text);
/* the native OnAccept of an edited text (Input/ActionSet.h): params player, item name; the virtual keyboard
 * starts on the text of the item */
void TextEdit_Start(cAbstractFile *params);

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
SD_SIZE(cTimerScorer, 0xf0);

/* the time engine variables of the types, by type (their flags at -4, GVAR_FLAGS) */
extern cGVar GVar_TimeToCancelCurrentMission;
#define g_TimeToCancel (GVar_TimeToCancelCurrentMission.value.f)
extern cGVar GVar_TimeToFinishCurrentMission;
#define g_TimeToFinish (GVar_TimeToFinishCurrentMission.value.f)
extern cGVar GVar_TimeInactiveTimer;
#define g_TimeInactive (GVar_TimeInactiveTimer.value.f)
extern cGVar GVar_TimeSpecialUseTimer;
#define g_TimeSpecialUse (GVar_TimeSpecialUseTimer.value.f)
extern cGVar GVar_CounterType;
#define g_TimerMode (GVar_CounterType.value.i)		/* 0 counts down, 1 up, else stopped */
extern cGVar GVar_TimeWarning;
#define g_TimerWarning (GVar_TimeWarning.value.f)	/* OnTimeWarning below this, -1 after */

cTimerScorer *cTimerScorer_ctor(cTimerScorer *self);
void cTimerScorer_dtor(cTimerScorer *self);
cTimerScorer *cTimerScorer_Delete(cTimerScorer *self, byte flags);
/* set only: OnTimeExpired, OnTimeWarning, TimerType; then the text variables */
int cTimerScorer_Var(cTimerScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cTimerScorer_Draw(cTimerScorer *self, int player);

/* ---- the menu drawing helpers (between TimerScorer.cpp and WaypointScorer.cpp) ---- */

/* Menu_DrawStretch (scoreritems.h), Menu_DrawBar and Menu_DrawFrame (app.h); the native OnRender functions of the
 * menu items (params: the player, the item name) */
void Scorer_MetroUsrHere(cAbstractFile *params);
void Scorer_SpriteFadeIn(cAbstractFile *params);
void Scorer_SpriteFadeOut(cAbstractFile *params);
void Scorer_SpriteFadeOutFinal(cAbstractFile *params);
void Scorer_SpriteFlash(cAbstractFile *params);
void Scorer_SpriteGlow(cAbstractFile *params);
void Scorer_BoostScorer(cAbstractFile *params);

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
SD_SIZE(cWaypointScorer, 0xc4);

cWaypointScorer *cWaypointScorer_ctor(cWaypointScorer *self);
void cWaypointScorer_dtor(cWaypointScorer *self);
cWaypointScorer *cWaypointScorer_Delete(cWaypointScorer *self, byte flags);
int cWaypointScorer_Var(cWaypointScorer *self, const char *name, byte set, char **s, double *d, int *i);
/* the mark of the waypoint object: the rectangle around it in sight (named: the current one), the blinking dot on
 * another one in sight, else the arrow towards it */
void cWaypointScorer_DrawTarget(cWaypointScorer *self, cEntity *e, byte current);
/* the current waypoint (CurrentWaypointName, CurrentWaypointName2 for player 1), else the nearest one */
void cWaypointScorer_Draw(cWaypointScorer *self, int player);

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
SD_SIZE(cWeaponScorer, 0x1c4);
SD_OFFSET(cWeaponScorer, upgradeIconAc, 0x12c);
SD_OFFSET(cWeaponScorer, ammoIconAc, 0x168);
SD_OFFSET(cWeaponScorer, fontBackIndex, 0x18c);

cWeaponScorer *cWeaponScorer_ctor(cWeaponScorer *self);
void cWeaponScorer_dtor(cWeaponScorer *self);
cWeaponScorer *cWeaponScorer_Delete(cWeaponScorer *self, byte flags);
int cWeaponScorer_Var(cWeaponScorer *self, const char *name, byte set, char **s, double *d, int *i);
/* vf00: the weapon of the ship of the camera, its ammo and slots, the row of the weapons after a change */
void cWeaponScorer_Draw(cWeaponScorer *self, int player);

/* ---- WindFxScorer.cpp ---- */

/* the wind noise of the ship by the free space around it (0x100 bytes) */
typedef struct cWindFxScorer {
	cScorerItem base;		/* 0x00 */
	CPsaImage *img;			/* 0xac white, the debug bars */
	float wind[4];			/* 0xb0 the free space left, right, up, down 0..1 */
	cSound sound;			/* 0xc0 looped */
} cWindFxScorer;
SD_SIZE(cWindFxScorer, 0x100);

extern cGVar GVar_WindFxScorerSound;
#define g_WindFxSound (GVar_WindFxScorerSound.value.s)

cWindFxScorer *cWindFxScorer_ctor(cWindFxScorer *self);
void cWindFxScorer_dtor(cWindFxScorer *self);
cWindFxScorer *cWindFxScorer_Delete(cWindFxScorer *self, byte flags);
/* vf00: the volume, pan and pitch of the wind by the free space around the ship of the camera; the debug bars with
 * WindFxScorerShow */
void cWindFxScorer_Draw(cWindFxScorer *self, int player);

#endif
