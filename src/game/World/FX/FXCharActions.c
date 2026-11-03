/*
 * World/FX/FXCharActions.cpp of the game: the effects of the special actions of the characters, see
 * include/scrap/fxcharactions.h.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/chartype.h"
#include "scrap/crt.h"
#include "scrap/doors.h"
#include "scrap/fxcharactions.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxweapons.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/missileentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/sound.h"
#include "scrap/weapons.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

#define VT_FXBANKMASTERATTACK (&cObjElementFXBankMasterAttack_vftable)
#define VT_FXBANKMASTERSUCTION (&cObjElementFXBankMasterSuction_vftable)
#define VT_FXBANKERSTEAL (&cObjElementFXBankerSteal_vftable)
#define VT_FXBERTOATTACK (&cObjElementFXBertoAttack_vftable)
#define VT_FXBERTOFLASH (&cObjElementFXBertoFlash_vftable)
#define VT_FXBETTYATTACK (&cObjElementFXBettyAttack_vftable)
#define VT_FXBETTYKILL (&cObjElementFXBettyKill_vftable)
#define VT_FXBISHOPATTACK (&cObjElementFXBishopAttack_vftable)
#define VT_FXBISHOPSELLLIFE (&cObjElementFXBishopSellLife_vftable)
#define VT_FXDTRITUSATTACK (&cObjElementFXDTritusAttack_vftable)
#define VT_FXDTRITUSKILL (&cObjElementFXDTritusKill_vftable)
#define VT_FXFUNCTIONARYSPECIALACTION (&cObjElementFXFunctionarySpecialAction_vftable)
#define VT_FXGEARMISSILE (&cObjElementFXGearMissile_vftable)
#define VT_FXMERCENARYDRINK (&cObjElementFXMercenaryDrink_vftable)
#define VT_FXMESSENGERATTACK (&cObjElementFXMessengerAttack_vftable)
#define VT_FXMESSENGERTAKEDATAPACK (&cObjElementFXMessengerTakeDataPack_vftable)
#define VT_FXPOLICEGEARCONVERSION (&cObjElementFXPoliceGearConversion_vftable)
#define VT_FXPOLICESTEAL (&cObjElementFXPoliceSteal_vftable)
#define VT_FXSEBASTIANFLASH (&cObjElementFXSebastianFlash_vftable)
#define g_BankerSteal1Sound (GVar_BankerSteal1Sound.value.s)
#define g_BankerSteal2Sound (GVar_BankerSteal2Sound.value.s)
#define g_BankerSteal3Sound (GVar_BankerSteal3Sound.value.s)
#define g_BankerStealSoundVol (GVar_BankerStealSoundVol.value.f)
#define g_BankerStealPosX (GVar_BankerStealPosX.value.f)
#define g_BankerStealPosY (GVar_BankerStealPosY.value.f)
#define g_BankerStealPosZ (GVar_BankerStealPosZ.value.f)
#define g_BankMasterBatonPosX (GVar_BankMasterBatonPosX.value.f)
#define g_BankMasterBatonPosY (GVar_BankMasterBatonPosY.value.f)
#define g_BankMasterBatonPosZ (GVar_BankMasterBatonPosZ.value.f)
static const cObjElementFXVtbl cObjElementFXSebastianFlash_vftable;
static const cObjElementFXVtbl cObjElementFXPoliceSteal_vftable;
static const cObjElementFXVtbl cObjElementFXPoliceGearConversion_vftable;
static const cObjElementFXVtbl cObjElementFXMessengerTakeDataPack_vftable;
static const cObjElementFXVtbl cObjElementFXMessengerAttack_vftable;
static const cObjElementFXVtbl cObjElementFXMercenaryDrink_vftable;
static const cObjElementFXVtbl cObjElementFXGearMissile_vftable;
static const cObjElementFXVtbl cObjElementFXFunctionarySpecialAction_vftable;
static const cObjElementFXVtbl cObjElementFXDTritusKill_vftable;
static const cObjElementFXVtbl cObjElementFXDTritusAttack_vftable;
static const cObjElementFXVtbl cObjElementFXBishopSellLife_vftable;
static const cObjElementFXVtbl cObjElementFXBishopAttack_vftable;
static const cObjElementFXVtbl cObjElementFXBettyKill_vftable;
static const cObjElementFXVtbl cObjElementFXBettyAttack_vftable;
static const cObjElementFXVtbl cObjElementFXBertoFlash_vftable;
static const cObjElementFXVtbl cObjElementFXBertoAttack_vftable;
static const cObjElementFXVtbl cObjElementFXBankerSteal_vftable;
static const cObjElementFXVtbl cObjElementFXBankMasterSuction_vftable;
static const cObjElementFXVtbl cObjElementFXBankMasterAttack_vftable;
static cGVar GVar_BankerStealSoundVol;
static cGVar GVar_BankerStealPosZ;
static cGVar GVar_BankerStealPosY;
static cGVar GVar_BankerStealPosX;
static cGVar GVar_BankerSteal3Sound;
static cGVar GVar_BankerSteal2Sound;
static cGVar GVar_BankerSteal1Sound;
static cGVar GVar_BankMasterBatonPosZ;
static cGVar GVar_BankMasterBatonPosY;
static cGVar GVar_BankMasterBatonPosX;

/* the trail array members: CGameTrail by new[] style construction (rva) */
#define CGAMETRAIL_CTOR 0x21ddb0
#define CGAMETRAIL_DTOR 0x21de40
/* CGameModel as array member: its default constructor (CGameModel_ctor with NULL, rva 0x105e60) and CGameModel_Reset */
#define CGAMEMODEL_CTOR0 0x105e60
#define CGAMEMODEL_RESET 0x20ec10

/* rand() & 0x7fff as 0..1 (double) */
static inline double FXA_Rnd(void) {
	return (double)(crt_rand() & 0x7fff) * (1.0 / 32767.0);
}

/* rand in [0, 1] times range, plus add (all in double) */
static inline float FXA_Rand(double range, double add) {
	return (float)(FXA_Rnd() * range + add);
}

/* the same helpers as the constructors of FXMisc.c (kept local, FXMisc.c has them static) */
static inline void FXA_TrailsCtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_ctor(t, sizeof(CGameTrail), n, (void *)CGameTrail_ctor, (void *)CGameTrail_dtor);
}

static inline void FXA_TrailsDtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_dtor(t, sizeof(CGameTrail), n, (void *)CGameTrail_dtor);
}

static inline void *FXA_Delete(void *self, uint32_t flags, uint32_t size) {
	if (flags & 1) {
		operator_delete_sized(self, size);
	}
	return self;
}

/* the model of the entity, CHECKed */
static CGameModel *FXA_Model(cEntity *e) {
	CGameModel *m = Obj_Vtbl(e)->GetModel(e);

	CHECK(CGameModel_IsLoaded(m));
	return m;
}

/* the particles to the node name of the model of the debris, the node CHECKed */
static void FXA_AttachNode(CGameParticles *sp, CGameModel *m, const char *name) {
	int i = CGameModel_FindRenderNode(m, name);

	CHECK(i > -1);
	CGameParticles_Attach(sp, m, i);
}

/* the start of most Attaches: the particles on Dummy01 of the model of the debris */
static void FXA_AttachDummy01(CGameParticles *sp, cDebris *d) {
	CGameModel *m = Debris_Model(d);

	CHECK(CGameModel_IsLoaded(m));
	FXA_AttachNode(sp, m, "Dummy01");
}

/* n trails: segments, texture, type */
static void FXA_Trails(CGameTrail *t, int n, byte segments, int texture, byte type) {
	for (int i = 0; i < n; i++) {
		CGameTrail_SetMaxTrails(&t[i], segments);
		CGameTrail_SetTexture(&t[i], texture);
		CGameTrail_SetType(&t[i], type);
	}
}

static void FXA_ClearTrails(CGameTrail *t, int n) {
	for (int i = 0; i < n; i++) {
		CGameTrail_Clear(&t[i]);
	}
}

/* to - from normalized (as it is when its length is 0) */
static inline vec3_t FXA_Dir(const vec3_t *from, const vec3_t *to) {
	return FX_Unit((vec3_t){ to->x - from->x, to->y - from->y, to->z - from->z });
}

/* a random direction (FX_RandomDir) scaled by r */
static void FXA_RandomDirScaled(vec3_t *out, float r) {
	vec3_t tmp;
	const vec3_t *p = FX_RandomDir(&tmp);

	out->x = p->x * r;
	out->y = p->y * r;
	out->z = p->z * r;
}

/* ---- constructors ---- */

void *cObjElementFXBankMasterAttack_ctor(void *self) {
	cObjElementFXBankMasterAttack *s = self;

	FXObj_SetVtbl(s, VT_FXBANKMASTERATTACK);
	CGameParticles_ctor(&s->particles);
	CGameTrail_ctor(&s->trail);
	CGameTrail_ctor(&s->trail2);
	s->started = 0;
	s->ent = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	return s;
}

void *cObjElementFXBankMasterSuction_ctor(void *self) {
	cObjElementFXBankMasterSuction *s = self;

	FXObj_SetVtbl(s, VT_FXBANKMASTERSUCTION);
	CGameParticles_ctor(&s->particles);
	CGameTrail_ctor(&s->trail);
	s->started = 0;
	s->ent = NULL;
	s->target = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	return s;
}

void *cObjElementFXBankerSteal_ctor(void *self) {
	cObjElementFXBankerSteal *s = self;

	FXObj_SetVtbl(s, VT_FXBANKERSTEAL);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->ent = NULL;
	s->target = NULL;
	s->time = 0.0f;
	s->toggle = 0;
	s->lastSound = -1;
	s->node = -1;
	return s;
}

void *cObjElementFXBertoAttack_ctor(void *self) {
	cObjElementFXBertoAttack *s = self;

	FXObj_SetVtbl(s, VT_FXBERTOATTACK);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->ent = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	return s;
}

void *cObjElementFXBertoFlash_ctor(void *self) {
	cObjElementFXBertoFlash *s = self;

	FXObj_SetVtbl(s, VT_FXBERTOFLASH);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXBERTOFLASH_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->target = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	return s;
}

void *cObjElementFXBettyAttack_ctor(void *self) {
	cObjElementFXBettyAttack *s = self;

	FXObj_SetVtbl(s, VT_FXBETTYATTACK);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->particles2);
	FXA_TrailsCtor(s->trails, 2);
	FXA_TrailsCtor(s->trails2, FXBETTYATTACK_TRAILS);
	CGameTrail_ctor(&s->trail);
	CGameLensFlare_ctor(&s->flare);
	s->started = 0;
	s->ent = NULL;
	s->time = 0.0f;
	s->t2 = 0.0f;
	s->lastTime = 0.0f;
	return s;
}

static void FXKill_Ctor(cObjElementFXKill *s, const cObjElementFXVtbl *vt) {
	FXObj_SetVtbl(s, vt);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXKILL_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
}

cObjElementFXBettyKill *cObjElementFXBettyKill_ctor(cObjElementFXBettyKill *self) {
	FXKill_Ctor(self, VT_FXBETTYKILL);
	return self;
}

void *cObjElementFXBishopAttack_ctor(void *self) {
	cObjElementFXBishopAttack *s = self;

	FXObj_SetVtbl(s, VT_FXBISHOPATTACK);
	CGameParticles_ctor(&s->particles);
	for (int i = 0; i < FXBISHOPATTACK_TRAILS; i++) {
		CGameTrail_ctor(&s->trails[i]);
	}
	s->started = 0;
	s->ent = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	return s;
}

void *cObjElementFXBishopSellLife_ctor(void *self) {
	cObjElementFXBishopSellLife *s = self;

	FXObj_SetVtbl(s, VT_FXBISHOPSELLLIFE);
	CGameParticles_ctor(&s->glow);
	CGameParticles_ctor(&s->dataCodec);
	s->started = 0;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	s->attacker = NULL;
	s->attacked = NULL;
	return s;
}

void *cObjElementFXDTritusAttack_ctor(void *self) {
	cObjElementFXDTritusAttack *s = self;

	FXObj_SetVtbl(s, VT_FXDTRITUSATTACK);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXDTRITUSATTACK_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	return s;
}

cObjElementFXDTritusKill *cObjElementFXDTritusKill_ctor(cObjElementFXDTritusKill *self) {
	FXKill_Ctor(self, VT_FXDTRITUSKILL);
	return self;
}

void *cObjElementFXFunctionarySpecialAction_ctor(void *self) {
	cObjElementFXFunctionarySpecialAction *s = self;

	FXObj_SetVtbl(s, VT_FXFUNCTIONARYSPECIALACTION);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXFUNCTIONARY_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	return s;
}

void *cObjElementFXGearMissile_ctor(void *self) {
	cObjElementFXGearMissile *s = self;

	FXObj_SetVtbl(s, VT_FXGEARMISSILE);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->sparks);
	CGameTrail_ctor(&s->trail);
	FXA_TrailsCtor(s->trails, FXGEARMISSILE_TRAILS);
	s->started = 0;
	s->entity = NULL;
	s->emitTime = 0.0f;
	s->fadeLife = 0.5f;
	s->time = 0.0f;
	s->trailStep = 0.0f;
	return s;
}

void *cObjElementFXMercenaryDrink_ctor(void *self) {
	cObjElementFXMercenaryDrink *s = self;

	FXObj_SetVtbl(s, VT_FXMERCENARYDRINK);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->ent = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	return s;
}

void *cObjElementFXMessengerAttack_ctor(void *self) {
	cObjElementFXMessengerAttack *s = self;

	FXObj_SetVtbl(s, VT_FXMESSENGERATTACK);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXMESSENGERATTACK_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	return s;
}

void *cObjElementFXMessengerTakeDataPack_ctor(void *self) {
	cObjElementFXMessengerTakeDataPack *s = self;

	FXObj_SetVtbl(s, VT_FXMESSENGERTAKEDATAPACK);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXDATAPACK_TRAILS);
	s->started = 0;
	s->owner = NULL;
	s->time = 0.0f;
	return s;
}

void *cObjElementFXPoliceGearConversion_ctor(void *self) {
	cObjElementFXPoliceGearConversion *s = self;

	FXObj_SetVtbl(s, VT_FXPOLICEGEARCONVERSION);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->particles2);
	FXA_TrailsCtor(s->trails, FXGEAR_TRAILS);
	crt_eh_vector_ctor(s->armor, sizeof(CGameModel), FXGEAR_ARMORS, (void *)CGameModel_ctorDefault,
		(void *)CGameModel_Reset);
	s->started = 0;
	s->ent = NULL;
	s->time = 0.0f;
	s->t2 = 0.0f;
	return s;
}

void *cObjElementFXPoliceSteal_ctor(void *self) {
	cObjElementFXPoliceSteal *s = self;

	FXObj_SetVtbl(s, VT_FXPOLICESTEAL);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->ent = NULL;
	s->target = NULL;
	s->emitTime = 0.0f;
	s->unused_c0 = 0;
	s->unused_c4 = -1;
	return s;
}

void *cObjElementFXSebastianFlash_ctor(void *self) {
	cObjElementFXSebastianFlash *s = self;

	FXObj_SetVtbl(s, VT_FXSEBASTIANFLASH);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXSEBASTIAN_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->time = 0.0f;
	s->emitTime = 0.0f;
	return s;
}

/* ---- destructors ---- */

static void cObjElementFXBankMasterAttack_dtor(void *self) {
	cObjElementFXBankMasterAttack *s = self;

	FXObj_SetVtbl(s, VT_FXBANKMASTERATTACK);
	CGameTrail_dtor(&s->trail2);
	CGameTrail_dtor(&s->trail);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXBankMasterSuction_dtor(void *self) {
	cObjElementFXBankMasterSuction *s = self;

	FXObj_SetVtbl(s, VT_FXBANKMASTERSUCTION);
	CGameTrail_dtor(&s->trail);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

/* the classes with only their particles */
static void FXA_DtorParts(void *self, const cObjElementFXVtbl *vt, CGameParticles *parts) {
	FXObj_SetVtbl(self, vt);
	CGameParticles_Release(parts);
	cObjElement_dtor(self);
}

static void cObjElementFXBankerSteal_dtor(void *self) {
	FXA_DtorParts(self, VT_FXBANKERSTEAL, &((cObjElementFXBankerSteal *)self)->particles);
}

static void cObjElementFXBertoAttack_dtor(void *self) {
	FXA_DtorParts(self, VT_FXBERTOATTACK, &((cObjElementFXBertoAttack *)self)->particles);
}

/* the classes with particles and n trails */
static void FXA_DtorTrails(void *self, const cObjElementFXVtbl *vt, CGameParticles *parts, CGameTrail *trails,
	uint32_t n) {
	FXObj_SetVtbl(self, vt);
	FXA_TrailsDtor(trails, n);
	CGameParticles_Release(parts);
	cObjElement_dtor(self);
}

static void cObjElementFXBertoFlash_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXBERTOFLASH, &((cObjElementFXBertoFlash *)self)->particles, ((cObjElementFXBertoFlash *)self)->trails, FXBERTOFLASH_TRAILS);
}

static void cObjElementFXBettyAttack_dtor(void *self) {
	cObjElementFXBettyAttack *s = self;

	FXObj_SetVtbl(s, VT_FXBETTYATTACK);
	CGameLensFlare_dtor(&s->flare);
	CGameTrail_dtor(&s->trail);
	FXA_TrailsDtor(s->trails2, FXBETTYATTACK_TRAILS);
	FXA_TrailsDtor(s->trails, 2);
	CGameParticles_Release(&s->particles2);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXBettyKill_dtor(cObjElementFXBettyKill *self) {
	FXA_DtorTrails(self, VT_FXBETTYKILL, &((cObjElementFXKill *)self)->particles, ((cObjElementFXKill *)self)->trails, FXKILL_TRAILS);
}

static void cObjElementFXBishopAttack_dtor(void *self) {
	cObjElementFXBishopAttack *s = self;

	FXObj_SetVtbl(s, VT_FXBISHOPATTACK);
	for (int i = FXBISHOPATTACK_TRAILS - 1; i >= 0; i--) {
		CGameTrail_dtor(&s->trails[i]);
	}
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXBishopSellLife_dtor(void *self) {
	cObjElementFXBishopSellLife *s = self;

	FXObj_SetVtbl(s, VT_FXBISHOPSELLLIFE);
	CGameParticles_Release(&s->dataCodec);
	CGameParticles_Release(&s->glow);
	cObjElement_dtor(s);
}

static void cObjElementFXDTritusAttack_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXDTRITUSATTACK, &((cObjElementFXDTritusAttack *)self)->particles, ((cObjElementFXDTritusAttack *)self)->trails, FXDTRITUSATTACK_TRAILS);
}

static void cObjElementFXDTritusKill_dtor(cObjElementFXDTritusKill *self) {
	FXA_DtorTrails(self, VT_FXDTRITUSKILL, &((cObjElementFXKill *)self)->particles, ((cObjElementFXKill *)self)->trails, FXKILL_TRAILS);
}

static void cObjElementFXFunctionarySpecialAction_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXFUNCTIONARYSPECIALACTION, &((cObjElementFXFunctionarySpecialAction *)self)->particles, ((cObjElementFXFunctionarySpecialAction *)self)->trails, FXFUNCTIONARY_TRAILS);
}

static void cObjElementFXGearMissile_dtor(void *self) {
	cObjElementFXGearMissile *s = self;

	FXObj_SetVtbl(s, VT_FXGEARMISSILE);
	FXA_TrailsDtor(s->trails, FXGEARMISSILE_TRAILS);
	CGameTrail_dtor(&s->trail);
	CGameParticles_Release(&s->sparks);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXMercenaryDrink_dtor(void *self) {
	FXA_DtorParts(self, VT_FXMERCENARYDRINK, &((cObjElementFXMercenaryDrink *)self)->particles);
}

static void cObjElementFXMessengerAttack_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXMESSENGERATTACK, &((cObjElementFXMessengerAttack *)self)->particles, ((cObjElementFXMessengerAttack *)self)->trails, FXMESSENGERATTACK_TRAILS);
}

static void cObjElementFXMessengerTakeDataPack_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXMESSENGERTAKEDATAPACK, &((cObjElementFXMessengerTakeDataPack *)self)->particles, ((cObjElementFXMessengerTakeDataPack *)self)->trails, FXDATAPACK_TRAILS);
}

static void cObjElementFXPoliceGearConversion_dtor(void *self) {
	cObjElementFXPoliceGearConversion *s = self;

	FXObj_SetVtbl(s, VT_FXPOLICEGEARCONVERSION);
	crt_eh_vector_dtor(s->armor, sizeof(CGameModel), FXGEAR_ARMORS, (void *)CGameModel_Reset);
	FXA_TrailsDtor(s->trails, FXGEAR_TRAILS);
	CGameParticles_Release(&s->particles2);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

static void cObjElementFXPoliceSteal_dtor(void *self) {
	FXA_DtorParts(self, VT_FXPOLICESTEAL, &((cObjElementFXPoliceSteal *)self)->particles);
}

static void cObjElementFXSebastianFlash_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXSEBASTIANFLASH, &((cObjElementFXSebastianFlash *)self)->particles, ((cObjElementFXSebastianFlash *)self)->trails, FXSEBASTIAN_TRAILS);
}

/* ---- deleting destructors (vf00) ---- */

static cObjElementFX *cObjElementFXBankMasterAttack_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXBankMasterAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBankMasterAttack));
}

static cObjElementFX *cObjElementFXBankMasterSuction_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXBankMasterSuction_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBankMasterSuction));
}

static cObjElementFX *cObjElementFXBankerSteal_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXBankerSteal_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBankerSteal));
}

static cObjElementFX *cObjElementFXBertoAttack_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXBertoAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBertoAttack));
}

static cObjElementFX *cObjElementFXBertoFlash_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXBertoFlash_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBertoFlash));
}

static cObjElementFX *cObjElementFXBettyAttack_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXBettyAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBettyAttack));
}

static cObjElementFX *cObjElementFXBettyKill_Delete(cObjElementFX *base, byte flags) {
	cObjElementFXBettyKill *self = (cObjElementFXBettyKill *)base;
	cObjElementFXBettyKill_dtor(self);
	return FXA_Delete(self, flags, sizeof(*self));
}

static cObjElementFX *cObjElementFXBishopAttack_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXBishopAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBishopAttack));
}

static cObjElementFX *cObjElementFXBishopSellLife_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXBishopSellLife_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBishopSellLife));
}

static cObjElementFX *cObjElementFXDTritusAttack_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXDTritusAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXDTritusAttack));
}

static cObjElementFX *cObjElementFXDTritusKill_Delete(cObjElementFX *base, byte flags) {
	cObjElementFXDTritusKill *self = (cObjElementFXDTritusKill *)base;
	cObjElementFXDTritusKill_dtor(self);
	return FXA_Delete(self, flags, sizeof(*self));
}

static cObjElementFX *cObjElementFXFunctionarySpecialAction_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXFunctionarySpecialAction_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXFunctionarySpecialAction));
}

static cObjElementFX *cObjElementFXGearMissile_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXGearMissile_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXGearMissile));
}

static cObjElementFX *cObjElementFXMercenaryDrink_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXMercenaryDrink_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXMercenaryDrink));
}

static cObjElementFX *cObjElementFXMessengerAttack_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXMessengerAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXMessengerAttack));
}

static cObjElementFX *cObjElementFXMessengerTakeDataPack_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXMessengerTakeDataPack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXMessengerTakeDataPack));
}

static cObjElementFX *cObjElementFXPoliceGearConversion_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXPoliceGearConversion_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXPoliceGearConversion));
}

static cObjElementFX *cObjElementFXPoliceSteal_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXPoliceSteal_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXPoliceSteal));
}

static cObjElementFX *cObjElementFXSebastianFlash_Delete(cObjElementFX *base, byte flags) {
	void *self = (void *)base;
	cObjElementFXSebastianFlash_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXSebastianFlash));
}

/* ---- Alive (vf02) ---- */

/* BankMasterAttack and BertoAttack: the same start (vtable, entity, particles) */
static byte FXCharActions_AliveAction(cObjElementFX *base, cDebris *d, cDebrisSys *sys) {
	void *self = (void *)base;
	cObjElementFXBertoAttack *el = self;
	cEntity *e = el->ent;

	if (e && WithLife_Vtbl(e)->GetActCtrl(e)) {
		e = el->ent;
		if (cActCtrl_IsActive(WithLife_Vtbl(e)->GetActCtrl(e), "Action")) {
			d->life = 1.0f;
		} else {
			e = el->ent;
			if (cActCtrl_IsActive(WithLife_Vtbl(e)->GetActCtrl(e), "ActionInit")) {
				d->life = 1.0f;
			}
		}
	}
	return cDebris_Update(d, sys);
}

/* the effect of the model of the character off */
static void FXA_EffectOff(CGameModel *m) {
	CHECK(CGameModel_IsLoaded(m));
	CGameModel_SetEffect(m, 0);
	CGameModel_SetEffectParam(m, 0, 0);
}

/* Alive (vf02); the 18 byte ones of the other classes are FXMisc_Alive */
static byte cObjElementFXFunctionarySpecialAction_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys) {
	cObjElementFXFunctionarySpecialAction *self = (cObjElementFXFunctionarySpecialAction *)base;
	CGameModel *m;

	if (!cDebris_Update(d, sys)) {
		if (self->ent) {
			m = Obj_Vtbl(self->ent)->GetModel(self->ent);
			if (CGameModel_IsLoaded(m)) {
				FXA_EffectOff(m);
			}
		}
		return 0;
	}
	if (d->life > 0.4f && !self->ent) {
		d->life = 0.4f;
	}
	if (self->ent && cTimer_GetSpeed(&Obj(self->ent)->timer) == 1.0f
		&& cTimer_GetSpeedTo(&Obj(self->ent)->timer) == 1.0f && 0.4f >= d->life) {
		m = Obj_Vtbl(self->ent)->GetModel(self->ent);
		if (!CGameModel_IsLoaded(m)) {
			return 0;
		}
		FXA_EffectOff(m);
	}
	return 1;
}

