/*
 * CannonScorer.cpp of the game (Menu/): cCannonScorer, the crosshair of the ship of the camera. See
 * include/scrap/scoreritems.h.
 */
#include "scrap/app.h"
#include "scrap/scoreritems.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"

/* the lock of the target of a ship: on, and the time of the lock (0 while searching) */

cCannonScorer *cCannonScorer_ctor(cCannonScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CCANNONSCORER_VFTABLE;
	self->numWeapons = g_pWorld->weapons.count;
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x20) : NULL;
	self->img = img;
	img->color = 0xffffffff;
	self->angle = 0.0f;
	self->size[0] = 20.0f;
	self->size[1] = 20.0f;
	self->color = 0x80ffff00;
	self->shown = 1;
	self->base.w = 1.0f;
	self->base.h = 1.0f;
	return self;
}

void cCannonScorer_dtor(cCannonScorer *self) {
	CPsaImage *img = self->img;
	self->base.vftable = CCANNONSCORER_VFTABLE;
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, sizeof(CPsaImage));
	}
	cScorerItem_dtor(&self->base);
}

cScorerItem *cCannonScorer_Delete(cScorerItem *base, byte flags) {
	cCannonScorer *self = (cCannonScorer *)base;
	cCannonScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cCannonScorer));
	}
	return base;
}

/* the texture rectangle of a frame as the corners of a quad */
static void Cannon_FrameQuad(CPsaImage *img, int frame, float *uv) {
	float uv0[2], uv1[2];

	CPsaImage_GetFrameUV(img, frame, uv0, uv1);
	uv[0] = uv0[0];
	uv[1] = uv0[1];
	uv[2] = uv1[0];
	uv[3] = uv0[1];
	uv[4] = uv1[0];
	uv[5] = uv1[1];
	uv[6] = uv0[0];
	uv[7] = uv1[1];
}

void cCannonScorer_DrawAim(cCannonScorer *self, int weapon, const float *pos, const float *scale,
	const float *unused, float angle, uint32_t color) {
	static const float corners[4][2] = {{-0.5f, -0.375f}, {0.5f, -0.375f}, {0.5f, 0.375f}, {-0.5f, 0.375f}};
	float uv0[2], uv1[2], quad[4][2], uv[8], ofs[2], tmp[2];

	(void)unused;
	if (!self->aimIndex[weapon]) {
		return;
	}
	CPsaImage *img = self->img;
	img->color = color;
	img = self->img;
	float sx = scale[0] * 0.05f * 24.0f;
	float sy = scale[1] * 0.05f * 24.0f;
	img->scaleX = sx;
	img->scaleY = sy;
	img->pivotX = 0.0f;
	img->pivotY = 0.0f;
	CPsaImage_GetFrameUV(img, self->aimIndex[weapon], uv0, uv1);
	for (int k = 0; k < 3; k++) {
		uv[0] = uv0[0];
		uv[1] = uv0[1];
		uv[2] = uv1[0];
		uv[3] = uv0[1];
		uv[4] = uv1[0];
		uv[5] = uv1[1];
		uv[6] = uv0[0];
		uv[7] = uv1[1];
		float a = (float)k * 2.0943952f + angle;
		for (int c = 0; c < 4; c++) {
			const float *r = Vec2_Rotate(tmp, corners[c], a);
			quad[c][0] = r[0];
			quad[c][1] = r[1];
		}
		ofs[0] = scale[0] * 0.05f * 0.0f;
		ofs[1] = scale[1] * 0.05f * (weapon == 6 ? 8.0f : 12.0f);
		const float *r = Vec2_Rotate(tmp, ofs, a);
		float ry = r[1];
		img = self->img;
		img->x = pos[0] + r[0];
		img->y = ry + pos[1];
		CPsaImage_DrawPoly(img, &quad[0][0], uv, 0, 4, 6);
	}
}

void cCannonScorer_DrawShipAim(cCannonScorer *self, int player, cEntity *ship) {
	vec3_t aim;
	float screen[2];
	byte shown;

	cAutoAim_GetAimPos(&Veh(ship)->autoAim, &aim, ship);
	if (!Engine_WorldToScreen(&aim, screen, NULL)) {
		return;
	}
	if (g_SplitScreenGame) {
		screen[1] *= 0.5f;
		if (player == 1) {
			screen[1] += 240.0f;
		}
	}
	cWeaponManager *wm = &Veh(ship)->weapons;
	if (!(byte)cWeaponManager_HasMinAmmo(wm, wm->curWeap)
		&& !((int)(Timer_GetTime(&g_pWorld->timer) * 10.0f) & 1)) {
		/* without ammo the aim blinks */
		shown = 0;
	} else {
		int w = wm->curWeap;
		shown = 1;
		uint32_t color = w == Controls_GetWeapon(&Veh(ship)->controls) ? self->color : 0xc0ffffff;
		cCannonScorer_DrawAim(self, w, screen, self->size, &self->base.w, self->angle, color);
	}
	if (self->shown && !shown) {
		const char *snd = g_CannonNoAmmoSound;
		g_pSound->vftable->Play(g_pSound, snd, Snd_Volume(g_CannonNoAmmoSoundVol), 0);
	}
	self->shown = shown;
}

