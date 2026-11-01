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
	float rangeNear;		/* 0x34 "RangoCerca" */
	float rangeFar;			/* 0x38 "RangoLejos" */
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

CCamera *CCamera_ctor(CCamera *self);
CCamera *CCamera_ctorCopy(CCamera *self, const CCamera *other);
void CCamera_dtor(CCamera *self);
CCamera *CCamera_Delete(CCamera *self, byte flags);
/* the members after the copied ones cleared, the nodes forgotten (not released) */
void CCamera_Clear(CCamera *self);
void CCamera_SetNode(CCamera *self, CNodeRef *node);
void CCamera_SetNodeTarget(CCamera *self, CNodeRef *node);
/* the "CAM" block; ctx holds the node at 0x4 */
int CCamera_Read(CCamera *self, CFileIO *f, void *ctx);
int CCamera_Write(CCamera *self, CFileIO *f, void *ctx);
/* dir and up from the angles, viewPos from pos */
void CCamera_UpdateVectors(CCamera *self);
/* the angles that look from pos at target */
void CCamera_LookAtRot(CCamera *self);
/* the rotation looking along dir with up (made orthogonal) */
void CCamera_LookAt(CCamera *self, matrix_t *m, const vec3_t *dir, const vec3_t *up);
void CCamera_ViewMatrix(CCamera *self, matrix_t *m);
matrix_t *CCamera_GetView(CCamera *self, matrix_t *m);
/* 0 for a near and far plane too close or a too narrow field of view */
byte CCamera_ProjMatrix(CCamera *self, matrix_t *m);
matrix_t *CCamera_GetProj(CCamera *self, matrix_t *m);
void CCamera_Move(CCamera *self, float d);
/* along the x axis of the angles and the up vector */
void CCamera_Strafe(CCamera *self, float side, float rise);
/* the IObject3D methods: Update follows the node with CamaraNodos, the others do nothing */
void CCamera_Update(CCamera *self, CRenderNode *r);
void CCamera_Render(CCamera *self, void *arg, CRenderNode *r);
void CCamera_InitD3D(CCamera *self, void *dev);
void CCamera_ReleaseD3D(CCamera *self, void *dev);

#endif
