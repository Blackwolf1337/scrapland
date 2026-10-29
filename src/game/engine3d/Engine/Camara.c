/*
 * Cameras of the engine3d, original source Engine/camara.cpp (include/scrap/camera.h).
 */
#include <string.h>

#include "scrap/camara.h"
#include "scrap/scrapcamera.h"

#define CCAMARA_VFTABLE GAME_VFTABLE(CCamera_vftable, 0x3e1c98)
#define IOBJETO3D_VFTABLE GAME_VFTABLE(IObject3D_vftable, 0x3d79dc)
/* the limit of the projection, a float constant widened */
#define CAMERA_EPS ((double)0.01f)

typedef void(__thiscall *nodeDelete_t)(CNodeRef *self, int flags);

/* the reference to n in *slot, the old one released */
static void Camera_Link(CNodeRef **slot, CNodeRef *n) {
	if (n) {
		n->Refs++;
	}
	CHECK(!*slot || (*slot)->Refs > 1);
	if (*slot) {
		CNodeRef *old = *slot;
		old->Refs--;
		if (!old->Refs && old) {
			((nodeDelete_t)old->vftable[0])(old, 1);
		}
		*slot = NULL;
	}
	*slot = n;
}

static void CCamera_ClearVectors(CCamera *self) {
	memset(&self->viewPos, 0, sizeof(self->viewPos) + sizeof(self->dir) + sizeof(self->up));
}

CCamera *__fastcall CCamera_ctor(CCamera *self) {
	CCamera_ClearVectors(self);
	memset(&self->rot, 0, sizeof(self->rot) + sizeof(self->pos) + sizeof(self->target));
	self->vftable = CCAMARA_VFTABLE;
	self->class = CLASS_OBJ3D_CAMERA;
	self->node = NULL;
	self->nodeTarget = NULL;
	self->zNear = 1.0f;
	self->zFar = 1000.0f;
	self->f34 = 0.0f;
	self->f38 = 1000.0f;
	self->fov = 0.78539819f;
	self->aspect = 1.3333334f;
	self->aspectScreen = 1.3333334f;
	self->mode = CAMERA_LOOKAT;
	return self;
}

CCamera *__thiscall CCamera_ctorCopy(CCamera *self, const CCamera *other) {
	CCamera_ClearVectors(self);
	self->vftable = CCAMARA_VFTABLE;
	self->class = CLASS_OBJ3D_CAMERA;
	self->node = NULL;
	self->nodeTarget = NULL;
	Camera_Link(&self->node, other->node);
	Camera_Link(&self->nodeTarget, other->nodeTarget);
	memcpy(&self->rot, &other->rot, offsetof(CCamera, viewPos) - offsetof(CCamera, rot));
	return self;
}

void __fastcall CCamera_dtor(CCamera *self) {
	self->vftable = CCAMARA_VFTABLE;
	Camera_Link(&self->node, NULL);
	Camera_Link(&self->nodeTarget, NULL);
	self->vftable = IOBJETO3D_VFTABLE;
}

