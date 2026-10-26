/*
 * Animation groups of the engine3d, original source Engine/AnimGrupo.cpp (include/scrap/animgrupo.h).
 */
#include <string.h>

#include "scrap/animgrupo.h"
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/material.h"

#define MORPH_INTERP 25
#define MORPH_PARAM7 100


static uint16_t Morph_Tag(const MorphData *d) {
	uint16_t t;
	memcpy(&t, d->Tag, sizeof(t));
	return t;
}

static void Morph_SetTag(MorphData *d, uint16_t t) {
	memcpy(d->Tag, &t, sizeof(t));
}

void __fastcall AnimEventos_Free(CAnimEvent **list) {
	CAnimEvent *ev = *list;
	if (ev) {
		CAnimEvent_dtor(ev);
		operator_delete_sized(ev, sizeof(*ev));
		*list = NULL;
	}
}

void __fastcall CAnimGrupo_dtor(CAnimGrupo *self) {
	CHECK(self->Anim);
	ObjRef_Release(&self->Anim);
	if (self->morph) {
		/* new[] of MAX_MORPHDATA, deleted with the size of one */
		operator_delete_sized(self->morph, sizeof(MorphData));
		self->morph = NULL;
	}
	CAnimCtrl_dtor(&self->ctrl);
	AnimEventos_Free(&self->eventos);
	if (self->name) {
		Cad_Set(&self->name, NULL);
	}
}

CAnimGrupos *__fastcall CAnimGrupos_ctor(CAnimGrupos *self) {
	memset(self, 0, sizeof(*self));
	return self;
}

CAnimGrupo *__fastcall CAnimGrupo_ctor(CAnimGrupo *self) {
	self->name = NULL;
	self->eventos = NULL;
	CAnimCtrl_ctor(&self->ctrl);
	self->Anim = NULL;
	self->morph = NULL;
	return self;
}

void __fastcall CAnimGrupos_Clear(CAnimGrupos *self) {
	for (int i = 0; i < MAX_ANIMGRUPOS; i++) {
		CAnimGrupo *g = self->g[i];
		if (g) {
			CAnimGrupo_dtor(g);
			operator_delete_sized(g, sizeof(*g));
			self->g[i] = NULL;
		}
	}
}

int MorphData_Find(const MorphData *md, const char *tag) {
	CHECK(md && tag && tag[0]);
	uint16_t t = (uint16_t)((byte)tag[1] * 0x100 + (byte)tag[0]);
	for (int i = 0; i < MAX_MORPHDATA; i++) {
		if (Morph_Tag(&md[i]) == t) {
			return i;
		}
	}
	return -1;
}

byte MorphData_Read(CArchINI *ini, const char *sect, int i, MorphData *d, int targets) {
	char key[32];
	char tmp[256];

	Bprintf(key, sizeof(key), "%02d_Data", i);
	if (!CArchINI_GetText(ini, sect, key, tmp, sizeof(tmp), NULL)) {
		return 0;
	}
	if (Proc_Scan(tmp, "&b&b&b&b&b&b", &d->Target, &d->param3, &d->Interp, &d->NextMin, &d->NextMax,
			&d->param7) != 6) {
		Con_FatalErrorf("Invalid parameter: [%s] %s = %s", sect, key, tmp);
		return 0;
	}
	CHECK(d->NextMin < MAX_MORPHDATA);
	CHECK(d->NextMax < MAX_MORPHDATA);
	CHECK(d->Target < targets);
	CHECK(d->Interp > 0);
	Morph_SetTag(d, 0);
	Bprintf(key, sizeof(key), "%02d_Tag", i);
	if (CArchINI_GetText(ini, sect, key, tmp, sizeof(tmp), NULL)) {
		CHECK(strlen(tmp) >= sizeof(d->Tag));
		memcpy(d->Tag, tmp, sizeof(d->Tag));
	}
	return 1;
}

int __thiscall CAnimGrupos_Add(CAnimGrupos *self, CAnimGrupo *g) {
	CHECK(g);
	int i;
	for (i = 0; i < MAX_ANIMGRUPOS; i++) {
		if (!self->g[i]) {
			self->g[i] = g;
			return i;
		}
	}
	CHECK(0);
	return i;
}

CAnimGrupo *__thiscall CAnimGrupos_Get(CAnimGrupos *self, uint32_t i) {
	if (i < MAX_ANIMGRUPOS) {
		return self->g[i];
	}
	return NULL;
}

