/*
 * One UI transaction channel of the second multiplayer design, see include/scrap/uichan.h. Not in the original.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/game.h"
#include "scrap/instance.h"
#include "scrap/net2.h"
#include "scrap/netaudit.h"
#include "scrap/netrep.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/pyfunction.h"
#include "scrap/replicate.h"
#include "scrap/repent.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/sputniks.h"
#include "scrap/travel.h"
#include "scrap/uichan.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"

#include "../Global/testchannel.h"
#include "game/Script/levels/Levels.h"

#define UI_NAME 96					/* a function "Module.Func", a machine's name */
#define UI_MSGMAX 8192				/* an open or a transaction with the hangar's ships */
#define UI_TEXT 1024				/* a title, a text box's text, a save variable's value */
#define UI_HANGARS 10				/* Hangar.HangarShip1-9 and the spare ship (Hangar.iHangarShip 9) */
#define UI_PLATFORM "DM_Door_EditShip_00"	/* the ship editor's platform (ShipEdit.editPlatform) */

static const char *const ui_names[UI_K_COUNT] = { "-", "yesno", "textbox", "conv", "metro", "sputnik", "shipedit",
	"mission" };

/* the save variables an interface reads (prefixes: a name ending in '.' takes all of that prefix) */
static const char *const ui_convVars[] = { "Char", "Conversor.AvailableChars", NULL };
static const char *const ui_hangarVars[] = { "Hangar.", NULL };
static const char *const ui_missionVars[] = { "Mission.", "MissionState", "PrimaryMissionDesc", "SecondaryMissionDesc",
	"SuperDeal.", "CrazyDeal.", "JackInTheBox", "NewBluePrintAvaliable", "GameAct", NULL };
/* what Sputnik's screen changes: the ship flown and the destroyed ones (and the money) */
static const char *const ui_sputnikBack[] = { "Hangar.iHangarShip", "Hangar.DestroyedShips", NULL };

typedef struct {
	byte ui;						/* the interface open on its client, 0 */
	uint16_t seq;
	char yes[UI_NAME], no[UI_NAME];	/* the functions of a question's answers */
	int money;						/* at the open */
} UiPlayer;

static struct {
	UiPlayer pl[PLAYERS_MAX];
	uint16_t seq;
	uint32_t opens[UI_K_COUNT], dones[UI_K_COUNT], refusals[UI_K_COUNT];
	char lastRefusal[UI_NAME];
	char via[UI_NAME];				/* the function of the answer that runs (an editor it opens replays it) */
	/* client */
	byte cui;						/* the interface of the instance shown here, 0 */
	uint16_t cseq;
	byte answering;					/* the instance's values of an interface the client asked for are being shown */
	cEntity *kept;					/* the replica the shown interface plays here (Ui_ClientKeeps), NULL */
	byte hudBack;					/* the conversor closed here: the HUD skeleton waits for the controls back */
	uint32_t copens[UI_K_COUNT], cdones[UI_K_COUNT], crefused[UI_K_COUNT];
} ui;

const char *Ui_Name(int k) {
	return k > 0 && k < UI_K_COUNT ? ui_names[k] : "-";
}

/* ---- the save variables of an open or a transaction: u16 count, then name (str) and value (u16 length + bytes) ---- */

typedef struct {
	NetRepBuf *w;
	const char *const *names;
	int count;
} UiVars;

static byte Ui_Wanted(const char *const *names, const char *name) {
	for (int k = 0; names[k]; k++) {
		size_t n = strlen(names[k]);
		if (names[k][n - 1] == '.' ? !strncmp(name, names[k], n) : !strcmp(name, names[k])) {
			return 1;
		}
	}
	return 0;
}

static void Ui_PutVar(const char *name, const char *value, void *u) {
	UiVars *v = u;
	int n = (int)strlen(value);
	if (!Ui_Wanted(v->names, name) || strlen(name) > 255 || n > UI_TEXT) {
		return;
	}
	NetRep_PutStr(v->w, name);
	NetRep_PutU16(v->w, (uint32_t)n);
	CoordMsg_PutBytes(v->w, value, n);
	v->count++;
}

/* the variables of names as the current context reads them */
static void Ui_PutVars(NetRepBuf *w, const char *const *names) {
	UiVars v = { w, names, 0 };
	int at = w->size;
	NetRep_PutU16(w, 0);
	SaveVars_EachSeen(Ui_PutVar, &v);
	if (!w->bad) {
		w->p[at] = (uint8_t)v.count;
		w->p[at + 1] = (uint8_t)(v.count >> 8);
	}
}

/* the variables of a message: each of names set (set NULL: only read and checked), fn for each (NULL: none) */
static byte Ui_GetVars(NetRepReader *r, const char *const *names, byte set,
	void (*fn)(const char *name, const char *value, void *u), void *u) {
	static char val[UI_TEXT + 1];
	char name[256];
	int count = (int)NetRep_GetU16(r);
	for (int k = 0; k < count && !r->bad; k++) {
		NetRep_GetStr(r, name, sizeof(name));
		int n = (int)NetRep_GetU16(r);
		if (n > UI_TEXT) {
			r->bad = 1;
			break;
		}
		CoordMsg_GetBytes(r, val, n);
		val[n] = 0;
		if (r->bad || !Ui_Wanted(names, name)) {
			continue;
		}
		if (set) {
			SaveVar_Set(name, val);
		}
		if (fn) {
			fn(name, val, u);
		}
	}
	return !r->bad;
}

/* ---- instance ---- */

static void Ui_ToPlayer(int i, const uint8_t *buf, int size) {
	int conn = Rep_PlayerConn(i);
	if (conn >= 0 && Instance_Host()) {
		Net2_Send(Instance_Host(), conn, REP_CH_RELIABLE, buf, size);
	}
}

byte Ui_Instance(void) {
	return g_Mp2 && g_Instance;
}

/* the player an interface of the instance opens for: the current one, in the world's context the acting one
 * (PLAYERS_NONE: nobody, the interface is dropped); -1 when this is not a -mp2 instance (single player builds it) */
static int Ui_For(int k) {
	if (!g_Mp2 || !g_Instance) {
		return -1;
	}
	int i = Players_Current();
	if (i == PLAYERS_NONE) {
		NetAudit_Func(NA_HUD, "ui.world:", ui_names[k]);
		i = Players_Acting();
	}
	return i >= 0 && i < PLAYERS_MAX && Rep_PlayerConn(i) >= 0 ? i : PLAYERS_NONE;
}

