/*
 * The coop story on a member's client of the old path (without -mp2) and the notices of the world: the story's
 * copy, the cutscenes, menus, options, HUD items and targets the instance sends, see include/scrap/coop.h. Not in
 * the original.
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

#define CP_ANSWER_SECS 2.0			/* test: a question's answer after this */
#define CP_CONV_SECS 3.0			/* test: the conversor's pick after this */
#define CP_LIFEBOAT "SLifeBoat<-<-<-<-<-0,0,0,0,0,0,1<-50<-0,0,0,0,0,0"	/* the hangar's spare ship (a test's ship) */


/* ================================ client ================================ */

/* the video the next level load plays (a headless client plays none) */
static void Cp_ClientVideo(const char *file) {
	if (!g_HeadlessPlay && !Mode_GVarSetS("PlayVideo", file)) {
		Native_ErrPrint();
	}
}

/* the notices that name a level get its station's name in the client's language */
static const char *Cp_NoticeArg(const char *key, const char *arg) {
	return !strcmp(key, "MP_CoopTravel") || !strcmp(key, "MP_CoopScene") ? Places_LevelText(arg) : arg;
}

void Coop_Cue(int kind, const char *text) {
	char key[64], line[256];
	if (kind == COORD_CUE_COOP) {
		g_Coop.world = 1;
	} else if (kind == COORD_CUE_INTRO) {
		Con_Printf("coop: a new story: the intro %s\n", text);
		Cp_ClientVideo(text);
	} else if (kind == COORD_CUE_NOTICE) {
		const char *bar = strchr(text, '|');
		int n = bar ? (int)(bar - text) : (int)strlen(text);
		Str_PrintfQuiet(key, sizeof(key), "%.*s", n, text);
		Texts_Format(line, sizeof(line), key, Cp_NoticeArg(key, bar ? bar + 1 : ""));
		Con_Printf("coop: %s\n", line);
		Text_Message(COOP_NOTICE_MS, "%s", line);
	}
}

byte Coop_Client(void) {
	return g_Play && g_Coop.world;
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
	if (g_Play && g_Coop.world && Play_ViewCamera(0.0f, 0.0f, 0.0f)) {
		Con_Printf("coop: the camera of view 0 for the level's start\n");
	}
}

byte Coop_ClientNoScene(void) {
	return g_Play && g_Coop.world && !g_Coop.replay;
}

/* the scene's callback of the scripts is the instance's (Fake's chain would load the next level here) */
static void Cp_ClientNoCallback(void) {
	nativeVal_t none;
	Native_SetNone(&none);
	if (!Mode_SetModVar("Scene", "EndOfSceneCallback", &none)) {
		Native_ErrPrint();
	}
}

static byte Cp_ClientImport(const char *mod);

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
	if (r->bad || !strcmp(cls, g_Coop.clientScene)) {
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
	/* the events of the scene call the functions of its module (a mission's the client's level start did not load) */
	if ((dmod[0] && !Cp_ClientImport(dmod)) || (module[0] && !Cp_ClientImport(module))) {
		Native_ErrPrint();
	}
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
	g_Coop.replay = 1;
	byte ok = Mode_Import("Scene") && PORT_CALL(Scene, Play, NULL, "s", cls);
	g_Coop.replay = 0;
	if (!ok) {
		Native_ErrPrint();
		return;
	}
	Cp_ClientNoCallback();
	Str_Copy(g_Coop.clientScene, cls, sizeof(g_Coop.clientScene));
	g_Coop.skipped = 0;
	g_Coop.sceneAt = Watch_ShownTime();
}

