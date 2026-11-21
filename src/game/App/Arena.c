/*
 * The PvP modes of the original on the instance, see include/scrap/arena.h. Not in the original.
 *
 * Member i < ARENA_PLAYERS is player i of the replication (include/scrap/play.h), the bots of DeathMatch with bots
 * follow. The scores, teams, flag and phase live in ar.st (the state the clients get); what only the instance needs
 * (the entities, the ship texts, the times) in ar.sv.
 *
 * -mp2 (include/scrap/repent.h): the state and the events go to each player as records of the presentation interface
 * (include/scrap/pres.h, PRES_O_ARENA), in order with the player's other records; the texts, labels, sounds and fades
 * of the events, the place labels and the count down of a team change run here in the player's context, so their
 * sinks send them to that player alone; the flag, the mail bag and the postman zones are items of the instance that
 * the clients get as replicas, each player's way point an engine variable of its context; the requests come through
 * the UI channel (include/scrap/uichan.h, UI_C_ARENA). The client draws the scores and the mission scorer from the
 * state. The old path keeps its own messages and the client's texts and local items.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/ai.h"
#include "scrap/arena.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/logic.h"
#include "scrap/net2.h"
#include "scrap/netaudit.h"
#include "scrap/netrep.h"
#include "scrap/nodesworld.h"
#include "scrap/play.h"
#include "scrap/platform.h"
#include "scrap/players.h"
#include "scrap/pres.h"
#include "scrap/repent.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/texts.h"
#include "scrap/travel.h"
#include "scrap/vehicleattack.h"
#include "scrap/vehicleentity.h"
#include "scrap/world.h"

#define AR_ZONES 32
#define AR_ENT 64
#define AR_BOT_CHECK 1.5			/* s between the looks of a bot for a new target (NDeathMatchBots.CheckBotState) */
#define AR_BAG_BACK 60.0			/* s a lone bag lies before it goes back (NPostman.AUTO_RESPAWN_FLAG_TIME) */
#define AR_WAIT_SECS 1.0			/* the wait label of a flag touched before the round counts, once a second */
#define AR_BOT_LIFE 100				/* NDeathMatchBots.InitServerMod */
#define AR_TAKE_STEP 3				/* ticks between the looks for a touched flag or zone */

typedef struct ArSv {
	cEntity *e;						/* its ship (compared, read while World_FindEntity(name) is it) */
	char name[AR_ENT];
	double since;					/* the time it lost its ship or joined (s of the instance) */
	double notBefore;				/* no ship before this (a team change) */
	double lastWait;
	byte fire;						/* asked for a ship */
	byte leaving;					/* travels to the next level */
	int spawns;
	int pos, score, winner;			/* the place labels' last place, score and lead (-mp2, Ffa.OnPositionChange) */
	double lastCount;				/* the count down of a team change, its last text (-mp2) */
	char way[ARENA_ZONE + 16];		/* the way point of the player's context (-mp2) */
	char ship[ARENA_SHIP_CFG];		/* the ten lines of its garage ship */
} ArSv;

typedef struct ArZone {
	char key[ARENA_ZONE + 1];
	vec3_t pos;
} ArZone;

static struct {
	int mode;						/* -1 off */
	int fraglimit, timelimit;		/* s, 0 none */
	int endSecs;
	char maps[512];
	byte ready;
	ArenaState st;
	ArSv sv[ARENA_MEMBERS];
	byte dirty;
	uint64_t lastSent;				/* ms */
	double now;
	double startAt, endAt;			/* the round counted from, its end screen ends at */
	ArZone zone[AR_ZONES];
	int nzones, zoneAt;				/* the flag's (FlagHunt) or the bag's target (Postman) */
	vec3_t bagSpawn;
	double bagDropAt;
	uint32_t rnd;
	double nextBotCheck;
	int nbots;
	byte items;						/* the flag's or the bag's item and the zones made (-mp2) */
	char link[AR_ENT + 24];			/* the LinkModel of the bag's item ("" none) */
	struct {
		char desc[ARENA_NAME + 1], ship[32], prof[32];
	} bot[ARENA_BOTS];
} ar = { .mode = -1, .endSecs = ARENA_END_SECS, .zoneAt = -1 };

byte Arena_ParseParam(const char *param, const char *ext) {
	int n = crt_atoi(ext);
	if (!crt_stricmp("mode", param)) {
		int m = Arena_ModeOf(ext);
		if (m < 0) {
			return 0;			/* sandbox, freeroam: the factions' */
		}
		ar.mode = m;
		return 1;
	}
	if (!crt_stricmp("fraglimit", param)) {
		ar.fraglimit = n < 1 ? 1 : n;
		return 1;
	}
	if (!crt_stricmp("timelimit", param)) {
		ar.timelimit = (n < 0 ? 0 : n) * 60;
		return 1;
	}
	if (!crt_stricmp("maps", param)) {
		Str_Copy(ar.maps, ext, sizeof(ar.maps));
		return 1;
	}
	if (!crt_stricmp("endsecs", param)) {
		ar.endSecs = n < 1 ? 1 : n > 120 ? 120 : n;
		return 1;
	}
	return 0;
}

byte Arena_On(void) {
	return g_Instance && ar.mode >= 0;
}

const char *Arena_Maps(void) {
	return ar.maps;
}

const char *Arena_ModeName(void) {
	return ar.mode >= 0 ? g_ArenaModes[ar.mode].arg : NULL;
}

static int Ar_Flags(void) {
	return g_ArenaModes[ar.mode].flags;
}

static uint32_t Ar_Rand(uint32_t n) {
	ar.rnd ^= ar.rnd << 13;
	ar.rnd ^= ar.rnd >> 17;
	ar.rnd ^= ar.rnd << 5;
	return n ? ar.rnd % n : 0;
}

static const char *Ar_TgType(int team) {
	return team == ARENA_TEAM_RED ? "red" : team == ARENA_TEAM_GREEN ? "green" : "killer";
}

static void Ar_SetVarS(cEntity *e, const char *var, const char *value) {
	char *v = (char *)value;
	Entity_VarS(e, var, &v, 1);
}

/* the ship of member k while it is in the world and alive */
static cEntity *Ar_Ship(int k) {
	ArSv *v = &ar.sv[k];
	cEntity *e = v->e && v->name[0] ? World_FindEntity(v->name) : NULL;
	return e && e == v->e && !e->deadNode.target && WLife(e)->life > 0 ? e : NULL;
}

static void Ar_SetShip(int k, cEntity *e) {
	ar.sv[k].e = e;
	Str_Copy(ar.sv[k].name, e ? e->name : "", sizeof(ar.sv[k].name));
}

/* the member of an entity (a player's or a bot's ship), -1 */
static int Ar_MemberOf(cEntity *e) {
	int i = e ? Players_Of(e) : -1;
	if (i >= 0) {
		return i;
	}
	for (int k = ARENA_PLAYERS; e && k < ARENA_MEMBERS; k++) {
		if (ar.st.m[k].used && ar.sv[k].e == e && World_FindEntity(ar.sv[k].name) == e) {
			return k;
		}
	}
	return -1;
}

/* a message of the mode to player i: -mp2 a record of the presentation interface, in order with the texts and sounds
 * of its context, else on the instance connection's reliable channel */
static void Ar_Out(int i, const uint8_t *buf, int n) {
	Net2Host *host = Instance_Host();
	int conn = Rep_PlayerConn(i);
	if (n <= 0 || conn < 0) {
		return;
	}
	if (g_Mp2) {
		Pres_Bytes(PRES_O_ARENA, (uint64_t)1 << i, buf, n);
	} else if (host) {
		Net2_Send(host, conn, REP_CH_RELIABLE, buf, n);
	}
}

