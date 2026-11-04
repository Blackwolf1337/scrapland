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
#include "scrap/staticinit.h"

#define g_DesktopJumpPrice (GVar_DesktopJumpPrice.value.i)
#define g_DesktopJumpSlideTime (GVar_DesktopJumpSlideTime.value.f)
#define g_DesktopJumpSpeed (GVar_DesktopJumpSpeed.value.f)
#define g_FXDesktopFallEndDist (GVar_FXDesktopFallEndDist.value.f)
#define g_FXDesktopFallEndFile (GVar_FXDesktopFallEndFile.value.s)
static cGVar GVar_FXDesktopFallEndFile;
static cGVar GVar_FXDesktopFallEndDist;
static cGVar GVar_DesktopJumpSpeed;
static cGVar GVar_DesktopJumpSlideTime;
static cGVar GVar_DesktopJumpPrice;
#define DESKTOP_TWO_PI 6.2831854820251465	/* (double)6.2831855f */

/* params: the desktop. A fast landing seen from near enough: four puffs around the point it lands on and a ring of
 * dust thrown out of it. */
static uint32_t ChType_FXDesktopFallEndRamps[18] = {
	0xff000000, 0x80303030, 0x60304040, 0x60404040, 0x60404040, 0x80404040, 0xbe303030, 0xff000000, 0xff000000,
	0xff000000, 0x80404040, 0x60606060, 0x60606060, 0x60606060, 0x80606060, 0xbe404040, 0xff000000, 0xff000000,
};

static uint32_t ChType_FXDesktopFallEndTable[10] = { 0xff565656, 0xb4fff06e, 0x8cefa01e, 0x969a500e, 0xc3604b1e, 0xd2404040, 0xeb202020, 0xff000000, 0xff000000 };

