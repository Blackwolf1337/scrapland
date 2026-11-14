/*
 * The HUD and menu items of Menu/ from AbstractTextScorer.cpp to RadarScorer.cpp (the classes over cScorerItem of
 * include/scrap/scorer.h). Every class has the slots Draw(player), Var and Free.
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

/* ---- the base of the sprite items (SpriteScorer.cpp, not in this module) ---- */
/* the vtable of cSpriteScorer and the items made from it (bar, button, hangar ship, mission, gradient) */
struct cSpriteScorer;
typedef struct cSpriteScorerVtbl {
	cScorerItemVtbl base;
	void(*SetMultiSprite)(struct cSpriteScorer *self, byte on);
} cSpriteScorerVtbl;

typedef struct cSpriteScorer {
	cScorerItem base;
	float jumpFX;
	byte numSubSprites;		/* 1 */
	byte spriteIndex;		/* 1, the frame drawn */
	byte isMultiSprite;
	byte highRes;
	byte fixPosition;
	byte blendingMode;		/* 3 */
	byte rotate;
	byte mirror;
	byte discardable;
	byte pad_b9[3];
	CPsaImage *img;
	char *file;			/* cString */
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

/* a text in a font: the base of the text items */
typedef struct cAbstractTextScorer {
	cScorerItem base;
	uint32_t color;			/* 0xAARRGGBB */
	int align;			/* TEXTSCORER_LEFT, _RIGHT, _CENTER */
	int effect;			/* TEXTSCORER_OSCILE, _SHADOW, _OUTLINE or 0 */
	CFont *font;
	char *fontName;			/* cString */
	char *text;			/* cString */
} cAbstractTextScorer;

/* the credits: 20 lines in two fonts, faded in a render target */
#define CREDITS_LINES 20
#define CREDITS_LINE_SIZE 0x280
typedef struct cCreditsScorer {
	cAbstractTextScorer base;
	CFont *font;			/* ScrapMedium, the lines after the first */
	CFont *titleFont;		/* Horatio, the first line */
	float time;
	float life;
	float fadeIn;
	float fadeOut;
	/* Text0..Text19; a text in line 8 puts the lines in three columns, without the fades */
	char lines[CREDITS_LINES][CREDITS_LINE_SIZE];
} cCreditsScorer;

/* the hint of the cursor item and the label text (the same layout), with a frame behind */
typedef struct cHintScorer {
	cAbstractTextScorer base;
	CPsaImage *tab;			/* the frame sprite sheet */
	int tabIndex;			/* the first frame of Menu_DrawFrame, none if <= 0 */
	int tabBlend;			/* 0..6 */
} cHintScorer;
typedef cHintScorer cLabelScorer;

/* the message text of Scorer_SetMsgText */
typedef struct cMsgScorer {
	cAbstractTextScorer base;
	float lineHeight;		/* the height of "W" */
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
	cSpriteScorer base;
	float maxValue;
	float blinkTime;
	byte blink;
	byte blinkOff;			/* the bar is hidden in this half of the blink */
	byte pad_ce[2];
	int mainIndex;			/* the frame of the bar */
	int blackIndex;			/* the frame of the empty part */
	float mainUV0[2], mainUV1[2];
	float blackUV0[2], blackUV1[2];
	int backIndex;			/* the frame behind, none if 0 */
	uint32_t backColor;
	uint32_t mainColor;
	uint32_t blackColor;
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
	cSpriteScorer base;
	cStrTable lines;
	cByteTable isValue;		/* 1 for the value of a name:value line */
	CFont *font;			/* Horatio */
	float xAlignLeft;		/* XAlignLeft: x */
	float xAlignRight;		/* x + w */
	float xAlignCenter;		/* x + w / 2 */
	int highlight;
	int glow;
	int focusSpriteIndex;		/* 1 */
	int borderSpriteIndex;		/* -1 for none */
	int width;			/* the cells inside the box, 1 */
	int height;
	int maxLineSize;		/* -1: no word wrap */
	int hasText;
	int textAlign;			/* BUTTON_CENTER */
	int marker;
	int isNameValuePair;		/* 1 */
	int renderAlphaChannel;		/* 0, 1 or 2 */
	int boxH;			/* the box height in pixels */
	int textH;			/* the text height in pixels */
	float markerOffset;
	float highlightOffset;
	float lineHeight;
	float scaleSmoothWidth;		/* the scale of the focused sprite */
	float scaleSmoothHeight;
	float scaleSmoothTime;
	uint32_t textColor;
	uint32_t borderColor;
	float origScaleX, origScaleY;	/* the scale of the sprite */
	float textWidth;		/* the widest line, -1 */
	float textHeight;		/* its height */
	int complex;			/* 1 */
	uint32_t highLightColor;
	float minX, maxX;		/* the columns of name:value lines, -1 */
	CPsaImage *alphaImg;		/* white, behind the sprite (RenderAlphaChannel) */
	CPsaImage *focusImg;		/* the highlight and border frames (File) */
	int highLightBlend;		/* 3 */
} cButtonScorer;

cButtonScorer *cButtonScorer_ctor(cButtonScorer *self);

/* ---- CannonScorer.cpp ---- */

#define CANNON_WEAPONS 7

/* the crosshair of the ship of the camera: the center sprite, the aim of the weapon at the aim point of the auto aim,
 * the lock text and the life warning (0xec bytes) */
struct cCannonScorer;
typedef struct cCannonScorer {
	cScorerItem base;
	byte shown;			/* the aim was drawn in the last frame (the no ammo sound when it goes) */
	byte pad_ad[3];
	int numWeapons;			/* of the world at the creation */
	float angle;			/* of the aim */
	float size[2];			/* 20, 20 */
	uint32_t color;			/* the aim of the weapon in use */
	CPsaImage *img;
	int crossHairIndex;		/* the center frame, none if 0 */
	int aimIndex[CANNON_WEAPONS];	/* the aim frame per weapon, none if 0 */
	int aimAc;			/* the weapon of AimIndex */
} cCannonScorer;

extern cGVar GVar_WarningLifeLevel;
#define g_WarningLifeLevel (GVar_WarningLifeLevel.value.i)	/* WarningLifeLevel, percent */

cCannonScorer *cCannonScorer_ctor(cCannonScorer *self);

/* ---- ChatScorer.cpp ---- */

#define CHAT_MSGS 16

/* the chat lines of Scorer_AddChatMsg */
typedef struct cChatScorer {
	cScorerItem base;
	CPsaImage *img;			/* white, not drawn */
	int numMsg;			/* the newest lines shown, 0..16 */
	CFont *font;			/* HoratioSmall */
	float lineHeight;		/* 14 */
} cChatScorer;

/* a chat line, the oldest first */
typedef struct cChatMsg {
	char text[0x50];
	uint32_t color;
} cChatMsg;

cChatScorer *cChatScorer_ctor(cChatScorer *self);

/* ---- CircuitScorer.cpp ---- */

#define CIRCUIT_PATHS 9
#define CIRCUIT_FLARES 4
#define CIRCUIT_TRAIL 24

/* the circuit board picture with flares running along its paths */
typedef struct cCircuitScorer {
	cScorerItem base;
	float lastTime;			/* the game time of the last frame */
	float curTime;
	char *file;			/* cString, the last FileCircuit or FileFlare */
	int highRes;
	int blend;			/* 3 */
	CPsaImage *circuit;
	CPsaImage *flare;		/* frames 1 (head) and 2 (trail) */
} cCircuitScorer;

/* a path of the board in pixels of the picture, with the lengths of its segments */
typedef struct cCircuitPath {
	int numPoints;
	float pts[32][2];
	float segStart[32];		/* the length before each segment */
	float segLen[32];
	float normStart[32];		/* segStart / total */
	float normEnd[32];		/* (segStart + segLen) / total */
	float total;
} cCircuitPath;

/* a flare on a path, then a pause */
typedef struct cCircuitFlare {
	float time;			/* left of the run */
	float duration;
	float delay;			/* the pause before the next run */
	int path;
	int reverse;
	int unused_14;
} cCircuitFlare;

cCircuitScorer *cCircuitScorer_ctor(cCircuitScorer *self);

/* ---- CopMeterScorer.cpp ---- */

/* the police alarm meter: a back frame, a bar to the alarm level of the world, two blinking lights while the
 * alarm is on; fades in with the alarm (0xf0 bytes) */
typedef struct cCopMeterScorer {
	cScorerItem base;
	CPsaImage *img;
	CPsaImage *flash;		/* the same sheet for the lights */
	float backOffX, backOffY;
	int backIndex;
	float lightOffX, lightOffY;
	int lightIndex;			/* the light frames +0..+2 */
	float barOffX, barOffY;
	int barIndex;
	float percent;			/* the bar 0..1, follows target */
	float target;			/* the alarm level of the world */
	byte on;			/* the alarm is on */
	byte pad_e1[3];
	int unused_e4;
	float fade;			/* 0..1 */
	float blink;			/* 0..0.9 the blink phase */
} cCopMeterScorer;

cCopMeterScorer *cCopMeterScorer_ctor(cCopMeterScorer *self);

/* ---- HangarShipScorer.cpp ---- */

/* a ship of the world turning in a view of its own over a grid frame */
typedef struct cHangarShipScorer {
	cSpriteScorer base;
	CGameCamera *cam;
	CPsaImage *grid;		/* the sprite sheet of the item */
	char *targetName;		/* cString, the ship */
	byte reset;			/* the angles start at 0 in the next frame */
	byte pad_d1[3];
	vec3_t rot;			/* the angles of the ship in the view */
	int gridIndex;
	uint32_t gridColor;
	float camDist;			/* 100 */
	float camFov;			/* 20 degrees */
} cHangarShipScorer;

cHangarShipScorer *cHangarShipScorer_ctor(cHangarShipScorer *self);

/* ---- the models of the scorer (Scorer.cpp, SScorer.AddModel) ---- */

typedef struct cScorerModel {
	char name[0x20];
	CGameModel model;
	int numAnims;
	int anims[8];
	struct cScorerModel *next;
	float radius;
	float height;			/* the eyes */
	struct cEmbeddedFX *extra;	/* its Tick draws after the model */
} cScorerModel;

/* the model of that name of the scorer, NULL if none */
cScorerModel *cScorer_FindModel(cScorer *scorer, const char *name);

/* ---- HeadScorer.cpp ---- */

/* a talking head: a scorer model in a view of its own, the camera swaying */
/* the vtable of cHeadScorer, also the one of cMonitorScorer (the same two slots, its own functions) */
typedef struct cHeadScorerVtbl {
	cScorerItemVtbl base;
	void(*SetAnim)(cScorerItem *self, int anim);
	byte(*SetModel)(cScorerItem *self, const char *name);
} cHeadScorerVtbl;

/* calls through the slots of cHeadScorerVtbl */
static inline byte HeadScorer_SetModel(void *self, const char *name) {
	return VT(cHeadScorerVtbl, self)->SetModel(self, name);
}

static inline void HeadScorer_SetAnim(void *self, int anim) {
	VT(cHeadScorerVtbl, self)->SetAnim(self, anim);
}

typedef struct cHeadScorer {
	cScorerItem base;
	float time;			/* the sway, random start */
	cScorerModel *model;
	int anim;			/* 1 */
	CGameCamera *cam;
	float vw, vh;			/* the view size */
} cHeadScorer;

cHeadScorer *cHeadScorer_ctor(cHeadScorer *self);

/* ---- MissionScorer.cpp ---- */

#define MISSION_STATUS 3

/* the mission icons (three status lamps) and the text of the next mission's map (0xec bytes); the status and
 * the blink are engine variables shared by all */
typedef struct cMissionScorer {
	cSpriteScorer base;
	const char *nextText;		/* the language text NextMissionMapText */
	CFont *font;			/* Horatio */
	char *forceMap;			/* cString, the map instead of the save variable Mission.Map */
	int backIndex;
	int primaryIndex;		/* the frame of status 0 */
	int secondaryIndex;		/* of status 1 and 2 */
	uint32_t textColor;
	CPsaImage *tab;
	int tabIndex;
	int tabBlend;			/* 0..6 */
} cMissionScorer;

cMissionScorer *cMissionScorer_ctor(cMissionScorer *self);

/* ---- MonitorScorer.cpp ---- */

#define MONITOR_WEAPONS 7
#define MONITOR_UPGRADES 13

/* the ship monitor: the panel, the head of the talk (Scorer_SetHeadMonitor) with its text, the weapon and upgrade
 * icons, the objective, the static noise; the view of the target (0x354 bytes) */
typedef struct cMonitorScorer {
	cScorerItem base;
	float textTime;
	float noise;			/* 4 */
	CGameCamera *cam;
	cEntity *lastTarget;		/* the entity seen last, its descriptor is the text */
	char text[0x140];
	float textShown;		/* the time the text shows, 20 letters a second */
	CFont *smallFont;		/* Terminal8 */
	CFont *bigFont;			/* Horatio */
	cScorerModel *model;		/* the head */
	int anim;			/* 1 */
	byte targetView;
	byte pad_211[3];
	float objFrom[2];		/* the objective mark moves from there */
	float objPos[2];		/* to the screen position of the entity to photograph */
	char objName[0x80];		/* the name of that entity */
	float objBlend;			/* 1 */
	byte objWas;			/* the mark was on an entity before */
	byte pad_2a9[3];
	CPsaImage *weapon;		/* the weapon icons */
	float weaponOffX, weaponOffY;
	int weaponIcons[MONITOR_WEAPONS];
	int weaponIconAc;		/* the weapon of WeaponIconIndex */
	int upgradeIcons[MONITOR_UPGRADES];
	int upgradeIconAc;
	CPsaImage *panel;
	int panelIndex;
	CPsaImage *mask;
	int maskIndex;
	CPsaImage *frame;
	int frameIndex;
	CPsaImage *staticImg;
	int staticIndex;
	CPsaImage *objective;
	int objectiveIndex;
	int objectiveChargeIndex;
	int objectiveBlackIndex;
	int objectiveWhiteIndex;
	int canPossessIndex;
	int canPossessGlowIndex;
	CPsaImage *noiseImg;
	CPsaImage *img350;		/* white */
} cMonitorScorer;

cMonitorScorer *cMonitorScorer_ctor(cMonitorScorer *self);

/* ---- NetStatsScorer.cpp ---- */

#define NETSTATS_SLOTS 16

/* the ping and the bars of the net traffic of the client */
typedef struct cNetStatsScorer {
	cScorerItem base;
	float sent[NETSTATS_SLOTS];	/* the bar heights, going down */
	float recv[NETSTATS_SLOTS];
	float start[NETSTATS_SLOTS];	/* the game time of the send before */
	float end[NETSTATS_SLOTS];	/* of the last send */
	int sentBytes[NETSTATS_SLOTS];
	int recvBytes[NETSTATS_SLOTS];
	CPsaImage *img;			/* white */
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
	int engineInits;			/* g_EngineInits of the last frame: the texture is drawn again after a reset */
	vec3_t camPos;			/* of the entity view */
	CGameCamera *cam;
	char *head;			/* cString, NewsHead in use */
	char *logo;			/* cString, NewsLogoBmp in use */
	char *back;			/* cString, BackNewsBmp in use */
	CGameModel *model;		/* of the head (a scorer model or an entity) */
	CPsaImage *logoImg;
	CPsaImage *backImg;
	float lastTime;			/* the scorer time of the last update */
	float open;			/* 0..1 */
	byte closedDrawn;		/* the closed panel is in the texture */
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

/* the radar of the ship of the camera, in the menu (MenuMap) the scrolling map of the floors */
typedef struct cRadarScorer {
	cScorerItem base;
	float halfW, halfH;		/* the half size of the radar: 55 or 200, 100 in the menu */
	float halfW2, halfH2;		/* 56.25 or 200, 100 */
	CPsaImage *radar;
	int radarIndex;
	CPsaImage *mask;
	int maskIndex;
	CPsaImage *icons;
	int iconAc;			/* the icon of IconIndex */
	int iconIndex[(0x8c4 - 0xd4) / 4];	/* the frames of the icons, from 0x4cc (RADAR_ICONS on) their last colors */
	CPsaImage *noise;
	CPsaImage *cone;		/* white, the view cones */
	int floor;			/* the floor of the ship */
	int prevFloor;
	float floorFade;		/* 0..1 since the floor changed */
	int outAlphaFactor;		/* 100 */
	CPsaImage *floorMaps[RADAR_FLOORS];	/* FileNameMap2D of each floor */
	float map2dScale;		/* 512 */
	float map2dScaleFactor;		/* 0.01 */
	float mapW, mapH;		/* the map: DM_2dMap0 to DM_2dMap1, else the box of the world */
	float mapX0, mapZ0;
	byte menuMap;
	byte pad_935[3];
	float menuMinScale;		/* 512 */
	float menuMaxScale;		/* 512 */
	float menuScaleSpeed;		/* 10 */
	float scrollX, scrollY;		/* of the menu map */
} cRadarScorer;

extern cGVar GVar_FileNameMap2D;
#define g_FileNameMap2D (GVar_FileNameMap2D.value.s)	/* FileNameMap2D, a printf format of the floor */

cRadarScorer *cRadarScorer_ctor(cRadarScorer *self);
/* the icons the last radar (not the menu map) drew, for the test channel (not in the original): the icon frame, 1 when
 * it stood on the rim (out of range), its color and the world point it marks */
typedef struct RadarSeen {
	int icon;
	byte out;
	uint32_t color;
	vec3_t pos;
} RadarSeen;
#define RADAR_SEEN_MAX 256
const RadarSeen *Radar_Seen(int *n);
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
	cScorerItem base;
	float time;			/* the game time of the last draw */
	float now;
	int highRes;			/* HighRes 0 or 1: the files get the flag 0x10000 */
	CPsaImage *logo;		/* FileLogo */
	CPsaImage *glow;		/* FileGlow */
	CPsaImage *neon;		/* FileNeon */
	CPsaImage *lighting;		/* FileLighting */
} cScraplandMainMenuScorer;

/* the logo of a sub menu with its rings */
typedef struct cScraplandSubMenuScorer {
	cScorerItem base;
	float time;
	float now;
	char *file;			/* cString, the last file set */
	int highRes;
	CPsaImage *logo;		/* FileLogo */
	CPsaImage *glow;		/* FileGlow */
	CPsaImage *lighting;		/* FileLighting */
	CPsaImage *ringMask;		/* FileRingMask */
	CPsaImage *ringReflection;	/* FileRingReflection */
	CPsaImage *ringBorder;		/* FileRingBorder */
	float scale;			/* Scale 1 */
} cScraplandSubMenuScorer;

cScraplandMainMenuScorer *cScraplandMainMenuScorer_ctor(cScraplandMainMenuScorer *self);
cScraplandSubMenuScorer *cScraplandSubMenuScorer_ctor(cScraplandSubMenuScorer *self);

/* ---- SliderScorer.cpp ---- */

/* a slider of a sprite sheet: the back frame and the knob moved by the cursor */
typedef struct cSliderScorer {
	cScorerItem base;
	int highRes;			/* HighRes: half sizes, File with the flag 0x10000 */
	int blend;			/* BlendingMode 0..6, 3 */
	int backIndex;			/* BackSpriteIndex, not set by the constructor */
	int sliderIndex;		/* SliderSpriteIndex, not set by the constructor */
	int focusIndex;			/* FocusSpriteIndex, the knob over it while the item is the cursor */
	float value;			/* Value, MinValue..MaxValue */
	float lastValue;
	float valueStep;		/* ValueStep */
	float minValue;			/* MinValue */
	float maxValue;			/* MaxValue */
	float marginSize;		/* MarginSize at both ends */
	float maxValueForced;		/* MaxValueForced: the drag stops there if > 0 */
	byte accept;			/* the accept of this frame */
	byte lastAccept;
	byte dragging;
	byte pad_df;
	cPy_Function onChange;		/* OnChange (player, name) */
	CPsaImage *img;			/* File, its color by Red, Green, Blue, Alpha; Unit its b22 */
} cSliderScorer;

cSliderScorer *cSliderScorer_ctor(cSliderScorer *self);

/* ---- SpecialActionScorer.cpp ---- */

#define SPECIALACTION_MAX_ACTIONS 15

/* the special action of the character: its icon, energy and battery bars, the cannon aim */
typedef struct cSpecialActionScorer {
	cScorerItem base;
	int charAction;			/* CharAction: the draw handler 0..14 */
	int backIndex;			/* BackIndex */
	float backOffX, backOffY;	/* BackOffX, BackOffY */
	CPsaImage *sprite;		/* Sprite */
	int energyBackIndex;		/* EnergyBackIndex */
	float energyBackOffX, energyBackOffY;
	int energyBarIndex;		/* EnergyBarIndex */
	float energyBarOffX, energyBarOffY;
	int batteryBarIndex;		/* BatteryBarIndex */
	float batteryBarOffX, batteryBarOffY;
	float textOffX, textOffY;	/* TextOffX, TextOffY */
	uint32_t textColor;		/* TextColorRed..Alpha, not set by the constructor */
	CFont *textFont;		/* TextFont, Horatio */
	CPsaImage *cannon;		/* CannonSprite */
	int cannonIndex;		/* CannonIndex */
	int crossHairIndex;		/* CrossHairIndex */
	CPsaImage *icon[2];		/* IconSprite1, IconSprite2 */
	CPsaImage *iconGlow[2];		/* the same files, blend 2 */
	int iconIndexSprite0;		/* IconIndexSprite0 */
	float iconOffX, iconOffY;	/* IconOffX, IconOffY */
	int iconIndex;			/* IconIndex */
	int glowIconIndex;		/* GlowIconIndex */
	uint32_t inactiveColor;		/* InactiveColorRed..Alpha */
	uint32_t activeColor;		/* ActiveColorRed..Alpha */
	uint32_t glowColor;		/* GlowColorRed..Alpha */
	uint32_t energyBackColor;	/* 0x5cffcc00 */
	uint32_t batteryColor;		/* 0xffffffff */
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
