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

/* the vtables of the exe */
#define CABSTRACTTEXTSCORER_VFTABLE GAME_VFTABLE(cAbstractTextScorer_vftable)
#define CMSGSCORER_VFTABLE GAME_VFTABLE(cMsgScorer_vftable)
#define CLABELSCORER_VFTABLE GAME_VFTABLE(cLabelScorer_vftable)
#define CHINTSCORER_VFTABLE GAME_VFTABLE(cHintScorer_vftable)
#define CCREDITSSCORER_VFTABLE GAME_VFTABLE(cCreditsScorer_vftable)
#define CBARSCORER_VFTABLE GAME_VFTABLE(cBarScorer_vftable)

/* the x87 arithmetic of the originals on float returns (st0), in the precision the game set */
static inline float Scorer_X87MulSub(float a, float b, float c) {
	float r;

	/* c - a * b */
	__asm__("fmuls %1\n\tfsubrs %2" : "=t"(r) : "m"(b), "m"(c), "0"(a));
	return r;
}

static inline float Scorer_X87MulAdd(float a, float b, float c) {
	float r;

	/* a * b + c */
	__asm__("fmuls %1\n\tfadds %2" : "=t"(r) : "m"(b), "m"(c), "0"(a));
	return r;
}

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
SD_SIZE(cSpriteScorer, 0xc4);

cSpriteScorer *cSpriteScorer_ctor(cSpriteScorer *self);
void cSpriteScorer_dtor(cSpriteScorer *self);
int cSpriteScorer_Var(cSpriteScorer *self, const char *name, byte set, char **s, double *d, int *i);

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
SD_SIZE(cAbstractTextScorer, 0xc4);

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
SD_SIZE(cCreditsScorer, 0x32dc);

/* the hint of the cursor item and the label text (the same layout), with a frame behind (0xd0 bytes) */
typedef struct cHintScorer {
	cAbstractTextScorer base;	/* 0x00 */
	CPsaImage *tab;			/* 0xc4 the frame sprite sheet */
	int tabIndex;			/* 0xc8 the first frame of Menu_DrawFrame, none if <= 0 */
	int tabBlend;			/* 0xcc 0..6 */
} cHintScorer;
SD_SIZE(cHintScorer, 0xd0);
typedef cHintScorer cLabelScorer;

/* the message text of Scorer_SetMsgText (0xc8 bytes) */
typedef struct cMsgScorer {
	cAbstractTextScorer base;	/* 0x00 */
	float lineHeight;		/* 0xc4 the height of "W" */
} cMsgScorer;
SD_SIZE(cMsgScorer, 0xc8);

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
extern cGVar GVar_HintAppearsTime;
#define g_HintAppearsTime (GVar_HintAppearsTime.value.f)	/* HintAppearsTime */
extern cGVar GVar_MissionScorerName;
#define g_MissionScorerName (GVar_MissionScorerName.value.s)	/* MissionScorerName, the item hiding the label */
extern float g_MissionSolidTime;	/* cMissionScorer SolidTime */
/* the label (Scorer_SetLabelText) */
extern char g_LabelText[250];	/* 0xfa */
extern float g_LabelTime;		/* shown until this world time, always if <= 0 */
/* the message (Scorer_SetMsgText): the text and its lines */
extern char g_MsgText[62500];		/* 0xf424 */
extern int g_MsgNumLines;
extern char * g_MsgLines[20];		/* MSG_LINES */
#define MSG_LINES 20	/* the slots up to g_LabelText at 0x45d190 */
extern cGVar GVar_TimeToMsg;
#define g_MsgTime (GVar_TimeToMsg.value.f)
/* the hint text and its two lines */
extern char * g_HintLines[2];
extern char g_HintText[1280];		/* 0x500 */

cAbstractTextScorer *cAbstractTextScorer_ctor(cAbstractTextScorer *self);
cCreditsScorer *cCreditsScorer_ctor(cCreditsScorer *self);
cHintScorer *cHintScorer_ctor(cHintScorer *self);
cLabelScorer *cLabelScorer_ctor(cLabelScorer *self);
cMsgScorer *cMsgScorer_ctor(cMsgScorer *self);
void cAbstractTextScorer_dtor(cAbstractTextScorer *self);
void cCreditsScorer_dtor(cCreditsScorer *self);
void cHintScorer_dtor(cHintScorer *self);
void cLabelScorer_dtor(cLabelScorer *self);
cAbstractTextScorer *cAbstractTextScorer_Delete(cAbstractTextScorer *self, byte flags);
cCreditsScorer *cCreditsScorer_Delete(cCreditsScorer *self, byte flags);
cHintScorer *cHintScorer_Delete(cHintScorer *self, byte flags);
cLabelScorer *cLabelScorer_Delete(cLabelScorer *self, byte flags);
cMsgScorer *cMsgScorer_Delete(cMsgScorer *self, byte flags);
/* the 20 lines at the position of the phase: fade 0 the scorer, 1 the render target */
void cCreditsScorer_DrawLines(cCreditsScorer *self, byte fade, uint32_t color);
void cScorerItem_SetParam(cScorerItem *self, const char *name, const char *value);
/* Text, Red, Green, Blue, Alpha; set only: Font, CentralText, GetExtens (1: the size of the text); Align, Effect */
int cAbstractTextScorer_Var(cAbstractTextScorer *self, const char *name, byte set, char **s, double *d,
	int *i);
