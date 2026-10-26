/*
 * Engine math (original Math/ folder) and the math of the MSVC runtime inside Scrap.exe.
 */
#ifndef SCRAP_MATHLIB_H
#define SCRAP_MATHLIB_H

#include "scrap/shared.h"

#define M_PI_D 3.141592653589793	/* the double constants the engine uses (rva 0x3a3700, 0x3a3718) */
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
void Float_Approach(float *v, float target, float step);
void Vec3_Approach(vec3_t *v, const vec3_t *target, float step);
void Vec3_Bounce(const vec3_t *n, float m1, float m2, vec3_t *v1, vec3_t *v2);
byte Tri_HasPoint(const vec3_t *q, const vec3_t *t0, const vec3_t *t1, const vec3_t *t2);
byte Float_IsValid(float f);
byte Vec3_IsValid(const vec3_t *v);
float Dist_PointSegment(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out);
float Dist2_PointSegment(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out);
byte __thiscall Plane_SafeSet(plane_t *self, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3);
byte Quad_Solve(float a, float b, float c, float *t1, float *t2);
float Dist_PointTriangle(const vec3_t *p, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3, vec3_t *out);
long double Dist2_PointTriangle(const vec3_t *p, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3, vec3_t *out);
void Vec3_Reflect(const vec3_t *n, const vec3_t *v, vec3_t *out);

/* out = in rotated by the angles rot (radians: around x, then y, then z with x87 fsin/fcos), rva 0x15e3b0 */
void Vec3_Rotate(vec3_t *out, const vec3_t *in, const vec3_t *rot);
/* rotation angles (radians, x and y, z = 0) of the direction dir, rva 0x15df10 */
vec3_t *Vec3_GetRotAng(vec3_t *out, const vec3_t *dir);
/* angle in radians folded into (-pi, pi], rva 0x15e2e0 */
float NormAng(float ang);

/* acos, cos, sin and atan2 of the exe's runtime, so results match the original to the last bit (src/core/libm.c) */
double Game_Acos(double x);
double Game_Cos(double x);
double Game_Sin(double x);
double Game_Atan2(double y, double x);	/* _CIatan2 */

#endif
