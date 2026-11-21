/*
 * The coop story of a world on the instance: the party's save variables (the story's sync with the coordinator), the
 * party (its mission's member, money, death, the members' places, the boarding callbacks, a member's join), the
 * mission level changes as travel, see include/scrap/coop.h; the rules of data/coop/rules.def (Party.c). The old
 * path's forwarding and menus are in CoopOld.c, its client in CoopClient.c, the automatic test player in CoopTest.c.
 * Not in the original.
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

#define CP_MSG 16384				/* bytes of one story message to the coordinator */
#define COOP_STANDIN "CoopStandIn"
#define CP_GATHER ((float)Party_RuleNum("gather", 160.0))	/* units between the members placed next to the member of a scene */
#define CP_FAIL_SECS Party_RuleNum("failsecs", 5.0)	/* a failed mission's restart after this */
#define CP_ENTRY "Coop.Entry."
#define CP_PURSE "Coop.Money"		/* the party's money, a counter of the story (Cp_Purse) */

CoopState g_Coop = { .scene = -1, .loading = -1, .conv = -1, .missionPlayer = -1, .pilot = -1 };

static void Cp_SetMissionPlayer(int i);
static void Cp_Reset(const char *map);
static void Cp_TellGone(int i);

byte Cp_InList(const char *const *list, const char *s) {
	for (; *list; list++) {
		if (!crt_stricmp(*list, s)) {
			return 1;
		}
	}
	return 0;
}

byte Coop_ParseParam(const char *param, const char *ext) {
	if (!crt_stricmp("mode", param) && !crt_stricmp(ext, "coop")) {
		g_Coop.on = 1;
		return 1;
	}
	if (!crt_stricmp("story", param)) {
		Str_Copy(g_Coop.story, ext, sizeof(g_Coop.story));
		return 1;
	}
	return 0;
}

byte Coop_On(void) {
	return g_Instance && g_Coop.on;
}

byte Coop_Asked(void) {
	return g_Coop.on;
}

/* ---- the story ---- */

static byte Cp_Story(const char *name) {
	return name[0] && (int)strlen(name) < CP_NAME && CoordKv_KeyOk(name) && !Party_RuleHas("local", name);
}

static int Cp_DirtyAt(const char *name) {
	for (int i = 0; i < g_Coop.ndirty; i++) {
		if (!strcmp(g_Coop.dirty[i].name, name)) {
			return i;
		}
	}
	return -1;
}

/* a variable of the story the HUD shows to client i (-1: all here) */
static void Cp_HudVar(int i, const char *name) {
	uint8_t buf[2 * CP_NAME + 8];
	NetRepBuf w;
	const char *v = SaveVar_Get(name);
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_HUDVAR);
	NetRep_PutStr(&w, name);
	NetRep_PutStr(&w, v ? v : "0");
	if (w.bad) {
		return;
	}
	if (i < 0) {
		Cp_ToAll(buf, w.size);
	} else {
		Cp_ToClient(i, buf, w.size);
	}
}

byte Cp_ClientStoryVar(const char *name) {
	return Cp_Story(name) && !Party_RuleHas("clientnot", name);
}

void Coop_SaveVarChanged(const char *name, const char *old, byte inc) {
	if (g_Coop.on && Party_RuleHas("hud", name)) {
		Cp_HudVar(-1, name);
	}
	if (g_Coop.on && Cp_ClientStoryVar(name) && !g_Coop.coverflow) {
		int k = 0;
		while (k < g_Coop.ncdirty && strcmp(g_Coop.cdirty[k], name)) {
			k++;
		}
		if (k == CP_CDIRTY) {
			g_Coop.coverflow = 1;
		} else if (k == g_Coop.ncdirty) {
			Str_Copy(g_Coop.cdirty[g_Coop.ncdirty++], name, CP_NAME);
		}
	}
	if (!g_Coop.on || g_Coop.applying || !Cp_Story(name)) {
		return;
	}
	/* the member that sets the next mission is its member */
	if (g_Instance && !strcmp(name, "Mission.Library") && Players_Current() != PLAYERS_NONE) {
		Cp_SetMissionPlayer(Players_Current());
	}
	int i = Cp_DirtyAt(name);
	if (i < 0) {
		if (g_Coop.ndirty == COOP_DIRTY) {
			g_Coop.overflow = 1;
			return;
		}
		i = g_Coop.ndirty++;
		Str_Copy(g_Coop.dirty[i].name, name, CP_NAME);
		g_Coop.dirty[i].base = 0;
		g_Coop.dirty[i].hasBase = !old || CoordStory_Int(old, &g_Coop.dirty[i].base);
		g_Coop.dirty[i].counter = !strncmp(name, "Stats.", 6);
	}
	g_Coop.dirty[i].counter |= inc;
}

void Coop_LevelChar(const char *level) {
	const char *s = level ? strrchr(level, '/') : NULL;
	const char *lc = level && (g_Instance || g_Coop.world) ? MissionMaker_Char(s ? s + 1 : level) : NULL;
	if (lc && strcmp(lc, "Dtritus")) {
		SaveVar_Set("Char", lc);
		Con_Printf("coop: the level's start with its own character %s\n", lc);
	}
}

void Coop_LevelStart(const char *level) {
	static char text[COORD_STORY_MAX];
	static char key[CP_NAME], val[COORD_STORY_VALUE + 1];

	if (!Coop_On()) {
		return;
	}
	int n = g_Coop.story[0] ? Coord_ReadFile(g_Coop.story, text, sizeof(text)) : -1;
	if (n <= 0) {
		/* a new party: the new game of the single player (sent to the coordinator as the story's first changes) */
		SaveGame_CoopNewGame();
		Con_Printf("coop: a new story (the new game), %d changes for the coordinator\n", g_Coop.ndirty);
		return;
	}
	g_Coop.applying = 1;
	SaveVars_Delete("");
	int count = 0;
	for (int pos = 0; CoordKv_Next(text, n, &pos, key, sizeof(key), val, sizeof(val)); count++) {
		SaveVar_Set(key, val);
	}
	/* the level a chapter's or a mission's travel came to: its way point as the single player's level start has it */
	const char *map = SaveVar_Get(CP_ENTRY "Map");
	const char *s = map ? strrchr(map, '/') : NULL, *here = level ? strrchr(level, '/') : NULL;
	int nentry;
	const char *const *entry = Party_RuleList("entry", &nentry);
	if (s && here && !crt_stricmp(s, here)) {
		for (int k = 0; k < nentry; k++) {
			Str_PrintfQuiet(key, sizeof(key), CP_ENTRY "%s", entry[k]);
			const char *v = SaveVar_Get(key);
			SaveVar_Set(entry[k], v ? v : "");
		}
		Con_Printf("coop: the way point of the chapter: %s, spawn %s, from %s\n", map, SaveVar_Get("Spawn"),
			SaveVar_Get("ComeFrom"));
		/* taken: a later start of this level is the single player's load of a save there (Junkyard's arrival with
		 * Berto once) */
		g_Coop.applying = 0;
		for (int k = 0; k < nentry; k++) {
			Str_PrintfQuiet(key, sizeof(key), CP_ENTRY "%s", entry[k]);
			SaveVar_Delete(key);
		}
	}
	g_Coop.applying = 0;
	/* the character the members carry is theirs (rules.def: local) */
	if (!SaveVar_Get("Char")) {
		Coop_LevelChar(level);
	}
	Con_Printf("coop: the party's story, %d variables, mission %s in %s\n", count, SaveVar_Get("Mission.Library") ?
		SaveVar_Get("Mission.Library") : "-", SaveVar_Get("Mission.Map") ? SaveVar_Get("Mission.Map") : "-");
}