/* Time, Life, FadeIn, FadeOut; set only: Text0..Text19 */
int cCreditsScorer_Var(cCreditsScorer *self, const char *name, byte set, char **s, double *d, int *i);
/* TabSprite (set), TabBlendingMode, TabSpriteIndex, TabRed, TabGreen, TabBlue, TabAlpha (also cLabelScorer) */
int cHintScorer_Var(cHintScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cCreditsScorer_Draw(cCreditsScorer *self, int player);
void cHintScorer_Draw(cHintScorer *self, int player);
void cLabelScorer_Draw(cLabelScorer *self, int player);
void cMsgScorer_Draw(cMsgScorer *self, int player);

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
SD_SIZE(cBarScorer, 0x108);

/* IsAutoUpdatable: 1 down to AutoUpdateStopValue, 2 up to the maximum, by the frame time and the speed */
extern cGVar GVar_BarValue;
#define g_BarValue (GVar_BarValue.value.f)		/* BarValue */
extern cGVar GVar_AutoUpdateStopValue;
#define g_BarStopValue (GVar_AutoUpdateStopValue.value.f)	/* AutoUpdateStopValue */
extern cGVar GVar_AutoUpdateSpeedFactor;
#define g_BarSpeedFactor (GVar_AutoUpdateSpeedFactor.value.f)	/* AutoUpdateSpeedFactor */
extern cGVar GVar_IsAutoUpdatable;
#define g_BarAutoUpdate (GVar_IsAutoUpdatable.value.i)	/* IsAutoUpdatable */
extern cGVar GVar_BlinkTime;
#define g_BarBlinkTime (GVar_BlinkTime.value.f)	/* BlinkTime */
#define g_BarOnFinish (&GAME_GLOBAL_IN(cPy_Function, GVar_OnFinishAutoUpdate, 16))	/* OnFinishAutoUpdate */

cBarScorer *cBarScorer_ctor(cBarScorer *self);
void cBarScorer_dtor(cBarScorer *self);
cBarScorer *cBarScorer_Delete(cBarScorer *self, byte flags);
/* MaxValue, Value, AutoUpdateStopValue, AutoUpdateSpeedFactor, MainBarIndex, BlackBarIndex, IsAutoUpdatable,
 * Blink, BackIndex and the Red, Green, Blue, Alpha of Back, Main and Black */
int cBarScorer_Var(cBarScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cBarScorer_Draw(cBarScorer *self, int player);

/* ---- ButtonScorer.cpp ---- */

#define CBUTTONSCORER_VFTABLE GAME_VFTABLE(cButtonScorer_vftable)
#define CSTRING_DTOR 0x133990
#define CSTRING_CTOR 0x133980

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
SD_SIZE(cButtonScorer, 0x168);
SD_OFFSET(cButtonScorer, hasText, 0x100);
SD_OFFSET(cButtonScorer, minX, 0x154);

/* the Global/Table.h functions of cString and byte tables compiled here */
void StrTable_Free(cStrTable *self);
void ByteTable_Free(cByteTable *self);
char **StrTable_At(cStrTable *self, uint32_t i);
void StrTable_Init(cStrTable *self, int num);

cButtonScorer *cButtonScorer_ctor(cButtonScorer *self);
void cButtonScorer_dtor(cButtonScorer *self);
cButtonScorer *cButtonScorer_Delete(cButtonScorer *self, byte flags);
/* the text with a line break before every word past MaxLineSize + 75 pixels appended to *out; the lines */
int cButtonScorer_WrapLines(cButtonScorer *self, char **out, const char *text);
/* the sheet of the highlight and border frames */
void cButtonScorer_SetFile(cButtonScorer *self, uint32_t flags, const char *name);
/* the lines of the text (and of the name:value pairs) and the widest one; the lines measured */
int cButtonScorer_SetText(cButtonScorer *self, const char *text);
int cButtonScorer_Var(cButtonScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cButtonScorer_Draw(cButtonScorer *self, int player);

/* ---- CannonScorer.cpp ---- */

#define CCANNONSCORER_VFTABLE GAME_VFTABLE(cCannonScorer_vftable)
#define CANNON_WEAPONS 7

/* the crosshair of the ship of the camera: the center sprite, the aim of the weapon at the aim point of the auto aim,
 * the lock text and the life warning (0xec bytes) */
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
SD_SIZE(cCannonScorer, 0xec);

extern cGVar GVar_CannonScorerNoAmmoSound;
#define g_CannonNoAmmoSound (GVar_CannonScorerNoAmmoSound.value.s)	/* CannonScorerNoAmmoSound */
extern cGVar GVar_CannonScorerNoAmmoSoundVol;
#define g_CannonNoAmmoSoundVol (GVar_CannonScorerNoAmmoSoundVol.value.f)	/* CannonScorerNoAmmoSoundVol */
extern cGVar GVar_WarningLifeLevel;
#define g_WarningLifeLevel (GVar_WarningLifeLevel.value.i)	/* WarningLifeLevel, percent */

cCannonScorer *cCannonScorer_ctor(cCannonScorer *self);
void cCannonScorer_dtor(cCannonScorer *self);
cCannonScorer *cCannonScorer_Delete(cCannonScorer *self, byte flags);
/* the aim frame of weapon three times around pos, turned by angle */
void cCannonScorer_DrawAim(cCannonScorer *self, int weapon, const float *pos, const float *scale,
	const float *unused, float angle, uint32_t color);
/* the aim of the weapon of the ship at the screen point of its auto aim */
void cCannonScorer_DrawShipAim(cCannonScorer *self, int player, cEntity *ship);
/* the center sprite, the lock text and DANGER under the warning life */
void cCannonScorer_DrawCenter(cCannonScorer *self, int player, cEntity *ship);
/* Sprite (set), CrossHairIndex, AimAc, AimIndex */
int cCannonScorer_Var(cCannonScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cCannonScorer_Draw(cCannonScorer *self, int player);
/* vf04: "Searching..." or "Target Lock" above pos for the lock of the ship of the player */
void cCannonScorer_DrawLock(cCannonScorer *self, int player, const float *pos);

/* ---- ChatScorer.cpp ---- */

#define CCHATSCORER_VFTABLE GAME_VFTABLE(cChatScorer_vftable)
#define CHAT_MSGS 16

/* the chat lines of Scorer_AddChatMsg (0xbc bytes) */
typedef struct cChatScorer {
	cScorerItem base;		/* 0x00 */
	CPsaImage *img;			/* 0xac white, not drawn */
	int numMsg;			/* 0xb0 the newest lines shown, 0..16 */
	CFont *font;			/* 0xb4 HoratioSmall */
	float lineHeight;		/* 0xb8 14 */
} cChatScorer;
SD_SIZE(cChatScorer, 0xbc);

/* a chat line, the oldest first */
typedef struct cChatMsg {
	char text[0x50];		/* 0x00 */
	uint32_t color;			/* 0x50 */
} cChatMsg;
SD_SIZE(cChatMsg, 0x54);

extern cChatMsg g_ChatMsgs[16];	/* CHAT_MSGS */
extern float g_ChatTime;		/* of the last scroll */
extern cGVar GVar_ChatMsgPersistence;
#define g_ChatScrollTime (GVar_ChatMsgPersistence.value.f)	/* an empty line comes in after this */

cChatScorer *cChatScorer_ctor(cChatScorer *self);
void cChatScorer_dtor(cChatScorer *self);
cChatScorer *cChatScorer_Delete(cChatScorer *self, byte flags);
/* NumMsg */
int cChatScorer_Var(cChatScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cChatScorer_Draw(cChatScorer *self, int player);

/* ---- CircuitScorer.cpp ---- */

#define CCIRCUITSCORER_VFTABLE GAME_VFTABLE(cCircuitScorer_vftable)
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
SD_SIZE(cCircuitScorer, 0xc8);

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
SD_SIZE(cCircuitPath, 0x308);

/* a flare on a path, then a pause */
typedef struct cCircuitFlare {
	float time;			/* 0x00 left of the run */
	float duration;			/* 0x04 */
	float delay;			/* 0x08 the pause before the next run */
	int path;			/* 0x0c */
	int reverse;			/* 0x10 */
	int unused_14;			/* 0x14 */
} cCircuitFlare;
SD_SIZE(cCircuitFlare, 0x18);

extern cCircuitFlare g_CircuitFlares[4];	/* CIRCUIT_FLARES */
#define g_CircuitInit GAME_GLOBAL_IN(byte, g_CircuitFlares, 92)
extern cCircuitPath g_CircuitPaths[9];	/* CIRCUIT_PATHS */

/* the constructor of a path in the static array: nothing */
cCircuitPath *cCircuitPath_ctor(cCircuitPath *self);
cCircuitScorer *cCircuitScorer_ctor(cCircuitScorer *self);
void cCircuitScorer_dtor(cCircuitScorer *self);
cCircuitScorer *cCircuitScorer_Delete(cCircuitScorer *self, byte flags);
/* the points of the paths, their lengths, the flares stopped with their first delays (once) */
void CircuitScorer_InitPaths(void);
void cCircuitScorer_SetCircuitFile(cCircuitScorer *self, uint32_t flags, const char *name);
/* the flare sheet; the white default without the 0x40 flag */
void cCircuitScorer_SetFlareFile(cCircuitScorer *self, uint32_t flags, const char *name);
/* ScaleX, ScaleY, FileCircuit and FileFlare (set), HighRes, BlendingMode, AlphaCircuit, AlphaFlare */
int cCircuitScorer_Var(cCircuitScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cCircuitScorer_Draw(cCircuitScorer *self, int player);

/* ---- CopMeterScorer.cpp ---- */

#define CCOPMETERSCORER_VFTABLE GAME_VFTABLE(cCopMeterScorer_vftable)

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
SD_SIZE(cCopMeterScorer, 0xf0);

cCopMeterScorer *cCopMeterScorer_ctor(cCopMeterScorer *self);
void cCopMeterScorer_dtor(cCopMeterScorer *self);
cCopMeterScorer *cCopMeterScorer_Delete(cCopMeterScorer *self, byte flags);
/* Sprite (set), Percent, Target (0..100), BackIndex, BackOffX/Y, LightIndex, LightOffX/Y, BarOffX/Y, BarIndex */
int cCopMeterScorer_Var(cCopMeterScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cCopMeterScorer_Draw(cCopMeterScorer *self, int player);

/* ---- HangarShipScorer.cpp ---- */

#define CHANGARSHIPSCORER_VFTABLE GAME_VFTABLE(cHangarShipScorer_vftable)

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
SD_SIZE(cHangarShipScorer, 0xf0);

cHangarShipScorer *cHangarShipScorer_ctor(cHangarShipScorer *self);
void cHangarShipScorer_dtor(cHangarShipScorer *self);
cHangarShipScorer *cHangarShipScorer_Delete(cHangarShipScorer *self, byte flags);
/* GridSpriteIndex, GridColorRed/Green/Blue/Alpha, TargetName, CamDist, CamFov */
int cHangarShipScorer_Var(cHangarShipScorer *self, const char *name, byte set, char **s, double *d,
	int *i);
void cHangarShipScorer_Draw(cHangarShipScorer *self, int player);

/* ---- the models of the scorer (Scorer.cpp, SScorer.AddModel) ---- */

typedef struct cScorerModel {
	char name[0x20];		/* 0x00 */
	CGameModel model;		/* 0x20 */
	int numAnims;			/* 0x3c */
	int anims[8];			/* 0x40 */
	struct cScorerModel *next;	/* 0x60 */
	float radius;			/* 0x64 */
	float height;			/* 0x68 the eyes */
	void *extra;			/* 0x6c drawn after the model through its slot 0x10 */
} cScorerModel;
SD_OFFSET(cScorerModel, numAnims, 0x3c);
SD_OFFSET(cScorerModel, extra, 0x6c);

/* the model of that name of the scorer, NULL if none */
cScorerModel *cScorer_FindModel(cScorer *scorer, const char *name);

/* ---- HeadScorer.cpp ---- */

#define CHEADSCORER_VFTABLE GAME_VFTABLE(cHeadScorer_vftable)

/* a talking head: a scorer model in a view of its own, the camera swaying (0xc4 bytes) */
typedef struct cHeadScorer {
	cScorerItem base;		/* 0x00 */
	float time;			/* 0xac the sway, random start */
	cScorerModel *model;		/* 0xb0 */
	int anim;			/* 0xb4 1 */
	CGameCamera *cam;		/* 0xb8 */
	float vw, vh;			/* 0xbc the view size */
} cHeadScorer;
SD_SIZE(cHeadScorer, 0xc4);

/* the head slots of the vtables of cHeadScorer and cMonitorScorer */
#define HeadScorer_SetAnim(self, a) \
	((void(*)(void *, int))((const cScorerItemVtbl *)(self)->base.vftable)->vf04)((self), (a))
#define HeadScorer_SetModel(self, name) \
	((byte(*)(void *, const char *))((const cScorerItemVtbl *)(self)->base.vftable)->vf05)((self), (name))

cHeadScorer *cHeadScorer_ctor(cHeadScorer *self);
void cHeadScorer_dtor(cHeadScorer *self);
cHeadScorer *cHeadScorer_Delete(cHeadScorer *self, byte flags);
/* HeadModel (set), HeadAnimIndex, VW, VH */
int cHeadScorer_Var(cHeadScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cHeadScorer_Draw(cHeadScorer *self, int player);
/* vf05: the model of that name of the scorer of the item, its animation set again; 1 if found */
byte cHeadScorer_SetModel(cHeadScorer *self, const char *name);
/* vf04: the animation (an index of the model's list) from its start, looped */
void cHeadScorer_SetAnim(cHeadScorer *self, int anim);

/* ---- MissionScorer.cpp ---- */

#define CMISSIONSCORER_VFTABLE GAME_VFTABLE(cMissionScorer_vftable)
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
SD_SIZE(cMissionScorer, 0xec);

extern int g_MissionBlinkOn;
extern float g_MissionBlinkTime;	/* BlinkTime: the icons blink until 0 */
extern float g_MissionBlinkLast;	/* the blink time of the last toggle */
extern int g_MissionStatus[3];		/* Status0..2 */
extern float g_MissionStatusTime[4];	/* the scorer time each was set */
#define g_MissionMapName GAME_GLOBAL_IN(const char *, g_MissionStatusTime, 12)	/* the language text of the station */
extern cGVar GVar_ScorerMarginH;
#define g_ScorerMarginH (GVar_ScorerMarginH.value.i)		/* ScorerMarginH */
extern cGVar GVar_NextMissionFirstLineOffsetXBox;
#define g_NextMissionLine1XBox (GVar_NextMissionFirstLineOffsetXBox.value.f)	/* NextMissionFirstLineOffsetXBox */
extern cGVar GVar_NextMissionSecondLineOffsetXBox;
#define g_NextMissionLine2XBox (GVar_NextMissionSecondLineOffsetXBox.value.f)	/* NextMissionSecondLineOffsetXBox */
extern cGVar GVar_NextMissionFirstLineOffsetPC;
#define g_NextMissionLine1PC (GVar_NextMissionFirstLineOffsetPC.value.f)	/* NextMissionFirstLineOffsetPC */
extern cGVar GVar_NextMissionSecondLineOffsetPC;
#define g_NextMissionLine2PC (GVar_NextMissionSecondLineOffsetPC.value.f)	/* NextMissionSecondLineOffsetPC */
#define g_MissionStatusColors (GAME_ARRAY(const uint32_t, g_MissionStatusColors))
#define g_MissionStatusOfs (GAME_ARRAY(const float[2], g_MissionStatusOfs))

cMissionScorer *cMissionScorer_ctor(cMissionScorer *self);
void cMissionScorer_dtor(cMissionScorer *self);
cMissionScorer *cMissionScorer_Delete(cMissionScorer *self, byte flags);
/* ForceMap (set), BackIndex, PrimaryIndex, SecondaryIndex, BlinkTime, SolidTime, Status0..2, LastStatus (get),
 * StatusClear (set), TabSprite (set), TabBlendingMode, TabSpriteIndex, TabRed/Green/Blue/Alpha, TextRed/Green/Blue */
int cMissionScorer_Var(cMissionScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cMissionScorer_Draw(cMissionScorer *self, int player);

/* ---- MonitorScorer.cpp ---- */

#define CMONITORSCORER_VFTABLE GAME_VFTABLE(cMonitorScorer_vftable)
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
SD_SIZE(cMonitorScorer, 0x354);
SD_OFFSET(cMonitorScorer, model, 0x208);
SD_OFFSET(cMonitorScorer, objName, 0x224);
SD_OFFSET(cMonitorScorer, objWas, 0x2a8);
SD_OFFSET(cMonitorScorer, weapon, 0x2ac);

/* the head and text of Scorer_SetHeadMonitor */
extern char g_HeadMonitorName[128];	/* 0x80, the scorer model */
extern char g_HeadMonitorText[128];	/* 0x80 */
extern int g_HeadMonitorAnim;	/* the animation, negative: with noise */
extern cGVar GVar_FirstPhotoMessage;
#define g_FirstPhotoMessage (GVar_FirstPhotoMessage.value.s)	/* FirstPhotoMessage */

cMonitorScorer *cMonitorScorer_ctor(cMonitorScorer *self);
void cMonitorScorer_dtor(cMonitorScorer *self);
cMonitorScorer *cMonitorScorer_Delete(cMonitorScorer *self, byte flags);
/* the monitor of the target of the camera of player: the head of the talk or the view of the entity it looks at
 * with its text, the noise, the objective mark of a character */
void cMonitorScorer_DrawMonitor(cMonitorScorer *self, int player, cEntity *target);
/* vf01: TargetView=1 */
void cMonitorScorer_SetParam(cMonitorScorer *self, const char *name, const char *value);
int cMonitorScorer_Var(cMonitorScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cMonitorScorer_Draw(cMonitorScorer *self, int player);
byte cMonitorScorer_SetModel(cMonitorScorer *self, const char *name);
void cMonitorScorer_SetAnim(cMonitorScorer *self, int anim);
/* the message FirstPhotoMessage again when the message text ended */
void Monitor_RepeatPhotoMessage(void);

/* ---- NetStatsScorer.cpp ---- */

#define CNETSTATSSCORER_VFTABLE GAME_VFTABLE(cNetStatsScorer_vftable)
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
SD_SIZE(cNetStatsScorer, 0x230);

extern cGVar GVar_ClientPingX;
#define g_ClientPingX (GVar_ClientPingX.value.f)	/* ClientPingX */
extern cGVar GVar_ClientPingY;
#define g_ClientPingY (GVar_ClientPingY.value.f)	/* ClientPingY */
extern cGVar GVar_ShowClientPing;
#define g_ShowClientPing (GVar_ShowClientPing.value.i)	/* ShowClientPing */
extern cGVar GVar_ShowNetStatus;
#define g_ShowNetStatus (GVar_ShowNetStatus.value.i)	/* ShowNetStatus */
/* the counters of the net (Net/) for the stats: bytes and slot of the last send and receive */
extern int g_NetStatSent;
extern int g_NetStatSentSlot;
extern int g_NetStatRecv;
extern int g_NetStatRecvSlot;

cNetStatsScorer *cNetStatsScorer_ctor(cNetStatsScorer *self);
void cNetStatsScorer_dtor(cNetStatsScorer *self);
cNetStatsScorer *cNetStatsScorer_Delete(cNetStatsScorer *self, byte flags);
/* only the base variables */
int cNetStatsScorer_Var(cNetStatsScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cNetStatsScorer_Draw(cNetStatsScorer *self, int player);

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

extern cGVar GVar_NewsPanelActive;
#define g_NewsPanelActive (GVar_NewsPanelActive.value.i)		/* NewsPanelActive */
#define g_NewsPanelActiveDef (GVar_NewsPanelActive.def.i)	/* its default */
extern cGVar GVar_NewsPanelShowHead;
#define g_NewsPanelShowHead (GVar_NewsPanelShowHead.value.i)	/* NewsPanelShowHead */
#define g_NewsPanelShowHeadDef (GVar_NewsPanelShowHead.def.i)
extern cGVar GVar_NewsPanelOpenSpeed;
#define g_NewsPanelOpenSpeed (GVar_NewsPanelOpenSpeed.value.f)	/* NewsPanelOpenSpeed */
extern cGVar GVar_NewsPanelCloseSpeed;
#define g_NewsPanelCloseSpeed (GVar_NewsPanelCloseSpeed.value.f)	/* NewsPanelCloseSpeed */
#define g_NewsHead (&GAME_GLOBAL_IN(char *, GVar_NewsHead, 16))		/* NewsHead, a string variable */
#define g_NewsLogoBmp (&GAME_GLOBAL_IN(char *, GVar_NewsLogoBmp, 16))		/* NewsLogoBmp */
#define g_BackNewsBmp (&GAME_GLOBAL_IN(char *, GVar_BackNewsBmp, 16))		/* BackNewsBmp */
extern cGVar GVar_NewsPanelAspect;
#define g_NewsPanelAspect (GVar_NewsPanelAspect.value.f)	/* NewsPanelAspect */
extern cGVar GVar_NewsPanelMove;
#define g_NewsPanelMove (GVar_NewsPanelMove.value.f)		/* NewsPanelMove */
extern cGVar GVar_NewsPanelDisplacement;
#define g_NewsPanelDisplacement (GVar_NewsPanelDisplacement.value.f)	/* NewsPanelDisplacement */
extern cGVar GVar_NewsPanelDistance;
#define g_NewsPanelDistance (GVar_NewsPanelDistance.value.f)	/* NewsPanelDistance */
extern cGVar GVar_NewsPanelW;
#define g_NewsPanelW (GVar_NewsPanelW.value.f)		/* NewsPanelW */
extern cGVar GVar_NewsPanelH;
#define g_NewsPanelH (GVar_NewsPanelH.value.f)		/* NewsPanelH */
extern cGVar GVar_NewsPanelX;
#define g_NewsPanelX (GVar_NewsPanelX.value.f)		/* NewsPanelX */
extern cGVar GVar_NewsPanelY;
#define g_NewsPanelY (GVar_NewsPanelY.value.f)		/* NewsPanelY */

cNewsPanel *cNewsPanel_ctor(cNewsPanel *self);
void cNewsPanel_dtor(cNewsPanel *self);
/* the first entity of the watched list 0, 1 or 2 of the world */
cEntity *cNewsPanel_GetWatched(cNewsPanel *self, int list);
/* the switches back to their defaults, the images and the head dropped */
void cNewsPanel_Reset(cNewsPanel *self);
/* the texture of the frame, when the scorer time moved */
void cNewsPanel_Update(cNewsPanel *self);
/* the view of an entity from a point around it */
void cNewsPanel_DrawEntity(cNewsPanel *self, cEntity *e);
/* the news: the head opening between the background and the logo */
void cNewsPanel_DrawNews(cNewsPanel *self, float dt);

/* ---- RadarScorer.cpp ---- */

#define CRADARSCORER_VFTABLE GAME_VFTABLE(cRadarScorer_vftable)
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
SD_SIZE(cRadarScorer, 0x94c);
SD_OFFSET(cRadarScorer, noise, 0x8c4);
SD_OFFSET(cRadarScorer, map2dScale, 0x91c);

/* a floor of the world (0x18c bytes from world + 0x2f4, their count at 0x2f0): its lowest height first */
/* the view of the radar: the turn and center of the map, its scale */
extern float g_RadarAngle;
extern float g_RadarCenterX;
extern float g_RadarCenterZ;
extern float g_RadarScale;
extern cGVar GVar_FileNameMap2D;
#define g_FileNameMap2D (GVar_FileNameMap2D.value.s)	/* FileNameMap2D, a printf format of the floor */

cRadarScorer *cRadarScorer_ctor(cRadarScorer *self);
void cRadarScorer_dtor(cRadarScorer *self);
/* the Global/Table.h lookups of the input actions compiled here */
cInputAction *InputActionTable_At(void *table, uint32_t i);
cActionSet *ActionSetTable_At(void *table, uint32_t i);
cRadarScorer *cRadarScorer_Delete(cRadarScorer *self, byte flags);
/* the floor of height y; the fade starts when it changed */
void cRadarScorer_UpdateFloor(cRadarScorer *self, float y);
/* the radar: its map, the view cones, the places, doors, characters and vehicles, the target and the mission
 * objects */
void cRadarScorer_DrawRadar(cRadarScorer *self, cEntity *target, cEntity *cam);
/* the view of the radar at the target and its map (the start of DrawRadar) */
void cRadarScorer_DrawMap(cRadarScorer *self, cEntity *target, cEntity *cam);
/* the icon of an entity at pos (flags RADAR_ICON_OUT_*) */
void cRadarScorer_DrawIcon(cRadarScorer *self, const vec3_t *pos, int icon, float ang, int outIcon,
	uint32_t flags, uint32_t color, uint32_t outColor, byte always);
/* a view cone at pos to the angle, spread wide, range long, mode 1..3 the color */
void cRadarScorer_DrawCone(cRadarScorer *self, const vec3_t *pos, float ang, float spread, float range,
	int mode);
/* y is on the floor shown before */
byte cRadarScorer_OnPrevFloor(cRadarScorer *self, float y);
/* y below the floor (-1), on it (0) or above it (1) */
int cRadarScorer_FloorSide(cRadarScorer *self, float y);
int cRadarScorer_Var(cRadarScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cRadarScorer_Draw(cRadarScorer *self, int player);


/* ---- Scorer.cpp (src/game/Scorer/Scorer.c) ---- */

#define SCORER_MAX_HEAD_ANIMS 8

/* the flags and the default of an engine variable by its value (cGVar of include/scrap/scrap.h) */
#define GVAR_FLAGS(v) (*((byte *)&(v) - 4))
#define GVAR_DEF(type, v) (*(type *)((byte *)&(v) + 4))

extern cScorer *g_Scorers;		/* new cScorer[4] */
extern CPsaImage *g_ScorerWhite;	/* 2D/white.tga */
extern CPsaImage *g_ScorerNoise;	/* 2D/Noise.tga */
extern CPsaImage *g_ScorerMarker;	/* the focus marker (SScorer.SetMarkerSprite) */
extern CPsaImage *g_ScorerTabs;	/* 2D/Menu/Tabs.alpha.tga */
extern cNewsPanel *g_NewsPanel;
/* the engine variables of Scorer.cpp */
extern cGVar GVar_RadarMMSelect;
#define g_RadarMMSelect (GVar_RadarMMSelect.value.i)		/* RadarMMSelect */
extern cGVar GVar_NumEnemiesInRadar;
#define g_NumEnemiesInRadar (GVar_NumEnemiesInRadar.value.i)
extern cGVar GVar_CinemaBlackBandHeight;
#define g_CinemaBlackBandHeight (GVar_CinemaBlackBandHeight.value.f)	/* CinemaBlackBandHeight */
extern cGVar GVar_NoiseFactor;
#define g_NoiseFactor (GVar_NoiseFactor.value.i)		/* NoiseFactor */
extern cGVar GVar_NoiseFactorBegin;
#define g_NoiseFactorBegin (GVar_NoiseFactorBegin.value.i)	/* NoiseFactorBegin */
extern cGVar GVar_NoiseTime;
#define g_NoiseTime (GVar_NoiseTime.value.f)		/* NoiseTime */
extern cGVar GVar_NoiseEndTime;
#define g_NoiseEndTime (GVar_NoiseEndTime.value.f)		/* NoiseEndTime */
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
#define g_FullScreenConsoleCallback (&GAME_GLOBAL_IN(cPy_Function, GVar_FullScreenConsoleCallback, 16))	/* FullScreenConsoleCallback */
extern cGVar GVar_CurrentWaypointName;
#define g_CurrentWaypointName (GVar_CurrentWaypointName.value.s)	/* CurrentWaypointName */
extern cGVar GVar_CurrentWaypointName2;
#define g_CurrentWaypointName2 (GVar_CurrentWaypointName2.value.s)	/* CurrentWaypointName2 */
extern cGVar GVar_CameraCloudDistance;
#define g_CameraCloudDistance (GVar_CameraCloudDistance.value.f)	/* CameraCloudDistance */
extern cGVar GVar_CameraCloudPlaneSize;
#define g_CameraCloudPlaneSize (GVar_CameraCloudPlaneSize.value.f)	/* CameraCloudPlaneSize */
extern cGVar GVar_CameraCloudThickness;
#define g_CameraCloudThickness (GVar_CameraCloudThickness.value.f)	/* CameraCloudThickness */
extern cGVar GVar_CameraCloudAlpha;
#define g_CameraCloudAlpha (GVar_CameraCloudAlpha.value.i)	/* CameraCloudAlpha */
extern cGVar GVar_CameraCloudRed;
#define g_CameraCloudRed (GVar_CameraCloudRed.value.i)		/* CameraCloudRed */
extern cGVar GVar_CameraCloudGreen;
#define g_CameraCloudGreen (GVar_CameraCloudGreen.value.i)	/* CameraCloudGreen */
extern cGVar GVar_CameraCloudBlue;
#define g_CameraCloudBlue (GVar_CameraCloudBlue.value.i)		/* CameraCloudBlue */
extern cGVar GVar_OnAcceptSound;
#define g_OnAcceptSound (GVar_OnAcceptSound.value.s)	/* OnAcceptSound */
extern cGVar GVar_OnAcceptSoundVol;
#define g_OnAcceptSoundVol (GVar_OnAcceptSoundVol.value.f)	/* OnAcceptSoundVol */
extern cGVar GVar_OnFocusSound;
#define g_OnFocusSound (GVar_OnFocusSound.value.s)		/* OnFocusSound */
extern cGVar GVar_OnFocusSoundVol;
#define g_OnFocusSoundVol (GVar_OnFocusSoundVol.value.f)	/* OnFocusSoundVol */
extern cGVar GVar_ScorerMarker;
#define g_ScorerMarkerOn (GVar_ScorerMarker.value.i)		/* ScorerMarker */
#define g_ScorerMarkerColor GAME_GLOBAL_IN(uint32_t, GVar_ScorerMarkerColor, 16)	/* ScorerMarkerColor */
extern cGVar GVar_ScorerMarkerSpeed;
#define g_ScorerMarkerSpeed (GVar_ScorerMarkerSpeed.value.f)	/* ScorerMarkerSpeed */
extern cGVar GVar_CShow;
#define g_CShow (GVar_CShow.value.i)			/* CShow */
extern cGVar GVar_XboxNoPadMsg;
#define g_XboxNoPadMsg (GVar_XboxNoPadMsg.value.i)		/* XboxNoPadMsg */
extern cGVar GVar_XboxNoPadMsgP;
#define g_XboxNoPadMsgP (GVar_XboxNoPadMsgP.value.i)		/* XboxNoPadMsgP */
extern cGVar GVar_XboxNoPadMsgPBig;
#define g_XboxNoPadMsgPBig (GVar_XboxNoPadMsgPBig.value.i)	/* XboxNoPadMsgPBig */
extern cGVar GVar_XboxSavingMsg;
#define g_XboxSavingMsg (GVar_XboxSavingMsg.value.f)		/* XboxSavingMsg */
extern cGVar GVar_XboxSavingMsgStr;
#define g_XboxSavingMsgStr (GVar_XboxSavingMsgStr.value.s)	/* XboxSavingMsgStr */
extern cGVar GVar_ShowConsoleLog;
#define g_ShowConsoleLog (GVar_ShowConsoleLog.value.i)		/* ShowConsoleLog */
#define g_ScorerRandScale GAME_GLOBAL(double, g_FxRandScale)	/* 1/32767 */

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
	s->vftable->Play(s, snd, Snd_Volume(vol), 0);
}

ScorerCinema *ScorerCinema_ctor(ScorerCinema *self);
ScorerFade *ScorerFade_ctor(ScorerFade *self);
void *ScorerFx_ctor(void *self);
cScorer *cScorer_ctor(cScorer *self);
void cScorer_dtor(cScorer *self);
cScorer *cScorer_Delete(cScorer *self, byte flags);
cScorerItem *cScorerItem_Delete(cScorerItem *self, byte flags);
/* the scorer model of that name: its game model, NULL if none */
CGameModel *cScorer_FindModelModel(cScorer *scorer, const char *name);
void cScorer_Free(cScorer *scorer);
void cScorer_SetParam(cScorer *scorer, const char *name, const char *value);
void cScorer_RemoveDeleted(cScorer *scorer, int player);
void cScorer_SetCursorItem(cScorer *scorer, int player, const char *from, const char *to);
void Scorer_SetInputState(int state);
CPsaImage *Scorer_GetWhite(void);
CPsaImage *Scorer_GetMarker(void);
CPsaImage *Scorer_GetNoise(void);
void Scorer_GetCamVarStr(int player, char *buf, const char *name, int size);
int Scorer_GetCamVarInt(int player, const char *name);
void Scorer_DrawPyMemStats(void);
uint32_t Scorer_ColorLerp(uint32_t a, uint32_t b, float t);
void ScorerFade_Draw(ScorerFade *fade, int player, float dt);
void ScorerCinema_Draw(ScorerCinema *cinema, int player, float dt);
void ScorerFx_Draw(void *fx, int player, float dt);
void cScorer_UpdateMP(cScorer *scorer, float dt);
void cScorer_UpdateCursor(cScorer *scorer, int player);
void cScorer_Draw(cScorer *scorer, int player);
void cScorer_DrawClouds(cScorer *scorer, int player);
/* the big message: the start text; else the pad text of player index (pad the player number in split screen) */
void cScorer_DrawNoPadMsg(cScorer *scorer, int index, float y, byte big, int pad);
void cScorer_DrawXboxMsgs(cScorer *scorer, int player);
byte Scorer_Render(void);
/* the full screen console (the ring of the console lines sliding in and out, the input line), else the console
 * log of ShowConsoleLog and the console key: 1 while it is shown (instead of the scorer) */
byte Scorer_DrawConsole(void);
/* CShow (1 the grid, 2 the entities, 3 the sounds, 4 and 6 the AI, 5 the Python memory, 7 the ships), the dummies
 * and the AI drawings of NWAnalizeOn */
void Scorer_DrawDebug(void);
void Console_OnInput(int player, const char *text);
void Console_HistoryUp(void);
void Console_HistoryDown(void);

/* the image slots of the items below: the old image deleted, the new one made with flags, color white */
static inline CPsaImage *ScorerImg_Replace(CPsaImage *old, const char *name, int blend, uint32_t flags) {
	if (old) {
		CPsaImage_dtor(old);
		operator_delete_sized(old, sizeof(CPsaImage));
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
		operator_delete_sized(old, sizeof(CPsaImage));
	}
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctor(img, name, (int)flags) : NULL;
	img->color = 0xffffffff;
	return img;
}

static inline void ScorerImg_Free(CPsaImage *img) {
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, sizeof(CPsaImage));
	}
}

/* ---- ScraplandScorer.cpp: the animated logos of the main menu and of the sub menus ---- */

#define CSCRAPLANDMAINMENUSCORER_VFTABLE GAME_VFTABLE(cScraplandMainMenuScorer_vftable)
#define CSCRAPLANDSUBMENUSCORER_VFTABLE GAME_VFTABLE(cScraplandSubMenuScorer_vftable)

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
SD_SIZE(cScraplandMainMenuScorer, 0xc8);

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
SD_SIZE(cScraplandSubMenuScorer, 0xd8);

/* the global animation state, set up once by the first constructor (random times) */
#define g_MainMenuAnimInit GAME_GLOBAL_IN(byte, group_460428, 0)
#define g_SubMenuAnimInit GAME_GLOBAL_IN(byte, group_460428, 1)
extern float g_MainMenuWait[9];		/* [9] */
extern float g_MainMenuTime[9];		/* [9] */
extern float g_MainMenuLength[9];	/* [9] */
extern byte g_MainMenuSparkDir[19];	/* [19] */
extern float g_MainMenuSparkWait[19];	/* [19] */
extern float g_MainMenuSparkTime[19];	/* [19] */
extern float g_MainMenuSparkLength[19];	/* [19] */
extern int g_MainMenuSparkFrame[19];	/* [19] */
extern float g_MainMenuNeonNext;
extern float g_MainMenuNeonGroup[3];	/* [3] */
extern float g_MainMenuNeonWait[21];	/* [21] */
extern float g_SubMenuWait;
extern float g_SubMenuRingTime;
extern float g_SubMenuRingWait;
extern int g_SubMenuRingDir;		/* 1: the ring frames run backwards */
extern int g_SubMenuRingFrame;	/* the first frame of the ring */

cScraplandMainMenuScorer *cScraplandMainMenuScorer_ctor(cScraplandMainMenuScorer *self);
cScraplandSubMenuScorer *cScraplandSubMenuScorer_ctor(cScraplandSubMenuScorer *self);
void cScraplandMainMenuScorer_dtor(cScraplandMainMenuScorer *self);
void cScraplandSubMenuScorer_dtor(cScraplandSubMenuScorer *self);
cScraplandMainMenuScorer *cScraplandMainMenuScorer_Delete(cScraplandMainMenuScorer *self, byte flags);
cScraplandSubMenuScorer *cScraplandSubMenuScorer_Delete(cScraplandSubMenuScorer *self, byte flags);
/* the time of the item and, once, the random animation state */
void cScraplandMainMenuScorer_Init(cScraplandMainMenuScorer *self);
void cScraplandSubMenuScorer_Init(cScraplandSubMenuScorer *self);
/* the image setters (flags, file) */
void cScraplandMainMenuScorer_SetGlow(cScraplandMainMenuScorer *self, uint32_t flags, const char *file);
void cScraplandSubMenuScorer_SetGlow(cScraplandSubMenuScorer *self, uint32_t flags, const char *file);
void cScraplandSubMenuScorer_Draw(cScraplandSubMenuScorer *self, int player);
/* vf00: the logo, the neon tubes, the letters with their lighting, the sparks and "Remastered" */
void cScraplandMainMenuScorer_Draw(cScraplandMainMenuScorer *self, int player);
/* 2D/white.tga loads without the flag 0x40, any other file with it */
void cScraplandMainMenuScorer_SetLighting(cScraplandMainMenuScorer *self, uint32_t flags,
	const char *file);
void cScraplandSubMenuScorer_SetLighting(cScraplandSubMenuScorer *self, uint32_t flags, const char *file);
void cScraplandMainMenuScorer_SetLogo(cScraplandMainMenuScorer *self, uint32_t flags, const char *file);
void cScraplandSubMenuScorer_SetLogo(cScraplandSubMenuScorer *self, uint32_t flags, const char *file);
void cScraplandMainMenuScorer_SetNeon(cScraplandMainMenuScorer *self, uint32_t flags, const char *file);
void cScraplandSubMenuScorer_SetRingBorder(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file);
void cScraplandSubMenuScorer_SetRingMask(cScraplandSubMenuScorer *self, uint32_t flags, const char *file);
void cScraplandSubMenuScorer_SetRingReflection(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file);
/* HighRes; set only: FileLogo, FileGlow, FileNeon, FileLighting */
int cScraplandMainMenuScorer_Var(cScraplandMainMenuScorer *self, const char *name, byte set, char **s,
	double *d, int *i);
/* set only: FileLogo, FileGlow, FileLighting, FileRingMask, FileRingReflection, FileRingBorder; HighRes, Scale */
int cScraplandSubMenuScorer_Var(cScraplandSubMenuScorer *self, const char *name, byte set, char **s,
	double *d, int *i);

/* ---- SliderScorer.cpp ---- */

#define CSLIDERSCORER_VFTABLE GAME_VFTABLE(cSliderScorer_vftable)

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
SD_SIZE(cSliderScorer, 0xf8);

cSliderScorer *cSliderScorer_ctor(cSliderScorer *self);
void cSliderScorer_dtor(cSliderScorer *self);
cSliderScorer *cSliderScorer_Delete(cSliderScorer *self, byte flags);
void cSliderScorer_SetFile(cSliderScorer *self, uint32_t flags, const char *file);
int cSliderScorer_Var(cSliderScorer *self, const char *name, byte set, char **s, double *d, int *i);
void cSliderScorer_Draw(cSliderScorer *self, int player);

/* ---- SpecialActionScorer.cpp ---- */

#define CSPECIALACTIONSCORER_VFTABLE GAME_VFTABLE(cSpecialActionScorer_vftable)
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
SD_SIZE(cSpecialActionScorer, 0x138);
SD_OFFSET(cSpecialActionScorer, textFont, 0xf0);
SD_OFFSET(cSpecialActionScorer, iconIndex, 0x11c);
SD_OFFSET(cSpecialActionScorer, energyBackColor, 0x130);

extern cGVar GVar_SpecialActionActive;
#define g_SpecialActionActive (GVar_SpecialActionActive.value.i)	/* SpecialActionActive 0 off, 1 blink, 2 on */

cSpecialActionScorer *cSpecialActionScorer_ctor(cSpecialActionScorer *self);
void cSpecialActionScorer_dtor(cSpecialActionScorer *self);
cSpecialActionScorer *cSpecialActionScorer_Delete(cSpecialActionScorer *self, byte flags);
/* TextFont returns 0 when the font exists (and 1 when not), the original does so */
int cSpecialActionScorer_Var(cSpecialActionScorer *self, const char *name, byte set, char **s, double *d,
	int *i);
void cSpecialActionScorer_Draw(cSpecialActionScorer *self, int player);
void cSpecialActionScorer_DrawIcon(cSpecialActionScorer *self, int player, int state);
void cSpecialActionScorer_DrawEnergy(cSpecialActionScorer *self);
void cSpecialActionScorer_DrawCrossHair(cSpecialActionScorer *self, int player);
void cSpecialActionScorer_DrawCannon(cSpecialActionScorer *self, int player, const float *pos,
	const float *size, float ang, uint32_t color);
/* the target search of the gear and its aim, the sphere callback of the search */
void cSpecialActionScorer_DrawGearAim(cSpecialActionScorer *self, int player);
void GearAim_SphereFunc(cAbstractFile *params);
void SpecialAction_DrawDefault(cSpecialActionScorer *self, int player);
void SpecialAction_DrawBanker(cSpecialActionScorer *self, int player);
void SpecialAction_DrawTarget(cSpecialActionScorer *self, int player);
void SpecialAction_DrawGear(cSpecialActionScorer *self, int player);
void SpecialAction_DrawGear2(cSpecialActionScorer *self, int player);
void SpecialAction_DrawGear3(cSpecialActionScorer *self, int player);
void SpecialAction_DrawVictim(cSpecialActionScorer *self, int player);
void SpecialAction_DrawOn(cSpecialActionScorer *self, int player);
void SpecialAction_DrawLowEnergy(cSpecialActionScorer *self, int player);
void SpecialAction_DrawFunctionary(cSpecialActionScorer *self, int player);
void SpecialAction_DrawEnergyOnly(cSpecialActionScorer *self, int player);

extern cGVar GVar_EnergyBarActive;
#define g_EnergyBarActive (GVar_EnergyBarActive.value.i)		/* EnergyBarActive */
extern cGVar GVar_FunctionarySpecialActionLow;
#define g_FunctionarySpecialActionLow (GVar_FunctionarySpecialActionLow.value.f)	/* FunctionarySpecialActionLow */

/* the exe functions the special action draws use (other modules) */
/* frame + 1 stretched to the width w, frame at both ends (the right one mirrored), h the y scale */
void Menu_DrawStretch(CPsaImage *img, int frame, float x, float y, float w, float h);

#endif
