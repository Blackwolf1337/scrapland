/*
 * The texture functions of D3DX 8.1 the game calls (D3DXCheckTextureRequirements, D3DXCreateTextureFromFileInMemoryEx,
 * D3DXLoadSurfaceFromSurface), written from their documented behavior, not from the code of D3DX. The game loads
 * only DDS files through D3DX (DXT1, DXT3, DXT5, A8R8G8B8, R5G6B5, A4R4G4B4, X1R5G5B5); BMP and TGA go through its
 * own reader into an A8R8G8B8 surface and then D3DXLoadSurfaceFromSurface into the texture level.
 *
 * Pixels move as float RGBA texels (0..1), a band of rows at a time. Unsigned channels are rounded to the nearest
 * value. The signed (bump) formats map 0..1 to -1..1 like D3DX: U from red, V from green, W from blue, Q and the
 * luminance of L6V5U5 and X8L8V8U8 from alpha. DXT blocks are encoded with a range fit (the bounding box of the
 * colors, its diagonal chosen by the sign of the covariance, inset a little). Missing mip levels are box filtered
 * from the level above in the format of the texture, as D3DX does. Dithering (D3DX_FILTER_DITHER) is not done.
 *
 * The conversions between the uncompressed formats give the bytes of D3DX. What differs: DXT blocks it encodes
 * (another encoder), the DXT levels of a file smaller than 4 pixels (D3DX encodes them again, here the blocks of the
 * file are copied), the mip levels made here (D3DX rounds them on another path), and the scaled filters LINEAR,
 * TRIANGLE and BOX (only used when the size changes, which the game never asks for).
 */
#include <string.h>

#include "scrap/d3d8.h"

#include <windows.h>

#define D3DERR_NOTAVAILABLE ((int)0x8876086a)
#define D3DERR_INVALIDCALL ((int)0x8876086c)
#define D3DXERR_INVALIDDATA ((int)0x88760b59)
#define TEX_E_OUTOFMEMORY ((int)0x8007000e)

#define D3DX_DEFAULT 0xffffffffu
#define D3DX_FILTER_NONE 1
#define D3DX_FILTER_POINT 2
#define D3DX_FILTER_LINEAR 3
#define D3DX_FILTER_TRIANGLE 4
#define D3DX_FILTER_BOX 5

#define D3DUSAGE_VALID 0x20b		/* RENDERTARGET, DEPTHSTENCIL, WRITEONLY, DYNAMIC */
#define D3DUSAGE_DEPTHSTENCIL 2
#define D3DPOOL_SCRATCH 3
#define D3DRTYPE_TEXTURE 3
#define D3DLOCK_READONLY 0x10

/* IDirect3DDevice8, IDirect3D8, IDirect3DTexture8, IDirect3DSurface8 */
#define DEV_GETDIRECT3D 0x18
#define DEV_GETDEVICECAPS 0x1c
#define DEV_GETDISPLAYMODE 0x20
#define DEV_UPDATETEXTURE 0x74
#define D3D_CHECKDEVICEFORMAT 0x28
#define TEX_GETLEVELCOUNT 0x34
#define SURF_GETDESC 0x20
#define SURF_LOCKRECT 0x24
#define SURF_UNLOCKRECT 0x28
typedef int(__stdcall *devGetDirect3D_t)(void *dev, void **d3d);
typedef int(__stdcall *devGetCaps_t)(void *dev, D3DCaps8 *caps);
typedef int(__stdcall *devGetDisplayMode_t)(void *dev, D3DDisplayMode *mode);
typedef int(__stdcall *devUpdateTexture_t)(void *dev, void *src, void *dst);
typedef int(__stdcall *d3dCheckDeviceFormat_t)(void *d3d, uint32_t adapter, int devType, int adapterFmt, uint32_t usage,
	int rtype, int fmt);
typedef uint32_t(__stdcall *texGetLevelCount_t)(void *tex);
typedef int(__stdcall *surfGetDesc_t)(void *surf, d3dSurfaceDesc_t *desc);
typedef int(__stdcall *surfLockRect_t)(void *surf, d3dLockedRect_t *lr, const RECT *rect, uint32_t flags);
typedef int(__stdcall *surfUnlockRect_t)(void *surf);

#define TEXCAPS_POW2 0x2
#define TEXCAPS_SQUAREONLY 0x20
#define TEXCAPS_NONPOW2CONDITIONAL 0x100
#define TEXCAPS_MIPMAP 0x4000

/* ---- the formats ---- */

enum {
	FMT_UNKNOWN = 0,
	FMT_R8G8B8 = 20, FMT_A8R8G8B8, FMT_X8R8G8B8, FMT_R5G6B5, FMT_X1R5G5B5, FMT_A1R5G5B5, FMT_A4R4G4B4, FMT_R3G3B2,
	FMT_A8, FMT_A8R3G3B2, FMT_X4R4G4B4, FMT_A2B10G10R10, FMT_G16R16 = 34,
	FMT_A8P8 = 40, FMT_P8,
	FMT_L8 = 50, FMT_A8L8, FMT_A4L4,
	FMT_V8U8 = 60, FMT_L6V5U5, FMT_X8L8V8U8, FMT_Q8W8V8U8, FMT_V16U16, FMT_W11V11U10, FMT_A2W10V10U10 = 67,
	FMT_D16_LOCKABLE = 70, FMT_D32, FMT_D15S1 = 73, FMT_D24S8 = 75, FMT_D16 = 80, FMT_D24X8 = 77, FMT_D24X4S4 = 79,
	FMT_UYVY = 0x59565955, FMT_YUY2 = 0x32595559,
	FMT_DXT1 = 0x31545844, FMT_DXT2 = 0x32545844, FMT_DXT3 = 0x33545844, FMT_DXT4 = 0x34545844,
	FMT_DXT5 = 0x35545844,
};

/* what a format holds; a requested kind may become one of the kinds of texKindMatch */
enum { KIND_RGB, KIND_PALETTE, KIND_LUMINANCE, KIND_BUMP, KIND_DEPTH };
static const byte texKindMatch[5] = {
	1 << KIND_RGB, 1 << KIND_RGB | 1 << KIND_PALETTE, 1 << KIND_RGB | 1 << KIND_LUMINANCE, 1 << KIND_BUMP,
	1 << KIND_DEPTH
};

/* how the texels are stored */
enum { CODEC_NONE, CODEC_PACKED, CODEC_LUMINANCE, CODEC_SIGNED, CODEC_DXT };

typedef struct {
	int fmt;
	byte kind;
	byte bpp;		/* bits per pixel, the tie break of the closest format */
	byte pick;		/* may stand in for another format */
	byte bits[5];		/* channel bits compared by the closest format: rgb x a r g b, luminance x a l l l,
				   bump l q w v u, depth lockable . . stencil depth */
	byte codec;
	byte width[4];		/* the stored channels: r g b a (luminance l . . a, bump u v w q/l) */
	byte shift[4];
	byte unsigned3;		/* bump: the fourth channel is an unsigned luminance */
} texFormat_t;