static void ChType_FXDesktopFallEnd(cAbstractFile *params) {
	char name[0x80];
	vec3_t tmp, one, rot, d, xaxis;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = ChType_CheckChar(name);
	if (WChar(v)->wl.obj.cam) {
		cPad_Rumble(&g_Pads[0], 0.5f, 0.0f, 0.125f);
	}
	const vec3_t *c = Obj_GetViewPos(v, &tmp);
	if (!FX_IsVisible(c->x, c->y, c->z, 250.0f, g_FXDesktopFallEndDist, NULL, 0)) {
		return;
	}
	cWorld *w = g_pWorld;
	vec3_t cp = w->camPos[w->curCam];
	c = Obj_GetViewPos(v, &tmp);
	float dx = cp.x - c->x;
	float dy = cp.y - c->y;
	float dz = cp.z - c->z;
	if (dy * dy + dx * dx + dz * dz > g_FXDesktopFallEndDist * g_FXDesktopFallEndDist) {
		return;
	}
	vec3_t fall = WChar(v)->fallVel;
	if (fall.y > -g_FallSoundVel) {
		return;
	}
	float r = Obj_GetRadius(v);
	vec3_t p = *Entity_GetPos(v, &tmp);
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
	CGameModel_SetPos(Debris_Model(dbr), Obj_GetViewPos(v, &tmp));
	dbr->life = 1.0f;
	cDebrisSys_Drop(sys, dbr);
	cObjElementFXParticleSystem *el = Debris_Element(dbr);
	if (!el) {
		return;
	}
	if (!el->on) {
		el->move = 0;
		el->on = 1;
	}
	CGameParticles *pt = &el->particles;
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
		CGameParticles_SetColorRamp(pt, ChType_FXDesktopFallEndTable);
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
		CGameParticles_SetColorRamp(pt, ChType_FXDesktopFallEndRamps + 9 * ramp);
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
static void ChType_DesktopActionPress(cAbstractFile *params) {
	char name[0x80];
	vec3_t t1, t2, t3, v3, c, up;
	cEntity *target;
	const char *jump;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = ChType_CheckChar(name);
	const char *act = cActCtrl_GetActionName(ChType_ActCtrl(v), 0);
	if (!WChar(v)->falling || WChar(v)->slideTime > g_DesktopJumpSlideTime) {
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
		float ay = WChar(v)->view.y;
		up.x = 0.0f;
		up.y = g_DesktopJumpSpeed;
		up.z = 0.0f;
		/* the walk direction (the control at 0x4f8, 0x4fc) turns the jump, its length tilts it */
		double a = atan2((double)-WChar(v)->control.strafe, (double)-WChar(v)->control.walk);
		float yaw = (float)a + ay;
		float len = (float)__builtin_sqrt((double)(WChar(v)->control.walk * WChar(v)->control.walk +
			WChar(v)->control.strafe * WChar(v)->control.strafe));
		const vec3_t *vel = Vec3_RotateXY(&t1, &up, len * 0.25f, yaw);
		Obj_SetVel(v, vel);
		cActCtrl_Play(ChType_ActCtrl(v), jump, 0, 1.0f);
		return;
	}
	if (strncmp(act, "ActUp", 5)) {
		return;
	}
	float vx = Obj_GetVel(v, &t1)->x;
	float speed = g_DesktopJumpSpeed;
	v3.z = Obj_GetVel(v, &t2)->z;
	v3.x = vx;
	v3.y = speed;
	Obj_SetVel(v, &v3);
	if (!strcmp(act, "ActUpB")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreB", 0, 1.0f);
	} else if (!strcmp(act, "ActUpF")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreF", 0, 1.0f);
	} else if (!strcmp(act, "ActUpL")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreF", 0, 1.0f);
		WChar(v)->displAng = -1.04719758f;
	} else if (!strcmp(act, "ActUpR")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActMoreF", 0, 1.0f);
		WChar(v)->displAng = 1.04719758f;
	}
	const vec3_t *p = Entity_GetPos(v, &t3);
	c.x = p->x + 0.0f;
	c.y = p->y - 400.0f;
	c.z = p->z + 0.0f;
	target = NULL;
	float radius = WLife(v)->prim.radius;
	p = Entity_GetPos(v, &t3);
	if (!cWorld_TestSphereEntities(g_pWorld, v, 0x10, p, radius, &target, 1, &c) || !target ||
		!Entity_IsType(target, TYEN_WALKCHAR) || !ChType_ActCtrl(target)) {
		return;
	}
	g_pWorld->money += g_DesktopJumpPrice;
	SaveVar_Inc("Stats.Jump", 1);
	if (Desktop_BonusClass(target)) {
		g_pWorld->money += g_DesktopJumpPrice;
		SaveVar_Inc("Stats.Jump.Police", 1);
	}
}

static cGVar GVar_DesktopJumpPrice;
static cGVar GVar_DesktopJumpSlideTime;
static cGVar GVar_FXDesktopFallEndDist;
static cGVar GVar_DesktopJumpSpeed;
static cGVar GVar_FXDesktopFallEndFile;
cGVar GVar_FallSoundVel;
static cPy_NativeFunc NativeFunc_DesktopActionPress;
static cPy_NativeFunc NativeFunc_FXDesktopFallEnd;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_DesktopJumpSpeed, &GVar_DesktopJumpSpeed, "DesktopJumpSpeed", 0, "Jump speed in cm/sec", 1000.0f)
INIT_GVAR_INT(Init_GVar_DesktopJumpPrice, &GVar_DesktopJumpPrice, "DesktopJumpPrice", 0, "if desktop jump over any character, the price of money is...", 5)
INIT_GVAR_FLOAT(Init_GVar_DesktopJumpSlideTime, &GVar_DesktopJumpSlideTime, "DesktopJumpSlideTime", 0, "Sliding time to jump again.", 0.5f)
FINI_GVAR_STRING(Fini_GVar_FXDesktopFallEndFile, &GVar_FXDesktopFallEndFile)
INIT_GVAR_STRING(Init_GVar_FXDesktopFallEndFile, Fini_GVar_FXDesktopFallEndFile, &GVar_FXDesktopFallEndFile, "FXDesktopFallEndFile", 0, "Default Model for Desktop Fall End Effect", "Models/GFX/Desktop/FallEnd.M3D")
INIT_GVAR_FLOAT(Init_GVar_FXDesktopFallEndDist, &GVar_FXDesktopFallEndDist, "FXDesktopFallEndDist", 0, "Max Distance to show Desktop Fall End Effect", 10000.0f)
INIT_GVAR_FLOAT(Init_GVar_FallSoundVel, &GVar_FallSoundVel, "FallSoundVel", 0, "Beyond this fallspeed, the fallsound will run...", 600.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_DesktopActionPress, &NativeFunc_DesktopActionPress, "DesktopActionPress", ChType_DesktopActionPress)
INIT_NATIVE_FUNC(Init_NativeFunc_FXDesktopFallEnd, &NativeFunc_FXDesktopFallEnd, "FXDesktopFallEnd", ChType_FXDesktopFallEnd)
