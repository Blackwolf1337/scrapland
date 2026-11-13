/*
 * The abilities of the freeroam factions besides the Police (include/scrap/faction.h): the Bishops (heal, the green
 * ray, revive, the sanctuaries), the Bankers (steal, suck, loans, bribes), the Mercs (bounties, contracts, hired
 * ships, the heavy loadout), the Bureaucracy (bullet time in an area, bla-bla, permits, fines) and the Press (photos
 * of crimes, courier jobs). Instance side only. Not in the original.
 */
#include <math.h>
#include <string.h>

#include "scrap/ai.h"
#include "scrap/alarm.h"
#include "scrap/console.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/logic.h"
#include "scrap/netrep.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/scrapcollision.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/travel.h"
#include "scrap/vehicleattack.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weapons.h"
#include "scrap/world.h"

#include "game/App/FactionInt.h"

#define FD_CONE 0.6f				/* radians either side of the view: "in front" */
#define FD_HIRED 32					/* hired ships of all mercs */
#define FD_SLOWED 256				/* entities in bullet times */
#define FD_SLOW_EVERY 3				/* ticks between the looks of the bullet times */
#define FD_EVERY NETREP_TICK_HZ		/* ticks between the looks of jobs and hired ships */
#define FD_BOUNTY_REACH 3000.0f		/* units: a bounty without a revenge target goes on the nearest player */
#define FD_LAST_BY 300.0			/* s the last player that hurt one stays its revenge target */
#define FD_HIRE_FIGHT 30.0			/* s the hired ships go for the last player that hurt their merc */

typedef struct FdRef {
	cEntity *e;
	char name[FR_NAME];
} FdRef;

typedef struct FdHired {
	FdRef ship;
	int owner;
	uint16_t join;
	double until;
} FdHired;

static struct {
	FdHired hired[FD_HIRED];
	FdRef slowed[FD_SLOWED];
	int nslowed;
	vec3_t zonePos[FACTION_ZONES];
	int8_t zoneKnown[FACTION_ZONES];	/* 0 not looked, 1 found, -1 none */
} fd;

static void Fd_Ref(FdRef *r, cEntity *e) {
	r->e = e;
	Str_Copy(r->name, e ? e->name : "", sizeof(r->name));
}

/* the entity of a reference while it is the same one */
static cEntity *Fd_Get(const FdRef *r) {
	cEntity *e = r->e ? World_FindEntity(r->name) : NULL;
	return e && e == r->e && !e->deadNode.target ? e : NULL;
}

static const FactionTable *Fd_T(void) {
	return Faction_Table();
}

static cEntity *Fd_Char(int j) {
	cEntity *o = Players_Get(j);
	return o && NetPl_Kind(o) == NETREP_CHAR ? o : NULL;
}

/* the player that hurt player i last, recently, while it is in the level; -1 */
static int Fd_LastBy(int i, double secs) {
	FrPlayer *p = Fr_Player(i);
	int j = p->lastBy;
	if (j < 0 || j == i || !Players_Get(j) || Players_Joined(j) != p->lastByJoin || Fr_Now() - p->lastByTime > secs) {
		return -1;
	}
	return j;
}

/* a line of sight between the eyes of two entities */
static byte Fd_Line(cEntity *a, cEntity *b) {
	vec3_t pa, pb;
	return !Engine_TestLine(Obj_GetViewPos(a, &pa), Obj_GetViewPos(b, &pb), NULL, NULL);
}

/* ---- the sanctuaries ---- */

byte FrD_InSanctuary(cEntity *e) {
	const FactionTable *t = Fd_T();
	if (!e || !t) {
		return 0;
	}
	for (int z = 0; z < t->nzones; z++) {
		const FactionZone *zn = &t->zones[z];
		if (crt_stricmp(zn->level, Instance_Level())) {
			continue;
		}
		if (!strcmp(zn->dummy, "*")) {
			return 1;
		}
		if (!fd.zoneKnown[z]) {
			fd.zoneKnown[z] = (int8_t)(Fr_Dummy(zn->dummy, &fd.zonePos[z], NULL) ? 1 : -1);
		}
		if (fd.zoneKnown[z] > 0 && Fr_DistSq(e, &fd.zonePos[z]) <= zn->radius * zn->radius) {
			return 1;
		}
	}
	return 0;
}

/* ---- Bishops ---- */

static byte Fd_Wounded(int i, int j) {
	(void)i;
	cEntity *o = Fd_Char(j);
	return o && WLife(o)->life > 0 && WLife(o)->life < Fr_Player(j)->fullLife;
}

