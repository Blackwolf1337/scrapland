/*
 * RGBA byte images of the engine3d, original source Engine/bmp.cpp (include/scrap/bmp.h), and the helpers all image
 * classes share.
 */
#include <string.h>

#include "scrap/archio.h"
#include "scrap/bmp.h"
#include "scrap/e3dutil.h"
#include "scrap/filesys.h"

#define BMP_PIXEL 4
#define CRT_EEXIST 17
const int g_ErrPcxFormat = 50;

/* errno for a bad file (also TXF) */
#define g_ImgBadFormat g_ErrPcxFormat

#define TEX_MAGIC 0x1a584554
#define BMP_TYPE 0x4d42
#define TGA_TYPE_RGB 2
#define TGA_TYPE_RLE 10
#define TGA_TOP_DOWN 0x20
#define TGA_RUN 0x80		/* packet header: a run, else raw pixels */
#define TGA_PACKET_MAX 128

void *Img_Pixel(const CImage *b, uint32_t x, uint32_t y, int bytes) {
	CHECK(b->img);
	CHECK(x < (uint32_t)b->width);
	CHECK(y < (uint32_t)b->height);
	return (byte *)b->img + (b->width * y + x) * bytes;
}

void Img_Free(CImage *b, int bytes) {
	b->width = 0;
	b->height = 0;
	if (b->img) {
		operator_delete_sized(b->img, bytes);
		b->img = NULL;
	}
}

void Img_Create(CImage *b, int width, int height, int bytes) {
	Img_Free(b, bytes);
	b->width = width;
	b->height = height;
	b->img = New_Array(width * height, bytes);
}

int Img_IsExt(const char *name, const char *ext) {
	int len = strlen(name);

	return !crt_stricmp(name + (len - 4 > 0 ? len - 4 : 0), ext);
}

int Img_Clip(const CImage *self, int x, int y, const CImage *orig, int ox, int oy, int *w, int *h) {
	if (orig->width - ox <= *w) {
		*w = orig->width - ox;
	}
	if (orig->height - oy <= *h) {
		*h = orig->height - oy;
	}
	if (self->width - x <= *w) {
		*w = self->width - x;
	}
	if (self->height - y <= *h) {
		*h = self->height - y;
	}
	return *w > 0 && *h > 0;
}

int Img_BoxStart(int i, float scale) {
	return (int)((double)((float)i * scale) + 1e-4);
}

int Img_ResampleCopy(CImage *self, const CImage *orig, uint32_t mask, int bytes) {
	CHECK(orig);
	CHECK(orig->img);
	CHECK(mask);
	if (!self->img) {
		Img_Create(self, orig->width, orig->height, bytes);
		CHECK(self->img);
	}
	if (self->width == orig->width && self->height == orig->height && mask == 0xf) {
		memcpy(self->img, orig->img, self->height * self->width * bytes);
		return 1;
	}
	return 0;
}

static byte Bmp_Clamp(int v) {
	return v <= 0 ? 0 : v >= 0xff ? 0xff : v;
}

/* memory order R G B A to 0xAARRGGBB, and back */
static uint32_t Bmp_SwapRB(uint32_t p) {
	return (p & 0xff00ff00) | (p & 0xff) << 16 | (p >> 16 & 0xff);
}

/* 0xAARRGGBB colors: a + (b - a) * t in steps of 1/256 */
static uint32_t Color_Lerp(uint32_t a, uint32_t b, float t) {
	int k = (int)(t * 256.0f);
	uint32_t r = 0;

	for (int s = 24; s >= 0; s -= 8) {
		int c = ((int)(a >> s & 0xff) * (0x100 - k) + (int)(b >> s & 0xff) * k) >> 8;
		r = r << 8 | Bmp_Clamp(c);
	}
	return r;
}

uint32_t *CBmp_Pixel(const CBmp *self, uint32_t x, uint32_t y) {
	return Img_Pixel(self, x, y, BMP_PIXEL);
}

static void CBmp_Create(CBmp *self, int width, int height) {
	Img_Create(self, width, height, BMP_PIXEL);
}

void CBmp_Fill(CBmp *self, uint32_t color) {
	CHECK(self->img);
	uint32_t *p = self->img;
	for (int n = self->height * self->width; n > 0; n--) {
		*p++ = color;
	}
}

