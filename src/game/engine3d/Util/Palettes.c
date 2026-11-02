/*
 * Palettes of the engine3d, original source Util/Paletas.cpp with the inlines of engine3d/util/paletas.h
 * (include/scrap/e3dutil.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"

/* the shifts of the exe are bytes, the cpu masks them to 5 bits */
#define PAL_SH(v) ((v) & 31)

Pal16Shift g_Pal16Shift;

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

