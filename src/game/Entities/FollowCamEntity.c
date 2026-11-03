/*
 * cFollowCamEntity ("FCam"), the camera behind an object or the free camera of a view, see
 * include/scrap/camcontrol.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/camcontrol.h"
#include "scrap/fxelements.h"
#include "scrap/input.h"
#include "scrap/scrapcollision.h"
#include "scrap/staticinit.h"

#define CFOLLOWCAMENTITY_VFTABLE (&cFollowCamEntity_vftable.base)
#define g_FreeCameraCollide (GVar_FreeCameraCollide.value.i)	/* FreeCameraCollide */
#define g_FullCameraTest (GVar_FullCameraTest.value.i)	/* FullCameraTest */
#define g_VehicleCamNear (GVar_VehicleCamNear.value.f)
#define g_VehicleCamFar (GVar_VehicleCamFar.value.f)
#define g_WalkCamNear (GVar_WalkCamNear.value.f)
#define g_WalkCamFar (GVar_WalkCamFar.value.f)
#define g_OutWalkCamNear (GVar_OutWalkCamNear.value.f)
#define g_OutWalkCamFar (GVar_OutWalkCamFar.value.f)
static const cCamControlVtbl cFollowCamEntity_vftable;
static cGVar GVar_WalkCamNear;
static cGVar GVar_WalkCamFar;
static cGVar GVar_VehicleCamNear;
static cGVar GVar_VehicleCamFar;
static cGVar GVar_OutWalkCamNear;
static cGVar GVar_OutWalkCamFar;
static cGVar GVar_FullCameraTest;
static cGVar GVar_FreeCameraCollide;
#define FCAM_SLOW_DT 0.06666667f		/* rva 0x3ad094: a frame slower than 15 per second */
#define FCAM_HALF_PI 1.5707964f
#define FCAM_NEAR_SCALE 1.65f			/* rva 0x3ad09c: the sphere radius of the camera, times clipNear */

static inline cFollowCamEntity *FCam(cEntity *e) {
	return (cFollowCamEntity *)e;
}

static inline cObjEntity *FCam_Obj(cEntity *e) {
	return (cObjEntity *)e;
}

/* the state of input action i of action set set for the player of the view */
static inline float FCam_Act(int set, int i, int slot) {
	return TABLE_AT(TABLE_AT(g_ActionSets, set)->actions, i)->value[slot];
}

cEntity *cFollowCamEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cFollowCamEntity *fc = mem;

	cCamEntity_ctor(mem, name, x, y, z);
	fc->base.base.vftable = CFOLLOWCAMENTITY_VFTABLE;
	cPy_Function_ctor(&fc->onSwitch);
	fc->target = NULL;
	fc->camDist = g_VehicleCamDist;
	fc->dist = fc->camDist;
	fc->camAng = -0.261799395f;
	fc->camRot = -0.0f;
	fc->base.cam.fov = 1.13446403f;
	fc->hSpeed = 5.0f;
	fc->hSpeedTo = 5.0f;
	fc->vSpeed = 5.0f;
	fc->vSpeedTo = 5.0f;
	fc->hSpeedRate = 2.0f;
	fc->vSpeedRate = 2.0f;
	fc->retSpeed = 1000.0f;
	fc->addTurn = 0.0f;
	fc->addTurnSpeed = 0.0f;
	fc->prevAng[1] = 0.0f;
	fc->prevAng[0] = 0.0f;
	fc->curAng[1] = 0.0f;
	fc->curAng[0] = 0.0f;
	fc->ang[1] = 0.0f;
	fc->ang[0] = 0.0f;
	fc->freeCam = 0;
	fc->mainTarget[0] = 0;
	fc->quakeTime = 0.0f;
	fc->quakeFactor = 0.0f;
	fc->slowFrames = 0;
	fc->tiltTo = 0.0f;
	fc->tilt = 0.0f;
	return mem;
}

