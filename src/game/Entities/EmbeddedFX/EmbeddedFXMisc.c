/*
 * Entities/EmbeddedFX/EmbeddedFXMisc.cpp of the game: the effects embedded in the object entities, see
 * include/scrap/embeddedfxmisc.h.
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/embeddedfxmisc.h"
#include "scrap/filesys.h"
#include "scrap/fxmisc.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/scorer.h"
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
	self->t = 0.0f;
	self->arcT = 0.0f;
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
	self->shown = 0;
	self->level = 0.0f;
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
	self->t = 0.0f;
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
		self->life[i] = 0.0f;
		self->delay[i] = (float)((double)r * (1.0 / 32767.0) * 0x1.99999cp-1 + 0x1.99999ap-2);
	}
	self->sparkT = 0.0f;
	return 1;
}

byte __fastcall cEmbeddedFXBossShield_Reset(cEmbeddedFXBossShield *self) {
	for (int i = 0; i < FXBOSSSHIELD_SLOTS; i++) {
		self->t[i] = -1.0f;
	}
	self->spin = 0.0f;
	self->f1cac = 0.0f;
	for (int i = 0; i < FXBOSSSHIELD_TRAILS; i++) {
		CGameTrail *t = &self->trails[i];
		CGameTrail_SetMaxTrails(t, 31);
		CGameTrail_SetSmooth(t, 0);
		CGameTrail_SetTexture(t, 11);
	}
	self->mode = 0;
	self->hit = 0.0f;
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
	self->scale = 1.0f;
	self->t = 0.0f;
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
	self->timer[0] = 0.0f;
	self->timer[1] = 0.0f;
	self->timer[2] = 0.0f;
	self->scale = 1.0f;
	return 1;
}

byte __fastcall cEmbeddedFXMoto_Reset(cEmbeddedFXMoto *self) {
	self->t[0] = 0.0f;
	self->t[1] = 0.0f;
	self->t[2] = 0.0f;
	self->f7d4 = 1.0f;
	self->f7d8 = 0.0f;
	self->b7dc = 0;
	return 1;
}

byte __fastcall cEmbeddedFXTreasure_Reset(cEmbeddedFXTreasure *self) {
	self->timer = 0.0f;
	self->dur = 0.0f;
	self->life = 0.0f;
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
	self->sparkT = 0.0f;
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
	self->timer = 0.0f;
	self->dur = 0.0f;
	self->life = 0.0f;
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

void __thiscall cEmbeddedFXBossShield_SetMode(cEmbeddedFXBossShield *self, int mode) {
	self->mode = mode;
}

void __thiscall cEmbeddedFXBossShield_SetHit(cEmbeddedFXBossShield *self, float hit) {
	self->hit = hit;
}

/* the float at 0xc0: the scale of Coin and Item, t of Conversor and Flag */
void __thiscall cEmbeddedFXCoin_SetScale(cEmbeddedFXCoin *self, float scale) {
	self->scale = scale;
}

/* the int at 0xc4: the color of Coin and Item */
void __thiscall cEmbeddedFXCoin_SetColor(cEmbeddedFXCoin *self, uint32_t color) {
	self->color = color;
}