/* an open of interface k for player i begun into w (its number kept) */
static void Ui_Begin(NetRepBuf *w, uint8_t *buf, int size, int i, int k) {
	UiPlayer *p = &ui.pl[i];
	memset(p, 0, sizeof(*p));
	p->ui = (byte)k;
	p->seq = ++ui.seq;
	p->money = Players_Money(i);
	NetRepBuf_Init(w, buf, size);
	NetRep_PutU8(w, UI_MSG);
	NetRep_PutU8(w, UI_S_OPEN);
	NetRep_PutU8(w, (uint32_t)k);
	NetRep_PutU16(w, p->seq);
}

static byte Ui_Send(NetRepBuf *w, int i, int k) {
	if (w->bad) {
		Con_Printf("ui: the %s of player %d does not fit a message\n", ui_names[k], i);
		ui.pl[i].ui = 0;
		return 1;
	}
	Ui_ToPlayer(i, w->p, w->size);
	ui.opens[k]++;
	NetAudit_Func(NA_HUD, "ui.open:", ui_names[k]);
	Con_Printf("ui: the %s opens on the client of player %d\n", ui_names[k], i);
	return 1;
}

static void Ui_PutText(NetRepBuf *w, const char *text) {
	int n = (int)strlen(text);
	n = n > UI_TEXT ? UI_TEXT : n;
	NetRep_PutU16(w, (uint32_t)n);
	CoordMsg_PutBytes(w, text, n);
}

byte Ui_YesNo(const char *title, const char *yes, const char *no) {
	static uint8_t buf[UI_TEXT + 32];
	NetRepBuf w;
	int i = Ui_For(UI_K_YESNO);
	if (i < 0 || i == PLAYERS_NONE) {
		return i != -1;
	}
	Ui_Begin(&w, buf, sizeof(buf), i, UI_K_YESNO);
	Str_Copy(ui.pl[i].yes, yes, UI_NAME);
	Str_Copy(ui.pl[i].no, no, UI_NAME);
	Ui_PutText(&w, title);
	return Ui_Send(&w, i, UI_K_YESNO);
}

byte Ui_TextBox(const char *text) {
	static uint8_t buf[UI_TEXT + 32];
	NetRepBuf w;
	int i = Ui_For(UI_K_TEXTBOX);
	if (i < 0 || i == PLAYERS_NONE) {
		return i != -1;
	}
	/* a text box changes nothing: no transaction comes back, the player's interface stays as it was */
	byte was = ui.pl[i].ui;
	uint16_t seq = ui.pl[i].seq;
	Ui_Begin(&w, buf, sizeof(buf), i, UI_K_TEXTBOX);
	Ui_PutText(&w, text);
	ui.pl[i].ui = was;
	ui.pl[i].seq = seq;
	return Ui_Send(&w, i, UI_K_TEXTBOX);
}

/* the speed of the clock of player i's machine (the conversor waits for the pick while the world goes on) */
static void Ui_MachineSpeed(int i, double speed) {
	cPlayer *p = i >= 0 && i < PLAYERS_MAX ? Players_Ctx(i) : NULL;
	cEntity *m = p && p->machine && World_FindEntity(p->machineName) == p->machine ? p->machine : NULL;
	if (m) {
		Entity_VarF(m, "TimeSpeed", &speed, 1);
	}
}

byte Ui_Conv(void) {
	static uint8_t buf[UI_MSGMAX];
	NetRepBuf w;
	int i = Ui_For(UI_K_CONV);
	if (i < 0 || i == PLAYERS_NONE) {
		return i != -1;
	}
	Ui_Begin(&w, buf, sizeof(buf), i, UI_K_CONV);
	int was = Players_Enter(i);
	Ui_PutVars(&w, ui_convVars);
	Players_Leave(was);
	Ui_MachineSpeed(i, 0.0);
	return Ui_Send(&w, i, UI_K_CONV);
}

byte Ui_Metro(const char *usr, const char *door) {
	if (!g_Mp2 || !g_Instance) {
		return 0;
	}
	/* the metro's map and its pick are the travel's (include/scrap/travel.h: COORD_MSG_METRO, COORD_MSG_PICK) */
	cEntity *e = World_FindEntity(usr);
	int i = e ? Players_Of(e) : -1;
	if (i >= 0 && Travel_MetroUse(usr, door)) {
		ui.opens[UI_K_METRO]++;
		NetAudit_Func(NA_HUD, "ui.open:", ui_names[UI_K_METRO]);
	}
	return 1;
}

byte Ui_Sputnik(void) {
	static uint8_t buf[UI_MSGMAX];
	NetRepBuf w;
	int i = Ui_For(UI_K_SPUTNIK);
	if (i < 0 || i == PLAYERS_NONE) {
		return i != -1;
	}
	Ui_Begin(&w, buf, sizeof(buf), i, UI_K_SPUTNIK);
	NetRep_PutU32(&w, (uint32_t)Players_Money(i));
	int was = Players_Enter(i);
	Ui_PutVars(&w, ui_hangarVars);
	Players_Leave(was);
	return Ui_Send(&w, i, UI_K_SPUTNIK);
}

byte Ui_ShipEdit(void) {
	static uint8_t buf[UI_MSGMAX];
	NetRepBuf w;
	int i = Ui_For(UI_K_SHIPEDIT);
	if (i < 0 || i == PLAYERS_NONE) {
		return i != -1;
	}
	Ui_Begin(&w, buf, sizeof(buf), i, UI_K_SHIPEDIT);
	NetRep_PutStr(&w, ui.via);
	NetRep_PutU32(&w, (uint32_t)Players_Money(i));
	int was = Players_Enter(i);
	Ui_PutVars(&w, ui_hangarVars);
	Players_Leave(was);
	return Ui_Send(&w, i, UI_K_SHIPEDIT);
}

byte Ui_Open(int k) {
	int i = g_Mp2 && g_Instance ? Players_Current() : PLAYERS_NONE;
	return i >= 0 && i < PLAYERS_MAX && ui.pl[i].ui == k;
}

void Ui_NewsHead(int on) {
	uint8_t buf[4] = { UI_MSG, UI_S_NEWS, (uint8_t)(on != 0), 0 };
	int i = g_Mp2 && g_Instance ? Players_Current() : PLAYERS_NONE;
	if (i >= 0 && i < PLAYERS_MAX) {
		Ui_ToPlayer(i, buf, 3);
	}
}