/* the message being written: its count patched in at the end */
typedef struct CpOut {
	uint8_t buf[CP_MSG + 16];
	NetRepBuf w;
	int count;
} CpOut;

static void Cp_OutStart(CpOut *o) {
	NetRepBuf_Init(&o->w, o->buf, CP_MSG);
	NetRep_PutU8(&o->w, g_Coop.levelSent ? COORD_MSG_STORY : COORD_MSG_STORYINIT);
	NetRep_PutU16(&o->w, 0);
	o->count = 0;
}

static void Cp_OutFlush(CpOut *o) {
	if (o->count) {
		o->buf[1] = (uint8_t)o->count;
		o->buf[2] = (uint8_t)(o->count >> 8);
		Travel_SendCoord(o->buf, o->w.size);
	}
	Cp_OutStart(o);
}

static void Cp_OutPut(CpOut *o, int kind, const char *name, const char *value) {
	int vn = value ? (int)strlen(value) : 0;
	if (vn > COORD_STORY_VALUE) {
		Con_Printf("coop: the value of %s is too long for the story (%d bytes)\n", name, vn);
		return;
	}
	if (o->w.size + 4 + (int)strlen(name) + vn > CP_MSG || o->count == 0xffff) {
		Cp_OutFlush(o);
	}
	CoordStory_Put(&o->w, kind, name, value);
	o->count++;
}

/* a dirty name: its value, a counter's change as the addition since it became dirty (none when it is back) */
static void Cp_OutDirty(CpOut *o, int i) {
	char delta[24];
	long v;
	const char *value = SaveVar_Get(g_Coop.dirty[i].name);
	if (value && g_Coop.dirty[i].counter && g_Coop.dirty[i].hasBase && CoordStory_Int(value, &v)) {
		if (v != g_Coop.dirty[i].base) {
			Str_PrintfQuiet(delta, sizeof(delta), "%ld", v - g_Coop.dirty[i].base);
			Cp_OutPut(o, COORD_OP_ADD, g_Coop.dirty[i].name, delta);
		}
		return;
	}
	Cp_OutPut(o, value ? COORD_OP_SET : COORD_OP_DEL, g_Coop.dirty[i].name, value);
}

static void Cp_PutAll(const char *name, const char *value, void *u) {
	if (Cp_Story(name)) {
		Cp_OutPut(u, COORD_OP_SET, name, value);
	}
}

/* the changes to the coordinator now (once the whole story came: before it the coordinator's would go over them) */
static void Cp_Send(void) {
	static CpOut o;
	if (!g_Coop.synced || g_Coop.hold || (!g_Coop.ndirty && !g_Coop.overflow)) {
		return;
	}
	Cp_OutStart(&o);
	if (g_Coop.overflow) {
		SaveVars_Each(Cp_PutAll, &o);
	} else {
		for (int i = 0; i < g_Coop.ndirty; i++) {
			Cp_OutDirty(&o, i);
		}
	}
	Cp_OutFlush(&o);
	g_Coop.ndirty = 0;
	g_Coop.overflow = 0;
	g_Coop.levelSent = 1;
}

/* the party's one purse (include/scrap/coop.h): what the members here earned or spent since the last tick goes into
 * the story's counter, every member gets the sum; a story without one takes the money of its first member */
static void Cp_Purse(void) {
	char text[16];
	const char *v = SaveVar_Get(CP_PURSE);
	int purse = v ? crt_atoi(v) : 0, add = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (Rep_PlayerConn(i) < 0) {
			continue;
		}
		int m = Players_Money(i);
		if (g_Coop.pl[i].purseJoined == Players_Joined(i)) {
			add += m - g_Coop.pl[i].purse;
			continue;
		}
		g_Coop.pl[i].purseJoined = Players_Joined(i);
		g_Coop.pl[i].purse = purse;
		if (!v) {
			Str_PrintfQuiet(text, sizeof(text), "%d", m);
			SaveVar_Set(CP_PURSE, text);
			v = SaveVar_Get(CP_PURSE);
			purse = m;
			g_Coop.pl[i].purse = m;
		}
		Con_Printf("coop: player %d has the party's money %d (its own was %d)\n", i, purse, m);
	}
	if (add) {
		purse = SaveVar_Inc(CP_PURSE, add);
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (Rep_PlayerConn(i) >= 0) {
			Players_SetMoney(i, purse);
			g_Coop.pl[i].purse = purse;
		}
	}
}

void Coop_Pass(void) {
	if (Coop_On() && !g_Coop.started) {
		/* the level's start (a scene of its Init: GamblinDen's MeetGambler, Fake0) waits for the member that comes,
		 * as the single player's plays it from its start */
		g_Coop.started = 1;
		g_Coop.waiting = 1;
		for (int i = 0; i < PLAYERS_MAX; i++) {
			g_Coop.waiting = g_Coop.waiting && Rep_PlayerConn(i) < 0;
		}
		if (g_Coop.waiting) {
			Timer_SetSpeed(&g_pWorld->timer, 0.0f);
			Con_Printf("coop: the level waits for its first member\n");
		}
	}
	if (Coop_On()) {
		/* nothing draws the talk scorer here: its time and the end of a remote communication (a talk of the level's
		 * start waits for the first member, as the level does) */
		if (!g_Coop.waiting && !g_TalkModels) {
			Scorer_TalkTick(1.0f / INSTANCE_TICK_HZ);
		}
		Scorer_Tick(0);
		if (g_Coop.conv >= 0 && g_Coop.convAsked && Rep_PlayerConn(g_Coop.conv) < 0) {
			/* the member left with the conversor's interface open: as its Escape */
			Cp_ConvPicked(g_Coop.conv, "");
		}
		Cp_Purse();
		if (g_Coop.failAt > 0.0 && Instance_Time() >= g_Coop.failAt) {
			g_Coop.failAt = 0.0;
			Cp_Reset(Instance_Level());
		}
		Cp_Send();
		/* the old path's forwarding (CoopOld.c); -mp2: the answers are the members' input (Speech.c), the HUD and the
		 * targets' marks go by pres.h and the replicas */
		if (!g_Mp2) {
			Cp_TellOptions();
			Cp_TellTargets();
			Cp_TellScorer();
		}
		if (g_Coop.ncdirty || g_Coop.coverflow) {
			Cp_TellStory(-1, g_Coop.coverflow);
			g_Coop.ncdirty = 0;
			g_Coop.coverflow = 0;
		}
	}
}

byte Coop_Revives(const nativeVal_t *name) {
	cEntity *e = Coop_On() && name->type == NV_STR ? World_FindEntity(Native_Text(name)) : NULL;
	return !Coop_On() || (e && Players_Of(e) >= 0);
}

