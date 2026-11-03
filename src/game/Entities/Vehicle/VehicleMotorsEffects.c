/*
 * Entities/Vehicle/VehicleMotorsEffects.cpp of the game: the exhaust effects of the motors of a ship, see
 * include/scrap/vehiclemotors.h: the particles at the DF_MotorTrust nodes, the glow, the trails, the smoke of the
 * motors, the Calima (hot air) particles and the exhaust particles of each FxClass type.
 * Not here: cNetVController (rva 0x1186b0..0x119850, the network copy of a ship) that follows in the exe is in
 * NetVController.c.
 */
#include <string.h>

#include "scrap/embeddedfxvehicle.h"
#include "scrap/fxmisc.h"
#include "scrap/vehicleentity.h"
#include "scrap/vehiclemotors.h"
#include "scrap/weaps2.h"
#include "scrap/staticinit.h"

#define g_FXEngineTrailOffset (GVar_FXEngineTrailOffset.value.f)
#define g_FXEngineTrailSize (GVar_FXEngineTrailSize.value.f)
#define g_FXEngineTrailDist (GVar_FXEngineTrailDist.value.f)
#define g_FXEngineGlowSizeAtt (GVar_FXEngineGlowSizeAtt.value.f)
#define g_FXEngineGlowDistFadeFrom (GVar_FXEngineGlowDistFadeFrom.value.f)
#define g_FXEngineBirthRate (GVar_FXEngineBirthRate.value.i)	/* of the exhaust at thrust 0, 100 at full thrust */
#define g_FXEngineGlowSize (GVar_FXEngineGlowSize.value.f)
#define g_FXEngineGlowSizeMax (GVar_FXEngineGlowSizeMax.value.f)
#define g_FXEngineTrailDistSpeedFactor (GVar_FXEngineTrailDistSpeedFactor.value.f)
static cGVar GVar_FXEngineTrailSize;
static cGVar GVar_FXEngineTrailOffset;
static cGVar GVar_FXEngineTrailDistSpeedFactor;
static cGVar GVar_FXEngineTrailDist;
static cGVar GVar_FXEngineGlowSizeMax;
static cGVar GVar_FXEngineGlowSizeAtt;
static cGVar GVar_FXEngineGlowSize;
static cGVar GVar_FXEngineGlowDistFadeFrom;
static cGVar GVar_FXEngineBirthRate;

/* (rand() & 0x7fff) / 32767 * mul + add, the double expression of the original */
static inline double VME_Rand(double mul, double add) {
	return (double)(crt_rand() & 0x7fff) * RAND_SCALE * mul + add;
}

static CGameParticles *VME_NewParticles(void) {
	CGameParticles *p = operator_new(sizeof(CGameParticles));
	return p ? CGameParticles_ctor(p) : NULL;
}

/* the particle system of node of model with no emission */
static void VME_StartParticles(CGameParticles *p, CGameModel *model, int node) {
	CGameParticles_Attach(p, model, node);
	CHECK(p->ParticleSystem);
	p->lifeTime = 0.0f;
	p->birthRate = 0;
	CGameParticles_Apply(p);
}

/* the position of node relative to node ref of model */
static vec3_t VME_NodeOffset(CGameModel *model, int ref, int node) {
	vec3_t a, b, r;

	vec3_t *pr = CGameModel_NodePos(model, &a, ref, 0);
	vec3_t *pn = CGameModel_NodePos(model, &b, node, 0);
	r.x = pn->x - pr->x;
	r.y = pn->y - pr->y;
	r.z = pn->z - pr->z;
	return r;
}

void cVehicleMotorsEffects_Init(cVehicleMotorsEffects *self) {
	char buf[256];
	int i, n;

	CHECK(self->owner);
	if (!self->motors && !self->motor) {
		return;
	}
	self->on = 0;
	self->type = 0;
	self->numExhausts = 0;
	const char *type = Veh(self->owner)->typeName;
	if (!crt_stricmp(type, "TVoteMayor") || !crt_stricmp(type, "TVoteBishop")) {
		return;
	}
	/* no trails for the type names T... */
	byte noTrails = type[0] == 'T';
	for (i = 0; i < VM_SIDES; i++) {
		CGameModel *model;
		if (self->motors) {
			cEntity *e = cVehicleMotors_GetOwner(self->motors);
			model = Obj_GetModel(e);
		} else if (self->motor && self->motor->node[i] != -1) {
			model = &self->motor->model[i];
		} else {
			continue;
		}
		if (!model || !CGameModel_IsLoaded(model)) {
			continue;
		}
		if (self->motors) {
			Str_PrintfQuiet(buf, 0xff, "DF_MotorTrust_%02d", i + 1);
		} else if (self->motor) {
			Str_PrintfQuiet(buf, 0xff, "DF_MotorTrust");
		}
		n = CGameModel_FindRenderNode(model, buf);
		if (n == -1) {
			if (self->motors && i == 0) {
				Str_PrintfQuiet(buf, 0xff, "DF_MotorTrust");
				n = CGameModel_FindRenderNode(model, buf);
			}
			if (n == -1 && self->motor) {
				Str_PrintfQuiet(buf, 0xff, "DF_MotorTrust_01");
				n = CGameModel_FindRenderNode(model, buf);
			}
		}
		self->node[i][0] = n;
		if (n == -1) {
			continue;
		}
		self->on = 1;
		if (self->node[i][0] != -1) {
			CHECK(!self->particleSystem[i][0]);
			self->particleSystem[i][0] = VME_NewParticles();
			VME_StartParticles(self->particleSystem[i][0], model, self->node[i][0]);
		}
		n = -1;
		if (self->motor) {
			Str_PrintfQuiet(buf, 0xff, "DF_MotorTrust_02");
			n = CGameModel_FindRenderNode(model, buf);
		}
		self->node[i][1] = n;
		if (n != -1) {
			CHECK(!self->particleSystem[i][1]);
			self->particleSystem[i][1] = VME_NewParticles();
			VME_StartParticles(self->particleSystem[i][1], model, self->node[i][1]);
		}
		CHECK(!self->trail[i]);
		if (!noTrails) {
			CGameTrail *t = operator_new(sizeof(CGameTrail));
			t = t ? CGameTrail_ctor(t) : NULL;
			self->trail[i] = t;
			CGameTrail_SetMaxTrails(t, 0x1e);
			CGameTrail_SetTexture(self->trail[i], 1);
			CGameTrail_SetType(self->trail[i], 2);
			CGameTrail_SetFade(self->trail[i], 1);
			CGameTrail_SetSmooth(self->trail[i], 0);
			CGameTrail_SetColor(self->trail[i], 0xffffffff);
			CGameTrail_SetWidth(self->trail[i], g_FXEngineTrailSize);
			CGameTrail_SetScale(self->trail[i], 1.0f, 8.0f);
		}
		vec3_t pos;
		if (self->motors) {
			pos = VME_NodeOffset(model, CGameModel_FindRenderNode(model, "DC_Root"), self->node[i][0]);
		} else {
			if (!self->motor) {
				continue;
			}
			CGameModel *vm = Obj_GetModel(self->owner);
			int mn = self->motor->node[i];
			if (mn == -1) {
				continue;
			}
			vec3_t d1 = VME_NodeOffset(vm, CGameModel_FindRenderNode(vm, "DC_Root"), mn);
			vec3_t rot, e, ang, r;
			matrix_t m;
			vec3_t *pr = CGameModel_GetRot(vm, &rot);
			vec3_t *pe = Matrix_GetEuler(CGameModel_NodeMatrix(vm, &m, mn, 0, NULL, 0), &e);
			ang.y = pe->y - pr->y;
			ang.z = pe->z - pr->z;
			ang.x = pe->x - pr->x;
			vec3_t d2 = VME_NodeOffset(model, CGameModel_FindRenderNode(model, "DC_Ship_Link"), self->node[i][0]);
			vec3_t *pv = Vec3_Rotate(&r, &d2, &ang);
			pos.x = pv->x + d1.x;
			pos.y = pv->y + d1.y;
			pos.z = pv->z + d1.z;
		}
		self->exhaustPos[i] = pos;
		self->exhaustSide[self->numExhausts] = i;
		self->numExhausts++;
	}
	self->trailThrust = 0.0f;
	self->lastThrust = 0.0f;
	self->dist = 0.0f;
	self->shrinkTime = 0.0f;
	self->colorBlend = 0.0f;
	self->segDist = g_FXEngineTrailDist;
	cVehicleMotorsEffects_Stop(self);
}

float Rand_FloatRange(float lo, float hi) {
	float d = hi - lo;
	return (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * (double)d + (double)lo);
}

static const uint32_t VMEffects_SetupHazeRamp[9] = { 0xbf404040, 0x7f808080, 0x7f808080, 0x7f808080, 0xa35c5c5c, 0xbf404040, 0xdf202020, 0xff000000, 0xff000000 };

void VMEffects_SetupHaze(float thrust, float speed, vec3_t *pos, int n, CGameParticles *ps) {
	float t = Float_MaxSS(0.3f, thrust);

	ps->birthRate = (int)(t * 26.0f) * n;
	CGameParticles_SetEmitPos(ps, pos, (byte)n);
	float v = speed * 150.0f;
	ps->initVelMin.x = -50.0f - v;
	ps->initVelMin.y = -50.0f - v;
	ps->initVelMin.z = (t + speed) * 200.0f + 850.0f;
	ps->initVelMax.x = v + 50.0f;
	ps->initVelMax.y = v + 50.0f;
	float k = speed * 3.0f + t;
	ps->initVelMax.z = k * 650.0f + 1450.0f;
	float ay = (float)VME_Rand(40.0, -20.0);
	float ax = (float)VME_Rand(40.0, -20.0);
	ps->initAccMin.z = 0.0f;
	ps->initAccMin.x = ax;
	ps->initAccMin.y = ay;
	ps->initAccMax = ps->initAccMin;
	ps->colorIni = 0xd9262626;
	ps->sizeIni = k * 75.0f + 150.0f;
	ps->colorEnd = 0xffffff;
	ps->sizeEnd = (speed * 1.5f + t) * 140.0f + 210.0f;
	CGameParticles_SetColorRampOn(ps, 0);
	CGameParticles_SetColorRamp(ps, VMEffects_SetupHazeRamp);
	ps->spriteIni = 1;
	ps->spriteEnd = 1;
	float rot = (float)VME_Rand(0x1.921fb6p+2, 0.0);
	ps->rotIni = rot;
	ps->rotEnd = rot;
	ps->rotVar = (float)VME_Rand(0x1.0c1524p+0, -0x1.0c1524p-1);
	ps->lifeTime = 0.6f - (float)VME_Rand(0x1.eb851cp-6, 0x1.99999ap-5) * t;
	CGameParticles_Apply(ps);
}

void cVehicleMotorsEffects_SetType(cVehicleMotorsEffects *self, int type) {
	uint32_t c;

	self->type = type;
	switch (type) {
	case 10:
	case 45:
		c = 0x2850fa;
		break;
	case 11:
		c = 0xd28c41;
		break;
	case 12:
		c = 0x0a50fa;
		break;
	case 13:
		c = 0x194beb;
		break;
	case 14:
		c = 0x4b4beb;
		break;
	case 15:
		c = 0x0a5afa;
		break;
	case 20:
		c = 0x5feb19;
		break;
	case 21:
		c = 0x19eb5f;
		break;
	case 25:
		c = 0xd2963c;
		break;
	case 26:
	case 40:
	case 55:
		c = 0xd24b14;
		break;
	case 27:
		c = 0xd75019;
		break;
	case 30:
		c = 0xd7ac48;
		break;
	case 35:
		c = 0xeb1e5f;
		break;
	case 36:
		c = 0xf02869;
		break;
	case 50:
		c = 0xe1781e;
		break;
	case 60:
		c = 0xc82814;
		break;
	default:
		return;
	}
	cVehicleMotorsEffects_SetColor(self, c);
}