/* sell life: the nearest wounded player gets life, the healer the price */
static int Fd_Heal(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	int j = Fr_Nearest(i, e, t->reachFoot, 0.0f, Fd_Wounded);
	if (j < 0) {
		Fr_Notice(i, FT_NO_WOUNDED, 0, NULL, NULL);
		return 0;
	}
	cEntity *o = Players_Get(j);
	FrPlayer *q = Fr_Player(j);
	int add = q->fullLife - WLife(o)->life;
	add = add > t->healLife ? t->healLife : add;
	int price = t->healLife > 0 ? (t->healPrice * add + t->healLife - 1) / t->healLife : 0;
	if (Fr_Money(j) < price) {
		Fr_Notice(i, FT_NO_MONEY, 0, NULL, NULL);
		return 0;
	}
	WLife(o)->life += add;
	WLife(o)->prevLife = WLife(o)->life;
	Fr_AddMoney(j, -price);
	Fr_AddMoney(i, price);
	Fr_Fx(FFX_HEAL, e, o);
	Fr_Notice(i, FT_HEAL_GIVE, price, q->name, NULL);
	Fr_Notice(j, FT_HEAL_GOT, price, Fr_Player(i)->name, NULL);
	Fr_Deed(i);
	return 1;
}

/* the entity in front of e within reach: a player (that i may hurt), else a character or ship with life */
static cEntity *Fd_InFront(int i, cEntity *e, float reach) {
	cEntity *best = NULL;
	float bestD = reach * reach;
	float yaw = NetPl_Kind(e) == NETREP_CHAR ? WChar(e)->view.y : Obj(e)->ang.y;
	for (cEntity *o = cWorld_GetFirstEntity(g_pWorld); o; o = cEntity_Next(o)) {
		if (o == e || o->deadNode.target || !o->active || Obj(o)->proxy ||
			!(Entity_IsType(o, TYEN_WALKCHAR) || Entity_IsType(o, TYEN_CAR)) || WLife(o)->life <= 0 || WLife(o)->god) {
			continue;
		}
		float d = Fr_DistSq(o, &e->pos);
		float a = atan2f(-(o->pos.x - e->pos.x), -(o->pos.z - e->pos.z));
		if (d >= bestD || fabsf(NormAng(a - yaw)) > FD_CONE || !Fd_Line(e, o)) {
			continue;
		}
		int j = Players_Of(o);
		if (j >= 0 && !Fr_MayAffect(i, j, 0)) {
			continue;
		}
		bestD = d;
		best = o;
	}
	return best;
}

/* the green ray: damage to what is in front */
static int Fd_Ray(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	cEntity *o = Fd_InFront(i, e, t->rayReach);
	if (!o) {
		Fr_Notice(i, FT_RAY_MISS, 0, NULL, NULL);
		return 1;
	}
	if (Rep_Hurt(e, o, "Bishop")) {
		WLife(o)->life -= t->rayDamage;
		cVehicleAttack_HitBy(&WLife(o)->attack, e->name);
	}
	int j = Players_Of(o);
	Fr_Fx(FFX_RAY, e, o);
	Fr_Notice(i, FT_RAY_HIT, 0, j >= 0 ? Fr_Player(j)->name : o->name, NULL);
	return 1;
}

/* revive: the nearest dead player whose body is within reach comes back there (half its life); it pays the price */
static int Fd_Revive(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	int best = -1;
	float bestD = t->reviveReach * t->reviveReach;
	for (int j = 0; j < PLAYERS_MAX; j++) {
		FrPlayer *q = Fr_Player(j);
		if (j == i || !q->dead || Players_Get(j)) {
			continue;
		}
		float d = Fr_DistSq(e, &q->deadPos);
		if (d < bestD) {
			bestD = d;
			best = j;
		}
	}
	if (best < 0) {
		Fr_Notice(i, FT_NO_BODY, 0, NULL, NULL);
		return 0;
	}
	FrPlayer *q = Fr_Player(best);
	vec3_t at = q->deadPos;
	q->revived = 1;
	Fr_Rebody(best, &at, q->deadYaw);
	cEntity *o = Players_Get(best);
	if (!o) {
		q->revived = 0;
		Fr_Notice(i, FT_NO_BODY, 0, NULL, NULL);
		return 0;
	}
	int price = Fr_Money(best) < t->revivePrice ? (Fr_Money(best) > 0 ? Fr_Money(best) : 0) : t->revivePrice;
	Fr_AddMoney(best, -price);
	Fr_AddMoney(i, price);
	Fr_Fx(FFX_REVIVE, e, o);
	Fr_Notice(i, FT_REVIVE_GIVE, price, q->name, NULL);
	Fr_Notice(best, FT_REVIVE_GOT, 0, Fr_Player(i)->name, NULL);
	Fr_News(FN_REVIVE, 0, Fr_Player(i)->name, q->name);
	Fr_Deed(i);
	return 1;
}

