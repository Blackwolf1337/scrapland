/*
 * Entities/Game/Doors.cpp of the game: the doors and accelerators of the levels, see include/scrap/doors.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/doors.h"
#include "scrap/embeddedfxvehicle.h"
#include "scrap/vehicleentity.h"

#define TYEN_OBJECT "Obj"

/* the radius of the door sphere: a value of the action class */
static inline float ActCtrl_Radius(void *ctrl) {
	return ((cActCtrl *)ctrl)->cls->wRadius;
}

static inline const char *ActCtrl_ClassName(void *ctrl) {
	return ((cActCtrl *)ctrl)->cls->name;
}

static inline byte Door_IsActive(cEntity *door, const char *action) {
	return cActCtrl_IsActive(WithLife_Vtbl(door)->GetActCtrl(door), action);
}

/* the action, then its end state as the default loop */
static void Door_Play(cEntity *door, const char *action, const char *loop) {
	cActCtrl_Play(WithLife_Vtbl(door)->GetActCtrl(door), action, 0, 1.0f);
	cActCtrl_SetDefLoop(WithLife_Vtbl(door)->GetActCtrl(door), loop, 0);
}

/* World.h: the entity of local player 0 */
static cEntity *World_Usr0(void) {
	cWorld *w = g_pWorld;
	cEntity *pl = w->owners[0];

	if (pl && !pl->vftable->IsType(pl, TYEN_WITH_LIFE)) {
		CheckFailed("UsrEntities[i]->IsType(\"WithLife\")", "World.h", __TIMESTAMP__, __LINE__);
	}
	return w->owners[0];
}

/* params: the accelerator object, the car that entered it */
void Doors_AcceleratorOnEnter(cAbstractFile *params) {
	char objName[0x80], carName[0x80];
	vec3_t tmp, push, dir;
	const vec3_t *v;

	cAbstractFile_ReadString(params, objName, 0x80);
	cAbstractFile_ReadString(params, carName, 0x80);
	cEntity *car = World_FindEntity(carName);
	if (!car->vftable->IsType(car, TYEN_CAR)) {
		return;
	}
	if (0.5 > __builtin_fabs((double)Veh(car)->controls.acc) && 0.5 > __builtin_fabs((double)Veh(car)->controls.str)) {
		return;
	}
	cEntity *obj = World_FindEntity(objName);
	if (!obj->vftable->IsType(obj, TYEN_OBJECT) || Veh(car)->traffic.active) {
		return;
	}
	float speed = 28000.0f;
	if (World_IsEasySingle() && car->vftable->GetAI(car)) {
		speed = 22400.0f;
	}
	push.x = 0.0f;
	push.y = 0.0f;
	push.z = speed;
	Vec3_Rotate(&dir, &push, Obj_Vtbl(obj)->GetAng(obj, &tmp));
	const vec3_t *vel = Obj_Vtbl(car)->GetVel(car, &tmp);
	/* along the direction of the object, or against it when the car comes from the other side */
	if (vel->y * dir.y + dir.x * vel->x + vel->z * dir.z > 0.0f) {
		v = &dir;
	} else {
		push.x = -dir.x;
		push.y = -dir.y;
		push.z = -dir.z;
		v = &push;
	}
	Obj_Vtbl(car)->SetVel(car, v);
	cVehicleEntity_SetNetFlag(car, 1);
	cVehicleEntity_UpdateSpeed(car);
	if (cVehicleEntity_GetOwnerSlot(car) != -1) {
		cFollowCamEntity *cam = (cFollowCamEntity *)Vehicle_Player(car);
		if (cam && cam->quakeTime <= 0.0f) {
			cam->quakeFactor = 1e-8f;
			cam->quakeTime = 0.35f;
		}
	}
}

void Doors_AutoDoorSlot(cAbstractFile *params) {
	Doors_Slot(params, DOOR_CLOSE | DOOR_FRONT | DOOR_BACK);
}

/* params: the door, an unused float. The entities in the sphere of the door set the side flags first. */
void Doors_Slot(cAbstractFile *params, uint32_t flags) {
	char name[0x80];
	int unused;
	vec3_t tmp;

	cAbstractFile_ReadString(params, name, 0x80);
	params->vftable->Read(params, &unused, 4);
	if (g_IsMapOutDoor && !World_Usr0()) {
		return;
	}
	cEntity *door = World_FindEntity(name);
	cPy_Function_SetNative(g_DoorSphereFunc, (nativefunc_t)GAME_FUNC(Doors_SphereEntity, DOORS_SPHEREENTITY));
	g_DoorBack = 0;
	g_DoorFront = 0;
	g_DoorUsrBack = 0;
	g_DoorUsrFront = 0;
	const vec3_t *p = door->vftable->GetPos(door, &tmp);
	g_DoorPos.x = p->x;
	g_DoorPos.y = p->y;
	g_DoorPos.z = p->z;
	g_DoorAngY = Obj_Vtbl(door)->GetAng(door, &tmp)->y;
	float radius = ActCtrl_Radius(WithLife_Vtbl(door)->GetActCtrl(door));
	cWorld_SphereCallF(g_pWorld, 0x11, door->vftable->GetPos(door, &tmp), radius, g_DoorSphereFunc, 1);
	if (((flags & DOOR_FRONT) && g_DoorFront) || ((flags & DOOR_USR) && (g_DoorUsrFront || g_DoorUsrBack)) ||
		((flags & DOOR_BACK) && g_DoorBack)) {
		if (!Door_IsActive(door, "Open") && !Door_IsActive(door, "Opened") && !Door_IsActive(door, "Close")) {
			Door_Play(door, "Open", "Opened");
			return;
		}
	}
	if ((flags & DOOR_CLOSE) && !g_DoorFront && !g_DoorBack) {
		if (!Door_IsActive(door, "Open") && !Door_IsActive(door, "Close") && !Door_IsActive(door, "Closed")) {
			Door_Play(door, "Close", "Closed");
		}
	}
}