void cVehicleMotorsEffects_Glow(cVehicleMotorsEffects *self, float thrust, float k, float dist,
	int *first, CGameParticles *ps) {
	float fade, size;
	int i, j;

	if (!self->type) {
		return;
	}
	self->glowOn = 1;
	float mult = g_FXEngineGlowDistFadeMult;
	float from = g_FXEngineGlowDistFadeFrom;
	float f = mult * from;
	if (f > dist) {
		fade = 0.0f;
	} else {
		float to = g_FXEngineGlowDistFadeTo;
		if (to * mult > dist) {
			fade = (dist - f) / ((to - from) * mult);
		} else {
			fade = 1.0f;
		}
	}
	float mid = g_FXEngineGlowDistFadeMid * mult;
	if (mid >= dist) {
		size = Float_MaxSS(40.0f, dist / mid * fade * g_FXEngineGlowSize);
	} else {
		float q = dist / mid;
		size = (q * q * g_FXEngineGlowSizeAtt + 1.0f) * fade * g_FXEngineGlowSize;
	}
	size = Float_MinSS(g_FXEngineGlowSizeMax, size);
	int a1 = (int)(fade * k * thrust * 255.0f);
	int a2 = (int)(fade * thrust * 255.0f);
	i = first ? *first : 0;
	float scale = self->scale;
	if (a1 > 0xf5) {
		a1 = 0xf5;
	}
	float s1 = scale * size;
	uint32_t c1 = (uint32_t)~a1 << 24 | (self->color & 0xffffff);
	if (a2 > 0xa2) {
		a2 = 0xa2;
	}
	float s2 = s1 * 1.5f;
	uint32_t c2 = (uint32_t)~a2 << 24 | (self->color2 & 0xffffff);
	float z1 = self->offset * scale;
	float z2 = (self->offset + 5.0f) * scale;
	for (j = 0; j < self->numExhausts; j++) {
		const vec3_t *p = &self->exhaustPos[self->exhaustSide[j]];
		CGameParticles_SetParticle(ps, i, p->x + 0.0f, p->y + 0.0f, p->z + z1, s1, c1, 2, 0.0f);
		CGameParticles_SetParticle(ps, i + 1, p->x + 0.0f, p->y + 0.0f, p->z + z2, s2, c2, 5, 0.0f);
		i += 2;
	}
	if (first) {
		*first = i;
	}
}

void cVehicleMotorsEffects_StopGlow(cVehicleMotorsEffects *self, int *first, CGameParticles *ps) {
	if (!self->glowOn || !self->type) {
		return;
	}
	self->glowOn = 0;
	int i = first ? *first : 0;
	if (self->numExhausts > 0) {
		int end = i + self->numExhausts * 2;
		CGameParticles_KillRange(ps, i, end);
		i = end;
	}
	if (first) {
		*first = i;
	}
}

/* color2: each channel c * c / 255 */
void cVehicleMotorsEffects_SetColor(cVehicleMotorsEffects *self, uint32_t color) {
	self->color = color;
	float r = (float)(int)(color >> 16 & 0xff) / 255.0f;
	float g = (float)(int)(color >> 8 & 0xff) / 255.0f;
	float b = (float)(int)(self->color & 0xff) / 255.0f;
	self->color2 = ((uint32_t)(int)(r * r * 255.0f) << 8 | (uint32_t)(int)(g * g * 255.0f)) << 8 |
		(uint32_t)(int)(b * b * 255.0f);
}

void cVehicleMotorsEffects_SetOffset(cVehicleMotorsEffects *self, float v) {
	self->offset = v;
}

void cVehicleMotorsEffects_SetScale(cVehicleMotorsEffects *self, float v) {
	self->scale = v;
}

void cVehicleMotorsEffects_Set(cVehicleMotorsEffects *self, cEntity *owner, cVehicleMotors *motors,
	cVehicleMotor *motor) {
	self->owner = owner;
	self->motors = motors;
	self->motor = motor;
}

void cVehicleMotorsEffects_SetVisible(cVehicleMotorsEffects *self, byte on) {
	int i, j;

	if (self->visible == on) {
		return;
	}
	for (i = 0; i < VM_SIDES; i++) {
		if (self->node[i][0] == -1) {
			continue;
		}
		CGameModel *model = cVehicleMotorsEffects_GetModel(self, i);
		if (!model || !CGameModel_IsEnabled(model)) {
			continue;
		}
		for (j = 0; j < 2; j++) {
			CGameParticles *p = self->particleSystem[i][j];
			if (p && p->ParticleSystem) {
				CGameParticles_SetEnabled(p, on);
				CGameParticles_SetVisible(self->particleSystem[i][j], on);
			}
		}
	}
	self->visible = on;
}

static uint32_t cVehicleMotorsEffects_SmokeRamps[18] = {
	0xff000000, 0x60191919, 0x60232323, 0x602d2d2d, 0x60282828, 0xa41e1e1e, 0xd40f0f0f, 0xff000000, 0xff000000,
	0xff000000, 0x80232323, 0x60323232, 0x60414141, 0x60373737, 0xa4282828, 0xd4141414, 0xff000000, 0xff000000,
};

void cVehicleMotorsEffects_Smoke(cVehicleMotorsEffects *self, float dt, CGameParticles *ps) {
	vec3_t v, rot, up, dir, r, mp, rd;
	int j;

	CHECK(self->owner);
	CHECK(ps && ps->ParticleSystem);
	float t = dt + self->smokeTime;
	self->smokeTime = t;
	if (0.015f > t) {
		return;
	}
	self->smokeTime = 0.0f;
	vec3_t *pv = Obj_GetVel(self->owner, &v);
	float vx = pv->x * 0.8f;
	float vy = pv->y * 0.8f;
	float vz = pv->z * 0.8f;
	CGameModel *model = Obj_GetModel(self->owner);
	CHECK(CGameModel_IsLoaded(model));
	CGameModel_GetRot(model, &rot);
	up.x = 0.0f;
	up.y = 0.0f;
	up.z = 1.0f;
	Vec3_Rotate(&dir, &up, &rot);
	for (j = 0; j < self->numExhausts; j++) {
		vec3_t *pr = Vec3_Rotate(&r, &self->exhaustPos[j], &rot);
		vec3_t *pm = CGameModel_GetPos(model, &mp);
		float bx = pr->x + pm->x;
		float by = pm->y + pr->y;
		float bz = pm->z + pr->z;
		if (!(8.0f > (float)VME_Rand(10.0, 0.0))) {
			continue;
		}
		ps->birthRate = 0;
		float s = (float)VME_Rand(10.0, 25.0);
		float ox = s * dir.x;
		float oy = s * dir.y;
		float oz = s * dir.z;
		float k = (float)VME_Rand(16.0, 4.0);
		vec3_t *pd = FX_RandomDir(&rd);
		ps->initPosMin.x = pd->x * k + bx + ox;
		ps->initPosMin.y = pd->y * k + by + oy;
		ps->initPosMin.z = pd->z * k + bz + oz;
		ps->initPosMax = ps->initPosMin;
		float vs = (float)VME_Rand(200.0, 1150.0);
		ps->initVelMin.x = vs * dir.x + vx;
		ps->initVelMin.y = vs * dir.y + vy;
		ps->initVelMin.z = vs * dir.z + vz;
		ps->initVelMax = ps->initVelMin;
		float a = (float)VME_Rand(0x1.9999ap-5, 0.5);
		ps->initAccMin.x = a * -ps->initVelMin.x + 0.0f;
		ps->initAccMin.y = a * -ps->initVelMin.y + 1980.0f;
		ps->initAccMin.z = a * -ps->initVelMin.z + 0.0f;
		ps->initAccMax = ps->initAccMin;
		ps->lifeTime = (float)VME_Rand(0x1.1eb85p-4, 0x1.1eb852p-2);
		ps->sizeIni = (float)VME_Rand(25.0, 200.0);
		ps->sizeLife = 1.0f;
		ps->sizeEnd = (float)VME_Rand(55.0, 270.0);
		float rt = (float)VME_Rand(0x1.921fb6p+2, 0.0);
		ps->rotIni = rt;
		ps->rotEnd = rt;
		ps->rotVar = (float)VME_Rand(0x1.0c1524p+0, -0x1.0c1524p-1);
		byte sprite;
		switch (crt_rand() % 3) {
		case 0:
			ps->spriteIni = 0x21;
			ps->spriteEnd = 0x25;
			break;
		case 1:
			sprite = (byte)((crt_rand() & 1) + 0x11);
			ps->spriteIni = sprite;
			ps->spriteEnd = sprite;
			break;
		case 2:
			sprite = (byte)((crt_rand() & 1) + 0x1c);
			ps->spriteIni = sprite;
			ps->spriteEnd = sprite;
			break;
		}
		CGameParticles_SetColorRampOn(ps, 1);
		CGameParticles_SetColorRamp(ps, cVehicleMotorsEffects_SmokeRamps + 9 * (crt_rand() & 1));
		CGameParticles_Apply(ps);
		CGameParticles_EmitN(ps, 1, 0);
	}
}

/* the trail point of exhaust j: its position turned by rot, at pos */
static vec3_t VME_TrailPoint(cVehicleMotorsEffects *self, int j, float zoff, const vec3_t *rot, const vec3_t *pos) {
	const vec3_t *e = &self->exhaustPos[self->exhaustSide[j]];
	vec3_t lp, r, p;

	lp.x = e->x + 0.0f;
	lp.y = e->y + 0.0f;
	lp.z = e->z + zoff;
	vec3_t *pr = Vec3_Rotate(&r, &lp, rot);
	p.x = pos->x + pr->x;
	p.y = pr->y + pos->y;
	p.z = pr->z + pos->z;
	return p;
}

void cVehicleMotorsEffects_Trails(cVehicleMotorsEffects *self, float dt, vec3_t pos, vec3_t rot,
	vec3_t vel, float thrust, float k, uint32_t color) {
	float d;
	int j;

	if (!self->trailsOn) {
		self->trailsOn = 1;
		return;
	}
	float t = Float_MaxSS(0.0f, thrust);
	if (0.01f >= t && 0.01f >= self->trailThrust) {
		if (self->dist > 0.0f) {
			for (j = 0; j < self->numExhausts; j++) {
				if (self->trail[j] && CGameTrail_IsRender(self->trail[j])) {
					break;
				}
			}
			if (j == self->numExhausts) {
				self->dist = 0.0f;
			}
		}
		if (self->dist == 0.0f) {
			return;
		}
	}
	Float_Approach(&self->trailThrust, t, dt + dt);
	Float_Approach(&self->segDist,
		(float)__builtin_fabs((double)(g_FXEngineTrailDistSpeedFactor * vel.z)) + g_FXEngineTrailDist, dt * 1500.0f);
	float zoff = g_SplitScreenGame ? g_FXEngineTrailOffset : 0.0f;
	float dy = pos.y - self->lastPos.y;
	float dx = pos.x - self->lastPos.x;
	float dz = pos.z - self->lastPos.z;
	float d2 = dy * dy + dx * dx + dz * dz;
	if (!(self->trailThrust >= 0.2f)) {
		d = d2;
		if (self->shrinkTime >= 0.05f) {
			for (j = 0; j < self->numExhausts; j++) {
				if (self->trail[j]) {
					CGameTrail_Shrink(self->trail[j]);
				}
			}
			self->shrinkTime = 0.0f;
		}
		self->shrinkTime = self->shrinkTime + dt;
	} else {
		d = (float)__builtin_sqrt((double)d2);
		int a = (int)(255.0f - (self->trailThrust - 0.2f) * t * 232.0f);
		float w = (g_FXEngineTrailSize * 0.25f * k + g_FXEngineTrailSize) * self->scale;
		uint32_t c = (uint32_t)a << 24 | (color & 0xffffff);
		if (0.2f > self->lastThrust) {
			for (j = 0; j < self->numExhausts; j++) {
				if (!self->trail[j]) {
					continue;
				}
				vec3_t p = VME_TrailPoint(self, j, zoff, &rot, &pos);
				vec3_t p1 = { p.x + 1e-4f, p.y + 1e-4f, p.z + 1e-4f };
				CGameTrail_Add(self->trail[j], p1, w, c | 0xff000000);
				vec3_t p2 = { p.x + 2e-4f, p.y + 2e-4f, p.z + 2e-4f };
				CGameTrail_Add(self->trail[j], p2, w, c | 0xff000000);
			}
		}
		if (d > self->segDist) {
			self->lastPos = pos;
			Float_Approach(&self->colorBlend, color == 0xffffff ? 0.0f : 1.0f, dt * 0.5f);
			for (j = 0; j < self->numExhausts; j++) {
				if (!self->trail[j]) {
					continue;
				}
				vec3_t p = VME_TrailPoint(self, j, zoff, &rot, &pos);
				uint32_t col = c | 0xff000000;
				CGameTrail_SetLast(self->trail[j], NULL, NULL, &col);
				CGameTrail_SetPoint(self->trail[j], 1, NULL, NULL, &c);
				CGameTrail_Add(self->trail[j], p, w, col);
				CGameTrail_SetSegments(self->trail[j], (byte)(12 - (int)(self->colorBlend * -18.0f)));
			}
		} else {
			for (j = 0; j < self->numExhausts; j++) {
				if (!self->trail[j]) {
					continue;
				}
				VME_TrailPoint(self, j, zoff, &rot, &pos);
				uint32_t a8 = c >> 24;
				uint32_t c2 = (a8 - (int)((d / self->segDist - 1.0f) * (float)(int)(0xff - a8))) << 24 | (c & 0xffffff);
				CGameTrail_SetPoint(self->trail[j], 1, NULL, NULL, &c2);
			}
		}
	}
	self->lastThrust = self->trailThrust;
	self->dist = d;
}

