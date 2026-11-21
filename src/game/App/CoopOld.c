/*
 * The coop story of the old path (without -mp2), what the second design does elsewhere: the conversations' texts,
 * voices and heads, the mission's HUD items and targets, the arrow options, the cutscenes on the clients, the
 * scripts' menus, the ship editor, Sputnik's hangar and the conversor's interface on a member's client, see
 * include/scrap/coop.h. Under -mp2 pres.h, the UI channel (UiChan.c), Speech.c and NetScene.c do it; these stay
 * quiet there. Deleted with the old path (stage 10). Not in the original.
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
#include "scrap/uichan.h"
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

#define COOP_CAM "CoopCam"
#define CP_DEAL_SECS Party_RuleNum("dealsecs", 8.0)	/* the super deal's scores before their Continue */


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
	/* the world's own view 0 (the scripts of the world's context link it: a talk, a scene) */
	cWorld_SetCamOwn(g_pWorld, 0, COOP_CAM);
}


/* the entity player e would use: the one it talks to, else the one in front of it with a use arrow */
static cEntity *Cp_UseTarget(cEntity *e) {
	cEntity *t = WChar(e)->listenOn ? WChar_Listen(e) : NULL;
	if (!t) {
		t = WChar_Victim(e);
	}
	return t && Entity_IsType(t, TYEN_WITH_LIFE) && WLife(t)->arrow.canUse ? t : NULL;
}

/* the options message of the arrow of t (NULL: none) into w: their count */
int Cp_OptionsMsg(NetRepBuf *w, cEntity *t) {
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
void Cp_TellOptions(void) {
	uint8_t buf[CP_OPTS_MSG];
	NetRepBuf w;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = Players_Get(i);
		if (Rep_PlayerConn(i) < 0) {
			g_Coop.pl[i].sentSize = 0;
			continue;
		}
		cEntity *t = e && NetPl_Kind(e) == NETREP_CHAR ? Cp_UseTarget(e) : NULL;
		NetRepBuf_Init(&w, buf, sizeof(buf));
		int n = Cp_OptionsMsg(&w, t);
		if (w.bad || (w.size == g_Coop.pl[i].sentSize && !memcmp(buf, g_Coop.pl[i].sent, (size_t)w.size))) {
			continue;
		}
		/* nothing to tell a member that was told nothing */
		if (!n && !g_Coop.pl[i].sentSize) {
			continue;
		}
		memcpy(g_Coop.pl[i].sent, buf, (size_t)w.size);
		g_Coop.pl[i].sentSize = n ? w.size : 0;
		Cp_ToClient(i, buf, w.size);
		Con_Printf("coop: player %d: %d options of %s (%d)\n", i, n, n ? t->name : "-", n ? WLife(t)->arrow.iOpt : 0);
	}
}

/* ---- key names ---- */

#define CP_KEY_OPEN "{{"
#define CP_KEY_CLOSE "}}"

const char *Coop_KeyToken(const char *set, const char *action, const char *device) {
	static char token[3 * CP_NAME];
	if (!Coop_On() || !set || !action) {
		return NULL;
	}
	Str_PrintfQuiet(token, sizeof(token), CP_KEY_OPEN "%s|%s|%s" CP_KEY_CLOSE, set, action, device ? device : "");
	return token;
}

void Coop_ClientKeys(char *text, int size) {
	char out[1201], part[3][CP_NAME];
	int len = 0;
	const char *p = text;
	if (!strstr(text, CP_KEY_OPEN)) {
		return;
	}
	while (*p && len < (int)sizeof(out) - 1) {
		const char *end = !strncmp(p, CP_KEY_OPEN, 2) ? strstr(p, CP_KEY_CLOSE) : NULL;
		int n = 0;
		if (end) {
			const char *f = p + 2;
			for (n = 0; n < 3 && f <= end; n++) {
				int k = (int)strcspn(f, "|}");
				k = f + k > end ? (int)(end - f) : k;
				Str_PrintfQuiet(part[n], CP_NAME, "%.*s", k, f);
				f += k + 1;
			}
		}
		if (n == 3) {
			const char *key = Input_GetDefinedEntry(0, part[0], part[1], part[2], 0);
			len += Str_PrintfQuiet(out + len, (int)sizeof(out) - len, "%s", key ? key : "");
			len = len < (int)sizeof(out) ? len : (int)sizeof(out) - 1;
			p = end + 2;
		} else {
			out[len++] = *p++;
		}
	}
	out[len] = 0;
	Str_Copy(text, out, size);
}