byte Ui_MachineBusy(const char *machine, const char *msgKey) {
	if (!g_Mp2 || !g_Instance) {
		return 0;
	}
	cEntity *m = machine ? World_FindEntity(machine) : NULL;
	int i = Players_Current(), user = Players_OfMachine(m);
	if (user >= 0 && user != i) {
		/* single player's message of a machine that cannot be used now */
		if (!Mode_LangMsg(msgKey)) {
			Native_ErrPrint();
		}
		ui.refusals[UI_K_CONV]++;
		Str_Copy(ui.lastRefusal, "busy", sizeof(ui.lastRefusal));
		NetAudit_Func(NA_HUD, "ui.refuse:busy:", machine ? "machine" : "-");
		Con_Printf("ui: %s is player %d's, player %d's use refused\n", machine, user, i);
		return 1;
	}
	Players_SetMachine(i, m);
	return 0;
}

void Ui_MachineFree(void) {
	int i = g_Mp2 && g_Instance ? Players_Current() : PLAYERS_NONE;
	if (i >= 0 && i < PLAYERS_MAX) {
		Ui_MachineSpeed(i, 1.0);
		Players_SetMachine(i, NULL);
	}
}

/* the transaction of player i refused: single player's message (a language text, "" none) on its HUD, nothing changed */
static void Ui_Refuse(int i, int k, const char *why, const char *msgKey) {
	uint8_t buf[16];
	NetRepBuf w;
	if (msgKey && msgKey[0]) {
		int was = Players_Enter(i);
		if (!Mode_LangMsg(msgKey)) {
			Native_ErrPrint();
		}
		Players_Leave(was);
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, UI_MSG);
	NetRep_PutU8(&w, UI_S_REFUSE);
	NetRep_PutU8(&w, (uint32_t)k);
	NetRep_PutU16(&w, ui.pl[i].seq);
	Ui_ToPlayer(i, buf, w.size);
	ui.refusals[k]++;
	Str_Copy(ui.lastRefusal, why, sizeof(ui.lastRefusal));
	NetAudit_Func(NA_HUD, "ui.refuse:", why);
	Con_Printf("ui: the %s of player %d refused (%s)\n", ui_names[k], i, why);
}

/* "Module.Func"(0, "") in the current context: the function of a question's answer */
static void Ui_CallAnswer(const char *fn) {
	char mod[UI_NAME];
	const char *dot = strchr(fn, '.');
	if (!dot || dot - fn >= UI_NAME) {
		return;
	}
	Str_PrintfQuiet(mod, sizeof(mod), "%.*s", (int)(dot - fn), fn);
	Str_Copy(ui.via, fn, sizeof(ui.via));
	if (!Mode_Import(mod) || !Native_CallMod(mod, dot + 1, NULL, "is", 0L, "")) {
		Native_ErrPrint();
	}
	ui.via[0] = 0;
}

/* ShipEdit.destroyedShips as the save variable keeps it ("[0, 1, ...]"): the UI_HANGARS values, 0 when it is not
 * such a list */
static byte Ui_Destroyed(const char *text, int out[UI_HANGARS]) {
	const char *s = text ? strchr(text, '[') : NULL;
	for (int k = 0; k < UI_HANGARS; k++) {
		if (!s) {
			return 0;
		}
		char *end;
		out[k] = (int)strtol(s + 1, &end, 10);
		if (end == s + 1) {
			return 0;
		}
		s = strchr(end, k + 1 < UI_HANGARS ? ',' : ']');
	}
	return 1;
}

/* the save variable of hangar k ("Hangar.HangarShip<k+1>", 9 the spare ship) */
static void Ui_HangarVar(int k, char *var, int cap) {
	if (k == 9) {
		Str_Copy(var, "Hangar.HangarShipAux", cap);
	} else {
		Str_PrintfQuiet(var, cap, "Hangar.HangarShip%d", k + 1);
	}
}

/* the ship text of hangar k of the current context ("" none) */
static const char *Ui_HangarShip(int k) {
	char var[32];
	Ui_HangarVar(k, var, sizeof(var));
	const char *v = SaveVar_Get(var);
	return v ? v : "";
}

typedef struct {
	char fly[16], lost[UI_TEXT];
	byte haveFly, haveLost;
} UiSputnikBack;

static void Ui_SputnikVar(const char *name, const char *value, void *u) {
	UiSputnikBack *b = u;
	if (!strcmp(name, "Hangar.iHangarShip")) {
		Str_Copy(b->fly, value, sizeof(b->fly));
		b->haveFly = 1;
	} else {
		Str_Copy(b->lost, value, sizeof(b->lost));
		b->haveLost = 1;
	}
}

/* the world's part of the end of Sputnik's screen in player i's context (also after a refusal: the screen closed on
 * the client): SaveMoney, the callback of the scripts */
static void Ui_EndSputnik(int i) {
	nativeVal_t cb, none;
	Native_SetNone(&cb);
	Native_SetNone(&none);
	int was = Players_Enter(i);
	if (!Mode_Import("SaveGame") || !PORT_CALL(SaveGame, SaveMoney, NULL, "") ||
		!Mode_ModGet("SputnikInterface", "EndSputnikCallBack", &cb) ||
		(Native_True(&cb) && (!Mode_CallArgs(&cb, NULL, NULL, 0) || !Mode_SetModVar("SputnikInterface", "EndSputnikCallBack",
		&none)))) {
		Native_ErrPrint();
	}
	Native_Free(&cb);
	Players_Leave(was);
}

/* Sputnik's screen closed on player i's client: the ship it flies and its money as single player's SelectShip gives
 * them, checked against its rules (an intact ship of the hangar, or a destroyed one rebuilt for rebuildCost with the
 * money for it, no other change), then single player's end of the screen (SaveMoney, the destroyed ships saved, the
 * callback of the scripts) in its context */
