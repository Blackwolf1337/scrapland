/*
 * The HUD and menu items of Menu/ from AbstractTextScorer.cpp to RadarScorer.cpp (the classes over cScorerItem of
 * include/scrap/scorer.h). Every class has Draw(player) at vf00, Var at vf02 and the deleting destructor at vf03.
 */
#ifndef SCRAP_SCORERITEMS_H
#define SCRAP_SCORERITEMS_H

#include "scrap/gvar.h"
#include "scrap/app.h"
#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/input.h"
#include "scrap/scorer.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapmodel.h"
#include "scrap/sound.h"

/* the abs of the originals (cdq, xor, sub): INT_MIN stays */
static inline int Scorer_Abs(int v) {
	uint32_t s = (uint32_t)(v >> 31);

	return (int)(((uint32_t)v ^ s) - s);
}

/* the variables of the scripts: a float (2) and the color bytes of an 0xAARRGGBB color */
static inline int ScorerVar_Float(float *v, byte set, double *d) {
	if (set) {
		*v = (float)*d;
	} else {
		*d = (double)*v;
	}
	return 2;
}

static inline int ScorerVar_Int(int *v, byte set, int *i) {
	if (set) {
		*v = *i;
	} else {
		*i = *v;
	}
	return 3;
}

/* shift 16 red, 8 green, 0 blue, 24 alpha */
static inline int ScorerVar_ColorByte(uint32_t *c, int shift, byte set, int *i) {
	if (set) {
		*c = (*c & ~(0xffu << shift)) | ((uint32_t)*i & 0xff) << shift;
	} else {
		*i = (*c >> shift) & 0xff;
	}
	return 3;
}

/* the color variables names[0..3] (red, green, blue, alpha) of c, tried in this order; 0 if none matches */
static inline int ScorerVar_Color(const char *name, const char *const names[4], uint32_t *c, byte set, int *i) {
	static const int shifts[4] = {16, 8, 0, 24};

	for (int k = 0; k < 4; k++) {
		if (!crt_stricmp(name, names[k])) {
			return ScorerVar_ColorByte(c, shifts[k], set, i);
		}
	}
	return 0;
}

/* ---- the base of the sprite items (SpriteScorer.cpp, not in this module), 0xc4 bytes ---- */
/* the vtable of cSpriteScorer and the items made from it (bar, button, hangar ship, mission, gradient) */
struct cSpriteScorer;
typedef struct cSpriteScorerVtbl {
	cScorerItemVtbl base;
	void(*SetMultiSprite)(struct cSpriteScorer *self, byte on);			/* 0x10 */
} cSpriteScorerVtbl;

typedef struct cSpriteScorer {
	cScorerItem base;		/* 0x00 */
	float jumpFX;			/* 0xac */
	byte numSubSprites;		/* 0xb0 1 */
	byte spriteIndex;		/* 0xb1 1, the frame drawn */
	byte isMultiSprite;		/* 0xb2 */
	byte highRes;			/* 0xb3 */
	byte fixPosition;		/* 0xb4 */
	byte blendingMode;		/* 0xb5 3 */
	byte rotate;			/* 0xb6 */
	byte mirror;			/* 0xb7 */
	byte discardable;		/* 0xb8 */
	byte pad_b9[3];
	CPsaImage *img;			/* 0xbc */
	char *file;			/* 0xc0 cString */
} cSpriteScorer;
void cSpriteScorer_SetMultiSprite(cSpriteScorer *self, byte on);

cSpriteScorer *cSpriteScorer_ctor(cSpriteScorer *self);
void cSpriteScorer_dtor(cSpriteScorer *self);
int cSpriteScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i);

/* ---- AbstractTextScorer.cpp ---- */

/* cAbstractTextScorer.align */
#define TEXTSCORER_LEFT 0
#define TEXTSCORER_RIGHT 1
#define TEXTSCORER_CENTER 4
/* cAbstractTextScorer.effect (the CFont effects) */
#define TEXTSCORER_OSCILE 0x10000
#define TEXTSCORER_SHADOW 0x20000
#define TEXTSCORER_OUTLINE 0x40000

/* a text in a font: the base of the text items, 0xc4 bytes */
typedef struct cAbstractTextScorer {
	cScorerItem base;		/* 0x00 */
	uint32_t color;			/* 0xac 0xAARRGGBB */
	int align;			/* 0xb0 TEXTSCORER_LEFT, _RIGHT, _CENTER */
	int effect;			/* 0xb4 TEXTSCORER_OSCILE, _SHADOW, _OUTLINE or 0 */
	CFont *font;			/* 0xb8 */
	char *fontName;			/* 0xbc cString */
	char *text;			/* 0xc0 cString */
} cAbstractTextScorer;

/* the credits: 20 lines in two fonts, faded in a render target (0x32dc bytes) */
#define CREDITS_LINES 20
#define CREDITS_LINE_SIZE 0x280
typedef struct cCreditsScorer {
	cAbstractTextScorer base;	/* 0x00 */
	CFont *font;			/* 0xc4 ScrapMedium, the lines after the first */
	CFont *titleFont;		/* 0xc8 Horatio, the first line */
	float time;			/* 0xcc */
	float life;			/* 0xd0 */
	float fadeIn;			/* 0xd4 */
	float fadeOut;			/* 0xd8 */
	/* Text0..Text19; a text in line 8 puts the lines in three columns, without the fades */
	char lines[CREDITS_LINES][CREDITS_LINE_SIZE];	/* 0xdc */
} cCreditsScorer;

