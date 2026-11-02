/*
 * HeadScorer.cpp of the game (Menu/): cHeadScorer, a talking head of the scorer models. See
 * include/scrap/scoreritems.h.
 */
#include "scrap/embeddedfxchar.h"
#include "scrap/console.h"
#include "scrap/scoreritems.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapworld.h"

#define HEAD_FOV 0.87266463f				/* 50 degrees */

const double g_HeadRandOfs;

const double g_HeadRandRange = 6.2831854820251465;

cHeadScorer *cHeadScorer_ctor(cHeadScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CHEADSCORER_VFTABLE;
	CGameCamera *cam = operator_new(sizeof(CGameCamera));
	self->cam = cam ? CGameCamera_ctor(cam) : NULL;
	self->model = NULL;
	self->anim = 1;
	self->vw = 0.0f;
	self->vh = 0.0f;
	self->time = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * g_HeadRandRange + g_HeadRandOfs);
	return self;
}

void cHeadScorer_dtor(cHeadScorer *self) {
	CGameCamera *cam = self->cam;
	self->base.vftable = CHEADSCORER_VFTABLE;
	if (cam) {
		CGameCamera_dtor(cam);
		operator_delete_sized(cam, sizeof(CGameCamera));
		self->cam = NULL;
	}
	cScorerItem_dtor(&self->base);
}

cScorerItem *cHeadScorer_Delete(cScorerItem *base, byte flags) {
	cHeadScorer *self = (cHeadScorer *)base;
	cHeadScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cHeadScorer));
	}
	return base;
}

int cHeadScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cHeadScorer *self = (cHeadScorer *)base;
	if (set && s && !crt_stricmp(name, "HeadModel")) {
		HeadScorer_SetModel(self, *s);
		return 1;
	}
	if (i && !crt_stricmp(name, "HeadAnimIndex")) {
		if (set) {
			HeadScorer_SetAnim(self, *i);
		} else {
			*i = self->anim;
		}
		return 3;
	}
	if (d) {
		if (!crt_stricmp(name, "VW")) {
			return ScorerVar_Float(&self->vw, set, d);
		}
		if (!crt_stricmp(name, "VH")) {
			return ScorerVar_Float(&self->vh, set, d);
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

void cHeadScorer_Draw(cScorerItem *base, int player) {
	cHeadScorer *self = (cHeadScorer *)base;
	vec3_t tmp;

	if (!self->model) {
		return;
	}
	float ft = Scorer_Get(player)->frameTime;
	Render_ClearRect(self->base.x, self->base.y, self->vw, self->vh, 0, 0);
	Eng_SetViewRect(self->base.x, self->base.y, self->vw, self->vh);
	CGameCamera *cur = Camera_GetCurrent();
	self->time = ft + ft + self->time;
	if (cur) {
		CGameCamera_Copy(self->cam, cur);
		CGameCamera_SetLookAt(self->cam, 1);
		float sx = (float)sin((double)self->time) * 10.0f;
		cScorerModel *m = self->model;
		float sy = (float)cos((double)(self->time * 0.5f)) * 10.0f + m->height;
		float sz = m->radius * -5.5f;
		const vec3_t *p = CGameModel_GetPos(&m->model, &tmp);
		float py = p->y;
		CGameCamera *cam = self->cam;
		cam->pos.z = p->z + sz;
		cam->pos.x = p->x + sx;
		cam->pos.y = py + sy;
		float h = self->model->height;
		p = CGameModel_GetPos(&self->model->model, &tmp);
		cam = self->cam;
		cam->target.x = p->x + 0.0f;
		cam->target.z = p->z + 0.0f;
		cam->target.y = p->y + h;
		self->cam->fov = HEAD_FOV;
		self->cam->aspectScreen = self->vw / self->vh;
		Camera_SetCurrent(self->cam);
		CGameModel_RootMotion(&self->model->model, ft, NULL, NULL);
		CGameModel_Enable(&self->model->model);
		CGameModel_RenderNow(&self->model->model, 2);
		cEmbeddedFX *extra = self->model->extra;
		if (extra) {
			extra->vftable->Tick(extra);
		}
		CGameModel_Disable(&self->model->model);
		Camera_SetCurrent(cur);
	}
	Eng_ResetViewRect();
}

byte cHeadScorer_SetModel(cScorerItem *base, const char *name) {
	cHeadScorer *self = (cHeadScorer *)base;
	cScorerModel *m = cScorer_FindModel(Scorer_Get(self->base.player), name);

	self->model = m;
	if (m) {
		CHECK(m->numAnims > 0);
		if (self->model->numAnims <= self->anim) {
			Con_Debugf("Minority Report : Head with invalid animation (%d), reseted\n", self->anim);
			self->anim = 0;
		}
		CGameModel_SetAnim(&self->model->model, self->model->anims[self->anim], 0, 0);
		CGameModel_SetAnimLoop(&self->model->model, self->model->anims[self->anim], 1);
	}
	return self->model != NULL;
}

void cHeadScorer_SetAnim(cScorerItem *base, int anim) {
	cHeadScorer *self = (cHeadScorer *)base;
	cScorerModel *m = self->model;

	if (!m) {
		return;
	}
	CHECK(anim >= 0 && anim < m->numAnims);
	byte changed = self->anim != anim && self->model;
	self->anim = anim;
	if (changed) {
		int aidx = self->model->anims[anim];
		CHECK(aidx >= 0);
		CGameModel_SetAnimSpeed(&self->model->model, aidx, 0.0f);
		CGameModel_SetAnim(&self->model->model, aidx, 0, 0);
		CGameModel_SetAnimLoop(&self->model->model, aidx, 1);
	}
}

/* the vtable of cHeadScorer */
const cHeadScorerVtbl cHeadScorer_vftable = {
	.base.Draw = cHeadScorer_Draw,
	.base.SetParam = cScorerItem_SetParam,
	.base.Var = cHeadScorer_Var,
	.base.Delete = cHeadScorer_Delete,
	.SetAnim = cHeadScorer_SetAnim,
	.SetModel = cHeadScorer_SetModel,
};
