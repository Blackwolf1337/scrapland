/*
 * ScrapEngTrails.cpp of the game: CGameTrail, see include/scrap/scraptrails.h.
 */
#include "scrap/scraptrails.h"

CGameTrail *CGameTrail_ctor(CGameTrail *self) {
	self->trailsys = NULL;
	CTrail *t = operator_new(sizeof(CTrail));
	self->trailsys = t ? CTrail_ctor(t) : NULL;
	if (!self->trailsys) {
		CheckFailed("trailsys", __FILE__, __TIMESTAMP__, __LINE__);
	}
	return self;
}

void CGameTrail_dtor(CGameTrail *self) {
	CTrail *t = self->trailsys;
	if (t) {
		CTrail_dtor(t);
		operator_delete_sized(t, sizeof(CTrail));
	}
}

void CGameTrail_Copy(CGameTrail *self, const CGameTrail *from) {
	if (!from) {
		CheckFailed("from", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (!from->trailsys) {
		CheckFailed("from->trailsys", __FILE__, __TIMESTAMP__, __LINE__);
	}
	CTrail_Copy(self->trailsys, from->trailsys);
}

void CGameTrail_BezierCoefs(CGameTrail *self, int n, const vec3_t *pts, vec3_t *out) {
	int last = n - 1;
	int c = 0;

	(void)self;
	for (int i = 0; i <= last; i++) {
		if (i == 0) {
			c = 1;
		} else if (i == 1) {
			c = last;
		} else {
			/* imul wraps */
			c = (int)((uint32_t)(last - i + 1) * (uint32_t)c) / i;
		}
		float f = (float)c;
		out[i].x = pts[i].x * f;
		out[i].y = pts[i].y * f;
		out[i].z = pts[i].z * f;
	}
}

void CGameTrail_BezierPoint(CGameTrail *self, int n, const vec3_t *coefs, vec3_t *out, float t) {
	vec3_t tmp[GAMETRAIL_MAX_BEZIER];
	int last = n - 1;
	float pw = t;

	(void)self;
	tmp[0] = coefs[0];
	for (int i = 1; i <= last; i++) {
		tmp[i].x = coefs[i].x * pw;
		tmp[i].y = coefs[i].y * pw;
		tmp[i].z = coefs[i].z * pw;
		pw = pw * t;
	}
	float s = 1.0f - t;
	*out = tmp[last];
	pw = s;
	for (int i = last - 1; i >= 0; i--) {
		out->x = tmp[i].x * pw + out->x;
		out->y = tmp[i].y * pw + out->y;
		out->z = tmp[i].z * pw + out->z;
		pw = pw * s;
	}
}

void CGameTrail_Clear(CGameTrail *self) {
	CTrail_Clear(self->trailsys);
}

void CGameTrail_Shrink(CGameTrail *self) {
	CTrail_Shrink(self->trailsys);
}

void CGameTrail_Reset(CGameTrail *self) {
	CTrail_Reset(self->trailsys);
}

int CGameTrail_GetTransp(const CGameTrail *self) {
	return self->trailsys->transp;
}

int CGameTrail_GetMaxTrails(const CGameTrail *self) {
	return self->trailsys->m_wMaxTrails;
}

byte CGameTrail_IsRender(const CGameTrail *self) {
	return self->trailsys->render;
}

void CGameTrail_AddPos(CGameTrail *self, vec3_t pos) {
	CTrail_AddPos(self->trailsys, pos);
}

void CGameTrail_AddPosColor(CGameTrail *self, vec3_t pos, uint32_t color) {
	CTrail_AddPosColor(self->trailsys, pos, color);
}

void CGameTrail_Add(CGameTrail *self, vec3_t pos, float width, uint32_t color) {
	CTrail_Add(self->trailsys, pos, width, color);
}

void CGameTrail_SetTransp(CGameTrail *self, byte transp) {
	CTrail_SetTransp(self->trailsys, transp);
}

void CGameTrail_SetColor(CGameTrail *self, uint32_t color) {
	CTrail_SetColor(self->trailsys, color);
}

void CGameTrail_SetSmooth(CGameTrail *self, byte on) {
	CTrail_SetSmooth(self->trailsys, on);
}

void CGameTrail_SetMaxTrails(CGameTrail *self, byte segments) {
	CTrail_SetMaxTrails(self->trailsys, segments);
}

void CGameTrail_SetUV(CGameTrail *self, float u0, float v0) {
	CTrail_SetUV(self->trailsys, u0, v0);
}

void CGameTrail_SetOrient(CGameTrail *self, vec3_t v, byte mode) {
	CTrail_SetOrient(self->trailsys, v, mode);
}

void CGameTrail_SetType(CGameTrail *self, byte type) {
	CTrail_SetType(self->trailsys, type);
}

void CGameTrail_SetSegments(CGameTrail *self, byte segments) {
	CTrail_SetSegments(self->trailsys, segments);
}

void CGameTrail_SetPremul(CGameTrail *self, byte on) {
	CTrail_SetPremul(self->trailsys, on);
}

void CGameTrail_SetWidth(CGameTrail *self, float width) {
	CTrail_SetWidth(self->trailsys, width);
}

void CGameTrail_SetScale(CGameTrail *self, float head, float tail) {
	CTrail_SetScale(self->trailsys, head, tail);
}

void CGameTrail_SetTexture(CGameTrail *self, int texture) {
	CTrail_SetTexture(self->trailsys, texture);
}

void CGameTrail_SetUVScale(CGameTrail *self, float du, float dv) {
	CTrail_SetUVScale(self->trailsys, du, dv);
}

void CGameTrail_SetFade(CGameTrail *self, byte on) {
	CTrail_SetFade(self->trailsys, on);
}

void CGameTrail_SetULen(CGameTrail *self, float uLen, float f50) {
	CTrail_SetULen(self->trailsys, uLen, f50);
}

void CGameTrail_SetLast(CGameTrail *self, const vec3_t *pos, const float *width, const uint32_t *color) {
	CTrail_SetLast(self->trailsys, pos, width, color);
}

void CGameTrail_SetLastPos(CGameTrail *self, vec3_t pos) {
	CTrail_SetLastPos(self->trailsys, pos);
}

void CGameTrail_SetLastPosColor(CGameTrail *self, vec3_t pos, uint32_t color) {
	CTrail_SetLastPosColor(self->trailsys, pos, color);
}

void CGameTrail_SetPoint(CGameTrail *self, int i, const vec3_t *pos, const float *width,
	const uint32_t *color) {
	CTrail_SetPoint(self->trailsys, i, pos, width, color);
}