/* the hint of the cursor item and the label text (the same layout), with a frame behind (0xd0 bytes) */
typedef struct cHintScorer {
	cAbstractTextScorer base;	/* 0x00 */
	CPsaImage *tab;			/* 0xc4 the frame sprite sheet */
	int tabIndex;			/* 0xc8 the first frame of Menu_DrawFrame, none if <= 0 */
	int tabBlend;			/* 0xcc 0..6 */
} cHintScorer;
typedef cHintScorer cLabelScorer;

/* the message text of Scorer_SetMsgText (0xc8 bytes) */
typedef struct cMsgScorer {
	cAbstractTextScorer base;	/* 0x00 */
	float lineHeight;		/* 0xc4 the height of "W" */
} cMsgScorer;

/* the engine variables of the credits render target */
extern cGVar GVar_R_CreditsX;
#define g_CreditsX (GVar_R_CreditsX.value.i)		/* R_CreditsX */
extern cGVar GVar_R_CreditsSizeX;
#define g_CreditsSizeX (GVar_R_CreditsSizeX.value.i)		/* R_CreditsSizeX */
extern cGVar GVar_R_CreditsY;
#define g_CreditsY (GVar_R_CreditsY.value.i)		/* R_CreditsY */
extern cGVar GVar_R_CreditsAlpha;
#define g_CreditsAlpha (GVar_R_CreditsAlpha.value.i)		/* R_CreditsAlpha */
extern cGVar GVar_R_CreditsClear;
#define g_CreditsClear (GVar_R_CreditsClear.value.i)		/* R_CreditsClear */
extern cGVar GVar_R_CreditsPhase;
#define g_CreditsPhase (GVar_R_CreditsPhase.value.i)		/* R_CreditsPhase */
extern float g_MissionSolidTime;	/* cMissionScorer SolidTime */
#define MSG_LINES 20	/* the slots up to g_LabelText at 0x45d190 */
extern cGVar GVar_TimeToMsg;
#define g_MsgTime (GVar_TimeToMsg.value.f)

cAbstractTextScorer *cAbstractTextScorer_ctor(cAbstractTextScorer *self);
cCreditsScorer *cCreditsScorer_ctor(cCreditsScorer *self);
cHintScorer *cHintScorer_ctor(cHintScorer *self);
cLabelScorer *cLabelScorer_ctor(cLabelScorer *self);
cMsgScorer *cMsgScorer_ctor(cMsgScorer *self);
void cAbstractTextScorer_dtor(cAbstractTextScorer *self);
void cScorerItem_SetParam(cScorerItem *self, const char *name, const char *value);
/* Text, Red, Green, Blue, Alpha; set only: Font, CentralText, GetExtens (1: the size of the text); Align, Effect */
int cAbstractTextScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i);

/* ---- BarScorer.cpp ---- */

/* a bar of a sprite sheet: main and black frames over a back frame (0x108 bytes). The value and the auto update
 * are engine variables shared by all bars. */
typedef struct cBarScorer {
	cSpriteScorer base;		/* 0x00 */
	float maxValue;			/* 0xc4 */
	float blinkTime;		/* 0xc8 */
	byte blink;			/* 0xcc */
	byte blinkOff;			/* 0xcd the bar is hidden in this half of the blink */
	byte pad_ce[2];
	int mainIndex;			/* 0xd0 the frame of the bar */
	int blackIndex;			/* 0xd4 the frame of the empty part */
	float mainUV0[2], mainUV1[2];	/* 0xd8 */
	float blackUV0[2], blackUV1[2];	/* 0xe8 */
	int backIndex;			/* 0xf8 the frame behind, none if 0 */
	uint32_t backColor;		/* 0xfc */
	uint32_t mainColor;		/* 0x100 */
	uint32_t blackColor;		/* 0x104 */
} cBarScorer;

/* IsAutoUpdatable: 1 down to AutoUpdateStopValue, 2 up to the maximum, by the frame time and the speed */
extern cGVar GVar_BarValue;
#define g_BarValue (GVar_BarValue.value.f)		/* BarValue */
extern cGVar GVar_AutoUpdateStopValue;
#define g_BarStopValue (GVar_AutoUpdateStopValue.value.f)	/* AutoUpdateStopValue */
extern cGVar GVar_AutoUpdateSpeedFactor;
#define g_BarSpeedFactor (GVar_AutoUpdateSpeedFactor.value.f)	/* AutoUpdateSpeedFactor */
extern cGVar GVar_IsAutoUpdatable;
#define g_BarAutoUpdate (GVar_IsAutoUpdatable.value.i)	/* IsAutoUpdatable */

cBarScorer *cBarScorer_ctor(cBarScorer *self);

/* ---- ButtonScorer.cpp ---- */


typedef TABLE(char *) cStrTable;	/* Global/Table.h of cString */
typedef TABLE(byte) cByteTable;

