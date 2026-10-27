/*
 * World/FX/FXCharActions.cpp of the game: the effects of the special actions of the characters, see
 * include/scrap/fxcharactions.h.
 */
#include "scrap/crt.h"
#include "scrap/doors.h"
#include "scrap/fxcharactions.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxweapons.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/scrapcollision.h"
#include "scrap/weapons.h"
#include "scrap/world.h"

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
static inline void FXA_SetVtbl(void *self, uint32_t vt) {
	*(void **)self = NULL;
	*(void **)self = Game_Ptr(vt);
}

static inline void FXA_TrailsCtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_ctor(t, sizeof(CGameTrail), n, Game_Ptr(CGAMETRAIL_CTOR), Game_Ptr(CGAMETRAIL_DTOR));
}

static inline void FXA_TrailsDtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_dtor(t, sizeof(CGameTrail), n, Game_Ptr(CGAMETRAIL_DTOR));
}

static inline void *FXA_Delete(void *self, uint32_t flags, uint32_t size) {
	if (flags & 1) {
		operator_delete_sized(self, size);
	}
	return self;
}

/* the model of the entity, CHECKed */
static CGameModel *FXA_Model(cEntity *e) {
	CGameModel *m = FX_Vtbl(e)->GetModel(e);

	CHECK(CGameModel_IsLoaded(m));
	return m;
}

/* the particles to the node name of the model of the debris, the node CHECKed */
static void FXA_AttachNode(CGameParticles *sp, CGameModel *m, const char *name) {
	int i = CGameModel_FindNodoR(m, name);

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
static vec3_t FXA_Dir(const vec3_t *from, const vec3_t *to) {
	vec3_t d;

	d.x = to->x - from->x;
	d.y = to->y - from->y;
	d.z = to->z - from->z;
	float len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	if (len != 0.0f) {
		float inv = 1.0f / len;
		d.x = d.x * inv;
		d.y = d.y * inv;
		d.z = d.z * inv;
	}
	return d;
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

void *__thiscall cObjElementFXBankMasterAttack_ctor(void *self) {
	cObjElementFXBankMasterAttack *s = self;

	FXA_SetVtbl(s, VT_FXBANKMASTERATTACK);
	CGameParticles_ctor(&s->particles);
	CGameTrail_ctor(&s->trail);
	CGameTrail_ctor(&s->trail2);
	s->started = 0;
	s->ent = NULL;
	s->fbc = 0.0f;
	s->fb8 = 0.0f;
	return s;
}

void *__thiscall cObjElementFXBankMasterSuction_ctor(void *self) {
	cObjElementFXBankMasterSuction *s = self;

	FXA_SetVtbl(s, VT_FXBANKMASTERSUCTION);
	CGameParticles_ctor(&s->particles);
	CGameTrail_ctor(&s->trail);
	s->started = 0;
	s->ent = NULL;
	s->target = NULL;
	s->fc0 = 0.0f;
	s->fbc = 0.0f;
	return s;
}

void *__thiscall cObjElementFXBankerSteal_ctor(void *self) {
	cObjElementFXBankerSteal *s = self;

	FXA_SetVtbl(s, VT_FXBANKERSTEAL);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->ent = NULL;
	s->i8 = 0;
	s->fc0 = 0.0f;
	s->fc4 = 0;
	s->fc8 = -1;
	s->node = -1;
	return s;
}

void *__thiscall cObjElementFXBertoAttack_ctor(void *self) {
	cObjElementFXBertoAttack *s = self;

	FXA_SetVtbl(s, VT_FXBERTOATTACK);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->ent = NULL;
	s->fbc = 0.0f;
	s->fb8 = 0.0f;
	return s;
}

void *__thiscall cObjElementFXBertoFlash_ctor(void *self) {
	cObjElementFXBertoFlash *s = self;

	FXA_SetVtbl(s, VT_FXBERTOFLASH);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXBERTOFLASH_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->target = NULL;
	s->fc0 = 0.0f;
	s->fbc = 0.0f;
	return s;
}

void *__thiscall cObjElementFXBettyAttack_ctor(void *self) {
	cObjElementFXBettyAttack *s = self;

	FXA_SetVtbl(s, VT_FXBETTYATTACK);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->particles2);
	FXA_TrailsCtor(s->trails, 2);
	FXA_TrailsCtor(s->trails2, FXBETTYATTACK_TRAILS);
	CGameTrail_ctor(&s->trail);
	CGameLensFlare_ctor(&s->flare);
	s->started = 0;
	s->ent = NULL;
	s->f16c = 0.0f;
	s->f168 = 0.0f;
	s->f170 = 0.0f;
	return s;
}

static void FXKill_Ctor(cObjElementFXKill *s, uint32_t vt) {
	FXA_SetVtbl(s, vt);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXKILL_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->fbc = 0.0f;
	s->fb8 = 0.0f;
}

void *__thiscall cObjElementFXBettyKill_ctor(void *self) {
	FXKill_Ctor(self, VT_FXBETTYKILL);
	return self;
}

void *__thiscall cObjElementFXBishopAttack_ctor(void *self) {
	cObjElementFXBishopAttack *s = self;

	FXA_SetVtbl(s, VT_FXBISHOPATTACK);
	CGameParticles_ctor(&s->particles);
	for (int i = 0; i < FXBISHOPATTACK_TRAILS; i++) {
		CGameTrail_ctor(&s->trails[i]);
	}
	s->started = 0;
	s->ent = NULL;
	s->f10 = 0.0f;
	s->fc = 0.0f;
	return s;
}

void *__thiscall cObjElementFXBishopSellLife_ctor(void *self) {
	cObjElementFXBishopSellLife *s = self;

	FXA_SetVtbl(s, VT_FXBISHOPSELLLIFE);
	CGameParticles_ctor(&s->glow);
	CGameParticles_ctor(&s->dataCodec);
	s->started = 0;
	s->f16c = 0.0f;
	s->f170 = 0.0f;
	s->attacker = NULL;
	s->attacked = NULL;
	return s;
}

void *__thiscall cObjElementFXDTritusAttack_ctor(void *self) {
	cObjElementFXDTritusAttack *s = self;

	FXA_SetVtbl(s, VT_FXDTRITUSATTACK);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXDTRITUSATTACK_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->fbc = 0.0f;
	s->fb8 = 0.0f;
	return s;
}

void *__thiscall cObjElementFXDTritusKill_ctor(void *self) {
	FXKill_Ctor(self, VT_FXDTRITUSKILL);
	return self;
}

void *__thiscall cObjElementFXFunctionarySpecialAction_ctor(void *self) {
	cObjElementFXFunctionarySpecialAction *s = self;

	FXA_SetVtbl(s, VT_FXFUNCTIONARYSPECIALACTION);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXFUNCTIONARY_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->fbc = 0.0f;
	s->fb8 = 0.0f;
	return s;
}

void *__thiscall cObjElementFXGearMissile_ctor(void *self) {
	cObjElementFXGearMissile *s = self;

	FXA_SetVtbl(s, VT_FXGEARMISSILE);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->sparks);
	CGameTrail_ctor(&s->trail);
	FXA_TrailsCtor(s->trails, FXGEARMISSILE_TRAILS);
	s->started = 0;
	s->i4 = 0;
	s->f170 = 0.0f;
	s->f174 = 0.5f;
	s->f178 = 0.0f;
	s->f18c = 0;
	return s;
}

