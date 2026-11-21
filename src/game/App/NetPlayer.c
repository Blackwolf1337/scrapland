/*
 * The entity of a player of the network on both sides: its input into the controls, one tick of it, the motion state
 * of a correction, see include/scrap/play.h. Not in the original.
 */
#include <math.h>
#include <string.h>

#include "scrap/actions.h"
#include "scrap/collisiongrid.h"
#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/input.h"
#include "scrap/inputtape.h"
#include "scrap/itementity.h"
#include "scrap/play.h"
#include "scrap/replicate.h"
#include "scrap/repent.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"


int NetPl_Kind(cEntity *e) {
	if (!e || e->deadNode.target) {
		return 0;
	}
	if (Entity_IsType(e, TYEN_CAR)) {
		return NETREP_SHIP;
	}
	return Entity_IsType(e, TYEN_WALKCHAR) ? NETREP_CHAR : 0;
}

byte NetPl_LocalWeapon(int w) {
	return w == g_VulcanIDWeap || w == g_LaserIDWeap || w == g_TeslaIDWeap || w == g_ATPCIDWeap ||
		w == g_DevastatorIDWeap;
}

byte NetPl_LaunchWeapon(int w) {
	return w == g_SwarmIDWeap || w == g_InfernoIDWeap;
}

float NetPl_Aim(cEntity *e, const vec3_t *at, float rot[2], byte rel) {
	/* yaw 0 looks to -z (forward = (-sin yaw, 0, -cos yaw)) */
	float dx = at->x - e->pos.x, dy = at->y - e->pos.y, dz = at->z - e->pos.z;
	float want = atan2f(-dx, -dz);
	float dyaw = remainderf(want - (NetPl_Kind(e) == NETREP_CHAR ? WChar(e)->view.y : Obj(e)->ang.y), M_2PI_F);
	float pitch = remainderf(atan2f(dy, sqrtf(dx * dx + dz * dz)) - (rel ? Obj(e)->ang.x : 0.0f), M_2PI_F);
	rot[0] = dyaw > 0.6f ? 1.0f : dyaw < -0.6f ? -1.0f : dyaw / 0.6f;
	rot[1] = pitch > 0.3f ? 0.6f : pitch < -0.3f ? -0.6f : pitch * 2.0f;
	return dyaw;
}

static inline float NetPl_Move(int8_t v) {
	return (float)v / NETREP_MOVE_SCALE;
}

static inline float NetPl_Rot(int16_t v) {
	return (float)v / NETREP_ROT_SCALE;
}

/* the buttons a -mp2 client runs on its own entity (stage 3.5): all but the flows of the scripts, which start on the
 * server (a character's use and possession) and the countermeasures (their missiles are the server's entities) */
#define NETPL_MP2_LOCAL (NETREP_IN_ACT | NETREP_IN_BOOST | NETREP_IN_FULL | NETREP_IN_LOCK)

#define NETPL_PARK_MODEL "models/Misc/Parking/Parking.m3d"
#define NETPL_PARK_REACH 2000.0f		/* units beyond the ship's radius the grid is asked for boxes (a box's radius: 1000) */
#define NETPL_PARK_SLOTS 3				/* slots of 1/60 s from one touch to the next: the item's slot of 0.05 s */
#define NETPL_PARK_MAX 4				/* boxes touched at once */

