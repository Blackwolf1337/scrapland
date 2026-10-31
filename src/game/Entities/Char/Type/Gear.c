/*
 * Entities/Char/Type/Gear.cpp of the game: the armored gear, whose missile is an electric ball that hurts the
 * characters around the point it hits (include/scrap/chartype.h). GearEndActionEnd is ChType_EndActionEnd. The
 * flash of the characters (Logic_Flash, used by SLogic.Flash and Berto) follows in the code without a CHECK of its
 * own and is kept here.
 */
#include <string.h>

#include "scrap/chartype.h"
/* fxcharactions.h (from chartype.h) names its trail count so too; not used here */
#undef FXMESSENGER_TRAILS
#include "scrap/embeddedfxchar.h"
#include "scrap/missileentity.h"
#include "scrap/primitive.h"
#include "scrap/vehicleattack.h"
#include "scrap/weaps2.h"

static inline cMissileEntity *Gear_Missile(cEntity *e) {
	return (cMissileEntity *)e;
}

/* the sound at the character for the player, else halfway to the camera */
static void Gear_Sound(cEntity *v, byte usr, char *const *name, const float *vol) {
	vec3_t tmp, pos;

	if (usr) {
		const char *s = *name;
		CSoundSys *snd = g_pSound;
		int volume = Snd_Volume(*vol);
		const vec3_t *p = v->vftable->GetPos(v, &tmp);
		snd->vftable->PlayAt(g_pSound, s, p, volume);
		return;
	}
	cWorld *w = g_pWorld;
	vec3_t c = w->camPos[w->curCam];
	const vec3_t *p = v->vftable->GetPos(v, &tmp);
	float hx = (c.x - p->x) * 0.5f;
	float hy = (c.y - p->y) * 0.5f;
	float hz = (c.z - p->z) * 0.5f;
	p = v->vftable->GetPos(v, &tmp);
	const char *s = *name;
	pos.x = hx + p->x;
	pos.y = p->y + hy;
	pos.z = p->z + hz;
	CSoundSys *snd = g_pSound;
	int volume = Snd_Volume(*vol);
	snd->vftable->PlayAt(g_pSound, s, &pos, volume);
}

/* the electric ball hits name (not the gear itself): it loses GearShootDamage of life (half against the AI in the
 * easy game), its logic hears of the gear unless noLogic, and a hit of the player counts (alarm, statistics) */
void ChType_GearHitChar(const char *name, byte noLogic) {
	cEntity *gear = NULL;
	byte usr;

	if (!strcmp(g_GearTarget, name) || !*name) {
		return;
	}
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return;
	}
	byte withLife = e->vftable->IsType(e, TYEN_WITH_LIFE);
	if (g_GearTarget[0] && (gear = World_FindEntity(g_GearTarget)) != NULL) {
		CHECK(Entity_IsType(gear, TYEN_WALKCHAR));
		usr = gear == cWorld_GetUsrEntity(g_pWorld, 0);
	} else {
		usr = 0;
	}
	int damage = g_GearShootDamage;
	if (!withLife) {
		return;
	}
	if (gear) {
		if (gear->vftable->GetAI(gear) && World_IsEasySingle()) {
			damage = damage / 2;
		}
		cLocalLogic *l = Logic_GetLocalLogic(e);
		if (l && !noLogic) {
			ChLogic_Vtbl(l)->OnHit(l, gear);
		}
		if (usr && gear == cWorld_GetUsrEntity(g_pWorld, 0)) {
			if (WChar_Life(e) <= damage) {
				Alarm_Set(&g_pWorld->alarm, 1.0f);
				SaveVar_Inc("Stats.Gear", 1);
				SaveVar_Inc("Stats.Killer", 1);
			} else {
				Alarm_Set(&g_pWorld->alarm, g_pWorld->alarm.value + g_GearShootHitAlarm);
			}
		}
	}
	if (WChar_Cam(e)) {
		cPad_Rumble(&g_Pads[0], 1.0f, 0.0f, 0.5f);
	}
	if (!WChar_God(e)) {
		WChar_Life(e) -= damage;
	}
	if (g_GearSw) {
		cVehicleAttack_HitBy(WChar_Attack(e), Gear_Missile(g_GearSw)->parent);
	}
}

