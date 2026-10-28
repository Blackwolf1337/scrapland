/*
 * Engine/bsp.cpp of the engine3d, the map compiler (CBsp with [mapa_sombras] calcular, see include/scrap/bsp.h): the
 * lights of the scene, the texels of the lists and the filters of their light maps. The rest of bsp.cpp is in Bsp.c.
 */
#include <string.h>

#include "scrap/bsp.h"
#include "scrap/console.h"
#include "scrap/luz.h"
#include "scrap/mathlib.h"
#include "scrap/nodo3d.h"
#include "scrap/varios.h"

/* 1/255 of the color bytes (0x3b808081) */
#define BSP_INV255 0x1.010102p-8f


/* the lights of the scene */

void Bsp_TomarLuces(CEscena *esc) {
	static const vec3_t omniDir = { 2.0f, -1.0f, 1.0f };

	g_BspNumLuces = 0;
	Con_Verbosef("**** ind shw dsc\n");
	for (CNodo3D *n = CEscena_FindClase(esc, NULL, CLASE_OBJ3D_LUZ); n; n = CEscena_FindClase(esc, n, CLASE_OBJ3D_LUZ)) {
		CLuz *luz = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_LUZ);
		if (!luz->activa) {
			CLuz_SetActiva(luz, 0);
			continue;
		}
		CHECK(g_BspNumLuces < BSP_MAX_LUCES);
		BspLuz *l = &g_BspLuces[g_BspNumLuces++];
		Bprintf(l->nombre, sizeof(l->nombre), "%s", n->name);
		l->puntual = luz->tipo != LUZ_DIREC;
		l->sombra = luz->sombra;
		l->rad = luz->f44;
		l->pos = luz->d3d.Position;
		vec3_t d = luz->tipo == LUZ_OMNI ? omniDir : luz->d3d.Direction;
		float len = (float)__builtin_sqrt((double)(d.x * d.x + d.y * d.y + d.z * d.z));
		if ((double)len != 0.0) {
			float k = (float)(1.0 / (double)len);
			d.x = k * d.x;
			d.y = k * d.y;
			d.z = k * d.z;
		}
		l->dir = d;
		l->v8c[0] = 0.0f;
		l->v8c[1] = 0.0f;
		l->v8c[2] = 0.0f;
		CLuz_FillD3D(luz, &l->d3d);
		l->f88 = luz->f30;
		l->d3d.Position = l->pos;
		l->d3d.Direction = l->dir;
		l->stencil = luz->sombra && n->flags & NODO_SOMBRA;
		l->noLmap = n->flags >> 27 & 1;
		Con_Verbosef(" %d  shw:%d  stencil:%d  lmap:%d  dir:%d  rad:%.2f  rgbai:(%d %d %d %d %.2f)  p:(%.2f %.2f %.2f) "
			"d:(%.2f %.2f %.2f) a:(%.2f %.2f) r:(%.2f %.2f) [%s]\n", g_BspNumLuces - 1, l->sombra, l->stencil, l->noLmap,
			!l->puntual, (double)l->rad, luz->color[0], luz->color[1], luz->color[2], luz->color[3],
			(double)luz->intensidad, (double)l->pos.x, (double)l->pos.y, (double)l->pos.z, (double)l->dir.x,
			(double)l->dir.y, (double)l->dir.z, (double)luz->theta, (double)luz->phi, (double)luz->f30,
			(double)luz->rango, l->nombre);
		if (luz->intensidad < 0.0f) {
			Con_Warnf("Light with negative intensity: %s\n", n->name);
			float m;
			if (esc->ini && (m = CArchINI_GetFloat(esc->ini, "mapa_sombras", "negative_lights_mult", 1.0f)) != 1.0f) {
				float k = luz->intensidad * m * BSP_INV255;
				l->d3d.Diffuse.r = (float)luz->color[0] * k;
				l->d3d.Diffuse.g = (float)luz->color[1] * k;
				l->d3d.Diffuse.b = (float)luz->color[2] * k;
				l->d3d.Diffuse.a = (float)luz->color[3] * k;
				Con_Warnf(" light color set to: %f %f %f\n", (double)l->d3d.Diffuse.r, (double)l->d3d.Diffuse.g,
					(double)l->d3d.Diffuse.b);
			}
		}
		if (l->stencil && l->noLmap) {
			E3D_LogBlue(g_FileLog, "Stencil light with NO_LIGHTMAP flag: %s", n->name);
		}
		if (l->stencil != (n->flags >> 26 & 1)) {
			E3D_LogBlue(g_FileLog, "Light with RT_SHADOWS flag but no shadow enabled: %s", n->name);
		}
		CLuz_SetActiva(luz, 0);
	}
	uint32_t c = esc->ambiente.rgba;
	float k = esc->ambiente.intens * BSP_INV255;
	Con_Verbosef("AMBIENT: %f %f %f (rgbi = %d %d %d %f)\n", (double)((float)(c & 0xff) * k),
		(double)((float)(c >> 8 & 0xff) * k), (double)((float)(c >> 16 & 0xff) * k), c & 0xff, c >> 8 & 0xff,
		c >> 16 & 0xff, (double)esc->ambiente.intens);
	if (!esc->ini) {
		return;
	}
	vec3_t def = { 0.0f, 0.0f, 0.0f }, tmp;
	vec3_t a = *CArchINI_GetVec3(esc->ini, &tmp, "mapa_sombras", "lightmap_ambient", &def);
	if (a.x != 0.0f || a.y != 0.0f || a.z != 0.0f) {
		ColorIntens ci;
		Bsp_ColorIntens((uint32_t *)&ci, &a.x);
		esc->ambiente = ci;
		k = ci.intens * BSP_INV255;
		Con_Warnf(" AMBIENT color set to: %f %f %f (rgbi = %d %d %d %f)\n", (double)((float)(ci.rgba & 0xff) * k),
			(double)((float)(ci.rgba >> 8 & 0xff) * k), (double)((float)(ci.rgba >> 16 & 0xff) * k), ci.rgba & 0xff,
			ci.rgba >> 8 & 0xff, ci.rgba >> 16 & 0xff, (double)ci.intens);
	}
}
