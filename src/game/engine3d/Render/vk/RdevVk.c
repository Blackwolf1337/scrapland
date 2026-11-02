/*
 * The Vulkan backend of the render device (include/scrap/rdev.h): the objects of Direct3D 8 that the game uses,
 * kept as the C objects of vkobj.h on Vulkan resources. One adapter; the device keeps the states of Direct3D 8 and
 * VkDraw.c draws with them. Textures are images that a lock fills from system memory; the back buffer is an image of
 * its own that Present blits to the swapchain.
 */
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "../rdevbackend.h"
#include "vkdraw.h"
#include "vkobj.h"

#define D3D_OK 0
#define D3DERR_INVALIDCALL ((int)0x8876086c)
#define D3DERR_NOTAVAILABLE ((int)0x8876086a)

#define D3DFMT_R8G8B8 20
#define D3DFMT_A8R8G8B8 21
#define D3DFMT_R5G6B5 23
#define D3DFMT_X1R5G5B5 24
#define D3DFMT_A1R5G5B5 25
#define D3DFMT_A4R4G4B4 26
#define D3DFMT_A8 28
#define D3DFMT_X4R4G4B4 30
#define D3DFMT_L8 50
#define D3DFMT_A8L8 51
#define D3DFMT_V8U8 60
#define D3DFMT_D24S8 75
#define D3DFMT_DXT1 0x31545844
#define D3DFMT_DXT2 0x32545844
#define D3DFMT_DXT3 0x33545844
#define D3DFMT_DXT4 0x34545844
#define D3DFMT_DXT5 0x35545844
#define D3DUSAGE_DEPTHSTENCIL 0x2
#define D3DLOCK_READONLY 0x10
#define D3DLOCK_DISCARD 0x2000
#define D3DLOCK_NOOVERWRITE 0x1000
#define D3DCLEAR_ZBUFFER 2
#define D3DCLEAR_STENCIL 4

/* ---- formats ---- */

typedef struct {
	VkFormat vk;
	VkComponentMapping swizzle;
	uint32_t bpp;			/* bytes per pixel of the D3D format, 0 for a block format */
	uint32_t block;			/* bytes per 4 x 4 block */
	byte convert;			/* the D3D data is converted to BGRA8 on upload */
} VkoFormat;

#define SW_ID { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, \
	VK_COMPONENT_SWIZZLE_IDENTITY }
#define SW_NOALPHA { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, \
	VK_COMPONENT_SWIZZLE_ONE }

static VkoFormat Vko_Format(uint32_t fmt) {
	switch (fmt) {
	case D3DFMT_A8R8G8B8:
		return (VkoFormat) { VK_FORMAT_B8G8R8A8_UNORM, SW_ID, 4, 0, 0 };
	case D3DFMT_X8R8G8B8:
		return (VkoFormat) { VK_FORMAT_B8G8R8A8_UNORM, SW_NOALPHA, 4, 0, 0 };
	case D3DFMT_R5G6B5:
		return (VkoFormat) { VK_FORMAT_R5G6B5_UNORM_PACK16, SW_ID, 2, 0, 0 };
	case D3DFMT_A1R5G5B5:
		return (VkoFormat) { VK_FORMAT_A1R5G5B5_UNORM_PACK16, SW_ID, 2, 0, 0 };
	case D3DFMT_X1R5G5B5:
		return (VkoFormat) { VK_FORMAT_A1R5G5B5_UNORM_PACK16, SW_NOALPHA, 2, 0, 0 };
	case D3DFMT_A4R4G4B4:
	case D3DFMT_X4R4G4B4:
		return (VkoFormat) { VK_FORMAT_B8G8R8A8_UNORM, fmt == D3DFMT_A4R4G4B4 ? (VkComponentMapping)SW_ID
			: (VkComponentMapping)SW_NOALPHA, 2, 0, 1 };
	case D3DFMT_R8G8B8:
		return (VkoFormat) { VK_FORMAT_B8G8R8A8_UNORM, SW_NOALPHA, 3, 0, 1 };
	case D3DFMT_A8:
		return (VkoFormat) { VK_FORMAT_R8_UNORM, { VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
			VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R }, 1, 0, 0 };
	case D3DFMT_L8:
		return (VkoFormat) { VK_FORMAT_R8_UNORM, { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE }, 1, 0, 0 };
	case D3DFMT_A8L8:
		return (VkoFormat) { VK_FORMAT_R8G8_UNORM, { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G }, 2, 0, 0 };
	case D3DFMT_V8U8:
		return (VkoFormat) { VK_FORMAT_R8G8_SNORM, SW_ID, 2, 0, 0 };
	case D3DFMT_DXT1:
		return (VkoFormat) { VK_FORMAT_BC1_RGBA_UNORM_BLOCK, SW_ID, 0, 8, 0 };
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
		return (VkoFormat) { VK_FORMAT_BC2_UNORM_BLOCK, SW_ID, 0, 16, 0 };
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		return (VkoFormat) { VK_FORMAT_BC3_UNORM_BLOCK, SW_ID, 0, 16, 0 };
	default:
		return (VkoFormat) { VK_FORMAT_UNDEFINED, SW_ID, 0, 0, 0 };
	}
}

static uint32_t Vko_Pitch(const VkoFormat *f, uint32_t w) {
	return f->block ? (w + 3) / 4 * f->block : w * f->bpp;
}

static uint32_t Vko_Rows(const VkoFormat *f, uint32_t h) {
	return f->block ? (h + 3) / 4 : h;
}

static uint32_t Vko_Dim(uint32_t size, uint32_t level) {
	uint32_t d = size >> level;
	return d ? d : 1;
}

/* a pixel of a D3D format to BGRA8 (the formats of VkoFormat.convert) */
static uint32_t Vko_ToBgra(const byte *p, uint32_t fmt) {
	switch (fmt) {
	case D3DFMT_R8G8B8:
		return 0xff000000u | p[2] << 16 | p[1] << 8 | p[0];
	case D3DFMT_A4R4G4B4:
	case D3DFMT_X4R4G4B4: {
		uint32_t c = (uint32_t)(p[0] | p[1] << 8);
		uint32_t a = c >> 12 & 15, r = c >> 8 & 15, g = c >> 4 & 15, b = c & 15;
		return (a * 17) << 24 | (r * 17) << 16 | (g * 17) << 8 | b * 17;
	}
	default:
		return 0;
	}
}

/* a BGRA8 pixel into a D3D format of 16 or 32 bits (the readback of a render target) */
static void Vko_FromBgra(byte *p, uint32_t c, uint32_t fmt) {
	uint32_t r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255, a = c >> 24;
	uint16_t v;

	switch (fmt) {
	case D3DFMT_R5G6B5:
		v = (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | b >> 3);
		memcpy(p, &v, 2);
		break;
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
		v = (uint16_t)((a >> 7) << 15 | (r >> 3) << 10 | (g >> 3) << 5 | b >> 3);
		memcpy(p, &v, 2);
		break;
	case D3DFMT_R8G8B8:
		p[0] = (byte)b, p[1] = (byte)g, p[2] = (byte)r;
		break;
	default:
		memcpy(p, &c, 4);
		break;
	}
}

/* ---- objects ---- */

static void *Vko_New(size_t size, uint32_t kind) {
	VkoObj *o = calloc(1, size);
	if (o) {
		o->kind = kind;
		o->refs = 1;
	}
	return o;
}

static void Vko_FreeSurf(VkoSurf *s) {
	Vk_DeferImage(&s->own);
	free(s->mem);
	free(s);
}

/* the surfaces of its levels go with the texture */
static void Vko_FreeTex(VkoTex *t) {
	for (uint32_t i = 0; i < t->levels; i++) {
		free(t->surf[i]);
		free(t->lock[i]);
	}
	Vk_DeferImage(&t->img);
	free(t);
}

