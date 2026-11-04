/*
 * CircuitScorer.cpp of the game (Menu/): cCircuitScorer, a circuit board with flares running along nine paths.
 * See include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/scoreritems.h"
#include "scrap/staticinit.h"

#define CCIRCUITSCORER_VFTABLE (&cCircuitScorer_vftable)
#define g_CircuitInit (*(byte *)&g_CircuitFlares[3].unused_14)
static cCircuitFlare g_CircuitFlares[4];
static const cScorerItemVtbl cCircuitScorer_vftable;

/* rand() / 32767 * scale + ofs in double, as the original */
static float Circuit_Rand(double scale, double ofs) {
	return (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * scale + ofs);
}

/* the constructor of a path in the static array: nothing */
static cCircuitPath *cCircuitPath_ctor(cCircuitPath *self) {
	return self;
}

static void CircuitScorer_InitPaths(void);
static void cCircuitScorer_SetCircuitFile(cCircuitScorer *self, uint32_t flags, const char *name);
static void cCircuitScorer_SetFlareFile(cCircuitScorer *self, uint32_t flags, const char *name);
cCircuitScorer *cCircuitScorer_ctor(cCircuitScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CCIRCUITSCORER_VFTABLE;
	cString_ctor(&self->file);
	self->highRes = 0;
	self->blend = 3;
	self->lastTime = 0.0f;
	self->curTime = 0.0f;
	self->circuit = NULL;
	self->flare = NULL;
	cCircuitScorer_SetCircuitFile(self, 0x20, "2D/white.tga");
	cCircuitScorer_SetFlareFile(self, 0x20, "2D/white.tga");
	self->base.x = 320.0f;
	self->base.y = 240.0f;
	if (!g_CircuitInit) {
		g_CircuitInit = 1;
		CircuitScorer_InitPaths();
	}
	return self;
}

static void Circuit_FreeImage(CPsaImage **img) {
	if (*img) {
		CPsaImage_dtor(*img);
		operator_delete(*img);
		*img = NULL;
	}
}

static void cCircuitScorer_dtor(cCircuitScorer *self) {
	Circuit_FreeImage(&self->circuit);
	Circuit_FreeImage(&self->flare);
	cString_dtor(&self->file);
	cScorerItem_dtor(&self->base);
}

static void cCircuitScorer_Free(cScorerItem *base) {
	cCircuitScorer *self = (cCircuitScorer *)base;
	cCircuitScorer_dtor(self);
	operator_delete(self);
}

/* the paths of the board in pixels of the picture: the count and the points */
static const struct {
	int numPoints;
	float pts[18][2];
} s_CircuitPoints[9] = {
	{ 16, {
		{ 30.0f, 0.0f }, { 30.0f, 196.0f }, { 44.0f, 210.0f }, { 44.0f, 334.0f }, { 24.0f, 354.0f },
		{ 24.0f, 446.0f }, { 32.0f, 452.0f }, { 32.0f, 552.0f }, { 24.0f, 558.0f }, { 24.0f, 642.0f },
		{ 72.0f, 690.0f }, { 72.0f, 816.0f }, { 66.0f, 824.0f }, { 66.0f, 944.0f }, { 74.0f, 946.0f },
		{ 74.0f, 1024.0f }
	} },
	{ 10, {
		{ 120.0f, 0.0f }, { 120.0f, 158.0f }, { 140.0f, 178.0f }, { 140.0f, 540.0f }, { 96.0f, 582.0f },
		{ 96.0f, 834.0f }, { 110.0f, 850.0f }, { 110.0f, 924.0f }, { 100.0f, 936.0f }, { 100.0f, 1024.0f }
	} },
	{ 9, {
		{ 100.0f, 1024.0f }, { 100.0f, 936.0f }, { 110.0f, 924.0f }, { 110.0f, 850.0f }, { 96.0f, 834.0f },
		{ 96.0f, 582.0f }, { 120.0f, 560.0f }, { 73.0f, 515.0f }, { 73.0f, 433.0f }
	} },
	{ 7, {
		{ 120.0f, 0.0f }, { 120.0f, 158.0f }, { 140.0f, 178.0f }, { 140.0f, 540.0f }, { 120.0f, 560.0f },
		{ 73.0f, 515.0f }, { 73.0f, 433.0f }
	} },
	{ 6, {
		{ 120.0f, 0.0f }, { 120.0f, 158.0f }, { 140.0f, 180.0f }, { 140.0f, 249.0f }, { 125.0f, 263.0f },
		{ 125.0f, 428.0f }
	} },
	{ 4, {
		{ 140.0f, 0.0f }, { 140.0f, 249.0f }, { 125.0f, 263.0f }, { 125.0f, 428.0f }
	} },
	{ 14, {
		{ 150.0f, 0.0f }, { 150.0f, 312.0f }, { 182.0f, 344.0f }, { 182.0f, 380.0f }, { 166.0f, 396.0f },
		{ 166.0f, 626.0f }, { 146.0f, 644.0f }, { 146.0f, 750.0f }, { 160.0f, 764.0f }, { 160.0f, 822.0f },
		{ 134.0f, 848.0f }, { 136.0f, 910.0f }, { 164.0f, 940.0f }, { 164.0f, 1024.0f }
	} },
	{ 6, {
		{ 150.0f, 0.0f }, { 150.0f, 312.0f }, { 182.0f, 344.0f }, { 182.0f, 445.0f }, { 206.0f, 445.0f },
		{ 206.0f, 385.0f }
	} },
	{ 18, {
		{ 150.0f, 0.0f }, { 150.0f, 312.0f }, { 182.0f, 344.0f }, { 182.0f, 445.0f }, { 194.0f, 445.0f },
		{ 194.0f, 536.0f }, { 183.0f, 548.0f }, { 184.0f, 668.0f }, { 189.0f, 674.0f }, { 189.0f, 727.0f },
		{ 181.0f, 737.0f }, { 181.0f, 791.0f }, { 198.0f, 806.0f }, { 198.0f, 877.0f }, { 150.0f, 926.0f },
		{ 165.0f, 942.0f }, { 165.0f, 942.0f }, { 165.0f, 1024.0f }
	} },
};

