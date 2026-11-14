/*
 * The coop story of a world: the party's save variables on the instances, the mission level changes as travel, the
 * cutscenes and conversations on the members' clients, see include/scrap/coop.h. Not in the original.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/ai.h"
#include "scrap/camcontrol.h"
#include "scrap/chartype.h"
#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/graphic2d.h"
#include "scrap/input.h"
#include "scrap/instance.h"
#include "scrap/netrep.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/pyfunction.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/sound.h"
#include "scrap/text.h"
#include "scrap/texts.h"
#include "scrap/travel.h"
#include "scrap/vehicleattack.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"
#include "game/Script/levels/Levels.h"

#define CP_NAME 64					/* bytes of a name of the story (CoordKv's keys) */
#define CP_MSG 16384				/* bytes of one story message to the coordinator */
#define CP_SCENE_MSG 8192			/* bytes of a scene's table for the clients */
#define CP_SCENE_LINES 256
#define CP_GATHER 160.0f			/* units between the members placed next to the member of a scene */
#define CP_VOICES 4
#define CP_OPTS_MSG (8 + CP_NAME + COOP_OPTS * (COOP_OPT_TEXT + 2))
#define COOP_CAM "CoopCam"
#define CP_MENU_MSG 8192			/* bytes of a menu message (the hangar's save variables) */
#define CP_MENU_TITLE 1024
#define CP_HANGAR "Hangar."			/* the save variables of the ship editor */
#define CP_ANSWER_SECS 2.0			/* test: a question's answer after this */
#define CP_LIFEBOAT "SLifeBoat<-<-<-<-<-0,0,0,0,0,0,1<-50<-0,0,0,0,0,0"	/* the hangar's spare ship (a test's ship) */

/* the variables of a player, not of the story (where it goes on, its character, money and alarm, its time) */
static const char *const cp_local[] = { "Map", "Spawn", "ComeFrom", "PrevMap", "Char", "AcBatteries", "Money", "Alarm",
	"GameplayTime", NULL };
/* the way point of a chapter's travel, given to the start of the target level (its scripts read them at once) */
static const char *const cp_entry[] = { "Map", "Spawn", "ComeFrom", "PrevMap", NULL };
#define CP_ENTRY "Coop.Entry."

#define CP_TABLES 4					/* call tables of one scene (its Load table and the actors' after it) */

typedef struct CpScene {
	char cls[CP_NAME];
	const char *module;
	int ntables;
	const char *const *lines[CP_TABLES];
	char dataModule[CP_NAME], dataSection[CP_NAME];	/* the level data section that loads it, "" */
} CpScene;

static struct {
	byte on;
	char story[COORD_PATH];			/* the file of -story */
	byte applying;					/* the coordinator's story goes in: not sent back */
	byte synced;					/* the whole story came since the coordinator connected */
	byte overflow;					/* more changes than the list holds: the whole story goes */
	byte levelSent;					/* the changes of the level's start went (COORD_MSG_STORYINIT) */
	byte hold;						/* test: the changes wait */
	int ndirty;
	struct {
		char name[CP_NAME];
		byte counter;				/* a counter (Stats.*, or SaveVar_Inc): its change goes as an addition */
		byte hasBase;
		long base;					/* its number when it became dirty (0 for a new one) */
	} dirty[COOP_DIRTY];
	int nscenes;
	CpScene scenes[COOP_SCENES];
	int loading;					/* the scene whose Load table ran last (its later tables join it), -1 */
	char dataModule[CP_NAME], dataSection[CP_NAME];	/* the level data section that runs, "" */
	byte inScene;					/* a scene of the scripts plays */
	int scene;						/* its table (the clients play it), -1 none */
	int scenePlayer;				/* the member bound when it started */
	uint32_t serial;				/* scenes started so far */
	uint32_t endingSerial;			/* the one whose end runs (its callback may start the next) */
	int endingTable;
	char endingCls[CP_NAME], sceneCls[CP_NAME];
	char voice[CP_VOICES][128];		/* the file opened on each voice channel */
	struct {
		uint8_t sent[CP_OPTS_MSG];	/* the options told to the member last */
		int sentSize;
		char choiceEnt[CP_NAME];	/* the entity whose option the member chose, "" */
		int choice;
		byte menu;					/* the menu it answers (COOP_MENU_*), 0 */
		char held[CP_NAME];			/* its character the scripts took (UsrControl -1), "" */
		char yes[CP_NAME], no[CP_NAME];
	} pl[PLAYERS_MAX];
	char via[CP_NAME];				/* the function of a menu's answer that runs ("" none) */
	struct {						/* client: the options of the instance */
		char ent[CP_NAME];
		int n, cur;
		char opt[COOP_OPTS][COOP_OPT_TEXT];
	} copt;
	char clientScene[CP_NAME];		/* client: the scene it plays for its instance */
	int pendingSize;				/* client: a scene message that waits for the client's own level to run */
	uint8_t pending[CP_SCENE_MSG];
	byte skipped;					/* client: it ended here (Escape, or its last frame before the instance's) */
	byte aborting;					/* client: Escape ends it */
	byte ended;						/* client: the story is finished, the world is left */
	byte coop;						/* client: its instance plays the coop story */
	byte world;						/* client: its world plays the coop story (the cue of the coordinator) */
	byte replay;					/* client: the scene of the instance starts */
	byte cmenu;						/* client: the instance's menu it shows (COOP_MENU_*), 0 */
	byte testEdit;					/* client test: SCRAP_COOP_SHIPEDIT */
	double cmenuAt;
	double sceneAt;					/* client: when it began */
	char notice[256];
} cp = { .scene = -1, .loading = -1 };

static byte Cp_InList(const char *const *list, const char *s) {
	for (; *list; list++) {
		if (!crt_stricmp(*list, s)) {
			return 1;
		}
	}
	return 0;
}

byte Coop_ParseParam(const char *param, const char *ext) {
	if (!crt_stricmp("mode", param) && !crt_stricmp(ext, "coop")) {
		cp.on = 1;
		return 1;
	}
	if (!crt_stricmp("story", param)) {
		Str_Copy(cp.story, ext, sizeof(cp.story));
		return 1;
	}
	return 0;
}

byte Coop_On(void) {
	return g_Instance && cp.on;
}

byte Coop_Asked(void) {
	return cp.on;
}

/* ---- the story ---- */

static byte Cp_Story(const char *name) {
	return name[0] && (int)strlen(name) < CP_NAME && CoordKv_KeyOk(name) && !Cp_InList(cp_local, name);
}

static int Cp_DirtyAt(const char *name) {
	for (int i = 0; i < cp.ndirty; i++) {
		if (!strcmp(cp.dirty[i].name, name)) {
			return i;
		}
	}
	return -1;
}

void Coop_SaveVarChanged(const char *name, const char *old, byte inc) {
	if (!cp.on || cp.applying || !Cp_Story(name)) {
		return;
	}
	int i = Cp_DirtyAt(name);
	if (i < 0) {
		if (cp.ndirty == COOP_DIRTY) {
			cp.overflow = 1;
			return;
		}
		i = cp.ndirty++;
		Str_Copy(cp.dirty[i].name, name, CP_NAME);
		cp.dirty[i].base = 0;
		cp.dirty[i].hasBase = !old || CoordStory_Int(old, &cp.dirty[i].base);
		cp.dirty[i].counter = !strncmp(name, "Stats.", 6);
	}
	cp.dirty[i].counter |= inc;
}