void *__thiscall cObjElementFXMercenaryDrink_ctor(void *self) {
	cObjElementFXMercenaryDrink *s = self;

	FXA_SetVtbl(s, VT_FXMERCENARYDRINK);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->ent = NULL;
	s->fbc = 0.0f;
	s->fb8 = 0.0f;
	return s;
}

void *__thiscall cObjElementFXMessengerAttack_ctor(void *self) {
	cObjElementFXMessengerAttack *s = self;

	FXA_SetVtbl(s, VT_FXMESSENGERATTACK);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXMESSENGER_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->f10 = 0.0f;
	s->fc = 0.0f;
	return s;
}

void *__thiscall cObjElementFXMessengerTakeDataPack_ctor(void *self) {
	cObjElementFXMessengerTakeDataPack *s = self;

	FXA_SetVtbl(s, VT_FXMESSENGERTAKEDATAPACK);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXDATAPACK_TRAILS);
	s->started = 0;
	s->owner = NULL;
	s->fc = 0.0f;
	return s;
}

void *__thiscall cObjElementFXPoliceGearConversion_ctor(void *self) {
	cObjElementFXPoliceGearConversion *s = self;

	FXA_SetVtbl(s, VT_FXPOLICEGEARCONVERSION);
	CGameParticles_ctor(&s->particles);
	CGameParticles_ctor(&s->particles2);
	FXA_TrailsCtor(s->trails, FXGEAR_TRAILS);
	crt_eh_vector_ctor(s->armor, sizeof(CGameModel), FXGEAR_ARMORS, Game_Ptr(CGAMEMODEL_CTOR0),
		Game_Ptr(CGAMEMODEL_RESET));
	s->started = 0;
	s->ent = NULL;
	s->fc = 0.0f;
	s->i8 = 0;
	return s;
}

void *__thiscall cObjElementFXPoliceSteal_ctor(void *self) {
	cObjElementFXPoliceSteal *s = self;

	FXA_SetVtbl(s, VT_FXPOLICESTEAL);
	CGameParticles_ctor(&s->particles);
	s->started = 0;
	s->ent = NULL;
	s->target = NULL;
	s->fbc = 0.0f;
	s->fc0 = 0;
	s->fc4 = -1;
	return s;
}

void *__thiscall cObjElementFXSebastianFlash_ctor(void *self) {
	cObjElementFXSebastianFlash *s = self;

	FXA_SetVtbl(s, VT_FXSEBASTIANFLASH);
	CGameParticles_ctor(&s->particles);
	FXA_TrailsCtor(s->trails, FXSEBASTIAN_TRAILS);
	s->started = 0;
	s->ent = NULL;
	s->fbc = 0.0f;
	s->fb8 = 0.0f;
	return s;
}

/* ---- destructors ---- */

void __thiscall cObjElementFXBankMasterAttack_dtor(void *self) {
	cObjElementFXBankMasterAttack *s = self;

	*(void **)s = Game_Ptr(VT_FXBANKMASTERATTACK);
	CGameTrail_dtor(&s->trail2);
	CGameTrail_dtor(&s->trail);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXBankMasterSuction_dtor(void *self) {
	cObjElementFXBankMasterSuction *s = self;

	*(void **)s = Game_Ptr(VT_FXBANKMASTERSUCTION);
	CGameTrail_dtor(&s->trail);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

/* the classes with only the particles at off */
static void FXA_DtorParts(void *self, uint32_t vt, uint32_t off) {
	*(void **)self = Game_Ptr(vt);
	CGameParticles_Release((CGameParticles *)((byte *)self + off));
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXBankerSteal_dtor(void *self) {
	FXA_DtorParts(self, VT_FXBANKERSTEAL, 0xc);
}

void __thiscall cObjElementFXBertoAttack_dtor(void *self) {
	FXA_DtorParts(self, VT_FXBERTOATTACK, 0x8);
}

/* the classes with the particles at poff and n trails at toff */
static void FXA_DtorTrails(void *self, uint32_t vt, uint32_t poff, uint32_t toff, uint32_t n) {
	*(void **)self = Game_Ptr(vt);
	FXA_TrailsDtor((CGameTrail *)((byte *)self + toff), n);
	CGameParticles_Release((CGameParticles *)((byte *)self + poff));
	cObjElement_dtor(self);
}

void __thiscall cObjElementFXBertoFlash_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXBERTOFLASH, 0xc, 0x664, FXBERTOFLASH_TRAILS);
}

void __thiscall cObjElementFXBettyAttack_dtor(void *self) {
	cObjElementFXBettyAttack *s = self;

	*(void **)s = Game_Ptr(VT_FXBETTYATTACK);
	CGameLensFlare_dtor(&s->flare);
	CGameTrail_dtor(&s->trail);
	FXA_TrailsDtor(s->trails2, FXBETTYATTACK_TRAILS);
	FXA_TrailsDtor(s->trails, 2);
	CGameParticles_Release(&s->particles2);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXBettyKill_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXBETTYKILL, 0x8, 0x8f4, FXKILL_TRAILS);
}

void __thiscall cObjElementFXBishopAttack_dtor(void *self) {
	cObjElementFXBishopAttack *s = self;

	*(void **)s = Game_Ptr(VT_FXBISHOPATTACK);
	for (int i = FXBISHOPATTACK_TRAILS - 1; i >= 0; i--) {
		CGameTrail_dtor(&s->trails[i]);
	}
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXBishopSellLife_dtor(void *self) {
	cObjElementFXBishopSellLife *s = self;

	*(void **)s = Game_Ptr(VT_FXBISHOPSELLLIFE);
	CGameParticles_Release(&s->dataCodec);
	CGameParticles_Release(&s->glow);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXDTritusAttack_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXDTRITUSATTACK, 0x8, 0x112c, FXDTRITUSATTACK_TRAILS);
}

void __thiscall cObjElementFXDTritusKill_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXDTRITUSKILL, 0x8, 0x8f4, FXKILL_TRAILS);
}

void __thiscall cObjElementFXFunctionarySpecialAction_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXFUNCTIONARYSPECIALACTION, 0x8, 0xc4, FXFUNCTIONARY_TRAILS);
}

void __thiscall cObjElementFXGearMissile_dtor(void *self) {
	cObjElementFXGearMissile *s = self;

	*(void **)s = Game_Ptr(VT_FXGEARMISSILE);
	FXA_TrailsDtor(s->trails, FXGEARMISSILE_TRAILS);
	CGameTrail_dtor(&s->trail);
	CGameParticles_Release(&s->sparks);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXMercenaryDrink_dtor(void *self) {
	FXA_DtorParts(self, VT_FXMERCENARYDRINK, 0x8);
}

void __thiscall cObjElementFXMessengerAttack_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXMESSENGERATTACK, 0x14, 0xe8, FXMESSENGER_TRAILS);
}

void __thiscall cObjElementFXMessengerTakeDataPack_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXMESSENGERTAKEDATAPACK, 0x10, 0xc0, FXDATAPACK_TRAILS);
}

void __thiscall cObjElementFXPoliceGearConversion_dtor(void *self) {
	cObjElementFXPoliceGearConversion *s = self;

	*(void **)s = Game_Ptr(VT_FXPOLICEGEARCONVERSION);
	crt_eh_vector_dtor(s->armor, sizeof(CGameModel), FXGEAR_ARMORS, Game_Ptr(CGAMEMODEL_RESET));
	FXA_TrailsDtor(s->trails, FXGEAR_TRAILS);
	CGameParticles_Release(&s->particles2);
	CGameParticles_Release(&s->particles);
	cObjElement_dtor(s);
}