static const char *Ar_MemberName(int k) {
	return k >= 0 && k < ARENA_MEMBERS && ar.st.m[k].used ? ar.st.m[k].name : "";
}

/* ---- the texts and sounds of the modes on player i's HUD (-mp2): in its context, the sinks send them to it alone
 * (the original's clients made them: Ffa and the n*.py modes) ---- */

static void Ar_Text(int i, const char *text, double secs, byte label) {
	nativeVal_t v;
	int was = Players_Enter(i);
	Native_SetStr(&v, text);
	if (!(label ? Mode_LabelText(&v, Mode_Time() + secs) : Mode_MsgText(&v, Mode_Time() + secs))) {
		Native_ErrPrint();
	}
	Native_Free(&v);
	Players_Leave(was);
}

/* the way point's sound of the flag (Sound.WaySound for the player or its team, EneWaySound for the others) */
static void Ar_WaySound(int i, byte mine) {
	int was = Players_Enter(i);
	if (!Mode_Import("Sound") || !(mine ? Mode_Sound("WaySound", "WayVolume") : Mode_Sound("EneWaySound", "EneWayVolume"))) {
		Native_ErrPrint();
	}
	Players_Leave(was);
}

/* Ffa.EndOfGame and the next round: the screen darker and back (the client adds its noise) */
static void Ar_Fade(int i, byte dark) {
	static const int clear[4] = { 0, 0, 0, 0 }, half[4] = { 0, 0, 0, 128 };
	int was = Players_Enter(i);
	if (!Mode_Fade(0, dark ? clear : half, dark ? half : clear, dark ? 1.0 : 0.5)) {
		Native_ErrPrint();
	}
	Players_Leave(was);
}

/* the texts, sounds and fades of an event for every player */
static void Ar_Tell(int ev, int a, int b, int num) {
	char text[ARENA_TEXT];
	const ArenaState *s = &ar.st;
	byte team = (Ar_Flags() & ARENA_F_TEAM) != 0;
	(void)num;
	for (int i = 0; i < ARENA_PLAYERS; i++) {
		if (!s->m[i].used || Rep_PlayerConn(i) < 0) {
			continue;
		}
		byte ours = a >= 0 && (a == i || (team && s->m[a].team == s->m[i].team));
		switch (ev) {
		case ARENA_EV_KILL:
			/* the dead player's own text is on its scores (the client's, from the event) */
			if (a == i && b >= 0 && a != b) {
				Arena_Lang(text, sizeof(text), "Net_Msg_You_Kill", Ar_MemberName(b), NULL);
				Ar_Text(i, text, 5.0, 0);
			}
			break;
		case ARENA_EV_JOIN:
			/* Ffa.PlayerJoinGame */
			Texts_Fill(text, sizeof(text), s->m[a].team == ARENA_TEAM_RED ? "%s joins red team." : s->m[a].team ==
				ARENA_TEAM_GREEN ? "%s joins green team." : "%s joins the game.", 0, Ar_MemberName(a), NULL);
			Ar_Text(i, text, 2.5, 0);
			break;
		case ARENA_EV_LEAVE:
		case ARENA_EV_SPECT:
			Texts_Fill(text, sizeof(text), ev == ARENA_EV_LEAVE ? "%s leaves the match." : "%s becomes spectator.", 0,
				Ar_MemberName(a), NULL);
			Ar_Text(i, text, 2.5, 0);
			break;
		case ARENA_EV_FLAG:
			Ar_WaySound(i, ours);
			/* NPostman.TakeFlag: the label of the carrier, of its team, of the enemies (team modes) */
			if (Ar_Flags() & ARENA_F_POST) {
				const char *key = a == i ? "Net_Msg_You_HaveFlag" : ours ? "Net_Msg_Your_TeamFlag" : team ?
					"Net_Msg_Enemy_TeamFlag" : NULL;
				if (key) {
					Ar_Text(i, Script_Lang(key), 5.0, 0);
				}
			}
			break;
		case ARENA_EV_DROP:
			Ar_WaySound(i, 0);
			break;
		case ARENA_EV_BACK:
			Ar_Text(i, Script_Lang("MP_Arena_BagBack"), 3.0, 0);
			break;
		case ARENA_EV_DELIVER:
			Ar_WaySound(i, ours);
			Arena_Lang(text, sizeof(text), "MP_Arena_Delivered", Ar_MemberName(a), NULL);
			Ar_Text(i, text, 3.0, 0);
			break;
		case ARENA_EV_END:
		case ARENA_EV_START:
			Ar_Fade(i, ev == ARENA_EV_END);
			break;
		default:
			break;
		}
	}
}

static void Ar_SendState(byte now);

/* an event to every player (a changed state first, so the client knows the names and teams of the event) */
static void Ar_Event(int ev, int a, int b, int num) {
	uint8_t buf[16];
	int n = Arena_WriteEvent(buf, sizeof(buf), ev, a, b, num);
	Ar_SendState(1);
	for (int i = 0; i < ARENA_PLAYERS; i++) {
		Ar_Out(i, buf, n);
	}
	if (g_Mp2) {
		Ar_Tell(ev, a, b, num);
	}
	Con_Printf("arena: event %d %d %d %d\n", ev, a, b, num);
}

/* ---- the level ---- */

static float Ar_Num(const nativeVal_t *v) {
	return v->type == NV_FLOAT ? (float)v->f : v->type == NV_INT ? (float)v->i : 0.0f;
}

/* a free spawn point of the ships (OutMap.GetFreeSpawn: the DM_Spawn dummies, the first free one) */
static byte Ar_FreeSpawn(vec3_t *p, float *yaw) {
	nativeVal_t out, at, c, a;
	Native_SetNone(&out);
	Native_SetNone(&at);
	Native_SetNone(&a);
	byte ok = Mode_Import("OutMap") && PORT_CALL(OutMap, GetFreeSpawn, &out, "") && Native_SeqItem(&out, 0, &at) &&
		Native_SeqItem(&out, 1, &a);
	for (int k = 0; ok && k < 3; k++) {
		Native_SetNone(&c);
		ok = Native_SeqItem(&at, k, &c);
		(&p->x)[k] = Ar_Num(&c);
		Native_Free(&c);
	}
	*yaw = ok ? Ar_Num(&a) : 0.0f;
	if (!ok) {
		Native_ErrPrint();
	}
	Native_Free(&out);
	Native_Free(&at);
	Native_Free(&a);
	return ok;
}

static void Ar_Zones(const char *pattern, int prefix) {
	char name[64];
	vec3_t p, ang;
	ar.nzones = 0;
	DummySearch_Start(pattern, 1);
	while (ar.nzones < AR_ZONES && DummySearch_NextEuler(name, sizeof(name), &p, &ang)) {
		ArZone *z = &ar.zone[ar.nzones++];
		Str_Copy(z->key, (int)strlen(name) > prefix ? name + prefix : name, sizeof(z->key));
		z->pos = p;
	}
	Con_Printf("arena: %d zones of %s\n", ar.nzones, pattern);
}

/* the next zone of the flag or the bag: random, not the one before (GetRndFlag, GetRndZone) */
static void Ar_PickZone(void) {
	int z = (int)Ar_Rand((uint32_t)ar.nzones);
	if (ar.nzones > 1 && z == ar.zoneAt) {
		z = (z + 1 + (int)Ar_Rand((uint32_t)ar.nzones - 1)) % ar.nzones;
	}
	ar.zoneAt = ar.nzones ? z : -1;
	Str_Copy(ar.st.zone, ar.zoneAt >= 0 ? ar.zone[ar.zoneAt].key : "", sizeof(ar.st.zone));
	ar.dirty = 1;
}

