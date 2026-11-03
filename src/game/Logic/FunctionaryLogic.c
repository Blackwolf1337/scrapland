/*
 * Logic/FunctionaryLogic.cpp of the game: the functionaries (cFunctionaryLocalLogic, include/scrap/logic.h), the
 * characters with a desktop of the list DesktopsFunctionaryListName. A functionary waits TimeToSitDown, walks to
 * its desktop (state 7), sits down and works (state 8) for TimeToStopWorking (the relax at the desktop at half of
 * it), gets up and starts again. Attacked it escapes with its special action (state 3, the fast and slow parts
 * change every FunctionarySpecialActionStatesTransitionTime) for FunctionarySpecialActionTime. Berto and Betty
 * derive from it.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/chartype.h"
#include "scrap/objentity.h"
#include "scrap/scrapmodel.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

#define CFUNCTIONARYLOCALLOGIC_VFTABLE (&cFunctionaryLocalLogic_vftable.base.base)
static const cFunctionaryLocalLogicVtbl cFunctionaryLocalLogic_vftable;

static cGVar GVar_DesktopsFunctionaryListName;
#define g_DesktopsListName (GVar_DesktopsFunctionaryListName.value.s)	/* DesktopsFunctionaryListName */
static cGVar GVar_FunctionaryAssignWorkDistance;
#define g_FunctionaryAssignWorkDistance (GVar_FunctionaryAssignWorkDistance.value.f)
static cGVarFunction GVar_OnFunctionaryStartsWorking;
#define g_OnFunctionaryStartsWorking (((cPy_Function *)&GVar_OnFunctionaryStartsWorking.func))
static cGVar GVar_FunctionarySpecialActionStatesTransitionTime;
#define g_FunctionarySpecialActionStatesTransitionTime (GVar_FunctionarySpecialActionStatesTransitionTime.value.f)
static cGVar GVar_FunctionaryMinTimeToSitDown;
#define g_FunctionaryMinTimeToSitDown (GVar_FunctionaryMinTimeToSitDown.value.f)
static cGVar GVar_FunctionaryMaxTimeToSitDown;
#define g_FunctionaryMaxTimeToSitDown (GVar_FunctionaryMaxTimeToSitDown.value.f)
static cGVar GVar_FunctionaryMintimeToStopWorking;
#define g_FunctionaryMinTimeToStopWorking (GVar_FunctionaryMintimeToStopWorking.value.f)
static cGVar GVar_FunctionaryMaxtimeToStopWorking;
#define g_FunctionaryMaxTimeToStopWorking (GVar_FunctionaryMaxtimeToStopWorking.value.f)
static cGVar GVar_FunctionaryEffectFastSound;
#define g_FunctionaryEffectFastSound (GVar_FunctionaryEffectFastSound.value.s)
static cGVar GVar_FunctionaryEffectFastSoundVol;
#define g_FunctionaryEffectFastSoundVol (GVar_FunctionaryEffectFastSoundVol.value.f)
cGVar GVar_CharProcessReduction;

#define g_CharProcessReduction (GVar_CharProcessReduction.value.f)

#define Func_Vtbl(self) Functionary_Vtbl(self)
#define Func_Parent(self) ((self)->base.base.parent)

static inline cTimer *Func_Timer(cEntity *e) {
	return &Obj(e)->timer;
}

static void Func_SetWorkTime(cFunctionaryLocalLogic *self) {
	float t = Rand_FloatRange(g_FunctionaryMinTimeToStopWorking, g_FunctionaryMaxTimeToStopWorking);

	self->workTime = t;
	self->relaxTime = t * 0.5f;
	self->playTime = t * 3.0f;
}

static cEntityListData *g_DesktopsList;

cFunctionaryLocalLogic *cFunctionaryLocalLogic_ctor(cFunctionaryLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(&self->base, parent, 0);
	self->base.base.vftable = CFUNCTIONARYLOCALLOGIC_VFTABLE;
	self->desktop.target = NULL;
	self->desktop.data = NULL;
	self->desktop.next = &self->desktop;
	self->desktop.prev = &self->desktop;
	self->forced = NULL;
	self->transTime = 0.0f;
	self->specialTime = 0.0f;
	self->probWorking = 1.0f;
	self->probPlaying = 0.0f;
	self->probWalking = 0.35f;
	self->escapeRadiusSqr = 0.0f;
	self->goToWorkSpeed = g_GenCharRelaxSpeed;
	self->relaxed = 0;
	self->working = 1;
	self->keepWorking = 0;
	self->special = 0;
	self->fast = 0;
	self->fastEscape = 1;
	self->escapeInRadius = 0;
	self->forceSpecial = 0;
	self->forceCancel = 0;
	self->ignoreBanker = 0;
	self->stopInRelax = 1;
	self->prevState = 0;
	self->sitTime = Rand_FloatRange(g_FunctionaryMinTimeToSitDown, g_FunctionaryMaxTimeToSitDown);
	Func_SetWorkTime(self);
	if (!g_DesktopsList) {
		g_DesktopsList = (cEntityListData *)cWorld_GetList(g_pWorld, g_DesktopsListName);
		CHECK(g_DesktopsList && "desktopsList && \"No se ha creado previamente la lista de mesas para los funcionarios\"");
	}
	self->desktop.data = self;
	return self;
}

