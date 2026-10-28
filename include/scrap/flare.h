/*
 * Engine/flare.cpp of the engine3d: CFlare (class CLASE_OBJ3D_FLARE), a lens flare: a 2D sprite on the screen
 * position of its node that fades in while it is on the screen and the floor does not hide it (traced every 50 ms
 * and a bit), and out else. The [flares] variables of the ini: tam_def the size, fundido_ent and fundido_sal the
 * fades per second, with aten_dist hidden beyond aten_dist_fin.
 */
#ifndef SCRAP_FLARE_H
#define SCRAP_FLARE_H

#include "scrap/grafico2d.h"
#include "scrap/nodo3d.h"

#define CFLARE_VFTABLE GAME_VFTABLE(CFlare_vftable, 0x3e1958)

#define g_FlareTam GAME_DATA(float, 0x432998)		/* tam_def */
#define g_FlareAtenDist GAME_DATA(int, 0x4d6d90)	/* aten_dist */
#define g_FlareAtenDistIni GAME_DATA(float, 0x43299c)	/* aten_dist_ini */
#define g_FlareAtenDistFin GAME_DATA(float, 0x4329a0)	/* aten_dist_fin */
#define g_FlareFundidoEnt GAME_DATA(float, 0x4329a4)	/* fundido_ent */
#define g_FlareFundidoSal GAME_DATA(float, 0x4329a8)	/* fundido_sal */
#define g_FlareVerInfo GAME_DATA(int, 0x4d6d94)		/* ver_info */
#define g_SkipFlares GAME_DATA(int, 0x4d6dc0)		/* R_SkipFlares */
#define g_ShowFlaresInfo GAME_DATA(int, 0x4d6da8)	/* R_ShowFlaresInfo */
#define g_Sobredib GAME_DATA(int, 0x4a8f2c)		/* video sobredib, the overdraw view */
#define g_TimeMsF GAME_DATA(float, 0x4a8ed0)		/* the time of the engine in ms */

typedef struct CFlare {
	void **vftable;			/* 0x00 OBJ3D_VF_* */
	uint32_t clase;			/* 0x04 CLASE_OBJ3D_FLARE */
	CNodo3D *nodo;			/* 0x08 counted */
	vec3_t pos;			/* 0x0c for aten_dist */
	float escala[2];		/* 0x18 tam_def */
	uint32_t color;			/* 0x20 of the sprite, the alpha from transp */
	byte visible;			/* 0x24 this frame */
	byte visTrace;			/* 0x25 the floor did not hide it at the last trace */
	byte pad_26[2];
	float intens;			/* 0x28 1 */
	uint32_t lastTime;		/* 0x2c g_TimeMsF of the last render */
	uint32_t nextTrace;		/* 0x30 */
	CGrafico2D *grf;		/* 0x34 owned */
	float transp;			/* 0x38 1, the fade 0..1 */
} CFlare;
SD_SIZE(CFlare, 0x3c);

/* rva 0x297bd0, not called: 1 if the 11 floats of a and b are equal */
byte Floats11_Equal(const float *a, const float *b);
CFlare *__fastcall CFlare_ctor(CFlare *self);
void __fastcall CFlare_dtor(CFlare *self);
CFlare *__thiscall CFlare_Delete(CFlare *self, byte flags);
/* the sprite with the color and the size (the deferred draw of CFlare_Render) */
void __fastcall CFlare_Draw(CFlare *self);
void __thiscall CFlare_Render(CFlare *self, void *dev, CNodoR *r);
/* a new sprite of the texture, centered, size from escala */
void __thiscall CFlare_SetTexture(CFlare *self, const char *tex, const char *opac, int src, int dst, byte alpha);
/* additive (5, 6, 255) */
void __thiscall CFlare_SetTextureAdd(CFlare *self, const char *tex, const char *opac);

#endif
