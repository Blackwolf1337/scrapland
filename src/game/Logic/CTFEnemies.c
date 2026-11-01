/*
 * Logic/CTFEnemies.cpp of the game: the enemy ships of capture the flag ("CTFEnemies", include/scrap/logic.h). Every
 * 0.5 s the free ships get a target: the player (up to PercEnemiesAttackPlayerWhenWinning of them while its team
 * wins) or a random friend; the leader goes for LeaderFlag, or the player while its bonus time runs. A hit of a
 * ship below 75 percent of its life turns it against the attacker. The logic of a ship is a plain cLocalLogic
 * with the script variable "IsLeader".
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/itementity.h"
#include "scrap/vehicleentity.h"

cEntity *Logic_AsCar(cEntity *e) {
	if (e && !e->vftable->IsType(e, TYEN_CAR)) {
		return NULL;
	}
	return e;
}

cCTFEnemies *__fastcall cCTFEnemies_ctor(cCTFEnemies *self) {
	cCTFEnemiesData *d = operator_new(sizeof(*d));

	if (!d) {
		self->d = NULL;
		return self;
	}
	cString_ctor(&d->leader);
	cString_Set(&d->leader, LOGIC_EMPTY);
	d->list = NULL;
	d->timer = 0.0f;
	d->bonus = 0.0f;
	self->d = d;
	return self;
}

void __fastcall cCTFEnemies_dtor(cCTFEnemies *self) {
	cCTFEnemiesData *d = self->d;

	if (d) {
		cString_dtor(&d->leader);
		operator_delete_sized(d, sizeof(*d));
	}
}

cCTFEnemiesLocalLogic *__thiscall cCTFEnemiesLocalLogic_Delete(cCTFEnemiesLocalLogic *self, byte flags) {
	self->vftable = CCTFENEMIESLOCALLOGIC_VFTABLE;
	cLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the ships after the flag: the AI state 6 to the flag of that name */
void __thiscall cCTFEnemiesData_GoToFlag(cCTFEnemiesData *self, const char *name, cAIVehicle *ai) {
	vec3_t tmp;

	(void)self;
	if (!name || !ai || !*name) {
		return;
	}
	cEntity *flag = World_FindEntity(name);
	if (flag) {
		vec3_t pos = *flag->vftable->GetPos(flag, &tmp);
		cAIVehicle_SetState(ai, AIV_STATE_FLAG, pos, (double)((cItemEntity *)flag)->radius, 0);
	}
}

/* a random friend ship, or the player (once while no ship attacks it) if attackUsr */
cEntity *__thiscall cCTFEnemiesData_PickTarget(cCTFEnemiesData *self, cEntity *e, byte attackUsr) {
	(void)e;
	cEntity *usr = ChType_Usr0();
	cEntityListData *friends = cCTFFriends_GetList((&g_pWorld->logic.ctfFriends));
	Nto1Node *n = Logic_ListFirst(friends);
	byte pick = attackUsr && !self->numAttack;
	int num = Logic_Count(friends->numElems);
	if (pick || !num) {
		num++;
	}
	int r = crt_rand() % num;
	if (pick && r == num - 1 && usr && usr->vftable->IsType(usr, TYEN_CAR)) {
		self->numAttack++;
		return Logic_AsCar(usr);
	}
	for (int i = 0; n; n = Logic_ListNext(n), i++) {
		if (i == r) {
			return Logic_AsCar(n->data);
		}
	}
	return NULL;
}

/* the state 3 of a ship without a target (the constant arguments of the original) */
static void CTF_Patrol(cAIVehicle *ai) {
	cAIVehicle_SetState(ai, 3, NULL, 5.0, 2.0);
}

/* the flag name copied for the leader (the strcpy of the original, the buffer has 0x80 bytes) */
static void CTF_SetFlagName(const char *flag) {
	size_t n = strnlen(flag, 0x7f);

	memcpy(g_CTFFlagName, flag, n);
	g_CTFFlagName[n] = 0;
}

void __fastcall cCTFEnemiesData_Think(cCTFEnemiesData *self) {
	Nto1Node *n = Logic_ListFirst(self->list);

	if (!n) {
		return;
	}
	cEntity *usr = Logic_AsCar(ChType_Usr0());
	cEntity *leader = Logic_AsCar(World_FindEntity(self->leader));
	double maxAttack = crt_ceil((double)((float)Logic_Count(self->list->numElems) *
		g_PercEnemiesAttackPlayerWhenWinning));
	self->numAttack = 0;
	do {
		cEntity *e = Logic_AsCar(n->data);
		CHECK(e->vftable->IsType(e, TYEN_CAR));
		cAIVehicle *ai = e->vftable->GetAI(e);
		CHECK(ai);
		int st = cAIVehicle_GetState(ai);
		cEntity *t = Logic_AsCar(Logic_AttackTarget(ai));
		if (st == AIV_STATE_ATTACK && t && usr == t) {
			self->numAttack++;
		}
		if (leader != e) {
			if (st == 0 || st == 3) {
				int win = g_PlayerCTFTeamIsWinning;
				if (win == 1 && usr && self->numAttack < (int)maxAttack) {
					cAIVehicle_SetState(ai, AIV_STATE_ATTACK, usr);
					self->numAttack++;
				} else {
					cEntity *p = cCTFEnemiesData_PickTarget(self, e, win == 0);
					if (p) {
						cAIVehicle_SetState(ai, AIV_STATE_ATTACK, p);
					}
				}
			}
			continue;
		}
		if (self->bonus > 0.0f) {
			if (usr) {
				if (st != AIV_STATE_ATTACK) {
					cAIVehicle_SetState(ai, AIV_STATE_ATTACK, usr);
				}
				continue;
			}
			self->bonus = 0.0f;
			CTF_Patrol(ai);
			continue;
		}
		const char *flag = g_LeaderFlag;
		if (st == 0 || st == 3) {
			g_CTFFlagName[0] = 0;
			if (strcmp(flag, LOGIC_EMPTY)) {
				CTF_SetFlagName(flag);
				cCTFEnemiesData_GoToFlag(self, flag, ai);
			} else if (usr) {
				cAIVehicle_SetState(ai, AIV_STATE_ATTACK, usr);
			} else if (st != 3) {
				CTF_Patrol(ai);
			}
		} else if (st == AIV_STATE_FLAG) {
			if (!World_FindEntity(g_CTFFlagName)) {
				CTF_Patrol(ai);
				g_CTFFlagName[0] = 0;
			}
		} else if (st == AIV_STATE_ATTACK) {
			if (strcmp(flag, LOGIC_EMPTY)) {
				CTF_SetFlagName(flag);
				cCTFEnemiesData_GoToFlag(self, flag, ai);
			}
		}
	} while ((n = Logic_ListNext(n)) != NULL);
}

