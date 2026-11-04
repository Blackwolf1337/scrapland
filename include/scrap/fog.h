/*
 * Engine/niebla.cpp of the engine3d: CFog, the fog of the world (the base of CWorld). Its color is its own or the
 * fog color of the floor scene; start and end are parts of the camera range (zNear..zFar), or world distances with
 * the table fog and R_FogAbsDist. Table fog (pixel fog with a D3DFOGMODE) when it is wanted and the device has it,
 * else linear vertex fog.
 */
#ifndef SCRAP_FOG_H
#define SCRAP_FOG_H

#include "scrap/gvar.h"
#include "scrap/node3d.h"

enum {
	FOG_COLOR_SCENE = 0,	/* the fog color of the floor scene, the own one without a floor */
	FOG_COLOR_OWN = 1,
};

typedef struct CFog {
	byte active;
	byte pad_1[3];
	float start;			/* 0.5 */
	float end;			/* 1 */
	float density;			/* 0.5, for the exponential table modes */
	int font;			/* NIEBLA_COLOR_* */
	ColorIntens color;
	byte table;			/* table fog wanted */
	byte capsW;			/* the device has w fog (the range is then the camera's) */
	byte capsTable;			/* the device has table fog */
	byte capsVertex;		/* the device has vertex fog, 1 */
	int mode;			/* D3DFOGMODE of the table fog, 3 linear */
} CFog;

/* the engine variables of the fog (Gvars) */
extern cGVar GVar_R_FogEnable;
#define g_FogEnable (GVar_R_FogEnable.value.i)

CFog *CFog_ctor(CFog *self);
/* empty */
void CFog_dtor(CFog *self);
/* the fog capabilities of the device */
void CFog_InitD3D(CFog *self);
/* the fog render states of the device for the frame */
void CFog_Apply(CFog *self);
/* the own color (ColorIntens as its two words) */
void CFog_SetColor(CFog *self, uint32_t rgba, float intens);
void CFog_SetRange(CFog *self, float start, float end);

/* other modules (src/imports.inc) */
/* the D3D color of a color and its intensity (src/game/ScrapEng/ScrapEngWorld.c) */
uint32_t Color_ToD3D(const ColorIntens *c);

#endif
