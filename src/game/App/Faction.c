/*
 * The factions of the freeroam mode in the game: the instance's side (membership, abilities, crimes and the wanted
 * level, the status to the clients) and the client's (the keys, the text line, the siren of the players' ships), see
 * include/scrap/faction.h. Not in the original.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/alarm.h"
#include "scrap/console.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/faction.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/graphic2d.h"
#include "scrap/input.h"
#include "scrap/instance.h"
#include "scrap/logic.h"
#include "scrap/net2.h"
#include "scrap/netrep.h"
#include "scrap/nodesworld.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/sound.h"
#include "scrap/text.h"
#include "scrap/travel.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weapons.h"
#include "scrap/world.h"

#define FR_NAME 64
#define FR_CRIMES 256				/* crimes kept between two ticks */
#define FR_YIELDS 64				/* traffic ships moving away from a siren */
#define FR_SIREN_EVERY (NETREP_TICK_HZ / 2)	/* ticks between the siren's looks for traffic */
#define FR_BACKUP_INDOOR 6000.0f	/* units: the gears go for a wanted player this near the officer */
#define FR_BACKUP_OUTDOOR 30000.0f	/* and the police ships */
#define FR_SHIPS 64					/* players' ships a client keeps the siren of */
#define FR_TEXT 160
#define FR_NOTICE_MS 3000
#define FR_DEFAULT_CHAR "Dtritus"
#define FR_SIREN_SND 0x1f			/* the vehicle sound of the siren (snd31 of OutSound) */

/* every text a player sees (a writing pass comes later) */
enum {
	FT_JOINED, FT_LEFT, FT_NOT_OFFICE, FT_LEAVE_FIRST, FT_WANTED_NO_JOIN, FT_ON_FOOT, FT_MEMBERS_ONLY, FT_NOT_READY,
	FT_NEED_SHIP, FT_SIREN_ON, FT_SIREN_OFF, FT_ARMOUR_ON, FT_ARMOUR_OFF, FT_NO_SUSPECT, FT_ARRESTED, FT_ARREST_MADE,
	FT_RANK_UP, FT_BACKUP, FT_SHIP, FT_NO_SHIP, FT_WANTED, FT_NOT_FREEROAM, FT_YOU_ARE_WANTED, FT_HUD_MEMBER,
	FT_HUD_WANTED, FT_HUD_RANK, FT_COUNT
};
static const char *const g_FactionTexts[FT_COUNT] = {
	[FT_JOINED] = "You joined the %s.",
	[FT_LEFT] = "You left the %s.",
	[FT_NOT_OFFICE] = "Join or leave at the office.",
	[FT_LEAVE_FIRST] = "Leave the %s first.",
	[FT_WANTED_NO_JOIN] = "Wanted players cannot join.",
	[FT_ON_FOOT] = "Only on foot.",
	[FT_MEMBERS_ONLY] = "Not for you.",
	[FT_NOT_READY] = "Not ready yet.",
	[FT_NEED_SHIP] = "Only in a police ship.",
	[FT_SIREN_ON] = "Siren on.",
	[FT_SIREN_OFF] = "Siren off.",
	[FT_ARMOUR_ON] = "Armour on.",
	[FT_ARMOUR_OFF] = "Armour off.",
	[FT_NO_SUSPECT] = "No wanted player near.",
	[FT_ARRESTED] = "Arrested! Fine: %d.",
	[FT_ARREST_MADE] = "Arrest made. Reward: %d.",
	[FT_RANK_UP] = "Promoted: %s.",
	[FT_BACKUP] = "Backup is coming.",
	[FT_SHIP] = "Police ship called.",
	[FT_NO_SHIP] = "No room for a ship here.",
	[FT_WANTED] = "Wanted level %d!",
	[FT_NOT_FREEROAM] = "No factions on this server.",
	[FT_YOU_ARE_WANTED] = "Not while you are wanted.",
	[FT_HUD_MEMBER] = "%s - %s",
	[FT_HUD_WANTED] = "Wanted: %d",
	[FT_HUD_RANK] = "Rank %d",
};
/* the rank names of the police (other factions: FT_HUD_RANK) */
static const char *const g_PoliceRanks[] = { "Rookie", "Officer", "Sergeant", "Lieutenant", "Captain" };

static FactionTable fr_table;
static byte fr_loaded, fr_bad;

const FactionTable *Faction_Table(void) {
	char err[160];
	if (!fr_loaded) {
		fr_loaded = 1;
		if (FactionTable_Parse(&fr_table, g_FactionTableText, err, sizeof(err)) < 0) {
			Con_Errorf("factions: the table is bad (%s)\n", err);
			fr_bad = 1;
		}
	}
	return fr_bad ? NULL : &fr_table;
}

static const char *Fr_RankName(int f, int rank, char *buf, int cap) {
	const FactionTable *t = Faction_Table();
	if (t && f >= 0 && !crt_stricmp(t->f[f].name, "Police") && rank >= 1 &&
		rank <= (int)(sizeof(g_PoliceRanks) / sizeof(g_PoliceRanks[0]))) {
		return g_PoliceRanks[rank - 1];
	}
	Str_PrintfQuiet(buf, cap, g_FactionTexts[FT_HUD_RANK], rank);
	return buf;
}

/* the text id with its argument: a name where it has %s, else the number */
static void Fr_Text(char *out, int cap, int id, int arg, const char *name) {
	if (strstr(g_FactionTexts[id], "%s")) {
		Str_PrintfQuiet(out, cap, g_FactionTexts[id], name ? name : "");
	} else {
		Str_PrintfQuiet(out, cap, g_FactionTexts[id], arg);
	}
}

/* ================================ the instance ================================ */

typedef struct FrPlayer {
	int faction;					/* FACTION_NONE */
	int arrests;
	char charCls[FACTION_NAME];		/* the class it spawns as when it is no member, "" Dtritus */
	byte armoured;
	byte siren;
	byte callShip;					/* the next ship it calls is its police ship */
	cEntity *ship;					/* its ship of the siren (compared, read while World_FindEntity(shipName) is it) */
	char shipName[FR_NAME];
	FactionWanted wanted;
	int level;						/* the wanted level of the last tick */
	int jailFine;					/* arrested and sent to the jail of another level with this fine: its notice
									   there (state key jailfine), -1 */
	double ready[FA_COUNT];			/* the instance time the ability is ready again */
	uint8_t sent[8];				/* the last status sent */
	byte sentAny;
	uint32_t requests, done;
} FrPlayer;

