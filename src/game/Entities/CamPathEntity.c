/*
 * cCamPathEntity ("CamPath"), the camera that moves along its points, see include/scrap/camcontrol.h.
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/camcontrol.h"
#include "scrap/fxelements.h"

#define CCAMPATHENTITY_VFTABLE (&cCamPathEntity_vftable.base)
static const cCamControlVtbl cCamPathEntity_vftable;

static inline cCamPathEntity *Path(cEntity *e) {
	return (cCamPathEntity *)e;
}

static cCamPathPoint *cCamPathPoint_ctor(cCamPathPoint *self) {
	self->pos.x = 0.0f;
	self->pos.y = 0.0f;
	self->pos.z = 0.0f;
	self->ang.x = 0.0f;
	self->ang.y = 0.0f;
	self->ang.z = 0.0f;
	self->zNear = 10.0f;
	self->zFar = 40000.0f;
	self->fov = 1.5707964f;
	self->quake = 0.0f;
	self->cam[0] = 0;
	self->time = 1.0f;
	self->fadeIn = 1;
	self->fadeOut = 1;
	return self;
}

cEntity *cCamPathEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cCamPathEntity *p = mem;

	cCamEntity_ctor(mem, name, x, y, z);
	p->base.base.vftable = CCAMPATHENTITY_VFTABLE;
	cPy_Function_ctor(&p->onEnd);
	for (int i = 0; i < CAMPATH_POINTS; i++) {
		cCamPathPoint_ctor(&p->points[i]);
	}
	p->time = 0.0f;
	p->numPoints = 0;
	return mem;
}

/* ---- CamPathEntity.c ---- */
static void cCamPathEntity_dtor(cEntity *self) {
	self->vftable = CCAMPATHENTITY_VFTABLE;
	cPy_Function_dtor(&Path(self)->onEnd);
	cCamControl_dtor(self);
}

static cEntity *cCamPathEntity_Delete(cEntity *self, uint32_t flags) {
	cCamPathEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CCAMPATHENTITY_SIZE);
	}
	return self;
}

/* the index NN of name = prefix + two digits, -1 for none or over 32 */
static int cCamPathEntity_PointIndex(cEntity *self, const char *name, const char *prefix, uint32_t len) {
	(void)self;
	if (!strncmp(name, prefix, len)) {
		const signed char *d = (const signed char *)name + len;
		if (d[0] >= '0' && d[0] <= '9' && d[1] >= '0' && d[1] <= '9' && !d[2]) {
			int i = atoi((const char *)d);
			return i > CAMPATH_POINTS ? -1 : i;
		}
	}
	return -1;
}

static byte cCamPathEntity_VarF(cEntity *self, const char *name, double *d, int set) {
	cCamPathEntity *p = Path(self);
	int i;

	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "Time")) {
		if ((byte)set) {
			p->time = (float)*d;
			Entity_Update(self);
		} else {
			*d = p->time;
		}
		return 1;
	}
	i = cCamPathEntity_PointIndex(self, name, "Near", 4);
	if (i != -1) {
		if ((byte)set) {
			p->points[i].zNear = (float)*d;
		} else {
			*d = p->points[i].zNear;
		}
		return 1;
	}
	i = cCamPathEntity_PointIndex(self, name, "Far", 3);
	if (i != -1) {
		if ((byte)set) {
			p->points[i].zFar = (float)*d;
		} else {
			*d = p->points[i].zFar;
		}
		return 1;
	}
	i = cCamPathEntity_PointIndex(self, name, "FOV", 3);
	if (i != -1) {
		if ((byte)set) {
			p->points[i].fov = Cam_Deg2Rad(*d);
		} else {
			*d = Rad2Deg(p->points[i].fov);
		}
		return 1;
	}
	i = cCamPathEntity_PointIndex(self, name, "Quake", 5);
	if (i != -1) {
		if ((byte)set) {
			p->points[i].quake = Cam_Deg2Rad(*d);
		} else {
			*d = Rad2Deg(p->points[i].quake);
		}
		return 1;
	}
	i = cCamPathEntity_PointIndex(self, name, "Time", 4);
	if (i != -1) {
		if ((byte)set) {
			p->points[i].time = (float)*d;
		} else {
			*d = p->points[i].time;
		}
		return 1;
	}
	return cCamControl_VarF(self, name, d, set);
}

