/*
 * World/FX/FXChar.cpp of the game: the effects of the characters, see include/scrap/fxchar.h.
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/doors.h"
#include "scrap/fx.h"
#include "scrap/fxchar.h"
#include "scrap/fxcharactions.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/sound.h"
#include "scrap/timer.h"
#include "scrap/walkcharentity.h"
#include "scrap/weapons.h"
#include "scrap/world.h"

/* the color ramps (9 colors) of the particles */
#define FXC_RAMP_MONEY 0x430240

/* rand() & 0x7fff as 0..1 (double) */
static inline double FXC_Rnd(void) {
	return (double)(crt_rand() & 0x7fff) * (1.0 / 32767.0);
}

/* rand in [0, 1] times range, plus add (all in double) */
static inline float FXC_Rand(double range, double add) {
	return (float)(FXC_Rnd() * range + add);
}

static inline float FXC_Delta(void) {
	return cTimer_GetDelta(&g_pWorld->timer);
}

/* an entity that is deleted or off is let go */
static inline cEntity *FXC_Keep(cEntity *e) {
	if (e && (Entity_Removed(e) || !e->flag_4c)) {
		return NULL;
	}
	return e;
}

/* the node of name in the model, CHECKed */
static int FXC_Node(CGameModel *m, const char *name) {
	int i = CGameModel_FindRenderNode(m, name);

	CHECK(i > -1);
	return i;
}

/* the model of the entity, CHECKed */
static CGameModel *FXC_Model(cEntity *e) {
	CGameModel *m = Obj_Vtbl(e)->GetModel(e);

	CHECK(CGameModel_IsLoaded(m));
	return m;
}

/* the effect of the model off and its sort bias back */
static void FXC_EffectOff(CGameModel *m) {
	CGameModel_SetEffect(m, 0);
	CGameModel_SetSortZ(m, 0.0f, 1.0f);
}

static inline vec3_t FXC_CamPos(void) {
	cWorld *w = g_pWorld;

	return w->camPos[w->curCam];
}

/* ---- the setters ---- */

void __thiscall cObjElementFXMoneyTransfer_SetEntities(cObjElementFXMoneyTransfer *self, cEntity *from, cEntity *to) {
	self->from = from;
	CHECK(from->vftable->IsType(from, TYEN_WITH_LIFE));
	self->to = to;
	CHECK(to->vftable->IsType(to, TYEN_WITH_LIFE));
}

void __thiscall cObjElementFXCharacterDeath_SetEntity(cObjElementFXCharacterDeath *self, cEntity *e) {
	self->ent = e;
	FXC_Model(e);
}

void __thiscall FXSputnik_Start(cObjElementFXSputnik *self, cEntity *e) {
	vec3_t pos, rot;

	self->ent = e;
	CGameModel *m = FXC_Model(e);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	self->node = -1;
	self->node = CGameModel_FindRenderNode(m, "Bip Spootnick MASTER");
}

void __thiscall cObjElementFXCharacterConversion_SetAttacked(cObjElementFXCharacterConversion *self, cEntity *e) {
	self->attacked = e;
	if (e) {
		FXC_Model(e);
	}
}

void __thiscall cObjElementFXCharacterConversion_SetAttacker(cObjElementFXCharacterConversion *self, cEntity *e) {
	self->attacker = e;
	if (e) {
		FXC_Model(e);
		Entity_InConversion(self->attacker) = 1;
	}
}

void __thiscall cObjElementFXCharacterConversor_SetCharacter(cObjElementFXCharacterConversor *self, cEntity *e) {
	self->character = e;
	if (!e) {
		return;
	}
	CGameModel *m = FXC_Model(e);
	if (self->phase == 1) {
		CGameModel_SetEffect(m, 0);
		CGameModel_Enable(m);
	} else if (self->phase == 2) {
		CGameModel_SetEffect(m, 2);
		CGameModel_SetEffectParam(m, 0, 0);
		CGameModel_Enable(m);
	}
}

void __thiscall cObjElementFXCharacterConversor_SetConversor(cObjElementFXCharacterConversor *self, cEntity *e) {
	char buf[256];

	self->conversor = e;
	if (!e) {
		return;
	}
	CGameModel *m = FXC_Model(e);
	for (int i = 1; i <= 3; i++) {
		Str_PrintfQuiet(buf, 0xff, "DF_Pared_%02d", i);
		self->nodeWall[i - 1] = CGameModel_FindRenderNode(m, buf);
		Str_PrintfQuiet(buf, 0xff, "DF_Salida_%02d", i);
		self->nodeExit[i - 1] = CGameModel_FindRenderNode(m, buf);
	}
	for (int i = 1; i <= 4; i++) {
		Str_PrintfQuiet(buf, 0xff, "DF_Light_%02d", i);
		self->nodeLight[i - 1] = CGameModel_FindRenderNode(m, buf);
	}
}

void __thiscall FXChar_SetI4(void *self, int v) {
	*(int *)((byte *)self + 4) = v;
}

void __thiscall cObjElementFXMaintenanceWork_SetMaintenance(cObjElementFXMaintenanceWork *self, cEntity *e) {
	self->maintenance = e;
	CGameModel *m = FXC_Model(e);
	self->nodeLaser = CGameModel_FindRenderNode(m, "DF_Laser");
	CHECK(self->nodeLaser > -1);
}

void __thiscall cObjElementFXMaintenanceWork_SetShip(cObjElementFXMaintenanceWork *self, cEntity *e) {
	self->ship = e;
	FXC_Model(e);
}

void __thiscall FXSputnik_SetB(cObjElementFXSputnik *self, float v) {
	self->minMove = v;
}

void __thiscall FXSputnik_SetA(cObjElementFXSputnik *self, float v) {
	self->duration = v;
}

/* ---- Attach (vf03) and Reset (vf04) ---- */

/* a new random point of the conversion trails */
static void FXConversion_NewPoint(cObjElementFXCharacterConversion *self, int i) {
	vec3_t tmp;

	float r = FXC_Rand(0.699999988079071, 0.30000001192092896);
	const vec3_t *p = FX_RandomDir(&tmp);
	self->dir[i].x = r * p->x;
	self->dir[i].y = p->y * r;
	self->dir[i].z = p->z * r;
	self->speed[i] = FXC_Rand(12.0, 12.0);
	self->scale[i] = FXC_Rand(0.800000011920929, 0.699999988079071);
}

static void FXConversion_RandomPoints(cObjElementFXCharacterConversion *self) {
	for (int i = 0; i < FXCONVERSION_POINTS; i++) {
		self->phase[i] = (float)(FXC_Rnd() + 0.0);
		FXConversion_NewPoint(self, i);
	}
	self->f178 = 0.0f;
}

void __thiscall cObjElementFXCharacterConversion_Attach(cObjElementFXCharacterConversion *self, cDebris *d) {
	CGameModel *m = Debris_Model(d);
	uint32_t i = CGameModel_IsLoaded(m) ? (uint32_t)CGameModel_FindRenderNode(m, "Dummy01") : (uint32_t)(uintptr_t)m;

	if (i != 0xffffffffu) {
		CGameParticles_Attach(&self->particles, m, i);
	}
	CHECK(self->particles.ParticleSystem);
	if (CGameModel_IsLoaded(m)) {
		i = (uint32_t)CGameModel_FindRenderNode(m, "Dummy02");
	}
	if (i != 0xffffffffu) {
		CGameParticles_Attach(&self->particles2, m, i);
	}
	CHECK(self->particles2.ParticleSystem);
	for (int k = 0; k < FXCONVERSION_TRAILS; k++) {
		CGameTrail_SetMaxTrails(&self->trails[k], 15);
		CGameTrail_SetType(&self->trails[k], 3);
	}
	FXConversion_RandomPoints(self);
}

void __thiscall cObjElementFXCharacterConversion_Reset(cObjElementFXCharacterConversion *self, int arg) {
	(void)arg;
	self->time = 0.0f;
	self->emitT = 0.0f;
	self->attacker = NULL;
	self->attacked = NULL;
	for (int k = 0; k < FXCONVERSION_TRAILS; k++) {
		CGameTrail_Clear(&self->trails[k]);
	}
	FXConversion_RandomPoints(self);
}

void __thiscall cObjElementFXCharacterConversor_Attach(cObjElementFXCharacterConversor *self, cDebris *d) {
	FX_AttachDummy(&self->particles, Debris_Model(d));
	self->numTrails = FXCONVERSOR_TRAILS;
	for (int i = 0; i < self->numTrails; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 12);
		CGameTrail_SetSmooth(t, 1);
		CGameTrail_SetColor(t, 0x72b0c6);
		CGameTrail_SetTexture(t, 9);
		CGameTrail_SetPremul(t, 1);
		CGameTrail_SetUVScale(t, 8.0f, 1.0f);
		self->width[i] = 0.5f;
		self->trailT[i] = -1.0f;
	}
	self->numPoints = 5;
}

void __thiscall cObjElementFXCharacterConversor_Reset(cObjElementFXCharacterConversor *self, int arg) {
	(void)arg;
	self->time = 0.0f;
	self->emitT = 0.0f;
	self->phase = 0;
	self->character = NULL;
	self->conversor = NULL;
	CHECK(&self->particles);
	CHECK(self->particles.ParticleSystem);
	CGameParticles_KillAll(&self->particles);
	for (int i = 0; i < self->numTrails; i++) {
		CGameTrail_Clear(&self->trails[i]);
		self->trailT[i] = 0.0f;
	}
}

static void FXDeath_ResetTrails(cObjElementFXCharacterDeath *self, byte attach) {
	self->time = 0.0f;
	self->emitT = 0.0f;
	self->lastTime = 0.0f;
	for (int k = 0; k < FXDEATH_TRAILS; k++) {
		CGameTrail *t = &self->trails[k];
		if (attach) {
			CGameTrail_SetMaxTrails(t, 31);
			CGameTrail_SetType(t, 3);
			CGameTrail_SetTexture(t, 12);
		} else {
			CGameTrail_Clear(t);
		}
	}
	for (int i = 0; i < FXDEATH_POINTS; i++) {
		self->phase[i] = 5.0f;
	}
}

void __thiscall cObjElementFXCharacterDeath_Attach(cObjElementFXCharacterDeath *self, cDebris *d) {
	CGameModel *m = Debris_Model(d);

	CHECK(CGameModel_IsLoaded(m));
	CGameParticles_Attach(&self->particles, m, FXC_Node(m, "Dummy01"));
	CGameParticles_Attach(&self->particles2, m, FXC_Node(m, "Dummy02"));
	CGameParticles_Attach(&self->particles3, m, FXC_Node(m, "Dummy03"));
	FXDeath_ResetTrails(self, 1);
}

void __thiscall cObjElementFXCharacterDeath_Reset(cObjElementFXCharacterDeath *self, int arg) {
	(void)arg;
	FXDeath_ResetTrails(self, 0);
}

void __thiscall cObjElementFXCharacterTeleport_Attach(cObjElementFXCharacterTeleport *self, cDebris *d) {
	FX_AttachDummy(&self->particles, Debris_Model(d));
}

void __thiscall cObjElementFXCharacterTeleport_Reset(cObjElementFXCharacterTeleport *self, int arg) {
	(void)arg;
	self->t = 0.0f;
	self->emitT = 0.0f;
	self->mode = 0;
}

void __thiscall cObjElementFXMaintenanceWork_Attach(cObjElementFXMaintenanceWork *self, cDebris *d) {
	CGameModel *m = Debris_Model(d);

	CHECK(CGameModel_IsLoaded(m));
	CGameParticles_Attach(&self->particles, m, FXC_Node(m, "Dummy01"));
	CHECK(self->particles.ParticleSystem);
	CGameTrail_SetMaxTrails(&self->trails[0], 3);
	CGameTrail_SetTexture(&self->trails[0], 0);
	CGameTrail_SetMaxTrails(&self->trails[1], 3);
	CGameTrail_SetTexture(&self->trails[1], 3);
	self->life = 0.0f;
	self->sound = 0;
}

void __thiscall cObjElementFXMaintenanceWork_Reset(cObjElementFXMaintenanceWork *self, int arg) {
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	CGameParticles_KillAll(&self->particles);
	CGameTrail_Clear(&self->trails[0]);
	CGameTrail_Clear(&self->trails[1]);
	self->life = 0.0f;
	self->sound = 0;
}

void __thiscall cObjElementFXMoneyTransfer_Attach(cObjElementFXMoneyTransfer *self, cDebris *d) {
	CGameModel *m = Debris_Model(d);

	CHECK(CGameModel_IsLoaded(m));
	CGameParticles_Attach(&self->particles, m, FXC_Node(m, "Dummy01"));
}

void __thiscall cObjElementFXMoneyTransfer_Reset(cObjElementFXMoneyTransfer *self, int arg) {
	(void)arg;
	self->from = NULL;
	self->to = NULL;
	self->emitT = 0.0f;
}

void __thiscall cObjElementFXSputnik_Attach(cObjElementFXSputnik *self, cDebris *d) {
	FX_AttachDummy(&self->particles, Debris_Model(d));
	self->moveT = 0.0f;
}

void __thiscall cObjElementFXSputnik_Reset(cObjElementFXSputnik *self, int arg) {
	(void)arg;
	self->t = 0.0f;
	self->emitT = 0.0f;
	self->duration = 0.0f;
	CGameParticles_KillAll(&self->particles);
	self->moveT = 0.0f;
}

/* ---- Alive (vf02) ---- */

byte __thiscall cObjElementFXCharacterConversion_Alive(cObjElementFXCharacterConversion *self, cDebris *d,
	cDebrisSys *sys) {
	if (cDebris_Update(d, sys)) {
		return 1;
	}
	if (self->attacker) {
		CGameModel *m = Obj_Vtbl(self->attacker)->GetModel(self->attacker);
		if (m && CGameModel_IsLoaded(m)) {
			CHECK(CGameModel_IsLoaded(m));
			FXC_EffectOff(m);
			CGameModel_Disable(m);
		}
	}
	return 0;
}

byte __thiscall cObjElementFXCharacterConversor_Alive(cObjElementFXCharacterConversor *self, cDebris *d,
	cDebrisSys *sys) {
	if (cDebris_Update(d, sys)) {
		return 1;
	}
	if (self->character) {
		CGameModel *m = Obj_Vtbl(self->character)->GetModel(self->character);
		if (m && CGameModel_IsLoaded(m)) {
			CHECK(CGameModel_IsLoaded(m));
			if (self->phase == 1) {
				FXC_EffectOff(m);
				CGameModel_Disable(m);
				return 0;
			}
			if (self->phase == 2) {
				FXC_EffectOff(m);
				CGameModel_Enable(m);
			}
		}
	}
	return 0;
}

