/*
 * The online game modes in C (Data.packed scripts/net/n*.py, issue #2): NDeathMatch, NDeathMatchBots, NTeamDM,
 * NTeamFlag, NFlagHunt, NPostman and NTeamPost. init.py preloads the mode of ServerType ("N" + ServerType,
 * NDeathMatch when there is no such file) after Ffa in a network game and sets Ffa.ModLib to it; Ffa calls the mode
 * through ModLib (DeathFunc, CheckEndOfGame, GetGameDescriptor, InitServerMod, InitClientMod, ShowHelp,
 * ServerHidePlayer), the engine through the callbacks the modes set by name ("NFlagHunt.ServerTakeFlag").
 *
 * The functions the modes share are written once: those that use the module's own globals take the module name
 * (m) and a variant (v: 1 for the team version of a mode), the per-module entries only pass them on.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values and call the engine in C. The
 * variables of the modes stay in their module dicts, because the tests and Ffa read them and a mission or a test may
 * replace them: the menu entries (MultiplayerMenuOnGame, MultiplayerMenuSpectator), TeamGame, BotsList, the model
 * names, AUTO_RESPAWN_FLAG_TIME and the state of the flag modes (Flag, ZoneList, AcFlag or AcZone, FlagSpawnPos).
 * The C keeps the values it works on in the struct of each mode (nm_state) and reads them from the dict where the
 * script reads the global (OBJLIB 5). Flag and the zones of the postman modes are entity handles: they stay Python
 * objects until entities are native values (OBJLIB 7). The modes read and write the variables of Ffa (Scorer, the
 * team totals, ScorerFraglimit, MasterFlag; OBJLIB 5). The module code of NPostman and NTeamPost creates their flag
 * entity when the module loads; it stays in the .mod files.
 *
 * Direct calls (stage 3): the modes call their own functions through PORT_OWN (the global, a mission may replace
 * one; the shared code picks the site of its module, FL_OWN, PO_OWN, FP_OWN) and Ffa, Menu and VehiclesNPC through
 * PORT_CALL (scrap/scriptnative.h: the port seam). The functions given to the scheduler by name (Reborn,
 * CheckBotState, TryLinkFlagTo, RespawnFlag) take their entries (stage 4: Port_Scheduled). What stays by name:
 * TryGarage set into ShipEdit as a value (OBJLIB 5) and SAI.SetStateVehicle (OBJLIB 7).
 */
#include <limits.h>
#include <string.h>

#include "scrap/net.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

/* the language keys of the title and the help of each mode */
static const struct {
	const char *mod, *title, *help;
} nm_texts[] = {
	{ "NDeathMatch", "Net_Title_DeathMatch", "Net_Help_DeathMatch" },
	{ "NDeathMatchBots", "Net_Title_DeathMatch", "Net_Help_DeathMatch" },
	{ "NTeamDM", NULL, "Net_Help_TeamDM" },
	{ "NTeamFlag", NULL, "Net_Help_TeamFlag" },
	{ "NFlagHunt", "Net_Title_FlagHunt", "Net_Help_FlagHunt" },
	{ "NPostman", "Net_Title_Postman", "Net_Help_Postman" },
	{ "NTeamPost", NULL, "Net_Help_TeamPost" },
};
#define NM_MODES (int)(sizeof(nm_texts) / sizeof(nm_texts[0]))

/* the values of each mode the C works on (OBJLIB 5: mirrored, the dict holds them between calls) */
typedef struct {
	nativeVal_t ac;			/* AcFlag or AcZone: the key of ZoneList the flag goes to */
	nativeVal_t zoneList;		/* the flag modes: the flag positions by zone (the postman zones are handles) */
	nativeVal_t flagSpawnPos;	/* the postman modes: the start of the flag */
} nmState_t;

static nmState_t nm_state[NM_MODES];

static int Nm_Index(const char *m) {
	for (int i = 0; i < NM_MODES; i++) {
		if (!strcmp(nm_texts[i].mod, m)) {
			return i;
		}
	}
	return 0;
}

static const char *Nm_Text(const char *m, int help) {
	return help ? nm_texts[Nm_Index(m)].help : nm_texts[Nm_Index(m)].title;
}

/* Ffa.<name> into *out (OBJLIB 5: a variable of Ffa) */
static byte Nm_Ffa(const char *name, nativeVal_t *out) {
	return Mode_ModVar("Ffa", name, out);
}

/* Ffa.Scorer[name] = Ffa.Scorer[name] + inc */
static byte Nm_ScorerAdd(const nativeVal_t *name, const nativeVal_t *inc) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sc = Mode_V(&t), *v = Mode_V(&t), *n = Mode_V(&t);
	return Mode_Done(&t, Nm_Ffa("Scorer", sc) && Native_Subscript(sc, name, v) && Native_Arith('+', v, inc, n) &&
		Mode_ModDictPut("Ffa", "Scorer", name, n));
}

/* `if Ffa.Scorer[name] > 0: Ffa.Scorer[name] = Ffa.Scorer[name] - 1` */
static byte Nm_ScorerDec(modeVals_t *t, const nativeVal_t *name) {
	nativeVal_t *sc = Mode_V(t), *v = Mode_V(t), *n = Mode_V(t);
	if (!Nm_Ffa("Scorer", sc) || !Native_Subscript(sc, name, v)) {
		return 0;
	}
	if (Native_Compare(v, Mode_I(t, 0)) <= 0) {
		return 1;
	}
	Native_Free(sc);
	Native_Free(v);
	return Nm_Ffa("Scorer", sc) && Native_Subscript(sc, name, v) && Native_Arith('-', v, Mode_I(t, 1), n) &&
		Mode_ModDictPut("Ffa", "Scorer", name, n);
}

/* the team total of the entity name: TotalRed when its TgType is "red", else TotalGreen; NULL with the error */
static const char *Nm_Total(modeVals_t *t, const nativeVal_t *name) {
	nativeVal_t *tg = Mode_V(t);
	if (!Mode_EntGet(name, "TgType", tg)) {
		return NULL;
	}
	return Native_IsStr(tg, "red") ? "TotalRed" : "TotalGreen";
}

/* Ffa.<total> = Ffa.<total> + inc */
static byte Nm_TotalAddTo(modeVals_t *t, const char *total, const nativeVal_t *inc) {
	nativeVal_t *v = Mode_V(t), *n = Mode_V(t);
	return Nm_Ffa(total, v) && Native_Arith('+', v, inc, n) && Mode_SetModVar("Ffa", total, n);
}

/* Ffa.TotalRed = Ffa.TotalRed + inc (or TotalGreen) by the TgType of name */
static byte Nm_TotalAdd(modeVals_t *t, const nativeVal_t *name, long inc) {
	const char *total = Nm_Total(t, name);
	return total && Nm_TotalAddTo(t, total, Mode_I(t, inc));
}

/* `int(Pos[0]),int(Pos[1]),int(Pos[2])` in backquotes */
static byte Nm_PosRepr(modeVals_t *t, const nativeVal_t *pos, nativeVal_t *out) {
	nativeVal_t *tup = Mode_V(t);
	Native_SetEmpty(tup, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		nativeVal_t *c = Mode_V(t);
		if (!Native_Subscript(pos, Mode_I(t, i), c) || !Native_ToInt(c, Native_Append(tup))) {
			return 0;
		}
	}
	nativeBuf_t b;
	Native_BufInit(&b);
	Native_Repr(&b, tup, NR_REPR);
	Native_SetText(out, b.p, b.len);
	Native_BufFree(&b);
	return 1;
}

static byte Nm_FlagGet(const char *m, const char *var, nativeVal_t *out) {
	PyObject *flag = Mode_GlobalPy(m, "Flag");
	byte ok = flag && Mode_HGet(flag, var, out);
	Py_XDECREF(flag);
	return ok;
}

/* Flag.SetModel(<model>) of the module */
static byte Nm_FlagModel(const char *m, const char *model) {
	PyObject *flag = Mode_GlobalPy(m, "Flag");
	byte ok = flag && Mode_HSetModel(flag, m, model);
	Py_XDECREF(flag);
	return ok;
}

/* Scrap.Set("CurrentWaypointName", Flag.Name) */
static byte Nm_WayToFlag(const char *m) {
	nativeVal_t name;
	Native_SetNone(&name);
	byte ok = Nm_FlagGet(m, "Name", &name) && Mode_GVarSet("CurrentWaypointName", &name);
	Native_Free(&name);
	return ok;
}

