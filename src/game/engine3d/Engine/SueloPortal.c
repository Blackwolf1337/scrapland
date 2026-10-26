/*
 * Engine/SueloPortal.cpp of the engine3d: the sectors and their lookups, see include/scrap/sueloportal.h.
 */
#include <string.h>

#include "scrap/e3dutil.h"
#include "scrap/sistpart.h"
#include "scrap/sueloportal.h"

#define g_Mundo GAME_DATA(byte *, 0x4afde8)	/* gMundo of mundo.h */
#define MUNDO_SUELO 0xb0			/* gMundo->obtSuelo() */

CZona *__fastcall CZona_ctor(CZona *self) {
	self->rect = (box_t){ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	self->id = -1;
	self->bloq = 0;
	self->Frame3D = 0;
	self->nodo = NULL;
	self->numNodos = 0;
	self->numPortales = 0;
	self->exterior = 0;
	self->b139 = 0;
	self->b13a = 0;
	self->nodos[0] = NULL;
	self->nodos[1] = NULL;
	memset(self->portales, 0, sizeof(self->portales));
	memset(self->zonas, 0, sizeof(self->zonas));
	return self;
}

void __fastcall CZona_dtor(CZona *self) {
	if (self->nodo) {
		ObjRef_Release(&self->nodo);
	}
	self->nodo = NULL;
	for (int i = 0; i < self->numNodos; i++) {
		if (self->nodos[i]) {
			ObjRef_Release(&self->nodos[i]);
		}
		self->nodos[i] = NULL;
	}
	for (int i = 0; i < self->numPortales; i++) {
		if (self->portales[i]) {
			ObjRef_Release(&self->portales[i]);
		}
		self->portales[i] = NULL;
	}
}

void __thiscall SectorVis_Draw(SectorVis *self, const vec3_t *pos, byte visible, byte cached, const char *name) {
	uint32_t off, on;

	(void)name;
	if (!(self->flags & SECTORVIS_VALID)) {
		Debug_Text3D(g_DebugFont2, 0.0f, 0.0f, pos, 0x808080, Game_Ptr(0x3dd0ec));	/* "[-]" */
		return;
	}
	uint32_t color;
	if (self->sector < 0) {
		off = 0x800080;
		on = 0xff0000;
	} else {
		if (visible) {
			color = 0xff;
			goto draw;
		}
		off = 0x808080;
		on = 0x8000;
		visible = cached;
	}
	color = visible ? on : off;
draw:
	Debug_Text3D(g_DebugFont2, 0.0f, 0.0f, pos, color, Game_Ptr(0x3dd0f0), self->sector,	/* "%d%s" */
		(self->flags & SECTORVIS_CACHED) ? Game_Ptr(0x3a3228) : Game_Ptr(0x3ca94c));	/* "", "*" */
}

/* the cached sector is seen (none counts as seen) */
static byte SectorVis_Seen(SectorVis *self) {
	self->flags |= SECTORVIS_CACHED;
	if (self->sector < 0) {
		return 1;
	}
	return Sector_Get((uint32_t)self->sector)->Frame3D == g_FrameCount;
}

static byte SectorVis_SamePos(const SectorVis *self, const vec3_t *pos) {
	return pos->x == self->pos.x && pos->y == self->pos.y && pos->z == self->pos.z && g_SectorCache;
}

byte __thiscall SectorVis_Check(SectorVis *self, const vec3_t *pos, const char *name) {
	if (!(self->flags & SECTORVIS_VALID)) {
		return SectorVis_Update(self, pos, name);
	}
	if (SectorVis_SamePos(self, pos)) {
		return SectorVis_Seen(self);
	}
	self->flags &= ~SECTORVIS_CACHED;
	return 1;
}

byte __thiscall SectorVis_Update(SectorVis *self, const vec3_t *pos, const char *name) {
	if ((self->flags & SECTORVIS_VALID) && SectorVis_SamePos(self, pos)) {
		return SectorVis_Seen(self);
	}
	self->sector = -2;
	self->flags = (self->flags & ~SECTORVIS_CACHED) | SECTORVIS_VALID;
	self->pos = *pos;
	return Sector_FindPos(&self->pos, &self->sector, name);
}

byte Sector_FindPos(const vec3_t *pos, int *sector, const char *name) {
	SueloRayo res;

	(void)name;
	if (!g_PortalesOn) {
		return 1;
	}
	CHECK(g_Mundo);
	void *suelo = *(void **)(g_Mundo + MUNDO_SUELO);
	CHECK(suelo);
	res.flags = 0x202;
	res.y = pos->y;
	if (!CSuelo_RayoVert(suelo, pos->x, pos->z, &res)) {
		return 1;
	}
	int s = res.sector;
	byte seen = 1;
	if (s != -1) {
		vec3_t a = { pos->x, res.hitY, pos->z };
		s = Sector_Recorrer(s, &a, pos, 0);
		seen = Sector_Get((uint32_t)s)->Frame3D == g_FrameCount;
	}
	if (sector) {
		*sector = s;
	}
	return seen;
}

const char *Sector_GetName(int sector) {
	if (sector == -1) {
		return Game_Ptr(0x3dd0a4);	/* "_unasigned_" */
	}
	return Sector_Get((uint32_t)sector)->nodo->name;
}

byte Sector_IsExterior(int sector) {
	return Sector_Get((uint32_t)sector)->exterior;
}

byte Sector_IsOutside(int sector) {
	if (sector == -1) {
		return 0;
	}
	return Sector_Get((uint32_t)sector)->b13a;
}

byte Sector_Get139(int sector) {
	return Sector_Get((uint32_t)sector)->b139;
}

byte Sector_IsVisible(int sector) {
	return Sector_Get((uint32_t)sector)->Frame3D == g_FrameCount;
}

CZona *Zona_FindNodo(CNodo3D *nodo, byte main) {
	for (int i = 0; i < g_NumZonas; i++) {
		CZona *z = g_Zonas[i];
		if (!main) {
			for (int k = 0; k < z->numNodos; k++) {
				if (z->nodos[k] == nodo) {
					return z;
				}
			}
		} else if (nodo == z->nodo) {
			return z;
		}
	}
	return NULL;
}

int Sector_OfNode(CNodo3D *nodo) {
	for (; nodo; nodo = nodo->padre) {
		int s = Sector_Find(nodo->name);
		if (s != -1) {
			return s;
		}
	}
	return -1;
}

int Sector_Find(const char *name) {
	for (int i = 0; i < g_NumZonas; i++) {
		if (!strcmp(name, g_Zonas[i]->nodo->name)) {
			return g_Zonas[i]->id;
		}
	}
	return -1;
}

CZona *Sector_Get(uint32_t sector) {
	CHECK(sector < (uint32_t)g_NumZonas);
	CHECK(g_Zonas[sector]);
	return g_Zonas[sector];
}

int Sector_Num(void) {
	return g_NumZonas;
}

int Sector_OfZonaNodo(CNodo3D *nodo) {
	CZona *z = Zona_FindNodo(nodo, 0);
	return z ? z->id : -1;
}

void Sectores_Free(void) {
	for (int i = 0; i < g_NumZonas; i++) {
		CHECK(g_Zonas[i]);
		CZona *z = g_Zonas[i];
		if (z) {
			CZona_dtor(z);
			operator_delete_sized(z, sizeof(CZona));
			g_Zonas[i] = NULL;
		}
	}
	g_NumZonas = 0;
}