void __thiscall cObjElementFXPoliceSteal_dtor(void *self) {
	FXA_DtorParts(self, VT_FXPOLICESTEAL, 0xc);
}

void __thiscall cObjElementFXSebastianFlash_dtor(void *self) {
	FXA_DtorTrails(self, VT_FXSEBASTIANFLASH, 0x8, 0x7c8, FXSEBASTIAN_TRAILS);
}

/* ---- deleting destructors (vf00) ---- */

void *__thiscall cObjElementFXBankMasterAttack_Delete(void *self, uint32_t flags) {
	cObjElementFXBankMasterAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBankMasterAttack));
}

void *__thiscall cObjElementFXBankMasterSuction_Delete(void *self, uint32_t flags) {
	cObjElementFXBankMasterSuction_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBankMasterSuction));
}

void *__thiscall cObjElementFXBankerSteal_Delete(void *self, uint32_t flags) {
	cObjElementFXBankerSteal_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBankerSteal));
}

void *__thiscall cObjElementFXBertoAttack_Delete(void *self, uint32_t flags) {
	cObjElementFXBertoAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBertoAttack));
}

void *__thiscall cObjElementFXBertoFlash_Delete(void *self, uint32_t flags) {
	cObjElementFXBertoFlash_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBertoFlash));
}

void *__thiscall cObjElementFXBettyAttack_Delete(void *self, uint32_t flags) {
	cObjElementFXBettyAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBettyAttack));
}

void *__thiscall cObjElementFXBettyKill_Delete(void *self, uint32_t flags) {
	cObjElementFXBettyKill_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXKill));
}

void *__thiscall cObjElementFXBishopAttack_Delete(void *self, uint32_t flags) {
	cObjElementFXBishopAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBishopAttack));
}

void *__thiscall cObjElementFXBishopSellLife_Delete(void *self, uint32_t flags) {
	cObjElementFXBishopSellLife_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXBishopSellLife));
}

void *__thiscall cObjElementFXDTritusAttack_Delete(void *self, uint32_t flags) {
	cObjElementFXDTritusAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXDTritusAttack));
}

void *__thiscall cObjElementFXDTritusKill_Delete(void *self, uint32_t flags) {
	cObjElementFXDTritusKill_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXKill));
}

void *__thiscall cObjElementFXFunctionarySpecialAction_Delete(void *self, uint32_t flags) {
	cObjElementFXFunctionarySpecialAction_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXFunctionarySpecialAction));
}

void *__thiscall cObjElementFXGearMissile_Delete(void *self, uint32_t flags) {
	cObjElementFXGearMissile_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXGearMissile));
}

void *__thiscall cObjElementFXMercenaryDrink_Delete(void *self, uint32_t flags) {
	cObjElementFXMercenaryDrink_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXMercenaryDrink));
}

void *__thiscall cObjElementFXMessengerAttack_Delete(void *self, uint32_t flags) {
	cObjElementFXMessengerAttack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXMessengerAttack));
}

void *__thiscall cObjElementFXMessengerTakeDataPack_Delete(void *self, uint32_t flags) {
	cObjElementFXMessengerTakeDataPack_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXMessengerTakeDataPack));
}

void *__thiscall cObjElementFXPoliceGearConversion_Delete(void *self, uint32_t flags) {
	cObjElementFXPoliceGearConversion_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXPoliceGearConversion));
}

void *__thiscall cObjElementFXPoliceSteal_Delete(void *self, uint32_t flags) {
	cObjElementFXPoliceSteal_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXPoliceSteal));
}

void *__thiscall cObjElementFXSebastianFlash_Delete(void *self, uint32_t flags) {
	cObjElementFXSebastianFlash_dtor(self);
	return FXA_Delete(self, flags, sizeof(cObjElementFXSebastianFlash));
}

/* ---- Alive (vf02) ---- */