static void Ui_DoneSputnik(int i, NetRepReader *r) {
	UiSputnikBack b;
	int oldLost[UI_HANGARS], newLost[UI_HANGARS];
	memset(&b, 0, sizeof(b));
	int money = (int)NetRep_GetU32(r);
	if (!Ui_GetVars(r, ui_sputnikBack, 0, Ui_SputnikVar, &b)) {
		return;
	}
	int was = Players_Enter(i);
	const char *fly0 = SaveVar_Get("Hangar.iHangarShip");
	int k = b.haveFly ? crt_atoi(b.fly) : fly0 ? crt_atoi(fly0) : -1;
	byte lostOk = Ui_Destroyed(SaveVar_Get("Hangar.DestroyedShips"), oldLost);
	if (!lostOk) {
		/* a hangar without destroyed ships yet: none destroyed */
		memset(oldLost, 0, sizeof(oldLost));
	}
	if (!b.haveLost) {
		memcpy(newLost, oldLost, sizeof(newLost));
	}
	nativeVal_t cost;
	Native_SetNone(&cost);
	int rebuild = Mode_ModGet("SputnikInterface", "rebuildCost", &cost) && (cost.type == NV_INT || cost.type == NV_LONG) ?
		(int)cost.i : -1;
	Native_Free(&cost);
	Native_ErrClear();
	const char *why = NULL, *msg = "";
	int rebuilt = 0;
	if ((b.haveLost && !Ui_Destroyed(b.lost, newLost)) || k < 0 || k >= UI_HANGARS || !Ui_HangarShip(k)[0]) {
		why = "hangar";
	}
	for (int h = 0; !why && h < UI_HANGARS; h++) {
		if (newLost[h] == oldLost[h]) {
			continue;
		}
		if (h == k && oldLost[h] == 1 && newLost[h] == 0) {
			rebuilt = 1;
		} else {
			why = "destroyed";
		}
	}
	if (!why && newLost[k] != 0) {
		why = "destroyed";
	}
	if (!why && rebuilt && (rebuild < 0 || ui.pl[i].money < rebuild)) {
		why = "money";
		msg = "HINT_NoMoney";
	}
	if (!why && money != ui.pl[i].money - (rebuilt ? rebuild : 0)) {
		why = "money";
	}
	/* the money changed in the world meanwhile stays (the transaction is the screen's change) */
	long long now = (long long)Players_Money(i) + money - ui.pl[i].money;
	if (!why && now < 0) {
		why = "money";
		msg = "HINT_NoMoney";
	}
	if (why) {
		Players_Leave(was);
		Ui_Refuse(i, UI_K_SPUTNIK, why, msg);
		Ui_EndSputnik(i);
		return;
	}
	char fly[16];
	Str_PrintfQuiet(fly, sizeof(fly), "%d", k);
	*Players_CurMoney() = (int)now;
	SaveVar_Set("Hangar.iHangarShip", fly);
	if (b.haveLost) {
		SaveVar_Set("Hangar.DestroyedShips", b.lost);
	}
	Players_Leave(was);
	Ui_EndSputnik(i);
	Con_Printf("ui: player %d flies hangar %d%s, money %d\n", i, k, rebuilt ? " (rebuilt)" : "", (int)now);
}

typedef struct {
	char ship[UI_HANGARS][UI_TEXT + 1];
	char fly[16], lost[UI_TEXT + 1];
	byte have[UI_HANGARS], haveFly, haveLost;
} UiHangarBack;

static void Ui_HangarBackVar(const char *name, const char *value, void *u) {
	UiHangarBack *b = u;
	char var[32];
	if (!strcmp(name, "Hangar.iHangarShip")) {
		Str_Copy(b->fly, value, sizeof(b->fly));
		b->haveFly = 1;
		return;
	}
	if (!strcmp(name, "Hangar.DestroyedShips")) {
		Str_Copy(b->lost, value, sizeof(b->lost));
		b->haveLost = 1;
		return;
	}
	for (int k = 0; k < UI_HANGARS; k++) {
		Ui_HangarVar(k, var, sizeof(var));
		if (!strcmp(name, var)) {
			Str_Copy(b->ship[k], value, sizeof(b->ship[k]));
			b->have[k] = 1;
		}
	}
}

/* the worth of a hangar: each intact ship as single player's editor prices it (HangarTab.ComputeShipTotalCost: its sale
 * gives that, its parts cost their share), a destroyed one nothing (its rebuild costs its whole price); 0 when a ship is
 * not one the editor makes */
static byte Ui_HangarWorth(const char *const ships[UI_HANGARS], const int lost[UI_HANGARS], long long *worth) {
	*worth = 0;
	for (int k = 0; k < UI_HANGARS; k++) {
		int cost;
		if (!HangarTab_TextCost(ships[k], &cost)) {
			Native_ErrPrint();
			return 0;
		}
		*worth += lost[k] ? 0 : cost;
	}
	return 1;
}

/* the world's part of the editor's end in player i's context (also after a refusal: the editor closed on the
 * client) */
static void Ui_EndShipEdit(int i) {
	int was = Players_Enter(i);
	if (!ShipEdit_ClientEnded()) {
		Native_ErrPrint();
	}
	Players_Leave(was);
}

/* the ship editor closed on player i's client: its hangar and money, checked against single player's prices (the money
 * it has plus the worth of its hangar not above what they were at the open: each part, ship and rebuild paid at the
 * price of single player's editor, every ship one the editor makes, the money not below 0), then applied with the
 * editor's end in the world (its way point, the ship on the platform, its callback) in the player's context */
