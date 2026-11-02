/*
 * Scorer.cpp of the game (Menu/): the scorers of the local players (cScorer), the base item cScorerItem, the scorer
 * models, the cursor moves, the screen effects (fade, cinema bars, noise, motion blur, clouds) and the full screen
 * console. See include/scrap/scorer.h and include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/astar.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/embeddedfxchar.h"
#include "scrap/fonts.h"
#include "scrap/itementity.h"
#include "scrap/missileentity.h"
#include "scrap/nodestraffic.h"
#include "scrap/nodesworld.h"
#include "scrap/nodesworld2d.h"
#include "scrap/pymempool.h"
#include "scrap/rendertarget.h"
#include "scrap/scoreritems.h"
#include "scrap/scrapmodel.h"
#include "scrap/sound.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"
#include "scrap/global.h"
#include "scrap/platform.h"
#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/aivehiclecontrol.h"
#include "scrap/collisiongrid.h"
#include "scrap/logic.h"
#include "scrap/objentity.h"
#include "scrap/scrap.h"
#include "scrap/utild3d.h"
#include "scrap/weapons.h"
#include "scrap/scorerhud.h"
#include "scrap/staticinit.h"

extern const cScorerVtbl cScorer_vftable;
#define CSCORER_VFTABLE (&cScorer_vftable)
extern const cScorerItemVtbl cScorerItem_vftable;
#define CSCORERITEM_VFTABLE (&cScorerItem_vftable)
#define SCORER_NO_ACCEPT -10.0f			/* cScorer.acceptTime off */

/* ---- construction ---- */

ScorerCinema *ScorerCinema_ctor(ScorerCinema *self) {
	self->time = 1.0f;
	self->life = 1.0f;
	self->on = 0;
	self->height = g_CinemaBlackBandHeight;
	return self;
}

ScorerFade *ScorerFade_ctor(ScorerFade *self) {
	for (int k = 0; k < 4; k++) {
		self->from[k] = 0.0f;
		self->to[k] = 0.0f;
	}
	self->time = 0.0f;
	self->life = 1.0f;
	return self;
}

/* the noise and motion blur: globals only */
void *ScorerFx_ctor(void *self) {
	g_NoiseFactor = 0;
	g_NoiseFactorBegin = 0;
	g_NoiseTime = 0.0f;
	g_NoiseEndTime = 0.0f;
	g_MotionBlurFactorBegin = 0.0f;
	g_MotionBlurFactorEnd = 0.0f;
	g_MotionBlurTime = 0.0f;
	g_MotionBlurEndTime = 0.0f;
	g_MotionBlurOffVar = 0.0f;
	return self;
}

char str_Empty[] = "";

cScorer *cScorer_ctor(cScorer *self) {
	self->vftable = CSCORER_VFTABLE;
	cString_ctor(&self->hint);
	cString_ctor(&self->specialHint);
	cString_ctor(&self->current);
	cString_ctor(&self->defaultItem);
	cPy_Function_ctor(&self->onCancel);
	cPy_Function_ctor(&self->onNext);
	cPy_Function_ctor(&self->onPrev);
	cPy_Function_ctor(&self->onSpecialHint);
	ScorerFade_ctor(&self->fade);
	ScorerCinema_ctor(&self->cinema);
	ScorerFx_ctor(&self->fx);
	cPy_Function_ctor(&self->mpFunc);
	self->items = NULL;
	cString_Set(&self->defaultItem, NULL);
	self->cursor = NULL;
	cString_Set(&self->current, NULL);
	self->visible = 1;
	cString_Set(&self->hint, str_Empty);
	cString_Set(&self->specialHint, str_Empty);
	self->hintTime = 0.0f;
	cPy_Function_SetFunc(&self->onCancel, NULL);
	cPy_Function_SetFunc(&self->onPrev, NULL);
	cPy_Function_SetFunc(&self->onNext, NULL);
	cPy_Function_SetFunc(&self->onSpecialHint, NULL);
	g_RadarMMSelect = 1;
	self->skipFrames = 2;
	g_BarValue = 0.0f;
	g_BarStopValue = 0.0f;
	g_BarSpeedFactor = 1.0f;
	g_CinemaBlackBandHeight = GVAR_DEF(float, g_CinemaBlackBandHeight);
	g_CameraScene = GVAR_DEF(int, g_CameraScene);
	cPy_Function_Clear(g_FullScreenConsoleCallback);
	g_BarAutoUpdate = 0;
	g_NumEnemiesInRadar = 0;
	self->time = 0.0f;
	self->lastTime = 0.0f;
	self->acceptTime = SCORER_NO_ACCEPT;
	self->models = NULL;
	cPy_Function_SetFunc(&self->mpFunc, NULL);
	self->hintWidth = 0xff;
	self->found = NULL;
	self->focusSound = 0;
	self->marker[0] = 320.0f;
	self->marker[1] = 240.0f;
	self->marker[2] = 320.0f;
	self->marker[3] = 240.0f;
	self->markerAlpha = 0.0f;
	g_MissionSolidTime = 0.0f;
	return self;
}

cScorerItem *cScorerItem_ctor(cScorerItem *self) {
	self->vftable = CSCORERITEM_VFTABLE;
	cString_ctor(&self->name);
	cString_ctor(&self->hint);
	cString_ctor(&self->left);
	cString_ctor(&self->right);
	cString_ctor(&self->up);
	cString_ctor(&self->down);
	cPy_Function_ctor(&self->onAccept);
	cPy_Function_ctor(&self->onDelete);
	cPy_Function_ctor(&self->onRender);
	cPy_Function_ctor(&self->onLooseFocus);
	cPy_Function_ctor(&self->onGainFocus);
	cString_Set(&self->name, NULL);
	self->x = 0.0f;
	self->y = 0.0f;
	self->w = 0.0f;
	self->h = 0.0f;
	self->focusPivotX = 0.0f;
	self->focusPivotY = 0.0f;
	self->delMe = 0;
	self->visible = 1;
	self->next = NULL;
	cString_Set(&self->down, str_Empty);
	cString_Assign(&self->up, &self->down);
	cString_Assign(&self->right, &self->up);
	cString_Assign(&self->left, &self->right);
	cString_Set(&self->hint, str_Empty);
	self->player = 0;
	self->multiPress = 0;
	self->maxHintLineWidth = 0xff;
	return self;
}

void cScorer_dtor(cScorer *self) {
	self->vftable = CSCORER_VFTABLE;
	cScorer_Free(self);
	cPy_Function_dtor(&self->mpFunc);
	cPy_Function_dtor(&self->onSpecialHint);
	cPy_Function_dtor(&self->onPrev);
	cPy_Function_dtor(&self->onNext);
	cPy_Function_dtor(&self->onCancel);
	cString_dtor(&self->defaultItem);
	cString_dtor(&self->current);
	cString_dtor(&self->specialHint);
	cString_dtor(&self->hint);
}

void cScorerItem_dtor(cScorerItem *self) {
	self->vftable = CSCORERITEM_VFTABLE;
	cPy_Function_dtor(&self->onGainFocus);
	cPy_Function_dtor(&self->onLooseFocus);
	cPy_Function_dtor(&self->onRender);
	cPy_Function_dtor(&self->onDelete);
	cPy_Function_dtor(&self->onAccept);
	cString_dtor(&self->down);
	cString_dtor(&self->up);
	cString_dtor(&self->right);
	cString_dtor(&self->left);
	cString_dtor(&self->hint);
	cString_dtor(&self->name);
}

/* the vector deleting destructor: flags 2 the array of new[] (its count before it), 1 frees */
cScorer *cScorer_Delete(cScorer *self, byte flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(cScorer), *mem, (void *)cScorer_dtor);
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return (cScorer *)mem;
	}
	cScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cScorer));
	}
	return self;
}

cScorerItem *cScorerItem_Delete(cScorerItem *self, byte flags) {
	cScorerItem_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cScorerItem));
	}
	return self;
}

/* ---- the models ---- */

void *cScorer_AddModel(cScorer *scorer, const char *name, const char *model, const char *anim, int numAnim,
	float radius, float height, byte head) {
	char file[0x200];
	const char *base;
	vec3_t pivot;

	CHECK(numAnim <= SCORER_MAX_HEAD_ANIMS);
	cScorerModel *m = operator_new(sizeof(cScorerModel));
	if (m) {
		CGameModel_ctor(&m->model, NULL);
	}
	Str_PrintfQuiet(file, sizeof(file), "%s.M3D", model);
	CGameModel_LoadName(&m->model, file);
	pivot.x = 0.5f;
	pivot.y = 0.5f;
	pivot.z = 0.5f;
	CGameModel_SetPivot(&m->model, &pivot, 1, 0);
	CGameModel_Disable(&m->model);
	m->radius = radius;
	m->height = height;
	m->numAnims = numAnim;
	base = anim ? anim : model;
	if (head) {
		Str_PrintfQuiet(file, sizeof(file), "%s_Relax.M3D", base);
		m->anims[0] = CGameModel_AddAnim(&m->model, file);
		Str_PrintfQuiet(file, sizeof(file), "%s_Talk.M3D", base);
		m->anims[1] = CGameModel_AddAnim(&m->model, file);
		for (int k = 0; k < numAnim - 2; k++) {
			Str_PrintfQuiet(file, sizeof(file), "%s_Talk%d.M3D", base, k);
			m->anims[2 + k] = CGameModel_AddAnim(&m->model, file);
		}
	} else {
		for (int k = 0; k < numAnim; k++) {
			Str_PrintfQuiet(file, sizeof(file), "%s.anim%d.M3D", base, k);
			m->anims[k] = CGameModel_AddAnim(&m->model, file);
		}
	}
	Str_Copy(m->name, name, sizeof(m->name));
	m->next = scorer->models;
	scorer->models = m;
	m->extra = EmbeddedFXCharHead_Create(&m->model);
	return &m->model;
}

cScorerModel *cScorer_FindModel(cScorer *scorer, const char *name) {
	for (cScorerModel *m = scorer->models; m; m = m->next) {
		if (!strcmp(m->name, name)) {
			return m;
		}
	}
	return NULL;
}

CGameModel *cScorer_FindModelModel(cScorer *scorer, const char *name) {
	for (cScorerModel *m = scorer->models; m; m = m->next) {
		if (!strcmp(m->name, name)) {
			return &m->model;
		}
	}
	return NULL;
}

/* ---- the items ---- */

void cScorer_Clear(cScorer *scorer) {
	scorer->found = NULL;
	cString_Set(&scorer->defaultItem, NULL);
	scorer->cursor = NULL;
	cString_Set(&scorer->current, NULL);
	scorer->visible = 1;
	cPy_Function_SetFunc(&scorer->onCancel, NULL);
	cPy_Function_SetFunc(&scorer->onPrev, NULL);
	cPy_Function_SetFunc(&scorer->onNext, NULL);
	for (cScorerItem *it = scorer->items; it; it = it->next) {
		it->delMe = 1;
	}
	scorer->cleared = 1;
	scorer->acceptTime = SCORER_NO_ACCEPT;
	scorer->focusSound = 0;
}

