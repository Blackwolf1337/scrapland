/*
 * The scene and model lists of the engine3d, original source Engine/util_escena.cpp (include/scrap/modelo.h):
 * crear_escena and crear_modelo with the shared scenes, gModelosRender and the list dump.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/material.h"
#include "scrap/modelo.h"
#include "scrap/varios.h"

#define g_EscenaAnimPadre GAME_DATA(CEscena *, 0x4e1ec0)	/* while crear_animacion makes its scene */
#define g_EscenaLODPadre GAME_DATA(CEscena *, 0x4e1ec4)	/* while a LOD model is made */
#define g_SortModels GAME_DATA(int, 0x4ae790)	/* R_SortModels */
#define g_SortShow GAME_DATA(int, 0x4ae7a8)	/* R_SortShow */
#define g_SortInvert GAME_DATA(int, 0x4ae7c0)	/* R_SortInvert */

#define SORT_TEXT_X 200.0f
#define SORT_TEXT_Y 10.0f
#define SORT_TEXT_COLOR 0xff00

static void ListaEscena_Free(CListaEscena *le) {
	if (le->escena) {
		Escena_Unref(&le->escena);
	}
	if (le->name2) {
		Cad_Set(&le->name2, NULL);
	}
	if (le->name) {
		Cad_Set(&le->name, NULL);
	}
	operator_delete_sized(le, sizeof(*le));
}

CListaEscena *__thiscall CListaEscena_ctor(CListaEscena *self, CEscena *esc, const char *name, const char *name2,
	const vec3_t *escala, uint32_t flags, CListaEscena **lista) {
	self->name = NULL;
	self->name2 = NULL;
	Cad_Set(&self->name, name);
	Cad_Set(&self->name2, name2);
	self->escala = *escala;
	self->flags = flags;
	self->escena = esc;
	self->next = NULL;
	if (esc) {
		esc->Refs++;
	}
	CHECK(lista);
	while (*lista) {
		lista = &(*lista)->next;
	}
	*lista = self;
	return self;
}

CListaModelo *__thiscall CListaModelo_ctor(CListaModelo *self, CModelo *m, const char *name, const char *name2,
	uint32_t flags, CListaModelo **lista) {
	self->name = NULL;
	self->name2 = NULL;
	Cad_Set(&self->name, name);
	Cad_Set(&self->name2, name2);
	self->flags = flags;
	self->modelo = m;
	self->next = NULL;
	CHECK(lista);
	while (*lista) {
		lista = &(*lista)->next;
	}
	*lista = self;
	self->sortKey = 0.0f;
	self->indiceRender = -1;
	m->lista = self;
	Modelo_UpdateRender(m);
	return self;
}

void __fastcall CListaModelo_dtor(CListaModelo *self) {
	CHECK(self->flags & ESCENA_AUTOINIC);
	CHECK(self->modelo);
	CModelo *m = self->modelo;
	if (m->enable) {
		m->enable = 0;
		m->visible = 0;
		Modelo_UpdateRender(m);
	}
	if (self->modelo) {
		((modeloDelete_t)((void **)self->modelo->vftable)[0])(self->modelo, 1);
		self->modelo = NULL;
	}
	if (self->name2) {
		Cad_Set(&self->name2, NULL);
	}
	if (self->name) {
		Cad_Set(&self->name, NULL);
	}
}

void Modelo_UpdateRender(CModelo *m) {
	CListaModelo *lm = m->lista;

	if (!lm) {
		return;
	}
	int idx = lm->indiceRender;
	if (m->enable) {
		if (idx != -1) {
			return;
		}
		CHECK(g_NumModelosRender < MAX_MODELOS_RENDER);
		int n = g_NumModelosRender;
		g_ModelosRender[n] = lm;
		lm->indiceRender = n;
		n++;
		g_NumModelosRender = n;
		g_MaxModelosRender = g_MaxModelosRender > n ? g_MaxModelosRender : n;
		return;
	}
	if (idx == -1) {
		return;
	}
	CHECK(idx < MAX_MODELOS_RENDER);
	CHECK(g_NumModelosRender <= MAX_MODELOS_RENDER);
	CHECK(g_ModelosRender[idx] == lm);
	CHECK(lm->modelo == m);
	int n = g_NumModelosRender;
	g_NumModelosRender = n - 1;
	CListaModelo *last = g_ModelosRender[n - 1];
	g_ModelosRender[idx] = last;
	last->indiceRender = idx;
	lm->indiceRender = -1;
}

