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
	void(*Draw)(cScorerItem *self, int player);
	/* a "name=value" parameter, nothing in most classes */
	void(*SetParam)(cScorerItem *self, const char *name, const char *value);
	/* like cActClass_Var: set != 0 writes *s or *d / *i; returns 1 string, 2 double, 3 int, 0 */
	int(*Var)(cScorerItem *self, const char *name, int set, char **s, double *d, int *i);
	void (*Free)(cScorerItem *self);
} cScorerItemVtbl;

/* calls through the slots of cScorerItemVtbl */
static inline void ScorerItem_Free(void *self) {
	VT(cScorerItemVtbl, self)->Free(self);
}

static inline void ScorerItem_Draw(void *self, int player) {
	VT(cScorerItemVtbl, self)->Draw(self, player);
}

static inline void ScorerItem_SetParam(void *self, const char *name, const char *value) {
	VT(cScorerItemVtbl, self)->SetParam(self, name, value);
}

static inline int ScorerItem_Var(void *self, const char *name, int set, char **s, double *d, int *i) {
	return VT(cScorerItemVtbl, self)->Var(self, name, set, s, d, i);
}

/* the items of a scorer (Scorer.cpp, vftable) */
struct cScorerItem {
	const cScorerItemVtbl *vftable;
	char *name;			/* cString */
	char *hint;			/* cString, shown while the item is the cursor */
	int maxHintLineWidth;		/* 0xff */
	float x, y;
	float w, h;
	float focusPivotX, focusPivotY;
	byte delMe;
	byte visible;			/* 1 */
	byte pad_2a[2];
	struct cScorerItem *next;	/* in the list of the scorer */
	int player;			/* of the scorer */
	char *left, *right, *up, *down;	/* cStrings, the item names of the cursor moves */
	cPy_Function onAccept;
	cPy_Function onDelete;
	cPy_Function onRender;
	cPy_Function onLooseFocus;
	cPy_Function onGainFocus;
	byte multiPress;
	byte pad_a9[3];
};

/* the base class (Scorer.cpp): constructor and destructor */
cScorerItem *cScorerItem_ctor(cScorerItem *self);
void cScorerItem_dtor(cScorerItem *self);
/* Hint, MaxHintLineWidth, Name, DelMe, X, Y, W, H, FocusPivotX/Y, Left, Right, Up, Down, the On* functions,
 * MultiPress, Visible */
int cScorerItem_Var(cScorerItem *self, const char *name, int set, char **s, double *d, int *i);

/* the cinema bars (SScorer.SetCinema) */
typedef struct ScorerCinema {
	float time;			/* 0..life */
	float life;
	byte on;
	byte pad_9[3];
	float height;			/* CinemaBlackBandHeight while on */
} ScorerCinema;

/* a fade of the whole screen (SScorer.Fade): the colors as floats 0..255 */
typedef struct ScorerFade {
	float from[4];			/* r, g, b, a: the color at the start */
	float to[4];			/* the color at the end */
	float time;			/* counts down to 0 */
	float life;
} ScorerFade;

typedef struct cScorer {
	cScorerItem *found;		/* the last item of FindItem */
	byte cleared;			/* Clear ran in this frame: only items marked DelMe are drawn */
	byte pad_09[3];
	float marker[4];		/* x0, y0, x1, y1 of the focus marker, moving to the cursor item */
	float markerAlpha;		/* 0..1 */
	float time;			/* the time of the scorer (blinking) */
	float lastTime;
	float frameTime;		/* the time of the last frame */
	float acceptTime;		/* counts down while the accept is held; -10 off */
	char *hint;			/* cString, the hint shown (cHintScorer) */
	char *specialHint;		/* cString, the hint while onSpecialHint returns 1 */
	float hintTime;			/* the hint shows HintAppearsTime after this */
	int hintWidth;			/* the line width of the hint */
	cScorerItem *items;		/* linked by next, drawn in this order */
	byte visible;			/* SScorer.Show/Hide */
	byte pad_45[3];
	char *current;			/* cString, the name of the cursor item (SScorer.GetActual) */
	char *defaultItem;		/* SScorer.GetDefault */
	cScorerItem *cursor;		/* SScorer.SetCursor */
	cPy_Function onCancel;
	cPy_Function onNext;
	cPy_Function onPrev;
	cPy_Function onSpecialHint;
	int skipFrames;			/* frames without the cursor moves (Scorer_SetInputState) */
	struct cScorerModel *models;	/* SScorer.AddModel */
	int player;
	ScorerFade fade;
	ScorerCinema cinema;
	byte fx;			/* the noise and motion blur, an object without fields (globals) */
	byte pad_e9[3];
	cPy_Function mpFunc;		/* SScorer.SetMPFunc */
	byte focusSound;		/* the focus sound plays on the next cursor move */
	byte pad_101[3];
} cScorer;

cScorer *Scorer_Get(int player);
cScorerItem *cScorer_FindItem(cScorer *scorer, const char *name);
/* runs the OnAccept of the item with the player and the item name, as the accept key of the menu does (SScorer.Accept,
 * the test channel); 0 when the item or its OnAccept is missing */
byte Scorer_AcceptItem(int player, const char *name);
cScorerItem *cScorer_AddItem(cScorer *scorer, const char *name, const char *type, byte flag);
void cScorer_Clear(cScorer *scorer);
void cScorer_SetDefault(cScorer *scorer, const char *name);
void cScorer_SetMarkerSprite(cScorer *scorer, const char *sprite);
/* the model of the new entry (never NULL) */
void *cScorer_AddModel(cScorer *scorer, const char *name, const char *model, const char *anim, int numAnim,
	float radius, float height, byte head);
void ScorerFade_Start(ScorerFade *fade, uint32_t from, uint32_t to, float time);
/* 0xAARRGGBB from the (r, g, b, a) values of the scripts (SScorer.Fade) */
/* SScorer.Get(player, name, var): 1 a text in *s, 2 a float in *d, 3 an int in *i, 0 (None) without the item or
 * variable */
int Scorer_GetItemVar(int player, const char *name, const char *var, char **s, double *d, int *i);
static inline uint32_t Scorer_PackColor(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
	return ((a << 8 | r) << 8 | g) << 8 | b;
}
void ScorerCinema_Set(ScorerCinema *cinema, byte on, float time);
/* cPyMenuEntity (type object): Python handle of a scorer item by player and item name */
typedef struct cPyMenuEntity {
	PyObject_HEAD
	int player;
	char *name;			/* Mem_Alloc copy */
} cPyMenuEntity;

/* the handle of a scorer item, NULL (no Python error) if there is no such item */
PyObject *ScorerItem_PyNew(int player, const char *name);

void Scorer_AddChatMsg(const char *text, uint32_t rgb);
void Scorer_SetConsole(int on);
void Scorer_SetSpeechText(const char *text, float time, int a, int b, int c);
struct nativeVal_s;
/* the talk callback by name (cPy_Function_SetFunc); the items of arg (a list value, NULL: none) that are a text, a
 * float or an int are its parameters */
void Scorer_SetSpeechCallback(const char *func, const struct nativeVal_s *arg);
void Scorer_CancelSpeech(float time);
void Scorer_SetHeadMonitor(const char *a, const char *b, int c);
void Scorer_SetMsgText(const char *text, float time, const char *font);
void Scorer_SetLabelText(const char *text, float time);
void Scorer_PreloadTexture(const char *name, int flag);
/* screen position of a world point into out[2], 0 off the screen; last argument 0 here */
byte Engine_WorldToScreen(const vec3_t *pos, float *out, float *depth);

#endif
