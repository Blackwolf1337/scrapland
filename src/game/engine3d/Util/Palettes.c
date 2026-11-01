/*
 * Palettes of the engine3d, original source Util/Paletas.cpp with the inlines of engine3d/util/paletas.h
 * (include/scrap/e3dutil.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"

/* the shifts of the exe are bytes, the cpu masks them to 5 bits */
#define PAL_SH(v) ((v) & 31)

uint32_t Pal16_FromRGB(byte r, byte g, byte b) {
	const Pal16Shift *s = &g_Pal16Shift;
	uint32_t c = (uint32_t)(r >> PAL_SH(s->rShr)) << PAL_SH(s->rShl);

	c |= (uint32_t)(g >> PAL_SH(s->gShr)) << PAL_SH(s->gShl);
	c |= (uint32_t)(b >> PAL_SH(s->bShr)) << PAL_SH(s->bShl);
	return c & 0xffff;
}

void Pal16_ToRGB(byte *r, byte *g, byte *b, uint16_t c) {
	const Pal16Shift *s = &g_Pal16Shift;

	CHECK(r && g && b);
	*r = (byte)((uint32_t)c >> PAL_SH(s->rShl) << PAL_SH(s->rShr));
	*g = (byte)((uint32_t)c >> PAL_SH(s->gShl) << PAL_SH(s->gShr));
	*b = (byte)((uint32_t)c >> PAL_SH(s->bShl) << PAL_SH(s->bShr));
}

static int Pal_Abs(int v) {
	return v < 0 ? -v : v;
}

int Pal_Nearest(const byte *pal, int first, int last, int r, int g, int b) {
	int best = first, dist = 10000;

	CHECK(pal);
	for (int i = first; i <= last; i++) {
		const byte *c = pal + i * 4;
		int d = Pal_Abs(g - c[1]) + Pal_Abs(b - c[2]) + Pal_Abs(r - c[0]);
		if (d < dist) {
			if (!d) {
				return i;
			}
			best = i;
			dist = d;
		}
	}
	return best;
}

void Pal_Load(const char *name, byte *dest) {
	byte buf[0x300];

	CHECK(name && dest);
	int fd = crt_open(name, 0x8000);
	if (fd == -1) {
		Err_Report(*crt_errno(), 1, " (%s)", name);
	}
	if (crt_read(fd, buf, sizeof(buf)) == -1) {
		Err_Report(*crt_errno(), 1, " (%s)", name);
	}
	if (crt_close(fd) == -1) {
		Err_Report(*crt_errno(), 1, " (%s)", name);
	}
	for (int i = 0; i < 0x100; i++) {
		dest[i * 4] = (byte)(buf[i * 3] << 2);
		dest[i * 4 + 1] = (byte)(buf[i * 3 + 1] << 2);
		dest[i * 4 + 2] = (byte)(buf[i * 3 + 2] << 2);
		dest[i * 4 + 3] = 5;
	}
}

void Pal_Copy(byte *dst, int i1, const byte *src, int i2, int num) {
	CHECK(dst && src);
	CHECK(num > 0 && i2 > 0 && i1 > 0);
	memmove(dst + i1 * 4, src + i2 * 4, (uint32_t)num * 4);
}

void *Pal_ToPal16(const byte *pal, int num) {
	CHECK(num > 0);
	void *p = Mem_Alloc(num * 2 + 0x200);
	uint16_t *c = PAL16_COLORS(p);
	for (int i = 0; i < num; i++) {
		c[i] = (uint16_t)Pal16_FromRGB(pal[i * 4], pal[i * 4 + 1], pal[i * 4 + 2]);
	}
	return p;
}

/* cvttss2si, then clamped */
static byte Pal_Clamp(float v) {
	int i = (int)v;

	if (i <= 0) {
		return 0;
	}
	return i >= 0xff ? 0xff : (byte)i;
}

void *Pal_ToPal16Scaled(const byte *pal, int num, float r, float g, float b) {
	CHECK(num > 0);
	void *p = Mem_Alloc(num * 2 + 0x200);
	uint16_t *c = PAL16_COLORS(p);
	for (int i = 0; i < num; i++) {
		byte cr = Pal_Clamp((float)pal[i * 4] * r);
		byte cg = Pal_Clamp((float)pal[i * 4 + 1] * g);
		byte cb = Pal_Clamp((float)pal[i * 4 + 2] * b);
		c[i] = (uint16_t)Pal16_FromRGB(cr, cg, cb);
	}
	return p;
}

/* the shifts of one mask: the zero bits below it (shl), 8 less its one bits (shr) */
static void Pal16_Mask(uint32_t m, byte *shr, byte *shl) {
	while (!(m & 1)) {
		(*shl)++;
		m >>= 1;
	}
	do {
		(*shr)--;
		m >>= 1;
	} while (m & 1);
}

void Pal16_SetMasks(uint32_t r, uint32_t g, uint32_t b) {
	Pal16Shift *s = &g_Pal16Shift;

	s->rShr = 8;
	s->rShl = 0;
	s->gShr = 8;
	s->gShl = 0;
	s->bShr = 8;
	s->bShl = 0;
	if (!r || !g || !b) {
		return;
	}
	Pal16_Mask(r, &s->rShr, &s->rShl);
	Pal16_Mask(g, &s->gShr, &s->gShl);
	Pal16_Mask(b, &s->bShr, &s->bShl);
}

void RGB_Blend(int *r, int *g, int *b, int r2, int g2, int b2, int t) {
	if (t <= -32) {
		return;
	}
	if (t >= 32) {
		*r = r2;
		*g = g2;
		*b = b2;
		return;
	}
	uint32_t k = (uint32_t)t + 32, k1 = 64 - k;
	*r = (int)((uint32_t)*r * k1 + k * (uint32_t)r2) >> 6;
	*g = (int)((uint32_t)*g * k1 + k * (uint32_t)g2) >> 6;
	*b = (int)((uint32_t)*b * k1 + k * (uint32_t)b2) >> 6;
}

void Pal16_Scale(void *pal16, int num, float r, float g, float b) {
	CHECK(pal16);
	uint16_t *c = PAL16_COLORS(pal16);
	for (int i = 0; i < num; i++) {
		byte cr, cg, cb;
		Pal16_ToRGB(&cr, &cg, &cb, c[i]);
		cr = Pal_Clamp((float)cr * r);
		cg = Pal_Clamp((float)cg * g);
		cb = Pal_Clamp((float)cb * b);
		c[i] = (uint16_t)Pal16_FromRGB(cr, cg, cb);
	}
}
