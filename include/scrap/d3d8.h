/*
 * The parts of Direct3D 8 the engine3d uses: COM methods called by their vtable offset (stdcall, the object first)
 * and the constants of d3d8types.h.
 */
#ifndef SCRAP_D3D8_H
#define SCRAP_D3D8_H

#include "scrap/mathlib.h"

#ifndef E_FAIL
#define E_FAIL ((int)0x80004005)
#endif

#define D3D_METHOD(obj, type, off) ((type)(*(void ***)(obj))[(off) / 4])

/* IUnknown */
typedef uint32_t(__stdcall *d3dRelease_t)(void *obj);
#define COM_RELEASE(obj) D3D_METHOD(obj, d3dRelease_t, 0x8)(obj)

/* IDirect3DDevice8 */
#define D3DDEV_CREATETEXTURE 0x50
#define D3DDEV_CREATEVB 0x5c
#define D3DDEV_CREATEIB 0x60
#define D3DDEV_CREATEIMAGESURFACE 0x6c
#define D3DDEV_GETBACKBUFFER 0x40
#define D3DDEV_SETGAMMARAMP 0x48
#define D3DDEV_COPYRECTS 0x70
#define D3DDEV_SETRENDERTARGET 0x7c
#define D3DDEV_GETRENDERTARGET 0x80
#define D3DDEV_GETDEPTHSTENCILSURFACE 0x84
#define D3DDEV_CLEAR 0x90
#define D3DDEV_SETTRANSFORM 0x94
#define D3DDEV_GETTRANSFORM 0x98
#define D3DDEV_SETVIEWPORT 0xa0
#define D3DDEV_GETVIEWPORT 0xa4
#define D3DDEV_SETMATERIAL 0xa8
#define D3DDEV_SETLIGHT 0xb0
#define D3DDEV_GETLIGHT 0xb4
#define D3DDEV_LIGHTENABLE 0xb8
#define D3DDEV_GETLIGHTENABLE 0xbc
#define D3DDEV_SETRENDERSTATE 0xc8
#define D3DDEV_GETRENDERSTATE 0xcc
#define D3DDEV_SETTEXTURE 0xf4
#define D3DDEV_GETTEXTURESTAGESTATE 0xf8
#define D3DDEV_SETTEXTURESTAGESTATE 0xfc
#define D3DDEV_DRAWPRIMITIVE 0x118
#define D3DDEV_DRAWINDEXEDPRIMITIVE 0x11c
#define D3DDEV_DRAWPRIMITIVEUP 0x120
#define D3DDEV_DRAWINDEXEDPRIMITIVEUP 0x124
#define D3DDEV_SETVERTEXSHADER 0x130	/* an FVF for the fixed pipeline */
#define D3DDEV_SETSTREAMSOURCE 0x14c
#define D3DDEV_SETINDICES 0x154
#define D3DDEV_CREATEPIXELSHADER 0x15c
#define D3DDEV_SETPIXELSHADER 0x160
#define D3DDEV_DELETEPIXELSHADER 0x168
#define D3DDEV_SETPIXELSHADERCONSTANT 0x16c
typedef int(__stdcall *d3dCreateVB_t)(void *dev, uint32_t len, uint32_t usage, uint32_t fvf, int pool, void **vb);
typedef int(__stdcall *d3dCreateIB_t)(void *dev, uint32_t len, uint32_t usage, int fmt, int pool, void **ib);
typedef int(__stdcall *d3dSetTransform_t)(void *dev, uint32_t state, const void *m);
typedef int(__stdcall *d3dGetTransform_t)(void *dev, uint32_t state, void *m);
typedef int(__stdcall *d3dSetMaterial_t)(void *dev, const void *mat);
typedef int(__stdcall *d3dGetLight_t)(void *dev, uint32_t i, void *light);
typedef int(__stdcall *d3dGetLightEnable_t)(void *dev, uint32_t i, int *on);
typedef int(__stdcall *d3dSetPixelShaderConstant_t)(void *dev, uint32_t reg, const void *data, uint32_t count);
typedef int(__stdcall *d3dGetViewport_t)(void *dev, uint32_t *vp);
typedef int(__stdcall *d3dSetViewport_t)(void *dev, const uint32_t *vp);
typedef int(__stdcall *d3dSetTexture_t)(void *dev, uint32_t stage, void *tex);
typedef int(__stdcall *d3dGetTextureStageState_t)(void *dev, uint32_t stage, uint32_t type, uint32_t *value);
typedef int(__stdcall *d3dDeletePixelShader_t)(void *dev, uint32_t handle);
typedef int(__stdcall *d3dDrawIndexedPrimitiveUP_t)(void *dev, int type, uint32_t minIndex, uint32_t numVertices,
	uint32_t primCount, const void *idx, int idxFmt, const void *data, uint32_t stride);
