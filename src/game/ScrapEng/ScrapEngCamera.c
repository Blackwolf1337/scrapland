/*
 * ScrapEngCamera.cpp of the game: CGameCamera, see include/scrap/scrapcamera.h.
 */
#include <stddef.h>
#include <string.h>

#include "scrap/animation.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapfont.h"
#include "scrap/misc.h"
#include "scrap/staticinit.h"

#define g_CameraDefViewAspect (GVar_CameraDefViewAspect.value.f)	/* CameraDefViewAspect */
#define g_ShowSceneCamera (GVar_ShowSceneCamera.value.i)		/* ShowSceneCamera */
static cGVar GVar_ShowSceneCamera;
static cGVar GVar_CameraDefViewAspect;
#define CAM_CHECK(self) \
	do { \
		if (!(self)->Camera) { \
			CheckFailed("Camera", __FILE__, __TIMESTAMP__, __LINE__); \
		} \
	} while (0)

#define CAM_FOV 0x1.921fb6p+0f		/* pi / 2 */
#define CAM_ROLL_EPS 0x1.6e05a695f8191p-13	/* 0.01 degrees in radians: smaller rolls are dropped */
#define CAM_ASPECT_WIDE 0x1.c71c71c71c71cp+0	/* 16 / 9 */
#define CAM_ASPECT_4_3 0x1.5555555555555p+0

CGameCamera *CGameCamera_ctor(CGameCamera *self) {
	CCamera *c = operator_new(sizeof(CCamera));
	c = c ? CCamera_ctor(c) : NULL;
	self->Camera = c;
	self->Node = NULL;
	memset(self, 0, offsetof(CGameCamera, zNear));
	self->zNear = 10.0f;
	self->zFar = 100000.0f;
	self->fov = CAM_FOV;
	self->aspectScreen = g_CameraDefViewAspect;
	CAM_CHECK(self);
	self->Camera->mode = CAMERA_FREE;
	return self;
}

void CGameCamera_dtor(CGameCamera *self) {
	RenderNode_Unref(&self->Node);
	if (Camera_GetCurrent() == self) {
		CheckFailed("GetActiveCamera() != this", __FILE__, __TIMESTAMP__, __LINE__);
	}
	CAM_CHECK(self);
	if (self->Camera) {
		Object3D_Delete(self->Camera, 1);
		self->Camera = NULL;
	}
}

void CGameCamera_Move(CGameCamera *self, float d) {
	CGameCamera_Update(self);
	CCamera_Move(self->Camera, d);
	self->pos = self->Camera->pos;
}

static byte CGameCamera_IsLookAt(const CGameCamera *self);
void CGameCamera_Copy(CGameCamera *self, const CGameCamera *other) {
	if (self->Node) {
		CheckFailed("!Node", __FILE__, __TIMESTAMP__, __LINE__);
	}
	memcpy(self, other, offsetof(CGameCamera, Camera));
	CGameCamera_SetLookAt(self, CGameCamera_IsLookAt(other));
}

void CGameCamera_FromCamera(CGameCamera *self, const CCamera *c) {
	self->pos = c->pos;
	self->rot = c->rot;
	self->target = c->target;
	self->fov = c->fov;
	CGameCamera_SetLookAt(self, c->mode == CAMERA_LOOKAT);
}

static byte CGameCamera_IsLookAt(const CGameCamera *self) {
	CAM_CHECK(self);
	return self->Camera->mode == CAMERA_LOOKAT;
}

void CGameCamera_SetLookAt(CGameCamera *self, byte on) {
	CAM_CHECK(self);
	self->Camera->mode = (on == 0) + CAMERA_LOOKAT;
}

byte CGameCamera_SetNodeModel(CGameCamera *self, CGameModel *model, int i) {
	RenderNode_Unref(&self->Node);
	if (model) {
		if (i == -1) {
			i = CGameModel_FindRenderNode(model, "D_Cam-Composite");
			if (i == -1) {
				i = CGameModel_FindRenderNode(model, "Camera_01");
			}
		}
		CRenderNode *node = CGameModel_RenderNodeRef(model, (uint32_t)i);
		RenderNode_Unref(&self->Node);
		self->Node = node;
	}
	return 1;
}

/* what CamComposite_Find found last, for ShowSceneCamera */
static int g_CamCompositeIdx;
static float g_CamCompositeFrame;
static float g_CamCompositeFPS = 1.0f;

/* the camera node of the [Cam-Composite] section ini of a node of model, NULL with a log line for none */
static CNode3D *CamComposite_Find(CModel *model, CIniFile *ini) {
	char key[32], name[128];
	int idx = -1;

	TABLE_CHECK(model->slots.p, 0, model->slots.n);
	CAnimGroup *anim = ((CAnimSlot *)model->slots.p)[0].anim;
	float frame = anim ? anim->ctrl.frame : 0.0f;
	for (int i = 1; i < 100; i++) {
		Str_PrintfQuiet(key, sizeof(key), "%02d_StartFrame", i);
		float start = CIniFile_GetFloat(ini, "Cam-Composite", key, -1.0f);
		if (start > frame || 0.0f > start) {
			idx = i - 1;
			break;
		}
	}
	g_CamCompositeIdx = idx;
	g_CamCompositeFrame = frame;
	g_CamCompositeFPS = anim ? anim->ctrl.vel : 1.0f;
	if (idx > 0) {
		Str_PrintfQuiet(key, sizeof(key), "%02d_Camera", idx);
		if (CIniFile_GetText(ini, "Cam-Composite", key, name, sizeof(name), NULL)) {
			CRenderNode *r = CModel_FindRenderNode(model, name, NULL, 1, 0);
			if (r) {
				return r->node;
			}
			E3D_LogBlue((&g_FileLog), "CAM-COMPOSITE camera not found: %s", name);
		}
	}
	if (idx) {
		E3D_LogBlue((&g_FileLog), "CAM-COMPOSITE information invalid on model: %s", model->name);
	}
	return NULL;
}

