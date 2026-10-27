/*
 * Entities/Vehicle/VehicleMotorsEffects.cpp of the game: the exhaust effects of the motors of a ship, see
 * include/scrap/vehiclemotors.h: the particles at the DF_MotorTrust nodes, the glow, the trails, the smoke of the
 * motors and the Calima (hot air) particles.
 * Not here: the exhaust particles per FxClass (rva 0x10f210, 34 KB, a switch over the types); cNetVController
 * (rva 0x1186b0..0x119850, the network copy of a ship) that follows in the exe is in NetVController.c.
 */
#include <string.h>

#include "scrap/vehiclemotors.h"

#define g_RandScale GAME_DATA(double, 0x3a3d60)	/* 1/32767 */

/* (rand() & 0x7fff) / 32767 * mul + add, the double expression of the original */
static inline double VME_Rand(double mul, double add) {
	return (double)(crt_rand() & 0x7fff) * g_RandScale * mul + add;
}

static CGameParticles *VME_NewParticles(void) {
	CGameParticles *p = operator_new(sizeof(CGameParticles));
	return p ? CGameParticles_ctor(p) : NULL;
}

/* the particle system of node of model with no emission */
static void VME_StartParticles(CGameParticles *p, CGameModel *model, int node) {
	CGameParticles_Attach(p, model, node);
	CHECK(p->SistPart);
	p->lifeTime = 0.0f;
	p->birthRate = 0;
	CGameParticles_Apply(p);
}

/* the position of node relative to node ref of model */
static vec3_t VME_NodeOffset(CGameModel *model, int ref, int node) {
	vec3_t a, b, r;

	vec3_t *pr = CGameModel_NodoPos(model, &a, ref, 0);
	vec3_t *pn = CGameModel_NodoPos(model, &b, node, 0);
	r.x = pn->x - pr->x;
	r.y = pn->y - pr->y;
	r.z = pn->z - pr->z;
	return r;
}

