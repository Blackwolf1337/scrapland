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
#include "scrap/material.h"
#include "scrap/mathlib.h"
#include "scrap/nodo3d.h"
#include "scrap/tabla.h"

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

/* the state of the reader while an M3D file is read */
#define g_M3DVersion GAME_DATA(int, 0x4e1fa0)		/* 5 .. 14 */
#define g_M3DPriFrame GAME_DATA(int, 0x4e1fa8)
#define g_M3DUltFrame GAME_DATA(int, 0x4e1fac)
#define g_M3DFrames GAME_DATA(int, 0x4e1fb0)		/* ult - pri + 1 */
#define g_M3DPreScaleU GAME_DATA(float, 0x4e1fb8)	/* [Model] PreScaleU of the ini */
#define g_M3DPreScaleV GAME_DATA(float, 0x4e1fbc)
/* the materials of the file: each with its material and sub material index of the file, used by a face */
#define M3D_MAX_MAT 0x200
#define g_M3DNumMat GAME_DATA(int, 0x4e25cc)
#define g_M3DMat ((CMaterial **)Game_Ptr(0x4e28e8))	/* [M3D_MAX_MAT] */
#define g_M3DMatUsado ((byte *)Game_Ptr(0x4e30e8))	/* [M3D_MAX_MAT] */
#define g_M3DMatId ((int *)Game_Ptr(0x4e32e8))		/* [M3D_MAX_MAT] */
#define g_M3DMatSub ((int *)Game_Ptr(0x4e3ae8))		/* [M3D_MAX_MAT] -1 for a material */
#define g_M3DLMat GAME_DATA(CListaMat *, 0x4e24c4)	/* the materials read, until they go to the scene */
#define M3D_MAX_MAT_ESCENA 0x100
#define g_M3DTexturePath ((char *)Game_Ptr(0x4e27e0))	/* MAX_RUTA, the dir of the file or [Model] TexturePath */
#define g_SharedTexturesPath GAME_DATA(const char *, 0x4e1f50)	/* gvar SharedTexuresPath, for "$name" maps */
#define g_OptimizeNodes GAME_DATA(int, 0x4e1f08)	/* gvar: 1 delete unused nodes, 2 collapse to bip nodes, 4 AniMask */
#define g_UseWSM GAME_DATA(int, 0x4e1f80)		/* gvar UseWSM */
#define g_UsarTexturas GAME_DATA(int, 0x431bcc)		/* video UsarTexturas: without, maps of b8 1 become colors */

/* an animation table of the file: one value a frame (CTabla of int or float) */
typedef struct M3DTabla {
	int n;				/* 0x0 */
	void *p;			/* 0x4 */
	byte fija;			/* 0x8 one run over all frames: constant */
	byte pad9[3];
} M3DTabla;
SD_SIZE(M3DTabla, 0xc);

/* the tables of a vector or rotation: x, then z and y (the files are z up), then w or the angle */
typedef struct M3DTabla4 {
	M3DTabla t[4];			/* 0x00 */
} M3DTabla4;
SD_SIZE(M3DTabla4, 0x30);

/* the animation of a node: PosX..Z, Sgn, QRotX..A, URotX..A, EscX..Z; the rotations turned into quaternions */
typedef struct M3DPista {
	M3DTabla4 pos;			/* 0x00 */
	M3DTabla sgn;			/* 0x30 */
	M3DTabla4 rot;			/* 0x3c */
	M3DTabla4 rotEsc;		/* 0x6c */
	M3DTabla4 esc;			/* 0x9c */
} M3DPista;
SD_SIZE(M3DPista, 0xcc);

/* the Physique weights of a vertex */
#define SKIN_MAX_BONES 3
typedef struct M3DSkinVert {
	int NumBones;			/* 0x00 */
	char *bone[SKIN_MAX_BONES];	/* 0x04 Cad */
	float peso[SKIN_MAX_BONES];	/* 0x10 */
} M3DSkinVert;
SD_SIZE(M3DSkinVert, 0x1c);

/* a transform of the scene nodes ("MT:" blocks) */
typedef struct m3dTransform_t {
	vec3_t pos;			/* 0x00 */
	quat_t rot;			/* 0x0c */
	quat_t rotEsc;			/* 0x1c the axes of the scale */
	vec3_t esc;			/* 0x2c */
	float signo;			/* 0x38 */
} m3dTransform_t;
SD_SIZE(m3dTransform_t, 0x3c);

/* what the reader keeps of each object of the file (CNodo3D indice) */
typedef struct M3DNodo {
	m3dTransform_t mt;		/* 0x000 the transform of the object */
	m3dTransform_t mt2;		/* 0x03c */
	matrix_t mNodo;			/* 0x078 */
	matrix_t mPivote;		/* 0x0b8 "local_a_pivote" */
	char *objetivo;			/* 0x0f8 Cad: the target of a camera or light, side 1 of a portal */
	char *lado2;			/* 0x0fc Cad: side 2 of a portal */
	struct CAnimNodo *AnimNodo;	/* 0x100 */
	CTabla skin;			/* 0x104 M3DSkinVert of each vertex */
	M3DPista pista;			/* 0x10c */
	M3DPista pista2;		/* 0x1d8 */
	M3DTabla t2a4[6];		/* 0x2a4 */
} M3DNodo;
SD_OFFSET(M3DNodo, AnimNodo, 0x100);
SD_OFFSET(M3DNodo, pista, 0x10c);
SD_SIZE(M3DNodo, 0x2ec);
#define g_M3DNodos GAME_DATA(M3DNodo *, 0x4e27dc)	/* new[] of the objects */

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
/* the material of the file for its material index and sub material (-1: the material itself); a sub material
 * index past the end wraps around unless it is below the count */
