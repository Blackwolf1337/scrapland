/*
 * Engine/ListaGrf2D.cpp and Engine/fondo2d.cpp of the engine3d: a list of owned 2D sprites, and the backgrounds:
 * CBackground2D, a 512x512 image drawn over the whole viewport in four 256x256 pieces through the map of one material,
 * and the background sprite of the screens (Background2D_Set, drawn once per frame by Background2D_Render).
 */
#ifndef SCRAP_BACKGROUND2D_H
#define SCRAP_BACKGROUND2D_H

#include "scrap/bmp.h"
#include "scrap/graphic2d.h"
#include "scrap/node3d.h"

typedef struct NodeGraphic2D {
	struct NodeGraphic2D *next;
	CGraphic2D *graphic;
} NodeGraphic2D;

typedef struct CGraphic2DList {
	NodeGraphic2D *first;
	NodeGraphic2D *last;
} CGraphic2DList;

CGraphic2DList *CGraphic2DList_ctor(CGraphic2DList *self);
/* the sprites deleted */
void CGraphic2DList_DeleteAll(CGraphic2DList *self);
void CGraphic2DList_Render(CGraphic2DList *self);

#define BACKGROUND2D_PIECE 0x100

typedef struct CBackground2D {
	CBmp bmp;
	ColorIntens color;		/* 0xffffff, 1 */
	CMaterial *Mat;			/* shared, its map 0 gets the pieces */
} CBackground2D;

/* the background sprite: the name set, a new sprite when it changes */
#define BACKGROUND2D_NAME 0x108

void Background2D_Reset(void);
/* the sprite of the new name (once a frame, CHECK) */
void Background2D_Render(void);
void Background2D_Release(void);

#endif
