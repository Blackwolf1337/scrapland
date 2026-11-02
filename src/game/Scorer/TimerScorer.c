/*
 * TimerScorer.cpp of the game (Menu/): cTimerScorer, the "mm:ss" text of one of the four mission timers. See
 * include/scrap/scorerhud.h.
 */
#include "scrap/scorerhud.h"
#include "scrap/timer.h"
#include "scrap/staticinit.h"

#define CTIMERSCORER_VFTABLE (&cTimerScorer_vftable)
#define g_TimeToCancel (GVar_TimeToCancelCurrentMission.value.f)
#define g_TimeToFinish (GVar_TimeToFinishCurrentMission.value.f)
#define g_TimeInactive (GVar_TimeInactiveTimer.value.f)
#define g_TimeSpecialUse (GVar_TimeSpecialUseTimer.value.f)
#define g_TimerMode (GVar_CounterType.value.i)		/* 0 counts down, 1 up, else stopped */
#define g_TimerWarning (GVar_TimeWarning.value.f)	/* OnTimeWarning below this, -1 after */
static const cScorerItemVtbl cTimerScorer_vftable;
static cGVar GVar_TimeWarning;
static cGVar GVar_TimeToFinishCurrentMission;
static cGVar GVar_TimeToCancelCurrentMission;
static cGVar GVar_TimeSpecialUseTimer;
static cGVar GVar_TimeInactiveTimer;
static cGVar GVar_CounterType;

cTimerScorer *cTimerScorer_ctor(cTimerScorer *self) {
	cAbstractTextScorer_ctor(&self->base);
	self->base.base.vftable = CTIMERSCORER_VFTABLE;
	self->type = 0;
	cPy_Function_ctor(&self->onTimeExpired);
	cPy_Function_ctor(&self->onTimeWarning);
	return self;
}

static void cTimerScorer_dtor(cTimerScorer *self) {
	self->base.base.vftable = CTIMERSCORER_VFTABLE;
	cPy_Function_dtor(&self->onTimeWarning);
	cPy_Function_dtor(&self->onTimeExpired);
	cAbstractTextScorer_dtor(&self->base);
}

static cScorerItem *cTimerScorer_Delete(cScorerItem *base, byte flags) {
	cTimerScorer *self = (cTimerScorer *)base;
	cTimerScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cTimerScorer));
	}
	return base;
}

/* set only: OnTimeExpired, OnTimeWarning, TimerType; then the text variables */
static int cTimerScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cTimerScorer *self = (cTimerScorer *)base;
	static const char *const types[TIMER_TYPES] = {"TimeToCancel", "TimeToFinish", "TimeInactive",
		"TimeSpecialUse"};

	if (set && s) {
		if (!crt_stricmp(name, "OnTimeExpired")) {
			cPy_Function_SetFunc(&self->onTimeExpired, *s);
			return 1;
		}
		if (!crt_stricmp(name, "OnTimeWarning")) {
			cPy_Function_SetFunc(&self->onTimeWarning, *s);
			return 1;
		}
		if (!crt_stricmp(name, "TimerType")) {
			for (int k = 0; k < TIMER_TYPES; k++) {
				if (!crt_stricmp(*s, types[k])) {
					self->type = k;
					return 1;
				}
			}
			CHECK(0);
			return 1;
		}
	}
	return cAbstractTextScorer_Var(&self->base.base, name, set, s, d, i);
}

static void cTimerScorer_Draw(cScorerItem *base, int player) {
	cTimerScorer *self = (cTimerScorer *)base;
	cAbstractTextScorer *t = &self->base;
	float *var = NULL;
	float time, left;

	(void)player;
	switch (self->type) {
	case 0:
		var = &g_TimeToCancel;
		break;
	case 1:
		var = &g_TimeToFinish;
		break;
	case 2:
		var = &g_TimeInactive;
		break;
	case 3:
		var = &g_TimeSpecialUse;
		break;
	default:
		CHECK(0);
		break;
	}
	time = *var;
	if (!(time != 0.0f)) {
		return;
	}
	left = time;
	if (g_TimerMode == 0) {
		left = Float_MaxSS(0.0f, time - cTimer_GetDelta(&g_pWorld->timer));
	} else if (g_TimerMode == 1) {
		left = cTimer_GetDelta(&g_pWorld->timer) + time;
	}
	/* no text for the inactive time */
	if (self->type != 2) {
		uint32_t min = Float_ToU32(left / 60.0f);
		uint32_t sec = Float_ToU32(left - (float)(double)min * 60.0f);
		CFont_PrintfAlignColorShadow(t->font, t->base.x, t->base.y, 4, t->color, "%.2d:%.2d", min, sec);
	}
	if (!(GVAR_FLAGS(*var) & 1)) {
		*var = left;
	}
	if (g_TimerWarning > left) {
		if (!(GVAR_FLAGS(g_TimerWarning) & 1)) {
			g_TimerWarning = -1.0f;
		}
		cPy_Function_Execute(&self->onTimeWarning, 0);
	}
	if (left == 0.0f) {
		cPy_Function_Execute(&self->onTimeExpired, 0);
	}
}

/* the vtable of cTimerScorer */
static const cScorerItemVtbl cTimerScorer_vftable = {
	.Draw = cTimerScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cTimerScorer_Var,
	.Delete = cTimerScorer_Delete,
};

static cGVar GVar_TimeToFinishCurrentMission;
/* the time engine variables of the types, by type (their flags at -4, GVAR_FLAGS) */
static cGVar GVar_TimeToCancelCurrentMission;
static cGVar GVar_CounterType;
static cGVar GVar_TimeInactiveTimer;
static cGVar GVar_TimeSpecialUseTimer;
static cGVar GVar_TimeWarning;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_TimeWarning, &GVar_TimeWarning, "TimeWarning", 0, "Cuando quede esta cantidad de tiempo se avisa al usuario de que se de prisa o se cancelará la apuesta loca (en segundos)", 31.0f)
INIT_GVAR_FLOAT(Init_GVar_TimeInactiveTimer, &GVar_TimeInactiveTimer, "TimeInactiveTimer", 0, "Tiempo actual para que se cancele la misión por inactividad (en segundos)", 121.0f)
INIT_GVAR_FLOAT(Init_GVar_TimeSpecialUseTimer, &GVar_TimeSpecialUseTimer, "TimeSpecialUseTimer", 0, "Temporizador definido por el usuario para usos varios.", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_TimeToCancelCurrentMission, &GVar_TimeToCancelCurrentMission, "TimeToCancelCurrentMission", 0, "Tiempo actual para cancelar la misión (en segundos)", 21.0f)
INIT_GVAR_FLOAT(Init_GVar_TimeToFinishCurrentMission, &GVar_TimeToFinishCurrentMission, "TimeToFinishCurrentMission", 0, "Tiempo actual para que acabe la misión (en segundos)", 0.0f)
INIT_GVAR_INT(Init_GVar_CounterType, &GVar_CounterType, "CounterType", 0, "Indica si se incrementa, se decrementa o se para el contador (0:decrementa, 1:incrementa, 2:se para)", 0)
