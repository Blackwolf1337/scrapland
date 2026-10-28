/*
 * Engine/escena.cpp of the engine3d: the scene (CEscena, the struct is in nodo3d.h), a list of nodes with their
 * materials, an animation (AnimM3D) and the user properties. It is read and written as the "SCN" block, converted
 * to vertex buffer meshes (convertir_a_D3D), to a floor (convertir_a_suelo) and lit (precalc_ilum).
 */
#ifndef SCRAP_ESCENA_H
#define SCRAP_ESCENA_H

#include "scrap/nodo3d.h"

#define CESCENA_VFTABLE GAME_VFTABLE(CEscena_vftable, 0x3d9b24)

/* node flags read here */
#define NODO_JOIN1 (1u << 0)		/* CEscena_Join with sel */
#define NODO_JOIN2 (1u << 1)		/* CEscena_Join without sel */
#define NODO_BORRAR (1u << 3)		/* joined into another node: deleted by CEscena_DeleteEmpty */
#define NODO_COLLIDE (1u << 7)		/* cleared on the node that receives a join */
#define NODO_HUESO (1u << 20)		/* a bone, never deleted */
#define NODO_OCULTO (1u << 25)		/* skipped by CEscena_PrecalcIlum with visible */

/* the messages of CEscena_Msg */
enum {
	ESCENA_MSG_JOIN = 0,		/* a: sel, b: !sel; nothing without both */
	ESCENA_MSG_CONVERTD3D = 1,	/* a: flags */
	ESCENA_MSG_CALCPESOS = 2,
	ESCENA_MSG_CAMARAS = 3		/* a, b: the floats at 0x2c and 0x30 of the cameras, 0 keeps them */
};


CEscena *__fastcall CEscena_ctor(CEscena *self);
void __fastcall CEscena_dtor(CEscena *self);
CEscena *__thiscall CEscena_Delete(CEscena *self, byte flags);
void __thiscall CEscena_Msg(CEscena *self, int msg, int a, int b, int c, int d);
/* OBJ3D_VF_RELEASED3D and OBJ3D_VF_INITD3D of the objects of all nodes */
void __thiscall CEscena_ReleaseD3D(CEscena *self, void *dev);
void __thiscall CEscena_InitD3D(CEscena *self, void *dev);
/* rva 0x24ecb0, empty */
void __thiscall CEscena_Shared(CEscena *self);
/* the "SCN" block; bAnim: the animation must be there (read) or is all there is (write) */
void __thiscall CEscena_Read(CEscena *self, CArchIO *f, byte bAnim);
void __thiscall CEscena_Write(CEscena *self, CArchIO *f, byte bAnim);
/* all nodes (those with the class bits 0xe0 last), or the render nodes of the model of ctx */
void __thiscall CEscena_Render(CEscena *self, NodoAnimCtx *ctx);
void __thiscall CEscena_Update(CEscena *self, NodoAnimCtx *ctx);
void __fastcall CEscena_UpdateNodes(CEscena *self);
/* nodo, unlinked, at the end of the list */
void __thiscall CEscena_AddNodo(CEscena *self, CNodo3D *nodo);
void __thiscall CEscena_RemoveNodo(CEscena *self, CNodo3D *nodo);
/* the materials without atrib 4 released, the others moved to the front */
void __fastcall CEscena_CompactMats(CEscena *self);
void __fastcall CEscena_DeleteNodes(CEscena *self);
CNodo3D *__thiscall CEscena_NodoObjeto(CEscena *self, int indice);
/* the next node after from (the first without) of the class, any with clase 0 */
CNodo3D *__thiscall CEscena_FindClase(CEscena *self, CNodo3D *from, uint32_t clase);
CNodo3D *__thiscall CEscena_FindNodo(CEscena *self, const char *name);
/* box from the boxes of the nodes, merged into out (copied if first); an empty scene gets a zero box with first */
void __thiscall CEscena_UpdateBox(CEscena *self, box_t *out, byte first);
/* the skin weights of the vertex buffer meshes (calcular_pesos) */
void __fastcall CEscena_CalcPesos(CEscena *self);
int __thiscall CEscena_CountClase(CEscena *self, uint32_t clase);
int __fastcall CEscena_NumNodos(CEscena *self);
/* convertir_a_D3D; the static lighting first when the ini says so */
void __thiscall CEscena_ConvertD3D(CEscena *self, uint32_t flags);
/* convertir_a_suelo: a floor node made of the meshes, the joined nodes deleted */
void __fastcall CEscena_ToSuelo(CEscena *self);
/* the nodes of the class (only NODO_BORRAR ones with flagged) that nobody else holds deleted, repeated */
void __thiscall CEscena_DeleteEmpty(CEscena *self, uint32_t clase, byte bRefs, byte flagged);
void __thiscall CEscena_SetEscala(CEscena *self, const vec3_t *escala);
/* the meshes under each NODO_JOIN1 (sel) or NODO_JOIN2 node (with a name containing name) joined into it */
void __thiscall CEscena_Join(CEscena *self, byte sel, const char *name);
/* the alpha of the materials of the vertex buffer meshes, of all nodes except the one named name */
void __thiscall CEscena_SetAlpha(CEscena *self, float alpha, const char *name);
void __thiscall CEscena_SetNodos54(CEscena *self, int v);
/* precalc_ilum of the meshes and the floors; scene lights unless noLuces, visible skips NODO_OCULTO */
void __thiscall CEscena_PrecalcIlum(CEscena *self, byte noLuces, byte visible);

/* other modules (src/imports.inc) */
/* rva 0x239880, the box of box through m */
box_t *Box_Transform(box_t *out, const box_t *box, const matrix_t *m);
/* NodeFX_New (the FX object of a NODO_FX node): nodefx.h */
/* CAnimacion_ctor, CAnimacion_dtor: animacion.h */
/* CalcPesos.c, CalcIlum.c and rva 0x261630: per mesh, with the scene lights of esc */
struct CMallaD3D;
byte __thiscall CMallaD3D_CalcPesos(struct CMallaD3D *malla, CNodo3D *nodo);
void __thiscall CMallaD3D_CalcIlum(struct CMallaD3D *malla, CNodo3D *nodo, CEscena *esc);
void __thiscall CSuelo_CalcIlum(void *suelo, CNodo3D *nodo, CEscena *esc);

#endif
