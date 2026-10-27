/*
 * WindFxScorer.cpp of the game (Menu/): cWindFxScorer, the wind noise of the ship by the free space around it. See
 * include/scrap/scorerhud.h.
 */
#include "scrap/scorerhud.h"
#include "scrap/sound.h"

cWindFxScorer *__fastcall cWindFxScorer_ctor(cWindFxScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CWINDFXSCORER_VFTABLE);
	cSound_Init(&self->sound);
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	/* the color stays the one of the image */
	self->img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x24) : NULL;
	cSound_SetSound(&self->sound, g_WindFxSound, 0);
	self->sound.desc.loop = 1;
	self->wind[1] = 0.0f;
	self->wind[0] = 0.0f;
	self->wind[2] = 0.0f;
	self->wind[3] = self->wind[1];
	return self;
}

void __fastcall cWindFxScorer_dtor(cWindFxScorer *self) {
	self->base.vftable = Game_Ptr(CWINDFXSCORER_VFTABLE);
	ScorerImg_Free(self->img);
	cSound_Stop(&self->sound);
	cScorerItem_dtor(&self->base);
}

cWindFxScorer *__thiscall cWindFxScorer_Delete(cWindFxScorer *self, byte flags) {
	cWindFxScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cWindFxScorer));
	}
	return self;
}
