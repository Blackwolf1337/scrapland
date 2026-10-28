/*
 * Entities/Game/Doors.cpp of the game: the C functions of the level scripts for the doors and the accelerators
 * (cPy_NativeFunc "!AutoDoorSlot" and the like, src/game/Entities/Game/Doors.c). A door is an "Obj" with the
 * actions Open, Opened, Close and Closed; the slot functions open it when an entity is in the sphere of the door
 * (radius from its action class) on the side the slot asks for, and close it when there is nobody.
 */
#ifndef SCRAP_DOORS_H
#define SCRAP_DOORS_H

#include "scrap/filesys.h"
#include "scrap/objentity.h"
#include "scrap/pyfunction.h"
#include "scrap/world.h"

/* the slot flags of Doors_Slot */
#define DOOR_CLOSE 0x1		/* close when nobody is near */
#define DOOR_FRONT 0x2		/* open for an entity in front */
#define DOOR_BACK 0x4		/* open for an entity behind */
#define DOOR_USR 0x8		/* open for player 0 on either side */

/* the state of the last sphere test of Doors_Slot */
#define g_DoorFront GAME_GLOBAL_IN(byte, group_447600, 0, 0x447600)		/* an entity in front of the door */
#define g_DoorBack GAME_GLOBAL_IN(byte, group_447600, 1, 0x447601)
#define g_DoorUsrFront GAME_GLOBAL_IN(byte, group_447600, 2, 0x447602)	/* player 0 in front */
#define g_DoorUsrBack GAME_GLOBAL_IN(byte, group_447600, 3, 0x447603)
#define g_DoorAngY GAME_GLOBAL(float, g_DoorAngY, 0x447604)		/* the y angle of the door */
#define g_DoorPos GAME_GLOBAL(vec3_t, g_DoorPos, 0x447658)
#define g_DoorSphereFunc (&GAME_GLOBAL(cPy_Function, g_ChTypeSphereFunc, 0x46679c))	/* Doors_SphereEntity for the sphere test */
#define DOORS_SPHEREENTITY 0x0f0570

/* the entity vtable slots used here (Obj, Car) */
typedef struct cDoorEntityVtbl {
	void *vf00_1f[32];
	vec3_t *(__thiscall *GetAng)(cEntity *self, vec3_t *out);		/* 0x80 */
	void *vf21;
	vec3_t *(__thiscall *GetVel)(cEntity *self, vec3_t *out);		/* 0x88 */
	void(__thiscall *SetVel)(cEntity *self, const vec3_t *vel);		/* 0x8c */
	void *vf24_34[17];
	void *(__thiscall *GetActCtrl)(cEntity *self);				/* 0xd4 */
} cDoorEntityVtbl;
SD_OFFSET(cDoorEntityVtbl, GetAng, 0x80);
SD_OFFSET(cDoorEntityVtbl, SetVel, 0x8c);
SD_OFFSET(cDoorEntityVtbl, GetActCtrl, 0xd4);

/* the action controller of an entity: the action class at 0x8 (its name at 0x4, the door radius at 0x164) */
byte __thiscall cActCtrl_IsActive(void *ctrl, const char *action);
byte __thiscall cActCtrl_Play(void *ctrl, const char *action, int arg, float speed);
/* the action that runs when nothing else does; a warning if the class has no such action */
void __thiscall cActCtrl_SetDefLoop(void *ctrl, const char *action, byte force);
/* the layer i of the actions (16 bytes, the time of its action first) */
float *__thiscall cActCtrl_GetLayer(void *ctrl, int i);

/* vehicles: a state flag for the network game, and the speed from the velocity */
void __thiscall cVehicleEntity_SetNetFlag(cEntity *self, byte on);
void __thiscall cVehicleEntity_UpdateSpeed(cEntity *self);

/* the C functions; params holds the arguments of the call */
void Doors_AcceleratorOnEnter(cAbstractFile *params);
void Doors_Slot(cAbstractFile *params, uint32_t flags);
void Doors_AutoDoorSlot(cAbstractFile *params);
void Doors_AutoDoorUsrSlot(cAbstractFile *params);
void Doors_BackDoorSlot(cAbstractFile *params);
void Doors_ChangeDoorState(cAbstractFile *params);
void Doors_CloseDoorSlot(cAbstractFile *params);
void Doors_FrontDoorSlot(cAbstractFile *params);
void Doors_SphereEntity(cAbstractFile *params);
void Doors_Usr0AutoDoorSlot(cAbstractFile *params);

#endif
