/*
 * AI/AIBankDirector.cpp of the game: the AI of the bank director (cAIBankDirector, a cAIChar). Its attack check and
 * its state changes are the same code as the ones of the bank master (AIBankMaster.c). See include/scrap/ai.h.
 */
#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"
#include "scrap/staticinit.h"

#define CAIBANKDIRECTOR_VFTABLE (&cAIBankDirector_vftable)
#define g_MaxHeightChaseCharAI (GVar_MaxHeightChaseCharAI.value.f)
static const cAICharVtbl cAIBankDirector_vftable;
static cGVar GVar_MaxHeightChaseCharAI;

void *cAIBankDirector_ctor(void *mem, cEntity *e, int *arg) {
	cAIBankDirector *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->base.vftable = CAIBANKDIRECTOR_VFTABLE;
	cAIChar_NewControl(&self->base, CAICHARCONTROL_SIZE, cAIControlBankDirector_ctor, e, arg);
	return self;
}

/* AIBankDirector.c, AIBankMaster.c */
static void cAIBankDirector_Free(cAI *elem) {
	cAIBankDirector *self = (cAIBankDirector *)elem;
	cAIChar_dtor(&self->base);
	operator_delete(self);
}

void cAIBank_CheckAttack(cAI *elem) {
	cAIBank *self = (cAIBank *)elem;
	cAIChar *c = &self->base;
	cAICharControl *ctl = c->control;
	byte go;
	vec3_t a, b;

	if (AICC_GetWalkDir(ctl) == 0 && AICC_GetOrbitSide(ctl) == 0 && !c->ctrl->act) {
		go = 1;
	} else if (c->ctrl->act) {
		go = 1;
	} else {
		go = self->money > WChar(c->ent)->money;
	}
	cEntity *tgt = c->base.target.target;
	const vec3_t *op = Entity_GetPos(c->ent, &a);
	const vec3_t *tp = Entity_GetPos(tgt, &b);
	float dx = tp->x - op->x;
	float dz = tp->z - op->z;
	float dist = (float)__builtin_sqrt((double)(dx * dx + 0.0f + dz * dz));
	if (go && AICC_IsDirect2(c->control)) {
		tgt = c->base.target.target;
		tp = Entity_GetPos(tgt, &b);
		op = Entity_GetPos(c->ent, &a);
		if ((double)g_MaxHeightChaseCharAI > __builtin_fabs((double)(tp->y - op->y)) &&
			AIChar_Action0(c->ent)->priority <= 0x65 && dist > c->actMin && c->actMax > dist) {
			c->ctrl->act = 1;
			return;
		}
	}
	c->ctrl->act = 0;
}

/* vf56 */
static void cAIBankDirector_SetRanges(cAI *elem) {
	cAIBankDirector *self = (cAIBankDirector *)elem;
	cAIChar *c = &self->base;

	if (c->state != 3) {
		if (c->state != 7) {
			cAIChar_SetDists(&c->base.base);
			return;
		}
		c->actMin = 0.0f;
		c->actMax = 1000.0f;
	}
	c->chaseFar = 550.0f;
	c->chaseNear = 300.0f;
}

byte cAIBank_SetState(cAI *elem, int state, ...) {
	cAIBank *self = (cAIBank *)elem;
	cAIChar *c = &self->base;
	va_list ap;

	AIChar_DebugState(c, "Demanda externa de CAMBIO de ESTADO -> %s", state);
	if (state == 2) {
		self->money = WChar(c->ent)->money;
		state = 7;
	}
	if (!AIChar_IsGroupState(c, state) || AIGroup_CanChange(g_CharGroup)) {
		AIChar_BeginChange(c, state);
	}
	va_start(ap, state);
	byte r = cAIChar_ChangeState(&c->base.base, state, ap);
	va_end(ap);
	return r;
}

byte cAIBank_SetStateIntern(cAI *elem, int state, ...) {
	cAIBank *self = (cAIBank *)elem;
	cAIChar *c = &self->base;
	va_list ap;

	AIChar_DebugState(c, "Demanda interna de CAMBIO de ESTADO -> %s", state);
	if (AIChar_IsGroupState(c, state)) {
		AIGroup_AllowChange(g_CharGroup);
	}
	if (state == 2) {
		self->money = WChar(c->ent)->money;
		state = 7;
	}
	AIChar_BeginChange(c, state);
	va_start(ap, state);
	byte r = cAIChar_ChangeState(&c->base.base, state, ap);
	va_end(ap);
	return r;
}