/* cButtonScorer.textAlign */
#define BUTTON_LEFT 0
#define BUTTON_CENTER 1
#define BUTTON_RIGHT 2

/* a menu button: a box of 10 pixel cells of the sprite sheet (Complex) or one frame, a text of lines (name:value
 * pairs in two columns), the focus highlight (0x168 bytes) */
typedef struct cButtonScorer {
	cSpriteScorer base;		/* 0x00 */
	cStrTable lines;		/* 0xc4 */
	cByteTable isValue;		/* 0xcc 1 for the value of a name:value line */
	CFont *font;			/* 0xd4 Horatio */
	float xAlignLeft;		/* 0xd8 XAlignLeft: x */
	float xAlignRight;		/* 0xdc x + w */
	float xAlignCenter;		/* 0xe0 x + w / 2 */
	int highlight;			/* 0xe4 */
	int glow;			/* 0xe8 */
	int focusSpriteIndex;		/* 0xec 1 */
	int borderSpriteIndex;		/* 0xf0 -1 for none */
	int width;			/* 0xf4 the cells inside the box, 1 */
	int height;			/* 0xf8 */
	int maxLineSize;		/* 0xfc -1: no word wrap */
	int hasText;			/* 0x100 */
	int textAlign;			/* 0x104 BUTTON_CENTER */
	int marker;			/* 0x108 */
	int isNameValuePair;		/* 0x10c 1 */
	int renderAlphaChannel;		/* 0x110 0, 1 or 2 */
	int boxH;			/* 0x114 the box height in pixels */
	int textH;			/* 0x118 the text height in pixels */
	float markerOffset;		/* 0x11c */
	float highlightOffset;		/* 0x120 */
	float lineHeight;		/* 0x124 */
	float scaleSmoothWidth;		/* 0x128 the scale of the focused sprite */
	float scaleSmoothHeight;	/* 0x12c */
	float scaleSmoothTime;		/* 0x130 */
	uint32_t textColor;		/* 0x134 */
	uint32_t borderColor;		/* 0x138 */
	float origScaleX, origScaleY;	/* 0x13c the scale of the sprite */
	float textWidth;		/* 0x144 the widest line, -1 */
	float textHeight;		/* 0x148 its height */
	int complex;			/* 0x14c 1 */
	uint32_t highLightColor;	/* 0x150 */
	float minX, maxX;		/* 0x154 the columns of name:value lines, -1 */
	CPsaImage *alphaImg;		/* 0x15c white, behind the sprite (RenderAlphaChannel) */
	CPsaImage *focusImg;		/* 0x160 the highlight and border frames (File) */
	int highLightBlend;		/* 0x164 3 */
} cButtonScorer;

cButtonScorer *cButtonScorer_ctor(cButtonScorer *self);

/* ---- CannonScorer.cpp ---- */

#define CANNON_WEAPONS 7

/* the crosshair of the ship of the camera: the center sprite, the aim of the weapon at the aim point of the auto aim,
 * the lock text and the life warning (0xec bytes) */
struct cCannonScorer;
typedef struct cCannonScorer {
	cScorerItem base;		/* 0x00 */
	byte shown;			/* 0xac the aim was drawn in the last frame (the no ammo sound when it goes) */
	byte pad_ad[3];
	int numWeapons;			/* 0xb0 of the world at the creation */
	float angle;			/* 0xb4 of the aim */
	float size[2];			/* 0xb8 20, 20 */
	uint32_t color;			/* 0xc0 the aim of the weapon in use */
	CPsaImage *img;			/* 0xc4 */
	int crossHairIndex;		/* 0xc8 the center frame, none if 0 */
	int aimIndex[CANNON_WEAPONS];	/* 0xcc the aim frame per weapon, none if 0 */
	int aimAc;			/* 0xe8 the weapon of AimIndex */
} cCannonScorer;

extern cGVar GVar_WarningLifeLevel;
#define g_WarningLifeLevel (GVar_WarningLifeLevel.value.i)	/* WarningLifeLevel, percent */

cCannonScorer *cCannonScorer_ctor(cCannonScorer *self);

/* ---- ChatScorer.cpp ---- */

#define CHAT_MSGS 16

/* the chat lines of Scorer_AddChatMsg (0xbc bytes) */
typedef struct cChatScorer {
	cScorerItem base;		/* 0x00 */
	CPsaImage *img;			/* 0xac white, not drawn */
	int numMsg;			/* 0xb0 the newest lines shown, 0..16 */
	CFont *font;			/* 0xb4 HoratioSmall */
	float lineHeight;		/* 0xb8 14 */
} cChatScorer;

/* a chat line, the oldest first */
typedef struct cChatMsg {
	char text[0x50];		/* 0x00 */
	uint32_t color;			/* 0x50 */
} cChatMsg;

cChatScorer *cChatScorer_ctor(cChatScorer *self);

/* ---- CircuitScorer.cpp ---- */

#define CIRCUIT_PATHS 9
#define CIRCUIT_FLARES 4
#define CIRCUIT_TRAIL 24