/* the items and the models deleted */
void cScorer_Free(cScorer *scorer) {
	cString_Set(&scorer->defaultItem, NULL);
	scorer->cursor = NULL;
	cString_Set(&scorer->current, NULL);
	scorer->visible = 1;
	scorer->found = NULL;
	cPy_Function_SetFunc(&scorer->onCancel, NULL);
	cPy_Function_SetFunc(&scorer->onPrev, NULL);
	cPy_Function_SetFunc(&scorer->onNext, NULL);
	while (scorer->items) {
		cScorerItem *it = scorer->items;
		cScorerItem *next = it->next;
		if (it) {
			it->vftable->Delete(it, 1);
		}
		scorer->items = next;
	}
	while (scorer->models) {
		cScorerModel *next = scorer->models->next;
		cEmbeddedFX *fx = scorer->models->extra;
		if (fx) {
			fx->vftable->Release(fx);
			fx = scorer->models->extra;
			if (fx) {
				fx->vftable->Delete(fx, 1);
				scorer->models->extra = NULL;
			}
		}
		cScorerModel *m = scorer->models;
		if (m) {
			CGameModel_Reset(&m->model);
			operator_delete_sized(m, sizeof(cScorerModel));
		}
		scorer->models = next;
	}
}

cScorerItem *cScorer_FindItem(cScorer *scorer, const char *name) {
	cScorerItem *it;

	if (scorer->found && cString_EqStr(&scorer->found->name, name)) {
		return scorer->found;
	}
	for (it = scorer->items; it; it = it->next) {
		if (!it->delMe && !cString_ICmpStr(&it->name, name)) {
			break;
		}
	}
	scorer->found = it;
	return it;
}

/* SetParam(name, value) of every item */
void cScorer_SetParam(cScorer *scorer, const char *name, const char *value) {
	for (cScorerItem *it = scorer->items; it; it = it->next) {
		it->vftable->SetParam(it, name, value);
	}
}

/* the items marked DelMe deleted */
void cScorer_RemoveDeleted(cScorer *scorer, int player) {
	cScorerItem **link = &scorer->items;

	(void)player;
	while (*link) {
		cScorerItem *it = *link;
		if (it->delMe) {
			*link = it->next;
			if (scorer->cursor == it) {
				scorer->cursor = NULL;
			}
			if (it == scorer->found) {
				scorer->found = NULL;
			}
			if (it) {
				it->vftable->Delete(it, 1);
			}
		} else {
			link = &it->next;
		}
	}
}

/* a new item of the class type, first in the list or last (append) */
cScorerItem *cScorer_AddItem(cScorer *scorer, const char *name, const char *type, byte append) {
	/* the item classes of SScorer.Add by type name, in the order the original compares them */
	const struct {
		const char *type;
		uint32_t size;
		void *ctor;
	} s_ItemTypes[] = {
		{"Hint", sizeof(cHintScorer), (void *)cHintScorer_ctor},
		{"Label", sizeof(cLabelScorer), (void *)cLabelScorer_ctor},
		{"Msg", sizeof(cMsgScorer), (void *)cMsgScorer_ctor},
		{"Timer", sizeof(cTimerScorer), (void *)cTimerScorer_ctor},
		{"Talk", sizeof(cTalkScorer), (void *)cTalkScorer_ctor},
		{"Chat", sizeof(cChatScorer), (void *)cChatScorer_ctor},
		{"WindFx", sizeof(cWindFxScorer), (void *)cWindFxScorer_ctor},
		{"Bar", sizeof(cBarScorer), (void *)cBarScorer_ctor},
		{"Cannon", sizeof(cCannonScorer), (void *)cCannonScorer_ctor},
		{"Monitor", sizeof(cMonitorScorer), (void *)cMonitorScorer_ctor},
		{"Target", sizeof(cTargetScorer), (void *)cTargetScorer_ctor},
		{"Waypoint", sizeof(cWaypointScorer), (void *)cWaypointScorer_ctor},
		{"Radar", sizeof(cRadarScorer), (void *)cRadarScorer_ctor},
		{"Sprite", sizeof(cSpriteScorer), (void *)cSpriteScorer_ctor},
		{"Text", sizeof(cTextScorer), (void *)cTextScorer_ctor},
		{"NetStats", sizeof(cNetStatsScorer), (void *)cNetStatsScorer_ctor},
		{"Weapon", sizeof(cWeaponScorer), (void *)cWeaponScorer_ctor},
		{"Status", sizeof(cStatusScorer), (void *)cStatusScorer_ctor},
		{"CopMeter", sizeof(cCopMeterScorer), (void *)cCopMeterScorer_ctor},
		{"Button", sizeof(cButtonScorer), (void *)cButtonScorer_ctor},
		{"SpecialAction", sizeof(cSpecialActionScorer), (void *)cSpecialActionScorer_ctor},
		{"HangarShip", sizeof(cHangarShipScorer), (void *)cHangarShipScorer_ctor},
		{"Circuit", sizeof(cCircuitScorer), (void *)cCircuitScorer_ctor},
		{"Tab", sizeof(cTabScorer), (void *)cTabScorer_ctor},
		{"Slider", sizeof(cSliderScorer), (void *)cSliderScorer_ctor},
		{"Head", sizeof(cHeadScorer), (void *)cHeadScorer_ctor},
		{"Mission", sizeof(cMissionScorer), (void *)cMissionScorer_ctor},
		{"Credits", sizeof(cCreditsScorer), (void *)cCreditsScorer_ctor},
		{"TextTyping", sizeof(cTextTypingScorer), (void *)cTextTypingScorer_ctor},
		{"ScraplandMainMenu", sizeof(cScraplandMainMenuScorer), (void *)cScraplandMainMenuScorer_ctor},
		{"ScraplandSubMenu", sizeof(cScraplandSubMenuScorer), (void *)cScraplandSubMenuScorer_ctor},
		{"Gradient", sizeof(cGradientScorer), (void *)cGradientScorer_ctor},
	};
	cScorerItem *it = NULL;
	uint32_t k;

	if (cScorer_FindItem(scorer, name)) {
		Con_Debugf("Item '%s' duplicated at scorer\n", name);
		return NULL;
	}
	for (k = 0; k < sizeof(s_ItemTypes) / sizeof(s_ItemTypes[0]); k++) {
		if (!crt_stricmp(type, s_ItemTypes[k].type)) {
			break;
		}
	}
	if (k == sizeof(s_ItemTypes) / sizeof(s_ItemTypes[0])) {
		return NULL;
	}
	void *mem = operator_new(s_ItemTypes[k].size);
	if (mem) {
		it = ((cScorerItem * (*)(void *)) s_ItemTypes[k].ctor)(mem);
	}
	cString_Set(&it->name, name);
	it->player = scorer->player;
	if (!append) {
		it->next = scorer->items;
		scorer->items = it;
		return it;
	}
	cScorerItem *last = scorer->items;
	for (cScorerItem *p = last; p; p = p->next) {
		last = p;
	}
	if (last) {
		last->next = it;
	} else {
		scorer->items = it;
	}
	it->next = NULL;
	return it;
}

int cScorerItem_Var(cScorerItem *self, const char *name, int set, char **s, double *d, int *i) {
	static const char *const strNames[4] = {"Left", "Right", "Up", "Down"};
	static const char *const funcNames[5] = {"OnLooseFocus", "OnGainFocus", "OnAccept", "OnDelete", "OnRender"};
	static const char *const floatNames[6] = {"X", "Y", "W", "H", "FocusPivotX", "FocusPivotY"};
	char **strs[4] = {&self->left, &self->right, &self->up, &self->down};
	cPy_Function *funcs[5] = {&self->onLooseFocus, &self->onGainFocus, &self->onAccept, &self->onDelete,
		&self->onRender};

	if (s && !crt_stricmp(name, "Hint")) {
		if (set) {
			cString_Set(&self->hint, *s);
		} else {
			*s = self->hint;
		}
		return 1;
	}
	if (i && !crt_stricmp(name, "MaxHintLineWidth")) {
		return ScorerVar_Int(&self->maxHintLineWidth, set, i);
	}
	if (!crt_stricmp(name, "Name") && !set && s) {
		*s = self->name;
		return 1;
	}
	if (!crt_stricmp(name, "DelMe") && set && i && *i == 1) {
		self->delMe = 1;
		return 3;
	}
	if (d) {
		for (int k = 0; k < 6; k++) {
			if (!crt_stricmp(name, floatNames[k])) {
				return ScorerVar_Float(&self->x + k, set, d);
			}
		}
	}
	if (s) {
		for (int k = 0; k < 4; k++) {
			if (!crt_stricmp(name, strNames[k])) {
				if (set) {
					cString_Set(strs[k], *s);
				} else {
					*s = *strs[k];
				}
				return 1;
			}
		}
	}
	if (set && s) {
		for (int k = 0; k < 5; k++) {
			if (!crt_stricmp(name, funcNames[k])) {
				cPy_Function_SetFunc(funcs[k], *s);
				return 1;
			}
		}
	}
	if (i) {
		if (!crt_stricmp(name, "MultiPress")) {
			return ScorerVar_Bool(&self->multiPress, set, i);
		}
		if (!crt_stricmp(name, "Visible")) {
			return ScorerVar_Bool(&self->visible, set, i);
		}
	}
	return 0;
}

/* the hint of the item, or the special hint while onSpecialHint returns 1 */
static void cScorer_TakeHint(cScorer *scorer, cScorerItem *it) {
	char **src = &it->hint;

	if (cPy_Function_IsValid(&scorer->onSpecialHint) && cPy_Function_Execute(&scorer->onSpecialHint, 0) == 1) {
		src = &scorer->specialHint;
	}
	cString_Assign(&scorer->hint, src);
}

/* the cursor moves from the item from to the item to (names, NULL for none) */
void cScorer_SetCursorItem(cScorer *scorer, int player, const char *from, const char *to) {
	cScorerItem *it;

	scorer->acceptTime = SCORER_NO_ACCEPT;
	if (from && (it = cScorer_FindItem(scorer, from)) && cPy_Function_IsValid(&it->onLooseFocus)) {
		cPy_Function_AddParamI(&it->onLooseFocus, player);
		cPy_Function_AddParamS(&it->onLooseFocus, from);
		cPy_Function_AddParamS(&it->onLooseFocus, to ? to : str_Empty);
		cPy_Function_Execute(&it->onLooseFocus, 0);
	}
	if (!to) {
		cString_Set(&scorer->hint, str_Empty);
		return;
	}
	it = cScorer_FindItem(scorer, to);
	if (it && cPy_Function_IsValid(&it->onGainFocus)) {
		cPy_Function_AddParamI(&it->onGainFocus, player);
		cPy_Function_AddParamS(&it->onGainFocus, to);
		cPy_Function_AddParamS(&it->onGainFocus, from);
		cPy_Function_Execute(&it->onGainFocus, 0);
	}
	scorer->hintTime = scorer->time;
	if (!it || !cPy_Function_IsValid(&it->onAccept)) {
		cString_Set(&scorer->hint, str_Empty);
		return;
	}
	cScorer_TakeHint(scorer, it);
	scorer->hintWidth = it->maxHintLineWidth;
	if (scorer->focusSound) {
		Scorer_PlaySound(g_OnFocusSound, g_OnFocusSoundVol);
	}
}

void cScorer_SetDefault(cScorer *scorer, const char *name) {
	cString_Set(&scorer->defaultItem, name);
	if (scorer->defaultItem != scorer->current) {
		cScorer_SetCursorItem(scorer, scorer->player, scorer->current, scorer->defaultItem);
		scorer->acceptTime = SCORER_NO_ACCEPT;
	}
	cString_Set(&scorer->current, name);
}