/* ---- Bankers ---- */

static byte Fd_HasMoney(int i, int j) {
	(void)i;
	return Fr_Money(j) > 0;
}

static int Fd_Steal(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	int j = Fr_Nearest(i, e, t->stealReach, FD_CONE, Fd_HasMoney);
	if (j < 0) {
		Fr_Notice(i, FT_NO_ONE_NEAR, 0, NULL, NULL);
		return 0;
	}
	if (!Fr_MayAffect(i, j, 1)) {
		return 0;
	}
	int n = Fr_Money(j) < t->steal ? Fr_Money(j) : t->steal;
	Fr_AddMoney(j, -n);
	Fr_AddMoney(i, n);
	Fr_AddCrime(i, FCRIME_THEFT);
	Fr_Fx(FFX_STEAL, e, Players_Get(j));
	Fr_Notice(i, FT_STEAL_GIVE, n, Fr_Player(j)->name, NULL);
	Fr_Notice(j, FT_STEAL_GOT, n, Fr_Player(i)->name, NULL);
	Fr_News(FN_STEAL, 0, Fr_Player(i)->name, Fr_Player(j)->name);
	return 1;
}

/* the baton: money out of every player in front within reach (that the banker may rob) */
static int Fd_Suck(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	float yaw = NetPl_Kind(e) == NETREP_CHAR ? WChar(e)->view.y : Obj(e)->ang.y;
	int sum = 0;
	for (int j = 0; j < PLAYERS_MAX; j++) {
		cEntity *o = j != i ? Players_Get(j) : NULL;
		if (!o || Fr_Money(j) <= 0 || Fr_DistSq(o, &e->pos) > t->suckReach * t->suckReach) {
			continue;
		}
		float a = atan2f(-(o->pos.x - e->pos.x), -(o->pos.z - e->pos.z));
		if (fabsf(NormAng(a - yaw)) > FD_CONE || !Fr_MayAffect(i, j, 0)) {
			continue;
		}
		int n = Fr_Money(j) < t->suck ? Fr_Money(j) : t->suck;
		Fr_AddMoney(j, -n);
		Fr_AddMoney(i, n);
		Fr_Fx(FFX_SUCK, e, o);
		Fr_Notice(j, FT_STEAL_GOT, n, Fr_Player(i)->name, NULL);
		sum += n;
	}
	if (!sum) {
		Fr_Notice(i, FT_NO_ONE_NEAR, 0, NULL, NULL);
		return 0;
	}
	Fr_AddCrime(i, FCRIME_THEFT);
	Fr_Notice(i, FT_SUCK_GIVE, sum, NULL, NULL);
	return 1;
}

static byte Fd_NoDebt(int i, int j) {
	(void)i;
	return Fr_Player(j)->debt <= 0;
}

/* a loan of the bank to the nearest player without debt (else to the banker itself): money now, a debt its later
 * income pays; the banker's commission */
static int Fd_Loan(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	FrPlayer *p = Fr_Player(i);
	int j = Fr_Nearest(i, e, t->reachFoot, 0.0f, Fd_NoDebt);
	if (j < 0) {
		if (p->debt > 0) {
			Fr_Notice(i, FT_HAS_DEBT, 0, p->name, NULL);
			return 0;
		}
		j = i;
	}
	FrPlayer *q = Fr_Player(j);
	q->debt = (int)((float)t->loan * t->loanOwed + 0.5f);
	Fr_AddMoney(j, t->loan);
	/* not an income that pays the debt */
	q->lastMoney += t->loan;
	Fr_Notice(j, FT_LOAN_GOT, t->loan, NULL, NULL);
	if (j != i) {
		int c = (int)((float)t->loan * t->loanCommission + 0.5f);
		Fr_AddMoney(i, c);
		Fr_Fx(FFX_MONEY, e, Players_Get(j));
		Fr_Notice(i, FT_LOAN_GIVE, c, q->name, NULL);
		Fr_Deed(i);
	}
	Travel_Report(j);
	return 1;
}

static byte Fd_Officer(int i, int j) {
	(void)i;
	FrPlayer *q = Fr_Player(j);
	return q->faction == Faction_Find(Fd_T(), "Police") && q->level == 0;
}