/* ---- the mission's items of the HUD ---- */

/* the variables of an item a client's copy gets (the ones its class has; the functions stay the instance's) */
static const char *const cp_scorerVars[] = { "X", "Y", "W", "H", "Visible", "Text", "Font", "Align", "Effect",
	"CentralText", "Red", "Green", "Blue", "Alpha", "Value", "MaxValue", "MainBarIndex", "BackIndex", "BlackBarIndex",
	"Blink", "MainRed", "MainGreen", "MainBlue", "MainAlpha", "BackRed", "BackGreen", "BackBlue", "BackAlpha",
	"BlackRed", "BlackGreen", "BlackBlue", "BlackAlpha", "File", "SpriteIndex", "ScaleX", "ScaleY", "BlendingMode",
	NULL };
/* the classes that are copied (a talk, a radar, a menu's item are the client's own) */
static const char *const cp_scorerTypes[] = { "Text", "Label", "Timer", "Bar", "Sprite", NULL };
/* the engine variables of the four timers and their direction */
const char *const g_CoopTimerVars[] = { "TimeToCancelCurrentMission", "TimeToFinishCurrentMission",
	"TimeInactiveTimer", "TimeSpecialUseTimer", "CounterType", NULL };
static const char *const cp_timerTypes[] = { "TimeToCancel", "TimeToFinish", "TimeInactive", "TimeSpecialUse" };

void Coop_ScorerAdded(const char *name, const char *type) {
	if (!Coop_On() || !name || (int)strlen(name) >= CP_NAME) {
		return;
	}
	int k = 0;
	while (k < g_Coop.nstype && strcmp(g_Coop.stype[k].name, name)) {
		k++;
	}
	if (k == g_Coop.nstype) {
		if (k == CP_STYPES) {
			return;
		}
		g_Coop.nstype++;
	}
	Str_Copy(g_Coop.stype[k].name, name, CP_NAME);
	Str_Copy(g_Coop.stype[k].type, type, sizeof(g_Coop.stype[k].type));
}

void Coop_ScorerBase(int id) {
	cScorer *sc = Coop_On() && !id ? Scorer_Get(0) : NULL;
	if (!sc) {
		return;
	}
	g_Coop.nsbase = 0;
	for (cScorerItem *it = sc->items; it && g_Coop.nsbase < CP_STYPES; it = it->next) {
		if (!it->delMe && it->name) {
			Str_Copy(g_Coop.sbase[g_Coop.nsbase++], it->name, CP_NAME);
		}
	}
}

static const char *Cp_ScorerType(const char *name) {
	for (int k = 0; k < g_Coop.nstype; k++) {
		if (!strcmp(g_Coop.stype[k].name, name)) {
			return Cp_InList(cp_scorerTypes, g_Coop.stype[k].type) ? g_Coop.stype[k].type : NULL;
		}
	}
	return NULL;
}

/* the variables of item it into w: their count first */
static void Cp_PutScorerVars(NetRepBuf *w, cScorerItem *it) {
	char val[256];
	int at = w->size, n = 0;
	NetRep_PutU8(w, 0);
	int tt = cTimerScorer_Type(it);
	if (tt >= 0 && tt < 4) {
		NetRep_PutStr(w, "TimerType");
		NetRep_PutU8(w, 1);
		NetRep_PutStr(w, cp_timerTypes[tt]);
		n++;
	}
	for (int k = 0; cp_scorerVars[k]; k++) {
		char *str = NULL;
		double d = 0.0;
		int i = 0, kind = ScorerItem_Var(it, cp_scorerVars[k], 0, &str, &d, &i);
		if (kind == 1) {
			Str_Copy(val, str ? str : "", sizeof(val));
		} else if (kind == 2) {
			Str_PrintfQuiet(val, sizeof(val), "%.3f", d);
		} else if (kind == 3) {
			Str_PrintfQuiet(val, sizeof(val), "%d", i);
		} else {
			continue;
		}
		NetRep_PutStr(w, cp_scorerVars[k]);
		NetRep_PutU8(w, (uint32_t)kind);
		NetRep_PutStr(w, val);
		n++;
	}
	if (!w->bad) {
		w->p[at] = (uint8_t)n;
	}
}

