/*
 * The scorer: HUD and menus of each local player (original source: Menu/). Scorer_Get(player) returns the scorer of
 * player 0..3 (0x104 bytes each) or NULL. Items are named HUD elements (cScorerItem and derived classes).
 */
#ifndef SCRAP_SCORER_H
#define SCRAP_SCORER_H

#include "scrap/cstring.h"
#include "scrap/game.h"
#include "scrap/mathlib.h"
#include "scrap/pyfunction.h"
#include "scrap/scrapfont.h"
#include "scrap/python.h"

typedef struct cScorerItem cScorerItem;
typedef struct cScorerItemVtbl {
	union {
		void *vf00_01[2];
		struct {
			void(__thiscall *Draw)(cScorerItem *self, int player);				/* 0x0 */
			/* a "name=value" parameter, nothing in most classes */
			void(__thiscall *SetParam)(cScorerItem *self, const char *name, const char *value);	/* 0x4 */
		};
	};
	/* like cActClass_Var: set != 0 writes *s or *d / *i; returns 1 string, 2 double, 3 int, 0 */
	int(__thiscall *Var)(cScorerItem *self, const char *name, int set, char **s, double *d, int *i);	/* 0x8 */
	cScorerItem *(__thiscall *Delete)(cScorerItem *self, byte flags);			/* 0xc */
	/* only in some classes: SetAnim of the heads, the lock text of the cannon, a slot of cSpriteScorer */
	void *vf04;										/* 0x10 */
	void *vf05;										/* 0x14 SetModel of the heads */
} cScorerItemVtbl;
SD_OFFSET(cScorerItemVtbl, Var, 0x8);
SD_OFFSET(cScorerItemVtbl, vf05, 0x14);

/* the items of a scorer (Scorer.cpp, vftable rva 0x3c3ba0), 0xac bytes */
struct cScorerItem {
	const cScorerItemVtbl *vftable;	/* 0x00 */
	char *name;			/* 0x04 cString */
	char *hint;			/* 0x08 cString, shown while the item is the cursor */
	int maxHintLineWidth;		/* 0x0c 0xff */
	float x, y;			/* 0x10 */
	float w, h;			/* 0x18 */
	float focusPivotX, focusPivotY;	/* 0x20 */
	byte delMe;			/* 0x28 */
	byte visible;			/* 0x29 1 */
	byte pad_2a[2];
	int i2c;			/* 0x2c */
	int i30;			/* 0x30 */
	char *left, *right, *up, *down;	/* 0x34 cStrings, the item names of the cursor moves */
	cPy_Function onAccept;		/* 0x44 */
	cPy_Function onDelete;		/* 0x58 */
	cPy_Function onRender;		/* 0x6c */
	cPy_Function onLooseFocus;	/* 0x80 */
	cPy_Function onGainFocus;	/* 0x94 */
	byte multiPress;		/* 0xa8 */
	byte pad_a9[3];
};
SD_SIZE(cScorerItem, 0xac);
SD_OFFSET(cScorerItem, visible, 0x29);
SD_OFFSET(cScorerItem, onAccept, 0x44);
SD_OFFSET(cScorerItem, multiPress, 0xa8);

/* the base class (Scorer.cpp): fastcall constructor and destructor */
cScorerItem *__fastcall cScorerItem_ctor(cScorerItem *self);
void __fastcall cScorerItem_dtor(cScorerItem *self);
/* Hint, MaxHintLineWidth, Name, DelMe, X, Y, W, H, FocusPivotX/Y, Left, Right, Up, Down, the On* functions,
 * MultiPress, Visible */
int __thiscall cScorerItem_Var(cScorerItem *self, const char *name, byte set, char **s, double *d, int *i);

