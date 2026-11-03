/*
 * cCamControl ("Cam"), the plain camera entity, see include/scrap/camcontrol.h.
 */
#include <string.h>

#include "scrap/camcontrol.h"

#define CCAMCONTROL_VFTABLE (&cCamControl_vftable.base)
static const cCamControlVtbl cCamControl_vftable;

static inline cCamControl *Cam(cEntity *e) {
	return (cCamControl *)e;
}

cEntity *cCamEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cCamControl *c = mem;

	cEntity_ctor(mem, name, x, y, z);
	c->base.vftable = CCAMCONTROL_VFTABLE;
	CGameCamera_ctor(&c->cam);
	c->cam.zNear = 100.0f;
	c->cam.zFar = 400000.0f;
	c->cam.pos.x = x;
	c->cam.pos.y = y;
	c->cam.pos.z = z;
	c->slot = -1;
	c->link[0] = 0;
	return mem;
}

/* the view it showed goes back to no camera */
void cCamControl_dtor(cEntity *self) {
	cCamControl *c = Cam(self);

	self->vftable = CCAMCONTROL_VFTABLE;
	if (c->slot >= 0 && !strcmp(self->name, cWorld_GetCamName(g_pWorld, c->slot))) {
		cWorld_SetCam(g_pWorld, c->slot, str_Empty);
	}
	CGameCamera_dtor(&c->cam);
	cEntity_dtor(self);
}

static cEntity *cCamControl_Delete(cEntity *self, uint32_t flags) {
	cCamControl_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CCAMCONTROL_SIZE);
	}
	return self;
}

void cCamControl_NopF(cEntity *self, float f) {
	(void)self;
	(void)f;
}

byte cCamControl_VarF(cEntity *self, const char *name, double *d, int set) {
	CGameCamera *cam = &Cam(self)->cam;

	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "Fov")) {
		if ((byte)set) {
			cam->fov = Cam_Deg2Rad(*d);
		} else {
			*d = Rad2Deg(cam->fov);
		}
		return 1;
	}
	if (!strcmp(name, "clipNear")) {
		if ((byte)set) {
			cam->zNear = (float)*d;
		} else {
			*d = cam->zNear;
		}
		return 1;
	}
	if (!strcmp(name, "clipFar")) {
		if ((byte)set) {
			cam->zFar = (float)*d;
		} else {
			*d = cam->zFar;
		}
		return 1;
	}
	return cEntity_VarF(self, name, d, set);
}

void cCamControl_GetCamPos(cEntity *self, float *x, float *y, float *z) {
	const CGameCamera *cam = &Cam(self)->cam;

	*x = cam->pos.x;
	*y = cam->pos.y;
	*z = cam->pos.z;
}

static const char *cCamControl_GetClassName(cEntity *self) {
	(void)self;
	return CAM_CLASSNAME;
}

byte cCamControl_VarI(cEntity *self, const char *name, int *i, int set) {
	if (!*name) {
		return 0;
	}
	if (!(byte)set && !strcmp(name, "UsrControl")) {
		*i = Cam(self)->slot;
		return 1;
	}
	return cEntity_VarI(self, name, i, set);
}

byte cCamControl_IsType(cEntity *self, const char *type) {
	if (!strcmp(type, TYEN_CAM)) {
		return 1;
	}
	return cEntity_IsType(self, type);
}

byte cCamControl_SetTarget(cEntity *self, const char *name) {
	cCamControl *c = Cam(self);

	if (name && *name) {
		cEntity *e = World_FindEntity(name);
		if (e && Entity_IsType(e, TYEN_OBJECT) && Obj_GetModel(e)) {
			CGameCamera_SetNodeModel(&c->cam, Obj_GetModel(e), -1);
			Str_Copy(c->link, name, 0x80);
			return 1;
		}
	}
	c->link[0] = 0;
	CGameCamera_SetNodeModel(&c->cam, NULL, -1);
	CGameCamera_SetLookAt(&c->cam, 0);
	return 0;
}

void cCamControl_SetCamPos(cEntity *self, float x, float y, float z) {
	CGameCamera *cam = &Cam(self)->cam;

	cam->pos.x = x;
	cam->pos.y = y;
	cam->pos.z = z;
}

void cCamControl_SetViewPos(cEntity *self, int i, float x, float y, float z) {
	if (!strcmp(cWorld_GetCamName(g_pWorld, i), self->name)) {
		g_pWorld->camPos[i].x = x;
		g_pWorld->camPos[i].y = y;
		g_pWorld->camPos[i].z = z;
	}
}

byte cCamControl_VarS(cEntity *self, const char *name, char **s, int set) {
	if (!strcmp(name, "EntityLink")) {
		if ((byte)set) {
			CamCtl_SetTarget(self, *s);
		} else {
			*s = Cam(self)->link;
		}
		return 1;
	}
	return cEntity_VarS(self, name, s, set);
}

byte cCamControl_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set) {
	CGameCamera *cam = &Cam(self)->cam;

	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "camPos")) {
		if ((byte)set) {
			float fx = (float)*x;
			float fy = (float)*y;
			float fz = (float)*z;
			cam->pos.x = fx;
			cam->pos.y = fy;
			cam->pos.z = fz;
		} else {
			float fx = cam->pos.x;
			float fy = cam->pos.y;
			float fz = cam->pos.z;
			*x = fx;
			*y = fy;
			*z = fz;
		}
		return 1;
	}
	if (!strcmp(name, "camAng")) {
		if ((byte)set) {
			float fx = Cam_Deg2Rad(*x);
			float fy = Cam_Deg2Rad(*y);
			float fz = Cam_Deg2Rad(*z);
			cam->rot.x = fx;
			cam->rot.y = fy;
			cam->rot.z = fz;
		} else {
			float fx = cam->rot.x;
			float fy = cam->rot.y;
			float fz = cam->rot.z;
			*x = Rad2Deg(fx);
			*y = Rad2Deg(fy);
			*z = Rad2Deg(fz);
		}
		return 1;
	}
	return cEntity_VarV(self, name, x, y, z, set);
}

/* the vtable of cCamControl */
static const cCamControlVtbl cCamControl_vftable = {
	.base.Update = cEntity_Update,
	.base.Render = cEntity_Render,
	.base.UpdateOff = cEntity_UpdateOff,
	.base.Delete = cCamControl_Delete,
	.base.GetClassName = cCamControl_GetClassName,
	.base.IsType = cCamControl_IsType,
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
	.base.VarI = cCamControl_VarI,
	.base.VarF = cCamControl_VarF,
	.base.VarS = cCamControl_VarS,
	.base.VarV = cCamControl_VarV,
	.base.Vf20 = cEntity_Vf20,
	.base.Vf21 = cEntity_Vf21,
	.base.GetAI = cEntity_GetAI,
	.GetCamPos = cCamControl_GetCamPos,
	.SetCamPos = cCamControl_SetCamPos,
	.Lerp = cCamControl_NopF,
	.Slot = cCamControl_NopF,
	.SetTarget = cCamControl_SetTarget,
	.UpdateOff = cCamControl_NopF,
};
