/*
 * Entities/Char/WalkCharControl.cpp of the game: the input of a walking character (include/scrap/walkcharcontrol.h).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/arrowindicator.h"
#include "scrap/staticinit.h"

/* the input actions of the character set the control reads */
#define WCC_SET 3
enum {
	WCC_UP = 2,		/* VRot + */
	WCC_DOWN = 3,
	WCC_TURNL = 4,		/* HRot - */
	WCC_TURNR = 5,
	WCC_FORWARD = 6,	/* Walk + */
	WCC_BACK = 7,
	WCC_STRAFEL = 8,
	WCC_STRAFER = 9,
	WCC_JUMPUP = 10,
	WCC_JUMPDOWN = 11,
	WCC_PADL = 12,		/* the turn of the pad, also a strafe */
	WCC_PADR = 13,
	WCC_CENTER = 14,	/* the view back to the front */
	WCC_STRAFE = 15,	/* the turn keys strafe */
	WCC_RUN = 16,		/* walk instead of run (AlwaysRun) */
	WCC_ACT3 = 17,
	WCC_ACT2 = 18,
	WCC_ACTION = 19,
	WCC_LOCK = 20,
	WCC_NEXTOPT = 21,	/* the options of the arrow of the listener */
	WCC_PREVOPT = 22
};

static cInputAction *WCC_Action(int i) {
	return TABLE_AT(TABLE_AT(g_ActionSets, WCC_SET)->actions, i);
}

/* the value of the action for the player slot (the AI has -1 and reads the slot before) */
static float WCC_Value(int i, int slot) {
	return (&WCC_Action(i)->value[0])[slot];
}

static void WCC_ClearValues(cWalkCharControl *self) {
	self->vRot = 0.0f;
	self->hRot = 0.0f;
	self->strafe = 0.0f;
	self->walk = 0.0f;
	self->jump = 0.0f;
	self->centerTime = g_WalkCameraCenter;
	self->act2 = 0;
	self->prevAct2 = 0;
	self->act3 = 0;
	self->prevAct3 = 0;
	self->act = 0;
	self->prevAct = 0;
}

cWalkCharControl *cWalkCharControl_ctor(cWalkCharControl *self) {
	self->target.target = NULL;
	self->target.data = NULL;
	self->target.next = &self->target;
	self->target.prev = &self->target;
	cWalkCharControl_Clear(self);
	return self;
}

void cWalkCharControl_Clear(cWalkCharControl *self) {
	WCC_ClearValues(self);
	self->disabled = 0;
	self->lockStart = 0;
	self->lock = 0;
	self->target.data = self;
}

void cWalkCharControl_dtor(cWalkCharControl *self) {
	Nto1Node_Done(&self->target);
}

void cWalkCharControl_SetTarget(cWalkCharControl *self, cEntity *e) {
	CHECK(self->target.data);
	Nto1Node_Unlink(&self->target);
	if (e) {
		cEntity_AddLink(e, &self->target);
	}
}

static byte WCC_Var(float *v, double *d, byte set) {
	if (set) {
		*v = (float)*d;
	} else {
		*d = (double)*v;
	}
	return 1;
}

byte cWalkCharControl_VarF(cWalkCharControl *self, const char *name, double *d, byte set) {
	if (!strcmp(name, "Walk")) {
		return WCC_Var(&self->walk, d, set);
	}
	if (!strcmp(name, "Str")) {
		return WCC_Var(&self->strafe, d, set);
	}
	if (!strcmp(name, "HRot")) {
		return WCC_Var(&self->hRot, d, set);
	}
	if (!strcmp(name, "VRot")) {
		return WCC_Var(&self->vRot, d, set);
	}
	if (!strcmp(name, "Act")) {
		if (set) {
			self->act = *d > 0.5;
		} else if (self->act) {
			*d = 1.0;
		} else {
			*d = 0.0;
		}
		return 1;
	}
	return 0;
}

void cWalkCharControl_RestartCenter(cWalkCharControl *self) {
	self->centerTime = g_WalkCameraCenter;
}

void cWalkCharControl_Reset(cWalkCharControl *self, byte on) {
	WCC_ClearValues(self);
	self->lockStart = 0;
	self->lock = 0;
	self->target.data = self;
	self->disabled = on;
}

void cWalkCharControl_Latch(cWalkCharControl *self, cEntity *wle, float dt) {
	(void)wle;
	(void)dt;
	self->prevAct = self->act;
	self->prevAct2 = self->act2;
	self->prevAct3 = self->act3;
}