/* the items of the instance's HUD that are no standard ones (the mission's: the race's scores, its timers) to the
 * clients here when they changed */
void Cp_TellScorer(void) {
	static uint8_t buf[CP_SCORER_MSG];
	uint64_t now = (uint64_t)(Instance_Time() * 1000.0);
	cScorer *sc = Scorer_Get(0);
	if (!sc || now - g_Coop.slookAt < COOP_SCORER_MS) {
		return;
	}
	g_Coop.slookAt = now;
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_SCORER);
	for (int k = 0; g_CoopTimerVars[k]; k++) {
		char v[32];
		nativeVal_t t;
		Native_SetNone(&t);
		if (Native_GVarGet(g_CoopTimerVars[k], &t) && (t.type == NV_INT || t.type == NV_FLOAT)) {
			Str_PrintfQuiet(v, sizeof(v), "%.3f", t.type == NV_INT ? (double)t.i : t.f);
		} else {
			v[0] = 0;
		}
		Native_ErrClear();
		Native_Free(&t);
		NetRep_PutStr(&w, v);
	}
	int at = w.size, n = 0;
	NetRep_PutU8(&w, 0);
	for (cScorerItem *it = sc->items; it && n < COOP_SCORER_ITEMS && !w.bad; it = it->next) {
		const char *type = it->delMe || !it->name ? NULL : Cp_ScorerType(it->name);
		byte base = 0;
		for (int k = 0; type && k < g_Coop.nsbase && !base; k++) {
			base = !strcmp(g_Coop.sbase[k], it->name);
		}
		if (!type || base) {
			continue;
		}
		NetRep_PutStr(&w, it->name);
		NetRep_PutStr(&w, type);
		Cp_PutScorerVars(&w, it);
		n++;
	}
	if (w.bad) {
		return;
	}
	buf[at] = (uint8_t)n;
	if (w.size == g_Coop.ssentSize && !memcmp(buf, g_Coop.ssent, (size_t)w.size) && now - g_Coop.ssentAt < COOP_TARGETS_MS) {
		return;
	}
	if (n != g_Coop.nsitems) {
		Con_Printf("coop: the mission's items of the HUD: %d\n", n);
		g_Coop.nsitems = n;
	}
	memcpy(g_Coop.ssent, buf, (size_t)w.size);
	g_Coop.ssentSize = w.size;
	g_Coop.ssentAt = now;
	Cp_ToAll(buf, w.size);
}

/* the mission's targets to the clients here when they changed (or every COOP_TARGETS_MS: a proxy of one came into a
 * client's interest since) */
void Cp_TellTargets(void) {
	uint8_t buf[16 + COOP_TARGETS * (CP_NAME + 4)];
	char sig[sizeof(g_Coop.targets)];
	const char *names[COOP_TARGETS];
	nativeVal_t cur, list, it[COOP_TARGETS];
	NetRepBuf w;
	int n = 0, len = 0;
	byte listed = 0;
	Native_SetNone(&cur);
	Native_SetNone(&list);
	if (Mode_ModGet("MissionsFuncs", "currentTarget", &cur) && cur.type == NV_STR && Native_Text(&cur)[0]) {
		names[n++] = Native_Text(&cur);
	} else if (Mode_ModGet("MissionsFuncs", "targetsList", &list)) {
		listed = 1;
		int k = Mode_Loop(&list);
		for (int j = 0; j < k && n < COOP_TARGETS; j++) {
			Native_SetNone(&it[n]);
			if (Native_SeqItem(&list, j, &it[n]) && it[n].type == NV_STR) {
				names[n] = Native_Text(&it[n]);
				n++;
			} else {
				Native_Free(&it[n]);
			}
		}
	}
	Native_ErrClear();
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_TARGETS);
	NetRep_PutU8(&w, (uint32_t)n);
	sig[0] = 0;
	for (int k = 0; k < n; k++) {
		int slot = Rep_EntitySlot(World_FindEntity(names[k]));
		NetRep_PutStr(&w, names[k]);
		NetRep_PutU16(&w, slot < 0 ? NETREP_NO_SLOT : (uint32_t)slot);
		len += Str_PrintfQuiet(sig + len, (int)sizeof(sig) - len, "%s:%d,", names[k], slot);
		len = len < (int)sizeof(sig) ? len : (int)sizeof(sig) - 1;
	}
	uint64_t now = (uint64_t)(Instance_Time() * 1000.0);
	if (!w.bad && (strcmp(sig, g_Coop.targets) || now - g_Coop.targetsAt >= COOP_TARGETS_MS)) {
		if (strcmp(sig, g_Coop.targets)) {
			Con_Printf("coop: the mission's targets: %s\n", sig[0] ? sig : "none");
		}
		Str_Copy(g_Coop.targets, sig, sizeof(g_Coop.targets));
		g_Coop.targetsAt = now;
		Cp_ToAll(buf, w.size);
	}
	for (int k = 0; listed && k < n; k++) {
		Native_Free(&it[k]);
	}
	Native_Free(&cur);
	Native_Free(&list);
}