void Coop_Boarded(int i, cEntity *ship) {
	static const char *const cb[2][2] = { { "SwitchCharMissionCallback", "SwitchCharMissionCallbackAux" },
		{ "SwitchVehicleMissionCallback", "SwitchVehicleMissionCallbackAux" } };
	byte called;
	if (!Coop_On() || !Mode_Import("Parking")) {
		Native_ErrClear();
		return;
	}
	/* single player has one pilot: the callbacks of boarding run for the first member in a ship, the ones of leaving for
	 * that member (DestroyMickus' scan: a second member's boarding started a second scan, bound to that member) */
	cEntity *p = g_Coop.pilot >= 0 && Rep_PlayerConn(g_Coop.pilot) >= 0 ? Players_Get(g_Coop.pilot) : NULL;
	byte first = strcmp(Party_Rule("pilot", "first"), "each") != 0;
	if (first && (ship ? g_Coop.pilot >= 0 && g_Coop.pilot != i && p && NetPl_Kind(p) == NETREP_SHIP : g_Coop.pilot != i)) {
		Con_Printf("coop: player %d %s a ship: the callbacks stay with player %d\n", i, ship ? "into" : "out of", g_Coop.pilot);
		return;
	}
	g_Coop.pilot = ship ? i : -1;
	/* a member without an own ship: the ship it boarded stands for it while the callbacks run (single player always has
	 * its own there: a super deal's start sets Parking.PlayerShip.Invulnerable); none again after them, so that ship's
	 * death is no own ship's (Vehicles.OnDeath, the deal's respawns) */
	cEntity *stand = ship && !Players_Ship(i) ? ship : NULL;
	if (stand) {
		Players_SetShip(i, stand);
	}
	int was = Players_Enter(i);
	for (int k = 0; k < 2; k++) {
		if (!Mode_CallIf("Parking", cb[ship != NULL][k], &called)) {
			Native_ErrPrint();
		} else if (called) {
			Con_Printf("coop: player %d %s a ship: Parking.%s\n", i, ship ? "into" : "out of", cb[ship != NULL][k]);
		}
	}
	Players_Leave(was);
	if (stand && Players_Ship(i) == stand) {
		Players_SetShip(i, NULL);
	}
}

/* player i asks to go to its party's leader (the coordinator knows where it plays) */
void Cp_ToLeader(int i) {
	uint8_t buf[NET2_KEY_SIZE + 4];
	NetRepBuf w;
	const uint8_t *key = Travel_KeyOfPlayer(i);
	if (!key) {
		Con_Printf("coop: player %d has no key for the leader travel\n", i);
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_LEADER);
	CoordMsg_PutBytes(&w, key, NET2_KEY_SIZE);
	Travel_SendCoord(buf, w.size);
	Con_Printf("coop: player %d asks to go to its leader\n", i);
}

byte Coop_Message(Net2Host *host, int conn, const uint8_t *data, int size) {
	char ent[CP_NAME];
	NetRepReader r;
	(void)host;
	if (!Coop_On() || size < 2 || data[0] != COOP_MSG) {
		return 0;
	}
	int i = Rep_ConnPlayer(conn);
	NetRepReader_Init(&r, data + 2, size - 2);
	if (i < 0 || i >= PLAYERS_MAX) {
		return 1;
	}
	if (data[1] == COOP_C_ANSWER) {
		int kind = (int)NetRep_GetU8(&r);
		if (r.bad || !kind || kind != g_Coop.pl[i].menu) {
			return 1;
		}
		g_Coop.pl[i].menu = 0;
		if (kind == COOP_MENU_SHIPEDIT) {
			Cp_ShipEdited(i, &r);
			return 1;
		}
		if (kind == COOP_MENU_SPUTNIK) {
			Cp_SputnikDone(i, &r);
			return 1;
		}
		if (kind == COOP_MENU_CONV) {
			NetRep_GetStr(&r, ent, sizeof(ent));
			if (!r.bad && i == g_Coop.conv) {
				Cp_ConvPicked(i, ent);
			}
			return 1;
		}
		int yes = (int)NetRep_GetU8(&r);
		if (!r.bad) {
			Con_Printf("coop: player %d answers %s\n", i, yes ? "yes" : "no");
			Cp_MenuFunc(i, yes ? g_Coop.pl[i].yes : g_Coop.pl[i].no);
		}
	} else if (data[1] == COOP_C_LEADER) {
		Cp_ToLeader(i);
	} else if (data[1] == COOP_C_CHOICE) {
		NetRep_GetStr(&r, ent, sizeof(ent));
		int k = (int)NetRep_GetU8(&r);
		cEntity *t = r.bad ? NULL : World_FindEntity(ent);
		if (!t || !Entity_IsType(t, TYEN_WITH_LIFE) || k >= WLife(t)->arrow.nOpt) {
			return 1;
		}
		Str_Copy(g_Coop.pl[i].choiceEnt, ent, CP_NAME);
		g_Coop.pl[i].choice = k;
		/* shown at once on the arrow (the others near it see the same as in the single player) */
		WLife(t)->arrow.iOpt = k;
		Con_Printf("coop: player %d chooses option %d of %s (%s)\n", i, k, ent, cArrowIndicator_GetOpt(&WLife(t)->arrow, k));
	}
	return 1;
}

void Coop_CoordLost(void) {
	g_Coop.synced = 0;
}

/* a change of the coordinator into the save variables; a name changed here and not sent yet keeps its value, a
 * counter its own addition on the coordinator's number */
static void Cp_Apply(int kind, const char *name, const char *value) {
	char sum[24];
	long m, v;
	if (!Cp_Story(name)) {
		return;
	}
	int i = Cp_DirtyAt(name);
	if (i >= 0) {
		const char *here = SaveVar_Get(name);
		if (kind != COORD_OP_SET || !g_Coop.dirty[i].counter || !g_Coop.dirty[i].hasBase || !here || !CoordStory_Int(here, &v) ||
			!CoordStory_Int(value, &m)) {
			return;
		}
		Str_PrintfQuiet(sum, sizeof(sum), "%ld", m + v - g_Coop.dirty[i].base);
		g_Coop.dirty[i].base = m;
		value = sum;
	}
	g_Coop.applying = 1;
	if (kind == COORD_OP_SET) {
		SaveVar_Set(name, value);
	} else {
		SaveVar_Delete(name);
	}
	g_Coop.applying = 0;
}

typedef struct CpNames {
	int n;
	char name[COOP_DIRTY * 2][CP_NAME];
} CpNames;

static void Cp_Collect(const char *name, const char *value, void *u) {
	CpNames *l = u;
	(void)value;
	if (Cp_Story(name) && Cp_DirtyAt(name) < 0 && l->n < COOP_DIRTY * 2) {
		Str_Copy(l->name[l->n++], name, CP_NAME);
	}
}

/* the whole story: the variables of the story not in it go */
static void Cp_ApplyAll(const char *text, int len) {
	static CpNames old;
	static char key[CP_NAME], val[COORD_STORY_VALUE + 1];
	old.n = 0;
	SaveVars_Each(Cp_Collect, &old);
	for (int i = 0; i < old.n; i++) {
		if (!CoordKv_Get(text, len, old.name[i], val, sizeof(val))) {
			Cp_Apply(COORD_OP_DEL, old.name[i], NULL);
		}
	}
	int count = 0;
	for (int pos = 0; CoordKv_Next(text, len, &pos, key, sizeof(key), val, sizeof(val)); count++) {
		Cp_Apply(COORD_OP_SET, key, val);
	}
	Con_Printf("coop: the whole story from the coordinator, %d variables (%d changes of here to send)\n", count,
		g_Coop.ndirty);
}

