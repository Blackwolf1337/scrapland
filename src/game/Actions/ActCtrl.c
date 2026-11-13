/*
 * Actions/ActCtrl.cpp of the game: the action controller of an entity (cActCtrl), see include/scrap/actions.h.
 */
#include <float.h>
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/staticinit.h"

#define g_LogActEvents (GVar_LogActEvents.value.i)		/* LogActEvents: the events into the console */
static cGVar GVar_LogActEvents;

static CGameModel *Ctrl_Model(cActCtrl *self) {
	return Obj_GetModel(self->owner);
}

static cTimer *Ctrl_Timer(cActCtrl *self) {
	return &Obj(self->owner)->timer;
}

static int Action_Anim(const cAction *a) {
	return a ? a->anim : -1;
}

/* the end of the action of the layer, -1 without one */
static float Layer_End(const cActLayer *l) {
	return l->action ? l->action->anmTotalTime : -1.0f;
}

static void cActCtrl_InitModel(cActCtrl *self);
cActCtrl *cActCtrl_ctor(cActCtrl *self, cEntity *owner, cActClass *cls) {
	for (int k = 0; k < ACT_LAYERS; k++) {
		self->layers[k].time = 0.0f;
		self->layers[k].on = 0;
		self->layers[k].action = NULL;
		self->layers[k].event = NULL;
	}
	self->headBlend = 1.0f;
	self->headMod = -1;
	self->owner = owner;
	self->cls = cls;
	cActCtrl_InitModel(self);
	self->layers[0].on = 1;
	cAction *a = self->cls->defAction;
	self->layers[0].action = a;
	self->defLoop = a;
	CGameModel *m = Ctrl_Model(self);
	self->layers[0].event = self->layers[0].action->events;
	CGameModel_SetAnim(m, Action_Anim(self->layers[0].action), 0, 0);
	self->rootMove.x = 0.0f;
	self->rootMove.y = 0.0f;
	self->rootMove.z = 0.0f;
	self->rootSum.x = 0.0f;
	self->rootSum.y = 0.0f;
	self->rootSum.z = 0.0f;
	self->rootTime = 0.0f;
	self->walkSpeed = 0.5f;
	if (Entity_IsType(owner, TYEN_WALKCHAR)) {
		cWalkCharEntity_SetYModelDisp(WChar(owner), self->cls->yModelDisp);
		WChar(owner)->relaxTime = self->cls->minTimeToExtraRelax;
	}
	self->cls->users++;
	self->heads = NULL;
	return self;
}

static void cActCtrl_dtor(cActCtrl *self) {
	self->cls->users--;
	Entity_SetModel(self->owner, NULL, 1.0f, 1.0f, 1.0f, 1);
	if (self->heads) {
		operator_delete_array(self->heads);
	}
}

void cActCtrl_Free(cActCtrl *self) {
	cActCtrl_dtor(self);
	operator_delete(self);
}

void cActCtrl_AnimCmd(cActCtrl *self, const char *cmd) {
	CGameModel_AnimCmd(Ctrl_Model(self), cmd);
}

/* the model of the class on the owner with the animations of the actions */
static void cActCtrl_InitModel(cActCtrl *self) {
	Entity_SetModel(self->owner, self->cls->modelFile, 1.0f, 1.0f, 1.0f, 0);
	CGameModel *m = Ctrl_Model(self);
	for (cAction *a = self->cls->first; a; a = a->next) {
		int idx = CGameModel_FindAnim(m, a->anmName);
		if (idx == -1) {
			idx = CGameModel_AddAnim(m, a->anmFile);
		}
		CHECK(idx == a->anim);
	}
	if (Entity_IsType(self->owner, TYEN_WITH_LIFE)) {
		Entity_SetEmbeddedFX(self->owner, CGameModel_Name(m));
	}
}

