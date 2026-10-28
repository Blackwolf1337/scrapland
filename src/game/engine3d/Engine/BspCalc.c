/*
 * Engine/bsp.cpp of the engine3d, the map compiler (CBsp with [mapa_sombras] calcular, see include/scrap/bsp.h): the
 * lights of the scene, the texels of the lists and the filters of their light maps. The rest of bsp.cpp is in Bsp.c.
 */
#include <float.h>
#include <string.h>

#include "scrap/archio.h"
#include "scrap/bsp.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/luz.h"
#include "scrap/mathlib.h"
#include "scrap/nodo3d.h"
#include "scrap/prof.h"
#include "scrap/scrapmain.h"
#include "scrap/suelo.h"
#include "scrap/sueloportal.h"
#include "scrap/varios.h"

/* 1/255 of the color bytes (0x3b808081) */
#define BSP_INV255 0x1.010102p-8f


/* the lights of the scene */

void Bsp_TomarLuces(CEscena *esc) {
	static const vec3_t omniDir = { 2.0f, -1.0f, 1.0f };

	g_BspNumLuces = 0;
	Con_Verbosef("**** ind shw dsc\n");
	for (CNodo3D *n = CEscena_FindClase(esc, NULL, CLASE_OBJ3D_LUZ); n; n = CEscena_FindClase(esc, n, CLASE_OBJ3D_LUZ)) {
		CLuz *luz = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_LUZ);
		if (!luz->activa) {
			CLuz_SetActiva(luz, 0);
			continue;
		}
		CHECK(g_BspNumLuces < BSP_MAX_LUCES);
		BspLuz *l = &g_BspLuces[g_BspNumLuces++];
		Bprintf(l->nombre, sizeof(l->nombre), "%s", n->name);
		l->puntual = luz->tipo != LUZ_DIREC;
		l->sombra = luz->sombra;
		l->rad = luz->f44;
		l->pos = luz->d3d.Position;
		vec3_t d = luz->tipo == LUZ_OMNI ? omniDir : luz->d3d.Direction;
		float len = (float)__builtin_sqrt((double)(d.x * d.x + d.y * d.y + d.z * d.z));
		if ((double)len != 0.0) {
			float k = (float)(1.0 / (double)len);
			d.x = k * d.x;
			d.y = k * d.y;
			d.z = k * d.z;
		}
		l->dir = d;
		l->cache[0] = 0;
		l->cache[1] = 0;
		l->cache[2] = 0;
		CLuz_FillD3D(luz, &l->ilum.d3d);
		l->ilum.rangeIni = luz->f30;
		l->ilum.d3d.Position = l->pos;
		l->ilum.d3d.Direction = l->dir;
		l->stencil = luz->sombra && n->flags & NODO_SOMBRA;
		l->noLmap = n->flags >> 27 & 1;
		Con_Verbosef(" %d  shw:%d  stencil:%d  lmap:%d  dir:%d  rad:%.2f  rgbai:(%d %d %d %d %.2f)  p:(%.2f %.2f %.2f) "
			"d:(%.2f %.2f %.2f) a:(%.2f %.2f) r:(%.2f %.2f) [%s]\n", g_BspNumLuces - 1, l->sombra, l->stencil, l->noLmap,
			!l->puntual, (double)l->rad, luz->color[0], luz->color[1], luz->color[2], luz->color[3],
			(double)luz->intensidad, (double)l->pos.x, (double)l->pos.y, (double)l->pos.z, (double)l->dir.x,
			(double)l->dir.y, (double)l->dir.z, (double)luz->theta, (double)luz->phi, (double)luz->f30,
			(double)luz->rango, l->nombre);
		if (luz->intensidad < 0.0f) {
			Con_Warnf("Light with negative intensity: %s\n", n->name);
			float m;
			if (esc->ini && (m = CArchINI_GetFloat(esc->ini, "mapa_sombras", "negative_lights_mult", 1.0f)) != 1.0f) {
				float k = luz->intensidad * m * BSP_INV255;
				l->ilum.d3d.Diffuse.r = (float)luz->color[0] * k;
				l->ilum.d3d.Diffuse.g = (float)luz->color[1] * k;
				l->ilum.d3d.Diffuse.b = (float)luz->color[2] * k;
				l->ilum.d3d.Diffuse.a = (float)luz->color[3] * k;
				Con_Warnf(" light color set to: %f %f %f\n", (double)l->ilum.d3d.Diffuse.r, (double)l->ilum.d3d.Diffuse.g,
					(double)l->ilum.d3d.Diffuse.b);
			}
		}
		if (l->stencil && l->noLmap) {
			E3D_LogBlue(g_FileLog, "Stencil light with NO_LIGHTMAP flag: %s", n->name);
		}
		if (l->stencil != (n->flags >> 26 & 1)) {
			E3D_LogBlue(g_FileLog, "Light with RT_SHADOWS flag but no shadow enabled: %s", n->name);
		}
		CLuz_SetActiva(luz, 0);
	}
	uint32_t c = esc->ambiente.rgba;
	float k = esc->ambiente.intens * BSP_INV255;
	Con_Verbosef("AMBIENT: %f %f %f (rgbi = %d %d %d %f)\n", (double)((float)(c & 0xff) * k),
		(double)((float)(c >> 8 & 0xff) * k), (double)((float)(c >> 16 & 0xff) * k), c & 0xff, c >> 8 & 0xff,
		c >> 16 & 0xff, (double)esc->ambiente.intens);
	if (!esc->ini) {
		return;
	}
	vec3_t def = { 0.0f, 0.0f, 0.0f }, tmp;
	vec3_t a = *CArchINI_GetVec3(esc->ini, &tmp, "mapa_sombras", "lightmap_ambient", &def);
	if (a.x != 0.0f || a.y != 0.0f || a.z != 0.0f) {
		ColorIntens ci;
		Bsp_ColorIntens((uint32_t *)&ci, &a.x);
		esc->ambiente = ci;
		k = ci.intens * BSP_INV255;
		Con_Warnf(" AMBIENT color set to: %f %f %f (rgbi = %d %d %d %f)\n", (double)((float)(ci.rgba & 0xff) * k),
			(double)((float)(ci.rgba >> 8 & 0xff) * k), (double)((float)(ci.rgba >> 16 & 0xff) * k), ci.rgba & 0xff,
			ci.rgba >> 8 & 0xff, ci.rgba >> 16 & 0xff, (double)ci.intens);
	}
}


/* the texels of the lists */

