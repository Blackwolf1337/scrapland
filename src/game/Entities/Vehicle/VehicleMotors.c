/*
 * Entities/Vehicle/VehicleMotors.cpp of the game: the motors of a ship, see include/scrap/vehiclemotors.h.
 * The constructors and destructors of cVehicleMotor and cVehicleMotors are in VehicleEntity.cpp.
 */
#include <string.h>

#include "scrap/arrowindicator.h"
#include "scrap/embeddedfxvehicle.h"
#include "scrap/fxmisc.h"
#include "scrap/vehicleentity.h"
#include "scrap/vehiclemotors.h"
#include "scrap/weaps2.h"


/* "DC_EngineA_01": the slot of motor (A..D) and side (1, 2) */
static inline void VM_SlotName(char *buf, int motor, int side) {
	memcpy(buf, "DC_EngineA_01", 14);
	buf[9] += (char)motor;
	buf[12] += (char)side;
}

/* the number of nodes of a motor: they come first, the first -1 ends them */
static inline int VM_NumNodes(const cVehicleMotor *m) {
	int n;

	for (n = 0; n < VM_SIDES; n++) {
		if (!m || m->node[n] == -1) {
			break;
		}
	}
	return n;
}

static inline CGameModel *VM_OwnerModel(cVehicleMotors *self) {
	return Obj_Vtbl(self->owner)->GetModel(self->owner);
}

/* the position and the angles of render node i of the ship model */
static byte VM_NodeInfo(cVehicleMotors *self, int i, vec3_t *pos, vec3_t *ang) {
	matrix_t m;
	vec3_t e;

	CGameModel_NodeMatrix(VM_OwnerModel(self), &m, i, 0, NULL, 0);
	if (pos) {
		pos->x = m.m[12];
		pos->y = m.m[13];
		pos->z = m.m[14];
	}
	if (ang) {
		*ang = *Matrix_GetEuler(&m, &e);
	}
	return 1;
}

static inline int VM_Digit(char c) {
	return c >= '0' && c <= '3';
}


/* the values of cVehicleMotors_Update the effects of every motor and of the ship use */
typedef struct VMUpdate {
	float dt;
	float speed;			/* of the ship */
	float thrust;			/* the thrust clamped to -0.5..1 */
	float k4c8;			/* at most 1 */
	float k4bc;
	float fade;			/* of the exhaust particles with the camera distance */
	float alpha;			/* of the embedded effects */
	float camDist;
	float fxTime;			/* since the visibility of the ship changed */
	const cEntity *player;		/* the follow camera of a player ship */
	byte cloud;
	byte visible;
	byte followed;
	byte traffic;
	int first;			/* the next glow particle */
	CGameParticles *glowPs;
	CGameParticles *smokePs;
} VMUpdate;

static void VM_UpdateEffects(cVehicleMotors *self, cVehicleMotorsEffects *eff, VMUpdate *u) {
	vec3_t a, c;

	if (!eff->on || Veh(self->owner)->rendering) {
		return;
	}
	if (u->visible || self->lastVisible) {
		if (!self->stopped) {
			if (u->speed == 0.0f || (!u->player &&
				!(g_FXEngineGlowDistFadeTo * g_FXEngineGlowDistFadeMult >= u->camDist) && !u->followed)) {
				cVehicleMotorsEffects_SetVisible(eff, 0);
			} else {
				cVehicleMotorsEffects_SetVisible(eff, 1);
				cVehicleMotorsEffects_Exhaust(eff, u->thrust, u->k4c8, u->k4bc, self->speed, u->cloud, u->fade);
			}
			if (u->speed == 0.0f || !u->visible || (u->player && !g_SplitScreenGame)) {
				if (eff->glowOn) {
					cVehicleMotorsEffects_StopGlow(eff, &u->first, u->glowPs);
				}
			} else {
				cVehicleMotorsEffects_Glow(eff, self->speed * u->alpha, u->thrust, u->camDist, &u->first, u->glowPs);
			}
		}
		if (u->speed > 0.0f && u->cloud && u->smokePs) {
			cVehicleMotorsEffects_Smoke(eff, u->dt, u->smokePs);
		}
	}
	if ((u->player && !g_SplitScreenGame) || u->traffic || (!u->visible && !(g_FXEngineTrailHideTime > u->fxTime))) {
		cVehicleMotorsEffects_HideTrails(eff);
		return;
	}
	vec3_t vel = Veh(self->owner)->localVel;
	vec3_t *pa = Obj_Vtbl(self->owner)->GetViewAng(self->owner, &a);
	vec3_t *pc = Obj_Vtbl(self->owner)->GetViewPos(self->owner, &c);
	uint32_t color = cArrowIndicator_GetColor((cArrowIndicator *)VM_Team(self->owner));
	cVehicleMotorsEffects_Trails(eff, u->dt, *pc, *pa, vel, u->thrust * u->alpha, u->k4c8, color);
}