static void cFunctionaryLocalLogic_ReleaseDesktop(cLocalLogic *base);
void cFunctionaryLocalLogic_dtor(cFunctionaryLocalLogic *self) {
	self->base.base.vftable = CFUNCTIONARYLOCALLOGIC_VFTABLE;
	cFunctionaryLocalLogic_ReleaseDesktop(&self->base.base.logic);
	g_DesktopsList = NULL;
	Nto1Node_Done(&self->desktop);
	cGenCharLocalLogic_dtor(&self->base);
}

static cLocalLogic *cFunctionaryLocalLogic_Delete(cLocalLogic *base, byte flags) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)base;
	cFunctionaryLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, 0xb8);
	}
	return base;
}

/* vf26: the view flag of the character and its team */
void cFunctionaryLocalLogic_UpdateView(cLocalLogic *elem) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	cEntity *p = Func_Parent(self);

	ChType_ActCtrl(p);
	cEntity *usr = ChType_Usr0();
	if (usr) {
		usr->vftable->IsType(usr, TYEN_WALKCHAR);
	}
	int st = self->base.base.state;
	if (!self->base.base.busy && !g_pWorld->alarm.on && st != 4 && st != 5 && st != 10) {
		WChar(p)->wl.arrow.canUse = 1;
		WLife(p)->arrow.tgType = 3;
		return;
	}
	WChar(p)->wl.arrow.canUse = 0;
	WLife(p)->arrow.tgType = 3;
}

/* vf40: the nearest desktop (within maxDist when above 0) taken out of the desktop list and linked */
static void cFunctionaryLocalLogic_AssignDesktop(cLocalLogic *base, float maxDist) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)base;
	vec3_t tmp;

	if (self->forced || self->desktop.target) {
		return;
	}
	cEntity *p = Func_Parent(self);
	void *list = cWorld_GetList(g_pWorld, g_DesktopsListName);
	if (!list || !p) {
		return;
	}
	float dist = 0.0f;
	cEntity *d = cEntityList_Nearest(list, p->vftable->GetPos(p, &tmp), NULL, &dist);
	if (d && (0.0f >= maxDist || maxDist >= dist)) {
		cEntity_SetList(d, NULL);
		cEntity_AddLink(d, &self->desktop);
	}
}

/* vf02: an escaping functionary runs at normal speed; another type of player than "Functionary" makes it
 * suspicious (state 3) unless it wants to talk */
static void cFunctionaryLocalLogic_NormalSpeed(cFunctionaryLocalLogic *self, cEntity *e);
byte cFunctionaryLocalLogic_Vf02(cLocalLogic *elem) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	if (Func_Vtbl(self)->FastEscape(&self->base.base.logic) && (self->base.base.idle || self->base.base.alwaysLock ||
		self->escapeInRadius)) {
		cFunctionaryLocalLogic_NormalSpeed(self, Func_Parent(self));
		return 1;
	}
	cEntity *usr = ChType_Usr0();
	if (strcmp(ChType_ActClass(ChType_ActCtrl(usr))->name, "Functionary") &&
		!Func_Vtbl(self)->CanTalk(&self->base.base.logic)) {
		Logic_Vtbl(self)->SetState(&self->base.base.logic, 3, usr);
		return 0;
	}
	return 1;
}

/* vf33: back to the start while working with time left */
void cFunctionaryLocalLogic_Vf33(cLocalLogic *elem, byte restart) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	if (self->base.base.state != 8) {
		return;
	}
	if (self->working ? self->workTime > 0.0f : self->playTime > 0.0f) {
		Func_Vtbl(self)->StopWork(&self->base.base.logic, restart);
	}
}

cEntity *cFunctionaryLocalLogic_GetDesktop(cFunctionaryLocalLogic *self) {
	return self->forced ? self->forced : self->desktop.target;
}

/* the desktop shows its animation "Down" unless it loops "Loop" */
void cFunctionaryLocalLogic_FreeDesktop(cFunctionaryLocalLogic *self) {
	cEntity *d = cFunctionaryLocalLogic_GetDesktop(self);

	if (!d) {
		return;
	}
	void *ctrl = ChType_ActCtrl(d);
	if (ctrl && strcmp(cActCtrl_GetDefLoopName(ctrl), "Loop")) {
		cActCtrl_Play(ctrl, "Down", 0, 1.0f);
		cActCtrl_SetDefLoop(ctrl, "Loop", 0);
	}
}

