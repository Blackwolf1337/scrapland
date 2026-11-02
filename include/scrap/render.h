/*
 * Engine/render.cpp of the engine3d: the renderer over the D3D 8 device (g_pd3dDevice): the frame state (matrices,
 * viewport, frustum, render pass), the material and world matrix of each draw, pixel shaders, dynamic vertex and
 * index buffers, clears and the debug primitives. Decompiled in parts (src/game/engine3d/Engine/Render.c); the rest is
 * still the exe's, the rva of each of those is given.
 */
#ifndef SCRAP_RENDER_H
#define SCRAP_RENDER_H

#include "scrap/gvar.h"
#include "scrap/cstring.h"
#include "scrap/d3d8.h"
#include "scrap/mathlib.h"
#include "scrap/table.h"

extern cGVar GVar_R_LastDevice;
extern cGVar g_GvEnvBump;
extern cGVar g_GvEnvMap;

struct CMaterial;
typedef struct CMaterial CMaterial;
struct CVertList;

/* the D3D device of the engine3d, NULL without one */
extern void *g_pd3dDevice;
/* the identity matrix */
extern matrix_t g_MatIdentity;
/* the triangles and vertices drawn in the frame (statistics) */
extern int g_RenderCounts[10];
#define g_RenderTris g_RenderCounts[0]
#define g_RenderVerts g_RenderCounts[1]
/* nothing is drawn */
extern byte g_RenderOff;
/* the meshes are drawn from their vertex buffers */
extern byte g_UseVB;
extern int g_RenderPass;
/* the D3DRS_ZENABLE value of the frame and whether the fog is on */
extern uint32_t g_ZEnable;
extern int g_FogActive;
/* the D3DRS_AMBIENT of the scenes */
extern uint32_t g_AmbientD3D;
/* the viewport: its size, and all of it (x, y, w, h, minZ, maxZ) */
extern uint32_t g_Viewport[6];	/* X, Y, Width, Height, MinZ, MaxZ */
#define g_ViewportW ((int)g_Viewport[2])
#define g_ViewportH ((int)g_Viewport[3])
/* the planes of the view frustum (normal, d; outside where the distance is positive) */
#define MAX_PLANES_FRUSTUM 6
extern uint32_t g_FilterMip;
extern cGVar GVar_R_TexFilter;
#define g_TexFilter (GVar_R_TexFilter.value.i)		/* R_TexFilter 0 point, 1 linear, 2 anisotropic */
extern float g_MipmapBias2;
#define g_TexChanges g_RenderCounts[2]
extern byte g_TileOn;
#define g_MatChanges g_RenderCounts[3]
extern CMaterial *g_MatGround;
/* the material of a ModelFX in place of the ones of the meshes, NULL */
extern const CMaterial *g_MatForced;
/* the other overrides of ModelFX.cpp, cleared with it */
extern CMaterial *g_MatFX1;	/* in place of every material */
extern CMaterial *g_MatFX2;	/* in place of the lightmap material */
/* bUsarHWEnvMap: the environment maps through the texture transform of the device (materials with two texture sets
 * get VERT_TEX1 instead of VERT_TEX2) */
