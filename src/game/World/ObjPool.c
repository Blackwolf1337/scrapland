/*
 * World/ObjPool.cpp of the game: the debris systems (cDebrisSys), named pools of debris (cDebris: a model with a
 * move in 0.05 s steps, or the carrier of an effect cObjElementFX), see include/scrap/scrap.h. The list at 0x0 holds
 * the free debris (cDebrisSys_FreeActive frees it), the one at 0x4 the dropped ones (cDebrisSys_FreeIdle).
 */
#include "scrap/console.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxmisc.h"
#include "scrap/missileentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmodel.h"
#include "scrap/world.h"

#define COBJELEMENTABSTRACT_VFTABLE GAME_VFTABLE(cObjElementAbstract_vftable, 0x3cb0e4)
#define DEBRIS_STEP 0.05f
#define DEBRIS_MAXSTEP 0.1f

#define g_OShow GAME_DATA(int, 0x467400)	/* O_Show, the debug text of the pools */

/* the effects of cDebris_SetFX by type: the size and the constructor (thiscall, returns the object) of the class */
typedef cObjElementFX *(__thiscall *ObjElementFXCtor_t)(void *mem);
static const struct {
	int type;
	uint32_t size;
	uint32_t ctor;
} debrisFX[] = {
	{1, 0xb8, 0x1da3a0},		/* cObjElementFXSistPart */
	{2, 0x1c, 0x1da500},		/* cObjElementFXTrail */
	{3, 0x14, 0x1da190},		/* cObjElementFXFadeOutTrail */
	{4, 0xc70, 0x1da460},		/* cObjElementFXSparks */
	{5, 0x14, 0x1da330},		/* cObjElementFXModelFade */
	{6, 0xcc, 0x1da200},		/* cObjElementFXItemFade */
	{7, 0x648, 0x1da290},		/* cObjElementFXItemTake */
	{8, 0xb8, 0x1da400},		/* cObjElementFXSistPartDuDv */
	{30, 0xd4, 0x1ea0d0},		/* cObjElementFXFiredEntity */
	{31, 0x15c, 0x1c97b0},		/* cObjElementFXHook */
	{32, 0xe0, 0x1f9970},		/* cObjElementFXSwarmMissile */
	{33, 0x768, 0x1f99e0},		/* cObjElementFXTesla */
	{34, 0x1e8, 0x1f9690},		/* cObjElementFXATPC */
	{35, 0xd20, 0x1c96b0},		/* cObjElementFXEMI */
	{36, 0x82e4, 0x1ea320},		/* cObjElementFXVehicleEMI */
	{38, 0x2d0, 0x1ea220},		/* cObjElementFXVehicleDamage */
	{39, 0x1720, 0x1ea500},		/* cObjElementFXVehicleSputnik */
	{40, 0xdc, 0x1ea070},		/* cObjElementFXFiredDebris */
	{41, 0x124, 0x1f9900},		/* cObjElementFXLaser */
	{42, 0x1f4, 0x1c9630},		/* cObjElementFXDebrisCloud */
	{43, 0xe4, 0x1f9880},		/* cObjElementFXInfernoMissile */
	{44, 0x24c8, 0x1f97e0},		/* cObjElementFXInfernoExplosion */
	{45, 0x14, 0x1ea2b0},		/* cObjElementFXVehicleDeathTrail */
	{46, 0x14, 0x1f9770},		/* cObjElementFXDevastatorShoot */
	{47, 0x4c0, 0x1ea1a0},		/* cObjElementFXVehicleAccelerator */
	{48, 0xdc, 0x1ea140},		/* cObjElementFXSmokeDebris */
	{49, 0x1fc, 0x1ea3e0},		/* cObjElementFXVehicleFire */
	{50, 0x220, 0x1c9850},		/* cObjElementFXSonicExplosion */
	{51, 0x14, 0x1da370},		/* cObjElementFXModelNoise */
	{60, 0xe0, 0x1a8300},		/* cObjElementFXSputnik */
	{61, 0xf68, 0x1a8080},		/* cObjElementFXCharacterDeath */
	{62, 0x298, 0x1a7fd0},		/* cObjElementFXCharacterConversor */
	{63, 0x89c, 0x1a7f20},		/* cObjElementFXCharacterConversion */
	{64, 0xd8, 0x1a8120},		/* cObjElementFXCharacterTeleport */
	{71, 0x6bc, 0x1b5ac0},		/* cObjElementFXGearMissile */
	{72, 0xd0, 0x1b5450},		/* cObjElementFXBankerSteal */
	{73, 0xcc, 0x1b5df0},		/* cObjElementFXPoliceSteal */
	{74, 0x1ac, 0x1b5a20},		/* cObjElementFXFunctionarySpecialAction */
	{75, 0xe0, 0x1a81b0},		/* cObjElementFXMaintenanceWork */
	{76, 0xa54, 0x1b5d30},		/* cObjElementFXPoliceGearConversion */
	{77, 0x918, 0x1b5980},		/* cObjElementFXDTritusKill */
	{78, 0x1158, 0x1b58e0},		/* cObjElementFXDTritusAttack */
	{79, 0x180, 0x1b5850},		/* cObjElementFXBishopSellLife */
	{80, 0xfc, 0x1b57a0},		/* cObjElementFXBishopAttack */
	{81, 0xcc, 0x1b54f0},		/* cObjElementFXBertoAttack */
	{82, 0x678, 0x1b5570},		/* cObjElementFXBertoFlash */
	{83, 0xcc, 0x1a8270},		/* cObjElementFXMoneyTransfer */
	{84, 0x3ec, 0x1b5c10},		/* cObjElementFXMessengerAttack */
	{85, 0x174, 0x1b5ca0},		/* cObjElementFXMessengerTakeDataPack */
	{86, 0x7e0, 0x1b5e80},		/* cObjElementFXSebastianFlash */
	{87, 0x1a8, 0x1b5610},		/* cObjElementFXBettyAttack */
	{88, 0x918, 0x1b5700},		/* cObjElementFXBettyKill */
	{89, 0xd4, 0x1b5320},		/* cObjElementFXBankMasterAttack */
	{90, 0x1ec, 0x1b53c0},		/* cObjElementFXBankMasterSuction */
	{91, 0xd4, 0x1b5b90},		/* cObjElementFXMercenaryDrink */
	{100, 0x11ec, 0x1de450},	/* cObjElementFXSceneDecontamination */
	{101, 0x1b8, 0x1de390},		/* cObjElementFXSceneBishops */
	{102, 0xec, 0x1de2f0},		/* cObjElementFXSceneBillArrives_A */
	{103, 0xd4, 0x1de6c0},		/* cObjElementFXSceneYakuzzi */
	{104, 0x240, 0x1de230},		/* cObjElementFXSceneBeatPickus */
	{105, 0x184, 0x1de5a0},		/* cObjElementFXSceneHumanCD */
	{106, 0x798, 0x1de500},		/* cObjElementFXSceneEnterMatrix */
	{107, 0xcc, 0x1de630},		/* cObjElementFXSceneTheEndB */
};