static const char *cCamPathEntity_GetClassName(cEntity *self) {
	(void)self;
	return CAMPATH_CLASSNAME;
}

static byte cCamPathEntity_VarI(cEntity *self, const char *name, int *iv, int set) {
	cCamPathEntity *p = Path(self);
	int i;

	if (!*name) {
		return 0;
	}
	i = cCamPathEntity_PointIndex(self, name, "FadeIn", 6);
	if (i != -1) {
		if ((byte)set) {
			p->points[i].fadeIn = *iv != 0;
		} else {
			*iv = p->points[i].fadeIn != 0;
		}
		return 1;
	}
	i = cCamPathEntity_PointIndex(self, name, "FadeOut", 7);
	if (i != -1) {
		if ((byte)set) {
			p->points[i].fadeOut = *iv != 0;
		} else {
			*iv = p->points[i].fadeOut != 0;
		}
		return 1;
	}
	if (!strcmp(name, "nWayPoint")) {
		if ((byte)set) {
			p->numPoints = *iv;
		} else {
			*iv = p->numPoints;
		}
		return 1;
	}
	return cCamControl_VarI(self, name, iv, set);
}

/* the camera at f (0..1) of the way from cur to next, shaken by the quake of the way */
static void cCamPathPoint_Refresh(cCamPathPoint *self);
static void cCamPathEntity_Interp(cEntity *self, cCamPathPoint *next, cCamPathPoint *cur, float f) {
	CGameCamera *cam = &((cCamControl *)self)->cam;
	vec3_t d, r;
	float s, u;

	cCamPathPoint_Refresh(next);
	cCamPathPoint_Refresh(cur);
	switch ((cur->fadeOut ? 2 : 0) + (cur->fadeIn != 0)) {
	case 1:
		s = f * f * (2.0f - f);
		break;
	case 2:
		u = 1.0f - f;
		s = 1.0f - u * u * (2.0f - u);
		break;
	case 3:
		s = (3.0f - (f + f)) * (f * f);
		break;
	default:
		s = f;
		break;
	}
	u = 1.0f - s;
	d.x = cur->ang.x - next->ang.x;
	d.y = cur->ang.y - next->ang.y;
	d.z = cur->ang.z - next->ang.z;
	const vec3_t *n = Vec3_NormAng(&r, &d);
	cam->rot.y = next->ang.y + n->y * u;
	cam->rot.z = next->ang.z + n->z * u;
	cam->rot.x = n->x * u + next->ang.x;
	cam->pos.y = next->pos.y * s + cur->pos.y * u;
	cam->pos.z = next->pos.z * s + cur->pos.z * u;
	cam->pos.x = next->pos.x * s + cur->pos.x * u;
	cam->zFar = cur->zFar * u + next->zFar * s;
	cam->zNear = cur->zNear * u + next->zNear * s;
	cam->fov = cur->fov * u + next->fov * s;
	/* the shake: three random angles in [-quake, quake] */
	float q = cur->quake * u + next->quake * s;
	float nq = -q;
	double range = (double)(q - nq);
	double lo = (double)nq;
	float rz = FxElem_Rand(range, lo);
	float ry = FxElem_Rand(range, lo);
	float rx = FxElem_Rand(range, lo);
	cam->rot.y = cam->rot.y + ry;
	cam->rot.x = rx + cam->rot.x;
	cam->rot.z = cam->rot.z + rz;
}

static byte cCamPathEntity_IsType(cEntity *self, const char *type) {
	if (!strcmp(type, TYEN_CAMPATH)) {
		return 1;
	}
	return cCamControl_IsType(self, type);
}