/* a bribe: one wanted level less for money; the nearest officer gets it */
static int Fd_Bribe(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	FrPlayer *p = Fr_Player(i);
	if (p->level <= 0) {
		Fr_Notice(i, FT_NOT_WANTED, 0, NULL, NULL);
		return 0;
	}
	if (Fr_Money(i) < t->bribe) {
		Fr_Notice(i, FT_NO_MONEY, 0, NULL, NULL);
		return 0;
	}
	Fr_AddMoney(i, -t->bribe);
	p->level--;
	/* the middle of the level below (its start would decay below it at once) */
	p->wanted.points = p->level ? (FactionWanted_Points(t, p->level) + FactionWanted_Points(t, p->level + 1)) * 0.5f :
		0.0f;
	if (!p->level) {
		int prev = Players_Bound();
		Players_Bind(i);
		Alarm_Set(&g_pWorld->alarm, 0.0f);
		Players_Bind(prev);
	}
	Fr_Notice(i, FT_BRIBE, t->bribe, NULL, NULL);
	int j = Fr_Nearest(i, e, t->reachFoot * 2.0f, 0.0f, Fd_Officer);
	if (j >= 0) {
		Fr_AddMoney(j, t->bribe);
		Fr_Fx(FFX_MONEY, e, Players_Get(j));
		Fr_Notice(j, FT_BRIBE_GOT, t->bribe, p->name, NULL);
	}
	Fr_Deed(i);
	return 1;
}

/* the debts: a share of each income since the last tick */
static void Fd_Loans(void) {
	const FactionTable *t = Fd_T();
	for (int i = 0; i < PLAYERS_MAX; i++) {
		FrPlayer *p = Fr_Player(i);
		if (!Players_Get(i)) {
			p->moneyKnown = 0;
			continue;
		}
		int money = Fr_Money(i);
		if (p->moneyKnown && p->debt > 0) {
			int pay = FactionLoan_Repay(t, &p->debt, money - p->lastMoney);
			if (pay > 0) {
				Fr_AddMoney(i, -pay);
				money -= pay;
				Con_Printf("factions: player %d paid %d of its loan, %d left\n", i, pay, p->debt);
				if (!p->debt) {
					Fr_Notice(i, FT_DEBT_PAID, 0, NULL, NULL);
					Travel_Report(i);
				}
			}
		}
		p->lastMoney = money;
		p->moneyKnown = 1;
	}
}

/* ---- Mercs ---- */

static byte Fd_Other(int i, int j) {
	return i != j;
}

/* a bounty on the last player that hurt the placer, else on the nearest player; the placer pays it */
static int Fd_Bounty(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	int j = Fd_LastBy(i, FD_LAST_BY);
	if (j < 0) {
		j = Fr_Nearest(i, e, FD_BOUNTY_REACH, 0.0f, Fd_Other);
	}
	if (j < 0) {
		Fr_Notice(i, FT_NO_ONE_NEAR, 0, NULL, NULL);
		return 0;
	}
	if (Fr_Money(i) < t->bounty) {
		Fr_Notice(i, FT_NO_MONEY, 0, NULL, NULL);
		return 0;
	}
	FrPlayer *q = Fr_Player(j);
	int take = FactionBounty_Add(t, &q->bounty, &q->bountyEnds, t->bounty, Fr_Unix());
	if (take <= 0) {
		Fr_Notice(i, FT_BOUNTY_FULL, 0, NULL, NULL);
		return 0;
	}
	Fr_AddMoney(i, -take);
	Fr_Notice(i, FT_BOUNTY_SET, take, q->name, NULL);
	Fr_Notice(j, FT_BOUNTY_ON_YOU, q->bounty, NULL, NULL);
	Travel_Report(j);
	return 1;
}

byte FrD_Kill(int i, int j) {
	const FactionTable *t = Fd_T();
	FrPlayer *p = Fr_Player(i), *q = Fr_Player(j);
	if (!t || Faction_Cooldown(t, p->faction, FA_CONTRACTS) < 0.0f ||
		FactionBounty_Live(&q->bounty, &q->bountyEnds, Fr_Unix()) <= 0) {
		return 0;
	}
	int reward = q->bounty;
	q->bounty = 0;
	q->bountyEnds = 0;
	Fr_AddMoney(i, reward);
	Fr_Notice(i, FT_BOUNTY_CLAIM, reward, q->name, NULL);
	Fr_News(FN_BOUNTY, 0, p->name, q->name);
	Fr_Deed(i);
	Travel_Report(j);
	return 1;
}

/* the contracts: the bounties of the players of the level, the highest first (three) */
static int Fd_Contracts(int i) {
	byte told[PLAYERS_MAX] = { 0 };
	int n = 0;
	for (; n < 3; n++) {
		int best = -1;
		for (int j = 0; j < PLAYERS_MAX; j++) {
			FrPlayer *q = Fr_Player(j);
			if (j == i || told[j] || !Players_Get(j) || FactionBounty_Live(&q->bounty, &q->bountyEnds, Fr_Unix()) <= 0) {
				continue;
			}
			if (best < 0 || q->bounty > Fr_Player(best)->bounty) {
				best = j;
			}
		}
		if (best < 0) {
			break;
		}
		told[best] = 1;
		Fr_Notice(i, FT_CONTRACT, Fr_Player(best)->bounty, Fr_Player(best)->name, NULL);
	}
	if (!n) {
		Fr_Notice(i, FT_NO_CONTRACTS, 0, NULL, NULL);
	}
	return 1;
}