byte __thiscall cObjElementFXCharacterTeleport_Alive(cObjElementFXCharacterTeleport *self, cDebris *d,
	cDebrisSys *sys) {
	if (cDebris_Update(d, sys)) {
		return 1;
	}
	if (self->mode == 2 && self->character) {
		FXC_EffectOff(FXC_Model(self->character));
	}
	return 0;
}

byte __thiscall cObjElementFXMaintenanceWork_Alive(cObjElementFXMaintenanceWork *self, cDebris *d, cDebrisSys *sys) {
	if (cDebris_Update(d, sys)) {
		return 1;
	}
	if (self->sound) {
		CSoundSys *snd = g_pSound;
		snd->vftable->StopSound(snd, self->sound);
		self->sound = 0;
	}
	return 0;
}

byte __thiscall cObjElementFXSputnik_Alive(cObjElementFXSputnik *self, cDebris *d, cDebrisSys *sys) {
	if (self->duration == 0.0f) {
		Debris_Life(d) = 1.0f;
	}
	return cDebris_Update(d, sys);
}

/* ---- Update (vf01) ---- */

void __thiscall cObjElementFXMoneyTransfer_Update(cObjElementFXMoneyTransfer *self, cDebris *d, float dt) {
	vec3_t a, b, mid, dir;

	(void)dt;
	self->from = FXC_Keep(self->from);
	if (!self->from || !self->from->flag_4c) {
		return;
	}
	self->to = FXC_Keep(self->to);
	if (!self->to || !self->to->flag_4c) {
		return;
	}
	if (0.65f > Debris_Life(d)) {
		return;
	}
	Obj_Vtbl(self->from)->GetViewPos(self->from, &a);
	Obj_Vtbl(self->to)->GetViewPos(self->to, &b);
	float rFrom = Obj_Vtbl(self->from)->GetRadius(self->from);
	float rTo = Obj_Vtbl(self->to)->GetRadius(self->to);
	mid.x = (b.x - a.x) * 0.5f + a.x;
	mid.y = (b.y - a.y) * 0.5f + a.y;
	mid.z = (b.z - a.z) * 0.5f + a.z;
	CGameModel_SetPos(Debris_Model(d), &mid);
	float t = FXC_Delta() + self->emitT;
	self->emitT = t;
	if (0.04f > t) {
		return;
	}
	self->emitT = 0.0f;
	FX_RandomDir(&dir);
	float life = FXC_Rand(0.19999998807907104, 0.44999998807907104);
	CGameParticles *sp = &self->particles;
	CGameParticles_SetSparksOn(sp, 0);
	sp->birthRate = 0;
	float r = Rand_FloatRange(rFrom * 0.25f, rFrom * 0.5f);
	sp->initPosMin.x = dir.x * r + a.x;
	sp->initPosMin.y = dir.y * r + a.y;
	sp->initPosMin.z = dir.z * r + a.z;
	sp->initPosMax = sp->initPosMin;
	r = Rand_FloatRange(rTo * 0.25f, rTo * 0.5f);
	float inv = 1.0f / life;
	sp->initVelMin.x = ((dir.x * r + b.x) - sp->initPosMin.x) * inv;
	sp->initVelMin.y = ((dir.y * r + b.y) - sp->initPosMin.y) * inv;
	sp->initVelMin.z = ((dir.z * r + b.z) - sp->initPosMin.z) * inv;
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initAccMax = sp->initAccMin;
	sp->lifeTime = life;
	sp->sizeLife = 1.0f;
	sp->sizeIni = sp->sizeEnd = FXC_Rand(5.0, 35.0);
	sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
	sp->rotVar = (float)(FXC_Rnd() * 0.7853981852531433 - 0.39269909262657166);
	byte sprite = (byte)(crt_rand() % 7 + 1);
	sp->spriteIni = sprite;
	sp->spriteEnd = sprite + 8;
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, GAME_PTR(cObjElementFXMoneyTransfer_Update_430240, 0, FXC_RAMP_MONEY));
	CGameParticles_EmitN(sp, 1, 0);
}

/* the vertical speed of the sparks of the conversion: up or down by the height band of the emitter */
static float FXConversion_SparkVel(const CGameParticles *sp, float q) {
	if ((int)(sp->initPosMin.y / 10.0f) & 1) {
		return -(FXC_Rand(25.0, 300.0) * q);
	}
	return FXC_Rand(25.0, 300.0) * q;
}

/* the sparks at a vertex v of the attacked: a still velocity up or down and the acceleration against it */
static void FXConversion_Spark(CGameParticles *sp, const vec3_t *v, double range, double add, float q, double accAdd) {
	vec3_t tmp;

	float r = FXC_Rand(range, add);
	const vec3_t *p = FX_RandomDir(&tmp);
	sp->initPosMin.x = r * p->x + v->x;
	sp->initPosMin.y = p->y * r + v->y;
	sp->initPosMin.z = p->z * r + v->z;
	sp->initPosMax = sp->initPosMin;
	float vy = FXConversion_SparkVel(sp, q);
	sp->initVelMin.z = 0.0f;
	sp->initVelMin.y = vy;
	sp->initVelMin.x = 0.0f;
	sp->initVelMax = sp->initVelMin;
	float s = FXC_Rand(0.20000004768371582, accAdd);
	sp->initAccMin.x = -sp->initVelMin.x * s;
	sp->initAccMin.y = -sp->initVelMin.y * s;
	sp->initAccMin.z = -sp->initVelMin.z * s;
	sp->initAccMax = sp->initAccMin;
	sp->lifeTime = FXC_Rand(0.040000081062316895, 1.1799999475479126) * 0.18000000715255737f;
}

#define FXCONVERSION_SPARK_T 0.005000000353902578f
#define FXCONVERSION_SPARK_VERTS 12

void __thiscall cObjElementFXCharacterConversion_Update(cObjElementFXCharacterConversion *self, cDebris *d, float dt) {
	vec3_t tmp, tmp2, pa, pb, D, nOut, verts[FXCONVERSION_SPARK_VERTS];
	float fr;

	(void)dt;
	self->attacker = FXC_Keep(self->attacker);
	if (!self->attacker || !self->attacker->flag_4c) {
		return;
	}
	self->attacked = FXC_Keep(self->attacked);
	if (!self->attacked || !self->attacked->flag_4c) {
		return;
	}
	tmp = FXC_CamPos();
	CGameModel_SetPos(Debris_Model(d), &tmp);
	CGameModel_SetSortZ(Debris_Model(d), -63.0f, 0.0f);
	float delta = FXC_Delta();
	self->time = self->time + delta;
	self->emitT = self->emitT + delta;
	CGameModel *ma = Obj_Vtbl(self->attacker)->GetModel(self->attacker);
	if (!ma || !CGameModel_IsLoaded(ma)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(ma));
	CGameModel *mb = Obj_Vtbl(self->attacked)->GetModel(self->attacked);
	if (!mb || !CGameModel_IsLoaded(mb)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(mb));
	/* the attacker fades into the effect */
	FX_InRange(self->time, 0.01f, 0.05f, &fr);
	CGameModel_SetSortZ(ma, -62.9f, 0.0f);
	CGameModel_SetEffect(ma, 2);
	float f = fr * fr * (2.0f - fr);
	uint32_t bits;
	memcpy(&bits, &f, sizeof(bits));
	CGameModel_SetEffectParam(ma, 0, bits);
	float ra = Obj_Vtbl(self->attacker)->GetRadius(self->attacker);
	float rb = Obj_Vtbl(self->attacked)->GetRadius(self->attacked);
	if (FX_InRange(self->time, 0.0f, 0.2f, &fr)) {
		/* the trails from the attacker to the attacked */
		const vec3_t *b = CGameModel_GetPos(mb, &pb);
		const vec3_t *a = CGameModel_GetPos(ma, &pa);
		float dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;
		float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		int alpha = (int)(fr * fr * (2.0f - fr) * 255.0f);
		for (int k = 0; k < FXCONVERSION_TRAILS; k++) {
			CGameTrail_Reset(&self->trails[k]);
			FX_InRange(self->time, 0.1f, 0.2f, &fr);
			CGameTrail_SetTransp(&self->trails[k], (byte)alpha);
		}
		float distK = dist * 0.0005000000237487257f;
		int trail = 0, point = 0;
		for (int i = 0, idx = 254; i < FXCONVERSION_POINTS; i++, idx--) {
			float ph = self->phase[i];
			if (0.0f > ph || ph > 1.0f) {
				self->phase[i] = 0.0f;
				FXConversion_NewPoint(self, i);
			}
			self->phase[i] = (self->scale[i] * self->emitT) / distK + self->phase[i];
			const vec3_t *dir = &self->dir[i];
			float ox = ra * dir->x * 0.5f, oy = dir->y * ra * 0.5f, oz = ra * dir->z * 0.5f;
			const vec3_t *c = Obj_Vtbl(self->attacker)->GetViewPos(self->attacker, &tmp);
			vec3_t A = { ox + c->x, c->y + oy, c->z + oz };
			float bx = rb * dir->x * 0.5f, by = dir->y * rb * 0.5f, bz = rb * dir->z * 0.5f;
			const vec3_t *pB = self->attacked->vftable->GetPos(self->attacked, &tmp2);
			D.x = (bx + pB->x) - A.x;
			D.y = (pB->y + by) - A.y;
			D.z = (pB->z + bz) - A.z;
			Vec3_NormalizeTo(&nOut, &D);
			vec3_t n = nOut;
			float sp = self->speed[i];
			ph = self->phase[i];
			vec3_t P;
			P.x = (D.x - n.x * sp * 2.0f) * ph + A.x;
			P.y = (D.y - n.y * sp * 2.0f) * ph + A.y;
			P.z = (D.z - n.z * sp * 2.0f) * ph + A.z;
			int alphaP = 0;
			if (0.1f > ph) {
				alphaP = (int)((1.0f - ph / 0.1f) * 255.0f);
			} else if (ph > 0.699999988079071f) {
				alphaP = (int)(((ph - 0.699999988079071f) / 0.699999988079071f) * 255.0f);
			}
			uint32_t top = (uint32_t)alphaP << 24;
			sp = self->speed[i];
			vec3_t T = { P.x - n.x * sp, P.y - n.y * sp, P.z - n.z * sp };
			CGameTrail_Add(&self->trails[trail], T, 6.0f, top | 0x204020);
			sp = self->speed[i];
			vec3_t Q = { n.x * sp + P.x, n.y * sp + P.y, n.z * sp + P.z };
			CGameTrail_Add(&self->trails[trail], Q, 12.0f, top | 0x80f580);
			int a2 = alphaP + alpha;
			if (a2 > 255) {
				a2 = 255;
			}
			sp = self->speed[i];
			Q.x = n.x * sp + P.x;
			Q.y = n.y * sp + P.y;
			Q.z = n.z * sp + P.z;
			int sprite = crt_rand() % 8 + 1;
			CGameParticles_SetParticle(&self->particles2, idx, Q.x, Q.y, Q.z, 7.0f,
				((uint32_t)a2 << 24) | 0xc4ffc4, sprite, 0.0f);
			if (++point >= 15) {
				trail++;
				point = 0;
			}
		}
	}
	/* the sparks on the attacked */
	if (FXCONVERSION_SPARK_T > self->emitT) {
		return;
	}
	float t = self->time - self->emitT;
	if (!(self->time > t)) {
		return;
	}
	do {
		self->emitT = 0.0f;
		if (FX_InRange(t, 0.075f, 0.12f, &fr)) {
			CGameModel_Vertices(mb, verts, NULL, NULL, FXCONVERSION_SPARK_VERTS, -1, NULL, 0xffffffffu, NULL);
			float q = rb / 80.0f;
			CGameParticles *sp2 = &self->particles2;
			CGameParticles *sp = &self->particles;
			for (int j = 0; j < FXCONVERSION_SPARK_VERTS; j++) {
				sp2->birthRate = 0;
				FXConversion_Spark(sp2, &verts[j], 2.0, 1.0, q, 1.5);
				sp2->sizeIni = FXC_Rand(0.09999999403953552, 0.3199999928474426) * rb * 0.3199999928474426f;
				sp2->sizeLife = 1.0f;
				sp2->rotIni = 0.0f;
				sp2->rotEnd = 0.0f;
				sp2->rotVar = 0.0f;
				sp2->sizeEnd = FXC_Rand(0.09999999403953552, 0.3199999928474426) * rb * 0.3199999928474426f;
				byte sprite = (byte)(crt_rand() % 8 + 1);
				sp2->spriteIni = sprite;
				sp2->spriteEnd = sprite;
				CGameParticles_Apply(sp2);
				CGameParticles_SetColorRampOn(sp2, 1);
				CGameParticles_SetColorRamp(sp2, GAME_PTR(cObjElementFXCharacterConversion_Emit_430464, 0, 0x430464));
				CGameParticles_EmitDt(sp2, 1, 0, FXC_Rand(0.10000000149011612, 0.0));
				sp->birthRate = 0;
				FXConversion_Spark(sp, &verts[j], 4.0, 2.0, q, 1.2999999523162842);
				sp->sizeIni = FXC_Rand(0.09999999403953552, 0.3199999928474426) * rb * 1.2000000476837158f;
				sp->sizeLife = 0.3f;
				sp->sizeEnd = FXC_Rand(0.09999999403953552, 0.3199999928474426) * rb * 1.899999976158142f;
				sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
				sp->rotVar = (float)(FXC_Rnd() * 1.5707963705062866 - 0.7853981852531433);
				sprite = (crt_rand() & 1) ? 5 : 9;
				sp->spriteIni = sprite;
				sp->spriteEnd = sprite;
				CGameParticles_Apply(sp);
				CGameParticles_SetColorRampOn(sp, 1);
				CGameParticles_SetColorRamp(sp, GAME_ARRAY(uint32_t, cObjElementFXCharacterConversion_Emit_4303f8, 0x4303f8) + 9 * (crt_rand() & 1));
				CGameParticles_EmitDt(sp, 1, 0, FXC_Rand(0.10000000149011612, 0.0));
			}
		}
		t = t + FXCONVERSION_SPARK_T;
	} while (self->time > t);
}

/* the alpha a plus add (at most 255) in the top byte of rgb */
static inline uint32_t FXC_Alpha(int a, int add, uint32_t rgb) {
	int v = a + add;

	if (v > 255) {
		v = 255;
	}
	return ((uint32_t)v << 24) | rgb;
}

/* a still particle at p */
static void FXC_SetStill(CGameParticles *sp, const vec3_t *p) {
	sp->initPosMin.x = p->x + 0.0f;
	sp->initPosMin.y = p->y + 0.0f;
	sp->initPosMin.z = p->z + 0.0f;
	sp->initPosMax = sp->initPosMin;
	sp->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initAccMax = sp->initAccMin;
}

