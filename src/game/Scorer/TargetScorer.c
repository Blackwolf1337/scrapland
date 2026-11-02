/*
 * TargetScorer.cpp of the game (Menu/): cTargetScorer, the target of the ship of the camera and the enemies aiming
 * at it. See include/scrap/scorerhud.h.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/camcontrol.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/scorerhud.h"
#include "scrap/scrapcollision.h"
#include "scrap/timer.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"
#include "scrap/withlifeentity.h"

cGVar GVar_LittlePointingArrowSize;

#define g_EnemyMarkScale (GVar_LittlePointingArrowSize.value.f)	/* the size of the marks of DrawEnemies, none for 0 */
#define TS_PULSE_SPEED 4.0f
#define TS_PULSE_RANGE -254.0
#define TS_MARK_HEIGHT 380.0f
#define TS_HALF_SCREEN 240.0f
cGVar GVar_TargetScorerSwitchSound;

#define g_TargetSwitchSound (GVar_TargetScorerSwitchSound.value.s)	/* TargetScorerSwitchSound */
cGVar GVar_TargetScorerSwitchSoundVol;

#define g_TargetSwitchSoundVol (GVar_TargetScorerSwitchSoundVol.value.f)
cGVar GVar_EnemyFireSize;

#define g_EnemyFireSize (GVar_EnemyFireSize.value.f)		/* the scale of the arrow to the attacker */
extern cGVar GVar_AttackerMarkTime;
#define g_AttackerMarkTime (GVar_AttackerMarkTime.value.f)		/* the attacker arrow shows this long after a hit */
#define TS_RECT_SIZE 380.0f		/* the rectangle per radius and depth */
#define TS_RECT_MIN 12.0f
#define TS_ARROW_TARGET 0.6f		/* the arrows on this part of the way to the edge */
#define TS_ARROW_ATTACKER 0.3f
#define TS_ASPECT 1.333f

/* the y of a screen point in the view of the player in split screen */
static void Target_SplitY(cTargetScorer *self, float *scr) {
	if (g_SplitScreenGame) {
		scr[1] = scr[1] * 0.5f;
		if (self->base.player == 1) {
			scr[1] = scr[1] + TS_HALF_SCREEN;
		}
	}
}

/* the arrow towards a point off the screen: its angle (down: the frame points down) and its place on the way to the
 * edge */
static float Target_ArrowPos(cTargetScorer *self, const vec3_t *p, float part, byte down, float *scr) {
	Engine_DirToScreen(p, scr);
	float sx = scr[0], sy = scr[1];
	float a = down ? -(float)ScorerHud_ScreenAtan2(sx, sy) : ScorerHud_ArrowAngle(sx, sy);
	scr[0] = (sx - 320.0f) / TS_ASPECT * part + 320.0f;
	scr[1] = (sy - TS_HALF_SCREEN) * part + TS_HALF_SCREEN;
	Target_SplitY(self, scr);
	return a;
}

/* the mark of the target e of the ship: the rectangle on the screen, else the arrow; red for an enemy (green in a
 * network game of team 0), blinking white while it is in sight; TargetView of the scorer follows the view */