/* the answer to the instance's question: the menu closes here, the instance runs the function of the answer */
static void Cp_ClientAnswer(byte yes) {
	uint8_t buf[4] = { COOP_MSG, COOP_C_ANSWER, COOP_MENU_YESNO, yes };
	if (g_Coop.cmenu != COOP_MENU_YESNO) {
		return;
	}
	g_Coop.cmenu = 0;
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

/* the conversor's interface of the instance: the member's class and the classes the story has, as the single
 * player's machine opens it */
static void Cp_ClientConvOpen(NetRepReader *r) {
	static char avl[COORD_STORY_VALUE + 1];
	char cls[CP_NAME];
	nativeVal_t text, list;
	NetRep_GetStr(r, cls, sizeof(cls));
	int n = (int)NetRep_GetU16(r);
	if (r->bad || n > COORD_STORY_VALUE) {
		return;
	}
	CoordMsg_GetBytes(r, avl, n);
	avl[n] = 0;
	if (r->bad) {
		return;
	}
	SaveVar_Set("Char", cls);
	SaveVar_Set("Conversor.AvailableChars", avl);
	Native_SetStr(&text, avl);
	Native_SetNone(&list);
	if (!Mode_Import("CharConversor") || !Native_EvalText("CharConversor", &text, &list) ||
		!Mode_SetModVar("CharConversor", "AvailableChars", &list)) {
		Native_ErrPrint();
	}
	Native_Free(&text);
	Native_Free(&list);
	g_Coop.cmenu = COOP_MENU_CONV;
	g_Coop.cmenuAt = Watch_ShownTime();
	if (!PORT_CALL(CharConversor, AddInterfaceContent, NULL, "i", 0L)) {
		Native_ErrPrint();
	}
	Con_Printf("coop: the conversor's interface of the instance (as %s)\n", cls);
}

byte Coop_ClientConverted(const char *cls) {
	uint8_t buf[8 + CP_NAME];
	NetRepBuf w;
	if (!g_Play || g_Coop.cmenu != COOP_MENU_CONV) {
		return 0;
	}
	g_Coop.cmenu = 0;
	if (!PORT_CALL(Menu, BackToGame, NULL, "is", 0L, "")) {
		Native_ErrPrint();
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_C_ANSWER);
	NetRep_PutU8(&w, COOP_MENU_CONV);
	NetRep_PutStr(&w, cls);
	if (!w.bad && Watch_SendReliable(buf, w.size)) {
		Con_Printf("coop: the conversor's pick %s to the instance\n", cls[0] ? cls : "(none)");
	}
	return 1;
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
		g_Coop.cmenu = COOP_MENU_YESNO;
		g_Coop.cmenuAt = Watch_ShownTime();
		Con_Printf("coop: the instance asks: %s\n", title);
		return;
	}
	if (kind == COOP_MENU_CONV) {
		Cp_ClientConvOpen(r);
		return;
	}
	if (kind == COOP_MENU_WARNING) {
		int n = (int)NetRep_GetU16(r);
		if (n > CP_MENU_TITLE) {
			return;
		}
		CoordMsg_GetBytes(r, title, n);
		title[n] = 0;
		Coop_ClientKeys(title, (int)sizeof(title));
		/* the box of the single player (it waits until the member plays: its entry calls it again) */
		if (!r->bad && (!Mode_Import("Menu") || !PORT_CALL(Menu, ShowWarning, NULL, "is", 0L, title))) {
			Native_ErrPrint();
		}
		g_Coop.cmenu = COOP_MENU_WARNING;
		g_Coop.cmenuAt = Watch_ShownTime();
		Con_Printf("coop: a text box of the instance: %.60s\n", title);
		return;
	}
	if (kind == COOP_MENU_SPUTNIK) {
		int m = (int)NetRep_GetU32(r);
		if (r->bad) {
			return;
		}
		Cp_GetVars(r, 1);
		*Players_CurMoney() = m;
		g_Coop.cmenu = COOP_MENU_SPUTNIK;
		g_Coop.cmenuAt = Watch_ShownTime();
		if (!Mode_Import("SputnikInterface") || !PORT_CALL(SputnikInterface, EnterSputnikInterface, NULL, "i", 0L)) {
			Native_ErrPrint();
		}
		Con_Printf("coop: Sputnik's hangar screen of the instance, money %d\n", m);
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
	*Players_CurMoney() = money;
	if (g_Coop.testEdit) {
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
	g_Coop.cmenu = COOP_MENU_SHIPEDIT;
	g_Coop.cmenuAt = Watch_ShownTime();
	Con_Printf("coop: the ship editor of the instance%s%s, money %d\n", via[0] ? " with " : "", via, money);
}

byte Coop_ClientKeeps(cEntity *e) {
	return g_Coop.cmenu == COOP_MENU_SHIPEDIT && Entity_IsType(e, TYEN_CAR);
}

/* the money and the hangar to the instance: the menu of kind closed here */
static void Cp_ClientHangarBack(int kind) {
	static uint8_t buf[CP_MENU_MSG];
	NetRepBuf w;
	if (!g_Play || g_Coop.cmenu != kind) {
		return;
	}
	g_Coop.cmenu = 0;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_C_ANSWER);
	NetRep_PutU8(&w, (uint32_t)kind);
	NetRep_PutU32(&w, (uint32_t)*Players_CurMoney());
	Cp_PutVars(&w, 0);
	if (!w.bad && Watch_SendReliable(buf, w.size)) {
		Con_Printf("coop: the hangar to the instance (ship %s, money %d)\n", SaveVar_Get(CP_HANGAR "iHangarShip") ?
			SaveVar_Get(CP_HANGAR "iHangarShip") : "-", *Players_CurMoney());
	}
}

void Coop_ClientShipEdited(void) {
	Cp_ClientHangarBack(COOP_MENU_SHIPEDIT);
}

void Coop_ClientSputnikDone(void) {
	Cp_ClientHangarBack(COOP_MENU_SPUTNIK);
}

static void Cp_ClientOptions(NetRepReader *r) {
	char ent[CP_NAME];
	NetRep_GetStr(r, ent, sizeof(ent));
	int cur = (int)NetRep_GetU8(r), n = (int)NetRep_GetU8(r);
	if (n > COOP_OPTS) {
		return;
	}
	for (int k = 0; k < n; k++) {
		NetRep_GetStr(r, g_Coop.copt.opt[k], COOP_OPT_TEXT);
	}
	if (r->bad) {
		return;
	}
	Str_Copy(g_Coop.copt.ent, ent, sizeof(g_Coop.copt.ent));
	g_Coop.coop = 1;
	g_Coop.copt.n = n;
	g_Coop.copt.cur = cur < n ? cur : 0;
	if (n) {
		Con_Printf("coop: %d options of %s, %d: %s\n", n, ent, g_Coop.copt.cur, g_Coop.copt.opt[g_Coop.copt.cur]);
	}
}

/* another option of the instance's list: shown here and told to the instance */
static void Cp_ClientChoose(int k) {
	uint8_t buf[CP_NAME + 8];
	NetRepBuf w;
	g_Coop.copt.cur = k;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_C_CHOICE);
	NetRep_PutStr(&w, g_Coop.copt.ent);
	NetRep_PutU8(&w, (uint32_t)k);
	if (!w.bad && Watch_SendReliable(buf, w.size)) {
		Con_Printf("coop: option %d of %s chosen: %s\n", k, g_Coop.copt.ent, g_Coop.copt.opt[k]);
	}
}

