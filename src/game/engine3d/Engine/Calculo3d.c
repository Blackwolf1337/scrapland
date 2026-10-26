/*
 * 3D helpers of the engine3d, original source Engine/calculo3d.cpp (include/scrap/mathlib.h).
 */
#include "scrap/archio.h"
#include "scrap/e3dutil.h"

#define VEC3_STR 0x40
#define g_Vec3StrDeg ((char *)Game_Ptr(0x4ac018))	/* 4 x VEC3_STR */
#define g_Vec3Str ((char *)Game_Ptr(0x4ac118))

void Matrix_AddPos(matrix_t *m, const vec3_t *v) {
	m->m[12] = v->x + m->m[12];
	m->m[13] = v->y + m->m[13];
	m->m[14] = v->z + m->m[14];
}

int Vec3_TransformH(vec3_t *out, const vec3_t *v, const matrix_t *m) {
	const float *a = m->m;
	float x = v->x, y = v->y, z = v->z;
	float w = a[3] * x + a[7] * y + a[11] * z + a[15];

	out->y = (a[1] * x + a[5] * y + a[9] * z + a[13]) / w;
	out->x = (a[4] * y + x * a[0] + a[8] * z + a[12]) / w;
	out->z = (a[2] * x + a[6] * y + a[10] * z + a[14]) / w;
	return 0;
}

int Vec3_TransformDir0(vec3_t *out, const vec3_t *v, const matrix_t *m) {
	Matrix_TransformDir(out, v, m);
	return 0;
}

int Rand_Other(int min, int max, int avoid, int tries) {
	int r = avoid;

	CHECK(min <= max);
	for (int i = 0; i < tries; i++) {
		r = crt_rand() % (max - min + 1) + min;
		if (r != avoid) {
			break;
		}
	}
	return r;
}

/* v / len, unchanged for len 0 */
static vec3_t Vec3_Div(vec3_t v, float len) {
	if ((double)len != 0.0) {
		float k = (float)(1.0 / (double)len);
		v.x = v.x * k;
		v.y = v.y * k;
		v.z = v.z * k;
	}
	return v;
}

byte Sph_InCone(const vec3_t *center, float radius, const vec3_t *apex, const vec3_t *dir, float angle) {
	vec3_t d = { center->x - apex->x, center->y - apex->y, center->z - apex->z };
	float dist = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));

	if (radius >= dist) {
		return 1;
	}
	vec3_t n = Vec3_Div(*dir, (float)__builtin_sqrt((double)(dir->x * dir->x + dir->y * dir->y + dir->z * dir->z)));
	d = Vec3_Div(d, (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z)));
	float c = d.y * n.y + d.x * n.x + d.z * n.z;
	c = -1.0f >= c ? -1.0f : 1.0f < c ? 1.0f : c;
	float ang = (float)Game_Acos((double)c);
	float s = radius / dist;
	CHECK(__builtin_fabs((double)s) <= 1.0);
	float half = (float)Game_Asin((double)s);
	return (double)half + (double)angle * 0.5 >= (double)ang;
}

static char *Vec3_Format(char *buf, uint32_t slot, int prec, double x, double y, double z) {
	CHECK(slot < 4);
	char *s = buf + slot * VEC3_STR;
	Bprintf(s, VEC3_STR, "%.*f %.*f %.*f", prec, x, prec, y, prec, z);
	return s;
}

char *Vec3_StrDeg(const vec3_t *v, uint32_t slot, int prec) {
	return Vec3_Format(g_Vec3StrDeg, slot, prec, (double)v->x * 180.0 / M_PI_D, (double)v->y * 180.0 / M_PI_D,
		(double)v->z * 180.0 / M_PI_D);
}

char *Vec3_Str(const vec3_t *v, uint32_t slot, int prec) {
	return Vec3_Format(g_Vec3Str, slot, prec, (double)v->x, (double)v->y, (double)v->z);
}

/* struct _exception of the CRT */
typedef struct {
	int type;
	char *name;
	double arg1, arg2, retval;
} crtException_t;

int E3D_MathErr(void *exc) {
	static const char *const msgs[] = { "argument domain error", "argument singularity", "overflow range error",
		"underflow range error", "total loss of precision", "partial loss of precision" };
	const crtException_t *e = exc;

	if (!e) {
		return 0;
	}
	const char *msg = e->type >= 1 && e->type <= 6 ? msgs[e->type - 1] : "unknown math error";
	E3D_LogBlue(g_FileLog, "Math Error: %s: %s: %f", e->name, msg, e->arg1);
	return 0;
}
