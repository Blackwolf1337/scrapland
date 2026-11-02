/*
 * Logic/Enemies.cpp of the game: the enemy ships of the outdoor missions ("Enemies", include/scrap/logic.h). Every
 * 0.5 s (not in the action set 2) the ships without an enemy target go for the player in its car, or for a random
 * ship of "Friends" (unless IgnoreFriends); a ship after a target without an AI chases the player on foot. The
 * logic of a ship is a plain cLocalLogic with the variable "IgnoreFriends" (shared by all the ships).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/input.h"
#include "scrap/world.h"

/* defined further down or in another file without a header */
cLocalLogic *Enemies_MakeLogic(cEntity *e);

cEnemies *cEnemies_ctor(cEnemies *self) {
	self->usr = NULL;
	self->ignoreFriends = 0;
	self->attacking = 0;
	self->relax = 1;
	self->timer = 0.0f;
	self->list = NULL;
	return self;
}

void Logic_NopDtor(void *self) {
	(void)self;
}

void Logic_NativeNop(cAbstractFile *params) {
	(void)params;
}

void cEnemiesLocalLogic_dtor(cEnemiesLocalLogic *self) {
	self->vftable = CENEMIESLOCALLOGIC_VFTABLE;
	cLocalLogic_dtor(self);
}