const char *Coop_ClientOption(int *count) {
	if (!Coop_Client() || !g_Coop.copt.n || g_Coop.clientScene[0]) {
		return NULL;
	}
	*count = g_Coop.copt.n;
	return g_Coop.copt.opt[g_Coop.copt.cur];
}

/* the keys that pick another option of the instance's list (the talk panel shows it) */
static void Cp_ClientOptionsFrame(void) {
	static float prevNext, prevPrev;
	int n = g_Coop.copt.n;
	if (!n || g_Coop.clientScene[0]) {
		return;
	}
	float next = Input_BoundValue(ACTSET_WALK, WALK_NEXT_ACTION, 0), prev = Input_BoundValue(ACTSET_WALK, WALK_PREV_ACTION, 0);
	if (n > 1 && next > 0.5f && prevNext <= 0.5f) {
		Cp_ClientChoose((g_Coop.copt.cur + 1) % n);
	} else if (n > 1 && prev > 0.5f && prevPrev <= 0.5f) {
		Cp_ClientChoose((g_Coop.copt.cur + n - 1) % n);
	}
	prevNext = next;
	prevPrev = prev;
}

/* the key that takes this member to its party's leader */
static void Cp_ClientLeaderKey(void) {
	static byte was;
	byte down = Input_BoundValue(ACTSET_WALK, WALK_COOP_LEADER, 0) > 0.5f;
	if (down && !was && g_Coop.coop) {
		uint8_t buf[2] = { COOP_MSG, COOP_C_LEADER };
		if (Watch_SendReliable(buf, sizeof(buf))) {
			Con_Printf("coop: to the party's leader\n");
			Texts_Format(g_Coop.notice, sizeof(g_Coop.notice), "MP_CoopToLeader", "");
			Text_Message(COOP_NOTICE_MS, "%s", g_Coop.notice);
		}
	}
	was = down;
}