/* m is one of the postman modes (NPostman, NTeamPost) */
static byte Nm_Postman(const char *m) {
	return !strcmp(m, "NPostman") || !strcmp(m, "NTeamPost");
}

/* name(args) of the module m through its global (a mission may replace it: the port seam), one site per module: m a
 * flag mode (FL_OWN), a postman mode (PO_OWN) or one of the four (FP_OWN) */
#define FL_OWN(m, name, out, ...) \
	(!strcmp(m, "NTeamFlag") ? PORT_OWN(NTeamFlag, name, out, __VA_ARGS__) : PORT_OWN(NFlagHunt, name, out, __VA_ARGS__))
#define PO_OWN(m, name, out, ...) \
	(!strcmp(m, "NTeamPost") ? PORT_OWN(NTeamPost, name, out, __VA_ARGS__) : PORT_OWN(NPostman, name, out, __VA_ARGS__))
#define FP_OWN(m, name, out, ...) (Nm_Postman(m) ? PO_OWN(m, name, out, __VA_ARGS__) : FL_OWN(m, name, out, __VA_ARGS__))

/* ---- the functions every mode has ---- */

/* CheckEndOfGame() of the free for all modes: a player has the frag limit, the server ends the game */
static byte Nm_CheckEndOfGameC(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *keys = Mode_V(&t);
	if (!Mode_Import("Ffa") || !Mode_Keys("Ffa", "Scorer", 0, keys)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(keys);
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *usr = Mode_V(&t), *sc = Mode_V(&t), *score = Mode_V(&t), *lim = Mode_V(&t);
		if (!Native_SeqItem(keys, i, usr) || !Nm_Ffa("Scorer", sc) || !Native_Subscript(sc, usr, score) ||
			!Nm_Ffa("ScorerFraglimit", lim)) {
			return Mode_Done(&t, 0);
		}
		if (Native_Compare(score, lim) >= 0) {
			Native_SetInt(out, 1);
			return Mode_Done(&t, PORT_CALL(Ffa, ServerEndOfGame, NULL, ""));
		}
		Mode_Release(&t, mark);
	}
	Native_SetInt(out, 0);
	return Mode_Done(&t, n >= 0);
}

/* CheckEndOfGame() of the team modes: a team has the frag limit */
static byte Nm_CheckEndOfTeamGameC(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *red = Mode_V(&t), *green = Mode_V(&t), *lim = Mode_V(&t);
	if (!Mode_Import("Ffa") || !Nm_Ffa("TotalRed", red) || !Nm_Ffa("ScorerFraglimit", lim)) {
		return Mode_Done(&t, 0);
	}
	int end = Native_Compare(red, lim) >= 0;
	if (!end) {
		Native_Free(lim);
		if (!Nm_Ffa("TotalGreen", green) || !Nm_Ffa("ScorerFraglimit", lim)) {
			return Mode_Done(&t, 0);
		}
		end = Native_Compare(green, lim) >= 0;
	}
	Native_SetInt(out, end);
	return Mode_Done(&t, !end || PORT_CALL(Ffa, ServerEndOfGame, NULL, ""));
}

/* DeathFunc(WinnerName, LooserName, inc) of NDeathMatch and NTeamPost: the winner scores */
static byte Nm_DeathFuncC(const nativeVal_t *a) {
	return Mode_Import("Ffa") && Nm_ScorerAdd(&a[0], &a[2]);
}

/* GetGameDescriptor() of the team modes: the score of the teams */
static byte Nm_TeamDescriptorC(nativeVal_t *out) {
	return Mode_Import("Ffa") && PORT_CALL(Ffa, GetGameTeamDescriptor, out, "");
}

/* GetGameDescriptor(): the title of the mode */
static byte Nm_GetGameDescriptorC(const char *m, nativeVal_t *out) {
	Mode_Lang(Nm_Text(m, 0), out);
	return 1;
}

/* ShowHelp(id): the help of the mode in a warning box */
static byte Nm_ShowHelpC(const char *m, const nativeVal_t *id) {
	nativeVal_t text;
	if (!Mode_Import("Menu")) {
		return 0;
	}
	Mode_Lang(Nm_Text(m, 1), &text);
	byte ok = PORT_CALL(Menu, ShowWarning, NULL, "vv", id, &text);
	Native_Free(&text);
	return ok;
}

/* ---- NTeamDM ---- */

/* DeathFunc(WinnerName, LooserName, inc): the winner and its team score */
static byte NTeamDM_DeathFuncC(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	if (!Mode_Import("Ffa") || !Nm_ScorerAdd(&a[0], &a[2])) {
		return 0;
	}
	const char *total = Nm_Total(&t, &a[0]);
	return Mode_Done(&t, total && Nm_TotalAddTo(&t, total, &a[2]));
}

/* ---- NDeathMatchBots ---- */

/* func(args) through the scheduler at Scrap.GetTime() + delay (delay a value), func the global of the module */
static byte Nm_Later(const char *m, const nativeVal_t *delay, const char *func, const nativeVal_t *args, int n,
	const char *name) {
	nativeVal_t now, tm, nm;
	PyObject *fn = Mode_GlobalPy(m, func);
	if (!fn) {
		return 0;
	}
	Native_SetFloat(&now, Mode_Time());
	byte ok = Native_Arith('+', &now, delay, &tm);
	if (ok) {
		if (name) {
			Native_SetStr(&nm, name);
		}
		ok = Mode_Schedule(&tm, fn, args, n, name ? &nm : NULL);
	}
	Py_DECREF(fn);
	return ok;
}

/* DeathFunc(WinnerName, LooserName, inc): a dead bot is reborn after 3 s; the winner scores */
static byte Bots_DeathFuncC(const char *m, const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	cEntity *e;
	nativeVal_t *ai = Mode_V(&t), *rb = Mode_V(&t);
	if (!Mode_Import("Ffa") || !Mode_Ent(&a[1], &e) || !Mode_Get(e, Native_Text(&a[1]), "isAI", ai)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(ai)) {
		/* AddScheduledFunc(Scrap.GetTime() + 3, Reborn, (LooserName, e.MaxLife)) */
		PyObject *fn = Mode_GlobalPy(m, "Reborn");
		nativeVal_t *tm = Mode_V(&t);
		Native_SetFloat(tm, Mode_Time() + 3.0);
		Native_SetEmpty(rb, NV_LIST);
		Native_Copy(Native_Append(rb), &a[1]);
		byte ok = fn && Mode_Get(e, Native_Text(&a[1]), "MaxLife", Native_Append(rb)) &&
			Mode_Schedule(tm, fn, rb->items, 2, NULL) && Mode_SetI(e, Native_Text(&a[1]), "Active", 0);
		Py_XDECREF(fn);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Nm_ScorerAdd(&a[0], &a[2]));
}

/* Reborn(LooserName, Life): the bot at a spawn point with its life, hunting again */
static byte Bots_RebornC(const nativeVal_t *a) {
	return Mode_Import("Ffa") && PORT_CALL(Ffa, SetSpawnPos, NULL, "v", &a[0]) && Mode_EntSet(&a[0], "Life", &a[1]) &&
		PORT_OWN(NDeathMatchBots, SetBotState, NULL, "v", &a[0]);
}

/* TryGarage(id, control): the garage is closed on a server with bots */
static byte Bots_TryGarageC(const nativeVal_t *a) {
	nativeVal_t text;
	Native_SetStr(&text, "Not in server :-(");
	return Mode_ScorerSet(&a[0], &a[1], "Text", &text);
}

/* SetBotState(name): the bot attacks a random other active ship */
static byte Bots_SetBotStateC(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = Mode_V(&t), *r = Mode_V(&t), *idx = Mode_V(&t), *target = Mode_V(&t);
	Native_SetEmpty(l, NV_LIST);
	/* e = Scrap.GetFirst(); while e: ...; e = Scrap.GetEntity(e.NextInSlot) */
	cEntity *e = cWorld_GetFirstEntity(g_pWorld);
	while (e) {
		int mark = t.n;
		nativeVal_t *act = Mode_V(&t), *n = Mode_V(&t);
		if (Entity_IsType(e, "Car")) {
			if (!Native_EntVar(e, e->name, "Active", act)) {
				return Mode_Done(&t, 0);
			}
			if (!Native_Compare(act, Mode_I(&t, 1))) {
				if (!Native_EntVar(e, e->name, "Name", n)) {
					return Mode_Done(&t, 0);
				}
				if (Native_Compare(n, name)) {
					if (!Native_EntVar(e, e->name, "Name", Native_Append(l))) {
						return Mode_Done(&t, 0);
					}
				}
			}
		}
		if (!Mode_NextInSlot(&e)) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
	}
	if (l->n > 0) {
		if (!Mode_RandD(0.0, (double)l->n - 0.05, r) || !Native_ToInt(r, idx) || !Native_Subscript(l, idx, target)) {
			return Mode_Done(&t, 0);
		}
		/* OBJLIB 7: the state handlers of SAI take Python arguments */
		return Mode_Done(&t, Native_CallMod("SAI", "SetStateVehicle", NULL, "ivv", 2L, name, target));
	}
	return Mode_Done(&t, 1);
}

