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

/* out = in rotated by the angles rot (radians: around x, then y, then z with x87 fsin/fcos), rva 0x15e3b0 */
void Vec3_Rotate(vec3_t *out, const vec3_t *in, const vec3_t *rot);
/* rotation angles (radians, x and y, z = 0) of the direction dir, rva 0x15df10 */
vec3_t *Vec3_GetRotAng(vec3_t *out, const vec3_t *dir);
/* angle in radians folded into (-pi, pi], rva 0x15e2e0 */
float NormAng(float ang);

/* acos of the exe's runtime, so results match the original to the last bit (src/core/libm.c) */
double Game_Acos(double x);
/* the CRT rand of the exe (rva 0x323c2f), same random sequence as the original */
int crt_rand(void);

#endif