/* the layers dt on: speeds, events, the end of the actions, the root motion of layer 0 */
static void cActCtrl_FireEvents(cActCtrl *self, int chan, float t, byte silent);
static void cActCtrl_Update(cActCtrl *self, float dt) {
	CGameModel *m = Ctrl_Model(self);
	vec3_t move;
	int jumps;

	for (int k = 0; k < ACT_LAYERS; k++) {
		cActLayer *l = &self->layers[k];
		if (!l->on) {
			continue;
		}
		cAction *a = l->action;
		CGameModel_SetAnimFactor(m, Action_Anim(a),
			(1.0f - self->walkSpeed) * a->minSpeed + a->maxSpeed * self->walkSpeed);
		CGameModel_SetAnimLoop(m, Action_Anim(l->action), a->isLoop);
	}
	CGameModel_RootMotion(m, dt, &move, &jumps);
	for (int k = 0; k < ACT_LAYERS; k++) {
		cActLayer *l = &self->layers[k];
		if (!l->on) {
			continue;
		}
		cAction *a = l->action;
		CGameModel_ChanTime(m, k, &l->time);
		if (a->isLoop && jumps > 0) {
			/* the loop went round: the rest of its events, then from the first one again */
			cActCtrl_FireEvents(self, k, Layer_End(l), 0);
			a = l->action;
			l->event = a->events;
		}
		cActCtrl_FireEvents(self, k, l->time, 0);
		if (!a->isLoop && CGameModel_ChanEnd(m, k)) {
			cActCtrl_FireEvents(self, k, Layer_End(l), 0);
			if (k != 0) {
				CGameModel_StopAnim(m, k);
				l->on = 0;
				continue;
			}
			if (self->layers[0].action != self->defLoop &&
				Action_Anim(self->layers[0].action) == self->defLoop->anim) {
				CGameModel_StopAnim(m, 0);
			}
			cActCtrl_PlayAction(self, self->defLoop, 1, 0, 1.0f);
		} else if (k != 0) {
			continue;
		}
		self->rootSum.x = self->rootSum.x + move.x;
		self->rootSum.y = move.y + self->rootSum.y;
		self->rootSum.z = move.z + self->rootSum.z;
		float t = dt + self->rootTime;
		float inv = 1.0f / t;
		self->rootTime = t;
		self->rootMove.x = self->rootSum.x * inv;
		self->rootMove.y = self->rootSum.y * inv;
		self->rootMove.z = self->rootSum.z * inv;
	}
}

/* the events of the layer chan up to t (FLT_MAX from the end of the action on); silent skips the discardable ones
 * and logs with Con_Verbosef */
static void cActCtrl_FireEvents(cActCtrl *self, int chan, float t, byte silent) {
	cActLayer *l = &self->layers[chan];

	if (t >= Layer_End(l)) {
		t = FLT_MAX;
	}
	for (cActEvent *ev = l->event; ev; ev = l->event) {
		if (ev->time >= t) {
			return;
		}
		if (silent && ev->discardable) {
			l->event = ev->next;
			continue;
		}
		cAction *a = l->action;
		l->event = ev->next;
		cActEvent_Fire(ev, self->owner->name);
		if (g_LogActEvents) {
			if (silent) {
				Con_Verbosef("Event \"%s\" (%i,%s)\n", self->owner->name, chan, ev->name);
			} else {
				Con_Debugf("Event \"%s\" (%i,%s)\n", self->owner->name, chan, ev->name);
			}
		}
		/* the event played another action */
		if (ev->next != l->event || a != l->action) {
			return;
		}
	}
}

byte cActCtrl_VarI(cActCtrl *self, const char *name, int *i, int set) {
	if (set && strcmp(name, "StopSecondary") == 0) {
		cActCtrl_StopChan(self, *i);
		return 1;
	}
	return 0;
}

const char *cActCtrl_GetActionName(void *ctrl, int i) {
	cActLayer *l = &((cActCtrl *)ctrl)->layers[i];

	if (l->on && l->action) {
		return l->action->name;
	}
	return str_Empty;
}

cActLayer *cActCtrl_GetLayer(void *ctrl, int i) {
	return &((cActCtrl *)ctrl)->layers[i];
}

const char *cActCtrl_GetDefLoopName(void *ctrl) {
	cActCtrl *self = ctrl;

	return self->defLoop ? self->defLoop->name : str_Empty;
}

static void cActCtrl_InitHeads(cActCtrl *self) {
	if (self->heads) {
		return;
	}
	cActHead *h = operator_new_array(ACT_NODEMODS * sizeof(cActHead));
	if (h) {
		for (int k = 0; k < ACT_NODEMODS; k++) {
			h[k].cur[0] = 0.0f;
			h[k].cur[1] = 0.0f;
			h[k].to[0] = 0.0f;
			h[k].to[1] = 0.0f;
			h[k].from[0] = 0.0f;
			h[k].from[1] = 0.0f;
		}
	}
	self->heads = h;
}

/* the target of the blend set (the old one becomes the start) */
static void cActHead_Set(cActHead *self, const float *to) {
	self->from[0] = self->to[0];
	self->from[1] = self->to[1];
	self->to[0] = to[0];
	self->to[1] = to[1];
	self->cur[0] = self->to[0];
	self->cur[1] = self->to[1];
}