static byte cObjElementFXPoliceGearConversion_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys) {
	cObjElementFXPoliceGearConversion *self = (cObjElementFXPoliceGearConversion *)base;
	if (cDebris_Update(d, sys)) {
		return 1;
	}
	for (int i = 0; i < FXGEAR_ARMORS; i++) {
		CGameModel_Disable(&self->armor[i]);
	}
	return 0;
}

/* ---- Attach (vf03) ---- */

/* Attach (vf03) */
static void cObjElementFXBankMasterAttack_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXBankMasterAttack *self = (cObjElementFXBankMasterAttack *)base;
	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.ParticleSystem);
	CGameTrail_SetMaxTrails(&self->trail, 20);
	CGameTrail_SetType(&self->trail, 1);
	CGameTrail_SetTexture(&self->trail, 0);
	CGameTrail_SetMaxTrails(&self->trail2, 16);
	CGameTrail_SetType(&self->trail2, 0);
	CGameTrail_SetTexture(&self->trail2, 0);
	self->t = 0.0f;
}

static void cObjElementFXBankMasterSuction_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXBankMasterSuction *self = (cObjElementFXBankMasterSuction *)base;
	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.ParticleSystem);
	self->lastSound = 0;
	self->count = 0;
}

/* BankerSteal and PoliceSteal: the same start (vtable, entity, target, particles) */
static void FXCharActions_AttachC(cObjElementFX *base, cDebris *d) {
	void *self = (void *)base;
	FXA_AttachDummy01(&((cObjElementFXPoliceSteal *)self)->particles, d);
}

/* BertoAttack and MercenaryDrink: the same start (vtable, entity, particles) */
static void FXCharActions_Attach8(cObjElementFX *base, cDebris *d) {
	void *self = (void *)base;
	CGameParticles *sp = &((cObjElementFXBertoAttack *)self)->particles;

	FXA_AttachDummy01(sp, d);
	CHECK(sp->ParticleSystem);
}

static void cObjElementFXBertoFlash_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXBertoFlash *self = (cObjElementFXBertoFlash *)base;
	FXA_AttachDummy01(&self->particles, d);
	FXA_Trails(self->trails, FXBERTOFLASH_TRAILS, 31, 2, 3);
	CHECK(self->particles.ParticleSystem);
}

static void cObjElementFXBettyAttack_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXBettyAttack *self = (cObjElementFXBettyAttack *)base;
	CGameModel *m = Debris_Model(d);

	FXA_AttachDummy01(&self->particles, d);
	int i = CGameModel_FindRenderNode(m, "Dummy02 (+dudv_pass)");
	if (i > -1) {
		CGameParticles_Attach(&self->particles2, m, i);
	}
	FXA_Trails(self->trails, 2, 31, 0, 3);
	FXA_Trails(self->trails2, FXBETTYATTACK_TRAILS, 31, 10, 3);
	CGameTrail_SetMaxTrails(&self->trail, 31);
	CGameTrail_SetType(&self->trail, 1);
	CHECK(self->particles.ParticleSystem);
	self->ring = 0.0f;
	CGameLensFlare_SetType(&self->flare, 2);
}

static void cObjElementFXKill_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXKill *self = (cObjElementFXKill *)base;
	FXA_AttachDummy01(&self->particles, d);
	FXA_Trails(self->trails, FXKILL_TRAILS, 31, 10, 3);
	CHECK(self->particles.ParticleSystem);
}

/* a trail of the Bishop attack: segments, type 3, texture, no fade, premultiplied */
static void FXBishop_Trail(CGameTrail *t, byte segments, int texture, const float *uv) {
	CGameTrail_SetMaxTrails(t, segments);
	CGameTrail_SetType(t, 3);
	CGameTrail_SetTexture(t, texture);
	if (uv) {
		CGameTrail_SetUVScale(t, uv[0], uv[1]);
	}
	CGameTrail_SetFade(t, 0);
	CGameTrail_SetPremul(t, 1);
}

static void cObjElementFXBishopAttack_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXBishopAttack *self = (cObjElementFXBishopAttack *)base;
	static const float uv0[2] = { 4.0f, 1.0f };
	static const float uv1[2] = { 16.0f, 0.5f };

	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.ParticleSystem);
	FXBishop_Trail(&self->trails[0], 3, 4, uv0);
	FXBishop_Trail(&self->trails[1], 31, 14, uv1);
	FXBishop_Trail(&self->trails[2], 31, 4, NULL);
	FXBishop_Trail(&self->trails[3], 31, 4, NULL);
}

static void cObjElementFXBishopSellLife_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXBishopSellLife *self = (cObjElementFXBishopSellLife *)base;
	FXA_AttachDummy01(&self->glow, d);
	CHECK(self->glow.ParticleSystem);
	FXA_AttachNode(&self->dataCodec, Debris_Model(d), "Dummy02");
	CHECK(self->dataCodec.ParticleSystem);
}

static void cObjElementFXDTritusAttack_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXDTritusAttack *self = (cObjElementFXDTritusAttack *)base;
	FXA_AttachDummy01(&self->particles, d);
	FXA_Trails(self->trails, FXDTRITUSATTACK_TRAILS, 31, 10, 3);
	CHECK(self->particles.ParticleSystem);
}

static void cObjElementFXFunctionarySpecialAction_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXFunctionarySpecialAction *self = (cObjElementFXFunctionarySpecialAction *)base;
	FXA_AttachDummy01(&self->particles, d);
	for (int i = 0; i < FXFUNCTIONARY_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 8);
		CGameTrail_SetTexture(t, 2);
		CGameTrail_SetSmooth(t, 1);
		CGameTrail_SetFade(t, 1);
		CGameTrail_SetOrient(t, (vec3_t){ 0.0f, 1.0f, 0.0f }, 2);
	}
	self->alpha = 0.0f;
}

static void cObjElementFXGearMissile_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXGearMissile *self = (cObjElementFXGearMissile *)base;
	CGameModel *m = Debris_Model(d);

	CHECK(CGameModel_IsLoaded(m));
	self->node = CGameModel_FindRenderNode(m, "Dummy01");
	CHECK(self->node > -1);
	CGameParticles_Attach(&self->particles, m, self->node);
	CHECK(self->particles.ParticleSystem);
	self->nodeSparks = CGameModel_FindRenderNode(m, "Dummy02");
	CHECK(self->nodeSparks > -1);
	CGameParticles_Attach(&self->sparks, m, self->nodeSparks);
	CHECK(self->sparks.ParticleSystem);
	for (int i = 0; i < FXGEARMISSILE_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 14);
		CGameTrail_SetSmooth(t, 1);
		CGameTrail_SetColor(t, 0xc0c0ff);
		CGameTrail_SetTexture(t, 9);
		CGameTrail_SetULen(t, 4.0f, 1.0f);
		CGameTrail_SetPremul(t, 1);
		self->boltScale[i] = 0.5f;
		self->boltTime[i] = -1.0f;
	}
	CGameTrail_SetMaxTrails(&self->trail, 10);
	CGameTrail_SetSmooth(&self->trail, 0);
	CGameTrail_SetFade(&self->trail, 1);
	CGameTrail_SetScale(&self->trail, 1.0f, 0.4f);
	CGameTrail_SetTexture(&self->trail, 3);
}

/* the lightning trails of the Messenger: texture 8 (u length 4) for the main ones, else 9 (6) */
static void FXMessenger_Trail(CGameTrail *t, byte segments, byte main) {
	CGameTrail_SetMaxTrails(t, segments);
	CGameTrail_SetPremul(t, 1);
	CGameTrail_SetSmooth(t, 1);
	if (main) {
		CGameTrail_SetTexture(t, 8);
		CGameTrail_SetULen(t, 4.0f, 1.0f);
	} else {
		CGameTrail_SetTexture(t, 9);
		CGameTrail_SetULen(t, 6.0f, 1.0f);
	}
}

static void cObjElementFXMessengerAttack_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXMessengerAttack *self = (cObjElementFXMessengerAttack *)base;
	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.ParticleSystem);
	for (int i = 0; i < FXMESSENGERATTACK_TRAILS; i++) {
		FXMessenger_Trail(&self->trails[i], 31, i == 0);
	}
}

static void cObjElementFXMessengerTakeDataPack_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXMessengerTakeDataPack *self = (cObjElementFXMessengerTakeDataPack *)base;
	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.ParticleSystem);
	for (int i = 0; i < FXDATAPACK_TRAILS; i++) {
		FXMessenger_Trail(&self->trails[i], 12, i & 1);
	}
}

/* the pieces of the Gear armors: armor, node name, the index of its time in pieceTime (as one flat array) and the
 * time */
typedef struct FXGearPiece {
	int armor;
	const char *name;
	uint32_t idx;
	float time;
} FXGearPiece;

static const FXGearPiece fxa_gearPieces[] = {
	{ 0, "OB_PiezasGear_08", 0, 2.9f }, { 0, "OB_PiezasGear_09", 1, 2.9f },
	{ 0, "OB_PiezasGear_10", 2, 2.83f }, { 0, "OB_PiezasGear_11", 3, 3.2f },
	{ 0, "OB_PiezasGear_12", 4, 2.96f }, { 0, "OB_PiezasGear_13", 5, 3.03f },
	{ 0, "OB_PiezasGear_14", 6, 3.16f }, { 0, "OB_PiezasGear_17", 7, 3.1f },
	{ 1, "OB_PiezasGear_15", 15, 3.3f }, { 1, "OB_PiezasGear_16", 16, 3.36f },
	{ 1, "OB_PiezasGear_18", 17, 3.13f }, { 1, "OB_PiezasGear_19", 18, 3.13f },
	{ 1, "OB_PiezasGear_20", 19, 3.26f }, { 1, "OB_PiezasGear_21", 20, 3.33f },
	{ 1, "OB_PiezasGear_22", 21, 3.4f }, { 1, "OB_PiezasGear_23", 22, 3.66f },
	{ 1, "OB_PiezasGear_24", 23, 3.26f }, { 1, "OB_PiezasGear_38", 24, 3.43f },
	{ 1, "OB_PiezasGear_39", 25, 3.33f }, { 1, "OB_PiezasGear_40", 26, 3.26f },
	{ 2, "OB_PiezasGear_01", 31, 3.66f }, { 2, "OB_PiezasGear_25", 32, 3.26f },
	{ 2, "OB_PiezasGear_26", 33, 3.33f }, { 2, "OB_PiezasGear_27", 34, 3.46f },
	{ 2, "OB_PiezasGear_28", 35, 3.46f }, { 2, "OB_PiezasGear_29", 36, 3.53f },
	{ 2, "OB_PiezasGear_30", 37, 3.53f }, { 2, "OB_PiezasGear_31", 38, 3.6f },
	{ 2, "OB_PiezasGear_32", 39, 3.6f }, { 2, "OB_PiezasGear_37", 40, 3.53f },
};
static const int fxa_gearNumPieces[FXGEAR_ARMORS] = { 8, 12, 10 };

static void cObjElementFXPoliceGearConversion_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXPoliceGearConversion *self = (cObjElementFXPoliceGearConversion *)base;
	char buf[256];
	int i, k;

	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.ParticleSystem);
	for (i = 0; i < FXGEAR_ARMORS; i++) {
		CGameModel *m = &self->armor[i];
		vec3_t scale = { 1.0f, 1.0f, 1.0f };

		Str_PrintfQuiet(buf, 0xff, "Models/Chars/Gear/Gear_Armor%d.M3D", i + 1);
		CGameModel_LoadScaled(m, buf, &scale);
		CGameModel_Disable(m);
		CGameModel_SetPivotNode(m, "DC_Root");
		Str_PrintfQuiet(buf, 0xff, "Models/Chars/Gear/Anm/Gear_ArmorFusion%d.M3D", i + 1);
		self->anim[i] = CGameModel_AddAnim(m, buf);
		for (k = 0; k < FXGEAR_PIECES; k++) {
			self->pieceOff[i][k] = 0;
		}
	}
	const FXGearPiece *p = fxa_gearPieces;
	for (i = 0; i < FXGEAR_ARMORS; i++) {
		self->numPieces[i] = fxa_gearNumPieces[i];
		for (k = 0; k < fxa_gearNumPieces[i]; k++, p++) {
			self->piece[i][k] = CGameModel_FindRenderNodeWild(&self->armor[i], p->name);
			(&self->pieceTime[0][0])[p->idx] = p->time;
		}
		for (; k < FXGEAR_PIECES; k++) {
			self->piece[i][k] = -1;
		}
	}
	FXA_Trails(self->trails, FXGEAR_TRAILS, 31, 10, 3);
	self->count = 0;
	self->time = 0.0f;
	self->lastTime = 0.0f;
	self->emitT = 0.0f;
	self->smokeT = 0.0f;
}

static void cObjElementFXSebastianFlash_Attach(cObjElementFX *base, cDebris *d) {
	cObjElementFXSebastianFlash *self = (cObjElementFXSebastianFlash *)base;
	FXA_AttachDummy01(&self->particles, d);
	FXA_Trails(self->trails, FXSEBASTIAN_TRAILS, 31, 2, 3);
	CHECK(self->particles.ParticleSystem);
}

/* ---- Reset (vf04) ---- */

/* Reset (vf04) */
static void cObjElementFXBankMasterAttack_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXBankMasterAttack *self = (cObjElementFXBankMasterAttack *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	CGameTrail_Clear(&self->trail);
	CGameTrail_Clear(&self->trail2);
	self->t = 0.0f;
}

static void cObjElementFXBankMasterSuction_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXBankMasterSuction *self = (cObjElementFXBankMasterSuction *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	self->count = 0;
	self->lastSound = 0;
}

static void cObjElementFXBankerSteal_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXBankerSteal *self = (cObjElementFXBankerSteal *)base;
	(void)arg;
	self->ent = NULL;
	self->target = NULL;
	self->time = 0.0f;
}

static void cObjElementFXBertoAttack_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXBertoAttack *self = (cObjElementFXBertoAttack *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	CGameParticles_KillAll(&self->particles);
}

static void cObjElementFXBertoFlash_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXBertoFlash *self = (cObjElementFXBertoFlash *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	FXA_ClearTrails(self->trails, FXBERTOFLASH_TRAILS);
	CGameParticles_KillAll(&self->particles);
}

static void cObjElementFXBettyAttack_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXBettyAttack *self = (cObjElementFXBettyAttack *)base;
	(void)arg;
	self->time = 0.0f;
	self->t2 = 0.0f;
	self->lastTime = 0.0f;
	FXA_ClearTrails(self->trails, 2);
	FXA_ClearTrails(self->trails2, FXBETTYATTACK_TRAILS);
	CGameTrail_Clear(&self->trail);
	CGameParticles_KillAll(&self->particles);
	if (self->particles2.ParticleSystem) {
		CGameParticles_KillAll(&self->particles2);
	}
	self->ring = 0.0f;
}

static void cObjElementFXKill_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXKill *self = (cObjElementFXKill *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	FXA_ClearTrails(self->trails, FXKILL_TRAILS);
	CGameParticles_KillAll(&self->particles);
}

static void cObjElementFXBishopAttack_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXBishopAttack *self = (cObjElementFXBishopAttack *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	CGameParticles_KillAll(&self->particles);
	FXA_ClearTrails(self->trails, FXBISHOPATTACK_TRAILS);
}

static void cObjElementFXBishopSellLife_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXBishopSellLife *self = (cObjElementFXBishopSellLife *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	self->attacker = NULL;
	self->attacked = NULL;
	CGameParticles_KillAll(&self->glow);
	CGameParticles_KillAll(&self->dataCodec);
}

static void cObjElementFXDTritusAttack_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXDTritusAttack *self = (cObjElementFXDTritusAttack *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	FXA_ClearTrails(self->trails, FXDTRITUSATTACK_TRAILS);
	CGameParticles_KillAll(&self->particles);
}

static void cObjElementFXFunctionarySpecialAction_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXFunctionarySpecialAction *self = (cObjElementFXFunctionarySpecialAction *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	CGameParticles_KillAll(&self->particles);
	FXA_ClearTrails(self->trails, FXFUNCTIONARY_TRAILS);
	self->alpha = 0.0f;
}

static void cObjElementFXGearMissile_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXGearMissile *self = (cObjElementFXGearMissile *)base;
	(void)arg;
	self->entity = NULL;
	self->emitTime = 0.0f;
	self->time = 0.0f;
	for (int i = 0; i < FXGEARMISSILE_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
		self->boltTime[i] = -1.0f;
	}
	self->trailStep = 0.0f;
	CGameTrail_Clear(&self->trail);
	CGameParticles_KillAll(&self->particles);
}

static void cObjElementFXMercenaryDrink_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXMercenaryDrink *self = (cObjElementFXMercenaryDrink *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
}

static void cObjElementFXMessengerAttack_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXMessengerAttack *self = (cObjElementFXMessengerAttack *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	CGameParticles_KillAll(&self->particles);
	FXA_ClearTrails(self->trails, FXMESSENGERATTACK_TRAILS);
}

static void cObjElementFXMessengerTakeDataPack_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXMessengerTakeDataPack *self = (cObjElementFXMessengerTakeDataPack *)base;
	(void)arg;
	self->time = 0.0f;
	CGameParticles_KillAll(&self->particles);
	FXA_ClearTrails(self->trails, FXDATAPACK_TRAILS);
}

static void cObjElementFXPoliceGearConversion_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXPoliceGearConversion *self = (cObjElementFXPoliceGearConversion *)base;
	(void)arg;
	self->time = 0.0f;
	self->lastTime = 0.0f;
	self->t2 = 0.0f;
	/* the 45 piece flags, 0x96c..0x998 (the last 0x2d bytes, the movups of the original) */
	for (int i = 0; i < FXGEAR_ARMORS; i++) {
		for (int k = 0; k < FXGEAR_PIECES; k++) {
			self->pieceOff[i][k] = 0;
		}
	}
	CGameParticles_KillAll(&self->particles);
	FXA_ClearTrails(self->trails, FXGEAR_TRAILS);
	self->count = 0;
	self->emitT = 0.0f;
	self->smokeT = 0.0f;
}

static void cObjElementFXPoliceSteal_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXPoliceSteal *self = (cObjElementFXPoliceSteal *)base;
	(void)arg;
	self->ent = NULL;
	self->target = NULL;
	self->emitTime = 0.0f;
}

static void cObjElementFXSebastianFlash_Reset(cObjElementFX *base, cDebris *arg) {
	cObjElementFXSebastianFlash *self = (cObjElementFXSebastianFlash *)base;
	(void)arg;
	self->time = 0.0f;
	self->emitTime = 0.0f;
	FXA_ClearTrails(self->trails, FXSEBASTIAN_TRAILS);
	CGameParticles_KillAll(&self->particles);
}

/* ---- the setters and starts ---- */

/* the effects that start with the vtable, their entity and a target entity (BankerSteal, PoliceSteal, ...) */
typedef struct FXElemTarget {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
	cEntity *ent;			/* 0x04 */
	cEntity *target;		/* 0x08 */
} FXElemTarget;

void FXCharActions_SetEntity8(void *self, cEntity *e) {
	((FXElemTarget *)self)->target = e;
	FXA_Model(e);
}

void cObjElementFXBankerSteal_SetEntity(cObjElementFXBankerSteal *self, cEntity *e) {
	self->ent = e;
	self->node = CGameModel_FindRenderNode(FXA_Model(e), "DF_BankerSteal");
}

void cObjElementFXBishopSellLife_SetAttacked(cObjElementFXBishopSellLife *self, cEntity *e) {
	self->attacked = e;
	if (e) {
		FXA_Model(e);
	}
}

void cObjElementFXBishopSellLife_SetAttacker(cObjElementFXBishopSellLife *self, cEntity *e) {
	self->attacker = e;
	if (!e) {
		return;
	}
	CGameModel *m = FXA_Model(e);
	self->nodeHand[0] = CGameModel_FindRenderNode(m, "R_Bishop_Mano_Izq");
	CHECK(self->nodeHand[0] > -1);
	self->nodeHand[1] = CGameModel_FindRenderNode(m, "R_Bishop_Mano_Der");
	CHECK(self->nodeHand[1] > -1);
}

void cObjElementFXMessengerTakeDataPack_SetItem(cObjElementFXMessengerTakeDataPack *self, cEntity *e) {
	self->item = e;
	CHECK(e);
	CHECK(self->item->vftable->IsType(self->item, "Obj"));
}

void cObjElementFXMessengerTakeDataPack_SetOwner(cObjElementFXMessengerTakeDataPack *self, cEntity *e) {
	self->owner = e;
	CHECK(e);
	CHECK(self->owner->vftable->IsType(self->owner, TYEN_WALKCHAR));
}

void cObjElementFXMercenaryDrink_SetC0(cObjElementFXMercenaryDrink *self, float a, float b, float c) {
	self->offset.x = a;
	self->offset.y = b;
	self->offset.z = c;
}

void cObjElementFXMercenaryDrink_Start(cObjElementFXMercenaryDrink *self, cEntity *e) {
	self->ent = e;
	self->nodeJar = CGameModel_FindRenderNode(FXA_Model(e), "OB_JarraMercurio_MercA");
}

void cObjElementFXBankMasterAttack_Start(cObjElementFXBankMasterAttack *self, cEntity *e) {
	self->ent = e;
	self->nodeBaton = CGameModel_FindRenderNode(FXA_Model(e), "DF_Batton");
	CHECK(self->nodeBaton >= 0);
}

void cObjElementFXBertoAttack_Start(cObjElementFXBertoAttack *self, cEntity *e) {
	self->ent = e;
	self->nodeGlass = CGameModel_FindRenderNode(FXA_Model(e), "R_Berto_TroncoInf-cristal");
	CHECK(self->nodeGlass > -1);
}

void cObjElementFXKill_SetAngles(cObjElementFXKill *self, float x, float y, float z) {
	self->angles.x = x;
	self->angles.y = y;
	self->angles.z = z;
}

void cObjElementFXBishopAttack_SetLine(cObjElementFXBishopAttack *self, float fx, float fy, float fz,
	float tx, float ty, float tz) {
	self->from = (vec3_t){ fx, fy, fz };
	self->to = (vec3_t){ tx, ty, tz };
	self->dir = FXA_Dir(&self->from, &self->to);
}

void cObjElementFXGearMissile_Run(void *self, cDebris *d) {
	cObjElementFX *fx = self;

	for (int i = 0; i < 8; i++) {
		fx->vftable->Update(fx, d, 0.0f);
	}
}

static cGVar GVar_GearMissileThunder1Sound;

/* ---- cObjElementFXGearMissile_Update ---- */

#define g_GearMissileThunder1Sound (GVar_GearMissileThunder1Sound.value.s)
static cGVar GVar_GearMissileThunder2Sound;
#define g_GearMissileThunder2Sound (GVar_GearMissileThunder2Sound.value.s)
static cGVar GVar_GearMissileThunderSoundVol;
#define g_GearMissileThunderSoundVol (GVar_GearMissileThunderSoundVol.value.f)
#define FXGEAR_RAMP_BALL 0x4306b0
#define FXGEAR_RAMPS 0x4306d4		/* 3 color ramps of 9 colors */
#define FXGEAR_RAMP_PUFF 0x4306f8
#define FXGEAR_RAMP_SPARK 0x430788
#define FXGEAR_ENTITIES 0x11		/* the entity lists the bolts hit */
#define FX_2PI 6.2831854820251465

/* the slot 0x80 of the entity vtable: the angles into out */
static inline vec3_t *GM_EntityRot(cEntity *e, vec3_t *out) {
	return Obj_Vtbl(e)->GetAng(e, out);
}

static uint32_t GM_Color(int a, uint32_t rgb) {
	if (a > 0xff) {
		a = 0xff;
	}
	return (uint32_t)a << 24 | rgb;
}

static uint32_t FXGEAR_PUFF_RAMPS[27] = {
	0xff5b66ff, 0x966462fe, 0x506d5dfd, 0x7812fc, 0x8551fb, 0x40964af9, 0x809b47f9, 0xc49b47f9, 0xff9b47f9,
	0xff86edff, 0xff8ddbff, 0x8095c4ff, 0x9dadff, 0xa598ff, 0x40ae7eff, 0x80b765ff, 0xffb765ff, 0xffb765ff,
	0xffffad2a, 0xc0feee90, 0x80ef6f38, 0x40bc3010, 0x407a1f0a, 0x5c3f1005, 0x800e0401, 0xff000000, 0xff000000,
};

static uint32_t *GM_Ramp3(void) {
	return FXGEAR_PUFF_RAMPS + crt_rand() % 3 * 9;
}

/* the particles still at the node: no position, speed or acceleration */
static void GM_Still(CGameParticles *p) {
	p->birthRate = 0;
	p->initPosMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	p->initPosMax = p->initPosMin;
	p->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	p->initVelMax = p->initVelMin;
	p->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	p->initAccMax = p->initAccMin;
}

/* a random rotation without variation */
static void GM_RandRot(CGameParticles *p) {
	float r = FXA_Rand(FX_2PI, 0.0);
	p->rotVar = 0.0f;
	p->rotIni = r;
	p->rotEnd = r;
}

static void GM_NoRot(CGameParticles *p) {
	p->rotIni = 0.0f;
	p->rotEnd = 0.0f;
	p->rotVar = 0.0f;
}

/* a still puff of one of the 3 ramps */
static void GM_Puff(CGameParticles *p, double lifeRange, double lifeAdd, double iniRange, double iniAdd, byte sprIni,
	byte sprEnd, double endRange, double endAdd) {
	GM_Still(p);
	GM_RandRot(p);
	p->sizeLife = 1.0f;
	p->lifeTime = FXA_Rand(lifeRange, lifeAdd);
	p->sizeIni = FXA_Rand(iniRange, iniAdd);
	p->spriteIni = sprIni;
	p->spriteEnd = sprEnd;
	p->sizeEnd = FXA_Rand(endRange, endAdd);
	CGameParticles_Apply(p);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, GM_Ramp3());
	CGameParticles_EmitN(p, 1, 0);
}

/* the running bolt i from pos: the points shaken by the length, the trail along their curve, the glows at the end;
 * out when its time is over */