static void Ar_FlagAt(const vec3_t *p) {
	ar.st.flagPos[0] = (int32_t)p->x;
	ar.st.flagPos[1] = (int32_t)p->y;
	ar.st.flagPos[2] = (int32_t)p->z;
	ar.dirty = 1;
}

/* the bag lies at its start, nobody carries it, no zone */
static void Ar_BagHome(void) {
	ar.st.flag = ARENA_FL_LIES;
	ar.st.carrier = 0;
	ar.zoneAt = -1;
	ar.st.zone[0] = 0;
	ar.bagDropAt = 0.0;
	Ar_FlagAt(&ar.bagSpawn);
}

byte Arena_Module(int mode) {
	char path[64];
	const char *m = g_ArenaModes[mode].module;
	Str_PrintfQuiet(path, sizeof(path), "Net/%s.pyc", m);
	if (!PORT_CALL(Scrap, PreloadLibrary, NULL, "ss", m, path) || !Mode_Import(m)) {
		Native_ErrPrint();
		return 0;
	}
	return 1;
}

/* the bots of the mode's module (NDeathMatchBots.BotsList: (descriptor, ship, AI profile)) and their profiles: the
 * one of the ship with the attributes of the profile, no countermeasures, a wide attack angle */
static void Ar_LoadBots(void) {
	nativeVal_t list, bot, it[3], prof;
	const char *m = g_ArenaModes[ar.mode].module;
	Native_SetNone(&list);
	/* the AI of the ships needs the level's graph of AI nodes (the small arenas have none) */
	if (!NodesWorld_IsCreated(g_NodesWorld)) {
		Con_Printf("arena: %s has no AI nodes: no bots\n", Instance_Level());
		return;
	}
	if (!Arena_Module(ar.mode) || !Mode_Import("VehiclesNPC") || !Mode_G(m, "BotsList", &list)) {
		Native_ErrPrint();
		return;
	}
	int n = Native_Size(&list);
	for (int i = 0; i < n && ar.nbots < ARENA_BOTS; i++) {
		Native_SetNone(&bot);
		Native_SetNone(&prof);
		for (int k = 0; k < 3; k++) {
			Native_SetNone(&it[k]);
		}
		byte ok = Native_SeqItem(&list, i, &bot) && Native_SeqItem(&bot, 0, &it[0]) && Native_SeqItem(&bot, 1, &it[1]) &&
			Native_SeqItem(&bot, 2, &it[2]) && it[0].type == NV_STR && it[1].type == NV_STR && it[2].type == NV_STR;
		ok = ok && PORT_CALL(VehiclesNPC, GetIAProf, &prof, "v", &it[1]) &&
			PORT_CALL(VehiclesNPC, SetAIProfile, NULL, "vv", &prof, &it[2]) && Mode_VSetI(&prof, "CadenceShootCM", 0) &&
			Mode_VSetF(&prof, "AngAttack", 3.14) && Mode_ModDictPut("VehiclesNPC", "AIProf", &it[2], &prof);
		if (ok) {
			Str_Copy(ar.bot[ar.nbots].desc, Native_Text(&it[0]), sizeof(ar.bot[0].desc));
			Str_Copy(ar.bot[ar.nbots].ship, Native_Text(&it[1]), sizeof(ar.bot[0].ship));
			Str_Copy(ar.bot[ar.nbots].prof, Native_Text(&it[2]), sizeof(ar.bot[0].prof));
			ar.nbots++;
		} else {
			Native_ErrPrint();
		}
		Native_Free(&bot);
		Native_Free(&prof);
		for (int k = 0; k < 3; k++) {
			Native_Free(&it[k]);
		}
	}
	Native_Free(&list);
	Con_Printf("arena: %d bots\n", ar.nbots);
}

/* bot b's ship (VehiclesNPC.MakeAI of its profile) at a free spawn point with 100 life and the last weapon slot */
static void Ar_MakeBot(int b) {
	int k = ARENA_PLAYERS + b;
	ArSv *v = &ar.sv[k];
	nativeVal_t ship;
	char name[AR_ENT], wl[128];
	vec3_t p;
	float yaw;

	Native_SetNone(&ship);
	Str_PrintfQuiet(name, sizeof(name), "ArenaBot%d_%d", b, ++v->spawns);
	cEntity *e = NULL;
	if (PORT_CALL(VehiclesNPC, MakeAI, &ship, "ssi", ar.bot[b].prof, name, 0L) && ship.type == NV_ENT) {
		e = Native_Ent(&ship);
	} else {
		Native_ErrPrint();
	}
	Native_Free(&ship);
	if (!e) {
		return;
	}
	int life = AR_BOT_LIFE;
	Entity_VarI(e, "Life", &life, 1);
	Entity_VarI(e, "MinLife", &life, 1);
	Entity_VarI(e, "MaxLife", &life, 1);
	Ar_SetVarS(e, "Descriptor", ar.bot[b].desc);
	Ar_SetVarS(e, "TgType", Ar_TgType(ar.st.m[k].team));
	/* WeapList[:-1] + "1" */
	char *s = NULL;
	if (Entity_VarS(e, "WeapList", &s, 0) && s && *s) {
		Str_Copy(wl, s, sizeof(wl));
		wl[strlen(wl) - 1] = '1';
		Ar_SetVarS(e, "WeapList", wl);
	}
	if (Ar_FreeSpawn(&p, &yaw)) {
		vec3_t ang = { 0.0f, yaw, 0.0f };
		e->pos = p;
		Obj_SetAng(e, &ang);
	}
	Ar_SetShip(k, e);
	ar.st.m[k].flags |= ARENA_MF_SHIP;
	ar.dirty = 1;
	Con_Printf("arena: bot %d (%s) in %s\n", b, ar.bot[b].desc, e->name);
}

static void Ar_AddBots(void) {
	for (int b = 0; b < ar.nbots; b++) {
		ArenaMember *m = &ar.st.m[ARENA_PLAYERS + b];
		memset(m, 0, sizeof(*m));
		m->used = 1;
		m->flags = ARENA_MF_BOT | ARENA_MF_JOINED;
		m->team = (uint8_t)ArenaRules_JoinTeam(&ar.st, ARENA_TEAM_RED, ARENA_PLAYERS + b);
		Str_Copy(m->name, ar.bot[b].desc, sizeof(m->name));
		Ar_MakeBot(b);
	}
}

static void Ar_Ready(void) {
	ar.ready = 1;
	ar.rnd = (uint32_t)Plat_RealNs() | 1u;
	ar.st.mode = (uint8_t)ar.mode;
	ar.st.fraglimit = ar.fraglimit ? ar.fraglimit : (int)GVar_GetNum("Fraglimit", 0);
	ar.st.timeLeft = ar.timelimit ? ar.timelimit : -1;
	ar.st.you = ARENA_NOBODY;
	ar.st.round = 1;
	Arena_NextLevel(ar.maps, Instance_Level(), ar.st.next, sizeof(ar.st.next));
	if ((Ar_Flags() & (ARENA_F_FLAG | ARENA_F_POST)) && g_Mp2 && !Arena_Module(ar.mode)) {
		Con_Errorf("arena: no module %s: no flag models\n", g_ArenaModes[ar.mode].module);
	}
	if (Ar_Flags() & ARENA_F_FLAG) {
		Ar_Zones("DM_Multiplayer_Flag_*", 19);
		Ar_PickZone();
		if (ar.zoneAt >= 0) {
			ar.st.flag = ARENA_FL_LIES;
			Ar_FlagAt(&ar.zone[ar.zoneAt].pos);
		}
	} else if (Ar_Flags() & ARENA_F_POST) {
		char name[64];
		vec3_t ang;
		DummySearch_Start("DM_Domination_Flag", 0);
		if (DummySearch_NextEuler(name, sizeof(name), &ar.bagSpawn, &ang)) {
			Ar_Zones("DM_Domination_Zone_*", 19);
			Ar_BagHome();
		} else {
			Con_Errorf("arena: %s has no DM_Domination_Flag for %s\n", Instance_Level(), g_ArenaModes[ar.mode].type);
		}
	}
	if (Ar_Flags() & ARENA_F_BOTS) {
		Ar_LoadBots();
		Ar_AddBots();
	}
	Con_Printf("arena: %s on %s, frag limit %d, time limit %d s, next %s\n", g_ArenaModes[ar.mode].type,
		Instance_Level(), ar.st.fraglimit, ar.timelimit, ar.st.next);
	ar.dirty = 1;
}

