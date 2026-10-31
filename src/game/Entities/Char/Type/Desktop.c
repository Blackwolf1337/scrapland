/*
 * Entities/Char/Type/Desktop.cpp of the game: the desktop, who jumps (forward, back or to a side) and can land on
 * a character for money, with the dust of the landing (include/scrap/chartype.h).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/fxelements.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapparticles.h"
#include "scrap/weapons.h"

#define DESKTOP_TWO_PI 6.2831854820251465	/* rva 0x3a5d00, (double)6.2831855f */

/* params: the desktop. A fast landing seen from near enough: four puffs around the point it lands on and a ring of
 * dust thrown out of it. */
void ChType_FXDesktopFallEnd(cAbstractFile *params) {
	char name[0x80];
	vec3_t tmp, one, rot, d, xaxis;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = ChType_CheckChar(name);
	if (WChar_Cam(v)) {
		cPad_Rumble(&g_Pads[0], 0.5f, 0.0f, 0.125f);
	}
	const vec3_t *c = Obj_Vtbl(v)->GetViewPos(v, &tmp);
	if (!FX_IsVisible(c->x, c->y, c->z, 250.0f, g_FXDesktopFallEndDist, NULL, 0)) {
		return;
	}
	cWorld *w = g_pWorld;
	vec3_t cp = w->camPos[w->curCam];
	c = Obj_Vtbl(v)->GetViewPos(v, &tmp);
	float dx = cp.x - c->x;
	float dy = cp.y - c->y;
	float dz = cp.z - c->z;
	if (dy * dy + dx * dx + dz * dz > g_FXDesktopFallEndDist * g_FXDesktopFallEndDist) {
		return;
	}
	vec3_t fall = WChar_FallVel(v);
	if (fall.y > -g_FallSoundVel) {
		return;
	}
	float r = Obj_Vtbl(v)->GetRadius(v);
	vec3_t p = *v->vftable->GetPos(v, &tmp);
	vec3_t hit, to;
	/* the point it lands on: the floor below the feet, else a bit under its center */
	float ax = p.x + 0.0f;
	float ay = p.y + r * -1.39999998f;
	float az = p.z + 0.0f;
	to.x = p.x - 0.0f;
	to.y = p.y - r * 2.0f;
	to.z = p.z - 0.0f;
	if (Engine_TestLine(&p, &to, &hit, NULL)) {
		ax = hit.x + 0.0f;
		ay = hit.y + 10.0f;
		az = hit.z + 0.0f;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXDesktopFallEndFile);
	cDebris *dbr = cDebrisSys_NewElement(sys);
	CGameModel_SetPos(Debris_Model(dbr), Obj_Vtbl(v)->GetViewPos(v, &tmp));
	Debris_Life(dbr) = 1.0f;
	cDebrisSys_Drop(sys, dbr);
	byte *el = Debris_Element(dbr);
	if (!el) {
		return;
	}
	if (!el[0xb5]) {
		*(uint16_t *)(el + 0xb4) = 0x100;
	}
	CGameParticles *pt = (CGameParticles *)(el + 4);
	float bx = ax + 0.0f;
	float by = ay - 5.0f;
	float bz = az + 0.0f;
	pt->birthRate = 0;
	for (int i = 0; i < 4; i++) {
		float r1 = FxElem_Rand(DESKTOP_TWO_PI, 0.0);
		float r2 = FxElem_Rand(DESKTOP_TWO_PI, 0.0);
		one.x = 1.0f;
		one.y = 1.0f;
		one.z = 1.0f;
		rot.x = FxElem_Rand(DESKTOP_TWO_PI, 0.0);
		rot.y = r2;
		rot.z = r1;
		const vec3_t *dir = Vec3_Rotate(&d, &one, &rot);
		float k = FxElem_Rand(2.0, 1.0);
		pt->initPosMin.x = bx + k * dir->x;
		pt->initPosMin.y = by + k * dir->y;
		pt->initPosMin.z = bz + k * dir->z;
		pt->initPosMax = pt->initPosMin;
		pt->initVelMin.x = 0.0f;
		pt->initVelMin.y = 0.0f;
		pt->initVelMin.z = 0.0f;
		pt->initVelMax = pt->initVelMin;
		pt->initAccMin.x = 0.0f;
		pt->initAccMin.y = 0.0f;
		pt->initAccMin.z = 0.0f;
		pt->initAccMax = pt->initAccMin;
		pt->sizeIni = FxElem_Rand(10.0, 10.0);
		pt->sizeEnd = FxElem_Rand(20.0, 80.0);
		pt->sizeLife = 0.200000003f;
		pt->lifeTime = FxElem_Rand(0.09999999403953552, 0.30000001192092896);
		pt->rotVar = 0.0f;
		pt->spriteIni = 2;
		pt->spriteEnd = 2;
		float a = FxElem_Rand(DESKTOP_TWO_PI, 0.0);
		pt->rotIni = a;
		pt->rotEnd = a;
		CGameParticles_Apply(pt);
		CGameParticles_SetColorRampOn(pt, 1);
		CGameParticles_SetColorRamp(pt, GAME_ARRAY(uint32_t, ChType_FXDesktopFallEnd_428050, 0x428050));
		CGameParticles_EmitN(pt, 1, 1);
	}
	/* the ring: every 6 degrees a particle thrown out and up */
	float ang = 0.0f;
	do {
		rot.x = 0.0f;
		rot.y = ang;
		rot.z = 0.0f;
		xaxis.x = 1.0f;
		xaxis.y = 0.0f;
		xaxis.z = 0.0f;
		Vec3_Rotate(&d, &xaxis, &rot);
		float k = FxElem_Rand(2.0, 0.0);
		pt->initPosMin.x = k * d.x + ax;
		pt->initPosMin.y = k * d.y + ay;
		pt->initPosMin.z = k * d.z + az;
		pt->initPosMax = pt->initPosMin;
		float s = FxElem_Rand(40.0, 145.0);
		pt->initVelMin.x = d.x * s;
		pt->initVelMin.y = d.y * s;
		pt->initVelMin.z = d.z * s;
		pt->initVelMax = pt->initVelMin;
		float up = FxElem_Rand(30.0, 115.0);
		float g = FxElem_Rand(0.10000002384185791, 0.6499999761581421);
		pt->initAccMin.y = -pt->initVelMin.y * g + up;
		pt->initAccMin.x = -pt->initVelMin.x * g + 0.0f;
		pt->initAccMin.z = -pt->initVelMin.z * g + 0.0f;
		pt->initAccMax = pt->initAccMin;
		pt->sizeIni = FxElem_Rand(10.0, 25.0);
		pt->sizeEnd = FxElem_Rand(20.0, 75.0);
		pt->sizeLife = 1.0f;
		pt->lifeTime = FxElem_Rand(0.15000003576278687, 0.6499999761581421);
		float a = FxElem_Rand(DESKTOP_TWO_PI, 0.0);
		pt->rotIni = a;
		pt->rotEnd = a;
		pt->rotVar = FxElem_Rand(0.5235987901687622, -0.2617993950843811);
		byte sprite = (byte)((crt_rand() & 1) + 0x11);
		pt->spriteIni = sprite;
		pt->spriteEnd = sprite;
		CGameParticles_Apply(pt);
		CGameParticles_SetColorRampOn(pt, 1);
		int ramp = crt_rand() % 2;
		CGameParticles_SetColorRamp(pt, GAME_ARRAY(uint32_t, ChType_FXDesktopFallEnd_428008, 0x428008) + 9 * ramp);
		float dt = FxElem_Rand(0.15000000596046448, 0.0) + 0.0500000007f;
		CGameParticles_EmitDt(pt, 1, 1, dt);
		ang = ang + 0.104719758f;
	} while (6.28318548f > ang);
}