/* ---- cVehicleMotorsEffects_Exhaust: the exhaust particles of the FxClass types ---- */

#define VME_PI_2 1.5707964f
#define VME_PI_4 0.7853982f

/* the function statics of the original: the guards and the emit positions they make once */
#define VME_GUARD_10 0x448510
#define VME_POS_10 0x4484e0	/* [4] */
#define VME_GUARD_12 0x448560
#define VME_POS_12 0x448518	/* [6] */
#define VME_GUARD_13 0x44857c
#define VME_POS_13 0x448564	/* [2] */
#define VME_GUARD_14 0x4485b0
#define VME_POS_14 0x448580	/* [4] */
#define VME_POS_15 0x4485b8	/* [8], made on every call */
#define VME_GUARD_27 0x448648
#define VME_POS_27 0x448618	/* [4] */
#define VME_GUARD_50 0x4486b0
#define VME_POS_50 0x448650	/* [8] */
#define VME_GUARD_60 0x4486d8
#define VME_POS_60 0x4486b4	/* [3] */

/* what every type reads */
typedef struct VMExhaust {
	cVehicleMotorsEffects *self;
	CGameModel *model;		/* of the side, NULL for a motor without the node */
	CGameParticles *ps;		/* the particles of node 0 of the side */
	CGameParticles *ps2;		/* of node 1, only type 21 uses them */
	float t;			/* thrust * f4bc */
	float k;
	float speed;
	byte cloud;
} VMExhaust;

/* the placed particles show while the motors run, with a cloud only one frame in four */
static byte VME_Show(float speed, byte cloud) {
	if (cloud) {
		return (crt_rand() & 3) == 3;
	}
	return speed > 0.0f;
}

/* the birth rate of thrust t: base at t 0, 100 more at t 1 */
static int VME_Birth(float t, float speed, int base) {
	return (int)((float)((int)((float)(100 - g_FXEngineBirthRate) * Float_MaxSS(t, 0.0f)) + base) * speed);
}

/* the birth rate n scaled to 0..0.35 while the ship leaves a cloud (the older types through VME_Rand) */
static int VME_CloudBirthD(int n, double mul) {
	float r = (float)VME_Rand(mul, 0.0);
	return (int)((float)(double)(uint32_t)n * r);
}

static int VME_CloudBirth(int n) {
	float r = Rand_FloatRange(0.0f, 0.35f);
	return (int)(r * (float)(double)(uint32_t)n);
}

/* colorIni and colorEnd from the table of the type (10 colors) by t + k */
static void VME_Colors(CGameParticles *ps, const uint32_t *tab, float t, float k) {
	int i = (int)(__builtin_fabs((double)(t + k)) * 4.0);

	ps->colorIni = tab[i];
	ps->colorEnd = tab[i + 1];
}

/* rotIni = rotEnd at random, rotVar in lo..hi */
static void VME_RotSame(CGameParticles *ps, float lo, float hi) {
	float r = Rand_FloatRange(0.0f, M_2PI_F);
	ps->rotIni = r;
	ps->rotEnd = r;
	ps->rotVar = Rand_FloatRange(lo, hi);
}

/* rotIni at random, rotEnd up to pi / 2 off rotIni of base */
static void VME_RotTurn(CGameParticles *ps, const CGameParticles *base, float var) {
	ps->rotIni = Rand_FloatRange(0.0f, M_2PI_F);
	ps->rotEnd = Rand_FloatRange(-VME_PI_2, VME_PI_2) + base->rotIni;
	ps->rotVar = var;
}

/* the life time c - (lo..hi) * t */
static float VME_Life(float c, float lo, float hi, float t) {
	return (float)(c - (Rand_FloatRange(lo, hi)) * t);
}

static inline uint32_t VME_Alpha(float a, uint32_t rgb) {
	return (uint32_t)(-1 - (int)a) << 24 | rgb;
}

static vec3_t VME_Turn(const vec3_t *rot, float x, float y, float z) {
	vec3_t a = { x, y, z }, r;

	Vec3_Rotate(&r, &a, rot);
	return r;
}

/* the axis x y z of the model */
static vec3_t VME_Axis(CGameModel *model, float x, float y, float z) {
	vec3_t rot;

	return VME_Turn(CGameModel_GetRot(model, &rot), x, y, z);
}

/* the static emit positions (x, y, z[i]) */
static void VME_SetPos(vec3_t *pos, const float *xz, int n, float y) {
	int i;

	for (i = 0; i < n; i++) {
		pos[i].x = xz[i * 2];
		pos[i].y = y;
		pos[i].z = xz[i * 2 + 1];
	}
}

/* the guard of a static of the original: 1 when this call makes it, then VME_StaticEnd */
static byte VME_StaticBegin(int *g) {
	if (*g > crt_InitEpoch()) {
		crt_Init_thread_header(g);
		return *g == -1;
	}
	return 0;
}

static void VME_StaticEnd(int *g) {
	crt_Init_thread_footer(g);
}

/* n particles along dir from first: at distance i * m * dk + d0, size (c0 - i * cs) * s * sk + s0 */
static void VME_Line(CGameParticles *ps, int first, int n, vec3_t dir, float m, float dk, float d0, int c0, int cs,
	float s, float sk, float s0, uint32_t col) {
	int i;

	for (i = 0; i < n; i++) {
		float d = (float)i * m * dk + d0;
		float size = (float)(c0 - i * cs) * s * sk + s0;
		CGameParticles_SetParticle(ps, first + i, d * dir.x, d * dir.y, d * dir.z, size, col, 1, 0.0f);
	}
}

/* two pairs along dir, beside it at v * vm and v * -vm (first + c, first + 2 + c) */
static void VME_Pair(CGameParticles *ps, int first, vec3_t dir, vec3_t v, float vm, float m, float dk, float d0,
	float s, float sk, float s0, uint32_t col) {
	int c;

	for (c = 0; c < 2; c++) {
		float d = (float)c * m * dk + d0;
		float size = (float)(2 - c) * s * sk + s0;
		float px = dir.x * d;
		float py = dir.y * d;
		float pz = dir.z * d;
		CGameParticles_SetParticle(ps, first + c, v.x * vm + px, v.y * vm + py, v.z * vm + pz, size, col, 1,
			0.0f);
		CGameParticles_SetParticle(ps, first + 2 + c, v.x * -vm + px, v.y * -vm + py, v.z * -vm + pz, size, col,
			1, 0.0f);
	}
}

/* two quads along dir, at a * vm, a * -vm, b * vm, b * -vm beside it, smaller backwards */
static void VME_Quad(CGameParticles *ps, int first, vec3_t dir, vec3_t a, vec3_t b, float vm, float t, float dk,
	float d0, float s, float sk, float s0, uint32_t col) {
	int c;

	for (c = 0; c < 2; c++) {
		float d = (float)c * t * dk + d0;
		float px = dir.x * d;
		float py = dir.y * d;
		float pz = dir.z * d;
		float f = 0.0f > t ? t + 1.0f : 1.0f;
		float size = ((float)(2 - c) * s * sk + s0) * f;
		CGameParticles_SetParticle(ps, first + c, a.x * vm + px, a.y * vm + py, a.z * vm + pz, size, col, 1, 0.0f);
		CGameParticles_SetParticle(ps, first + 2 + c, a.x * -vm + px, a.y * -vm + py, a.z * -vm + pz, size, col, 1,
			0.0f);
		CGameParticles_SetParticle(ps, first + 4 + c, b.x * vm + px, b.y * vm + py, b.z * vm + pz, size, col, 1,
			0.0f);
		CGameParticles_SetParticle(ps, first + 6 + c, b.x * -vm + px, b.y * -vm + py, b.z * -vm + pz, size, col, 1,
			0.0f);
	}
}

/* a particle at each emit position (x * sx, z, y) turned by the model */
static void VME_Ring(CGameParticles *ps, CGameModel *model, int first, const vec3_t *pos, int n, float sx, float y,
	float size, uint32_t col) {
	vec3_t v, rot, r;
	int i;

	for (i = 0; i < n; i++) {
		v.x = pos[i].x * sx;
		v.y = pos[i].z;
		v.z = y;
		Vec3_Rotate(&r, &v, CGameModel_GetRot(model, &rot));
		CGameParticles_SetParticle(ps, first + i, r.x, r.y, r.z, size, col, 1, 0.0f);
	}
}

/* the flare of k at the exhaust */
static void VME_Flare(CGameParticles *ps, int i, float k, float size, uint32_t rgb, int sprite) {
	if (k > 0.0f) {
		CGameParticles_SetParticle(ps, i, 0.0f, 0.0f, 0.0f, k * size, VME_Alpha(k * 255.0f, rgb), sprite, 0.0f);
	} else {
		CGameParticles_Kill(ps, i);
	}
}

/* the flare at dm along the z axis of the model */
static void VME_FlareDir(CGameParticles *ps, CGameModel *model, int i, float k, float dm, float size, uint32_t rgb,
	int sprite) {
	if (k > 0.0f) {
		vec3_t d = VME_Axis(model, 0.0f, 0.0f, 1.0f);
		CGameParticles_SetParticle(ps, i, d.x * dm, d.y * dm, d.z * dm, k * size, VME_Alpha(k * 255.0f, rgb), sprite,
			0.0f);
	} else {
		CGameParticles_Kill(ps, i);
	}
}

/* two flares 30 along the z axis, at v * vm and v * -vm beside it */
static void VME_FlarePair(CGameParticles *ps, CGameModel *model, int first, float k, float vx, float vy, float vz,
	float vm, float size, uint32_t rgb) {
	if (k > 0.0f) {
		vec3_t d = VME_Axis(model, 0.0f, 0.0f, 1.0f);
		vec3_t v = VME_Axis(model, vx, vy, vz);
		uint32_t col = VME_Alpha(k * 255.0f, rgb);
		float s = k * size;
		CGameParticles_SetParticle(ps, first, d.x * 30.0f + v.x * vm, d.y * 30.0f + v.y * vm, d.z * 30.0f + v.z * vm,
			s, col, 6, 0.0f);
		CGameParticles_SetParticle(ps, first + 1, d.x * 30.0f + v.x * -vm, d.y * 30.0f + v.y * -vm,
			d.z * 30.0f + v.z * -vm, s, col, 6, 0.0f);
	} else {
		CGameParticles_KillRange(ps, first, first + 2);
	}
}

/* 1: four particles along the z axis in the color of the motor */
static void VME_Exhaust1(const VMExhaust *x) {
	CGameParticles *ps = x->ps;
	cVehicleMotorsEffects *self = x->self;
	int i;

	CGameParticles_NumParticles(ps);
	if (!VME_Show(x->speed, x->cloud)) {
		CGameParticles_KillRange(ps, 0, 4);
		return;
	}
	vec3_t dir = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
	float a = x->speed * 32.0f;
	float s = (x->k + x->k + x->t) * 60.0f;
	for (i = 0; i < 4; i++) {
		float r = (float)VME_Rand(10.0, 20.0);
		float scale = self->scale;
		float d = (float)i * x->t * 40.0f * scale + r * scale + self->offset;
		uint32_t col = (uint32_t)(i * 30 - (int)((float)(4 - i) * a) - 0x5b) << 24 | (self->color & 0xffffff);
		CGameParticles_SetParticle(ps, i, dir.x * d, dir.y * d, dir.z * d, ((float)(0xf5 - i * 0x41) + s) * scale,
			col, 1, 0.0f);
	}
}