/* params: the gear. ActionInit, then Action as the loop once it runs (else the button counts as up) */
void ChType_GearActionPress(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = World_FindEntity(name);
	ChType_Usr0();
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	void *ctrl = ChType_ActCtrl(v);
	if (strcmp(cActCtrl_GetActionName(ctrl, 0), "ActionInit")) {
		cActCtrl_Play(ctrl, "ActionInit", 0, 1.0f);
		if (strcmp(cActCtrl_GetActionName(ctrl, 0), "ActionInit")) {
			/* the button and the byte after it */
			*(uint16_t *)&WChar_ActionPress(v) = 0;
			return;
		}
	}
	cActCtrl_SetDefLoop(ctrl, "Action", 0);
}

/* params: the gear, an event, a float: the end when the button is up or the energy of the player too low */
void ChType_GearEndAction(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = World_FindEntity(name);
	cEntity *usr = ChType_Usr0();
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	if (!WChar_ActionPress(v) || (usr == v && WChar_EnergyBarDecr(v) > 1.0f - WChar_EnergyUsed(v))) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionEnd", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Relax", 1);
		*(uint16_t *)&WChar_ActionPress(v) = 0;
	}
}

/* the sphere test of the ball: params a character in the sphere */
void ChType_GearSphereHit(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	ChType_GearHitChar(name, 0);
}

/* the OnHit of the missile: params the missile, the gear, the entity it hit. The hit entity, then the ones in the
 * sphere of the explosion, and the hit sound. */
void ChType_GearHit(cAbstractFile *params) {
	char name[0x80];
	vec3_t tmp;

	cAbstractFile_ReadString(params, g_GearSwName, 0x80);
	cAbstractFile_ReadString(params, g_GearTarget, 0x80);
	cAbstractFile_ReadString(params, name, 0x80);
	g_GearSw = World_FindEntity(g_GearSwName);
	CHECK(g_GearSw);
	CHECK(Entity_IsType(g_GearSw, TYEN_MISSILE));
	g_GearXplodePos = *g_GearSw->vftable->GetPos(g_GearSw, &tmp);
	ChType_GearHitChar(name, 1);
	cPy_Function_SetNative(g_ChTypeSphereFunc, (nativefunc_t)GAME_FUNC(ChType_GearSphereHit, CHTYPE_GEARSPHEREHIT));
	cWorld_SphereCallF(g_pWorld, 0x11, &g_GearXplodePos, g_GearXplodeRadius, g_ChTypeSphereFunc, 1);
	CSoundSys *snd = g_pSound;
	const char *s = g_GearHitSound;
	int volume = Snd_Volume(g_GearHitSoundVol);
	const vec3_t *p = g_GearSw->vftable->GetPos(g_GearSw, &tmp);
	snd->vftable->PlayAt(g_pSound, s, p, volume);
}

/* params: the gear, an event, a float: the load sound */
void ChType_GearInitAction(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	Gear_Sound(v, ChType_Usr0() == v, &g_GearLoadWeaponSound, &g_GearLoadWeaponSoundVol);
}

/* params: the gear, an event, a float. The missile "GearMissile" from the gun of the gear: aimed for an AI or a
 * character that sets its view, else straight ahead; the shot of the player calls the sentinels. */
