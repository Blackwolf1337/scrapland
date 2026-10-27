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
/* errno for a bad file (also TXF) */
#define g_ImgBadFormat GAME_DATA(int, 0x3de07c)

typedef struct __attribute__((packed)) {
	uint16_t type;			/* 0x0 "BM" */
	uint32_t size;			/* 0x2 */
	uint32_t reserved;		/* 0x6 */
	uint32_t offBits;		/* 0xa */
} bmpFileCab_t;
SD_SIZE(bmpFileCab_t, 0xe);

typedef struct {
	uint32_t size;			/* 0x00 */
	int32_t ancho;			/* 0x04 */
	int32_t alto;			/* 0x08 bottom up */
	uint16_t planes;		/* 0x0c */
	uint16_t bits;			/* 0x0e */
	uint32_t compression;		/* 0x10 */
	uint32_t sizeImage;		/* 0x14 */
	int32_t xPels;			/* 0x18 */
	int32_t yPels;			/* 0x1c */
	uint32_t clrUsed;		/* 0x20 */
	uint32_t clrImportant;		/* 0x24 */
} bmpInfoCab_t;
SD_SIZE(bmpInfoCab_t, 0x28);

typedef struct {
	uint32_t magic;			/* 0x00 "TEX\x1a" */
	int32_t version;		/* 0x04 1 */
	int32_t ancho;			/* 0x08 */
	int32_t alto;			/* 0x0c */
	uint32_t pad[4];		/* 0x10 */
} texCab_t;
SD_SIZE(texCab_t, 0x20);

typedef struct __attribute__((packed)) {
	byte idLength;			/* 0x00 */
	byte cmapType;			/* 0x01 */
	byte type;			/* 0x02 2 true color, 10 RLE */
	byte cmap[5];			/* 0x03 */
	uint16_t x, y;			/* 0x08 */
	uint16_t ancho, alto;		/* 0x0c */
	byte bits;			/* 0x10 */
	byte desc;			/* 0x11 0x20 top down */
} tgaCab_t;
SD_SIZE(tgaCab_t, 0x12);

#define TEX_MAGIC 0x1a584554
#define BMP_TYPE 0x4d42
#define TGA_TYPE_RGB 2
#define TGA_TYPE_RLE 10
#define TGA_TOP_DOWN 0x20
#define TGA_RUN 0x80		/* packet header: a run, else raw pixels */
#define TGA_PACKET_MAX 128

#define g_BmpFileCab (&GAME_DATA(bmpFileCab_t, 0x4dbf08))
#define g_BmpInfoCab (&GAME_DATA(bmpInfoCab_t, 0x4dbf18))
#define g_TexCab (&GAME_DATA(texCab_t, 0x4dbf40))
#define g_TgaCab (&GAME_DATA(tgaCab_t, 0x4dbf60))

void *Img_Pixel(const CImagen *b, uint32_t x, uint32_t y, int bytes) {
	CHECK(b->img);
	CHECK(x < (uint32_t)b->ancho);
	CHECK(y < (uint32_t)b->alto);
	return (byte *)b->img + (b->ancho * y + x) * bytes;
}

void Img_Free(CImagen *b, int bytes) {
	b->ancho = 0;
	b->alto = 0;
	if (b->img) {
		operator_delete_sized(b->img, bytes);
		b->img = NULL;
	}
}

void __fastcall CBmp_dtor(CBmp *self) {
	Img_Free(self, BMP_PIXEL);
}

void Img_Create(CImagen *b, int ancho, int alto, int bytes) {
	Img_Free(b, bytes);
	b->ancho = ancho;
	b->alto = alto;
	b->img = New_Array(ancho * alto, bytes);
}

int Img_IsExt(const char *name, const char *ext) {
	int len = strlen(name);

	return !crt_stricmp(name + (len - 4 > 0 ? len - 4 : 0), ext);
}

int Img_Clip(const CImagen *self, int x, int y, const CImagen *orig, int ox, int oy, int *w, int *h) {
	if (orig->ancho - ox <= *w) {
		*w = orig->ancho - ox;
	}
	if (orig->alto - oy <= *h) {
		*h = orig->alto - oy;
	}
	if (self->ancho - x <= *w) {
		*w = self->ancho - x;
	}
	if (self->alto - y <= *h) {
		*h = self->alto - y;
	}
	return *w > 0 && *h > 0;
}

