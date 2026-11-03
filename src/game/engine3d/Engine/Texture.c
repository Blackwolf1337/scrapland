/*
 * The texture cache of the engine3d, original source Engine/Textura.cpp (include/scrap/texture.h): the list of the
 * CTexture entries, their files, the Direct3D 8 textures made from them (by hand from a CBmp, or by D3DX from the
 * raw file), the bump and DUDV conversions, the DDS export of the developer tools and two CBmp helpers of bmp.h.
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/gvar.h"
#include "scrap/archio.h"
#include "scrap/console.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/texture.h"
#include "scrap/staticinit.h"

#define BMP_PIXEL 4

/* D3DFORMAT */
#define FMT_R8G8B8 0x14
#define FMT_A8R8G8B8 0x15
#define FMT_A1R5G5B5 0x19
#define FMT_A4R4G4B4 0x1a
#define FMT_P8 0x29
#define FMT_DXT1 0x31545844
#define FMT_DXT5 0x35545844
#define D3DX_FILTER_DITHER_DIFFUSION 0x80000
#define D3DX_FILTER_POINT 2
#define D3DX_FILTER_TRIANGLE 4
#define D3DX_FILTER_BOX 5
#define P_WAIT 0

/* the frame counter of the engine */
static cGVar GVar_R_Tex_Count;

/* entries alive (engine variable R_Tex_Count) */
#define g_TexCount (GVar_R_Tex_Count.value.i)
/* statistics: device textures made from raw files, raw files with a device texture, entries used in the frame and
 * their pixels */
extern int g_TexCreated;
extern int g_TexCached;
extern int g_TexUsed;
extern int g_TexUsedPixels;
/* keep the CBmp pixels after the device texture is made */
/* the [video] settings Sharpen, ErrDif, TextBPP, TextOpBPP */
extern int g_Sharpen;
#define g_TexSharpen g_Sharpen
extern int g_TexErrDif;
extern int g_TextBPP;
static cGVar GVar_SharpenMore;

/* engine variables */
#define g_SharpenMore (GVar_SharpenMore.value.i)
static cGVar GVar_MipmapFade;
#define g_MipmapFade (GVar_MipmapFade.value.i)
static cGVar GVar_MipmapColor;
#define g_MipmapColor (GVar_MipmapColor.value.i)
static cGVar GVar_CachedTex;
#define g_CachedTex (GVar_CachedTex.value.i)
static cGVar GVar_BumpMipmap;
#define g_BumpMipmap (GVar_BumpMipmap.value.i)
static cGVar GVar_DUDVMipmap;
#define g_DUDVMipmap (GVar_DUDVMipmap.value.i)
cGVar GVar_R_PShaders;

#define g_PShaders (GVar_R_PShaders.value.i)
extern cGVar GVar_LoadTextures;
#define g_LoadTextures (GVar_LoadTextures.value.i)
extern cGVar GVar_TexturePool;
#define g_TexturePool (GVar_TexturePool.value.i)
static cGVar GVar_BuildDDS;
#define g_BuildDDS (GVar_BuildDDS.value.i)
static cGVar GVar_TextureLOD;
#define g_TextureLOD (GVar_TextureLOD.value.i)
static cGVar GVar_BumpDDS;
#define g_BumpDDS (GVar_BumpDDS.value.i)
static cGVar GVar_SaveDXT5;
#define g_SaveDXT5 (GVar_SaveDXT5.value.i)

/* kernel32 of the exe: the names compare in the user's locale */

/* the texture and surface methods */

static void Tex_ReleaseD3D(CTexture *t) {
	if (t->tex) {
		Rd_Release(t->tex);
		t->tex = NULL;
	}
}

/* the uncompressed formats: R8G8B8, X8R8G8B8, A8R8G8B8, R5G6B5, A4R4G4B4 and the bump formats */
static int Tex_FormatRGB(int bpp, int alpha, int bump) {
	if (bump) {
		if (bpp == 24 || bpp == 32) {
			return alpha ? 0x3e : 0x3f;
		}
		return alpha ? 0x3d : 0x3c;
	}
	if (bpp == 24) {
		return alpha ? 0x15 : 0x14;
	}
	if (bpp == 32) {
		return alpha ? 0x15 : 0x16;
	}
	return alpha ? 0x1a : 0x17;
}

static int g_TexFixedBPP = 24;

static int Tex_BPP(const CTexture *t) {
	return t->flags & TEXF_FIXEDBPP ? g_TexFixedBPP : t->bpp;
}

/* the filters of D3DX: TRIANGLE for the image, BOX for the mipmaps */
static uint32_t Tex_Dither(void) {
	return g_TexErrDif ? D3DX_FILTER_DITHER_DIFFUSION : 0;
}

static const char *Tex_OpacName(const CTexture *t) {
	return t->Opac[0] ? Path_FileName(t->Opac) : "none";
}

