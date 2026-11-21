/*
 * The automatic test player of the coop story (the test lines of an instance: auto, fight, walk, possess, travel,
 * leader, story ...; docs/testing.md): it plays a member's input on the instance. Not in the original.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/ai.h"
#include "scrap/aicharcontrol.h"
#include "scrap/alarm.h"
#include "scrap/aivehicle.h"
#include "scrap/camcontrol.h"
#include "scrap/chartype.h"
#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/faction.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/graphic2d.h"
#include "scrap/input.h"
#include "scrap/instance.h"
#include "scrap/itementity.h"
#include "scrap/missionmaker.h"
#include "scrap/netrep.h"
#include "scrap/netscene.h"
#include "scrap/party.h"
#include "scrap/nodesworld.h"
#include "scrap/nodesworld2d.h"
#include "scrap/places.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/repent.h"
#include "scrap/scorer.h"
#include "scrap/scorerhud.h"
#include "scrap/scrap.h"
#include "scrap/pyfunction.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/scrapcollision.h"
#include "scrap/sound.h"
#include "scrap/sputniks.h"
#include "scrap/text.h"
#include "scrap/texts.h"
#include "scrap/travel.h"
#include "scrap/vehicleattack.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/withlifeentity.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxweapons.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"
#include "game/Script/levels/Levels.h"
#include "game/App/CoopInt.h"

#define CP_CHECKPOINT "CheckPoint"	/* the checkpoint of a race of RaceMaker */
#define CP_RACE_GAP 2.5				/* seconds: test: the race's ship at the next checkpoint after this */
#define CP_FIGHT_FAR 6000.0f		/* test: a fighter further from its target is put CP_FIGHT_NEAR from it */
#define CP_FIGHT_NEAR 2500.0f
#define CP_FOLLOW_NEAR 1000.0f		/* test: a ship it stays with (no fight) is kept this near */
#define CP_FOLLOW_FAR 2000.0f		/* test: and put back that near when it is further */
#define CP_ESCORT 6000.0f			/* test: the red ones this near a ship it stays with are fought first */
#define CP_FIGHT_HIT 3000.0f		/* test: a target this near is fought with the instant-hit weapons first */
#define CP_FIGHT_LOCK 0.4			/* test: s a homing missile's lock is on the target before the release */
#define CP_FIGHT_LOCKCONE 0.5f		/* test: radians: it holds a homing missile while the target is this near ahead */
#define CP_FIGHT_SHOT 8000.0f		/* test: units a second of a shot, to lead a moving target */
#define CP_FIGHT_TURN 0.25f			/* test: radians: a ship turns at full speed to a target further aside */
#define CP_FIGHT_SIZE 250.0f		/* test: it fires when its aim is this near the target's middle */
#define CP_FIGHT_RANGE 8000.0f		/* test: it fires at a target this near and ahead */
#define CP_FIGHT_MAX 64				/* test: the targets looked at */
#define CP_AMMO_GAP 4.0				/* test: s between two ammo boxes */
#define CP_WORLD_EDGE 200.0f		/* test: units a placed player keeps from the box of the level's grid of points */
#define CP_LIFE_LOW 35				/* test: percent of its most life under which a ship takes a life box */
#define CP_FIGHT_USELESS 10.0		/* test: s a weapon fires at a target near ahead that loses no life: not taken again */
#define CP_FIGHT_RED "red"			/* test: the fight list of all the red ones (or enemies) of the level */
#define CP_POSSESS_SECS 90.0		/* test: a possession is given up after this */
#define CP_AUTO_GAP 8.0				/* test: the automatic player goes to its target again after this */
#define CP_AUTO_NEAR 250.0f			/* test: units from a target it uses */
#define CP_AUTO_OUT 900.0f			/* test: its ship flies this near a target on foot before it gets out */
#define CP_FLY_GAP 0.5				/* test: s between two looks at a flight's way */
#define CP_FLY_NEAR 400.0f			/* test: a point of its flight's path this near is passed */
#define CP_FLY_STUCK 8.0			/* test: s, else it is placed CP_FIGHT_NEAR from its goal (as before) */
#define CP_AUTO_SHIP 8000.0f		/* test: outdoors a target on foot further than this is flown to */
#define CP_USE_BOARD 120.0			/* test: s after a use before it boards for a mission that starts so */
#define CP_USE_HOLD 5.0				/* test: s it waits after a use (the talk of a call starts after it) */
#define CP_SHIP_ITEM 1000.0f		/* test: outdoors an item this wide is a ship's (flown onto) */
#define CP_SHIP_ASK 12.0			/* test: s it asks for a ship (the switch) before it walks */
#define CP_MSG_CARRY 600.0f			/* test: a messenger farther from its pack's goal is put next to it */
#define CP_MSG_NEAR 700.0f			/* test: a messenger this near the pack's carrier is shot first */
#define CP_MSG_SHOT 1500.0f			/* test: a messenger fires its lightning at a messenger this near (it reaches 2000) */
#define CP_AIR 1500.0f				/* test: an item this high above the floor is flown onto */
#define CP_WALK_FAR 300.0f			/* test: a walker walks to a target further than this (GoWithHuman: the switch up a step) */
#define CP_WALK_STEP 300.0f			/* test: a walk comes a quarter of this further within CP_WALK_STUCK */
#define CP_WALK_GAP 0.5				/* test: s between two looks at its way and its followers */
#define CP_WALK_NEAR 60.0f			/* test: a point of its path this near is passed */
#define CP_WALK_WAIT 900.0f			/* test: a walker waits for a character this near (one that follows it) */
#define CP_WALK_ALONE 15.0			/* test: s it waits for one at most */
#define CP_WALK_LEASH 1500.0f		/* test: followers further than this are none (BeatPickus' thugs stop there) */
#define CP_WALK_STUCK 4.0			/* test: s without coming CP_WALK_STEP / 4 further: placed at its path's point */
#define CP_WALK_END 600.0f			/* test: a walk that comes no nearer within this ends (the goto's last step) */
#define CP_MELEE 90.0f				/* test: units from a character it fights on foot (D-Tritus' punch: 50 ahead, 100 wide) */
#define CP_MELEE_FAR 600.0f			/* test: a character further is walked to (put next to it) */


/* ---- tests ---- */

/* test: player i's ship along the race route, checkpoint by checkpoint: CP_RACE_GAP seconds after RaceMaker's
 * checkpoint came to a place (it moves on when taken) the ship is put there, as a pilot that knows the route (the
 * streets of a level have no straight line between two checkpoints); the race's own take of the checkpoint, its
 * count down and its score do the rest */
static void Cp_TestRace(int i) {
	cEntity *e = Players_Get(i), *c = World_FindEntity(CP_CHECKPOINT);
	if (!e || !c || NetPl_Kind(e) != NETREP_SHIP) {
		return;
	}
	vec3_t *at = &g_Coop.pl[i].raceAt;
	double now = Instance_Time();
	if (at->x != c->pos.x || at->y != c->pos.y || at->z != c->pos.z) {
		*at = c->pos;
		g_Coop.pl[i].raceSince = now;
		Con_Printf("coop: player %d: race checkpoint at %.0f %.0f %.0f\n", i, (double)at->x, (double)at->y, (double)at->z);
	}
	if (now - g_Coop.pl[i].raceSince < CP_RACE_GAP) {
		return;
	}
	g_Coop.pl[i].raceSince = now;
	e->pos = c->pos;
	Obj(e)->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
	/* the placement goes to its client too (its own ship's step there would take it back: MortalRace's ship stood
	 * short of its checkpoint) */
	Rep_Placed(i);
}

/* test: a target of the mission to fight (its radar mark red, or in the scripts' list of enemies) */
static int Cp_Foe(cEntity *e) {
	char *tg = NULL, *list = NULL;
	return (Entity_VarS(e, "TgType", &tg, 0) && tg && (!crt_stricmp(tg, "red") || !crt_stricmp(tg, "killer"))) ||
		(Entity_VarS(e, "EntityList", &list, 0) && list && !strcmp(list, "Enemies"));
}

/* test: a character of the mission while the mission's death hook of the characters waits (CharAct.onNPCDeath:
 * ApologizeMayor's staplers, CatchTrurl's functionaries, FicusBanker's Scrapus): the mission wants it dead, as its text
 * tells the single player */
static byte Cp_DeathWanted(cEntity *e) {
	nativeVal_t v;
	if (!Entity_IsType(e, TYEN_WALKCHAR)) {
		return 0;
	}
	byte hook = Mode_ModGet("CharAct", "onNPCDeath", &v) && v.type != NV_NONE;
	Native_Free(&v);
	Native_ErrClear();
	return hook;
}

/* test: an entity of the scripts with a life that is no character and no ship (IntoTheGDB's restore point: life 0) */
static byte Cp_Thing(cEntity *e) {
	return !Entity_IsType(e, TYEN_WALKCHAR) && !Entity_IsType(e, TYEN_CAR);
}

/* test: the living entities of src (see fightList) that are no player's (foes 1: only targets to fight, 0: only the
 * others, -1: all), into out; their count; of the mission's targets only the ones on the radar (a player does not see
 * the ones the scripts took off it: IntoTheGDB's scanned bishops); things of the scripts with a life 0 count */
static int Cp_FightTargets(const char *src, cEntity **out, int max, int foes) {
	char mod[CP_NAME];
	nativeVal_t v, it;
	int n = 0;
	if (!strcmp(src, CP_FIGHT_RED) || src[0] == '@') {
		/* the red ones, or the entities of an entity list of the scripts ("@OutCop": the police ships) but a green one of
		 * the list (DistractHumphrey's Humphrey among the traffic) */
		for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e && n < max; e = cEntity_Next(e)) {
			char *list = NULL, *tg = NULL;
			if (Entity_IsType(e, TYEN_WITH_LIFE) && WLife(e)->life > 0.0f && !e->deadNode.target && !Obj(e)->replica &&
				Players_Of(e) < 0 && (src[0] == '@' ? Entity_VarS(e, "EntityList", &list, 0) && list && !strcmp(list, src + 1) &&
				!(Entity_VarS(e, "TgType", &tg, 0) && tg && !crt_stricmp(tg, "green")) : Cp_Foe(e) && foes != 0)) {
				out[n++] = e;
			}
		}
		return n;
	}
	Native_SetNone(&v);
	Str_Copy(mod, src[0] ? src : "MissionsFuncs.currentTarget", sizeof(mod));
	/* a character the mission wants dead is its current target only (not one of the list while the current one is
	 * chosen: ApologizeMayor's mayor between two staplers was punched and fought back for good) */
	byte wanted = 1;
	char *dot = strchr(mod, '.');
	if (!dot) {
		Native_SetText(&v, src, (int)strlen(src));
	} else {
		*dot = 0;
		if (!Mode_ModGet(mod, dot + 1, &v) || (!src[0] && (v.type != NV_STR || !Native_Text(&v)[0]))) {
			Native_Free(&v);
			Native_SetNone(&v);
			if (!src[0] && !Mode_ModGet("MissionsFuncs", "targetsList", &v)) {
				Native_SetNone(&v);
			}
			wanted = src[0] != 0;
		}
	}
	int k = v.type == NV_STR ? 1 : v.type == NV_NONE ? 0 : Mode_Loop(&v);
	for (int j = 0; j < k && n < max; j++) {
		Native_SetNone(&it);
		cEntity *e = v.type == NV_STR ? World_FindEntity(Native_Text(&v)) : Native_SeqItem(&v, j, &it) &&
			it.type == NV_STR ? World_FindEntity(Native_Text(&it)) : NULL;
		/* the mission's target whose radar mark and arrow the scripts took away (ActivateTarget 0: DistractHumphrey's phone
		 * cab after the call, IntoTheGDB's scanned bishops) is none for a player */
		if (e && Entity_IsType(e, TYEN_WITH_LIFE) && (WLife(e)->life > 0.0f || Cp_Thing(e)) && !e->deadNode.target &&
			Players_Of(e) < 0 && (foes < 0 || (Cp_Foe(e) || (wanted && Cp_DeathWanted(e))) == foes) &&
			(src[0] || Obj(e)->radarShow || WLife(e)->arrow.canMission)) {
			out[n++] = e;
		}
		Native_Free(&it);
	}
	Native_ErrClear();
	Native_Free(&v);
	return n;
}

/* test: the weapon of ship e to fight a target d away with: the first it has with ammo of the homing missiles, the
 * heavy and the instant-hit weapons (near: the instant-hit ones first) but the useless ones (bits), else its own */
