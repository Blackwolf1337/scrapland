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
	IFXTexture *(*Delete)(IFXTexture *self, byte flags);	/* 0x0 */
	int(*Update)(IFXTexture *self, CMap *map);		/* 0x4 */
	void *(*GetTexture)(IFXTexture *self);			/* 0x8 */
} IFXTextureVtbl;

/* calls through the slots of IFXTextureVtbl */
static inline IFXTexture *FXTexture_Delete(void *self, byte flags) {
	return VT(IFXTextureVtbl, self)->Delete(self, flags);
}

struct IFXTexture {
	const IFXTextureVtbl *vftable;	/* 0x0 */
};

typedef struct CFXTexLava {
	IFXTexture base;
	CBmp *bmp;			/* 0x04 the file, changed in place and copied into the map */
	CBmp *orig;			/* 0x08 a copy of it */
	uint32_t lastTime;		/* 0x0c */
} CFXTexLava;

typedef struct CFXTexScroll {
	IFXTexture base;
	CBmp *bmp;			/* 0x04 */
	CBmp *orig;			/* 0x08 */
	uint32_t lastTime;		/* 0x0c */
	int offset;			/* 0x10 the pixels scrolled */
	int unused_14;			/* 0x14 */
} CFXTexScroll;

typedef struct CFXNewsPanel {
	IFXTexture base;
	int time;			/* 0x4 of the last update */
} CFXNewsPanel;

/* the effect of kind for the file of map, NULL for an unknown kind */
void *FXTexture_Create(byte kind, CMap *map);
/* the kind of the first "(...)" tag of the name that is one, 0 for none */
byte FXTexture_Kind(const char *name);

#endif
