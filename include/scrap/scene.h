/*
 * Engine/escena.cpp of the engine3d: the scene (CScene, the struct is in node3d.h), a list of nodes with their
 * materials, an animation (AnimM3D) and the user properties. It is read and written as the "SCN" block, converted
 * to vertex buffer meshes (convertir_a_D3D), to a floor (convertir_a_suelo) and lit (precalc_ilum).
 */
#ifndef SCRAP_SCENE_H
#define SCRAP_SCENE_H

#include "scrap/node3d.h"

/* node flags read here */
#define NODE_JOIN1 (1u << 0)		/* CScene_Join with sel */
#define NODE_JOIN2 (1u << 1)		/* CScene_Join without sel */
#define NODE_DELETE (1u << 3)		/* joined into another node: deleted by CScene_DeleteEmpty */
#define NODE_COLLIDE (1u << 7)		/* cleared on the node that receives a join */
#define NODE_BONE (1u << 20)		/* a bone, never deleted */
#define NODE_HIDDEN (1u << 25)		/* skipped by CScene_PrecalcIllum with visible */

/* the messages of CScene_Msg */
enum {
	SCENE_MSG_JOIN = 0,		/* a: sel, b: !sel; nothing without both */
	SCENE_MSG_CONVERTD3D = 1,	/* a: flags */
	SCENE_MSG_CALCWEIGHTS = 2,
	SCENE_MSG_CAMERAS = 3		/* a, b: the floats at 0x2c and 0x30 of the cameras, 0 keeps them */
};

CScene *CScene_ctor(CScene *self);
void CScene_Msg(CScene *self, int msg, int a, int b, int c, int d);
/* ReleaseD3D and InitD3D of the objects of all nodes */
void CScene_ReleaseD3D(CScene *self, void *dev);
void CScene_InitD3D(CScene *self, void *dev);
/* empty */
void CScene_Shared(CScene *self);
/* the "SCN" block; bAnim: the animation must be there (read) or is all there is (write) */
void CScene_Read(CScene *self, CFileIO *f, byte bAnim);
void CScene_Write(CScene *self, CFileIO *f, byte bAnim);
/* all nodes (those with the class bits 0xe0 last), or the render nodes of the model of ctx */
void CScene_Render(CScene *self, NodeAnimCtx *ctx);
void CScene_Update(CScene *self, NodeAnimCtx *ctx);
void CScene_UpdateNodes(CScene *self);
/* node, unlinked, at the end of the list */
void CScene_AddNode(CScene *self, CNode3D *node);
void CScene_RemoveNode(CScene *self, CNode3D *node);
/* the materials without attrib 4 released, the others moved to the front */
void CScene_CompactMats(CScene *self);
void CScene_DeleteNodes(CScene *self);
CNode3D *CScene_NodeObject(CScene *self, int index);
/* the next node after from (the first without) of the class, any with class 0 */
CNode3D *CScene_FindClass(CScene *self, CNode3D *from, uint32_t class);
CNode3D *CScene_FindNode(CScene *self, const char *name);
/* box from the boxes of the nodes, merged into out (copied if first); an empty scene gets a zero box with first */
void CScene_UpdateBox(CScene *self, box_t *out, byte first);
int CScene_NumNodes(CScene *self);
/* convertir_a_D3D; the static lighting first when the ini says so */
void CScene_ConvertD3D(CScene *self, uint32_t flags);
/* convertir_a_suelo: a floor node made of the meshes, the joined nodes deleted */
void CScene_ToGround(CScene *self);
/* the nodes of the class (only NODE_DELETE ones with flagged) that nobody else holds deleted, repeated */
void CScene_DeleteEmpty(CScene *self, uint32_t class, byte bRefs, byte flagged);
void CScene_SetScale(CScene *self, const vec3_t *scale);
/* the meshes under each NODE_JOIN1 (sel) or NODE_JOIN2 node (with a name containing name) joined into it */
void CScene_Join(CScene *self, byte sel, const char *name);
/* the alpha of the materials of the vertex buffer meshes, of all nodes except the one named name */
void CScene_SetMarks(CScene *self, int v);
/* precalc_ilum of the meshes and the floors; scene lights unless noLights, visible skips NODE_HIDDEN */
void CScene_PrecalcIllum(CScene *self, byte noLights, byte visible);

/* other modules (src/imports.inc) */
/* the box of box through m */
box_t *Box_Transform(box_t *out, const box_t *box, const matrix_t *m);
/* NodeFX_New (the FX object of a NODE_FX node): nodefx.h */
/* CAnimation_ctor, CAnimation_dtor: animation.h */
/* CalcWeights.c, CalcIllum.c and: per mesh, with the scene lights of esc */
struct CMeshD3D;
byte CMeshD3D_CalcWeights(struct CMeshD3D *mesh, CNode3D *node);
void CMeshD3D_CalcIllum(struct CMeshD3D *mesh, CNode3D *node, CScene *scn);
void CGround_CalcIllum(void *ground, CNode3D *node, CScene *scn);

#endif