void __thiscall cEmbeddedFXExtraLife_SetScale(cEmbeddedFXExtraLife *self, float scale) {
	self->scale = scale;
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

/* the shield of the entity BossShield hit */
void EmbeddedFXMisc_BossShieldHit(void) {
	cEmbeddedFXBossShield *fx = (cEmbeddedFXBossShield *)EFXM_ObjFX("BossShield");

	CHECK(fx);
	fx->hit = 1.0f;
}

/* the entity name and the mode of its shield */
void EmbeddedFXMisc_BossShieldMsg(cAbstractFile *f) {
	char name[128];
	int v = 0;

	cAbstractFile_ReadString(f, name, sizeof(name));
	f->vftable->Read(f, &v, 4);
	cEmbeddedFXBossShield *fx = (cEmbeddedFXBossShield *)EFXM_ObjFX(name);
	CHECK(fx);
	fx->mode = v;
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

/* ---- vf03: Update ---- */

/* the DataPack engine variables */
#define g_DataPackInterTime GAME_DATA(float, 0x4473ac)
#define g_DataPackLaserLong GAME_DATA(float, 0x4473d4)
#define g_DataPackLaserSize GAME_DATA(float, 0x44742c)

/*
 * The model if it is loaded, visible and without a model effect (*visible set), else NULL (*visible cleared, the
 * trail too if given).
 */
static CGameModel *EFXM_Shown(cEmbeddedFX *self, byte *visible, CGameTrail *trail) {
	CGameModel *model = FX_Vtbl(self->ent)->GetModel(self->ent);

	if (!CGameModel_IsLoaded(model)) {
		return NULL;
	}
	CHECK(CGameModel_IsLoaded(model));
	byte vis = CGameModel_IsVisible(model);
	if (vis && !CGameModel_GetEffect(model, NULL)) {
		*visible = vis;
		return model;
	}
	if (*visible) {
		if (trail) {
			CGameTrail_Clear(trail);
		}
		*visible = 0;
	}
	return NULL;
}

/* the model if it is loaded and visible (*visible set); else NULL, the trail cleared when it was visible */
static CGameModel *EFXM_ShownTrail(cEmbeddedFX *self, byte *visible, CGameTrail *trail) {
	CGameModel *model = FX_Vtbl(self->ent)->GetModel(self->ent);

	if (!CGameModel_IsLoaded(model)) {
		return NULL;
	}
	CHECK(CGameModel_IsLoaded(model));
	byte vis = CGameModel_IsVisible(model);
	if (!vis) {
		if (*visible != vis) {
			if (trail) {
				CGameTrail_Clear(trail);
			}
			*visible = 0;
		}
		return NULL;
	}
	*visible = vis;
	return model;
}

static inline uint32_t EFXM_Alpha(uint32_t color, int a) {
	return (color & 0xffffff) | ((uint32_t)a << 24);
}

/* the alpha of a flash at frac (0..1): 0xff + 255 * (2 (1 - f) - 3) (1 - f)^2 */
static int EFXM_FlashAlpha(float frac) {
	float f1 = 1.0f - frac;
	float e = (f1 + f1 - 3.0f) * (f1 * f1) * -255.0f;
	return 0xff - (int)e;
}

static inline int EFXM_Min255(int a) {
	return a > 0xff ? 0xff : a;
}


void __thiscall cEmbeddedFXCoin_Update(cEmbeddedFXCoin *self, float dt) {
	CGameParticles *p = &self->glow;

	(void)dt;
	if (!EFXM_Shown(&self->base, &self->visible, NULL)) {
		return;
	}
	int n = CGameParticles_NumParticles(p);
	int r = crt_rand() % 16;
	CGameParticles_SetParticle(p, 0, 0.0f, 0.0f, 0.0f, self->scale * 160.0f, EFXM_Alpha(self->color, r - 0x60), 5,
		0.0f);
	r = crt_rand() % 8;
	CGameParticles_SetParticle(p, 1, 0.0f, 0.0f, 0.0f, self->scale * 55.0f, EFXM_Alpha(self->color, r - 0x1a), 2,
		0.0f);
	if (n < 4) {
		return;
	}
	r = crt_rand() % 8;
	CGameParticles_SetParticle(p, 2, -25.0f, 0.0f, 0.0f, self->scale * 55.0f, EFXM_Alpha(self->color, r - 0x1f), 2,
		0.0f);
	r = crt_rand() % 8;
	CGameParticles_SetParticle(p, 3, 25.0f, 0.0f, 0.0f, self->scale * 55.0f, EFXM_Alpha(self->color, r - 0x1a), 2,
		0.0f);
}

/* the glow pulses with the world time, the phase differs per item */
void __thiscall cEmbeddedFXItem_Update(cEmbeddedFXItem *self, float dt) {
	cEmbeddedFXCoin *c = &self->coin;
	CGameParticles *p = &c->glow;

	(void)dt;
	if (!EFXM_Shown(&c->base, &c->visible, NULL)) {
		return;
	}
	float t = Timer_GetTime(&g_pWorld->timer);
	int k = (self->phase - (int)(t * -650.0f)) % 511 - 0xff;
	float f = (float)(k < 0 ? -k : k) / 255.0f;
	CGameParticles_SetAlpha(p, f * 0.8f + 0.2f);
	int r = crt_rand() % 8;
	f = f * 250.0f;
	CGameParticles_SetParticle(p, 0, 0.0f, 0.0f, 0.0f, (f + 2150.0f) * c->scale, EFXM_Alpha(c->color, r + 0x2a), 2,
		0.0f);
	float rot = (float)(EFXM_Rnd() * 6.2831854820251465 + 0.0);
	r = crt_rand() % 32;
	CGameParticles_SetParticle(p, 1, 0.0f, 0.0f, 0.0f, (f + 2100.0f) * c->scale, EFXM_Alpha(c->color, r - 0x7c), 1,
		rot);
}

/* a pulse in the middle and 9 glows on a sphere around it */
void __thiscall cEmbeddedFXConversor_Update(cEmbeddedFXConversor *self, float dt) {
	CGameParticles *p = &self->glow;
	vec3_t out;

	if (!EFXM_Shown(&self->base, &self->visible, NULL)) {
		return;
	}
	CGameParticles_NumParticles(p);
	float t = dt * 3.0f + self->t;
	self->t = t;
	float c = (float)Game_Cos((double)t);
	float a = (float)__builtin_fabs((double)c);
	int r = crt_rand() % 24;
	CGameParticles_SetParticle(p, 0, 0.0f, 0.0f, 0.0f, 180.0f,
		((uint32_t)(r - (int)(a * -60.0f) + 0x50) << 24) | 0x2337ff, 5, 0.0f);
	int k = (int)(a * -80.0f);
	float size = a * 10.0f + 70.0f;
	int n = 1;
	float ax = 0.0f;
	do {
		float az = 0.0f;
		do {
			vec3_t rot = { ax, 0.0f, az };
			vec3_t up = { 0.0f, 1.0f, 0.0f };
			Vec3_Rotate(&out, &up, &rot);
			float x = out.x * 26.0f;
			float y = out.y * 26.0f;
			float z = out.z * 26.0f;
			r = crt_rand() % 12;
			CGameParticles_SetParticle(p, n, x, y, z, size, ((uint32_t)(r - k - 0x74) << 24) | 0x1840ff, 2, 0.0f);
			n++;
			az += 2.0943952f;
		} while (6.2831855f > az);
		ax += 2.0943952f;
	} while (6.2831855f > ax);
}

/* the alpha of segment i of the electric arcs at t (0..1) */
static int EFXM_ArcAlpha(int i, float t) {
	if (i == 0) {
		return 0xff - (int)(t * 239.0f);
	}
	if (i == 2) {
		return 0x10 - (int)(t * -239.0f);
	}
	return 0x10;
}

/* the two points of an arc segment: base + a * d, -/+ b * kb * w, width w * kw */
static void EFXM_ArcAdd(CGameTrail *trail, const vec3_t *a, const vec3_t *b, const vec3_t *base, float d, float w,
	float kb, float kw, uint32_t color) {
	vec3_t p = { a->x * d + base->x, a->y * d + base->y, a->z * d + base->z };
	vec3_t q;

	q.x = p.x - b->x * kb * w;
	q.y = p.y - b->y * kb * w;
	q.z = p.z - b->z * kb * w;
	float width = w * kw;
	CGameTrail_Add(trail, q, width, color);
	q.x = b->x * kb * w + p.x;
	q.y = b->y * kb * w + p.y;
	q.z = b->z * kb * w + p.z;
	CGameTrail_Add(trail, q, width, color);
}

/* the axes z and x turned by rot and then by the angles ang into a and b */
static void EFXM_ArcAxes(vec3_t *a, vec3_t *b, const vec3_t *rot, const vec3_t *ang) {
	vec3_t dir, tmp;

	dir.x = 0.0f;
	dir.y = 0.0f;
	dir.z = 1.0f;
	Vec3_Rotate(a, Vec3_Rotate(&tmp, &dir, rot), ang);
	dir.x = 1.0f;
	dir.y = 0.0f;
	dir.z = 0.0f;
	Vec3_Rotate(b, Vec3_Rotate(&tmp, &dir, rot), ang);
}

/* the trail started at pos (the angles of the matrix into ang, the arc time advanced) */
static void EFXM_ArcStart(CGameTrail *trail, const matrix_t *m, vec3_t pos, vec3_t *ang, float *t, float dt) {
	Matrix_GetEuler(m, ang);
	CGameTrail_Reset(trail);
	CGameTrail_SetOrient(trail, pos, 1);
	float v = dt * 4.0f + *t;
	*t = v;
	if (v >= 1.0f) {
		*t = 0.0f;
	}
}

/* four arcs of three segments around the body */
void __thiscall cEmbeddedFXAmplifier_Update(cEmbeddedFXAmplifier *self, float dt) {
	matrix_t m;
	vec3_t ang, pos, a, b;

	CGameModel *model = EFXM_ShownTrail(&self->base, &self->visible, &self->trail);
	if (!model) {
		return;
	}
	CGameModel_NodoMatrix(model, &m, self->dummyRoot, 1, NULL, 0);
	pos.x = m.m[12];
	pos.y = m.m[13];
	pos.z = m.m[14];
	EFXM_ArcStart(&self->trail, &m, pos, &ang, &self->t, dt);
	for (int j = 0; j < 4; j++) {
		vec3_t rot = { 0.0f, (float)j * 1.5707964f, 0.0f };
		EFXM_ArcAxes(&a, &b, &rot, &ang);
		for (int i = 0; i < 3; i++) {
			float t = self->t;
			float s = (float)i + t;
			float w = s * 0.5f + 0.8f;
			float d = s * 180.0f + 200.0f;
			uint32_t color = ((uint32_t)EFXM_ArcAlpha(i, t) << 24) | 0xff1405;
			EFXM_ArcAdd(&self->trail, &a, &b, &pos, d, w, 180.0f, 360.0f, color);
		}
	}
}

/* the calling light blinks while the owner calls; the arc while the animation DTritusDesktop_Calling plays */
void __thiscall cEmbeddedFXDTritusDesktop_Update(cEmbeddedFXDTritusDesktop *self, float dt) {
	matrix_t m;
	vec3_t ang, rot, pos, a, b;
	int anim;

	CGameModel *model = EFXM_ShownTrail(&self->base, &self->visible, &self->trail);
	if (!model) {
		return;
	}
	self->t = dt + self->t;
	cEntity *e = self->base.ent;
	byte on = 0;
	if (e && e->vftable->IsType(e, TYEN_WITH_LIFE) && *((byte *)self->base.ent + 0x383)) {
		on = (byte)((int)(self->t * 3.0f) & 1);
	}
	CGameParticles_SetVisible(&self->glow, on);
	anim = -1;
	CGameModel_ChanInfo(model, 0, NULL, &anim);
	if (anim > -1 && !strcmp(CGameModel_AnimName(model, anim), "DTritusDesktop_Calling")) {
		CGameModel_NodoMatrix(model, &m, self->dummyRoot, 1, NULL, 0);
		vec3_t off = { -60.0f, 35.0f, -35.0f };
		vec3_t tmp;
		Vec3_Rotate(&tmp, &off, CGameModel_GetRot(model, &rot));
		pos.x = tmp.x + m.m[12];
		pos.y = tmp.y + m.m[13];
		pos.z = tmp.z + m.m[14];
		EFXM_ArcStart(&self->trail, &m, pos, &ang, &self->arcT, dt);
		vec3_t r2 = { 0.0f, 2.7015622f, 0.0f };
		EFXM_ArcAxes(&a, &b, &r2, &ang);
		for (int i = 0; i < 3; i++) {
			float t = self->arcT;
			float s = (float)i + t;
			float w = s * 0.5f + 0.8f;
			float d = s * 15.0f;
			uint32_t color = ((uint32_t)EFXM_ArcAlpha(i, t) << 24) | 0x1840c0;
			EFXM_ArcAdd(&self->trail, &a, &b, &pos, d, w, 18.0f, 18.0f, color);
		}
		return;
	}
	if (CGameTrail_IsRender(&self->trail)) {
		CGameTrail_Clear(&self->trail);
	}
}

/* the laser along -y of the model; the glow and the letters fade with the global switch */
void __thiscall cEmbeddedFXDataPack_Update(cEmbeddedFXDataPack *self, float dt) {
	vec3_t rot, d, base, q, tmp;

	CGameModel *model = EFXM_ShownTrail(&self->base, &self->visible, &self->trail);
	if (!model) {
		return;
	}
	vec3_t down = { 0.0f, -1.0f, 0.0f };
	Vec3_Rotate(&d, &down, CGameModel_GetRot(model, &rot));
	float kx = d.x * 50.0f;
	float ky = d.y * 50.0f;
	float kz = d.z * 50.0f;
	vec3_t *p = CGameModel_GetPos(model, &tmp);
	base.x = p->x - kx;
	base.y = p->y - ky;
	base.z = p->z - kz;
	CGameTrail_Reset(&self->trail);
	CGameTrail_Add(&self->trail, base, g_DataPackLaserSize, 0x10ff1818);
	float len = g_DataPackLaserLong;
	q.x = d.x * len * 0.5f + base.x;
	q.y = d.y * len * 0.5f + base.y;
	q.z = d.z * len * 0.5f + base.z;
	CGameTrail_Add(&self->trail, q, g_DataPackLaserSize, 0x10ff1818);
	len = g_DataPackLaserLong;
	q.x = d.x * len + base.x;
	q.y = d.y * len + base.y;
	q.z = d.z * len + base.z;
	CGameTrail_Add(&self->trail, q, g_DataPackLaserSize * 0.5f, 0x20ff1010);
	byte on = self->on;
	if (self->shown == on) {
		return;
	}
	Float_Approach(&self->level, on ? 1.0f : 0.0f, dt / g_DataPackInterTime);
	float v = self->level;
	if (v == 0.0f || v == 1.0f) {
		self->shown = self->on;
	}
	byte show = v > 0.0f;
	CGameParticles_SetVisible(&self->glow, show);
	CGameParticles_SetAlpha(&self->glow, self->level);
	CGameParticles_SetVisible(&self->codec, show);
	CGameParticles_SetAlpha(&self->codec, self->level);
	if (show) {
		CGameTrail_SetTransp(&self->trail, (byte)(0xff - (int)(self->level * 255.0f)));
		return;
	}
	CGameTrail_Clear(&self->trail);
}

/* three glows around the pole, bigger with the distance to the camera; a flash at the top every 0.5 s */
void __thiscall cEmbeddedFXFlag_Update(cEmbeddedFXFlag *self, float dt) {
	CGameParticles *p = &self->particles;
	vec3_t tmp, out;
	float frac;

	CGameModel *model = EFXM_ShownTrail(&self->base, &self->visible, NULL);
	if (!model) {
		return;
	}
	cWorld *w = g_pWorld;
	vec3_t cam = w->camPos[w->curCam];
	vec3_t *pos = CGameModel_GetPos(model, &tmp);
	float dy = pos->y - cam.y;
	float dx = pos->x - cam.x;
	float dz = pos->z - cam.z;
	float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) / 6500.0f;
	if (2.3f > d) {
		d = 2.3f;
	} else if (!(8.5f > d)) {
		d = 8.5f;
	}
	float scale = self->scale;
	int n = 0;
	float sd = scale * d;
	float m = scale == 0.37f ? -160.0f : -180.0f;
	vec3_t off = { scale * -130.0f, m * scale, scale * 60.0f };
	for (int j = 0; j < 3; j++) {
		vec3_t rot = { 0.0f, (float)j * 2.0943952f, 0.0f };
		Vec3_Rotate(&out, &off, &rot);
		CGameParticles_SetParticle(p, n, out.x, out.y, out.z, self->scale * 180.0f,
			(self->color & 0xffffff) | 0x20000000, 2, 0.0f);
		n++;
		int r = crt_rand() & 0x7fff;
		float size = (float)((double)r * (1.0 / 32767.0) * 25.0 + 720.0) * self->scale;
		CGameParticles_SetParticle(p, n, out.x, out.y, out.z, size, (self->color2 & 0xffffff) | 0x80000000, 5, 0.0f);
		n++;
	}
	float t = self->t + dt;
	self->t = t;
	if (t > 0.5f) {
		self->t = 0.0f;
	}
	if (!FX_InRange(self->t, 0.0f, 0.2f, &frac)) {
		return;
	}
	float h = frac * 0.5f;
	int a = EFXM_FlashAlpha(frac);
	/* the flash at the top and at the glows */
	for (int k = 0; k < 2; k++) {
		scale = self->scale;
		float x = scale * 0.0f;
		float y = k ? scale * (scale == 0.37f ? -160.0f : -180.0f) : scale * 1335.0f;
		CGameParticles_SetParticle(p, n, x, y, x, (1.0f - h) * sd * 150.0f, EFXM_Alpha(self->color, EFXM_Min255(a + 8)),
			2, 0.0f);
		n++;
		CGameParticles_SetParticle(p, n, x, y, x, (1.0f - h) * sd * 850.0f,
			EFXM_Alpha(self->color2, EFXM_Min255(a + 0x10)), 5, 0.0f);
		n++;
	}
}
/* the light flashes every 0.8 s; the count down shows three lights one after the other */
void __thiscall cEmbeddedFXMeetingPoint_Update(cEmbeddedFXMeetingPoint *self, float dt) {
	static const uint32_t colorsA[3] = { 0xffffa4a4, 0xffffffa4, 0xffa4ffa4 };
	static const uint32_t colorsB[3] = { 0xffff0808, 0xffffff08, 0xff08ff08 };
	vec3_t tmp;
	float frac;

	CGameModel *model = EFXM_ShownTrail(&self->base, &self->visible, NULL);
	if (!model) {
		return;
	}
	float t = self->t + dt;
	self->t = t;
	if (t > 0.8f) {
		self->t = 0.0f;
	}
	if (FX_InRange(self->t, 0.0f, 0.4f, &frac)) {
		cWorld *w = g_pWorld;
		vec3_t cam = w->camPos[w->curCam];
		vec3_t *pos = CGameModel_GetPos(model, &tmp);
		float dy = pos->y - cam.y;
		float dx = pos->x - cam.x;
		float dz = pos->z - cam.z;
		float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) / 4000.0f;
		if (2.0f > d) {
			d = 2.0f;
		} else if (!(8.0f > d)) {
			d = 8.0f;
		}
		float h = frac * 0.5f;
		int a = EFXM_FlashAlpha(frac);
		CGameParticles_SetParticle(&self->light, 0, 0.0f, 0.0f, 0.0f, (1.0f - h) * d * 250.0f,
			((uint32_t)EFXM_Min255(a + 8) << 24) | 0xffe140, 2, 0.0f);
		CGameParticles_SetParticle(&self->light, 1, 0.0f, 0.0f, 0.0f, (1.0f - frac * 0.5f) * d * 950.0f,
			((uint32_t)EFXM_Min255(a + 0x10) << 24) | 0xff2004, 5, 0.0f);
	}
	float ct = self->countT;
	if (!(ct >= 0.0f) || !(self->countA * 3.0f > ct)) {
		return;
	}
	self->countT = ct + dt;
	int n = 0;
	for (int i = 0, ang = -180; ang < 360; i++, ang += 180, n += 2) {
		float len = self->countA;
		float from = (float)i * len;
		if (!FX_InRange(self->countT, from, from + (len + 0.3f), &frac)) {
			continue;
		}
		float g;
		if (0.05f > frac) {
			g = frac * 20.0f;
		} else if (frac > 0.8f) {
			g = 1.0f - (frac - 0.8f) * 5.0f;
		} else {
			g = 1.0f;
		}
		float e = (g + g - 3.0f) * (g * g) * -255.0f;
		int a = 0xff - (int)e;
		float size = g * 0.1f + 0.9f;
		float x = (float)ang;
		float z = self->countB;
		if (g > 0.0f) {
			CGameParticles_SetParticle(&self->countDown, n, x, -30.0f, z, size * 950.0f,
				EFXM_Alpha(colorsA[i], EFXM_Min255(a + 4)), 2, 0.0f);
			CGameParticles_SetParticle(&self->countDown, n + 1, x, -30.0f, z, size * 2850.0f,
				EFXM_Alpha(colorsB[i], EFXM_Min255(a + 0x18)), 5, 0.0f);
		} else {
			CGameParticles_Kill(&self->countDown, n);
			CGameParticles_Kill(&self->countDown, n + 1);
		}
	}
}

