/*
 * Engine/bsp_q3.cpp of the engine3d: a loader of Quake 3 BSP maps (IBSP version 0x2e, or 0x2f of RtCW). The faces
 * of type 1 (polygons) become fans of triangles in one CMeshD3D for the collision and in one vertex list per
 * material and light map (BspQ3Mesh, with the light map as the second texture); the light maps are written as
 * "<map>_lmap%04d.bmp" next to the map when missing. Dead code in the game: BspQ3_Create is empty, so g_BspQ3 stays
 * NULL and the global functions return at once.
 */
#ifndef SCRAP_BSPQ3_H
#define SCRAP_BSPQ3_H

#include "scrap/vertlist.h"
#include "scrap/meshd3d.h"
#include "scrap/material.h"

#define BSPQ3_ID 0x50534249		/* "IBSP" */
#define BSPQ3_VER 0x2e
#define BSPQ3_VER_RCW 0x2f
#define BSPQ3_NUM_LUMPS 17
#define BSPQ3_MAX_MESHES 256
#define BSPQ3_MAX_VERT 64
#define BSPQ3_LMAP_SIZE 128
#define BSPQ3_SIZE_ATTRIBTABLE 0x80

enum {
	BSPQ3_LUMP_SHADERS = 1,
	BSPQ3_LUMP_VERTICES = 10,
	BSPQ3_LUMP_FACES = 13,
	BSPQ3_LUMP_LIGHTMAPS = 14
};

/* face types */
enum {
	BSPQ3_POLYGON = 1,
	BSPQ3_PATCH = 2,
	BSPQ3_MESH = 3,
	BSPQ3_OTHER = 1000		/* billboards and unknown ones, not drawn */
};

typedef struct BspQ3Lump {
	int ofs;			/* 0x0 */
	int len;			/* 0x4 */
} BspQ3Lump;

typedef struct BspQ3Header {
	int id;				/* 0x00 BSPQ3_ID */
	int ver;			/* 0x04 */
	BspQ3Lump lumps[BSPQ3_NUM_LUMPS];	/* 0x08 */
} BspQ3Header;
SD_SIZE(BspQ3Header, 0x90);

typedef struct BspQ3Shader {
	char name[64];			/* 0x00 the texture, without extension */
	int flags;			/* 0x40 surface flags, 0x20: no light map */
	int contents;			/* 0x44 */
} BspQ3Shader;
SD_SIZE(BspQ3Shader, 0x48);

typedef struct BspQ3Vert {
	vec3_t pos;			/* 0x00 z up */
	float tc[2];			/* 0x0c */
	float lm[2];			/* 0x14 in the light map */
	vec3_t n;			/* 0x1c */
	byte color[4];			/* 0x28 */
} BspQ3Vert;
SD_SIZE(BspQ3Vert, 0x2c);

/* a face of the file */
typedef struct BspQ3FileFace {
	int shader;			/* 0x00 */
	int effect;			/* 0x04 */
	int type;			/* 0x08 */
	int vert;			/* 0x0c the first */
	int nVerts;			/* 0x10 */
	int meshVert;			/* 0x14 */
	int nMeshVerts;			/* 0x18 */
	int lmIndex;			/* 0x1c */
	int lmStart[2];			/* 0x20 */
	int lmSize[2];			/* 0x28 */
	vec3_t lmOrigin;		/* 0x30 */
	vec3_t lmVecs[2];		/* 0x3c */
	vec3_t normal;			/* 0x54 */
	int size[2];			/* 0x60 */
} BspQ3FileFace;
SD_SIZE(BspQ3FileFace, 0x68);

/* a face as the loader keeps it */
typedef struct BspQ3Face {
	int unused_00;			/* 0x00 */
	int type;			/* 0x04 BSPQ3_POLYGON .. */
	BspQ3Shader *shader;		/* 0x08 */
	CMaterial *mat;			/* 0x0c polygons, of the shader texture as .tga */
	int lmIndex;			/* 0x10 -1 for shaders without light map */
	BspQ3Vert *verts;		/* 0x14 */
	int nVerts;			/* 0x18 */
	int unused_1c;			/* 0x1c */
	int nMeshVerts;			/* 0x20 */
	int dist;			/* 0x24 polygon: the plane distance, truncated; patch: its index */
	vec3_t normal;			/* 0x28 polygon */
	vec3_t origin;			/* 0x34 polygon */
	vec3_t lmVecs[2];		/* 0x40 patch */
} BspQ3Face;
SD_SIZE(BspQ3Face, 0x58);

/* 8 bytes a face */
typedef struct BspQ3FaceRef {
	BspQ3Face *face;		/* 0x0 NULL for BSPQ3_OTHER */
	int i4;
} BspQ3FaceRef;

typedef struct BspQ3LMap {
	int unused_00;			/* 0x00 -1 */
	byte *data;			/* 0x04 RGB in the file buffer while loading, then NULL */
	int width;			/* 0x08 */
	int height;			/* 0x0c */
	int bpp;			/* 0x10 3 */
	CMaterial *mat;			/* 0x14 "Lightmap Mat %04d" */
} BspQ3LMap;
SD_SIZE(BspQ3LMap, 0x18);

