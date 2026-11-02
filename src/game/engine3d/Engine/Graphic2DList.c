/*
 * A list of 2D sprites of the engine3d, original source Engine/ListaGrf2D.cpp (include/scrap/background2d.h).
 */
#include "scrap/background2d.h"

CGraphic2DList *CGraphic2DList_ctor(CGraphic2DList *self) {
	self->first = NULL;
	self->last = NULL;
	return self;
}

static void Graphic2D_Delete(CGraphic2D *graphic) {
	if (graphic) {
		CGraphic2D_dtor(graphic);
		operator_delete_sized(graphic, sizeof(CGraphic2D));
	}
}

static void CGraphic2DList_Remove(CGraphic2DList *self, CGraphic2D *graphic);
void CGraphic2DList_DeleteAll(CGraphic2DList *self) {
	while (self->first) {
		CGraphic2D *graphic = self->first->graphic;
		CGraphic2DList_Remove(self, graphic);
		Graphic2D_Delete(graphic);
	}
}

void CGraphic2DList_Render(CGraphic2DList *self) {
	for (NodeGraphic2D *n = self->first; n; n = n->next) {
		CGraphic2D_Render(n->graphic);
	}
}

static void CGraphic2DList_Remove(CGraphic2DList *self, CGraphic2D *graphic) {
	NodeGraphic2D *node = self->first;
	NodeGraphic2D *prev = NULL;

	CHECK(graphic);
	while (node && node->graphic != graphic) {
		prev = node;
		node = node->next;
	}
	CHECK(node);
	if (self->first == node) {
		self->first = node->next;
	}
	if (self->last == node) {
		self->last = prev;
	}
	if (prev) {
		prev->next = node->next;
	}
	node->next = NULL;
	node->graphic = NULL;
	operator_delete_sized(node, sizeof(*node));
}