byte __thiscall FXCharActions_AliveAction(void *self, cDebris *d, cDebrisSys *sys) {
	cEntity *e = *(cEntity **)((byte *)self + 4);

	if (e && WithLife_Vtbl(e)->GetActCtrl(e)) {
		e = *(cEntity **)((byte *)self + 4);
		if (cActCtrl_IsActive(WithLife_Vtbl(e)->GetActCtrl(e), "Action")) {
			Debris_Life(d) = 1.0f;
		} else {
			e = *(cEntity **)((byte *)self + 4);
			if (cActCtrl_IsActive(WithLife_Vtbl(e)->GetActCtrl(e), "ActionInit")) {
				Debris_Life(d) = 1.0f;
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

byte __thiscall cObjElementFXFunctionarySpecialAction_Alive(cObjElementFXFunctionarySpecialAction *self, cDebris *d,
	cDebrisSys *sys) {
	CGameModel *m;

	if (!cDebris_Update(d, sys)) {
		if (self->ent) {
			m = FX_Vtbl(self->ent)->GetModel(self->ent);
			if (CGameModel_IsLoaded(m)) {
				FXA_EffectOff(m);
			}
		}
		return 0;
	}
	if (Debris_Life(d) > 0.4f && !self->ent) {
		Debris_Life(d) = 0.4f;
	}
	if (self->ent && cTimer_GetSpeed(Entity_Timer(self->ent)) == 1.0f
		&& cTimer_GetSpeedTo(Entity_Timer(self->ent)) == 1.0f && 0.4f >= Debris_Life(d)) {
		m = FX_Vtbl(self->ent)->GetModel(self->ent);
		if (!CGameModel_IsLoaded(m)) {
			return 0;
		}
		FXA_EffectOff(m);
	}
	return 1;
}

byte __thiscall cObjElementFXPoliceGearConversion_Alive(cObjElementFXPoliceGearConversion *self, cDebris *d,
	cDebrisSys *sys) {
	if (cDebris_Update(d, sys)) {
		return 1;
	}
	for (int i = 0; i < FXGEAR_ARMORS; i++) {
		CGameModel_Disable(&self->armor[i]);
	}
	return 0;
}

/* ---- Attach (vf03) ---- */

void __thiscall cObjElementFXBankMasterAttack_Attach(cObjElementFXBankMasterAttack *self, cDebris *d) {
	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.SistPart);
	CGameTrail_SetMaxTrails(&self->trail, 20);
	CGameTrail_SetType(&self->trail, 1);
	CGameTrail_SetTexture(&self->trail, 0);
	CGameTrail_SetMaxTrails(&self->trail2, 16);
	CGameTrail_SetType(&self->trail2, 0);
	CGameTrail_SetTexture(&self->trail2, 0);
	self->t = 0.0f;
}

void __thiscall cObjElementFXBankMasterSuction_Attach(cObjElementFXBankMasterSuction *self, cDebris *d) {
	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.SistPart);
	self->f1e4 = 0;
	self->f1e0 = 0;
}

void __thiscall FXCharActions_AttachC(void *self, cDebris *d) {
	FXA_AttachDummy01((CGameParticles *)((byte *)self + 0xc), d);
}

void __thiscall FXCharActions_Attach8(void *self, cDebris *d) {
	CGameParticles *sp = (CGameParticles *)((byte *)self + 8);

	FXA_AttachDummy01(sp, d);
	CHECK(sp->SistPart);
}

void __thiscall cObjElementFXBertoFlash_Attach(cObjElementFXBertoFlash *self, cDebris *d) {
	FXA_AttachDummy01(&self->particles, d);
	FXA_Trails(self->trails, FXBERTOFLASH_TRAILS, 31, 2, 3);
	CHECK(self->particles.SistPart);
}

void __thiscall cObjElementFXBettyAttack_Attach(cObjElementFXBettyAttack *self, cDebris *d) {
	CGameModel *m = Debris_Model(d);

	FXA_AttachDummy01(&self->particles, d);
	int i = CGameModel_FindNodoR(m, "Dummy02 (+dudv_pass)");
	if (i > -1) {
		CGameParticles_Attach(&self->particles2, m, i);
	}
	FXA_Trails(self->trails, 2, 31, 0, 3);
	FXA_Trails(self->trails2, FXBETTYATTACK_TRAILS, 31, 10, 3);
	CGameTrail_SetMaxTrails(&self->trail, 31);
	CGameTrail_SetType(&self->trail, 1);
	CHECK(self->particles.SistPart);
	self->f188 = 0;
	CGameLensFlare_SetI1c(&self->flare, 2);
}

void __thiscall cObjElementFXKill_Attach(cObjElementFXKill *self, cDebris *d) {
	FXA_AttachDummy01(&self->particles, d);
	FXA_Trails(self->trails, FXKILL_TRAILS, 31, 10, 3);
	CHECK(self->particles.SistPart);
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

void __thiscall cObjElementFXBishopAttack_Attach(cObjElementFXBishopAttack *self, cDebris *d) {
	static const float uv0[2] = { 4.0f, 1.0f };
	static const float uv1[2] = { 16.0f, 0.5f };

	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.SistPart);
	FXBishop_Trail(&self->trails[0], 3, 4, uv0);
	FXBishop_Trail(&self->trails[1], 31, 14, uv1);
	FXBishop_Trail(&self->trails[2], 31, 4, NULL);
	FXBishop_Trail(&self->trails[3], 31, 4, NULL);
}

void __thiscall cObjElementFXBishopSellLife_Attach(cObjElementFXBishopSellLife *self, cDebris *d) {
	FXA_AttachDummy01(&self->glow, d);
	CHECK(self->glow.SistPart);
	FXA_AttachNode(&self->dataCodec, Debris_Model(d), "Dummy02");
	CHECK(self->dataCodec.SistPart);
}

void __thiscall cObjElementFXDTritusAttack_Attach(cObjElementFXDTritusAttack *self, cDebris *d) {
	FXA_AttachDummy01(&self->particles, d);
	FXA_Trails(self->trails, FXDTRITUSATTACK_TRAILS, 31, 10, 3);
	CHECK(self->particles.SistPart);
}

void __thiscall cObjElementFXFunctionarySpecialAction_Attach(cObjElementFXFunctionarySpecialAction *self, cDebris *d) {
	FXA_AttachDummy01(&self->particles, d);
	for (int i = 0; i < FXFUNCTIONARY_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 8);
		CGameTrail_SetTexture(t, 2);
		CGameTrail_SetSmooth(t, 1);
		CGameTrail_SetFade(t, 1);
		CGameTrail_SetOrient(t, (vec3_t){ 0.0f, 1.0f, 0.0f }, 2);
	}
	self->f1a4 = 0;
}

void __thiscall cObjElementFXGearMissile_Attach(cObjElementFXGearMissile *self, cDebris *d) {
	CGameModel *m = Debris_Model(d);

	CHECK(CGameModel_IsLoaded(m));
	self->node = CGameModel_FindNodoR(m, "Dummy01");
	CHECK(self->node > -1);
	CGameParticles_Attach(&self->particles, m, self->node);
	CHECK(self->particles.SistPart);
	self->nodeSparks = CGameModel_FindNodoR(m, "Dummy02");
	CHECK(self->nodeSparks > -1);
	CGameParticles_Attach(&self->sparks, m, self->nodeSparks);
	CHECK(self->sparks.SistPart);
	for (int i = 0; i < FXGEARMISSILE_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 14);
		CGameTrail_SetSmooth(t, 1);
		CGameTrail_SetColor(t, 0xc0c0ff);
		CGameTrail_SetTexture(t, 9);
		CGameTrail_SetULen(t, 4.0f, 1.0f);
		CGameTrail_SetPremul(t, 1);
		self->f690[i] = 0.5f;
		self->f190[i] = -1.0f;
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

void __thiscall cObjElementFXMessengerAttack_Attach(cObjElementFXMessengerAttack *self, cDebris *d) {
	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.SistPart);
	for (int i = 0; i < FXMESSENGER_TRAILS; i++) {
		FXMessenger_Trail(&self->trails[i], 31, i == 0);
	}
}

void __thiscall cObjElementFXMessengerTakeDataPack_Attach(cObjElementFXMessengerTakeDataPack *self, cDebris *d) {
	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.SistPart);
	for (int i = 0; i < FXDATAPACK_TRAILS; i++) {
		FXMessenger_Trail(&self->trails[i], 12, i & 1);
	}
}

/* the pieces of the Gear armors: armor, node name, the offset of its time in the element and the time */
typedef struct FXGearPiece {
	int armor;
	const char *name;
	uint32_t off;
	float time;
} FXGearPiece;

static const FXGearPiece fxa_gearPieces[] = {
	{ 0, "OB_PiezasGear_08", 0x99c, 2.9f }, { 0, "OB_PiezasGear_09", 0x9a0, 2.9f },
	{ 0, "OB_PiezasGear_10", 0x9a4, 2.83f }, { 0, "OB_PiezasGear_11", 0x9a8, 3.2f },
	{ 0, "OB_PiezasGear_12", 0x9ac, 2.96f }, { 0, "OB_PiezasGear_13", 0x9b0, 3.03f },
	{ 0, "OB_PiezasGear_14", 0x9b4, 3.16f }, { 0, "OB_PiezasGear_17", 0x9b8, 3.1f },
	{ 1, "OB_PiezasGear_15", 0x9d8, 3.3f }, { 1, "OB_PiezasGear_16", 0x9dc, 3.36f },
	{ 1, "OB_PiezasGear_18", 0x9e0, 3.13f }, { 1, "OB_PiezasGear_19", 0x9e4, 3.13f },
	{ 1, "OB_PiezasGear_20", 0x9e8, 3.26f }, { 1, "OB_PiezasGear_21", 0x9ec, 3.33f },
	{ 1, "OB_PiezasGear_22", 0x9f0, 3.4f }, { 1, "OB_PiezasGear_23", 0x9f4, 3.66f },
	{ 1, "OB_PiezasGear_24", 0x9f8, 3.26f }, { 1, "OB_PiezasGear_38", 0x9fc, 3.43f },
	{ 1, "OB_PiezasGear_39", 0xa00, 3.33f }, { 1, "OB_PiezasGear_40", 0xa04, 3.26f },
	{ 2, "OB_PiezasGear_01", 0xa18, 3.66f }, { 2, "OB_PiezasGear_25", 0xa1c, 3.26f },
	{ 2, "OB_PiezasGear_26", 0xa20, 3.33f }, { 2, "OB_PiezasGear_27", 0xa24, 3.46f },
	{ 2, "OB_PiezasGear_28", 0xa28, 3.46f }, { 2, "OB_PiezasGear_29", 0xa2c, 3.53f },
	{ 2, "OB_PiezasGear_30", 0xa30, 3.53f }, { 2, "OB_PiezasGear_31", 0xa34, 3.6f },
	{ 2, "OB_PiezasGear_32", 0xa38, 3.6f }, { 2, "OB_PiezasGear_37", 0xa3c, 3.53f },
};
static const int fxa_gearNumPieces[FXGEAR_ARMORS] = { 8, 12, 10 };

