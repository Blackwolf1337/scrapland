/*
 * CCamera of the engine3d (original source Engine/camara.cpp): a camera object of the scenes, linked to the scene
 * nodes it follows. The nodes are reference counted (Refs at 0x8), the last release deletes them through their
 * virtual destructor.
 */
#ifndef SCRAP_CAMERA_H
#define SCRAP_CAMERA_H

#include "scrap/archio.h"
#include "scrap/mathlib.h"
#include "scrap/node3d.h"

#define CAMERA_LOOKAT 0x463		/* mode: the angles follow the target */

typedef struct CNodeRef {
	void **vftable;			/* 0x0 [0] the deleting destructor */
	int pad_4;
	int Refs;			/* 0x8 */
} CNodeRef;

typedef struct CCamera {
	void *vftable;			/* 0x00 */
	uint32_t class;			/* 0x04 CLASS_OBJ3D_CAMERA */
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
	CNodeRef *node;			/* 0x70 */
	CNodeRef *nodeTarget;		/* 0x74 */
} CCamera;
SD_SIZE(CCamera, 0x78);

CCamera *__fastcall CCamera_ctor(CCamera *self);
CCamera *__thiscall CCamera_ctorCopy(CCamera *self, const CCamera *other);
void __fastcall CCamera_dtor(CCamera *self);
CCamera *__thiscall CCamera_Delete(CCamera *self, byte flags);
/* the members after the copied ones cleared, the nodes forgotten (not released) */
void __fastcall CCamera_Clear(CCamera *self);
void __thiscall CCamera_SetNode(CCamera *self, CNodeRef *node);
void __thiscall CCamera_SetNodeTarget(CCamera *self, CNodeRef *node);
/* the "CAM" block; ctx holds the node at 0x4 */
int __thiscall CCamera_Read(CCamera *self, CFileIO *f, void *ctx);
int __thiscall CCamera_Write(CCamera *self, CFileIO *f, void *ctx);
/* dir and up from the angles, viewPos from pos */
void __fastcall CCamera_UpdateVectors(CCamera *self);
/* the angles that look from pos at target */
void __fastcall CCamera_LookAtRot(CCamera *self);
/* the rotation looking along dir with up (made orthogonal) */
void __thiscall CCamera_LookAt(CCamera *self, matrix_t *m, const vec3_t *dir, const vec3_t *up);
void __thiscall CCamera_ViewMatrix(CCamera *self, matrix_t *m);
matrix_t *__thiscall CCamera_GetView(CCamera *self, matrix_t *m);
/* 0 for a near and far plane too close or a too narrow field of view */
byte __thiscall CCamera_ProjMatrix(CCamera *self, matrix_t *m);
matrix_t *__thiscall CCamera_GetProj(CCamera *self, matrix_t *m);
void __thiscall CCamera_Move(CCamera *self, float d);
/* along the x axis of the angles and the up vector */
void __thiscall CCamera_Strafe(CCamera *self, float side, float rise);
/* the IObject3D methods: Update follows the node with CamaraNodos, the others do nothing */
void __thiscall CCamera_Update(CCamera *self, CRenderNode *r);
void __thiscall CCamera_Render(CCamera *self, void *arg, CRenderNode *r);
void __thiscall CCamera_InitD3D(CCamera *self, void *dev);
void __thiscall CCamera_ReleaseD3D(CCamera *self, void *dev);

#endif