void NetPl_Apply(cEntity *e, const NetRepInput *in, byte local, byte replay) {
	uint16_t b = !local ? in->buttons : g_Mp2 ? in->buttons & NETPL_MP2_LOCAL : 0;

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
		/* the client fires its instant-hit weapons itself and launches its missile weapons (their effects: the
		 * missiles come from the server), not again in a replay */
		byte own = NetPl_LocalWeapon(c->weapon) || NetPl_LaunchWeapon(c->weapon);
		c->fire = ((local ? (replay || !own ? 0 : in->buttons) : b) & NETREP_IN_ACT) != 0;
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

void NetPl_Actions(cEntity *e, float dt) {
	cWithLifeEntity *w = WLife(e);
	if (w->actCtrl) {
		cActCtrl_Tick(w->actCtrl, dt);
	}
}

void NetPl_Slot(cEntity *e, const NetRepInput *in, byte local, byte replay, int part, float iv) {
	NetRepInput slot = *in;

	if (part) {
		memcpy(slot.move, in->move2, sizeof(slot.move));
	}
	slot.buttons = NetRep_InSlot(in->buttons, part);
	NetPl_Apply(e, &slot, local, replay);
	/* the solo instance: the exact controls and slot lengths of the input tape (include/scrap/inputtape.h) */
	byte taped = g_InputTape == INPUTTAPE_SOLO && !local;
	if (taped) {
		InputTape_SoloApply(e, part);
		iv = InputTape_SoloInterval(part, iv);
	}
	Obj_Tick(e, iv);
	if (taped) {
		InputTape_SlotDone(e);
	}
}

/* the entity NetPl_StepPart steps now (its actions and its slot), the slot of that step (its input's tick *
 * PLAY_STEPS + part) and the draws of NetPl_StepRand in it */
static cEntity *netPlStepping;
static uint32_t netPlStepSlot, netPlStepDraws;

byte NetPl_StepRand(const cEntity *e, int *r) {
	if (!g_Mp2 || !netPlStepping || e != netPlStepping) {
		return 0;
	}
	uint32_t h = netPlStepSlot * 2654435761u ^ ++netPlStepDraws * 40503u;
	h ^= h >> 15;
	h *= 0x2c1b3c6du;
	h ^= h >> 12;
	*r = (int)(h & 0x7fff);
	return 1;
}

byte NetPl_StepBrakes(const cEntity *e) {
	return g_Mp2 && e && Obj(e)->netStep && NetPl_Kind((cEntity *)e) == NETREP_SHIP;
}

byte NetPl_IsParkBox(cEntity *e) {
	CGameModel *m = &Obj(e)->model;
	return Entity_IsType(e, "item") && CGameModel_IsLoaded(m) && m->Model->scene && m->Model->scene->name &&
		!crt_stricmp(m->Model->scene->name, NETPL_PARK_MODEL);
}

/* -mp2: the parking boxes in the step of a player's ship. Single player's box touches the ships in its own slots (20
 * a second, Parking.OnPark: a ship faster than MinParkingVel gets 0.6 of its velocity, a slower one parks); here every
 * third slot of 1/60 s of the ship's step on both sides: the server runs the box's OnTake, its client predicts what
 * the script does with the free boxes it has as replicas (their respawn time comes with them): the brake, or the start
 * of the parking's move (Parking_PredictPark) */
static void NetPl_ParkBoxes(cEntity *e) {
	cVehicleEntity *v = Veh(e);
	float r = v->wl.prim.radius + NETPL_PARK_REACH;
	vec3_t lo = { e->pos.x - r, e->pos.y - r, e->pos.z - r }, hi = { e->pos.x + r, e->pos.y + r, e->pos.z + r };
	cEntity *touch[NETPL_PARK_MAX];
	int n = 0;

	if (v->wl.obj.move.active || !(e->mask & 0x11) || !(v->wl.prim.on && v->wl.prim.radius > 0.0f)) {
		return;
	}
	/* the boxes first: the scripts may change the grid */
	for (cEntity *b = Grid_FirstInBox(&g_pWorld->grid, &lo, &hi); b && n < NETPL_PARK_MAX; b = Grid_NextInBox(&g_pWorld->grid)) {
		cItemEntity *it = (cItemEntity *)b;
		if (b->deadNode.target || !b->active || !NetPl_IsParkBox(b) || it->respawn > 0.0f) {
			continue;
		}
		float dx = e->pos.x - b->pos.x, dy = e->pos.y - b->pos.y, dz = e->pos.z - b->pos.z;
		float rr = v->wl.prim.radius + it->radius;
		if (rr * rr >= dx * dx + dy * dy + dz * dz) {
			touch[n++] = b;
		}
	}
	for (int k = 0; k < n && !v->wl.obj.move.active; k++) {
		if (g_Instance) {
			cItemEntity_TouchBy(touch[k], e);
		} else if ((double)v->speed > (double)Mode_ModGetI("Parking", "MinParkingVel", 1000)) {
			/* the script's Vel * 0.6 in double */
			vec3_t vel = { (float)((double)v->wl.obj.vel.x * 0.6), (float)((double)v->wl.obj.vel.y * 0.6),
				(float)((double)v->wl.obj.vel.z * 0.6) };
			Obj_SetVel(e, &vel);
		} else {
			Parking_PredictPark(touch[k], e);
		}
	}
}

void NetPl_StepPart(cEntity *e, const NetRepInput *in, byte local, byte replay, int part) {
	/* the entity's time speed: 1, slower in a bullet time of the freeroam factions (the server and the prediction) */
	float iv = PLAY_SLOT * cTimer_GetSpeed(&Obj(e)->timer);

	/* a stopped entity (paused by a scene) does not step, as the engine's update leaves it */
	if (OBJ_MIN_DT >= iv) {
		NetRepInput slot = *in;
		if (part) {
			memcpy(slot.move, in->move2, sizeof(slot.move));
		}
		slot.buttons = NetRep_InSlot(in->buttons, part);
		NetPl_Apply(e, &slot, local, replay);
		return;
	}
	/* single player's frame at 60 a second: the actions of the frame, then its slot (a -mp2 client's replay runs them
	 * too: the layers are part of its rollback state, their presentation events stay off) */
	netPlStepping = e;
	netPlStepSlot = in->tick * PLAY_STEPS + (uint32_t)part;
	netPlStepDraws = 0;
	if (NetPl_StepBrakes(e) && netPlStepSlot % NETPL_PARK_SLOTS == 0) {
		NetPl_ParkBoxes(e);
	}
	if (!replay || g_Mp2) {
		NetPl_Actions(e, iv);
	}
	NetPl_Slot(e, in, local, replay, part, iv);
	netPlStepping = NULL;
}

void NetPl_Step(cEntity *e, const NetRepInput *in, byte local, byte replay) {
	for (int i = 0; i < PLAY_STEPS; i++) {
		NetPl_StepPart(e, in, local, replay, i);
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
		/* the use button acts as in single player (on what the arrow is on); the switch of the sandbox is the mission
		 * info key (Play_Switch) */
		b = (c->act ? NETREP_IN_ACT : 0) | (c->act2 ? NETREP_IN_ACT2 : 0) | (c->act3 ? NETREP_IN_ACT3 : 0) |
			(c->lock ? NETREP_IN_LOCK : 0);
		/* the counters of the arrow's option presses, the newest of the tick's frames */
		a->buttons &= (uint16_t)~NETREP_IN_OPTS;
		b |= (uint16_t)((c->opts & 3) << NETREP_IN_OPTNEXT_SHIFT | (c->opts >> 4 & 3) << NETREP_IN_OPTPREV_SHIFT);
		break;
	}
	case NETREP_SHIP: {
		const cVehicleControls *c = &Veh(e)->controls;
		m[0] = c->acc;
		m[1] = c->str;
		m[2] = c->vStr;
		r[0] = c->hRot;
		r[1] = c->vRot;
		b = (c->fire ? NETREP_IN_ACT : 0) | (c->cm ? NETREP_IN_ACT2 : 0) | (c->boostKey ? NETREP_IN_BOOST : 0) |
			(c->fullThrust ? NETREP_IN_FULL : 0);
		a->weapon = (uint8_t)c->weapon;
		break;
	}
	default:
		return;
	}
	/* the moves of the last frame, as a single player's slot takes the controls of now (an average with the frames
	 * before the press started a walk a tick later); the turns summed over the frames */
	for (int k = 0; k < 3; k++) {
		a->move[k] = m[k];
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
		out->move[i] = NetRep_QMove(a->move[i]);
		out->move2[i] = out->move[i];
	}
	out->rot[0] = NetRep_QRot(a->rot[0] * k);
	out->rot[1] = NetRep_QRot(a->rot[1] * k);
	out->buttons = a->buttons;
	out->weapon = a->weapon;
	uint8_t weapon = a->weapon;
	memset(a, 0, sizeof(*a));
	a->weapon = weapon;
}

