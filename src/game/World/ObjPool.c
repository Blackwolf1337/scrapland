/*
 * World/ObjPool.cpp of the game: the debris systems (cDebrisSys), named pools of debris (cDebris: a model with a
 * move in 0.05 s steps, or the carrier of an effect cObjElementFX), see include/scrap/scrap.h. The list at 0x0 holds
 * the free debris (cDebrisSys_FreeActive frees it), the one at 0x4 the dropped ones (cDebrisSys_FreeIdle).
 */
#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxmisc.h"
#include "scrap/fxscene.h"
#include "scrap/missileentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmodel.h"
#include "scrap/world.h"
#include "scrap/fxcharactions.h"
#include "scrap/fxvehicle.h"
#include "scrap/fxweapons.h"
#include "scrap/staticinit.h"

/* defined further down or in another file without a header */
void *cObjElementFXCharacterConversion_ctor(void *self);
void *cObjElementFXCharacterConversor_ctor(void *self);
void *cObjElementFXCharacterDeath_ctor(void *self);
void *cObjElementFXCharacterTeleport_ctor(void *self);
void *cObjElementFXMaintenanceWork_ctor(void *self);
void *cObjElementFXMoneyTransfer_ctor(void *self);
void *cObjElementFXSceneBeatPickus_ctor(void *self);
void *cObjElementFXSceneBillArrives_A_ctor(void *self);
void *cObjElementFXSceneBishops_ctor(void *self);
void *cObjElementFXSceneDecontamination_ctor(void *self);
void *cObjElementFXSceneEnterMatrix_ctor(void *self);
void *cObjElementFXSceneHumanCD_ctor(void *self);
void *cObjElementFXSceneTheEndB_ctor(void *self);
void *cObjElementFXSceneYakuzzi_ctor(void *self);
void *cObjElementFXSputnik_ctor(void *self);

static const cObjElementFXVtbl cObjElementAbstract_vftable;
#define COBJELEMENTABSTRACT_VFTABLE (&cObjElementAbstract_vftable)
#define DEBRIS_STEP 0.05f
#define DEBRIS_MAXSTEP 0.1f

static cGVar GVar_O_Show;
#define g_OShow (GVar_O_Show.value.i)	/* O_Show, the debug text of the pools */

/* the constructor of an effect class (returns the object) */
typedef cObjElementFX *(*ObjElementFXCtor_t)(void *mem);