void __thiscall cObjElementFXMaintenanceWork_Update(cObjElementFXMaintenanceWork *self, cDebris *d, float dt) {
	vec3_t laserPos, c, diff, n, dir, rot;

	self->maintenance = FXC_Keep(self->maintenance);
	if (!self->maintenance || !self->maintenance->flag_4c) {
		return;
	}
	self->ship = FXC_Keep(self->ship);
	if (!self->ship || !self->ship->flag_4c) {
		return;
	}
	CGameModel_SetSortZ(Debris_Model(d), -75.0f, 0.0f);
	float delta = FXC_Delta();
	self->time = self->time + delta;
	self->emitTime = self->emitTime + delta;
	/* the work ends with the edit door of the ship changing or the robot not working */
	cEntity *door = World_FindEntity("DM_Door_EditShip_00");
	if (!door || (self->life > 0.0f && cActCtrl_IsActive(WithLife_Vtbl(door)->GetActCtrl(door), "Change"))) {
		self->life = delta;
	}
	cEntity *robot = self->maintenance;
	if (!cActCtrl_IsActive(WithLife_Vtbl(robot)->GetActCtrl(robot), "Work") && self->life > 0.1f) {
		self->life = 0.1f - delta;
	}
	float life = self->life - delta;
	self->life = life;
	if (!(life > 0.0f)) {
		CGameTrail_Clear(&self->trails[0]);
		CGameTrail_Clear(&self->trails[1]);
		CGameParticles_KillRange(&self->particles, 0, 6);
		if (self->sound) {
			CSoundSys *snd = g_pSound;
			snd->vftable->StopSound(snd, self->sound);
			self->sound = 0;
		}
		return;
	}
	int alpha = 255 - (int)(Float_MinSS(1.0f, life / 0.1f) * 255.0f);
	CGameTrail_SetTransp(&self->trails[0], (byte)alpha);
	CGameTrail_SetTransp(&self->trails[1], (byte)alpha);
	CGameModel *mm = Obj_Vtbl(self->maintenance)->GetModel(self->maintenance);
	CHECK(CGameModel_IsLoaded(mm));
	CGameModel *ms = Obj_Vtbl(self->ship)->GetModel(self->ship);
	CHECK(CGameModel_IsLoaded(ms));
	CGameModel_Vertices(mm, &laserPos, NULL, NULL, 1, 0, NULL, (uint32_t)self->nodeLaser, NULL);
	vec3_t laser = laserPos;
	Obj_Vtbl(self->ship)->GetViewPos(self->ship, &c);
	CGameModel_Vertices(ms, &c, NULL, NULL, 1, self->vertex, NULL, 0xffffffffu, NULL);
	/* the beam */
	CGameTrail_Reset(&self->trails[0]);
	CGameTrail_Add(&self->trails[0], laser, 16.0f, 0x880c4ff);
	CGameTrail_Add(&self->trails[0], c, 16.0f, 0x880c4ff);
	CGameTrail_Reset(&self->trails[1]);
	CGameTrail_Add(&self->trails[1], laser, 84.0f, 0x40202080);
	CGameTrail_Add(&self->trails[1], c, 84.0f, 0x40202080);
	/* the glows at both ends */
	CGameParticles *sp = &self->particles;
	CGameParticles_SetParticle(sp, 0, laser.x, laser.y, laser.z, 24.0f, FXC_Alpha(alpha, 8, 0xe6e6ff), 1, 0.0f);
	CGameParticles_SetParticle(sp, 1, laser.x, laser.y, laser.z, 84.0f, FXC_Alpha(alpha, 16, 0xa2a2ff), 1, 0.0f);
	CGameParticles_SetParticle(sp, 2, laser.x, laser.y, laser.z, 120.0f, FXC_Alpha(alpha, 128, 0x4040ff), 5, 0.0f);
	CGameParticles_SetParticle(sp, 3, c.x, c.y, c.z, 30.0f, FXC_Alpha(alpha, 8, 0xdcdcff), 1, 0.0f);
	CGameParticles_SetParticle(sp, 4, c.x, c.y, c.z, 95.0f, FXC_Alpha(alpha, 64, 0xa2a2ff), 1, 0.0f);
	CGameParticles_SetParticle(sp, 5, c.x, c.y, c.z, 155.0f, FXC_Alpha(alpha, 192, 0x4040ff), 2, 0.0f);
	if (!(self->emitTime > 0.04f)) {
		return;
	}
	self->emitTime = 0.0f;
	/* the flashes and sparks at the ship */
	sp->birthRate = 0;
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, GAME_PTR(cObjElementFXMaintenanceWork_Update_430520, 0, 0x430520));
	FXC_SetStill(sp, &c);
	sp->sizeIni = FXC_Rand(10.0, 30.0);
	sp->sizeEnd = FXC_Rand(20.0, 240.0);
	float lt = FXC_Rand(0.040000006556510925, 0.1899999976158142);
	sp->sizeLife = 0.15f;
	sp->spriteIni = 5;
	sp->spriteEnd = 5;
	sp->lifeTime = Float_MinSS(Debris_Life(d) - dt, lt);
	CGameParticles_Apply(sp);
	CGameParticles_EmitN(sp, 1, 1);
	CGameParticles_SetColorRamp(sp, GAME_PTR(cObjElementFXMaintenanceWork_Update_430520, 0, 0x430520));
	FXC_SetStill(sp, &c);
	sp->sizeIni = FXC_Rand(2.0, 4.0);
	sp->sizeEnd = FXC_Rand(5.0, 45.0);
	lt = FXC_Rand(0.019999995827674866, 0.18000000715255737);
	sp->sizeLife = 0.15f;
	sp->spriteIni = 1;
	sp->spriteEnd = 1;
	sp->lifeTime = Float_MinSS(Debris_Life(d) - dt, lt);
	CGameParticles_Apply(sp);
	CGameParticles_EmitN(sp, 1, 1);
	int sparks = (int)FXC_Rand(5.0, 3.0);
	CGameParticles_SetSparksOn(sp, 1);
	for (; sparks > 0; sparks--) {
		CGameParticles_SetColorRamp(sp, GAME_PTR(cObjElementFXMaintenanceWork_Update_430568, 0, 0x430568));
		float r1 = (float)(FXC_Rnd() * 1.2566370964050293 - 0.6283185482025146);
		float r2 = (float)(FXC_Rnd() * 1.2566370964050293 - 0.6283185482025146);
		rot.x = (float)(FXC_Rnd() * 1.2566370964050293 - 0.6283185482025146);
		rot.y = r2;
		rot.z = r1;
		diff.x = laser.x - c.x;
		diff.y = laser.y - c.y;
		diff.z = laser.z - c.z;
		Vec3_Rotate(&dir, Vec3_NormalizeTo(&n, &diff), &rot);
		float s = FXC_Rand(20.0, 5.0);
		sp->initPosMin.x = s * dir.x + c.x;
		sp->initPosMin.y = s * dir.y + c.y;
		sp->initPosMin.z = s * dir.z + c.z;
		sp->initPosMax = sp->initPosMin;
		s = FXC_Rand(900.0, 550.0);
		sp->initVelMin.x = s * dir.x;
		sp->initVelMin.y = s * dir.y;
		sp->initVelMin.z = s * dir.z;
		sp->initVelMax = sp->initVelMin;
		float a = FXC_Rand(0.04999999701976776, 0.15000000596046448);
		sp->initAccMin.x = sp->initVelMin.x * a + 0.0f;
		sp->initAccMin.y = a * sp->initVelMin.y - 1550.0f;
		sp->initAccMin.z = a * sp->initVelMin.z + 0.0f;
		sp->initAccMax = sp->initAccMin;
		float e = FXC_Rand(0.029999999329447746, 0.0);
		float rem = Debris_Life(d) - dt;
		lt = Float_MinSS(rem, FXC_Rand(0.09999999403953552, 0.30000001192092896));
		CGameParticles_SetSparksOn(sp, 1);
		sp->lifeTime = lt;
		float size = FXC_Rand(10.0, 55.0);
		sp->sizeLife = 1.0f;
		sp->rotIni = 0.0f;
		sp->rotEnd = 0.0f;
		sp->rotVar = 0.0f;
		sp->sizeIni = size;
		sp->sizeEnd = size;
		byte sprite = (byte)((crt_rand() & 1) + 0x41);
		sp->spriteEnd = 0x43;
		sp->sparkVelScale = 1.0f;
		sp->spriteIni = sprite;
		sp->sparkMaxSize = FXC_Rand(0.699999988079071, 0.30000001192092896);
		CGameParticles_Apply(sp);
		CGameParticles_EmitDt(sp, 1, 0, e + 0.03f);
	}
	CGameParticles_SetSparksOn(sp, 0);
}

#define FXC_RAMP_TELEPORT 0x430340	/* 3 color ramps of 9 colors */
#define FXC_TELEPORT_RAMPS (GAME_ARRAY(uint32_t, FXC_TELEPORT_RAMPS, FXC_RAMP_TELEPORT))	/* [3 * 9] */
#define FXC_RAMP_TELEPORT_GLOW 0x430388

static inline uint32_t *FXC_Ramp(uint32_t *ramps, int i) {
	return ramps + i * 9;
}

/* a particle that does not move at x y z */
static void FXC_Still(CGameParticles *sp, float x, float y, float z) {
	sp->initPosMin.x = x;
	sp->initPosMin.y = y;
	sp->initPosMin.z = z;
	sp->initPosMax = sp->initPosMin;
	sp->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initAccMax = sp->initAccMin;
}

/* velocity and acceleration along y only */
static void FXC_VelY(CGameParticles *sp, float vy, float ay) {
	sp->initVelMin = (vec3_t){ 0.0f, vy, 0.0f };
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin = (vec3_t){ 0.0f, ay, 0.0f };
	sp->initAccMax = sp->initAccMin;
}

/* the glow of the teleport on the character at x y z, its size by the radius r */
static void FXTeleport_Glow(CGameParticles *sp, float x, float y, float z, float r) {
	CGameParticles_SetSparksOn(sp, 0);
	sp->birthRate = 0;
	FXC_Still(sp, x, y, z);
	sp->rotVar = 0.0f;
	sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
	sp->sizeLife = 1.0f;
	sp->lifeTime = FXC_Rand(0.019999995827674866, 0.18000000715255737);
	sp->spriteIni = 9;
	sp->spriteEnd = 9;
	sp->sizeIni = sp->sizeEnd = FXC_Rand(0.099999904632568359, 1.2000000476837158) * r;
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FXC_Ramp(FXC_TELEPORT_RAMPS, crt_rand() % 3));
	CGameParticles_EmitN(sp, 1, 0);
}

/* a spark out of the center c: up (mode 1) or down from yDown, the speed and acceleration along y */
static void FXTeleport_Spark(CGameParticles *sp, const vec3_t *c, int mode, float yUp, float yDown, double velRange,
	double velAdd, float size, float maxSize, int sprite) {
	vec3_t d;

	FX_RandomDir(&d);
	CGameParticles_SetSparksOn(sp, 1);
	sp->birthRate = 0;
	float k = FXC_Rand(25.0, 75.0);
	float vy, ay;
	if (mode == 1) {
		sp->initPosMin.x = (c->x + 0.0f) + d.x * k;
		sp->initPosMin.y = (c->y + yUp) + d.y * k;
		sp->initPosMin.z = (c->z + 0.0f) + d.z * k;
		sp->initPosMax = sp->initPosMin;
		vy = FXC_Rand(velRange, velAdd);
		ay = FXC_Rand(150.0, 550.0);
	} else {
		sp->initPosMin.x = (c->x + 0.0f) + d.x * k;
		sp->initPosMin.y = (c->y + yDown) + d.y * k;
		sp->initPosMin.z = (c->z + 0.0f) + d.z * k;
		sp->initPosMax = sp->initPosMin;
		vy = -FXC_Rand(velRange, velAdd);
		ay = -FXC_Rand(150.0, 550.0);
	}
	FXC_VelY(sp, vy, ay);
	sp->rotIni = 0.0f;
	sp->rotEnd = 0.0f;
	sp->rotVar = 0.0f;
	sp->sizeLife = 1.0f;
	sp->sizeIni = size;
	sp->sizeEnd = size;
	sp->lifeTime = FXC_Rand(0.19999998807907104, 0.30000001192092896);
	if (sprite < 0) {
		sprite = (crt_rand() & 1) + 0x45;
	}
	sp->spriteIni = (byte)sprite;
	sp->spriteEnd = (byte)sprite;
	sp->sparkMaxSize = maxSize;
	sp->sparkVelScale = 100.0f;
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FXC_Ramp(FXC_TELEPORT_RAMPS, crt_rand() & 1));
	CGameParticles_EmitN(sp, 1, 0);
}

/* the smoke along the character: the random rotation, the life (random for life < 0), sprite 9 */
static void FXTeleport_Smoke(CGameParticles *sp, float life) {
	sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
	sp->rotVar = FXC_Rand(1.5707963705062866, 0.0);
	if (life < 0.0f) {
		life = FXC_Rand(0.15000000596046448, 0.34999999403953552);
	}
	sp->lifeTime = life;
	sp->sizeIni = FXC_Rand(5.0, 55.0);
	sp->sizeLife = 0.1f;
	sp->spriteIni = 9;
	sp->spriteEnd = 9;
	sp->sizeEnd = FXC_Rand(5.0, 70.0);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FXC_Ramp(FXC_TELEPORT_RAMPS, crt_rand() % 2));
	CGameParticles_Apply(sp);
	CGameParticles_EmitN(sp, 1, 0);
}

#define FXTELEPORT_VERTS 0x60
#define FXTELEPORT_GLOWS 0x18

