/*
 * Engine math (original Math/ folder) and the math of the MSVC runtime inside Scrap.exe.
 */
#ifndef SCRAP_MATHLIB_H
#define SCRAP_MATHLIB_H

#include "scrap/shared.h"

#define M_PI_D 3.141592653589793	/* the double constants the engine uses (rva 0x3a3700, 0x3a3718) */
#define M_2PI_D 6.283185307179586	/* rva 0x3b45c8 */
#define M_2PI_F 6.2831855f		/* rva 0x3a3a34 */
#define M_1_PI_D 0.3183098861837907	/* rva 0x3d9ae0 */
#define DEG_PER_PI 180.0

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
vec3_t *__thiscall Box_GetCorner(const box_t *self, vec3_t *out, int i);
void Box_Merge(box_t *box, const box_t *other, byte first);
float *Float_Approach(float *v, float target, float step);
vec3_t *Vec3_Approach(vec3_t *v, const vec3_t *target, float step);
vec3_t *Vec3_Bounce(const vec3_t *n, float m1, float m2, vec3_t *v1, vec3_t *v2);
byte Tri_HasPoint(const vec3_t *q, const vec3_t *t0, const vec3_t *t1, const vec3_t *t2);
byte Float_IsValid(float f);
byte Vec3_IsValid(const vec3_t *v);
float Dist_PointSegment(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out);
float Dist2_PointSegment(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out);
byte __thiscall Plane_SafeSet(plane_t *self, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3);
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

/* row vectors: v * M, translation in m[12..14]. Like the original the functions return a pointer (mostly the output),
 * callers in the exe use it */
typedef struct {
	float m[16];
} matrix_t;
SD_SIZE(matrix_t, 64);

typedef struct {
	float x, y, z, w;
} quat_t;
SD_SIZE(quat_t, 16);

/* Math/Matrix.cpp (src/game/Math/Matrix.c) */
void __thiscall Matrix_Identity(matrix_t *self);
matrix_t *__thiscall Matrix_Copy(matrix_t *self, const matrix_t *src);	/* two copies in the exe */
matrix_t *__thiscall Matrix_Set(matrix_t *self, float m0, float m1, float m2, float m3, float m4, float m5, float m6,
	float m7, float m8, float m9, float m10, float m11, float m12, float m13, float m14, float m15);
matrix_t *__thiscall Matrix_FromFloats(matrix_t *self, const float *f);
matrix_t *__thiscall Matrix_Mul(const matrix_t *self, matrix_t *out, const matrix_t *other);	/* out = self * other */
vec3_t *__thiscall Matrix_GetEuler(const matrix_t *self, vec3_t *out);
vec3_t *__thiscall Matrix_GetRotAng(const matrix_t *self, vec3_t *out);
byte Matrix_InvertAffine(matrix_t *out, const matrix_t *m);
matrix_t *Matrix_Rotate(matrix_t *m, const vec3_t *ang);	/* m = m * Rz * Rx * Ry */
const vec3_t *__thiscall Matrix_Scale(matrix_t *self, const vec3_t *s);	/* returns s */
byte Matrix_Perspective(matrix_t *out, float fov, float aspect, float zNear, float zFar);
void __thiscall Matrix_RotX(matrix_t *self, float ang);
void __thiscall Matrix_RotY(matrix_t *self, float ang);
void __thiscall Matrix_RotZ(matrix_t *self, float ang);
void __thiscall Matrix_RotAxis(matrix_t *self, const vec3_t *axis, float ang);
const vec3_t *__thiscall Matrix_Translate(matrix_t *self, const vec3_t *v);	/* returns v */
byte Matrix_TransformProject(vec3_t *out, const vec3_t *v, const matrix_t *m);
/* the math of the engine3d still in the exe */
matrix_t *Matrix_LoadIdentity(matrix_t *m);			/* src/game/ScrapEng/ScrapEngWorld.c */
void Matrix_PostScale(matrix_t *m, const vec3_t *s);		/* rva 0x239210 */
void Matrix_PostRotate(matrix_t *m, const vec3_t *rot);		/* rva 0x23a100, yaw y pitch x roll z */
matrix_t *Matrix_RotationX(matrix_t *out, float ang);		/* rva 0x23a270, D3DXMatrixRotationX */
matrix_t *Matrix_RotationZ(matrix_t *out, float ang);		/* rva 0x23a2b0, D3DXMatrixRotationZ */
vec3_t *Vec3_ToAngles(vec3_t *out, const vec3_t *dir);		/* rva 0x239150, x pitch, y yaw, z 0 */
int Matrix_Invert(matrix_t *out, const matrix_t *m);		/* rva 0x239560 (matriz.h), 0 S_OK */
matrix_t *Matrix_MulD3D(matrix_t *out, const matrix_t *a, const matrix_t *b);	/* rva 0x2391f0, out = a * b */
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

/* acos, cos, sin and atan2 of the exe's runtime, so results match the original to the last bit (src/core/libm.c) */
double Game_Acos(double x);
double Game_Cos(double x);
double Game_Sin(double x);
double Game_Atan2(double y, double x);	/* _CIatan2 */
double Game_Fmod(double x, double y);	/* _CIfmod */
double Game_Asin(double x);
double Game_Exp(double x);
double Game_Log(double x);
double Game_Pow(double x, double y);
int64_t X87_Fistp(double x);	/* round to nearest */
void X87_SinCos(float a, float *s, float *c);	/* fsincos, both rounded to float */
void Game_ExpPd(const double in[2], double out[2]);	/* the vectorised exp of loops, two at once */
void Game_SinPd(const double in[2], double out[2]);
void Game_CosPd(const double in[2], double out[2]);


/* Engine/calculo3d.cpp (src/game/engine3d/Engine/Calculo3d.c) */
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

/* the value hidden from the optimizer: keeps an operation the compiler would fold algebraically ((-a) - (-b) to
 * b - a), which changes the sign of a NaN result */
static inline float Float_Keep(float v) {
	__asm__("" : "+x"(v));
	return v;
}

#endif