/* bmp.h */
static CBmp *CBmp_ctor(CBmp *self, int width, int height) {
	self->width = width;
	self->height = height;
	self->img = NULL;
	self->img = New_Array((uint32_t)width * height, BMP_PIXEL);
	return self;
}

static CBmp *CBmp_Delete(CBmp *self, byte flags) {
	Img_Free(self, BMP_PIXEL);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CBmp));
	}
	return self;
}

static CTexture *g_TexList;

static CTexture *CTexture_ctor(CTexture *self, const char *name, const char *opac, int kind, uint32_t flags) {
	CMultiSprite_ctor(&self->multi);
	g_TexCount++;
	self->Refs = 0;
	self->Name[0] = 0;
	self->Opac[0] = 0;
	CHECK(name && strlen(name) < sizeof(self->Name));
	CHECK(!opac || strlen(opac) < sizeof(self->Opac));
	if (name) {
		Str_Copy(self->Name, name, sizeof(self->Name));
	}
	if (opac) {
		Str_Copy(self->Opac, opac, sizeof(self->Opac));
	}
	self->kind = kind;
	self->width = 0;
	self->height = 0;
	self->bpp = 0;
	self->flags = flags;
	self->alpha = 0;
	self->tex = NULL;
	self->bitmap = NULL;
	self->PngData = NULL;
	self->PngSize = 0;
	self->frame = 0;
	self->mipmap = !(flags & TEXF_NOMIPMAP);
	self->bump = 0;
	self->dudv = 0;
	self->cube = 0;
	self->compressed = 0;
	self->failed = 0;
	self->ddx = 0;
	self->ddy = 0;
	self->ddv = 0;
	self->unused_245 = 0;
	if (flags & TEXF_FIXEDBPP) {
		self->mipmap = 0;
	}
	self->next = g_TexList;
	g_TexList = self;
	return self;
}

static void CTexture_dtor(CTexture *self) {
	g_TexCount--;
	CHECK(!self->Refs);
	Tex_ReleaseD3D(self);
	if (self->bitmap) {
		CBmp_Delete(self->bitmap, 1);
		self->bitmap = NULL;
	}
	Ptr_Delete(&self->PngData);
	if (g_TexList == self) {
		g_TexList = self->next;
	} else {
		for (CTexture *t = g_TexList; t; t = t->next) {
			if (t->next == self) {
				t->next = self->next;
			}
		}
	}
	CMultiSprite_dtor(&self->multi);
}

static void Texture_Free(CTexture *t) {
	CTexture_dtor(t);
	operator_delete_sized(t, sizeof(CTexture));
}

/* the least recently used raw file texture when more than g_CachedTex have a device texture */
static CTexture *Texture_FindPurge(void) {
	CTexture *best = NULL;
	int n = 0;
	int frame = g_FrameCount;

	for (CTexture *t = g_TexList; t; t = t->next) {
		if (!t->PngData || !t->tex) {
			continue;
		}
		if (best || frame > (int)((uint32_t)t->frame + 100)) {
			if (!best) {
				best = t;
			}
			if (best->frame > t->frame) {
				best = t;
			}
		}
		n++;
	}
	g_TexCached = n;
	if (best && n > g_CachedTex) {
		CHECK(best->PngData);
		CHECK(best->tex);
		return best;
	}
	return NULL;
}

static int g_TexMipFadeOn = 1;

/* src scaled into mipmap level and the levels below */
static int Surface_FromBmp(void *surf, const CBmp *bmp);
static int CTexture_UploadLevel(CTexture *self, CBmp *src, int level) {
	void *dev = NULL;
	void *surf = NULL;
	void *dst;
	d3dSurfaceDesc_t desc;
	CBmp *p;
	int hr;

	hr = Rd_TexGetDevice(self->tex, &dev);
	if (hr < 0) {
		goto out;
	}
	hr = Rd_TexGetLevelDesc(self->tex, level, &desc);
	if (hr < 0) {
		goto out;
	}
	hr = Rd_DevCreateImageSurface(dev, desc.Width, desc.Height, FMT_A8R8G8B8, &surf);
	if (hr < 0) {
		goto out;
	}
	if (g_TexSharpen && level > 0 && g_SharpenMore) {
		CBmp_Sharpen(src);
	}
	p = operator_new(sizeof(CBmp));
	if (p) {
		p = CBmp_ctor(p, desc.Width, desc.Height);
	}
	CBmp_Resample(p, src, 0xf);
	if (g_TexSharpen && level > 0) {
		CBmp_Sharpen(p);
	}
	if (g_TexMipFadeOn && level > 0 && g_MipmapFade) {
		CBmp fade;
		fade.width = p->width;
		fade.height = p->height;
		fade.img = New_Array((uint32_t)fade.width * fade.height, BMP_PIXEL);
		CBmp_Fill(&fade, 0x64c8ff);
		CBmp_Blend(p, 0, 0, &fade, 0, 0, fade.width, fade.height, 10, 0x100);
		Img_Free(&fade, BMP_PIXEL);
	}
	if (g_MipmapColor && level > 0) {
		/* green, blue, red for the levels 1, 2, 3.. */
		uint32_t color = level == 1 ? 0x8000ff00 : level == 2 ? 0x80ff0000 : 0x800000ff;
		CBmp_Fill(p, color);
	}
	hr = Surface_FromBmp(surf, p);
	if (hr >= 0) {
		hr = Rd_TexGetSurfaceLevel(self->tex, level, &dst);
		if (hr >= 0) {
			hr = D3DX_LoadSurfaceFromSurface(dst, NULL, NULL, surf, NULL, NULL, Tex_Dither() | D3DX_FILTER_POINT, 0);
			Rd_Release(dst);
			if (hr < 0) {
				E3D_LogBlue((&g_FileLog), "D3DXLoadSurfaceFromSurface: hr: 0x%08x", hr);
			} else {
				if (surf) {
					Rd_Release(surf);
					surf = NULL;
				}
				if (self->mipmap) {
					level++;
					if (Rd_TexGetLevelCount(self->tex) > (uint32_t)level) {
						hr = CTexture_UploadLevel(self, p, level);
					}
				}
			}
		}
	}
	if (p) {
		CBmp_Delete(p, 1);
	}
out:
	if (surf) {
		Rd_Release(surf);
		surf = NULL;
	}
	if (dev) {
		Rd_Release(dev);
	}
	return hr;
}

