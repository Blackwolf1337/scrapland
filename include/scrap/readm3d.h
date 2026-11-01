/*
 * Engine/lee_M3D.cpp of the engine3d, the reader of the M3D models and scenes. The files are text (read with
 * include/scrap/readdata.h, "Label: value" lines) or binary with the same order of values; a binary file may hold the
 * labels too, as strings. Binary ints are variable length: 6 bits and a sign bit in the first byte, 7 more bits in
 * each following byte while bit 7 is set.
 *
 * The game data has no M3D files, only their binary caches (.CM3, .SM3, read through CScene_Read), so the reader of
 * the M3D files themselves (Scene_Load, model.h) only gets past the .CM3 of an animation in the tools.
 */
#ifndef SCRAP_READM3D_H
#define SCRAP_READM3D_H

#include "scrap/game.h"
#include "scrap/material.h"
#include "scrap/mathlib.h"
#include "scrap/node3d.h"
#include "scrap/table.h"

#define M3D_STRING 0x400

#define g_M3DBinary GAME_GLOBAL_IN(byte, group_4e1fa4, 0)
#define g_M3DLabels GAME_GLOBAL_IN(byte, group_4e1fa4, 1)	/* text: lines start with the label; binary: labels stored */
#define g_M3DString (GAME_ARRAY(char, g_M3DString))	/* M3D_STRING bytes, strings without a destination */
#define g_M3DName (GAME_ARRAY(char, g_M3DName))	/* the file being read, for messages */
#define g_M3DDir (GAME_ARRAY(char, g_M3DDir))	/* MAX_FILEPATH, its directory */
#define g_M3DBaseName (GAME_ARRAY(char, g_M3DBaseName))	/* MAX_FILEPATH, its name without the extension */
#define g_M3DSceneMax (GAME_ARRAY(char, g_M3DSceneMax))	/* MAX_FILEPATH, "Escena:" (the scene of the editor) */
#define g_M3DFileINI GAME_GLOBAL(CIniFile *, g_M3DFileINI)	/* the .ini of the file, until the scene takes it */
#define g_M3DSceneTables GAME_GLOBAL(M3DTable *, g_M3DSceneTables)	/* 6: the animated ambient and background colors */
/* the scene of the model while an animation of it (lee_M3D_Estatica) or a LOD model of it is read */
#define g_SceneAnimParent GAME_GLOBAL(CScene *, g_SceneAnimParent)
#define g_SceneLODParent GAME_GLOBAL(CScene *, g_SceneLODParent)
/* the binary caches of the M3D files: .CM3 of the animations, .SM3 of the models, with the version and the
 * times of the M3D and the model (CM3) or the .INI (SM3) */
#define M3D_CACHE_VER 0x6515f8
#define g_CM3CheckTime GAME_GLOBAL_IN(int, GVar_CM3_CheckTime, 16)		/* CM3_CheckTime */
#define g_CM3UseScenes GAME_GLOBAL_IN(int, GVar_CM3_UseScenes, 16)		/* CM3_UseScenes 0, 1, 2 */
#define g_SM3CheckTime GAME_GLOBAL_IN(int, GVar_SM3_CheckTime, 16)		/* SM3_CheckTime */
#define g_SM3UseScenes GAME_GLOBAL_IN(int, GVar_SM3_UseScenes, 16)		/* SM3_UseScenes 0, 1, 2 */
#define g_CM3DeleteM3D GAME_GLOBAL_IN(int, GVar_CM3_DeleteM3D, 16)		/* CM3_DeleteM3D */
#define g_M3DOk GAME_GLOBAL_IN(byte, group_432a7c, 0)
#define g_ShowNodesTree GAME_GLOBAL_IN(int, g_ShowNodesTreeVar, 16)		/* ShowNodesTree */
/* the [Model] switches of the ini of the model being read */
#define g_M3DUseOpacity GAME_GLOBAL_IN(byte, group_4e1fa4, 2)
#define g_M3DUseEnvmap GAME_GLOBAL_IN(byte, group_4e1fa4, 3)
#define g_M3DUseBumpmap GAME_GLOBAL_IN(byte, group_4e1fb4, 0)
#define g_M3DUseGlowmap GAME_GLOBAL_IN(byte, group_4e1fb4, 1)