/* CheckBotState(name): a bot that stopped (state 0 or 3) gets a new target; again in 1.5 s */
static byte Bots_CheckBotStateC(const char *m, const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *state = Mode_V(&t), *delay = Mode_V(&t);
	if (!Mode_AIState(name, state)) {
		return Mode_Done(&t, 0);
	}
	if ((!Native_Compare(state, Mode_I(&t, 0)) || !Native_Compare(state, Mode_I(&t, 3))) &&
		!PORT_OWN(NDeathMatchBots, SetBotState, NULL, "v", name)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(delay, 1.5);
	return Mode_Done(&t, Nm_Later(m, delay, "CheckBotState", name, 1, NULL));
}

/* InitServerMod(): no garage; the bots of BotsList join with their ships, AI profiles, 100 life and a nuke */
static byte Bots_InitServerModC(const char *m) {
	static const char *const mods[] = { "Ffa", "VehiclesNPC", "ShipEdit", "ShipMaker", "string" };
	modeVals_t t = { .n = 0 };
	for (int i = 0; i < 5; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	/* ShipEdit.EnterEditShipMenu = TryGarage (OBJLIB 5: a function of the module as a value; the calls of
	 * EnterEditShipMenu through its ref take the slow path to it) */
	PyObject *fn = Mode_GlobalPy(m, "TryGarage"), *se = fn ? Script_Module("ShipEdit") : NULL;
	int bad = !se || PyObject_SetAttrString(se, "EnterEditShipMenu", fn) < 0;
	Py_XDECREF(se);
	Py_XDECREF(fn);
	nativeVal_t *bots = Mode_V(&t);
	if (bad || !Mode_G(m, "BotsList", bots)) {
		return Mode_Done(&t, 0);
	}
	/* for i in range(len(BotsList)) */
	int count = Native_Size(bots);
	if (count < 0) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < count; i++) {
		int mark = t.n;
		nativeVal_t *list = Mode_V(&t), *bot = Mode_V(&t), *b0 = Mode_V(&t), *b1 = Mode_V(&t), *b2 = Mode_V(&t);
		nativeVal_t *key = Mode_V(&t), *ships = Mode_V(&t), *ship = Mode_V(&t), *pilots = Mode_V(&t);
		nativeVal_t *botname = Mode_V(&t), *r = Mode_V(&t), *ri = Mode_V(&t), *pilot = Mode_V(&t);
		nativeVal_t *s[7], *wl = Mode_V(&t), *cut = Mode_V(&t);
		const nativeVal_t *zero = Mode_I(&t, 0), *life = Mode_I(&t, 100);
		nativeVal_t *weap = Mode_V(&t), *red = Mode_S(&t, "red");
		PyObject *prof = NULL, *aip = NULL, *kp = NULL, *handle;
		cEntity *ply;
		char nb[128];
		byte ok = Mode_G(m, "BotsList", list) && Native_Subscript(list, Mode_I(&t, i), bot) &&
			Native_Subscript(bot, Mode_I(&t, 1), b1) && Native_Upper(b1, key) && Mode_ModVar("ShipMaker", "StdShips", ships) &&
			Native_Subscript(ships, key, ship) && Mode_ModVar("ShipMaker", "PilotsList", pilots);
		int npilots = ok ? Native_Size(pilots) : -1;
		ok = npilots >= 0;
		/* pd = VehiclesNPC.GetIAProf(bot[1]): the profile object of VehiclesNPC, changed in place below (the object
		 * itself on both paths) */
		if (ok) {
			prof = PORT_ASK(VehiclesNPC, GetIAProf, "v", b1);
			ok = prof && Native_Subscript(bot, Mode_I(&t, 2), b2) && PORT_CALL(VehiclesNPC, SetAIProfile, NULL, "Ov", prof, b2);
		}
		if (ok) {
			PyObject *cad = PyInt_FromLong(0), *ang = PyFloat_FromDouble(3.14);
			ok = cad && ang && PyObject_SetAttrString(prof, "CadenceShootCM", cad) == 0 &&
				PyObject_SetAttrString(prof, "AngAttack", ang) == 0;
			Py_XDECREF(cad);
			Py_XDECREF(ang);
		}
		if (ok) {
			/* VehiclesNPC.AIProf[bot[2]] = pd */
			aip = Script_Attr("VehiclesNPC", "AIProf");
			kp = aip ? Native_ToPy(b2) : NULL;
			ok = kp && PyObject_SetItem(aip, kp, prof) == 0;
		}
		Py_XDECREF(kp);
		Py_XDECREF(aip);
		if (ok) {
			/* ply = Scrap.CreateEntity(SNet.GetBotName(), 0.0, 0.0, 0.0, "Car") */
			nativeVal_t *x = Mode_V(&t);
			Net_GetBotName(nb);
			Native_SetStr(botname, nb);
			Native_SetFloat(x, 0.0);
			ok = Mode_Create(botname, x, x, x, "Car", &ply);
		}
		/* the name the handle of the bot keeps */
		char pnb[0x80];
		const char *pn = ok && ply ? Str_Copy(pnb, ply->name, sizeof(pnb)), pnb : NULL;
		nativeVal_t *name = Mode_V(&t);
		if (ok) {
			ok = Mode_Get(ply, pn, "Name", name) && Native_Subscript(ship, Mode_I(&t, 6), s[0] = Mode_V(&t)) &&
				Native_Subscript(bot, Mode_I(&t, 0), b0) && Mode_RandD(0.0, (double)npilots - 0.05, r) && Native_ToInt(r, ri) &&
				Mode_ModVar("ShipMaker", "PilotsList", pilots) && Native_Subscript(pilots, ri, pilot);
			for (int j = 1; ok && j <= 5; j++) {
				ok = Native_Subscript(ship, Mode_I(&t, j), s[j] = Mode_V(&t));
			}
			/* Ffa.ClientJoin(ply.Name, bot[1], ship[6], bot[0], PilotsList[...], ship[1] .. ship[5], "red") */
			ok = ok && PORT_CALL(Ffa, ClientJoin, NULL, "vvvvvvvvvvv", name, b1, s[0], b0, pilot, s[1], s[2], s[3], s[4], s[5], red);
		}
		/* after the calls of other ports the handle of the bot finds its entity again (gone: the handle's error) */
		ok = ok && Mode_NameSet(pn, "Active", zero);
		if (ok) {
			/* the handle of the bot goes to VehiclesNPC (OBJLIB 7) */
			handle = Mode_Handle(World_FindEntity(pn));
			ok = PORT_CALL(VehiclesNPC, SetIAProf, NULL, "Ov", handle, b2);
			Py_DECREF(handle);
		}
		Native_Free(name);
		ok = ok && Mode_NameGet(pn, "Name", name) && PORT_CALL(Ffa, SetSpawnPos, NULL, "v", name) &&
			Mode_NameSet(pn, "Descriptor", b0) && Mode_NameSet(pn, "Life", life) && Mode_NameSet(pn, "MinLife", life) &&
			Mode_NameSet(pn, "MaxLife", life);
		/* ply.WeapList = ply.WeapList[0:len(ply.WeapList) - 1] + "1" */
		if (ok) {
			int len;
			ok = Mode_NameGet(pn, "WeapList", wl) && (len = Native_Size(wl)) >= 0 && Native_Slice(wl, 0, len - 1, cut) &&
				Native_Arith('+', cut, Mode_S(&t, "1"), weap) && Mode_NameSet(pn, "WeapList", weap);
		}
		if (ok) {
			/* SetBotState(ply.Name); AddScheduledFunc(Scrap.GetTime() + 2, CheckBotState, (ply.Name,)) */
			const nativeVal_t *delay = Mode_I(&t, 2);
			Native_Free(name);
			ok = Mode_NameGet(pn, "Name", name) && PORT_OWN(NDeathMatchBots, SetBotState, NULL, "v", name);
			Native_Free(name);
			ok = ok && Mode_NameGet(pn, "Name", name) && Nm_Later(m, delay, "CheckBotState", name, 1, NULL);
		}
		Py_XDECREF(prof);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, 1);
}

/* ---- NFlagHunt and NTeamFlag (v 1): a flag at a random DM_Multiplayer_Flag_ dummy, taking it scores ---- */

/* DeathFunc(WinnerName, LooserName, inc): the dead player loses a point (and its team in NTeamFlag) */
static byte Fl_DeathFuncC(int v, const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	if (!Mode_Import("Ffa") || !Nm_ScorerDec(&t, &a[1])) {
		return Mode_Done(&t, 0);
	}
	if (v) {
		const char *total = Nm_Total(&t, &a[1]);
		nativeVal_t *x = Mode_V(&t);
		if (!total || !Nm_Ffa(total, x)) {
			return Mode_Done(&t, 0);
		}
		if (Native_Compare(x, Mode_I(&t, 0)) > 0) {
			Native_Free(x);
			return Mode_Done(&t, Nm_TotalAddTo(&t, total, Mode_I(&t, -1)));
		}
	}
	return Mode_Done(&t, 1);
}

/* GetRndFlag() and GetRndZone() of the postman modes: a random key of ZoneList into the global ac, the first (or
 * the second) when it is the last one */
static byte Fl_RndKey(const char *m, const char *ac) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *slot = &nm_state[Nm_Index(m)].ac;
	nativeVal_t *k = Mode_V(&t), *last = Mode_V(&t), *n = Mode_V(&t), *i = Mode_V(&t), *key = Mode_V(&t);
	int len;
	if (!Mode_Keys(m, "ZoneList", 1, k) || !Mode_G(m, ac, last) || (len = Native_Size(k)) < 0 || !Mode_Rand(Mode_I(&t, 0), Mode_I(&t, len), n) ||
		!Native_ToInt(n, i) || !Native_Subscript(k, i, key) || !Mode_Put(m, ac, slot, key)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_Compare(slot, last)) {
		Native_Free(key);
		if (!Native_Subscript(k, Mode_I(&t, 0), key) || !Mode_Put(m, ac, slot, key)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_Compare(slot, last)) {
			Native_Free(key);
			if (!Native_Subscript(k, Mode_I(&t, 1), key) || !Mode_Put(m, ac, slot, key)) {
				return Mode_Done(&t, 0);
			}
		}
	}
	return Mode_Done(&t, 1);
}