void Coop_LevelStart(const char *level) {
	static char text[COORD_STORY_MAX];
	static char key[CP_NAME], val[COORD_STORY_VALUE + 1];

	if (!Coop_On()) {
		return;
	}
	int n = cp.story[0] ? Coord_ReadFile(cp.story, text, sizeof(text)) : -1;
	if (n <= 0) {
		/* a new party: the new game of the single player (sent to the coordinator as the story's first changes) */
		SaveGame_CoopNewGame();
		Con_Printf("coop: a new story (the new game), %d changes for the coordinator\n", cp.ndirty);
		return;
	}
	cp.applying = 1;
	SaveVars_Delete("");
	int count = 0;
	for (int pos = 0; CoordKv_Next(text, n, &pos, key, sizeof(key), val, sizeof(val)); count++) {
		SaveVar_Set(key, val);
	}
	/* the level a chapter's travel came to: its way point as the single player's level start has it */
	const char *map = SaveVar_Get(CP_ENTRY "Map");
	const char *s = map ? strrchr(map, '/') : NULL, *here = level ? strrchr(level, '/') : NULL;
	if (s && here && !crt_stricmp(s, here)) {
		for (int k = 0; cp_entry[k]; k++) {
			Str_PrintfQuiet(key, sizeof(key), CP_ENTRY "%s", cp_entry[k]);
			const char *v = SaveVar_Get(key);
			SaveVar_Set(cp_entry[k], v ? v : "");
		}
		Con_Printf("coop: the way point of the chapter: %s, spawn %s, from %s\n", map, SaveVar_Get("Spawn"),
			SaveVar_Get("ComeFrom"));
		/* taken: a later start of this level is the single player's load of a save there (Junkyard's arrival with
		 * Berto once) */
		cp.applying = 0;
		for (int k = 0; cp_entry[k]; k++) {
			Str_PrintfQuiet(key, sizeof(key), CP_ENTRY "%s", cp_entry[k]);
			SaveVar_Delete(key);
		}
	}
	cp.applying = 0;
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
	NetRep_PutU8(&o->w, cp.levelSent ? COORD_MSG_STORY : COORD_MSG_STORYINIT);
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
	const char *value = SaveVar_Get(cp.dirty[i].name);
	if (value && cp.dirty[i].counter && cp.dirty[i].hasBase && CoordStory_Int(value, &v)) {
		if (v != cp.dirty[i].base) {
			Str_PrintfQuiet(delta, sizeof(delta), "%ld", v - cp.dirty[i].base);
			Cp_OutPut(o, COORD_OP_ADD, cp.dirty[i].name, delta);
		}
		return;
	}
	Cp_OutPut(o, value ? COORD_OP_SET : COORD_OP_DEL, cp.dirty[i].name, value);
}

static void Cp_PutAll(const char *name, const char *value, void *u) {
	if (Cp_Story(name)) {
		Cp_OutPut(u, COORD_OP_SET, name, value);
	}
}

/* the changes to the coordinator now (once the whole story came: before it the coordinator's would go over them) */
static void Cp_Send(void) {
	static CpOut o;
	if (!cp.synced || cp.hold || (!cp.ndirty && !cp.overflow)) {
		return;
	}
	Cp_OutStart(&o);
	if (cp.overflow) {
		SaveVars_Each(Cp_PutAll, &o);
	} else {
		for (int i = 0; i < cp.ndirty; i++) {
			Cp_OutDirty(&o, i);
		}
	}
	Cp_OutFlush(&o);
	cp.ndirty = 0;
	cp.overflow = 0;
	cp.levelSent = 1;
}

void Coop_SceneCamera(void) {
	if (!Coop_On() || !g_pWorld) {
		return;
	}
	const char *name = cWorld_GetCamName(g_pWorld, 0);
	if (name && *name && World_FindEntity(name)) {
		return;
	}
	if (!World_FindEntity(COOP_CAM) && !cWorld_CreateEntity(g_pWorld, COOP_CAM, 0.0f, 0.0f, 0.0f, TYEN_FCAM)) {
		return;
	}
	cWorld_SetCam(g_pWorld, 0, COOP_CAM);
}

static void Cp_ToClient(int i, const uint8_t *buf, int size);
static void Cp_MenuFunc(int i, const char *fn);
static void Cp_ShipEdited(int i, NetRepReader *r);

/* the entity player e would use: the one it talks to, else the one in front of it with a use arrow */
static cEntity *Cp_UseTarget(cEntity *e) {
	cEntity *t = WChar(e)->listenOn ? WChar_Listen(e) : NULL;
	if (!t) {
		t = WChar_Victim(e);
	}
	return t && Entity_IsType(t, TYEN_WITH_LIFE) && WLife(t)->arrow.canUse ? t : NULL;
}

/* the options message of the arrow of t (NULL: none) into w: their count */
static int Cp_OptionsMsg(NetRepBuf *w, cEntity *t) {
	cArrowIndicator *a = t ? &WLife(t)->arrow : NULL;
	int n = a ? (a->nOpt < COOP_OPTS ? a->nOpt : COOP_OPTS) : 0;
	NetRep_PutU8(w, COOP_MSG);
	NetRep_PutU8(w, COOP_Y_OPTS);
	NetRep_PutStr(w, n ? t->name : "");
	NetRep_PutU8(w, n ? (uint32_t)a->iOpt : 0);
	NetRep_PutU8(w, (uint32_t)n);
	for (int k = 0; k < n; k++) {
		const char *o = cArrowIndicator_GetOpt(a, k);
		char cut[COOP_OPT_TEXT];
		Str_Copy(cut, o ? o : "", sizeof(cut));
		NetRep_PutStr(w, cut);
	}
	return n;
}

/* the options of each member's target when they changed */
static void Cp_TellOptions(void) {
	uint8_t buf[CP_OPTS_MSG];
	NetRepBuf w;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = Players_Get(i);
		if (Rep_PlayerConn(i) < 0) {
			cp.pl[i].sentSize = 0;
			continue;
		}
		cEntity *t = e && NetPl_Kind(e) == NETREP_CHAR ? Cp_UseTarget(e) : NULL;
		NetRepBuf_Init(&w, buf, sizeof(buf));
		int n = Cp_OptionsMsg(&w, t);
		if (w.bad || (w.size == cp.pl[i].sentSize && !memcmp(buf, cp.pl[i].sent, (size_t)w.size))) {
			continue;
		}
		/* nothing to tell a member that was told nothing */
		if (!n && !cp.pl[i].sentSize) {
			continue;
		}
		memcpy(cp.pl[i].sent, buf, (size_t)w.size);
		cp.pl[i].sentSize = n ? w.size : 0;
		Cp_ToClient(i, buf, w.size);
		Con_Printf("coop: player %d: %d options of %s (%d)\n", i, n, n ? t->name : "-", n ? WLife(t)->arrow.iOpt : 0);
	}
}

void Coop_Pass(void) {
	if (Coop_On()) {
		/* nothing draws the talk scorer here: its time and the end of a remote communication */
		Scorer_TalkTick(1.0f / INSTANCE_TICK_HZ);
		Cp_Send();
		Cp_TellOptions();
	}
}

void Coop_BeforeUse(int i, cEntity *t) {
	if (!Coop_On() || i < 0 || i >= PLAYERS_MAX || !t || !Entity_IsType(t, TYEN_WITH_LIFE)) {
		return;
	}
	cArrowIndicator *a = &WLife(t)->arrow;
	if (cp.pl[i].choiceEnt[0] && !strcmp(cp.pl[i].choiceEnt, t->name) && cp.pl[i].choice < a->nOpt) {
		a->iOpt = cp.pl[i].choice;
	}
	/* the scripts move the arrow on after a use: the next choice starts from there */
	cp.pl[i].choiceEnt[0] = 0;
}

/* player i asks to go to its party's leader (the coordinator knows where it plays) */
static void Cp_ToLeader(int i) {
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
		if (r.bad || !kind || kind != cp.pl[i].menu) {
			return 1;
		}
		cp.pl[i].menu = 0;
		if (kind == COOP_MENU_SHIPEDIT) {
			Cp_ShipEdited(i, &r);
			return 1;
		}
		int yes = (int)NetRep_GetU8(&r);
		if (!r.bad) {
			Con_Printf("coop: player %d answers %s\n", i, yes ? "yes" : "no");
			Cp_MenuFunc(i, yes ? cp.pl[i].yes : cp.pl[i].no);
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
		Str_Copy(cp.pl[i].choiceEnt, ent, CP_NAME);
		cp.pl[i].choice = k;
		/* shown at once on the arrow (the others near it see the same as in the single player) */
		WLife(t)->arrow.iOpt = k;
		Con_Printf("coop: player %d chooses option %d of %s (%s)\n", i, k, ent, cArrowIndicator_GetOpt(&WLife(t)->arrow, k));
	}
	return 1;
}

