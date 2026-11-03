/*
 * HangarShipScorer.cpp of the game (Menu/): cHangarShipScorer, a ship turning in a view of its own. See
 * include/scrap/scoreritems.h.
 */
#include "scrap/scoreritems.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapworld.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"

#define CHANGARSHIPSCORER_VFTABLE (&cHangarShipScorer_vftable.base)
static const cSpriteScorerVtbl cHangarShipScorer_vftable;

static const char *const s_GridColors[4] = {"GridColorRed", "GridColorGreen", "GridColorBlue", "GridColorAlpha"};

cHangarShipScorer *cHangarShipScorer_ctor(cHangarShipScorer *self) {
	cSpriteScorer_ctor(&self->base);
	self->base.base.vftable = CHANGARSHIPSCORER_VFTABLE;
	self->grid = NULL;
	cString_ctor(&self->targetName);
	self->reset = 1;
	self->gridIndex = 0;
	self->gridColor = 0;
	self->camDist = 100.0f;
	self->camFov = 20.0f;
	CGameCamera *cam = operator_new(sizeof(CGameCamera));
	self->cam = cam ? CGameCamera_ctor(cam) : NULL;
	return self;
}

static void cHangarShipScorer_dtor(cHangarShipScorer *self) {
	CGameCamera *cam = self->cam;
	self->base.base.vftable = CHANGARSHIPSCORER_VFTABLE;
	if (cam) {
		CGameCamera_dtor(cam);
		operator_delete_sized(cam, sizeof(CGameCamera));
	}
	if (self->grid) {
		CPsaImage_dtor(self->grid);
		operator_delete_sized(self->grid, sizeof(CPsaImage));
	}
	cString_dtor(&self->targetName);
	cSpriteScorer_dtor(&self->base);
}

static cScorerItem *cHangarShipScorer_Delete(cScorerItem *base, byte flags) {
	cHangarShipScorer *self = (cHangarShipScorer *)base;
	cHangarShipScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cHangarShipScorer));
	}
	return base;
}

/* GridSpriteIndex, GridColorRed/Green/Blue/Alpha, TargetName, CamDist, CamFov */
static int cHangarShipScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cHangarShipScorer *self = (cHangarShipScorer *)base;
	int r;

	if (i) {
		if (!crt_stricmp(name, "GridSpriteIndex")) {
			if (!set) {
				*i = self->gridIndex;
				return 3;
			}
			CPsaImage *old = self->grid;
			self->gridIndex = *i;
			if (old) {
				CPsaImage_dtor(old);
				operator_delete_sized(old, sizeof(CPsaImage));
			}
			CPsaImage *img = operator_new(sizeof(CPsaImage));
			self->grid = img ? CPsaImage_ctor(img, self->base.file, 0x60) : NULL;
			return 3;
		}
		if ((r = ScorerVar_Color(name, s_GridColors, &self->gridColor, set, i))) {
			return r;
		}
	}
	if (s && !crt_stricmp(name, "TargetName")) {
		if (set) {
			self->reset = 1;
			cString_Set(&self->targetName, *s);
		} else {
			*s = self->targetName;
		}
		return 1;
	}
	if (d) {
		if (!crt_stricmp(name, "CamDist")) {
			return ScorerVar_Float(&self->camDist, set, d);
		}
		if (!crt_stricmp(name, "CamFov")) {
			return ScorerVar_Float(&self->camFov, set, d);
		}
	}
	return cSpriteScorer_Var(&self->base.base, name, set, s, d, i);
}

static void cHangarShipScorer_Draw(cScorerItem *base, int player) {
	cHangarShipScorer *self = (cHangarShipScorer *)base;
	cScorerItem *it = &self->base.base;
	vec3_t tmp, ang;

	if (!self->targetName) {
		return;
	}
	Render_ClearRect(it->x, it->y, it->w, it->h, 1, 0);
	Eng_SetViewRect(it->x, it->y, it->w, it->h);
	cEntity *ship = World_FindEntity(self->targetName);
	if (ship && !Entity_IsType(ship, TYEN_CAR)) {
		ship = NULL;
	}
	CGameCamera *cur = Camera_GetCurrent();
	if (ship && cur) {
		CGameCamera_Copy(self->cam, cur);
		CGameCamera_SetLookAt(self->cam, 1);
		self->cam->aspectScreen = it->w / it->h;
		float dz = self->camDist * -1.0f;
		float dxy = self->camDist * 0.0f;
		const vec3_t *c = Obj_GetViewPos(ship, &tmp);
		float cy = c->y;
		CGameCamera *cam = self->cam;
		cam->pos.z = c->z + dz;
		cam->pos.x = c->x + dxy;
		cam->pos.y = cy + dxy;
		self->cam->fov = (float)((double)self->camFov * 3.141592653589793 / 180.0);
		c = Obj_GetViewPos(ship, &tmp);
		self->cam->target = *c;
		Camera_SetCurrent(self->cam);
		Obj_GetAng(ship, &ang);
		uint32_t fx = CGameModel_GetEffect(&Veh(ship)->wl.obj.model, NULL);
		byte reset = self->reset;
		self->reset = 0;
		if (!reset) {
			float r = Scorer_Get(player)->frameTime * 0.29670596f + self->rot.y;
			self->rot.y = r;
			if (r >= 6.2831855f) {
				self->rot.y = r - 6.2831855f;
			}
			Obj_SetAng(ship, &self->rot);
		} else {
			self->rot.x = 0.0f;
			self->rot.y = 0.0f;
			self->rot.z = 0.0f;
			Obj_SetAng(ship, &self->rot);
		}
		if (self->grid) {
			self->grid->color = self->gridColor;
			CPsaImage_DrawFrame(self->grid, self->gridIndex, it->x, it->y);
		}
		Obj_SetEffect(ship, 4);
		cVehicleEntity_UpdatePilotPos(Veh(ship), 1);
		Obj_Render(ship);
		Obj_SetAng(ship, &ang);
		Obj_SetEffect(ship, fx);
		Camera_SetCurrent(cur);
	}
	Eng_ResetViewRect();
}

/* the vtable of cHangarShipScorer */
static const cSpriteScorerVtbl cHangarShipScorer_vftable = {
	.base.Draw = cHangarShipScorer_Draw,
	.base.SetParam = cScorerItem_SetParam,
	.base.Var = cHangarShipScorer_Var,
	.base.Delete = cHangarShipScorer_Delete,
	.SetMultiSprite = cSpriteScorer_SetMultiSprite,
};
