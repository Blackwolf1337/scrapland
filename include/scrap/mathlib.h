/*
 * Engine math (original Math/ folder) and the math of the MSVC runtime inside Scrap.exe.
 */
#ifndef SCRAP_MATHLIB_H
#define SCRAP_MATHLIB_H

#include <math.h>

#include "scrap/shared.h"

#define M_PI_D 3.141592653589793	/* the double constants the engine uses */
#define M_2PI_D 6.283185307179586
#define M_2PI_F 6.2831855f
#define M_PI_F 3.1415927f
#define M_1_PI_D 0.3183098861837907
#define DEG_PER_PI 180.0
#define RAND_SCALE (1.0 / 32767.0)	/* crt_rand() * RAND_SCALE: 0..1 */

/* radians to degrees in double, as the engine computes it (rad * 180 / pi) */
static inline double Rad2Deg(double rad) {
	return rad * DEG_PER_PI / M_PI_D;
}

typedef struct {
	float x, y, z;
} vec3_t;
SD_SIZE(vec3_t, 12);

typedef struct {
	vec3_t min, max;
} box_t;
SD_SIZE(box_t, 24);

/* n.p + d = 0 */
typedef struct {
	vec3_t n;
	float d;
} plane_t;
SD_SIZE(plane_t, 16);

/* _fpclass results */
enum { FPCLASS_NN = 0x8, FPCLASS_NZ = 0x20, FPCLASS_PZ = 0x40, FPCLASS_PN = 0x100 };

/* Math/MathUtils.cpp (src/game/Math/MathUtils.c) */
vec3_t *Box_GetCorner(const box_t *self, vec3_t *out, int i);
/* the box constructor of two corners (rva 0x2a4830, src/game/engine3d/Engine/RenderLock.c) */
box_t *Box_Set(box_t *self, const vec3_t *min, const vec3_t *max);
void Box_Merge(box_t *box, const box_t *other, byte first);
float *Float_Approach(float *v, float target, float step);
vec3_t *Vec3_Approach(vec3_t *v, const vec3_t *target, float step);
vec3_t *Vec3_Bounce(const vec3_t *n, float m1, float m2, vec3_t *v1, vec3_t *v2);
byte Tri_HasPoint(const vec3_t *q, const vec3_t *t0, const vec3_t *t1, const vec3_t *t2);
byte Float_IsValid(float f);
byte Vec3_IsValid(const vec3_t *v);
float Dist_PointSegment(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out);
float Dist2_PointSegment(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out);
byte Plane_SafeSet(plane_t *self, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3);
byte Quad_Solve(float a, float b, float c, float *t1, float *t2);
float Dist_PointTriangle(const vec3_t *p, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3, vec3_t *out);
long double Dist2_PointTriangle(const vec3_t *p, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3, vec3_t *out);
vec3_t *Vec3_Reflect(const vec3_t *n, const vec3_t *v, vec3_t *out);

/* the rest of the math code of the exe around Math/Matrix.cpp (src/game/Math/MathUtils.c), angles in radians */
vec3_t *Vec3_GetRotAng(vec3_t *out, const vec3_t *dir);	/* x and y that turn the z axis into dir, z = 0 */
vec3_t *Vec3_RotateSC(vec3_t *out, const vec3_t *in, const vec3_t *c, const vec3_t *s);
vec3_t *Vec3_RotateInv(vec3_t *out, const vec3_t *in, const vec3_t *rot);
vec3_t *Vec3_NormAng(vec3_t *out, const vec3_t *in);
float NormAng(float ang);	/* folded into (-pi, pi] */
float *Vec2_Rotate(float *out, const float *in, float ang);
vec3_t *Vec3_Rotate(vec3_t *out, const vec3_t *in, const vec3_t *rot);	/* around z, then x, then y */
vec3_t *Vec3_RotateXY(vec3_t *out, const vec3_t *in, float ax, float ay);
/* the 3x3 rotation (row major) that turns the unit vector from into to; m */
float *Matrix3_RotFromTo(const vec3_t *from, const vec3_t *to, float *m);

