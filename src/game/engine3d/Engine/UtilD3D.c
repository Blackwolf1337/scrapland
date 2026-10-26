/*
 * Engine/util_d3d.cpp of the engine3d: debug lines, see include/scrap/utild3d.h.
 */
#include "scrap/e3dutil.h"
#include "scrap/nodo3d.h"
#include "scrap/utild3d.h"

#define M_2PI_D 6.283185307179586	/* rva 0x3b45c8 */

static int Color_Clamp(int v) {
	if (v <= 0) {
		return 0;
	}
	return v >= 0xff ? 0xff : v;
}

uint32_t Color_FromFloat(const float *c) {
	int r = (int)X87_Fistp((double)(c[0] * 255.0f));
	int g = (int)X87_Fistp((double)(c[1] * 255.0f));
	int b = (int)X87_Fistp((double)(c[2] * 255.0f));
	int a = (int)X87_Fistp((double)(c[3] * 255.0f));
	return (uint32_t)Color_Clamp(a) << 24 | (uint32_t)Color_Clamp(r) << 16 | (uint32_t)Color_Clamp(g) << 8
		| (uint32_t)Color_Clamp(b);
}

/* the 12 edges of the box, corners as bits x 4, y 2, z 1 (set: mx) */
static void Box_Lines(const vec3_t *mn, const vec3_t *mx, uint32_t color) {
	static const byte edges[12][2] = {
		{ 0, 4 }, { 0, 2 }, { 0, 1 }, { 1, 3 }, { 1, 5 }, { 5, 4 },
		{ 4, 6 }, { 7, 3 }, { 7, 5 }, { 7, 6 }, { 6, 2 }, { 2, 3 },
	};
	for (int i = 0; i < 12; i++) {
		vec3_t p[2];
		for (int k = 0; k < 2; k++) {
			int c = edges[i][k];
			p[k] = (vec3_t){ c & 4 ? mx->x : mn->x, c & 2 ? mx->y : mn->y, c & 1 ? mx->z : mn->z };
		}
		Render_Line(&p[0], &p[1], color);
	}
}

void Debug_DrawBoxM(const matrix_t *m, const vec3_t *mn, const vec3_t *mx, uint32_t color) {
	Render_SetWorldMatrix(m, 0);
	Box_Lines(mn, mx, color);
}

void Debug_DrawBox(const vec3_t *mn, const vec3_t *mx, uint32_t color) {
	Render_SetWorldMatrix(g_MatIdentity, 0);
	Box_Lines(mn, mx, color);
}

void Debug_DrawConeM(const matrix_t *m, float len, float ang) {
	double h = (double)ang * 0.5;
	float r = (float)Game_Sin(h) * len;
	float d = -((float)Game_Cos(h) * len);
	const vec3_t o = { 0.0f, 0.0f, 0.0f };
	const vec3_t ends[4] = { { -r, d, 0.0f }, { r, d, 0.0f }, { 0.0f, d, r }, { 0.0f, d, -r } };
	for (int i = 0; i < 4; i++) {
		Render_SetWorldMatrix(m, 0);
		Render_Line(&o, &ends[i], 0xc0c0c0);
	}
	for (int i = 0; i < 32;) {
		double a = (double)(float)((double)i * M_PI_D * 0.0625);
		vec3_t p = { (float)Game_Cos(a) * r, d, (float)Game_Sin(a) * r };
		i += 2;
		a = (double)(float)((double)i * M_PI_D * 0.0625);
		vec3_t q = { (float)Game_Cos(a) * r, d, (float)Game_Sin(a) * r };
		Render_SetWorldMatrix(m, 0);
		Render_Line(&p, &q, 0x808080);
	}
}

/* the lines from pos - size to pos + size along x, y and z */
static void Cross_Lines(const vec3_t *pos, float size, const uint32_t color[3]) {
	for (int i = 0; i < 3; i++) {
		vec3_t d = { i == 0 ? size : 0.0f, i == 1 ? size : 0.0f, i == 2 ? size : 0.0f };
		vec3_t a = { pos->x - d.x, pos->y - d.y, pos->z - d.z };
		vec3_t b = { pos->x + d.x, pos->y + d.y, pos->z + d.z };
		Render_Line(&a, &b, color[i]);
	}
}

void Debug_DrawAxes(const matrix_t *m, const vec3_t *pos, float size) {
	static const uint32_t colors[3] = { 0x800000, 0x8000, 0x80 };
	Render_SetWorldMatrix(m, 0);
	Cross_Lines(pos, size, colors);
}

void Debug_DrawPointM(const matrix_t *m, const vec3_t *pos, float size, uint32_t color) {
	const uint32_t colors[3] = { color, color, color };
	Render_SetWorldMatrix(m, 0);
	Cross_Lines(pos, size, colors);
}

