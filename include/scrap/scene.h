/*
 * Engine/escena.cpp of the engine3d: the scene (CScene, the struct is in node3d.h), a list of nodes with their
 * materials, an animation (AnimM3D) and the user properties. It is read and written as the "SCN" block, converted
 * to vertex buffer meshes (convertir_a_D3D), to a floor (convertir_a_suelo) and lit (precalc_ilum).
 */
#ifndef SCRAP_SCENE_H
#define SCRAP_SCENE_H

#include "scrap/node3d.h"

#define CSCENE_VFTABLE GAME_VFTABLE(CScene_vftable, 0x3d9b24)

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


CScene *__fastcall CScene_ctor(CScene *self);
void __fastcall CScene_dtor(CScene *self);
CScene *__thiscall CScene_Delete(CScene *self, byte flags);
void __thiscall CScene_Msg(CScene *self, int msg, int a, int b, int c, int d);
/* OBJ3D_VF_RELEASED3D and OBJ3D_VF_INITD3D of the objects of all nodes */
void __thiscall CScene_ReleaseD3D(CScene *self, void *dev);
void __thiscall CScene_InitD3D(CScene *self, void *dev);
/* rva 0x24ecb0, empty */
void __thiscall CScene_Shared(CScene *self);
/* the "SCN" block; bAnim: the animation must be there (read) or is all there is (write) */
void __thiscall CScene_Read(CScene *self, CFileIO *f, byte bAnim);
void __thiscall CScene_Write(CScene *self, CFileIO *f, byte bAnim);
/* all nodes (those with the class bits 0xe0 last), or the render nodes of the model of ctx */
void __thiscall CScene_Render(CScene *self, NodeAnimCtx *ctx);
void __thiscall CScene_Update(CScene *self, NodeAnimCtx *ctx);
void __fastcall CScene_UpdateNodes(CScene *self);
/* node, unlinked, at the end of the list */
void __thiscall CScene_AddNode(CScene *self, CNode3D *node);
void __thiscall CScene_RemoveNode(CScene *self, CNode3D *node);
/* the materials without attrib 4 released, the others moved to the front */
void __fastcall CScene_CompactMats(CScene *self);
void __fastcall CScene_DeleteNodes(CScene *self);
CNode3D *__thiscall CScene_NodeObject(CScene *self, int index);
/* the next node after from (the first without) of the class, any with class 0 */
CNode3D *__thiscall CScene_FindClass(CScene *self, CNode3D *from, uint32_t class);
CNode3D *__thiscall CScene_FindNode(CScene *self, const char *name);
/* box from the boxes of the nodes, merged into out (copied if first); an empty scene gets a zero box with first */
void __thiscall CScene_UpdateBox(CScene *self, box_t *out, byte first);
/* the skin weights of the vertex buffer meshes (calcular_pesos) */
void __fastcall CScene_CalcWeights(CScene *self);
int __thiscall CScene_CountClass(CScene *self, uint32_t class);
int __fastcall CScene_NumNodes(CScene *self);
/* convertir_a_D3D; the static lighting first when the ini says so */
void __thiscall CScene_ConvertD3D(CScene *self, uint32_t flags);
/* convertir_a_suelo: a floor node made of the meshes, the joined nodes deleted */
void __fastcall CScene_ToGround(CScene *self);
/* the nodes of the class (only NODE_DELETE ones with flagged) that nobody else holds deleted, repeated */
void __thiscall CScene_DeleteEmpty(CScene *self, uint32_t class, byte bRefs, byte flagged);
void __thiscall CScene_SetScale(CScene *self, const vec3_t *scale);
/* the meshes under each NODE_JOIN1 (sel) or NODE_JOIN2 node (with a name containing name) joined into it */
void __thiscall CScene_Join(CScene *self, byte sel, const char *name);
/* the alpha of the materials of the vertex buffer meshes, of all nodes except the one named name */
void __thiscall CScene_SetAlpha(CScene *self, float alpha, const char *name);
void __thiscall CScene_SetMarks(CScene *self, int v);
/* precalc_ilum of the meshes and the floors; scene lights unless noLights, visible skips NODE_HIDDEN */
void __thiscall CScene_PrecalcIllum(CScene *self, byte noLights, byte visible);

/* other modules (src/imports.inc) */
/* rva 0x239880, the box of box through m */
box_t *Box_Transform(box_t *out, const box_t *box, const matrix_t *m);
/* NodeFX_New (the FX object of a NODE_FX node): nodefx.h */
/* CAnimation_ctor, CAnimation_dtor: animation.h */
/* CalcWeights.c, CalcIllum.c and rva 0x261630: per mesh, with the scene lights of esc */
struct CMeshD3D;
byte __thiscall CMeshD3D_CalcWeights(struct CMeshD3D *mesh, CNode3D *node);
void __thiscall CMeshD3D_CalcIllum(struct CMeshD3D *mesh, CNode3D *node, CScene *scn);
void __thiscall CGround_CalcIllum(void *ground, CNode3D *node, CScene *scn);

#endif