/* ---- FollowCamEntity.c ---- */
static void cFollowCamEntity_dtor(cEntity *self) {
	cFollowCamEntity *fc = FCam(self);

	self->vftable = CFOLLOWCAMENTITY_VFTABLE;
	cPy_Function_Clear(&fc->onSwitch);
	cCamEntity_SetTarget(self, NULL);
	cPy_Function_dtor(&fc->onSwitch);
	cCamControl_dtor(self);
}

static cEntity *cFollowCamEntity_Delete(cEntity *self, uint32_t flags) {
	cFollowCamEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CFOLLOWCAMENTITY_SIZE);
	}
	return self;
}

/* the angles turn towards the ones of the target, slower for a far camera; at once for a near one or in slow
 * frames of a fast world */
static void cFollowCamEntity_Slot(cEntity *self, float dt) {
	cFollowCamEntity *fc = FCam(self);
	vec3_t tmp;

	if (!fc->target) {
		return;
	}
	const vec3_t *a = Obj_GetRenderAng(fc->target, &tmp);
	float ax = a->x;
	float ty = a->y;
	if (fc->addTurnSpeed != 0.0f) {
		fc->addTurn = fc->addTurnSpeed * dt + fc->addTurn;
	}
	float ay = fc->addTurn + ty;
	fc->prevAng[0] = fc->curAng[0];
	fc->prevAng[1] = fc->curAng[1];
	float k = 1001.0f / (fc->dist + 1.0f);
	Float_Approach(&fc->hSpeed, fc->hSpeedTo, fc->hSpeedRate * dt);
	Float_Approach(&fc->vSpeed, fc->vSpeedTo, fc->vSpeedRate * dt);
	if (fc->dist > 1.0f && (fc->slowFrames <= 0 || 0.25f >= cTimer_GetSpeed(&g_pWorld->timer))) {
		double e = (double)(float)((double)dt * 10.0);
		float fy = (float)(1.0 / pow((double)((fc->vSpeed - 1.0f) * k + 1.0f), e));
		float fx = (float)(1.0 / pow((double)((fc->hSpeed - 1.0f) * k + 1.0f), e));
		fc->curAng[0] = fc->curAng[0] + NormAng(ax - fc->ang[0]) * (1.0f - fy);
		fc->curAng[1] = fc->curAng[1] + NormAng(ay - fc->ang[1]) * (1.0f - fx);
		return;
	}
	fc->curAng[0] = ax;
	fc->curAng[1] = ay;
}

/* the camera between two slots: the angles lerped, the position behind the target at the camera distance, nearer
 * where the world or an entity is in the way */