/* BrowseAreas(): ZoneList, the positions of the DM_Multiplayer_Flag_ dummies by the rest of their names (NFlagHunt
 * prints each dummy) */
static byte Fl_BrowseAreasC(const char *m, int v) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *zones = &nm_state[Nm_Index(m)].zoneList, *empty = Mode_V(&t), *dummy = Mode_V(&t);
	Native_SetEmpty(empty, NV_DICT);
	if (!Mode_Put(m, "ZoneList", zones, empty)) {
		return Mode_Done(&t, 0);
	}
	Mode_DummyStart("DM_Multiplayer_Flag_*", 1);
	Mode_DummyNext(dummy);
	while (Native_True(dummy)) {
		int mark = t.n;
		nativeVal_t *pos = Mode_V(&t), *name = Mode_V(&t), *key = Mode_V(&t);
		if ((!v && !Mode_PrintV(dummy)) || !Native_Subscript(dummy, Mode_I(&t, 1), pos) || !Native_Subscript(dummy, Mode_I(&t, 0), name) ||
			!Native_Slice(name, 19, INT_MAX, key) || !Mode_DictPut(m, "ZoneList", zones, key, pos)) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
		Native_Free(dummy);
		Mode_DummyNext(dummy);
	}
	return Mode_Done(&t, 1);
}

/* the flag a mode's module code or CreateFlag made, m.Flag = Scrap.CreateEntity(...) */
static byte Fl_NewFlag(const char *m) {
	nativeVal_t name, zero;
	cEntity *e;
	Native_SetStr(&name, "FlagToBeHunted");
	Native_SetInt(&zero, 0);
	if (!Mode_Create(&name, &zero, &zero, &zero, "Item", &e)) {
		return 0;
	}
	/* OBJLIB 7: the module keeps the handle */
	return Script_SetGlobal(m, "Flag", Mode_Handle(e));
}

/* the rest of CreateFlag(isServer) of both kinds: the radius, the radar, the server's take callback and position
 * (pos the global of the position) or the client's message callback, the way point */
static byte Fl_FlagSetup(const char *m, int server, byte postman) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pos = Mode_V(&t);
	if (!Nm_FlagModel(m, "FlagModel") || !Mode_FlagSet(m, "Radius", Mode_I(&t, 2000)) || !Mode_FlagSet(m, "RadarShow", Mode_I(&t, 1))) {
		return Mode_Done(&t, 0);
	}
	if (server) {
		nmState_t *st = &nm_state[Nm_Index(m)];
		byte ok = postman || FL_OWN(m, GetRndFlag, NULL, "");
		ok = ok && Mode_FlagSet(m, "OnTake", Mode_Name(&t, m, "ServerTakeFlag"));
		if (postman) {
			ok = ok && Mode_Read(m, "FlagSpawnPos", &st->flagSpawnPos) && Mode_FlagSet(m, "Pos", &st->flagSpawnPos);
		} else {
			ok = ok && Mode_Read(m, "ZoneList", &st->zoneList) && Mode_Read(m, "AcFlag", &st->ac) &&
				Native_Subscript(&st->zoneList, &st->ac, pos) && Mode_FlagSet(m, "Pos", pos);
		}
		if (!ok || !Mode_GVarSet("OnClientJoin", Mode_Name(&t, m, "ClientJoin"))) {
			return Mode_Done(&t, 0);
		}
	} else if (!Mode_GVarSet("OnUsrString", Mode_Name(&t, m, "OnUsrClient"))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Nm_WayToFlag(m));
}

/* CreateFlag(isServer): the flag; the server puts it at a random zone and takes the joins */
static byte Fl_CreateFlagC(const char *m, const nativeVal_t *server) {
	return Fl_NewFlag(m) && Fl_FlagSetup(m, Native_True(server), 0);
}

/* `usr = Scrap.UsrEntity(0); usr and usr.TgType == Scrap.GetEntity(taker).TgType` (team) or `usr.Name == taker`: 1, 0
 * or -1 */
static int Fl_Mine(modeVals_t *t, cEntity *usr, const nativeVal_t *taker, byte team) {
	nativeVal_t *a = Mode_V(t), *b = Mode_V(t);
	if (!usr) {
		return 0;
	}
	if (team) {
		if (!Native_EntVar(usr, usr->name, "TgType", a) || !Mode_EntGet(taker, "TgType", b)) {
			return -1;
		}
		return !Native_Compare(a, b);
	}
	return Native_EntVar(usr, usr->name, "Name", a) ? !Native_Compare(a, taker) : -1;
}

/* TakeFlag(TakerName): the flag at the current zone, the sound of a taker that is the player (its team in
 * NTeamFlag) or else of an enemy */
static byte Fl_TakeFlagC(const char *m, int v, const nativeVal_t *taker) {
	modeVals_t t = { .n = 0 };
	nmState_t *st = &nm_state[Nm_Index(m)];
	nativeVal_t *pos = Mode_V(&t);
	if (!Mode_Import("Sound") || !Mode_Read(m, "ZoneList", &st->zoneList) || !Mode_Read(m, "AcFlag", &st->ac) ||
		!Native_Subscript(&st->zoneList, &st->ac, pos) || !Mode_FlagSet(m, "Pos", pos)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_Compare(taker, Mode_S(&t, ""))) {
		return Mode_Done(&t, 1);
	}
	int way = Fl_Mine(&t, Mode_Usr(), taker, (byte)v);
	return Mode_Done(&t, way >= 0 && (way ? Mode_Sound("WaySound", "WayVolume") : Mode_Sound("EneWaySound", "EneWayVolume")));
}

/* OnUsrClient(id, s) of the flag and postman modes: "F'zone','taker'" a flag taken, "D(x, y, z)" the flag dropped
 * (postman modes, drop set), the rest goes to Ffa */
