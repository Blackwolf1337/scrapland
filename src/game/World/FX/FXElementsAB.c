/*
 * FXElementsAB.cpp of the game: the fake limit and the meteors of the level AB, see include/scrap/fxelements.h.
 */
#include <string.h>

#include "scrap/camcontrol.h"
#include "scrap/fxelements.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/objentity.h"
#include "scrap/scrap.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

#define CFXELEMENTSABFAKEMETEORS_VFTABLE (&cFXElementsABFakeMeteors_vftable)
#define CFXELEMENTSABFAKELIMIT_VFTABLE (&cFXElementsABFakeLimit_vftable)
#define g_ABLimitRingRotSpeed (GVar_FXABFakeLimitRingRotSpeed.value.f)	/* RingRotSpeed */
#define g_ABLimitSegmentStep (GVar_FXABFakeLimitSegmentStep.value.f)	/* SegmentStep */
#define g_ABLimitSegmentSize (GVar_FXABFakeLimitSegmentSize.value.f)	/* SegmentSize */
#define g_ABLimitRadius (GVar_FXABFakeLimitRadius.value.f)		/* Radius */
#define g_ABLimitDistLighting (GVar_FXABFakeLimitDistLighting.value.f)	/* DistLighting */
#define g_ABLimitQuakeFactor (GVar_FXABFakeLimitQuakeFactor.value.f)	/* QuakeFactor */
#define g_ABLimitQuakeTime (GVar_FXABFakeLimitQuakeTime.value.f)		/* QuakeTime */
static const cMapElementVtbl cFXElementsABFakeMeteors_vftable;
static const cMapElementVtbl cFXElementsABFakeLimit_vftable;
static cGVar GVar_FXABFakeLimitSegmentStep;
static cGVar GVar_FXABFakeLimitSegmentSize;
static cGVar GVar_FXABFakeLimitRingRotSpeed;
static cGVar GVar_FXABFakeLimitRadius;
static cGVar GVar_FXABFakeLimitQuakeTime;
static cGVar GVar_FXABFakeLimitQuakeFactor;
static cGVar GVar_FXABFakeLimitDistLighting;
#define AB_NAME_SKIP 11		/* "DM_Element_" before the name of the element in the dummy */

/* ---- construction ---- */

static cFXElementsABFakeLimit *cFXElementsABFakeLimit_ctor(cFXElementsABFakeLimit *self, const char *name,
	const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base, name, pos, rot);
	self->base.vftable = CFXELEMENTSABFAKELIMIT_VFTABLE;
	CGameParticles_ctor(&self->lighting);
	crt_eh_vector_ctor(self->bolts, sizeof(CGameTrail), FXABBOLTS, (void *)CGameTrail_ctor,
		(void *)CGameTrail_dtor);
	CGameParticles_ctor(&self->nearParts);
	crt_eh_vector_ctor(self->rings, sizeof(CGameTrail), FXABRINGS * FXABRINGTRAILS, (void *)CGameTrail_ctor,
		(void *)CGameTrail_dtor);
	return self;
}

static cFXElementsABFakeMeteors *cFXElementsABFakeMeteors_ctor(cFXElementsABFakeMeteors *self, const char *name,
	const vec3_t *pos, const vec3_t *rot) {
	cMapElement_ctor(&self->base, name, pos, rot);
	self->base.vftable = CFXELEMENTSABFAKEMETEORS_VFTABLE;
	crt_eh_vector_ctor(self->parts, sizeof(CGameParticles), 2, (void *)CGameParticles_ctor,
		(void *)CGameParticles_Release);
	CGameTrail_ctor(&self->curve);
	for (int i = 0; i < FXABMETEORS; i++) {
		self->t[i] = -1.0f;
	}
	return self;
}

/* the bolt times cleared, every trail cleared */
static void cFXElementsABFakeLimit_Clear(cFXElementsABFakeLimit *self) {
	for (int i = 0; i < FXABBOLTS; i++) {
		self->boltTime[i] = 0.0f;
		CGameTrail_Clear(&self->bolts[i]);
	}
	for (int r = 0; r < FXABRINGS; r++) {
		for (int j = 0; j < FXABRINGTRAILS; j++) {
			CGameTrail_Clear(&self->rings[r][j]);
		}
	}
}