/* the Calima particles (the hot air under the exhausts) of the motors of a player ship */
static void VM_Haze(cVehicleMotors *self, VMUpdate *u) {
	vec3_t t;
	int i, k, n;

	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXMotorHazeFile);
	cDebris *d = cDebrisSys_GetIdle(sys);
	d->life = 1.0f;
	if (!cDebrisSys_IsIdle(sys, d)) {
		cDebrisSys_Drop(sys, d);
	}
	cObjElementFXParticleSystem *el = Debris_Element(d);
	if (!el) {
		return;
	}
	if (!el->on) {
		el->move = 0;
	}
	CGameModel *vm = VM_OwnerModel(self);
	CGameModel_SetSortZ(Debris_Model(d), -0.5f, 0.0f);
	CGameModel_SetPos(Debris_Model(d), CGameModel_GetPos(vm, &t));
	CGameModel_SetRot(Debris_Model(d), CGameModel_GetRot(vm, &t));
	float off = g_FXMotorHazeOffset;
	n = 0;
	for (i = 0; i < VM_MOTORS; i++) {
		cVehicleMotor *m = self->motor[i];
		if (!m || !m->effects.on) {
			continue;
		}
		for (k = 0; k < m->effects.numExhausts; k++) {
			g_HazePos[n].x = m->effects.exhaustPos[k].x + 0.0f;
			g_HazePos[n].y = m->effects.exhaustPos[k].y + 0.0f;
			g_HazePos[n].z = m->effects.exhaustPos[k].z + off;
			n++;
		}
	}
	if (n > 0) {
		VMEffects_SetupHaze(u->thrust, u->k4c8, g_HazePos, n, &el->particles);
	}
}