static byte Fl_UsrClientC(const char *m, const char *ac, int drop, const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *c0 = Mode_V(&t), *rest = Mode_V(&t), *val = Mode_V(&t), *v = Mode_V(&t);
	if (!Mode_Import("Ffa") || !Native_Subscript(&a[1], Mode_I(&t, 0), c0)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_Compare(c0, Mode_S(&t, "F"))) {
		/* AcFlag, taker = eval(s[1:]) */
		nativeVal_t *two = Mode_V(&t);
		Mode_V(&t);
		if (!Native_Slice(&a[1], 1, INT_MAX, rest) || !Native_EvalText(m, rest, val) || !Mode_Unpack(val, 2, two)) {
			return Mode_Done(&t, 0);
		}
		return Mode_Done(&t, Mode_Put(m, ac, &nm_state[Nm_Index(m)].ac, &two[0]) && FP_OWN(m, TakeFlag, NULL, "v", &two[1]));
	}
	if (drop) {
		Native_Free(c0);
		if (!Native_Subscript(&a[1], Mode_I(&t, 0), c0)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_Compare(c0, Mode_S(&t, "D"))) {
			/* DropFlag(eval(s[1:])) through the global; the global is looked up after the argument, as at the other
			 * PORT_OWN sites (the script looked it up first: the same unless both fail) */
			return Mode_Done(&t, Native_Slice(&a[1], 1, INT_MAX, rest) && Native_EvalText(m, rest, v) &&
				PO_OWN(m, DropFlag, NULL, "v", v));
		}
	}
	return Mode_Done(&t, PORT_CALL(Ffa, OnUsrClient, NULL, "vv", &a[0], &a[1]));
}

/* Ffa.ClientJoin with the 11 arguments, then the client id of the entity name ("net5") into *id */
static byte Nm_Join(modeVals_t *t, const nativeVal_t *a, nativeVal_t *id) {
	nativeVal_t *num = Mode_V(t);
	return Mode_Import("Ffa") &&
		PORT_CALL(Ffa, ClientJoin, NULL, "vvvvvvvvvvv", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9],
			&a[10]) && Native_Slice(&a[0], 3, INT_MAX, num) && Native_ToInt(num, id);
}

/* ClientJoin(entityname, ModelName, MaxLife, UsrName, PilotName, Motor0..3, WeaponBay, TgType): the join of Ffa,
 * then the current zone to the new client */
static byte Fl_ClientJoinC(const char *m, const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *id = Mode_V(&t), *s = Mode_V(&t), *ac = &nm_state[Nm_Index(m)].ac;
	return Mode_Done(&t, Nm_Join(&t, a, id) && Mode_Read(m, "AcFlag", ac) && Mode_Cat(&t, s, 3, Mode_S(&t, "F'"), ac, Mode_S(&t, "',''")) &&
		Mode_Send(id, s));
}

/* ServerTakeFlag(ItemName, VehicleName): a player took the flag after the start: it scores (its team too in
 * NTeamFlag), the flag moves on and the clients hear of it; before the start a label says to wait */
static byte Fl_ServerTakeFlagC(const char *m, int v, const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *veh = (nativeVal_t *)&a[1], *started = Mode_V(&t), *s = Mode_V(&t), *end = Mode_V(&t);
	nativeVal_t *ac = &nm_state[Nm_Index(m)].ac;
	int has;
	if (!Mode_Import("Ffa") || (has = Mode_HasKey("Ffa", "Scorer", 0, veh)) < 0) {
		return Mode_Done(&t, 0);
	}
	if (!has) {
		return Mode_Done(&t, 1);
	}
	if (!PORT_CALL(Ffa, CheckGameStart, started, "")) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(started)) {
		return Mode_Done(&t, Mode_Label(0, "Net_Label_Waiting_Challenge", 1));
	}
	if (!Nm_ScorerAdd(veh, Mode_I(&t, 1)) || (v && !Nm_TotalAdd(&t, veh, 1)) || !FL_OWN(m, GetRndFlag, NULL, "") ||
		!Mode_Read(m, "AcFlag", ac) || !Mode_Cat(&t, s, 5, Mode_S(&t, "F'"), ac, Mode_S(&t, "','"), veh, Mode_S(&t, "'")) ||
		!Mode_Send(Mode_I(&t, -1), s) || !FL_OWN(m, TakeFlag, NULL, "v", veh) ||
		!PORT_CALL(Ffa, ActualizeGameScorer, NULL, "") || !PORT_CALL(Ffa, ServerSendPlayerStatus, NULL, "v", veh) ||
		!FL_OWN(m, CheckEndOfGame, end, "")) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !Native_True(end) || Mode_FlagSet(m, "OnTake", Mode_S(&t, "")));
}

/* InitServerMod(), InitClientMod() of the flag and postman modes (server; Ffa imported in the postman server, imp) */
static byte Fl_InitC(const char *m, int server, int imp) {
	return (!imp || Mode_Import("Ffa")) && FP_OWN(m, BrowseAreas, NULL, "") && FP_OWN(m, CreateFlag, NULL, "i", (long)server);
}

/* ---- NPostman and NTeamPost (v 1): one flag at DM_Domination_Flag, its taker carries it to a random zone ---- */

/* ServerHidePlayer(VehicleName): the flag drops when its carrier leaves */
static byte Po_ServerHidePlayerC(const char *m, const nativeVal_t *veh) {
	nativeVal_t mf;
	Native_SetNone(&mf);
	if (!Mode_Import("Ffa") || !Nm_Ffa("MasterFlag", &mf)) {
		return 0;
	}
	byte carried = !Native_Compare(&mf, veh);
	Native_Free(&mf);
	return !carried || PO_OWN(m, ServerDropFlag, NULL, "");
}

/* BrowseAreas(): FlagSpawnPos from DM_Domination_Flag; ZoneList, an item of radius 3000 at each DM_Domination_Zone_
 * dummy by the rest of its name (OBJLIB 7: the zones are handles in the dict) */
static byte Po_BrowseAreasC(const char *m) {
	modeVals_t t = { .n = 0 };
	nmState_t *st = &nm_state[Nm_Index(m)];
	nativeVal_t *dummy = Mode_V(&t), *pos = Mode_V(&t);
	Mode_DummyStart("DM_Domination_Flag", 0);
	Mode_DummyNext(dummy);
	if (!Native_Subscript(dummy, Mode_I(&t, 1), pos) || !Mode_Put(m, "FlagSpawnPos", &st->flagSpawnPos, pos)) {
		return Mode_Done(&t, 0);
	}
	PyObject *zones = PyDict_New();
	if (!zones || !Script_SetGlobal(m, "ZoneList", zones)) {
		return Mode_Done(&t, 0);
	}
	Mode_DummyStart("DM_Domination_Zone_*", 1);
	Native_Free(dummy);
	Mode_DummyNext(dummy);
	while (Native_True(dummy)) {
		int mark = t.n;
		nativeVal_t *name = Mode_V(&t), *at = Mode_V(&t), *key = Mode_V(&t);
		const nativeVal_t *zero = Mode_I(&t, 0);
		cEntity *e;
		if (!Native_Subscript(dummy, Mode_I(&t, 0), name) || !Mode_Create(name, zero, zero, zero, "Item", &e)) {
			return Mode_Done(&t, 0);
		}
		const char *en = e ? e->name : NULL;
		PyObject *h = NULL, *zl = NULL, *k = NULL;
		byte ok = Native_Subscript(dummy, Mode_I(&t, 1), at) && Mode_Set(e, en, "Pos", at) && Mode_SetI(World_FindEntity(en), en, "Radius", 3000);
		if (ok) {
			/* ZoneList[dummy[0][19:]] = ent */
			h = Mode_Handle(World_FindEntity(en));
			zl = Mode_GlobalPy(m, "ZoneList");
			ok = zl && Native_Subscript(dummy, Mode_I(&t, 0), name) && Native_Slice(name, 19, INT_MAX, key) && (k = Native_ToPy(key)) &&
				PyObject_SetItem(zl, k, h) == 0;
		}
		Py_XDECREF(k);
		Py_XDECREF(zl);
		Py_XDECREF(h);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
		Native_Free(dummy);
		Mode_DummyNext(dummy);
	}
	return Mode_Done(&t, 1);
}

/* CreateFlag(isServer): the flag of the module code with its models; the server puts it at FlagSpawnPos */
static byte Po_CreateFlagC(const char *m, const nativeVal_t *server) {
	nativeVal_t model;
	byte ok = Mode_G(m, "FlagPilotModel", &model) && Mode_Preload(&model);
	Native_Free(&model);
	ok = ok && Mode_G(m, "FlagNoUsrModel", &model) && Mode_Preload(&model);
	Native_Free(&model);
	return ok && Fl_FlagSetup(m, Native_True(server), 1);
}