void ChType_GearShoot(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t a, rot, off, t1, t2, t3, start, dir, ang, aim, ma;
	cEntity *target = NULL;
	byte usr;

	ChType_ReadParams(params, name, event);
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	a = *WChar_Ang(v);
	rot.x = a.x * 0.5f;
	rot.y = a.y;
	rot.z = 0.0f;
	off.x = g_GearShootPosX;
	off.y = g_GearShootPosY;
	off.z = g_GearShootPosZ;
	const vec3_t *r = Vec3_Rotate(&t1, &off, &rot);
	const vec3_t *p = v->vftable->GetPos(v, &t2);
	start.x = r->x + p->x;
	start.y = p->y + r->y;
	start.z = p->z + r->z;
	/* the rotated direction is not used */
	dir.x = 1000.0f;
	dir.y = 1500.0f;
	dir.z = 0.0f;
	Vec3_Rotate(&t3, &dir, &a);
	float dist = g_GearShootDistance;
	ang.x = 0.0f;
	ang.y = 0.0f;
	ang.z = 0.0f;
	v->vftable->GetTargetAngle(v, &ang.x, dist, &target);
	v->vftable->AimWeapon(v, ang.x, ang.y, &dist, &aim, &target, 0x11);
	if (target) {
		target->vftable->IsType(target, TYEN_WITH_LIFE);
	}
	if (ChType_Usr0() == v) {
		usr = 1;
		AICtrl_SendSentinelToWatch(&g_pWorld->logic.aiCtrl, v->vftable->GetPos(v, &t2));
		Alarm_Set(&g_pWorld->alarm, g_pWorld->alarm.value + g_GearShootFailAlarm);
	} else {
		usr = 0;
	}
	if (WChar_Cam(v)) {
		cPad_Rumble(&g_Pads[0], 0.75f, 0.0f, 0.125f);
	}
	cEntity *e = cWorld_CreateEntity(g_pWorld, "GearMissile", start.x, start.y, start.z, TYEN_MISSILE);
	if (e) {
		cMissileEntity *m = Gear_Missile(e);
		cMissileEntity_SetHiddenSwarm(m, 1);
		Obj_Vtbl(e)->SetAng(e, &ang);
		cMissileEntity_SetLifeTime(m, 5.0f);
		cMissileEntity_SetSwarmSpeed(m, 20000.0f);
		if (!WChar_God(e)) {
			WChar_Life(e) = 50;
		}
		cMissileEntity_SetNoWorldHit(m, 0);
		cMissileEntity_SetFlag584(m, 0);
		cMissileEntity_SetFlag585(m, 1);
		cPrimitive_SetSize(WChar_Shadow(e), 40.0f);
		cMissileEntity_SetRadius(m, 50.0f);
		cMissileEntity_SetTopRadius(m, 50.0f);
		cMissileEntity_SetRadiusGrow(m, 2000.0f);
		cMissileEntity_SetSpeed(m, g_GearMissileSpeed);
		cMissileEntity_SetTurnSpeed(m, 50.0f);
		cPy_Function_SetNative(&m->onHit, (nativefunc_t)GAME_FUNC(ChType_GearHit, CHTYPE_GEARHIT));
		cMissileEntity_SetParent(m, name);
		if (!WChar_ViewLock(v)) {
			vec3_t va = *WChar_Ang(v);
			ma.x = va.x;
			ma.y = Obj_Vtbl(v)->GetAng(v, &t2)->y;
			ma.z = 0.0f;
			Obj_Vtbl(e)->SetAng(e, &ma);
			cMissileEntity_SetSpeed(m, g_GearMissileSpeed);
		} else if (usr) {
			cMissileEntity_SetTarget(m, g_GearTargetName);
		} else if (target) {
			cMissileEntity_SetTarget(m, target->name);
		}
		Weap_Vtbl(e)->AddBees(e, 0);
		e->vftable->SetEmbeddedFX(e, "GearMissile");
		cSndInterface *snd = WChar_Sounds(e);
		cSndInterface_Resize(snd, 1);
		cSndInterface_SetSound(snd, 0, g_GearMissileSound);
		cSndItem *s = PtrArray_Get(snd, 0);
		if (s) {
			s->vols[0] = g_GearMissileSoundVol;
			s->sound.desc.loop = 1;
		}
	}
	Gear_Sound(v, usr, &g_GearShootSound, &g_GearShootSoundVol);
	float used = WChar_EnergyBarDecr(v) + WChar_EnergyUsed(v);
	WChar_EnergyUsed(v) = used;
	if (used >= 1.0f) {
		WChar_EnergyUsed(v) = 1.0f;
	}
}

/* the color ramps of the shot of the gear (9 colors each): 0..3 sparks and flashes, 2 also the core */
#define g_RampGearShoot (GAME_ARRAY(uint32_t, g_RampGearShoot, 0x428078))

/* rand() & 0x7fff as 0..1 (double), unscaled */
static inline double Gear_Rnd(void) {
	return (double)(crt_rand() & 0x7fff) * (1.0 / 32767.0);
}

static inline void Gear_Pos(CGameParticles *p, float x, float y, float z) {
	p->initPosMin.x = x;
	p->initPosMin.y = y;
	p->initPosMin.z = z;
	p->initPosMax = p->initPosMin;
}

static inline void Gear_Vel(CGameParticles *p, float x, float y, float z) {
	p->initVelMin.x = x;
	p->initVelMin.y = y;
	p->initVelMin.z = z;
	p->initVelMax = p->initVelMin;
}

static inline void Gear_Acc(CGameParticles *p, float x, float y, float z) {
	p->initAccMin.x = x;
	p->initAccMin.y = y;
	p->initAccMin.z = z;
	p->initAccMax = p->initAccMin;
}