void Coop_BeforeUse(int i, cEntity *t) {
	if (!Coop_On() || i < 0 || i >= PLAYERS_MAX || !t || !Entity_IsType(t, TYEN_WITH_LIFE)) {
		return;
	}
	cArrowIndicator *a = &WLife(t)->arrow;
	if (g_Coop.pl[i].choiceEnt[0] && !strcmp(g_Coop.pl[i].choiceEnt, t->name) && g_Coop.pl[i].choice < a->nOpt) {
		a->iOpt = g_Coop.pl[i].choice;
	}
	/* the scripts move the arrow on after a use: the next choice starts from there */
	g_Coop.pl[i].choiceEnt[0] = 0;
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
	while (k < g_Coop.nscenes && strcmp(g_Coop.scenes[k].cls, cls)) {
		k++;
	}
	if (k == COOP_SCENES) {
		return NULL;
	}
	g_Coop.nscenes += k == g_Coop.nscenes;
	Str_Copy(g_Coop.scenes[k].cls, cls, CP_NAME);
	return &g_Coop.scenes[k];
}

void Coop_SceneTable(const char *module, const char *const *lines) {
	static const char head[] = "Scene.Load|";
	char cls[CP_NAME];
	if (!Coop_On() || !lines || !lines[0]) {
		return;
	}
	if (strncmp(lines[0], head, sizeof(head) - 1)) {
		CpScene *sc = g_Coop.loading >= 0 ? &g_Coop.scenes[g_Coop.loading] : NULL;
		if (sc && sc->module == module && sc->ntables < CP_TABLES && Cp_SceneLines(lines)) {
			sc->lines[sc->ntables++] = lines;
		} else {
			g_Coop.loading = -1;
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
	g_Coop.loading = (int)(sc - g_Coop.scenes);
}

void Coop_DataSection(const char *module, const char *section) {
	if (!Coop_On()) {
		return;
	}
	Str_Copy(g_Coop.dataModule, module ? module : "", CP_NAME);
	Str_Copy(g_Coop.dataSection, section ? section : "", CP_NAME);
}

void Coop_SceneLoad(const char *cls) {
	CpScene *sc;
	/* only the load of a data section (a call table's load is kept by Coop_SceneTable) */
	if (!Coop_On() || !g_Coop.dataSection[0] || !(sc = Cp_SceneSlot(cls))) {
		return;
	}
	sc->module = "";
	sc->ntables = 0;
	Str_Copy(sc->dataModule, g_Coop.dataModule, CP_NAME);
	Str_Copy(sc->dataSection, g_Coop.dataSection, CP_NAME);
	Con_Printf("coop: scene %s loads from the data section %s of %s\n", cls, g_Coop.dataSection, g_Coop.dataModule);
}

/* the message of the scene that plays (its table, or the class alone of a scene of the level's own data); 0 when it
 * does not fit */
int Cp_SceneMsg(uint8_t *buf, int size) {
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, size);
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_SCENE);
	if (g_Coop.scene < 0) {
		NetRep_PutStr(&w, "");
		NetRep_PutStr(&w, g_Coop.sceneCls);
		NetRep_PutU16(&w, 0);
		NetRep_PutStr(&w, "");
		NetRep_PutStr(&w, "");
		return w.size;
	}
	const CpScene *sc = &g_Coop.scenes[g_Coop.scene];
	int n = 0;
	for (int t = 0; t < sc->ntables; t++) {
		for (const char *const *l = sc->lines[t]; *l && n < CP_SCENE_LINES; l++) {
			n++;
		}
	}
	NetRep_PutStr(&w, sc->module);
	NetRep_PutStr(&w, g_Coop.sceneCls);
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
	g_Coop.inScene = 1;
	Input_SelectActionSetNamed("");
	g_Coop.serial++;
	g_Coop.scenePlayer = Cp_Player();
	g_Coop.scene = -1;
	Str_Copy(g_Coop.sceneCls, cls, sizeof(g_Coop.sceneCls));
	for (int k = 0; k < g_Coop.nscenes; k++) {
		if (!strcmp(g_Coop.scenes[k].cls, cls)) {
			g_Coop.scene = k;
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
		g_Coop.scene = -1;
		size = Cp_SceneMsg(buf, sizeof(buf));
	}
	/* a -mp2 instance: the scene record of include/scrap/netscene.h instead */
	if (!g_Mp2) {
		Cp_ToAll(buf, size);
	}
	if (g_Coop.scene < 0) {
		/* a scene of the level's own data (map.def): the clients have its class from their level's start */
		Con_Printf("coop: scene %s plays (no table: the clients play the class they have)\n", cls);
	} else {
		Con_Printf("coop: scene %s plays on the clients (player %d)\n", cls, g_Coop.scenePlayer);
	}
}

/* a scene the clients play themselves makes its own texts and voices */
/* the texts, voices and heads of the conversations; on -mp2 they go by include/scrap/pres.h */
static byte Cp_Quiet(void) {
	return !Coop_On() || g_Coop.inScene || g_Mp2;
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
	/* the speaker's client shows the conversation; a remote communication the mission's member and the members near it
	 * */
	int i = Players_Acting();
	Con_Printf("coop: %s for player %d: %.160s\n", g_Coop.remote ? "remote communication" : "speech",
		i == PLAYERS_NONE ? -1 : i, text);
	if (!g_Coop.remote && i != PLAYERS_NONE) {
		g_Coop.pl[i].talkUntil = Instance_Time() + time;
	}
	if (g_Coop.remote && i != PLAYERS_NONE) {
		Cp_ToNear(i, buf, w.size);
	} else if (i != PLAYERS_NONE && Rep_PlayerConn(i) >= 0) {
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

void Coop_Fade(const int *from, const int *to, double time) {
	uint8_t buf[16];
	NetRepBuf w;
	if (!Coop_On() || g_Mp2) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_FADE);
	for (int k = 0; k < 4; k++) {
		NetRep_PutU8(&w, (uint32_t)(from[k] & 255));
	}
	for (int k = 0; k < 4; k++) {
		NetRep_PutU8(&w, (uint32_t)(to[k] & 255));
	}
	NetRep_PutU32(&w, (uint32_t)(time > 0.0 ? time * 1000.0 : 0.0));
	Cp_ToAll(buf, w.size);
}

void Coop_OpenVoice(const char *file, int channel) {
	if (Coop_On() && channel >= 0 && channel < CP_VOICES) {
		Str_Copy(g_Coop.voice[channel], file, sizeof(g_Coop.voice[channel]));
	}
}

void Coop_PlayVoice(int channel) {
	uint8_t buf[192];
	NetRepBuf w;
	if (Cp_Quiet() || channel < 0 || channel >= CP_VOICES || !g_Coop.voice[channel][0]) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_VOICE);
	NetRep_PutU8(&w, (uint32_t)channel);
	NetRep_PutStr(&w, g_Coop.voice[channel]);
	/* the speaker and the members near it hear it */
	Cp_ToNear(Players_Acting(), buf, w.size);
}

void Coop_RemoteCom(byte on) {
	if (!Coop_On() || (!on && !g_Coop.remote)) {
		return;
	}
	/* nested (the voice's call around the text's): the outer one binds and gives the binding back */
	if (on && g_Coop.remote++) {
		return;
	}
	if (on) {
		int i = Cp_MissionPlayer();
		g_Coop.remoteWas = i >= 0 ? Players_Enter(i) : Players_Current();
	} else if (!--g_Coop.remote) {
		Players_Leave(g_Coop.remoteWas);
	}
}

void Coop_RemoteHead(const char *model, const char *text, int index) {
	uint8_t buf[512];
	NetRepBuf w;
	if (Cp_Quiet() || !g_Coop.remote) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_HEAD);
	NetRep_PutStr(&w, model);
	NetRep_PutStr(&w, text);
	NetRep_PutU8(&w, (uint32_t)(index & 255));
	if (!w.bad) {
		Cp_ToNear(Players_Acting(), buf, w.size);
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

void Cp_PutVars(NetRepBuf *w, byte act) {
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
void Cp_GetVars(NetRepReader *r, byte act) {
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

void Coop_PutHangar(NetRepBuf *w) {
	Cp_PutVars(w, 0);
}

void Coop_GetHangar(NetRepReader *r) {
	Cp_GetVars(r, 0);
}

byte Coop_ShipEditAway(void) {
	/* -mp2: the editor went to the client by the UI channel (its tutorial runs there, not on the instance) */
	if (Ui_Open(UI_K_SHIPEDIT)) {
		return 1;
	}
	int i = Coop_On() ? Cp_Player() : -1;
	return i >= 0 && g_Coop.pl[i].menu == COOP_MENU_SHIPEDIT;
}

const char *Coop_ActionSet(const char *name) {
	return !Coop_On() ? name : g_Coop.inScene ? "Inactive" : "Walk";
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
	g_Coop.pl[i].menu = COOP_MENU_YESNO;
	Str_Copy(g_Coop.pl[i].yes, yes, CP_NAME);
	Str_Copy(g_Coop.pl[i].no, no, CP_NAME);
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: player %d is asked (%s or %s)\n", i, yes, no);
	return 1;
}

byte Coop_DealScores(int deal, const char *title, const char *yes, const char *no) {
	char mod[CP_NAME];
	if (!Coop_On()) {
		return 0;
	}
	if (deal == 1) {
		return Coop_MenuYesNo(title, yes, no);
	}
	/* Continue: its action after the members read the table on their HUDs */
	Str_Copy(mod, yes, sizeof(mod));
	char *dot = strchr(mod, '.');
	if (!dot) {
		return 0;
	}
	*dot = 0;
	/* the arguments of a menu's action (the player, the control), as Cp_MenuFunc gives them */
	nativeVal_t args[2];
	Native_SetInt(&args[0], 0);
	Native_SetText(&args[1], "", 0);
	byte ok = Mode_SchedAt(Mode_Time() + CP_DEAL_SECS, mod, dot + 1, args, 2, NULL);
	Native_Free(&args[1]);
	if (!ok) {
		Native_ErrPrint();
		return 0;
	}
	Con_Printf("coop: the super deal's scores, then %s in %.0f s\n", yes, CP_DEAL_SECS);
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
	NetRep_PutStr(&w, g_Coop.via);
	NetRep_PutU32(&w, (uint32_t)Players_Money(i));
	Cp_PutVars(&w, 1);
	if (w.bad) {
		Con_Printf("coop: the hangar of player %d does not fit a message\n", i);
		return 0;
	}
	g_Coop.pl[i].menu = COOP_MENU_SHIPEDIT;
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: the ship editor opens on the client of player %d%s%s\n", i, g_Coop.via[0] ? " with " : "", g_Coop.via);
	return 1;
}

/* "Module.Func"(0, "") for player i: the function its answer chose */
void Cp_MenuFunc(int i, const char *fn) {
	char mod[CP_NAME];
	const char *dot = strchr(fn, '.');
	if (!dot || dot - fn >= CP_NAME) {
		return;
	}
	Str_PrintfQuiet(mod, sizeof(mod), "%.*s", (int)(dot - fn), fn);
	int was = Players_Enter(i);
	Str_Copy(g_Coop.via, fn, sizeof(g_Coop.via));
	if (!Mode_Import(mod) || !Native_CallMod(mod, dot + 1, NULL, "is", 0L, "")) {
		Native_ErrPrint();
	}
	g_Coop.via[0] = 0;
	Players_Leave(was);
}

/* the current player's money and hangar from its client into the story (a menu of the hangar closed there): its
 * money, -1 for a bad message */
static int Cp_HangarBack(NetRepReader *r) {
	int money = (int)NetRep_GetU32(r);
	if (r->bad) {
		return -1;
	}
	Cp_GetVars(r, 0);
	*Players_CurMoney() = money;
	return money;
}

/* the function a module's variable holds (a menu's callback of the scripts) with the menu's id 0 and control "" when
 * args; then None in it with clear */
static void Cp_MenuCallback(const char *mod, const char *var, byte args, byte clear) {
	nativeVal_t cb, a[2];
	Native_SetNone(&cb);
	if (Mode_ModGet(mod, var, &cb) && cb.type != NV_NONE) {
		Native_SetInt(&a[0], 0);
		Native_SetStr(&a[1], "");
		if (clear) {
			nativeVal_t none;
			Native_SetNone(&none);
			Mode_SetModVar(mod, var, &none);
		}
		if (!Mode_CallArgs(&cb, NULL, args ? a : NULL, args ? 2 : 0)) {
			Native_ErrPrint();
		}
		Native_Free(&a[1]);
	}
	Native_ErrClear();
	Native_Free(&cb);
}

/* player i closed the ship editor: its hangar into the story, its money, the ship on the platform as the editor's end
 * leaves it, then the editor's callback of the scripts */
void Cp_ShipEdited(int i, NetRepReader *r) {
	int was = Players_Enter(i);
	int money = Cp_HangarBack(r);
	if (money < 0) {
		Players_Leave(was);
		return;
	}
	if (!Junkyard_PlatformShip()) {
		Native_ErrPrint();
	}
	const char *cur = SaveVar_Get(CP_HANGAR "iHangarShip");
	Con_Printf("coop: player %d closed the ship editor (hangar %s, money %d)\n", i, cur ? cur : "-", money);
	Cp_MenuCallback("ShipEdit", "EndShipEditCallBack", 1, 0);
	Players_Leave(was);
}

byte Coop_Warning(const char *text) {
	uint8_t buf[CP_MENU_TITLE + 16];
	NetRepBuf w;
	if (!Coop_On()) {
		return 0;
	}
	int i = Cp_Player(), n = (int)strlen(text);
	if (i < 0 || Rep_PlayerConn(i) < 0) {
		return 1;
	}
	n = n > CP_MENU_TITLE ? CP_MENU_TITLE : n;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MENU);
	NetRep_PutU8(&w, COOP_MENU_WARNING);
	NetRep_PutU16(&w, (uint32_t)n);
	CoordMsg_PutBytes(&w, text, n);
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: a text box for player %d: %.60s\n", i, text);
	return 1;
}

byte Coop_SputnikOpen(void) {
	static uint8_t buf[CP_MENU_MSG];
	NetRepBuf w;
	nativeVal_t spt;
	Native_SetNone(&spt);
	/* the member that used the party's Sputnik (its interface's events may run in another one's context) */
	int user = Coop_On() && Mode_ModGet("Sputnik", "spt", &spt) && spt.type == NV_ENT ? Sputniks_CoopUser(Native_Ent(&spt)) : -1;
	Native_Free(&spt);
	Native_ErrClear();
	int i = user >= 0 ? user : Coop_On() ? Cp_Player() : -1;
	if (i < 0 || Rep_PlayerConn(i) < 0) {
		return 0;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MENU);
	NetRep_PutU8(&w, COOP_MENU_SPUTNIK);
	NetRep_PutU32(&w, (uint32_t)Players_Money(i));
	Cp_PutVars(&w, 1);
	if (w.bad) {
		return 0;
	}
	g_Coop.pl[i].menu = COOP_MENU_SPUTNIK;
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: Sputnik's hangar screen opens on the client of player %d\n", i);
	return 1;
}

/* player i closed Sputnik's screen: its hangar and money, then the callback of the scripts (once) */
void Cp_SputnikDone(int i, NetRepReader *r) {
	int was = Players_Enter(i);
	int money = Cp_HangarBack(r);
	if (money < 0) {
		Players_Leave(was);
		return;
	}
	const char *cur = SaveVar_Get(CP_HANGAR "iHangarShip");
	Con_Printf("coop: player %d closed Sputnik's screen (hangar %s, money %d)\n", i, cur ? cur : "-", money);
	Cp_MenuCallback("SputnikInterface", "EndSputnikCallBack", 0, 1);
	Players_Leave(was);
}

/* the machine of the member's conversor runs (1) or waits for its pick (0) */
static void Cp_ConvMachine(byte run) {
	cEntity *m = World_FindEntity(g_Coop.convMachine);
	double speed = run ? 1.0 : 0.0;
	if (m) {
		Entity_VarF(m, "TimeSpeed", &speed, 1);
	}
}

void Coop_ConvEnter(void) {
	nativeVal_t c;
	int i = Coop_On() ? Players_Acting() : PLAYERS_NONE;
	if (i == PLAYERS_NONE) {
		return;
	}
	Native_SetNone(&c);
	if (Mode_ModGet("CharConversor", "_charName", &c) && c.type == NV_STR) {
		g_Coop.conv = i;
		g_Coop.convAsked = 0;
		Str_Copy(g_Coop.convMachine, Native_Text(&c), sizeof(g_Coop.convMachine));
		Con_Printf("coop: player %d enters the conversor %s\n", i, g_Coop.convMachine);
	}
	Native_ErrClear();
	Native_Free(&c);
}

byte Coop_ConvMenu(void) {
	static uint8_t buf[CP_MENU_MSG];
	NetRepBuf w;
	int i = Coop_On() ? g_Coop.conv : -1;
	if (i < 0) {
		return 0;
	}
	if (Rep_PlayerConn(i) < 0) {
		return 1;
	}
	const char *avl = SaveVar_Get("Conversor.AvailableChars");
	cEntity *e = Players_Get(i);
	cActCtrl *a = e && Entity_IsType(e, TYEN_WITH_LIFE) ? WLife(e)->actCtrl : NULL;
	int n = avl ? (int)strlen(avl) : 0;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MENU);
	NetRep_PutU8(&w, COOP_MENU_CONV);
	NetRep_PutStr(&w, a && a->cls ? a->cls->name : "Dtritus");
	NetRep_PutU16(&w, (uint32_t)n);
	CoordMsg_PutBytes(&w, avl ? avl : "", n);
	if (w.bad) {
		return 1;
	}
	/* the menu's bookkeeping of the single player (the mode and time to go back to), the machine waits */
	int was = Players_Enter(i);
	if (!Mode_Import("Menu") || !PORT_CALL(Menu, InitMenuSys, NULL, "i", 0L)) {
		Native_ErrPrint();
	}
	Players_Leave(was);
	g_Coop.pl[i].menu = COOP_MENU_CONV;
	g_Coop.convAsked = 1;
	Cp_ConvMachine(0);
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: the conversor's interface opens on the client of player %d\n", i);
	return 1;
}

/* player i picked a class in the conversor (cls "": none), as the single player's Convert and Escape run */
void Cp_ConvPicked(int i, const char *cls) {
	char k[CP_NAME + 8];
	nativeVal_t v;
	int was = Players_Enter(i);
	g_Coop.convAsked = 0;
	Cp_ConvMachine(1);
	Con_Printf("coop: player %d picks %s in the conversor\n", i, cls[0] ? cls : "nothing");
	Str_PrintfQuiet(k, sizeof(k), "Char_%s", cls);
	Native_SetStr(&v, k);
	byte ok = Mode_Import("CharConversor") && (cls[0] ? Mode_SetModVar("CharConversor", "KONTROL", &v) &&
		PORT_CALL(CharConversor, ConvertExecute, NULL, "") : PORT_CALL(CharConversor, ExitCharConversor, NULL, "is", 0L, ""));
	if (!ok) {
		Native_ErrPrint();
	}
	Native_Free(&v);
	Players_Leave(was);
}

void Coop_ConvEnd(void) {
	/* the member's context for the rest of the machine's end (the dispatch point around it leaves it again) */
	if (Coop_On() && g_Coop.conv >= 0) {
		Players_Enter(g_Coop.conv);
		g_Coop.conv = -1;
	}
}