extern int g_UseEnvMap;		/* video UsarEnvMap */
extern int g_UseHWEnvMap;
extern int g_UseBumpMap;		/* video UsarBumpMap */
extern int g_UseGlowMap;		/* video UsarGlowMap */
#define g_EnvBump (g_GvEnvBump.value.i)		/* R_EnvBump */
#define g_EnvMap (g_GvEnvMap.value.i)		/* R_EnvMap */
/* the render functions of the shader modes (two tables, the second for the modes with 0x100) */
#define MAX_SHADER_TAB 32
typedef void (*shaderRender_t)(void *a, void *b);
typedef struct ShaderTab {
	uint32_t Index;			/* 0x00 its own index */
	const char *Name;		/* 0x04 */
	const char *File;		/* 0x08 the .psh, NULL without a pixel shader */
	int unused_0c;			/* 0x0c */
	uint32_t flags;			/* 0x10 0x100, 0x101 */
	shaderRender_t fnRender;	/* 0x14 */
} ShaderTab;
extern ShaderTab g_ShaderTab[32];	/* [32], ends with a NULL name */
/* the pixel shaders: on (the device has them), their handles, the ones switched off (a bit each from 0x200000) */
#define MAX_PS 9
extern cGVar GVar_R_PShaders;
#define g_PSOn (GVar_R_PShaders.value.i)
#define PS_POST 21				/* the first of the post effect shaders */
/* the pixel shaders of g_PSNames the shader modes use */
enum {
	PS_GLOWMAPMASKENVBUMP = 2,
	PS_MASKENVBUMP = 3,
	PS_MASKENVMAP = 4,
	PS_GLOWMAPMASKENVMAP = 5,
	PS_GLOWMAPMASKENVMAP_LM = 6,
	PS_MASKENVBUMP_LM = 7,
	PS_MASKENVMAP_LM = 8,
	PS_CLOUDS = 9,
	PS_ENVMAP = 10,
	PS_ENVMAP_LM = 11,
	PS_GLOWMAP = 12,
	PS_GLOWMAP_LM = 13,
	PS_HOLOGRAM = 14,
	PS_GLOWFLICK_LM = 15,
	PS_ELECTRIC = 16,
	PS_FIRE = 17,
	PS_GLASS = 18,
	PS_WAVES = 19,
	PS_NOISE = 20,
};
/* the whole viewport (g_Viewport is the one of the current view) */
extern uint32_t g_ViewportFull[6];
extern byte g_D3DOn;
/* what CNode3D_Render draws: the object classes (low 24 bits of the mask), the nodes whose flags & g_RenderFlagMask
 * is not 0 when g_RenderFlagNeg */
extern uint32_t g_RenderClassMask;
extern uint32_t g_RenderFlagMask;
extern byte g_RenderFlagNeg;	/* the last one Render_SetMaterial set */
#define g_DriverName (((cString *)&GVar_R_LastDevice.value))
extern byte g_TexNoPow2;	/* D3DPTEXTURECAPS_NONPOW2CONDITIONAL */
extern byte g_TexPow2;	/* D3DPTEXTURECAPS_POW2 */
extern byte g_TexRepeat;	/* D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE */

/* a dynamic index buffer of triangles (16 bit indices) */
typedef struct CRenderIB {
	void *pVertIB;			/* 0x0 IDirect3DIndexBuffer8 */
	int Total;			/* 0x4 triangles */
	int Used;			/* 0x8 */
} CRenderIB;

/* a dynamic vertex buffer */
typedef struct CRenderVB {
	void *pVertVB;			/* 0x00 IDirect3DVertexBuffer8 */
	int Total;			/* 0x04 vertices */
	int Used;			/* 0x08 */
	int Size;			/* 0x0c of a vertex */
	uint32_t FVF;			/* 0x10 */
} CRenderVB;

/* pixel shader i on (flags 1) or off (flags 2), the fixed texture stages (Render_PSFixed) without it; arg is not
 * used */
void Render_PS0(void *arg, byte flags);
void Render_PS1(void *arg, byte flags);
void Render_PS2(void *arg, byte flags);
void Render_PS3(void *arg, byte flags);
void Render_PS4(void *arg, byte flags);
void Render_PS5(void *arg, byte flags);
void Render_PS6(void *arg, byte flags);
void Render_PS7(void *arg, byte flags);
void Render_PS8(void *arg, byte flags);
/* stage 0 texture times diffuse (four times with DiffuseMod4x when on) */
void Render_PSFixed(void *arg, byte flags);
/* the render functions of the shader modes, g_ShaderTab and the lightmap ones of g_ShaderTab2 (flags: 1 begin, 4
 * the next material of the mode, 2 end) */
