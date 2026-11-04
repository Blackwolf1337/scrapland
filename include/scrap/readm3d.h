/*
 * Engine/lee_M3D.cpp of the engine3d, the reader of the M3D models and scenes. The files are text (read with
 * include/scrap/readdata.h, "Label: value" lines) or binary with the same order of values; a binary file may hold the
 * labels too, as strings. Binary ints are variable length: 6 bits and a sign bit in the first byte, 7 more bits in
 * each following byte while bit 7 is set.
 *
 * The game data has no M3D files, only their binary caches (.CM3, .SM3, read through CScene_Read), so the reader of
 * the M3D files themselves (Scene_Load, model.h) only gets past the .CM3 of an animation in the tools.
 */
#ifndef SCRAP_READM3D_H
#define SCRAP_READM3D_H

#include "scrap/gvar.h"
#include "scrap/game.h"
#include "scrap/material.h"
#include "scrap/mathlib.h"
#include "scrap/node3d.h"
#include "scrap/table.h"

#define M3D_STRING 0x400

/* the scene of the model while an animation of it (lee_M3D_Estatica) or a LOD model of it is read */
extern CScene *g_SceneAnimParent;
extern CScene *g_SceneLODParent;
/* the binary caches of the M3D files: .CM3 of the animations, .SM3 of the models, with the version and the
 * times of the M3D and the model (CM3) or the .INI (SM3) */
#define M3D_CACHE_VER 0x6515f8

/* the materials of the file: each with its material and sub material index of the file, used by a face */
#define M3D_MAX_MAT 0x200
#define M3D_MAX_MAT_SCENE 0x100
extern int g_UseTextures;		/* video UsarTexturas: without, maps of b8 1 become colors */

/* an animation table of the file: one value a frame (CTable of int or float) */
typedef struct M3DTable {
	int n;
	void *p;
	byte fixed;			/* one run over all frames: constant */
	byte pad9[3];
} M3DTable;

/* the tables of a vector or rotation: x, then z and y (the files are z up), then w or the angle */
typedef struct M3DTable4 {
	M3DTable t[4];
} M3DTable4;

/* the animation of a node: PosX..Z, Sgn, QRotX..A, URotX..A, EscX..Z; the rotations turned into quaternions */
typedef struct M3DTrack {
	M3DTable4 pos;
	M3DTable sgn;
	M3DTable4 rot;
	M3DTable4 rotScale;
	M3DTable4 scale;
} M3DTrack;

/* the Physique weights of a vertex */
#define SKIN_MAX_BONES 3
typedef struct M3DSkinVert {
	int NumBones;
	char *bone[SKIN_MAX_BONES];	/* Cad */
	float weight[SKIN_MAX_BONES];
} M3DSkinVert;

/* a transform of the scene nodes ("MT:" blocks) */
typedef struct m3dTransform_t {
	vec3_t pos;
	quat_t rot;
	quat_t rotScale;			/* the axes of the scale */
	vec3_t scale;
	float sign;
} m3dTransform_t;

/* what the reader keeps of each object of the file (CNode3D index) */
typedef struct M3DNode {
	m3dTransform_t mt;		/* the transform of the object */
	m3dTransform_t mt2;
	matrix_t mNode;
	matrix_t mPivot;		/* "local_a_pivote" */
	char *target;			/* Cad: the target of a camera or light, side 1 of a portal */
	char *side2;			/* Cad: side 2 of a portal */
	struct CAnimNode *AnimNode;
	CTable skin;			/* M3DSkinVert of each vertex */
	M3DTrack track;
	M3DTrack track2;
	M3DTable t2a4[6];
} M3DNode;

/* the scene of the .SM3 of name, NULL without a valid one or with SM3_UseScenes not 1 */
CScene *SM3_Load(const char *name);
void SM3_Save(const char *name, CScene *scn);
M3DSkinVert *M3DSkinVert_ctor(M3DSkinVert *self);
void M3DSkinVert_dtor(M3DSkinVert *self);

/* other modules: Engine/descomp_afin.cpp (src/game/engine3d/Engine/AffineDecomp.c) */
/* the transform of a matrix */
void Matrix_ToTransform(const matrix_t *m, m3dTransform_t *out);
/* and 0x2d65d0: a transform as a matrix (the pivots of the meshes, the frames of the animation) */
void M3DTransform_ToMatrix(m3dTransform_t *self, matrix_t *out);
matrix_t *M3DTransform_ToMatrixAnim(m3dTransform_t *self, matrix_t *out);
/* (ScrapEngMain.cpp): the node tree of the scene in the log */
void Scene_ShowTree(CScene *scn, CNode3D *parent, int indent);

#endif