void Doors_AutoDoorUsrSlot(cAbstractFile *params) {
	Doors_Slot(params, DOOR_CLOSE | DOOR_USR);
}

void Doors_BackDoorSlot(cAbstractFile *params) {
	Doors_Slot(params, DOOR_CLOSE | DOOR_BACK);
}

/* params: the door, "on" or "off", a float that is only printed */
void Doors_ChangeDoorState(cAbstractFile *params) {
	char name[0x80], state[0x80];
	float arg;

	cAbstractFile_ReadString(params, name, 0x80);
	cAbstractFile_ReadString(params, state, 0x80);
	params->vftable->Read(params, &arg, 4);
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	CHECK(ent->vftable->IsType(ent, TYEN_OBJECT));
	if (!crt_stricmp(state, "off")) {
		cObjEntity_Switch(ent, 0);
		return;
	}
	if (!crt_stricmp(state, "on")) {
		cObjEntity_Switch(ent, 1);
		return;
	}
	Con_Warnf("ChangeDoorState('%s','%s',%f): unknow state.\n", name, state, (double)arg);
}

void Doors_CloseDoorSlot(cAbstractFile *params) {
	Doors_Slot(params, DOOR_CLOSE);
}

void Doors_FrontDoorSlot(cAbstractFile *params) {
	Doors_Slot(params, DOOR_CLOSE | DOOR_FRONT);
}

/* params: an entity in the sphere of the door; characters (but the gate keepers), player 0 and AI cars count */
void Doors_SphereEntity(cAbstractFile *params) {
	char name[0x80];
	vec3_t tmp;

	cAbstractFile_ReadString(params, name, 0x80);
	if (!name[0]) {
		return;
	}
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return;
	}
	cEntity *usr = World_Usr0();
	if (!e->vftable->IsType(e, TYEN_WALKCHAR) ||
		!strcmp("GateKeeper", ActCtrl_ClassName(WithLife_Vtbl(e)->GetActCtrl(e)))) {
		if (usr != e && (!e->vftable->IsType(e, TYEN_CAR) || !e->vftable->GetAI(e))) {
			return;
		}
	}
	const vec3_t *p = e->vftable->GetPos(e, &tmp);
	double dx = (double)(p->x - g_DoorPos.x);
	double dz = (double)(p->z - g_DoorPos.z);
	float side = (float)(Game_Atan2(dx, dz) - 1.5707963267948966);
	side = NormAng(side + g_DoorAngY);
	if (side > 0.0f) {
		g_DoorFront = 1;
		if (usr == e) {
			g_DoorUsrFront = 1;
		}
	} else {
		g_DoorBack = 1;
		if (usr == e) {
			g_DoorUsrBack = 1;
		}
	}
}

/* params: the door, an unused float. Opens while player 0 is in its sphere (plus the radius of the player). */
void Doors_Usr0AutoDoorSlot(cAbstractFile *params) {
	char name[0x80];
	int unused;
	vec3_t t1, t2;

	cAbstractFile_ReadString(params, name, 0x80);
	params->vftable->Read(params, &unused, 4);
	cEntity *usr = World_Usr0();
	if (!usr) {
		return;
	}
	cEntity *door = World_FindEntity(name);
	float r = ActCtrl_Radius(WithLife_Vtbl(door)->GetActCtrl(door)) + WLife(usr)->prim.radius;
	const vec3_t *pu = usr->vftable->GetPos(usr, &t1);
	const vec3_t *pd = door->vftable->GetPos(door, &t2);
	float dy = pd->y - pu->y;
	float dx = pd->x - pu->x;
	float dz = pd->z - pu->z;
	if (r * r > dy * dy + dx * dx + dz * dz) {
		if (!Door_IsActive(door, "Open") && !Door_IsActive(door, "Opened")) {
			Door_Play(door, "Open", "Opened");
		}
	} else if (!Door_IsActive(door, "Close") && !Door_IsActive(door, "Closed")) {
		Door_Play(door, "Close", "Closed");
		/* the slot function of the door */
		cPy_Function_SetNative(&((cObjEntity *)door)->onObjSlot, NULL);
	}
}