typedef int(__stdcall *d3dSetRenderState_t)(void *dev, uint32_t state, uint32_t value);
typedef int(__stdcall *d3dGetRenderState_t)(void *dev, uint32_t state, uint32_t *value);
typedef int(__stdcall *d3dSetGammaRamp_t)(void *dev, uint32_t flags, const void *ramp);
typedef int(__stdcall *d3dCopyRects_t)(void *dev, void *src, const void *rects, uint32_t n, void *dst,
	const void *points);
typedef int(__stdcall *d3dSetRenderTarget_t)(void *dev, void *rt, void *zs);
typedef int(__stdcall *d3dGetSurface_t)(void *dev, void **surf);
typedef int(__stdcall *d3dGetBackBuffer_t)(void *dev, uint32_t i, int type, void **surf);
typedef int(__stdcall *d3dCreateTexture_t)(void *dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage,
	int fmt, int pool, void **tex);
typedef int(__stdcall *d3dSetTextureStageState_t)(void *dev, uint32_t stage, uint32_t type, uint32_t value);
typedef int(__stdcall *d3dSetPixelShader_t)(void *dev, uint32_t handle);
typedef int(__stdcall *d3dCreatePixelShader_t)(void *dev, const void *code, uint32_t *handle);
typedef int(__stdcall *d3dClear_t)(void *dev, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z,
	uint32_t stencil);
typedef int(__stdcall *d3dDrawPrimitive_t)(void *dev, int type, uint32_t start, uint32_t count);
typedef int(__stdcall *d3dDrawIndexedPrimitive_t)(void *dev, int type, uint32_t minIndex, uint32_t numVertices,
	uint32_t startIndex, uint32_t primCount);
typedef int(__stdcall *d3dDrawPrimitiveUP_t)(void *dev, int type, uint32_t count, const void *data, uint32_t stride);
typedef int(__stdcall *d3dSetVertexShader_t)(void *dev, uint32_t fvf);
typedef int(__stdcall *d3dSetStreamSource_t)(void *dev, uint32_t stream, void *vb, uint32_t stride);
typedef int(__stdcall *d3dSetIndices_t)(void *dev, void *ib, uint32_t baseVertex);

/* Direct3D 8: D3DSURFACE_DESC and D3DLOCKED_RECT */
typedef struct {
	int Format;			/* 0x00 */
	int Type;			/* 0x04 */
	uint32_t Usage;			/* 0x08 */
	int Pool;			/* 0x0c */
	uint32_t Size;			/* 0x10 */
	int MultiSampleType;		/* 0x14 */
	uint32_t Width;			/* 0x18 */
	uint32_t Height;		/* 0x1c */
} d3dSurfaceDesc_t;
SD_SIZE(d3dSurfaceDesc_t, 0x20);

typedef struct {
	int Pitch;			/* 0x0 */
	void *pBits;			/* 0x4 */
} d3dLockedRect_t;

/* IDirect3DTexture8 */
#define D3DTEX_GETLEVELDESC 0x38
#define D3DTEX_GETSURFACELEVEL 0x3c
#define D3DTEX_LOCKRECT 0x40
#define D3DTEX_UNLOCKRECT 0x44
typedef int(__stdcall *d3dGetLevelDesc_t)(void *tex, uint32_t level, d3dSurfaceDesc_t *desc);
typedef int(__stdcall *d3dGetSurfaceLevel_t)(void *tex, uint32_t level, void **surf);
typedef int(__stdcall *d3dLockRect_t)(void *tex, uint32_t level, void *locked, const void *rect, uint32_t flags);
typedef int(__stdcall *d3dUnlockRect_t)(void *tex, uint32_t level);

/* IDirect3DVertexBuffer8, IDirect3DIndexBuffer8 */
#define D3DBUF_LOCK 0x2c
#define D3DBUF_UNLOCK 0x30
typedef int(__stdcall *d3dLock_t)(void *buf, uint32_t off, uint32_t size, void *data, uint32_t flags);
typedef int(__stdcall *d3dUnlock_t)(void *buf);
typedef uint32_t(__stdcall *d3dAddRef_t)(void *obj);
#define COM_ADDREF(obj) D3D_METHOD(obj, d3dAddRef_t, 0x4)(obj)