/* the circuit board picture with flares running along its paths (0xc8 bytes) */
typedef struct cCircuitScorer {
	cScorerItem base;		/* 0x00 */
	float lastTime;			/* 0xac the game time of the last frame */
	float curTime;			/* 0xb0 */
	char *file;			/* 0xb4 cString, the last FileCircuit or FileFlare */
	int highRes;			/* 0xb8 */
	int blend;			/* 0xbc 3 */
	CPsaImage *circuit;		/* 0xc0 */
	CPsaImage *flare;		/* 0xc4 frames 1 (head) and 2 (trail) */
} cCircuitScorer;

/* a path of the board in pixels of the picture, with the lengths of its segments */
typedef struct cCircuitPath {
	int numPoints;			/* 0x000 */
	float pts[32][2];		/* 0x004 */
	float segStart[32];		/* 0x104 the length before each segment */
	float segLen[32];		/* 0x184 */
	float normStart[32];		/* 0x204 segStart / total */
	float normEnd[32];		/* 0x284 (segStart + segLen) / total */
	float total;			/* 0x304 */
} cCircuitPath;

/* a flare on a path, then a pause */
typedef struct cCircuitFlare {
	float time;			/* 0x00 left of the run */
	float duration;			/* 0x04 */
	float delay;			/* 0x08 the pause before the next run */
	int path;			/* 0x0c */
	int reverse;			/* 0x10 */
	int unused_14;			/* 0x14 */
} cCircuitFlare;

cCircuitScorer *cCircuitScorer_ctor(cCircuitScorer *self);

/* ---- CopMeterScorer.cpp ---- */

/* the police alarm meter: a back frame, a bar to the alarm level of the world, two blinking lights while the
 * alarm is on; fades in with the alarm (0xf0 bytes) */
typedef struct cCopMeterScorer {
	cScorerItem base;		/* 0x00 */
	CPsaImage *img;			/* 0xac */
	CPsaImage *flash;		/* 0xb0 the same sheet for the lights */
	float backOffX, backOffY;	/* 0xb4 */
	int backIndex;			/* 0xbc */
	float lightOffX, lightOffY;	/* 0xc0 */
	int lightIndex;			/* 0xc8 the light frames +0..+2 */
	float barOffX, barOffY;		/* 0xcc */
	int barIndex;			/* 0xd4 */
	float percent;			/* 0xd8 the bar 0..1, follows target */
	float target;			/* 0xdc the alarm level of the world */
	byte on;			/* 0xe0 the alarm is on */
	byte pad_e1[3];
	int unused_e4;			/* 0xe4 */
	float fade;			/* 0xe8 0..1 */
	float blink;			/* 0xec 0..0.9 the blink phase */
} cCopMeterScorer;

cCopMeterScorer *cCopMeterScorer_ctor(cCopMeterScorer *self);

/* ---- HangarShipScorer.cpp ---- */

/* a ship of the world turning in a view of its own over a grid frame (0xf0 bytes) */
typedef struct cHangarShipScorer {
	cSpriteScorer base;		/* 0x00 */
	CGameCamera *cam;		/* 0xc4 */
	CPsaImage *grid;		/* 0xc8 the sprite sheet of the item */
	char *targetName;		/* 0xcc cString, the ship */
	byte reset;			/* 0xd0 the angles start at 0 in the next frame */
	byte pad_d1[3];
	vec3_t rot;			/* 0xd4 the angles of the ship in the view */
	int gridIndex;			/* 0xe0 */
	uint32_t gridColor;		/* 0xe4 */
	float camDist;			/* 0xe8 100 */
	float camFov;			/* 0xec 20 degrees */
} cHangarShipScorer;

cHangarShipScorer *cHangarShipScorer_ctor(cHangarShipScorer *self);

/* ---- the models of the scorer (Scorer.cpp, SScorer.AddModel) ---- */

typedef struct cScorerModel {
	char name[0x20];		/* 0x00 */
	CGameModel model;		/* 0x20 */
	int numAnims;			/* 0x3c */
	int anims[8];			/* 0x40 */
	struct cScorerModel *next;	/* 0x60 */
	float radius;			/* 0x64 */
	float height;			/* 0x68 the eyes */
	struct cEmbeddedFX *extra;	/* 0x6c its Tick draws after the model */
} cScorerModel;

/* the model of that name of the scorer, NULL if none */
cScorerModel *cScorer_FindModel(cScorer *scorer, const char *name);

/* ---- HeadScorer.cpp ---- */

/* a talking head: a scorer model in a view of its own, the camera swaying (0xc4 bytes) */
/* the vtable of cHeadScorer, also the one of cMonitorScorer (the same two slots, its own functions) */
typedef struct cHeadScorerVtbl {
	cScorerItemVtbl base;
	void(*SetAnim)(cScorerItem *self, int anim);					/* 0x10 */
	byte(*SetModel)(cScorerItem *self, const char *name);			/* 0x14 */
} cHeadScorerVtbl;

/* calls through the slots of cHeadScorerVtbl */
static inline byte HeadScorer_SetModel(void *self, const char *name) {
	return VT(cHeadScorerVtbl, self)->SetModel(self, name);
}

static inline void HeadScorer_SetAnim(void *self, int anim) {
	VT(cHeadScorerVtbl, self)->SetAnim(self, anim);
}

