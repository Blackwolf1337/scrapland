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
#include "scrap/chartype.h"
#include "scrap/console.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/faction.h"
#include "scrap/fx.h"
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
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/sound.h"
#include "scrap/text.h"
#include "scrap/travel.h"
#include "scrap/vehicleattack.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/weapons.h"
#include "scrap/world.h"

#include "game/App/FactionInt.h"

#define FR_CRIMES 256				/* crimes kept between two ticks */
#define FR_YIELDS 64				/* traffic ships moving away from a siren */
#define FR_SIREN_EVERY (NETREP_TICK_HZ / 2)	/* ticks between the siren's looks for traffic */
#define FR_BACKUP_INDOOR 6000.0f	/* units: the gears go for a wanted player this near the officer */
#define FR_BACKUP_OUTDOOR 30000.0f	/* and the police ships */
#define FR_SHIPS 64					/* players' ships a client keeps the siren of */
#define FR_NOTICE_MS 3000
#define FR_NEWS_MS 9000
#define FR_DEFAULT_CHAR "Dtritus"
#define FR_SIREN_SND 0x1f			/* the vehicle sound of the siren (snd31 of OutSound) */
#define FR_DESK_PREFIX "FactionDesk_"
#define FR_LAST_BY 300.0			/* s the last player that hurt one stays its revenge target */

/* every text a player sees (a writing pass comes later); %s are the names in order, %d the number */
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
	[FT_SHIP] = "Faction ship called.",
	[FT_NO_SHIP] = "No room for a ship here.",
	[FT_WANTED] = "Wanted level %d!",
	[FT_NOT_FREEROAM] = "No factions on this server.",
	[FT_YOU_ARE_WANTED] = "Not while you are wanted.",
	[FT_NO_ONE_NEAR] = "No one near.",
	[FT_ONLY_SHIP] = "Only in a ship.",
	[FT_ONLY_OUTDOORS] = "Only outdoors.",
	[FT_NO_MONEY] = "Not enough money.",
	[FT_PROTECTED] = "%s is protected here.",
	[FT_NEED_PERMIT] = "You need a ship permit from the Bureaucracy.",
	[FT_HEAL_GIVE] = "You healed %s. Paid: %d.",
	[FT_HEAL_GOT] = "%s healed you. Price: %d.",
	[FT_NO_WOUNDED] = "No wounded player near.",
	[FT_RAY_MISS] = "The ray hit nothing.",
	[FT_RAY_HIT] = "The ray hit %s.",
	[FT_NO_BODY] = "No body near to revive.",
	[FT_REVIVE_GIVE] = "You revived %s. Paid: %d.",
	[FT_REVIVE_GOT] = "%s revived you.",
	[FT_STEAL_GIVE] = "You took %d from %s.",
	[FT_STEAL_GOT] = "%s robbed you: %d.",
	[FT_SUCK_GIVE] = "Sucked %d.",
	[FT_LOAN_GIVE] = "Loan to %s. Commission: %d.",
	[FT_LOAN_GOT] = "Loan: %d. Your income pays it back.",
	[FT_HAS_DEBT] = "%s still owes the bank.",
	[FT_DEBT_PAID] = "Your loan is paid back.",
	[FT_BRIBE] = "Bribe paid: %d. Wanted level down.",
	[FT_BRIBE_GOT] = "%s paid you a bribe: %d.",
	[FT_NOT_WANTED] = "You are not wanted.",
	[FT_BOUNTY_SET] = "Bounty on %s: %d.",
	[FT_BOUNTY_ON_YOU] = "Bounty on you: %d.",
	[FT_BOUNTY_FULL] = "That bounty is at the most.",
	[FT_BOUNTY_CLAIM] = "Bounty on %s claimed: %d.",
	[FT_CONTRACT] = "Contract: %s, %d.",
	[FT_NO_CONTRACTS] = "No contracts here.",
	[FT_HIRED] = "Mercenaries hired: %d.",
	[FT_HIRE_GONE] = "Your mercenaries left.",
	[FT_HEAVY] = "Heavy loadout on.",
	[FT_SLOW_ON] = "Bullet time!",
	[FT_SLOWED] = "Bullet time: you are slowed down.",
	[FT_ASLEEP] = "Bla bla bla... you fall asleep.",
	[FT_BLABLA] = "Asleep: %d.",
	[FT_PERMIT_GIVE] = "Permit for %s. Paid: %d.",
	[FT_PERMIT_GOT] = "You got a ship permit.",
	[FT_HAS_PERMIT] = "Everyone near has a permit.",
	[FT_FINE_GIVE] = "Fined %s: %d.",
	[FT_FINE_GOT] = "Fined: %d.",
	[FT_NO_OFFENDER] = "No offender near.",
	[FT_PHOTO_GIVE] = "Photo of %s! Reward: %d.",
	[FT_PHOTO_GOT] = "The Press caught your crime on camera.",
	[FT_NO_PHOTO] = "Nothing to report.",
	[FT_JOB] = "Deliver the data pack to %s in %d s.",
	[FT_JOB_DONE] = "Delivered! Reward: %d.",
	[FT_JOB_LATE] = "Too late: the delivery failed.",
	[FT_JOB_AT_DESK] = "Take jobs at the Press desk.",
	[FT_JOB_HAVE] = "Deliver to %s first.",
	[FN_ARREST] = "%s arrested %s.",
	[FN_BOUNTY] = "%s claimed the bounty on %s.",
	[FN_PHOTO] = "%s caught on camera by %s.",
	[FN_REVIVE] = "%s brought %s back to life.",
	[FN_STEAL] = "%s robbed %s.",
	[FN_DELIVERY] = "%s delivered the news in %d s.",
	[FN_JOINED] = "%s joined the %s.",
	[FT_HUD_MEMBER] = "%s - %s",
	[FT_HUD_WANTED] = "Wanted: %d",
	[FT_HUD_RANK] = "Rank %d",
	[FT_HUD_BOUNTY] = "Bounty on you: %d",
	[FT_HUD_DEBT] = "Debt: %d",
	[FT_HUD_JOB] = "Job: %s, %d s",
	[FT_HUD_PERMIT] = "Ship permit",
	[FT_HUD_NEWS] = "Scrap News: %s",
	[FT_HUD_KEYS] = "Keys: %s",
	[FT_DESK] = "%s office",
};
/* the rank names of each faction (the table's order) */
static const char *const g_FactionRanks[][5] = {
	{ "Rookie", "Officer", "Sergeant", "Lieutenant", "Captain" },
	{ "Novice", "Brother", "Priest", "Bishop", "Archbishop" },
	{ "Clerk", "Teller", "Broker", "Director", "Bank Master" },
	{ "Intern", "Clerk", "Officer", "Head Officer", "Deputy Mayor" },
	{ "Runner", "Reporter", "Editor", "Anchor", "Chief Editor" },
	{ "Thug", "Gun", "Hunter", "Veteran", "Warlord" },
};
/* the names of the abilities on the client's line of keys */
static const char *const g_AbilityLabels[FA_COUNT] = {
	"Join", "Siren", "Armour", "Arrest", "Backup", "Ship", "Heal", "Ray", "Revive", "Steal", "Suck", "Loan", "Bribe",
	"Bounty", "Contracts", "Hire", "Heavy", "Bullet time", "Bla-bla", "Permit", "Fine", "Photo", "Courier",
};

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
	if (f >= 0 && f < (int)(sizeof(g_FactionRanks) / sizeof(g_FactionRanks[0])) && rank >= 1 && rank <= 5) {
		return g_FactionRanks[f][rank - 1];
	}
	Str_PrintfQuiet(buf, cap, g_FactionTexts[FT_HUD_RANK], rank);
	return buf;
}