void Coop_CoordLost(void) {
	cp.synced = 0;
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
		if (kind != COORD_OP_SET || !cp.dirty[i].counter || !cp.dirty[i].hasBase || !here || !CoordStory_Int(here, &v) ||
			!CoordStory_Int(value, &m)) {
			return;
		}
		Str_PrintfQuiet(sum, sizeof(sum), "%ld", m + v - cp.dirty[i].base);
		cp.dirty[i].base = m;
		value = sum;
	}
	cp.applying = 1;
	if (kind == COORD_OP_SET) {
		SaveVar_Set(name, value);
	} else {
		SaveVar_Delete(name);
	}
	cp.applying = 0;
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
		cp.ndirty);
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
		cp.synced = 1;
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

static void Cp_ToClient(int i, const uint8_t *buf, int size) {
	int conn = Rep_PlayerConn(i);
	if (conn >= 0 && Instance_Host()) {
		Net2_Send(Instance_Host(), conn, REP_CH_RELIABLE, buf, size);
	}
}

static void Cp_ToAll(const uint8_t *buf, int size) {
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

/* the member the scripts act for: the bound one, else the first one here */
static int Cp_Player(void) {
	int i = Players_Bound();
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
	if (!Coop_On() || cp.applying || prefix[0]) {
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

byte Coop_QLoad(const char *map) {
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
	int i = cp.inScene && cp.scenePlayer >= 0 && Players_Get(cp.scenePlayer) ? cp.scenePlayer : Cp_Player();
	if (i < 0) {
		return 0;
	}
	const char *spawn = SaveVar_Get("Spawn");
	byte party = CoordCoop_Chapter(level) || CoordCoop_Chapter(Instance_Level());
	if (party) {
		char key[CP_NAME];
		for (int k = 0; cp_entry[k]; k++) {
			const char *v = SaveVar_Get(cp_entry[k]);
			Str_PrintfQuiet(key, sizeof(key), CP_ENTRY "%s", cp_entry[k]);
			SaveVar_Set(key, !strcmp(cp_entry[k], "Map") ? map : v ? v : "");
		}
	}
	/* the story of now first: the target instance starts with it */
	Cp_Send();
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

byte Coop_NoShipCall(void) {
	return Coop_On() && !g_IsMapOutDoor;
}

byte Coop_UseAny(cEntity *t) {
	return Coop_On() && t && !Entity_IsType(t, TYEN_CAR);
}

/* ---- the cutscenes ---- */

/* a table of the scene's classes and actors (the lines of the modules a scene's load uses) */
static byte Cp_SceneLines(const char *const *lines) {
	static const char *const mods[] = { "Scene.", "SAct.", "MakeChar.", NULL };
	for (; *lines; lines++) {
		for (int k = 0; mods[k]; k++) {
			if (!strncmp(*lines, mods[k], strlen(mods[k]))) {
				return 1;
			}
		}
	}
	return 0;
}

/* the entry of scene cls (a new one when missing), NULL when the list is full */
static CpScene *Cp_SceneSlot(const char *cls) {
	int k = 0;
	while (k < cp.nscenes && strcmp(cp.scenes[k].cls, cls)) {
		k++;
	}
	if (k == COOP_SCENES) {
		return NULL;
	}
	cp.nscenes += k == cp.nscenes;
	Str_Copy(cp.scenes[k].cls, cls, CP_NAME);
	return &cp.scenes[k];
}

void Coop_SceneTable(const char *module, const char *const *lines) {
	static const char head[] = "Scene.Load|";
	char cls[CP_NAME];
	if (!Coop_On() || !lines || !lines[0]) {
		return;
	}
	if (strncmp(lines[0], head, sizeof(head) - 1)) {
		CpScene *sc = cp.loading >= 0 ? &cp.scenes[cp.loading] : NULL;
		if (sc && sc->module == module && sc->ntables < CP_TABLES && Cp_SceneLines(lines)) {
			sc->lines[sc->ntables++] = lines;
		} else {
			cp.loading = -1;
		}
		return;
	}
	const char *c = lines[0] + sizeof(head) - 1;
	int n = 0;
	while (c[n] && c[n] != '|' && n < CP_NAME - 1) {
		n++;
	}
	memcpy(cls, c, (size_t)n);
	cls[n] = 0;
	CpScene *sc = Cp_SceneSlot(cls);
	if (!sc) {
		return;
	}
	sc->module = module;
	sc->ntables = 1;
	sc->lines[0] = lines;
	sc->dataModule[0] = sc->dataSection[0] = 0;
	cp.loading = (int)(sc - cp.scenes);
}

void Coop_DataSection(const char *module, const char *section) {
	if (!Coop_On()) {
		return;
	}
	Str_Copy(cp.dataModule, module ? module : "", CP_NAME);
	Str_Copy(cp.dataSection, section ? section : "", CP_NAME);
}

void Coop_SceneLoad(const char *cls) {
	CpScene *sc;
	/* only the load of a data section (a call table's load is kept by Coop_SceneTable) */
	if (!Coop_On() || !cp.dataSection[0] || !(sc = Cp_SceneSlot(cls))) {
		return;
	}
	sc->module = "";
	sc->ntables = 0;
	Str_Copy(sc->dataModule, cp.dataModule, CP_NAME);
	Str_Copy(sc->dataSection, cp.dataSection, CP_NAME);
	Con_Printf("coop: scene %s loads from the data section %s of %s\n", cls, cp.dataSection, cp.dataModule);
}

/* the message of the scene that plays (its table, or the class alone of a scene of the level's own data); 0 when it
 * does not fit */
static int Cp_SceneMsg(uint8_t *buf, int size) {
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, size);
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_SCENE);
	if (cp.scene < 0) {
		NetRep_PutStr(&w, "");
		NetRep_PutStr(&w, cp.sceneCls);
		NetRep_PutU16(&w, 0);
		NetRep_PutStr(&w, "");
		NetRep_PutStr(&w, "");
		return w.size;
	}
	const CpScene *sc = &cp.scenes[cp.scene];
	int n = 0;
	for (int t = 0; t < sc->ntables; t++) {
		for (const char *const *l = sc->lines[t]; *l && n < CP_SCENE_LINES; l++) {
			n++;
		}
	}
	NetRep_PutStr(&w, sc->module);
	NetRep_PutStr(&w, cp.sceneCls);
	NetRep_PutU16(&w, (uint32_t)n);
	int m = 0;
	for (int t = 0; t < sc->ntables; t++) {
		for (const char *const *l = sc->lines[t]; *l && m < n; l++, m++) {
			int len = (int)strlen(*l);
			NetRep_PutU16(&w, (uint32_t)len);
			CoordMsg_PutBytes(&w, *l, len);
		}
	}
	NetRep_PutStr(&w, sc->dataModule);
	NetRep_PutStr(&w, sc->dataSection);
	return w.bad ? 0 : w.size;
}

void Coop_ScenePlay(const char *cls) {
	static uint8_t buf[CP_SCENE_MSG];
	uint8_t nb[256];
	NetRepBuf nw;

	if (!Coop_On()) {
		return;
	}
	cp.inScene = 1;
	cp.serial++;
	cp.scenePlayer = Cp_Player();
	cp.scene = -1;
	Str_Copy(cp.sceneCls, cls, sizeof(cp.sceneCls));
	for (int k = 0; k < cp.nscenes; k++) {
		if (!strcmp(cp.scenes[k].cls, cls)) {
			cp.scene = k;
		}
	}
	/* the members elsewhere: a notice through the coordinator */
	NetRepBuf_Init(&nw, nb, sizeof(nb));
	NetRep_PutU8(&nw, COORD_MSG_PNOTICE);
	char text[96];
	Str_PrintfQuiet(text, sizeof(text), "MP_CoopScene|%s", Instance_Level());
	NetRep_PutStr(&nw, text);
	Travel_SendCoord(nb, nw.size);
	int size = Cp_SceneMsg(buf, sizeof(buf));
	if (!size) {
		Con_Printf("coop: the table of scene %s does not fit a message\n", cls);
		cp.scene = -1;
		size = Cp_SceneMsg(buf, sizeof(buf));
	}
	Cp_ToAll(buf, size);
	if (cp.scene < 0) {
		/* a scene of the level's own data (map.def): the clients have its class from their level's start */
		Con_Printf("coop: scene %s plays (no table: the clients play the class they have)\n", cls);
	} else {
		Con_Printf("coop: scene %s plays on the clients (player %d)\n", cls, cp.scenePlayer);
	}
}

void Coop_Spawned(cEntity *e) {
	if (Coop_On() && NetPl_Kind(e) == NETREP_CHAR) {
		WChar(e)->listenOn = 1;
		/* a talk turns the camera of view 0 too */
		Coop_SceneCamera();
	}
}

void Coop_Joined(int i) {
	static uint8_t buf[CP_SCENE_MSG];
	nativeVal_t h;
	if (!Coop_On()) {
		return;
	}
	cp.pl[i].menu = 0;
	cp.pl[i].held[0] = 0;
	/* an empty list of options: the client knows it plays the coop story (its keys) */
	uint8_t none[16];
	NetRepBuf nw;
	NetRepBuf_Init(&nw, none, sizeof(none));
	Cp_OptionsMsg(&nw, NULL);
	cp.pl[i].sentSize = 0;
	Cp_ToClient(i, none, nw.size);
	if (!cp.inScene) {
		return;
	}
	/* hidden and stopped as the scene's start did to the members there (its end shows it again) */
	cEntity *e = Players_Get(i);
	Native_SetNone(&h);
	if (e && (!Mode_EntHandle(e, &h) || !Mode_Import("Scene") || !PORT_CALL(Scene, HideThis, NULL, "v", &h))) {
		Native_ErrPrint();
	}
	Native_Free(&h);
	int size = Cp_SceneMsg(buf, sizeof(buf));
	if (size) {
		Cp_ToClient(i, buf, size);
	}
	Con_Printf("coop: player %d came in during scene %s: it plays on its client too\n", i, cp.sceneCls);
}

byte Coop_SceneHolds(void) {
	return Coop_On() && cp.inScene;
}

byte Coop_Holds(int i) {
	return Coop_On() && (cp.inScene || (i >= 0 && i < PLAYERS_MAX && (cp.pl[i].menu == COOP_MENU_SHIPEDIT ||
		cp.pl[i].held[0])));
}

int Coop_HeldPlayer(cEntity *e) {
	for (int i = 0; Coop_On() && e && i < PLAYERS_MAX; i++) {
		if (cp.pl[i].held[0] && !strcmp(cp.pl[i].held, e->name)) {
			return i;
		}
	}
	return -1;
}

int Coop_UsrSlot(cEntity *e, int old, int slot) {
	if (!Coop_On()) {
		return slot;
	}
	if (slot == -1 && old >= PLAYERS_NET_SLOT && old < PLAYERS_NET_SLOT + PLAYERS_MAX) {
		Str_Copy(cp.pl[old - PLAYERS_NET_SLOT].held, e->name, CP_NAME);
		return slot;
	}
	if (slot >= PLAYERS_NET_SLOT && slot < PLAYERS_NET_SLOT + PLAYERS_MAX) {
		cp.pl[slot - PLAYERS_NET_SLOT].held[0] = 0;
	}
	if (slot < 0 || slot >= MAX_LOCAL_PLAYERS) {
		return slot;
	}
	int i = Coop_HeldPlayer(e);
	if (i < 0) {
		i = Players_Bound();
	}
	if (i == PLAYERS_NONE) {
		return slot;
	}
	cp.pl[i].held[0] = 0;
	return PLAYERS_NET_SLOT + i;
}

/* the other members on foot next to the member of the scene */
static void Cp_Gather(void) {
	cEntity *m = cp.scenePlayer >= 0 ? Players_Get(cp.scenePlayer) : NULL;
	if (!m) {
		return;
	}
	int k = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = Players_Get(i);
		if (i == cp.scenePlayer || !e || Rep_PlayerConn(i) < 0 || NetPl_Kind(e) != NETREP_CHAR) {
			continue;
		}
		vec3_t p;
		if (!Cp_Beside(m, ++k, &p)) {
			continue;
		}
		e->pos = p;
		Obj(e)->ang.y = Obj(m)->ang.y;
		WChar(e)->view.y = Obj(m)->ang.y;
		Con_Printf("coop: player %d stands next to player %d after the scene\n", i, cp.scenePlayer);
	}
}

void Coop_SceneEnding(void) {
	if (!Coop_On() || !cp.inScene) {
		return;
	}
	cp.endingSerial = cp.serial;
	cp.endingTable = cp.scene;
	Str_Copy(cp.endingCls, cp.sceneCls, sizeof(cp.endingCls));
	if (cp.scenePlayer >= 0 && Players_Get(cp.scenePlayer)) {
		Players_Bind(cp.scenePlayer);
	}
}

void Coop_SceneEnd(void) {
	uint8_t buf[128];
	NetRepBuf w;
	if (!Coop_On() || !cp.inScene || cp.endingSerial == 0) {
		return;
	}
	if (cp.endingTable >= 0) {
		NetRepBuf_Init(&w, buf, sizeof(buf));
		NetRep_PutU8(&w, COOP_MSG);
		NetRep_PutU8(&w, COOP_Y_SCENEEND);
		NetRep_PutStr(&w, cp.endingCls);
		Cp_ToAll(buf, w.size);
	}
	Con_Printf("coop: scene %s ended\n", cp.endingCls);
	/* the callback started the next scene: that one plays on */
	if (cp.serial == cp.endingSerial) {
		Cp_Gather();
		cp.inScene = 0;
		cp.scene = -1;
	}
	cp.endingSerial = 0;
	Players_BindTurn();
}

/* ---- the texts and voices of the scripts ---- */

/* a scene the clients play themselves makes its own texts and voices */
static byte Cp_Quiet(void) {
	return !Coop_On() || cp.inScene;
}

void Coop_SpeechText(const char *text, float time, int r, int g, int b) {
	uint8_t buf[1024];
	NetRepBuf w;
	if (Cp_Quiet()) {
		return;
	}
	int n = (int)strlen(text);
	n = n > 900 ? 900 : n;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_SPEECH);
	NetRep_PutU16(&w, (uint32_t)n);
	CoordMsg_PutBytes(&w, text, n);
	NetRep_PutU32(&w, (uint32_t)(time > 0.0f ? time * 1000.0f : 0.0f));
	NetRep_PutU8(&w, (uint32_t)(r & 255));
	NetRep_PutU8(&w, (uint32_t)(g & 255));
	NetRep_PutU8(&w, (uint32_t)(b & 255));
	/* the speaker's client shows the conversation */
	int i = Players_Bound();
	Con_Printf("coop: speech for player %d: %.60s\n", i == PLAYERS_NONE ? -1 : i, text);
	if (i != PLAYERS_NONE && Rep_PlayerConn(i) >= 0) {
		Cp_ToClient(i, buf, w.size);
	} else {
		Cp_ToAll(buf, w.size);
	}
}