void __thiscall cObjElementFXPoliceGearConversion_Attach(cObjElementFXPoliceGearConversion *self, cDebris *d) {
	char buf[256];
	int i, k;

	FXA_AttachDummy01(&self->particles, d);
	CHECK(self->particles.SistPart);
	for (i = 0; i < FXGEAR_ARMORS; i++) {
		CGameModel *m = &self->armor[i];
		vec3_t scale = { 1.0f, 1.0f, 1.0f };

		Str_PrintfQuiet(buf, 0xff, "Models/Chars/Gear/Gear_Armor%d.M3D", i + 1);
		CGameModel_LoadScaled(m, buf, &scale);
		CGameModel_Disable(m);
		CGameModel_SetPivotNodo(m, "DC_Root");
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
			self->piece[i][k] = CGameModel_FindNodoRWild(&self->armor[i], p->name);
			*(float *)((byte *)self + p->off) = p->time;
		}
		for (; k < FXGEAR_PIECES; k++) {
			self->piece[i][k] = -1;
		}
	}
	FXA_Trails(self->trails, FXGEAR_TRAILS, 31, 10, 3);
	self->f844 = 0;
	self->fc = 0.0f;
	self->f10 = 0.0f;
	self->f848 = 0;
	self->fcc = 0;
}

void __thiscall cObjElementFXSebastianFlash_Attach(cObjElementFXSebastianFlash *self, cDebris *d) {
	FXA_AttachDummy01(&self->particles, d);
	FXA_Trails(self->trails, FXSEBASTIAN_TRAILS, 31, 2, 3);
	CHECK(self->particles.SistPart);
}

/* ---- Reset (vf04) ---- */

void __thiscall cObjElementFXBankMasterAttack_Reset(cObjElementFXBankMasterAttack *self, int arg) {
	(void)arg;
	self->fbc = 0.0f;
	self->fb8 = 0.0f;
	CGameTrail_Clear(&self->trail);
	CGameTrail_Clear(&self->trail2);
	self->t = 0.0f;
}

void __thiscall cObjElementFXBankMasterSuction_Reset(cObjElementFXBankMasterSuction *self, int arg) {
	(void)arg;
	self->fc0 = 0.0f;
	self->fbc = 0.0f;
	self->f1e0 = 0;
	self->f1e4 = 0;
}

void __thiscall cObjElementFXBankerSteal_Reset(cObjElementFXBankerSteal *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->i8 = 0;
	self->fc0 = 0.0f;
}

void __thiscall cObjElementFXBertoAttack_Reset(cObjElementFXBertoAttack *self, int arg) {
	(void)arg;
	self->fbc = 0.0f;
	self->fb8 = 0.0f;
	CGameParticles_KillAll(&self->particles);
}

void __thiscall cObjElementFXBertoFlash_Reset(cObjElementFXBertoFlash *self, int arg) {
	(void)arg;
	self->fc0 = 0.0f;
	self->fbc = 0.0f;
	FXA_ClearTrails(self->trails, FXBERTOFLASH_TRAILS);
	CGameParticles_KillAll(&self->particles);
}

void __thiscall cObjElementFXBettyAttack_Reset(cObjElementFXBettyAttack *self, int arg) {
	(void)arg;
	self->f16c = 0.0f;
	self->f168 = 0.0f;
	self->f170 = 0.0f;
	FXA_ClearTrails(self->trails, 2);
	FXA_ClearTrails(self->trails2, FXBETTYATTACK_TRAILS);
	CGameTrail_Clear(&self->trail);
	CGameParticles_KillAll(&self->particles);
	if (self->particles2.SistPart) {
		CGameParticles_KillAll(&self->particles2);
	}
	self->f188 = 0;
}

void __thiscall cObjElementFXKill_Reset(cObjElementFXKill *self, int arg) {
	(void)arg;
	self->fbc = 0.0f;
	self->fb8 = 0.0f;
	FXA_ClearTrails(self->trails, FXKILL_TRAILS);
	CGameParticles_KillAll(&self->particles);
}

void __thiscall cObjElementFXBishopAttack_Reset(cObjElementFXBishopAttack *self, int arg) {
	(void)arg;
	self->f10 = 0.0f;
	self->fc = 0.0f;
	CGameParticles_KillAll(&self->particles);
	FXA_ClearTrails(self->trails, FXBISHOPATTACK_TRAILS);
}

void __thiscall cObjElementFXBishopSellLife_Reset(cObjElementFXBishopSellLife *self, int arg) {
	(void)arg;
	self->f16c = 0.0f;
	self->f170 = 0.0f;
	self->attacker = NULL;
	self->attacked = NULL;
	CGameParticles_KillAll(&self->glow);
	CGameParticles_KillAll(&self->dataCodec);
}

void __thiscall cObjElementFXDTritusAttack_Reset(cObjElementFXDTritusAttack *self, int arg) {
	(void)arg;
	self->fbc = 0.0f;
	self->fb8 = 0.0f;
	FXA_ClearTrails(self->trails, FXDTRITUSATTACK_TRAILS);
	CGameParticles_KillAll(&self->particles);
}

void __thiscall cObjElementFXFunctionarySpecialAction_Reset(cObjElementFXFunctionarySpecialAction *self, int arg) {
	(void)arg;
	self->fbc = 0.0f;
	self->fb8 = 0.0f;
	CGameParticles_KillAll(&self->particles);
	FXA_ClearTrails(self->trails, FXFUNCTIONARY_TRAILS);
	self->f1a4 = 0;
}

void __thiscall cObjElementFXGearMissile_Reset(cObjElementFXGearMissile *self, int arg) {
	(void)arg;
	self->i4 = 0;
	self->f170 = 0.0f;
	self->f178 = 0.0f;
	for (int i = 0; i < FXGEARMISSILE_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
		self->f190[i] = -1.0f;
	}
	self->f18c = 0;
	CGameTrail_Clear(&self->trail);
	CGameParticles_KillAll(&self->particles);
}

void __thiscall cObjElementFXMercenaryDrink_Reset(cObjElementFXMercenaryDrink *self, int arg) {
	(void)arg;
	self->fbc = 0.0f;
	self->fb8 = 0.0f;
}

void __thiscall cObjElementFXMessengerAttack_Reset(cObjElementFXMessengerAttack *self, int arg) {
	(void)arg;
	self->f10 = 0.0f;
	self->fc = 0.0f;
	CGameParticles_KillAll(&self->particles);
	FXA_ClearTrails(self->trails, FXMESSENGER_TRAILS);
}

void __thiscall cObjElementFXMessengerTakeDataPack_Reset(cObjElementFXMessengerTakeDataPack *self, int arg) {
	(void)arg;
	self->fc = 0.0f;
	CGameParticles_KillAll(&self->particles);
	FXA_ClearTrails(self->trails, FXDATAPACK_TRAILS);
}

void __thiscall cObjElementFXPoliceGearConversion_Reset(cObjElementFXPoliceGearConversion *self, int arg) {
	(void)arg;
	self->fc = 0.0f;
	self->f10 = 0.0f;
	self->i8 = 0;
	/* the 45 piece flags, 0x96c..0x998 (the last 0x2d bytes, the movups of the original) */
	for (int i = 0; i < FXGEAR_ARMORS; i++) {
		for (int k = 0; k < FXGEAR_PIECES; k++) {
			self->pieceOff[i][k] = 0;
		}
	}
	CGameParticles_KillAll(&self->particles);
	FXA_ClearTrails(self->trails, FXGEAR_TRAILS);
	self->f844 = 0;
	self->f848 = 0;
	self->fcc = 0;
}

