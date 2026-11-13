/*
 * The entity of a player of the network on both sides: its input into the controls, one tick of it, the motion state
 * of a correction, see include/scrap/play.h. Not in the original.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/global.h"
#include "scrap/input.h"
#include "scrap/play.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

#define NETPL_TICK (1.0f / NETREP_TICK_HZ)
#define NETPL_SLOT 0.0166666675f		/* 1/60: the slot of a player's character and ship (SetUsrControl) */

int NetPl_Kind(cEntity *e) {
	if (!e || e->deadNode.target) {
		return 0;
	}
	if (Entity_IsType(e, TYEN_CAR)) {
		return NETREP_SHIP;
	}
	return Entity_IsType(e, TYEN_WALKCHAR) ? NETREP_CHAR : 0;
}

static inline float NetPl_Move(int8_t v) {
	return (float)v / NETREP_MOVE_SCALE;
}

static inline float NetPl_Rot(int16_t v) {
	return (float)v / NETREP_ROT_SCALE;
}

void NetPl_Apply(cEntity *e, const NetRepInput *in, byte local) {
	uint16_t b = local ? 0 : in->buttons;

	switch (NetPl_Kind(e)) {
	case NETREP_CHAR: {
		cWalkCharControl *c = &WChar(e)->control;
		c->walk = NetPl_Move(in->move[0]);
		c->strafe = NetPl_Move(in->move[1]);
		c->jump = NetPl_Move(in->move[2]);
		c->hRot = NetPl_Rot(in->rot[0]);
		c->vRot = NetPl_Rot(in->rot[1]);
		c->act = (b & NETREP_IN_ACT) != 0;
		c->act2 = (b & NETREP_IN_ACT2) != 0;
		c->act3 = !c->act && (b & NETREP_IN_ACT3) != 0;
		break;
	}
	case NETREP_SHIP: {
		cVehicleControls *c = &Veh(e)->controls;
		c->acc = NetPl_Move(in->move[0]);
		c->str = NetPl_Move(in->move[1]);
		c->vStr = NetPl_Move(in->move[2]);
		c->hRot = NetPl_Rot(in->rot[0]);
		c->vRot = NetPl_Rot(in->rot[1]);
		c->fire = (b & NETREP_IN_ACT) != 0;
		c->cm = (b & NETREP_IN_ACT2) != 0;
		c->boostKey = (b & NETREP_IN_BOOST) != 0;
		c->fullThrust = (in->buttons & NETREP_IN_FULL) != 0;
		if (!local && in->weapon < g_pWorld->weapons.count) {
			c->weapon = in->weapon;
		}
		break;
	}
	}
}

void NetPl_Step(cEntity *e, const NetRepInput *in, byte local, byte replay) {
	cWithLifeEntity *w = WLife(e);

	NetPl_Apply(e, in, local);
	if (!replay && w->actCtrl) {
		cActCtrl_Tick(w->actCtrl, NETPL_TICK);
	}
	for (int i = 0; i < PLAY_STEPS; i++) {
		Obj_Tick(e, NETPL_SLOT);
	}
}

void NetPl_Accumulate(cEntity *e, NetPlAccum *a, float dt) {
	float m[3], r[2];
	uint16_t b = 0;

	switch (NetPl_Kind(e)) {
	case NETREP_CHAR: {
		const cWalkCharControl *c = &WChar(e)->control;
		m[0] = c->walk;
		m[1] = c->strafe;
		m[2] = c->jump;
		r[0] = c->hRot;
		r[1] = c->vRot;
		b = (c->act ? NETREP_IN_ACT : 0) | (c->act2 ? NETREP_IN_ACT2 : 0) |
			(c->act3 ? NETREP_IN_ACT3 | NETREP_IN_SWITCH : 0) | (c->lock ? NETREP_IN_LOCK : 0);
		break;
	}
	case NETREP_SHIP: {
		const cVehicleControls *c = &Veh(e)->controls;
		cInputAction *info = TABLE_AT(TABLE_AT(g_ActionSets, ACTSET_RACER)->actions, RACER_MISSION_INFO);
		m[0] = c->acc;
		m[1] = c->str;
		m[2] = c->vStr;
		r[0] = c->hRot;
		r[1] = c->vRot;
		b = (c->fire ? NETREP_IN_ACT : 0) | (c->cm ? NETREP_IN_ACT2 : 0) | (c->boostKey ? NETREP_IN_BOOST : 0) |
			(c->fullThrust ? NETREP_IN_FULL : 0) | (info->value[0] > 0.5f ? NETREP_IN_SWITCH : 0);
		a->weapon = (uint8_t)c->weapon;
		break;
	}
	default:
		return;
	}
	for (int k = 0; k < 3; k++) {
		a->move[k] += m[k] * dt;
	}
	a->rot[0] += r[0] * dt;
	a->rot[1] += r[1] * dt;
	a->time += dt;
	a->buttons |= b;
}

