/*
 * Engine/bsp.cpp of the engine3d, the map compiler (CBsp with [mapa_sombras] calcular, see include/scrap/bsp.h): the
 * lights of the scene, the texels of the lists and the filters of their light maps. The rest of bsp.cpp is in Bsp.c.
 */
#include <float.h>
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/archio.h"
#include "scrap/bsp.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/scene.h"
#include "scrap/light.h"
#include "scrap/meshd3d.h"
#include "scrap/trimesh.h"
#include "scrap/mathlib.h"
#include "scrap/node3d.h"
#include "scrap/prof.h"
#include "scrap/scrapmain.h"
#include "scrap/ground.h"
#include "scrap/groundportal.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"
#include "scrap/win32.h"

/* 1/255 of the color bytes (0x3b808081) */
#define BSP_INV255 0x1.010102p-8f


/* the lights of the scene */

void Bsp_TakeLights(CScene *scn) {
	static const vec3_t omniDir = { 2.0f, -1.0f, 1.0f };

	g_BspNumLights = 0;
	Con_Verbosef("**** ind shw dsc\n");
	for (CNode3D *n = CScene_FindClass(scn, NULL, CLASS_OBJ3D_LIGHT); n; n = CScene_FindClass(scn, n, CLASS_OBJ3D_LIGHT)) {
		CLight *light = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_LIGHT);
		if (!light->active) {
			CLight_SetActive(light, 0);
			continue;
		}
		CHECK(g_BspNumLights < BSP_MAX_LIGHTS);
		BspLight *l = &g_BspLights[g_BspNumLights++];
		Bprintf(l->name, sizeof(l->name), "%s", n->name);
		l->point = light->type != LIGHT_DIR;
		l->shadow = light->shadow;
		l->rad = light->radCoef;
		l->pos = light->d3d.Position;
		vec3_t d = light->type == LIGHT_OMNI ? omniDir : light->d3d.Direction;
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
		CLight_FillD3D(light, &l->illum.d3d);
		l->illum.rangeIni = light->rangeStart;
		l->illum.d3d.Position = l->pos;
		l->illum.d3d.Direction = l->dir;
		l->stencil = light->shadow && n->flags & NODE_SHADOW;
		l->noLmap = n->flags >> 27 & 1;
		Con_Verbosef(" %d  shw:%d  stencil:%d  lmap:%d  dir:%d  rad:%.2f  rgbai:(%d %d %d %d %.2f)  p:(%.2f %.2f %.2f) "
			"d:(%.2f %.2f %.2f) a:(%.2f %.2f) r:(%.2f %.2f) [%s]\n", g_BspNumLights - 1, l->shadow, l->stencil, l->noLmap,
			!l->point, (double)l->rad, light->color[0], light->color[1], light->color[2], light->color[3],
			(double)light->intensity, (double)l->pos.x, (double)l->pos.y, (double)l->pos.z, (double)l->dir.x,
			(double)l->dir.y, (double)l->dir.z, (double)light->theta, (double)light->phi, (double)light->rangeStart,
			(double)light->range, l->name);
		if (light->intensity < 0.0f) {
			Con_Warnf("Light with negative intensity: %s\n", n->name);
			float m;
			if (scn->ini && (m = CIniFile_GetFloat(scn->ini, "mapa_sombras", "negative_lights_mult", 1.0f)) != 1.0f) {
				float k = light->intensity * m * BSP_INV255;
				l->illum.d3d.Diffuse.r = (float)light->color[0] * k;
				l->illum.d3d.Diffuse.g = (float)light->color[1] * k;
				l->illum.d3d.Diffuse.b = (float)light->color[2] * k;
				l->illum.d3d.Diffuse.a = (float)light->color[3] * k;
				Con_Warnf(" light color set to: %f %f %f\n", (double)l->illum.d3d.Diffuse.r, (double)l->illum.d3d.Diffuse.g,
					(double)l->illum.d3d.Diffuse.b);
			}
		}
		if (l->stencil && l->noLmap) {
			E3D_LogBlue(g_FileLog, "Stencil light with NO_LIGHTMAP flag: %s", n->name);
		}
		if (l->stencil != (n->flags >> 26 & 1)) {
			E3D_LogBlue(g_FileLog, "Light with RT_SHADOWS flag but no shadow enabled: %s", n->name);
		}
		CLight_SetActive(light, 0);
	}
	uint32_t c = scn->ambient.rgba;
	float k = scn->ambient.intens * BSP_INV255;
	Con_Verbosef("AMBIENT: %f %f %f (rgbi = %d %d %d %f)\n", (double)((float)(c & 0xff) * k),
		(double)((float)(c >> 8 & 0xff) * k), (double)((float)(c >> 16 & 0xff) * k), c & 0xff, c >> 8 & 0xff,
		c >> 16 & 0xff, (double)scn->ambient.intens);
	if (!scn->ini) {
		return;
	}
	vec3_t def = { 0.0f, 0.0f, 0.0f }, tmp;
	vec3_t a = *CIniFile_GetVec3(scn->ini, &tmp, "mapa_sombras", "lightmap_ambient", &def);
	if (a.x != 0.0f || a.y != 0.0f || a.z != 0.0f) {
		ColorIntens ci;
		Bsp_ColorIntens((uint32_t *)&ci, &a.x);
		scn->ambient = ci;
		k = ci.intens * BSP_INV255;
		Con_Warnf(" AMBIENT color set to: %f %f %f (rgbi = %d %d %d %f)\n", (double)((float)(ci.rgba & 0xff) * k),
			(double)((float)(ci.rgba >> 8 & 0xff) * k), (double)((float)(ci.rgba >> 16 & 0xff) * k), ci.rgba & 0xff,
			ci.rgba >> 8 & 0xff, ci.rgba >> 16 & 0xff, (double)ci.intens);
	}
}


/* the texels of the lists */