static int CTexture_LoadFile(CTexture *self, byte load);
static CTexture *Texture_Lookup(const char *name, const char *opac);
int Texture_Add(const char *name, const char *opac, int kind, uint32_t flags) {
	CTexture *t;

	if (!name) {
		return TEX_E_INVALIDARG;
	}
	if (Texture_Lookup(name, opac)) {
		return 0;
	}
	t = operator_new(sizeof(CTexture));
	if (t) {
		t = CTexture_ctor(t, name, opac, kind, flags);
	}
	if (!t) {
		return TEX_E_OUTOFMEMORY;
	}
	if (CTexture_LoadFile(t, 0) < 0) {
		Texture_Free(t);
		return TEX_E_FAIL;
	}
	return 0;
}

int Texture_Release(CTexture *t) {
	if (!t) {
		return -1;
	}
	CHECK(t->Refs > 0);
	t->Refs--;
	return t->Refs;
}

int Texture_DeleteAll(void) {
	while (g_TexList) {
		Texture_Free(g_TexList);
	}
	return 0;
}

void Texture_Delete(CTexture *t) {
	if (t) {
		E3D_LogDebug2((&g_FileLog), "  Texture deleted: %s", t->Name);
		Texture_Free(t);
	}
}

static int g_TexPurgeFrame;

static int CTexture_CreateD3D(CTexture *self, void *dev);
void *Texture_Load(CTexture *t, void *dev) {
	if (t->PngData) {
		if (g_TexPurgeFrame != g_FrameCount) {
			CTexture *old;
			g_TexPurgeFrame = g_FrameCount;
			old = Texture_FindPurge();
			if (old) {
				Tex_ReleaseD3D(old);
			}
		}
		CHECK(t->PngData);
		if (t->frame != g_FrameCount) {
			g_TexUsed++;
			g_TexUsedPixels += t->height * t->width;
		}
		if (!t->tex) {
			CHECK(t->PngData);
			CTexture_CreateD3D(t, dev);
			g_TexCreated++;
		}
		t->frame = g_FrameCount;
	}
	return t->tex;
}

CMultiSprite *Texture_GetMultiSprite(CTexture *t, const char *name) {
	if (!t) {
		return NULL;
	}
	if (t->multi.n) {
		return &t->multi;
	}
	if (name) {
		byte ok = CMultiSprite_Build(&t->multi, name);
		E3D_LogWhite((&g_FileLog), "CMapaMat: BuildMultiSpriteTable: %s: %s", ok ? "OK" : "ERROR", name);
		if (ok) {
			return &t->multi;
		}
	}
	return NULL;
}

CTexture *Texture_Find(const char *name, const char *opac) {
	CTexture *t = Texture_Lookup(name, opac);

	if (t) {
		t->Refs++;
		CHECK(t->Refs > 0);
	}
	return t;
}

int Texture_GetInfo(CTexture *t, int *info) {
	if (!t) {
		return TEX_E_NOTFOUND;
	}
	if (info) {
		info[0] = t->width;
		info[1] = t->height;
		info[2] = t->flags;
		info[3] = t->alpha;
		info[4] = t->compressed;
	}
	return 0;
}

int Texture_ReleaseAllD3D(void) {
	for (CTexture *t = g_TexList; t; t = t->next) {
		Tex_ReleaseD3D(t);
	}
	return 0;
}

