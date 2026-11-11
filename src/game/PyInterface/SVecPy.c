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
#include "scrap/scriptapi.h"

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
API_FN(SVec, Add) {
	double x1, y1, z1, x2, y2, z2;
	return Mode_Parse(a, n, "(ddd)(ddd)", &x1, &y1, &z1, &x2, &y2, &z2) &&
		Api_Build(out, "ddd", x1 + x2, y1 + y2, z1 + z2);
}

/* res Sub(v1,v2) : Resta dos vectores 3D */
API_FN(SVec, Sub) {
	double x1, y1, z1, x2, y2, z2;
	return Mode_Parse(a, n, "(ddd)(ddd)", &x1, &y1, &z1, &x2, &y2, &z2) &&
		Api_Build(out, "ddd", x1 - x2, y1 - y2, z1 - z2);
}

/* res Mod(v1) : Devuelve el modulo de un vector */
API_FN(SVec, Mod) {
	double x, y, z;
	return Mode_Parse(a, n, "(ddd)", &x, &y, &z) && Api_Build(out, "d", sqrt(x * x + y * y + z * z));
}

/* res ModSqr(v1) : Devuelve el modulo (al cuadrado) de un vector */
API_FN(SVec, ModSqr) {
	double x, y, z;
	return Mode_Parse(a, n, "(ddd)", &x, &y, &z) && Api_Build(out, "d", x * x + y * y + z * z);
}

/* res Norm(v1) : Normaliza un vector. Below a length of 1e-6 the result is (0, 1, 0). */
API_FN(SVec, Norm) {
	double x, y, z;
	if (!Mode_Parse(a, n, "(ddd)", &x, &y, &z)) {
		return 0;
	}
	double len = sqrt(x * x + y * y + z * z);
	if (len < (double)1e-6f) { /* the constant is the float 1e-6 widened to double */
		return Api_Build(out, "ddd", 0.0, 1.0, 0.0);
	}
	return Api_Build(out, "ddd", x / len, y / len, z / len);
}

/* res Prod(v1,f) : multiplica un vector por un numero */
API_FN(SVec, Prod) {
	double x, y, z, f;
	return Mode_Parse(a, n, "(ddd)d", &x, &y, &z, &f) && Api_Build(out, "ddd", x * f, y * f, z * f);
}

/* res DProd(v1,v2) : Calcula el producto escalar de dos vectores */
API_FN(SVec, DProd) {
	vec3_t u, v;
	return Mode_Parse(a, n, "(fff)(fff)", &u.x, &u.y, &u.z, &v.x, &v.y, &v.z) &&
		Api_Build(out, "f", (double)(u.x * v.x + u.y * v.y + u.z * v.z));
}

/* vRes CProd(v1,v2) : Calcula el producto vectorial de dos vectores (a tuple of three floats) */
API_FN(SVec, CProd) {
	vec3_t v1, v2;
	if (!Mode_Parse(a, n, "(fff)(fff)", &v1.x, &v1.y, &v1.z, &v2.x, &v2.y, &v2.z)) {
		return 0;
	}
	vec3_t v3 = {
		v1.y * v2.z - v1.z * v2.y,
		v1.z * v2.x - v1.x * v2.z,
		v1.x * v2.y - v1.y * v2.x,
	};
	return Api_Build(out, "ddd", (double)v3.x, (double)v3.y, (double)v3.z);
}

/* res Rotate3D(src,rot) : Rota un vector. rot in degrees. */
API_FN(SVec, Rotate3D) {
	double sx, sy, sz, rx, ry, rz;
	if (!Mode_Parse(a, n, "(ddd)(ddd)", &sx, &sy, &sz, &rx, &ry, &rz)) {
		return 0;
	}
	vec3_t rot = { (float)Deg2Rad(rx), (float)Deg2Rad(ry), (float)Deg2Rad(rz) };
	vec3_t src = { (float)sx, (float)sy, (float)sz };
	vec3_t res;
	Vec3_Rotate(&res, &src, &rot);
	return Api_Build(out, "ddd", (double)res.x, (double)res.y, (double)res.z);
}

/* Ang GetAngle(vec) : Obtiene el ángulo entre dos vectores, in degrees */
API_FN(SVec, GetAngle) {
	vec3_t u, v;
	if (!Mode_Parse(a, n, "(fff)(fff)", &u.x, &u.y, &u.z, &v.x, &v.y, &v.z)) {
		return 0;
	}
	u = Vec3_Normalized(u);
	v = Vec3_Normalized(v);
	float c = u.x * v.x + u.y * v.y + u.z * v.z;
	/* clamp to [-1, 1] as compiled: comiss, then maxss, so NaN goes through */
	if (c > 1.0f) {
		c = 1.0f;
	}
	else if (-1.0f > c) {
		c = -1.0f;
	}
	return Api_Build(out, "d", Rad2Deg(acos((double)c)));
}

/* AngX,AngY GetRotAng(vec) : Obtiene la rotacion de un vector. Takes x, y, z or a tuple; degrees. */
API_FN(SVec, GetRotAng) {
	double x, y, z;
	if (!Mode_Parse(a, n, "ddd", &x, &y, &z) && (Native_ErrClear(), !Mode_Parse(a, n, "(ddd)", &x, &y, &z))) {
		return 0;
	}
	vec3_t dir = { (float)x, (float)y, (float)z };
	vec3_t ang;
	Vec3_GetRotAng(&ang, &dir);
	return Api_Build(out, "dd", Rad2Deg((double)ang.x), Rad2Deg((double)ang.y));
}

/* rAng NormAng(Ang) : Normaliza un angulo, in degrees */
API_FN(SVec, NormAng) {
	double ang;
	return Mode_Parse(a, n, "d", &ang) && Api_Build(out, "d", Rad2Deg((double)NormAng((float)Deg2Rad(ang))));
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

PORTS(PyInterface_SVecPy)