/* the state of the reader while an M3D file is read */
#define g_M3DVersion GAME_GLOBAL(int, g_M3DVersion)		/* 5 .. 14 */
#define g_M3DFirstFrame GAME_GLOBAL(int, g_M3DFirstFrame)
#define g_M3DLastFrame GAME_GLOBAL(int, g_M3DLastFrame)
#define g_M3DFrames GAME_GLOBAL(int, g_M3DFrames)		/* last - first + 1 */
#define g_M3DPreScaleU GAME_GLOBAL(float, g_M3DPreScaleU)	/* [Model] PreScaleU of the ini */
#define g_M3DPreScaleV GAME_GLOBAL(float, g_M3DPreScaleV)
/* the materials of the file: each with its material and sub material index of the file, used by a face */
#define M3D_MAX_MAT 0x200
#define g_M3DNumMat GAME_GLOBAL(int, g_M3DNumMat)
#define g_M3DMat (GAME_ARRAY(CMaterial *, g_M3DMat))	/* [M3D_MAX_MAT] */
#define g_M3DMatUsado (GAME_ARRAY(byte, g_M3DMatUsado))	/* [M3D_MAX_MAT] */
#define g_M3DMatId (GAME_ARRAY(int, g_M3DMatId))		/* [M3D_MAX_MAT] */
#define g_M3DMatSub (GAME_ARRAY(int, g_M3DMatSub))		/* [M3D_MAX_MAT] -1 for a material */
#define g_M3DLMat GAME_GLOBAL(CMatList *, g_M3DLMat)	/* the materials read, until they go to the scene */
#define M3D_MAX_MAT_SCENE 0x100
#define g_M3DTexturePath (GAME_ARRAY(char, g_M3DTexturePath))	/* MAX_FILEPATH, the dir of the file or [Model] TexturePath */
#define g_SharedTexturesPath GAME_GLOBAL_IN(const char *, GVar_SharedTexuresPath, 16)	/* gvar SharedTexuresPath, for "$name" maps */
#define g_OptimizeNodes GAME_GLOBAL_IN(int, GVar_OptimizeNodes, 16)	/* gvar: 1 delete unused nodes, 2 collapse to bip nodes, 4 AniMask */
#define g_UseWSM GAME_GLOBAL_IN(int, g_UseWSMVar, 16)		/* gvar UseWSM */
#define g_UseTextures GAME_GLOBAL(int, g_UseTextures)		/* video UsarTexturas: without, maps of b8 1 become colors */

/* an animation table of the file: one value a frame (CTable of int or float) */
typedef struct M3DTable {
	int n;				/* 0x0 */
	void *p;			/* 0x4 */
	byte fixed;			/* 0x8 one run over all frames: constant */
	byte pad9[3];
} M3DTable;
SD_SIZE(M3DTable, 0xc);

/* the tables of a vector or rotation: x, then z and y (the files are z up), then w or the angle */
typedef struct M3DTable4 {
	M3DTable t[4];			/* 0x00 */
} M3DTable4;
SD_SIZE(M3DTable4, 0x30);

/* the animation of a node: PosX..Z, Sgn, QRotX..A, URotX..A, EscX..Z; the rotations turned into quaternions */
typedef struct M3DTrack {
	M3DTable4 pos;			/* 0x00 */
	M3DTable sgn;			/* 0x30 */
	M3DTable4 rot;			/* 0x3c */
	M3DTable4 rotScale;		/* 0x6c */
	M3DTable4 scale;			/* 0x9c */
} M3DTrack;
SD_SIZE(M3DTrack, 0xcc);

/* the Physique weights of a vertex */
#define SKIN_MAX_BONES 3
typedef struct M3DSkinVert {
	int NumBones;			/* 0x00 */
	char *bone[SKIN_MAX_BONES];	/* 0x04 Cad */
	float weight[SKIN_MAX_BONES];	/* 0x10 */
} M3DSkinVert;
SD_SIZE(M3DSkinVert, 0x1c);

