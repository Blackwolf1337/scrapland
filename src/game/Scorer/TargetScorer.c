/*
 * TargetScorer.cpp of the game (Menu/): cTargetScorer, the target of the ship of the camera and the enemies aiming
 * at it. See include/scrap/scorerhud.h.
 */
#include "scrap/scorerhud.h"

cTargetScorer *__fastcall cTargetScorer_ctor(cTargetScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CTARGETSCORER_VFTABLE);
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x20) : NULL;
	self->img = img;
	img->color = 0xffffffff;
	self->view = 1.0f;
	self->target[0] = 0;
	self->enemyIndex = 0x31;
	self->testTime = 0.0f;
	return self;
}

void __fastcall cTargetScorer_dtor(cTargetScorer *self) {
	self->base.vftable = Game_Ptr(CTARGETSCORER_VFTABLE);
	ScorerImg_Free(self->img);
	cScorerItem_dtor(&self->base);
}

cTargetScorer *__thiscall cTargetScorer_Delete(cTargetScorer *self, byte flags) {
	cTargetScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cTargetScorer));
	}
	return self;
}

int __thiscall cTargetScorer_Var(cTargetScorer *self, const char *name, byte set, char **s, double *d, int *i) {
	if (s && !crt_stricmp(name, "Sprite") && set) {
		self->img = ScorerImg_Replace(self->img, *s, 2, 0x10060);
		return 1;
	}
	if (i) {
		if (!crt_stricmp(name, "TargetRectIndex")) {
			return ScorerVar_Int(&self->targetRectIndex, set, i);
		}
		if (!crt_stricmp(name, "TargetArrowIndex")) {
			return ScorerVar_Int(&self->targetArrowIndex, set, i);
		}
		if (!crt_stricmp(name, "EnemyFireIndex")) {
			return ScorerVar_Int(&self->enemyFireIndex, set, i);
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

void __thiscall cTargetScorer_Draw(cTargetScorer *self, int player) {
	cEntity *ship;

	if (!ScorerHud_CamShip(player, &ship)) {
		return;
	}
	self->testTime = self->testTime - Scorer_Get(player)->frameTime;
	cTargetScorer_DrawTarget(self, ship);
	cTargetScorer_DrawEnemies(self, ship);
	/* the visibility tests of the next frames run four times a second */
	if (0.0f >= self->testTime) {
		self->testTime = 0.25f;
	}
}
