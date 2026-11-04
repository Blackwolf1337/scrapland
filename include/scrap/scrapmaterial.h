/*
 * ScrapEngMaterial.cpp of the game: the blend modes of the script materials. BlendModeTable holds for each mode the
 * mode itself and the D3D source and destination blend factors.
 */
#ifndef SCRAP_SCRAPMATERIAL_H
#define SCRAP_SCRAPMATERIAL_H

#include "scrap/game.h"

#define BLENDMODE_NUM_MODES 7

typedef struct BlendModeEntry {
	uint32_t mode;			/* the index */
	uint32_t src;			/* D3DBLEND */
	uint32_t dst;
} BlendModeEntry;

uint32_t BlendMode_GetDst(uint32_t mode);
/* ScrapEngMaterial.cpp: the source and destination blend of a blend mode (CHECK mode < 7) */
uint32_t BlendMode_GetSrc(uint32_t mode);

#endif