typedef struct FrYield {
	cEntity *e;
	char name[FR_NAME];
	double until;
} FrYield;

static struct {
	byte freeroam;
	byte policePvp;
	FrPlayer pl[PLAYERS_MAX];
	struct {
		int8_t player;
		int8_t crime;
	} crimes[FR_CRIMES];
	int ncrimes;
	FrYield yield[FR_YIELDS];
	uint32_t crimesLost;
} fr = { .policePvp = 1 };

byte Faction_ParseParam(const char *param, const char *ext) {
	if (!crt_stricmp("mode", param)) {
		if (!crt_stricmp(ext, "freeroam")) {
			fr.freeroam = 1;
		} else if (!crt_stricmp(ext, "sandbox")) {
			fr.freeroam = 0;
		} else {
			Con_Errorf("instance: -mode is sandbox or freeroam, not '%s'\n", ext);
		}
		return 1;
	}
	if (!crt_stricmp("policepvp", param)) {
		fr.policePvp = crt_atoi(ext) != 0;
		return 1;
	}
	return 0;
}

byte Faction_Freeroam(void) {
	return fr.freeroam && Faction_Table() != NULL;
}

const char *Faction_ModeName(void) {
	return fr.freeroam ? "freeroam" : "sandbox";
}

static FrPlayer *Fr_Player(int i) {
	return i >= 0 && i < PLAYERS_MAX ? &fr.pl[i] : NULL;
}

static double Fr_Now(void) {
	return Instance_Time();
}

static int Fr_Rank(const FrPlayer *p) {
	const FactionTable *t = Faction_Table();
	return t && p->faction >= 0 ? Faction_Rank(&t->f[p->faction], p->arrests) : 0;
}

void Faction_Admit(int i, const char *state, int len) {
	char v[64];
	FrPlayer *p = Fr_Player(i);
	const FactionTable *t = Faction_Table();
	if (!p) {
		return;
	}
	memset(p, 0, sizeof(*p));
	p->faction = FACTION_NONE;
	p->jailFine = -1;
	FactionWanted_Init(&p->wanted);
	if (!state) {
		return;
	}
	if (CoordKv_Get(state, len, "jailfine", v, sizeof(v)) && v[0]) {
		p->jailFine = crt_atoi(v);
	}
	if (CoordKv_Get(state, len, "char", v, sizeof(v))) {
		Str_Copy(p->charCls, v, sizeof(p->charCls));
	}
	if (CoordKv_Get(state, len, "arrests", v, sizeof(v))) {
		p->arrests = crt_atoi(v) < 0 ? 0 : crt_atoi(v);
	}
	if (t && CoordKv_Get(state, len, "faction", v, sizeof(v)) && v[0]) {
		p->faction = Faction_Find(t, v);
	}
	if (t && CoordKv_Get(state, len, "wanted", v, sizeof(v))) {
		float w = (float)crt_atof(v);
		p->wanted.points = w < 0.0f ? 0.0f : w;
		p->level = FactionWanted_Level(t, &p->wanted);
	}
	/* a member plays its faction's class; a faction of another mode is kept but has no effect */
	if (t && p->faction >= 0 && fr.freeroam) {
		Str_Copy(p->charCls, t->f[p->faction].play, sizeof(p->charCls));
	}
}

void Faction_State(int i, char *state, int *len, int cap) {
	char v[32];
	FrPlayer *p = Fr_Player(i);
	const FactionTable *t = Faction_Table();
	if (!p) {
		return;
	}
	/* the class it spawns as, not its armour */
	CoordKv_Set(state, len, cap, "char", p->charCls[0] ? p->charCls : FR_DEFAULT_CHAR);
	if (!t) {
		return;
	}
	CoordKv_Set(state, len, cap, "faction", p->faction >= 0 ? t->f[p->faction].name : "");
	Str_PrintfQuiet(v, sizeof(v), "%d", Fr_Rank(p));
	CoordKv_Set(state, len, cap, "rank", v);
	Str_PrintfQuiet(v, sizeof(v), "%d", p->arrests);
	CoordKv_Set(state, len, cap, "arrests", v);
	Str_PrintfQuiet(v, sizeof(v), "%.2f", (double)p->wanted.points);
	CoordKv_Set(state, len, cap, "wanted", v);
	Str_PrintfQuiet(v, sizeof(v), "%d", p->jailFine);
	CoordKv_Set(state, len, cap, "jailfine", p->jailFine >= 0 ? v : "");
}

/* the class is loaded (its resources, as the conversor's Chars.LoadRes): 1 when it is there */
static byte Fr_LoadClass(const char *cls) {
	if (cActClassList_Get(g_pActClassList, cls)) {
		return 1;
	}
	/* outdoors the level loads no character scripts (Init's LoadChars): the class's script and its sound first */
	if (!Mode_Import("Init") || !PORT_CALL(Init, LoadCharLib, NULL, "s", cls)) {
		Native_ErrPrint();
	}
	if (!Mode_Import("Chars") || !PORT_CALL(Chars, LoadRes, NULL, "s", cls)) {
		Native_ErrPrint();
		return 0;
	}
	return cActClassList_Get(g_pActClassList, cls) != NULL;
}

void Faction_EnsureClass(const char *cls) {
	if (cls && *cls) {
		Fr_LoadClass(cls);
	}
}

const char *Faction_CharClass(int i) {
	FrPlayer *p = Fr_Player(i);
	const FactionTable *t = Faction_Table();
	const char *cls = FR_DEFAULT_CHAR;
	if (p && t && fr.freeroam && p->faction >= 0) {
		const FactionDef *f = &t->f[p->faction];
		cls = p->armoured && f->armour[0] ? f->armour : f->play;
	} else if (p && p->charCls[0]) {
		cls = p->charCls;
	}
	if (strcmp(cls, FR_DEFAULT_CHAR) && !Fr_LoadClass(cls)) {
		Con_Printf("factions: class %s of player %d cannot load, %s\n", cls, i, FR_DEFAULT_CHAR);
		cls = FR_DEFAULT_CHAR;
	}
	return cls;
}

