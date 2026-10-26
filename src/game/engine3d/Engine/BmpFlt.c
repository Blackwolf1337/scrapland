/*
 * Float images of the engine3d and their TXF files, original sources Engine/BmpFlt.cpp (RGBA) and Engine/BmpFlt2.cpp
 * (RGB), include/scrap/bmpflt.h. The two classes are the same code for another number of channels. The files go
 * through the C runtime of the exe, not the packs.
 */
#include <string.h>

#include "scrap/archio.h"
#include "scrap/bmpflt.h"
#include "scrap/e3dutil.h"

#define CRT_EEXIST 17
#define CRT_SH_DENYRW 0x10
#define CRT_SH_DENYWR 0x20
/* errno for a bad TXF header */
#define g_TXFBadFormat GAME_DATA(int, 0x3de07c)

#define RGBA 4
#define RGB 3

typedef struct {
	uint32_t cab;			/* rva of the header */
	int version;
	int ch;
} txfKind_t;

static float *BmpFlt_Pixel(const CBmpFlt *b, uint32_t x, uint32_t y, int ch) {
	CHECK(b->img);
	CHECK(x < (uint32_t)b->ancho);
	CHECK(y < (uint32_t)b->alto);
	return b->img + (b->ancho * y + x) * ch;
}

/* the original frees the pixels with the sized delete of one pixel */
static void BmpFlt_Free(CBmpFlt *b, int ch) {
	b->ancho = 0;
	b->alto = 0;
	if (b->img) {
		operator_delete_sized(b->img, ch * sizeof(float));
		b->img = NULL;
	}
}

static void BmpFlt_Create(CBmpFlt *b, int ancho, int alto, int ch) {
	BmpFlt_Free(b, ch);
	b->ancho = ancho;
	b->alto = alto;
	b->img = New_Array(ancho * alto, ch * sizeof(float));
}

static int TXF_IsTxf(const char *name) {
	int len = strlen(name);

	return !crt_stricmp(name + (len - 4 > 0 ? len - 4 : 0), ".txf");
}

static int TXF_LoadKind(const txfKind_t *k, const char *name, CBmpFlt *dest, void *extra, int nbytes) {
	TXF_cab *cab = Game_Ptr(k->cab);

	CHECK(name && dest);
	void *f = crt_fsopen(name, "rb", CRT_SH_DENYWR);
	if (!f) {
		return -1;
	}
	if (crt_fread(cab, sizeof(*cab), 1, f) != 1) {
		goto fail;
	}
	if (cab->magic != TXF_MAGIC || cab->version != k->version || cab->ancho <= 0 || cab->alto <= 0) {
		*crt_errno() = g_TXFBadFormat;
		goto fail;
	}
	BmpFlt_Create(dest, cab->ancho, cab->alto, k->ch);
	if (crt_fread(dest->img, cab->alto * cab->ancho * k->ch * sizeof(float), 1, f) != 1) {
		goto fail;
	}
	if (extra && cab->extra && nbytes) {
		CHECK(nbytes == cab->extra);
		if (crt_fread(extra, cab->extra, 1, f) != 1) {
			goto fail;
		}
	}
	if (!crt_fclose(f)) {
		return 0;
	}
fail:
	crt_fclose(f);
	E3D_LogBlue(g_FileLog, "LoadTXF: Read error: '%s' [%s]", name, Err_Str(*crt_errno()));
	return -1;
}

static int TXF_SaveKind(const txfKind_t *k, const char *name, const CBmpFlt *orig, byte flags, const void *extra,
	int nbytes) {
	TXF_cab *cab = Game_Ptr(k->cab);

	CHECK(name && orig && orig->img);
	if (!(flags & TXF_OVERWRITE)) {
		void *old = crt_fopen(name, "rb");
		if (old) {
			crt_fclose(old);
			*crt_errno() = CRT_EEXIST;
			return -1;
		}
	}
	void *f = crt_fsopen(name, "w+b", CRT_SH_DENYRW);
	if (!f) {
		goto log;
	}
	cab->magic = TXF_MAGIC;
	memset(cab->pad, 0, sizeof(cab->pad));
	cab->version = k->version;
	cab->ancho = orig->ancho;
	cab->alto = orig->alto;
	cab->extra = extra ? nbytes : 0;
	if (crt_fwrite(cab, sizeof(*cab), 1, f) != 1 ||
		crt_fwrite(orig->img, cab->alto * cab->ancho * k->ch * sizeof(float), 1, f) != 1) {
		goto fail;
	}
	if (cab->extra && crt_fwrite(extra, cab->extra, 1, f) != 1) {
		goto fail;
	}
	if (!crt_fclose(f)) {
		return 0;
	}
fail:
	crt_fclose(f);
log:
	E3D_LogBlue(g_FileLog, "SaveTXF: Write error: '%s' [%s]", name, Err_Str(*crt_errno()));
	return -1;
}

static int BmpFlt_Load(const txfKind_t *k, CBmpFlt *b, const char *name, void *extra, int nbytes) {
	CHECK(name);
	BmpFlt_Free(b, k->ch);
	if (!TXF_IsTxf(name)) {
		return -1;
	}
	return TXF_LoadKind(k, name, b, extra, nbytes);
}

