/*
 * Engine/RenderMallaD3D.cpp, RenderMallaD3D_transp.cpp and RenderObjBase3D.cpp of the engine3d: the batches of the
 * meshes, the transparent meshes and the particle systems of a view, see include/scrap/render.h.
 */
#include "scrap/e3dutil.h"
#include "scrap/flare.h"
#include "scrap/global.h"
#include "scrap/meshd3d.h"
#include "scrap/render.h"
#include "scrap/particlesystem.h"
#include "scrap/utild3d.h"
#include "scrap/misc.h"

/* the table made on first use (table.h, inline in the original) */
static void Batch_Alloc(CTable *t, uint32_t size) {
	if (!t->n) {
		CHECK(!t->p);
		t->n = MAX_RENDER_BATCH;
		t->p = New_Array(MAX_RENDER_BATCH, size);
		CHECK(t->p);
	}
}

CTable g_TabMesh;

RenderMeshEntry *g_MeshList[1026];

int MeshD3D_Render(CMeshD3D *self, void *ctx) {
	CRenderNode *r = ctx;
	matrix_t world;

	if (!g_Engine->device) {
		return E_FAIL;
	}
	Render_GetWorld(&world);
	Batch_Alloc(&g_TabMesh, sizeof(RenderMeshEntry));
	int list = g_UsedMesh;
	byte cont = 0;
	for (CMeshD3D *m = self; m && m->vert.nVert; m = m->next) {
		if (g_UsedMesh == g_TabMesh.n) {
			RenderMeshD3D_Flush();
		}
		CMaterial *mat = m->mat;
		if (mat && Mat_IsTransp(mat)) {
			RenderMeshD3D_AddTransp(m, &world, cont, r);
			cont = 1;
		} else if (g_MeshCache) {
			uint32_t i = (uint32_t)g_UsedMesh;
			TABLE_CHECK(g_TabMesh.p, i, g_TabMesh.n);
			((RenderMeshEntry *)g_TabMesh.p)[i].mesh = m;
			TABLE_CHECK(g_TabMesh.p, i, g_TabMesh.n);
			((RenderMeshEntry *)g_TabMesh.p)[i].world = world;
			uint32_t tex = 0;
			if (mat && mat->map[MAP_DIFFUSE]) {
				tex = (uint32_t)((uintptr_t)CMap_GetTexture(mat->map[MAP_DIFFUSE], 0) >> 1);	/* a sort key */
			}
			TABLE_CHECK(g_TabMesh.p, i, g_TabMesh.n);
			((RenderMeshEntry *)g_TabMesh.p)[i].tex = tex;
			TABLE_CHECK(g_TabMesh.p, i, g_TabMesh.n);
			((RenderMeshEntry *)g_TabMesh.p)[i].list = list;
			TABLE_CHECK(g_TabMesh.p, i, g_TabMesh.n);
			((RenderMeshEntry *)g_TabMesh.p)[i].r = r;
			TABLE_CHECK(g_TabMesh.p, i, g_TabMesh.n);
			g_MeshList[i] = (RenderMeshEntry *)g_TabMesh.p + i;
			g_UsedMesh = (int)i + 1;
		} else {
			Render_SetWorldD3D(&world);
			Render_MeshD3D(m, r);
		}
	}
	return 0;
}

int RenderMeshD3D_Flush(void) {
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	if (g_UsedMesh) {
		Render_WorldDirty();
		CHECK(g_TabMesh.p && g_UsedMesh <= g_TabMesh.n);
		crt_qsort(g_MeshList, (uint32_t)g_UsedMesh, sizeof(RenderMeshEntry *), RenderMesh_Compare);
		int last = -1;
		for (int i = 0; i < g_UsedMesh; i++) {
			RenderMeshEntry *e = g_MeshList[i];
			if (g_MeshCache == 10) {
				if (last == -1) {
					E3D_LogVerbose((&g_FileLog), "%s", e->mesh->node->scene->name);
				}
				CMeshD3D *m = e->mesh;
				E3D_LogDebug((&g_FileLog), " %s  [%d (%d)] skin:%d [%s]", m->node->name, e->list, last, m->pSkin != NULL,
					m->mat->Name);
			}
			if (e->list != last) {
				last = e->list;
				/* the meshes of a list share the world matrix, a skinned one sets its own */
				if (!e->mesh->pSkin) {
					Render_SetWorldD3D(&e->world);
				}
			}
			Render_MeshD3D(e->mesh, e->r);
		}
	}
	g_UsedMesh = 0;
	return 0;
}