/* the vtable of cAIBankDirector */
static const cAICharVtbl cAIBankDirector_vftable = {
	.base.base.Free = cAIBankDirector_Free,
	.base.base.Update = cAIChar_Update,
	.base.base.UpdateOff = cAIChar_UpdateOff,
	.base.base.Draw = cAIChar_Draw,
	.base.base.GetChanged = cAI_GetChanged,
	.base.base.IsVisible = cAI_IsVisible,
	.base.base.Debugf = cAIChar_Debugf,
	.base.base.VarI = cAIChar_VarI,
	.base.base.VarF = cAIChar_VarF,
	.base.base.VarS = cAIChar_VarS,
	.base.base.VarV = cAIChar_VarV,
	.base.base.Print = cAI_Print,
	.base.base.GetEventName = cAI_GetEventName,
	.base.base.OnEvent = cAI_OnEvent,
	.base.GetTarget = cAIMove_GetTarget,
	.base.CanJump = cAIMove_CanJump,
	.base.GetStopped = cAIMove_GetStopped,
	.base.IsPaused = cAIChar_IsPaused,
	.base.GetTargetEnt = cAIMove_GetTargetEnt,
	.base.GetArrived = cAIMove_GetArrived,
	.base.RandomPoint = cAIMove_RandomPoint,
	.base.CheckStopped = cAIMove_CheckStopped,
	.base.Event = cAIChar_Event,
	.base.IsMoving = cAIChar_IsMoving,
	.base.CanTarget = cAIMove_CanTarget,
	.CheckParams = cAIChar_CheckParams,
	.ChangeState = cAIChar_ChangeState,
	.SetState = cAIBank_SetState,
	.RestoreState = cAIChar_RestoreState,
	.SetInternStateV = cAIChar_SetInternStateV,
	.SetInternState = cAIChar_SetInternState,
	.GetState = cAIChar_GetState,
	.GetInternState = cAIChar_GetInternState,
	.StateName = cAIChar_StateName,
	.GetStateName = cAIChar_GetStateName,
	.InternStateName = cAIChar_InternStateName,
	.GetInternStateName = cAIChar_GetInternStateName,
	.GetDirect = cAIChar_GetDirect,
	.GetDirect2 = cAIChar_GetDirect2,
	.GetControl = cAIChar_GetControl,
	.GetLast = cAIChar_GetLast,
	.SaveTo = cAIChar_SaveTo,
	.SetRepos = cAIChar_SetRepos,
	.GetRepos = cAIChar_GetRepos,
	.IsTalking = cAIChar_IsTalking,
	.GetCollide = cAIChar_GetCollide,
	.React = cAIChar_React,
	.CheckAction = cAIChar_CheckAction,
	.GetChanged2 = cAIChar_GetChanged,
	.CanTalk = cAIChar_CanTalk,
	.SetStateIntern = cAIBank_SetStateIntern,
	.RestoreStateIntern = cAIChar_RestoreStateIntern,
	.ChangeStateMore = cAIChar_ChangeStateMore,
	.IsGroupState = cAIChar_IsGroupState,
	.Repos = cAIChar_Repos,
	.AfterRepos = cAIChar_AfterRepos,
	.SetDists = cAIBankDirector_SetRanges,
	.SetDists2 = cAIChar_SetDists2,
	.BeginChange = cAIChar_BeginChange,
	.SetPauseCheck = cAIChar_SetPauseCheck,
	.SaveState = cAIChar_SaveState,
	.RestoreSaved = cAIChar_RestoreSaved,
	.SetPaused = cAIChar_SetPaused,
	.UpdateAction1 = cAIChar_UpdateAction1,
	.UpdateAction0 = cAIChar_UpdateAction0,
	.UpdateAction = cAI_UpdateActionNop,
	.CheckActDist = cAIBank_CheckAttack,
	.SetAction = cAIChar_SetAction,
	.UpdateMove = cAIChar_UpdateMove,
	.EventDefault = cAIChar_EventDefault,
	.EventInRoute = cAIChar_EventInRoute,
	.EventPatrol = cAIChar_EventPatrol,
	.EventGoTo = cAIChar_EventGoTo,
	.EventGetAway = cAIChar_EventGetAway,
	.EventChase = cAI_EventChaseFalse,
	.EventChaseBack = cAIChar_EventChaseBack,
	.EventChaseNoAction = cAI_EventChaseNoActionFalse,
	.EventLostPath = cAIChar_EventLostPath,
	.Resume = cAIChar_Resume,
	.UpdateThink = cAIChar_UpdateThink,
	.CheckPause = cAIChar_CheckPause,
	.CheckRepos = cAIChar_CheckRepos,
	.CanRepos = cAIChar_CanRepos,
	.Think = cAIChar_Think,
	.ThinkInRoute = cAIChar_ThinkInRoute,
	.ThinkGetAway = cAI_ThinkGetAwayNop,
	.ThinkChase = cAI_ThinkChaseNop,
	.ThinkChaseBack = cAI_ThinkChaseBackNop,
	.ThinkChaseNoAction = cAI_ThinkChaseNoActionNop,
	.ThinkLostPath = cAIChar_ThinkLostPath,
	.ThinkLostPathChase = cAIChar_ThinkLostPathChase,
	.ReactLostPath = cAIChar_ReactLostPath,
	.ReactLostPathChase = cAIChar_ReactLostPath,
	.ReactPatrol = cAIChar_ReactPatrol,
	.ReactInRoute = cAIChar_ReactInRoute,
	.ThinkGoTo = cAI_ThinkGoToNop,
	.ThinkPatrol = cAIChar_ThinkPatrol,
	.CheckTalk = cAIChar_CheckTalk,
	.Facing = cAIChar_Facing,
};

static cGVar GVar_MaxHeightChaseCharAI;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_MaxHeightChaseCharAI, &GVar_MaxHeightChaseCharAI, "MaxHeightChaseCharAI", 4, "Max height to chase.\n", 300.0f)