cEntityListData *__fastcall cCTFEnemies_GetList(cCTFEnemies *self) {
	return self->d->list;
}

char *__fastcall cCTFEnemies_GetLeader(cCTFEnemies *self) {
	return self->d->leader;
}

/* the entity list with the hit callback and the logic of its ships */
cEntityListData *__fastcall cCTFEnemies_Init(cCTFEnemies *self) {
	cCTFEnemiesData *d = self->d;

	d->list = cWorld_NewEntityList(g_pWorld, "CTFEnemies");
	cPy_Function_SetNative(&d->list->onDamage, (nativefunc_t)GAME_FUNC(CTFEnemies_OnDamage));
	d->list->func = (int (*)(void))GAME_FUNC(CTFEnemies_MakeLogic);
	return d->list;
}

/* "IsLeader": 1 if the ship is the leader; set 1 makes it the leader, 0 of the leader none */
byte __thiscall cCTFEnemiesLocalLogic_VarI(cCTFEnemiesLocalLogic *self, const char *name, int *i, byte set) {
	if (strcmp(name, "IsLeader")) {
		return 0;
	}
	cEntity *e = Logic_AsCar(self->parent);
	cCTFEnemies *ctf = (&g_pWorld->logic.ctfEnemies);
	cEntity *leader = Logic_AsCar(World_FindEntity(ctf->d->leader));
	if (!set) {
		*i = e == leader;
	} else if (*i == 1) {
		cString_Set(&ctf->d->leader, e->name);
	} else if (e == leader) {
		cString_Set(&ctf->d->leader, LOGIC_EMPTY);
	}
	return 1;
}

/* the native of the hits (cPy_Function onDamage of the list): the ship and its attacker */
void CTFEnemies_OnDamage(cAbstractFile *params) {
	char name[0x80], attacker[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cAbstractFile_ReadString(params, attacker, 0x80);
	if (!name[0]) {
		return;
	}
	cEntity *v = World_FindEntity(name);
	if (!v || !v->vftable->IsType(v, TYEN_CAR)) {
		return;
	}
	CHECK(v->vftable->IsType(v, TYEN_CAR));
	cVehicleEntity *veh = (cVehicleEntity *)v;
	if (veh->wl.life >= (int)((float)veh->maxLife * 0.75f) || !attacker[0]) {
		return;
	}
	cEntity *a = World_FindEntity(attacker);
	if (!a || !a->vftable->IsType(a, TYEN_CAR)) {
		return;
	}
	CHECK(a->vftable->IsType(a, TYEN_CAR));
	cAIVehicle *ai = v->vftable->GetAI(v);
	CHECK(ai);
	cCTFEnemies *ctf = (&g_pWorld->logic.ctfEnemies);
	cEntity *leader = Logic_AsCar(World_FindEntity(ctf->d->leader));
	if (leader == v) {
		cEntity *usr = Logic_AsCar(cWorld_GetUsrEntity(g_pWorld, 0));
		if (usr && usr == a) {
			ctf->d->bonus = 10.0f;
		}
		return;
	}
	cAIVehicle_SetState(ai, AIV_STATE_ATTACK, a);
}

void __thiscall cCTFEnemies_SetBonus(cCTFEnemies *self, float t) {
	self->d->bonus = t;
}

void __thiscall cCTFEnemies_SetLeader(cCTFEnemies *self, const char *name) {
	cString_Set(&self->d->leader, name);
}

void __thiscall cCTFEnemies_Update(cCTFEnemies *self, float dt) {
	cCTFEnemiesData *d = self->d;
	float b = d->bonus;

	if (b > 0.0f) {
		d->bonus = b - Float_MinSS(b, dt);
	}
	float t = d->timer + dt;
	d->timer = t;
	if (!(0.5f >= t)) {
		d->timer = 0.0f;
		cCTFEnemiesData_Think(d);
	}
}

/* the logic of a ship of the list (cEntityListData.func) */
cLocalLogic *CTFEnemies_MakeLogic(cEntity *e) {
	CHECK(e && e->vftable->IsType(e, TYEN_CAR) && e->vftable->IsType(e, TYEN_CAR));
	cLocalLogic *l = operator_new(sizeof(*l));
	if (l) {
		l->parent = e;
		l->vftable = CCTFENEMIESLOCALLOGIC_VFTABLE;
		return l;
	}
	return NULL;
}