void __thiscall cObjElementFXCharacterTeleport_Update(cObjElementFXCharacterTeleport *self, cDebris *d, float dt) {
	vec3_t c, p, diff, n, verts[FXTELEPORT_VERTS];
	uint32_t bits;

	(void)dt;
	self->character = FXC_Keep(self->character);
	if (!self->character || !self->character->flag_4c) {
		return;
	}
	self->teleport = FXC_Keep(self->teleport);
	if (!self->teleport || !self->teleport->flag_4c) {
		return;
	}
	vec3_t cam = FXC_CamPos();
	CGameModel_SetPos(Debris_Model(d), &cam);
	CGameModel_SetSortZ(Debris_Model(d), -64.0f, 0.0f);
	float delta = FXC_Delta();
	self->t = self->t + delta;
	self->emitT = self->emitT + delta;
	Obj_Vtbl(self->teleport)->GetViewPos(self->teleport, &c);
	float r = Obj_Vtbl(self->character)->GetRadius(self->character);
	/* f: the character comes in (mode 1) or goes out (mode 2); g: the effect of its model over the duration */
	float q = 0.0f;
	if (self->mode == 1) {
		q = self->timeIn > self->t ? self->t / self->timeIn : 1.0f;
	} else if (self->mode == 2) {
		float b = self->duration - self->timeOut;
		q = self->t > b ? (self->timeOut - (self->t - b)) / self->timeOut : 1.0f;
	}
	float f = 0.0f > q ? 0.0f : Float_MinSS(1.0f, q);
	float g = (self->t * 1.2f - 0.2f) / self->duration;
	g = 0.0f > g ? 0.0f : Float_MinSS(1.0f, g);
	CGameModel *m = Obj_Vtbl(self->character)->GetModel(self->character);
	CHECK(CGameModel_IsLoaded(m));
	CGameModel_SetSortZ(m, -63.9f, 0.0f);
	CGameModel_SetEffect(m, 2);
	float e = self->mode == 2 ? 1.0f - g : g;
	memcpy(&bits, &e, sizeof(bits));
	CGameModel_SetEffectParam(m, 0, bits);
	CGameModel_GetPos(m, &p);
	/* the particles sit a bit towards the camera */
	cam = FXC_CamPos();
	diff.x = cam.x - c.x;
	diff.y = cam.y - c.y;
	diff.z = cam.z - c.z;
	Vec3_NormalizeTo(&n, &diff);
	float ox = n.x * r * 0.45f, oy = n.y * r * 0.45f, oz = n.z * r * 0.45f;
	/* the column of light of the teleport */
	int a = 0x80 - (int)((f - 1.0f) * 128.0f);
	if (a < 0x80) {
		a = 0x80;
	} else if (a > 0xff) {
		a = 0xff;
	}
	for (int i = 0; i < 5; i++) {
		uint32_t div = (i > 0 && i < 4) ? 1 : 2;
		uint32_t gl = 0x20 / div;
		uint32_t color = (((uint32_t)a << 8 | gl) << 8 | gl) << 8 | (0xff / div);
		CGameParticles_SetParticle(&self->particles, i, c.x + 0.0f, (float)i * 100.0f * 1.4f + c.y, c.z + 0.0f,
			800.0f, color, 2, 0.0f);
	}
	if (!(self->emitT > 0.025f)) {
		return;
	}
	if (!(self->duration - (self->timeOut + self->timeOut) > self->t)) {
		return;
	}
	/* the glows on the vertices at the height that moves over the character */
	float h = ((self->t - self->timeIn) + (self->t - self->timeIn)) / ((self->duration - self->timeIn) - self->timeOut)
		- 1.0f;
	h = -1.0f > h ? -1.0f : Float_MinSS(1.0f, h);
	if (self->mode == 2) {
		h = -h;
	}
	float s = r * 2.5f * h;
	CGameParticles *sp = &self->particles;
	CGameModel_RandomVertices(m, verts, NULL, NULL, FXTELEPORT_VERTS, 0xffffffffu);
	int cnt = 0;
	for (int j = 0; j < FXTELEPORT_VERTS; j++) {
		const vec3_t *v = &verts[j];
		if (__builtin_fabs((double)((p.y + s) - v->y)) > 20.0) {
			continue;
		}
		if (cnt++ > FXTELEPORT_GLOWS) {
			break;
		}
		FXTeleport_Glow(sp, v->x + ox, v->y + oy, v->z + oz, r);
	}
	/* too few vertices there: random points around the height */
	if ((uint32_t)(cnt - 1) <= 6) {
		for (int k = 0; k < crt_rand() % 8 + 8; k++) {
			float rz = FXC_Rand(0.20000000298023224, -0.10000000149011612);
			float ry = FXC_Rand(0.10000000149011612, -0.05000000074505806);
			float rx = FXC_Rand(0.20000000298023224, -0.10000000149011612);
			vec3_t *v = &verts[k];
			v->x = rx * r + p.x;
			v->y = ry * r + (p.y + s);
			v->z = rz * r + p.z;
			FXTeleport_Glow(sp, v->x + ox, oy + v->y, oz + v->z, r);
		}
	}
	/* the sparks and the smoke of the column */
	FXTeleport_Spark(sp, &c, self->mode, 100.0f, 400.0f, 700.0, 250.0, 250.0f, 1.5f, -1);
	FXTeleport_Spark(sp, &c, self->mode, 0.0f, 300.0f, 600.0, 350.0, 200.0f, 2.5f, 0x46);
	vec3_t dir;
	FX_RandomDir(&dir);
	CGameParticles_SetSparksOn(sp, 0);
	sp->birthRate = 0;
	float k = FXC_Rand(90.0, 10.0);
	float dx = dir.x * k, dy = dir.y * k, dz = dir.z * k;
	float ry = FXC_Rand(250.0, -50.0);
	float vy;
	if (self->mode == 1) {
		sp->initPosMin.x = (c.x + 0.0f) + dx;
		sp->initPosMin.y = (c.y + ry) + dy;
		sp->initPosMin.z = (c.z + 0.0f) + dz;
		sp->initPosMax = sp->initPosMin;
		vy = FXC_Rand(150.0, 150.0);
	} else {
		sp->initPosMin.x = ((c.x + 0.0f) + 0.0f) + dx;
		sp->initPosMin.y = ((c.y + ry) + 150.0f) + dy;
		sp->initPosMin.z = ((c.z + 0.0f) + 0.0f) + dz;
		sp->initPosMax = sp->initPosMin;
		vy = -FXC_Rand(150.0, 150.0);
	}
	FXC_VelY(sp, vy, 0.0f);
	sp->rotVar = 0.0f;
	sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
	sp->sizeLife = 0.2f;
	sp->lifeTime = FXC_Rand(0.19999998807907104, 0.60000002384185791);
	sp->sizeIni = FXC_Rand(5.0, 30.0);
	sp->sizeEnd = FXC_Rand(5.0, 70.0);
	byte sprite = (crt_rand() & 1) ? 9 : 15;
	sp->spriteIni = sprite;
	sp->spriteEnd = sprite;
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FXC_Ramp(FXC_TELEPORT_RAMPS, crt_rand() & 1));
	CGameParticles_EmitN(sp, 1, 0);
	/* smoke out of 8 vertices of the character */
	CHECK(CGameModel_IsLoaded(m));
	CGameModel_RandomVertices(m, verts, NULL, NULL, 8, 0xffffffffu);
	for (int i = 0; i < 8; i++) {
		const vec3_t *v = &verts[i];
		if (self->mode == 1) {
			diff.x = v->x - c.x;
			diff.y = v->y - c.y;
			diff.z = v->z - c.z;
			n = FX_Unit(diff);
			sp->birthRate = 0;
			sp->initPosMin.x = (v->x + ox) + n.x * 2.0f;
			sp->initPosMin.y = (v->y + oy) + n.y * 2.0f;
			sp->initPosMin.z = (v->z + oz) + n.z * 2.0f;
			sp->initPosMax = sp->initPosMin;
			float up = FXC_Rand(100.0, 200.0);
			float kv = FXC_Rand(10.0, 80.0);
			sp->initVelMin.x = n.x * kv + 0.0f;
			sp->initVelMin.y = n.y * kv + up;
			sp->initVelMin.z = n.z * kv + 0.0f;
			sp->initVelMax = sp->initVelMin;
			float up2 = FXC_Rand(100.0, 200.0);
			float ka = FXC_Rand(0.099999964237213135, 0.80000001192092896);
			sp->initAccMin.x = ka * sp->initVelMin.x + 0.0f;
			sp->initAccMin.y = ka * sp->initVelMin.y + up2;
			sp->initAccMin.z = ka * sp->initVelMin.z + 0.0f;
			sp->initAccMax = sp->initAccMin;
			FXTeleport_Smoke(sp, -1.0f);
		} else {
			vec3_t rd;
			FX_RandomDir(&rd);
			diff.x = v->x - c.x;
			diff.y = v->y - c.y;
			diff.z = v->z - c.z;
			n = FX_Unit(diff);
			float lt = FXC_Rand(0.099999994039535522, 0.34999999403953552);
			sp->birthRate = 0;
			float rx = rd.x * 100.0f * 0.5f, rry = rd.y * 100.0f * 0.5f, rz = rd.z * 100.0f * 0.5f;
			float up = FXC_Rand(150.0, 150.0);
			sp->initPosMin.x = ((c.x + ox) + 0.0f) + rx;
			sp->initPosMin.z = ((c.z + oz) + 0.0f) + rz;
			sp->initPosMin.y = (up + (oy + c.y)) + rry;
			sp->initPosMax = sp->initPosMin;
			float nx = n.x * 2.0f, ny = n.y * 2.0f, nz = n.z * 2.0f;
			float inv = 1.0f / lt;
			sp->initVelMin.x = ((nx + v->x) - sp->initPosMin.x) * inv;
			sp->initVelMin.y = ((ny + v->y) - sp->initPosMin.y) * inv;
			sp->initVelMin.z = ((nz + v->z) - sp->initPosMin.z) * inv;
			sp->initVelMax = sp->initVelMin;
			sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
			sp->initAccMax = sp->initAccMin;
			FXTeleport_Smoke(sp, lt);
			/* and a flash where it arrives */
			sp->birthRate = 0;
			FXC_Still(sp, nx + v->x, ny + v->y, nz + v->z);
			sp->rotVar = 0.0f;
			sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
			sp->lifeTime = FXC_Rand(0.030000008642673492, 0.11999999731779099);
			sp->sizeIni = FXC_Rand(15.0, 50.0);
			sp->sizeEnd = FXC_Rand(15.0, 100.0);
			sp->sizeLife = 0.1f;
			sp->spriteIni = 5;
			sp->spriteEnd = 5;
			CGameParticles_SetColorRampOn(sp, 1);
			CGameParticles_SetColorRamp(sp, GAME_PTR(FXC_TELEPORT_RAMPS, 72, FXC_RAMP_TELEPORT_GLOW));
			CGameParticles_Apply(sp);
			CGameParticles_EmitDt(sp, 1, 0, lt);
		}
	}
	self->emitT = 0.0f;
}
#define FXC_RAMP_DEATH 0x430288		/* 4 color ramps of 9 colors */
#define FXDEATH_VERTS 0x60

static inline float FXC_Smooth(float k) {
	return (3.0f - (k + k)) * (k * k);
}

/* the dust of the death at a vertex v (moved by o) of the dead character: up or down by the height band */
static void FXDeath_Dust(CGameParticles *sp, const vec3_t *v, float ox, float oy, float oz, double kRange,
	double kAdd, float velK, float q) {
	vec3_t dir;

	sp->birthRate = 0;
	float k = FXC_Rand(kRange, kAdd);
	FX_RandomDir(&dir);
	sp->initPosMin.x = (v->x + ox) + k * dir.x;
	sp->initPosMin.y = (v->y + oy) + dir.y * k;
	sp->initPosMin.z = (v->z + oz) + dir.z * k;
	sp->initPosMax = sp->initPosMin;
	float vy;
	if ((int)(sp->initPosMin.y / 10.0f) & 1) {
		vy = velK < 0.0f ? -(FXC_Rand(25.0, 300.0) * q) : FXC_Rand(25.0, 300.0) * -velK * q;
	} else {
		vy = velK < 0.0f ? FXC_Rand(25.0, 300.0) * q : FXC_Rand(25.0, 300.0) * velK * q;
	}
	sp->initVelMin = (vec3_t){ 0.0f, vy, 0.0f };
	sp->initVelMax = sp->initVelMin;
	float k2 = FXC_Rand(0.019999980926513672, 1.6000000238418579);
	sp->initAccMin.x = k2 * -sp->initVelMin.x;
	sp->initAccMin.y = k2 * -sp->initVelMin.y;
	sp->initAccMin.z = k2 * -sp->initVelMin.z;
	sp->initAccMax = sp->initAccMin;
	sp->lifeTime = FXC_Rand(0.030000030994415283, 0.81999999284744263);
	sp->sizeLife = 1.0f;
}

/* a glow at the hand of the dead character */
static void FXDeath_Glow(CGameParticles *sp, const vec3_t *v, float r, double iniRange, double iniAdd, double endRange,
	double endAdd, byte sprite) {
	FXC_Still(sp, v->x, v->y, v->z);
	sp->lifeTime = FXC_Rand(0.019999995827674866, 0.2199999988079071);
	sp->sizeIni = FXC_Rand(iniRange, iniAdd) * r;
	sp->sizeLife = 0.25f;
	sp->sizeEnd = FXC_Rand(endRange, endAdd) * r;
	sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
	sp->rotVar = 0.0f;
	sp->spriteIni = sprite;
	sp->spriteEnd = sprite;
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FXC_Ramp(GAME_ARRAY(uint32_t, FXDeath_Glow_430288, FXC_RAMP_DEATH), 0));
	CGameParticles_EmitN(sp, 1, 0);
}