static void GM_Bolt(cObjElementFXGearMissile *self, int i, const vec3_t *pos, float t) {
	CGameTrail *trail = &self->trails[i];
	vec3_t *pts = self->pts[i];
	vec3_t coefs[FXGEARMISSILE_SLOTS], pt, tmp;
	const int last = FXGEARMISSILE_POINTS - 1;

	float bt = self->boltTime[i] - t;
	self->boltTime[i] = bt;
	if (!(bt > 0.0f)) {
		CGameParticles_KillRange(&self->sparks, i * 3, i * 3 + 3);
		CGameTrail_Clear(trail);
		return;
	}
	int alpha = 0;
	if (0.05f > bt) {
		alpha = (int)((1.0f - (bt - t) / 0.05f) * 255.0f);
		if (alpha < 0) {
			alpha = 0;
		} else if (alpha > 0xff) {
			alpha = 0xff;
		}
	}
	CGameTrail_SetTransp(trail, (byte)alpha);
	pts[0] = *pos;
	float dy = pos->y - pts[last].y;
	float dx = pos->x - pts[last].x;
	float dz = pos->z - pts[last].z;
	float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	for (int k = 1; k < last; k++) {
		float a = FXA_Rand(1.0, 1.0);
		const vec3_t *d = FX_RandomDir(&tmp);
		float s = a * len * t;
		pts[k].x = s * d->x + pts[k].x;
		pts[k].y = pts[k].y + d->y * s;
		pts[k].z = pts[k].z + d->z * s;
	}
	int maxT = CGameTrail_GetMaxTrails(trail);
	CGameTrail_BezierCoefs(trail, FXGEARMISSILE_POINTS, pts, coefs);
	if (maxT - 1 > 0) {
		float den = (float)(maxT - 2);
		for (int k = 0; k < maxT - 1; k++) {
			CGameTrail_BezierPoint(trail, FXGEARMISSILE_POINTS, coefs, &pt, (float)k / den);
			float w;
			if (k > 0 && k < maxT - 2) {
				float jit = FXA_Rand(1.0, 6.0);
				const vec3_t *d = FX_RandomDir(&tmp);
				pt.x = pt.x + jit * d->x;
				pt.y = pt.y + d->y * jit;
				pt.z = pt.z + d->z * jit;
				w = FXA_Rand(40.0, 35.0);
			} else {
				w = FXA_Rand(10.0, 115.0);
			}
			float g = FXA_Rand(4.0, 22.0);
			float r = FXA_Rand(9.0, 55.0);
			float b = FXA_Rand(9.0, 55.0);
			uint32_t color = ((((uint32_t)(int)g << 8 | (uint32_t)(int)r) << 8 | (uint32_t)(int)b) << 8) | 0xff;
			CGameTrail_Add(trail, pt, self->boltScale[i] * w, color);
		}
	}
	const vec3_t *e = &pts[last];
	CGameParticles *sp = &self->sparks;
	uint32_t c = GM_Color(alpha + 4, 0xc0c0ff);
	CGameParticles_SetParticle(sp, i * 3, e->x, e->y, e->z, FXA_Rand(10.0, 75.0) * self->boltScale[i], c, 1, 0.0f);
	c = GM_Color(alpha + 0x18, 0x3030c4);
	CGameParticles_SetParticle(sp, i * 3 + 1, e->x, e->y, e->z, FXA_Rand(40.0, 230.0) * self->boltScale[i], c, 2,
		0.0f);
	c = GM_Color(alpha + 0xa4, 0x1010ff);
	CGameParticles_SetParticle(sp, i * 3 + 2, e->x, e->y, e->z, FXA_Rand(100.0, 650.0) * self->boltScale[i], c, 5,
		0.0f);
}

/* a new bolt i from pos: to the world in a random direction or to an entity of the sphere (not a dead one, not the
 * one that shot in the first 0.4 seconds); not too near the end of another bolt, a thunder at its end */
static void GM_NewBolt(cObjElementFXGearMissile *self, int i, const vec3_t *pos, cDebris *debris, float dt) {
	vec3_t tmp, end, hit, o;
	CCollisionTri tri;
	cEntity *found;
	int kind = 0;

	float R = g_GearXplodeRadius;
	const vec3_t *d = FX_RandomDir(&tmp);
	end.x = d->x * R + pos->x;
	end.y = d->y * R + pos->y;
	end.z = d->z * R + pos->z;
	if (Engine_TestLine(pos, &end, &hit, &tri)) {
		kind = 1;
		hit.x = hit.x + tri.plane.n.x * 30.0f;
		hit.y = hit.y + tri.plane.n.y * 30.0f;
		hit.z = hit.z + tri.plane.n.z * 30.0f;
	}
	found = NULL;
	if (cWorld_TestSphereEntities(g_pWorld, self->entity, FXGEAR_ENTITIES, pos, R, &found, 1, 0)) {
		kind = 2;
		d = FX_RandomDir(&tmp);
		float r = Obj_Vtbl(found)->GetRadius(found);
		float ox = r * d->x * 0.25f;
		float oy = r * d->y * 0.25f;
		float oz = r * d->z * 0.25f;
		const vec3_t *c = Obj_Vtbl(found)->GetViewPos(found, &tmp);
		hit.x = c->x + ox;
		hit.y = c->y + oy;
		hit.z = c->z + oz;
		if (found->vftable->IsType(found, "WithLife")) {
			if (WLife(found)->life <= 0) {
				kind = 0;
			} else if (0.4f > self->time && self->entity &&
				!strcmp(((cMissileEntity *)self->entity)->parent, found->name)) {
				kind = 0;
			}
		}
	}
	float L = self->fadeLife;
	float life = debris->life;
	float factor;
	if (life > L) {
		factor = kind == 1 ? 1.3f : (kind == 2 ? 0.15f : 0.45f);
	} else {
		factor = 0.1f;
	}
	if (kind) {
		for (int m = 0; m < FXGEARMISSILE_TRAILS; m++) {
			if (i != m && self->boltTime[m] > 0.0f) {
				const vec3_t *p = &self->pts[m][FXGEARMISSILE_POINTS - 1];
				float dy = hit.y - p->y;
				float dx = hit.x - p->x;
				float dz = hit.z - p->z;
				if (factor * R > (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz))) {
					kind = 0;
					break;
				}
			}
		}
	}
	float ex = pos->x - hit.x;
	float ey = pos->y - hit.y;
	float ez = pos->z - hit.z;
	if (!kind) {
		return;
	}
	if (!(L > life - dt) && !((float)__builtin_sqrt((double)(ey * ey + ex * ex + ez * ez)) > R * 0.25f)) {
		return;
	}

	self->boltTime[i] = FXA_Rand(0.099999994039535522, 0.14000000059604645);
	self->boltScale[i] = FXA_Rand(0.11999998986721039, 0.20000000298023224);
	vec3_t *pts = self->pts[i];
	float s = FXA_Rand(25.0, 0.0);
	d = FX_RandomDir(&tmp);
	pts[0].x = d->x * s + pos->x;
	pts[0].y = d->y * s + pos->y;
	pts[0].z = d->z * s + pos->z;
	pts[FXGEARMISSILE_POINTS - 1] = hit;
	float sx = (hit.x - pts[0].x) * 0.2f;
	float sy = (hit.y - pts[0].y) * 0.2f;
	float sz = (hit.z - pts[0].z) * 0.2f;
	for (int k = 1; k < FXGEARMISSILE_POINTS - 1; k++) {
		float fk = (float)k;
		pts[k].x = pts[0].x + sx * fk;
		pts[k].y = pts[0].y + sy * fk;
		pts[k].z = pts[0].z + sz * fk;
		float j = FXA_Rand(8.0, 6.0);
		d = FX_RandomDir(&tmp);
		pts[k].y = d->y * j + pts[k].y;
		pts[k].z = d->z * j + pts[k].z;
		pts[k].x = d->x * j + pts[k].x;
	}
	/* two kinks in the middle */
	FXA_RandomDirScaled(&o, FXA_Rand(20.0, 40.0));
	vec3_t *q = &pts[2 + (crt_rand() & 1)];
	q->x = o.x + q->x;
	q->y = o.y + q->y;
	q->z = o.z + q->z;
	FXA_RandomDirScaled(&o, FXA_Rand(105.0, 20.0));
	q = &pts[2 + (crt_rand() & 1)];
	q->x = o.x + q->x;
	q->y = o.y + q->y;
	q->z = o.z + q->z;
	int r = crt_rand();
	const char *name = (r & 1) ? g_GearMissileThunder1Sound : g_GearMissileThunder2Sound;
	int vol = Snd_Volume(g_GearMissileThunderSoundVol);
	CSoundSys *snd = g_pSound;
	snd->vftable->PlayAt(snd, name, &hit, vol);
}

static uint32_t GM_PuffRamp[9] = { 0xff565656, 0xf6f6ff, 0x8282e6, 0x5e5eab, 0x44444c, 0xf0f38, 0x80070728, 0xff020219, 0xff000000 };

/* the puffs of the ball and the sparks behind it */
static void GM_Puffs(cObjElementFXGearMissile *self, const vec3_t *pos) {
	CGameParticles *p = &self->particles;
	CGameParticles *sp = &self->sparks;
	vec3_t v;

	GM_Still(p);
	float r = FXA_Rand(FX_2PI, 0.0);
	p->rotIni = r;
	p->rotEnd = r;
	p->rotVar = FXA_Rand(FX_2PI, 0.0);
	p->sizeLife = 1.0f;
	p->lifeTime = FXA_Rand(0.01000000536441803, 0.079999998211860657);
	p->sizeIni = FXA_Rand(10.0, 60.0);
	p->spriteIni = 2;
	p->spriteEnd = 2;
	p->sizeEnd = FXA_Rand(15.0, 80.0);
	CGameParticles_Apply(p);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, &FXGEAR_PUFF_RAMPS[9]);
	CGameParticles_EmitN(p, 1, 0);

	p->birthRate = 0;
	p->initPosMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	p->initPosMax = p->initPosMin;
	FXA_RandomDirScaled(&p->initVelMin, FXA_Rand(30.0, 120.0));
	p->initVelMax = p->initVelMin;
	float g = FXA_Rand(0.69999998807907104, 0.80000001192092896);
	p->initAccMin.x = -p->initVelMin.x * g;
	p->initAccMin.y = -p->initVelMin.y * g;
	p->initAccMin.z = -p->initVelMin.z * g;
	p->initAccMax = p->initAccMin;
	GM_RandRot(p);
	p->lifeTime = FXA_Rand(0.039999991655349731, 0.18000000715255737);
	p->sizeLife = FXA_Rand(0.099999964237213135, 0.80000001192092896);
	p->sizeIni = FXA_Rand(10.0, 60.0);
	p->spriteIni = 0x29;
	p->spriteEnd = 0x29;
	p->sizeEnd = FXA_Rand(15.0, 120.0);
	CGameParticles_Apply(p);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, GM_Ramp3());
	CGameParticles_EmitDt(p, 1, 0, 0.01f);

	GM_Puff(p, 0.030000001192092896, 0.15000000596046448, 20.0, 135.0, 0x25, 0x29, 25.0, 190.0);
	GM_Puff(p, 0.02000001072883606, 0.15999999642372131, 30.0, 45.0, 0x2a, 0x2a, 25.0, 150.0);

	CGameParticles_SetSparksOn(sp, 1);
	for (int n = 0; n < 3; n++) {
		vec3_t nd = FX_Unit((vec3_t){ self->lastPos.x - pos->x, self->lastPos.y - pos->y, self->lastPos.z - pos->z });
		float b1 = FXA_Rand(0.52359879016876221, -0.2617993950843811);
		float b2 = FXA_Rand(0.52359879016876221, -0.2617993950843811);
		float b3 = FXA_Rand(0.52359879016876221, -0.2617993950843811);
		vec3_t rot = { b3, b2, b1 };
		Vec3_Rotate(&v, &nd, &rot);
		sp->birthRate = 0;
		float q = FXA_Rand(30.0, 90.0);
		sp->initPosMin.x = pos->x - nd.x * 100.0f + q * v.x;
		sp->initPosMin.y = pos->y - nd.y * 100.0f + q * v.y;
		sp->initPosMin.z = pos->z - nd.z * 100.0f + q * v.z;
		sp->initPosMax = sp->initPosMin;
		float vs = FXA_Rand(1000.0, 850.0);
		sp->initVelMin.x = vs * v.x;
		sp->initVelMin.y = vs * v.y;
		sp->initVelMin.z = vs * v.z;
		sp->initVelMax = sp->initVelMin;
		float as = FXA_Rand(600.0, 200.0);
		sp->initAccMin.x = 0.0f - as * nd.x;
		sp->initAccMin.y = -2000.0f - as * nd.y;
		sp->initAccMin.z = 0.0f - as * nd.z;
		sp->initAccMax = sp->initAccMin;
		sp->lifeTime = FXA_Rand(0.44999998807907104, 0.5);
		GM_NoRot(sp);
		float size = FXA_Rand(15.0, 60.0);
		sp->sizeIni = size;
		sp->sizeEnd = size;
		int k = crt_rand();
		sp->spriteEnd = 0x44;
		sp->sparkMaxSize = 1.0f;
		sp->spriteIni = (byte)((k & 1) + 0x41);
		sp->sparkVelScale = 1.0f;
		sp->bounce = 1;
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, GM_PuffRamp);
		CGameParticles_EmitN(sp, 1, 0);
	}
	CGameParticles_SetSparksOn(sp, 0);
	sp->bounce = 0;
}

/* the fade of the ball at the end of the debris: 4 times three glows of the particles 0..11 around the node */
static void GM_Fade(cObjElementFXGearMissile *self, float q) {
	CGameParticles *p = &self->particles;
	vec3_t o;

	int a1 = 0xff - (int)(q * 512.0f);
	a1 = a1 < 0 ? 0 : (a1 > 0xff ? 0xff : a1);
	int a2 = 0xff - (int)(q * 350.0f);
	a2 = a2 < 0 ? 0 : (a2 > 0xff ? 0xff : a2);
	int n = 0;
	for (int it = 0; it < 4; it++) {
		int c = a1 + 4 > 0xff ? 0xff : a1 + 4;
		float rr = FXA_Rand(FX_2PI, 0.0);
		float size = FXA_Rand(15.0, 105.0);
		FXA_RandomDirScaled(&o, FXA_Rand(10.0, 10.0));
		int g = crt_rand() % 32;
		int r = crt_rand() % 32;
		uint32_t color = (uint32_t)(r + 0x80) << 16 | (uint32_t)c << 24 | (uint32_t)(g + 0x80) << 8 | 0xff;
		CGameParticles_SetParticle(p, n, o.x, o.y, o.z, size, color, 2, rr);
		n++;

		c = a2 + 8 > 0xff ? 0xff : a2 + 8;
		rr = FXA_Rand(FX_2PI, 0.0);
		size = FXA_Rand(40.0, 290.0);
		FXA_RandomDirScaled(&o, FXA_Rand(15.0, 20.0));
		g = crt_rand() % 16;
		r = crt_rand() % 16;
		color = (uint32_t)(r + 0x5c) << 16 | (uint32_t)c << 24 | (uint32_t)(g + 0x5c) << 8 | 0xff;
		CGameParticles_SetParticle(p, n, o.x, o.y, o.z, size, color, 5, rr);
		n++;

		c = a2 + 8 > 0xff ? 0xff : a2 + 8;
		rr = FXA_Rand(FX_2PI, 0.0);
		size = FXA_Rand(40.0, 290.0);
		FXA_RandomDirScaled(&o, FXA_Rand(15.0, 10.0));
		g = crt_rand() % 32;
		r = crt_rand() % 32;
		color = (uint32_t)(r + 0x40) << 16 | (uint32_t)c << 24 | (uint32_t)(g + 0x40) << 8 | 0xff;
		CGameParticles_SetParticle(p, n, o.x, o.y, o.z, size, color, 0x29, rr);
		n++;
	}
}

static uint32_t cObjElementFXGearMissile_UpdateRamp[9] = { 0xffb9b9ed, 0x80b5b5f0, 0xb2b2f2, 0xafaff5, 0x20aaaaf9, 0x40a7a7fc, 0x80a3a3ff, 0xc0a3a3ff, 0xffa3a3ff };

/* the sparks of the ball while it fades */
static void GM_FadeSparks(cObjElementFXGearMissile *self, cDebris *debris, float dt) {
	CGameParticles *p = &self->particles;
	vec3_t d;

	self->emitTime = 0.0f;
	CGameParticles_SetSparksOn(p, 1);
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, cObjElementFXGearMissile_UpdateRamp);
	for (int it = 0; it < 8; it++) {
		FX_RandomDir(&d);
		p->birthRate = 0;
		float s = FXA_Rand(5.0, 25.0);
		p->initPosMin.x = d.x * s;
		p->initPosMin.y = d.y * s;
		p->initPosMin.z = d.z * s;
		p->initPosMax = p->initPosMin;
		float vs = FXA_Rand(600.0, 350.0);
		p->initVelMin.x = d.x * vs;
		p->initVelMin.y = d.y * vs;
		p->initVelMin.z = d.z * vs;
		p->initVelMax = p->initVelMin;
		float as = FXA_Rand(0.099999994039535522, 0.30000001192092896);
		p->initAccMin.x = -p->initVelMin.x * as + 0.0f;
		p->initAccMin.y = -p->initVelMin.y * as - 980.0f;
		p->initAccMin.z = -p->initVelMin.z * as + 0.0f;
		p->initAccMax = p->initAccMin;
		float lt = FXA_Rand(0.099999994039535522, 0.34999999403953552);
		p->lifeTime = lt;
		p->lifeTime = Float_MinSS(debris->life - dt, lt);
		GM_NoRot(p);
		float size = FXA_Rand(30.0, 40.0);
		p->sizeIni = size;
		p->sizeEnd = size;
		int k = crt_rand();
		p->spriteEnd = 0x44;
		p->sparkMaxSize = 1.0f;
		p->spriteIni = (byte)((k & 1) + 0x41);
		p->sparkVelScale = 1.0f;
		CGameParticles_Apply(p);
		CGameParticles_EmitDt(p, 1, 0, FXA_Rand(0.029999999329447746, 0.0));
	}
	CGameParticles_SetSparksOn(p, 0);
}

/* vf01: the bolts, the tail, the glows, puffs and sparks of the ball and its fade at the end of the debris */
static void cObjElementFXGearMissile_Update(cObjElementFX *base, cDebris *debris, float dt) {
	cObjElementFXGearMissile *self = (cObjElementFXGearMissile *)base;
	CGameModel *model = Debris_Model(debris);
	CGameParticles *p = &self->particles;
	vec3_t pos, rot, a, b, tmp;

	cEntity *e = self->entity;
	if (e && (Entity_Removed(e) || !e->active)) {
		if (!e->active) {
			e->vftable->GetPos(e, &a);
			GM_EntityRot(self->entity, &b);
			CGameModel_SetPos(model, &a);
			CGameModel_SetRot(model, &b);
			self->trailStep = 0.03f;
		}
		self->entity = NULL;
	}
	e = self->entity;
	if (!e || !e->active || self->node == -1) {
		if (debris->life > self->fadeLife) {
			debris->life = self->fadeLife;
		}
	}
	float t = cTimer_GetDelta(&g_pWorld->timer);
	self->emitTime = self->emitTime + t;
	self->time = self->time + t;
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	e = self->entity;
	if (e && e->active) {
		pos = *Obj_Vtbl(e)->GetViewPos(e, &tmp);
		CGameModel_SetPos(model, &pos);
		CGameModel_SetRot(model, &rot);
		CGameModel_SetSortZ(model, -71.0f, 0.0f);
	}
	if (self->time > 0.2f) {
		for (int i = 0; i < FXGEARMISSILE_TRAILS; i++) {
			if (self->boltTime[i] > 0.0f) {
				GM_Bolt(self, i, &pos, t);
			} else if (debris->life - dt > 0.25f) {
				GM_NewBolt(self, i, &pos, debris, dt);
			}
		}
	}

	/* the tail */
	float life = debris->life;
	float L = self->fadeLife;
	int fade = 0;
	if (L > life) {
		fade = (int)((1.0f - (life - t) / L) * 255.0f);
		if (fade < 0) {
			fade = 0;
		} else if (fade > 0xff) {
			fade = 0xff;
		}
	}
	float ts = t + self->trailStep;
	self->trailStep = ts;
	if (ts > 0.025f) {
		self->trailStep = 0.0f;
		if (debris->life > L) {
			CGameTrail_Add(&self->trail, pos, 95.0f, 0x604040ff);
		} else if (CGameTrail_IsRender(&self->trail)) {
			CGameTrail_SetLastPos(&self->trail, pos);
			CGameTrail_Shrink(&self->trail);
		}
	} else {
		const vec3_t *d = FX_RandomDir(&tmp);
		float s = FXA_Rand(10.0, 0.0);
		CGameTrail_SetLastPos(&self->trail, (vec3_t){ d->x * s + pos.x, d->y * s + pos.y, d->z * s + pos.z });
	}

	/* the glows of the ball */
	uint32_t c = GM_Color(fade + 0xeb, 0x1818c0);
	CGameParticles_SetParticle(p, 3, 0.0f, 0.0f, 0.0f, FXA_Rand(110.0, 1040.0), c, 5, 0.0f);
	c = GM_Color(fade + 0xc0, 0x2626dc);
	CGameParticles_SetParticle(p, 2, 0.0f, 0.0f, 0.0f, FXA_Rand(50.0, 410.0), c, 2, 0.0f);
	c = GM_Color(fade + 0x5c, 0x3232ff);
	CGameParticles_SetParticle(p, 1, 0.0f, 0.0f, 0.0f, FXA_Rand(50.0, 270.0), c, 2, 0.0f);
	c = GM_Color(fade + 0x10, 0xa4a4ff);
	CGameParticles_SetParticle(p, 0, 0.0f, 0.0f, 0.0f, FXA_Rand(35.0, 175.0), c, 1, 0.0f);
	if (debris->life > self->fadeLife && self->emitTime > 0.02f) {
		self->emitTime = 0.0f;
		GM_Puffs(self, &pos);
	}

	life = debris->life;
	L = self->fadeLife;
	if (!(L < life)) {
		GM_Fade(self, (life - dt) / L);
	}
	life = debris->life;
	L = self->fadeLife;
	if (!(L < life) && life - dt > 0.2f && self->emitTime > 0.015f) {
		GM_FadeSparks(self, debris, dt);
	}
	if (self->entity && self->entity->active) {
		self->lastPos = pos;
	}
}

void cObjElementFXBankMasterSuction_RandomPoints(cObjElementFXBankMasterSuction *self) {
	vec3_t a, b;

	self->ent->vftable->GetPos(self->ent, &a);
	self->target->vftable->GetPos(self->target, &b);
	for (int i = 0; i < FXBMSUCTION_POINTS; i++) {
		self->phase[i] = (float)(FXA_Rnd() * (double)0.9f - (double)0.9f);
		FXA_RandomDirScaled(&self->dir[i], FXA_Rand(0.75, 0.25));
		self->angle[i] = FXA_Rand((double)6.28318548f, 0.0);
	}
}

void cObjElementFXBertoFlash_RandomPoints(cObjElementFXBertoFlash *self) {
	for (int i = 0; i < FXBERTOFLASH_POINTS; i++) {
		FXA_RandomDirScaled(&self->dir[i], FXA_Rand(4.0, 10.0));
		float lo = Obj_Vtbl(self->target)->GetRadius(self->target) * 0.65f;
		float hi = Obj_Vtbl(self->target)->GetRadius(self->target) * 0.85f;
		FXA_RandomDirScaled(&self->dir2[i], (float)(FXA_Rnd() * (double)(hi - lo) + (double)lo));
	}
}

void cObjElementFXSebastianFlash_RandomPoints(cObjElementFXSebastianFlash *self) {
	for (int i = 0; i < FXSEBASTIAN_POINTS; i++) {
		FXA_RandomDirScaled(&self->dir[i], FXA_Rand(3.0, 0.0));
		FXA_RandomDirScaled(&self->dir2[i], FXA_Rand(65.0, 15.0));
	}
}

/* the sines and cosines of the negated angles of rot, for Vec3_RotateSC */
static void FXA_InvRotSC(const vec3_t *rot, vec3_t *c, vec3_t *s) {
	Float_SinCos(-rot->x, &s->x, &c->x);
	Float_SinCos(-rot->y, &s->y, &c->y);
	Float_SinCos(-rot->z, &s->z, &c->z);
}

void cObjElementFXKill_Start(cObjElementFXKill *self, cEntity *e) {
	vec3_t center, rot, c, s, tmp, rel;
	vec3_t pts[FXKILL_POINTS];

	self->ent = e;
	CGameModel *m = FXA_Model(e);
	CGameModel_GetPos(m, &center);
	CGameModel_GetRot(m, &rot);
	float scale = Obj_Vtbl(self->ent)->GetRadius(self->ent) / 90.0f;
	CGameModel_Vertices(m, pts, NULL, NULL, FXKILL_POINTS, -1, self->vertex, 0xffffffffu, NULL);
	FXA_InvRotSC(&rot, &c, &s);
	for (int i = 0; i < FXKILL_POINTS; i++) {
		rel.x = pts[i].x - center.x;
		rel.y = pts[i].y - center.y;
		rel.z = pts[i].z - center.z;
		self->point[i] = *Vec3_RotateSC(&tmp, &rel, &c, &s);
		self->scale[i] = FXA_Rand(15.0, 60.0) * scale;
		self->lenTarget[i] = FXA_Rand(100.0, 525.0);
		self->len[i] = FXA_Rand(30.0, 105.0);
	}
}

