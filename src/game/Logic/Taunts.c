/*
 * The taunts of the outdoor maps (Logic/, the part between NurseLogic.cpp and Parking.cpp; include/scrap/logic.h).
 * Every TimeBetweeenTaunts a ship that attacks the player in its car taunts it through the script function OnTaunt
 * (ship name, race of the pilot, kind): a random police car with the alarm on, else a random ship of the traffic
 * or the enemies, or the target of the player while the boss does not control the robots. While it does, the
 * police taunts go by the race of the ship (3, 4, 6 in turn per race, not twice the same race, TimeBetweeenTaunts2
 * apart). The warnings tell the player of a phone cab near it while the alarm is on ("DeepThroat").
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/crt.h"
#include "scrap/filepak.h"
#include "scrap/logic.h"
#include "scrap/nodestraffic.h"
#include "scrap/objentity.h"
#include "scrap/scrap.h"
#include "scrap/scrapmodel.h"
#include "scrap/timer.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"

/* the statics of the police taunts: the count per race, the races, the last race and the time */
#define s_TauntRaceCounts (GAME_ARRAY(HashTable, s_TauntRaceCounts))	/* of cNameItem, 0x1c9 slots, case sensitive */
extern char * s_TauntRaces[5];		/* "SBi", "SBa", "SFu", "SPo" */
#define s_TauntLastRace (GAME_ARRAY(char *, s_TauntRaces)[4])
extern int s_TauntPoliceCount;		/* -1 at the start */
extern float s_TauntLastTime;
#define s_TauntCountsGuard (&GAME_GLOBAL(int, s_TauntCountsGuard))
#define s_TauntTimeGuard (&GAME_GLOBAL(int, s_TauntTimeGuard))

/* the atexit destructor of the count table */
void Taunts_FiniCounts(void) {
	NameTable_Clear(s_TauntRaceCounts);
}

/* a random index below n (rand01 * (n - 1) rounded half up), the node that far into the ring */
static int Taunts_RandIndex(int n) {
	float f = (float)(Weap_Rand01() * (double)(float)(n - 1) + 0.0);
	int i = (int)f;
	int r = (int)((double)f + 0.5);

	return r > i ? r : i;
}

cEntity *Logic_RandomCar(cEntityListData *list) {
	if (!list) {
		return NULL;
	}
	Nto1Node *n = Logic_ListFirst(list);
	int count = Logic_Count(list->numElems);
	if (count == 0) {
		return NULL;
	}
	if (count != 1) {
		for (int i = Taunts_RandIndex(count); i; i--) {
			n = Logic_ListNext(n);
		}
	}
	return Logic_AsCar(n->data);
}

/* the kind of the police taunt of the ship (3, 4, 6 by the count of its race), -1 for none now */
int Taunts_PoliceKind(cEntity *ship) {
	int epoch = crt_InitEpoch();

	if (*s_TauntCountsGuard > epoch) {
		crt_Init_thread_header(s_TauntCountsGuard);
		if (*s_TauntCountsGuard == -1) {
			s_TauntRaceCounts->slots = New_Array(0x1c9, sizeof(*s_TauntRaceCounts->slots));
			memset(s_TauntRaceCounts->slots, 0, 0x1c9 * sizeof(*s_TauntRaceCounts->slots));
			s_TauntRaceCounts->locked = 0;
			s_TauntRaceCounts->caseSensitive = 1;
			crt_atexit(Taunts_FiniCounts);
			crt_Init_thread_footer(s_TauntCountsGuard);
		}
	}
	if (*s_TauntTimeGuard > epoch) {
		crt_Init_thread_header(s_TauntTimeGuard);
		if (*s_TauntTimeGuard == -1) {
			s_TauntLastTime = Timer_GetTime(&g_pWorld->timer);
			crt_Init_thread_footer(s_TauntTimeGuard);
		}
	}
	const char *race = CGameModel_Name(Obj_Vtbl(ship)->GetModel(ship));
	if (!(0.0f >= g_RemainTalkTime) || !strncmp(s_TauntLastRace, race, 3)) {
		return -1;
	}
	float now = Timer_GetTime(&g_pWorld->timer);
	if (!(now - s_TauntLastTime >= g_TimeBetweeenTaunts2)) {
		return -1;
	}
	s_TauntLastTime = now;
	int old = 0;
	cNameItem *it = NameTable_Find(s_TauntRaceCounts, race, HashTable_Slot(s_TauntRaceCounts, race));
	if (it) {
		old = it->value;
		int v = (old + 1) % 3;
		NameTable_Set(s_TauntRaceCounts, race, &v);
	} else {
		int v = 1;
		NameTable_Add(s_TauntRaceCounts, race, &v, 0);
	}
	for (int i = 0; i < 4; i++) {
		if (!strncmp(s_TauntRaces[i], race, 3)) {
			s_TauntLastRace = s_TauntRaces[i];
			break;
		}
	}
	if (!old) {
		return 3;
	}
	return old != 1 ? 6 : 4;
}