static int Cp_FightWeapon(cEntity *e, float d, uint32_t useless) {
	cWeaponManager *m = (cWeaponManager *)&Veh(e)->weapons;
	const int far[] = { g_SwarmIDWeap, g_InfernoIDWeap, g_ATPCIDWeap, g_DevastatorIDWeap, g_TeslaIDWeap, g_VulcanIDWeap,
		g_LaserIDWeap };
	const int near[] = { g_ATPCIDWeap, g_TeslaIDWeap, g_VulcanIDWeap, g_DevastatorIDWeap, g_SwarmIDWeap, g_InfernoIDWeap,
		g_LaserIDWeap };
	const int *pick = d < CP_FIGHT_HIT ? near : far;
	for (int k = 0; k < (int)(sizeof(far) / sizeof(far[0])); k++) {
		if (pick[k] >= 0 && pick[k] < g_pWorld->weapons.count && m->weaps[pick[k]].slots &&
			cWeaponManager_HasMinAmmo(m, (uint32_t)pick[k]) && !(pick[k] < 32 && (useless >> pick[k] & 1))) {
			return pick[k];
		}
	}
	return Veh(e)->controls.weapon;
}

/* test: the weakest weapon of ship e that has ammo (the laser, else the Vulcan), else its own */
static int Cp_GentleWeapon(cEntity *e) {
	cWeaponManager *m = (cWeaponManager *)&Veh(e)->weapons;
	const int weak[] = { g_LaserIDWeap, g_VulcanIDWeap };
	for (int k = 0; k < 2; k++) {
		if (weak[k] >= 0 && weak[k] < g_pWorld->weapons.count && m->weaps[weak[k]].slots &&
			cWeaponManager_HasMinAmmo(m, (uint32_t)weak[k])) {
			return weak[k];
		}
	}
	return Veh(e)->controls.weapon;
}

/* test: a weapon that fired at t for CP_FIGHT_USELESS s (near enough) while t lost no life is useless against it, as a
 * player sees (the mercury freight that only the ATPC hurts); a life that changes at all (a shield that regenerates)
 * starts the watch again */
static void Cp_FightUseless(int i, cEntity *t, int w, byte firing, double now) {
	float life = WLife(t)->life;
	if (firing && w == g_Coop.pl[i].lifeWeap && life < g_Coop.pl[i].lifeMark - 0.5f && w >= 0 && w < 32) {
		g_Coop.pl[i].useful |= 1u << w;
	}
	if (!firing || w != g_Coop.pl[i].lifeWeap || fabsf(life - g_Coop.pl[i].lifeMark) >= 0.5f) {
		g_Coop.pl[i].lifeWeap = firing ? w : -1;
		g_Coop.pl[i].lifeMark = life;
		g_Coop.pl[i].lifeSince = now;
		return;
	}
	if (now - g_Coop.pl[i].lifeSince < CP_FIGHT_USELESS || w < 0 || w >= 32) {
		return;
	}
	g_Coop.pl[i].useless |= 1u << w;
	g_Coop.pl[i].lifeWeap = -1;
	Con_Printf("coop: player %d: weapon %d does not hurt %s\n", i, w, t->name);
}

/* test: the weapons that hurt the target are all out of ammo: player i's ship is put at the nearest ammo box of the
 * level it did not take lately (its pickup radius takes it, as a player flies through it); 1 while it goes for ammo */
static byte Cp_FightAmmo(int i, cEntity *e, double now) {
	cWeaponManager *m = (cWeaponManager *)&Veh(e)->weapons;
	uint32_t want = g_Coop.pl[i].useful;
	for (int w = 0; want && w < 32 && w < g_pWorld->weapons.count; w++) {
		if ((want >> w & 1) && m->weaps[w].slots && cWeaponManager_HasMinAmmo(m, (uint32_t)w)) {
			return 0;
		}
	}
	if (!want) {
		return 0;
	}
	if (now - g_Coop.pl[i].ammoAt < CP_AMMO_GAP) {
		return 1;
	}
	cEntity *box = NULL;
	float best = 0.0f;
	for (cEntity *b = cWorld_GetFirstEntity(g_pWorld); b; b = cEntity_Next(b)) {
		char *res = NULL;
		int k = 0;
		if (!b->active || b->deadNode.target || !Entity_IsType(b, "item") || !Entity_VarS(b, "NetRes", &res, 0) || !res ||
			strncmp(res, "Box:", 4) || !strcmp(res + 4, "Life")) {
			continue;
		}
		while (k < CP_AMMO_TRIED && strcmp(g_Coop.pl[i].ammoTried[k], b->name)) {
			k++;
		}
		float dx = b->pos.x - e->pos.x, dy = b->pos.y - e->pos.y, dz = b->pos.z - e->pos.z, d = dx * dx + dy * dy + dz * dz;
		if (k == CP_AMMO_TRIED && (!box || d < best)) {
			box = b;
			best = d;
		}
	}
	if (!box) {
		/* every one taken lately: the oldest again next time */
		g_Coop.pl[i].ammoTried[g_Coop.pl[i].ammoNext][0] = 0;
		g_Coop.pl[i].ammoNext = (g_Coop.pl[i].ammoNext + 1) % CP_AMMO_TRIED;
		return 0;
	}
	Str_Copy(g_Coop.pl[i].ammoTried[g_Coop.pl[i].ammoNext], box->name, CP_NAME);
	g_Coop.pl[i].ammoNext = (g_Coop.pl[i].ammoNext + 1) % CP_AMMO_TRIED;
	g_Coop.pl[i].ammoAt = now;
	e->pos = (vec3_t){ box->pos.x, box->pos.y + 200.0f, box->pos.z };
	Obj(e)->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
	Rep_Placed(i);
	Con_Printf("coop: player %d: no ammo for the weapons that hurt its target: ammo box %s\n", i, box->name);
	return 1;
}

/* test: a ship under CP_LIFE_LOW percent of its most life is put at the nearest life box first, as a player flies to one
 * between two attacks (alone the test player died in the mosquitoes of MosquitoClue); a taken box waits for its respawn */
static byte Cp_FightLife(int i, cEntity *e, double now) {
	int most = 0;
	if (!Entity_VarI(e, "MaxLife", &most, 0) || most <= 0 || WLife(e)->life * 100 >= most * CP_LIFE_LOW ||
		now - g_Coop.pl[i].lifeBoxAt < CP_AMMO_GAP) {
		return 0;
	}
	cEntity *box = NULL;
	float best = 0.0f;
	for (cEntity *b = cWorld_GetFirstEntity(g_pWorld); b; b = cEntity_Next(b)) {
		char *res = NULL;
		if (!b->active || b->deadNode.target || !Entity_IsType(b, "item") || ((const cItemEntity *)b)->respawn > 0.0f ||
			!Entity_VarS(b, "NetRes", &res, 0) || !res || strcmp(res, "Box:Life")) {
			continue;
		}
		float dx = b->pos.x - e->pos.x, dy = b->pos.y - e->pos.y, dz = b->pos.z - e->pos.z, d = dx * dx + dy * dy + dz * dz;
		if (!box || d < best) {
			box = b;
			best = d;
		}
	}
	if (!box) {
		return 0;
	}
	g_Coop.pl[i].lifeBoxAt = now;
	e->pos = (vec3_t){ box->pos.x, box->pos.y + 200.0f, box->pos.z };
	Obj(e)->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
	Rep_Placed(i);
	Con_Printf("coop: player %d: life %d of %d: life box %s\n", i, WLife(e)->life, most, box->name);
	return 1;
}

/* test: p kept inside the box of the level's grid of points (a fighter put above a target at the box's top made the
 * AI's grid lookups of the instance fail their check: FZ's racers) */
static void Cp_InWorld(vec3_t *p) {
	const cGridElements *g = g_NodesWorld ? g_NodesWorld->sup.grid : NULL;
	if (!g) {
		return;
	}
	float *v[3] = { &p->x, &p->y, &p->z };
	const float lo[3] = { g->min.x, g->min.y, g->min.z }, hi[3] = { g->max.x, g->max.y, g->max.z };
	for (int k = 0; k < 3; k++) {
		*v[k] = *v[k] < lo[k] + CP_WORLD_EDGE ? lo[k] + CP_WORLD_EDGE : *v[k] > hi[k] - CP_WORLD_EDGE ? hi[k] - CP_WORLD_EDGE : *v[k];
	}
}

static void Cp_Approach(cEntity *e, cEntity *t, float dist, float *dx, float *dz);

/* test: player i on foot at character t (d away): next to it on a side with a free line to it, facing it, the action
 * key when it is near ahead (D-Tritus' punch, the attacks of the other characters) */