void CBmp_Copy(CBmp *self, int x, int y, const CBmp *orig, int ox, int oy, int w, int h, uint32_t mask) {
	CHECK(orig);
	CHECK(orig->img);
	CHECK(mask);
	CHECK(self->img);
	if (!Img_Clip(self, x, y, orig, ox, oy, &w, &h)) {
		return;
	}
	for (int row = 0; row < h; row++) {
		const byte *s = (const byte *)CBmp_Pixel(orig, ox, oy + row);
		byte *d = (byte *)CBmp_Pixel(self, x, y + row);
		if (mask == 0xf) {
			memcpy(d, s, w * BMP_PIXEL);
			continue;
		}
		for (int i = 0; i < w * BMP_PIXEL; i++) {
			if (mask & 1u << (i & 3)) {
				d[i] = s[i];
			}
		}
	}
}

void CBmp_MixChannels(CBmp *self, uint32_t in, uint32_t out) {
	CHECK(in && out);
	CHECK(self->img);
	for (int y = 0; y < self->height; y++) {
		for (int x = 0; x < self->width; x++) {
			byte c[4];
			memcpy(c, CBmp_Pixel(self, x, y), 4);
			uint32_t sum = 0, n = 0;
			for (int i = 0; i < 4; i++) {
				if (in & 1u << i) {
					sum += c[i];
					n++;
				}
			}
			if (!n) {
				CHECK(0);
				continue;
			}
			for (int i = 0; i < 4; i++) {
				if (out & 1u << i) {
					c[i] = (byte)(sum / n);
				}
			}
			memcpy(CBmp_Pixel(self, x, y), c, 4);
		}
	}
}

static int Bmp_Weight(int a) {
	return a < -0xfffff ? -0x100000 : a > 0xfffff ? 0x100000 : a;
}

void CBmp_Blend(CBmp *self, int x, int y, const CBmp *orig, int ox, int oy, int w, int h, int a, int b) {
	CHECK(orig);
	CHECK(orig->img);
	CHECK(self->img);
	if (!Img_Clip(self, x, y, orig, ox, oy, &w, &h)) {
		return;
	}
	a = Bmp_Weight(a);
	b = Bmp_Weight(b);
	for (int row = 0; row < h; row++) {
		const byte *s = (const byte *)CBmp_Pixel(orig, ox, oy + row);
		byte *d = (byte *)CBmp_Pixel(self, x, y + row);
		for (int i = 0; i < w * BMP_PIXEL; i++) {
			d[i] = Bmp_Clamp((s[i] * a + d[i] * b) >> 8);
		}
	}
}

/* t cut to 0..1, NaN stays */
static float Bmp_Frac(float t) {
	if (0.0f >= t) {
		return 0.0f;
	}
	return 1.0f < t ? 1.0f : t;
}

uint32_t *CBmp_Sample(const CBmp *self, uint32_t *out, float u, float v) {
	CHECK(u >= 0.0f && u <= 1.0f);
	CHECK(v >= 0.0f && v <= 1.0f);
	CHECK(self->img);
	const uint32_t *img = self->img;
	float fx = (float)self->width * u - 0.5f;
	float fy = (float)self->height * v - 0.5f;
	int ix = (int)fx, iy = (int)fy;
	int x0 = ix < self->width - 1 ? ix : self->width - 1;
	int x1 = ix + 1 < self->width - 1 ? ix + 1 : self->width - 1;
	int y0 = iy < self->height - 1 ? iy : self->height - 1;
	int y1 = iy + 1 < self->height - 1 ? iy + 1 : self->height - 1;
	float tx = Bmp_Frac(fx - (float)ix);
	float ty = Bmp_Frac(fy - (float)iy);
	uint32_t c1 = Color_Lerp(Bmp_SwapRB(img[x0 + y1 * self->width]), Bmp_SwapRB(img[x1 + y1 * self->width]), tx);
	uint32_t c0 = Color_Lerp(Bmp_SwapRB(img[x0 + y0 * self->width]), Bmp_SwapRB(img[x1 + y0 * self->width]), tx);
	*out = Bmp_SwapRB(Color_Lerp(c0, c1, ty));
	return out;
}