/* the surface of a texture level counts its references on the texture, as in Direct3D 8 */
static uint32_t Vk_AddRef(void *obj) {
	VkoObj *o = obj;
	if (o->kind == VKO_SURF && ((VkoSurf *)obj)->tex) {
		o = &((VkoSurf *)obj)->tex->o;
	}
	return ++o->refs;
}

static uint32_t Vk_Release(void *obj) {
	VkoObj *o = obj;

	if (o->kind == VKO_SURF && ((VkoSurf *)obj)->tex) {
		o = &((VkoSurf *)obj)->tex->o;
	}
	if (--o->refs) {
		return o->refs;
	}
	switch (o->kind) {
	case VKO_TEX:
		Vko_FreeTex((VkoTex *)o);
		break;
	case VKO_SURF:
		Vko_FreeSurf((VkoSurf *)o);
		break;
	case VKO_VB:
	case VKO_IB: {
		VkoBuf *b = obj;
		Vk_DeferBuffer(&b->mem);
		for (int i = 0; i < b->numSpares; i++) {
			Vk_DeferBuffer(&b->spare[i]);
		}
		free(b);
		break;
	}
	case VKO_DEV:
		Vkd_Shutdown();
		Vk_DestroyDevice();
		free(obj);
		break;
	default:
		free(obj);
		break;
	}
	return 0;
}

/* ---- IDirect3D8 ---- */

static const uint32_t vk_modes[][2] = {
	{ 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1152, 864 }, { 1280, 720 }, { 1280, 800 }, { 1280, 960 },
	{ 1280, 1024 }, { 1360, 768 }, { 1366, 768 }, { 1440, 900 }, { 1600, 900 }, { 1600, 1200 }, { 1680, 1050 },
	{ 1920, 1080 }, { 1920, 1200 }, { 2560, 1080 }, { 2560, 1440 }, { 3440, 1440 }, { 3840, 2160 },
};
#define VK_NMODES ((uint32_t)(sizeof(vk_modes) / sizeof(vk_modes[0])))

static void *Vk_Direct3DCreate8(uint32_t sdk) {
	(void)sdk;
	if (!Vk_Init()) {
		return NULL;
	}
	return Vko_New(sizeof(VkoD3D), VKO_D3D);
}

static uint32_t Vk_D3DGetAdapterCount(void *d3d) {
	(void)d3d;
	return 1;
}

static int Vk_D3DGetAdapterIdentifier(void *d3d, uint32_t adapter, uint32_t flags, void *id) {
	D3DAdapterId *a = id;

	(void)d3d, (void)adapter, (void)flags;
	memset(a, 0, sizeof(*a));
	strcpy(a->Driver, "vulkan");
	strcpy(a->Description, "Vulkan");
	return D3D_OK;
}

/* the desktop: the size of the screen in X8R8G8B8 */
static void Vk_Desktop(D3DDisplayMode *m) {
	m->Width = (uint32_t)GetSystemMetrics(SM_CXSCREEN);
	m->Height = (uint32_t)GetSystemMetrics(SM_CYSCREEN);
	m->RefreshRate = 60;
	m->Format = D3DFMT_X8R8G8B8;
}

static int Vk_D3DGetAdapterDisplayMode(void *d3d, uint32_t adapter, void *out) {
	(void)d3d, (void)adapter;
	Vk_Desktop(out);
	return D3D_OK;
}

/* the modes up to the desktop size, in X8R8G8B8 and R5G6B5 */
static uint32_t Vk_D3DGetAdapterModeCount(void *d3d, uint32_t adapter) {
	D3DDisplayMode desk;
	uint32_t n = 0;

	(void)d3d, (void)adapter;
	Vk_Desktop(&desk);
	for (uint32_t i = 0; i < VK_NMODES; i++) {
		n += vk_modes[i][0] <= desk.Width && vk_modes[i][1] <= desk.Height;
	}
	return n * 2;
}

static int Vk_D3DEnumAdapterModes(void *d3d, uint32_t adapter, uint32_t mode, void *out) {
	D3DDisplayMode desk, *m = out;
	uint32_t n = 0;

	(void)d3d, (void)adapter;
	Vk_Desktop(&desk);
	for (uint32_t i = 0; i < VK_NMODES; i++) {
		if (vk_modes[i][0] > desk.Width || vk_modes[i][1] > desk.Height) {
			continue;
		}
		if (n == mode / 2) {
			m->Width = vk_modes[i][0];
			m->Height = vk_modes[i][1];
			m->RefreshRate = 60;
			m->Format = mode & 1 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
			return D3D_OK;
		}
		n++;
	}
	return D3DERR_INVALIDCALL;
}

static int Vk_D3DCheckDeviceType(void *d3d, uint32_t adapter, uint32_t type, uint32_t dfmt, uint32_t bfmt,
	int windowed) {
	(void)d3d, (void)adapter, (void)dfmt, (void)bfmt, (void)windowed;
	return type == 1 ? D3D_OK : D3DERR_NOTAVAILABLE;	/* only the HAL */
}

static int Vk_D3DCheckDeviceFormat(void *d3d, uint32_t adapter, int devType, int adapterFmt, uint32_t usage,
	int rtype, int fmt) {
	(void)d3d, (void)adapter, (void)devType, (void)adapterFmt, (void)rtype;
	if (usage & D3DUSAGE_DEPTHSTENCIL) {
		return fmt == D3DFMT_D24S8 ? D3D_OK : D3DERR_NOTAVAILABLE;
	}
	return Vko_Format((uint32_t)fmt).vk != VK_FORMAT_UNDEFINED ? D3D_OK : D3DERR_NOTAVAILABLE;
}

static int Vk_D3DCheckDepthStencilMatch(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, uint32_t rt,
	uint32_t ds) {
	(void)d3d, (void)adapter, (void)type, (void)fmt, (void)rt;
	return ds == D3DFMT_D24S8 ? D3D_OK : D3DERR_NOTAVAILABLE;
}

static int Vk_D3DCheckDeviceMultiSampleType(void *d3d, uint32_t adapter, uint32_t type, uint32_t fmt, int windowed,
	uint32_t ms) {
	(void)d3d, (void)adapter, (void)type, (void)fmt, (void)windowed;
	return ms == 0 ? D3D_OK : D3DERR_NOTAVAILABLE;
}