static cCircuitPath g_CircuitPaths[9];

/* the points of the paths, their lengths, the flares stopped with their first delays (once) */
static void CircuitScorer_InitPaths(void) {
	for (int p = 0; p < 9; p++) {
		g_CircuitPaths[p].numPoints = s_CircuitPoints[p].numPoints;
		memcpy(g_CircuitPaths[p].pts, s_CircuitPoints[p].pts, sizeof(g_CircuitPaths[p].pts[0]) * (size_t)s_CircuitPoints[p].numPoints);
	}
}

static CPsaImage *Circuit_NewImage(CPsaImage *old, const char *name, int blend, uint32_t flags) {
	if (old) {
		CPsaImage_dtor(old);
		operator_delete(old);
	}
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctorFlags(img, name, 0, blend, flags) : NULL;
	img->color = 0xffffffff;
	return img;
}

static void cCircuitScorer_SetCircuitFile(cCircuitScorer *self, uint32_t flags, const char *name) {
	self->circuit = Circuit_NewImage(self->circuit, name, self->blend, flags | 0x40000);
}

/* the flare sheet; the white default without the 0x40 flag */
static void cCircuitScorer_SetFlareFile(cCircuitScorer *self, uint32_t flags, const char *name) {
	if (self->flare) {
		CPsaImage_dtor(self->flare);
		operator_delete(self->flare);
	}
	if (!strcmp(name, "2D/white.tga")) {
		flags &= 0xffbf;
	} else {
		flags |= 0x40;
	}
	self->flare = Circuit_NewImage(NULL, name, self->blend, flags | 0x40000);
}