void cActCtrl_TurnHead(void *ctrl, const float *ang, float dt) {
	cActCtrl *self = ctrl;
	cActClass *cls;

	if (self->headMod != self->layers[0].action->nodeModifier) {
		/* a new modifier: blend from where the old blend is */
		self->headMod = self->layers[0].action->nodeModifier;
		self->headBlend = 1.0f - self->headBlend;
		for (int k = 0; k < ACT_NODEMODS; k++) {
			if (self->cls->nodes[k].node == ACT_NODE_NONE) {
				break;
			}
			cActCtrl_InitHeads(self);
			int mod = self->layers[0].action->nodeModifier;
			if (mod != -1) {
				cActHead_Set(&self->heads[k], self->cls->nodes[k].ang[mod]);
			} else {
				cActHead *h = &self->heads[k];
				h->from[0] = h->to[0];
				h->from[1] = h->to[1];
				h->to[1] = 0.0f;
				h->to[0] = 0.0f;
				h->cur[0] = 0.0f;
				h->cur[1] = h->to[1];
			}
		}
	}
	float b = self->layers[0].action->angleModifierSpeed * dt + self->headBlend;
	float f = 1.0f;
	self->headBlend = b;
	if (b > 1.0f) {
		self->headBlend = 1.0f;
	} else {
		float s = (3.0f - (b + b)) * (b * b);
		f = (3.0f - (s + s)) * (s * s);
	}
	for (int k = 0; k < ACT_NODEMODS; k++) {
		cls = self->cls;
		int node = cls->nodes[k].node;
		if (node == ACT_NODE_NONE) {
			return;
		}
		cActCtrl_InitHeads(self);
		cActHead *h = &self->heads[k];
		float r = 1.0f - f;
		h->cur[1] = h->from[1] * r + h->to[1] * f;
		h->cur[0] = h->from[0] * r + h->to[0] * f;
		vec3_t turn;
		turn.y = self->heads[k].cur[1] * ang[1];
		turn.x = self->heads[k].cur[0] * ang[0];
		turn.z = 0.0f;
		if (self->cls->nodes[k].node == ACT_NODE_MODEL) {
			vec3_t rot, v;
			const vec3_t *r0 = CGameModel_GetRot(Ctrl_Model(self), &rot);
			v.y = r0->y + turn.y;
			v.z = r0->z + turn.z;
			v.x = r0->x + turn.x;
			CGameModel_SetRot(Ctrl_Model(self), &v);
		} else {
			CGameModel_NodeLook(Ctrl_Model(self), node, 5, &turn);
		}
	}
}

byte cActCtrl_PlayAction(void *ctrl, void *action, int arg, int set, float speed) {
	cActCtrl *self = ctrl;
	cAction *a = action;

	if (!a) {
		return 0;
	}
	int chan = a->chanel;
	cActLayer *l = &self->layers[chan];
	if (l->action && !(byte)arg && l->action->priority > a->priority) {
		return 0;
	}
	if ((byte)set) {
		cActCtrl_FireEvents(self, chan, Layer_End(l), 1);
	}
	l->on = 1;
	l->action = a;
	l->time = 0.0f;
	CGameModel *m = Ctrl_Model(self);
	if (cTimer_GetSpeed(Ctrl_Timer(self)) > 0.0f) {
		cAction *cur = l->action;
		int anim = Action_Anim(cur);
		float v = cur->interpTime * speed;
		CGameModel_SetAnimSpeed(m, anim, v / cTimer_GetSpeed(Ctrl_Timer(self)));
	}
	CGameModel_SetAnim(m, Action_Anim(l->action), chan, 0);
	l->event = l->action->events;
	return 1;
}

byte cActCtrl_Play(void *ctrl, const char *action, int arg, float speed) {
	cActCtrl *self = ctrl;

	return cActCtrl_PlayAction(self, cActClass_GetAction(self->cls, action), arg, 1, speed);
}

byte cActCtrl_PlayNew(void *ctrl, const char *action, int arg, float speed) {
	cActCtrl *self = ctrl;
	cAction *a = cActClass_GetAction(self->cls, action);

	if (a && self->layers[a->chanel].action != a) {
		return cActCtrl_PlayAction(self, a, arg, 1, speed);
	}
	return 0;
}

static void DefLoop_Warn(cActCtrl *self, const char *action) {
	Con_Warnf("WARNING: cActCtrl::SetDefLoop '%s' has no action '%s'\n", self->cls->name, action);
}

