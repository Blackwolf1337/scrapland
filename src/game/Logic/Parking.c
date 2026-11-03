/*
 * Logic/Parking.cpp of the game: the parking zones of the world (cParking of cLogicControl, include/scrap/logic.h).
 * Scrap.AddParkingZone adds a zone to one of two rings, Scrap.GetNearestParked/GetFarestParked find the zone nearest
 * to (farthest from) a point and the ship or item parked in it. Every one to two seconds the next zone of each
 * ring that is far enough from the player (ParkingVehiCheckDistance in a ship, ParkingCharCheckDistance on foot)
 * is checked: an item in a zone of ring 1 calls OnParkedOut, a ship in a zone of ring 0 OnParkingOccupied; ring 1
 * only while the player is in a ship.
 */
#include <float.h>
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/chartype.h"
#include "scrap/cstring.h"
#include "scrap/logic.h"
#include "scrap/world.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

static cGVar GVar_ParkingCheckRadius;
#define g_ParkingCheckRadius (GVar_ParkingCheckRadius.value.f)
static cGVar GVar_ParkingCharCheckDistance;
#define g_ParkingCharCheckDistance (GVar_ParkingCharCheckDistance.value.f)
static cGVar GVar_ParkingVehicleCheckDistance;
#define g_ParkingVehicleCheckDistance (GVar_ParkingVehicleCheckDistance.value.f)
static cGVarFunction GVar_OnParkedOut;
#define g_OnParkedOut (((cPy_Function *)&GVar_OnParkedOut.func))
static cGVarFunction GVar_OnParkingOccupied;
#define g_OnParkingOccupied (((cPy_Function *)&GVar_OnParkingOccupied.func))
/* what the sphere native found: a ship, else the last other entity */
#define PARKING_NATIVE 0x155ed0
#define PARKING_FARTHER 0x155e90
#define PARKING_NEARER 0x155eb0
#define TYEN_ITEM "item"

typedef byte (*ParkingCmp)(float d, float best);

cParking *Parking_ctor(cParking *self) {
	memset(&self->ships, 0, sizeof(self->ships));
	memset(&self->items, 0, sizeof(self->items));
	self->timer = 0.0f;
	return self;
}

/* a node of the free list or a new one with its cString */
static cParkingNode *Parking_NewNode(cParkingList *list) {
	cParkingNode *n = list->freeNodes;

	if (n) {
		list->numFree--;
		list->freeNodes = n->next;
		return n;
	}
	n = operator_new(sizeof(*n));
	if (!n) {
		return NULL;
	}
	cString_ctor(&n->zone.name);
	return n;
}

/* to the free list while it has room, else deleted with its cString */
static void Parking_Release(cParkingList *list, cParkingNode *n) {
	if (list->numFree < list->maxFree) {
		n->next = list->freeNodes;
		list->numFree++;
		list->freeNodes = n;
		return;
	}
	cString_dtor(&n->zone.name);
	operator_delete_sized(n, sizeof(*n));
}

static void Parking_ClearList(cParkingList *list) {
	while (list->head) {
		cParkingNode *p = list->head;
		list->iter = NULL;
		if (p == p->next) {
			Parking_Release(list, p);
			list->head = NULL;
		} else {
			list->head = p->next;
			p->prev->next = p->next;
			p->next->prev = p->prev;
			Parking_Release(list, p);
		}
		list->count--;
	}
}

static void Parking_FreePool(cParkingList *list) {
	cParkingNode *p = list->freeNodes;

	while (p) {
		list->freeNodes = p->next;
		cString_dtor(&p->zone.name);
		operator_delete_sized(p, sizeof(*p));
		p = list->freeNodes;
	}
	list->numFree = 0;
}

void Parking_dtor(cParking *self) {
	Parking_ClearList(&self->items);
	Parking_FreePool(&self->items);
	Parking_ClearList(&self->ships);
	Parking_FreePool(&self->ships);
}