int Img_BoxStart(int i, float scale) {
	return (int)((double)((float)i * scale) + 1e-4);
}

int Img_ResampleCopy(CImagen *self, const CImagen *orig, uint32_t mask, int bytes) {
	CHECK(orig);
	CHECK(orig->img);
	CHECK(mask);
	if (!self->img) {
		Img_Create(self, orig->ancho, orig->alto, bytes);
		CHECK(self->img);
	}
	if (self->ancho == orig->ancho && self->alto == orig->alto && mask == 0xf) {
		memcpy(self->img, orig->img, self->alto * self->ancho * bytes);
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

uint32_t Color_Lerp(uint32_t a, uint32_t b, float t) {
	int k = (int)(t * 256.0f);
	uint32_t r = 0;

	for (int s = 24; s >= 0; s -= 8) {
		int c = ((int)(a >> s & 0xff) * (0x100 - k) + (int)(b >> s & 0xff) * k) >> 8;
		r = r << 8 | Bmp_Clamp(c);
	}
	return r;
}

uint32_t *__thiscall CBmp_Pixel(const CBmp *self, uint32_t x, uint32_t y) {
	return Img_Pixel(self, x, y, BMP_PIXEL);
}

void __thiscall CBmp_Create(CBmp *self, int ancho, int alto) {
	Img_Create(self, ancho, alto, BMP_PIXEL);
}

void __thiscall CBmp_Fill(CBmp *self, uint32_t color) {
	CHECK(self->img);
	uint32_t *p = self->img;
	for (int n = self->alto * self->ancho; n > 0; n--) {
		*p++ = color;
	}
}

void __thiscall CBmp_Copy(CBmp *self, int x, int y, const CBmp *orig, int ox, int oy, int w, int h, uint32_t mask) {
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

void __thiscall CBmp_Effect(CBmp *self, int kind) {
	CHECK(self->img);
	int w = self->ancho, h = self->alto;
	if (kind == 1) {
		for (int y = 0; y < h; y++) {
			byte *p = (byte *)CBmp_Pixel(self, 0, y);
			if (w - 2 <= 2) {
				continue;
			}
			/* the pixels to the left are taken before they change */
			byte l2[4], l1[4];
			memcpy(l2, p, 4);
			memcpy(l1, p + 4, 4);
			for (int x = 2; x < w - 2; x++) {
				byte *c = p + x * 4;
				byte cur[4];
				memcpy(cur, c, 4);
				for (int i = 0; i < 3; i++) {
					c[i] = (byte)((unsigned)(l1[i] + l2[i] + c[8 + i] + c[4 + i] + c[i]) / 5);
				}
				memcpy(l2, l1, 4);
				memcpy(l1, cur, 4);
			}
		}
	} else if (kind == 2) {
		for (int x = 0; x < w; x++) {
			byte *p = (byte *)CBmp_Pixel(self, x, h - 1);
			p[0] = (crt_rand() & 0x7f) + 0x80;
			p[1] = crt_rand();
			p[2] = 0;
		}
		for (int y = 0; y < h - 1; y++) {
			byte *row = (byte *)CBmp_Pixel(self, 0, y);
			const byte *next = row + w * 4;
			for (int x = 1; x < w - 1; x++) {
				for (int i = 0; i < 3; i++) {
					const byte *n = next + x * 4 + i;
					row[x * 4 + i] = (byte)((((unsigned)n[-4] + n[0] + n[4]) * 0x3f + row[x * 4 + i] * 0x40u) >> 8);
				}
			}
		}
	} else if (kind == 3) {
		for (int y = 0; y < h; y++) {
			byte *p = (byte *)CBmp_Pixel(self, 0, y);
			for (int x = 0; x < w; x++, p += 4) {
				for (int i = 0; i < 3; i++) {
					p[i] = (byte)(p[i] * 3 >> 2);
				}
			}
		}
	}
}

void __thiscall CBmp_Exposure(CBmp *self, float a, float b) {
	CHECK(self->img);
	byte *p = self->img;
	for (int n = self->alto * self->ancho; n > 0; n--, p += 4) {
		for (int i = 0; i < 4; i++) {
			double e = Game_Exp((double)-((float)p[i] * a / b));
			p[i] = Bmp_Clamp((int)X87_Fistp((1.0 - e) * (double)b));
		}
	}
}

void __thiscall CBmp_Gamma(CBmp *self, float g, float b) {
	CHECK(self->img);
	if (g == 1.0f) {
		return;
	}
	float inv = 1.0f / g;
	byte *p = self->img;
	for (int n = self->alto * self->ancho; n > 0; n--, p += 4) {
		for (int i = 0; i < 4; i++) {
			p[i] = Bmp_Clamp((int)X87_Fistp(Game_Pow((double)((float)p[i] / b), (double)inv) * (double)b));
		}
	}
}

void __thiscall CBmp_MixChannels(CBmp *self, uint32_t in, uint32_t out) {
	CHECK(in && out);
	CHECK(self->img);
	for (int y = 0; y < self->alto; y++) {
		for (int x = 0; x < self->ancho; x++) {
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

void __thiscall CBmp_Blend(CBmp *self, int x, int y, const CBmp *orig, int ox, int oy, int w, int h, int a, int b) {
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

uint32_t *__thiscall CBmp_Sample(const CBmp *self, uint32_t *out, float u, float v) {
	CHECK(u >= 0.0f && u <= 1.0f);
	CHECK(v >= 0.0f && v <= 1.0f);
	CHECK(self->img);
	const uint32_t *img = self->img;
	float fx = (float)self->ancho * u - 0.5f;
	float fy = (float)self->alto * v - 0.5f;
	int ix = (int)fx, iy = (int)fy;
	int x0 = ix < self->ancho - 1 ? ix : self->ancho - 1;
	int x1 = ix + 1 < self->ancho - 1 ? ix + 1 : self->ancho - 1;
	int y0 = iy < self->alto - 1 ? iy : self->alto - 1;
	int y1 = iy + 1 < self->alto - 1 ? iy + 1 : self->alto - 1;
	float tx = Bmp_Frac(fx - (float)ix);
	float ty = Bmp_Frac(fy - (float)iy);
	uint32_t c1 = Color_Lerp(Bmp_SwapRB(img[x0 + y1 * self->ancho]), Bmp_SwapRB(img[x1 + y1 * self->ancho]), tx);
	uint32_t c0 = Color_Lerp(Bmp_SwapRB(img[x0 + y0 * self->ancho]), Bmp_SwapRB(img[x1 + y0 * self->ancho]), tx);
	*out = Bmp_SwapRB(Color_Lerp(c0, c1, ty));
	return out;
}

/* a 3x3 kernel on the inner pixels, read from a copy */
static void Bmp_Filter3x3(CBmp *self, const int k[9], int round, int shift) {
	CImagen tmp;

	CHECK(self->img);
	tmp.img = NULL;
	CBmp_Resample(&tmp, self, 0xf);
	for (int y = 0; y < self->alto; y++) {
		const byte *s = (const byte *)CBmp_Pixel(&tmp, 0, y);
		byte *d = (byte *)CBmp_Pixel(self, 0, y);
		for (int x = 0; x < self->ancho; x++) {
			if (x <= 0 || y <= 0 || x >= self->ancho - 1 || y >= self->alto - 1) {
				continue;
			}
			int stride = self->ancho * 4;
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

void __fastcall CBmp_Sharpen(CBmp *self) {
	static const int k[9] = { -1, -3, -1, -3, 48, -3, -1, -3, -1 };

	Bmp_Filter3x3(self, k, 0x10, 5);
}

void __fastcall CBmp_Blur(CBmp *self) {
	static const int k[9] = { 1, 2, 1, 2, 4, 2, 1, 2, 1 };

	Bmp_Filter3x3(self, k, 8, 4);
}

void __thiscall CBmp_Resample(CBmp *self, const CBmp *orig, uint32_t mask) {
	if (Img_ResampleCopy(self, orig, mask, BMP_PIXEL)) {
		return;
	}
	if (orig->ancho == self->ancho * 2 && orig->alto == self->alto * 2 && mask == 0xf) {
		for (int y = 0; y < self->alto; y++) {
			const byte *s = (const byte *)CBmp_Pixel(orig, 0, y * 2);
			const byte *s2 = s + orig->ancho * 4;
			byte *d = (byte *)CBmp_Pixel(self, 0, y);
			for (int x = 0; x < self->ancho; x++) {
				for (int i = 0; i < 4; i++) {
					d[x * 4 + i] = (byte)((unsigned)(s2[x * 8 + 4 + i] + s[x * 8 + i] + s[x * 8 + 4 + i] + s2[x * 8 + i]) >> 2);
				}
			}
		}
		return;
	}
	float sx = (float)orig->ancho / (float)self->ancho;
	float sy = (float)orig->alto / (float)self->alto;
	for (int y = 0; y < self->alto; y++) {
		int y0 = Img_BoxStart(y, sy);
		int y1 = Img_BoxStart(y + 1, sy);
		if (y1 == y0) {
			y1++;
		}
		for (int x = 0; x < self->ancho; x++) {
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

int BMP_Save(const char *name, const CBmp *orig, uint32_t flags) {
	bmpFileCab_t *fc = g_BmpFileCab;
	bmpInfoCab_t *ic = g_BmpInfoCab;

	CHECK(name && orig && orig->img);
	if (Img_SaveRefused(name, flags)) {
		return -1;
	}
	void *f = File_OpenRel(name, "wb");
	if (!f) {
		return -1;
	}
	fc->type = BMP_TYPE;
	uint32_t size = (orig->ancho + 1) * 3 & ~3u;
	fc->offBits = sizeof(*fc) + sizeof(*ic);
	fc->size = orig->alto * size + sizeof(*fc) + sizeof(*ic);
	fc->reserved = 0;
	if (crt_fwrite(fc, sizeof(*fc), 1, f) != 1) {
		goto fail;
	}
	memset(ic, 0, sizeof(*ic));
	ic->size = sizeof(*ic);
	ic->ancho = orig->ancho;
	ic->alto = orig->alto;
	ic->planes = 1;
	ic->bits = 24;
	if (crt_fwrite(ic, sizeof(*ic), 1, f) != 1) {
		goto fail;
	}
	/* the padding of the rows is what new[] left there */
	byte *buf = operator_new_array(size);
	for (int y = 0; y < ic->alto; y++) {
		const byte *s = (const byte *)CBmp_Pixel(orig, 0, ic->alto - y - 1);
		for (int x = 0; x < ic->ancho; x++) {
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
	E3D_LogBlue(g_FileLog, "escr_BMP_24(): Error de escritura: '%s'", name);
	return -1;
}

int TEX_Save(const char *name, const CBmp *orig, uint32_t flags) {
	texCab_t *c = g_TexCab;

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
	c->ancho = orig->ancho;
	c->alto = orig->alto;
	if (crt_fwrite(c, sizeof(*c), 1, f) == 1 && crt_fwrite(orig->img, c->alto * c->ancho * BMP_PIXEL, 1, f) == 1) {
		crt_fclose(f);
		return 0;
	}
	crt_fclose(f);
	E3D_LogBlue(g_FileLog, "escr_TEX(): Error de escritura: '%s'", name);
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

int TGA_Save(const char *name, const CBmp *orig, uint32_t flags) {
	tgaCab_t *c = g_TgaCab;

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
	c->ancho = orig->ancho;
	c->alto = orig->alto;
	c->desc = TGA_TOP_DOWN;
	c->bits = alpha ? 32 : 24;
	if (crt_fwrite(c, sizeof(*c), 1, f) != 1) {
		crt_fclose(f);
		E3D_LogBlue(g_FileLog, "escr_TGA(): Error de escritura: '%s'", name);
		return -1;
	}
	for (int y = 0; y < orig->alto; y++) {
		Tga_PutRow(CBmp_Pixel(orig, 0, y), orig->ancho, alpha ? 0xffffffff : 0xffffff, alpha, f);
	}
	crt_fclose(f);
	return 0;
}

int __thiscall CBmp_Save(CBmp *self, const char *name, uint32_t flags) {
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
	ArchIO_Close(f);
	*crt_errno() = g_ImgBadFormat;
	E3D_LogBlue(g_FileLog, fmt, name);
	return -1;
}

int BMP_Load(const char *name, CBmp *dest, uint32_t flags) {
	static const char bad[] = "leer_BMP_24(): Formato no v\xe1lido: '%s'";
	bmpFileCab_t *fc = g_BmpFileCab;
	bmpInfoCab_t *ic = g_BmpInfoCab;
	byte pal[256 * 4];

	CHECK(name && dest);
	void *f = ArchIO_Open(name, "rb");
	if (!f) {
		return -1;
	}
	if (ArchIO_Read(fc, sizeof(*fc), 1, f) != 1 || ArchIO_Read(ic, sizeof(*ic), 1, f) != 1 || fc->type != BMP_TYPE ||
		ic->size != sizeof(*ic) || ic->planes != 1 || (ic->bits != 32 && ic->bits != 24 && ic->bits != 8) ||
		ic->compression || ic->ancho <= 0 || ic->alto <= 0) {
		return Img_LoadFailed(f, bad, name);
	}
	if (ic->bits == 8) {
		if (!ic->clrUsed) {
			ic->clrUsed = 256;
		} else if (ic->clrUsed > 256) {
			return Img_LoadFailed(f, bad, name);
		}
		if (ArchIO_Read(pal, ic->clrUsed * 4, 1, f) != 1) {
			return Img_LoadFailed(f, bad, name);
		}
	}
	CHECK(fc->offBits == (uint32_t)ArchIO_Tell(f));
	CBmp_Create(dest, ic->ancho, ic->alto);
	uint32_t size = (((ic->bits + 7) >> 3) * ic->ancho + 3) & ~3u;
	byte *buf = operator_new_array(size);
	for (int y = 0; y < ic->alto; y++) {
		if (ArchIO_Read(buf, size, 1, f) != 1) {
			return Img_LoadFailed(f, bad, name);
		}
		byte *d = (byte *)CBmp_Pixel(dest, 0, ic->alto - y - 1);
		for (int x = 0; x < ic->ancho; x++, d += 4) {
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
	ArchIO_Close(f);
	return 0;
}

int TEX_Load(const char *name, CBmp *dest, uint32_t flags) {
	texCab_t *c = g_TexCab;

	(void)flags;
	CHECK(name && dest);
	void *f = ArchIO_Open(name, "rb");
	if (!f) {
		return -1;
	}
	if (ArchIO_Read(c, sizeof(*c), 1, f) == 1 && c->magic == TEX_MAGIC && c->version == 1 && c->ancho > 0 &&
		c->alto > 0) {
		CBmp_Create(dest, c->ancho, c->alto);
		if (ArchIO_Read(dest->img, c->alto * c->ancho * BMP_PIXEL, 1, f) == 1) {
			ArchIO_Close(f);
			return 0;
		}
	}
	return Img_LoadFailed(f, "leer_TEX(): Formato no v\xe1lido: '%s'", name);
}

/* r g b from b g r and the alpha of 32 bit files */
static void Tga_GetPixel(byte *d, int alpha, void *f) {
	d[2] = ArchIO_Getc(f);
	d[1] = ArchIO_Getc(f);
	d[0] = ArchIO_Getc(f);
	d[3] = alpha ? ArchIO_Getc(f) : 0;
}

int TGA_Load(const char *name, CBmp *dest, uint32_t flags) {
	tgaCab_t *c = g_TgaCab;

	(void)flags;
	CHECK(name && dest);
	void *f = ArchIO_Open(name, "rb");
	if (!f) {
		return -1;
	}
	if (ArchIO_Read(c, sizeof(*c), 1, f) != 1 || c->cmapType || (c->type != TGA_TYPE_RLE && c->type != TGA_TYPE_RGB) ||
		(c->bits != 24 && c->bits != 32)) {
		return Img_LoadFailed(f, "leer_TGA(): Formato no v\xe1lido: '%s'", name);
	}
	CHECK(!c->idLength);
	CBmp_Create(dest, c->ancho, c->alto);
	for (int y = 0; y < dest->alto; y++) {
		byte *row = (byte *)CBmp_Pixel(dest, 0, y);
		if (!(c->desc & TGA_TOP_DOWN)) {
			row = (byte *)CBmp_Pixel(dest, 0, dest->alto - y - 1);
		}
		int x = 0;
		while (x < dest->ancho) {
			if (c->type == TGA_TYPE_RLE) {
				int h = ArchIO_Getc(f);
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
			if (x > dest->ancho) {
				CHECK(x <= dest->ancho);
			}
		}
	}
	ArchIO_Close(f);
	return 0;
}

int __thiscall CBmp_Load(CBmp *self, const char *name, uint32_t flags) {
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