int RenderMesh_Compare(const void *a, const void *b) {
	const RenderMeshEntry *ea = *(RenderMeshEntry *const *)a, *eb = *(RenderMeshEntry *const *)b;
	int d = (int)(ea->tex - eb->tex);

	if (d) {
		return d;
	}
#if SD_32BIT
	d = (int)((uint32_t)ea->mesh->mat - (uint32_t)eb->mesh->mat);	/* the difference of the addresses, as the original */
#else
	d = (ea->mesh->mat > eb->mesh->mat) - (ea->mesh->mat < eb->mesh->mat);
#endif
	if (d) {
		return d;
	}
	return (eb->mesh->pSkin == NULL) - (ea->mesh->pSkin == NULL);
}

CTable g_TabTransp;

RenderTranspEntry *g_TranspList[1026];

int g_TranspGroup = 1;

int RenderMeshD3D_AddTransp(CMeshD3D *m, const matrix_t *world, byte cont, CRenderNode *r) {
	if (!g_Engine->device) {
		return E_FAIL;
	}
	Batch_Alloc(&g_TabTransp, sizeof(RenderTranspEntry));
	if (!cont) {
		g_TranspGroup++;
	}
	if (m && m->vert.nVert) {
		if (g_UsedTransp == g_TabTransp.n) {
			RenderMeshD3D_Flush2();
		}
		RenderTranspEntry *e = Table_GetN(&g_TabTransp, (uint32_t)g_UsedTransp, sizeof(RenderTranspEntry));
		e->mesh = m;
		matrix_t w = *world;
		e = Table_GetN(&g_TabTransp, (uint32_t)g_UsedTransp, sizeof(RenderTranspEntry));
		e->world = w;
		int group = g_TranspGroup;
		if (world == (&g_MatIdentity)) {
			group = 0;
		}
		e = Table_GetN(&g_TabTransp, (uint32_t)g_UsedTransp, sizeof(RenderTranspEntry));
		e->group = group;
		e = Table_GetN(&g_TabTransp, (uint32_t)g_UsedTransp, sizeof(RenderTranspEntry));
		e->r = r;
		e = Table_GetN(&g_TabTransp, (uint32_t)g_UsedTransp, sizeof(RenderTranspEntry));
		g_TranspList[g_UsedTransp] = e;
		g_UsedTransp++;
	}
	return 0;
}