void cActCtrl_SetDefLoop(void *ctrl, const char *action, byte force) {
	cActCtrl *self = ctrl;
	cAction *a = cActClass_GetAction(self->cls, action);

	if (!a) {
		DefLoop_Warn(self, action);
		return;
	}
	if (!self->defLoop || force || self->defLoop->priority <= a->priority) {
		self->defLoop = a;
	}
}

/* the time of the layer wrapped into its action, the part of the action that it is */
static float Ship_Wrap(cActLayer *l, cAction *a, float t) {
	t = (float)fmod((double)t, (double)Layer_End(l));
	l->time = t;
	return t / (a ? a->anmTotalTime : -1.0f);
}

/* the view angle z of the ship */
static float Ship_AngZ(cVehicleEntity *v) {
	vec3_t ang;

	return Obj_GetViewAng(Veh_Ent(v), &ang)->z;
}

/* the update of DinaShip classes: the parts of the ship (Motor, Rudder, Flaps, ...) by its state */
static void cActCtrl_UpdateShip(cActCtrl *self, float dt) {
	cVehicleEntity *v = (cVehicleEntity *)self->owner;
	CGameModel *m = Ctrl_Model(self);

	for (int k = 0; k < ACT_LAYERS; k++) {
		cActLayer *l = &self->layers[k];
		if (!l->on) {
			continue;
		}
		cAction *a = l->action;
		const char *name = a->name;
		/* the part of the action to show (0..1), from the state of the ship by the name of the action */
		float f = 0.0f;
		if (strcmp(name, "Motor") == 0) {
			f = Ship_Wrap(l, a,
				(float)((__builtin_fabs((double)v->smoothThrust) * 2.0 + 0.5) * (double)dt + (double)l->time));
		} else if (strcmp(name, "Helix") == 0) {
			float end = Layer_End(l);
			f = Ship_Wrap(l, a,
				(float)(((double)v->smoothThrust + 0.1) * 3.0 * (double)dt + (double)l->time + (double)end));
		} else if (strcmp(name, "Loop") == 0) {
			f = Ship_Wrap(l, a, l->time + dt);
		} else if (strcmp(name, "Boost") == 0) {
			f = 1.0f - v->shake;
		} else if (strcmp(name, "Flaps") == 0) {
			f = v->smoothThrust + 0.5f;
		} else if (strcmp(name, "VRudder") == 0) {
			f = 0.5f - v->smoothRotVel.x * 0.75f / M_PI_F;
		} else if (strcmp(name, "Rudder") == 0) {
			f = Ship_AngZ(v) * 3.0f / M_PI_F + 0.5f;
		} else if (strcmp(name, "Left") == 0) {
			float rv = v->smoothRotVel.x;
			f = 0.5f - Ship_AngZ(v) * 3.0f / M_PI_F + rv / M_2PI_F;
		} else if (strcmp(name, "Right") == 0) {
			float rv = v->smoothRotVel.x;
			float z = Ship_AngZ(v);
			f = z * 3.0f / M_PI_F + 0.5f + rv / M_2PI_F;
		} else if (strcmp(name, "LeftFlap") == 0) {
			float thrust = v->smoothThrust;
			f = thrust * 0.25f + 0.25f;
			if (thrust > 0.0f && Ship_AngZ(v) > 0.0f) {
				float t2 = v->smoothThrust;
				f = f + Ship_AngZ(v) * t2;
			}
		} else if (strcmp(name, "RightFlap") == 0) {
			float thrust = v->smoothThrust;
			f = thrust * 0.25f + 0.25f;
			if (thrust > 0.0f && 0.0f > Ship_AngZ(v)) {
				float t2 = v->smoothThrust;
				f = f - Ship_AngZ(v) * t2;
			}
		}
		float t;
		if (0.0f > f) {
			t = 0.0f;
		} else if (f > 1.0f) {
			t = Layer_End(l);
		} else {
			t = Layer_End(l) * f;
		}
		CGameModel_AnimDelta(m, k, t, NULL);
	}
}

/* the proxy of a watcher (include/scrap/replicate.h): the actions of the layers from the network (index + 1 in the
 * class, 0 none), each started when it changes and played at rate (its time per second, 0 keeps the speed) with the
 * model's own animation update; no events and no ends of actions */