cScorer *g_Scorers;

/* ---- the scorers of the players ---- */

cScorer *Scorer_Get(int player) {
	if (player < 4 && player >= 0) {
		return &g_Scorers[player];
	}
	return NULL;
}

void Scorer_SetInputState(int state) {
	cScorer *sc = g_Scorers;

	sc[0].skipFrames = state;
	sc[1].skipFrames = state;
	sc[2].skipFrames = state;
	sc[3].skipFrames = state;
}

CPsaImage *g_ScorerWhite;

CPsaImage *Scorer_GetWhite(void) {
	return g_ScorerWhite;
}

CPsaImage *g_ScorerMarker;

CPsaImage *Scorer_GetMarker(void) {
	return g_ScorerMarker;
}

CPsaImage *g_ScorerNoise;

CPsaImage *Scorer_GetNoise(void) {
	return g_ScorerNoise;
}

/* a new image of the sheet name (CPsaImage_ctorTex), NULL if the allocation fails */
static CPsaImage *Scorer_NewTex(const char *name, const char *a, int flags) {
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	return img ? CPsaImage_ctorTex(img, name, a, flags) : NULL;
}

cNewsPanel *g_NewsPanel;

void Scorer_Init(void) {
	g_ScorerWhite = Scorer_NewTex("2D/white.tga", 0, 0x24);
	g_ScorerNoise = Scorer_NewTex("2D/Noise.tga", 0, 0x34);
	g_ScorerMarker = Scorer_NewTex("2D/Marker.alpha.tga", 0, 0x20);
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	g_ScorerTabs = img ? CPsaImage_ctorFlags(img, "2D/Menu/Tabs.alpha.tga", 0, 6, 0x10060) : NULL;
	CHECK(g_Scorers == NULL);
	uint32_t *mem = operator_new_array(4 * sizeof(cScorer) + 4);
	cScorer *sc = NULL;
	if (mem) {
		sc = (cScorer *)(mem + 1);
		*mem = 4;
		crt_eh_vector_ctor(sc, sizeof(cScorer), 4, (void *)cScorer_ctor, (void *)cScorer_dtor);
	}
	g_Scorers = sc;
	sc[0].player = 0;
	sc[1].player = 1;
	sc[2].player = 2;
	sc[3].player = 3;
	cString_Set(&g_FileNameMap2D, "2D/Black.tga");
	Scorer_SetSpeechText(str_Empty, 0.0f, 0xff, 0xff, 0xff);
	Scorer_SetSpeechCallback(str_Empty, NULL);
	Scorer_SetHeadMonitor(str_Empty, str_Empty, 1);
	if (!(GVAR_FLAGS(g_CurrentWaypointName) & 1)) {
		cString_Set(&g_CurrentWaypointName, str_Empty);
	}
	if (!(GVAR_FLAGS(g_CurrentWaypointName2) & 1)) {
		cString_Set(&g_CurrentWaypointName2, str_Empty);
	}
	if (!(GVAR_FLAGS(g_CameraCloudDistance) & 1)) {
		g_CameraCloudDistance = GVAR_DEF(float, g_CameraCloudDistance);
	}
	if (!(GVAR_FLAGS(g_CameraCloudPlaneSize) & 1)) {
		g_CameraCloudPlaneSize = GVAR_DEF(float, g_CameraCloudPlaneSize);
	}
	if (!(GVAR_FLAGS(g_CameraCloudThickness) & 1)) {
		g_CameraCloudThickness = GVAR_DEF(float, g_CameraCloudThickness);
	}
	if (!(GVAR_FLAGS(g_CameraCloudAlpha) & 1)) {
		g_CameraCloudAlpha = GVAR_DEF(int, g_CameraCloudAlpha);
	}
	if (!(GVAR_FLAGS(g_CameraCloudRed) & 1)) {
		g_CameraCloudRed = GVAR_DEF(int, g_CameraCloudRed);
	}
	if (!(GVAR_FLAGS(g_CameraCloudGreen) & 1)) {
		g_CameraCloudGreen = GVAR_DEF(int, g_CameraCloudGreen);
	}
	if (!(GVAR_FLAGS(g_CameraCloudBlue) & 1)) {
		g_CameraCloudBlue = GVAR_DEF(int, g_CameraCloudBlue);
	}
	Scorer_SetMsgText(str_Empty, 0.0f, "Horatio");
	Scorer_SetLabelText(str_Empty, 0.0f);
	cNewsPanel *np = operator_new(sizeof(cNewsPanel));
	np = np ? cNewsPanel_ctor(np) : NULL;
	g_NewsPanel = np;
	cNewsPanel_Reset(np);
}

static void Scorer_FreeImage(CPsaImage *img) {
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, sizeof(CPsaImage));
	}
}

void Scorer_Done(void) {
	cScorer *sc = g_Scorers;

	if (!sc) {
		CHECK(g_Scorers != NULL);
		sc = g_Scorers;
	}
	if (sc) {
		if (((uint32_t *)sc)[-1]) {
			sc->vftable->Delete(sc, 3);
		} else {
			operator_delete_array((uint32_t *)sc - 1);
		}
	}
	g_Scorers = NULL;
	Scorer_SetConsole(0);
	cString_Set(&g_FileNameMap2D, "2D/Black.tga");
	Scorer_FreeImage(g_ScorerWhite);
	Scorer_FreeImage(g_ScorerNoise);
	Scorer_FreeImage(g_ScorerMarker);
	Scorer_FreeImage(g_ScorerTabs);
	cNewsPanel *np = g_NewsPanel;
	if (np) {
		cNewsPanel_dtor(np);
		operator_delete_sized(np, sizeof(cNewsPanel));
	}
	g_NewsPanel = NULL;
}

void Scorer_PreloadTexture(const char *name, int flag) {
	CPsaImage img;

	CPsaImage_ctor(&img, name, (byte)flag ? 0x40 : 0);
	CPsaImage_dtor(&img);
}

void cScorer_SetMarkerSprite(cScorer *scorer, const char *sprite) {
	(void)scorer;
	if (g_ScorerMarker) {
		Scorer_FreeImage(g_ScorerMarker);
		g_ScorerMarker = NULL;
	}
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	if (!img) {
		g_ScorerMarker = NULL;
		return;
	}
	if (!sprite || !*sprite) {
		sprite = "2D/Marker.alpha.tga";
	}
	g_ScorerMarker = CPsaImage_ctorTex(img, sprite, 0, 0x20);
}

/* the text of the entity variable name of the player's camera target: an int ("%d"), a float ("%.0f") or a
 * string, buf unchanged if there is none */
void Scorer_GetCamVarStr(int player, char *buf, const char *name, int size) {
	const char *camName = cWorld_GetCamName(g_pWorld, player);
	cEntity *e;
	int iv;
	double dv;
	char *sv;

	if (!*camName || !(e = World_FindEntity(camName)) || !e->vftable->IsType(e, FCAM_CLASSNAME)
		|| !(e = cCamEntity_GetTarget(e)) || !e->vftable->IsType(e, TYEN_WITH_LIFE)) {
		return;
	}
	if (e->vftable->VarI(e, name, &iv, 0)) {
		Str_PrintfQuiet(buf, size, "%d", iv);
		return;
	}
	if (e->vftable->VarF(e, name, &dv, 0)) {
		Str_PrintfQuiet(buf, size, "%.0f", dv);
		return;
	}
	if (e->vftable->VarS(e, name, &sv, 0)) {
		Str_Copy(buf, sv, size);
	}
}

/* the int (or the float cut) of the entity variable name of the player's camera target, 0 if none */
int Scorer_GetCamVarInt(int player, const char *name) {
	const char *camName;
	cEntity *e;
	int iv;
	double dv;

	if (!*name) {
		return 0;
	}
	camName = cWorld_GetCamName(g_pWorld, player);
	e = NULL;
	if (*camName) {
		cEntity *cam = World_FindEntity(camName);
		if (cam) {
			if (!cam->vftable->IsType(cam, FCAM_CLASSNAME)) {
				return 0;
			}
			e = cCamEntity_GetTarget(cam);
			if (!e || !e->vftable->IsType(e, TYEN_WITH_LIFE)) {
				return 0;
			}
		}
	}
	if (e->vftable->VarI(e, name, &iv, 0)) {
		return iv;
	}
	if (!e->vftable->VarF(e, name, &dv, 0)) {
		return 0;
	}
	return (int)dv;
}

void Scorer_DrawPyMemStats(void) {
	cMemStats *ms = (&g_PyMemStats);

	CFont_PrintfAlign(g_FontHoratioSmall, 48.0f, 40.0f, 0, "%d Bytes. (%d)top", ms->bytes, ms->peakBytes);
	CFont_PrintfAlign(g_FontHoratioSmall, 48.0f, 60.0f, 0, "%d Blocks. (%d)top", ms->blocks, ms->peakBlocks);
	CFont_PrintfAlign(g_FontHoratioSmall, 48.0f, 80.0f, 0, "%d NumAlloc.", ms->allocs);
	CFont_PrintfAlign(g_FontHoratioSmall, 48.0f, 100.0f, 0, "%d NumFree.", ms->frees);
}

/* ---- the screen effects ---- */

/* the color a + (b - a) * t per byte, a for t <= 0, b for t >= 1; the differences are or-ed into one word as the
 * original does (a negative one spills into the higher bytes) */
uint32_t Scorer_ColorLerp(uint32_t a, uint32_t b, float t) {
	if (t <= 0.0f) {
		return a;
	}
	if (t >= 1.0f) {
		return b;
	}
	uint32_t a0 = a & 0xff, a1 = a >> 8 & 0xff, a2 = a >> 16 & 0xff, a3 = a >> 24;
	uint32_t d = ((b >> 16 & 0xff) - a2) << 16 | ((b >> 8 & 0xff) - a1) << 8 | ((b >> 24) - a3) << 24
		| ((b & 0xff) - a0);
	uint32_t r = (uint32_t)((int)((float)(int)(d >> 16 & 0xff) * t) + a2) << 16;
	r |= (uint32_t)((int)((float)(int)(d >> 8 & 0xff) * t) + a1) << 8;
	return (uint32_t)((int)((float)(int)(d >> 24) * t) + a3) << 24 | r
		| (uint32_t)((int)((float)(int)(d & 0xff) * t) + a0);
}

void ScorerFade_Start(ScorerFade *fade, uint32_t from, uint32_t to, float time) {
	fade->from[0] = (float)(int)(from >> 16 & 0xff);
	fade->from[1] = (float)(int)(from >> 8 & 0xff);
	fade->from[2] = (float)(int)(from & 0xff);
	fade->from[3] = (float)(int)(from >> 24);
	fade->to[0] = (float)(int)(to >> 16 & 0xff);
	fade->to[1] = (float)(int)(to >> 8 & 0xff);
	fade->to[2] = (float)(int)(to & 0xff);
	fade->to[3] = (float)(int)(to >> 24);
	fade->time = time;
	fade->life = time;
}