/* 5: five particles along the y axis, a flare and the emitted smoke */
static void VME_Exhaust5(const VMExhaust *x) {
	CGameParticles *ps = x->ps;
	float t = x->t, k = x->k;
	vec3_t rot;
	int i;

	CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		CGameModel_GetRot(x->model, &rot);
		vec3_t dir = VME_Axis(x->model, 0.0f, 1.0f, 0.0f);
		float a = x->speed * 48.0f;
		float s = k + k;
		for (i = 0; i < 5; i++) {
			float r = (float)VME_Rand(5.0, 20.0);
			float fi = (float)i;
			float d = r + (fi * t * 11.0f - 20.0f + fi * k * 100.0f);
			uint32_t col = VME_Alpha((float)(4 - i) * a, x->self->color & 0xffffff);
			CGameParticles_SetParticle(ps, i, dir.x * d, dir.y * d, dir.z * d, s * fi * 50.0f + 90.0f, col, 1, 0.0f);
		}
	} else {
		CGameParticles_KillRange(ps, 0, 5);
	}
	if (k > 0.0f) {
		float m = k - 1.0f;
		uint32_t col = (uint32_t)(0xb4 - (int)(m * -128.0f)) << 8 | (uint32_t)(0x37 - (int)(m * 100.0f)) << 24 |
			(uint32_t)(0xff - (int)(m * -32.0f)) << 16 | 0x3c;
		CGameParticles_SetParticle(ps, 5, 0.0f, 80.0f, 0.0f, k * k * k * 900.0f, col, 1, 0.0f);
	} else {
		CGameParticles_Kill(ps, 5);
	}
	ps->birthRate = (int)(Float_MaxSS(k, 0.0f) * 60.0f * x->speed);
	if (x->cloud) {
		ps->birthRate = VME_CloudBirthD(ps->birthRate, (double)0.35f);
	}
	float k40 = k * 40.0f;
	ps->initPosMin = (vec3_t){ -10.0f, k40 + 15.0f, -10.0f };
	ps->initPosMax = (vec3_t){ 10.0f, k40 + 20.0f, 10.0f };
	ps->sizeIni = (k + k) * 55.0f + 130.0f;
	ps->sizeEnd = k * 1.5f * 30.0f + 80.0f;
	float a = (float)VME_Rand(32.0, 64.0);
	float r = (float)VME_Rand(55.0, 200.0);
	float h = k * 0.5f + 0.5f;
	float g = (float)VME_Rand(64.0, 64.0) * h;
	ps->colorIni = (((uint32_t)(int)a << 8 | (uint32_t)(int)r) << 8 | (uint32_t)(int)g) << 8;
	a = (float)VME_Rand(16.0, 16.0);
	r = (float)VME_Rand(55.0, 200.0);
	g = (float)VME_Rand(64.0, 64.0) * h;
	ps->colorEnd = (((uint32_t)(int)a << 8 | (uint32_t)(int)r) << 8 | (uint32_t)(int)g) << 8;
	float k150 = k * 150.0f;
	float k3 = k * 3.0f;
	float v = -200.0f - k150;
	ps->initVelMin = (vec3_t){ v, k * 300.0f + 850.0f, k * 250.0f + 500.0f };
	ps->initVelMax = (vec3_t){ k150 + 200.0f, k3 * 700.0f + 1250.0f, k * 500.0f + 750.0f };
	ps->initAccMin = (vec3_t){ v, v, k * 9500.0f + 15000.0f };
	ps->initAccMax = (vec3_t){ v, v, k3 * 9500.0f + 15000.0f };
	ps->spriteIni = 9;
	ps->spriteEnd = 0xb;
	float rt = (float)VME_Rand((double)M_2PI_F, 0.0);
	ps->rotIni = rt;
	ps->rotEnd = rt;
	ps->rotVar = (float)VME_Rand((double)VME_PI_2, -(double)VME_PI_4);
	ps->lifeTime = 0.14f - (float)VME_Rand((double)0.030000001f, (double)0.02f) * t;
	CGameParticles_Apply(ps);
}

/* 6: two quads of particles along the z axis */
static void VME_Exhaust6(const VMExhaust *x) {
	CGameParticles *ps = x->ps;
	int n = CGameParticles_NumParticles(ps);

	if (!VME_Show(x->speed, x->cloud)) {
		CGameParticles_KillRange(ps, n - 8, n);
		return;
	}
	vec3_t l = VME_Axis(x->model, -1.0f, 0.45f, 0.0f);
	vec3_t r = VME_Axis(x->model, 1.0f, 0.45f, 0.0f);
	vec3_t d = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
	VME_Quad(ps, n - 8, d, l, r, 45.0f, x->t, 40.0f, 12.0f, x->k * 4.0f + x->t, 20.0f, 75.0f,
		VME_Alpha(x->speed * 130.0f, 0xff0f0f));
}

/* 10: the emitted smoke, two pairs beside the z axis and a flare */
static const uint32_t VME_Exhaust10Table[10] = {
	0x320a280f, 0x30191919, 0x2d0f2332, 0x2c0a3264, 0x2a143796, 0x282850fa, 0x283264fa, 0x23466efb, 0x1e5a82fd,
	0x146496ff,
};

static vec3_t VME_Exhaust10Pos[4];
static int VME_Exhaust10Table20[2];

static void VME_Exhaust10(const VMExhaust *x) {
	static const float xz[] = { -30.0f, 0.0f, 30.0f, 0.0f, -10.0f, 0.0f, 10.0f, 0.0f };
	CGameParticles *ps = x->ps;
	vec3_t *pos = VME_Exhaust10Pos;
	float t = x->t, k = x->k;

	ps->birthRate = VME_Birth(t, x->speed, g_FXEngineBirthRate);
	if (x->cloud) {
		ps->birthRate = VME_CloudBirthD(ps->birthRate, (double)0.15f);
	}
	float r = (float)VME_Rand(10.0, 25.0);
	if (VME_StaticBegin(VME_Exhaust10Table20)) {
		VME_SetPos(pos, xz, 4, r + 0.0f);
		VME_StaticEnd(VME_Exhaust10Table20);
	}
	CGameParticles_SetEmitPos(ps, pos, 4);
	float s = k + k + t;
	ps->sizeIni = s * 80.0f + 120.0f;
	ps->sizeEnd = (t + k) * 40.0f + 70.0f;
	VME_Colors(ps, VME_Exhaust10Table, t, k);
	if (t > 0.0f) {
		ps->initVelMin = (vec3_t){ -30.0f, s * 300.0f + 900.0f, -30.0f };
		ps->initVelMax = (vec3_t){ 30.0f, s * 600.0f + 1200.0f, 30.0f };
	} else {
		ps->initVelMin = (vec3_t){ 0.0f, 50.0f, 0.0f };
		ps->initVelMax = (vec3_t){ 0.0f, 100.0f, 0.0f };
	}
	ps->spriteIni = 9;
	ps->spriteEnd = 0xc;
	ps->rotIni = (float)VME_Rand((double)M_2PI_F, 0.0);
	ps->rotVar = VME_PI_4;
	ps->rotEnd = ps->rotIni + (float)VME_Rand((double)M_2PI_F, -(double)M_PI_F);
	ps->lifeTime = 0.14f - (float)VME_Rand((double)0.01f, (double)0.01f) * t;
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		vec3_t d = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
		vec3_t v = VME_Axis(x->model, 1.0f, 0.0f, 0.0f);
		VME_Pair(ps, n - 4, d, v, 35.0f, t, 85.0f, 60.0f, k * 4.0f + t, 60.0f, 200.0f,
			VME_Alpha(x->speed * 130.0f, 0x1e32d2));
	} else {
		CGameParticles_KillRange(ps, n - 4, n);
	}
	VME_Flare(ps, n - 5, k, 750.0f, 0x1e32d2, 7);
}

static const uint32_t VME_Exhaust10Table2[10] = {
	0x320f0f28, 0x30191919, 0x2d32230f, 0x2c64320a, 0x2ac85500, 0x28fa6419, 0x28fa6432, 0x23fb6e46, 0x1efd825a,
	0x14ff9664,
};

/* 11: the emitted smoke, four particles along the z axis and a flare */
static void VME_Exhaust11(const VMExhaust *x) {
	CGameParticles *ps = x->ps;
	float t = x->t, k = x->k;
	int i;

	ps->birthRate = VME_Birth(t, x->speed, g_FXEngineBirthRate);
	if (x->cloud) {
		ps->birthRate = VME_CloudBirthD(ps->birthRate, (double)0.35f);
	}
	float k50 = k * 50.0f;
	ps->initPosMin = (vec3_t){ -8.0f, k50 + 20.0f, -8.0f };
	ps->initPosMax = (vec3_t){ 8.0f, k50 + 25.0f, 8.0f };
	float s = k * 3.5f + t;
	ps->sizeIni = s * 80.0f + 145.0f;
	ps->sizeEnd = (k + k + t) * 30.0f + 60.0f;
	VME_Colors(ps, VME_Exhaust10Table2, t, k);
	if (t > 0.0f) {
		float a = k * 150.0f;
		ps->initVelMin = (vec3_t){ -200.0f - a, (t + k) * 250.0f + 1125.0f, -200.0f - a };
		ps->initVelMax = (vec3_t){ a + 200.0f, (k * 3.0f + t) * 850.0f + 1400.0f, a + 200.0f };
	} else {
		ps->initVelMin = (vec3_t){ 0.0f, 100.0f, 0.0f };
		ps->initVelMax = (vec3_t){ 0.0f, 150.0f, 0.0f };
	}
	ps->spriteIni = 9;
	ps->spriteEnd = 0xc;
	ps->rotIni = (float)VME_Rand((double)M_2PI_F, 0.0);
	ps->rotVar = M_2PI_F;
	ps->rotEnd = ps->rotIni + (float)VME_Rand((double)M_PI_F, -(double)VME_PI_2);
	ps->lifeTime = 0.15f - (float)VME_Rand((double)0.030000001f, (double)0.02f) * t;
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		vec3_t dir = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
		for (i = 0; i < 4; i++) {
			float d = (float)i * t * 50.0f + 50.0f;
			uint32_t col = VME_Alpha((float)(0x80 - i * 0x19) * x->speed, 0x053c96);
			CGameParticles_SetParticle(ps, n - 4 + i, d * dir.x, d * dir.y, d * dir.z, (float)(3 - i) * s * 75.0f + 160.0f,
				col, 1, 0.0f);
		}
	} else {
		CGameParticles_KillRange(ps, n - 4, n);
	}
	VME_Flare(ps, n - 5, k, 850.0f, 0xe66414, 6);
}

/* 12: the emitted smoke at six positions, a particle at each and a flare */
static const uint32_t VME_Exhaust10Table3[10] = {
	0x321e1e0f, 0x30191919, 0x2d0f2332, 0x2c0a3264, 0x32143796, 0x640a50fa, 0x643264fa, 0x37466efb, 0x1e5a82fd,
	0x646496ff,
};

static vec3_t VME_Exhaust10Pos2[6];
static int VME_Exhaust10Count;

static void VME_Exhaust12(const VMExhaust *x) {
	static const float xz[] = { -26.0f, -26.0f, 26.0f, 26.0f, 26.0f, -26.0f, -26.0f, 26.0f, 0.0f, -26.0f, 0.0f, 26.0f };
	CGameParticles *ps = x->ps;
	vec3_t *pos = VME_Exhaust10Pos2;
	float t = x->t, k = x->k;

	ps->birthRate = VME_Birth(t, x->speed, 0) * 2;
	if (x->cloud) {
		ps->birthRate = VME_CloudBirthD(ps->birthRate, (double)0.35f) * 2;
	}
	float r = (float)VME_Rand(10.0, 15.0) + k * 35.0f;
	if (VME_StaticBegin((&VME_Exhaust10Count))) {
		VME_SetPos(pos, xz, 6, r + 0.0f);
		VME_StaticEnd((&VME_Exhaust10Count));
	}
	CGameParticles_SetEmitPos(ps, pos, 4);
	float k5 = k * 5.0f;
	ps->sizeIni = (k * 4.0f + t) * 65.0f + 75.0f;
	ps->sizeEnd = (k * 2.5f + t) * 45.0f + 40.0f;
	VME_Colors(ps, VME_Exhaust10Table3, t, k);
	ps->initVelMin = (vec3_t){ -6.0f - k5, (k * 3.0f + t) * 250.0f + 800.0f, -6.0f - k5 };
	ps->initVelMax = (vec3_t){ k5 + 6.0f, (k * 6.0f + t) * 600.0f + 1250.0f, k5 + 6.0f };
	ps->spriteIni = 0xd;
	ps->spriteEnd = 0xf;
	float rt = (float)VME_Rand((double)M_2PI_F, 0.0);
	ps->rotIni = rt;
	ps->rotEnd = rt;
	ps->rotVar = (float)VME_Rand((double)2.0943952f, -(double)1.0471976f);
	ps->lifeTime = 0.11f - (float)VME_Rand((double)0.010000001f, (double)0.015f) * t;
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		float m = Float_MaxSS(0.0f, t);
		VME_Ring(ps, x->model, n - 6, pos, 6, 1.3f, m * 10.0f, (k5 + m) * 90.0f + 110.0f,
			VME_Alpha(x->speed * 130.0f, 0x2828b4));
	} else {
		CGameParticles_KillRange(ps, n - 6, n);
	}
	VME_FlareDir(ps, x->model, n - 7, k, 40.0f, 900.0f, 0xb4b4f0, 6);
}