/* the effect of type for the debris (the old one deleted) and its Start; 0 without one */
static byte cDebris_SetFX(int type, cDebris *d) {
	/* the effects by type: the size and the constructor of the class */
	const struct {
		int type;
		uint32_t size;
		void *ctor;
	} debrisFX[] = {
		{1, sizeof(cObjElementFXParticleSystem), (void *)cObjElementFXParticleSystem_ctor},
		{2, sizeof(cObjElementFXTrail), (void *)cObjElementFXTrail_ctor},
		{3, sizeof(cObjElementFXFadeOutTrail), (void *)cObjElementFXFadeOutTrail_ctor},
		{4, sizeof(cObjElementFXSparks), (void *)cObjElementFXSparks_ctor},
		{5, sizeof(cObjElementFXModelFade), (void *)cObjElementFXModelFade_ctor},
		{6, sizeof(cObjElementFXItemFade), (void *)cObjElementFXItemFade_ctor},
		{7, sizeof(cObjElementFXItemTake), (void *)cObjElementFXItemTake_ctor},
		{8, sizeof(cObjElementFXParticleSystemDuDv), (void *)cObjElementFXParticleSystemDuDv_ctor},
		{30, sizeof(cObjElementFXFiredEntity), (void *)cObjElementFXFiredEntity_ctor},
		{31, sizeof(cObjElementFXHook), (void *)cObjElementFXHook_ctor},
		{32, sizeof(cObjElementFXSwarmMissile), (void *)cObjElementFXSwarmMissile_ctor},
		{33, sizeof(cObjElementFXTesla), (void *)cObjElementFXTesla_ctor},
		{34, sizeof(cObjElementFXATPC), (void *)cObjElementFXATPC_ctor},
		{35, sizeof(cObjElementFXEMI), (void *)cObjElementFXEMI_ctor},
		{36, sizeof(cObjElementFXVehicleEMI), (void *)cObjElementFXVehicleEMI_ctor},
		{38, sizeof(cObjElementFXVehicleDamage), (void *)cObjElementFXVehicleDamage_ctor},
		{39, sizeof(cObjElementFXVehicleSputnik), (void *)cObjElementFXVehicleSputnik_ctor},
		{40, sizeof(cObjElementFXFiredDebris), (void *)cObjElementFXFiredDebris_ctor},
		{41, sizeof(cObjElementFXLaser), (void *)cObjElementFXLaser_ctor},
		{42, sizeof(cObjElementFXDebrisCloud), (void *)cObjElementFXDebrisCloud_ctor},
		{43, sizeof(cObjElementFXInfernoMissile), (void *)cObjElementFXInfernoMissile_ctor},
		{44, sizeof(cObjElementFXInfernoExplosion), (void *)cObjElementFXInfernoExplosion_ctor},
		{45, sizeof(cObjElementFXVehicleDeathTrail), (void *)cObjElementFXVehicleDeathTrail_ctor},
		{46, sizeof(cObjElementFXDevastatorShoot), (void *)cObjElementFXDevastatorShoot_ctor},
		{47, sizeof(cObjElementFXVehicleAccelerator), (void *)cObjElementFXVehicleAccelerator_ctor},
		{48, sizeof(cObjElementFXSmokeDebris), (void *)cObjElementFXSmokeDebris_ctor},
		{49, sizeof(cObjElementFXVehicleFire), (void *)cObjElementFXVehicleFire_ctor},
		{50, sizeof(cObjElementFXSonicExplosion), (void *)cObjElementFXSonicExplosion_ctor},
		{51, sizeof(cObjElementFXModelNoise), (void *)cObjElementFXModelNoise_ctor},
		{60, sizeof(cObjElementFXSputnik), (void *)cObjElementFXSputnik_ctor},
		{61, sizeof(cObjElementFXCharacterDeath), (void *)cObjElementFXCharacterDeath_ctor},
		{62, sizeof(cObjElementFXCharacterConversor), (void *)cObjElementFXCharacterConversor_ctor},
		{63, sizeof(cObjElementFXCharacterConversion), (void *)cObjElementFXCharacterConversion_ctor},
		{64, sizeof(cObjElementFXCharacterTeleport), (void *)cObjElementFXCharacterTeleport_ctor},
		{71, sizeof(cObjElementFXGearMissile), (void *)cObjElementFXGearMissile_ctor},
		{72, sizeof(cObjElementFXBankerSteal), (void *)cObjElementFXBankerSteal_ctor},
		{73, sizeof(cObjElementFXPoliceSteal), (void *)cObjElementFXPoliceSteal_ctor},
		{74, sizeof(cObjElementFXFunctionarySpecialAction), (void *)cObjElementFXFunctionarySpecialAction_ctor},
		{75, sizeof(cObjElementFXMaintenanceWork), (void *)cObjElementFXMaintenanceWork_ctor},
		{76, sizeof(cObjElementFXPoliceGearConversion), (void *)cObjElementFXPoliceGearConversion_ctor},
		{77, sizeof(cObjElementFXDTritusKill), (void *)cObjElementFXDTritusKill_ctor},
		{78, sizeof(cObjElementFXDTritusAttack), (void *)cObjElementFXDTritusAttack_ctor},
		{79, sizeof(cObjElementFXBishopSellLife), (void *)cObjElementFXBishopSellLife_ctor},
		{80, sizeof(cObjElementFXBishopAttack), (void *)cObjElementFXBishopAttack_ctor},
		{81, sizeof(cObjElementFXBertoAttack), (void *)cObjElementFXBertoAttack_ctor},
		{82, sizeof(cObjElementFXBertoFlash), (void *)cObjElementFXBertoFlash_ctor},
		{83, sizeof(cObjElementFXMoneyTransfer), (void *)cObjElementFXMoneyTransfer_ctor},
		{84, sizeof(cObjElementFXMessengerAttack), (void *)cObjElementFXMessengerAttack_ctor},
		{85, sizeof(cObjElementFXMessengerTakeDataPack), (void *)cObjElementFXMessengerTakeDataPack_ctor},
		{86, sizeof(cObjElementFXSebastianFlash), (void *)cObjElementFXSebastianFlash_ctor},
		{87, sizeof(cObjElementFXBettyAttack), (void *)cObjElementFXBettyAttack_ctor},
		{88, sizeof(cObjElementFXBettyKill), (void *)cObjElementFXBettyKill_ctor},
		{89, sizeof(cObjElementFXBankMasterAttack), (void *)cObjElementFXBankMasterAttack_ctor},
		{90, sizeof(cObjElementFXBankMasterSuction), (void *)cObjElementFXBankMasterSuction_ctor},
		{91, sizeof(cObjElementFXMercenaryDrink), (void *)cObjElementFXMercenaryDrink_ctor},
		{100, sizeof(cObjElementFXSceneDecontamination), (void *)cObjElementFXSceneDecontamination_ctor},
		{101, sizeof(cObjElementFXSceneBishops), (void *)cObjElementFXSceneBishops_ctor},
		{102, sizeof(cObjElementFXSceneBillArrives_A), (void *)cObjElementFXSceneBillArrives_A_ctor},
		{103, sizeof(cObjElementFXSceneYakuzzi), (void *)cObjElementFXSceneYakuzzi_ctor},
		{104, sizeof(cObjElementFXSceneBeatPickus), (void *)cObjElementFXSceneBeatPickus_ctor},
		{105, sizeof(cObjElementFXSceneHumanCD), (void *)cObjElementFXSceneHumanCD_ctor},
		{106, sizeof(cObjElementFXSceneEnterMatrix), (void *)cObjElementFXSceneEnterMatrix_ctor},
		{107, sizeof(cObjElementFXSceneTheEndB), (void *)cObjElementFXSceneTheEndB_ctor},
	};
	unsigned i;

	if (!d) {
		return 0;
	}
	if (d->fx) {
		d->fx->vftable->Delete(d->fx, 1);
		d->fx = NULL;
	}
	for (i = 0; i < sizeof(debrisFX) / sizeof(debrisFX[0]); i++) {
		if (debrisFX[i].type == type) {
			void *mem = operator_new(debrisFX[i].size);

			d->fx = mem ? ((ObjElementFXCtor_t)debrisFX[i].ctor)(mem) : NULL;
			break;
		}
	}
	if (type && d->fx) {
		d->fx->vftable->Start(d->fx, d);
		return 1;
	}
	return 0;
}