/* ---- the players ---- */

void Arena_Joined(int i) {
	if (!Arena_On() || i < 0 || i >= ARENA_PLAYERS) {
		return;
	}
	ArenaMember *m = &ar.st.m[i];
	memset(m, 0, sizeof(*m));
	memset(&ar.sv[i], 0, sizeof(ar.sv[i]));
	m->used = 1;
	Str_PrintfQuiet(m->name, sizeof(m->name), "Player %d", i + 1);
	ar.sv[i].since = ar.now;
	ar.sv[i].pos = 1;
	ar.dirty = 1;
}

/* the bag drops where its carrier k was (NPostman.ServerDropFlag) */
static void Ar_Drop(int k, const vec3_t *at) {
	if (!(Ar_Flags() & ARENA_F_POST) || ar.st.carrier != k + 1) {
		return;
	}
	ar.st.flag = ARENA_FL_LIES;
	ar.st.carrier = 0;
	ar.zoneAt = -1;
	ar.st.zone[0] = 0;
	ar.bagDropAt = ar.now;
	Ar_FlagAt(at);
	Ar_Event(ARENA_EV_DROP, k, -1, 0);
}

/* the place of member k's ship now, else where the state last had the bag */
static vec3_t Ar_Where(int k) {
	cEntity *e = Ar_Ship(k);
	if (!e && k < ARENA_PLAYERS) {
		e = Players_Get(k);
	}
	return e ? e->pos : (vec3_t){ (float)ar.st.flagPos[0], (float)ar.st.flagPos[1], (float)ar.st.flagPos[2] };
}

void Arena_Left(int i) {
	if (!Arena_On() || i < 0 || i >= ARENA_PLAYERS || !ar.st.m[i].used) {
		return;
	}
	vec3_t at = Ar_Where(i);
	Ar_Drop(i, &at);
	Ar_Event(ARENA_EV_LEAVE, i, -1, 0);
	memset(&ar.st.m[i], 0, sizeof(ar.st.m[i]));
	memset(&ar.sv[i], 0, sizeof(ar.sv[i]));
	ar.dirty = 1;
}

/* the ten lines of a garage ship (Ffa.BuildCfgString): model, max life, name, pilot, the four motors, the weapons, the
 * team; 0 when it has not all of them */
static byte Ar_ShipLines(const char *text, char lines[10][64]) {
	int n = 0;
	for (const char *p = text; n < 10;) {
		const char *e = strchr(p, '\n');
		int len = e ? (int)(e - p) : (int)strlen(p);
		Str_PrintfQuiet(lines[n++], 64, "%.*s", len > 63 ? 63 : len, p);
		if (!e) {
			break;
		}
		p = e + 1;
	}
	return n == 10 && lines[0][0];
}

/* the garage ship of player i on the ship entity name (Vehicles.SetProperties, as Ffa.ClientSetConfig sets it up),
 * the original's default ship when that is no valid ship */
static byte Ar_SetUp(int i, const char *name) {
	char l[10][64];
	nativeVal_t out;
	Native_SetNone(&out);
	byte ok = Mode_Import("Vehicles");
	if (ok && Ar_ShipLines(ar.sv[i].ship, l)) {
		ok = PORT_CALL(Vehicles, SetProperties, &out, "ssisssssss", name, l[0], (long)crt_atoi(l[1]), l[3], l[4], l[5],
			l[6], l[7], l[8], ar.st.m[i].name) && out.type == NV_ENT;
		if (!ok) {
			Native_ErrPrint();
			Con_Printf("arena: player %d's ship (%s) is no valid ship: the default\n", i, l[0]);
		}
		Native_Free(&out);
		if (ok) {
			return 1;
		}
	}
	/* the defaults of the engine variables of the garage ship (PlayerModel, PilotModel, ...) */
	Native_SetNone(&out);
	ok = PORT_CALL(Vehicles, SetProperties, &out, "ssisssssss", name, "SBetty1", 80L, "P_Betty", "MBetty1", "", "", "",
		"1,1,0,1,3,0", ar.st.m[i].name) && out.type == NV_ENT;
	if (!ok) {
		Native_ErrPrint();
	}
	Native_Free(&out);
	return ok;
}

cEntity *Arena_MakeShip(int i, const char *name) {
	vec3_t p = { 0.0f, 0.0f, 0.0f };
	float yaw = 0.0f;
	int zero = 0;

	Ar_FreeSpawn(&p, &yaw);
	cEntity *e = cWorld_CreateEntity(g_pWorld, name, p.x, p.y, p.z, TYEN_CAR);
	if (!e) {
		return NULL;
	}
	if (!Ar_SetUp(i, name) || World_FindEntity(name) != e) {
		if (World_FindEntity(name) == e) {
			cEntity_Del(e);
		}
		return NULL;
	}
	vec3_t ang = { 0.0f, yaw, 0.0f };
	Obj_SetAng(e, &ang);
	Obj(e)->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
	Entity_VarI(e, "ArrowCanUse", &zero, 1);
	Ar_SetVarS(e, "UseFunc", "");
	Ar_SetVarS(e, "TgType", Ar_TgType(ar.st.m[i].team));
	Ar_SetVarS(e, "Descriptor", ar.st.m[i].name);
	Entity_SetActive(e, 1);
	Ar_SetShip(i, e);
	ar.sv[i].fire = 0;
	ar.sv[i].spawns++;
	ar.st.m[i].flags |= ARENA_MF_SHIP;
	ar.dirty = 1;
	Con_Printf("arena: player %d (%s) in %s at %.0f %.0f %.0f\n", i, ar.st.m[i].name, name, (double)p.x, (double)p.y,
		(double)p.z);
	return e;
}

byte Arena_SpawnDue(int i, uint64_t ticks) {
	if (!Arena_On() || i < 0 || i >= ARENA_PLAYERS) {
		return 0;
	}
	const ArenaMember *m = &ar.st.m[i];
	const ArSv *v = &ar.sv[i];
	if (!ar.ready || !m->used || !(m->flags & ARENA_MF_JOINED) || (m->flags & ARENA_MF_SPECT) ||
		ar.st.phase == ARENA_PH_END || v->leaving || ar.now < v->notBefore) {
		return 0;
	}
	double secs = (double)ticks / INSTANCE_TICK_HZ;
	return (v->fire && secs >= ARENA_RESPAWN_SECS) || secs >= ARENA_AUTO_SECS;
}

static void Ar_End(void);