static void Ui_DoneShipEdit(int i, NetRepReader *r) {
	static UiHangarBack b;
	const char *before[UI_HANGARS], *after[UI_HANGARS];
	int lost0[UI_HANGARS], lost1[UI_HANGARS];
	char var[32];
	memset(&b, 0, sizeof(b));
	int money = (int)NetRep_GetU32(r);
	if (!Ui_GetVars(r, ui_hangarVars, 0, Ui_HangarBackVar, &b)) {
		return;
	}
	int was = Players_Enter(i);
	for (int k = 0; k < UI_HANGARS; k++) {
		before[k] = Ui_HangarShip(k);
		after[k] = b.have[k] ? b.ship[k] : before[k];
	}
	if (!Ui_Destroyed(SaveVar_Get("Hangar.DestroyedShips"), lost0)) {
		memset(lost0, 0, sizeof(lost0));
	}
	const char *why = NULL, *msg = "";
	if (!b.haveLost) {
		memcpy(lost1, lost0, sizeof(lost1));
	} else if (!Ui_Destroyed(b.lost, lost1)) {
		why = "hangar";
	}
	/* -1: no ship (an empty hangar was chosen last, as single player's editor leaves it) */
	int fly = b.haveFly ? crt_atoi(b.fly) : 0;
	if (!why && b.haveFly && (fly < -1 || fly >= UI_HANGARS)) {
		why = "hangar";
	}
	long long worth0 = 0, worth1 = 0;
	if (!why && (!Ui_HangarWorth(before, lost0, &worth0) || !Ui_HangarWorth(after, lost1, &worth1))) {
		why = "ship";
	}
	/* the money changed in the world meanwhile stays (the transaction is the editor's change) */
	long long now = (long long)Players_Money(i) + money - ui.pl[i].money;
	/* worth no more than before: single player's editor may give less (a sale pays HangarTab.shipTotalCost, which a
	 * ship bought just now does not have yet), never more */
	if (!why && (long long)money + worth1 > (long long)ui.pl[i].money + worth0) {
		why = "price";
	}
	if (!why && (money < 0 || now < 0)) {
		why = "money";
		msg = "HINT_NoMoney";
	}
	if (why) {
		Con_Printf("ui: the ship editor of player %d: money %d -> %d, hangar worth %lld -> %lld\n", i, ui.pl[i].money, money,
			worth0, worth1);
		Players_Leave(was);
		Ui_Refuse(i, UI_K_SHIPEDIT, why, msg);
		Ui_EndShipEdit(i);
		return;
	}
	*Players_CurMoney() = (int)now;
	for (int k = 0; k < UI_HANGARS; k++) {
		if (b.have[k] && strcmp(before[k], b.ship[k])) {
			Ui_HangarVar(k, var, sizeof(var));
			SaveVar_Set(var, b.ship[k]);
		}
	}
	if (b.haveFly) {
		SaveVar_Set("Hangar.iHangarShip", b.fly);
	}
	if (b.haveLost) {
		SaveVar_Set("Hangar.DestroyedShips", b.lost);
	}
	Players_Leave(was);
	Ui_EndShipEdit(i);
	Con_Printf("ui: player %d closed the ship editor (hangar %d, money %d, worth %lld)\n", i, fly, (int)now, worth1);
}

byte Ui_Message(Net2Host *host, int conn, const uint8_t *data, int size) {
	NetRepReader r;
	(void)host;
	if (size < 2 || data[0] != UI_MSG) {
		return 0;
	}
	int i = Rep_ConnPlayer(conn);
	if (!g_Mp2 || i < 0 || i >= PLAYERS_MAX) {
		return 1;
	}
	NetRepReader_Init(&r, data + 2, size - 2);
	int k = (int)NetRep_GetU8(&r);
	if (data[1] == UI_C_ASK) {
		if (k == UI_K_MISSION && !r.bad) {
			static uint8_t buf[UI_MSGMAX];
			NetRepBuf w;
			byte was = ui.pl[i].ui;
			uint16_t seq = ui.pl[i].seq;
			Ui_Begin(&w, buf, sizeof(buf), i, UI_K_MISSION);
			int prev = Players_Enter(i);
			Ui_PutVars(&w, ui_missionVars);
			Players_Leave(prev);
			ui.pl[i].ui = was;
			ui.pl[i].seq = seq;
			Ui_Send(&w, i, UI_K_MISSION);
		}
		return 1;
	}
	int seq = (int)NetRep_GetU16(&r);
	UiPlayer *p = &ui.pl[i];
	if (data[1] != UI_C_DONE || r.bad || k <= 0 || k >= UI_K_COUNT) {
		return 1;
	}
	if (p->ui != k || p->seq != seq) {
		/* not the interface the instance opened for it (an old one, another one): nothing changes */
		Ui_Refuse(i, k, "unasked", "");
		return 1;
	}
	p->ui = 0;
	ui.dones[k]++;
	NetAudit_Func(NA_HUD, "ui.done:", ui_names[k]);
	switch (k) {
	case UI_K_YESNO: {
		int yes = (int)NetRep_GetU8(&r);
		if (r.bad) {
			break;
		}
		Con_Printf("ui: player %d answers %s\n", i, yes ? "yes" : "no");
		int was = Players_Enter(i);
		Ui_CallAnswer(yes ? p->yes : p->no);
		Players_Leave(was);
		break;
	}
	case UI_K_CONV: {
		char cls[UI_NAME];
		NetRep_GetStr(&r, cls, sizeof(cls));
		if (r.bad) {
			break;
		}
		Ui_MachineSpeed(i, 1.0);
		int was = Players_Enter(i);
		byte ok = CharConversor_Pick(cls);
		Players_Leave(was);
		if (ok) {
			Con_Printf("ui: player %d picks %s in the conversor\n", i, cls[0] ? cls : "nothing");
		} else {
			Ui_Refuse(i, k, "class", "");
		}
		break;
	}
	case UI_K_SPUTNIK:
		Ui_DoneSputnik(i, &r);
		break;
	case UI_K_SHIPEDIT:
		Ui_DoneShipEdit(i, &r);
		break;
	default:
		break;
	}
	return 1;
}

void Ui_PlayerGone(int i) {
	if (i >= 0 && i < PLAYERS_MAX) {
		memset(&ui.pl[i], 0, sizeof(ui.pl[i]));
	}
}

/* ---- client ---- */
/* the interface shown here closed: the replica it played goes back to the instance's state */
static void Ui_ClientClosed(void) {
	ui.cui = 0;
	if (ui.kept && World_FindEntity(UI_PLATFORM) == ui.kept) {
		Obj(ui.kept)->replica = 1;
	}
	ui.kept = NULL;
}


static void Ui_ClientSend(const uint8_t *buf, int size, int k) {
	if (Watch_SendReliable(buf, size)) {
		ui.cdones[k]++;
		NetAudit_Func(NA_HUD, "ui.client.done:", ui_names[k]);
	}
}

static void Ui_ClientBegin(NetRepBuf *w, uint8_t *buf, int size, int k) {
	NetRepBuf_Init(w, buf, size);
	NetRep_PutU8(w, UI_MSG);
	NetRep_PutU8(w, UI_C_DONE);
	NetRep_PutU8(w, (uint32_t)k);
	NetRep_PutU16(w, ui.cseq);
}