static void Cp_TestMelee(int i, NetRepInput *in, cEntity *t, float d, double now) {
	cEntity *e = Players_Get(i);
	if (d > CP_MELEE_FAR) {
		if (now - g_Coop.pl[i].fightSince >= 1.0) {
			float dx, dz;
			Cp_Approach(e, t, CP_MELEE, &dx, &dz);
			Travel_TestGoto(i, t->name, dx, dz);
			g_Coop.pl[i].fightSince = now;
		}
		return;
	}
	float rot[2], dyaw = NetPl_Aim(e, &t->pos, rot, 1);
	in->rot[0] = NetRep_QRot(rot[0]);
	in->move[0] = NetRep_QMove(d > CP_MELEE && fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
	if (d < CP_MELEE * 2.0f && fabsf(dyaw) < 0.3f && fmod(now, 0.5) < 1.0 / INSTANCE_TICK_HZ) {
		in->buttons |= NETREP_IN_ACT;
	}
}

/* test: player i fights the nearest of its targets through its input, as a player at the keys: turns to it and fires
 * when it is ahead and near (the weapon's own shots and damage, the target's OnDeath with the member as the winner);
 * a target further than CP_FIGHT_FAR: the member is put CP_FIGHT_NEAR from it (once a second at most, as a pilot that
 * knows the way through the streets) */
static void Cp_Fight(int i, NetRepInput *in, double back) {
	cEntity *e = Players_Get(i), *all[CP_FIGHT_MAX], *t = NULL;
	int kind = NetPl_Kind(e), n = kind ? Cp_FightTargets(g_Coop.pl[i].fightList, all, CP_FIGHT_MAX, -1) : 0;
	float best = 0.0f;
	/* the one it fights until it dies, else the nearest */
	for (int k = 0; k < n; k++) {
		float dx = all[k]->pos.x - e->pos.x, dy = all[k]->pos.y - e->pos.y, dz = all[k]->pos.z - e->pos.z;
		float d = dx * dx + dy * dy + dz * dz;
		if (!strcmp(all[k]->name, g_Coop.pl[i].fightAt)) {
			t = all[k];
			best = d;
			break;
		}
		if (!t || d < best) {
			t = all[k];
			best = d;
		}
	}
	if (!t) {
		if (kind && g_Coop.pl[i].fightAt[0]) {
			Con_Printf("coop: player %d: no target left to fight\n", i);
			g_Coop.pl[i].fightAt[0] = 0;
		}
		return;
	}
	/* a ship the mission wants hit but alive (AttackTraitorius: scared under 30% of its life for a minute) is spared
	 * there, as a player watches its life bar */
	byte gentle = 0;
	if (g_Coop.pl[i].spare > 0 && Entity_IsType(t, TYEN_CAR)) {
		float pct = WLife(t)->life * 100.0f / (float)(Veh(t)->maxLife > 0 ? Veh(t)->maxLife : 1);
		g_Coop.pl[i].follow |= pct <= (float)g_Coop.pl[i].spare;
		/* near it the weakest weapon, as a player does not to kill it */
		gentle = pct <= (float)g_Coop.pl[i].spare * 2.0f;
	}
	float d = sqrtf(best), near = g_Coop.pl[i].follow ? CP_FOLLOW_NEAR : CP_FIGHT_NEAR;
	double now = Instance_Time();
	if (kind == NETREP_CHAR && Entity_IsType(t, TYEN_CAR)) {
		/* a ship to fight a ship: the switch (into the nearest parked ship, else a called one) */
		in->buttons |= fmod(now, 2.0) < 1.0 / INSTANCE_TICK_HZ ? NETREP_IN_SWITCH : 0;
		return;
	}
	if (strcmp(g_Coop.pl[i].fightAt, t->name)) {
		Str_Copy(g_Coop.pl[i].fightAt, t->name, sizeof(g_Coop.pl[i].fightAt));
		Con_Printf("coop: player %d fights %s (%.0f away)\n", i, t->name, (double)d);
		g_Coop.pl[i].useless = g_Coop.pl[i].useful = 0;
		g_Coop.pl[i].lifeWeap = -1;
	}
	if (kind == NETREP_CHAR) {
		Cp_TestMelee(i, in, t, d, now);
		return;
	}
	if (d > (g_Coop.pl[i].follow ? CP_FOLLOW_FAR : CP_FIGHT_FAR) && now - g_Coop.pl[i].fightSince >= (g_Coop.pl[i].follow ? 0.5 : 1.0)) {
		float k = near / d;
		e->pos = (vec3_t){ t->pos.x + (e->pos.x - t->pos.x) * k, t->pos.y + (kind == NETREP_SHIP ? 300.0f : 100.0f),
			t->pos.z + (e->pos.z - t->pos.z) * k };
		Cp_InWorld(&e->pos);
		/* a ship it stays with: at its speed, as a pilot behind it (a scan needs it near for seconds) */
		Obj(e)->vel = g_Coop.pl[i].follow ? Obj(t)->vel : (vec3_t){ 0.0f, 0.0f, 0.0f };
		/* facing it (yaw 0 looks to -z) */
		Obj(e)->ang.y = atan2f(-(t->pos.x - e->pos.x), -(t->pos.z - e->pos.z));
		if (kind == NETREP_CHAR) {
			WChar(e)->view.y = Obj(e)->ang.y;
		}
		Rep_Placed(i);
		g_Coop.pl[i].fightSince = now;
		d = near;
	}
	if (kind == NETREP_SHIP && (Cp_FightLife(i, e, now) || Cp_FightAmmo(i, e, now))) {
		return;
	}
	/* where its client shows it (the server's shot looks there) */
	back = back < 0.0 ? 0.0 : back > 0.5 ? 0.5 : back;
	/* and ahead of it by the flight of a shot (CP_FIGHT_SHOT units a second), as a player leads a moving target */
	float lead = kind == NETREP_SHIP ? d / CP_FIGHT_SHOT - (float)back : -(float)back;
	vec3_t *v = &Obj(t)->vel, at = { t->pos.x + v->x * lead, t->pos.y + v->y * lead, t->pos.z + v->z * lead };
	float rot[2], dy = at.y - e->pos.y, cone = atan2f(CP_FIGHT_SIZE, d);
	float dyaw = NetPl_Aim(e, &at, rot, 1);
	if (kind == NETREP_SHIP) {
		/* a pilot's quick turn: the full turn until it is this near (NetPl_Aim's own is the bots' slow one) */
		float hz = sqrtf((at.x - e->pos.x) * (at.x - e->pos.x) + (at.z - e->pos.z) * (at.z - e->pos.z));
		float pitch = atan2f(dy, hz) - Obj(e)->ang.x;
		rot[0] = dyaw > CP_FIGHT_TURN ? 1.0f : dyaw < -CP_FIGHT_TURN ? -1.0f : dyaw / CP_FIGHT_TURN;
		rot[1] = pitch > CP_FIGHT_TURN ? 1.0f : pitch < -CP_FIGHT_TURN ? -1.0f : pitch / CP_FIGHT_TURN;
	}
	byte ahead = fabsf(dyaw) < cone && (kind != NETREP_SHIP || fabsf(rot[1]) < cone * 2.0f) && d < CP_FIGHT_RANGE;
	in->rot[0] = NetRep_QRot(rot[0]);
	if (kind == NETREP_SHIP) {
		in->rot[1] = NetRep_QRot(rot[1]);
		in->weapon = (uint8_t)(gentle ? Cp_GentleWeapon(e) : Cp_FightWeapon(e, d, g_Coop.pl[i].useless));
		Cp_FightUseless(i, t, in->weapon, d < CP_FIGHT_RANGE && !g_Coop.pl[i].follow, now);
		/* closer while far, else its speed (behind it, turning with it) */
		float tv = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z), want = d > near * 1.5f ? tv + 1500.0f : tv;
		in->move[0] = NetRep_QMove(Veh(e)->speed > want + 200.0f ? -1.0f : Veh(e)->speed < want ? 1.0f : 0.0f);
		in->move[2] = NetRep_QMove(dy > 500.0f ? 0.8f : dy < -500.0f ? -0.8f : dy / 625.0f);
	} else {
		in->move[0] = NetRep_QMove(d > near * 0.5f && fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
	}
	/* the weapons that fire at the release (the homing missiles lock on while held, the Devastator charges) in
	 * bursts, the others held while the target is ahead */
	int w = kind == NETREP_SHIP ? (int)in->weapon : -1;
	if (g_Coop.pl[i].follow) {
		/* it stays with the target and shoots nothing */
	} else if (w >= 0 && (w == g_SwarmIDWeap || w == g_InfernoIDWeap)) {
		/* a homing missile: held while the target is near ahead until its lock is on it, released then */
		const char *lk = cTargetLock_GetEntity((cTargetLock *)&Veh(e)->weapons);
		if (!lk || strcmp(lk, t->name)) {
			g_Coop.pl[i].lockAt = now;
		}
		if (now - g_Coop.pl[i].lockAt >= CP_FIGHT_LOCK) {
			g_Coop.pl[i].lockAt = now + 0.2;
		} else if (fabsf(dyaw) < CP_FIGHT_LOCKCONE && d < CP_FIGHT_RANGE && now >= g_Coop.pl[i].lockAt - 0.3) {
			in->buttons |= NETREP_IN_ACT;
		}
	} else if (ahead && (kind != NETREP_SHIP || w != g_DevastatorIDWeap || fmod(now, 3.0) < 2.0)) {
		/* the Devastator charges while held and fires at the release, the others are held */
		in->buttons |= NETREP_IN_ACT;
	}
	if (now - g_Coop.pl[i].fightTold >= 5.0) {
		g_Coop.pl[i].fightTold = now;
		Con_Printf("coop: player %d at %s: %.0f away, yaw %.2f, its life %.0f, weapon %d, ammo %.0f\n", i, t->name,
			(double)d, (double)dyaw, (double)WLife(t)->life, kind == NETREP_SHIP ? Veh(e)->controls.weapon : -1,
			kind == NETREP_SHIP ? (double)cWeaponManager_GetWeapAmmo(&Veh(e)->weapons, (uint32_t)Veh(e)->controls.weapon) : 0.0);
	}
}

/* test: player i holds its fire: the variable of the scripts it watches is at its most (DistractHumphrey: ten ships
 * shot down before the time ends the round too soon, a player watches the count on the HUD) */
static byte Cp_UpTo(int i) {
	char mod[CP_NAME];
	nativeVal_t v;
	Str_Copy(mod, g_Coop.pl[i].upTo, sizeof(mod));
	char *dot = strchr(mod, '.');
	if (!dot) {
		return 0;
	}
	*dot = 0;
	Native_SetNone(&v);
	byte at = Mode_ModGet(mod, dot + 1, &v) && v.type == NV_INT && v.i >= g_Coop.pl[i].upToMax;
	Native_ErrClear();
	Native_Free(&v);
	return at;
}

static void Cp_TestFight(int i, NetRepInput *in, double back) {
	byte follow = g_Coop.pl[i].follow;
	g_Coop.pl[i].follow |= Cp_UpTo(i);
	Cp_Fight(i, in, back);
	g_Coop.pl[i].follow = follow;
}

/* test: where player e stands to use t (its arrow selects only what it sees: a line of sight from it): dist from t on
 * the side e comes from, else the next of eight sides with floor under it and nothing between it and t */
static void Cp_Approach(cEntity *e, cEntity *t, float dist, float *dx, float *dz) {
	float a = atan2f(e->pos.x - t->pos.x, e->pos.z - t->pos.z);
	for (int k = 0; k < 8; k++) {
		float b = a + (float)((k + 1) / 2) * (k & 1 ? 1.0f : -1.0f) * (M_PI_F / 4.0f);
		vec3_t at = t->pos, p = { t->pos.x + sinf(b) * dist, t->pos.y + 100.0f, t->pos.z + cosf(b) * dist };
		vec3_t down = { p.x, p.y - 400.0f, p.z };
		at.y += 20.0f;
		if (!Engine_TestLine(&p, &at, NULL, NULL) && Engine_TestLine(&p, &down, NULL, NULL)) {
			*dx = p.x - t->pos.x;
			*dz = p.z - t->pos.z;
			return;
		}
	}
	*dx = 0.0f;
	*dz = dist;
}

/* test: a flying ship to shoot down (not a parked one, not a green one to stay with) */
static byte Cp_ShipFoe(cEntity *v) {
	char *tg = NULL;
	return Entity_IsType(v, TYEN_CAR) && strncmp(v->name, "Parked", 6) && Entity_VarS(v, "TgType", &tg, 0) &&
		!(tg && !crt_stricmp(tg, "green"));
}

/* test: the nearest entity to e the scripts marked with the mission arrow (MissionsFuncs.ActivateTarget: a clue to take,
 * a blueprint; things with life 0 too: GetDataPart2's desks), not a door of the level's exits; a ship to shoot down before the others (Ambush's probes while the
 * phone cab of the mission's start keeps its arrow); NULL */
/* test: the entity of MissionsFuncs.currentTarget (the mission's target the arrow of the HUD shows), NULL */
static cEntity *Cp_CurrentTarget(void) {
	nativeVal_t v;
	cEntity *t = NULL;
	Native_SetNone(&v);
	if (Mode_ModGet("MissionsFuncs", "currentTarget", &v)) {
		t = v.type == NV_STR ? World_FindEntity(Native_Text(&v)) : v.type == NV_ENT ? Native_Ent(&v) : NULL;
	}
	Native_Free(&v);
	Native_ErrClear();
	return t;
}

static cEntity *Cp_ArrowTarget(const cEntity *e) {
	cEntity *t = NULL;
	float best = 0.0f;
	byte ship = 0;
	/* the mission's current target before the other marked ones (GetDataPart3's call to Deep Throat: the boss' phone
	 * cab kept its arrow and its use, which starts the data's waves again) */
	cEntity *cur = Cp_CurrentTarget();
	byte curOk = 0;
	for (cEntity *v = cWorld_GetFirstEntity(g_pWorld); v; v = cEntity_Next(v)) {
		if (!Entity_IsType(v, TYEN_WITH_LIFE) || !WLife(v)->arrow.canMission || (WLife(v)->life <= 0.0f && !Cp_Thing(v)) ||
			v->deadNode.target ||
			Obj(v)->replica || Players_Of(v) >= 0) {
			continue;
		}
		/* a door only when the mission took its use (a function of the mission, no port of the doors' modules Elevator,
		 * Metro: GoToOutskirts' elevator to GamblinDen) */
		const char *um = cPy_Function_Module(&WLife(v)->arrow.onUse);
		if (!strncmp(v->name, "DM_Door", 7) && um[0] && strncmp(um, "Mission_", 8)) {
			continue;
		}
		curOk |= v == cur;
		float dx = v->pos.x - e->pos.x, dz = v->pos.z - e->pos.z, d = dx * dx + dz * dz;
		byte s = Cp_ShipFoe(v);
		if (!t || s > ship || (s == ship && d < best)) {
			t = v;
			best = d;
			ship = s;
		}
	}
	/* (a ship to shoot down still first) */
	return curOk && !ship ? cur : t;
}

/* test: the nearest item to e the scripts show on the radar with a function of the mission for its taker
 * (ProtectMayorTransport's meeting point: "a yellow arrow on your radar"; not the level's parkings and links), NULL */
static cEntity *Cp_RadarItem(const cEntity *e) {
	const char *mission = SaveVar_Get("Mission.Library"), *deal = SaveVar_Get("SuperDealType");
	cEntity *t = NULL;
	float best = 0.0f;
	for (cEntity *v = cWorld_GetFirstEntity(g_pWorld); v; v = cEntity_Next(v)) {
		const cItemEntity *it = (const cItemEntity *)v;
		const char *m = Entity_IsType(v, "item") ? cPy_Function_Module(&it->onTake) : "";
		/* the mission's, or a super deal mode's (Domination's baton and zones; a race's checkpoint is raced) */
		if (!v->active || v->deadNode.target || !Entity_IsType(v, "item") || !Obj(v)->radarShow || Obj(v)->replica ||
			it->respawn > 0.0f || !mission || (strcmp(m, mission) && !(deal && deal[0] && !strcmp(m, deal) &&
			strncmp(v->name, CP_CHECKPOINT, sizeof(CP_CHECKPOINT) - 1)))) {
			continue;
		}
		float dx = v->pos.x - e->pos.x, dz = v->pos.z - e->pos.z, d = dx * dx + dz * dz;
		/* a deal's baton first (Domination's zones count only with it) */
		d = !strncmp(v->name, "Baton", 5) ? -1.0f : d;
		if (!t || d < best) {
			t = v;
			best = d;
		}
	}
	return t;
}

/* test: no floor within CP_AIR under entity v (DistractHumphrey's meeting point: a ship takes it, the scripts' camera
 * travel moves a ship) */
static byte Cp_InAir(const cEntity *v) {
	vec3_t p = v->pos, down = { p.x, p.y - CP_AIR, p.z };
	return !Engine_TestLine(&p, &down, NULL, NULL);
}

/* test: the mission's green ship (to follow or escort: StupidMerc's mercenary), NULL */
static cEntity *Cp_Escorted(void) {
	cEntity *all[CP_FIGHT_MAX];
	int n = Cp_FightTargets("", all, CP_FIGHT_MAX, 0);
	for (int k = 0; k < n; k++) {
		char *tg = NULL;
		if (Entity_IsType(all[k], TYEN_CAR) && strncmp(all[k]->name, "Parked", 6) && Entity_VarS(all[k], "TgType", &tg, 0) &&
			tg && !crt_stricmp(tg, "green")) {
			return all[k];
		}
	}
	return NULL;
}

/* test: a red one within CP_ESCORT of e */
static byte Cp_RedNear(const cEntity *e) {
	cEntity *all[CP_FIGHT_MAX];
	int n = Cp_FightTargets(CP_FIGHT_RED, all, CP_FIGHT_MAX, 1);
	for (int k = 0; k < n; k++) {
		float dx = all[k]->pos.x - e->pos.x, dy = all[k]->pos.y - e->pos.y, dz = all[k]->pos.z - e->pos.z;
		if (dx * dx + dy * dy + dz * dz < CP_ESCORT * CP_ESCORT) {
			return 1;
		}
	}
	return 0;
}

/* test: the level has the walkers' AI graph (the indoor levels) */
static byte Cp_WalkGraph(void) {
	return g_AStar2D && g_NodesWorld2D && NodesWorld_IsCreated(NW2D_SUP(g_NodesWorld2D));
}

/* test: walking character v follows e within the scripts' leash: its logic chases e (LLogicTarget with LLogicState
 * GENCHAR_CHASE: BeatPickus' thugs; not the police whose logic attacks it) or the target of its AI's chase without
 * action is e (SAI.SetStateChar 3: GoWithHuman's and TalkToBoss' human; not one that attacks it) */
static byte Cp_Follows(cEntity *v, const cEntity *e) {
	char *tgt = NULL;
	int st = -1;
	float vx = v->pos.x - e->pos.x, vy = v->pos.y - e->pos.y, vz = v->pos.z - e->pos.z;
	if (!v->active || !Entity_IsType(v, TYEN_WALKCHAR) || Players_Of(v) >= 0 || Obj(v)->replica ||
		vx * vx + vy * vy + vz * vz >= CP_WALK_LEASH * CP_WALK_LEASH) {
		return 0;
	}
	cAIChar *ai = Entity_GetAI(v);
	return (Entity_VarS(v, "LLogicTarget", &tgt, 0) && tgt && !strcmp(tgt, e->name) && Entity_VarI(v, "LLogicState", &st, 0) &&
		st == GENCHAR_CHASE) || (ai && ai->state == AIC_CHASE_WITHOUT_ACTION && ai->base.target.target == e);
}

/* test: the point player e walks to next on its way to `to`: the walking NPCs' A* on the level's AI graph (CAStar2D,
 * the graph of the indoor levels), the furthest point of the path its body reaches on a straight line; 0 without a
 * graph or a path */
static byte Cp_PathPoint(cEntity *e, const vec3_t *to, vec3_t *next) {
	static cNWVecList path;
	if (!Cp_WalkGraph()) {
		return 0;
	}
	vec3_t from = e->pos, dest = *to;
	float r = Obj_GetRadius(e), h = WChar(e)->floorDist, fr = WithLife_GetActCtrl(e)->cls->sRadius;
	byte noPath = 0, reach = 0, got = 0;
	int res = 0;
	if (CAStar2D_FindPath(g_AStar2D, &path, &dest, &from, r, 0.0f, h, fr, 0, &noPath, &reach, &res) && path.head) {
		/* from its end where the walker is */
		cNWVecNode *n = path.head, *last = path.head->prev;
		float hx = n->v.x - from.x, hz = n->v.z - from.z, lx = last->v.x - from.x, lz = last->v.z - from.z;
		byte fwd = hx * hx + hz * hz <= lx * lx + lz * lz;
		n = fwd ? n : last;
		for (int k = 0; k < path.count && n; k++, n = fwd ? n->next : n->prev) {
			float dx = n->v.x - from.x, dz = n->v.z - from.z;
			if (got && !CNodesWorld2D_TestPath(g_NodesWorld2D, &from, &n->v, r, r, h, fr, NULL)) {
				break;
			}
			if (got || dx * dx + dz * dz > CP_WALK_NEAR * CP_WALK_NEAR) {
				*next = n->v;
				got = 1;
			}
		}
	}
	NWVecList_Free(&path);
	return got;
}

/* test: the point ship e flies to next on its way to `to`: `to` on a free line, else the AI ships' A* on the level's
 * graph of visibility (CAStar, the outdoor levels), the furthest point of the path it reaches on a straight line; 0
 * without a graph or a path */
static byte Cp_FlyPoint(cEntity *e, const vec3_t *to, vec3_t *next) {
	static cNWVecList path;
	if (!g_AStar || !g_NodesWorld || !NodesWorld_IsCreated(g_NodesWorld)) {
		return 0;
	}
	vec3_t from = e->pos, dest = *to;
	float r = Obj_GetRadius(e);
	byte startOk = 0, goalOk = 0, got = 0;
	int state = 0;
	/* a free line: straight there */
	if (CNodesWorld_TestPath(g_NodesWorld, &from, &dest, r, r, NULL, 0)) {
		*next = dest;
		return 1;
	}
	if (CAStar_FindPath(g_AStar, &path, &dest, &from, r, 0.0f, 0, &startOk, &goalOk, &state) && path.head) {
		/* from its end where the ship is */
		cNWVecNode *n = path.head, *last = path.head->prev;
		float hx = n->v.x - from.x, hz = n->v.z - from.z, lx = last->v.x - from.x, lz = last->v.z - from.z;
		byte fwd = hx * hx + hz * hz <= lx * lx + lz * lz;
		n = fwd ? n : last;
		for (int k = 0; k < path.count && n; k++, n = fwd ? n->next : n->prev) {
			float dx = n->v.x - from.x, dy = n->v.y - from.y, dz = n->v.z - from.z;
			if (got && !CNodesWorld_TestPath(g_NodesWorld, &from, &n->v, r, r, NULL, 0)) {
				break;
			}
			if (got || dx * dx + dy * dy + dz * dz > CP_FLY_NEAR * CP_FLY_NEAR) {
				*next = n->v;
				got = 1;
			}
		}
	}
	NWVecList_Free(&path);
	return got;
}

/* test: player i's ship (e) flies to g through its input along the level's graph of visibility, as a pilot steers
 * round the buildings; when it comes no nearer for CP_FLY_STUCK s it is placed CP_FIGHT_NEAR from g (the placement of
 * before, kept where the graph gives no way); 1 while it flies, 0 within `near` of g (on the ground) or stuck that
 * near */
static byte Cp_FlyTo(int i, NetRepInput *in, cEntity *e, cEntity *g, float near, double now) {
	float dx = g->pos.x - e->pos.x, dy = g->pos.y - e->pos.y, dz = g->pos.z - e->pos.z;
	float d = sqrtf(dx * dx + dy * dy + dz * dz);
	/* over it (a goal on a platform the ship cannot come down to: out of the ship there, the fall takes it down) */
	if (dx * dx + dz * dz <= near * near) {
		g_Coop.pl[i].flyGoal[0] = 0;
		return 0;
	}
	/* a new goal, or a flight a fight broke off: its watch starts again */
	if (strcmp(g_Coop.pl[i].flyGoal, g->name) || now - g_Coop.pl[i].flyAt > CP_FLY_GAP * 4.0) {
		Str_Copy(g_Coop.pl[i].flyGoal, g->name, sizeof(g_Coop.pl[i].flyGoal));
		g_Coop.pl[i].flyBest = d;
		g_Coop.pl[i].flyMoved = now;
		g_Coop.pl[i].flyAt = 0.0;
		Con_Printf("coop: player %d (auto) flies to %s (%.0f away)\n", i, g->name, (double)d);
	}
	if (d < g_Coop.pl[i].flyBest - CP_FLY_ON) {
		g_Coop.pl[i].flyBest = d;
		g_Coop.pl[i].flyMoved = now;
	} else if (now - g_Coop.pl[i].flyMoved >= CP_FLY_STUCK) {
		/* as near as the placement puts it: the way ends here (the goal on a platform it cannot fly onto) */
		if (dx * dx + dz * dz <= CP_FIGHT_NEAR * CP_FIGHT_NEAR * 2.25f) {
			return 0;
		}
		float k = CP_FIGHT_NEAR / d;
		if (k < 1.0f) {
			e->pos = (vec3_t){ g->pos.x - dx * k, g->pos.y + 300.0f, g->pos.z - dz * k };
			Obj(e)->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
			Obj(e)->ang.y = atan2f(-dx, -dz);
			Rep_Placed(i);
			Con_Printf("coop: player %d (auto) is stuck on its flight to %s: placed %.0f from it\n", i, g->name,
				(double)CP_FIGHT_NEAR);
		}
		g_Coop.pl[i].flyBest = d < CP_FIGHT_NEAR ? d : CP_FIGHT_NEAR;
		g_Coop.pl[i].flyMoved = now;
		g_Coop.pl[i].flyAt = now - CP_FLY_GAP;
		return 1;
	}
	if (now - g_Coop.pl[i].flyAt >= CP_FLY_GAP) {
		g_Coop.pl[i].flyAt = now;
		vec3_t up = { g->pos.x, g->pos.y + 300.0f, g->pos.z };
		if (!Cp_FlyPoint(e, &up, &g_Coop.pl[i].flyTo)) {
			g_Coop.pl[i].flyTo = up;
		}
	}
	/* a pilot's quick turn to its point, full speed while it faces it, slow in the turn */
	vec3_t *p = &g_Coop.pl[i].flyTo;
	float rot[2], dyaw = NetPl_Aim(e, p, rot, 1);
	float hz = sqrtf((p->x - e->pos.x) * (p->x - e->pos.x) + (p->z - e->pos.z) * (p->z - e->pos.z));
	float py = p->y - e->pos.y, pitch = atan2f(py, hz) - Obj(e)->ang.x;
	rot[0] = dyaw > CP_FIGHT_TURN ? 1.0f : dyaw < -CP_FIGHT_TURN ? -1.0f : dyaw / CP_FIGHT_TURN;
	rot[1] = pitch > CP_FIGHT_TURN ? 1.0f : pitch < -CP_FIGHT_TURN ? -1.0f : pitch / CP_FIGHT_TURN;
	in->rot[0] = NetRep_QRot(rot[0]);
	in->rot[1] = NetRep_QRot(rot[1]);
	in->move[0] = NetRep_QMove(fabsf(dyaw) < 0.5f ? 1.0f : fabsf(dyaw) < 1.2f ? 0.3f : 0.0f);
	in->move[2] = NetRep_QMove(py > 500.0f ? 0.8f : py < -500.0f ? -0.8f : py / 625.0f);
	return 1;
}

/* test: player i on foot (e) further than CP_WALK_FAR from t walks there through its input along the level's AI graph
 * while a follower is near it, waits for one that fell behind and goes back to it after CP_WALK_ALONE s, as a player
 * walks with characters that follow him (BeatPickus' thugs, or the one of the test line: GoWithHuman's human;
 * followers only with the test line walk); a walk that does not come on is placed at its path's point, one within
 * CP_WALK_END that comes no nearer ends; 1 while it walks */
static byte Cp_WalkStep(int i, NetRepInput *in, cEntity *e, cEntity *t, double now) {
	float dx = t->pos.x - e->pos.x, dz = t->pos.z - e->pos.z, d = sqrtf(dx * dx + dz * dz);
	if (d <= CP_WALK_FAR) {
		g_Coop.pl[i].walkWait = 0.0;
		g_Coop.pl[i].walkNearAt = 0.0;
		return 0;
	}
	/* a walk that ended short of this target stays ended (the goto and the use come next: its walk's restart took
	 * them away, DistractHumphrey's phone cab with the party around it) while it stays that near */
	if (!strcmp(g_Coop.pl[i].walkShort, t->name) && d <= CP_WALK_END) {
		return 0;
	}
	g_Coop.pl[i].walkShort[0] = 0;
	if (now - g_Coop.pl[i].autoSince >= CP_WALK_GAP) {
		/* its followers (test line walk): the characters that follow it (or the one named by the test line) */
		cEntity *back = NULL;
		float best = 0.0f;
		for (cEntity *v = g_Coop.pl[i].walk ? cWorld_GetFirstEntity(g_pWorld) : NULL; v; v = cEntity_Next(v)) {
			float vx = v->pos.x - e->pos.x, vy = v->pos.y - e->pos.y, vz = v->pos.z - e->pos.z, vd = vx * vx + vy * vy + vz * vz;
			if (v != e && (!g_Coop.pl[i].walkWho[0] || !strcmp(v->name, g_Coop.pl[i].walkWho)) && Cp_Follows(v, e) &&
				(!back || vd < best)) {
				back = v;
				best = vd;
			}
		}
		if (back) {
			Str_Copy(g_Coop.pl[i].follower, back->name, sizeof(g_Coop.pl[i].follower));
			g_Coop.pl[i].walkBack = 0.0;
		} else if (g_Coop.pl[i].walk && g_Coop.pl[i].follower[0] && (back = World_FindEntity(g_Coop.pl[i].follower)) != NULL) {
			/* one that stopped follows another now (BeatPickus' thugs behind their leader): back to it; next to it for
			 * CP_WALK_ALONE s without it following again it is no follower */
			float vx = back->pos.x - e->pos.x, vz = back->pos.z - e->pos.z;
			best = CP_WALK_WAIT * CP_WALK_WAIT;
			if (vx * vx + vz * vz >= best) {
				g_Coop.pl[i].walkBack = 0.0;
			} else if (g_Coop.pl[i].walkBack <= 0.0) {
				g_Coop.pl[i].walkBack = now;
			} else if (now - g_Coop.pl[i].walkBack >= CP_WALK_ALONE) {
				g_Coop.pl[i].follower[0] = 0;
				back = NULL;
			}
		}
		byte near = !back || best < CP_WALK_WAIT * CP_WALK_WAIT;
		g_Coop.pl[i].autoSince = now;
		g_Coop.pl[i].autoUse = 0.0;
		g_Coop.pl[i].autoAt[0] = 0;
		if (near) {
			g_Coop.pl[i].walkWait = 0.0;
		} else if (g_Coop.pl[i].walkWait <= 0.0) {
			g_Coop.pl[i].walkWait = now;
		}
		/* none kept up for a while: back to the nearest one (they stopped where it left them) */
		cEntity *to = !near && now - g_Coop.pl[i].walkWait >= CP_WALK_ALONE ? back : t;
		if (!near || to != t) {
			g_Coop.pl[i].walkNearAt = 0.0;
		} else if (g_Coop.pl[i].walkNearAt <= 0.0 || d < g_Coop.pl[i].walkNear - CP_WALK_STEP * 0.25f) {
			g_Coop.pl[i].walkNear = d;
			g_Coop.pl[i].walkNearAt = now;
		} else if (d <= CP_WALK_END && now - g_Coop.pl[i].walkNearAt >= CP_WALK_STUCK) {
			/* its way ends short of the target (BeatPickus' blueprints on a wall, the graph's last node 500 away): the
			 * goto's last step, as at the end of a walk */
			g_Coop.pl[i].walkNearAt = 0.0;
			Str_Copy(g_Coop.pl[i].walkShort, t->name, sizeof(g_Coop.pl[i].walkShort));
			return 0;
		}
		if (!Cp_PathPoint(e, &to->pos, &g_Coop.pl[i].walkTo)) {
			g_Coop.pl[i].walkTo = to->pos;
		}
		float mx = e->pos.x - g_Coop.pl[i].walkFrom.x, mz = e->pos.z - g_Coop.pl[i].walkFrom.z;
		if (!near && to == t) {
			g_Coop.pl[i].walkMoved = now;
		} else if (mx * mx + mz * mz > CP_WALK_STEP * CP_WALK_STEP * 0.0625f) {
			g_Coop.pl[i].walkFrom = e->pos;
			g_Coop.pl[i].walkMoved = now;
		} else if (now - g_Coop.pl[i].walkMoved >= CP_WALK_STUCK) {
			/* stuck at a corner or a door the graph's line misses: a step onto its path's point */
			vec3_t p = { g_Coop.pl[i].walkTo.x, g_Coop.pl[i].walkTo.y + 150.0f, g_Coop.pl[i].walkTo.z };
			vec3_t down = { p.x, p.y - 2000.0f, p.z }, floor;
			if (Engine_TestLine(&p, &down, &floor, NULL)) {
				p.y = floor.y + 100.0f;
			}
			e->pos = g_Coop.pl[i].walkFrom = p;
			g_Coop.pl[i].walkMoved = now;
			Rep_Placed(i);
			Con_Printf("coop: player %d (auto) is stuck on its walk: placed at %.0f %.0f %.0f\n", i, (double)p.x,
				(double)p.y, (double)p.z);
		}
		if (now - g_Coop.pl[i].walkTold >= 5.0) {
			g_Coop.pl[i].walkTold = now;
			Con_Printf("coop: player %d (auto) walks to %s (%.0f away) by %.0f %.0f, its follower %s %.0f away%s\n", i,
				to->name, (double)d, (double)g_Coop.pl[i].walkTo.x, (double)g_Coop.pl[i].walkTo.z, back ? back->name : "-",
				(double)sqrtf(best), !near && to == t ? ", waits for it" : "");
		}
	}
	if (g_Coop.pl[i].walkWait > 0.0 && now - g_Coop.pl[i].walkWait < CP_WALK_ALONE) {
		return 1;
	}
	float rot[2], dyaw = NetPl_Aim(e, &g_Coop.pl[i].walkTo, rot, 1);
	in->rot[0] = NetRep_QRot(rot[0]);
	in->move[0] = NetRep_QMove(fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
	return 1;
}

/* test: a ship of the mission player i stays near without a shot (test line scan: DestroyMickus' transports until
 * the scan finds Mickus, who turns red; IntoTheGDB's bishops) */
static byte Cp_Scanned(int i, cEntity *v) {
	return g_Coop.pl[i].scan && !Cp_Foe(v);
}

/* test: the item of the scripts' waypoint (CurrentWaypointName: BankersCTF's flag), NULL */
static cEntity *Cp_WaypointItem(void) {
	const char *n = g_CurrentWaypointName;
	cEntity *v = n && n[0] ? World_FindEntity(n) : NULL;
	return v && v->active && !v->deadNode.target && Entity_IsType(v, "item") ? v : NULL;
}

/* test: player i as a banker (on foot) robs the first of the mission's characters n in all that has money, as a
 * player does with the banker's slap (TalkToMayor: Corruptus emptied): it walks there, steps behind it (a victim that
 * looks away notices fewer robberies) and holds the action key while its arrow is on it; 0 when it is no banker or
 * no target has money */
static byte Cp_BankerSlaps(int i, NetRepInput *in, cEntity *e, cEntity **all, int n, double now) {
	cActCtrl *a = NetPl_Kind(e) == NETREP_CHAR ? WithLife_GetActCtrl(e) : NULL;
	cEntity *t = NULL;
	if (!a || !a->cls || strcmp(a->cls->name, "BankDirector")) {
		return 0;
	}
	for (int k = 0; k < n && !t; k++) {
		if (Entity_IsType(all[k], TYEN_WALKCHAR) && WChar(all[k])->money > 0) {
			t = all[k];
		}
	}
	if (!t) {
		return 0;
	}
	if (now - g_Coop.pl[i].autoTold >= 5.0) {
		g_Coop.pl[i].autoTold = now;
		Con_Printf("coop: player %d (auto) robs %s (money %d, %.0f away, its arrow on %s)\n", i, t->name, WChar(t)->money,
			(double)sqrtf((t->pos.x - e->pos.x) * (t->pos.x - e->pos.x) + (t->pos.z - e->pos.z) * (t->pos.z - e->pos.z)),
			WChar_Victim(e) ? WChar_Victim(e)->name : "nothing");
	}
	if (Cp_WalkStep(i, in, e, t, now)) {
		return 1;
	}
	/* yaw 0 looks to -z: behind it is along (sin yaw, cos yaw); from in front or aside it steps round to its back
	 * unless its arrow is on it already near enough */
	float yaw = WChar(t)->view.y, rot[2], bx = sinf(yaw), bz = cosf(yaw);
	float dx = e->pos.x - t->pos.x, dz = e->pos.z - t->pos.z, d = sqrtf(dx * dx + dz * dz);
	byte reach = WChar_Victim(e) == t && d < CP_MELEE * 2.5f;
	if (!reach && dx * bx + dz * bz < d * 0.7f) {
		vec3_t back = { t->pos.x + bx * CP_MELEE, t->pos.y, t->pos.z + bz * CP_MELEE };
		float dyaw = NetPl_Aim(e, &back, rot, 1);
		in->rot[0] = NetRep_QRot(rot[0]);
		in->move[0] = NetRep_QMove(fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
		return 1;
	}
	float dyaw = NetPl_Aim(e, &t->pos, rot, 1);
	in->rot[0] = NetRep_QRot(rot[0]);
	/* it stands while its arrow is on the victim: running into it (the two radii keep it about 160 away) plays Run
	 * over the banker's ActionInit and the slap never comes */
	in->move[0] = NetRep_QMove(!reach && d > CP_MELEE && fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
	/* the banker's action loops while the key is held, each loop robs once */
	if (reach && fabsf(dyaw) < 0.3f) {
		in->buttons |= NETREP_IN_ACT;
	}
	return 1;
}

/* test: a messenger of the level (not flashed) within CP_MSG_NEAR of c, NULL */
static cEntity *Cp_MessengerNear(const cEntity *c) {
	for (cEntity *m = cWorld_GetFirstEntity(g_pWorld); m; m = cEntity_Next(m)) {
		float mx = m->pos.x - c->pos.x, mz = m->pos.z - c->pos.z;
		if (m->active && Entity_IsType(m, TYEN_WALKCHAR) && Players_Of(m) < 0 && mx * mx + mz * mz < CP_MSG_NEAR * CP_MSG_NEAR &&
			!strcmp(ChType_ActClass(ChType_ActCtrl(m))->name, "Messenger") &&
			crt_strncmp(cActCtrl_GetActionName(ChType_ActCtrl(m), 0), "Flashed", 7)) {
			return m;
		}
	}
	return NULL;
}

/* test: player i as a messenger plays the messengers' game of the data pack where the mission marks it
 * (MessengerDataPackMissionArrow: GetAuthorization's tube): another messenger's pack is shot off it (the action key
 * held at it: the lightning flashes it, it drops the pack), a free pack or one on an open desk is walked onto (the
 * touch takes it), its own pack goes to the desk with the mission arrow (the messengers' logic drops it there; the
 * messengers that come for it are shot on the way), another member's pack is escorted; 0
 * when it is no messenger or no pack is marked */
static byte Cp_MessengerPack(int i, NetRepInput *in, cEntity *e, double now) {
	cActCtrl *a = NetPl_Kind(e) == NETREP_CHAR ? WithLife_GetActCtrl(e) : NULL;
	cEntity *pack = g_MessengerDataPack ? World_FindEntity(g_MessengerDataPack) : NULL;
	nativeVal_t v;
	if (!a || !a->cls || strcmp(a->cls->name, "Messenger") || !pack || !Entity_IsType(pack, "item")) {
		return 0;
	}
	byte marked = Native_GVarGet("MessengerDataPackMissionArrow", &v) && Native_True(&v);
	Native_Free(&v);
	Native_ErrClear();
	if (!marked) {
		return 0;
	}
	cEntity *carrier = Obj(pack)->mod.link.target, *to = pack;
	byte shoot = carrier && carrier != e && Entity_IsType(carrier, TYEN_WALKCHAR) && Players_Of(carrier) < 0 &&
		!strcmp(ChType_ActClass(ChType_ActCtrl(carrier))->name, "Messenger");
	if (carrier == e || (carrier && Players_Of(carrier) >= 0)) {
		/* the messengers go for the one that carries it: one near it (not flashed) is shot first, as a player keeps them
		 * off its own pack or a member's; else its own pack goes to the desk, a member's is escorted */
		cEntity *m = Cp_MessengerNear(carrier);
		/* one behind a wall cannot be hit: its own pack goes on to the desk (alone it waited there for ever) */
		if (m && carrier == e) {
			vec3_t from = { e->pos.x, e->pos.y + 100.0f, e->pos.z }, at = { m->pos.x, m->pos.y + 100.0f, m->pos.z };
			m = Engine_TestLine(&from, &at, NULL, NULL) ? NULL : m;
		}
		to = m ? m : carrier == e ? Cp_ArrowTarget(e) : carrier;
		shoot = m != NULL;
	} else if (shoot) {
		to = carrier;
	}
	if (!to) {
		return 1;
	}
	if (now - g_Coop.pl[i].autoTold >= 5.0) {
		g_Coop.pl[i].autoTold = now;
		Con_Printf("coop: player %d (auto) %s %s (the pack %s)\n", i, shoot ? carrier == e ? "keeps off" : "shoots the pack off" :
			carrier == e ? "carries the pack to" : carrier && to == carrier ? "escorts" : "takes the pack at", to->name,
			carrier ? carrier->name : "free");
	}
	/* one that comes for its own pack is shot where it stands (no chase away from the desk) */
	byte keep = shoot && carrier == e;
	float dx = to->pos.x - e->pos.x, dz = to->pos.z - e->pos.z, d = sqrtf(dx * dx + dz * dz);
	/* its own pack is put next to its goal (a desk, the tube room) as the auto player's other moves are: the messengers
	 * that chase a walking carrier flashed it on the way across the station every time (M7k) */
	if (carrier == e && !shoot && d > CP_MSG_CARRY) {
		if (now - g_Coop.pl[i].carryAt >= 2.0) {
			g_Coop.pl[i].carryAt = now;
			Travel_TestGoto(i, to->name, 0.0f, CP_MSG_CARRY * 0.5f);
		}
		return 1;
	}
	if (!keep && (!shoot || d > CP_MSG_SHOT) && Cp_WalkStep(i, in, e, to, now)) {
		return 1;
	}
	/* the lightning goes where it looks: up or down at the messenger too (one on a stair or a desk was missed) */
	float rot[2], dyaw = NetPl_Aim(e, &to->pos, rot, 1), pitch = atan2f(to->pos.y - e->pos.y, d > 1.0f ? d : 1.0f) - WChar(e)->view.x;
	in->rot[0] = NetRep_QRot(rot[0]);
	in->rot[1] = shoot ? NetRep_QRot(pitch > 0.3f ? 0.6f : pitch < -0.3f ? -0.6f : pitch * 2.0f) : 0;
	in->move[0] = NetRep_QMove(!keep && d > (shoot ? CP_MSG_SHOT * 0.25f : CP_MELEE * 0.5f) && fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
	/* the messenger's lightning fires while the key is held */
	vec3_t from = { e->pos.x, e->pos.y + 100.0f, e->pos.z }, at = { to->pos.x, to->pos.y + 100.0f, to->pos.z };
	if (shoot && fabsf(dyaw) < 0.15f && !Engine_TestLine(&from, &at, NULL, NULL)) {
		in->buttons |= NETREP_IN_ACT;
	}
	return 1;
}

/* test: the mission waits for the player to board (Parking.SwitchVehicleMissionCallback: GetDataPart1, the super
 * deals) */
static byte Cp_WaitsForShip(void) {
	nativeVal_t v;
	byte set = Mode_ModGet("Parking", "SwitchVehicleMissionCallback", &v) && v.type != NV_NONE;
	Native_Free(&v);
	Native_ErrClear();
	return set;
}

/* test: player i plays the mission's targets on its own: fights the ones to fight, flies a race of RaceMaker, else
 * goes to the target (out of its ship) and uses its arrow, again every CP_AUTO_GAP s while no talk plays; a green ship
 * of the mission is escorted as a player does: the red ones near it fought first, else it stays with it (the helper
 * too) */
static void Cp_TestAuto(int i, NetRepInput *in, double back) {
	cEntity *e = Players_Get(i), *all[CP_FIGHT_MAX];
	int kind = NetPl_Kind(e);
	if (!kind || Coop_Holds(i)) {
		return;
	}
	if (g_Coop.pl[i].tough && Entity_IsType(e, TYEN_WITH_LIFE)) {
		WLife(e)->god = 1;
	}
	double now = Instance_Time();
	if (now < g_Coop.pl[i].talkUntil) {
		/* a conversation of its own plays: it listens (single player's input stops there), its next use CP_AUTO_GAP after
		 * it (a use between two lines started BeatPickus' talk with Traitorius again) */
		g_Coop.pl[i].autoSince = now;
		return;
	}
	byte pulse = fmod(now, 2.0) < 1.0 / INSTANCE_TICK_HZ;
	/* the mission's targets to fight, else any red one of the level (a mission's enemies it marks without targets; not
	 * while the mission has a target of another kind: a ship to follow, a character to talk to; the helper fights them) */
	cEntity *escort = Cp_Escorted();
	/* the waypoint's item (test line waypoint: a flag of the CTF) is flown onto first, as a player follows its arrow */
	cEntity *wp = g_Coop.pl[i].waypoint ? Cp_WaypointItem() : NULL;
	if (wp) {
		if (strcmp(g_Coop.pl[i].autoAt, wp->name) || now - g_Coop.pl[i].autoSince >= 1.0) {
			Str_Copy(g_Coop.pl[i].autoAt, wp->name, sizeof(g_Coop.pl[i].autoAt));
			g_Coop.pl[i].autoSince = now;
			Con_Printf("coop: player %d (auto) goes onto %s (the waypoint)\n", i, wp->name);
			Travel_TestGoto(i, wp->name, 0.0f, 0.0f);
		}
		return;
	}
	/* a messenger plays the pack's game before the fights (the messengers attack the one that carries the pack) */
	if (g_Coop.pl[i].autoOn != 2 && Cp_MessengerPack(i, in, e, now)) {
		return;
	}
	/* a mission arrow to take (BankersCombat's blueprints while the bankers attack) before the level's red ones; one on
	 * a ship to shoot down before the mission's targets to use (Ambush's probes, its phone cab still the target) */
	cEntity *arrow = Cp_ArrowTarget(e);
	/* in a super deal the mode's items (Domination's baton and zones) come before its red ones, so does the mission
	 * MortalRace's baton (dropped again at a member's death) */
	const char *lib = SaveVar_Get("Mission.Library");
	byte dealItem = g_Coop.pl[i].autoOn != 2 && lib && (!strncmp(lib, "SuperDeal_", 10) || !strcmp(lib, "Mission_MortalRace")) &&
		Cp_RadarItem(e);
	byte other = Cp_FightTargets("", all, 1, 0) || arrow || dealItem;
	const char *foes = Cp_FightTargets("", all, CP_FIGHT_MAX, 1) ? "" : arrow && Cp_ShipFoe(arrow) && !Cp_Scanned(i, arrow) ? arrow->name : (escort ? Cp_RedNear(escort) :
		(g_Coop.pl[i].autoOn == 2 || !other) && Cp_FightTargets(CP_FIGHT_RED, all, CP_FIGHT_MAX, 1)) ? CP_FIGHT_RED : NULL;
	if (foes) {
		Str_Copy(g_Coop.pl[i].fightList, foes, sizeof(g_Coop.pl[i].fightList));
		Cp_TestFight(i, in, back);
		return;
	}
	if (g_Coop.pl[i].autoOn == 2 && !escort) {
		/* a helper: it only fights (what it was told to when the mission shows nothing) */
		if (g_Coop.pl[i].autoList[0]) {
			Str_Copy(g_Coop.pl[i].fightList, g_Coop.pl[i].autoList, sizeof(g_Coop.pl[i].fightList));
			Cp_TestFight(i, in, back);
		}
		return;
	}
	/* a race: the mission's member races (a deal's baton taken first: MortalRace), a helper that stays with a ship of
	 * the mission does not (eight ships put on one checkpoint pushed one out of the level) */
	if (World_FindEntity(CP_CHECKPOINT) && g_Coop.pl[i].autoOn != 2 && !dealItem) {
		if (kind == NETREP_SHIP) {
			Cp_TestRace(i);
		} else {
			in->buttons |= pulse ? NETREP_IN_SWITCH : 0;
		}
		return;
	}
	/* the mission's targets one after the other (the right one of three to talk to: ThreeStooges' mercenaries) */
	int nt = Cp_FightTargets("", all, CP_FIGHT_MAX, 0);
	if (g_Coop.pl[i].autoOn != 2 && Cp_BankerSlaps(i, in, e, all, nt, now)) {
		return;
	}
	cEntity *t = g_Coop.pl[i].autoOn == 2 ? escort : nt ? all[g_Coop.pl[i].autoTurn % nt] : NULL;
	/* else the nearest one the scripts marked with the mission arrow */
	if (!t) {
		t = Cp_ArrowTarget(e);
	}
	/* a talk plays, or one may start after its use: it listens (the call's talk of a phone cab: no ship and no item
	 * meanwhile, GetDataPart3's boarding started the data's waves again) */
	if (g_RemainTalkTime > 0.01f || (g_Coop.pl[i].usedAt > 0.0 && now - g_Coop.pl[i].usedAt < CP_USE_HOLD)) {
		g_Coop.pl[i].autoSince = now;
		return;
	}
	/* else, and before a target to use, an item on the radar: flown or walked onto, as a player takes it (DistractHumphrey's
	 * meeting point while the phone cab of the call stays the mission's target) */
	cEntity *item = t && Entity_IsType(t, TYEN_CAR) ? NULL : Cp_RadarItem(e);
	float ix = item ? item->pos.x - e->pos.x : 0.0f, iz = item ? item->pos.z - e->pos.z : 0.0f;
	if (item && kind == NETREP_CHAR && (Cp_InAir(item) || (!Cp_WalkGraph() && (ix * ix + iz * iz > CP_AUTO_SHIP * CP_AUTO_SHIP ||
		((cItemEntity *)item)->radius >= CP_SHIP_ITEM)))) {
		/* one in the air, far away outdoors or a ship's (a wide one: DistractHumphrey's meeting point, made for the
		 * player's ship, on foot a point below the traffic's grid stopped the instance) is flown onto: into a ship
		 * first (the switch) */
		in->buttons |= pulse ? NETREP_IN_SWITCH : 0;
		return;
	}
	if (item) {
		/* again at once when it moved on (Win1stRace's buoy, taken at each checkpoint) */
		vec3_t *at = &g_Coop.pl[i].autoItemAt;
		float mx = item->pos.x - at->x, my = item->pos.y - at->y, mz = item->pos.z - at->z;
		if (strcmp(g_Coop.pl[i].autoAt, item->name) || now - g_Coop.pl[i].autoSince >= CP_AUTO_GAP ||
			mx * mx + my * my + mz * mz > CP_AUTO_NEAR * CP_AUTO_NEAR) {
			*at = item->pos;
			Str_Copy(g_Coop.pl[i].autoAt, item->name, sizeof(g_Coop.pl[i].autoAt));
			g_Coop.pl[i].autoSince = now;
			Con_Printf("coop: player %d (auto) goes onto %s (on the radar)\n", i, item->name);
			Travel_TestGoto(i, item->name, 0.0f, 0.0f);
		}
		return;
	}
	if (!t) {
		if (g_Coop.pl[i].autoList[0]) {
			Str_Copy(g_Coop.pl[i].fightList, g_Coop.pl[i].autoList, sizeof(g_Coop.pl[i].fightList));
			Cp_TestFight(i, in, back);
		} else if (kind == NETREP_CHAR && Cp_WaitsForShip() &&
			(g_Coop.pl[i].usedAt <= 0.0 || now - g_Coop.pl[i].usedAt >= CP_USE_BOARD)) {
			/* the mission starts when the player boards (its level again after a failure): into a ship; not soon after a
			 * use (GetDataPart3's call to Deep Throat: the boarding's callback started the data's waves again) */
			in->buttons |= pulse ? NETREP_IN_SWITCH : 0;
		}
		return;
	}
	/* what it was told to fight comes before the rest (not before a parked ship of the mission to board) */
	if (g_Coop.pl[i].autoList[0] && !(Entity_IsType(t, TYEN_CAR) && !strncmp(t->name, "Parked", 6)) &&
		Cp_FightTargets(g_Coop.pl[i].autoList, all, 1, -1)) {
		Str_Copy(g_Coop.pl[i].fightList, g_Coop.pl[i].autoList, sizeof(g_Coop.pl[i].fightList));
		Cp_TestFight(i, in, back);
		return;
	}
	if (strcmp(g_Coop.pl[i].autoAt, t->name) && now - g_Coop.pl[i].autoTold >= 10.0) {
		g_Coop.pl[i].autoTold = now;
		Con_Printf("coop: player %d (auto) at the target %s\n", i, t->name);
	}
	/* a flying ship of the mission is to shoot down, a green one (to escort or follow) to stay with */
	char *tg = NULL;
	if (Entity_IsType(t, TYEN_CAR) && strncmp(t->name, "Parked", 6) && Entity_VarS(t, "TgType", &tg, 0)) {
		Str_Copy(g_Coop.pl[i].fightList, t->name, sizeof(g_Coop.pl[i].fightList));
		g_Coop.pl[i].follow = !Cp_ShipFoe(t) || Cp_Scanned(i, t);
		Cp_TestFight(i, in, back);
		g_Coop.pl[i].follow = 0;
		return;
	}
	if (kind == NETREP_SHIP) {
		/* its ship flies to the target first, then out of it near the place it goes to (a roof pushes a ship off: no
		 * time to wait for a calm one) */
		if (!Cp_FlyTo(i, in, e, t, CP_AUTO_OUT, now)) {
			byte pulse = fmod(now, 0.5) < 1.0 / INSTANCE_TICK_HZ;
			in->buttons |= pulse ? NETREP_IN_SWITCH : 0;
			if (pulse) {
				/* a ship that does not come to rest near its goal (it circles at full speed on -mp2) is stopped: the
				 * switch leaves only a slow ship */
				const vec3_t *v = &Obj(e)->vel;
				double speed = sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
				Con_Printf("coop: player %d (auto) gets out of %s near %s (speed %.0f)\n", i, e->name, t->name, speed);
				if (speed > PLAY_LEAVE_SPEED) {
					vec3_t zero = { 0.0f, 0.0f, 0.0f };
					Obj_SetVel(e, &zero);
				}
			}
		}
		return;
	}
	/* outdoors a target far away: into a ship first (the switch: a parked one near, else its called ship), as a player
	 * flies there; it walks when it gets none */
	float fx = t->pos.x - e->pos.x, fz = t->pos.z - e->pos.z;
	if (Cp_WalkGraph() || Entity_IsType(t, TYEN_CAR) || fx * fx + fz * fz <= CP_AUTO_SHIP * CP_AUTO_SHIP ||
		(g_Coop.pl[i].usedAt > 0.0 && now - g_Coop.pl[i].usedAt < CP_USE_BOARD)) {
		/* (not soon after a use: GetDataPart3's boarding callback starts the data's waves again during the call) */
		g_Coop.pl[i].shipAsk = 0.0;
	} else if (g_Coop.pl[i].shipAsk <= 0.0) {
		g_Coop.pl[i].shipAsk = now;
		Con_Printf("coop: player %d (auto) asks for a ship to %s\n", i, t->name);
	} else if (now - g_Coop.pl[i].shipAsk < CP_SHIP_ASK) {
		in->buttons |= fmod(now, 2.0) < 1.0 / INSTANCE_TICK_HZ ? NETREP_IN_SWITCH : 0;
		return;
	}
	/* a parked ship of the mission (the one to steal): next to it and into it with the switch */
	if (Entity_IsType(t, TYEN_CAR) && !strncmp(t->name, "Parked", 6)) {
		if (strcmp(g_Coop.pl[i].autoAt, t->name) || now - g_Coop.pl[i].autoSince >= CP_AUTO_GAP) {
			Str_Copy(g_Coop.pl[i].autoAt, t->name, sizeof(g_Coop.pl[i].autoAt));
			g_Coop.pl[i].autoSince = now;
			Con_Printf("coop: player %d (auto) boards %s\n", i, t->name);
			Travel_TestGoto(i, t->name, 0.0f, 300.0f);
			Rep_TestSwitch(i, 0);
		}
		return;
	}
	/* a phone cab, a desktop or a character to talk to while the police is after it: it waits for the alarm to fall, as
	 * the single player does after the call's "the police is still after you" (PhoneCab.UseAudioCom, the missions'
	 * UsePhoneCab; ApologizeMayor's mayor only warns while the alarm of a stapler's death is on) */
	char *fn = NULL;
	const cAlarm *al = Players_Alarm(i);
	if (al && al->on && Entity_VarS(t, "UseFunc", &fn, 0) && fn && (strstr(fn, "Phone") || strstr(fn, "AudioCom") ||
		strstr(fn, "Desktop") || strstr(fn, "Speech"))) {
		if (now - g_Coop.pl[i].autoTold >= 10.0) {
			g_Coop.pl[i].autoTold = now;
			Con_Printf("coop: player %d (auto) waits for its alarm to fall before %s\n", i, t->name);
		}
		g_Coop.pl[i].autoUse = 0.0;
		return;
	}
	if (g_Coop.pl[i].autoUse > 0.0 && now >= g_Coop.pl[i].autoUse) {
		/* the use of its arrow, as the test line use (the option its client chose) */
		g_Coop.pl[i].autoUse = 0.0;
		g_Coop.pl[i].autoTurn++;
		cEntity *v = WChar_Victim(e);
		Con_Printf("coop: player %d (auto) %s %s (its arrow on %s)\n", i, WLife(t)->arrow.canPhoto ? "photographs" : "uses",
			t->name, v ? v->name : "nothing");
		if (WLife(t)->arrow.canPhoto) {
			/* a photo is the use button with the arrow on it */
			g_Coop.pl[i].press = NETREP_IN_ACT3;
			g_Coop.pl[i].pressTicks = 12;
			g_Coop.pl[i].pressPulse = 1;
		} else {
			Travel_TestUse(i, t->name, "");
			g_Coop.pl[i].usedAt = now;
		}
	}
	/* not while its photo's press runs (the walk to the next target would turn it away); outdoors (no graph of the
	 * walkers) it walks straight to a target far away */
	if ((g_Coop.pl[i].walk || !Cp_WalkGraph()) && g_Coop.pl[i].pressTicks <= 0 && Cp_WalkStep(i, in, e, t, now)) {
		return;
	}
	if (!strcmp(g_Coop.pl[i].autoAt, t->name) && now - g_Coop.pl[i].autoSince < CP_AUTO_GAP) {
		return;
	}
	Str_Copy(g_Coop.pl[i].autoAt, t->name, sizeof(g_Coop.pl[i].autoAt));
	g_Coop.pl[i].autoSince = now;
	/* a target without a use or a photo (the arrow over an item to take) is walked onto */
	byte use = WLife(t)->arrow.canUse || WLife(t)->arrow.canPhoto;
	g_Coop.pl[i].autoUse = use ? now + 2.0 : 0.0;
	float dx = 0.0f, dz = 0.0f;
	if (use) {
		Cp_Approach(e, t, CP_AUTO_NEAR, &dx, &dz);
	}
	Travel_TestGoto(i, t->name, dx, dz);
}

/* test: player i possesses the character: next to it, facing it, until its arrow is on it (a walking one moves on),
 * then the possession button; given up after CP_POSSESS_SECS */
static void Cp_TestPossess(int i, NetRepInput *in) {
	cEntity *e = Players_Get(i), *c = World_FindEntity(g_Coop.pl[i].possess);
	double now = Instance_Time();
	/* the possessed one goes (the member's new character of its class) */
	if (!c || e == c || now - g_Coop.pl[i].possessSince > CP_POSSESS_SECS) {
		Con_Printf("coop: player %d: the possession of %s %s\n", i, g_Coop.pl[i].possess, !c || e == c ? "done" : "given up");
		g_Coop.pl[i].possess[0] = 0;
		return;
	}
	if (NetPl_Kind(e) != NETREP_CHAR) {
		return;		/* between its old and its new character */
	}
	if (WChar_Victim(e) == c) {
		in->buttons |= fmod(now, 0.5) < 1.0 / INSTANCE_TICK_HZ ? NETREP_IN_ACT2 : 0;
	} else if (now - g_Coop.pl[i].possessAt >= 0.5) {
		g_Coop.pl[i].possessAt = now;
		Travel_TestGoto(i, c->name, 0.0f, 150.0f);
	}
}

void Coop_TestInput(int i, NetRepInput *in, double back) {
	if (i < 0 || i >= PLAYERS_MAX) {
		return;
	}
	if (g_Coop.pl[i].possess[0]) {
		Cp_TestPossess(i, in);
	}
	if (g_Coop.pl[i].race > 0.0f) {
		Cp_TestRace(i);
	}
	if (g_Coop.pl[i].fight) {
		Cp_TestFight(i, in, back);
	} else if (g_Coop.pl[i].autoOn) {
		Cp_TestAuto(i, in, back);
	}
	if (g_Coop.pl[i].pressTicks <= 0) {
		return;
	}
	/* a pulsed press (a negative count of ticks): pressed one tick in every four */
	in->buttons |= !g_Coop.pl[i].pressPulse || g_Coop.pl[i].pressTicks % 4 == 0 ? g_Coop.pl[i].press : 0;
	g_Coop.pl[i].pressTicks--;
}

/* the buttons of a test press: act (the action), act2 (the possession), act3 (the use), lock */
static uint16_t Cp_Buttons(const char *names) {
	static const struct {
		const char *name;
		uint16_t bit;
	} bits[] = { { "act", NETREP_IN_ACT }, { "act2", NETREP_IN_ACT2 }, { "act3", NETREP_IN_ACT3 },
		{ "lock", NETREP_IN_LOCK }, { "boost", NETREP_IN_BOOST }, { "switch", NETREP_IN_SWITCH } };
	uint16_t b = 0;
	for (int k = 0; k < (int)(sizeof(bits) / sizeof(bits[0])); k++) {
		const char *at = strstr(names, bits[k].name);
		size_t n = strlen(bits[k].name);
		while (at && ((at > names && at[-1] != ',') || (at[n] && at[n] != ','))) {
			at = strstr(at + 1, bits[k].name);
		}
		b |= at ? bits[k].bit : 0;
	}
	return b;
}

/* test line "where <prefix>": the place, life, logic target, target type and entity list of the entities whose names
 * start with it (the followers of a walk, a mission's characters) */
static byte Cp_TestWhere(const char *line) {
	char name[CP_NAME];
	if (sscanf(line, "where %63s", name) != 1) {
		return 0;
	}
	size_t n = strlen(name);
	for (cEntity *v = cWorld_GetFirstEntity(g_pWorld); v; v = cEntity_Next(v)) {
		char *tgt = NULL, *tg = NULL, *list = NULL;
		if (strncmp(v->name, name, n)) {
			continue;
		}
		Entity_VarS(v, "LLogicTarget", &tgt, 0);
		Entity_VarS(v, "TgType", &tg, 0);
		Entity_VarS(v, "EntityList", &list, 0);
		Con_Printf("coop: %s at %.0f %.0f %.0f, %s, life %.0f, player %d, logic target %s, target type %s, list %s\n", v->name,
			(double)v->pos.x, (double)v->pos.y, (double)v->pos.z, v->active ? "active" : "inactive",
			Entity_IsType(v, TYEN_WALKCHAR) || Entity_IsType(v, TYEN_CAR) ? (double)WLife(v)->life : 0.0, Players_Of(v),
			tgt ? tgt : "-", tg ? tg : "-", list ? list : "-");
	}
	return 1;
}

byte Coop_Command(const char *line) {
	char name[CP_NAME], val[256];
	int i;
	if (!Coop_On()) {
		return 0;
	}
	if (Cp_TestWhere(line)) {
		return 1;
	}
	if (sscanf(line, "leader %d", &i) == 1) {
		Cp_ToLeader(i);
		return 1;
	}
	if (sscanf(line, "qload %63s", name) == 1 || !strcmp(line, "restart") || !strcmp(line, "theend")) {
		/* as the scripts do: SaveGame.QLoad, SaveGame.Restart (a super deal), the end's credits section */
		int was = Players_Enter(Cp_Player());
		if (!strcmp(line, "theend")) {
			SaveVars_ScriptDelete("");
			Str_Copy(name, "Levels/Menu", sizeof(name));
		}
		byte ok = Mode_Import("SaveGame") && (!strcmp(line, "restart") ? PORT_CALL(SaveGame, Restart, NULL, "") :
			PORT_CALL(SaveGame, QLoad, NULL, "s", name));
		if (!ok) {
			Native_ErrPrint();
		}
		Players_Leave(was);
		return 1;
	}
	if (!strncmp(line, "call ", 5) && line[5] != '@') {
		/* a call of the scripts as a call table line ("Module.Func|arg|..."), for the current player */
		const char *one[2] = { line + 5, NULL };
		int was = Players_Enter(Cp_Player());
		Con_Printf("coop: call %s: %s\n", line + 5, Mode_Calls(NULL, one) ? "done" : "failed");
		Native_ErrPrint();
		Players_Leave(was);
		return 1;
	}
	if (sscanf(line, "arrow %63s", name) == 1) {
		/* test: what the auto player sees of an entity (its arrow, life, radar mark, use function) */
		cEntity *e = World_FindEntity(name);
		if (!e || !Entity_IsType(e, TYEN_WITH_LIFE)) {
			Con_Printf("coop: arrow %s: %s\n", name, e ? "no life" : "none");
		} else {
			Con_Printf("coop: arrow %s: mission %d use %d life %.0f radar %d active %d dead %d use module %s\n", name,
				WLife(e)->arrow.canMission, WLife(e)->arrow.canUse, (double)WLife(e)->life, (int)Obj(e)->radarShow, e->active,
				e->deadNode.target != NULL, cPy_Function_Module(&WLife(e)->arrow.onUse));
		}
		return 1;
	}
	if (sscanf(line, "show %63s", name) == 1 && strchr(name, '.')) {
		/* a variable of a module of the scripts */
		nativeVal_t v, r;
		char *dot = strchr(name, '.');
		*dot = 0;
		Native_SetNone(&v);
		Native_SetNone(&r);
		if (Mode_ModGet(name, dot + 1, &v)) {
			Mode_Repr(&v, &r);
			Con_Printf("coop: %s.%s = %.200s\n", name, dot + 1, r.type == NV_STR ? Native_Text(&r) : "?");
		} else {
			Con_Printf("coop: no %s.%s\n", name, dot + 1);
		}
		Native_ErrClear();
		Native_Free(&v);
		Native_Free(&r);
		return 1;
	}
	if (sscanf(line, "aiv %63s", name) == 1) {
		/* a vehicle's AI: its state and sub state, the point it goes to, its life */
		cEntity *e = World_FindEntity(name);
		cAIVehicle *ai = e && Entity_IsType(e, TYEN_CAR) ? Entity_GetAI(e) : NULL;
		char st[256] = "", in[256] = "";
		vec3_t d, p;
		cAIChar *ac = e && Entity_IsType(e, TYEN_WALKCHAR) ? Entity_GetAI(e) : NULL;
		if (ac) {
			/* a walking character's AI: its state and target */
			Con_Printf("coop: %s char AI state %d intern %d, target %s, at %.0f %.0f %.0f\n", name, ac->state, ac->internState,
				Nto1Node_TargetName(&ac->base.target), (double)e->pos.x, (double)e->pos.y, (double)e->pos.z);
			return 1;
		}
		if (!ai) {
			Con_Printf("coop: %s has no vehicle AI\n", name);
			return 1;
		}
		cAIVehicle_GetStateName(ai, st);
		cAIVehicle_GetInternStateName(ai, in);
		cAIVehicleControl_GetDest(ai->control, &d);
		Entity_GetPos(e, &p);
		double away = sqrt((double)((d.x - p.x) * (d.x - p.x) + (d.y - p.y) * (d.y - p.y) + (d.z - p.z) * (d.z - p.z)));
		Con_Printf("coop: %s AI %s (%d) %s at %.0f %.0f %.0f, to %.0f %.0f %.0f (%.0f away), life %.0f\n", name, st,
			ai->state, in, (double)p.x, (double)p.y, (double)p.z, (double)d.x, (double)d.y, (double)d.z, away,
			(double)WLife(e)->life);
		return 1;
	}
	if (sscanf(line, "kill %63s", name) == 1 && strchr(name, '.')) {
		/* the entities of a list of the scripts (the targets of a mission) die as by a weapon of the member the scripts
		 * act for: their OnDeath runs with it as the winner */
		nativeVal_t list, it;
		char *dot = strchr(name, '.');
		*dot = 0;
		int p = Cp_Player(), n = Mode_ModGet(name, dot + 1, &list) ? list.type == NV_STR ? 1 : Mode_Loop(&list) : -1;
		cEntity *by = p >= 0 ? Players_Get(p) : NULL;
		for (int k = 0; k < n; k++) {
			Native_SetNone(&it);
			cEntity *v = list.type == NV_STR ? World_FindEntity(Native_Text(&list)) : Native_SeqItem(&list, k, &it) &&
				it.type == NV_STR ? World_FindEntity(Native_Text(&it)) : NULL;
			if (v && Entity_IsType(v, TYEN_WITH_LIFE)) {
				if (by) {
					cVehicleAttack_HitBy(&WLife(v)->attack, by->name);
				}
				WLife(v)->life = 0;
				Con_Printf("coop: %s of %s.%s killed by %s\n", v->name, name, dot + 1, by ? by->name : "-");
			}
			Native_Free(&it);
		}
		Native_ErrPrint();
		if (n >= 0) {
			Native_Free(&list);
		}
		return 1;
	}
	int ticks = 3;
	if (sscanf(line, "press %d %63s %d", &i, name, &ticks) >= 2 && i >= 0 && i < PLAYERS_MAX) {
		/* the buttons as the member's client sends them, for some ticks (the game's own input path) */
		g_Coop.pl[i].press = Cp_Buttons(name);
		g_Coop.pl[i].pressTicks = ticks < 0 ? -ticks : ticks;
		g_Coop.pl[i].pressPulse = ticks < 0;
		cEntity *e = Players_Get(i), *v = NetPl_Kind(e) == NETREP_CHAR ? WChar_Victim(e) : NULL;
		Con_Printf("coop: player %d presses %s (%04x) for %d ticks (arrow on %s)\n", i, name, g_Coop.pl[i].press, ticks,
			v ? v->name : "-");
		return 1;
	}
	int on = 1;
	if (sscanf(line, "race %d %d", &i, &on) >= 1 && i >= 0 && i < PLAYERS_MAX) {
		g_Coop.pl[i].race = on ? 1.0f : 0.0f;
		g_Coop.pl[i].raceAt = (vec3_t){ 0.0f, 0.0f, 0.0f };
		Con_Printf("coop: player %d flies the race%s\n", i, on ? "" : ": off");
		return 1;
	}
	name[0] = 0;
	if (sscanf(line, "auto %d %d %63s", &i, &on, name) >= 1 && i >= 0 && i < PLAYERS_MAX) {
		g_Coop.pl[i].autoOn = (byte)(on < 0 ? 0 : on > 2 ? 2 : on);
		Str_Copy(g_Coop.pl[i].autoList, name, sizeof(g_Coop.pl[i].autoList));
		g_Coop.pl[i].autoAt[0] = 0;
		g_Coop.pl[i].autoUse = 0.0;
		g_Coop.pl[i].fightAt[0] = 0;
		Con_Printf("coop: player %d plays the mission's targets%s\n", i, !on ? ": off" : on == 2 ? ": fights only" : "");
		return 1;
	}
	int pct = 0;
	name[0] = 0;
	if (sscanf(line, "walk %d %d %63s", &i, &pct, name) >= 2 && i >= 0 && i < PLAYERS_MAX) {
		g_Coop.pl[i].walk = pct != 0;
		g_Coop.pl[i].follower[0] = 0;
		Str_Copy(g_Coop.pl[i].walkWho, name, sizeof(g_Coop.pl[i].walkWho));
		Con_Printf("coop: player %d %s to its targets\n", i, pct ? "walks" : "goes");
		return 1;
	}
	if (sscanf(line, "waypoint %d %d", &i, &pct) == 2 && i >= 0 && i < PLAYERS_MAX) {
		g_Coop.pl[i].waypoint = pct != 0;
		Con_Printf("coop: player %d %s the waypoint's item first\n", i, pct ? "takes" : "leaves");
		return 1;
	}
	if (sscanf(line, "scan %d %d", &i, &pct) == 2 && i >= 0 && i < PLAYERS_MAX) {
		g_Coop.pl[i].scan = pct != 0;
		Con_Printf("coop: player %d %s the mission's ships that are no foes\n", i, pct ? "scans" : "fights");
		return 1;
	}
	if (sscanf(line, "upto %d %63s %d", &i, name, &pct) == 3 && i >= 0 && i < PLAYERS_MAX) {
		Str_Copy(g_Coop.pl[i].upTo, strcmp(name, "0") ? name : "", sizeof(g_Coop.pl[i].upTo));
		g_Coop.pl[i].upToMax = pct;
		Con_Printf("coop: player %d holds its fire while %s is at %d\n", i, name, pct);
		return 1;
	}
	if (sscanf(line, "spare %d %d", &i, &pct) == 2 && i >= 0 && i < PLAYERS_MAX) {
		g_Coop.pl[i].spare = pct;
		Con_Printf("coop: player %d spares a ship under %d%% of its life\n", i, pct);
		return 1;
	}
	if (sscanf(line, "ship %d", &i) == 1 && i >= 0 && i < PLAYERS_MAX) {
		/* into the nearest parked ship (or a called one) when it is on foot; nothing in a ship */
		cEntity *e = Players_Get(i);
		if (e && NetPl_Kind(e) == NETREP_CHAR) {
			Rep_TestSwitch(i, 0);
		}
		return 1;
	}
	if (sscanf(line, "tough %d %d", &i, &pct) == 2 && i >= 0 && i < PLAYERS_MAX) {
		cEntity *e = Players_Get(i);
		g_Coop.pl[i].tough = pct != 0;
		if (!pct && e && Entity_IsType(e, TYEN_WITH_LIFE)) {
			WLife(e)->god = 0;
		}
		Con_Printf("coop: player %d %s\n", i, pct ? "takes no damage" : "takes damage again");
		return 1;
	}
	if (sscanf(line, "possess %d %63s", &i, name) == 2 && i >= 0 && i < PLAYERS_MAX) {
		Str_Copy(g_Coop.pl[i].possess, name, sizeof(g_Coop.pl[i].possess));
		g_Coop.pl[i].possessSince = Instance_Time();
		g_Coop.pl[i].possessAt = 0.0;
		Con_Printf("coop: player %d goes to possess %s\n", i, name);
		return 1;
	}
	if (sscanf(line, "usenear %d %63s", &i, name) == 2 && i >= 0 && i < PLAYERS_MAX) {
		/* the member next to the nearest entity whose name starts with the text (a conversor, a phone cab), and it uses
		 * its arrow */
		cEntity *e = Players_Get(i), *best = NULL;
		float bestD = 0.0f;
		size_t n = strlen(name);
		for (cEntity *v = e ? cWorld_GetFirstEntity(g_pWorld) : NULL; v; v = cEntity_Next(v)) {
			float dx = v->pos.x - e->pos.x, dz = v->pos.z - e->pos.z, d = dx * dx + dz * dz;
			if (!strncmp(v->name, name, n) && Entity_IsType(v, TYEN_WITH_LIFE) && (!best || d < bestD)) {
				best = v;
				bestD = d;
			}
		}
		if (best) {
			Travel_TestGoto(i, best->name, 0.0f, 250.0f);
			Travel_TestUse(i, best->name, "");
		} else {
			Con_Printf("coop: no %s for player %d\n", name, i);
		}
		return 1;
	}
	if (sscanf(line, "board %d %63s", &i, name) == 2 && i >= 0 && i < PLAYERS_MAX) {
		/* the member next to the nearest parked ship whose model has the text, and into it with its switch */
		cEntity *e = Players_Get(i), *best = NULL;
		float bestD = 0.0f;
		for (cEntity *v = e ? cWorld_GetFirstEntity(g_pWorld) : NULL; v; v = cEntity_Next(v)) {
			char *model = NULL, up[256];
			if (!Entity_IsType(v, TYEN_CAR) || strncmp(v->name, "Parked", 6) || v->deadNode.target ||
				WLife(v)->life <= 0.0f || Players_Of(v) >= 0 || !Entity_VarS(v, "Model", &model, 0) || !model) {
				continue;
			}
			Str_Copy(up, model, sizeof(up));
			crt_strupr(up);
			float dx = v->pos.x - e->pos.x, dz = v->pos.z - e->pos.z, d = dx * dx + dz * dz;
			if (strstr(up, name) && (!best || d < bestD)) {
				best = v;
				bestD = d;
			}
		}
		if (best) {
			Travel_TestGoto(i, best->name, 0.0f, 300.0f);
			Rep_TestSwitch(i, 0);
		} else {
			Con_Printf("coop: no parked ship of %s for player %d\n", name, i);
		}
		return 1;
	}
	name[0] = 0;
	if (sscanf(line, "fight %d %63s", &i, name) >= 1 && i >= 0 && i < PLAYERS_MAX) {
		g_Coop.pl[i].fight = strcmp(name, "0") != 0;
		Str_Copy(g_Coop.pl[i].fightList, g_Coop.pl[i].fight ? name : "", sizeof(g_Coop.pl[i].fightList));
		g_Coop.pl[i].fightAt[0] = 0;
		g_Coop.pl[i].fightSince = 0.0;
		Con_Printf("coop: player %d fights %s\n", i, !g_Coop.pl[i].fight ? "no more" : name[0] ? name : "the mission's targets");
		return 1;
	}
	float dx = 0.0f, dz = 0.0f;
	if (sscanf(line, "target %d %f %f", &i, &dx, &dz) >= 1 || sscanf(line, "usetarget %d", &i) == 1) {
		/* the member onto the mission's target (MissionsFuncs.currentTarget), or it uses the target */
		nativeVal_t t;
		Native_SetNone(&t);
		if (Mode_ModGet("MissionsFuncs", "currentTarget", &t) && t.type == NV_STR) {
			if (line[0] == 'u') {
				Travel_TestUse(i, Native_Text(&t), "");
			} else {
				Travel_TestGoto(i, Native_Text(&t), dx, dz);
			}
		} else {
			Con_Printf("coop: no mission target\n");
		}
		Native_ErrClear();
		Native_Free(&t);
		return 1;
	}
	if (sscanf(line, "travel %d %63s", &i, name) == 2) {
		Con_Printf("coop: player %d travels alone to %s\n", i, name);
		Travel_Go(i, name, "", "", 0);
		return 1;
	}
	if (strncmp(line, "story", 5)) {
		return 0;
	}
	if (sscanf(line, "story set %63s %255[^\n]", name, val) == 2) {
		SaveVar_Set(name, val);
		Con_Printf("coop: %s = %s\n", name, val);
	} else if (sscanf(line, "story inc %63s %d", name, &i) == 2) {
		Con_Printf("coop: %s = %d\n", name, SaveVar_Inc(name, i));
	} else if (!strcmp(line, "story deals")) {
		/* test: the three crazy deals done (the super deal's talk needs them: each deal's variable at its target) */
		for (i = 1; i <= 3; i++) {
			char var[32], tgt[32];
			Str_PrintfQuiet(var, sizeof(var), "CrazyDeal.%d.Var", i);
			Str_PrintfQuiet(tgt, sizeof(tgt), "CrazyDeal.%d.Tgt", i);
			const char *v = SaveVar_Get(var), *t = SaveVar_Get(tgt);
			if (v && t) {
				Str_Copy(name, v, sizeof(name));
				SaveVar_Set(name, t);
				Con_Printf("coop: crazy deal %d: %s = %s\n", i, name, t);
			}
		}
	} else if (sscanf(line, "story hold %d", &i) == 1) {
		g_Coop.hold = (byte)(i != 0);
		Con_Printf("coop: the story changes %s\n", g_Coop.hold ? "wait" : "go");
	} else if (sscanf(line, "story get %63s", name) == 1) {
		const char *v = SaveVar_Get(name);
		Con_Printf("coop: %s %s%s\n", name, v ? "= " : "is not set", v ? v : "");
	} else {
		const char *lib = SaveVar_Get("Mission.Library"), *map = SaveVar_Get("Mission.Map");
		Con_Printf("coop: story %s, %d changes to send, mission %s in %s, scene %s, action set %s\n", g_Coop.synced ?
			"in sync" : "waiting for the coordinator", g_Coop.ndirty, lib ? lib : "-", map ? map : "-", g_Coop.inScene ? "playing" :
			"none", TABLE_AT(g_ActionSets, Input_GetActionSet())->name);
	}
	return 1;
}