CAnimGrupo *__thiscall CAnimGrupos_Find(CAnimGrupos *self, const char *name, int *idx) {
	if (!name) {
		return NULL;
	}
	for (int i = 0; i < MAX_ANIMGRUPOS; i++) {
		CAnimGrupo *g = self->g[i];
		if (g && !crt_stricmp(g->name, name)) {
			if (idx) {
				*idx = i;
			}
			return self->g[i];
		}
	}
	return NULL;
}

int __fastcall CAnimGrupos_Count(CAnimGrupos *self) {
	int n = 0;
	for (int i = 0; i < MAX_ANIMGRUPOS; i++) {
		if (self->g[i]) {
			n++;
		}
	}
	return n;
}

byte AnimGrupo_AlignNode(CEscena *escena, CEscena *anim, const char *nodo) {
	if (!nodo) {
		return 0;
	}
	CHECK(escena && anim);
	CHECK(escena->Nodo3D);
	CHECK(anim->AnimM3D);
	CHECK(anim->Nodo3D);
	CNodo3D *nodo1 = CNodo3D_Find(escena->Nodo3D, nodo, 1);
	CNodo3D *nodo2 = CNodo3D_Find(anim->Nodo3D, nodo, 1);
	if (!nodo2) {
		for (int i = 0; i < anim->AnimM3D->num_objetos; i++) {
			CHECK(CAnimNodo_GetFlags(CAnimacion_GetNodo(anim->AnimM3D, i)) == ANIM_VERT);
		}
		anim->AnimM3D->flag = 1;
		return 1;
	}
	CHECK(nodo1 && nodo2 && anim && anim->AnimM3D);
	if (!nodo1 || !anim->AnimM3D) {
		return 0;
	}
	CAnimNodo *an = CAnimacion_GetNodo(anim->AnimM3D, nodo2->indice);
	if (!an) {
		return 0;
	}
	vec3_t pos;
	quat_t rot;
	vec3_t d;
	if (CAnimNodo_GetPosRot(an, &pos, &rot, an->primer_frame, 0.0f)) {
		d.x = nodo1->pos.x - pos.x;
		d.y = nodo1->pos.y - pos.y;
		d.z = nodo1->pos.z - pos.z;
	} else {
		d.x = nodo1->pos.x - nodo2->pos.x;
		d.y = nodo1->pos.y - nodo2->pos.y;
		d.z = nodo1->pos.z - nodo2->pos.z;
	}
	CAnimNodo_Translate(an, &d);
	return 1;
}

/* the defaults: each step its own target, the next ones the following target */
static void Morph_Init(MorphData *md, int targets) {
	int target = 0;
	for (int i = 0; i < MAX_MORPHDATA; i++) {
		MorphData *d = &md[i];
		Morph_SetTag(d, 0);
		d->Target = (byte)target;
		d->param3 = 0;
		d->Interp = MORPH_INTERP;
		target = (i + 1) % targets;
		d->NextMin = (byte)target;
		d->NextMax = (byte)target;
		d->param7 = MORPH_PARAM7;
	}
}

CAnimGrupo *AnimGrupo_New(CEscena *escena, CEscena *anim, const char *nombre) {
	CHECK(escena && anim);
	CHECK(escena->Nodo3D);
	CHECK(anim->AnimM3D);
	CHECK(!anim->Nodo3D || (escena == anim));
	CAnimGrupo *g = operator_new(sizeof(*g));
	if (g) {
		g->name = NULL;
		g->eventos = NULL;
		CAnimCtrl_ctor(&g->ctrl);
		g->Anim = NULL;
		g->morph = NULL;
	}
	g->Anim = anim;
	anim->Refs++;
	if (!nombre || !*nombre) {
		nombre = g->Anim->AnimM3D->name;
	}
	Cad_Set(&g->name, nombre);
	g->ctrl.vel = g->Anim->AnimM3D->fps;
	CNodo3D *n = CNodo3D_Find(escena->Nodo3D, escena->nodo, 1);
	if (n) {
		g->ctrl.nodo = CAnimacion_GetNodoTabla(g->Anim->AnimM3D, n->indice);
	}
	CAnimCtrl_SetFrames(&g->ctrl, g->Anim->AnimM3D->primer_frame, g->Anim->AnimM3D->num_frames);
	g->ctrl.eventos = &g->eventos;
	int last = (int)((uint32_t)g->ctrl.primero - 1 + (uint32_t)g->ctrl.frames);
	CAnimEvent *ev = operator_new(sizeof(*ev));
	if (ev) {
		ev = CAnimEvent_ctor(ev, (float)last, ANIMEVENT_FIN, 0);
	}
	AnimEventos_Add(&g->eventos, ev);
	if (!anim->AnimM3D->flag) {
		return g;
	}
	int targets = g->ctrl.frames;
	g->morph = operator_new_array(MAX_MORPHDATA * sizeof(MorphData));
	g->morphAct = 0;
	Morph_Init(g->morph, targets);
	if (!escena->ini) {
		return g;
	}
	char sect[64];
	Bprintf(sect, sizeof(sect), "Morphing:%s", g->name);
	if (!CArchINI_FindSection(escena->ini, sect, 1)) {
		E3D_LogBlue(g_FileLog, "Morphing sequence data not found: [%s]", sect);
	}
	for (int i = 0; i < MAX_MORPHDATA; i++) {
		if (MorphData_Read(escena->ini, sect, i, &g->morph[i], targets) && i > 0 && !Morph_Tag(&g->morph[i])) {
			Morph_SetTag(&g->morph[i], Morph_Tag(&g->morph[i - 1]));
		}
	}
	return g;
}

