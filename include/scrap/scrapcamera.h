/*
 * ScrapEngCamera.cpp of the game: CGameCamera, the camera of the game over a CCamara of the engine3d. It keeps its own
 * copy of the camera values and writes them to the CCamara on each update; with a render node of a model it follows
 * the camera node of the model (or the camera of the [Cam-Composite] section of the node: the camera whose
 * NN_StartFrame range holds the frame of the first animation slot).
 */
#ifndef SCRAP_SCRAPCAMERA_H
#define SCRAP_SCRAPCAMERA_H

#include "scrap/camara.h"
#include "scrap/modelo.h"
#include "scrap/scrapmodel.h"

#define CAMARA_FREE (CAMARA_LOOKAT + 1)	/* mode: the angles are set directly */

typedef struct CGameCamera {
	vec3_t pos;			/* 0x00 */
	vec3_t rot;			/* 0x0c */
	vec3_t target;			/* 0x18 */
	float zNear;			/* 0x24 */
	float zFar;			/* 0x28 */
	float fov;			/* 0x2c radians */
	float aspectScreen;		/* 0x30 */
	CCamara *Camera;		/* 0x34 owned */
	CNodoR *Node;			/* 0x38 a reference (CGameModel_NodoRRef) */
} CGameCamera;
SD_OFFSET(CGameCamera, Camera, 0x34);
SD_SIZE(CGameCamera, 0x3c);

/* the engine variables (Gvars) */
#define g_CameraDefViewAspect GAME_DATA(float, 0x4a8938)	/* CameraDefViewAspect */
#define g_PhysicalAspectRatio GAME_DATA(float, 0x4a8968)	/* PhysicalAspectRatio */
#define g_ShowSceneCamera GAME_DATA(int, 0x4a8950)		/* ShowSceneCamera */
#define g_CameraScene GAME_DATA(int, 0x4600a0)		/* CameraScene */
/* 1: the cameras follow their scene nodes (also CCamara at rva 0x29cba0) */
#define g_CamaraNodos GAME_DATA(int, 0x431bc4)
/* what CamComposite_Find found last, for ShowSceneCamera */
#define g_CamCompositeIdx GAME_DATA(int, 0x4a8970)
#define g_CamCompositeFrame GAME_DATA(float, 0x4a8974)
#define g_CamCompositeFPS GAME_DATA(float, 0x431b54)
#define g_CamCompositeName ((char *)Game_Ptr(0x4a8978))	/* [0x80] */

CGameCamera *__fastcall CGameCamera_ctor(CGameCamera *self);
void __fastcall CGameCamera_dtor(CGameCamera *self);
void __thiscall CGameCamera_Move(CGameCamera *self, float d);
/* the values, not the node; the mode of the camera too */
void __thiscall CGameCamera_Copy(CGameCamera *self, const CGameCamera *other);
void __thiscall CGameCamera_FromCamara(CGameCamera *self, const CCamara *c);
byte __fastcall CGameCamera_IsLookAt(const CGameCamera *self);
void __thiscall CGameCamera_SetLookAt(CGameCamera *self, byte on);
/* the node i of model (-1: D_Cam-Composite, else Camera_01), none without model; 1 */
byte __thiscall CGameCamera_SetNodeModel(CGameCamera *self, CGameModel *model, int i);
/* takes over the reference of node */
void __thiscall CGameCamera_SetNode(CGameCamera *self, CNodoR *node);
/* the camera node of the [Cam-Composite] section ini of a node of model, NULL with a log line for none */
CNodo3D *CamComposite_Find(CModelo *model, CArchINI *ini);
/* ShowSceneCamera: the camera and frame of the last CamComposite_Find */
void CamComposite_DrawDebug(void);
void __thiscall CGameCamera_Strafe(CGameCamera *self, float side, float rise);
/* from the node, then the values into the CCamara; the CCamara */
CCamara *__fastcall CGameCamera_Update(CGameCamera *self);
/* in turned into the camera space (no projection) */
vec3_t *__thiscall CGameCamera_ToLocal(const CGameCamera *self, vec3_t *out, const vec3_t *in);

/* rva 0x215c00 (ScrapEngMain.cpp): the camera the world is drawn with */
CGameCamera *ScrapEng_GetActiveCamera(void);

#endif