/* the values of the "Cam" entity the point names */
static void cCamPathPoint_Refresh(cCamPathPoint *self) {
	if (!self->cam[0]) {
		return;
	}
	cEntity *e = World_FindEntity(self->cam);
	if (e && Entity_IsType(e, TYEN_CAM)) {
		const CGameCamera *c = &((cCamControl *)e)->cam;
		self->zFar = c->zFar;
		self->zNear = c->zNear;
		self->pos = c->pos;
		self->ang = c->rot;
		self->fov = c->fov;
		self->quake = 0.0f;
	}
}

/* the time on by the frame; the way it is in, or the last point and OnEnd */
static void cCamPathEntity_Update(cEntity *self) {
	cCamPathEntity *p = Path(self);
	float acc = 0.0f;
	float t = (float)((cTimer_GetDelta(&g_pWorld->timer)) + p->time);

	p->time = t;
	int n = p->numPoints - 1;
	for (int i = 0; i < n; i++) {
		float d = p->points[i].time;
		if (d > 0.0f) {
			float f = (t - acc) / d;
			if (f >= 0.0f && 1.0f >= f) {
				cCamPathEntity_Interp(self, &p->points[i + 1], &p->points[i], f);
				return;
			}
		}
		acc = acc + d;
	}
	cCamPathPoint *last = &p->points[p->numPoints - 1];
	cCamPathEntity_Interp(self, last, last, 1.0f);
	cPy_Function_AddParamS(&p->onEnd, self->name);
	cPy_Function_Execute(&p->onEnd, 1);
}

static byte cCamPathEntity_VarS(cEntity *self, const char *name, char **s, int set) {
	cCamPathEntity *p = Path(self);

	if (!*name) {
		return 0;
	}
	int i = cCamPathEntity_PointIndex(self, name, "TgtCam", 6);
	if (i != -1) {
		if ((byte)set) {
			/* strcpy in the original */
			Str_Copy(p->points[i].cam, *s, 0x80);
			cCamPathPoint_Refresh(&p->points[i]);
		} else {
			*s = p->points[i].cam;
		}
		return 1;
	}
	if ((byte)set && !strcmp(name, "OnEnd")) {
		cPy_Function_SetFunc(&p->onEnd, *s);
		return 1;
	}
	return cCamControl_VarS(self, name, s, set);
}

static byte cCamPathEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set) {
	cCamPathEntity *p = Path(self);
	int i;

	if (!*name) {
		return 0;
	}
	i = cCamPathEntity_PointIndex(self, name, "Pos", 3);
	if (i != -1) {
		vec3_t *v = &p->points[i].pos;
		if ((byte)set) {
			float fx = (float)*x;
			float fy = (float)*y;
			float fz = (float)*z;
			v->x = fx;
			v->y = fy;
			v->z = fz;
		} else {
			*x = v->x;
			*y = v->y;
			*z = v->z;
		}
		return 1;
	}
	i = cCamPathEntity_PointIndex(self, name, "Ang", 3);
	if (i != -1) {
		vec3_t *v = &p->points[i].ang;
		if ((byte)set) {
			float fx = Cam_Deg2Rad(*x);
			float fy = Cam_Deg2Rad(*y);
			float fz = Cam_Deg2Rad(*z);
			v->x = fx;
			v->y = fy;
			v->z = fz;
		} else {
			*x = Rad2Deg(v->x);
			*y = Rad2Deg(v->y);
			*z = Rad2Deg(v->z);
		}
		return 1;
	}
	return cCamControl_VarV(self, name, x, y, z, set);
}

/* the vtable of cCamPathEntity */
static const cCamControlVtbl cCamPathEntity_vftable = {
	.base.Update = cCamPathEntity_Update,
	.base.Render = cEntity_Render,
	.base.UpdateOff = cEntity_UpdateOff,
	.base.Delete = cCamPathEntity_Delete,
	.base.GetClassName = cCamPathEntity_GetClassName,
	.base.IsType = cCamPathEntity_IsType,
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
	.base.VarI = cCamPathEntity_VarI,
	.base.VarF = cCamPathEntity_VarF,
	.base.VarS = cCamPathEntity_VarS,
	.base.VarV = cCamPathEntity_VarV,
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