void cVehicleMotors_Update(cVehicleMotors *self) {
	VMUpdate u;
	cEntity *owner = self->owner;
	int i, j;

	u.dt = cTimer_GetDelta(&Obj(owner)->timer);
	float a = Veh(owner)->smoothThrust;
	float s = Veh(owner)->smoothSpeed + Veh(owner)->smoothSpeed;
	u.speed = Veh(owner)->active;
	float f = a > 0.0f ? a * 2.0f + 1.0f : 1.0f - a;
	float animStep = (s / 28000.0f + f) * (u.speed * u.dt) * 0.25f;
	float animF = Veh(owner)->smoothThrust;
	if (0.0f > animF) {
		animF = animF + animF;
	}
	u.player = Vehicle_Player(owner);
	u.cloud = Veh(owner)->cloud > 0.0f;
	CHECK(Veh_FX(Veh(owner)));
	cEmbeddedFXVehicle *fx = Veh_FX(Veh(self->owner));
	u.glowPs = &fx->follow;
	CHECK(fx->follow.ParticleSystem);
	u.alpha = fx->alpha;
	u.visible = fx->visible;
	u.camDist = fx->camDist;
	u.followed = fx->followed;
	u.traffic = fx->traffic;
	u.fxTime = fx->time;
	if (self->lastVisible != u.visible) {
		self->stopped = 0;
	}
	u.fade = 0.0f;
	if (!self->stopped) {
		if (!u.player && !g_SplitScreenGame && !u.followed) {
			float m = g_FXEngineGlowDistFadeMult * g_FXEngineGlowDistFadeMid;
			if (m > u.camDist) {
				u.fade = 1.0f;
			} else if (g_FXEngineGlowDistFadeTo * g_FXEngineGlowDistFadeMult > u.camDist) {
				u.fade = 1.0f - (u.camDist - m) /
					((g_FXEngineGlowDistFadeTo - g_FXEngineGlowDistFadeMid) * g_FXEngineGlowDistFadeMult);
			} else {
				u.fade = 0.0f;
			}
		} else {
			u.fade = 1.0f;
		}
		if (self->speed != u.speed) {
			Float_Approach(&self->speed, u.speed, u.dt);
		}
	}
	owner = self->owner;
	float th = Veh(owner)->smoothThrust;
	u.thrust = 0.0f > th ? th * 0.5f : Float_MinSS(1.0f, th);
	u.k4c8 = Veh(owner)->shake;
	if (u.k4c8 > 1.0f) {
		u.k4c8 = 1.0f;
	}
	u.k4bc = Veh(owner)->inAtmosphere;
	u.first = 0;
	u.smokePs = NULL;
	if (u.speed > 0.0f && u.cloud) {
		cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXMotorSmokeFile);
		cDebris *d = cDebrisSys_GetIdle(sys);
		vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
		CGameModel_SetPos(Debris_Model(d), &cam);
		CGameModel_SetSortZ(Debris_Model(d), -0.1f, 0.0f);
		d->life = 0.5f;
		if (!cDebrisSys_IsIdle(sys, d)) {
			cDebrisSys_Drop(sys, d);
		}
		cObjElementFXParticleSystem *el = Debris_Element(d);
		u.smokePs = &el->particles;
		if (!el->on) {
			el->move = 0;
			el->on = 1;
		}
	}
	for (i = 0; i < VM_MOTORS; i++) {
		cVehicleMotor *m = self->motor[i];
		if (!m) {
			continue;
		}
		byte any = 0;
		for (j = 0; j < VM_SIDES && self->motor[i] && m->node[j] != -1; j++) {
			matrix_t mat;
			vec3_t pos, e, rot;
			CGameModel_NodeMatrix(VM_OwnerModel(self), &mat, m->node[j], 0, NULL, 0);
			pos.x = mat.m[12];
			pos.y = mat.m[13];
			pos.z = mat.m[14];
			rot = *Matrix_GetEuler(&mat, &e);
			CGameModel_SetRot(&m->model[j], &rot);
			CGameModel_SetPos(&m->model[j], &pos);
			any = 1;
		}
		if (any) {
			cVehicleMotor_SetAnimF(m, animF);
			cVehicleMotor_UpdateAnimM(m, animStep);
		}
		VM_UpdateEffects(self, &m->effects, &u);
	}
	VM_UpdateEffects(self, &self->effects, &u);
	if (g_SceneDuDvRTarget && g_FXMotorHaze && u.player && !self->stopped && !u.traffic && !g_SplitScreenGame) {
		VM_Haze(self, &u);
	}
	owner = self->owner;
	if (Veh(owner)->smoothThrust == 0.0f && Veh(owner)->boost / Veh(owner)->boostTime == 0.0f &&
		Veh(owner)->active == 0.0f && !u.cloud && self->lastVisible) {
		self->stopped = 1;
	} else {
		self->stopped = 0;
	}
	self->lastVisible = u.visible;
}

void cVehicleMotor_Reset(cVehicleMotor *self) {
	int i;

	self->name[0] = 0;
	self->animMOn = 0;
	self->animFOn = 0;
	for (i = 0; i < VM_SIDES; i++) {
		if (CGameModel_IsLoaded(&self->model[i])) {
			CGameModel_Disable(&self->model[i]);
		}
		self->node[i] = -1;
	}
	cVehicleMotorsEffects_Clear(&self->effects);
}

void cVehicleMotors_Reset(cVehicleMotors *self) {
	int i;

	self->stopped = 0;
	self->lastVisible = 0;
	self->speed = 0.0f;
	for (i = 0; i < VM_MOTORS; i++) {
		if (self->motor[i]) {
			cVehicleMotor_Reset(self->motor[i]);
		}
	}
}

void cVehicleMotors_ClearEffects(cVehicleMotors *self) {
	cVehicleMotorsEffects_Clear(&self->effects);
}

void cVehicleMotors_InitEffects(cVehicleMotors *self) {
	cVehicleMotorsEffects_Set(&self->effects, self->owner, self, NULL);
	cVehicleMotorsEffects_Init(&self->effects);
}

