/*
 * Engine/RenderSuelo.cpp of the engine3d: the meshes of the floor drawn in batches. The opaque meshes of a list are
 * collected (up to BATCH_MAX, then drawn) and drawn sorted by texture, material and depth of the box center; the
 * transparent ones go to the sorted list of the transparent meshes at once.
 */
#ifndef SCRAP_RENDERGROUND_H
#define SCRAP_RENDERGROUND_H

#include "scrap/meshd3d.h"
#include "scrap/table.h"

#define BATCH_MAX 0x200

typedef struct BatchMesh {
	CMeshD3D *mesh;		/* 0x0 */
	uint32_t tex;			/* 0x4 the device texture of map 0, halved, 0 without */
	float z;			/* 0x8 the box center in view space */
} BatchMesh;
SD_SIZE(BatchMesh, 0xc);

#define g_BatchTab (GAME_ARRAY(CTable, g_BatchTab, 0x4e42e8))		/* BATCH_MAX BatchMesh, made on the first use */
#define g_BatchOrder (GAME_ARRAY(BatchMesh *, g_BatchOrder, 0x4e42f0))	/* [BATCH_MAX] */
#define g_BatchUsed GAME_GLOBAL(uint32_t, g_BatchUsed, 0x4e4af0)

/* the meshes of the list mesh (up to the first without vertices); dev is only passed on */
void Batch_AddMesh(void *mesh, void *dev);
void Batch_Flush(void *dev);
/* qsort order of g_BatchOrder: texture, material, then z ascending */
int Batch_Compare(const void *a, const void *b);
/* RenderMeshD3D_AddTransp (a mesh into the list of the transparent meshes): render.h */

#endif
