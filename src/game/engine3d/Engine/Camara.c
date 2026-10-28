/*
 * Cameras of the engine3d, original source Engine/camara.cpp (include/scrap/camara.h).
 */
#include <string.h>

#include "scrap/camara.h"
#include "scrap/scrapcamera.h"

#define CCAMARA_VFTABLE GAME_VFTABLE(CCamara_vftable, 0x3e1c98)
#define IOBJETO3D_VFTABLE GAME_VFTABLE(IObjeto3D_vftable, 0x3d79dc)
/* the limit of the projection, a float constant widened */
#define CAMARA_EPS ((double)0.01f)

typedef void(__thiscall *nodoDelete_t)(CNodoRef *self, int flags);

/* the reference to n in *slot, the old one released */
static void Camara_Link(CNodoRef **slot, CNodoRef *n) {
	if (n) {
		n->Refs++;
	}
	CHECK(!*slot || (*slot)->Refs > 1);
	if (*slot) {
		CNodoRef *old = *slot;
		old->Refs--;
		if (!old->Refs && old) {
			((nodoDelete_t)old->vftable[0])(old, 1);
		}
		*slot = NULL;
	}
	*slot = n;
}

static void CCamara_ClearVectors(CCamara *self) {
	memset(&self->viewPos, 0, sizeof(self->viewPos) + sizeof(self->dir) + sizeof(self->up));
}

CCamara *__fastcall CCamara_ctor(CCamara *self) {
	CCamara_ClearVectors(self);
	memset(&self->rot, 0, sizeof(self->rot) + sizeof(self->pos) + sizeof(self->target));
	self->vftable = CCAMARA_VFTABLE;
	self->clase = CLASE_OBJ3D_CAMARA;
	self->nodo = NULL;
	self->nodoObjetivo = NULL;
	self->zNear = 1.0f;
	self->zFar = 1000.0f;
	self->f34 = 0.0f;
	self->f38 = 1000.0f;
	self->fov = 0.78539819f;
	self->aspect = 1.3333334f;
	self->aspectScreen = 1.3333334f;
	self->mode = CAMARA_LOOKAT;
	return self;
}

CCamara *__thiscall CCamara_ctorCopy(CCamara *self, const CCamara *other) {
	CCamara_ClearVectors(self);
	self->vftable = CCAMARA_VFTABLE;
	self->clase = CLASE_OBJ3D_CAMARA;
	self->nodo = NULL;
	self->nodoObjetivo = NULL;
	Camara_Link(&self->nodo, other->nodo);
	Camara_Link(&self->nodoObjetivo, other->nodoObjetivo);
	memcpy(&self->rot, &other->rot, offsetof(CCamara, viewPos) - offsetof(CCamara, rot));
	return self;
}

void __fastcall CCamara_dtor(CCamara *self) {
	self->vftable = CCAMARA_VFTABLE;
	Camara_Link(&self->nodo, NULL);
	Camara_Link(&self->nodoObjetivo, NULL);
	self->vftable = IOBJETO3D_VFTABLE;
}

