/*
 * Animation groups of the engine3d, original source Engine/AnimGrupo.cpp (include/scrap/animgroup.h).
 */
#include <string.h>

#include "scrap/animgroup.h"
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

/* the events of the list deleted */
static void AnimEvents_Free(CAnimEvent **list) {
	CAnimEvent *ev = *list;
	if (ev) {
		CAnimEvent_dtor(ev);
		operator_delete(ev);
		*list = NULL;
	}
}

static void CAnimGroup_dtor(CAnimGroup *self) {
	CHECK(self->Anim);
	ObjRef_Release(&self->Anim);
	if (self->morph) {
		/* new[] of MAX_MORPHDATA, deleted with the size of one */
		operator_delete(self->morph);
		self->morph = NULL;
	}
	CAnimCtrl_dtor(&self->ctrl);
	AnimEvents_Free(&self->events);
	if (self->name) {
		String_Set(&self->name, NULL);
	}
}

CAnimGroups *CAnimGroups_ctor(CAnimGroups *self) {
	memset(self, 0, sizeof(*self));
	return self;
}

void CAnimGroups_Clear(CAnimGroups *self) {
	for (int i = 0; i < MAX_ANIMGROUPS; i++) {
		CAnimGroup *g = self->g[i];
		if (g) {
			CAnimGroup_dtor(g);
			operator_delete(g);
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

/* "<i>_Data" (6 bytes from Target on) and "<i>_Tag" of the section; 0 without the data */
static byte MorphData_Read(CIniFile *ini, const char *sect, int i, MorphData *d, int targets) {
	char key[32];
	char tmp[256];

	Bprintf(key, sizeof(key), "%02d_Data", i);
	if (!CIniFile_GetText(ini, sect, key, tmp, sizeof(tmp), NULL)) {
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
	if (CIniFile_GetText(ini, sect, key, tmp, sizeof(tmp), NULL)) {
		CHECK(strlen(tmp) >= sizeof(d->Tag));
		memcpy(d->Tag, tmp, sizeof(d->Tag));
	}
	return 1;
}

int CAnimGroups_Add(CAnimGroups *self, CAnimGroup *g) {
	CHECK(g);
	int i;
	for (i = 0; i < MAX_ANIMGROUPS; i++) {
		if (!self->g[i]) {
			self->g[i] = g;
			return i;
		}
	}
	CHECK(0);
	return i;
}

CAnimGroup *CAnimGroups_Get(CAnimGroups *self, uint32_t i) {
	if (i < MAX_ANIMGROUPS) {
		return self->g[i];
	}
	return NULL;
}

CAnimGroup *CAnimGroups_Find(CAnimGroups *self, const char *name, int *idx) {
	if (!name) {
		return NULL;
	}
	for (int i = 0; i < MAX_ANIMGROUPS; i++) {
		CAnimGroup *g = self->g[i];
		if (g && !crt_stricmp(g->name, name)) {
			if (idx) {
				*idx = i;
			}
			return self->g[i];
		}
	}
	return NULL;
}

int CAnimGroups_Count(CAnimGroups *self) {
	int n = 0;
	for (int i = 0; i < MAX_ANIMGROUPS; i++) {
		if (self->g[i]) {
			n++;
		}
	}
	return n;
}

byte AnimGroup_AlignNode(CScene *scene, CScene *anim, const char *node) {
	if (!node) {
		return 0;
	}
	CHECK(scene && anim);
	CHECK(scene->Node3D);
	CHECK(anim->AnimM3D);
	CHECK(anim->Node3D);
	CNode3D *node1 = CNode3D_Find(scene->Node3D, node, 1);
	CNode3D *node2 = CNode3D_Find(anim->Node3D, node, 1);
	if (!node2) {
		for (int i = 0; i < anim->AnimM3D->num_objects; i++) {
			CHECK(CAnimNode_GetFlags(CAnimation_GetNode(anim->AnimM3D, i)) == ANIM_VERT);
		}
		anim->AnimM3D->flag = 1;
		return 1;
	}
	CHECK(node1 && node2 && anim && anim->AnimM3D);
	if (!node1 || !anim->AnimM3D) {
		return 0;
	}
	CAnimNode *an = CAnimation_GetNode(anim->AnimM3D, node2->index);
	if (!an) {
		return 0;
	}
	vec3_t pos;
	quat_t rot;
	vec3_t d;
	if (CAnimNode_GetPosRot(an, &pos, &rot, an->first_frame, 0.0f)) {
		d.x = node1->pos.x - pos.x;
		d.y = node1->pos.y - pos.y;
		d.z = node1->pos.z - pos.z;
	} else {
		d.x = node1->pos.x - node2->pos.x;
		d.y = node1->pos.y - node2->pos.y;
		d.z = node1->pos.z - node2->pos.z;
	}
	CAnimNode_Translate(an, &d);
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

CAnimGroup *AnimGroup_New(CScene *scene, CScene *anim, const char *name) {
	CHECK(scene && anim);
	CHECK(scene->Node3D);
	CHECK(anim->AnimM3D);
	CHECK(!anim->Node3D || (scene == anim));
	CAnimGroup *g = operator_new(sizeof(*g));
	if (g) {
		g->name = NULL;
		g->events = NULL;
		CAnimCtrl_ctor(&g->ctrl);
		g->Anim = NULL;
		g->morph = NULL;
	}
	g->Anim = anim;
	anim->Refs++;
	if (!name || !*name) {
		name = g->Anim->AnimM3D->name;
	}
	String_Set(&g->name, name);
	g->ctrl.vel = g->Anim->AnimM3D->fps;
	CNode3D *n = CNode3D_Find(scene->Node3D, scene->node, 1);
	if (n) {
		g->ctrl.node = CAnimation_GetNodeTable(g->Anim->AnimM3D, n->index);
	}
	CAnimCtrl_SetFrames(&g->ctrl, g->Anim->AnimM3D->first_frame, g->Anim->AnimM3D->num_frames);
	g->ctrl.events = &g->events;
	int last = (int)((uint32_t)g->ctrl.firstFrame - 1 + (uint32_t)g->ctrl.frames);
	CAnimEvent *ev = operator_new(sizeof(*ev));
	if (ev) {
		ev = CAnimEvent_ctor(ev, (float)last, ANIMEVENT_END, 0);
	}
	AnimEvents_Add(&g->events, ev);
	if (!anim->AnimM3D->flag) {
		return g;
	}
	int targets = g->ctrl.frames;
	g->morph = operator_new_array(MAX_MORPHDATA * sizeof(MorphData));
	g->morphAct = 0;
	Morph_Init(g->morph, targets);
	if (!scene->ini) {
		return g;
	}
	char sect[64];
	Bprintf(sect, sizeof(sect), "Morphing:%s", g->name);
	if (!CIniFile_FindSection(scene->ini, sect, 1)) {
		E3D_LogBlue((&g_FileLog), "Morphing sequence data not found: [%s]", sect);
	}
	for (int i = 0; i < MAX_MORPHDATA; i++) {
		if (MorphData_Read(scene->ini, sect, i, &g->morph[i], targets) && i > 0 && !Morph_Tag(&g->morph[i])) {
			Morph_SetTag(&g->morph[i], Morph_Tag(&g->morph[i - 1]));
		}
	}
	return g;
}

void AnimGroup_MapNodes(CScene *scene, CScene *anim) {
	CHECK(scene && anim);
	CHECK(scene->Node3D);
	CHECK(anim->AnimM3D);
	CHECK(anim->Node3D);
	int max = -1;
	for (CNode3D *n = scene->Node3D; n; n = n->next) {
		CHECK(n->scene == scene);
		if (n->index > max) {
			max = n->index;
		}
	}
	Table_Alloc1(&anim->AnimM3D->table, max + 1);
	CTable *t = &anim->AnimM3D->table;
	CHECK(t->p);
	memset(t->p, 0, t->n);
	for (CNode3D *n = scene->Node3D; n; n = n->next) {
		CNode3D *node2 = CNode3D_Find(anim->Node3D, n->name, 1);
		byte ok = 0;
		while (node2) {
			/* the same name: the one under a parent of the same name, else the last one */
			if ((n->parent == NULL) == (node2->parent == NULL)) {
				if (!node2->parent || !strcmp(n->parent->name, node2->parent->name)) {
					ok = 1;
					break;
				}
				if (!CNode3D_Find(node2, n->name, 0)) {
					Con_Warnf("Parent mismatch [Node: %s] [Anim: %s]\n", n->name, anim->AnimM3D->name);
					ok = 1;
					break;
				}
			}
			node2 = CNode3D_Find(node2, n->name, 0);
		}
		CHECK(!ok == !node2);
		if (n->index != -1 && node2) {
			CHECK((uint32_t)node2->index <= 254);
		}
		if (n->index < 0) {
			continue;
		}
		uint32_t data = 0;
		if (ok) {
			data = node2->index + 1;
			CHECK(data <= 255);
		}
		t = &anim->AnimM3D->table;
		CHECK(t->p && (uint32_t)n->index < (uint32_t)t->n);
		((byte *)t->p)[n->index] = (byte)data;
	}
}

void AnimGroup_Compact(CScene *anim) {
	for (CNode3D *n = anim->Node3D; n; n = n->next) {
		CHECK(n->obj3D == NULL);
	}
	for (int i = 0; i < anim->AnimM3D->num_objects; i++) {
		if (CAnimNode_GetFlags(CAnimation_GetNode(anim->AnimM3D, i))) {
			continue;
		}
		for (int j = i; j < anim->AnimM3D->num_objects; j++) {
			if (!CAnimNode_GetFlags(CAnimation_GetNode(anim->AnimM3D, j))) {
				continue;
			}
			CAnimation_SwapNodes(anim->AnimM3D, i, j);
			CNode3D *node3d_i = CScene_NodeObject(anim, i);
			CNode3D *node3d_j = CScene_NodeObject(anim, j);
			if (node3d_i) {
				node3d_i->index = j;
			}
			CHECK(node3d_j);
			node3d_j->index = i;
			break;
		}
	}
	CAnimation_Trim(anim->AnimM3D);
}