static void Ui_ClientAnswer(int yes) {
	uint8_t buf[16];
	NetRepBuf w;
	if (ui.cui != UI_K_YESNO) {
		return;
	}
	Ui_ClientClosed();
	if (!PORT_CALL(Menu, BackToGame, NULL, "is", 0L, "")) {
		Native_ErrPrint();
	}
	Ui_ClientBegin(&w, buf, sizeof(buf), UI_K_YESNO);
	NetRep_PutU8(&w, (uint32_t)yes);
	Ui_ClientSend(buf, w.size, UI_K_YESNO);
	Con_Printf("ui: the answer %s to the instance\n", yes ? "yes" : "no");
}

byte Ui_ClientYesNo(int yes) {
	if (!Ui_ClientIn(UI_K_YESNO)) {
		return 0;
	}
	Con_Printf("ui: test: the answer %s\n", yes ? "yes" : "no");
	Ui_ClientAnswer(yes);
	return 1;
}

static void Ui_MenuYes(cAbstractFile *params) {
	(void)params;
	Ui_ClientAnswer(1);
}

static void Ui_MenuNo(cAbstractFile *params) {
	(void)params;
	Ui_ClientAnswer(0);
}

static void Ui_GetText(NetRepReader *r, char *out) {
	int n = (int)NetRep_GetU16(r);
	if (n > UI_TEXT) {
		r->bad = 1;
		n = 0;
	}
	CoordMsg_GetBytes(r, out, n);
	out[n] = 0;
}

/* the ship editor of the instance's hangar on the client: the ship on the platform again (the one of the client's own
 * level start goes), then the function of the answer that opened it there (a tutorial), else the editor as the keypad
 * opens it (MapShipEdit.EnterEditShip without its world part: the keypad and the player are the instance's) */
static void Ui_ClientEditor(const char *via) {
	char mod[UI_NAME];
	nativeVal_t none, plt;
	Native_SetNone(&none);
	const char *dot = strchr(via, '.');
	byte ok = Mode_Import("HangarTab") && Mode_SetModVar("HangarTab", "prevShip", &none) && Mode_Import("ShipEdit"), done = 0;
	/* the platform the client's level start made went with the purge (include/scrap/repent.h): the editor takes its
	 * replica */
	if (ok && Native_SetEnt(&plt, UI_PLATFORM)) {
		ok = Mode_SetModVar("ShipEdit", "editPlatform", &plt);
		Native_Free(&plt);
		/* the place of the ship on it (since the level split of stage 4.2 its initializer runs on the server only) */
		ok = ok && ShipEdit_ClientPlatform(World_FindEntity(UI_PLATFORM));
	}
	/* its Change animation and the events of it (ShipEdit.ChangeShip, VerifyChangeShip) run here while the editor is
	 * open: the replica advances as an entity of the client's own */
	ui.kept = World_FindEntity(UI_PLATFORM);
	if (ui.kept && Entity_IsType(ui.kept, TYEN_OBJECT)) {
		Obj(ui.kept)->replica = 0;
	} else {
		ui.kept = NULL;
	}
	ok = ok && Junkyard_PlatformShip();
	if (ok && dot && dot - via < UI_NAME) {
		Str_PrintfQuiet(mod, sizeof(mod), "%.*s", (int)(dot - via), via);
		done = Mode_Import(mod) && Native_CallMod(mod, dot + 1, NULL, "is", 0L, "");
		Native_ErrPrint();
	}
	if (ok && !done) {
		ok = Mode_Import("Menu") && PORT_CALL(Menu, InitMenuSys, NULL, "i", 0L) &&
			PORT_CALL(ShipEdit, EnterEditShipMenu, NULL, "is", 0L, "");
		Script_SetNum(0, "Cursor", "Visible", 0);
	}
	if (!ok) {
		Native_ErrPrint();
	}
}

static void Ui_ClientOpen(int k, NetRepReader *r) {
	static cPy_NativeFunc natives[2];
	static char text[UI_TEXT + 1];
	char via[UI_NAME];
	if (!natives[0].name) {
		cPy_NativeFunc_ctor(&natives[0], "UiMenuYes", Ui_MenuYes);
		cPy_NativeFunc_ctor(&natives[1], "UiMenuNo", Ui_MenuNo);
	}
	ui.copens[k]++;
	NetAudit_Func(NA_HUD, "ui.client.open:", ui_names[k]);
	switch (k) {
	case UI_K_YESNO:
		Ui_GetText(r, text);
		if (r->bad || !Mode_Import("Menu")) {
			return;
		}
		ui.cui = UI_K_YESNO;
		/* as the scripts open it in single player, its answers to the instance */
		if (!PORT_CALL(Menu, InitMenuSys, NULL, "i", 0L) ||
			!PORT_CALL(Menu, YesNoMenu, NULL, "isss", 0L, text, "!UiMenuYes", "!UiMenuNo")) {
			Native_ErrPrint();
			return;
		}
		Script_SetNum(0, "Cursor", "Visible", 0);
		Script_SetDefault(0, "Yes");
		Con_Printf("ui: the instance asks: %s\n", text);
		return;
	case UI_K_TEXTBOX:
		Ui_GetText(r, text);
		/* single player's box (it waits until the player plays: its entry calls it again) */
		if (!r->bad && (!Mode_Import("Menu") || !PORT_CALL(Menu, ShowWarning, NULL, "is", 0L, text))) {
			Native_ErrPrint();
		}
		Con_Printf("ui: a text box of the instance: %.60s\n", text);
		return;
	case UI_K_CONV: {
		nativeVal_t t, list;
		if (!Ui_GetVars(r, ui_convVars, 1, NULL, NULL)) {
			return;
		}
		const char *avl = SaveVar_Get("Conversor.AvailableChars");
		Native_SetStr(&t, avl ? avl : "[]");
		Native_SetNone(&list);
		if (!Mode_Import("CharConversor") || !Native_EvalText("CharConversor", &t, &list) ||
			!Mode_SetModVar("CharConversor", "AvailableChars", &list)) {
			Native_ErrPrint();
		}
		Native_Free(&t);
		Native_Free(&list);
		ui.cui = UI_K_CONV;
		if (!PORT_CALL(CharConversor, AddInterfaceContent, NULL, "i", 0L)) {
			Native_ErrPrint();
		}
		Con_Printf("ui: the conversor's interface of the instance (as %s)\n", SaveVar_Get("Char") ? SaveVar_Get("Char") :
			"-");
		return;
	}
	case UI_K_SPUTNIK: {
		int money = (int)NetRep_GetU32(r);
		if (r->bad || !Ui_GetVars(r, ui_hangarVars, 1, NULL, NULL)) {
			return;
		}
		*Players_CurMoney() = money;
		ui.cui = UI_K_SPUTNIK;
		if (Mode_Import("ShipEdit")) {
			Sputniks_ClientDummy();
		}
		if (!Mode_Import("SputnikInterface") || !PORT_CALL(SputnikInterface, EnterSputnikInterface, NULL, "i", 0L)) {
			Native_ErrPrint();
		}
		Con_Printf("ui: Sputnik's hangar screen of the instance, money %d\n", money);
		return;
	}
	case UI_K_SHIPEDIT: {
		NetRep_GetStr(r, via, sizeof(via));
		int money = (int)NetRep_GetU32(r);
		if (r->bad || !Ui_GetVars(r, ui_hangarVars, 1, NULL, NULL)) {
			return;
		}
		*Players_CurMoney() = money;
		ui.cui = UI_K_SHIPEDIT;
		Ui_ClientEditor(via);
		Con_Printf("ui: the ship editor of the instance%s%s, money %d\n", via[0] ? " with " : "", via, money);
		return;
	}
	case UI_K_MISSION:
		if (!Ui_GetVars(r, ui_missionVars, 1, NULL, NULL)) {
			return;
		}
		ui.answering = 1;
		if (!Mode_Import("Menu") || !PORT_CALL(Menu, EnterMissionInfo, NULL, "i", 0L)) {
			Native_ErrPrint();
		}
		ui.answering = 0;
		return;
	default:
		return;
	}
}

