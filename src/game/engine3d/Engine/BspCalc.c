/*
 * Engine/bsp.cpp of the engine3d, the map compiler (CBsp with [mapa_sombras] calcular, see include/scrap/bsp.h): the
 * lights of the scene, the texels of the lists and the filters of their light maps. The rest of bsp.cpp is in Bsp.c.
 */
#include <float.h>
#include <string.h>

#include "scrap/bsp.h"
#include "scrap/console.h"
#include "scrap/luz.h"
#include "scrap/mathlib.h"
#include "scrap/nodo3d.h"
#include "scrap/suelo.h"
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
		l->v8c[0] = 0.0f;
		l->v8c[1] = 0.0f;
		l->v8c[2] = 0.0f;
		CLuz_FillD3D(luz, &l->d3d);
		l->f88 = luz->f30;
		l->d3d.Position = l->pos;
		l->d3d.Direction = l->dir;
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
				l->d3d.Diffuse.r = (float)luz->color[0] * k;
				l->d3d.Diffuse.g = (float)luz->color[1] * k;
				l->d3d.Diffuse.b = (float)luz->color[2] * k;
				l->d3d.Diffuse.a = (float)luz->color[3] * k;
				Con_Warnf(" light color set to: %f %f %f\n", (double)l->d3d.Diffuse.r, (double)l->d3d.Diffuse.g,
					(double)l->d3d.Diffuse.b);
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
