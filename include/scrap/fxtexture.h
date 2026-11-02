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
extern const IFXTextureVtbl CFXNewsPanel_vftable;
#define CFXNEWSPANEL_VFTABLE (&CFXNewsPanel_vftable)
extern const IFXTextureVtbl CFXTexScroll_vftable;
#define CFXTEXSCROLL_VFTABLE (&CFXTexScroll_vftable)
extern const IFXTextureVtbl CFXTexLava_vftable;
#define CFXTEXLAVA_VFTABLE (&CFXTexLava_vftable)
extern const IFXTextureVtbl IFXTexture_vftable;
#define IFXTEXTURE_VFTABLE (&IFXTexture_vftable)

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

/* the cos of the 256 angles of the lava, made by the first lava */
extern float g_LavaCos[256];
extern byte g_LavaCosInit;
extern int g_LavaOffset[128];	/* FXTEX_SIZE, the row offset of each column */

void IFXTexture_dtor(IFXTexture *self);
IFXTexture *IFXTexture_Delete(IFXTexture *self, byte flags);
void *IFXTexture_GetTexture(IFXTexture *self);
CFXTexLava *CFXTexLava_ctor(CFXTexLava *self, const char *file);
void CFXTexLava_dtor(CFXTexLava *self);
IFXTexture *CFXTexLava_Delete(IFXTexture *base, byte flags);
int CFXTexLava_Update(IFXTexture *base, CMap *map);
CFXTexScroll *CFXTexScroll_ctor(CFXTexScroll *self, const char *file);
void CFXTexScroll_dtor(CFXTexScroll *self);
IFXTexture *CFXTexScroll_Delete(IFXTexture *base, byte flags);
int CFXTexScroll_Update(IFXTexture *base, CMap *map);
CFXNewsPanel *CFXNewsPanel_ctor(CFXNewsPanel *self);
IFXTexture *CFXNewsPanel_Delete(IFXTexture *elem, byte flags);
int CFXNewsPanel_Update(IFXTexture *base, CMap *map);
void *CFXNewsPanel_GetTexture(IFXTexture *base);
/* the effect of kind for the file of map, NULL for an unknown kind */
void *FXTexture_Create(byte kind, CMap *map);
/* the kind of the first "(...)" tag of the name that is one, 0 for none */
byte FXTexture_Kind(const char *name);

#endif