byte Escena_Release(CEscena *esc, byte force) {
	CListaEscena *le = g_ListaEscenas;

	if (!esc || !le) {
		return 0;
	}
	CHECK(esc->Refs > 1);
	if (force) {
		if (esc->Refs != 2) {
			Con_Warnf("WARN: Trying to release a nonzero reference scene: %s\n", esc->name);
			return 1;
		}
	} else if (esc->Refs != 2) {
		return 0;
	}
	if (le->escena == esc) {
		if ((le->flags & ESCENA_COMPARTIR) && !force) {
			return 0;
		}
		g_ListaEscenas = le->next;
	} else {
		for (;;) {
			if (!le->next) {
				return 0;
			}
			if (le->next->escena == esc) {
				break;
			}
			le = le->next;
		}
		CListaEscena **slot = &le->next;
		le = *slot;
		if ((le->flags & ESCENA_COMPARTIR) && !force) {
			return 0;
		}
		*slot = le->next;
	}
	if (!le) {
		return 0;
	}
	ListaEscena_Free(le);
	return 1;
}

void Modelo_Destroy(CModelo **pm, byte force) {
	CHECK(pm);
	if (!*pm) {
		return;
	}
	if (g_ListaEscenas) {
		CEscena *esc = (*pm)->escena;

		CHECK(esc);
		CHECK(esc->Refs > 1);
		Escena_Release(esc, force);
	}
	CListaModelo *lm = g_ListaModelos;
	if (lm) {
		CModelo *m = *pm;
		CListaModelo *found = NULL;

		if (lm->modelo == m) {
			g_ListaModelos = lm->next;
			found = lm;
		} else {
			while (lm->next) {
				if (lm->next->modelo == m) {
					CListaModelo **slot = &lm->next;
					found = *slot;
					*slot = found->next;
					break;
				}
				lm = lm->next;
			}
		}
		if (found) {
			CListaModelo_dtor(found);
			operator_delete_sized(found, sizeof(*found));
			*pm = NULL;
			return;
		}
	}
	if (*pm) {
		((modeloDelete_t)((void **)(*pm)->vftable)[0])(*pm, 1);
		*pm = NULL;
	}
}

void Modelo_SetEnable(CModelo *m, byte on) {
	if (m->enable != on) {
		m->enable = on;
		m->visible = 0;
		Modelo_UpdateRender(m);
	}
}

/* the CHECKs on an entry of gModelosRender */
static CListaModelo *Modelos_Render_Get(int i) {
	CListaModelo *lm = g_ModelosRender[i];

	CHECK(lm->flags & ESCENA_AUTOINIC);
	CHECK(lm->indiceRender == i);
	CHECK(lm->modelo->enable);
	return lm;
}

void Modelos_UpdateAnims(float dt) {
	for (int i = 0; i < g_NumModelosRender; i++) {
		CModelo_UpdateAnims(Modelos_Render_Get(i)->modelo, dt, -1);
	}
}

void UtilEscena_FreeAll(void) {
	if (g_ListaModelos) {
		Con_Warnf("WARN: Model list not empty.\n");
	}
	for (CListaEscena *le = g_ListaEscenas; le;) {
		CListaEscena *next = le->next;

		CHECK(le->escena->Refs == 1);
		ListaEscena_Free(le);
		le = next;
	}
	for (CListaModelo *lm = g_ListaModelos; lm;) {
		CListaModelo *next = lm->next;

		CListaModelo_dtor(lm);
		operator_delete_sized(lm, sizeof(*lm));
		lm = next;
	}
	g_ListaModelos = NULL;
	g_ListaEscenas = NULL;
}

void UtilEscena_ReleaseD3D(void) {
	void *dev = g_MotorDev;

	CHECK((g_pApp->flags >> 2 & 1) || dev);
	for (CListaEscena *le = g_ListaEscenas; le; le = le->next) {
		if (le->flags & ESCENA_AUTOINIC) {
			CEscena_ReleaseD3D(le->escena, dev);
		}
	}
	for (CListaModelo *lm = g_ListaModelos; lm; lm = lm->next) {
		CHECK(lm->flags & ESCENA_AUTOINIC);
		CModelo_ReleaseD3D(lm->modelo, dev, 0);
	}
}

