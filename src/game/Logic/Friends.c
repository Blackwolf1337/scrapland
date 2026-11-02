/*
 * Logic/Friends.cpp of the game: the friend ships of the outdoor missions ("Friends", include/scrap/logic.h).
 * Every 1.5 s (not in the action set 2): a free ship follows the player in its car or patrols, a ship attacks the
 * next active enemy ship of the world in attack (the walk over the entities of the world goes on from ship to
 * ship and stops after one round), and drops a target that is no enemy any more.
 */
#include "scrap/chartype.h"
#include "scrap/input.h"
#include "scrap/world.h"

cFriends *cFriends_ctor(cFriends *self) {
	self->timer = 0.0f;
	self->list = NULL;
	return self;
}

static void Friends_Patrol(cAIVehicle *ai) {
	cAIVehicle_SetState(ai, 3, 1, 5.0, 2.0);
}

static void Friends_Follow(cAIVehicle *ai, cEntity *usr) {
	cAIVehicle_SetState(ai, AIV_STATE_CHASE, usr, (double)g_AIChaseDefault1, (double)g_AIChaseDefault2);
}

/* t is an active enemy car of v */
static byte Friends_IsEnemyCar(cEntity *v, cEntity *t) {
	return t->active && t->vftable->IsType(t, TYEN_CAR) && Team_IsEnemy(WLife(t)->arrow.tgType,
		WLife(v)->arrow.tgType);
}

static void cFriends_Attack(cFriends *self, cEntity *usr) {
	Nto1Node *n = Logic_ListFirst(self->list);
	cEntity *cur = cWorld_GetFirstEntity(g_pWorld);

	if (!cur) {
		return;
	}
	/* one round over the entities of the world is done */
	byte done = 0;
	for (; n; n = Logic_ListNext(n)) {
		cEntity *v = n->data;
		CHECK(v && "friendVehicle");
		CHECK(v->vftable->IsType(v, TYEN_CAR) && "friendVehicle->IsType(TYEN_VEHICLE)");
		cAIVehicle *ai = v->vftable->GetAI(v);
		CHECK(ai && "friendAI");
		int st = cAIVehicle_GetState(ai);
		WLife(v)->arrow.tgType = 2;
		if (st == 0) {
			if (usr) {
				Friends_Follow(ai, usr);
			} else {
				Friends_Patrol(ai);
			}
			continue;
		}
		if (st == AIV_STATE_ATTACK) {
			cEntity *t = ai->vftable->GetTargetEnt(&ai->base.base);
			if (!t || !Friends_IsEnemyCar(v, t)) {
				cAIVehicle_SetState(ai, 0);
			}
			continue;
		}
		if (!usr) {
			if (st == AIV_STATE_CHASE) {
				Friends_Patrol(ai);
				continue;
			}
		} else if (st == 3) {
			Friends_Follow(ai, usr);
			continue;
		}
		if (done) {
			continue;
		}
		cEntity *start = cur;
		while (cAIVehicle_GetState(ai) != AIV_STATE_ATTACK) {
			if (!cur) {
				cur = cWorld_GetFirstEntity(g_pWorld);
			} else {
				byte found = 0;
				if (Friends_IsEnemyCar(v, cur) && cur->vftable->GetAI(cur) &&
					cAIVehicle_GetState(cur->vftable->GetAI(cur)) == AIV_STATE_ATTACK) {
					found = 1;
					cAIVehicle_SetState(ai, AIV_STATE_ATTACK, cur);
				}
				cur = cEntity_Next(cur);
				if (found) {
					break;
				}
			}
			if (cur == start) {
				done = 1;
				break;
			}
		}
	}
}

static void cFriends_Think(cFriends *self) {
	int *num = self->list->numElems;

	if (num && *num == 0) {
		return;
	}
	cEntity *usr = NULL;
	if (Input_GetActionSet() != 2) {
		usr = cWorld_GetUsrEntity(g_pWorld, 0);
		if (usr && !usr->vftable->IsType(usr, TYEN_CAR)) {
			usr = NULL;
		}
	}
	cFriends_Attack(self, usr);
}

void cFriends_Init(cFriends *self) {
	self->list = cWorld_NewEntityList(g_pWorld, "Friends");
	cPy_Function_SetNative(&self->list->onDamage, (nativefunc_t)Logic_NativeNop);
	cPy_Function_SetNative(&self->list->onDeath, (nativefunc_t)Logic_NativeNop);
}

void cFriends_Update(cFriends *self, float dt) {
	float t = self->timer + dt;

	self->timer = t;
	if (!(1.5f >= t)) {
		self->timer = 0.0f;
		cFriends_Think(self);
	}
}
