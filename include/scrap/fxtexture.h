/*
 * Engine/fxtextura.cpp of the engine3d: the effects of animated maps, chosen by the map name: "(fxlava)" waves of a
 * 128x128 bitmap over itself, "(fxscroll)" scrolls it, "(+fx:NewsPanel)" shows the render target of the news panel.
 * The lava and scroll effects update the texture of the map every 50 ms.
 */
#ifndef SCRAP_FXTEXTURE_H
#define SCRAP_FXTEXTURE_H

#include "scrap/bmp.h"
#include "scrap/material.h"

#define FXTEX_SIZE 128
#define FXTEX_PERIOD 50

/* the kinds of FXTexture_Create (CMap fx) */
enum { FXTEX_LAVA = 1, FXTEX_SCROLL = 2, FXTEX_NEWSPANEL = 3 };

typedef struct IFXTexture IFXTexture;

typedef struct IFXTextureVtbl {
	void (*Free)(IFXTexture *self);
	int(*Update)(IFXTexture *self, CMap *map);
	void *(*GetTexture)(IFXTexture *self);
} IFXTextureVtbl;

/* calls through the slots of IFXTextureVtbl */
static inline void *FXTexture_GetTexture(void *self) {
	return VT(IFXTextureVtbl, self)->GetTexture(self);
}

static inline int FXTexture_Update(void *self, CMap *map) {
	return VT(IFXTextureVtbl, self)->Update(self, map);
}

static inline void FXTexture_Free(void *self) {
	VT(IFXTextureVtbl, self)->Free(self);
}

struct IFXTexture {
	const IFXTextureVtbl *vftable;
};

typedef struct CFXTexLava {
	IFXTexture base;
	CBmp *bmp;			/* the file, changed in place and copied into the map */
	CBmp *orig;			/* a copy of it */
	uint32_t lastTime;
} CFXTexLava;

typedef struct CFXTexScroll {
	IFXTexture base;
	CBmp *bmp;
	CBmp *orig;
	uint32_t lastTime;
	int offset;			/* the pixels scrolled */
	int unused_14;
} CFXTexScroll;

typedef struct CFXNewsPanel {
	IFXTexture base;
	int time;			/* of the last update */
} CFXNewsPanel;

/* the effect of kind for the file of map, NULL for an unknown kind */
void *FXTexture_Create(byte kind, CMap *map);
/* the kind of the first "(...)" tag of the name that is one, 0 for none */
byte FXTexture_Kind(const char *name);

#endif