void Debug_DrawPoint(const vec3_t *pos, float size, uint32_t color) {
	Debug_DrawPointM(g_MatIdentity, pos, size, color);
}

void Debug_DrawSphereM(const matrix_t *m, const vec3_t *c, float r, uint32_t color, int divV, int divH) {
	vec3_t pts[MAX_DIVY_ESFERA];

	if (0.0f >= r) {
		return;
	}
	int nEcu = divH != -1 ? divH : divV;
	CHECK(divV > 1);
	CHECK(nEcu > 1);
	CHECK(nEcu < MAX_DIVY_ESFERA);
	Debug_DrawPointM(m, c, r, 0x640000);
	Render_SetWorldMatrix(m, 0);
	/* the half circle of the first meridian; two points at a time through the packed sin and cos like the
	 * vectorised loop of the original, the last odd one through the scalar ones */
	if (nEcu >= 0) {
		float k = (float)(M_PI_D / (double)nEcu);
		int i = 0;
		if ((uint32_t)nEcu + 1 >= 2) {
			int last = nEcu - (((uint32_t)nEcu + 1) & 1);
			do {
				double a[2] = { (double)((float)i * k), (double)((float)(i + 1) * k) };
				double s[2], co[2];
				Game_SinPd(a, s);
				Game_CosPd(a, co);
				for (int j = 0; j < 2; j++) {
					pts[i + j] = (vec3_t){ (float)s[j] * r + c->x, (float)co[j] * r + c->y, c->z };
				}
				i += 2;
			} while (i <= last);
		}
		for (; i <= nEcu; i++) {
			double a = (double)((float)i * k);
			pts[i].x = (float)Game_Sin(a) * r + c->x;
			pts[i].y = (float)Game_Cos(a) * r + c->y;
			pts[i].z = c->z;
		}
	}
	/* each meridian from its predecessor (kept in pts) */
	float kv = (float)(M_2PI_D / (double)divV);
	for (int j = 1; j <= divV; j++) {
		float sv, cv;
		X87_SinCos((float)j * kv, &sv, &cv);
		int i = 1;
		if (nEcu > 1) {
			float kh = (float)(M_PI_D / (double)nEcu);
			for (; i < nEcu; i++) {
				float s, co;
				X87_SinCos((float)i * kh, &s, &co);
				float sr = s * r;
				float cr = co * r;
				vec3_t p = { sr * cv + c->x, c->y + cr, c->z - sr * sv };
				Render_Line(&pts[i - 1], &p, color);
				Render_Line(&pts[i], &p, color);
				pts[i] = p;
			}
		}
		Render_Line(&pts[i - 1], &pts[i], color);
	}
}

void Debug_DrawSphere(const vec3_t *c, float r, uint32_t color, int divV, int divH) {
	Debug_DrawSphereM(g_MatIdentity, c, r, color, divV, divH);
}

void Debug_DrawLineM(const matrix_t *m, const vec3_t *a, const vec3_t *b, uint32_t color) {
	Render_SetWorldMatrix(m, 0);
	Render_Line(a, b, color);
}

void Debug_DrawLine(const vec3_t *a, const vec3_t *b, uint32_t color) {
	Debug_DrawLineM(g_MatIdentity, a, b, color);
}

void Debug_DrawTriM(const matrix_t *m, const vec3_t *a, const vec3_t *b, const vec3_t *c, uint32_t color) {
	Render_SetWorldMatrix(m, 0);
	Render_Line(a, b, color);
	Render_Line(b, c, color);
	Render_Line(c, a, color);
}

void Debug_DrawTri(const vec3_t *a, const vec3_t *b, const vec3_t *c, uint32_t color) {
	Debug_DrawTriM(g_MatIdentity, a, b, c, color);
}

const char *Obj3D_ClaseName(uint32_t clase) {
	switch (clase) {
	case 0:
		return "(null)";
	case CLASE_OBJ3D_MALLATRI:
		return "MallaTri";
	case CLASE_OBJ3D_MALLAD3D:
		return "MallaD3D";
	case CLASE_OBJ3D_CAMARA:
		return "Camara";
	case CLASE_OBJ3D_LUZ:
		return "Luz";
	case CLASE_OBJ3D_SUELO:
		return "Suelo";
	case CLASE_OBJ3D_SISTPART:
		return "SistPart";
	case CLASE_OBJ3D_GRAFICO3D:
		return "Grafico3D";
	case CLASE_OBJ3D_FLARE:
		return "Flare";
	case CLASE_OBJ3D_PORTAL:
		return "Portal";
	default:
		return "otro";
	}
}
