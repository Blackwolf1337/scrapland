/*
 * The script handles of entities (cEntityRef) and their Python type cPyEntity (method table cPyEntity_methods). The
 * original source file is not known (no CHECK in these functions); the type lives in PyInterface/. Angles are degrees
 * in the script API and radians for the engine (deg * pi / 180 in double, then float). The methods and the attributes
 * work on native values (the handle as NV_ENT, stage 7 batch 7c of docs/objlib-plan.md); the type functions are the
 * Python face on them.
 */
#include "scrap/console.h"
#include "scrap/pyentity.h"
#include "scrap/scriptapi.h"

static inline float Deg2RadF(double deg) {
	return (float)(deg * M_PI_D / DEG_PER_PI);
}

/* a method of the handles: cPyEntity_<name>_E, its entry on native values (the handle is a[0], the arguments of the call
 * follow), whose body follows the macro, and cPyEntity_<name>, the method of the type on it */
#define ENT_FN(name) \
	static byte cPyEntity_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out); \
	static PyObject *cPyEntity_##name(PyObject *self, PyObject *args) { \
		return Mode_SelfEntry(self, args, cPyEntity_##name##_E); \
	} \
	static byte cPyEntity_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out)

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
	ref->face = NULL;
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

/* type functions of cPyEntity (type object) */

static PyTypeObject cPyEntity_Type;
PyObject *PyEntity_Face(cEntityRef *ref) {
	if (ref->face) {
		Py_INCREF(ref->face);
		return (PyObject *)ref->face;
	}
	cPyEntity *self = (cPyEntity *)_PyObject_New(&cPyEntity_Type);
	if (!self) {
		return NULL;
	}
	self->ref = EntRef_Keep(ref);
	ref->face = self;
	return (PyObject *)self;
}

byte PyEntity_Check(PyObject *o) {
	return o->ob_type == &cPyEntity_Type;
}

/* tp_dealloc */
static void PyEntity_Dealloc(PyObject *self) {
	cEntityRef *ref = ((cPyEntity *)self)->ref;
	if (ref) {
		ref->face = NULL;
		EntRef_Drop(ref);
	}
	ScriptPool_Free(self);
}

/* tp_print: the text goes to fprintf as the format, like in the original */
static int PyEntity_Print(PyObject *self, FILE *fp, int flags) {
	static char buf[0x100];

	(void)flags;
	Native_EntPrint(((cPyEntity *)self)->ref->name, buf, sizeof(buf));
	crt_fprintf(fp, buf);
	return 0;
}

static const char str_PyMethod_cPyEntity_Del[];
static const char str_PyMethod_cPyEntity_DrawWeap[];
static const char str_PyMethod_cPyEntity_GetTgAng[];
static const char str_PyMethod_cPyEntity_IsType[];
static const char str_PyMethod_cPyEntity_SetFX[];
static const char str_PyMethod_cPyEntity_SetModel[];
static PyMethodDef cPyEntity_methods[7] = {
	{ .ml_name = (void *)str_PyMethod_cPyEntity_Del, .ml_meth = (void *)cPyEntity_Del, .ml_flags = 1 },
	{
		.ml_name = (void *)str_PyMethod_cPyEntity_SetModel,
		.ml_meth = (void *)cPyEntity_SetModel,
		.ml_flags = 1,
	},
	{ .ml_name = (void *)str_PyMethod_cPyEntity_SetFX, .ml_meth = (void *)cPyEntity_SetFX, .ml_flags = 1 },
	{
		.ml_name = (void *)str_PyMethod_cPyEntity_GetTgAng,
		.ml_meth = (void *)cPyEntity_GetTgAng,
		.ml_flags = 1,
	},
	{
		.ml_name = (void *)str_PyMethod_cPyEntity_DrawWeap,
		.ml_meth = (void *)cPyEntity_DrawWeap,
		.ml_flags = 1,
	},
	{ .ml_name = (void *)str_PyMethod_cPyEntity_IsType, .ml_meth = (void *)cPyEntity_IsType, .ml_flags = 1 },
};

/* a method of cPyEntity by name */
static PyObject *PyEntity_FindMethod(PyObject *self, char *name) {
	return Py_FindMethod(cPyEntity_methods, self, name);
}

/* tp_getattr: script variable of the entity (int, float, vector, string: Native_EntRead), else a method */
static PyObject *PyEntity_GetAttr(PyObject *self, char *name) {
	cEntityRef *ref = ((cPyEntity *)self)->ref;
	cEntity *e = ref->link.target;
	nativeVal_t v;

	if (!e) {
		Con_Errorf("'%s'->%s : Entity invalid.\n", ref->name, name);
		return NULL;
	}
	if (Native_EntRead(e, name, &v)) {
		return Mode_Return(1, &v);
	}
	PyObject *method = PyEntity_FindMethod(self, name);
	if (!method) {
		Con_Errorf("'%s'->%s : Member invalid.\n", ref->name, name);
	}
	return method;
}

/*
 * a script variable of an entity from a Python value, as the attribute set of its handle (Native_EntStore on the value
 * as Native_Arg makes it). ref names the entity in the messages. -1 with the message printed when no variable took it.
 * v NULL is `del handle.name`: the original parses the NULL value (a crash when the entity lives); here it is a value
 * of none of the forms ("Invalid type").
 */
static int PyEntity_SetVar(cEntity *e, const char *ref, const char *name, PyObject *v) {
	nativeVal_t x;
	if (v) {
		Native_Arg(v, &x);
	} else {
		Mode_KwSkip(&x);
	}
	byte ok = Native_EntStore(e, ref, name, &x);
	Native_Free(&x);
	return ok ? 0 : -1;
}

static int PyEntity_SetAttr(PyObject *self, char *name, PyObject *v) {
	cEntityRef *ref = ((cPyEntity *)self)->ref;
	return PyEntity_SetVar(ref->link.target, ref->name, name, v);
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

static const char str_CPyEntity[] = "cPyEntity";
static const char str_PyMethod_cPyEntity_Del[] = "Del";
static const char str_PyMethod_cPyEntity_SetModel[] = "SetModel";
static const char str_PyMethod_cPyEntity_SetFX[] = "SetFX";
static const char str_PyMethod_cPyEntity_GetTgAng[] = "GetTgAng";
static const char str_PyMethod_cPyEntity_DrawWeap[] = "DrawWeap";
static const char str_PyMethod_cPyEntity_IsType[] = "IsType";
extern const char str_CPyEntity[];
static PyTypeObject cPyEntity_Type = {
	.ob_refcnt = 1,
	.ob_type = &PyType_Type,
	.tp_name = (void *)str_CPyEntity,
	.tp_basicsize = sizeof(cPyEntity),
	.tp_dealloc = (void *)PyEntity_Dealloc,
	.tp_print = (void *)PyEntity_Print,
	.tp_getattr = (void *)PyEntity_GetAttr,
	.tp_setattr = (void *)PyEntity_SetAttr,
};
