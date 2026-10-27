/*
 * Engine/animacion.cpp of the engine3d: keyframe animation. A CAnimacion holds one CAnimNodo per animated object
 * (tabla maps the object numbers of a model to the nodes, 1 based, 0 for none). A node has up to six tracks of
 * num_frames values from primer_frame on, and a vertex animation (CEVA). A track in OptFlags holds a single value;
 * in StmFlags too it is a stream: ushort size in bytes, ushort first key frame (bit 15 unused), ushort keys, then the
 * keys, between which the values are interpolated. The tracks of a read animation point into its memory block
 * (OPT_INBLOCK) and are not freed one by one.
 */
#ifndef SCRAP_ANIMACION_H
#define SCRAP_ANIMACION_H

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

typedef struct CAnimNodo {
	uint16_t primer_frame;		/* 0x00 */
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
} CAnimNodo;
SD_SIZE(CAnimNodo, 0x24);

typedef struct CAnimBlock {
	byte *MemBlock;			/* 0x0 */
	int SizeBlock;			/* 0x4 */
	int UsedBlock;			/* 0x8 */
} CAnimBlock;
SD_SIZE(CAnimBlock, 0xc);

typedef struct CAnimacion {
	const char *name;		/* 0x00 */
	int pad_4;
	float fps;			/* 0x08 */
	int primer_frame;		/* 0x0c */
	int num_frames;			/* 0x10 */
	byte flag;			/* 0x14 all nodes are ANIM_VERT (morph), read by AnimGrupo.c */
	byte pad_15[3];
	CTabla tabla;			/* 0x18 n bytes: node number + 1 of each object */
	CAnimNodo *AnimNodo;		/* 0x20 new[] of num_objetos */
	int num_objetos;		/* 0x24 */
	CAnimBlock AnimBlock;		/* 0x28 */
} CAnimacion;
SD_OFFSET(CAnimacion, tabla, 0x18);
SD_OFFSET(CAnimacion, AnimBlock, 0x28);
SD_SIZE(CAnimacion, 0x34);

/* a stream while it is built */
typedef struct CAnimStream {
	byte *p;			/* 0x0 Mem_Realloc */
	int len;			/* 0x4 */
} CAnimStream;
SD_SIZE(CAnimStream, 0x8);

/* the block the tracks of the nodes are read into, set while an animation reads or deletes its nodes */
#define g_ActAnimBlock GAME_DATA(CAnimBlock *, 0x4dc090)
/* the engine variables of the animations (the values of their cGVar objects): R_AnimNodo_Count the live CAnimNodo
 * objects; OptimizeAnims (0 none, 1 compact the tracks before writing) and the errors that still count as equal */
#define g_AnimNodoCount GAME_DATA(int, 0x4dc104)
#define g_OptimizeAnims GAME_DATA(int, 0x4dc0d4)
#define g_OptAnimPosErr GAME_DATA(float, 0x4dc0bc)
#define g_OptAnimRotErr GAME_DATA(float, 0x4dc11c)	/* degrees */
#define g_OptAnimFovErr GAME_DATA(float, 0x4dc0ec)	/* degrees */
/* the name of the node an animation is loaded for, in the log lines */
#define g_AnimNodoName ((char *)Game_Ptr(0x4dbf78))
/* the memory statistics of the animations */
#define g_AnimMemStats (&GAME_DATA(cMemStats, 0x4dc078))