int RenderMeshD3D_Flush2(void) {
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	if (g_UsedTransp) {
		Render_WorldDirty();
		CHECK(g_TabTransp.p && g_UsedTransp <= g_TabTransp.n);
		const matrix_t *view = Render_GetView();
		for (int i = 0; i < g_UsedTransp; i++) {
			RenderTranspEntry *e = &((RenderTranspEntry *)g_TabTransp.p)[i];
			vec3_t c = e->mesh->center;
			if (e->group) {
				Vec3_TransformH(&c, &c, &e->world);
			}
			if (g_ShowTranspBox) {
				CMeshD3D *m = e->mesh;
				uint32_t color;
				if (m->pSkin) {
					matrix_t t, tmp;
					t = *Matrix_MulD3D(&tmp, &m->node->mObj, &e->world);
					Debug_DrawBoxM(&t, &m->ext.min, &m->ext.max, 0xffff00);
					color = 0x808000;
				} else {
					Debug_DrawBoxM(&e->world, &m->ext.min, &m->ext.max, 0xff00);
					color = 0x8000;
				}
				Debug_DrawPoint(&c, 20.0f, e->group ? color : 0xffffff);
			}
			Vec3_TransformH(&c, &c, view);
			e->dist2 = c.x * c.x + c.y * c.y + c.z * c.z;
			if (e->mesh->mat->params.attrib & 0x10) {
				float d = (float)__builtin_sqrt((double)e->dist2);
				e->dist2 = (d + g_TranspZBias) * (d + g_TranspZBias);
			}
		}
		crt_qsort(g_TranspList, (uint32_t)g_UsedTransp, sizeof(RenderTranspEntry *), RenderTransp_Compare);
		int last = -1;
		for (int i = 0; i < g_UsedTransp; i++) {
			RenderTranspEntry *e = g_TranspList[i];
			if (e->group != last) {
				Render_SetWorldD3D(&e->world);
				last = e->group;
			}
			Render_MeshD3D(e->mesh, e->r);
		}
	}
	g_UsedTransp = 0;
	g_TranspGroup = 1;
	return 0;
}

int RenderTransp_Compare(const void *a, const void *b) {
	const RenderTranspEntry *ea = *(RenderTranspEntry *const *)a, *eb = *(RenderTranspEntry *const *)b;
	return eb->dist2 > ea->dist2 ? 1 : -1;
}

CTable g_TabObj;

RenderObjEntry *g_ObjList[1026];

int ParticleSystem_Draw(CParticleSystem *sp, CRenderNode *r) {
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	Batch_Alloc(&g_TabObj, sizeof(RenderObjEntry));
	if (sp) {
		if (g_UsedObj == g_TabObj.n) {
			RenderObjBase3D_Flush();
		}
		uint32_t i = (uint32_t)g_UsedObj;
		TABLE_CHECK(g_TabObj.p, i, g_TabObj.n);
		RenderObjEntry *e = (RenderObjEntry *)g_TabObj.p + i;
		e->r = r;
		e->obj = (IObject3D *)sp;
		Render_GetWorld(&e->world);
		g_ObjList[g_UsedObj] = e;
		g_UsedObj++;
	}
	return 0;
}

int RenderObjBase3D_Flush(void) {
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	if (g_UsedObj) {
		Render_WorldDirty();
		CHECK(g_TabObj.p && g_UsedObj <= g_TabObj.n);
		const matrix_t *view = Render_GetView();
		for (int i = 0; i < g_UsedObj; i++) {
			RenderObjEntry *e = &((RenderObjEntry *)g_TabObj.p)[i];
			vec3_t c = { e->world.m[12], e->world.m[13], e->world.m[14] };
			Vec3_TransformH(&c, &c, view);
			e->dist2 = c.y * c.y + c.x * c.x + c.z * c.z;
		}
		crt_qsort(g_ObjList, (uint32_t)g_UsedObj, sizeof(RenderObjEntry *), RenderObj_Compare);
		for (int i = 0; i < g_UsedObj; i++) {
			RenderObjEntry *e = g_ObjList[i];
			IObject3D *o = e->obj;
			if (o->class == CLASS_OBJ3D_PARTICLESYSTEM) {
				CParticleSystem_Render2((CParticleSystem *)o, e->r);
			} else if (o->class == CLASS_OBJ3D_FLARE) {
				CFlare_Draw((CFlare *)o);
			} else {
				CHECK(0);
			}
		}
	}
	g_UsedObj = 0;
	return 0;
}

int RenderObj_Compare(const void *a, const void *b) {
	const RenderObjEntry *ea = *(RenderObjEntry *const *)a, *eb = *(RenderObjEntry *const *)b;
	return eb->dist2 > ea->dist2 ? 1 : -1;
}

cGVar GVar_R_MeshCache;

cGVar GVar_R_TranspZBias;

cGVar GVar_R_ShowTranspBox;

