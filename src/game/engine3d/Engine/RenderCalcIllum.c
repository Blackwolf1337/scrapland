/*
 * The lighting of points by the D3D light formulas on the CPU (the light maps of bsp.cpp), original source
 * Engine/RenderCalcIlum.cpp (include/scrap/light.h).
 */
#include "scrap/vertlist.h"
#include "scrap/light.h"
#include "scrap/trimesh.h"
#include "scrap/material.h"
#include "scrap/mathlib.h"

float g_IllumSmooth = 0.5f;

/* the weight of the smoothing of the light angle (ILLUM_SOFT) */

float Illum_Smooth(float v) {
	float k = g_IllumSmooth;

	if (k != 0.0f) {
		v = v * v * (2.0f - v) * k + (1.0f - k) * v;
	}
	return v;
}

/* (x, y, z) / its length, unchanged for length 0 */
static vec3_t Illum_Normalize(float x, float y, float z) {
	vec3_t v = { x, y, z };
	float len = (float)__builtin_sqrt((double)(y * y + x * x + z * z));

	if ((double)len != 0.0) {
		float k = (float)(1.0 / (double)len);
		v.x = x * k;
		v.y = y * k;
		v.z = z * k;
	}
	return v;
}

int Illum_VertList(color4_t *dest, CVertList *lv, CMaterial *Mat, CLight *Light) {
	float mc[17];
	D3DLight l;

	CHECK(Mat && Light);
	CMaterial_CalcColors(Mat, mc);
	CLight_FillD3D(Light, &l);
	CLight_AdjustD3D(Light, &l);
	/* the direction and the position of the light as it is set, the rest adjusted */
	vec3_t dir = Illum_Normalize(Light->d3d.Direction.x, Light->d3d.Direction.y, Light->d3d.Direction.z);
	vec3_t org = Light->d3d.Position;
	for (int i = 0; i < lv->nVert; i++) {
		vec3_t pos, n, d;
		float att;
		CVertList_GetPos(lv, &pos, i);
		CVertList_GetNormal(lv, &n, i);
		int type = Light->type;
		if (type == LIGHT_DIR) {
			att = 1.0f;
			d = dir;
		} else {
			float dy = pos.y - org.y, dx = pos.x - org.x, dz = pos.z - org.z;
			float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			if (dist > l.Range) {
				continue;
			}
			att = (float)(1.0 / (double)(l.Attenuation1 * dist + l.Attenuation0 + dist * dist * l.Attenuation2));
			d = Illum_Normalize(dx, dy, dz);
		}
		if (type == LIGHT_SPOT) {
			float c = d.x * dir.x + d.y * dir.y + d.z * dir.z;
			float cosTheta = (float)cos((double)l.Theta * 0.5);
			float cosPhi = (float)cos((double)l.Phi * 0.5);
			if (cosPhi > c) {
				continue;
			}
			if (cosTheta > c) {
				float s = (c - cosPhi) / (cosTheta - cosPhi);
				att = (float)pow((double)s, (double)l.Falloff) * att;
			}
		}
		float dot = -(n.x * d.x + n.y * d.y + n.z * d.z);
		if (!(dot > 0.0f)) {
			continue;
		}
		color4_t *o = &dest[i], c;
		c.r = ((mc[0] * dot) * l.Diffuse.r + l.Ambient.r * mc[4]) * att + o->r;
		c.g = ((mc[1] * dot) * l.Diffuse.g + l.Ambient.g * mc[5]) * att + o->g;
		c.b = ((mc[2] * dot) * l.Diffuse.b + l.Ambient.b * mc[6]) * att + o->b;
		c.a = ((mc[3] * dot) * l.Diffuse.a + l.Ambient.a * mc[7]) * att + o->a;
		*o = c;
	}
	return 0;
}

int Illum_Light(color4_t *dest, const vec3_t *pos, const vec3_t *n, const float *mat, const IllumLight *light,
	uint32_t flags) {
	const D3DLight *l = &light->d3d;
	vec3_t d;
	float att;

	CHECK(dest);
	if (l->Type == D3DLIGHT_DIRECTIONAL) {
		att = 1.0f;
		d = l->Direction;
	} else {
		float dy = pos->y - l->Position.y, dx = pos->x - l->Position.x, dz = pos->z - l->Position.z;
		float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if (dist > l->Range) {
			return 1;
		}
		if (dist == 0.0f) {
			return 1;
		}
		if (flags & ILLUM_SOFT) {
			att = 1.0f;
			if (dist > light->rangeIni) {
				float t = (l->Range - dist) / (l->Range - light->rangeIni);
				att = (3.0f - (t + t)) * (t * t);
			}
		} else {
			att = (float)(1.0 / (double)(l->Attenuation1 * dist + l->Attenuation0 +
				dist * dist * l->Attenuation2));
		}
		d.x = dx / dist;
		d.y = dy / dist;
		d.z = dz / dist;
	}
	if (l->Type == D3DLIGHT_SPOT) {
		float c = d.x * l->Direction.x + d.y * l->Direction.y + d.z * l->Direction.z;
		float cosTheta = (float)cos((double)l->Theta * 0.5);
		float cosPhi = (float)cos((double)l->Phi * 0.5);
		if (cosPhi > c) {
			return 1;
		}
		if (cosTheta > c) {
			float s = (c - cosPhi) / (cosTheta - cosPhi);
			if (l->Falloff != 1.0f) {
				s = (float)pow((double)s, (double)l->Falloff);
			}
			if (flags & ILLUM_SOFT) {
				s = s * s * (3.0f - (s + s));
			}
			att = s * att;
		}
	}
	float dot = -(n->x * d.x + n->y * d.y + n->z * d.z);
	if (0.0f >= dot) {
		return 1;
	}
	if (flags & ILLUM_SOFT) {
		dot = Illum_Smooth(dot);
	}
	color4_t c;
	c.r = (l->Diffuse.r * (mat[0] * dot) + l->Ambient.r * mat[4]) * att + dest->r;
	c.g = (l->Diffuse.g * (mat[1] * dot) + l->Ambient.g * mat[5]) * att + dest->g;
	c.b = (l->Diffuse.b * (mat[2] * dot) + l->Ambient.b * mat[6]) * att + dest->b;
	c.a = (l->Diffuse.a * (mat[3] * dot) + l->Ambient.a * mat[7]) * att + dest->a;
	*dest = c;
	return 0;
}