/* the caps of a D3D8 card of its time with what the game asks for: pixel shaders 1.1, stencil, 8 lights */
static void Vk_Caps(D3DCaps8 *c) {
	memset(c, 0, sizeof(*c));
	c->DeviceType = 1;
	c->Caps2 = 0x20000 | 0x80000;		/* FULLSCREENGAMMA, CANRENDERWINDOWED */
	c->PresentationIntervals = 0x80000001;
	/* HWTRANSFORMANDLIGHT, PUREDEVICE, HWRASTERIZATION, DRAWPRIMTLVERTEX, TEXTUREVIDEOMEMORY */
	c->DevCaps = 0x10000 | 0x100000 | 0x80000 | 0x400 | 0x200;
	c->PrimitiveMiscCaps = 0x2 | 0x10 | 0x20 | 0x40 | 0x80;
	c->RasterCaps = 0x1 | 0x10 | 0x80 | 0x100 | 0x2000 | 0x20000 | 0x100000 | 0x200000;
	c->ZCmpCaps = 0xff;
	c->SrcBlendCaps = 0x1fff;
	c->DestBlendCaps = 0x1fff;
	c->AlphaCmpCaps = 0xff;
	c->ShadeCaps = 0x8 | 0x200 | 0x4000 | 0x80000;
	c->TextureCaps = 0x1 | 0x4 | 0x40 | 0x4000 | 0x10000;
	c->TextureFilterCaps = 0x0303 | 0x0400 | 0x03000000 | 0x0200 | 0x100 | 0x20000 | 0x10000;
	c->TextureAddressCaps = 0x1f;
	c->MaxTextureWidth = 4096;
	c->MaxTextureHeight = 4096;
	c->MaxTextureRepeat = 8192;
	c->MaxTextureAspectRatio = 4096;
	c->MaxAnisotropy = 16;
	c->MaxVertexW = 1e10f;
	c->GuardBandLeft = -8192.0f;
	c->GuardBandTop = -8192.0f;
	c->GuardBandRight = 8192.0f;
	c->GuardBandBottom = 8192.0f;
	c->StencilCaps = 0xff;
	c->FVFCaps = 0x80000 | 8;
	c->TextureOpCaps = 0x03ffffff;
	c->MaxTextureBlendStages = VKO_STAGES;
	c->MaxSimultaneousTextures = 4;
	c->VertexProcessingCaps = 0x3f;
	c->MaxActiveLights = VKO_LIGHTS;
	c->MaxPointSize = 64.0f;
	c->MaxPrimitiveCount = 0xffffff;
	c->MaxVertexIndex = 0xffffff;
	c->MaxStreams = 1;
	c->MaxStreamStride = 256;
	c->VertexShaderVersion = 0xfffe0101;
	c->MaxVertexShaderConst = 96;
	c->PixelShaderVersion = D3DPS_VERSION(1, 1);
	c->MaxPixelShaderValue = 1.0f;
}

static int Vk_D3DGetDeviceCaps(void *d3d, uint32_t adapter, uint32_t type, void *caps) {
	(void)d3d, (void)adapter, (void)type;
	Vk_Caps(caps);
	return D3D_OK;
}

/* ---- the device ---- */

static VkoSurf *Vko_NewTargetSurf(uint32_t format, uint32_t w, uint32_t h, int depth) {
	VkoSurf *s = Vko_New(sizeof(VkoSurf), VKO_SURF);
	if (!s) {
		return NULL;
	}
	s->format = format;
	s->width = w;
	s->height = h;
	s->img = &s->own;
	int ok = depth
		? Vk_CreateImage(&s->own, vk.depthFormat, w, h, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
			| VK_IMAGE_USAGE_TRANSFER_DST_BIT, (VkComponentMapping)SW_ID)
		: Vk_CreateImage(&s->own, VK_FORMAT_B8G8R8A8_UNORM, w, h, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
			| VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			(VkComponentMapping)SW_ID);
	if (!ok) {
		free(s);
		return NULL;
	}
	return s;
}

/* the back buffer and depth buffer of the present parameters */
static int Vk_MakeTargets(VkoDev *d) {
	D3DPresentParams *pp = &d->pp;
	if (!pp->BackBufferWidth || !pp->BackBufferHeight) {
		RECT r;
		GetClientRect(d->hwnd, &r);
		pp->BackBufferWidth = (uint32_t)(r.right - r.left);
		pp->BackBufferHeight = (uint32_t)(r.bottom - r.top);
	}
	if (!pp->BackBufferFormat) {
		pp->BackBufferFormat = D3DFMT_X8R8G8B8;
	}
	d->backBuffer = Vko_NewTargetSurf(pp->BackBufferFormat, pp->BackBufferWidth, pp->BackBufferHeight, 0);
	if (!d->backBuffer) {
		return 0;
	}
	if (pp->EnableAutoDepthStencil) {
		d->depth = Vko_NewTargetSurf(pp->AutoDepthStencilFormat, pp->BackBufferWidth, pp->BackBufferHeight, 1);
		if (!d->depth) {
			return 0;
		}
	}
	d->rt = d->backBuffer;
	d->zs = d->depth;
	d->viewport[0] = 0;
	d->viewport[1] = 0;
	d->viewport[2] = pp->BackBufferWidth;
	d->viewport[3] = pp->BackBufferHeight;
	float zmax = 1.0f;
	d->viewport[4] = 0;
	memcpy(&d->viewport[5], &zmax, 4);
	return 1;
}