/* member k lost its ship (killed by killer, -1 none): the scores, the bag, the end */
static void Ar_Death(int k, int killer) {
	ArenaMember *m = &ar.st.m[k];
	vec3_t at = Ar_Where(k);
	m->flags &= (uint8_t)~ARENA_MF_SHIP;
	ar.sv[k].since = ar.now;
	ar.sv[k].fire = 0;
	Ar_Drop(k, &at);
	if (ar.st.phase == ARENA_PH_END) {
		return;
	}
	ArenaRules_Kill(&ar.st, k, killer);
	ar.dirty = 1;
	Ar_Event(ARENA_EV_KILL, killer, k, 0);
	Con_Printf("arena: %s killed by %s, scores %d / %d, teams %d / %d\n", m->name,
		killer >= 0 ? ar.st.m[killer].name : "-", m->score, killer >= 0 ? ar.st.m[killer].score : 0, ar.st.red,
		ar.st.green);
	if (ArenaRules_Ended(&ar.st)) {
		Ar_End();
	}
}

/* the member whose weapon hit e last, -1 */
static int Ar_Killer(cEntity *e) {
	cEntity *by = WLife(e)->attack.hitBy.target ? World_FindEntity(Nto1Node_TargetName(&WLife(e)->attack.hitBy)) : NULL;
	return by && by != e ? Ar_MemberOf(by) : -1;
}

void Arena_Died(int i, cEntity *e) {
	if (!Arena_On() || i < 0 || i >= ARENA_PLAYERS || !ar.st.m[i].used) {
		return;
	}
	Ar_Death(i, e ? Ar_Killer(e) : -1);
}

/* player i's ship goes without a death (spectate, a team change, the end of the round, the travel) */
static void Ar_Remove(int i) {
	vec3_t at = Ar_Where(i);
	Ar_Drop(i, &at);
	if (Players_Get(i)) {
		Rep_Remove(i);
	}
	ar.st.m[i].flags &= (uint8_t)~ARENA_MF_SHIP;
	ar.sv[i].since = ar.now;
	ar.sv[i].fire = 0;
	ar.dirty = 1;
}

static void Ar_Name(char *out, int cap, const char *in) {
	int n = 0;
	for (const char *p = in; *p && n < cap - 1; p++) {
		if ((unsigned char)*p >= 32 && *p != 127) {
			out[n++] = *p;
		}
	}
	out[n] = 0;
}

static void Ar_Request(int i, int rq, const char *name, int team, const char *ship) {
	ArenaMember *m = &ar.st.m[i];
	ArSv *v = &ar.sv[i];
	switch (rq) {
	case ARENA_RQ_JOIN:
		if (m->flags & ARENA_MF_JOINED) {
			break;
		}
		/* the name of its ticket in a coordinator's world, else the one it sent */
		char tk[ARENA_NAME + 1];
		Ar_Name(m->name, sizeof(m->name), Travel_Name(i, tk, sizeof(tk)) ? tk : name);
		if (!m->name[0]) {
			Str_PrintfQuiet(m->name, sizeof(m->name), "Player %d", i + 1);
		}
		m->team = (uint8_t)ArenaRules_JoinTeam(&ar.st, team, i);
		m->flags |= ARENA_MF_JOINED;
		Str_Copy(v->ship, ship, sizeof(v->ship));
		v->since = ar.now;
		ar.dirty = 1;
		Ar_Event(ARENA_EV_JOIN, i, -1, 0);
		Con_Printf("arena: player %d joins as %s, team %d\n", i, m->name, m->team);
		break;
	case ARENA_RQ_RESPAWN:
		v->fire = 1;
		break;
	case ARENA_RQ_SPECT:
		if (!(m->flags & ARENA_MF_JOINED)) {
			break;
		}
		if (m->flags & ARENA_MF_SPECT) {
			/* Ffa.ServerRemoveSpectatorState: back with 0 points */
			m->flags &= (uint8_t)~ARENA_MF_SPECT;
			m->score = 0;
			v->since = ar.now;
			Ar_Event(ARENA_EV_JOIN, i, -1, 0);
		} else {
			Ar_Remove(i);
			m->flags |= ARENA_MF_SPECT;
			m->score = 0;
			Ar_Event(ARENA_EV_SPECT, i, -1, 0);
		}
		ar.dirty = 1;
		break;
	case ARENA_RQ_TEAM:
		if (!(Ar_Flags() & ARENA_F_TEAM) || !(m->flags & ARENA_MF_JOINED) ||
			(team != ARENA_TEAM_RED && team != ARENA_TEAM_GREEN) || (team == m->team && !(m->flags & ARENA_MF_SPECT))) {
			break;
		}
		/* Ffa.ReJoinGame: out of the game, the new team, back after the count down with 0 points */
		Ar_Remove(i);
		m->team = (uint8_t)team;
		m->flags &= (uint8_t)~ARENA_MF_SPECT;
		m->score = 0;
		v->notBefore = ar.now + ARENA_REJOIN_SECS;
		v->lastCount = 0.0;
		v->fire = 1;
		ar.dirty = 1;
		Ar_Event(ARENA_EV_JOIN, i, -1, (int)ARENA_REJOIN_SECS);
		break;
	default:
		break;
	}
}

byte Arena_Message(Net2Host *host, int conn, const uint8_t *data, int size) {
	char name[ARENA_NAME + 1], ship[ARENA_SHIP_CFG];
	int rq, team;
	(void)host;
	if (size < 1 || data[0] != ARENA_MSG_REQ) {
		return 0;
	}
	int i = Rep_ConnPlayer(conn);
	if (!Arena_On() || i < 0 || i >= ARENA_PLAYERS || !ar.st.m[i].used ||
		!Arena_ReadRequest(data, size, &rq, name, sizeof(name), &team, ship, sizeof(ship))) {
		return 1;
	}
	int was = Players_Enter(i);
	Ar_Request(i, rq, name, team, ship);
	Players_Leave(was);
	return 1;
}

void Arena_UiRequest(int i, const uint8_t *data, int size) {
	static const char *const rqs[4] = { "join", "respawn", "spect", "team" };
	char name[ARENA_NAME + 1], ship[ARENA_SHIP_CFG];
	int rq, team;
	if (!Arena_On() || i < 0 || i >= ARENA_PLAYERS || !ar.st.m[i].used ||
		!Arena_ReadRequest(data, size, &rq, name, sizeof(name), &team, ship, sizeof(ship))) {
		NetAudit_Func(NA_HUD, "ui.arena:", "bad");
		return;
	}
	NetAudit_Func(NA_HUD, "ui.arena:", rqs[rq]);
	int was = Players_Enter(i);
	Ar_Request(i, rq, name, team, ship);
	Players_Leave(was);
}

/* ---- the round ---- */

static void Ar_End(void) {
	ar.st.phase = ARENA_PH_END;
	ar.endAt = ar.now + ar.endSecs;
	ar.st.endLeft = ar.endSecs;
	for (int k = 0; k < ARENA_MEMBERS; k++) {
		if (!ar.st.m[k].used) {
			continue;
		}
		if (k < ARENA_PLAYERS) {
			int was = Players_Enter(k);
			Ar_Remove(k);
			Players_Leave(was);
		} else {
			cEntity *e = Ar_Ship(k);
			if (e) {
				cEntity_Del(e);
			}
			Ar_SetShip(k, NULL);
			ar.st.m[k].flags &= (uint8_t)~ARENA_MF_SHIP;
		}
	}
	ar.dirty = 1;
	Ar_Event(ARENA_EV_END, -1, -1, ar.endSecs);
	Con_Printf("arena: round %d ends on %s, next %s\n", ar.st.round, Instance_Level(), ar.st.next);
}

