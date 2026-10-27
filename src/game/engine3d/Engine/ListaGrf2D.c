/*
 * A list of 2D sprites of the engine3d, original source Engine/ListaGrf2D.cpp (include/scrap/fondo2d.h).
 */
#include "scrap/fondo2d.h"

CListaGrf2D *__fastcall CListaGrf2D_ctor(CListaGrf2D *self) {
	self->pri = NULL;
	self->ult = NULL;
	return self;
}

static void Grafico2D_Delete(CGrafico2D *grf) {
	if (grf) {
		CGrafico2D_dtor(grf);
		operator_delete_sized(grf, sizeof(CGrafico2D));
	}
}

void __fastcall CListaGrf2D_DeleteAll(CListaGrf2D *self) {
	while (self->pri) {
		CGrafico2D *grf = self->pri->grf;
		CListaGrf2D_Remove(self, grf);
		Grafico2D_Delete(grf);
	}
}

void __fastcall CListaGrf2D_Render(CListaGrf2D *self) {
	for (NodoGrf2D *n = self->pri; n; n = n->sig) {
		CGrafico2D_Render(n->grf);
	}
}

void __thiscall CListaGrf2D_Add(CListaGrf2D *self, CGrafico2D *grf) {
	NodoGrf2D *n = operator_new(sizeof(*n));

	if (n) {
		n->grf = grf;
		n->sig = NULL;
	}
	CHECK(grf);
	if (self->ult) {
		CHECK(!self->ult->sig);
		self->ult->sig = n;
		self->ult = n;
		return;
	}
	CHECK(!self->pri);
	self->pri = n;
	self->ult = n;
}

void __thiscall CListaGrf2D_Delete(CListaGrf2D *self, CGrafico2D *grf) {
	CListaGrf2D_Remove(self, grf);
	Grafico2D_Delete(grf);
}

void __thiscall CListaGrf2D_Remove(CListaGrf2D *self, CGrafico2D *grf) {
	NodoGrf2D *nodo = self->pri;
	NodoGrf2D *ant = NULL;

	CHECK(grf);
	while (nodo && nodo->grf != grf) {
		ant = nodo;
		nodo = nodo->sig;
	}
	CHECK(nodo);
	if (self->pri == nodo) {
		self->pri = nodo->sig;
	}
	if (self->ult == nodo) {
		self->ult = ant;
	}
	if (ant) {
		ant->sig = nodo->sig;
	}
	nodo->sig = NULL;
	nodo->grf = NULL;
	operator_delete_sized(nodo, sizeof(*nodo));
}
