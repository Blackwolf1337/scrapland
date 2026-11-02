/*
 * DebugUtils.cpp of the game: a copy of the debug lines of Engine/util_d3d.cpp with its own identity matrix and
 * brighter axes. The copies of Debug_DrawPointM, Debug_DrawBoxM, Debug_DrawLineM and Debug_DrawTriM are hooked to
 * the engine's C functions (src/hooks.inc); the sphere has its own axes at the center and divH 0 for divV.
 */
#include "scrap/render.h"
#include "scrap/scrapmain.h"
#include "scrap/utild3d.h"

/* DebugUtils.cpp (src/game/ScrapEng/DebugUtils.c): util_d3d.cpp lines with the identity of 0x431b10 */
static void DebugUtils_DrawAxes(const matrix_t *m, const vec3_t *pos, float size) {
	static const uint32_t colors[3] = { 0xff0000, 0xff00, 0xff };
	Render_SetWorldMatrix(m, 0);
	Debug_CrossLines(pos, size, colors);
}

static matrix_t g_DebugIdentity = { .m = { 1.0f, 0, 0, 0, 0, 1.0f, 0, 0, 0, 0, 1.0f, 0, 0, 0, 0, 1.0f } };

void DebugUtils_DrawBox(const vec3_t *mn, const vec3_t *mx, uint32_t color) {
	Debug_DrawBoxM((&g_DebugIdentity), mn, mx, color);
}

void DebugUtils_DrawLine(const vec3_t *a, const vec3_t *b, uint32_t color) {
	Debug_DrawLineM((&g_DebugIdentity), a, b, color);
}

void DebugUtils_DrawTri(const vec3_t *a, const vec3_t *b, const vec3_t *c, uint32_t color) {
	Debug_DrawTriM((&g_DebugIdentity), a, b, c, color);
}

/* divV meridians, divH (0: divV) parts of each, the axes at the center */
static void DebugUtils_DrawSphereM(const matrix_t *m, const vec3_t *c, float r, uint32_t color, int divV, int divH) {
	if (0.0f >= r) {
		return;
	}
	Debug_SphereLines(m, c, r, color, divV, divH ? divH : divV, DebugUtils_DrawAxes);
}

void DebugUtils_DrawSphere(const vec3_t *c, float r, uint32_t color, int divV, int divH) {
	DebugUtils_DrawSphereM((&g_DebugIdentity), c, r, color, divV, divH);
}