const char *Faction_Pilot(int i) {
	FrPlayer *p = Fr_Player(i);
	const FactionTable *t = Faction_Table();
	return p && t && fr.freeroam && p->faction >= 0 && !crt_stricmp(t->f[p->faction].name, "Police") ? "P_Police" :
		"P_" FR_DEFAULT_CHAR;
}

void Faction_Spawned(int i, cEntity *e) {
	FrPlayer *p = Fr_Player(i);
	const FactionTable *t = Faction_Table();
	if (!p || !e || !t || p->faction < 0 || !p->armoured || NetPl_Kind(e) != NETREP_CHAR) {
		return;
	}
	cWithLifeEntity *w = WLife(e);
	w->life = w->prevLife = t->f[p->faction].armourLife;
}

void Faction_Left(int i) {
	FrPlayer *p = Fr_Player(i);
	if (p) {
		p->siren = 0;
		p->ship = NULL;
		p->sentAny = 0;
	}
}

/* ---- the messages to the client ---- */

static void Fr_Notice(int i, int text, int arg, const char *name) {
	uint8_t buf[96];
	NetRepBuf w;
	int conn = Rep_PlayerConn(i);
	if (conn < 0 || !Instance_Host()) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, FACTION_MSG_STATUS);
	NetRep_PutU8(&w, FACTION_ST_NOTICE);
	NetRep_PutU8(&w, (uint32_t)text);
	NetRep_PutSVar(&w, arg);
	NetRep_PutStr(&w, name ? name : "");
	if (!w.bad) {
		Net2_Send(Instance_Host(), conn, REP_CH_RELIABLE, buf, w.size);
	}
	char line[FR_TEXT];
	Fr_Text(line, sizeof(line), text, arg, name);
	Con_Printf("factions: player %d: %s\n", i, line);
}

/* faction, rank, wanted level, arrests, flags, ready abilities: sent when they change */
static void Fr_SendState(Net2Host *host, int i) {
	uint8_t st[8], buf[16];
	NetRepBuf w;
	FrPlayer *p = &fr.pl[i];
	const FactionTable *t = Faction_Table();
	int conn = Rep_PlayerConn(i);
	if (conn < 0 || !t) {
		return;
	}
	double now = Fr_Now();
	uint8_t ready = 0;
	for (int a = 0; a < FA_COUNT; a++) {
		ready |= (uint8_t)((p->faction >= 0 && t->f[p->faction].cooldown[a] >= 0.0f && now >= p->ready[a]) << a);
	}
	st[0] = (uint8_t)(p->faction >= 0 ? p->faction : 255);
	st[1] = (uint8_t)Fr_Rank(p);
	st[2] = (uint8_t)p->level;
	st[3] = (uint8_t)(p->arrests > 255 ? 255 : p->arrests);
	st[4] = (uint8_t)((p->arrests >> 8) > 255 ? 255 : p->arrests >> 8);
	st[5] = (uint8_t)((p->siren ? 1 : 0) | (p->armoured ? 2 : 0) | (fr.freeroam ? 4 : 0));
	st[6] = ready;
	st[7] = 0;
	if (p->sentAny && !memcmp(st, p->sent, sizeof(st))) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, FACTION_MSG_STATUS);
	NetRep_PutU8(&w, FACTION_ST_STATE);
	for (int k = 0; k < 7; k++) {
		NetRep_PutU8(&w, st[k]);
	}
	if (!w.bad) {
		Net2_Send(host, conn, REP_CH_RELIABLE, buf, w.size);
		memcpy(p->sent, st, sizeof(st));
		p->sentAny = 1;
	}
}

/* ---- the abilities ---- */

static float Fr_DistSq(const cEntity *a, const vec3_t *b) {
	float dx = a->pos.x - b->x, dy = a->pos.y - b->y, dz = a->pos.z - b->z;
	return dx * dx + dy * dy + dz * dz;
}

/* the model of a ship ("Model", else its type name) */
static const char *Fr_Model(cEntity *v) {
	char *s = NULL;
	if (Entity_VarS(v, "Model", &s, 0) && s && *s) {
		return s;
	}
	return Veh(v)->typeName;
}

/* the position of the office's desk (an entity, else a dummy of the level): 1 found */
static byte Fr_Desk(const FactionDef *f, vec3_t *pos) {
	char found[64];
	vec3_t ang;
	cEntity *e = f->desk[0] ? World_FindEntity(f->desk) : NULL;
	if (e) {
		*pos = e->pos;
		return 1;
	}
	if (!f->desk[0]) {
		return 0;
	}
	DummySearch_Start(f->desk, 1);
	return DummySearch_NextEuler(found, sizeof(found), pos, &ang);
}

/* the player's character again (another class, out of its ship), at pos (NULL: where it is) */
static void Fr_Rebody(int i, const vec3_t *pos, float yaw) {
	FrPlayer *p = &fr.pl[i];
	p->siren = 0;
	p->ship = NULL;
	Rep_Rebody(i, pos, yaw);
}

static int Fr_Join(int i, cEntity *e) {
	const FactionTable *t = Faction_Table();
	FrPlayer *p = &fr.pl[i];
	vec3_t desk;

	if (NetPl_Kind(e) != NETREP_CHAR) {
		Fr_Notice(i, FT_ON_FOOT, 0, NULL);
		return 0;
	}
	int here = FACTION_NONE;
	for (int k = 0; k < t->n; k++) {
		if (t->f[k].cooldown[FA_JOIN] >= 0.0f && !crt_stricmp(t->f[k].office, Instance_Level())) {
			here = k;
		}
	}
	if (here < 0 || (Fr_Desk(&t->f[here], &desk) && Fr_DistSq(e, &desk) > t->deskReach * t->deskReach)) {
		Fr_Notice(i, FT_NOT_OFFICE, 0, NULL);
		return 0;
	}
	if (p->faction >= 0 && p->faction != here) {
		Fr_Notice(i, FT_LEAVE_FIRST, 0, t->f[p->faction].name);
		return 0;
	}
	if (p->faction == here) {
		p->faction = FACTION_NONE;
		p->armoured = 0;
		Str_Copy(p->charCls, FR_DEFAULT_CHAR, sizeof(p->charCls));
		Fr_Rebody(i, NULL, 0.0f);
		Fr_Notice(i, FT_LEFT, 0, t->f[here].name);
		Travel_Report(i);
		return 1;
	}
	if (p->level > 0) {
		Fr_Notice(i, FT_WANTED_NO_JOIN, 0, NULL);
		return 0;
	}
	p->faction = here;
	p->armoured = 0;
	Str_Copy(p->charCls, t->f[here].play, sizeof(p->charCls));
	Fr_Rebody(i, NULL, 0.0f);
	Fr_Notice(i, FT_JOINED, 0, t->f[here].name);
	Travel_Report(i);
	return 1;
}