/* a text of the table: %s the names in order, %d the number (no format string of printf) */
static void Fr_Format(char *out, int cap, const char *fmt, int num, const char *name, const char *name2) {
	const char *names[2] = { name ? name : "", name2 ? name2 : "" };
	int n = 0, k = 0;
	for (const char *f = fmt; *f && n < cap - 1; f++) {
		char tmp[16];
		const char *ins = NULL;
		if (f[0] == '%' && f[1] == 's') {
			ins = names[k < 2 ? k : 1];
			k++;
		} else if (f[0] == '%' && f[1] == 'd') {
			Str_PrintfQuiet(tmp, sizeof(tmp), "%d", num);
			ins = tmp;
		}
		if (!ins) {
			out[n++] = *f;
			continue;
		}
		for (; *ins && n < cap - 1; ins++) {
			out[n++] = *ins;
		}
		f++;
	}
	out[n] = 0;
}

static void Fr_Text(char *out, int cap, int id, int num, const char *name, const char *name2) {
	Fr_Format(out, cap, g_FactionTexts[id], num, name, name2);
}

/* ================================ the instance ================================ */

typedef struct FrYield {
	cEntity *e;
	char name[FR_NAME];
	double until;
} FrYield;

static struct {
	byte freeroam;
	byte policePvp, factionPvp;
	FrPlayer pl[PLAYERS_MAX];
	struct {
		int8_t player;
		int8_t crime;
	} crimes[FR_CRIMES];
	int ncrimes;
	FrYield yield[FR_YIELDS];
	uint32_t crimesLost;
} fr = { .policePvp = 1, .factionPvp = 1 };

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
	if (!crt_stricmp("factionpvp", param)) {
		fr.factionPvp = crt_atoi(ext) != 0;
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

FrPlayer *Fr_Player(int i) {
	return i >= 0 && i < PLAYERS_MAX ? &fr.pl[i] : NULL;
}

double Fr_Now(void) {
	return Instance_Time();
}

int64_t Fr_Unix(void) {
	return (int64_t)(Coord_UnixMs() / 1000u);
}

int Fr_Rank(const FrPlayer *p) {
	const FactionTable *t = Faction_Table();
	return t && p->faction >= 0 ? Faction_Rank(&t->f[p->faction], p->deeds) : 0;
}

static int64_t Fr_KvInt(const char *state, int len, const char *key, int64_t def) {
	char v[32];
	return CoordKv_Get(state, len, key, v, sizeof(v)) && v[0] ? (int64_t)crt_atof(v) : def;
}

void Faction_Admit(int i, const char *state, int len) {
	char v[96];
	FrPlayer *p = Fr_Player(i);
	const FactionTable *t = Faction_Table();
	if (!p) {
		return;
	}
	memset(p, 0, sizeof(*p));
	p->faction = FACTION_NONE;
	p->jailFine = -1;
	p->lastBy = -1;
	p->lastCrime = -1.0;
	FactionWanted_Init(&p->wanted);
	Str_PrintfQuiet(p->name, sizeof(p->name), "Player %d", i + 1);
	if (!state) {
		return;
	}
	if (CoordKv_Get(state, len, "name", v, sizeof(v)) && v[0]) {
		Str_Copy(p->name, v, sizeof(p->name));
	}
	p->jailFine = (int)Fr_KvInt(state, len, "jailfine", -1);
	if (CoordKv_Get(state, len, "char", v, sizeof(v))) {
		Str_Copy(p->charCls, v, sizeof(p->charCls));
	}
	/* the deeds (M6a stored the police's arrests) */
	p->deeds = (int)Fr_KvInt(state, len, "deeds", Fr_KvInt(state, len, "arrests", 0));
	p->deeds = p->deeds < 0 ? 0 : p->deeds;
	p->bounty = (int)Fr_KvInt(state, len, "bounty", 0);
	p->bountyEnds = Fr_KvInt(state, len, "bountyends", 0);
	FactionBounty_Live(&p->bounty, &p->bountyEnds, Fr_Unix());
	p->debt = (int)Fr_KvInt(state, len, "debt", 0);
	p->debt = p->debt < 0 ? 0 : p->debt;
	p->permit = Fr_KvInt(state, len, "permit", 0) != 0;
	/* job: level/dummy/start/end */
	if (CoordKv_Get(state, len, "job", v, sizeof(v)) && v[0]) {
		char *lv = strtok(v, "/"), *dm = strtok(NULL, "/"), *st = strtok(NULL, "/"), *en = strtok(NULL, "/");
		if (lv && dm && st && en) {
			Str_Copy(p->jobLevel, lv, sizeof(p->jobLevel));
			Str_Copy(p->jobDummy, dm, sizeof(p->jobDummy));
			p->jobStart = (int64_t)crt_atof(st);
			p->jobEnds = (int64_t)crt_atof(en);
		}
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
		Str_Copy(p->charCls, Faction_PlayClass(&t->f[p->faction], Fr_Rank(p)), sizeof(p->charCls));
	}
}

void Faction_State(int i, char *state, int *len, int cap) {
	char v[96];
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
	Str_PrintfQuiet(v, sizeof(v), "%d", p->deeds);
	CoordKv_Set(state, len, cap, "deeds", v);
	CoordKv_Set(state, len, cap, "arrests", "");
	Str_PrintfQuiet(v, sizeof(v), "%.2f", (double)p->wanted.points);
	CoordKv_Set(state, len, cap, "wanted", v);
	Str_PrintfQuiet(v, sizeof(v), "%d", p->jailFine);
	CoordKv_Set(state, len, cap, "jailfine", p->jailFine >= 0 ? v : "");
	Str_PrintfQuiet(v, sizeof(v), "%d", p->bounty);
	CoordKv_Set(state, len, cap, "bounty", p->bounty > 0 ? v : "");
	Str_PrintfQuiet(v, sizeof(v), "%lld", (long long)p->bountyEnds);
	CoordKv_Set(state, len, cap, "bountyends", p->bounty > 0 ? v : "");
	Str_PrintfQuiet(v, sizeof(v), "%d", p->debt);
	CoordKv_Set(state, len, cap, "debt", p->debt > 0 ? v : "");
	CoordKv_Set(state, len, cap, "permit", p->permit ? "1" : "");
	Str_PrintfQuiet(v, sizeof(v), "%s/%s/%lld/%lld", p->jobLevel, p->jobDummy, (long long)p->jobStart,
		(long long)p->jobEnds);
	CoordKv_Set(state, len, cap, "job", p->jobLevel[0] ? v : "");
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
		cls = p->armoured && f->armour[0] ? f->armour : Faction_PlayClass(f, Fr_Rank(p));
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
	return p && t && fr.freeroam && p->faction >= 0 ? t->f[p->faction].pilot : "P_" FR_DEFAULT_CHAR;
}

byte Faction_Uniform(const char *cls) {
	if (!fr.freeroam || !cls) {
		return 0;
	}
	const FactionTable *t = Faction_Table();
	FrPlayer *p = Fr_Player(Players_Bound());
	if (!t || !p || p->faction < 0) {
		return 0;
	}
	const FactionDef *f = &t->f[p->faction];
	for (int k = 0; k < f->nplay; k++) {
		if (!crt_stricmp(f->play[k], cls)) {
			return 1;
		}
	}
	return !crt_stricmp(f->armour, cls);
}

void Faction_Spawned(int i, cEntity *e) {
	FrPlayer *p = Fr_Player(i);
	const FactionTable *t = Faction_Table();
	if (!p || !e || NetPl_Kind(e) != NETREP_CHAR) {
		return;
	}
	cWithLifeEntity *w = WLife(e);
	p->dead = 0;
	p->asleepUntil = 0.0;
	if (t && p->faction >= 0 && p->armoured) {
		w->life = w->prevLife = t->f[p->faction].armourLife;
	}
	p->fullLife = w->life;
	if (p->revived) {
		/* a revive brings it back with half its life */
		w->life = w->prevLife = (w->life + 1) / 2;
		p->revived = 0;
	}
}

static void Fr_CrimeOf(cEntity *victim, int j, int i, byte died);

void Faction_Died(int i) {
	FrPlayer *p = Fr_Player(i);
	const FactionTable *t = Faction_Table();
	cEntity *e = Players_Get(i);
	if (!p) {
		return;
	}
	/* the kill: the player whose weapon hit it last */
	cEntity *by = e && Faction_Freeroam() && WLife(e)->attack.hitBy.target ?
		World_FindEntity(Nto1Node_TargetName(&WLife(e)->attack.hitBy)) : NULL;
	int k = by && by != e ? Players_Of(by) : -1;
	if (k >= 0 && k != i) {
		Fr_CrimeOf(e, i, k, 1);
	}
	p->armoured = 0;
	p->siren = 0;
	p->ship = NULL;
	FactionWanted_Init(&p->wanted);
	p->level = 0;
	p->dead = e != NULL;
	if (e) {
		p->deadPos = e->pos;
		p->deadYaw = NetPl_Kind(e) == NETREP_CHAR ? WChar(e)->view.y : Obj(e)->ang.y;
	}
	p->home = fr.freeroam && t && p->faction >= 0 && !crt_stricmp(t->f[p->faction].office, Instance_Level());
	FrD_Gone(i);
}

void Faction_Left(int i) {
	FrPlayer *p = Fr_Player(i);
	if (p) {
		p->siren = 0;
		p->ship = NULL;
		p->sentLen = 0;
		p->dead = 0;
		FrD_Gone(i);
	}
}

byte Fr_Dummy(const char *name, vec3_t *pos, float *yaw) {
	char found[64];
	vec3_t ang;
	if (!name || !name[0]) {
		return 0;
	}
	DummySearch_Start(name, 1);
	if (!DummySearch_NextEuler(found, sizeof(found), pos, &ang)) {
		return 0;
	}
	if (yaw) {
		*yaw = ang.y;
	}
	return 1;
}

byte Faction_SpawnPlace(int i, void *at, float *yaw) {
	FrPlayer *p = Fr_Player(i);
	const FactionTable *t = Faction_Table();
	if (!p || !p->home || !t || p->faction < 0) {
		return 0;
	}
	p->home = 0;
	if (!Fr_Dummy(t->f[p->faction].jail, at, yaw)) {
		return 0;
	}
	*yaw += M_PI_F;
	Con_Printf("factions: player %d starts at the %s's %s\n", i, t->f[p->faction].name, t->f[p->faction].jail);
	return 1;
}

uint64_t Faction_DownedTicks(int i) {
	const FactionTable *t = Faction_Table();
	FrPlayer *p = Fr_Player(i);
	if (!Faction_Freeroam() || !p || !p->dead) {
		return 0;
	}
	/* while another player that can revive is in the level */
	for (int j = 0; j < PLAYERS_MAX; j++) {
		if (j != i && Players_Get(j) && Faction_Cooldown(t, fr.pl[j].faction, FA_REVIVE) >= 0.0f) {
			return (uint64_t)(t->downed * NETREP_TICK_HZ);
		}
	}
	return 0;
}

/* ---- the messages to the client ---- */

static void Fr_SendText(int i, int kind, int text, int num, const char *name, const char *name2) {
	uint8_t buf[192];
	NetRepBuf w;
	int conn = Rep_PlayerConn(i);
	if (conn < 0 || !Instance_Host()) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, FACTION_MSG_STATUS);
	NetRep_PutU8(&w, (uint32_t)kind);
	NetRep_PutU8(&w, (uint32_t)text);
	NetRep_PutSVar(&w, num);
	NetRep_PutStr(&w, name ? name : "");
	NetRep_PutStr(&w, name2 ? name2 : "");
	if (!w.bad) {
		Net2_Send(Instance_Host(), conn, REP_CH_RELIABLE, buf, w.size);
	}
}

