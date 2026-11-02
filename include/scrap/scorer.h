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
	void(*Draw)(cScorerItem *self, int player);					/* 0x0 */
	/* a "name=value" parameter, nothing in most classes */
	void(*SetParam)(cScorerItem *self, const char *name, const char *value);	/* 0x4 */
	/* like cActClass_Var: set != 0 writes *s or *d / *i; returns 1 string, 2 double, 3 int, 0 */
	int(*Var)(cScorerItem *self, const char *name, int set, char **s, double *d, int *i);	/* 0x8 */
	cScorerItem *(*Delete)(cScorerItem *self, byte flags);			/* 0xc */
} cScorerItemVtbl;
SD_SIZE(cScorerItemVtbl, 0x10);

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
	struct cScorerItem *next;	/* 0x2c in the list of the scorer */
	int player;			/* 0x30 of the scorer */
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

/* the base class (Scorer.cpp): constructor and destructor */
cScorerItem *cScorerItem_ctor(cScorerItem *self);
void cScorerItem_dtor(cScorerItem *self);
/* Hint, MaxHintLineWidth, Name, DelMe, X, Y, W, H, FocusPivotX/Y, Left, Right, Up, Down, the On* functions,
 * MultiPress, Visible */
int cScorerItem_Var(cScorerItem *self, const char *name, int set, char **s, double *d, int *i);

/* the cinema bars (SScorer.SetCinema), 0x10 bytes */
typedef struct ScorerCinema {
	float time;			/* 0x0 0..life */
	float life;			/* 0x4 */
	byte on;			/* 0x8 */
	byte pad_9[3];
	float height;			/* 0xc CinemaBlackBandHeight while on */
} ScorerCinema;
SD_SIZE(ScorerCinema, 0x10);

/* a fade of the whole screen (SScorer.Fade): the colors as floats 0..255, 0x28 bytes */
typedef struct ScorerFade {
	float from[4];			/* 0x00 r, g, b, a: the color at the start */
	float to[4];			/* 0x10 the color at the end */
	float time;			/* 0x20 counts down to 0 */
	float life;			/* 0x24 */
} ScorerFade;
SD_SIZE(ScorerFade, 0x28);

typedef struct cScorer {
	void *vftable;			/* 0x00 rva 0x3c3bb4, only the deleting destructor */
	cScorerItem *found;		/* 0x04 the last item of FindItem */
	byte cleared;			/* 0x08 Clear ran in this frame: only items marked DelMe are drawn */
	byte pad_09[3];
	float marker[4];		/* 0x0c x0, y0, x1, y1 of the focus marker, moving to the cursor item */
	float markerAlpha;		/* 0x1c 0..1 */
	float time;			/* 0x20 the time of the scorer (blinking) */
	float lastTime;			/* 0x24 */
	float frameTime;		/* 0x28 the time of the last frame */
	float acceptTime;		/* 0x2c counts down while the accept is held; -10 off */
	char *hint;			/* 0x30 cString, the hint shown (cHintScorer) */
	char *specialHint;		/* 0x34 cString, the hint while onSpecialHint returns 1 */
	float hintTime;			/* 0x38 the hint shows HintAppearsTime after this */
	int hintWidth;			/* 0x3c the line width of the hint */
	cScorerItem *items;		/* 0x40 linked by next, drawn in this order */
	byte visible;			/* 0x44 SScorer.Show/Hide */
	byte pad_45[3];
	char *current;			/* 0x48 cString, the name of the cursor item (SScorer.GetActual) */
	char *defaultItem;		/* 0x4c SScorer.GetDefault */
	cScorerItem *cursor;		/* 0x50 SScorer.SetCursor */
	cPy_Function onCancel;		/* 0x54 */
	cPy_Function onNext;		/* 0x68 */
	cPy_Function onPrev;		/* 0x7c */
	cPy_Function onSpecialHint;	/* 0x90 */
	int skipFrames;			/* 0xa4 frames without the cursor moves (Scorer_SetInputState) */
	struct cScorerModel *models;	/* 0xa8 SScorer.AddModel */
	int player;			/* 0xac */
	ScorerFade fade;		/* 0xb0 */
	ScorerCinema cinema;		/* 0xd8 */
	byte fx;			/* 0xe8 the noise and motion blur, an object without fields (globals) */
	byte pad_e9[3];
	cPy_Function mpFunc;		/* 0xec SScorer.SetMPFunc */
	byte focusSound;		/* 0x100 the focus sound plays on the next cursor move */
	byte pad_101[3];
} cScorer;
SD_SIZE(cScorer, 0x104);
SD_OFFSET(cScorer, time, 0x20);
SD_OFFSET(cScorer, frameTime, 0x28);
SD_OFFSET(cScorer, specialHint, 0x34);
SD_OFFSET(cScorer, hintWidth, 0x3c);
SD_OFFSET(cScorer, visible, 0x44);
SD_OFFSET(cScorer, cursor, 0x50);
SD_OFFSET(cScorer, onSpecialHint, 0x90);
SD_OFFSET(cScorer, models, 0xa8);
SD_OFFSET(cScorer, fade, 0xb0);
SD_OFFSET(cScorer, cinema, 0xd8);
SD_OFFSET(cScorer, mpFunc, 0xec);
SD_OFFSET(cScorer, focusSound, 0x100);

cScorer *Scorer_Get(int player);
cScorerItem *cScorer_FindItem(cScorer *scorer, const char *name);
cScorerItem *cScorer_AddItem(cScorer *scorer, const char *name, const char *type, byte flag);
void cScorer_Clear(cScorer *scorer);
void cScorer_SetDefault(cScorer *scorer, const char *name);
void cScorer_SetMarkerSprite(cScorer *scorer, const char *sprite);
/* the model of the new entry (never NULL) */
void *cScorer_AddModel(cScorer *scorer, const char *name, const char *model, const char *anim, int numAnim,
	float radius, float height, byte head);
void ScorerFade_Start(ScorerFade *fade, uint32_t from, uint32_t to, float time);
void ScorerCinema_Set(ScorerCinema *cinema, byte on, float time);
/* cPyMenuEntity (type object rva 0x428f78): Python handle of a scorer item by player and item name */
typedef struct cPyMenuEntity {
	PyObject_HEAD
	int player;			/* 0x8 */
	char *name;			/* 0xc Mem_Alloc copy */
} cPyMenuEntity;
SD_SIZE(cPyMenuEntity, 0x10);

extern PyTypeObject cPyMenuEntity_Type;

/* the handle of a scorer item, NULL (no Python error) if there is no such item */
PyObject *ScorerItem_PyNew(int player, const char *name);


void Scorer_AddChatMsg(const char *text, uint32_t rgb);
void Scorer_SetConsole(int on);
void Scorer_SetSpeechText(const char *text, float time, int a, int b, int c);
void Scorer_SetSpeechCallback(const char *func, PyObject *arg);
void Scorer_CancelSpeech(float time);
void Scorer_SetHeadMonitor(const char *a, const char *b, int c);
void Scorer_SetMsgText(const char *text, float time, const char *font);
void Scorer_SetLabelText(const char *text, float time);
void Scorer_PreloadTexture(const char *name, int flag);
/* screen position of a world point into out[2], 0 off the screen; last argument 0 here */
byte Engine_WorldToScreen(const vec3_t *pos, float *out, float *depth);

#endif