void *Texture_GetD3D(CTexture *t, void *dev) {
	if (!t) {
		return NULL;
	}
	if (!t->tex) {
		if (t->PngData) {
			return NULL;
		}
		CTexture_CreateD3D(t, dev);
	}
	return t->tex;
}

/* the entry of name with opacity map opac (NULL or "" for none) */
static CTexture *Texture_Lookup(const char *name, const char *opac) {
	for (CTexture *t = g_TexList; t; t = t->next) {
		if (crt_stricmp(name, t->Name)) {
			continue;
		}
		if ((!opac || !opac[0]) && !t->Opac[0]) {
			return t;
		}
		if (opac && t->Opac[0]) {
			if (!crt_stricmp(opac, t->Opac)) {
				return t;
			}
			E3D_LogVerbose((&g_FileLog), "WARN: Same texture with different alpha: %s, %s, %s", name, opac, t->Opac);
		}
	}
	return NULL;
}

static byte Tex_IsPower2(int n) {
	if (n < 1) {
		return 0;
	}
	return !(n & (n - 1));
}

int g_TextOpBPP = 32;

/* the file: its kind from the name, a CBmp (pixels only with load) or the raw file */
static void Bmp_ToDudv(CBmp *bmp);
static byte Path_HasSubExt(const char *name, const char *ext);
static int CTexture_LoadFile(CTexture *self, byte load) {
	char *name = self->Name;
	char *ext;
	byte isBmp, isTga, isTex, isPng, isDdc, isDds;
	byte opac;

	if (!File_Exists(name)) {
		return TEX_E_NOTFOUND;
	}
	ext = crt_mbsrchr(name, '.');
	if (!ext) {
		return TEX_E_INVALIDARG;
	}
	isBmp = !crt_stricmp(ext, ".bmp");
	isTga = !crt_stricmp(ext, ".tga");
	isTex = !crt_stricmp(ext, ".tex");
	isPng = !crt_stricmp(ext, ".png");
	isDdc = !crt_stricmp(ext, ".ddc");
	isDds = !crt_stricmp(ext, ".dds");
	self->ddx = !crt_stricmp(ext, ".ddx");
	self->ddy = !crt_stricmp(ext, ".ddy");
	self->ddv = !crt_stricmp(ext, ".ddv");
	self->compressed = isDds || isDdc || self->ddx || self->ddy || self->ddv;
	self->width = 0;
	self->height = 0;
	self->bpp = g_TextBPP;
	self->alpha = 0;
	if (Path_HasSubExt(name, ".alpha")) {
		self->bpp = g_TextOpBPP;
		self->alpha = 1;
	}
	if (Path_HasSubExt(name, ".bump")) {
		self->bpp = 16;
		self->bump = 1;
		self->mipmap = g_BumpMipmap ? self->mipmap : 0;
		if (!g_PShaders) {
			return TEX_E_NOTFOUND;
		}
	}
	if (Path_HasSubExt(name, ".cube")) {
		self->cube = 1;
	}
	if (Path_HasSubExt(name, ".dudv")) {
		self->bpp = 32;
		self->alpha = 1;
		self->dudv = 1;
		self->mipmap = g_DUDVMipmap ? self->mipmap : 0;
	}
	if (!isBmp && !isTga && !isTex) {
		/* D3DX makes the texture from the file */
		if (isPng || isDdc) {
			CHECK(!self->PngData);
			FileIO_ReadAll(name, &self->PngData, &self->PngSize);
		}
		if (self->Opac[0] && !self->alpha) {
			self->bpp = g_TextOpBPP;
			self->alpha = 1;
			E3D_LogBlue((&g_FileLog), "ERROR: %s with opacity map:", ext);
			E3D_LogVerbose((&g_FileLog), "  text: '%s'", name);
			E3D_LogVerbose((&g_FileLog), "  opac: '%s'", self->Opac);
		}
		return 0;
	}
	CHECK(!self->bitmap);
	CBmp *b = operator_new(sizeof(CBmp));
	if (b) {
		b->img = NULL;
	}
	self->bitmap = b;
	opac = self->Opac[0] && !self->alpha;
	if (opac) {
		self->bpp = g_TextOpBPP;
		self->alpha = 1;
	}
	if (!load) {
		/* an empty CBmp: CTexture_CreateD3D loads it */
		Img_Free(b, BMP_PIXEL);
		return 0;
	}
	if (CBmp_Load(b, name, 0)) {
		if (self->bitmap) {
			CBmp_Delete(self->bitmap, 1);
			self->bitmap = NULL;
		}
		return TEX_E_NOTFOUND;
	}
	self->width = self->bitmap->width;
	self->height = self->bitmap->height;
	if (opac) {
		CBmp o = { 0, 0, NULL };
		if (!CBmp_Load(&o, self->Opac, 0)) {
			/* the average of R G B as the alpha */
			CBmp_MixChannels(&o, 7, 8);
			CBmp_Resample(self->bitmap, &o, 8);
		} else {
			E3D_LogBlue((&g_FileLog), "WARN: Opacity map not found: '%s'", self->Opac);
		}
		Img_Free(&o, BMP_PIXEL);
	}
	if (self->dudv) {
		Bmp_ToDudv(self->bitmap);
	}
	return 0;
}