void cActCtrl_NetDrive(cActCtrl *self, const uint16_t *act, const float *rate, int layers, float dt) {
	CGameModel *m = Ctrl_Model(self);

	for (int k = 0; k < layers && k < ACT_LAYERS; k++) {
		cActLayer *l = &self->layers[k];
		cAction *a = act[k] ? self->cls->first : NULL;
		for (int i = 1; a && i < act[k]; i++) {
			a = a->next;
		}
		if (a != l->action) {
			if (!a) {
				if (k > 0) {
					CGameModel_StopAnim(m, k);
					l->on = 0;
					l->action = NULL;
				}
				continue;
			}
			l->on = 1;
			l->action = a;
			l->event = NULL;
			CGameModel_SetAnimLoop(m, Action_Anim(a), a->isLoop);
			CGameModel_SetAnim(m, Action_Anim(a), k, 0);
		}
		if (l->on && rate[k] > 0.0f) {
			CGameModel_SetAnimFactor(m, Action_Anim(l->action), rate[k]);
		}
	}
	CGameModel_RootMotion(m, dt, NULL, NULL);
	for (int k = 0; k < layers && k < ACT_LAYERS; k++) {
		if (self->layers[k].on) {
			CGameModel_ChanTime(m, k, &self->layers[k].time);
		}
	}
}

void cActCtrl_Tick(cActCtrl *self, float dt) {
	if (self->cls->dynShip) {
		cActCtrl_UpdateShip(self, dt);
	} else {
		cActCtrl_Update(self, dt);
	}
}

void cActCtrl_StopChan(cActCtrl *self, int i) {
	CHECK(i > 0);
	cActLayer *l = &self->layers[i];
	l->on = 0;
	l->action = NULL;
	l->time = 0.0f;
	CGameModel_StopAnim(Ctrl_Model(self), i);
	l->event = NULL;
}

byte cActCtrl_VarS(cActCtrl *self, const char *name, char **s, int set) {
	cAction *a;

	if (set) {
		if (strcmp(name, "ForceAction") == 0) {
			cActCtrl_PlayAction(self, cActClass_GetAction(self->cls, *s), 1, 1, 1.0f);
			return 1;
		}
		if (strcmp(name, "ForceDefLoop") == 0) {
			const char *n = *s;
			a = cActClass_GetAction(self->cls, n);
			if (a) {
				self->defLoop = a;
			} else {
				DefLoop_Warn(self, n);
			}
			return 1;
		}
	}
	if (strcmp(name, "Action") == 0) {
		if (set) {
			cActCtrl_PlayAction(self, cActClass_GetAction(self->cls, *s), 0, 1, 1.0f);
		} else if (self->layers[0].on && self->layers[0].action) {
			*s = self->layers[0].action->name;
		} else {
			*s = str_Empty;
		}
		return 1;
	}
	/* "ActChan0".."ActChan3" */
	if (!set && crt_strncmp(name, "ActChan", 7) == 0 && name[8] == 0 && name[7] >= '0' && name[7] < '4') {
		*s = (char *)cActCtrl_GetActionName(self, name[7] - '0');
		return 1;
	}
	if (strcmp(name, "DefLoop") != 0) {
		return 0;
	}
	if (set) {
		const char *n = *s;
		a = cActClass_GetAction(self->cls, n);
		if (!a) {
			DefLoop_Warn(self, n);
			return 1;
		}
		if (self->defLoop && a->priority < self->defLoop->priority) {
			return 1;
		}
		self->defLoop = a;
		return 1;
	}
	*s = self->defLoop ? self->defLoop->name : str_Empty;
	return 1;
}

byte cActCtrl_IsActive(void *ctrl, const char *action) {
	cActCtrl *self = ctrl;

	for (int k = 0; k < ACT_LAYERS; k++) {
		cActLayer *l = &self->layers[k];
		if (l->on && l->action && strcmp(action, l->action->name) == 0) {
			return 1;
		}
	}
	return 0;
}

/* the slot VarF of a class that does only return 0 */
byte cActCtrl_VarFFalse(cActCtrl *self, const char *name, double *d, int set) {
	(void)self;
	(void)name;
	(void)d;
	(void)set;
	return 0;
}

/* the slot VarV of a class that does only return 0 */
byte cActCtrl_VarVFalse(cActCtrl *self, const char *name, double *x, double *y, double *z, int set) {
	(void)self;
	(void)name;
	(void)x;
	(void)y;
	(void)z;
	(void)set;
	return 0;
}

/* the vtable of cActCtrl */

static cGVar GVar_LogActEvents;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_LogActEvents, &GVar_LogActEvents, "LogActEvents", 0, "Drops to the console the log events.", 0)
