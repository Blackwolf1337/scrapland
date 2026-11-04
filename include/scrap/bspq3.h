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
	int ofs;
	int len;
} BspQ3Lump;

typedef struct BspQ3Header {
	int id;				/* BSPQ3_ID */
	int ver;
	BspQ3Lump lumps[BSPQ3_NUM_LUMPS];
} BspQ3Header;

typedef struct BspQ3Shader {
	char name[64];			/* the texture, without extension */
	int flags;			/* surface flags, 0x20: no light map */
	int contents;
} BspQ3Shader;

typedef struct BspQ3Vert {
	vec3_t pos;			/* z up */
	float tc[2];
	float lm[2];			/* in the light map */
	vec3_t n;
	byte color[4];
} BspQ3Vert;

/* a face of the file */
typedef struct BspQ3FileFace {
	int shader;
	int effect;
	int type;
	int vert;			/* the first */
	int nVerts;
	int meshVert;
	int nMeshVerts;
	int lmIndex;
	int lmStart[2];
	int lmSize[2];
	vec3_t lmOrigin;
	vec3_t lmVecs[2];
	vec3_t normal;
	int size[2];
} BspQ3FileFace;

/* a face as the loader keeps it */
typedef struct BspQ3Face {
	int unused_00;
	int type;			/* BSPQ3_POLYGON .. */
	BspQ3Shader *shader;
	CMaterial *mat;			/* polygons, of the shader texture as .tga */
	int lmIndex;			/* -1 for shaders without light map */
	BspQ3Vert *verts;
	int nVerts;
	int unused_1c;
	int nMeshVerts;
	int dist;			/* polygon: the plane distance, truncated; patch: its index */
	vec3_t normal;			/* polygon */
	vec3_t origin;			/* polygon */
	vec3_t lmVecs[2];		/* patch */
} BspQ3Face;

/* 8 bytes a face */
typedef struct BspQ3FaceRef {
	BspQ3Face *face;		/* NULL for BSPQ3_OTHER */
	int i4;
} BspQ3FaceRef;

typedef struct BspQ3LMap {
	int unused_00;			/* -1 */
	byte *data;			/* RGB in the file buffer while loading, then NULL */
	int width;
	int height;
	int bpp;			/* 3 */
	CMaterial *mat;			/* "Lightmap Mat %04d" */
} BspQ3LMap;

/* the triangles of one material and light map: the vertices in VERT_TEX2 */
typedef struct BspQ3Mesh {
	BspQ3Face *face;		/* the first face (material and light map) */
	int nTri;			/* counted first, then filled up to totTri */
	int totTri;
	tri16_t *pTri;			/* new[] */
	CVertList vert;
	void *pVertVB;
	void *pVertIB;
} BspQ3Mesh;

typedef struct BspQ3Meshes {
	BspQ3Mesh *m[BSPQ3_MAX_MESHES];
	int n;
} BspQ3Meshes;

typedef struct CBspQ3 {
	char *name;			/* Cad */
	byte *lump;			/* the whole file while loading */
	BspQ3Header *head;
	int unused_0c;
	int nVerts;
	int unused_14[3];
	int nShaders;
	int nFaces;
	int unused_28[2];
	int nLMaps;
	int unused_34[3];
	BspQ3Vert *verts;		/* new[] */
	BspQ3Shader *shaders;		/* new[] */
	BspQ3LMap *lmaps;		/* new[] */
	int nPatches;
	BspQ3Face *faces;		/* new[] */
	BspQ3FaceRef *faceRefs;		/* new[] */
	int unused_58[2];
	CMeshD3D *MeshD3D;		/* all polygons */
	void *pMesh;			/* ID3DXMesh, drawn instead with the L key held */
	BspQ3Meshes *MeshList;
} CBspQ3;

extern byte g_KeyboardKeys[256];
#define g_KeyL g_KeyboardKeys[22]

#endif