/* rays from the two ends of the bar along the angles of the root, glows on the bar */
void __thiscall cEmbeddedFXExtraLife_Update(cEmbeddedFXExtraLife *self, float dt) {
	CGameParticles *p = &self->glow;
	matrix_t m, mb;
	vec3_t pos, rot, ang, tmp, w, a;

	CGameModel *model = EFXM_Shown(&self->base, &self->visible, &self->trail);
	if (!model) {
		return;
	}
	CGameModel_GetPos(model, &pos);
	CGameModel_GetRot(model, &rot);
	CGameModel_NodoMatrix(model, &m, self->dummyRoot, 1, NULL, 0);
	float rx = m.m[12];
	float ry = m.m[13];
	float rz = m.m[14];
	Matrix_GetEuler(&m, &ang);
	vec3_t down = { 0.0f, -1.0f, 0.0f };
	Vec3_Rotate(&tmp, &down, &ang);
	CGameParticles_NumParticles(p);
	matrix_t *bar = CGameModel_NodoMatrix(model, &mb, self->dummyBarra, 1, NULL, 0);
	float dy = pos.y - bar->m[13];
	float dx = pos.x - bar->m[12];
	float dz = pos.z - bar->m[14];
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	CGameTrail_Reset(&self->trail);
	int n = 0;
	for (int k = 0; k < 2; k++) {
		vec3_t v = { 0.0f, k ? 50.0f : -54.0f, 0.0f };
		Vec3_Rotate(&w, &v, &ang);
		vec3_t q = { w.x + rx, w.y + ry, w.z + rz };
		for (int j = 0; j < 6; j++) {
			int alpha = crt_rand() % 48 + 0x10;
			vec3_t r = { 0.0f, (float)j * 1.0471976f + 0.5235988f, 0.0f };
			vec3_t dir = { 0.0f, 0.0f, 1.0f };
			Vec3_Rotate(&a, Vec3_Rotate(&tmp, &dir, &r), &ang);
			vec3_t s = { a.x * dist + q.x, a.y * dist + q.y, a.z * dist + q.z };
			CGameTrail_SetUV(&self->trail, (float)(EFXM_Rnd() + 0.0), 0.0f);
			uint32_t color = ((uint32_t)alpha << 24) | 0x400d37;
			CGameTrail_Add(&self->trail, q, self->scale * 6.0f, color);
			CGameTrail_Add(&self->trail, s, self->scale * 6.0f, color);
			int r2 = crt_rand() % 16;
			CGameParticles_SetParticle(p, n, s.x, s.y, s.z, self->scale * 8.0f,
				((uint32_t)(r2 + 0x10) << 24) | 0xffc0f8, 1, 0.0f);
			n++;
			float f = (float)(EFXM_Rnd() * 0.17000001668930054 + 0.949999988079071);
			r2 = crt_rand() % 64;
			CGameParticles_SetParticle(p, n, s.x, s.y, s.z, f * 28.0f * self->scale,
				((uint32_t)(r2 + 0x20) << 24) | 0xff67f6, 5, 0.0f);
			n++;
		}
	}
	float t = dt * 1.25f + self->t;
	self->t = t;
	float c = (float)Game_Cos((double)t);
	float pulse = (float)__builtin_fabs((double)c);
	n = 24;
	for (int i = 0; i < 8; i++) {
		vec3_t v = { 0.0f, (float)i * 12.0f - 48.0f, 0.0f };
		Vec3_Rotate(&w, &v, &ang);
		vec3_t q = { rx + w.x, w.y + ry, w.z + rz };
		int r = crt_rand() % 48;
		CGameParticles_SetParticle(p, n, q.x, q.y, q.z, self->scale * 70.0f, ((uint32_t)(r - 0x4c) << 24) | 0xff7af8,
			2, 0.0f);
		n++;
		float spin = (float)(EFXM_Rnd() * 6.2831854820251465 + 0.0);
		r = crt_rand() % 24;
		int al = -1 - (int)((float)(r + 0x52) * pulse);
		CGameParticles_SetParticle(p, n, q.x, q.y, q.z, self->scale * 210.0f, ((uint32_t)al << 24) | 0xff7af8, 5,
			spin);
		n++;
	}
}