#define D3DPT_LINELIST 2
#define D3DPT_TRIANGLELIST 4
#define D3DPT_TRIANGLESTRIP 5
#define D3DTS_VIEW 2
#define D3DTS_PROJECTION 3
#define D3DTS_TEXTURE0 16
#define D3DTS_WORLD 256
#define D3DRS_ZENABLE 7
#define D3DRS_FILLMODE 8
#define D3DFILL_WIREFRAME 2
#define D3DRS_STENCILENABLE 52
#define D3DRS_STENCILFAIL 53
#define D3DRS_STENCILZFAIL 54
#define D3DRS_STENCILPASS 55
#define D3DRS_STENCILFUNC 56
#define D3DRS_STENCILREF 57
#define D3DRS_STENCILMASK 58
#define D3DRS_STENCILWRITEMASK 59
#define D3DSTENCILOP_KEEP 1
#define D3DSTENCILOP_INCR 7
#define D3DCMP_LESSEQUAL 4
#define D3DCMP_NOTEQUAL 6
#define D3DCMP_ALWAYS 8
#define D3DRS_ZWRITEENABLE 14
#define D3DRS_ALPHATESTENABLE 15
#define D3DRS_SRCBLEND 19
#define D3DRS_DESTBLEND 20
#define D3DRS_ZFUNC 23
#define D3DRS_ALPHAREF 24
#define D3DRS_ALPHAFUNC 25
#define D3DRS_ALPHABLENDENABLE 27
#define D3DRS_ZBIAS 47
#define D3DRS_TEXTUREFACTOR 60
#define D3DRS_LIGHTING 137
#define D3DRS_INDEXEDVERTEXBLENDENABLE 151
#define D3DRS_COLORWRITEENABLE 168
#define D3DCMP_GREATEREQUAL 7
#define D3DRS_CULLMODE 22
#define D3DRS_FOGENABLE 28
#define D3DRS_AMBIENT 139
#define D3DCULL_NONE 1
#define D3DCULL_CCW 3
#define D3DCLEAR_TARGET 1
#define D3DCLEAR_ZBUFFER 2
#define D3DCLEAR_STENCIL 4
#define D3DTSS_COLOROP 1
#define D3DTSS_COLORARG1 2
#define D3DTSS_COLORARG2 3
#define D3DTSS_ALPHAOP 4
#define D3DTSS_ALPHAARG1 5
#define D3DTSS_ALPHAARG2 6
#define D3DTSS_BUMPENVMAT00 7
#define D3DTSS_BUMPENVMAT01 8
#define D3DTSS_BUMPENVMAT10 9
#define D3DTSS_BUMPENVMAT11 10
#define D3DTSS_TEXCOORDINDEX 11
#define D3DTSS_ADDRESSU 13
#define D3DTSS_ADDRESSV 14
#define D3DTSS_MAGFILTER 16
#define D3DTSS_MINFILTER 17
#define D3DTSS_MIPFILTER 18
#define D3DTSS_MIPMAPLODBIAS 19
#define D3DTSS_MAXANISOTROPY 21
#define D3DTADDRESS_WRAP 1
#define D3DTADDRESS_CLAMP 3
#define D3DTSS_TEXTURETRANSFORMFLAGS 24
#define D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR 0x30000
#define D3DTTFF_DISABLE 0
#define D3DTTFF_COUNT2 2
#define D3DTA_DIFFUSE 0
#define D3DTA_CURRENT 1
#define D3DTA_TEXTURE 2
#define D3DTA_TFACTOR 3
#define D3DTOP_DISABLE 1
#define D3DTOP_SELECTARG1 2
#define D3DTOP_SELECTARG2 3
#define D3DTOP_MODULATE 4
#define D3DTOP_MODULATE2X 5
#define D3DTOP_MODULATE4X 6
#define D3DTOP_ADD 7
#define D3DTOP_BLENDCURRENTALPHA 16
#define D3DTOP_MODULATEALPHA_ADDCOLOR 18
#define D3DTOP_BUMPENVMAP 22
#define D3DUSAGE_RENDERTARGET 1
#define D3DUSAGE_WRITEONLY 8
#define D3DPOOL_DEFAULT 0
#define D3DPOOL_SYSTEMMEM 2
#define D3DPOOL_MANAGED 1
#define D3DUSAGE_WRITEONLY_DYNAMIC 0x208
#define D3DFMT_X8R8G8B8 22
#define D3DFMT_INDEX16 101
#define D3DFMT_D15S1 73
#define D3DFMT_D24S8 75
#define D3DFMT_D24X4S4 79
#define D3DFVF_XYZ 0x2
#define D3DFVF_XYZRHW 0x4
#define D3DFVF_DIFFUSE 0x40
#define D3DLOCK_NOOVERWRITE 0x1000
#define D3DLOCK_DISCARD 0x2000

