/*
 * Python module SVec (method table SVec_methods): 3D vector math for the game scripts. Original source
 * PyInterface/SVecPy.cpp.
 *
 * Vectors come in as tuples. Functions parsing "d" compute in double, functions parsing "f" in float (SSE single
 * precision, like the MSVC build). Angles in the script API are degrees; the engine helpers take radians and
 * the conversion is deg * pi / 180 and rad * 180 / pi in double, in that order.
 */
#include <math.h>

#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/python.h"

static inline double Deg2Rad(double deg) {
	return deg * M_PI_D / DEG_PER_PI;
}

/* v / |v| in float; a zero vector stays as it is (the inline Normalize of the engine's vector class) */
static vec3_t Vec3_Normalized(vec3_t v) {
	float len = (float)sqrt((double)(v.x * v.x + v.y * v.y + v.z * v.z));
	if (len == 0.0f) {
		return v;
	}
	float inv = 1.0f / len;
	return (vec3_t){ inv * v.x, inv * v.y, inv * v.z };
}

/* res Add(v1,v2) : suma dos vectores 3D */
static PyObject *SVec_Add(PyObject *self, PyObject *args) {
	double x1, y1, z1, x2, y2, z2;

	(void)self;
	if (!PyArg_ParseTuple(args, "(ddd)(ddd)", &x1, &y1, &z1, &x2, &y2, &z2)) {
		return NULL;
	}
	return Py_BuildValue("ddd", x1 + x2, y1 + y2, z1 + z2);
}

/* res Sub(v1,v2) : Resta dos vectores 3D */
static PyObject *SVec_Sub(PyObject *self, PyObject *args) {
	double x1, y1, z1, x2, y2, z2;

	(void)self;
	if (!PyArg_ParseTuple(args, "(ddd)(ddd)", &x1, &y1, &z1, &x2, &y2, &z2)) {
		return NULL;
	}
	return Py_BuildValue("ddd", x1 - x2, y1 - y2, z1 - z2);
}

/* res Mod(v1) : Devuelve el modulo de un vector */
static PyObject *SVec_Mod(PyObject *self, PyObject *args) {
	double x, y, z;

	(void)self;
	if (!PyArg_ParseTuple(args, "(ddd)", &x, &y, &z)) {
		return NULL;
	}
	return Py_BuildValue("d", sqrt(x * x + y * y + z * z));
}

/* res ModSqr(v1) : Devuelve el modulo (al cuadrado) de un vector */
static PyObject *SVec_ModSqr(PyObject *self, PyObject *args) {
	double x, y, z;

	(void)self;
	if (!PyArg_ParseTuple(args, "(ddd)", &x, &y, &z)) {
		return NULL;
	}
	return Py_BuildValue("d", x * x + y * y + z * z);
}

/* res Norm(v1) : Normaliza un vector. Below a length of 1e-6 the result is (0, 1, 0). */
static PyObject *SVec_Norm(PyObject *self, PyObject *args) {
	double x, y, z;

	(void)self;
	if (!PyArg_ParseTuple(args, "(ddd)", &x, &y, &z)) {
		return NULL;
	}
	double len = sqrt(x * x + y * y + z * z);
	if (len < (double)1e-6f) { /* the constant is the float 1e-6 widened to double */
		return Py_BuildValue("ddd", 0.0, 1.0, 0.0);
	}
	return Py_BuildValue("ddd", x / len, y / len, z / len);
}

/* res Prod(v1,f) : multiplica un vector por un numero */
static PyObject *SVec_Prod(PyObject *self, PyObject *args) {
	double x, y, z, f;

	(void)self;
	if (!PyArg_ParseTuple(args, "(ddd)d", &x, &y, &z, &f)) {
		return NULL;
	}
	return Py_BuildValue("ddd", x * f, y * f, z * f);
}

/* res DProd(v1,v2) : Calcula el producto escalar de dos vectores */
static PyObject *SVec_DProd(PyObject *self, PyObject *args) {
	vec3_t a, b;

	(void)self;
	if (!PyArg_ParseTuple(args, "(fff)(fff)", &a.x, &a.y, &a.z, &b.x, &b.y, &b.z)) {
		return NULL;
	}
	return Py_BuildValue("f", (double)(a.x * b.x + a.y * b.y + a.z * b.z));
}