void Coop_MsgText(const char *text, float time, const char *font) {
	uint8_t buf[1200];
	NetRepBuf w;
	if (Cp_Quiet()) {
		return;
	}
	int n = (int)strlen(text);
	n = n > 1000 ? 1000 : n;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MSG);
	NetRep_PutU16(&w, (uint32_t)n);
	CoordMsg_PutBytes(&w, text, n);
	NetRep_PutU32(&w, (uint32_t)(time > 0.0f ? time * 1000.0f : 0.0f));
	NetRep_PutStr(&w, font ? font : "");
	Cp_ToAll(buf, w.size);
}

void Coop_OpenVoice(const char *file, int channel) {
	if (Coop_On() && channel >= 0 && channel < CP_VOICES) {
		Str_Copy(cp.voice[channel], file, sizeof(cp.voice[channel]));
	}
}

void Coop_PlayVoice(int channel) {
	uint8_t buf[192];
	NetRepBuf w;
	if (Cp_Quiet() || channel < 0 || channel >= CP_VOICES || !cp.voice[channel][0]) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_VOICE);
	NetRep_PutU8(&w, (uint32_t)channel);
	NetRep_PutStr(&w, cp.voice[channel]);
	/* the speaker and the members near it hear it */
	int s = Players_Bound();
	cEntity *se = s != PLAYERS_NONE ? Players_Get(s) : NULL;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = Players_Get(i);
		if (!e || Rep_PlayerConn(i) < 0) {
			continue;
		}
		float dx = se ? e->pos.x - se->pos.x : 0.0f, dy = se ? e->pos.y - se->pos.y : 0.0f;
		float dz = se ? e->pos.z - se->pos.z : 0.0f;
		if (dx * dx + dy * dy + dz * dz <= COOP_HEAR * COOP_HEAR) {
			Cp_ToClient(i, buf, w.size);
		}
	}
}

