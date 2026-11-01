/*
 * CircuitScorer.cpp of the game (Menu/): cCircuitScorer, a circuit board with flares running along nine paths.
 * See include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/scoreritems.h"



/* rand() / 32767 * scale + ofs in double, as the original */
static float Circuit_Rand(double scale, double ofs) {
	return (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * scale + ofs);
}

cCircuitPath *cCircuitPath_ctor(cCircuitPath *self) {
	return self;
}

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
		operator_delete_sized(*img, sizeof(CPsaImage));
		*img = NULL;
	}
}

void cCircuitScorer_dtor(cCircuitScorer *self) {
	self->base.vftable = CCIRCUITSCORER_VFTABLE;
	Circuit_FreeImage(&self->circuit);
	Circuit_FreeImage(&self->flare);
	cString_dtor(&self->file);
	cScorerItem_dtor(&self->base);
}

cCircuitScorer *cCircuitScorer_Delete(cCircuitScorer *self, byte flags) {
	cCircuitScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cCircuitScorer));
	}
	return self;
}

void CircuitScorer_InitPaths(void) {
	/* the stores of the original in its order: 16 bytes of the exe's tables (src) or a dword (value) */
	const struct {
		void *dst;
		const void *src;
		uint32_t value;
	} paths[] = {
		{ GAME_PTR(g_CircuitPaths, 4), GAME_PTR(s_PathData_3c2640, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 20), GAME_PTR(s_PathData_3c2680, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 36), GAME_PTR(s_PathData_3c26d0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 52), GAME_PTR(s_PathData_3c2710, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 0), NULL, 0x10 },
		{ GAME_PTR(g_CircuitPaths, 68), GAME_PTR(s_PathData_3c2740, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 776), NULL, 0xa },
		{ GAME_PTR(g_CircuitPaths, 84), GAME_PTR(s_PathData_3c2790, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 1552), NULL, 0x9 },
		{ GAME_PTR(g_CircuitPaths, 100), GAME_PTR(s_PathData_3c2820, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 1620), NULL, 0x42920000 },
		{ GAME_PTR(g_CircuitPaths, 116), GAME_PTR(s_PathData_3c2830, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 1624), NULL, 0x43d88000 },
		{ GAME_PTR(g_CircuitPaths, 780), GAME_PTR(s_PathData_3c2630, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 2328), NULL, 0x7 },
		{ GAME_PTR(g_CircuitPaths, 796), GAME_PTR(s_PathData_3c2700, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 2380), NULL, 0x42920000 },
		{ GAME_PTR(g_CircuitPaths, 812), GAME_PTR(s_PathData_3c27b0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 2384), NULL, 0x43d88000 },
		{ GAME_PTR(g_CircuitPaths, 828), GAME_PTR(s_PathData_3c27f0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 3104), NULL, 0x6 },
		{ GAME_PTR(g_CircuitPaths, 844), GAME_PTR(s_PathData_3c2840, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 3880), NULL, 0x4 },
		{ GAME_PTR(g_CircuitPaths, 1556), GAME_PTR(s_PathData_3c2800, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 4656), NULL, 0xe },
		{ GAME_PTR(g_CircuitPaths, 1572), GAME_PTR(s_PathData_3c27c0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 5432), NULL, 0x6 },
		{ GAME_PTR(g_CircuitPaths, 1588), GAME_PTR(s_PathData_3c2720, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 1604), GAME_PTR(s_PathData_3c26e0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 2332), GAME_PTR(s_PathData_3c2630, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 2348), GAME_PTR(s_PathData_3c2700, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 2364), GAME_PTR(s_PathData_3c26e0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 3108), GAME_PTR(s_PathData_3c2630, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 3124), GAME_PTR(s_PathData_3c2660, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 3140), GAME_PTR(s_PathData_3c26b0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 3884), GAME_PTR(s_PathData_3c2650, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 3900), GAME_PTR(s_PathData_3c26b0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 4660), GAME_PTR(s_PathData_3c2670, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 4676), GAME_PTR(s_PathData_3c2690, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 4692), GAME_PTR(s_PathData_3c2730, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 4708), GAME_PTR(s_PathData_3c2770, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 4724), GAME_PTR(s_PathData_3c27a0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 4740), GAME_PTR(s_PathData_3c27e0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 4756), GAME_PTR(s_PathData_3c2850, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 5436), GAME_PTR(s_PathData_3c2670, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 5452), GAME_PTR(s_PathData_3c26c0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 6208), NULL, 0x12 },
		{ GAME_PTR(g_CircuitPaths, 5468), GAME_PTR(s_PathData_3c26a0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 6212), GAME_PTR(s_PathData_3c2670, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 6228), GAME_PTR(s_PathData_3c26c0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 6244), GAME_PTR(s_PathData_3c26f0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 6260), GAME_PTR(s_PathData_3c2750, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 6276), GAME_PTR(s_PathData_3c2760, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 6292), GAME_PTR(s_PathData_3c2780, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 6308), GAME_PTR(s_PathData_3c27d0, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 6324), GAME_PTR(s_PathData_3c2810, 0), 0 },
		{ GAME_PTR(g_CircuitPaths, 6340), GAME_PTR(s_PathData_3c2860, 0), 0 },
	};

	for (size_t k = 0; k < sizeof(paths) / sizeof(paths[0]); k++) {
		if (paths[k].src) {
			memcpy(paths[k].dst, paths[k].src, 16);
		} else {
			*(uint32_t *)paths[k].dst = paths[k].value;
		}
	}
	for (int p = 0; p < CIRCUIT_PATHS; p++) {
		cCircuitPath *path = &g_CircuitPaths[p];
		path->total = 0.0f;
		for (int s = 0; s < path->numPoints - 1; s++) {
			float dx = path->pts[s][0] - path->pts[s + 1][0];
			float dy = path->pts[s][1] - path->pts[s + 1][1];
			path->segStart[s] = path->total;
			float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx));
			path->segLen[s] = len;
			path->total = len + path->total;
		}
		for (int s = 0; s < path->numPoints; s++) {
			path->normStart[s] = path->segStart[s] / path->total;
			path->normEnd[s] = (path->segLen[s] + path->segStart[s]) / path->total;
		}
	}
	for (int k = 0; k < CIRCUIT_FLARES; k++) {
		g_CircuitFlares[k].time = 0.0f;
		g_CircuitFlares[k].delay = (float)k * 0.2f;
	}
}