/* the player given to its party's travel (the coordinator's summon) */
static void Cp_Summon(NetRepReader *r) {
	uint8_t key[NET2_KEY_SIZE];
	char level[COORD_LEVEL + 1], entrance[COORD_LEVEL + 1], door[COORD_LEVEL + 1];
	CoordMsg_GetBytes(r, key, NET2_KEY_SIZE);
	NetRep_GetStr(r, level, sizeof(level));
	NetRep_GetStr(r, entrance, sizeof(entrance));
	NetRep_GetStr(r, door, sizeof(door));
	int i = r->bad ? -1 : Travel_PlayerOfKey(key);
	if (i < 0) {
		return;
	}
	Con_Printf("coop: player %d goes along with its party to %s\n", i, level);
	Travel_Go(i, level, entrance, door, 0);
}

byte Coop_CoordMessage(const uint8_t *data, int size) {
	static char name[CP_NAME], val[COORD_STORY_VALUE + 1];
	NetRepReader r;
	int kind;

	if (!Coop_On() || size < 1) {
		return 0;
	}
	NetRepReader_Init(&r, data + 1, size - 1);
	switch (data[0]) {
	case COORD_MSG_STORY: {
		int count = (int)NetRep_GetU16(&r);
		for (int i = 0; i < count && CoordStory_Next(&r, &kind, name, sizeof(name), val, sizeof(val)) > 0; i++) {
			Cp_Apply(kind, name, val);
		}
		return 1;
	}
	case COORD_MSG_STORYALL: {
		uint32_t n = NetRep_GetU32(&r);
		if (r.bad || n > (uint32_t)(size - 5)) {
			return 1;
		}
		Cp_ApplyAll((const char *)data + 5, (int)n);
		g_Coop.synced = 1;
		Cp_Send();
		return 1;
	}
	case COORD_MSG_SUMMON:
		Cp_Summon(&r);
		return 1;
	}
	return 0;
}

/* ---- the mission level changes ---- */

void Cp_ToClient(int i, const uint8_t *buf, int size) {
	int conn = Rep_PlayerConn(i);
	if (conn >= 0 && Instance_Host()) {
		Net2_Send(Instance_Host(), conn, REP_CH_RELIABLE, buf, size);
	}
}

void Cp_ToAll(const uint8_t *buf, int size) {
	for (int i = 0; i < PLAYERS_MAX; i++) {
		Cp_ToClient(i, buf, size);
	}
}

static void Cp_Notice(int i, const char *key, const char *arg) {
	uint8_t buf[256];
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_NOTICE);
	NetRep_PutStr(&w, key);
	NetRep_PutStr(&w, arg);
	if (i < 0) {
		Cp_ToAll(buf, w.size);
	} else {
		Cp_ToClient(i, buf, w.size);
	}
}

/* the member the scripts act for: the current one, else the first one here */
int Cp_Player(void) {
	int i = Players_Current();
	if (i != PLAYERS_NONE && Players_Get(i)) {
		return i;
	}
	for (i = 0; i < PLAYERS_MAX; i++) {
		if (Rep_PlayerConn(i) >= 0 && Players_Get(i)) {
			return i;
		}
	}
	return -1;
}

byte Coop_KeepStory(const char *prefix) {
	if (!Coop_On() || g_Coop.applying || prefix[0]) {
		return 0;
	}
	/* the single player's end deletes every save variable before the menu: the party keeps its story */
	Con_Printf("coop: the story is kept (all save variables deleted by the scripts)\n");
	return 1;
}

/* the end of the story (QLoad of the menu after the credits' start): kept as finished; every member here sees the
 * credits in the main menu, the members elsewhere get a notice */
static void Cp_StoryEnd(void) {
	uint8_t buf[8], nb[128];
	NetRepBuf w, nw;
	SaveVar_Set(COORD_COOP_FINISHED, "yes");
	Cp_Send();
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_END);
	Cp_ToAll(buf, w.size);
	NetRepBuf_Init(&nw, nb, sizeof(nb));
	NetRep_PutU8(&nw, COORD_MSG_PNOTICE);
	NetRep_PutStr(&nw, "MP_CoopEnd|");
	Travel_SendCoord(nb, nw.size);
	Con_Printf("coop: the story is finished: the credits on the clients\n");
}

/* the level again (a super deal's restart): the instance starts again, every member here travels to its new start */
static void Cp_Reset(const char *map) {
	uint8_t buf[4] = { COORD_MSG_RESET };
	const char *spawn = SaveVar_Get("Spawn");
	Cp_Send();
	Travel_SendCoord(buf, 1);
	Con_Printf("coop: the level again (QLoad %s): the instance starts again for its members\n", map);
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (Rep_PlayerConn(i) >= 0) {
			Travel_Go(i, Instance_Level(), spawn ? spawn : "", "", 0);
		}
	}
}

/* the video the scripts set for the level change (Scrap.Set("PlayVideo"), a mission's end) to the client of member
 * i (-1: all here): it plays it before its next level, as the single player's level load does */
static void Cp_Video(int i) {
	uint8_t buf[CP_NAME * 2 + 8];
	nativeVal_t v;
	NetRepBuf w;
	Native_SetNone(&v);
	if (Native_GVarGet("PlayVideo", &v) && v.type == NV_STR && Native_Text(&v)[0]) {
		NetRepBuf_Init(&w, buf, sizeof(buf));
		NetRep_PutU8(&w, COOP_MSG);
		NetRep_PutU8(&w, COOP_Y_VIDEO);
		NetRep_PutStr(&w, Native_Text(&v));
		if (!w.bad) {
			if (i < 0) {
				Cp_ToAll(buf, w.size);
			} else {
				Cp_ToClient(i, buf, w.size);
			}
			Con_Printf("coop: the video %s to %s\n", Native_Text(&v), i < 0 ? "the members here" : "the member");
		}
		Mode_GVarSetS("PlayVideo", "");
	}
	Native_ErrClear();
	Native_Free(&v);
}

byte Coop_QLoad(const char *map) {
	char key[CP_NAME];
	if (!Coop_On()) {
		return 0;
	}
	const char *s = strrchr(map, '/');
	const char *level = s ? s + 1 : map;
	if (!crt_stricmp(level, "Menu")) {
		Cp_StoryEnd();
		return 1;
	}
	if (!crt_stricmp(level, Instance_Level())) {
		Cp_Reset(map);
		return 1;
	}
	/* the member of the scene whose end goes on (its callback), else the bound one */
	int i = g_Coop.inScene && g_Coop.scenePlayer >= 0 && Players_Get(g_Coop.scenePlayer) ? g_Coop.scenePlayer : Cp_Player();
	if (i < 0) {
		return 0;
	}
	const char *spawn = SaveVar_Get("Spawn");
	byte party = CoordCoop_Chapter(level) || CoordCoop_Chapter(Instance_Level());
	/* the way point goes with the story: the target level's start reads it as the single player's (its race and
	 * arrow are placed at the spawn, Coop_StandIn) */
	int nentry;
	const char *const *entry = Party_RuleList("entry", &nentry);
	for (int k = 0; k < nentry; k++) {
		const char *v = SaveVar_Get(entry[k]);
		Str_PrintfQuiet(key, sizeof(key), CP_ENTRY "%s", entry[k]);
		SaveVar_Set(key, !strcmp(entry[k], "Map") ? map : v ? v : "");
	}
	/* the story of now first: the target instance starts with it */
	Cp_Send();
	Cp_Video(party ? -1 : i);
	Con_Printf("coop: mission level change of player %d to %s (%s)%s\n", i, level, spawn ? spawn : "", party ?
		", the party goes along" : "");
	if (party) {
		Cp_Notice(-1, "MP_CoopTravel", level);
	}
	/* a way point from a door class (ComeFrom "DoorEditShip"): the members arrive in that door, as the single player's
	 * start puts the player in its elevator */
	const char *from = SaveVar_Get("ComeFrom");
	return Travel_Go(i, level, spawn ? spawn : "", from && !strncmp(from, "Door", 4) ? from : "", party);
}

