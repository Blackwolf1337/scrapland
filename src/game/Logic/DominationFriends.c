/*
 * Logic/DominationFriends.cpp of the game: the friend side of the domination mode ("DominationFriends",
 * include/scrap/logic.h). Every 0.5 s every ship goes through cDominationLogic_UpdateShip with the player and the
 * enemy leader.
 */
#include "scrap/chartype.h"
#include "scrap/filepak.h"
#include "scrap/hashtable.h"

cDominationFriendsLogic *__fastcall cDominationFriendsLogic_ctor(cDominationFriendsLogic *self) {
	cDominationLogic_ctor(&self->base);
	self->base.vftable = CDOMINATIONFRIENDSLOGIC_VFTABLE;
	cDominationFriendsSide *s = operator_new(sizeof(*s));
	if (s) {
		s->owner = &self->base;
		s->list = NULL;
		s->timer = 0.0f;
		self->side = s;
		return self;
	}
	self->side = NULL;
	return self;
}

void __fastcall cDominationFriendsLogic_dtor(cDominationFriendsLogic *self) {
	self->base.vftable = CDOMINATIONFRIENDSLOGIC_VFTABLE;
	if (self->side) {
		operator_delete_sized(self->side, sizeof(*self->side));
	}
	cDominationLogic_dtor(&self->base);
}

cDominationFriendsLogic *__thiscall cDominationFriendsLogic_Delete(cDominationFriendsLogic *self, byte flags) {
	cDominationFriendsLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void __fastcall cDominationFriendsSide_Think(cDominationFriendsSide *self) {
	Nto1Node *n = Logic_ListFirst(self->list);

	if (!n) {
		return;
	}
	cEntity *usr = Logic_AsCar(ChType_Usr0());
	cEntity *leader = cDominationEnemiesLogic_GetLeader((&g_pWorld->logic.dominEnemies));
	do {
		cEntity *e = n->data;
		if (!e) {
			CHECK(e && "e = node->GetNode()");
		} else if (!e->vftable->IsType(e, TYEN_CAR)) {
			e = NULL;
		}
		cDominationLogic_UpdateShip(self->owner, e, usr, leader);
	} while ((n = Logic_ListNext(n)) != NULL);
}

/* vf03 */
cEntityListData *__thiscall cDominationFriendsLogic_GetList(cDominationFriendsLogic *self) {
	return self->side->list;
}

/* vf04 */
int __thiscall cDominationFriendsLogic_GetTeam(cDominationFriendsLogic *self) {
	(void)self;
	return 1;
}

cEntity *__thiscall cDominationFriendsLogic_FindShipOfZone(cDominationFriendsLogic *self, cEntity *zone) {
	for (Nto1Node *n = Logic_ListFirst(self->side->list); n; n = Logic_ListNext(n)) {
		cEntity *e = n->data;
		CHECK(e && "e = node->GetNode()");
		const char *key = e->name;
		cNameItem *it = NameTable_Find(&self->base.assigned, key, HashTable_Slot(&self->base.assigned, key));
		if (it && it->value && (cEntity *)it->value == zone) {
			return Logic_AsCar(e);
		}
	}
	return NULL;
}

/* vf01 */
void __thiscall cDominationFriendsLogic_Init(cDominationFriendsLogic *self) {
	cDominationFriendsSide *s = self->side;

	CHECK(s->owner && "fl");
	s->list = cWorld_NewEntityList(g_pWorld, "DominationFriends");
	cPy_Function_SetNative(&s->list->onDeath, (nativefunc_t)GAME_FUNC(DominationFriends_OnDeath, 0x146630));
}

/* the native of the deaths */
void DominationFriends_OnDeath(cAbstractFile *params) {
	Domination_OnDeath(params, &g_pWorld->logic.dominFriends.base, cDominationFriendsLogic_ReleaseZone,
		g_OnDominFriendDeath);
}

cEntity *__thiscall cDominationFriendsLogic_ReleaseZone(cDominationLogic *self, const char *name) {
	return Domination_ReleaseZone(self, name, 3, 1);
}

/* vf02 */
void __thiscall cDominationFriendsLogic_Update(cDominationFriendsLogic *self, float dt) {
	cDominationFriendsSide *s = self->side;
	float t = s->timer + dt;

	s->timer = t;
	if (!(0.5f >= t)) {
		s->timer = 0.0f;
		cDominationFriendsSide_Think(s);
	}
}