/* ---- cDebrisSys ---- */

cDebrisSys *cDebrisSys_ctor(void *mem) {
	cDebrisSys *sys = mem;

	cString_ctor(&sys->name);
	sys->idle = NULL;
	sys->idleCount = 0;
	sys->maxIdle = 0x100;
	sys->next = NULL;
	sys->active = NULL;
	sys->time = 0.0f;
	sys->radiusSq = 1e8f;
	sys->b = 1.0f;
	sys->c = 1.0f;
	sys->d = 0.0f;
	sys->flag = 0;
	sys->onHit = NULL;
	sys->fxType = 0;
	return sys;
}

void cObjElement_dtor(void *self) {
	((cObjElementFX *)self)->vftable = COBJELEMENTABSTRACT_VFTABLE;
}

void cDebrisSys_dtor(cDebrisSys *sys) {
	cDebrisSys_FreeActive(sys);
	cDebrisSys_FreeIdle(sys);
	cString_dtor(&sys->name);
}

static void Debris_Free(cDebris *d) {
	if (d->fx) {
		d->fx->vftable->Delete(d->fx, 1);
	}
	CGameModel_Reset(Debris_Mod(d));
	operator_delete_sized(d, sizeof(*d));
}

/* the move of a step of dt without an effect */
static void Debris_Move(cDebris *d, float dt) {
	vec3_t v;

	v.x = d->drop[0].x + d->drop[2].x * dt;
	v.y = d->drop[0].y + d->drop[2].y * dt;
	v.z = d->drop[0].z + d->drop[2].z * dt;
	CGameModel_SetPos(Debris_Mod(d), &v);
	v.x = d->drop[3].x * dt + d->drop[1].x;
	v.y = d->drop[1].y + d->drop[3].y * dt;
	v.z = d->drop[1].z + d->drop[3].z * dt;
	CGameModel_SetRot(Debris_Mod(d), &v);
}

