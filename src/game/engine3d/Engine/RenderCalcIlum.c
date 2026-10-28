/*
 * The lighting of points by the D3D light formulas on the CPU (the light maps of bsp.cpp), original source
 * Engine/RenderCalcIlum.cpp (include/scrap/luz.h).
 */
#include "scrap/listavert.h"
#include "scrap/luz.h"
#include "scrap/mallatri.h"
#include "scrap/material.h"
#include "scrap/mathlib.h"

/* the weight of the smoothing of the light angle (ILUM_SUAVE) */
#define g_IlumSmooth GAME_GLOBAL(float, g_IlumSmooth, 0x432a84)

float Ilum_Smooth(float v) {
	float k = g_IlumSmooth;

	if (k != 0.0f) {
		v = v * v * (2.0f - v) * k + (1.0f - k) * v;
	}
	return v;
}

/* (x, y, z) / its length, unchanged for length 0 */
static vec3_t Ilum_Normalize(float x, float y, float z) {
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

int Ilum_ListaVert(color4_t *dest, CListaVert *lv, CMaterial *Mat, CLuz *Luz) {
	float mc[17];
	D3DLight l;

	CHECK(Mat && Luz);
	CMaterial_CalcColors(Mat, mc);
	CLuz_FillD3D(Luz, &l);
	CLuz_AdjustD3D(Luz, &l);
	/* the direction and the position of the light as it is set, the rest adjusted */
	vec3_t dir = Ilum_Normalize(Luz->d3d.Direction.x, Luz->d3d.Direction.y, Luz->d3d.Direction.z);
	vec3_t org = Luz->d3d.Position;
	for (int i = 0; i < lv->nVert; i++) {
		vec3_t pos, n, d;
		float att;
		CListaVert_GetPos(lv, &pos, i);
		CListaVert_GetNormal(lv, &n, i);
		int tipo = Luz->tipo;
		if (tipo == LUZ_DIREC) {
			att = 1.0f;
			d = dir;
		} else {
			float dy = pos.y - org.y, dx = pos.x - org.x, dz = pos.z - org.z;
			float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			if (dist > l.Range) {
				continue;
			}
			att = (float)(1.0 / (double)(l.Attenuation1 * dist + l.Attenuation0 + dist * dist * l.Attenuation2));
			d = Ilum_Normalize(dx, dy, dz);
		}
		if (tipo == LUZ_FOCO) {
			float c = d.x * dir.x + d.y * dir.y + d.z * dir.z;
			float cosTheta = (float)Game_Cos((double)l.Theta * 0.5);
			float cosPhi = (float)Game_Cos((double)l.Phi * 0.5);
			if (cosPhi > c) {
				continue;
			}
			if (cosTheta > c) {
				float s = (c - cosPhi) / (cosTheta - cosPhi);
				att = (float)Game_Pow((double)s, (double)l.Falloff) * att;
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

int Ilum_Light(color4_t *dest, const vec3_t *pos, const vec3_t *n, const float *mat, const IlumLuz *luz,
	uint32_t flags) {
	const D3DLight *l = &luz->d3d;
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
		if (flags & ILUM_SUAVE) {
			att = 1.0f;
			if (dist > luz->rangeIni) {
				float t = (l->Range - dist) / (l->Range - luz->rangeIni);
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
		float cosTheta = (float)Game_Cos((double)l->Theta * 0.5);
		float cosPhi = (float)Game_Cos((double)l->Phi * 0.5);
		if (cosPhi > c) {
			return 1;
		}
		if (cosTheta > c) {
			float s = (c - cosPhi) / (cosTheta - cosPhi);
			if (l->Falloff != 1.0f) {
				s = (float)Game_Pow((double)s, (double)l->Falloff);
			}
			if (flags & ILUM_SUAVE) {
				s = s * s * (3.0f - (s + s));
			}
			att = s * att;
		}
	}
	float dot = -(n->x * d.x + n->y * d.y + n->z * d.z);
	if (0.0f >= dot) {
		return 1;
	}
	if (flags & ILUM_SUAVE) {
		dot = Ilum_Smooth(dot);
	}
	color4_t c;
	c.r = (l->Diffuse.r * (mat[0] * dot) + l->Ambient.r * mat[4]) * att + dest->r;
	c.g = (l->Diffuse.g * (mat[1] * dot) + l->Ambient.g * mat[5]) * att + dest->g;
	c.b = (l->Diffuse.b * (mat[2] * dot) + l->Ambient.b * mat[6]) * att + dest->b;
	c.a = (l->Diffuse.a * (mat[3] * dot) + l->Ambient.a * mat[7]) * att + dest->a;
	*dest = c;
	return 0;
}