/* a copy of zone in front of head, as the new head */
static void Parking_ListAdd(cParkingList *list, const cParkingZone *zone) {
	list->iter = NULL;
	cParkingNode *n = Parking_NewNode(list);
	n->zone.pos = zone->pos;
	cString_Assign(&n->zone.name, &zone->name);
	DList_LinkFirst(list, n);
}

void Parking_AddZone(void *parking, int flag, const vec3_t *pos, const char *name) {
	cParking *self = parking;
	cParkingZone z;

	cString_ctor(&z.name);
	z.pos = *pos;
	cString_Set(&z.name, name);
	Parking_ListAdd((byte)flag ? &self->items : &self->ships, &z);
	cString_dtor(&z.name);
}

/* the next zone of the ring after the iterator, from head again at the end; NULL when empty */
static cParkingZone *Parking_Next(cParkingList *list) {
	cParkingNode *n = list->iter;

	if (n) {
		n = n->next;
		list->iter = n;
		if (n != list->head) {
			return &n->zone;
		}
	}
	n = list->head;
	list->iter = n;
	return n ? &n->zone : NULL;
}

static cEntity *g_ParkingShip;
static cEntity *g_ParkingOther;

/* the ship (ParkingShip) or other entity (ParkingOther) in the zone, of the ParkingCheckRadius sphere */
static void Parking_Native(cAbstractFile *params);
static void Parking_Look(const cParkingZone *z) {
	cPy_Function_SetNative(g_LogicSphereFunc, (void *)Parking_Native);
	g_ParkingShip = NULL;
	g_ParkingOther = NULL;
	cWorld_SphereCallF(g_pWorld, 5, &z->pos, g_ParkingCheckRadius, g_LogicSphereFunc, 1);
}

static void Parking_Call(cPy_Function *f, cEntity *e, const cParkingZone *z) {
	cPy_Function_AddParamS(f, e->name);
	cPy_Function_AddParamF(f, z->pos.x);
	cPy_Function_AddParamF(f, z->pos.y);
	cPy_Function_AddParamF(f, z->pos.z);
	cPy_Function_AddParamS(f, z->name);
	cPy_Function_Execute(f, 0);
}

static inline float Parking_Dist2(const cParkingZone *z, const vec3_t *p) {
	float dx = z->pos.x - p->x;
	float dy = z->pos.y - p->y;
	float dz = z->pos.z - p->z;

	return dy * dy + dx * dx + dz * dz;
}

/* one zone of each ring; 1 when the player is not in a ship */
static int Parking_Think(cParking *self) {
	vec3_t tmp;
	cEntity *usr = ChType_Usr0();
	byte inShip = 0;
	float d;

	if (!usr) {
		return 1;
	}
	if (Entity_IsType(usr, TYEN_CAR)) {
		d = g_ParkingVehicleCheckDistance;
		inShip = 1;
	} else {
		if (!Entity_IsType(usr, TYEN_WALKCHAR)) {
			return 1;
		}
		d = g_ParkingCharCheckDistance;
	}
	float d2 = d * d;
	if (d2 == 0.0f) {
		return inShip ^ 1;
	}
	if (inShip) {
		cParkingZone *z = Parking_Next(&self->items);
		if (z && Parking_Dist2(z, Entity_GetPos(usr, &tmp)) > d2) {
			Parking_Look(z);
			if (!g_ParkingShip && g_ParkingOther && Entity_IsType(g_ParkingOther, TYEN_ITEM)) {
				Parking_Call(g_OnParkedOut, g_ParkingOther, z);
			}
		}
	}
	cParkingZone *z = Parking_Next(&self->ships);
	if (z && Parking_Dist2(z, Entity_GetPos(usr, &tmp)) > d2) {
		Parking_Look(z);
		if (!g_ParkingOther && g_ParkingShip && Entity_IsType(g_ParkingShip, TYEN_CAR)) {
			Parking_Call(g_OnParkingOccupied, g_ParkingShip, z);
		}
	}
	return inShip ^ 1;
}