void Coop_ClientSceneAbort(void) {
	g_Coop.aborting = 1;
}

void Coop_ClientSceneEnded(void) {
	byte esc = g_Coop.aborting;
	g_Coop.aborting = 0;
	if (!g_Coop.clientScene[0] || g_Coop.skipped) {
		return;
	}
	/* the scene plays on for the others; this member gets its character back with the instance's end */
	g_Coop.skipped = 1;
	if (!esc) {
		Con_Printf("coop: scene %s ended here before the instance's end\n", g_Coop.clientScene);
		return;
	}
	Con_Printf("coop: scene %s skipped here, it plays on in the instance\n", g_Coop.clientScene);
	Texts_Format(g_Coop.notice, sizeof(g_Coop.notice), "MP_CoopSceneSkipped", "");
	Text_Message(COOP_NOTICE_MS, "%s", g_Coop.notice);
}

/* the instance's mission items of the HUD on this client's HUD (made again when its HUD was drawn anew); the items
 * it made that the instance no longer has go */
static void Cp_ClientScorer(void) {
	char names[COOP_SCORER_ITEMS][CP_NAME], name[CP_NAME], type[16], var[CP_NAME], val[256];
	cScorer *sc = Scorer_Get(0);
	if (!sc || g_Coop.cscoreSize < 2) {
		return;
	}
	NetRepReader r;
	NetRepReader_Init(&r, g_Coop.cscore + 2, g_Coop.cscoreSize - 2);
	for (int k = 0; g_CoopTimerVars[k]; k++) {
		nativeVal_t t;
		NetRep_GetStr(&r, val, sizeof(val));
		if (!r.bad && val[0]) {
			if (k == 4) {
				Native_SetInt(&t, atol(val));
			} else {
				Native_SetFloat(&t, atof(val));
			}
			if (!Native_GVarSet(g_CoopTimerVars[k], &t)) {
				Native_ErrPrint();
			}
		}
	}
	int n = (int)NetRep_GetU8(&r), m = 0;
	for (int j = 0; j < n && j < COOP_SCORER_ITEMS && !r.bad; j++) {
		NetRep_GetStr(&r, name, sizeof(name));
		NetRep_GetStr(&r, type, sizeof(type));
		int nv = (int)NetRep_GetU8(&r);
		cScorerItem *it = r.bad ? NULL : cScorer_FindItem(sc, name);
		if (!r.bad && !it) {
			it = cScorer_AddItem(sc, name, type, 1);
		}
		for (int k = 0; k < nv && !r.bad; k++) {
			NetRep_GetStr(&r, var, sizeof(var));
			int kind = (int)NetRep_GetU8(&r);
			NetRep_GetStr(&r, val, sizeof(val));
			char *str = val;
			double d = atof(val);
			int i = atoi(val);
			if (it && !r.bad) {
				ScorerItem_Var(it, var, 1, kind == 1 ? &str : NULL, kind == 2 ? &d : NULL, kind == 3 ? &i : NULL);
			}
		}
		if (it) {
			Str_Copy(names[m++], name, CP_NAME);
		}
	}
	for (int k = 0; k < g_Coop.ncnames; k++) {
		byte keep = 0;
		for (int j = 0; j < m && !keep; j++) {
			keep = !strcmp(names[j], g_Coop.cnames[k]);
		}
		cScorerItem *it = keep ? NULL : cScorer_FindItem(sc, g_Coop.cnames[k]);
		if (it) {
			it->delMe = 1;
		}
	}
	if (m != g_Coop.ncnames) {
		Con_Printf("coop: the instance's mission items on the HUD here: %d%s%s\n", m, m ? ", " : "", m ? names[0] : "");
	}
	memcpy(g_Coop.cnames, names, sizeof(names[0]) * (size_t)m);
	g_Coop.ncnames = m;
}

/* a variable of the client's copy of the story (value NULL: deleted) */
static void Cp_ClientStorySet(const char *name, const char *value) {
	if (!Cp_ClientStoryVar(name)) {
		return;
	}
	if (value) {
		SaveVar_Set(name, value);
	} else {
		SaveVar_Delete(name);
	}
}