/* ShipMotorScale and ShipMotorOffset, compared without the terminator like the original */
byte cVehicleMotors_VarF(struct cVehicleMotors *base, const char *name, double *v, int set) {
	cVehicleMotors *self = (cVehicleMotors *)base;
	if (!memcmp(name, "ShipMotorScale", 14)) {
		if (set) {
			cVehicleMotorsEffects_SetScale(&self->effects, (float)*v);
		} else {
			*v = (double)self->effects.scale;
		}
		return 1;
	}
	if (!memcmp(name, "ShipMotorOffset", 15)) {
		if (set) {
			cVehicleMotorsEffects_SetOffset(&self->effects, (float)*v);
		} else {
			*v = (double)self->effects.offset;
		}
		return 1;
	}
	return 0;
}

byte cVehicleMotors_GetExhaust(cVehicleMotors *self, uint32_t motor, uint32_t side, vec3_t *pos,
	vec3_t *ang) {
	if (motor > 3 || side > 1 || !self->motor[motor] || self->motor[motor]->node[side] == -1) {
		return 0;
	}
	return VM_NodeInfo(self, self->motor[motor]->node[side], pos, ang);
}

byte cVehicleMotors_GetSlot(cVehicleMotors *self, uint32_t motor, uint32_t side, vec3_t *pos,
	vec3_t *ang) {
	char name[16];

	if (motor > 3 || side > 1) {
		return 0;
	}
	VM_SlotName(name, (int)motor, (int)side);
	int i = CGameModel_FindRenderNode(VM_OwnerModel(self), name);
	if (i == -1) {
		return 0;
	}
	return VM_NodeInfo(self, i, pos, ang);
}

const char *cVehicleMotors_GetMotorName(cVehicleMotors *self, uint32_t motor, int *count) {
	if (motor > 3) {
		if (count) {
			*count = 0;
		}
		return str_Empty;
	}
	int n = VM_NumNodes(self->motor[motor]);
	if (count) {
		*count = n;
	}
	return n > 0 ? self->motor[motor]->name : str_Empty;
}

int cVehicleMotors_NumSlots(cVehicleMotors *self, byte motor) {
	char name[16];
	int i;

	for (i = 0; i < VM_SIDES; i++) {
		VM_SlotName(name, motor, i);
		if (CGameModel_FindRenderNode(VM_OwnerModel(self), name) == -1) {
			return i;
		}
	}
	return VM_SIDES;
}

cEntity *cVehicleMotors_GetOwner(cVehicleMotors *self) {
	return self->owner;
}

/* get: NEng0..3 (the node count), nEng0..3 (the slot count); set: EngFxClass0..3, FxClass, ShipMotorColorR/G/B */
byte cVehicleMotors_VarI(struct cVehicleMotors *base, const char *name, int *v, int set) {
	cVehicleMotors *self = (cVehicleMotors *)base;
	size_t len = strlen(name);
	uint32_t c;

	if (!set) {
		if (len == 5 && VM_Digit(name[4])) {
			if (!memcmp(name, "NEng", 4)) {
				cVehicleMotors_GetMotorName(self, name[4] - '0', v);
				return 1;
			}
			if (!memcmp(name, "nEng", 4)) {
				*v = cVehicleMotors_NumSlots(self, name[4] - '0');
				return 1;
			}
		}
		return 0;
	}
	if (len == 11) {
		if (VM_Digit(name[10]) && !memcmp(name, "EngFxClass", 10)) {
			cVehicleMotor *m = self->motor[name[10] - '0'];
			if (m) {
				cVehicleMotorsEffects_SetType(&m->effects, *v);
				return 1;
			}
		}
		return 0;
	}
	if (len == 7) {
		if (!memcmp(name, "FxClass", 7)) {
			cVehicleMotorsEffects_SetType(&self->effects, *v);
			return 1;
		}
		return 0;
	}
	if (len != 15) {
		return 0;
	}
	if (!memcmp(name, "ShipMotorColorR", 15)) {
		c = ((uint32_t)*v << 16 ^ self->effects.color) & 0xff0000;
	} else if (!memcmp(name, "ShipMotorColorG", 15)) {
		c = ((uint32_t)*v << 8 ^ self->effects.color) & 0xff00;
	} else if (!memcmp(name, "ShipMotorColorB", 15)) {
		c = (self->effects.color ^ (uint32_t)*v) & 0xff;
	} else {
		return 0;
	}
	cVehicleMotorsEffects_SetColor(&self->effects, c ^ self->effects.color);
	return 1;
}

void cVehicleMotor_Render(cVehicleMotor *self) {
	int i;

	for (i = 0; i < VM_SIDES; i++) {
		if (self->node[i] != -1) {
			CGameModel_RenderNow(&self->model[i], 1);
		}
	}
}