void Shader_DiffuseNoLit(CMaterial *mat, uint32_t flags);
void Shader_Envmap(CMaterial *mat, uint32_t flags);
void Shader_MaskEnvmap(CMaterial *mat, uint32_t flags);
void Shader_MaskEnvBump(CMaterial *mat, uint32_t flags);
void Shader_Glowmap(CMaterial *mat, uint32_t flags);		/* also GlowFlick */
void Shader_GlowmapMaskEnvmap(CMaterial *mat, uint32_t flags);
void Shader_GlowmapMaskEnvBump(CMaterial *mat, uint32_t flags);
void Shader_Glass(CMaterial *mat, uint32_t flags);
void Shader_Scroll(CMaterial *mat, uint32_t flags);
void Shader_Clouds(CMaterial *mat, uint32_t flags);
void Shader_Hologram(CMaterial *mat, uint32_t flags);
void Shader_Electric(CMaterial *mat, uint32_t flags);
void Shader_Fire(CMaterial *mat, uint32_t flags);
void Shader_Waves(CMaterial *mat, uint32_t flags);
void Shader_Noise(CMaterial *mat, uint32_t flags);
void CRenderIB_Reset(CRenderIB *self);
void CRenderVB_Reset(CRenderVB *self);
void CRenderIB_Add(CRenderIB *self, int numtri, const uint16_t *idx);
void CRenderVB_Add(CRenderVB *self, int numvert, const void *verts);
/* the buffer (AddRef) and the used count */
void *CRenderIB_Get(CRenderIB *self, int *used);
void *CRenderVB_Get(CRenderVB *self, int *used);
void CRenderIB_Create(CRenderIB *self, uint32_t numtri);
void CRenderVB_Create(CRenderVB *self, uint32_t numvert, int size, uint32_t fvf);
/* the batches drawn first; rect (x0, y0, x1, y1 in the viewport) NULL for all; D3DCLEAR_ZBUFFER also clears the
 * stencil of a device that has one; whether the device cleared */
byte Render_Clear(const box_t *rect, uint32_t flags, uint32_t color, float z, uint32_t stencil);
/* a textured rectangle of the viewport, its left side in cLeft, its right side in cRight */
void Render_DrawTexRect(const box_t *rect, uint32_t cLeft, uint32_t cRight, void *tex);
/* the end of the D3D device: textures and pixel shaders off, the shaders deleted, the device objects of the modules
 * released */
void Render_EndD3D(void);
/* the end of a view: the batches drawn, the default material, the whole viewport */
void Render_EndView(void);
/* the batches drawn, the materials of ModelFX cleared */
void Render_ClearForced(void);
const char *Render_GetDriver(void);
/* the start of the D3D device: caps, the log of the device, base stage states, the pixel shaders from their
 * files, the device objects of the modules, the default materials */
void Render_IniD3D(void);
/* the start of a frame: the device of the application, the whole viewport, the base states */
void Render_BeginFrame(void);
/* the triangles of idx over the vertices of v (counted with stats) */
int Render_DrawListIdx(const struct CVertList *v, const uint16_t *idx, int ntri, byte stats);
int Render_SetWorldD3D(const matrix_t *m);
void Render_WorldDirty(void);
/* the world transform of the device (with defer only remembered); 0 */
int Render_SetWorldMatrix(const matrix_t *m, byte defer);
/* the world matrix given to the device once (HRESULT) */
int Render_SetWorld(void);
/* the base texture stage states and the filters of the variables */
void Render_ResetState(void);
/* the device states and texture of a material for pass 0 (the material) or 1 (its second texture set added); NULL
 * with pass -1 forgets the last material; HRESULT */