/* ---- the menus of the scripts ---- */

/* the save variables of the ship editor (and the act of the game for the editor's choice of parts) as COOP_VARS */
typedef struct CpVars {
	NetRepBuf *w;
	byte act;
	int count;
} CpVars;

static void Cp_PutVar(const char *name, const char *value, void *u) {
	CpVars *v = u;
	int n = (int)strlen(value);
	byte hangar = !strncmp(name, CP_HANGAR, sizeof(CP_HANGAR) - 1);
	if ((!hangar && (!v->act || strcmp(name, "GameAct"))) || strlen(name) > 255 || n > COORD_STORY_VALUE) {
		return;
	}
	NetRep_PutStr(v->w, name);
	NetRep_PutU16(v->w, (uint32_t)n);
	CoordMsg_PutBytes(v->w, value, n);
	v->count++;
}

static void Cp_PutVars(NetRepBuf *w, byte act) {
	CpVars v = { w, act, 0 };
	int at = w->size;
	NetRep_PutU16(w, 0);
	SaveVars_Each(Cp_PutVar, &v);
	if (!w->bad) {
		w->p[at] = (uint8_t)v.count;
		w->p[at + 1] = (uint8_t)(v.count >> 8);
	}
}

/* COOP_VARS into the save variables (only the ship editor's, GameAct too with act) */
static void Cp_GetVars(NetRepReader *r, byte act) {
	static char val[COORD_STORY_VALUE + 1];
	char name[256];
	int count = (int)NetRep_GetU16(r);
	for (int k = 0; k < count && !r->bad; k++) {
		NetRep_GetStr(r, name, sizeof(name));
		int n = (int)NetRep_GetU16(r);
		if (n > COORD_STORY_VALUE) {
			r->bad = 1;
			break;
		}
		CoordMsg_GetBytes(r, val, n);
		val[n] = 0;
		if (!r->bad && (!strncmp(name, CP_HANGAR, sizeof(CP_HANGAR) - 1) || (act && !strcmp(name, "GameAct")))) {
			SaveVar_Set(name, val);
		}
	}
}

byte Coop_ShipEditAway(void) {
	int i = Coop_On() ? Cp_Player() : -1;
	return i >= 0 && cp.pl[i].menu == COOP_MENU_SHIPEDIT;
}

byte Coop_KeepTime(void) {
	return Coop_On();
}

byte Coop_MenuYesNo(const char *title, const char *yes, const char *no) {
	uint8_t buf[CP_MENU_TITLE + 16];
	NetRepBuf w;
	if (!Coop_On()) {
		return 0;
	}
	int i = Cp_Player();
	if (i < 0 || Rep_PlayerConn(i) < 0) {
		return 0;
	}
	int n = (int)strlen(title);
	n = n > CP_MENU_TITLE ? CP_MENU_TITLE : n;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MENU);
	NetRep_PutU8(&w, COOP_MENU_YESNO);
	NetRep_PutU16(&w, (uint32_t)n);
	CoordMsg_PutBytes(&w, title, n);
	cp.pl[i].menu = COOP_MENU_YESNO;
	Str_Copy(cp.pl[i].yes, yes, CP_NAME);
	Str_Copy(cp.pl[i].no, no, CP_NAME);
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: player %d is asked (%s or %s)\n", i, yes, no);
	return 1;
}

byte Coop_ShipEditOpen(void) {
	static uint8_t buf[CP_MENU_MSG];
	NetRepBuf w;
	if (!Coop_On()) {
		return 0;
	}
	int i = Cp_Player();
	if (i < 0 || Rep_PlayerConn(i) < 0) {
		return 0;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MENU);
	NetRep_PutU8(&w, COOP_MENU_SHIPEDIT);
	NetRep_PutStr(&w, cp.via);
	NetRep_PutU32(&w, (uint32_t)Players_Money(i));
	Cp_PutVars(&w, 1);
	if (w.bad) {
		Con_Printf("coop: the hangar of player %d does not fit a message\n", i);
		return 0;
	}
	cp.pl[i].menu = COOP_MENU_SHIPEDIT;
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: the ship editor opens on the client of player %d%s%s\n", i, cp.via[0] ? " with " : "", cp.via);
	return 1;
}

/* "Module.Func"(0, "") for player i: the function its answer chose */
static void Cp_MenuFunc(int i, const char *fn) {
	char mod[CP_NAME];
	const char *dot = strchr(fn, '.');
	if (!dot || dot - fn >= CP_NAME) {
		return;
	}
	Str_PrintfQuiet(mod, sizeof(mod), "%.*s", (int)(dot - fn), fn);
	Players_Bind(i);
	Str_Copy(cp.via, fn, sizeof(cp.via));
	if (!Mode_Import(mod) || !Native_CallMod(mod, dot + 1, NULL, "is", 0L, "")) {
		Native_ErrPrint();
	}
	cp.via[0] = 0;
	Players_BindTurn();
}

/* player i closed the ship editor: its hangar into the story, its money, the ship on the platform as the editor's end
 * leaves it, then the editor's callback of the scripts */
static void Cp_ShipEdited(int i, NetRepReader *r) {
	nativeVal_t cb, args[2];
	int money = (int)NetRep_GetU32(r);
	if (r->bad) {
		return;
	}
	Players_Bind(i);
	Cp_GetVars(r, 0);
	g_pWorld->money = money;
	if (!Junkyard_PlatformShip()) {
		Native_ErrPrint();
	}
	const char *cur = SaveVar_Get(CP_HANGAR "iHangarShip");
	Con_Printf("coop: player %d closed the ship editor (hangar %s, money %d)\n", i, cur ? cur : "-", money);
	Native_SetNone(&cb);
	if (Mode_ModGet("ShipEdit", "EndShipEditCallBack", &cb) && cb.type != NV_NONE) {
		Native_SetInt(&args[0], 0);
		Native_SetStr(&args[1], "");
		if (!Mode_CallArgs(&cb, NULL, args, 2)) {
			Native_ErrPrint();
		}
		Native_Free(&args[1]);
	}
	Native_ErrClear();
	Native_Free(&cb);
	Players_BindTurn();
}

/* ---- tests ---- */