/* the characters a landing pays twice for */
static byte Desktop_BonusClass(cEntity *t) {
	const char *cls = ChType_ActClass(ChType_ActCtrl(t))->name;

	if (!strcmp(cls, "Gear")) {
		return 1;
	}
	cls = ChType_ActClass(ChType_ActCtrl(t))->name;
	if (!strcmp(cls, "Police")) {
		return 1;
	}
	cls = ChType_ActClass(ChType_ActCtrl(t))->name;
	return !strcmp(cls, "Sentinel");
}

/* params: the desktop. On the ground the jump of the direction it walks (ActUpB/F/L/R) with the speed of its
 * walk; in the air from a jump the fall (ActMoreB/F, the view turned for a side), and a landing on a character
 * pays DesktopJumpPrice. */
void ChType_DesktopActionPress(cAbstractFile *params) {
	char name[0x80];
	vec3_t t1, t2, t3, v3, c, up;
	cEntity *target;
	const char *jump;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = ChType_CheckChar(name);
	const char *act = cActCtrl_GetActionName(ChType_ActCtrl(v), 0);
	if (!WChar_Falling(v) || WChar_SlideTime(v) > g_DesktopJumpSlideTime) {
		if (!strcmp(act, "Back")) {
			jump = "ActUpB";
		} else if (!strcmp(act, "StrLeft")) {
			jump = "ActUpL";
		} else if (strcmp(act, "StrRight")) {
			jump = "ActUpF";
		} else {
			jump = "ActUpR";
		}
		cActCtrl_Play(ChType_ActCtrl(v), jump, 0, 1.0f);
		if (!cActCtrl_IsActive(ChType_ActCtrl(v), jump)) {
			return;
		}
		cWalkCharEntity_SetFalling(v, 1);
		float ay = WChar_Ang(v)->y;
		up.x = 0.0f;
		up.y = g_DesktopJumpSpeed;
		up.z = 0.0f;
		/* the walk direction (the control at 0x4f8, 0x4fc) turns the jump, its length tilts it */
		double a = Game_Atan2((double)-WChar_ControlF4fc(v), (double)-WChar_ControlF4f8(v));
		float yaw = (float)a + ay;
		float len = (float)__builtin_sqrt((double)(WChar_ControlF4f8(v) * WChar_ControlF4f8(v) +
			WChar_ControlF4fc(v) * WChar_ControlF4fc(v)));
		const vec3_t *vel = Vec3_RotateXY(&t1, &up, len * 0.25f, yaw);
		Obj_Vtbl(v)->SetVel(v, vel);
		cActCtrl_Play(ChType_ActCtrl(v), jump, 0, 1.0f);
		return;
	}
	if (strncmp(act, "ActUp", 5)) {
		return;
	}
	float vx = Obj_Vtbl(v)->GetVel(v, &t1)->x;
	float speed = g_DesktopJumpSpeed;
	v3.z = Obj_Vtbl(v)->GetVel(v, &t2)->z;
	v3.x = vx;
	v3.y = speed;
	Obj_Vtbl(v)->SetVel(v, &v3);
	if (!strcmp(act, "ActUpB")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreB", 0, 1.0f);
	} else if (!strcmp(act, "ActUpF")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreF", 0, 1.0f);
	} else if (!strcmp(act, "ActUpL")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreF", 0, 1.0f);
		WChar_DisplAng(v) = -1.04719758f;
	} else if (!strcmp(act, "ActUpR")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreF", 0, 1.0f);
		WChar_DisplAng(v) = 1.04719758f;
	}
	const vec3_t *p = v->vftable->GetPos(v, &t3);
	c.x = p->x + 0.0f;
	c.y = p->y - 400.0f;
	c.z = p->z + 0.0f;
	target = NULL;
	float radius = Entity_Radius(v);
	p = v->vftable->GetPos(v, &t3);
	if (!cWorld_TestSphereEntities(g_pWorld, v, (void *)0x10, p, radius, &target, 1, &c) || !target ||
		!target->vftable->IsType(target, TYEN_WALKCHAR) || !ChType_ActCtrl(target)) {
		return;
	}
	g_pWorld->money += g_DesktopJumpPrice;
	SaveVar_Inc("Stats.Jump", 1);
	if (Desktop_BonusClass(target)) {
		g_pWorld->money += g_DesktopJumpPrice;
		SaveVar_Inc("Stats.Jump.Police", 1);
	}
}