void cObjElementFXDTritusAttack_Start(cObjElementFXDTritusAttack *self, cEntity *e) {
	vec3_t center, rot, c, s, tmp, rel;
	vec3_t pts[FXDTRITUSATTACK_POINTS];

	self->ent = e;
	CGameModel *m = FXA_Model(e);
	CGameModel_GetPos(m, &center);
	CGameModel_GetRot(m, &rot);
	self->nodeMaster = CGameModel_FindRenderNode(m, "R_Detritus_Tronco");
	CHECK(self->nodeMaster > -1);
	CGameModel_Vertices(m, &center, NULL, NULL, 1, 0, NULL, (uint32_t)self->nodeMaster, NULL);
	CGameModel_Vertices(m, pts, NULL, NULL, FXDTRITUSATTACK_POINTS, -1, self->vertex, 0xffffffffu, NULL);
	FXA_InvRotSC(&rot, &c, &s);
	for (int i = 0; i < FXDTRITUSATTACK_POINTS; i++) {
		rel.x = pts[i].x - center.x;
		rel.y = pts[i].y - center.y;
		rel.z = pts[i].z - center.z;
		self->point[i] = *Vec3_RotateSC(&tmp, &rel, &c, &s);
		self->width[i] = FXA_Rand(15.0, 40.0);
		self->lenTarget[i] = FXA_Rand(50.0, 375.0);
		self->len[i] = FXA_Rand(10.0, 25.0);
	}
}

/* the vertices of the Functionary model the special action starts from, found once (rva 0x466fc0..) */
static byte g_FXFunctionaryInit;
static int g_FXFunctionaryVertex[14];
static uint32_t g_FXFunctionaryColor[14];

void cObjElementFXFunctionarySpecialAction_Start(void *self, cEntity *e) {
	cObjElementFXFunctionarySpecialAction *s = self;
	vec3_t v, rot, out, pos;

	s->ent = e;
	CHECK(s->ent);
	CHECK(s->ent->vftable->IsType(s->ent, TYEN_WALKCHAR));
	CGameModel *m = Obj_Vtbl(s->ent)->GetModel(s->ent);
	CHECK(CGameModel_IsLoaded(m));
	if (!g_FXFunctionaryInit) {
		g_FXFunctionaryInit = 1;
		float r = CGameModel_Radius(m) * 0.8f;
		float step = (r + r) * 0.0625f;
		float up = r * 0.1f;
		for (int i = 0; i < FXFUNCTIONARY_TRAILS; i++) {
			float side = (i & 1) ? -20.0f : 20.0f;
			v.x = ((crt_rand() & 1) ? 1.0f : 1.5f) * side;
			v.y = (float)(i & ~1) * step - r;
			v.z = up;
			const vec3_t *p = Vec3_Rotate(&out, &v, CGameModel_GetRot(m, &rot));
			const vec3_t *q = CGameModel_GetPos(m, &pos);
			CGameModel_ClosestVertex(m, p->x + q->x, q->y + p->y, q->z + p->z, &s->pos[i],
				&g_FXFunctionaryVertex[i], 0xffffffffu);
		}
	}
	for (int i = 0; i < FXFUNCTIONARY_TRAILS; i++) {
		uint32_t g = (uint32_t)(crt_rand() % 24 + 0x92) << 8;
		uint32_t r = (uint32_t)(crt_rand() % 24 + 0x92) << 16;
		g_FXFunctionaryColor[i] = (uint32_t)(crt_rand() % 24 + 0x92) | r | g;
		CGameModel_Vertices(m, &s->pos[i], NULL, NULL, 1, g_FXFunctionaryVertex[i], NULL, 0xffffffffu, NULL);
	}
}

/* the ramps of the particles (9 colors each) */
#define FXA_RAMP_BETTY 0x4307d0
#define FXA_RAMP_GEAR 0x4305d8		/* [3] */

/* p moved by a random direction (FX_RandomDir) times r */
static void FXA_Jitter(vec3_t *p, float r) {
	vec3_t tmp;
	const vec3_t *d = FX_RandomDir(&tmp);

	p->x = r * d->x + p->x;
	p->y = p->y + d->y * r;
	p->z = p->z + d->z * r;
}

static uint32_t cObjElementFXBettyAttack_EmitRamps[18] = {
	0x20fff5c3, 0x20ffc36e, 0x20965028, 0x305e220a, 0x60441205, 0xa4190700, 0xde0a0200, 0xff000000, 0xff000000,
	0x20fff57d, 0x20ffb950, 0x20a0461e, 0x306e1805, 0x604e1202, 0xa4280700, 0xde0a0200, 0xff000000, 0xff000000,
};

void cObjElementFXBettyAttack_Emit(cObjElementFXBettyAttack *self, float x, float y, float z) {
	CGameParticles *sp = &self->particles;
	vec3_t v, ang, out;

	CHECK(self->ent);
	self->pos.x = x;
	self->pos.y = y;
	self->pos.z = z;
	sp->birthRate = 0;
	CGameParticles_Apply(sp);
	sp->rotIni = 0.0f;
	sp->rotEnd = 0.0f;
	sp->rotVar = 0.0f;
	float life = FXA_Rand((double)0.04000002f, (double)0.7f);
	sp->sizeLife = 1.0f;
	sp->spriteIni = 5;
	sp->spriteEnd = 5;
	sp->lifeTime = life;
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, cObjElementFXBettyAttack_EmitRamps);
	float a = -3.14159274f;
	do {
		float b = -3.14159274f;
		do {
			ang = (vec3_t){ a, b, 0.0f };
			v = (vec3_t){ 0.0f, -1.0f, 0.0f };
			Vec3_Rotate(&out, &v, &ang);
			float r1 = (float)(FXA_Rnd() * 4.0 - 2.0);
			float r2 = (float)(FXA_Rnd() * 4.0 - 2.0);
			float r3 = (float)(FXA_Rnd() * 4.0 - 2.0);
			sp->initPosMin.z = r1 + (out.z * 15.0f + self->pos.z);
			sp->initPosMin.x = r3 + (self->pos.x + out.x * 15.0f);
			sp->initPosMin.y = r2 + (out.y * 15.0f + self->pos.y);
			sp->initPosMax = sp->initPosMin;
			sp->initVelMin.x = out.x * 150.0f;
			sp->initVelMin.y = out.y * 150.0f;
			sp->initVelMin.z = out.z * 150.0f;
			sp->initVelMax = sp->initVelMin;
			float k = FXA_Rand((double)0.050000012f, (double)0.45f);
			sp->initAccMin.x = k * -sp->initVelMin.x;
			sp->initAccMin.y = k * -sp->initVelMin.y;
			sp->initAccMin.z = k * -sp->initVelMin.z;
			sp->initAccMax = sp->initAccMin;
			sp->sizeIni = FXA_Rand((double)0.099999964f, (double)0.8f) * 190.0f;
			sp->sizeEnd = FXA_Rand((double)0.099999964f, (double)0.8f) * 265.0f;
			CGameParticles_Apply(sp);
			CGameParticles_EmitN(sp, 1, 0);
			b = b + 1.04719758f;
		} while (3.14159274f > b);
		a = a + 1.04719758f;
	} while (3.14159274f > a);
}

/* the yaw of the direction from the camera (or a point) to p, the way the original compares sides */
static float FXBetty_Yaw(const vec3_t *from, const vec3_t *to) {
	vec3_t ang;
	vec3_t n = FXA_Dir(from, to);

	return Vec3_GetRotAng(&ang, &n)->y;
}

/* the attack of Betty: bolts out of the center, a ring that grows with bolts and dust on the side away from the
 * camera, the ring line, a wave in the dudv pass and a flash */
static void cObjElementFXBettyAttack_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXBettyAttack *self = (cObjElementFXBettyAttack *)base;
	vec3_t p, rot, o, dir, ang, v;
	float a;

	(void)dt;
	cEntity *e = self->ent;
	if (e && (Entity_Removed(e) || !e->active)) {
		self->ent = NULL;
	}
	if (!self->ent || !self->ent->active) {
		d->life = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &p);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &p);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -87.0f, 0.0f);
	CGameParticles *sp = &self->particles;
	int np = CGameParticles_NumParticles(sp);
	float delta = cTimer_GetDelta(&g_pWorld->timer);
	self->time = self->time + delta;
	self->t2 = self->t2 + delta;
	vec3_t cam = g_pWorld->camPos[g_pWorld->curCam];
	/* the side of the attack the camera sees */
	float yaw = FXBetty_Yaw(&cam, &p);

	/* the bolts out of the center */
	if (FX_InRange(self->time, 0.0f, 0.8f, &a)) {
		int alpha = (int)(a * 255.0f);
		CGameTrail_Reset(&self->trails[0]);
		CGameTrail_Reset(&self->trails[1]);
		int pt = 0, pidx = np - 0x3d;
		CGameTrail *tr = &self->trails[0];
		for (int i = 0; i < 30; i++) {
			ang = (vec3_t){ 0.0f, FXA_Rand(6.2831854820251465, 0.0), 0.0f };
			v = (vec3_t){ FXA_Rand(1.0, 0.0), 0.0f, 0.0f };
			Vec3_Rotate(&o, &v, &ang);
			float k = FXA_Rand(86.0, 10.0);
			float x = (self->pos.x + 0.0f) + o.x * k * a;
			float y = (self->pos.y - 20.0f) + o.y * k * a;
			float z = (self->pos.z + 0.0f) + o.z * k * a;
			float y2 = y + FXA_Rand(70.0, 180.0);
			float w = FXA_Rand(10.0, 35.0) * a + 10.0f;
			int c1 = alpha + 0x20;
			if (c1 > 0xff) {
				c1 = 0xff;
			}
			CGameTrail_Add(tr, (vec3_t){ x, y, z }, w, (uint32_t)c1 << 24 | 0xffd25a);
			CGameTrail_Add(tr, (vec3_t){ x + 0.0f, y2, z + 0.0f }, w, 0xffff500a);
			int c2 = alpha + 0x10;
			if (c2 > 0xff) {
				c2 = 0xff;
			}
			uint32_t g = (uint32_t)(crt_rand() % 15 + 0xe1) << 8;
			uint32_t rg = (uint32_t)(crt_rand() % 20 + 0xe6) << 16 | g;
			uint32_t color = (uint32_t)(crt_rand() % 10 + 0x91) | (uint32_t)c2 << 24 | rg;
			CGameParticles_SetParticle(sp, pidx, x, y, z, w * 2.5f, color, 2, 0.0f);
			if (++pt >= 15) {
				pt = 0;
				tr++;
			}
			pidx--;
		}
		CGameParticles_SetParticle(sp, 0, self->pos.x, self->pos.y, self->pos.z, (1.0f - a) * 400.0f,
			(uint32_t)(int)Float_MinSS(255.0f, a * 255.0f + 64.0f) << 24 | 0xb4b4ff, 5, 0.0f);
	}

	/* the ring that grows, its bolts and glows on the side away from the camera */
	if (FX_InRange(self->time, 0.0f, 1.45f, &a)) {
		int alpha = (int)(a * 255.0f);
		int pt = 0;
		CGameTrail *tr = &self->trails2[0];
		for (int i = 0; i < FXBETTYATTACK_TRAILS; i++) {
			CGameTrail_Reset(&self->trails2[i]);
		}
		for (int i = 0, pidx = np - 1; i < 60; i++, pidx--) {
			ang = (vec3_t){ 0.0f, (float)i / 60.0f * 6.28318548f, 0.0f };
			v = (vec3_t){ 1.0f, 0.0f, 0.0f };
			Vec3_Rotate(&o, &v, &ang);
			float q = 1.0f - a;
			float r = (q * q) * (2.0f - q) * delta * 28.5f + self->ring;
			self->ring = r;
			float x = o.x * r + self->pos.x, y = o.y * r + self->pos.y, z = o.z * r + self->pos.z;
			float y2 = self->ring * FXA_Rand(0.10000002384185791, 0.5) + 75.0f + y;
			float x2 = x + 0.0f, z2 = z + 0.0f;
			float w = self->ring * FXA_Rand(0.040000021457672119, 0.51999998092651367);
			vec3_t pp = { x, y, z };
			byte back = !(1.5f > (float)__builtin_fabs((double)(FXBetty_Yaw(&pp, &p) - yaw)));
			if (back) {
				uint32_t g = (uint32_t)(crt_rand() % 20 - (int)((a * 0.8f + -1.0f) * 85.0f)) << 8;
				uint32_t bg = (uint32_t)(crt_rand() % 10 - (int)((a * 0.9f + -1.0f) * 30.0f)) | g;
				uint32_t color = (uint32_t)(crt_rand() % 24 + 0xe6) << 16 | bg;
				int c = alpha + 0x30;
				if (c < 0xff) {
					CGameTrail_Add(tr, pp, w, (uint32_t)c << 24 | (color & 0xffffff));
					CGameTrail_Add(tr, (vec3_t){ x2, y2, z2 }, w, color | 0xff000000);
				}
				if (++pt >= 15) {
					pt = 0;
					tr++;
				}
			}
			uint32_t g = (uint32_t)(crt_rand() % 20 - (int)((a * 0.8f + -1.0f) * 165.0f)) << 8;
			uint32_t bg = (uint32_t)(crt_rand() % 10 - (int)((a * 0.9f + -1.0f) * 80.0f)) | g;
			uint32_t color = (uint32_t)(crt_rand() % 24 + 0xe6) << 16 | bg;
			if (back) {
				int al = 0.6f > a ? 0 : (int)(((a - 0.5f) + (a - 0.5f)) * 255.0f);
				al += 8;
				if (al > 0xff) {
					al = 0xff;
				}
				CGameParticles_SetParticle(sp, pidx, x2, a * 40.0f + y, z2, w * 0.7f,
					(uint32_t)al << 24 | (color & 0xffffff), 1, 0.0f);
			} else {
				CGameParticles_Kill(sp, pidx);
			}
			if (g_SceneDuDvRTarget && self->particles2.ParticleSystem) {
				int n2 = CGameParticles_NumParticles(&self->particles2);
				if (back) {
					int ai = (int)(a * 255.0f);
					uint32_t cc = (uint32_t)(0xff - ai);
					CGameParticles_SetParticle(&self->particles2, n2 - i - 1, x2, a * 140.0f + y, z2, w * 0.6f,
						(uint32_t)ai << 24 | cc << 16 | cc << 8 | cc, 1, 0.0f);
				} else {
					CGameParticles_Kill(&self->particles2, n2 - i - 1);
				}
			}
		}
	}

	/* the ring line */
	if (FX_InRange(self->time, 0.0f, 1.05f, &a)) {
		int ai = (int)(a * 255.0f) + 0x20;
		uint32_t a24 = (uint32_t)(ai > 0xff ? 0xff : ai) << 24;
		CGameTrail_Reset(&self->trail);
		for (int i = 0; i <= 0x1a; i++) {
			ang = (vec3_t){ 0.0f, (float)i / 25.0f * 6.28318548f, 0.0f };
			v = (vec3_t){ 1.0f, 0.0f, 0.0f };
			Vec3_Rotate(&o, &v, &ang);
			float r = self->ring;
			vec3_t q = { (o.x * r + self->pos.x) + 0.0f, (o.y * r + self->pos.y) + 1.0f, (o.z * r + self->pos.z) + 0.0f };
			CGameTrail_Add(&self->trail, q, 0.0f, a24 | 0xffa032);
		}
		vec3_t q = { self->pos.x + 0.0f, self->pos.y + 1.0f, self->pos.z + 0.0f };
		CGameTrail_Add(&self->trail, q, 0.0f, a24 | 0xff5a10);
	}

	/* the dust on the ring, 15 times a second */
	if (FX_InRange(self->time, 0.0f, 0.75f, &a) && (int)(self->lastTime * 15.0f) != (int)(self->time * 15.0f)) {
		sp->birthRate = 0;
		for (int i = 0; i <= 0x18; i++) {
			float ay = FXA_Rand(0.10000000894069672, 0.05000000074505806);
			ang = (vec3_t){ 0.0f, ay + (float)i / 24.0f * 6.28318548f, 0.0f };
			v = (vec3_t){ 1.0f, 0.0f, 0.0f };
			Vec3_Rotate(&o, &v, &ang);
			float r = self->ring;
			vec3_t pp = { o.x * r + self->pos.x, o.y * r + self->pos.y, o.z * r + self->pos.z };
			if (1.0f > (float)__builtin_fabs((double)(FXBetty_Yaw(&pp, &p) - yaw))) {
				continue;
			}
			float k = FXA_Rand(5.0, 0.0);
			FX_RandomDir(&dir);
			sp->initPosMin.x = k * dir.x + pp.x;
			sp->initPosMin.y = dir.y * k + pp.y;
			sp->initPosMin.z = dir.z * k + pp.z;
			sp->initPosMax = sp->initPosMin;
			float kv = FXA_Rand(160.0, 90.0);
			sp->initVelMin.x = o.x * kv;
			sp->initVelMin.y = o.y * kv;
			sp->initVelMin.z = o.z * kv;
			sp->initVelMax = sp->initVelMin;
			float ka = FXA_Rand(0.050000011920928955, 0.44999998807907104);
			sp->initAccMin.x = -sp->initVelMin.x * ka;
			sp->initAccMin.y = -sp->initVelMin.y * ka;
			sp->initAccMin.z = -sp->initVelMin.z * ka;
			sp->initAccMax = sp->initAccMin;
			sp->sizeIni = (a * 1.4f + 0.8f) * (FXA_Rand(0.19999998807907104, 0.80000001192092896) * 120.0f);
			sp->sizeEnd = (a * 1.4f + 0.8f) * (FXA_Rand(0.19999998807907104, 0.80000001192092896) * 310.0f);
			sp->rotIni = sp->rotEnd = FXA_Rand(6.2831854820251465, 0.0);
			sp->rotVar = FXA_Rand(4.7123889923095703, -2.3561944961547852);
			sp->sizeLife = 1.0f;
			sp->spriteIni = 0x33;
			sp->spriteEnd = 0x33;
			sp->lifeTime = FXA_Rand(0.04999995231628418, 0.85000002384185791);
			CGameParticles_SetColorRampOn(sp, 1);
			CGameParticles_SetColorRamp(sp, cObjElementFXBettyAttack_EmitRamps + (crt_rand() & 1) * 9);
			CGameParticles_Apply(sp);
			CGameParticles_EmitN(sp, 1, 0);
		}
	}

	/* the wave in the dudv pass */
	if (g_SceneDuDvRTarget && self->particles2.ParticleSystem && FX_InRange(self->time, 0.0f, 0.2f, &a)) {
		float q;
		if (0.06f > self->time) {
			FX_InRange(self->time, 0.0f, 0.06f, &a);
			q = (a * a) * (2.0f - a);
		} else {
			FX_InRange(self->time, 0.08f, 0.2f, &a);
			q = 1.0f - (3.0f - a * 2.0f) * (a * a);
		}
		uint32_t c = (uint32_t)(int)(q * 255.0f);
		uint32_t color = (((~c << 8 | c) << 8 | c) << 8) | c;
		for (int i = 2; i < 12; i++) {
			CGameParticles_SetParticle(&self->particles2, i, self->pos.x, self->pos.y, self->pos.z,
				(float)(i * i) * q * 1.6f, color, 2, 0.0f);
		}
	}

	/* the flash */
	if (FX_InRange(self->time, 0.0f, 0.4f, &a)) {
		CGameLensFlare_SetPos(&self->flare, self->pos);
		CGameLensFlare_SetOn(&self->flare, 1);
		CGameLensFlare_SetIntensity(&self->flare, 0.5f - a * 0.5f);
		CGameLensFlare_SetScale(&self->flare, 1.0f);
	} else {
		CGameLensFlare_SetOn(&self->flare, 0);
	}
	self->lastTime = self->time;
}
#define FXA_RAMP_GEARPIECE 0x430644

/* the armors still shown hidden */
static void FXGear_HideArmors(cObjElementFXPoliceGearConversion *self) {
	for (int i = 0; i < FXGEAR_ARMORS; i++) {
		if (CGameModel_IsEnabled(&self->armor[i])) {
			CGameModel_Disable(&self->armor[i]);
		}
	}
}

/* a still particle at p */
static void FXA_StillAt(CGameParticles *sp, float x, float y, float z) {
	sp->initPosMin.x = x;
	sp->initPosMin.y = y;
	sp->initPosMin.z = z;
	sp->initPosMax = sp->initPosMin;
	sp->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initAccMax = sp->initAccMin;
}

/* the acceleration k * -vel (the particles slow down) */
static void FXA_Brake(CGameParticles *sp, float k) {
	sp->initAccMin.x = k * -sp->initVelMin.x;
	sp->initAccMin.y = k * -sp->initVelMin.y;
	sp->initAccMin.z = k * -sp->initVelMin.z;
	sp->initAccMax = sp->initAccMin;
}

static uint32_t cObjElementFXPoliceGearConversion_StartRamps[27] = {
	0xffff6e46, 0x6eff5a37, 0x80b4462a, 0x8778371e, 0x9632190f, 0xa4140503, 0xc0080201, 0xff000000, 0xff000000,
	0xffff965a, 0x6eff6e41, 0x80b45032, 0x87783c23, 0x9632190f, 0xa4140503, 0xc0080201, 0xff000000, 0xff000000,
	0xffa48278, 0x6e91695f, 0x806e5046, 0x875a3c32, 0x96321923, 0xa4140519, 0xc0080205, 0xff000000, 0xff000000,
};

static uint32_t cObjElementFXPoliceGearConversion_StartRamp[9] = { 0xffebebff, 0x20e1e1ff, 0x20d5d5ff, 0x20cacaff, 0x40c0c0ff, 0x60c0c0ff, 0x80c0c0ff, 0xffc0c0ff, 0xffc0c0ff };

/* the conversion of the police into Gear: the armors fade in, bubbles with bolts rise around the waist, smoke, the
 * pieces of the armors fall off with sparks */