/* vf41: the desktop back into the desktop list and unlinked */
static void cFunctionaryLocalLogic_ReleaseDesktop(cLocalLogic *base) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)base;
	if (!cFunctionaryLocalLogic_GetDesktop(self)) {
		return;
	}
	cFunctionaryLocalLogic_FreeDesktop(self);
	cEntity_SetList(cFunctionaryLocalLogic_GetDesktop(self), g_DesktopsListName);
	CHECK(self->desktop.data && "Node");
	Nto1Node_Unlink(&self->desktop);
}

/* the place and the angles at the desktop, sitting down when nobody is there (state 8) */
static void Func_SitDown(cFunctionaryLocalLogic *self, cEntity *p) {
	double x = 0.0, y = 0.0, z = 0.0;
	vec3_t tmp, pos;
	cObjEntity *o = (cObjEntity *)p;

	if (self->working && cFunctionaryLocalLogic_GetDesktop(self)) {
		cEntity *d = cFunctionaryLocalLogic_GetDesktop(self);
		d->vftable->VarV(d, "NT_NodePos_DC_Sit", &x, &y, &z, 0);
		o->move.moveAng = *Func_Vtbl(self)->GetWorkAng(&self->base.base.logic, &tmp);
	}
	pos.x = (float)x;
	pos.y = (float)y;
	pos.z = (float)z;
	if (cWorld_TestSphereEntities(g_pWorld, p, 0x10, &pos, WLife(p)->prim.radius, NULL, 1, 0)) {
		return;
	}
	o->move.movePos.x = (float)x;
	o->move.movePos.y = (float)y;
	o->move.movePos.z = (float)z;
	cObjMove_SetTime(&o->move, 0.5f);
	o->move.fade = OBJMOVE_SMOOTH;
	cActCtrl_Play(ChType_ActCtrl(p), "SitDown", 0, 1.0f);
	if (cPy_Function_IsValid(g_OnFunctionaryStartsWorking)) {
		cPy_Function_AddParamS(g_OnFunctionaryStartsWorking, p->name);
		cPy_Function_Execute(g_OnFunctionaryStartsWorking, 0);
	}
	if (self->working && cFunctionaryLocalLogic_GetDesktop(self)) {
		cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Work", 0);
		if (!self->base.ignoreAlarm) {
			void *ctrl = ChType_ActCtrl(cFunctionaryLocalLogic_GetDesktop(self));
			if (ctrl) {
				cActCtrl_Play(ctrl, "Up", 0, 1.0f);
				cActCtrl_SetDefLoop(ctrl, "LoopUp", 0);
			}
		}
	}
	Func_SetWorkTime(self);
	Logic_Vtbl(self)->SetState(&self->base.base.logic, 8, NULL);
}

/* vf32: the wait before the walk to the desktop, the arrival at it, the work and the relax there */
static void Functionary_WorkRelax(cEntity *e);
void cFunctionaryLocalLogic_Work(cLocalLogic *elem, float dt) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	vec3_t tmp;

	if (!cFunctionaryLocalLogic_GetDesktop(self)) {
		return;
	}
	cEntity *p = Func_Parent(self);
	cAIChar *ai = p->vftable->GetAI(p);
	if (ai->vftable->base.IsPaused(&ai->base.base)) {
		return;
	}
	int st = self->base.base.state;
	if ((st == 0 || st == 6) && self->sitTime > 0.0f) {
		float t = self->sitTime - dt;
		self->sitTime = t;
		if (0.0f >= t) {
			Logic_Vtbl(self)->SetState(&self->base.base.logic, 7, NULL);
		}
		return;
	}
	if (st == 8) {
		if (!(self->working ? self->workTime > 0.0f : self->playTime > 0.0f) || self->keepWorking) {
			return;
		}
		float d = dt;
		if (!CGameModel_IsVisible(Obj_Vtbl(p)->GetModel(p))) {
			float cd = cWorld_GetMinCamDistSq(g_pWorld, p->vftable->GetPos(p, &tmp));
			if (g_CharProcessReduction * g_CharProcessReduction > cd) {
				d = dt * 5.0f;
			}
		}
		byte w = self->working;
		if (w) {
			self->workTime = self->workTime - d;
		} else {
			self->playTime = self->playTime - d;
		}
		if (w) {
			if (!self->relaxed && self->relaxTime >= self->workTime) {
				self->relaxed = 1;
				Functionary_WorkRelax(p);
				return;
			}
			if (!(0.0f >= self->workTime)) {
				return;
			}
		} else if (!(0.0f >= self->playTime)) {
			return;
		}
		if (strcmp(p->vftable->GetClassName(p), TYEN_WALKCHAR)) {
			return;
		}
		cObjMove_SetTime(&Obj(p)->move, 0.0f);
		cActCtrl_Play(ChType_ActCtrl(p), "GetUp", 0, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Relax", 1);
		Func_Vtbl(self)->StopWork(&self->base.base.logic, 1);
		return;
	}
	if (st != 7) {
		return;
	}
	if (((cAIChar *)ai)->vftable->GetState(&ai->base.base) != 0 ||
		!((cAIChar *)ai)->vftable->base.GetArrived(&ai->base.base) ||
		strcmp(p->vftable->GetClassName(p), TYEN_WALKCHAR)) {
		return;
	}
	Func_SitDown(self, p);
}