void NetPl_Input(NetPlAccum *a, NetRepInput *out) {
	if (a->time <= 0.0f) {
		return;			/* no frame since the last tick: the input before again */
	}
	float k = 1.0f / a->time;
	for (int i = 0; i < 3; i++) {
		out->move[i] = NetRep_QMove(a->move[i] * k);
	}
	out->rot[0] = NetRep_QRot(a->rot[0] * k);
	out->rot[1] = NetRep_QRot(a->rot[1] * k);
	out->buttons = a->buttons;
	out->weapon = a->weapon;
	uint8_t weapon = a->weapon;
	memset(a, 0, sizeof(*a));
	a->weapon = weapon;
}

void NetPl_Save(cEntity *e, NetPlMotion *m) {
	cObjEntity *o = Obj(e);

	memset(m, 0, sizeof(*m));
	m->pos = e->pos;
	m->vel = o->vel;
	m->ang = o->ang;
	m->rotVel = o->rotVel;
	if (NetPl_Kind(e) == NETREP_CHAR) {
		cWalkCharEntity *w = WChar(e);
		m->view = w->view;
		m->viewVel = w->viewVel;
		m->slideTime = w->slideTime;
		m->displAng = w->displAng;
		m->lastMove = w->lastMove;
		m->lastDisplAng = w->lastDisplAng;
		m->walkTime = w->walkTime;
		m->move = w->move;
		m->moved2 = w->moved2;
		m->fallVel = w->fallVel;
		m->falling = w->falling;
		m->walking = w->walking;
	} else if (NetPl_Kind(e) == NETREP_SHIP) {
		cVehicleEntity *v = Veh(e);
		m->speed = v->speed;
		m->smoothSpeed = v->smoothSpeed;
		m->thrust = v->thrust;
		m->turnSpeedX = v->turnSpeedX;
		m->turnSpeedY = v->turnSpeedY;
		m->roll = v->roll;
		m->boost = v->boost;
		m->pushTime = v->pushTime;
		m->push = v->push;
	}
}

void NetPl_Restore(cEntity *e, const NetPlMotion *m) {
	cObjEntity *o = Obj(e);

	e->pos = m->pos;
	o->vel = m->vel;
	o->ang = m->ang;
	o->rotVel = m->rotVel;
	if (NetPl_Kind(e) == NETREP_CHAR) {
		cWalkCharEntity *w = WChar(e);
		w->view = m->view;
		w->viewVel = m->viewVel;
		w->slideTime = m->slideTime;
		w->displAng = m->displAng;
		w->lastMove = m->lastMove;
		w->lastDisplAng = m->lastDisplAng;
		w->walkTime = m->walkTime;
		w->move = m->move;
		w->moved2 = m->moved2;
		w->fallVel = m->fallVel;
		w->falling = m->falling;
		w->walking = m->walking;
	} else if (NetPl_Kind(e) == NETREP_SHIP) {
		cVehicleEntity *v = Veh(e);
		v->speed = m->speed;
		v->smoothSpeed = m->smoothSpeed;
		v->thrust = m->thrust;
		v->turnSpeedX = m->turnSpeedX;
		v->turnSpeedY = m->turnSpeedY;
		v->roll = m->roll;
		v->boost = m->boost;
		v->pushTime = m->pushTime;
		v->push = m->push;
	}
}

/* the fields of the constants of a ship, in the order of the player message (the trusts, speed and friction
 * follow from them each slot: cVehicleEntity_UpdateTrust) */
static float *NetPl_PhysField(cEntity *e, int i) {
	cVehicleEntity *v = Veh(e);
	float *f[] = { &v->trustTime, &v->boostTime, &v->style, &v->engine, &v->wl.mass, &v->wRadius,
		&v->wl.prim.radius };
	return i < (int)(sizeof(f) / sizeof(f[0])) ? f[i] : NULL;
}

#define NETPL_PHYS_MINLIFE 7			/* the int MinLife after the fields */

int NetPl_GetPhys(cEntity *e, float *phys) {
	int n = 0;
	if (NetPl_Kind(e) == NETREP_SHIP) {
		for (float *f; (f = NetPl_PhysField(e, n)) != NULL; n++) {
			phys[n] = *f;
		}
		phys[n++] = (float)Veh(e)->minLife;
	}
	return n;
}

void NetPl_SetPhys(cEntity *e, const float *phys, int n) {
	if (NetPl_Kind(e) != NETREP_SHIP) {
		return;
	}
	for (int i = 0; i < n; i++) {
		float *f = NetPl_PhysField(e, i);
		if (f) {
			*f = phys[i];
		} else if (i == NETPL_PHYS_MINLIFE) {
			Veh(e)->minLife = (int)phys[i];
		}
	}
}
