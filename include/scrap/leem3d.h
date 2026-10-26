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

#define M3D_CAD 0x400

#define g_M3DBinary GAME_DATA(byte, 0x4e1fa4)
#define g_M3DLabels GAME_DATA(byte, 0x4e1fa5)	/* text: lines start with the label; binary: labels stored */
#define g_M3DCad ((char *)Game_Ptr(0x4e1fc0))	/* M3D_CAD bytes, strings without a destination */
#define g_M3DName ((char *)Game_Ptr(0x4e23c0))	/* the file being read, for messages */

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

#endif
