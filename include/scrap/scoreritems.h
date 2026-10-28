/*
 * The HUD and menu items of Menu/ from AbstractTextScorer.cpp to RadarScorer.cpp (the classes over cScorerItem of
 * include/scrap/scorer.h). Every class has Draw(player) at vf00, Var at vf02 and the deleting destructor at vf03;
 * the constructors and destructors are fastcall, the methods thiscall.
 */
#ifndef SCRAP_SCORERITEMS_H
#define SCRAP_SCORERITEMS_H

#include "scrap/app.h"
#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/input.h"
#include "scrap/scorer.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapmodel.h"
#include "scrap/sound.h"

/* the vtables of the exe */
#define CABSTRACTTEXTSCORER_VFTABLE 0x3c1b4c
#define CMSGSCORER_VFTABLE 0x3c1b60
#define CLABELSCORER_VFTABLE 0x3c1b74
#define CHINTSCORER_VFTABLE 0x3c1b88
#define CCREDITSSCORER_VFTABLE 0x3c1b9c
#define CBARSCORER_VFTABLE 0x3c1de4

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

cSpriteScorer *__fastcall cSpriteScorer_ctor(cSpriteScorer *self);
void __fastcall cSpriteScorer_dtor(cSpriteScorer *self);
int __thiscall cSpriteScorer_Var(cSpriteScorer *self, const char *name, byte set, char **s, double *d, int *i);

/* ---- AbstractTextScorer.cpp ---- */

/* cAbstractTextScorer.align */
#define TEXTSCORER_LEFT 0
#define TEXTSCORER_RIGHT 1
#define TEXTSCORER_CENTER 4
/* cAbstractTextScorer.effect (the CFont efectos) */
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
#define g_CreditsX GAME_DATA(int, 0x4a8b7c)		/* R_CreditsX */
#define g_CreditsSizeX GAME_DATA(int, 0x4a8be4)		/* R_CreditsSizeX */
#define g_CreditsY GAME_DATA(int, 0x4a8c2c)		/* R_CreditsY */
#define g_CreditsAlpha GAME_DATA(int, 0x4a8d54)		/* R_CreditsAlpha */
#define g_CreditsClear GAME_DATA(int, 0x4a8d04)		/* R_CreditsClear */
#define g_CreditsPhase GAME_DATA(int, 0x4a8aa4)		/* R_CreditsPhase */
#define g_HintAppearsTime GAME_DATA(float, 0x4600d0)	/* HintAppearsTime */
#define g_MissionScorerName GAME_DATA(char *, 0x45d2a0)	/* MissionScorerName, the item hiding the label */
#define g_MissionSolidTime GAME_DATA(float, 0x45fa8c)	/* cMissionScorer SolidTime */
/* the label (Scorer_SetLabelText) */
#define g_LabelText ((char *)Game_Ptr(0x45d190))	/* 0xfa */
#define g_LabelTime GAME_DATA(float, 0x45d28c)		/* shown until this world time, always if <= 0 */
/* the message (Scorer_SetMsgText): the text and its lines */
#define g_MsgText ((char *)Game_Ptr(0x44dd18))		/* 0xf424 */
#define g_MsgNumLines GAME_DATA(int, 0x45d13c)
#define g_MsgLines ((char **)Game_Ptr(0x45d140))		/* 0xfa lines */
#define g_MsgTime GAME_DATA(float, 0x45d2b8)
/* the hint text and its two lines */
#define g_HintLines ((char **)Game_Ptr(0x45d2c0))
#define g_HintText ((char *)Game_Ptr(0x45d2c8))		/* 0x500 */

cAbstractTextScorer *__fastcall cAbstractTextScorer_ctor(cAbstractTextScorer *self);
cCreditsScorer *__fastcall cCreditsScorer_ctor(cCreditsScorer *self);
cHintScorer *__fastcall cHintScorer_ctor(cHintScorer *self);
cLabelScorer *__fastcall cLabelScorer_ctor(cLabelScorer *self);
cMsgScorer *__fastcall cMsgScorer_ctor(cMsgScorer *self);
void __fastcall cAbstractTextScorer_dtor(cAbstractTextScorer *self);
void __fastcall cCreditsScorer_dtor(cCreditsScorer *self);
void __fastcall cHintScorer_dtor(cHintScorer *self);
void __fastcall cLabelScorer_dtor(cLabelScorer *self);
cAbstractTextScorer *__thiscall cAbstractTextScorer_Delete(cAbstractTextScorer *self, byte flags);
cCreditsScorer *__thiscall cCreditsScorer_Delete(cCreditsScorer *self, byte flags);
cHintScorer *__thiscall cHintScorer_Delete(cHintScorer *self, byte flags);
cLabelScorer *__thiscall cLabelScorer_Delete(cLabelScorer *self, byte flags);
cMsgScorer *__thiscall cMsgScorer_Delete(cMsgScorer *self, byte flags);
/* the 20 lines at the position of the phase: fade 0 the scorer, 1 the render target */
void __thiscall cCreditsScorer_DrawLines(cCreditsScorer *self, byte fade, uint32_t color);
void __thiscall cScorerItem_SetParam(cScorerItem *self, const char *name, const char *value);
/* Text, Red, Green, Blue, Alpha; set only: Font, CentralText, GetExtens (1: the size of the text); Align, Effect */
int __thiscall cAbstractTextScorer_Var(cAbstractTextScorer *self, const char *name, byte set, char **s, double *d,
	int *i);
