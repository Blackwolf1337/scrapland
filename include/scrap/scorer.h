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
