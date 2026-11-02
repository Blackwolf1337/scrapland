/*
 * Entities/Char/Type/Rusty.cpp of the game: Rusty, whose hit throws sparks and smoke (include/scrap/chartype.h).
 */
#include "scrap/chartype.h"
#include "scrap/fxelements.h"

uint32_t RUSTY_RAMPS[18] = {
	0x80fff696, 0x20ffb45a, 0x40e68211, 0x80ab5e0d, 0x964c4408, 0xa4380f03, 0xc0280700, 0xff190200, 0xff000000,
	0x80ffebd2, 0x20ffd75a, 0x40e6c811, 0x80ab8c0d, 0x964c3a08, 0xa4380f03, 0xc0280700, 0xff190200, 0xff000000,
};

#define RUSTY_RAMP_SMOKE ((&RUSTY_RAMPS[9]))	/* the second of RUSTY_RAMPS */
#define RUSTY_TWO_PI 6.2831854820251465

static void Rusty_PosMin(CGameParticles *p, float x, float y, float z) {
	p->initPosMin.x = x;
	p->initPosMin.y = y;
	p->initPosMin.z = z;
	p->initPosMax = p->initPosMin;
}

/* a still particle at pos */
static void Rusty_Still(CGameParticles *p, const vec3_t *pos) {
	Rusty_PosMin(p, pos->x, pos->y, pos->z);
	p->initVelMin.x = 0.0f;
	p->initVelMin.y = 0.0f;
	p->initVelMin.z = 0.0f;
	p->initVelMax = p->initVelMin;
	p->initAccMin.x = 0.0f;
	p->initAccMin.y = 0.0f;
	p->initAccMin.z = 0.0f;
	p->initAccMax = p->initAccMin;
}

uint32_t RUSTY_RAMP_DEBRIS[9] = { 0xff808080, 0xa4808080, 0xa4808080, 0xa4808080, 0xa4808080, 0xc0808080, 0xde808080, 0xff000000, 0xff000000 };

/* params: Rusty. At the point of the hit when it is in sight: a flash, a glow and smoke, 7..35 sparks sprayed up
 * along the view and 12 pieces of debris */
