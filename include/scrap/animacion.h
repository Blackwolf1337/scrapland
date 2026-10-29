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

#include "scrap/animvert.h"
#include "scrap/tabla.h"

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
SD_SIZE(CAnimNode, 0x24);

typedef struct CAnimBlock {
	byte *MemBlock;			/* 0x0 */
	int SizeBlock;			/* 0x4 */
	int UsedBlock;			/* 0x8 */
} CAnimBlock;
SD_SIZE(CAnimBlock, 0xc);

typedef struct CAnimation {
	char *name;			/* 0x00 String_Set */
	int Refs;			/* 0x04 objref.h, 0 at the destructor */
	float fps;			/* 0x08 */
	int first_frame;		/* 0x0c */
	int num_frames;			/* 0x10 */
	byte flag;			/* 0x14 all nodes are ANIM_VERT (morph), read by AnimGrupo.c */
	byte pad_15[3];
	CTable table;			/* 0x18 n bytes: node number + 1 of each object */
	CAnimNode *AnimNode;		/* 0x20 new[] of num_objects */
	int num_objects;		/* 0x24 */
	CAnimBlock AnimBlock;		/* 0x28 */
} CAnimation;
SD_OFFSET(CAnimation, table, 0x18);
SD_OFFSET(CAnimation, AnimBlock, 0x28);
SD_SIZE(CAnimation, 0x34);

/* a stream while it is built */
typedef struct CAnimStream {
	byte *p;			/* 0x0 Mem_Realloc */
	int len;			/* 0x4 */
} CAnimStream;
SD_SIZE(CAnimStream, 0x8);

/* the block the tracks of the nodes are read into, set while an animation reads or deletes its nodes */
#define g_ActAnimBlock GAME_GLOBAL(CAnimBlock *, g_ActAnimBlock, 0x4dc090)
/* the engine variables of the animations (the values of their cGVar objects): R_AnimNodo_Count the live CAnimNode
 * objects; OptimizeAnims (0 none, 1 compact the tracks before writing) and the errors that still count as equal */
#define g_AnimNodeCount GAME_GLOBAL_IN(int, GVar_R_AnimNode_Count, 16, 0x4dc104)
#define g_OptimizeAnims GAME_GLOBAL_IN(int, GVar_OptimizeAnims, 16, 0x4dc0d4)
#define g_OptAnimPosErr GAME_GLOBAL_IN(float, GVar_OptAnimPosErr, 16, 0x4dc0bc)
#define g_OptAnimRotErr GAME_GLOBAL_IN(float, GVar_OptAnimRotErr, 16, 0x4dc11c)	/* degrees */
#define g_OptAnimFovErr GAME_GLOBAL_IN(float, GVar_OptAnimFovErr, 16, 0x4dc0ec)	/* degrees */
/* the name of the node an animation is loaded for, in the log lines */
#define g_AnimNodeName (GAME_ARRAY(char, g_AnimNodeName, 0x4dbf78))
/* the memory statistics of the animations */
#define g_AnimMemStats (&GAME_GLOBAL(cMemStats, g_AnimMemStats, 0x4dc078))