void Coop_ClientStory(const char *text, int len) {
	static char key[CP_NAME], val[COORD_STORY_VALUE + 1];
	int count = 0;
	/* the client's own variables (a single player game before) are no part of it */
	SaveVars_Delete("");
	for (int pos = 0; CoordKv_Next(text, len, &pos, key, sizeof(key), val, sizeof(val)); count++) {
		Cp_ClientStorySet(key, val);
	}
	Con_Printf("coop: the party's story here, %d variables\n", count);
}

/* the client's HUD was drawn anew (a level, a ship): the instance's items again */
static void Cp_ClientScorerFrame(void) {
	cScorer *sc = Scorer_Get(0);
	for (int k = 0; sc && k < g_Coop.ncnames; k++) {
		if (!cScorer_FindItem(sc, g_Coop.cnames[k])) {
			Cp_ClientScorer();
			return;
		}
	}
}

/* the entities the instance's level start deleted go from the client's level too (its own ones, not the proxies of
 * the instance's) */
static void Cp_ClientGone(void) {
	char name[CP_NAME];
	NetRepReader r;
	NetRepReader_Init(&r, g_Coop.cgone + 2, g_Coop.cgoneSize - 2);
	g_Coop.cgoneSize = 0;
	int n = (int)NetRep_GetU16(&r), del = 0;
	for (int k = 0; k < n && !r.bad; k++) {
		NetRep_GetStr(&r, name, sizeof(name));
		cEntity *e = r.bad ? NULL : World_FindEntity(name);
		if (e && e != Players_Local() && !(Entity_IsType(e, TYEN_OBJECT) && Obj(e)->replica) && cEntity_Del(e)) {
			del++;
		}
	}
	Con_Printf("coop: %d of the %d entities the instance's level start deleted were here\n", del, n);
}

