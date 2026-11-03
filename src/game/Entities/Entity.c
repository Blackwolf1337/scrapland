/*
 * Entities/Entity.cpp of the game: cEntity, the base class of every game entity (include/scrap/entity.h). The
 * entity is in the name table and the entity ring of the world, has a node for the dead list, the collision grid
 * and its entity list, and the head of the script handles. Its AI and local logic answer the script variables
 * that start with "AI_" and "LLogic".
 */
#include <string.h>

#include "scrap/collisiongrid.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/cstring.h"
#include "scrap/hashtable.h"
#include "scrap/scrapcollision.h"
#include "scrap/world.h"

#define CENTITY_VFTABLE (&cEntity_vftable)
static const cEntityVtbl cEntity_vftable;
#define ENTITY_CLASSNAME "Entity"

/* the variable methods of the AI (from vtable slot 7) and of the local logic (from slot 3) */
typedef struct cEntVars {
	byte(*VarI)(void *self, const char *name, int *i, int set);
	byte(*VarF)(void *self, const char *name, double *d, int set);
	byte(*VarS)(void *self, const char *name, char **s, int set);
	byte(*VarV)(void *self, const char *name, double *x, double *y, double *z, int set);
} cEntVars;

static inline const cEntVars *AI_Vars(void *ai) {
	return (const cEntVars *)(*(void ***)ai + 7);
}

static inline const cEntVars *LLogic_Vars(void *logic) {
	return (const cEntVars *)(*(void ***)logic + 3);
}

/* the deleting destructor in slot 0 */
static inline void Obj_Delete(void *obj) {
	(*(void(**)(void *, int)) * (void **)obj)(obj, 1);
}

/* the entity of a ring node, NULL at the head */
static inline cEntity *Node_Entity(const Nto1Node *n) {
	if (n->data && n) {
		return n->data;
	}
	return NULL;
}

cEntity *cEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cEntity *self = mem;
	void *value;

	self->vftable = CENTITY_VFTABLE;
	cString_ctor((cString *)&self->name);
	Nto1Node_Init(&self->worldNode);
	Nto1Node_Init(&self->deadNode);
	Nto1Node_Init(&self->gridNode);
	Nto1Node_Init(&self->listNode.node);
	self->listNode.numElems = NULL;
	Nto1Node_Init(&self->links);
	if (!*name || strlen(name) >= 0x7c) {
		Con_Bluef("ERROR: '%s' invalid size name.\n", name);
		CHECK(1 == 0);
	}
	cString_Set((cString *)&self->name, name);
	/* a taken name gets the counter of the world appended until it is free */
	value = self;
	for (;;) {
		HashTable *t = &g_pWorld->entities;
		const char *key = self->name;
		CHECK(!t->locked);
		uint32_t slot = HashTable_Slot(t, key);
		if (!PtrTable_Find(t, key, slot)) {
			PtrTable_Insert(t, key, slot, &value);
			break;
		}
		cString_Printf((cString *)&self->name, "%s%i", name, g_pWorld->nameCount);
		g_pWorld->nameCount++;
	}
	self->worldNode.data = self;
	self->deadNode.data = self;
	self->gridNode.data = self;
	self->links.target = self;
	Nto1Node_LinkTo(&g_pWorld->entityList, &self->worldNode);
	self->pos.x = x;
	self->pos.y = y;
	self->pos.z = z;
	self->savedMask = 0;
	self->mask = 0;
	self->active = 1;
	self->listNode.node.data = self;
	self->lLogic = NULL;
	self->ai = NULL;
	return self;
}

/* rva 0xeeb20 and 0xeeb90 (the cells of the grid) */
void Nto1Node_dtor(Nto1Node *node) {
	Nto1Node_Done(node);
}

void EntityDList_dtor(void *list) {
	EntityDNode *self = list;

	if (!self->node.data) {
		CHECK(self->numElems);
		operator_delete_sized(self->numElems, 4);
	} else if (self->numElems) {
		(*self->numElems)--;
		self->numElems = NULL;
	}
	Nto1Node_Done(&self->node);
}