/* vf39 */
static byte cFunctionaryLocalLogic_FastEscape(cLocalLogic *base) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)base;
	return self->fastEscape;
}

/* the probabilities must be in [0, 1] */
static byte Func_VarProb(float *f, double *d, byte set, const char *check) {
	if (!set) {
		*d = (double)*f;
		return 1;
	}
	float v = (float)*d;
	*f = v;
	if (!(v >= 0.0f) || !(1.0f >= v)) {
		CheckFailed(check, __FILE__, __TIMESTAMP__, __LINE__);
	}
	return 1;
}

static byte Func_VarFloat(float *f, double *d, byte set) {
	if (set) {
		*f = (float)*d;
		return 1;
	}
	*d = (double)*f;
	return 1;
}

/* vf04 */
byte cFunctionaryLocalLogic_VarF(cLocalLogic *elem, const char *name, double *d, byte set) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	if (!strcmp(name, "ProbTalkWhenWorking")) {
		return Func_VarProb(&self->probWorking, d, set,
			"probTalkWhenWorking >= 0.0f && probTalkWhenWorking <= 1.0f");
	}
	if (!strcmp(name, "ProbTalkWhenPlaying")) {
		return Func_VarProb(&self->probPlaying, d, set,
			"probTalkWhenPlaying >= 0.0f && probTalkWhenPlaying <= 1.0f");
	}
	if (!strcmp(name, "ProbTalkWhenWalking")) {
		return Func_VarProb(&self->probWalking, d, set,
			"probTalkWhenWalking >= 0.0f && probTalkWhenWalking <= 1.0f");
	}
	if (!strcmp(name, "EscapeRadiusSqr")) {
		return Func_VarFloat(&self->escapeRadiusSqr, d, set);
	}
	if (!strcmp(name, "GoToWorkSpeed")) {
		return Func_VarFloat(&self->goToWorkSpeed, d, set);
	}
	if (!strcmp(name, "TimeToSitDown")) {
		return Func_VarFloat(&self->sitTime, d, set);
	}
	if (!strcmp(name, "TimeToStopWorking")) {
		return Func_VarFloat(&self->workTime, d, set);
	}
	return cBaseLocalLogic_VarF(&self->base.base.logic, name, d, set);
}

/* vf37: the work stops, a new wait before the walk to the desktop (none with a forced desktop) */
void cFunctionaryLocalLogic_StopWork(cLocalLogic *base, byte restart) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)base;
	if (self->working && cFunctionaryLocalLogic_GetDesktop(self)) {
		cFunctionaryLocalLogic_FreeDesktop(self);
	}
	self->relaxed = 0;
	if (!self->forced) {
		self->sitTime = Rand_FloatRange(g_FunctionaryMinTimeToSitDown, g_FunctionaryMaxTimeToSitDown);
	}
	if (restart) {
		Logic_Vtbl(self)->SetState(&self->base.base.logic, 0, NULL);
	}
}

/* a flag variable: set takes 1 only */
static byte Func_VarFlag(byte *b, int *i, byte set) {
	if (set) {
		*b = *i == 1;
		return 1;
	}
	*i = *b != 0;
	return 1;
}

/* vf03 */
static void cFunctionaryLocalLogic_SetKeepWorking(cFunctionaryLocalLogic *self, byte on);
byte cFunctionaryLocalLogic_VarI(cLocalLogic *elem, const char *name, int *i, byte set) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	if (!strcmp(name, "WorkArea")) {
		if (!set) {
			*i = cFunctionaryLocalLogic_GetDesktop(self) != NULL;
			return 1;
		}
		if (*i) {
			Func_Vtbl(self)->AssignDesktop(&self->base.base.logic, 0.0f);
		} else {
			Func_Vtbl(self)->ReleaseDesktop(&self->base.base.logic);
		}
		return 1;
	}
	if (!strcmp(name, "KeepWorking")) {
		if (!set) {
			*i = self->keepWorking != 0;
			return 1;
		}
		cFunctionaryLocalLogic_SetKeepWorking(self, *i != 0);
		return 1;
	}
	if (!strcmp(name, "ForceSpecialBehaviour")) {
		return Func_VarFlag(&self->forceSpecial, i, set);
	}
	if (!strcmp(name, "ForceCancelSpecialBehaviour")) {
		return Func_VarFlag(&self->forceCancel, i, set);
	}
	if (!strcmp(name, "EscapeWithinRadius")) {
		return Func_VarFlag(&self->escapeInRadius, i, set);
	}
	if (!strcmp(name, "FastEscape")) {
		return Func_VarFlag(&self->fastEscape, i, set);
	}
	if (!strcmp(name, "IgnoreAlarm")) {
		return Func_VarFlag(&self->base.ignoreAlarm, i, set);
	}
	if (!strcmp(name, "IgnoreBankerAttack")) {
		return Func_VarFlag(&self->ignoreBanker, i, set);
	}
	if (!strcmp(name, "StopInRelax")) {
		return Func_VarFlag(&self->stopInRelax, i, set);
	}
	return cGenCharLocalLogic_VarI(&self->base.base.logic, name, i, set);
}

