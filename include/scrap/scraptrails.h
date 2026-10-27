/*
 * ScrapEngTrails.cpp of the game: CGameTrail, a trail of the engine3d (CTrail) owned by an entity, with lines and
 * Bezier curves filled into all its points at once.
 */
#ifndef SCRAP_SCRAPTRAILS_H
#define SCRAP_SCRAPTRAILS_H

#include "scrap/trails.h"

#define GAMETRAIL_MAX_BEZIER 16	/* the control points the stack arrays of the Bezier functions hold */

typedef struct CGameTrail {
	CTrail *trailsys;		/* 0x0 owned */
} CGameTrail;
SD_SIZE(CGameTrail, 4);

CGameTrail *__fastcall CGameTrail_ctor(CGameTrail *self);
void __fastcall CGameTrail_dtor(CGameTrail *self);
void __thiscall CGameTrail_Copy(CGameTrail *self, const CGameTrail *from);
/* the coefficients of the n control points pts, binomial(n - 1, i) * pts[i] (self is not used) */
void __thiscall CGameTrail_BezierCoefs(CGameTrail *self, int n, const vec3_t *pts, vec3_t *out);
/* the curve of the coefficients at t (self is not used) */
void __thiscall CGameTrail_BezierPoint(CGameTrail *self, int n, const vec3_t *coefs, vec3_t *out, float t);
/* the points of the trail along the Bezier curve of the n control points pts */
void __thiscall CGameTrail_AddBezier(CGameTrail *self, int n, const vec3_t *pts, uint32_t color);
/* the points of the trail along the line from .. to */
void __thiscall CGameTrail_AddLine(CGameTrail *self, vec3_t from, vec3_t to);

/* the CTrail methods for the trail */
void __fastcall CGameTrail_Clear(CGameTrail *self);
void __fastcall CGameTrail_Shrink(CGameTrail *self);
void __fastcall CGameTrail_Reset(CGameTrail *self);
int __fastcall CGameTrail_GetTransp(const CGameTrail *self);
int __fastcall CGameTrail_GetMaxTrails(const CGameTrail *self);
byte __fastcall CGameTrail_IsRender(const CGameTrail *self);
void __thiscall CGameTrail_AddPos(CGameTrail *self, vec3_t pos);
void __thiscall CGameTrail_AddPosColor(CGameTrail *self, vec3_t pos, uint32_t color);
void __thiscall CGameTrail_Add(CGameTrail *self, vec3_t pos, float width, uint32_t color);
void __thiscall CGameTrail_SetTransp(CGameTrail *self, byte transp);
void __thiscall CGameTrail_SetColor(CGameTrail *self, uint32_t color);
void __thiscall CGameTrail_SetSmooth(CGameTrail *self, byte on);
void __thiscall CGameTrail_SetMaxTrails(CGameTrail *self, byte segments);
void __thiscall CGameTrail_SetUV(CGameTrail *self, float u0, float v0);
void __thiscall CGameTrail_SetOrient(CGameTrail *self, vec3_t v, byte mode);
void __thiscall CGameTrail_SetType(CGameTrail *self, byte type);
void __thiscall CGameTrail_SetSegments(CGameTrail *self, byte segments);
void __thiscall CGameTrail_SetPremul(CGameTrail *self, byte on);
void __thiscall CGameTrail_SetWidth(CGameTrail *self, float width);
void __thiscall CGameTrail_SetScale(CGameTrail *self, float head, float tail);
void __thiscall CGameTrail_SetTexture(CGameTrail *self, int texture);
void __thiscall CGameTrail_SetUVScale(CGameTrail *self, float du, float dv);
void __thiscall CGameTrail_SetFade(CGameTrail *self, byte on);
void __thiscall CGameTrail_SetULen(CGameTrail *self, float uLen, float f50);
void __thiscall CGameTrail_SetLast(CGameTrail *self, const vec3_t *pos, const float *width, const uint32_t *color);
void __thiscall CGameTrail_SetLastPos(CGameTrail *self, vec3_t pos);
void __thiscall CGameTrail_SetLastPosColor(CGameTrail *self, vec3_t pos, uint32_t color);
void __thiscall CGameTrail_SetPoint(CGameTrail *self, int i, const vec3_t *pos, const float *width,
	const uint32_t *color);
void __thiscall CGameTrail_Move(CGameTrail *self, vec3_t d);

#endif