void __thiscall cObjElementFXPoliceSteal_Reset(cObjElementFXPoliceSteal *self, int arg) {
	(void)arg;
	self->ent = NULL;
	self->target = NULL;
	self->fbc = 0.0f;
}

void __thiscall cObjElementFXSebastianFlash_Reset(cObjElementFXSebastianFlash *self, int arg) {
	(void)arg;
	self->fbc = 0.0f;
	self->fb8 = 0.0f;
	FXA_ClearTrails(self->trails, FXSEBASTIAN_TRAILS);
	CGameParticles_KillAll(&self->particles);
}

/* ---- the setters and starts ---- */

void __thiscall FXCharActions_SetEntity8(void *self, cEntity *e) {
	*(cEntity **)((byte *)self + 8) = e;
	FXA_Model(e);
}

void __thiscall cObjElementFXBankerSteal_SetEntity(cObjElementFXBankerSteal *self, cEntity *e) {
	self->ent = e;
	self->node = CGameModel_FindNodoR(FXA_Model(e), "DF_BankerSteal");
}

void __thiscall cObjElementFXBishopSellLife_SetAttacked(cObjElementFXBishopSellLife *self, cEntity *e) {
	self->attacked = e;
	if (e) {
		FXA_Model(e);
	}
}

void __thiscall cObjElementFXBishopSellLife_SetAttacker(cObjElementFXBishopSellLife *self, cEntity *e) {
	self->attacker = e;
	if (!e) {
		return;
	}
	CGameModel *m = FXA_Model(e);
	self->nodeHand[0] = CGameModel_FindNodoR(m, "R_Bishop_Mano_Izq");
	CHECK(self->nodeHand[0] > -1);
	self->nodeHand[1] = CGameModel_FindNodoR(m, "R_Bishop_Mano_Der");
	CHECK(self->nodeHand[1] > -1);
}

void __thiscall cObjElementFXMessengerTakeDataPack_SetItem(cObjElementFXMessengerTakeDataPack *self, cEntity *e) {
	self->item = e;
	CHECK(e);
	CHECK(self->item->vftable->IsType(self->item, "Obj"));
}

void __thiscall cObjElementFXMessengerTakeDataPack_SetOwner(cObjElementFXMessengerTakeDataPack *self, cEntity *e) {
	self->owner = e;
	CHECK(e);
	CHECK(self->owner->vftable->IsType(self->owner, TYEN_WALKCHAR));
}

void __thiscall cObjElementFXMercenaryDrink_SetC0(cObjElementFXMercenaryDrink *self, float a, float b, float c) {
	self->fc0 = a;
	self->fc4 = b;
	self->fc8 = c;
}

void __thiscall cObjElementFXMercenaryDrink_Start(cObjElementFXMercenaryDrink *self, cEntity *e) {
	self->ent = e;
	self->nodeJar = CGameModel_FindNodoR(FXA_Model(e), "OB_JarraMercurio_MercA");
}

void __thiscall cObjElementFXBankMasterAttack_Start(cObjElementFXBankMasterAttack *self, cEntity *e) {
	self->ent = e;
	self->nodeBaton = CGameModel_FindNodoR(FXA_Model(e), "DF_Batton");
	CHECK(self->nodeBaton >= 0);
}

void __thiscall cObjElementFXBertoAttack_Start(cObjElementFXBertoAttack *self, cEntity *e) {
	self->ent = e;
	self->nodeCristal = CGameModel_FindNodoR(FXA_Model(e), "R_Berto_TroncoInf-cristal");
	CHECK(self->nodeCristal > -1);
}

void __thiscall cObjElementFXKill_SetAngles(cObjElementFXKill *self, float x, float y, float z) {
	self->angles.x = x;
	self->angles.y = y;
	self->angles.z = z;
}

void __thiscall cObjElementFXBishopAttack_SetLine(cObjElementFXBishopAttack *self, float fx, float fy, float fz,
	float tx, float ty, float tz) {
	self->from = (vec3_t){ fx, fy, fz };
	self->to = (vec3_t){ tx, ty, tz };
	self->dir = FXA_Dir(&self->from, &self->to);
}

void __thiscall cObjElementFXGearMissile_Run(void *self, cDebris *d) {
	cObjElementFX *fx = self;

	for (int i = 0; i < 8; i++) {
		fx->vftable->Update(fx, d, 0.0f);
	}
}

void __fastcall cObjElementFXBankMasterSuction_RandomPoints(cObjElementFXBankMasterSuction *self) {
	vec3_t a, b;

	self->ent->vftable->GetPos(self->ent, &a);
	self->target->vftable->GetPos(self->target, &b);
	for (int i = 0; i < FXBMSUCTION_POINTS; i++) {
		self->phase[i] = (float)(FXA_Rnd() * (double)0.9f - (double)0.9f);
		FXA_RandomDirScaled(&self->dir[i], FXA_Rand(0.75, 0.25));
		self->angle[i] = FXA_Rand((double)6.28318548f, 0.0);
	}
}

void __fastcall cObjElementFXBertoFlash_RandomPoints(cObjElementFXBertoFlash *self) {
	for (int i = 0; i < FXBERTOFLASH_POINTS; i++) {
		FXA_RandomDirScaled(&self->dir[i], FXA_Rand(4.0, 10.0));
		float lo = FX_Vtbl(self->target)->GetRadius(self->target) * 0.65f;
		float hi = FX_Vtbl(self->target)->GetRadius(self->target) * 0.85f;
		FXA_RandomDirScaled(&self->dir2[i], (float)(FXA_Rnd() * (double)(hi - lo) + (double)lo));
	}
}

void __fastcall cObjElementFXSebastianFlash_RandomPoints(cObjElementFXSebastianFlash *self) {
	for (int i = 0; i < FXSEBASTIAN_POINTS; i++) {
		FXA_RandomDirScaled(&self->dir[i], FXA_Rand(3.0, 0.0));
		FXA_RandomDirScaled(&self->dir2[i], FXA_Rand(65.0, 15.0));
	}
}

/* the sines and cosines of the negated angles of rot, for Vec3_RotateSC */
static void FXA_InvRotSC(const vec3_t *rot, vec3_t *c, vec3_t *s) {
	X87_SinCos(-rot->x, &s->x, &c->x);
	X87_SinCos(-rot->y, &s->y, &c->y);
	X87_SinCos(-rot->z, &s->z, &c->z);
}

void __thiscall cObjElementFXKill_Start(cObjElementFXKill *self, cEntity *e) {
	vec3_t center, rot, c, s, tmp, rel;
	vec3_t pts[FXKILL_POINTS];

	self->ent = e;
	CGameModel *m = FXA_Model(e);
	CGameModel_GetPos(m, &center);
	CGameModel_GetRot(m, &rot);
	float scale = FX_Vtbl(self->ent)->GetRadius(self->ent) / 90.0f;
	CGameModel_Vertices(m, pts, NULL, NULL, FXKILL_POINTS, -1, self->vertex, 0xffffffffu, NULL);
	FXA_InvRotSC(&rot, &c, &s);
	for (int i = 0; i < FXKILL_POINTS; i++) {
		rel.x = pts[i].x - center.x;
		rel.y = pts[i].y - center.y;
		rel.z = pts[i].z - center.z;
		self->point[i] = *Vec3_RotateSC(&tmp, &rel, &c, &s);
		self->scale[i] = FXA_Rand(15.0, 60.0) * scale;
		self->f7c8[i] = FXA_Rand(100.0, 525.0);
		self->f69c[i] = FXA_Rand(30.0, 105.0);
	}
}