/* the relax at the desktop */
static void Functionary_WorkRelax(cEntity *e) {
	if (!e || strcmp(e->vftable->GetClassName(e), TYEN_WALKCHAR)) {
		return;
	}
	cActCtrl_Play(ChType_ActCtrl(e), "WorkRelax", 0, 1.0f);
	cActCtrl_SetDefLoop(ChType_ActCtrl(e), "Work", 0);
}

/* the fast part of the special action: the timer of the character at four times the speed and the sound */
static void Functionary_FastEffect(cEntity *e) {
	vec3_t tmp;

	cTimer_SetSpeedTo(Func_Timer(e), 4.0f);
	cTimer_SetBlend(Func_Timer(e), 0.1f);
	const char *snd = g_FunctionaryEffectFastSound;
	int vol = Snd_Volume(g_FunctionaryEffectFastSoundVol);
	const vec3_t *pos = e->vftable->GetPos(e, &tmp);
	g_pSound->vftable->PlayAt(g_pSound, snd, pos, vol);
}

/* the slow part: half the speed */
static void cFunctionaryLocalLogic_SlowEffect(cFunctionaryLocalLogic *self, cEntity *e) {
	(void)self;
	cTimer_SetSpeedTo(Func_Timer(e), 0.5f);
	cTimer_SetBlend(Func_Timer(e), 0.45f);
}

/* vf17 */
void cFunctionaryLocalLogic_Update(cLocalLogic *elem, float dt) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	vec3_t t1, t2;

	if (self->forceSpecial) {
		GenChar_Vtbl(self)->Work(&self->base.base.logic, dt);
		self->forceSpecial = 0;
		GenChar_Vtbl(self)->UpdateView(&self->base.base.logic);
		return;
	}
	if (self->forceCancel) {
		Func_Vtbl(self)->StopWork(&self->base.base.logic, 1);
		self->forceCancel = 0;
		GenChar_Vtbl(self)->UpdateView(&self->base.base.logic);
		return;
	}
	if (self->special) {
		cBaseLocalLogic_Update(&self->base.base.logic, dt);
		cEntity *p = Func_Parent(self);
		self->transTime = self->transTime + dt;
		float st = self->specialTime + dt;
		self->specialTime = st;
		byte idle = 1;
		if (!self->base.base.idle && !self->base.base.alwaysLock) {
			idle = 0;
			if (!(st >= g_FunctionarySpecialActionTime)) {
				if (Func_Vtbl(self)->FastEscape(&self->base.base.logic) &&
					self->transTime >= g_FunctionarySpecialActionStatesTransitionTime) {
					byte was = self->fast;
					self->transTime = 0.0f;
					self->fast = was == 0;
					if (self->fast) {
						Functionary_FastEffect(p);
					} else {
						cFunctionaryLocalLogic_SlowEffect(self, p);
					}
				}
				GenChar_Vtbl(self)->UpdateView(&self->base.base.logic);
				return;
			}
		}
		self->special = 0;
		self->fast = 0;
		self->transTime = 0.0f;
		self->specialTime = 0.0f;
		if (Func_Vtbl(self)->FastEscape(&self->base.base.logic)) {
			cFunctionaryLocalLogic_NormalSpeed(self, p);
		}
		Logic_Vtbl(self)->SetState(&self->base.base.logic, self->prevState == 3 ? 0 : self->prevState, NULL);
		if (idle) {
			Logic_Vtbl(self)->SetIdle(&self->base.base.logic, 1);
		}
		GenChar_Vtbl(self)->UpdateView(&self->base.base.logic);
		return;
	}
	if (self->escapeInRadius) {
		cBaseLocalLogic_Update(&self->base.base.logic, dt);
		cEntity *usr = cWorld_GetUsrEntity(g_pWorld, 0);
		if (!self->base.base.idle && !self->base.base.alwaysLock && usr) {
			const vec3_t *up = usr->vftable->GetPos(usr, &t1);
			cEntity *p = Func_Parent(self);
			const vec3_t *pp = p->vftable->GetPos(p, &t2);
			float dy = pp->y - up->y, dx = pp->x - up->x, dz = pp->z - up->z;
			if (self->escapeRadiusSqr >= dy * dy + dx * dx + dz * dz) {
				Logic_Vtbl(self)->SetState(&self->base.base.logic, 3, usr);
			}
		}
		GenChar_Vtbl(self)->UpdateView(&self->base.base.logic);
		return;
	}
	cGenCharLocalLogic_Update(&self->base.base.logic, dt);
}