void NetPl_InputSlot(const NetPlAccum *a, NetRepInput *out) {
	if (a->time <= 0.0f) {
		return;
	}
	for (int i = 0; i < 3; i++) {
		out->move2[i] = NetRep_QMove(a->move[i]);
	}
	if (g_Mp2) {
		out->buttons |= NetRep_InLate((uint16_t)(a->buttons & ~out->buttons));
	}
}

static void NetPl_SaveFull(cEntity *e, NetPlMotion *m);
static void NetPl_RestoreFull(cEntity *e, const NetPlMotion *m);

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
		_Static_assert(sizeof(m->collision) == sizeof(v->collision), "cVehicleEntity.collision");
		memcpy(m->collision, v->collision, sizeof(m->collision));
	}
	if (g_Mp2) {
		NetPl_SaveFull(e, m);
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
		memcpy(v->collision, m->collision, sizeof(v->collision));
	}
	if (m->full) {
		NetPl_RestoreFull(e, m);
	}
}

/* ---- the rollback state of a -mp2 client (stage 3.5) ---- */

static void NetPl_SaveFull(cEntity *e, NetPlMotion *m) {
	cObjEntity *o = Obj(e);
	cObjMove *mv = &o->move;

	m->full = 1;
	m->moveStartPos = mv->startPos;
	m->moveStartAng = mv->startAng;
	m->moveLastPos = mv->lastPos;
	m->moveLastAng = mv->lastAng;
	m->movePos = mv->movePos;
	m->moveAng = mv->moveAng;
	m->moveTime = mv->time;
	m->moveLen = mv->moveTime;
	m->moveFade = mv->fade;
	m->moveOn = mv->active;
	m->moveView = mv->viewAng;
	m->acts = WLife(e)->actCtrl != NULL;
	if (m->acts) {
		cActCtrl_Save(WLife(e)->actCtrl, &m->act);
	}
	if (NetPl_Kind(e) == NETREP_CHAR) {
		cWalkCharEntity *w = WChar(e);
		m->energyUsed = w->energyUsed;
		m->relaxTime = w->relaxTime;
		m->hitFlash = w->hitFlash;
		m->viewLockF = w->viewLockF;
		m->viewLock = w->viewLock;
		m->floorDist = w->floorDist;
		m->centerTime = w->control.centerTime;
		m->prevAct = w->control.prevAct;
		m->prevAct2 = w->control.prevAct2;
		m->prevAct3 = w->control.prevAct3;
		m->lockStart = w->control.lockStart;
		m->lock = w->control.lock;
	} else if (NetPl_Kind(e) == NETREP_SHIP) {
		cVehicleEntity *v = Veh(e);
		m->cloud = v->cloud;
		m->cloudOn = v->cloudOn;
		m->emi = v->emi;
		m->noFire = v->noFire;
		m->histTime = v->histTime;
		m->histSpeed = v->histSpeed;
		m->histVel = v->histVel;
		m->histAcc = v->histAcc;
		m->histAccLen = v->histAccLen;
		m->hitTime = v->hitTime;
	}
}