void Coop_ClientFrame(void) {
	static double skipAfter = -1.0, editSecs, answerSecs, convSecs, sputnikSecs, warnSecs;
	static int choose = -2, answer;
	static const char *chooseOf = "";
	static char conv[CP_NAME];
	static int convVisits;
	if (skipAfter < 0.0) {
		const char *s = getenv("SCRAP_COOP_SKIP"), *c = getenv("SCRAP_COOP_CHOOSE"), *a = getenv("SCRAP_COOP_ANSWER");
		const char *e = getenv("SCRAP_COOP_SHIPEDIT");
		answer = !a ? -1 : !crt_strncmp(a, "yes", 3) ? 1 : !crt_strncmp(a, "no", 2) ? 0 : -1;
		answerSecs = a && strchr(a, ':') ? atof(strchr(a, ':') + 1) : CP_ANSWER_SECS;
		editSecs = e ? atof(e) : 0.0;
		skipAfter = s ? atof(s) : 0.0;
		choose = c ? atoi(c) : -1;
		chooseOf = c && strchr(c, ':') ? strchr(c, ':') + 1 : "";
		const char *cv = getenv("SCRAP_COOP_CONV"), *sp = getenv("SCRAP_COOP_SPUTNIK");
		Str_PrintfQuiet(conv, sizeof(conv), "%.*s", cv ? (int)strcspn(cv, ":") : 0, cv ? cv : "");
		convSecs = cv && strchr(cv, ':') ? atof(strchr(cv, ':') + 1) : CP_CONV_SECS;
		sputnikSecs = sp ? atof(sp) : 0.0;
		const char *wn = getenv("SCRAP_COOP_WARNING");
		warnSecs = wn ? atof(wn) : 0.0;
	}
	if (g_Coop.pendingSize) {
		NetRepReader r;
		NetRepReader_Init(&r, g_Coop.pending + 2, g_Coop.pendingSize - 2);
		g_Coop.pendingSize = 0;
		Cp_ClientScene(&r);
	}
	if (g_Coop.cgoneSize) {
		Cp_ClientGone();
	}
	Cp_ClientOptionsFrame();
	Cp_ClientScorerFrame();
	Cp_ClientLeaderKey();
	/* test (SCRAP_COOP_CHOOSE=<k>[:<entity prefix>]): option k of each list of more options than k */
	if (choose >= 0 && g_Coop.copt.n > choose && g_Coop.copt.cur != choose && !g_Coop.clientScene[0] &&
		!strncmp(g_Coop.copt.ent, chooseOf, strlen(chooseOf))) {
		Cp_ClientChoose(choose);
	}
	/* tests: a question's answer, the ship editor closed with the hangar's spare ship */
	if (answer >= 0 && g_Coop.cmenu == COOP_MENU_YESNO && Watch_ShownTime() - g_Coop.cmenuAt >= answerSecs) {
		Cp_ClientAnswer((byte)answer);
	}
	g_Coop.testEdit = editSecs > 0.0;
	if (editSecs > 0.0 && g_Coop.cmenu == COOP_MENU_SHIPEDIT && Watch_ShownTime() - g_Coop.cmenuAt >= editSecs) {
		Con_Printf("coop: test: the ship editor closes\n");
		if (!PORT_CALL(ShipEdit, ExitEditShipMenu, NULL, "is", 0L, "Exit")) {
			Native_ErrPrint();
		}
		if (g_Coop.cmenu == COOP_MENU_SHIPEDIT) {
			Con_Printf("coop: test: the ship editor did not close\n");
			g_Coop.cmenu = 0;
		}
	}
	/* tests: the conversor's pick (as its button's Convert and ConvertExecute), Sputnik's screen closed; -mp2: the
	 * interface of the UI channel (include/scrap/uichan.h) */
	static double uiConvAt = -1.0;
	byte uiConv = Ui_ClientIn(UI_K_CONV);
	if (!uiConv) {
		uiConvAt = -1.0;
	} else if (uiConvAt < 0.0) {
		uiConvAt = Watch_ShownTime();
	}
	if (conv[0] && ((g_Coop.cmenu == COOP_MENU_CONV && Watch_ShownTime() - g_Coop.cmenuAt >= convSecs) ||
		(uiConv && Watch_ShownTime() - uiConvAt >= convSecs))) {
		/* a list "A,B,...": each visit the next class of it, the last one again after its end */
		char k[CP_NAME + 8], pick[CP_NAME];
		const char *at = conv;
		for (int n = 0; n < convVisits && strchr(at, ','); n++) {
			at = strchr(at, ',') + 1;
		}
		Str_PrintfQuiet(pick, sizeof(pick), "%.*s", (int)strcspn(at, ","), at);
		convVisits++;
		nativeVal_t v;
		Con_Printf("coop: test: the conversor's pick %s\n", pick);
		Str_PrintfQuiet(k, sizeof(k), "Char_%s", pick);
		Native_SetStr(&v, k);
		byte ok = strcmp(pick, "none") ? Mode_SetModVar("CharConversor", "KONTROL", &v) &&
			PORT_CALL(CharConversor, ConvertExecute, NULL, "") : PORT_CALL(CharConversor, ExitCharConversor, NULL, "is", 0L, "");
		if (!ok) {
			Native_ErrPrint();
		}
		Native_Free(&v);
		if (g_Coop.cmenu == COOP_MENU_CONV) {
			Con_Printf("coop: test: %s is no class of the conversor here\n", pick);
			Coop_ClientConverted("");
		} else if (Ui_ClientIn(UI_K_CONV)) {
			Con_Printf("coop: test: %s is no class of the conversor here\n", pick);
			Ui_ClientConv("");
		}
	}
	if (sputnikSecs > 0.0 && g_Coop.cmenu == COOP_MENU_SPUTNIK && Watch_ShownTime() - g_Coop.cmenuAt >= sputnikSecs) {
		Con_Printf("coop: test: Sputnik's screen closes\n");
		if (!PORT_CALL(SputnikInterface, ExitSputnikInterface, NULL, "is", 0L, "Exit")) {
			Native_ErrPrint();
		}
		if (g_Coop.cmenu == COOP_MENU_SPUTNIK) {
			Coop_ClientSputnikDone();
		}
	}
	if (warnSecs > 0.0 && g_Coop.cmenu == COOP_MENU_WARNING && Watch_ShownTime() - g_Coop.cmenuAt >= warnSecs) {
		Con_Printf("coop: test: the text box closes\n");
		g_Coop.cmenu = 0;
		if (!PORT_CALL(Menu, BackToGame, NULL, "is", 0L, "")) {
			Native_ErrPrint();
		}
	}
	/* test: Escape the given seconds into each scene of the instance */
	if (skipAfter > 0.0 && g_Coop.clientScene[0] && !g_Coop.skipped && Watch_ShownTime() - g_Coop.sceneAt >= skipAfter &&
		(!Mode_Import("Scene") || !PORT_CALL(Scene, AbortScene, NULL, "i", 0L))) {
		Native_ErrPrint();
		g_Coop.skipped = 1;
	}
}