CMaterial *M3D_FindMat(int iMat, int iSubmat);
/* the entries of mat in the file table marked used, CHECK of at least one */
void M3D_MatUsado(CMaterial *mat);
/* the materials of the table (not the multi materials) replaced by the equal ones of lista */
void M3D_CompartirMats(CListaMat *lista);
/* Ambient, Diffuse, Specular, Shine, ShineStrength, Transparency of a standard material */
void M3D_ReadColores(CMaterial *mat);
/* the transform named name as a matrix */
void M3D_ReadMatrix(const char *name, matrix_t *out);
void M3D_ReadVec2Next(float *v);
vec3_t *M3D_ReadVec3Ret(vec3_t *v, const char *tok, byte swapYZ);
void M3D_ReadVec3Next(vec3_t *v, byte swapYZ);
char *M3D_ReadQString(const char *tok, char *dest, int size);
/* the reader data of the object of nodo */
M3DNodo *M3D_NodoDat(const CNodo3D *nodo);
/* frame i of the tables (x, z, y order of the file: the y up result) */
vec3_t *__thiscall M3DTabla4_GetVec(M3DTabla4 *self, vec3_t *out, uint32_t i);
quat_t *__thiscall M3DTabla4_GetQuat(M3DTabla4 *self, quat_t *out, uint32_t i);
void __thiscall M3DTabla4_SetVec(M3DTabla4 *self, uint32_t i, const vec3_t *v);
/* the axis and angle of frame i turned into the quaternion (the axis negated) */
void __thiscall M3DTabla4_RotToQuat(M3DTabla4 *self, uint32_t i);
/* the transform of frame of the first track as a matrix */
matrix_t *__thiscall M3DNodo_FrameMatrix(M3DNodo *self, matrix_t *out, uint32_t frame);
/* the table "Animacion: nombre" of g_M3DFrames values */
void __thiscall M3DTabla_ReadInt(M3DTabla *self, const char *nombre);
void __thiscall M3DTabla_ReadFloat(M3DTabla *self, const char *nombre);
/* up to four float tables; inv23 swaps the second and third */
void __thiscall M3DTabla4_Read(M3DTabla4 *self, const char *n1, const char *n2, const char *n3, const char *n4,
	byte inv23);
void __thiscall M3DPista_Read(M3DPista *self);
/* flag set with on in the name, cleared with off, else taken from the parent */
void Nodo_FlagNombre(CNodo3D *nodo, const char *on, const char *off, uint32_t flag);
/* the flags of the "(+x)" / "(-x)" names, the slerp and the node effects of the user properties */
void Escena_FlagsNombres(CEscena *esc);
/* SlerpNodes, MorphNodes, HideNodes, UnhideNodes of the ini (version 13 on); the hidden bones of the skins shown */
void Escena_FlagsIni(CEscena *esc);
/* the first node from nodo on (not NODO_BORRAR) below antecesor without a NODO_BORRAR node between */
CNodo3D *Nodo_BuscarDescendiente(CNodo3D *nodo, const CNodo3D *antecesor);
/* the same back from the node before nodo, through any node */
CNodo3D *Nodo_BuscarDescendienteAnt(CNodo3D *nodo, const CNodo3D *antecesor);
/* a plain mesh child of padre that can be joined into it (with a LOD the last one, or the first not in the main LOD) */
CNodo3D *Escena_BuscarColapsable(CNodo3D *nodo, const CNodo3D *padre);
/* the used materials of the file into the scene table and g_ListaMat, g_M3DLMat deleted */
void Escena_MatsUsados(CEscena *esc);
/* the camera, light and portal of nodo (with anim: the animated values of frame of an existing one) */
void M3D_LeeCamara(CNodo3D *nodo, byte anim, int frame);
void M3D_LeeLuz(CNodo3D *nodo, byte anim, int frame);
void M3D_LeePortal(CNodo3D *nodo, byte anim);
/* a texture map of mat and its sub maps (tipo: of the parent map, the "Mapa:" class sets it; idx unused) */
void M3D_LeeMapa(CMaterial *mat, int iMat, int iSubmat, int idx, int tipo);
/* a material into lista and the table of the file, with its maps and sub materials (iSubmat -1 for a material) */
void M3D_LeeMaterial(CListaMat *lista, int iMat, int iSubmat);

/* other modules (src/imports.inc) */
/* rva 0x2d6750 and 0x2d65d0: a transform as a matrix (the pivots of the meshes, the frames of the animation) */
void __thiscall M3DTransform_ToMatrix(m3dTransform_t *self, matrix_t *out);
matrix_t *__thiscall M3DTransform_ToMatrixAnim(m3dTransform_t *self, matrix_t *out);
/* rva 0x216960 (ScrapEngMain.cpp): the node tree of the scene in the log */
void Escena_ShowTree(CEscena *esc, CNodo3D *padre, int indent);

#endif