/* three rays between two points of the root that move at random; Matrix.m3d only while Matrix_Visible plays */
void __thiscall cEmbeddedFXMatrix_Update(cEmbeddedFXMatrix *self, float dt) {
	matrix_t m;
	vec3_t rot, d, ang, t1, t2, pp;
	int anim;

	CGameModel *model = FX_Vtbl(self->base.ent)->GetModel(self->base.ent);
	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	byte vis = CGameModel_IsVisible(model);
	if (self->isMatrix) {
		anim = -1;
		CGameModel_ChanInfo(model, 0, NULL, &anim);
		if (!strcmp(CGameModel_AnimName(model, anim), "Matrix_Visible")) {
			CGameModel_Enable(model);
			vis = 1;
		} else {
			CGameModel_Disable(model);
			vis = 0;
		}
	}
	if (!vis) {
		if (self->visible) {
			CGameTrail_Clear(&self->trail);
			self->visible = 0;
		}
		return;
	}
	self->visible = vis;
	vec3_t down = { 0.0f, -1.0f, 0.0f };
	Vec3_Rotate(&d, &down, CGameModel_GetRot(model, &rot));
	CGameModel_NodoMatrix(model, &m, self->dummyRoot, 1, NULL, 0);
	Matrix_GetEuler(&m, &ang);
	if (self->isMatrix) {
		ang.z = ang.z + 1.5707964f;
	}
	vec3_t v = { self->scale * -3.0f, 0.0f, 0.0f };
	float rx = m.m[12];
	float ry = m.m[13];
	float rz = m.m[14];
	vec3_t *r = Vec3_Rotate(&t1, &v, &ang);
	vec3_t a = { rx + r->x, r->y + ry, r->z + rz };
	v.x = self->scale * 56.0f;
	v.y = 0.0f;
	v.z = 0.0f;
	r = Vec3_Rotate(&t1, &v, &ang);
	vec3_t b = { r->x + a.x, r->y + a.y, r->z + a.z };
	CGameTrail_Reset(&self->trail);
	int n = CGameParticles_NumParticles(&self->glow);
	for (int i = 0; i < 3; i++) {
		float tm = self->timer[i] - dt;
		self->timer[i] = tm;
		if (0.0f >= tm) {
			float dur = (float)(EFXM_Rnd() * 0.29999998211860657 + 0.15000000596046448);
			self->dur[i] = dur;
			self->timer[i] = dur;
			float r1 = (float)(EFXM_Rnd() * 8.0 - 4.0);
			float r2 = (float)(EFXM_Rnd() * 8.0 - 4.0);
			float scale = self->scale;
			self->off[i].x = scale * 0.0f;
			self->off[i].y = r2 * scale;
			self->off[i].z = r1 * scale;
		}
		int al = 0xff - (int)(self->timer[i] / self->dur[i] * 192.0f);
		vec3_t *o = &self->off[i];
		r = Vec3_Rotate(&t1, o, &ang);
		vec3_t p1 = { r->x + a.x, r->y + a.y, r->z + a.z };
		r = Vec3_Rotate(&t2, o, &ang);
		vec3_t p2 = { r->x + b.x, r->y + b.y, r->z + b.z };
		CGameTrail_SetUV(&self->trail, (float)(EFXM_Rnd() + 0.0), 0.0f);
		uint32_t color = ((uint32_t)al << 24) | 0x803c6e;
		CGameTrail_Add(&self->trail, p1, self->scale * 4.0f, color);
		CGameTrail_Add(&self->trail, p2, self->scale * 4.0f, color);
		color = ((uint32_t)al << 24) | 0xff78e6;
		n--;
		int rr = crt_rand() & 0x7fff;
		float y = self->scale * -26.5f + o->y;
		float x = o->x + 0.0f;
		float z = o->z + 0.0f;
		float size = (float)((double)rr * (1.0 / 32767.0) * 2.0 + 10.0);
		CGameParticles_SetParticle(&self->glow, n, x, y, z, size, color, 5, 0.0f);
		n--;
		rr = crt_rand() & 0x7fff;
		x = o->x + 0.0f;
		y = self->scale * 26.5f + o->y;
		z = o->z + 0.0f;
		size = (float)((double)rr * (1.0 / 32767.0) * 2.0 + 10.0);
		CGameParticles_SetParticle(&self->glow, n, x, y, z, size, color, 5, 0.0f);
	}
	for (int i = 0; i < CGameParticles_NumParticles(&self->flares); i++) {
		CGameParticles_GetParticle(&self->flares, i, &pp, NULL, NULL, NULL, NULL);
		float size = (float)(EFXM_Rnd() * 8.0 + 10.0);
		int rr = crt_rand() % 92;
		CGameParticles_SetParticle(&self->flares, i, pp.x, pp.y, pp.z, size, ((uint32_t)(rr + 8) << 24) | 0xff78e6, 5,
			0.0f);
	}
}

