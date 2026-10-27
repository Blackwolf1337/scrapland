/*
 * Engine/RenderSuelo.cpp of the engine3d: the batches of the floor meshes, see include/scrap/rendersuelo.h.
 */
#include "scrap/crt.h"
#include "scrap/material.h"
#include "scrap/render.h"
#include "scrap/rendersuelo.h"

static BatchMalla *Batch_Entry(uint32_t i) {
	TABLA_CHECK(g_BatchTab->p, i, g_BatchTab->n);
	return &((BatchMalla *)g_BatchTab->p)[i];
}

void Batch_AddMalla(void *malla, void *dev) {
	if (!g_BatchTab->n) {
		CHECK(!g_BatchTab->p);
		g_BatchTab->n = BATCH_MAX;
		g_BatchTab->p = New_Array(BATCH_MAX, sizeof(BatchMalla));
		CHECK(g_BatchTab->p);
	}
	for (CMallaD3D *m = malla; m && m->vert.nVert; m = m->next) {
		if (g_BatchUsados == (uint32_t)g_BatchTab->n) {
			Batch_Flush(dev);
		}
		CMaterial *mat = m->mat;
		if (mat && Mat_IsTransp(mat)) {
			RenderTransp_Add(m, g_MatIdentity, 0, 0);
			continue;
		}
		uint32_t i = g_BatchUsados;
		Batch_Entry(i)->malla = m;
		uint32_t tex = 0;
		if (mat && mat->mapa[MAPA_DIFFUSE]) {
			tex = (uint32_t)CMapa_GetTexture(mat->mapa[MAPA_DIFFUSE], 0) >> 1;
		}
		Batch_Entry(g_BatchUsados)->tex = tex;
		uint32_t u = g_BatchUsados;
		g_BatchOrden[u] = Batch_Entry(u);
		g_BatchUsados = u + 1;
	}
}

void Batch_Flush(void *dev) {
	(void)dev;
	if (!g_BatchUsados) {
		g_BatchUsados = 0;
		return;
	}
	Render_SetWorldD3D(g_MatIdentity);
	CHECK(g_BatchTab->p && (int)g_BatchUsados <= g_BatchTab->n);
	const matrix_t *view = Render_GetView();
	for (int i = 0; i < (int)g_BatchUsados; i++) {
		BatchMalla *e = &((BatchMalla *)g_BatchTab->p)[i];
		const CMallaD3D *m = e->malla;
		vec3_t c;
		c.x = (m->box.max.x + m->box.min.x) * 0.5f;
		c.y = (m->box.max.y + m->box.min.y) * 0.5f;
		c.z = (m->box.max.z + m->box.min.z) * 0.5f;
		Vec3_TransformH(&c, &c, view);
		e->z = c.z;
	}
	crt_qsort(g_BatchOrden, g_BatchUsados, sizeof(BatchMalla *), Batch_Compare);
	for (int i = 0; i < (int)g_BatchUsados; i++) {
		Render_MallaD3D(g_BatchOrden[i]->malla, NULL);
	}
	g_BatchUsados = 0;
}

int Batch_Compare(const void *a, const void *b) {
	const BatchMalla *ea = *(BatchMalla *const *)a;
	const BatchMalla *eb = *(BatchMalla *const *)b;
	int d = (int)(ea->tex - eb->tex);

	if (d) {
		return d;
	}
	d = (int)((uintptr_t)ea->malla->mat - (uintptr_t)eb->malla->mat);
	if (d) {
		return d;
	}
	return !(eb->z > ea->z) ? 1 : -1;
}