/* the log lines of a new device texture: the format asked for and the one the device made */
static void Tex_LogCreated(CTexture *self, const char *line, const char *formatLine, int fmt, int made) {
	E3D_LogDebug((&g_FileLog), line, D3DFmt_Name(fmt), self->width, self->height, Path_FileName(self->Name));
	if (made != fmt) {
		E3D_LogDebug((&g_FileLog), formatLine, D3DFmt_Name(made));
	}
}

static int g_TexKeepBitmaps;

/* the device texture, its old one released */
static int CTexture_SaveDDS(CTexture *self, CBmp *orig, int level, byte keepExt);
static int Tex_Format(int bpp, byte alpha, byte palette, byte bump, byte compressed, byte unused);
static int CTexture_CreateD3D(CTexture *self, void *dev) {
	d3dSurfaceDesc_t desc = { 0 };
	int fmt, bpp, hr;

	Tex_ReleaseD3D(self);
	if (!dev || (uint32_t)(g_LoadTextures - 1) > 1) {
		return TEX_E_INVALIDCALL;
	}
	if (self->failed) {
		return TEX_E_FAIL;
	}
	if (self->bitmap) {
		uint32_t w, h;
		if (!self->bitmap->width && !self->bitmap->height) {
			/* the pixels were not loaded or were freed */
			CBmp_Delete(self->bitmap, 1);
			self->bitmap = NULL;
			if (CTexture_LoadFile(self, 1) < 0) {
				return TEX_E_FAIL;
			}
			CHECK(self->bitmap);
		}
		fmt = Tex_FormatRGB(Tex_BPP(self), self->alpha != 0, self->bump);
		w = self->width;
		h = self->height;
		D3DX_CheckTextureRequirements(dev, &w, &h, NULL, 0, &fmt, D3DPOOL_MANAGED);
		if (fmt == FMT_A1R5G5B5) {
			fmt = FMT_A4R4G4B4;
			D3DX_CheckTextureRequirements(dev, &w, &h, NULL, 0, &fmt, D3DPOOL_MANAGED);
		}
		if (w != (uint32_t)self->width || h != (uint32_t)self->height) {
			E3D_LogVerbose((&g_FileLog), "Invalid texture size: %d x %d", self->width, self->height);
		}
		hr = Rd_DevCreateTexture(dev, w, h, !self->mipmap, 0, fmt, D3DPOOL_MANAGED, &self->tex);
		if (!hr) {
			hr = CTexture_UploadLevel(self, self->bitmap, 0);
		}
		if (g_BuildDDS) {
			CTexture_SaveDDS(self, self->bitmap, 0, 0);
		}
		if (!g_TexKeepBitmaps) {
			Img_Free(self->bitmap, BMP_PIXEL);
		}
		if (hr < 0) {
			E3D_LogBlue((&g_FileLog), "ERROR: creando la textura (CBMP): '%s'", Path_FileName(self->Name));
			return hr;
		}
		Rd_TexGetLevelDesc(self->tex, 0, &desc);
		Tex_LogCreated(self, "Texture (CBMP):  %-8s  %4d %4d  '%s'", "Format  (CBMP):  %-8s", fmt, desc.Format);
	} else if (self->PngData) {
		byte comp = self->compressed;
		bpp = Tex_BPP(self);
		fmt = Tex_Format(bpp, self->alpha != 0, bpp <= 8, self->bump, comp, 1);
		hr = D3DX_CreateTextureFromFileInMemoryEx(dev, self->PngData, self->PngSize, 0, 0, !self->mipmap, 0,
			comp ? 0 : fmt, D3DPOOL_MANAGED, Tex_Dither() | D3DX_FILTER_TRIANGLE, Tex_Dither() | D3DX_FILTER_BOX, 0,
			NULL, NULL, &self->tex);
		if (hr < 0) {
			E3D_LogBlue((&g_FileLog), "ERROR: creando la textura (D3DX): '%s'", Path_FileName(self->Name));
			return hr;
		}
		Rd_TexGetLevelDesc(self->tex, 0, &desc);
		self->width = desc.Width;
		self->height = desc.Height;
		if (!self->frame) {
			/* the first time only, the texture is made again after each purge */
			Tex_LogCreated(self, "Texture (D3DX):  %-8s  %4d %4d  [cached] '%s'", "Format  (D3DX):  %-8s", fmt,
				desc.Format);
		}
		self->frame = g_FrameCount;
	} else {
		bpp = Tex_BPP(self);
		fmt = Tex_Format(bpp, self->alpha != 0, bpp <= 8, self->bump, self->compressed, 1);
		if (!self->cube) {
			void *buf;
			uint32_t size;
			FileIO_ReadAll(self->Name, &buf, &size);
			hr = D3DX_CreateTextureFromFileInMemoryEx(dev, buf, size, 0, 0, !self->mipmap, 0,
				self->compressed ? 0 : fmt, g_TexturePool == 1, Tex_Dither() | D3DX_FILTER_TRIANGLE,
				Tex_Dither() | D3DX_FILTER_BOX, 0, NULL, NULL, &self->tex);
			Ptr_Delete(&buf);
			if (hr < 0) {
				E3D_LogBlue((&g_FileLog), "ERROR: creando la textura (D3DX): '%s'", Path_FileName(self->Name));
				return hr;
			}
			Rd_TexGetLevelDesc(self->tex, 0, &desc);
			self->width = desc.Width;
			self->height = desc.Height;
		} else {
			CHECK(0);
		}
		Tex_LogCreated(self, "Texture (D3DX):  %-8s  %4d %4d  '%s'", "Format  (D3DX):  %-8s", fmt, desc.Format);
	}
	if (self->tex && g_TextureLOD > 0) {
		Rd_TexSetLOD(self->tex, g_TextureLOD);
	}
	return 0;
}