void ScorerFade_Draw(ScorerFade *fade, int player, float dt) {
	(void)player;
	fade->time = fade->time - dt;
	if (0.0f > fade->time) {
		fade->time = 0.0f;
	}
	float f = fade->time / fade->life;
	float g = 1.0f - f;
	int alpha = (int)(fade->to[3] * g + fade->from[3] * f);
	if (alpha) {
		int r = (int)(fade->to[0] * g + fade->from[0] * f);
		int gr = (int)(fade->to[1] * g + fade->from[1] * f);
		int b = (int)(fade->to[2] * g + fade->from[2] * f);
		CPsaImage *img = g_ScorerWhite;
		img->scaleX = 640.0f;
		img->scaleY = 480.0f;
		img->color = (((uint32_t)alpha << 8 | (uint32_t)r) << 8 | (uint32_t)gr) << 8 | (uint32_t)b;
		CPsaImage_Draw(img, 0.0f, 0.0f);
	}
}

void ScorerCinema_Set(ScorerCinema *cinema, byte on, float time) {
	CHECK(time >= 0);
	if (time == 0.0f) {
		cinema->time = 1.0f;
		cinema->life = 1.0f;
		cinema->on = on;
		return;
	}
	if (cinema->on != on) {
		cinema->on = on;
		float f = 1.0f - cinema->time / cinema->life;
		cinema->life = time;
		cinema->time = (3.0f - (f + f)) * (f * f) * time;
	}
}

void ScorerCinema_Draw(ScorerCinema *cinema, int player, float dt) {
	float life = cinema->life;
	float t = cinema->time + dt;
	float f, h;

	(void)player;
	cinema->time = t;
	if (t >= life) {
		cinema->time = life;
	}
	if (cinema->on) {
		cinema->height = g_CinemaBlackBandHeight;
		f = cinema->time / life;
		if (g_CameraScene == 1) {
			float ar = g_PhysicalAspectRatio;
			if (1.0f > ar) {
				cinema->height = 0.0f;
			} else {
				cinema->height = (1.7777778f - ar) / 0.44444442f * cinema->height;
			}
		}
	} else {
		f = 1.0f - cinema->time / life;
	}
	h = (3.0f - (f + f)) * (f * f) * cinema->height;
	if (h > 1.0f) {
		CPsaImage *img = g_ScorerWhite;
		img->scaleX = 640.0f;
		img->scaleY = h;
		img->color = 0xff000000;
		CPsaImage_Draw(img, 0.0f, 0.0f);
		CPsaImage_Draw(g_ScorerWhite, 0.0f, 480.0f - h);
	}
}

/* the noise over the screen and the motion blur of the scene */
void ScorerFx_Draw(void *fx, int player, float dt) {
	float f = 1.0f;
	float v;
	int n;

	(void)fx;
	if (g_NoiseEndTime > 0.0f) {
		float t = g_NoiseTime + dt;
		g_NoiseTime = t;
		t = t / g_NoiseEndTime;
		if (t >= 1.0f) {
			g_NoiseTime = 0.0f;
			g_NoiseEndTime = 0.0f;
		} else {
			f = (3.0f - (t + t)) * (t * t);
		}
	}
	n = (int)((1.0f - f) * (float)g_NoiseFactorBegin + (float)g_NoiseFactor * f);
	if (n > 1) {
		double r0 = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
		int r1 = crt_rand() & 0x7fff;
		CPsaImage *img = g_ScorerNoise;
		double r1d = (double)r1 * RAND_SCALE;
		float u = (float)(r0 + r0 + 0.0);
		float w = (float)(r1d + r1d + 0.0);
		img->uv0[1] = u;
		img->uv0[0] = w;
		img->uv1[0] = w + 2.0f;
		img->uv1[1] = u + 2.0f;
		img->scaleX = 320.0f;
		img->scaleY = 240.0f;
		img->color = (uint32_t)n << 24 | 0xffffff;
		CPsaImage_Draw(img, 0.0f, 0.0f);
	}
	v = g_MotionBlurFactorEnd;
	if (g_MotionBlurEndTime > 0.0f) {
		float t = g_MotionBlurTime + dt;
		g_MotionBlurTime = t;
		t = t / g_MotionBlurEndTime;
		if (t >= 1.0f) {
			g_MotionBlurTime = 0.0f;
			g_MotionBlurEndTime = 0.0f;
		} else {
			float b = g_MotionBlurFactorBegin;
			if (v >= b) {
				v = (v - b) * t + b;
			} else {
				v = b - (b - v) * t;
			}
		}
	}
	g_SceneMotionBlurValue = v;
	if (v > 0.0f && g_MotionBlurOffVar > 0.0f && cTimer_GetSpeed(&g_pWorld->timer) > 0.0f) {
		cScorer *sc = Scorer_Get(player);
		double c = cos((double)(sc->time + sc->time));
		g_SceneMotionBlurOff = (float)(c * (double)(g_MotionBlurOffVar * v));
		return;
	}
	g_SceneMotionBlurOff = 0.0f;
}

/* the MPFunc of the scorer with (player, 1) on the press and (player, 0) on the release of the action 23 of the
 * character set */
void cScorer_UpdateMP(cScorer *scorer, float dt) {
	(void)dt;
	if (Input_GetActionSet() != 1) {
		return;
	}
	if (cInputAction_Pressed(TABLE_AT(TABLE_AT(g_ActionSets, 1)->actions, 23), scorer->player)) {
		cPy_Function_AddParamI(&scorer->mpFunc, scorer->player);
		cPy_Function_AddParamI(&scorer->mpFunc, 1);
		cPy_Function_Execute(&scorer->mpFunc, 0);
	}
	if (cInputAction_Released(TABLE_AT(TABLE_AT(g_ActionSets, 1)->actions, 23), scorer->player)) {
		cPy_Function_AddParamI(&scorer->mpFunc, scorer->player);
		cPy_Function_AddParamI(&scorer->mpFunc, 0);
		cPy_Function_Execute(&scorer->mpFunc, 0);
	}
}

/* the actions of the menu set (0) */
#define MENU_CANCEL 0
#define MENU_UP_AXIS 2			/* the mouse and pad axes of the cursor */
#define MENU_DOWN_AXIS 3
#define MENU_LEFT_AXIS 4
#define MENU_RIGHT_AXIS 5
#define MENU_UP 6			/* the moves between the items */
#define MENU_DOWN 7
#define MENU_LEFT 8
#define MENU_RIGHT 9
#define MENU_ACCEPT 10
#define MENU_PREV 11
#define MENU_NEXT 12
#define MENU_DELETE 13
#define SCORER_ACCEPT_REPEAT 0.65f	/* MultiPress: the first repeat of a held accept, then every 0.07 s */

static inline cInputAction *Menu_Action(int i) {
	return TABLE_AT(TABLE_AT(g_ActionSets, 0)->actions, i);
}

/* the item under the point of the cursor item, NULL if none */
static cScorerItem *cScorer_ItemAt(cScorer *scorer, const cScorerItem *cur) {
	for (cScorerItem *it = scorer->items; it; it = it->next) {
		if (it->visible && it->w > 0.0f && it->h > 0.0f) {
			float x = it->x, fpx = it->focusPivotX;
			if (cur->x > x + fpx && x + it->w + fpx > cur->x) {
				float y = it->y, fpy = it->focusPivotY;
				if (cur->y > y + fpy && y + it->h + fpy > cur->y) {
					return it;
				}
			}
		}
	}
	return NULL;
}

/* a function of the scorer with (player, actual), the accept sound after it */
static void cScorer_Call(cScorer *scorer, cPy_Function *f, int player) {
	cPy_Function_AddParamI(f, player);
	cPy_Function_AddParamS(f, scorer->current);
	cPy_Function_Execute(f, 0);
	Scorer_PlaySound(g_OnAcceptSound, g_OnAcceptSoundVol);
	scorer->focusSound = 0;
}

/* the cursor item moved by the axes and the item moves by the keys, the accept, delete, cancel, next and prev
 * functions, the cursor blinking */
void cScorer_UpdateCursor(cScorer *scorer, int player) {
	cScorerItem *it;
	cScorerItem *cur;
	float dy = (Menu_Action(MENU_DOWN_AXIS)->value[player] - Menu_Action(MENU_UP_AXIS)->value[player])
		* scorer->frameTime * 480.0f;
	float dx = (Menu_Action(MENU_RIGHT_AXIS)->value[player] - Menu_Action(MENU_LEFT_AXIS)->value[player])
		* scorer->frameTime * 640.0f;
	byte up, down, left, right;

	scorer->cursor->y = dy + scorer->cursor->y;
	scorer->cursor->x = dx + scorer->cursor->x;
	if (scorer->cursor->x > 640.0f) {
		scorer->cursor->x = 640.0f;
	}
	if (0.0f > scorer->cursor->x) {
		scorer->cursor->x = 0.0f;
	}
	if (scorer->cursor->y > 480.0f) {
		scorer->cursor->y = 480.0f;
	}
	if (0.0f > scorer->cursor->y) {
		scorer->cursor->y = 0.0f;
	}
	if (__builtin_fabs((double)dy) > (double)0.0001f || __builtin_fabs((double)dx) > (double)0.0001f) {
		scorer->cursor->visible = 1;
	}
	cur = scorer->cursor;
	if (cur->visible) {
		char *name = NULL;
		it = cScorer_ItemAt(scorer, cur);
		if (it) {
			if (cString_Cmp(&scorer->current, &it->name)) {
				cScorer_SetCursorItem(scorer, player, scorer->current, it->name);
			}
			name = it->name;
		}
		if (!name && scorer->current) {
			cScorer_SetCursorItem(scorer, player, scorer->current, NULL);
		}
		if (!cString_EqStr(&scorer->current, name)) {
			cString_Set(&scorer->current, name);
		}
	}
	up = cInputAction_Pressed(Menu_Action(MENU_UP), player);
	down = cInputAction_Pressed(Menu_Action(MENU_DOWN), player);
	left = cInputAction_Pressed(Menu_Action(MENU_LEFT), player);
	right = cInputAction_Pressed(Menu_Action(MENU_RIGHT), player);
	if (up || down || left || right) {
		scorer->cursor->visible = 0;
		if (!scorer->current && scorer->defaultItem) {
			cScorer_SetCursorItem(scorer, player, NULL, scorer->defaultItem);
			cString_Assign(&scorer->current, &scorer->defaultItem);
		} else {
			for (it = scorer->items; it; it = it->next) {
				if (!it->visible || !(it->w > 0.0f) || !(it->h > 0.0f)) {
					continue;
				}
				if (!scorer->current) {
					cString_Assign(&scorer->current, &it->name);
					break;
				}
				if (!cString_Eq(&scorer->current, &it->name)) {
					continue;
				}
				cString_Assign(&scorer->current, &it->name);
				if (up && cString_CmpStr(&it->up, str_Empty)) {
					cString_Assign(&scorer->current, &it->up);
				}
				if (down && cString_CmpStr(&it->down, str_Empty)) {
					cString_Assign(&scorer->current, &it->down);
				}
				if (left && cString_CmpStr(&it->left, str_Empty)) {
					cString_Assign(&scorer->current, &it->left);
				}
				if (right && cString_CmpStr(&it->right, str_Empty)) {
					cString_Assign(&scorer->current, &it->right);
				}
				if (cString_Cmp(&scorer->current, &it->name)) {
					cScorer_SetCursorItem(scorer, player, it->name, scorer->current);
				}
				break;
			}
		}
		it = cScorer_FindItem(scorer, scorer->current);
		if (it) {
			scorer->cursor->x = it->w * 0.5f + it->x;
			scorer->cursor->y = it->y + it->h * 0.5f;
		}
	}
	if (!scorer->current) {
		scorer->acceptTime = SCORER_NO_ACCEPT;
	} else {
		it = cScorer_FindItem(scorer, scorer->current);
		if (cInputAction_Pressed(Menu_Action(MENU_ACCEPT), player) && cPy_Function_IsValid(&it->onAccept)) {
			scorer->acceptTime = SCORER_ACCEPT_REPEAT;
		}
		float t = scorer->acceptTime;
		/* the held accept reads the value of player 0 */
		if (t == SCORER_ACCEPT_REPEAT || (t > SCORER_NO_ACCEPT && t <= 0.0f
			&& Menu_Action(MENU_ACCEPT)->value[0] > 0.5f && cPy_Function_IsValid(&it->onAccept))) {
			if (!it->multiPress) {
				scorer->acceptTime = SCORER_NO_ACCEPT;
			} else if (scorer->acceptTime != SCORER_ACCEPT_REPEAT) {
				scorer->acceptTime = 0.07f;
			}
			cScorer_TakeHint(scorer, it);
			Scorer_PlaySound(g_OnAcceptSound, g_OnAcceptSoundVol);
			scorer->focusSound = 0;
			cPy_Function_AddParamI(&it->onAccept, player);
			cPy_Function_AddParamS(&it->onAccept, scorer->current);
			cPy_Function_Execute(&it->onAccept, 0);
		}
		scorer->acceptTime = scorer->acceptTime - scorer->frameTime;
		if (cInputAction_Pressed(Menu_Action(MENU_DELETE), player)) {
			cPy_Function_AddParamI(&it->onDelete, player);
			cPy_Function_AddParamS(&it->onDelete, scorer->current);
			cPy_Function_Execute(&it->onDelete, 0);
		}
	}
	if (cInputAction_Pressed(Menu_Action(MENU_CANCEL), player) && cPy_Function_IsValid(&scorer->onCancel)) {
		cScorer_Call(scorer, &scorer->onCancel, player);
	}
	if (cInputAction_Pressed(Menu_Action(MENU_NEXT), player) && cPy_Function_IsValid(&scorer->onNext)) {
		cScorer_Call(scorer, &scorer->onNext, player);
	}
	if (cInputAction_Pressed(Menu_Action(MENU_PREV), player) && cPy_Function_IsValid(&scorer->onPrev)) {
		cScorer_Call(scorer, &scorer->onPrev, player);
	}
	if (scorer->cursor) {
		int alpha = Scorer_Abs((int)(byte)(int)(scorer->time * 512.0f) - 0x80) + 0x7f;
		double scale;
		cur = scorer->cursor;
		cur->vftable->Var(cur, "Alpha", 1, NULL, NULL, &alpha);
		scale = Menu_Action(MENU_ACCEPT)->value[player] > 0.5f ? 1.25 : 1.0;
		cur = scorer->cursor;
		cur->vftable->Var(cur, "ScaleX", 1, NULL, &scale, NULL);
		cur = scorer->cursor;
		cur->vftable->Var(cur, "ScaleY", 1, NULL, &scale, NULL);
	}
}