/* glows in the color of the type, a spark now and then, rays up from the root */
void __thiscall cEmbeddedFXTreasure_Update(cEmbeddedFXTreasure *self, float dt) {
	CGameParticles *p = &self->glow;
	matrix_t m;
	vec3_t ang, w;

	CGameModel *model = EFXM_ShownTrail(&self->base, &self->visible, &self->trail);
	if (!model) {
		return;
	}
	uint32_t rgb = 0xff4aff;
	switch (self->type) {
	case 1:
		rgb = 0xff4aff;
		break;
	case 2:
		rgb = 0x7eff5a;
		break;
	case 3:
		rgb = 0x5c18ff;
		break;
	}
	int n = 0;
	for (int k = 0; k < 4; k++) {
		float x = (float)(k - 2) * 28.0f + 10.0f;
		for (int j = 0; j < 2; j++) {
			float size = (float)(EFXM_Rnd() * 8.0 + 110.0);
			float y = (float)j * 38.0f;
			float z = (float)(EFXM_Rnd() * 6.0 - 2.0);
			float scale = self->scale;
			float px = x * scale;
			float py = (y - 20.0f) * scale;
			float pz = (z + 28.0f) * scale;
			int r = crt_rand() % 14;
			CGameParticles_SetParticle(p, n, px, py, pz, size * self->scale, ((uint32_t)(r - 0x46) << 24) | rgb, 5,
				0.0f);
			n++;
		}
	}
	float t = self->timer - dt;
	self->timer = t;
	if (0.0f >= t) {
		float life = self->life;
		if (0.0f >= life) {
			/* a new spark */
			self->timer = (float)(EFXM_Rnd() * 1.0999999046325684 + 0.30000001192092896);
			float dur = (float)(EFXM_Rnd() * 0.09999996423721313 + 0.800000011920929);
			self->dur = dur;
			self->life = dur;
			self->rot = (float)(EFXM_Rnd() * 6.2831854820251465 + 0.0);
			float speed = (float)(EFXM_Rnd() + 1.0);
			self->rotSpeed = (float)((crt_rand() & 1) * 2 - 1) * speed;
			double a;
			float b, y;
			if (self->type == 3) {
				a = EFXM_Rnd();
				b = (float)(EFXM_Rnd() * 56.0 - 28.0);
				y = (float)((crt_rand() & 1) ? -10 : 10);
				a = a + a + 0.0;
			} else {
				a = EFXM_Rnd();
				float b1 = (float)(EFXM_Rnd() * 0.30000001192092896 - 0.15000000596046448);
				b = (float)(EFXM_Rnd() * 56.0 - 28.0);
				y = (float)((crt_rand() & 1) ? -10 : 10) + b1;
				a = a + a + 8.0;
			}
			self->spark.x = b + 10.0f;
			self->spark.y = y - 20.0f;
			self->spark.z = (float)a + 28.0f;
		} else {
			life = life - dt;
			self->life = life;
			float f = 1.0f - life / self->dur;
			int al = 0x10 - (int)(f * -239.0f);
			float sz = f * 160.0f;
			float scale = self->scale;
			self->rot = self->rotSpeed * dt + self->rot;
			float px = self->spark.x * scale;
			float py = self->spark.y * scale;
			float pz = self->spark.z * scale;
			if (life > 0.0f) {
				CGameParticles_SetParticle(p, n, px, py, pz, scale * 25.0f, ((uint32_t)al << 24) | 0xffffff, 1, 0.0f);
				CGameParticles_SetParticle(p, n + 1, px, py, pz, sz * self->scale * 0.3f, ((uint32_t)al << 24) | rgb,
					1, 0.0f);
				CGameParticles_SetParticle(p, n + 2, px, py, pz, sz * self->scale * 0.7f,
					((uint32_t)EFXM_Min255(al + 0x10) << 24) | rgb, 2, 0.0f);
				CGameParticles_SetParticle(p, n + 3, px, py, pz, sz * self->scale,
					((uint32_t)EFXM_Min255(al + 0x20) << 24) | rgb, 10, self->rot);
			} else {
				CGameParticles_KillRange(p, n, n + 4);
			}
		}
	}
	CGameTrail_Reset(&self->trail);
	CGameModel_NodoMatrix(model, &m, self->dummyRoot, 1, NULL, 0);
	float rx = m.m[12];
	float ry = m.m[13];
	float rz = m.m[14];
	Matrix_GetEuler(&m, &ang);
	uint32_t top = rgb | 0xff000000;
	for (int k = 0; k < 4; k++) {
		float z = (float)(k - 2) * 20.0f + 10.0f;
		for (int j = 0; j < 2; j++) {
			vec3_t v;
			v.y = (float)(EFXM_Rnd() * 4.0 - 16.0);
			v.x = (float)j * 32.0f - 16.0f;
			v.z = z;
			float scale = self->scale;
			Vec3_Rotate(&w, &v, &ang);
			float sx = scale * w.x;
			float sy = w.y * scale;
			float sz = w.z * scale;
			vec3_t q = { sx + rx, sy + ry, sz + rz };
			int r = crt_rand() % 8;
			CGameTrail_Add(&self->trail, q, self->scale * 85.0f, ((uint32_t)(r - 0x2a) << 24) | rgb);
			float f = (float)(EFXM_Rnd() * 40.0 + 185.0);
			scale = self->scale;
			q.x = (rx + sx * 1.9f) + scale * 0.0f;
			q.y = (ry + sy * 1.9f) + scale * f;
			q.z = (rz + sz * 1.9f) + scale * 0.0f;
			CGameTrail_Add(&self->trail, q, scale * 105.0f, top);
		}
	}
}

