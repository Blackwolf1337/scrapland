/*
 * Methods of the Python type cPyEntity (method table cPyEntity_methods). The original source file
 * is not known (no CHECK in these functions); the type lives in PyInterface/. Angles are degrees in the script
 * API and radians for the engine (deg * pi / 180 in double, then float).
 */
#include "scrap/console.h"
#include "scrap/pyentity.h"

static inline float Deg2RadF(double deg) {
	return (float)(deg * M_PI_D / DEG_PER_PI);
}

/* Del() : removes the entity; 1 if it went */
static PyObject *cPyEntity_Del(PyObject *self, PyObject *args) {
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	cEntity *e = PyEntity_Get(self);
	if (e && cEntity_Del(e)) {
		return Py_BuildValue("i", 1);
	}
	return Py_BuildValue("i", 0);
}

/* SetModel(file[,scale]) : model of the entity, uniform scale (default 1) */
static PyObject *cPyEntity_SetModel(PyObject *self, PyObject *args) {
	char *file;
	double scale = 1.0;

	if (!PyArg_ParseTuple(args, "s|d", &file, &scale)) {
		return NULL;
	}
	cEntity *e = PyEntity_Get(self);
	if (!e) {
		return Py_BuildValue("i", 0);
	}
	float s = (float)scale;
	Entity_SetModel(e, file, s, s, s, 1);
	return Py_BuildValue("i", 1);
}

/* SetFX(type) : embedded effect controller of the entity */
static PyObject *cPyEntity_SetFX(PyObject *self, PyObject *args) {
	char *type;

	if (!PyArg_ParseTuple(args, "s", &type)) {
		return NULL;
	}
	cEntity *e = PyEntity_Get(self);
	if (!e) {
		return Py_BuildValue("i", 0);
	}
	Entity_SetEmbeddedFX(e, type);
	return Py_BuildValue("i", 1);
}

/*
 * GetTgAng(angx, angy, dist) : angles to the target and its name ("" if none). The original keeps the target in
 * the stack slot of self, so if the engine writes no target it reads a name from the Python object; same here.
 */
static PyObject *cPyEntity_GetTgAng(PyObject *self, PyObject *args) {
	double ax, ay, dist;

	if (!PyArg_ParseTuple(args, "ddd", &ax, &ay, &dist)) {
		return NULL;
	}
	cEntity *e = PyEntity_Get(self);
	if (!e) {
		return Py_BuildValue("i", 0);
	}
	float ang[3] = { Deg2RadF(ax), Deg2RadF(ay), 0.0f };
	cEntity *target = (cEntity *)self;
	Entity_GetTargetAngle(e, ang, (float)dist, &target);
	return Py_BuildValue("dds", Rad2Deg(ang[0]), Rad2Deg(ang[1]), target ? target->name : "");
}

/* DrawWeap(angx, angy, dist) : aim of the weapon: ((x, y, z), dist, target name or "") */
static PyObject *cPyEntity_DrawWeap(PyObject *self, PyObject *args) {
	double ax, ay, d;

	if (!PyArg_ParseTuple(args, "ddd", &ax, &ay, &d)) {
		return NULL;
	}
	cEntity *e = PyEntity_Get(self);
	if (!e) {
		return Py_BuildValue("i", 0);
	}
	float dist = (float)d;
	vec3_t aim;
	cEntity *target = NULL; /* uninitialized in the original */
	Entity_AimWeapon(e, Deg2RadF(ax), Deg2RadF(ay), &dist, &aim, &target, 8);
	return Py_BuildValue("(ddd)ds", (double)aim.x, (double)aim.y, (double)aim.z, (double)dist,
		target ? target->name : "");
}

/* IsType(type) : 1 if the entity is of that type; None without entity */
static PyObject *cPyEntity_IsType(PyObject *self, PyObject *args) {
	char *type = NULL;

	if (!PyArg_ParseTuple(args, "s", &type)) {
		return NULL;
	}
	cEntity *e = PyEntity_Get(self);
	if (!e) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return Py_BuildValue("i", Entity_IsType(e, type) ? 1 : 0);
}

/* type functions of cPyEntity (type object) */