/* the triangles of one material and light map: the vertices in VERT_TEX2 (0x28 bytes) */
typedef struct BspQ3Mesh {
	BspQ3Face *face;		/* 0x00 the first face (material and light map) */
	int nTri;			/* 0x04 counted first, then filled up to totTri */
	int totTri;			/* 0x08 */
	tri16_t *pTri;			/* 0x0c new[] */
	CVertList vert;		/* 0x10 */
	void *pVertVB;			/* 0x24 */
	void *pVertIB;			/* 0x28 */
} BspQ3Mesh;
SD_SIZE(BspQ3Mesh, 0x2c);

typedef struct BspQ3Meshes {
	BspQ3Mesh *m[BSPQ3_MAX_MESHES];	/* 0x000 */
	int n;				/* 0x400 */
} BspQ3Meshes;
SD_SIZE(BspQ3Meshes, 0x404);

typedef struct CBspQ3 {
	char *name;			/* 0x00 Cad */
	byte *lump;			/* 0x04 the whole file while loading */
	BspQ3Header *head;		/* 0x08 */
	int unused_0c;			/* 0x0c */
	int nVerts;			/* 0x10 */
	int unused_14[3];			/* 0x14 */
	int nShaders;			/* 0x20 */
	int nFaces;			/* 0x24 */
	int unused_28[2];			/* 0x28 */
	int nLMaps;			/* 0x30 */
	int unused_34[3];			/* 0x34 */
	BspQ3Vert *verts;		/* 0x40 new[] */
	BspQ3Shader *shaders;		/* 0x44 new[] */
	BspQ3LMap *lmaps;		/* 0x48 new[] */
	int nPatches;			/* 0x4c */
	BspQ3Face *faces;		/* 0x50 new[] */
	BspQ3FaceRef *faceRefs;		/* 0x54 new[] */
	int unused_58[2];			/* 0x58 */
	CMeshD3D *MeshD3D;		/* 0x60 all polygons */
	void *pMesh;			/* 0x64 ID3DXMesh, drawn instead with the L key held */
	BspQ3Meshes *MeshList;	/* 0x68 */
} CBspQ3;
SD_OFFSET(CBspQ3, verts, 0x40);
SD_OFFSET(CBspQ3, MeshD3D, 0x60);
SD_SIZE(CBspQ3, 0x6c);

#define g_BspQ3 GAME_GLOBAL(CBspQ3 *, g_BspQ3, 0x4e5bf8)
/* the corners of the face being cut into triangles: for the CTriMesh and for the vertex lists */
#define g_BspQ3Vert (GAME_ARRAY(CMeshVert, g_BspQ3Vert, 0x4e6600))	/* [BSPQ3_MAX_VERT] */
#define g_BspQ3VertL (GAME_ARRAY(byte, g_BspQ3VertL, 0x4e4af8))	/* [BSPQ3_MAX_VERT] of 0x28 bytes */
/* D3DXATTRIBUTERANGE of pMesh, 0x14 bytes each */
#define g_BspQ3Attrib (GAME_ARRAY(uint32_t, g_BspQ3Attrib, 0x4e5c00))	/* [BSPQ3_SIZE_ATTRIBTABLE * 5] */
#define g_KeyL GAME_GLOBAL_IN(byte, g_KeyboardKeys, 22, 0x4db2e6)

/* the map loaded: shaders, vertices, faces, light maps, then the meshes */
CBspQ3 *__thiscall CBspQ3_ctor(CBspQ3 *self, const char *name);
void __fastcall CBspQ3_dtor(CBspQ3 *self);
BspQ3Meshes *__fastcall BspQ3Meshes_ctor(BspQ3Meshes *self);
void __fastcall BspQ3Meshes_Free(BspQ3Meshes *self);
BspQ3Mesh *__fastcall BspQ3Mesh_ctor(BspQ3Mesh *self);
void __fastcall BspQ3Mesh_dtor(BspQ3Mesh *self);
/* the mesh of the material and light map of face, NULL */
BspQ3Mesh *__thiscall BspQ3Meshes_Find(BspQ3Meshes *self, BspQ3Face *face);
/* ntri more triangles for the mesh of face, a new mesh for a new pair */
void __thiscall BspQ3Meshes_Count(BspQ3Meshes *self, BspQ3Face *face, int ntri);
/* a triangle of the corners (0x28 bytes each) into the mesh of face; the first one allocates the counted ones */
void __thiscall BspQ3Meshes_AddTri(BspQ3Meshes *self, const void *a, const void *b, const void *c, BspQ3Face *face);
/* the x87 dot product (returned in st0, not rounded) */
long double Vec3_DotX87(const float *a, const float *b);
/* lump id copied into a new[] *out: the count of elements */
int __thiscall CBspQ3_ReadLump(CBspQ3 *self, int id, void **out, int elem);
void __fastcall CBspQ3_ReadShaders(CBspQ3 *self);
void __fastcall CBspQ3_ReadVerts(CBspQ3 *self);
void __fastcall CBspQ3_ReadFaces(CBspQ3 *self);
void __fastcall CBspQ3_ReadLMaps(CBspQ3 *self);
void __fastcall CBspQ3_Build(CBspQ3 *self);
void __fastcall CBspQ3_InitD3D(CBspQ3 *self);
void __fastcall CBspQ3_ReleaseD3D(CBspQ3 *self);
void __fastcall CBspQ3_Render(CBspQ3 *self);
/* rva 0x2c8770, an uncalled thiscall that returns this */
void *__fastcall BspQ3_Identity(void *self);

#endif