/* the mission's targets of the instance on the client's copies (the level's entity of the name, the proxy of a
 * replicated one), as its MissionsFuncs sets them: the radar marks, the mission arrow */
static void Cp_ClientTargets(NetRepReader *r) {
	char name[CP_NAME], sig[sizeof(g_Coop.ctargets)];
	nativeVal_t list;
	int n = (int)NetRep_GetU8(r), len = 0, got = 0;
	sig[0] = 0;
	Native_SetEmpty(&list, NV_LIST);
	for (int k = 0; k < n && k < COOP_TARGETS && !r->bad; k++) {
		NetRep_GetStr(r, name, sizeof(name));
		int slot = (int)NetRep_GetU16(r);
		cEntity *e = r->bad ? NULL : slot != NETREP_NO_SLOT ? Watch_SlotEntity(slot) : World_FindEntity(name);
		if (!e) {
			continue;
		}
		Native_SetStr(Native_Append(&list), e->name);
		len += Str_PrintfQuiet(sig + len, (int)sizeof(sig) - len, "%s,", e->name);
		len = len < (int)sizeof(sig) ? len : (int)sizeof(sig) - 1;
		got++;
	}
	if (r->bad || !strcmp(sig, g_Coop.ctargets) || !Mode_Import("MissionsFuncs")) {
		Native_Free(&list);
		return;
	}
	Str_Copy(g_Coop.ctargets, sig, sizeof(g_Coop.ctargets));
	byte ok;
	if (got == 1) {
		ok = PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &list.items[0]);
	} else if (got) {
		ok = PORT_CALL(MissionsFuncs, SetMultipleMissionTargets, NULL, "v", &list);
	} else {
		nativeVal_t none;
		Native_SetNone(&none);
		ok = PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &none);
	}
	if (!ok) {
		Native_ErrPrint();
	}
	Con_Printf("coop: the mission's targets here: %s\n", sig[0] ? sig : "none");
	Native_Free(&list);
}