typedef struct cScorer {
	void *vftable;			/* 0x00 */
	byte pad_04[0x20 - 0x04];
	float time;			/* 0x20 the time of the scorer (blinking) */
	byte pad_24[0x28 - 0x24];
	float frameTime;		/* 0x28 the time of the last frame */
	byte pad_2c[0x30 - 0x2c];
	char *hint;			/* 0x30 cString, the hint shown (cHintScorer) */
	char *specialHint;		/* 0x34 cString, the hint while onSpecialHint returns 1 */
	float hintTime;			/* 0x38 the hint shows HintAppearsTime after this */
	int hintWidth;			/* 0x3c the line width of the hint */
	byte pad_40[0x44 - 0x40];
	byte visible;			/* 0x44 SScorer.Show/Hide */
	byte pad_45[3];
	char *actual;			/* 0x48 cString, the name of the cursor item (SScorer.GetActual) */
	char *defaultItem;		/* 0x4c SScorer.GetDefault */
	cScorerItem *cursor;		/* 0x50 SScorer.SetCursor */
	cPy_Function onCancel;		/* 0x54 */
	cPy_Function onNext;		/* 0x68 */
	cPy_Function onPrev;		/* 0x7c */
	cPy_Function onSpecialHint;	/* 0x90 */
	byte pad_a4[0xb0 - 0xa4];
	byte fade[0xd8 - 0xb0];		/* 0xb0 fade object (SScorer.Fade) */
	byte cinema[0xec - 0xd8];	/* 0xd8 cinema bars (SScorer.SetCinema) */
	cPy_Function mpFunc;		/* 0xec */
	byte pad_100[0x104 - 0x100];
} cScorer;
SD_SIZE(cScorer, 0x104);
SD_OFFSET(cScorer, time, 0x20);
SD_OFFSET(cScorer, frameTime, 0x28);
SD_OFFSET(cScorer, specialHint, 0x34);
SD_OFFSET(cScorer, hintWidth, 0x3c);
SD_OFFSET(cScorer, visible, 0x44);
SD_OFFSET(cScorer, cursor, 0x50);
SD_OFFSET(cScorer, onSpecialHint, 0x90);
SD_OFFSET(cScorer, fade, 0xb0);
SD_OFFSET(cScorer, cinema, 0xd8);
SD_OFFSET(cScorer, mpFunc, 0xec);

cScorer *Scorer_Get(int player);
cScorerItem *__thiscall cScorer_FindItem(cScorer *scorer, const char *name);
void *__thiscall cScorer_AddItem(cScorer *scorer, const char *name, const char *type, byte flag);
void __thiscall cScorer_Clear(cScorer *scorer);
void __thiscall cScorer_SetDefault(cScorer *scorer, const char *name);
void __thiscall cScorer_SetMarkerSprite(cScorer *scorer, const char *sprite);
int __thiscall cScorer_AddModel(cScorer *scorer, const char *name, const char *model, const char *anim, int numAnim,
	float radius, float c, byte flag);
void __thiscall ScorerFade_Start(void *fade, uint32_t from, uint32_t to, float time);
void __thiscall ScorerCinema_Set(void *cinema, byte on, float time);
/* cPyMenuEntity (type object rva 0x428f78): Python handle of a scorer item by player and item name */
typedef struct cPyMenuEntity {
	PyObject_HEAD
	int player;			/* 0x8 */
	char *name;			/* 0xc Mem_Alloc copy */
} cPyMenuEntity;
SD_SIZE(cPyMenuEntity, 0x10);

#define cPyMenuEntity_Type GAME_DATA(PyTypeObject, 0x428f78)

/* the handle of a scorer item, NULL (no Python error) if there is no such item */
PyObject *ScorerItem_PyNew(int player, const char *name);


void Scorer_AddChatMsg(const char *text, uint32_t rgb);
void Scorer_SetConsole(int on);
void Scorer_SetSpeechText(const char *text, float time, int a, int b, int c);
void Scorer_SetSpeechCallback(const char *func, PyObject *arg);
void Scorer_CancelSpeech(int arg);
void Scorer_SetHeadMonitor(const char *a, const char *b, int c);
void Scorer_SetMsgText(const char *text, float time, const char *font);
void Scorer_SetLabelText(const char *text, float time);
void Scorer_PreloadTexture(const char *name, int flag);
/* screen position of a world point into out[2], 0 off the screen; last argument 0 here */
byte Engine_WorldToScreen(const vec3_t *pos, float *out, float *depth);

#endif