/* LinkFlagTo(TakerName, Node): the flag onto the node of the taker (no older try left) */
static byte Po_LinkFlagToC(const char *m, const nativeVal_t *a) {
	return Mode_DelSchedS("TryLinkFlagTo") && PO_OWN(m, TryLinkFlagTo, NULL, "vv", &a[0], &a[1]);
}

/* TryLinkFlagTo(TakerName, Node): links the flag once the taker has a model, else tries again in 0.5 s */
static byte Po_TryLinkFlagToC(const char *m, const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *model = Mode_V(&t), *link = Mode_V(&t), *delay = Mode_V(&t);
	cEntity *e;
	if (!Mode_Ent(&a[0], &e)) {
		return Mode_Done(&t, 0);
	}
	if (e) {
		if (!Mode_Get(e, Native_Text(&a[0]), "Model", model)) {
			return Mode_Done(&t, 0);
		}
		if (Native_Compare(model, Mode_S(&t, ""))) {
			return Mode_Done(&t, Mode_Cat(&t, link, 3, &a[0], Mode_S(&t, ","), &a[1]) && Mode_FlagSet(m, "LinkModel", link));
		}
	}
	Native_SetFloat(delay, 0.5);
	return Mode_Done(&t, Nm_Later(m, delay, "TryLinkFlagTo", a, 2, "TryLinkFlagTo"));
}

/* ZoneList[AcZone].var = v (OBJLIB 7: the zones are entity handles in the dict) */
static byte Po_ZoneSet(const char *m, const char *var, const nativeVal_t *v) {
	nativeVal_t *ac = &nm_state[Nm_Index(m)].ac;
	return Mode_Read(m, "AcZone", ac) && Mode_AtSet(m, "ZoneList", ac, var, v);
}

/* Scrap.Set("CurrentWaypointName", ZoneList[AcZone].Name) */
static byte Po_WayToZone(const char *m) {
	nativeVal_t name, *ac = &nm_state[Nm_Index(m)].ac;
	Native_SetNone(&name);
	byte ok = Mode_Read(m, "AcZone", ac) && Mode_AtGet(m, "ZoneList", ac, "Name", &name) &&
		Mode_GVarSet("CurrentWaypointName", &name);
	Native_Free(&name);
	return ok;
}

/* TakeFlag(TakerName): the taker carries the flag to the current zone; the sounds, waypoint and messages of the
 * player who took it, (NTeamPost) of its team, of the enemies */
static byte Po_TakeFlagC(const char *m, int v, const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *mf = Mode_V(&t), *uc = Mode_V(&t);
	cEntity *taker;
	if (!Mode_Ent(name, &taker)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_Compare(name, Mode_S(&t, "")) || !taker) {
		return Mode_Done(&t, 1);
	}
	const char *tn = Native_Text(name);
	if (!Mode_Import("Sound") || !Mode_Import("Ffa") || !PO_OWN(m, AbortZoneKeys, NULL, "") || !Po_ZoneSet(m, "RadarShow", Mode_I(&t, 2)) ||
		!Nm_Ffa("MasterFlag", mf)) {
		return Mode_Done(&t, 0);
	}
	int take = Native_Compare(mf, name) != 0;
	if (take) {
		nativeVal_t *zero = Mode_V(&t);
		Native_SetEmpty(zero, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			Native_SetInt(Native_Append(zero), 0);
		}
		/* the handle of the taker, held since the start of the function */
		if (!Mode_SetModVar("Ffa", "MasterFlag", name) || !Mode_NameGet(tn, "UsrControl", uc) ||
			!Nm_FlagModel(m, !Native_Compare(uc, Mode_I(&t, 0)) ? "FlagNoUsrModel" : "FlagPilotModel") ||
			!PO_OWN(m, LinkFlagTo, NULL, "vs", name, "DC_Flag_Player") || !Mode_FlagSet(m, "OffModPos", zero) ||
			!Mode_FlagSet(m, "DeltaOffMul", Mode_I(&t, 4)) || !Mode_FlagSet(m, "OffMul", Mode_I(&t, 0)) ||
			!Mode_FlagSet(m, "UseLinkAng", Mode_I(&t, 1))) {
			return Mode_Done(&t, 0);
		}
	}
	cEntity *usr = Mode_Usr();
	int me = Fl_Mine(&t, usr, name, 0);
	if (me < 0) {
		return Mode_Done(&t, 0);
	}
	byte ok = 1;
	if (me) {
		ok = v ? Po_WayToZone(m) && Mode_Sound("WaySound", "WayVolume") : Mode_Sound("WaySound", "WayVolume") && Po_WayToZone(m);
		ok = ok && (!take || Mode_Label(1, "Net_Msg_You_HaveFlag", 5));
	} else {
		int team = v ? Fl_Mine(&t, usr, name, 1) : 0;
		if (team < 0) {
			return Mode_Done(&t, 0);
		}
		if (team) {
			ok = Mode_Sound("WaySound", "WayVolume") && (!take || Mode_Label(1, "Net_Msg_Your_TeamFlag", 5));
		} else {
			ok = Mode_Sound("EneWaySound", "EneWayVolume") && (!v || !take || Mode_Label(1, "Net_Msg_Enemy_TeamFlag", 5));
		}
	}
	return Mode_Done(&t, ok && PORT_CALL(Ffa, ActualizeGameScorer, NULL, ""));
}

/* DropFlag(Pos): the flag alone at Pos with nobody carrying it, the zones off */
static byte Po_DropFlagC(const char *m, int v, const nativeVal_t *pos) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rt = Mode_V(&t), *none = Mode_V(&t);
	Native_SetFloat(rt, 0.2);
	return Mode_Done(&t, Mode_Import("Sound") && Mode_Import("Ffa") && Mode_FlagSet(m, "LinkModel", Mode_S(&t, ",")) &&
		(!v || Nm_FlagModel(m, "FlagModel")) && Mode_FlagSet(m, "Pos", pos) && Mode_FlagSet(m, "rTime", rt) &&
		Mode_SetModVar("Ffa", "MasterFlag", none) && (v || Nm_FlagModel(m, "FlagModel")) && Nm_WayToFlag(m) &&
		Mode_Sound("EneWaySound", "EneWayVolume") && PO_OWN(m, AbortZoneKeys, NULL, "") &&
		PORT_CALL(Ffa, ActualizeGameScorer, NULL, ""));
}

/* AbortZoneKeys(): no zone on the radar or taking */
static byte Po_AbortZoneKeysC(const char *m) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *keys = Mode_V(&t), *none = Mode_S(&t, "");
	const nativeVal_t *off = Mode_I(&t, 0);
	if (!Mode_Keys(m, "ZoneList", 1, keys)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(keys);
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *idx = Mode_V(&t);
		if (!Native_SeqItem(keys, i, idx) || !Mode_AtSet(m, "ZoneList", idx, "RadarShow", off) ||
			!Mode_AtSet(m, "ZoneList", idx, "OnTake", none)) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, n >= 0);
}

/* "D" + `int(Pos[0]),int(Pos[1]),int(Pos[2])` to all clients */
static byte Po_SendDrop(modeVals_t *t, const nativeVal_t *pos) {
	nativeVal_t *rep = Mode_V(t), *s = Mode_V(t);
	return Nm_PosRepr(t, pos, rep) && Mode_Cat(t, s, 2, Mode_S(t, "D"), rep) && Mode_Send(Mode_I(t, -1), s);
}

/* ClientJoin(...): the join of Ffa, then the carrier and zone to the new client, or the flag's place to all */
static byte Po_ClientJoinC(const char *m, const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *id = Mode_V(&t), *mf = Mode_V(&t), *s = Mode_V(&t), *pos = Mode_V(&t), *ac = &nm_state[Nm_Index(m)].ac;
	if (!Nm_Join(&t, a, id) || !Nm_Ffa("MasterFlag", mf)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(mf)) {
		Native_Free(mf);
		return Mode_Done(&t, Mode_Read(m, "AcZone", ac) && Nm_Ffa("MasterFlag", mf) &&
			Mode_Cat(&t, s, 5, Mode_S(&t, "F'"), ac, Mode_S(&t, "','"), mf, Mode_S(&t, "'")) && Mode_Send(id, s));
	}
	return Mode_Done(&t, Nm_FlagGet(m, "Pos", pos) && Po_SendDrop(&t, pos));
}