static void cFXElementsABFakeLimit_dtor(cFXElementsABFakeLimit *self) {
	self->base.vftable = CFXELEMENTSABFAKELIMIT_VFTABLE;
	cFXElementsABFakeLimit_Clear(self);
	crt_eh_vector_dtor(self->rings, sizeof(CGameTrail), FXABRINGS * FXABRINGTRAILS, (void *)CGameTrail_dtor);
	CGameParticles_Release(&self->nearParts);
	crt_eh_vector_dtor(self->bolts, sizeof(CGameTrail), FXABBOLTS, (void *)CGameTrail_dtor);
	CGameParticles_Release(&self->lighting);
	cMapElement_dtor(&self->base);
}

static void cFXElementsABFakeMeteors_dtor(cFXElementsABFakeMeteors *self) {
	self->base.vftable = CFXELEMENTSABFAKEMETEORS_VFTABLE;
	CGameTrail_dtor(&self->curve);
	crt_eh_vector_dtor(self->parts, sizeof(CGameParticles), 2, (void *)CGameParticles_Release);
	cMapElement_dtor(&self->base);
}

static cMapElement *cFXElementsABFakeLimit_Delete(cMapElement *base, byte flags) {
	cFXElementsABFakeLimit *self = (cFXElementsABFakeLimit *)base;
	cFXElementsABFakeLimit_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

static cMapElement *cFXElementsABFakeMeteors_Delete(cMapElement *base, byte flags) {
	cFXElementsABFakeMeteors *self = (cFXElementsABFakeMeteors *)base;
	cFXElementsABFakeMeteors_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

/* the particles of the render nodes Dummy01 and Dummy02 of the model; the shared start of the two Inits */
static void AB_AttachParticles(cMapElement *e, CGameParticles *p1, CGameParticles *p2) {
	CGameModel *model = &e->Mod;

	cMapElement_Init(e);
	CHECK(CGameModel_IsLoaded(model));
	int node = CGameModel_FindRenderNode(model, "Dummy01");
	CHECK(node != -1);
	CGameParticles_Attach(p1, model, node);
	CHECK(p1->ParticleSystem);
	node = CGameModel_FindRenderNode(model, "Dummy02");
	CHECK(node != -1);
	CGameParticles_Attach(p2, model, node);
	CHECK(p2->ParticleSystem);
}

/* ---- cFXElementsABFakeMeteors ---- */

static byte cFXElementsABFakeMeteors_Init(cMapElement *elem) {
	cFXElementsABFakeMeteors *self = (cFXElementsABFakeMeteors *)elem;
	AB_AttachParticles(&self->base, &self->parts[0], &self->parts[1]);
	CGameModel_SetSortZ(&self->base.Mod, 500000.0f, 0.0f);
	return 1;
}

/* a random sign, -1 or 1 */
static inline float AB_RandSign(void) {
	return (float)((crt_rand() & 1) * 2 - 1);
}

/* every meteor moved along the curve, a new one at the start of its way */
static void cFXElementsABFakeMeteors_Update(cMapElement *elem, float dt) {
	cFXElementsABFakeMeteors *self = (cFXElementsABFakeMeteors *)elem;
	vec3_t pts[3] = {{380000.0f, 25000.0f, 0.0f}, {0.0f, 25000.0f, 0.0f}, {-380000.0f, 25000.0f, 0.0f}};
	vec3_t coefs[GAMETRAIL_MAX_BEZIER];

	cMapElement_Update(&self->base, dt);
	CGameTrail_BezierCoefs(&self->curve, 3, pts, coefs);
	for (int i = 0; i < FXABMETEORS; i++) {
		float t = dt * self->speed[i] + self->t[i];
		self->t[i] = t;
		self->rot[i] = self->rotSpeed[i] * dt + self->rot[i];
		float fade = self->fadeSpeed[i] * dt + self->fade[i];
		self->fade[i] = fade;
		if (0.0f > fade) {
			self->fade[i] = 1.0f;
		} else if (fade > 1.0f) {
			self->fade[i] = 0.0f;
		}
		CGameParticles *part = &self->parts[i > FXABMETEORSPLIT];
		if (0.0f > t || t >= 1.0f) {
			self->t[i] = 0.0f;
			float y, z;
			if (crt_rand() & 1) {
				float a = FxElem_Rand(240000.0, 0.0);
				float b = FxElem_Rand(65000.0, 85000.0);
				b = AB_RandSign() * b;
				z = AB_RandSign() * a;
				y = b;
			} else {
				float b = FxElem_Rand(125000.0, 85000.0);
				float a = FxElem_Rand(190000.0, 0.0);
				a = AB_RandSign() * a;
				z = AB_RandSign() * b;
				y = a;
			}
			self->ofs[i].z = z;
			self->ofs[i].y = y;
			self->ofs[i].x = 0.0f;
			self->speed[i] = FxElem_Rand(0.04749999940395355, 0.0024999999441206455);
			self->size[i] = FxElem_Rand(11000.0, 3500.0);
			self->sprite[i] = crt_rand() % 16 + 1;
			self->rot[i] = FxElem_Rand(6.2831854820251465, 0.0);
			self->rotSpeed[i] = FxElem_Rand(1.0471975803375244, -0.5235987901687622);
			self->fade[i] = FxElem_Rand(1.0, 0.0);
			float s = FxElem_Rand(0.20000000298023224, 0.20000000298023224);
			self->fadeSpeed[i] = AB_RandSign() * s;
		}
		vec3_t p;
		CGameTrail_BezierPoint(&self->curve, 3, coefs, &p, self->t[i]);
		float x = p.x + self->ofs[i].x;
		float y = p.y + self->ofs[i].y;
		float z = p.z + self->ofs[i].z;
		int sprite = 1 - (int)(self->fade[i] * -19.0f);
		t = self->t[i];
		int alpha = 0;
		if (0.1f > t) {
			alpha = 0xff - (int)(t * 255.0f * 10.0f);
		} else if (t > 0.9f) {
			alpha = (int)((t - 0.9f) * 255.0f * 10.0f);
		}
		uint32_t c = (uint32_t)(0xff - alpha);
		CGameParticles_SetParticle(part, i > FXABMETEORSPLIT ? i - FXABMETEORSPLIT : i, x, y, z, self->size[i],
			c << 8 | c << 16 | (uint32_t)alpha << 24 | c, sprite, self->rot[i]);
	}
}

/* ---- cFXElementsABFakeLimit ---- */

static byte cFXElementsABFakeLimit_Init(cMapElement *elem) {
	cFXElementsABFakeLimit *self = (cFXElementsABFakeLimit *)elem;
	char buf[0x80];
	vec3_t pos;
	float ang[2];
	byte flag;

	AB_AttachParticles(&self->base, &self->lighting, &self->nearParts);
	CGameModel_SetSortZ(&self->base.Mod, -500000.0f, 0.0f);
	DummySearch_Start("DM_Mission_Centre", 0);
	if (DummySearch_Next(buf, sizeof(buf), &pos, ang, &flag)) {
		self->centre = pos;
	}
	for (int i = 0; i < FXABBOLTS; i++) {
		CGameTrail *trail = &self->bolts[i];
		self->boltTime[i] = 0.0f;
		CGameTrail_SetMaxTrails(trail, 0xe);
		CGameTrail_SetSmooth(trail, 0);
		CGameTrail_SetTexture(trail, i & 1 ? 8 : 9);
		CGameTrail_SetPremul(trail, 1);
		CGameTrail_SetUVScale(trail, 0.25f, 1.0f);
	}
	for (int r = 0; r < FXABRINGS; r++) {
		self->scroll[r] = (float)r * 3.1415927f;
		for (int j = 0; j < FXABRINGTRAILS; j++) {
			CGameTrail *trail = &self->rings[r][j];
			CGameTrail_SetMaxTrails(trail, 0x1f);
			CGameTrail_SetSmooth(trail, 0);
			CGameTrail_SetTexture(trail, 0);
			CGameTrail_SetPremul(trail, 1);
		}
		/* the random offset of the original is set to 0 again right away */
		float f = FxElem_Rand(3.1415927410125732, 0.0);
		self->ringOfs[r].z = 0.0f;
		if (r < 2) {
			self->ringOfs[r].x = f;
			self->ringOfs[r].y = 0.0f;
		} else {
			self->ringOfs[r].x = 0.0f;
			self->ringOfs[r].y = f;
		}
		self->ringOfs[r].x = 0.0f;
		self->ringOfs[r].y = 0.0f;
		self->ringOfs[r].z = 0.0f;
	}
	return 1;
}

/* the rings around the centre: 14 trails each, rings 0 and 1 turn around x, 2 and 3 around y (faster) */
static void Limit_Rings(cFXElementsABFakeLimit *self, float dt) {
	vec3_t tmp;

	for (int r = 0; r < FXABRINGS; r++) {
		float f = dt * g_ABLimitRingRotSpeed;
		if (r >= 2) {
			f = f * 1.5f;
		}
		f = f + self->scroll[r];
		self->scroll[r] = f;
		if (f > 6.2831855f) {
			self->scroll[r] = f - 6.2831855f;
		}
		for (int j = 0; j < FXABRINGTRAILS; j++) {
			CGameTrail *trail = &self->rings[r][j];
			int cnt = FXABRINGTRAILS - j;
			CGameTrail_Reset(trail);
			CGameTrail_SetTransp(trail, (byte)(int)((float)cnt / 14.0f * 232.0f));
			int n = CGameTrail_GetMaxTrails(trail);
			int half = n / 2;
			float fj = (float)j;
			float fhalf = (float)half;
			for (int k = 0; k < n; k++) {
				float a;
				if (k < half) {
					a = -(1.0f - (float)k / fhalf);
				} else {
					a = (float)k / fhalf - 1.0f;
				}
				float width = g_ABLimitSegmentSize;
				float fa = (float)__builtin_fabs((double)a) * -191.0f;
				uint32_t color = (uint32_t)(0x40 - (int)fa) << 24 | 0x121240;
				float s, c;
				Float_SinCos(a, &s, &c);
				float step = g_ABLimitSegmentStep;
				vec3_t in, rot, at;
				const vec3_t *o;
				if (r < 2) {
					float ang = fj * step + self->scroll[r];
					rot.x = self->ringOfs[r].x + 0.0f;
					rot.y = self->ringOfs[r].y + ang;
					rot.z = self->ringOfs[r].z + 0.0f;
					in.x = 0.0f;
					in.y = s;
					in.z = c;
					float rad = g_ABLimitRadius;
					o = Vec3_Rotate(&tmp, &in, &rot);
					at.x = rad * o->x + self->centre.x;
					at.y = self->centre.y + o->y * rad;
					at.z = self->centre.z + o->z * rad;
				} else {
					rot.x = fj * step + self->scroll[r] + self->ringOfs[r].x;
					rot.y = self->ringOfs[r].y + 0.0f;
					rot.z = self->ringOfs[r].z + 0.0f;
					in.x = s;
					in.y = 0.0f;
					in.z = c;
					float rad = g_ABLimitRadius;
					o = Vec3_Rotate(&tmp, &in, &rot);
					at.x = o->x * rad + self->centre.x;
					at.y = self->centre.y + o->y * rad;
					at.z = self->centre.z + o->z * rad;
				}
				CGameTrail_Add(trail, at, width, color);
			}
		}
	}
}

/* the follow camera of a car (of a player) and its shaking */
#define Car_Data(e) ((cFollowCamEntity *)Obj(e)->cam)
#define CarData_QuakeTime(d) ((d)->quakeTime)
#define CarData_QuakeFactor(d) ((d)->quakeFactor)
/* the model of an entity with render effects */

/* the particles of the near limit on a grid of angles around dir (ang its angles), at the radius */
static void Limit_NearGrid(cFXElementsABFakeLimit *self, const vec3_t *ang) {
	vec3_t tmp;
	int idx = 0;
	float az = ang->z + 0.0f;

	for (float a = -0.07f; 0.07f > a; a += 0.011666666716337204f) {
		float ax = ang->x + a;
		float fa = (float)__builtin_fabs((double)a) / 0.07f;
		for (float b = -0.07f; 0.07f > b; b += 0.011666666716337204f) {
			vec3_t in = {0.0f, 0.0f, 1.0f};
			vec3_t rot = {ax, ang->y + b, az};
			float rad = g_ABLimitRadius;
			const vec3_t *o = Vec3_Rotate(&tmp, &in, &rot);
			float z = o->z * rad + self->centre.z;
			float x = o->x * rad + self->centre.x;
			float y = o->y * rad + self->centre.y;
			float v = ((float)__builtin_fabs((double)b) / 0.07f + fa) * 0.5f;
			int rnd = crt_rand() % 32;
			int g = rnd - (int)(v * -32.0f) + 0x40;
			uint32_t color = (uint32_t)(0xc0 - (int)(v * -63.0f)) << 24 | (uint32_t)g << 8 | (uint32_t)g << 16 |
				(uint32_t)(rnd + 0xdc);
			CGameParticles_SetParticle(&self->nearParts, idx, x, y, z, 3050.0f, color, 1, 0.0f);
			idx++;
		}
	}
}

/* bolt i drawn from its start on the car to the limit, its glow at the start */
static void Limit_DrawBolt(cFXElementsABFakeLimit *self, int i, CGameModel *carModel, const vec3_t *dir, float dist,
	float nearF, float dt) {
	CGameTrail *trail = &self->bolts[i];
	vec3_t coefs[GAMETRAIL_MAX_BEZIER];
	vec3_t pts[4];
	vec3_t tmp, rs;
	vec3_t *c0 = &self->boltCtrl[i][0];
	vec3_t *c1 = &self->boltCtrl[i][1];

	int alpha = 0xff - (int)(Float_MinSS(1.0f, self->boltTime[i] / 0.15f) * 255.0f);
	CGameTrail_Reset(trail);
	CGameTrail_SetTransp(trail, (byte)alpha);
	const vec3_t *r = FX_RandomDir(&tmp);
	float ry = r->y * dt * 20.0f;
	float rz = r->z * dt * 20.0f;
	float rx = r->x * dt * 20.0f;
	c0->y = ry + c0->y;
	c0->z = rz + c0->z;
	c0->x = c0->x + rx;
	r = FX_RandomDir(&tmp);
	rx = dt * r->x * 20.0f;
	ry = r->y * dt * 20.0f;
	rz = r->z * dt * 20.0f;
	c1->x = rx + c1->x;
	c1->z = rz + c1->z;
	c1->y = c1->y + ry;

	/* the start: its point on the car turned with the car */
	Vec3_Rotate(&rs, &self->boltStart[i], CGameModel_GetRot(carModel, &tmp));
	const vec3_t *cp = CGameModel_GetPos(carModel, &tmp);
	pts[0].x = rs.x + cp->x;
	pts[0].y = cp->y + rs.y;
	pts[0].z = cp->z + rs.z;
	/* the end: on the line from the centre to the car, near the limit, moved at random */
	float j = FxElem_Rand(20.0, 15.0);
	r = FX_RandomDir(&tmp);
	float jy = r->y * j;
	float jz = r->z * j;
	float jx = j * r->x;
	float len = (dist + 1500.0f) - nearF * 500.0f;
	pts[3].x = jx + ((dir->x * len + self->centre.x) + rs.x * 2.0f);
	pts[3].y = jy + ((self->centre.y + dir->y * len) + rs.y * 2.0f);
	pts[3].z = jz + ((self->centre.z + dir->z * len) + rs.z * 2.0f);
	float ex = (pts[3].x - pts[0].x) * 0.33333334f;
	float ey = (pts[3].y - pts[0].y) * 0.33333334f;
	float ez = (pts[3].z - pts[0].z) * 0.33333334f;
	pts[1].x = pts[0].x + ex + c0->x;
	pts[1].y = pts[0].y + ey + c0->y;
	pts[1].z = pts[0].z + ez + c0->z;
	pts[2].x = pts[0].x + ex * 2.0f + c1->x;
	pts[2].y = pts[0].y + ey * 2.0f + c1->y;
	pts[2].z = pts[0].z + ez * 2.0f + c1->z;
	CGameTrail_BezierCoefs(trail, 4, pts, coefs);
	int n = CGameTrail_GetMaxTrails(trail);
	float fn2 = (float)(n - 2);
	for (int k = 0; k < n - 1; k++) {
		vec3_t pt;
		float fk = (float)k;
		CGameTrail_BezierPoint(trail, 4, coefs, &pt, fk / fn2);
		if (k > 0 && k < n - 2) {
			float s = FxElem_Rand(5.09999942779541, 7.300000190734863);
			r = FX_RandomDir(&tmp);
			pt.x = pt.x + s * r->x;
			pt.y = pt.y + r->y * s;
			pt.z = pt.z + r->z * s;
		}
		float w = FxElem_Rand(20.0, 55.0);
		int g = crt_rand() % 3;
		int b = crt_rand() % 3;
		int a = 0x10 - (int)(fk / (float)n * -192.0f);
		uint32_t color = (uint32_t)(b + 0x24) << 16 | (uint32_t)a << 24 | (uint32_t)(g + 0x24) << 8 | 0x5a;
		CGameTrail_Add(trail, pt, self->boltSize[i] * w, color);
	}
	CGameParticles_SetVisible(&self->lighting, 1);
	int a1 = alpha + 0x20;
	if (a1 > 0xff) {
		a1 = 0xff;
	}
	CGameParticles_SetParticle(&self->lighting, 2 * i, pts[0].x, pts[0].y, pts[0].z, self->boltSize[i] * 174.0f,
		(uint32_t)a1 << 24 | 0x8080ff, 2, 0.0f);
	int a2 = alpha + 0xc0;
	if (a2 > 0xff) {
		a2 = 0xff;
	}
	float rot = FxElem_Rand(6.2831854820251465, 0.0);
	CGameParticles_SetParticle(&self->lighting, 2 * i + 1, pts[0].x, pts[0].y, pts[0].z, self->boltSize[i] * 850.0f,
		(uint32_t)a2 << 24 | 0x3030ff, 5, rot);
}

/* a new bolt i from a random vertex of the car */
static void Limit_StartBolt(cFXElementsABFakeLimit *self, int i, CGameModel *carModel) {
	vec3_t v, tmp, local, out;

	self->boltTime[i] = FxElem_Rand(0.1599999964237213, 0.10000000149011612);
	self->boltSize[i] = FxElem_Rand(0.7500000596046448, 0.699999988079071);
	CGameModel_Vertices(carModel, &v, NULL, NULL, 1, -1, NULL, 0xffffffff, NULL);
	const vec3_t *p = CGameModel_GetPos(carModel, &tmp);
	local.y = v.y - p->y;
	local.z = v.z - p->z;
	local.x = v.x - p->x;
	self->boltStart[i] = *Vec3_RotateInv(&out, &local, CGameModel_GetRot(carModel, &tmp));
	float s = FxElem_Rand(270.0, 50.0);
	const vec3_t *r = FX_RandomDir(&tmp);
	self->boltCtrl[i][0].x = s * r->x;
	self->boltCtrl[i][0].y = r->y * s;
	self->boltCtrl[i][0].z = r->z * s;
	s = FxElem_Rand(270.0, 50.0);
	r = FX_RandomDir(&tmp);
	self->boltCtrl[i][1].x = s * r->x;
	self->boltCtrl[i][1].y = r->y * s;
	self->boltCtrl[i][1].z = r->z * s;
}

/* the car of the player near the limit: its camera shakes, it glows, bolts from it to the limit */
static void Limit_Car(cFXElementsABFakeLimit *self, cEntity *e, float dt) {
	vec3_t tmp, dirA, dirB, dirc, angTmp;
	const vec3_t *dir;

	CGameModel *carModel = Obj_GetModel(e);
	CHECK(carModel && CGameModel_IsLoaded(carModel));
	const vec3_t *c = Obj_GetViewPos(e, &tmp);
	float dy = c->y - self->centre.y;
	float dx = c->x - self->centre.x;
	float dz = c->z - self->centre.z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (dist == 0.0f) {
		dirA.x = dx;
		dirA.y = dy;
		dirA.z = dz;
		dir = &dirA;
	} else {
		float inv = 1.0f / dist;
		dirB.x = dx * inv;
		dirB.y = dy * inv;
		dirB.z = dz * inv;
		dir = &dirB;
	}
	dirc = *dir;
	vec3_t ang = *Vec3_GetRotAng(&angTmp, &dirc);
	float nearF = 0.0f;
	float inner = g_ABLimitRadius - g_ABLimitDistLighting;
	if (dist > inner) {
		nearF = (dist - inner) / g_ABLimitDistLighting;
		if (nearF > 0.0f) {
			cFollowCamEntity *car = Car_Data(e);
			float quake = nearF * g_ABLimitQuakeFactor;
			if (car && (quake > CarData_QuakeFactor(car) || 0.01f > CarData_QuakeTime(car))) {
				CarData_QuakeFactor(car) = quake;
				if (0.05f > CarData_QuakeTime(car)) {
					CarData_QuakeTime(car) = g_ABLimitQuakeTime;
				}
			}
		}
	}
	uint32_t fx = CGameModel_GetEffect(&Obj(e)->model, NULL);
	if (nearF > 0.0f) {
		if (fx == 0 || fx == 6) {
			Obj_SetEffect(e, MODEL_EFFECT_ELECTRIC);
			Obj_SetEffectParam(e, 0, nearF * nearF * (2.0f - nearF));
		}
	} else if (fx == 6) {
		Obj_SetEffect(e, MODEL_EFFECT_NONE);
	}
	if (nearF > 0.0f) {
		CGameParticles_SetAlpha(&self->nearParts,
			(dist - (g_ABLimitRadius - g_ABLimitDistLighting)) / g_ABLimitDistLighting);
		Limit_NearGrid(self, &ang);
	} else {
		CGameParticles_SetAlpha(&self->nearParts, 0.0f);
	}
	for (int i = 0; i < FXABBOLTS; i++) {
		if (self->boltTime[i] > 0.0f) {
			float t = self->boltTime[i] - dt;
			self->boltTime[i] = t;
			if (t > 0.0f) {
				Limit_DrawBolt(self, i, carModel, &dirc, dist, nearF, dt);
			}
			if (0.0f >= self->boltTime[i]) {
				CGameTrail_Clear(&self->bolts[i]);
				CGameParticles_KillRange(&self->lighting, 2 * i, 2 * i + 2);
			}
		} else if (nearF > 0.0f && nearF >= FxElem_Rand(0.6499999761581421, 0.0)) {
			Limit_StartBolt(self, i, carModel);
		}
	}
}

static void cFXElementsABFakeLimit_Update(cMapElement *elem, float dt) {
	cFXElementsABFakeLimit *self = (cFXElementsABFakeLimit *)elem;
	cMapElement_Update(&self->base, dt);
	Limit_Rings(self, dt);
	cWorld *w = g_pWorld;
	cEntity *e = w->owners[0];
	if (e) {
		CHECK(Entity_IsType(e, TYEN_WITH_LIFE));
	}
	e = w->owners[0];
	if (e && Entity_IsType(e, TYEN_CAR) && Car_Data(e)) {
		Limit_Car(self, e, dt);
		return;
	}
	if (CGameParticles_IsVisible(&self->lighting)) {
		for (int i = 0; i < FXABBOLTS; i++) {
			self->boltTime[i] = 0.0f;
			CGameTrail_Clear(&self->bolts[i]);
		}
		CGameParticles_KillAll(&self->lighting);
		CGameParticles_SetVisible(&self->lighting, 0);
	}
}

/* ---- the loader ---- */

void FXElementsAB_Create(void) {
	char buf[0x80];
	char *name = buf + AB_NAME_SKIP;
	vec3_t pos, rot;
	float ang[2];
	byte flag;

	DummySearch_Start("DM_Element_ABFakeMeteors", 0);
	while (DummySearch_Next(buf, sizeof(buf), &pos, ang, &flag)) {
		char *p = strrchr(name, '_');
		if (p) {
			*p = 0;
		}
		cFXElementsABFakeMeteors *m = operator_new(sizeof(cFXElementsABFakeMeteors));
		if (m) {
			rot.x = ang[0];
			rot.y = ang[1];
			rot.z = 0.0f;
			m = cFXElementsABFakeMeteors_ctor(m, name, &pos, &rot);
		}
		CHECK(m);
		MapElement_Add(&m->base);
	}
	DummySearch_Start("DM_Element_ABFakeLimit", 0);
	while (DummySearch_Next(buf, sizeof(buf), &pos, ang, &flag)) {
		char *p = strrchr(name, '_');
		if (p) {
			*p = 0;
		}
		cFXElementsABFakeLimit *l = operator_new(sizeof(cFXElementsABFakeLimit));
		if (l) {
			rot.x = ang[0];
			rot.y = ang[1];
			rot.z = 0.0f;
			l = cFXElementsABFakeLimit_ctor(l, name, &pos, &rot);
		}
		CHECK(l);
		MapElement_Add(&l->base);
	}
}

/* the vtable of cFXElementsABFakeMeteors */
static const cMapElementVtbl cFXElementsABFakeMeteors_vftable = {
	.Delete = cFXElementsABFakeMeteors_Delete,
	.Init = cFXElementsABFakeMeteors_Init,
	.Update = cFXElementsABFakeMeteors_Update,
};

/* the vtable of cFXElementsABFakeLimit */
static const cMapElementVtbl cFXElementsABFakeLimit_vftable = {
	.Delete = cFXElementsABFakeLimit_Delete,
	.Init = cFXElementsABFakeLimit_Init,
	.Update = cFXElementsABFakeLimit_Update,
};

static cGVar GVar_FXABFakeLimitSegmentStep;
/* the engine variables FXABFakeLimit* */
static cGVar GVar_FXABFakeLimitRingRotSpeed;
static cGVar GVar_FXABFakeLimitRadius;
static cGVar GVar_FXABFakeLimitDistLighting;
static cGVar GVar_FXABFakeLimitSegmentSize;
static cGVar GVar_FXABFakeLimitQuakeFactor;
static cGVar GVar_FXABFakeLimitQuakeTime;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_FXABFakeLimitRadius, &GVar_FXABFakeLimitRadius, "FXABFakeLimitRadius", 0, "", 96000.0f)
INIT_GVAR_FLOAT(Init_GVar_FXABFakeLimitDistLighting, &GVar_FXABFakeLimitDistLighting, "FXABFakeLimitDistLighting", 0, "", 20000.0f)
INIT_GVAR_FLOAT(Init_GVar_FXABFakeLimitRingRotSpeed, &GVar_FXABFakeLimitRingRotSpeed, "FXABFakeLimitRingRotSpeed", 0, "", 2.0f)
INIT_GVAR_FLOAT(Init_GVar_FXABFakeLimitSegmentSize, &GVar_FXABFakeLimitSegmentSize, "FXABFakeLimitSegmentSize", 0, "", 15000.0f)
INIT_GVAR_FLOAT(Init_GVar_FXABFakeLimitSegmentStep, &GVar_FXABFakeLimitSegmentStep, "FXABFakeLimitSegmentStep", 0, "", 0.038f)
INIT_GVAR_FLOAT(Init_GVar_FXABFakeLimitQuakeFactor, &GVar_FXABFakeLimitQuakeFactor, "FXABFakeLimitQuakeFactor", 0, "", 0.015f)
INIT_GVAR_FLOAT(Init_GVar_FXABFakeLimitQuakeTime, &GVar_FXABFakeLimitQuakeTime, "FXABFakeLimitQuakeTime", 0, "", 0.1f)