static void NetPl_RestoreFull(cEntity *e, const NetPlMotion *m) {
	cObjEntity *o = Obj(e);
	cObjMove *mv = &o->move;

	mv->startPos = m->moveStartPos;
	mv->startAng = m->moveStartAng;
	mv->lastPos = m->moveLastPos;
	mv->lastAng = m->moveLastAng;
	mv->movePos = m->movePos;
	mv->moveAng = m->moveAng;
	mv->time = m->moveTime;
	mv->moveTime = m->moveLen;
	mv->fade = m->moveFade;
	mv->active = m->moveOn;
	mv->viewAng = m->moveView;
	if (m->acts && WLife(e)->actCtrl) {
		cActCtrl_Restore(WLife(e)->actCtrl, &m->act);
	}
	if (NetPl_Kind(e) == NETREP_CHAR) {
		cWalkCharEntity *w = WChar(e);
		w->energyUsed = m->energyUsed;
		w->relaxTime = m->relaxTime;
		w->hitFlash = m->hitFlash;
		w->viewLockF = m->viewLockF;
		w->viewLock = m->viewLock;
		w->floorDist = m->floorDist;
		w->control.centerTime = m->centerTime;
		w->control.prevAct = m->prevAct;
		w->control.prevAct2 = m->prevAct2;
		w->control.prevAct3 = m->prevAct3;
		w->control.lockStart = m->lockStart;
		w->control.lock = m->lock;
	} else if (NetPl_Kind(e) == NETREP_SHIP) {
		cVehicleEntity *v = Veh(e);
		v->cloud = m->cloud;
		v->cloudOn = m->cloudOn;
		v->emi = m->emi;
		v->noFire = m->noFire;
		v->histTime = m->histTime;
		v->histSpeed = m->histSpeed;
		v->histVel = m->histVel;
		v->histAcc = m->histAcc;
		v->histAccLen = m->histAccLen;
		v->hitTime = m->hitTime;
	}
}

