/*
 * The script handles of entities (cEntityRef), their attributes and methods (the type cPyEntity of the original, method
 * table cPyEntity_methods). The original source file is not known (no CHECK in these functions); the type lives in
 * PyInterface/. Angles are degrees in the script API and radians for the engine (deg * pi / 180 in double, then
 * float). The methods and the attributes work on native values (the handle as NV_ENT, stage 7 batch 7c of
 * docs/objlib-plan.md; Native_EntGetAttr).
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/nativeobj.h"
#include "scrap/pyentity.h"
#include "scrap/scriptapi.h"

static inline float Deg2RadF(double deg) {
	return (float)(deg * M_PI_D / DEG_PER_PI);
}

/* a method of the handles: cPyEntity_<name>_E, its entry on native values (the handle is a[0], the arguments of the
 * call follow), whose body follows the macro */
#define ENT_FN(name) static byte cPyEntity_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out)

/* the entity of the handle a method is called on, NULL when it is gone */
static cEntity *Ent_Self(const nativeVal_t *a) {
	return a[0].ent->link.target;
}

/* Del() : removes the entity; 1 if it went */
ENT_FN(Del) {
	if (!Mode_Parse(a + 1, n - 1, "")) {
		return 0;
	}
	cEntity *e = Ent_Self(a);
	return Api_Build(out, "i", e && cEntity_Del(e) ? 1 : 0);
}

/* SetModel(file[,scale]) : model of the entity, uniform scale (default 1) */
ENT_FN(SetModel) {
	const char *file;
	double scale = 1.0;

	if (!Mode_Parse(a + 1, n - 1, "s|d", &file, &scale)) {
		return 0;
	}
	cEntity *e = Ent_Self(a);
	if (!e) {
		return Api_Build(out, "i", 0);
	}
	float s = (float)scale;
	Entity_SetModel(e, file, s, s, s, 1);
	return Api_Build(out, "i", 1);
}

/* SetFX(type) : embedded effect controller of the entity */
ENT_FN(SetFX) {
	const char *type;

	if (!Mode_Parse(a + 1, n - 1, "s", &type)) {
		return 0;
	}
	cEntity *e = Ent_Self(a);
	if (!e) {
		return Api_Build(out, "i", 0);
	}
	Entity_SetEmbeddedFX(e, type);
	return Api_Build(out, "i", 1);
}

/*
 * GetTgAng(angx, angy, dist) : angles to the target and its name ("" if none). The original keeps the target in
 * the stack slot of self and would read a name from the Python object if the engine wrote no target; every
 * GetTargetAngle of the engine writes it (NULL for none), so it starts as none here.
 */
ENT_FN(GetTgAng) {
	double ax, ay, dist;

	if (!Mode_Parse(a + 1, n - 1, "ddd", &ax, &ay, &dist)) {
		return 0;
	}
	cEntity *e = Ent_Self(a);
	if (!e) {
		return Api_Build(out, "i", 0);
	}
	float ang[3] = { Deg2RadF(ax), Deg2RadF(ay), 0.0f };
	cEntity *target = NULL;
	Entity_GetTargetAngle(e, ang, (float)dist, &target);
	return Api_Build(out, "dds", Rad2Deg(ang[0]), Rad2Deg(ang[1]), target ? target->name : "");
}

/* DrawWeap(angx, angy, dist) : aim of the weapon: ((x, y, z), dist, target name or "") */
ENT_FN(DrawWeap) {
	double ax, ay, d;

	if (!Mode_Parse(a + 1, n - 1, "ddd", &ax, &ay, &d)) {
		return 0;
	}
	cEntity *e = Ent_Self(a);
	if (!e) {
		return Api_Build(out, "i", 0);
	}
	float dist = (float)d;
	vec3_t aim;
	cEntity *target = NULL; /* uninitialized in the original */
	Entity_AimWeapon(e, Deg2RadF(ax), Deg2RadF(ay), &dist, &aim, &target, 8);
	return Api_Build(out, "(ddd)ds", (double)aim.x, (double)aim.y, (double)aim.z, (double)dist,
		target ? target->name : "");
}