void BspTri_Mascara(BspTri *list, CBmp *cob) {
	int x0 = list->ext[0], y0 = list->ext[1], x1 = list->ext[2], y1 = list->ext[3];
	int eje = list->eje;

	for (BspTri *t = list; t; t = t->sig) {
		box_t b = { { FLT_MAX, FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX, -FLT_MAX } };
		int bx0, by0, bx1, by1;

		Box_AddTri(&b, t);
		Bsp_TexelBox(&b, eje, g_MapaSombrasEscala * g_MapaSombrasResol, &bx0, &by0, &bx1, &by1);
		bx0 = bx0 > x0 ? bx0 : x0;
		by0 = by0 > y0 ? by0 : y0;
		bx1 = bx1 < x1 ? bx1 : x1;
		by1 = by1 < y1 ? by1 : y1;
		t->ext2[0] = bx0;
		t->ext2[1] = by0;
		t->ext2[2] = bx1;
		t->ext2[3] = by1;
		for (int y = by0; y <= by1; y++) {
			float fy = (float)y + 0.0f;
			uint32_t ry = (uint32_t)(y - y0);
			for (int x = bx0; x <= bx1; x++) {
				float scale = g_MapaSombrasEscala * g_MapaSombrasResol;
				float fx = (float)x;
				float fxa = fx + 0.0f;
				vec3_t p[4];
				int cov;

				/* fully covered when all four corners are inside, else the weight of the samples */
				Bsp_TexelToWorld(&p[0], fxa, fy, eje, scale, &t->plano);
				byte dentro = Suelo_PointInTri(&p[0], &t->v[0], &t->v[1], &t->v[2]);
				if (dentro) {
					float fx1 = fx + 1.0f;
					float fy1 = (float)y + 1.0f;
					Bsp_TexelToWorld(&p[1], fx1, fy, eje, scale, &t->plano);
					Bsp_TexelToWorld(&p[2], fxa, fy1, eje, scale, &t->plano);
					Bsp_TexelToWorld(&p[3], fx1, fy1, eje, scale, &t->plano);
					dentro = Suelo_PointInTri(&p[1], &t->v[0], &t->v[1], &t->v[2]) &&
						Suelo_PointInTri(&p[2], &t->v[0], &t->v[1], &t->v[2]) &&
						Suelo_PointInTri(&p[3], &t->v[0], &t->v[1], &t->v[2]);
				}
				if (dentro) {
					cov = 0xff;
				} else {
					vec3_t m;
					float peso;
					BspTri_Muestra(&m, x, y, eje, scale, t, NULL, &peso);
					cov = (int)(peso * 255.0f);
				}
				uint32_t rx = (uint32_t)(x - x0);
				if (rx >= (uint32_t)g_MapaSombrasTexX || ry >= (uint32_t)g_MapaSombrasTexY) {
					continue;
				}
				uint32_t c = *CBmp_Pixel(cob, rx, ry);
				if ((byte)cov < (byte)c) {
					continue;
				}
				*CBmp_Pixel(cob, rx, ry) = (c & ~0xffu) | (byte)cov;
				/* the mask holds the triangle itself */
				*CBmp_Pixel(&list->mask, rx, ry) = Color_SwapRB((uint32_t)(uintptr_t)t);
				if (list->mask2.img) {
					*CBmp_Pixel(&list->mask2, rx, ry) = Color_SwapRB((uint32_t)t->i12c);
				}
			}
		}
	}
}

/* a vertex of the lists while they are made */
typedef struct BspVert {
	vec3_t pos;			/* 0x00 */
	vec3_t n;			/* 0x0c in steps of 1/1000 */
	uint32_t color;			/* 0x18 */
	float uv[2];			/* 0x1c */
	float uvl[2];			/* 0x24 */
} BspVert;
SD_SIZE(BspVert, 0x2c);

static inline float Bsp_Cuantizar(float v) {
	return (float)(int32_t)X87_Fistp((double)v * 1000.0) * 0.001f;
}

void __fastcall CBsp_CrearBuffers(CBsp *self) {
	/* the lists of the same material, page and sector joined while they stay below 4096 triangles */
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *a = self->listas[i];
		BspTri *ult = a;
		while (ult->sig) {
			ult = ult->sig;
		}
		for (int j = i + 1; j < self->num_listas;) {
			BspTri *b = self->listas[j];
			if (a->Mat != b->Mat || a->LMap != b->LMap || a->zona != b->zona) {
				j++;
				continue;
			}
			CHECK(!((b->flags ^ a->flags) & BSPTRI_NOLMAP));
			if (BspTri_Count(a) + BspTri_Count(b) >= 0x1000) {
				j++;
				continue;
			}
			ult->sig = b;
			while (ult->sig) {
				ult = ult->sig;
			}
			self->listas[j] = self->listas[self->num_listas - 1];
			self->num_listas--;
		}
	}
	crt_qsort(self->listas, (size_t)self->num_listas, sizeof(BspTri *),
		(int (*)(const void *, const void *))Game_Ptr(0x28e520));
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *l = self->listas[i];
		E3D_LogPrint(g_FileLog, "%4d %-40s (%p)  lmap %02d  zona %02d  tri %d", i, l->Mat->Nombre, (void *)l->Mat,
			l->LMap ? l->LMap->id : -1, l->zona, BspTri_Count(l));
		E3D_LogPrint(g_FileLog, "     [%s]", CMaterial_GetMapName(self->listas[i]->Mat, MAPA_DIFFUSE));
	}
	/* the vertices of each list without the repeated ones */
	int caras = 0, verts = 0;
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *list = self->listas[i];
		E3D_DebugPrintf("opt. buffer:  %d / %d  \r", i + 1, self->num_listas);
		int n = BspTri_Count(list);
		CHECK(!list->pTri);
		uint16_t *idx = New_Array((uint32_t)n, 6);
		list->numTri = n;
		list->pTri = idx;
		CTabla tab = { 0, NULL };
		for (BspTri *t = list; t; t = t->sig, idx += 3) {
			for (int k = 0; k < 3; k++) {
				BspVert e;
				e.pos = t->v[k];
				e.n = (vec3_t){ 0.0f, 0.0f, 0.0f };
				if (list->Mat->params.b3b) {
					e.n.x = Bsp_Cuantizar(t->n[k].x);
					e.n.y = Bsp_Cuantizar(t->n[k].y);
					e.n.z = Bsp_Cuantizar(t->n[k].z);
				}
				e.color = t->color[k];
				e.uv[0] = t->uv[k][0];
				e.uv[1] = t->uv[k][1];
				e.uvl[0] = t->uvl[k][0];
				e.uvl[1] = t->uvl[k][1];
				if (list->flags & BSPTRI_NOLMAP) {
					e.uvl[0] = 0.0f;
					e.uvl[1] = 0.0f;
				}
				/* the newest vertices first */
				int nv = tab.n;
				for (int s = 0; s < tab.n; s++) {
					int ix = tab.n - 1 - s;
					TABLA_CHECK(tab.p, ix, tab.n);
					const BspVert *o = &((const BspVert *)tab.p)[ix];
					if (o->pos.x == e.pos.x && o->pos.y == e.pos.y && o->pos.z == e.pos.z && o->n.x == e.n.x &&
						o->n.y == e.n.y && o->n.z == e.n.z && o->uv[0] == e.uv[0] && o->uv[1] == e.uv[1] &&
						o->uvl[0] == e.uvl[0] && o->uvl[1] == e.uvl[1] && o->color == e.color) {
						nv = ix;
						break;
					}
				}
				if (nv == tab.n) {
					Tabla_Resize2C(&tab, nv + 1);
					TABLA_CHECK(tab.p, nv, tab.n);
					((BspVert *)tab.p)[nv] = e;
				}
				CHECK(nv <= 65535);
				idx[k] = (uint16_t)nv;
			}
		}
		byte normal = list->Mat->params.b3b;
		int frmt = normal ? VERT_LTEX2N : VERT_LTEX2;
		if (list->flags & BSPTRI_NOLMAP) {
			frmt = normal ? VERT_LTEX1N : VERT_LTEX1;
		}
		CListaVert *lv = &list->lv;
		CListaVert_SetFormat(lv, frmt);
		CListaVert_Alloc(lv, tab.n, 0);
		for (int v = 0; v < tab.n; v++) {
			TABLA_CHECK(tab.p, v, tab.n);
			const BspVert *e = &((const BspVert *)tab.p)[v];
			*(vec3_t *)CListaVert_GetField(lv, LVERT_POS, (uint32_t)v) = e->pos;
			vec3_t *pn = CListaVert_GetField(lv, LVERT_NORMAL, (uint32_t)v);
			if (pn) {
				*pn = e->n;
			}
			uint32_t *pc = CListaVert_GetField(lv, LVERT_DIFFUSE, (uint32_t)v);
			if (pc) {
				*pc = e->color;
			}
			uint32_t *ps = CListaVert_GetField(lv, LVERT_SPECULAR, (uint32_t)v);
			if (ps) {
				*ps = 0;
			}
			float *pt = CListaVert_GetField(lv, LVERT_TEX2D_SET1, (uint32_t)v);
			if (pt) {
				pt[0] = e->uv[0];
				pt[1] = e->uv[1];
			}
			float *pl = CListaVert_GetField(lv, LVERT_TEX2D_SET1 + 1, (uint32_t)v);
			if (pl) {
				pl[0] = e->uvl[0];
				pl[1] = e->uvl[1];
			}
		}
		caras += n;
		verts += lv->nVert;
		if (tab.p) {
			operator_delete_array(tab.p);
		}
	}
	E3D_DebugPrintf("\ncrear_buffers():   listas: %d   caras: %d   vert: %d\n", self->num_listas, caras, verts);
}