static void cObjElementFXPoliceGearConversion_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXPoliceGearConversion *self = (cObjElementFXPoliceGearConversion *)base;
	vec3_t p, rot, v, dir, o, ang, tmp, verts[6];
	float a;
	uint32_t bits;

	(void)dt;
	cEntity *e = self->ent;
	if (e && (Entity_Removed(e) || !e->active)) {
		self->ent = NULL;
		FXGear_HideArmors(self);
	}
	if (!self->ent || !self->ent->active) {
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &p);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &p);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -76.0f, 0.0f);
	if (CGameModel_IsVisible(m)) {
		CGameModel_Enable(Debris_Model(d));
	} else {
		CGameModel_Disable(Debris_Model(d));
	}
	float delta = cTimer_GetDelta(&g_pWorld->timer);
	self->time = delta + self->time;
	self->t2 = delta + self->t2;
	if (self->ent && !cActCtrl_IsActive(WithLife_Vtbl(self->ent)->GetActCtrl(self->ent), "Fusion")) {
		FXGear_HideArmors(self);
	}
	/* the armors fade in over their times of the fusion */
	static const float from[FXGEAR_ARMORS] = { 2.6f, 3.0f, 3.2f }, to[FXGEAR_ARMORS] = { 2.9f, 3.1f, 3.5f };
	for (int i = 0; i < FXGEAR_ARMORS; i++) {
		FX_InRange(self->time, from[i], to[i], &a);
		CGameModel *am = &self->armor[i];
		CGameModel_AnimDelta(am, (uint32_t)self->anim[i], self->time, NULL);
		CGameModel_SetEffect(am, 3);
		float f = 1.0f - (a * a) * (2.0f - a);
		memcpy(&bits, &f, sizeof(bits));
		CGameModel_SetEffectParam(am, 0, bits);
	}
	CGameParticles *sp = &self->particles;
	if (FX_InRange(self->time, 0.6f, 4.0f, &a)) {
		vec3_t root;

		self->emitT = self->emitT + delta;
		CGameModel_Vertices(m, &root, NULL, NULL, 1, 0, NULL, (uint32_t)self->nodeRoot, NULL);
		float s = (3.0f - a * 2.0f) * (a * a);
		int alpha = (int)((3.0f - (s + s)) * (s * s) * 255.0f);
		for (int i = 0; i < FXGEAR_TRAILS; i++) {
			CGameTrail_Reset(&self->trails[i]);
			CGameTrail_SetTransp(&self->trails[i], (byte)alpha);
		}
		/* the bubbles rise and leave bolts and glows */
		int trail = 0, pt = 0;
		if (CGameModel_IsVisible(m)) {
			for (int k = 0, idx = 0xfe; k < self->count; k++, idx--) {
				vec3_t *bp = &self->bubblePos[k];
				if (self->bubbleH[k] > 0.25f && trail < 4) {
					float r1 = FXA_Rand(112.0, -16.0);
					float y = (((root.y - bp->y) + (root.y - bp->y)) + r1) + bp->y;
					float x = bp->x + 0.0f, z = bp->z + 0.0f;
					float w = self->bubbleH[k] * FXA_Rand(0.30000001192092896, 0.80000001192092896);
					uint32_t g = (uint32_t)(crt_rand() % 32 + 0x46) << 8;
					uint32_t rg = (uint32_t)(crt_rand() % 64 + 0xb4) << 16 | g;
					CGameTrail *tr = &self->trails[trail];
					uint32_t color = (uint32_t)(crt_rand() % 32 + 0x28) | rg | 0x30000000;
					CGameTrail_Add(tr, *bp, w, color);
					g = (uint32_t)(crt_rand() % 24 + 0x32) << 8;
					rg = (uint32_t)(crt_rand() % 48 + 0x6e) << 16 | g;
					color = (uint32_t)(crt_rand() % 16 + 0x1e) | rg | 0xff000000;
					CGameTrail_Add(tr, (vec3_t){ x, y, z }, w, color);
					int al = alpha + 0x30;
					if (al > 0xff) {
						al = 0xff;
					}
					float sz = FXA_Rand(0.74999994039535522, 0.80000001192092896);
					float px = bp->x + 0.0f, py = bp->y + 10.0f, pz = bp->z + 0.0f;
					g = (uint32_t)(crt_rand() % 32 + 0x3c) << 8;
					rg = (uint32_t)(crt_rand() % 24 + 0xdc) << 16 | g;
					color = (uint32_t)(crt_rand() % 32 + 0x14) | (uint32_t)al << 24 | rg;
					CGameParticles_SetParticle(sp, idx, px, py, pz, sz * w, color, 6, 0.0f);
					if (++pt >= 15) {
						trail++;
						pt = 0;
					}
				}
				float vel = self->bubbleVel[k];
				self->bubbleH[k] = vel * delta + self->bubbleH[k];
				self->bubbleVel[k] = delta * self->bubbleAcc[k] + vel;
				float mx = self->bubbleMax[k];
				if (self->bubbleH[k] > mx && 3.40282347e+38f > mx) {
					self->bubbleH[k] = mx;
					self->bubbleVel[k] = 0.0f;
					self->bubbleAcc[k] = 0.0f;
				}
			}
		}
		/* new bubbles on a ring around the waist */
		while (self->count < FXGEAR_BUBBLES) {
			float r = Obj_Vtbl(self->ent)->GetRadius(self->ent);
			float hh = WChar(self->ent)->floorDist;
			float a1 = FXA_Rand(6.2831854820251465, -3.1415927410125732);
			ang.x = FXA_Rand(6.2831854820251465, -3.1415927410125732);
			ang.y = 0.0f;
			ang.z = a1;
			v = (vec3_t){ 0.0f, -1.0f, 0.0f };
			Vec3_Rotate(&o, &v, &ang);
			float x = root.x + o.x * r;
			float z = root.z + o.z * r;
			const vec3_t *c = Obj_Vtbl(self->ent)->GetViewPos(self->ent, &tmp);
			int k = self->count;
			self->bubblePos[k].y = c->y - hh;
			self->bubblePos[k].x = x;
			self->bubblePos[k].z = z;
			self->bubbleH[k] = FXA_Rand(1.5, 4.5);
			self->bubbleVel[k] = FXA_Rand(110.0, 85.0);
			self->bubbleAcc[k] = -FXA_Rand(7.0, 8.5);
			self->bubbleMax[k] = FXA_Rand(30.0, 105.0);
			self->count++;
		}
		/* the smoke of the bubbles */
		if (CGameModel_IsVisible(m) && 3.2f > self->time && self->emitT > 0.03f) {
			self->emitT = 0.0f;
			crt_rand();
			crt_rand();
			for (int n = 0; n < 3; n++) {
				const vec3_t *bp = &self->bubblePos[crt_rand() % self->count];
				float r1 = FXA_Rand(32.0, -16.0);
				float x0 = (bp->x + 0.0f) + 0.0f;
				float y0 = r1 + (bp->y + (root.y - bp->y) * 0.5f);
				float z0 = (bp->z + 0.0f) + 0.0f;
				float r2 = FXA_Rand(6.0, 2.0);
				float r3 = FXA_Rand(192.0, 64.0);
				float r4 = FXA_Rand(6.0, 2.0);
				float x1 = r4 + (bp->x + 0.0f);
				float y1 = r3 + (bp->y + (root.y - bp->y));
				float z1 = r2 + (bp->z + 0.0f);
				float lt = FXA_Rand(0.050000011920928955, 0.44999998807907104);
				float k = FXA_Rand(5.0, 10.0);
				FX_RandomDir(&dir);
				sp->initPosMin.x = dir.x * k + x0;
				sp->initPosMin.y = dir.y * k + y0;
				sp->initPosMin.z = dir.z * k + z0;
				sp->initPosMax = sp->initPosMin;
				float inv = 1.0f / lt;
				sp->initVelMin.x = inv * (x1 - x0);
				sp->initVelMin.y = inv * (y1 - y0);
				sp->initVelMin.z = inv * (z1 - z0);
				sp->initVelMax = sp->initVelMin;
				FXA_Brake(sp, FXA_Rand(0.049999997019767761, 0.20000000298023224));
				sp->sizeIni = FXA_Rand(20.0, 115.0);
				sp->sizeLife = 1.0f;
				sp->sizeEnd = FXA_Rand(10.0, 165.0);
				sp->lifeTime = lt;
				sp->rotIni = sp->rotEnd = FXA_Rand(6.2831854820251465, 0.0);
				sp->spriteIni = 0x33;
				sp->spriteEnd = 0x33;
				sp->rotVar = FXA_Rand(1.0471975803375244, -0.52359879016876221);
				CGameParticles_Apply(sp);
				CGameParticles_SetColorRampOn(sp, 1);
				CGameParticles_SetColorRamp(sp, cObjElementFXPoliceGearConversion_StartRamps + crt_rand() % 3 * 9);
				CGameParticles_EmitDt(sp, 1, 1, FXA_Rand(0.019999999552965164, 0.0));
			}
		}
	} else if (CGameTrail_IsRender(&self->trails[0])) {
		FXA_ClearTrails(self->trails, FXGEAR_TRAILS);
		CGameParticles_KillRange(sp, 0xfb, 0xff);
	}
	if (!CGameModel_IsVisible(m)) {
		return;
	}
	/* the smoke ring over the head */
	if (FX_InRange(self->time, 2.0f, 3.0f, &a)) {
		float st = self->smokeT + delta;
		self->smokeT = st;
		if (st > 0.03f) {
			self->smokeT = 0.0f;
			const vec3_t *c = Obj_Vtbl(self->ent)->GetViewPos(self->ent, &tmp);
			float bx = c->x + 0.0f, by = c->y + 180.0f, bz = c->z + 0.0f;
			for (int n = 0; n < 6; n++) {
				FX_RandomDir(&dir);
				float k = FXA_Rand(30.0, 20.0);
				FXA_StillAt(sp, k * dir.x + bx, k * dir.y + by, k * dir.z + bz);
				sp->sizeIni = FXA_Rand(20.0, 45.0);
				sp->sizeEnd = FXA_Rand(10.0, 135.0);
				sp->sizeLife = 0.3f;
				sp->lifeTime = FXA_Rand(0.049999982118606567, 0.40000000596046448);
				sp->rotIni = sp->rotEnd = FXA_Rand(6.2831854820251465, 0.0);
				sp->rotVar = FXA_Rand(1.0471975803375244, -0.52359879016876221);
				byte sprite = (crt_rand() & 1) ? 5 : 2;
				sp->spriteIni = sprite;
				sp->spriteEnd = sprite;
				CGameParticles_Apply(sp);
				CGameParticles_SetColorRampOn(sp, 1);
				CGameParticles_SetColorRamp(sp, cObjElementFXPoliceGearConversion_StartRamps + crt_rand() % 3 * 9);
				CGameParticles_EmitDt(sp, 1, 0, FXA_Rand(0.10000000894069672, 0.05000000074505806));
			}
		}
	}
	/* the pieces of the armors fall off at their times: sparks at 6 vertices of each, a flash in their middle */
	if (FX_InRange(self->time, 2.8f, 3.8f, &a)) {
		for (int i = 0; i < FXGEAR_ARMORS; i++) {
			CGameModel *am = &self->armor[i];
			for (int k = 0; k < self->numPieces[i]; k++) {
				int node = self->piece[i][k];
				if (node == -1 || self->pieceOff[i][k]) {
					continue;
				}
				float pt = self->pieceTime[i][k];
				if (!(self->time >= pt) || !(pt >= self->lastTime)) {
					continue;
				}
				CGameModel_Vertices(am, verts, NULL, NULL, 6, -1, NULL, (uint32_t)node, NULL);
				sp->birthRate = 0;
				CGameParticles_SetColorRampOn(sp, 1);
				vec3_t c = verts[0];
				float rad = 0.0f;
				for (int j = 0; j < 6; j++) {
					const vec3_t *vv = &verts[j];
					CGameParticles_SetColorRamp(sp, cObjElementFXPoliceGearConversion_StartRamps + (crt_rand() & 1) * 9);
					sp->initPosMin = *vv;
					sp->initPosMax = sp->initPosMin;
					float kv = FXA_Rand(15.0, 15.0);
					FX_RandomDir(&dir);
					sp->initVelMin.x = dir.x * kv;
					sp->initVelMin.y = dir.y * kv;
					sp->initVelMin.z = dir.z * kv;
					sp->initVelMax = sp->initVelMin;
					FXA_Brake(sp, FXA_Rand(0.099999994039535522, 0.34999999403953552));
					sp->sizeIni = FXA_Rand(30.0, 45.0);
					sp->sizeEnd = FXA_Rand(25.0, 155.0);
					sp->sizeLife = 0.35f;
					sp->lifeTime = FXA_Rand(0.099999964237213135, 0.85000002384185791);
					switch (crt_rand() % 4) {
					case 0:
						sp->spriteIni = (byte)((crt_rand() & 1) + 9);
						break;
					case 1:
						sp->spriteIni = 0x36;
						break;
					case 2:
						sp->spriteIni = 5;
						break;
					case 3:
						sp->spriteIni = 2;
						break;
					}
					sp->spriteEnd = sp->spriteIni;
					sp->rotIni = sp->rotEnd = FXA_Rand(6.2831854820251465, 0.0);
					sp->rotVar = FXA_Rand(2.0943951606750488, -1.0471975803375244);
					CGameParticles_Apply(sp);
					CGameParticles_EmitN(sp, 1, 0);
					/* the middle of the vertices so far and their spread */
					float nx = c.x + (vv->x - c.x) * 0.5f;
					float ny = c.y + (vv->y - c.y) * 0.5f;
					float nz = c.z + (vv->z - c.z) * 0.5f;
					float dy = vv->y - ny, dx = vv->x - nx, dz = vv->z - nz;
					c = (vec3_t){ nx, ny, nz };
					float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
					rad = (len + rad) * 0.5f;
				}
				CGameParticles_SetColorRamp(sp, cObjElementFXPoliceGearConversion_StartRamp);
				FXA_StillAt(sp, c.x, c.y, c.z);
				sp->sizeIni = FXA_Rand(0.19999998807907104, 0.69999998807907104) * rad;
				sp->sizeEnd = FXA_Rand(1.5, 7.5) * rad;
				sp->sizeLife = 0.25f;
				sp->spriteIni = 5;
				sp->spriteEnd = 5;
				sp->lifeTime = FXA_Rand(0.02000001072883606, 0.25999999046325684);
				sp->rotIni = sp->rotEnd = FXA_Rand(6.2831854820251465, 0.0);
				sp->rotVar = FXA_Rand(1.5707963705062866, -0.78539818525314331);
				CGameParticles_Apply(sp);
				CGameParticles_EmitN(sp, 1, 0);
				self->pieceOff[i][k] = 1;
			}
		}
	}
	self->lastTime = self->time;
}
void cObjElementFXMessengerTakeDataPack_Start(cObjElementFXMessengerTakeDataPack *self) {
	vec3_t ang, t1, t2, o1, o2, p0, d;

	CHECK(CGameModel_IsLoaded(Obj_Vtbl(self->owner)->GetModel(self->owner)));
	CHECK(CGameModel_IsLoaded(Obj_Vtbl(self->item)->GetModel(self->item)));
	for (int i = 0; i < FXDATAPACK_TRAILS; i++) {
		ang = (vec3_t){ 0.0f, (float)i * 26.0f - 50.0f, 0.0f };
		const vec3_t *a = Vec3_Rotate(&o1, &ang, Obj_Vtbl(self->owner)->GetViewAng(self->owner, &t1));
		const vec3_t *c = Obj_Vtbl(self->owner)->GetViewPos(self->owner, &t2);
		p0.x = c->x + a->x;
		p0.y = c->y + a->y;
		p0.z = c->z + a->z;
		ang = (vec3_t){ 0.0f, (float)i * 33.0f - 50.0f, 0.0f };
		const vec3_t *b = Vec3_Rotate(&o2, &ang, Obj_Vtbl(self->item)->GetViewAng(self->item, &t1));
		c = Obj_Vtbl(self->item)->GetViewPos(self->item, &t2);
		d.x = (b->x + c->x) - p0.x;
		d.y = (c->y + b->y) - p0.y;
		d.z = (c->z + b->z) - p0.z;
		self->width[i] = FXA_Rand((double)0.6f, 1.0);
		d.x = d.x * 0.5f;
		d.y = d.y * 0.5f;
		d.z = d.z * 0.5f;
		for (int k = 0; k < FXDATAPACK_POINTS; k++) {
			vec3_t *p = &self->points[i][k];
			p->x = d.x * (float)k + p0.x;
			p->y = d.y * (float)k + p0.y;
			p->z = d.z * (float)k + p0.z;
			if (k == 1) {
				FXA_Jitter(p, FXA_Rand(20.0, 25.0));
				if (crt_rand() % 4 == 0) {
					FXA_Jitter(p, FXA_Rand(50.0, 35.0));
				}
			}
		}
	}
}

void cObjElementFXMessengerAttack_SetLine(cObjElementFXMessengerAttack *self, float fx, float fy, float fz,
	float tx, float ty, float tz) {
	vec3_t coefs[FXMESSENGER_POINTS];
	vec3_t p, q, d;
	CCollisionTri tri;
	int i, k;

	self->from = (vec3_t){ fx, fy, fz };
	self->to = (vec3_t){ tx, ty, tz };
	self->dir = FXA_Dir(&self->from, &self->to);
	d.x = self->to.x - self->from.x;
	d.y = self->to.y - self->from.y;
	d.z = self->to.z - self->from.z;
	self->width[0] = FXA_Rand((double)0.5999999f, (double)2.2f);
	d.x = d.x * 0.111111112f;
	d.y = d.y * 0.111111112f;
	d.z = d.z * 0.111111112f;
	for (k = 0; k < FXMESSENGER_POINTS; k++) {
		vec3_t *r = &self->lines[0][k];
		r->x = d.x * (float)k + self->from.x;
		r->y = d.y * (float)k + self->from.y;
		r->z = d.z * (float)k + self->from.z;
		if ((unsigned)(k - 1) <= 7) {
			FXA_Jitter(r, FXA_Rand(20.0, 25.0));
			if (crt_rand() % 4 == 0) {
				FXA_Jitter(r, FXA_Rand(80.0, 45.0));
			}
		}
	}
	CGameTrail_BezierCoefs(&self->trails[0], FXMESSENGER_POINTS, self->lines[0], coefs);
	for (i = 0; i < FXMESSENGER_LINES; i++) {
		self->width[1 + i] = FXA_Rand((double)0.39999998f, (double)1.1f);
		CGameTrail_BezierPoint(&self->trails[0], FXMESSENGER_POINTS, coefs, &p,
			FXA_Rand((double)0.84999996f, (double)0.1f));
		float r = FXA_Rand(180.0, 175.0);
		vec3_t tmp;
		const vec3_t *dir = FX_RandomDir(&tmp);
		q.x = p.x + r * dir->x;
		q.y = p.y + dir->y * r;
		q.z = p.z + dir->z * r;
		Engine_TestLine(&p, &q, &q, &tri);
		d.x = (q.x - p.x) * 0.111111112f;
		d.y = (q.y - p.y) * 0.111111112f;
		d.z = (q.z - p.z) * 0.111111112f;
		for (k = 0; k < FXMESSENGER_POINTS; k++) {
			vec3_t *l = &self->lines[1 + i][k];
			l->x = p.x + d.x * (float)k;
			l->y = p.y + d.y * (float)k;
			l->z = p.z + d.z * (float)k;
			if ((unsigned)(k - 1) <= 7) {
				FXA_Jitter(l, FXA_Rand(10.0, 15.0));
				if (crt_rand() % 4 == 0) {
					FXA_Jitter(l, FXA_Rand(80.0, 65.0));
				}
			}
		}
	}
}

void cObjElementFXPoliceGearConversion_Start(cObjElementFXPoliceGearConversion *self, cEntity *e) {
	CGameParticles *sp = &self->particles;
	vec3_t t, v, ang, out, base;
	int i;

	self->ent = e;
	CGameModel *m = FXA_Model(e);
	self->nodeRoot = CGameModel_FindRenderNode(m, "R_Police-NoArmr_tronco-inferior");
	CHECK(self->nodeRoot > -1);
	for (i = 0; i < FXGEAR_ARMORS; i++) {
		CGameModel *a = &self->armor[i];
		CGameModel_SetPos(a, CGameModel_GetPos(Obj_Vtbl(self->ent)->GetModel(self->ent), &t));
		CGameModel_SetRot(a, CGameModel_GetRot(Obj_Vtbl(self->ent)->GetModel(self->ent), &t));
		CGameModel_SetSortZ(a, (float)(FXGEAR_ARMORS - i) / 10.0f - 76.0f, 0.0f);
		CGameModel_Enable(a);
		CGameModel_SetAnimSpeed(a, self->anim[i], 0.0f);
		CGameModel_SetAnimLoop(a, self->anim[i], 0);
		CGameModel_SetAnimFactor(a, self->anim[i], 1.0f);
		CGameModel_SetAnim(a, self->anim[i], 0, 0);
		CGameModel_SetEffect(a, 3);
		CGameModel_SetEffectParam(a, 0, 0x3f800000);
	}
	if (!CGameModel_IsVisible(m)) {
		return;
	}
	float h = WChar(self->ent)->floorDist;
	const vec3_t *c = Obj_Vtbl(self->ent)->GetViewPos(self->ent, &t);
	base.x = (c->x - 0.0f) + 0.0f;
	base.y = (c->y - h) + 15.0f;
	base.z = (c->z - 0.0f) + 0.0f;
	/* the smoke ring */
	for (i = 0; i < 6; i++) {
		ang = (vec3_t){ 0.0f, FXA_Rand((double)6.28318548f, 0.0), 0.0f };
		v = (vec3_t){ 1.0f, 0.0f, 0.0f };
		Vec3_Rotate(&out, &v, &ang);
		float r = FXA_Rand(35.0, 10.0);
		sp->initPosMin.x = base.x + out.x * r;
		sp->initPosMin.y = base.y + out.y * r;
		sp->initPosMin.z = base.z + out.z * r;
		sp->initPosMax = sp->initPosMin;
		sp->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		sp->initVelMax = sp->initVelMin;
		sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		sp->initAccMax = sp->initAccMin;
		sp->sizeIni = FXA_Rand(20.0, 45.0);
		sp->sizeEnd = FXA_Rand(110.0, 325.0);
		float life = FXA_Rand((double)0.050000012f, 0.25);
		sp->sizeLife = 0.2f;
		sp->lifeTime = life;
		sp->rotIni = sp->rotEnd = FXA_Rand((double)6.28318548f, 0.0);
		float rv = (float)(FXA_Rnd() * (double)0.5235988f - (double)0.2617994f);
		sp->spriteIni = 6;
		sp->spriteEnd = 6;
		sp->rotVar = rv;
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, cObjElementFXPoliceGearConversion_StartRamps + crt_rand() % 3 * 9);
		CGameParticles_EmitDt(sp, 1, 1, FXA_Rand((double)0.02f, 0.0) + 0.6f);
	}
	/* the sparks around */
	float a = 0.0f;
	do {
		ang = (vec3_t){ 0.0f, a, 0.0f };
		v = (vec3_t){ 1.0f, 0.0f, 0.0f };
		Vec3_Rotate(&out, &v, &ang);
		float r = FXA_Rand(10.0, 10.0);
		sp->initPosMin.x = base.x + out.x * r;
		sp->initPosMin.y = base.y + out.y * r;
		sp->initPosMin.z = base.z + out.z * r;
		sp->initPosMax = sp->initPosMin;
		float s = FXA_Rand(50.0, 725.0);
		sp->initVelMin.x = out.x * s;
		sp->initVelMin.y = out.y * s;
		sp->initVelMin.z = out.z * s;
		sp->initVelMax = sp->initVelMin;
		float up = FXA_Rand(60.0, 125.0);
		float k = FXA_Rand((double)0.019999996f, (double)0.2f);
		sp->initAccMin.y = -sp->initVelMin.y * k + up;
		sp->initAccMin.x = -sp->initVelMin.x * k + 0.0f;
		sp->initAccMin.z = -sp->initVelMin.z * k + 0.0f;
		sp->initAccMax = sp->initAccMin;
		sp->sizeIni = FXA_Rand(10.0, 35.0);
		sp->sizeEnd = FXA_Rand(10.0, 345.0);
		float life = FXA_Rand((double)0.030000031f, (double)0.53f);
		sp->sizeLife = 1.0f;
		sp->lifeTime = life;
		sp->rotIni = sp->rotEnd = FXA_Rand((double)6.28318548f, 0.0);
		float rv = (float)(FXA_Rnd() * (double)0.5235988f - (double)0.2617994f);
		sp->spriteIni = 2;
		sp->spriteEnd = 2;
		sp->rotVar = rv;
		CGameParticles_Apply(sp);
		CGameParticles_SetColorRampOn(sp, 1);
		CGameParticles_SetColorRamp(sp, cObjElementFXPoliceGearConversion_StartRamps + (crt_rand() & 1) * 9);
		CGameParticles_EmitDt(sp, 1, 1, FXA_Rand((double)0.01f, (double)0.01f) + 0.6f);
		a = a + 0.157079637f;
	} while (6.28318548f > a);
}

/* ---- Update (vf01) ---- */

static inline float FXA_Delta(void) {
	return cTimer_GetDelta(&g_pWorld->timer);
}

/* an entity that is deleted or off is let go */
static inline void FXA_Keep(cEntity **e) {
	if (*e && (Entity_Removed(*e) || !(*e)->active)) {
		*e = NULL;
	}
}

/* the debris at the model of the character, seen when it is */
static void FXA_FollowModel(cDebris *d, CGameModel *m, vec3_t *pos, vec3_t *rot, float sortBias) {
	CGameModel_GetPos(m, pos);
	CGameModel_GetRot(m, rot);
	CGameModel_SetPos(Debris_Model(d), pos);
	CGameModel_SetRot(Debris_Model(d), rot);
	CGameModel_SetSortZ(Debris_Model(d), sortBias, 0.0f);
	if (CGameModel_IsVisible(m)) {
		CGameModel_Enable(Debris_Model(d));
	} else {
		CGameModel_Disable(Debris_Model(d));
	}
}

/* the beams of BettyKill, DTritusKill and DTritusAttack: n points of the model (in its space, turned by rot and moved
 * to pos), each a beam of length len (moving to lenTarget plus a random -75..75 by step) turned by beamRot, 15 beams a
 * trail; the alpha fades in over 0..fadeIn of the time, or out at the start (0..fadeOut) */
typedef struct FXBeams {
	CGameParticles *sp;
	CGameTrail *trails;
	int numTrails;
	int n;
	const vec3_t *point;
	float *len;
	const float *lenTarget;
	const float *width;
	const vec3_t *beamRot;
	float step;
	float fadeIn;
	float fadeOut;
	uint32_t inner;
	uint32_t outer;
	uint32_t rgb;
} FXBeams;

static void FXBeams_Draw(const FXBeams *b, float time, float dt, const vec3_t *pos, const vec3_t *rot) {
	vec3_t o1, o2, v, w, q;
	float f1, f2;

	FX_InRange(time, 0.0f, b->fadeIn, &f1);
	float s = (3.0f - f1 * 2.0f) * (f1 * f1);
	int alpha = (int)((3.0f - (s + s)) * (s * s) * 255.0f);
	if (FX_InRange(time, 0.0f, b->fadeOut, &f2)) {
		alpha = 0xff - (int)(f2 * 255.0f);
	}
	for (int k = 0; k < b->numTrails; k++) {
		CGameTrail_Reset(&b->trails[k]);
		CGameTrail_SetTransp(&b->trails[k], (byte)alpha);
	}
	CGameParticles *sp = b->sp;
	sp->birthRate = 0;
	CGameParticles_Apply(sp);
	int t = 0, cnt = 0;
	for (int i = 0; i < b->n; i++) {
		if (t >= b->numTrails) {
			continue;
		}
		float r = (float)(FXA_Rnd() * 150.0 - 75.0);
		Float_Approach(&b->len[i], b->lenTarget[i] + r, dt * b->step);
		const vec3_t *a = Vec3_Rotate(&o1, &b->point[i], rot);
		w.x = a->x + pos->x;
		w.y = a->y + pos->y;
		w.z = a->z + pos->z;
		v = (vec3_t){ 0.0f, 0.0f, b->len[i] };
		const vec3_t *e = Vec3_Rotate(&o2, &v, b->beamRot ? b->beamRot : rot);
		q.x = e->x + w.x;
		q.y = e->y + w.y;
		q.z = e->z + w.z;
		float width = b->width[i];
		CGameTrail_Add(&b->trails[t], w, width, b->inner);
		CGameTrail_Add(&b->trails[t], q, width, b->outer);
		CGameParticles_SetParticle(sp, i, w.x, w.y, w.z, width * 1.5f, (uint32_t)alpha << 24 | b->rgb, 2, 0.0f);
		if (++cnt >= 15) {
			cnt = 0;
			t++;
		}
	}
}

/* BettyKill and DTritusKill: the sort bias, the end of the fade out and the colors */
static void FXKill_Update(cObjElementFXKill *self, cDebris *d, float sortBias, float fadeOut, uint32_t inner,
	uint32_t outer, uint32_t rgb) {
	vec3_t pos, rot;

	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->active) {
		d->life = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	FXA_FollowModel(d, m, &pos, &rot, sortBias);
	float dt = FXA_Delta();
	self->time = self->time + dt;
	self->emitTime = self->emitTime + dt;
	FXBeams b = { &self->particles, self->trails, FXKILL_TRAILS, FXKILL_POINTS, self->point, self->len, self->lenTarget,
		self->scale, &self->angles, 12000.0f, 0.84f, fadeOut, inner, outer, rgb };
	FXBeams_Draw(&b, self->time, dt, &pos, &rot);
}

static void cObjElementFXBettyKill_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXBettyKill *self = (cObjElementFXBettyKill *)base;
	(void)dt;
	FXKill_Update(self, d, -88.0f, 0.28f, 0x40f54019, 0xffe1190a, 0xf54019);
}

static void cObjElementFXDTritusKill_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXDTritusKill *self = (cObjElementFXDTritusKill *)base;
	(void)dt;
	FXKill_Update(self, d, -77.0f, 0.02f, 0x401940f5, 0xff0a19e1, 0x1940f5);
}