void __fastcall cVehicleMotorsEffects_Init(cVehicleMotorsEffects *self) {
	char buf[256];
	int i, n;

	CHECK(self->owner);
	if (!self->motors && !self->motor) {
		return;
	}
	self->on = 0;
	self->type = 0;
	self->numExhausts = 0;
	const char *type = Vehicle_TypeName(self->owner);
	if (!crt_stricmp(type, "TVoteMayor") || !crt_stricmp(type, "TVoteBishop")) {
		return;
	}
	/* no trails for the type names T... */
	byte noTrails = type[0] == 'T';
	for (i = 0; i < VM_SIDES; i++) {
		CGameModel *model;
		if (self->motors) {
			cEntity *e = cVehicleMotors_GetOwner(self->motors);
			model = Weap_Vtbl(e)->GetModel(e);
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
		n = CGameModel_FindNodoR(model, buf);
		if (n == -1) {
			if (self->motors && i == 0) {
				Str_PrintfQuiet(buf, 0xff, "DF_MotorTrust");
				n = CGameModel_FindNodoR(model, buf);
			}
			if (n == -1 && self->motor) {
				Str_PrintfQuiet(buf, 0xff, "DF_MotorTrust_01");
				n = CGameModel_FindNodoR(model, buf);
			}
		}
		self->node[i][0] = n;
		if (n == -1) {
			continue;
		}
		self->on = 1;
		if (self->node[i][0] != -1) {
			CHECK(!self->sistPart[i][0]);
			self->sistPart[i][0] = VME_NewParticles();
			VME_StartParticles(self->sistPart[i][0], model, self->node[i][0]);
		}
		n = -1;
		if (self->motor) {
			Str_PrintfQuiet(buf, 0xff, "DF_MotorTrust_02");
			n = CGameModel_FindNodoR(model, buf);
		}
		self->node[i][1] = n;
		if (n != -1) {
			CHECK(!self->sistPart[i][1]);
			self->sistPart[i][1] = VME_NewParticles();
			VME_StartParticles(self->sistPart[i][1], model, self->node[i][1]);
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
			pos = VME_NodeOffset(model, CGameModel_FindNodoR(model, "DC_Root"), self->node[i][0]);
		} else {
			if (!self->motor) {
				continue;
			}
			CGameModel *vm = Weap_Vtbl(self->owner)->GetModel(self->owner);
			int mn = self->motor->node[i];
			if (mn == -1) {
				continue;
			}
			vec3_t d1 = VME_NodeOffset(vm, CGameModel_FindNodoR(vm, "DC_Root"), mn);
			vec3_t rot, e, ang, r;
			matrix_t m;
			vec3_t *pr = CGameModel_GetRot(vm, &rot);
			vec3_t *pe = Matrix_GetEuler(CGameModel_NodoMatrix(vm, &m, mn, 0, NULL, 0), &e);
			ang.y = pe->y - pr->y;
			ang.z = pe->z - pr->z;
			ang.x = pe->x - pr->x;
			vec3_t d2 = VME_NodeOffset(model, CGameModel_FindNodoR(model, "DC_Ship_Link"), self->node[i][0]);
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
	return (float)((double)(crt_rand() & 0x7fff) * g_RandScale * (double)d + (double)lo);
}

void __stdcall VMEffects_SetupCalima(float thrust, float speed, vec3_t *pos, int n, CGameParticles *ps) {
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
	CGameParticles_SetColorRamp(ps, Game_Ptr(0x3b57d8));
	ps->spriteIni = 1;
	ps->spriteEnd = 1;
	float rot = (float)VME_Rand(0x1.921fb6p+2, 0.0);
	ps->rotIni = rot;
	ps->rotEnd = rot;
	ps->rotVar = (float)VME_Rand(0x1.0c1524p+0, -0x1.0c1524p-1);
	ps->lifeTime = 0.6f - (float)VME_Rand(0x1.eb851cp-6, 0x1.99999ap-5) * t;
	CGameParticles_Apply(ps);
}

void __thiscall cVehicleMotorsEffects_SetType(cVehicleMotorsEffects *self, int type) {
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

void __thiscall cVehicleMotorsEffects_Glow(cVehicleMotorsEffects *self, float thrust, float k, float dist,
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

void __thiscall cVehicleMotorsEffects_StopGlow(cVehicleMotorsEffects *self, int *first, CGameParticles *ps) {
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
void __thiscall cVehicleMotorsEffects_SetColor(cVehicleMotorsEffects *self, uint32_t color) {
	self->color = color;
	float r = (float)(int)(color >> 16 & 0xff) / 255.0f;
	float g = (float)(int)(color >> 8 & 0xff) / 255.0f;
	float b = (float)(int)(self->color & 0xff) / 255.0f;
	self->color2 = ((uint32_t)(int)(r * r * 255.0f) << 8 | (uint32_t)(int)(g * g * 255.0f)) << 8 |
		(uint32_t)(int)(b * b * 255.0f);
}

void __thiscall cVehicleMotorsEffects_SetOffset(cVehicleMotorsEffects *self, float v) {
	self->offset = v;
}

void __thiscall cVehicleMotorsEffects_SetScale(cVehicleMotorsEffects *self, float v) {
	self->scale = v;
}

void __thiscall cVehicleMotorsEffects_Set(cVehicleMotorsEffects *self, cEntity *owner, cVehicleMotors *motors,
	cVehicleMotor *motor) {
	self->owner = owner;
	self->motors = motors;
	self->motor = motor;
}

void __thiscall cVehicleMotorsEffects_SetVisible(cVehicleMotorsEffects *self, byte on) {
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
			CGameParticles *p = self->sistPart[i][j];
			if (p && p->SistPart) {
				CGameParticles_SetEnabled(p, on);
				CGameParticles_SetVisible(self->sistPart[i][j], on);
			}
		}
	}
	self->visible = on;
}

void __thiscall cVehicleMotorsEffects_Smoke(cVehicleMotorsEffects *self, float dt, CGameParticles *ps) {
	vec3_t v, rot, up, dir, r, mp, rd;
	int j;

	CHECK(self->owner);
	CHECK(ps && ps->SistPart);
	float t = dt + self->smokeTime;
	self->smokeTime = t;
	if (0.015f > t) {
		return;
	}
	self->smokeTime = 0.0f;
	vec3_t *pv = Weap_Vtbl(self->owner)->GetVel(self->owner, &v);
	float vx = pv->x * 0.8f;
	float vy = pv->y * 0.8f;
	float vz = pv->z * 0.8f;
	CGameModel *model = Weap_Vtbl(self->owner)->GetModel(self->owner);
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
		CGameParticles_SetColorRamp(ps, Game_Ptr(0x4287f8 + (crt_rand() & 1) * 0x24));
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

void __thiscall cVehicleMotorsEffects_Trails(cVehicleMotorsEffects *self, float dt, vec3_t pos, vec3_t rot,
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
