/*
 * Engine/util_d3d.cpp of the engine3d: debug lines in the world (boxes, crosses, axes, a cone, a sphere, triangles),
 * drawn at once through Render_Line in the space of a matrix (the identity without one), the D3D color of a float
 * color and the names of the object classes. Colors are 0xRRGGBB. The timer functions before it in the exe (rva
 * 0x276ba0..0x276e40, busy waits for the calibration) are still the exe's.
 */
#ifndef SCRAP_UTILD3D_H
#define SCRAP_UTILD3D_H

#include "scrap/mathlib.h"

#define MAX_DIVY_ESFERA 32

/* r, g, b, a (0..1, clamped) to 0xAARRGGBB */
uint32_t Color_FromFloat(const float *c);
void Debug_DrawBoxM(const matrix_t *m, const vec3_t *mn, const vec3_t *mx, uint32_t color);
void Debug_DrawBox(const vec3_t *mn, const vec3_t *mx, uint32_t color);
/* a cone of length len and opening ang (radians) from the origin down y */
void Debug_DrawConeM(const matrix_t *m, float len, float ang);
/* the axes x, y, z (red, green, blue) of length size to both sides of pos */
void Debug_DrawAxes(const matrix_t *m, const vec3_t *pos, float size);
/* a cross of size to both sides of pos */
void Debug_DrawPointM(const matrix_t *m, const vec3_t *pos, float size, uint32_t color);
void Debug_DrawPoint(const vec3_t *pos, float size, uint32_t color);
/* divV meridians, divH (-1: divV) parts of each, a cross at the center */
void Debug_DrawSphereM(const matrix_t *m, const vec3_t *c, float r, uint32_t color, int divV, int divH);
void Debug_DrawSphere(const vec3_t *c, float r, uint32_t color, int divV, int divH);
void Debug_DrawLineM(const matrix_t *m, const vec3_t *a, const vec3_t *b, uint32_t color);
void Debug_DrawLine(const vec3_t *a, const vec3_t *b, uint32_t color);
void Debug_DrawTriM(const matrix_t *m, const vec3_t *a, const vec3_t *b, const vec3_t *c, uint32_t color);
void Debug_DrawTri(const vec3_t *a, const vec3_t *b, const vec3_t *c, uint32_t color);
/* "MallaTri", "Luz", ... "otro" for an unknown class */
const char *Obj3D_ClaseName(uint32_t clase);

#endif