/* 13: the emitted smoke at two random positions, two pairs and two flares */
static const uint32_t VME_Exhaust10Table4[10] = {
	0x32280f0f, 0x30191919, 0x2d0f2332, 0x2c0a2d80, 0x2a0f41e1, 0x28194beb, 0x282855f8, 0x23325ffa, 0x1e3c69fd,
	0x144678ff,
};

static vec3_t VME_Exhaust10Pos3[2];
static int VME_Exhaust10Count2;

static void VME_Exhaust13(const VMExhaust *x) {
	CGameParticles *ps = x->ps;
	vec3_t *pos = VME_Exhaust10Pos3;
	float t = x->t, k = x->k;

	ps->birthRate = VME_Birth(t, x->speed, 0) * 3;
	if (x->cloud) {
		ps->birthRate = VME_CloudBirthD(ps->birthRate, (double)0.35f) * 3;
	}
	float pz = (float)VME_Rand(6.0, -3.0);
	float py = (float)VME_Rand(5.0, 30.0);
	float px = (float)VME_Rand(6.0, -3.0);
	py = k * 50.0f + py;
	if (VME_StaticBegin((&VME_Exhaust10Count2))) {
		pos[0] = (vec3_t){ px + 0.0f, py + 0.0f, pz - 40.0f };
		pos[1] = (vec3_t){ px + 0.0f, py + 0.0f, pz + 40.0f };
		VME_StaticEnd((&VME_Exhaust10Count2));
	}
	CGameParticles_SetEmitPos(ps, pos, 2);
	float k4 = k * 4.0f;
	ps->sizeIni = (k4 + t) * 50.0f + 65.0f;
	ps->sizeEnd = (k * 1.5f + t) * 20.0f + 40.0f;
	VME_Colors(ps, VME_Exhaust10Table4, t, k);
	float a = k * 150.0f;
	ps->initVelMin = (vec3_t){ -200.0f - a, (t + k) * 330.0f + 1100.0f, -200.0f - a };
	ps->initVelMax = (vec3_t){ a + 200.0f, (k + k + t) * 600.0f + 1600.0f, a + 200.0f };
	ps->spriteIni = 9;
	ps->spriteEnd = 0xb;
	ps->rotIni = (float)VME_Rand((double)M_2PI_F, 0.0);
	ps->rotVar = M_2PI_F;
	ps->rotEnd = ps->rotIni + (float)VME_Rand((double)M_PI_F, -(double)VME_PI_2);
	ps->lifeTime = 0.14f - (float)VME_Rand((double)0.01f, (double)0.03f) * t;
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		float m = Float_MaxSS(0.0f, t);
		vec3_t d = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
		vec3_t v = VME_Axis(x->model, 1.0f, 0.0f, 0.0f);
		VME_Pair(ps, n - 4, d, v, 40.0f, m, 40.0f, m * 25.0f + 15.0f, m + k4, 95.0f, 95.0f,
			VME_Alpha(x->speed * 140.0f, 0x3c46d7));
	} else {
		CGameParticles_KillRange(ps, n - 4, n);
	}
	VME_FlarePair(ps, x->model, n - 6, k, 1.0f, 0.0f, 0.0f, 40.0f, 800.0f, 0x1464e6);
}

/* 14: the emitted smoke at four positions, two quads and a flare */
static const uint32_t VME_Exhaust10Table5[10] = {
	0x320f0f19, 0x30191923, 0x2d23233c, 0x2c2d2d80, 0x2a4141e1, 0x284b4beb, 0x285555f8, 0x235f5ffa, 0x1e6969fd,
	0x147878ff,
};

static vec3_t VME_Exhaust10Pos4[4];
static int VME_Exhaust10Table21[2];

static void VME_Exhaust14(const VMExhaust *x) {
	static const float xz[] = { 0.0f, -65.0f, 0.0f, 65.0f, -65.0f, 0.0f, 65.0f, 0.0f };
	CGameParticles *ps = x->ps;
	vec3_t *pos = VME_Exhaust10Pos4;
	float t = x->t, k = x->k;

	ps->birthRate = VME_Birth(t, x->speed, 0) * 3;
	if (x->cloud) {
		ps->birthRate = VME_CloudBirth(ps->birthRate) * 3;
	}
	float r = Rand_FloatRange(15.0f, 20.0f) + k * 50.0f;
	if (VME_StaticBegin(VME_Exhaust10Table21)) {
		VME_SetPos(pos, xz, 4, r + 0.0f);
		VME_StaticEnd(VME_Exhaust10Table21);
	}
	CGameParticles_SetEmitPos(ps, pos, 4);
	float s = k * 4.0f + t;
	ps->sizeIni = s * 50.0f + 60.0f;
	ps->sizeEnd = (k * 1.5f + t) * 40.0f + 35.0f;
	VME_Colors(ps, VME_Exhaust10Table5, t, k);
	float a = k * 30.0f;
	ps->initVelMin = (vec3_t){ -20.0f - a, (t + k) * 120.0f + 800.0f, -20.0f - a };
	ps->initVelMax = (vec3_t){ a + 20.0f, (k * 3.0f + t) * 650.0f + 1700.0f, a + 20.0f };
	ps->spriteIni = 0xd;
	ps->spriteEnd = 0x10;
	VME_RotSame(ps, -VME_PI_2, VME_PI_2);
	ps->lifeTime = VME_Life(0.14f, 0.02f, 0.05f, t);
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		vec3_t rot;
		CGameModel_GetRot(x->model, &rot);
		vec3_t ax = VME_Turn(&rot, 1.0f, 0.0f, 0.0f);
		vec3_t ay = VME_Turn(&rot, 0.0f, 1.0f, 0.0f);
		vec3_t az = VME_Turn(&rot, 0.0f, 0.0f, 1.0f);
		VME_Quad(ps, n - 8, az, ax, ay, 65.0f, t, 40.0f, t * 25.0f + 5.0f, s, 65.0f, 90.0f,
			VME_Alpha(x->speed * 160.0f, 0x0a5ae6));
	} else {
		CGameParticles_KillRange(ps, n - 8, n);
	}
	VME_FlareDir(ps, x->model, n - 9, k, 40.0f, 1200.0f, 0x50a0e6, 7);
}

/* 15: the emitted smoke on a circle of eight positions, a particle at each and a flare */
static const uint32_t VME_Exhaust10Table6[10] = {
	0x320f0f19, 0x30191923, 0x2d23233c, 0x2c2d2d80, 0x2a4141e1, 0x284b4beb, 0x285555f8, 0x235f5ffa, 0x1e6969fd,
	0x147878ff,
};

static vec3_t VME_Exhaust10Pos5[8];

static void VME_Exhaust15(const VMExhaust *x) {
	CGameParticles *ps = x->ps;
	vec3_t *pos = VME_Exhaust10Pos5;
	float t = x->t, k = x->k;
	vec3_t ang, v, r;
	int i;

	ps->birthRate = VME_Birth(t, x->speed, g_FXEngineBirthRate) * 3;
	if (x->cloud) {
		ps->birthRate = VME_CloudBirth(ps->birthRate) * 3;
	}
	float y = Rand_FloatRange(5.0f, 10.0f) + k * 10.0f;
	float a = 0.0f;
	for (i = 1;; i++) {
		ang = (vec3_t){ 0.0f, a, 0.0f };
		v = (vec3_t){ 0.0f, 0.0f, (float)((i & 1) ? 30 : -30) };
		Vec3_Rotate(&r, &v, &ang);
		pos[i - 1].x = r.x + 0.0f;
		pos[i - 1].z = r.z + 0.0f;
		pos[i - 1].y = r.y + y;
		if (i >= 8) {
			break;
		}
		a = a + VME_PI_4;
		if (!(M_2PI_F > a)) {
			break;
		}
	}
	CGameParticles_SetEmitPos(ps, pos, 8);
	float s = k * 3.0f + t;
	ps->sizeIni = s * 45.0f + 45.0f;
	ps->sizeEnd = (k * 1.5f + t) * 30.0f + 35.0f;
	VME_Colors(ps, VME_Exhaust10Table6, t, k);
	if (t > 0.0f) {
		float b = k * 20.0f;
		ps->initVelMin = (vec3_t){ -15.0f - b, (t + k) * 150.0f + 1200.0f, -15.0f - b };
		ps->initVelMax = (vec3_t){ b + 15.0f, s * 800.0f + 2500.0f, b + 15.0f };
	} else {
		ps->initVelMin = (vec3_t){ 0.0f, 100.0f, 0.0f };
		ps->initVelMax = (vec3_t){ 0.0f, 300.0f, 0.0f };
	}
	ps->spriteIni = 0xd;
	ps->spriteEnd = 0x10;
	ps->rotIni = (float)VME_Rand((double)M_2PI_F, 0.0);
	ps->rotVar = M_2PI_F;
	ps->rotEnd = (float)VME_Rand((double)M_PI_F, -(double)VME_PI_2) + ps->rotIni;
	ps->lifeTime = 0.15f - (float)VME_Rand((double)0.030000001f, (double)0.02f) * t;
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		float m = Float_MaxSS(0.0f, t);
		VME_Ring(ps, x->model, n - 8, pos, 8, 1.0f, m * 10.0f, (k * 5.0f + m) * 65.0f + 60.0f,
			VME_Alpha(x->speed * 130.0f, 0x1e32d2));
	} else {
		CGameParticles_KillRange(ps, n - 8, n);
	}
	VME_FlareDir(ps, x->model, n - 9, k, 40.0f, 700.0f, 0x1e3cd2, 5);
}

static const uint32_t VME_Exhaust10Table7[10] = {
	0x320f0f28, 0x30191919, 0x2d23320f, 0x2c2d800a, 0x2a50e100, 0x285feb19, 0x2864f832, 0x236efa46, 0x1e82fd5a,
	0x1496ff64,
};

/* 20: the emitted smoke, four particles along the z axis and a flare */
static void VME_Exhaust20(const VMExhaust *x) {
	CGameParticles *ps = x->ps;
	float t = x->t, k = x->k;

	ps->birthRate = VME_Birth(t, x->speed, g_FXEngineBirthRate);
	if (x->cloud) {
		ps->birthRate = VME_CloudBirth(ps->birthRate);
	}
	float t25 = t * 25.0f;
	float k50 = k * 50.0f;
	ps->initPosMin = (vec3_t){ -8.0f, t25 + 15.0f + k50, -8.0f };
	ps->initPosMax = (vec3_t){ 8.0f, t25 + 20.0f + k50, 8.0f };
	ps->sizeIni = (k + k + t) * 60.0f + 140.0f;
	ps->sizeEnd = (k * 1.5f + t) * 20.0f + 75.0f;
	VME_Colors(ps, VME_Exhaust10Table7, t, k);
	if (t > 0.0f) {
		float a = k * 150.0f;
		ps->initVelMin = (vec3_t){ -200.0f - a, (t + k) * 500.0f + 1150.0f, -200.0f - a };
		ps->initVelMax = (vec3_t){ a + 200.0f, (k * 3.0f + t) * 900.0f + 1550.0f, a + 200.0f };
	} else {
		ps->initVelMin = (vec3_t){ 0.0f, 100.0f, 0.0f };
		ps->initVelMax = (vec3_t){ 0.0f, 300.0f, 0.0f };
	}
	ps->spriteIni = 9;
	ps->spriteEnd = 0xb;
	VME_RotTurn(ps, ps, M_2PI_F);
	ps->lifeTime = VME_Life(0.14f, 0.02f, 0.05f, t);
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		vec3_t d = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
		VME_Line(ps, n - 4, 4, d, t, 60.0f, 40.0f, 4, 1, k * 3.0f + t, 70.0f, 160.0f,
			VME_Alpha(x->speed * 125.0f, 0x009632));
	} else {
		CGameParticles_KillRange(ps, n - 4, n);
	}
	VME_Flare(ps, n - 5, k, 700.0f, 0x64e614, 7);
}