/* a track: Mem_Free unless it is in the active block; CHECK that it is when flags has OPT_INBLOCK */
void AnimBlock_Free(void *ptr, uint32_t flags);
/* how a track gives frame: 0 none, 1 before the first key, 2 a key or between two, 3 after the last key, 4 one value */
int __thiscall CAnimNode_TrackKind(CAnimNode *self, int frame, int unused, uint32_t flag);
void __thiscall CAnimation_Read(CAnimation *self, CFileIO *f);
void __thiscall CAnimNode_Read(CAnimNode *self, CFileIO *f);
/* x and y times 2 in 11 bits each from 1024 on, z in 10 bits from 512 on */
uint32_t Vec3_Pack(const vec3_t *v);
void __thiscall AnimStream_AddByte(CAnimStream *self, int data);
void __thiscall AnimStream_AddBytes(CAnimStream *self, const void *data, int n);
/* the header of a stream: start twice (the first is the size later), then the number of keys */
void __thiscall AnimStream_AddHeader(CAnimStream *self, int start, int keys);
void __thiscall AnimStream_AddShort(CAnimStream *self, int data);
/* the negated euler angles of a quaternion */
vec3_t *Quat_ToEulerNeg(vec3_t *out, const quat_t *q);
/* the place of a track in the active block; size is read from the file for a stream */
void __thiscall CAnimNode_ReadTrack(CAnimNode *self, CFileIO *f, uint32_t flag, void **ptr, int size);
void __thiscall CAnimation_Write(CAnimation *self, CFileIO *f);
/* the size of a stream (header) or the data of a track */
void __thiscall CAnimNode_WriteTrack(CAnimNode *self, CFileIO *f, uint32_t flag, const void *ptr, int size, byte header);
void __thiscall CAnimNode_WriteData(CAnimNode *self, CFileIO *f);
void __thiscall CAnimNode_WriteHeader(CAnimNode *self, CFileIO *f);
/* b - a, each folded once into -pi..pi */
vec3_t *Vec3_AngDiff(vec3_t *out, const vec3_t *a, const vec3_t *b);
/* frame - first_frame, CHECKs for a plain track */
int __thiscall CAnimNode_FrameIndex(CAnimNode *self, int frame, const void *ptr, uint32_t flag);
/* drops the nodes without tracks at the end */
void __fastcall CAnimation_Trim(CAnimation *self);
byte __thiscall CAnimNode_Translate(CAnimNode *self, const vec3_t *d);
/* CHECK(0) first: the tracks of flags (pos, rot) freed */
void __thiscall CAnimNode_FreeTracks(CAnimNode *self, uint32_t flags);
/* each component equal or of the same sign with a ratio within 1 +- 2^-22 */
byte Quat_Similar(const quat_t *a, const quat_t *b);
void __thiscall CAnimNode_Scale(CAnimNode *self, const vec3_t *s);
void __thiscall CAnimation_Scale(CAnimation *self, const vec3_t *s);
/* the node of object i through the table, NULL for none */
CAnimNode *__thiscall CAnimation_GetNodeTable(CAnimation *self, uint32_t i);
/* the tracks of flags new for nframes, the old ones and the EVA freed */
void __thiscall CAnimNode_Init(CAnimNode *self, uint32_t nframes, uint32_t first, uint32_t flags);
/* numObjects new nodes, the old ones and the block freed */
void __thiscall CAnimation_Init(CAnimation *self, int numObjects, int nframes, int first);
/* a new EVA of verts vertices */
void __thiscall CAnimNode_SetEVA(CAnimNode *self, int verts);
CAnimNode *__thiscall CAnimation_GetNode(CAnimation *self, uint32_t i);
CAnimVert *__thiscall CAnimNode_GetVert(CAnimNode *self, uint32_t i);
byte __thiscall CAnimNode_GetColor(CAnimNode *self, vec3_t *color, float *intens, int frame, float subframe);
byte __thiscall CAnimNode_GetFOV(CAnimNode *self, float *fov, int frame, float subframe);
/* the ANIM_* tracks the node has */
uint32_t __fastcall CAnimNode_GetFlags(CAnimNode *self);
/* the values of a frame, plain tracks only */
byte __thiscall CAnimNode_GetPosRotFrame(CAnimNode *self, vec3_t *pos, quat_t *rot, int frame);
byte __thiscall CAnimNode_GetPosRot(CAnimNode *self, vec3_t *pos, quat_t *rot, int frame, float subframe);
byte __thiscall CAnimNode_GetVisible(CAnimNode *self, byte *visible, int frame, float subframe);
/* the EVA optimized, deleted when no vertex moves */
void __fastcall CAnimNode_OptimizeEVA(CAnimNode *self);
/* CHECK that node i has no tracks */
void __thiscall CAnimation_SwapNodes(CAnimation *self, uint32_t i, uint32_t j);
/* before writing, with OptimizeAnims: pos, rot and fov tracks that do not change become one value, those that change
 * only in the middle a stream of the middle keys */
void __fastcall CAnimNode_Optimize(CAnimNode *self);

/* the constructors and destructors (inline in the headers of the original) */
CAnimNode *__fastcall CAnimNode_ctor(CAnimNode *self);
/* with pos and rot tracks of nframes */
CAnimNode *__thiscall CAnimNode_ctorFrames(CAnimNode *self, uint32_t nframes, uint32_t first);
void __fastcall CAnimNode_dtor(CAnimNode *self);
/* the vector deleting destructor (flags 2: an array of the count before it) */
CAnimNode *__thiscall CAnimNode_Delete(CAnimNode *self, uint32_t flags);
CAnimation *__fastcall CAnimation_ctor(CAnimation *self);
void __fastcall CAnimation_dtor(CAnimation *self);
void __fastcall AnimStream_dtor(CAnimStream *self);
/* CHECK(!MemBlock) */
void __fastcall AnimBlock_dtor(CAnimBlock *self);

/* the code of the exe this module calls */

#endif
