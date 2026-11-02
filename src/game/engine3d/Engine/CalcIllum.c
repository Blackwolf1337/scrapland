/*
 * Engine/CalcIlum.cpp of the engine3d: the preillumination of a mesh (precalc_ilum): its VERT_TEX1 vertices become
 * VERT_LTEX1N with a diffuse color, the emissive color of the material without a scene, else the ambient light of the
 * scene plus the lights of the scene (Engine/RenderCalcIlum.cpp) on the vertices moved into the world.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/e3dutil.h"
#include "scrap/scene.h"
#include "scrap/vertlist.h"
#include "scrap/light.h"
#include "scrap/meshd3d.h"
#include "scrap/trimesh.h"
#include "scrap/material.h"
#include "scrap/utild3d.h"

cGVar GVar_R_ShowPreIllumVtx;

#define g_ShowPreIllumVtx (GVar_R_ShowPreIllumVtx.value.i)	/* R_ShowPreIllumVtx: 1 no light colors, 2 no emissive ones */

/* a vertex of VERT_LTEX1N */
typedef struct VertLTex1N {
	vec3_t pos;			/* 0x00 */
	vec3_t n;			/* 0x0c */
	uint32_t color;			/* 0x18 */
	uv_t uv;			/* 0x1c */
} VertLTex1N;
SD_SIZE(VertLTex1N, 0x24);

/* the colors of CMaterial_CalcColors */
enum { COL_DIFFUSE = 0, COL_AMBIENT = 4, COL_SPECULAR = 8, COL_EMISSIVE = 12, COL_POWER = 16, COL_TOTAL = 17 };

void CMeshD3D_CalcIllum(CMeshD3D *self, CNode3D *node, CScene *scn) {
	if (self->next) {
		CMeshD3D_CalcIllum(self->next, node, scn);
	}
	if (!self->mat || self->vert.frmt != VERT_TEX1) {
		return;
	}
	CVertList lv;
	CVertList_ctor(&lv);
	CVertList_SetFormat(&lv, VERT_LTEX1N);
	CVertList_Alloc(&lv, self->vert.nVert, 0);
	float colors[COL_TOTAL];
	CMaterial_CalcColors(self->mat, colors);
	/* the vertices without a light color stay green, magenta with the scene */
	uint32_t mark = scn ? 0x800080 : 0x8000;
	for (int i = 0; i < self->vert.nVert; i++) {
		VertLTex1N *d = (VertLTex1N *)CVertList_LTex1N(&lv, (uint32_t)i);
		vec3_t t;
		uv_t uv;
		d->pos = *CVertList_GetPos(&self->vert, &t, (uint32_t)i);
		d->n = *CVertList_GetNormal(&self->vert, &t, (uint32_t)i);
		d->color = mark;
		d->uv = *CVertList_GetUV(&self->vert, &uv, (uint32_t)i);
	}
	if (!scn) {
		for (int i = 0; i < lv.nVert; i++) {
			VertLTex1N *d = (VertLTex1N *)CVertList_LTex1N(&lv, (uint32_t)i);
			float c[4] = { colors[COL_EMISSIVE], colors[COL_EMISSIVE + 1], colors[COL_EMISSIVE + 2], 0.0f };
			if (g_DiffuseMod4x) {
				c[0] = c[0] * 0.25f;
				c[1] = c[1] * 0.25f;
				c[2] = c[2] * 0.25f;
			}
			c[3] = colors[COL_DIFFUSE + 3];
			if (!(g_ShowPreIllumVtx & 2)) {
				d->color = Color_FromFloat(c);
			}
		}
	} else {
		CHECK(node);
		CScene_UpdateNodes(scn);
		CVertList_Transform(&self->vert, &node->m, 1);
		CTable acc = { 0, NULL };
		Table_Alloc10(&acc, lv.nVert);
		CHECK(acc.p);
		float(*sum)[4] = acc.p;
		memset(sum, 0, (uint32_t)acc.n << 4);
		float k = scn->ambient.intens * (1.0f / 255.0f);
		const byte *amb = (const byte *)&scn->ambient.rgba;
		float base[4];
		for (int j = 0; j < 4; j++) {
			base[j] = colors[COL_EMISSIVE + j] + (float)amb[j] * k * colors[COL_AMBIENT + j];
		}
		for (int i = 0; i < self->vert.nVert; i++) {
			TABLE_CHECK(sum, i, acc.n);
			memcpy(sum[i], base, sizeof(base));
		}
		for (CNode3D *n = CScene_FindClass(scn, NULL, CLASS_OBJ3D_LIGHT); n;
			n = CScene_FindClass(scn, n, CLASS_OBJ3D_LIGHT)) {
			CLight *light = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_LIGHT);
			if (light && light->active) {
				Illum_VertList((color4_t *)sum, &self->vert, self->mat, light);
			}
		}
		for (int i = 0; i < lv.nVert; i++) {
			VertLTex1N *d = (VertLTex1N *)CVertList_LTex1N(&lv, (uint32_t)i);
			if (g_DiffuseMod4x) {
				TABLE_CHECK(sum, i, acc.n);
				float c[4] = { sum[i][0] * 0.25f, sum[i][1] * 0.25f, sum[i][2] * 0.25f, sum[i][3] * 0.25f };
				TABLE_CHECK(sum, i, acc.n);
				memcpy(sum[i], c, sizeof(c));
			}
			TABLE_CHECK(sum, i, acc.n);
			sum[i][3] = colors[COL_DIFFUSE + 3];
			if (!(g_ShowPreIllumVtx & 1)) {
				TABLE_CHECK(sum, i, acc.n);
				d->color = Color_FromFloat(sum[i]);
			}
		}
		if (sum) {
			operator_delete_array(sum);
		}
	}
	CVertList_Swap(&self->vert, &lv);
	CVertList_Free(&lv);
	self->mat->params.lighting = 0;
	CVertList_Free(&lv);
}