/* the acceleration against the velocity, w of it */
static inline void Gear_Brake(CGameParticles *p, float w) {
	Gear_Acc(p, -p->initVelMin.x * w, -p->initVelMin.y * w, -p->initVelMin.z * w);
}

static inline void Gear_Rot(CGameParticles *p) {
	float rot = FX_Rand(6.2831854820251465, 0.0);
	p->rotIni = rot;
	p->rotEnd = rot;
}

static inline void Gear_Emit(CGameParticles *p, uint32_t *ramp, float dt) {
	CGameParticles_Apply(p);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, ramp);
	CGameParticles_EmitDt(p, 1, 0, dt);
}

/* a flash at the gun that falls down from y */
static void Gear_Flash(CGameParticles *p, float y, double sizeR, double sizeA, double endR, double endA, byte sprite,
	float dt) {
	p->birthRate = 0;
	Gear_Pos(p, 0.0f, y, 0.0f);
	Gear_Vel(p, 0.0f, 0.0f, 0.0f);
	Gear_Acc(p, 0.0f, 0.0f, 0.0f);
	Gear_Rot(p);
	p->rotVar = FX_Rand(0.7853981852531433, -0.39269909262657166);
	p->sizeLife = 0.1f;
	p->lifeTime = FX_Rand(0.019999995827674866, 0.18000000715255737);
	p->sizeIni = FX_Rand(sizeR, sizeA);
	p->spriteIni = sprite;
	p->spriteEnd = sprite;
	p->sizeEnd = FX_Rand(endR, endA);
	Gear_Emit(p, g_RampGearShoot + 18, dt);
}

/* params: the gear, an event and the delay of the end flashes. The charge of the electric ball on the shot particles
 * of its embedded effect (visible models only): 32 sparks, 20 and 5 glows flying out, three flashes falling from the
 * gun, two end flashes and 12 glows thrown up */
