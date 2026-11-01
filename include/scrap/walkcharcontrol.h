/*
 * Entities/Char/WalkCharControl.cpp of the game (src/game/Entities/Char/WalkCharControl.c): the control of a walking
 * character (at 0x4ec of cWalkCharEntity). Each frame it turns the input actions of the player slot of the character
 * (action set 3) into the turn, walk and strafe values and the action buttons; the script reads them by name ("HRot",
 * "VRot", "Walk", "Str", "Act"). With "WalkCameraCenter" set the view turns back to the front after that many seconds
 * without input. The node links the control to the entity the view stays on.
 * The cWalkCharEntity functions between the control and Entities/Char/WalkCharEntity.cpp (constructor, destructor,
 * the energy and the action callbacks) are in the same C file.
 */
#ifndef SCRAP_WALKCHARCONTROL_H
#define SCRAP_WALKCHARCONTROL_H

#include "scrap/entity.h"
#include "scrap/nto1links.h"

typedef struct cWalkCharControl {
	float hRot;			/* 0x00 "HRot" the turn left/right */
	float vRot;			/* 0x04 "VRot" the turn up/down */
	float centerTime;		/* 0x08 the seconds left until the view centers (WalkCameraCenter) */
	float walk;			/* 0x0c "Walk" forward/back */
	float strafe;			/* 0x10 "Str" */
	float jump;			/* 0x14 */
	byte act;			/* 0x18 "Act" the action button */
	byte prevAct;			/* 0x19 of the last frame (cWalkCharControl_Latch) */
	byte act2;			/* 0x1a action 18 */
	byte prevAct2;			/* 0x1b */
	byte act3;			/* 0x1c action 17 while the action button is up */
	byte prevAct3;			/* 0x1d */
	byte disabled;			/* 0x1e no input at all */
	byte lockStart;			/* 0x1f the lock button went down (action 20, or 19 for some classes) */
	byte lock;			/* 0x20 the view stays on the target */
	byte pad_21[3];
	Nto1Node target;		/* 0x24 the entity the view turns to */
} cWalkCharControl;
SD_OFFSET(cWalkCharControl, walk, 0x0c);
SD_OFFSET(cWalkCharControl, act, 0x18);
SD_OFFSET(cWalkCharControl, disabled, 0x1e);
SD_OFFSET(cWalkCharControl, lock, 0x20);
SD_OFFSET(cWalkCharControl, target, 0x24);
SD_SIZE(cWalkCharControl, 0x34);

#define g_WalkCameraCenter GAME_GLOBAL_IN(float, GVar_WalkCameraCenter, 16)	/* "WalkCameraCenter" */
#define g_AlwaysRun GAME_GLOBAL_IN(int, GVar_AlwaysRun, 16)		/* "AlwaysRun" */
#define g_PadTurnSpeed GAME_GLOBAL_IN(float, GVar_PadTurnSpeed, 16)	/* "PadTurnSpeed" */
#define g_EnergyBarActive GAME_GLOBAL_IN(int, GVar_EnergyBarActive, 16)	/* "EnergyBarActive" */
#define g_EnergyRechargeTime GAME_GLOBAL_IN(float, GVar_EnergyRechargeTime, 16)	/* "EnergyRechargeTime" */

cWalkCharControl *__fastcall cWalkCharControl_ctor(cWalkCharControl *self);
void __fastcall cWalkCharControl_dtor(cWalkCharControl *self);
/* the values cleared, the control off (on) or on */
void __thiscall cWalkCharControl_Reset(cWalkCharControl *self, byte on);
/* the values cleared, the control on */
void __fastcall cWalkCharControl_Clear(cWalkCharControl *self);
/* the center time back to WalkCameraCenter */
void __fastcall cWalkCharControl_RestartCenter(cWalkCharControl *self);
/* the buttons of this frame become the ones of the last (the end of the frame of wle; wle and dt unused) */
void __thiscall cWalkCharControl_Latch(cWalkCharControl *self, cEntity *wle, float dt);
/* the view turns to e (NULL: to nothing) */
void __thiscall cWalkCharControl_SetTarget(cWalkCharControl *self, cEntity *e);
/* the script values by name: set 1 writes *d, 0 reads it; 0 for an unknown name */
byte __thiscall cWalkCharControl_VarF(cWalkCharControl *self, const char *name, double *d, byte set);
/* the input of the character wle for this frame */
void __thiscall cWalkCharControl_Update(cWalkCharControl *self, cEntity *wle, float dt);

#endif
