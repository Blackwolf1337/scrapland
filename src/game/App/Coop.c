/*
 * The coop story of a world: the party's save variables on the instances, the mission level changes as travel, the
 * cutscenes and conversations on the members' clients, see include/scrap/coop.h. Not in the original.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "scrap/ai.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/netrep.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/sound.h"
#include "scrap/text.h"
#include "scrap/texts.h"
#include "scrap/travel.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

#define CP_NAME 64					/* bytes of a name of the story (CoordKv's keys) */
#define CP_MSG 16384				/* bytes of one story message to the coordinator */
#define CP_SCENE_MSG 8192			/* bytes of a scene's table for the clients */
#define CP_SCENE_LINES 256
#define CP_GATHER 160.0f			/* units between the members placed next to the member of a scene */
#define CP_VOICES 4
#define COOP_CAM "CoopCam"

/* the variables of a player, not of the story (where it goes on, its character, money and alarm, its time) */
static const char *const cp_local[] = { "Map", "Spawn", "ComeFrom", "PrevMap", "Char", "AcBatteries", "Money", "Alarm",
	"GameplayTime", NULL };
/* the levels of the story alone: a level change into or out of one takes the party along */
static const char *const cp_chapter[] = { "Orbit", "Fake", "Junkyard", "AB", "ABIndoor", NULL };

#define CP_TABLES 4					/* call tables of one scene (its Load table and the actors' after it) */

typedef struct CpScene {
	char cls[CP_NAME];
	const char *module;
	int ntables;
	const char *const *lines[CP_TABLES];
} CpScene;

static struct {
	byte on;
	char story[COORD_PATH];			/* the file of -story */
	byte applying;					/* the coordinator's story goes in: not sent back */
	byte synced;					/* the whole story came since the coordinator connected */
	byte overflow;					/* more changes than the list holds: the whole story goes */
	byte levelSent;					/* the changes of the level's start went (COORD_MSG_STORYINIT) */
	int ndirty;
	char dirty[COOP_DIRTY][CP_NAME];
	int nscenes;
	CpScene scenes[COOP_SCENES];
	int loading;					/* the scene whose Load table ran last (its later tables join it), -1 */
	byte inScene;					/* a scene of the scripts plays */
	int scene;						/* its table (the clients play it), -1 none */
	int scenePlayer;				/* the member bound when it started */
	uint32_t serial;				/* scenes started so far */
	uint32_t endingSerial;			/* the one whose end runs (its callback may start the next) */
	int endingTable;
	char endingCls[CP_NAME], sceneCls[CP_NAME];
	char voice[CP_VOICES][128];		/* the file opened on each voice channel */
	char clientScene[CP_NAME];		/* client: the scene it plays for its instance */
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
		if (!strcmp(cp.dirty[i], name)) {
			return i;
		}
	}
	return -1;
}

void Coop_SaveVarChanged(const char *name) {
	if (!cp.on || cp.applying || !Cp_Story(name) || Cp_DirtyAt(name) >= 0) {
		return;
	}
	if (cp.ndirty == COOP_DIRTY) {
		cp.overflow = 1;
		return;
	}
	Str_Copy(cp.dirty[cp.ndirty++], name, CP_NAME);
}