/* the developer tool: orig as a DDS file next to the texture, through bmp/nvdxt.exe and bmp/stitch.exe */
static void Bmp_ToBump(CBmp *bmp);
static void Path_ToBackslash(char *path);
static int CTexture_SaveDDS(CTexture *self, CBmp *orig, int level, byte keepExt) {
	char path[MAX_FILEPATH];
	char tmp[MAX_FILEPATH];
	char dir[MAX_FILEPATH];
	char cmd[0x400];
	const char *name = self->Name;
	const char *mode;
	CBmp b;
	int lx, ly, dx, ok;

	if (self->bump && !g_BumpDDS) {
		return 0;
	}
	if (!(self->flags & TEXF_DDS) || (self->flags & TEXF_FIXEDBPP) || (self->flags & TEXF_NODDS)) {
		return 0;
	}
	Path_Format(path, name, "&U&D_dds");
	if (!Path_IsDir(path)) {
		return 0;
	}
	E3D_DebugPrintf("\nSaveDDS: %s [opac:%s]\n", Path_FileName(name), Tex_OpacName(self));
	if (self->alpha) {
		mode = g_SaveDXT5 && !keepExt ? "dxt5" : "dxt3";
	} else {
		mode = "dxt1c";
	}
	lx = orig->width;
	dx = 0;
	ly = orig->height;
	CHECK(Tex_IsPower2(lx) && Tex_IsPower2(ly));
	for (;;) {
		b.width = lx;
		b.height = ly;
		b.img = New_Array((uint32_t)ly * lx, BMP_PIXEL);
		CBmp_Resample(&b, orig, 0xf);
		if (g_TexSharpen && level > 0) {
			CBmp_Sharpen(&b);
		}
		if (self->bump) {
			Bmp_ToBump(&b);
		}
		Bprintf(tmp, sizeof(tmp), "&U&D_dds/_dds_tmp_%02d.tga", level);
		Path_Format(path, name, tmp);
		Path_ToBackslash(path);
		if (CBmp_Save(&b, path, self->alpha ? IMG_ALPHA : 0)) {
			Con_FatalErrorf("SaveCBitmapDDS: Could not write to file: '%s'", path);
		}
		Path_Format(dir, path, "&U&D.");
		Bprintf(cmd, sizeof(cmd), "-outdir \"%s\" -nomipmap -%s -file \"%s\"", dir, mode, path);
		CHECK(!crt_spawnlp(P_WAIT, "bmp/nvdxt.exe", "nvdxt.exe", cmd, NULL));
		dx += lx;
		if (!self->mipmap || lx == 1) {
			break;
		}
		lx = lx > 1 ? lx >> 1 : 1;
		ly = ly > 1 ? ly >> 1 : 1;
		level++;
		Img_Free(&b, BMP_PIXEL);
	}
	Img_Free(&b, BMP_PIXEL);
	if (self->mipmap) {
		ok = dx + 1 == orig->width * 2;
	} else {
		ok = dx == orig->width;
	}
	CHECK(ok);
	Bprintf(tmp, sizeof(tmp), "&U&D_dds/_dds_tmp_%02d.dds", level + 1);
	Path_Format(path, name, tmp);
	CHECK(crt_access(path, 0));
	Path_Format(path, name, "\"&U&D_dds/_dds_tmp\"");
	Path_ToBackslash(path);
	CHECK(!crt_spawnlp(P_WAIT, "bmp/stitch.exe", "stitch.exe", path, NULL));
	for (int i = 0; i <= level; i++) {
		Bprintf(tmp, sizeof(tmp), "&U&D_dds/_dds_tmp_%02d.tga", i);
		Path_Format(path, name, tmp);
		CHECK(!crt_remove(path));
		Bprintf(tmp, sizeof(tmp), "&U&D_dds/_dds_tmp_%02d.dds", i);
		Path_Format(path, name, tmp);
		CHECK(!crt_remove(path));
	}
	Bprintf(tmp, sizeof(tmp), "&U&D_dds/&N%s.dds", self->Opac[0] ? ".alpha" : "");
	Path_Format(path, name, "&U&D_dds/_dds_tmp.dds");
	Path_Format(tmp, name, tmp);
	if (keepExt) {
		Bprintf(tmp + strlen(tmp), 8, ".%s", mode);
	}
	if (crt_rename(path, tmp)) {
		Con_FatalErrorf("SaveCBitmapDDS: Could not rename: '%s' -> '%s'", Path_FileName(path), Path_FileName(tmp));
	}
	E3D_LogDebug((&g_FileLog), "SaveCBitmapDDS: File saved: '%s' [mip:%d] [opac:%s]", Path_FileName(tmp), level,
		Tex_OpacName(self));
	return 0;
}