/* D3DCAPS8 */
typedef struct D3DCaps8 {
	uint32_t DeviceType;			/* 0x00 */
	uint32_t AdapterOrdinal;		/* 0x04 */
	uint32_t Caps;				/* 0x08 */
	uint32_t Caps2;				/* 0x0c */
	uint32_t Caps3;				/* 0x10 */
	uint32_t PresentationIntervals;		/* 0x14 */
	uint32_t CursorCaps;			/* 0x18 */
	uint32_t DevCaps;			/* 0x1c */
	uint32_t PrimitiveMiscCaps;		/* 0x20 */
	uint32_t RasterCaps;			/* 0x24 */
	uint32_t ZCmpCaps;			/* 0x28 */
	uint32_t SrcBlendCaps;			/* 0x2c */
	uint32_t DestBlendCaps;			/* 0x30 */
	uint32_t AlphaCmpCaps;			/* 0x34 */
	uint32_t ShadeCaps;			/* 0x38 */
	uint32_t TextureCaps;			/* 0x3c */
	uint32_t TextureFilterCaps;		/* 0x40 */
	uint32_t CubeTextureFilterCaps;		/* 0x44 */
	uint32_t VolumeTextureFilterCaps;	/* 0x48 */
	uint32_t TextureAddressCaps;		/* 0x4c */
	uint32_t VolumeTextureAddressCaps;	/* 0x50 */
	uint32_t LineCaps;			/* 0x54 */
	uint32_t MaxTextureWidth;		/* 0x58 */
	uint32_t MaxTextureHeight;		/* 0x5c */
	uint32_t MaxVolumeExtent;		/* 0x60 */
	uint32_t MaxTextureRepeat;		/* 0x64 */
	uint32_t MaxTextureAspectRatio;		/* 0x68 */
	uint32_t MaxAnisotropy;			/* 0x6c */
	float MaxVertexW;			/* 0x70 */
	float GuardBandLeft;			/* 0x74 */
	float GuardBandTop;			/* 0x78 */
	float GuardBandRight;			/* 0x7c */
	float GuardBandBottom;			/* 0x80 */
	float ExtentsAdjust;			/* 0x84 */
	uint32_t StencilCaps;			/* 0x88 */
	uint32_t FVFCaps;			/* 0x8c */
	uint32_t TextureOpCaps;			/* 0x90 */
	uint32_t MaxTextureBlendStages;		/* 0x94 */
	uint32_t MaxSimultaneousTextures;	/* 0x98 */
	uint32_t VertexProcessingCaps;		/* 0x9c */
	uint32_t MaxActiveLights;		/* 0xa0 */
	uint32_t MaxUserClipPlanes;		/* 0xa4 */
	uint32_t MaxVertexBlendMatrices;	/* 0xa8 */
	uint32_t MaxVertexBlendMatrixIndex;	/* 0xac */
	float MaxPointSize;			/* 0xb0 */
	uint32_t MaxPrimitiveCount;		/* 0xb4 */
	uint32_t MaxVertexIndex;		/* 0xb8 */
	uint32_t MaxStreams;			/* 0xbc */
	uint32_t MaxStreamStride;		/* 0xc0 */
	uint32_t VertexShaderVersion;		/* 0xc4 */
	uint32_t MaxVertexShaderConst;		/* 0xc8 */
	uint32_t PixelShaderVersion;		/* 0xcc */
	float MaxPixelShaderValue;		/* 0xd0 */
} D3DCaps8;
SD_OFFSET(D3DCaps8, RasterCaps, 0x24);
SD_OFFSET(D3DCaps8, MaxAnisotropy, 0x6c);
SD_OFFSET(D3DCaps8, PixelShaderVersion, 0xcc);
SD_SIZE(D3DCaps8, 0xd4);

/* D3DDISPLAYMODE */
typedef struct D3DDisplayMode {
	uint32_t Width;			/* 0x0 */
	uint32_t Height;		/* 0x4 */
	uint32_t RefreshRate;		/* 0x8 */
	uint32_t Format;		/* 0xc */
} D3DDisplayMode;
SD_SIZE(D3DDisplayMode, 0x10);