typedef struct cHeadScorer {
	cScorerItem base;		/* 0x00 */
	float time;			/* 0xac the sway, random start */
	cScorerModel *model;		/* 0xb0 */
	int anim;			/* 0xb4 1 */
	CGameCamera *cam;		/* 0xb8 */
	float vw, vh;			/* 0xbc the view size */
} cHeadScorer;

cHeadScorer *cHeadScorer_ctor(cHeadScorer *self);

/* ---- MissionScorer.cpp ---- */

#define MISSION_STATUS 3

/* the mission icons (three status lamps) and the text of the next mission's map (0xec bytes); the status and
 * the blink are engine variables shared by all */
typedef struct cMissionScorer {
	cSpriteScorer base;		/* 0x00 */
	const char *nextText;		/* 0xc4 the language text NextMissionMapText */
	CFont *font;			/* 0xc8 Horatio */
	char *forceMap;			/* 0xcc cString, the map instead of the save variable Mission.Map */
	int backIndex;			/* 0xd0 */
	int primaryIndex;		/* 0xd4 the frame of status 0 */
	int secondaryIndex;		/* 0xd8 of status 1 and 2 */
	uint32_t textColor;		/* 0xdc */
	CPsaImage *tab;			/* 0xe0 */
	int tabIndex;			/* 0xe4 */
	int tabBlend;			/* 0xe8 0..6 */
} cMissionScorer;

cMissionScorer *cMissionScorer_ctor(cMissionScorer *self);

/* ---- MonitorScorer.cpp ---- */

#define MONITOR_WEAPONS 7
#define MONITOR_UPGRADES 13

/* the ship monitor: the panel, the head of the talk (Scorer_SetHeadMonitor) with its text, the weapon and upgrade
 * icons, the objective, the static noise; the view of the target (0x354 bytes) */
typedef struct cMonitorScorer {
	cScorerItem base;		/* 0x000 */
	float textTime;			/* 0x0ac */
	float noise;			/* 0x0b0 4 */
	CGameCamera *cam;		/* 0x0b4 */
	cEntity *lastTarget;		/* 0x0b8 the entity seen last, its descriptor is the text */
	char text[0x140];		/* 0x0bc */
	float textShown;		/* 0x1fc the time the text shows, 20 letters a second */
	CFont *smallFont;		/* 0x200 Terminal8 */
	CFont *bigFont;			/* 0x204 Horatio */
	cScorerModel *model;		/* 0x208 the head */
	int anim;			/* 0x20c 1 */
	byte targetView;		/* 0x210 */
	byte pad_211[3];
	float objFrom[2];		/* 0x214 the objective mark moves from there */
	float objPos[2];		/* 0x21c to the screen position of the entity to photograph */
	char objName[0x80];		/* 0x224 the name of that entity */
	float objBlend;			/* 0x2a4 1 */
	byte objWas;			/* 0x2a8 the mark was on an entity before */
	byte pad_2a9[3];
	CPsaImage *weapon;		/* 0x2ac the weapon icons */
	float weaponOffX, weaponOffY;	/* 0x2b0 */
	int weaponIcons[MONITOR_WEAPONS];	/* 0x2b8 */
	int weaponIconAc;		/* 0x2d4 the weapon of WeaponIconIndex */
	int upgradeIcons[MONITOR_UPGRADES];	/* 0x2d8 */
	int upgradeIconAc;		/* 0x30c */
	CPsaImage *panel;		/* 0x310 */
	int panelIndex;			/* 0x314 */
	CPsaImage *mask;		/* 0x318 */
	int maskIndex;			/* 0x31c */
	CPsaImage *frame;		/* 0x320 */
	int frameIndex;			/* 0x324 */
	CPsaImage *staticImg;		/* 0x328 */
	int staticIndex;		/* 0x32c */
	CPsaImage *objective;		/* 0x330 */
	int objectiveIndex;		/* 0x334 */
	int objectiveChargeIndex;	/* 0x338 */
	int objectiveBlackIndex;		/* 0x33c */
	int objectiveWhiteIndex;		/* 0x340 */
	int canPossessIndex;		/* 0x344 */
	int canPossessGlowIndex;	/* 0x348 */
	CPsaImage *noiseImg;		/* 0x34c */
	CPsaImage *img350;		/* 0x350 white */
} cMonitorScorer;

cMonitorScorer *cMonitorScorer_ctor(cMonitorScorer *self);

/* ---- NetStatsScorer.cpp ---- */

#define NETSTATS_SLOTS 16

/* the ping and the bars of the net traffic of the client (0x230 bytes) */
typedef struct cNetStatsScorer {
	cScorerItem base;		/* 0x000 */
	float sent[NETSTATS_SLOTS];	/* 0x0ac the bar heights, going down */
	float recv[NETSTATS_SLOTS];	/* 0x0ec */
	float start[NETSTATS_SLOTS];	/* 0x12c the game time of the send before */
	float end[NETSTATS_SLOTS];	/* 0x16c of the last send */
	int sentBytes[NETSTATS_SLOTS];	/* 0x1ac */
	int recvBytes[NETSTATS_SLOTS];	/* 0x1ec */
	CPsaImage *img;			/* 0x22c white */
} cNetStatsScorer;