void cCannonScorer_DrawCenter(cCannonScorer *self, int player, cEntity *ship) {
	static const float corners[8] = {-0.25f, -0.25f, 0.25f, -0.25f, 0.25f, 0.25f, -0.25f, 0.25f};
	float uv[8], y;

	if (!self->crossHairIndex) {
		return;
	}
	CPsaImage *img = self->img;
	img->pivotX = 0.0f;
	img->pivotY = 0.0f;
	img = self->img;
	img->scaleX = 32.0f;
	img->scaleY = 32.0f;
	self->img->color = 0x80ffffff;
	Cannon_FrameQuad(self->img, self->crossHairIndex, uv);
	if (g_SplitScreenGame) {
		y = player == 0 ? -120.0f + 240.0f : 120.0f + 240.0f;
	} else {
		y = 240.0f;
	}
	img = self->img;
	img->x = 320.0f;
	img->y = y;
	CPsaImage_DrawPoly(self->img, corners, uv, 0, 4, 6);
	((const cCannonScorerVtbl *)self->base.vftable)->DrawLock(self, player, &self->img->x);
	if (WLife(ship)->life * 100 / Veh(ship)->maxLife < g_WarningLifeLevel) {
		cScorer *sc = Scorer_Get(player);
		int v = Scorer_Abs((int)(sc->time * 1024.0f) % 511 - 255);
		CFont_PrintfAlignColor(g_FontHoratioSmall, self->img->x, self->img->y - 50.0f, TEXTSCORER_CENTER,
			((uint32_t)v | 0xff00) << 8, "%s", "DANGER");
	}
}

int cCannonScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cCannonScorer *self = (cCannonScorer *)base;
	if (s && !crt_stricmp(name, "Sprite") && set) {
		if (self->img) {
			CPsaImage_dtor(self->img);
			operator_delete_sized(self->img, sizeof(CPsaImage));
		}
		CPsaImage *img = operator_new(sizeof(CPsaImage));
		img = img ? CPsaImage_ctor(img, *s, 0x10064) : NULL;
		self->img = img;
		img->color = 0xffffffff;
		return 1;
	}
	if (i) {
		if (!crt_stricmp(name, "CrossHairIndex")) {
			return ScorerVar_Int(&self->crossHairIndex, set, i);
		}
		if (!crt_stricmp(name, "AimAc")) {
			return ScorerVar_Int(&self->aimAc, set, i);
		}
		if (!crt_stricmp(name, "AimIndex")) {
			return ScorerVar_Int(&self->aimIndex[self->aimAc], set, i);
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

void cCannonScorer_Draw(cScorerItem *base, int player) {
	cCannonScorer *self = (cCannonScorer *)base;
	const char *camName = cWorld_GetCamName(g_pWorld, player);
	cEntity *ship = NULL;

	if (*camName) {
		cEntity *cam = World_FindEntity(camName);
		if (cam) {
			if (!cam->vftable->IsType(cam, "FCam")) {
				return;
			}
			ship = cCamEntity_GetTarget(cam);
			if (!ship || !ship->vftable->IsType(ship, TYEN_CAR)) {
				return;
			}
		}
	}
	cCannonScorer_DrawCenter(self, player, ship);
	if (!WithLife_Vtbl(ship)->IsEMIed(ship)) {
		cPy_Function *f = &TABLE_AT(g_pWorld->weapons, Veh(ship)->weapons.curWeap)->setFx;
		cPy_Function_AddParamS(f, ship->name);
		cPy_Function_AddParamI(f, player);
		cPy_Function_AddParamS(f, self->base.name);
		cPy_Function_Execute(f, 0);
		cCannonScorer_DrawShipAim(self, player, ship);
	}
}

void cCannonScorer_DrawLock(cCannonScorer *self, int player, const float *pos) {
	cEntity *e = g_pWorld->owners[player];
	const char *text;
	uint32_t color;
	int v;

	(void)self;
	CHECK(!e || e->vftable->IsType(e, TYEN_WITH_LIFE));
	e = g_pWorld->owners[player];
	if (!e || !e->vftable->IsType(e, TYEN_CAR) || !Veh(e)->controls.follow) {
		return;
	}
	float t = Veh(e)->controls.attackTime;
	if (t == 0.0f) {
		text = "Searching...";
		v = Scorer_Abs((int)(Scorer_Get(player)->time * 1024.0f) % 511 - 255);
		color = ((uint32_t)v << 8 | (uint32_t)v) << 8;
	} else {
		if (!(1.5f >= t)) {
			return;
		}
		text = "Target Lock";
		v = Scorer_Abs((int)(t * 1024.0f) % 511 - 255);
		color = ((uint32_t)v | 0xff00) << 8;
	}
	CFont_PrintfAlignColor(g_FontHoratio, pos[0], pos[1] - 50.0f, TEXTSCORER_CENTER, color, "%s", text);
}

/* the vtable of cCannonScorer */
const cCannonScorerVtbl cCannonScorer_vftable = {
	.base.Draw = cCannonScorer_Draw,
	.base.SetParam = cScorerItem_SetParam,
	.base.Var = cCannonScorer_Var,
	.base.Delete = cCannonScorer_Delete,
	.DrawLock = cCannonScorer_DrawLock,
};