/* Time, Life, FadeIn, FadeOut; set only: Text0..Text19 */
int __thiscall cCreditsScorer_Var(cCreditsScorer *self, const char *name, byte set, char **s, double *d, int *i);
/* TabSprite (set), TabBlendingMode, TabSpriteIndex, TabRed, TabGreen, TabBlue, TabAlpha (also cLabelScorer) */
int __thiscall cHintScorer_Var(cHintScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cCreditsScorer_Draw(cCreditsScorer *self, int player);
void __thiscall cHintScorer_Draw(cHintScorer *self, int player);
void __thiscall cLabelScorer_Draw(cLabelScorer *self, int player);
void __thiscall cMsgScorer_Draw(cMsgScorer *self, int player);

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
#define g_BarValue GAME_DATA(float, 0x45d85c)		/* BarValue */
#define g_BarStopValue GAME_DATA(float, 0x45d844)	/* AutoUpdateStopValue */
#define g_BarSpeedFactor GAME_DATA(float, 0x45d82c)	/* AutoUpdateSpeedFactor */
#define g_BarAutoUpdate GAME_DATA(int, 0x45d814)	/* IsAutoUpdatable */
#define g_BarBlinkTime GAME_DATA(float, 0x45d7fc)	/* BlinkTime */
#define g_BarOnFinish (&GAME_DATA(cPy_Function, 0x45d7d8))	/* OnFinishAutoUpdate */

cBarScorer *__fastcall cBarScorer_ctor(cBarScorer *self);
void __fastcall cBarScorer_dtor(cBarScorer *self);
cBarScorer *__thiscall cBarScorer_Delete(cBarScorer *self, byte flags);
/* MaxValue, Value, AutoUpdateStopValue, AutoUpdateSpeedFactor, MainBarIndex, BlackBarIndex, IsAutoUpdatable,
 * Blink, BackIndex and the Red, Green, Blue, Alpha of Back, Main and Black */
int __thiscall cBarScorer_Var(cBarScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cBarScorer_Draw(cBarScorer *self, int player);

/* ---- ButtonScorer.cpp ---- */

#define CBUTTONSCORER_VFTABLE 0x3c1ebc
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
void __fastcall StrTable_Free(cStrTable *self);
void __fastcall ByteTable_Free(cByteTable *self);
char **__thiscall StrTable_At(cStrTable *self, uint32_t i);
void __thiscall StrTable_Init(cStrTable *self, int num);
/* the vector deleting destructor of cString: flags 2 an array of new[], 1 delete */
cString *__thiscall cString_DeleteVec(cString *self, uint32_t flags);

cButtonScorer *__fastcall cButtonScorer_ctor(cButtonScorer *self);
void __fastcall cButtonScorer_dtor(cButtonScorer *self);
cButtonScorer *__thiscall cButtonScorer_Delete(cButtonScorer *self, byte flags);
/* the text with a line break before every word past MaxLineSize + 75 pixels appended to *out; the lines */
int __thiscall cButtonScorer_WrapLines(cButtonScorer *self, char **out, const char *text);
/* the sheet of the highlight and border frames */
void __thiscall cButtonScorer_SetFile(cButtonScorer *self, uint32_t flags, const char *name);
/* the lines of the text (and of the name:value pairs) and the widest one; the lines measured */
int __thiscall cButtonScorer_SetText(cButtonScorer *self, const char *text);
int __thiscall cButtonScorer_Var(cButtonScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cButtonScorer_Draw(cButtonScorer *self, int player);

/* ---- CannonScorer.cpp ---- */

#define CCANNONSCORER_VFTABLE 0x3c2324
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

#define g_CannonNoAmmoSound GAME_DATA(char *, 0x45d874)	/* CannonScorerNoAmmoSound */
#define g_CannonNoAmmoSoundVol GAME_DATA(float, 0x45d8d4)	/* CannonScorerNoAmmoSoundVol */
#define g_WarningLifeLevel GAME_DATA(int, 0x4607b4)	/* WarningLifeLevel, percent */

cCannonScorer *__fastcall cCannonScorer_ctor(cCannonScorer *self);
void __fastcall cCannonScorer_dtor(cCannonScorer *self);
cCannonScorer *__thiscall cCannonScorer_Delete(cCannonScorer *self, byte flags);
/* the aim frame of weapon three times around pos, turned by angle */
void __thiscall cCannonScorer_DrawAim(cCannonScorer *self, int weapon, const float *pos, const float *scale,
	const float *unused, float angle, uint32_t color);
/* the aim of the weapon of the ship at the screen point of its auto aim */
void __thiscall cCannonScorer_DrawShipAim(cCannonScorer *self, int player, cEntity *ship);
/* the center sprite, the lock text and DANGER under the warning life */
void __thiscall cCannonScorer_DrawCenter(cCannonScorer *self, int player, cEntity *ship);
/* Sprite (set), CrossHairIndex, AimAc, AimIndex */
int __thiscall cCannonScorer_Var(cCannonScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cCannonScorer_Draw(cCannonScorer *self, int player);
/* vf04: "Searching..." or "Target Lock" above pos for the lock of the ship of the player */
void __thiscall cCannonScorer_DrawLock(cCannonScorer *self, int player, const float *pos);

/* ---- ChatScorer.cpp ---- */

#define CCHATSCORER_VFTABLE 0x3c2540
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

#define g_ChatMsgs ((cChatMsg *)Game_Ptr(0x45d940))	/* CHAT_MSGS */
#define g_ChatTime GAME_DATA(float, 0x45dec8)		/* of the last scroll */
#define g_ChatScrollTime GAME_DATA(float, 0x45d908)	/* an empty line comes in after this */

cChatScorer *__fastcall cChatScorer_ctor(cChatScorer *self);
void __fastcall cChatScorer_dtor(cChatScorer *self);
cChatScorer *__thiscall cChatScorer_Delete(cChatScorer *self, byte flags);
/* NumMsg */
int __thiscall cChatScorer_Var(cChatScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cChatScorer_Draw(cChatScorer *self, int player);

/* ---- CircuitScorer.cpp ---- */

#define CCIRCUITSCORER_VFTABLE 0x3c255c
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
	int i14;			/* 0x14 */
} cCircuitFlare;
SD_SIZE(cCircuitFlare, 0x18);

#define g_CircuitFlares ((cCircuitFlare *)Game_Ptr(0x45ded4))	/* CIRCUIT_FLARES */
#define g_CircuitInit GAME_DATA(byte, 0x45df30)
#define g_CircuitPaths ((cCircuitPath *)Game_Ptr(0x45df38))	/* CIRCUIT_PATHS */

/* the constructor of a path in the static array: nothing */
cCircuitPath *__fastcall cCircuitPath_ctor(cCircuitPath *self);
cCircuitScorer *__fastcall cCircuitScorer_ctor(cCircuitScorer *self);
void __fastcall cCircuitScorer_dtor(cCircuitScorer *self);
cCircuitScorer *__thiscall cCircuitScorer_Delete(cCircuitScorer *self, byte flags);
/* the points of the paths, their lengths, the flares stopped with their first delays (once) */
void CircuitScorer_InitPaths(void);
void __thiscall cCircuitScorer_SetCircuitFile(cCircuitScorer *self, uint32_t flags, const char *name);
/* the flare sheet; the white default without the 0x40 flag */
void __thiscall cCircuitScorer_SetFlareFile(cCircuitScorer *self, uint32_t flags, const char *name);
/* ScaleX, ScaleY, FileCircuit and FileFlare (set), HighRes, BlendingMode, AlphaCircuit, AlphaFlare */
int __thiscall cCircuitScorer_Var(cCircuitScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cCircuitScorer_Draw(cCircuitScorer *self, int player);

/* ---- CopMeterScorer.cpp ---- */

#define CCOPMETERSCORER_VFTABLE 0x3c2874

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
	int ie4;			/* 0xe4 */
	float fade;			/* 0xe8 0..1 */
	float blink;			/* 0xec 0..0.9 the blink phase */
} cCopMeterScorer;
SD_SIZE(cCopMeterScorer, 0xf0);

cCopMeterScorer *__fastcall cCopMeterScorer_ctor(cCopMeterScorer *self);
void __fastcall cCopMeterScorer_dtor(cCopMeterScorer *self);
cCopMeterScorer *__thiscall cCopMeterScorer_Delete(cCopMeterScorer *self, byte flags);
/* Sprite (set), Percent, Target (0..100), BackIndex, BackOffX/Y, LightIndex, LightOffX/Y, BarOffX/Y, BarIndex */
int __thiscall cCopMeterScorer_Var(cCopMeterScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cCopMeterScorer_Draw(cCopMeterScorer *self, int player);

/* ---- HangarShipScorer.cpp ---- */

#define CHANGARSHIPSCORER_VFTABLE 0x3c28e8

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

cHangarShipScorer *__fastcall cHangarShipScorer_ctor(cHangarShipScorer *self);
void __fastcall cHangarShipScorer_dtor(cHangarShipScorer *self);
cHangarShipScorer *__thiscall cHangarShipScorer_Delete(cHangarShipScorer *self, byte flags);
/* GridSpriteIndex, GridColorRed/Green/Blue/Alpha, TargetName, CamDist, CamFov */
int __thiscall cHangarShipScorer_Var(cHangarShipScorer *self, const char *name, byte set, char **s, double *d,
	int *i);
void __thiscall cHangarShipScorer_Draw(cHangarShipScorer *self, int player);

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

/* the model of that name of the scorer, NULL if none (rva 0x170d60) */
cScorerModel *__thiscall cScorer_FindModel(cScorer *scorer, const char *name);

/* ---- HeadScorer.cpp ---- */

#define CHEADSCORER_VFTABLE 0x3c2968

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
	((void(__thiscall *)(void *, int))((const cScorerItemVtbl *)(self)->base.vftable)->vf04)((self), (a))
#define HeadScorer_SetModel(self, name) \
	((byte(__thiscall *)(void *, const char *))((const cScorerItemVtbl *)(self)->base.vftable)->vf05)((self), (name))

cHeadScorer *__fastcall cHeadScorer_ctor(cHeadScorer *self);
void __fastcall cHeadScorer_dtor(cHeadScorer *self);
cHeadScorer *__thiscall cHeadScorer_Delete(cHeadScorer *self, byte flags);
/* HeadModel (set), HeadAnimIndex, VW, VH */
int __thiscall cHeadScorer_Var(cHeadScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cHeadScorer_Draw(cHeadScorer *self, int player);
/* vf05: the model of that name of the scorer of the item, its animation set again; 1 if found */
byte __thiscall cHeadScorer_SetModel(cHeadScorer *self, const char *name);
/* vf04: the animation (an index of the model's list) from its start, looped */
void __thiscall cHeadScorer_SetAnim(cHeadScorer *self, int anim);

/* ---- MissionScorer.cpp ---- */

#define CMISSIONSCORER_VFTABLE 0x3c2b70
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

#define g_MissionBlinkOn GAME_DATA(int, 0x45fa80)
#define g_MissionBlinkTime GAME_DATA(float, 0x45fa84)	/* BlinkTime: the icons blink until 0 */
#define g_MissionBlinkLast GAME_DATA(float, 0x45fa88)	/* the blink time of the last toggle */
#define g_MissionStatus ((int *)Game_Ptr(0x45fa90))		/* Status0..2 */
#define g_MissionStatusTime ((float *)Game_Ptr(0x45fa9c))	/* the scorer time each was set */
#define g_MissionMapName GAME_DATA(const char *, 0x45faa8)	/* the language text of the station */
#define g_ScorerMarginH GAME_DATA(int, 0x460040)		/* ScorerMarginH */
#define g_NextMissionLine1XBox GAME_DATA(float, 0x45faec)	/* NextMissionFirstLineOffsetXBox */
#define g_NextMissionLine2XBox GAME_DATA(float, 0x45fabc)	/* NextMissionSecondLineOffsetXBox */
#define g_NextMissionLine1PC GAME_DATA(float, 0x45fb04)	/* NextMissionFirstLineOffsetPC */
#define g_NextMissionLine2PC GAME_DATA(float, 0x45fad4)	/* NextMissionSecondLineOffsetPC */
#define g_MissionStatusColors ((const uint32_t *)Game_Ptr(0x428bd0))
#define g_MissionStatusOfs ((const float (*)[2])Game_Ptr(0x428bdc))

cMissionScorer *__fastcall cMissionScorer_ctor(cMissionScorer *self);
void __fastcall cMissionScorer_dtor(cMissionScorer *self);
cMissionScorer *__thiscall cMissionScorer_Delete(cMissionScorer *self, byte flags);
/* ForceMap (set), BackIndex, PrimaryIndex, SecondaryIndex, BlinkTime, SolidTime, Status0..2, LastStatus (get),
 * StatusClear (set), TabSprite (set), TabBlendingMode, TabSpriteIndex, TabRed/Green/Blue/Alpha, TextRed/Green/Blue */
int __thiscall cMissionScorer_Var(cMissionScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cMissionScorer_Draw(cMissionScorer *self, int player);

/* ---- MonitorScorer.cpp ---- */

#define CMONITORSCORER_VFTABLE 0x3c2cb4
#define MONITOR_WEAPONS 7
#define MONITOR_UPGRADES 13

/* the ship monitor: the panel, the head of the talk (Scorer_SetHeadMonitor) with its text, the weapon and upgrade
 * icons, the objective, the static noise; the view of the target (0x354 bytes) */
typedef struct cMonitorScorer {
	cScorerItem base;		/* 0x000 */
	float textTime;			/* 0x0ac */
	float noise;			/* 0x0b0 4 */
	CGameCamera *cam;		/* 0x0b4 */
	float fb8;			/* 0x0b8 */
	char text[0x140];		/* 0x0bc */
	int textPos;			/* 0x1fc the characters shown */
	CFont *smallFont;		/* 0x200 Terminal8 */
	CFont *bigFont;			/* 0x204 Horatio */
	cScorerModel *model;		/* 0x208 the head */
	int anim;			/* 0x20c 1 */
	byte targetView;		/* 0x210 */
	byte pad_211[3];
	int i214, i218, i21c, i220;	/* 0x214 */
	byte b224;			/* 0x224 */
	byte pad_225[0x2a4 - 0x225];
	float f2a4;			/* 0x2a4 1 */
	byte b2a8;			/* 0x2a8 */
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
	CPsaImage *objetive;		/* 0x330 */
	int objetiveIndex;		/* 0x334 */
	int objetiveChargeIndex;	/* 0x338 */
	int objetiveBlackIndex;		/* 0x33c */
	int objetiveWhiteIndex;		/* 0x340 */
	int canPossessIndex;		/* 0x344 */
	int canPossessGlowIndex;	/* 0x348 */
	CPsaImage *noiseImg;		/* 0x34c */
	CPsaImage *img350;		/* 0x350 white */
} cMonitorScorer;
SD_SIZE(cMonitorScorer, 0x354);
SD_OFFSET(cMonitorScorer, model, 0x208);
SD_OFFSET(cMonitorScorer, weapon, 0x2ac);

/* the head and text of Scorer_SetHeadMonitor */
#define g_HeadMonitorName ((char *)Game_Ptr(0x45fb10))	/* 0x80, the scorer model */
#define g_HeadMonitorText ((char *)Game_Ptr(0x45fb90))	/* 0x80 */
#define g_HeadMonitorAnim GAME_DATA(int, 0x428bf4)	/* the animation, negative: with noise */
#define g_FirstPhotoMessage GAME_DATA(char *, 0x45fc20)	/* FirstPhotoMessage */

cMonitorScorer *__fastcall cMonitorScorer_ctor(cMonitorScorer *self);
void __fastcall cMonitorScorer_dtor(cMonitorScorer *self);
cMonitorScorer *__thiscall cMonitorScorer_Delete(cMonitorScorer *self, byte flags);
/* the monitor of the entity of the camera of player (rva 0x168c60, not decompiled) */
void __thiscall cMonitorScorer_DrawMonitor(cMonitorScorer *self, int player, cEntity *target);
/* vf01: TargetView=1 */
void __thiscall cMonitorScorer_SetParam(cMonitorScorer *self, const char *name, const char *value);
int __thiscall cMonitorScorer_Var(cMonitorScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cMonitorScorer_Draw(cMonitorScorer *self, int player);
byte __thiscall cMonitorScorer_SetModel(cMonitorScorer *self, const char *name);
void __thiscall cMonitorScorer_SetAnim(cMonitorScorer *self, int anim);
/* the message FirstPhotoMessage again when the message text ended */
void Monitor_RepeatPhotoMessage(void);

/* ---- NetStatsScorer.cpp ---- */

#define CNETSTATSSCORER_VFTABLE 0x3c2f7c
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

#define g_ClientPingX GAME_DATA(float, 0x45fc80)	/* ClientPingX */
#define g_ClientPingY GAME_DATA(float, 0x45fc68)	/* ClientPingY */
#define g_ShowClientPing GAME_DATA(int, 0x45fc50)	/* ShowClientPing */
#define g_ShowNetStatus GAME_DATA(int, 0x45fc38)	/* ShowNetStatus */
/* the counters of the net (Net/) for the stats: bytes and slot of the last send and receive */
#define g_NetStatSent GAME_DATA(int, 0x465558)
#define g_NetStatSentSlot GAME_DATA(int, 0x465554)
#define g_NetStatRecv GAME_DATA(int, 0x465550)
#define g_NetStatRecvSlot GAME_DATA(int, 0x46554c)

cNetStatsScorer *__fastcall cNetStatsScorer_ctor(cNetStatsScorer *self);
void __fastcall cNetStatsScorer_dtor(cNetStatsScorer *self);
cNetStatsScorer *__thiscall cNetStatsScorer_Delete(cNetStatsScorer *self, byte flags);
/* only the base variables */
int __thiscall cNetStatsScorer_Var(cNetStatsScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cNetStatsScorer_Draw(cNetStatsScorer *self, int player);

/* ---- the news panel of the scorer (between NetStatsScorer.cpp and RadarScorer.cpp, no CHECK) ---- */

/* the news panel rendered into the texture "NewsPanel": the news head between the background and the logo, or
 * the watched entities while the alarm is on */
typedef struct cNewsPanel {
	int motorInits;			/* 0x00 g_MotorInits of the last frame: the texture is drawn again after a reset */
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

#define g_NewsPanelActive GAME_DATA(int, 0x45fdb8)		/* NewsPanelActive */
#define g_NewsPanelActiveDef GAME_DATA(int, 0x45fdbc)	/* its default */
#define g_NewsPanelShowHead GAME_DATA(int, 0x45fcc8)	/* NewsPanelShowHead */
#define g_NewsPanelShowHeadDef GAME_DATA(int, 0x45fccc)
#define g_NewsPanelOpenSpeed GAME_DATA(float, 0x45fd10)	/* NewsPanelOpenSpeed */
#define g_NewsPanelCloseSpeed GAME_DATA(float, 0x45fc98)	/* NewsPanelCloseSpeed */
#define g_NewsHead (&GAME_DATA(char *, 0x45fda0))		/* NewsHead, a string variable */
#define g_NewsLogoBmp (&GAME_DATA(char *, 0x45fcb0))		/* NewsLogoBmp */
#define g_BackNewsBmp (&GAME_DATA(char *, 0x45fd58))		/* BackNewsBmp */
#define g_NewsPanelAspect GAME_DATA(float, 0x45fde8)	/* NewsPanelAspect */
#define g_NewsPanelMove GAME_DATA(float, 0x45fdd0)		/* NewsPanelMove */
#define g_NewsPanelDisplacement GAME_DATA(float, 0x45fd28)	/* NewsPanelDisplacement */
#define g_NewsPanelDistance GAME_DATA(float, 0x45fd88)	/* NewsPanelDistance */
#define g_NewsPanelW GAME_DATA(float, 0x45fce0)		/* NewsPanelW */
#define g_NewsPanelH GAME_DATA(float, 0x45fd40)		/* NewsPanelH */
#define g_NewsPanelX GAME_DATA(float, 0x45fcf8)		/* NewsPanelX */
#define g_NewsPanelY GAME_DATA(float, 0x45fd70)		/* NewsPanelY */

cNewsPanel *__fastcall cNewsPanel_ctor(cNewsPanel *self);
void __fastcall cNewsPanel_dtor(cNewsPanel *self);
/* the first entity of the watched list 0, 1 or 2 of the world */
cEntity *__thiscall cNewsPanel_GetWatched(cNewsPanel *self, int list);
/* the switches back to their defaults, the images and the head dropped */
void __fastcall cNewsPanel_Reset(cNewsPanel *self);
/* the texture of the frame, when the scorer time moved */
void __fastcall cNewsPanel_Update(cNewsPanel *self);
/* the view of an entity from a point around it */
void __thiscall cNewsPanel_DrawEntity(cNewsPanel *self, cEntity *e);
/* the news: the head opening between the background and the logo */
void __thiscall cNewsPanel_DrawNews(cNewsPanel *self, float dt);

/* ---- RadarScorer.cpp ---- */

#define CRADARSCORER_VFTABLE 0x3c33c4
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
#define World_NumFloors() (*(int *)((byte *)g_pWorld + 0x2f0))
#define World_FloorHeight(i) (*(float *)((byte *)g_pWorld + 0x2f4 + (i) * 0x18c))
/* the view of the radar: the turn and center of the map, its scale */
#define g_RadarAngle GAME_DATA(float, 0x45fe84)
#define g_RadarCenterX GAME_DATA(float, 0x45fe88)
#define g_RadarCenterZ GAME_DATA(float, 0x45fe8c)
#define g_RadarScale GAME_DATA(float, 0x45fe80)
#define g_FileNameMap2D GAME_DATA(char *, 0x45fe60)	/* FileNameMap2D, a printf format of the floor */

cRadarScorer *__fastcall cRadarScorer_ctor(cRadarScorer *self);
void __fastcall cRadarScorer_dtor(cRadarScorer *self);
/* the Global/Table.h lookups of the input actions compiled here */
cInputAction *__thiscall InputActionTable_At(void *table, uint32_t i);
cActionSet *__thiscall ActionSetTable_At(void *table, uint32_t i);
cRadarScorer *__thiscall cRadarScorer_Delete(cRadarScorer *self, byte flags);
/* the floor of height y; the fade starts when it changed */
void __thiscall cRadarScorer_UpdateFloor(cRadarScorer *self, float y);
/* the radar and the entities on it (rva 0x16d570, not decompiled) */
void __thiscall cRadarScorer_DrawRadar(cRadarScorer *self, cEntity *target, cEntity *cam);
/* the view of the radar at the target and its map (the start of DrawRadar) */
void __thiscall cRadarScorer_DrawMap(cRadarScorer *self, cEntity *target, cEntity *cam);
/* the icon of an entity at pos (flags RADAR_ICON_OUT_*) */
void __thiscall cRadarScorer_DrawIcon(cRadarScorer *self, const vec3_t *pos, int icon, float ang, int outIcon,
	uint32_t flags, uint32_t color, uint32_t outColor, byte always);
/* a view cone at pos to the angle, spread wide, range long, mode 1..3 the color */
void __thiscall cRadarScorer_DrawCone(cRadarScorer *self, const vec3_t *pos, float ang, float spread, float range,
	int mode);
/* y is on the floor shown before */
byte __thiscall cRadarScorer_OnPrevFloor(cRadarScorer *self, float y);
/* y below the floor (-1), on it (0) or above it (1) */
int __thiscall cRadarScorer_FloorSide(cRadarScorer *self, float y);
int __thiscall cRadarScorer_Var(cRadarScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cRadarScorer_Draw(cRadarScorer *self, int player);


/* ---- Scorer.cpp (src/game/Scorer/Scorer.c) ---- */

#define SCORER_MAX_HEAD_ANIMS 8

/* the flags and the default of an engine variable by its value (cGVar of include/scrap/scrap.h) */
#define GVAR_FLAGS(v) (*((byte *)&(v) - 4))
#define GVAR_DEF(type, v) (*(type *)((byte *)&(v) + 4))

#define g_Scorers GAME_DATA(cScorer *, 0x4601d4)		/* new cScorer[4] */
#define g_ScorerWhite GAME_DATA(CPsaImage *, 0x4601c8)	/* 2D/white.tga */
#define g_ScorerNoise GAME_DATA(CPsaImage *, 0x4601cc)	/* 2D/Noise.tga */
#define g_ScorerMarker GAME_DATA(CPsaImage *, 0x4601d0)	/* the focus marker (SScorer.SetMarkerSprite) */
#define g_ScorerTabs GAME_DATA(CPsaImage *, 0x45fe94)	/* 2D/Menu/Tabs.alpha.tga */
#define g_NewsPanel GAME_DATA(cNewsPanel *, 0x45fe90)
/* the engine variables of Scorer.cpp */
#define g_RadarMMSelect GAME_DATA(int, 0x45fe78)		/* RadarMMSelect */
#define g_NumEnemiesInRadar GAME_DATA(int, 0x45fe30)
#define g_CinemaBlackBandHeight GAME_DATA(float, 0x45fea8)	/* CinemaBlackBandHeight */
#define g_NoiseFactor GAME_DATA(int, 0x45ff98)		/* NoiseFactor */
#define g_NoiseFactorBegin GAME_DATA(int, 0x460058)	/* NoiseFactorBegin */
#define g_NoiseTime GAME_DATA(float, 0x45fff8)		/* NoiseTime */
#define g_NoiseEndTime GAME_DATA(float, 0x45ff80)		/* NoiseEndTime */
#define g_MotionBlurFactorBegin GAME_DATA(float, 0x45fec0)	/* MotionBlurFactorBegin */
#define g_MotionBlurFactorEnd GAME_DATA(float, 0x460118)	/* MotionBlurFactorEnd */
#define g_MotionBlurTime GAME_DATA(float, 0x460088)	/* MotionBlurTime */
#define g_MotionBlurEndTime GAME_DATA(float, 0x45ffb0)	/* MotionBlurEndTime */
#define g_MotionBlurOffVar GAME_DATA(float, 0x4601a8)	/* MotionBlurOffVar */
#define g_FullScreenConsoleCallback (&GAME_DATA(cPy_Function, 0x4601e8))	/* FullScreenConsoleCallback */
#define g_CurrentWaypointName GAME_DATA(char *, 0x462428)	/* CurrentWaypointName */
#define g_CurrentWaypointName2 GAME_DATA(char *, 0x462410)	/* CurrentWaypointName2 */
#define g_CameraCloudDistance GAME_DATA(float, 0x45ff68)	/* CameraCloudDistance */
#define g_CameraCloudPlaneSize GAME_DATA(float, 0x45fef0)	/* CameraCloudPlaneSize */
#define g_CameraCloudThickness GAME_DATA(float, 0x460130)	/* CameraCloudThickness */
#define g_CameraCloudAlpha GAME_DATA(int, 0x460178)	/* CameraCloudAlpha */
#define g_CameraCloudRed GAME_DATA(int, 0x45ff08)		/* CameraCloudRed */
#define g_CameraCloudGreen GAME_DATA(int, 0x460010)	/* CameraCloudGreen */
#define g_CameraCloudBlue GAME_DATA(int, 0x460100)		/* CameraCloudBlue */
#define g_OnAcceptSound GAME_DATA(char *, 0x45ffc8)	/* OnAcceptSound */
#define g_OnAcceptSoundVol GAME_DATA(float, 0x45fed8)	/* OnAcceptSoundVol */
#define g_OnFocusSound GAME_DATA(char *, 0x4600e8)		/* OnFocusSound */
#define g_OnFocusSoundVol GAME_DATA(float, 0x45ff50)	/* OnFocusSoundVol */
#define g_ScorerMarkerOn GAME_DATA(int, 0x460070)		/* ScorerMarker */
#define g_ScorerMarkerColor GAME_DATA(uint32_t, 0x460190)	/* ScorerMarkerColor */
#define g_ScorerMarkerSpeed GAME_DATA(float, 0x4600b8)	/* ScorerMarkerSpeed */
#define g_CShow GAME_DATA(int, 0x4601c0)			/* CShow */
#define g_XboxNoPadMsg GAME_DATA(int, 0x460160)		/* XboxNoPadMsg */
#define g_XboxNoPadMsgP GAME_DATA(int, 0x45ff38)		/* XboxNoPadMsgP */
#define g_XboxNoPadMsgPBig GAME_DATA(int, 0x45ffe0)	/* XboxNoPadMsgPBig */
#define g_XboxSavingMsg GAME_DATA(float, 0x460028)		/* XboxSavingMsg */
#define g_XboxSavingMsgStr GAME_DATA(char *, 0x45ff20)	/* XboxSavingMsgStr */
#define g_ShowConsoleLog GAME_DATA(int, 0x460418)		/* ShowConsoleLog */
#define g_ScorerRandScale GAME_DATA(double, 0x3a3d60)	/* 1/32767 */

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

ScorerCinema *__fastcall ScorerCinema_ctor(ScorerCinema *self);
ScorerFade *__fastcall ScorerFade_ctor(ScorerFade *self);
void *__fastcall ScorerFx_ctor(void *self);
cScorer *__fastcall cScorer_ctor(cScorer *self);
void __fastcall cScorer_dtor(cScorer *self);
cScorer *__thiscall cScorer_Delete(cScorer *self, byte flags);
cScorerItem *__thiscall cScorerItem_Delete(cScorerItem *self, byte flags);
/* the scorer model of that name: its game model, NULL if none */
CGameModel *__thiscall cScorer_FindModelModel(cScorer *scorer, const char *name);
void __fastcall cScorer_Free(cScorer *scorer);
void __thiscall cScorer_SetParam(cScorer *scorer, const char *name, const char *value);
void __thiscall cScorer_RemoveDeleted(cScorer *scorer, int player);
void __thiscall cScorer_SetCursorItem(cScorer *scorer, int player, const char *from, const char *to);
void Scorer_SetInputState(int state);
CPsaImage *Scorer_GetWhite(void);
CPsaImage *Scorer_GetMarker(void);
CPsaImage *Scorer_GetNoise(void);
void Scorer_GetCamVarStr(int player, char *buf, const char *name, int size);
int Scorer_GetCamVarInt(int player, const char *name);
void Scorer_DrawPyMemStats(void);
uint32_t Scorer_ColorLerp(uint32_t a, uint32_t b, float t);
void __thiscall ScorerFade_Draw(ScorerFade *fade, int player, float dt);
void __thiscall ScorerCinema_Draw(ScorerCinema *cinema, int player, float dt);
void __thiscall ScorerFx_Draw(void *fx, int player, float dt);
void __thiscall cScorer_UpdateMP(cScorer *scorer, float dt);
void __thiscall cScorer_UpdateCursor(cScorer *scorer, int player);
void __thiscall cScorer_Draw(cScorer *scorer, int player);
void __thiscall cScorer_DrawClouds(cScorer *scorer, int player);
/* the big message: the start text; else the pad text of player index (pad the player number in split screen) */
void __thiscall cScorer_DrawNoPadMsg(cScorer *scorer, int index, float y, byte big, int pad);
void __thiscall cScorer_DrawXboxMsgs(cScorer *scorer, int player);
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

#define CSCRAPLANDMAINMENUSCORER_VFTABLE 0x3c3ff4
#define CSCRAPLANDSUBMENUSCORER_VFTABLE 0x3c4008

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
#define g_MainMenuAnimInit GAME_DATA(byte, 0x460428)
#define g_SubMenuAnimInit GAME_DATA(byte, 0x460429)
#define g_MainMenuWait ((float *)Game_Ptr(0x46042c))		/* [9] */
#define g_MainMenuTime ((float *)Game_Ptr(0x460450))		/* [9] */
#define g_MainMenuLength ((float *)Game_Ptr(0x460474))	/* [9] */
#define g_MainMenuSparkDir ((byte *)Game_Ptr(0x4604e4))	/* [19] */
#define g_MainMenuSparkWait ((float *)Game_Ptr(0x460498))	/* [19] */
#define g_MainMenuSparkTime ((float *)Game_Ptr(0x4604f8))	/* [19] */
#define g_MainMenuSparkLength ((float *)Game_Ptr(0x460548))	/* [19] */
#define g_MainMenuSparkFrame ((int *)Game_Ptr(0x4605a0))	/* [19] */
#define g_MainMenuNeonNext GAME_DATA(float, 0x460544)
#define g_MainMenuNeonGroup ((float *)Game_Ptr(0x460594))	/* [3] */
#define g_MainMenuNeonWait ((float *)Game_Ptr(0x4605f0))	/* [21] */
#define g_SubMenuWait GAME_DATA(float, 0x4605ec)
#define g_SubMenuRingTime GAME_DATA(float, 0x460644)
#define g_SubMenuRingWait GAME_DATA(float, 0x460648)
#define g_SubMenuRingDir GAME_DATA(int, 0x46064c)		/* 1: the ring frames run backwards */
#define g_SubMenuRingFrame GAME_DATA(int, 0x460650)	/* the first frame of the ring */

cScraplandMainMenuScorer *__fastcall cScraplandMainMenuScorer_ctor(cScraplandMainMenuScorer *self);
cScraplandSubMenuScorer *__fastcall cScraplandSubMenuScorer_ctor(cScraplandSubMenuScorer *self);
void __fastcall cScraplandMainMenuScorer_dtor(cScraplandMainMenuScorer *self);
void __fastcall cScraplandSubMenuScorer_dtor(cScraplandSubMenuScorer *self);
cScraplandMainMenuScorer *__thiscall cScraplandMainMenuScorer_Delete(cScraplandMainMenuScorer *self, byte flags);
cScraplandSubMenuScorer *__thiscall cScraplandSubMenuScorer_Delete(cScraplandSubMenuScorer *self, byte flags);
/* the time of the item and, once, the random animation state */
void __fastcall cScraplandMainMenuScorer_Init(cScraplandMainMenuScorer *self);
void __fastcall cScraplandSubMenuScorer_Init(cScraplandSubMenuScorer *self);
/* the image setters (flags, file) */
void __thiscall cScraplandMainMenuScorer_SetGlow(cScraplandMainMenuScorer *self, uint32_t flags, const char *file);
void __thiscall cScraplandSubMenuScorer_SetGlow(cScraplandSubMenuScorer *self, uint32_t flags, const char *file);
void __thiscall cScraplandSubMenuScorer_Draw(cScraplandSubMenuScorer *self, int player);
/* 2D/white.tga loads without the flag 0x40, any other file with it */
void __thiscall cScraplandMainMenuScorer_SetLighting(cScraplandMainMenuScorer *self, uint32_t flags,
	const char *file);
void __thiscall cScraplandSubMenuScorer_SetLighting(cScraplandSubMenuScorer *self, uint32_t flags, const char *file);
void __thiscall cScraplandMainMenuScorer_SetLogo(cScraplandMainMenuScorer *self, uint32_t flags, const char *file);
void __thiscall cScraplandSubMenuScorer_SetLogo(cScraplandSubMenuScorer *self, uint32_t flags, const char *file);
void __thiscall cScraplandMainMenuScorer_SetNeon(cScraplandMainMenuScorer *self, uint32_t flags, const char *file);
void __thiscall cScraplandSubMenuScorer_SetRingBorder(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file);
void __thiscall cScraplandSubMenuScorer_SetRingMask(cScraplandSubMenuScorer *self, uint32_t flags, const char *file);
void __thiscall cScraplandSubMenuScorer_SetRingReflection(cScraplandSubMenuScorer *self, uint32_t flags,
	const char *file);
/* HighRes; set only: FileLogo, FileGlow, FileNeon, FileLighting */
int __thiscall cScraplandMainMenuScorer_Var(cScraplandMainMenuScorer *self, const char *name, byte set, char **s,
	double *d, int *i);
/* set only: FileLogo, FileGlow, FileLighting, FileRingMask, FileRingReflection, FileRingBorder; HighRes, Scale */
int __thiscall cScraplandSubMenuScorer_Var(cScraplandSubMenuScorer *self, const char *name, byte set, char **s,
	double *d, int *i);

/* ---- SliderScorer.cpp ---- */

#define CSLIDERSCORER_VFTABLE 0x3c4140

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

cSliderScorer *__fastcall cSliderScorer_ctor(cSliderScorer *self);
void __fastcall cSliderScorer_dtor(cSliderScorer *self);
cSliderScorer *__thiscall cSliderScorer_Delete(cSliderScorer *self, byte flags);
void __thiscall cSliderScorer_SetFile(cSliderScorer *self, uint32_t flags, const char *file);
int __thiscall cSliderScorer_Var(cSliderScorer *self, const char *name, byte set, char **s, double *d, int *i);
void __thiscall cSliderScorer_Draw(cSliderScorer *self, int player);

/* ---- SpecialActionScorer.cpp ---- */

#define CSPECIALACTIONSCORER_VFTABLE 0x3c42d4
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

#define g_SpecialActionActive GAME_DATA(int, 0x46067c)	/* SpecialActionActive 0 off, 1 blink, 2 on */

cSpecialActionScorer *__fastcall cSpecialActionScorer_ctor(cSpecialActionScorer *self);
void __fastcall cSpecialActionScorer_dtor(cSpecialActionScorer *self);
cSpecialActionScorer *__thiscall cSpecialActionScorer_Delete(cSpecialActionScorer *self, byte flags);
/* TextFont returns 0 when the font exists (and 1 when not), the original does so */
int __thiscall cSpecialActionScorer_Var(cSpecialActionScorer *self, const char *name, byte set, char **s, double *d,
	int *i);
void __thiscall cSpecialActionScorer_Draw(cSpecialActionScorer *self, int player);
void __thiscall cSpecialActionScorer_DrawIcon(cSpecialActionScorer *self, int player, int state);
void __fastcall cSpecialActionScorer_DrawEnergy(cSpecialActionScorer *self);
void __thiscall cSpecialActionScorer_DrawCrossHair(cSpecialActionScorer *self, int player);
void __thiscall cSpecialActionScorer_DrawCannon(cSpecialActionScorer *self, int player, const float *pos,
	const float *size, float ang, uint32_t color);
/* the target search of the gear and its aim, the sphere callback of the search */
void __thiscall cSpecialActionScorer_DrawGearAim(cSpecialActionScorer *self, int player);
void GearAim_SphereFunc(cAbstractFile *params);
void __thiscall SpecialAction_DrawDefault(cSpecialActionScorer *self, int player);
void __thiscall SpecialAction_DrawBanker(cSpecialActionScorer *self, int player);
void __thiscall SpecialAction_DrawTarget(cSpecialActionScorer *self, int player);
void __thiscall SpecialAction_DrawGear(cSpecialActionScorer *self, int player);
void __thiscall SpecialAction_DrawGear2(cSpecialActionScorer *self, int player);
void __thiscall SpecialAction_DrawGear3(cSpecialActionScorer *self, int player);
void __thiscall SpecialAction_DrawVictim(cSpecialActionScorer *self, int player);
void __thiscall SpecialAction_DrawOn(cSpecialActionScorer *self, int player);
void __thiscall SpecialAction_DrawLowEnergy(cSpecialActionScorer *self, int player);
void __thiscall SpecialAction_DrawFunctionary(cSpecialActionScorer *self, int player);
void __thiscall SpecialAction_DrawEnergyOnly(cSpecialActionScorer *self, int player);

#define g_EnergyBarActive GAME_DATA(int, 0x44726c)		/* EnergyBarActive */
#define g_FunctionarySpecialActionLow GAME_DATA(float, 0x446738)	/* FunctionarySpecialActionLow */

/* the exe functions the special action draws use (other modules) */
/* the fraction of the weapon energy above the full batteries, and their count */
float __thiscall cWalkCharEntity_GetEnergyFrac(cEntity *e);	/* rva 0x0d3740 */
int __thiscall cWalkCharEntity_GetBatteries(cEntity *e);	/* rva 0x0d38e0 */
/* frame + 1 stretched to the width w, frame at both ends (the right one mirrored), h the y scale (rva 0x184710) */
void Menu_DrawStretch(CPsaImage *img, int frame, float x, float y, float w, float h);

#endif