void cVehicleMotors_Render(cVehicleMotors *self) {
	int i;

	for (i = 0; i < VM_MOTORS; i++) {
		if (self->motor[i]) {
			cVehicleMotor_Render(self->motor[i]);
		}
	}
}

void cVehicleMotor_SetEnabled(cVehicleMotor *self, byte on) {
	int i;

	for (i = 0; i < VM_SIDES; i++) {
		if (self->node[i] != -1) {
			if (on) {
				CGameModel_Enable(&self->model[i]);
			} else {
				CGameModel_Disable(&self->model[i]);
			}
		}
	}
	if (self->effects.on && !on) {
		cVehicleMotorsEffects_Stop(&self->effects);
	}
}

void cVehicleMotors_SetEnabled(cVehicleMotors *self, byte on) {
	int i;

	if (!on) {
		self->speed = 0.0f;
	}
	for (i = 0; i < VM_MOTORS; i++) {
		if (self->motor[i]) {
			cVehicleMotor_SetEnabled(self->motor[i], on);
		}
	}
	if (self->effects.on && !on) {
		cVehicleMotorsEffects_Stop(&self->effects);
	}
}

void cVehicleMotors_StartAnimF(cVehicleMotors *self, uint32_t motor, const char *anim) {
	if (motor < VM_MOTORS && self->motor[motor]) {
		cVehicleMotor_StartAnimF(self->motor[motor], anim);
	}
}

void cVehicleMotors_StartAnimM(cVehicleMotors *self, uint32_t motor, const char *anim) {
	if (motor < VM_MOTORS && self->motor[motor]) {
		cVehicleMotor_StartAnimM(self->motor[motor], anim);
	}
}

int cVehicleMotor_Load(cVehicleMotor *self, cEntity *owner, byte motor, const char *file,
	const vec3_t *scale) {
	char path[0x200], name[16];
	vec3_t v;
	int i;

	cVehicleMotor_Reset(self);
	cVehicleMotorsEffects_Clear(&self->effects);
	if (!*file) {
		return 0;
	}
	cVehicleMotorsEffects_Set(&self->effects, owner, NULL, self);
	Str_Printf(path, sizeof(path), g_EnginePathFmt, file, file);
	for (i = 0; i < VM_SIDES; i++) {
		VM_SlotName(name, motor, i);
		self->node[i] = CGameModel_FindRenderNode(Obj_Vtbl(owner)->GetModel(owner), name);
		if (self->node[i] == -1) {
			break;
		}
		CGameModel_LoadScaled(&self->model[i], path, scale);
		CGameModel_SetPivotNode(&self->model[i], "DC_Ship_Link");
		CGameModel_SetPos(&self->model[i], owner->vftable->GetPos(owner, &v));
		CGameModel_SetRot(&self->model[i], Obj_Vtbl(owner)->GetAng(owner, &v));
		if (!owner->active) {
			CGameModel_Disable(&self->model[i]);
		}
	}
	if (i) {
		Str_Copy(self->name, file, sizeof(self->name));
	}
	cVehicleMotorsEffects_Init(&self->effects);
	return i;
}

int cVehicleMotors_SetMotor(cVehicleMotors *self, uint32_t motor, const char *file, float sx, float sy,
	float sz) {
	if (motor > 3) {
		return 0;
	}
	if (!self->motor[motor]) {
		cVehicleMotor *m = operator_new(sizeof(cVehicleMotor));
		self->motor[motor] = m ? cVehicleMotor_ctor(m) : NULL;
	}
	vec3_t scale = { sx, sy, sz };
	return cVehicleMotor_Load(self->motor[motor], self->owner, (byte)motor, file, &scale);
}

/* the animation anim of the motor file on channel chan of both models, its length into *dur */
static void VM_StartAnim(cVehicleMotor *self, const char *anim, uint32_t chan, float *dur) {
	char path[0x200];
	float fps;
	int i;

	Str_Printf(path, sizeof(path), g_EnginePathFmt, self->name, anim);
	for (i = 0; i < VM_SIDES; i++) {
		if (self->node[i] != -1) {
			CGameModel_AddAnim(&self->model[i], path);
			int a = CGameModel_FindAnim(&self->model[i], anim);
			CGameModel_SetAnim(&self->model[i], a, chan, 0);
			CGameModel_AnimInfo(&self->model[i], a, dur, &fps);
		}
	}
}

