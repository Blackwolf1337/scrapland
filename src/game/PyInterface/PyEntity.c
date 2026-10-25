/*
 * Methods of the Python type cPyEntity (method table cPyEntity_methods, rva 0x428e88). The original source file
 * is not known (no CHECK in these functions); the type lives in PyInterface/. Angles are degrees in the script
 * API and radians for the engine (deg * pi / 180 in double, then float).
 */
#include "scrap/pyentity.h"

static inline float Deg2RadF(double deg) {
	return (float)(deg * M_PI_D / DEG_PER_PI);
}

static inline double Rad2Deg(float rad) {
	return (double)rad * DEG_PER_PI / M_PI_D;
}

/* Del() : removes the entity; 1 if it went */
PyObject *cPyEntity_Del(PyObject *self, PyObject *args) {
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
PyObject *cPyEntity_SetModel(PyObject *self, PyObject *args) {
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
	e->vftable->SetModel(e, file, s, s, s, 1);
	return Py_BuildValue("i", 1);
}

/* SetFX(type) : embedded effect controller of the entity */
PyObject *cPyEntity_SetFX(PyObject *self, PyObject *args) {
	char *type;

	if (!PyArg_ParseTuple(args, "s", &type)) {
		return NULL;
	}
	cEntity *e = PyEntity_Get(self);
	if (!e) {
		return Py_BuildValue("i", 0);
	}
	e->vftable->SetEmbeddedFX(e, type);
	return Py_BuildValue("i", 1);
}

/*
 * GetTgAng(angx, angy, dist) : angles to the target and its name ("" if none). The original keeps the target in
 * the stack slot of self, so if the engine writes no target it reads a name from the Python object; same here.
 */
PyObject *cPyEntity_GetTgAng(PyObject *self, PyObject *args) {
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
	e->vftable->GetTargetAngle(e, ang, (float)dist, &target);
	return Py_BuildValue("dds", Rad2Deg(ang[0]), Rad2Deg(ang[1]), target ? target->name : "");
}

/* DrawWeap(angx, angy, dist) : aim of the weapon: ((x, y, z), dist, target name or "") */
PyObject *cPyEntity_DrawWeap(PyObject *self, PyObject *args) {
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
	e->vftable->AimWeapon(e, Deg2RadF(ax), Deg2RadF(ay), &dist, &aim, &target, 8);
	return Py_BuildValue("(ddd)ds", (double)aim.x, (double)aim.y, (double)aim.z, (double)dist,
		target ? target->name : "");
}

/* IsType(type) : 1 if the entity is of that type; None without entity */
PyObject *cPyEntity_IsType(PyObject *self, PyObject *args) {
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