void BspTri_Mask(BspTri *list, CBmp *cob) {
	int x0 = list->ext[0], y0 = list->ext[1], x1 = list->ext[2], y1 = list->ext[3];
	int axis = list->axis;

	for (BspTri *t = list; t; t = t->next) {
		box_t b = { { FLT_MAX, FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX, -FLT_MAX } };
		int bx0, by0, bx1, by1;

		Box_AddTri(&b, t);
		Bsp_TexelBox(&b, axis, g_ShadowMapScale * g_ShadowMapRes, &bx0, &by0, &bx1, &by1);
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
				float scale = g_ShadowMapScale * g_ShadowMapRes;
				float fx = (float)x;
				float fxa = fx + 0.0f;
				vec3_t p[4];
				int cov;

				/* fully covered when all four corners are inside, else the weight of the samples */
				Bsp_TexelToWorld(&p[0], fxa, fy, axis, scale, &t->plane);
				byte inside = Ground_PointInTri(&p[0], &t->v[0], &t->v[1], &t->v[2]);
				if (inside) {
					float fx1 = fx + 1.0f;
					float fy1 = (float)y + 1.0f;
					Bsp_TexelToWorld(&p[1], fx1, fy, axis, scale, &t->plane);
					Bsp_TexelToWorld(&p[2], fxa, fy1, axis, scale, &t->plane);
					Bsp_TexelToWorld(&p[3], fx1, fy1, axis, scale, &t->plane);
					inside = Ground_PointInTri(&p[1], &t->v[0], &t->v[1], &t->v[2]) &&
						Ground_PointInTri(&p[2], &t->v[0], &t->v[1], &t->v[2]) &&
						Ground_PointInTri(&p[3], &t->v[0], &t->v[1], &t->v[2]);
				}
				if (inside) {
					cov = 0xff;
				} else {
					vec3_t m;
					float weight;
					BspTri_Sample(&m, x, y, axis, scale, t, NULL, &weight);
					cov = (int)(weight * 255.0f);
				}
				uint32_t rx = (uint32_t)(x - x0);
				if (rx >= (uint32_t)g_ShadowMapTexX || ry >= (uint32_t)g_ShadowMapTexY) {
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
					*CBmp_Pixel(&list->mask2, rx, ry) = Color_SwapRB((uint32_t)t->maskColor);
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

static inline float Bsp_Quantize(float v) {
	return (float)(int32_t)llrint((double)v * 1000.0) * 0.001f;
}

void CBsp_CreateBuffers(CBsp *self) {
	/* the lists of the same material, page and sector joined while they stay below 4096 triangles */
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *a = self->lists[i];
		BspTri *last = a;
		while (last->next) {
			last = last->next;
		}
		for (int j = i + 1; j < self->num_lists;) {
			BspTri *b = self->lists[j];
			if (a->Mat != b->Mat || a->LMap != b->LMap || a->zone != b->zone) {
				j++;
				continue;
			}
			CHECK(!((b->flags ^ a->flags) & BSPTRI_NOLMAP));
			if (BspTri_Count(a) + BspTri_Count(b) >= 0x1000) {
				j++;
				continue;
			}
			last->next = b;
			while (last->next) {
				last = last->next;
			}
			self->lists[j] = self->lists[self->num_lists - 1];
			self->num_lists--;
		}
	}
	crt_qsort(self->lists, (size_t)self->num_lists, sizeof(BspTri *),
		(int (*)(const void *, const void *))(void *)BspTri_CmpZone);
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *l = self->lists[i];
		E3D_LogPrint(g_FileLog, "%4d %-40s (%p)  lmap %02d  zona %02d  tri %d", i, l->Mat->Name, (void *)l->Mat,
			l->LMap ? l->LMap->id : -1, l->zone, BspTri_Count(l));
		E3D_LogPrint(g_FileLog, "     [%s]", CMaterial_GetMapName(self->lists[i]->Mat, MAP_DIFFUSE));
	}
	/* the vertices of each list without the repeated ones */
	int faces = 0, verts = 0;
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *list = self->lists[i];
		E3D_DebugPrintf("opt. buffer:  %d / %d  \r", i + 1, self->num_lists);
		int n = BspTri_Count(list);
		CHECK(!list->pTri);
		uint16_t *idx = New_Array((uint32_t)n, 6);
		list->numTri = n;
		list->pTri = idx;
		CTable tab = { 0, NULL };
		for (BspTri *t = list; t; t = t->next, idx += 3) {
			for (int k = 0; k < 3; k++) {
				BspVert e;
				e.pos = t->v[k];
				e.n = (vec3_t){ 0.0f, 0.0f, 0.0f };
				if (list->Mat->params.reflection) {
					e.n.x = Bsp_Quantize(t->n[k].x);
					e.n.y = Bsp_Quantize(t->n[k].y);
					e.n.z = Bsp_Quantize(t->n[k].z);
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
					TABLE_CHECK(tab.p, ix, tab.n);
					const BspVert *o = &((const BspVert *)tab.p)[ix];
					if (o->pos.x == e.pos.x && o->pos.y == e.pos.y && o->pos.z == e.pos.z && o->n.x == e.n.x &&
						o->n.y == e.n.y && o->n.z == e.n.z && o->uv[0] == e.uv[0] && o->uv[1] == e.uv[1] &&
						o->uvl[0] == e.uvl[0] && o->uvl[1] == e.uvl[1] && o->color == e.color) {
						nv = ix;
						break;
					}
				}
				if (nv == tab.n) {
					Table_Resize2C(&tab, nv + 1);
					TABLE_CHECK(tab.p, nv, tab.n);
					((BspVert *)tab.p)[nv] = e;
				}
				CHECK(nv <= 65535);
				idx[k] = (uint16_t)nv;
			}
		}
		byte normal = list->Mat->params.reflection;
		int frmt = normal ? VERT_LTEX2N : VERT_LTEX2;
		if (list->flags & BSPTRI_NOLMAP) {
			frmt = normal ? VERT_LTEX1N : VERT_LTEX1;
		}
		CVertList *lv = &list->lv;
		CVertList_SetFormat(lv, frmt);
		CVertList_Alloc(lv, tab.n, 0);
		for (int v = 0; v < tab.n; v++) {
			TABLE_CHECK(tab.p, v, tab.n);
			const BspVert *e = &((const BspVert *)tab.p)[v];
			*(vec3_t *)CVertList_GetField(lv, LVERT_POS, (uint32_t)v) = e->pos;
			vec3_t *pn = CVertList_GetField(lv, LVERT_NORMAL, (uint32_t)v);
			if (pn) {
				*pn = e->n;
			}
			uint32_t *pc = CVertList_GetField(lv, LVERT_DIFFUSE, (uint32_t)v);
			if (pc) {
				*pc = e->color;
			}
			uint32_t *ps = CVertList_GetField(lv, LVERT_SPECULAR, (uint32_t)v);
			if (ps) {
				*ps = 0;
			}
			float *pt = CVertList_GetField(lv, LVERT_TEX2D_SET1, (uint32_t)v);
			if (pt) {
				pt[0] = e->uv[0];
				pt[1] = e->uv[1];
			}
			float *pl = CVertList_GetField(lv, LVERT_TEX2D_SET1 + 1, (uint32_t)v);
			if (pl) {
				pl[0] = e->uvl[0];
				pl[1] = e->uvl[1];
			}
		}
		faces += n;
		verts += lv->nVert;
		if (tab.p) {
			operator_delete_array(tab.p);
		}
	}
	E3D_DebugPrintf("\ncrear_buffers():   listas: %d   caras: %d   vert: %d\n", self->num_lists, faces, verts);
}


/* the direct light of the lights of g_BspLights */

extern cGVar GVar_LightmapLinearAtten;
#define g_LightmapLinearAtten (GVar_LightmapLinearAtten.value.i)	/* gvar */

/* the segment from c (moved back along the light direction d) to the light at dest blocked by the floor */
static byte Bsp_Shadow(CGround *ground, const vec3_t *c, const vec3_t *d, const vec3_t *dest, vec3_t *hit, int *cache) {
	float off = 0.3f / g_ShadowMapScale;
	vec3_t from = { c->x - off * d->x, c->y - off * d->y, c->z - off * d->z };

	return CGround_Trace(ground, &from, dest, hit, 0.8f / (g_ShadowMapScale * g_ShadowMapRes), cache);
}

/* the direction of the light l at p (from the light) and its end of the shadow segment: the light, or diag back along
 * the direction of a directional one */
static void Bsp_LightDir(const BspLight *l, const vec3_t *p, float diag, vec3_t *d, vec3_t *dest) {
	if (l->point) {
		float dx = p->x - l->pos.x, dy = p->y - l->pos.y, dz = p->z - l->pos.z;
		float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if ((double)len != 0.0) {
			float kl = (float)(1.0 / (double)len);
			*d = (vec3_t){ kl * dx, kl * dy, kl * dz };
		} else {
			*d = (vec3_t){ dx, dy, dz };
		}
		*dest = l->pos;
	} else {
		*d = l->dir;
		*dest = (vec3_t){ p->x - d->x * diag, p->y - d->y * diag, p->z - d->z * diag };
	}
}

static inline float Bsp_Dist2(const vec3_t *a, const vec3_t *b) {
	float dy = a->y - b->y, dx = a->x - b->x, dz = a->z - b->z;

	return dy * dy + dx * dx + dz * dz;
}