static int BmpFlt_Save(const txfKind_t *k, CBmpFlt *b, const char *name, byte flags, const void *extra, int nbytes) {
	CHECK(name);
	if (!TXF_IsTxf(name)) {
		return -1;
	}
	return TXF_SaveKind(k, name, b, flags, extra, nbytes);
}

static void BmpFlt_Fill(CBmpFlt *b, const float *color, int ch) {
	CHECK(b->img);
	float *p = b->img;
	for (int n = b->alto * b->ancho; n > 0; n--) {
		memcpy(p, color, ch * sizeof(float));
		p += ch;
	}
}

static void BmpFlt_Blend(CBmpFlt *self, int x, int y, const CBmpFlt *orig, int ox, int oy, int w, int h, int a,
	int b, int ch) {
	CHECK(orig);
	CHECK(orig->img);
	CHECK(self->img);
	if (orig->ancho - ox <= w) {
		w = orig->ancho - ox;
	}
	if (orig->alto - oy <= h) {
		h = orig->alto - oy;
	}
	if (self->ancho - x <= w) {
		w = self->ancho - x;
	}
	if (self->alto - y <= h) {
		h = self->alto - y;
	}
	if (w <= 0 || h <= 0) {
		return;
	}
	float fa = (float)a * (1.0f / 256);
	float fb = (float)b * (1.0f / 256);
	for (int row = 0; row < h; row++) {
		const float *s = BmpFlt_Pixel(orig, ox, oy + row, ch);
		float *d = BmpFlt_Pixel(self, x, y + row, ch);
		for (int i = 0; i < w * ch; i++) {
			d[i] = s[i] * fa + d[i] * fb;
		}
	}
}

/* the start of Resample: a missing image gets the size of orig, the same size with all channels is a copy */
static int BmpFlt_ResampleCopy(CBmpFlt *self, const CBmpFlt *orig, uint32_t mask, int ch) {
	CHECK(orig);
	CHECK(orig->img);
	CHECK(mask);
	if (!self->img) {
		BmpFlt_Create(self, orig->ancho, orig->alto, ch);
		CHECK(self->img);
	}
	if (self->ancho == orig->ancho && self->alto == orig->alto && mask == 0xf) {
		memcpy(self->img, orig->img, self->alto * self->ancho * ch * sizeof(float));
		return 1;
	}
	return 0;
}

static const txfKind_t txfRgba = { TXF_CAB, 1, RGBA };
static const txfKind_t txfRgb = { TXF2_CAB, 2, RGB };

float *__thiscall CBmpFlt_Pixel(const CBmpFlt *self, uint32_t x, uint32_t y) {
	return BmpFlt_Pixel(self, x, y, RGBA);
}

int TXF_Load(const char *name, CBmpFlt *dest, int flags, void *extra, int nbytes) {
	(void)flags;
	return TXF_LoadKind(&txfRgba, name, dest, extra, nbytes);
}

int TXF_Save(const char *name, const CBmpFlt *orig, byte flags, const void *extra, int nbytes) {
	return TXF_SaveKind(&txfRgba, name, orig, flags, extra, nbytes);
}

void __thiscall CBmpFlt_Fill(CBmpFlt *self, const float *color) {
	BmpFlt_Fill(self, color, RGBA);
}

int __thiscall CBmpFlt_Save(CBmpFlt *self, const char *name, byte flags, const void *extra, int nbytes) {
	return BmpFlt_Save(&txfRgba, self, name, flags, extra, nbytes);
}

void __thiscall CBmpFlt_Exposure(CBmpFlt *self, float a, float b) {
	CHECK(self->img);
	float *p = self->img;
	for (int n = self->alto * self->ancho; n > 0; n--) {
		for (int i = 0; i < RGBA; i += 2) {
			double in[2] = { -(a * p[i] / b), -(a * p[i + 1] / b) };
			double out[2];
			Game_ExpPd(in, out);
			p[i] = (1.0f - (float)out[0]) * b;
			p[i + 1] = (1.0f - (float)out[1]) * b;
		}
		p += RGBA;
	}
}

void __thiscall CBmpFlt_Create(CBmpFlt *self, int ancho, int alto) {
	BmpFlt_Create(self, ancho, alto, RGBA);
}

int __thiscall CBmpFlt_Load(CBmpFlt *self, const char *name, int flags, void *extra, int nbytes) {
	(void)flags;
	return BmpFlt_Load(&txfRgba, self, name, extra, nbytes);
}

void __thiscall CBmpFlt_Blend(CBmpFlt *self, int x, int y, const CBmpFlt *orig, int ox, int oy, int w, int h, int a,
	int b) {
	BmpFlt_Blend(self, x, y, orig, ox, oy, w, h, a, b, RGBA);
}

/* the first source row or column of destination i */
static int BmpFlt_BoxStart(int i, float scale) {
	return (int)((double)((float)i * scale) + 1e-4);
}