static const uint32_t VME_Exhaust10Table8[10] = {
	0x32030f0a, 0x30071914, 0x2d0a321e, 0x2c0f8028, 0x2a14e137, 0x2819d741, 0x2828f855, 0x2332fa5f, 0x1e41fd6e,
	0x1450ff7d,
};

/* 21: like 20 at node 0, a second smoke and one particle at node 1 */
static void VME_Exhaust21(const VMExhaust *x) {
	CGameParticles *ps = x->ps, *ps2 = x->ps2;
	float t = x->t, k = x->k;

	float m = Float_MaxSS(0.0f, t);
	ps->birthRate = VME_Birth(t, x->speed, g_FXEngineBirthRate);
	if (x->cloud) {
		ps->birthRate = VME_CloudBirth(ps->birthRate);
	}
	float t15 = t * 15.0f;
	ps->initPosMin = (vec3_t){ -10.0f, k * 40.0f + 5.0f + t15, -10.0f };
	ps->initPosMax = (vec3_t){ 10.0f, k * 45.0f + 15.0f + t15, 10.0f };
	float s2 = k + k + t;
	float tk = t + k;
	ps->sizeIni = s2 * 80.0f + 75.0f;
	float s15 = k * 1.5f + t;
	ps->sizeEnd = s15 * 60.0f + 55.0f;
	const uint32_t *tab = VME_Exhaust10Table8;
	int c = (int)(__builtin_fabs((double)tk) * 4.0);
	ps->colorIni = tab[c];
	ps->colorEnd = tab[c + 1];
	if (t > 0.0f) {
		float a = k * 150.0f;
		ps->initVelMin = (vec3_t){ -200.0f - a, tk * 500.0f + 1150.0f, -200.0f - a };
		ps->initVelMax = (vec3_t){ a + 200.0f, (k * 3.0f + t) * 900.0f + 1550.0f, a + 200.0f };
	} else {
		ps->initVelMin = (vec3_t){ 0.0f, 100.0f, 0.0f };
		ps->initVelMax = (vec3_t){ 0.0f, 300.0f, 0.0f };
	}
	ps->spriteIni = 0xd;
	ps->spriteEnd = 0xf;
	VME_RotSame(ps, -VME_PI_2, VME_PI_2);
	ps->lifeTime = VME_Life(0.14f, 0.02f, 0.05f, t);
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		vec3_t d = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
		VME_Line(ps, n - 4, 4, d, m, 35.0f, 5.0f, 4, 1, k * 3.0f + m, 80.0f, 80.0f,
			VME_Alpha(x->speed * 125.0f, 0x0a9632));
	} else {
		CGameParticles_KillRange(ps, n - 4, n);
	}
	VME_Flare(ps, n - 5, k, 700.0f, 0x64e614, 7);

	ps2->birthRate = VME_Birth(t, x->speed, 0);
	if (x->cloud) {
		ps2->birthRate = VME_CloudBirth(ps2->birthRate);
	}
	float k50 = k * 50.0f;
	ps2->initPosMin = (vec3_t){ -5.0f, t15 + 10.0f + k50, -5.0f };
	ps2->initPosMax = (vec3_t){ 5.0f, t15 + 15.0f + k50, 5.0f };
	ps2->colorIni = tab[c];
	ps2->sizeIni = s2 * 30.0f + 50.0f;
	ps2->colorEnd = tab[c + 1];
	ps2->sizeEnd = s15 * 10.0f + 35.0f;
	if (t > 0.0f) {
		float a = k * 150.0f;
		ps2->initVelMin = (vec3_t){ -20.0f - a, tk * 400.0f + 600.0f, -20.0f - a };
		float s3 = k * 3.0f + t;
		ps2->initVelMax = (vec3_t){ a + 20.0f, s3 * 700.0f + 900.0f, a + 20.0f };
		ps2->initAccMin = (vec3_t){ 0.0f, 0.0f, s2 * -4200.0f };
		ps2->initAccMax = (vec3_t){ 0.0f, 0.0f, s3 * -5500.0f };
	} else {
		ps2->initVelMin = (vec3_t){ 0.0f, 50.0f, 0.0f };
		ps2->initVelMax = (vec3_t){ 0.0f, 200.0f, 0.0f };
		ps2->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		ps2->initAccMax = (vec3_t){ 0.0f, 0.0f, 0.0f };
	}
	ps2->spriteIni = 9;
	ps2->spriteEnd = (byte)(ps->spriteIni + 2);
	VME_RotTurn(ps2, ps, M_2PI_F);
	ps2->lifeTime = VME_Life(0.14f, 0.02f, 0.05f, t);
	CGameParticles_Apply(ps2);
	n = CGameParticles_NumParticles(ps2);
	if (VME_Show(x->speed, x->cloud)) {
		CGameParticles_SetParticle(ps2, n - 1, 0.0f, 0.0f, 0.0f, (k * 3.0f + m) * 80.0f + 60.0f,
			VME_Alpha(x->speed * 125.0f, 0x0a9632), 1, 0.0f);
	} else {
		CGameParticles_KillRange(ps2, n - 1, n);
	}
	VME_Flare(ps2, n - 2, k, 500.0f, 0x14e664, 8);
}

/* 25, 30, 35: the emitted smoke, four particles along the z axis and a flare along it; the constants per type */
typedef struct VMExhaustLine {
	float posY;			/* the emit height over k * 60 (5 more for initPosMax) */
	float sizeK;			/* sizeIni (k * sizeK + t) * sizeIniK + sizeIni0, also the velocity */
	float sizeIniK, sizeIni0;
	float sizeEndK, sizeEnd0;	/* sizeEnd (k * 1.5 + t) * sizeEndK + sizeEnd0 */
	float velMinK;			/* the y of initVelMin (k * velMinK + t) * 150 + 1200 */
	vec3_t velMin0, velMax0;	/* the velocities at a thrust <= 0 */
	float rotVar;
	byte maxT;			/* the particles spaced by max(0, t), else by t + 0.4 */
	float lineDk, lineD0;
	float lineSk, line0;		/* their size (2 - i) * (k * 4 + t) * lineSk + line0 */
	uint32_t rgb;
	float flareD, flareSize;
	uint32_t flareRgb;
	int flareSprite;
} VMExhaustLine;

static const VMExhaustLine vme_Line25 = {
	45.0f, 4.0f, 80.0f, 110.0f, 35.0f, 65.0f, 2.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 5.0f, 0.0f }, VME_PI_2,
	1, 50.0f, 25.0f, 70.0f, 190.0f, 0xd26432, 50.0f, 1100.0f, 0xf0c850, 8
};
static const VMExhaustLine vme_Line30 = {
	50.0f, 4.0f, 85.0f, 120.0f, 35.0f, 65.0f, 2.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 5.0f, 0.0f }, VME_PI_2,
	0, 45.0f, 35.0f, 45.0f, 210.0f, 0xd28c32, 50.0f, 1100.0f, 0xf0c850, 8
};
static const VMExhaustLine vme_Line35 = {
	40.0f, 3.0f, 60.0f, 80.0f, 40.0f, 60.0f, 1.0f, { 0.0f, 100.0f, 0.0f }, { 0.0f, 300.0f, 0.0f }, M_2PI_F,
	0, 50.0f, 55.0f, 40.0f, 200.0f, 0xd2326e, 40.0f, 700.0f, 0xe66496, 1
};

static void VME_ExhaustLine(const VMExhaust *x, const VMExhaustLine *l, int base, const uint32_t *colors) {
	CGameParticles *ps = x->ps;
	float t = x->t, k = x->k;

	ps->birthRate = VME_Birth(t, x->speed, base);
	if (x->cloud) {
		ps->birthRate = VME_CloudBirth(ps->birthRate);
	}
	float k60 = k * 60.0f;
	ps->initPosMin = (vec3_t){ -5.0f, k60 + l->posY, -5.0f };
	ps->initPosMax = (vec3_t){ 5.0f, k60 + (l->posY + 5.0f), 5.0f };
	float s = k * l->sizeK + t;
	ps->sizeIni = s * l->sizeIniK + l->sizeIni0;
	ps->sizeEnd = (k * 1.5f + t) * l->sizeEndK + l->sizeEnd0;
	VME_Colors(ps, colors, t, k);
	if (t > 0.0f) {
		float a = k * 150.0f;
		ps->initVelMin = (vec3_t){ -200.0f - a, (k * l->velMinK + t) * 150.0f + 1200.0f, -200.0f - a };
		ps->initVelMax = (vec3_t){ a + 200.0f, s * 800.0f + 2500.0f, a + 200.0f };
	} else {
		ps->initVelMin = l->velMin0;
		ps->initVelMax = l->velMax0;
	}
	ps->spriteIni = 0xd;
	ps->spriteEnd = 0x10;
	VME_RotTurn(ps, ps, l->rotVar);
	ps->lifeTime = VME_Life(0.15f, 0.02f, 0.05f, t);
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		float m = l->maxT ? Float_MaxSS(0.0f, t) : t + 0.4f;
		vec3_t d = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
		VME_Line(ps, n - 4, 4, d, m, l->lineDk, l->lineD0, 2, 1, k * 4.0f + t, l->lineSk, l->line0,
			VME_Alpha(x->speed * 130.0f, l->rgb));
	} else {
		CGameParticles_KillRange(ps, n - 4, n);
	}
	VME_FlareDir(ps, x->model, n - 5, k, l->flareD, l->flareSize, l->flareRgb, l->flareSprite);
}

/* 26, 40, 55: the emitted smoke, four particles along the z axis and a flare; the constants per type */
typedef struct VMExhaustLine4 {
	float posX;			/* initPosMin (-posX, (k * 30 + posY0) + t * 25, -5) */
	float posY0, posY1;		/* initPosMax (posX, (k * 45 + posY1) + t * 25, 5) */
	float sizeIniK, sizeIniT, sizeIni0;	/* sizeIni (k * 5 + t) * sizeIniK + (t * sizeIniT + sizeIni0) */
	float sizeEndK, sizeEndT;		/* sizeEnd (k * 1.5 + t) * sizeEndK + (t * sizeEndT + 60) */
	float velMin0, velMax0;		/* the y of the velocities over (t + k) * 500, (k + k + t) * 900 */
	float lifeLo, lifeHi;
	byte maxT;			/* the particles by max(0, t), else by t */
	float lineDk, lineD0;
	float lineSk, lineST, line0;	/* their size (4 - i) * (k * 3 + m) * lineSk + (m * lineST + line0) */
	int flareSprite;
} VMExhaustLine4;

static const VMExhaustLine4 vme_Line26 = {
	5.0f, 0.0f, 5.0f, 50.0f, 75.0f, 125.0f, 20.0f, 45.0f, 2100.0f, 2850.0f, 0.03f, 0.06f,
	1, 70.0f, 30.0f, 75.0f, 120.0f, 230.0f, 7
};
static const VMExhaustLine4 vme_Line40 = {
	15.0f, 10.0f, 20.0f, 50.0f, 80.0f, 100.0f, 20.0f, 50.0f, 2100.0f, 2850.0f, 0.02f, 0.05f,
	0, 80.0f, 40.0f, 75.0f, 190.0f, 100.0f, 7
};
static const VMExhaustLine4 vme_Line55 = {
	5.0f, 15.0f, 25.0f, 60.0f, 70.0f, 105.0f, 30.0f, 40.0f, 1800.0f, 2650.0f, 0.03f, 0.06f,
	1, 70.0f, 40.0f, 60.0f, 110.0f, 190.0f, 8
};