/* the direct light of the lights of g_BspLuces */

#define g_LightmapLinealAtten GAME_DATA(int, 0x4d5d20)	/* gvar */

/* the segment from c (moved back along the light direction d) to the light at dest blocked by the floor */
static byte Bsp_Sombra(CSuelo *suelo, const vec3_t *c, const vec3_t *d, const vec3_t *dest, vec3_t *hit, int *cache) {
	float off = 0.3f / g_MapaSombrasEscala;
	vec3_t from = { c->x - off * d->x, c->y - off * d->y, c->z - off * d->z };

	return CSuelo_Trace(suelo, &from, dest, hit, 0.8f / (g_MapaSombrasEscala * g_MapaSombrasResol), cache);
}

static inline float Bsp_Dist2(const vec3_t *a, const vec3_t *b) {
	float dy = a->y - b->y, dx = a->x - b->x, dz = a->z - b->z;

	return dy * dy + dx * dx + dz * dz;
}

/* the light maps of a list: luz all lights, luz2 the stencil ones, rad2 each times its rad */
static void BspTri_LuzDirecta(BspTri *list, CEscena *esc, CSuelo *suelo, float diag) {
	static const float sinCalc[3] = { 1.0f, 0.0f, 1.0f };
	float colors[17];
	int eje = list->eje;

	CMaterial_CalcColors(list->Mat, colors);
	float k = esc->ambiente.intens * BSP_INV255;
	float ar = (float)(esc->ambiente.rgba & 0xff) * k;
	float ag = (float)(esc->ambiente.rgba >> 8 & 0xff) * k;
	float ab = (float)(esc->ambiente.rgba >> 16 & 0xff) * k;
	if (g_MapaSombrasNoLuzAmb) {
		ar = 0.0f;
		ag = 0.0f;
		ab = 0.0f;
	}
	float br = ar * colors[4] + colors[12];
	float bg = ag * colors[5] + colors[13];
	float bb = ab * colors[6] + colors[14];
	CBmpFlt2_CreateFill(&list->luz, list->lx, list->ly, sinCalc);
	CBmpFlt2_CreateFill(&list->luz2, list->lx, list->ly, sinCalc);
	CBmpFlt2_Assign(&list->rad2, &list->luz);
	int x0 = list->ext[0], y0 = list->ext[1];
	int nTri = BspTri_Count(list);
	int k3 = 0;
	for (BspTri *t = list; t; t = t->sig, k3++) {
		byte ok = 1;
		if (t->flags & BSPTRI_NOLMAP) {
			ok = 0;
			CHECK(0);
		} else {
			E3D_DebugPrintf("tri: %d / %d  \r", k3 + 1, nTri);
		}
		int tx0 = t->ext2[0], ty0 = t->ext2[1], tx1 = t->ext2[2], ty1 = t->ext2[3];
		for (int y = ty0; y <= ty1 && ok; y++) {
			uint32_t ry = (uint32_t)(y - y0);
			for (int x = tx0; x <= tx1; x++) {
				float a1[4] = { br, bg, bb, 0.0f };
				float a2[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
				float a3[4] = { br, bg, bb, 0.0f };
				uint32_t rx = (uint32_t)(x - x0);
				if (rx >= (uint32_t)g_MapaSombrasTexX || ry >= (uint32_t)g_MapaSombrasTexY) {
					continue;
				}
				if (Color_SwapRB(*CBmp_Pixel(&list->mask, rx, ry)) != (uint32_t)(uintptr_t)t) {
					continue;
				}
				vec3_t p, n;
				float scale = g_MapaSombrasEscala * g_MapaSombrasResol;
				Bsp_TexelToWorld(&p, (float)x + 0.5f, (float)y + 0.5f, eje, scale, &t->plano);
				BspTri_Interp(t, &p, &n, NULL, 0);
				for (int li = 0; li < g_BspNumLuces; li++) {
					BspLuz *l = &g_BspLuces[li];
					float w = 1.0f;
					if (l->noLmap) {
						continue;
					}
					color4_t c = { 0.0f, 0.0f, 0.0f, 1.0f };
					if (Ilum_Light(&c, &p, &n, colors, &l->ilum, g_LightmapLinealAtten != 0)) {
						continue;
					}
					if (l->sombra && !(t->nodo->flags >> 6 & 1)) {
						vec3_t hit = p;
						vec3_t *phit = g_MapaSombrasAten != 0.0f ? &hit : NULL;
						float d2 = 0.0f;
						byte puntual = l->puntual;
						vec3_t d, dest;
						if (puntual) {
							float dx = p.x - l->pos.x, dy = p.y - l->pos.y, dz = p.z - l->pos.z;
							float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
							if ((double)len != 0.0) {
								float kl = (float)(1.0 / (double)len);
								d = (vec3_t){ kl * dx, kl * dy, kl * dz };
							} else {
								d = (vec3_t){ dx, dy, dz };
							}
							dest = l->pos;
						} else {
							d = l->dir;
							dest = (vec3_t){ p.x - d.x * diag, p.y - d.y * diag, p.z - d.z * diag };
						}
						if (Bsp_Sombra(suelo, &p, &d, &dest, phit, l->cache)) {
							w = 0.0f;
							d2 = Bsp_Dist2(&p, &hit);
						}
						if (g_MapaSombrasSuaves) {
							/* the four corners of the texel, then the occlusion of the samples around p */
							vec3_t q[4];
							float fx = (float)x + 0.0f, fy = (float)y + 0.0f;
							float fx1 = (float)x + 1.0f, fy1 = (float)y + 1.0f;
							Bsp_TexelToWorld(&q[0], fx, fy, eje, scale, &t->plano);
							Bsp_TexelToWorld(&q[1], fx1, fy, eje, scale, &t->plano);
							Bsp_TexelToWorld(&q[2], fx, fy1, eje, scale, &t->plano);
							Bsp_TexelToWorld(&q[3], fx1, fy1, eje, scale, &t->plano);
							for (int i = 0; i < 4; i++) {
								if (Bsp_Sombra(suelo, &q[i], &d, &dest, phit, l->cache)) {
									d2 = Bsp_Dist2(&p, &hit);
								}
							}
							CHECK(d2 == 0.0f);
							w = Bsp_Oclusion(&p, &n, 0.8f / (g_MapaSombrasEscala * g_MapaSombrasResol), 400, &dest, suelo);
						}
						if (d2 > 0.0f) {
							float s = (float)__builtin_sqrt((double)d2) * g_MapaSombrasEscala * g_MapaSombrasAten;
							w = (float)(1.0 - 1.0 / ((double)s + 1.0) * (1.0 - (double)w));
						}
					}
					float cr = c.r * w, cg = c.g * w, cb = c.b * w, ca = c.a * w;
					if (l->stencil) {
						a2[0] = a2[0] + cr;
						a2[1] = cg + a2[1];
						a2[2] = cb + a2[2];
						a2[3] = ca + a2[3];
					}
					a1[0] = a1[0] + cr;
					a1[1] = cg + a1[1];
					a1[2] = cb + a1[2];
					a1[3] = a1[3] + ca;
					a3[0] = l->rad * cr + a3[0];
					a3[1] = l->rad * cg + a3[1];
					a3[2] = l->rad * cb + a3[2];
					a3[3] = l->rad * ca + a3[3];
				}
				for (int i = 0; i < 3; i++) {
					if (0.0f > a1[i]) {
						a1[i] = 0.0f;
					}
				}
				for (int i = 0; i < 3; i++) {
					if (0.0f > a2[i]) {
						a2[i] = 0.0f;
					}
				}
				for (int i = 0; i < 3; i++) {
					if (0.0f > a3[i]) {
						a3[i] = 0.0f;
					}
				}
				memcpy(CBmpFlt2_Pixel(&list->luz, rx, ry), a1, 12);
				memcpy(CBmpFlt2_Pixel(&list->luz2, rx, ry), a2, 12);
				memcpy(CBmpFlt2_Pixel(&list->rad2, rx, ry), a3, 12);
			}
		}
	}
	if (!g_CalcLmap2) {
		Img_Free(&list->luz2, 0xc);
	}
	if (!g_MapaSombrasIlumGlobal) {
		Img_Free(&list->rad2, 0xc);
	}
}

void __fastcall CBsp_LuzDirecta(CBsp *self) {
	CEscena *esc = self->modelo->escena;
	CSuelo *suelo = Mundo_Suelo();
	vec3_t dim = suelo->Dim;
	float diag = (float)__builtin_sqrt((double)(dim.y * dim.y + dim.x * dim.x + dim.z * dim.z));
	diag = diag + diag;
	BspTxf txf[3];

	BspTxf_ctor(&txf[0], self->nombre, "dir", 0, 1);
	BspTxf_ctor(&txf[1], self->nombre, "dir", 1, g_CalcLmap2);
	BspTxf_ctor(&txf[2], self->nombre, "dir", 9, g_MapaSombrasIlumGlobal);
	/* again while the caches find lists to compute once more */
	while (txf[0].rebuild || txf[1].rebuild || txf[2].rebuild) {
		txf[0].rebuild = 0;
		txf[1].rebuild = 0;
		txf[2].rebuild = 0;
		for (int i = 0; i < self->num_listas; i++) {
			BspTri *list = self->listas[i];
			if (g_TxfMode) {
				E3D_DebugPrintf("Check TXF active, skipping...   \n");
				break;
			}
			E3D_DebugPrintf("direct: %d / %d   ", i + 1, self->num_listas);
			E3D_DebugPrintf("%.1f Mb   ", (double)g_MemStats->bytes * 0.0009765625 * 0.0009765625);
			byte c0 = BspTxf_Check(&txf[0], i, 1, 0);
			byte c1 = BspTxf_Check(&txf[1], i, 0, 0);
			byte c2 = BspTxf_Check(&txf[2], i, 0, 0);
			E3D_DebugPrintf("\n");
			if (!c0 && !c1 && !c2) {
				continue;
			}
			BspTri_LuzDirecta(list, esc, suelo, diag);
			BspTxf_Guardar(&txf[0], &list->luz);
			BspTxf_Guardar(&txf[1], &list->luz2);
			BspTxf_Guardar(&txf[2], &list->rad2);
		}
		BspTxf_Cargar(&txf[0], self->listas, self->num_listas, 1);
		BspTxf_Cargar(&txf[1], self->listas, self->num_listas, 2);
		BspTxf_Cargar(&txf[2], self->listas, self->num_listas, 4);
	}
	BspTxf_dtor(&txf[2]);
	BspTxf_dtor(&txf[1]);
	BspTxf_dtor(&txf[0]);
}


/* the .EMI file of the map, the counterpart of CBsp_LeerEMI (version 105) */

int __fastcall CBsp_GuardarEMI(CBsp *self) {
	char ruta[0x104], buf[0x104];
	CTablaHash tabMat;
	CTablaHashNodo **elems;
	int num;

	Ruta_Merge(ruta, ".emi", self->nombre);
	CArchIO *mem = operator_new(sizeof(CArchIO));
	CArchIO *f = mem ? CArchIO_ctor(mem, ruta, "wb") : NULL;
	if (!f->error) {
		CArchIO_BeginBlockWrite(f, "EMI");
		CArchIO_WriteInt(f, 105);
		/* the materials once each, the key is the pointer */
		CTablaHash_ctor(&tabMat, 0x1f);
		for (int i = 0; i < self->num_listas; i++) {
			CMaterial *mat = self->listas[i]->Mat;
			CTablaHash_AddInt(&tabMat, (int)(intptr_t)mat, mat, 0);
		}
		CTablaHash_GetList(&tabMat, &elems, &num);
		CArchIO_WriteInt(f, num);
		for (int i = 0; i < num; i++) {
			CMaterial *mat = elems[i]->value;
			CArchIO_WriteInt(f, (int32_t)(intptr_t)mat);
			CMaterial_Write(mat, f);
		}
		CTablaHash_FreeList(&tabMat, &elems);
		CTablaHash_dtor(&tabMat);
		/* the pages: key, file name, 1 and the file name of the secondary page */
		for (BspLMap *lm = g_BspLMaps; lm; lm = lm->sig) {
			CArchIO_WriteInt(f, (int32_t)(intptr_t)lm);
			CMapa *mapa = lm->Mat->mapa[0];
			CHECK(mapa && mapa->Nombre);
			Ruta_Part(buf, mapa->Nombre, 0xc);
			CArchIO_WriteString(f, buf);
			CArchIO_WriteInt(f, 1);
			mapa = lm->lmap2->Mat->mapa[0];
			CHECK(mapa && mapa->Nombre);
			Ruta_Part(buf, mapa->Nombre, 0xc);
			CArchIO_WriteString(f, buf);
		}
		CArchIO_WriteInt(f, 0);
		CArchIO_WriteInt(f, self->num_listas);
		for (int i = 0; i < self->num_listas; i++) {
			BspTri *tri = self->listas[i];
			const char *sector;
			CArchIO_BeginBlockWrite(f, "TRI");
			CArchIO_WriteInt(f, (int32_t)tri->flags);
			CArchIO_WriteString(f, tri->nodo->name);
			CArchIO_WriteInt(f, tri->zona);
			CHECK(tri->zona == BspTri_Zona(tri->nodo));
			if (!Sector_Num()) {
				sector = "_noportals_";
			} else if (tri->nodo->flags >> 28 & 1) {
				sector = "_nosector_";
			} else {
				sector = BspTri_ZonaName(tri);
			}
			CArchIO_WriteString(f, sector);
			CArchIO_WriteInt(f, (int32_t)(intptr_t)tri->Mat);
			CArchIO_WriteInt(f, (int32_t)(intptr_t)tri->LMap);
			CHECK(tri->pTri);
			CArchIO_WriteInt(f, tri->numTri);
			CArchIO_Write(f, tri->pTri, tri->numTri * 6);
			CListaVert_Write(&tri->lv, f);
			CListaVert_Write(&tri->lv2, f);
			CArchIO_EndBlockWrite(f, "TRI");
		}
		CArchIO_EndBlockWrite(f, "EMI");
		if (!f->error) {
			CArchIO_dtor(f);
			operator_delete_sized(f, sizeof(CArchIO));
			return 0;
		}
	}
	Err_Report(f->err, 0, "\n'%s'", ruta);
	CArchIO_dtor(f);
	operator_delete_sized(f, sizeof(CArchIO));
	return -1;
}


/* the radiosity */

/* the area light of the material of a list, read into the list (0x134..0x153) */
static inline RadAreaLuz *BspTri_AreaLuz(BspTri *t) {
	return (RadAreaLuz *)&t->nombres;
}

/* the world point of the texel x, y of t in list: the center for the large lists, else the samples of t */
static void BspTri_RadPunto(vec3_t *p, const BspTri *list, const BspTri *t, int x, int y) {
	vec3_t tmp;
	float sc = g_MapaSombrasEscala * g_MapaSombrasResol;

	if (list->grande) {
		Bsp_TexelToWorld(p, (float)x + 0.5f, (float)y + 0.5f, list->eje, sc, &t->plano);
	} else {
		*p = *BspTri_Muestra(&tmp, x, y, list->eje, sc, t, list, NULL);
	}
}

/* the side of q of the plane of s; a q behind it (below k) is moved to k in front into e */
static float BspTri_RadMover(vec3_t *e, const BspTri *s, const vec3_t *q, float k) {
	float dist = s->plano.n.y * q->y + q->x * s->plano.n.x + s->plano.n.z * q->z + s->plano.d;

	if (k > dist) {
		float m = k - dist;
		e->x = q->x + s->plano.n.x * m;
		e->y = q->y + s->plano.n.y * m;
		e->z = q->z + s->plano.n.z * m;
	} else {
		*e = *q;
	}
	return dist;
}

/* a corner of s is in front of the plane n, dn of the texel (above lim) */
static byte BspTri_RadDelante(const BspTri *s, const vec3_t *n, float dn, float lim) {
	return !(lim >= s->v[0].y * n->y + n->x * s->v[0].x + s->v[0].z * n->z + dn &&
		lim >= s->v[1].y * n->y + n->x * s->v[1].x + s->v[1].z * n->z + dn &&
		lim >= s->v[2].y * n->y + n->x * s->v[2].x + s->v[2].z * n->z + dn);
}

/* the distance of e to the sphere of s, at least dist */
static float BspTri_RadLejos(const BspTri *s, const vec3_t *e, float dist) {
	float cx = e->x - s->centro.x;
	float cy = e->y - s->centro.y;
	float cz = e->z - s->centro.z;
	float dc = (float)__builtin_sqrt((double)(cx * cx + cy * cy + cz * cz));

	return Float_MaxSS(dc - s->radio, dist);
}

/* the light path from an area at pos to the texel point e, d = e - pos: the cosines at the texel (normal n) and at the
 * area (the plane of s), the squared length and the falloff with distance; 0 when one side faces away */
typedef struct BspRadRayo {
	vec3_t d;
	float ca, cb, d2, k, den;
} BspRadRayo;

static int BspRad_Rayo(BspRadRayo *r, const vec3_t *e, const vec3_t *pos, const vec3_t *n, const BspTri *s) {
	r->d.x = e->x - pos->x;
	r->d.y = e->y - pos->y;
	r->d.z = e->z - pos->z;
	r->ca = -(n->x * r->d.x + n->y * r->d.y + n->z * r->d.z);
	if (0.001 > (double)r->ca) {
		return 1;
	}
	r->cb = s->plano.n.y * r->d.y + s->plano.n.x * r->d.x + s->plano.n.z * r->d.z;
	if (0.001 > (double)r->cb) {
		return 2;
	}
	r->d2 = r->d.x * r->d.x + r->d.y * r->d.y + r->d.z * r->d.z;
	r->k = Float_MaxSS(g_BspRadMinD2, r->d2) * 0.2f * (g_MapaSombrasEscala * g_MapaSombrasEscala);
	r->den = r->k * r->d2;
	return 0;
}

/* the segment of the path of r from near the texel point e (b) to just off the area at pos (a) */
static void BspRad_Segmento(vec3_t *a, vec3_t *b, const BspRadRayo *r, const vec3_t *e, const vec3_t *pos) {
	vec3_t u;
	float len = (float)__builtin_sqrt((double)r->d2);

	if ((double)len != 0.0) {
		float inv = (float)(1.0 / (double)len);
		u.x = inv * r->d.x;
		u.y = inv * r->d.y;
		u.z = inv * r->d.z;
	} else {
		u = r->d;
	}
	float k3 = 0.3f / g_MapaSombrasEscala;
	a->x = u.x * k3 * 0.01f + pos->x;
	a->y = u.y * k3 * 0.01f + pos->y;
	a->z = u.z * k3 * 0.01f + pos->z;
	b->x = e->x - u.x * k3;
	b->y = e->y - u.y * k3;
	b->z = e->z - u.z * k3;
}

/* the floor hides the segment */
static inline byte BspRad_Oculto(CSuelo *suelo, const vec3_t *a, const vec3_t *b) {
	return CSuelo_Trace(suelo, b, a, NULL, 0.8f / (g_MapaSombrasEscala * g_MapaSombrasResol), NULL);
}

/* the start of a list of the radiosity passes: the count and %, then the TXF check */
static byte BspRad_Lista(BspTxf *txf, const char *fmt, int pass, int i, int n, int *nAreas, const BspTri *list) {
	*nAreas += list->numAreas;
	if (pass < 0) {
		E3D_DebugPrintf(fmt, i + 1, n);
	} else {
		E3D_DebugPrintf(fmt, pass, i + 1, n);
	}
	E3D_DebugPrintf("areas: %-5d   ", list->numAreas);
	E3D_DebugPrintf("%.1f %%   ", (double)*nAreas * 100.0 / (double)g_BspAreasTotal[0]);
	if (!BspTxf_Check(txf, i, 1, 1)) {
		return 0;
	}
	E3D_DebugPrintf("\n");
	return 1;
}

static const float bspMagenta[3] = { 1.0f, 0.0f, 1.0f };

/* the global light: per bounce, every texel of the lists gathers the light of the areas of all other lists (rad2, the
 * last bounce) in front of it that the floor (and the portals) do not hide; the result into rad, with the TXF cache.
 * At the end the statistics and rad added to the light maps (luz) */
void __fastcall CBsp_RadGlobal(CBsp *self) {
	CSuelo *suelo = Mundo_Suelo();
	float mc[17];
	vec3_t p, n, e, a, b;
	BspRadRayo r;
	BspTxf txf;
	int64_t sumAreas = 0, sumSel = 0;
	int nNoArea = 0, nDetras = 0, nNo = 0, nSombra = 0, nSector = 0, nComp = 0;

	if (g_MapaSombrasIlumGlobal) {
		for (int bounce = 0; bounce < g_MapaSombrasRebotes; bounce++) {
			E3D_DebugPrintf("total areas: %d %d %d %d\n", g_BspAreasTotal[0], g_BspAreasTotal[1], g_BspAreasTotal[2],
				g_BspAreasTotal[3]);
			E3D_DebugPrintf("memoria usada: %-5d Kb\n", g_MemStats->bytes / 1024);
			/* rad2 is the light the areas send: the direct light, then the last bounce */
			for (int i = 0; i < self->num_listas; i++) {
				BspTri *tri = self->listas[i];
				CBmpFlt2_Blend(&tri->luz, 0, 0, &tri->rad, 0, 0, tri->luz.ancho, tri->luz.alto, 0x100, 0x100);
				CBmpFlt2_Blend(&tri->rad2, 0, 0, &tri->rad, 0, 0, tri->rad2.ancho, tri->rad2.alto, 0x100,
					bounce ? 0 : 0x100);
			}
			CBsp_AreasGlow(self);
			CBsp_AreasMax(self);
			g_ProfTicks[13] -= __builtin_ia32_rdtsc();
			BspTxf_ctor(&txf, self->nombre, "rad", bounce, 1);
			int nAreas = 0;
			while (txf.rebuild) {
				txf.rebuild = 0;
				for (int i = 0; i < self->num_listas; i++) {
					BspTri *list = self->listas[i];
					if (g_TxfMode) {
						E3D_DebugPrintf("Check TXF active, skipping...   \n");
						break;
					}
					if (!BspRad_Lista(&txf, "global (%d): %d / %d   ", bounce, i, self->num_listas, &nAreas, list)) {
						continue;
					}
					int zona = list->zona;
					CMaterial_CalcColors(list->Mat, mc);
					CBmpFlt2_Fill(&list->rad, bspMagenta);
					int lx0 = list->ext[0];
					int ly0 = list->ext[1];
					int count = BspTri_Count(list);
					int k = 0;
					for (BspTri *t = list; t; t = t->sig) {
						k++;
						E3D_DebugPrintf("tri: %d / %d   ", k, count);
						E3D_DebugPrintf("areas: %d  \r", t->areas[0].n);
						CHECK(!(t->flags & BSPTRI_NOLMAP));
						for (int y = t->ext2[1]; y <= t->ext2[3]; y++) {
							for (int x = t->ext2[0]; x <= t->ext2[2]; x++) {
								uint32_t tx = (uint32_t)(x - lx0);
								uint32_t ty = (uint32_t)(y - ly0);
								float sr = 0.0f, sg = 0.0f, sb = 0.0f;
								if (tx >= (uint32_t)g_MapaSombrasTexX || ty >= (uint32_t)g_MapaSombrasTexY) {
									continue;
								}
								if (Color_SwapRB(*CBmp_Pixel(&list->mask, tx, ty)) != (uint32_t)(uintptr_t)t) {
									continue;
								}
								BspTri_RadPunto(&p, list, t, x, y);
								BspTri_Interp(t, &p, &n, NULL, 0);
								float dn = -(n.y * p.y + n.x * p.x + n.z * p.z);
								for (int j = 0; j < self->num_listas && !list->radluz_ok; j++) {
									BspTri *L = self->listas[j];
									if (L->radluz_ok) {
										continue;
									}
									CHECK(!(L->flags & BSPTRI_NOLMAP));
									int lzona = L->zona;
									if (g_RadSkipBySectors && lzona != zona) {
										continue;
									}
									for (BspTri *S = L; S; S = S->sig) {
										if (S == t || !S->areas[0].n || S->f178 == 0.0f) {
											continue;
										}
										float lod = g_BspLodLevel;
										float dist = BspTri_RadMover(&e, S, &p, lod * 0.25f);
										if (-lod > dist || !BspTri_RadDelante(S, &n, dn, lod * 0.02f)) {
											continue;
										}
										g_ProfTicks[14] -= __builtin_ia32_rdtsc();
										CTabla *sel = BspTri_GetAreas(S, BspTri_RadLejos(S, &e, dist), 1);
										sumAreas += S->areas[0].n;
										sumSel += sel->n;
										for (int ia = 0; ia < sel->n; ia++) {
											TABLA_CHECK(sel->p, ia, sel->n);
											BspArea *ar = &((BspArea *)sel->p)[ia];
											int no = BspRad_Rayo(&r, &e, &ar->pos, &n, S);
											if (no) {
												if (no == 1) {
													nNoArea++;
												} else {
													nDetras++;
												}
												continue;
											}
											float cc = r.cb * r.ca;
											if (!((double)(ar->f14 * cc) > (double)r.den * 1.25e-06)) {
												nNo++;
												continue;
											}
											nComp++;
											/* the segment from near the texel to just off the area */
											g_ProfTicks[11] -= __builtin_ia32_rdtsc();
											BspRad_Segmento(&a, &b, &r, &e, &ar->pos);
											if (g_RadiosityPortals && lzona != zona && zona >= 0 && lzona >= 0 &&
												Sector_Recorrer(zona, &b, &a, 0) != lzona) {
												g_ProfTicks[11] += __builtin_ia32_rdtsc();
												nSombra++;
												nSector++;
												continue;
											}
											if (BspRad_Oculto(suelo, &a, &b)) {
												g_ProfTicks[11] += __builtin_ia32_rdtsc();
												nSombra++;
												continue;
											}
											g_ProfTicks[11] += __builtin_ia32_rdtsc();
											const float *px = CBmpFlt2_Pixel(&L->rad2, ar->x, ar->y);
											float f = ar->peso * cc / r.den * g_MapaSombrasCoefRad;
											sr = mc[0] * px[0] * f + sr;
											sg = mc[1] * px[1] * f + sg;
											sb = mc[2] * px[2] * f + sb;
										}
										g_ProfTicks[14] += __builtin_ia32_rdtsc();
									}
								}
								float *dst = CBmpFlt2_Pixel(&list->rad, tx, ty);
								dst[0] = sr;
								dst[1] = sg;
								dst[2] = sb;
							}
						}
					}
					BspTxf_Guardar(&txf, &list->rad);
				}
				BspTxf_Cargar(&txf, self->listas, self->num_listas, 3);
			}
			g_ProfTicks[13] += __builtin_ia32_rdtsc();
			BspTxf_dtor(&txf);
		}
	}
	g_ProfTicks[10] += __builtin_ia32_rdtsc();
	E3D_DebugPrintf("\ntotal areas: %d %d %d %d", g_BspAreasTotal[0], g_BspAreasTotal[1], g_BspAreasTotal[2],
		g_BspAreasTotal[3]);
	E3D_DebugPrintf("\nmemoria usada: %-5d Kb", g_MemStats->bytes / 1024);
	E3D_DebugPrintf("\nradiosidad, tsc(10): %.2f  (local+global)", (double)(uint64_t)Prof_Reset(10) / 450000.0);
	E3D_DebugPrintf("\nradiosidad, tsc(11): %.2f  (sombras global)", (double)(uint64_t)Prof_Reset(11) / 450000.0);
	E3D_DebugPrintf("\nradiosidad, tsc(12): %.2f  (buscar_CTriLMap)", (double)(uint64_t)Prof_Reset(12) / 450000.0);
	E3D_DebugPrintf("\nradiosidad, tsc(13): %.2f  (global)", (double)(uint64_t)Prof_Reset(13) / 450000.0);
	E3D_DebugPrintf("\nradiosidad, tsc(14): %.2f  (global)", (double)(uint64_t)Prof_Reset(14) / 450000.0);
	double pot = (double)(uint64_t)sumAreas;
	E3D_DebugPrintf("\npotenciales: %.1f  descartados: %d %d %d %d %d %d", pot, 0, nNoArea, nDetras, nNo, nSombra,
		nSector);
	double pot2 = (double)(uint64_t)sumSel;
	E3D_DebugPrintf("\npotenciales2: %.1f (%.1f%%)", pot2, pot2 * 100.0 / pot);
	E3D_DebugPrintf("\nselec_rad_area: %d %d %d %d", g_BspLodCount[0], g_BspLodCount[1], g_BspLodCount[2],
		g_BspLodCount[3]);
	E3D_DebugPrintf("\nesc: %.3f  res: %.3f  umbral (x1e6): %.3f  gamma: %.3f", (double)g_MapaSombrasEscala,
		(double)g_MapaSombrasResol, 1.25, (double)g_MapaSombrasGamma);
	E3D_DebugPrintf("\nareas2:   2x2 -> %.1f/%.1f   4x4 -> %.1f/%.1f   8x8 -> %.1f/%.1f", (double)g_BspLodDist[0],
		(double)g_BspLodDist[1], (double)g_BspLodDist[2], (double)g_BspLodDist[3], (double)g_BspLodDist[4],
		(double)g_BspLodDist[5]);
	E3D_DebugPrintf("\nilum. global:  { comp: %d  ->  vis: %d  no: %d }  no: %d", nComp, nComp - nSombra, nSombra,
		nNo);
	E3D_DebugPrintf("\n");
	if (g_TxfMode && g_TxfMode != 3) {
		E3D_LogPrint(g_FileLog, "TFX checking finished");
		Err_Printf(0, "TFX checking finished");
		crt_exit(-1);
	}
	for (int i = 0; i < self->num_listas; i++) {
		BspTri *tri = self->listas[i];
		CBmpFlt2_Blend(&tri->luz, 0, 0, &tri->rad, 0, 0, tri->luz.ancho, tri->luz.alto, 0x100, 0x100);
		Img_Free(&tri->rad, 0xc);
		Img_Free(&tri->rad2, 0xc);
	}
}

/* the color and intensity of the m3d.ini line "r g b i" (CHECKed) into c */
static void BspRad_LeerColor(const char *buf, byte *c, float *intens) {
	CHECK(Proc_Scan(buf, "&b&b&b&f", &c[0], &c[1], &c[2], intens) == 4);
}

/* the light of the area lights: the lists with an area light ("(+area_light: name)" materials, or the sections
 * mats_luces_rad and maps_luces_rad of the ini of the scene) and the lists with a glow map (lglow, scaled by the
 * lglow_mult of the ini) light the texels of all lists in front of them that the floor does not hide; into rad, with
 * the TXF cache. The area lights themselves get twice their color */
void __fastcall CBsp_RadAreas(CBsp *self) {
	CSuelo *suelo = Mundo_Suelo();
	char buf[0x80];
	float mc[17];
	vec3_t p, n, e, a, b;
	BspRadRayo r;
	BspTxf txf;
	CRadiosidad rad;
	uint32_t num;
	byte glowDef[4] = { 0xff, 0xff, 0xff, 0 };
	float glowDefInt = 1.0f;

	Rad_Normales(&num);
	Con_Verbosef("**** materials:\n");
	CEscena *esc = self->modelo->escena;
	for (int i = 0; i < esc->mats.n; i++) {
		TABLA_CHECK(esc->mats.p, i, esc->mats.n);
		Bsp_MatInfo(((CMaterial **)esc->mats.p)[i]);
	}
	CRadiosidad_ctor(&rad, self->nombre);
	E3D_DebugPrintf("\ninic. atrib. materiales\n");
	Con_Verbosef("\ninic. atrib. materiales\n");
	int nArea = 0, nGlow = 0;
	if (esc->ini) {
		memset(buf, 0, sizeof(buf));
		if (CArchINI_GetText(esc->ini, "lglow_mult", "_default_", buf, sizeof(buf), NULL)) {
			BspRad_LeerColor(buf, glowDef, &glowDefInt);
			Con_Verbosef("default lglow mult: (rgbi = %d %d %d %.2f)\n", glowDef[0], glowDef[1], glowDef[2],
				(double)glowDefInt);
		}
	}
	for (int i = 0; i < self->num_listas; i++) {
		static const float cero[3] = { 0.0f, 0.0f, 0.0f };
		BspTri *list = self->listas[i];
		RadAreaLuz *al = BspTri_AreaLuz(list);
		list->b155 = 0;
		list->radluz_ok = CRadiosidad_ReadMatWarn(&rad, list->Mat, al);
		Img_Create(&list->rad, list->luz.ancho, list->luz.alto, 0xc);
		CBmpFlt2_Fill(&list->rad, cero);
		if (esc->ini) {
			const char *m0 = CMaterial_GetMapName(list->Mat, 0);
			const char *m4 = CMaterial_GetMapName(list->Mat, 4);
			memset(buf, 0, sizeof(buf));
			CArchINI_GetText(esc->ini, "mats_luces_rad", list->Mat->Nombre, buf, sizeof(buf), NULL);
			if (m0) {
				CArchINI_GetText(esc->ini, "maps_luces_rad", m0, buf, sizeof(buf), NULL);
			}
			if (m4) {
				CArchINI_GetText(esc->ini, "maps_luces_rad", m4, buf, sizeof(buf), NULL);
			}
			if (buf[0]) {
				if (list->radluz_ok) {
					Con_Warnf("  area light color from rad was (rgbi = %d %d %d %.2f)\n", al->rgbi[0], al->rgbi[1],
						al->rgbi[2], (double)al->intensidad);
				}
				BspRad_LeerColor(buf, al->rgbi, &al->intensidad);
				Con_Verbosef("  area light from ini (rgbi = %d %d %d %.2f)\n", al->rgbi[0], al->rgbi[1], al->rgbi[2],
					(double)al->intensidad);
				Bsp_MatInfo(list->Mat);
				list->radluz_ok = 1;
			}
		}
		/* the glow map scaled by its lglow_mult */
		byte glow[4];
		memcpy(glow, glowDef, sizeof(glow));
		float glowInt = glowDefInt;
		if (esc->ini) {
			const char *m4 = CMaterial_GetMapName(list->Mat, 4);
			memset(buf, 0, sizeof(buf));
			if (m4) {
				CArchINI_GetText(esc->ini, "lglow_mult", m4, buf, sizeof(buf), NULL);
			}
			if (buf[0]) {
				BspRad_LeerColor(buf, glow, &glowInt);
				Con_Verbosef("  lglow mult (rgbi = %d %d %d %.2f) [%s]\n", glow[0], glow[1], glow[2], (double)glowInt,
					m4);
			}
		}
		if (list->luzA.img) {
			float *px = list->luzA.img;
			for (int k = 0; k < list->luzA.alto * list->luzA.ancho; k++, px += 4) {
				float s = glowInt * BSP_INV255;
				float sr = (float)glow[0] * s;
				float sg = s * (float)glow[1];
				float sb = s * (float)glow[2];
				float v[4] = { px[0] * sr, sg * px[1], sb * px[2], 0.0f };
				memcpy(px, v, sizeof(v));
			}
			CBmpFlt avg;
			avg.ancho = 1;
			avg.alto = 1;
			avg.img = New_Array(1, 0x10);
			CBmpFlt_Resample(&avg, &list->luzA, 0xf);
			float v[4];
			memcpy(v, CBmpFlt_Pixel(&avg, 0, 0), sizeof(v));
			const char *m4 = CMaterial_GetMapName(list->Mat, 4);
			Con_Verbosef("  lglow (average): %f %f %f [%s] [%s]\n", (double)v[0], (double)v[1], (double)v[2],
				list->Mat->Nombre, m4);
			list->b155 = Float_MaxSS(v[2], Float_MaxSS(v[1], v[0])) > 0.0f;
			if (!list->b155) {
				Con_Warnf("  lglow disabled (average is black) [%s]\n", list->Mat->Nombre);
			}
			if (list->radluz_ok) {
				Con_Warnf("  area light with lglow [%s]\n", list->Mat->Nombre);
			}
			Img_Free(&avg, 0x10);
		}
		nArea += list->radluz_ok != 0;
		nGlow += list->b155 != 0;
		if (list->radluz_ok) {
			Con_Verbosef(" area light: %s (rgbi = %d %d %d %.2f)\n", list->Mat->Nombre, al->rgbi[0], al->rgbi[1],
				al->rgbi[2], (double)al->intensidad);
		}
		if (list->b155) {
			Con_Verbosef(" lglow light: %s [%s] (%dx%d)\n", list->Mat->Nombre, CMaterial_GetMapName(list->Mat, 4),
				list->luzA.ancho, list->luzA.alto);
		}
	}
	CBsp_IniciarAreas(self);
	E3D_DebugPrintf("area lights: %d  glow lights: %d\n", nArea, nGlow);
	Con_Verbosef("area lights: %d  glow lights: %d\n", nArea, nGlow);
	g_ProfTicks[10] -= __builtin_ia32_rdtsc();
	if (nArea || nGlow) {
		BspTxf_ctor(&txf, self->nombre, "loc", 0, 1);
		int nPot = 0;
		int nAreas = 0;
		while (txf.rebuild) {
			txf.rebuild = 0;
			for (int i = 0; i < self->num_listas; i++) {
				BspTri *list = self->listas[i];
				if (g_TxfMode) {
					E3D_DebugPrintf("Check TXF active, skipping...   \n");
					break;
				}
				if (!BspRad_Lista(&txf, "local: %d / %d   ", -1, i, self->num_listas, &nAreas, list)) {
					continue;
				}
				CHECK(!(list->flags & BSPTRI_NOLMAP));
				int zona = list->zona;
				CMaterial_CalcColors(list->Mat, mc);
				CBmpFlt2_Fill(&list->rad, bspMagenta);
				int lx0 = list->ext[0];
				int ly0 = list->ext[1];
				int count = BspTri_Count(list);
				int k = 0;
				for (BspTri *t = list; t; t = t->sig) {
					k++;
					E3D_DebugPrintf("tri: %d / %d   ", k, count);
					E3D_DebugPrintf("areas: %d  \r", t->areas[0].n);
					for (int y = t->ext2[1]; y <= t->ext2[3]; y++) {
						for (int x = t->ext2[0]; x <= t->ext2[2]; x++) {
							uint32_t tx = (uint32_t)(x - lx0);
							uint32_t ty = (uint32_t)(y - ly0);
							float sr = 0.0f, sg = 0.0f, sb = 0.0f;
							if (tx >= (uint32_t)g_MapaSombrasTexX || ty >= (uint32_t)g_MapaSombrasTexY) {
								continue;
							}
							if (Color_SwapRB(*CBmp_Pixel(&list->mask, tx, ty)) != (uint32_t)(uintptr_t)t) {
								continue;
							}
							BspTri_RadPunto(&p, list, t, x, y);
							BspTri_Interp(t, &p, &n, NULL, 0);
							float dn = -(n.y * p.y + n.x * p.x + n.z * p.z);
							for (int j = 0; j < self->num_listas; j++) {
								BspTri *L = self->listas[j];
								if (!L->radluz_ok && !L->b155) {
									continue;
								}
								if (g_RadSkipBySectors && L->zona != zona) {
									continue;
								}
								const RadAreaLuz *lal = BspTri_AreaLuz(L);
								for (BspTri *S = L; S; S = S->sig) {
									if (S == t || !S->areas[0].n) {
										continue;
									}
									float lod = g_BspLodLevel;
									float dist = BspTri_RadMover(&e, S, &p, lod * 0.1f);
									if (-lod > dist || !BspTri_RadDelante(S, &n, dn, lod * 0.02f)) {
										continue;
									}
									CTabla *sel = BspTri_GetAreas(S, BspTri_RadLejos(S, &e, dist), 0);
									for (int ia = 0; ia < sel->n; ia++) {
										TABLA_CHECK(sel->p, ia, sel->n);
										BspArea *ar = &((BspArea *)sel->p)[ia];
										nPot++;
										if (BspRad_Rayo(&r, &e, &ar->pos, &n, S)) {
											continue;
										}
										float cc = r.cb * r.ca;
										/* the light of the area: its area light and its glow */
										float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
										if (L->radluz_ok) {
											float s = lal->intensidad * BSP_INV255;
											c[0] = (float)lal->rgbi[0] * s + 0.0f;
											c[1] = (float)lal->rgbi[1] * s + 0.0f;
											c[2] = (float)lal->rgbi[2] * s + 0.0f;
											c[3] = (float)lal->rgbi[3] * s + 0.0f;
										}
										if (L->b155) {
											const float *g = CBmpFlt_Pixel(&L->luzA, ar->x, ar->y);
											c[3] = c[3] + 1.0f;
											c[0] = c[0] + g[0];
											c[1] = g[1] + c[1];
											c[2] = g[2] + c[2];
										}
										float m = Float_MaxSS(c[2], Float_MaxSS(c[1], c[0]));
										if (!((double)(m * cc) > (double)r.den * 1.25e-06)) {
											continue;
										}
										BspRad_Segmento(&a, &b, &r, &e, &ar->pos);
										if (BspRad_Oculto(suelo, &a, &b)) {
											continue;
										}
										c[0] = c[0] * mc[0];
										c[1] = mc[1] * c[1];
										c[2] = mc[2] * c[2];
										c[3] = mc[3] * c[3];
										float den = r.den;
										if (lal->falloff != 1.0f) {
											float len = (float)__builtin_sqrt((double)r.d2);
											float pw = (float)Game_Pow((double)(r.ca / len), (double)lal->falloff);
											cc = pw * r.cb;
											den = len * r.k;
										}
										float f = ar->peso * cc / den;
										f = f + f;
										sr = sr + f * c[0];
										sg = sg + f * c[1];
										sb = sb + f * c[2];
									}
								}
							}
							if (list->radluz_ok) {
								const RadAreaLuz *own = BspTri_AreaLuz(list);
								float s = own->intensidad * BSP_INV255;
								sr = (float)own->rgbi[0] * s * 2.0f;
								sg = (float)own->rgbi[1] * s * 2.0f;
								sb = (float)own->rgbi[2] * s * 2.0f;
							}
							float *dst = CBmpFlt2_Pixel(&list->rad, tx, ty);
							dst[0] = sr;
							dst[1] = sg;
							dst[2] = sb;
						}
					}
				}
				BspTxf_Guardar(&txf, &list->rad);
			}
			BspTxf_Cargar(&txf, self->listas, self->num_listas, 3);
			E3D_DebugPrintf("\npot. local: %d\n", nPot);
		}
		BspTxf_dtor(&txf);
	}
	CRadiosidad_dtor(&rad);
}