void Fr_Notice(int i, int text, int num, const char *name, const char *name2) {
	char line[FR_TEXT];
	Fr_SendText(i, FACTION_ST_NOTICE, text, num, name, name2);
	Fr_Text(line, sizeof(line), text, num, name, name2);
	Con_Printf("factions: player %d: %s\n", i, line);
}

void Fr_News(int text, int num, const char *name, const char *name2) {
	char line[FR_TEXT];
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (Players_Get(i) || fr.pl[i].dead) {
			Fr_SendText(i, FACTION_ST_NEWS, text, num, name, name2);
		}
	}
	Fr_Text(line, sizeof(line), text, num, name, name2);
	Con_Printf("factions: news: %s\n", line);
}

void Fr_Fx(int fx, cEntity *a, cEntity *b) {
	uint8_t buf[8];
	NetRepBuf w;
	if (!Instance_Host()) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, FACTION_MSG_STATUS);
	NetRep_PutU8(&w, FACTION_ST_FX);
	NetRep_PutU8(&w, (uint32_t)fx);
	NetRep_PutU16(&w, (uint32_t)(Rep_SlotOf(a) + 1));
	NetRep_PutU16(&w, (uint32_t)(Rep_SlotOf(b) + 1));
	for (int i = 0; i < PLAYERS_MAX && !w.bad; i++) {
		int conn = Players_Get(i) ? Rep_PlayerConn(i) : -1;
		if (conn >= 0) {
			Net2_Send(Instance_Host(), conn, REP_CH_RELIABLE, buf, w.size);
		}
	}
}

/* faction, rank, wanted level, flags, ready keys, deeds, bounty, debt, job: sent when they change */
static void Fr_SendState(Net2Host *host, int i) {
	uint8_t buf[96];
	NetRepBuf w;
	FrPlayer *p = &fr.pl[i];
	const FactionTable *t = Faction_Table();
	int conn = Rep_PlayerConn(i);
	if (conn < 0 || !t) {
		return;
	}
	double now = Fr_Now();
	uint8_t ready = 0;
	for (int k = 0; k < FACTION_KEYS; k++) {
		int a = Faction_KeyAbility(t, p->faction, k);
		ready |= (uint8_t)((a >= 0 && now >= p->ready[a]) << k);
	}
	int64_t left = p->jobLevel[0] ? p->jobEnds - Fr_Unix() : -1;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, FACTION_MSG_STATUS);
	NetRep_PutU8(&w, FACTION_ST_STATE);
	NetRep_PutU8(&w, (uint32_t)(p->faction >= 0 ? p->faction : 255));
	NetRep_PutU8(&w, (uint32_t)Fr_Rank(p));
	NetRep_PutU8(&w, (uint32_t)p->level);
	NetRep_PutU8(&w, (uint32_t)((p->siren ? 1 : 0) | (p->armoured ? 2 : 0) | (fr.freeroam ? 4 : 0) |
		(p->permit ? 8 : 0)));
	NetRep_PutU8(&w, ready);
	NetRep_PutSVar(&w, p->deeds);
	NetRep_PutSVar(&w, p->bounty);
	NetRep_PutSVar(&w, p->debt);
	NetRep_PutSVar(&w, (int)(left < -1 ? 0 : left > 100000 ? 100000 : left));
	NetRep_PutStr(&w, p->jobLevel);
	if (w.bad || (p->sentLen == w.size && !memcmp(buf, p->sent, (size_t)w.size))) {
		return;
	}
	Net2_Send(host, conn, REP_CH_RELIABLE, buf, w.size);
	memcpy(p->sent, buf, (size_t)w.size);
	p->sentLen = w.size;
}