void cVehicleMotor_StartAnimF(cVehicleMotor *self, const char *anim) {
	if (!self->animFOn) {
		self->animFOn = 1;
		VM_StartAnim(self, anim, 0, &self->durF);
	}
}

void cVehicleMotor_SetAnimF(cVehicleMotor *self, float k) {
	int i;

	if (!self->animFOn) {
		return;
	}
	float t = (k + 1.0f) * self->durF * 0.5f;
	for (i = 0; i < VM_SIDES; i++) {
		if (self->node[i] != -1) {
			CGameModel_AnimDelta(&self->model[i], 0, t, NULL);
		}
	}
}

void cVehicleMotor_StartAnimM(cVehicleMotor *self, const char *anim) {
	if (!self->animMOn) {
		self->animMOn = 1;
		VM_StartAnim(self, anim, 1, &self->durM);
	}
}

void cVehicleMotor_UpdateAnimM(cVehicleMotor *self, float dt) {
	int i;

	if (!self->animMOn) {
		return;
	}
	self->timeM = (float)fmod((double)(dt + self->timeM), (double)self->durM);
	for (i = 0; i < VM_SIDES; i++) {
		if (self->node[i] != -1) {
			CGameModel_AnimDelta(&self->model[i], 1, self->timeM, NULL);
		}
	}
}

void cVehicleMotors_SetOwner(cVehicleMotors *self, cEntity *owner) {
	CHECK(self->owner == NULL);
	self->owner = owner;
}

void cVehicleMotor_SetEffect(cVehicleMotor *self, uint32_t fx) {
	int i;

	for (i = 0; i < VM_SIDES; i++) {
		if (CGameModel_IsLoaded(&self->model[i])) {
			CGameModel_SetEffect(&self->model[i], fx);
		}
	}
}

void cVehicleMotors_SetEffect(cVehicleMotors *self, uint32_t fx) {
	int i;

	for (i = 0; i < VM_MOTORS; i++) {
		if (self->motor[i]) {
			cVehicleMotor_SetEffect(self->motor[i], fx);
		}
	}
}

void cVehicleMotor_SetEffectParam(cVehicleMotor *self, int i, float v) {
	uint32_t bits;
	int j;

	memcpy(&bits, &v, 4);
	for (j = 0; j < VM_SIDES; j++) {
		if (CGameModel_IsLoaded(&self->model[j])) {
			CGameModel_SetEffectParam(&self->model[j], i, bits);
		}
	}
}

void cVehicleMotors_SetEffectParam(cVehicleMotors *self, int i, float v) {
	int j;

	for (j = 0; j < VM_MOTORS; j++) {
		if (self->motor[j]) {
			cVehicleMotor_SetEffectParam(self->motor[j], i, v);
		}
	}
}

/* MEng0..3: the file of the motor (set: loaded at scale 1); set only: EngAnmM0..3, EngAnmF0..3 */
byte cVehicleMotors_VarS(struct cVehicleMotors *base, const char *name, char **v, int set) {
	cVehicleMotors *self = (cVehicleMotors *)base;
	size_t len = strlen(name);

	if (len == 5 && VM_Digit(name[4]) && !memcmp(name, "MEng", 4)) {
		if (set) {
			cVehicleMotors_SetMotor(self, name[4] - '0', *v, 1.0f, 1.0f, 1.0f);
		} else {
			*v = (char *)cVehicleMotors_GetMotorName(self, name[4] - '0', NULL);
		}
		return 1;
	}
	if (set && len == 8 && VM_Digit(name[7]) && !memcmp(name, "EngA", 4)) {
		if (!memcmp(name + 4, "nmM", 3)) {
			cVehicleMotors_StartAnimM(self, name[7] - '0', *v);
			return 1;
		}
		if (!memcmp(name + 4, "nmF", 3)) {
			cVehicleMotors_StartAnimF(self, name[7] - '0', *v);
			return 1;
		}
	}
	return 0;
}

static inline int VM_Side(char c) {
	return c >= 'A' && c <= 'B';
}

