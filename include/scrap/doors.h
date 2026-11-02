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

extern cPy_Function g_ChTypeSphereFunc;
#define g_DoorSphereFunc (&g_ChTypeSphereFunc)	/* Doors_SphereEntity for the sphere test */
#define DOORS_SPHEREENTITY 0x0f0570

#endif