/* vf12: not when IgnoreBankerAttack */
void cFunctionaryLocalLogic_OnRobbery(cLocalLogic *elem, cEntity *e, float amount) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	if (!self->ignoreBanker) {
		cGenCharLocalLogic_OnRobbery(&self->base.base.logic, e, amount);
	}
}

/* vf18: the nearest desktop within FunctionaryAssignWorkDistance instead of the origin */
static void cFunctionaryLocalLogic_SetOriginFrom(cLocalLogic *base, cEntity *e) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)base;
	Func_Vtbl(self)->ReleaseDesktop(&self->base.base.logic);
	Func_Vtbl(self)->AssignDesktop(&self->base.base.logic, g_FunctionaryAssignWorkDistance);
	Logic_Vtbl(self)->SetState(&self->base.base.logic, 0, NULL);
	self->sitTime = 0.1f;
	cEntity *d = cFunctionaryLocalLogic_GetDesktop(self);
	if (d) {
		Con_Debugf("The functionary %s is working at %s \n", e->name, d->name);
		return;
	}
	Con_Debugf("The functionary %s unemployed \n", e->name);
}

/* vf20: not idle from the work state goes on with the walk to the desktop */
void cFunctionaryLocalLogic_SetIdle(cLocalLogic *elem, byte idle) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	if (!idle && !self->base.base.busy) {
		if (self->base.base.state == 8) {
			self->base.base.state = 7;
		}
		Logic_Vtbl(self)->SetState(&self->base.base.logic, self->base.base.state, self->base.base.target.target);
		return;
	}
	cBaseLocalLogic_SetIdle(&self->base.base.logic, idle);
}

/* the walk to the chair of the desktop (node DC_Sit of its model), looking along the work angles */
static void Func_GoToWork(cFunctionaryLocalLogic *self, cAIChar *ai) {
	matrix_t m;
	vec3_t ang = { 0.0f, 0.0f, 0.0f }, dir, pos = { 0.0f, 0.0f, 0.0f };

	if (self->working) {
		cEntity *d = cFunctionaryLocalLogic_GetDesktop(self);
		if (d) {
			int i = CGameModel_FindRenderNode(Obj_Vtbl(d)->GetModel(d), "DC_Sit");
			CHECK(i != -1);
			d = cFunctionaryLocalLogic_GetDesktop(self);
			const matrix_t *mm = CGameModel_NodeMatrix(Obj_Vtbl(d)->GetModel(d), &m, i, 0, NULL, 0);
			pos.x = mm->m[12];
			pos.y = mm->m[13];
			pos.z = mm->m[14];
			ang = *Func_Vtbl(self)->GetWorkAng(&self->base.base.logic, &dir);
		} else {
			CHECK(false && "Aqui no debe de llegar nunca");
		}
	}
	vec3_t fwd = { 0.0f, 0.0f, -1.0f };
	Vec3_Rotate(&dir, &fwd, &ang);
	ai->vftable->SetState(&ai->base.base, 4, pos, dir, 50.0, (double)self->goToWorkSpeed);
}

/* vf19 */
byte cFunctionaryLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base.base;

	if (state == 0) {
		if (self->stopInRelax) {
			return cGenCharLocalLogic_SetState(&self->base.base.logic, 0, target);
		}
		byte ok = (byte)cBaseLocalLogic_SetState(&b->logic, 0, target);
		cAIChar *ai = b->parent->vftable->GetAI(b->parent);
		if (ok) {
			GenChar_Vtbl(self)->UpdateView(&self->base.base.logic);
		}
		if (b->state == 0 && ok) {
			ai->vftable->SetState(&ai->base.base, 6, b->origin, (double)b->actionRadius, (double)g_GenCharRelaxSpeed, 0);
		}
		return 1;
	}
	if (state == 3) {
		self->prevState = b->state == 2 || b->state == 8 ? 0 : b->state;
		if ((byte)cBaseLocalLogic_SetState(&b->logic, 3, target)) {
			cEntity *p = b->parent;
			cAIChar *ai = p->vftable->GetAI(p);
			ai->vftable->SetState(&ai->base.base, 5, target, (double)g_GenCharAlertSpeed);
			byte was = self->special;
			self->fast = 1;
			self->special = 1;
			cObjMove_SetTime(&Obj(p)->move, 0.0f);
			cFunctionaryLocalLogic_FreeDesktop(self);
			if (Func_Vtbl(self)->FastEscape(&self->base.base.logic)) {
				if (!was) {
					p->vftable->SetEmbeddedFX(p, "FunctionarySpecialAction");
				}
				Functionary_FastEffect(p);
			}
		}
		return 1;
	}
	if (state == 7) {
		if ((byte)cBaseLocalLogic_SetState(&b->logic, 7, target)) {
			GenChar_Vtbl(self)->UpdateView(&self->base.base.logic);
			Func_GoToWork(self, b->parent->vftable->GetAI(b->parent));
		}
		return 1;
	}
	if (state != 8) {
		cEntity *p = b->parent;
		if (p && Obj(p)->move.active) {
			cObjMove_SetTime(&Obj(p)->move, 0.0f);
			cFunctionaryLocalLogic_FreeDesktop(self);
		}
		self->relaxed = 0;
		if (!self->forced) {
			self->sitTime = Rand_FloatRange(g_FunctionaryMinTimeToSitDown, g_FunctionaryMaxTimeToSitDown);
		}
	}
	return cGenCharLocalLogic_SetState(&self->base.base.logic, state, target);
}