byte Coop_Command(const char *line) {
	char name[CP_NAME], val[256];
	int i;
	if (!Coop_On()) {
		return 0;
	}
	if (sscanf(line, "leader %d", &i) == 1) {
		Cp_ToLeader(i);
		return 1;
	}
	if (sscanf(line, "qload %63s", name) == 1 || !strcmp(line, "restart") || !strcmp(line, "theend")) {
		/* as the scripts do: SaveGame.QLoad, SaveGame.Restart (a super deal), the end's credits section */
		Players_Bind(Cp_Player());
		if (!strcmp(line, "theend")) {
			SaveVars_ScriptDelete("");
			Str_Copy(name, "Levels/Menu", sizeof(name));
		}
		byte ok = Mode_Import("SaveGame") && (!strcmp(line, "restart") ? PORT_CALL(SaveGame, Restart, NULL, "") :
			PORT_CALL(SaveGame, QLoad, NULL, "s", name));
		if (!ok) {
			Native_ErrPrint();
		}
		Players_BindTurn();
		return 1;
	}
	if (!strncmp(line, "call ", 5) && line[5] != '@') {
		/* a call of the scripts as a call table line ("Module.Func|arg|..."), for the bound player */
		const char *one[2] = { line + 5, NULL };
		Players_Bind(Cp_Player());
		Con_Printf("coop: call %s: %s\n", line + 5, Mode_Calls(NULL, one) ? "done" : "failed");
		Native_ErrPrint();
		Players_BindTurn();
		return 1;
	}
	if (sscanf(line, "kill %63s", name) == 1 && strchr(name, '.')) {
		/* the entities of a list of the scripts (the targets of a mission) die as by a weapon of the member the scripts
		 * act for: their OnDeath runs with it as the winner */
		nativeVal_t list, it;
		char *dot = strchr(name, '.');
		*dot = 0;
		int p = Cp_Player(), n = Mode_ModGet(name, dot + 1, &list) ? Mode_Loop(&list) : -1;
		cEntity *by = p >= 0 ? Players_Get(p) : NULL;
		for (int k = 0; k < n; k++) {
			Native_SetNone(&it);
			cEntity *v = Native_SeqItem(&list, k, &it) && it.type == NV_STR ? World_FindEntity(Native_Text(&it)) : NULL;
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
	} else if (sscanf(line, "story hold %d", &i) == 1) {
		cp.hold = (byte)(i != 0);
		Con_Printf("coop: the story changes %s\n", cp.hold ? "wait" : "go");
	} else if (sscanf(line, "story get %63s", name) == 1) {
		const char *v = SaveVar_Get(name);
		Con_Printf("coop: %s %s%s\n", name, v ? "= " : "is not set", v ? v : "");
	} else {
		const char *lib = SaveVar_Get("Mission.Library"), *map = SaveVar_Get("Mission.Map");
		Con_Printf("coop: story %s, %d changes to send, mission %s in %s, scene %s, action set %s\n", cp.synced ?
			"in sync" : "waiting for the coordinator", cp.ndirty, lib ? lib : "-", map ? map : "-", cp.inScene ? "playing" :
			"none", TABLE_AT(g_ActionSets, Input_GetActionSet())->name);
	}
	return 1;
}

/* ================================ client ================================ */

void Coop_Cue(int kind, const char *text) {
	char key[64], line[256];
	if (kind == COORD_CUE_COOP) {
		cp.world = 1;
	} else if (kind == COORD_CUE_INTRO) {
		Con_Printf("coop: a new story: the intro %s\n", text);
		if (!g_HeadlessPlay && !Mode_GVarSetS("PlayVideo", text)) {
			Native_ErrPrint();
		}
	} else if (kind == COORD_CUE_NOTICE) {
		const char *bar = strchr(text, '|');
		int n = bar ? (int)(bar - text) : (int)strlen(text);
		Str_PrintfQuiet(key, sizeof(key), "%.*s", n, text);
		Texts_Format(line, sizeof(line), key, bar ? bar + 1 : "");
		Con_Printf("coop: %s\n", line);
		Text_Message(COOP_NOTICE_MS, "%s", line);
	}
}

byte Coop_Client(void) {
	return g_Play && cp.world;
}

void Coop_ClientNoAnim(const char *entity, const char *action) {
	static char told[16][CP_NAME * 2];
	static int ntold;
	char key[CP_NAME * 2];
	Str_PrintfQuiet(key, sizeof(key), "%s.%s", entity, action);
	for (int k = 0; k < ntold; k++) {
		if (!strcmp(told[k], key)) {
			return;
		}
	}
	if (ntold < 16) {
		Str_Copy(told[ntold++], key, sizeof(told[0]));
	}
	Con_Printf("coop: %s has no animation for %s here: the scene's event is left out\n", entity, action);
}

void Coop_ClientLevelStart(void) {
	/* the level's start sets up the camera of view 0 as the single player's (Fake's clip planes, its credits); a
	 * client has none before its own character came */
	if (g_Play && cp.world && Play_ViewCamera(0.0f, 0.0f, 0.0f)) {
		Con_Printf("coop: the camera of view 0 for the level's start\n");
	}
}

byte Coop_ClientNoScene(void) {
	return g_Play && cp.world && !cp.replay;
}

/* the scene's callback of the scripts is the instance's (Fake's chain would load the next level here) */
static void Cp_ClientNoCallback(void) {
	nativeVal_t none;
	Native_SetNone(&none);
	if (!Mode_SetModVar("Scene", "EndOfSceneCallback", &none)) {
		Native_ErrPrint();
	}
}

/* the scene's table run and the scene played here */
static void Cp_ClientScene(NetRepReader *r) {
	static char text[CP_SCENE_MSG];
	static const char *lines[CP_SCENE_LINES + 1];
	char module[CP_NAME], cls[CP_NAME];
	NetRep_GetStr(r, module, sizeof(module));
	NetRep_GetStr(r, cls, sizeof(cls));
	int n = (int)NetRep_GetU16(r), at = 0;
	for (int k = 0; k < n && k < CP_SCENE_LINES && !r->bad; k++) {
		int len = (int)NetRep_GetU16(r);
		if (at + len + 1 > (int)sizeof(text)) {
			r->bad = 1;
			break;
		}
		CoordMsg_GetBytes(r, text + at, len);
		text[at + len] = 0;
		lines[k] = text + at;
		at += len + 1;
		lines[k + 1] = NULL;
	}
	char dmod[CP_NAME], dsec[CP_NAME];
	NetRep_GetStr(r, dmod, sizeof(dmod));
	NetRep_GetStr(r, dsec, sizeof(dsec));
	if (r->bad || !strcmp(cls, cp.clientScene)) {
		return;
	}
	/* a class the client has already (made by its level's start, or a scene of the level's own data) plays as it
	 * is: its table again would add its actions a second time (a fatal check) */
	nativeVal_t sname, have;
	char text2[CP_NAME + 8];
	Str_PrintfQuiet(text2, sizeof(text2), "Scene%s", cls);
	Native_SetText(&sname, text2, (int)strlen(text2));
	Native_SetNone(&have);
	byte got = Mode_SActV("GetClass", &sname, &have) && Native_True(&have);
	Native_Free(&have);
	Native_ErrClear();
	/* a scene of a level data section: the client runs that section */
	if (!got && dsec[0]) {
		if (!Script_LevelSection(dmod, dsec)) {
			Native_ErrPrint();
		}
		Native_SetNone(&have);
		got = Mode_SActV("GetClass", &sname, &have) && Native_True(&have);
		Native_Free(&have);
		Native_ErrClear();
		Con_Printf("coop: scene %s from the data section %s of %s: %s\n", cls, dsec, dmod, got ? "loaded" : "no class");
	}
	Native_Free(&sname);
	if (!n && !got) {
		Con_Printf("coop: scene %s of the instance: no class here\n", cls);
		return;
	}
	if (got) {
		n = 0;
	}
	Con_Printf("coop: scene %s of the instance plays here\n", cls);
	/* line by line, a failing one does not stop the scene. The "@" calls of the level's module run too: an event of
	 * theirs that forces an action whose animation the client's model lacks (the instance's mission gave it, Orbit's
	 * desk functionary) is left out (cActCtrl_PlayAction) */
	for (int k = 0; k < n && lines[k]; k++) {
		const char *one[2] = { lines[k], NULL };
		if (!Mode_Calls(module, one)) {
			Native_ErrPrint();
		}
	}
	cp.replay = 1;
	byte ok = Mode_Import("Scene") && PORT_CALL(Scene, Play, NULL, "s", cls);
	cp.replay = 0;
	if (!ok) {
		Native_ErrPrint();
		return;
	}
	Cp_ClientNoCallback();
	Str_Copy(cp.clientScene, cls, sizeof(cp.clientScene));
	cp.skipped = 0;
	cp.sceneAt = Watch_ShownTime();
}

/* the answer to the instance's question: the menu closes here, the instance runs the function of the answer */
static void Cp_ClientAnswer(byte yes) {
	uint8_t buf[4] = { COOP_MSG, COOP_C_ANSWER, COOP_MENU_YESNO, yes };
	if (cp.cmenu != COOP_MENU_YESNO) {
		return;
	}
	cp.cmenu = 0;
	if (!PORT_CALL(Menu, BackToGame, NULL, "is", 0L, "")) {
		Native_ErrPrint();
	}
	if (Watch_SendReliable(buf, sizeof(buf))) {
		Con_Printf("coop: the answer %s to the instance\n", yes ? "yes" : "no");
	}
}

static void Cp_MenuYes(cAbstractFile *params) {
	(void)params;
	Cp_ClientAnswer(1);
}

static void Cp_MenuNo(cAbstractFile *params) {
	(void)params;
	Cp_ClientAnswer(0);
}

/* a library of the level's scripts imported here: a mission's the client's own level start did not load (its story
 * is the instance's), as the level start preloads it */
static byte Cp_ClientImport(const char *mod) {
	char dir[COORD_PATH], pyc[CP_NAME + 8];
	nativeVal_t path;
	if (Mode_Import(mod)) {
		return 1;
	}
	Native_ErrClear();
	if (!g_pApp->levelPath) {
		return 0;
	}
	Str_PrintfQuiet(dir, sizeof(dir), "%s/Scripts", g_pApp->levelPath);
	Str_PrintfQuiet(pyc, sizeof(pyc), "%s.pyc", mod);
	Native_SetStr(&path, dir);
	byte ok = Mode_Pack(&path) && PORT_CALL(Scrap, PreloadLibrary, NULL, "ss", mod, pyc);
	Native_Free(&path);
	Mode_Pack(NULL);
	return ok && Mode_Import(mod);
}

/* a menu of the instance's scripts for this member */
static void Cp_ClientMenu(NetRepReader *r) {
	static cPy_NativeFunc natives[2];
	static char title[CP_MENU_TITLE + 1];
	char via[CP_NAME], mod[CP_NAME];
	int kind = (int)NetRep_GetU8(r);
	if (!natives[0].name) {
		cPy_NativeFunc_ctor(&natives[0], "CoopMenuYes", Cp_MenuYes);
		cPy_NativeFunc_ctor(&natives[1], "CoopMenuNo", Cp_MenuNo);
	}
	if (kind == COOP_MENU_YESNO) {
		int n = (int)NetRep_GetU16(r);
		if (n > CP_MENU_TITLE) {
			return;
		}
		CoordMsg_GetBytes(r, title, n);
		title[n] = 0;
		if (r->bad || !Mode_Import("Menu")) {
			return;
		}
		/* as the scripts open it in the single player, the answers to the instance */
		if (!PORT_CALL(Menu, InitMenuSys, NULL, "i", 0L) ||
			!PORT_CALL(Menu, YesNoMenu, NULL, "isss", 0L, title, "!CoopMenuYes", "!CoopMenuNo")) {
			Native_ErrPrint();
			return;
		}
		Script_SetNum(0, "Cursor", "Visible", 0);
		Script_SetDefault(0, "Yes");
		cp.cmenu = COOP_MENU_YESNO;
		cp.cmenuAt = Watch_ShownTime();
		Con_Printf("coop: the instance asks: %s\n", title);
		return;
	}
	if (kind != COOP_MENU_SHIPEDIT) {
		return;
	}
	NetRep_GetStr(r, via, sizeof(via));
	int money = (int)NetRep_GetU32(r);
	if (r->bad) {
		return;
	}
	Cp_GetVars(r, 1);
	g_pWorld->money = money;
	if (cp.testEdit) {
		/* test: the hangar's spare ship is the member's (SCRAP_COOP_SHIPEDIT) */
		SaveVar_Set(CP_HANGAR "HangarShipAux", CP_LIFEBOAT);
		SaveVar_Set(CP_HANGAR "iHangarShip", "9");
	}
	/* the ship of the client's own level start goes (CreatePlayerShip keeps the old one on the platform) */
	nativeVal_t none;
	Native_SetNone(&none);
	if (!Mode_Import("HangarTab") || !Mode_SetModVar("HangarTab", "prevShip", &none)) {
		Native_ErrPrint();
	}
	/* the editor of the hangar the instance has: the ship on the platform again, then the function of the answer
	 * that opened it there (its tutorial), else the keypad's */
	const char *dot = strchr(via, '.');
	byte ok = Junkyard_PlatformShip(), done = 0;
	if (ok && dot && dot - via < CP_NAME) {
		Str_PrintfQuiet(mod, sizeof(mod), "%.*s", (int)(dot - via), via);
		done = Cp_ClientImport(mod) && Native_CallMod(mod, dot + 1, NULL, "is", 0L, "");
		/* else the plain editor at least */
		Native_ErrPrint();
	}
	if (ok && !done) {
		ok = Mode_Import("MapShipEdit") && Native_CallMod("MapShipEdit", "EnterEditShip", NULL, "ssss", "", "", "", "");
	}
	if (!ok) {
		Native_ErrPrint();
	}
	cp.cmenu = COOP_MENU_SHIPEDIT;
	cp.cmenuAt = Watch_ShownTime();
	Con_Printf("coop: the ship editor of the instance%s%s, money %d\n", via[0] ? " with " : "", via, money);
}

byte Coop_ClientKeeps(cEntity *e) {
	return cp.cmenu == COOP_MENU_SHIPEDIT && Entity_IsType(e, TYEN_CAR);
}

void Coop_ClientShipEdited(void) {
	static uint8_t buf[CP_MENU_MSG];
	NetRepBuf w;
	if (!g_Play || cp.cmenu != COOP_MENU_SHIPEDIT) {
		return;
	}
	cp.cmenu = 0;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_C_ANSWER);
	NetRep_PutU8(&w, COOP_MENU_SHIPEDIT);
	NetRep_PutU32(&w, (uint32_t)g_pWorld->money);
	Cp_PutVars(&w, 0);
	if (!w.bad && Watch_SendReliable(buf, w.size)) {
		Con_Printf("coop: the hangar to the instance (ship %s, money %d)\n", SaveVar_Get(CP_HANGAR "iHangarShip") ?
			SaveVar_Get(CP_HANGAR "iHangarShip") : "-", g_pWorld->money);
	}
}