/* row vectors: v * M, translation in m[12..14]. Like the original the functions return a pointer (mostly the output),
 * callers in the exe use it */
typedef struct {
	union {
		float m[16];
		struct {
			float rot[12];
			vec3_t pos;	/* the translation, m[12..14] */
			float w;
		};
	};
} matrix_t;
SD_SIZE(matrix_t, 64);

typedef struct {
	float x, y, z, w;
} quat_t;
SD_SIZE(quat_t, 16);

/* Math/Matrix.cpp (src/game/Math/Matrix.c) */
void Matrix_Identity(matrix_t *self);
matrix_t *Matrix_Copy(matrix_t *self, const matrix_t *src);	/* two copies in the exe */
matrix_t *Matrix_FromFloats(matrix_t *self, const float *f);
matrix_t *Matrix_Mul(const matrix_t *self, matrix_t *out, const matrix_t *other);	/* out = self * other */
vec3_t *Matrix_GetEuler(const matrix_t *self, vec3_t *out);
vec3_t *Matrix_GetRotAng(const matrix_t *self, vec3_t *out);
byte Matrix_InvertAffine(matrix_t *out, const matrix_t *m);
matrix_t *Matrix_Rotate(matrix_t *m, const vec3_t *ang);	/* m = m * Rz * Rx * Ry */
const vec3_t *Matrix_Scale(matrix_t *self, const vec3_t *s);	/* returns s */
byte Matrix_Perspective(matrix_t *out, float fov, float aspect, float zNear, float zFar);
void Matrix_RotX(matrix_t *self, float ang);
void Matrix_RotY(matrix_t *self, float ang);
void Matrix_RotZ(matrix_t *self, float ang);
void Matrix_RotAxis(matrix_t *self, const vec3_t *axis, float ang);
const vec3_t *Matrix_Translate(matrix_t *self, const vec3_t *v);	/* returns v */
byte Matrix_TransformProject(vec3_t *out, const vec3_t *v, const matrix_t *m);
/* the math of the engine3d (matriz.h: src/game/engine3d/Engine/Calc3d.c and Math/Matrix.c), D3DX through
 * the stubs of the exe */
matrix_t *Matrix_LoadIdentity(matrix_t *m);			/* src/game/ScrapEng/ScrapEngWorld.c */
const vec3_t *Matrix_PostScale(matrix_t *m, const vec3_t *s);	/* rva 0x239210, Matrix_Scale, returns s */
void Matrix_PostRotate(matrix_t *m, const vec3_t *rot);		/* rva 0x23a100, yaw y pitch x roll z */
matrix_t *Matrix_RotationX(matrix_t *out, float ang);		/* rva 0x23a270, D3DXMatrixRotationX */
matrix_t *Matrix_RotationY(matrix_t *out, float ang);		/* rva 0x23a290, D3DXMatrixRotationY */
matrix_t *Matrix_RotationZ(matrix_t *out, float ang);		/* rva 0x23a2b0, D3DXMatrixRotationZ */
/* rva 0x23a2d0: Matrix_RotAxis returning out */
matrix_t *Matrix_RotationAxis(matrix_t *out, const vec3_t *axis, float ang);
int Matrix_Invert(matrix_t *out, const matrix_t *m);		/* rva 0x239560, Matrix_InvertAffine, 0 S_OK */
int Matrix_PerspectiveD3D(matrix_t *out, float fov, float aspect, float zNear, float zFar);
matrix_t *Matrix_MulD3D(matrix_t *out, const matrix_t *a, const matrix_t *b);	/* rva 0x2391f0, out = a * b */
/* rva 0x239820: out = a * b through a copy, out may be a or b */
void Matrix_MulTo(matrix_t *out, const matrix_t *a, const matrix_t *b);
/* the euler angles of a rotation matrix */
vec3_t *Matrix_ToEuler(vec3_t *out, const matrix_t *m);
vec3_t *Matrix_TransformDir(vec3_t *out, const vec3_t *v, const matrix_t *m);
const matrix_t *Matrix_TransformParts(float *x, float *y, float *z, float *w, const vec3_t *v, const matrix_t *m);	/* m */

