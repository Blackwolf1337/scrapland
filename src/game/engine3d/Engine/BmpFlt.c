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
#define g_TXFBadFormat GAME_GLOBAL(int, g_ErrPcxFormat)

#define RGBA 4
#define RGB 3

typedef struct {
	int version;			/* 1 RGBA (the header g_TXFCab), 2 RGB (g_TXFCab2) */
	int ch;
} txfKind_t;

static TXF_header *TXF_Header(const txfKind_t *k) {
	return k->version == 1 ? g_TXFCab : g_TXFCab2;
}

static float *BmpFlt_Pixel(const CBmpFlt *b, uint32_t x, uint32_t y, int ch) {
	return Img_Pixel(b, x, y, ch * sizeof(float));
}

static void BmpFlt_Free(CBmpFlt *b, int ch) {
	Img_Free(b, ch * sizeof(float));
}

static void BmpFlt_Create(CBmpFlt *b, int width, int height, int ch) {
	Img_Create(b, width, height, ch * sizeof(float));
}

static int TXF_LoadKind(const txfKind_t *k, const char *name, CBmpFlt *dest, void *extra, int nbytes) {
	TXF_header *cab = TXF_Header(k);

	CHECK(name && dest);
	void *f = crt_fsopen(name, "rb", CRT_SH_DENYWR);
	if (!f) {
		return -1;
	}
	if (crt_fread(cab, sizeof(*cab), 1, f) != 1) {
		goto fail;
	}
	if (cab->magic != TXF_MAGIC || cab->version != k->version || cab->width <= 0 || cab->height <= 0) {
		*crt_errno() = g_TXFBadFormat;
		goto fail;
	}
	BmpFlt_Create(dest, cab->width, cab->height, k->ch);
	if (crt_fread(dest->img, cab->height * cab->width * k->ch * sizeof(float), 1, f) != 1) {
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
	TXF_header *cab = TXF_Header(k);

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
	cab->width = orig->width;
	cab->height = orig->height;
	cab->extra = extra ? nbytes : 0;
	if (crt_fwrite(cab, sizeof(*cab), 1, f) != 1 ||
		crt_fwrite(orig->img, cab->height * cab->width * k->ch * sizeof(float), 1, f) != 1) {
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
	if (!Img_IsExt(name, ".txf")) {
		return -1;
	}
	return TXF_LoadKind(k, name, b, extra, nbytes);
}

static int BmpFlt_Save(const txfKind_t *k, CBmpFlt *b, const char *name, byte flags, const void *extra, int nbytes) {
	CHECK(name);
	if (!Img_IsExt(name, ".txf")) {
		return -1;
	}
	return TXF_SaveKind(k, name, b, flags, extra, nbytes);
}

static void BmpFlt_Fill(CBmpFlt *b, const float *color, int ch) {
	CHECK(b->img);
	float *p = b->img;
	for (int n = b->height * b->width; n > 0; n--) {
		memcpy(p, color, ch * sizeof(float));
		p += ch;
	}
}

static void BmpFlt_Blend(CBmpFlt *self, int x, int y, const CBmpFlt *orig, int ox, int oy, int w, int h, int a,
	int b, int ch) {
	CHECK(orig);
	CHECK(orig->img);
	CHECK(self->img);
	if (!Img_Clip(self, x, y, orig, ox, oy, &w, &h)) {
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

static const txfKind_t txfRgba = { 1, RGBA };
static const txfKind_t txfRgb = { 2, RGB };

float *CBmpFlt_Pixel(const CBmpFlt *self, uint32_t x, uint32_t y) {
	return BmpFlt_Pixel(self, x, y, RGBA);
}

int TXF_Load(const char *name, CBmpFlt *dest, int flags, void *extra, int nbytes) {
	(void)flags;
	return TXF_LoadKind(&txfRgba, name, dest, extra, nbytes);
}

void CBmpFlt_Fill(CBmpFlt *self, const float *color) {
	BmpFlt_Fill(self, color, RGBA);
}

int CBmpFlt_Save(CBmpFlt *self, const char *name, byte flags, const void *extra, int nbytes) {
	return BmpFlt_Save(&txfRgba, self, name, flags, extra, nbytes);
}

void CBmpFlt_Exposure(CBmpFlt *self, float a, float b) {
	CHECK(self->img);
	float *p = self->img;
	for (int n = self->height * self->width; n > 0; n--) {
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

void CBmpFlt_Create(CBmpFlt *self, int width, int height) {
	BmpFlt_Create(self, width, height, RGBA);
}

int CBmpFlt_Load(CBmpFlt *self, const char *name, int flags, void *extra, int nbytes) {
	(void)flags;
	return BmpFlt_Load(&txfRgba, self, name, extra, nbytes);
}

void CBmpFlt_Blend(CBmpFlt *self, int x, int y, const CBmpFlt *orig, int ox, int oy, int w, int h, int a,
	int b) {
	BmpFlt_Blend(self, x, y, orig, ox, oy, w, h, a, b, RGBA);
}

void CBmpFlt_Resample(CBmpFlt *self, const CBmpFlt *orig, uint32_t mask) {
	if (Img_ResampleCopy(self, orig, mask, RGBA * sizeof(float))) {
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

float *CBmpFlt2_Pixel(const CBmpFlt2 *self, uint32_t x, uint32_t y) {
	return BmpFlt_Pixel(self, x, y, RGB);
}

int TXF2_Load(const char *name, CBmpFlt2 *dest, int flags, void *extra, int nbytes) {
	(void)flags;
	return TXF_LoadKind(&txfRgb, name, dest, extra, nbytes);
}

void CBmpFlt2_Fill(CBmpFlt2 *self, const float *color) {
	BmpFlt_Fill(self, color, RGB);
}

void CBmpFlt2_Scale(CBmpFlt2 *self, float k) {
	CHECK(self->img);
	if (k == 1.0f) {
		return;
	}
	float *p = self->img;
	for (int n = self->height * self->width; n > 0; n--) {
		for (int i = 0; i < RGB; i++) {
			p[i] = p[i] * k;
		}
		p += RGB;
	}
}

int CBmpFlt2_Save(CBmpFlt2 *self, const char *name, byte flags, const void *extra, int nbytes) {
	return BmpFlt_Save(&txfRgb, self, name, flags, extra, nbytes);
}

void CBmpFlt2_Exposure(CBmpFlt2 *self, float a, float b) {
	CHECK(self->img);
	float *p = self->img;
	for (int n = self->height * self->width; n > 0; n--) {
		for (int i = 0; i < RGB; i++) {
			p[i] = (1.0f - (float)Game_Exp((double)-(p[i] * a / b))) * b;
		}
		p += RGB;
	}
}

void CBmpFlt2_Gamma(CBmpFlt2 *self, float g, float b) {
	CHECK(self->img);
	if (g == 1.0f) {
		return;
	}
	float inv = 1.0f / g;
	float *p = self->img;
	for (int n = self->height * self->width; n > 0; n--) {
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

void CBmpFlt2_Create(CBmpFlt2 *self, int width, int height) {
	BmpFlt_Create(self, width, height, RGB);
}

int CBmpFlt2_Load(CBmpFlt2 *self, const char *name, int flags, void *extra, int nbytes) {
	(void)flags;
	return BmpFlt_Load(&txfRgb, self, name, extra, nbytes);
}

void CBmpFlt2_Blend(CBmpFlt2 *self, int x, int y, const CBmpFlt2 *orig, int ox, int oy, int w, int h,
	int a, int b) {
	BmpFlt_Blend(self, x, y, orig, ox, oy, w, h, a, b, RGB);
}

void CBmpFlt2_Resample(CBmpFlt2 *self, const CBmpFlt2 *orig, uint32_t mask) {
	if (!Img_ResampleCopy(self, orig, mask, RGB * sizeof(float))) {
		CHECK(0);
	}
}
