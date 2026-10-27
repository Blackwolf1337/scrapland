/*
 * Entities/EmbeddedFX/EmbeddedFXMisc.cpp of the game: the effects embedded in the object entities, see
 * include/scrap/embeddedfxmisc.h.
 */
#include "scrap/crt.h"
#include "scrap/embeddedfxmisc.h"
#include "scrap/filesys.h"
#include "scrap/fxmisc.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/timer.h"

#define CGAMEPARTICLES_CTOR 0x21d1b0
#define CGAMEPARTICLES_DTOR 0x21d1c0
#define CGAMETRAIL_CTOR 0x21ddb0
#define CGAMETRAIL_DTOR 0x21de40

/* the switch of the DataPack lasers (a Gvar) */
#define g_DataPackOn GAME_DATA(int, 0x44d48c)
/* "" of the original, the glow node of a coin name that matches none */
#define EFXM_EMPTY ((const char *)Game_Ptr(0x3a3228))

/* rand() & 0x7fff as 0..1 (double) */
static inline double EFXM_Rnd(void) {
	return (double)(crt_rand() & 0x7fff) * (1.0 / 32767.0);
}

static inline void EFXM_SetVtbl(void *self, uint32_t vt) {
	*(void **)self = Game_Ptr(vt);
}

static inline void EFXM_ParticlesCtor(CGameParticles *p, uint32_t n) {
	crt_eh_vector_ctor(p, sizeof(CGameParticles), n, Game_Ptr(CGAMEPARTICLES_CTOR), Game_Ptr(CGAMEPARTICLES_DTOR));
}

static inline void EFXM_ParticlesDtor(CGameParticles *p, uint32_t n) {
	crt_eh_vector_dtor(p, sizeof(CGameParticles), n, Game_Ptr(CGAMEPARTICLES_DTOR));
}

static inline void EFXM_TrailsCtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_ctor(t, sizeof(CGameTrail), n, Game_Ptr(CGAMETRAIL_CTOR), Game_Ptr(CGAMETRAIL_DTOR));
}

static inline void EFXM_TrailsDtor(CGameTrail *t, uint32_t n) {
	crt_eh_vector_dtor(t, sizeof(CGameTrail), n, Game_Ptr(CGAMETRAIL_DTOR));
}

static inline cEmbeddedFX *EFXM_Delete(cEmbeddedFX *self, byte flags, uint32_t size) {
	if (flags & 1) {
		operator_delete_sized(self, size);
	}
	return self;
}

/* ---- constructors ---- */

cEmbeddedFXAmplifier *__fastcall cEmbeddedFXAmplifier_ctor(cEmbeddedFXAmplifier *self) {
	EFXM_SetVtbl(self, VT_EFX_AMPLIFIER);
	CGameParticles_ctor(&self->flares);
	CGameTrail_ctor(&self->trail);
	self->base.ent = NULL;
	self->t = 0.0f;
	return self;
}

cEmbeddedFXBattery *__fastcall cEmbeddedFXBattery_ctor(cEmbeddedFXBattery *self) {
	EFXM_SetVtbl(self, VT_EFX_BATTERY);
	CGameParticles_ctor(&self->glow);
	EFXM_TrailsCtor(self->trails, FXBATTERY_TRAILS);
	self->base.ent = NULL;
	return self;
}

cEmbeddedFXBossShield *__fastcall cEmbeddedFXBossShield_ctor(cEmbeddedFXBossShield *self) {
	EFXM_SetVtbl(self, VT_EFX_BOSSSHIELD);
	CGameParticles_ctor(&self->particles);
	EFXM_TrailsCtor(self->trails, FXBOSSSHIELD_TRAILS);
	self->base.ent = NULL;
	return self;
}

cEmbeddedFXCapsule *__fastcall cEmbeddedFXCapsule_ctor(cEmbeddedFXCapsule *self) {
	EFXM_SetVtbl(self, VT_EFX_CAPSULE);
	EFXM_ParticlesCtor(self->motors, FXCAPSULE_MOTORS);
	self->base.ent = NULL;
	return self;
}

/* Coin, Conversor and Item: the vtable and one particle system at 0x10 */
static void *EFXM_Ctor10(void *self, uint32_t vt) {
	EFXM_SetVtbl(self, vt);
	CGameParticles_ctor((CGameParticles *)((byte *)self + 0x10));
	((cEmbeddedFX *)self)->ent = NULL;
	return self;
}

cEmbeddedFXCoin *__fastcall cEmbeddedFXCoin_ctor(cEmbeddedFXCoin *self) {
	return EFXM_Ctor10(self, VT_EFX_COIN);
}

cEmbeddedFXConversor *__fastcall cEmbeddedFXConversor_ctor(cEmbeddedFXConversor *self) {
	return EFXM_Ctor10(self, VT_EFX_CONVERSOR);
}

cEmbeddedFXDTritusDesktop *__fastcall cEmbeddedFXDTritusDesktop_ctor(cEmbeddedFXDTritusDesktop *self) {
	EFXM_SetVtbl(self, VT_EFX_DTRITUSDESKTOP);
	CGameParticles_ctor(&self->glow);
	CGameTrail_ctor(&self->trail);
	self->base.ent = NULL;
	self->i0c = 0;
	self->i0c8 = 0;
	return self;
}

cEmbeddedFXDataCube *__fastcall cEmbeddedFXDataCube_ctor(cEmbeddedFXDataCube *self) {
	EFXM_SetVtbl(self, VT_EFX_DATACUBE);
	CGameParticles_ctor(&self->glow);
	CGameParticles_ctor(&self->flares);
	self->base.ent = NULL;
	return self;
}

cEmbeddedFXDataPack *__fastcall cEmbeddedFXDataPack_ctor(cEmbeddedFXDataPack *self) {
	EFXM_SetVtbl(self, VT_EFX_DATAPACK);
	CGameParticles_ctor(&self->glow);
	CGameParticles_ctor(&self->codec);
	CGameParticles_ctor(&self->laser);
	CGameTrail_ctor(&self->trail);
	self->base.ent = NULL;
	self->on = 0;
	self->b0a = 0;
	self->i0c = 0;
	return self;
}

cEmbeddedFXExtraLife *__fastcall cEmbeddedFXExtraLife_ctor(cEmbeddedFXExtraLife *self) {
	EFXM_SetVtbl(self, VT_EFX_EXTRALIFE);
	CGameParticles_ctor(&self->glow);
	CGameTrail_ctor(&self->trail);
	self->base.ent = NULL;
	return self;
}