/* the counters of the net (Net/) for the stats: bytes and slot of the last send and receive */
extern int g_NetStatSent;
extern int g_NetStatSentSlot;
extern int g_NetStatRecv;
extern int g_NetStatRecvSlot;

cNetStatsScorer *cNetStatsScorer_ctor(cNetStatsScorer *self);

/* ---- the news panel of the scorer (between NetStatsScorer.cpp and RadarScorer.cpp, no CHECK) ---- */

/* the news panel rendered into the texture "NewsPanel": the news head between the background and the logo, or
 * the watched entities while the alarm is on */
typedef struct cNewsPanel {
	int engineInits;			/* 0x00 g_EngineInits of the last frame: the texture is drawn again after a reset */
	vec3_t camPos;			/* 0x04 of the entity view */
	CGameCamera *cam;		/* 0x10 */
	char *head;			/* 0x14 cString, NewsHead in use */
	char *logo;			/* 0x18 cString, NewsLogoBmp in use */
	char *back;			/* 0x1c cString, BackNewsBmp in use */
	CGameModel *model;		/* 0x20 of the head (a scorer model or an entity) */
	CPsaImage *logoImg;		/* 0x24 */
	CPsaImage *backImg;		/* 0x28 */
	float lastTime;			/* 0x2c the scorer time of the last update */
	float open;			/* 0x30 0..1 */
	byte closedDrawn;		/* 0x34 the closed panel is in the texture */
	byte pad_35[3];
} cNewsPanel;

cNewsPanel *cNewsPanel_ctor(cNewsPanel *self);
void cNewsPanel_dtor(cNewsPanel *self);
/* the switches back to their defaults, the images and the head dropped */
void cNewsPanel_Reset(cNewsPanel *self);
/* the texture of the frame, when the scorer time moved */
void cNewsPanel_Update(cNewsPanel *self);

/* ---- RadarScorer.cpp ---- */

#define RADAR_FLOORS 16
#define RADAR_ICONS 254		/* the icon frames, then their colors */
/* the flags of cRadarScorer_DrawIcon for an entity out of the radar */
#define RADAR_ICON_OUT_POINT 0x01	/* the icon points to the entity */
#define RADAR_ICON_OUT_ANG 0x02		/* turned like in the radar */
#define RADAR_ICON_OUT_FADE 0x04	/* fades with the distance (OutAlphaFactor) */
#define RADAR_ICON_OUT_BIG 0x08		/* twice the size */
#define RADAR_ICON_OUT_HIDE 0x10	/* not drawn */

/* the radar of the ship of the camera, in the menu (MenuMap) the scrolling map of the floors (0x94c bytes) */
typedef struct cRadarScorer {
	cScorerItem base;		/* 0x000 */
	float halfW, halfH;		/* 0x0ac the half size of the radar: 55 or 200, 100 in the menu */
	float halfW2, halfH2;		/* 0x0b4 56.25 or 200, 100 */
	CPsaImage *radar;		/* 0x0bc */
	int radarIndex;			/* 0x0c0 */
	CPsaImage *mask;		/* 0x0c4 */
	int maskIndex;			/* 0x0c8 */
	CPsaImage *icons;		/* 0x0cc */
	int iconAc;			/* 0x0d0 the icon of IconIndex */
	int iconIndex[(0x8c4 - 0xd4) / 4];	/* 0x0d4 the frames of the icons, from 0x4cc (RADAR_ICONS on) their last colors */
	CPsaImage *noise;		/* 0x8c4 */
	CPsaImage *cone;		/* 0x8c8 white, the view cones */
	int floor;			/* 0x8cc the floor of the ship */
	int prevFloor;			/* 0x8d0 */
	float floorFade;		/* 0x8d4 0..1 since the floor changed */
	int outAlphaFactor;		/* 0x8d8 100 */
	CPsaImage *floorMaps[RADAR_FLOORS];	/* 0x8dc FileNameMap2D of each floor */
	float map2dScale;		/* 0x91c 512 */
	float map2dScaleFactor;		/* 0x920 0.01 */
	float mapW, mapH;		/* 0x924 the map: DM_2dMap0 to DM_2dMap1, else the box of the world */
	float mapX0, mapZ0;		/* 0x92c */
	byte menuMap;			/* 0x934 */
	byte pad_935[3];
	float menuMinScale;		/* 0x938 512 */
	float menuMaxScale;		/* 0x93c 512 */
	float menuScaleSpeed;		/* 0x940 10 */
	float scrollX, scrollY;		/* 0x944 of the menu map */
} cRadarScorer;

extern cGVar GVar_FileNameMap2D;
#define g_FileNameMap2D (GVar_FileNameMap2D.value.s)	/* FileNameMap2D, a printf format of the floor */

cRadarScorer *cRadarScorer_ctor(cRadarScorer *self);
/* the Global/Table.h lookups of the input actions compiled here */
cInputAction *InputActionTable_At(void *table, uint32_t i);
cActionSet *ActionSetTable_At(void *table, uint32_t i);

/* ---- Scorer.cpp (src/game/Scorer/Scorer.c) ---- */

#define SCORER_MAX_HEAD_ANIMS 8