static int Fr_Siren(int i, cEntity *e) {
	FrPlayer *p = &fr.pl[i];
	if (NetPl_Kind(e) != NETREP_SHIP || Faction_OfShip(Faction_Table(), Fr_Model(e)) != p->faction) {
		Fr_Notice(i, FT_NEED_SHIP, 0, NULL);
		return 0;
	}
	p->siren = !p->siren;
	p->ship = e;
	Str_Copy(p->shipName, e->name, sizeof(p->shipName));
	Fr_Notice(i, p->siren ? FT_SIREN_ON : FT_SIREN_OFF, 0, NULL);
	return 1;
}

static int Fr_Armour(int i, cEntity *e) {
	FrPlayer *p = &fr.pl[i];
	if (NetPl_Kind(e) != NETREP_CHAR) {
		Fr_Notice(i, FT_ON_FOOT, 0, NULL);
		return 0;
	}
	p->armoured = !p->armoured;
	Fr_Rebody(i, NULL, 0.0f);
	Fr_Notice(i, p->armoured ? FT_ARMOUR_ON : FT_ARMOUR_OFF, 0, NULL);
	return 1;
}

/* the nearest other player with a wanted level within reach of e, -1 */
static int Fr_Suspect(int i, cEntity *e, float reach) {
	int best = -1;
	float bestD = reach * reach;
	for (int j = 0; j < PLAYERS_MAX; j++) {
		cEntity *o = j != i ? Players_Get(j) : NULL;
		if (!o || fr.pl[j].level <= 0) {
			continue;
		}
		float d = Fr_DistSq(o, &e->pos);
		if (d < bestD) {
			bestD = d;
			best = j;
		}
	}
	return best;
}

/* the arrested player to the jail of the faction's office level: the travel there (a coordinator's world; the notice
 * of the fine goes with it, the client leaves this instance at once), a place at the jail (this is the office level),
 * else its spawn again */
static void Fr_Jail(int j, const FactionDef *f, int fine) {
	char found[64];
	vec3_t pos, ang;
	cEntity *e = Players_Get(j);
	if (!e) {
		return;
	}
	if (!crt_stricmp(f->office, Instance_Level())) {
		DummySearch_Start(f->jail, 1);
		if (f->jail[0] && DummySearch_NextEuler(found, sizeof(found), &pos, &ang)) {
			Fr_Rebody(j, &pos, ang.y + M_PI_F);
		} else {
			Rep_Respawn(j);
		}
		Fr_Notice(j, FT_ARRESTED, fine, NULL);
		Con_Printf("factions: player %d jailed in %s\n", j, Instance_Level());
		return;
	}
	/* the state the travel hands over carries the notice */
	fr.pl[j].jailFine = fine;
	byte gone = Travel_Coordinated() && Travel_Request(e->name, f->office, f->jail, "");
	fr.pl[j].jailFine = -1;
	if (gone) {
		Con_Printf("factions: player %d sent to the jail of %s\n", j, f->office);
		return;
	}
	Fr_Notice(j, FT_ARRESTED, fine, NULL);
	Con_Printf("factions: player %d has no way to %s (no coordinator): it starts again here\n", j, f->office);
	Rep_Respawn(j);
}

static int Fr_Arrest(int i, cEntity *e) {
	const FactionTable *t = Faction_Table();
	FrPlayer *p = &fr.pl[i];
	char rank[32];

	if (p->level > 0) {
		Fr_Notice(i, FT_YOU_ARE_WANTED, 0, NULL);
		return 0;
	}
	int j = Fr_Suspect(i, e, NetPl_Kind(e) == NETREP_SHIP ? t->reachShip : t->reachFoot);
	if (j < 0) {
		Fr_Notice(i, FT_NO_SUSPECT, 0, NULL);
		return 0;
	}
	FrPlayer *q = &fr.pl[j];
	int level = q->level;
	int prev = Players_Bound();
	Players_Bind(j);
	int fine = t->fine * level;
	fine = fine > g_pWorld->money ? (g_pWorld->money > 0 ? g_pWorld->money : 0) : fine;
	g_pWorld->money -= fine;
	Alarm_Set(&g_pWorld->alarm, 0.0f);
	Players_Bind(i);
	g_pWorld->money += t->reward * level;
	Players_Bind(prev);
	FactionWanted_Init(&q->wanted);
	q->level = 0;
	int before = Fr_Rank(p);
	p->arrests++;
	Con_Printf("factions: player %d arrested player %d (wanted %d): fine %d, reward %d, %d arrests\n", i, j, level,
		fine, t->reward * level, p->arrests);
	Fr_Notice(i, FT_ARREST_MADE, t->reward * level, NULL);
	if (Fr_Rank(p) > before) {
		Fr_Notice(i, FT_RANK_UP, Fr_Rank(p), Fr_RankName(p->faction, Fr_Rank(p), rank, sizeof(rank)));
	}
	Fr_Jail(j, &t->f[p->faction], fine);
	/* the rank and the money in the store at once */
	Travel_Report(i);
	return 1;
}