void UtilEscena_InitD3D(void) {
	void *dev = g_MotorDev;

	CHECK(dev);
	for (CListaModelo *lm = g_ListaModelos; lm; lm = lm->next) {
		CHECK(lm->flags & ESCENA_AUTOINIC);
		CModelo_InitD3D(lm->modelo, dev, 0);
	}
	for (CListaEscena *le = g_ListaEscenas; le; le = le->next) {
		if (le->flags & ESCENA_AUTOINIC) {
			CEscena_InitD3D(le->escena, dev);
		}
	}
}

static const char *Dump_Flag(uint32_t flags, uint32_t bit, const char *text) {
	return flags & bit ? text : "";
}

static void Dump_Modelo(utilPrintf_t fn, CListaModelo *lm) {
	uint32_t f = lm->flags;
	CModelo *m = lm->modelo;

	fn("\n  '%s' (%08Xh %08Xh)%s%s%s", lm->name, m, m->escena, Dump_Flag(f, ESCENA_COMPARTIR, "  COMPARTIR"),
		Dump_Flag(f, ESCENA_AUTOINIC, "  AUTOINIC"), Dump_Flag(f, ESCENA_USARANIM, "  USARANIM"));
	fn("   Render: %d  Vis: %d", lm->modelo->enable, lm->modelo->visible);
}

void UtilEscena_Dump(utilPrintf_t fn) {
	int nModelos = 0, nRender = 0, nVis = 0, nEscenas = 0, nAnim = 0, nVisRender = 0;

	if (!fn) {
		fn = E3D_DebugPrintf;
	}
	fn("\nModelos:");
	for (CListaModelo *lm = g_ListaModelos; lm; lm = lm->next) {
		Dump_Modelo(fn, lm);
		nModelos++;
		if (lm->modelo->enable) {
			nRender++;
		}
		if (lm->modelo->visible & 1) {
			nVis++;
		}
	}
	fn("\n\nEscenas:");
	for (CListaEscena *le = g_ListaEscenas; le; le = le->next) {
		uint32_t f = le->flags;
		CEscena *e = le->escena;

		fn("\n  '%s' (%08Xh)%s%s%s%s%s", le->name, e, Dump_Flag(f, ESCENA_COMPARTIR, "  COMPARTIR"),
			Dump_Flag(f, ESCENA_AUTOINIC, "  AUTOINIC"), Dump_Flag(f, ESCENA_USARANIM, "  USARANIM"),
			Dump_Flag(f, ESCENA_IGN_ESCALA, "  IGN_ESCALA"), e->AnimM3D ? "  [AnimM3D]" : "");
		fn("   escala: %s", Vec3_Str(&le->escala, 0, 6));
		nEscenas++;
		if (le->escena->AnimM3D && !le->escena->Nodo3D) {
			nAnim++;
		}
	}
	fn("\n\nModelos activos:");
	for (int i = 0; i < g_NumModelosRender; i++) {
		CListaModelo *lm = g_ModelosRender[i];

		Dump_Modelo(fn, lm);
		if (lm->modelo->visible & 1) {
			nVisRender++;
		}
	}
	fn("\n\n%d modelos (%d render, %d vis), %d escenas (%d anim)\n\n", nModelos, nRender, nVis, nEscenas, nAnim);
	CHECK(nRender == g_NumModelosRender);
	CHECK(nVis == nVisRender);
}

void Escenas_Msg(int msg, int a, int b, int c, int d) {
	for (CListaEscena *le = g_ListaEscenas; le; le = le->next) {
		if (le->flags & ESCENA_AUTOINIC) {
			CEscena_Msg(le->escena, msg, a, b, c, d);
		}
	}
}

CEscena *Escena_Next(CEscena *esc) {
	CListaEscena *le = g_ListaEscenas;

	if (esc) {
		for (;;) {
			if (!le) {
				return NULL;
			}
			CEscena *e = le->escena;
			le = le->next;
			if (e == esc) {
				break;
			}
		}
	}
	if (!le) {
		return NULL;
	}
	return le->escena;
}

