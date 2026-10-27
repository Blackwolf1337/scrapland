/*
 * Entities/Char/WalkCharEntity.cpp of the game, its first part (src/game/Entities/Char/WalkCharEntity.c): the
 * walking character (cWalkCharEntity, "WalkChar", 0x574 bytes, a cWithLifeEntity) with its constructor and
 * destructor, the action callbacks of its class and the energy, the render update and the view lock. These sit in
 * the exe between WalkCharControl.cpp and the CHECKs of WalkCharEntity.cpp. The fields the types use are the
 * WChar_ macros of include/scrap/chartype.h. The constructor is declared in include/scrap/world.h.
 */
#ifndef SCRAP_WALKCHARENTITY_H
#define SCRAP_WALKCHARENTITY_H

#include "scrap/entity.h"

#define CWALKCHARENTITY_VFTABLE 0x3b03bc
#define CWALKCHARENTITY_SIZE 0x574

void __fastcall cWalkCharEntity_dtor(cEntity *self);
cEntity *__thiscall cWalkCharEntity_Delete(cEntity *self, uint32_t flags);
/* vf63: the press and release callbacks of the class, the talk with the victim or listener, the energy over dt */
void __thiscall cWalkCharEntity_ActionTick(cEntity *self, float dt);
/* vf24 (UpdateModel): the model at the render position and angles, the head turned to the view (blended by the view
 * lock) */
void __thiscall cWalkCharEntity_UpdateModel(cEntity *self);
/* the view lock of the class (and the blend towards it in dt) */
void __thiscall cWalkCharEntity_UpdateViewLock(cEntity *self, float dt);
/* the energy is full and the victim a WalkChar and no action of priority runs; with checkClass its class must
 * allow it */
byte __thiscall cWalkCharEntity_CanActOnVictim(cEntity *self, byte checkClass);
/* the link node at 0x3e8 unlinked */
void __fastcall cWalkCharEntity_Unlink3e8(cEntity *self);

#endif
