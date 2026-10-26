/*
 * Engine/lee_M3D.cpp of the engine3d, the reader of the M3D models and scenes. The files are text (read with
 * include/scrap/leedatos.h, "Label: value" lines) or binary with the same order of values; a binary file may hold the
 * labels too, as strings. Binary ints are variable length: 6 bits and a sign bit in the first byte, 7 more bits in
 * each following byte while bit 7 is set.
 */
#ifndef SCRAP_LEEM3D_H
#define SCRAP_LEEM3D_H

#include "scrap/game.h"
#include "scrap/mathlib.h"
#include "scrap/nodo3d.h"

#define M3D_CAD 0x400

#define g_M3DBinary GAME_DATA(byte, 0x4e1fa4)
#define g_M3DLabels GAME_DATA(byte, 0x4e1fa5)	/* text: lines start with the label; binary: labels stored */
#define g_M3DCad ((char *)Game_Ptr(0x4e1fc0))	/* M3D_CAD bytes, strings without a destination */
#define g_M3DName ((char *)Game_Ptr(0x4e23c0))	/* the file being read, for messages */
/* the scene of the model while an animation of it (lee_M3D_Estatica) or a LOD model of it is read */
#define g_EscenaAnimPadre GAME_DATA(CEscena *, 0x4e1ec0)
#define g_EscenaLODPadre GAME_DATA(CEscena *, 0x4e1ec4)
/* the binary caches of the M3D files: .CM3 of the animations, .SM3 of the models, with the version and the
 * times of the M3D and the model (CM3) or the .INI (SM3) */
#define M3D_CACHE_VER 0x6515f8
#define g_CM3CheckTime GAME_DATA(int, 0x4e1f68)		/* CM3_CheckTime */
#define g_CM3UseScenes GAME_DATA(int, 0x4e1f20)		/* CM3_UseScenes 0, 1, 2 */
#define g_SM3CheckTime GAME_DATA(int, 0x4e1f38)		/* SM3_CheckTime */
#define g_SM3UseScenes GAME_DATA(int, 0x4e1f98)		/* SM3_UseScenes 0, 1, 2 */

/* a transform of the scene nodes ("MT:" blocks) */
typedef struct m3dTransform_t {
	vec3_t pos;			/* 0x00 */
	quat_t rot;			/* 0x0c */
	quat_t rotEsc;			/* 0x1c the axes of the scale */
	vec3_t esc;			/* 0x2c */
	float signo;			/* 0x38 */
} m3dTransform_t;
SD_SIZE(m3dTransform_t, 0x3c);

/* text: the line must start with the label (or just the next line without labels); binary: the stored label must
 * be name */
void M3D_Label(const char *name);
/* the values; tok is the label before them, NULL to go on at the same place */
int M3D_ReadInt(const char *tok);
float M3D_ReadFloat(const char *tok);
byte M3D_ReadBool(const char *tok);	/* text "si" or "no" */
/* into dest (g_M3DCad when NULL); text quoted or the rest of the line */
char *M3D_ReadString(const char *tok, char *dest, int size, byte quoted);
/* swapYZ: the files are z up */
void M3D_ReadVec3(const char *tok, vec3_t *v, byte swapYZ);
void M3D_ReadVec2(const char *tok, float *v);
/* axis and angle in the file, the axis turned around */
void M3D_ReadQuat(const char *tok, quat_t *q, byte swapYZ);
/* the transform named name */
void M3D_ReadTransform(const char *name, m3dTransform_t *mt);
/* an index line "n:", fatal if it is not expected */
void M3D_ReadIndex(int expected);
/* the animation scene of the .CM3 of name for g_EscenaAnimPadre, NULL without a valid one */
CEscena *CM3_Load(const char *name);
/* the scene of the .SM3 of name, NULL without a valid one or with SM3_UseScenes not 1 */
CEscena *SM3_Load(const char *name);
/* rva 0x2c2650: the particle systems of model_particles.ini for the scene */
void Escena_Particles(CEscena *esc);

#endif