CFont *g_FontHoratioSmall;
static char g_CamCompositeName[128];

void CamComposite_DrawDebug(void) {
	if (!g_ShowSceneCamera) {
		return;
	}
	CFont_PrintfColorShadow(g_FontHoratioSmall, 10.0f, 400.0f, 0xff00, "%02d", g_CamCompositeIdx);
	CFont_PrintfColorShadow(g_FontHoratioSmall, g_FontHoratioSmall->x, 400.0f, 0xffffff, " %s", g_CamCompositeName);
	CFont_PrintfColorShadow(g_FontHoratioSmall, g_FontHoratioSmall->x, 400.0f, 0xff00, " %d",
		(int)g_CamCompositeFrame);
	CFont_PrintfColorShadow(g_FontScrapBig, 10.0f, 420.0f, 0xff00, "%d", (int)g_CamCompositeFrame);
	int frame = (int)g_CamCompositeFrame;
	int fps = (int)g_CamCompositeFPS;
	CFont_PrintfColorShadow(g_FontScrapBig, 120.0f, 420.0f, 0xff00, "%d:%02d", frame / fps, frame % fps);
}

void CGameCamera_Strafe(CGameCamera *self, float side, float rise) {
	CGameCamera_Update(self);
	CCamera_Strafe(self->Camera, side, rise);
	if (self->Camera->mode == CAMERA_LOOKAT) {
		CCamera_LookAtRot(self->Camera);
	}
	self->pos = self->Camera->pos;
	self->rot = self->Camera->rot;
}

/* the position and angles of the camera node node of model, the fov of its animation */
static void CGameCamera_FromNode(CGameCamera *self, CModel *model, CNode3D *node) {
	matrix_t m, rx, mr;
	vec3_t ang;
	float fov = CAM_FOV;

	CModel_GetNodeMatrix(model, &m, (uint32_t)node->slot, 1, 0, NULL, &fov);
	Matrix_RotationX(&rx, CAM_FOV);
	CAM_CHECK(self);
	self->Camera->mode = CAMERA_FREE;
	self->pos.x = m.m[12];
	self->pos.y = m.m[13];
	self->pos.z = m.m[14];
	self->rot = *Matrix_ToEuler(&ang, Matrix_MulD3D(&mr, &rx, &m));
	self->fov = fov;
	if (CAM_ROLL_EPS > __builtin_fabs((double)self->rot.z)) {
		self->rot.z = 0.0f;
	}
	if (g_ShowSceneCamera) {
		Str_Copy(g_CamCompositeName, node->name, 0x80);
	}
}

int g_CameraNodes = 1;

CCamera *CGameCamera_Update(CGameCamera *self) {
	CAM_CHECK(self);
	CRenderNode *node = self->Node;
	if (node) {
		CModel *model = (CModel *)node->owner;
		CNode3D *node3d = node->node;
		if (CNode3D_Class(node3d) != CLASS_OBJ3D_CAMERA && node3d->ini) {
			node3d = CamComposite_Find(model, node3d->ini);
		}
		if (g_CameraNodes && node3d && CNode3D_Class(node3d) == CLASS_OBJ3D_CAMERA) {
			CGameCamera_FromNode(self, model, node3d);
		}
	}
	CCamera *c = self->Camera;
	c->pos = self->pos;
	c->rot = self->rot;
	c->target = self->target;
	c->zNear = self->zNear;
	c->zFar = self->zFar;
	c->fov = self->fov;
	if (g_CameraScene == 1) {
		float aspect = g_PhysicalAspectRatio;
		if ((double)aspect > CAM_ASPECT_WIDE) {
			c->fov = (float)((double)c->fov * 0.75);
		} else {
			c->fov = (float)(CAM_ASPECT_4_3 / (double)aspect * (double)c->fov);
		}
	}
	c->aspect = g_PhysicalAspectRatio;
	c->aspectScreen = self->aspectScreen;
	return self->Camera;
}

/* the engine variables (Gvars) */
static cGVar GVar_CameraDefViewAspect;
static cGVar GVar_ShowSceneCamera;
cGVar GVar_PhysicalAspectRatio;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_PhysicalAspectRatio, &GVar_PhysicalAspectRatio, "PhysicalAspectRatio", 0, "Physical aspect ratio\n", 1.3333334f)
INIT_GVAR_FLOAT(Init_GVar_CameraDefViewAspect, &GVar_CameraDefViewAspect, "CameraDefViewAspect", 0, "Default logical viewport aspect ratio", 1.3333334f)
INIT_GVAR_INT(Init_GVar_ShowSceneCamera, &GVar_ShowSceneCamera, "ShowSceneCamera", 0, "Show cutscene camera name and frame number", 0)
