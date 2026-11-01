/*
 * Engine/flare.cpp of the engine3d: CFlare (class CLASS_OBJ3D_FLARE), a lens flare: a 2D sprite on the screen
 * position of its node that fades in while it is on the screen and the floor does not hide it (traced every 50 ms
 * and a bit), and out else. The [flares] variables of the ini: tam_def the size, fundido_ent and fundido_sal the
 * fades per second, with aten_dist hidden beyond aten_dist_fin.
 */
#ifndef SCRAP_FLARE_H
#define SCRAP_FLARE_H

#include "scrap/gvar.h"
#include "scrap/graphic2d.h"
#include "scrap/node3d.h"

#define CFLARE_VFTABLE GAME_VFTABLE(CFlare_vftable)

extern float g_FlareSize;		/* tam_def */
extern int g_FlareAttenDist;	/* aten_dist */
extern float g_FlareAttenDistIni;	/* aten_dist_ini */
extern float g_FlareAttenDistEnd;	/* aten_dist_fin */
extern float g_FlareFadeIn;	/* fundido_ent */
extern float g_FlareFadeOut;	/* fundido_sal */
extern int g_FlareShowInfo;		/* ver_info */
extern cGVar GVar_R_SkipFlares;
#define g_SkipFlares (GVar_R_SkipFlares.value.i)		/* R_SkipFlares */
extern cGVar GVar_R_ShowFlaresInfo;
#define g_ShowFlaresInfo (GVar_R_ShowFlaresInfo.value.i)	/* R_ShowFlaresInfo */
extern int g_Overdraw;		/* video sobredib, the overdraw view */
extern float g_TimeMsF;		/* the time of the engine in ms */

typedef struct CFlare {
	void **vftable;			/* 0x00 OBJ3D_VF_* */
	uint32_t class;			/* 0x04 CLASS_OBJ3D_FLARE */
	CNode3D *node;			/* 0x08 counted */
	vec3_t pos;			/* 0x0c for aten_dist */
	float scale[2];		/* 0x18 tam_def */
	uint32_t color;			/* 0x20 of the sprite, the alpha from transp */
	byte visible;			/* 0x24 this frame */
	byte visTrace;			/* 0x25 the floor did not hide it at the last trace */
	byte pad_26[2];
	float intens;			/* 0x28 1 */
	uint32_t lastTime;		/* 0x2c g_TimeMsF of the last render */
	uint32_t nextTrace;		/* 0x30 */
	CGraphic2D *graphic;		/* 0x34 owned */
	float transp;			/* 0x38 1, the fade 0..1 */
} CFlare;
SD_SIZE(CFlare, 0x3c);

/* rva 0x297bd0, not called: 1 if the 11 floats of a and b are equal */
byte Floats11_Equal(const float *a, const float *b);
CFlare *CFlare_ctor(CFlare *self);
void CFlare_dtor(CFlare *self);
CFlare *CFlare_Delete(CFlare *self, byte flags);
/* the sprite with the color and the size (the deferred draw of CFlare_Render) */
void CFlare_Draw(CFlare *self);
void CFlare_Render(CFlare *self, void *dev, CRenderNode *r);
/* a new sprite of the texture, centered, size from scale */
void CFlare_SetTexture(CFlare *self, const char *tex, const char *opac, int src, int dst, byte alpha);
/* additive (5, 6, 255) */
void CFlare_SetTextureAdd(CFlare *self, const char *tex, const char *opac);

#endif
