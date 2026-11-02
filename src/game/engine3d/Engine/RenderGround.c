/*
 * Engine/RenderSuelo.cpp of the engine3d: the batches of the floor meshes, see include/scrap/renderground.h.
 */
#include "scrap/crt.h"
#include "scrap/material.h"
#include "scrap/render.h"
#include "scrap/renderground.h"

static CTable g_BatchTab;

static BatchMesh *Batch_Entry(uint32_t i) {
	TABLE_CHECK(g_BatchTab.p, i, g_BatchTab.n);
	return &((BatchMesh *)g_BatchTab.p)[i];
}

static BatchMesh *g_BatchOrder[512];
static uint32_t g_BatchUsed;

void Batch_AddMesh(void *mesh, void *dev) {
	if (!g_BatchTab.n) {
		CHECK(!g_BatchTab.p);
		g_BatchTab.n = BATCH_MAX;
		g_BatchTab.p = New_Array(BATCH_MAX, sizeof(BatchMesh));
		CHECK(g_BatchTab.p);
	}
	for (CMeshD3D *m = mesh; m && m->vert.nVert; m = m->next) {
		if (g_BatchUsed == (uint32_t)g_BatchTab.n) {
			Batch_Flush(dev);
		}
		CMaterial *mat = m->mat;
		if (mat && Mat_IsTransp(mat)) {
			RenderMeshD3D_AddTransp(m, (&g_MatIdentity), 0, NULL);
			continue;
		}
		uint32_t i = g_BatchUsed;
		Batch_Entry(i)->mesh = m;
		uint32_t tex = 0;
		if (mat && mat->map[MAP_DIFFUSE]) {
			tex = (uint32_t)((uintptr_t)CMap_GetTexture(mat->map[MAP_DIFFUSE], 0) >> 1);	/* a sort key */
		}
		Batch_Entry(g_BatchUsed)->tex = tex;
		uint32_t u = g_BatchUsed;
		g_BatchOrder[u] = Batch_Entry(u);
		g_BatchUsed = u + 1;
	}
}

static int Batch_Compare(const void *a, const void *b);
void Batch_Flush(void *dev) {
	(void)dev;
	if (!g_BatchUsed) {
		g_BatchUsed = 0;
		return;
	}
	Render_SetWorldD3D((&g_MatIdentity));
	CHECK(g_BatchTab.p && (int)g_BatchUsed <= g_BatchTab.n);
	const matrix_t *view = Render_GetView();
	for (int i = 0; i < (int)g_BatchUsed; i++) {
		BatchMesh *e = &((BatchMesh *)g_BatchTab.p)[i];
		const CMeshD3D *m = e->mesh;
		vec3_t c;
		c.x = (m->box.max.x + m->box.min.x) * 0.5f;
		c.y = (m->box.max.y + m->box.min.y) * 0.5f;
		c.z = (m->box.max.z + m->box.min.z) * 0.5f;
		Vec3_TransformH(&c, &c, view);
		e->z = c.z;
	}
	crt_qsort(g_BatchOrder, g_BatchUsed, sizeof(BatchMesh *), Batch_Compare);
	for (int i = 0; i < (int)g_BatchUsed; i++) {
		Render_MeshD3D(g_BatchOrder[i]->mesh, NULL);
	}
	g_BatchUsed = 0;
}

/* qsort order of g_BatchOrder: texture, material, then z ascending */
static int Batch_Compare(const void *a, const void *b) {
	const BatchMesh *ea = *(BatchMesh *const *)a;
	const BatchMesh *eb = *(BatchMesh *const *)b;
	int d = (int)(ea->tex - eb->tex);

	if (d) {
		return d;
	}
	d = (int)((uintptr_t)ea->mesh->mat - (uintptr_t)eb->mesh->mat);
	if (d) {
		return d;
	}
	return !(eb->z > ea->z) ? 1 : -1;
}

/* the atexit destructor of (&g_BatchTab) and the static initializer that registers it */
static void RenderGround_FiniBatchTab(void) {
	Table_Free((&g_BatchTab));
}

void RenderGround_InitBatchTab(void) {
	crt_atexit(RenderGround_FiniBatchTab);
}