/* a transform of the scene nodes ("MT:" blocks) */
typedef struct m3dTransform_t {
	vec3_t pos;			/* 0x00 */
	quat_t rot;			/* 0x0c */
	quat_t rotScale;			/* 0x1c the axes of the scale */
	vec3_t scale;			/* 0x2c */
	float sign;			/* 0x38 */
} m3dTransform_t;
SD_SIZE(m3dTransform_t, 0x3c);

/* what the reader keeps of each object of the file (CNode3D index) */
typedef struct M3DNode {
	m3dTransform_t mt;		/* 0x000 the transform of the object */
	m3dTransform_t mt2;		/* 0x03c */
	matrix_t mNode;			/* 0x078 */
	matrix_t mPivot;		/* 0x0b8 "local_a_pivote" */
	char *target;			/* 0x0f8 Cad: the target of a camera or light, side 1 of a portal */
	char *side2;			/* 0x0fc Cad: side 2 of a portal */
	struct CAnimNode *AnimNode;	/* 0x100 */
	CTable skin;			/* 0x104 M3DSkinVert of each vertex */
	M3DTrack track;			/* 0x10c */
	M3DTrack track2;		/* 0x1d8 */
	M3DTable t2a4[6];		/* 0x2a4 */
} M3DNode;
SD_OFFSET(M3DNode, AnimNode, 0x100);
SD_OFFSET(M3DNode, track, 0x10c);
SD_SIZE(M3DNode, 0x2ec);
#define g_M3DNodes GAME_GLOBAL(M3DNode *, g_M3DNodes)	/* new[] of the objects */

/* text: the line must start with the label (or just the next line without labels); binary: the stored label must
 * be name */
void M3D_Label(const char *name);
/* the values; tok is the label before them, NULL to go on at the same place */
int M3D_ReadInt(const char *tok);
float M3D_ReadFloat(const char *tok);
byte M3D_ReadBool(const char *tok);	/* text "si" or "no" */
/* into dest (g_M3DString when NULL); text quoted or the rest of the line */
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
/* the animation scene of the .CM3 of name for g_SceneAnimParent, NULL without a valid one */
CScene *CM3_Load(const char *name);
/* the scene of the .SM3 of name, NULL without a valid one or with SM3_UseScenes not 1 */
CScene *SM3_Load(const char *name);
/* the .CM3 of the animation scene esc of name and the .SM3 of the model scene esc (with SM3_UseScenes); a
 * failed write is reported, not fatal. CM3_DeleteM3D deletes the M3D after */
void CM3_Save(const char *name, CScene *scn);
void SM3_Save(const char *name, CScene *scn);
/* ok: g_M3DOk set; else the first failure after it reported */
void M3D_SetOk(byte ok);
/* the particle systems of the nodes with a Particles section (model_particles.ini of the scene in userProp); the
 * nodes of a LOD model point at those of the main LOD */
void Scene_Particles(CScene *scn);
/* the log of the other nodes named as node before it */
void Scene_CheckDuplicates(CScene *scn, CNode3D *node);
void Node_MarkBip(CNode3D *node);
/* the "flare*" nodes to CFlare (size from "(tam=n)" in the name), the "sprite*" nodes to CGraphic3D, both from the
 * material of their first triangle */