/* a track: Mem_Free unless it is in the active block; CHECK that it is when flags has OPT_INBLOCK */
void AnimBlock_Free(void *ptr, uint32_t flags);
/* how a track gives frame: 0 none, 1 before the first key, 2 a key or between two, 3 after the last key, 4 one value */
int __thiscall CAnimNodo_TrackKind(CAnimNodo *self, int frame, int unused, uint32_t flag);
void __thiscall CAnimacion_Read(CAnimacion *self, CArchIO *f);
void __thiscall CAnimNodo_Read(CAnimNodo *self, CArchIO *f);
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
void __thiscall CAnimNodo_ReadTrack(CAnimNodo *self, CArchIO *f, uint32_t flag, void **ptr, int size);
void __thiscall CAnimacion_Write(CAnimacion *self, CArchIO *f);
/* the size of a stream (header) or the data of a track */
void __thiscall CAnimNodo_WriteTrack(CAnimNodo *self, CArchIO *f, uint32_t flag, const void *ptr, int size, byte header);
void __thiscall CAnimNodo_WriteData(CAnimNodo *self, CArchIO *f);
void __thiscall CAnimNodo_WriteHeader(CAnimNodo *self, CArchIO *f);
/* b - a, each folded once into -pi..pi */
vec3_t *Vec3_AngDiff(vec3_t *out, const vec3_t *a, const vec3_t *b);
/* frame - primer_frame, CHECKs for a plain track */
int __thiscall CAnimNodo_FrameIndex(CAnimNodo *self, int frame, const void *ptr, uint32_t flag);
/* drops the nodes without tracks at the end */
void __fastcall CAnimacion_Trim(CAnimacion *self);
byte __thiscall CAnimNodo_Translate(CAnimNodo *self, const vec3_t *d);
/* CHECK(0) first: the tracks of flags (pos, rot) freed */
void __thiscall CAnimNodo_FreeTracks(CAnimNodo *self, uint32_t flags);
/* each component equal or of the same sign with a ratio within 1 +- 2^-22 */
byte Quat_Similar(const quat_t *a, const quat_t *b);
void __thiscall CAnimNodo_Scale(CAnimNodo *self, const vec3_t *s);
void __thiscall CAnimacion_Scale(CAnimacion *self, const vec3_t *s);
/* the node of object i through the tabla, NULL for none */
CAnimNodo *__thiscall CAnimacion_GetNodoTabla(CAnimacion *self, uint32_t i);
/* the tracks of flags new for nframes, the old ones and the EVA freed */
void __thiscall CAnimNodo_Init(CAnimNodo *self, uint32_t nframes, uint32_t first, uint32_t flags);
/* nobjetos new nodes, the old ones and the block freed */
void __thiscall CAnimacion_Init(CAnimacion *self, int nobjetos, int nframes, int first);
/* a new EVA of verts vertices */
void __thiscall CAnimNodo_SetEVA(CAnimNodo *self, int verts);
CAnimNodo *__thiscall CAnimacion_GetNodo(CAnimacion *self, uint32_t i);
CAnimVert *__thiscall CAnimNodo_GetVert(CAnimNodo *self, uint32_t i);
byte __thiscall CAnimNodo_GetColor(CAnimNodo *self, vec3_t *color, float *intens, int frame, float subframe);
byte __thiscall CAnimNodo_GetFOV(CAnimNodo *self, float *fov, int frame, float subframe);
/* the ANIM_* tracks the node has */
uint32_t __fastcall CAnimNodo_GetFlags(CAnimNodo *self);
/* the values of a frame, plain tracks only */
byte __thiscall CAnimNodo_GetPosRotFrame(CAnimNodo *self, vec3_t *pos, quat_t *rot, int frame);
byte __thiscall CAnimNodo_GetPosRot(CAnimNodo *self, vec3_t *pos, quat_t *rot, int frame, float subframe);
byte __thiscall CAnimNodo_GetVisible(CAnimNodo *self, byte *visible, int frame, float subframe);
/* the EVA optimized, deleted when no vertex moves */
void __fastcall CAnimNodo_OptimizeEVA(CAnimNodo *self);
/* CHECK that node i has no tracks */
void __thiscall CAnimacion_SwapNodos(CAnimacion *self, uint32_t i, uint32_t j);
/* before writing, with OptimizeAnims: pos, rot and fov tracks that do not change become one value, those that change
 * only in the middle a stream of the middle keys */
void __fastcall CAnimNodo_Optimize(CAnimNodo *self);

/* the code of the exe this module calls */

#endif