static void Target_DrawMark(cTargetScorer *self, cEntity *ship, cEntity *e) {
	float scr[2], depth;
	vec3_t tmp, pos;
	cWithLifeEntity *sw = (cWithLifeEntity *)ship, *ew = (cWithLifeEntity *)e;
	const cObjEntityVtbl *vt = (const cObjEntityVtbl *)e->vftable;

	float old = self->view;
	if ((byte)cVehicleEntity_CanFire(Veh(e))) {
		float v = Scorer_Get(self->base.player)->frameTime + self->view;
		self->view = v;
		if (v > 1.0f) {
			self->view = 1.0f;
		}
	} else {
		self->view = 0.0f;
	}
	pos = *vt->GetViewPos(e, &tmp);
	if (0.0f >= self->testTime) {
		const vec3_t *from = ship->vftable->GetPos(ship, &tmp);
		ew->arrow.visible = !Engine_TestLine(from, &pos, NULL, NULL);
	}
	byte vis = ew->arrow.visible;
	byte on = Engine_WorldToScreen(&pos, scr, &depth);
	Target_SplitY(self, scr);
	if (vis && on) {
		self->view = 0.0f;
	}
	if (self->view != old) {
		const char *val = NULL;
		if (self->view == 0.0f) {
			val = "1";
		} else if (old == 0.0f) {
			val = "0";
		}
		if (val) {
			cScorer_SetParam(Scorer_Get(self->base.player), "TargetView", val);
		}
	}
	uint32_t a = (uint32_t)(int)((1.0f - self->view) * 255.0f) << 24;
	uint32_t enemyColor, otherColor;
	if (sw->arrow.tgType == 0 && (g_pApp->flags & (APPF_CLIENT | APPF_SERVER))) {
		otherColor = a | 0xff0000;
		enemyColor = a | 0xff00;
	} else {
		enemyColor = a | 0xff0000;
		otherColor = a | 0xff00;
	}
	int blink = (int)(Scorer_Get(self->base.player)->time * 20.0f) % 8;
	uint32_t color = Team_IsEnemy(sw->arrow.tgType, ew->arrow.tgType) ? enemyColor : otherColor;
	if (vis && (blink == 0 || blink == 2)) {
		color = (uint32_t)(int)((1.0f - self->view) * 255.0f) << 24 | 0xffffff;
	}
	CPsaImage *img = self->img;
	img->color = color;
	img->scaleX = 1.0f;
	img->scaleY = 1.0f;
	if (on) {
		float d = (float)((vt->GetRadius(e)) * TS_RECT_SIZE * depth);
		if (TS_RECT_MIN > d) {
			d = TS_RECT_MIN;
		}
		ScorerHud_DrawCorners(img, (vis == 0) + self->targetRectIndex, scr, d);
	} else {
		float ang = Target_ArrowPos(self, &pos, TS_ARROW_TARGET, 0, scr);
		ScorerHud_DrawArrow(img, (vis == 0) + self->targetArrowIndex, ang, scr[0], scr[1], 0);
	}
}

/* the arrow towards the ship that hit the ship last, fading out over AttackerMarkTime (not while it burns) */
static void Target_DrawAttacker(cTargetScorer *self, cEntity *ship) {
	float scr[2], depth;
	vec3_t tmp, pos;
	cWithLifeEntity *sw = (cWithLifeEntity *)ship;

	float t = sw->attack.hitTime + g_AttackerMarkTime;
	float r = (float)(t - (Timer_GetTime(&sw->obj.timer)));
	float f = (r + r) / g_AttackerMarkTime;
	if (!(f > 0.0f) || !(0.0f >= Veh(ship)->wl.fireRemain)) {
		return;
	}
	cEntity *e = World_FindEntity(Nto1Node_TargetName(&sw->attack.hitBy));
	if (!e || !Entity_IsType(e, TYEN_CAR)) {
		return;
	}
	pos = *((const cObjEntityVtbl *)e->vftable)->GetViewPos(e, &tmp);
	if (Engine_WorldToScreen(&pos, scr, &depth)) {
		return;
	}
	float ang = Target_ArrowPos(self, &pos, TS_ARROW_ATTACKER, 1, scr);
	CPsaImage *img = self->img;
	if (f > 1.0f) {
		img->color = ((uint32_t)(int)((f - 1.0f) * 128.0f) | 0xffffff00) << 8;
	} else {
		img->color = (uint32_t)(int)(f * 255.0f) << 24 | 0xff0000;
	}
	img->scaleX = g_EnemyFireSize;
	img->scaleY = g_EnemyFireSize;
	ScorerHud_DrawArrow(img, self->enemyFireIndex, ang, scr[0], scr[1], 1);
	img->scaleX = 1.0f;
	img->scaleY = 1.0f;
}

void cTargetScorer_DrawTarget(cTargetScorer *self, cEntity *ship) {
	cVehicleEntity *v = Veh(ship);

	if (self->target[0] && strcmp(self->target, v->target)) {
		Scorer_PlaySound(g_TargetSwitchSound, g_TargetSwitchSoundVol);
	}
	Str_Copy(self->target, v->target, sizeof(self->target));
	if (self->target[0]) {
		cEntity *e = World_FindEntity(self->target);
		if (e && Entity_IsType(e, TYEN_CAR)) {
			Target_DrawMark(self, ship, e);
		}
	}
	Target_DrawAttacker(self, ship);
}

cTargetScorer *cTargetScorer_ctor(cTargetScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CTARGETSCORER_VFTABLE;
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

void cTargetScorer_dtor(cTargetScorer *self) {
	self->base.vftable = CTARGETSCORER_VFTABLE;
	ScorerImg_Free(self->img);
	cScorerItem_dtor(&self->base);
}

cScorerItem *cTargetScorer_Delete(cScorerItem *base, byte flags) {
	cTargetScorer *self = (cTargetScorer *)base;
	cTargetScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cTargetScorer));
	}
	return base;
}

int cTargetScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cTargetScorer *self = (cTargetScorer *)base;
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