static void cFollowCamEntity_Quake(cEntity *self, float dt);
static void cFollowCamEntity_Lerp(cEntity *self, float f) {
	cFollowCamEntity *fc = FCam(self);
	CGameCamera *cam = &fc->base.cam;
	vec3_t tmp, r, want, hit;
	float s1, c1, s2, c2;

	if (!fc->target || fc->base.link[0]) {
		return;
	}
	float dt = cTimer_GetDelta(&g_pWorld->timer);
	int n = fc->slowFrames;
	if (dt > FCAM_SLOW_DT) {
		if (n <= 0) {
			n = 1;
		}
	} else if (n > 1) {
		n--;
	} else {
		n = 0;
	}
	fc->slowFrames = n;
	float step = fc->retSpeed * dt;
	byte walk = Entity_IsType(fc->target, TYEN_WALKCHAR);
	fc->center = *Obj_GetRenderPos(fc->target, &tmp);
	float u = 1.0f - f;
	float ax = fc->curAng[0] * u + fc->prevAng[0] * f;
	float ay = fc->curAng[1] * u + fc->prevAng[1] * f;
	fc->ang[0] = ax;
	fc->ang[1] = ay;
	cam->rot.x = fc->camRot - ax;
	cam->rot.y = ay + M_PI_F;
	float tilt = 0.0f;
	if (walk && g_WalkCameraCenter != 0.0f) {
		Float_Approach(&fc->tilt, fc->tiltTo, (float)__builtin_fabs((double)(fc->tilt - fc->tiltTo)) * dt);
		tilt = (fc->tilt * M_PI_F) / 6.0f;
	}
	vec3_t off = FCam_Obj(fc->target)->camOff;
	const vec3_t *o = Vec3_RotateXY(&r, &off, fc->ang[0], fc->ang[1]);
	fc->center.x = o->x + fc->center.x;
	fc->center.y = o->y + fc->center.y;
	fc->center.z = o->z + fc->center.z;
	Float_SinCos((fc->camAng + fc->ang[0]) - FCAM_HALF_PI, &s1, &c1);
	Float_SinCos(fc->ang[1] - tilt, &s2, &c2);
	float dx = -(s2 * s1);
	float dz = -(c2 * s1);
	float dy = -c1;
	if (fc->camDist - step > fc->dist) {
		fc->dist = fc->dist + step;
	} else {
		fc->dist = fc->camDist;
	}
	float d = fc->dist;
	want.y = fc->center.y + d * dy;
	want.z = fc->center.z + d * dz;
	want.x = d * dx + fc->center.x;
	CollisionEpsilon_Set(0);
	if ((!g_FullCameraTest && fc->dist == fc->camDist && !Engine_TestLine(&fc->center, &want, NULL, NULL) &&
		    !Engine_TestSphere(&want, &want, cam->zNear * FCAM_NEAR_SCALE, NULL, NULL)) ||
		!Engine_TestSphere(&fc->center, &want, cam->zNear * FCAM_NEAR_SCALE, &hit, NULL)) {
		cam->pos = want;
	} else {
		cam->pos = hit;
		float ex = fc->center.x - hit.x;
		float ey = fc->center.y - hit.y;
		float ez = fc->center.z - hit.z;
		fc->dist = (float)__builtin_sqrt((double)(ey * ey + ex * ex + ez * ez));
	}
	CollisionEpsilon_Set(1);
	if (walk && cWorld_TestLineEntities(g_pWorld, fc->target, 0x11, &fc->center, &cam->pos, &hit, NULL,
		1)) {
		float ex = hit.x - fc->center.x;
		float ey = hit.y - fc->center.y;
		float ez = hit.z - fc->center.z;
		fc->dist = (float)__builtin_sqrt((double)(ey * ey + ex * ex + ez * ez));
		d = fc->dist;
		cam->pos.y = fc->center.y + d * dy;
		cam->pos.x = d * dx + fc->center.x;
		cam->pos.z = fc->center.z + d * dz;
	}
	cam->rot.z = 0.0f;
	cFollowCamEntity_Quake(self, dt);
}

static byte cFollowCamEntity_VarF(cEntity *self, const char *name, double *d, int set) {
	cFollowCamEntity *fc = FCam(self);
	float *deg = NULL;

	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "QuakeFactor")) {
		deg = &fc->quakeFactor;
	} else if (!strcmp(name, "QuakeTime")) {
		if ((byte)set) {
			fc->quakeTime = (float)*d;
		} else {
			*d = fc->quakeTime;
		}
		return 1;
	} else if (!strcmp(name, "HSpeed")) {
		if ((byte)set) {
			fc->hSpeed = (float)*d;
		} else {
			*d = fc->hSpeed;
		}
		return 1;
	} else if (!strcmp(name, "VSpeed")) {
		if ((byte)set) {
			fc->vSpeed = (float)*d;
		} else {
			*d = fc->vSpeed;
		}
		return 1;
	} else if (!strcmp(name, "RetSpd")) {
		if ((byte)set) {
			fc->retSpeed = (float)*d;
		} else {
			*d = fc->retSpeed;
		}
		return 1;
	} else if (!strcmp(name, "CamDist")) {
		if ((byte)set) {
			float v = (float)*d;
			fc->camDist = v;
			fc->dist = v;
		} else {
			*d = fc->camDist;
		}
		return 1;
	} else if (!strcmp(name, "CamAng")) {
		deg = &fc->camAng;
	} else if (!strcmp(name, "CamRot")) {
		deg = &fc->camRot;
	} else if (!strcmp(name, "AddTurn")) {
		deg = &fc->addTurn;
	} else if (!strcmp(name, "AddTurnSpeed")) {
		deg = &fc->addTurnSpeed;
	} else {
		return cCamControl_VarF(self, name, d, set);
	}
	if ((byte)set) {
		*deg = Cam_Deg2Rad(*d);
	} else {
		*d = Rad2Deg(*deg);
	}
	return 1;
}