/* a 3x3 kernel on the inner pixels, read from a copy */
static void Bmp_Filter3x3(CBmp *self, const int k[9], int round, int shift) {
	CImage tmp;

	CHECK(self->img);
	tmp.img = NULL;
	CBmp_Resample(&tmp, self, 0xf);
	for (int y = 0; y < self->height; y++) {
		const byte *s = (const byte *)CBmp_Pixel(&tmp, 0, y);
		byte *d = (byte *)CBmp_Pixel(self, 0, y);
		for (int x = 0; x < self->width; x++) {
			if (x <= 0 || y <= 0 || x >= self->width - 1 || y >= self->height - 1) {
				continue;
			}
			int stride = self->width * 4;
			for (int i = 0; i < 4; i++) {
				const byte *c = s + x * 4 + i;
				int sum = round;
				for (int r = 0; r < 3; r++) {
					const byte *line = c + (r - 1) * stride;
					sum += line[-4] * k[r * 3] + line[0] * k[r * 3 + 1] + line[4] * k[r * 3 + 2];
				}
				d[x * 4 + i] = Bmp_Clamp(sum >> shift);
			}
		}
	}
	if (tmp.img) {
		operator_delete_sized(tmp.img, BMP_PIXEL);
	}
}

void CBmp_Sharpen(CBmp *self) {
	static const int k[9] = { -1, -3, -1, -3, 48, -3, -1, -3, -1 };

	Bmp_Filter3x3(self, k, 0x10, 5);
}

void CBmp_Resample(CBmp *self, const CBmp *orig, uint32_t mask) {
	if (Img_ResampleCopy(self, orig, mask, BMP_PIXEL)) {
		return;
	}
	if (orig->width == self->width * 2 && orig->height == self->height * 2 && mask == 0xf) {
		for (int y = 0; y < self->height; y++) {
			const byte *s = (const byte *)CBmp_Pixel(orig, 0, y * 2);
			const byte *s2 = s + orig->width * 4;
			byte *d = (byte *)CBmp_Pixel(self, 0, y);
			for (int x = 0; x < self->width; x++) {
				for (int i = 0; i < 4; i++) {
					d[x * 4 + i] = (byte)((unsigned)(s2[x * 8 + 4 + i] + s[x * 8 + i] + s[x * 8 + 4 + i] + s2[x * 8 + i]) >> 2);
				}
			}
		}
		return;
	}
	float sx = (float)orig->width / (float)self->width;
	float sy = (float)orig->height / (float)self->height;
	for (int y = 0; y < self->height; y++) {
		int y0 = Img_BoxStart(y, sy);
		int y1 = Img_BoxStart(y + 1, sy);
		if (y1 == y0) {
			y1++;
		}
		for (int x = 0; x < self->width; x++) {
			int x0 = Img_BoxStart(x, sx);
			int x1 = Img_BoxStart(x + 1, sx);
			if (x1 == x0) {
				x1++;
			}
			int sum[4] = { 0, 0, 0, 0 };
			int n = 0;
			for (int ix = x0; ix < x1; ix++) {
				if (y0 < y1) {
					n += y1 - y0;
				}
				for (int iy = y0; iy < y1; iy++) {
					const byte *p = (const byte *)CBmp_Pixel(orig, ix, iy);
					for (int i = 0; i < 4; i++) {
						sum[i] += p[i];
					}
				}
			}
			byte c[4] = { 0, 0, 0, 0 };
			if (!n) {
				CHECK(0);
			} else {
				if (mask != 0xf) {
					memcpy(c, CBmp_Pixel(self, x, y), 4);
				}
				for (int i = 0; i < 4; i++) {
					if (mask & 1u << i) {
						c[i] = (byte)(sum[i] / n);
					}
				}
			}
			memcpy(CBmp_Pixel(self, x, y), c, 4);
		}
	}
}

/* without IMG_OVERWRITE an existing file is EEXIST */
static int Img_SaveRefused(const char *name, uint32_t flags) {
	if (flags & IMG_OVERWRITE) {
		return 0;
	}
	void *f = File_OpenRel(name, "rb");
	if (!f) {
		return 0;
	}
	crt_fclose(f);
	*crt_errno() = CRT_EEXIST;
	return 1;
}

