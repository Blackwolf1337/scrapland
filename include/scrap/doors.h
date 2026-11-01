/*
 * Entities/Game/Doors.cpp of the game: the C functions of the level scripts for the doors and the accelerators
 * (cPy_NativeFunc "!AutoDoorSlot" and the like, src/game/Entities/Game/Doors.c). A door is an "Obj" with the
 * actions Open, Opened, Close and Closed; the slot functions open it when an entity is in the sphere of the door
 * (radius from its action class) on the side the slot asks for, and close it when there is nobody.
 */
#ifndef SCRAP_DOORS_H
#define SCRAP_DOORS_H

#include "scrap/actions.h"
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
#define g_DoorFront GAME_GLOBAL_IN(byte, group_447600, 0)		/* an entity in front of the door */
#define g_DoorBack GAME_GLOBAL_IN(byte, group_447600, 1)
#define g_DoorUsrFront GAME_GLOBAL_IN(byte, group_447600, 2)	/* player 0 in front */
#define g_DoorUsrBack GAME_GLOBAL_IN(byte, group_447600, 3)
extern float g_DoorAngY;		/* the y angle of the door */
extern vec3_t g_DoorPos;
#define g_DoorSphereFunc (&GAME_GLOBAL(cPy_Function, g_ChTypeSphereFunc))	/* Doors_SphereEntity for the sphere test */
#define DOORS_SPHEREENTITY 0x0f0570


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