void cCamEntity_SetAng(cEntity *cam, vec3_t ang) {
	cFollowCamEntity *fc = FCam(cam);

	if (fc->freeCam) {
		return;
	}
	fc->prevAng[0] = ang.x;
	fc->prevAng[1] = ang.y;
	fc->curAng[0] = ang.x;
	fc->curAng[1] = ang.y;
	fc->ang[0] = ang.x;
	fc->ang[1] = ang.y;
	CamCtl_Slot(cam, 0.05f);
	CamCtl_Lerp(cam, 1.0f);
}

/* the camera moved by the input actions of its view slot (sets 1 and 3) */
static void cFollowCamEntity_FreeMove(cEntity *self) {
	cCamControl *c = (cCamControl *)self;
	vec3_t move, r, to, out;

	if (c->link[0] || c->slot < 0) {
		return;
	}
	float dt = cTimer_GetDelta(&g_pWorld->timer);
	byte strafe = FCam_Act(1, 18, c->slot) > 0.5f || FCam_Act(3, 15, c->slot) > 0.5f;
	byte fast = FCam_Act(1, 9, c->slot) > 0.5f || FCam_Act(3, 17, c->slot) > 0.5f;
	byte slow = FCam_Act(1, 17, c->slot) > 0.5f || FCam_Act(3, 19, c->slot) > 0.5f;
	float v = (FCam_Act(1, 3, c->slot) - FCam_Act(1, 2, c->slot)) * dt;
	c->cam.rot.x = v + v + c->cam.rot.x;
	v = (FCam_Act(3, 3, c->slot) - FCam_Act(3, 2, c->slot)) * dt;
	c->cam.rot.x = v + v + c->cam.rot.x;
	if (!strafe) {
		v = (FCam_Act(1, 5, c->slot) - FCam_Act(1, 4, c->slot)) * dt;
		c->cam.rot.y = v + v + c->cam.rot.y;
		v = (FCam_Act(3, 5, c->slot) - FCam_Act(3, 4, c->slot)) * dt;
		c->cam.rot.y = v + v + c->cam.rot.y;
	}
	if (c->cam.rot.x > FCAM_HALF_PI) {
		c->cam.rot.x = FCAM_HALF_PI;
	}
	if (-FCAM_HALF_PI > c->cam.rot.x) {
		c->cam.rot.x = -FCAM_HALF_PI;
	}
	move.y = 0.0f;
	move.z = (FCam_Act(3, 6, c->slot) - FCam_Act(3, 7, c->slot)) + (FCam_Act(1, 6, c->slot) - FCam_Act(1, 7, c->slot));
	move.x = (FCam_Act(3, 9, c->slot) - FCam_Act(3, 8, c->slot)) +
		(FCam_Act(1, 11, c->slot) - FCam_Act(1, 10, c->slot));
	if (strafe) {
		move.x = (FCam_Act(3, 5, c->slot) - FCam_Act(3, 4, c->slot)) +
			(FCam_Act(1, 5, c->slot) - FCam_Act(1, 4, c->slot));
	}
	float setSpeed = Input_GetActionSet() == 1 ? 1.0f : 0.25f;
	float speed = slow ? 2100.0f : (fast ? 16800.0f : 8400.0f);
	const vec3_t *m = Vec3_Rotate(&r, &move, &c->cam.rot);
	to.x = m->x * dt * speed * setSpeed + c->cam.pos.x;
	to.y = c->cam.pos.y + m->y * dt * speed * setSpeed;
	to.z = c->cam.pos.z + m->z * dt * speed * setSpeed;
	if (!g_FreeCameraCollide) {
		out = to;
	} else {
		if (to.y > g_TopLimit) {
			to.y = g_TopLimit;
		}
		Collision_Slide(&c->cam.pos, &to, 500.0f, &out, NULL);
	}
	c->cam.pos = out;
}

