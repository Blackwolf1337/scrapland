/*
 * AI/AINurse.cpp of the game: the AI of the nurse (cAINurse, a cAIChar with cAIControlNurse). It uses the action
 * button on a dazed target: at once when the attack (state 2) just began and the target is within actMax, else from
 * beyond actMax up to DistActionNurseAI. See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"
#include "scrap/staticinit.h"

#define CAINURSE_SIZE sizeof(cAINurse)

void *cAINurse_ctor(void *mem, cEntity *e, int *arg) {
	cAINurse *self = mem;
	cAIChar *c = &self->base;

	cAIChar_ctor(mem, e, arg);
	c->vftable = CAINURSE_VFTABLE;
	cAIChar_NewControl(c, CAICHARCONTROL_SIZE, cAIControlNurse_ctor, e, arg);
	self->fresh = 0;
	c->canTalk = 0;
	return self;
}

cAI *cAINurse_Delete(cAI *base, byte flags) {
	cAIChar *self = (cAIChar *)base;
	self->vftable = CAINURSE_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAINURSE_SIZE);
	}
	return base;
}

void cAINurse_CheckActDist(cAI *elem) {
	cAINurse *self = (cAINurse *)elem;
	cAIChar *c = &self->base;
	float dist = AIChar_TargetDist(c);

	if (AIChar_Action0(c->ent)->useFlyDisp && AIChar_ActionIs(c->base.target.target, "Dazed") &&
		((g_DistActionNurseAI > dist && dist > c->actMax) || (c->actMax > dist && self->fresh))) {
		self->fresh = 0;
		c->ctrl->act = 1;
		return;
	}
	self->fresh = 1;
	c->ctrl->act = 0;
}

void cAINurse_SetDists(cAI *elem) {
	cAINurse *self = (cAINurse *)elem;
	cAIChar *c = &self->base;

	if (c->state != 2) {
		cAIChar_SetDists(&c->base.base);
		return;
	}
	float nearD = WChar(c->ent)->wl.prim.radius * 1.1f + WChar(c->base.target.target)->wl.prim.radius;
	c->chaseNear = nearD;
	float farD = nearD + g_OffsetMoveNurseAI;
	c->actMin = c->chaseNear;
	c->chaseFar = farD;
	c->actMax = c->chaseFar;
}

byte cAINurse_SetState(cAI *elem, int state, ...) {
	cAINurse *self = (cAINurse *)elem;
	cAIChar *c = &self->base;
	va_list ap;

	AIChar_DebugState(c, "Demanda externa de CAMBIO de ESTADO -> %s", state);
	if (!c->vftable->IsGroupState(&c->base.base, state) || g_CharGroup->vftable->CanChange(&g_CharGroup->base)) {
		c->vftable->BeginChange(&c->base.base, state);
		if (c->state == 2) {
			self->fresh = 1;
		}
	}
	va_start(ap, state);
	byte r = cAIChar_ChangeState(&c->base.base, state, ap);
	va_end(ap);
	return r;
}

byte cAINurse_SetStateIntern(cAI *elem, int state, ...) {
	cAINurse *self = (cAINurse *)elem;
	cAIChar *c = &self->base;
	va_list ap;

	AIChar_DebugState(c, "Demanda interna de CAMBIO de ESTADO -> %s", state);
	if (c->vftable->IsGroupState(&c->base.base, state)) {
		g_CharGroup->vftable->AllowChange(&g_CharGroup->base);
	}
	c->vftable->BeginChange(&c->base.base, state);
	if (c->state == 2) {
		self->fresh = 1;
	}
	va_start(ap, state);
	byte r = cAIChar_ChangeState(&c->base.base, state, ap);
	va_end(ap);
	return r;
}

/* the vtable of cAINurse */
const cAICharVtbl cAINurse_vftable = {
	.base.base.Delete = cAINurse_Delete,
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
	.SetState = cAINurse_SetState,
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
	.SetStateIntern = cAINurse_SetStateIntern,
	.RestoreStateIntern = cAIChar_RestoreStateIntern,
	.ChangeStateMore = cAIChar_ChangeStateMore,
	.IsGroupState = cAIChar_IsGroupState,
	.Repos = cAIChar_Repos,
	.AfterRepos = cAIChar_AfterRepos,
	.SetDists = cAINurse_SetDists,
	.SetDists2 = cAIChar_SetDists2,
	.BeginChange = cAIChar_BeginChange,
	.SetPauseCheck = cAIChar_SetPauseCheck,
	.SaveState = cAIChar_SaveState,
	.RestoreSaved = cAIChar_RestoreSaved,
	.SetPaused = cAIChar_SetPaused,
	.UpdateAction1 = cAIChar_UpdateAction1,
	.UpdateAction0 = cAIChar_UpdateAction0,
	.UpdateAction = cAI_UpdateActionNop,
	.CheckActDist = cAINurse_CheckActDist,
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

cGVar GVar_DistActionNurseAI;
cGVar GVar_OffsetMoveNurseAI;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_DistActionNurseAI, &GVar_DistActionNurseAI, "DistActionNurseAI", 4, "Distance to action the Nurse.\n", 1600.0f)
INIT_GVAR_FLOAT(Init_GVar_OffsetMoveNurseAI, &GVar_OffsetMoveNurseAI, "OffsetMoveNurseAI", 4, "Offset to move Nurse.\n", 25.0f)