void Coop_EntityGone(cEntity *e) {
	if (!Coop_On() || (g_Coop.started && !g_Coop.starting) || g_Coop.ngone == CP_GONE || e == g_Coop.standIn || Players_Of(e) >= 0) {
		return;
	}
	for (int k = 0; k < g_Coop.ngone; k++) {
		if (!strcmp(g_Coop.gone[k], e->name)) {
			return;
		}
	}
	Str_Copy(g_Coop.gone[g_Coop.ngone++], e->name, CP_NAME);
}

/* the entities the level's start deleted (a mission's Init: the client's level start runs none) to member i */
static void Cp_TellGone(int i) {
	static uint8_t buf[CP_GONE_MSG];
	NetRepBuf w;
	if (!g_Coop.ngone) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_GONE);
	NetRep_PutU16(&w, (uint32_t)g_Coop.ngone);
	for (int k = 0; k < g_Coop.ngone; k++) {
		NetRep_PutStr(&w, g_Coop.gone[k]);
	}
	if (!w.bad) {
		Cp_ToClient(i, buf, w.size);
	}
}

/* a super deal whose mode drops the baton or the flag at the player's death and brings the player back (its
 * Chars.OnMainCharDeath: CTF, Domination, MortalRace; the others count the death against the party) */
static byte Cp_DealDeathHook(void) {
	const char *lib = SaveVar_Get("Mission.Library"), *type = SaveVar_Get("SuperDealType");
	nativeVal_t hook;
	if (!lib || strncmp(lib, "SuperDeal_", 10) || !type ||
		(strcmp(type, "CTF") && strcmp(type, "Domination") && strcmp(type, "MortalRace")) || !Mode_Import("Chars")) {
		return 0;
	}
	Native_SetNone(&hook);
	byte has = Mode_ModGet("Chars", "OnMainCharDeath", &hook) && hook.type != NV_NONE;
	Native_Free(&hook);
	Native_ErrClear();
	return has;
}

void Coop_Died(int i, cEntity *e) {
	if (!Coop_On() || g_Coop.partyDead) {
		return;
	}
	/* a super deal's mode hears every member's death (a member that carried the baton dropped it; it was lost with
	 * him) */
	byte deal = Cp_DealDeathHook();
	for (int k = 0; !deal && k < PLAYERS_MAX; k++) {
		if (k != i && Rep_PlayerConn(k) >= 0 && !Rep_PlayerDead(k) && Players_Get(k)) {
			return;
		}
	}
	/* the party died: the single player's death (the mission's hook, else the end of the game: Coop_MissionFailed) */
	Con_Printf(deal ? "coop: player %d died in the super deal\n" : "coop: every member here died (player %d last)\n", i);
	nativeVal_t a[2];
	Native_SetStr(&a[0], e->name);
	Native_SetStr(&a[1], "");
	g_Coop.partyDead = 1;
	int was = Players_Enter(i);
	if (!Mode_Import("Chars") || !PORT_CALL(Chars, MainCharDeath, NULL, "vv", &a[0], &a[1])) {
		Native_ErrPrint();
	}
	Players_Leave(was);
	g_Coop.partyDead = 0;
	Native_Free(&a[0]);
	Native_Free(&a[1]);
}

byte Coop_PartyDeath(void) {
	return g_Coop.partyDead;
}

byte Coop_MissionFailed(void) {
	if (!Coop_On()) {
		return 0;
	}
	if (g_Coop.failAt <= 0.0) {
		g_Coop.failAt = Instance_Time() + CP_FAIL_SECS;
		Cp_Notice(-1, "MP_CoopMissionFailed", "");
		Con_Printf("coop: the mission failed: the level starts again in %.0f s\n", CP_FAIL_SECS);
	}
	return 1;
}

byte Coop_PartyTravel(const char *level) {
	const char *s = strrchr(level, '/');
	level = s ? s + 1 : level;
	if (!Coop_On() || (!CoordCoop_Chapter(level) && !CoordCoop_Chapter(Instance_Level()))) {
		return 0;
	}
	Cp_Send();
	Cp_Notice(-1, "MP_CoopTravel", level);
	return 1;
}

/* a free place next to member m, the k-th around it */
static byte Cp_Beside(const cEntity *m, int k, vec3_t *out) {
	float a = (float)k * 1.3f;
	vec3_t at = { m->pos.x + CP_GATHER * cosf(a), m->pos.y + 20.0f, m->pos.z + CP_GATHER * sinf(a) };
	if (Rep_FreeSpot(&at, 0, out)) {
		return 1;
	}
	return Rep_FreeSpot(&m->pos, 1, out);
}

byte Coop_Spawn(int player, vec3_t *pos, float *yaw) {
	char found[64];
	vec3_t ang;
	if (!Coop_On()) {
		return 0;
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *m = i != player ? Players_Get(i) : NULL;
		if (m && Rep_PlayerConn(i) >= 0 && NetPl_Kind(m) == NETREP_CHAR && Cp_Beside(m, player + 1, pos)) {
			*yaw = Obj(m)->ang.y;
			Con_Printf("coop: player %d starts next to player %d\n", player, i);
			return 1;
		}
	}
	/* the single player's start without a way point (Chars.CreateStartupPlayer): the first spawn but the junkyard's */
	DummySearch_Start("DM_Player_Spawn*", 1);
	while (DummySearch_NextEuler(found, sizeof(found), pos, &ang)) {
		if (strcmp(found, "DM_Player_Spawn_Junkyard")) {
			*yaw = ang.y + M_PI_F;
			Con_Printf("coop: player %d starts at %s\n", player, found);
			return 1;
		}
	}
	return 0;
}

cEntity *Coop_HangarShip(const vec3_t *p, float yaw) {
	char var[CP_NAME];
	nativeVal_t c[3], pos, cad, out;
	const char *n = SaveVar_Get("Hangar.iHangarShip");
	int k = n ? crt_atoi(n) : -1;
	/* the hangar's aux slot (9) is the junkyard's lifeboat of the story's start: a standard ship then, as the single
	 * player takes a parked one; the lifeboat where the level has none of those (GDB: the bishops' ships only) */
	Native_SetFloat(&c[0], p->x);
	Native_SetFloat(&c[1], p->y);
	Native_SetFloat(&c[2], p->z);
	Mode_Tuple3(&pos, &c[0], &c[1], &c[2]);
	if (k == 9) {
		nativeVal_t ang;
		Native_SetFloat(&ang, yaw);
		Native_SetNone(&out);
		cEntity *std = PORT_CALL(Parking, CreateParkedShip, &out, "vv", &pos, &ang) && out.type == NV_ENT ? Native_Ent(&out) : NULL;
		Native_ErrClear();
		Native_Free(&out);
		if (std) {
			Native_Free(&pos);
			return std;
		}
		Str_Copy(var, "Hangar.HangarShipAux", sizeof(var));
	} else {
		Str_PrintfQuiet(var, sizeof(var), "Hangar.HangarShip%d", k + 1);
	}
	const char *text = k >= 0 && k <= 9 ? SaveVar_Get(var) : NULL;
	if (!text || !text[0] || text[0] == '<') {
		Native_Free(&pos);
		return NULL;
	}
	/* Parking.CreatePlayerShip's ship of the hangar the story flies, a parked one for the member */
	Native_SetStr(&cad, text);
	Native_SetNone(&out);
	cEntity *ship = NULL;
	if (Mode_Import("ShipEdit") && PORT_CALL(ShipEdit, CreateShipFromSaveVar, &out, "vvs", &cad, &pos, "Parked") &&
		out.type == NV_ENT && (ship = Native_Ent(&out)) != NULL) {
		int one = 1;
		vec3_t ang = { 0.0f, yaw, 0.0f };
		Entity_VarI(ship, "Active", &one, 1);
		Entity_VarI(ship, "ActiveCMask", &one, 1);
		Obj_SetAng(ship, &ang);
		if (!PORT_CALL(Parking, SetShipAsParked, NULL, "s", ship->name)) {
			Native_ErrPrint();
		}
		Con_Printf("coop: the party's hangar ship %s (%s)\n", ship->name, var);
	} else {
		Native_ErrPrint();
	}
	Native_Free(&out);
	Native_Free(&cad);
	Native_Free(&pos);
	return ship;
}