void Scene_Flares(CScene *scn);
void Scene_Sprites(CScene *scn);
/* the empty nodes deleted; the tree logged with ShowNodesTree */
void Scene_Clear(CScene *scn, const char *pass);
/* a and b equal or of the same sign within 2^-22 of each other */
byte Float_AlmostEqual(float a, float b);
/* ant is above node; cross: also through NODE_DELETE nodes */
byte Node_IsAncestor(const CNode3D *node, const CNode3D *prev, byte cross);
/* the nodes the game uses by name: DR_, DM_, DC_, DF_, D_Cam-, Actor_, Traffic_, NAVEBERTO */
byte Node_IsOfGame(const CNode3D *node);
/* the int of "(key=n)" or "(+key=n)" in name */
byte Node_ParamInt(const char *name, const char *key, int *out);
/* bytes above 127 to '_', a warning for a space at the end */
void Node_ClearName(char *name);
/* map mapid of the material used by the switches of the ini of the model and the video (MAT_SHADER: all) */
byte M3D_ActiveMap(const CMaterial *mat, int mapid);
/* the constructors and destructors of the reader data; M3DTable is freed by Table_Free */
M3DTable *M3DTable_ctor(M3DTable *self);
char **String_ctor(char **self);
M3DTrack *M3DTrack_ctor(M3DTrack *self);
M3DNode *M3DNode_ctor(M3DNode *self);
M3DSkinVert *M3DSkinVert_ctor(M3DSkinVert *self);
void M3DSkinVert_dtor(M3DSkinVert *self);
void M3DTable4_dtor(M3DTable4 *self);
void M3DTrack_dtor(M3DTrack *self);
/* 6 tables freed, the last first */
void M3DTable6_dtor(M3DTable *self);
/* the delete[] of a table of M3DSkinVert */
void M3DSkin_Free(CTable *self);
void M3DNode_dtor(M3DNode *self);
/* flags 2: the new[] array with the count before it; 1: freed */
M3DNode *M3DNode_Delete(M3DNode *self, uint32_t flags);
M3DSkinVert *M3DSkinVert_Delete(M3DSkinVert *self, uint32_t flags);
/* the material of the file for its material index and sub material (-1: the material itself); a sub material
 * index past the end wraps around unless it is below the count */
CMaterial *M3D_FindMat(int iMat, int iSubmat);
/* the entries of mat in the file table marked used, CHECK of at least one */
void M3D_MatUsado(CMaterial *mat);
/* the materials of the table (not the multi materials) replaced by the equal ones of list */
void M3D_ShareMats(CMatList *list);
/* Ambient, Diffuse, Specular, Shine, ShineStrength, Transparency of a standard material */
void M3D_ReadColors(CMaterial *mat);
/* the transform named name as a matrix */
void M3D_ReadMatrix(const char *name, matrix_t *out);
void M3D_ReadVec2Next(float *v);
vec3_t *M3D_ReadVec3Ret(vec3_t *v, const char *tok, byte swapYZ);
void M3D_ReadVec3Next(vec3_t *v, byte swapYZ);
char *M3D_ReadQString(const char *tok, char *dest, int size);
/* the reader data of the object of node */
M3DNode *M3D_NodeDat(const CNode3D *node);
/* frame i of the tables (x, z, y order of the file: the y up result) */
vec3_t *M3DTable4_GetVec(M3DTable4 *self, vec3_t *out, uint32_t i);
quat_t *M3DTable4_GetQuat(M3DTable4 *self, quat_t *out, uint32_t i);
void M3DTable4_SetVec(M3DTable4 *self, uint32_t i, const vec3_t *v);
/* the axis and angle of frame i turned into the quaternion (the axis negated) */
void M3DTable4_RotToQuat(M3DTable4 *self, uint32_t i);
/* the transform of frame of the first track as a matrix */
matrix_t *M3DNode_FrameMatrix(M3DNode *self, matrix_t *out, uint32_t frame);
/* the table "Animacion: nombre" of g_M3DFrames values */
void M3DTable_ReadInt(M3DTable *self, const char *name);
void M3DTable_ReadFloat(M3DTable *self, const char *name);
/* up to four float tables; inv23 swaps the second and third */
void M3DTable4_Read(M3DTable4 *self, const char *n1, const char *n2, const char *n3, const char *n4,
	byte inv23);