/* D3DPRESENT_PARAMETERS */
typedef struct D3DPresentParams {
	uint32_t BackBufferWidth;		/* 0x00 */
	uint32_t BackBufferHeight;		/* 0x04 */
	uint32_t BackBufferFormat;		/* 0x08 */
	uint32_t BackBufferCount;		/* 0x0c */
	uint32_t MultiSampleType;		/* 0x10 */
	uint32_t SwapEffect;			/* 0x14 */
	void *hDeviceWindow;			/* 0x18 */
	int Windowed;				/* 0x1c */
	int EnableAutoDepthStencil;		/* 0x20 */
	uint32_t AutoDepthStencilFormat;	/* 0x24 */
	uint32_t Flags;				/* 0x28 */
	uint32_t FullScreen_RefreshRateInHz;	/* 0x2c */
	uint32_t FullScreen_PresentationInterval;	/* 0x30 */
} D3DPresentParams;
SD_SIZE(D3DPresentParams, 0x34);
#define D3DSWAPEFFECT_DISCARD 1
#define D3DSWAPEFFECT_COPY_VSYNC 4
#define D3DPRESENT_INTERVAL_IMMEDIATE 0x80000000

#define D3DPTEXTURECAPS_POW2 0x2
#define D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE 0x40
#define D3DPTEXTURECAPS_NONPOW2CONDITIONAL 0x100
#define D3DPTEXTURECAPS_MIPMAP 0x4000
#define D3DPS_VERSION(major, minor) (0xffff0000u | (major) << 8 | (minor))

/* D3DADAPTER_IDENTIFIER8 */
typedef struct D3DAdapterId {
	char Driver[0x200];		/* 0x000 */
	char Description[0x200];	/* 0x200 */
	uint16_t DriverVersion[4];	/* 0x400 build, sub version, version, product */
	uint32_t VendorId;		/* 0x408 */
	uint32_t DeviceId;		/* 0x40c */
	uint32_t SubSysId;		/* 0x410 */
	uint32_t Revision;		/* 0x414 */
	byte DeviceIdentifier[16];	/* 0x418 */
	uint32_t WHQLLevel;		/* 0x428 */
} D3DAdapterId;
SD_SIZE(D3DAdapterId, 0x42c);

/* the D3DX math of the exe (stdcall; src/lib/d3dx8/D3dxMath.c, hooks on its dispatcher stubs) */
vec3_t *__stdcall D3DX_Vec3Normalize(vec3_t *out, const vec3_t *v);
vec3_t *__stdcall D3DX_Vec3Transform(vec3_t *out, const vec3_t *v, const matrix_t *m);
matrix_t *__stdcall D3DX_MatrixRotationYPR(matrix_t *out, float yaw, float pitch, float roll);
matrix_t *__stdcall D3DX_MatrixMultiply(matrix_t *out, const matrix_t *a, const matrix_t *b);
matrix_t *__stdcall D3DX_MatrixTranspose(matrix_t *out, const matrix_t *m);
matrix_t *__stdcall D3DX_MatrixRotationX(matrix_t *out, float ang);
matrix_t *__stdcall D3DX_MatrixRotationY(matrix_t *out, float ang);
matrix_t *__stdcall D3DX_MatrixRotationZ(matrix_t *out, float ang);
matrix_t *__stdcall D3DX_MatrixRotationQuaternion(matrix_t *out, const quat_t *q);
quat_t *__stdcall D3DX_QuaternionRotationYPR(quat_t *out, float yaw, float pitch, float roll);
/* NULL when m has no inverse */
matrix_t *__stdcall D3DX_MatrixInverse(matrix_t *out, float *det, const matrix_t *m);

/* the D3DX texture functions (stdcall; src/lib/d3dx8/D3dxTex.c, DDS files only) */
int __stdcall D3DX_CheckTextureRequirements(void *dev, uint32_t *w, uint32_t *h, uint32_t *levels, uint32_t usage,
	int *fmt, int pool);
int __stdcall D3DX_CreateTextureFromFileInMemoryEx(void *dev, const void *data, uint32_t size, uint32_t w, uint32_t h,
	uint32_t levels, uint32_t usage, int fmt, int pool, uint32_t filter, uint32_t mipFilter, uint32_t colorKey,
	void *srcInfo, void *palette, void **tex);
/* the rectangles are RECTs, NULL for the whole surface */
int __stdcall D3DX_LoadSurfaceFromSurface(void *dst, const void *dstPal, const void *dstRect, void *src,
	const void *srcPal, const void *srcRect, uint32_t filter, uint32_t colorKey);
typedef void *(__stdcall *d3dxGetPointer_t)(void *buf);
#endif