static CPsaImage *Circuit_NewImage(CPsaImage *old, const char *name, int blend, uint32_t flags) {
	if (old) {
		CPsaImage_dtor(old);
		operator_delete_sized(old, sizeof(CPsaImage));
	}
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctorFlags(img, name, 0, blend, flags) : NULL;
	img->color = 0xffffffff;
	return img;
}

void cCircuitScorer_SetCircuitFile(cCircuitScorer *self, uint32_t flags, const char *name) {
	self->circuit = Circuit_NewImage(self->circuit, name, self->blend, flags | 0x40000);
}

void cCircuitScorer_SetFlareFile(cCircuitScorer *self, uint32_t flags, const char *name) {
	if (self->flare) {
		CPsaImage_dtor(self->flare);
		operator_delete_sized(self->flare, sizeof(CPsaImage));
	}
	if (!strcmp(name, "2D/white.tga")) {
		flags &= 0xffbf;
	} else {
		flags |= 0x40;
	}
	self->flare = Circuit_NewImage(NULL, name, self->blend, flags | 0x40000);
}

int cCircuitScorer_Var(cCircuitScorer *self, const char *name, byte set, char **s, double *d, int *i) {
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
	float t = len * (float)((double)r * RAND_SCALE * GAME_GLOBAL(double, cCircuitScorer_Draw_3c2608) + (double)0.7f);
	f->duration = t;
	f->time = t;
	f->reverse = crt_rand() & 1;
}

void cCircuitScorer_Draw(cCircuitScorer *self, int player) {
	(void)player;
	CPsaImage_Draw(self->circuit, self->base.x, self->base.y);
	self->lastTime = self->curTime;
	float now = (float)g_pApp->vftable->GetGameTime(g_pApp);
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
