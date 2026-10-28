/*
 * Trails of the engine3d (original source Engine/trails.cpp): ribbons behind moving objects. A trail keeps a ring of
 * up to m_wMaxTrails points (position, color with the alpha inverted, width); the newest is at head - 1. An active
 * trail sits in gTrailsRender[texture] and is turned into geometry once per frame (Trails_Render): one dynamic
 * vertex and index buffer for all trails, filled per texture, flushed when full.
 */
#ifndef SCRAP_TRAILS_H
#define SCRAP_TRAILS_H

#include "scrap/game.h"

#define MAX_TRAILS_TEXTURES 17
#define MAX_TRAILS_RENDER 256
#define TRAILS_MAXSIZE 32
#define MAX_TRAILS_VB 0x1000
#define MAX_TRAILS_IB 0x1800

/* CTrail.type: how the points become geometry */
enum {
	TRAIL_RIBBON,			/* a band facing the eye, the width scaled from tail to head */
	TRAIL_FAN,			/* a fan from the newest point, uv 0.5 */
	TRAIL_CROSS,			/* two bands turned by +-45 degrees */
	TRAIL_QUADS			/* one band piece for each pair of points */
};

/* CTrail.orient: where the bands face */
enum {
	TRAIL_ORIENT_EYE,		/* the camera position (Render_GetEyePos) */
	TRAIL_ORIENT_POINT,		/* orientVec as a point */
	TRAIL_ORIENT_DIR		/* orientVec normalized as the normal itself */
};

typedef struct TrailPoint {
	vec3_t pos;			/* 0x00 */
	uint32_t color;			/* 0x0c the alpha byte is 255 - alpha */
	float width;			/* 0x10 */
} TrailPoint;
SD_SIZE(TrailPoint, 0x14);

/* FVF 0x1c2: XYZ, DIFFUSE, SPECULAR, TEX1 */
typedef struct TrailVertex {
	vec3_t pos;			/* 0x00 */
	uint32_t color;			/* 0x0c */
	uint32_t specular;		/* 0x10 always 0 */
	float u, v;			/* 0x14 */
} TrailVertex;
SD_SIZE(TrailVertex, 0x1c);

typedef struct CTrail {
	byte render;			/* 0x00 in gTrailsRender (Trails_UpdateRender) */
	byte pad_1[3];
	int IndiceRender;		/* 0x04 the slot in gTrailsRender[texture], -1 */
	int texture;			/* 0x08 the material, < MAX_TRAILS_TEXTURES */
	byte fade;			/* 0x0c alpha from 0 at the tail to 255 at the head */
	byte smooth;			/* 0x0d the newest piece of a ribbon in two halves */
	byte premul;			/* 0x0e color multiplied by the alpha */
	byte transp;			/* 0x0f 255 is invisible */
	uint32_t color;			/* 0x10 of AddPos */
	float width;			/* 0x14 of AddPos and AddPosColor */
	float scaleHead;		/* 0x18 */
	float scaleTail;		/* 0x1c */
	byte m_wMaxTrails;		/* 0x20 */
	byte numTrails;			/* 0x21 at most m_wMaxTrails - 1 */
	byte head;			/* 0x22 the next point to write */
	byte segments;			/* 0x23 the points drawn, <= m_wMaxTrails */
	byte last;			/* 0x24 the point written last */
	byte type;			/* 0x25 TRAIL_* */
	byte pad_26[2];
	TrailPoint *m_pTrails;		/* 0x28 new[] of m_wMaxTrails */
	float u0, v0;			/* 0x2c */
	float du, dv;			/* 0x34 */
	byte orient;			/* 0x3c TRAIL_ORIENT_* */
	byte pad_3d[3];
	vec3_t orientVec;		/* 0x40 */
	float uLen;			/* 0x4c the points for du */
	float f50;			/* 0x50 */
} CTrail;
SD_SIZE(CTrail, 0x54);
SD_OFFSET(CTrail, m_pTrails, 0x28);
SD_OFFSET(CTrail, orientVec, 0x40);