byte Coop_NoShipCall(void) {
	return Coop_On() && !g_IsMapOutDoor;
}

byte Coop_UseAny(cEntity *t) {
	return Coop_On() && t && !Entity_IsType(t, TYEN_CAR);
}

void Coop_Spawned(cEntity *e) {
	if (Coop_On() && NetPl_Kind(e) == NETREP_CHAR) {
		WChar(e)->listenOn = 1;
		/* a talk turns the camera of view 0 too (the world's; -mp2: a player's context has its own camera of view 0,
		 * made by its join's steps) */
		Coop_SceneCamera();
	}
}

byte Coop_StartPos(vec3_t *p, float *yaw) {
	/* where the single player would stand: the way point's spawn, else the level's start */
	const char *spawn = SaveVar_Get("Spawn");
	char found[64];
	vec3_t ang;
	if (!Coop_On()) {
		return 0;
	}
	if (spawn && spawn[0]) {
		DummySearch_Start(spawn, 1);
	}
	if (spawn && spawn[0] && DummySearch_NextEuler(found, sizeof(found), p, &ang)) {
		*yaw = ang.y + M_PI_F;
		return 1;
	}
	return Coop_Spawn(-1, p, yaw);
}

void Coop_StandIn(byte on) {
	vec3_t p;
	float yaw = 0.0f;
	/* the Init of the map and of the mission libraries runs (Coop_EntityGone keeps what it deletes) */
	g_Coop.starting += on ? 1 : g_Coop.starting > 0 ? -1 : 0;
	if (!on && !g_Coop.starting && Coop_On()) {
		Con_Printf("coop: the level's start deleted %d entities so far (told to the clients)\n", g_Coop.ngone);
	}
	if (!on) {
		/* the stand-in stays in the world (the scripts keep its name) but is no player now (also after the
		 * MissionsFuncs.Init inside the level's Init, as before) */
		g_Coop.standIn = NULL;
		/* the outer start ends: inactive, no step, no fall and no sound of its own near the members who come (it is
		 * not replicated: a member heard its fall at the spawn beside its own, pres.double:caida-dtritus) */
		if (!g_Coop.starting && g_Coop.standInKept && World_FindEntity(g_Coop.standInName) == g_Coop.standInKept) {
			Entity_SetActive(g_Coop.standInKept, 0);
		}
		return;
	}
	if (g_Coop.standIn || !Coop_On() || Players_Any()) {
		return;
	}
	/* the one of an earlier start of this level again (the world numbers a second one's name: CoopStandIn17) */
	if (g_Coop.standInKept && World_FindEntity(g_Coop.standInName) == g_Coop.standInKept) {
		g_Coop.standIn = g_Coop.standInKept;
		Entity_SetActive(g_Coop.standIn, 1);
		return;
	}
	if (!Coop_StartPos(&p, &yaw)) {
		return;
	}
	/* inside the box of the level's grid of points: a level started without the story's way point (an instance kept up
	 * before the story came) took a far spawn dummy, and the AI's paths to the stand-in failed the grid's check (QF) */
	const cGridElements *g = g_NodesWorld ? g_NodesWorld->sup.grid : NULL;
	if (g) {
		p.x = p.x < g->min.x ? g->min.x : p.x > g->max.x ? g->max.x : p.x;
		p.y = p.y < g->min.y ? g->min.y : p.y > g->max.y ? g->max.y : p.y;
		p.z = p.z < g->min.z ? g->min.z : p.z > g->max.z ? g->max.z : p.z;
	}
	g_Coop.standIn = g_Coop.standInKept = cWorld_CreateEntity(g_pWorld, COOP_STANDIN, p.x, p.y, p.z, TYEN_WALKCHAR);
	if (g_Coop.standIn) {
		vec3_t ang = { 0.0f, yaw, 0.0f };
		Str_Copy(g_Coop.standInName, g_Coop.standIn->name, sizeof(g_Coop.standInName));
		Faction_EnsureClass("Dtritus");
		WithLife_SetActCtrl(g_Coop.standIn, "Dtritus");
		Obj_SetAng(g_Coop.standIn, &ang);
		Con_Printf("coop: a stand-in player for the level's start at %.0f %.0f %.0f\n", (double)p.x, (double)p.y,
			(double)p.z);
	}
}

byte Coop_IsStandIn(const cEntity *e) {
	return Coop_On() && e && e->name && !strncmp(e->name, COOP_STANDIN, sizeof(COOP_STANDIN) - 1);
}

cEntity *Coop_StandInEntity(void) {
	return g_Coop.standIn;
}

const char *Coop_MainCharName(void) {
	int i = Coop_On() || Rep_SpFlows() ? Players_Acting() : PLAYERS_NONE;
	if (i == PLAYERS_NONE) {
		return NULL;
	}
	Str_PrintfQuiet(g_Coop.mainChar, sizeof(g_Coop.mainChar), REP_PLAYER_PREFIX "%d_c%u", i, ++g_Coop.mainChars);
	return g_Coop.mainChar;
}

byte Coop_Adopts(int i, cEntity *e) {
	int kind = Coop_On() || Rep_SpFlows() ? NetPl_Kind(e) : 0;
	if ((kind != NETREP_CHAR && kind != NETREP_SHIP) || WLife(e)->life <= 0) {
		return 0;
	}
	cActCtrl *a = WLife(e)->actCtrl;
	const char *cls = a && a->cls ? a->cls->name : "";
	if (kind == NETREP_CHAR) {
		Faction_SetChar(i, cls);
		Coop_Spawned(e);
	}
	Con_Printf("%s: player %d plays as %s (%s), a %s of the scripts\n", Coop_On() ? "coop" : "instance", i, e->name, cls,
		kind == NETREP_CHAR ? "character" : "ship");
	return 1;
}

void Coop_ActCtrlSet(cEntity *e) {
	int i = Coop_On() ? Players_Of(e) : -1;
	cActCtrl *a = i >= 0 && NetPl_Kind(e) == NETREP_CHAR ? WLife(e)->actCtrl : NULL;
	if (a && a->cls) {
		Faction_SetChar(i, a->cls->name);
		Con_Printf("coop: player %d is a %s now (its class set by the scripts)\n", i, a->cls->name);
	}
}