void ChType_GearShootLoad(cAbstractFile *params) {
	char name[0x80], event[0x80];
	float delay;

	cAbstractFile_ReadString(params, name, 0x80);
	cAbstractFile_ReadString(params, event, 0x80);
	params->vftable->Read(params, &delay, 4);
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	CGameModel *model = Obj_Vtbl(v)->GetModel(v);
	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	if (CGameModel_IsLoaded(model) && !CGameModel_IsVisible(model)) {
		return;
	}
	cEmbeddedFXGear *fx = WChar_FX(v);
	if (!fx) {
		return;
	}
	CGameParticles *p = &fx->shoot;
	CHECK(p->ParticleSystem);
	for (int i = 0; i < 32; i++) {
		float dt = FX_Rand(0.0010000001639127731, 0.008999999612569809) * (float)i;
		double r = Gear_Rnd();
		float y = FX_Rand(0.699999988079071, 0.30000001192092896);
		float x = FX_Rand(2.0, -1.0);
		y = -y;
		float z = (float)(r + r - 1.0);
		CGameParticles_SetSparksOn(p, 1);
		p->birthRate = 0;
		float s = FX_Rand(20.0, 100.0);
		Gear_Pos(p, s * x, s * y, s * z);
		float sp = FX_Rand(100.0, 450.0);
		Gear_Vel(p, -x * sp, -y * sp, -z * sp);
		Gear_Brake(p, FX_Rand(0.09999996423721313, 0.800000011920929));
		p->lifeTime = FX_Rand(0.030000001192092896, 0.2199999988079071);
		float size = FX_Rand(10.0, 105.0);
		p->rotIni = 0.0f;
		p->rotEnd = 0.0f;
		p->rotVar = 0.0f;
		p->sparkVelScale = 10.0f;
		p->sizeIni = size;
		p->sizeEnd = size;
		p->sparkMaxSize = (float)i * 0.07f + 1.0f;
		byte sprite = (~crt_rand() & 1) | 0x46;
		p->spriteIni = sprite;
		p->spriteEnd = sprite;
		Gear_Emit(p, g_RampGearShoot + (crt_rand() % 3) * 9, dt);
		CGameParticles_SetSparksOn(p, 0);
	}
	for (int i = 0; i < 20; i++) {
		double r = Gear_Rnd();
		float y = FX_Rand(1.0, 0.5);
		p->birthRate = 0;
		float x = FX_Rand(2.0, -1.0);
		y = -y;
		float z = (float)(r + r - 1.0);
		float s = FX_Rand(10.0, 60.0);
		Gear_Pos(p, x * s, y * s, z * s);
		float sp = FX_Rand(205.0, 350.0);
		Gear_Vel(p, -x * sp, -y * sp, -z * sp);
		Gear_Brake(p, FX_Rand(0.09999996423721313, 0.6000000238418579));
		Gear_Rot(p);
		p->rotVar = FX_Rand(3.1415927410125732, 0.0);
		p->sizeLife = 1.0f;
		p->lifeTime = FX_Rand(0.019999995827674866, 0.20000000298023224);
		p->sizeIni = FX_Rand(10.0, 110.0);
		p->spriteIni = 0x29;
		p->spriteEnd = 0x29;
		p->sizeEnd = FX_Rand(5.0, 70.0);
		CGameParticles_Apply(p);
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetColorRamp(p, g_RampGearShoot + (crt_rand() & 1) * 9);
		CGameParticles_EmitDt(p, 1, 0, FX_Rand(0.05000000074505806, 0.0) + (float)i * 0.017f);
	}
	for (int i = 0; i < 5; i++) {
		double r = Gear_Rnd();
		float y = FX_Rand(0.6000000238418579, 0.4000000059604645);
		p->birthRate = 0;
		float x = FX_Rand(2.0, -1.0);
		y = -y;
		float z = (float)(r + r - 1.0);
		float s = FX_Rand(5.0, 15.0);
		Gear_Pos(p, x * s, y * s, z * s);
		float sp = FX_Rand(50.0, 125.0);
		Gear_Vel(p, -x * sp, -y * sp, -z * sp);
		Gear_Brake(p, FX_Rand(0.09999996423721313, 0.800000011920929));
		p->rotVar = 0.0f;
		Gear_Rot(p);
		p->sizeLife = 1.0f;
		p->lifeTime = FX_Rand(0.04999999701976776, 0.20000000298023224);
		p->sizeIni = FX_Rand(50.0, 50.0);
		p->spriteIni = 2;
		p->spriteEnd = 2;
		p->sizeEnd = FX_Rand(25.0, 225.0);
		CGameParticles_Apply(p);
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetColorRamp(p, g_RampGearShoot + 18);
		CGameParticles_EmitDt(p, 1, 0, FX_Rand(0.06000000238418579, 0.17000000178813934) + (float)i * 0.048f);
	}
	int k = 0;
	for (int d = 0; d < 0x3c; d += 0x14) {
		p->birthRate = 0;
		Gear_Pos(p, 0.0f, -10.0f - (float)d, 0.0f);
		Gear_Vel(p, 0.0f, 0.0f, -400.0f);
		Gear_Acc(p, 0.0f, 0.0f, 0.0f);
		Gear_Rot(p);
		p->rotVar = FX_Rand(1.5707963705062866, 0.0);
		p->sizeLife = 0.1f;
		p->lifeTime = FX_Rand(0.019999995827674866, 0.18000000715255737);
		p->sizeIni = FX_Rand(50.0, 50.0);
		p->sizeEnd = FX_Rand(50.0, 250.0);
		p->spriteIni = 6;
		p->spriteEnd = 6;
		Gear_Emit(p, g_RampGearShoot + 18, (float)k * 0.022f + 0.4f);
		k++;
	}
	Gear_Flash(p, -30.0f, 75.0, 225.0, 100.0, 550.0, 2, delay + 0.41f);
	Gear_Flash(p, -40.0f, 125.0, 325.0, 150.0, 1100.0, 6, delay + 0.41f);
	for (int i = 0; i < 12; i++) {
		float z = FX_Rand(0.4000000059604645, -0.20000000298023224);
		float y = FX_Rand(0.699999988079071, 0.800000011920929);
		p->birthRate = 0;
		Gear_Pos(p, 0.0f, 0.0f, 0.0f);
		float x = FX_Rand(0.4000000059604645, -0.20000000298023224);
		y = -y;
		float sp = FX_Rand(175.0, 450.0);
		Gear_Vel(p, x * sp, y * sp, z * sp);
		Gear_Brake(p, FX_Rand(0.09999996423721313, 0.6000000238418579));
		Gear_Rot(p);
		p->rotVar = FX_Rand(3.1415927410125732, 0.0);
		p->sizeLife = 1.0f;
		p->lifeTime = FX_Rand(0.02000001072883606, 0.1599999964237213);
		p->sizeIni = FX_Rand(15.0, 50.0);
		p->spriteIni = 0x29;
		p->spriteEnd = 0x29;
		p->sizeEnd = FX_Rand(20.0, 105.0);
		CGameParticles_Apply(p);
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetColorRamp(p, g_RampGearShoot + (crt_rand() & 3) * 9);
		CGameParticles_EmitDt(p, 1, 0, (float)i * 0.014f + 0.36f);
	}
}