void __thiscall cObjElementFXDTritusAttack_Start(cObjElementFXDTritusAttack *self, cEntity *e) {
	vec3_t center, rot, c, s, tmp, rel;
	vec3_t pts[FXDTRITUSATTACK_POINTS];

	self->ent = e;
	CGameModel *m = FXA_Model(e);
	CGameModel_GetPos(m, &center);
	CGameModel_GetRot(m, &rot);
	self->nodeMaster = CGameModel_FindNodoR(m, "R_Detritus_Tronco");
	CHECK(self->nodeMaster > -1);
	CGameModel_Vertices(m, &center, NULL, NULL, 1, 0, NULL, (uint32_t)self->nodeMaster, NULL);
	CGameModel_Vertices(m, pts, NULL, NULL, FXDTRITUSATTACK_POINTS, -1, self->vertex, 0xffffffffu, NULL);
	FXA_InvRotSC(&rot, &c, &s);
	for (int i = 0; i < FXDTRITUSATTACK_POINTS; i++) {
		rel.x = pts[i].x - center.x;
		rel.y = pts[i].y - center.y;
		rel.z = pts[i].z - center.z;
		self->point[i] = *Vec3_RotateSC(&tmp, &rel, &c, &s);
		self->fa24[i] = FXA_Rand(15.0, 40.0);
		self->fed4[i] = FXA_Rand(50.0, 375.0);
		self->fc7c[i] = FXA_Rand(10.0, 25.0);
	}
}