/* the model moved by the part dt of the next step (the effect does it with one) */
void cDebris_Step(cDebris *d, float dt) {
	if (d->fx) {
		d->fx->vftable->Update(d->fx, d, dt);
		return;
	}
	Debris_Move(d, dt);
}

void cDebrisSys_Drop(cDebrisSys *sys, cDebris *d) {
	float t;

	if (!d->fx && !(sys->radiusSq > cWorld_GetMinCamDistSq(g_pWorld, &d->drop[0]))) {
		cDebrisSys_Release(sys, d);
		return;
	}
	d->next = sys->active;
	sys->active = d;
	CGameModel_Enable(Debris_Mod(d));
	/* back by the time left, the next steps move it there again */
	t = sys->time;
	d->drop[1].x = d->drop[1].x - d->drop[3].x * t;
	d->drop[1].y = d->drop[1].y - d->drop[3].y * t;
	d->drop[1].z = d->drop[1].z - d->drop[3].z * t;
	t = sys->time;
	d->drop[0].x = d->drop[0].x - d->drop[2].x * t;
	d->drop[0].y = d->drop[0].y - d->drop[2].y * t;
	d->drop[0].z = d->drop[0].z - d->drop[2].z * t;
}

/* the list at 0x0 freed */
void cDebrisSys_FreeActive(cDebrisSys *sys) {
	while (sys->idle) {
		cDebris *d = sys->idle;

		sys->idle = d->next;
		Debris_Free(d);
	}
	sys->idleCount = 0;
}

/* the list at 0x4 freed */
void cDebrisSys_FreeIdle(cDebrisSys *sys) {
	while (sys->active) {
		cDebris *d = sys->active;

		sys->active = d->next;
		Debris_Free(d);
	}
}

cDebris *cDebrisSys_NewElement(cDebrisSys *sys) {
	cDebris *d;
	vec3_t scale;
	void *mem;

	CHECK(sys->name);
	d = sys->idle;
	if (d) {
		sys->idleCount--;
		sys->idle = d->next;
		d->next = NULL;
		return d;
	}
	mem = operator_new(sizeof(cDebris));
	d = NULL;
	if (mem) {
		CGameModel_ctor(mem, NULL);
		d = mem;
	}
	scale.x = scale.y = scale.z = sys->c;
	CGameModel_LoadScaled(Debris_Mod(d), sys->name, &scale);
	CGameModel_SetPivotNode(Debris_Mod(d), "DC_Root");
	d->drop[0].x = 0.0f;
	d->drop[0].y = 0.0f;
	d->drop[0].z = 0.0f;
	d->drop[1].x = 0.0f;
	d->drop[1].y = 0.0f;
	d->drop[1].z = 0.0f;
	d->drop[2].x = 0.0f;
	d->drop[2].y = 0.0f;
	d->drop[2].z = 0.0f;
	d->drop[3].x = 0.0f;
	d->drop[3].y = 0.0f;
	d->drop[3].z = 0.0f;
	d->life = 0.0f;
	d->next = NULL;
	d->fx = NULL;
	cDebris_SetFX(sys->fxType, d);
	if (sys->active) {
		Con_Debugf("WARN: ObjPool '%s' exceds cache limit (dynamic memory usage)\n", sys->name);
	}
	d->next = NULL;
	return d;
}

