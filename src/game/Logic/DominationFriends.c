/*
 * Logic/DominationFriends.cpp of the game: the friend side of the domination mode ("DominationFriends",
 * include/scrap/logic.h). Every 0.5 s every ship goes through cDominationLogic_UpdateShip with the player and the
 * enemy leader.
 */
#include "scrap/chartype.h"
#include "scrap/filepak.h"
#include "scrap/hashtable.h"

cDominationLogic *__fastcall cDominationFriendsLogic_ctor(cDominationLogic *self) {
	cDominationLogic_ctor(self);
	self->vftable = Game_Ptr(CDOMINATIONFRIENDSLOGIC_VFTABLE);
	cDominationFriendsSide *s = operator_new(sizeof(*s));
	if (s) {
		s->owner = self;
		s->list = NULL;
		s->timer = 0.0f;
		self->side = (cDominationSide *)s;
		return self;
	}
	self->side = NULL;
	return self;
}

void __fastcall cDominationFriendsLogic_dtor(cDominationLogic *self) {
	self->vftable = Game_Ptr(CDOMINATIONFRIENDSLOGIC_VFTABLE);
	if (self->side) {
		operator_delete_sized(self->side, sizeof(cDominationFriendsSide));
	}
	cDominationLogic_dtor(self);
}

cDominationLogic *__thiscall cDominationFriendsLogic_Delete(cDominationLogic *self, byte flags) {
	cDominationFriendsLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, 0x14);
	}
	return self;
}

void __fastcall cDominationFriendsSide_Think(cDominationFriendsSide *self) {
	Nto1Node *n = Logic_ListFirst(self->list);

	if (!n) {
		return;
	}
	cEntity *usr = Logic_AsCar(ChType_Usr0());
	cEntity *leader = cDominationEnemiesLogic_GetLeader(World_DominEnemies());
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
cEntityListData *__thiscall cDominationFriendsLogic_GetList(cDominationLogic *self) {
	return self->side->list;
}

/* vf04 */
int __thiscall cDominationFriendsLogic_GetTeam(cDominationLogic *self) {
	(void)self;
	return 1;
}

cEntity *__thiscall cDominationFriendsLogic_FindShipOfZone(cDominationLogic *self, cEntity *zone) {
	for (Nto1Node *n = Logic_ListFirst(self->side->list); n; n = Logic_ListNext(n)) {
		cEntity *e = n->data;
		CHECK(e && "e = node->GetNode()");
		const char *key = e->name;
		cNameItem *it = NameTable_Find(&self->assigned, key, HashTable_Slot(&self->assigned, key));
		if (it && it->value && (cEntity *)it->value == zone) {
			return Logic_AsCar(e);
		}
	}
	return NULL;
}

/* vf01 */
void __thiscall cDominationFriendsLogic_Init(cDominationLogic *self) {
	cDominationFriendsSide *s = (cDominationFriendsSide *)self->side;

	CHECK(s->owner && "fl");
	s->list = cWorld_NewEntityList(g_pWorld, "DominationFriends");
	cPy_Function_SetNative(&s->list->onDeath, (nativefunc_t)Game_Ptr(0x146630));
}

/* the native of the deaths */
void DominationFriends_OnDeath(cAbstractFile *params) {
	Domination_OnDeath(params, World_DominFriends(), cDominationFriendsLogic_ReleaseZone, g_OnDominFriendDeath);
}

cEntity *__thiscall cDominationFriendsLogic_ReleaseZone(cDominationLogic *self, const char *name) {
	return Domination_ReleaseZone(self, name, 3, 1);
}

/* vf02 */
void __thiscall cDominationFriendsLogic_Update(cDominationLogic *self, float dt) {
	cDominationFriendsSide *s = (cDominationFriendsSide *)self->side;
	float t = s->timer + dt;

	s->timer = t;
	if (!(0.5f >= t)) {
		s->timer = 0.0f;
		cDominationFriendsSide_Think(s);
	}
}