static int Fr_Backup(int i, cEntity *e) {
	const FactionTable *t = Faction_Table();
	if (cWorld_GetList(g_pWorld, "OutCop") && t->backupShip[0]) {
		/* outdoors: police ships of the wave type at a free spawn place near the officer (VehiclesNPC.MakeAI as
		 * OutPolice.MakePoli does, but not far from it) into the police cars; each goes for the nearest wanted
		 * player */
		int prev = Players_Bound();
		Players_Bind(i);
		int made = 0, j = Fr_Suspect(i, e, FR_BACKUP_OUTDOOR);
		for (int k = 0; k < t->backupCount; k++) {
			nativeVal_t ship = { 0 }, list;
			Native_SetStr(&list, "OutCop");
			if (Mode_Import("VehiclesNPC") && PORT_CALL(VehiclesNPC, MakeAI, &ship, "ssi", t->backupShip, "Police", 0L) &&
				Mode_VSet(&ship, "EntityList", &list)) {
				made++;
				if (j >= 0) {
					ExtPolice_CallCop(Players_Get(j));
				}
			} else {
				Native_ErrPrint();
			}
			Native_Free(&ship);
			Native_Free(&list);
		}
		Players_Bind(prev);
		Con_Printf("factions: player %d called %d police ships%s%d\n", i, made, j >= 0 ? " on player " : ", no suspect ",
			j);
	} else {
		/* indoors: the level's gears go for the nearest wanted player, else a sentinel watches the officer */
		int j = Fr_Suspect(i, e, FR_BACKUP_INDOOR);
		if (j >= 0) {
			AICtrl_GearsChase(&g_pWorld->logic.aiCtrl, Players_Get(j));
		} else {
			AICtrl_SendSentinelToWatch(&g_pWorld->logic.aiCtrl, &e->pos);
		}
		Con_Printf("factions: player %d called the gears%s%d\n", i, j >= 0 ? " on player " : ", no suspect ", j);
	}
	Fr_Notice(i, FT_BACKUP, 0, NULL);
	return 1;
}

static int Fr_Ship(int i, cEntity *e) {
	FrPlayer *p = &fr.pl[i];
	if (NetPl_Kind(e) != NETREP_CHAR) {
		Fr_Notice(i, FT_ON_FOOT, 0, NULL);
		return 0;
	}
	p->callShip = 1;
	Rep_TestSwitch(i, 1);
	int ok = !p->callShip;
	p->callShip = 0;
	Fr_Notice(i, ok ? FT_SHIP : FT_NO_SHIP, 0, NULL);
	return ok;
}

cEntity *Faction_CallShip(int i, const void *at, float yaw) {
	const FactionTable *t = Faction_Table();
	FrPlayer *p = Fr_Player(i);
	const vec3_t *pos = at;
	char model[FACTION_NAME];
	if (!p || !p->callShip || !t || p->faction < 0 || !t->f[p->faction].nships) {
		return NULL;
	}
	const FactionDef *f = &t->f[p->faction];
	int r = Fr_Rank(p);
	Str_Copy(model, f->ships[(r > f->nships ? f->nships : r) - 1], sizeof(model));
	cEntity *ship = cWorld_CreateEntity(g_pWorld, "ParkedPoli", pos->x, pos->y, pos->z, TYEN_CAR);
	if (!ship) {
		return NULL;
	}
	vec3_t ang = { 0.0f, yaw, 0.0f };
	Obj_SetAng(ship, &ang);
	if (!PORT_CALL(Vehicles, SetStdProperties, NULL, "sss", ship->name, model, "P_None") ||
		!PORT_CALL(Parking, SetShipAsParked, NULL, "s", ship->name)) {
		Native_ErrPrint();
	}
	p->callShip = 0;
	Con_Printf("factions: player %d called police ship %s (%s, rank %d)\n", i, ship->name, model, r);
	return ship;
}

static void Fr_Request(int i, int a) {
	const FactionTable *t = Faction_Table();
	FrPlayer *p = Fr_Player(i);
	cEntity *e = Players_Get(i);
	if (!p || !e || a < 0 || a >= FA_COUNT) {
		return;
	}
	p->requests++;
	if (!Faction_Freeroam()) {
		Fr_Notice(i, FT_NOT_FREEROAM, 0, NULL);
		return;
	}
	double now = Fr_Now();
	if (a != FA_JOIN && (p->faction < 0 || t->f[p->faction].cooldown[a] < 0.0f)) {
		Fr_Notice(i, FT_MEMBERS_ONLY, 0, NULL);
		return;
	}
	if (now < p->ready[a]) {
		Fr_Notice(i, FT_NOT_READY, 0, NULL);
		return;
	}
	int prev = Players_Bound();
	Players_Bind(i);
	int ok = 0;
	switch (a) {
	case FA_JOIN:
		ok = Fr_Join(i, e);
		break;
	case FA_SIREN:
		ok = Fr_Siren(i, e);
		break;
	case FA_ARMOUR:
		ok = Fr_Armour(i, e);
		break;
	case FA_ARREST:
		ok = Fr_Arrest(i, e);
		break;
	case FA_BACKUP:
		ok = Fr_Backup(i, e);
		break;
	case FA_SHIP:
		ok = Fr_Ship(i, e);
		break;
	}
	Players_Bind(prev);
	if (ok) {
		int f = p->faction >= 0 ? p->faction : FACTION_NONE;
		float cd = f >= 0 && t->f[f].cooldown[a] > 0.0f ? t->f[f].cooldown[a] : 0.0f;
		p->ready[a] = now + cd;
		p->done++;
	}
}

byte Faction_Message(Net2Host *host, int conn, const uint8_t *data, int size) {
	(void)host;
	if (size < 1 || data[0] != FACTION_MSG_REQ) {
		return 0;
	}
	if (size >= 2) {
		Fr_Request(Rep_ConnPlayer(conn), data[1]);
	}
	return 1;
}

/* ---- the crimes and the wanted level ---- */

/* the faction of an entity that is not a player's (its class, else the model of a ship or the pilot's class) */
static int Fr_FactionOf(cEntity *e) {
	const FactionTable *t = Faction_Table();
	cActCtrl *a = Entity_IsType(e, TYEN_WITH_LIFE) ? WLife(e)->actCtrl : NULL;
	if (Entity_IsType(e, TYEN_WALKCHAR)) {
		return Faction_OfClass(t, a && a->cls ? a->cls->name : "");
	}
	if (!Entity_IsType(e, TYEN_CAR)) {
		return FACTION_NONE;
	}
	int f = Faction_OfShip(t, Fr_Model(e));
	if (f < 0) {
		char *pil = NULL;
		if (Entity_VarS(e, "MPil0", &pil, 0) && pil && !strncmp(pil, "P_", 2)) {
			f = Faction_OfClass(t, pil + 2);
		}
	}
	return f;
}