/* the flash around pos (radius): ChType_FlashHit for each character in the sphere, at most max of them flashed (-1
 * for all); 2 when one got flashed, 1 when one was flashed already, else 0 */
int Logic_Flash(const vec3_t *pos, float radius, int max) {
	g_FlashResult = 0;
	g_FlashCount = 0;
	g_FlashPos = *pos;
	g_FlashMax = max;
	cPy_Function_SetNative(g_ChTypeSphereFunc, (nativefunc_t)GAME_FUNC(ChType_FlashHit, CHTYPE_FLASHHIT));
	cWorld_SphereCallF(g_pWorld, 0x10, &g_FlashPos, radius, g_ChTypeSphereFunc, 1);
	return g_FlashResult;
}

/* the flashed state of e: 1 when it runs already (its time from 0 again), 2 when it starts (OnFlash with the name
 * then), 0 without one */
int ChType_FlashChar(cEntity *e) {
	int r = 0;

	void *flashed = ChType_Vtbl(e)->FindAction(e, "Flashed");
	if (!flashed) {
		return 0;
	}
	if (flashed == ChType_LayerAction(cActCtrl_GetLayer(ChType_ActCtrl(e), 0))) {
		ChType_LayerTime(cActCtrl_GetLayer(ChType_ActCtrl(e), 0)) = 0.0f;
		return 1;
	}
	if (cActCtrl_PlayNew(ChType_ActCtrl(e), "FlashedInit", 0, 1.0f)) {
		cActCtrl_SetDefLoop(ChType_ActCtrl(e), "Relax", 1);
		WChar_B381(e) = 0;
		r = 2;
		if (cPy_Function_IsValid(g_OnFlash)) {
			cPy_Function_AddParamS(g_OnFlash, e->name);
			cPy_Function_Execute(g_OnFlash, 0);
		}
	}
	return r;
}

/* the sphere test of Logic_Flash: params a character; an AI that looks within 90 degrees of the flash gets
 * flashed (up to the count) */
void ChType_FlashHit(cAbstractFile *params) {
	char name[0x80];
	vec3_t t1, d, r;

	cAbstractFile_ReadString(params, name, 0x80);
	if (!name[0]) {
		return;
	}
	cEntity *e = World_FindEntity(name);
	if (!e || !e->vftable->IsType(e, TYEN_WALKCHAR) || cWalkCharEntity_GetOwnerSlot(e) != -1) {
		return;
	}
	void *flashed = ChType_Vtbl(e)->FindAction(e, "Flashed");
	if (!flashed) {
		return;
	}
	const vec3_t *p = e->vftable->GetPos(e, &t1);
	d.y = g_FlashPos.y - p->y;
	d.z = g_FlashPos.z - p->z;
	d.x = g_FlashPos.x - p->x;
	Vec3_GetRotAng(&r, &d);
	float n = NormAng(Obj_Vtbl(e)->GetAng(e, &t1)->y - r.y);
	if (FLASH_ANGLE > __builtin_fabs((double)n)) {
		return;
	}
	if (flashed == ChType_LayerAction(cActCtrl_GetLayer(ChType_ActCtrl(e), 0))) {
		if (g_FlashResult <= 0) {
			g_FlashResult = 1;
		}
		ChType_LayerTime(cActCtrl_GetLayer(ChType_ActCtrl(e), 0)) = 0.0f;
		return;
	}
	if (g_FlashMax != -1 && g_FlashCount >= g_FlashMax) {
		return;
	}
	if (cActCtrl_PlayNew(ChType_ActCtrl(e), "FlashedInit", 0, 1.0f)) {
		g_FlashCount++;
		cActCtrl_SetDefLoop(ChType_ActCtrl(e), "Relax", 1);
		WChar_B381(e) = 0;
		g_FlashResult = 2;
		if (cPy_Function_IsValid(g_OnFlash)) {
			cPy_Function_AddParamS(g_OnFlash, e->name);
			cPy_Function_Execute(g_OnFlash, 0);
		}
	}
}