/* ---- shared steps of the abilities ---- */

float Fr_DistSq(const cEntity *a, const vec3_t *b) {
	float dx = a->pos.x - b->x, dy = a->pos.y - b->y, dz = a->pos.z - b->z;
	return dx * dx + dy * dy + dz * dz;
}

int Fr_Money(int i) {
	return Players_Money(i);
}

void Fr_AddMoney(int i, int delta) {
	int prev = Players_Bound();
	Players_Bind(i);
	g_pWorld->money += delta;
	Players_Bind(prev);
}

/* the direction e looks to (a character's view, a ship's angles) */
static float Fr_Yaw(cEntity *e) {
	return NetPl_Kind(e) == NETREP_CHAR ? WChar(e)->view.y : Obj(e)->ang.y;
}

int Fr_Nearest(int i, cEntity *e, float reach, float cone, byte (*test)(int i, int j)) {
	int best = -1;
	float bestD = reach * reach;
	float yaw = Fr_Yaw(e);
	for (int j = 0; j < PLAYERS_MAX; j++) {
		cEntity *o = j != i ? Players_Get(j) : NULL;
		if (!o || (test && !test(i, j))) {
			continue;
		}
		float d = Fr_DistSq(o, &e->pos);
		if (d >= bestD) {
			continue;
		}
		if (cone > 0.0f) {
			/* yaw 0 looks to -z */
			float a = atan2f(-(o->pos.x - e->pos.x), -(o->pos.z - e->pos.z));
			if (fabsf(NormAng(a - yaw)) > cone) {
				continue;
			}
		}
		bestD = d;
		best = j;
	}
	return best;
}

static void Fr_Side(int i, FactionSide *s) {
	FrPlayer *p = &fr.pl[i];
	s->faction = p->faction;
	s->wanted = p->level;
	s->bounty = p->bounty;
	s->sanctuary = FrD_InSanctuary(Players_Get(i));
}

static int Fr_Rule(int i, int j) {
	FactionSide a, b;
	Fr_Side(i, &a);
	Fr_Side(j, &b);
	return Faction_PvPRule(Faction_Table(), &a, &b, fr.policePvp, fr.factionPvp);
}

byte Fr_MayAffect(int i, int j, byte tell) {
	int r = Fr_Rule(i, j);
	if (r > 0 || (r < 0 && Instance_PvP())) {
		return 1;
	}
	if (tell) {
		Fr_Notice(i, FT_PROTECTED, 0, fr.pl[j].name, NULL);
	}
	return 0;
}

int Faction_PvP(cEntity *attacker, cEntity *victim) {
	int i = Players_Of(attacker), j = Players_Of(victim);
	if (!Faction_Freeroam() || i < 0 || j < 0) {
		return -1;
	}
	return Fr_Rule(i, j);
}

void Fr_Deed(int i) {
	char rank[32];
	FrPlayer *p = &fr.pl[i];
	const FactionTable *t = Faction_Table();
	int before = Fr_Rank(p);
	p->deeds++;
	if (Fr_Rank(p) > before) {
		Fr_Notice(i, FT_RANK_UP, Fr_Rank(p), Fr_RankName(p->faction, Fr_Rank(p), rank, sizeof(rank)), NULL);
		/* the class of the new rank when it differs (on foot) */
		const char *now = Faction_PlayClass(&t->f[p->faction], Fr_Rank(p));
		if (strcmp(now, Faction_PlayClass(&t->f[p->faction], before))) {
			Str_Copy(p->charCls, now, sizeof(p->charCls));
			if (!p->armoured) {
				Fr_Rebody(i, NULL, 0.0f);
			}
		}
	}
	Travel_Report(i);
}

void Fr_AddCrime(int i, int crime) {
	if (fr.ncrimes < FR_CRIMES) {
		fr.crimes[fr.ncrimes].player = (int8_t)i;
		fr.crimes[fr.ncrimes].crime = (int8_t)crime;
		fr.ncrimes++;
	} else {
		fr.crimesLost++;
	}
}

byte Fr_Outdoors(void) {
	return cWorld_GetList(g_pWorld, "OutCop") != NULL;
}

/* ---- the desks ---- */

/* the faction whose office is this level, FACTION_NONE */
static int Fr_OfficeHere(void) {
	const FactionTable *t = Faction_Table();
	for (int k = 0; t && k < t->n; k++) {
		if (t->f[k].cooldown[FA_JOIN] >= 0.0f && !crt_stricmp(t->f[k].office, Instance_Level())) {
			return k;
		}
	}
	return FACTION_NONE;
}

/* the position of the office's desk (an entity, else a dummy of the level): 1 found */
static byte Fr_Desk(const FactionDef *f, vec3_t *pos) {
	cEntity *e = f->desk[0] ? World_FindEntity(f->desk) : NULL;
	if (e) {
		*pos = e->pos;
		return 1;
	}
	return Fr_Dummy(f->desk, pos, NULL);
}

/* the use point at the desk of the level's office (both sides: the arrow of the client shows it, the server's use
 * of it joins), on the radar, made when it is not there */
static void Fr_MakeDesk(int f) {
	char name[FR_NAME], opt[FR_TEXT], text[FR_TEXT];
	vec3_t pos;
	const FactionTable *t = Faction_Table();
	if (f < 0) {
		return;
	}
	Str_PrintfQuiet(name, sizeof(name), FR_DESK_PREFIX "%s", t->f[f].name);
	if (World_FindEntity(name) || !Fr_Desk(&t->f[f], &pos)) {
		return;
	}
	cEntity *e = cWorld_CreateEntity(g_pWorld, name, pos.x, pos.y + 100.0f, pos.z, TYEN_WITH_LIFE);
	if (!e) {
		return;
	}
	int one = 1, zero = 0, radar = 2, rad = 120;
	char *s;
	Fr_Text(text, sizeof(text), FT_DESK, 0, t->f[f].name, NULL);
	Str_PrintfQuiet(opt, sizeof(opt), "Join:%s", text);
	s = opt;
	Entity_VarI(e, "VRadius", &rad, 1);
	Entity_VarI(e, "ArrowCanUse", &one, 1);
	Entity_VarI(e, "ArrowCanView", &one, 1);
	Entity_VarI(e, "AlwaysUse", &one, 1);
	Entity_VarI(e, "ActiveCMask", &zero, 1);
	Entity_VarI(e, "RadarShow", &radar, 1);
	Entity_VarS(e, "ArrOpt0", &s, 1);
	Con_Printf("factions: desk %s at %.0f %.0f %.0f\n", name, (double)pos.x, (double)pos.y, (double)pos.z);
}

byte Fr_AtDesk(int f, cEntity *e) {
	const FactionTable *t = Faction_Table();
	vec3_t desk;
	return f >= 0 && t && !crt_stricmp(t->f[f].office, Instance_Level()) &&
		(!Fr_Desk(&t->f[f], &desk) || Fr_DistSq(e, &desk) <= t->deskReach * t->deskReach);
}