/* the scores, the totals, the flag and the time of a new round */
static void Ar_Reset(void) {
	for (int k = 0; k < ARENA_MEMBERS; k++) {
		ar.st.m[k].score = 0;
		ar.sv[k].since = ar.now - ARENA_AUTO_SECS;	/* the players' ships at once */
		ar.sv[k].notBefore = 0.0;
	}
	ar.st.red = ar.st.green = 0;
	ar.st.phase = ARENA_PH_WAIT;
	ar.st.started = 0;
	ar.st.endLeft = 0;
	ar.st.timeLeft = ar.timelimit ? ar.timelimit : -1;
	ar.st.round++;
	if (Ar_Flags() & ARENA_F_FLAG) {
		Ar_PickZone();
		if (ar.zoneAt >= 0) {
			Ar_FlagAt(&ar.zone[ar.zoneAt].pos);
		}
	} else if (Ar_Flags() & ARENA_F_POST) {
		Ar_BagHome();
	}
	for (int b = 0; b < ar.nbots; b++) {
		Ar_MakeBot(b);
	}
	ar.dirty = 1;
	Ar_Event(ARENA_EV_START, -1, -1, ar.st.round);
}

/* after the end screen: the next level for all (a coordinator's world), else the round again here */
static void Ar_NextRound(void) {
	byte go = Travel_Coordinated() && crt_stricmp(ar.st.next, Instance_Level()) != 0;
	for (int i = 0; go && i < ARENA_PLAYERS; i++) {
		if (ar.st.m[i].used && Travel_Send(i, ar.st.next)) {
			ar.sv[i].leaving = 1;
		}
	}
	Con_Printf("arena: %s\n", go ? "the players go to the next level" : "the round starts again");
	Ar_Reset();
}

/* the bots: a dead one is reborn after ARENA_BOT_SECS, one without a target attacks a random other ship */
static void Ar_Bots(void) {
	byte look = ar.now >= ar.nextBotCheck;
	if (look) {
		ar.nextBotCheck = ar.now + AR_BOT_CHECK;
	}
	for (int b = 0; b < ar.nbots; b++) {
		int k = ARENA_PLAYERS + b;
		ArSv *v = &ar.sv[k];
		cEntity *e = v->e ? World_FindEntity(v->name) : NULL;
		if (e != v->e) {
			e = NULL;
		}
		if (v->e && (!e || e->deadNode.target || WLife(e)->life <= 0)) {
			/* dead: the game's death runs on, the score counts */
			int killer = e ? Ar_Killer(e) : -1;
			Ar_SetShip(k, NULL);
			Ar_Death(k, killer);
			continue;
		}
		if (!v->e) {
			if (ar.now - v->since >= ARENA_BOT_SECS && ar.st.phase != ARENA_PH_END) {
				Ar_MakeBot(b);
			}
			continue;
		}
		cAIVehicle *ai = Entity_GetAI(e);
		int state = ai ? cAIVehicle_GetState(ai) : -1;
		if (!look || !ai || (state != AIV_FREEZE && state != AIV_IN_ROUTE && AIMove_GetTargetEnt(ai))) {
			continue;
		}
		/* NDeathMatchBots.SetBotState: a random other active ship */
		cEntity *pick[ARENA_MEMBERS];
		int n = 0;
		for (int j = 0; j < ARENA_MEMBERS; j++) {
			cEntity *o = j == k ? NULL : j < ARENA_PLAYERS ? Players_Get(j) : Ar_Ship(j);
			if (o && ar.st.m[j].used && WLife(o)->life > 0) {
				pick[n++] = o;
			}
		}
		if (n) {
			cAIVehicle_SetState(ai, AIV_STATE_ATTACK, pick[Ar_Rand((uint32_t)n)]);
		}
	}
}

/* the members' ships and slots in the state */
static void Ar_Members(void) {
	for (int k = 0; k < ARENA_MEMBERS; k++) {
		ArenaMember *m = &ar.st.m[k];
		if (!m->used) {
			continue;
		}
		cEntity *e = k < ARENA_PLAYERS ? Players_Get(k) : Ar_Ship(k);
		int s = e ? Rep_SlotOf(e) : -1;
		uint16_t slot = (uint16_t)(s + 1);
		uint8_t flags = (uint8_t)((m->flags & ~ARENA_MF_SHIP) | (e ? ARENA_MF_SHIP : 0));
		if (slot != m->slot || flags != m->flags) {
			m->slot = slot;
			m->flags = flags;
			ar.dirty = 1;
		}
	}
}

static float Ar_DistSq(const cEntity *e, const vec3_t *p) {
	float dx = e->pos.x - p->x, dy = e->pos.y - p->y, dz = e->pos.z - p->z;
	return dx * dx + dy * dy + dz * dz;
}

/* member k touched the flag or the bag before the round counts (Net_Label_Waiting_Challenge) */
static byte Ar_TooEarly(int k) {
	if (ArenaRules_Started(&ar.st)) {
		return 0;
	}
	if (k < ARENA_PLAYERS && ar.now - ar.sv[k].lastWait >= AR_WAIT_SECS) {
		uint8_t buf[16];
		ar.sv[k].lastWait = ar.now;
		Ar_Out(k, buf, Arena_WriteEvent(buf, sizeof(buf), ARENA_EV_WAIT, k, -1, 0));
		if (g_Mp2) {
			Ar_Text(k, Script_Lang("Net_Label_Waiting_Challenge"), 1.0, 1);
		}
	}
	return 1;
}

/* FlagHunt: who touches the flag scores, it moves on */
static void Ar_FlagHunt(void) {
	vec3_t at = { (float)ar.st.flagPos[0], (float)ar.st.flagPos[1], (float)ar.st.flagPos[2] };
	if (ar.zoneAt < 0) {
		return;
	}
	for (int k = 0; k < ARENA_MEMBERS; k++) {
		cEntity *e = ar.st.m[k].used ? (k < ARENA_PLAYERS ? Players_Get(k) : Ar_Ship(k)) : NULL;
		if (!e || Ar_DistSq(e, &at) > ARENA_FLAG_RADIUS * ARENA_FLAG_RADIUS || Ar_TooEarly(k)) {
			continue;
		}
		ArenaRules_Score(&ar.st, k);
		Ar_PickZone();
		Ar_FlagAt(&ar.zone[ar.zoneAt].pos);
		Ar_Event(ARENA_EV_FLAG, k, -1, 0);
		Con_Printf("arena: %s took the flag (%d), next at %s\n", ar.st.m[k].name, ar.st.m[k].score, ar.st.zone);
		if (ArenaRules_Ended(&ar.st)) {
			Ar_End();
		}
		return;
	}
}

