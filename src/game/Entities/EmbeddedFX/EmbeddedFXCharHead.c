/*
 * Entities/EmbeddedFX/EmbeddedFXCharHead.cpp of the game: the glow on the hair of the head model of the Crazy
 * Gambler, see include/scrap/embeddedfxchar.h.
 */
#include "scrap/crt.h"
#include "scrap/embeddedfxchar.h"
#include "scrap/global.h"
#include "scrap/timer.h"

#define VT_HEADCRAZYGAMBLER GAME_VFTABLE(cEmbeddedFXHeadCrazyGambler_vftable)
#define HEAD_GLOWS 12

cEmbeddedFXHeadCrazyGambler *cEmbeddedFXHeadCrazyGambler_ctor(cEmbeddedFXHeadCrazyGambler *self) {
	self->vftable = NULL;
	self->vftable = VT_HEADCRAZYGAMBLER;
	CGameParticles_ctor(&self->particles);
	self->unused_c0 = 0;
	self->owner = NULL;
	self->t = 0.0f;
	self->nodeHead = -1;
	return self;
}

void cEmbeddedFXHeadCrazyGambler_dtor(cEmbeddedFXHeadCrazyGambler *self) {
	self->vftable = VT_HEADCRAZYGAMBLER;
	CGameParticles_Release(&self->particles);
	cEmbeddedFX_dtor((cEmbeddedFX *)self);
}

cEmbeddedFXHeadCrazyGambler *cEmbeddedFXHeadCrazyGambler_Delete(cEmbeddedFXHeadCrazyGambler *self,
	byte flags) {
	cEmbeddedFXHeadCrazyGambler_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* every 0.02 s two glows on 12 vertices of the hair node */
void cEmbeddedFXHeadCrazyGambler_Tick(cEmbeddedFXHeadCrazyGambler *self) {
	CGameModel *model = self->owner;
	vec3_t pos[HEAD_GLOWS];
	int index[HEAD_GLOWS];

	if (!model || !CGameModel_IsLoaded(model)) {
		return;
	}
	float t = self->t + cTimer_GetDelta(&g_pWorld->timer);
	self->t = t;
	if (!(t > 0.02f)) {
		return;
	}
	self->t = 0.0f;
	CGameModel_Vertices(model, pos, NULL, NULL, HEAD_GLOWS, -1, index, self->nodeHead, NULL);
	for (int i = 0; i < HEAD_GLOWS; i++) {
		CGameParticles_SetParticle(&self->particles, i * 2, pos[i].x, pos[i].y, pos[i].z, 24.0f, 0xe040ff40, 2,
			0.0f);
		CGameParticles_SetParticle(&self->particles, i * 2 + 1, pos[i].x, pos[i].y, pos[i].z, 3.0f, 0x5080ff80, 2,
			0.0f);
	}
}

void cEmbeddedFXHeadCrazyGambler_Release(cEmbeddedFXHeadCrazyGambler *self) {
	CGameParticles_Release(&self->particles);
}

void cEmbeddedFXHeadCrazyGambler_Clear(cEmbeddedFXHeadCrazyGambler *self) {
	CGameParticles_KillAll(&self->particles);
}

/* the particles on the render node glow of the owner model */
void cEmbeddedFXHeadCrazyGambler_Init(cEmbeddedFXHeadCrazyGambler *self, CGameModel *owner,
	const char *glow) {
	self->owner = owner;
	CHECK(CGameModel_IsLoaded(owner));
	self->nodeHead = CGameModel_FindRenderNode(owner, "R_Gambler_Cabeza-Pelos");
	CHECK(self->nodeHead > -1);
	int nodeindex = CGameModel_FindRenderNode(owner, glow);
	CHECK(nodeindex > -1);
	CGameParticles_Attach(&self->particles, owner, nodeindex);
	CHECK(self->particles.ParticleSystem);
}

static cEmbeddedFX *EFXHead_New(CGameModel *head, const char *glow) {
	void *p = operator_new(sizeof(cEmbeddedFXHeadCrazyGambler));
	cEmbeddedFXHeadCrazyGambler *fx = p ? cEmbeddedFXHeadCrazyGambler_ctor(p) : NULL;

	fx->vftable->Reset((cEmbeddedFX *)fx);
	cEmbeddedFXHeadCrazyGambler_Init(fx, head, glow);
	return (cEmbeddedFX *)fx;
}

cEmbeddedFX *EmbeddedFXCharHead_Create(CGameModel *head) {
	CHECK(head);
	CHECK(CGameModel_IsLoaded(head));
	if (!crt_stricmp(CGameModel_Name(head), "HeadGambler.M3D")) {
		return EFXHead_New(head, "DF_GlowLighting");
	}
	if (!crt_stricmp(CGameModel_Name(head), "HeadEvilGambler.M3D")) {
		return EFXHead_New(head, "DF_Glow");
	}
	return NULL;
}