static const texFormat_t texFormats[] = {
	{ FMT_R8G8B8, KIND_RGB, 24, 1, { 0, 0, 8, 8, 8 }, CODEC_PACKED, { 8, 8, 8, 0 }, { 16, 8, 0, 0 }, 0 },
	{ FMT_A8R8G8B8, KIND_RGB, 32, 1, { 0, 8, 8, 8, 8 }, CODEC_PACKED, { 8, 8, 8, 8 }, { 16, 8, 0, 24 }, 0 },
	{ FMT_X8R8G8B8, KIND_RGB, 32, 1, { 0, 0, 8, 8, 8 }, CODEC_PACKED, { 8, 8, 8, 0 }, { 16, 8, 0, 0 }, 0 },
	{ FMT_R5G6B5, KIND_RGB, 16, 1, { 0, 0, 5, 6, 5 }, CODEC_PACKED, { 5, 6, 5, 0 }, { 11, 5, 0, 0 }, 0 },
	{ FMT_X1R5G5B5, KIND_RGB, 16, 1, { 0, 0, 5, 5, 5 }, CODEC_PACKED, { 5, 5, 5, 0 }, { 10, 5, 0, 0 }, 0 },
	{ FMT_A8P8, KIND_PALETTE, 16, 1, { 0, 8, 8, 8, 8 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_P8, KIND_PALETTE, 8, 1, { 0, 8, 8, 8, 8 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_A1R5G5B5, KIND_RGB, 16, 1, { 0, 1, 5, 5, 5 }, CODEC_PACKED, { 5, 5, 5, 1 }, { 10, 5, 0, 15 }, 0 },
	{ FMT_A4R4G4B4, KIND_RGB, 16, 1, { 0, 4, 4, 4, 4 }, CODEC_PACKED, { 4, 4, 4, 4 }, { 8, 4, 0, 12 }, 0 },
	{ FMT_R3G3B2, KIND_RGB, 8, 1, { 0, 0, 3, 3, 2 }, CODEC_PACKED, { 3, 3, 2, 0 }, { 5, 2, 0, 0 }, 0 },
	{ FMT_A8, KIND_RGB, 8, 1, { 0, 8, 0, 0, 0 }, CODEC_PACKED, { 0, 0, 0, 8 }, { 0, 0, 0, 0 }, 0 },
	{ FMT_A8R3G3B2, KIND_RGB, 16, 1, { 0, 8, 3, 3, 2 }, CODEC_PACKED, { 3, 3, 2, 8 }, { 5, 2, 0, 8 }, 0 },
	{ FMT_X4R4G4B4, KIND_RGB, 16, 1, { 0, 0, 4, 4, 4 }, CODEC_PACKED, { 4, 4, 4, 0 }, { 8, 4, 0, 0 }, 0 },
	{ FMT_A2B10G10R10, KIND_RGB, 32, 1, { 0, 2, 10, 10, 10 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_G16R16, KIND_RGB, 32, 1, { 0, 0, 16, 16, 0 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_L8, KIND_LUMINANCE, 8, 1, { 0, 0, 8, 8, 8 }, CODEC_LUMINANCE, { 8, 0, 0, 0 }, { 0, 0, 0, 0 }, 0 },
	{ FMT_A8L8, KIND_LUMINANCE, 16, 1, { 0, 8, 8, 8, 8 }, CODEC_LUMINANCE, { 8, 0, 0, 8 }, { 0, 0, 0, 8 }, 0 },
	{ FMT_A4L4, KIND_LUMINANCE, 8, 1, { 0, 4, 4, 4, 4 }, CODEC_LUMINANCE, { 4, 0, 0, 4 }, { 0, 0, 0, 4 }, 0 },
	{ FMT_V8U8, KIND_BUMP, 16, 1, { 0, 0, 0, 8, 8 }, CODEC_SIGNED, { 8, 8, 0, 0 }, { 0, 8, 0, 0 }, 0 },
	{ FMT_L6V5U5, KIND_BUMP, 16, 1, { 6, 0, 0, 5, 5 }, CODEC_SIGNED, { 5, 5, 0, 6 }, { 0, 5, 0, 10 }, 1 },
	{ FMT_X8L8V8U8, KIND_BUMP, 32, 1, { 8, 0, 0, 8, 8 }, CODEC_SIGNED, { 8, 8, 0, 8 }, { 0, 8, 0, 16 }, 1 },
	{ FMT_Q8W8V8U8, KIND_BUMP, 32, 1, { 0, 8, 8, 8, 8 }, CODEC_SIGNED, { 8, 8, 8, 8 }, { 0, 8, 16, 24 }, 0 },
	{ FMT_V16U16, KIND_BUMP, 32, 1, { 0, 0, 0, 16, 16 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_W11V11U10, KIND_BUMP, 32, 1, { 0, 0, 11, 11, 10 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_A2W10V10U10, KIND_BUMP, 32, 1, { 2, 0, 10, 10, 10 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_UYVY, KIND_RGB, 16, 0, { 0, 0, 8, 8, 8 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_YUY2, KIND_RGB, 16, 0, { 0, 0, 8, 8, 8 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_DXT1, KIND_RGB, 16, 0, { 0, 1, 5, 6, 5 }, CODEC_DXT, { 0 }, { 0 }, 0 },
	{ FMT_DXT2, KIND_RGB, 16, 0, { 0, 4, 5, 6, 5 }, CODEC_DXT, { 0 }, { 0 }, 0 },
	{ FMT_DXT3, KIND_RGB, 16, 0, { 0, 4, 5, 6, 5 }, CODEC_DXT, { 0 }, { 0 }, 0 },
	{ FMT_DXT4, KIND_RGB, 16, 0, { 0, 8, 5, 6, 5 }, CODEC_DXT, { 0 }, { 0 }, 0 },
	{ FMT_DXT5, KIND_RGB, 16, 0, { 0, 8, 5, 6, 5 }, CODEC_DXT, { 0 }, { 0 }, 0 },
	{ FMT_D16_LOCKABLE, KIND_DEPTH, 16, 1, { 1, 0, 0, 0, 16 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_D32, KIND_DEPTH, 32, 1, { 0, 0, 0, 0, 32 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_D15S1, KIND_DEPTH, 16, 1, { 0, 0, 0, 1, 15 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_D24S8, KIND_DEPTH, 32, 1, { 0, 0, 0, 8, 24 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_D16, KIND_DEPTH, 16, 1, { 0, 0, 0, 0, 16 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_D24X8, KIND_DEPTH, 32, 1, { 0, 0, 0, 0, 24 }, CODEC_NONE, { 0 }, { 0 }, 0 },
	{ FMT_D24X4S4, KIND_DEPTH, 32, 1, { 0, 0, 0, 4, 24 }, CODEC_NONE, { 0 }, { 0 }, 0 },
};
#define TEX_FORMATS (int)(sizeof(texFormats) / sizeof(texFormats[0]))

/* an unknown format is looked for as A8R8G8B8 */
static const texFormat_t *Tex_Format(int fmt) {
	for (int i = 0; i < TEX_FORMATS; i++) {
		if (texFormats[i].fmt == fmt) {
			return &texFormats[i];
		}
	}
	return &texFormats[1];
}

static int Tex_IsDxt(int fmt) {
	return fmt == FMT_DXT1 || fmt == FMT_DXT2 || fmt == FMT_DXT3 || fmt == FMT_DXT4 || fmt == FMT_DXT5;
}

/* bytes of a row of pixels, of a row of 4x4 blocks for DXT */
static uint32_t Tex_RowBytes(const texFormat_t *f, uint32_t w) {
	if (f->codec == CODEC_DXT) {
		return (w + 3) / 4 * (f->fmt == FMT_DXT1 ? 8 : 16);
	}
	return w * (f->bpp / 8);
}

/* the rows of a level as Tex_RowBytes counts them */
static uint32_t Tex_Rows(const texFormat_t *f, uint32_t h) {
	return f->codec == CODEC_DXT ? (h + 3) / 4 : h;
}

/*
 * How far candidate c is from the requested format r, -1 when it can not stand in for it: per channel, a missing one
 * weighs most, then each bit fewer, then a channel r does not have, then each bit more.
 */
static int64_t Tex_FormatDistance(const texFormat_t *r, const texFormat_t *c) {
	int64_t d = 0;
	int want = 0, missing = 0;

	if (!(texKindMatch[r->kind] & 1 << c->kind)) {
		return -1;
	}
	for (int i = 0; i < 5; i++) {
		int rb = r->bits[i], cb = c->bits[i];
		if (rb) {
			want++;
		}
		if (rb > cb) {
			if (!cb) {
				missing++;
				d += 1 << 24;
			} else {
				d += (int64_t)(rb - cb) << 16;
			}
		} else if (rb < cb) {
			d += rb ? cb - rb : 0x100;
		}
	}
	return missing == want ? -1 : d;
}

/* the requested format when the device has it, else the closest one it has; 0 for none */
static int Tex_PickFormat(void *dev, uint32_t usage, int fmt) {
	const texFormat_t *req = Tex_Format(fmt);
	const texFormat_t *best = NULL;
	void *d3d = NULL;
	D3DCaps8 caps;
	D3DDisplayMode mode;
	int64_t bestDist = -1;

	if (dev) {
		D3D_METHOD(dev, devGetDirect3D_t, DEV_GETDIRECT3D)(dev, &d3d);
		D3D_METHOD(dev, devGetCaps_t, DEV_GETDEVICECAPS)(dev, &caps);
		D3D_METHOD(dev, devGetDisplayMode_t, DEV_GETDISPLAYMODE)(dev, &mode);
	}
	for (int i = 0; i < TEX_FORMATS; i++) {
		const texFormat_t *c = &texFormats[i];
		if (d3d && D3D_METHOD(d3d, d3dCheckDeviceFormat_t, D3D_CHECKDEVICEFORMAT)(d3d, caps.AdapterOrdinal,
			(int)caps.DeviceType, (int)mode.Format, usage, D3DRTYPE_TEXTURE, c->fmt) < 0) {
			continue;
		}
		if (c->fmt == req->fmt) {
			best = c;
			break;
		}
		if (!c->pick) {
			continue;
		}
		int64_t d = Tex_FormatDistance(req, c);
		if (d < 0) {
			continue;
		}
		if (!best || d < bestDist || (d == bestDist && c->bpp < best->bpp)) {
			best = c;
			bestDist = d;
		}
	}
	if (d3d) {
		COM_RELEASE(d3d);
	}
	return best ? best->fmt : 0;
}

static uint32_t Tex_Pow2(uint32_t v) {
	uint32_t p = 1;
	while (p < v) {
		p <<= 1;
	}
	return p;
}

/* the levels of a full chain: the bits of the larger side */
static uint32_t Tex_ChainLevels(uint32_t w, uint32_t h) {
	uint32_t n = 0, m = w > h ? w : h;
	for (; m; m >>= 1) {
		n++;
	}
	return n;
}

/*
 * D3DXCheckTextureRequirements: the format the device has (closest to the one asked for, A8R8G8B8 for none), the
 * size clamped to the caps (256 when neither side is given, the other side when one is missing, at least 1), the
 * aspect ratio limit, powers of 2 when the device needs them, multiples of 4 for DXT, and the levels: a full
 * chain for 0 or more than it has, 1 without mipmaps (or with conditional non-power-of-2 sizes).
 */
int __stdcall D3DX_CheckTextureRequirements(void *dev, uint32_t *pw, uint32_t *ph, uint32_t *plevels, uint32_t usage,
	int *pfmt, int pool) {
	D3DCaps8 caps;
	uint32_t w, h, levels;
	int fmt;

	if (usage == D3DX_DEFAULT) {
		usage = 0;
	}
	if (!dev || (usage & ~D3DUSAGE_VALID) || (uint32_t)pool > D3DPOOL_SCRATCH) {
		return D3DERR_INVALIDCALL;
	}
	fmt = pfmt ? *pfmt : FMT_UNKNOWN;
	if (pool != D3DPOOL_SCRATCH) {
		fmt = Tex_PickFormat(dev, usage, fmt);
		if (!fmt) {
			return D3DERR_NOTAVAILABLE;
		}
	} else if (fmt == FMT_UNKNOWN) {
		fmt = FMT_A8R8G8B8;
	}
	w = pw ? *pw : D3DX_DEFAULT;
	h = ph ? *ph : D3DX_DEFAULT;
	if (w == D3DX_DEFAULT && h == D3DX_DEFAULT) {
		w = h = 256;
	} else {
		if (w == D3DX_DEFAULT) {
			w = h;
		} else if (h == D3DX_DEFAULT) {
			h = w;
		}
		if (!w) {
			w = 1;
		}
		if (!h) {
			h = 1;
		}
	}
	levels = plevels ? *plevels : D3DX_DEFAULT;
	if (pool != D3DPOOL_SCRATCH) {
		D3D_METHOD(dev, devGetCaps_t, DEV_GETDEVICECAPS)(dev, &caps);
		uint32_t texCaps = caps.TextureCaps;
		if (w > caps.MaxTextureWidth) {
			w = caps.MaxTextureWidth;
		}
		if (h > caps.MaxTextureHeight) {
			h = caps.MaxTextureHeight;
		}
		uint32_t ratio = texCaps & TEXCAPS_SQUAREONLY ? 1 : caps.MaxTextureAspectRatio;
		if (ratio) {
			if (w > ratio * h) {
				w = ratio * h;
			}
			if (h > ratio * w) {
				h = ratio * w;
			}
		}
		if (texCaps & TEXCAPS_POW2) {
			w = Tex_Pow2(w);
			h = Tex_Pow2(h);
		}
		if (Tex_IsDxt(fmt)) {
			w = (w + 3) & ~3u;
			h = (h + 3) & ~3u;
		}
		if (!(texCaps & TEXCAPS_MIPMAP)
			|| ((texCaps & TEXCAPS_NONPOW2CONDITIONAL) && ((w & (w - 1)) || (h & (h - 1))))) {
			levels = 1;
		}
	} else if (Tex_IsDxt(fmt)) {
		w = (w + 3) & ~3u;
		h = (h + 3) & ~3u;
	}
	uint32_t chain = Tex_ChainLevels(w, h);
	if (!levels || levels > chain) {
		levels = chain;
	}
	if (pw) {
		*pw = w;
	}
	if (ph) {
		*ph = h;
	}
	if (plevels) {
		*plevels = levels;
	}
	if (pfmt) {
		*pfmt = fmt;
	}
	return 0;
}

/* ---- texels ---- */

/* r g b a in 0..1; a signed channel s is held as (s + 1) / 2 */
typedef struct {
	float c[4];
} texel_t;

static void *Tex_Alloc(size_t n) {
	return HeapAlloc(GetProcessHeap(), 0, n ? n : 1);
}

static void Tex_Free(void *p) {
	if (p) {
		HeapFree(GetProcessHeap(), 0, p);
	}
}

static uint32_t Tex_ReadWord(const byte *p, int bytes) {
	uint32_t v = 0;
	for (int i = 0; i < bytes; i++) {
		v |= (uint32_t)p[i] << (8 * i);
	}
	return v;
}

static void Tex_WriteWord(byte *p, int bytes, uint32_t v) {
	for (int i = 0; i < bytes; i++) {
		p[i] = (byte)(v >> (8 * i));
	}
}

/* 0..1 in n bits, rounded */
static uint32_t Tex_Quantize(float x, int n) {
	uint32_t m = (1u << n) - 1;
	if (!(x > 0.0f)) {
		return 0;
	}
	if (x >= 1.0f) {
		return m;
	}
	return (uint32_t)((double)x * m + 0.5);
}

/* 0..1 as -1..1 in n bits: trunc(s * 2^(n-1) + 0.5) in two's complement, as D3DX (8 bits give x - 127 for x of
 * 0..255; 1 wraps to the lowest value) */
static uint32_t Tex_QuantizeSigned(float x, int n) {
	double s = 2.0 * x - 1.0;
	s = s < -1.0 ? -1.0 : s > 1.0 ? 1.0 : s;
	return (uint32_t)(int)(s * (1 << (n - 1)) + 0.5) & ((1u << n) - 1);
}

static float Tex_Expand(uint32_t v, int n) {
	return (float)((double)v / (double)((1u << n) - 1));
}

static float Tex_ExpandSigned(uint32_t v, int n) {
	int half = 1 << (n - 1);
	int s = (int)(v & ((1u << n) - 1));
	if (s >= half) {
		s -= 2 * half;
	}
	return (float)(((double)s / half + 1.0) * 0.5);
}

/* the luminance of Rec. 709, as D3DX */
static float Tex_Luminance(const texel_t *t) {
	return (float)(0.2125 * t->c[0] + 0.7154 * t->c[1] + 0.0721 * t->c[2]);
}

static void Tex_DecodePixel(const texFormat_t *f, const byte *p, texel_t *t) {
	uint32_t v = Tex_ReadWord(p, f->bpp / 8);
	float c[4];

	for (int i = 0; i < 4; i++) {
		uint32_t x = f->width[i] ? v >> f->shift[i] & ((1u << f->width[i]) - 1) : 0;
		if (!f->width[i]) {
			c[i] = 0.0f;
		} else if (f->codec == CODEC_SIGNED && !(i == 3 && f->unsigned3)) {
			c[i] = Tex_ExpandSigned(x, f->width[i]);
		} else {
			c[i] = Tex_Expand(x, f->width[i]);
		}
	}
	switch (f->codec) {
	case CODEC_LUMINANCE:
		t->c[0] = t->c[1] = t->c[2] = c[0];
		t->c[3] = f->width[3] ? c[3] : 1.0f;
		break;
	case CODEC_SIGNED:
		/* a missing signed channel is 0, the middle */
		t->c[0] = c[0];
		t->c[1] = f->width[1] ? c[1] : 0.5f;
		t->c[2] = f->width[2] ? c[2] : 0.5f;
		t->c[3] = f->width[3] ? c[3] : 1.0f;
		break;
	default:
		t->c[0] = c[0];
		t->c[1] = c[1];
		t->c[2] = c[2];
		t->c[3] = f->width[3] ? c[3] : 1.0f;
		break;
	}
}

static void Tex_EncodePixel(const texFormat_t *f, const texel_t *t, byte *p) {
	uint32_t v = 0;

	for (int i = 0; i < 4; i++) {
		uint32_t x;
		if (!f->width[i]) {
			continue;
		}
		if (f->codec == CODEC_LUMINANCE && i == 0) {
			x = Tex_Quantize(Tex_Luminance(t), f->width[0]);
		} else if (f->codec == CODEC_SIGNED && !(i == 3 && f->unsigned3)) {
			x = Tex_QuantizeSigned(t->c[i], f->width[i]);
		} else {
			x = Tex_Quantize(t->c[i], f->width[i]);
		}
		v |= x << f->shift[i];
	}
	Tex_WriteWord(p, f->bpp / 8, v);
}

/* ---- DXT ---- */

static void Tex_From565(uint32_t c, texel_t *t) {
	t->c[0] = Tex_Expand(c >> 11 & 31, 5);
	t->c[1] = Tex_Expand(c >> 5 & 63, 6);
	t->c[2] = Tex_Expand(c & 31, 5);
	t->c[3] = 1.0f;
}

static void Tex_MixTexel(const texel_t *a, const texel_t *b, double wa, double wb, texel_t *out) {
	for (int k = 0; k < 4; k++) {
		out->c[k] = (float)((a->c[k] * wa + b->c[k] * wb) / (wa + wb));
	}
}

/* the four colors of a color block; three and transparent black when c0 <= c1 and three are allowed */
static void Tex_Palette(uint32_t c0, uint32_t c1, int three, texel_t pal[4]) {
	Tex_From565(c0, &pal[0]);
	Tex_From565(c1, &pal[1]);
	if (c0 > c1 || !three) {
		Tex_MixTexel(&pal[0], &pal[1], 2, 1, &pal[2]);
		Tex_MixTexel(&pal[0], &pal[1], 1, 2, &pal[3]);
	} else {
		Tex_MixTexel(&pal[0], &pal[1], 1, 1, &pal[2]);
		pal[3] = (texel_t){ { 0.0f, 0.0f, 0.0f, 0.0f } };
	}
}

static void Tex_AlphaPalette(int a0, int a1, float pal[8]) {
	pal[0] = Tex_Expand((uint32_t)a0, 8);
	pal[1] = Tex_Expand((uint32_t)a1, 8);
	if (a0 > a1) {
		for (int i = 1; i < 7; i++) {
			pal[i + 1] = (float)((pal[0] * (7 - i) + pal[1] * i) / 7.0);
		}
	} else {
		for (int i = 1; i < 5; i++) {
			pal[i + 1] = (float)((pal[0] * (5 - i) + pal[1] * i) / 5.0);
		}
		pal[6] = 0.0f;
		pal[7] = 1.0f;
	}
}

/* a block into 16 texels (rows of 4) */
static void Tex_DecodeBlock(int fmt, const byte *p, texel_t t[16]) {
	texel_t pal[4];
	const byte *c = fmt == FMT_DXT1 ? p : p + 8;

	Tex_Palette(Tex_ReadWord(c, 2), Tex_ReadWord(c + 2, 2), fmt == FMT_DXT1, pal);
	uint32_t idx = Tex_ReadWord(c + 4, 4);
	for (int i = 0; i < 16; i++) {
		t[i] = pal[idx >> (2 * i) & 3];
	}
	if (fmt == FMT_DXT2 || fmt == FMT_DXT3) {
		for (int i = 0; i < 16; i++) {
			t[i].c[3] = Tex_Expand(p[i / 2] >> (4 * (i & 1)) & 15, 4);
		}
	} else if (fmt == FMT_DXT4 || fmt == FMT_DXT5) {
		float apal[8];
		uint64_t bits = 0;
		Tex_AlphaPalette(p[0], p[1], apal);
		for (int i = 0; i < 6; i++) {
			bits |= (uint64_t)p[2 + i] << (8 * i);
		}
		for (int i = 0; i < 16; i++) {
			t[i].c[3] = apal[bits >> (3 * i) & 7];
		}
	}
}

/* the encoder works on 0..255 */
typedef struct {
	int c[4];
} texInt_t;

static int Tex_Dist(const texInt_t *a, const texInt_t *b) {
	int dr = a->c[0] - b->c[0], dg = a->c[1] - b->c[1], db = a->c[2] - b->c[2];
	return dr * dr + dg * dg + db * db;
}

static uint32_t Tex_To565(const int c[3]) {
	return Tex_Quantize(c[0] / 255.0f, 5) << 11 | Tex_Quantize(c[1] / 255.0f, 6) << 5
		| Tex_Quantize(c[2] / 255.0f, 5);
}

/* the color block of 16 texels: the bounding box, on the diagonal the covariance points along, inset by 1/16 */
static void Tex_EncodeColors(const texInt_t t[16], int dxt1, byte *out) {
	int lo[3] = { 255, 255, 255 }, hi[3] = { 0, 0, 0 }, sum[3] = { 0, 0, 0 }, n = 0, transparent = 0;
	texel_t pal[4];
	texInt_t ipal[4];

	for (int i = 0; i < 16; i++) {
		if (dxt1 && t[i].c[3] < 128) {
			transparent = 1;
			continue;
		}
		for (int k = 0; k < 3; k++) {
			int v = t[i].c[k];
			lo[k] = v < lo[k] ? v : lo[k];
			hi[k] = v > hi[k] ? v : hi[k];
			sum[k] += v;
		}
		n++;
	}
	if (!n) {
		/* all transparent */
		Tex_WriteWord(out, 2, 0);
		Tex_WriteWord(out + 2, 2, 0);
		Tex_WriteWord(out + 4, 4, 0xffffffff);
		return;
	}
	/* the channel with the largest range leads, the others go down the box where they fall as it rises */
	int lead = 0;
	for (int k = 1; k < 3; k++) {
		if (hi[k] - lo[k] > hi[lead] - lo[lead]) {
			lead = k;
		}
	}
	for (int k = 0; k < 3; k++) {
		int cov = 0;
		if (k == lead) {
			continue;
		}
		for (int i = 0; i < 16; i++) {
			if (!(dxt1 && t[i].c[3] < 128)) {
				cov += (t[i].c[lead] * n - sum[lead]) * (t[i].c[k] * n - sum[k]) / 256;
			}
		}
		if (cov < 0) {
			int s = lo[k];
			lo[k] = hi[k];
			hi[k] = s;
		}
	}
	for (int k = 0; k < 3; k++) {
		int inset = (hi[k] - lo[k]) / 16;
		hi[k] -= inset;
		lo[k] += inset;
	}
	uint32_t c0 = Tex_To565(hi), c1 = Tex_To565(lo);
	/* four colors need c0 > c1, three and transparent black c0 <= c1 */
	if ((transparent && c0 > c1) || (!transparent && c0 < c1)) {
		uint32_t s = c0;
		c0 = c1;
		c1 = s;
	}
	Tex_Palette(c0, c1, dxt1, pal);
	for (int k = 0; k < 4; k++) {
		for (int j = 0; j < 4; j++) {
			ipal[k].c[j] = (int)Tex_Quantize(pal[k].c[j], 8);
		}
	}
	uint32_t idx = 0;
	for (int i = 0; i < 16; i++) {
		int best = 0;
		if (transparent && t[i].c[3] < 128) {
			best = 3;
		} else if (c0 != c1) {
			int bd = Tex_Dist(&t[i], &ipal[0]);
			for (int k = 1; k < (transparent ? 3 : 4); k++) {
				int d = Tex_Dist(&t[i], &ipal[k]);
				if (d < bd) {
					bd = d;
					best = k;
				}
			}
		}
		idx |= (uint32_t)best << (2 * i);
	}
	Tex_WriteWord(out, 2, c0);
	Tex_WriteWord(out + 2, 2, c1);
	Tex_WriteWord(out + 4, 4, idx);
}

static void Tex_EncodeBlock(int fmt, const texel_t ft[16], byte *out) {
	texInt_t t[16];

	for (int i = 0; i < 16; i++) {
		for (int k = 0; k < 4; k++) {
			t[i].c[k] = (int)Tex_Quantize(ft[i].c[k], 8);
		}
	}
	if (fmt == FMT_DXT1) {
		Tex_EncodeColors(t, 1, out);
		return;
	}
	if (fmt == FMT_DXT2 || fmt == FMT_DXT3) {
		memset(out, 0, 8);
		for (int i = 0; i < 16; i++) {
			out[i / 2] |= (byte)(Tex_Quantize(ft[i].c[3], 4) << (4 * (i & 1)));
		}
	} else {
		int lo = 255, hi = 0;
		float apal[8];
		uint64_t bits = 0;
		for (int i = 0; i < 16; i++) {
			lo = t[i].c[3] < lo ? t[i].c[3] : lo;
			hi = t[i].c[3] > hi ? t[i].c[3] : hi;
		}
		/* eight alphas from hi down to lo (a0 > a1); all index 0 when they are the same */
		Tex_AlphaPalette(hi, lo, apal);
		for (int i = 0; i < 16 && hi != lo; i++) {
			int best = 0;
			float bd = 2.0f;
			for (int k = 0; k < 8; k++) {
				float d = apal[k] > ft[i].c[3] ? apal[k] - ft[i].c[3] : ft[i].c[3] - apal[k];
				if (d < bd) {
					bd = d;
					best = k;
				}
			}
			bits |= (uint64_t)best << (3 * i);
		}
		out[0] = (byte)hi;
		out[1] = (byte)lo;
		for (int i = 0; i < 6; i++) {
			out[2 + i] = (byte)(bits >> (8 * i));
		}
	}
	Tex_EncodeColors(t, 0, out + 8);
}

/* ---- rows of texels ---- */

/* a level of a surface or texture as it is locked */
typedef struct {
	const texFormat_t *f;
	byte *bits;
	int pitch;
	uint32_t w, h;
} texLevel_t;

/* rows y0..y0+n-1 of a level into out (w texels a row) */
static void Tex_DecodeRows(const texLevel_t *l, uint32_t y0, uint32_t n, texel_t *out) {
	for (uint32_t y = y0; y < y0 + n; y++) {
		texel_t *row = out + (y - y0) * l->w;
		if (l->f->codec == CODEC_DXT) {
			texel_t blk[16];
			int bsize = l->f->fmt == FMT_DXT1 ? 8 : 16;
			const byte *brow = l->bits + (y / 4) * l->pitch;
			for (uint32_t bx = 0; bx < l->w; bx += 4) {
				Tex_DecodeBlock(l->f->fmt, brow + (bx / 4) * bsize, blk);
				for (uint32_t x = 0; x < 4 && bx + x < l->w; x++) {
					row[bx + x] = blk[(y & 3) * 4 + x];
				}
			}
		} else {
			const byte *p = l->bits + y * l->pitch;
			for (uint32_t x = 0; x < l->w; x++) {
				Tex_DecodePixel(l->f, p + x * (l->f->bpp / 8), &row[x]);
			}
		}
	}
}

/* rows y0..y0+n-1 of a level from in; DXT takes a row of blocks (y0 a multiple of 4), the rows below the level and
 * the columns right of it repeat the last ones */
static void Tex_EncodeRows(const texLevel_t *l, uint32_t y0, uint32_t n, const texel_t *in) {
	if (l->f->codec == CODEC_DXT) {
		texel_t blk[16];
		int bsize = l->f->fmt == FMT_DXT1 ? 8 : 16;
		byte *brow = l->bits + (y0 / 4) * l->pitch;
		for (uint32_t bx = 0; bx < l->w; bx += 4) {
			for (uint32_t y = 0; y < 4; y++) {
				uint32_t sy = y < n ? y : n - 1;
				for (uint32_t x = 0; x < 4; x++) {
					uint32_t sx = bx + x < l->w ? bx + x : l->w - 1;
					blk[y * 4 + x] = in[sy * l->w + sx];
				}
			}
			Tex_EncodeBlock(l->f->fmt, blk, brow + (bx / 4) * bsize);
		}
		return;
	}
	for (uint32_t y = y0; y < y0 + n; y++) {
		byte *p = l->bits + y * l->pitch;
		const texel_t *row = in + (y - y0) * l->w;
		for (uint32_t x = 0; x < l->w; x++) {
			Tex_EncodePixel(l->f, &row[x], p + x * (l->f->bpp / 8));
		}
	}
}

/* the rows written at once */
static uint32_t Tex_Band(const texLevel_t *l) {
	return l->f->codec == CODEC_DXT ? 4 : 1;
}

/* the color key (A8R8G8B8) becomes transparent black, compared in 8 bits */
static void Tex_ColorKey(texel_t *t, uint32_t n, uint32_t key) {
	if (!key) {
		return;
	}
	for (uint32_t i = 0; i < n; i++) {
		uint32_t v = Tex_Quantize(t[i].c[3], 8) << 24 | Tex_Quantize(t[i].c[0], 8) << 16
			| Tex_Quantize(t[i].c[1], 8) << 8 | Tex_Quantize(t[i].c[2], 8);
		if (v == key) {
			t[i] = (texel_t){ { 0.0f, 0.0f, 0.0f, 0.0f } };
		}
	}
}

/* the average of the texels of img (w wide) in [x0, x1) x [y0, y1) */
static texel_t Tex_Average(const texel_t *img, uint32_t w, uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1) {
	double sum[4] = { 0.0, 0.0, 0.0, 0.0 };
	double n = (double)((x1 - x0) * (y1 - y0));
	texel_t r;

	for (uint32_t y = y0; y < y1; y++) {
		for (uint32_t x = x0; x < x1; x++) {
			for (int k = 0; k < 4; k++) {
				sum[k] += img[y * w + x].c[k];
			}
		}
	}
	for (int k = 0; k < 4; k++) {
		r.c[k] = (float)(sum[k] / n);
	}
	return r;
}

static texel_t Tex_Bilinear(const texel_t *img, uint32_t sw, uint32_t sh, uint32_t dw, uint32_t dh, uint32_t x,
	uint32_t y) {
	double fx = ((double)x + 0.5) * sw / dw - 0.5, fy = ((double)y + 0.5) * sh / dh - 0.5;
	fx = fx < 0.0 ? 0.0 : fx;
	fy = fy < 0.0 ? 0.0 : fy;
	uint32_t x0 = (uint32_t)fx, y0 = (uint32_t)fy;
	uint32_t x1 = x0 + 1 < sw ? x0 + 1 : x0, y1 = y0 + 1 < sh ? y0 + 1 : y0;
	double ax = fx - x0, ay = fy - y0;
	texel_t r;
	for (int k = 0; k < 4; k++) {
		double top = img[y0 * sw + x0].c[k] * (1.0 - ax) + img[y0 * sw + x1].c[k] * ax;
		double bot = img[y1 * sw + x0].c[k] * (1.0 - ax) + img[y1 * sw + x1].c[k] * ax;
		r.c[k] = (float)(top * (1.0 - ay) + bot * ay);
	}
	return r;
}

/* pixel (x, y) of a dw x dh picture of img (sw x sh) with a D3DX filter */
static texel_t Tex_Sample(const texel_t *img, uint32_t sw, uint32_t sh, uint32_t dw, uint32_t dh, uint32_t x,
	uint32_t y, uint32_t filter) {
	switch (filter) {
	case D3DX_FILTER_NONE:
		/* no scaling: outside the source is transparent black */
		if (x < sw && y < sh) {
			return img[y * sw + x];
		}
		return (texel_t){ { 0.0f, 0.0f, 0.0f, 0.0f } };
	case D3DX_FILTER_POINT:
		return img[(y * sh / dh) * sw + x * sw / dw];
	case D3DX_FILTER_LINEAR:
		return Tex_Bilinear(img, sw, sh, dw, dh, x, y);
	default:
		if (dw > sw || dh > sh) {
			return Tex_Bilinear(img, sw, sh, dw, dh, x, y);
		} else {
			uint32_t x0 = x * sw / dw, x1 = (x + 1) * sw / dw, y0 = y * sh / dh, y1 = (y + 1) * sh / dh;
			return Tex_Average(img, sw, x0, x1 > x0 ? x1 : x0 + 1, y0, y1 > y0 ? y1 : y0 + 1);
		}
	}
}

/*
 * src into dst, converted; scaled with filter when the sizes differ. The same size goes a band of rows at a time, a
 * scaled one decodes the whole source first.
 */
static int Tex_Convert(const texLevel_t *src, const texLevel_t *dst, uint32_t filter, uint32_t colorKey) {
	uint32_t band = Tex_Band(dst);
	texel_t *rows, *img = NULL;
	int same = src->w == dst->w && src->h == dst->h;

	if (src->f->codec == CODEC_NONE || dst->f->codec == CODEC_NONE) {
		return D3DERR_INVALIDCALL;
	}
	rows = Tex_Alloc((size_t)dst->w * band * sizeof(texel_t));
	if (!same) {
		img = Tex_Alloc((size_t)src->w * src->h * sizeof(texel_t));
	}
	if (!rows || (!same && !img)) {
		Tex_Free(rows);
		Tex_Free(img);
		return TEX_E_OUTOFMEMORY;
	}
	if (!same) {
		Tex_DecodeRows(src, 0, src->h, img);
		Tex_ColorKey(img, src->w * src->h, colorKey);
	}
	for (uint32_t y = 0; y < dst->h; y += band) {
		uint32_t n = dst->h - y < band ? dst->h - y : band;
		if (same) {
			Tex_DecodeRows(src, y, n, rows);
			Tex_ColorKey(rows, n * dst->w, colorKey);
		} else {
			for (uint32_t r = 0; r < n; r++) {
				for (uint32_t x = 0; x < dst->w; x++) {
					rows[r * dst->w + x] = Tex_Sample(img, src->w, src->h, dst->w, dst->h, x, y + r, filter);
				}
			}
		}
		Tex_EncodeRows(dst, y, n, rows);
	}
	Tex_Free(rows);
	Tex_Free(img);
	return 0;
}

/* the next mip level from the one above in the same format: 2x2 averages (2x1 or 1x2 at a side of 1 pixel), the
 * top left pixel of each for NONE and POINT */
static int Tex_MipDown(const texLevel_t *src, const texLevel_t *dst, uint32_t filter) {
	uint32_t band = Tex_Band(dst);
	texel_t *rows = Tex_Alloc((size_t)dst->w * band * sizeof(texel_t));
	texel_t *up = Tex_Alloc((size_t)src->w * band * 2 * sizeof(texel_t));

	if (src->f->codec == CODEC_NONE) {
		Tex_Free(rows);
		Tex_Free(up);
		return D3DERR_INVALIDCALL;
	}
	if (!rows || !up) {
		Tex_Free(rows);
		Tex_Free(up);
		return TEX_E_OUTOFMEMORY;
	}
	int boxed = filter != D3DX_FILTER_NONE && filter != D3DX_FILTER_POINT;
	for (uint32_t y = 0; y < dst->h; y += band) {
		uint32_t n = dst->h - y < band ? dst->h - y : band;
		uint32_t sy = src->h > 1 ? 2 * y : 0, sn = src->h > 1 ? 2 * n : 1;
		if (sy + sn > src->h) {
			sn = src->h - sy;
		}
		Tex_DecodeRows(src, sy, sn, up);
		for (uint32_t r = 0; r < n; r++) {
			uint32_t r0 = src->h > 1 ? 2 * r : 0, r1 = src->h > 1 && 2 * r + 1 < sn ? 2 * r + 2 : r0 + 1;
			for (uint32_t x = 0; x < dst->w; x++) {
				uint32_t x0 = src->w > 1 ? 2 * x : 0, x1 = src->w > 1 && 2 * x + 1 < src->w ? 2 * x + 2 : x0 + 1;
				rows[r * dst->w + x] = boxed ? Tex_Average(up, src->w, x0, x1, r0, r1) : up[r0 * src->w + x0];
			}
		}
		Tex_EncodeRows(dst, y, n, rows);
	}
	Tex_Free(rows);
	Tex_Free(up);
	return 0;
}

/* the filter without its flags; the default for none */
static uint32_t Tex_Filter(uint32_t filter, uint32_t def) {
	filter &= 0xff;
	return filter >= D3DX_FILTER_NONE && filter <= D3DX_FILTER_BOX ? filter : def;
}

/* ---- surfaces ---- */

/*
 * D3DXLoadSurfaceFromSurface: the source rectangle into the target rectangle (whole surfaces for NULL), converted
 * and, when the sizes differ, scaled with the filter. The palettes are not used (no palette formats here).
 */
int __stdcall D3DX_LoadSurfaceFromSurface(void *dst, const void *dstPal, const void *dstRectArg, void *src,
	const void *srcPal, const void *srcRectArg, uint32_t filter, uint32_t colorKey) {
	const RECT *dstRect = dstRectArg, *srcRect = srcRectArg;
	d3dSurfaceDesc_t sd, dd;
	d3dLockedRect_t sl, dl;
	RECT sr, dr;
	int hr;

	(void)dstPal;
	(void)srcPal;
	if (!dst || !src) {
		return D3DERR_INVALIDCALL;
	}
	if ((hr = D3D_METHOD(src, surfGetDesc_t, SURF_GETDESC)(src, &sd)) < 0
		|| (hr = D3D_METHOD(dst, surfGetDesc_t, SURF_GETDESC)(dst, &dd)) < 0) {
		return hr;
	}
	sr = srcRect ? *srcRect : (RECT){ 0, 0, (LONG)sd.Width, (LONG)sd.Height };
	dr = dstRect ? *dstRect : (RECT){ 0, 0, (LONG)dd.Width, (LONG)dd.Height };
	if (sr.left < 0 || sr.top < 0 || sr.right <= sr.left || sr.bottom <= sr.top || (uint32_t)sr.right > sd.Width
		|| (uint32_t)sr.bottom > sd.Height || dr.left < 0 || dr.top < 0 || dr.right <= dr.left
		|| dr.bottom <= dr.top || (uint32_t)dr.right > dd.Width || (uint32_t)dr.bottom > dd.Height) {
		return D3DERR_INVALIDCALL;
	}
	texLevel_t s = { Tex_Format(sd.Format), NULL, 0, (uint32_t)(sr.right - sr.left), (uint32_t)(sr.bottom - sr.top) };
	texLevel_t d = { Tex_Format(dd.Format), NULL, 0, (uint32_t)(dr.right - dr.left), (uint32_t)(dr.bottom - dr.top) };
	if (s.f->fmt != sd.Format || d.f->fmt != dd.Format || s.f->codec == CODEC_NONE || d.f->codec == CODEC_NONE) {
		return D3DERR_INVALIDCALL;
	}
	/* DXT rectangles start on a block */
	if ((s.f->codec == CODEC_DXT && ((sr.left | sr.top) & 3)) || (d.f->codec == CODEC_DXT && ((dr.left | dr.top) & 3))) {
		return D3DERR_INVALIDCALL;
	}
	if ((hr = D3D_METHOD(src, surfLockRect_t, SURF_LOCKRECT)(src, &sl, srcRect, D3DLOCK_READONLY)) < 0) {
		return hr;
	}
	if ((hr = D3D_METHOD(dst, surfLockRect_t, SURF_LOCKRECT)(dst, &dl, dstRect, 0)) < 0) {
		D3D_METHOD(src, surfUnlockRect_t, SURF_UNLOCKRECT)(src);
		return hr;
	}
	s.bits = sl.pBits;
	s.pitch = sl.Pitch;
	d.bits = dl.pBits;
	d.pitch = dl.Pitch;
	hr = Tex_Convert(&s, &d, Tex_Filter(filter, D3DX_FILTER_TRIANGLE), colorKey);
	D3D_METHOD(dst, surfUnlockRect_t, SURF_UNLOCKRECT)(dst);
	D3D_METHOD(src, surfUnlockRect_t, SURF_UNLOCKRECT)(src);
	return hr;
}

/* ---- DDS files ---- */

#define DDS_MAGIC 0x20534444	/* "DDS " */
#define DDS_HEADER 128
#define DDPF_ALPHAPIXELS 0x1
#define DDPF_ALPHA 0x2
#define DDPF_FOURCC 0x4
#define DDPF_RGB 0x40
#define DDPF_LUMINANCE 0x20000
#define DDPF_BUMPDUDV 0x80000
#define DDSCAPS2_CUBEMAP 0x200
#define DDSCAPS2_VOLUME 0x200000

/* an image file: DDS (its levels in place) or TGA (one level, decoded into a buffer of its own) */
typedef struct {
	uint32_t w, h, levels;
	int fmt;
	const byte *data;	/* the first level */
	uint32_t size;		/* of the levels */
	byte *owned;		/* the decoded TGA */
} texFile_t;

/* the D3DFORMAT of a DDS pixel format: its FourCC or the masks of a known format */
static int Dds_Format(const byte *pf) {
	uint32_t flags = Tex_ReadWord(pf + 4, 4), four = Tex_ReadWord(pf + 8, 4), bits = Tex_ReadWord(pf + 12, 4);
	uint32_t m[4] = { Tex_ReadWord(pf + 16, 4), Tex_ReadWord(pf + 20, 4), Tex_ReadWord(pf + 24, 4),
		Tex_ReadWord(pf + 28, 4) };

	if (flags & DDPF_FOURCC) {
		return Tex_IsDxt((int)four) ? (int)four : FMT_UNKNOWN;
	}
	if (!(flags & DDPF_ALPHAPIXELS)) {
		m[3] = 0;
	}
	if (flags & DDPF_ALPHA) {
		return bits == 8 && m[3] == 0xff ? FMT_A8 : FMT_UNKNOWN;
	}
	if (!(flags & (DDPF_RGB | DDPF_LUMINANCE | DDPF_BUMPDUDV))) {
		return FMT_UNKNOWN;
	}
	for (int i = 0; i < TEX_FORMATS; i++) {
		const texFormat_t *f = &texFormats[i];
		uint32_t fm[4];
		if ((f->codec != CODEC_PACKED && f->codec != CODEC_LUMINANCE && f->codec != CODEC_SIGNED) || f->bpp != bits
			|| ((flags & DDPF_LUMINANCE) != 0) != (f->codec == CODEC_LUMINANCE)
			|| ((flags & DDPF_BUMPDUDV) != 0) != (f->codec == CODEC_SIGNED)) {
			continue;
		}
		for (int k = 0; k < 4; k++) {
			fm[k] = f->width[k] ? ((1u << f->width[k]) - 1) << f->shift[k] : 0;
		}
		if (fm[0] == m[0] && fm[1] == m[1] && fm[2] == m[2] && fm[3] == m[3]) {
			return f->fmt;
		}
	}
	return FMT_UNKNOWN;
}

/* the header; the levels are its mip count (with or without the flag of the count, as D3DX), as far as the data goes */
static int Dds_Parse(const void *data, uint32_t size, texFile_t *dds) {
	const byte *p = data;

	if (size < DDS_HEADER || Tex_ReadWord(p, 4) != DDS_MAGIC || Tex_ReadWord(p + 4, 4) != 124) {
		return 0;
	}
	if (Tex_ReadWord(p + 112, 4) & (DDSCAPS2_CUBEMAP | DDSCAPS2_VOLUME)) {
		return 0;
	}
	dds->h = Tex_ReadWord(p + 12, 4);
	dds->w = Tex_ReadWord(p + 16, 4);
	dds->fmt = Dds_Format(p + 76);
	if (!dds->w || !dds->h || dds->fmt == FMT_UNKNOWN) {
		return 0;
	}
	dds->data = p + DDS_HEADER;
	dds->size = size - DDS_HEADER;
	uint32_t want = Tex_ReadWord(p + 28, 4), n = 0, at = 0, w = dds->w, h = dds->h;
	const texFormat_t *f = Tex_Format(dds->fmt);
	if (!want) {
		want = 1;
	}
	while (n < want) {
		uint32_t len = Tex_RowBytes(f, w) * Tex_Rows(f, h);
		if (at + len > dds->size || at + len < at) {
			break;
		}
		at += len;
		n++;
		if (w == 1 && h == 1) {
			break;
		}
		w = w > 1 ? w / 2 : 1;
		h = h > 1 ? h / 2 : 1;
	}
	dds->levels = n;
	return n > 0;
}

#define TGA_HEADER 18

/* one pixel of a TGA in the layout of fmt; 0 when the data ends */
static int Tga_Pixel(const byte **p, const byte *end, int bytes, const byte *pal, int palBytes, uint32_t palFirst,
	uint32_t palLen, int outBytes, byte *out) {
	if (*p + bytes > end) {
		return 0;
	}
	if (pal) {
		uint32_t i = Tex_ReadWord(*p, bytes) - palFirst;
		if (i >= palLen) {
			return 0;
		}
		memcpy(out, pal + i * (uint32_t)palBytes, (size_t)outBytes);
	} else {
		memcpy(out, *p, (size_t)outBytes);
	}
	*p += bytes;
	return 1;
}

/*
 * TGA (the game has one, bmp/norm1.alpha.psa, through D3DX): colormapped, truecolor and gray, plain or RLE, in the
 * file's format (A8R8G8B8 or X8R8G8B8 for 32 bits, R8G8B8, A1R5G5B5 or X1R5G5B5, L8; a colormap becomes the format
 * of its entries), rows top down.
 */
static int Tga_Parse(const void *data, uint32_t size, texFile_t *file) {
	const byte *p = data, *end = p + size;
	int type, bpp, alpha, palBits = 0, bytes, outBytes;
	uint32_t palFirst = 0, palLen = 0;
	const byte *pal = NULL;

	if (size < TGA_HEADER) {
		return 0;
	}
	type = p[2] & ~8;
	bpp = p[16];
	alpha = p[17] & 15;
	file->w = Tex_ReadWord(p + 12, 2);
	file->h = Tex_ReadWord(p + 14, 2);
	if ((type != 1 && type != 2 && type != 3) || (p[2] & ~11) || !file->w || !file->h || p[1] > 1
		|| (type == 1) != (p[1] == 1)) {
		return 0;
	}
	if (type == 1) {
		palFirst = Tex_ReadWord(p + 3, 2);
		palLen = Tex_ReadWord(p + 5, 2);
		palBits = p[7];
		if (bpp != 8 && bpp != 16) {
			return 0;
		}
	}
	int pixBits = type == 1 ? palBits : bpp;
	switch (pixBits) {
	case 32:
		file->fmt = alpha ? FMT_A8R8G8B8 : FMT_X8R8G8B8;
		break;
	case 24:
		file->fmt = FMT_R8G8B8;
		break;
	case 15:
	case 16:
		file->fmt = type == 3 ? FMT_A8L8 : alpha ? FMT_A1R5G5B5 : FMT_X1R5G5B5;
		break;
	case 8:
		file->fmt = type == 3 ? FMT_L8 : FMT_UNKNOWN;
		break;
	default:
		file->fmt = FMT_UNKNOWN;
		break;
	}
	if (file->fmt == FMT_UNKNOWN) {
		return 0;
	}
	bytes = (bpp + 7) / 8;
	outBytes = (pixBits + 7) / 8;
	p += TGA_HEADER + p[0];
	if (type == 1) {
		pal = p;
		p += palLen * (uint32_t)outBytes;
	}
	if (p > end) {
		return 0;
	}
	file->owned = Tex_Alloc((size_t)file->w * file->h * (size_t)outBytes);
	if (!file->owned) {
		return 0;
	}
	int rle = (((const byte *)data)[2] & 8) != 0, topDown = (((const byte *)data)[17] & 0x20) != 0;
	int rightToLeft = (((const byte *)data)[17] & 0x10) != 0;
	uint32_t run = 0, raw = 0;
	byte px[4];
	for (uint32_t y = 0; y < file->h; y++) {
		byte *row = file->owned + (topDown ? y : file->h - 1 - y) * file->w * (uint32_t)outBytes;
		for (uint32_t x = 0; x < file->w; x++) {
			byte *out = row + (rightToLeft ? file->w - 1 - x : x) * (uint32_t)outBytes;
			if (rle && !run && !raw) {
				if (p >= end) {
					goto fail;
				}
				byte h = *p++;
				if (h & 0x80) {
					run = (h & 0x7fu) + 1;
					if (!Tga_Pixel(&p, end, bytes, pal, outBytes, palFirst, palLen, outBytes, px)) {
						goto fail;
					}
				} else {
					raw = (h & 0x7fu) + 1;
				}
			}
			if (run) {
				memcpy(out, px, (size_t)outBytes);
				run--;
			} else {
				if (!Tga_Pixel(&p, end, bytes, pal, outBytes, palFirst, palLen, outBytes, out)) {
					goto fail;
				}
				if (raw) {
					raw--;
				}
			}
		}
	}
	file->data = file->owned;
	file->size = file->w * file->h * (uint32_t)outBytes;
	file->levels = 1;
	return 1;
fail:
	Tex_Free(file->owned);
	file->owned = NULL;
	return 0;
}

static int Tex_ParseFile(const void *data, uint32_t size, texFile_t *file) {
	memset(file, 0, sizeof(*file));
	return Dds_Parse(data, size, file) || Tga_Parse(data, size, file);
}

/* D3DXIMAGE_INFO of D3DX 8 */
typedef struct {
	uint32_t Width;
	uint32_t Height;
	uint32_t Depth;
	uint32_t MipLevels;
	int Format;
} d3dxImageInfo_t;

#define TEX_LOCKRECT 0x40
#define TEX_UNLOCKRECT 0x44
typedef int(__stdcall *texLockRect_t)(void *tex, uint32_t level, d3dLockedRect_t *lr, const RECT *rect, uint32_t flags);
typedef int(__stdcall *texUnlockRect_t)(void *tex, uint32_t level);

/* lock level lv of tex as l */
static int Tex_LockLevel(void *tex, uint32_t lv, int fmt, uint32_t flags, texLevel_t *l) {
	d3dSurfaceDesc_t desc;
	d3dLockedRect_t lr;
	int hr;

	if ((hr = D3D_METHOD(tex, d3dGetLevelDesc_t, D3DTEX_GETLEVELDESC)(tex, lv, &desc)) < 0
		|| (hr = D3D_METHOD(tex, texLockRect_t, TEX_LOCKRECT)(tex, lv, &lr, NULL, flags)) < 0) {
		return hr;
	}
	*l = (texLevel_t){ Tex_Format(fmt), lr.pBits, lr.Pitch, desc.Width, desc.Height };
	return 0;
}

/*
 * The levels of tex from the file: a level the file has in the same size is copied when the format is the same, else
 * converted; the first level is scaled with filter when its size differs. The levels below are box filtered with
 * mipFilter from the level above, in the format of the texture as D3DX did.
 */
static int Tex_FillLevels(void *tex, const texFile_t *dds, int fmt, uint32_t filter, uint32_t mipFilter,
	uint32_t colorKey) {
	const texFormat_t *sf = Tex_Format(dds->fmt);
	uint32_t levels = D3D_METHOD(tex, texGetLevelCount_t, TEX_GETLEVELCOUNT)(tex);
	texLevel_t file = { sf, (byte *)dds->data, 0, dds->w, dds->h };
	int hr = 0;

	for (uint32_t lv = 0; lv < levels && hr >= 0; lv++) {
		texLevel_t l, up;
		if ((hr = Tex_LockLevel(tex, lv, fmt, 0, &l)) < 0) {
			break;
		}
		file.pitch = (int)Tex_RowBytes(sf, file.w);
		if (lv < dds->levels && (lv == 0 || (l.w == file.w && l.h == file.h))) {
			if (sf == l.f && !colorKey && l.w == file.w && l.h == file.h) {
				for (uint32_t y = 0; y < Tex_Rows(sf, file.h); y++) {
					memcpy(l.bits + y * l.pitch, file.bits + y * file.pitch, (size_t)file.pitch);
				}
			} else {
				hr = Tex_Convert(&file, &l, filter, colorKey);
			}
		} else if ((hr = Tex_LockLevel(tex, lv - 1, fmt, D3DLOCK_READONLY, &up)) >= 0) {
			hr = Tex_MipDown(&up, &l, mipFilter);
			D3D_METHOD(tex, texUnlockRect_t, TEX_UNLOCKRECT)(tex, lv - 1);
		}
		D3D_METHOD(tex, texUnlockRect_t, TEX_UNLOCKRECT)(tex, lv);
		if (lv < dds->levels) {
			file.bits += file.pitch * Tex_Rows(sf, file.h);
			file.w = file.w > 1 ? file.w / 2 : 1;
			file.h = file.h > 1 ? file.h / 2 : 1;
		}
	}
	return hr;
}

/*
 * D3DXCreateTextureFromFileInMemoryEx for DDS and TGA files: the size, levels and format of the file where the
 * arguments are 0 or D3DX_DEFAULT (a full chain for levels), adjusted by D3DX_CheckTextureRequirements, then the
 * levels. P8 has no palette in these files and becomes X8R8G8B8 (D3DX did the same on a device without P8 textures).
 * A texture of the default pool is filled through one in system memory and UpdateTexture.
 */
int __stdcall D3DX_CreateTextureFromFileInMemoryEx(void *dev, const void *data, uint32_t size, uint32_t w, uint32_t h,
	uint32_t levels, uint32_t usage, int fmt, int pool, uint32_t filter, uint32_t mipFilter, uint32_t colorKey,
	void *srcInfo, void *palette, void **out) {
	texFile_t file;
	void *tex = NULL, *fill = NULL;
	int hr;

	(void)palette;
	if (!dev || !data || !size || !out) {
		return D3DERR_INVALIDCALL;
	}
	*out = NULL;
	if (!Tex_ParseFile(data, size, &file)) {
		return D3DXERR_INVALIDDATA;
	}
	if (srcInfo) {
		d3dxImageInfo_t *info = srcInfo;
		info->Width = file.w;
		info->Height = file.h;
		info->Depth = 1;
		info->MipLevels = file.levels;
		info->Format = file.fmt;
	}
	if (!w || w == D3DX_DEFAULT) {
		w = file.w;
	}
	if (!h || h == D3DX_DEFAULT) {
		h = file.h;
	}
	if (levels == D3DX_DEFAULT) {
		levels = 0;
	}
	if (fmt == FMT_UNKNOWN) {
		fmt = file.fmt;
	} else if (fmt == FMT_P8) {
		fmt = FMT_X8R8G8B8;
	} else if (fmt == FMT_A8P8) {
		fmt = FMT_A8R8G8B8;
	}
	if ((hr = D3DX_CheckTextureRequirements(dev, &w, &h, &levels, usage, &fmt, pool)) < 0
		|| (hr = D3D_METHOD(dev, d3dCreateTexture_t, D3DDEV_CREATETEXTURE)(dev, w, h, levels, usage, fmt, pool,
			&tex)) < 0) {
		goto done;
	}
	fill = tex;
	if (pool == D3DPOOL_DEFAULT && (hr = D3D_METHOD(dev, d3dCreateTexture_t, D3DDEV_CREATETEXTURE)(dev, w, h, levels,
		0, fmt, D3DPOOL_SYSTEMMEM, &fill)) < 0) {
		fill = NULL;
		goto done;
	}
	hr = Tex_FillLevels(fill, &file, fmt, Tex_Filter(filter, D3DX_FILTER_TRIANGLE),
		Tex_Filter(mipFilter, D3DX_FILTER_BOX), colorKey);
	if (hr >= 0 && fill != tex) {
		hr = D3D_METHOD(dev, devUpdateTexture_t, DEV_UPDATETEXTURE)(dev, fill, tex);
	}
done:
	if (fill && fill != tex) {
		COM_RELEASE(fill);
	}
	if (hr < 0 && tex) {
		COM_RELEASE(tex);
	} else if (hr >= 0) {
		*out = tex;
	}
	Tex_Free(file.owned);
	return hr < 0 ? hr : 0;
}