static byte Fr_IsDesk(cEntity *e) {
	return e && !strncmp(e->name, FR_DESK_PREFIX, sizeof(FR_DESK_PREFIX) - 1);
}

static void Fr_Request(int i, int a);

void Faction_Input(int i, cEntity *e, NetRepInput *in) {
	FrPlayer *p = Fr_Player(i);
	if (!Faction_Freeroam() || !p || !e) {
		return;
	}
	if (p->asleep) {
		uint8_t weapon = in->weapon;
		uint32_t tick = in->tick;
		uint16_t slot = in->slot;
		memset(in, 0, sizeof(*in));
		in->weapon = weapon;
		in->tick = tick;
		in->slot = slot;
		return;
	}
	/* the use button on a desk: join or leave (not the game's use, not a ship) */
	byte desk = NetPl_Kind(e) == NETREP_CHAR && Fr_IsDesk(WChar_Victim(e));
	byte use = (in->buttons & (NETREP_IN_SWITCH | NETREP_IN_ACT3)) != 0;
	if (desk && use) {
		if (!p->useHeld) {
			Fr_Request(i, FA_JOIN);
		}
		in->buttons &= (uint16_t)~(NETREP_IN_SWITCH | NETREP_IN_ACT3);
	}
	p->useHeld = desk && use;
}

/* ---- the abilities of the police ---- */

void Fr_Rebody(int i, const vec3_t *pos, float yaw) {
	FrPlayer *p = &fr.pl[i];
	p->siren = 0;
	p->ship = NULL;
	Rep_Rebody(i, pos, yaw);
}

static int Fr_Join(int i, cEntity *e) {
	const FactionTable *t = Faction_Table();
	FrPlayer *p = &fr.pl[i];

	if (NetPl_Kind(e) != NETREP_CHAR) {
		Fr_Notice(i, FT_ON_FOOT, 0, NULL, NULL);
		return 0;
	}
	int here = Fr_OfficeHere();
	if (here < 0 || !Fr_AtDesk(here, e)) {
		Fr_Notice(i, FT_NOT_OFFICE, 0, NULL, NULL);
		return 0;
	}
	if (p->faction >= 0 && p->faction != here) {
		Fr_Notice(i, FT_LEAVE_FIRST, 0, t->f[p->faction].name, NULL);
		return 0;
	}
	if (p->faction == here) {
		p->faction = FACTION_NONE;
		p->armoured = 0;
		Str_Copy(p->charCls, FR_DEFAULT_CHAR, sizeof(p->charCls));
		Fr_Rebody(i, NULL, 0.0f);
		Fr_Notice(i, FT_LEFT, 0, t->f[here].name, NULL);
		Travel_Report(i);
		return 1;
	}
	if (p->level > 0) {
		Fr_Notice(i, FT_WANTED_NO_JOIN, 0, NULL, NULL);
		return 0;
	}
	p->faction = here;
	p->armoured = 0;
	Str_Copy(p->charCls, Faction_PlayClass(&t->f[here], Fr_Rank(p)), sizeof(p->charCls));
	Fr_Rebody(i, NULL, 0.0f);
	Fr_Notice(i, FT_JOINED, 0, t->f[here].name, NULL);
	Fr_News(FN_JOINED, 0, p->name, t->f[here].name);
	Travel_Report(i);
	return 1;
}

/* the model of a ship ("Model", else its type name) */
static const char *Fr_Model(cEntity *v) {
	char *s = NULL;
	if (Entity_VarS(v, "Model", &s, 0) && s && *s) {
		return s;
	}
	return Veh(v)->typeName;
}

static int Fr_Siren(int i, cEntity *e) {
	FrPlayer *p = &fr.pl[i];
	if (NetPl_Kind(e) != NETREP_SHIP || Faction_OfShip(Faction_Table(), Fr_Model(e)) != p->faction) {
		Fr_Notice(i, FT_NEED_SHIP, 0, NULL, NULL);
		return 0;
	}
	p->siren = !p->siren;
	p->ship = e;
	Str_Copy(p->shipName, e->name, sizeof(p->shipName));
	Fr_Notice(i, p->siren ? FT_SIREN_ON : FT_SIREN_OFF, 0, NULL, NULL);
	return 1;
}

static int Fr_Armour(int i, cEntity *e) {
	FrPlayer *p = &fr.pl[i];
	if (NetPl_Kind(e) != NETREP_CHAR) {
		Fr_Notice(i, FT_ON_FOOT, 0, NULL, NULL);
		return 0;
	}
	p->armoured = !p->armoured;
	Fr_Rebody(i, NULL, 0.0f);
	Fr_Notice(i, p->armoured ? FT_ARMOUR_ON : FT_ARMOUR_OFF, 0, NULL, NULL);
	return 1;
}

static byte Fr_IsWanted(int i, int j) {
	(void)i;
	return fr.pl[j].level > 0;
}

/* the arrested player to the jail of the faction's office level: the travel there (a coordinator's world; the notice
 * of the fine goes with it, the client leaves this instance at once), a place at the jail (this is the office level),
 * else its spawn again */
