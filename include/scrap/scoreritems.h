/*
 * The HUD and menu items of Menu/ from AbstractTextScorer.cpp to RadarScorer.cpp (the classes over cScorerItem of
 * include/scrap/scorer.h). Every class has Draw(player) at vf00, Var at vf02 and the deleting destructor at vf03;
 * the constructors and destructors are fastcall, the methods thiscall.
 */
#ifndef SCRAP_SCORERITEMS_H
#define SCRAP_SCORERITEMS_H

#include "scrap/entity.h"
#include "scrap/scorer.h"
#include "scrap/scrapmain.h"

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

#endif
