/*
 * PCX images of the engine3d, original source Util/Pcx.cpp (include/scrap/e3dutil.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"

#define PCX_RUN 0xc0
#define PCX_COUNT 0x3f

/* the header of an 8 bit image of lx * ly with planes planes */
static void Pcx_InitHeader(PcxHeader *hdr, int lx, int ly, byte planes) {
	memset(hdr, 0, sizeof(*hdr));
	hdr->manuf = 10;
	hdr->version = 5;
	hdr->encoding = 1;
	hdr->bpp = 8;
	hdr->xmax = (int16_t)(lx - 1);
	hdr->ymax = (int16_t)(ly - 1);
	hdr->planes = planes;
	hdr->bpl = (uint16_t)lx;
	hdr->lx = (uint16_t)lx;
	hdr->ly = (uint16_t)ly;
}

/* the runs of one value v (rep times) into buf from pos2 on; pos advanced, the value at the new pos into v */
#define PCX_PUT_RUNS(buf, pos2, pos, rep, v, at) \
	do { \
		int n_ = (rep); \
		if ((rep) > 1 || ((v) & PCX_RUN) == PCX_RUN) { \
			n_ = (rep) < PCX_COUNT ? (rep) : PCX_COUNT; \
			(buf)[(pos2)++] = (byte)n_ | PCX_RUN; \
		} \
		(pos) += n_; \
		(rep) -= n_; \
		(buf)[(pos2)++] = (v); \
		(v) = (at); \
	} while ((rep) > 0)

int Pcx_Write24(void *f, const byte *mem, int lx, int ly) {
	PcxHeader hdr;
	int size = lx * 3;

	CHECK(f && mem);
	CHECK(lx > 0 && ly > 0);
	byte *buf = Mem_Alloc(size * 2);
	Pcx_InitHeader(&hdr, lx, ly, 3);
	hdr.palInfo = 1;
	if (crt_fwrite(&hdr, sizeof(hdr), 1, f) != 1) {
		return -1;
	}
	/* the planes r g b of a line one after the other */
#define PCX_AT24(i) mem[(uint32_t)(i) / (uint32_t)lx + (uint32_t)(i) % (uint32_t)lx * 3]
	for (int y = 0; y < ly; y++) {
		int pos = 0, pos2 = 0;
		if (size > 0) {
			do {
				int rep = 1;
				byte v = PCX_AT24(pos);
				for (int i = pos + 1; i < size && PCX_AT24(i) == v; i++) {
					rep++;
				}
				PCX_PUT_RUNS(buf, pos2, pos, rep, v, PCX_AT24(pos));
				CHECK(!rep);
				CHECK(pos <= size);
				CHECK(pos2 <= size * 2);
			} while (pos < size);
		}
		CHECK(pos == size);
		if (crt_fwrite(buf, pos2, 1, f) != 1) {
			return -1;
		}
		mem += size;
	}
#undef PCX_AT24
	Mem_Free(buf);
	return 0;
}