static void Cp_ClientOptions(NetRepReader *r) {
	char ent[CP_NAME];
	NetRep_GetStr(r, ent, sizeof(ent));
	int cur = (int)NetRep_GetU8(r), n = (int)NetRep_GetU8(r);
	if (n > COOP_OPTS) {
		return;
	}
	for (int k = 0; k < n; k++) {
		NetRep_GetStr(r, cp.copt.opt[k], COOP_OPT_TEXT);
	}
	if (r->bad) {
		return;
	}
	Str_Copy(cp.copt.ent, ent, sizeof(cp.copt.ent));
	cp.coop = 1;
	cp.copt.n = n;
	cp.copt.cur = cur < n ? cur : 0;
	if (n) {
		Con_Printf("coop: %d options of %s, %d: %s\n", n, ent, cp.copt.cur, cp.copt.opt[cp.copt.cur]);
	}
}

/* another option of the instance's list: shown here and told to the instance */
static void Cp_ClientChoose(int k) {
	uint8_t buf[CP_NAME + 8];
	NetRepBuf w;
	cp.copt.cur = k;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_C_CHOICE);
	NetRep_PutStr(&w, cp.copt.ent);
	NetRep_PutU8(&w, (uint32_t)k);
	if (!w.bad && Watch_SendReliable(buf, w.size)) {
		Con_Printf("coop: option %d of %s chosen: %s\n", k, cp.copt.ent, cp.copt.opt[k]);
	}
}

const char *Coop_ClientOption(int *count) {
	if (!Coop_Client() || !cp.copt.n || cp.clientScene[0]) {
		return NULL;
	}
	*count = cp.copt.n;
	return cp.copt.opt[cp.copt.cur];
}

/* the keys that pick another option of the instance's list (the talk panel shows it) */
static void Cp_ClientOptionsFrame(void) {
	static float prevNext, prevPrev;
	int n = cp.copt.n;
	if (!n || cp.clientScene[0]) {
		return;
	}
	float next = Input_BoundValue(ACTSET_WALK, WALK_NEXT_ACTION, 0), prev = Input_BoundValue(ACTSET_WALK, WALK_PREV_ACTION, 0);
	if (n > 1 && next > 0.5f && prevNext <= 0.5f) {
		Cp_ClientChoose((cp.copt.cur + 1) % n);
	} else if (n > 1 && prev > 0.5f && prevPrev <= 0.5f) {
		Cp_ClientChoose((cp.copt.cur + n - 1) % n);
	}
	prevNext = next;
	prevPrev = prev;
}

/* the key that takes this member to its party's leader */
static void Cp_ClientLeaderKey(void) {
	static byte was;
	byte down = Input_BoundValue(ACTSET_WALK, WALK_COOP_LEADER, 0) > 0.5f;
	if (down && !was && cp.coop) {
		uint8_t buf[2] = { COOP_MSG, COOP_C_LEADER };
		if (Watch_SendReliable(buf, sizeof(buf))) {
			Con_Printf("coop: to the party's leader\n");
			Texts_Format(cp.notice, sizeof(cp.notice), "MP_CoopToLeader", "");
			Text_Message(COOP_NOTICE_MS, "%s", cp.notice);
		}
	}
	was = down;
}

void Coop_ClientSceneAbort(void) {
	cp.aborting = 1;
}

