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

typedef struct CCamera {
	union {
		IObject3D base;
		struct {
			const IObject3DVtbl *vftable;
			uint32_t class;			/* CLASS_OBJ3D_CAMERA */
		};
	};
	vec3_t rot;
	vec3_t pos;
	vec3_t target;
	float zNear;
	float zFar;
	float rangeNear;		/* "RangoCerca" */
	float rangeFar;			/* "RangoLejos" */
	float fov;			/* radians */
	float aspect;
	float aspectScreen;
	int mode;
	vec3_t viewPos;			/* set with dir and up */
	vec3_t dir;
	vec3_t up;
	CNode3D *node;
	CNode3D *nodeTarget;
} CCamera;

CCamera *CCamera_ctor(CCamera *self);
CCamera *CCamera_ctorCopy(CCamera *self, const CCamera *other);
void CCamera_dtor(CCamera *self);
void CCamera_SetNode(CCamera *self, CNode3D *node);
/* dir and up from the angles, viewPos from pos */
void CCamera_UpdateVectors(CCamera *self);
/* the angles that look from pos at target */
void CCamera_LookAtRot(CCamera *self);
matrix_t *CCamera_GetView(CCamera *self, matrix_t *m);
matrix_t *CCamera_GetProj(CCamera *self, matrix_t *m);
void CCamera_Move(CCamera *self, float d);
/* along the x axis of the angles and the up vector */
void CCamera_Strafe(CCamera *self, float side, float rise);

#endif