void __thiscall cObjElementFXCharacterDeath_Update(cObjElementFXCharacterDeath *self, cDebris *d, float dt) {
	vec3_t p, dir, verts[FXDEATH_VERTS];
	float a;
	uint32_t bits;

	(void)dt;
	self->ent = FXC_Keep(self->ent);
	if (!self->ent || !self->ent->flag_4c) {
		return;
	}
	float delta = FXC_Delta();
	float t = self->time + delta;
	float et = self->emitT + delta;
	self->time = t;
	self->emitT = et;
	/* the character slows down over the first 0.7 s */
	FX_InRange(t, 0.0f, 0.7f, &a);
	float k = 1.0f - a;
	if (self->ent) {
		cTimer_SetSpeedChecked(Entity_Timer(self->ent), FXC_Smooth(k));
	}
	CGameModel *m = Obj_Vtbl(self->ent)->GetModel(self->ent);
	CHECK(CGameModel_IsLoaded(m));
	CGameModel_GetPos(m, &p);
	CGameModel_SetPos(Debris_Model(d), &p);
	CGameModel_SetSortZ(Debris_Model(d), -61.0f, 0.0f);
	/* the model fades out from 0.75 s on, the effect shows while the character does */
	if (FX_InRange(self->time, 0.75f, 1.0f, &a)) {
		CGameModel_SetSortZ(m, -60.9f, 0.0f);
		CGameModel_SetEffect(m, 2);
		float e = (a * a) * (2.0f - a);
		memcpy(&bits, &e, sizeof(bits));
		CGameModel_SetEffectParam(m, 0, bits);
	} else if (a >= 1.0f) {
		CGameModel_Disable(m);
	} else if (CGameModel_IsVisible(m)) {
		CGameModel_Enable(Debris_Model(d));
	} else {
		CGameModel_Disable(Debris_Model(d));
	}
	byte vis = CGameModel_IsVisible(Debris_Model(d));
	vec3_t cam = FXC_CamPos();
	vec3_t n = FX_Unit((vec3_t){ cam.x - p.x, cam.y - p.y, cam.z - p.z });
	float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
	float ox = n.x * r * 0.6f, oy = n.y * r * 0.6f, oz = n.z * r * 0.6f;

	/* the trails up from the vertices */
	if (!FX_InRange(self->time, 0.55f, 1.6f, &a)) {
		if (CGameTrail_IsRender(&self->trails[0])) {
			for (int i = 0; i < FXDEATH_TRAILS; i++) {
				CGameTrail_Clear(&self->trails[i]);
			}
			CGameParticles_KillAll(&self->particles);
		}
	} else {
		float q = 0.0f;
		if (FX_InRange(self->time, 0.55f, 0.75f, &q)) {
			q = 1.0f - (q * q) * (2.0f - q);
		} else if (FX_InRange(self->time, 1.0f, 1.5f, &q)) {
			q = (3.0f - q * 2.0f) * (q * q);
		}
		CGameParticles_SetAlpha(&self->particles, 1.0f - q);
		for (int i = 0; i < FXDEATH_TRAILS; i++) {
			CGameTrail *tr = &self->trails[i];
			CGameTrail_Reset(tr);
			CGameTrail_SetTransp(tr, (byte)(int)(q * 255.0f));
			CGameTrail_SetUV(tr, (i & 1) ? 0.5f : 0.0f, (i & 2) ? 0.5f : 0.0f);
			CGameTrail_SetULen(tr, 0.15f, 0.25f);
		}
		if (vis) {
			int pt = 0, trail = 0;
			for (int i = 0; i < FXDEATH_POINTS; i++) {
				float ph = self->phase[i];
				if (ph > 1.0f && 1.6f > self->time) {
					/* a new point on a random vertex */
					if (ph == 5.0f) {
						self->phase[i] = (float)(FXC_Rnd() - 1.0);
					} else {
						self->phase[i] = 0.0f;
					}
					vec3_t *pp = &self->pos[i];
					CGameModel_Vertices(m, pp, NULL, NULL, 1, -1, NULL, 0xffffffffu, NULL);
					float kp = FXC_Rand(0.049999997019767761, 0.029999999329447746);
					FX_RandomDir(&dir);
					float dx = dir.x * r, dy = dir.y * r, dz = dir.z * r;
					pp->x = pp->x + kp * dx;
					pp->y = pp->y + kp * dy;
					pp->z = pp->z + kp * dz;
					float w = FXC_Rand(0.40000003576278687, 0.20000000298023224);
					self->width[i] = w;
					if (self->time > 1.0f) {
						self->width[i] = (self->time * self->time) * w;
					}
					uint32_t g = (uint32_t)(crt_rand() % 40 + 0x8c) << 8;
					uint32_t rg = (uint32_t)(crt_rand() % 55 + 0xc8) << 16 | g;
					self->color[i] = (uint32_t)(crt_rand() % 26 + 0x3c) | rg;
					self->speed[i] = FXC_Rand(1.2999999523162842, 2.7999999523162842);
				}
				float v = self->speed[i] * delta + self->phase[i];
				self->phase[i] = v;
				if (0.0f > v || v > 1.0f) {
					continue;
				}
				int alpha = 0;
				if (0.1f > v) {
					alpha = (int)((v / 0.1f - 1.0f) * -255.0f);
				} else if (v > 0.5f) {
					alpha = (int)(((v - 0.5f) * 2.0f) * 255.0f);
				}
				float v4 = v * v * v * v;
				float s = (v4 + v4) + 1.0f;
				float h = s * r * self->width[i];
				if (alpha < 0xff) {
					const vec3_t *pp = &self->pos[i];
					CGameTrail *tr = &self->trails[trail];
					uint32_t col = (self->color[i] & 0xffffff) | (uint32_t)alpha << 24;
					float wd = r * 0.2f;
					vec3_t q1 = { (pp->x + ox) - 0.0f, (pp->y + oy) - h, (pp->z + oz) - 0.0f };
					CGameTrail_Add(tr, q1, wd, col);
					vec3_t q2 = { (pp->x + ox) + 0.0f, (pp->y + oy) + h, (pp->z + oz) + 0.0f };
					CGameTrail_Add(tr, q2, wd, col);
					float kk = FXC_Rand(0.099999964237213135, 0.85000002384185791);
					CGameParticles_SetParticle(&self->particles, i, pp->x + ox, pp->y + oy, pp->z + oz,
						((s * self->width[i]) * r) * kk, col, 5, 0.0f);
				}
				if (++pt >= FXDEATH_TRAILPOINTS) {
					pt = 0;
					trail++;
				}
			}
		}
	}

	/* the dust when the model is gone */
	if (self->time >= 1.0f && 1.0f >= self->lastTime && vis) {
		CGameModel_Vertices(m, verts, NULL, NULL, FXDEATH_VERTS, -1, NULL, 0xffffffffu, NULL);
		float q = r / 60.0f;
		for (int j = 0; j < FXDEATH_VERTS; j++) {
			const vec3_t *v = &verts[j];
			CGameParticles *sp = &self->particles3;
			FXDeath_Dust(sp, v, ox, oy, oz, 5.0, 1.0, -1.0f, q);
			sp->sizeIni = sp->sizeEnd = FXC_Rand(0.34000000357627869, 0.34000000357627869) * r * 0.5f;
			sp->rotIni = sp->rotEnd = (float)(crt_rand() % 4) * 1.57079637f;
			sp->rotVar = 0.0f;
			byte sprite = (byte)(crt_rand() % 4 + 1);
			sp->spriteIni = sprite;
			sp->spriteEnd = sprite;
			CGameParticles_Apply(sp);
			CGameParticles_SetColorRampOn(sp, 1);
			CGameParticles_SetColorRamp(sp, FXC_Ramp(GAME_ARRAY(uint32_t, FXDeath_Glow_430288, FXC_RAMP_DEATH), crt_rand() & 1));
			CGameParticles_EmitN(sp, 1, 0);
			sp = &self->particles2;
			FXDeath_Dust(sp, v, ox, oy, oz, 4.0, 0.0, 0.95f, q);
			sp->sizeIni = sp->sizeEnd = FXC_Rand(0.2199999988079071, 0.25999999046325684) * r * 0.85f;
			sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
			sp->spriteIni = 5;
			sp->spriteEnd = 5;
			sp->rotVar = FXC_Rand(1.5707963705062866, -0.78539818525314331);
			CGameParticles_Apply(sp);
			CGameParticles_SetColorRampOn(sp, 1);
			CGameParticles_SetColorRamp(sp, FXC_Ramp(GAME_ARRAY(uint32_t, FXDeath_Glow_430288, FXC_RAMP_DEATH), 0));
			CGameParticles_EmitN(sp, 1, 0);
		}
	}
	float tt = self->time;
	self->lastTime = tt;
	if (0.025f > self->emitT) {
		return;
	}
	self->emitT = 0.0f;
	if (!vis) {
		return;
	}
	CGameParticles *sp = &self->particles2;
	/* the smoke from the vertices */
	if (FX_InRange(tt, 0.15f, 0.55f, &a)) {
		CGameModel_Vertices(m, verts, NULL, NULL, 0x18, -1, NULL, 0xffffffffu, NULL);
		ox = ox * 0.5f;
		oy = oy * 0.5f;
		oz = oz * 0.5f;
		sp->birthRate = 0;
		for (int j = 0; j < 0x18; j++) {
			const vec3_t *v = &verts[j];
			FXC_Still(sp, v->x + ox, v->y + oy, v->z + oz);
			sp->lifeTime = FXC_Rand(0.02000001072883606, 0.12999999523162842);
			sp->sizeIni = FXC_Rand(0.0099999904632568359, 0.20000000298023224) * r;
			sp->sizeLife = 0.2f;
			sp->sizeEnd = FXC_Rand(0.01000000536441803, 0.2199999988079071) * r;
			sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
			sp->rotVar = FXC_Rand(1.5707963705062866, -0.78539818525314331);
			sp->spriteIni = (byte)(crt_rand() % 4 + 0x25);
			sp->spriteEnd = 0x28;
			CGameParticles_Apply(sp);
			CGameParticles_SetColorRampOn(sp, 1);
			CGameParticles_SetColorRamp(sp, FXC_Ramp(GAME_ARRAY(uint32_t, FXDeath_Glow_430288, FXC_RAMP_DEATH), crt_rand() % 4));
			CGameParticles_EmitN(sp, 1, 0);
		}
	}
	/* now and then a flash with sparks between two vertices */
	if (!(crt_rand() & 1)) {
		return;
	}
	if (!FX_InRange(self->time, 0.1f, 0.5f, &a)) {
		return;
	}
	CGameModel_Vertices(m, verts, NULL, NULL, 2, -1, NULL, 0xffffffffu, NULL);
	sp->birthRate = 0;
	FXDeath_Glow(sp, &verts[0], r, 0.020000003278255463, 0.11999999731779099, 0.060000002384185791, 0.2800000011920929,
		2);
	FXDeath_Glow(sp, &verts[1], r, 0.099999994039535522, 0.14000000059604645, 0.030000001192092896,
		0.44999998807907104, 5);
	CGameParticles_SetSparksOn(sp, 1);
	vec3_t nd = FX_Unit((vec3_t){ verts[0].x - p.x, verts[0].y - p.y, verts[0].z - p.z });
	for (int j = 0; j < crt_rand() % 8 + 8; j++) {
		vec3_t ang, tdir;
		sp->initPosMin = verts[0];
		sp->initPosMax = sp->initPosMin;
		float w = FXC_Rand(215.0, 135.0);
		float a1 = FXC_Rand(1.0471975803375244, -0.52359879016876221);
		float a2 = FXC_Rand(1.0471975803375244, -0.52359879016876221);
		ang.x = FXC_Rand(1.0471975803375244, -0.52359879016876221);
		ang.y = a2;
		ang.z = a1;
		Vec3_Rotate(&tdir, &nd, &ang);
		float tx = w * tdir.x, ty = tdir.y * w, tz = tdir.z * w;
		float kv = FXC_Rand(75.0, 10.0);
		FX_RandomDir(&dir);
		sp->initVelMin.x = kv * dir.x + tx;
		sp->initVelMin.y = dir.y * kv + ty;
		sp->initVelMin.z = dir.z * kv + tz;
		sp->initVelMax = sp->initVelMin;
		float ka = FXC_Rand(0.099999994039535522, 0.30000001192092896);
		sp->initAccMin.x = ka * -sp->initVelMin.x + 0.0f;
		sp->initAccMin.y = ka * -sp->initVelMin.y - 280.0f;
		sp->initAccMin.z = ka * -sp->initVelMin.z + 0.0f;
		sp->initAccMax = sp->initAccMin;
		sp->lifeTime = FXC_Rand(0.069999992847442627, 0.37999999523162842);
		sp->sizeLife = 1.0f;
		sp->rotIni = 0.0f;
		sp->rotEnd = 0.0f;
		sp->rotVar = 0.0f;
		sp->sizeIni = sp->sizeEnd = FXC_Rand(0.060000002384185791, 0.12999999523162842) * r;
		sp->spriteIni = (byte)((crt_rand() & 1) + 0x41);
		sp->spriteEnd = 0x44;
		sp->sparkMaxSize = 1.0f;
		sp->sparkVelScale = 10.0f;
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, FXC_Ramp(GAME_ARRAY(uint32_t, FXDeath_Glow_430288, FXC_RAMP_DEATH), 0));
		CGameParticles_EmitDt(sp, 1, 0, FXC_Rand(0.039999999105930328, 0.0));
	}
	CGameParticles_SetSparksOn(sp, 0);
}
#define FXC_RAMP_CONVERSOR 0x4304b0	/* 3 color ramps of 9 colors */
#define FXCONVERSOR_VERTS 0x60

/* lo when v is smaller, at most 255 */
static inline int FXC_Clamp(int v, int lo) {
	if (v < lo) {
		return lo;
	}
	return v > 0xff ? 0xff : v;
}

/* the two glows of the conversor on the character at x y z, their life limited by the debris */
static void FXConversor_Glow(CGameParticles *sp, cDebris *d, float x, float y, float z, float r) {
	CGameParticles_SetSparksOn(sp, 0);
	sp->birthRate = 0;
	FXC_Still(sp, x, y, z);
	sp->rotVar = 0.0f;
	sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
	float lt = FXC_Rand(0.019999995827674866, 0.18000000715255737);
	sp->sizeLife = 1.0f;
	sp->lifeTime = Float_MinSS(Debris_Life(d) * 0.9f, lt);
	sp->spriteIni = 9;
	sp->spriteEnd = 9;
	sp->sizeIni = sp->sizeEnd = FXC_Rand(0.099999904632568359, 1.2000000476837158) * r;
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FXC_Ramp(GAME_ARRAY(uint32_t, FXConversor_Glow_4304b0, FXC_RAMP_CONVERSOR), crt_rand() % 3));
	CGameParticles_EmitN(sp, 1, 0);
	CGameParticles_SetSparksOn(sp, 0);
	sp->birthRate = 0;
	FXC_Still(sp, x, y, z);
	sp->rotVar = 0.0f;
	sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
	lt = FXC_Rand(0.049999982118606567, 0.30000001192092896);
	sp->sizeLife = 1.0f;
	sp->lifeTime = Float_MinSS(Debris_Life(d) * 0.9f, lt);
	sp->spriteIni = 0x2b;
	sp->spriteEnd = 0x2b;
	sp->sizeIni = sp->sizeEnd = FXC_Rand(0.10000002384185791, 1.0) * r;
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FXC_Ramp(GAME_ARRAY(uint32_t, FXConversor_Glow_4304b0, FXC_RAMP_CONVERSOR), crt_rand() % 3));
	CGameParticles_EmitDt(sp, 1, 0, FXC_Rand(0.23999999463558197, 0.0099999997764825821));
}

/* the direction of the exit node of the conversor (y of its matrix, turned by the conversor) and its position */
static void FXConversor_Exit(CGameModel *mc, int node, const vec3_t *rot, vec3_t *dir, vec3_t *p) {
	matrix_t mat;
	vec3_t eu, o1;
	const vec3_t up = { 0.0f, 1.0f, 0.0f };

	CGameModel_NodeMatrix(mc, &mat, (uint32_t)node, 1, NULL, 0);
	p->x = mat.m[12];
	p->y = mat.m[13];
	p->z = mat.m[14];
	Matrix_GetEuler(&mat, &eu);
	Vec3_Rotate(dir, Vec3_Rotate(&o1, &up, rot), &eu);
}