void ChType_FXRustyHit(cAbstractFile *params) {
	char name[0x80];
	vec3_t ang, off, t, t2, pos, d0, sang, n, in;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	Obj_Vtbl(v)->GetAng(v, &ang);
	off.x = g_RustyHitPosX;
	off.y = g_RustyHitPosY;
	off.z = g_RustyHitPosZ;
	const vec3_t *r = Vec3_Rotate(&t, &off, &ang);
	const vec3_t *vp = v->vftable->GetPos(v, &t2);
	pos.x = r->x + vp->x;
	pos.y = vp->y + r->y;
	pos.z = vp->z + r->z;
	if (!FX_IsVisible(pos.x, pos.y, pos.z, 1000.0f, 0.0f, NULL, 0)) {
		return;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXRustyHitFile);
	cDebris *d = cDebrisSys_NewElement(sys);
	CGameModel_SetPos(Debris_Model(d), &pos);
	d->life = 1.0f;
	cDebrisSys_Drop(sys, d);
	cObjElementFXParticleSystem *el = Debris_Element(d);
	if (!el) {
		return;
	}
	CGameParticles *p = &el->particles;
	if (!el->on) {
		el->move = 0;
		el->on = 1;
	}
	/* the angles of the spray: up and ahead in the view */
	d0.x = 0.0f;
	d0.y = 1.0f;
	d0.z = -0.800000012f;
	sang = *Vec3_GetRotAng(&t2, Vec3_Rotate(&t, &d0, &ang));
	p->birthRate = 0;
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, RUSTY_RAMP_SMOKE);
	/* the flash */
	Rusty_Still(p, &pos);
	p->sizeIni = FxElem_Rand(15.0, 50.0);
	p->sizeEnd = FxElem_Rand(10.0, 140.0);
	float lt = FxElem_Rand(0.02000001072883606, 0.41999998688697815);
	p->sizeLife = 0.200000003f;
	*(uint16_t *)&p->spriteIni = 0x101;
	p->lifeTime = lt;
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
	/* the glow */
	CGameParticles_SetColorRamp(p, RUSTY_RAMPS);
	Rusty_Still(p, &pos);
	p->sizeIni = FxElem_Rand(20.0, 65.0);
	p->sizeEnd = FxElem_Rand(50.0, 350.0);
	lt = FxElem_Rand(0.019999995827674866, 0.2199999988079071);
	p->sizeLife = 0.200000003f;
	*(uint16_t *)&p->spriteIni = 0x202;
	p->lifeTime = lt;
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
	/* the smoke */
	CGameParticles_SetColorRamp(p, RUSTY_RAMPS);
	Rusty_Still(p, &pos);
	p->sizeIni = FxElem_Rand(25.0, 110.0);
	p->sizeEnd = FxElem_Rand(50.0, 700.0);
	lt = FxElem_Rand(0.03999999165534973, 0.20000000298023224);
	p->sizeLife = 0.150000006f;
	*(uint16_t *)&p->spriteIni = 0x505;
	p->lifeTime = lt;
	float rot = FxElem_Rand(RUSTY_TWO_PI, 0.0);
	p->rotIni = rot;
	p->rotEnd = rot;
	p->rotVar = FxElem_Rand(0.0, 0.39269909262657166);
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
	/* the sparks, each from the point along the velocity of the one before */
	for (int i = (int)FxElem_Rand(7.0, 35.0); i > 0; i--) {
		int bits = crt_rand();
		CGameParticles_SetColorRamp(p, RUSTY_RAMPS + (crt_rand() & 1) * 9);
		float f = FxElem_Rand(8.0, 2.0);
		float vx = p->initVelMin.x;
		float vy = p->initVelMin.y;
		float vz = p->initVelMin.z;
		float len = (float)__builtin_sqrt((double)(vx * vx + vy * vy + vz * vz));
		if (len == 0.0f) {
			n = p->initVelMin;
		} else {
			float inv = 1.0f / len;
			n.x = vx * inv;
			n.y = vy * inv;
			n.z = vz * inv;
		}
		Rusty_PosMin(p, f * n.x + pos.x, f * n.y + pos.y, f * n.z + pos.z);
		float s1 = FxElem_Rand(200.0, 0.0);
		if (!(bits & 2)) {
			s1 = -s1;
		}
		float s2 = FxElem_Rand(200.0, 0.0);
		if (!(bits & 1)) {
			s2 = -s2;
		}
		in.x = s2;
		in.y = s1;
		in.z = FxElem_Rand(2150.0, 350.0);
		p->initVelMin = *Vec3_Rotate(&t, &in, &sang);
		p->initVelMax = p->initVelMin;
		float ax = p->initVelMin.x * 0.200000003f;
		float ay = p->initVelMin.y * 0.200000003f;
		float az = p->initVelMin.z * 0.200000003f;
		p->initAccMin.y = ay - 950.0f;
		p->initAccMin.x = ax + 0.0f;
		p->initAccMin.z = az + 0.0f;
		p->initAccMax = p->initAccMin;
		float e = FxElem_Rand(0.029999999329447746, 0.0);
		lt = FxElem_Rand(0.3500000238418579, 0.25);
		CGameParticles_SetSparksOn(p, 1);
		p->lifeTime = lt;
		float sz = FxElem_Rand(35.0, 10.0);
		p->sizeLife = 1.0f;
		p->rotIni = 0.0f;
		p->rotEnd = 0.0f;
		p->rotVar = 0.0f;
		p->sizeIni = sz;
		p->sizeEnd = sz;
		int sr = crt_rand();
		p->spriteEnd = 0x44;
		p->sparkVelScale = 1.0f;
		p->spriteIni = (byte)((sr & 1) + 0x41);
		p->sparkMaxSize = FxElem_Rand(1.100000023841858, 0.4000000059604645);
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 1, e + 0.0299999993f);
	}
	/* the debris */
	CGameParticles_SetSparksOn(p, 0);
	for (int i = 0; i < 12; i++) {
		CGameParticles_SetColorRamp(p, RUSTY_RAMP_DEBRIS);
		float a = FxElem_Rand(50.0, -25.0);
		float b = FxElem_Rand(50.0, -25.0);
		float c = FxElem_Rand(50.0, -25.0);
		Rusty_PosMin(p, c + pos.x, b + pos.y, a + pos.z);
		float q = FxElem_Rand(100.0, 150.0);
		float w = FxElem_Rand(120.0, -60.0);
		in.x = FxElem_Rand(120.0, -60.0);
		in.y = w;
		in.z = q;
		p->initVelMin = *Vec3_Rotate(&t, &in, &sang);
		p->initVelMax = p->initVelMin;
		float nx = -p->initVelMin.x;
		float ny = -p->initVelMin.y;
		float nz = -p->initVelMin.z;
		p->initAccMin.x = nx * 0.600000024f;
		p->initAccMin.y = ny * 0.600000024f;
		p->initAccMin.z = nz * 0.600000024f;
		p->initAccMax = p->initAccMin;
		p->sizeIni = FxElem_Rand(20.0, 55.0);
		p->sizeEnd = FxElem_Rand(15.0, 135.0);
		lt = FxElem_Rand(0.050000011920928955, 0.75);
		p->sizeLife = 1.0f;
		p->lifeTime = lt;
		byte s;
		if (crt_rand() & 1) {
			s = (byte)((crt_rand() & 1) + 0x11);
		} else {
			s = (byte)(crt_rand() % 4 + 0x21);
		}
		p->spriteEnd = s;
		p->spriteIni = s;
		rot = FxElem_Rand(RUSTY_TWO_PI, 0.0);
		p->rotIni = rot;
		p->rotEnd = rot;
		p->rotVar = FxElem_Rand(0.6283185482025146, -0.3141592741012573);
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 1, (float)i * 0.0299999993f);
	}
}

cGVar GVar_RustyHitPosX;

cGVar GVar_RustyHitPosZ;

cGVar GVar_RustyHitPosY;

cGVar GVar_FXRustyHitFile;