void Ui_ClientMessage(const uint8_t *data, int size) {
	NetRepReader r;
	if (size < 3 || data[0] != UI_MSG) {
		return;
	}
	if (data[1] == UI_S_NEWS) {
		GVar_SetNum("NewsPanelShowHead", data[2] != 0);
		return;
	}
	NetRepReader_Init(&r, data + 2, size - 2);
	int k = (int)NetRep_GetU8(&r);
	int seq = (int)NetRep_GetU16(&r);
	if (r.bad || k <= 0 || k >= UI_K_COUNT) {
		return;
	}
	if (data[1] == UI_S_OPEN) {
		if (k != UI_K_TEXTBOX && k != UI_K_MISSION) {
			ui.cseq = (uint16_t)seq;
		}
		Ui_ClientOpen(k, &r);
	} else if (data[1] == UI_S_REFUSE) {
		ui.crefused[k]++;
		Con_Printf("ui: the instance refused the %s\n", ui_names[k]);
	} else if (data[1] == UI_S_CLOSE && ui.cui == k && ui.cseq == seq) {
		Ui_ClientClosed();
		if (!PORT_CALL(Menu, BackToGame, NULL, "is", 0L, "")) {
			Native_ErrPrint();
		}
	}
}

byte Ui_ClientIn(int k) {
	return g_Mp2 && g_Watch && ui.cui && (!k || ui.cui == k);
}

byte Ui_ClientMoney(void) {
	return Ui_ClientIn(UI_K_SPUTNIK) || Ui_ClientIn(UI_K_SHIPEDIT);
}

byte Ui_ClientKeeps(const cEntity *e) {
	return e && e == ui.kept;
}


byte Ui_ClientConv(const char *cls) {
	uint8_t buf[16 + UI_NAME];
	NetRepBuf w;
	if (!Ui_ClientIn(UI_K_CONV)) {
		return 0;
	}
	Ui_ClientClosed();
	ui.hudBack = 1;
	if (!PORT_CALL(Menu, BackToGame, NULL, "is", 0L, "")) {
		Native_ErrPrint();
	}
	Ui_ClientBegin(&w, buf, sizeof(buf), UI_K_CONV);
	NetRep_PutStr(&w, cls);
	if (!w.bad) {
		Ui_ClientSend(buf, w.size, UI_K_CONV);
		Con_Printf("ui: the conversor's pick %s to the instance\n", cls[0] ? cls : "(none)");
	}
	return 1;
}

byte Ui_ClientHangar(int k) {
	static uint8_t buf[UI_MSGMAX];
	NetRepBuf w;
	if (!Ui_ClientIn(k)) {
		return 0;
	}
	Ui_ClientClosed();
	Ui_ClientBegin(&w, buf, sizeof(buf), k);
	NetRep_PutU32(&w, (uint32_t)*Players_CurMoney());
	Ui_PutVars(&w, k == UI_K_SPUTNIK ? ui_sputnikBack : ui_hangarVars);
	if (!w.bad) {
		const char *cur = SaveVar_Get("Hangar.iHangarShip");
		Ui_ClientSend(buf, w.size, k);
		Con_Printf("ui: the %s to the instance (hangar %s, money %d)\n", ui_names[k], cur ? cur : "-", *Players_CurMoney());
	}
	return 1;
}

void Ui_ClientHudCleared(void) {
	ui.hudBack = 1;
}

void Ui_ClientControls(const char *set) {
	/* the end of the machine's scene (CharConversor.EndOfScene) builds the HUD skeleton the instance does not send
	 * (include/scrap/pres.h): the client's own, as Play.c builds it */
	byte racer = !strcmp(set, "Racer");
	/* an interface shown here (Sputnik.EnterInterface gives the walk controls with its screen) keeps its menu */
	if (!ui.hudBack || ui.cui || (!racer && strcmp(set, "Walk"))) {
		return;
	}
	ui.hudBack = 0;
	const char *mod = racer ? "RacerScorer" : "CharScorer";
	if (!Mode_Import(mod) || !Native_CallMod(mod, "Create", NULL, "i", 0L)) {
		Native_ErrPrint();
	}
}

byte Ui_ClientAsk(int k) {
	uint8_t buf[4] = { UI_MSG, UI_C_ASK, (uint8_t)k, 0 };
	if (!g_Mp2 || !g_Watch || ui.answering) {
		return 0;
	}
	return Watch_SendReliable(buf, 3);
}

/* ---- the test verb ---- */

/* a save variable of the hangar (not the ones Ui_State lists) or of the blueprints */
static void Ui_StateVar(const char *name, const char *value, void *u) {
	if ((!strncmp(name, "Hangar.", 7) && strncmp(name, "Hangar.HangarShip", 17) && strcmp(name, "Hangar.iHangarShip") &&
		strcmp(name, "Hangar.DestroyedShips")) || !strncmp(name, "Parked.", 7)) {
		TC_Addf(u, "%s\t%s\n", name, value);
	}
}