void __thiscall cObjElementFXCharacterConversor_Update(cObjElementFXCharacterConversor *self, cDebris *d, float dt) {
	vec3_t c, cc, rot, eu, o1, o2, p, pt, dir, verts[FXCONVERSOR_VERTS], coefs[FXCONVERSOR_POINTS];
	matrix_t mat;
	uint32_t bits;
	float ct;

	(void)dt;
	cEntity *ch = self->character;
	if (ch && (Entity_Removed(ch) || !ch->flag_4c)) {
		/* the character went: its model back to normal */
		CGameModel *m = Obj_Vtbl(ch)->GetModel(ch);
		if (!m || !CGameModel_IsLoaded(m)) {
			self->character = NULL;
			return;
		}
		CHECK(CGameModel_IsLoaded(m));
		if (self->phase == 1) {
			CGameModel_SetEffect(m, 0);
			CGameModel_SetSortZ(m, 0.0f, 1.0f);
			CGameModel_Disable(m);
		} else if (self->phase == 2) {
			CGameModel_SetEffect(m, 0);
			CGameModel_SetSortZ(m, 0.0f, 1.0f);
			CGameModel_Enable(m);
		}
		self->character = NULL;
	}
	if (!self->character || !self->character->flag_4c) {
		return;
	}
	self->conversor = FXC_Keep(self->conversor);
	vec3_t cam = FXC_CamPos();
	CGameModel_SetPos(Debris_Model(d), &cam);
	CGameModel_SetSortZ(Debris_Model(d), -62.0f, 0.0f);
	float delta = FXC_Delta();
	float et = self->emitT + delta;
	float t = self->time + delta;
	self->emitT = et;
	self->time = t;
	/* f: the fade of the character, h: the height of the glows, g: the effect of its model */
	float q = 0.0f;
	if (self->phase == 1) {
		q = self->timeIn > t ? t / self->timeIn : 1.0f;
	} else if (self->phase == 2) {
		float b = self->duration - self->timeOut;
		q = t > b ? (self->timeOut - (t - b)) / self->timeOut : 1.0f;
	}
	float f = 0.0f > q ? 0.0f : Float_MinSS(1.0f, q);
	float r = Obj_Vtbl(self->character)->GetRadius(self->character);
	t = self->time;
	float h = ((t - self->timeIn) + (t - self->timeIn)) / ((self->duration - self->timeIn) - self->timeOut) - 1.0f;
	h = -1.0f > h ? -1.0f : Float_MinSS(1.0f, h);
	if (self->phase == 2) {
		h = -h;
	}
	float g = 0.0f;	/* not set in the original for the other phases */
	if (self->phase == 1) {
		g = ((t - self->timeIn) - self->timeIn) / (self->duration - self->timeIn * 5.0f);
	} else if (self->phase == 2) {
		g = ((self->duration - self->timeOut) - t) / (self->duration - self->timeOut * 5.0f);
	}
	g = 0.0f > g ? 0.0f : Float_MinSS(1.0f, g);
	CGameModel *m = Obj_Vtbl(self->character)->GetModel(self->character);
	CHECK(CGameModel_IsLoaded(m));
	if (m) {
		if (!CGameModel_IsEnabled(m)) {
			CGameModel_Enable(m);
		}
		CGameModel_SetSortZ(m, -61.9f, 0.0f);
		CGameModel_SetEffect(m, 2);
		memcpy(&bits, &g, sizeof(bits));
		CGameModel_SetEffectParam(m, 0, bits);
	}
	CGameParticles *sp = &self->particles;
	if (self->conversor) {
		/* the end of the animation of the conversor ends the effect */
		CGameModel *cm = Obj_Vtbl(self->conversor)->GetModel(self->conversor);
		CHECK(CGameModel_IsLoaded(cm));
		if (CGameModel_IsVisible(cm)) {
			CGameModel_Enable(Debris_Model(d));
		} else {
			CGameModel_Disable(Debris_Model(d));
		}
		CGameModel_ChanTime(cm, 0, &ct);
		if ((self->phase == 1 && ct >= 1.7f) || (self->phase == 2 && ct >= 1.0f && 1.7f >= ct)) {
			CHECK(sp);
			CHECK(sp->ParticleSystem);
			CGameParticles_KillAll(sp);
			for (int i = 0; i < self->numTrails; i++) {
				CGameTrail_Clear(&self->trails[i]);
				self->trailT[i] = 0.0f;
			}
			return;
		}
	}
	Obj_Vtbl(self->character)->GetViewPos(self->character, &c);
	if (self->conversor) {
		CGameModel *mc = Obj_Vtbl(self->conversor)->GetModel(self->conversor);
		CHECK(CGameModel_IsLoaded(mc));
		CGameModel_GetRot(mc, &rot);
		Obj_Vtbl(self->conversor)->GetViewPos(self->conversor, &cc);
		const vec3_t ang = { 0.0f, rot.y, 0.0f };
		/* the column of the conversor */
		float f1 = f - 1.0f;
		uint32_t col = (uint32_t)FXC_Clamp(0xc4 - (int)(f1 * 64.0f), 0xc4) << 24 | 0x4040ff;
		for (int i = 0, hh = 0; hh < 500; i++, hh += 125) {
			const vec3_t v = { 0.0f, -25.0f, -100.0f };
			float y = (float)hh * 0.4f;
			const vec3_t *o = Vec3_Rotate(&o1, &v, &ang);
			CGameParticles_SetParticle(sp, i, (o->x + cc.x) + 0.0f, y + (o->y + cc.y), (o->z + cc.z) + 0.0f, 500.0f, col,
				2, 0.0f);
		}
		int k1 = FXC_Clamp(0x20 - (int)(f1 * 220.0f), 0x20);
		int k2 = FXC_Clamp(0x80 - (int)(f1 * 128.0f), 0x80);
		/* the lights */
		for (int j = 0, idx = 0x17; idx < 0x1f; j++, idx += 2) {
			int node = self->nodeLight[j];
			if (node == -1) {
				continue;
			}
			const vec3_t v = { 0.0f, 0.0f, 4.0f };
			CGameModel_NodeMatrix(mc, &mat, (uint32_t)node, 1, NULL, 0);
			float px = mat.m[12], py = mat.m[13], pz = mat.m[14];
			Matrix_GetEuler(&mat, &eu);
			Vec3_Rotate(&o2, Vec3_Rotate(&o1, &v, &eu), &ang);
			float x = o2.x + px, y = o2.y + py, z = o2.z + pz;
			CGameParticles_SetParticle(sp, idx - 1, x, y, z, 30.0f, (uint32_t)k1 << 24 | 0xffc8c8, 2, 0.0f);
			CGameParticles_SetParticle(sp, idx, x, y, z, 150.0f, (uint32_t)k2 << 24 | 0xff0000, 5, 0.0f);
		}
		/* the walls and the exits */
		for (int j = 0, idx = 5; idx < 0xb; j++, idx += 2) {
			int node = self->nodeWall[j];
			if (node != -1) {
				const vec3_t v = { 0.0f, 0.0f, -8.0f };
				CGameModel_NodeMatrix(mc, &mat, (uint32_t)node, 1, NULL, 0);
				float px = mat.m[12], py = mat.m[13], pz = mat.m[14];
				Vec3_Rotate(&o2, &v, &ang);
				float x = o2.x + px, y = o2.y + py, z = o2.z + pz;
				CGameParticles_SetParticle(sp, idx - 1, x, y, z, 40.0f, (uint32_t)k1 << 24 | 0xc8c8ff, 2, 0.0f);
				CGameParticles_SetParticle(sp, idx, x, y, z, 200.0f, (uint32_t)k2 << 24 | 0x32ff, 5, 0.0f);
			}
			node = self->nodeExit[j];
			if (node != -1) {
				FXConversor_Exit(mc, node, &rot, &o2, &p);
				CGameParticles_SetParticle(sp, idx + 5, o2.x * 5.0f + p.x, o2.y * 5.0f + p.y, o2.z * 5.0f + p.z, 35.0f,
					(uint32_t)k1 << 24 | 0x96c8ff, 2, 0.0f);
				CGameParticles_SetParticle(sp, idx + 6, o2.x * 5.0f + p.x, o2.y * 5.0f + p.y, o2.z * 5.0f + p.z,
					120.0f, (uint32_t)k2 << 24 | 0x1432ff, 5, 0.0f);
			}
		}
		/* the lightnings from the walls to the exits */
		for (int j = 0; j < self->numTrails; j++) {
			int idx = 0x11 + 2 * j;
			CGameTrail *tr = &self->trails[j];
			vec3_t *pts = self->pts[j];
			float tt = self->trailT[j] - delta;
			self->trailT[j] = tt;
			int a = 0;
			if (0.02f > tt) {
				a = (int)((1.0f - (tt - delta) / 0.02f) * 255.0f);
				if (a < 0) {
					a = 0;
				} else if (a > 0xff) {
					a = 0xff;
				}
			}
			CGameTrail_SetTransp(tr, (byte)a);
			if (self->trailT[j] > 0.0f) {
				int node = self->nodeWall[j];
				if (node != -1) {
					const vec3_t v = { 0.0f, 0.0f, -30.0f };
					const matrix_t *mp = CGameModel_NodeMatrix(mc, &mat, (uint32_t)node, 1, NULL, 0);
					pts[0].x = mp->m[12];
					pts[0].y = mp->m[13];
					pts[0].z = mp->m[14];
					const vec3_t *o = Vec3_Rotate(&o1, &v, &rot);
					pts[1].x = o->x + pts[0].x;
					pts[1].y = o->y + pts[0].y;
					pts[1].z = o->z + pts[0].z;
				}
				node = self->nodeExit[j];
				if (node != -1) {
					FXConversor_Exit(mc, node, &rot, &o2, &p);
					int n = self->numPoints;
					pts[n - 1].x = o2.x * -22.0f + p.x;
					pts[n - 1].y = o2.y * -22.0f + p.y;
					pts[n - 1].z = o2.z * -22.0f + p.z;
					pts[n - 2].x = o2.x * -45.0f + p.x;
					pts[n - 2].y = o2.y * -45.0f + p.y;
					pts[n - 2].z = o2.z * -45.0f + p.z;
					CGameParticles_SetParticle(sp, idx - 1, o2.x * -23.0f + p.x, o2.y * -23.0f + p.y,
						o2.z * -23.0f + p.z, 40.0f, FXC_Alpha(a, 0x20, 0xc8c8ff), 2, 0.0f);
					CGameParticles_SetParticle(sp, idx, o2.x * -24.0f + p.x, o2.y * -24.0f + p.y, o2.z * -24.0f + p.z,
						150.0f, FXC_Alpha(a, 0x40, 0x32ff), 5, 0.0f);
				}
				/* the points between shake */
				for (int k = 2; k < self->numPoints - 2; k++) {
					float w = FXC_Rand(250.0, 100.0);
					FX_RandomDir(&dir);
					float dx = w * dir.x, dy = dir.y * w, dz = dir.z * w;
					pts[k].x = pts[k].x + dx * delta;
					pts[k].y = dy * delta + pts[k].y;
					pts[k].z = dz * delta + pts[k].z;
				}
				int mx = CGameTrail_GetMaxTrails(tr);
				CGameTrail_BezierCoefs(tr, self->numPoints, pts, coefs);
				int cnt = mx - 1;
				if (cnt > 0) {
					float dm = (float)(mx - 2);
					vec3_t prev = { 0.0f, 0.0f, 0.0f }, cur;
					for (int i = 0; i < cnt; i++) {
						CGameTrail_BezierPoint(tr, self->numPoints, coefs, &pt, (float)i / dm);
						cur = pt;
						if (i == 0) {
							prev = pt;
						}
						float dx = prev.x - pt.x, dy = prev.y - pt.y, dz = prev.z - pt.z;
						float wd;
						if (i > 0 && i < mx - 3) {
							/* a zigzag by the length of the piece */
							double rr = FXC_Rnd();
							float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
							float jit = (float)(rr * (double)(len * 0.18f) + 0.0);
							FX_RandomDir(&dir);
							cur.x = jit * dir.x + cur.x;
							cur.y = dir.y * jit + cur.y;
							cur.z = dir.z * jit + cur.z;
							wd = FXC_Rand(20.0, 35.0);
						} else {
							wd = FXC_Rand(10.0, 85.0);
						}
						float gg = FXC_Rand(6.0, 18.0);
						float b1 = FXC_Rand(16.0, 32.0);
						float b2 = FXC_Rand(16.0, 32.0);
						uint32_t color = ((((uint32_t)(int)gg << 8 | (uint32_t)(int)b1) << 8 | (uint32_t)(int)b2) << 8) |
							0x5c;
						CGameTrail_Add(tr, cur, self->width[j] * wd, color);
						prev = cur;
					}
				}
			} else if (self->duration - self->timeOut > self->time) {
				/* a new lightning */
				self->trailT[j] = FXC_Rand(0.11000000685453415, 0.070000000298023224);
				self->width[j] = FXC_Rand(0.069999992847442627, 0.090000003576278687);
				int node = self->nodeWall[j];
				if (node != -1) {
					const matrix_t *mp = CGameModel_NodeMatrix(mc, &mat, (uint32_t)node, 1, NULL, 0);
					pts[0].x = mp->m[12];
					pts[0].y = mp->m[13];
					pts[0].z = mp->m[14];
				}
				node = self->nodeExit[j];
				int n = self->numPoints;
				if (node != -1) {
					FXConversor_Exit(mc, node, &rot, &o2, &p);
					pts[n - 1].x = o2.x * -22.0f + p.x;
					pts[n - 1].y = o2.y * -22.0f + p.y;
					pts[n - 1].z = o2.z * -22.0f + p.z;
					pts[n - 2].x = o2.x * -45.0f + p.x;
					pts[n - 2].y = o2.y * -45.0f + p.y;
					pts[n - 2].z = o2.z * -45.0f + p.z;
				}
				n = self->numPoints;
				float inv = 1.0f / (float)(n - 2);
				float dx = (pts[n - 2].x - pts[0].x) * inv;
				float dy = (pts[n - 2].y - pts[0].y) * inv;
				float dz = (pts[n - 2].z - pts[0].z) * inv;
				for (int k = 1; k < self->numPoints - 2; k++) {
					float fk = (float)k;
					pts[k].x = dx * fk + pts[0].x;
					pts[k].y = dy * fk + pts[0].y;
					pts[k].z = dz * fk + pts[0].z;
					float wv = FXC_Rand(5.0, 10.0);
					FX_RandomDir(&dir);
					pts[k].x = wv * dir.x + pts[k].x;
					pts[k].y = dir.y * wv + pts[k].y;
					pts[k].z = dir.z * wv + pts[k].z;
				}
			}
		}
	}
	/* the glows over the vertices of the character at the height that moves */
	if (!CGameModel_IsVisible(m)) {
		return;
	}
	if (!(self->emitT > 0.02f)) {
		return;
	}
	if (!(self->duration - self->timeOut > self->time)) {
		return;
	}
	float s = r * 2.5f * h;
	cam = FXC_CamPos();
	const vec3_t *mp = CGameModel_GetPos(m, &p);
	vec3_t n = FX_Unit((vec3_t){ cam.x - mp->x, cam.y - mp->y, cam.z - mp->z });
	float ox = n.x * r * 0.25f, oy = n.y * r * 0.25f, oz = n.z * r * 0.25f;
	CGameModel_Vertices(m, verts, NULL, NULL, FXCONVERSOR_VERTS, -1, NULL, 0xffffffffu, NULL);
	int cnt = 0;
	for (int j = 0; j < FXCONVERSOR_VERTS; j++) {
		const vec3_t *v = &verts[j];
		if (__builtin_fabs((double)((c.y + s) - v->y)) > 15.0) {
			continue;
		}
		if (cnt++ > 0x30) {
			break;
		}
		FXConversor_Glow(sp, d, v->x + ox, oy + v->y, oz + v->z, r);
	}
	if ((uint32_t)(cnt - 1) <= 0xe) {
		for (int k = 0; k < crt_rand() % 8 + 8; k++) {
			float rz = FXC_Rand(0.20000000298023224, -0.10000000149011612);
			float ry = FXC_Rand(0.10000000149011612, -0.05000000074505806);
			float rx = FXC_Rand(0.20000000298023224, -0.10000000149011612);
			vec3_t *v = &verts[k];
			v->x = rx * r + c.x;
			v->y = (c.y + s) + ry * r;
			v->z = rz * r + c.z;
			FXConversor_Glow(sp, d, v->x + ox, oy + v->y, oz + v->z, r);
		}
	}
	self->emitT = 0.0f;
}
/* the acceleration of the Sputnik sparks: against the velocity and down */
static void FXSputnik_SetAcc(CGameParticles *sp) {
	sp->initAccMin.x = -(sp->initVelMin.x * 0.15f) + 0.0f;
	sp->initAccMin.y = -(sp->initVelMin.y * 0.15f) - 25.0f;
	sp->initAccMin.z = -(sp->initVelMin.z * 0.15f) + 0.0f;
	sp->initAccMax = sp->initAccMin;
}