/* the swing of the baton while the action runs, fading out in 0.5 s after it */
static void cObjElementFXBankMasterAttack_Update(cObjElementFX *elem, cDebris *d, float dt) {
	cObjElementFXBankMasterAttack *self = (cObjElementFXBankMasterAttack *)elem;
	vec3_t pos, rot, base, v, ang, o1, o2, p;
	matrix_t mtx;

	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->active) {
		d->life = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	const matrix_t *mm = CGameModel_NodeMatrix(m, &mtx, (uint32_t)self->nodeBaton, 1, NULL, 0);
	base.x = mm->m[12];
	base.y = mm->m[13];
	base.z = mm->m[14];
	CGameModel_SetPos(Debris_Model(d), &base);
	CGameModel_SetRot(Debris_Model(d), &rot);
	float delta = FXA_Delta();
	self->time = delta + self->time;
	self->emitTime = delta + self->emitTime;
	float layer = 0.0f;
	void *ctrl = WithLife_Vtbl(self->ent)->GetActCtrl(self->ent);
	byte action = 0;
	if (ctrl) {
		action = cActCtrl_IsActive(ctrl, "Action");
		layer = cActCtrl_GetLayer(ctrl, 0)->time / 1.33f;
	}
	if (!action) {
		self->t = dt + self->t;
	}
	if (self->t > 0.5f) {
		CGameTrail_Clear(&self->trail);
		CGameTrail_Clear(&self->trail2);
		return;
	}
	float f = 1.0f - (self->t + self->t);
	uint32_t c = (uint32_t)(int)(f * 255.0f);
	uint32_t blue = (uint32_t)(int)(f * 106.0f);
	uint32_t rgb = ((c << 8 | c) << 8 | blue) & 0xffffff;
	uint32_t color = rgb | 0x5c000000;
	CGameTrail_Reset(&self->trail);
	CGameTrail_Add(&self->trail, base, 1.0f, color);
	CGameTrail_Reset(&self->trail2);
	layer = layer * 25.1327419f;
	for (int i = 0; i < 14; i++) {
		float q = (float)i / 13.0f;
		ang = (vec3_t){ 0.0f, 0.0f, q * 2.51327419f - layer };
		v = (vec3_t){ 0.0f, 67.0f, 0.0f };
		const vec3_t *r = Vec3_Rotate(&o2, Vec3_Rotate(&o1, &v, &ang), &rot);
		p.x = r->x + base.x;
		p.y = r->y + base.y;
		p.z = r->z + base.z;
		uint32_t pc = (uint32_t)(0x20 - (int)(q * -223.0f)) << 24 | rgb;
		CGameTrail_Add(&self->trail, p, 1.0f, pc);
		CGameTrail_Add(&self->trail2, p, 6.0f, pc);
	}
	CGameTrail_Add(&self->trail, base, 1.0f, color);
}

/* the particles on 32 vertices of the crystal of Berto */
static void cObjElementFXBertoAttack_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXBertoAttack *self = (cObjElementFXBertoAttack *)base;
	vec3_t pos, rot, avg;
	vec3_t pts[32];
	float f;

	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->active) {
		d->life = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -81.0f, 0.0f);
	float delta = FXA_Delta();
	self->time = delta + self->time;
	self->emitTime = delta + self->emitTime;
	int alpha = 0;
	if (FX_InRange(self->time, 0.0f, 0.6f, &f)) {
		alpha = 0xff - (int)(f * 255.0f);
	}
	if (0.95f > d->life) {
		FX_InRange(d->life - dt, 0.0f, 1.0f, &f);
		alpha = 0xff - (int)(f * 255.0f);
	}
	CGameParticles *sp = &self->particles;
	sp->birthRate = 0;
	CGameParticles_Apply(sp);
	CGameModel_Vertices(m, pts, NULL, NULL, 32, -1, NULL, (uint32_t)self->nodeGlass, NULL);
	uint32_t color = (uint32_t)alpha << 24 | 0xc08040;
	avg = pts[0];
	for (int i = 0; i < 32; i++) {
		CGameParticles_SetParticle(sp, i, pts[i].x, pts[i].y, pts[i].z, FXA_Rand(10.0, 35.0) * 1.5f, color, 2, 0.0f);
		avg.z = (pts[i].z - avg.z) * 0.5f + avg.z;
		avg.x = (pts[i].x - avg.x) * 0.5f + avg.x;
		avg.y = (pts[i].y - avg.y) * 0.5f + avg.y;
	}
	CGameParticles_SetParticle(sp, 32, avg.x, avg.y, avg.z, FXA_Rand(100.0, 250.0), color, 5, 0.0f);
	CGameParticles_SetParticle(sp, 33, avg.x, avg.y, avg.z, FXA_Rand(50.0, 150.0), color, 2, 0.0f);
}

/* the beams from the vertices of the character, fading in, and the particles at their roots */
static void cObjElementFXDTritusAttack_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXDTritusAttack *self = (cObjElementFXDTritusAttack *)base;
	vec3_t pos, rot;

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->active) {
		d->life = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -78.0f, 0.0f);
	float delta = FXA_Delta();
	self->time = delta + self->time;
	self->emitTime = delta + self->emitTime;
	CGameModel_Vertices(m, &pos, NULL, NULL, 1, 0, NULL, (uint32_t)self->nodeMaster, NULL);
	FXBeams b = { &self->particles, self->trails, FXDTRITUSATTACK_TRAILS, FXDTRITUSATTACK_POINTS, self->point,
		self->len, self->lenTarget, self->width, NULL, 2000.0f, 0.72f, 0.1f, 0x40f54019, 0xffe1190a, 0xf54019 };
	FXBeams_Draw(&b, self->time, delta, &pos, &rot);
}

#define FXA_RAMP_POLICESTEAL 0x4307ac

static uint32_t cObjElementFXBankerSteal_UpdateRamp[9] = { 0xff000000, 0x80808080, 0xc4c4c4, 0xc4c4c4, 0xc4c4c4, 0xc4c4c4, 0x80808080, 0xff000000, 0xff000000 };

/* the coins (particles) from the target to the police every 0.04 s */
static void cObjElementFXPoliceSteal_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXPoliceSteal *self = (cObjElementFXPoliceSteal *)base;
	vec3_t tmp, a, b, v, ang, dir;

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->active) {
		return;
	}
	FXA_Keep(&self->target);
	if (!self->target || !self->target->active) {
		return;
	}
	if (0.65f > d->life) {
		return;
	}
	CGameModel_SetPos(Debris_Model(d), Obj_Vtbl(self->ent)->GetViewPos(self->ent, &tmp));
	CGameModel_SetRot(Debris_Model(d), Obj_Vtbl(self->ent)->GetViewAng(self->ent, &tmp));
	float t = self->emitTime + FXA_Delta();
	self->emitTime = t;
	if (0.04f > t) {
		return;
	}
	self->emitTime = 0.0f;
	Obj_Vtbl(self->target)->GetViewPos(self->target, &b);
	Obj_Vtbl(self->ent)->GetViewPos(self->ent, &a);
	float az = FXA_Rand((double)6.28318548f, 0.0);
	float ay = FXA_Rand((double)6.28318548f, 0.0);
	v = (vec3_t){ 1.0f, 1.0f, 1.0f };
	ang.x = FXA_Rand((double)6.28318548f, 0.0);
	ang.y = ay;
	ang.z = az;
	Vec3_Rotate(&dir, &v, &ang);
	float life = FXA_Rand((double)0.19999999f, (double)0.45f);
	CGameParticles *sp = &self->particles;
	CGameParticles_SetSparksOn(sp, 0);
	sp->birthRate = 0;
	float hi = Obj_Vtbl(self->target)->GetRadius(self->target) * 0.5f;
	float lo = Obj_Vtbl(self->target)->GetRadius(self->target) * 0.25f;
	float r = Rand_FloatRange(lo, hi);
	sp->initPosMin.x = b.x + dir.x * r;
	sp->initPosMin.y = b.y + dir.y * r;
	sp->initPosMin.z = b.z + dir.z * r;
	sp->initPosMax = sp->initPosMin;
	hi = Obj_Vtbl(self->ent)->GetRadius(self->ent) * 0.5f;
	lo = Obj_Vtbl(self->ent)->GetRadius(self->ent) * 0.25f;
	r = Rand_FloatRange(lo, hi);
	float inv = 1.0f / life;
	sp->initVelMin.x = ((a.x + dir.x * r) - sp->initPosMin.x) * inv;
	sp->initVelMin.y = ((a.y + dir.y * r) - sp->initPosMin.y) * inv;
	sp->initVelMin.z = ((a.z + dir.z * r) - sp->initPosMin.z) * inv;
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initAccMax = sp->initAccMin;
	sp->lifeTime = life;
	sp->sizeLife = 1.0f;
	sp->sizeIni = sp->sizeEnd = FXA_Rand(5.0, 35.0);
	sp->rotIni = sp->rotEnd = FXA_Rand((double)6.28318548f, 0.0);
	sp->rotVar = (float)(FXA_Rnd() * (double)0.785398185f - (double)0.392699093f);
	byte sprite = (byte)(crt_rand() % 7 + 1);
	sp->spriteIni = sprite;
	sp->spriteEnd = sprite + 8;
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, cObjElementFXBankerSteal_UpdateRamp);
	CGameParticles_EmitN(sp, 1, 0);
}

/* the flash: random trails up from the character, in, out after 1.3 s, gone at 1.5 s */
static void cObjElementFXSebastianFlash_Update(cObjElementFX *elem, cDebris *d, float dt) {
	cObjElementFXSebastianFlash *self = (cObjElementFXSebastianFlash *)elem;
	vec3_t pos, rot, tmp, D, p1, p2, e, base;
	float f;
	int alpha;

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->active) {
		d->life = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -82.0f, 0.0f);
	float delta = FXA_Delta();
	float time = delta + self->time;
	self->emitTime = delta + self->emitTime;
	self->time = time;
	if (0.1f > time) {
		FX_InRange(time, 0.0f, 0.1f, &f);
		float g = 1.0f - f;
		alpha = (int)(g * g * (2.0f - g) * 255.0f);
	} else if (1.3f > time) {
		alpha = 0;
	} else if (1.5f > time) {
		FX_InRange(time, 1.3f, 1.5f, &f);
		alpha = (int)((3.0f - f * 2.0f) * (f * f) * 255.0f);
	} else {
		d->life = 0.0f;
		return;
	}
	for (int k = 0; k < FXSEBASTIAN_TRAILS; k++) {
		CGameTrail_Reset(&self->trails[k]);
		CGameTrail_SetTransp(&self->trails[k], (byte)alpha);
	}
	CGameParticles *sp = &self->particles;
	sp->birthRate = 0;
	CGameParticles_Apply(sp);
	const vec3_t *c = Obj_Vtbl(self->ent)->GetViewPos(self->ent, &tmp);
	base.x = c->x + 0.0f;
	base.y = c->y + 18.0f;
	base.z = c->z + 0.0f;
	int t = 0, cnt = 0;
	for (int i = 0; i < FXSEBASTIAN_POINTS; i++) {
		if (t >= FXSEBASTIAN_TRAILS) {
			continue;
		}
		D = *FX_RandomDir(&tmp);
		float r1 = (float)(FXA_Rnd() + 0.0);
		p1.x = (self->dir[i].x + base.x) + r1 * D.x;
		p1.y = (self->dir[i].y + base.y) + r1 * D.y;
		p1.z = (base.z + self->dir[i].z) + r1 * D.z;
		double rr = FXA_Rnd();
		float r2 = (float)(rr + rr + 0.0);
		e.x = r2 * D.x;
		e.y = r2 * D.y;
		e.z = r2 * D.z;
		float up = FXA_Rand(80.0, 140.0);
		p2.y = self->dir2[i].y + (up + base.y) + e.y;
		p2.x = self->dir2[i].x + (base.x + 0.0f) + e.x;
		p2.z = self->dir2[i].z + (base.z + 0.0f) + e.z;
		uint32_t a = (uint32_t)(crt_rand() % 32 + 0xc4);
		rr = FXA_Rnd();
		float w = (float)(rr + rr - 1.0) + 12.0f;
		uint32_t g = (uint32_t)(crt_rand() % 20 + 0x7d) << 8;
		uint32_t rb = (uint32_t)(crt_rand() % 10 + 0x96) << 16 | g;
		CGameTrail_Add(&self->trails[t], p1, w, (uint32_t)(crt_rand() % 30 + 0x3c) | a << 24 | rb);
		uint32_t r = ((uint32_t)(crt_rand() % 10 - 0x6a) | 0xffffff00u) << 16;
		r = (uint32_t)(crt_rand() % 20 + 0x7d) << 8 | r;
		CGameTrail_Add(&self->trails[t], p2, w * 4.0f, (uint32_t)(crt_rand() % 30 + 0x3c) | r);
		if (++cnt >= 15) {
			cnt = 0;
			t++;
		}
	}
	uint32_t color = (uint32_t)alpha << 24 | 0xc08040;
	CGameParticles_SetParticle(sp, 0, base.x, base.y, base.z, FXA_Rand(50.0, 250.0), color, 5, 0.0f);
	CGameParticles_SetParticle(sp, 1, base.x, base.y, base.z, FXA_Rand(50.0, 100.0), color, 2, 0.0f);
}

/* the flash of Berto: trails from Berto to the target, in 0.2 s, out until 0.7 s */
static void cObjElementFXBertoFlash_Update(cObjElementFX *elem, cDebris *d, float dt) {
	cObjElementFXBertoFlash *self = (cObjElementFXBertoFlash *)elem;
	vec3_t pos, rot, tmp, tmp2, D, p1, p2, e, base;
	float f, v;

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent) {
		d->life = 0.0f;
		return;
	}
	FXA_Keep(&self->target);
	if (!self->target) {
		d->life = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -82.0f, 0.0f);
	float delta = FXA_Delta();
	self->time = self->time + delta;
	self->emitTime = self->emitTime + delta;
	if (FX_InRange(self->time, 0.0f, 0.2f, &f)) {
		float g = 1.0f - f;
		v = g * g * (2.0f - g);
	} else if (FX_InRange(self->time, 0.2f, 0.7f, &f)) {
		v = (3.0f - f * 2.0f) * (f * f);
	} else {
		d->life = 0.0f;
		return;
	}
	int alpha = (int)(v * 255.0f);
	for (int k = 0; k < FXBERTOFLASH_TRAILS; k++) {
		CGameTrail_Reset(&self->trails[k]);
		CGameTrail_SetTransp(&self->trails[k], (byte)alpha);
	}
	CGameParticles *sp = &self->particles;
	sp->birthRate = 0;
	CGameParticles_Apply(sp);
	const vec3_t *c = Obj_Vtbl(self->ent)->GetViewPos(self->ent, &tmp);
	base.x = c->x - 0.0f;
	base.y = c->y - 15.0f;
	base.z = c->z - 0.0f;
	int t = 0, cnt = 0;
	for (int i = 0; i < FXBERTOFLASH_POINTS; i++) {
		if (t >= FXBERTOFLASH_TRAILS) {
			continue;
		}
		D = *FX_RandomDir(&tmp);
		float r = FXA_Rand(3.0, 3.0);
		p1.x = (self->dir[i].x + base.x) + r * D.x;
		p1.y = (self->dir[i].y + base.y) + r * D.y;
		p1.z = (base.z + self->dir[i].z) + r * D.z;
		float r2 = FXA_Rand(4.0, 4.0);
		e.x = r2 * D.x;
		e.y = r2 * D.y;
		e.z = r2 * D.z;
		const vec3_t *c2 = Obj_Vtbl(self->target)->GetViewPos(self->target, &tmp2);
		p2.x = (self->dir2[i].x + c2->x) + e.x;
		p2.y = (c2->y + self->dir2[i].y) + e.y;
		p2.z = (c2->z + self->dir2[i].z) + e.z;
		uint32_t a = (uint32_t)(crt_rand() % 64 + 0x20);
		float w = (float)(FXA_Rnd() * 6.0 - 3.0) + 30.0f;
		CGameTrail_Add(&self->trails[t], p1, w, a << 24 | 0xc08040);
		CGameTrail_Add(&self->trails[t], p2, w + w, 0xffc08040);
		if (++cnt >= 15) {
			cnt = 0;
			t++;
		}
	}
	uint32_t color = (uint32_t)alpha << 24 | 0xc08040;
	CGameParticles_SetParticle(sp, 0, base.x, base.y, base.z, FXA_Rand(50.0, 450.0), color, 5, 0.0f);
	CGameParticles_SetParticle(sp, 1, base.x, base.y, base.z, FXA_Rand(50.0, 300.0), color, 2, 0.0f);
}

/* one of the three steal sounds other than the last one at the position of m (at without m); the new last one */
static int FXA_StealSound(int last, CGameModel *m, const vec3_t *at) {
	vec3_t pos;
	const char *name;

	int s = crt_rand() % 3;
	if (s == last) {
		s = (s + 1) % 3;
	}
	switch (s) {
	case 0:
		name = g_BankerSteal1Sound;
		break;
	case 1:
		name = g_BankerSteal2Sound;
		break;
	case 2:
		name = g_BankerSteal3Sound;
		break;
	default:
		return s;
	}
	const CSoundSysVtbl *vt = g_pSound->vftable;
	int vol = Snd_Volume(g_BankerStealSoundVol);
	vt->PlayAt(g_pSound, name, m ? CGameModel_GetPos(m, &pos) : at, vol);
	return s;
}

/* the coins from the target to the banker, faster when near, a steal sound every second coin */
static void cObjElementFXBankerSteal_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXBankerSteal *self = (cObjElementFXBankerSteal *)base;
	vec3_t tmp, a, b, off, src, tp, v, ang, dir;
	matrix_t mtx;

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->active) {
		return;
	}
	FXA_Keep(&self->target);
	if (!self->target || !self->target->active) {
		return;
	}
	if (0.65f > d->life) {
		return;
	}
	CGameModel *m = Obj_Vtbl(self->ent)->GetModel(self->ent);
	if (!CGameModel_IsLoaded(m)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(m));
	CGameModel *mt = Obj_Vtbl(self->target)->GetModel(self->target);
	if (!CGameModel_IsLoaded(mt)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(mt));
	CGameModel_SetPos(Debris_Model(d), CGameModel_GetPos(m, &tmp));
	CGameModel_SetRot(Debris_Model(d), CGameModel_GetRot(m, &tmp));
	self->time = self->time + FXA_Delta();
	const vec3_t *pa = CGameModel_GetPos(mt, &a);
	const vec3_t *pb = CGameModel_GetPos(m, &b);
	float dy = pb->y - pa->y;
	float dx = pb->x - pa->x;
	float dz = pb->z - pa->z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	float x = (500.0f - dist) / 3000.0f;
	float wait = 0.04f > x ? 0.04f : Float_MinSS(0.15f, x);
	if (wait > self->time) {
		return;
	}
	self->time = 0.0f;
	off.x = g_BankerStealPosX;
	off.y = g_BankerStealPosY;
	off.z = g_BankerStealPosZ;
	const vec3_t *r = Vec3_Rotate(&v, &off, CGameModel_GetRot(m, &tmp));
	const vec3_t *p = CGameModel_GetPos(m, &b);
	src.x = r->x + p->x;
	src.y = p->y + r->y;
	src.z = p->z + r->z;
	CGameModel_GetPos(mt, &tp);
	if (self->node != -1) {
		const matrix_t *mm = CGameModel_NodeMatrix(m, &mtx, (uint32_t)self->node, 1, NULL, 0);
		src.x = mm->m[12];
		src.y = mm->m[13];
		src.z = mm->m[14];
	}
	float az = FXA_Rand((double)6.28318548f, 0.0);
	float ay = FXA_Rand((double)6.28318548f, 0.0);
	ang.x = FXA_Rand((double)6.28318548f, 0.0);
	ang.y = ay;
	ang.z = az;
	v = (vec3_t){ 1.0f, 1.0f, 1.0f };
	Vec3_Rotate(&dir, &v, &ang);
	float life = FXA_Rand((double)0.19999999f, (double)0.45f);
	CGameParticles *sp = &self->particles;
	CGameParticles_SetSparksOn(sp, 0);
	sp->birthRate = 0;
	float hi = Obj_Vtbl(self->target)->GetRadius(self->target) * 0.5f;
	float lo = Obj_Vtbl(self->target)->GetRadius(self->target) * 0.25f;
	float rr = Rand_FloatRange(lo, hi);
	sp->initPosMin.x = tp.x + dir.x * rr;
	sp->initPosMin.z = tp.z + dir.z * rr;
	sp->initPosMin.y = tp.y + dir.y * rr;
	sp->initPosMax = sp->initPosMin;
	float inv = 1.0f / life;
	sp->initVelMin.x = (src.x - sp->initPosMin.x) * inv;
	sp->initVelMin.y = (src.y - sp->initPosMin.y) * inv;
	sp->initVelMin.z = (src.z - sp->initPosMin.z) * inv;
	sp->initVelMax = sp->initVelMin;
	sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initAccMax = sp->initAccMin;
	sp->lifeTime = life;
	sp->sizeLife = 1.0f;
	sp->sizeIni = sp->sizeEnd = FXA_Rand(5.0, 35.0);
	sp->rotIni = sp->rotEnd = FXA_Rand((double)6.28318548f, 0.0);
	sp->rotVar = (float)(FXA_Rnd() * (double)0.785398185f - (double)0.392699093f);
	byte sprite = (byte)(crt_rand() % 7 + 1);
	sp->spriteIni = sprite;
	sp->spriteEnd = sprite + 8;
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, cObjElementFXBankerSteal_UpdateRamp);
	CGameParticles_EmitN(sp, 1, 0);
	self->toggle ^= 1;
	if (self->toggle) {
		self->lastSound = FXA_StealSound(self->lastSound, mt, NULL);
	}
}

/* the money sucked from the target to the baton along a Bezier curve, a steal sound every 8 points */
static void cObjElementFXBankMasterSuction_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXBankMasterSuction *self = (cObjElementFXBankMasterSuction *)base;
	vec3_t pos, rot, off, o, a, t, in, p, b;
	vec3_t pts[4], coefs[4];

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent) {
		d->life = 0.0f;
		return;
	}
	FXA_Keep(&self->target);
	if (!self->target) {
		d->life = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	float delta = FXA_Delta();
	self->time = self->time + delta;
	self->emitTime = self->emitTime + delta;
	float radius = Obj_Vtbl(self->target)->GetRadius(self->target) * 0.5f;
	Obj_Vtbl(self->target)->GetViewPos(self->target, &t);
	off.x = g_BankMasterBatonPosX;
	off.y = g_BankMasterBatonPosY;
	off.z = g_BankMasterBatonPosZ;
	const vec3_t *r = Vec3_Rotate(&o, &off, &rot);
	a.x = pos.x + r->x;
	a.y = r->y + pos.y;
	a.z = r->z + pos.z;
	float dx = a.x - t.x;
	float dy = a.y - t.y;
	float dz = a.z - t.z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	pts[0] = t;
	in = (vec3_t){ 0.0f, 0.0f, dist * 0.5f };
	r = Vec3_Rotate(&o, &in, &rot);
	pts[1].x = a.x - r->x;
	pts[1].y = a.y - r->y;
	pts[1].z = a.z - r->z;
	in = (vec3_t){ 0.0f, 0.0f, dist * 0.25f };
	r = Vec3_Rotate(&o, &in, &rot);
	pts[2].x = a.x - r->x;
	pts[2].y = a.y - r->y;
	pts[2].z = a.z - r->z;
	pts[3] = a;
	CGameTrail_BezierCoefs(&self->trail, 4, pts, coefs);
	float step = delta * 2.0f;
	CGameParticles *sp = &self->particles;
	for (int i = 0; i < FXBMSUCTION_POINTS; i++) {
		float old = self->phase[i];
		float v = step + old;
		self->phase[i] = v;
		if (v > 0.0f && 0.0f >= old) {
			int k = self->count;
			self->count = k + 1;
			if (k > 6) {
				self->count = 0;
				self->lastSound = FXA_StealSound(self->lastSound, NULL, &pos);
			}
		}
		v = self->phase[i];
		if (0.0f > v || v >= 1.0f) {
			CGameParticles_Kill(sp, i);
			continue;
		}
		int alpha = 0;
		if (0.1f >= v) {
			alpha = 0xff - (int)(v * 10.0f * 255.0f);
		} else if (v >= 0.8f) {
			alpha = (int)((v - 0.8f) * 5.0f * 255.0f);
		}
		float size = (45.0f - radius) * v + radius;
		CGameTrail_BezierPoint(&self->trail, 4, coefs, &b, v);
		uint32_t c = (uint32_t)(0xff - alpha);
		p.x = b.x + self->dir[i].x * size;
		p.y = b.y + self->dir[i].y * size;
		p.z = b.z + size * self->dir[i].z;
		int sprite = 1 - (int)(self->phase[i] * -16.0f);
		CGameParticles_SetParticle(sp, i, p.x, p.y, p.z, 40.0f, (c << 8 | c << 16) | (uint32_t)alpha << 24 | c, sprite,
			self->angle[i]);
	}
}

/* a player character: it has a follow camera */
#define FXA_Player(e) (Obj(e)->cam)

static inline uint32_t FXA_Bits(float f) {
	uint32_t u;

	memcpy(&u, &f, sizeof(u));
	return u;
}