/* the D3DFORMAT for a texture file */
static int Tex_Format(int bpp, byte alpha, byte palette, byte bump, byte compressed, byte unused) {
	(void)unused;
	if (bump) {
		if (compressed && bpp == 16) {
			return FMT_DXT1;
		}
		return Tex_FormatRGB(bpp, alpha, 1);
	}
	if (palette && !alpha && bpp <= 8) {
		return FMT_P8;
	}
	if (compressed) {
		return alpha ? FMT_DXT5 : FMT_DXT1;
	}
	return Tex_FormatRGB(bpp, alpha, 0);
}

static void Path_ToBackslash(char *path) {
	for (; *path; path++) {
		if (*path == '/') {
			*path = '\\';
		}
	}
}

/* name ends in ext before its last extension ("a.alpha.tga") */
static byte Path_HasSubExt(const char *name, const char *ext) {
	size_t len = strlen(ext);
	const char *dot = crt_mbsrchr(name, '.');

	CHECK(dot);
	if ((uintptr_t)name <= (uintptr_t)dot - len && !crt_strnicmp(dot - len, ext, len)) {
		return 1;
	}
	return 0;
}

static byte Bump_Channel(uint32_t c) {
	if (c <= 0x40) {
		return 0;
	}
	if (c >= 0xb8) {
		return 0x78;
	}
	return c - 0x40;
}

/* R and G to signed offsets around 0 (V8U8 style) */
static void Bmp_ToBump(CBmp *bmp) {
	CHECK(bmp);
	for (int y = 0; y < bmp->height; y++) {
		for (int x = 0; x < bmp->width; x++) {
			uint32_t p = *CBmp_Pixel(bmp, x, y);
			uint32_t u = Bump_Channel(p & 0xff);
			uint32_t v = Bump_Channel(p >> 8 & 0xff);
			*CBmp_Pixel(bmp, x, y) = u | v << 8 | u << 16;
		}
	}
}

/* R and G to the DUDV pair, the smaller part in A */
static void Bmp_ToDudv(CBmp *bmp) {
	CBmp tmp;

	CHECK(bmp);
	tmp.width = bmp->width;
	tmp.height = bmp->height;
	tmp.img = New_Array((uint32_t)tmp.width * tmp.height, BMP_PIXEL);
	for (int y = 0; y < bmp->height; y++) {
		for (int x = 0; x < bmp->width; x++) {
			uint32_t p = *CBmp_Pixel(bmp, x, y);
			byte u = (byte)p >> 2;
			byte v = (byte)(p >> 8) >> 2;
			byte m = u < v ? u : v;
			byte a;
			if (m >= 0x20) {
				u -= 0x20;
				v -= 0x20;
				a = 0;
			} else {
				v -= m;
				u -= m;
				a = 0x20 - m;
			}
			*CBmp_Pixel(&tmp, x, y) = u | v << 8 | u << 16 | (uint32_t)a << 24;
		}
	}
	CBmp_Resample(bmp, &tmp, 0xf);
	Img_Free(&tmp, BMP_PIXEL);
}

/* bmp into a locked A8R8G8B8 IDirect3DSurface8 of the same size */
static int Surface_FromBmp(void *surf, const CBmp *bmp) {
	d3dSurfaceDesc_t desc;
	d3dLockedRect_t lr;
	byte *row;
	int hr;

	hr = Rd_SurfGetDesc(surf, &desc);
	if (hr < 0) {
		return hr;
	}
	CHECK(desc.Format == FMT_A8R8G8B8);
	CHECK(desc.Width == (uint32_t)bmp->width);
	CHECK(desc.Height == (uint32_t)bmp->height);
	hr = Rd_SurfLockRect(surf, &lr, NULL, 0);
	if (hr < 0) {
		return hr;
	}
	row = lr.pBits;
	for (uint32_t y = 0; y < desc.Height; y++) {
		const uint32_t *s = CBmp_Pixel(bmp, 0, y);
		uint32_t *d = (uint32_t *)row;
		/* R G B A bytes to A8R8G8B8 */
		for (uint32_t x = 0; x < desc.Width; x++) {
			uint32_t p = s[x];
			d[x] = (p & 0xff00ff00) | (p & 0xff) << 16 | (p >> 16 & 0xff);
		}
		row += lr.Pitch;
	}
	Rd_SurfUnlockRect(surf);
	return 0;
}