/* kind 8 plays only every second time, and only for ships with LaunchTaunts the other kinds */
static byte Taunts_Allowed(cEntity *ship, int kind) {
	if (!Veh_Vtbl(Veh(ship))->GetLaunchTaunts(Veh(ship))) {
		if (kind != 8) {
			return 0;
		}
	} else if (kind != 8) {
		return 1;
	}
	return !((float)(Weap_Rand01() + 0.0) > 0.5f);
}

void Taunts_Ship(cEntity *ship, int kind) {
	if (!ship || !Taunts_Allowed(ship, kind)) {
		return;
	}
	const char *pilot = Veh_Vtbl(Veh(ship))->GetPilot(Veh(ship), 0);
	if (pilot && strcmp(pilot, "")) {
		Taunts_Play(ship->name, pilot + 2, kind);
	}
}

void Taunts_Play(const char *ship, const char *race, int kind) {
	cEntity *e = World_FindEntity(ship);

	if (e && e->vftable->IsType(e, TYEN_CAR)) {
		if (!Taunts_Allowed(e, kind)) {
			return;
		}
		const char *list = cEntity_GetListName(e);
		if (list && !strcmp(list, "Friends")) {
			return;
		}
		const char *boss = SaveVar_Get("RobotsControlledByBoss");
		if (boss && !strcmp(boss, "1") && kind != 8 && kind != 3 && kind != 4) {
			kind = Taunts_PoliceKind(e);
			if (kind == -1) {
				return;
			}
		}
	} else if (kind != 8 && strcmp(ship, "")) {
		return;
	}
	if (!(byte)cPy_Function_IsValid(g_OnTaunt)) {
		return;
	}
	cPy_Function_AddParamS(g_OnTaunt, ship);
	cPy_Function_AddParamS(g_OnTaunt, race);
	cPy_Function_AddParamI(g_OnTaunt, kind);
	cPy_Function_Execute(g_OnTaunt, 0);
	Taunts_UpdateEndTime(&g_pWorld->logic);
}

/* the ship that attacks usr taunts it by its AI state; the police has its own kinds */
void Taunts_Attacker(cEntity *usr, cEntity *ship) {
	if (!usr || !ship) {
		return;
	}
	cAIVehicle *ai = ship->vftable->GetAI(ship);
	if (!ai) {
		return;
	}
	cEntity *t = Logic_AttackTarget(ai);
	if (!t || !t->vftable->IsType(t, TYEN_CAR) || usr != t) {
		return;
	}
	byte police = !strcmp(Veh_Vtbl(Veh(ship))->GetPilot(Veh(ship), 0) + 2, "Police");
	int st = cAIVehicle_GetInternState(ai);
	if (st == 5 || st == 4) {
		Taunts_Ship(ship, police ? 9 : 5);
		return;
	}
	if (cAIVehicle_ControlFlag(ai)) {
		if (police) {
			Taunts_Ship(ship, crt_rand() % 4 + 4);
			return;
		}
		s_TauntPoliceCount = (s_TauntPoliceCount + 1) % 3;
		Taunts_Ship(ship, s_TauntPoliceCount + 2);
		return;
	}
	if (!police) {
		Taunts_Ship(ship, 6);
		return;
	}
	Taunts_Ship(ship, crt_rand() % 2 ? 8 : 3);
}