static byte Fr_IsPolice(int f) {
	const FactionTable *t = Faction_Table();
	return f >= 0 && !crt_stricmp(t->f[f].name, "Police");
}

void Faction_Crime(cEntity *victim, cEntity *attacker, byte died) {
	if (!Faction_Freeroam() || !attacker || attacker == victim) {
		return;
	}
	int i = Players_Of(attacker);
	FrPlayer *p = Fr_Player(i);
	if (!p) {
		return;
	}
	int crime;
	int j = Players_Of(victim);
	if (j >= 0) {
		/* police against a wanted player is no crime */
		if (Fr_IsPolice(p->faction) && p->level == 0 && fr.pl[j].level > 0) {
			return;
		}
		crime = Fr_IsPolice(fr.pl[j].faction) ? (died ? FCRIME_KILL_POLICE : FCRIME_HURT_POLICE) :
			died ? FCRIME_KILL_PLAYER : FCRIME_HURT_PLAYER;
	} else {
		/* an empty ship is nobody's: no crime */
		if (Entity_IsType(victim, TYEN_CAR) && !Entity_GetAI(victim)) {
			return;
		}
		if (!Entity_IsType(victim, TYEN_CAR) && !Entity_IsType(victim, TYEN_WALKCHAR)) {
			return;
		}
		byte police = Fr_IsPolice(Fr_FactionOf(victim));
		crime = police ? (died ? FCRIME_KILL_POLICE : FCRIME_HURT_POLICE) :
			died ? FCRIME_KILL_CIVILIAN : FCRIME_HURT_CIVILIAN;
	}
	if (fr.ncrimes < FR_CRIMES) {
		fr.crimes[fr.ncrimes].player = (int8_t)i;
		fr.crimes[fr.ncrimes].crime = (int8_t)crime;
		fr.ncrimes++;
	} else {
		fr.crimesLost++;
	}
}

byte Faction_MayHurt(cEntity *attacker, cEntity *victim) {
	if (!Faction_Freeroam() || !fr.policePvp) {
		return 0;
	}
	FrPlayer *a = Fr_Player(Players_Of(attacker)), *v = Fr_Player(Players_Of(victim));
	if (!a || !v) {
		return 0;
	}
	return (Fr_IsPolice(a->faction) && a->level == 0 && v->level > 0) ||
		(Fr_IsPolice(v->faction) && v->level == 0 && a->level > 0);
}

/* the crimes of the frame into the wanted levels; a level that rises turns the player's alarm on */
static void Fr_Crimes(void) {
	const FactionTable *t = Faction_Table();
	double now = Fr_Now();
	for (int k = 0; k < fr.ncrimes; k++) {
		int i = fr.crimes[k].player;
		FrPlayer *p = &fr.pl[i];
		if (FactionWanted_Crime(t, &p->wanted, fr.crimes[k].crime, now)) {
			Con_Printf("factions: player %d crime %s, wanted points %.1f\n", i, Faction_CrimeName(fr.crimes[k].crime),
				(double)p->wanted.points);
		}
	}
	fr.ncrimes = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		FrPlayer *p = &fr.pl[i];
		if (!Players_Get(i)) {
			continue;
		}
		const cAlarm *al = Players_Alarm(i);
		/* the alarm the police of the level raised: the player is wanted at least at level 1 */
		if (al->on && p->wanted.points < t->wanted[0]) {
			p->wanted.points = t->wanted[0];
		}
		FactionWanted_Decay(t, &p->wanted, 1.0f / NETREP_TICK_HZ, al->on);
		int level = FactionWanted_Level(t, &p->wanted);
		if (level > p->level) {
			int prev = Players_Bound();
			Players_Bind(i);
			Alarm_Set(&g_pWorld->alarm, 1.0f);
			Players_Bind(prev);
			Fr_Notice(i, FT_WANTED, level, NULL);
		}
		p->level = level;
	}
}

/* ---- the siren ---- */

static cEntity *Fr_SirenShip(FrPlayer *p, int i) {
	cEntity *e = p->siren && p->ship ? World_FindEntity(p->shipName) : NULL;
	return e && e == p->ship && e == Players_Get(i) ? e : NULL;
}

uint8_t Faction_Flags(cEntity *e) {
	int i = fr.freeroam && Entity_IsType(e, TYEN_CAR) ? Players_Of(e) : -1;
	return i >= 0 && Fr_SirenShip(&fr.pl[i], i) ? NETREP_FL_SIREN : 0;
}

/* traffic within the siren's radius moves away from the ship for a while, then goes back to its route */
static void Fr_Sirens(uint64_t tick) {
	const FactionTable *t = Faction_Table();
	double now = Fr_Now();
	for (int k = 0; k < FR_YIELDS; k++) {
		FrYield *y = &fr.yield[k];
		if (!y->e || now < y->until) {
			continue;
		}
		cEntity *e = World_FindEntity(y->name);
		cAIVehicle *ai = e == y->e && !e->deadNode.target ? Entity_GetAI(e) : NULL;
		if (ai && cAIVehicle_GetState(ai) == AIV_GET_AWAY_STOP) {
			/* the route again with the traffic's defaults (Traffic_Patrol) */
			cAIVehicle_SetState(ai, AIV_IN_ROUTE, 1, 5.0, 2.0);
		}
		y->e = NULL;
	}
	if (tick % FR_SIREN_EVERY) {
		return;
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		FrPlayer *p = &fr.pl[i];
		cEntity *s = Fr_SirenShip(p, i);
		if (!s) {
			p->siren = 0;
			continue;
		}
		for (cEntity *v = cWorld_GetFirstEntity(g_pWorld); v; v = cEntity_Next(v)) {
			if (v == s || !Entity_IsType(v, TYEN_CAR) || v->deadNode.target || Players_Of(v) >= 0 ||
				Fr_DistSq(v, &s->pos) > t->sirenRadius * t->sirenRadius) {
				continue;
			}
			/* the ships of the routes (the big ones of the fixed lanes, AIV_TRAFFIC, cannot leave them: the graph
			 * of the AI takes no ship that large) */
			cAIVehicle *ai = v->active ? Entity_GetAI(v) : NULL;
			if (!ai || cAIVehicle_GetState(ai) != AIV_IN_ROUTE || !(Obj_GetRadius(v) < g_NWNodeRadius)) {
				continue;
			}
			int free = -1;
			for (int k = 0; k < FR_YIELDS && free < 0; k++) {
				free = fr.yield[k].e ? -1 : k;
			}
			if (free < 0) {
				return;
			}
			if (cAIVehicle_SetState(ai, AIV_GET_AWAY_STOP, s, (double)t->sirenAway)) {
				FrYield *y = &fr.yield[free];
				y->e = v;
				Str_Copy(y->name, v->name, sizeof(y->name));
				y->until = now + t->sirenTime;
				Con_Printf("factions: %s gives way to the siren of player %d\n", v->name, i);
			}
		}
	}
}