void Coop_ClientSceneEnded(void) {
	byte esc = cp.aborting;
	cp.aborting = 0;
	if (!cp.clientScene[0] || cp.skipped) {
		return;
	}
	/* the scene plays on for the others; this member gets its character back with the instance's end */
	cp.skipped = 1;
	if (!esc) {
		Con_Printf("coop: scene %s ended here before the instance's end\n", cp.clientScene);
		return;
	}
	Con_Printf("coop: scene %s skipped here, it plays on in the instance\n", cp.clientScene);
	Texts_Format(cp.notice, sizeof(cp.notice), "MP_CoopSceneSkipped", "");
	Text_Message(COOP_NOTICE_MS, "%s", cp.notice);
}

void Coop_ClientFrame(void) {
	static double skipAfter = -1.0, editSecs, answerSecs;
	static int choose = -2, answer;
	static const char *chooseOf = "";
	if (skipAfter < 0.0) {
		const char *s = getenv("SCRAP_COOP_SKIP"), *c = getenv("SCRAP_COOP_CHOOSE"), *a = getenv("SCRAP_COOP_ANSWER");
		const char *e = getenv("SCRAP_COOP_SHIPEDIT");
		answer = !a ? -1 : !crt_strncmp(a, "yes", 3) ? 1 : !crt_strncmp(a, "no", 2) ? 0 : -1;
		answerSecs = a && strchr(a, ':') ? atof(strchr(a, ':') + 1) : CP_ANSWER_SECS;
		editSecs = e ? atof(e) : 0.0;
		skipAfter = s ? atof(s) : 0.0;
		choose = c ? atoi(c) : -1;
		chooseOf = c && strchr(c, ':') ? strchr(c, ':') + 1 : "";
	}
	if (cp.pendingSize) {
		NetRepReader r;
		NetRepReader_Init(&r, cp.pending + 2, cp.pendingSize - 2);
		cp.pendingSize = 0;
		Cp_ClientScene(&r);
	}
	Cp_ClientOptionsFrame();
	Cp_ClientLeaderKey();
	/* test (SCRAP_COOP_CHOOSE=<k>[:<entity prefix>]): option k of each list of more options than k */
	if (choose >= 0 && cp.copt.n > choose && cp.copt.cur != choose && !cp.clientScene[0] &&
		!strncmp(cp.copt.ent, chooseOf, strlen(chooseOf))) {
		Cp_ClientChoose(choose);
	}
	/* tests: a question's answer, the ship editor closed with the hangar's spare ship */
	if (answer >= 0 && cp.cmenu == COOP_MENU_YESNO && Watch_ShownTime() - cp.cmenuAt >= answerSecs) {
		Cp_ClientAnswer((byte)answer);
	}
	cp.testEdit = editSecs > 0.0;
	if (editSecs > 0.0 && cp.cmenu == COOP_MENU_SHIPEDIT && Watch_ShownTime() - cp.cmenuAt >= editSecs) {
		Con_Printf("coop: test: the ship editor closes\n");
		if (!PORT_CALL(ShipEdit, ExitEditShipMenu, NULL, "is", 0L, "Exit")) {
			Native_ErrPrint();
		}
		if (cp.cmenu == COOP_MENU_SHIPEDIT) {
			Con_Printf("coop: test: the ship editor did not close\n");
			cp.cmenu = 0;
		}
	}
	/* test: Escape the given seconds into each scene of the instance */
	if (skipAfter > 0.0 && cp.clientScene[0] && !cp.skipped && Watch_ShownTime() - cp.sceneAt >= skipAfter &&
		(!Mode_Import("Scene") || !PORT_CALL(Scene, AbortScene, NULL, "i", 0L))) {
		Native_ErrPrint();
		cp.skipped = 1;
	}
}

static void Cp_ClientSceneEnd(NetRepReader *r) {
	char cls[CP_NAME], name[CP_NAME + 16];
	NetRep_GetStr(r, cls, sizeof(cls));
	if (r->bad || strcmp(cls, cp.clientScene)) {
		return;
	}
	cp.clientScene[0] = 0;
	if (cp.skipped) {
		Con_Printf("coop: scene %s ended in the instance\n", cls);
		return;
	}
	Str_PrintfQuiet(name, sizeof(name), "DM_Scene_%s", cls);
	Cp_ClientNoCallback();
	if (World_FindEntity(name) && !PORT_CALL(Scene, EndOfScene, NULL, "ssi", name, "", 0L)) {
		Native_ErrPrint();
	}
}

byte Coop_ClientMessage(const uint8_t *data, int size) {
	static char text[1201];
	char font[64], file[160], key[64], arg[96], line[256];
	NetRepReader r;

	if (size < 2 || data[0] != COOP_MSG) {
		return 0;
	}
	cp.world = 1;
	NetRepReader_Init(&r, data + 2, size - 2);
	switch (data[1]) {
	case COOP_Y_SCENE:
		/* played in the next frame of the running level (a member that arrives gets it before its level runs: its
		 * camera would be taken over by the level's start) */
		if (size <= (int)sizeof(cp.pending)) {
			memcpy(cp.pending, data, (size_t)size);
			cp.pendingSize = size;
		}
		break;
	case COOP_Y_SCENEEND: {
		/* a scene that waits is dropped by its own end only (a callback starts the next scene before the end of the
		 * last one goes out) */
		char cls[CP_NAME], pcls[CP_NAME];
		NetRepReader pr, er = r;
		NetRep_GetStr(&er, cls, sizeof(cls));
		NetRepReader_Init(&pr, cp.pending + 2, cp.pendingSize > 2 ? cp.pendingSize - 2 : 0);
		NetRep_GetStr(&pr, pcls, sizeof(pcls));
		NetRep_GetStr(&pr, pcls, sizeof(pcls));
		if (cp.pendingSize && !pr.bad && !er.bad && !strcmp(cls, pcls)) {
			cp.pendingSize = 0;
		}
		Cp_ClientSceneEnd(&r);
		break;
	}
	case COOP_Y_SPEECH:
	case COOP_Y_MSG: {
		int n = (int)NetRep_GetU16(&r);
		if (n >= (int)sizeof(text)) {
			break;
		}
		CoordMsg_GetBytes(&r, text, n);
		text[n] = 0;
		float t = (float)NetRep_GetU32(&r) / 1000.0f;
		if (data[1] == COOP_Y_MSG) {
			NetRep_GetStr(&r, font, sizeof(font));
			if (!r.bad) {
				Scorer_SetMsgText(text, t, font[0] ? font : NULL);
			}
			break;
		}
		int cr = (int)NetRep_GetU8(&r), cg = (int)NetRep_GetU8(&r), cb = (int)NetRep_GetU8(&r);
		if (!r.bad) {
			Scorer_SetSpeechText(text, t, cr, cg, cb);
		}
		break;
	}
	case COOP_Y_VOICE: {
		int ch = (int)NetRep_GetU8(&r);
		NetRep_GetStr(&r, file, sizeof(file));
		if (!r.bad && g_pSound) {
			SoundSys_OpenVoice(g_pSound, file, ch, 1);
			SoundSys_PlayVoice(g_pSound, ch);
		}
		break;
	}
	case COOP_Y_OPTS:
		Cp_ClientOptions(&r);
		break;
	case COOP_Y_MENU:
		Cp_ClientMenu(&r);
		break;
	case COOP_Y_END:
		/* the credits of the main menu, as the single player's end; the world is left */
		Con_Printf("coop: the story is finished: the credits\n");
		if (!Mode_GVarSetI("ShowCredits", 1)) {
			Native_ErrPrint();
		}
		/* not here in the middle of the instance's messages: the join ends in its next pass */
		cp.ended = 1;
		break;
	case COOP_Y_NOTICE:
		NetRep_GetStr(&r, key, sizeof(key));
		NetRep_GetStr(&r, arg, sizeof(arg));
		if (!r.bad) {
			Texts_Format(line, sizeof(line), key, arg);
			Con_Printf("coop: %s\n", line);
			Text_Message(COOP_NOTICE_MS, "%s", line);
		}
		break;
	}
	return 1;
}

byte Coop_ClientEnded(void) {
	byte e = cp.ended;
	cp.ended = 0;
	return e;
}

void Coop_ClientReset(byte left) {
	cp.world = cp.world && !left;
	cp.cmenu = 0;
	cp.clientScene[0] = 0;
	cp.pendingSize = 0;
	cp.coop = 0;
	cp.copt.n = 0;
	cp.skipped = 0;
}