static inline void NetPl_PutV(float *f, const vec3_t *v) {
	f[0] = v->x;
	f[1] = v->y;
	f[2] = v->z;
}

static inline void NetPl_GetV(vec3_t *v, const float *f) {
	*v = (vec3_t){ f[0], f[1], f[2] };
}

void NetPl_OwnOf(cEntity *e, uint32_t tick, uint8_t moved, NetRepOwn *o) {
	cObjEntity *ob = Obj(e);
	float *f = o->f;
	int n;

	memset(o, 0, sizeof(*o));
	o->tick = tick;
	o->kind = (uint8_t)NetPl_Kind(e);
	o->moved = moved;
	NetPl_PutV(f + NETPL_O_POS, &e->pos);
	NetPl_PutV(f + NETPL_O_VEL, &ob->vel);
	NetPl_PutV(f + NETPL_O_ANG, &ob->ang);
	NetPl_PutV(f + NETPL_O_ROTVEL, &ob->rotVel);
	f[NETPL_O_LIFE] = (float)WLife(e)->life;
	if (o->kind == NETREP_CHAR) {
		cWalkCharEntity *w = WChar(e);
		NetPl_PutV(f + NETPL_O_VIEW, &w->view);
		NetPl_PutV(f + NETPL_O_VIEWVEL, &w->viewVel);
		NetPl_PutV(f + NETPL_O_FALLVEL, &w->fallVel);
		f[NETPL_O_SLIDE] = w->slideTime;
		f[NETPL_O_DISPL] = w->displAng;
		f[NETPL_O_LASTMOVE] = w->lastMove;
		f[NETPL_O_LASTDISPL] = w->lastDisplAng;
		f[NETPL_O_WALKTIME] = w->walkTime;
		f[NETPL_O_MOVE] = w->move;
		f[NETPL_O_MOVED2] = w->moved2;
		f[NETPL_O_ENERGY] = w->energyUsed;
		f[NETPL_O_RELAX] = w->relaxTime;
		o->flags |= (w->falling ? NETPL_OF_FALLING : 0) | (w->walking ? NETPL_OF_WALKING : 0) |
			(w->disableControl || w->control.disabled ? NETPL_OF_NOCTRL : 0) | (g_EnergyBarActive ? NETPL_OF_ENERGYBAR : 0);
		cActCtrl *c = w->wl.actCtrl;
		if (c) {
			f[NETPL_O_WALKSPEED] = c->walkSpeed;
			o->layers = NETREP_OWN_LAYERS;
			for (int k = 0; k < ACT_LAYERS && k < NETREP_OWN_LAYERS; k++) {
				const cActLayer *l = &c->layers[k];
				o->act[k] = l->on && l->action ? Rep_ActIndex(c->cls, l->action) : 0;
				f[NETPL_O_ACTTIME + k] = o->act[k] ? l->time : 0.0f;
				f[NETPL_O_ACTFRAME + k] = o->act[k] ? cActCtrl_LayerFrame(c, k) : 0.0f;
			}
			o->defLoop = c->defLoop ? Rep_ActIndex(c->cls, c->defLoop) : 0;
		}
		n = NETPL_O_CHAR;
	} else if (o->kind == NETREP_SHIP) {
		cVehicleEntity *v = Veh(e);
		f[NETPL_O_SPEED] = v->speed;
		f[NETPL_O_THRUST] = v->thrust;
		f[NETPL_O_TURNX] = v->turnSpeedX;
		f[NETPL_O_TURNY] = v->turnSpeedY;
		f[NETPL_O_ROLL] = v->roll;
		f[NETPL_O_BOOST] = v->boost;
		f[NETPL_O_PUSHTIME] = v->pushTime;
		NetPl_PutV(f + NETPL_O_PUSH, &v->push);
		f[NETPL_O_CLOUD] = v->cloud;
		f[NETPL_O_EMI] = v->emi;
		o->flags |= v->cloudOn ? NETPL_OF_CLOUD : 0;
		n = NETPL_O_SHIP;
	} else {
		n = NETPL_O_COMMON;
	}
	const cObjMove *mv = &ob->move;
	if (mv->active) {
		o->flags |= NETPL_OF_MOVE;
		f[n + NETPL_O_MOVETIME] = mv->time;
		f[n + NETPL_O_MOVELEN] = mv->moveTime;
		NetPl_PutV(f + n + NETPL_O_MOVEPOS, &mv->movePos);
		NetPl_PutV(f + n + NETPL_O_MOVEANG, &mv->moveAng);
		NetPl_PutV(f + n + NETPL_O_MOVESTART, &mv->startPos);
		NetPl_PutV(f + n + NETPL_O_MOVESTARTANG, &mv->startAng);
		n += NETPL_O_MOVEN;
	}
	o->nf = (uint8_t)n;
}

