/*
 * RGBA byte images of the engine3d, original source Engine/bmp.cpp (include/scrap/bmp.h), and the helpers all image
 * classes share.
 */
#include <string.h>

#include "scrap/bmp.h"

#define BMP_PIXEL 4

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