static void VME_ExhaustLine4(const VMExhaust *x, const VMExhaustLine4 *l, const uint32_t *colors) {
	CGameParticles *ps = x->ps;
	float t = x->t, k = x->k;

	ps->birthRate = VME_Birth(t, x->speed, g_FXEngineBirthRate);
	if (x->cloud) {
		ps->birthRate = VME_CloudBirth(ps->birthRate);
	}
	float t25 = t * 25.0f;
	ps->initPosMin = (vec3_t){ -l->posX, k * 30.0f + l->posY0 + t25, -5.0f };
	ps->initPosMax = (vec3_t){ l->posX, k * 45.0f + l->posY1 + t25, 5.0f };
	ps->sizeIni = (k * 5.0f + t) * l->sizeIniK + (t * l->sizeIniT + l->sizeIni0);
	ps->sizeEnd = (k * 1.5f + t) * l->sizeEndK + (t * l->sizeEndT + 60.0f);
	VME_Colors(ps, colors, t, k);
	if (t > 0.0f) {
		float a = k * 150.0f;
		ps->initVelMin = (vec3_t){ -150.0f - a, (t + k) * 500.0f + l->velMin0, -150.0f - a };
		ps->initVelMax = (vec3_t){ a + 150.0f, (k + k + t) * 900.0f + l->velMax0, a + 150.0f };
	} else {
		ps->initVelMin = (vec3_t){ 0.0f, 50.0f, 0.0f };
		ps->initVelMax = (vec3_t){ 0.0f, 200.0f, 0.0f };
	}
	ps->spriteIni = 9;
	ps->spriteEnd = 0xc;
	VME_RotTurn(ps, ps, VME_PI_2);
	ps->lifeTime = VME_Life(0.14f, l->lifeLo, l->lifeHi, t);
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		float m = l->maxT ? Float_MaxSS(0.0f, t) : t;
		vec3_t d = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
		VME_Line(ps, n - 4, 4, d, m, l->lineDk, l->lineD0, 4, 1, k * 3.0f + m, l->lineSk, m * l->lineST + l->line0,
			VME_Alpha(x->speed * 125.0f, 0xa01e05));
	} else {
		CGameParticles_KillRange(ps, n - 4, n);
	}
	VME_Flare(ps, n - 5, k, 1200.0f, 0xfff05a, l->flareSprite);
}

/* 27: the emitted smoke at four positions, two quads around the z axis and a flare */
static const uint32_t VME_Exhaust10Table11[10] = {
	0x32320f0a, 0x303c190f, 0x2d4b2314, 0x2c64371e, 0x2ac84b23, 0x28fa5a28, 0x28fa6432, 0x23fb6e46, 0x1efd825a,
	0x14ff9664,
};

static vec3_t VME_Exhaust10Pos6[4];
static int VME_Exhaust10Table22[2];

static void VME_Exhaust27(const VMExhaust *x) {
	static const float xz[] = { -36.0f, 36.0f, 36.0f, 36.0f, -36.0f, -36.0f, 36.0f, -36.0f };
	CGameParticles *ps = x->ps;
	vec3_t *pos = VME_Exhaust10Pos6;
	float t = x->t, k = x->k;
	int c;

	ps->birthRate = VME_Birth(t, x->speed, 0) * 3;
	if (x->cloud) {
		ps->birthRate = VME_CloudBirth(ps->birthRate) * 3;
	}
	float k15 = k * 15.0f;
	float r = Rand_FloatRange(0.0f, 5.0f) + k15;
	if (VME_StaticBegin(VME_Exhaust10Table22)) {
		VME_SetPos(pos, xz, 4, r + 0.0f);
		VME_StaticEnd(VME_Exhaust10Table22);
	}
	CGameParticles_SetEmitPos(ps, pos, 4);
	float s = k * 4.0f + t;
	ps->sizeIni = s * 60.0f + 60.0f;
	ps->sizeEnd = (k * 1.5f + t) * 40.0f + 35.0f;
	VME_Colors(ps, VME_Exhaust10Table11, t, k);
	ps->initVelMin = (vec3_t){ -10.0f - k15, (t + k) * 120.0f + 700.0f, -15.0f - k15 };
	ps->initVelMax = (vec3_t){ k15 + 10.0f, (k * 3.0f + t) * 600.0f + 1400.0f, k15 + 15.0f };
	ps->spriteIni = 0xd;
	ps->spriteEnd = 0x10;
	VME_RotSame(ps, -VME_PI_2, VME_PI_2);
	ps->lifeTime = VME_Life(0.14f, 0.02f, 0.05f, t);
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		vec3_t rot;
		CGameModel_GetRot(x->model, &rot);
		vec3_t ax = VME_Turn(&rot, 1.0f, 0.0f, 0.0f);
		vec3_t ay = VME_Turn(&rot, 0.0f, 1.0f, 0.0f);
		vec3_t az = VME_Turn(&rot, 0.0f, 0.0f, 1.0f);
		uint32_t col = VME_Alpha(x->speed * 125.0f, 0xaa280a);
		for (c = 0; c < 2; c++) {
			float d = (float)c * t * 35.0f - 25.0f;
			float px = d * az.x;
			float py = d * az.y;
			float pz = d * az.z;
			float f = 0.0f > t ? t + 1.0f : 1.0f;
			float size = ((float)(2 - c) * s * 105.0f + 95.0f) * f;
			CGameParticles_SetParticle(ps, n - 8 + c, ax.x * 38.0f + px + ay.x * 38.0f, ax.y * 38.0f + py + ay.y * 38.0f,
				ax.z * 38.0f + pz + ay.z * 38.0f, size, col, 1, 0.0f);
			CGameParticles_SetParticle(ps, n - 6 + c, ax.x * -38.0f + px + ay.x * 38.0f,
				ax.y * -38.0f + py + ay.y * 38.0f, ax.z * -38.0f + pz + ay.z * 38.0f, size, col, 1, 0.0f);
			CGameParticles_SetParticle(ps, n - 4 + c, ax.x * 38.0f + px + ay.x * -38.0f,
				ax.y * 38.0f + py + ay.y * -38.0f, ax.z * 38.0f + pz + ay.z * -38.0f, size, col, 1, 0.0f);
			CGameParticles_SetParticle(ps, n - 2 + c, ax.x * -38.0f + px + ay.x * -38.0f,
				ax.y * -38.0f + py + ay.y * -38.0f, ax.z * -38.0f + pz + ay.z * -38.0f, size, col, 1, 0.0f);
		}
	} else {
		CGameParticles_KillRange(ps, n - 8, n);
	}
	VME_FlareDir(ps, x->model, n - 9, k, 40.0f, 1200.0f, 0xfff05a, 7);
}

static const uint32_t VME_Exhaust10Table14[10] = {
	0x320f030f, 0x30190719, 0x2d320d23, 0x2c80122d, 0x2ae11750, 0x28eb1e5f, 0x28f82864, 0x23fa2d6e, 0x1efd3282,
	0x14ff3296,
};

/* 36: the emitted smoke, five particles along the z axis and a flare along it */
static void VME_Exhaust36(const VMExhaust *x) {
	CGameParticles *ps = x->ps;
	float t = x->t, k = x->k;

	ps->birthRate = VME_Birth(t, x->speed, g_FXEngineBirthRate) * 2;
	if (x->cloud) {
		ps->birthRate = VME_CloudBirth(ps->birthRate);
	}
	float k60 = k * 60.0f;
	ps->initPosMin = (vec3_t){ -5.0f, k60 + 40.0f, -5.0f };
	ps->initPosMax = (vec3_t){ 5.0f, k60 + 45.0f, 5.0f };
	ps->sizeIni = (k * 4.0f + t) * 70.0f + 95.0f;
	ps->sizeEnd = (k * 2.5f + t) * 50.0f + 75.0f;
	VME_Colors(ps, VME_Exhaust10Table14, t, k);
	if (t > 0.0f) {
		float a = k * 150.0f;
		ps->initVelMin = (vec3_t){ -150.0f - a, (k * 1.5f + t) * 150.0f + 1500.0f, -150.0f - a };
		ps->initVelMax = (vec3_t){ a + 150.0f, (k * 4.5f + t) * 800.0f + 2100.0f, a + 150.0f };
	} else {
		ps->initVelMin = (vec3_t){ 0.0f, 100.0f, 0.0f };
		ps->initVelMax = (vec3_t){ 0.0f, 300.0f, 0.0f };
	}
	ps->spriteIni = 0xd;
	ps->spriteEnd = 0x10;
	VME_RotTurn(ps, ps, M_2PI_F);
	ps->lifeTime = VME_Life(0.14f, 0.02f, 0.05f, t);
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		vec3_t d = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
		VME_Line(ps, n - 5, 5, d, t * 0.25f + 0.5f, 60.0f, 25.0f, 2, 1, k * 3.0f + t, 70.0f, 230.0f,
			VME_Alpha(x->speed * 140.0f, 0xd2326e));
	} else {
		CGameParticles_KillRange(ps, n - 5, n);
	}
	VME_FlareDir(ps, x->model, n - 6, k, 40.0f, 800.0f, 0xe678be, 1);
}

static const uint32_t VME_Exhaust10Table16[10] = {
	0x320a140f, 0x300c1919, 0x2d0f2332, 0x2c0a3264, 0x2a143796, 0x282850fa, 0x283264fa, 0x23466efb, 0x1e5a82fd,
	0x146496ff,
};

/* 45: the emitted smoke, four particles along the z axis and a flare along it */
static void VME_Exhaust45(const VMExhaust *x) {
	CGameParticles *ps = x->ps;
	float t = x->t, k = x->k;

	ps->birthRate = VME_Birth(t, x->speed, g_FXEngineBirthRate);
	if (x->cloud) {
		ps->birthRate = VME_CloudBirth(ps->birthRate);
	}
	float k60 = k * 60.0f;
	ps->initPosMin = (vec3_t){ -15.0f, k60 + 10.0f, -15.0f };
	ps->initPosMax = (vec3_t){ 15.0f, k60 + 20.0f, 15.0f };
	ps->sizeIni = (k * 4.0f + t) * 95.0f + 125.0f;
	ps->sizeEnd = (k * 1.5f + t) * 35.0f + 60.0f;
	VME_Colors(ps, VME_Exhaust10Table16, t, k);
	if (t > 0.0f) {
		float a = k * 150.0f;
		ps->initVelMin = (vec3_t){ -200.0f - a, (t + k) * 150.0f + 1400.0f, -200.0f - a };
		ps->initVelMax = (vec3_t){ a + 200.0f, (k * 3.0f + t) * 800.0f + 2700.0f, a + 200.0f };
	} else {
		ps->initVelMin = (vec3_t){ 0.0f, 100.0f, 0.0f };
		ps->initVelMax = (vec3_t){ 0.0f, 300.0f, 0.0f };
	}
	ps->spriteIni = 9;
	ps->spriteEnd = 0xc;
	VME_RotSame(ps, VME_PI_4, VME_PI_2);
	ps->lifeTime = VME_Life(0.15f, 0.02f, 0.05f, t);
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		vec3_t d = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
		/* the sizes 100, 50, 0, -50 over the base */
		VME_Line(ps, n - 4, 4, d, t + 0.4f, 50.0f, 10.0f, 100, 50, 1.0f, 1.0f, (k + k + t) * 90.0f + 220.0f,
			VME_Alpha(x->speed * 90.0f, 0x7896d2));
	} else {
		CGameParticles_KillRange(ps, n - 4, n);
	}
	VME_FlareDir(ps, x->model, n - 5, k, 40.0f, 1100.0f, 0x6496e6, 7);
}

/* 50: the emitted smoke at eight positions, a particle at each and a flare */
static const uint32_t VME_Exhaust10Table17[10] = {
	0x32320f0a, 0x303c190f, 0x2d4b2314, 0x2c64371e, 0x2ac84b23, 0x28fa5a28, 0x28fa6432, 0x23fb6e46, 0x1efd825a,
	0x14ff9664,
};

static vec3_t VME_Exhaust10Pos7[8];
static int VME_Exhaust10Count3;