cEmbeddedFXFlag *__fastcall cEmbeddedFXFlag_ctor(cEmbeddedFXFlag *self) {
	EFXM_SetVtbl(self, VT_EFX_FLAG);
	CGameParticles_ctor(&self->particles);
	self->base.ent = NULL;
	self->t = 0.0f;
	self->scale = 1.0f;
	return self;
}

cEmbeddedFXItem *__fastcall cEmbeddedFXItem_ctor(cEmbeddedFXItem *self) {
	return EFXM_Ctor10(self, VT_EFX_ITEM);
}

cEmbeddedFXMatrix *__fastcall cEmbeddedFXMatrix_ctor(cEmbeddedFXMatrix *self) {
	EFXM_SetVtbl(self, VT_EFX_MATRIX);
	CGameParticles_ctor(&self->glow);
	CGameParticles_ctor(&self->flares);
	CGameTrail_ctor(&self->trail);
	self->base.ent = NULL;
	self->isMatrix = 0;
	return self;
}

cEmbeddedFXMeetingPoint *__fastcall cEmbeddedFXMeetingPoint_ctor(cEmbeddedFXMeetingPoint *self) {
	EFXM_SetVtbl(self, VT_EFX_MEETINGPOINT);
	CGameParticles_ctor(&self->light);
	CGameParticles_ctor(&self->flares);
	CGameParticles_ctor(&self->countDown);
	self->base.ent = NULL;
	self->i0c0 = 0;
	self->countT = -1.0f;
	self->countA = 1.9f;
	self->countB = 0.0f;
	return self;
}

cEmbeddedFXMoto *__fastcall cEmbeddedFXMoto_ctor(cEmbeddedFXMoto *self) {
	EFXM_SetVtbl(self, VT_EFX_MOTO);
	EFXM_ParticlesCtor(self->engine, FXMOTO_ENGINES);
	EFXM_ParticlesCtor(self->down, FXMOTO_DOWN);
	EFXM_ParticlesCtor(self->tube, FXMOTO_TUBES);
	self->base.ent = NULL;
	return self;
}

cEmbeddedFXTreasure *__fastcall cEmbeddedFXTreasure_ctor(cEmbeddedFXTreasure *self) {
	EFXM_SetVtbl(self, VT_EFX_TREASURE);
	CGameParticles_ctor(&self->glow);
	CGameTrail_ctor(&self->trail);
	self->base.ent = NULL;
	self->type = 0;
	self->scale = 1.0f;
	return self;
}

/* ---- destructors ---- */

