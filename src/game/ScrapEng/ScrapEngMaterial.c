/*
 * ScrapEngMaterial.cpp of the game: the blend mode table, see include/scrap/scrapmaterial.h.
 */
#include "scrap/scrapmaterial.h"

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