static void Fr_Jail(int j, const FactionDef *f, int fine) {
	vec3_t pos;
	float yaw;
	cEntity *e = Players_Get(j);
	if (!e) {
		return;
	}
	if (!crt_stricmp(f->office, Instance_Level())) {
		if (Fr_Dummy(f->jail, &pos, &yaw)) {
			Fr_Rebody(j, &pos, yaw + M_PI_F);
		} else {
			Rep_Respawn(j);
		}
		Fr_Notice(j, FT_ARRESTED, fine, NULL, NULL);
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
	Fr_Notice(j, FT_ARRESTED, fine, NULL, NULL);
	Con_Printf("factions: player %d has no way to %s (no coordinator): it starts again here\n", j, f->office);
	Rep_Respawn(j);
}

static int Fr_Arrest(int i, cEntity *e) {
	const FactionTable *t = Faction_Table();
	FrPlayer *p = &fr.pl[i];

	if (p->level > 0) {
		Fr_Notice(i, FT_YOU_ARE_WANTED, 0, NULL, NULL);
		return 0;
	}
	int j = Fr_Nearest(i, e, NetPl_Kind(e) == NETREP_SHIP ? t->reachShip : t->reachFoot, 0.0f, Fr_IsWanted);
	if (j < 0) {
		Fr_Notice(i, FT_NO_SUSPECT, 0, NULL, NULL);
		return 0;
	}
	FrPlayer *q = &fr.pl[j];
	int level = q->level;
	int fine = t->fine * level;
	int has = Fr_Money(j);
	fine = fine > has ? (has > 0 ? has : 0) : fine;
	Fr_AddMoney(j, -fine);
	int prev = Players_Bound();
	Players_Bind(j);
	Alarm_Set(&g_pWorld->alarm, 0.0f);
	Players_Bind(prev);
	Fr_AddMoney(i, t->reward * level);
	FactionWanted_Init(&q->wanted);
	q->level = 0;
	Con_Printf("factions: player %d arrested player %d (wanted %d): fine %d, reward %d, %d deeds\n", i, j, level,
		fine, t->reward * level, p->deeds + 1);
	Fr_Notice(i, FT_ARREST_MADE, t->reward * level, NULL, NULL);
	Fr_News(FN_ARREST, 0, p->name, q->name);
	Fr_Deed(i);
	Fr_Jail(j, &t->f[p->faction], fine);
	return 1;
}

static int Fr_Backup(int i, cEntity *e) {
	const FactionTable *t = Faction_Table();
	if (Fr_Outdoors() && t->backupShip[0]) {
		/* outdoors: police ships of the wave type at a free spawn place near the officer (VehiclesNPC.MakeAI as
		 * OutPolice.MakePoli does, but not far from it) into the police cars; each goes for the nearest wanted
		 * player */
		int prev = Players_Bound();
		Players_Bind(i);
		int made = 0, j = Fr_Nearest(i, e, FR_BACKUP_OUTDOOR, 0.0f, Fr_IsWanted);
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
		int j = Fr_Nearest(i, e, FR_BACKUP_INDOOR, 0.0f, Fr_IsWanted);
		if (j >= 0) {
			AICtrl_GearsChase(&g_pWorld->logic.aiCtrl, Players_Get(j));
		} else {
			AICtrl_SendSentinelToWatch(&g_pWorld->logic.aiCtrl, &e->pos);
		}
		Con_Printf("factions: player %d called the gears%s%d\n", i, j >= 0 ? " on player " : ", no suspect ", j);
	}
	Fr_Notice(i, FT_BACKUP, 0, NULL, NULL);
	return 1;
}

static int Fr_Ship(int i, cEntity *e) {
	FrPlayer *p = &fr.pl[i];
	const FactionTable *t = Faction_Table();
	if (NetPl_Kind(e) != NETREP_CHAR) {
		Fr_Notice(i, FT_ON_FOOT, 0, NULL, NULL);
		return 0;
	}
	/* the ships of the higher ranks need a permit of the Bureaucracy (not its own members) */
	int bur = Faction_Find(t, "Bureaucracy");
	if (Faction_NeedsPermit(t, Fr_Rank(p)) && !p->permit && p->faction != bur) {
		Fr_Notice(i, FT_NEED_PERMIT, 0, NULL, NULL);
		return 0;
	}
	p->callShip = 1;
	Rep_TestSwitch(i, 1);
	int ok = !p->callShip;
	p->callShip = 0;
	Fr_Notice(i, ok ? FT_SHIP : FT_NO_SHIP, 0, NULL, NULL);
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
	Str_Copy(model, Faction_RankShip(f, r), sizeof(model));
	cEntity *ship = cWorld_CreateEntity(g_pWorld, "ParkedFaction", pos->x, pos->y, pos->z, TYEN_CAR);
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
	Con_Printf("factions: player %d called faction ship %s (%s, rank %d)\n", i, ship->name, model, r);
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
		Fr_Notice(i, FT_NOT_FREEROAM, 0, NULL, NULL);
		return;
	}
	double now = Fr_Now();
	float cd = Faction_Cooldown(t, p->faction, a);
	if (cd < 0.0f) {
		Fr_Notice(i, FT_MEMBERS_ONLY, 0, NULL, NULL);
		return;
	}
	if (now < p->ready[a]) {
		Fr_Notice(i, FT_NOT_READY, 0, NULL, NULL);
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
	default:
		ok = FrD_Request(i, e, a) > 0;
		break;
	}
	Players_Bind(prev);
	if (ok) {
		/* the cooldown of the faction it is in now (join and leave change it) */
		cd = Faction_Cooldown(t, p->faction, a);
		p->ready[a] = now + (cd > 0.0f ? cd : 0.0f);
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

/* victim (player j's entity, j -1 for no player) hurt or killed by player i */
static void Fr_CrimeOf(cEntity *victim, int j, int i, byte died) {
	FrPlayer *p = Fr_Player(i);
	int crime;
	if (j >= 0) {
		FrPlayer *q = &fr.pl[j];
		q->lastBy = i;
		q->lastByJoin = Players_Joined(i);
		q->lastByTime = Fr_Now();
		/* police against a wanted player is no crime; a merc that kills its bounty claims it */
		if (Fr_IsPolice(p->faction) && p->level == 0 && q->level > 0) {
			return;
		}
		if (died && FrD_Kill(i, j)) {
			return;
		}
		if (!died && q->bounty > 0 && Faction_Cooldown(Faction_Table(), p->faction, FA_CONTRACTS) >= 0.0f) {
			return;
		}
		crime = Fr_IsPolice(q->faction) ? (died ? FCRIME_KILL_POLICE : FCRIME_HURT_POLICE) :
			died ? FCRIME_KILL_PLAYER : FCRIME_HURT_PLAYER;
	} else {
		/* an empty ship is nobody's: no crime */
		if (Entity_IsType(victim, TYEN_CAR) && !Entity_GetAI(victim)) {
			return;
		}
		if (!Entity_IsType(victim, TYEN_CAR) && !Entity_IsType(victim, TYEN_WALKCHAR)) {
			return;
		}
		/* the body of a dead player (Faction_Died counted the kill) */
		if (!strncmp(victim->name, REP_PLAYER_PREFIX, sizeof(REP_PLAYER_PREFIX) - 1)) {
			return;
		}
		byte police = Fr_IsPolice(Fr_FactionOf(victim));
		crime = police ? (died ? FCRIME_KILL_POLICE : FCRIME_HURT_POLICE) :
			died ? FCRIME_KILL_CIVILIAN : FCRIME_HURT_CIVILIAN;
	}
	Fr_AddCrime(i, crime);
}

void Faction_Crime(cEntity *victim, cEntity *attacker, byte died) {
	if (!Faction_Freeroam() || !attacker || attacker == victim) {
		return;
	}
	int i = Players_Of(attacker), j = Players_Of(victim);
	/* a player's death is Faction_Died's (the instance may drop its entity before the game's OnDeath runs) */
	if (i < 0 || (j >= 0 && died)) {
		return;
	}
	Fr_CrimeOf(victim, j, i, died);
}

/* the crimes of the frame into the wanted levels; a level that rises turns the player's alarm on */
static void Fr_Crimes(void) {
	const FactionTable *t = Faction_Table();
	double now = Fr_Now();
	for (int k = 0; k < fr.ncrimes; k++) {
		int i = fr.crimes[k].player;
		FrPlayer *p = &fr.pl[i];
		if (FactionWanted_Crime(t, &p->wanted, fr.crimes[k].crime, now)) {
			p->lastCrime = now;
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
			Fr_Notice(i, FT_WANTED, level, NULL, NULL);
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
	int i = fr.freeroam ? Players_Of(e) : -1;
	if (i < 0) {
		return 0;
	}
	return (Entity_IsType(e, TYEN_CAR) && Fr_SirenShip(&fr.pl[i], i) ? NETREP_FL_SIREN : 0) | FrD_Flags(i);
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
	if (tick % NETREP_TICK_HZ == 0) {
		Fr_MakeDesk(Fr_OfficeHere());
	}
	Fr_Crimes();
	Fr_Sirens(tick);
	FrD_Tick(tick);
	for (int i = 0; i < PLAYERS_MAX; i++) {
		FrPlayer *p = &fr.pl[i];
		if (!host || !Players_Get(i)) {
			continue;
		}
		if (p->jailFine >= 0) {
			Fr_Notice(i, FT_ARRESTED, p->jailFine, NULL, NULL);
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
	const FactionTable *t = Faction_Table();
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
		p->faction = t ? Faction_Find(t, arg) : FACTION_NONE;
		p->armoured = 0;
		Str_Copy(p->charCls, t && p->faction >= 0 ? Faction_PlayClass(&t->f[p->faction], Fr_Rank(p)) : FR_DEFAULT_CHAR,
			sizeof(p->charCls));
		Fr_Rebody(i, NULL, 0.0f);
		Con_Printf("factions: player %d set to %s\n", i, t && p->faction >= 0 ? t->f[p->faction].name : "-");
		Travel_Report(i);
		return 1;
	}
	if (!strcmp(what, "crime")) {
		for (int c = 0; c < FCRIME_COUNT; c++) {
			if (!strcmp(arg, Faction_CrimeName(c))) {
				Fr_AddCrime(i, c);
			}
		}
		return 1;
	}
	if (!strcmp(what, "arrests") || !strcmp(what, "deeds")) {
		p->deeds = crt_atoi(arg);
		Con_Printf("factions: player %d deeds %d, rank %d\n", i, p->deeds, Fr_Rank(p));
		return 1;
	}
	if (!strcmp(what, "hit")) {
		/* tests: player i hurts player j (faction <i> hit <j> [<damage>, 0 kills]) as a weapon does (its hitBy) */
		int j = -1, dmg = 0;
		sscanf(line, "faction %*d %*s %d %d", &j, &dmg);
		cEntity *a = Players_Get(i), *v = j >= 0 && j < PLAYERS_MAX ? Players_Get(j) : NULL;
		if (a && v) {
			cVehicleAttack_HitBy(&WLife(v)->attack, a->name);
			WLife(v)->life = dmg > 0 ? WLife(v)->life - dmg : 0;
		}
		Con_Printf("factions: player %d hits player %d (%d)\n", i, j, dmg);
		return 1;
	}
	if (!strcmp(what, "setjob")) {
		/* tests: a courier job to the route of the level */
		for (int r = 0; t && r < t->nroutes; r++) {
			if (!crt_stricmp(t->routes[r].level, arg)) {
				Str_Copy(p->jobLevel, t->routes[r].level, sizeof(p->jobLevel));
				Str_Copy(p->jobDummy, t->routes[r].dummy, sizeof(p->jobDummy));
				p->jobStart = Fr_Unix();
				p->jobEnds = p->jobStart + t->courierTime;
			}
		}
		Con_Printf("factions: player %d job %s/%s\n", i, p->jobLevel, p->jobDummy);
		return 1;
	}
	if (!strcmp(what, "setbounty") || !strcmp(what, "setdebt") || !strcmp(what, "setpermit")) {
		int v = crt_atoi(arg);
		if (what[3] == 'b') {
			p->bounty = 0;
			p->bountyEnds = 0;
			FactionBounty_Add(t, &p->bounty, &p->bountyEnds, v, Fr_Unix());
		} else if (what[3] == 'd') {
			p->debt = v;
		} else {
			p->permit = v != 0;
		}
		Con_Printf("factions: player %d bounty %d debt %d permit %d\n", i, p->bounty, p->debt, p->permit);
		return 1;
	}
	if (!strcmp(what, "status")) {
		Con_Printf("factions: player %d faction %s rank %d deeds %d wanted %d (%.1f points) armour %d siren %d class "
			"%s money %d bounty %d debt %d permit %d job %s/%s slowed %d asleep %d, %u requests %u done\n", i,
			t && p->faction >= 0 ? t->f[p->faction].name : "-", Fr_Rank(p), p->deeds, p->level,
			(double)p->wanted.points, p->armoured, p->siren, Faction_CharClass(i), Players_Get(i) ? Fr_Money(i) : -1,
			p->bounty, p->debt, p->permit, p->jobLevel, p->jobDummy, p->slowed, p->asleep, p->requests, p->done);
		return 1;
	}
	int a = !strcmp(what, "leave") ? FA_JOIN : Faction_AbilityOf(what);
	if (a >= 0) {
		Fr_Request(i, a);
		return 1;
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
	int faction, rank, wanted, deeds, bounty, debt, jobLeft;
	char jobLevel[FACTION_NAME];
	uint8_t flags, ready;
	uint8_t own;					/* the state flags of the own entity */
	float prev[FACTION_KEYS];		/* the keys of the last frame */
	FrShip ships[FR_SHIPS];
	char news[FR_TEXT];
	double newsUntil;				/* real s */
	double deskLook;				/* real s of the next look for the desk */
	byte slowFx, asleepSeen;
	double botStart;
	char bot[256];
	byte botTried;
	uint32_t botDone;				/* the steps of SCRAP_PLAYBOT_FACTION sent */
} frc = { .faction = FACTION_NONE };

static double Fr_RealNow(void) {
	return (double)Plat_RealNs() / 1e9;
}

void Faction_ClientReset(void) {
	memset(frc.ships, 0, sizeof(frc.ships));
	frc.have = 0;
	frc.faction = FACTION_NONE;
	frc.botStart = 0.0;
	frc.deskLook = 0.0;
	frc.own = 0;
	if (frc.slowFx) {
		Char_SlowEffect(0, 1.0);
		frc.slowFx = 0;
	}
}

byte Faction_OwnAsleep(void) {
	return (frc.own & NETREP_FL_ASLEEP) != 0;
}

/* an effect between the entities of two slots */
static void Fr_ClientFx(int fx, int sa, int sb) {
	cEntity *a = Watch_SlotEntity(sa - 1), *b = Watch_SlotEntity(sb - 1);
	if ((a && !Entity_IsType(a, TYEN_WITH_LIFE)) || (b && !Entity_IsType(b, TYEN_WITH_LIFE)) || g_HeadlessPlay) {
		return;
	}
	switch (fx) {
	case FFX_HEAL:
		if (b) {
			FX_BishopSellLife(a, b);
		}
		break;
	case FFX_RAY:
		if (a && b) {
			FX_BishopRay(a, b, &b->pos);
		}
		break;
	case FFX_REVIVE:
		if (b) {
			FX_BishopSellLife(NULL, b);
		}
		break;
	case FFX_STEAL:
		if (a && b) {
			FX_BankerSteal(a, b);
		}
		break;
	case FFX_SUCK:
		if (a && b) {
			FX_BankMasterSuction(a, b);
		}
		break;
	case FFX_MONEY:
		if (a && b) {
			FX_MoneyTransfer(a, b, 1.0f);
		}
		break;
	case FFX_PHOTO:
		if (a && b) {
			FX_BertoFlash(a, b);
		}
		break;
	default:
		break;
	}
}

void Faction_ClientMessage(const uint8_t *data, int size) {
	char name[64], name2[64], text[FR_TEXT];
	NetRepReader r;
	if (size < 2) {
		return;
	}
	NetRepReader_Init(&r, data + 2, size - 2);
	if (data[1] == FACTION_ST_STATE) {
		int f = (int)NetRep_GetU8(&r), rank = (int)NetRep_GetU8(&r), wanted = (int)NetRep_GetU8(&r);
		uint8_t flags = (uint8_t)NetRep_GetU8(&r), ready = (uint8_t)NetRep_GetU8(&r);
		int deeds = NetRep_GetSVar(&r), bounty = NetRep_GetSVar(&r), debt = NetRep_GetSVar(&r);
		int job = NetRep_GetSVar(&r);
		NetRep_GetStr(&r, frc.jobLevel, sizeof(frc.jobLevel));
		if (r.bad) {
			return;
		}
		frc.have = 1;
		frc.faction = f == 255 ? FACTION_NONE : f;
		frc.rank = rank;
		frc.wanted = wanted;
		frc.flags = flags;
		frc.ready = ready;
		frc.deeds = deeds;
		frc.bounty = bounty;
		frc.debt = debt;
		frc.jobLeft = job;
		Con_Printf("factions: faction %d rank %d wanted %d deeds %d flags %d ready %02x bounty %d debt %d job %s %d\n",
			frc.faction, frc.rank, frc.wanted, frc.deeds, frc.flags, frc.ready, frc.bounty, frc.debt, frc.jobLevel,
			frc.jobLeft);
		return;
	}
	if (data[1] == FACTION_ST_FX) {
		int fx = (int)NetRep_GetU8(&r), sa = (int)NetRep_GetU16(&r), sb = (int)NetRep_GetU16(&r);
		if (!r.bad) {
			Fr_ClientFx(fx, sa, sb);
		}
		return;
	}
	if (data[1] != FACTION_ST_NOTICE && data[1] != FACTION_ST_NEWS) {
		return;
	}
	int id = (int)NetRep_GetU8(&r);
	int num = NetRep_GetSVar(&r);
	NetRep_GetStr(&r, name, sizeof(name));
	NetRep_GetStr(&r, name2, sizeof(name2));
	if (r.bad || id < 0 || id >= FT_COUNT) {
		return;
	}
	Fr_Text(text, sizeof(text), id, num, name, name2);
	if (data[1] == FACTION_ST_NEWS) {
		Con_Printf("factions: news: %s\n", text);
		Str_Copy(frc.news, text, sizeof(frc.news));
		frc.newsUntil = Fr_RealNow() + FR_NEWS_MS / 1000.0;
		return;
	}
	Con_Printf("factions: %s\n", text);
	if (!g_HeadlessPlay) {
		Text_Message(FR_NOTICE_MS, "%s", text);
	}
}

static void Fr_ClientSend(int a) {
	uint8_t buf[2] = { FACTION_MSG_REQ, (uint8_t)a };
	if (a >= 0 && Watch_SendReliable(buf, sizeof(buf))) {
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
	double now = Fr_RealNow();
	if (frc.botStart == 0.0) {
		frc.botStart = now;
	}
	char list[256];
	Str_Copy(list, frc.bot, sizeof(list));
	uint32_t k = 0;
	for (char *tok = strtok(list, ","); tok && k < 32; tok = strtok(NULL, ","), k++) {
		char *c = strchr(tok, ':');
		if (!c || (frc.botDone >> k & 1) || now - frc.botStart < crt_atof(tok)) {
			continue;
		}
		frc.botDone |= 1u << k;
		Fr_ClientSend(Faction_AbilityOf(c + 1));
	}
}

/* the own entity's time speed in a bullet time (the prediction steps it so, include/scrap/play.h) and the effect */
static void Fr_ClientSlow(const FactionTable *t, cEntity *own) {
	byte slow = (frc.own & NETREP_FL_SLOW) != 0;
	float k = slow ? t->slowSpeed : 1.0f;
	if (cTimer_GetSpeed(&Obj(own)->timer) != k) {
		Timer_SetSpeed(&Obj(own)->timer, k);
	}
	if (slow != frc.slowFx) {
		Con_Printf("factions: own bullet time %s\n", slow ? "on" : "off");
		if (!g_HeadlessPlay) {
			Char_SlowEffect(slow, 1.0);
		}
	}
	byte asleep = Faction_OwnAsleep();
	if (asleep != frc.asleepSeen) {
		Con_Printf("factions: own sleep %s\n", asleep ? "on" : "off");
		frc.asleepSeen = asleep;
	}
	frc.slowFx = slow;
}

/* the lines of the client: the faction and rank, the keys, the wanted level, the bounty, debt, permit and job */
static void Fr_ClientLines(const FactionTable *t) {
	char line[FR_TEXT], rank[32], keys[FR_TEXT];
	int w, h;
	Graphic2D_GetScreen(&w, &h);
	int x = (int)((float)w * 0.02f), y = (int)((float)h * 0.30f), dy = (int)((float)h * 0.035f);
	if (frc.faction >= 0 && frc.faction < t->n) {
		Fr_Text(line, sizeof(line), FT_HUD_MEMBER, 0, t->f[frc.faction].name,
			Fr_RankName(frc.faction, frc.rank, rank, sizeof(rank)));
		Text_Printf(x, y, "%s", line);
		y += dy;
	}
	/* the keys: a cooling one in brackets */
	int n = 0;
	keys[0] = 0;
	for (int k = 0; k < FACTION_KEYS; k++) {
		int a = Faction_KeyAbility(t, frc.faction, k);
		if (a >= 0) {
			byte ready = (frc.ready >> k & 1) != 0;
			Str_PrintfQuiet(keys + n, (int)sizeof(keys) - n, "%s%s%s%s", n ? " " : "", ready ? "" : "(",
				g_AbilityLabels[a], ready ? "" : ")");
			n += (int)strlen(keys + n);
		}
	}
	Fr_Text(line, sizeof(line), FT_HUD_KEYS, 0, keys, NULL);
	Text_Printf(x, y, "%s", line);
	y += dy;
	if (frc.wanted > 0) {
		Fr_Text(line, sizeof(line), FT_HUD_WANTED, frc.wanted, NULL, NULL);
		Text_Printf(x, y, "%s", line);
		y += dy;
	}
	if (frc.bounty > 0) {
		Fr_Text(line, sizeof(line), FT_HUD_BOUNTY, frc.bounty, NULL, NULL);
		Text_Printf(x, y, "%s", line);
		y += dy;
	}
	if (frc.debt > 0) {
		Fr_Text(line, sizeof(line), FT_HUD_DEBT, frc.debt, NULL, NULL);
		Text_Printf(x, y, "%s", line);
		y += dy;
	}
	if (frc.flags & 8) {
		Text_Printf(x, y, "%s", g_FactionTexts[FT_HUD_PERMIT]);
		y += dy;
	}
	if (frc.jobLevel[0]) {
		Fr_Text(line, sizeof(line), FT_HUD_JOB, frc.jobLeft, frc.jobLevel, NULL);
		Text_Printf(x, y, "%s", line);
	}
	if (frc.news[0] && Fr_RealNow() < frc.newsUntil) {
		Fr_Text(line, sizeof(line), FT_HUD_NEWS, 0, frc.news, NULL);
		/* above the radar's right edge */
		Text_Printf((int)((float)w * 0.24f), (int)((float)h * 0.80f), "%s", line);
	}
}

void Faction_ClientFrame(void) {
	const FactionTable *t = Faction_Table();
	cEntity *own = Play_Entity();
	if (!g_Play || !own || !t) {
		return;
	}
	Fr_ClientBot();
	Fr_ClientSlow(t, own);
	/* the keys: QMsg1..QMsg6 of the Racer set, on foot as in a ship (no menu open), the abilities of the faction */
	uint32_t set = Input_GetActionSet();
	for (int k = 0; k < FACTION_KEYS; k++) {
		float v = Input_BoundValue(ACTSET_RACER, RACER_QMSG1 + k, 0);
		if (v > 0.5f && frc.prev[k] <= 0.5f && (set == ACTSET_RACER || set == ACTSET_WALK)) {
			Fr_ClientSend(Faction_KeyAbility(t, frc.have ? frc.faction : FACTION_NONE, k));
		}
		frc.prev[k] = v;
	}
	if (!frc.have || !(frc.flags & 4)) {
		return;
	}
	/* the use point of the office's desk, as the instance has it */
	double now = Fr_RealNow();
	if (now >= frc.deskLook) {
		frc.deskLook = now + 1.0;
		Fr_MakeDesk(Fr_OfficeHere());
	}
	if (!g_HeadlessPlay) {
		Fr_ClientLines(t);
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
	if (e && e == Play_Entity()) {
		frc.own = flags;
	}
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