void Modelos_Render(void) {
	if (!g_ModelosOn) {
		return;
	}
	vec3_t cam = *Render_GetEyePos();
	int n = g_NumModelosRender;
	for (int i = 0; i < n; i++) {
		CListaModelo *lm = Modelos_Render_Get(i);
		CModelo *m = lm->modelo;
		float dy = m->pos.y - cam.y;
		float dx = m->pos.x - cam.x;
		float dz = m->pos.z - cam.z;
		float key = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * m->sortZMult + m->sortZBias;

		lm->sortKey = key;
		int pass = g_RenderPass;
		if (pass == 1 || pass == 2 || pass == 0xb || pass == 0xc) {
			lm->sortKey = -key;
		}
		if (g_SortInvert) {
			lm->sortKey = -lm->sortKey;
		}
	}
	if (g_SortModels) {
		crt_qsort(g_ModelosRender, n, sizeof(CListaModelo *), ListaModelo_CompareKey);
	}
	for (int i = 0; i < g_NumModelosRender; i++) {
		g_ModelosRender[i]->indiceRender = i;
		CModelo_Render(g_ModelosRender[i]->modelo);
	}
	int pass = g_RenderPass;
	if (!g_SortShow || (pass != 0 && pass != 3 && pass != 0xd)) {
		return;
	}
	int y = 0x34;
	n = g_NumModelosRender;
	for (int k = 0; k < n; k++, y += 4) {
		CListaModelo *lm = g_ModelosRender[n - k - 1];
		CModelo *m = lm->modelo;
		byte vis = m->visible;

		if (!vis && g_SortShow < 2) {
			continue;
		}
		Font_SetColor(g_DebugFont, vis & 1 ? 0xffffff : vis ? 0xffff00 : 0xff);
		Font_Printf(g_DebugFont, SORT_TEXT_X, (float)y + SORT_TEXT_Y, 2, "%s %.2f", m->name, (double)lm->sortKey);
		if (m->sortZBias != 0.0f || m->sortZMult != 1.0f) {
			Font_SetColor(g_DebugFont, SORT_TEXT_COLOR);
			float *f = g_DebugFont;
			Font_Printf(f, f[0], f[1], 2, " %.2f %.2f", (double)m->sortZBias, (double)m->sortZMult);
		}
		n = g_NumModelosRender;
		y += 6;
	}
}

CListaEscena *__thiscall CListaEscena_Find(CListaEscena *self, const char *name, const char *name2,
	const vec3_t *escala, uint32_t flags, uint32_t mask) {
	for (CListaEscena *le = self; le; le = le->next) {
		if ((le->flags & mask) == (flags & mask) && Cad_EqualNoCase(le->name, name) &&
			Cad_EqualNoCase(le->name2, name2) && ((flags & ESCENA_IGN_ESCALA) || (le->escala.x == escala->x &&
			le->escala.y == escala->y && le->escala.z == escala->z))) {
			return le;
		}
	}
	return NULL;
}

CListaModelo *__thiscall CListaModelo_Find(CListaModelo *self, const char *name, const char *name2, uint32_t flags,
	uint32_t mask) {
	for (CListaModelo *lm = self; lm; lm = lm->next) {
		if ((lm->flags & mask) == (flags & mask) && Cad_EqualNoCase(lm->name, name) &&
			Cad_EqualNoCase(lm->name2, name2)) {
			return lm;
		}
	}
	return NULL;
}

CModelo *Modelo_FindName(const char *name) {
	for (CListaModelo *lm = g_ListaModelos; lm; lm = lm->next) {
		CHECK(lm->modelo);
		const char *n = lm->modelo->name;
		if (n && !strcmp(name, n)) {
			return lm->modelo;
		}
	}
	return NULL;
}

int Cad_EqualNoCase(const char *a, const char *b) {
	if (a) {
		if (b) {
			return !crt_stricmp(a, b);
		}
		if (*a) {
			return 0;
		}
	}
	return !b || !*b;
}

void Crear_Animacion(CModelo *m, const char *name, uint32_t flags) {
	CEscena *esc = m ? m->escena : NULL;
	byte shared;

	CHECK(esc);
	const char *name2 = esc->nodo;
	E3D_DebugPrintf("\ncrear_animacion(): '%s'", name);
	g_EscenaAnimPadre = esc;
	CEscena *anim = Crear_Escena(&shared, name, name2, &m->escala, esc->umbralUnirVert, flags);
	g_EscenaAnimPadre = NULL;
	CHECK(anim && anim->AnimM3D);
	CModelo_AddAnim(m, anim, NULL);
}