/* ServerTakeBase(ItemName, VehicleName): the carrier reached the zone: it scores (NTeamPost: 10 and a point for
 * its team) and takes the flag again for the next zone */
static byte Po_ServerTakeBaseC(const char *m, int v, const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *mf = Mode_V(&t), *fname = Mode_V(&t);
	if (!Mode_Import("Ffa") || !Nm_Ffa("MasterFlag", mf)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(mf, &a[1])) {
		return Mode_Done(&t, 1);
	}
	/* ServerTakeFlag(Flag.Name, VehicleName): the global is looked up after Flag.Name, as at the other PORT_OWN sites
	 * (the script looked it up first: the same unless both fail) */
	return Mode_Done(&t, Nm_ScorerAdd(&a[1], Mode_I(&t, v ? 10 : 1)) && (!v || Nm_TotalAdd(&t, &a[1], 1)) &&
		PORT_CALL(Ffa, ActualizeGameScorer, NULL, "") && PORT_CALL(Ffa, ServerSendPlayerStatus, NULL, "v", &a[1]) &&
		Nm_FlagGet(m, "Name", fname) && PO_OWN(m, ServerTakeFlag, NULL, "vv", fname, &a[1]) && PO_OWN(m, CheckEndOfGame, NULL, ""));
}

/* ServerDropFlag(): the flag drops where its carrier is (back to the start in AUTO_RESPAWN_FLAG_TIME) or at
 * FlagSpawnPos, for all clients */
static byte Po_ServerDropFlagC(const char *m) {
	modeVals_t t = { .n = 0 };
	nmState_t *st = &nm_state[Nm_Index(m)];
	nativeVal_t *mf = Mode_V(&t), *pos = Mode_V(&t), *delay = Mode_V(&t);
	if (!Mode_Import("Ffa") || !Nm_Ffa("MasterFlag", mf)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(mf)) {
		Native_Free(mf);
		if (!Nm_Ffa("MasterFlag", mf) || !Mode_EntGet(mf, "Pos", pos) || !Mode_G(m, "AUTO_RESPAWN_FLAG_TIME", delay) ||
			!Nm_Later(m, delay, "RespawnFlag", NULL, 0, "RespawnMainFlag")) {
			return Mode_Done(&t, 0);
		}
	} else if (!Mode_DelSchedS("RespawnMainFlag") || !Mode_Read(m, "FlagSpawnPos", &st->flagSpawnPos)) {
		return Mode_Done(&t, 0);
	} else {
		Native_Copy(pos, &st->flagSpawnPos);
	}
	return Mode_Done(&t, Po_SendDrop(&t, pos) && Mode_FlagSet(m, "OnTake", Mode_Name(&t, m, "ServerTakeFlag")) &&
		PO_OWN(m, DropFlag, NULL, "v", pos));
}

/* ServerTakeFlag(ItemName, VehicleName): a player took the flag after the start: a new zone takes it, for all
 * clients; before the start a label says to wait */
static byte Po_ServerTakeFlagC(const char *m, const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *veh = (nativeVal_t *)&a[1], *started = Mode_V(&t), *s = Mode_V(&t), *ac = &nm_state[Nm_Index(m)].ac;
	int has;
	if (!Mode_Import("Ffa") || (has = Mode_HasKey("Ffa", "Scorer", 0, veh)) < 0) {
		return Mode_Done(&t, 0);
	}
	if (!has) {
		return Mode_Done(&t, 1);
	}
	if (!PORT_CALL(Ffa, CheckGameStart, started, "")) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(started)) {
		return Mode_Done(&t, Mode_Label(0, "Net_Label_Waiting_Challenge", 1));
	}
	return Mode_Done(&t, Mode_DelSchedS("RespawnMainFlag") && PO_OWN(m, GetRndZone, NULL, "") && Mode_Read(m, "AcZone", ac) &&
		Mode_Cat(&t, s, 5, Mode_S(&t, "F'"), ac, Mode_S(&t, "','"), veh, Mode_S(&t, "'")) && Mode_Send(Mode_I(&t, -1), s) &&
		PO_OWN(m, TakeFlag, NULL, "v", veh) && Po_ZoneSet(m, "OnTake", Mode_Name(&t, m, "ServerTakeBase")) &&
		Mode_FlagSet(m, "OnTake", Mode_S(&t, "")));
}

/* RespawnFlag(): nobody carries the flag, it goes back to the start */
static byte Po_RespawnFlagC(const char *m) {
	nativeVal_t none;
	Native_SetNone(&none);
	return Mode_Import("Ffa") && Mode_SetModVar("Ffa", "MasterFlag", &none) && PO_OWN(m, ServerDropFlag, NULL, "");
}

/* ---- the adapters of the module tables (OBJLIB 7: arguments, results and the errors of the Python calls) ---- */

/* the entry of module mod for a function that does nothing with its n arguments */
#define NM_PASS(mod, name, nargs) \
	static PyObject *mod##_##name(PyObject *self, PyObject *args) { \
		static const char fmts[4][4] = { "", "O", "OO", "OOO" }; \
		PyObject *o[3]; \
		(void)self; \
		return PyArg_ParseTuple(args, (char *)fmts[nargs], &o[0], &o[1], &o[2]) ? Py_ReturnNone() : NULL; \
	}

#define NM_ENTRY(mod, name) { #name, mod##_##name, 1, NULL },

