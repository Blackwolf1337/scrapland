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
	uint32_t nTr;			/* 0x0 frames */
	vec3_t *Tr;			/* 0x4 positions, until compressed */
	float scale;			/* 0x8 */
	int8_t *Tr2;			/* 0xc compressed */
} CAnimVert;

typedef struct CEVA {
	uint32_t nV;			/* 0x0 */
	CAnimVert *AnimV;		/* 0x4 new[] of nV */
} CEVA;

CAnimVert *CAnimVert_ctor(CAnimVert *self);
CEVA *CEVA_ctor(CEVA *self);
void CAnimVert_dtor(CAnimVert *self);
/* the scalar and vector deleting destructor: flags 1 delete, 2 an array */
void *CAnimVert_Delete(CAnimVert *self, uint32_t flags);
void CAnimVert_FreeTr(CAnimVert *self);
void CAnimVert_FreeTr2(CAnimVert *self);
void CAnimVert_AllocTr(CAnimVert *self, int frames);
void CAnimVert_AllocTr2(CAnimVert *self);
vec3_t *CAnimVert_Frame(CAnimVert *self, uint32_t frame);
/* the position between frame and the next one: replaced from Tr, added from Tr2 */
void CAnimVert_GetPos(CAnimVert *self, uint32_t frame, float subframe, vec3_t *pos);
/* compressed: the position between two frames added to pos */
void CAnimVert_AddPos(CAnimVert *self, uint32_t frame1, uint32_t frame2, float t, float *pos);
void CAnimVert_Scale(CAnimVert *self, float x, float y, float z);
/* Tr to Tr2 */
void CAnimVert_Compress(CAnimVert *self);
/* no animation if all frames are like the first one, else compressed */
void CAnimVert_Optimize(CAnimVert *self);
/* compressed from the file into *pAV (a flag first) */
void AnimVert_Read(CFileIO *f, CAnimVert **pAV);
void AnimVert_Write(CFileIO *f, CAnimVert *av);

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
