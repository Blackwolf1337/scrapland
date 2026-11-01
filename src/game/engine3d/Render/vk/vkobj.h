/*
 * The objects of the Vulkan backend behind the opaque pointers of the render device (include/scrap/rdev.h): the
 * IDirect3D8, the device with the state that Direct3D 8 keeps, textures, surfaces and buffers. Every object starts
 * with VkoObj, so Rd_AddRef and Rd_Release work on any of them.
 */
#ifndef VKOBJ_H
#define VKOBJ_H

#include "scrap/d3d8.h"
#include "vkcore.h"

enum { VKO_D3D = 0x5644, VKO_DEV, VKO_TEX, VKO_SURF, VKO_VB, VKO_IB };

typedef struct VkoObj {
	uint32_t kind;
	uint32_t refs;
} VkoObj;

#define VKO_MAX_LEVELS 14

typedef struct VkoTex VkoTex;

/* a surface: a level of a texture, the back buffer, the depth buffer or an image in system memory */
typedef struct VkoSurf {
	VkoObj o;
	VkoTex *tex;			/* the texture of a level, NULL else */
	uint32_t level;
	VkImg *img;			/* the image (of the texture, the back buffer or the depth buffer), NULL in memory */
	VkImg own;			/* the back buffer and depth buffer images */
	uint32_t format;		/* D3DFORMAT */
	uint32_t width, height;
	byte *mem;			/* system memory (image surface) or the copy of a locked GPU surface */
	uint32_t pitch;
	byte lockedRead;		/* the lock read the GPU surface (a lock of a render target or the back buffer) */
} VkoSurf;

struct VkoTex {
	VkoObj o;
	uint32_t format, usage, pool;
	uint32_t width, height, levels;
	VkImg img;
	VkoSurf *surf[VKO_MAX_LEVELS];	/* the surface objects of the levels, made by GetSurfaceLevel */
	byte *lock[VKO_MAX_LEVELS];	/* the memory of a locked level */
	uint32_t lockPitch[VKO_MAX_LEVELS];
};

/* a vertex or index buffer in host visible memory; a lock with D3DLOCK_DISCARD moves it into the frame ring */
typedef struct VkoBuf {
	VkoObj o;
	uint32_t size, usage, fvf, format, pool;
	VkBuf mem;
	VkBuffer cur;			/* where the data is now: mem or a part of a frame ring */
	VkDeviceSize curOff;
	byte *curMap;
	uint64_t used;			/* the frame serial of the last draw from it */
} VkoBuf;

#define VKO_STAGES 8
#define VKO_LIGHTS 8

typedef struct VkoD3D {
	VkoObj o;
} VkoD3D;

typedef struct VkoDev {
	VkoObj o;
	VkoD3D *d3d;
	void *hwnd;
	D3DPresentParams pp;
	VkoSurf *backBuffer, *depth;
	VkoSurf *rt, *zs;		/* the targets of the draws */
	uint32_t rs[256];		/* render states */
	uint32_t tss[VKO_STAGES][32];	/* texture stage states */
	VkoTex *tex[VKO_STAGES];
	float transform[512][16];	/* D3DTS_ up to the world matrices at 256 */
	uint32_t viewport[6];		/* x, y, w, h and the floats min z, max z */
	float material[17];
	float light[VKO_LIGHTS][26];
	byte lightOn[VKO_LIGHTS];
	VkoBuf *stream;
	uint32_t streamStride;
	VkoBuf *indices;
	uint32_t baseVertex;
	uint32_t fvf;
	uint32_t ps;			/* the pixel shader handle, 0 for the fixed function */
	float psConst[8][4];
	uint16_t gamma[3][256];
	byte inScene;
} VkoDev;

#endif