void Faction_Tick(Net2Host *host, uint64_t tick) {
	if (!Faction_Freeroam()) {
		return;
	}
	Fr_Crimes();
	Fr_Sirens(tick);
	for (int i = 0; i < PLAYERS_MAX; i++) {
		FrPlayer *p = &fr.pl[i];
		if (!host || !Players_Get(i)) {
			continue;
		}
		if (p->jailFine >= 0) {
			Fr_Notice(i, FT_ARRESTED, p->jailFine, NULL);
			p->jailFine = -1;
		}
		Fr_SendState(host, i);
	}
}

/* ---- tests ---- */

byte Faction_Command(const char *line) {
	char what[32], arg[64];
	int i;
	arg[0] = 0;
	if (sscanf(line, "faction %d %31s %63s", &i, what, arg) < 2) {
		return 0;
	}
	FrPlayer *p = Fr_Player(i);
	if (!p) {
		Con_Printf("factions: no player %d\n", i);
		return 1;
	}
	if (!strcmp(what, "dump")) {
		/* the dummies and the entities of the level whose names match (the office and jail of the table) */
		char found[64];
		vec3_t pos, ang;
		DummySearch_Start(arg[0] ? arg : "*", 1);
		while (DummySearch_NextEuler(found, sizeof(found), &pos, &ang)) {
			Con_Printf("factions: dummy %s at %.0f %.0f %.0f\n", found, (double)pos.x, (double)pos.y, (double)pos.z);
		}
		cEntity *me = Players_Get(i);
		for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
			cActCtrl *a = Entity_IsType(e, TYEN_WITH_LIFE) ? WLife(e)->actCtrl : NULL;
			byte car = !strcmp(arg, "cars") && Entity_IsType(e, TYEN_CAR) && Entity_GetAI(e);
			if (car || (me && Fr_DistSq(e, &me->pos) < 3000.0f * 3000.0f)) {
				cAIVehicle *ai = Entity_IsType(e, TYEN_CAR) ? Entity_GetAI(e) : NULL;
				Con_Printf("factions: entity %s (%s) at %.0f %.0f %.0f, ai state %d\n", e->name,
					a && a->cls ? a->cls->name : "-", (double)e->pos.x, (double)e->pos.y, (double)e->pos.z,
					ai ? cAIVehicle_GetState(ai) : -1);
			}
		}
		return 1;
	}
	if (!strcmp(what, "wanted")) {
		p->wanted.points = (float)crt_atof(arg);
		Con_Printf("factions: player %d wanted points %.1f\n", i, (double)p->wanted.points);
		return 1;
	}
	if (!strcmp(what, "set")) {
		/* tests: a member without the office */
		const FactionTable *t = Faction_Table();
		p->faction = t ? Faction_Find(t, arg) : FACTION_NONE;
		p->armoured = 0;
		Str_Copy(p->charCls, t && p->faction >= 0 ? t->f[p->faction].play : FR_DEFAULT_CHAR, sizeof(p->charCls));
		Fr_Rebody(i, NULL, 0.0f);
		Con_Printf("factions: player %d set to %s\n", i, t && p->faction >= 0 ? t->f[p->faction].name : "-");
		return 1;
	}
	if (!strcmp(what, "crime")) {
		for (int c = 0; c < FCRIME_COUNT; c++) {
			if (!strcmp(arg, Faction_CrimeName(c)) && fr.ncrimes < FR_CRIMES) {
				fr.crimes[fr.ncrimes].player = (int8_t)i;
				fr.crimes[fr.ncrimes].crime = (int8_t)c;
				fr.ncrimes++;
			}
		}
		return 1;
	}
	if (!strcmp(what, "arrests")) {
		p->arrests = crt_atoi(arg);
		Con_Printf("factions: player %d arrests %d, rank %d\n", i, p->arrests, Fr_Rank(p));
		return 1;
	}
	if (!strcmp(what, "status")) {
		const FactionTable *t = Faction_Table();
		Con_Printf("factions: player %d faction %s rank %d arrests %d wanted %d (%.1f points) armour %d siren %d class "
			"%s, %u requests %u done\n", i, t && p->faction >= 0 ? t->f[p->faction].name : "-", Fr_Rank(p), p->arrests,
			p->level, (double)p->wanted.points, p->armoured, p->siren, Faction_CharClass(i), p->requests, p->done);
		return 1;
	}
	for (int a = 0; a < FA_COUNT; a++) {
		if (!strcmp(what, Faction_AbilityName(a)) || (a == FA_JOIN && !strcmp(what, "leave"))) {
			Fr_Request(i, a);
			return 1;
		}
	}
	Con_Printf("factions: unknown test line %s\n", line);
	return 1;
}

/* ================================ the client ================================ */

typedef struct FrShip {
	cEntity *e;
	char name[FR_NAME];
	byte siren;
} FrShip;

static struct {
	byte have;
	int faction, rank, wanted, arrests;
	uint8_t flags, ready;
	float prev[FA_COUNT];			/* the keys of the last frame */
	FrShip ships[FR_SHIPS];
	double botStart;
	char bot[128];
	byte botTried;
	uint32_t botDone;				/* the steps of SCRAP_PLAYBOT_FACTION sent */
} frc = { .faction = FACTION_NONE };

void Faction_ClientReset(void) {
	memset(frc.ships, 0, sizeof(frc.ships));
	frc.have = 0;
	frc.faction = FACTION_NONE;
	frc.botStart = 0.0;
}

