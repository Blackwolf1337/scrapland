/*
 * ScrapEngMaterial.cpp of the game: the blend mode table, see include/scrap/scrapmaterial.h.
 */
#include "scrap/scrapmaterial.h"

static BlendModeEntry g_BlendModeTable[7] = {
	{ .src = 2, .dst = 1 },
	{ .mode = 1, .src = 2, .dst = 2 },
	{ .mode = 2, .src = 5, .dst = 2 },
	{ .mode = 3, .src = 5, .dst = 6 },
	{ .mode = 4, .src = 6, .dst = 5 },
	{ .mode = 5, .src = 1, .dst = 3 },
	{ .mode = 6, .src = 2, .dst = 6 },
};

static const BlendModeEntry *BlendMode_Get(uint32_t mode) {
	CHECK(mode < BLENDMODE_NUM_MODES);
	CHECK(g_BlendModeTable[mode].mode == mode);
	return &g_BlendModeTable[mode];
}

uint32_t BlendMode_GetDst(uint32_t mode) {
	return BlendMode_Get(mode)->dst;
}

uint32_t BlendMode_GetSrc(uint32_t mode) {
	return BlendMode_Get(mode)->src;
}
