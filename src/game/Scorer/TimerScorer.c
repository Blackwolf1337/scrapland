/*
 * TimerScorer.cpp of the game (Menu/): cTimerScorer, the "mm:ss" text of one of the four mission timers. See
 * include/scrap/scorerhud.h.
 */
#include "scrap/scorerhud.h"
#include "scrap/timer.h"

cTimerScorer *__fastcall cTimerScorer_ctor(cTimerScorer *self) {
	cAbstractTextScorer_ctor(&self->base);
	self->base.base.vftable = CTIMERSCORER_VFTABLE;
	self->type = 0;
	cPy_Function_ctor(&self->onTimeExpired);
	cPy_Function_ctor(&self->onTimeWarning);
	return self;
}

void __fastcall cTimerScorer_dtor(cTimerScorer *self) {
	self->base.base.vftable = CTIMERSCORER_VFTABLE;
	cPy_Function_dtor(&self->onTimeWarning);
	cPy_Function_dtor(&self->onTimeExpired);
	cAbstractTextScorer_dtor(&self->base);
}

cTimerScorer *__thiscall cTimerScorer_Delete(cTimerScorer *self, byte flags) {
	cTimerScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cTimerScorer));
	}
	return self;
}

int __thiscall cTimerScorer_Var(cTimerScorer *self, const char *name, byte set, char **s, double *d, int *i) {
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
	return cAbstractTextScorer_Var(&self->base, name, set, s, d, i);
}

void __thiscall cTimerScorer_Draw(cTimerScorer *self, int player) {
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
		uint32_t min = Game_FtoUI(left / 60.0f);
		uint32_t sec = Game_FtoUI(left - (float)(double)min * 60.0f);
		CFont_PrintfAlignColorSombra(t->font, t->base.x, t->base.y, 4, t->color, "%.2d:%.2d", min, sec);
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