CApp *g_pApp;

/* the frame of a scorer: time, screen effects, cursor, the items and the focus marker */
void cScorer_Draw(cScorer *scorer, int player) {
	scorer->lastTime = scorer->time;
	float t = (float)g_pApp->vftable->GetGameTime(g_pApp);
	scorer->time = t;
	scorer->frameTime = t - scorer->lastTime;
	ScorerFade_Draw(&scorer->fade, player, scorer->frameTime);
	ScorerFx_Draw(&scorer->fx, player, scorer->frameTime);
	ScorerCinema_Draw(&scorer->cinema, player, scorer->frameTime);
	if (scorer->frameTime > 0.1f) {
		scorer->frameTime = 0.1f;
		scorer->lastTime = scorer->time - 0.1f;
	}
	if (scorer->skipFrames > 0) {
		scorer->skipFrames--;
	} else if (scorer->cursor) {
		cScorer_UpdateCursor(scorer, player);
	}
	cScorer_RemoveDeleted(scorer, player);
	scorer->cleared = 0;
	if (scorer->visible) {
		for (cScorerItem *it = scorer->items; it; it = it->next) {
			if (it->visible && (it->delMe || !scorer->cleared)) {
				cPy_Function_AddParamI(&it->onRender, player);
				cPy_Function_AddParamS(&it->onRender, it->name);
				cPy_Function_Execute(&it->onRender, 0);
				it->vftable->Draw(it, player);
			}
		}
	}
	if (g_ScorerMarkerOn) {
		cScorerItem *it = cScorer_FindItem(scorer, scorer->current);
		if (!it || !(it->w > 0.0f) || !(it->h > 0.0f)) {
			scorer->markerAlpha = 0.0f;
		} else {
			float a = scorer->markerAlpha;
			if (1.0f > a) {
				scorer->markerAlpha = scorer->frameTime * g_ScorerMarkerSpeed + a;
			} else {
				scorer->markerAlpha = 1.0f;
			}
			float k = Float_MinSS(1.0f, scorer->frameTime * g_ScorerMarkerSpeed);
			CPsaImage *img = g_ScorerMarker;
			float x0 = it->x + it->focusPivotX - scorer->marker[0];
			float y0 = it->y + it->focusPivotY - scorer->marker[1];
			scorer->marker[1] = scorer->marker[1] + y0 * k;
			scorer->marker[0] = x0 * k + scorer->marker[0];
			float x1 = it->focusPivotX + (it->x + it->w) - scorer->marker[2];
			float y1 = it->focusPivotY + (it->y + it->h) - scorer->marker[3];
			scorer->marker[2] = x1 * k + scorer->marker[2];
			scorer->marker[3] = scorer->marker[3] + y1 * k;
			int v = Scorer_Abs((int)((double)scorer->time * 680.0) % 255 - 0x7f);
			byte b = (byte)(v + 0x80);
			img->color = (uint32_t)(byte)(int)((float)(int)b * scorer->markerAlpha) << 24
				| (g_ScorerMarkerColor & 0xffffff);
			v = Scorer_Abs((int)((double)scorer->time * 680.0) % 255 - 0x7f);
			float s = (float)v * 0.0078125f + 1.0f;
			float s2 = s * 2.0f;
			float s3 = s * 3.0f;
			CPsaImage_DrawFrameFlip(img, 0, scorer->marker[0] - s3, scorer->marker[1] - s2, 0, 0);
			CPsaImage_DrawFrameFlip(g_ScorerMarker, 0, s3 + scorer->marker[2] - 8.0f, scorer->marker[1] - s2, 1, 0);
			CPsaImage_DrawFrameFlip(g_ScorerMarker, 0, s3 + scorer->marker[2] - 8.0f,
				scorer->marker[3] + s2 - 8.0f, 2, 0);
			CPsaImage_DrawFrameFlip(g_ScorerMarker, 0, scorer->marker[0] - s3, scorer->marker[3] + s2 - 8.0f, 3,
				0);
		}
	}
	cScorer_RemoveDeleted(scorer, player);
	cScorer_UpdateMP(scorer, scorer->frameTime);
	scorer->focusSound = 1;
}

/* the cloud fade of a camera near the atmosphere limit: a gradient over the screen */
void cScorer_DrawClouds(cScorer *scorer, int player) {
	static const float pos[8] = {0.0f, 0.0f, 640.0f, 0.0f, 640.0f, 480.0f, 0.0f, 480.0f};
	static const float uv[8] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
	/* the corners of the cloud plane around the camera; corner k colors vertex k + 1 (mod 4) */
	static const signed char corners[4][2] = {{-1, 1}, {-1, -1}, {1, -1}, {1, 1}};
	const char *camName;
	cEntity *e;
	vec3_t camPos, camAng, v, out;
	uint32_t c[4];
	float pv[8], puv[8];

	(void)scorer;
	if (!cWorld_GetCamName(g_pWorld, player)) {
		return;
	}
	camName = cWorld_GetCamName(g_pWorld, player);
	if (!*camName || !(e = World_FindEntity(camName))) {
		return;
	}
	camPos = ((cCamControl *)e)->cam.pos;
	camAng = ((cCamControl *)e)->cam.rot;
	if ((float)__builtin_fabs((double)(camPos.y - g_AtmosphereLimit))
		> (g_CameraCloudPlaneSize + g_CameraCloudThickness) * 1.5f + g_CameraCloudDistance) {
		return;
	}
	for (int k = 0; k < 8; k++) {
		pv[k] = pos[k];
		puv[k] = uv[k];
	}
	for (int k = 0; k < 4; k++) {
		float d, f;
		v.x = corners[k][0] < 0 ? -g_CameraCloudPlaneSize : g_CameraCloudPlaneSize;
		v.y = corners[k][1] < 0 ? -g_CameraCloudPlaneSize : g_CameraCloudPlaneSize;
		v.z = 0.0f;
		vec3_t *r = Vec3_Rotate(&out, &v, &camAng);
		d = (float)(__builtin_fabs((double)(r->y + camPos.y - g_AtmosphereLimit)) - (double)g_CameraCloudThickness);
		if (0.0f > d) {
			f = 1.0f;
		} else if (d > g_CameraCloudDistance) {
			f = 0.0f;
		} else {
			f = 1.0f - d / g_CameraCloudDistance;
		}
		c[(k + 1) & 3] = (((uint32_t)(int)((float)g_CameraCloudAlpha * f) << 8 | (uint32_t)g_CameraCloudRed) << 8
			| (uint32_t)g_CameraCloudGreen) << 8 | (uint32_t)g_CameraCloudBlue;
	}
	CPsaImage *img = g_ScorerWhite;
	img->pivotX = 0.0f;
	img->pivotY = 0.0f;
	img->x = 0.0f;
	img->y = 0.0f;
	img->scaleX = 1.0f;
	img->scaleY = 1.0f;
	CPsaImage_DrawPoly(img, pv, puv, c, 4, 6);
}

/* the Xbox texts of the scorer: Saving..., the pad messages */
void cScorer_DrawNoPadMsg(cScorer *scorer, int index, float y, byte big, int pad) {
	CPsaImage *img = g_ScorerWhite;
	const char *next;
	float off;

	(void)index;
	img->scaleX = 640.0f;
	if (big) {
		img->scaleY = 240.0f;
		img->color = 0xc0000000;
		CPsaImage_Draw(img, 0.0f, y - 70.0f);
		CFont_PrintfAlignColor(g_FontHoratio, 320.0f, y + 40.0f, TEXTSCORER_CENTER, 0xffffff, "%s",
			Lang_GetStr("XBox_Pres_Start"));
		return;
	}
	img->scaleY = 80.0f;
	img->color = 0x80000000;
	CPsaImage_Draw(img, 0.0f, y);
	off = (float)__builtin_fabs(1.0 - fmod((double)scorer->time, 2.0)) * 10.0f;
	if (g_SplitScreenGame) {
		CFont_PrintfAlignColor(g_FontHoratio, off + 320.0f, y + 26.0f, TEXTSCORER_CENTER, 0xffffff,
			Lang_GetStr("XBox_NoPad_MP_Msg_1"), pad);
		next = "XBox_NoPad_MP_Msg_2";
	} else {
		CFont_PrintfAlignColor(g_FontHoratio, off + 320.0f, y + 26.0f, TEXTSCORER_CENTER, 0xffffff, "%s",
			Lang_GetStr("XBox_NoPad_Msg_1"));
		next = "XBox_NoPad_Msg_2";
	}
	CFont_PrintfAlignColor(g_FontHoratio, 320.0f - off, y + 54.0f, TEXTSCORER_CENTER, 0xffffff, "%s",
		Lang_GetStr(next));
}

