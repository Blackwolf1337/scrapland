/*
 * World/FX/FXChar.cpp of the game: the effects of the characters, see include/scrap/fxchar.h.
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/fx.h"
#include "scrap/fxchar.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/sound.h"
#include "scrap/timer.h"
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
	int i = CGameModel_FindNodoR(m, name);

	CHECK(i > -1);
	return i;
}

/* the model of the entity, CHECKed */
static CGameModel *FXC_Model(cEntity *e) {
	CGameModel *m = FX_Vtbl(e)->GetModel(e);

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
	self->node = CGameModel_FindNodoR(m, "Bip Spootnick MASTER");
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
		self->nodePared[i - 1] = CGameModel_FindNodoR(m, buf);
		Str_PrintfQuiet(buf, 0xff, "DF_Salida_%02d", i);
		self->nodeSalida[i - 1] = CGameModel_FindNodoR(m, buf);
	}
	for (int i = 1; i <= 4; i++) {
		Str_PrintfQuiet(buf, 0xff, "DF_Light_%02d", i);
		self->nodeLight[i - 1] = CGameModel_FindNodoR(m, buf);
	}
}

void __thiscall FXChar_SetI4(void *self, int v) {
	*(int *)((byte *)self + 4) = v;
}

void __thiscall cObjElementFXMaintenanceWork_SetMaintenance(cObjElementFXMaintenanceWork *self, cEntity *e) {
	self->maintenance = e;
	CGameModel *m = FXC_Model(e);
	self->nodeLaser = CGameModel_FindNodoR(m, "DF_Laser");
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

/* the random points of the conversion trails */
static void FXConversion_RandomPoints(cObjElementFXCharacterConversion *self) {
	vec3_t tmp;

	for (int i = 0; i < FXCONVERSION_POINTS; i++) {
		self->phase[i] = (float)(FXC_Rnd() + 0.0);
		float r = FXC_Rand(0.699999988079071, 0.30000001192092896);
		const vec3_t *p = FX_RandomDir(&tmp);
		self->dir[i].x = r * p->x;
		self->dir[i].y = p->y * r;
		self->dir[i].z = p->z * r;
		self->speed[i] = FXC_Rand(12.0, 12.0);
		self->scale[i] = FXC_Rand(0.800000011920929, 0.699999988079071);
	}
	self->f178 = 0.0f;
}

void __thiscall cObjElementFXCharacterConversion_Attach(cObjElementFXCharacterConversion *self, cDebris *d) {
	CGameModel *m = Debris_Model(d);
	uint32_t i = CGameModel_IsLoaded(m) ? (uint32_t)CGameModel_FindNodoR(m, "Dummy01") : (uint32_t)(uintptr_t)m;

	if (i != 0xffffffffu) {
		CGameParticles_Attach(&self->particles, m, i);
	}
	CHECK(self->particles.SistPart);
	if (CGameModel_IsLoaded(m)) {
		i = (uint32_t)CGameModel_FindNodoR(m, "Dummy02");
	}
	if (i != 0xffffffffu) {
		CGameParticles_Attach(&self->particles2, m, i);
	}
	CHECK(self->particles2.SistPart);
	for (int k = 0; k < FXCONVERSION_TRAILS; k++) {
		CGameTrail_SetMaxTrails(&self->trails[k], 15);
		CGameTrail_SetType(&self->trails[k], 3);
	}
	FXConversion_RandomPoints(self);
}

void __thiscall cObjElementFXCharacterConversion_Reset(cObjElementFXCharacterConversion *self, int arg) {
	(void)arg;
	self->f16c = 0.0f;
	self->f170 = 0.0f;
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
		self->f284[i] = 0.5f;
		self->trailT[i] = -1.0f;
	}
	self->n290 = 5;
}

void __thiscall cObjElementFXCharacterConversor_Reset(cObjElementFXCharacterConversor *self, int arg) {
	(void)arg;
	self->fcc = 0.0f;
	self->fd0 = 0.0f;
	self->phase = 0;
	self->character = NULL;
	self->conversor = NULL;
	CHECK(&self->particles);
	CHECK(self->particles.SistPart);
	CGameParticles_KillAll(&self->particles);
	for (int i = 0; i < self->numTrails; i++) {
		CGameTrail_Clear(&self->trails[i]);
		self->trailT[i] = 0.0f;
	}
}