int Render_SetMaterial(void *mat, int pass);
/* D3DCULL_CCW is none with R_CullMode */
void Render_SetCull(uint32_t mode);
/* the lightmap material (counted) and its modulation (2 2x, 4 4x) */
void Render_SetLightmapMat(CMaterial *mat, int mode);
/* the texture of map (NULL none) on stage: its frame, mipmaps, wrap and anisotropy */
struct CMap;
int Render_SetMap(uint32_t stage, struct CMap *map);
/* ntri triangles from startTri of ib over numVerts vertices of vb in format frmt from baseVert */
int Render_DrawVBIBFmt(void *vb, void *ib, int frmt, uint32_t numVerts, uint32_t ntri, uint32_t baseVert,
	uint32_t startTri);
struct CMeshD3D;
int Render_DrawMeshD3D(const struct CMeshD3D *m);
int Render_DrawVBIB(void *vb, void *ib, const struct CVertList *v, uint32_t ntri);
/* a mesh with its material (and the second pass of its second texture set), the skinned and stencil shadow ones
 * through their modules; r is the render node, NULL */
struct CRenderNode;
int Render_MeshD3D(struct CMeshD3D *m, struct CRenderNode *r);
int Render_GetWorld(matrix_t *out);

const matrix_t *Render_GetProj(void);
const matrix_t *Render_GetView(void);
const matrix_t *Render_GetBillboard(void);
const vec3_t *Render_GetEyePos(void);

/* a line in the current world matrix (the line material); an error without a device */
int Render_Line(const vec3_t *a, const vec3_t *b, uint32_t color);
/* a line in pixels of the viewport (pretransformed, the line material); an error without a device */
int Render_Line2D(float x0, float y0, float x1, float y1, uint32_t color);
/* the viewport size, CHECK that it is set */
int Render_GetViewportW(void);
int Render_GetViewportH(void);
/* the pixels of rect (0..1 of the viewport vp) as a D3DRECT, clamped; 0 (a point) when nothing is left */
byte Render_ClipRect(int *out, const uint32_t *vp, const box_t *rect);
/* the stencil bits of the depth buffer */
int Render_StencilBits(void);
/* the vertex buffer of a vertex list and the index buffer of triangles, in system memory with dynamic (HRESULT) */
int Render_CreateVB(void **vb, struct CVertList *vert, byte dynamic);
int Render_CreateIB(void **ib, const void *tris, int nTri, byte dynamic);
/* the batches waiting for the view drawn */
void E3D_FlushBatch(void);
/* the box in the space of m is in the view frustum (as the sphere around it, scale times its radius); with
 * R_ShowSphereVis drawn */
byte Box_IsVisible(const matrix_t *m, const box_t *box, const char *name, float scale);
byte Frustum_SegVisible(const vec3_t *a, const vec3_t *b);
/* arg is not used */
byte Sph_IsVisible(const vec3_t *center, float radius, int arg);
/* the view and projection of the camera, the frustum of them */
struct CCamera;
void Render_SetCamera(struct CCamera *cam);
/* the viewport narrowed to rect and the projection to it (with proj); NULL the whole view again; 1 */
byte Render_SetViewRect(const box_t *rect, byte proj);
/* the screen position (pixels) of a world position; 0, -1 when it is not on the screen */
int Render_ToScreen(const vec3_t *pos, float *xy);

/* Engine/RenderEVA.cpp (src/game/engine3d/Engine/RenderEVA.c): the meshes with a vertex animation (EVA) or a skin,
 * moved in software into a dynamic vertex buffer of 0x1000 vertices (VERT_TEX1), their shadow positions into the
 * main mesh of the node; and the particles, quads in a dynamic vertex buffer of 0x800 vertices drawn through a fixed
 * index buffer of 0x200 quads */
#define MAX_VERT_VB 0x1000
#define VB_PARTICLES 0x200
/* the particles drawn in the frame (statistics) */
#define g_RenderParts g_RenderCounts[6]

