/*
 * CircuitScorer.cpp of the game (Menu/): cCircuitScorer, a circuit board with flares running along nine paths.
 * See include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/scoreritems.h"

#define g_CircuitRandScale GAME_DATA(double, 0x3a3d60)	/* 1/32767 */

/* the stores of InitPaths: 16 bytes of the exe's tables (copy 1) or a dword, in the original order */
static const struct {
	uint32_t dst, src;
	byte copy;
} s_PathData[] = {
	{0x45df3c, 0x3c2640, 1}, {0x45df4c, 0x3c2680, 1}, {0x45df5c, 0x3c26d0, 1}, {0x45df6c, 0x3c2710, 1},
	{0x45df38, 0x10, 0}, {0x45df7c, 0x3c2740, 1}, {0x45e240, 0xa, 0}, {0x45df8c, 0x3c2790, 1},
	{0x45e548, 0x9, 0}, {0x45df9c, 0x3c2820, 1}, {0x45e58c, 0x42920000, 0}, {0x45dfac, 0x3c2830, 1},
	{0x45e590, 0x43d88000, 0}, {0x45e244, 0x3c2630, 1}, {0x45e850, 0x7, 0}, {0x45e254, 0x3c2700, 1},
	{0x45e884, 0x42920000, 0}, {0x45e264, 0x3c27b0, 1}, {0x45e888, 0x43d88000, 0}, {0x45e274, 0x3c27f0, 1},
	{0x45eb58, 0x6, 0}, {0x45e284, 0x3c2840, 1}, {0x45ee60, 0x4, 0}, {0x45e54c, 0x3c2800, 1},
	{0x45f168, 0xe, 0}, {0x45e55c, 0x3c27c0, 1}, {0x45f470, 0x6, 0}, {0x45e56c, 0x3c2720, 1},
	{0x45e57c, 0x3c26e0, 1}, {0x45e854, 0x3c2630, 1}, {0x45e864, 0x3c2700, 1}, {0x45e874, 0x3c26e0, 1},
	{0x45eb5c, 0x3c2630, 1}, {0x45eb6c, 0x3c2660, 1}, {0x45eb7c, 0x3c26b0, 1}, {0x45ee64, 0x3c2650, 1},
	{0x45ee74, 0x3c26b0, 1}, {0x45f16c, 0x3c2670, 1}, {0x45f17c, 0x3c2690, 1}, {0x45f18c, 0x3c2730, 1},
	{0x45f19c, 0x3c2770, 1}, {0x45f1ac, 0x3c27a0, 1}, {0x45f1bc, 0x3c27e0, 1}, {0x45f1cc, 0x3c2850, 1},
	{0x45f474, 0x3c2670, 1}, {0x45f484, 0x3c26c0, 1}, {0x45f778, 0x12, 0}, {0x45f494, 0x3c26a0, 1},
	{0x45f77c, 0x3c2670, 1}, {0x45f78c, 0x3c26c0, 1}, {0x45f79c, 0x3c26f0, 1}, {0x45f7ac, 0x3c2750, 1},
	{0x45f7bc, 0x3c2760, 1}, {0x45f7cc, 0x3c2780, 1}, {0x45f7dc, 0x3c27d0, 1}, {0x45f7ec, 0x3c2810, 1},
	{0x45f7fc, 0x3c2860, 1},
};

/* rand() / 32767 * scale + ofs in double, as the original */
static float Circuit_Rand(double scale, double ofs) {
	return (float)((double)(crt_rand() & 0x7fff) * g_CircuitRandScale * scale + ofs);
}

cCircuitPath *__fastcall cCircuitPath_ctor(cCircuitPath *self) {
	return self;
}

cCircuitScorer *__fastcall cCircuitScorer_ctor(cCircuitScorer *self) {
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

void __fastcall cCircuitScorer_dtor(cCircuitScorer *self) {
	self->base.vftable = CCIRCUITSCORER_VFTABLE;
	Circuit_FreeImage(&self->circuit);
	Circuit_FreeImage(&self->flare);
	cString_dtor(&self->file);
	cScorerItem_dtor(&self->base);
}

cCircuitScorer *__thiscall cCircuitScorer_Delete(cCircuitScorer *self, byte flags) {
	cCircuitScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cCircuitScorer));
	}
	return self;
}

void CircuitScorer_InitPaths(void) {
	for (size_t k = 0; k < sizeof(s_PathData) / sizeof(s_PathData[0]); k++) {
		if (s_PathData[k].copy) {
			memcpy(Game_Ptr(s_PathData[k].dst), Game_Ptr(s_PathData[k].src), 16);
		} else {
			GAME_DATA(uint32_t, s_PathData[k].dst) = s_PathData[k].src;
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

void __thiscall cCircuitScorer_SetCircuitFile(cCircuitScorer *self, uint32_t flags, const char *name) {
	self->circuit = Circuit_NewImage(self->circuit, name, self->blend, flags | 0x40000);
}

void __thiscall cCircuitScorer_SetFlareFile(cCircuitScorer *self, uint32_t flags, const char *name) {
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

int __thiscall cCircuitScorer_Var(cCircuitScorer *self, const char *name, byte set, char **s, double *d, int *i) {
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
	float t = len * (float)((double)r * g_CircuitRandScale * GAME_DATA(double, 0x3c2608) + (double)0.7f);
	f->duration = t;
	f->time = t;
	f->reverse = crt_rand() & 1;
}

void __thiscall cCircuitScorer_Draw(cCircuitScorer *self, int player) {
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
