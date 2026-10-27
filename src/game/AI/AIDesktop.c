/*
 * AI/AIDesktop.cpp of the game: the AI of the desktop (cAIDesktop, a cAIChar with the move control of cAIChar). It
 * presses the action button while it flees, or while it patrols at full walk with the alarm on, as long as it does
 * not strafe, fall or play one of its ActMore and ActUp actions. See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/scrap.h"
#include "scrap/walkcharentity.h"

#define CAIDESKTOP_SIZE 0x380

void *__thiscall cAIDesktop_ctor(void *mem, cEntity *e, int *arg) {
	cAIChar *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->vftable = Game_Ptr(CAIDESKTOP_VFTABLE);
	return self;
}

void __fastcall cAIDesktop_dtor(cAIChar *self) {
	self->vftable = Game_Ptr(CAIDESKTOP_VFTABLE);
	cAIChar_dtor(self);
}

cAIChar *__thiscall cAIDesktop_Delete(cAIChar *self, byte flags) {
	self->vftable = Game_Ptr(CAIDESKTOP_VFTABLE);
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIDESKTOP_SIZE);
	}
	return self;
}

void __thiscall cAIDesktop_UpdateAction0(cAIChar *self) {
	static const char *const busy[] = {"Fall", "ActMoreF", "ActMoreB", "ActUpB", "ActUpF", "ActUpL", "ActUpR"};
	byte act = 0;

	if ((self->state == AIC_GET_AWAY ||
			(self->state == AIC_PATROL_ZONE && self->ctrl->walk == 1.0f && g_pWorld->alarmOn)) &&
		self->ctrl->strafe == 0.0f) {
		uint32_t i;
		for (i = 0; i < sizeof(busy) / sizeof(busy[0]); i++) {
			if (AIChar_ActionIs(self->ent, busy[i])) {
				break;
			}
		}
		if (i == sizeof(busy) / sizeof(busy[0])) {
			act = 1;
		}
	}
	self->ctrl->act = act;
}
