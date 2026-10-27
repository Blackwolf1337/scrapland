/*
 * DebugUtils.cpp of the game: a copy of the debug lines of Engine/util_d3d.cpp with its own identity matrix and
 * brighter axes. The copies of Debug_DrawPointM, Debug_DrawBoxM, Debug_DrawLineM and Debug_DrawTriM are hooked to
 * the engine's C functions (src/hooks.inc); the sphere (rva 0x219780, 0x219d20) is still the exe's.
 */
#include "scrap/render.h"
#include "scrap/scrapmain.h"
#include "scrap/utild3d.h"

#define g_DebugIdentity ((const matrix_t *)Game_Ptr(0x431b10))

void DebugUtils_DrawAxes(const matrix_t *m, const vec3_t *pos, float size) {
	static const uint32_t colors[3] = { 0xff0000, 0xff00, 0xff };
	Render_SetWorldMatrix(m, 0);
	Debug_CrossLines(pos, size, colors);
}

void DebugUtils_DrawPoint(const vec3_t *pos, float size, uint32_t color) {
	Debug_DrawPointM(g_DebugIdentity, pos, size, color);
}

void DebugUtils_DrawBox(const vec3_t *mn, const vec3_t *mx, uint32_t color) {
	Debug_DrawBoxM(g_DebugIdentity, mn, mx, color);
}

void DebugUtils_DrawLine(const vec3_t *a, const vec3_t *b, uint32_t color) {
	Debug_DrawLineM(g_DebugIdentity, a, b, color);
}

void DebugUtils_DrawTri(const vec3_t *a, const vec3_t *b, const vec3_t *c, uint32_t color) {
	Debug_DrawTriM(g_DebugIdentity, a, b, c, color);
}