cEntity *cCamEntity_GetTarget(cEntity *cam) {
	return FCam(cam)->target;
}

static const char *cFollowCamEntity_GetClassName(cEntity *self) {
	(void)self;
	return FCAM_CLASSNAME;
}

static void cFollowCamEntity_UpdateOff(cEntity *self, float dt) {
	cFollowCamEntity_Quake(self, dt);
}

static void cFollowCamEntity_SetFreeCam(cEntity *self, byte on);
static byte cFollowCamEntity_VarI(cEntity *self, const char *name, int *i, int set) {
	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "FreeCam")) {
		if ((byte)set) {
			cFollowCamEntity_SetFreeCam(self, *i != 0);
		} else {
			*i = FCam(self)->freeCam != 0;
		}
		return 1;
	}
	return cCamControl_VarI(self, name, i, set);
}

static byte cFollowCamEntity_IsType(cEntity *self, const char *type) {
	if (!strcmp(type, TYEN_FCAM)) {
		return 1;
	}
	return cCamControl_IsType(self, type);
}

static void cFollowCamEntity_ResetAngles(cEntity *self);
static byte cFollowCamEntity_SetTargetName(cEntity *self, const char *name) {
	byte r = cCamControl_SetTarget(self, name);

	if (!r && FCam(self)->target) {
		cFollowCamEntity_ResetAngles(self);
	}
	return r;
}

/* the random shake of the angles while QuakeTime runs */
static void cFollowCamEntity_Quake(cEntity *self, float dt) {
	cFollowCamEntity *fc = FCam(self);
	vec3_t *rot = &fc->base.cam.rot;

	if (fc->quakeTime > 0.0f && fc->quakeFactor > 0.0f) {
		float q = fc->quakeFactor;
		float nq = -q;
		rot->z = rot->z + FxElem_Rand((double)(q - nq), (double)nq);
		q = fc->quakeFactor;
		nq = -q;
		rot->x = rot->x + FxElem_Rand((double)(q - nq), (double)nq);
		q = fc->quakeFactor;
		nq = -q;
		rot->y = rot->y + FxElem_Rand((double)(q - nq), (double)nq);
		fc->quakeTime = fc->quakeTime - dt;
	}
}

/* the three angle pairs from the target (y plus AddTurn) */
static void cFollowCamEntity_ResetAngles(cEntity *self) {
	cFollowCamEntity *fc = FCam(self);
	vec3_t tmp;

	if (!fc->target) {
		return;
	}
	float x = Obj_GetRenderAng(fc->target, &tmp)->x;
	fc->ang[0] = x;
	fc->curAng[0] = x;
	fc->prevAng[0] = x;
	float y = fc->addTurn + Obj_GetRenderAng(fc->target, &tmp)->y;
	fc->ang[1] = y;
	fc->curAng[1] = y;
	fc->prevAng[1] = y;
}

/* on: no target; off: the MainTarget "Obj" again (free when there is none) */
static void cFollowCamEntity_SetFreeCam(cEntity *self, byte on) {
	cFollowCamEntity *fc = FCam(self);

	if (!on && fc->mainTarget[0]) {
		cEntity *e = World_FindEntity(fc->mainTarget);
		if (e && Entity_IsType(e, TYEN_OBJECT)) {
			cCamEntity_SetTarget(self, e);
			fc->freeCam = 0;
			return;
		}
	}
	cCamEntity_SetTarget(self, NULL);
	fc->freeCam = 1;
}