/* the BossShield engine variables */
#define g_FXBossShieldAspect GAME_DATA(float, 0x447404)
#define g_FXBossShieldRadius GAME_DATA(float, 0x447454)
#define g_FXBossShieldScale GAME_DATA(float, 0x4473ec)

/* a random point on the shield */
static void EFXM_ShieldDir(vec3_t *out) {
	vec3_t tmp;
	float aspect = g_FXBossShieldAspect;
	float radius = g_FXBossShieldRadius;

	vec3_t *d = FX_RandomDir(&tmp);
	out->x = d->x * radius;
	out->y = d->y * radius * aspect;
	out->z = d->z * radius;
}

/* a random color of the mode (1 blue, 2 green), else keep */
static uint32_t EFXM_ShieldColor(int mode, uint32_t keep) {
	uint32_t c;

	if (mode == 1) {
		c = (uint32_t)(crt_rand() % 16 + 0x40) << 8;
		c |= (uint32_t)(crt_rand() % 16 + 0x40) << 16;
		return (uint32_t)(crt_rand() % 32 + 0xa4) | c;
	}
	if (mode == 2) {
		c = (uint32_t)(crt_rand() % 32 + 0x9a) << 8;
		c |= (uint32_t)(crt_rand() % 16 + 0x40) << 16;
		return (uint32_t)(crt_rand() % 16 + 0x40) | c;
	}
	return keep;
}

/* the swell of the shield by the hit: 1 + 0.4 h^2 (2 - h) */
static inline float EFXM_ShieldSwell(float h) {
	return h * h * (2.0f - h) * 0.4f + 1.0f;
}

/* the visibility of the shield by the hit and the regeneration, random */
static float EFXM_ShieldVis(cEmbeddedFXBossShield *self, float h, float a) {
	float w = h > 1.0f ? 1.0f + self->regen : h + self->regen + self->regen;

	w = w * a;
	return Float_MinSS(1.0f, (float)(EFXM_Rnd() * 0.40000003576278687 + 0.800000011920929) * w);
}