void __thiscall cObjElementFXFunctionarySpecialAction_Start(void *self, cEntity *e) {
	cObjElementFXFunctionarySpecialAction *s = self;
	vec3_t v, rot, out, pos;

	s->ent = e;
	CHECK(s->ent);
	CHECK(s->ent->vftable->IsType(s->ent, TYEN_WALKCHAR));
	CGameModel *m = FX_Vtbl(s->ent)->GetModel(s->ent);
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

void __thiscall cObjElementFXBettyAttack_Emit(cObjElementFXBettyAttack *self, float x, float y, float z) {
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
	CGameParticles_SetColorRamp(sp, Game_Ptr(FXA_RAMP_BETTY));
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

void __fastcall cObjElementFXMessengerTakeDataPack_Start(cObjElementFXMessengerTakeDataPack *self) {
	vec3_t ang, t1, t2, o1, o2, p0, d;

	CHECK(CGameModel_IsLoaded(FX_Vtbl(self->owner)->GetModel(self->owner)));
	CHECK(CGameModel_IsLoaded(FX_Vtbl(self->item)->GetModel(self->item)));
	for (int i = 0; i < FXDATAPACK_TRAILS; i++) {
		ang = (vec3_t){ 0.0f, (float)i * 26.0f - 50.0f, 0.0f };
		const vec3_t *a = Vec3_Rotate(&o1, &ang, FX_Vtbl(self->owner)->GetAngles(self->owner, &t1));
		const vec3_t *c = FX_Vtbl(self->owner)->GetCenter(self->owner, &t2);
		p0.x = c->x + a->x;
		p0.y = c->y + a->y;
		p0.z = c->z + a->z;
		ang = (vec3_t){ 0.0f, (float)i * 33.0f - 50.0f, 0.0f };
		const vec3_t *b = Vec3_Rotate(&o2, &ang, FX_Vtbl(self->item)->GetAngles(self->item, &t1));
		c = FX_Vtbl(self->item)->GetCenter(self->item, &t2);
		d.x = (b->x + c->x) - p0.x;
		d.y = (c->y + b->y) - p0.y;
		d.z = (c->z + b->z) - p0.z;
		self->f160[i] = FXA_Rand((double)0.6f, 1.0);
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

void __thiscall cObjElementFXMessengerAttack_SetLine(cObjElementFXMessengerAttack *self, float fx, float fy, float fz,
	float tx, float ty, float tz) {
	vec3_t coefs[FXMESSENGER_POINTS];
	vec3_t p, q, d;
	CColisTri tri;
	int i, k;

	self->from = (vec3_t){ fx, fy, fz };
	self->to = (vec3_t){ tx, ty, tz };
	self->dir = FXA_Dir(&self->from, &self->to);
	d.x = self->to.x - self->from.x;
	d.y = self->to.y - self->from.y;
	d.z = self->to.z - self->from.z;
	self->f3d0 = FXA_Rand((double)0.5999999f, (double)2.2f);
	d.x = d.x * 0.111111112f;
	d.y = d.y * 0.111111112f;
	d.z = d.z * 0.111111112f;
	for (k = 0; k < FXMESSENGER_POINTS; k++) {
		vec3_t *r = &self->ray[k];
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
	CGameTrail_BezierCoefs(&self->trails[0], FXMESSENGER_POINTS, self->ray, coefs);
	for (i = 0; i < FXMESSENGER_LINES; i++) {
		self->f3d4[i] = FXA_Rand((double)0.39999998f, (double)1.1f);
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
			vec3_t *l = &self->lines[i][k];
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

void __thiscall cObjElementFXPoliceGearConversion_Start(cObjElementFXPoliceGearConversion *self, cEntity *e) {
	CGameParticles *sp = &self->particles;
	vec3_t t, v, ang, out, base;
	int i;

	self->ent = e;
	CGameModel *m = FXA_Model(e);
	self->nodeRoot = CGameModel_FindNodoR(m, "R_Police-NoArmr_tronco-inferior");
	CHECK(self->nodeRoot > -1);
	for (i = 0; i < FXGEAR_ARMORS; i++) {
		CGameModel *a = &self->armor[i];
		CGameModel_SetPos(a, CGameModel_GetPos(FX_Vtbl(self->ent)->GetModel(self->ent), &t));
		CGameModel_SetRot(a, CGameModel_GetRot(FX_Vtbl(self->ent)->GetModel(self->ent), &t));
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
	float h = Entity_F45C(self->ent);
	const vec3_t *c = FX_Vtbl(self->ent)->GetCenter(self->ent, &t);
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
		CGameParticles_SetColorRamp(sp, (uint32_t *)Game_Ptr(FXA_RAMP_GEAR) + crt_rand() % 3 * 9);
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
		CGameParticles_SetColorRamp(sp, (uint32_t *)Game_Ptr(FXA_RAMP_GEAR) + (crt_rand() & 1) * 9);
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
	if (*e && (Entity_Removed(*e) || !(*e)->flag_4c)) {
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
	if (!self->ent || !self->ent->flag_4c) {
		Debris_Life(d) = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	FXA_FollowModel(d, m, &pos, &rot, sortBias);
	float dt = FXA_Delta();
	self->fbc = self->fbc + dt;
	self->fb8 = self->fb8 + dt;
	FXBeams b = { &self->particles, self->trails, FXKILL_TRAILS, FXKILL_POINTS, self->point, self->f69c, self->f7c8,
		self->scale, &self->angles, 12000.0f, 0.84f, fadeOut, inner, outer, rgb };
	FXBeams_Draw(&b, self->fbc, dt, &pos, &rot);
}

void __thiscall cObjElementFXBettyKill_Update(cObjElementFXKill *self, cDebris *d, float dt) {
	(void)dt;
	FXKill_Update(self, d, -88.0f, 0.28f, 0x40f54019, 0xffe1190a, 0xf54019);
}

void __thiscall cObjElementFXDTritusKill_Update(cObjElementFXKill *self, cDebris *d, float dt) {
	(void)dt;
	FXKill_Update(self, d, -77.0f, 0.02f, 0x401940f5, 0xff0a19e1, 0x1940f5);
}

void __thiscall cObjElementFXBankMasterAttack_Update(cObjElementFXBankMasterAttack *self, cDebris *d, float dt) {
	vec3_t pos, rot, base, v, ang, o1, o2, p;
	matrix_t mtx;

	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->flag_4c) {
		Debris_Life(d) = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	const matrix_t *mm = CGameModel_NodoMatrix(m, &mtx, (uint32_t)self->nodeBaton, 1, NULL, 0);
	base.x = mm->m[12];
	base.y = mm->m[13];
	base.z = mm->m[14];
	CGameModel_SetPos(Debris_Model(d), &base);
	CGameModel_SetRot(Debris_Model(d), &rot);
	float delta = FXA_Delta();
	self->fbc = delta + self->fbc;
	self->fb8 = delta + self->fb8;
	float layer = 0.0f;
	void *ctrl = WithLife_Vtbl(self->ent)->GetActCtrl(self->ent);
	byte action = 0;
	if (ctrl) {
		action = cActCtrl_IsActive(ctrl, "Action");
		layer = *cActCtrl_GetLayer(ctrl, 0) / 1.33f;
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

void __thiscall cObjElementFXBertoAttack_Update(cObjElementFXBertoAttack *self, cDebris *d, float dt) {
	vec3_t pos, rot, avg;
	vec3_t pts[32];
	float f;

	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->flag_4c) {
		Debris_Life(d) = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -81.0f, 0.0f);
	float delta = FXA_Delta();
	self->fbc = delta + self->fbc;
	self->fb8 = delta + self->fb8;
	int alpha = 0;
	if (FX_InRange(self->fbc, 0.0f, 0.6f, &f)) {
		alpha = 0xff - (int)(f * 255.0f);
	}
	if (0.95f > Debris_Life(d)) {
		FX_InRange(Debris_Life(d) - dt, 0.0f, 1.0f, &f);
		alpha = 0xff - (int)(f * 255.0f);
	}
	CGameParticles *sp = &self->particles;
	sp->birthRate = 0;
	CGameParticles_Apply(sp);
	CGameModel_Vertices(m, pts, NULL, NULL, 32, -1, NULL, (uint32_t)self->nodeCristal, NULL);
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

void __thiscall cObjElementFXDTritusAttack_Update(cObjElementFXDTritusAttack *self, cDebris *d, float dt) {
	vec3_t pos, rot;

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->flag_4c) {
		Debris_Life(d) = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -78.0f, 0.0f);
	float delta = FXA_Delta();
	self->fbc = delta + self->fbc;
	self->fb8 = delta + self->fb8;
	CGameModel_Vertices(m, &pos, NULL, NULL, 1, 0, NULL, (uint32_t)self->nodeMaster, NULL);
	FXBeams b = { &self->particles, self->trails, FXDTRITUSATTACK_TRAILS, FXDTRITUSATTACK_POINTS, self->point,
		self->fc7c, self->fed4, self->fa24, NULL, 2000.0f, 0.72f, 0.1f, 0x40f54019, 0xffe1190a, 0xf54019 };
	FXBeams_Draw(&b, self->fbc, delta, &pos, &rot);
}

#define FXA_RAMP_POLICESTEAL 0x4307ac

void __thiscall cObjElementFXPoliceSteal_Update(cObjElementFXPoliceSteal *self, cDebris *d, float dt) {
	vec3_t tmp, a, b, v, ang, dir;

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->flag_4c) {
		return;
	}
	FXA_Keep(&self->target);
	if (!self->target || !self->target->flag_4c) {
		return;
	}
	if (0.65f > Debris_Life(d)) {
		return;
	}
	CGameModel_SetPos(Debris_Model(d), FX_Vtbl(self->ent)->GetCenter(self->ent, &tmp));
	CGameModel_SetRot(Debris_Model(d), FX_Vtbl(self->ent)->GetAngles(self->ent, &tmp));
	float t = self->fbc + FXA_Delta();
	self->fbc = t;
	if (0.04f > t) {
		return;
	}
	self->fbc = 0.0f;
	FX_Vtbl(self->target)->GetCenter(self->target, &b);
	FX_Vtbl(self->ent)->GetCenter(self->ent, &a);
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
	float hi = FX_Vtbl(self->target)->GetRadius(self->target) * 0.5f;
	float lo = FX_Vtbl(self->target)->GetRadius(self->target) * 0.25f;
	float r = Rand_FloatRange(lo, hi);
	sp->initPosMin.x = b.x + dir.x * r;
	sp->initPosMin.y = b.y + dir.y * r;
	sp->initPosMin.z = b.z + dir.z * r;
	sp->initPosMax = sp->initPosMin;
	hi = FX_Vtbl(self->ent)->GetRadius(self->ent) * 0.5f;
	lo = FX_Vtbl(self->ent)->GetRadius(self->ent) * 0.25f;
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
	CGameParticles_SetColorRamp(sp, Game_Ptr(FXA_RAMP_POLICESTEAL));
	CGameParticles_EmitN(sp, 1, 0);
}

void __thiscall cObjElementFXSebastianFlash_Update(cObjElementFXSebastianFlash *self, cDebris *d, float dt) {
	vec3_t pos, rot, tmp, D, p1, p2, e, base;
	float f;
	int alpha;

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent || !self->ent->flag_4c) {
		Debris_Life(d) = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -82.0f, 0.0f);
	float delta = FXA_Delta();
	float time = delta + self->fbc;
	self->fb8 = delta + self->fb8;
	self->fbc = time;
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
		Debris_Life(d) = 0.0f;
		return;
	}
	for (int k = 0; k < FXSEBASTIAN_TRAILS; k++) {
		CGameTrail_Reset(&self->trails[k]);
		CGameTrail_SetTransp(&self->trails[k], (byte)alpha);
	}
	CGameParticles *sp = &self->particles;
	sp->birthRate = 0;
	CGameParticles_Apply(sp);
	const vec3_t *c = FX_Vtbl(self->ent)->GetCenter(self->ent, &tmp);
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

void __thiscall cObjElementFXBertoFlash_Update(cObjElementFXBertoFlash *self, cDebris *d, float dt) {
	vec3_t pos, rot, tmp, tmp2, D, p1, p2, e, base;
	float f, v;

	(void)dt;
	FXA_Keep(&self->ent);
	if (!self->ent) {
		Debris_Life(d) = 0.0f;
		return;
	}
	FXA_Keep(&self->target);
	if (!self->target) {
		Debris_Life(d) = 0.0f;
		return;
	}
	CGameModel *m = FXA_Model(self->ent);
	CGameModel_GetPos(m, &pos);
	CGameModel_GetRot(m, &rot);
	CGameModel_SetPos(Debris_Model(d), &pos);
	CGameModel_SetRot(Debris_Model(d), &rot);
	CGameModel_SetSortZ(Debris_Model(d), -82.0f, 0.0f);
	float delta = FXA_Delta();
	self->fc0 = self->fc0 + delta;
	self->fbc = self->fbc + delta;
	if (FX_InRange(self->fc0, 0.0f, 0.2f, &f)) {
		float g = 1.0f - f;
		v = g * g * (2.0f - g);
	} else if (FX_InRange(self->fc0, 0.2f, 0.7f, &f)) {
		v = (3.0f - f * 2.0f) * (f * f);
	} else {
		Debris_Life(d) = 0.0f;
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
	const vec3_t *c = FX_Vtbl(self->ent)->GetCenter(self->ent, &tmp);
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
		const vec3_t *c2 = FX_Vtbl(self->target)->GetCenter(self->target, &tmp2);
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