void AnimGrupo_MapNodes(CEscena *escena, CEscena *anim) {
	CHECK(escena && anim);
	CHECK(escena->Nodo3D);
	CHECK(anim->AnimM3D);
	CHECK(anim->Nodo3D);
	int max = -1;
	for (CNodo3D *n = escena->Nodo3D; n; n = n->sig) {
		CHECK(n->escena == escena);
		if (n->indice > max) {
			max = n->indice;
		}
	}
	Tabla_Alloc1(&anim->AnimM3D->tabla, max + 1);
	CTabla *t = &anim->AnimM3D->tabla;
	CHECK(t->p);
	memset(t->p, 0, t->n);
	for (CNodo3D *n = escena->Nodo3D; n; n = n->sig) {
		CNodo3D *nodo2 = CNodo3D_Find(anim->Nodo3D, n->name, 1);
		byte ok = 0;
		while (nodo2) {
			/* the same name: the one under a parent of the same name, else the last one */
			if ((n->padre == NULL) == (nodo2->padre == NULL)) {
				if (!nodo2->padre || !strcmp(n->padre->name, nodo2->padre->name)) {
					ok = 1;
					break;
				}
				if (!CNodo3D_Find(nodo2, n->name, 0)) {
					Con_Warnf("Parent mismatch [Node: %s] [Anim: %s]\n", n->name, anim->AnimM3D->name);
					ok = 1;
					break;
				}
			}
			nodo2 = CNodo3D_Find(nodo2, n->name, 0);
		}
		CHECK(!ok == !nodo2);
		if (n->indice != -1 && nodo2) {
			CHECK((uint32_t)nodo2->indice <= 254);
		}
		if (n->indice < 0) {
			continue;
		}
		uint32_t dato = 0;
		if (ok) {
			dato = nodo2->indice + 1;
			CHECK(dato <= 255);
		}
		t = &anim->AnimM3D->tabla;
		CHECK(t->p && (uint32_t)n->indice < (uint32_t)t->n);
		((byte *)t->p)[n->indice] = (byte)dato;
	}
}

void AnimGrupo_Compact(CEscena *anim) {
	for (CNodo3D *n = anim->Nodo3D; n; n = n->sig) {
		CHECK(n->obj3D == NULL);
	}
	for (int i = 0; i < anim->AnimM3D->num_objetos; i++) {
		if (CAnimNodo_GetFlags(CAnimacion_GetNodo(anim->AnimM3D, i))) {
			continue;
		}
		for (int j = i; j < anim->AnimM3D->num_objetos; j++) {
			if (!CAnimNodo_GetFlags(CAnimacion_GetNodo(anim->AnimM3D, j))) {
				continue;
			}
			CAnimacion_SwapNodos(anim->AnimM3D, i, j);
			CNodo3D *nodo3d_i = CEscena_NodoObjeto(anim, i);
			CNodo3D *nodo3d_j = CEscena_NodoObjeto(anim, j);
			if (nodo3d_i) {
				nodo3d_i->indice = j;
			}
			CHECK(nodo3d_j);
			nodo3d_j->indice = i;
			break;
		}
	}
	CAnimacion_Trim(anim->AnimM3D);
}
