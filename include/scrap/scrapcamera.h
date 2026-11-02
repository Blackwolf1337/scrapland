/*
 * ScrapEngCamera.cpp of the game: CGameCamera, the camera of the game over a CCamera of the engine3d. It keeps its own
 * copy of the camera values and writes them to the CCamera on each update; with a render node of a model it follows
 * the camera node of the model (or the camera of the [Cam-Composite] section of the node: the camera whose
 * NN_StartFrame range holds the frame of the first animation slot).
 */
#ifndef SCRAP_SCRAPCAMERA_H
#define SCRAP_SCRAPCAMERA_H

#include "scrap/gvar.h"
#include "scrap/camera.h"
#include "scrap/model.h"
#include "scrap/scrapmodel.h"

#define CAMERA_FREE (CAMERA_LOOKAT + 1)	/* mode: the angles are set directly */

typedef struct CGameCamera {
	vec3_t pos;			/* 0x00 */
	vec3_t rot;			/* 0x0c */
	vec3_t target;			/* 0x18 */
	float zNear;			/* 0x24 */
	float zFar;			/* 0x28 */
	float fov;			/* 0x2c radians */
	float aspectScreen;		/* 0x30 */
	CCamera *Camera;		/* 0x34 owned */
	CRenderNode *Node;			/* 0x38 a reference (CGameModel_RenderNodeRef) */
} CGameCamera;

extern cGVar GVar_PhysicalAspectRatio;
#define g_PhysicalAspectRatio (GVar_PhysicalAspectRatio.value.f)	/* PhysicalAspectRatio */
extern cGVar GVar_CameraScene;
#define g_CameraScene (GVar_CameraScene.value.i)		/* CameraScene */
/* 1: the cameras follow their scene nodes (also CCamera at rva 0x29cba0) */
extern int g_CameraNodes;

CGameCamera *CGameCamera_ctor(CGameCamera *self);
void CGameCamera_dtor(CGameCamera *self);
void CGameCamera_Move(CGameCamera *self, float d);
/* the values, not the node; the mode of the camera too */
void CGameCamera_Copy(CGameCamera *self, const CGameCamera *other);
void CGameCamera_FromCamera(CGameCamera *self, const CCamera *c);
void CGameCamera_SetLookAt(CGameCamera *self, byte on);
/* the node i of model (-1: D_Cam-Composite, else Camera_01), none without model; 1 */
byte CGameCamera_SetNodeModel(CGameCamera *self, CGameModel *model, int i);
/* ShowSceneCamera: the camera and frame of the last CamComposite_Find */
void CamComposite_DrawDebug(void);
void CGameCamera_Strafe(CGameCamera *self, float side, float rise);
/* from the node, then the values into the CCamera; the CCamera */
CCamera *CGameCamera_Update(CGameCamera *self);

/* rva 0x215c00 (ScrapEngMain.cpp): the camera the world is drawn with */

#endif