void cEntity_dtor(cEntity *self) {
	self->vftable = CENTITY_VFTABLE;
	if (self->lLogic) {
		Obj_Delete(self->lLogic);
	}
	if (self->ai) {
		Obj_Delete(self->ai);
	}
	Nto1Node_Done(&self->links);
	EntityDList_dtor(&self->listNode);
	Nto1Node_Done(&self->gridNode);
	Nto1Node_Done(&self->deadNode);
	Nto1Node_Done(&self->worldNode);
	cString_dtor((cString *)&self->name);
}

static cEntity *cEntity_Delete(cEntity *self, uint32_t flags) {
	cEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cEntity));
	}
	return self;
}

/* node joins the ring of head and counts in its numElems */
static void EntityDList_Add(void *head, EntityDNode *node) {
	EntityDNode *h = head;

	CHECK(node->numElems == NULL);
	CHECK(h->numElems);
	node->numElems = h->numElems;
	Nto1Node_LinkTo(&h->node, &node->node);
	(*h->numElems)++;
}

void cEntity_CollisionOff(cEntity *self) {
	uint32_t mask = self->mask;

	if (mask) {
		self->savedMask = mask;
		self->mask = 0;
	}
}

static void cEntity_CollisionOn(cEntity *self) {
	if (!self->mask) {
		self->mask = self->savedMask;
		self->savedMask = 0;
	}
}

void Nto1Node_LinkAs(Nto1Node *node, const Nto1Node *src) {
	if (src->target) {
		cEntity_AddLink(src->target, node);
		return;
	}
	CHECK(node->data);
	Nto1Node_Unlink(node);
}

byte cEntity_Del(cEntity *self) {
	if (self->deadNode.target) {
		return 0;
	}
	Nto1Node_LinkTo(&g_pWorld->deadList, &self->deadNode);
	return 1;
}