static bmpFileCab_t g_BmpFileHeader;
static bmpInfoCab_t g_BmpInfoCab;

static int BMP_Save(const char *name, const CBmp *orig, uint32_t flags) {
	bmpFileCab_t *fc = (&g_BmpFileHeader);
	bmpInfoCab_t *ic = (&g_BmpInfoCab);

	CHECK(name && orig && orig->img);
	if (Img_SaveRefused(name, flags)) {
		return -1;
	}
	void *f = File_OpenRel(name, "wb");
	if (!f) {
		return -1;
	}
	fc->type = BMP_TYPE;
	uint32_t size = (orig->width + 1) * 3 & ~3u;
	fc->offBits = sizeof(*fc) + sizeof(*ic);
	fc->size = orig->height * size + sizeof(*fc) + sizeof(*ic);
	fc->reserved = 0;
	if (crt_fwrite(fc, sizeof(*fc), 1, f) != 1) {
		goto fail;
	}
	memset(ic, 0, sizeof(*ic));
	ic->size = sizeof(*ic);
	ic->width = orig->width;
	ic->height = orig->height;
	ic->planes = 1;
	ic->bits = 24;
	if (crt_fwrite(ic, sizeof(*ic), 1, f) != 1) {
		goto fail;
	}
	/* the padding of the rows is what new[] left there */
	byte *buf = operator_new_array(size);
	for (int y = 0; y < ic->height; y++) {
		const byte *s = (const byte *)CBmp_Pixel(orig, 0, ic->height - y - 1);
		for (int x = 0; x < ic->width; x++) {
			buf[x * 3 + 2] = s[x * 4];
			buf[x * 3 + 1] = s[x * 4 + 1];
			buf[x * 3] = s[x * 4 + 2];
		}
		if (crt_fwrite(buf, size, 1, f) != 1) {
			goto fail;
		}
	}
	operator_delete_sized(buf, 1);
	crt_fclose(f);
	return 0;
fail:
	crt_fclose(f);
	E3D_LogBlue((&g_FileLog), "escr_BMP_24(): Error de escritura: '%s'", name);
	return -1;
}

static texCab_t g_TexHeader;

static int TEX_Save(const char *name, const CBmp *orig, uint32_t flags) {
	texCab_t *c = (&g_TexHeader);

	CHECK(name && orig && orig->img);
	if (Img_SaveRefused(name, flags)) {
		return -1;
	}
	void *f = File_OpenRel(name, "wb");
	if (!f) {
		return -1;
	}
	memset(c->pad, 0, sizeof(c->pad));
	c->magic = TEX_MAGIC;
	c->version = 1;
	c->width = orig->width;
	c->height = orig->height;
	if (crt_fwrite(c, sizeof(*c), 1, f) == 1 && crt_fwrite(orig->img, c->height * c->width * BMP_PIXEL, 1, f) == 1) {
		crt_fclose(f);
		return 0;
	}
	crt_fclose(f);
	E3D_LogBlue((&g_FileLog), "escr_TEX(): Error de escritura: '%s'", name);
	return -1;
}

static void Tga_PutPixel(const byte *p, int alpha, void *f) {
	crt_fputc(p[2], f);
	crt_fputc(p[1], f);
	crt_fputc(p[0], f);
	if (alpha) {
		crt_fputc(p[3], f);
	}
}

/* packets of up to 128 pixels: a run of equal pixels from 3 on (2 when the next two are equal as well), else raw
 * pixels up to the next 3 equal ones */
static void Tga_PutRow(const uint32_t *row, int w, uint32_t mask, int alpha, void *f) {
	int x = 0;

	while (x < w) {
		int start = x;
		int n = 0;
		do {
			n++;
			x++;
		} while (n != TGA_PACKET_MAX && x < w && (row[x - 1] & mask) == (row[x] & mask));
		if (n > 2 || (n == 2 && x < w - 1 && (row[x] & mask) == (row[x + 1] & mask))) {
			crt_fputc((n - 1) | TGA_RUN, f);
			Tga_PutPixel((const byte *)&row[start], alpha, f);
			continue;
		}
		start = x - n;
		while (x < w) {
			if (x < w - 2 && (row[x] & mask) == (row[x + 1] & mask) && (row[x] & mask) == (row[x + 2] & mask)) {
				break;
			}
			n++;
			x++;
			if (n == TGA_PACKET_MAX) {
				break;
			}
		}
		if (n) {
			crt_fputc(n - 1, f);
			for (int i = 0; i < n; i++) {
				Tga_PutPixel((const byte *)&row[start + i], alpha, f);
			}
		}
	}
}