struct CParticle;
struct Graphic3DVert;
/* both vertex buffers and the index buffer of the particles made (EVA_crear_VB); HRESULT */
int RenderEVA_InitD3D(void);
int RenderEVA_ReleaseD3D(void);
/* the mesh with the vertex animation skin (a CEVA) at the blend of the animation slot of r; HRESULT */
int RenderEVA_DrawSkinNode(struct CMeshD3D *m, struct CRenderNode *r, void *skin);
/* the mesh with its skin over the render nodes of the model; HRESULT */
int RenderEVA_DrawSkin(struct CMeshD3D *m, struct CRenderNode *r);

/* Engine/RenderMallaD3D.cpp, RenderMallaD3D_transp.cpp and RenderObjBase3D.cpp
 * (src/game/engine3d/Engine/RenderMeshD3D.c): the batches of a view. Opaque meshes (R_MeshCache) are sorted by
 * texture, material and skin, transparent ones and the particle systems and flares far to near, each drawn with the
 * world matrix it was queued with. Up to 0x400 of each, a full batch is drawn at once. */
#define MAX_RENDER_BATCH 0x400
typedef struct RenderMeshEntry {
	struct CMeshD3D *mesh;	/* 0x00 */
	matrix_t world;			/* 0x04 */
	uint32_t tex;			/* 0x44 the diffuse texture >> 1, 0 without */
	int list;			/* 0x48 the batch count when the list of the mesh was queued */
	struct CRenderNode *r;		/* 0x4c */
} RenderMeshEntry;
typedef struct RenderTranspEntry {
	struct CMeshD3D *mesh;	/* 0x00 */
	matrix_t world;			/* 0x04 */
	int group;			/* 0x44 g_TranspGroup, 0 for the identity */
	struct CRenderNode *r;		/* 0x48 */
	float dist2;			/* 0x4c to the eye, squared */
} RenderTranspEntry;
typedef struct RenderObjEntry {
	struct IObject3D *obj;		/* 0x00 a CParticleSystem or a CFlare */
	matrix_t world;			/* 0x04 */
	struct CRenderNode *r;		/* 0x44 */
	float dist2;			/* 0x48 */
} RenderObjEntry;

/* the batches drawn; E_FAIL without a device, else 0 */
int RenderMeshD3D_Flush(void);
int RenderMeshD3D_Flush2(void);
int RenderObjBase3D_Flush(void);
/* a transparent mesh queued (cont: the same list as the one before, else a new group); HRESULT */
int RenderMeshD3D_AddTransp(struct CMeshD3D *m, const matrix_t *world, byte cont, struct CRenderNode *r);

/* Engine/RenderLock.cpp (src/game/engine3d/Engine/RenderLock.c) */
void RenderLock_ReleaseD3D(void);
void RenderLock_InitD3D(void);
/* the lock of the frame of the mode RenderLock */
void RenderLock_Frame(void);

/* Engine/RenderGamma.cpp (src/game/engine3d/Engine/RenderGamma.c): the gamma ramp of the device from the engine
 * variables, set again when one changed */
void RenderGamma_ReleaseD3D(void);
void RenderGamma_Begin(void);
/* Engine/RenderShader.cpp: the shader modes of the materials */
void RenderGamma_LogShaderMask(void);		/* the mask of the shader features on, logged when it changes */

/* still the exe's */

/* other modules (src/imports.inc) */
/* rva 0x267530 (stencil.cpp): the shadow volume of the triangles of a mesh */
void Stencil_Mesh(struct CVertList *v, const void *tris, uint32_t nTri, struct CMeshD3D *m, struct CRenderNode *r);
/* CVertList_EnvMapUV: vertlist.h */
void VertList_InitD3D(void);			/* rva 0x2404f0, empty */
void RenderTarget_InitD3D(void);
void VertList_ReleaseD3D(void);		/* rva 0x2404e0 (ListaVert.cpp), empty */
void RenderTarget_ReleaseD3D(void);		/* rva 0x220c60 (RenderTarget.cpp) */

#endif
