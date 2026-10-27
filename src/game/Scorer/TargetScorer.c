/*
 * TargetScorer.cpp of the game (Menu/): cTargetScorer, the target of the ship of the camera and the enemies aiming
 * at it. See include/scrap/scorerhud.h.
 */
#include "scrap/camcontrol.h"
#include "scrap/mathlib.h"
#include "scrap/scorerhud.h"
#include "scrap/scrapcollision.h"
#include "scrap/timer.h"
#include "scrap/withlifeentity.h"

#define g_EnemyMarkScale GAME_DATA(float, 0x462260)	/* the size of the marks of DrawEnemies, none for 0 */
#define TS_PULSE_SPEED 4.0f		/* rva 0x3a5aac */
#define TS_PULSE_RANGE -254.0		/* rva 0x3c5018 */
#define TS_MARK_HEIGHT 380.0f		/* rva 0x3c5010 */
#define TS_HALF_SCREEN 240.0f		/* rva 0x3c23c8 */

/* the x87 a * b * c of the original (a on the stack from a call), rounded by the control word, then to float */
static inline float TS_X87MulMul(long double a, float b, float c) {
	long double r;

	__asm__("fmuls %2\n\tfmuls %3" : "=t"(r) : "0"(a), "m"(b), "m"(c));
	return (float)r;
}

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

/* a mark over the entities with life in view that are enemies or friends of the ship, pulsing red for the enemies
 * (blue in a network game of team 0), the visibility from the camera tested when testTime ran out */
void __thiscall cTargetScorer_DrawEnemies(cTargetScorer *self, cEntity *ship) {
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
	double v = Game_Fmod((double)Timer_GetTime(&g_pWorld->timer) * (double)TS_PULSE_SPEED, 1.0);
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
		if (e == ship || !e->flag_4c || !e->vftable->IsType(e, "WithLife") || w->arrow.canMission) {
			continue;
		}
		byte enemy = Team_IsEnemy(sw->arrow.tgType, w->arrow.tgType);
		byte isFriend = Team_IsFriend(sw->arrow.tgType, w->arrow.tgType);
		if (!enemy && !isFriend) {
			continue;
		}
		const cObjEntityVtbl *vt = (const cObjEntityVtbl *)e->vftable;
		byte on = Engine_WorldToScreen(vt->GetViewPos(e, &tmp), scr, &depth);
		if (g_SplitScreenGame) {
			scr[1] = scr[1] * 0.5f;
			if (self->base.player == 1) {
				scr[1] = scr[1] + TS_HALF_SCREEN;
			}
		}
		if (!on) {
			continue;
		}
		cam = g_pWorld->camPos[g_pWorld->curCam];
		cEntity *fc = w->obj.cam;
		if (fc) {
			CamCtl_Vtbl(fc)->GetCamPos(fc, &cam.x, &cam.y, &cam.z);
		}
		if (0.0f >= self->testTime) {
			w->arrow.b71 = !Engine_TestLine(vt->GetViewPos(e, &tmp), &cam, NULL, NULL);
		}
		if (!w->arrow.b71) {
			continue;
		}
		float h = TS_X87MulMul(vt->GetRadius(e), TS_MARK_HEIGHT, depth);
		self->img->color = enemy ? enemyColor : friendColor;
		self->img->x = scr[0];
		self->img->y = scr[1] - h;
		CPsaImage_DrawPoly(self->img, &pos[0][0], &uv[0][0], NULL, 4, 6);
	}
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
