/*
 * Engine/animacion.cpp of the engine3d: keyframe animation. A CAnimation holds one CAnimNode per animated object
 * (table maps the object numbers of a model to the nodes, 1 based, 0 for none). A node has up to six tracks of
 * num_frames values from first_frame on, and a vertex animation (CEVA). A track in OptFlags holds a single value;
 * in StmFlags too it is a stream: ushort size in bytes, ushort first key frame (bit 15 unused), ushort keys, then the
 * keys, between which the values are interpolated. The tracks of a read animation point into its memory block
 * (OPT_INBLOCK) and are not freed one by one.
 */
#ifndef SCRAP_ANIMATION_H
#define SCRAP_ANIMATION_H

#include "scrap/gvar.h"
#include "scrap/animvert.h"
#include "scrap/table.h"

/* the tracks (flags of a node) */
#define ANIM_POS 0x1			/* vec3_t */
#define ANIM_ROT 0x2			/* quat_t */
#define ANIM_FOV 0x4			/* float */
#define ANIM_COLOR 0x8			/* 4 bytes, rgb used */
#define ANIM_INTENS 0x10		/* float */
#define ANIM_VISIBLE 0x80		/* byte */
#define ANIM_EVA 0x1000			/* the CEVA */
#define ANIM_TRACKS 6
/* OptFlags: the tracks are in the memory block of the animation */
#define OPT_INBLOCK 0x8000

typedef struct CAnimNode {
	uint16_t first_frame;		/* 0x00 */
	uint16_t num_frames;		/* 0x02 */
	uint16_t OptFlags;		/* 0x04 */
	uint16_t StmFlags;		/* 0x06 */
	vec3_t *Pos;			/* 0x08 */
	quat_t *Rot;			/* 0x0c */
	float *FOV;			/* 0x10 */
	byte *Color;			/* 0x14 */
	float *Intens;			/* 0x18 */
	byte *Visible;			/* 0x1c */
	CEVA *EVA;			/* 0x20 */
} CAnimNode;

typedef struct CAnimBlock {
	byte *MemBlock;			/* 0x0 */
	int SizeBlock;			/* 0x4 */
	int UsedBlock;			/* 0x8 */
} CAnimBlock;

typedef struct CAnimation {
	char *name;			/* 0x00 String_Set */
	int Refs;			/* 0x04 objref.h, 0 at the destructor */
	float fps;			/* 0x08 */
	int first_frame;		/* 0x0c */
	int num_frames;			/* 0x10 */
	byte flag;			/* 0x14 all nodes are ANIM_VERT (morph), read by AnimGroup.c */
	byte pad_15[3];
	CTable table;			/* 0x18 n bytes: node number + 1 of each object */
	CAnimNode *AnimNode;		/* 0x20 new[] of num_objects */
	int num_objects;		/* 0x24 */
	CAnimBlock AnimBlock;		/* 0x28 */
} CAnimation;

/* a stream while it is built */
typedef struct CAnimStream {
	byte *p;			/* 0x0 Mem_Realloc */
	int len;			/* 0x4 */
} CAnimStream;

/* the name of the node an animation is loaded for, in the log lines */
extern char g_AnimNodeName[256];

/* how a track gives frame: 0 none, 1 before the first key, 2 a key or between two, 3 after the last key, 4 one value */
int CAnimNode_TrackKind(CAnimNode *self, int frame, int unused, uint32_t flag);
void CAnimation_Read(CAnimation *self, CFileIO *f);
void CAnimation_Write(CAnimation *self, CFileIO *f);
/* frame - first_frame, CHECKs for a plain track */
int CAnimNode_FrameIndex(CAnimNode *self, int frame, const void *ptr, uint32_t flag);
/* drops the nodes without tracks at the end */
void CAnimation_Trim(CAnimation *self);
byte CAnimNode_Translate(CAnimNode *self, const vec3_t *d);
void CAnimation_Scale(CAnimation *self, const vec3_t *s);
/* the node of object i through the table, NULL for none */
CAnimNode *CAnimation_GetNodeTable(CAnimation *self, uint32_t i);
/* the tracks of flags new for nframes, the old ones and the EVA freed */
void CAnimNode_Init(CAnimNode *self, uint32_t nframes, uint32_t first, uint32_t flags);
/* numObjects new nodes, the old ones and the block freed */
void CAnimation_Init(CAnimation *self, int numObjects, int nframes, int first);
/* a new EVA of verts vertices */
void CAnimNode_SetEVA(CAnimNode *self, int verts);
CAnimNode *CAnimation_GetNode(CAnimation *self, uint32_t i);
CAnimVert *CAnimNode_GetVert(CAnimNode *self, uint32_t i);
byte CAnimNode_GetColor(CAnimNode *self, vec3_t *color, float *intens, int frame, float subframe);
byte CAnimNode_GetFOV(CAnimNode *self, float *fov, int frame, float subframe);
/* the ANIM_* tracks the node has */
uint32_t CAnimNode_GetFlags(CAnimNode *self);
byte CAnimNode_GetPosRot(CAnimNode *self, vec3_t *pos, quat_t *rot, int frame, float subframe);
byte CAnimNode_GetVisible(CAnimNode *self, byte *visible, int frame, float subframe);
/* the EVA optimized, deleted when no vertex moves */
void CAnimNode_OptimizeEVA(CAnimNode *self);
/* CHECK that node i has no tracks */
void CAnimation_SwapNodes(CAnimation *self, uint32_t i, uint32_t j);

CAnimation *CAnimation_ctor(CAnimation *self);
void CAnimation_dtor(CAnimation *self);

/* the code of the exe this module calls */

#endif