void cScorer_DrawXboxMsgs(cScorer *scorer, int player) {
	char buf[0x80];

	if (g_XboxSavingMsg > 0.0f && !g_IsDemo) {
		CPsaImage *img = g_ScorerWhite;
		img->scaleX = 640.0f;
		img->scaleY = 80.0f;
		img->color = 0x80000000;
		CPsaImage_Draw(img, 0.0f, 100.0f);
		g_XboxSavingMsg = g_XboxSavingMsg - (scorer->time - scorer->lastTime);
		Str_PrintfQuiet(buf, sizeof(buf), "%s1", g_XboxSavingMsgStr);
		CFont_PrintfAlignColor(g_FontHoratio, 320.0f, 126.0f, TEXTSCORER_CENTER, 0xffffff, "%s", Lang_GetStr(buf));
		Str_PrintfQuiet(buf, sizeof(buf), "%s2", g_XboxSavingMsgStr);
		CFont_PrintfAlignColor(g_FontHoratio, 320.0f, 154.0f, TEXTSCORER_CENTER, 0xffffff, "%s", Lang_GetStr(buf));
	}
	if (g_XboxNoPadMsg && player == 0) {
		int y;
		if (g_SplitScreenGame) {
			y = 70;
		} else {
			y = crt_stricmp(g_pApp->levelPath + 7, "Menu") ? 300 : 350;
		}
		cScorer_DrawNoPadMsg(scorer, 0, (float)y, 0, g_XboxNoPadMsg);
	}
	if (g_XboxNoPadMsgP && player == 1 && g_SplitScreenGame) {
		cScorer_DrawNoPadMsg(scorer, 1, 310.0f, g_XboxNoPadMsgPBig != 0, g_XboxNoPadMsgP);
	}
}

/* the scorers of the frame (one view, two in split screen), the sound update; always 1 */
byte Scorer_Render(void) {
	CGameCamera *saved = NULL;
	int n;

	if (g_SplitScreenGame) {
		saved = Camera_GetCurrent();
	}
	n = 0;
	do {
		if (g_SplitScreenGame) {
			const char *camName = cWorld_GetCamName(g_pWorld, n);
			if (*camName) {
				cEntity *e = World_FindEntity(camName);
				if (e) {
					CHECK(e->vftable->IsType(e, CAM_CLASSNAME));
					Camera_SetCurrent(&((cCamControl *)e)->cam);
				}
			}
			Eng_SetViewRect(0.0f, n == 1 ? 240.0f : 0.0f, 640.0f, 240.0f);
		}
		if (g_CameraCloudDistance > 0.0f) {
			cScorer_DrawClouds(&g_Scorers[n], n);
		}
		if (!Scorer_DrawConsole()) {
			cScorer_Draw(&g_Scorers[n], n);
			if (g_CShow || g_NWAnalyzeOn) {
				Scorer_DrawDebug();
			}
		}
		cScorer_DrawXboxMsgs(&g_Scorers[n], n);
		n++;
	} while (n < (g_SplitScreenGame != 0) + 1);
	if (g_SplitScreenGame) {
		Eng_ResetViewRect();
		Camera_SetCurrent(saved);
	}
	g_pSound->vftable->Update(g_pSound);
	g_pSound->vftable->DebugDraw(g_pSound, 10, 120);
	cWorld_ShowDebrisInfo(g_pWorld, 10, 120);
	if (g_NewsPanel) {
		cNewsPanel_Update(g_NewsPanel);
	}
	return 1;
}

/* ---- the full screen console ---- */

#define CONSOLE_HISTORY 16
#define CONSOLE_KEYBOARD_SIZE 0x10c
#define CONSOLE_ONINPUT 0x1741b0			/* Console_OnInput as the exe stores it */

byte g_ConsoleOn;
char *g_ConsoleHistory[16];
int g_ConsoleHistoryPos;
cPlayerInput g_PlayerInput[4];

/* the line typed into the console: a Python line for FullScreenConsoleCallback, else a console command */
void Console_OnInput(int player, const char *text) {
	(void)player;
	for (int k = CONSOLE_HISTORY - 1; k > 0; k--) {
		cString_Assign(&g_ConsoleHistory[k], &g_ConsoleHistory[k - 1]);
	}
	cString_Set(&g_ConsoleHistory[1], text);
	if ((g_pApp->flags || cPy_Function_IsValid(g_FullScreenConsoleCallback)) && text[0] != ':' && text[0] != '/') {
		if (text[0] == '!') {
			Console_Execute(text + 1);
		} else {
			cPy_Function_AddParamS(g_FullScreenConsoleCallback, text);
			cPy_Function_Execute(g_FullScreenConsoleCallback, 0);
		}
	} else {
		Console_Execute(text);
	}
	cPlayerInput_SetString(&g_PlayerInput[0], str_Empty);
	g_ConsoleHistoryPos = 0;
	if (g_ConsoleOn) {
		g_PlayerInput[0].onDone = (void *)Console_OnInput;
	}
}

void Console_HistoryUp(void) {
	if (!g_ConsoleHistoryPos) {
		cString_Set(&g_ConsoleHistory[0], g_PlayerInput[0].text);
	}
	g_ConsoleHistoryPos++;
	if (g_ConsoleHistoryPos >= CONSOLE_HISTORY) {
		g_ConsoleHistoryPos = CONSOLE_HISTORY - 1;
	}
	if (g_ConsoleHistory[g_ConsoleHistoryPos]) {
		cPlayerInput_SetString(&g_PlayerInput[0], g_ConsoleHistory[g_ConsoleHistoryPos]);
	}
}

void Console_HistoryDown(void) {
	g_ConsoleHistoryPos--;
	if (g_ConsoleHistoryPos < 0) {
		g_ConsoleHistoryPos = 0;
	}
	if (g_ConsoleHistory[g_ConsoleHistoryPos]) {
		cPlayerInput_SetString(&g_PlayerInput[0], g_ConsoleHistory[g_ConsoleHistoryPos]);
	}
}

byte g_ConsoleCheckInput;
byte g_ConsoleClosing;
byte g_ConsoleBusy;
cVirtualKeyboard g_ConsoleSavedKeyboard;
uint32_t g_ConsoleSavedSet;
cPlayerInput g_ConsoleSavedInput;
CPsaImage *g_ConsoleWhite;
float g_ConsoleTime;

void Scorer_SetConsole(int on) {
	byte b = (byte)on;

	if ((g_ConsoleOn == 0) == (b == 0) || g_ConsoleBusy) {
		return;
	}
	g_ConsoleBusy = 1;
	g_ConsoleOn = b;
	if (b) {
		g_ConsoleSavedSet = Input_GetActionSet();
		Input_SelectActionSet(2);
		memcpy((&g_ConsoleSavedInput), &g_PlayerInput[0], sizeof(cPlayerInput));
		cPlayerInput_SetString(&g_PlayerInput[0], str_Empty);
		g_PlayerInput[0].onDone = (void *)Console_OnInput;
		CPsaImage *img = operator_new(sizeof(CPsaImage));
		g_ConsoleWhite = img ? CPsaImage_ctor(img, "2D/white.tga", 0x24) : NULL;
		g_ConsoleCheckInput = g_pWorld->checkInput;
		g_pWorld->checkInput = 0;
		double t = g_pApp->vftable->GetGameTime(g_pApp);
		g_ConsoleClosing = 0;
		memcpy((&g_ConsoleSavedKeyboard), &g_VirtualKeyboard, CONSOLE_KEYBOARD_SIZE);
		g_ConsoleTime = (float)t;
		cVirtualKeyboard_Set(&g_VirtualKeyboard, "Console");
		g_ConsoleHistoryPos = 0;
	} else {
		Input_SelectActionSet((int)g_ConsoleSavedSet);
		memcpy(&g_PlayerInput[0], (&g_ConsoleSavedInput), sizeof(cPlayerInput));
		Scorer_FreeImage(g_ConsoleWhite);
		g_ConsoleWhite = NULL;
		g_pWorld->checkInput = g_ConsoleCheckInput;
		memcpy(&g_VirtualKeyboard, (&g_ConsoleSavedKeyboard), CONSOLE_KEYBOARD_SIZE);
	}
	g_ConsoleBusy = 0;
}

#define CONSOLE_SPEED 500.0f		/* pixels per second of the slide */
#define CONSOLE_HEIGHT 234.0f
#define CONSOLE_FADE 192.0f		/* the background is opaque below this slide */
#define CONSOLE_BG 0xc0c0c0
#define CONSOLE_W 640.0f
#define CONSOLE_LINES_H 208.0f		/* the line part, the input line below */
#define CONSOLE_INPUT_BG 0xc0000000
#define CONSOLE_INPUT_H 27.0f
#define CONSOLE_INPUT_Y 217.0f
#define CONSOLE_MARGIN 10.0f
#define CONSOLE_LINE_STEP 9
#define CONSOLE_LOG_H 211.0f		/* ShowConsoleLog */
#define CONSOLE_LOG_BG 0x80ffffff
#define CONSOLE_BLINK 511		/* the cursor blinks every 511/1024 s */
/* the actions of the input sets: 1 the console key, 2 and 3 the history */
#define CONSOLE_ACT_KEY 1
#define CONSOLE_ACT_UP 2
#define CONSOLE_ACT_DOWN 3
#define CONSOLE_VK_CONTROL 0x11

static cInputAction *Console_Action(int set, int k) {
	return InputActionTable_At(&ActionSetTable_At(&g_ActionSets, set)->actions, k);
}

/* the 0xBBGGRR color of a console line as 0x00RRGGBB */
static uint32_t Console_LineColor(uint32_t c) {
	return ((c >> 16) & 0xff) | (c & 0xff) << 16 | (c & 0xff00);
}

static const cConLine *Console_Line(int i) {
	return &g_ConLines[(g_ConLine + 1 + i) % CON_LINES];
}

/* ":name<tab>" in the console: the help of the variable, then its completion */
static void Console_Complete(void) {
	char buf[0x80];
	const char *text = g_PlayerInput[0].text;

	if (text[0] != ':' || !strchr(text, '\t')) {
		return;
	}
	Str_Copy(buf, text, sizeof(buf));
	buf[0] = '?';
	*strchr(buf, '\t') = 0;
	Console_Execute(buf);
	buf[0] = ':';
	if (GVar_Complete(buf + 1)) {
		Str_PrintfQuiet(buf, sizeof(buf), ":%s", GVar_Complete(buf + 1));
	}
	cPlayerInput_SetString(&g_PlayerInput[0], buf);
}

