/*
 * Engine/AnimVert.cpp of the engine3d: vertex animation. A CAnimVert holds the positions of one vertex in each
 * frame; compressed they are 3 signed bytes a frame, the offset to frame 0 in steps of esc, and added to the
 * position of the mesh. A CEVA ("EVA" block of the model files) is the set of a mesh.
 */
#ifndef SCRAP_ANIMVERT_H
#define SCRAP_ANIMVERT_H

#include "scrap/archio.h"
#include "scrap/mathlib.h"

typedef struct CAnimVert {
	uint32_t nTr;			/* frames */
	vec3_t *Tr;			/* positions, until compressed */
	float scale;
	int8_t *Tr2;			/* compressed */
} CAnimVert;

typedef struct CEVA {
	uint32_t nV;
	CAnimVert *AnimV;		/* new[] of nV */
} CEVA;

CEVA *CEVA_ctor(CEVA *self);
void CAnimVert_AllocTr(CAnimVert *self, int frames);
vec3_t *CAnimVert_Frame(CAnimVert *self, uint32_t frame);
/* compressed: the position between two frames added to pos */
void CAnimVert_AddPos(CAnimVert *self, uint32_t frame1, uint32_t frame2, float t, float *pos);

void CEVA_Free(CEVA *self);
void CEVA_Alloc(CEVA *self, int verts);
CAnimVert *CEVA_Get(CEVA *self, uint32_t i);
void CEVA_Scale(CEVA *self, float x, float y, float z);
/* the bytes of the set */
int CEVA_MemSize(CEVA *self);
/* the vertices with an animation */
int CEVA_NumAnimated(CEVA *self);
/* each vertex optimized; the set is emptied when none is animated */
void CEVA_Optimize(CEVA *self);
/* the "EVA" block into a new set if flag */
void EVA_Read(CFileIO *f, int flag, CEVA **pEVA);
void EVA_Write(CFileIO *f, int flag, CEVA *eva);

#endif