static byte Parking_Farther(float d, float best);
static byte Parking_Nearer(float d, float best);
const char *Parking_Find(void *parking, int flag, const vec3_t *pos, vec3_t *out, int farest) {
	cParking *self = parking;
	cParkingList *list = (byte)flag ? &self->items : &self->ships;
	cParkingNode *saved = list->iter;
	cParkingZone *best = NULL;
	ParkingCmp cmp;
	float bestD;

	list->iter = list->head;
	cParkingZone *z = list->head ? &list->head->zone : NULL;
	if ((byte)farest) {
		cmp = (void *)Parking_Farther;
		bestD = 0.0f;
	} else {
		cmp = (void *)Parking_Nearer;
		bestD = FLT_MAX;
	}
	while (z) {
		if ((byte)flag || strcmp(z->name, "_Junkyard")) {
			float d = Parking_Dist2(z, pos);
			if (cmp(d, bestD)) {
				bestD = d;
				best = z;
			}
		}
		cParkingNode *n = list->iter->next;
		list->iter = n;
		if (n == list->head) {
			break;
		}
		z = &n->zone;
	}
	const char *name = NULL;
	if (best) {
		Parking_Look(best);
		if (g_ParkingShip && Entity_IsType(g_ParkingShip, TYEN_CAR)) {
			name = g_ParkingShip->name;
		} else if (g_ParkingOther && Entity_IsType(g_ParkingOther, TYEN_ITEM)) {
			name = g_ParkingOther->name;
		}
	}
	list->iter = saved;
	if (name && out) {
		*out = best->pos;
	}
	return name;
}

/* the comparisons of Parking_Find (farest: d > best, else d <= best) */
static byte Parking_Farther(float d, float best) {
	return d > best;
}

static byte Parking_Nearer(float d, float best) {
	return best >= d;
}

/* the sphere native: a ship (or the other entity) of that name */
static void Parking_Native(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, sizeof(name));
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return;
	}
	if (Entity_IsType(e, TYEN_CAR)) {
		g_ParkingShip = e;
		return;
	}
	g_ParkingOther = e;
}

void Parking_Update(cParking *self, float dt) {
	float t = self->timer - dt;

	self->timer = t;
	if (!(0.0f >= t)) {
		return;
	}
	if (Parking_Think(self)) {
		self->timer = (float)(Weap_Rand01() * 0.25 + 0.25);
		Parking_Think(self);
		return;
	}
	self->timer = (float)(Weap_Rand01() + 1.0);
}

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_ParkingCharCheckDistance, &GVar_ParkingCharCheckDistance, "ParkingCharCheckDistance", 0, "At this distance, the parking will be check.", 20000.0f)
INIT_GVAR_FLOAT(Init_GVar_ParkingVehicleCheckDistance, &GVar_ParkingVehicleCheckDistance, "ParkingVehiCheckDistance", 0, "At this distance, the parking will be check.", 100000.0f)
INIT_GVAR_FLOAT(Init_GVar_ParkingCheckRadius, &GVar_ParkingCheckRadius, "ParkingCheckRadius", 0, "Spere to check the position of the item or ship.", 200.0f)
FINI_CALL(Fini_GVar_OnParkingOccupied, &GVar_OnParkingOccupied.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnParkingOccupied, Fini_GVar_OnParkingOccupied, &GVar_OnParkingOccupied, "OnParkingOccupied", 0, "Callback when a parking zone is occupied.", "")
FINI_CALL(Fini_GVar_OnParkedOut, &GVar_OnParkedOut.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnParkedOut, Fini_GVar_OnParkedOut, &GVar_OnParkedOut, "OnParkedOut", 0, "Callback when a occupied parking zone is free.", "")