static tgaCab_t g_TgaCab;

static int TGA_Save(const char *name, const CBmp *orig, uint32_t flags) {
	tgaCab_t *c = (&g_TgaCab);

	CHECK(name && orig && orig->img);
	if (Img_SaveRefused(name, flags)) {
		return -1;
	}
	void *f = File_OpenRel(name, "wb");
	if (!f) {
		return -1;
	}
	int alpha = (flags & IMG_ALPHA) != 0;
	memset(c, 0, sizeof(*c));
	c->type = TGA_TYPE_RLE;
	c->width = orig->width;
	c->height = orig->height;
	c->desc = TGA_TOP_DOWN;
	c->bits = alpha ? 32 : 24;
	if (crt_fwrite(c, sizeof(*c), 1, f) != 1) {
		crt_fclose(f);
		E3D_LogBlue((&g_FileLog), "escr_TGA(): Error de escritura: '%s'", name);
		return -1;
	}
	for (int y = 0; y < orig->height; y++) {
		Tga_PutRow(CBmp_Pixel(orig, 0, y), orig->width, alpha ? 0xffffffff : 0xffffff, alpha, f);
	}
	crt_fclose(f);
	return 0;
}

int CBmp_Save(CBmp *self, const char *name, uint32_t flags) {
	CHECK(name);
	if (Img_IsExt(name, ".tex")) {
		return TEX_Save(name, self, flags);
	}
	if (Img_IsExt(name, ".tga")) {
		return TGA_Save(name, self, flags);
	}
	return BMP_Save(name, self, flags);
}

/* a file of the packs that is not what it should be */
static int Img_LoadFailed(void *f, const char *fmt, const char *name) {
	FileIO_Close(f);
	*crt_errno() = g_ImgBadFormat;
	E3D_LogBlue((&g_FileLog), fmt, name);
	return -1;
}

static int BMP_Load(const char *name, CBmp *dest, uint32_t flags) {
	static const char bad[] = "leer_BMP_24(): Formato no v\xe1lido: '%s'";
	bmpFileCab_t *fc = (&g_BmpFileHeader);
	bmpInfoCab_t *ic = (&g_BmpInfoCab);
	byte pal[256 * 4];

	CHECK(name && dest);
	void *f = FileIO_Open(name, "rb");
	if (!f) {
		return -1;
	}
	if (FileIO_Read(fc, sizeof(*fc), 1, f) != 1 || FileIO_Read(ic, sizeof(*ic), 1, f) != 1 || fc->type != BMP_TYPE ||
		ic->size != sizeof(*ic) || ic->planes != 1 || (ic->bits != 32 && ic->bits != 24 && ic->bits != 8) ||
		ic->compression || ic->width <= 0 || ic->height <= 0) {
		return Img_LoadFailed(f, bad, name);
	}
	if (ic->bits == 8) {
		if (!ic->clrUsed) {
			ic->clrUsed = 256;
		} else if (ic->clrUsed > 256) {
			return Img_LoadFailed(f, bad, name);
		}
		if (FileIO_Read(pal, ic->clrUsed * 4, 1, f) != 1) {
			return Img_LoadFailed(f, bad, name);
		}
	}
	CHECK(fc->offBits == (uint32_t)FileIO_Tell(f));
	CBmp_Create(dest, ic->width, ic->height);
	uint32_t size = (((ic->bits + 7) >> 3) * ic->width + 3) & ~3u;
	byte *buf = operator_new_array(size);
	for (int y = 0; y < ic->height; y++) {
		if (FileIO_Read(buf, size, 1, f) != 1) {
			return Img_LoadFailed(f, bad, name);
		}
		byte *d = (byte *)CBmp_Pixel(dest, 0, ic->height - y - 1);
		for (int x = 0; x < ic->width; x++, d += 4) {
			if (ic->bits == 8) {
				const byte *c = pal + buf[x] * 4;
				d[0] = c[2];
				d[1] = c[1];
				d[2] = c[0];
				d[3] = flags & IMG_INDEX_ALPHA ? buf[x] : c[3];
			} else if (ic->bits == 24) {
				d[0] = buf[x * 3 + 2];
				d[1] = buf[x * 3 + 1];
				d[2] = buf[x * 3];
				d[3] = 0;
			} else {
				d[0] = buf[x * 4 + 2];
				d[1] = buf[x * 4 + 1];
				d[2] = buf[x * 4];
				d[3] = buf[x * 4 + 3];
			}
		}
	}
	operator_delete_array(buf);
	FileIO_Close(f);
	return 0;
}