/* vRes CProd(v1,v2) : Calcula el producto vectorial de dos vectores */
static PyObject *SVec_CProd(PyObject *self, PyObject *args) {
	vec3_t v1, v2;
	PyObject *tuple;

	(void)self;
	if (!PyArg_ParseTuple(args, "(fff)(fff)", &v1.x, &v1.y, &v1.z, &v2.x, &v2.y, &v2.z)) {
		return NULL;
	}
	CHECK(tuple = PyTuple_New(3));
	vec3_t v3 = {
		v1.y * v2.z - v1.z * v2.y,
		v1.z * v2.x - v1.x * v2.z,
		v1.x * v2.y - v1.y * v2.x,
	};
	CHECK(0 == PyTuple_SetItem(tuple, 0, PyFloat_FromDouble((double)v3.x)));
	CHECK(0 == PyTuple_SetItem(tuple, 1, PyFloat_FromDouble((double)v3.y)));
	CHECK(0 == PyTuple_SetItem(tuple, 2, PyFloat_FromDouble((double)v3.z)));
	return tuple;
}

/* res Rotate3D(src,rot) : Rota un vector. rot in degrees. */
static PyObject *SVec_Rotate3D(PyObject *self, PyObject *args) {
	double sx, sy, sz, rx, ry, rz;

	(void)self;
	if (!PyArg_ParseTuple(args, "(ddd)(ddd)", &sx, &sy, &sz, &rx, &ry, &rz)) {
		return NULL;
	}
	vec3_t rot = { (float)Deg2Rad(rx), (float)Deg2Rad(ry), (float)Deg2Rad(rz) };
	vec3_t src = { (float)sx, (float)sy, (float)sz };
	vec3_t res;
	Vec3_Rotate(&res, &src, &rot);
	return Py_BuildValue("ddd", (double)res.x, (double)res.y, (double)res.z);
}

/* Ang GetAngle(vec) : Obtiene el ángulo entre dos vectores, in degrees */
static PyObject *SVec_GetAngle(PyObject *self, PyObject *args) {
	vec3_t a, b;

	(void)self;
	if (!PyArg_ParseTuple(args, "(fff)(fff)", &a.x, &a.y, &a.z, &b.x, &b.y, &b.z)) {
		return NULL;
	}
	a = Vec3_Normalized(a);
	b = Vec3_Normalized(b);
	float c = a.x * b.x + a.y * b.y + a.z * b.z;
	/* clamp to [-1, 1] as compiled: comiss, then maxss, so NaN goes through */
	if (c > 1.0f) {
		c = 1.0f;
	}
	else if (-1.0f > c) {
		c = -1.0f;
	}
	return Py_BuildValue("d", Rad2Deg(acos((double)c)));
}

/* AngX,AngY GetRotAng(vec) : Obtiene la rotacion de un vector. Takes x, y, z or a tuple; degrees. */
static PyObject *SVec_GetRotAng(PyObject *self, PyObject *args) {
	double x, y, z;

	(void)self;
	if (!PyArg_ParseTuple(args, "ddd", &x, &y, &z)) {
		PyErr_Clear();
		if (!PyArg_ParseTuple(args, "(ddd)", &x, &y, &z)) {
			return NULL;
		}
	}
	vec3_t dir = { (float)x, (float)y, (float)z };
	vec3_t ang;
	Vec3_GetRotAng(&ang, &dir);
	return Py_BuildValue("dd", Rad2Deg((double)ang.x), Rad2Deg((double)ang.y));
}

/* rAng NormAng(Ang) : Normaliza un angulo, in degrees */
static PyObject *SVec_NormAng(PyObject *self, PyObject *args) {
	double ang;

	(void)self;
	if (!PyArg_ParseTuple(args, "d", &ang)) {
		return NULL;
	}
	return Py_BuildValue("d", Rad2Deg((double)NormAng((float)Deg2Rad(ang))));
}

