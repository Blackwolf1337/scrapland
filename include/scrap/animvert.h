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
	float esc;			/* 0x8 */
	int8_t *Tr2;			/* 0xc compressed */
} CAnimVert;
SD_SIZE(CAnimVert, 0x10);

typedef struct CEVA {
	uint32_t nV;			/* 0x0 */
	CAnimVert *AnimV;		/* 0x4 new[] of nV */
} CEVA;
SD_SIZE(CEVA, 0x8);

CAnimVert *__fastcall CAnimVert_ctor(CAnimVert *self);
CEVA *__thiscall CEVA_ctor(CEVA *self);
void __fastcall CAnimVert_dtor(CAnimVert *self);
/* the scalar and vector deleting destructor: flags 1 delete, 2 an array */
void *__thiscall CAnimVert_Delete(CAnimVert *self, uint32_t flags);
void __fastcall CAnimVert_FreeTr(CAnimVert *self);
void __fastcall CAnimVert_FreeTr2(CAnimVert *self);
void __thiscall CAnimVert_AllocTr(CAnimVert *self, int frames);
void __fastcall CAnimVert_AllocTr2(CAnimVert *self);
vec3_t *__thiscall CAnimVert_Frame(CAnimVert *self, uint32_t frame);
/* the position between frame and the next one: replaced from Tr, added from Tr2 */
void __thiscall CAnimVert_GetPos(CAnimVert *self, uint32_t frame, float subframe, vec3_t *pos);
/* compressed: the position between two frames added to pos */
void __thiscall CAnimVert_AddPos(CAnimVert *self, uint32_t frame1, uint32_t frame2, float t, float *pos);
void __thiscall CAnimVert_Scale(CAnimVert *self, float x, float y, float z);
/* Tr to Tr2 */
void __fastcall CAnimVert_Compress(CAnimVert *self);
/* no animation if all frames are like the first one, else compressed */
void __fastcall CAnimVert_Optimize(CAnimVert *self);
/* compressed from the file into *pAV (a flag first) */
void AnimVert_Read(CArchIO *f, CAnimVert **pAV);
void AnimVert_Write(CArchIO *f, CAnimVert *av);

void __fastcall CEVA_Free(CEVA *self);
void __thiscall CEVA_Alloc(CEVA *self, int verts);
CAnimVert *__thiscall CEVA_Get(CEVA *self, uint32_t i);
void __thiscall CEVA_Scale(CEVA *self, float x, float y, float z);
/* the bytes of the set */
int __fastcall CEVA_MemSize(CEVA *self);
/* the vertices with an animation */
int __fastcall CEVA_NumAnimated(CEVA *self);
/* each vertex optimized; the set is emptied when none is animated */
void __fastcall CEVA_Optimize(CEVA *self);
/* the "EVA" block into a new set if flag */
void EVA_Read(CArchIO *f, int flag, CEVA **pEVA);
void EVA_Write(CArchIO *f, int flag, CEVA *eva);

#endif