static void Vk_DefaultState(VkoDev *d) {
	static const float id[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	float one = 1.0f;

	memset(d->rs, 0, sizeof(d->rs));
	d->rs[7] = d->pp.EnableAutoDepthStencil;	/* ZENABLE */
	d->rs[8] = 3;				/* FILLMODE solid */
	d->rs[9] = 2;				/* SHADEMODE gouraud */
	d->rs[14] = 1;				/* ZWRITEENABLE */
	d->rs[16] = 1;				/* LASTPIXEL */
	d->rs[19] = 2;				/* SRCBLEND one */
	d->rs[20] = 1;				/* DESTBLEND zero */
	d->rs[22] = 3;				/* CULLMODE ccw */
	d->rs[23] = 4;				/* ZFUNC lessequal */
	d->rs[25] = 8;				/* ALPHAFUNC always */
	memcpy(&d->rs[38], &one, 4);		/* FOGDENSITY */
	memcpy(&d->rs[37], &one, 4);		/* FOGEND */
	d->rs[56] = 8;				/* STENCILFUNC always */
	d->rs[53] = d->rs[54] = d->rs[55] = 1;	/* stencil ops keep */
	d->rs[58] = d->rs[59] = 0xffffffff;	/* STENCILMASK, STENCILWRITEMASK */
	d->rs[60] = 0xffffffff;			/* TEXTUREFACTOR */
	d->rs[136] = 1;				/* CLIPPING */
	d->rs[137] = 1;				/* LIGHTING */
	d->rs[141] = 1;				/* COLORVERTEX */
	d->rs[142] = 1;				/* LOCALVIEWER */
	d->rs[145] = 1;				/* DIFFUSEMATERIALSOURCE color1 */
	d->rs[146] = 2;				/* SPECULARMATERIALSOURCE color2 */
	memcpy(&d->rs[154], &one, 4);		/* POINTSIZE */
	d->rs[162] = 0xffffffff;		/* MULTISAMPLEMASK */
	d->rs[168] = 0xf;			/* COLORWRITEENABLE */
	d->rs[171] = 1;				/* BLENDOP add */
	for (int s = 0; s < VKO_STAGES; s++) {
		memset(d->tss[s], 0, sizeof(d->tss[s]));
		d->tss[s][1] = s ? 1 : 4;	/* COLOROP disable, modulate on stage 0 */
		d->tss[s][2] = 2;		/* COLORARG1 texture */
		d->tss[s][3] = 1;		/* COLORARG2 current */
		d->tss[s][4] = s ? 1 : 2;	/* ALPHAOP disable, selectarg1 on stage 0 */
		d->tss[s][5] = 2;
		d->tss[s][6] = 1;
		d->tss[s][11] = (uint32_t)s;	/* TEXCOORDINDEX */
		d->tss[s][13] = d->tss[s][14] = d->tss[s][25] = 1;	/* ADDRESS wrap */
		d->tss[s][16] = d->tss[s][17] = 1;	/* MAG, MIN point */
		d->tss[s][21] = 1;		/* MAXANISOTROPY */
		d->tex[s] = NULL;
	}
	for (int i = 0; i < 512; i++) {
		memcpy(d->transform[i], id, sizeof(id));
	}
	for (int i = 0; i < 256; i++) {
		d->gamma[0][i] = d->gamma[1][i] = d->gamma[2][i] = (uint16_t)(i * 257);
	}
}

static int Vk_D3DCreateDevice(void *d3d, uint32_t adapter, uint32_t type, void *hwnd, uint32_t behavior, void *pp,
	void **dev) {
	(void)adapter, (void)type, (void)behavior;
	VkoDev *d = Vko_New(sizeof(VkoDev), VKO_DEV);
	if (!d) {
		return D3DERR_NOTAVAILABLE;
	}
	d->d3d = d3d;
	d->pp = *(const D3DPresentParams *)pp;
	d->hwnd = d->pp.hDeviceWindow ? d->pp.hDeviceWindow : hwnd;
	byte vsync = d->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE;
	RECT r;
	GetClientRect(d->hwnd, &r);
	if (!Vk_CreateDevice(d->hwnd, (uint32_t)(r.right - r.left), (uint32_t)(r.bottom - r.top), vsync)
		|| !Vk_MakeTargets(d) || !Vkd_Init(d)) {
		Com_Printf("vulkan: no device\n");
		free(d);
		return D3DERR_NOTAVAILABLE;
	}
	Vk_DefaultState(d);
	*dev = d;
	return D3D_OK;
}

static int Vk_DevReset(void *dev, void *pp) {
	VkoDev *d = dev;

	Vkd_End();
	Vk_Flush();
	Vk_Release(d->backBuffer);
	if (d->depth) {
		Vk_Release(d->depth);
	}
	d->backBuffer = d->depth = NULL;
	d->pp = *(const D3DPresentParams *)pp;
	RECT r;
	GetClientRect(d->hwnd, &r);
	byte vsync = d->pp.FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE;
	if (!Vk_ResizeSwapchain((uint32_t)(r.right - r.left), (uint32_t)(r.bottom - r.top), vsync) || !Vk_MakeTargets(d)) {
		return D3DERR_NOTAVAILABLE;
	}
	Vk_DefaultState(d);
	return D3D_OK;
}

static int Vk_DevTestCooperativeLevel(void *dev) {
	(void)dev;
	return D3D_OK;
}

static uint32_t Vk_DevGetAvailableTextureMem(void *dev) {
	(void)dev;
	return 512u << 20;
}

static int Vk_DevGetDeviceCaps(void *dev, void *caps) {
	(void)dev;
	Vk_Caps(caps);
	return D3D_OK;
}

static int Vk_DevGetDirect3D(void *dev, void **d3d) {
	VkoDev *d = dev;
	Vk_AddRef(d->d3d);
	*d3d = d->d3d;
	return D3D_OK;
}

static int Vk_DevGetDisplayMode(void *dev, void *mode) {
	VkoDev *d = dev;
	D3DDisplayMode *m = mode;
	m->Width = d->pp.BackBufferWidth;
	m->Height = d->pp.BackBufferHeight;
	m->RefreshRate = 60;
	m->Format = d->pp.BackBufferFormat;
	return D3D_OK;
}

static int Vk_DevShowCursor(void *dev, int show) {
	(void)dev, (void)show;
	return 0;
}

static void Vk_DevSetCursorPosition(void *dev, int x, int y, uint32_t flags) {
	(void)dev, (void)x, (void)y, (void)flags;
}

static int Vk_DevSetGammaRamp(void *dev, uint32_t flags, const void *ramp) {
	(void)flags;
	memcpy(((VkoDev *)dev)->gamma, ramp, sizeof(((VkoDev *)dev)->gamma));
	return D3D_OK;
}

static int Vk_DevBeginScene(void *dev) {
	((VkoDev *)dev)->inScene = 1;
	return D3D_OK;
}

static int Vk_DevEndScene(void *dev) {
	((VkoDev *)dev)->inScene = 0;
	return D3D_OK;
}

static int Vk_DevPresent(void *dev, const void *src, const void *dst, void *hwnd, const void *dirty) {
	VkoDev *d = dev;

	(void)src, (void)dst, (void)hwnd, (void)dirty;
	return Vkd_Present(d) ? D3D_OK : D3DERR_INVALIDCALL;
}

static int Vk_DevGetBackBuffer(void *dev, uint32_t i, int type, void **surf) {
	VkoDev *d = dev;
	(void)i, (void)type;
	Vk_AddRef(d->backBuffer);
	*surf = d->backBuffer;
	return D3D_OK;
}

static int Vk_DevGetDepthStencilSurface(void *dev, void **surf) {
	VkoDev *d = dev;
	if (!d->zs) {
		*surf = NULL;
		return D3DERR_NOTAVAILABLE;
	}
	Vk_AddRef(d->zs);
	*surf = d->zs;
	return D3D_OK;
}

static int Vk_DevGetRenderTarget(void *dev, void **surf) {
	VkoDev *d = dev;
	Vk_AddRef(d->rt);
	*surf = d->rt;
	return D3D_OK;
}

static int Vk_DevSetRenderTarget(void *dev, void *rt, void *zs) {
	VkoDev *d = dev;

	Vkd_End();
	if (rt) {
		d->rt = rt;
	}
	d->zs = zs;
	if (Vkd_Tracing()) {
		Com_Printf("target %ux%u fmt %u tex %p zs %p\n", d->rt->width, d->rt->height, d->rt->format,
			(void *)d->rt->tex, zs);
	}
	VkoSurf *t = d->rt;
	float zmax = 1.0f;
	d->viewport[0] = d->viewport[1] = 0;
	d->viewport[2] = t->width;
	d->viewport[3] = t->height;
	d->viewport[4] = 0;
	memcpy(&d->viewport[5], &zmax, 4);
	return D3D_OK;
}

static int Vk_DevClear(void *dev, uint32_t count, const void *rects, uint32_t flags, uint32_t color, float z,
	uint32_t stencil) {
	if (Vkd_Tracing()) {
		Com_Printf("clear %u rects flags %x color %08x z %g\n", count, flags, color, (double)z);
	}
	return Vkd_Clear(dev, count, rects, flags, color, z, stencil) ? D3D_OK : D3DERR_INVALIDCALL;
}

/* ---- states ---- */

static int Vk_DevSetRenderState(void *dev, uint32_t state, uint32_t value) {
	if (state < 256) {
		((VkoDev *)dev)->rs[state] = value;
	}
	return D3D_OK;
}

static int Vk_DevGetRenderState(void *dev, uint32_t state, uint32_t *value) {
	*value = state < 256 ? ((VkoDev *)dev)->rs[state] : 0;
	return D3D_OK;
}

static int Vk_DevSetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t value) {
	if (stage < VKO_STAGES && type < 32) {
		((VkoDev *)dev)->tss[stage][type] = value;
	}
	return D3D_OK;
}

static int Vk_DevGetTextureStageState(void *dev, uint32_t stage, uint32_t type, uint32_t *value) {
	*value = stage < VKO_STAGES && type < 32 ? ((VkoDev *)dev)->tss[stage][type] : 0;
	return D3D_OK;
}

static int Vk_DevSetTexture(void *dev, uint32_t stage, void *tex) {
	VkoDev *d = dev;
	if (stage >= VKO_STAGES) {
		return D3DERR_INVALIDCALL;
	}
	if (tex) {
		Vk_AddRef(tex);
	}
	if (d->tex[stage]) {
		Vk_Release(d->tex[stage]);
	}
	d->tex[stage] = tex;
	return D3D_OK;
}

static int Vk_DevSetTransform(void *dev, uint32_t state, const void *m) {
	if (state < 512) {
		memcpy(((VkoDev *)dev)->transform[state], m, 64);
	}
	return D3D_OK;
}

static int Vk_DevGetTransform(void *dev, uint32_t state, void *m) {
	if (state < 512) {
		memcpy(m, ((VkoDev *)dev)->transform[state], 64);
	}
	return D3D_OK;
}

static int Vk_DevSetViewport(void *dev, const uint32_t *vp) {
	memcpy(((VkoDev *)dev)->viewport, vp, 24);
	return D3D_OK;
}

static int Vk_DevGetViewport(void *dev, uint32_t *vp) {
	memcpy(vp, ((VkoDev *)dev)->viewport, 24);
	return D3D_OK;
}

