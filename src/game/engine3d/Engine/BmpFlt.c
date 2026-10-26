/*
 * Float RGBA images of the engine3d and their TXF files, original source Engine/BmpFlt.cpp (include/scrap/bmpflt.h).
 * The files go through the C runtime of the exe, not the packs.
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

#define BMPFLT_PIXEL (4 * sizeof(float))

float *__thiscall CBmpFlt_Pixel(const CBmpFlt *self, uint32_t x, uint32_t y) {
	CHECK(self->img);
	CHECK(x < (uint32_t)self->ancho);
	CHECK(y < (uint32_t)self->alto);
	return self->img + (self->ancho * y + x) * 4;
}

/* the original frees the pixels with the sized delete of one pixel */
static void CBmpFlt_Free(CBmpFlt *self) {
	self->ancho = 0;
	self->alto = 0;
	if (self->img) {
		operator_delete_sized(self->img, BMPFLT_PIXEL);
		self->img = NULL;
	}
}

static int TXF_IsTxf(const char *name) {
	int len = strlen(name);

	return !crt_stricmp(name + (len - 4 > 0 ? len - 4 : 0), ".txf");
}

int TXF_Load(const char *name, CBmpFlt *dest, int flags, void *extra, int nbytes) {
	TXF_cab *cab = g_TXFCab;

	(void)flags;
	CHECK(name && dest);
	void *f = crt_fsopen(name, "rb", CRT_SH_DENYWR);
	if (!f) {
		return -1;
	}
	if (crt_fread(cab, sizeof(*cab), 1, f) != 1) {
		goto fail;
	}
	if (cab->magic != TXF_MAGIC || cab->version != TXF_VERSION || cab->ancho <= 0 || cab->alto <= 0) {
		*crt_errno() = g_TXFBadFormat;
		goto fail;
	}
	CBmpFlt_Create(dest, cab->ancho, cab->alto);
	if (crt_fread(dest->img, cab->alto * cab->ancho * BMPFLT_PIXEL, 1, f) != 1) {
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

int TXF_Save(const char *name, const CBmpFlt *orig, byte flags, const void *extra, int nbytes) {
	TXF_cab *cab = g_TXFCab;

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
	cab->version = TXF_VERSION;
	cab->ancho = orig->ancho;
	cab->alto = orig->alto;
	cab->extra = extra ? nbytes : 0;
	if (crt_fwrite(cab, sizeof(*cab), 1, f) != 1 ||
		crt_fwrite(orig->img, cab->alto * cab->ancho * BMPFLT_PIXEL, 1, f) != 1) {
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

void __thiscall CBmpFlt_Fill(CBmpFlt *self, const float *color) {
	CHECK(self->img);
	float *p = self->img;
	for (int n = self->alto * self->ancho; n > 0; n--) {
		memcpy(p, color, BMPFLT_PIXEL);
		p += 4;
	}
}

int __thiscall CBmpFlt_Save(CBmpFlt *self, const char *name, byte flags, const void *extra, int nbytes) {
	CHECK(name);
	if (!TXF_IsTxf(name)) {
		return -1;
	}
	return TXF_Save(name, self, flags, extra, nbytes);
}

void __thiscall CBmpFlt_Exposure(CBmpFlt *self, float a, float b) {
	CHECK(self->img);
	float *p = self->img;
	for (int n = self->alto * self->ancho; n > 0; n--) {
		for (int i = 0; i < 4; i += 2) {
			double in[2] = { -(a * p[i] / b), -(a * p[i + 1] / b) };
			double out[2];
			Game_ExpPd(in, out);
			p[i] = (1.0f - (float)out[0]) * b;
			p[i + 1] = (1.0f - (float)out[1]) * b;
		}
		p += 4;
	}
}

void __thiscall CBmpFlt_Create(CBmpFlt *self, int ancho, int alto) {
	CBmpFlt_Free(self);
	self->ancho = ancho;
	self->alto = alto;
	self->img = New_Array(ancho * alto, BMPFLT_PIXEL);
}

int __thiscall CBmpFlt_Load(CBmpFlt *self, const char *name, int flags, void *extra, int nbytes) {
	CHECK(name);
	CBmpFlt_Free(self);
	if (!TXF_IsTxf(name)) {
		return -1;
	}
	return TXF_Load(name, self, flags, extra, nbytes);
}

void __thiscall CBmpFlt_Blend(CBmpFlt *self, int x, int y, const CBmpFlt *orig, int ox, int oy, int w, int h, int a,
	int b) {
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
		const float *s = CBmpFlt_Pixel(orig, ox, oy + row);
		float *d = CBmpFlt_Pixel(self, x, y + row);
		for (int i = 0; i < w * 4; i++) {
			d[i] = s[i] * fa + d[i] * fb;
		}
	}
}

/* the first source row or column of destination i */
static int BmpFlt_BoxStart(int i, float scale) {
	return (int)((double)((float)i * scale) + 1e-4);
}

void __thiscall CBmpFlt_Resample(CBmpFlt *self, const CBmpFlt *orig, uint32_t mask) {
	CHECK(orig);
	CHECK(orig->img);
	CHECK(mask);
	if (!self->img) {
		CBmpFlt_Create(self, orig->ancho, orig->alto);
		CHECK(self->img);
	}
	if (self->ancho == orig->ancho && self->alto == orig->alto && mask == 0xf) {
		memcpy(self->img, orig->img, self->alto * self->ancho * BMPFLT_PIXEL);
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
			float sum[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			int n = 0;
			for (int ix = x0; ix < x1; ix++) {
				if (y0 < y1) {
					n += y1 - y0;
				}
				for (int iy = y0; iy < y1; iy++) {
					const float *p = CBmpFlt_Pixel(orig, ix, iy);
					for (int c = 0; c < 4; c++) {
						sum[c] = p[c] + sum[c];
					}
				}
			}
			CHECK(n);
			float c[4];
			memcpy(c, CBmpFlt_Pixel(self, x, y), sizeof(c));
			float fn = (float)(double)(uint32_t)n;
			for (int i = 0; i < 4; i++) {
				if (mask & 1u << i) {
					c[i] = sum[i] / fn;
				}
			}
			memcpy(CBmpFlt_Pixel(self, x, y), c, sizeof(c));
		}
	}
}