/* the tolerances of the compare: the same code on both sides gives the same numbers, what differs comes from the
 * world (a hit, a push, another entity in the way, an input the server ran without) */
#define NETPL_TOL_POS PLAY_CORRECT_MIN
#define NETPL_TOL_VEL 20.0f			/* units a second */
#define NETPL_TOL_ANG 0.01f			/* radians */
#define NETPL_TOL_TIME 0.05f		/* s of the timed states and the actions */
#define NETPL_TOL_ENERGY 0.02f

static float NetPl_Dist(const float *a, const float *b) {
	float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

static float NetPl_AngDist(const float *a, const float *b) {
	float m = 0.0f;
	for (int k = 0; k < 3; k++) {
		float d = fabsf(remainderf(a[k] - b[k], M_2PI_F));
		m = d > m ? d : m;
	}
	return m;
}

static byte NetPl_Off(float a, float b, float tol) {
	return fabsf(a - b) > tol;
}

byte NetPl_OwnSame(const NetRepOwn *a, const NetRepOwn *b) {
	if (a->kind != b->kind || ((a->flags ^ b->flags) & ~NETPL_OF_TOUCHED) || a->nf != b->nf || a->layers != b->layers ||
		a->defLoop != b->defLoop) {
		return 0;
	}
	for (int k = 0; k < a->nf; k++) {
		if (a->f[k] != b->f[k]) {
			return 0;
		}
	}
	for (int k = 0; k < a->layers; k++) {
		if (a->act[k] != b->act[k]) {
			return 0;
		}
	}
	return 1;
}

int NetPl_OwnDiff(const NetRepOwn *a, const NetRepOwn *b) {
	const float *x = a->f, *y = b->f;
	int d = 0;

	if (a->kind != b->kind) {
		return NETPL_D_POS | NETPL_D_STATE;
	}
	if (NetPl_Dist(x + NETPL_O_POS, y + NETPL_O_POS) > NETPL_TOL_POS) {
		d |= NETPL_D_POS;
	}
	if (NetPl_Dist(x + NETPL_O_VEL, y + NETPL_O_VEL) > NETPL_TOL_VEL) {
		d |= NETPL_D_VEL;
	}
	if (NetPl_AngDist(x + NETPL_O_ANG, y + NETPL_O_ANG) > NETPL_TOL_ANG) {
		d |= NETPL_D_ANG;
	}
	if (x[NETPL_O_LIFE] != y[NETPL_O_LIFE]) {
		d |= NETPL_D_LIFE;
	}
	if ((a->flags ^ b->flags) & (NETPL_OF_FALLING | NETPL_OF_WALKING | NETPL_OF_CLOUD | NETPL_OF_NOCTRL |
		NETPL_OF_ENERGYBAR)) {
		d |= NETPL_D_STATE;
	}
	if ((a->flags ^ b->flags) & NETPL_OF_MOVE) {
		d |= NETPL_D_MOVE;
	}
	int n = NETPL_O_COMMON;
	if (a->kind == NETREP_CHAR) {
		if (NetPl_AngDist(x + NETPL_O_VIEW, y + NETPL_O_VIEW) > NETPL_TOL_ANG) {
			d |= NETPL_D_ANG;
		}
		if (NetPl_Dist(x + NETPL_O_FALLVEL, y + NETPL_O_FALLVEL) > NETPL_TOL_VEL ||
			NetPl_Off(x[NETPL_O_ENERGY], y[NETPL_O_ENERGY], NETPL_TOL_ENERGY)) {
			d |= NETPL_D_STATE;
		}
		for (int k = 0; k < a->layers && k < b->layers; k++) {
			if (a->act[k] != b->act[k] ||
				NetPl_Off(x[NETPL_O_ACTTIME + k], y[NETPL_O_ACTTIME + k], NETPL_TOL_TIME * 2.0f)) {
				d |= NETPL_D_ACT;
			}
		}
		n = NETPL_O_CHAR;
	} else if (a->kind == NETREP_SHIP) {
		if ((x[NETPL_O_BOOST] > 0.0f) != (y[NETPL_O_BOOST] > 0.0f) ||
			NetPl_Off(x[NETPL_O_BOOST], y[NETPL_O_BOOST], NETPL_TOL_TIME) ||
			(x[NETPL_O_EMI] > 0.0f) != (y[NETPL_O_EMI] > 0.0f) ||
			NetPl_Off(x[NETPL_O_CLOUD], y[NETPL_O_CLOUD], NETPL_TOL_TIME) ||
			NetPl_Off(x[NETPL_O_PUSHTIME], y[NETPL_O_PUSHTIME], NETPL_TOL_TIME)) {
			d |= NETPL_D_STATE;
		}
		n = NETPL_O_SHIP;
	}
	if ((a->flags & b->flags & NETPL_OF_MOVE) && a->nf >= n + NETPL_O_MOVEN && b->nf >= n + NETPL_O_MOVEN &&
		(NetPl_Off(x[n + NETPL_O_MOVETIME], y[n + NETPL_O_MOVETIME], NETPL_TOL_TIME) ||
		NetPl_Dist(x + n + NETPL_O_MOVEPOS, y + n + NETPL_O_MOVEPOS) > NETPL_TOL_POS)) {
		d |= NETPL_D_MOVE;
	}
	return d;
}

void NetPl_OwnApply(cEntity *e, const NetRepOwn *o) {
	cObjEntity *ob = Obj(e);
	const float *f = o->f;
	int n;

	if (o->kind != NetPl_Kind(e)) {
		return;
	}
	NetPl_GetV(&e->pos, f + NETPL_O_POS);
	NetPl_GetV(&ob->vel, f + NETPL_O_VEL);
	NetPl_GetV(&ob->ang, f + NETPL_O_ANG);
	NetPl_GetV(&ob->rotVel, f + NETPL_O_ROTVEL);
	WLife(e)->life = (int)f[NETPL_O_LIFE];
	if (o->kind == NETREP_CHAR) {
		cWalkCharEntity *w = WChar(e);
		NetPl_GetV(&w->view, f + NETPL_O_VIEW);
		NetPl_GetV(&w->viewVel, f + NETPL_O_VIEWVEL);
		NetPl_GetV(&w->fallVel, f + NETPL_O_FALLVEL);
		w->slideTime = f[NETPL_O_SLIDE];
		w->displAng = f[NETPL_O_DISPL];
		w->lastMove = f[NETPL_O_LASTMOVE];
		w->lastDisplAng = f[NETPL_O_LASTDISPL];
		w->walkTime = f[NETPL_O_WALKTIME];
		w->move = f[NETPL_O_MOVE];
		w->moved2 = f[NETPL_O_MOVED2];
		w->energyUsed = f[NETPL_O_ENERGY];
		w->relaxTime = f[NETPL_O_RELAX];
		w->falling = (o->flags & NETPL_OF_FALLING) != 0;
		w->walking = (o->flags & NETPL_OF_WALKING) != 0;
		/* the scripts' hold of the character: its own flag (the control's goes with the next input) */
		w->disableControl = (o->flags & NETPL_OF_NOCTRL) != 0;
		g_EnergyBarActive = (o->flags & NETPL_OF_ENERGYBAR) != 0;
		cActCtrl *c = w->wl.actCtrl;
		if (c && o->layers) {
			c->walkSpeed = f[NETPL_O_WALKSPEED];
			cActCtrl_NetSet(c, o->act, f + NETPL_O_ACTTIME, f + NETPL_O_ACTFRAME, o->layers, o->defLoop,
				NETPL_TOL_TIME * 2.0f);
		}
		n = NETPL_O_CHAR;
	} else if (o->kind == NETREP_SHIP) {
		cVehicleEntity *v = Veh(e);
		v->speed = f[NETPL_O_SPEED];
		v->thrust = f[NETPL_O_THRUST];
		v->turnSpeedX = f[NETPL_O_TURNX];
		v->turnSpeedY = f[NETPL_O_TURNY];
		v->roll = f[NETPL_O_ROLL];
		v->boost = f[NETPL_O_BOOST];
		v->pushTime = f[NETPL_O_PUSHTIME];
		NetPl_GetV(&v->push, f + NETPL_O_PUSH);
		v->cloud = f[NETPL_O_CLOUD];
		v->emi = f[NETPL_O_EMI];
		v->cloudOn = (o->flags & NETPL_OF_CLOUD) != 0;
		/* the server's step after this tick flies with the trusts of this life (its armour weighs) and cloud */
		cVehicleEntity_UpdateFlight(e);
		n = NETPL_O_SHIP;
	} else {
		n = NETPL_O_COMMON;
	}
	cObjMove *mv = &ob->move;
	if ((o->flags & NETPL_OF_MOVE) && o->nf >= n + NETPL_O_MOVEN) {
		mv->active = 1;
		mv->time = f[n + NETPL_O_MOVETIME];
		mv->moveTime = f[n + NETPL_O_MOVELEN];
		NetPl_GetV(&mv->movePos, f + n + NETPL_O_MOVEPOS);
		NetPl_GetV(&mv->moveAng, f + n + NETPL_O_MOVEANG);
		NetPl_GetV(&mv->startPos, f + n + NETPL_O_MOVESTART);
		NetPl_GetV(&mv->startAng, f + n + NETPL_O_MOVESTARTANG);
	} else {
		mv->active = 0;
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