/* the clip distances for a vehicle (also the free camera of the vehicle input) or a character */
static void FCam_SetClip(cFollowCamEntity *fc, byte vehicle) {
	CGameCamera *cam = &fc->base.cam;

	if (vehicle) {
		cam->zNear = g_VehicleCamNear;
		cam->zFar = g_VehicleCamFar;
	} else if (g_IsMapOutDoor) {
		cam->zNear = g_OutWalkCamNear;
		cam->zFar = g_OutWalkCamFar;
	} else {
		cam->zNear = g_WalkCamNear;
		cam->zFar = g_WalkCamFar;
	}
}

void cCamEntity_SetTarget(cEntity *cam, cEntity *target) {
	cFollowCamEntity *fc = FCam(cam);
	vec3_t tmp;

	if (target == fc->target) {
		return;
	}
	if (fc->target) {
		FCam_Obj(fc->target)->cam = NULL;
	}
	fc->target = target;
	if (target) {
		const vec3_t *p = Entity_GetPos(target, &tmp);
		cCamControl_SetViewPos(cam, 0, p->x, p->y, p->z);
		FCam_Obj(fc->target)->cam = cam;
		FCam_SetClip(fc, Entity_IsType(fc->target, TYEN_CAR) != 0);
		cFollowCamEntity_ResetAngles(cam);
	} else {
		FCam_SetClip(fc, Input_GetActionSet() == 1);
	}
	cPy_Function_AddParamS(&fc->onSwitch, cam->name);
	cPy_Function_AddParamI(&fc->onSwitch, fc->base.slot);
	cPy_Function_Execute(&fc->onSwitch, 0);
}

/* the free camera: the button of the view picks the next active vehicle of an AI or of the network, or the next
 * active character, as the target; without one it flies */
static void cFollowCamEntity_Update(cEntity *self) {
	cFollowCamEntity *fc = FCam(self);
	int slot;

	if (!fc->freeCam) {
		return;
	}
	slot = fc->base.slot;
	if (slot >= 0 && (cInputAction_Pressed(TABLE_AT(TABLE_AT(g_ActionSets, 1)->actions, 16), fc->base.slot) ||
		(fc->base.slot >= 0 &&
			cInputAction_Pressed(TABLE_AT(TABLE_AT(g_ActionSets, 3)->actions, 18), fc->base.slot)))) {
		cEntity *e = fc->target ? cEntity_Next(fc->target) : cWorld_GetFirstEntity(g_pWorld);
		while (e) {
			if (e->active) {
				if (Entity_IsType(e, TYEN_CAR) && (Net_IsNetEntity(e) || Entity_GetAI(e))) {
					break;
				}
				if (e->active && Entity_IsType(e, TYEN_WALKCHAR)) {
					break;
				}
			}
			e = cEntity_Next(e);
		}
		cCamEntity_SetTarget(self, e);
	}
	if (!fc->target) {
		cFollowCamEntity_FreeMove(self);
	}
}

static byte cFollowCamEntity_VarS(cEntity *self, const char *name, char **s, int set) {
	cFollowCamEntity *fc = FCam(self);

	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "OnSwitch")) {
		if ((byte)set) {
			cPy_Function_SetFunc(&fc->onSwitch, *s);
		} else {
			*s = FCAM_ONSWITCH_DESC;
		}
		return 1;
	}
	if (!strcmp(name, "MainTarget")) {
		if ((byte)set) {
			Str_Copy(fc->mainTarget, *s, 0x80);
		} else {
			*s = fc->mainTarget;
		}
		return 1;
	}
	if (!strcmp(name, "Target")) {
		if ((byte)set) {
			cEntity *e = World_FindEntity(*s);
			cCamEntity_SetTarget(self, e && Entity_IsType(e, TYEN_OBJECT) ? e : NULL);
		} else {
			*s = fc->target ? fc->target->name : str_Empty;
		}
		return 1;
	}
	return cCamControl_VarS(self, name, s, set);
}

static byte cFollowCamEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set) {
	if (!*name) {
		return 0;
	}
	return cCamControl_VarV(self, name, x, y, z, set);
}