/* "KeepWorking": on takes a desktop (any distance), off leaves it */
static void cFunctionaryLocalLogic_SetKeepWorking(cFunctionaryLocalLogic *self, byte on) {
	self->keepWorking = on;
	if (on) {
		Func_Vtbl(self)->AssignDesktop(&self->base.base.logic, 0.0f);
		return;
	}
	Func_Vtbl(self)->ReleaseDesktop(&self->base.base.logic);
}

/* vf05 */
byte cFunctionaryLocalLogic_VarS(cLocalLogic *elem, const char *name, char **s, byte set) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	if (strcmp(name, "ForcedDesktopAssigned")) {
		return cGenCharLocalLogic_VarS(&self->base.base.logic, name, s, set);
	}
	if (!set) {
		if (self->forced) {
			*s = self->forced->name;
		}
		return 1;
	}
	cEntityListData *list = (cEntityListData *)cWorld_GetList(g_pWorld, g_DesktopsListName);
	for (Nto1Node *n = Logic_ListFirst(list); n; n = Logic_ListNext(n)) {
		cEntity *d = n->data;
		if (d && !strcmp(d->name, *s)) {
			self->forced = d;
			return 1;
		}
	}
	return 1;
}

/* the normal speed again after the special action */
static void cFunctionaryLocalLogic_NormalSpeed(cFunctionaryLocalLogic *self, cEntity *e) {
	(void)self;
	cTimer_SetSpeedChecked(Func_Timer(e), 1.0f);
	cTimer_SetSpeedTo(Func_Timer(e), 1.0f);
	cTimer_SetBlend(Func_Timer(e), 0.0f);
}

/* vf36: the chance to talk of the state */
static byte cFunctionaryLocalLogic_CanTalk(cLocalLogic *elem) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)elem;
	int st = self->base.base.state;

	if (st == 8) {
		float prob = self->working ? self->probWorking : self->probPlaying;
		float r = (float)(Weap_Rand01() + 0.0);
		return prob >= r;
	}
	if (st != 0 && st != 6 && st != 7) {
		return 0;
	}
	float r = (float)(Weap_Rand01() + 0.0);
	return self->probWalking >= r;
}

/* vf38: the angles of the desktop */
static vec3_t *cFunctionaryLocalLogic_GetWorkAng(cLocalLogic *base, vec3_t *out) {
	cFunctionaryLocalLogic *self = (cFunctionaryLocalLogic *)base;
	CHECK(cFunctionaryLocalLogic_GetDesktop(self) && "desktopAssigned()");
	cEntity *d = cFunctionaryLocalLogic_GetDesktop(self);
	Obj_Vtbl(d)->GetAng(d, out);
	return out;
}

/* the vtable of cFunctionaryLocalLogic */
static const cFunctionaryLocalLogicVtbl cFunctionaryLocalLogic_vftable = {
	.base.base.base.Delete = cFunctionaryLocalLogic_Delete,
	.base.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.base.Vf02 = cFunctionaryLocalLogic_Vf02,
	.base.base.base.VarI = cFunctionaryLocalLogic_VarI,
	.base.base.base.VarF = cFunctionaryLocalLogic_VarF,
	.base.base.base.VarS = cFunctionaryLocalLogic_VarS,
	.base.base.base.VarV = cBaseLocalLogic_VarV,
	.base.base.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.base.base.Refresh = cBaseLocalLogic_Refresh,
	.base.base.Listens = cLocalLogic_ListensTrue,
	.base.base.OnHit = cGenCharLocalLogic_OnHit,
	.base.base.OnSeen = cGenCharLocalLogic_OnSeen,
	.base.base.OnAttack = cFunctionaryLocalLogic_OnRobbery,
	.base.base.OnShot = cBaseLocalLogic_OnShot,
	.base.base.OnHammer = cLocalLogic_OnHammerNop,
	.base.base.GivesMoney = cGenCharLocalLogic_GivesMoney,
	.base.base.OnAlarm = cGenCharLocalLogic_OnAlarm,
	.base.base.Update = cFunctionaryLocalLogic_Update,
	.base.base.SetOriginFrom = cFunctionaryLocalLogic_SetOriginFrom,
	.base.base.SetState = cFunctionaryLocalLogic_SetState,
	.base.base.SetIdle = cFunctionaryLocalLogic_SetIdle,
	.base.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.base.RestartState = cGenCharLocalLogic_RestartState,
	.base.CanAttack = cGenCharLocalLogic_CanAttack,
	.base.UpdateView = cFunctionaryLocalLogic_UpdateView,
	.base.ToRelax = cGenCharLocalLogic_ToRelax,
	.base.ToAlert = cGenCharLocalLogic_ToAlert,
	.base.Accuse = Logic_SetState2,
	.base.FindTarget = cLocalLogic_FindTargetNull,
	.base.TargetDone = cLocalLogic_TargetDoneTrue,
	.base.Work = cFunctionaryLocalLogic_Work,
	.base.Vf33 = cFunctionaryLocalLogic_Vf33,
	.base.GoTo = cGenCharLocalLogic_GoTo,
	.base.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
	.CanTalk = cFunctionaryLocalLogic_CanTalk,
	.StopWork = cFunctionaryLocalLogic_StopWork,
	.GetWorkAng = cFunctionaryLocalLogic_GetWorkAng,
	.FastEscape = cFunctionaryLocalLogic_FastEscape,
	.AssignDesktop = cFunctionaryLocalLogic_AssignDesktop,
	.ReleaseDesktop = cFunctionaryLocalLogic_ReleaseDesktop,
};