static PyTypeObject cPyEntity_Type;
PyObject *PyEntity_New(const char *name) {
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return NULL;
	}
	cPyEntity *self = (cPyEntity *)_PyObject_New(&cPyEntity_Type);
	if (!self) {
		return NULL;
	}
	cEntityRef *ref = operator_new(sizeof(*ref));
	Nto1Node_Init(&ref->link);
	self->ref = ref;
	ref->link.data = ref;
	cEntity_AddLink(e, &ref->link);
	Str_Copy(ref->name, name, sizeof(ref->name));
	return (PyObject *)self;
}

/* tp_dealloc */
static void PyEntity_Dealloc(PyObject *self) {
	cEntityRef *ref = ((cPyEntity *)self)->ref;
	if (ref) {
		Nto1Node_Done(&ref->link);
		operator_delete(ref);
	}
	PyPool_Free(self);
}

/* tp_print: the text goes to fprintf as the format, like in the original */
static int PyEntity_Print(PyObject *self, FILE *fp, int flags) {
	static char buf[0x100];
	const char *name = ((cPyEntity *)self)->ref->name;

	(void)flags;
	if (World_FindEntity(name)) {
		Str_Printf(buf, sizeof(buf), "Entity: %s", name);
	} else {
		Str_Printf(buf, sizeof(buf), " : The Entity called '%s' is invalid.", name);
	}
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

/* tp_getattr: script variable of the entity (int, float, vector, string), else a method */
static PyObject *PyEntity_GetAttr(PyObject *self, char *name) {
	cEntityRef *ref = ((cPyEntity *)self)->ref;
	cEntity *e = ref->link.target;
	int i;
	double d, x, y, z;
	char *s;

	if (!e) {
		Con_Errorf("'%s'->%s : Entity invalid.\n", ref->name, name);
		return NULL;
	}
	if (Entity_VarI(e, name, &i, 0)) {
		return Py_BuildValue("i", i);
	}
	if (Entity_VarF(e, name, &d, 0)) {
		return Py_BuildValue("d", d);
	}
	if (Entity_VarV(e, name, &x, &y, &z, 0)) {
		return Py_BuildValue("ddd", x, y, z);
	}
	if (Entity_VarS(e, name, &s, 0)) {
		if (s) {
			return Py_BuildValue("s", s);
		}
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *method = PyEntity_FindMethod(self, name);
	if (!method) {
		Con_Errorf("'%s'->%s : Member invalid.\n", ref->name, name);
	}
	return method;
}

/*
 * a script variable of an entity from a Python value, as the attribute set of its handle: the value is tried as int,
 * float, vector and string, each type that parses goes to the matching variable until one takes it. ref names the
 * entity in the messages. -1 with the message printed when no variable took it.
 */
int PyEntity_SetVar(cEntity *e, const char *ref, const char *name, PyObject *v) {
	int parsed = 0;
	int i;
	double d, x, y, z;
	char *s;

	if (!e) {
		Con_Errorf("'%s'->%s : Entity invalid.\n", ref, name);
		return -1;
	}
	if (PyArg_Parse(v, "i", &i)) {
		parsed = 1;
		if (Entity_VarI(e, name, &i, 1)) {
			return 0;
		}
	} else {
		PyErr_Clear();
	}
	if (PyArg_Parse(v, "d", &d)) {
		parsed = 1;
		if (Entity_VarF(e, name, &d, 1)) {
			return 0;
		}
	} else {
		PyErr_Clear();
	}
	if (PyArg_ParseTuple(v, "ddd", &x, &y, &z)) {
		parsed = 1;
		if (Entity_VarV(e, name, &x, &y, &z, 1)) {
			return 0;
		}
	} else {
		PyErr_Clear();
	}
	if (PyArg_Parse(v, "s", &s)) {
		if (Entity_VarS(e, name, &s, 1)) {
			return 0;
		}
	} else {
		PyErr_Clear();
		if (!parsed) {
			Con_Errorf("'%s'->%s : Invalid type.\n", ref, name);
			return -1;
		}
	}
	Con_Errorf("'%s'->%s : Member invalid.\n", ref, name);
	return -1;
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