/* the glows of the slots move on the shield; 16 rings turn around it; the regeneration from the scorer */
void __thiscall cEmbeddedFXBossShield_Update(cEmbeddedFXBossShield *self, float dt) {
	CGameParticles *p = &self->particles;
	vec3_t pos, rot, o, o2, P = { 0.0f, 0.0f, 0.0f };
	uint32_t c = 0;

	CGameModel *model = FX_Vtbl(self->base.ent)->GetModel(self->base.ent);
	CHECK(CGameModel_IsLoaded(model));
	if (self->mode < 1 || self->mode > 2) {
		if (CGameModel_IsEnabled(model)) {
			for (int i = 0; i < FXBOSSSHIELD_TRAILS; i++) {
				CGameTrail_Clear(&self->trails[i]);
			}
		}
		CGameModel_Disable(model);
		return;
	}
	CGameModel_Enable(model);
	CGameParticles_SetAlpha(p, (float)(EFXM_Rnd() * 0.5 + 0.5));
	for (int i = 0; i < FXBOSSSHIELD_SLOTS; i++) {
		float t = self->speed[i] * dt + self->t[i];
		self->t[i] = t;
		if (0.0f > t || t >= 1.0f) {
			self->t[i] = 0.0f;
			EFXM_ShieldDir(&self->dir[i]);
			self->speed[i] = (float)(EFXM_Rnd() * 6.350000381469727 + 2.450000047683716);
			self->size[i] = (float)(EFXM_Rnd() * 65.0 + 155.0) * g_FXBossShieldScale;
			self->color[i] = EFXM_ShieldColor(self->mode, self->color[i]);
		}
		P = self->dir[i];
		int odd = i & 1;
		if (odd) {
			EFXM_ShieldDir(&P);
		}
		float h = self->hit;
		float size = self->size[i];
		if (h > 0.0f) {
			float k = EFXM_ShieldSwell(h);
			P.x = P.x * k;
			P.y = P.y * k;
			P.z = P.z * k;
			size = self->size[i] * ((3.0f - (h + h)) * (h * h) + 1.0f);
		}
		float tt = self->t[i];
		float a;
		if (0.1f > tt) {
			a = 1.0f - tt * 10.0f;
		} else if (tt > 0.5f) {
			a = (tt - 0.5f) + (tt - 0.5f);
		} else {
			a = 0.0f;
		}
		float vis = EFXM_ShieldVis(self, h, a);
		int al = 0xff - (int)(vis * 255.0f);
		uint32_t color = (self->color[i] & 0xffffff) | ((uint32_t)al << 24);
		if (al >= 0xff) {
			CGameParticles_Kill(p, i);
		} else if (odd) {
			float spin = (float)(EFXM_Rnd() * 6.2831854820251465 + 0.0);
			float s = (float)(EFXM_Rnd() * 0.10000002384185791 + 1.350000023841858) * size;
			CGameParticles_SetParticle(p, i, P.x, P.y, P.z, s, color, 0x2b, spin);
		} else {
			CGameParticles_SetParticle(p, i, P.x, P.y, P.z, size * 0.6f, color, 5, 0.0f);
		}
	}
	self->spin = dt * 14.0f + self->spin;
	self->f1cac = dt * 20.0f + self->f1cac;
	for (int j = 0, n = FXBOSSSHIELD_TRAILS; n > 0; j++, n--) {
		CGameTrail *trail = &self->trails[j];
		c = EFXM_ShieldColor(self->mode, c);
		float radius = g_FXBossShieldRadius * 1.8f;
		float h = self->hit;
		if (h > 0.0f) {
			radius = radius * EFXM_ShieldSwell(h);
		}
		float vis = EFXM_ShieldVis(self, h, 1.0f);
		CGameTrail_Reset(trail);
		CGameTrail_SetTransp(trail, (byte)(int)((float)n * 0.0625f * 232.0f));
		int np = CGameTrail_GetMaxTrails(trail);
		int half = np / 2;
		for (int k = 0; k < np; k++) {
			float f = (float)k / (float)half;
			float a = k < half ? -(1.0f - f) : f - 1.0f;
			int r = crt_rand() % 64;
			int al = -1 - (int)((float)(r - (int)(((float)__builtin_fabs((double)a) - 1.0f) * 191.0f)) * vis);
			uint32_t color = ((uint32_t)al << 24) | (c & 0xffffff);
			float sn, cs;
			X87_SinCos(a, &sn, &cs);
			float aspect = g_FXBossShieldAspect;
			float ang = self->spin + (float)j * 0.055f;
			vec3_t rr, v;
			if (j & 1) {
				rr = (vec3_t){ ang, 3.1415927f, 0.0f };
			} else {
				rr = (vec3_t){ ang + 3.1415927f, 3.1415927f, 0.0f };
			}
			v = (vec3_t){ sn, 0.0f, cs };
			Vec3_Rotate(&o, &v, &rr);
			vec3_t q = { o.x * radius, o.y * radius * aspect, o.z * radius };
			vec3_t *r2 = Vec3_Rotate(&o2, &q, CGameModel_GetRot(model, &rot));
			vec3_t *at = CGameModel_GetPos(model, &pos);
			vec3_t pt = { r2->x + at->x, r2->y + at->y, r2->z + at->z };
			CGameTrail_Add(trail, pt, 125.0f, color);
		}
	}
	cEntity *boss = World_FindEntity("Boss");
	if (boss && boss->vftable->IsType(boss, TYEN_CAR)) {
		cScorer *scorer = Scorer_Get(0);
		cScorerItem *item = scorer ? cScorer_FindItem(scorer, "RegenerationBar") : NULL;
		if (item) {
			/* not written by Var when the item has no Value: then the original reads the x, y of the last glow */
			double v;
			memcpy(&v, &P, sizeof(v));
			self->regen = 0.0f;
			item->vftable->Var(item, "Value", 0, NULL, &v, NULL);
			self->regen = (float)v;
		}
	}
	if (self->hit > 0.0f) {
		self->hit = self->hit - dt * 4.0f;
		return;
	}
	self->hit = 0.0f;
}

/* the color ramps of the battery sparks (9 colors) */
#define RAMP_BATTERY_FLASH 0x428450
#define RAMP_BATTERY_SPARKS 0x428474	/* [2] */

static inline void EFXM_ParticleColor(CGameParticles *p, int n, const vec3_t *pos, float size, int a, int sprite) {
	CGameParticles_SetParticle(p, n, pos->x, pos->y, pos->z, size, ((uint32_t)a << 24) | 0x64d2ff, sprite, 0.0f);
}

/* the start of the particles of the sparks at s * a random direction from c */
static void EFXM_SparkPos(CGameParticles *p, float s, const vec3_t *c) {
	vec3_t tmp;

	vec3_t *dir = FX_RandomDir(&tmp);
	p->initPosMin.x = s * dir->x + c->x;
	p->initPosMin.y = dir->y * s + c->y;
	p->initPosMin.z = dir->z * s + c->z;
	p->initPosMax = p->initPosMin;
}

/* a flash of the sparks: the sizes and the life from the random ranges */
static void EFXM_SparkFlash(CGameParticles *p, double ini, double iniAdd, double endAdd, byte sprite, double life,
	double lifeAdd) {
	CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(RAMP_BATTERY_FLASH));
	p->sizeIni = (float)(EFXM_Rnd() * ini + iniAdd);
	p->sizeEnd = (float)(EFXM_Rnd() * 15.0 + endAdd);
	int r = crt_rand() & 0x7fff;
	p->sizeLife = 0.15f;
	p->spriteIni = sprite;
	p->spriteEnd = sprite;
	p->lifeTime = (float)((double)r * (1.0 / 32767.0) * life + lifeAdd);
	CGameParticles_Apply(p);
	CGameParticles_EmitN(p, 1, 1);
}

