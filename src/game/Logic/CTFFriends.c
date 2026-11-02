/*
 * Logic/CTFFriends.cpp of the game: the friend ships of capture the flag ("CTFFriends", include/scrap/logic.h).
 * Every 0.5 s the free ships go for the enemy leader (up to PercFriendsAttackLeaderWhenWinning of them while the
 * team of the player loses) or a random enemy; a ship hit below 75 percent of its life by an enemy (not the
 * player) turns against it.
 */
#include "scrap/chartype.h"
#include "scrap/vehicleentity.h"

/* defined further down or in another file without a header */
void CTFFriends_OnDamage(cAbstractFile *params);

cEntity *Logic_FindCar(const char *name) {
	return Logic_AsCar(World_FindEntity(name));
}

cCTFFriends *cCTFFriends_ctor(cCTFFriends *self) {
	cCTFFriendsData *d = operator_new(sizeof(*d));

	if (!d) {
		self->d = NULL;
		return self;
	}
	d->list = NULL;
	d->timer = 0.0f;
	self->d = d;
	return self;
}

void cCTFFriends_dtor(cCTFFriends *self) {
	if (self->d) {
		operator_delete_sized(self->d, sizeof(*self->d));
	}
}

cEntityListData *cCTFFriends_GetList(cCTFFriends *self) {
	return self->d->list;
}

/* a random enemy ship, or the enemy leader (once while no ship attacks it) if attackLeader */
cEntity *cCTFFriendsData_PickTarget(cCTFFriendsData *self, cEntity *e, byte attackLeader) {
	(void)e;
	cEntityListData *enemies = cCTFEnemies_GetList((&g_pWorld->logic.ctfEnemies));
	Nto1Node *n = Logic_ListFirst(enemies);
	byte pick = attackLeader && !self->numAttack;
	int num = Logic_Count(enemies->numElems);
	if (pick || !num) {
		num++;
	}
	int r = crt_rand() % num;
	if (pick && r == num - 1) {
		self->numAttack++;
		return Logic_FindCar(cCTFEnemies_GetLeader((&g_pWorld->logic.ctfEnemies)));
	}
	for (int i = 0; n; n = Logic_ListNext(n), i++) {
		if (i == r) {
			return Logic_AsCar(n->data);
		}
	}
	return NULL;
}

void cCTFFriendsData_Think(cCTFFriendsData *self) {
	Nto1Node *n = Logic_ListFirst(self->list);

	if (!n) {
		return;
	}
	cEntity *usr = ChType_Usr0();
	if (usr) {
		/* the result is not used */
		usr->vftable->IsType(usr, TYEN_CAR);
	}
	cEntity *leader = Logic_FindCar(cCTFEnemies_GetLeader((&g_pWorld->logic.ctfEnemies)));
	double maxAttack = ceil((double)((float)Logic_Count(self->list->numElems) *
		g_PercFriendsAttackLeaderWhenWinning));
	self->numAttack = 0;
	do {
		cEntity *e = Logic_AsCar(n->data);
		CHECK(e->vftable->IsType(e, TYEN_CAR));
		cAIVehicle *ai = e->vftable->GetAI(e);
		CHECK(ai);
		int st = cAIVehicle_GetState(ai);
		if (st == AIV_STATE_ATTACK) {
			if (leader == Logic_AsCar(Logic_AttackTarget(ai))) {
				self->numAttack++;
			}
		} else if (st == 0 || st == 3) {
			int win = g_PlayerCTFTeamIsWinning;
			if (win == 0 && leader && self->numAttack < (int)maxAttack) {
				cAIVehicle_SetState(ai, AIV_STATE_ATTACK, leader);
				self->numAttack++;
			} else {
				cEntity *t = cCTFFriendsData_PickTarget(self, e, win == 1);
				if (t) {
					cAIVehicle_SetState(ai, AIV_STATE_ATTACK, t);
				}
			}
		}
	} while ((n = Logic_ListNext(n)) != NULL);
}

void cCTFFriends_Init(cCTFFriends *self) {
	cCTFFriendsData *d = self->d;

	d->list = cWorld_NewEntityList(g_pWorld, "CTFFriends");
	cPy_Function_SetNative(&d->list->onDamage, (nativefunc_t)CTFFriends_OnDamage);
}

/* the native of the hits: the ship and its attacker */
void CTFFriends_OnDamage(cAbstractFile *params) {
	char name[0x80], attacker[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cAbstractFile_ReadString(params, attacker, 0x80);
	if (!attacker[0]) {
		return;
	}
	cEntity *a = World_FindEntity(attacker);
	if (!a || !a->vftable->IsType(a, TYEN_CAR)) {
		return;
	}
	CHECK(a->vftable->IsType(a, TYEN_CAR));
	cEntity *usr = ChType_Usr0();
	if (usr && usr->vftable->IsType(usr, TYEN_CAR) && usr == a) {
		return;
	}
	cEntity *v = Logic_FindCar(name);
	if (!v) {
		return;
	}
	CHECK(v->vftable->IsType(v, TYEN_CAR));
	cVehicleEntity *veh = (cVehicleEntity *)v;
	if (veh->wl.life >= (int)((float)veh->maxLife * 0.75f)) {
		return;
	}
	cAIVehicle *ai = v->vftable->GetAI(v);
	CHECK(ai);
	cAIVehicle_SetState(ai, AIV_STATE_ATTACK, a);
}

void cCTFFriends_Update(cCTFFriends *self, float dt) {
	cCTFFriendsData *d = self->d;
	float t = d->timer + dt;

	d->timer = t;
	if (!(0.5f >= t)) {
		d->timer = 0.0f;
		cCTFFriendsData_Think(d);
	}
}

cGVar GVar_PercFriendsAttackLeaderWhenWinning;