const char *D3DFmt_Name(int fmt) {
	switch (fmt) {
	case 0: return "UNKNOWN";
	case 0x14: return "R8G8B8";
	case 0x15: return "A8R8G8B8";
	case 0x16: return "X8R8G8B8";
	case 0x17: return "R5G6B5";
	case 0x18: return "X1R5G5B5";
	case 0x19: return "A1R5G5B5";
	case 0x1a: return "A4R4G4B4";
	case 0x1b: return "R3G3B2";
	case 0x1c: return "A8";
	case 0x1d: return "A8R3G3B2";
	case 0x1e: return "X4R4G4B4";
	case 0x1f: return "A2B10G10R10";
	case 0x22: return "G16R16";
	case 0x28: return "A8P8";
	case 0x29: return "P8";
	case 0x32: return "L8";
	case 0x33: return "A8L8";
	case 0x34: return "A4L4";
	case 0x3c: return "V8U8";
	case 0x3d: return "L6V5U5";
	case 0x3e: return "X8L8V8U8";
	case 0x3f: return "Q8W8V8U8";
	case 0x40: return "V16U16";
	case 0x41: return "W11V11U10";
	case 0x43: return "A2W10V10U10";
	case 0x46: return "D16_LOCKABLE";
	case 0x47: return "D32";
	case 0x49: return "D15S1";
	case 0x4b: return "D24S8";
	case 0x4d: return "D24X8";
	case 0x4f: return "D24X4S4";
	case 0x50: return "D16";
	case 100: return "VERTEXDATA";
	case 0x65: return "INDEX16";
	case 0x66: return "INDEX32";
	case 0x31545844: return "DXT1";
	case 0x32545844: return "DXT2";
	case 0x33545844: return "DXT3";
	case 0x34545844: return "DXT4";
	case 0x35545844: return "DXT5";
	case 0x32595559: return "YUY2";
	case 0x59565955: return "UYVY";
	}
	return "(invalid D3DFMT)";
}

/* the atexit destructor of g_TexList: the first texture of the list deleted */
static void Texture_FiniTexList(void) {
	CTexture *t = g_TexList;

	if (t) {
		CTexture_dtor(t);
		operator_delete_sized(t, sizeof(CTexture));
	}
}

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_R_PShaders, &GVar_R_PShaders, "R_PShaders", 0, "Use pixel shaders (0/1)", 1)
INIT_GVAR_INT(Init_GVar_BuildDDS, &GVar_BuildDDS, "BuildDDS", 0, "Build DDS Textures (0/1)\n", 0)
INIT_GVAR_INT(Init_GVar_BumpMipmap, &GVar_BumpMipmap, "BumpMipmap", 0, "Allow mipmapping for bump textures (0/1)", 0)
INIT_GVAR_INT(Init_GVar_DUDVMipmap, &GVar_DUDVMipmap, "DUDVMipmap", 0, "Allow mipmapping for DUDV textures (0/1)", 1)
INIT_GVAR_INT(Init_GVar_TextureLOD, &GVar_TextureLOD, "TextureLOD", 2, "Most detailed mipmap level used (0=disable)", 0)
INIT_GVAR_INT(Init_GVar_SharpenMore, &GVar_SharpenMore, "SharpenMore", 0, "Extra sharpen for each mipmap level (0/1)", 0)
INIT_GVAR_INT(Init_GVar_MipmapFade, &GVar_MipmapFade, "MipmapFade", 0, "Fadeout each mipmap level (0/1)", 0)
INIT_GVAR_INT(Init_GVar_MipmapColor, &GVar_MipmapColor, "MipmapColor", 0, "Colorize each mipmap level (0/1)", 0)
INIT_GVAR_INT(Init_GVar_CachedTex, &GVar_CachedTex, "CachedTex", 0, "Max. cached textures", 20)
INIT_GVAR_INT(Init_GVar_BumpDDS, &GVar_BumpDDS, "BumpDDS", 0, "Allow compression of bump textures (0/1)", 0)
INIT_GVAR_INT(Init_GVar_SaveDXT5, &GVar_SaveDXT5, "SaveDXT5", 0, "Save DXT5 textures (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_Tex_Count, &GVar_R_Tex_Count, "R_Tex_Count", 0, "Texture counter", 0)
INIT_ATEXIT(Init_TexList, Texture_FiniTexList)
