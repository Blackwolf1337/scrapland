/*
 * Engine/RenderSuelo.cpp of the engine3d: the meshes of the floor drawn in batches. The opaque meshes of a list are
 * collected (up to BATCH_MAX, then drawn) and drawn sorted by texture, material and depth of the box center; the
 * transparent ones go to the sorted list of the transparent meshes at once.
 */
#ifndef SCRAP_RENDERSUELO_H
#define SCRAP_RENDERSUELO_H

#include "scrap/mallad3d.h"
#include "scrap/tabla.h"

#define BATCH_MAX 0x200

typedef struct BatchMalla {
	CMallaD3D *malla;		/* 0x0 */
	uint32_t tex;			/* 0x4 the device texture of map 0, halved, 0 without */
	float z;			/* 0x8 the box center in view space */
} BatchMalla;
SD_SIZE(BatchMalla, 0xc);

#define g_BatchTab ((CTabla *)Game_Ptr(0x4e42e8))		/* BATCH_MAX BatchMalla, made on the first use */
#define g_BatchOrden ((BatchMalla **)Game_Ptr(0x4e42f0))	/* [BATCH_MAX] */
#define g_BatchUsados GAME_DATA(uint32_t, 0x4e4af0)

/* the meshes of the list malla (up to the first without vertices); dev is only passed on */
void Batch_AddMalla(void *malla, void *dev);
void Batch_Flush(void *dev);
/* qsort order of g_BatchOrden: texture, material, then z ascending */
int Batch_Compare(const void *a, const void *b);
/* rva 0x2b6cc0: a mesh with its matrix into the list of the transparent meshes */
int RenderTransp_Add(CMallaD3D *malla, const matrix_t *m, int a, int b);

#endif