static void FXDeath_ResetTrails(cObjElementFXCharacterDeath *self, byte attach) {
	self->f218 = 0.0f;
	self->f21c = 0.0f;
	self->f220 = 0.0f;
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
	for (int i = 0; i < 0x78; i++) {
		self->f7e4[i] = 5.0f;
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
	self->fcc = 0.0f;
	self->fd0 = 0.0f;
	self->mode = 0;
}

void __thiscall cObjElementFXMaintenanceWork_Attach(cObjElementFXMaintenanceWork *self, cDebris *d) {
	CGameModel *m = Debris_Model(d);

	CHECK(CGameModel_IsLoaded(m));
	CGameParticles_Attach(&self->particles, m, FXC_Node(m, "Dummy01"));
	CHECK(self->particles.SistPart);
	CGameTrail_SetMaxTrails(&self->trails[0], 3);
	CGameTrail_SetTexture(&self->trails[0], 0);
	CGameTrail_SetMaxTrails(&self->trails[1], 3);
	CGameTrail_SetTexture(&self->trails[1], 3);
	self->life = 0.0f;
	self->sound = 0;
}

void __thiscall cObjElementFXMaintenanceWork_Reset(cObjElementFXMaintenanceWork *self, int arg) {
	(void)arg;
	self->fc0 = 0.0f;
	self->fbc = 0.0f;
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
		CGameModel *m = FX_Vtbl(self->attacker)->GetModel(self->attacker);
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
		CGameModel *m = FX_Vtbl(self->character)->GetModel(self->character);
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
	FX_Vtbl(self->from)->GetCenter(self->from, &a);
	FX_Vtbl(self->to)->GetCenter(self->to, &b);
	float rFrom = FX_Vtbl(self->from)->GetRadius(self->from);
	float rTo = FX_Vtbl(self->to)->GetRadius(self->to);
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
	CGameParticles_SetColorRamp(sp, Game_Ptr(FXC_RAMP_MONEY));
	CGameParticles_EmitN(sp, 1, 0);
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

static void FXC_EmitRamp(CGameParticles *sp, uint32_t ramp, float dt) {
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, Game_Ptr(ramp));
	CGameParticles_EmitDt(sp, 1, 0, dt);
}

#define FXC_EMIT_VERTS 128

void __fastcall cObjElementFXCharacterConversion_Emit(cObjElementFXCharacterConversion *self) {
	vec3_t verts[FXC_EMIT_VERTS], pa, pb;

	CGameModel *ma = FX_Vtbl(self->attacker)->GetModel(self->attacker);
	if (!ma || !CGameModel_IsLoaded(ma)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(ma));
	CGameModel *mb = FX_Vtbl(self->attacked)->GetModel(self->attacked);
	if (!mb || !CGameModel_IsLoaded(mb)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(mb));
	float ra = FX_Vtbl(self->attacker)->GetRadius(self->attacker);
	float rb = FX_Vtbl(self->attacked)->GetRadius(self->attacked);
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
		FXC_EmitRamp(sp, 0x430440, 0.07f);
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
		FXC_EmitRamp(sp, 0x4303f8, 0.07f);
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
		FXC_EmitRamp(sp2, 0x430464, 0.08f);

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
		CGameParticles_SetColorRamp(sp, Game_Ptr(0x4303f8 + (crt_rand() & 1) * 0x24));
		CGameParticles_EmitDt(sp, 1, 0, 0.08f);
	}
}

void __thiscall cObjElementFXMaintenanceWork_Start(cObjElementFXMaintenanceWork *self, cDebris *d) {
	vec3_t laser, pos;
	cSoundDesc desc;

	self->life = Debris_Life(d);
	CGameModel *m = FX_Vtbl(self->maintenance)->GetModel(self->maintenance);
	CHECK(CGameModel_IsLoaded(m));
	CGameModel_Vertices(m, &laser, NULL, NULL, 1, 0, NULL, (uint32_t)self->nodeLaser, NULL);
	m = FX_Vtbl(self->ship)->GetModel(self->ship);
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
	const vec3_t *a = FX_Vtbl(attacker)->GetCenter(attacker, &t1);
	const vec3_t *b = FX_Vtbl(attacked)->GetCenter(attacked, &t2);
	float dx = (b->x - a->x) * 0.5f;
	float dy = (b->y - a->y) * 0.5f;
	float dz = (b->z - a->z) * 0.5f;
	a = FX_Vtbl(attacker)->GetCenter(attacker, &t3);
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
	el->fc4 = b;
	el->fc8 = b;
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
		CGameModel *m = FX_Vtbl(e)->GetModel(e);
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
		s->vftable->PlayAt(s, snd, FX_Vtbl(e)->GetCenter(e, &tmp), vol);
		return;
	}
	if (!crt_strncmp(name, "CharacterSputnik", 16)) {
		if (!CGameModel_IsLoaded(FX_Vtbl(e)->GetModel(e))) {
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
		if (!CGameModel_IsLoaded(FX_Vtbl(e)->GetModel(e))) {
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