/* the story to the clients' copies: the changed variables (all: the whole story) to client i (-1: all here) */
typedef struct CpStoryOut {
	int to;
	byte all;
	NetRepBuf w;
	int at, n;
} CpStoryOut;

static void Cp_StoryFlush(CpStoryOut *o) {
	if (o->n && !o->w.bad) {
		o->w.p[o->at] = (uint8_t)(o->n & 0xff);
		o->w.p[o->at + 1] = (uint8_t)(o->n >> 8);
		if (o->to < 0) {
			Cp_ToAll(o->w.p, o->w.size);
		} else {
			Cp_ToClient(o->to, o->w.p, o->w.size);
		}
	}
	NetRepBuf_Init(&o->w, o->w.p, NET2_MAX_MESSAGE);
	NetRep_PutU8(&o->w, COOP_MSG);
	NetRep_PutU8(&o->w, COOP_Y_STORY);
	o->at = o->w.size;
	NetRep_PutU16(&o->w, 0);
	o->n = 0;
}

static void Cp_StoryPut(CpStoryOut *o, const char *name, const char *value) {
	int len = (int)strlen(name) + (value ? (int)strlen(value) : 0) + 8;
	if (o->w.size + len > NET2_MAX_MESSAGE) {
		Cp_StoryFlush(o);
	}
	CoordStory_Put(&o->w, value ? COORD_OP_SET : COORD_OP_DEL, name, value);
	o->n++;
}

static void Cp_StoryEach(const char *name, const char *value, void *u) {
	if (Cp_ClientStoryVar(name)) {
		Cp_StoryPut(u, name, value);
	}
}

void Cp_TellStory(int to, byte all) {
	static uint8_t buf[NET2_MAX_MESSAGE];
	CpStoryOut o = { .to = to, .all = all };
	o.w.p = buf;
	Cp_StoryFlush(&o);
	if (all) {
		SaveVars_Each(Cp_StoryEach, &o);
	} else {
		for (int k = 0; k < g_Coop.ncdirty; k++) {
			Cp_StoryPut(&o, g_Coop.cdirty[k], SaveVar_Get(g_Coop.cdirty[k]));
		}
	}
	Cp_StoryFlush(&o);
}

/* a talk the level's Init started with the stand-in (TalkToTraitoriusAfterCTF: Traitorius' talk in front of the
 * player) goes on with member i as its player: Speech.PlayerName (the stand-in is gone after the Init), its line again */
static void Cp_TalkPlayer(int i) {
	nativeVal_t pn, v;
	cEntity *e = Players_Get(i);
	Native_SetNone(&pn);
	if (e && Mode_ModGet("Speech", "PlayerName", &pn) && pn.type == NV_STR &&
		!strncmp(Native_Text(&pn), COOP_STANDIN, sizeof(COOP_STANDIN) - 1)) {
		Native_SetStr(&v, e->name);
		/* its line again for the member (the stand-in's had no client and no end) */
		nativeVal_t cur;
		Native_SetNone(&cur);
		int was = Players_Enter(i);
		if (Mode_SetModVar("Speech", "PlayerName", &v) && Mode_ModGet("Speech", "CurrNI", &cur) && cur.type == NV_INT &&
			PORT_CALL(Speech, RunNI, NULL, "v", &cur)) {
			Con_Printf("coop: the talk of the level's start goes on with player %d\n", i);
		} else {
			Native_ErrPrint();
		}
		Players_Leave(was);
		Native_Free(&cur);
		Native_Free(&v);
	}
	Native_ErrClear();
	Native_Free(&pn);
}

/* a scene of the level's start that the stand-in started (IntroHuman2's news: its end's callback gets the scene's
 * player, Scene.usr, the stand-in that is gone) is the first member's: it is the scene's player now */
static void Cp_ScenePlayer(int i) {
	nativeVal_t usr, name, h;
	cEntity *e = Players_Get(i);
	if (!g_Coop.inScene || g_Coop.scenePlayer >= 0 || !e) {
		return;
	}
	Native_SetNone(&usr);
	Native_SetNone(&name);
	Native_SetNone(&h);
	byte gone = !Mode_ModGet("Scene", "usr", &usr) || !Native_True(&usr) || !Mode_VGet(&usr, "Name", &name) ||
		name.type != NV_STR || !strncmp(Native_Text(&name), COOP_STANDIN, sizeof(COOP_STANDIN) - 1);
	Native_ErrClear();
	if (gone && Mode_EntHandle(e, &h) && Mode_SetModVar("Scene", "usr", &h)) {
		g_Coop.scenePlayer = i;
		Con_Printf("coop: the scene %s of the level's start is player %d's\n", g_Coop.sceneCls, i);
	}
	Native_ErrPrint();
	Native_Free(&usr);
	Native_Free(&name);
	Native_Free(&h);
}

void Coop_Joined(int i) {
	static uint8_t buf[CP_SCENE_MSG];
	nativeVal_t h;
	if (!Coop_On()) {
		return;
	}
	/* the client's copy of the story for its next level starts */
	Cp_TellStory(i, 1);
	Cp_TellGone(i);
	g_Coop.pl[i].menu = 0;
	g_Coop.pl[i].held[0] = 0;
	int nhud;
	const char *const *hud = Party_RuleList("hud", &nhud);
	for (int k = 0; k < nhud; k++) {
		Cp_HudVar(i, hud[k]);
	}
	if (g_Coop.waiting) {
		g_Coop.waiting = 0;
		Timer_SetSpeed(&g_pWorld->timer, 1.0f);
		Con_Printf("coop: player %d came: the level goes on\n", i);
		Cp_TalkPlayer(i);
		Cp_ScenePlayer(i);
		Elevator_FirstMember();
	}
	/* an empty list of options: the client knows it plays the coop story (its keys) */
	uint8_t none[16];
	NetRepBuf nw;
	NetRepBuf_Init(&nw, none, sizeof(none));
	Cp_OptionsMsg(&nw, NULL);
	g_Coop.pl[i].sentSize = 0;
	Cp_ToClient(i, none, nw.size);
	if (!g_Coop.inScene) {
		return;
	}
	/* hidden and stopped as the scene's start did to the members there (its end shows it again) */
	cEntity *e = Players_Get(i);
	Native_SetNone(&h);
	if (e && (!Mode_EntHandle(e, &h) || !Mode_Import("Scene") || !PORT_CALL(Scene, HideThis, NULL, "v", &h))) {
		Native_ErrPrint();
	}
	Native_Free(&h);
	int size = g_Mp2 ? 0 : Cp_SceneMsg(buf, sizeof(buf));
	if (size) {
		Cp_ToClient(i, buf, size);
	}
	NetScene_Joined(i);
	Con_Printf("coop: player %d came in during scene %s: it plays on its client too\n", i, g_Coop.sceneCls);
}

byte Coop_SceneHolds(void) {
	return Coop_On() && g_Coop.inScene;
}

byte Coop_Holds(int i) {
	return Coop_On() && (g_Coop.inScene || (i >= 0 && i < PLAYERS_MAX && (g_Coop.pl[i].menu == COOP_MENU_SHIPEDIT ||
		g_Coop.pl[i].menu == COOP_MENU_SPUTNIK || Sputniks_CoopHolds(i) || g_Coop.pl[i].held[0])));
}