/* the effect of type for the debris (the old one deleted) and its Start; 0 without one */
byte cDebris_SetFX(int type, cDebris *d) {
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

			d->fx = mem ? ((ObjElementFXCtor_t)Game_Ptr(debrisFX[i].ctor))(mem) : NULL;
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

cDebrisSys *__thiscall cDebrisSys_ctor(void *mem) {
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

void __thiscall cObjElement_dtor(void *self) {
	((cObjElementFX *)self)->vftable = COBJELEMENTABSTRACT_VFTABLE;
}

void __thiscall cDebrisSys_dtor(cDebrisSys *sys) {
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
void __thiscall cDebris_Step(cDebris *d, float dt) {
	if (d->fx) {
		d->fx->vftable->Update(d->fx, d, dt);
		return;
	}
	Debris_Move(d, dt);
}

void __thiscall cDebrisSys_Drop(cDebrisSys *sys, cDebris *d) {
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
void __thiscall cDebrisSys_FreeActive(cDebrisSys *sys) {
	while (sys->idle) {
		cDebris *d = sys->idle;

		sys->idle = d->next;
		Debris_Free(d);
	}
	sys->idleCount = 0;
}

/* the list at 0x4 freed */
void __thiscall cDebrisSys_FreeIdle(cDebrisSys *sys) {
	while (sys->active) {
		cDebris *d = sys->active;

		sys->active = d->next;
		Debris_Free(d);
	}
}

cDebris *__thiscall cDebrisSys_NewElement(cDebrisSys *sys) {
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
	CGameModel_SetPivotNodo(Debris_Mod(d), "DC_Root");
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
	d->speed = 0.0f;
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
cDebris *__thiscall cDebrisSys_GetIdle(cDebrisSys *sys) {
	if (sys->active) {
		return sys->active;
	}
	return cDebrisSys_NewElement(sys);
}

/* n debris of the effect type made and put back into the pool (the ones over maxIdle freed) */
void __thiscall cDebrisSys_Init(cDebrisSys *sys, int n, int type) {
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

byte __thiscall cDebrisSys_IsIdle(cDebrisSys *sys, cDebris *d) {
	return d == sys->active;
}

void __thiscall cDebris_Move(cDebris *d, float dt) {
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
byte __thiscall cDebris_Update(cDebris *d, cDebrisSys *sys) {
	vec3_t from, to;
	vec3_t *pos = &d->drop[0], *vel = &d->drop[2];
	CColisTri tri;
	float k;

	if (sys->radiusSq != 0.0f && cWorld_GetMinCamDistSq(g_pWorld, pos) >= sys->radiusSq) {
		return 0;
	}
	d->speed = d->speed - DEBRIS_STEP;
	if (0.0f >= d->speed) {
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

void __thiscall cDebrisSys_ShowInfo(cDebrisSys *sys, int x, int y, float *line) {
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
			(double)d->drop[0].y, (double)d->drop[0].z, (double)p->x, (double)my, (double)z, (double)d->speed,
			fx);
		d = d->next;
	} while (d);
	if (line) {
		*line = fy + 20.0f;
	}
}

/* the step of the debris: by its effect or the own move */
byte __thiscall cDebris_IsAlive(cDebris *d, cDebrisSys *sys) {
	if (d->fx) {
		return d->fx->vftable->IsAlive(d->fx, d, sys);
	}
	return cDebris_Update(d, sys);
}

/* the debris back into the pool (or freed over maxIdle) */
void __thiscall cDebrisSys_Release(cDebrisSys *sys, cDebris *d) {
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

/* the free debris freed, at most n kept from now */
void __thiscall cDebrisSys_SetMaxIdle(cDebrisSys *sys, int n) {
	cDebrisSys_FreeActive(sys);
	sys->maxIdle = n;
}

void __thiscall cDebrisSys_SetName(cDebrisSys *sys, const char *name) {
	CHECK(!sys->name);
	cString_Set(&sys->name, name);
}

void __thiscall cDebrisSys_Update(cDebrisSys *sys, float dt) {
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