/* the functions every mode has, those of the free for all and of the team modes, the ones that do nothing */
#define NM_COMMON(mod) MODE_FN(mod, ShowHelp, 1, Nm_ShowHelpC(#mod, &a[0]))
#define NM_FFAEND(mod) \
	MODE_FN(mod, GetGameDescriptor, 0, Nm_GetGameDescriptorC(#mod, out)) \
	MODE_FN(mod, CheckEndOfGame, 0, Nm_CheckEndOfGameC(out))
#define NM_TEAMEND(mod) \
	MODE_FN(mod, CheckEndOfTeamGame, 0, Nm_CheckEndOfTeamGameC(out)) \
	MODE_FN(mod, TeamDescriptor, 0, Nm_TeamDescriptorC(out))

NM_COMMON(NDeathMatch)
NM_FFAEND(NDeathMatch)
NM_PASS(NDeathMatch, InitClientModPass, 0)
NM_PASS(NDeathMatch, ServerHidePlayerPass, 1)
MODE_FN(NDeathMatch, DeathFunc, 3, Nm_DeathFuncC(a))

PyMethodDef g_NDeathMatchMethods[] = {
	NM_ENTRY(NDeathMatch, DeathFunc)
	NM_ENTRY(NDeathMatch, CheckEndOfGame)
	NM_ENTRY(NDeathMatch, GetGameDescriptor)
	{ "InitServerMod", NDeathMatch_InitClientModPass, 1, NULL },
	{ "InitClientMod", NDeathMatch_InitClientModPass, 1, NULL },
	NM_ENTRY(NDeathMatch, ShowHelp)
	{ "ServerHidePlayer", NDeathMatch_ServerHidePlayerPass, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

NM_COMMON(NDeathMatchBots)
NM_FFAEND(NDeathMatchBots)
NM_PASS(NDeathMatchBots, InitClientModPass, 0)
NM_PASS(NDeathMatchBots, ServerHidePlayerPass, 1)
MODE_FN(NDeathMatchBots, DeathFunc, 3, Bots_DeathFuncC("NDeathMatchBots", a))
MODE_FN(NDeathMatchBots, Reborn, 2, Bots_RebornC(a))
MODE_FN(NDeathMatchBots, TryGarage, 2, Bots_TryGarageC(a))
MODE_FN(NDeathMatchBots, SetBotState, 1, Bots_SetBotStateC(&a[0]))
MODE_FN(NDeathMatchBots, CheckBotState, 1, Bots_CheckBotStateC("NDeathMatchBots", &a[0]))
MODE_FN(NDeathMatchBots, InitServerMod, 0, Bots_InitServerModC("NDeathMatchBots"))

PyMethodDef g_NDeathMatchBotsMethods[] = {
	NM_ENTRY(NDeathMatchBots, DeathFunc)
	NM_ENTRY(NDeathMatchBots, Reborn)
	NM_ENTRY(NDeathMatchBots, CheckEndOfGame)
	NM_ENTRY(NDeathMatchBots, GetGameDescriptor)
	NM_ENTRY(NDeathMatchBots, TryGarage)
	NM_ENTRY(NDeathMatchBots, SetBotState)
	NM_ENTRY(NDeathMatchBots, CheckBotState)
	NM_ENTRY(NDeathMatchBots, InitServerMod)
	{ "InitClientMod", NDeathMatchBots_InitClientModPass, 1, NULL },
	NM_ENTRY(NDeathMatchBots, ShowHelp)
	{ "ServerHidePlayer", NDeathMatchBots_ServerHidePlayerPass, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

NM_COMMON(NTeamDM)
NM_TEAMEND(NTeamDM)
NM_PASS(NTeamDM, InitClientModPass, 0)
NM_PASS(NTeamDM, ServerHidePlayerPass, 1)
MODE_FN(NTeamDM, DeathFunc, 3, NTeamDM_DeathFuncC(a))

PyMethodDef g_NTeamDMMethods[] = {
	NM_ENTRY(NTeamDM, DeathFunc)
	{ "CheckEndOfGame", NTeamDM_CheckEndOfTeamGame, 1, NULL },
	{ "GetGameDescriptor", NTeamDM_TeamDescriptor, 1, NULL },
	{ "InitServerMod", NTeamDM_InitClientModPass, 1, NULL },
	{ "InitClientMod", NTeamDM_InitClientModPass, 1, NULL },
	NM_ENTRY(NTeamDM, ShowHelp)
	{ "ServerHidePlayer", NTeamDM_ServerHidePlayerPass, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* the functions of a flag mode (v 1 the team version) */
#define FL_MODE(mod, v) \
	NM_COMMON(mod) \
	NM_PASS(mod, ServerHidePlayerPass, 1) \
	MODE_FN(mod, DeathFunc, 3, Fl_DeathFuncC(v, a)) \
	MODE_FN(mod, InitServerMod, 0, Fl_InitC(#mod, 1, 0)) \
	MODE_FN(mod, InitClientMod, 0, Fl_InitC(#mod, 0, 0)) \
	MODE_FN(mod, GetRndFlag, 0, Fl_RndKey(#mod, "AcFlag")) \
	MODE_FN(mod, BrowseAreas, 0, Fl_BrowseAreasC(#mod, v)) \
	MODE_FN(mod, CreateFlag, 1, Fl_CreateFlagC(#mod, &a[0])) \
	MODE_FN(mod, TakeFlag, 1, Fl_TakeFlagC(#mod, v, &a[0])) \
	MODE_FN(mod, OnUsrClient, 2, Fl_UsrClientC(#mod, "AcFlag", 0, a)) \
	MODE_FN(mod, ClientJoin, 11, Fl_ClientJoinC(#mod, a)) \
	MODE_FN(mod, ServerTakeFlag, 2, Fl_ServerTakeFlagC(#mod, v, a))
#define FL_ENTRIES(mod) \
	NM_ENTRY(mod, DeathFunc) \
	NM_ENTRY(mod, InitServerMod) \
	NM_ENTRY(mod, InitClientMod) \
	NM_ENTRY(mod, ShowHelp) \
	{ "ServerHidePlayer", mod##_ServerHidePlayerPass, 1, NULL }, \
	NM_ENTRY(mod, GetRndFlag) \
	NM_ENTRY(mod, BrowseAreas) \
	NM_ENTRY(mod, CreateFlag) \
	NM_ENTRY(mod, TakeFlag) \
	NM_ENTRY(mod, OnUsrClient) \
	NM_ENTRY(mod, ClientJoin) \
	NM_ENTRY(mod, ServerTakeFlag)

FL_MODE(NFlagHunt, 0)
NM_FFAEND(NFlagHunt)

PyMethodDef g_NFlagHuntMethods[] = {
	FL_ENTRIES(NFlagHunt)
	NM_ENTRY(NFlagHunt, CheckEndOfGame)
	NM_ENTRY(NFlagHunt, GetGameDescriptor)
	{ NULL, NULL, 0, NULL },
};

FL_MODE(NTeamFlag, 1)
NM_TEAMEND(NTeamFlag)

PyMethodDef g_NTeamFlagMethods[] = {
	FL_ENTRIES(NTeamFlag)
	{ "CheckEndOfGame", NTeamFlag_CheckEndOfTeamGame, 1, NULL },
	{ "GetGameDescriptor", NTeamFlag_TeamDescriptor, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* DeathFunc of NPostman does nothing, NTeamPost scores the winner */
#define PO_DEATH_0(mod) NM_PASS(mod, DeathFunc, 3)
#define PO_DEATH_1(mod) MODE_FN(mod, DeathFunc, 3, Nm_DeathFuncC(a))

/* the functions of a postman mode (v 1 the team version) */
#define PO_MODE(mod, v) \
	NM_COMMON(mod) \
	PO_DEATH_##v(mod) \
	MODE_FN(mod, InitServerMod, 0, Fl_InitC(#mod, 1, 1)) \
	MODE_FN(mod, InitClientMod, 0, Fl_InitC(#mod, 0, 0)) \
	MODE_FN(mod, ServerHidePlayer, 1, Po_ServerHidePlayerC(#mod, &a[0])) \
	MODE_FN(mod, GetRndZone, 0, Fl_RndKey(#mod, "AcZone")) \
	MODE_FN(mod, BrowseAreas, 0, Po_BrowseAreasC(#mod)) \
	MODE_FN(mod, CreateFlag, 1, Po_CreateFlagC(#mod, &a[0])) \
	MODE_FN(mod, LinkFlagTo, 2, Po_LinkFlagToC(#mod, a)) \
	MODE_FN(mod, TryLinkFlagTo, 2, Po_TryLinkFlagToC(#mod, a)) \
	MODE_FN(mod, TakeFlag, 1, Po_TakeFlagC(#mod, v, &a[0])) \
	MODE_FN(mod, DropFlag, 1, Po_DropFlagC(#mod, v, &a[0])) \
	MODE_FN(mod, AbortZoneKeys, 0, Po_AbortZoneKeysC(#mod)) \
	MODE_FN(mod, OnUsrClient, 2, Fl_UsrClientC(#mod, "AcZone", 1, a)) \
	MODE_FN(mod, ClientJoin, 11, Po_ClientJoinC(#mod, a)) \
	MODE_FN(mod, ServerTakeBase, 2, Po_ServerTakeBaseC(#mod, v, a)) \
	MODE_FN(mod, ServerDropFlag, 0, Po_ServerDropFlagC(#mod)) \
	MODE_FN(mod, ServerTakeFlag, 2, Po_ServerTakeFlagC(#mod, a)) \
	MODE_FN(mod, RespawnFlag, 0, Po_RespawnFlagC(#mod))
#define PO_ENTRIES(mod) \
	NM_ENTRY(mod, DeathFunc) \
	NM_ENTRY(mod, InitServerMod) \
	NM_ENTRY(mod, InitClientMod) \
	NM_ENTRY(mod, ShowHelp) \
	NM_ENTRY(mod, ServerHidePlayer) \
	NM_ENTRY(mod, GetRndZone) \
	NM_ENTRY(mod, BrowseAreas) \
	NM_ENTRY(mod, CreateFlag) \
	NM_ENTRY(mod, LinkFlagTo) \
	NM_ENTRY(mod, TryLinkFlagTo) \
	NM_ENTRY(mod, TakeFlag) \
	NM_ENTRY(mod, DropFlag) \
	NM_ENTRY(mod, AbortZoneKeys) \
	NM_ENTRY(mod, OnUsrClient) \
	NM_ENTRY(mod, ClientJoin) \
	NM_ENTRY(mod, ServerTakeBase) \
	NM_ENTRY(mod, ServerDropFlag) \
	NM_ENTRY(mod, ServerTakeFlag) \
	NM_ENTRY(mod, RespawnFlag)

PO_MODE(NPostman, 0)
NM_FFAEND(NPostman)

PyMethodDef g_NPostmanMethods[] = {
	PO_ENTRIES(NPostman)
	NM_ENTRY(NPostman, CheckEndOfGame)
	NM_ENTRY(NPostman, GetGameDescriptor)
	{ NULL, NULL, 0, NULL },
};

PO_MODE(NTeamPost, 1)
NM_TEAMEND(NTeamPost)

PyMethodDef g_NTeamPostMethods[] = {
	PO_ENTRIES(NTeamPost)
	{ "CheckEndOfGame", NTeamPost_CheckEndOfTeamGame, 1, NULL },
	{ "GetGameDescriptor", NTeamPost_TeamDescriptor, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(NetModes)