static void Cp_ClientSceneEnd(NetRepReader *r) {
	char cls[CP_NAME], name[CP_NAME + 16];
	NetRep_GetStr(r, cls, sizeof(cls));
	if (r->bad || strcmp(cls, g_Coop.clientScene)) {
		return;
	}
	g_Coop.clientScene[0] = 0;
	if (g_Coop.skipped) {
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
	g_Coop.world = 1;
	NetRepReader_Init(&r, data + 2, size - 2);
	switch (data[1]) {
	case COOP_Y_SCENE:
		/* played in the next frame of the running level (a member that arrives gets it before its level runs: its
		 * camera would be taken over by the level's start) */
		if (size <= (int)sizeof(g_Coop.pending)) {
			memcpy(g_Coop.pending, data, (size_t)size);
			g_Coop.pendingSize = size;
		}
		break;
	case COOP_Y_SCENEEND: {
		/* a scene that waits is dropped by its own end only (a callback starts the next scene before the end of the
		 * last one goes out) */
		char cls[CP_NAME], pcls[CP_NAME];
		NetRepReader pr, er = r;
		NetRep_GetStr(&er, cls, sizeof(cls));
		NetRepReader_Init(&pr, g_Coop.pending + 2, g_Coop.pendingSize > 2 ? g_Coop.pendingSize - 2 : 0);
		NetRep_GetStr(&pr, pcls, sizeof(pcls));
		NetRep_GetStr(&pr, pcls, sizeof(pcls));
		if (g_Coop.pendingSize && !pr.bad && !er.bad && !strcmp(cls, pcls)) {
			g_Coop.pendingSize = 0;
		}
		Cp_ClientSceneEnd(&r);
		break;
	}
	case COOP_Y_GONE:
		/* deleted in the next frame of the running level (the level the client loads makes them first) */
		if (size <= (int)sizeof(g_Coop.cgone)) {
			memcpy(g_Coop.cgone, data, (size_t)size);
			g_Coop.cgoneSize = size;
		}
		break;
	case COOP_Y_HEAD: {
		char model[128];
		NetRep_GetStr(&r, model, sizeof(model));
		NetRep_GetStr(&r, text, sizeof(text));
		int index = (int)NetRep_GetU8(&r);
		if (!r.bad) {
			/* the client's head of that name may have fewer animations than the instance's (a mission's AddModel there:
			 * ProtectMayorTransport's Rusty): its talk then */
			const cScorerModel *m = cScorer_FindModel(Scorer_Get(0), model);
			if (m && (index >= m->numAnims || -index >= m->numAnims)) {
				index = index < 0 ? -1 : 1;
			}
			Scorer_SetHeadMonitor(model, text, index);
			Con_Printf("coop: a remote communication of %s\n", text);
		}
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
		Coop_ClientKeys(text, (int)sizeof(text));
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
	case COOP_Y_FADE: {
		uint32_t c[8];
		for (int k = 0; k < 8; k++) {
			c[k] = NetRep_GetU8(&r);
		}
		float t = (float)NetRep_GetU32(&r) / 1000.0f;
		cScorer *sc = Scorer_Get(0);
		if (!r.bad && sc) {
			ScorerFade_Start(&sc->fade, Scorer_PackColor(c[0], c[1], c[2], c[3]), Scorer_PackColor(c[4], c[5], c[6], c[7]), t);
		}
		break;
	}
	case COOP_Y_VIDEO:
		NetRep_GetStr(&r, file, sizeof(file));
		if (!r.bad) {
			Con_Printf("coop: the video %s before the next level\n", file);
			Cp_ClientVideo(file);
		}
		break;
	case COOP_Y_STORY: {
		static char name[CP_NAME], val[COORD_STORY_VALUE + 1];
		int count = (int)NetRep_GetU16(&r), kind;
		for (int i = 0; i < count && CoordStory_Next(&r, &kind, name, sizeof(name), val, sizeof(val)) > 0; i++) {
			Cp_ClientStorySet(name, kind == COORD_OP_DEL ? NULL : val);
		}
		break;
	}
	case COOP_Y_SCORER:
		if (size <= (int)sizeof(g_Coop.cscore)) {
			memcpy(g_Coop.cscore, data, (size_t)size);
			g_Coop.cscoreSize = size;
			Cp_ClientScorer();
		}
		break;
	case COOP_Y_HUDVAR:
		NetRep_GetStr(&r, key, sizeof(key));
		NetRep_GetStr(&r, arg, sizeof(arg));
		if (!r.bad && Party_RuleHas("hud", key)) {
			SaveVar_Set(key, arg);
			if (!Mode_GVarSetI(key, atoi(arg))) {
				Native_ErrPrint();
			}
		}
		break;
	case COOP_Y_TARGETS:
		Cp_ClientTargets(&r);
		break;
	case COOP_Y_END:
		/* the credits of the main menu, as the single player's end; the world is left */
		Con_Printf("coop: the story is finished: the credits\n");
		if (!Mode_GVarSetI("ShowCredits", 1)) {
			Native_ErrPrint();
		}
		/* not here in the middle of the instance's messages: the join ends in its next pass */
		g_Coop.ended = 1;
		break;
	case COOP_Y_NOTICE:
		NetRep_GetStr(&r, key, sizeof(key));
		NetRep_GetStr(&r, arg, sizeof(arg));
		if (!r.bad) {
			Texts_Format(line, sizeof(line), key, Cp_NoticeArg(key, arg));
			Con_Printf("coop: %s\n", line);
			Text_Message(COOP_NOTICE_MS, "%s", line);
		}
		break;
	}
	return 1;
}

byte Coop_ClientEnded(void) {
	byte e = g_Coop.ended;
	g_Coop.ended = 0;
	return e;
}

void Coop_ClientReset(byte left) {
	g_Coop.world = g_Coop.world && !left;
	g_Coop.cmenu = 0;
	g_Coop.clientScene[0] = 0;
	g_Coop.pendingSize = 0;
	g_Coop.coop = 0;
	g_Coop.copt.n = 0;
	g_Coop.skipped = 0;
	g_Coop.ctargets[0] = 0;
	g_Coop.cscoreSize = 0;
	g_Coop.ncnames = 0;
}