/* the trails of the Functionary while it runs fast (its speed above 1) */
static void cObjElementFXFunctionarySpecialAction_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXFunctionarySpecialAction *self = (cObjElementFXFunctionarySpecialAction *)base;
	vec3_t pos, rot;
	vec3_t pts[FXFUNCTIONARY_TRAILS];
	float target;

	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->active) {
		return;
	}
	CGameModel *m = Obj_Vtbl(self->ent)->GetModel(self->ent);
	if (!CGameModel_IsLoaded(m)) {
		self->ent = NULL;
		return;
	}
	CHECK(CGameModel_IsLoaded(m));
	float delta = FXA_Delta();
	self->time = self->time + delta;
	self->emitTime = self->emitTime + delta;
	if (WLife(self->ent)->life <= 0) {
		if (d->life > 0.4f) {
			d->life = 0.4f;
		}
		return;
	}
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -74.0f, 0.0f);
	Obj_Vtbl(self->ent)->GetRadius(self->ent);
	float speed = cTimer_GetSpeed(&Obj(self->ent)->timer);
	cTimer_GetSpeedTo(&Obj(self->ent)->timer);
	byte run = 0;
	if (WithLife_Vtbl(self->ent)->GetActCtrl(self->ent)) {
		if (cActCtrl_IsActive(WithLife_Vtbl(self->ent)->GetActCtrl(self->ent), "Run") && speed > 1.0f) {
			run = 1;
		}
	}
	if (self->ent && FXA_Player(self->ent) && speed == 1.0f && d->life > 0.4f) {
		d->life = 0.4f;
	}
	target = (speed - 1.0f) / 3.0f;
	if (0.4f > d->life) {
		FX_InRange(d->life - dt, 0.0f, 0.4f, &target);
	} else {
		FX_InRange(target, 0.0f, 1.0f, &target);
	}
	Float_Approach(&self->alpha, target, delta * 4.0f);
	if (0.0f >= self->alpha) {
		CGameModel_SetEffect(m, 0);
		CGameModel_SetEffectParam(m, 0, 0);
	} else {
		CGameModel_SetEffect(m, 2);
		CGameModel_SetEffectParam(m, 0, FXA_Bits(self->alpha * 0.5f));
	}
	CGameModel_Vertices(m, pts, NULL, NULL, FXFUNCTIONARY_TRAILS, -1, NULL, 0xffffffffu, g_FXFunctionaryVertex);
	CGameParticles *sp = &self->particles;
	for (int i = 0; i < FXFUNCTIONARY_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		vec3_t *p = &self->pos[i];

		CGameTrail_SetTransp(t, (byte)(0xff - (int)(self->alpha * 255.0f)));
		CGameParticles_SetAlpha(sp, self->alpha);
		CGameParticles_SetParticle(sp, i, pts[i].x, pts[i].y, pts[i].z, 55.0f, (g_FXFunctionaryColor[i] & 0xffffff) |
			0xa4000000, 2, 0.0f);
		float dx = p->x - pts[i].x;
		float dy = p->y - pts[i].y;
		float dz = p->z - pts[i].z;
		if (dy * dy + dx * dx + dz * dz > 3600.0f) {
			*p = pts[i];
			self->emitTime = 0.0f;
			if (!run) {
				continue;
			}
			CGameTrail_Add(t, pts[i], 50.0f, (g_FXFunctionaryColor[i] & 0xffffff) | 0xc4000000);
			sp->birthRate = 0;
			sp->lifeTime = FXA_Rand((double)0.019999996f, (double)0.24f);
			sp->sizeIni = FXA_Rand(20.0, 45.0);
			sp->sizeEnd = FXA_Rand(10.0, 105.0);
			sp->sizeLife = FXA_Rand((double)0.02000001f, (double)0.34f);
			sp->rotIni = sp->rotEnd = FXA_Rand((double)6.28318548f, 0.0);
			float rv = (float)(FXA_Rnd() * (double)0.785398185f - (double)0.392699093f);
			sp->spriteIni = 0x2b;
			sp->spriteEnd = 0x2b;
			sp->rotVar = rv;
			CGameParticles_SetColorRampOn(sp, 0);
			sp->colorIni = (g_FXFunctionaryColor[i] & 0xffffff) | 0xc4000000;
			sp->colorEnd = g_FXFunctionaryColor[i] | 0xff000000;
			sp->initPosMin.x = pts[i].x + (pts[i].x - p->x) * 0.5f;
			sp->initPosMin.y = pts[i].y + (pts[i].y - p->y) * 0.5f;
			sp->initPosMin.z = pts[i].z + (pts[i].z - p->z) * 0.5f;
			sp->initPosMax = sp->initPosMin;
			sp->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
			sp->initVelMax = sp->initVelMin;
			sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
			sp->initAccMax = sp->initAccMin;
			CGameParticles_Apply(sp);
			CGameParticles_EmitN(sp, 1, 0);
		} else {
			CGameTrail_SetLastPos(t, pts[i]);
			if (self->emitTime > 0.01f && 1.0f >= speed && self->emitTime > 0.0f) {
				float k = 0.0f;
				do {
					CGameTrail_Shrink(t);
					k = k + 0.01f;
				} while (self->emitTime > k);
			}
		}
	}
	if (self->emitTime > 0.01f) {
		self->emitTime = 0.0f;
	}
}

static inline int FXA_Min255(int v) {
	return v > 0xff ? 0xff : v;
}

/* the lightning: the 6 trails along the Bezier curves of the lines, flashes at their ends */
static void cObjElementFXMessengerAttack_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXMessengerAttack *self = (cObjElementFXMessengerAttack *)base;
	vec3_t pos, rot, q, tmp;
	vec3_t coefs[FXMESSENGER_POINTS];
	float f, a;

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->active) {
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -84.0f, 0.0f);
	float delta = FXA_Delta();
	self->emitTime = delta + self->emitTime;
	float t = delta + self->time;
	self->time = t;
	if (0.08f > t) {
		FX_InRange(t, 0.0f, 0.08f, &f);
		a = f;
	} else if (0.12f > t) {
		a = 1.0f;
	} else if (FX_InRange(t, 0.12f, 0.38f, &f)) {
		a = 1.0f - f;
	} else {
		a = 0.0f;
	}
	int alpha = (int)((1.0f - a * a * (2.0f - a)) * 255.0f);
	CGameParticles *sp = &self->particles;
	for (int k = 0; k <= FXMESSENGER_LINES; k++) {
		CGameTrail *tr = &self->trails[k];
		const vec3_t *pts = self->lines[k];
		int base = 2 + 6 * k;

		int n = CGameTrail_GetMaxTrails(tr);
		int a2 = alpha;
		if (k > 0) {
			a2 = FXA_Min255(alpha + 0x40);
		}
		CGameTrail_SetTransp(tr, (byte)a2);
		CGameTrail_Reset(tr);
		CGameTrail_BezierCoefs(tr, FXMESSENGER_POINTS, pts, coefs);
		float dx = pts[0].x - pts[9].x;
		float dy = pts[0].y - pts[9].y;
		float dz = pts[0].z - pts[9].z;
		float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		int segs = (int)(len / 1000.0f * (float)n);
		if (segs > n) {
			segs = n;
		}
		CGameTrail_SetSegments(tr, (byte)segs);
		for (int j = 0; j < segs - 1; j++) {
			float w;

			CGameTrail_BezierPoint(tr, FXMESSENGER_POINTS, coefs, &q, (float)j / (float)(segs - 2));
			if (j > 0 && j < segs - 2) {
				double r0 = FXA_Rnd();
				if (k > 0) {
					float r = (float)(r0 * 5.0 + 3.0);
					const vec3_t *dd = FX_RandomDir(&tmp);
					q.x = q.x + r * dd->x;
					q.y = q.y + dd->y * r;
					q.z = q.z + dd->z * r;
				} else {
					float r = (float)(r0 + r0 + 0.0);
					const vec3_t *dd = FX_RandomDir(&tmp);
					q.x = r * dd->x + q.x;
					q.y = dd->y * r + q.y;
					q.z = dd->z * r + q.z;
				}
				w = FXA_Rand(7.0, 8.0);
			} else {
				w = FXA_Rand(5.0, 20.0);
			}
			double rg = FXA_Rnd();
			float g = k > 0 ? (float)(rg * 8.0 + 14.0) : (float)(rg * 6.0 + 10.0);
			float h1 = FXA_Rand(15.0, 60.0);
			float h2 = FXA_Rand(15.0, 60.0);
			float b = FXA_Rand(15.0, 240.0);
			uint32_t color = ((((uint32_t)(int)g << 8 | (uint32_t)(int)h1) << 8 | (uint32_t)(int)h2) << 8) |
				(uint32_t)(int)b;
			CGameTrail_Add(tr, q, self->width[k] * w, color);
		}
		int e1 = FXA_Min255(a2 + 4);
		int e2 = FXA_Min255(a2 + 0x20);
		int e3 = FXA_Min255(a2 + 0xa4);
		const vec3_t *p0 = &pts[0];
		const vec3_t *p9 = &pts[9];
		CGameParticles_SetParticle(sp, base - 2, p0->x, p0->y, p0->z, self->width[k] * 20.0f,
			(uint32_t)e1 << 24 | 0xc4c4ff, 1, 0.0f);
		CGameParticles_SetParticle(sp, base - 1, p0->x, p0->y, p0->z, self->width[k] * 65.0f,
			(uint32_t)e2 << 24 | 0x8080ff, 2, 0.0f);
		CGameParticles_SetParticle(sp, base, p0->x, p0->y, p0->z, self->width[k] * 120.0f,
			(uint32_t)e3 << 24 | 0x4040ff, 2, 0.0f);
		CGameParticles_SetParticle(sp, base + 1, p9->x, p9->y, p9->z, self->width[k] * 20.0f,
			(uint32_t)e1 << 24 | 0xc4c4ff, 1, 0.0f);
		CGameParticles_SetParticle(sp, base + 2, p9->x, p9->y, p9->z, self->width[k] * 65.0f,
			(uint32_t)e2 << 24 | 0x8080ff, 2, 0.0f);
		CGameParticles_SetParticle(sp, base + 3, p9->x, p9->y, p9->z, self->width[k] * 120.0f,
			(uint32_t)e3 << 24 | 0x4040ff, 2, 0.0f);
	}
	if (!(0.02f > self->emitTime)) {
		self->emitTime = 0.0f;
	}
}

/* the data pack trails from the owner to the item, the middle points following, flashes at the ends */
static void cObjElementFXMessengerTakeDataPack_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXMessengerTakeDataPack *self = (cObjElementFXMessengerTakeDataPack *)base;
	vec3_t pos, rot, ang, t1, t2, o1, o2, q, tmp;
	vec3_t coefs[FXDATAPACK_POINTS];
	float f, a;

	(void)dt;
	FXA_Keep(&self->owner);
	if (!self->owner || !self->owner->active) {
		return;
	}
	CGameModel *m = FXA_Model(self->owner);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -85.0f, 0.0f);
	float delta = FXA_Delta();
	float t = delta + self->time;
	self->time = t;
	if (0.05f > t) {
		FX_InRange(t, 0.0f, 0.05f, &f);
		a = f;
	} else if (0.15f > t) {
		a = 1.0f;
	} else if (FX_InRange(t, 0.15f, 0.65f, &f)) {
		a = 1.0f - f;
	} else {
		a = 0.0f;
	}
	int alpha = (int)((1.0f - a * a * (2.0f - a)) * 255.0f);
	CGameParticles *sp = &self->particles;
	for (int i = 0; i < FXDATAPACK_TRAILS; i++) {
		vec3_t *p = self->points[i];
		CGameTrail *tr = &self->trails[i];
		int base = 2 + 6 * i;

		ang = (vec3_t){ 0.0f, (float)i * 26.0f - 50.0f, 0.0f };
		const vec3_t *r = Vec3_Rotate(&o1, &ang, Obj_Vtbl(self->owner)->GetViewAng(self->owner, &t1));
		const vec3_t *c = Obj_Vtbl(self->owner)->GetViewPos(self->owner, &t2);
		p[0].x = c->x + r->x;
		p[0].y = c->y + r->y;
		p[0].z = c->z + r->z;
		ang = (vec3_t){ 0.0f, (float)i * 37.0f - 50.0f, 0.0f };
		r = Vec3_Rotate(&o2, &ang, Obj_Vtbl(self->item)->GetViewAng(self->item, &t1));
		c = Obj_Vtbl(self->item)->GetViewPos(self->item, &t2);
		p[2].x = r->x + c->x;
		p[2].y = c->y + r->y;
		p[2].z = c->z + r->z;
		p[1].x = (((p[2].x - p[0].x) * 0.5f + p[0].x) - p[1].x) * 0.5f * delta * 20.0f + p[1].x;
		p[1].y = (((p[2].y - p[0].y) * 0.5f + p[0].y) - p[1].y) * 0.5f * delta * 20.0f + p[1].y;
		p[1].z = (((p[2].z - p[0].z) * 0.5f + p[0].z) - p[1].z) * 0.5f * delta * 20.0f + p[1].z;
		int n = CGameTrail_GetMaxTrails(tr);
		CGameTrail_SetTransp(tr, (byte)alpha);
		CGameTrail_Reset(tr);
		CGameTrail_BezierCoefs(tr, FXDATAPACK_POINTS, p, coefs);
		float dx = p[0].x - p[2].x;
		float dy = p[0].y - p[2].y;
		float dz = p[0].z - p[2].z;
		float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		int segs = (int)(len / 100.0f * (float)n);
		if (segs > n) {
			segs = n;
		}
		CGameTrail_SetSegments(tr, (byte)segs);
		for (int j = 0; j < segs - 1; j++) {
			float w;

			CGameTrail_BezierPoint(tr, FXDATAPACK_POINTS, coefs, &q, (float)j / (float)(segs - 2));
			if (j > 0 && j < segs - 2) {
				double r0 = FXA_Rnd();
				float rr = (float)(r0 + r0 + 2.0);
				const vec3_t *dd = FX_RandomDir(&tmp);
				q.x = q.x + rr * dd->x;
				q.y = q.y + dd->y * rr;
				q.z = q.z + dd->z * rr;
				w = FXA_Rand(7.0, 8.0);
			} else {
				w = FXA_Rand(5.0, 20.0);
			}
			int g = (int)FXA_Rand(8.0, 14.0);
			float h1 = FXA_Rand(15.0, 60.0);
			float h2 = FXA_Rand(15.0, 60.0);
			float b = FXA_Rand(15.0, 240.0);
			uint32_t color = ((((uint32_t)g << 8 | (uint32_t)(int)h1) << 8 | (uint32_t)(int)h2) << 8) |
				(uint32_t)(int)b;
			CGameTrail_Add(tr, q, self->width[i] * w, color);
		}
		uint32_t e1 = (uint32_t)FXA_Min255(alpha + 4) << 24;
		uint32_t e2 = (uint32_t)FXA_Min255(alpha + 0x20) << 24;
		uint32_t e3 = (uint32_t)FXA_Min255(alpha + 0xa4) << 24;
		CGameParticles_SetParticle(sp, base - 2, p[0].x, p[0].y, p[0].z, self->width[i] * 15.0f, e1 | 0xc4c4ff, 1,
			0.0f);
		CGameParticles_SetParticle(sp, base - 1, p[0].x, p[0].y, p[0].z, self->width[i] * 60.0f, e2 | 0x6060ff, 2,
			0.0f);
		CGameParticles_SetParticle(sp, base, p[0].x, p[0].y, p[0].z, self->width[i] * 130.0f, e3 | 0x3030ff, 5, 0.0f);
		CGameParticles_SetParticle(sp, base + 1, p[2].x, p[2].y, p[2].z, self->width[i] * 15.0f, e1 | 0xc4c4ff, 1,
			0.0f);
		CGameParticles_SetParticle(sp, base + 2, p[2].x, p[2].y, p[2].z, self->width[i] * 60.0f, e2 | 0x6060ff, 2,
			0.0f);
		CGameParticles_SetParticle(sp, base + 3, p[2].x, p[2].y, p[2].z, self->width[i] * 130.0f, e3 | 0x3030ff, 5,
			0.0f);
	}
}

#define FXA_RAMP_MERCURY 0x430668		/* [2] */

static uint32_t FXMercenary_DropRamps[18] = {
	0x30a0ff8c, 0x40a0ff8c, 0x4ba0ff8c, 0x5ca0ff8c, 0x80a0ff8c, 0xa4a0ff8c, 0xc4408037, 0xff000000, 0xff000000,
	0x10c4ff80, 0x20c4ff64, 0x30baff5a, 0x5caaff46, 0x80a0ff32, 0xa496ff26, 0xc4408012, 0xff000000, 0xff000000,
};

/* a drop of mercury: the sprite and size, the rotation, the ramp and the emission */
static void FXMercenary_Drop(CGameParticles *sp, float life, byte second) {
	sp->lifeTime = life;
	sp->sizeLife = 1.0f;
	sp->rotIni = sp->rotEnd = FXA_Rand((double)6.28318548f, 0.0);
	sp->rotVar = (float)(FXA_Rnd() * (double)1.57079637f - (double)0.785398185f);
	switch (crt_rand() % 3) {
	case 0:
		sp->spriteIni = 0x48;
		sp->sizeIni = FXA_Rand(2.5, 2.5);
		break;
	case 1:
		sp->spriteIni = 9;
		sp->sizeIni = FXA_Rand(2.5, 2.5);
		break;
	case 2:
		sp->spriteIni = 2;
		sp->sizeIni = FXA_Rand(5.5, 2.5);
		break;
	}
	sp->spriteEnd = sp->spriteIni;
	if (second) {
		float s = sp->sizeIni * FXA_Rand((double)0.19999999f, (double)0.6f);
		sp->sizeIni = s;
		sp->sizeEnd = s;
	} else {
		sp->sizeEnd = sp->sizeIni;
	}
	CGameParticles_Apply(sp);
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FXMercenary_DropRamps + (crt_rand() & 1) * 9);
	CGameParticles_EmitDt(sp, 1, 0, FXA_Rand((double)0.05f, 0.0));
}

/* the mercury drops out of the jar of the Mercenary from 0.5 to 1.3 s */
static void cObjElementFXMercenaryDrink_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXMercenaryDrink *self = (cObjElementFXMercenaryDrink *)base;
	vec3_t pos, rot, o, o2, dir, mouth, v, e, tmp;
	vec3_t pts[2];
	int idx[2] = { 1, 2 };
	float f;

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent) {
		d->life = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -91.0f, 0.0f);
	float delta = FXA_Delta();
	self->time = self->time + delta;
	float t = self->emitTime + delta;
	self->emitTime = t;
	if (!(t > 0.03f)) {
		return;
	}
	self->emitTime = 0.0f;
	Vec3_Rotate(&o, &self->offset, &rot);
	CGameModel_Vertices(m, pts, NULL, NULL, 2, -1, NULL, (uint32_t)self->nodeJar, idx);
	dir.x = pts[0].x - pts[1].x;
	dir.y = pts[0].y - pts[1].y;
	dir.z = pts[0].z - pts[1].z;
	rot = *Vec3_GetRotAng(&o, &dir);
	const vec3_t *r = Vec3_Rotate(&o, &self->offset, &rot);
	mouth.x = pts[0].x + r->x;
	mouth.y = r->y + pts[0].y;
	mouth.z = r->z + pts[0].z;
	if (!FX_InRange(self->time, 0.5f, 1.3f, &f)) {
		return;
	}
	float scale = 1.0f;
	if (strcmp(CGameModel_Name(m), "MercenaryC.M3D") == 0) {
		scale = 1.5f;
	}
	CGameParticles *sp = &self->particles;
	for (int i = 0; i < 18; i++) {
		sp->birthRate = 0;
		float life = FXA_Rand((double)0.019999996f, (double)0.22f) * scale;
		float rr = FXA_Rand(3.0, (double)0.2f);
		const vec3_t *dd = FX_RandomDir(&tmp);
		sp->initPosMin.x = dd->x * rr + mouth.x;
		sp->initPosMin.y = dd->y * rr + mouth.y;
		sp->initPosMin.z = dd->z * rr + mouth.z;
		sp->initPosMax = sp->initPosMin;
		float s = FXA_Rand(80.0, 60.0);
		float c = FXA_Rand((double)0.19999999f, (double)0.3f);
		float b = (float)(FXA_Rnd() * (double)1.2f - 1.0);
		v.x = (float)(FXA_Rnd() * (double)0.14f - (double)0.07f);
		v.y = b;
		v.z = c;
		const vec3_t *ov = Vec3_Rotate(&o, &v, &rot);
		sp->initVelMin.x = ov->x * s;
		sp->initVelMin.y = ov->y * s;
		sp->initVelMin.z = ov->z * s;
		sp->initVelMax = sp->initVelMin;
		float g = FXA_Rand(20.0, 40.0);
		v = (vec3_t){ 0.0f, 0.0f, -1.0f };
		const vec3_t *og = Vec3_Rotate(&o2, &v, &rot);
		sp->initAccMin.z = og->z * g + 0.0f;
		sp->initAccMin.x = og->x * g + 0.0f;
		sp->initAccMin.y = og->y * g - 980.0f;
		sp->initAccMax = sp->initAccMin;
		FXMercenary_Drop(sp, life, 0);
	}
	for (int i = 0; i < 6; i++) {
		sp->birthRate = 0;
		float life = FXA_Rand((double)0.01999998f, (double)0.36f);
		float rr = FXA_Rand(3.0, (double)0.2f);
		const vec3_t *dd = FX_RandomDir(&tmp);
		e.x = dd->x * rr;
		e.y = dd->y * rr;
		e.z = dd->z * rr;
		float zz = FXA_Rand(3.0, 8.0);
		v.x = 0.0f;
		v.y = (float)(-28.0 - FXA_Rnd() * 4.0) * scale;
		v.z = zz;
		const vec3_t *ov = Vec3_Rotate(&o, &v, &rot);
		sp->initPosMin.x = (ov->x + mouth.x) + e.x;
		sp->initPosMin.y = (ov->y + mouth.y) + e.y;
		sp->initPosMin.z = (ov->z + mouth.z) + e.z;
		sp->initPosMax = sp->initPosMin;
		float s = FXA_Rand(50.0, 70.0);
		float vz = (float)(FXA_Rnd() * (double)1.6f - (double)0.8f);
		float vy = FXA_Rand((double)0.6f, (double)1.3f);
		float vx = (float)(FXA_Rnd() * (double)1.6f - (double)0.8f);
		sp->initVelMin.y = vy * s;
		sp->initVelMin.z = vz * s;
		sp->initVelMin.x = vx * s;
		sp->initVelMax = sp->initVelMin;
		sp->initAccMin = (vec3_t){ 0.0f, -980.0f, 0.0f };
		sp->initAccMax = sp->initAccMin;
		FXMercenary_Drop(sp, life, 1);
	}
}

/* the 15 sparks of an end of the Bishop ray: spread by up to 60 degrees around dir times len (out of the end with
 * sign 1, into it with -1) */
static void FXBishop_Sparks(CGameTrail *tr, const vec3_t *end, const vec3_t *dir, float len, float sign, uint32_t c,
	uint32_t c2) {
	vec3_t ang, o, tmp;

	for (int i = 0; i < 15; i++) {
		float az = (float)(FXA_Rnd() * (double)2.09439516f - (double)1.04719758f);
		float ay = (float)(FXA_Rnd() * (double)2.09439516f - (double)1.04719758f);
		ang.x = (float)(FXA_Rnd() * (double)2.09439516f - (double)1.04719758f);
		ang.y = ay;
		ang.z = az;
		const vec3_t *r = Vec3_Rotate(&o, dir, &ang);
		float ox = r->x * len;
		float oy = r->y * len;
		float oz = r->z * len;
		float w = FXA_Rand(20.0, 50.0);
		float rr = FXA_Rand(10.0, 0.0);
		const vec3_t *dd = FX_RandomDir(&tmp);
		vec3_t pa, pb;
		if (sign > 0.0f) {
			pa.x = dd->x * rr + end->x;
			pa.y = end->y + dd->y * rr;
			pa.z = end->z + dd->z * rr;
			pb.x = ox + end->x;
			pb.y = oy + end->y;
			pb.z = oz + end->z;
		} else {
			pa.x = end->x + dd->x * rr;
			pa.y = end->y + dd->y * rr;
			pa.z = end->z + dd->z * rr;
			pb.x = end->x - ox;
			pb.y = end->y - oy;
			pb.z = end->z - oz;
		}
		CGameTrail_Add(tr, pa, w, c);
		CGameTrail_Add(tr, pb, w * 2.5f, c2);
	}
}

/* the flashes at an end of the Bishop ray: three at the end, eight spread around it along dir times k */
static void FXBishop_Flashes(CGameParticles *sp, int first, const vec3_t *end, const vec3_t *dir, float k, float spread,
	const float *sizes, float size8, int alpha) {
	vec3_t tmp;

	CGameParticles_SetParticle(sp, first, end->x, end->y, end->z, sizes[0],
		(uint32_t)FXA_Min255(alpha + 4) << 24 | 0xc0ffc0, 1, 0.0f);
	CGameParticles_SetParticle(sp, first + 1, end->x, end->y, end->z, sizes[1],
		(uint32_t)FXA_Min255(alpha + 0x10) << 24 | 0x80ff80, 5, 0.0f);
	CGameParticles_SetParticle(sp, first + 2, end->x, end->y, end->z, sizes[2],
		(uint32_t)FXA_Min255(alpha + 0xc0) << 24 | 0x40ff40, 2, 0.0f);
	for (int i = 0; i < 8; i++) {
		uint32_t e = (uint32_t)FXA_Min255(alpha + 0x40) << 24 | 0xc0ffc0;
		const vec3_t *dd = FX_RandomDir(&tmp);
		float x = (end->x + dir->x * k) + dd->x * spread;
		float y = (end->y + dir->y * k) + dd->y * spread;
		float z = (end->z + dir->z * k) + dd->z * spread;
		CGameParticles_SetParticle(sp, first + 3 + i, x, y, z, size8, e, 2, 0.0f);
	}
}