void M3DTrack_Read(M3DTrack *self);
/* flag set with on in the name, cleared with off, else taken from the parent */
void Node_FlagName(CNode3D *node, const char *on, const char *off, uint32_t flag);
/* the flags of the "(+x)" / "(-x)" names, the slerp and the node effects of the user properties */
void Scene_FlagNames(CScene *scn);
/* SlerpNodes, MorphNodes, HideNodes, UnhideNodes of the ini (version 13 on); the hidden bones of the skins shown */
void Scene_FlagsIni(CScene *scn);
/* the first node from node on (not NODE_DELETE) below ancestor without a NODE_DELETE node between */
CNode3D *Node_FindDescendant(CNode3D *node, const CNode3D *ancestor);
/* the same back from the node before node, through any node */
CNode3D *Node_FindDescendantPrev(CNode3D *node, const CNode3D *ancestor);
/* a plain mesh child of parent that can be joined into it (with a LOD the last one, or the first not in the main LOD) */
CNode3D *Scene_FindCollapsible(CNode3D *node, const CNode3D *parent);
/* the used materials of the file into the scene table and g_MatList, g_M3DLMat deleted */
void Scene_UsedMats(CScene *scn);
/* the camera, light and portal of node (with anim: the animated values of frame of an existing one) */
void M3D_ReadCamera(CNode3D *node, byte anim, int frame);
void M3D_ReadLight(CNode3D *node, byte anim, int frame);
void M3D_ReadPortal(CNode3D *node, byte anim);
/* the mesh of node ("TipoGeom: Malla"): vertices through the pivot, Physique weights, faces with their materials,
 * texture vertices and faces, normals (nothing with anim) */
void M3D_ReadMesh(CNode3D *node, byte anim);
/* a texture map of mat and its sub maps (type: of the parent map, the "Mapa:" class sets it; idx unused) */
void M3D_ReadMap(CMaterial *mat, int iMat, int iSubmat, int idx, int type);
/* a material into list and the table of the file, with its maps and sub materials (iSubmat -1 for a material) */
void M3D_ReadMaterial(CMatList *list, int iMat, int iSubmat);

/* the matrix of the transform of the object */
matrix_t *M3DNode_CalcMatrix(M3DNode *self);
/* Read_NextLine in text files (0 for binary ones; the original leaves eax there) */
int M3D_NextLine(void);
/* the world matrices: CHECK of no scale, qLook from them, mObj their inverses */
void Scene_MatricesObj(CScene *scn);
/* the positions and rotations relative to the parents, with anim also those of the animation frames */
void Scene_AnimLocal(CScene *scn, byte anim);
/* the mesh of a plain child joined into the empty Biped nodes */
void Scene_CollapseBip(CScene *scn);
/* with Physique weights: all visible meshes (without skin: of their node) joined into one "SkinMesh" node */
void Scene_SkinMesh(CScene *scn);
/* i48 of the nodes (those without NODE_NO_RENDERNODE first), the bones of the skins to it; the AnimNodes masks */
void Scene_NodeTable(CScene *scn);
/* a LOD model: the nodes matched with those of the main LOD by name, warnings for the differences */
void Scene_SetupLOD(CScene *scn);
/* the Physique weights into the skins of the meshes (version 13 on) */
void Scene_PhysiqueSkin(CScene *scn);
/* the nodes reordered so that the parents come first */
void Scene_Sort(CScene *scn);
/* the parents of an animation scene set as in the model, then reordered */
void Scene_AnimParents(CScene *scn, CScene *staticScn);
/* with OptimizeNodes: the nodes under deleted (NODE_DELETE) parents moved up, cleaned before and after */
void Scene_Hierarchy(CScene *scn);

/* other modules: Engine/descomp_afin.cpp (src/game/engine3d/Engine/AffineDecomp.c) */
/* rva 0x2d6c50: the transform of a matrix */
void Matrix_ToTransform(const matrix_t *m, m3dTransform_t *out);
/* rva 0x2d6750 and 0x2d65d0: a transform as a matrix (the pivots of the meshes, the frames of the animation) */
void M3DTransform_ToMatrix(m3dTransform_t *self, matrix_t *out);
matrix_t *M3DTransform_ToMatrixAnim(m3dTransform_t *self, matrix_t *out);
/* rva 0x216960 (ScrapEngMain.cpp): the node tree of the scene in the log */
void Scene_ShowTree(CScene *scn, CNode3D *parent, int indent);

#endif