int Coop_HeldPlayer(cEntity *e) {
	for (int i = 0; Coop_On() && e && i < PLAYERS_MAX; i++) {
		if (g_Coop.pl[i].held[0] && !strcmp(g_Coop.pl[i].held, e->name)) {
			return i;
		}
	}
	return -1;
}

int Coop_UsrRead(int slot) {
	return (Coop_On() || Rep_SpFlows()) && slot >= PLAYERS_NET_SLOT && slot < PLAYERS_NET_SLOT + PLAYERS_MAX ? 0 : slot;
}

int Coop_UsrSlot(cEntity *e, int old, int slot) {
	if (!Coop_On()) {
		/* single player's flows of the players (include/scrap/play.h, Rep_SpFlows): the scripts' 0 is the acting
		 * player's slot */
		if (!Rep_SpFlows()) {
			return slot;
		}
		if (slot >= 0 && slot < MAX_LOCAL_PLAYERS) {
			int i = Players_Acting();
			slot = old >= PLAYERS_NET_SLOT && old < PLAYERS_NET_SLOT + PLAYERS_MAX ? old :
				i == PLAYERS_NONE ? slot : PLAYERS_NET_SLOT + i;
		}
		/* the server steps a player's entity from the moment the scripts give it to the player, as single player's
		 * from that frame on (Rep_OwnFrame); one they take from it steps by itself again */
		Obj(e)->netStep = slot >= PLAYERS_NET_SLOT && slot < PLAYERS_NET_SLOT + PLAYERS_MAX;
		return slot;
	}
	if (slot == -1 && old >= PLAYERS_NET_SLOT && old < PLAYERS_NET_SLOT + PLAYERS_MAX) {
		/* a dead one is no ride of the scripts: its member gets a new character */
		if (!Entity_IsType(e, TYEN_WITH_LIFE) || WLife(e)->life > 0) {
			Str_Copy(g_Coop.pl[old - PLAYERS_NET_SLOT].held, e->name, CP_NAME);
		}
		return slot;
	}
	if (slot >= PLAYERS_NET_SLOT && slot < PLAYERS_NET_SLOT + PLAYERS_MAX) {
		g_Coop.pl[slot - PLAYERS_NET_SLOT].held[0] = 0;
	}
	if (slot < 0 || slot >= MAX_LOCAL_PLAYERS) {
		return slot;
	}
	if (old >= PLAYERS_NET_SLOT && old < PLAYERS_NET_SLOT + PLAYERS_MAX) {
		return old;		/* a member's own (the scripts read its slot as 0, Coop_UsrRead) */
	}
	int i = Coop_HeldPlayer(e);
	if (i < 0) {
		i = Players_Acting();
	}
	if (i == PLAYERS_NONE) {
		return slot;
	}
	g_Coop.pl[i].held[0] = 0;
	return PLAYERS_NET_SLOT + i;
}

/* the other members on foot next to the member of the scene */
static void Cp_Gather(void) {
	cEntity *m = g_Coop.scenePlayer >= 0 ? Players_Get(g_Coop.scenePlayer) : NULL;
	if (!m) {
		return;
	}
	int k = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = Players_Get(i);
		if (i == g_Coop.scenePlayer || !e || Rep_PlayerConn(i) < 0 || NetPl_Kind(e) != NETREP_CHAR) {
			continue;
		}
		vec3_t p;
		if (!Cp_Beside(m, ++k, &p)) {
			continue;
		}
		e->pos = p;
		Obj(e)->ang.y = Obj(m)->ang.y;
		WChar(e)->view.y = Obj(m)->ang.y;
		Con_Printf("coop: player %d stands next to player %d after the scene\n", i, g_Coop.scenePlayer);
	}
}

void Coop_SceneEnding(void) {
	if (!Coop_On() || !g_Coop.inScene) {
		return;
	}
	g_Coop.endingSerial = g_Coop.serial;
	g_Coop.endingTable = g_Coop.scene;
	Str_Copy(g_Coop.endingCls, g_Coop.sceneCls, sizeof(g_Coop.endingCls));
	/* the scene's member (hidden by the scene its entity is no player found: its connection counts) for the end and its
	 * callback, until Coop_SceneEnd */
	g_Coop.sceneWas = Players_Current();
	if (g_Coop.scenePlayer >= 0 && Rep_PlayerConn(g_Coop.scenePlayer) >= 0) {
		Players_Enter(g_Coop.scenePlayer);
	}
}

void Coop_SceneEnd(void) {
	uint8_t buf[128];
	NetRepBuf w;
	if (!Coop_On() || !g_Coop.inScene || g_Coop.endingSerial == 0) {
		return;
	}
	if (g_Coop.endingTable >= 0 && !g_Mp2) {
		NetRepBuf_Init(&w, buf, sizeof(buf));
		NetRep_PutU8(&w, COOP_MSG);
		NetRep_PutU8(&w, COOP_Y_SCENEEND);
		NetRep_PutStr(&w, g_Coop.endingCls);
		Cp_ToAll(buf, w.size);
	}
	Con_Printf("coop: scene %s ended\n", g_Coop.endingCls);
	/* the callback started the next scene: that one plays on */
	if (g_Coop.serial == g_Coop.endingSerial) {
		Cp_Gather();
		g_Coop.inScene = 0;
		g_Coop.scene = -1;
		Input_SelectActionSetNamed("");
	}
	g_Coop.endingSerial = 0;
	Players_Leave(g_Coop.sceneWas);
}

/* ---- the texts and voices of the scripts ---- */

/* to member s and the members within COOP_HEAR of it (s none: to all) */
void Cp_ToNear(int s, const uint8_t *buf, int size) {
	cEntity *se = s >= 0 && s < PLAYERS_MAX ? Players_Get(s) : NULL;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = Players_Get(i);
		if (Rep_PlayerConn(i) < 0 || (!e && i != s)) {
			continue;
		}
		float dx = se && e ? e->pos.x - se->pos.x : 0.0f, dy = se && e ? e->pos.y - se->pos.y : 0.0f;
		float dz = se && e ? e->pos.z - se->pos.z : 0.0f;
		if (i == s || dx * dx + dy * dy + dz * dz <= COOP_HEAR * COOP_HEAR) {
			Cp_ToClient(i, buf, size);
		}
	}
}

/* the mission's member here: the one bound when the story's mission changed last, else the first member that came
 * while it was gone (the one that traveled to the mission's level); -1 */
int Cp_MissionPlayer(void) {
	int i = g_Coop.missionPlayer;
	if (i >= 0 && i < PLAYERS_MAX && Rep_PlayerConn(i) >= 0 && Players_Joined(i) == g_Coop.missionJoined) {
		return i;
	}
	g_Coop.missionPlayer = -1;
	for (i = 0; i < PLAYERS_MAX; i++) {
		if (Rep_PlayerConn(i) >= 0) {
			Cp_SetMissionPlayer(i);
			return i;
		}
	}
	return -1;
}

int Coop_MissionPlayer(void) {
	return Coop_On() ? Cp_MissionPlayer() : -1;
}

static void Cp_SetMissionPlayer(int i) {
	if (i >= 0 && i < PLAYERS_MAX && Rep_PlayerConn(i) >= 0 && i != g_Coop.missionPlayer) {
		g_Coop.missionPlayer = i;
		g_Coop.missionJoined = Players_Joined(i);
		Con_Printf("coop: player %d is the mission's member here\n", i);
	}
}