void cEntity_AimWeapon(cEntity *self, float ax, float ay, float *dist, vec3_t *out, cEntity **target,
	int flags) {
	vec3_t ang, dir, rot, end;

	*target = NULL;
	ang.x = ax;
	ang.y = ay;
	ang.z = 0.0f;
	dir.x = 0.0f;
	dir.y = 0.0f;
	dir.z = -*dist;
	vec3_t *r = Vec3_Rotate(&rot, &dir, &ang);
	end.x = self->pos.x + r->x;
	end.y = self->pos.y + r->y;
	end.z = self->pos.z + r->z;
	Engine_TestLine(&self->pos, &end, out, NULL);
	end = *out;
	cWorld_TestLineEntities(g_pWorld, self, flags, &self->pos, &end, out, target, 1);
	float dy = self->pos.y - out->y;
	float dx = self->pos.x - out->x;
	float dz = self->pos.z - out->z;
	*dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

byte cEntity_VarF(cEntity *self, const char *name, double *d, int set) {
	if (!crt_strncmp("AI_", name, 3) && self->ai && AI_Vars(self->ai)->VarF(self->ai, name + 3, d, set)) {
		return 1;
	}
	if (self->lLogic && !crt_strnicmp(name, "LLogic", 6) &&
		LLogic_Vars(self->lLogic)->VarF(self->lLogic, name + 6, d, set)) {
		return 1;
	}
	return 0;
}

void *cEntity_GetAI(cEntity *self) {
	return self->ai;
}

char *cEntity_GetListName(cEntity *self) {
	char **list = self->listNode.node.target;

	return list ? *list : NULL;
}

cEntity *cEntity_Next(cEntity *e) {
	return Node_Entity(e->worldNode.next);
}

vec3_t *cEntity_GetPos(cEntity *self, vec3_t *out) {
	*out = self->pos;
	return out;
}

void cEntity_GetTargetAngle(cEntity *self, float *ang, float dist, cEntity **target) {
	(void)self;
	(void)ang;
	(void)dist;
	*target = NULL;
}

static const char *cEntity_GetClassName(cEntity *self) {
	(void)self;
	return ENTITY_CLASSNAME;
}

void cEntity_UpdateOff(cEntity *self) {
	(void)self;
}

byte cEntity_VarI(cEntity *self, const char *name, int *i, int set) {
	if (!*name) {
		return 0;
	}
	if (!crt_strncmp("AI_", name, 3) && self->ai && AI_Vars(self->ai)->VarI(self->ai, name + 3, i, set)) {
		return 1;
	}
	if (self->lLogic && !crt_strnicmp(name, "LLogic", 6) &&
		LLogic_Vars(self->lLogic)->VarI(self->lLogic, name + 6, i, set)) {
		return 1;
	}
	if (!strcmp(name, "ActiveCMask")) {
		if (!(byte)set) {
			*i = self->mask != 0;
		} else if (*i) {
			cEntity_CollisionOn(self);
		} else {
			cEntity_CollisionOff(self);
		}
		return 1;
	}
	if (!strcmp(name, "Active")) {
		if ((byte)set) {
			Entity_SetActive(self, *i != 0);
		} else {
			*i = self->active != 0;
		}
		return 1;
	}
	if (!(byte)set) {
		if (!strcmp(name, "isLocalLogic")) {
			*i = self->lLogic != NULL;
			return 1;
		}
		if (!strcmp(name, "isAI")) {
			*i = Entity_GetAI(self) != NULL;
			return 1;
		}
	}
	return 0;
}

byte cEntity_IsType(cEntity *self, const char *type) {
	(void)self;
	return !strcmp(type, "Entity");
}

byte cEntity_SetList(cEntity *self, const char *list) {
	EntityDNode *node = &self->listNode;

	CHECK(node->node.data);
	Nto1Node_Unlink(&node->node);
	if (node->numElems) {
		(*node->numElems)--;
		node->numElems = NULL;
	}
	if (!list || !*list) {
		return 1;
	}
	cEntityListData *l = (cEntityListData *)cWorld_GetList(g_pWorld, list);
	if (!l) {
		return 0;
	}
	if (self->lLogic) {
		Obj_Delete(self->lLogic);
		self->lLogic = NULL;
	}
	/* the factory of the local logic: cdecl with the entity, the default returns NULL */
	self->lLogic = ((void *(*)(cEntity *))(void (*)(void))l->func)(self);
	EntityDList_Add(&l->ring, node);
	return 1;
}

void Nto1Node_LinkByName(Nto1Node *node, const char *name) {
	if (*name) {
		HashTable *t = &g_pWorld->entities;
		cPtrItem *it = PtrTable_Find(t, name, HashTable_Slot(t, name));
		if (it && it->value) {
			cEntity_AddLink(it->value, node);
			return;
		}
	}
	CHECK(node->data);
	Nto1Node_Unlink(node);
}

void cEntity_Render(cEntity *self, void *arg) {
	(void)self;
	(void)arg;
}

void cEntity_SetActive(cEntity *self, byte on) {
	self->active = on;
	if (!on) {
		Grid_Add(&g_pWorld->grid, self);
	}
}

void cEntity_SetEmbeddedFX(cEntity *self, const char *fxType) {
	(void)fxType;
	Con_Warnf("WARNING %s::SetFX() not implemented!\n", Entity_GetClassName(self));
}

void cEntity_SetModel(cEntity *self, const char *file, float sx, float sy, float sz, int flag) {
	(void)file;
	(void)sx;
	(void)sy;
	(void)sz;
	(void)flag;
	Con_Warnf("WARNING %s::SetModel() not implemented!\n", Entity_GetClassName(self));
}

void cEntity_SetPos(cEntity *self, const vec3_t *pos) {
	self->pos.x = pos->x;
	self->pos.y = pos->y;
	self->pos.z = pos->z;
}

void cEntity_Update(cEntity *self) {
	(void)self;
}

byte cEntity_VarS(cEntity *self, const char *name, char **s, int set) {
	if (!*name) {
		return 0;
	}
	if ((!crt_strncmp("AI_", name, 3) && self->ai && AI_Vars(self->ai)->VarS(self->ai, name + 3, s, set)) ||
		(self->lLogic && !crt_strnicmp(name, "LLogic", 6) &&
			LLogic_Vars(self->lLogic)->VarS(self->lLogic, name + 6, s, set))) {
		return 1;
	}
	if (!(byte)set) {
		const Nto1Node *n = NULL;
		if (!strcmp(name, "NextInSlot")) {
			n = self->worldNode.next;
		} else if (!strcmp(name, "NextInList")) {
			n = self->listNode.node.next;
		}
		if (n) {
			cEntity *e = Node_Entity(n);
			*s = e ? e->name : str_Empty;
			return 1;
		}
	}
	if (!strcmp(name, "EntityList")) {
		if ((byte)set) {
			return cEntity_SetList(self, *s);
		}
		char **list = self->listNode.node.target;
		char *ln = list ? *list : NULL;
		*s = ln ? ln : str_Empty;
		return 1;
	}
	if (!strcmp(name, "Name") && !(byte)set) {
		*s = self->name;
		return 1;
	}
	if (!strcmp(name, "Type") && !(byte)set) {
		*s = (char *)Entity_GetClassName(self);
		return 1;
	}
	return 0;
}

const char *Nto1Node_TargetName(const Nto1Node *node) {
	cEntity *e = node->target;

	return e ? e->name : str_Empty;
}

byte cEntity_VarV(cEntity *self, const char *name, double *x, double *y, double *z, int set) {
	vec3_t v;

	if (!*name) {
		return 0;
	}
	if (!crt_strncmp("AI_", name, 3) && self->ai && AI_Vars(self->ai)->VarV(self->ai, name + 3, x, y, z, set)) {
		return 1;
	}
	if (self->lLogic && !crt_strnicmp(name, "LLogic", 6) &&
		LLogic_Vars(self->lLogic)->VarV(self->lLogic, name + 6, x, y, z, set)) {
		return 1;
	}
	if (!strcmp(name, "Pos")) {
		if ((byte)set) {
			v.x = (float)*x;
			v.y = (float)*y;
			v.z = (float)*z;
			Entity_SetPos(self, &v);
		} else {
			Entity_GetPos(self, &v);
			*x = v.x;
			*y = v.y;
			*z = v.z;
		}
		return 1;
	}
	return 0;
}

/* the inline virtual functions of Entity.h; their code sits in the camera part of the exe (rva 0xbd430..) */
byte cEntity_TestLine(cEntity *self, const vec3_t *from, vec3_t *hit) {
	(void)self;
	(void)from;
	(void)hit;
	return 0;
}

void cEntity_Vf20(cEntity *self) {
	(void)self;
}

int cEntity_Vf21(cEntity *self, int a) {
	(void)self;
	(void)a;
	return 0;
}

/* the vtable of cEntity */
static const cEntityVtbl cEntity_vftable = {
	.Update = cEntity_Update,
	.Render = cEntity_Render,
	.UpdateOff = cEntity_UpdateOff,
	.Delete = cEntity_Delete,
	.GetClassName = cEntity_GetClassName,
	.IsType = cEntity_IsType,
	.GetPos = cEntity_GetPos,
	.SetPos = cEntity_SetPos,
	.SetActive = cEntity_SetActive,
	.TestLine = cEntity_TestLine,
	.TestSphere = cEntity_TestSphereFalse,
	.Touch = cEntity_TouchNop,
	.SetModel = cEntity_SetModel,
	.GetTargetAngle = cEntity_GetTargetAngle,
	.AimWeapon = cEntity_AimWeapon,
	.SetEmbeddedFX = cEntity_SetEmbeddedFX,
	.VarI = cEntity_VarI,
	.VarF = cEntity_VarF,
	.VarS = cEntity_VarS,
	.VarV = cEntity_VarV,
	.Vf20 = cEntity_Vf20,
	.Vf21 = cEntity_Vf21,
	.GetAI = cEntity_GetAI,
};