CCamera *__thiscall CCamera_Delete(CCamera *self, byte flags) {
	CCamera_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void __fastcall CCamera_Clear(CCamera *self) {
	CCamera_ClearVectors(self);
	self->class = CLASS_OBJ3D_CAMERA;
	self->node = NULL;
	self->nodeTarget = NULL;
}

void __thiscall CCamera_SetNode(CCamera *self, CNodeRef *node) {
	Camera_Link(&self->node, node);
}

void __thiscall CCamera_SetNodeTarget(CCamera *self, CNodeRef *node) {
	Camera_Link(&self->nodeTarget, node);
}

/* the members from rot to mode as the file has them */
static const struct {
	uint8_t off, size;
} cameraFields[] = {
	{ offsetof(CCamera, rot), 12 }, { offsetof(CCamera, pos), 12 }, { offsetof(CCamera, target), 12 },
	{ offsetof(CCamera, zNear), 4 }, { offsetof(CCamera, zFar), 4 }, { offsetof(CCamera, f34), 4 },
	{ offsetof(CCamera, f38), 4 }, { offsetof(CCamera, fov), 4 }, { offsetof(CCamera, aspect), 4 },
	{ offsetof(CCamera, aspectScreen), 4 }, { offsetof(CCamera, mode), 4 },
};

int __thiscall CCamera_Read(CCamera *self, CFileIO *f, void *ctx) {
	CCamera_SetNode(self, *(CNodeRef **)((byte *)ctx + 4));
	CFileIO_BeginBlock(f, "CAM");
	CHECK(CFileIO_ReadInt(f) == 1);
	for (size_t i = 0; i < sizeof(cameraFields) / sizeof(cameraFields[0]); i++) {
		CFileIO_Read(f, (byte *)self + cameraFields[i].off, cameraFields[i].size);
	}
	CFileIO_Error(f, "CAM");
	return 1;
}

int __thiscall CCamera_Write(CCamera *self, CFileIO *f, void *ctx) {
	CHECK(self->node == *(CNodeRef **)((byte *)ctx + 4));
	CFileIO_BeginBlockWrite(f, "CAM");
	CFileIO_WriteInt(f, 1);
	CHECK(!self->nodeTarget);
	for (size_t i = 0; i < sizeof(cameraFields) / sizeof(cameraFields[0]); i++) {
		CFileIO_Write(f, (byte *)self + cameraFields[i].off, cameraFields[i].size);
	}
	CFileIO_EndBlockWrite(f, "CAM");
	return 1;
}

void __fastcall CCamera_UpdateVectors(CCamera *self) {
	const vec3_t z = { 0.0f, 0.0f, 1.0f };
	const vec3_t y = { 0.0f, 1.0f, 0.0f };
	vec3_t t;

	vec3_t dir = *Vec3_Rotate(&t, &z, &self->rot);
	vec3_t up = *Vec3_Rotate(&t, &y, &self->rot);
	self->dir = dir;
	self->up = up;
	self->viewPos = self->pos;
}

/* v / |v| (the length summed as given), unchanged for length 0 */
static vec3_t Camera_Unit(vec3_t v, float len) {
	if ((double)len != 0.0) {
		float k = (float)(1.0 / (double)len);
		v.x = v.x * k;
		v.y = v.y * k;
		v.z = v.z * k;
	}
	return v;
}

void __thiscall CCamera_LookAt(CCamera *self, matrix_t *m, const vec3_t *dir, const vec3_t *up) {
	(void)self;
	vec3_t n = Camera_Unit(*dir, (float)__builtin_sqrt((double)(dir->x * dir->x + dir->y * dir->y + dir->z * dir->z)));
	float d = n.y * up->y + n.x * up->x + n.z * up->z;
	vec3_t u = { up->x - d * n.x, up->y - d * n.y, up->z - d * n.z };
	u = Camera_Unit(u, (float)__builtin_sqrt((double)(u.y * u.y + u.x * u.x + u.z * u.z)));
	float *a = m->m;
	a[3] = 0.0f;
	a[7] = 0.0f;
	a[11] = 0.0f;
	a[15] = 1.0f;
	a[0] = u.y * n.z - u.z * n.y;
	a[1] = u.x;
	a[2] = n.x;
	a[4] = u.z * n.x - u.x * n.z;
	a[5] = u.y;
	a[6] = n.y;
	a[8] = u.x * n.y - u.y * n.x;
	a[9] = u.z;
	a[10] = n.z;
}

void __fastcall CCamera_LookAtRot(CCamera *self) {
	float dy = self->target.y - self->pos.y;
	float dx = self->target.x - self->pos.x;
	float dz = self->target.z - self->pos.z;
	self->rot.z = 0.0f;
	self->rot.y = (float)Game_Atan2((double)dx, (double)dz);
	self->rot.x = (float)Game_Atan2((double)-dy, __builtin_sqrt((double)(dz * dz + dx * dx)));
}

void __thiscall CCamera_ViewMatrix(CCamera *self, matrix_t *m) {
	if (self->mode == CAMERA_LOOKAT) {
		CCamera_LookAtRot(self);
	}
	CCamera_UpdateVectors(self);
	CCamera_LookAt(self, m, &self->dir, &self->up);
	const vec3_t *p = &self->viewPos;
	float *a = m->m;
	a[12] = -(a[4] * p->y + p->x * a[0] + a[8] * p->z);
	a[13] = -(a[1] * p->x + a[5] * p->y + a[9] * p->z);
	a[14] = -(a[2] * p->x + a[6] * p->y + a[10] * p->z);
}

matrix_t *__thiscall CCamera_GetView(CCamera *self, matrix_t *m) {
	CCamera_ViewMatrix(self, m);
	return m;
}

byte __thiscall CCamera_ProjMatrix(CCamera *self, matrix_t *m) {
	float zNear = self->zNear, zFar = self->zFar;
	float d = zFar - zNear;
	float s, c;

	if (CAMERA_EPS > __builtin_fabs((double)d)) {
		return 0;
	}
	X87_SinCos(self->fov * 0.5f, &s, &c);
	if (CAMERA_EPS > __builtin_fabs((double)s)) {
		return 0;
	}
	float cot = c / s;
	float q = zFar / d;
	float sx = (float)((16.0 / 9.0 / (double)(self->aspectScreen * self->aspect)) * (double)cot);
	memset(m->m, 0, sizeof(m->m));
	m->m[0] = sx;
	m->m[11] = 1.0f;
	m->m[5] = (float)((double)cot * (4.0 / 3.0));
	m->m[10] = q;
	m->m[14] = -(q * zNear);
	return 1;
}

matrix_t *__thiscall CCamera_GetProj(CCamera *self, matrix_t *m) {
	CCamera_ProjMatrix(self, m);
	return m;
}

void __thiscall CCamera_Move(CCamera *self, float d) {
	self->pos.y = self->dir.y * d + self->pos.y;
	self->pos.z = self->dir.z * d + self->pos.z;
	self->pos.x = self->pos.x + self->dir.x * d;
}

void __thiscall CCamera_Strafe(CCamera *self, float side, float rise) {
	const vec3_t x = { 1.0f, 0.0f, 0.0f };
	vec3_t t;

	vec3_t v = *Vec3_Rotate(&t, &x, &self->rot);
	self->pos.y = v.y * side + self->pos.y;
	self->pos.z = v.z * side + self->pos.z;
	self->pos.x = self->pos.x + side * v.x;
	self->pos.x = self->pos.x + self->up.x * rise;
	self->pos.y = self->pos.y + self->up.y * rise;
	self->pos.z = self->pos.z + self->up.z * rise;
}

/* OBJ3D_VF_UPDATE: with CamaraNodos the fov and the position from the animated node */
void __thiscall CCamera_Update(CCamera *self, CRenderNode *r) {
	(void)r;
	if (!g_CameraNodes || !self->node || !CNode3D_Track((CNode3D *)self->node, 0)) {
		return;
	}
	CNode3D_AnimFloat((CNode3D *)self->node, &self->fov);
	const CNode3D *n = (const CNode3D *)self->node;
	self->pos = (vec3_t){ n->m.m[12], n->m.m[13], n->m.m[14] };
	CCamera_UpdateVectors(self);
}

/* OBJ3D_VF_RENDER, OBJ3D_VF_INITD3D and OBJ3D_VF_RELEASED3D do nothing */
void __thiscall CCamera_Render(CCamera *self, void *arg, CRenderNode *r) {
	(void)self;
	(void)arg;
	(void)r;
}

void __thiscall CCamera_InitD3D(CCamera *self, void *dev) {
	(void)self;
	(void)dev;
}

void __thiscall CCamera_ReleaseD3D(CCamera *self, void *dev) {
	(void)self;
	(void)dev;
}