/* a mark over the entities with life in view that are enemies or friends of the ship, pulsing red for the enemies
 * (blue in a network game of team 0), the visibility from the camera tested when testTime ran out */
void cTargetScorer_DrawEnemies(cTargetScorer *self, cEntity *ship) {
	float uv0[2], uv1[2], pos[4][2], uv[4][2], scr[2], depth;
	vec3_t tmp, cam;

	if (!(g_EnemyMarkScale > 0.0f)) {
		return;
	}
	cEntity *e = cWorld_GetFirstEntity(g_pWorld);
	CPsaImage_GetFrameUV(self->img, self->enemyIndex, uv0, uv1);
	pos[0][0] = uv0[0] - uv0[0];
	pos[0][1] = uv0[1] - uv0[1];
	pos[1][0] = uv1[0] - uv0[0];
	pos[1][1] = uv0[1] - uv0[1];
	pos[2][0] = uv1[0] - uv0[0];
	pos[2][1] = uv1[1] - uv0[1];
	pos[3][0] = uv0[0] - uv0[0];
	pos[3][1] = uv1[1] - uv0[1];
	uv[0][0] = uv0[0];
	uv[0][1] = uv0[1];
	uv[1][0] = uv1[0];
	uv[1][1] = uv0[1];
	uv[2][0] = uv1[0];
	uv[2][1] = uv1[1];
	uv[3][0] = uv0[0];
	uv[3][1] = uv1[1];
	self->img->scaleX = g_EnemyMarkScale;
	self->img->scaleY = g_EnemyMarkScale;
	double v = fmod((double)Timer_GetTime(&g_pWorld->timer) * (double)TS_PULSE_SPEED, 1.0);
	int c = -0x80 - (int)(v * TS_PULSE_RANGE);
	c = c < 0 ? -c : c;
	uint32_t enemyColor, friendColor;
	cWithLifeEntity *sw = (cWithLifeEntity *)ship;
	if (sw->arrow.tgType == 0 && (g_pApp->flags & (APPF_CLIENT | APPF_SERVER))) {
		friendColor = (((uint32_t)c | 0xffffff00u) << 8) | (uint32_t)c;
		enemyColor = ((uint32_t)c << 16) | (uint32_t)c | 0xff00ff00u;
	} else {
		enemyColor = (((uint32_t)c | 0xffffff00u) << 8) | (uint32_t)c;
		friendColor = ((uint32_t)c << 16) | (uint32_t)c | 0xff00ff00u;
	}
	self->img->pivotY = uv1[1] - uv0[1];
	self->img->pivotX = (uv1[0] - uv0[0]) * 0.5f;
	for (; e; e = cEntity_Next(e)) {
		cWithLifeEntity *w = (cWithLifeEntity *)e;
		if (e == ship || !e->active || !e->vftable->IsType(e, "WithLife") || w->arrow.canMission) {
			continue;
		}
		byte enemy = Team_IsEnemy(sw->arrow.tgType, w->arrow.tgType);
		byte isFriend = Team_IsFriend(sw->arrow.tgType, w->arrow.tgType);
		if (!enemy && !isFriend) {
			continue;
		}
		const cObjEntityVtbl *vt = (const cObjEntityVtbl *)e->vftable;
		byte on = Engine_WorldToScreen(vt->GetViewPos(e, &tmp), scr, &depth);
		Target_SplitY(self, scr);
		if (!on) {
			continue;
		}
		cam = g_pWorld->camPos[g_pWorld->curCam];
		cEntity *fc = w->obj.cam;
		if (fc) {
			CamCtl_Vtbl(fc)->GetCamPos(fc, &cam.x, &cam.y, &cam.z);
		}
		if (0.0f >= self->testTime) {
			w->arrow.visible = !Engine_TestLine(vt->GetViewPos(e, &tmp), &cam, NULL, NULL);
		}
		if (!w->arrow.visible) {
			continue;
		}
		float h = (float)((vt->GetRadius(e)) * TS_MARK_HEIGHT * depth);
		self->img->color = enemy ? enemyColor : friendColor;
		self->img->x = scr[0];
		self->img->y = scr[1] - h;
		CPsaImage_DrawPoly(self->img, &pos[0][0], &uv[0][0], NULL, 4, 6);
	}
}

void cTargetScorer_Draw(cScorerItem *base, int player) {
	cTargetScorer *self = (cTargetScorer *)base;
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

/* the vtable of cTargetScorer */
const cScorerItemVtbl cTargetScorer_vftable = {
	.Draw = cTargetScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cTargetScorer_Var,
	.Delete = cTargetScorer_Delete,
};