void cWalkCharControl_Update(cWalkCharControl *self, cEntity *wle, float dt) {
	vec3_t t1, t2, d, a;
	byte b;

	if (self->disabled) {
		return;
	}
	int slot = cWalkCharEntity_GetOwnerSlot(wle);
	self->hRot = 0.0f;
	self->vRot = 0.0f;
	self->walk = 0.0f;
	self->strafe = 0.0f;
	self->jump = 0.0f;
	float turn = WCC_Value(WCC_TURNR, slot) - WCC_Value(WCC_TURNL, slot);
	if (WCC_Value(WCC_STRAFE, slot) > 0.5f) {
		self->strafe = turn;
	} else {
		self->hRot = turn;
	}
	self->vRot = WCC_Value(WCC_UP, slot) + self->vRot;
	self->vRot = self->vRot - WCC_Value(WCC_DOWN, slot);
	self->walk = WCC_Value(WCC_FORWARD, slot) + self->walk;
	self->walk = self->walk - WCC_Value(WCC_BACK, slot);
	self->strafe = WCC_Value(WCC_STRAFER, slot) + self->strafe;
	self->strafe = self->strafe - WCC_Value(WCC_STRAFEL, slot);
	self->jump = WCC_Value(WCC_JUMPUP, slot) + self->jump;
	self->jump = self->jump - WCC_Value(WCC_JUMPDOWN, slot);
	self->strafe = WCC_Value(WCC_PADR, slot) + self->strafe;
	self->strafe = self->strafe - WCC_Value(WCC_PADL, slot);
	float lock = WCC_Value(WCC_LOCK, slot);
	float lockPrev = (&WCC_Action(WCC_LOCK)->prev[0])[slot];
	self->lock = lock > 0.25f;
	if (lock >= 0.850000024f && 0.850000024f > lockPrev) {
		self->lockStart = 1;
	}
	if (g_WalkCameraCenter != 0.0f && ChType_ActClass(ChType_ActCtrl(wle))->fireAsSelect) {
		b = WCC_Value(WCC_ACTION, slot) > 0.5f;
		if (cInputAction_Pressed(WCC_Action(WCC_ACTION), slot)) {
			self->lockStart = 1;
		}
		self->lock = self->lock || b;
	}
	b = WCC_Value(WCC_CENTER, slot) > 0.5f;
	if (b) {
		self->centerTime = g_WalkCameraCenter;
	}
	if (!self->lock) {
		float k = (WCC_Value(WCC_BACK, slot) - WCC_Value(WCC_FORWARD, slot)) * 0.5f + 1.5f;
		k = k * g_PadTurnSpeed;
		if (!b) {
			self->hRot = WCC_Value(WCC_PADR, slot) * k + self->hRot;
			self->hRot = self->hRot - WCC_Value(WCC_PADL, slot) * k;
		}
	} else if (self->target.target) {
		cEntity *t = self->target.target;
		const vec3_t *tp = t->vftable->GetPos(t, &t1);
		const vec3_t *wp = wle->vftable->GetPos(wle, &t2);
		d.x = wp->x - tp->x;
		d.y = wp->y - tp->y;
		d.z = wp->z - tp->z;
		a = WChar(wle)->view;
		const vec3_t *r = Vec3_GetRotAng(&t2, &d);
		float dy = r->y - a.y;
		self->vRot = NormAng(r->x - a.x);
		self->hRot = NormAng(dy);
	}
	if (g_WalkCameraCenter != 0.0f) {
		if (self->hRot == 0.0f && self->vRot == 0.0f && self->walk == 0.0f && self->strafe == 0.0f &&
			self->jump == 0.0f) {
			float c = self->centerTime - dt;
			self->centerTime = c;
			if (!(0.0f < c)) {
				cAction *relax = WithLife_Vtbl(wle)->GetAction(wle, "Relax");
				if (!relax || relax->rotSpeed != 0.0f) {
					self->centerTime = g_WalkCameraCenter;
				} else {
					a = WChar(wle)->view;
					self->vRot = -a.x;
					a = WChar(wle)->view;
					self->hRot = NormAng(Obj_Vtbl(wle)->GetAng(wle, &t1)->y - a.y);
				}
			}
		} else {
			self->centerTime = g_WalkCameraCenter;
		}
	}
	if ((WCC_Value(WCC_RUN, slot) > 0.5f) != (g_AlwaysRun == 0)) {
		self->walk = self->walk * 0.5f;
		self->strafe = self->strafe * 0.5f;
	}
	self->act = WCC_Value(WCC_ACTION, slot) > 0.5f;
	self->act3 = !self->act && WCC_Value(WCC_ACT3, slot) > 0.5f;
	self->act2 = WCC_Value(WCC_ACT2, slot) > 0.5f;
	byte next = cInputAction_Pressed(WCC_Action(WCC_NEXTOPT), slot);
	byte prev = cInputAction_Pressed(WCC_Action(WCC_PREVOPT), slot);
	if ((next || prev) && WChar(wle)->listenOn && WChar_Listen(wle)) {
		cEntity *l = WChar_Listen(wle);
		CHECK(Entity_IsType(l, TYEN_WITH_LIFE));
		if (next) {
			cArrowIndicator_NextOpt(&WLife(l)->arrow);
		}
		if (prev) {
			cArrowIndicator_PrevOpt(&WLife(l)->arrow);
		}
	}
}

cGVar GVar_AlwaysRun;

cGVar GVar_WalkCameraCenter;

cGVar GVar_PadTurnSpeed;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_PadTurnSpeed, &GVar_PadTurnSpeed, "PadTurnSpeed", 2, "Aditional rotation for pad controllers.\n", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_WalkCameraCenter, &GVar_WalkCameraCenter, "WalkCameraCenter", 0, "0 is none else is the time to back to the standard view.\n", 0.0f)
INIT_GVAR_INT(Init_GVar_AlwaysRun, &GVar_AlwaysRun, "AlwaysRun", 2, "1 allways run, 0 allways walk.", 1)