/* Postman: the bag taken, carried to its zone, back home after a while alone */
static void Ar_Postman(void) {
	if (ar.st.flag == ARENA_FL_LIES) {
		vec3_t at = { (float)ar.st.flagPos[0], (float)ar.st.flagPos[1], (float)ar.st.flagPos[2] };
		if (ar.bagDropAt > 0.0 && ar.now - ar.bagDropAt >= AR_BAG_BACK) {
			Ar_BagHome();
			Ar_Event(ARENA_EV_BACK, -1, -1, 0);
			return;
		}
		for (int k = 0; k < ARENA_PLAYERS; k++) {
			cEntity *e = ar.st.m[k].used ? Players_Get(k) : NULL;
			if (!e || Ar_DistSq(e, &at) > ARENA_FLAG_RADIUS * ARENA_FLAG_RADIUS || Ar_TooEarly(k)) {
				continue;
			}
			ar.st.flag = ARENA_FL_CARRIED;
			ar.st.carrier = (uint8_t)(k + 1);
			ar.bagDropAt = 0.0;
			Ar_PickZone();
			Ar_Event(ARENA_EV_FLAG, k, -1, 0);
			Con_Printf("arena: %s took the bag to zone %s\n", ar.st.m[k].name, ar.st.zone);
			return;
		}
		return;
	}
	int k = ar.st.carrier - 1;
	cEntity *e = k >= 0 ? Players_Get(k) : NULL;
	if (!e) {
		return;
	}
	vec3_t p = e->pos;
	int32_t q[3] = { (int32_t)p.x, (int32_t)p.y, (int32_t)p.z };
	if (memcmp(q, ar.st.flagPos, sizeof(q))) {
		memcpy(ar.st.flagPos, q, sizeof(q));
	}
	if (ar.zoneAt >= 0 && Ar_DistSq(e, &ar.zone[ar.zoneAt].pos) <= ARENA_ZONE_RADIUS * ARENA_ZONE_RADIUS) {
		ArenaRules_Score(&ar.st, k);
		Ar_Event(ARENA_EV_DELIVER, k, -1, 0);
		Con_Printf("arena: %s delivered at %s (%d)\n", ar.st.m[k].name, ar.st.zone, ar.st.m[k].score);
		Ar_PickZone();
		if (ArenaRules_Ended(&ar.st)) {
			Ar_End();
		}
	}
}

/* Ffa.OnPositionChange (-mp2, the modes without teams): the labels of a player whose lead or place changed */
static void Ar_Places(void) {
	char text[ARENA_TEXT], place[16];
	if (Ar_Flags() & ARENA_F_TEAM) {
		return;
	}
	for (int i = 0; i < ARENA_PLAYERS; i++) {
		const ArenaMember *m = &ar.st.m[i];
		ArSv *v = &ar.sv[i];
		int pos, score, winner;
		if (!m->used || !(m->flags & ARENA_MF_JOINED) || (m->flags & ARENA_MF_SPECT) || Rep_PlayerConn(i) < 0) {
			continue;
		}
		Arena_Place(&ar.st, i, &pos, &score, &winner);
		if (pos == v->pos && score == v->score && winner == v->winner) {
			continue;
		}
		if (winner != v->winner || (score != v->score && v->winner != -1)) {
			Ar_Text(i, Script_Lang(winner == -1 ? "Net_Label_Lost_Lead" : winner == 0 ? "Net_Label_Tied_Lead" :
				"Net_Label_Tied_Leading"), 5.0, 1);
		} else {
			Str_PrintfQuiet(place, sizeof(place), pos == 1 ? "1st" : pos == 2 ? "2nd" : "%dth", pos);
			Arena_Lang(text, sizeof(text), pos < v->pos ? "Net_Label_Up_to" : pos > v->pos ? "Net_Label_Down_to" :
				"Net_Label_Still_to", place, NULL);
			Ar_Text(i, text, 5.0, 1);
		}
		v->pos = pos;
		v->score = score;
		v->winner = winner;
	}
}

/* the state to every player when it changed (now: at once, else at most every ARENA_SEND_MS) */
static void Ar_SendState(byte now) {
	static uint8_t buf[8192];
	uint64_t ms = Plat_RealNs() / 1000000;
	if (!ar.dirty || (!now && ms - ar.lastSent < ARENA_SEND_MS)) {
		return;
	}
	ar.dirty = 0;
	ar.lastSent = ms;
	for (int i = 0; i < ARENA_PLAYERS; i++) {
		if (ar.st.m[i].used) {
			Ar_Out(i, buf, Arena_WriteState(buf, sizeof(buf), &ar.st, i));
		}
	}
	if (g_Mp2) {
		Ar_Places();
	}
}

/* ---- -mp2: the flag, the bag and the zones as items of the instance, the way points, the count down ---- */

static cEntity *Ar_Item(const char *name, const vec3_t *p) {
	cEntity *e = World_FindEntity(name);
	return e ? e : cWorld_CreateEntity(g_pWorld, name, p->x, p->y, p->z, "Item");
}

/* a model of the mode's module (FlagModel, FlagPilotModel, FlagNoUsrModel), 0 none */
static byte Ar_ModelPath(const char *var, char *model, int cap) {
	byte ok = Mode_ModGetS(g_ArenaModes[ar.mode].module, var, model, cap) && model[0];
	Native_ErrClear();
	return ok;
}

static void Ar_Model(cEntity *e, const char *var) {
	char model[128];
	if (Ar_ModelPath(var, model, sizeof(model))) {
		Entity_SetModel(e, model, 1.0f, 1.0f, 1.0f, 1);
	}
}

/* the zones of the postman modes: an item of radius 3000 at each zone (NPostman.BrowseAreas), on the radar while it
 * is the carried bag's target */
static void Ar_ZoneItems(void) {
	char ent[ARENA_ZONE + 16];
	for (int z = 0; z < ar.nzones; z++) {
		Str_PrintfQuiet(ent, sizeof(ent), ARENA_ZONE_ENT "%s", ar.zone[z].key);
		cEntity *e = Ar_Item(ent, &ar.zone[z].pos);
		int radar = ar.st.flag == ARENA_FL_CARRIED && z == ar.zoneAt ? 2 : 0, radius = (int)ARENA_ZONE_RADIUS;
		if (e && !ar.items) {
			Entity_VarI(e, "Radius", &radius, 1);
		}
		int cur = -1;
		if (e && (!Entity_VarI(e, "RadarShow", &cur, 0) || cur != radar)) {
			Entity_VarI(e, "RadarShow", &radar, 1);
		}
	}
}

static void Ar_Items(void) {
	char link[sizeof(ar.link)];
	vec3_t p = { (float)ar.st.flagPos[0], (float)ar.st.flagPos[1], (float)ar.st.flagPos[2] };
	if (!(Ar_Flags() & (ARENA_F_FLAG | ARENA_F_POST)) || ar.st.flag == ARENA_FL_NONE) {
		return;
	}
	cEntity *f = Ar_Item(ARENA_FLAG_ENT, &p);
	if (!f) {
		return;
	}
	if (Ar_Flags() & ARENA_F_POST) {
		Ar_ZoneItems();
	}
	if (!ar.items) {
		int radius = (int)ARENA_FLAG_RADIUS, radar = 1;
		ar.items = 1;
		Ar_Model(f, "FlagModel");
		Entity_VarI(f, "Radius", &radius, 1);
		Entity_VarI(f, "RadarShow", &radar, 1);
	}
	int k = ar.st.carrier - 1;
	cEntity *c = ar.st.flag == ARENA_FL_CARRIED && k >= 0 && k < ARENA_PLAYERS ? Players_Get(k) : NULL;
	if (c) {
		/* NPostman.TakeFlag: on the carrier's DC_Flag_Player with its pilot, the model without it on the carrier's
		 * own client (REPENT_J_OWNMODEL); a link before goes first (NPostman.DropFlag) */
		Str_PrintfQuiet(link, sizeof(link), "%s,DC_Flag_Player", c->name);
		if (strcmp(ar.link, link)) {
			char own[128];
			int four = 4, zero = 0, one = 1;
			if (ar.link[0]) {
				Ar_SetVarS(f, "LinkModel", ",");
			}
			Ar_Model(f, "FlagPilotModel");
			if (Ar_ModelPath("FlagNoUsrModel", own, sizeof(own))) {
				RepEnt_Note(f, REPENT_J_OWNMODEL, own);
			}
			Ar_SetVarS(f, "LinkModel", link);
			Entity_VarI(f, "DeltaOffMul", &four, 1);
			Entity_VarI(f, "OffMul", &zero, 1);
			Entity_VarI(f, "UseLinkAng", &one, 1);
			Str_Copy(ar.link, link, sizeof(ar.link));
		}
	} else {
		if (ar.link[0]) {
			Ar_SetVarS(f, "LinkModel", ",");
			Ar_Model(f, "FlagModel");
			ar.link[0] = 0;
		}
		Entity_SetPos(f, &p);
	}
}