void Coop_LevelStart(void) {
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

static void Cp_OutPut(CpOut *o, const char *name, const char *value) {
	int vn = value ? (int)strlen(value) : 0;
	if (vn > COORD_STORY_VALUE) {
		Con_Printf("coop: the value of %s is too long for the story (%d bytes)\n", name, vn);
		return;
	}
	if (o->w.size + 4 + (int)strlen(name) + vn > CP_MSG || o->count == 0xffff) {
		Cp_OutFlush(o);
	}
	NetRep_PutU8(&o->w, value ? COORD_OP_SET : COORD_OP_DEL);
	NetRep_PutStr(&o->w, name);
	if (value) {
		NetRep_PutU16(&o->w, (uint32_t)vn);
		CoordMsg_PutBytes(&o->w, value, vn);
	}
	o->count++;
}

static void Cp_PutAll(const char *name, const char *value, void *u) {
	if (Cp_Story(name)) {
		Cp_OutPut(u, name, value);
	}
}

/* the changes to the coordinator now (once the whole story came: before it the coordinator's would go over them) */
static void Cp_Send(void) {
	static CpOut o;
	if (!cp.synced || (!cp.ndirty && !cp.overflow)) {
		return;
	}
	Cp_OutStart(&o);
	if (cp.overflow) {
		SaveVars_Each(Cp_PutAll, &o);
	} else {
		for (int i = 0; i < cp.ndirty; i++) {
			Cp_OutPut(&o, cp.dirty[i], SaveVar_Get(cp.dirty[i]));
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

void Coop_Pass(void) {
	if (Coop_On()) {
		Cp_Send();
	}
}

void Coop_CoordLost(void) {
	cp.synced = 0;
}

/* a change of the coordinator into the save variables; a name changed here and not sent yet keeps its value */
static void Cp_Apply(int kind, const char *name, const char *value) {
	if (!Cp_Story(name) || Cp_DirtyAt(name) >= 0) {
		return;
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

byte Coop_QLoad(const char *map) {
	if (!Coop_On()) {
		return 0;
	}
	const char *s = strrchr(map, '/');
	const char *level = s ? s + 1 : map;
	if (!crt_stricmp(level, "Menu")) {
		Con_Printf("coop: the story ends (QLoad %s): not yet in coop\n", map);
		return 0;
	}
	if (!crt_stricmp(level, Instance_Level())) {
		Con_Printf("coop: the same level again (QLoad %s): not yet in coop\n", map);
		return 0;
	}
	/* the member of the scene whose end goes on (its callback), else the bound one */
	int i = cp.inScene && cp.scenePlayer >= 0 && Players_Get(cp.scenePlayer) ? cp.scenePlayer : Cp_Player();
	if (i < 0) {
		return 0;
	}
	const char *spawn = SaveVar_Get("Spawn");
	byte party = Cp_InList(cp_chapter, level) || Cp_InList(cp_chapter, Instance_Level());
	/* the story of now first: the target instance starts with it */
	Cp_Send();
	Con_Printf("coop: mission level change of player %d to %s (%s)%s\n", i, level, spawn ? spawn : "", party ?
		", the party goes along" : "");
	if (party) {
		Cp_Notice(-1, "MP_CoopTravel", level);
	}
	return Travel_Go(i, level, spawn ? spawn : "", "", party);
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
	int k = 0;
	while (k < cp.nscenes && strcmp(cp.scenes[k].cls, cls)) {
		k++;
	}
	if (k == COOP_SCENES) {
		return;
	}
	cp.nscenes += k == cp.nscenes;
	Str_Copy(cp.scenes[k].cls, cls, CP_NAME);
	cp.scenes[k].module = module;
	cp.scenes[k].ntables = 1;
	cp.scenes[k].lines[0] = lines;
	cp.loading = k;
}

void Coop_ScenePlay(const char *cls) {
	static uint8_t buf[CP_SCENE_MSG];
	NetRepBuf w;
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
	if (cp.scene < 0) {
		/* a scene of the level's own data (map.def): the clients have its class from their level's start */
		NetRepBuf_Init(&w, buf, sizeof(buf));
		NetRep_PutU8(&w, COOP_MSG);
		NetRep_PutU8(&w, COOP_Y_SCENE);
		NetRep_PutStr(&w, "");
		NetRep_PutStr(&w, cls);
		NetRep_PutU16(&w, 0);
		Cp_ToAll(buf, w.size);
		Con_Printf("coop: scene %s plays (no table: the clients play the class they have)\n", cls);
		return;
	}
	const CpScene *sc = &cp.scenes[cp.scene];
	int n = 0;
	for (int t = 0; t < sc->ntables; t++) {
		for (const char *const *l = sc->lines[t]; *l && n < CP_SCENE_LINES; l++) {
			n++;
		}
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_SCENE);
	NetRep_PutStr(&w, sc->module);
	NetRep_PutStr(&w, cls);
	NetRep_PutU16(&w, (uint32_t)n);
	int m = 0;
	for (int t = 0; t < sc->ntables; t++) {
		for (const char *const *l = sc->lines[t]; *l && m < n; l++, m++) {
			int len = (int)strlen(*l);
			NetRep_PutU16(&w, (uint32_t)len);
			CoordMsg_PutBytes(&w, *l, len);
		}
	}
	if (w.bad) {
		Con_Printf("coop: the table of scene %s does not fit a message\n", cls);
		cp.scene = -1;
		return;
	}
	Cp_ToAll(buf, w.size);
	Con_Printf("coop: scene %s plays on the clients (player %d, %d lines)\n", cls, cp.scenePlayer, n);
}

byte Coop_SceneHolds(void) {
	return Coop_On() && cp.inScene;
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

/* ---- tests ---- */

byte Coop_Command(const char *line) {
	char name[CP_NAME], val[256];
	if (!Coop_On() || strncmp(line, "story", 5)) {
		return 0;
	}
	if (sscanf(line, "story set %63s %255[^\n]", name, val) == 2) {
		SaveVar_Set(name, val);
		Con_Printf("coop: %s = %s\n", name, val);
	} else if (sscanf(line, "story get %63s", name) == 1) {
		const char *v = SaveVar_Get(name);
		Con_Printf("coop: %s %s%s\n", name, v ? "= " : "is not set", v ? v : "");
	} else {
		const char *lib = SaveVar_Get("Mission.Library"), *map = SaveVar_Get("Mission.Map");
		Con_Printf("coop: story %s, %d changes to send, mission %s in %s, scene %s\n", cp.synced ? "in sync" :
			"waiting for the coordinator", cp.ndirty, lib ? lib : "-", map ? map : "-", cp.inScene ? "playing" : "none");
	}
	return 1;
}

/* ================================ client ================================ */

void Coop_Cue(int kind, const char *text) {
	char key[64], line[256];
	if (kind == COORD_CUE_INTRO) {
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
	if (r->bad) {
		return;
	}
	if (!n) {
		nativeVal_t sname, have;
		char text2[CP_NAME + 8];
		Str_PrintfQuiet(text2, sizeof(text2), "Scene%s", cls);
		Native_SetText(&sname, text2, (int)strlen(text2));
		Native_SetNone(&have);
		byte got = Mode_SActV("GetClass", &sname, &have) && Native_True(&have);
		Native_Free(&sname);
		Native_Free(&have);
		Native_ErrClear();
		if (!got) {
			Con_Printf("coop: scene %s of the instance: no class here\n", cls);
			return;
		}
	}
	Con_Printf("coop: scene %s of the instance plays here\n", cls);
	/* line by line, a failing one does not stop the scene; the calls of the level's own module are left out (they
	 * work on entities the instance's mission made or changed: the client has them as they were) */
	for (int k = 0; k < n && lines[k]; k++) {
		const char *one[2] = { lines[k], NULL };
		if (lines[k][0] != '@' && !Mode_Calls(module, one)) {
			Native_ErrPrint();
		}
	}
	if (!Mode_Import("Scene") || !PORT_CALL(Scene, Play, NULL, "s", cls)) {
		Native_ErrPrint();
		return;
	}
	Str_Copy(cp.clientScene, cls, sizeof(cp.clientScene));
}

static void Cp_ClientSceneEnd(NetRepReader *r) {
	char cls[CP_NAME], name[CP_NAME + 16];
	NetRep_GetStr(r, cls, sizeof(cls));
	if (r->bad || strcmp(cls, cp.clientScene)) {
		return;
	}
	cp.clientScene[0] = 0;
	Str_PrintfQuiet(name, sizeof(name), "DM_Scene_%s", cls);
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
	NetRepReader_Init(&r, data + 2, size - 2);
	switch (data[1]) {
	case COOP_Y_SCENE:
		Cp_ClientScene(&r);
		break;
	case COOP_Y_SCENEEND:
		Cp_ClientSceneEnd(&r);
		break;
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

void Coop_ClientReset(void) {
	cp.clientScene[0] = 0;
}