/* the vtable of cFollowCamEntity */
static const cCamControlVtbl cFollowCamEntity_vftable = {
	.base.Update = cFollowCamEntity_Update,
	.base.Render = cEntity_Render,
	.base.UpdateOff = cEntity_UpdateOff,
	.base.Delete = cFollowCamEntity_Delete,
	.base.GetClassName = cFollowCamEntity_GetClassName,
	.base.IsType = cFollowCamEntity_IsType,
	.base.GetPos = cEntity_GetPos,
	.base.SetPos = cEntity_SetPos,
	.base.SetActive = cEntity_SetActive,
	.base.TestLine = cEntity_TestLine,
	.base.TestSphere = cEntity_TestSphereFalse,
	.base.Touch = cEntity_TouchNop,
	.base.SetModel = cEntity_SetModel,
	.base.GetTargetAngle = cEntity_GetTargetAngle,
	.base.AimWeapon = cEntity_AimWeapon,
	.base.SetEmbeddedFX = cEntity_SetEmbeddedFX,
	.base.VarI = cFollowCamEntity_VarI,
	.base.VarF = cFollowCamEntity_VarF,
	.base.VarS = cFollowCamEntity_VarS,
	.base.VarV = cFollowCamEntity_VarV,
	.base.Unused20 = cEntity_Unused20,
	.base.Unused21 = cEntity_Unused21,
	.base.GetAI = cEntity_GetAI,
	.GetCamPos = cCamControl_GetCamPos,
	.SetCamPos = cCamControl_SetCamPos,
	.Lerp = cFollowCamEntity_Lerp,
	.Slot = cFollowCamEntity_Slot,
	.SetTarget = cFollowCamEntity_SetTargetName,
	.UpdateOff = cFollowCamEntity_UpdateOff,
};

static cGVar GVar_FullCameraTest;
static cGVar GVar_VehicleCamFar;
/* engine variables */
static cGVar GVar_FreeCameraCollide;
cGVar GVar_VehicleCamDist;
static cGVar GVar_VehicleCamNear;
static cGVar GVar_WalkCamFar;
static cGVar GVar_OutWalkCamFar;
static cGVar GVar_OutWalkCamNear;
static cGVar GVar_WalkCamNear;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_VehicleCamDist, &GVar_VehicleCamDist, "VehicleCamDist", 2, "Default camera distance for vehicles in cm", 1000.0f)
INIT_GVAR_INT(Init_GVar_FreeCameraCollide, &GVar_FreeCameraCollide, "FreeCameraCollide", 0, "If the camera can collide with the world.", 1)
INIT_GVAR_FLOAT(Init_GVar_VehicleCamNear, &GVar_VehicleCamNear, "VehicleCamNear", 0, "default clip near distance for vehicles", 50.0f)
INIT_GVAR_FLOAT(Init_GVar_VehicleCamFar, &GVar_VehicleCamFar, "VehicleCamFar", 0, "default clip far  distance for vehicles", 500000.0f)
INIT_GVAR_FLOAT(Init_GVar_WalkCamNear, &GVar_WalkCamNear, "WalkCamNear", 0, "default clip near distance for characters (indoor)", 10.0f)
INIT_GVAR_FLOAT(Init_GVar_WalkCamFar, &GVar_WalkCamFar, "WalkCamFar", 0, "default clip far  distance for characters (indoor)", 100000.0f)
INIT_GVAR_FLOAT(Init_GVar_OutWalkCamNear, &GVar_OutWalkCamNear, "OutWalkCamNear", 0, "default clip near distance for characters (outdoor)", 20.0f)
INIT_GVAR_FLOAT(Init_GVar_OutWalkCamFar, &GVar_OutWalkCamFar, "OutWalkCamFar", 0, "default clip far  distance for characters (outdoor)", 400000.0f)
INIT_GVAR_INT(Init_GVar_FullCameraTest, &GVar_FullCameraTest, "FullCameraTest", 0, "Activates/deactivates de camera test.", 0)