/* hired merc ships (outdoors): they follow the merc and go for the last player that hurt it */
static int Fd_Hire(int i) {
	const FactionTable *t = Fd_T();
	if (!Fr_Outdoors() || !t->hireShip[0]) {
		Fr_Notice(i, FT_ONLY_OUTDOORS, 0, NULL, NULL);
		return 0;
	}
	if (Fr_Money(i) < t->hirePrice) {
		Fr_Notice(i, FT_NO_MONEY, 0, NULL, NULL);
		return 0;
	}
	int made = 0;
	for (int k = 0; k < t->hireCount; k++) {
		int free = -1;
		for (int h = 0; h < FD_HIRED && free < 0; h++) {
			free = Fd_Get(&fd.hired[h].ship) ? -1 : h;
		}
		if (free < 0) {
			break;
		}
		nativeVal_t ship = { 0 }, name = { 0 };
		if (Mode_Import("VehiclesNPC") && PORT_CALL(VehiclesNPC, MakeAI, &ship, "ssi", t->hireShip, "HiredMerc", 0L) &&
			Mode_VGet(&ship, "Name", &name) && name.type == NV_STR) {
			cEntity *v = World_FindEntity(Native_Text(&name));
			if (v) {
				FdHired *h = &fd.hired[free];
				Fd_Ref(&h->ship, v);
				h->owner = i;
				h->join = Players_Joined(i);
				h->until = Fr_Now() + t->hireTime;
				made++;
			}
		} else {
			Native_ErrPrint();
		}
		Native_Free(&ship);
		Native_Free(&name);
	}
	if (!made) {
		Fr_Notice(i, FT_NO_SHIP, 0, NULL, NULL);
		return 0;
	}
	Fr_AddMoney(i, -t->hirePrice);
	Fr_Notice(i, FT_HIRED, made, NULL, NULL);
	Con_Printf("factions: player %d hired %d ships\n", i, made);
	return 1;
}

static void Fd_HiredTick(void) {
	double now = Fr_Now();
	for (int h = 0; h < FD_HIRED; h++) {
		FdHired *hd = &fd.hired[h];
		cEntity *v = Fd_Get(&hd->ship);
		if (!v) {
			hd->ship.e = NULL;
			continue;
		}
		cEntity *owner = Players_Get(hd->owner);
		if (!owner || Players_Joined(hd->owner) != hd->join || now >= hd->until) {
			/* the time is up or the merc is gone: the ship goes */
			if (owner && Players_Joined(hd->owner) == hd->join) {
				Fr_Notice(hd->owner, FT_HIRE_GONE, 0, NULL, NULL);
			}
			Con_Printf("factions: hired ship %s of player %d leaves\n", v->name, hd->owner);
			cEntity_Del(v);
			hd->ship.e = NULL;
			continue;
		}
		cAIVehicle *ai = Entity_GetAI(v);
		if (!ai) {
			continue;
		}
		WLife(v)->arrow.tgType = 2;
		int foe = Fd_LastBy(hd->owner, FD_HIRE_FIGHT);
		cEntity *target = foe >= 0 ? Players_Get(foe) : NULL;
		if (target) {
			if (cAIVehicle_GetState(ai) != AIV_STATE_ATTACK || AIMove_GetTargetEnt(ai) != target) {
				cAIVehicle_SetState(ai, AIV_STATE_ATTACK, target);
				Con_Printf("factions: hired ship %s of player %d goes for player %d\n", v->name, hd->owner, foe);
			}
		} else if (cAIVehicle_GetState(ai) != AIV_STATE_CHASE) {
			cAIVehicle_SetState(ai, AIV_STATE_CHASE, owner, (double)g_AIChaseDefault1, (double)g_AIChaseDefault2);
		}
	}
}

/* the heavy loadout: the ship's armour above its life and full ammo */
static int Fd_Heavy(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	if (NetPl_Kind(e) != NETREP_SHIP) {
		Fr_Notice(i, FT_ONLY_SHIP, 0, NULL, NULL);
		return 0;
	}
	cVehicleEntity *v = Veh(e);
	int life = (int)((float)(v->maxLife > 0 ? v->maxLife : v->wl.life) * t->heavyLife);
	if (v->wl.life < life) {
		v->wl.life = v->wl.prevLife = life;
	}
	for (int k = 0; k < g_pWorld->ammo.count; k++) {
		cWeaponManager_SetAmmo(&v->weapons, k, TABLE_AT(g_pWorld->ammo, k)->max);
	}
	Fr_Notice(i, FT_HEAVY, 0, NULL, NULL);
	return 1;
}