/* three rays that jump between random points now and then, a burst of sparks at the top */
void __thiscall cEmbeddedFXBattery_Update(cEmbeddedFXBattery *self, float dt) {
	CGameParticles *p = &self->glow;
	matrix_t m;
	vec3_t rot, d, ang, tmp, ta, tb, up;

	CGameModel *model = FX_Vtbl(self->base.ent)->GetModel(self->base.ent);
	if (!CGameModel_IsLoaded(model)) {
		return;
	}
	CHECK(CGameModel_IsLoaded(model));
	byte vis = CGameModel_IsVisible(model);
	if (!vis) {
		if (self->visible != vis) {
			cEmbeddedFXBattery_Clear(self);
			self->visible = 0;
		}
		return;
	}
	self->visible = vis;
	vec3_t down = { 0.0f, -1.0f, 0.0f };
	Vec3_Rotate(&d, &down, CGameModel_GetRot(model, &rot));
	CGameModel_NodoMatrix(model, &m, self->dummyRoot, 1, NULL, 0);
	float rx = m.m[12];
	float ry = m.m[13];
	float rz = m.m[14];
	Matrix_GetEuler(&m, &ang);
	CGameParticles_NumParticles(p);
	for (int i = 0; i < FXBATTERY_TRAILS; i++) {
		CGameTrail *trail = &self->trails[i];
		cEmbeddedFXBatteryRay *ray = &self->rays[i];
		if (self->life[i] > 0.0f) {
			CGameTrail_Clear(trail);
			CGameTrail_SetUV(trail, (float)(EFXM_Rnd() + 0.0), 0.0f);
			float t = self->life[i] - dt;
			self->life[i] = t;
			if (!(t > 0.0f)) {
				CGameParticles_KillRange(p, 4 * i, 4 * i + 4);
				continue;
			}
			float frac = 1.0f;
			FX_InRange(t, 0.0f, 0.05f, &frac);
			int al = (int)((1.0f - frac) * 164.0f);
			vec3_t *r = Vec3_Rotate(&ta, &ray->a, &ang);
			vec3_t a = { rx + r->x, r->y + ry, r->z + rz };
			r = Vec3_Rotate(&tb, &ray->b, &ang);
			vec3_t b = { r->x + rx, r->y + ry, r->z + rz };
			uint32_t color = ((uint32_t)(al + 8) << 24) | 0x122c40;
			vec3_t step = { b.x - a.x, b.y - a.y, b.z - a.z };
			CGameTrail_Add(trail, a, 8.0f, color);
			step.x = step.x * 0.25f;
			step.y = step.y * 0.25f;
			step.z = step.z * 0.25f;
			for (int j = 1; j < 4; j++) {
				float s = (float)(EFXM_Rnd() * 3.0 + 0.5);
				vec3_t *dir = FX_RandomDir(&tmp);
				float nx = s * dir->x;
				float ny = dir->y * s;
				float nz = dir->z * s;
				float u = (float)(EFXM_Rnd() * 0.3500000238418579 + 0.25) + (float)j;
				vec3_t q = { a.x + step.x * u + nx, a.y + step.y * u + ny, a.z + step.z * u + nz };
				CGameTrail_Add(trail, q, (float)(EFXM_Rnd() + 0.0) + 5.0f, color);
			}
			CGameTrail_Add(trail, b, 8.0f, color);
			EFXM_ParticleColor(p, 4 * i, &b, 8.0f, crt_rand() % 16 + (al + 0x10), 1);
			EFXM_ParticleColor(p, 4 * i + 1, &b, 24.0f, crt_rand() % 64 + (al + 0x20), 5);
			EFXM_ParticleColor(p, 4 * i + 2, &a, 8.0f, crt_rand() % 16 + (al + 0x10), 1);
			EFXM_ParticleColor(p, 4 * i + 3, &a, 24.0f, crt_rand() % 64 + (al + 0x20), 5);
			continue;
		}
		float delay = self->delay[i];
		if (delay > 0.0f) {
			self->delay[i] = delay - dt;
			continue;
		}
		if (CGameModel_GetEffect(model, NULL)) {
			continue;
		}
		/* a new ray from the bottom (y -36) to the top (y 30) or the other way */
		self->delay[i] = (float)(EFXM_Rnd() * 0.5 + 0.30000001192092896);
		self->life[i] = (float)(EFXM_Rnd() * 0.04999999701976776 + 0.20000000298023224);
		float z;
		if (crt_rand() & 1) {
			z = (float)(EFXM_Rnd() * 2.0 + 0.0) + 18.0f;
		} else {
			z = -18.0f - (float)(EFXM_Rnd() * 2.0 + 0.0);
		}
		vec3_t *from = &ray->b;
		vec3_t *to = &ray->a;
		if (crt_rand() & 1) {
			from = &ray->a;
			to = &ray->b;
		}
		from->y = -36.0f;
		from->x = (float)(EFXM_Rnd() * 64.0 - 32.0);
		from->z = z;
		to->x = (float)(EFXM_Rnd() * 12.0 - 6.0) + from->x;
		to->y = 30.0f;
		to->z = z;
	}
	float st = self->sparkT - dt;
	self->sparkT = st;
	if (!(0.0f >= st)) {
		return;
	}
	self->sparkT = (float)(EFXM_Rnd() * 0.699999988079071 + 0.30000001192092896);
	p->birthRate = 0;
	CGameParticles_SetSparksOn(p, 0);
	vec3_t v = { 0.0f, 55.0f, 0.0f };
	vec3_t *r = Vec3_Rotate(&tmp, &v, &ang);
	vec3_t c = { rx + r->x, r->y + ry, r->z + rz };
	float r1 = (float)(EFXM_Rnd() - 0.5);
	float r2 = (float)(EFXM_Rnd() - 0.5);
	float r3 = (float)(EFXM_Rnd() - 0.5);
	vec3_t rr = { r3, r2, r1 };
	vec3_t upv = { 0.0f, 1.0f, 0.0f };
	Vec3_Rotate(&up, &upv, &rr);
	EFXM_SparkPos(p, (float)(EFXM_Rnd() * 3.0 + 1.0), &c);
	p->initVelMin.x = 0.0f;
	p->initVelMin.y = 0.0f;
	p->initVelMin.z = 0.0f;
	p->initVelMax = p->initVelMin;
	p->initAccMin.x = 0.0f;
	p->initAccMin.y = 0.0f;
	p->initAccMin.z = 0.0f;
	p->initAccMax = p->initAccMin;
	EFXM_SparkFlash(p, 10.0, 40.0, 165.0, 6, 0.019999995827674866, 0.18000000715255737);
	EFXM_SparkFlash(p, 2.0, 4.0, 55.0, 5, 0.02000001072883606, 0.1599999964237213);
	CGameParticles_SetSparksOn(p, 1);
	p->birthRate = 0;
	for (int j = 0; j < crt_rand() % 6 + 8; j++) {
		EFXM_SparkPos(p, (float)(EFXM_Rnd() * 3.0 + 1.0), &c);
		float speed = (float)(EFXM_Rnd() * 250.0 + 150.0);
		float q1 = (float)(EFXM_Rnd() * 1.2000000476837158 - 0.6000000238418579);
		float q2 = (float)(EFXM_Rnd() * 1.2000000476837158 - 0.6000000238418579);
		float q3 = (float)(EFXM_Rnd() * 1.2000000476837158 - 0.6000000238418579);
		vec3_t rot2 = { q3, q2, q1 };
		vec3_t *w = Vec3_Rotate(&ta, &up, &rot2);
		p->initVelMin.x = w->x * speed;
		p->initVelMin.y = w->y * speed;
		p->initVelMin.z = w->z * speed;
		p->initVelMax = p->initVelMin;
		float k = (float)(EFXM_Rnd() * 0.06000000238418579 + 0.6200000047683716);
		float ax = -p->initVelMin.x * k;
		float ay = -p->initVelMin.y * k;
		float az = -p->initVelMin.z * k;
		p->initAccMin.y = ay - 280.0f;
		p->initAccMin.x = ax + 0.0f;
		p->initAccMin.z = az + 0.0f;
		p->initAccMax = p->initAccMin;
		float size = (float)(EFXM_Rnd() * 6.0 + 12.0);
		p->sizeIni = size;
		p->sizeEnd = size;
		int rl = crt_rand() & 0x7fff;
		p->sizeLife = 1.0f;
		p->rotIni = 0.0f;
		p->rotEnd = 0.0f;
		p->rotVar = 0.0f;
		p->sparkVelScale = 1.0f;
		p->lifeTime = (float)((double)rl * (1.0 / 32767.0) * 0.20000001788139343 + 0.4000000059604645);
		p->sparkMaxSize = (float)(EFXM_Rnd() * 1.899999976158142 + 0.6000000238418579);
		p->spriteIni = (byte)((crt_rand() & 1) + 0x41);
		p->spriteEnd = 0x44;
		CGameParticles_Apply(p);
		CGameParticles_SetColorRampOn(p, 1);
		CGameParticles_SetColorRamp(p, (uint32_t *)Game_Ptr(RAMP_BATTERY_SPARKS + 36 * (crt_rand() & 1)));
		CGameParticles_EmitDt(p, 1, 0, (float)(EFXM_Rnd() * 0.10000000149011612 + 0.0));
	}
}