byte Scorer_DrawConsole(void) {
	float h;

	if (!g_ConsoleOn) {
		if ((Input_GetActionSet() == 0 && cInputAction_Released(Console_Action(0, CONSOLE_ACT_KEY), 0))
			|| (Input_GetActionSet() == 3 && cInputAction_Released(Console_Action(3, CONSOLE_ACT_KEY), 0))
			|| (Input_GetActionSet() == 1 && cInputAction_Released(Console_Action(1, CONSOLE_ACT_KEY), 0))) {
			if (Plat_KeyDown(CONSOLE_VK_CONTROL)) {
				Scorer_SetConsole(1);
			}
		}
		if (g_ShowConsoleLog) {
			Scorer_GetWhite()->color = CONSOLE_LOG_BG;
			CPsaImage *w = Scorer_GetWhite();
			w->scaleX = CONSOLE_W;
			w->scaleY = CONSOLE_LOG_H;
			CPsaImage_Draw(Scorer_GetWhite(), 0.0f, 0.0f);
			for (int i = 0, y = 0; y < CON_LINES * CONSOLE_LINE_STEP; i++, y += CONSOLE_LINE_STEP) {
				const cConLine *l = Console_Line(i);
				CFont_PrintfColor(g_FontTerminal8, CONSOLE_MARGIN, (float)y + CONSOLE_MARGIN,
					Console_LineColor(l->color), "%s", l->text);
			}
		}
		return g_ConsoleOn;
	}
	Console_Complete();
	if (g_ConsoleClosing) {
		double t = g_pApp->vftable->GetGameTime(g_pApp);
		h = (float)(t - (double)g_ConsoleTime) * CONSOLE_SPEED;
		if (h > CONSOLE_HEIGHT) {
			Scorer_SetConsole(0);
			return 0;
		}
	} else {
		double t0 = (double)g_ConsoleTime;
		double t = g_pApp->vftable->GetGameTime(g_pApp);
		h = Float_MaxSS(0.0f, (float)(t0 - t) * CONSOLE_SPEED + CONSOLE_HEIGHT);
		if (cInputAction_Pressed(Console_Action(2, CONSOLE_ACT_UP), 0)) {
			Console_HistoryUp();
		}
		if (cInputAction_Pressed(Console_Action(2, CONSOLE_ACT_DOWN), 0)) {
			Console_HistoryDown();
		}
	}
	CPsaImage *img = g_ConsoleWhite;
	if (CONSOLE_FADE > h) {
		img->color = (uint32_t)(0xc0 - (int)h) << 24 | CONSOLE_BG;
	} else {
		img->color = CONSOLE_BG;
	}
	img->scaleX = CONSOLE_W - (h + h);
	img->scaleY = CONSOLE_LINES_H - h;
	CPsaImage_Draw(img, h + 0.0f, h * 0.5f + 0.0f);
	float off = h * 5.0f;
	img = g_ConsoleWhite;
	img->color = CONSOLE_INPUT_BG;
	img->scaleX = CONSOLE_W;
	img->scaleY = CONSOLE_INPUT_H;
	CPsaImage_Draw(img, off + 0.0f, CONSOLE_LINES_H);
	/* the lines fly apart left and right while the console slides */
	for (int i = 0, y = 0; y < CON_LINES * CONSOLE_LINE_STEP; i++, y += CONSOLE_LINE_STEP) {
		const cConLine *l = Console_Line(i);
		float sx = (i & 1) ? h : -h;
		CFont_PrintfColor(g_FontTerminal8, CONSOLE_MARGIN - sx * 5.0f,
			(float)(i - 11) * h / 5.0f + CONSOLE_MARGIN + (float)y, Console_LineColor(l->color), "%s", l->text);
	}
	int ms = (int)((g_pApp->vftable->GetGameTime(g_pApp)) * 1024.0);
	int blink = ms % CONSOLE_BLINK - 255;
	CFont_PrintfXY(g_FontTerminal8, off + CONSOLE_MARGIN, CONSOLE_INPUT_Y, (blink < 0 ? -blink : blink) > 128 ? "%s"
		: "%s_", g_PlayerInput[0].text);
	g_PlayerInput[0].active = 0;
	if (!g_ConsoleClosing && cInputAction_Released(Console_Action(2, CONSOLE_ACT_KEY), 0) && h == 0.0f) {
		g_ConsoleClosing = 1;
		g_ConsoleTime = (float)g_pApp->vftable->GetGameTime(g_pApp);
	}
	return g_ConsoleOn;
}

/* ---- CShow and the AI debug drawings ---- */

enum {
	CSHOW_GRID = 1, CSHOW_ENTITIES, CSHOW_SOUNDS, CSHOW_AI_HIDDEN, CSHOW_PYMEM, CSHOW_AI, CSHOW_SHIPS
};

/* slot off of the vftable of any object, called without arguments */
#define DBG_VF(p, off) ((void(*)(void *))(*(void *const *const *)(p))[(off) / 4])
#define DBG_ITEM_RADIUS(e) (((cItemEntity *)(e))->radius)
#define DBG_FCAM_POS(e) (&((cFollowCamEntity *)(e))->center)	/* the point a "FCam" follows */
#define DBG_FCAM_DIST(e) (((cCamControl *)(e))->cam.zNear)
#define DBG_FCAM_SCALE 1.65f
#define DBG_SPHERE_DIV 10
#define DBG_ARROW_Y 5.0f

static cActClass *Dbg_ActClass(cEntity *e) {
	return ((cActCtrl *)((const cWithLifeEntityVtbl *)e->vftable)->GetActCtrl(e))->cls;
}

/* a flat arrow over the feet of a walking character turned like the angles ang */
static void Dbg_DrawArrow(cEntity *e, float angY, float y, float len, float w, uint32_t color) {
	matrix_t m;
	vec3_t p, t, a, b, c;

	Matrix_RotY(&m, angY);
	const vec3_t *pos = e->vftable->GetPos(e, &t);
	p.x = pos->x + 0.0f;
	p.y = pos->y + y;
	p.z = pos->z + 0.0f;
	Matrix_Translate(&m, &p);
	a.x = 0.0f;
	a.y = 0.0f;
	a.z = -len;
	b.x = w;
	b.y = 0.0f;
	b.z = w;
	c.x = -w;
	c.y = 0.0f;
	c.z = w;
	Debug_DrawTriM(&m, &a, &b, &c, color);
}

/* the boxes of the body and of the step of a walking character, its view and render arrows, its walk radius */
static void Dbg_DrawWalkChar(cEntity *e) {
	const cObjEntityVtbl *vt = (const cObjEntityVtbl *)e->vftable;
	vec3_t mn, mx, t;
	const vec3_t *p;

	float r = Dbg_ActClass(e)->sRadius;
	float fall = Dbg_ActClass(e)->minFallDist;
	p = e->vftable->GetPos(e, &t);
	mx.x = r + p->x;
	mx.y = p->y + 0.0f;
	mx.z = p->z + r;
	float nr = -r;
	float ny = -WChar(e)->floorDist;
	p = e->vftable->GetPos(e, &t);
	mn.x = nr + p->x;
	mn.y = p->y + ny;
	mn.z = p->z + nr;
	DebugUtils_DrawBox(&mn, &mx, 0xffff00);
	float lo = -WChar(e)->floorDist - fall;
	p = e->vftable->GetPos(e, &t);
	mx.x = p->x + r;
	mx.y = p->y + lo;
	mx.z = p->z + r;
	ny = -WChar(e)->floorDist;
	p = e->vftable->GetPos(e, &t);
	mn.x = nr + p->x;
	mn.y = p->y + ny;
	mn.z = p->z + nr;
	DebugUtils_DrawBox(&mn, &mx, 0xff0000);
	float ang = vt->GetViewAng(e, &t)->y;
	if (!Dbg_ActClass(e)->canFly) {
		Dbg_DrawArrow(e, ang, DBG_ARROW_Y - WChar(e)->floorDist, 100.0f, 50.0f, 0xff);
	} else {
		Dbg_DrawArrow(e, ang, DBG_ARROW_Y - WLife(e)->prim.radius, 100.0f, 50.0f, 0xff);
	}
	ang = vt->GetRenderAng(e, &t)->y;
	Dbg_DrawArrow(e, ang, DBG_ARROW_Y - WChar(e)->floorDist, 50.0f, 25.0f, 0xff00);
	float wr = Dbg_ActClass(e)->wRadius;
	DebugUtils_DrawSphere(e->vftable->GetPos(e, &t), wr, 0xc0c0c0, DBG_SPHERE_DIV, 0);
}

/* the spheres of the active entities by type, the walking characters in detail, then their own debug drawing */
static void Dbg_DrawEntities(void) {
	vec3_t t, a, b;

	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (!e->active) {
			continue;
		}
		const cObjEntityVtbl *vt = (const cObjEntityVtbl *)e->vftable;
		if (e->vftable->IsType(e, "WithLife")) {
			DebugUtils_DrawSphere(e->vftable->GetPos(e, &t), WLife(e)->prim.radius, 0xff, DBG_SPHERE_DIV,
				0);
		}
		if (e->vftable->IsType(e, "Car")) {
			float r = vt->GetRadius(e);
			DebugUtils_DrawSphere(e->vftable->GetPos(e, &t), r, 0xc0c0c0, DBG_SPHERE_DIV, 0);
			vt->GetCamera(e, &a, &b);
			DebugUtils_DrawLine(&a, vt->GetViewPos(e, &t), 0xffff00);
		}
		if (e->vftable->IsType(e, "Missile")) {
			DebugUtils_DrawSphere(e->vftable->GetPos(e, &t), Missile(e)->radius, 0xffff00, DBG_SPHERE_DIV,
				0);
		}
		if (e->vftable->IsType(e, "item")) {
			DebugUtils_DrawSphere(e->vftable->GetPos(e, &t), DBG_ITEM_RADIUS(e), 0xff00, DBG_SPHERE_DIV, 0);
		}
		if (e->vftable->IsType(e, "FCam")) {
			vec3_t c = *DBG_FCAM_POS(e);
			DebugUtils_DrawSphere(&c, DBG_FCAM_DIST(e) * DBG_FCAM_SCALE, 0xffff, DBG_SPHERE_DIV, 0);
		}
		if (e->vftable->IsType(e, "WalkChar")) {
			Dbg_DrawWalkChar(e);
		}
		DBG_VF(e, 0x50)(e);
	}
}

void Scorer_DrawDebug(void) {
	switch (g_CShow) {
	case CSHOW_GRID:
		Grid_Draw(&g_pWorld->grid);
		break;
	case CSHOW_ENTITIES:
		Dbg_DrawEntities();
		break;
	case CSHOW_SOUNDS:
		SndPS_DrawAll();
		break;
	case CSHOW_AI_HIDDEN:
		Logic_ShowAI(0);
		break;
	case CSHOW_PYMEM:
		Scorer_DrawPyMemStats();
		break;
	case CSHOW_AI:
		Logic_ShowAI(1);
		break;
	case CSHOW_SHIPS:
		Logic_ShowShips();
		break;
	}
	Dummies_Draw();
	if (!g_NWAnalyzeOn) {
		return;
	}
	if (g_NodesWorld) {
		CNodesWorld_Draw(g_NodesWorld);
	}
	if (g_NodesWorld2D) {
		CNodesWorld2D_Draw(g_NodesWorld2D);
	}
	if (g_NodesTraffic) {
		g_NodesTraffic->vftable->Draw(g_NodesTraffic);
	}
	if (g_NodesTrafficInDoor) {
		g_NodesTrafficInDoor->vftable->Draw(g_NodesTrafficInDoor);
	}
	if (g_VehicleGroup) {
		cAIVehicleGroup_Draw(g_VehicleGroup);
	}
	if (g_AStar) {
		CAStar_DrawDebug((CAStar *)g_AStar);
	}
	if (g_AStar2D) {
		CAStar2D_DrawDebug((CAStar *)g_AStar2D);
	}
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (e->active) {
			void *ai = e->vftable->GetAI(e);
			if (ai) {
				AI_Vtbl(ai)->Draw(ai);
			}
		}
	}
}