/* the lists of the active trails and the shared buffers (IDirect3DVertexBuffer8, IDirect3DIndexBuffer8) */
#define g_TrailsMat (GAME_ARRAY(struct CMaterial *, g_TrailsMat, 0x4d6e08))	/* [MAX_TRAILS_TEXTURES] */
#define g_TrailsOn GAME_GLOBAL_IN(int, GVar_R_Trails, 16, 0x4d6e5c)
#define g_TrailsVB GAME_GLOBAL(void *, g_TrailsVB, 0x4d6e64)
#define g_TrailsIB GAME_GLOBAL(void *, g_TrailsIB, 0x4d6e68)
#define g_TrailsVertex GAME_GLOBAL(int, g_TrailsVertex, 0x4d6e6c)	/* vertices, primitives and indices in the buffers */
#define g_TrailsPrims GAME_GLOBAL(int, g_TrailsPrims, 0x4d6e70)
#define g_TrailsIndex GAME_GLOBAL(int, g_TrailsIndex, 0x4d6e74)
#define g_TrailsRender (GAME_ARRAY(CTrail *[MAX_TRAILS_RENDER], g_TrailsRender, 0x4d6e78))
#define g_nTrails GAME_GLOBAL(int, g_nTrails, 0x4db278)
#define g_nTrailsRender (GAME_ARRAY(int, g_nTrailsRender, 0x4db280))	/* [MAX_TRAILS_TEXTURES] */

CTrail *__fastcall CTrail_ctor(CTrail *self);
void __fastcall CTrail_dtor(CTrail *self);
/* into or out of gTrailsRender as render says; the last of the list takes the free slot */
void Trails_UpdateRender(CTrail *t);
/* no points, out of the render list */
void __fastcall CTrail_Clear(CTrail *self);
/* everything but uLen and f50, the points copied; the sizes must be equal */
void __thiscall CTrail_Copy(CTrail *self, const CTrail *from);
/* one point less, out of the render list at none */
void __fastcall CTrail_Shrink(CTrail *self);
void __thiscall CTrail_SetActive(CTrail *self, byte on);
/* no points, the render list as it is */
void __fastcall CTrail_Reset(CTrail *self);
void __thiscall CTrail_AddPos(CTrail *self, vec3_t pos);
void __thiscall CTrail_AddPosColor(CTrail *self, vec3_t pos, uint32_t color);
void __thiscall CTrail_Add(CTrail *self, vec3_t pos, float width, uint32_t color);
/* the geometry of the trail into the locked buffers, the pointers moved on; S_OK or E_FAIL for an unknown type */
int __thiscall CTrail_Build(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex);
int __thiscall CTrail_BuildRibbon(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex);
int __thiscall CTrail_BuildCross(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex);
int __thiscall CTrail_BuildQuads(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex);
int __thiscall CTrail_BuildFan(CTrail *self, TrailVertex **pVertex, uint16_t **pIndex);
void __thiscall CTrail_SetTransp(CTrail *self, byte transp);
void __thiscall CTrail_SetColor(CTrail *self, uint32_t color);
void __thiscall CTrail_SetSmooth(CTrail *self, byte on);
/* the point array made once; the same size again clears the trail */
void __thiscall CTrail_SetMaxTrails(CTrail *self, byte segments);
void __thiscall CTrail_SetUV(CTrail *self, float u0, float v0);
void __thiscall CTrail_SetOrient(CTrail *self, vec3_t v, byte mode);
void __thiscall CTrail_SetType(CTrail *self, byte type);
void __thiscall CTrail_SetSegments(CTrail *self, byte segments);
void __thiscall CTrail_SetPremul(CTrail *self, byte on);
void __thiscall CTrail_SetWidth(CTrail *self, float width);
void __thiscall CTrail_SetScale(CTrail *self, float head, float tail);
void __thiscall CTrail_SetTexture(CTrail *self, int texture);
void __thiscall CTrail_SetUVScale(CTrail *self, float du, float dv);
void __thiscall CTrail_SetFade(CTrail *self, byte on);
void __thiscall CTrail_SetULen(CTrail *self, float uLen, float f50);
void Trail_SetRender(CTrail *t, byte on);
/* all render lists emptied, their trails cleared; 0 */
int Trails_ResetAll(void);
/* the buffers released; 0 */
int Trails_Release(void);
/* the buffers and the materials made; S_OK or E_FAIL */
int Trails_Init(void);
/* the render lists emptied without touching the trails; 0 */
int Trails_InitLists(void);
/* the trails of one texture, or of all but the last for -1 */
void Trails_Render(int texture);
/* the members of the point written last, NULL for the ones to keep, only with points */
void __thiscall CTrail_SetLast(CTrail *self, const vec3_t *pos, const float *width, const uint32_t *color);
void __thiscall CTrail_SetLastPos(CTrail *self, vec3_t pos);
void __thiscall CTrail_SetLastPosColor(CTrail *self, vec3_t pos, uint32_t color);
/* point i counted back from the newest (0) */
void __thiscall CTrail_SetPoint(CTrail *self, int i, const vec3_t *pos, const float *width, const uint32_t *color);
/* all m_wMaxTrails points moved by d */
void __thiscall CTrail_Move(CTrail *self, vec3_t d);

#endif