void __thiscall cObjElementFXSputnik_Update(cObjElementFXSputnik *self, cDebris *d, float dt) {
	vec3_t pos, rot, dir;
	matrix_t mat;

	(void)dt;
	self->ent = FXC_Keep(self->ent);
	if (!self->ent || !self->ent->flag_4c) {
		return;
	}
	CGameModel *m = FXC_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), 0.0f, 0.0f);
	float delta = FXC_Delta();
	float e = delta + self->emitT;
	float t = delta + self->t;
	self->t = t;
	self->emitT = e;
	if (0.02f > e) {
		return;
	}
	if (self->duration > 0.0f && t > self->duration) {
		return;
	}
	m = Obj_Vtbl(self->ent)->GetModel(self->ent);
	if (!CGameModel_IsLoaded(m)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(m));
	if (!CGameModel_IsVisible(m) || self->node == -1) {
		return;
	}
	const matrix_t *mp = CGameModel_NodeMatrix(m, &mat, (uint32_t)self->node, 1, NULL, 0);
	float px = mp->m[12], py = mp->m[13], pz = mp->m[14];
	if (self->minMove > 0.0f) {
		float dx = self->last.x - px, dy = self->last.y - py, dz = self->last.z - pz;
		float mm = self->minMove * self->minMove;
		if (mm > dy * dy + dx * dx + dz * dz) {
			Float_Approach(&self->moveT, 0.0f, self->emitT * 4.0f);
		} else {
			Float_Approach(&self->moveT, 1.0f, self->emitT);
		}
		self->last.x = px;
		self->last.y = py;
		self->last.z = pz;
		if (0.5f > self->moveT) {
			return;
		}
	}
	/* the glow at the node */
	CGameParticles *sp = &self->particles;
	sp->birthRate = 0;
	CGameParticles_SetColorRampOn(sp, 1);
	sp->initPosMin.x = px;
	sp->initPosMin.y = py;
	sp->initPosMin.z = pz;
	sp->initPosMax = sp->initPosMin;
	sp->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initVelMax = sp->initVelMin;
	FXSputnik_SetAcc(sp);
	sp->lifeTime = FXC_Rand(0.10000000894069672, 0.20000000298023224);
	sp->sizeIni = FXC_Rand(10.0, 70.0);
	sp->sizeEnd = FXC_Rand(25.0, 265.0);
	sp->sizeLife = FXC_Rand(0.09999999403953552, 0.30000001192092896);
	sp->rotIni = FXC_Rand(6.2831854820251465, 0.0);
	float r = (float)(FXC_Rnd() * 0.7853981852531433 - 0.39269909262657166);
	sp->rotVar = 0.0f;
	sp->spriteIni = 5;
	sp->spriteEnd = 5;
	sp->rotEnd = r;
	CGameParticles_SetColorRamp(sp, GAME_PTR(cObjElementFXSputnik_Update_4303d4, 0, 0x4303d4));
	CGameParticles_Apply(sp);
	CGameParticles_EmitN(sp, 1, 0);
	/* three sparks */
	for (int i = 0; i < 3; i++) {
		FX_RandomDir(&dir);
		float s = FXC_Rand(30.0, 0.0);
		sp->initPosMin.x = dir.x * s + px;
		sp->initPosMin.y = dir.y * s + py;
		sp->initPosMin.z = dir.z * s + pz;
		sp->initPosMax = sp->initPosMin;
		FX_RandomDir(&dir);
		s = FXC_Rand(25.0, 40.0);
		sp->initVelMin.x = dir.x * s;
		sp->initVelMin.y = dir.y * s;
		sp->initVelMin.z = dir.z * s;
		sp->initVelMax = sp->initVelMin;
		FXSputnik_SetAcc(sp);
		sp->lifeTime = FXC_Rand(0.19999998807907104, 0.5);
		sp->sizeIni = FXC_Rand(10.0, 50.0);
		sp->sizeEnd = FXC_Rand(25.0, 135.0);
		sp->sizeLife = FXC_Rand(0.10000002384185791, 0.8999999761581421);
		sp->rotIni = FXC_Rand(6.2831854820251465, 0.0);
		sp->rotVar = 0.0f;
		sp->rotEnd = (float)(FXC_Rnd() * 0.7853981852531433 - 0.39269909262657166);
		byte sprite = (crt_rand() & 1) ? 9 : 15;
		sp->spriteIni = sprite;
		sp->spriteEnd = sprite;
		CGameParticles_SetColorRamp(sp, GAME_PTR(cObjElementFXSputnik_Update_4303b0, 0, 0x4303b0));
		CGameParticles_Apply(sp);
		CGameParticles_EmitN(sp, 1, 0);
	}
	self->emitT = 0.0f;
}

/* ---- the starts ---- */

/* the emitter of the particle at v plus a random direction of length 0..1, still */
static void FXC_SetEmitPoint(CGameParticles *sp, const vec3_t *v) {
	vec3_t tmp;

	sp->birthRate = 0;
	float r = (float)(FXC_Rnd() + 0.0);
	const vec3_t *p = FX_RandomDir(&tmp);
	sp->initPosMin.x = v->x + p->x * r;
	sp->initPosMin.y = v->y + p->y * r;
	sp->initPosMin.z = v->z + p->z * r;
	sp->initPosMax = sp->initPosMin;
}

/* the velocity towards the point of the attacked (o + pb) over the time 1/k, braked by the acceleration s * -vel */
static void FXC_SetFlight(CGameParticles *sp, const vec3_t *v, const vec3_t *o, const vec3_t *pb, float k,
	double range, double add) {
	sp->initVelMin.x = ((o->x + pb->x) - v->x) * k;
	sp->initVelMin.y = ((o->y + pb->y) - v->y) * k;
	sp->initVelMin.z = ((o->z + pb->z) - v->z) * k;
	sp->initVelMax = sp->initVelMin;
	float s = FXC_Rand(range, add);
	sp->initAccMin.x = s * -sp->initVelMin.x;
	sp->initAccMin.y = s * -sp->initVelMin.y;
	sp->initAccMin.z = s * -sp->initVelMin.z;
	sp->initAccMax = sp->initAccMin;
}

static void FXC_EmitRamp(CGameParticles *sp, uint32_t *ramp, float dt) {
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, ramp);
	CGameParticles_EmitDt(sp, 1, 0, dt);
}

#define FXC_EMIT_VERTS 128

void __fastcall cObjElementFXCharacterConversion_Emit(cObjElementFXCharacterConversion *self) {
	vec3_t verts[FXC_EMIT_VERTS], pa, pb;

	CGameModel *ma = Obj_Vtbl(self->attacker)->GetModel(self->attacker);
	if (!ma || !CGameModel_IsLoaded(ma)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(ma));
	CGameModel *mb = Obj_Vtbl(self->attacked)->GetModel(self->attacked);
	if (!mb || !CGameModel_IsLoaded(mb)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(mb));
	float ra = Obj_Vtbl(self->attacker)->GetRadius(self->attacker);
	float rb = Obj_Vtbl(self->attacked)->GetRadius(self->attacked);
	CGameModel_Vertices(ma, verts, NULL, NULL, FXC_EMIT_VERTS, -1, NULL, 0xffffffffu, NULL);
	CGameParticles *sp = &self->particles;
	CGameParticles *sp2 = &self->particles2;
	/* the glow on the attacker */
	for (int i = 0; i < 8; i++) {
		FXC_SetEmitPoint(sp, &verts[i]);
		sp->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		sp->initVelMax = sp->initVelMin;
		sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		sp->initAccMax = sp->initAccMin;
		sp->lifeTime = FXC_Rand(0.04999999701976776, 0.20000000298023224);
		sp->sizeIni = FXC_Rand(0.09999999403953552, 0.3199999928474426) * ra * 1.7999999523162842f;
		sp->sizeLife = 0.3f;
		sp->sizeEnd = FXC_Rand(0.09999999403953552, 0.3199999928474426) * ra * 3.799999952316284f;
		sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
		sp->spriteIni = 5;
		sp->spriteEnd = 5;
		sp->rotVar = (float)(FXC_Rnd() * 1.0471975803375244 - 0.5235987901687622);
		FXC_EmitRamp(sp, GAME_ARRAY(uint32_t, cObjElementFXCharacterConversion_Emit_430440, 0x430440), 0.07f);
	}
	for (int i = 8; i < 32; i++) {
		FXC_SetEmitPoint(sp, &verts[i]);
		sp->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		sp->initVelMax = sp->initVelMin;
		sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		sp->initAccMax = sp->initAccMin;
		sp->lifeTime = FXC_Rand(0.050000011920928955, 0.25);
		sp->sizeIni = FXC_Rand(0.09999999403953552, 0.3199999928474426) * ra * 1.2999999523162842f;
		sp->sizeLife = 0.4f;
		sp->sizeEnd = FXC_Rand(0.09999999403953552, 0.3199999928474426) * ra * 1.899999976158142f;
		sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
		sp->spriteIni = 20;
		sp->spriteEnd = 20;
		sp->rotVar = (float)(FXC_Rnd() * 1.5707963705062866 - 0.7853981852531433);
		FXC_EmitRamp(sp, GAME_ARRAY(uint32_t, cObjElementFXCharacterConversion_Emit_4303f8, 0x4303f8), 0.07f);
	}
	/* the flight of every vertex to the same place on the attacked, scaled by the radii */
	float ratio = rb / ra;
	CGameModel_GetPos(ma, &pa);
	CGameModel_GetPos(mb, &pb);
	for (int i = 0; i < FXC_EMIT_VERTS; i++) {
		const vec3_t *v = &verts[i];
		vec3_t o = { v->x - pa.x, v->y - pa.y, v->z - pa.z };
		FXC_SetEmitPoint(sp2, v);
		o.x = o.x * ratio;
		o.y = o.y * ratio;
		o.z = o.z * ratio;
		FXC_SetFlight(sp2, v, &o, &pb, 5.714285850524902f, 0.20000004768371582, 1.399999976158142);
		sp2->lifeTime = FXC_Rand(0.040000081062316895, 1.1799999475479126) * 0.25f;
		sp2->sizeIni = FXC_Rand(0.09999999403953552, 0.3199999928474426) * ra * 0.25f;
		sp2->sizeLife = 1.0f;
		sp2->rotIni = 0.0f;
		sp2->rotEnd = 0.0f;
		sp2->rotVar = 0.0f;
		sp2->sizeEnd = FXC_Rand(0.09999999403953552, 0.3199999928474426) * rb * 0.25f;
		byte sprite = (byte)(crt_rand() % 8 + 1);
		sp2->spriteIni = sprite;
		sp2->spriteEnd = sprite;
		FXC_EmitRamp(sp2, GAME_ARRAY(uint32_t, cObjElementFXCharacterConversion_Emit_430464, 0x430464), 0.08f);

		FXC_SetEmitPoint(sp, v);
		FXC_SetFlight(sp, v, &o, &pb, 4.0f, 0.2999999523162842, 1.2000000476837158);
		sp->lifeTime = FXC_Rand(0.040000081062316895, 1.1799999475479126) * 0.25f;
		sp->sizeIni = FXC_Rand(0.09999999403953552, 0.3199999928474426) * ra * 1.2999999523162842f;
		sp->sizeLife = 1.0f;
		sp->sizeEnd = FXC_Rand(0.09999999403953552, 0.3199999928474426) * rb * 1.2999999523162842f;
		sp->rotIni = sp->rotEnd = FXC_Rand(6.2831854820251465, 0.0);
		sp->spriteIni = 5;
		sp->spriteEnd = 5;
		sp->rotVar = (float)(FXC_Rnd() * 1.5707963705062866 - 0.7853981852531433);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, GAME_ARRAY(uint32_t, cObjElementFXCharacterConversion_Emit_4303f8, 0x4303f8) + 9 * (crt_rand() & 1));
		CGameParticles_EmitDt(sp, 1, 0, 0.08f);
	}
}

void __thiscall cObjElementFXMaintenanceWork_Start(cObjElementFXMaintenanceWork *self, cDebris *d) {
	vec3_t laser, pos;
	cSoundDesc desc;

	self->life = Debris_Life(d);
	CGameModel *m = Obj_Vtbl(self->maintenance)->GetModel(self->maintenance);
	CHECK(CGameModel_IsLoaded(m));
	CGameModel_Vertices(m, &laser, NULL, NULL, 1, 0, NULL, (uint32_t)self->nodeLaser, NULL);
	m = Obj_Vtbl(self->ship)->GetModel(self->ship);
	CHECK(CGameModel_IsLoaded(m));
	CGameModel_ClosestVertex(m, laser.x, laser.y, laser.z, &pos, &self->vertex, 0xffffffffu);
	self->sound = 0;
	cSoundDesc_Init(&desc);
	desc.param1 = g_MaintenanceWorkSoundAttIni;
	desc.param2 = g_MaintenanceWorkSoundAttEnd;
	desc.param3 = 0.0f;
	cSoundDesc_SetPos(&desc, &pos);
	desc.volume += Snd_Volume(g_MaintenanceWorkSoundVol);
	desc.loop = 1;
	CSoundSys *snd = g_pSound;
	snd->vftable->PlaySound(snd, g_MaintenanceWorkSound, &desc, &self->sound);
}

void FX_MoneyTransfer(cEntity *from, cEntity *to, float time) {
	cObjElementFXMoneyTransfer *el = FX_DropNew(g_FXMoneyTransferFile, time);

	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXMoneyTransfer_SetEntities(el, from, to);
	}
}