/* the vtable of cScorerItem */
const cScorerItemVtbl cScorerItem_vftable = {
	.Draw = (void *)crt_purecall,
	.SetParam = cScorerItem_SetParam,
	.Var = cScorerItem_Var,
	.Delete = cScorerItem_Delete,
};

/* the vtable of cScorer */
const cScorerVtbl cScorer_vftable = {
	.Delete = cScorer_Delete,
};

cGVar GVar_FileNameMap2D;
cGVar GVar_CinemaBlackBandHeight;
cGVar GVar_MotionBlurFactorBegin;
cGVar GVar_OnAcceptSoundVol;
cGVar GVar_CameraCloudPlaneSize;
cGVar GVar_CameraCloudRed;
cGVar GVar_XboxSavingMsgStr;
cGVar GVar_XboxNoPadMsgP;
cGVar GVar_OnFocusSoundVol;
cGVar GVar_CameraCloudDistance;
cGVar GVar_NoiseEndTime;
cGVar GVar_NoiseFactor;
cGVar GVar_MotionBlurEndTime;
cGVar GVar_OnAcceptSound;
cGVar GVar_XboxNoPadMsgPBig;
cGVar GVar_NoiseTime;
cGVar GVar_CameraCloudGreen;
cGVar GVar_XboxSavingMsg;
cGVar GVar_NoiseFactorBegin;
cGVar GVar_ScorerMarker;
cGVar GVar_MotionBlurTime;
cGVar GVar_CameraScene;
cGVar GVar_ScorerMarkerSpeed;
cGVar GVar_OnFocusSound;
cGVar GVar_CameraCloudBlue;
cGVar GVar_MotionBlurFactorEnd;
cGVar GVar_CameraCloudThickness;
cGVar GVar_XboxNoPadMsg;
cGVar GVar_CameraCloudAlpha;
cGVar GVar_ScorerMarkerColor;
cGVar GVar_MotionBlurOffVar;
cGVar GVar_CShow;
cGVarFunction GVar_FullScreenConsoleCallback;
cGVar GVar_ShowConsoleLog;
cGVar GVar_R_SceneMotionBlurOff;
cGVar GVar_SplitScreen;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_VECTOR(Fini_cPlayerInput_PlayerInput, g_PlayerInput, sizeof(cPlayerInput), 4, cPlayerInput_dtor)
INIT_VECTOR(Init_cPlayerInput_PlayerInput, Fini_cPlayerInput_PlayerInput, g_PlayerInput, sizeof(cPlayerInput), 4, cPlayerInput_ctor, cPlayerInput_dtor)
FINI_GVAR_STRING(Fini_GVar_FileNameMap2D, &GVar_FileNameMap2D)
INIT_GVAR_STRING(Init_GVar_FileNameMap2D, Fini_GVar_FileNameMap2D, &GVar_FileNameMap2D, "FileNameMap2D", 0, "2DMap bitmap file", "2D/Black.tga")
INIT_GVAR_INT(Init_GVar_CShow, &GVar_CShow, "CShow", 4, "Shows the colission areas", 0)
FINI_GVAR_STRING(Fini_GVar_OnFocusSound, &GVar_OnFocusSound)
INIT_GVAR_STRING(Init_GVar_OnFocusSound, Fini_GVar_OnFocusSound, &GVar_OnFocusSound, "OnFocusSound", 0, "Set the Focus Sound", "menufocus")
INIT_GVAR_FLOAT(Init_GVar_OnFocusSoundVol, &GVar_OnFocusSoundVol, "OnFocusSoundVol", 0, "Set Volume in the focus of sound", 1.0f)
FINI_GVAR_STRING(Fini_GVar_OnAcceptSound, &GVar_OnAcceptSound)
INIT_GVAR_STRING(Init_GVar_OnAcceptSound, Fini_GVar_OnAcceptSound, &GVar_OnAcceptSound, "OnAcceptSound", 0, "Set the Accept Sound", "menuclik")
INIT_GVAR_FLOAT(Init_GVar_OnAcceptSoundVol, &GVar_OnAcceptSoundVol, "OnAcceptSoundVol", 0, "Set the Accept sound Volume", 1.0f)
INIT_GVAR_INT(Init_GVar_ScorerMarker, &GVar_ScorerMarker, "ScorerMarker", 0, "Show Scorer Focus Marker", 1)
INIT_GVAR_INT(Init_GVar_ScorerMarkerColor, &GVar_ScorerMarkerColor, "ScorerMarkerColor", 0, "Scorer Focus Marker Color", 11853055)
INIT_GVAR_FLOAT(Init_GVar_ScorerMarkerSpeed, &GVar_ScorerMarkerSpeed, "ScorerMarkerSpeed", 0, "Scorer Focus Marker Speed", 6.0f)
INIT_GVAR_INT(Init_GVar_CameraScene, &GVar_CameraScene, "CameraScene", 0, "Camera Scene mode active", 0)
INIT_GVAR_FLOAT(Init_GVar_CinemaBlackBandHeight, &GVar_CinemaBlackBandHeight, "CinemaBlackBandHeight", 0, "Modifies the cinema format", 60.0f)
INIT_GVAR_INT(Init_GVar_NoiseFactorBegin, &GVar_NoiseFactorBegin, "NoiseFactorBegin", 0, "factor of noise fx", 0)
INIT_GVAR_INT(Init_GVar_NoiseFactor, &GVar_NoiseFactor, "NoiseFactor", 0, "factor of noise fx", 0)
INIT_GVAR_FLOAT(Init_GVar_NoiseTime, &GVar_NoiseTime, "NoiseTime", 0, "factor of noise fx", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_NoiseEndTime, &GVar_NoiseEndTime, "NoiseEndTime", 0, "factor of noise fx", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_MotionBlurFactorBegin, &GVar_MotionBlurFactorBegin, "MotionBlurFactorBegin", 0, "factor of motion blur fx", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_MotionBlurFactorEnd, &GVar_MotionBlurFactorEnd, "MotionBlurFactorEnd", 0, "factor of motion blur fx", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_MotionBlurTime, &GVar_MotionBlurTime, "MotionBlurTime", 0, "factor of motion blur fx", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_MotionBlurEndTime, &GVar_MotionBlurEndTime, "MotionBlurEndTime", 0, "factor of motion blur fx", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_MotionBlurOffVar, &GVar_MotionBlurOffVar, "MotionBlurOffVar", 0, "factor of motion blur fx", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_CameraCloudDistance, &GVar_CameraCloudDistance, "CameraCloudDistance", 0, "here start the white fade for de clouds", 4000.0f)
INIT_GVAR_FLOAT(Init_GVar_CameraCloudPlaneSize, &GVar_CameraCloudPlaneSize, "CameraCloudPlaneSize", 0, "The size of de gradient of the fade for de clouds", 1000.0f)
INIT_GVAR_FLOAT(Init_GVar_CameraCloudThickness, &GVar_CameraCloudThickness, "CameraCloudThickness", 0, "The thickness of the cloud fade for de clouds", 500.0f)
INIT_GVAR_INT(Init_GVar_CameraCloudAlpha, &GVar_CameraCloudAlpha, "CameraCloudAlpha", 0, "Aplha falue for the clouds", 240)
INIT_GVAR_INT(Init_GVar_CameraCloudRed, &GVar_CameraCloudRed, "CameraCloudRed", 0, "Red falue for the clouds", 255)
INIT_GVAR_INT(Init_GVar_CameraCloudGreen, &GVar_CameraCloudGreen, "CameraCloudGreen", 0, "Green falue for the clouds", 255)
INIT_GVAR_INT(Init_GVar_CameraCloudBlue, &GVar_CameraCloudBlue, "CameraCloudBlue", 0, "Blue falue for the clouds", 255)
INIT_GVAR_INT(Init_GVar_XboxNoPadMsgPBig, &GVar_XboxNoPadMsgPBig, "XboxNoPadMsgPBig", 0, "if 1 shows the 'please insert the pad' big (to start game).", 0)
INIT_GVAR_INT(Init_GVar_XboxNoPadMsg, &GVar_XboxNoPadMsg, "XboxNoPadMsg", 0, "if 1 shows the 'please insert the pad'.", 0)
INIT_GVAR_INT(Init_GVar_XboxNoPadMsgP, &GVar_XboxNoPadMsgP, "XboxNoPadMsgP", 0, "if 1 shows the 'please insert the pad' for player 2.", 0)
INIT_GVAR_FLOAT(Init_GVar_XboxSavingMsg, &GVar_XboxSavingMsg, "XboxSavingMsg", 0, "the remain time to hide de 'Saving...' message.", 0.0f)
FINI_GVAR_STRING(Fini_GVar_XboxSavingMsgStr, &GVar_XboxSavingMsgStr)
INIT_GVAR_STRING(Init_GVar_XboxSavingMsgStr, Fini_GVar_XboxSavingMsgStr, &GVar_XboxSavingMsgStr, "XboxSavingMsgStr", 0, "the remain 'Saving...' text message in language table.", "XBox_Saving_Msg_")
FINI_CALL(Fini_GVar_FullScreenConsoleCallback, &GVar_FullScreenConsoleCallback.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_FullScreenConsoleCallback, Fini_GVar_FullScreenConsoleCallback, &GVar_FullScreenConsoleCallback, "FullScreenConsoleCallback", 0, "This function is called when imput an string on fullscreen console.", "")
INIT_GVAR_INT(Init_GVar_ShowConsoleLog, &GVar_ShowConsoleLog, "ShowConsoleLog", 0, "1 to show allways the console", 0)
INIT_CALL(Init_cVirtualKeyboard_ConsoleSavedKeyboard, &g_ConsoleSavedKeyboard, cVirtualKeyboard_ctor)
FINI_CALL(Fini_cPlayerInput_ConsoleSavedInput, &((cPlayerInput *)&g_ConsoleSavedInput)->func, cPy_Function_dtor)
INIT_OBJECT(Init_cPlayerInput_ConsoleSavedInput, Fini_cPlayerInput_ConsoleSavedInput, &g_ConsoleSavedInput, cPlayerInput_ctor)
FINI_VECTOR(Fini_cString_ConsoleHistory, g_ConsoleHistory, sizeof(cString), 16, cString_dtor)
INIT_VECTOR(Init_cString_ConsoleHistory, Fini_cString_ConsoleHistory, g_ConsoleHistory, sizeof(cString), 16, cString_ctor, cString_dtor)
INIT_GVAR_INT(Init_GVar_SplitScreen, &GVar_SplitScreen, "SplitScreen", 0, "Is split Screen", 0)
INIT_GVAR_FLOAT(Init_GVar_R_SceneMotionBlurOff, &GVar_R_SceneMotionBlurOff, "R_SceneMotionBlurOff", 0, "Scene Motion Blur OffScreen", 0.005f)
