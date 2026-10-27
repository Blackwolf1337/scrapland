/*
 * Engine/lee_M3D.cpp of the engine3d, the reader of the M3D models and scenes. The files are text (read with
 * include/scrap/leedatos.h, "Label: value" lines) or binary with the same order of values; a binary file may hold the
 * labels too, as strings. Binary ints are variable length: 6 bits and a sign bit in the first byte, 7 more bits in
 * each following byte while bit 7 is set.
 *
 * The game data has no M3D files, only their binary caches (.CM3, .SM3, read through CEscena_Read), so the reader of
 * the M3D files themselves (Escena_Load, rva 0x2c2d60, and the parts it calls) is still the exe's: it only runs in
 * the tools.
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
#define g_CM3DeleteM3D GAME_DATA(int, 0x4e1ed8)		/* CM3_DeleteM3D */
#define g_M3DOk GAME_DATA(byte, 0x432a7c)
#define g_ShowNodesTree GAME_DATA(int, 0x4e1ef0)		/* ShowNodesTree */
/* the [Model] switches of the ini of the model being read */
#define g_M3DUseOpacity GAME_DATA(byte, 0x4e1fa6)
#define g_M3DUseEnvmap GAME_DATA(byte, 0x4e1fa7)
#define g_M3DUseBumpmap GAME_DATA(byte, 0x4e1fb4)
#define g_M3DUseGlowmap GAME_DATA(byte, 0x4e1fb5)

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
/* the .CM3 of the animation scene esc of name and the .SM3 of the model scene esc (with SM3_UseScenes); a
 * failed write is reported, not fatal. CM3_DeleteM3D deletes the M3D after */
void CM3_Save(const char *name, CEscena *esc);
void SM3_Save(const char *name, CEscena *esc);
/* ok: g_M3DOk set; else the first failure after it reported */
void M3D_SetOk(byte ok);
/* the particle systems of the nodes with a Particles section (model_particles.ini of the scene in userProp); the
 * nodes of a LOD model point at those of the main LOD */
void Escena_Particles(CEscena *esc);
/* the log of the other nodes named as nodo before it */
void Escena_CheckDuplicados(CEscena *esc, CNodo3D *nodo);
void Nodo_MarcarBip(CNodo3D *nodo);
/* the "flare*" nodes to CFlare (size from "(tam=n)" in the name), the "sprite*" nodes to CGrafico3D, both from the
 * material of their first triangle */
void Escena_Flares(CEscena *esc);
void Escena_Sprites(CEscena *esc);
/* the empty nodes deleted; the tree logged with ShowNodesTree */
void Escena_Limpiar(CEscena *esc, const char *pass);
/* a and b equal or of the same sign within 2^-22 of each other */
byte Float_CasiIgual(float a, float b);
/* ant is above nodo; cruzar: also through NODO_BORRAR nodes */
byte Nodo_EsAntecesor(const CNodo3D *nodo, const CNodo3D *ant, byte cruzar);
/* the nodes the game uses by name: DR_, DM_, DC_, DF_, D_Cam-, Actor_, Traffic_, NAVEBERTO */
byte Nodo_EsDeJuego(const CNodo3D *nodo);
/* the int of "(key=n)" or "(+key=n)" in name */
byte Nodo_ParamInt(const char *name, const char *key, int *out);
/* bytes above 127 to '_', a warning for a space at the end */
void Nodo_LimpiarNombre(char *name);
/* map idmapa of the material used by the switches of the ini of the model and the video (MAT_SHADER: all) */
byte M3D_MapaActivo(const CMaterial *mat, int idmapa);
/* rva 0x216960 (ScrapEngMain.cpp): the node tree of the scene in the log */
void Escena_ShowTree(CEscena *esc, CNodo3D *padre, int indent);

#endif