/* the first dropped debris, else a new one */
cDebris *cDebrisSys_GetIdle(cDebrisSys *sys) {
	if (sys->active) {
		return sys->active;
	}
	return cDebrisSys_NewElement(sys);
}

/* n debris of the effect type made and put back into the pool (the ones over maxIdle freed) */
void cDebrisSys_Init(cDebrisSys *sys, int n, int type) {
	cDebris *d = NULL;

	sys->fxType = type;
	if (n <= 0) {
		return;
	}
	do {
		cDebris *p = cDebrisSys_NewElement(sys);

		p->next = d;
		d = p;
	} while (--n);
	do {
		cDebris *next = d->next;

		cDebrisSys_Release(sys, d);
		d = next;
	} while (d);
}

byte cDebrisSys_IsIdle(cDebrisSys *sys, cDebris *d) {
	return d == sys->active;
}

void cDebris_Move(cDebris *d, float dt) {
	vec3_t v;

	v.x = d->drop[0].x + dt * d->drop[2].x;
	v.y = d->drop[0].y + d->drop[2].y * dt;
	v.z = d->drop[0].z + d->drop[2].z * dt;
	CGameModel_SetPos(Debris_Mod(d), &v);
	v.x = d->drop[1].x + d->drop[3].x * dt;
	v.y = d->drop[1].y + d->drop[3].y * dt;
	v.z = d->drop[1].z + d->drop[3].z * dt;
	CGameModel_SetRot(Debris_Mod(d), &v);
}

/* one step of 0.05 s: life, gravity, friction and the collision with the floor; 0 when the debris is gone */
byte cDebris_Update(cDebris *d, cDebrisSys *sys) {
	vec3_t from, to;
	vec3_t *pos = &d->drop[0], *vel = &d->drop[2];
	CCollisionTri tri;
	float k;

	if (sys->radiusSq != 0.0f && cWorld_GetMinCamDistSq(g_pWorld, pos) >= sys->radiusSq) {
		return 0;
	}
	d->life = d->life - DEBRIS_STEP;
	if (0.0f >= d->life) {
		return 0;
	}
	from = *pos;
	to.x = pos->x + vel->x * DEBRIS_STEP;
	to.y = pos->y + vel->y * DEBRIS_STEP;
	to.z = pos->z + vel->z * DEBRIS_STEP;
	d->drop[1].x = d->drop[3].x * DEBRIS_STEP + d->drop[1].x;
	d->drop[1].y = d->drop[1].y + d->drop[3].y * DEBRIS_STEP;
	d->drop[1].z = d->drop[1].z + d->drop[3].z * DEBRIS_STEP;
	/* the gravity is on y only, the + 0 of x and z is kept (it turns -0 into +0) */
	vel->x = vel->x + 0.0f;
	vel->y = vel->y + sys->d * -DEBRIS_STEP;
	vel->z = vel->z + 0.0f;
	k = sys->b;
	vel->x = vel->x - vel->x * DEBRIS_STEP * k;
	vel->y = vel->y - vel->y * DEBRIS_STEP * k;
	vel->z = vel->z - vel->z * DEBRIS_STEP * k;
	if (!sys->flag) {
		*pos = to;
		return 1;
	}
	if (!Engine_TestLine(&from, &to, pos, &tri)) {
		return 1;
	}
	if (sys->onHit) {
		sys->onHit(d, &tri);
		return 1;
	}
	vel->y = -vel->y * 0.9f;
	vel->z = -vel->z * 0.9f;
	vel->x = -vel->x * 0.9f;
	d->drop[3].x = -d->drop[3].x;
	d->drop[3].y = -d->drop[3].y;
	d->drop[3].z = -d->drop[3].z;
	Vec3_Reflect(&tri.plane.n, vel, vel);
	return 1;
}

/* ---- preloads of the models and animations of the level ---- */