void Faction_ClientMessage(const uint8_t *data, int size) {
	char name[64], text[FR_TEXT];
	NetRepReader r;
	if (size < 2) {
		return;
	}
	NetRepReader_Init(&r, data + 2, size - 2);
	if (data[1] == FACTION_ST_STATE) {
		uint8_t st[7];
		for (int k = 0; k < 7; k++) {
			st[k] = (uint8_t)NetRep_GetU8(&r);
		}
		if (r.bad) {
			return;
		}
		frc.have = 1;
		frc.faction = st[0] == 255 ? FACTION_NONE : st[0];
		frc.rank = st[1];
		frc.wanted = st[2];
		frc.arrests = st[3] | st[4] << 8;
		frc.flags = st[5];
		frc.ready = st[6];
		Con_Printf("factions: faction %d rank %d wanted %d arrests %d flags %d ready %02x\n", frc.faction, frc.rank,
			frc.wanted, frc.arrests, frc.flags, frc.ready);
		return;
	}
	if (data[1] != FACTION_ST_NOTICE) {
		return;
	}
	int id = (int)NetRep_GetU8(&r);
	int arg = NetRep_GetSVar(&r);
	NetRep_GetStr(&r, name, sizeof(name));
	if (r.bad || id < 0 || id >= FT_COUNT) {
		return;
	}
	Fr_Text(text, sizeof(text), id, arg, name);
	Con_Printf("factions: %s\n", text);
	if (!g_HeadlessPlay) {
		Text_Message(FR_NOTICE_MS, "%s", text);
	}
}

static void Fr_ClientSend(int a) {
	uint8_t buf[2] = { FACTION_MSG_REQ, (uint8_t)a };
	if (Watch_SendReliable(buf, sizeof(buf))) {
		Con_Printf("factions: request %s\n", Faction_AbilityName(a));
	}
}

/* tests: SCRAP_PLAYBOT_FACTION=<s>:<ability>,... sends each request once, that many seconds after a level started */
static void Fr_ClientBot(void) {
	if (!frc.botTried) {
		const char *b = getenv("SCRAP_PLAYBOT_FACTION");
		Str_Copy(frc.bot, b ? b : "", sizeof(frc.bot));
		frc.botTried = 1;
	}
	if (!frc.bot[0]) {
		return;
	}
	double now = (double)Plat_RealNs() / 1e9;
	if (frc.botStart == 0.0) {
		frc.botStart = now;
	}
	char list[128];
	Str_Copy(list, frc.bot, sizeof(list));
	uint32_t k = 0;
	for (char *tok = strtok(list, ","); tok && k < 32; tok = strtok(NULL, ","), k++) {
		char *c = strchr(tok, ':');
		if (!c || (frc.botDone >> k & 1) || now - frc.botStart < crt_atof(tok)) {
			continue;
		}
		frc.botDone |= 1u << k;
		for (int a = 0; a < FA_COUNT; a++) {
			if (!strcmp(c + 1, Faction_AbilityName(a))) {
				Fr_ClientSend(a);
			}
		}
	}
}

void Faction_ClientFrame(void) {
	char line[FR_TEXT], rank[32];
	const FactionTable *t = Faction_Table();
	if (!g_Play || !Play_Entity() || !t) {
		return;
	}
	Fr_ClientBot();
	/* the keys: QMsg1..QMsg6 of the Racer set, on foot as in a ship (no menu open) */
	uint32_t set = Input_GetActionSet();
	for (int a = 0; a < FA_COUNT; a++) {
		float v = Input_BoundValue(ACTSET_RACER, RACER_QMSG1 + a, 0);
		if (v > 0.5f && frc.prev[a] <= 0.5f && (set == ACTSET_RACER || set == ACTSET_WALK)) {
			Fr_ClientSend(a);
		}
		frc.prev[a] = v;
	}
	if (g_HeadlessPlay || !frc.have || !(frc.flags & 4)) {
		return;
	}
	int w, h;
	Graphic2D_GetScreen(&w, &h);
	int y = (int)((float)h * 0.30f);
	if (frc.faction >= 0 && frc.faction < t->n) {
		Str_PrintfQuiet(line, sizeof(line), g_FactionTexts[FT_HUD_MEMBER], t->f[frc.faction].name,
			Fr_RankName(frc.faction, frc.rank, rank, sizeof(rank)));
		Text_Printf((int)((float)w * 0.02f), y, "%s", line);
		y += (int)((float)h * 0.035f);
	}
	if (frc.wanted > 0) {
		Text_Printf((int)((float)w * 0.02f), y, g_FactionTexts[FT_HUD_WANTED], frc.wanted);
	}
}

/* the client's record of a player's ship (a free one for add), NULL */
static FrShip *Fr_ShipOf(cEntity *v, byte add) {
	FrShip *free = NULL;
	for (int k = 0; k < FR_SHIPS; k++) {
		FrShip *s = &frc.ships[k];
		if (s->e && (World_FindEntity(s->name) != s->e)) {
			s->e = NULL;
		}
		if (s->e == v && v) {
			return s;
		}
		if (!s->e && !free) {
			free = s;
		}
	}
	if (!add || !free) {
		return NULL;
	}
	free->e = v;
	Str_Copy(free->name, v->name, sizeof(free->name));
	free->siren = 0;
	return free;
}

static void Fr_SirenSound(cEntity *v, byte on) {
	cSndItem *it = PtrArray_Get(&Veh(v)->wl.obj.snd, FR_SIREN_SND);
	if (!it) {
		return;
	}
	it->volume = on ? it->vols[0] : 0.0f;
	it->pitch = on ? it->pitches[0] : 0.0f;
	if (on && !it->playing) {
		it->playing = 1;
		cSndItem_UpdateAt(it, v);
		cSndItem_Restart(it);
	}
}

void Faction_EntityFlags(cEntity *e, uint8_t flags) {
	if (!e || !Entity_IsType(e, TYEN_CAR) || !(flags & NETREP_FL_PLAYER)) {
		return;
	}
	FrShip *s = Fr_ShipOf(e, 1);
	byte on = (flags & NETREP_FL_SIREN) != 0;
	if (s && s->siren != on) {
		s->siren = on;
		if (strstr(Veh(e)->typeName, "Poli")) {
			Fr_SirenSound(e, on);
		}
	}
}

int Faction_SirenOf(cEntity *v) {
	if (!g_Watch) {
		return -1;
	}
	FrShip *s = Fr_ShipOf(v, 0);
	return s ? s->siren : -1;
}