/* the flags and the default of an engine variable by its value (cGVar of include/scrap/scrap.h) */
#define GVAR_OF_VALUE(v) ((cGVar *)((byte *)&(v) - offsetof(cGVar, value)))
#define GVAR_FLAGS(v) (GVAR_OF_VALUE(v)->base.flags)
#define GVAR_DEF(type, v) (*(type *)&GVAR_OF_VALUE(v)->def)

extern CPsaImage *g_ScorerTabs;	/* 2D/Menu/Tabs.alpha.tga */
/* the engine variables of Scorer.cpp */
extern cGVar GVar_RadarMMSelect;
#define g_RadarMMSelect (GVar_RadarMMSelect.value.i)		/* RadarMMSelect */
extern cGVar GVar_NumEnemiesInRadar;
#define g_NumEnemiesInRadar (GVar_NumEnemiesInRadar.value.i)
extern cGVar GVar_MotionBlurFactorBegin;
#define g_MotionBlurFactorBegin (GVar_MotionBlurFactorBegin.value.f)	/* MotionBlurFactorBegin */
extern cGVar GVar_MotionBlurFactorEnd;
#define g_MotionBlurFactorEnd (GVar_MotionBlurFactorEnd.value.f)	/* MotionBlurFactorEnd */
extern cGVar GVar_MotionBlurTime;
#define g_MotionBlurTime (GVar_MotionBlurTime.value.f)	/* MotionBlurTime */
extern cGVar GVar_MotionBlurEndTime;
#define g_MotionBlurEndTime (GVar_MotionBlurEndTime.value.f)	/* MotionBlurEndTime */
extern cGVar GVar_MotionBlurOffVar;
#define g_MotionBlurOffVar (GVar_MotionBlurOffVar.value.f)	/* MotionBlurOffVar */
extern cGVar GVar_CurrentWaypointName;
#define g_CurrentWaypointName (GVar_CurrentWaypointName.value.s)	/* CurrentWaypointName */
extern cGVar GVar_CurrentWaypointName2;
#define g_CurrentWaypointName2 (GVar_CurrentWaypointName2.value.s)	/* CurrentWaypointName2 */

/* a bool variable of an item: 1 or 0 */
static inline int ScorerVar_Bool(byte *v, byte set, int *i) {
	if (set) {
		*v = *i != 0;
	} else {
		*i = *v != 0;
	}
	return 3;
}

/* the sound of an engine variable at its volume */
static inline void Scorer_PlaySound(const char *snd, float vol) {
	CSoundSys *s = g_pSound;
	SoundSys_Play(s, snd, Snd_Volume(vol), 0);
}

void cScorer_SetParam(cScorer *scorer, const char *name, const char *value);
void Scorer_SetInputState(int state);
void Scorer_GetCamVarStr(int player, char *buf, const char *name, int size);
uint32_t Scorer_ColorLerp(uint32_t a, uint32_t b, float t);
byte Scorer_Render(void);

/* the image slots of the items below: the old image deleted, the new one made with flags, color white */
static inline CPsaImage *ScorerImg_Replace(CPsaImage *old, const char *name, int blend, uint32_t flags) {
	if (old) {
		CPsaImage_dtor(old);
		operator_delete(old);
	}
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctorFlags(img, name, 0, blend, (int)flags) : NULL;
	img->color = 0xffffffff;
	return img;
}

/* the same with CPsaImage_ctor (blend from the flags) */
static inline CPsaImage *ScorerImg_ReplaceSimple(CPsaImage *old, const char *name, uint32_t flags) {
	if (old) {
		CPsaImage_dtor(old);
		operator_delete(old);
	}
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctor(img, name, (int)flags) : NULL;
	img->color = 0xffffffff;
	return img;
}

static inline void ScorerImg_Free(CPsaImage *img) {
	if (img) {
		CPsaImage_dtor(img);
		operator_delete(img);
	}
}

/* ---- ScraplandScorer.cpp: the animated logos of the main menu and of the sub menus ---- */

/* the logo of the main menu with its glow, neon and lighting (200 bytes); the animation state is global */
typedef struct cScraplandMainMenuScorer {
	cScorerItem base;		/* 0x00 */
	float time;			/* 0xac the game time of the last draw */
	float now;			/* 0xb0 */
	int highRes;			/* 0xb4 HighRes 0 or 1: the files get the flag 0x10000 */
	CPsaImage *logo;		/* 0xb8 FileLogo */
	CPsaImage *glow;		/* 0xbc FileGlow */
	CPsaImage *neon;		/* 0xc0 FileNeon */
	CPsaImage *lighting;		/* 0xc4 FileLighting */
} cScraplandMainMenuScorer;

/* the logo of a sub menu with its rings (0xd8 bytes) */
typedef struct cScraplandSubMenuScorer {
	cScorerItem base;		/* 0x00 */
	float time;			/* 0xac */
	float now;			/* 0xb0 */
	char *file;			/* 0xb4 cString, the last file set */
	int highRes;			/* 0xb8 */
	CPsaImage *logo;		/* 0xbc FileLogo */
	CPsaImage *glow;		/* 0xc0 FileGlow */
	CPsaImage *lighting;		/* 0xc4 FileLighting */
	CPsaImage *ringMask;		/* 0xc8 FileRingMask */
	CPsaImage *ringReflection;	/* 0xcc FileRingReflection */
	CPsaImage *ringBorder;		/* 0xd0 FileRingBorder */
	float scale;			/* 0xd4 Scale 1 */
} cScraplandSubMenuScorer;