/* ---- Bureaucracy ---- */

static int Fd_Slow(int i, cEntity *e) {
	FrPlayer *p = Fr_Player(i);
	p->slowUntil = Fr_Now() + Fd_T()->slowTime;
	Fr_Fx(FFX_SLOW, e, NULL);
	Fr_Notice(i, FT_SLOW_ON, 0, NULL, NULL);
	return 1;
}

static byte Fd_Slowed(const cEntity *e) {
	for (int k = 0; k < fd.nslowed; k++) {
		if (fd.slowed[k].e == e) {
			return 1;
		}
	}
	return 0;
}

/* the entities within the radius of a running bullet time run at its speed (not the functionary; players only when
 * it may act against them), the ones that left it at their own again */
static void Fd_SlowTick(void) {
	const FactionTable *t = Fd_T();
	static FdRef now[FD_SLOWED];
	int n = 0;
	double tnow = Fr_Now();
	for (int i = 0; i < PLAYERS_MAX; i++) {
		Fr_Player(i)->slowed = 0;
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *c = Players_Get(i);
		if (!c || Fr_Player(i)->slowUntil <= tnow) {
			continue;
		}
		for (cEntity *o = cWorld_GetFirstEntity(g_pWorld); o && n < FD_SLOWED; o = cEntity_Next(o)) {
			if (o == c || o->deadNode.target || Obj(o)->proxy ||
				!(Entity_IsType(o, TYEN_WALKCHAR) || Entity_IsType(o, TYEN_CAR)) ||
				Fr_DistSq(o, &c->pos) > t->slowRadius * t->slowRadius) {
				continue;
			}
			int j = Players_Of(o);
			if (j >= 0 && !Fr_MayAffect(i, j, 0)) {
				continue;
			}
			byte dup = 0;
			for (int k = 0; k < n && !dup; k++) {
				dup = now[k].e == o;
			}
			if (!dup) {
				Fd_Ref(&now[n++], o);
			}
			if (j >= 0) {
				Fr_Player(j)->slowed = 1;
			}
		}
	}
	/* the ones that left: their own speed again */
	for (int k = 0; k < fd.nslowed; k++) {
		cEntity *o = Fd_Get(&fd.slowed[k]);
		byte still = 0;
		for (int m = 0; m < n && !still; m++) {
			still = now[m].e == fd.slowed[k].e;
		}
		if (o && !still) {
			Timer_SetSpeed(&Obj(o)->timer, 1.0f);
		}
	}
	for (int k = 0; k < n; k++) {
		if (!Fd_Slowed(now[k].e)) {
			Con_Printf("factions: %s in a bullet time\n", now[k].name);
		}
		Timer_SetSpeed(&Obj(now[k].e)->timer, t->slowSpeed);
	}
	memcpy(fd.slowed, now, sizeof(now[0]) * (size_t)n);
	fd.nslowed = n;
}

/* bla-bla: the players around fall asleep for a while, the characters of the level too */
static int Fd_Blabla(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	int n = 0;
	double until = Fr_Now() + t->blablaTime;
	for (cEntity *o = cWorld_GetFirstEntity(g_pWorld); o; o = cEntity_Next(o)) {
		if (o == e || o->deadNode.target || Obj(o)->proxy || !Entity_IsType(o, TYEN_WALKCHAR) ||
			WLife(o)->life <= 0 || Fr_DistSq(o, &e->pos) > t->blablaRadius * t->blablaRadius) {
			continue;
		}
		int j = Players_Of(o);
		cActCtrl *ctrl = WLife(o)->actCtrl;
		if (j >= 0) {
			if (!Fr_MayAffect(i, j, 0)) {
				continue;
			}
			Fr_Player(j)->asleepUntil = until;
			Fr_Notice(j, FT_ASLEEP, 0, NULL, NULL);
		} else if (!Logic_GetLocalLogic(o)) {
			continue;
		}
		if (ctrl && WithLife_GetAction(o, "Dazed")) {
			cActCtrl_SetDefLoop(ctrl, "Dazed", 1);
			cActCtrl_Play(ctrl, "DazedInit", 1, 1.0f);
		}
		n++;
	}
	Fr_Notice(i, FT_BLABLA, n, NULL, NULL);
	return 1;
}