static int Vk_DevSetMaterial(void *dev, const void *mat) {
	memcpy(((VkoDev *)dev)->material, mat, sizeof(((VkoDev *)dev)->material));
	return D3D_OK;
}

static int Vk_DevSetLight(void *dev, uint32_t idx, const void *l) {
	if (idx < VKO_LIGHTS) {
		memcpy(((VkoDev *)dev)->light[idx], l, sizeof(((VkoDev *)dev)->light[idx]));
	}
	return D3D_OK;
}

static int Vk_DevLightEnable(void *dev, uint32_t idx, int enable) {
	if (idx < VKO_LIGHTS) {
		((VkoDev *)dev)->lightOn[idx] = enable != 0;
	}
	return D3D_OK;
}

static int Vk_DevSetVertexShader(void *dev, uint32_t fvf) {
	((VkoDev *)dev)->fvf = fvf;
	return D3D_OK;
}

static int Vk_DevSetStreamSource(void *dev, uint32_t stream, void *vb, uint32_t stride) {
	VkoDev *d = dev;
	if (stream) {
		return D3DERR_INVALIDCALL;
	}
	d->stream = vb;
	d->streamStride = stride;
	return D3D_OK;
}

static int Vk_DevSetIndices(void *dev, void *ib, uint32_t baseVertex) {
	VkoDev *d = dev;
	d->indices = ib;
	d->baseVertex = baseVertex;
	return D3D_OK;
}

static int Vk_DevCreatePixelShader(void *dev, const char *name, const void *code, uint32_t *handle) {
	(void)dev, (void)code;
	*handle = Vkd_CreatePixelShader(name);
	return *handle ? D3D_OK : D3DERR_INVALIDCALL;
}

static int Vk_DevDeletePixelShader(void *dev, uint32_t handle) {
	(void)dev, (void)handle;
	return D3D_OK;
}

static int Vk_DevSetPixelShader(void *dev, uint32_t handle) {
	if (Vkd_Tracing()) {
		Com_Printf("ps %u\n", handle);
	}
	((VkoDev *)dev)->ps = handle;
	return D3D_OK;
}

static int Vk_DevSetPixelShaderConstant(void *dev, uint32_t reg, const void *data, uint32_t count) {
	VkoDev *d = dev;
	if (reg + count > 8) {
		return D3DERR_INVALIDCALL;
	}
	memcpy(d->psConst[reg], data, count * 16);
	return D3D_OK;
}

/* ---- draws ---- */

static int Vk_DevDrawPrimitive(void *dev, int type, uint32_t start, uint32_t count) {
	return Vkd_Draw(dev, type, count, start, NULL, 0, NULL, 0, 0);
}

static int Vk_DevDrawIndexedPrimitive(void *dev, int type, uint32_t minIndex, uint32_t numVertices,
	uint32_t startIndex, uint32_t primCount) {
	(void)minIndex, (void)numVertices;
	return Vkd_Draw(dev, type, primCount, startIndex, NULL, 0, NULL, 0, 1);
}

static int Vk_DevDrawPrimitiveUP(void *dev, int type, uint32_t count, const void *data, uint32_t stride) {
	return Vkd_Draw(dev, type, count, 0, data, stride, NULL, 0, 0);
}

static int Vk_DevDrawIndexedPrimitiveUP(void *dev, int type, uint32_t minIndex, uint32_t numVertices,
	uint32_t primCount, const void *idx, int idxFmt, const void *data, uint32_t stride) {
	(void)minIndex;
	return Vkd_DrawIndexedUP(dev, type, primCount, numVertices, idx, idxFmt, data, stride);
}

/* ---- textures ---- */

static int Vk_DevCreateTexture(void *dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, int fmt, int pool,
	void **tex) {
	(void)dev;
	VkoFormat f = Vko_Format((uint32_t)fmt);
	if (f.vk == VK_FORMAT_UNDEFINED || !w || !h) {
		Com_Printf("vulkan: texture format %d not supported\n", fmt);
		return D3DERR_INVALIDCALL;
	}
	uint32_t full = 1;
	while ((w >> full) || (h >> full)) {
		full++;
	}
	if (!levels || levels > full) {
		levels = full;
	}
	if (levels > VKO_MAX_LEVELS) {
		levels = VKO_MAX_LEVELS;
	}
	VkoTex *t = Vko_New(sizeof(VkoTex), VKO_TEX);
	if (!t) {
		return D3DERR_INVALIDCALL;
	}
	t->format = (uint32_t)fmt;
	t->usage = usage;
	t->pool = (uint32_t)pool;
	t->width = w;
	t->height = h;
	t->levels = levels;
	VkImageUsageFlags iu = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	VkFormat vf = f.vk;
	VkComponentMapping sw = f.swizzle;
	if (usage & D3DUSAGE_RENDERTARGET) {
		iu |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		vf = VK_FORMAT_B8G8R8A8_UNORM;
		sw = (VkComponentMapping)SW_ID;
	}
	if (!Vk_CreateImage(&t->img, vf, w, h, levels, iu, sw)) {
		free(t);
		return D3DERR_INVALIDCALL;
	}
	*tex = t;
	return D3D_OK;
}

static int Vk_TexGetDevice(void *tex, void **dev) {
	(void)tex;
	*dev = Vkd_Device();
	if (*dev) {
		Vk_AddRef(*dev);
	}
	return D3D_OK;
}

static uint32_t Vk_TexGetLevelCount(void *tex) {
	return ((VkoTex *)tex)->levels;
}

static int Vk_TexGetLevelDesc(void *tex, uint32_t level, void *desc) {
	VkoTex *t = tex;
	d3dSurfaceDesc_t *d = desc;
	if (level >= t->levels) {
		return D3DERR_INVALIDCALL;
	}
	VkoFormat f = Vko_Format(t->format);
	memset(d, 0, sizeof(*d));
	d->Format = (int)t->format;
	d->Type = 1;				/* D3DRTYPE_SURFACE */
	d->Usage = t->usage;
	d->Pool = (int)t->pool;
	d->Width = Vko_Dim(t->width, level);
	d->Height = Vko_Dim(t->height, level);
	d->Size = Vko_Pitch(&f, d->Width) * Vko_Rows(&f, d->Height);
	return D3D_OK;
}

static int Vk_TexGetSurfaceLevel(void *tex, uint32_t level, void **surf) {
	VkoTex *t = tex;
	if (level >= t->levels) {
		return D3DERR_INVALIDCALL;
	}
	VkoSurf *s = t->surf[level];
	if (!s) {
		s = Vko_New(sizeof(VkoSurf), VKO_SURF);
		if (!s) {
			return D3DERR_INVALIDCALL;
		}
		s->tex = t;
		s->level = level;
		s->img = &t->img;
		s->format = t->format;
		s->width = Vko_Dim(t->width, level);
		s->height = Vko_Dim(t->height, level);
		if (t->pool == D3DPOOL_SYSTEMMEM) {
			/* a level in memory: copies to and from it go through its lock memory, as for an image surface */
			VkoFormat f = Vko_Format(t->format);
			s->pitch = Vko_Pitch(&f, s->width);
			if (!t->lock[level]) {
				t->lock[level] = calloc(Vko_Rows(&f, s->height), s->pitch);
				t->lockPitch[level] = s->pitch;
			}
			s->img = NULL;
			s->mem = t->lock[level];
			if (!s->mem) {
				free(s);
				return D3DERR_INVALIDCALL;
			}
		}
		t->surf[level] = s;
	}
	Vk_AddRef(t);
	*surf = s;
	return D3D_OK;
}