static void VME_Exhaust50(const VMExhaust *x) {
	static const float xz[] = {
		-50.0f, -54.0f, 50.0f, -18.0f, -50.0f, 18.0f, 50.0f, 54.0f, 50.0f, -54.0f, -50.0f, -18.0f, 50.0f, 18.0f,
		-50.0f, 54.0f
	};
	CGameParticles *ps = x->ps;
	vec3_t *pos = VME_Exhaust10Pos7;
	float t = x->t, k = x->k;

	ps->birthRate = VME_Birth(t, x->speed, 0) * 4;
	if (x->cloud) {
		ps->birthRate = VME_CloudBirth(ps->birthRate) * 4;
	}
	float r = Rand_FloatRange(10.0f, 15.0f) + k * 35.0f;
	if (VME_StaticBegin((&VME_Exhaust10Count3))) {
		VME_SetPos(pos, xz, 8, r + 0.0f);
		VME_StaticEnd((&VME_Exhaust10Count3));
	}
	CGameParticles_SetEmitPos(ps, pos, 8);
	float s = k * 3.0f + t;
	ps->sizeIni = s * 65.0f + 50.0f;
	ps->sizeEnd = (k * 1.5f + t) * 35.0f + 50.0f;
	VME_Colors(ps, VME_Exhaust10Table17, t, k);
	float k10 = k * 10.0f;
	ps->initVelMin = (vec3_t){ -10.0f - k10, (k + k + t) * 100.0f + 600.0f, -10.0f - k10 };
	ps->initVelMax = (vec3_t){ k10 + 10.0f, s * 500.0f + 1300.0f, k10 + 10.0f };
	ps->spriteIni = 9;
	ps->spriteEnd = 0xb;
	VME_RotSame(ps, -VME_PI_2, VME_PI_2);
	ps->lifeTime = VME_Life(0.12f, 0.015f, 0.025f, t);
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		float m = Float_MaxSS(0.0f, t);
		uint32_t col = (uint32_t)(0xb4 - (int)(m * 130.0f * m)) | (uint32_t)(0x1e - (int)(m * -40.0f)) << 8 |
			(uint32_t)(0x1e - (int)(m * -120.0f)) << 16 | (uint32_t)(-1 - (int)(x->speed * 130.0f)) << 24;
		VME_Ring(ps, x->model, n - 8, pos, 8, 1.0f, m * 10.0f, (k * 5.0f + m) * 100.0f + 130.0f, col);
	} else {
		CGameParticles_KillRange(ps, n - 8, n);
	}
	VME_FlareDir(ps, x->model, n - 9, k, 40.0f, 1400.0f, 0xf0b496, 8);
}

/* 60: the emitted smoke at three positions, two pairs and two flares */
static const uint32_t VME_Exhaust10Table19[10] = {
	0x32320f0a, 0x303c190f, 0x2d4b2314, 0x2c64371e, 0x2ac84b23, 0x28fa5a28, 0x28fa6432, 0x23fb6e46, 0x1efd825a,
	0x14ff9664,
};

static vec3_t VME_Exhaust10Pos8[3];
static int VME_Exhaust10Count4;

static void VME_Exhaust60(const VMExhaust *x) {
	static const float xz[] = { 0.0f, 0.0f, 0.0f, -40.0f, 0.0f, 40.0f };
	CGameParticles *ps = x->ps;
	vec3_t *pos = VME_Exhaust10Pos8;
	float t = x->t, k = x->k;

	ps->birthRate = VME_Birth(t, x->speed, 0) * 2;
	if (x->cloud) {
		ps->birthRate = VME_CloudBirth(ps->birthRate) * 2;
	}
	float t20 = t * 20.0f;
	float r = Rand_FloatRange(10.0f, 15.0f);
	r = (float)(r + t20 + (k * 35.0f));
	if (VME_StaticBegin((&VME_Exhaust10Count4))) {
		VME_SetPos(pos, xz, 3, r + 0.0f);
		VME_StaticEnd((&VME_Exhaust10Count4));
	}
	CGameParticles_SetEmitPos(ps, pos, 3);
	ps->sizeIni = (k * 3.0f + t) * 70.0f + 85.0f;
	ps->sizeEnd = (k * 1.5f + t) * 50.0f + 45.0f;
	VME_Colors(ps, VME_Exhaust10Table19, t, k);
	float k10 = k * 10.0f;
	float s = k + k + t;
	ps->initVelMin = (vec3_t){ -10.0f - k10, s * 350.0f + 950.0f, -10.0f - k10 };
	ps->initVelMax = (vec3_t){ k10 + 10.0f, s * 950.0f + 1500.0f, k10 + 10.0f };
	ps->spriteIni = 9;
	ps->spriteEnd = 0xc;
	VME_RotSame(ps, -VME_PI_2, VME_PI_2);
	ps->lifeTime = VME_Life(0.13f, 0.015f, 0.02f, t);
	CGameParticles_Apply(ps);
	int n = CGameParticles_NumParticles(ps);
	if (VME_Show(x->speed, x->cloud)) {
		vec3_t d = VME_Axis(x->model, 0.0f, 0.0f, 1.0f);
		vec3_t v = VME_Axis(x->model, 0.0f, 1.0f, 0.0f);
		VME_Pair(ps, n - 4, d, v, 30.0f, t, 30.0f, t20, k * 4.0f + t, 125.0f, 105.0f,
			VME_Alpha(x->speed * 128.0f, 0xc82814));
	} else {
		CGameParticles_KillRange(ps, n - 4, n);
	}
	VME_FlarePair(ps, x->model, n - 6, k, 0.0f, 1.0f, 0.0f, 30.0f, 1000.0f, 0xe6963c);
}

static const uint32_t VME_Exhaust10Table9[10] = {
	0x320f0503, 0x30190f07, 0x2d32230d, 0x2c802d12, 0x2ae15017, 0x28eb5f1e, 0x28f86428, 0x23fa6e2d, 0x1efd8232,
	0x14ff9632,
};

static const uint32_t VME_Exhaust10Table10[10] = {
	0x32320f0a, 0x303c190f, 0x2d4b2314, 0x2c64371e, 0x2ac84b23, 0x28fa5a28, 0x28fa6432, 0x23fb6e46, 0x1efd825a,
	0x14ff9664,
};

static const uint32_t VME_Exhaust10Table12[10] = {
	0x320f0503, 0x30190f07, 0x2d32230d, 0x2c802d12, 0x2ae15017, 0x28eb5f1e, 0x28f86428, 0x23fa6e2d, 0x1efd8232,
	0x14ff9632,
};

static const uint32_t VME_Exhaust10Table13[10] = {
	0x320f030f, 0x30190719, 0x2d320d23, 0x2c80122d, 0x2ae11750, 0x28eb1e5f, 0x28f82864, 0x23fa2d6e, 0x1efd3282,
	0x14ff3296,
};

static const uint32_t VME_Exhaust10Table15[10] = {
	0x32320f0a, 0x303c190f, 0x2d4b2314, 0x2c64371e, 0x2ac84b23, 0x28fa5a28, 0x28fa6432, 0x23fb6e46, 0x1efd825a,
	0x14ff9664,
};

static const uint32_t VME_Exhaust10Table18[10] = {
	0x32320f0a, 0x303c190f, 0x2d4b2314, 0x2c64371e, 0x2ac84b23, 0x28fa5a28, 0x28fa6432, 0x23fb6e46, 0x1efd825a,
	0x14ff9664,
};

void cVehicleMotorsEffects_Exhaust(cVehicleMotorsEffects *self, float thrust, float k, float f4bc,
	float speed, byte cloud, float fade) {
	VMExhaust x;
	int i;

	if (!self->type) {
		return;
	}
	x.self = self;
	x.t = thrust * f4bc;
	x.k = k;
	x.speed = speed;
	x.cloud = cloud;
	for (i = 0; i < VM_SIDES; i++) {
		if (self->node[i][0] == -1) {
			continue;
		}
		x.model = cVehicleMotorsEffects_GetModel(self, i);
		if (x.model && !CGameModel_IsEnabled(x.model)) {
			continue;
		}
		x.ps = self->particleSystem[i][0];
		x.ps2 = self->particleSystem[i][1];
		if (!x.ps) {
			continue;
		}
		CGameParticles_SetAlpha(x.ps, fade);
		if (x.ps2) {
			CGameParticles_SetAlpha(x.ps2, fade);
		}
		switch (self->type) {
		case 1:
			VME_Exhaust1(&x);
			break;
		case 5:
			VME_Exhaust5(&x);
			break;
		case 6:
			VME_Exhaust6(&x);
			break;
		case 10:
			VME_Exhaust10(&x);
			break;
		case 11:
			VME_Exhaust11(&x);
			break;
		case 12:
			VME_Exhaust12(&x);
			break;
		case 13:
			VME_Exhaust13(&x);
			break;
		case 14:
			VME_Exhaust14(&x);
			break;
		case 15:
			VME_Exhaust15(&x);
			break;
		case 20:
			VME_Exhaust20(&x);
			break;
		case 21:
			VME_Exhaust21(&x);
			break;
		case 25:
			VME_ExhaustLine(&x, &vme_Line25, x.t > 0.0f ? g_FXEngineBirthRate : 0, VME_Exhaust10Table9);
			break;
		case 26:
			VME_ExhaustLine4(&x, &vme_Line26, VME_Exhaust10Table10);
			break;
		case 27:
			VME_Exhaust27(&x);
			break;
		case 30:
			VME_ExhaustLine(&x, &vme_Line30, g_FXEngineBirthRate, VME_Exhaust10Table12);
			break;
		case 35:
			VME_ExhaustLine(&x, &vme_Line35, g_FXEngineBirthRate, VME_Exhaust10Table13);
			break;
		case 36:
			VME_Exhaust36(&x);
			break;
		case 40:
			VME_ExhaustLine4(&x, &vme_Line40, VME_Exhaust10Table15);
			break;
		case 45:
			VME_Exhaust45(&x);
			break;
		case 50:
			VME_Exhaust50(&x);
			break;
		case 55:
			VME_ExhaustLine4(&x, &vme_Line55, VME_Exhaust10Table18);
			break;
		case 60:
			VME_Exhaust60(&x);
			break;
		}
	}
}

static cGVar GVar_FXEngineTrailOffset;
static cGVar GVar_FXEngineTrailSize;
static cGVar GVar_FXEngineTrailDist;
static cGVar GVar_FXEngineGlowSizeAtt;
static cGVar GVar_FXEngineGlowDistFadeFrom;
static cGVar GVar_FXEngineBirthRate;
static cGVar GVar_FXEngineGlowSize;
static cGVar GVar_FXEngineGlowSizeMax;
static cGVar GVar_FXEngineTrailDistSpeedFactor;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_FXEngineBirthRate, &GVar_FXEngineBirthRate, "FXEngineBirthRate", 0, "BirthRate at Trust 0, (always 100 at Max Trust)", 20)
INIT_GVAR_FLOAT(Init_GVar_FXEngineTrailSize, &GVar_FXEngineTrailSize, "FXEngineTrailSize", 0, "Size of Engine trail", 80.0f)
INIT_GVAR_FLOAT(Init_GVar_FXEngineTrailDist, &GVar_FXEngineTrailDist, "FXEngineTrailDist", 0, "Distance of Engine trail segments at Max Trust", 400.0f)
INIT_GVAR_FLOAT(Init_GVar_FXEngineTrailDistSpeedFactor, &GVar_FXEngineTrailDistSpeedFactor, "FXEngineTrailDistSpeedFactor", 0, "Speed Factor for add to Engine Trail Dist", 0.028f)
INIT_GVAR_FLOAT(Init_GVar_FXEngineTrailOffset, &GVar_FXEngineTrailOffset, "FXEngineTrailOffset", 0, "Offset Z of Engine trail", 850.0f)
INIT_GVAR_FLOAT(Init_GVar_FXEngineGlowDistFadeFrom, &GVar_FXEngineGlowDistFadeFrom, "FXEngineGlowDistFadeFrom", 0, "FX Engine Distance to Glow Fade From", 5000.0f)
INIT_GVAR_FLOAT(Init_GVar_FXEngineGlowSize, &GVar_FXEngineGlowSize, "FXEngineGlowSize", 0, "FX Engine Glow Size at Distance", 450.0f)
INIT_GVAR_FLOAT(Init_GVar_FXEngineGlowSizeMax, &GVar_FXEngineGlowSizeMax, "FXEngineGlowSizeMax", 0, "FX Engine Glow Size Maximun", 1200.0f)
INIT_GVAR_FLOAT(Init_GVar_FXEngineGlowSizeAtt, &GVar_FXEngineGlowSizeAtt, "FXEngineGlowSizeAtt", 0, "FX Engine Glow Size Attenuation Factor", 0.05f)