/* the light maps of a list: light all lights, light2 the stencil ones, rad2 each times its rad */
static void BspTri_DirectLight(BspTri *list, CScene *scn, CGround *ground, float diag) {
	static const float sinCalc[3] = { 1.0f, 0.0f, 1.0f };
	float colors[17];
	int axis = list->axis;

	CMaterial_CalcColors(list->Mat, colors);
	float k = scn->ambient.intens * BSP_INV255;
	float ar = (float)(scn->ambient.rgba & 0xff) * k;
	float ag = (float)(scn->ambient.rgba >> 8 & 0xff) * k;
	float ab = (float)(scn->ambient.rgba >> 16 & 0xff) * k;
	if (g_ShadowMapNoLightAmb) {
		ar = 0.0f;
		ag = 0.0f;
		ab = 0.0f;
	}
	float br = ar * colors[4] + colors[12];
	float bg = ag * colors[5] + colors[13];
	float bb = ab * colors[6] + colors[14];
	CBmpFlt2_CreateFill(&list->light, list->lx, list->ly, sinCalc);
	CBmpFlt2_CreateFill(&list->light2, list->lx, list->ly, sinCalc);
	CBmpFlt2_Assign(&list->rad2, &list->light);
	int x0 = list->ext[0], y0 = list->ext[1];
	int nTri = BspTri_Count(list);
	int k3 = 0;
	for (BspTri *t = list; t; t = t->next, k3++) {
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
				if (rx >= (uint32_t)g_ShadowMapTexX || ry >= (uint32_t)g_ShadowMapTexY) {
					continue;
				}
				if (Color_SwapRB(*CBmp_Pixel(&list->mask, rx, ry)) != (uint32_t)(uintptr_t)t) {
					continue;
				}
				vec3_t p, n;
				float scale = g_ShadowMapScale * g_ShadowMapRes;
				Bsp_TexelToWorld(&p, (float)x + 0.5f, (float)y + 0.5f, axis, scale, &t->plane);
				BspTri_Interp(t, &p, &n, NULL, 0);
				for (int li = 0; li < g_BspNumLights; li++) {
					BspLight *l = &g_BspLights[li];
					float w = 1.0f;
					if (l->noLmap) {
						continue;
					}
					color4_t c = { 0.0f, 0.0f, 0.0f, 1.0f };
					if (Illum_Light(&c, &p, &n, colors, &l->illum, g_LightmapLinearAtten != 0)) {
						continue;
					}
					if (l->shadow && !(t->node->flags >> 6 & 1)) {
						vec3_t hit = p;
						vec3_t *phit = g_ShadowMapAtten != 0.0f ? &hit : NULL;
						float d2 = 0.0f;
						vec3_t d, dest;
						Bsp_LightDir(l, &p, diag, &d, &dest);
						if (Bsp_Shadow(ground, &p, &d, &dest, phit, l->cache)) {
							w = 0.0f;
							d2 = Bsp_Dist2(&p, &hit);
						}
						if (g_ShadowMapSoft) {
							/* the four corners of the texel, then the occlusion of the samples around p */
							vec3_t q[4];
							float fx = (float)x + 0.0f, fy = (float)y + 0.0f;
							float fx1 = (float)x + 1.0f, fy1 = (float)y + 1.0f;
							Bsp_TexelToWorld(&q[0], fx, fy, axis, scale, &t->plane);
							Bsp_TexelToWorld(&q[1], fx1, fy, axis, scale, &t->plane);
							Bsp_TexelToWorld(&q[2], fx, fy1, axis, scale, &t->plane);
							Bsp_TexelToWorld(&q[3], fx1, fy1, axis, scale, &t->plane);
							for (int i = 0; i < 4; i++) {
								if (Bsp_Shadow(ground, &q[i], &d, &dest, phit, l->cache)) {
									d2 = Bsp_Dist2(&p, &hit);
								}
							}
							CHECK(d2 == 0.0f);
							w = Bsp_Occlusion(&p, &n, 0.8f / (g_ShadowMapScale * g_ShadowMapRes), 400, &dest, ground);
						}
						if (d2 > 0.0f) {
							float s = (float)__builtin_sqrt((double)d2) * g_ShadowMapScale * g_ShadowMapAtten;
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
				memcpy(CBmpFlt2_Pixel(&list->light, rx, ry), a1, 12);
				memcpy(CBmpFlt2_Pixel(&list->light2, rx, ry), a2, 12);
				memcpy(CBmpFlt2_Pixel(&list->rad2, rx, ry), a3, 12);
			}
		}
	}
	if (!g_CalcLmap2) {
		Img_Free(&list->light2, 0xc);
	}
	if (!g_ShadowMapIllumGlobal) {
		Img_Free(&list->rad2, 0xc);
	}
}

void CBsp_DirectLight(CBsp *self) {
	CScene *scn = self->model->scene;
	CGround *ground = World_Ground();
	vec3_t dim = ground->Dim;
	float diag = (float)__builtin_sqrt((double)(dim.y * dim.y + dim.x * dim.x + dim.z * dim.z));
	diag = diag + diag;
	BspTxf txf[3];

	BspTxf_ctor(&txf[0], self->name, "dir", 0, 1);
	BspTxf_ctor(&txf[1], self->name, "dir", 1, g_CalcLmap2);
	BspTxf_ctor(&txf[2], self->name, "dir", 9, g_ShadowMapIllumGlobal);
	/* again while the caches find lists to compute once more */
	while (txf[0].rebuild || txf[1].rebuild || txf[2].rebuild) {
		txf[0].rebuild = 0;
		txf[1].rebuild = 0;
		txf[2].rebuild = 0;
		for (int i = 0; i < self->num_lists; i++) {
			BspTri *list = self->lists[i];
			if (g_TxfMode) {
				E3D_DebugPrintf("Check TXF active, skipping...   \n");
				break;
			}
			E3D_DebugPrintf("direct: %d / %d   ", i + 1, self->num_lists);
			E3D_DebugPrintf("%.1f Mb   ", (double)g_MemStats->bytes * 0.0009765625 * 0.0009765625);
			byte c0 = BspTxf_Check(&txf[0], i, 1, 0);
			byte c1 = BspTxf_Check(&txf[1], i, 0, 0);
			byte c2 = BspTxf_Check(&txf[2], i, 0, 0);
			E3D_DebugPrintf("\n");
			if (!c0 && !c1 && !c2) {
				continue;
			}
			BspTri_DirectLight(list, scn, ground, diag);
			BspTxf_Save(&txf[0], &list->light);
			BspTxf_Save(&txf[1], &list->light2);
			BspTxf_Save(&txf[2], &list->rad2);
		}
		BspTxf_Load(&txf[0], self->lists, self->num_lists, 1);
		BspTxf_Load(&txf[1], self->lists, self->num_lists, 2);
		BspTxf_Load(&txf[2], self->lists, self->num_lists, 4);
	}
	BspTxf_dtor(&txf[2]);
	BspTxf_dtor(&txf[1]);
	BspTxf_dtor(&txf[0]);
}


/* the .EMI file of the map, the counterpart of CBsp_ReadEMI (version 105) */

int CBsp_SaveEMI(CBsp *self) {
	char path[0x104], buf[0x104];
	CHashTable tabMat;
	CHashTableNode **elems;
	int num;

	Path_Merge(path, ".emi", self->name);
	CFileIO *mem = operator_new(sizeof(CFileIO));
	CFileIO *f = mem ? CFileIO_ctor(mem, path, "wb") : NULL;
	if (!f->error) {
		CFileIO_BeginBlockWrite(f, "EMI");
		CFileIO_WriteInt(f, 105);
		/* the materials once each, the key is the pointer */
		CHashTable_ctor(&tabMat, 0x1f);
		for (int i = 0; i < self->num_lists; i++) {
			CMaterial *mat = self->lists[i]->Mat;
			CHashTable_AddInt(&tabMat, (int)(intptr_t)mat, mat, 0);
		}
		CHashTable_GetList(&tabMat, &elems, &num);
		CFileIO_WriteInt(f, num);
		for (int i = 0; i < num; i++) {
			CMaterial *mat = elems[i]->value;
			CFileIO_WriteInt(f, (int32_t)(intptr_t)mat);
			CMaterial_Write(mat, f);
		}
		CHashTable_FreeList(&tabMat, &elems);
		CHashTable_dtor(&tabMat);
		/* the pages: key, file name, 1 and the file name of the secondary page */
		for (BspLMap *lm = g_BspLMaps; lm; lm = lm->next) {
			CFileIO_WriteInt(f, (int32_t)(intptr_t)lm);
			CMap *map = lm->Mat->map[0];
			CHECK(map && map->Name);
			Path_Part(buf, map->Name, 0xc);
			CFileIO_WriteString(f, buf);
			CFileIO_WriteInt(f, 1);
			map = lm->lmap2->Mat->map[0];
			CHECK(map && map->Name);
			Path_Part(buf, map->Name, 0xc);
			CFileIO_WriteString(f, buf);
		}
		CFileIO_WriteInt(f, 0);
		CFileIO_WriteInt(f, self->num_lists);
		for (int i = 0; i < self->num_lists; i++) {
			BspTri *tri = self->lists[i];
			const char *sector;
			CFileIO_BeginBlockWrite(f, "TRI");
			CFileIO_WriteInt(f, (int32_t)tri->flags);
			CFileIO_WriteString(f, tri->node->name);
			CFileIO_WriteInt(f, tri->zone);
			CHECK(tri->zone == BspTri_Zone(tri->node));
			if (!Sector_Num()) {
				sector = "_noportals_";
			} else if (tri->node->flags >> 28 & 1) {
				sector = "_nosector_";
			} else {
				sector = BspTri_ZoneName(tri);
			}
			CFileIO_WriteString(f, sector);
			CFileIO_WriteInt(f, (int32_t)(intptr_t)tri->Mat);
			CFileIO_WriteInt(f, (int32_t)(intptr_t)tri->LMap);
			CHECK(tri->pTri);
			CFileIO_WriteInt(f, tri->numTri);
			CFileIO_Write(f, tri->pTri, tri->numTri * 6);
			CVertList_Write(&tri->lv, f);
			CVertList_Write(&tri->lv2, f);
			CFileIO_EndBlockWrite(f, "TRI");
		}
		CFileIO_EndBlockWrite(f, "EMI");
		if (!f->error) {
			CFileIO_dtor(f);
			operator_delete_sized(f, sizeof(CFileIO));
			return 0;
		}
	}
	Err_Report(f->err, 0, "\n'%s'", path);
	CFileIO_dtor(f);
	operator_delete_sized(f, sizeof(CFileIO));
	return -1;
}


/* the radiosity */

/* the area light of the material of a list, read into the list (0x134..0x153) */
static inline RadAreaLight *BspTri_AreaLight(BspTri *t) {
	return (RadAreaLight *)&t->names;
}

/* the world point of the texel x, y of t in list: the center for the large lists, else the samples of t */
static void BspTri_RadPoint(vec3_t *p, const BspTri *list, const BspTri *t, int x, int y) {
	vec3_t tmp;
	float sc = g_ShadowMapScale * g_ShadowMapRes;

	if (list->big) {
		Bsp_TexelToWorld(p, (float)x + 0.5f, (float)y + 0.5f, list->axis, sc, &t->plane);
	} else {
		*p = *BspTri_Sample(&tmp, x, y, list->axis, sc, t, list, NULL);
	}
}

/* the side of q of the plane of s; a q behind it (below k) is moved to k in front into e */
static float BspTri_RadMove(vec3_t *e, const BspTri *s, const vec3_t *q, float k) {
	float dist = s->plane.n.y * q->y + q->x * s->plane.n.x + s->plane.n.z * q->z + s->plane.d;

	if (k > dist) {
		float m = k - dist;
		e->x = q->x + s->plane.n.x * m;
		e->y = q->y + s->plane.n.y * m;
		e->z = q->z + s->plane.n.z * m;
	} else {
		*e = *q;
	}
	return dist;
}

/* a corner of s is in front of the plane n, dn of the texel (above lim) */
static byte BspTri_RadFront(const BspTri *s, const vec3_t *n, float dn, float lim) {
	return !(lim >= s->v[0].y * n->y + n->x * s->v[0].x + s->v[0].z * n->z + dn &&
		lim >= s->v[1].y * n->y + n->x * s->v[1].x + s->v[1].z * n->z + dn &&
		lim >= s->v[2].y * n->y + n->x * s->v[2].x + s->v[2].z * n->z + dn);
}

/* the distance of e to the sphere of s, at least dist */
static float BspTri_RadFar(const BspTri *s, const vec3_t *e, float dist) {
	float cx = e->x - s->center.x;
	float cy = e->y - s->center.y;
	float cz = e->z - s->center.z;
	float dc = (float)__builtin_sqrt((double)(cx * cx + cy * cy + cz * cz));

	return Float_MaxSS(dc - s->radio, dist);
}

/* the light path from an area at pos to the texel point e, d = e - pos: the cosines at the texel (normal n) and at the
 * area (the plane of s), the squared length and the falloff with distance; 0 when one side faces away */
typedef struct BspRadRay {
	vec3_t d;
	float ca, cb, d2, k, den;
} BspRadRay;

static int BspRad_Ray(BspRadRay *r, const vec3_t *e, const vec3_t *pos, const vec3_t *n, const BspTri *s) {
	r->d.x = e->x - pos->x;
	r->d.y = e->y - pos->y;
	r->d.z = e->z - pos->z;
	r->ca = -(n->x * r->d.x + n->y * r->d.y + n->z * r->d.z);
	if (0.001 > (double)r->ca) {
		return 1;
	}
	r->cb = s->plane.n.y * r->d.y + s->plane.n.x * r->d.x + s->plane.n.z * r->d.z;
	if (0.001 > (double)r->cb) {
		return 2;
	}
	r->d2 = r->d.x * r->d.x + r->d.y * r->d.y + r->d.z * r->d.z;
	r->k = Float_MaxSS(g_BspRadMinD2, r->d2) * 0.2f * (g_ShadowMapScale * g_ShadowMapScale);
	r->den = r->k * r->d2;
	return 0;
}

/* the segment of the path of r from near the texel point e (b) to just off the area at pos (a) */
static void BspRad_Segment(vec3_t *a, vec3_t *b, const BspRadRay *r, const vec3_t *e, const vec3_t *pos) {
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
	float k3 = 0.3f / g_ShadowMapScale;
	a->x = u.x * k3 * 0.01f + pos->x;
	a->y = u.y * k3 * 0.01f + pos->y;
	a->z = u.z * k3 * 0.01f + pos->z;
	b->x = e->x - u.x * k3;
	b->y = e->y - u.y * k3;
	b->z = e->z - u.z * k3;
}

/* the floor hides the segment */
static inline byte BspRad_Hidden(CGround *ground, const vec3_t *a, const vec3_t *b) {
	return CGround_Trace(ground, b, a, NULL, 0.8f / (g_ShadowMapScale * g_ShadowMapRes), NULL);
}

/* the start of a list of the radiosity passes: the count and %, then the TXF check */
static byte BspRad_List(BspTxf *txf, const char *fmt, int pass, int i, int n, int *nAreas, const BspTri *list) {
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
 * At the end the statistics and rad added to the light maps (light) */
void CBsp_RadGlobal(CBsp *self) {
	CGround *ground = World_Ground();
	float mc[17];
	vec3_t p, n, e, a, b;
	BspRadRay r;
	BspTxf txf;
	int64_t sumAreas = 0, sumSel = 0;
	int nNoArea = 0, nBehind = 0, nNo = 0, nShadow = 0, nSector = 0, nComp = 0;

	if (g_ShadowMapIllumGlobal) {
		for (int bounce = 0; bounce < g_ShadowMapBounces; bounce++) {
			E3D_DebugPrintf("total areas: %d %d %d %d\n", g_BspAreasTotal[0], g_BspAreasTotal[1], g_BspAreasTotal[2],
				g_BspAreasTotal[3]);
			E3D_DebugPrintf("memoria usada: %-5d Kb\n", g_MemStats->bytes / 1024);
			/* rad2 is the light the areas send: the direct light, then the last bounce */
			for (int i = 0; i < self->num_lists; i++) {
				BspTri *tri = self->lists[i];
				CBmpFlt2_Blend(&tri->light, 0, 0, &tri->rad, 0, 0, tri->light.width, tri->light.height, 0x100, 0x100);
				CBmpFlt2_Blend(&tri->rad2, 0, 0, &tri->rad, 0, 0, tri->rad2.width, tri->rad2.height, 0x100,
					bounce ? 0 : 0x100);
			}
			CBsp_AreasGlow(self);
			CBsp_AreasMax(self);
			BspTxf_ctor(&txf, self->name, "rad", bounce, 1);
			int nAreas = 0;
			while (txf.rebuild) {
				txf.rebuild = 0;
				for (int i = 0; i < self->num_lists; i++) {
					BspTri *list = self->lists[i];
					if (g_TxfMode) {
						E3D_DebugPrintf("Check TXF active, skipping...   \n");
						break;
					}
					if (!BspRad_List(&txf, "global (%d): %d / %d   ", bounce, i, self->num_lists, &nAreas, list)) {
						continue;
					}
					int zone = list->zone;
					CMaterial_CalcColors(list->Mat, mc);
					CBmpFlt2_Fill(&list->rad, bspMagenta);
					int lx0 = list->ext[0];
					int ly0 = list->ext[1];
					int count = BspTri_Count(list);
					int k = 0;
					for (BspTri *t = list; t; t = t->next) {
						k++;
						E3D_DebugPrintf("tri: %d / %d   ", k, count);
						E3D_DebugPrintf("areas: %d  \r", t->areas[0].n);
						CHECK(!(t->flags & BSPTRI_NOLMAP));
						for (int y = t->ext2[1]; y <= t->ext2[3]; y++) {
							for (int x = t->ext2[0]; x <= t->ext2[2]; x++) {
								uint32_t tx = (uint32_t)(x - lx0);
								uint32_t ty = (uint32_t)(y - ly0);
								float sr = 0.0f, sg = 0.0f, sb = 0.0f;
								if (tx >= (uint32_t)g_ShadowMapTexX || ty >= (uint32_t)g_ShadowMapTexY) {
									continue;
								}
								if (Color_SwapRB(*CBmp_Pixel(&list->mask, tx, ty)) != (uint32_t)(uintptr_t)t) {
									continue;
								}
								BspTri_RadPoint(&p, list, t, x, y);
								BspTri_Interp(t, &p, &n, NULL, 0);
								float dn = -(n.y * p.y + n.x * p.x + n.z * p.z);
								for (int j = 0; j < self->num_lists && !list->radlight_ok; j++) {
									BspTri *L = self->lists[j];
									if (L->radlight_ok) {
										continue;
									}
									CHECK(!(L->flags & BSPTRI_NOLMAP));
									int lzona = L->zone;
									if (g_RadSkipBySectors && lzona != zone) {
										continue;
									}
									for (BspTri *S = L; S; S = S->next) {
										if (S == t || !S->areas[0].n || S->glowMax == 0.0f) {
											continue;
										}
										float lod = g_BspLodLevel;
										float dist = BspTri_RadMove(&e, S, &p, lod * 0.25f);
										if (-lod > dist || !BspTri_RadFront(S, &n, dn, lod * 0.02f)) {
											continue;
										}
										CTable *sel = BspTri_GetAreas(S, BspTri_RadFar(S, &e, dist), 1);
										sumAreas += S->areas[0].n;
										sumSel += sel->n;
										for (int ia = 0; ia < sel->n; ia++) {
											TABLE_CHECK(sel->p, ia, sel->n);
											BspArea *ar = &((BspArea *)sel->p)[ia];
											int no = BspRad_Ray(&r, &e, &ar->pos, &n, S);
											if (no) {
												if (no == 1) {
													nNoArea++;
												} else {
													nBehind++;
												}
												continue;
											}
											float cc = r.cb * r.ca;
											if (!((double)(ar->glow * cc) > (double)r.den * 1.25e-06)) {
												nNo++;
												continue;
											}
											nComp++;
											/* the segment from near the texel to just off the area */
											BspRad_Segment(&a, &b, &r, &e, &ar->pos);
											if (g_RadiosityPortals && lzona != zone && zone >= 0 && lzona >= 0 &&
												Sector_Traverse(zone, &b, &a, 0) != lzona) {
												nShadow++;
												nSector++;
												continue;
											}
											if (BspRad_Hidden(ground, &a, &b)) {
												nShadow++;
												continue;
											}
											const float *px = CBmpFlt2_Pixel(&L->rad2, ar->x, ar->y);
											float f = ar->weight * cc / r.den * g_ShadowMapCoefRad;
											sr = mc[0] * px[0] * f + sr;
											sg = mc[1] * px[1] * f + sg;
											sb = mc[2] * px[2] * f + sb;
										}
									}
								}
								float *dst = CBmpFlt2_Pixel(&list->rad, tx, ty);
								dst[0] = sr;
								dst[1] = sg;
								dst[2] = sb;
							}
						}
					}
					BspTxf_Save(&txf, &list->rad);
				}
				BspTxf_Load(&txf, self->lists, self->num_lists, 3);
			}
			BspTxf_dtor(&txf);
		}
	}
	E3D_DebugPrintf("\ntotal areas: %d %d %d %d", g_BspAreasTotal[0], g_BspAreasTotal[1], g_BspAreasTotal[2],
		g_BspAreasTotal[3]);
	E3D_DebugPrintf("\nmemoria usada: %-5d Kb", g_MemStats->bytes / 1024);
	double pot = (double)(uint64_t)sumAreas;
	E3D_DebugPrintf("\npotenciales: %.1f  descartados: %d %d %d %d %d %d", pot, 0, nNoArea, nBehind, nNo, nShadow,
		nSector);
	double pot2 = (double)(uint64_t)sumSel;
	E3D_DebugPrintf("\npotenciales2: %.1f (%.1f%%)", pot2, pot2 * 100.0 / pot);
	E3D_DebugPrintf("\nselec_rad_area: %d %d %d %d", g_BspLodCount[0], g_BspLodCount[1], g_BspLodCount[2],
		g_BspLodCount[3]);
	E3D_DebugPrintf("\nesc: %.3f  res: %.3f  umbral (x1e6): %.3f  gamma: %.3f", (double)g_ShadowMapScale,
		(double)g_ShadowMapRes, 1.25, (double)g_ShadowMapGamma);
	E3D_DebugPrintf("\nareas2:   2x2 -> %.1f/%.1f   4x4 -> %.1f/%.1f   8x8 -> %.1f/%.1f", (double)g_BspLodDist[0],
		(double)g_BspLodDist[1], (double)g_BspLodDist[2], (double)g_BspLodDist[3], (double)g_BspLodDist[4],
		(double)g_BspLodDist[5]);
	E3D_DebugPrintf("\nilum. global:  { comp: %d  ->  vis: %d  no: %d }  no: %d", nComp, nComp - nShadow, nShadow,
		nNo);
	E3D_DebugPrintf("\n");
	if (g_TxfMode && g_TxfMode != 3) {
		E3D_LogPrint(g_FileLog, "TFX checking finished");
		Err_Printf(0, "TFX checking finished");
		crt_exit(-1);
	}
	for (int i = 0; i < self->num_lists; i++) {
		BspTri *tri = self->lists[i];
		CBmpFlt2_Blend(&tri->light, 0, 0, &tri->rad, 0, 0, tri->light.width, tri->light.height, 0x100, 0x100);
		Img_Free(&tri->rad, 0xc);
		Img_Free(&tri->rad2, 0xc);
	}
}

/* the color and intensity of the m3d.ini line "r g b i" (CHECKed) into c */
static void BspRad_ReadColor(const char *buf, byte *c, float *intens) {
	CHECK(Proc_Scan(buf, "&b&b&b&f", &c[0], &c[1], &c[2], intens) == 4);
}

/* the light of the area lights: the lists with an area light ("(+area_light: name)" materials, or the sections
 * mats_luces_rad and maps_luces_rad of the ini of the scene) and the lists with a glow map (lglow, scaled by the
 * lglow_mult of the ini) light the texels of all lists in front of them that the floor does not hide; into rad, with
 * the TXF cache. The area lights themselves get twice their color */
void CBsp_RadAreas(CBsp *self) {
	CGround *ground = World_Ground();
	char buf[0x80];
	float mc[17];
	vec3_t p, n, e, a, b;
	BspRadRay r;
	BspTxf txf;
	CRadiosity rad;
	uint32_t num;
	byte glowDef[4] = { 0xff, 0xff, 0xff, 0 };
	float glowDefInt = 1.0f;

	Rad_Normals(&num);
	Con_Verbosef("**** materials:\n");
	CScene *scn = self->model->scene;
	for (int i = 0; i < scn->mats.n; i++) {
		TABLE_CHECK(scn->mats.p, i, scn->mats.n);
		Bsp_MatInfo(((CMaterial **)scn->mats.p)[i]);
	}
	CRadiosity_ctor(&rad, self->name);
	E3D_DebugPrintf("\ninic. atrib. materiales\n");
	Con_Verbosef("\ninic. atrib. materiales\n");
	int nArea = 0, nGlow = 0;
	if (scn->ini) {
		memset(buf, 0, sizeof(buf));
		if (CIniFile_GetText(scn->ini, "lglow_mult", "_default_", buf, sizeof(buf), NULL)) {
			BspRad_ReadColor(buf, glowDef, &glowDefInt);
			Con_Verbosef("default lglow mult: (rgbi = %d %d %d %.2f)\n", glowDef[0], glowDef[1], glowDef[2],
				(double)glowDefInt);
		}
	}
	for (int i = 0; i < self->num_lists; i++) {
		static const float zero[3] = { 0.0f, 0.0f, 0.0f };
		BspTri *list = self->lists[i];
		RadAreaLight *al = BspTri_AreaLight(list);
		list->glowOn = 0;
		list->radlight_ok = CRadiosity_ReadMatWarn(&rad, list->Mat, al);
		Img_Create(&list->rad, list->light.width, list->light.height, 0xc);
		CBmpFlt2_Fill(&list->rad, zero);
		if (scn->ini) {
			const char *m0 = CMaterial_GetMapName(list->Mat, 0);
			const char *m4 = CMaterial_GetMapName(list->Mat, 4);
			memset(buf, 0, sizeof(buf));
			CIniFile_GetText(scn->ini, "mats_luces_rad", list->Mat->Name, buf, sizeof(buf), NULL);
			if (m0) {
				CIniFile_GetText(scn->ini, "maps_luces_rad", m0, buf, sizeof(buf), NULL);
			}
			if (m4) {
				CIniFile_GetText(scn->ini, "maps_luces_rad", m4, buf, sizeof(buf), NULL);
			}
			if (buf[0]) {
				if (list->radlight_ok) {
					Con_Warnf("  area light color from rad was (rgbi = %d %d %d %.2f)\n", al->rgbi[0], al->rgbi[1],
						al->rgbi[2], (double)al->intensity);
				}
				BspRad_ReadColor(buf, al->rgbi, &al->intensity);
				Con_Verbosef("  area light from ini (rgbi = %d %d %d %.2f)\n", al->rgbi[0], al->rgbi[1], al->rgbi[2],
					(double)al->intensity);
				Bsp_MatInfo(list->Mat);
				list->radlight_ok = 1;
			}
		}
		/* the glow map scaled by its lglow_mult */
		byte glow[4];
		memcpy(glow, glowDef, sizeof(glow));
		float glowInt = glowDefInt;
		if (scn->ini) {
			const char *m4 = CMaterial_GetMapName(list->Mat, 4);
			memset(buf, 0, sizeof(buf));
			if (m4) {
				CIniFile_GetText(scn->ini, "lglow_mult", m4, buf, sizeof(buf), NULL);
			}
			if (buf[0]) {
				BspRad_ReadColor(buf, glow, &glowInt);
				Con_Verbosef("  lglow mult (rgbi = %d %d %d %.2f) [%s]\n", glow[0], glow[1], glow[2], (double)glowInt,
					m4);
			}
		}
		if (list->lightA.img) {
			float *px = list->lightA.img;
			for (int k = 0; k < list->lightA.height * list->lightA.width; k++, px += 4) {
				float s = glowInt * BSP_INV255;
				float sr = (float)glow[0] * s;
				float sg = s * (float)glow[1];
				float sb = s * (float)glow[2];
				float v[4] = { px[0] * sr, sg * px[1], sb * px[2], 0.0f };
				memcpy(px, v, sizeof(v));
			}
			CBmpFlt avg;
			avg.width = 1;
			avg.height = 1;
			avg.img = New_Array(1, 0x10);
			CBmpFlt_Resample(&avg, &list->lightA, 0xf);
			float v[4];
			memcpy(v, CBmpFlt_Pixel(&avg, 0, 0), sizeof(v));
			const char *m4 = CMaterial_GetMapName(list->Mat, 4);
			Con_Verbosef("  lglow (average): %f %f %f [%s] [%s]\n", (double)v[0], (double)v[1], (double)v[2],
				list->Mat->Name, m4);
			list->glowOn = Float_MaxSS(v[2], Float_MaxSS(v[1], v[0])) > 0.0f;
			if (!list->glowOn) {
				Con_Warnf("  lglow disabled (average is black) [%s]\n", list->Mat->Name);
			}
			if (list->radlight_ok) {
				Con_Warnf("  area light with lglow [%s]\n", list->Mat->Name);
			}
			Img_Free(&avg, 0x10);
		}
		nArea += list->radlight_ok != 0;
		nGlow += list->glowOn != 0;
		if (list->radlight_ok) {
			Con_Verbosef(" area light: %s (rgbi = %d %d %d %.2f)\n", list->Mat->Name, al->rgbi[0], al->rgbi[1],
				al->rgbi[2], (double)al->intensity);
		}
		if (list->glowOn) {
			Con_Verbosef(" lglow light: %s [%s] (%dx%d)\n", list->Mat->Name, CMaterial_GetMapName(list->Mat, 4),
				list->lightA.width, list->lightA.height);
		}
	}
	CBsp_InitAreas(self);
	E3D_DebugPrintf("area lights: %d  glow lights: %d\n", nArea, nGlow);
	Con_Verbosef("area lights: %d  glow lights: %d\n", nArea, nGlow);
	if (nArea || nGlow) {
		BspTxf_ctor(&txf, self->name, "loc", 0, 1);
		int nPot = 0;
		int nAreas = 0;
		while (txf.rebuild) {
			txf.rebuild = 0;
			for (int i = 0; i < self->num_lists; i++) {
				BspTri *list = self->lists[i];
				if (g_TxfMode) {
					E3D_DebugPrintf("Check TXF active, skipping...   \n");
					break;
				}
				if (!BspRad_List(&txf, "local: %d / %d   ", -1, i, self->num_lists, &nAreas, list)) {
					continue;
				}
				CHECK(!(list->flags & BSPTRI_NOLMAP));
				int zone = list->zone;
				CMaterial_CalcColors(list->Mat, mc);
				CBmpFlt2_Fill(&list->rad, bspMagenta);
				int lx0 = list->ext[0];
				int ly0 = list->ext[1];
				int count = BspTri_Count(list);
				int k = 0;
				for (BspTri *t = list; t; t = t->next) {
					k++;
					E3D_DebugPrintf("tri: %d / %d   ", k, count);
					E3D_DebugPrintf("areas: %d  \r", t->areas[0].n);
					for (int y = t->ext2[1]; y <= t->ext2[3]; y++) {
						for (int x = t->ext2[0]; x <= t->ext2[2]; x++) {
							uint32_t tx = (uint32_t)(x - lx0);
							uint32_t ty = (uint32_t)(y - ly0);
							float sr = 0.0f, sg = 0.0f, sb = 0.0f;
							if (tx >= (uint32_t)g_ShadowMapTexX || ty >= (uint32_t)g_ShadowMapTexY) {
								continue;
							}
							if (Color_SwapRB(*CBmp_Pixel(&list->mask, tx, ty)) != (uint32_t)(uintptr_t)t) {
								continue;
							}
							BspTri_RadPoint(&p, list, t, x, y);
							BspTri_Interp(t, &p, &n, NULL, 0);
							float dn = -(n.y * p.y + n.x * p.x + n.z * p.z);
							for (int j = 0; j < self->num_lists; j++) {
								BspTri *L = self->lists[j];
								if (!L->radlight_ok && !L->glowOn) {
									continue;
								}
								if (g_RadSkipBySectors && L->zone != zone) {
									continue;
								}
								const RadAreaLight *lal = BspTri_AreaLight(L);
								for (BspTri *S = L; S; S = S->next) {
									if (S == t || !S->areas[0].n) {
										continue;
									}
									float lod = g_BspLodLevel;
									float dist = BspTri_RadMove(&e, S, &p, lod * 0.1f);
									if (-lod > dist || !BspTri_RadFront(S, &n, dn, lod * 0.02f)) {
										continue;
									}
									CTable *sel = BspTri_GetAreas(S, BspTri_RadFar(S, &e, dist), 0);
									for (int ia = 0; ia < sel->n; ia++) {
										TABLE_CHECK(sel->p, ia, sel->n);
										BspArea *ar = &((BspArea *)sel->p)[ia];
										nPot++;
										if (BspRad_Ray(&r, &e, &ar->pos, &n, S)) {
											continue;
										}
										float cc = r.cb * r.ca;
										/* the light of the area: its area light and its glow */
										float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
										if (L->radlight_ok) {
											float s = lal->intensity * BSP_INV255;
											c[0] = (float)lal->rgbi[0] * s + 0.0f;
											c[1] = (float)lal->rgbi[1] * s + 0.0f;
											c[2] = (float)lal->rgbi[2] * s + 0.0f;
											c[3] = (float)lal->rgbi[3] * s + 0.0f;
										}
										if (L->glowOn) {
											const float *g = CBmpFlt_Pixel(&L->lightA, ar->x, ar->y);
											c[3] = c[3] + 1.0f;
											c[0] = c[0] + g[0];
											c[1] = g[1] + c[1];
											c[2] = g[2] + c[2];
										}
										float m = Float_MaxSS(c[2], Float_MaxSS(c[1], c[0]));
										if (!((double)(m * cc) > (double)r.den * 1.25e-06)) {
											continue;
										}
										BspRad_Segment(&a, &b, &r, &e, &ar->pos);
										if (BspRad_Hidden(ground, &a, &b)) {
											continue;
										}
										c[0] = c[0] * mc[0];
										c[1] = mc[1] * c[1];
										c[2] = mc[2] * c[2];
										c[3] = mc[3] * c[3];
										float den = r.den;
										if (lal->falloff != 1.0f) {
											float len = (float)__builtin_sqrt((double)r.d2);
											float pw = (float)pow((double)(r.ca / len), (double)lal->falloff);
											cc = pw * r.cb;
											den = len * r.k;
										}
										float f = ar->weight * cc / den;
										f = f + f;
										sr = sr + f * c[0];
										sg = sg + f * c[1];
										sb = sb + f * c[2];
									}
								}
							}
							if (list->radlight_ok) {
								const RadAreaLight *own = BspTri_AreaLight(list);
								float s = own->intensity * BSP_INV255;
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
				BspTxf_Save(&txf, &list->rad);
			}
			BspTxf_Load(&txf, self->lists, self->num_lists, 3);
			E3D_DebugPrintf("\npot. local: %d\n", nPot);
		}
		BspTxf_dtor(&txf);
	}
	CRadiosity_dtor(&rad);
}


/* the compiler */

/* the point cx, cz is inside the triangle p seen from above (the same side of its three edges) */
static byte BspTri_InsideXZ(const BspTri *p, float cx, float cz) {
	const vec3_t *v = p->v;
	byte s1 = (cz - v[0].z) * (v[1].x - v[0].x) > (cx - v[0].x) * (v[1].z - v[0].z);
	byte s2 = (cz - v[1].z) * (v[2].x - v[1].x) > (cx - v[1].x) * (v[2].z - v[1].z);

	if (s1 != s2) {
		return 0;
	}
	byte s3 = (cz - v[2].z) * (v[0].x - v[2].x) > (cx - v[2].x) * (v[0].z - v[2].z);
	return s3 == s1;
}

/* the triangles of the pool g_BspTriTmp of the same node, axis, material and side, sharing two vertices with a
 * triangle of list and with their box still on one light map page, moved to the end of list (box its box). A floor
 * triangle over another one of the list stays (yellow mask color), one with three shared vertices too (red) */
static int BspTri_Join(BspTri *list, box_t *box) {
	box_t box2;
	BspTri *tail = list;
	int cnt = 1;

	for (BspTri *act = list; act; act = act->next) {
		BspTri **pp = &g_BspTriTmp;
		while (*pp) {
			BspTri *c = *pp;
			if (act->node != c->node) {
				break;
			}
			if (act->axis != c->axis || act->Mat != c->Mat || BspTri_Side(act) != BspTri_Side(c)) {
				pp = &c->next;
				continue;
			}
			CHECK(act->zone == c->zone);
			BspTri_AddBox(&box2, box, c);
			if (!Bsp_SameSize(&box2, act->axis, g_ShadowMapScale * g_ShadowMapRes, box)) {
				pp = &c->next;
				continue;
			}
			byte above = 0;
			int k = BspTri_VertEqual(act, 0, c) ? 1 : 0;
			if (BspTri_VertEqual(act, 1, c)) {
				k++;
			}
			if (BspTri_VertEqual(act, 2, c)) {
				k++;
			}
			if (k > 1 && c->axis == 1) {
				float cz = (c->v[0].z + c->v[1].z + c->v[2].z) / 3.0f;
				float cx = (c->v[0].x + c->v[1].x + c->v[2].x) / 3.0f;
				for (BspTri *p = list; p; p = p->next) {
					if (BspTri_InsideXZ(p, cx, cz)) {
						above = 1;
						c->maskColor = 0xffff00;
					}
				}
			}
			if (k > 2) {
				c->maskColor = 0xff0000;
				pp = &c->next;
				continue;
			}
			if (above || k <= 1) {
				pp = &c->next;
				continue;
			}
			CHECK(!tail->next);
			cnt++;
			tail->next = c;
			*pp = c->next;
			tail = c;
			c->next = NULL;
			*box = box2;
		}
	}
	return cnt;
}

/* the vertex colors of the triangles of a list: the ambient, with the lights for a list without light map */
static void BspTri_ColorVert(BspTri *list, const float *mc, const float *amb, CGround *ground, float diag) {
	for (BspTri *t = list; t; t = t->next) {
		for (int k = 0; k < 3; k++) {
			float col[4] = { amb[0], amb[1], amb[2], 0.0f };
			for (int li = 0; li < g_BspNumLights; li++) {
				BspLight *l = &g_BspLights[li];
				vec3_t p = t->v[k];
				float w = 1.0f;
				if (g_TxfMode || !(list->flags & BSPTRI_NOLMAP)) {
					break;
				}
				if (l->noLmap && !(t->node->flags >> 27 & 1)) {
					continue;
				}
				color4_t c = { 0.0f, 0.0f, 0.0f, 1.0f };
				if (Illum_Light(&c, &t->v[k], &t->n[k], mc, &l->illum, g_LightmapLinearAtten != 0)) {
					continue;
				}
				if (l->shadow && !(t->node->flags >> 6 & 1)) {
					vec3_t d, dest;
					Bsp_LightDir(l, &p, diag, &d, &dest);
					if (Bsp_Shadow(ground, &p, &d, &dest, NULL, l->cache)) {
						w = 0.0f;
					}
				}
				col[0] = c.r * w + col[0];
				col[1] = col[1] + w * c.g;
				col[2] = col[2] + w * c.b;
				col[3] = col[3] + w * c.a;
			}
			if (g_DiffuseMod4x) {
				col[0] = col[0] * 0.25f;
				col[1] = col[1] * 0.25f;
				col[2] = col[2] * 0.25f;
			}
			col[3] = mc[3];
			t->color[k] = Color_FromFloat(col);
		}
	}
}

/* the light map uv of the corners of the triangles of a list from the texel origin of the list */
static void BspTri_UvLmap(BspTri *list, int x0, int y0) {
	float fx0 = (float)x0;
	float fy0 = (float)y0;
	int axis = list->axis;

	for (BspTri *t = list; t; t = t->next) {
		vec3_t v[3] = { t->v[0], t->v[1], t->v[2] };
		for (int i = 0; i < 3; i++) {
			float u = axis == 0 ? v[i].y : v[i].x;
			float w = axis == 0 || axis == 1 ? v[i].z : v[i].y;
			t->uvl[i][0] = (g_ShadowMapScale * g_ShadowMapRes * u - fx0 + 0.0f) / (float)g_ShadowMapTexX;
			t->uvl[i][1] = (g_ShadowMapScale * g_ShadowMapRes * w - fy0 + 0.0f) / (float)g_ShadowMapTexY;
		}
	}
}

/* the triangles of the meshes of the scene into the pool g_BspTriTmp (the two sided ones again turned) */
static void Bsp_TakeTriangles(CScene *scn, byte allow) {
	CMeshVert vm[3];
	plane_t pl;
	BspTri *last = NULL;

	for (CNode3D *n = scn->Node3D; n; n = n->next) {
		byte outSector = strstr(n->name, "(+out_sector)") != NULL;
		if (CNode3D_Class(n) != CLASS_OBJ3D_TRIMESH) {
			continue;
		}
		if (g_LmapOption58 && !(n->flags >> 2 & 1)) {
			continue;
		}
		if (n->flags >> 25 & 1 || (n->flags >> 7 & 1 && !(n->flags >> 29 & 1))) {
			continue;
		}
		CTriMesh *mem = operator_new(sizeof(CTriMesh));
		CTriMesh *mesh = mem ? CTriMesh_ctorCopy(mem, Obj3D_Cast(n->obj3D, CLASS_OBJ3D_TRIMESH)) : NULL;
		CTriMesh_Transform(mesh, &n->m);
		int zone = BspTri_Zone(n);
		for (int pass = 0; pass < 2; pass++) {
			for (int i = 0; i < mesh->nTri; i++) {
				CMeshTri *tm = CTriMesh_GetTri(mesh, (uint32_t)i);
				if (pass && !((CMaterial *)tm->mat)->params.twoSided) {
					continue;
				}
				memset(vm, 0, sizeof(vm));
				CTriMesh_GetTriVert(mesh, &vm[0], (uint32_t)i, 0);
				CTriMesh_GetTriVert(mesh, &vm[1], (uint32_t)i, pass ? 2 : 1);
				CTriMesh_GetTriVert(mesh, &vm[2], (uint32_t)i, pass ? 1 : 2);
				if (Bsp_Vec3Equal(&vm[0].pos, &vm[1].pos) || Bsp_Vec3Equal(&vm[1].pos, &vm[2].pos) ||
					Bsp_Vec3Equal(&vm[2].pos, &vm[0].pos)) {
					E3D_LogBlue(g_FileLog, "Invalid triangle #%d in object: %s", i, n->name);
					continue;
				}
				if (!Plane_Calc(&pl, &vm[0].pos, &vm[1].pos, &vm[2].pos)) {
					E3D_LogBlue(g_FileLog, "Invalid face normal #%d in object: %s", i, n->name);
					continue;
				}
				if (last) {
					CHECK(!last->next);
				}
				BspTri *tm2 = operator_new(sizeof(BspTri));
				BspTri *t = tm2 ? BspTri_ctor(tm2) : NULL;
				if (last) {
					last->next = t;
				} else {
					g_BspTriTmp = t;
				}
				last = t;
				for (int k = 0; k < 3; k++) {
					t->v[k] = vm[k].pos;
					t->n[k] = vm[k].n;
					t->uv[k][0] = vm[k].uv.u;
					t->uv[k][1] = vm[k].uv.v;
					t->vid[k] = tm->v[k];
					if (pass) {
						t->n[k].x = -t->n[k].x;
						t->n[k].y = -t->n[k].y;
						t->n[k].z = -t->n[k].z;
					}
				}
				if (pass) {
					t->vid[1] = tm->v[2];
					t->vid[2] = tm->v[1];
				}
				t->Mat = tm->mat;
				t->zone = zone;
				t->node = n;
				t->Mat->params.lighting = 0;
				if (n->flags >> 27 & 1 && (allow || outSector)) {
					t->flags |= BSPTRI_NOLMAP;
				}
			}
		}
		((void(*)(CTriMesh *, int))((void **)mesh->vftable)[0])(mesh, 1);
		CNode3D_FreeObj3D(n);
		if (!(n->Refs - (n->prev != NULL) - (n->next != NULL))) {
			n->flags |= 0x20008;
		}
	}
}

void CBsp_Calculate(CBsp *self) {
	box_t box, box3;
	float mc[17], amb[3];
	CBmp cob;
	uint32_t t0 = exe_timeGetTime();
	CScene *scn = self->model->scene;

	CHECK(scn);
	CGround *ground = World_Get()->ObjGround;
	CHECK(ground);
	CScene_UpdateNodes(scn);
	Bsp_TakeLights(scn);
	vec3_t dim = ground->Dim;
	float diag = (float)__builtin_sqrt((double)(dim.y * dim.y + dim.x * dim.x + dim.z * dim.z));
	diag = diag + diag;
	byte allow = !scn->ini || CIniFile_GetBool(scn->ini, "mapa_sombras", "allow_nodo_no_lightmap", 1);
	Con_Verbosef("NODO_NO_LIGHTMAP (allow_nodo_no_lightmap): %s\n", allow ? "true" : "false");
	if (!allow) {
		Con_Warnf("allow_nodo_no_lightmap set to false\n");
	}
	E3D_DebugPrintf("\n\ncalcular_mapas(): '%s'", scn->name);
	E3D_DebugPrintf("\nluces: %d\n", g_BspNumLights);
	CHECK(!World_Get()->GroundScale->fromAmc);
	CHECK(!scn->fromAmc);
	Err_Printf(0, "USE_NO_COLLIDE_TRIS not defined, (+no_collide) objects will not cast shadows");
	{
		char title[0x400];
		Bprintf(title, sizeof(title), "Compiling Map [%s] - Version %d", self->name, g_EngineVersion);
		Plat_SetTitle(title);
	}
	CHECK(!g_TXF_Helper || g_ShadowMapUseTxf);
	if (g_TxfCheckFiles) {
		if (Plat_MessageBox("Generate .EMI from valid .TXF files?", "Message", 0x2024) == 6) {
			g_TxfMode = 3;
		} else {
			g_TxfMode = Plat_MessageBox("Delete invalid .TXF files?", "Message", 0x2024) == 6 ? 2 : 1;
		}
	}
	Plat_SetThreadPriority(-1);
	if (g_BspTriTmp) {
		BspTri *t = g_BspTriTmp;
		BspTri_dtor(t);
		operator_delete_sized(t, sizeof(BspTri));
		g_BspTriTmp = NULL;
	}
	Bsp_TakeTriangles(scn, allow);

	/* the lists, each with its light map texels and vertex colors */
	BspTri_CalcPlanes(g_BspTriTmp);
	int total = BspTri_Count(g_BspTriTmp);
	int done = 0;
	while (g_BspTriTmp) {
		BspTri *list = g_BspTriTmp;
		g_BspTriTmp = list->next;
		list->next = NULL;
		BspTri_ListBox(&box, list);
		int cnt = BspTri_Join(list, &box);
		BspTri_ListBox(&box3, list);
		CHECK(box.min.x == box3.min.x && box.min.y == box3.min.y && box.min.z == box3.min.z &&
			box.max.x == box3.max.x && box.max.y == box3.max.y && box.max.z == box3.max.z);
		done += cnt;
		E3D_DebugPrintf("cara: %d / %d   caras: %-3d  mat: '%s'\n", done, total, cnt, list->Mat->Name);
		int axis = list->axis;
		CMaterial_CalcColors(list->Mat, mc);
		float k = scn->ambient.intens * BSP_INV255;
		float ar = (float)(scn->ambient.rgba & 0xff) * k;
		float ag = (float)(scn->ambient.rgba >> 8 & 0xff) * k;
		float ab = (float)(scn->ambient.rgba >> 16 & 0xff) * k;
		if (g_ShadowMapNoLightAmb) {
			ar = 0.0f;
			ag = 0.0f;
			ab = 0.0f;
		}
		amb[0] = ar * mc[4] + mc[12];
		amb[1] = ag * mc[5] + mc[13];
		amb[2] = ab * mc[6] + mc[14];
		int x0, y0, x1, y1;
		BspTri_ListBox(&box, list);
		Bsp_TexelBox(&box, axis, g_ShadowMapScale * g_ShadowMapRes, &x0, &y0, &x1, &y1);
		BspTri_SetExt(list, x0, y0, x1, y1, !(list->flags & BSPTRI_NOLMAP));
		Img_Create(&list->mask, list->lx, list->ly, 4);
		CBmp_Fill(&list->mask, 0);
		cob.img = NULL;
		cob.width = list->lx;
		cob.height = list->ly;
		cob.img = New_Array((uint32_t)(cob.width * cob.height), 4);
		CBmp_Fill(&cob, 0xffff0000);
		BspTri_Mask(list, &cob);
		BspTri_CalcGlow(list);
		BspTri_UvLmap(list, x0, y0);
		BspTri_ColorVert(list, mc, amb, ground, diag);
		Img_Free(&cob, 4);
		if (list->flags & BSPTRI_NOLMAP) {
			Img_Free(&list->mask, 4);
			Img_Free(&list->mask2, 4);
			CHECK(self->num_nolmap < BSP_MAX_NOLMAP);
			self->nolmap[self->num_nolmap++] = list;
		} else {
			CHECK(self->num_lists < BSP_MAX_LISTS);
			self->lists[self->num_lists++] = list;
		}
	}
	E3D_DebugPrintf("\nlistas: %d  nolmap: %d\n", self->num_lists, self->num_nolmap);

	CBsp_DirectLight(self);
	if (g_ShadowMapCalculate && g_ShadowMapRadiosity) {
		float sc = g_ShadowMapScale * g_ShadowMapRes;
		g_BspLodLevel = 1.0f / sc;
		g_BspRadMinD2 = 1.0f / (sc * sc) * g_RadiosityMinDist;
		CBsp_RadAreas(self);
		CBsp_RadGlobal(self);
	}
	if (g_TXF_Helper) {
		if (g_ShadowMapDlg) {
			Plat_MessageBox("TXF helper finished", "Message", 0x2040);
		}
		crt_fflush(NULL);
		crt_exit_now(-1);
	}
	CBsp_RemoveLights(self);
	CBsp_AdjustGamma(self);
	CBsp_GroupMaps(self);
	float secs = (float)((double)(exe_timeGetTime() - t0) / 1000.0);
	int s = (int)secs;
	E3D_DebugPrintf("\ncalcular_mapas(): %.2f seg. (%d:%02d:%02d hms)\n", (double)secs, s / 3600, s / 60 % 60, s % 60);
	E3D_LogPrint(g_FileLog, "calcular_mapas(): %.2f seg. (%d:%02d:%02d hms)", (double)secs, s / 3600, s / 60 % 60,
		s % 60);
	E3D_DebugPrintf("\ng_CheckLine3D_lmap_ntricmp: %u   g_CheckLine3D_lmap_ncomp: %u\n", ground->traceStats[0],
		g_CollisionNComp);
	E3D_DebugPrintf("ult_aciertos: %u   ult_fallos: %u\n\n", ground->traceStats[1], ground->traceStats[2]);
	Bsp2_Stats((int)(intptr_t)self->lights);
	CBsp_CreateBuffers(self);
	CBsp_SaveEMI(self);
	CBsp_SaveIni(self);
	Plat_SetThreadPriority(0);
	Bsp_EndMessage(self, s);
}