/* quaternions (src/game/Math/Quat.c) */
quat_t *Quat_Mul(quat_t *out, const quat_t *a, const quat_t *b);
quat_t *Quat_FromAxisAngle(quat_t *out, const vec3_t *axis, float ang);
float *Quat_ToAxisAngle(vec3_t *axis, float *ang, const quat_t *q);	/* returns ang */
matrix_t *Quat_ToMatrix(matrix_t *out, const quat_t *q);
quat_t *Quat_Slerp(quat_t *out, const quat_t *a, const quat_t *b, float t);
/* Quat_Slerp that goes the other way round for large rotations (then 1) */
byte Quat_SlerpFlip(quat_t *out, const quat_t *a, const quat_t *b, float t);
quat_t *Quat_FromEuler(quat_t *out, float x, float y, float z);	/* radians */
float Quat_DotAbs(const quat_t *a, const quat_t *b);
matrix_t *Quat_ToMatrixD3D(matrix_t *out, const quat_t *q);

/* a float or double to an unsigned int, truncated; a negative value wraps like the original's conversion */
static inline uint32_t Float_ToU32(double x) {
	return (uint32_t)(int64_t)x;
}

/* the sine and cosine of a, both as floats */
static inline void Float_SinCos(float a, float *s, float *c) {
	*s = (float)sin(a);
	*c = (float)cos(a);
}


/* Engine/calculo3d.cpp (src/game/engine3d/Engine/Calc3d.c) */
void Matrix_AddPos(matrix_t *m, const vec3_t *v);
/* v * m divided by w, no check of w; 0 */
int Vec3_TransformH(vec3_t *out, const vec3_t *v, const matrix_t *m);
/* Matrix_TransformDir that returns 0 */
int Vec3_TransformDir0(vec3_t *out, const vec3_t *v, const matrix_t *m);
/* a rand() number in min..max other than avoid, up to tries times (avoid for no tries) */
int Rand_Other(int min, int max, int avoid, int tries);
/* whether a sphere is seen in a cone from apex along dir, angle the opening (radians) */
byte Sph_InCone(const vec3_t *center, float radius, const vec3_t *apex, const vec3_t *dir, float angle);
/* "x y z" with prec decimals into one of 4 static buffers (slot); StrDeg of an angle vector in degrees */
char *Vec3_StrDeg(const vec3_t *v, uint32_t slot, int prec);
char *Vec3_Str(const vec3_t *v, uint32_t slot, int prec);
/* the _matherr of the exe: a line in the file log, 0 */
int E3D_MathErr(void *exc);

/* the minimum and maximum the way minss and maxss pick: a when it is smaller (bigger), else b (also unordered) */
static inline float Float_MinSS(float a, float b) {
	return a < b ? a : b;
}

static inline float Float_MaxSS(float a, float b) {
	return a > b ? a : b;
}

/* v / |v| as the engine3d normalizes inline: the length of (y y + x x) + z z through double, the factor 1 / len
 * rounded to float; unchanged for the length 0 */
static inline void Vec3_Unit(vec3_t *v) {
	float len = (float)__builtin_sqrt((double)(v->y * v->y + v->x * v->x + v->z * v->z));
	if ((double)len != 0.0) {
		float k = (float)(1.0 / (double)len);
		v->x = v->x * k;
		v->y = v->y * k;
		v->z = v->z * k;
	}
}

/* __ltod3 of the CRT: the high word times 2^32 plus the low word as unsigned */
static inline double Int64_ToDouble(int64_t v) {
	double hi = (double)(int32_t)(v >> 32) * 4294967296.0;
	int32_t lo = (int32_t)(uint32_t)v;
	double d = (double)lo + (lo < 0 ? 4294967296.0 : 0.0);
	return d + hi;
}

/* the value hidden from the optimizer: keeps an operation the compiler would fold algebraically ((-a) - (-b) to
 * b - a), which changes the sign of a NaN result */
static inline float Float_Keep(float v) {
	__asm__("" : "+x"(v));
	return v;
}

#endif
