/*
 * Engine/RenderMallaD3D.cpp, RenderMallaD3D_transp.cpp and RenderObjBase3D.cpp of the engine3d: the batches of the
 * meshes, the transparent meshes and the particle systems of a view, see include/scrap/render.h.
 */
#include "scrap/e3dutil.h"
#include "scrap/flare.h"
#include "scrap/global.h"
#include "scrap/mallad3d.h"
#include "scrap/render.h"
#include "scrap/sistpart.h"
#include "scrap/utild3d.h"
#include "scrap/varios.h"

/* the table made on first use (tabla.h, inline in the original) */
static void Batch_Alloc(CTabla *t, uint32_t size) {
	if (!t->n) {
		CHECK(!t->p);
		t->n = MAX_RENDER_BATCH;
		t->p = New_Array(MAX_RENDER_BATCH, size);
		CHECK(t->p);
	}
}

int MallaD3D_Render(CMallaD3D *self, void *ctx) {
	CNodoR *r = ctx;
	matrix_t world;

	if (!MOTOR_DEV(g_Motor)) {
		return E_FAIL;
	}
	Render_GetWorld(&world);
	Batch_Alloc(&g_TabMalla, sizeof(RenderMallaEntry));
	int list = g_UsadosMalla;
	byte cont = 0;
	for (CMallaD3D *m = self; m && m->vert.nVert; m = m->next) {
		if (g_UsadosMalla == g_TabMalla.n) {
			RenderMallaD3D_Flush();
		}
		CMaterial *mat = m->mat;
		if (mat && Mat_IsTransp(mat)) {
			RenderMallaD3D_AddTransp(m, &world, cont, r);
			cont = 1;
		} else if (g_MeshCache) {
			uint32_t i = (uint32_t)g_UsadosMalla;
			TABLA_CHECK(g_TabMalla.p, i, g_TabMalla.n);
			((RenderMallaEntry *)g_TabMalla.p)[i].malla = m;
			TABLA_CHECK(g_TabMalla.p, i, g_TabMalla.n);
			((RenderMallaEntry *)g_TabMalla.p)[i].world = world;
			uint32_t tex = 0;
			if (mat && mat->mapa[MAPA_DIFFUSE]) {
				tex = (uint32_t)CMapa_GetTexture(mat->mapa[MAPA_DIFFUSE], 0) >> 1;
			}
			TABLA_CHECK(g_TabMalla.p, i, g_TabMalla.n);
			((RenderMallaEntry *)g_TabMalla.p)[i].tex = tex;
			TABLA_CHECK(g_TabMalla.p, i, g_TabMalla.n);
			((RenderMallaEntry *)g_TabMalla.p)[i].list = list;
			TABLA_CHECK(g_TabMalla.p, i, g_TabMalla.n);
			((RenderMallaEntry *)g_TabMalla.p)[i].r = r;
			TABLA_CHECK(g_TabMalla.p, i, g_TabMalla.n);
			g_ListaMalla[i] = (RenderMallaEntry *)g_TabMalla.p + i;
			g_UsadosMalla = (int)i + 1;
		} else {
			Render_SetWorldD3D(&world);
			Render_MallaD3D(m, r);
		}
	}
	return 0;
}

int RenderMallaD3D_Flush(void) {
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	if (g_UsadosMalla) {
		Render_WorldDirty();
		CHECK(g_TabMalla.p && g_UsadosMalla <= g_TabMalla.n);
		crt_qsort(g_ListaMalla, (uint32_t)g_UsadosMalla, sizeof(RenderMallaEntry *), RenderMalla_Compare);
		int last = -1;
		for (int i = 0; i < g_UsadosMalla; i++) {
			RenderMallaEntry *e = g_ListaMalla[i];
			if (g_MeshCache == 10) {
				if (last == -1) {
					E3D_LogVerbose(g_FileLog, GAME_STR(str_3bae88, 0x3bae88), e->malla->nodo->escena->name);
				}
				CMallaD3D *m = e->malla;
				E3D_LogDebug(g_FileLog, " %s  [%d (%d)] skin:%d [%s]", m->nodo->name, e->list, last, m->pSkin != NULL,
					m->mat->Nombre);
			}
			if (e->list != last) {
				last = e->list;
				/* the meshes of a list share the world matrix, a skinned one sets its own */
				if (!e->malla->pSkin) {
					Render_SetWorldD3D(&e->world);
				}
			}
			Render_MallaD3D(e->malla, e->r);
		}
	}
	g_UsadosMalla = 0;
	return 0;
}

int RenderMalla_Compare(const void *a, const void *b) {
	const RenderMallaEntry *ea = *(RenderMallaEntry *const *)a, *eb = *(RenderMallaEntry *const *)b;
	int d = (int)(ea->tex - eb->tex);

	if (d) {
		return d;
	}
	d = (int)((uint32_t)ea->malla->mat - (uint32_t)eb->malla->mat);
	if (d) {
		return d;
	}
	return (eb->malla->pSkin == NULL) - (ea->malla->pSkin == NULL);
}