static void Fd_SleepTick(void) {
	double now = Fr_Now();
	for (int i = 0; i < PLAYERS_MAX; i++) {
		FrPlayer *p = Fr_Player(i);
		byte asleep = p->asleepUntil > now && Fd_Char(i) != NULL;
		if (p->asleep && !asleep && Fd_Char(i)) {
			/* awake again */
			cActCtrl *ctrl = WLife(Fd_Char(i))->actCtrl;
			if (ctrl) {
				cActCtrl_SetDefLoop(ctrl, "Relax", 1);
				cActCtrl_Play(ctrl, "DazedEnd", 1, 1.0f);
			}
		}
		p->asleep = asleep;
	}
}

static byte Fd_NoPermit(int i, int j) {
	(void)i;
	return !Fr_Player(j)->permit;
}

/* a ship permit for the nearest player without one; it pays the functionary */
static int Fd_Permit(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	int j = Fr_Nearest(i, e, t->reachFoot, 0.0f, Fd_NoPermit);
	if (j < 0) {
		Fr_Notice(i, FT_HAS_PERMIT, 0, NULL, NULL);
		return 0;
	}
	if (Fr_Money(j) < t->permitPrice) {
		Fr_Notice(i, FT_NO_MONEY, 0, NULL, NULL);
		return 0;
	}
	FrPlayer *q = Fr_Player(j);
	Fr_AddMoney(j, -t->permitPrice);
	Fr_AddMoney(i, t->permitPrice);
	q->permit = 1;
	Fr_Fx(FFX_MONEY, Players_Get(j), e);
	Fr_Notice(i, FT_PERMIT_GIVE, t->permitPrice, q->name, NULL);
	Fr_Notice(j, FT_PERMIT_GOT, 0, NULL, NULL);
	Fr_Deed(i);
	Travel_Report(j);
	return 1;
}

/* a wanted player, or one in a ship of a rank that needs a permit without one */
static byte Fd_Offender(int i, int j) {
	(void)i;
	const FactionTable *t = Fd_T();
	FrPlayer *q = Fr_Player(j);
	cEntity *o = Players_Get(j);
	return q->level > 0 || (o && NetPl_Kind(o) == NETREP_SHIP && q->faction >= 0 && !q->permit &&
		q->faction != Faction_Find(t, "Bureaucracy") && Faction_NeedsPermit(t, Fr_Rank(q)));
}

static int Fd_Fine(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	int j = Fr_Nearest(i, e, NetPl_Kind(e) == NETREP_SHIP ? t->reachShip : t->reachFoot * 2.0f, 0.0f, Fd_Offender);
	if (j < 0) {
		Fr_Notice(i, FT_NO_OFFENDER, 0, NULL, NULL);
		return 0;
	}
	FrPlayer *q = Fr_Player(j);
	int fine = t->permitFine * (q->level > 1 ? q->level : 1);
	fine = Fr_Money(j) < fine ? (Fr_Money(j) > 0 ? Fr_Money(j) : 0) : fine;
	Fr_AddMoney(j, -fine);
	/* half to the functionary, the rest to the town */
	Fr_AddMoney(i, fine / 2);
	Fr_Fx(FFX_MONEY, Players_Get(j), e);
	Fr_Notice(i, FT_FINE_GIVE, fine, q->name, NULL);
	Fr_Notice(j, FT_FINE_GOT, fine, NULL, NULL);
	Fr_Deed(i);
	return 1;
}

/* ---- Press ---- */

/* a player with a crime of the last photoage seconds in sight */
static byte Fd_Fresh(int i, int j) {
	FrPlayer *q = Fr_Player(j);
	cEntity *a = Players_Get(i), *b = Players_Get(j);
	return q->lastCrime >= 0.0 && Fr_Now() - q->lastCrime <= Fd_T()->photoAge && a && b && Fd_Line(a, b);
}

static int Fd_Photo(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	int j = Fr_Nearest(i, e, t->photoReach, FD_CONE, Fd_Fresh);
	if (j < 0) {
		Fr_Notice(i, FT_NO_PHOTO, 0, NULL, NULL);
		return 1;
	}
	FrPlayer *q = Fr_Player(j);
	float top = FactionWanted_Points(t, FACTION_LEVELS) * 1.5f;
	q->wanted.points = q->wanted.points + t->photoPoints > top ? top : q->wanted.points + t->photoPoints;
	q->lastCrime = -1.0;
	Fr_AddMoney(i, t->photoReward);
	Fr_Fx(FFX_PHOTO, e, Players_Get(j));
	Fr_Notice(i, FT_PHOTO_GIVE, t->photoReward, q->name, NULL);
	Fr_Notice(j, FT_PHOTO_GOT, 0, NULL, NULL);
	Fr_News(FN_PHOTO, 0, q->name, Fr_Player(i)->name);
	Fr_Deed(i);
	return 1;
}