cGVar GVar_FunctionarySpecialActionTime;
cGVar GVar_GenCharRelaxSpeed;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_FunctionaryEffectFastSound, &GVar_FunctionaryEffectFastSound)
INIT_GVAR_STRING(Init_GVar_FunctionaryEffectFastSound, Fini_GVar_FunctionaryEffectFastSound, &GVar_FunctionaryEffectFastSound, "FunctionaryEffectFastSound", 0, "Functionary Fast Effect sound name", "FunctionaryEffectFastSound")
INIT_GVAR_FLOAT(Init_GVar_FunctionaryEffectFastSoundVol, &GVar_FunctionaryEffectFastSoundVol, "FunctionaryEffectFastSoundVol", 0, "Functionary Fast Effect sound volume", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_CharProcessReduction, &GVar_CharProcessReduction, "CharProcessReduction", 0, "Beyon this distance, the processing speed will be reduced if no visible.", 2000.0f)
FINI_GVAR_STRING(Fini_GVar_DesktopsFunctionaryListName, &GVar_DesktopsFunctionaryListName)
INIT_GVAR_STRING(Init_GVar_DesktopsFunctionaryListName, Fini_GVar_DesktopsFunctionaryListName, &GVar_DesktopsFunctionaryListName, "DesktopsFunctionaryListName", 1, "Desktops functionary list name", "DesktopsFunctionary")
INIT_GVAR_FLOAT(Init_GVar_FunctionaryMinTimeToSitDown, &GVar_FunctionaryMinTimeToSitDown, "FunctionaryMinTimeToSitDown", 0, "Functionary min time to sit down", 15.0f)
INIT_GVAR_FLOAT(Init_GVar_FunctionaryMaxTimeToSitDown, &GVar_FunctionaryMaxTimeToSitDown, "FunctionaryMaxTimeToSitDown", 0, "Functionary max time to sit down", 30.0f)
INIT_GVAR_FLOAT(Init_GVar_FunctionaryMintimeToStopWorking, &GVar_FunctionaryMintimeToStopWorking, "FunctionaryMintimeToStopWorking", 0, "Functionary min time to get up", 30.0f)
INIT_GVAR_FLOAT(Init_GVar_FunctionaryMaxtimeToStopWorking, &GVar_FunctionaryMaxtimeToStopWorking, "FunctionaryMaxtimeToStopWorking", 0, "Functionary max time to get up", 50.0f)
INIT_GVAR_FLOAT(Init_GVar_FunctionarySpecialActionStatesTransitionTime, &GVar_FunctionarySpecialActionStatesTransitionTime, "FunctionarySpecialActionStatesTransitionTime", 0, "Functionary special action states transition time", 0.5f)
INIT_GVAR_FLOAT(Init_GVar_FunctionarySpecialActionTime, &GVar_FunctionarySpecialActionTime, "FunctionarySpecialActionTime", 0, "Functionary special action time", 1.5f)
INIT_GVAR_FLOAT(Init_GVar_FunctionaryAssignWorkDistance, &GVar_FunctionaryAssignWorkDistance, "FunctionaryAssignWorkDistance", 0, "Distance from the chair to reposisionate the functionary", 600.0f)
FINI_CALL(Fini_GVar_OnFunctionaryStartsWorking, &GVar_OnFunctionaryStartsWorking.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnFunctionaryStartsWorking, Fini_GVar_OnFunctionaryStartsWorking, &GVar_OnFunctionaryStartsWorking, "OnFunctionaryStartsWorking", 0, "Function to be called when a functionary sits down and starts working", "")
INIT_GVAR_FLOAT(Init_GVar_GenCharRelaxSpeed, &GVar_GenCharRelaxSpeed, "GenCharRelaxSpeed", 0, "GenChar speed (Relax)", 0.4f)