cScraplandMainMenuScorer *cScraplandMainMenuScorer_ctor(cScraplandMainMenuScorer *self);
cScraplandSubMenuScorer *cScraplandSubMenuScorer_ctor(cScraplandSubMenuScorer *self);

/* ---- SliderScorer.cpp ---- */

/* a slider of a sprite sheet: the back frame and the knob moved by the cursor (0xf8 bytes) */
typedef struct cSliderScorer {
	cScorerItem base;		/* 0x00 */
	int highRes;			/* 0xac HighRes: half sizes, File with the flag 0x10000 */
	int blend;			/* 0xb0 BlendingMode 0..6, 3 */
	int backIndex;			/* 0xb4 BackSpriteIndex, not set by the constructor */
	int sliderIndex;		/* 0xb8 SliderSpriteIndex, not set by the constructor */
	int focusIndex;			/* 0xbc FocusSpriteIndex, the knob over it while the item is the cursor */
	float value;			/* 0xc0 Value, MinValue..MaxValue */
	float lastValue;		/* 0xc4 */
	float valueStep;		/* 0xc8 ValueStep */
	float minValue;			/* 0xcc MinValue */
	float maxValue;			/* 0xd0 MaxValue */
	float marginSize;		/* 0xd4 MarginSize at both ends */
	float maxValueForced;		/* 0xd8 MaxValueForced: the drag stops there if > 0 */
	byte accept;			/* 0xdc the accept of this frame */
	byte lastAccept;		/* 0xdd */
	byte dragging;			/* 0xde */
	byte pad_df;
	cPy_Function onChange;		/* 0xe0 OnChange (player, name) */
	CPsaImage *img;			/* 0xf4 File, its color by Red, Green, Blue, Alpha; Unit its b22 */
} cSliderScorer;

cSliderScorer *cSliderScorer_ctor(cSliderScorer *self);

/* ---- SpecialActionScorer.cpp ---- */

#define SPECIALACTION_MAX_ACTIONS 15

/* the special action of the character: its icon, energy and battery bars, the cannon aim (0x138 bytes) */
typedef struct cSpecialActionScorer {
	cScorerItem base;		/* 0x000 */
	int charAction;			/* 0x0ac CharAction: the draw handler 0..14 */
	int backIndex;			/* 0x0b0 BackIndex */
	float backOffX, backOffY;	/* 0x0b4 BackOffX, BackOffY */
	CPsaImage *sprite;		/* 0x0bc Sprite */
	int energyBackIndex;		/* 0x0c0 EnergyBackIndex */
	float energyBackOffX, energyBackOffY;	/* 0x0c4 */
	int energyBarIndex;		/* 0x0cc EnergyBarIndex */
	float energyBarOffX, energyBarOffY;	/* 0x0d0 */
	int batteryBarIndex;		/* 0x0d8 BatteryBarIndex */
	float batteryBarOffX, batteryBarOffY;	/* 0x0dc */
	float textOffX, textOffY;	/* 0x0e4 TextOffX, TextOffY */
	uint32_t textColor;		/* 0x0ec TextColorRed..Alpha, not set by the constructor */
	CFont *textFont;		/* 0x0f0 TextFont, Horatio */
	CPsaImage *cannon;		/* 0x0f4 CannonSprite */
	int cannonIndex;		/* 0x0f8 CannonIndex */
	int crossHairIndex;		/* 0x0fc CrossHairIndex */
	CPsaImage *icon[2];		/* 0x100 IconSprite1, IconSprite2 */
	CPsaImage *iconGlow[2];		/* 0x108 the same files, blend 2 */
	int iconIndexSprite0;		/* 0x110 IconIndexSprite0 */
	float iconOffX, iconOffY;	/* 0x114 IconOffX, IconOffY */
	int iconIndex;			/* 0x11c IconIndex */
	int glowIconIndex;		/* 0x120 GlowIconIndex */
	uint32_t inactiveColor;		/* 0x124 InactiveColorRed..Alpha */
	uint32_t activeColor;		/* 0x128 ActiveColorRed..Alpha */
	uint32_t glowColor;		/* 0x12c GlowColorRed..Alpha */
	uint32_t energyBackColor;	/* 0x130 0x5cffcc00 */
	uint32_t batteryColor;		/* 0x134 0xffffffff */
} cSpecialActionScorer;

extern cGVar GVar_SpecialActionActive;
#define g_SpecialActionActive (GVar_SpecialActionActive.value.i)	/* SpecialActionActive 0 off, 1 blink, 2 on */

cSpecialActionScorer *cSpecialActionScorer_ctor(cSpecialActionScorer *self);

extern cGVar GVar_EnergyBarActive;
#define g_EnergyBarActive (GVar_EnergyBarActive.value.i)		/* EnergyBarActive */

/* the exe functions the special action draws use (other modules) */
/* frame + 1 stretched to the width w, frame at both ends (the right one mirrored), h the y scale */
void Menu_DrawStretch(CPsaImage *img, int frame, float x, float y, float w, float h);

#endif