void __thiscall CBmpFlt_Resample(CBmpFlt *self, const CBmpFlt *orig, uint32_t mask) {
	if (BmpFlt_ResampleCopy(self, orig, mask, RGBA)) {
		return;
	}
	float sx = (float)orig->ancho / (float)self->ancho;
	float sy = (float)orig->alto / (float)self->alto;
	for (int y = 0; y < self->alto; y++) {
		int y0 = BmpFlt_BoxStart(y, sy);
		int y1 = BmpFlt_BoxStart(y + 1, sy);
		if (y1 == y0) {
			y1++;
		}
		for (int x = 0; x < self->ancho; x++) {
			int x0 = BmpFlt_BoxStart(x, sx);
			int x1 = BmpFlt_BoxStart(x + 1, sx);
			if (x1 == x0) {
				x1++;
			}
			float sum[RGBA] = { 0.0f, 0.0f, 0.0f, 0.0f };
			int n = 0;
			for (int ix = x0; ix < x1; ix++) {
				if (y0 < y1) {
					n += y1 - y0;
				}
				for (int iy = y0; iy < y1; iy++) {
					const float *p = CBmpFlt_Pixel(orig, ix, iy);
					for (int c = 0; c < RGBA; c++) {
						sum[c] = p[c] + sum[c];
					}
				}
			}
			CHECK(n);
			float c[RGBA];
			memcpy(c, CBmpFlt_Pixel(self, x, y), sizeof(c));
			float fn = (float)(double)(uint32_t)n;
			for (int i = 0; i < RGBA; i++) {
				if (mask & 1u << i) {
					c[i] = sum[i] / fn;
				}
			}
			memcpy(CBmpFlt_Pixel(self, x, y), c, sizeof(c));
		}
	}
}

float *__thiscall CBmpFlt2_Pixel(const CBmpFlt2 *self, uint32_t x, uint32_t y) {
	return BmpFlt_Pixel(self, x, y, RGB);
}

int TXF2_Load(const char *name, CBmpFlt2 *dest, int flags, void *extra, int nbytes) {
	(void)flags;
	return TXF_LoadKind(&txfRgb, name, dest, extra, nbytes);
}

int TXF2_Save(const char *name, const CBmpFlt2 *orig, byte flags, const void *extra, int nbytes) {
	return TXF_SaveKind(&txfRgb, name, orig, flags, extra, nbytes);
}

void __thiscall CBmpFlt2_Fill(CBmpFlt2 *self, const float *color) {
	BmpFlt_Fill(self, color, RGB);
}

void __thiscall CBmpFlt2_Scale(CBmpFlt2 *self, float k) {
	CHECK(self->img);
	if (k == 1.0f) {
		return;
	}
	float *p = self->img;
	for (int n = self->alto * self->ancho; n > 0; n--) {
		for (int i = 0; i < RGB; i++) {
			p[i] = p[i] * k;
		}
		p += RGB;
	}
}

int __thiscall CBmpFlt2_Save(CBmpFlt2 *self, const char *name, byte flags, const void *extra, int nbytes) {
	return BmpFlt_Save(&txfRgb, self, name, flags, extra, nbytes);
}

void __thiscall CBmpFlt2_Exposure(CBmpFlt2 *self, float a, float b) {
	CHECK(self->img);
	float *p = self->img;
	for (int n = self->alto * self->ancho; n > 0; n--) {
		for (int i = 0; i < RGB; i++) {
			p[i] = (1.0f - (float)Game_Exp((double)-(p[i] * a / b))) * b;
		}
		p += RGB;
	}
}

void __thiscall CBmpFlt2_Gamma(CBmpFlt2 *self, float g, float b) {
	CHECK(self->img);
	if (g == 1.0f) {
		return;
	}
	float inv = 1.0f / g;
	float *p = self->img;
	for (int n = self->alto * self->ancho; n > 0; n--) {
		for (int i = 0; i < RGB; i++) {
			if (0.0f > p[i]) {
				p[i] = 0.0f;
			}
		}
		for (int i = 0; i < RGB; i++) {
			p[i] = (float)Game_Pow((double)(p[i] / b), (double)inv) * b;
		}
		p += RGB;
	}
}

void __thiscall CBmpFlt2_Create(CBmpFlt2 *self, int ancho, int alto) {
	BmpFlt_Create(self, ancho, alto, RGB);
}

int __thiscall CBmpFlt2_Load(CBmpFlt2 *self, const char *name, int flags, void *extra, int nbytes) {
	(void)flags;
	return BmpFlt_Load(&txfRgb, self, name, extra, nbytes);
}

void __thiscall CBmpFlt2_Blend(CBmpFlt2 *self, int x, int y, const CBmpFlt2 *orig, int ox, int oy, int w, int h,
	int a, int b) {
	BmpFlt_Blend(self, x, y, orig, ox, oy, w, h, a, b, RGB);
}

void __thiscall CBmpFlt2_Resample(CBmpFlt2 *self, const CBmpFlt2 *orig, uint32_t mask) {
	if (!BmpFlt_ResampleCopy(self, orig, mask, RGB)) {
		CHECK(0);
	}
}