/* get only: PEng0A (position), AEng0A (angles in degrees) of the exhausts, PEngSlot0A of the slots */
byte cVehicleMotors_VarV(struct cVehicleMotors *base, const char *name, double *x, double *y, double *z, int set) {
	cVehicleMotors *self = (cVehicleMotors *)base;
	size_t len = strlen(name);
	vec3_t pos, ang;
	byte r;

	if (set) {
		return 0;
	}
	if (len == 6) {
		if (!VM_Digit(name[4]) || !VM_Side(name[5])) {
			return 0;
		}
		if (!memcmp(name, "PEng", 4)) {
			r = cVehicleMotors_GetExhaust(self, name[4] - '0', name[5] - 'A', &pos, &ang);
		} else if (!memcmp(name, "AEng", 4)) {
			r = cVehicleMotors_GetExhaust(self, name[4] - '0', name[5] - 'A', &ang, &pos);
			if (r) {
				*x = (double)pos.x * 180.0 / 3.141592653589793;
				*y = (double)pos.y * 180.0 / 3.141592653589793;
				*z = (double)pos.z * 180.0 / 3.141592653589793;
			}
			return r;
		} else {
			return 0;
		}
	} else if (len == 10 && VM_Digit(name[8]) && VM_Side(name[9]) && !memcmp(name, "PEngSlot", 8)) {
		r = cVehicleMotors_GetSlot(self, name[8] - '0', name[9] - 'A', &pos, &ang);
	} else {
		return 0;
	}
	if (r) {
		*x = (double)pos.x;
		*y = (double)pos.y;
		*z = (double)pos.z;
	}
	return r;
}

cVehicleMotorsEffects *cVehicleMotorsEffects_ctor(cVehicleMotorsEffects *self) {
	self->owner = NULL;
	self->motors = NULL;
	self->motor = NULL;
	self->on = 0;
	self->node[0][0] = -1;
	self->node[0][1] = -1;
	self->particleSystem[0][0] = NULL;
	self->particleSystem[0][1] = NULL;
	self->trail[0] = NULL;
	self->node[1][0] = -1;
	self->node[1][1] = -1;
	self->particleSystem[1][0] = NULL;
	self->particleSystem[1][1] = NULL;
	self->trail[1] = NULL;
	self->smokeTime = 0.0f;
	self->trailsOn = 0;
	self->visible = 0;
	self->glowOn = 0;
	self->type = 0;
	self->scale = 1.0f;
	self->offset = 20.0f;
	return self;
}

void cVehicleMotorsEffects_dtor(cVehicleMotorsEffects *self) {
	cVehicleMotorsEffects_Clear(self);
}

vec3_t *Vec3_Assign(vec3_t *self, const vec3_t *v) {
	self->x = v->x;
	self->y = v->y;
	self->z = v->z;
	return self;
}

/* the particles of a node killed, released and freed */
static void VM_FreeParticles(CGameParticles **slot) {
	if (*slot && (*slot)->ParticleSystem) {
		CGameParticles_KillAll(*slot);
		CGameParticles_Release(*slot);
	}
	if (*slot) {
		CGameParticles_Release(*slot);
		operator_delete_sized(*slot, sizeof(CGameParticles));
		*slot = NULL;
	}
}

void cVehicleMotorsEffects_Clear(cVehicleMotorsEffects *self) {
	int i;

	self->owner = NULL;
	self->motors = NULL;
	self->motor = NULL;
	self->type = 0;
	self->on = 0;
	self->numExhausts = 0;
	for (i = 0; i < VM_SIDES; i++) {
		self->node[i][0] = -1;
		self->node[i][1] = -1;
		VM_FreeParticles(&self->particleSystem[i][0]);
		VM_FreeParticles(&self->particleSystem[i][1]);
		if (self->trail[i]) {
			CGameTrail_Clear(self->trail[i]);
			if (self->trail[i]) {
				CGameTrail_dtor(self->trail[i]);
				operator_delete_sized(self->trail[i], sizeof(CGameTrail));
				self->trail[i] = NULL;
			}
		}
	}
}

static void VM_StopParticles(CGameParticles *ps) {
	if (ps && ps->ParticleSystem) {
		ps->birthRate = 0;
		CGameParticles_Apply(ps);
		CGameParticles_KillAll(ps);
	}
}

