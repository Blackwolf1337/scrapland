/*
 * Logic/MessengerLogic.cpp of the game: the messengers (cMessengers of the GenChar group, the list "Messenger")
 * and their logic (a GenChar logic that ignores the alarm). The messengers carry the data pack "MessengerDataPack"
 * between the desks of the tube state (functionaries or the door makers); the think that follows the pack
 * (cMessengers_Think, rva 0x153350) and its delivery (rva 0x153940) are not in C yet.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/scrapmodel.h"
#include "scrap/weapons.h"

#define MSG_VT(self) ((self)->base.vftable)

void *__thiscall cMessengers_ctor(void *self) {
	return self;
}

void __fastcall cMessengers_dtor(void *self) {
	(void)self;
}

cEntity *__fastcall cMessengers_FindDesk(cMessengers *self) {
	const char *type = g_MessengerTubeState ? "BureaucracyDesktopFunctionary" : "DoorDataTMaker";
	float count = (float)self->count;
	int k = (int)(float)(Weap_Rand01() * (double)count + 0.0);
	cEntity *best = NULL;
	int found = 0;

	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (!e->flag_4c || !e->vftable->IsType(e, TYEN_WITH_LIFE) || Logic_RadarShow(e) || !ChType_ActCtrl(e) ||
			strcmp(Logic_TypeName(e), type)) {
			continue;
		}
		if (!best || k <= 0) {
			best = e;
			if (k <= 0) {
				k = self->count;
			}
		}
		k--;
		found++;
	}
	if (found > self->count) {
		self->count = found;
	}
	return best;
}

void __fastcall cMessengers_Init(void *obj) {
	cMessengers *self = obj;

	self->list = cWorld_NewEntityList(g_pWorld, "Messenger");
	self->list->func = (int (*)(void))Game_Ptr(GENCHARGROUP_MAKELOGIC);
	g_MessengerDataPackMissionArrow = g_MessengerDataPackMissionArrowDef;
	g_MessengerTubeState = g_MessengerTubeStateDef;
	cPy_Function_Clear(g_MessengerDropCallback);
	g_MessengerDropPrice = g_MessengerDropPriceDef;
	self->timer = 5.0f;
	self->count = 5;
	self->desk = NULL;
	self->target = NULL;
}

void __thiscall cMessengers_SetStateAll(cMessengers *self, int state, cEntity *e) {
	for (Nto1Node *n = Logic_ListFirst(self->list); n; n = Logic_ListNext(n)) {
		cEntity *m = n->data;
		cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(m);
		if (m == e || Logic_RadarShow(m)) {
			continue;
		}
		if (state == -1) {
			if (!l->idle && !l->alwaysLock) {
				l->vftable->RestartState(l);
			}
		} else {
			l->vftable->SetState(l, state, e);
		}
	}
}

void *__thiscall cMessengerLocalLogic_ctor(void *mem, cEntity *parent) {
	cGenCharLocalLogic *self = mem;

	cGenCharLocalLogic_ctor(self, parent, 0);
	MSG_VT(self) = Game_Ptr(CMESSENGERLOCALLOGIC_VFTABLE);
	cGenCharLocalLogic_SetState(self, GENCHAR_RELAX, NULL);
	self->ignoreAlarm = 1;
	return self;
}

cGenCharLocalLogic *__thiscall cMessengerLocalLogic_Delete(cGenCharLocalLogic *self, byte flags) {
	MSG_VT(self) = Game_Ptr(CMESSENGERLOCALLOGIC_VFTABLE);
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf28 and vf27: the same walk in alert and relaxed */
void __thiscall cMessengerLocalLogic_ToAlert(cGenCharLocalLogic *self, cAIChar *ai) {
	(void)self;
	ai->vftable->SetState(ai, 1, 1.0, 0);
}

/* vf17: team 3, 0 while it attacks a player */
void __thiscall cMessengerLocalLogic_Update(cGenCharLocalLogic *self, float dt) {
	cGenCharLocalLogic_Update(self, dt);
	Entity_Team(self->base.parent) = 3;
	if (self->base.state == GENCHAR_ATTACK) {
		cEntity *t = self->base.target.target;
		if (t && t->vftable->IsType(t, TYEN_WALKCHAR) && cWalkCharEntity_GetOwnerSlot(t) != -1) {
			Entity_Team(self->base.parent) = 0;
		}
	}
}

/* vf34: to the node DC_DataPack of e when it has one (MessengerGotoSpeed, 0.25 in the easy single game), else the
 * walk of the GenChars */
void __thiscall cMessengerLocalLogic_GoTo(cGenCharLocalLogic *self, cAIChar *ai, cEntity *e) {
	vec3_t t1, t2, d, dir;
	matrix_t m;

	if (!e->vftable->IsType(e, TYEN_WITH_LIFE) || !Obj_Vtbl(e)->GetModel(e)) {
		cGenCharLocalLogic_GoTo(self, ai, e);
		return;
	}
	int node = CGameModel_FindNodoR(Obj_Vtbl(e)->GetModel(e), "DC_DataPack");
	if (node == -1) {
		cGenCharLocalLogic_GoTo(self, ai, e);
		return;
	}
	float speed = World_IsEasySingle() ? 0.25f : g_MessengerGotoSpeed;
	cEntity *p = self->base.parent;
	const vec3_t *pp = p->vftable->GetPos(p, &t1);
	const vec3_t *ep = e->vftable->GetPos(e, &t2);
	d.y = ep->y - pp->y;
	d.x = ep->x - pp->x;
	d.z = ep->z - pp->z;
	float len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	if (len == 0.0f) {
		dir = d;
	} else {
		float inv = 1.0f / len;
		dir.x = d.x * inv;
		dir.y = d.y * inv;
		dir.z = d.z * inv;
	}
	const matrix_t *mm = CGameModel_NodoMatrix(Obj_Vtbl(e)->GetModel(e), &m, node, 0, NULL, 0);
	vec3_t np = { mm->m[12], mm->m[13], mm->m[14] };
	ai->vftable->SetState(ai, 4, np, dir, 70.0, (double)speed);
}