int RenderMallaD3D_AddTransp(CMallaD3D *m, const matrix_t *world, byte cont, CNodoR *r) {
	if (!MOTOR_DEV(g_Motor)) {
		return E_FAIL;
	}
	Batch_Alloc(&g_TabTransp, sizeof(RenderTranspEntry));
	if (!cont) {
		g_TranspGroup++;
	}
	if (m && m->vert.nVert) {
		if (g_UsadosTransp == g_TabTransp.n) {
			RenderMallaD3D_Flush2();
		}
		RenderTranspEntry *e = Tabla_Get50(&g_TabTransp, (uint32_t)g_UsadosTransp);
		e->malla = m;
		matrix_t w = *world;
		e = Tabla_Get50(&g_TabTransp, (uint32_t)g_UsadosTransp);
		e->world = w;
		int group = g_TranspGroup;
		if (world == g_MatIdentity) {
			group = 0;
		}
		e = Tabla_Get50(&g_TabTransp, (uint32_t)g_UsadosTransp);
		e->group = group;
		e = Tabla_Get50(&g_TabTransp, (uint32_t)g_UsadosTransp);
		e->r = r;
		e = Tabla_Get50(&g_TabTransp, (uint32_t)g_UsadosTransp);
		g_ListaTransp[g_UsadosTransp] = e;
		g_UsadosTransp++;
	}
	return 0;
}

int RenderMallaD3D_Flush2(void) {
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	if (g_UsadosTransp) {
		Render_WorldDirty();
		CHECK(g_TabTransp.p && g_UsadosTransp <= g_TabTransp.n);
		const matrix_t *view = Render_GetView();
		for (int i = 0; i < g_UsadosTransp; i++) {
			RenderTranspEntry *e = &((RenderTranspEntry *)g_TabTransp.p)[i];
			vec3_t c = e->malla->center;
			if (e->group) {
				Vec3_TransformH(&c, &c, &e->world);
			}
			if (g_ShowTranspBox) {
				CMallaD3D *m = e->malla;
				uint32_t color;
				if (m->pSkin) {
					matrix_t t, tmp;
					t = *Matrix_MulD3D(&tmp, &m->nodo->mObj, &e->world);
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
			if (e->malla->mat->params.atrib & 0x10) {
				float d = (float)__builtin_sqrt((double)e->dist2);
				e->dist2 = (d + g_TranspZBias) * (d + g_TranspZBias);
			}
		}
		crt_qsort(g_ListaTransp, (uint32_t)g_UsadosTransp, sizeof(RenderTranspEntry *), RenderTransp_Compare);
		int last = -1;
		for (int i = 0; i < g_UsadosTransp; i++) {
			RenderTranspEntry *e = g_ListaTransp[i];
			if (e->group != last) {
				Render_SetWorldD3D(&e->world);
				last = e->group;
			}
			Render_MallaD3D(e->malla, e->r);
		}
	}
	g_UsadosTransp = 0;
	g_TranspGroup = 1;
	return 0;
}

int RenderTransp_Compare(const void *a, const void *b) {
	const RenderTranspEntry *ea = *(RenderTranspEntry *const *)a, *eb = *(RenderTranspEntry *const *)b;
	return eb->dist2 > ea->dist2 ? 1 : -1;
}

int SistPart_Draw(CSistPart *sp, CNodoR *r) {
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	Batch_Alloc(&g_TabObj, sizeof(RenderObjEntry));
	if (sp) {
		if (g_UsadosObj == g_TabObj.n) {
			RenderObjBase3D_Flush();
		}
		uint32_t i = (uint32_t)g_UsadosObj;
		TABLA_CHECK(g_TabObj.p, i, g_TabObj.n);
		RenderObjEntry *e = (RenderObjEntry *)g_TabObj.p + i;
		e->r = r;
		e->obj = (IObjeto3D *)sp;
		Render_GetWorld(&e->world);
		g_ListaObj[g_UsadosObj] = e;
		g_UsadosObj++;
	}
	return 0;
}

int RenderObjBase3D_Flush(void) {
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	if (g_UsadosObj) {
		Render_WorldDirty();
		CHECK(g_TabObj.p && g_UsadosObj <= g_TabObj.n);
		const matrix_t *view = Render_GetView();
		for (int i = 0; i < g_UsadosObj; i++) {
			RenderObjEntry *e = &((RenderObjEntry *)g_TabObj.p)[i];
			vec3_t c = { e->world.m[12], e->world.m[13], e->world.m[14] };
			Vec3_TransformH(&c, &c, view);
			e->dist2 = c.y * c.y + c.x * c.x + c.z * c.z;
		}
		crt_qsort(g_ListaObj, (uint32_t)g_UsadosObj, sizeof(RenderObjEntry *), RenderObj_Compare);
		for (int i = 0; i < g_UsadosObj; i++) {
			RenderObjEntry *e = g_ListaObj[i];
			IObjeto3D *o = e->obj;
			if (o->clase == CLASE_OBJ3D_SISTPART) {
				CSistPart_Render2((CSistPart *)o, e->r);
			} else if (o->clase == CLASE_OBJ3D_FLARE) {
				CFlare_Draw((CFlare *)o);
			} else {
				CHECK(0);
			}
		}
	}
	g_UsadosObj = 0;
	return 0;
}

int RenderObj_Compare(const void *a, const void *b) {
	const RenderObjEntry *ea = *(RenderObjEntry *const *)a, *eb = *(RenderObjEntry *const *)b;
	return eb->dist2 > ea->dist2 ? 1 : -1;
}