void cVehicleMotorsEffects_Stop(cVehicleMotorsEffects *self) {
	int i;

	for (i = 0; i < VM_SIDES; i++) {
		VM_StopParticles(self->particleSystem[i][0]);
		VM_StopParticles(self->particleSystem[i][1]);
		if (!self->trail[i]) {
			continue;
		}
		if (CGameTrail_IsRender(self->trail[i]) && self->owner && Veh(self->owner)->wl.life <= 0) {
			cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXVehicleDeathTrailFile);
			cDebris *d = cDebrisSys_NewElement(sys);
			d->life = 1.0f;
			cDebrisSys_Drop(sys, d);
			cObjElementFXDevastatorShoot *fx = Debris_Element(d);
			if (fx) {
				if (!fx->on) {
					fx->on = 1;
				}
				fx->base.vftable->Reset(&fx->base, d);
				CGameTrail_Copy(&fx->trail, self->trail[i]);
				int t = CGameTrail_GetTransp(self->trail[i]);
				fx->duration = 0.75f;
				fx->time = (float)t / 255.0f * 0.75f;
			}
		}
		if (self->trail[i]) {
			CGameTrail_Clear(self->trail[i]);
		}
	}
	self->visible = 0;
	self->glowOn = 0;
	self->trailsOn = 0;
}

void cVehicleMotorsEffects_HideTrails(cVehicleMotorsEffects *self) {
	int i;

	if (!self->trailsOn) {
		return;
	}
	for (i = 0; i < VM_SIDES; i++) {
		if (self->trail[i] && CGameTrail_IsRender(self->trail[i])) {
			CGameTrail_Clear(self->trail[i]);
		}
	}
	self->trailsOn = 0;
}

CGameModel *cVehicleMotorsEffects_GetModel(cVehicleMotorsEffects *self, int side) {
	if (self->motors) {
		cEntity *e = cVehicleMotors_GetOwner(self->motors);
		return Obj_Vtbl(e)->GetModel(e);
	}
	if (self->motor && self->motor->node[side] != -1) {
		return &self->motor->model[side];
	}
	return NULL;
}

/* ---- the constructors and destructors (rva 0x10bb90..0x10bdc0, after VehicleEntity.cpp) ---- */

#define CGAMEMODEL_DTOR 0x20ec10		/* CGameModel_Reset */

cVehicleMotor *cVehicleMotor_ctor(cVehicleMotor *self) {
	crt_eh_vector_ctor(self->model, sizeof(CGameModel), VM_SIDES, (void *)CGameModel_ctorDefault,
		(void *)CGameModel_Reset);
	cVehicleMotorsEffects_ctor(&self->effects);
	self->name[0] = 0;
	self->animMOn = 0;
	self->animFOn = 0;
	self->durF = 0.0f;
	self->durM = 0.0f;
	self->timeM = 0.0f;
	self->node[0] = -1;
	self->node[1] = -1;
	return self;
}

void *cVehicleMotors_ctor(void *motors) {
	cVehicleMotors *self = motors;

	self->vftable = CVEHICLEMOTORS_VFTABLE;
	cVehicleMotorsEffects_ctor(&self->effects);
	self->owner = NULL;
	self->stopped = 0;
	self->lastVisible = 0;
	self->speed = 0.0f;
	cVehicleMotorsEffects_SetType(&self->effects, 1);
	for (int i = 0; i < VM_MOTORS; i++) {
		self->motor[i] = NULL;
	}
	return self;
}

void cVehicleMotor_dtor(cVehicleMotor *self) {
	cVehicleMotorsEffects_Stop(&self->effects);
	cVehicleMotorsEffects_dtor(&self->effects);
	crt_eh_vector_dtor(self->model, sizeof(CGameModel), VM_SIDES, (void *)CGameModel_Reset);
}

void cVehicleMotors_dtor(void *motors) {
	cVehicleMotors *self = motors;

	self->vftable = CVEHICLEMOTORS_VFTABLE;
	cVehicleMotorsEffects_Clear(&self->effects);
	/* the pointers are kept */
	for (int i = 0; i < VM_MOTORS; i++) {
		cVehicleMotor *m = self->motor[i];
		if (m) {
			cVehicleMotor_dtor(m);
			operator_delete_sized(m, sizeof(cVehicleMotor));
		}
	}
	cVehicleMotorsEffects_dtor(&self->effects);
}

/* the vtable of cVehicleMotors */
const cVehicleMotorsVtbl cVehicleMotors_vftable = {
	.VarI = cVehicleMotors_VarI,
	.VarF = cVehicleMotors_VarF,
	.VarS = cVehicleMotors_VarS,
	.VarV = cVehicleMotors_VarV,
};