/* "ui state [player]": the values the interfaces change, as the player's context reads them */
static void Ui_State(tcBuf *b, int i) {
	static const char *const vars[] = { "Char", "Conversor.AvailableChars", "Conversor.FirstConversion", "Hangar.iHangarShip",
		"Hangar.DestroyedShips", "Hangar.HangarShip1", "Hangar.HangarShip2", "Hangar.HangarShipAux", "Spawn", "ComeFrom",
		"Map", "Money", "SecondaryMissionDesc", "Challenge.Type", NULL };
	int was = Players_Enter(i);
	cEntity *e = Players_Net() ? Players_Get(i) : cWorld_GetUsrEntity(g_pWorld, 0);
	const char *cls = "-";
	if (e && Entity_IsType(e, TYEN_WITH_LIFE) && WLife(e)->actCtrl && WLife(e)->actCtrl->cls) {
		cls = WLife(e)->actCtrl->cls->name;
	}
	TC_Addf(b, "money\t%d\n", *Players_CurMoney());
	TC_Addf(b, "class\t%s\n", cls);
	TC_Addf(b, "entity\t%s\n", !e ? "none" : Entity_IsType(e, TYEN_CAR) ? "ship" : Entity_IsType(e, TYEN_WALKCHAR) ?
		"char" : "other");
	TC_Addf(b, "life\t%d\n", e && Entity_IsType(e, TYEN_WITH_LIFE) ? (int)WLife(e)->life : -1);
	if (e) {
		TC_Addf(b, "pos\t%.0f %.0f %.0f\n", (double)e->pos.x, (double)e->pos.y, (double)e->pos.z);
	}
	for (int k = 0; vars[k]; k++) {
		const char *v = SaveVar_Get(vars[k]);
		TC_Addf(b, "%s\t%s\n", vars[k], v ? v : "(none)");
	}
	/* the news the player reads (NewsPanel, a PLAYER row of the module variables) */
	nativeVal_t v, r;
	Native_SetNone(&v);
	Native_SetNone(&r);
	if (Mode_Import("NewsPanel") && Mode_ModGet("NewsPanel", "ShowingNews", &v)) {
		Mode_Repr(&v, &r);
		TC_Addf(b, "NewsPanel.ShowingNews\t%s\n", r.type == NV_STR ? Native_Text(&r) : "?");
	}
	Native_ErrClear();
	Native_Free(&v);
	Native_Free(&r);
	/* the free activities (stage 6a): lives, the player's ship and its text, its Sputnik, the other hangar and
	 * blueprint variables */
	const char *lives = SaveVar_Get("Player.NumLives");
	TC_Addf(b, "Player.NumLives\t%s\n", lives ? lives : "(none)");
	SaveVars_EachSeen(Ui_StateVar, b);
	static const char *const ents[][2] = { { "Parking", "PlayerShip" }, { "Sputnik", "spt" } };
	cEntity *pair[2] = { NULL, NULL };
	for (int k = 0; k < 2; k++) {
		char text[512];
		cEntity *x = NULL;
		Native_SetNone(&v);
		if (Mode_Import(ents[k][0]) && Mode_ModGet(ents[k][0], ents[k][1], &v) && v.type == NV_ENT) {
			x = Native_Ent(&v);
		}
		Native_ErrClear();
		Native_Free(&v);
		text[0] = 0;
		if (x && k == 0) {
			ShipEdit_ShipText(x, text, sizeof(text));
		}
		TC_Addf(b, "%s.%s\t%s %d %s\n", ents[k][0], ents[k][1], x ? x->name : "None", x ? x->active : -1, text);
		pair[k] = x;
	}
	/* Sputnik beside the player's ship (both shown, within 2000 units) */
	float dx = pair[0] && pair[1] ? pair[0]->pos.x - pair[1]->pos.x : 1e9f;
	float dz = pair[0] && pair[1] ? pair[0]->pos.z - pair[1]->pos.z : 0.0f;
	TC_Addf(b, "Sputnik.beside\t%d\n", pair[0] && pair[1] && pair[0]->active && pair[1]->active && dx * dx + dz * dz <
		2000.0f * 2000.0f);
	Players_Leave(was);
}

void Ui_Status(tcBuf *b, const char *arg) {
	if (arg && !strncmp(arg, "state", 5)) {
		Ui_State(b, arg[5] ? crt_atoi(arg + 6) : 0);
		return;
	}
	int i, at = 0;
	if (arg && sscanf(arg, "call %d %n", &i, &at) == 1 && at > 0 && i >= 0 && i < PLAYERS_MAX) {
		/* "ui call <player> Module.Func|arg|...": a call table line of the scripts in the player's context (an
		 * interface the scripts open for that player) */
		const char *one[2] = { arg + at, NULL };
		char mod[UI_NAME];
		Str_PrintfQuiet(mod, sizeof(mod), "%.*s", (int)strcspn(arg + at, ".|"), arg + at);
		int was = Players_Enter(i);
		byte ok = Mode_Import(mod) && Mode_Calls(NULL, one);
		Native_ErrPrint();
		Players_Leave(was);
		TC_Addf(b, "%s\n", ok ? "called" : "failed");
		return;
	}
	TC_Addf(b, "ui: %s\n", !g_Mp2 ? "off" : g_Instance ? "instance" : "client");
	for (int k = 1; k < UI_K_COUNT; k++) {
		if (ui.opens[k] || ui.dones[k] || ui.refusals[k] || ui.copens[k] || ui.cdones[k] || ui.crefused[k]) {
			TC_Addf(b, "%s\topen %u done %u refused %u\tclient open %u done %u refused %u\n", ui_names[k], ui.opens[k],
				ui.dones[k], ui.refusals[k], ui.copens[k], ui.cdones[k], ui.crefused[k]);
		}
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (ui.pl[i].ui) {
			TC_Addf(b, "player %d\t%s (%u)\n", i, ui_names[ui.pl[i].ui], ui.pl[i].seq);
		}
	}
	if (ui.cui) {
		TC_Addf(b, "shown\t%s (%u)\n", ui_names[ui.cui], ui.cseq);
	}
	if (ui.lastRefusal[0]) {
		TC_Addf(b, "last refusal\t%s\n", ui.lastRefusal);
	}
}
