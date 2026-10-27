/*
 * Logic/GenCharLogic.cpp of the game, the first part: the constructor and the small slots of cGenCharLocalLogic
 * (include/scrap/logic.h), the logic of the generic characters. The rest of the source follows at rva 0x14c4f0.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/weapons.h"

cGenCharLocalLogic *__thiscall cGenCharLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent, int noRestart) {
	cBaseLocalLogic_ctor(&self->base, parent);
	self->base.vftable = Game_Ptr(CGENCHARLOCALLOGIC_VFTABLE);
	self->i48 = 0;
	self->i4c = 0;
	cString_ctor(&self->s54);
	self->b58 = 1;
	self->specialOn = 0;
	self->ignoreAlarm = 0;
	self->specialTime = 0.0f;
	float t = Rand_FloatRange(g_GenCharMinSpecialActionTime, g_GenCharMaxSpecialActionTime);
	self->i50 = 0;
	self->searchTime = t;
	cString_Set(&self->s54, LOGIC_EMPTY);
	self->alarmState = 3;
	if (!(byte)noRestart) {
		cGenCharLocalLogic_RestartState(self);
	}
	return self;
}

void __fastcall cGenCharLocalLogic_dtor(cGenCharLocalLogic *self) {
	self->base.vftable = Game_Ptr(CGENCHARLOCALLOGIC_VFTABLE);
	cString_dtor(&self->s54);
	cBaseLocalLogic_dtor(&self->base);
}

cGenCharLocalLogic *__thiscall cGenCharLocalLogic_Delete(cGenCharLocalLogic *self, byte flags) {
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the type name of the action class of a character */
static inline const char *GenChar_TypeName(cEntity *e) {
	return *(char **)(ChType_ActClass(ChType_ActCtrl(e)) + 4);
}

/* vf26: no view flag for a player of the own suspect type, else relaxed and no alarm; team 3 */
void __thiscall cGenCharLocalLogic_UpdateView(cGenCharLocalLogic *self) {
	cBaseLocalLogic *b = &self->base;
	cEntity *p = b->parent;
	const char *type = GenChar_TypeName(p);
	cEntity *usr = ChType_Usr0();

	if (usr && usr->vftable->IsType(usr, TYEN_WALKCHAR) && Logic_IsSuspectType(type) &&
		!strcmp(type, GenChar_TypeName(usr))) {
		WChar_B381(p) = 0;
	} else {
		int st = b->state;
		WChar_B381(p) = !b->busy && !g_pWorld->alarmOn && st != 4 && st != 3 && st != 5 && st != 10;
	}
	WChar_ViewMode(p) = 0;
	Entity_Team(p) = 3;
}

/* e can discover the player: it has the action Discover, Berto and Betty only when the player is not of the type
 * UsrDefChar */
byte __thiscall cGenCharLocalLogic_CanDiscover(cGenCharLocalLogic *self, cEntity *e) {
	(void)self;
	if (!ChType_Vtbl(e)->FindAction(e, "Discover")) {
		return 0;
	}
	const char *type = GenChar_TypeName(e);
	if (strcmp(type, "Berto") && strcmp(type, "Betty")) {
		return 1;
	}
	return cString_CmpStr(g_UsrDefChar, g_pWorld->alarmChars) != 0;
}

void __thiscall cGenCharLocalLogic_SetB58(cGenCharLocalLogic *self, byte v) {
	self->b58 = v;
}

/* vf16: the state of the alarm against e */
void __thiscall cGenCharLocalLogic_OnAlarm(cGenCharLocalLogic *self, cEntity *e) {
	self->base.vftable->SetState(self, self->alarmState, e);
}

int __thiscall cGenCharLocalLogic_GetI50(cGenCharLocalLogic *self) {
	return self->i50;
}

char *__thiscall cGenCharLocalLogic_GetS54(cGenCharLocalLogic *self) {
	return self->s54;
}

/* vf28 and vf27: the AI walks the zone around the origin, in alert or relaxed */
void __thiscall cGenCharLocalLogic_ToAlert(cGenCharLocalLogic *self, cAIChar *ai) {
	cBaseLocalLogic *b = &self->base;

	ai->vftable->SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_GenCharAlertSpeed, 0);
}

void __thiscall cGenCharLocalLogic_ToRelax(cGenCharLocalLogic *self, cAIChar *ai) {
	cBaseLocalLogic *b = &self->base;

	ai->vftable->SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_GenCharRelaxSpeed, 1);
}
