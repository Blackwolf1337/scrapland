/*
 * The scorer: HUD and menus of each local player (original source: Menu/). Scorer_Get(player) returns the scorer of
 * player 0..3 (0x104 bytes each) or NULL. Items are named HUD elements (cScorerItem and derived classes).
 */
#ifndef SCRAP_SCORER_H
#define SCRAP_SCORER_H

#include "scrap/game.h"
#include "scrap/mathlib.h"
#include "scrap/python.h"

/* a Python callback by name (cPy_Function, 0x14 bytes) */
typedef struct cPy_Function {
	byte data[0x14];
} cPy_Function;

typedef struct cScorerItem cScorerItem;
typedef struct cScorerItemVtbl {
	void *vf00_01[2];
	/* like cActClass_Var: set != 0 writes *s or *d / *i; returns 1 string, 2 double, 3 int, 0 */
	int(__thiscall *Var)(cScorerItem *self, const char *name, int set, char **s, double *d, int *i);	/* 0x8 */
} cScorerItemVtbl;

struct cScorerItem {
	const cScorerItemVtbl *vftable;
};

typedef struct cScorer {
	byte pad_00[0x34];
	byte specialHint[0x44 - 0x34];	/* 0x34 string object, cString_Set */
	byte visible;			/* 0x44 SScorer.Show/Hide */
	byte pad_45[3];
	char *actual;			/* 0x48 SScorer.GetActual */
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
/* the Python object of a scorer item */
PyObject *ScorerItem_PyNew(int player, const char *name);

void __thiscall cPy_Function_SetFunc(cPy_Function *f, const char *name);
void __thiscall cString_Set(void *str, const char *text);

void Scorer_AddChatMsg(const char *text, uint32_t rgb);
void Scorer_SetConsole(int on);
void Scorer_SetSpeechText(const char *text, float time, int a, int b, int c);
void Scorer_SetSpeechCallback(const char *func, PyObject *arg);
void Scorer_CancelSpeech(int arg);
void Scorer_SetHeadMonitor(const char *a, const char *b, int c);
void Scorer_SetMsgText(const char *text, float time, const char *font);
void Scorer_SetLabelText(const char *text, float time);
void Scorer_PreloadTexture(const char *name, int flag);
/* screen position of a world point; returns into out[2], last argument 0 here */
void Engine_WorldToScreen(const vec3_t *pos, float *out, int arg);
void *Font_Get(const char *name);
/* size of a printf text in the font: *size = width, height; x, y 0 here */
void Font_TextSize(void *font, float *rect, float *size, float x, float y, const char *fmt, ...);

#endif