CEscena *Crear_Escena(byte *shared, const char *name, const char *name2, const vec3_t *escala, float umbral,
	uint32_t flags) {
	CEscena *esc;
	void *p;

	CHECK(name && shared);
	uint32_t compartir = flags & ESCENA_COMPARTIR;
	if (compartir && g_ListaEscenas) {
		CListaEscena *le = CListaEscena_Find(g_ListaEscenas, name, name2, escala, flags, ESCENA_COMPARTIR);

		if (le) {
			CHECK(le->escena);
			esc = le->escena;
			*shared = 1;
			if (esc) {
				goto made;
			}
		}
	}
	if ((flags & ESCENA_SM3) && (esc = SM3_Load(name)) != NULL) {
		*shared = 0;
		E3D_DebugPrintf("  [sm3]");
		p = operator_new(sizeof(CListaEscena));
		if (p) {
			CListaEscena_ctor(p, esc, name, name2, escala, flags, &g_ListaEscenas);
		}
		if (!g_MotorDev) {
			return esc;
		}
		CHECK(flags & ESCENA_AUTOINIC);
		goto initD3D;
	}
	esc = Escena_Load(name, 1, name2);
	*shared = 0;
made:
	E3D_DebugPrintf("  [%s]", *shared ? "compartido" : "creado");
	if (!*shared) {
		p = operator_new(sizeof(CListaEscena));
		if (p) {
			CListaEscena_ctor(p, esc, name, name2, escala, flags, &g_ListaEscenas);
		}
		esc->umbralUnirVert = umbral;
		CEscena_SetEscala(esc, escala);
		CEscena_UpdateBox(esc, 0, 1);
		if (compartir && !g_EscenaAnimPadre) {
			CEscena_Shared(esc);
		}
	} else {
		CHECK(esc->umbralUnirVert == umbral);
	}
	if (*shared) {
		return esc;
	}
	uint32_t autoinic = flags & ESCENA_AUTOINIC;
	if (autoinic) {
		CEscena_Msg(esc, 1, 0, 0, 0, 0);
		CEscena_Msg(esc, 2, 0, 0, 0, 0);
		if (flags & ESCENA_F20) {
			Escena_PostLoad(name, esc);
		}
	}
	if (*shared || !g_MotorDev) {
		return esc;
	}
	CHECK(autoinic);
initD3D:
	CEscena_InitD3D(esc, g_MotorDev);
	CListaMat_Prepare(g_ListaMat, g_MotorDev);
	return esc;
}

CModelo *Crear_Modelo(const char *name, const char *name2, const vec3_t *escala, float umbral, uint32_t flags) {
	CModelo *m = operator_new(sizeof(CModelo));
	byte shared;

	m = m ? CModelo_ctor(m) : NULL;
	E3D_DebugPrintf("\ncrear_modelo(): '%s'", name);
	CEscena *esc = Crear_Escena(&shared, name, name2, escala, umbral, flags);
	Cad_Set(&m->name, Ruta_FileName(esc->name));
	m->escala = *escala;
	CHECK(esc->Nodo3D);
	CModelo_SetEscena(m, esc);
	if ((flags & ESCENA_USARANIM) && esc->AnimM3D) {
		CModelo_AddAnim(m, esc, NULL);
	}
	uint32_t autoinic = flags & ESCENA_AUTOINIC;
	if (autoinic) {
		void *p = operator_new(sizeof(CListaModelo));

		if (p) {
			CListaModelo_ctor(p, m, name, name2, flags, &g_ListaModelos);
		}
	}
	if (g_MotorDev && autoinic) {
		CModelo_InitD3D(m, g_MotorDev, 0);
	}
	return m;
}

CModelo *Crear_Modelo3(const char *name, const char *name2, const vec3_t *escala, uint32_t flags) {
	return Crear_Modelo(name, name2, escala, -1.0f, flags);
}

CModelo *Crear_ModeloLOD(const char *name, const char *name2, const vec3_t *escala, CModelo *main, uint32_t flags) {
	g_EscenaLODPadre = main->escena;
	CModelo *m = Crear_Modelo(name, name2, escala, -1.0f, flags);
	g_EscenaLODPadre = NULL;
	CModelo_Disable(m);
	return m;
}

CModelo *Crear_Modelo2(const char *name, const char *name2, uint32_t flags) {
	vec3_t one = { 1.0f, 1.0f, 1.0f };

	return Crear_Modelo(name, name2, &one, -1.0f, flags);
}

CModelo *Crear_Modelo4(const char *name, const char *name2, float umbral, uint32_t flags) {
	vec3_t one = { 1.0f, 1.0f, 1.0f };

	return Crear_Modelo(name, name2, &one, umbral, flags);
}

int ListaModelo_CompareKey(const void *a, const void *b) {
	const CListaModelo *la = *(CListaModelo *const *)a;
	const CListaModelo *lb = *(CListaModelo *const *)b;

	return lb->sortKey > la->sortKey ? 1 : -1;
}