void FX_CharacterConversion(cEntity *attacker, cEntity *attacked, float time) {
	vec3_t t1, t2, t3, pos;
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXCharacterConversionFile);
	cDebris *d = cDebrisSys_NewElement(sys);

	t1 = FXC_CamPos();
	CGameModel_SetPos(Debris_Model(d), &t1);
	Debris_Life(d) = time + 0.25f;
	cDebrisSys_Drop(sys, d);
	cObjElementFXCharacterConversion *el = Debris_Element(d);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXCharacterConversion_SetAttacker(el, attacker);
		cObjElementFXCharacterConversion_SetAttacked(el, attacked);
		cObjElementFXCharacterConversion_Emit(el);
	}
	const vec3_t *a = Obj_Vtbl(attacker)->GetViewPos(attacker, &t1);
	const vec3_t *b = Obj_Vtbl(attacked)->GetViewPos(attacked, &t2);
	float dx = (b->x - a->x) * 0.5f;
	float dy = (b->y - a->y) * 0.5f;
	float dz = (b->z - a->z) * 0.5f;
	a = Obj_Vtbl(attacker)->GetViewPos(attacker, &t3);
	pos.x = a->x + dx;
	pos.y = a->y + dy;
	pos.z = a->z + dz;
	const char *name = g_CharacterConversionSound;
	CSoundSys *snd = g_pSound;
	int vol = Snd_Volume(g_CharacterConversionSoundVol);
	snd->vftable->PlayAt(snd, name, &pos, vol);
}

void FX_CharacterConversor(cEntity *character, cEntity *conversor, int phase, float a, float b) {
	vec3_t tmp;

	CHECK(conversor);
	const vec3_t *p = conversor->vftable->GetPos(conversor, &tmp);
	if (!FX_IsVisible(p->x, p->y, p->z, 500.0f, 0.0f, NULL, 1)) {
		return;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXCharacterConversorFile);
	cDebris *d = cDebrisSys_NewElement(sys);
	CGameModel_SetPos(Debris_Model(d), conversor->vftable->GetPos(conversor, &tmp));
	Debris_Life(d) = a;
	cDebrisSys_Drop(sys, d);
	cObjElementFXCharacterConversor *el = Debris_Element(d);
	if (!el) {
		return;
	}
	if (!el->started) {
		el->started = 1;
	}
	el->phase = phase;
	cObjElementFXCharacterConversor_SetCharacter(el, character);
	cObjElementFXCharacterConversor_SetConversor(el, conversor);
	el->duration = a;
	el->timeIn = b;
	el->timeOut = b;
}

/* the Sputnik effects by name: the life of the debris and the duration of the element */
static const struct {
	const char *name;
	float life;
	float duration;
} fxc_sputniks[] = {
	{ "CharacterSputnikHello", 5.0f, 4.0f },
	{ "CharacterSputnikBye", 1.9f, 1.0f },
	{ "CharacterSputnikInterface", 1.7f, 1.4f },
	{ "CharacterSputnikJunkyard", 30.0f, 0.0f },
};

void FX_Character(cEntity *e, const char *name) {
	vec3_t tmp;

	if (!strcmp(name, "CharacterDeath")) {
		CGameModel *m = Obj_Vtbl(e)->GetModel(e);
		if (!CGameModel_IsLoaded(m)) {
			return;
		}
		if (!CGameModel_IsVisible(m)) {
			vec3_t cp = FXC_CamPos();
			const vec3_t *p = e->vftable->GetPos(e, &tmp);
			float dx = cp.x - p->x, dy = cp.y - p->y, dz = cp.z - p->z;
			float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			if (!(g_FXCharacterDeathMinDist > dist)) {
				CGameModel_Disable(m);
				return;
			}
		}
		cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXCharacterDeathFile);
		sys->radiusSq = 0.0f;
		cDebris *d = cDebrisSys_NewElement(sys);
		Debris_Life(d) = 2.0f;
		cDebrisSys_Drop(sys, d);
		cObjElementFXCharacterDeath *el = Debris_Element(d);
		if (el) {
			if (!el->started) {
				el->started = 1;
			}
			cObjElementFXCharacterDeath_SetEntity(el, e);
		}
		const char *snd = g_CharacterDeathSound;
		CSoundSys *s = g_pSound;
		int vol = Snd_Volume(g_CharacterDeathSoundVol);
		s->vftable->PlayAt(s, snd, Obj_Vtbl(e)->GetViewPos(e, &tmp), vol);
		return;
	}
	if (!crt_strncmp(name, "CharacterSputnik", 16)) {
		if (!CGameModel_IsLoaded(Obj_Vtbl(e)->GetModel(e))) {
			return;
		}
		int k = 0;
		while (strcmp(name, fxc_sputniks[k].name)) {
			if (++k == (int)(sizeof(fxc_sputniks) / sizeof(fxc_sputniks[0]))) {
				return;
			}
		}
		float duration = fxc_sputniks[k].duration;
		cObjElementFXSputnik *el = FX_DropNew(g_FXCharacterSputnikFile, fxc_sputniks[k].life);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		FXSputnik_Start(el, e);
		el->duration = duration;
		if (duration == 0.0f) {
			el->minMove = 5.0f;
		}
		return;
	}
	if (!strcmp(name, "FunctionarySpecialAction")) {
		if (!CGameModel_IsLoaded(Obj_Vtbl(e)->GetModel(e))) {
			return;
		}
		byte *el = FX_DropNew(g_FXFunctionarySpecialActionFile, g_FunctionarySpecialActionTime + 3.5f);
		if (el) {
			if (!el[0x1a8]) {
				el[0x1a8] = 1;
			}
			cObjElementFXFunctionarySpecialAction_Start(el, e);
		}
	}
}

/* ---- the native functions ---- */

/* the parameters: the character, a second name and 4 bytes */
static cEntity *FXC_ReadParams(cAbstractFile *f, char *name, char *name2, void *v) {
	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, name2, 0x80);
	f->vftable->Read(f, v, 4);
	cEntity *ch = World_FindEntity(name);
	CHECK(ch);
	return ch;
}

/* an idle debris of the system of file (else the oldest one) at pos with the life; dropped when it was idle */
static cDebris *FXC_IdleDebris(cDebrisSys *sys, const vec3_t *pos, float life) {
	cDebris *d = cDebrisSys_GetIdle(sys);

	CGameModel_SetPos(Debris_Model(d), pos);
	Debris_Life(d) = life;
	if (!cDebrisSys_IsIdle(sys, d)) {
		cDebrisSys_Drop(sys, d);
	}
	return d;
}

void FX_CharacterBlaBliBla(cAbstractFile *params) {
	char name[128], name2[128];
	int unused;
	vec3_t tmp, rot, in, p, n;

	cEntity *ch = FXC_ReadParams(params, name, name2, &unused);
	CHECK(Entity_IsType(ch, TYEN_WALKCHAR));
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXCharacterBlaBliBlaFile);
	vec3_t cp = FXC_CamPos();
	cDebris *d = FXC_IdleDebris(sys, &cp, 2.0f);
	cObjElementFXParticleSystem *el = Debris_Element(d);
	if (!el) {
		return;
	}
	CGameParticles *sp = &el->particles;
	if (!el->on) {
		el->move = 0;
		el->on = 1;
	}
	/* a word bubble above the head, turned with the character */
	rot.x = 0.0f;
	rot.y = Obj_Vtbl(ch)->GetViewAng(ch, &tmp)->y;
	rot.z = 0.0f;
	float r1 = FXC_Rand(0.10000002384185791, 0.44999998807907104);
	float r2 = FXC_Rand(0.06000000238418579, 0.3400000035762787);
	float r3 = (float)(FXC_Rnd() * 0.20000000298023224 - 0.10000000149011612);
	in.x = Obj_Vtbl(ch)->GetRadius(ch) * r3;
	in.y = Obj_Vtbl(ch)->GetRadius(ch) * r2;
	in.z = -(Obj_Vtbl(ch)->GetRadius(ch) * r1);
	Vec3_Rotate(&p, &in, &rot);
	sp->birthRate = 0;
	const vec3_t *c = Obj_Vtbl(ch)->GetViewPos(ch, &tmp);
	sp->initPosMin.x = c->x + p.x;
	sp->initPosMin.y = c->y + p.y;
	sp->initPosMin.z = c->z + p.z;
	sp->initPosMax = sp->initPosMin;
	float speed = FXC_Rand(75.0, 100.0);
	float len = (float)__builtin_sqrt((double)(p.y * p.y + p.x * p.x + p.z * p.z));
	if (len == 0.0f) {
		n = p;
	} else {
		float inv = 1.0f / len;
		n.x = p.x * inv;
		n.y = p.y * inv;
		n.z = p.z * inv;
	}
	sp->initVelMin.x = speed * n.x;
	sp->initVelMin.y = speed * n.y;
	sp->initVelMin.z = speed * n.z;
	sp->initVelMax = sp->initVelMin;
	float s = FXC_Rand(0.19999998807907104, 0.25);
	sp->initAccMin.x = s * -sp->initVelMin.x;
	sp->initAccMin.y = s * -sp->initVelMin.y;
	sp->initAccMin.z = s * -sp->initVelMin.z;
	sp->initAccMax = sp->initAccMin;
	sp->lifeTime = FXC_Rand(0.09999996423721313, 0.949999988079071);
	sp->sizeLife = FXC_Rand(0.19999998807907104, 0.25);
	sp->sizeIni = FXC_Rand(5.0, 5.0);
	sp->sizeEnd = FXC_Rand(30.0, 40.0);
	sp->rotIni = sp->rotEnd = (float)(FXC_Rnd() * 0.7853981852531433 - 0.39269909262657166);
	sp->rotVar = FXC_Rand(0.39269909262657166, 0.0);
	if (crt_rand() % 3 == 0) {
		sp->spriteIni = (byte)(crt_rand() % 3 + 1);
	} else {
		sp->spriteIni = 1;
	}
	sp->spriteEnd = sp->spriteIni;
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, GAME_PTR(FX_CharacterBlaBliBla_430318, 0, 0x430318));
	CGameParticles_EmitN(sp, 1, 0);
}

void FX_CharacterDazed(cAbstractFile *params) {
	char name[128], name2[128];
	float time;
	vec3_t tmp, rot, in, off;

	cEntity *ch = FXC_ReadParams(params, name, name2, &time);
	CHECK(Entity_IsType(ch, TYEN_WALKCHAR));
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXCharacterDazedFile);
	vec3_t cp = FXC_CamPos();
	cDebris *d = FXC_IdleDebris(sys, &cp, 2.0f);
	cObjElementFXParticleSystem *el = Debris_Element(d);
	if (!el) {
		return;
	}
	CGameParticles *sp = &el->particles;
	if (!el->on) {
		el->move = 0;
		el->on = 1;
	}
	/* stars above the head */
	int n = crt_rand() % 2 + 1;
	rot.x = 0.0f;
	rot.y = Obj_Vtbl(ch)->GetViewAng(ch, &tmp)->y;
	rot.z = 0.0f;
	float a = Obj_Vtbl(ch)->GetRadius(ch) * 0.25f;
	float b = Obj_Vtbl(ch)->GetRadius(ch) * -0.5f;
	in.x = 0.0f;
	in.y = a;
	in.z = b;
	Vec3_Rotate(&off, &in, &rot);
	for (int k = 0; k < n; k++) {
		sp->birthRate = 0;
		const vec3_t *c = Obj_Vtbl(ch)->GetViewPos(ch, &tmp);
		sp->initPosMin.x = c->x + off.x;
		sp->initPosMin.y = c->y + off.y;
		sp->initPosMin.z = c->z + off.z;
		sp->initPosMax = sp->initPosMin;
		float vz = (float)(FXC_Rnd() * 16.0 - 8.0);
		float vy = FXC_Rand(20.0, 80.0);
		sp->initVelMin.x = (float)(FXC_Rnd() * 16.0 - 8.0);
		sp->initVelMin.y = vy;
		sp->initVelMin.z = vz;
		sp->initVelMax = sp->initVelMin;
		double ra = FXC_Rnd();
		float ay = (float)(-25.0 - FXC_Rnd() * 5.0);
		double rb = FXC_Rnd();
		sp->initAccMin.x = (float)((rb + rb) - 1.0);
		sp->initAccMin.y = ay;
		sp->initAccMin.z = (float)((ra + ra) - 1.0);
		sp->initAccMax = sp->initAccMin;
		sp->sizeLife = 1.0f;
		sp->lifeTime = FXC_Rand(0.2999999523162842, 1.2000000476837158);
		sp->sizeIni = FXC_Rand(5.0, 15.0);
		sp->sizeEnd = FXC_Rand(5.0, 35.0);
		sp->rotIni = sp->rotEnd = (float)(FXC_Rnd() * 1.0471975803375244 - 0.5235987901687622);
		sp->spriteIni = 1;
		sp->spriteEnd = 1;
		sp->rotVar = FXC_Rand(0.39269909262657166, 0.0);
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, GAME_PTR(FX_CharacterDazed_430264, 0, 0x430264));
		CGameParticles_EmitDt(sp, 1, 0, (time / (float)n) * (float)k);
	}
}

void FX_CharacterTeleport(cAbstractFile *params) {
	char name[128], name2[128];
	int mode;
	vec3_t tmp;

	cEntity *ch = FXC_ReadParams(params, name, name2, &mode);
	CHECK(Entity_IsType(ch, TYEN_WALKCHAR));
	cEntity *tp = World_FindEntity(name2);
	CHECK(tp);
	CHECK(Entity_IsType(tp, TYEN_WITH_LIFE));
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXCharacterTeleportFile);
	cDebris *d = cDebrisSys_GetIdle(sys);
	CGameModel_SetPos(Debris_Model(d), tp->vftable->GetPos(tp, &tmp));
	Debris_Life(d) = 1.1f;
	if (!cDebrisSys_IsIdle(sys, d)) {
		cDebrisSys_Drop(sys, d);
	} else if (Debris_Element(d)) {
		/* the element of a reused debris starts over (vf04) */
		cObjElementFX *el = Debris_Element(d);
		el->vftable->Reset(el, d);
	}
	cObjElementFXCharacterTeleport *el = Debris_Element(d);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		el->character = ch;
		el->teleport = tp;
		el->mode = mode;
		el->duration = 1.0f;
		if (mode == 1) {
			el->timeIn = 0.2f;
			el->timeOut = 0.1f;
		} else if (mode == 2) {
			el->timeIn = 0.1f;
			el->timeOut = 0.2f;
		}
	}
	const char *snd;
	if (mode == 1) {
		snd = g_CharacterTeleportUpSound;
	} else if (mode == 2) {
		snd = g_CharacterTeleportDownSound;
	} else {
		return;
	}
	CSoundSys *s = g_pSound;
	int vol = Snd_Volume(g_CharacterTeleportSoundVol);
	s->vftable->PlayAt(s, snd, ch->vftable->GetPos(ch, &tmp), vol);
}