void Object3D_Preload(const char *name, const vec3_t *scale) {
	CGameModel mod;

	CGameModel_ctor(&mod, NULL);
	CGameModel_LoadScaled(&mod, name, scale);
	CGameModel_Reset(&mod);
}

void Anm_Preload(const char *model, const char *anim) {
	CGameModel mod;

	CGameModel_ctor(&mod, NULL);
	CGameModel_LoadName(&mod, model);
	CGameModel_AddAnim(&mod, anim);
	CGameModel_Reset(&mod);
}

void cDebrisSys_ShowInfo(cDebrisSys *sys, int x, int y, float *line) {
	cDebris *d;
	float fy;

	if (!g_OShow) {
		return;
	}
	d = sys->active;
	if (!d) {
		return;
	}
	CFont_PrintfColor(g_FontTerminal8, (float)x, (float)y, 0xffff00, "Object Pool: %s", sys->name);
	fy = (float)y;
	do {
		vec3_t a, b, c;
		const vec3_t *p;
		const char *fx;
		float z, my;

		fy = fy + 10.0f;
		p = CGameModel_GetPos(Debris_Mod(d), &a);
		fx = d->fx ? "(AbstractData)" : "";
		z = CGameModel_GetPos(Debris_Mod(d), &b)->z;
		my = CGameModel_GetPos(Debris_Mod(d), &c)->y;
		CFont_PrintfColor(g_FontTerminal8, (float)x + 10.0f, fy, 0xff00,
			"Pos:(%.2f,%.2f,%.2f) Model:(%.2f,%.2f,%.2f) Time:%f %s", (double)d->drop[0].x,
			(double)d->drop[0].y, (double)d->drop[0].z, (double)p->x, (double)my, (double)z, (double)d->life,
			fx);
		d = d->next;
	} while (d);
	if (line) {
		*line = fy + 20.0f;
	}
}

/* the step of the debris: by its effect or the own move */
static byte cDebris_IsAlive(cDebris *d, cDebrisSys *sys) {
	if (d->fx) {
		return d->fx->vftable->IsAlive(d->fx, d, sys);
	}
	return cDebris_Update(d, sys);
}

/* the debris back into the pool (or freed over maxIdle) */
void cDebrisSys_Release(cDebrisSys *sys, cDebris *d) {
	if (sys->idleCount >= sys->maxIdle) {
		Debris_Free(d);
		return;
	}
	d->next = sys->idle;
	sys->idleCount++;
	sys->idle = d;
	CGameModel_Disable(Debris_Mod(d));
	if (d->fx) {
		d->fx->vftable->Reset(d->fx, d);
	}
}

void cDebrisSys_SetName(cDebrisSys *sys, const char *name) {
	CHECK(!sys->name);
	cString_Set(&sys->name, name);
}

void cDebrisSys_Update(cDebrisSys *sys, float dt) {
	cDebris *d;

	sys->time = dt + sys->time;
	if (sys->time > DEBRIS_MAXSTEP) {
		sys->time = DEBRIS_MAXSTEP;
	}
	if (sys->time > DEBRIS_STEP) {
		cDebris **pp = &sys->active;

		sys->time = sys->time - DEBRIS_STEP;
		while (*pp) {
			if (cDebris_IsAlive(*pp, sys)) {
				pp = &(*pp)->next;
			} else {
				d = *pp;
				*pp = d->next;
				cDebrisSys_Release(sys, d);
			}
		}
	}
	for (d = sys->active; d; d = d->next) {
		cDebris_Step(d, sys->time);
	}
}

/* the vtable of cObjElementAbstract */
static const cObjElementFXVtbl cObjElementAbstract_vftable = {
	.Delete = cObjElementAbstract_Delete,
	.Update = (void *)crt_purecall,
	.IsAlive = (void *)crt_purecall,
	.Start = (void *)crt_purecall,
	.Reset = (void *)crt_purecall,
};

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_O_Show, &GVar_O_Show, "O_Show", 0, "Show Object Pool debug info on screen", 0)
