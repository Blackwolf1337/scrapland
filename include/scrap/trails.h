/*
 * Trails of the engine3d (original source Engine/trails.cpp): ribbons behind moving objects. A trail keeps a ring of
 * up to m_wMaxTrails points (position, color with the alpha inverted, width); the newest is at head - 1. An active
 * trail sits in gTrailsRender[texture] and is turned into geometry once per frame (Trails_Render): one dynamic
 * vertex and index buffer for all trails, filled per texture, flushed when full.
 */
#ifndef SCRAP_TRAILS_H
#define SCRAP_TRAILS_H

#include "scrap/gvar.h"
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
	vec3_t pos;
	uint32_t color;			/* the alpha byte is 255 - alpha */
	float width;
} TrailPoint;

/* FVF 0x1c2: XYZ, DIFFUSE, SPECULAR, TEX1 */
typedef struct TrailVertex {
	vec3_t pos;
	uint32_t color;
	uint32_t specular;		/* always 0 */
	float u, v;
} TrailVertex;

typedef struct CTrail {
	byte render;			/* in gTrailsRender (Trails_UpdateRender) */
	byte pad_1[3];
	int RenderIndex;		/* the slot in gTrailsRender[texture], -1 */
	int texture;			/* the material, < MAX_TRAILS_TEXTURES */
	byte fade;			/* alpha from 0 at the tail to 255 at the head */
	byte smooth;			/* the newest piece of a ribbon in two halves */
	byte premul;			/* color multiplied by the alpha */
	byte transp;			/* 255 is invisible */
	uint32_t color;			/* of AddPos */
	float width;			/* of AddPos and AddPosColor */
	float scaleHead;
	float scaleTail;
	byte m_wMaxTrails;
	byte numTrails;			/* at most m_wMaxTrails - 1 */
	byte head;			/* the next point to write */
	byte segments;			/* the points drawn, <= m_wMaxTrails */
	byte last;			/* the point written last */
	byte type;			/* TRAIL_* */
	byte pad_26[2];
	TrailPoint *m_pTrails;		/* new[] of m_wMaxTrails */
	float u0, v0;
	float du, dv;
	byte orient;			/* TRAIL_ORIENT_* */
	byte pad_3d[3];
	vec3_t orientVec;
	float uLen;			/* the points for du */
	float unused_50;
} CTrail;

CTrail *CTrail_ctor(CTrail *self);
void CTrail_dtor(CTrail *self);
/* no points, out of the render list */
void CTrail_Clear(CTrail *self);
/* everything but uLen and f50, the points copied; the sizes must be equal */
void CTrail_Copy(CTrail *self, const CTrail *from);
/* one point less, out of the render list at none */
void CTrail_Shrink(CTrail *self);
/* no points, the render list as it is */
void CTrail_Reset(CTrail *self);
void CTrail_AddPos(CTrail *self, vec3_t pos);
void CTrail_AddPosColor(CTrail *self, vec3_t pos, uint32_t color);
void CTrail_Add(CTrail *self, vec3_t pos, float width, uint32_t color);
void CTrail_SetTransp(CTrail *self, byte transp);
void CTrail_SetColor(CTrail *self, uint32_t color);
void CTrail_SetSmooth(CTrail *self, byte on);
/* the point array made once; the same size again clears the trail */
void CTrail_SetMaxTrails(CTrail *self, byte segments);
void CTrail_SetUV(CTrail *self, float u0, float v0);
void CTrail_SetOrient(CTrail *self, vec3_t v, byte mode);
void CTrail_SetType(CTrail *self, byte type);
void CTrail_SetSegments(CTrail *self, byte segments);
void CTrail_SetPremul(CTrail *self, byte on);
void CTrail_SetWidth(CTrail *self, float width);
void CTrail_SetScale(CTrail *self, float head, float tail);
void CTrail_SetTexture(CTrail *self, int texture);
void CTrail_SetUVScale(CTrail *self, float du, float dv);
void CTrail_SetFade(CTrail *self, byte on);
void CTrail_SetULen(CTrail *self, float uLen, float f50);
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
void CTrail_SetLast(CTrail *self, const vec3_t *pos, const float *width, const uint32_t *color);
void CTrail_SetLastPos(CTrail *self, vec3_t pos);
void CTrail_SetLastPosColor(CTrail *self, vec3_t pos, uint32_t color);
/* point i counted back from the newest (0) */
void CTrail_SetPoint(CTrail *self, int i, const vec3_t *pos, const float *width, const uint32_t *color);
/* all m_wMaxTrails points moved by d */
void CTrail_Move(CTrail *self, vec3_t d);

#endif