/* uploads w x h pixels of data (pitch) in the D3D format fmt into level of im at x, y */
static void Vko_Upload(VkImg *im, uint32_t level, uint32_t fmt, const byte *data, uint32_t pitch, uint32_t x,
	uint32_t y, uint32_t w, uint32_t h) {
	VkoFormat f = Vko_Format(fmt);
	if (im->format == VK_FORMAT_B8G8R8A8_UNORM && f.vk != VK_FORMAT_B8G8R8A8_UNORM) {
		f.convert = 1;		/* a 16 bit render target filled from memory */
	}
	uint32_t rows = Vko_Rows(&f, h);
	uint32_t rowBytes = f.convert ? w * 4 : Vko_Pitch(&f, w);
	VkBuffer buf;
	VkDeviceSize off;

	VkDeviceSize bytes = (VkDeviceSize)rowBytes * rows;
	VkBuf big = { 0 };

	Vkd_End();
	byte *dst = bytes <= VK_RING_SIZE / 4 ? Vk_RingAlloc(bytes, 16, &buf, &off) : NULL;
	if (!dst) {
		/* too large for the ring: a staging buffer of its own */
		if (!Vk_CreateBuffer(&big, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 1)) {
			return;
		}
		dst = big.map;
		buf = big.buf;
		off = 0;
	}
	for (uint32_t r = 0; r < rows; r++) {
		const byte *s = data + (size_t)r * pitch;
		byte *o = dst + (size_t)r * rowBytes;
		if (!f.convert) {
			memcpy(o, s, rowBytes);
			continue;
		}
		for (uint32_t c = 0; c < w; c++) {
			uint32_t v;
			if (fmt == D3DFMT_R5G6B5 || fmt == D3DFMT_X1R5G5B5 || fmt == D3DFMT_A1R5G5B5) {
				uint32_t p = (uint32_t)(s[2 * c] | s[2 * c + 1] << 8);
				if (fmt == D3DFMT_R5G6B5) {
					v = 0xff000000u | ((p >> 11) * 255 / 31) << 16 | ((p >> 5 & 63) * 255 / 63) << 8
						| (p & 31) * 255 / 31;
				} else {
					v = (p >> 15 ? 0xff000000u : 0) | ((p >> 10 & 31) * 255 / 31) << 16
						| ((p >> 5 & 31) * 255 / 31) << 8 | (p & 31) * 255 / 31;
				}
			} else if (f.bpp == 4) {
				memcpy(&v, s + 4 * c, 4);
			} else {
				v = Vko_ToBgra(s + c * f.bpp, fmt);
			}
			memcpy(o + 4 * c, &v, 4);
		}
	}
	VkCommandBuffer cmd = Vk_Cmd();
	VkImageLayout keep = im->layout;
	Vk_Transition(cmd, im, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VkBufferImageCopy copy = {
		.bufferOffset = off,
		.bufferRowLength = f.block ? 0 : (f.convert ? w : rowBytes / f.bpp),
		.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1 },
		.imageOffset = { (int32_t)x, (int32_t)y, 0 },
		.imageExtent = { w, h, 1 },
	};
	vkCmdCopyBufferToImage(cmd, buf, im->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	if (big.buf) {
		Vk_DeferBuffer(&big);
	}
	Vk_Transition(cmd, im, keep == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ? keep
		: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

/* reads the image of a GPU surface into memory in the D3D format fmt (pitch); waits for the GPU */
static void Vko_Readback(VkImg *im, uint32_t level, uint32_t fmt, byte *mem, uint32_t pitch, uint32_t w, uint32_t h) {
	VkBuf tmp;
	if (!Vk_CreateBuffer(&tmp, (VkDeviceSize)w * h * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 1)) {
		return;
	}
	Vkd_End();
	VkCommandBuffer cmd = Vk_Cmd();
	VkImageLayout keep = im->layout;
	Vk_Transition(cmd, im, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	VkBufferImageCopy copy = {
		.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1 },
		.imageExtent = { w, h, 1 },
	};
	vkCmdCopyImageToBuffer(cmd, im->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tmp.buf, 1, &copy);
	Vk_Transition(cmd, im, keep == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : keep);
	Vk_Flush();
	VkoFormat f = Vko_Format(fmt);
	uint32_t bpp = f.bpp ? f.bpp : 4;
	for (uint32_t r = 0; r < h; r++) {
		for (uint32_t c = 0; c < w; c++) {
			uint32_t v;
			memcpy(&v, tmp.map + ((size_t)r * w + c) * 4, 4);
			Vko_FromBgra(mem + (size_t)r * pitch + (size_t)c * bpp, v, fmt);
		}
	}
	Vk_DestroyBuffer(&tmp);
}

static int Vk_TexLockRect(void *tex, uint32_t level, void *locked, const void *rect, uint32_t flags) {
	VkoTex *t = tex;
	d3dLockedRect_t *lr = locked;
	(void)rect;
	if (level >= t->levels) {
		return D3DERR_INVALIDCALL;
	}
	VkoFormat f = Vko_Format(t->format);
	uint32_t w = Vko_Dim(t->width, level), h = Vko_Dim(t->height, level);
	uint32_t pitch = Vko_Pitch(&f, w);
	if (!t->lock[level]) {
		t->lock[level] = calloc(Vko_Rows(&f, h), pitch);
		if (!t->lock[level]) {
			return D3DERR_INVALIDCALL;
		}
	}
	if ((t->usage & D3DUSAGE_RENDERTARGET) && !f.block && !(flags & D3DLOCK_DISCARD)) {
		Vko_Readback(&t->img, level, t->format, t->lock[level], pitch, w, h);
	}
	t->lockPitch[level] = pitch;
	lr->Pitch = (int)pitch;
	lr->pBits = t->lock[level];
	return D3D_OK;
}

static int Vk_TexUnlockRect(void *tex, uint32_t level) {
	VkoTex *t = tex;
	if (level >= t->levels || !t->lock[level]) {
		return D3DERR_INVALIDCALL;
	}
	/* a system memory texture is only a source for UpdateTexture: it keeps its memory and never draws */
	if (t->pool == D3DPOOL_SYSTEMMEM) {
		return D3D_OK;
	}
	Vko_Upload(&t->img, level, t->format, t->lock[level], t->lockPitch[level], 0, 0, Vko_Dim(t->width, level),
		Vko_Dim(t->height, level));
	/* the memory of a managed texture stays for the next lock of its level, the others' goes */
	if (t->pool != D3DPOOL_MANAGED) {
		free(t->lock[level]);
		t->lock[level] = NULL;
	}
	return D3D_OK;
}

static uint32_t Vk_TexSetLOD(void *tex, uint32_t lod) {
	(void)tex, (void)lod;
	return 0;
}

static int Vk_DevUpdateTexture(void *dev, void *src, void *dst) {
	VkoTex *s = src, *t = dst;
	(void)dev;
	for (uint32_t l = 0; l < s->levels && l < t->levels; l++) {
		if (s->lock[l]) {
			Vko_Upload(&t->img, l, s->format, s->lock[l], s->lockPitch[l], 0, 0, Vko_Dim(s->width, l),
				Vko_Dim(s->height, l));
		}
	}
	return D3D_OK;
}

/* ---- surfaces ---- */

static int Vk_DevCreateImageSurface(void *dev, uint32_t w, uint32_t h, int fmt, void **surf) {
	(void)dev;
	VkoFormat f = Vko_Format((uint32_t)fmt);
	if (f.vk == VK_FORMAT_UNDEFINED) {
		return D3DERR_INVALIDCALL;
	}
	VkoSurf *s = Vko_New(sizeof(VkoSurf), VKO_SURF);
	if (!s) {
		return D3DERR_INVALIDCALL;
	}
	s->format = (uint32_t)fmt;
	s->width = w;
	s->height = h;
	s->pitch = Vko_Pitch(&f, w);
	s->mem = calloc(Vko_Rows(&f, h), s->pitch);
	if (!s->mem) {
		free(s);
		return D3DERR_INVALIDCALL;
	}
	*surf = s;
	return D3D_OK;
}

static int Vk_DevCreateDepthStencilSurface(void *dev, uint32_t w, uint32_t h, int fmt, int multisample, void **surf) {
	(void)dev, (void)multisample;
	VkoSurf *s = Vko_NewTargetSurf((uint32_t)fmt, w, h, 1);
	*surf = s;
	return s ? D3D_OK : D3DERR_INVALIDCALL;
}

static int Vk_SurfGetDesc(void *surf, void *desc) {
	VkoSurf *s = surf;
	d3dSurfaceDesc_t *d = desc;
	memset(d, 0, sizeof(*d));
	d->Format = (int)s->format;
	d->Type = 1;
	d->Usage = s->tex ? s->tex->usage : (s->img ? D3DUSAGE_RENDERTARGET : 0);
	d->Pool = s->mem && !s->img ? 2 : 0;
	d->Width = s->width;
	d->Height = s->height;
	return D3D_OK;
}

static int Vk_SurfGetDevice(void *surf, void **dev) {
	return Vk_TexGetDevice(surf, dev);
}

static int Vk_SurfLockRect(void *surf, void *lr, const void *rect, uint32_t flags) {
	VkoSurf *s = surf;
	d3dLockedRect_t *l = lr;
	(void)rect;
	if (s->tex) {
		return Vk_TexLockRect(s->tex, s->level, lr, rect, flags);
	}
	if (s->img) {
		/* the back buffer: a copy in memory, read from the GPU */
		VkoFormat f = Vko_Format(s->format);
		s->pitch = Vko_Pitch(&f, s->width);
		if (!s->mem) {
			s->mem = calloc(s->height, s->pitch);
			if (!s->mem) {
				return D3DERR_INVALIDCALL;
			}
		}
		Vko_Readback(s->img, 0, s->format, s->mem, s->pitch, s->width, s->height);
		s->lockedRead = !(flags & D3DLOCK_READONLY);
	}
	l->Pitch = (int)s->pitch;
	l->pBits = s->mem;
	return D3D_OK;
}

static int Vk_SurfUnlockRect(void *surf) {
	VkoSurf *s = surf;
	if (s->tex) {
		return Vk_TexUnlockRect(s->tex, s->level);
	}
	if (s->img && s->lockedRead) {
		Vko_Upload(s->img, 0, s->format, s->mem, s->pitch, 0, 0, s->width, s->height);
		s->lockedRead = 0;
	}
	return D3D_OK;
}

/* copies rect of src to point of dst: GPU to GPU on the GPU, memory to GPU as an upload, GPU to memory as a readback */
static void Vko_CopyRect(VkoSurf *src, const RECT *r, VkoSurf *dst, int dx, int dy) {
	uint32_t w = (uint32_t)(r->right - r->left), h = (uint32_t)(r->bottom - r->top);
	VkoFormat fs = Vko_Format(src->format), fd = Vko_Format(dst->format);
	uint32_t bs = fs.bpp ? fs.bpp : 4, bd = fd.bpp ? fd.bpp : 4;

	if (src->img && dst->img) {
		Vkd_End();
		VkCommandBuffer cmd = Vk_Cmd();
		VkImageLayout ks = src->img->layout, kd = dst->img->layout;
		Vk_Transition(cmd, src->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		Vk_Transition(cmd, dst->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		VkImageBlit b = {
			.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, src->level, 0, 1 },
			.srcOffsets = { { r->left, r->top, 0 }, { r->right, r->bottom, 1 } },
			.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, dst->level, 0, 1 },
			.dstOffsets = { { dx, dy, 0 }, { dx + (int32_t)w, dy + (int32_t)h, 1 } },
		};
		vkCmdBlitImage(cmd, src->img->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->img->img,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &b, VK_FILTER_NEAREST);
		Vk_Transition(cmd, src->img, ks == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : ks);
		Vk_Transition(cmd, dst->img, kd == VK_IMAGE_LAYOUT_UNDEFINED || kd == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : kd);
	} else if (src->img) {
		byte *tmp = malloc((size_t)src->width * src->height * bd);
		if (tmp) {
			Vko_Readback(src->img, src->level, dst->format, tmp, src->width * bd, src->width, src->height);
			for (uint32_t y = 0; y < h; y++) {
				memcpy(dst->mem + (size_t)(dy + (int32_t)y) * dst->pitch + (size_t)dx * bd,
					tmp + ((size_t)(r->top + (int32_t)y) * src->width + (size_t)r->left) * bd, (size_t)w * bd);
			}
			free(tmp);
		}
	} else if (dst->img) {
		Vko_Upload(dst->img, dst->level, src->format, src->mem + (size_t)r->top * src->pitch + (size_t)r->left * bs,
			src->pitch, (uint32_t)dx, (uint32_t)dy, w, h);
	} else {
		for (uint32_t y = 0; y < h; y++) {
			memcpy(dst->mem + (size_t)(dy + (int32_t)y) * dst->pitch + (size_t)dx * bd,
				src->mem + (size_t)(r->top + (int32_t)y) * src->pitch + (size_t)r->left * bs, (size_t)w * bs);
		}
	}
}

static int Vk_DevCopyRects(void *dev, void *src, const void *rects, uint32_t n, void *dst, const void *points) {
	VkoSurf *s = src, *d = dst;
	(void)dev;
	if (Vkd_Tracing()) {
		Com_Printf("copyrects %ux%u fmt %u -> %ux%u fmt %u n %u\n", s->width, s->height, s->format, d->width,
			d->height, d->format, n);
	}
	if (!rects || !n) {
		RECT all = { 0, 0, (LONG)s->width, (LONG)s->height };
		const POINT *p = points;
		Vko_CopyRect(s, &all, d, p ? p->x : 0, p ? p->y : 0);
		return D3D_OK;
	}
	for (uint32_t i = 0; i < n; i++) {
		const RECT *r = (const RECT *)rects + i;
		const POINT *p = points ? (const POINT *)points + i : NULL;
		Vko_CopyRect(s, r, d, p ? p->x : r->left, p ? p->y : r->top);
	}
	return D3D_OK;
}

/* ---- vertex and index buffers ---- */

static VkBufferUsageFlags Vko_BufUsage(const VkoBuf *b) {
	return b->o.kind == VKO_VB ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
}

static int Vko_NewBuf(uint32_t kind, uint32_t len, uint32_t usage, uint32_t fvf, int fmt, int pool, void **out) {
	VkoBuf *b = Vko_New(sizeof(VkoBuf), kind);
	if (!b) {
		return D3DERR_INVALIDCALL;
	}
	b->size = len;
	b->usage = usage;
	b->fvf = fvf;
	b->format = (uint32_t)fmt;
	b->pool = (uint32_t)pool;
	if (!Vk_CreateBuffer(&b->mem, len, Vko_BufUsage(b), 1)) {
		free(b);
		return D3DERR_INVALIDCALL;
	}
	*out = b;
	return D3D_OK;
}

static int Vk_DevCreateVertexBuffer(void *dev, uint32_t len, uint32_t usage, uint32_t fvf, int pool, void **vb) {
	(void)dev;
	return Vko_NewBuf(VKO_VB, len, usage, fvf, 0, pool, vb);
}

static int Vk_DevCreateIndexBuffer(void *dev, uint32_t len, uint32_t usage, int fmt, int pool, void **ib) {
	(void)dev;
	return Vko_NewBuf(VKO_IB, len, usage, 0, fmt, pool, ib);
}

/*
 * A DISCARD lock of a buffer the GPU may still read swaps in a spare version that it does not (a new one up to
 * VKO_BUF_SPARES, then it waits); NOOVERWRITE writes where it is; another lock of a busy buffer waits for the GPU.
 */
static int Vk_BufLock(void *buf, uint32_t off, uint32_t size, void *data, uint32_t flags) {
	VkoBuf *b = buf;
	int busy = b->used > vk.completed;

	if (busy && (flags & D3DLOCK_DISCARD)) {
		int i;
		for (i = 0; i < b->numSpares && b->spareUsed[i] > vk.completed; i++) {
		}
		if (i == b->numSpares && i < VKO_BUF_SPARES && Vk_CreateBuffer(&b->spare[i], b->size, Vko_BufUsage(b), 1)) {
			b->spareUsed[i] = 0;
			b->numSpares++;
		}
		if (i < b->numSpares) {
			VkBuf t = b->mem;
			b->mem = b->spare[i];
			b->spare[i] = t;
			b->spareUsed[i] = b->used;
			b->used = 0;
		} else {
			Vk_Flush();
		}
	} else if (busy && !(flags & (D3DLOCK_NOOVERWRITE | D3DLOCK_READONLY))) {
		Vk_Flush();
	}
	*(byte **)data = b->mem.map + off;
	(void)size;
	return D3D_OK;
}

static int Vk_BufUnlock(void *buf) {
	(void)buf;
	return D3D_OK;
}

/* ---- D3DX objects: their own vtables (the C of src/lib/d3dx8) ---- */

static void *Vk_XBufGetBufferPointer(void *buf) {
	return ((void *(__stdcall *)(void *))(*(void ***)buf)[0xc / 4])(buf);
}

static int Vk_XMeshDrawSubset(void *mesh, uint32_t attrib) {
	(void)mesh, (void)attrib;
	return D3D_OK;
}

static int Vk_XMeshGetAttributeTable(void *mesh, void *table, uint32_t *size) {
	(void)mesh, (void)table;
	*size = 0;
	return D3D_OK;
}

static uint32_t Vk_XMeshGetNumFaces(void *mesh) {
	(void)mesh;
	return 0;
}

static uint32_t Vk_XMeshGetNumVertices(void *mesh) {
	(void)mesh;
	return 0;
}

const RdBackend rd_vk = {
	.name = "vulkan",
	.Direct3DCreate8 = Vk_Direct3DCreate8,
	.AddRef = Vk_AddRef,
	.Release = Vk_Release,
	.D3DCheckDepthStencilMatch = Vk_D3DCheckDepthStencilMatch,
	.D3DCheckDeviceFormat = Vk_D3DCheckDeviceFormat,
	.D3DCheckDeviceMultiSampleType = Vk_D3DCheckDeviceMultiSampleType,
	.D3DCheckDeviceType = Vk_D3DCheckDeviceType,
	.D3DCreateDevice = Vk_D3DCreateDevice,
	.D3DEnumAdapterModes = Vk_D3DEnumAdapterModes,
	.D3DGetAdapterCount = Vk_D3DGetAdapterCount,
	.D3DGetAdapterDisplayMode = Vk_D3DGetAdapterDisplayMode,
	.D3DGetAdapterIdentifier = Vk_D3DGetAdapterIdentifier,
	.D3DGetAdapterModeCount = Vk_D3DGetAdapterModeCount,
	.D3DGetDeviceCaps = Vk_D3DGetDeviceCaps,
	.DevBeginScene = Vk_DevBeginScene,
	.DevClear = Vk_DevClear,
	.DevCopyRects = Vk_DevCopyRects,
	.DevCreateDepthStencilSurface = Vk_DevCreateDepthStencilSurface,
	.DevCreateImageSurface = Vk_DevCreateImageSurface,
	.DevCreateIndexBuffer = Vk_DevCreateIndexBuffer,
	.DevCreatePixelShader = Vk_DevCreatePixelShader,
	.DevCreateTexture = Vk_DevCreateTexture,
	.DevCreateVertexBuffer = Vk_DevCreateVertexBuffer,
	.DevDeletePixelShader = Vk_DevDeletePixelShader,
	.DevDrawIndexedPrimitive = Vk_DevDrawIndexedPrimitive,
	.DevDrawIndexedPrimitiveUP = Vk_DevDrawIndexedPrimitiveUP,
	.DevDrawPrimitive = Vk_DevDrawPrimitive,
	.DevDrawPrimitiveUP = Vk_DevDrawPrimitiveUP,
	.DevEndScene = Vk_DevEndScene,
	.DevGetAvailableTextureMem = Vk_DevGetAvailableTextureMem,
	.DevGetBackBuffer = Vk_DevGetBackBuffer,
	.DevGetDepthStencilSurface = Vk_DevGetDepthStencilSurface,
	.DevGetDeviceCaps = Vk_DevGetDeviceCaps,
	.DevGetDirect3D = Vk_DevGetDirect3D,
	.DevGetDisplayMode = Vk_DevGetDisplayMode,
	.DevGetRenderState = Vk_DevGetRenderState,
	.DevGetRenderTarget = Vk_DevGetRenderTarget,
	.DevGetTextureStageState = Vk_DevGetTextureStageState,
	.DevGetTransform = Vk_DevGetTransform,
	.DevGetViewport = Vk_DevGetViewport,
	.DevLightEnable = Vk_DevLightEnable,
	.DevPresent = Vk_DevPresent,
	.DevReset = Vk_DevReset,
	.DevSetCursorPosition = Vk_DevSetCursorPosition,
	.DevSetGammaRamp = Vk_DevSetGammaRamp,
	.DevSetIndices = Vk_DevSetIndices,
	.DevSetLight = Vk_DevSetLight,
	.DevSetMaterial = Vk_DevSetMaterial,
	.DevSetPixelShader = Vk_DevSetPixelShader,
	.DevSetPixelShaderConstant = Vk_DevSetPixelShaderConstant,
	.DevSetRenderState = Vk_DevSetRenderState,
	.DevSetRenderTarget = Vk_DevSetRenderTarget,
	.DevSetStreamSource = Vk_DevSetStreamSource,
	.DevSetTexture = Vk_DevSetTexture,
	.DevSetTextureStageState = Vk_DevSetTextureStageState,
	.DevSetTransform = Vk_DevSetTransform,
	.DevSetVertexShader = Vk_DevSetVertexShader,
	.DevSetViewport = Vk_DevSetViewport,
	.DevShowCursor = Vk_DevShowCursor,
	.DevTestCooperativeLevel = Vk_DevTestCooperativeLevel,
	.DevUpdateTexture = Vk_DevUpdateTexture,
	.TexGetDevice = Vk_TexGetDevice,
	.TexGetLevelCount = Vk_TexGetLevelCount,
	.TexGetLevelDesc = Vk_TexGetLevelDesc,
	.TexGetSurfaceLevel = Vk_TexGetSurfaceLevel,
	.TexLockRect = Vk_TexLockRect,
	.TexSetLOD = Vk_TexSetLOD,
	.TexUnlockRect = Vk_TexUnlockRect,
	.SurfGetDesc = Vk_SurfGetDesc,
	.SurfGetDevice = Vk_SurfGetDevice,
	.SurfLockRect = Vk_SurfLockRect,
	.SurfUnlockRect = Vk_SurfUnlockRect,
	.BufLock = Vk_BufLock,
	.BufUnlock = Vk_BufUnlock,
	.XBufGetBufferPointer = Vk_XBufGetBufferPointer,
	.XMeshDrawSubset = Vk_XMeshDrawSubset,
	.XMeshGetAttributeTable = Vk_XMeshGetAttributeTable,
	.XMeshGetNumFaces = Vk_XMeshGetNumFaces,
	.XMeshGetNumVertices = Vk_XMeshGetNumVertices,
};