/* ScaleX, ScaleY, FileCircuit and FileFlare (set), HighRes, BlendingMode, AlphaCircuit, AlphaFlare */
static int cCircuitScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cCircuitScorer *self = (cCircuitScorer *)base;
	if (d) {
		if (!crt_stricmp(name, "ScaleX")) {
			return ScorerVar_Float(&self->circuit->scaleX, set, d);
		}
		if (!crt_stricmp(name, "ScaleY")) {
			return ScorerVar_Float(&self->circuit->scaleY, set, d);
		}
	}
	if (s) {
		uint32_t flags;
		if (!crt_stricmp(name, "FileCircuit") && set) {
			flags = self->highRes ? 0x10020 : 0x20;
			cCircuitScorer_SetCircuitFile(self, flags, *s);
			cString_Set(&self->file, *s);
			return 1;
		}
		if (!crt_stricmp(name, "FileFlare") && set) {
			flags = self->highRes ? 0x10020 : 0x20;
			cCircuitScorer_SetFlareFile(self, flags, *s);
			cString_Set(&self->file, *s);
			return 1;
		}
	}
	if (i) {
		if (!crt_stricmp(name, "HighRes")) {
			CHECK(*i == 0 || *i == 1);
			return ScorerVar_Int(&self->highRes, set, i);
		}
		if (!crt_stricmp(name, "BlendingMode")) {
			int v = *i;
			CHECK(v >= 0 && v <= 6);
			if (set) {
				self->blend = v;
			}
			return 3;
		}
		if (!crt_stricmp(name, "AlphaCircuit")) {
			return ScorerVar_ColorByte(&self->circuit->color, 24, set, i);
		}
		if (!crt_stricmp(name, "AlphaFlare")) {
			return ScorerVar_ColorByte(&self->flare->color, 24, set, i);
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

static cCircuitFlare g_CircuitFlares[4];

/* a flare at its end: a path no other flare runs, a pause, the duration by the path length, a direction */
static void Circuit_Respawn(int k) {
	cCircuitFlare *f = &g_CircuitFlares[k];

	for (int n = 0; n < 8; n++) {
		f->path = crt_rand() % CIRCUIT_PATHS;
		int j;
		for (j = 0; j < CIRCUIT_FLARES; j++) {
			if (j != k && g_CircuitFlares[j].path == f->path) {
				break;
			}
		}
		if (j == CIRCUIT_FLARES) {
			break;
		}
	}
	f->delay = Circuit_Rand((double)0.2f, (double)0.2f);
	int r = crt_rand() & 0x7fff;
	float len = g_CircuitPaths[f->path].total / 550.0f;
	float t = len * (float)((double)r * RAND_SCALE * (double)0.50000006f + (double)0.7f);
	f->duration = t;
	f->time = t;
	f->reverse = crt_rand() & 1;
}

static void cCircuitScorer_Draw(cScorerItem *base, int player) {
	cCircuitScorer *self = (cCircuitScorer *)base;
	(void)player;
	CPsaImage_Draw(self->circuit, self->base.x, self->base.y);
	self->lastTime = self->curTime;
	float now = (float)App_GetGameTime(g_pApp);
	self->curTime = now;
	if (self->lastTime == 0.0f) {
		self->lastTime = now;
	}
	float dt = now - self->lastTime;
	for (int k = 0; k < CIRCUIT_FLARES; k++) {
		cCircuitFlare *f = &g_CircuitFlares[k];
		f->time = f->time - dt;
		if (0.0f >= f->time) {
			f->delay = f->delay - dt;
			if (0.0f >= f->delay) {
				Circuit_Respawn(k);
			}
			continue;
		}
		float u = f->reverse ? 1.0f - f->time / f->duration : f->time / f->duration;
		float alpha = 1.0f;
		if (!(u >= 0.0f) || !(1.0f >= u)) {
			continue;
		}
		if (0.05f > u) {
			alpha = u / 0.05f;
		} else if (u > 0.95f) {
			alpha = 1.0f - (u - 0.95f) / 0.05f;
		}
		for (int i = 0; i < CIRCUIT_TRAIL; i++) {
			float v = u;
			if (i > 0) {
				v = f->reverse ? u - (float)i * 0.0042f : (float)i * 0.0042f + u;
			}
			if (!(v >= 0.0f) || !(1.0f >= v)) {
				continue;
			}
			const cCircuitPath *path = &g_CircuitPaths[f->path];
			float px = 0.0f, py = 0.0f;
			for (int s = 0; s < path->numPoints - 1; s++) {
				float a = path->normStart[s];
				if (!(v >= a)) {
					continue;
				}
				float b = path->normEnd[s];
				if (!(b > v)) {
					continue;
				}
				float t = (v - a) / (b - a);
				px = (path->pts[s + 1][0] - path->pts[s][0]) * t + path->pts[s][0];
				py = path->pts[s][1] + (path->pts[s + 1][1] - path->pts[s][1]) * t;
			}
			px = px * 0.5f + (self->base.x - 7.0f);
			py = py * 0.5f + (self->base.y - 8.0f);
			int a8 = (26 - i) * 2;
			if (i == 0) {
				a8 = crt_rand() % 48 + 190;
			}
			self->flare->color = (uint32_t)(int)((float)a8 * alpha) << 24 | (self->flare->color & 0xffffff);
			CPsaImage_DrawFrame(self->flare, (i != 0) + 1, px, py);
		}
	}
}

/* the vtable of cCircuitScorer */
static const cScorerItemVtbl cCircuitScorer_vftable = {
	.Draw = cCircuitScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cCircuitScorer_Var,
	.Free = cCircuitScorer_Free,
};

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_ARRAY(Init_cCircuitPath_CircuitPaths, g_CircuitPaths, sizeof(cCircuitPath), 9, cCircuitPath_ctor)