/* IsType(type) : 1 if the entity is of that type; None without entity */
ENT_FN(IsType) {
	const char *type = NULL;

	if (!Mode_Parse(a + 1, n - 1, "s", &type)) {
		return 0;
	}
	cEntity *e = Ent_Self(a);
	if (!e) {
		return 1;
	}
	return Api_Build(out, "i", Entity_IsType(e, type) ? 1 : 0);
}

/* the methods as a handle gives them (the method table of cPyEntity, in its order) */
static const portFn_t ent_methods[] = {
	PORT_FN(Del, cPyEntity_Del),
	PORT_FN(SetModel, cPyEntity_SetModel),
	PORT_FN(SetFX, cPyEntity_SetFX),
	PORT_FN(GetTgAng, cPyEntity_GetTgAng),
	PORT_FN(DrawWeap, cPyEntity_DrawWeap),
	PORT_FN(IsType, cPyEntity_IsType),
	PORT_FNEND
};

static int Ent_CompareNames(const void *a, const void *b) {
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

byte Native_EntGetAttr(const nativeVal_t *h, const char *name, nativeVal_t *out) {
	cEntity *e = Native_Ent(h);
	nativeVal_t v;

	Native_SetNone(out);
	if (!e) {
		/* the getattr of the type returns NULL without an error */
		Con_Errorf("'%s'->%s : Entity invalid.\n", Native_EntName(h), name);
		return 0;
	}
	if (Native_EntRead(e, name, &v)) {
		/* the variable as the handle gave it to the script: a vector as the tuple of its floats */
		NObj_CallArg(&v, out);
		Native_Free(&v);
		return 1;
	}
	/* the method lookup of the type: __methods__ (the names sorted), then the methods by name; the type has no
	 * __doc__ */
	if (!strcmp(name, "__methods__")) {
		const char *names[sizeof(ent_methods) / sizeof(ent_methods[0])];
		int n = 0;
		for (; ent_methods[n].name; n++) {
			names[n] = ent_methods[n].name;
		}
		qsort((void *)names, (size_t)n, sizeof(names[0]), Ent_CompareNames);
		Native_SetEmpty(out, NV_LIST);
		for (int i = 0; i < n; i++) {
			Native_SetStr(Native_Append(out), names[i]);
		}
		NObj_Share(out);
		return 1;
	}
	for (const portFn_t *m = ent_methods; m->name; m++) {
		if (!strcmp(m->name, name)) {
			NObj_NewBound(out, m->name, m->entry, h);
			return 1;
		}
	}
	Native_Raise(NE_ATTRIBUTE, "%s", name);
	Con_Errorf("'%s'->%s : Member invalid.\n", Native_EntName(h), name);
	return 0;
}

/* the handles (cEntityRef) */

cEntityRef *EntRef_New(const char *name) {
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return NULL;
	}
	cEntityRef *ref = operator_new(sizeof(*ref));
	Nto1Node_Init(&ref->link);
	ref->link.data = ref;
	cEntity_AddLink(e, &ref->link);
	Str_Copy(ref->name, name, sizeof(ref->name));
	ref->holders = 1;
	return ref;
}

cEntityRef *EntRef_Keep(cEntityRef *ref) {
	ref->holders++;
	return ref;
}

void EntRef_Drop(cEntityRef *ref) {
	if (--ref->holders == 0) {
		Nto1Node_Done(&ref->link);
		operator_delete(ref);
	}
}

/* Global/Nto1Links.h instances */

void Nto1Head_UnlinkAll(Nto1Node *head) {
	CHECK(head->data == NULL);
	for (Nto1Node *node = head->next; node != head; node = head->next) {
		CHECK(node->data);
		Nto1Node_Unlink(node);
	}
}

void cEntity_AddLink(cEntity *self, Nto1Node *node) {
	CHECK(node->data);
	Nto1Node_Unlink(node);
	node->target = self->links.target;
	node->next = self->links.next;
	node->prev = &self->links;
	self->links.next->prev = node;
	self->links.next = node;
}