void __fastcall cEmbeddedFXAmplifier_dtor(cEmbeddedFXAmplifier *self) {
	self->base.vftable = Game_Ptr(VT_EFX_AMPLIFIER);
	CGameTrail_dtor(&self->trail);
	CGameParticles_Release(&self->flares);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXBattery_dtor(cEmbeddedFXBattery *self) {
	self->base.vftable = Game_Ptr(VT_EFX_BATTERY);
	EFXM_TrailsDtor(self->trails, FXBATTERY_TRAILS);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXBossShield_dtor(cEmbeddedFXBossShield *self) {
	self->base.vftable = Game_Ptr(VT_EFX_BOSSSHIELD);
	EFXM_TrailsDtor(self->trails, FXBOSSSHIELD_TRAILS);
	CGameParticles_Release(&self->particles);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXCapsule_dtor(cEmbeddedFXCapsule *self) {
	self->base.vftable = Game_Ptr(VT_EFX_CAPSULE);
	EFXM_ParticlesDtor(self->motors, FXCAPSULE_MOTORS);
	cEmbeddedFX_dtor(&self->base);
}

/* Coin, Conversor, Flag and Item */
static void EFXM_Dtor10(cEmbeddedFX *self, uint32_t vt) {
	self->vftable = Game_Ptr(vt);
	CGameParticles_Release((CGameParticles *)((byte *)self + 0x10));
	cEmbeddedFX_dtor(self);
}

void __fastcall cEmbeddedFXCoin_dtor(cEmbeddedFXCoin *self) {
	EFXM_Dtor10(&self->base, VT_EFX_COIN);
}

void __fastcall cEmbeddedFXConversor_dtor(cEmbeddedFXConversor *self) {
	EFXM_Dtor10(&self->base, VT_EFX_CONVERSOR);
}

void __fastcall cEmbeddedFXDTritusDesktop_dtor(cEmbeddedFXDTritusDesktop *self) {
	self->base.vftable = Game_Ptr(VT_EFX_DTRITUSDESKTOP);
	CGameTrail_dtor(&self->trail);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXDataCube_dtor(cEmbeddedFXDataCube *self) {
	self->base.vftable = Game_Ptr(VT_EFX_DATACUBE);
	CGameParticles_Release(&self->flares);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXDataPack_dtor(cEmbeddedFXDataPack *self) {
	self->base.vftable = Game_Ptr(VT_EFX_DATAPACK);
	CGameTrail_dtor(&self->trail);
	CGameParticles_Release(&self->laser);
	CGameParticles_Release(&self->codec);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXExtraLife_dtor(cEmbeddedFXExtraLife *self) {
	self->base.vftable = Game_Ptr(VT_EFX_EXTRALIFE);
	CGameTrail_dtor(&self->trail);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXFlag_dtor(cEmbeddedFXFlag *self) {
	EFXM_Dtor10(&self->base, VT_EFX_FLAG);
}

void __fastcall cEmbeddedFXItem_dtor(cEmbeddedFXItem *self) {
	EFXM_Dtor10(&self->coin.base, VT_EFX_ITEM);
}

void __fastcall cEmbeddedFXMatrix_dtor(cEmbeddedFXMatrix *self) {
	self->base.vftable = Game_Ptr(VT_EFX_MATRIX);
	CGameTrail_dtor(&self->trail);
	CGameParticles_Release(&self->flares);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXMeetingPoint_dtor(cEmbeddedFXMeetingPoint *self) {
	self->base.vftable = Game_Ptr(VT_EFX_MEETINGPOINT);
	CGameParticles_Release(&self->countDown);
	CGameParticles_Release(&self->flares);
	CGameParticles_Release(&self->light);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXMoto_dtor(cEmbeddedFXMoto *self) {
	self->base.vftable = Game_Ptr(VT_EFX_MOTO);
	EFXM_ParticlesDtor(self->tube, FXMOTO_TUBES);
	EFXM_ParticlesDtor(self->down, FXMOTO_DOWN);
	EFXM_ParticlesDtor(self->engine, FXMOTO_ENGINES);
	cEmbeddedFX_dtor(&self->base);
}

void __fastcall cEmbeddedFXTreasure_dtor(cEmbeddedFXTreasure *self) {
	self->base.vftable = Game_Ptr(VT_EFX_TREASURE);
	CGameTrail_dtor(&self->trail);
	CGameParticles_Release(&self->glow);
	cEmbeddedFX_dtor(&self->base);
}

/* ---- vf00: deleting destructors ---- */

cEmbeddedFX *__thiscall cEmbeddedFXAmplifier_Delete(cEmbeddedFXAmplifier *self, byte flags) {
	cEmbeddedFXAmplifier_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXBattery_Delete(cEmbeddedFXBattery *self, byte flags) {
	cEmbeddedFXBattery_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXBossShield_Delete(cEmbeddedFXBossShield *self, byte flags) {
	cEmbeddedFXBossShield_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXCapsule_Delete(cEmbeddedFXCapsule *self, byte flags) {
	cEmbeddedFXCapsule_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXCoin_Delete(cEmbeddedFXCoin *self, byte flags) {
	cEmbeddedFXCoin_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXConversor_Delete(cEmbeddedFXConversor *self, byte flags) {
	cEmbeddedFXConversor_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXDTritusDesktop_Delete(cEmbeddedFXDTritusDesktop *self, byte flags) {
	cEmbeddedFXDTritusDesktop_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXDataCube_Delete(cEmbeddedFXDataCube *self, byte flags) {
	cEmbeddedFXDataCube_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXDataPack_Delete(cEmbeddedFXDataPack *self, byte flags) {
	cEmbeddedFXDataPack_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXExtraLife_Delete(cEmbeddedFXExtraLife *self, byte flags) {
	cEmbeddedFXExtraLife_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXFlag_Delete(cEmbeddedFXFlag *self, byte flags) {
	cEmbeddedFXFlag_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXItem_Delete(cEmbeddedFXItem *self, byte flags) {
	cEmbeddedFXItem_dtor(self);
	return EFXM_Delete(&self->coin.base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXMatrix_Delete(cEmbeddedFXMatrix *self, byte flags) {
	cEmbeddedFXMatrix_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXMeetingPoint_Delete(cEmbeddedFXMeetingPoint *self, byte flags) {
	cEmbeddedFXMeetingPoint_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXMoto_Delete(cEmbeddedFXMoto *self, byte flags) {
	cEmbeddedFXMoto_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

cEmbeddedFX *__thiscall cEmbeddedFXTreasure_Delete(cEmbeddedFXTreasure *self, byte flags) {
	cEmbeddedFXTreasure_dtor(self);
	return EFXM_Delete(&self->base, flags, sizeof(*self));
}

/* ---- vf01: Reset ---- */

static void EFXM_TrailStyle(CGameTrail *t, byte segments, byte type, int texture) {
	CGameTrail_SetMaxTrails(t, segments);
	CGameTrail_SetType(t, type);
	CGameTrail_SetTexture(t, texture);
}

/* Amplifier and DTritusDesktop */
static byte EFXM_ResetTrail(CGameTrail *t) {
	EFXM_TrailStyle(t, 25, 3, 15);
	return 1;
}

byte __fastcall cEmbeddedFXAmplifier_Reset(cEmbeddedFXAmplifier *self) {
	return EFXM_ResetTrail(&self->trail);
}

byte __fastcall cEmbeddedFXBattery_Reset(cEmbeddedFXBattery *self) {
	for (int i = 0; i < FXBATTERY_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 10);
		CGameTrail_SetTexture(t, 9);
		CGameTrail_SetPremul(t, 1);
		int r = crt_rand() & 0x7fff;
		self->f11c[i] = 0.0f;
		self->f128[i] = (float)((double)r * (1.0 / 32767.0) * 0x1.99999cp-1 + 0x1.99999ap-2);
	}
	self->i0c4 = 0;
	return 1;
}

byte __fastcall cEmbeddedFXBossShield_Reset(cEmbeddedFXBossShield *self) {
	for (int i = 0; i < FXBOSSSHIELD_SLOTS; i++) {
		self->f0b8[i] = -1.0f;
	}
	self->i1ca8 = 0;
	self->i1cac = 0;
	for (int i = 0; i < FXBOSSSHIELD_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 31);
		CGameTrail_SetSmooth(t, 0);
		CGameTrail_SetTexture(t, 11);
	}
	self->i1c9c = 0;
	self->f1ca4 = 0.0f;
	return 1;
}

byte __fastcall cEmbeddedFXCoin_Reset(cEmbeddedFXCoin *self) {
	self->scale = 1.0f;
	return 1;
}

byte __fastcall cEmbeddedFXConversor_Reset(cEmbeddedFXConversor *self) {
	self->t = 0.0f;
	return 1;
}

byte __fastcall cEmbeddedFXDTritusDesktop_Reset(cEmbeddedFXDTritusDesktop *self) {
	return EFXM_ResetTrail(&self->trail);
}

byte __fastcall cEmbeddedFXDataPack_Reset(cEmbeddedFXDataPack *self) {
	CGameTrail_SetMaxTrails(&self->trail, 4);
	return 1;
}

byte __fastcall cEmbeddedFXExtraLife_Reset(cEmbeddedFXExtraLife *self) {
	EFXM_TrailStyle(&self->trail, 31, 3, 9);
	CGameTrail_SetPremul(&self->trail, 1);
	self->fcc = 1.0f;
	self->fd0 = 0.0f;
	return 1;
}

byte __fastcall cEmbeddedFXItem_Reset(cEmbeddedFXItem *self) {
	self->coin.scale = 1.0f;
	self->phase = crt_rand() % 512;
	return 1;
}

byte __fastcall cEmbeddedFXMatrix_Reset(cEmbeddedFXMatrix *self) {
	EFXM_TrailStyle(&self->trail, 7, 3, 9);
	CGameTrail_SetPremul(&self->trail, 1);
	self->v178.x = 0.0f;
	self->v178.y = 0.0f;
	self->v178.z = 0.0f;
	self->scale = 1.0f;
	return 1;
}

byte __fastcall cEmbeddedFXMoto_Reset(cEmbeddedFXMoto *self) {
	self->v08.x = 0.0f;
	self->v08.y = 0.0f;
	self->v08.z = 0.0f;
	self->f7d4 = 1.0f;
	self->f7d8 = 0.0f;
	self->b7dc = 0;
	return 1;
}

byte __fastcall cEmbeddedFXTreasure_Reset(cEmbeddedFXTreasure *self) {
	self->fc4 = 0.0f;
	self->fc8 = 0.0f;
	self->fcc = 0.0f;
	EFXM_TrailStyle(&self->trail, 31, 3, 0);
	return 1;
}

/* ---- vf02: Release ---- */

/* Amplifier, Coin, Conversor, Flag, Item, MeetingPoint: the dummy at 0x0c, the particles at 0x10 */
void __fastcall cEmbeddedFXCoin_Release(cEmbeddedFXCoin *self) {
	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
}

void __fastcall cEmbeddedFXBattery_Release(cEmbeddedFXBattery *self) {
	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
	for (int i = 0; i < FXBATTERY_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
	self->i0c4 = 0;
}

/* BossShield: the trails cleared (also its Stop) */
void __fastcall cEmbeddedFXBossShield_Release(cEmbeddedFXBossShield *self) {
	for (int i = 0; i < FXBOSSSHIELD_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
}

void __fastcall cEmbeddedFXCapsule_Release(cEmbeddedFXCapsule *self) {
	CGameParticles_Release(&self->motors[0]);
	CGameParticles_Release(&self->motors[1]);
	CGameParticles_Release(&self->motors[2]);
}

void __fastcall cEmbeddedFXDTritusDesktop_Release(cEmbeddedFXDTritusDesktop *self) {
	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
}

void __fastcall cEmbeddedFXDataCube_Release(cEmbeddedFXDataCube *self) {
	self->dummyGlow = -1;
	self->dummyFlares = -1;
	CGameParticles_Release(&self->glow);
	CGameParticles_Release(&self->flares);
}

void __fastcall cEmbeddedFXDataPack_Release(cEmbeddedFXDataPack *self) {
	self->dummyGlow = -1;
	self->dummyCodec = -1;
	self->dummyLaser = -1;
	CGameParticles_Release(&self->glow);
	CGameParticles_Release(&self->codec);
	CGameParticles_Release(&self->laser);
}

void __fastcall cEmbeddedFXExtraLife_Release(cEmbeddedFXExtraLife *self) {
	self->dummyGlow = -1;
	self->dummyBarra = -1;
	CGameParticles_Release(&self->glow);
}

void __fastcall cEmbeddedFXMatrix_Release(cEmbeddedFXMatrix *self) {
	self->dummyGlow = -1;
	self->dummyFlares = -1;
	CGameParticles_Release(&self->glow);
	CGameParticles_Release(&self->flares);
}

void __fastcall cEmbeddedFXMoto_Release(cEmbeddedFXMoto *self) {
	for (int i = 0; i < FXMOTO_ENGINES; i++) {
		CGameParticles_Release(&self->engine[i]);
	}
	for (int i = 0; i < FXMOTO_DOWN; i++) {
		CGameParticles_Release(&self->down[i]);
	}
	for (int i = 0; i < FXMOTO_TUBES; i++) {
		CGameParticles_Release(&self->tube[i]);
	}
	self->b7dc = 0;
}

void __fastcall cEmbeddedFXTreasure_Release(cEmbeddedFXTreasure *self) {
	self->dummyGlow = -1;
	CGameParticles_Release(&self->glow);
	CGameTrail_Clear(&self->trail);
	self->fc4 = 0.0f;
	self->fc8 = 0.0f;
	self->fcc = 0.0f;
}

/* ---- the trails cleared (vf05 with keep 0) ---- */

void __fastcall cEmbeddedFXAmplifier_Clear(cEmbeddedFXAmplifier *self) {
	CGameTrail_Clear(&self->trail);
}

void __fastcall cEmbeddedFXBattery_Clear(cEmbeddedFXBattery *self) {
	for (int i = 0; i < FXBATTERY_TRAILS; i++) {
		CGameTrail_Clear(&self->trails[i]);
	}
}

void __fastcall cEmbeddedFXDTritusDesktop_Clear(cEmbeddedFXDTritusDesktop *self) {
	CGameTrail_Clear(&self->trail);
}

void __fastcall cEmbeddedFXDataPack_Clear(cEmbeddedFXDataPack *self) {
	CGameTrail_Clear(&self->trail);
}

void __fastcall cEmbeddedFXExtraLife_Clear(cEmbeddedFXExtraLife *self) {
	CGameTrail_Clear(&self->trail);
}

void __fastcall cEmbeddedFXMatrix_Clear(cEmbeddedFXMatrix *self) {
	CGameTrail_Clear(&self->trail);
}

void __fastcall cEmbeddedFXTreasure_Clear(cEmbeddedFXTreasure *self) {
	CGameTrail_Clear(&self->trail);
}

/* ---- vf05: Stop ---- */

void __thiscall cEmbeddedFXAmplifier_Stop(cEmbeddedFXAmplifier *self, byte keep) {
	if (!keep) {
		CGameTrail_Clear(&self->trail);
	}
}

void __thiscall cEmbeddedFXBattery_Stop(cEmbeddedFXBattery *self, byte keep) {
	if (!keep) {
		cEmbeddedFXBattery_Clear(self);
	}
}

void __thiscall cEmbeddedFXDTritusDesktop_Stop(cEmbeddedFXDTritusDesktop *self, byte keep) {
	if (!keep) {
		CGameTrail_Clear(&self->trail);
	}
}

void __thiscall cEmbeddedFXDataPack_Stop(cEmbeddedFXDataPack *self, byte keep) {
	if (!keep) {
		CGameTrail_Clear(&self->trail);
	}
}

void __thiscall cEmbeddedFXExtraLife_Stop(cEmbeddedFXExtraLife *self, byte keep) {
	if (!keep) {
		CGameTrail_Clear(&self->trail);
	}
}

void __thiscall cEmbeddedFXMatrix_Stop(cEmbeddedFXMatrix *self, byte keep) {
	if (!keep) {
		CGameTrail_Clear(&self->trail);
	}
}

void __thiscall cEmbeddedFXTreasure_Stop(cEmbeddedFXTreasure *self, byte keep) {
	if (!keep) {
		CGameTrail_Clear(&self->trail);
	}
}

/* ---- setters (most have no caller in the exe) ---- */

void __thiscall cEmbeddedFXBossShield_SetI1c9c(cEmbeddedFXBossShield *self, int v) {
	self->i1c9c = v;
}

void __thiscall cEmbeddedFXBossShield_SetF1ca4(cEmbeddedFXBossShield *self, float v) {
	self->f1ca4 = v;
}

/* the float at 0xc0: the scale of Coin and Item, t of Conversor and Flag */
void __thiscall cEmbeddedFXCoin_SetScale(cEmbeddedFXCoin *self, float scale) {
	self->scale = scale;
}

/* the int at 0xc4: the color of Coin and Item */
void __thiscall cEmbeddedFXCoin_SetColor(cEmbeddedFXCoin *self, uint32_t color) {
	self->color = color;
}

void __thiscall cEmbeddedFXExtraLife_SetFcc(cEmbeddedFXExtraLife *self, float v) {
	self->fcc = v;
}

void __thiscall cEmbeddedFXMatrix_SetScale(cEmbeddedFXMatrix *self, float scale) {
	self->scale = scale;
}

void __thiscall cEmbeddedFXTreasure_SetScale(cEmbeddedFXTreasure *self, float scale) {
	self->scale = scale;
}

void __thiscall cEmbeddedFXTreasure_SetType(cEmbeddedFXTreasure *self, int type) {
	self->type = type;
}

void __thiscall cEmbeddedFXDataPack_SetOn(cEmbeddedFXDataPack *self, byte on) {
	self->on = on;
}

void __fastcall cEmbeddedFXDataPack_On(cEmbeddedFXDataPack *self) {
	self->on = 1;
}

void __thiscall cEmbeddedFXMoto_SetF7d4(cEmbeddedFXMoto *self, float a, float b) {
	self->f7d4 = a;
	self->b7dc = 1;
	self->f7d8 = b;
}

void __thiscall cEmbeddedFXFlag_SetColor(cEmbeddedFXFlag *self, const char *name) {
	self->scale = 1.0f;
	if (!crt_stricmp(name, "YellowS")) {
		self->color = 0xffff40;
		self->color2 = 0xffff08;
		self->scale = 0.37f;
		return;
	}
	if (!crt_stricmp(name, "Yellow")) {
		self->color = 0xffff40;
		self->color2 = 0xffff08;
		return;
	}
	if (!crt_stricmp(name, "Green")) {
		self->color = 0x40ff40;
		self->color2 = 0x20ff20;
		return;
	}
	if (!crt_stricmp(name, "Red")) {
		self->color = 0xff4040;
		self->color2 = 0xff2020;
	}
}

/* the count down of the meeting point: the light out, a and b for it */
void __thiscall cEmbeddedFXMeetingPoint_SetCountDown(cEmbeddedFXMeetingPoint *self, float a, float b) {
	CGameParticles_KillAll(&self->countDown);
	self->countA = a;
	self->countT = 0.0f;
	self->countB = b;
}

/* the position of the first engine node, animated */
vec3_t *__thiscall cEmbeddedFXMoto_EnginePos(cEmbeddedFXMoto *self, vec3_t *out) {
	matrix_t m;

	CGameModel *model = FX_Vtbl(self->base.ent)->GetModel(self->base.ent);
	CHECK(CGameModel_IsLoaded(model));
	CHECK(self->nodeEngine[0] > -1);
	CGameModel_NodoMatrix(model, &m, self->nodeEngine[0], 0, NULL, 0);
	out->x = m.m[12];
	out->y = m.m[13];
	out->z = m.m[14];
	return out;
}

/* ---- Init: the effect on the model of the object ---- */

/* the owner set and CHECKed, its model */
static CGameModel *EFXM_Owner(cEmbeddedFX *self, cEntity *e) {
	self->ent = e;
	CHECK(e);
	CHECK(self->ent->vftable->IsType(self->ent, TYEN_OBJECT));
	CGameModel *model = FX_Vtbl(self->ent)->GetModel(self->ent);
	CHECK(CGameModel_IsLoaded(model));
	return model;
}

/* the node index into *dummy and CHECKed */
static int EFXM_Node(int *dummy, CGameModel *model, const char *name) {
	*dummy = CGameModel_FindNodoR(model, name);
	CHECK(*dummy > -1);
	return *dummy;
}

/* the particles attached to the node name (its index into *dummy, if given) */
static void EFXM_Attach(CGameParticles *p, int *dummy, CGameModel *model, const char *name) {
	int i;

	if (dummy) {
		i = EFXM_Node(dummy, model, name);
	} else {
		i = CGameModel_FindNodoR(model, name);
		CHECK(i > -1);
	}
	CGameParticles_Attach(p, model, i);
	CHECK(p->SistPart);
}

void __thiscall cEmbeddedFXAmplifier_Init(cEmbeddedFXAmplifier *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);
	vec3_t pos;

	EFXM_Attach(&self->flares, &self->dummyFlares, model, "DOB_Amplificador_Flares");
	for (int i = 0; i < CGameParticles_NumParticles(&self->flares); i++) {
		CGameParticles_GetParticle(&self->flares, i, &pos, NULL, NULL, NULL, NULL);
		CGameParticles_SetParticle(&self->flares, i, pos.x, pos.y, pos.z, 380.0f, 0x30ff1e05, 5, 0.0f);
	}
	EFXM_Node(&self->dummyRoot, model, "OB_Amplificador_Cuerpo");
}

void __thiscall cEmbeddedFXBattery_Init(cEmbeddedFXBattery *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);

	EFXM_Attach(&self->glow, &self->dummyGlow, model, "DOB_Batery_Glow");
	EFXM_Node(&self->dummyRoot, model, "DC_Root");
}

void __thiscall cEmbeddedFXBossShield_Init(cEmbeddedFXBossShield *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);

	EFXM_Attach(&self->particles, NULL, model, "Dummy01");
}

void __thiscall cEmbeddedFXCapsule_Init(cEmbeddedFXCapsule *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);

	EFXM_Attach(&self->motors[0], NULL, model, "DS_CapsulaBill_Motor_01");
	EFXM_Attach(&self->motors[1], NULL, model, "DS_CapsulaBill_Motor_02");
	EFXM_Attach(&self->motors[2], NULL, model, "DS_CapsulaBill_Motor_03");
}

/* Coin and Item (two copies, the factory uses this one for both): the glow on node */
void __thiscall cEmbeddedFXCoin_Init(cEmbeddedFXCoin *self, cEntity *e, const char *node) {
	CGameModel *model = EFXM_Owner(&self->base, e);

	self->scale = 1.0f;
	CHECK(node);
	EFXM_Attach(&self->glow, &self->dummyGlow, model, node);
}

void __thiscall cEmbeddedFXConversor_Init(cEmbeddedFXConversor *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);

	EFXM_Attach(&self->glow, &self->dummyGlow, model, "DOB_Conversor-Mandos_Glow");
}

void __thiscall cEmbeddedFXDTritusDesktop_Init(cEmbeddedFXDTritusDesktop *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);

	EFXM_Attach(&self->glow, &self->dummyGlow, model, "DOB_Interfono_Calling");
	CGameParticles_SetParticle(&self->glow, 0, 0.0f, 0.0f, 0.0f, 8.0f, 0x0cff5c5c, 2, 0.0f);
	CGameParticles_SetParticle(&self->glow, 1, 0.0f, 0.0f, 0.0f, 22.0f, 0x20ff4040, 2, 0.0f);
	CGameParticles_SetParticle(&self->glow, 2, 0.0f, 0.0f, 0.0f, 115.0f, 0x60ff1616, 5, 0.0f);
	EFXM_Node(&self->dummyRoot, model, "OB_Interfono_Base");
}

void __thiscall cEmbeddedFXDataCube_Init(cEmbeddedFXDataCube *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);
	vec3_t pos;
	int n = 0;

	EFXM_Attach(&self->glow, &self->dummyGlow, model, "DOB_DataCube_Glow");
	/* the 8 corners of a cube */
	for (int x = -150; x < 450; x += 300) {
		for (int y = -150; y < 450; y += 300) {
			for (int z = -150; z < 450; z += 300) {
				CGameParticles_SetParticle(&self->glow, n, (float)x, (float)y, (float)z, 750.0f, 0xc3ffaaaa, 2,
					0.0f);
				n++;
			}
		}
	}
	EFXM_Attach(&self->flares, &self->dummyFlares, model, "DOB_DataCube_Flares");
	for (int i = 0; i < CGameParticles_NumParticles(&self->flares); i++) {
		CGameParticles_GetParticle(&self->flares, i, &pos, NULL, NULL, NULL, NULL);
		CGameParticles_SetParticle(&self->flares, i, pos.x * 0.9f, pos.y * 0.9f, pos.z * 0.9f, 375.0f, 0x5cfad246,
			5, 0.0f);
	}
}

void __thiscall cEmbeddedFXDataPack_Init(cEmbeddedFXDataPack *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);
	vec3_t pos;

	EFXM_Node(&self->dummyRoot, model, "DC_Root");
	self->rootPos = *CGameModel_NodoPos(model, &pos, self->dummyRoot, 0);
	EFXM_Attach(&self->glow, &self->dummyGlow, model, "DOB_DataPack_Glow");
	EFXM_Attach(&self->codec, &self->dummyCodec, model, "DOB_DataPack_Letras");
	EFXM_Attach(&self->laser, &self->dummyLaser, model, "DF_Laser");
	self->on = g_DataPackOn != 0;
}

void __thiscall cEmbeddedFXExtraLife_Init(cEmbeddedFXExtraLife *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);

	EFXM_Attach(&self->glow, &self->dummyGlow, model, "DOB_ExtraLife_Glow");
	EFXM_Node(&self->dummyBarra, model, "OB_ExtraLife_Barra_01");
	EFXM_Node(&self->dummyRoot, model, "DC_Root");
}

void __thiscall cEmbeddedFXFlag_Init(cEmbeddedFXFlag *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);

	EFXM_Attach(&self->particles, &self->dummy, model, "DOB_FlapCTF_Glow");
}

void __thiscall cEmbeddedFXMatrix_Init(cEmbeddedFXMatrix *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);

	EFXM_Attach(&self->glow, &self->dummyGlow, model, "DOB_DataCube_Glow");
	EFXM_Attach(&self->flares, &self->dummyFlares, model, "DOB_Matrix_Flares");
	EFXM_Node(&self->dummyRoot, model, "DC_Root");
}

void __thiscall cEmbeddedFXMeetingPoint_Init(cEmbeddedFXMeetingPoint *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);
	vec3_t pos;

	EFXM_Attach(&self->light, &self->dummy, model, "DF_Light");
	EFXM_Attach(&self->flares, NULL, model, "DOB_MeetingPoint_Flares");
	for (int i = 0; i < 2; i++) {
		CGameParticles_GetParticle(&self->flares, i, &pos, NULL, NULL, NULL, NULL);
		float x = pos.x + 0.0f;
		float y = pos.y + 50.0f;
		float z = pos.z + 0.0f;
		uint32_t c = (uint32_t)(crt_rand() % 10 + 0x78) << 8;
		c |= (uint32_t)(crt_rand() % 30 + 0xf0) << 16;
		c |= (uint32_t)(crt_rand() % 10 + 0x78) | 0x10000000;
		CGameParticles_SetParticle(&self->flares, i, x, y, z, 115.0f, c, 2, 0.0f);
		x = pos.x + 0.0f;
		y = pos.y + 50.0f;
		z = pos.z + 0.0f;
		c = (uint32_t)(crt_rand() % 30 + 0xdc) << 16;
		c |= ((uint32_t)(crt_rand() % 10) | 0x5c0000) << 8;
		c |= (uint32_t)(crt_rand() % 10);
		CGameParticles_SetParticle(&self->flares, i + 2, x, y, z, 825.0f, c, 5, 0.0f);
	}
	EFXM_Attach(&self->countDown, NULL, model, "DF_Light_02");
}

void __thiscall cEmbeddedFXMoto_Init(cEmbeddedFXMoto *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);
	char name[128];

	for (int i = 0; i < FXMOTO_ENGINES; i++) {
		Str_PrintfQuiet(name, 0xff, "DF_MotorTrust_%02d", i + 1);
		self->nodeEngine[i] = CGameModel_FindNodoR(model, name);
		CHECK(self->nodeEngine[0] > -1);
		CGameParticles_Attach(&self->engine[i], model, self->nodeEngine[i]);
		CHECK(self->engine[i].SistPart);
	}
	for (int i = 0; i < FXMOTO_DOWN; i++) {
		Str_PrintfQuiet(name, 0xff, "DF_MotorTrust_%02d", i + 4);
		EFXM_Attach(&self->down[i], NULL, model, name);
	}
	for (int i = 0; i < FXMOTO_TUBES; i++) {
		Str_PrintfQuiet(name, 0xff, "DF_EscapeTube_%02d", i + 1);
		EFXM_Attach(&self->tube[i], NULL, model, name);
	}
}

void __thiscall cEmbeddedFXTreasure_Init(cEmbeddedFXTreasure *self, cEntity *e) {
	CGameModel *model = EFXM_Owner(&self->base, e);

	EFXM_Attach(&self->glow, &self->dummyGlow, model, "DOB_Treasure_Glow");
	EFXM_Node(&self->dummyRoot, model, "DC_Root");
}

/* ---- creation ---- */

typedef void *(__fastcall *EFXM_Ctor)(void *self);

/* the old effect released and deleted, the new one made and reset */
static cEmbeddedFX *EFXM_New(cEmbeddedFX *cur, uint32_t size, EFXM_Ctor ctor) {
	if (cur) {
		cur->vftable->Release(cur);
		cur->vftable->Delete(cur, 1);
	}
	void *p = operator_new(size);
	cur = p ? ctor(p) : NULL;
	cur->vftable->Reset(cur);
	return cur;
}

#define EFXM_NEW(type) ((type *)(fx_ = EFXM_New(fx_, sizeof(type), (EFXM_Ctor)type##_ctor)))

static int EFXM_Is(const char *model, const char *name) {
	return !crt_stricmp(model, name);
}

void EmbeddedFXMisc_Create(cEntity *obj, const char *model, cEmbeddedFX **fx) {
	cEmbeddedFX *fx_ = *fx;

	CHECK(obj);
	if (EFXM_Is(model, "DataPack.M3D")) {
		cEmbeddedFXDataPack_Init(EFXM_NEW(cEmbeddedFXDataPack), obj);
	} else if (EFXM_Is(model, "DataCube.m3d")) {
		cEmbeddedFXDataCube_Init(EFXM_NEW(cEmbeddedFXDataCube), obj);
	} else if (EFXM_Is(model, "Capsule.M3D")) {
		cEmbeddedFXCapsule_Init(EFXM_NEW(cEmbeddedFXCapsule), obj);
	} else if (EFXM_Is(model, "MatrixInt.m3d") || EFXM_Is(model, "Matrix.m3d")) {
		cEmbeddedFXMatrix *m = EFXM_NEW(cEmbeddedFXMatrix);
		cEmbeddedFXMatrix_Init(m, obj);
		m->scale = 1.0f;
		if (EFXM_Is(model, "Matrix.m3d")) {
			m->isMatrix = 1;
		}
	} else if (EFXM_Is(model, "MatrixExt.m3d")) {
		cEmbeddedFXMatrix *m = EFXM_NEW(cEmbeddedFXMatrix);
		cEmbeddedFXMatrix_Init(m, obj);
		m->scale = 6.0f;
	} else if (EFXM_Is(model, "Amplifier.m3d")) {
		cEmbeddedFXAmplifier_Init(EFXM_NEW(cEmbeddedFXAmplifier), obj);
	} else if (EFXM_Is(model, "DTritusDesktop.m3d")) {
		cEmbeddedFXDTritusDesktop_Init(EFXM_NEW(cEmbeddedFXDTritusDesktop), obj);
	} else if (EFXM_Is(model, "MeetingPoint.m3d") || EFXM_Is(model, "MeetingPoint-Race.m3d")) {
		cEmbeddedFXMeetingPoint_Init(EFXM_NEW(cEmbeddedFXMeetingPoint), obj);
	} else if (EFXM_Is(model, "ExtraLife.m3d")) {
		cEmbeddedFXExtraLife_Init(EFXM_NEW(cEmbeddedFXExtraLife), obj);
	} else if (EFXM_Is(model, "Flag.m3d") || EFXM_Is(model, "FlagGreen.m3d") || EFXM_Is(model, "FlagRed.m3d")
		|| EFXM_Is(model, "FlagPl.m3d")) {
		cEmbeddedFXFlag *f = EFXM_NEW(cEmbeddedFXFlag);
		cEmbeddedFXFlag_Init(f, obj);
		if (EFXM_Is(model, "Flag.m3d")) {
			cEmbeddedFXFlag_SetColor(f, "Yellow");
		} else if (EFXM_Is(model, "FlagGreen.m3d")) {
			cEmbeddedFXFlag_SetColor(f, "Green");
		} else if (EFXM_Is(model, "FlagRed.m3d")) {
			cEmbeddedFXFlag_SetColor(f, "Red");
		} else if (EFXM_Is(model, "FlagPl.m3d")) {
			cEmbeddedFXFlag_SetColor(f, "YellowS");
		}
	} else if (EFXM_Is(model, "Moto.M3D")) {
		cEmbeddedFXMoto_Init(EFXM_NEW(cEmbeddedFXMoto), obj);
	} else if (EFXM_Is(model, "BossShield.M3D")) {
		cEmbeddedFXBossShield_Init(EFXM_NEW(cEmbeddedFXBossShield), obj);
	} else if (EFXM_Is(model, "Conversor.m3d")) {
		cEmbeddedFXConversor_Init(EFXM_NEW(cEmbeddedFXConversor), obj);
	} else if (EFXM_Is(model, "Battery.m3d")) {
		cEmbeddedFXBattery_Init(EFXM_NEW(cEmbeddedFXBattery), obj);
	} else if (EFXM_Is(model, "TreasureA.m3d") || EFXM_Is(model, "TreasureB.m3d") || EFXM_Is(model, "TreasureC.m3d")
		|| EFXM_Is(model, "TreasureA-Ext.m3d")) {
		cEmbeddedFXTreasure *t = EFXM_NEW(cEmbeddedFXTreasure);
		cEmbeddedFXTreasure_Init(t, obj);
		if (EFXM_Is(model, "TreasureA.m3d")) {
			t->scale = 1.0f;
			t->type = 1;
		} else if (EFXM_Is(model, "TreasureA-Ext.m3d")) {
			t->scale = 8.0f;
			t->type = 1;
		} else if (EFXM_Is(model, "TreasureB.m3d")) {
			t->scale = 1.0f;
			t->type = 2;
		} else if (EFXM_Is(model, "TreasureC.m3d")) {
			t->scale = 1.0f;
			t->type = 3;
		}
	} else if (EFXM_Is(model, "Coin1.m3d") || EFXM_Is(model, "Coin2.m3d") || EFXM_Is(model, "Coin3.m3d")
		|| EFXM_Is(model, "Coin4.m3d") || EFXM_Is(model, "Dollar.m3d") || EFXM_Is(model, "BCoin1.m3d")
		|| EFXM_Is(model, "BCoin2.m3d") || EFXM_Is(model, "BCoin3.m3d")) {
		cEmbeddedFXCoin *c = EFXM_NEW(cEmbeddedFXCoin);
		/* the coins of the B set are bigger */
		float scale = 1.0f;
		const char *node = EFXM_EMPTY;
		uint32_t color;
		if (EFXM_Is(model, "Coin1.m3d")) {
			node = "DOB_Coin1_Glow";
			color = 0x9746ff;
		} else if (EFXM_Is(model, "Coin2.m3d")) {
			node = "DOB_Coin2_Glow";
			color = 0xa0ff6e;
		} else if (EFXM_Is(model, "Coin3.m3d")) {
			node = "DOB_Coin3_Glow";
			color = 0xff91fa;
		} else if (EFXM_Is(model, "Coin4.m3d")) {
			node = "DOB_Coin4_Glow";
			color = 0xac663c;
		} else if (EFXM_Is(model, "Dollar.m3d")) {
			node = "DOB_Dollar_Glow";
			color = 0xe7cc82;
		} else if (EFXM_Is(model, "BCoin1.m3d")) {
			scale = 8.0f;
			node = "DOB_Coin1B_Glow";
			color = 0x9746ff;
		} else if (EFXM_Is(model, "BCoin2.m3d")) {
			scale = 8.0f;
			node = "DOB_Coin2B_Glow";
			color = 0xa0ff6e;
		} else if (EFXM_Is(model, "BCoin3.m3d")) {
			scale = 8.0f;
			node = "DOB_Coin3B_Glow";
			color = 0xff91fa;
		} else {
			/* not reached; the original leaves the entity pointer here */
			color = (uint32_t)(uintptr_t)obj;
		}
		cEmbeddedFXCoin_Init(c, obj, node);
		c->scale = scale;
		c->color = color;
	} else if (EFXM_Is(model, "Life.m3d") || EFXM_Is(model, "Ener.m3d") || EFXM_Is(model, "Miss.m3d")
		|| EFXM_Is(model, "Part.m3d")
		|| (*model == 'v' && (EFXM_Is(model, "vAim.m3d") || EFXM_Is(model, "vSpd.m3d")))
		|| (*model == 'd' && (EFXM_Is(model, "dAim.m3d") || EFXM_Is(model, "dBom.m3d")))
		|| (*model == 'i' && (EFXM_Is(model, "iSiz.m3d") || EFXM_Is(model, "iXtr.m3d")))
		|| (*model == 's' && (EFXM_Is(model, "sRck.m3d") || EFXM_Is(model, "sRot.m3d")))
		|| (*model == 't' && (EFXM_Is(model, "tAim.m3d") || EFXM_Is(model, "tTra.m3d")))
		|| (*model == 'a' && (EFXM_Is(model, "aBom.m3d") || EFXM_Is(model, "aSpd.m3d")))) {
		cEmbeddedFXItem *it = EFXM_NEW(cEmbeddedFXItem);
		/* the color by the kind: life, energy (t, a), missiles (s, i), parts (v, d) */
		uint32_t color;
		if (EFXM_Is(model, "Life.m3d")) {
			color = 0xb05eb4;
		} else if (EFXM_Is(model, "Ener.m3d") || *model == 't' || *model == 'a') {
			color = 0x82b400;
		} else if (EFXM_Is(model, "Miss.m3d") || *model == 's' || *model == 'i') {
			color = 0xb4060c;
		} else if (EFXM_Is(model, "Part.m3d") || *model == 'v' || *model == 'd') {
			color = 0x809bb4;
		} else {
			/* not reached; the original leaves the new object pointer here */
			color = (uint32_t)(uintptr_t)it;
		}
		/* the Init of the coins, not the one of the items */
		cEmbeddedFXCoin_Init(&it->coin, obj, "DC_Motion");
		it->coin.scale = 1.0f;
		it->coin.color = color;
	}
	*fx = fx_;
}

/* ---- the network messages and the boss shield switch ---- */

/* the effect of the "Obj" entity name, CHECKed */
static cEmbeddedFX *EFXM_ObjFX(const char *name) {
	cEntity *v = World_FindEntity(name);

	CHECK(v);
	CHECK(v->vftable->IsType(v, TYEN_OBJECT));
	return Entity_EmbeddedFX(v);
}

/* the entity BossShield: f1ca4 to 1 */
void EmbeddedFXMisc_BossShieldSet(void) {
	cEmbeddedFXBossShield *fx = (cEmbeddedFXBossShield *)EFXM_ObjFX("BossShield");

	CHECK(fx);
	fx->f1ca4 = 1.0f;
}

/* the entity name and the value of i1c9c */
void EmbeddedFXMisc_BossShieldMsg(cAbstractFile *f) {
	char name[128];
	int v = 0;

	cAbstractFile_ReadString(f, name, sizeof(name));
	f->vftable->Read(f, &v, 4);
	cEmbeddedFXBossShield *fx = (cEmbeddedFXBossShield *)EFXM_ObjFX(name);
	CHECK(fx);
	fx->i1c9c = v;
}

/* the entity name, a and the count of the meeting point count down (b = count * 220) */
void EmbeddedFXMisc_MeetingPointMsg(cAbstractFile *f) {
	char name[128];
	float a;
	int count = 0;

	cAbstractFile_ReadString(f, name, sizeof(name));
	f->vftable->Read(f, &a, 4);
	f->vftable->Read(f, &count, 4);
	cEmbeddedFXMeetingPoint *fx = (cEmbeddedFXMeetingPoint *)EFXM_ObjFX(name);
	CHECK(fx);
	cEmbeddedFXMeetingPoint_SetCountDown(fx, a, (float)count * 220.0f);
}