CCamara *__thiscall CCamara_Delete(CCamara *self, byte flags) {
	CCamara_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void __fastcall CCamara_Clear(CCamara *self) {
	CCamara_ClearVectors(self);
	self->clase = CLASE_OBJ3D_CAMARA;
	self->nodo = NULL;
	self->nodoObjetivo = NULL;
}

void __thiscall CCamara_SetNodo(CCamara *self, CNodoRef *nodo) {
	Camara_Link(&self->nodo, nodo);
}

void __thiscall CCamara_SetNodoObjetivo(CCamara *self, CNodoRef *nodo) {
	Camara_Link(&self->nodoObjetivo, nodo);
}

/* the members from rot to mode as the file has them */
static const struct {
	uint8_t off, size;
} camaraFields[] = {
	{ offsetof(CCamara, rot), 12 }, { offsetof(CCamara, pos), 12 }, { offsetof(CCamara, target), 12 },
	{ offsetof(CCamara, zNear), 4 }, { offsetof(CCamara, zFar), 4 }, { offsetof(CCamara, f34), 4 },
	{ offsetof(CCamara, f38), 4 }, { offsetof(CCamara, fov), 4 }, { offsetof(CCamara, aspect), 4 },
	{ offsetof(CCamara, aspectScreen), 4 }, { offsetof(CCamara, mode), 4 },
};

int __thiscall CCamara_Read(CCamara *self, CArchIO *f, void *ctx) {
	CCamara_SetNodo(self, *(CNodoRef **)((byte *)ctx + 4));
	CArchIO_BeginBlock(f, "CAM");
	CHECK(CArchIO_ReadInt(f) == 1);
	for (size_t i = 0; i < sizeof(camaraFields) / sizeof(camaraFields[0]); i++) {
		CArchIO_Read(f, (byte *)self + camaraFields[i].off, camaraFields[i].size);
	}
	CArchIO_Error(f, "CAM");
	return 1;
}

int __thiscall CCamara_Write(CCamara *self, CArchIO *f, void *ctx) {
	CHECK(self->nodo == *(CNodoRef **)((byte *)ctx + 4));
	CArchIO_BeginBlockWrite(f, "CAM");
	CArchIO_WriteInt(f, 1);
	CHECK(!self->nodoObjetivo);
	for (size_t i = 0; i < sizeof(camaraFields) / sizeof(camaraFields[0]); i++) {
		CArchIO_Write(f, (byte *)self + camaraFields[i].off, camaraFields[i].size);
	}
	CArchIO_EndBlockWrite(f, "CAM");
	return 1;
}

void __fastcall CCamara_UpdateVectors(CCamara *self) {
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
static vec3_t Camara_Unit(vec3_t v, float len) {
	if ((double)len != 0.0) {
		float k = (float)(1.0 / (double)len);
		v.x = v.x * k;
		v.y = v.y * k;
		v.z = v.z * k;
	}
	return v;
}

void __thiscall CCamara_LookAt(CCamara *self, matrix_t *m, const vec3_t *dir, const vec3_t *up) {
	(void)self;
	vec3_t n = Camara_Unit(*dir, (float)__builtin_sqrt((double)(dir->x * dir->x + dir->y * dir->y + dir->z * dir->z)));
	float d = n.y * up->y + n.x * up->x + n.z * up->z;
	vec3_t u = { up->x - d * n.x, up->y - d * n.y, up->z - d * n.z };
	u = Camara_Unit(u, (float)__builtin_sqrt((double)(u.y * u.y + u.x * u.x + u.z * u.z)));
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

void __fastcall CCamara_LookAtRot(CCamara *self) {
	float dy = self->target.y - self->pos.y;
	float dx = self->target.x - self->pos.x;
	float dz = self->target.z - self->pos.z;
	self->rot.z = 0.0f;
	self->rot.y = (float)Game_Atan2((double)dx, (double)dz);
	self->rot.x = (float)Game_Atan2((double)-dy, __builtin_sqrt((double)(dz * dz + dx * dx)));
}

void __thiscall CCamara_ViewMatrix(CCamara *self, matrix_t *m) {
	if (self->mode == CAMARA_LOOKAT) {
		CCamara_LookAtRot(self);
	}
	CCamara_UpdateVectors(self);
	CCamara_LookAt(self, m, &self->dir, &self->up);
	const vec3_t *p = &self->viewPos;
	float *a = m->m;
	a[12] = -(a[4] * p->y + p->x * a[0] + a[8] * p->z);
	a[13] = -(a[1] * p->x + a[5] * p->y + a[9] * p->z);
	a[14] = -(a[2] * p->x + a[6] * p->y + a[10] * p->z);
}

matrix_t *__thiscall CCamara_GetView(CCamara *self, matrix_t *m) {
	CCamara_ViewMatrix(self, m);
	return m;
}

byte __thiscall CCamara_ProjMatrix(CCamara *self, matrix_t *m) {
	float zNear = self->zNear, zFar = self->zFar;
	float d = zFar - zNear;
	float s, c;

	if (CAMARA_EPS > __builtin_fabs((double)d)) {
		return 0;
	}
	X87_SinCos(self->fov * 0.5f, &s, &c);
	if (CAMARA_EPS > __builtin_fabs((double)s)) {
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

matrix_t *__thiscall CCamara_GetProj(CCamara *self, matrix_t *m) {
	CCamara_ProjMatrix(self, m);
	return m;
}

void __thiscall CCamara_Move(CCamara *self, float d) {
	self->pos.y = self->dir.y * d + self->pos.y;
	self->pos.z = self->dir.z * d + self->pos.z;
	self->pos.x = self->pos.x + self->dir.x * d;
}

void __thiscall CCamara_Strafe(CCamara *self, float side, float rise) {
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
void __thiscall CCamara_Update(CCamara *self, CNodoR *r) {
	(void)r;
	if (!g_CamaraNodos || !self->nodo || !CNodo3D_Track((CNodo3D *)self->nodo, 0)) {
		return;
	}
	CNodo3D_AnimFloat((CNodo3D *)self->nodo, &self->fov);
	const CNodo3D *n = (const CNodo3D *)self->nodo;
	self->pos = (vec3_t){ n->m.m[12], n->m.m[13], n->m.m[14] };
	CCamara_UpdateVectors(self);
}

/* OBJ3D_VF_RENDER, OBJ3D_VF_INITD3D and OBJ3D_VF_RELEASED3D do nothing */
void __thiscall CCamara_Render(CCamara *self, void *arg, CNodoR *r) {
	(void)self;
	(void)arg;
	(void)r;
}

void __thiscall CCamara_InitD3D(CCamara *self, void *dev) {
	(void)self;
	(void)dev;
}

void __thiscall CCamara_ReleaseD3D(CCamara *self, void *dev) {
	(void)self;
	(void)dev;
}