static int TEX_Load(const char *name, CBmp *dest, uint32_t flags) {
	texCab_t *c = (&g_TexHeader);

	(void)flags;
	CHECK(name && dest);
	void *f = FileIO_Open(name, "rb");
	if (!f) {
		return -1;
	}
	if (FileIO_Read(c, sizeof(*c), 1, f) == 1 && c->magic == TEX_MAGIC && c->version == 1 && c->width > 0 &&
		c->height > 0) {
		CBmp_Create(dest, c->width, c->height);
		if (FileIO_Read(dest->img, c->height * c->width * BMP_PIXEL, 1, f) == 1) {
			FileIO_Close(f);
			return 0;
		}
	}
	return Img_LoadFailed(f, "leer_TEX(): Formato no v\xe1lido: '%s'", name);
}

/* r g b from b g r and the alpha of 32 bit files */
static void Tga_GetPixel(byte *d, int alpha, void *f) {
	d[2] = FileIO_Getc(f);
	d[1] = FileIO_Getc(f);
	d[0] = FileIO_Getc(f);
	d[3] = alpha ? FileIO_Getc(f) : 0;
}

static int TGA_Load(const char *name, CBmp *dest, uint32_t flags) {
	tgaCab_t *c = (&g_TgaCab);

	(void)flags;
	CHECK(name && dest);
	void *f = FileIO_Open(name, "rb");
	if (!f) {
		return -1;
	}
	if (FileIO_Read(c, sizeof(*c), 1, f) != 1 || c->cmapType || (c->type != TGA_TYPE_RLE && c->type != TGA_TYPE_RGB) ||
		(c->bits != 24 && c->bits != 32)) {
		return Img_LoadFailed(f, "leer_TGA(): Formato no v\xe1lido: '%s'", name);
	}
	CHECK(!c->idLength);
	CBmp_Create(dest, c->width, c->height);
	for (int y = 0; y < dest->height; y++) {
		byte *row = (byte *)CBmp_Pixel(dest, 0, y);
		if (!(c->desc & TGA_TOP_DOWN)) {
			row = (byte *)CBmp_Pixel(dest, 0, dest->height - y - 1);
		}
		int x = 0;
		while (x < dest->width) {
			if (c->type == TGA_TYPE_RLE) {
				int h = FileIO_Getc(f);
				int n = (h & 0x7f) + 1;
				if (h & TGA_RUN) {
					byte p[4];
					Tga_GetPixel(p, c->bits == 32, f);
					for (; n > 0; n--, x++) {
						memcpy(row + x * 4, p, 4);
					}
				} else {
					for (; n > 0; n--, x++) {
						Tga_GetPixel(row + x * 4, c->bits == 32, f);
					}
				}
			} else {
				Tga_GetPixel(row + x * 4, c->bits == 32, f);
				x++;
			}
			if (x > dest->width) {
				CHECK(x <= dest->width);
			}
		}
	}
	FileIO_Close(f);
	return 0;
}

int CBmp_Load(CBmp *self, const char *name, uint32_t flags) {
	CHECK(name);
	Img_Free(self, BMP_PIXEL);
	if (Img_IsExt(name, ".tex")) {
		return TEX_Load(name, self, flags);
	}
	if (Img_IsExt(name, ".tga")) {
		return TGA_Load(name, self, flags);
	}
	return BMP_Load(name, self, flags);
}