/* the courier job of player i is done at its target (1), late (cleared), or goes on */
static byte Fd_JobCheck(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	FrPlayer *p = Fr_Player(i);
	vec3_t at;
	if (!p->jobLevel[0]) {
		return 0;
	}
	int64_t now = Fr_Unix();
	if (now > p->jobEnds) {
		Fr_Notice(i, FT_JOB_LATE, 0, NULL, NULL);
		p->jobLevel[0] = 0;
		Travel_Report(i);
		return 0;
	}
	if (crt_stricmp(p->jobLevel, Instance_Level()) || !e || !Fr_Dummy(p->jobDummy, &at, NULL) ||
		Fr_DistSq(e, &at) > t->deskReach * t->deskReach) {
		return 0;
	}
	int secs = (int)(now - p->jobStart);
	int reward = Faction_CourierReward(t, secs);
	Fr_AddMoney(i, reward);
	Fr_Notice(i, FT_JOB_DONE, reward, NULL, NULL);
	Fr_News(FN_DELIVERY, secs, p->name, NULL);
	p->jobLevel[0] = 0;
	Fr_Deed(i);
	return 1;
}

/* a job at the Press desk: a data pack to the desk of another office in time */
static int Fd_Courier(int i, cEntity *e) {
	const FactionTable *t = Fd_T();
	FrPlayer *p = Fr_Player(i);
	if (p->jobLevel[0]) {
		if (Fd_JobCheck(i, e)) {
			return 1;
		}
		if (p->jobLevel[0]) {
			Fr_Notice(i, FT_JOB_HAVE, 0, p->jobLevel, NULL);
		}
		return 0;
	}
	if (!Fr_AtDesk(p->faction, e)) {
		Fr_Notice(i, FT_JOB_AT_DESK, 0, NULL, NULL);
		return 0;
	}
	int r = Faction_Route(t, Instance_Level(), (uint32_t)Coord_UnixMs() ^ (uint32_t)(i * 2654435761u));
	if (r < 0) {
		Fr_Notice(i, FT_NO_ONE_NEAR, 0, NULL, NULL);
		return 0;
	}
	Str_Copy(p->jobLevel, t->routes[r].level, sizeof(p->jobLevel));
	Str_Copy(p->jobDummy, t->routes[r].dummy, sizeof(p->jobDummy));
	p->jobStart = Fr_Unix();
	p->jobEnds = p->jobStart + t->courierTime;
	Fr_Notice(i, FT_JOB, t->courierTime, p->jobLevel, NULL);
	Travel_Report(i);
	return 1;
}

/* ---- the dispatch ---- */

int FrD_Request(int i, cEntity *e, int a) {
	switch (a) {
	case FA_HEAL:
		return Fd_Heal(i, e);
	case FA_RAY:
		return Fd_Ray(i, e);
	case FA_REVIVE:
		return Fd_Revive(i, e);
	case FA_STEAL:
		return Fd_Steal(i, e);
	case FA_SUCK:
		return Fd_Suck(i, e);
	case FA_LOAN:
		return Fd_Loan(i, e);
	case FA_BRIBE:
		return Fd_Bribe(i, e);
	case FA_BOUNTY:
		return Fd_Bounty(i, e);
	case FA_CONTRACTS:
		return Fd_Contracts(i);
	case FA_HIRE:
		return Fd_Hire(i);
	case FA_HEAVY:
		return Fd_Heavy(i, e);
	case FA_SLOW:
		return Fd_Slow(i, e);
	case FA_BLABLA:
		return Fd_Blabla(i, e);
	case FA_PERMIT:
		return Fd_Permit(i, e);
	case FA_FINE:
		return Fd_Fine(i, e);
	case FA_PHOTO:
		return Fd_Photo(i, e);
	case FA_COURIER:
		return Fd_Courier(i, e);
	default:
		return -1;
	}
}

void FrD_Tick(uint64_t tick) {
	Fd_Loans();
	Fd_SleepTick();
	if (tick % FD_SLOW_EVERY == 0) {
		Fd_SlowTick();
	}
	if (tick % FD_EVERY == 0) {
		Fd_HiredTick();
		for (int i = 0; i < PLAYERS_MAX; i++) {
			if (Players_Get(i)) {
				Fd_JobCheck(i, Players_Get(i));
			}
		}
	}
}

void FrD_Gone(int i) {
	FrPlayer *p = Fr_Player(i);
	p->slowUntil = 0.0;
	p->asleepUntil = 0.0;
	for (int h = 0; h < FD_HIRED; h++) {
		if (fd.hired[h].ship.e && fd.hired[h].owner == i) {
			fd.hired[h].until = 0.0;
		}
	}
}

uint8_t FrD_Flags(int i) {
	FrPlayer *p = Fr_Player(i);
	return (uint8_t)((p->slowed ? NETREP_FL_SLOW : 0) | (p->asleep ? NETREP_FL_ASLEEP : 0));
}