/* the green ray of the Bishop from .. to with sparks at both ends */
static void cObjElementFXBishopAttack_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXBishopAttack *self = (cObjElementFXBishopAttack *)base;
	vec3_t pos, rot, tmp;
	float f, a;
	static const float fromSizes[3] = { 100.0f, 240.0f, 300.0f };
	static const float toSizes[3] = { 120.0f, 260.0f, 320.0f };

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->active) {
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -80.0f, 0.0f);
	float delta = FXA_Delta();
	self->emitTime = delta + self->emitTime;
	float t = delta + self->time;
	self->time = t;
	if (0.06f > t) {
		FX_InRange(t, 0.0f, 0.06f, &f);
		a = f;
	} else if (0.1f > t) {
		a = 1.0f;
	} else if (FX_InRange(t, 0.1f, 0.35f, &f)) {
		a = 1.0f - f;
	} else {
		a = 0.0f;
	}
	float width = (3.0f - a * 2.0f) * (a * a) * 42.0f + 36.0f;
	int alpha = (int)((1.0f - (2.0f - a) * (a * a)) * 255.0f);
	uint32_t c = (uint32_t)FXA_Min255(alpha + 0x18) << 24 | 0x30ff30;
	CGameTrail_Reset(&self->trails[0]);
	CGameTrail_Add(&self->trails[0], self->from, width, c);
	CGameTrail_Add(&self->trails[0], self->to, width, c);
	CGameTrail_Reset(&self->trails[1]);
	for (int i = 0; i < 4; i++) {
		uint32_t a1 = (uint32_t)FXA_Min255(alpha + 4);
		float r = FXA_Rand(13.0, 1.0);
		const vec3_t *dd = FX_RandomDir(&tmp);
		float ex = dd->x * r;
		float ey = dd->y * r;
		float ez = dd->z * r;
		uint32_t col = (uint32_t)(0x80 - crt_rand() % 64) << 8;
		col = (uint32_t)(crt_rand() % 32 + 0x18) << 16 | col;
		col = (uint32_t)(crt_rand() % 32 + 0x18) | a1 << 24 | col;
		double rr = FXA_Rnd();
		float w = (float)(rr + rr + 11.0);
		vec3_t p = { ex + self->from.x, self->from.y + ey, self->from.z + ez };
		CGameTrail_Add(&self->trails[1], p, w, col);
		p = (vec3_t){ self->to.x + ex, self->to.y + ey, self->to.z + ez };
		CGameTrail_Add(&self->trails[1], p, w, col);
	}
	CGameTrail_SetUV(&self->trails[1], self->time * -8.0f, 0.0f);
	CGameTrail_Reset(&self->trails[2]);
	uint32_t c4 = (uint32_t)FXA_Min255(alpha + 8) << 24 | 0x3040c0;
	FXBishop_Sparks(&self->trails[2], &self->from, &self->dir, 150.0f, 1.0f, c4, c4 | 0xff000000);
	CGameParticles *sp = &self->particles;
	FXBishop_Flashes(sp, 0, &self->from, &self->dir, 35.0f, 18.0f, fromSizes, 95.0f, alpha);
	CGameTrail_Reset(&self->trails[3]);
	FXBishop_Sparks(&self->trails[3], &self->to, &self->dir, 160.0f, -1.0f, c4, c4 | 0xff000000);
	FXBishop_Flashes(sp, 0xc, &self->to, &self->dir, -15.0f, 20.0f, toSizes, 115.0f, alpha);
	if (!(0.02f > self->emitTime)) {
		self->emitTime = 0.0f;
	}
}

#define FXA_RAMP_SELLLIFE 0x430818		/* [3] */
#define FXA_RAMP_SELLLIFE2 0x43083c		/* [2] */

static uint32_t FXA_SELLLIFE_RAMPS[27] = {
	0x405aff5a, 0x4b4bff4b, 0x603ce63c, 0x7023ab23, 0x80264b26, 0xa40d380d, 0xc0051e05, 0xff000000, 0xff000000,
	0x4080ff80, 0x4b6eff6e, 0x605ae65a, 0x703cab3c, 0x80234b23, 0xa40a380a, 0xc0051e05, 0xff000000, 0xff000000,
	0x4040ff40, 0x4b32ff32, 0x6028e628, 0x701eab1e, 0x80144b14, 0xa40a380a, 0xc0051e05, 0xff000000, 0xff000000,
};

/* the glow of the Bishop: rotation, the ramp and one particle */
static void FXSellLife_Glow(CGameParticles *sp) {
	sp->rotIni = sp->rotEnd = FXA_Rand((double)6.28318548f, 0.0);
	sp->rotVar = (float)(FXA_Rnd() * (double)1.57079637f - (double)0.785398185f);
	byte sprite = (crt_rand() & 1) ? 2 : 5;
	sp->spriteIni = sprite;
	sp->spriteEnd = sprite;
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FXA_SELLLIFE_RAMPS);
	CGameParticles_Apply(sp);
	CGameParticles_EmitN(sp, 1, 0);
}

/* a data code of the Bishop: no rotation, one of 8 sprites, one of the three ramps and one particle */
static void FXSellLife_Code(CGameParticles *sp) {
	sp->rotIni = 0.0f;
	sp->rotEnd = 0.0f;
	sp->rotVar = 0.0f;
	sp->sizeLife = FXA_Rand((double)0.019999996f, (double)0.22f);
	byte sprite = (byte)(crt_rand() % 8 + 1);
	sp->spriteIni = sprite;
	sp->spriteEnd = sprite;
	CGameParticles_SetColorRampOn(sp, 1);
	CGameParticles_SetColorRamp(sp, FXA_SELLLIFE_RAMPS + crt_rand() % 3 * 9);
	CGameParticles_Apply(sp);
	CGameParticles_EmitN(sp, 1, 0);
}

static inline void FXA_Still(CGameParticles *sp) {
	sp->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	sp->initAccMax = sp->initAccMin;
}

/* the life sold by the Bishop: glows and data codes from the hands of the attacker into the attacked and a ring
 * of them at the attacked facing the camera */
static void cObjElementFXBishopSellLife_Update(cObjElementFX *base, cDebris *d, float dt) {
	cObjElementFXBishopSellLife *self = (cObjElementFXBishopSellLife *)base;
	vec3_t hand[2], pts4[4], tmp, mid, cam, v, n, off, pos;
	vec3_t pts[256];
	float f;

	(void)dt;
	FXA_Keep(&self->attacker);
	FXA_Keep(&self->attacked);
	if (!self->attacked || !self->attacked->active) {
		return;
	}
	CGameModel_SetSortZ(Debris_Model(d), -79.0f, 0.0f);
	float delta = FXA_Delta();
	self->time = self->time + delta;
	float t = self->emitTime + delta;
	self->emitTime = t;
	if (0.02f > t) {
		return;
	}
	CGameModel *ma = Obj_Vtbl(self->attacked)->GetModel(self->attacked);
	if (!ma || !CGameModel_IsLoaded(ma)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(ma));
	CGameParticles *glow = &self->glow;
	CGameParticles *code = &self->dataCodec;
	if (self->attacker) {
		CGameModel *mb = Obj_Vtbl(self->attacker)->GetModel(self->attacker);
		if (!mb || !CGameModel_IsLoaded(mb)) {
			return;
		}
		CHECK(CGameModel_IsLoaded(mb));
		for (int k = 0; k < 2; k++) {
			CGameModel_Vertices(mb, &hand[k], NULL, NULL, 1, 0, NULL, (uint32_t)self->nodeHand[k], NULL);
		}
		if (FX_InRange(self->time, 0.2f, 4.5f, &f)) {
			for (int k = 0; k < 2; k++) {
				const vec3_t *h = &hand[k];
				glow->birthRate = 0;
				float r = FXA_Rand(25.0, 0.0);
				const vec3_t *dd = FX_RandomDir(&tmp);
				glow->initPosMin.x = r * dd->x + h->x;
				glow->initPosMin.y = dd->y * r + h->y;
				glow->initPosMin.z = dd->z * r + h->z;
				glow->initPosMax = glow->initPosMin;
				r = FXA_Rand(25.0, 0.0);
				dd = FX_RandomDir(&tmp);
				glow->initVelMin.x = dd->x * r;
				glow->initVelMin.y = dd->y * r;
				glow->initVelMin.z = dd->z * r;
				glow->initVelMax = glow->initVelMin;
				FXA_Still(glow);
				glow->lifeTime = FXA_Rand((double)0.050000012f, (double)0.35f);
				glow->sizeIni = FXA_Rand(10.0, 25.0);
				glow->sizeEnd = FXA_Rand(15.0, 80.0);
				glow->sizeLife = FXA_Rand((double)0.050000012f, (double)0.35f);
				FXSellLife_Glow(glow);
				code->birthRate = 0;
				r = FXA_Rand(25.0, 0.0);
				dd = FX_RandomDir(&tmp);
				code->initPosMin.x = dd->x * r + h->x;
				code->initPosMin.y = dd->y * r + h->y;
				code->initPosMin.z = dd->z * r + h->z;
				code->initPosMax = code->initPosMin;
				r = FXA_Rand(25.0, 0.0);
				dd = FX_RandomDir(&tmp);
				code->initVelMin.x = r * dd->x;
				code->initVelMin.y = dd->y * r;
				code->initVelMin.z = dd->z * r;
				code->initVelMax = code->initVelMin;
				FXA_Still(code);
				code->lifeTime = FXA_Rand((double)0.050000012f, (double)0.35f);
				code->sizeIni = (float)(FXA_Rnd() + 6.0);
				code->sizeEnd = (float)(FXA_Rnd() + 9.0);
				FXSellLife_Code(code);
			}
		}
		if (FX_InRange(self->time, 3.9f, 5.0f, &f)) {
			mid.x = hand[0].x + (hand[1].x - hand[0].x) * 0.5f;
			mid.y = hand[0].y + (hand[1].y - hand[0].y) * 0.5f;
			mid.z = hand[0].z + (hand[1].z - hand[0].z) * 0.5f;
			CGameModel_Vertices(ma, pts4, NULL, NULL, 4, -1, NULL, 0xffffffffu, NULL);
			for (int k = 0; k < 4; k++) {
				const vec3_t *p = &pts4[k];
				float life = FXA_Rand((double)0.050000012f, (double)0.35f);
				glow->birthRate = 0;
				float r = FXA_Rand(15.0, 0.0);
				const vec3_t *dd = FX_RandomDir(&tmp);
				glow->initPosMin.x = r * dd->x + mid.x;
				glow->initPosMin.y = dd->y * r + mid.y;
				glow->initPosMin.z = dd->z * r + mid.z;
				float inv = 1.0f / life;
				glow->initPosMax = glow->initPosMin;
				glow->initVelMin.x = (p->x - glow->initPosMin.x) * inv;
				glow->initVelMin.y = (p->y - glow->initPosMin.y) * inv;
				glow->initVelMin.z = (p->z - glow->initPosMin.z) * inv;
				glow->initVelMax = glow->initVelMin;
				FXA_Still(glow);
				glow->lifeTime = life;
				glow->sizeIni = FXA_Rand(10.0, 25.0);
				glow->sizeEnd = FXA_Rand(15.0, 80.0);
				glow->sizeLife = FXA_Rand((double)0.050000012f, (double)0.35f);
				FXSellLife_Glow(glow);
				code->birthRate = 0;
				r = FXA_Rand(15.0, 0.0);
				dd = FX_RandomDir(&tmp);
				code->initPosMin.x = dd->x * r + mid.x;
				code->initPosMin.y = dd->y * r + mid.y;
				code->initPosMin.z = dd->z * r + mid.z;
				code->initPosMax = code->initPosMin;
				code->initVelMin.x = (p->x - code->initPosMin.x) * inv;
				code->initVelMin.y = (p->y - code->initPosMin.y) * inv;
				code->initVelMin.z = (p->z - code->initPosMin.z) * inv;
				code->initVelMax = code->initVelMin;
				FXA_Still(code);
				code->lifeTime = FXA_Rand((double)0.050000012f, (double)0.35f);
				code->sizeIni = (float)(FXA_Rnd() + 7.0);
				code->sizeEnd = (float)(FXA_Rnd() + 10.0);
				FXSellLife_Code(code);
			}
		}
	}
	float lo = self->attacker ? 4.0f : 0.0f;
	float hi = self->attacker ? 5.5f : 1.5f;
	if (FX_InRange(self->time, lo, hi, &f)) {
		cWorld *w = g_pWorld;
		cam = w->camPos[w->curCam];
		const vec3_t *c = Obj_Vtbl(self->attacked)->GetViewPos(self->attacked, &tmp);
		v.x = cam.x - c->x;
		v.y = cam.y - c->y;
		v.z = cam.z - c->z;
		Vec3_NormalizeTo(&n, &v);
		float radius = Obj_Vtbl(self->attacked)->GetRadius(self->attacked);
		off.x = n.x * radius * 0.3f;
		off.y = n.y * radius * 0.3f;
		off.z = n.z * radius * 0.3f;
		CGameModel_Vertices(ma, pts, NULL, NULL, 256, -1, NULL, 0xffffffffu, NULL);
		float k = self->attacker ? 1.8f : 1.2f;
		float r2 = Obj_Vtbl(self->attacked)->GetRadius(self->attacked) * k;
		float h = (1.0f - f * 2.0f) * r2;
		float s = Obj_Vtbl(self->attacked)->GetRadius(self->attacked) / 70.0f;
		CGameModel_GetPos(ma, &pos);
		float thr = s * 8.0f;
		int count = 0;
		for (int i = 0; i < 256; i++) {
			float y = pts[i].y;
			if (!(thr > __builtin_fabsf(y - (pos.y + h)))) {
				continue;
			}
			glow->birthRate = 0;
			glow->initPosMin.x = off.x + pts[i].x;
			glow->initPosMin.z = off.z + pts[i].z;
			glow->initPosMin.y = y + off.y;
			glow->initPosMax = glow->initPosMin;
			glow->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
			glow->initVelMax = glow->initVelMin;
			FXA_Still(glow);
			glow->lifeTime = FXA_Rand((double)0.120000035f, (double)0.42f);
			double rr = FXA_Rnd();
			glow->sizeIni = (float)(rr + rr + 8.0) * s;
			glow->sizeEnd = FXA_Rand(5.0, 40.0) * s;
			glow->sizeLife = FXA_Rand((double)0.10000001f, (double)0.2f);
			glow->rotIni = glow->rotEnd = FXA_Rand((double)6.28318548f, 0.0);
			float rv = (float)(FXA_Rnd() * (double)1.57079637f - (double)0.785398185f);
			glow->spriteIni = 5;
			glow->spriteEnd = 5;
			glow->rotVar = rv;
			CGameParticles_SetColorRampOn(glow, 1);
			CGameParticles_SetColorRamp(glow, FXA_SELLLIFE_RAMPS);
			CGameParticles_Apply(glow);
			CGameParticles_EmitN(glow, 1, 0);
			code->birthRate = 0;
			float up = (float)(FXA_Rnd() * 40.0 - 20.0) * s;
			code->initPosMin.x = pts[i].x + off.x;
			code->initPosMin.y = ((pos.y + h) + up) + off.y;
			code->initPosMin.z = pts[i].z + off.z;
			code->initPosMax = code->initPosMin;
			code->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
			code->initVelMax = code->initVelMin;
			float z0 = s * 0.0f;
			code->initAccMin.x = z0;
			code->initAccMin.y = s * 1500.0f;
			code->initAccMin.z = z0;
			code->initAccMax = code->initAccMin;
			code->lifeTime = FXA_Rand((double)0.03999999f, (double)0.28f);
			code->sizeIni = (float)(FXA_Rnd() + 4.0) * s;
			code->sizeEnd = (float)(FXA_Rnd() + 7.0) * s;
			code->rotIni = 0.0f;
			code->rotEnd = 0.0f;
			code->rotVar = 0.0f;
			code->sizeLife = FXA_Rand((double)0.02000001f, (double)0.32f);
			byte sprite = (byte)(crt_rand() % 8);
			code->spriteIni = sprite;
			code->spriteEnd = sprite;
			CGameParticles_SetColorRampOn(code, 1);
			CGameParticles_SetColorRamp(code, (&FXA_SELLLIFE_RAMPS[9]) + (crt_rand() & 1) * 9);
			CGameParticles_Apply(code);
			CGameParticles_EmitN(code, 1, 0);
			if (++count > 5) {
				break;
			}
		}
	}
	self->emitTime = 0.0f;
}

/* the vtable of cObjElementFXGearMissile */
static const cObjElementFXVtbl cObjElementFXGearMissile_vftable = {
	.Delete = cObjElementFXGearMissile_Delete,
	.Update = cObjElementFXGearMissile_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXGearMissile_Attach,
	.Reset = cObjElementFXGearMissile_Reset,
};

/* the vtable of cObjElementFXBankerSteal */
static const cObjElementFXVtbl cObjElementFXBankerSteal_vftable = {
	.Delete = cObjElementFXBankerSteal_Delete,
	.Update = cObjElementFXBankerSteal_Update,
	.IsAlive = FXMisc_Alive,
	.Start = FXCharActions_AttachC,
	.Reset = cObjElementFXBankerSteal_Reset,
};

/* the vtable of cObjElementFXPoliceSteal */
static const cObjElementFXVtbl cObjElementFXPoliceSteal_vftable = {
	.Delete = cObjElementFXPoliceSteal_Delete,
	.Update = cObjElementFXPoliceSteal_Update,
	.IsAlive = FXMisc_Alive,
	.Start = FXCharActions_AttachC,
	.Reset = cObjElementFXPoliceSteal_Reset,
};

/* the vtable of cObjElementFXFunctionarySpecialAction */
static const cObjElementFXVtbl cObjElementFXFunctionarySpecialAction_vftable = {
	.Delete = cObjElementFXFunctionarySpecialAction_Delete,
	.Update = cObjElementFXFunctionarySpecialAction_Update,
	.IsAlive = cObjElementFXFunctionarySpecialAction_Alive,
	.Start = cObjElementFXFunctionarySpecialAction_Attach,
	.Reset = cObjElementFXFunctionarySpecialAction_Reset,
};

/* the vtable of cObjElementFXPoliceGearConversion */
static const cObjElementFXVtbl cObjElementFXPoliceGearConversion_vftable = {
	.Delete = cObjElementFXPoliceGearConversion_Delete,
	.Update = cObjElementFXPoliceGearConversion_Update,
	.IsAlive = cObjElementFXPoliceGearConversion_Alive,
	.Start = cObjElementFXPoliceGearConversion_Attach,
	.Reset = cObjElementFXPoliceGearConversion_Reset,
};

/* the vtable of cObjElementFXDTritusAttack */
static const cObjElementFXVtbl cObjElementFXDTritusAttack_vftable = {
	.Delete = cObjElementFXDTritusAttack_Delete,
	.Update = cObjElementFXDTritusAttack_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXDTritusAttack_Attach,
	.Reset = cObjElementFXDTritusAttack_Reset,
};

/* the vtable of cObjElementFXDTritusKill */
static const cObjElementFXVtbl cObjElementFXDTritusKill_vftable = {
	.Delete = cObjElementFXDTritusKill_Delete,
	.Update = cObjElementFXDTritusKill_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXKill_Attach,
	.Reset = cObjElementFXKill_Reset,
};

/* the vtable of cObjElementFXBishopSellLife */
static const cObjElementFXVtbl cObjElementFXBishopSellLife_vftable = {
	.Delete = cObjElementFXBishopSellLife_Delete,
	.Update = cObjElementFXBishopSellLife_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXBishopSellLife_Attach,
	.Reset = cObjElementFXBishopSellLife_Reset,
};

/* the vtable of cObjElementFXBishopAttack */
static const cObjElementFXVtbl cObjElementFXBishopAttack_vftable = {
	.Delete = cObjElementFXBishopAttack_Delete,
	.Update = cObjElementFXBishopAttack_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXBishopAttack_Attach,
	.Reset = cObjElementFXBishopAttack_Reset,
};

/* the vtable of cObjElementFXBertoAttack */
static const cObjElementFXVtbl cObjElementFXBertoAttack_vftable = {
	.Delete = cObjElementFXBertoAttack_Delete,
	.Update = cObjElementFXBertoAttack_Update,
	.IsAlive = FXCharActions_AliveAction,
	.Start = FXCharActions_Attach8,
	.Reset = cObjElementFXBertoAttack_Reset,
};

/* the vtable of cObjElementFXBertoFlash */
static const cObjElementFXVtbl cObjElementFXBertoFlash_vftable = {
	.Delete = cObjElementFXBertoFlash_Delete,
	.Update = cObjElementFXBertoFlash_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXBertoFlash_Attach,
	.Reset = cObjElementFXBertoFlash_Reset,
};

/* the vtable of cObjElementFXMessengerAttack */
static const cObjElementFXVtbl cObjElementFXMessengerAttack_vftable = {
	.Delete = cObjElementFXMessengerAttack_Delete,
	.Update = cObjElementFXMessengerAttack_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXMessengerAttack_Attach,
	.Reset = cObjElementFXMessengerAttack_Reset,
};

/* the vtable of cObjElementFXMessengerTakeDataPack */
static const cObjElementFXVtbl cObjElementFXMessengerTakeDataPack_vftable = {
	.Delete = cObjElementFXMessengerTakeDataPack_Delete,
	.Update = cObjElementFXMessengerTakeDataPack_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXMessengerTakeDataPack_Attach,
	.Reset = cObjElementFXMessengerTakeDataPack_Reset,
};

/* the vtable of cObjElementFXSebastianFlash */
static const cObjElementFXVtbl cObjElementFXSebastianFlash_vftable = {
	.Delete = cObjElementFXSebastianFlash_Delete,
	.Update = cObjElementFXSebastianFlash_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXSebastianFlash_Attach,
	.Reset = cObjElementFXSebastianFlash_Reset,
};

/* the vtable of cObjElementFXBettyAttack */
static const cObjElementFXVtbl cObjElementFXBettyAttack_vftable = {
	.Delete = cObjElementFXBettyAttack_Delete,
	.Update = cObjElementFXBettyAttack_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXBettyAttack_Attach,
	.Reset = cObjElementFXBettyAttack_Reset,
};

/* the vtable of cObjElementFXBettyKill */
static const cObjElementFXVtbl cObjElementFXBettyKill_vftable = {
	.Delete = cObjElementFXBettyKill_Delete,
	.Update = cObjElementFXBettyKill_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXKill_Attach,
	.Reset = cObjElementFXKill_Reset,
};

/* the vtable of cObjElementFXBankMasterAttack */
static const cObjElementFXVtbl cObjElementFXBankMasterAttack_vftable = {
	.Delete = cObjElementFXBankMasterAttack_Delete,
	.Update = cObjElementFXBankMasterAttack_Update,
	.IsAlive = FXCharActions_AliveAction,
	.Start = cObjElementFXBankMasterAttack_Attach,
	.Reset = cObjElementFXBankMasterAttack_Reset,
};

/* the vtable of cObjElementFXBankMasterSuction */
static const cObjElementFXVtbl cObjElementFXBankMasterSuction_vftable = {
	.Delete = cObjElementFXBankMasterSuction_Delete,
	.Update = cObjElementFXBankMasterSuction_Update,
	.IsAlive = FXMisc_Alive,
	.Start = cObjElementFXBankMasterSuction_Attach,
	.Reset = cObjElementFXBankMasterSuction_Reset,
};

/* the vtable of cObjElementFXMercenaryDrink */
static const cObjElementFXVtbl cObjElementFXMercenaryDrink_vftable = {
	.Delete = cObjElementFXMercenaryDrink_Delete,
	.Update = cObjElementFXMercenaryDrink_Update,
	.IsAlive = FXMisc_Alive,
	.Start = FXCharActions_Attach8,
	.Reset = cObjElementFXMercenaryDrink_Reset,
};

static cGVar GVar_BankerStealSoundVol;
static cGVar GVar_BankerStealPosZ;
static cGVar GVar_BankerStealPosX;
static cGVar GVar_BankerSteal2Sound;
static cGVar GVar_BankerSteal3Sound;
static cGVar GVar_BankerStealPosY;
/* the engine variables (Gvars) */
static cGVar GVar_BankerSteal1Sound;
static cGVar GVar_BankMasterBatonPosZ;
static cGVar GVar_BankMasterBatonPosY;
static cGVar GVar_BankMasterBatonPosX;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_BankerStealPosX, &GVar_BankerStealPosX, "BankerStealPosX", 0, "Banker Steal Displacement from center", -50.0f)
INIT_GVAR_FLOAT(Init_GVar_BankerStealPosY, &GVar_BankerStealPosY, "BankerStealPosY", 0, "Banker Steal Displacement from center", 10.0f)
INIT_GVAR_FLOAT(Init_GVar_BankerStealPosZ, &GVar_BankerStealPosZ, "BankerStealPosZ", 0, "Banker Steal Displacement from center", -60.0f)
FINI_GVAR_STRING(Fini_GVar_BankerSteal1Sound, &GVar_BankerSteal1Sound)
INIT_GVAR_STRING(Init_GVar_BankerSteal1Sound, Fini_GVar_BankerSteal1Sound, &GVar_BankerSteal1Sound, "BankerSteal1Sound", 0, "Banker Steal sound name", "BankerSteal")
FINI_GVAR_STRING(Fini_GVar_BankerSteal2Sound, &GVar_BankerSteal2Sound)
INIT_GVAR_STRING(Init_GVar_BankerSteal2Sound, Fini_GVar_BankerSteal2Sound, &GVar_BankerSteal2Sound, "BankerSteal2Sound", 0, "Banker Steal sound name", "BankerSteal")
FINI_GVAR_STRING(Fini_GVar_BankerSteal3Sound, &GVar_BankerSteal3Sound)
INIT_GVAR_STRING(Init_GVar_BankerSteal3Sound, Fini_GVar_BankerSteal3Sound, &GVar_BankerSteal3Sound, "BankerSteal3Sound", 0, "Banker Steal sound name", "BankerSteal")
INIT_GVAR_FLOAT(Init_GVar_BankerStealSoundVol, &GVar_BankerStealSoundVol, "BankerStealSoundVol", 0, "Banker Steal sound volume", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_BankMasterBatonPosX, &GVar_BankMasterBatonPosX, "BankMasterBatonPosX", 0, "BankMaster Baton Displacement from center", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_BankMasterBatonPosY, &GVar_BankMasterBatonPosY, "BankMasterBatonPosY", 0, "BankMaster Baton Displacement from center", 35.0f)
INIT_GVAR_FLOAT(Init_GVar_BankMasterBatonPosZ, &GVar_BankMasterBatonPosZ, "BankMasterBatonPosZ", 0, "BankMaster Baton Displacement from center", -85.0f)
FINI_GVAR_STRING(Fini_GVar_GearMissileThunder1Sound, &GVar_GearMissileThunder1Sound)
INIT_GVAR_STRING(Init_GVar_GearMissileThunder1Sound, Fini_GVar_GearMissileThunder1Sound, &GVar_GearMissileThunder1Sound, "GearMissileThunder1Sound", 0, "Gear missile thunder 1 sound name", "GearMissileThunder1")
FINI_GVAR_STRING(Fini_GVar_GearMissileThunder2Sound, &GVar_GearMissileThunder2Sound)
INIT_GVAR_STRING(Init_GVar_GearMissileThunder2Sound, Fini_GVar_GearMissileThunder2Sound, &GVar_GearMissileThunder2Sound, "GearMissileThunder2Sound", 0, "Gear missile thunder 2 sound name", "GearMissileThunder2")
INIT_GVAR_FLOAT(Init_GVar_GearMissileThunderSoundVol, &GVar_GearMissileThunderSoundVol, "GearMissileThunderSoundVol", 0, "Gear missile thunder sound volume", 1.0f)
