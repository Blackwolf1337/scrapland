/*
 * PCX images of the engine3d, original source Util/Pcx.cpp (include/scrap/e3dutil.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"

#define PCX_RUN 0xc0
#define PCX_COUNT 0x3f

const byte *Pcx_Decode(byte *dest, const byte *src, const PcxHeader *hdr) {
	int lx = hdr->lx, rows = hdr->ly, np = hdr->planes;
	byte *lim = dest + np * rows * lx;

	CHECK(dest && src);
	CHECK(lim > dest);
	if (!rows) {
		return src;
	}
	do {
		byte rep = 0;
		if (np) {
			for (int p = 0; p < np; p++) {
				byte *d = dest + p;
				int x = lx;
				while (x > 0) {
					if (!rep) {
						rep = *src++;
					}
					if ((rep & PCX_RUN) != PCX_RUN) {
						*d = rep;
						x--;
						d += np;
						rep = 0;
						continue;
					}
					rep &= PCX_COUNT;
					if (x < rep) {
						/* the run goes on in the next plane */
						rep = (byte)(rep - x) | PCX_RUN;
						do {
							x--;
							*d = *src;
							d += np;
						} while (x > 0);
						break;
					}
					x -= rep;
					for (; rep; rep--) {
						*d = *src;
						d += np;
					}
					src++;
				}
				int skip = hdr->bpl - lx;
				CHECK(skip < 2);
				while (skip) {
					if (!rep) {
						rep = *src++;
					}
					CHECK(rep == 193);
					if ((rep & PCX_RUN) == PCX_RUN) {
						rep &= PCX_COUNT;
						if (skip < rep) {
							rep = (byte)(rep - skip) | PCX_RUN;
							break;
						}
						skip -= rep;
						rep = 0;
						src++;
					} else {
						skip--;
						rep = 0;
					}
				}
				CHECK(d <= lim + p);
				CHECK(!x);
			}
			CHECK(!rep);
		}
		dest += np * lx;
	} while (--rows > 0);
	return src;
}

int Pcx_DecodeFile(byte *dest, void *f, const PcxHeader *hdr) {
	int lx = hdr->lx, rows = hdr->ly, np = hdr->planes;
	byte *lim = dest + np * rows * lx;

	CHECK(dest && f);
	CHECK(lim > dest);
	/* one character read ahead */
	int c = crt_fgetc(f);
	if (rows) {
		do {
			byte rep = 0;
			if (np) {
				for (int p = 0; p < np; p++) {
					byte *d = dest + p;
					int x = lx;
					while (x > 0) {
						if (!rep) {
							if (c == -1) {
								return -1;
							}
							rep = (byte)c;
							c = crt_fgetc(f);
						}
						if ((rep & PCX_RUN) != PCX_RUN) {
							*d = rep;
							x--;
							d += np;
							rep = 0;
							continue;
						}
						rep &= PCX_COUNT;
						if (x < rep) {
							if (c == -1) {
								return -1;
							}
							rep = (byte)(rep - x) | PCX_RUN;
							do {
								*d = (byte)c;
								x--;
								d += np;
							} while (x > 0);
							break;
						}
						if (c == -1) {
							return -1;
						}
						x -= rep;
						for (; rep; rep--) {
							*d = (byte)c;
							d += np;
						}
						c = crt_fgetc(f);
					}
					int skip = hdr->bpl - lx;
					CHECK(skip < 2);
					while (skip) {
						if (!rep) {
							if (c == -1) {
								return -1;
							}
							rep = (byte)c;
							c = crt_fgetc(f);
						}
						CHECK(rep == 193);
						if ((rep & PCX_RUN) == PCX_RUN) {
							rep &= PCX_COUNT;
							if (skip < rep) {
								if (c == -1) {
									return -1;
								}
								rep = (byte)(rep - skip) | PCX_RUN;
								break;
							}
							if (c == -1) {
								return -1;
							}
							skip -= rep;
							rep = 0;
							c = crt_fgetc(f);
						} else {
							skip--;
							rep = 0;
						}
					}
					CHECK(d <= lim + p);
					CHECK(!x);
				}
				CHECK(!rep);
			}
			dest += np * lx;
		} while (--rows > 0);
	}
	crt_ungetc(c, f);
	return 0;
}

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

int Pcx_Write8(void *f, const byte *mem, int lx, int ly, const byte *pal, int pal4) {
	static const byte pcxPalMark = 12;
	PcxHeader hdr;

	byte *buf = Mem_Alloc(lx * 2);
	Pcx_InitHeader(&hdr, lx, ly, 1);
	hdr.palInfo = pal != NULL;
	if (crt_fwrite(&hdr, sizeof(hdr), 1, f) != 1) {
		return -1;
	}
	for (int y = 0; y < ly; y++) {
		int pos = 0, pos2 = 0;
		if (lx > 0) {
			do {
				int rep = 1;
				byte v = mem[pos];
				for (int i = pos + 1; i < lx && mem[i] == v; i++) {
					rep++;
				}
				PCX_PUT_RUNS(buf, pos2, pos, rep, v, mem[pos]);
				CHECK(!rep);
				CHECK(pos <= lx);
				CHECK(pos2 <= lx * 2);
			} while (pos < lx);
		}
		CHECK(pos == lx);
		if (crt_fwrite(buf, pos2, 1, f) != 1) {
			return -1;
		}
		mem += lx;
	}
	if (pal) {
		if (crt_fwrite(&pcxPalMark, 1, 1, f) != 1) {
			return -1;
		}
		if (!pal4) {
			for (int i = 0; i < 0x300; i++) {
				if (crt_fputc(pal[i] << 2, f) == -1) {
					return -1;
				}
			}
		} else {
			for (int i = 0; i < 0x100; i++) {
				if (crt_fputc(pal[i * 4], f) == -1 || crt_fputc(pal[i * 4 + 1], f) == -1 ||
					crt_fputc(pal[i * 4 + 2], f) == -1) {
					return -1;
				}
			}
		}
	}
	Mem_Free(buf);
	return 0;
}

int Pcx_ReadHeader(void *f, PcxHeader *hdr) {
	CHECK(f && hdr);
	if (crt_fread(hdr, sizeof(*hdr), 1, f) != 1) {
		return -1;
	}
	return Pcx_CheckHeader(hdr);
}

int Pcx_ReadPal(byte *dest, void *f, int num) {
	CHECK(dest && f);
	if (num <= 0) {
		return 0;
	}
	int c = crt_fgetc(f);
	if (c == -1) {
		return -1;
	}
	if (c != 12) {
		crt_ungetc(c, f);
		return 0;
	}
	for (int i = 0; i < num; i++) {
		for (int k = 0; k < 3; k++) {
			c = crt_fgetc(f);
			if (c == -1) {
				return -1;
			}
			dest[i * 4 + k] = (byte)c;
		}
		dest[i * 4 + 3] = 5;
	}
	return 0;
}

int Pcx_CheckHeader(PcxHeader *hdr) {
	CHECK(hdr);
	if (hdr->manuf != 10 || hdr->version != 5 || hdr->encoding != 1 || hdr->bpp != 8) {
		*crt_errno() = g_ErrPcxFormat;
		return -1;
	}
	if (!hdr->lx) {
		hdr->lx = (uint16_t)(hdr->xmax - hdr->xmin + 1);
	}
	if (!hdr->ly) {
		hdr->ly = (uint16_t)(hdr->ymax - hdr->ymin + 1);
	}
	return 0;
}