/* each player's way point (CurrentWaypointName in its context): the flag or the bag, the carried bag's zone for its
 * carrier (and its team in TeamPost) */
static void Ar_Ways(void) {
	char way[sizeof(ar.sv[0].way)];
	int k = ar.st.carrier - 1;
	if (!(Ar_Flags() & (ARENA_F_FLAG | ARENA_F_POST)) || ar.st.flag == ARENA_FL_NONE) {
		return;
	}
	for (int i = 0; i < ARENA_PLAYERS; i++) {
		const ArenaMember *m = &ar.st.m[i];
		if (!m->used || Rep_PlayerConn(i) < 0) {
			continue;
		}
		byte mine = k >= 0 && (k == i || ((Ar_Flags() & ARENA_F_TEAM) && ar.st.m[k].team == m->team));
		if ((Ar_Flags() & ARENA_F_POST) && ar.st.flag == ARENA_FL_CARRIED && mine && ar.st.zone[0]) {
			Str_PrintfQuiet(way, sizeof(way), ARENA_ZONE_ENT "%s", ar.st.zone);
		} else {
			Str_Copy(way, ARENA_FLAG_ENT, sizeof(way));
		}
		if (strcmp(way, ar.sv[i].way)) {
			int was = Players_Enter(i);
			Mode_GVarSetS("CurrentWaypointName", way);
			Players_Leave(was);
			Str_Copy(ar.sv[i].way, way, sizeof(ar.sv[i].way));
		}
	}
}

/* Ffa.RejoinMsg: the count down of a team change, a text a second */
static void Ar_CountDown(void) {
	char text[ARENA_TEXT], n[16];
	for (int i = 0; i < ARENA_PLAYERS; i++) {
		ArSv *v = &ar.sv[i];
		int left = (int)ceil(v->notBefore - ar.now);
		if (!ar.st.m[i].used || left <= 0 || ar.now - v->lastCount < 1.0) {
			continue;
		}
		v->lastCount = ar.now;
		Str_PrintfQuiet(n, sizeof(n), "%d", left);
		Arena_Lang(text, sizeof(text), "Net_Msg_Join_Time", n, NULL);
		Ar_Text(i, text, 1.0, 0);
	}
}

void Arena_Tick(Net2Host *host, uint64_t tick) {
	(void)host;
	if (!Arena_On()) {
		return;
	}
	ar.now = Instance_Time();
	if (!ar.ready) {
		Ar_Ready();
	}
	Ar_Members();
	if (g_Mp2) {
		Ar_Items();
		Ar_Ways();
		Ar_CountDown();
	}
	if (ar.st.phase == ARENA_PH_END) {
		int left = (int)ceil(ar.endAt - ar.now);
		if (left != ar.st.endLeft) {
			ar.st.endLeft = left < 0 ? 0 : left;
			ar.dirty = 1;
		}
		if (ar.now >= ar.endAt) {
			Ar_NextRound();
		}
	} else {
		Ar_Bots();
		if (tick % AR_TAKE_STEP == 0) {
			if (Ar_Flags() & ARENA_F_FLAG) {
				Ar_FlagHunt();
			} else if (Ar_Flags() & ARENA_F_POST) {
				Ar_Postman();
			}
		}
		if (ar.st.phase == ARENA_PH_WAIT && ArenaRules_Started(&ar.st)) {
			ar.st.phase = ARENA_PH_PLAY;
			ar.startAt = ar.now;
			ar.dirty = 1;
			Con_Printf("arena: the round counts\n");
		}
		if (ar.st.phase == ARENA_PH_PLAY && ar.timelimit) {
			int left = ar.timelimit - (int)(ar.now - ar.startAt);
			if (left != ar.st.timeLeft) {
				ar.st.timeLeft = left < 0 ? 0 : left;
				ar.dirty = 1;
			}
			if (left <= 0) {
				Ar_End();
			}
		}
	}
	Ar_SendState(0);
}

byte Arena_Command(const char *line) {
	int k, n;
	if (strncmp(line, "arena ", 6) || !Arena_On()) {
		return 0;
	}
	line += 6;
	if (!strcmp(line, "status")) {
		Con_Printf("arena: %s round %d phase %d started %d, teams %d / %d, flag %d carrier %d zone %s\n",
			g_ArenaModes[ar.mode].type, ar.st.round, ar.st.phase, ar.st.started, ar.st.red, ar.st.green, ar.st.flag,
			ar.st.carrier - 1, ar.st.zone);
		for (k = 0; k < ARENA_MEMBERS; k++) {
			const ArenaMember *m = &ar.st.m[k];
			if (m->used) {
				Con_Printf("arena: member %d %s team %d score %d flags %d slot %d\n", k, m->name, m->team, m->score,
					m->flags, m->slot - 1);
			}
		}
		return 1;
	}
	if (sscanf(line, "score %d %d", &k, &n) == 2 && k >= 0 && k < ARENA_MEMBERS && ar.st.m[k].used) {
		ar.st.m[k].score = n;
		ar.dirty = 1;
		if (ArenaRules_Ended(&ar.st) && ar.st.phase != ARENA_PH_END) {
			Ar_End();
		}
		return 1;
	}
	int j;
	if (sscanf(line, "kill %d %d", &k, &j) == 2 && k >= 0 && k < ARENA_MEMBERS && ar.st.m[k].used) {
		/* member k killed as a weapon of member j does it (its hitBy; j -1: no killer) */
		cEntity *v = k < ARENA_PLAYERS ? Players_Get(k) : Ar_Ship(k);
		cEntity *a = j >= 0 && j < ARENA_MEMBERS ? (j < ARENA_PLAYERS ? Players_Get(j) : Ar_Ship(j)) : NULL;
		if (v) {
			if (a) {
				cVehicleAttack_HitBy(&WLife(v)->attack, a->name);
			}
			WLife(v)->life = 0;
		}
		Con_Printf("arena: member %d killed by member %d (%s)\n", k, j, v ? v->name : "no ship");
		return 1;
	}
	if (!strcmp(line, "end")) {
		if (ar.st.phase != ARENA_PH_END) {
			Ar_End();
		}
		return 1;
	}
	float dist = 0.0f;
	if (sscanf(line, "flag %d %f", &k, &dist) >= 1 && k >= 0 && k < ARENA_MEMBERS && ar.st.m[k].used) {
		/* the member goes to the flag (FlagHunt), the bag or the bag's zone (Postman), or dist units east of it
		 * looking at it (screenshots) */
		cEntity *e = k < ARENA_PLAYERS ? Players_Get(k) : Ar_Ship(k);
		vec3_t at = { (float)ar.st.flagPos[0], (float)ar.st.flagPos[1], (float)ar.st.flagPos[2] };
		if (ar.st.flag == ARENA_FL_CARRIED && ar.zoneAt >= 0) {
			at = ar.zone[ar.zoneAt].pos;
		}
		if (e) {
			vec3_t ang = { 0.0f, M_PI_F * 0.5f, 0.0f };
			e->pos = (vec3_t){ at.x + dist, at.y + 300.0f, at.z };
			Obj(e)->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
			if (dist > 0.0f) {
				Obj_SetAng(e, &ang);
			}
		}
		Con_Printf("arena: member %d to %.0f %.0f %.0f\n", k, (double)at.x, (double)at.y, (double)at.z);
		return 1;
	}
	return 0;
}