static char str_PyDoc_SVec_Add[];
static char str_PyDoc_SVec_CProd[];
static char str_PyDoc_SVec_DProd[];
static char str_PyDoc_SVec_GetAngle[];
static char str_PyDoc_SVec_GetRotAng[];
static char str_PyDoc_SVec_Mod[];
static char str_PyDoc_SVec_ModSqr[];
static char str_PyDoc_SVec_Norm[];
static char str_PyDoc_SVec_NormAng[44];
static char str_PyDoc_SVec_Prod[];
static char str_PyDoc_SVec_Rotate3D[];
static char str_PyDoc_SVec_Sub[];
extern const char str_PyMethod_SScorer_Add[];
static const char str_PyMethod_SVec_CProd[];
static const char str_PyMethod_SVec_DProd[];
static const char str_PyMethod_SVec_GetAngle[];
static const char str_PyMethod_SVec_GetRotAng[];
static const char str_PyMethod_SVec_Mod[];
static const char str_PyMethod_SVec_ModSqr[];
static const char str_PyMethod_SVec_Norm[];
static const char str_PyMethod_SVec_NormAng[];
static const char str_PyMethod_SVec_Prod[];
static const char str_PyMethod_SVec_Rotate3D[];
static const char str_PyMethod_SVec_Sub[];
static PyMethodDef SVec_methods[13] = {
	{
		.ml_name = (void *)str_PyMethod_SScorer_Add,
		.ml_meth = (void *)SVec_Add,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_Add,
	},
	{
		.ml_name = (void *)str_PyMethod_SVec_Sub,
		.ml_meth = (void *)SVec_Sub,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_Sub,
	},
	{
		.ml_name = (void *)str_PyMethod_SVec_Mod,
		.ml_meth = (void *)SVec_Mod,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_Mod,
	},
	{
		.ml_name = (void *)str_PyMethod_SVec_ModSqr,
		.ml_meth = (void *)SVec_ModSqr,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_ModSqr,
	},
	{
		.ml_name = (void *)str_PyMethod_SVec_Norm,
		.ml_meth = (void *)SVec_Norm,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_Norm,
	},
	{
		.ml_name = (void *)str_PyMethod_SVec_Prod,
		.ml_meth = (void *)SVec_Prod,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_Prod,
	},
	{
		.ml_name = (void *)str_PyMethod_SVec_DProd,
		.ml_meth = (void *)SVec_DProd,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_DProd,
	},
	{
		.ml_name = (void *)str_PyMethod_SVec_CProd,
		.ml_meth = (void *)SVec_CProd,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_CProd,
	},
	{
		.ml_name = (void *)str_PyMethod_SVec_Rotate3D,
		.ml_meth = (void *)SVec_Rotate3D,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_Rotate3D,
	},
	{
		.ml_name = (void *)str_PyMethod_SVec_GetAngle,
		.ml_meth = (void *)SVec_GetAngle,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_GetAngle,
	},
	{
		.ml_name = (void *)str_PyMethod_SVec_GetRotAng,
		.ml_meth = (void *)SVec_GetRotAng,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_GetRotAng,
	},
	{
		.ml_name = (void *)str_PyMethod_SVec_NormAng,
		.ml_meth = (void *)SVec_NormAng,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SVec_NormAng,
	},
};

SD_PY_MODULE(SVec, 0x42fc80)

static const char str_PyMethod_SVec_Sub[] = "Sub";
static const char str_PyMethod_SVec_Mod[] = "Mod";
static const char str_PyMethod_SVec_ModSqr[] = "ModSqr";
static const char str_PyMethod_SVec_Norm[] = "Norm";
static const char str_PyMethod_SVec_Prod[] = "Prod";
static const char str_PyMethod_SVec_DProd[] = "DProd";
static const char str_PyMethod_SVec_CProd[] = "CProd";
static const char str_PyMethod_SVec_Rotate3D[] = "Rotate3D";
static const char str_PyMethod_SVec_GetAngle[] = "GetAngle";
static const char str_PyMethod_SVec_GetRotAng[] = "GetRotAng";
static const char str_PyMethod_SVec_NormAng[] = "NormAng";

static char str_PyDoc_SVec_Add[] = "res Add(v1,v2) : suma dos vectores 3D";

static char str_PyDoc_SVec_Sub[] = "res Sub(v1,v2) : Resta dos vectores 3D";

static char str_PyDoc_SVec_Mod[] = "res Mod(v1) : Devuelve el modulo de un vector";

static char str_PyDoc_SVec_ModSqr[] = "res ModSqr(v1) : Devuelve el modulo (al cuadrado) de un vector";

static char str_PyDoc_SVec_Norm[] = "res Norm(v1) : Normaliza un vector";

static char str_PyDoc_SVec_Prod[] = "res Prod(v1,f) : multiplica un vector por un numero";

static char str_PyDoc_SVec_DProd[] = "res DProd(v1,v2) : Calcula el producto escalar de dos vectores";

static char str_PyDoc_SVec_CProd[] = "vRes CProd(v1,v2) : Calcula el producto vectorial de dos vectores";

static char str_PyDoc_SVec_Rotate3D[] = "res Rotate3D(src,rot) : Rota un vector";

static char str_PyDoc_SVec_GetAngle[] = "Ang GetAngle(vec) : Obtiene el ángulo entre dos vectores";

static char str_PyDoc_SVec_GetRotAng[] = "AngX,AngY GetRotAng(vec) : Obtiene la rotacion de un vector";

static char str_PyDoc_SVec_NormAng[44] = "rAng NormAng(Ang) : Normaliza un angulo";

