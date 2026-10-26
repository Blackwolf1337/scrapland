/*
 * The debug lines of the engine3d (rva 0x276cb0..0x278350, the name of the source file is not known): lines, boxes
 * and axes in the world, drawn with the current device at once. Colors are 0xRRGGBB.
 */
#ifndef SCRAP_DEBUG3D_H
#define SCRAP_DEBUG3D_H

#include "scrap/mathlib.h"

/* rva 0x276f90: the box mn..mx in the space of m */
void Debug_DrawBoxM(const matrix_t *m, const vec3_t *mn, const vec3_t *mx, uint32_t color);
/* rva 0x2772b0 */
void Debug_DrawBox(const vec3_t *mn, const vec3_t *mx, uint32_t color);
/* rva 0x277850: the axes x, y, z (red, green, blue) of length size from pos in the space of m */
void Debug_DrawAxes(const matrix_t *m, const vec3_t *pos, float size);
/* rva 0x277af0: a cross of size at pos */
void Debug_DrawPoint(const vec3_t *pos, float size, uint32_t color);
/* rva 0x278220 */
void Debug_DrawLine(const vec3_t *a, const vec3_t *b, uint32_t color);

#endif
