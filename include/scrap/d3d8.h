/*
 * The parts of Direct3D 8 the engine3d uses: COM methods called by their vtable offset (stdcall, the object first)
 * and the constants of d3d8types.h.
 */
#ifndef SCRAP_D3D8_H
#define SCRAP_D3D8_H

#include "scrap/game.h"

#define D3D_METHOD(obj, type, off) ((type)(*(void ***)(obj))[(off) / 4])

/* IUnknown */
typedef uint32_t(__stdcall *d3dRelease_t)(void *obj);
#define COM_RELEASE(obj) D3D_METHOD(obj, d3dRelease_t, 0x8)(obj)

/* IDirect3DDevice8 */
#define D3DDEV_CREATETEXTURE 0x50
#define D3DDEV_CREATEVB 0x5c
#define D3DDEV_CREATEIB 0x60
#define D3DDEV_CREATEIMAGESURFACE 0x6c
#define D3DDEV_CLEAR 0x90
#define D3DDEV_SETTRANSFORM 0x94
#define D3DDEV_GETTRANSFORM 0x98
#define D3DDEV_GETVIEWPORT 0xa4
#define D3DDEV_SETMATERIAL 0xa8
#define D3DDEV_SETLIGHT 0xb0
#define D3DDEV_LIGHTENABLE 0xb8
#define D3DDEV_SETRENDERSTATE 0xc8
#define D3DDEV_SETTEXTURESTAGESTATE 0xfc
#define D3DDEV_DRAWINDEXEDPRIMITIVE 0x11c
#define D3DDEV_DRAWPRIMITIVEUP 0x120
#define D3DDEV_SETVERTEXSHADER 0x130	/* an FVF for the fixed pipeline */
#define D3DDEV_SETSTREAMSOURCE 0x14c
#define D3DDEV_SETINDICES 0x154
#define D3DDEV_SETPIXELSHADER 0x160
#define D3DDEV_SETPIXELSHADERCONSTANT 0x16c
typedef int(__stdcall *d3dCreateVB_t)(void *dev, uint32_t len, uint32_t usage, uint32_t fvf, int pool, void **vb);
typedef int(__stdcall *d3dCreateIB_t)(void *dev, uint32_t len, uint32_t usage, int fmt, int pool, void **ib);
typedef int(__stdcall *d3dSetTransform_t)(void *dev, uint32_t state, const void *m);
typedef int(__stdcall *d3dGetTransform_t)(void *dev, uint32_t state, void *m);
typedef int(__stdcall *d3dSetMaterial_t)(void *dev, const void *mat);
typedef int(__stdcall *d3dSetPixelShaderConstant_t)(void *dev, uint32_t reg, const void *data, uint32_t count);
typedef int(__stdcall *d3dGetViewport_t)(void *dev, uint32_t *vp);
typedef int(__stdcall *d3dSetRenderState_t)(void *dev, uint32_t state, uint32_t value);
typedef int(__stdcall *d3dSetTextureStageState_t)(void *dev, uint32_t stage, uint32_t type, uint32_t value);
typedef int(__stdcall *d3dSetPixelShader_t)(void *dev, uint32_t handle);
typedef int(__stdcall *d3dClear_t)(void *dev, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z,
	uint32_t stencil);
typedef int(__stdcall *d3dDrawIndexedPrimitive_t)(void *dev, int type, uint32_t minIndex, uint32_t numVertices,
	uint32_t startIndex, uint32_t primCount);
typedef int(__stdcall *d3dDrawPrimitiveUP_t)(void *dev, int type, uint32_t count, const void *data, uint32_t stride);
typedef int(__stdcall *d3dSetVertexShader_t)(void *dev, uint32_t fvf);
typedef int(__stdcall *d3dSetStreamSource_t)(void *dev, uint32_t stream, void *vb, uint32_t stride);
typedef int(__stdcall *d3dSetIndices_t)(void *dev, void *ib, uint32_t baseVertex);

/* IDirect3DVertexBuffer8, IDirect3DIndexBuffer8 */
#define D3DBUF_LOCK 0x2c
#define D3DBUF_UNLOCK 0x30
typedef int(__stdcall *d3dLock_t)(void *buf, uint32_t off, uint32_t size, void *data, uint32_t flags);
typedef int(__stdcall *d3dUnlock_t)(void *buf);
typedef uint32_t(__stdcall *d3dAddRef_t)(void *obj);
#define COM_ADDREF(obj) D3D_METHOD(obj, d3dAddRef_t, 0x4)(obj)

#define D3DPT_TRIANGLELIST 4
#define D3DPT_TRIANGLESTRIP 5
#define D3DTS_VIEW 2
#define D3DTS_PROJECTION 3
#define D3DTS_TEXTURE0 16
#define D3DRS_ZENABLE 7
#define D3DRS_CULLMODE 22
#define D3DRS_AMBIENT 139
#define D3DCULL_NONE 1
#define D3DCULL_CCW 3
#define D3DCLEAR_ZBUFFER 2
#define D3DCLEAR_STENCIL 4
#define D3DTSS_COLOROP 1
#define D3DTSS_COLORARG1 2
#define D3DTSS_COLORARG2 3
#define D3DTSS_BUMPENVMAT00 7
#define D3DTSS_BUMPENVMAT01 8
#define D3DTSS_BUMPENVMAT10 9
#define D3DTSS_BUMPENVMAT11 10
#define D3DTSS_TEXCOORDINDEX 11
#define D3DTSS_TEXTURETRANSFORMFLAGS 24
#define D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR 0x30000
#define D3DTTFF_DISABLE 0
#define D3DTTFF_COUNT2 2
#define D3DTA_DIFFUSE 0
#define D3DTA_TEXTURE 2
#define D3DTOP_MODULATE 4
#define D3DTOP_MODULATE4X 6
#define D3DUSAGE_WRITEONLY 8
#define D3DPOOL_DEFAULT 0
#define D3DPOOL_MANAGED 1
#define D3DUSAGE_WRITEONLY_DYNAMIC 0x208
#define D3DFMT_INDEX16 101
#define D3DLOCK_NOOVERWRITE 0x1000
#define D3DLOCK_DISCARD 0x2000

#endif