void Taunts_Think(float dt) {
	cEntity *usr = ChType_Usr0();
	cEntity *target = NULL;

	(void)dt;
	if (usr && !usr->vftable->IsType(usr, TYEN_CAR)) {
		usr = NULL;
	}
	const char *boss = SaveVar_Get("RobotsControlledByBoss");
	if (boss && strcmp(boss, "1") && usr) {
		if (!usr->vftable->IsType(usr, TYEN_CAR)) {
			usr = NULL;
		} else {
			target = Logic_FindCar(Veh(usr)->target);
		}
	}
	if (target) {
		Taunts_Attacker(usr, target);
		return;
	}
	int r = (int)(float)(Weap_Rand01() * (double)2.97f + 0.0);
	cLogicControl *lc = (&g_pWorld->logic);
	cEntityListData *traffic = lc->enemiesTraffic.enemies;
	cEntityListData *enemies = lc->enemies.list;
	cEntityListData *list;
	int n = 1;
	switch (r) {
	case 0:
		if (g_pWorld->alarm.on) {
			list = lc->extPolice.list;
			break;
		}
		/* fall through */
	case 1:
		if (traffic->numElems && (n = *traffic->numElems) > 0) {
			list = traffic;
			break;
		}
		/* fall through */
	case 2:
		if (!enemies->numElems || (n = *enemies->numElems) <= 0) {
			return;
		}
		list = enemies;
		break;
	default:
		return;
	}
	if (!list) {
		return;
	}
	int k = crt_rand() % n;
	Nto1Node *node = Logic_ListFirst(list);
	/* the original walks the ring without counting k down: only k 0 takes a ship */
	if (!node || k) {
		return;
	}
	Taunts_Attacker(usr, Logic_AsCar(node->data));
}

byte Taunts_Warnings(float dt) {
	vec3_t t1, t2;

	(void)dt;
	if (!g_pWorld->alarm.on) {
		return 0;
	}
	cEntityListData *cabs = (cEntityListData *)cWorld_GetList(g_pWorld, "PhoneCabs");
	if (!cabs) {
		return 0;
	}
	cEntity *usr = cWorld_GetUsrEntity(g_pWorld, 0);
	if (!usr) {
		return 0;
	}
	for (Nto1Node *n = Logic_ListFirst(cabs); n; n = Logic_ListNext(n)) {
		cEntity *cab = n->data;
		if (!cab || Obj(cab)->radarShow != 1) {
			continue;
		}
		const vec3_t *up = usr->vftable->GetPos(usr, &t1);
		const vec3_t *cp = cab->vftable->GetPos(cab, &t2);
		float dx = cp->x - up->x;
		float dy = cp->y - up->y;
		float dz = cp->z - up->z;
		float d = g_PhoneCabWarningDist;
		if (d * d >= dy * dy + dx * dx + dz * dz) {
			if ((byte)cPy_Function_IsValid(g_OnTaunt)) {
				cPy_Function_AddParamS(g_OnTaunt, LOGIC_EMPTY);
				cPy_Function_AddParamS(g_OnTaunt, "DeepThroat");
				cPy_Function_AddParamI(g_OnTaunt, 1);
				cPy_Function_Execute(g_OnTaunt, 0);
				Taunts_UpdateEndTime(&g_pWorld->logic);
			}
			return 1;
		}
	}
	return 0;
}
