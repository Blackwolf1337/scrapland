/*
 * Logic/DominationFriends.cpp of the game: the friend side of the domination mode ("DominationFriends",
 * include/scrap/logic.h). Every 0.5 s every ship goes through cDominationLogic_UpdateShip with the player and the
 * enemy leader.
 */
#include "scrap/chartype.h"
#include "scrap/filepak.h"
#include "scrap/hashtable.h"
#include "scrap/staticinit.h"

#define CDOMINATIONFRIENDSLOGIC_VFTABLE (&cDominationFriendsLogic_vftable)
#define g_OnDominFriendDeath (((cPy_Function *)&GVar_OnDominFriendDeath.func))
static const cDominationLogicVtbl cDominationFriendsLogic_vftable;
static cGVarFunction GVar_OnDominFriendDeath;

/* defined further down or in another file without a header */
static void DominationFriends_OnDeath(cAbstractFile *params);

cDominationFriendsLogic *cDominationFriendsLogic_ctor(cDominationFriendsLogic *self) {
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

void cDominationFriendsLogic_dtor(cDominationFriendsLogic *self) {
	self->base.vftable = CDOMINATIONFRIENDSLOGIC_VFTABLE;
	if (self->side) {
		operator_delete_sized(self->side, sizeof(*self->side));
	}
	cDominationLogic_dtor(&self->base);
}

static cDominationLogic *cDominationFriendsLogic_Delete(cDominationLogic *base, byte flags) {
	cDominationFriendsLogic *self = (cDominationFriendsLogic *)base;
	cDominationFriendsLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

static void cDominationFriendsSide_Think(cDominationFriendsSide *self) {
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
		} else if (!Entity_IsType(e, TYEN_CAR)) {
			e = NULL;
		}
		cDominationLogic_UpdateShip(self->owner, e, usr, leader);
	} while ((n = Logic_ListNext(n)) != NULL);
}

/* vf03 */
static cEntityListData *cDominationFriendsLogic_GetList(cDominationLogic *base) {
	cDominationFriendsLogic *self = (cDominationFriendsLogic *)base;
	return self->side->list;
}

/* vf01 */
static void cDominationFriendsLogic_Init(cDominationLogic *base) {
	cDominationFriendsLogic *self = (cDominationFriendsLogic *)base;
	cDominationFriendsSide *s = self->side;

	CHECK(s->owner && "fl");
	s->list = cWorld_NewEntityList(g_pWorld, "DominationFriends");
	cPy_Function_SetNative(&s->list->onDeath, (nativefunc_t)DominationFriends_OnDeath);
}

/* the native of the deaths */
static cEntity *cDominationFriendsLogic_ReleaseZone(cDominationLogic *self, const char *name);
static void DominationFriends_OnDeath(cAbstractFile *params) {
	Domination_OnDeath(params, &g_pWorld->logic.dominFriends.base, cDominationFriendsLogic_ReleaseZone,
		g_OnDominFriendDeath);
}

static cEntity *cDominationFriendsLogic_ReleaseZone(cDominationLogic *self, const char *name) {
	return Domination_ReleaseZone(self, name, 3, 1);
}

/* vf02 */
static void cDominationFriendsLogic_Update(cDominationLogic *base, float dt) {
	cDominationFriendsLogic *self = (cDominationFriendsLogic *)base;
	cDominationFriendsSide *s = self->side;
	float t = s->timer + dt;

	s->timer = t;
	if (!(0.5f >= t)) {
		s->timer = 0.0f;
		cDominationFriendsSide_Think(s);
	}
}

/* the vtable of cDominationFriendsLogic */
static const cDominationLogicVtbl cDominationFriendsLogic_vftable = {
	.Delete = cDominationFriendsLogic_Delete,
	.Init = cDominationFriendsLogic_Init,
	.Update = cDominationFriendsLogic_Update,
	.GetList = cDominationFriendsLogic_GetList,
	.GetTeam = cDominationLogic_GetTeamTrue,
};

static cGVarFunction GVar_OnDominFriendDeath;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_CALL(Fini_GVar_OnDominFriendDeath, &GVar_OnDominFriendDeath.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnDominFriendDeath, Fini_GVar_OnDominFriendDeath, &GVar_OnDominFriendDeath, "OnDominFriendDeath", 0, "Called when a friend dies", "")