cEnemiesLocalLogic *cEnemiesLocalLogic_Delete(cEnemiesLocalLogic *self, byte flags) {
	self->vftable = CENEMIESLOCALLOGIC_VFTABLE;
	cLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

static void Enemies_Chase(cAIVehicle *ai, cEntity *t) {
	cAIVehicle_SetState(ai, AIV_STATE_CHASE, t, (double)g_AIChaseDefault1, (double)g_AIChaseDefault2);
}

static void Enemies_AttackFriend(cAIVehicle *ai, cEntityListData *friends) {
	cEntity *f = Logic_RandomCar(friends);

	if (f) {
		cAIVehicle_SetState(ai, AIV_STATE_ATTACK, f);
	}
}

/* the targets of the ships: usr (the car of the player, NULL without one) or the friends */
void cEnemies_Attack(cEnemies *self, cEntity *usr) {
	Nto1Node *n = Logic_ListFirst(self->list);
	cEntityListData *friends = (cEntityListData *)cWorld_GetList(g_pWorld, "Friends");
	byte hasFriends = friends && friends->numElems && *friends->numElems > 0;
	/* the original keeps one flag: no player at first, later not IgnoreFriends */
	byte flag = usr == NULL;

	for (; n; n = Logic_ListNext(n)) {
		cEntity *e = Logic_AsCar(n->data);
		CHECK(e->vftable->IsType(e, TYEN_CAR) && "wle->IsType(TYEN_VEHICLE)");
		cAIVehicle *ai = e->vftable->GetAI(e);
		CHECK(ai && "enemyAI");
		WLife(e)->arrow.tgType = 0;
		byte ignore = self->ignoreFriends;
		int st = cAIVehicle_GetState(ai);
		if (!ignore && st != 0) {
			cEntity *t = Logic_AttackTarget(ai);
			if (t && Team_IsEnemy(WLife(e)->arrow.tgType, WLife(t)->arrow.tgType)) {
				goto keep;
			}
		}
		if (!usr) {
			if (hasFriends) {
				Enemies_AttackFriend(ai, friends);
			}
		} else if (flag && hasFriends) {
			if (!self->ignoreFriends) {
				Enemies_AttackFriend(ai, friends);
			}
		} else {
			if (usr->vftable->IsType(usr, TYEN_CAR)) {
				Nto1Node *att = ai->vftable->GetTargetNode(ai);
				if (st == 0 || att->target != usr) {
					cAIVehicle_SetState(ai, AIV_STATE_ATTACK, usr);
				}
			} else if (st == 0) {
				if (hasFriends) {
					Enemies_AttackFriend(ai, friends);
				} else {
					Enemies_Chase(ai, usr);
				}
			}
			flag = !self->ignoreFriends;
		}
keep:;
		cEntity *t = Logic_AttackTarget(ai);
		if (cAIVehicle_GetState(ai) != AIV_STATE_CHASE && t && !t->vftable->GetAI(t) && usr &&
			usr->vftable->IsType(usr, TYEN_WALKCHAR)) {
			Enemies_Chase(ai, usr);
		}
	}
}

/* the attack starts once the player has a car; before that the ships may relax once */
void cEnemies_Think(cEnemies *self) {
	int *num = self->list->numElems;

	if (num && *num == 0) {
		return;
	}
	cEntity *usr = ChType_Usr0();
	if (self->attacking) {
		cEnemies_Attack(self, usr);
		return;
	}
	if (!usr || !usr->vftable->IsType(usr, TYEN_CAR)) {
		return;
	}
	if (usr->vftable->IsType(usr, TYEN_CAR)) {
		cEnemies_Attack(self, usr);
		self->usr = usr;
		self->attacking = 1;
		return;
	}
	if (self->relax) {
		cEnemies_Relax(self);
		self->relax = 0;
	}
}

void cEnemies_Init(cEnemies *self) {
	self->attacking = 0;
	self->list = cWorld_NewEntityList(g_pWorld, "Enemies");
	cPy_Function_SetNative(&self->list->onDamage, (nativefunc_t)Logic_NativeNop);
	cPy_Function_SetNative(&self->list->onDeath, (nativefunc_t)Logic_NativeNop);
	self->list->func = (int (*)(void))(void *)Enemies_MakeLogic;
}

/* "IgnoreFriends", a flag of the world (+0x2174, the field of World_Enemies) for every ship */
byte cEnemiesLocalLogic_VarI(cEnemiesLocalLogic *self, const char *name, int *i, byte set) {
	(void)self;
	if (strcmp(name, "IgnoreFriends")) {
		return 0;
	}
	if (set) {
		g_pWorld->logic.enemies.ignoreFriends = *i > 0;
		return 1;
	}
	*i = g_pWorld->logic.enemies.ignoreFriends != 0;
	return 1;
}

/* every ship to the AI state 3 */
void cEnemies_Relax(cEnemies *self) {
	for (Nto1Node *n = Logic_ListFirst(self->list); n; n = Logic_ListNext(n)) {
		cEntity *e = Logic_AsCar(n->data);
		CHECK(e->vftable->IsType(e, TYEN_CAR) && "wle->IsType(TYEN_VEHICLE)");
		cAIVehicle *ai = e->vftable->GetAI(e);
		CHECK(ai && "ai");
		cAIVehicle_SetState(ai, 3, NULL, 5.0, 2.0);
	}
}

void cEnemies_Update(cEnemies *self, float dt) {
	float t = dt + self->timer;

	self->timer = t;
	if (!(0.5f >= t)) {
		self->timer = 0.0f;
		if (Input_GetActionSet() != 2) {
			cEnemies_Think(self);
		}
	}
}

/* the logic of a ship of the list (cEntityListData.func) */
cLocalLogic *Enemies_MakeLogic(cEntity *e) {
	CHECK(e && e->vftable->IsType(e, TYEN_CAR) && e->vftable->IsType(e, TYEN_CAR));
	cLocalLogic *l = operator_new(sizeof(*l));
	if (l) {
		l->parent = e;
		l->vftable = CENEMIESLOCALLOGIC_VFTABLE;
		return l;
	}
	return NULL;
}

/* the vtable of cEnemiesLocalLogic */
const cLocalLogicVtbl cEnemiesLocalLogic_vftable = {
	.Delete = cEnemiesLocalLogic_Delete,
	.GetName = cLocalLogic_GetName,
	.Vf02 = cLocalLogic_Vf02True,
	.VarI = cEnemiesLocalLogic_VarI,
	.VarF = cLocalLogic_VarFFalse,
	.VarS = cLocalLogic_VarSFalse,
	.VarV = cLocalLogic_VarVNone,
};
