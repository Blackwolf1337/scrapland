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

void *cAIDesktop_ctor(void *mem, cEntity *e, int *arg) {
	cAIDesktop *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->vftable = CAIDESKTOP_VFTABLE;
	return self;
}

void cAIDesktop_dtor(cAIDesktop *self) {
	self->vftable = CAIDESKTOP_VFTABLE;
	cAIChar_dtor(self);
}

cAIDesktop *cAIDesktop_Delete(cAIDesktop *self, byte flags) {
	self->vftable = CAIDESKTOP_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIDESKTOP_SIZE);
	}
	return self;
}

void cAIDesktop_UpdateAction0(cAIDesktop *self) {
	static const char *const busy[] = {"Fall", "ActMoreF", "ActMoreB", "ActUpB", "ActUpF", "ActUpL", "ActUpR"};
	byte act = 0;

	if ((self->state == AIC_GET_AWAY ||
			(self->state == AIC_PATROL_ZONE && self->ctrl->walk == 1.0f && g_pWorld->alarm.on)) &&
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
