/*
 * CCamara of the engine3d (original source Engine/camara.cpp): a camera object of the scenes, linked to the scene
 * nodes it follows. The nodes are reference counted (Refs at 0x8), the last release deletes them through their
 * virtual destructor.
 */
#ifndef SCRAP_CAMARA_H
#define SCRAP_CAMARA_H

#include "scrap/archio.h"
#include "scrap/mathlib.h"

#define CLASE_OBJ3D_CAMARA 0xa2000004
#define CAMARA_LOOKAT 0x463		/* mode: the angles follow the target */

typedef struct CNodoRef {
	void **vftable;			/* 0x0 [0] the deleting destructor */
	int pad_4;
	int Refs;			/* 0x8 */
} CNodoRef;

typedef struct CCamara {
	void *vftable;			/* 0x00 */
	uint32_t clase;			/* 0x04 CLASE_OBJ3D_CAMARA */
	vec3_t rot;			/* 0x08 */
	vec3_t pos;			/* 0x14 */
	vec3_t target;			/* 0x20 */
	float zNear;			/* 0x2c */
	float zFar;			/* 0x30 */
	float f34;			/* 0x34 */
	float f38;			/* 0x38 */
	float fov;			/* 0x3c radians */
	float aspect;			/* 0x40 */
	float aspectScreen;		/* 0x44 */
	int mode;			/* 0x48 */
	vec3_t viewPos;			/* 0x4c set with dir and up */
	vec3_t dir;			/* 0x58 */
	vec3_t up;			/* 0x64 */
	CNodoRef *nodo;			/* 0x70 */
	CNodoRef *nodoObjetivo;		/* 0x74 */
} CCamara;
SD_SIZE(CCamara, 0x78);

CCamara *__fastcall CCamara_ctor(CCamara *self);
CCamara *__thiscall CCamara_ctorCopy(CCamara *self, const CCamara *other);
void __fastcall CCamara_dtor(CCamara *self);
CCamara *__thiscall CCamara_Delete(CCamara *self, byte flags);
/* the members after the copied ones cleared, the nodes forgotten (not released) */
void __fastcall CCamara_Clear(CCamara *self);
void __thiscall CCamara_SetNodo(CCamara *self, CNodoRef *nodo);
void __thiscall CCamara_SetNodoObjetivo(CCamara *self, CNodoRef *nodo);
/* the "CAM" block; ctx holds the node at 0x4 */
int __thiscall CCamara_Read(CCamara *self, CArchIO *f, void *ctx);
int __thiscall CCamara_Write(CCamara *self, CArchIO *f, void *ctx);
/* dir and up from the angles, viewPos from pos */
void __fastcall CCamara_UpdateVectors(CCamara *self);
/* the angles that look from pos at target */
void __fastcall CCamara_LookAtRot(CCamara *self);
/* the rotation looking along dir with up (made orthogonal) */
void __thiscall CCamara_LookAt(CCamara *self, matrix_t *m, const vec3_t *dir, const vec3_t *up);
void __thiscall CCamara_ViewMatrix(CCamara *self, matrix_t *m);
matrix_t *__thiscall CCamara_GetView(CCamara *self, matrix_t *m);
/* 0 for a near and far plane too close or a too narrow field of view */
byte __thiscall CCamara_ProjMatrix(CCamara *self, matrix_t *m);
matrix_t *__thiscall CCamara_GetProj(CCamara *self, matrix_t *m);
void __thiscall CCamara_Move(CCamara *self, float d);
/* along the x axis of the angles and the up vector */
void __thiscall CCamara_Strafe(CCamara *self, float side, float rise);

#endif
