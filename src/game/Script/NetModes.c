/*
 * The online game modes in C (Data.packed scripts/net/n*.py, issue #2): NDeathMatch, NDeathMatchBots, NTeamDM,
 * NTeamFlag, NFlagHunt, NPostman and NTeamPost. init.py preloads the mode of ServerType ("N" + ServerType,
 * NDeathMatch when there is no such file) after Ffa in a network game and sets Ffa.ModLib to it; Ffa calls the mode
 * through ModLib (DeathFunc, CheckEndOfGame, GetGameDescriptor, InitServerMod, InitClientMod, ShowHelp,
 * ServerHidePlayer), the engine through the callbacks the modes set by name ("NFlagHunt.ServerTakeFlag").
 *
 * The modes call Ffa through its module. Their module variables stay variables of the modules: the menu entries
 * (MultiplayerMenuOnGame, MultiplayerMenuSpectator), TeamGame, BotsList, the model names, AUTO_RESPAWN_FLAG_TIME and
 * the state of the flag modes (Flag, ZoneList, AcFlag or AcZone, FlagSpawnPos). The module code of NPostman and
 * NTeamPost creates their flag entity when the module loads; it stays in the scripts.
 *
 * The functions the modes share are written once: those that use the module's own globals take the module name
 * (m) and a variant (v: 1 for the team version of a mode), the per-module entries only pass them on.
 */
#include <limits.h>
#include <string.h>

#include "scrap/script.h"

#define G(n) Sf_Global(f, m, n)
#define FFA(n) Sf_Mod(f, "Ffa", n)
/* the start of a shared function: the frame f and the arguments */
#define NM_ARGS(...) \
	scriptFrame_t frame = { 0 }, *f = &frame; \
	if (!PyArg_ParseTuple(args, __VA_ARGS__)) { \
		return NULL; \
	}
/* the entry of module mod for the shared function impl with the variant v */
#define NM_FN(mod, name, impl, v) \
	static PyObject *mod##_##name(PyObject *self, PyObject *args) { \
		(void)self; \
		return impl(#mod, v, args); \
	}

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

static const char *Nm_Text(const char *m, int help) {
	for (size_t i = 0; i < sizeof(nm_texts) / sizeof(nm_texts[0]); i++) {
		if (!strcmp(nm_texts[i].mod, m)) {
			return help ? nm_texts[i].help : nm_texts[i].title;
		}
	}
	return "";
}

/* "<module>.<func>", the callbacks the modes set by name */
static PyObject *Nm_Name(scriptFrame_t *f, const char *m, const char *func) {
	return Sf_Cat(f, "sss", m, ".", func);
}

/* Ffa.Scorer[name] = Ffa.Scorer[name] + inc */
static void Nm_ScorerAdd(scriptFrame_t *f, PyObject *name, PyObject *inc) {
	PyObject *v = Sf_Op(f, Sf_GetItem(f, FFA("Scorer"), name), '+', inc);
	Sf_SetItem(f, FFA("Scorer"), name, v);
}

/* `if Ffa.Scorer[name] > 0: Ffa.Scorer[name] = Ffa.Scorer[name] - 1` */
static void Nm_ScorerDec(scriptFrame_t *f, PyObject *name) {
	if (Sf_Compare(f, Sf_GetItem(f, FFA("Scorer"), name), ">", Sf_Int(f, 0)) > 0) {
		PyObject *v = Sf_OpI(f, Sf_GetItem(f, FFA("Scorer"), name), '-', 1);
		Sf_SetItem(f, FFA("Scorer"), name, v);
	}
}

/* the team total of the entity name: TotalRed when its TgType is "red", else TotalGreen */
static const char *Nm_Total(scriptFrame_t *f, PyObject *name) {
	int red = Sf_EqS(f, Sf_Attr(f, Sf_Entity(f, name), "TgType"), "red");
	return red > 0 ? "TotalRed" : red == 0 ? "TotalGreen" : NULL;
}

/* Ffa.TotalRed = Ffa.TotalRed + inc (or TotalGreen) by the TgType of name */
static void Nm_TotalAdd(scriptFrame_t *f, PyObject *name, long inc) {
	const char *t = Nm_Total(f, name);
	if (t) {
		Sf_SetMod(f, "Ffa", t, Sf_OpI(f, FFA(t), '+', inc));
	}
}

/* SSound.Play(Sound.<sound>, Sound.<volume>) */
static void Nm_Sound(scriptFrame_t *f, const char *sound, const char *volume) {
	PyObject *s = Sf_Mod(f, "Sound", sound), *v = Sf_Mod(f, "Sound", volume);
	Sf_Run(f, "SSound", "Play", "(OO)", s, v);
}

/* SScorer.<func>(Scrap.GetLangStr(key), Scrap.GetTime() + secs) */
static void Nm_Label(scriptFrame_t *f, const char *func, const char *key, long secs) {
	PyObject *text = Sf_Lang(f, Sf_Str(f, key)), *t = Sf_OpI(f, Sf_Time(f), '+', secs);
	Sf_Run(f, "SScorer", func, "(OO)", text, t);
}

/* `int(Pos[0]),int(Pos[1]),int(Pos[2])` in backquotes */
static PyObject *Nm_PosRepr(scriptFrame_t *f, PyObject *pos) {
	PyObject *x = Sf_ToInt(f, Sf_Item(f, pos, 0)), *y = Sf_ToInt(f, Sf_Item(f, pos, 1)), *z = Sf_ToInt(f, Sf_Item(f, pos, 2));
	return Sf_Repr(f, Sf_Build(f, "(OOO)", x, y, z));
}

/* `ent.var = v` of the entity in ZoneList[<ac>] (v borrowed) */
static void Nm_ZoneSet(scriptFrame_t *f, const char *m, const char *ac, const char *var, PyObject *v) {
	PyObject *zones = G("ZoneList"), *key = G(ac);
	Sf_SetAttr(f, Sf_GetItem(f, zones, key), var, v);
}

/* Flag.var = v of the module (v borrowed) */
static void Nm_FlagSet(scriptFrame_t *f, const char *m, const char *var, PyObject *v) {
	Sf_SetAttr(f, G("Flag"), var, v);
}

/* Flag.SetModel(<model>) of the module */
static void Nm_FlagModel(scriptFrame_t *f, const char *m, const char *model) {
	PyObject *flag = G("Flag");
	Sf_Method(f, flag, "SetModel", "(O)", G(model));
}

/* Scrap.Set("CurrentWaypointName", Flag.Name) */
static void Nm_WayToFlag(scriptFrame_t *f, const char *m) {
	Sf_Run(f, "Scrap", "Set", "(sO)", "CurrentWaypointName", Sf_Attr(f, G("Flag"), "Name"));
}

/* ---- the functions every mode has ---- */

/* CheckEndOfGame() of the free for all modes: a player has the frag limit, the server ends the game */
static PyObject *Nm_CheckEndOfGame(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Ffa");
	PyObject *keys = Sf_Method(&f, Sf_Mod(&f, "Ffa", "Scorer"), "keys", "()");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *usr = Sf_Next(&f, keys, i);
		if (!usr) {
			break;
		}
		PyObject *score = Sf_GetItem(&f, Sf_Mod(&f, "Ffa", "Scorer"), usr);
		if (Sf_Compare(&f, score, ">=", Sf_Mod(&f, "Ffa", "ScorerFraglimit")) > 0) {
			Sf_Run(&f, "Ffa", "ServerEndOfGame", "()");
			return Sf_Done(&f, PyInt_FromLong(1));
		}
		Sf_Release(&f, mark);
	}
	return Sf_Done(&f, PyInt_FromLong(0));
}

/* CheckEndOfGame() of the team modes: a team has the frag limit */
static PyObject *Nm_CheckEndOfTeamGame(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Ffa");
	int end = Sf_Compare(&f, Sf_Mod(&f, "Ffa", "TotalRed"), ">=", Sf_Mod(&f, "Ffa", "ScorerFraglimit"));
	if (end == 0) {
		end = Sf_Compare(&f, Sf_Mod(&f, "Ffa", "TotalGreen"), ">=", Sf_Mod(&f, "Ffa", "ScorerFraglimit"));
	}
	if (end > 0) {
		Sf_Run(&f, "Ffa", "ServerEndOfGame", "()");
		return Sf_Done(&f, PyInt_FromLong(1));
	}
	return Sf_Done(&f, PyInt_FromLong(0));
}

/* DeathFunc(WinnerName, LooserName, inc) of NDeathMatch and NTeamPost: the winner scores */
static PyObject *Nm_DeathFunc(PyObject *self, PyObject *args) {
	PyObject *winner, *looser, *inc;
	SF_ARGS("OOO", &winner, &looser, &inc);
	Sf_Import(&f, "Ffa");
	Nm_ScorerAdd(&f, winner, inc);
	return Sf_None(&f);
}

/* GetGameDescriptor() of the team modes: the score of the teams */
static PyObject *Nm_TeamDescriptor(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Ffa");
	return Sf_Return(&f, Sf_Ask(&f, "Ffa", "GetGameTeamDescriptor", "()"));
}

static PyObject *Nm_Pass(PyObject *self, PyObject *args) {
	SF_ARGS("");
	return Sf_None(&f);
}

static PyObject *Nm_Pass1(PyObject *self, PyObject *args) {
	PyObject *a;
	SF_ARGS("O", &a);
	return Sf_None(&f);
}

static PyObject *Nm_Pass3(PyObject *self, PyObject *args) {
	PyObject *a, *b, *c;
	SF_ARGS("OOO", &a, &b, &c);
	return Sf_None(&f);
}

/* GetGameDescriptor(): the title of the mode */
static PyObject *Nm_GetGameDescriptor(const char *m, int v, PyObject *args) {
	NM_ARGS("");
	(void)v;
	return Sf_Return(f, Sf_Lang(f, Sf_Str(f, Nm_Text(m, 0))));
}

/* ShowHelp(id): the help of the mode in a warning box */
static PyObject *Nm_ShowHelp(const char *m, int v, PyObject *args) {
	PyObject *id;
	NM_ARGS("O", &id);
	(void)v;
	Sf_Import(f, "Menu");
	Sf_Run(f, "Menu", "ShowWarning", "(OO)", id, Sf_Lang(f, Sf_Str(f, Nm_Text(m, 1))));
	return Sf_None(f);
}

/* ---- NTeamDM ---- */

/* DeathFunc(WinnerName, LooserName, inc): the winner and its team score */
static PyObject *NTeamDM_DeathFunc(PyObject *self, PyObject *args) {
	PyObject *winner, *looser, *inc;
	SF_ARGS("OOO", &winner, &looser, &inc);
	Sf_Import(&f, "Ffa");
	Nm_ScorerAdd(&f, winner, inc);
	const char *t = Nm_Total(&f, winner);
	if (t) {
		Sf_SetMod(&f, "Ffa", t, Sf_Op(&f, Sf_Mod(&f, "Ffa", t), '+', inc));
	}
	return Sf_None(&f);
}

/* ---- NDeathMatchBots ---- */

/* DeathFunc(WinnerName, LooserName, inc): a dead bot is reborn after 3 s; the winner scores */
static PyObject *Bots_DeathFunc(const char *m, int v, PyObject *args) {
	PyObject *winner, *looser, *inc;
	NM_ARGS("OOO", &winner, &looser, &inc);
	(void)v;
	Sf_Import(f, "Ffa");
	PyObject *e = Sf_Entity(f, looser);
	if (Sf_True(f, Sf_Attr(f, e, "isAI")) > 0) {
		PyObject *t = Sf_OpI(f, Sf_Time(f), '+', 3), *fn = G("Reborn");
		Sf_Schedule(f, t, fn, Sf_Build(f, "(OO)", looser, Sf_Attr(f, e, "MaxLife")), NULL);
		Sf_SetAttrI(f, e, "Active", 0);
	}
	Nm_ScorerAdd(f, winner, inc);
	return Sf_None(f);
}

/* Reborn(LooserName, Life): the bot at a spawn point with its life, hunting again */
static PyObject *Bots_Reborn(const char *m, int v, PyObject *args) {
	PyObject *looser, *life;
	NM_ARGS("OO", &looser, &life);
	(void)v;
	Sf_Import(f, "Ffa");
	Sf_Run(f, "Ffa", "SetSpawnPos", "(O)", looser);
	Sf_SetAttr(f, Sf_Entity(f, looser), "Life", life);
	Sf_Own(f, m, "SetBotState", "(O)", looser);
	return Sf_None(f);
}

/* TryGarage(id, control): the garage is closed on a server with bots */
static PyObject *Bots_TryGarage(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	SF_ARGS("OO", &id, &control);
	Sf_Run(&f, "SScorer", "Set", "(OOss)", id, control, "Text", "Not in server :-(");
	return Sf_None(&f);
}

/* SetBotState(name): the bot attacks a random other active ship */
static PyObject *Bots_SetBotState(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	PyObject *l = Sf_Build(&f, "[]"), *e = Sf_Ask(&f, "Scrap", "GetFirst", "()");
	int mark = f.n;
	while (Sf_True(&f, e) > 0) {
		int car = Sf_True(&f, Sf_Method(&f, e, "IsType", "(s)", "Car"));
		if (car > 0) {
			car = Sf_EqI(&f, Sf_Attr(&f, e, "Active"), 1);
		}
		if (car > 0) {
			car = Sf_Compare(&f, Sf_Attr(&f, e, "Name"), "!=", name);
		}
		if (car > 0) {
			Sf_Method(&f, l, "append", "(O)", Sf_Attr(&f, e, "Name"));
		}
		e = Sf_Entity(&f, Sf_Attr(&f, e, "NextInSlot"));
		Sf_ReleaseKeep(&f, mark, &e, 1);
	}
	int length = Sf_Len(&f, l);
	if (length > 0) {
		PyObject *index = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "Rand", "(dd)", 0.0, (double)length - 0.05));
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, name, Sf_GetItem(&f, l, index));
	}
	return Sf_None(&f);
}

/* CheckBotState(name): a bot that stopped (state 0 or 3) gets a new target; again in 1.5 s */
static PyObject *Bots_CheckBotState(const char *m, int v, PyObject *args) {
	PyObject *name;
	NM_ARGS("O", &name);
	(void)v;
	PyObject *state = Sf_Ask(f, "SAI", "GetStateVehicle", "(O)", name);
	int stop = Sf_EqI(f, state, 0);
	if (stop == 0) {
		stop = Sf_EqI(f, state, 3);
	}
	if (stop > 0) {
		Sf_Own(f, m, "SetBotState", "(O)", name);
	}
	PyObject *t = Sf_Op(f, Sf_Time(f), '+', Sf_Float(f, 1.5)), *fn = G("CheckBotState");
	Sf_Schedule(f, t, fn, Sf_Build(f, "(O)", name), NULL);
	return Sf_None(f);
}

/* InitServerMod(): no garage; the bots of BotsList join with their ships, AI profiles, 100 life and a nuke */
static PyObject *Bots_InitServerMod(const char *m, int v, PyObject *args) {
	NM_ARGS("");
	(void)v;
	static const char *const mods[] = { "Ffa", "VehiclesNPC", "ShipEdit", "ShipMaker", "string" };
	for (int i = 0; i < 5; i++) {
		Sf_Import(f, mods[i]);
	}
	Sf_SetMod(f, "ShipEdit", "EnterEditShipMenu", G("TryGarage"));
	PyObject *range = Sf_Ask(f, "__builtin__", "range", "(i)", Sf_Len(f, G("BotsList")));
	for (int k = 0;; k++) {
		int mark = f->n;
		PyObject *i = Sf_Next(f, range, k);
		if (!i) {
			break;
		}
		PyObject *bot = Sf_GetItem(f, G("BotsList"), i);
		PyObject *key = Sf_Ask(f, "string", "upper", "(O)", Sf_Item(f, bot, 1));
		PyObject *ship = Sf_GetItem(f, Sf_Mod(f, "ShipMaker", "StdShips"), key);
		int pilots = Sf_Len(f, Sf_Mod(f, "ShipMaker", "PilotsList"));
		PyObject *pd = Sf_Ask(f, "VehiclesNPC", "GetIAProf", "(O)", Sf_Item(f, bot, 1));
		Sf_Run(f, "VehiclesNPC", "SetAIProfile", "(OO)", pd, Sf_Item(f, bot, 2));
		Sf_SetAttrI(f, pd, "CadenceShootCM", 0);
		Sf_SetAttr(f, pd, "AngAttack", Sf_Float(f, 3.14));
		Sf_SetItem(f, Sf_Mod(f, "VehiclesNPC", "AIProf"), Sf_Item(f, bot, 2), pd);
		PyObject *botname = Sf_Ask(f, "SNet", "GetBotName", "()");
		PyObject *ply = Sf_Ask(f, "Scrap", "CreateEntity", "(Oddds)", botname, 0.0, 0.0, 0.0, "Car");
		PyObject *a[11];
		a[0] = Sf_Attr(f, ply, "Name");
		a[1] = Sf_Item(f, bot, 1);
		a[2] = Sf_Item(f, ship, 6);
		a[3] = Sf_Item(f, bot, 0);
		PyObject *list = Sf_Mod(f, "ShipMaker", "PilotsList");
		a[4] = Sf_GetItem(f, list, Sf_ToInt(f, Sf_Ask(f, "Scrap", "Rand", "(dd)", 0.0, (double)pilots - 0.05)));
		for (int j = 1; j <= 5; j++) {
			a[4 + j] = Sf_Item(f, ship, j);
		}
		a[10] = Sf_Str(f, "red");
		Sf_Run(f, "Ffa", "ClientJoin", "(OOOOOOOOOOO)", a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10]);
		Sf_SetAttrI(f, ply, "Active", 0);
		Sf_Run(f, "VehiclesNPC", "SetIAProf", "(OO)", ply, Sf_Item(f, bot, 2));
		Sf_Run(f, "Ffa", "SetSpawnPos", "(O)", Sf_Attr(f, ply, "Name"));
		Sf_SetAttr(f, ply, "Descriptor", Sf_Item(f, bot, 0));
		PyObject *life = Sf_Int(f, 100);
		Sf_SetAttr(f, ply, "Life", life);
		Sf_SetAttr(f, ply, "MinLife", life);
		Sf_SetAttr(f, ply, "MaxLife", life);
		PyObject *weap = Sf_Attr(f, ply, "WeapList");
		weap = Sf_Cat(f, "Os", Sf_Slice(f, weap, 0, Sf_Len(f, weap) - 1), "1");
		Sf_SetAttr(f, ply, "WeapList", weap);
		Sf_Own(f, m, "SetBotState", "(O)", Sf_Attr(f, ply, "Name"));
		PyObject *t = Sf_OpI(f, Sf_Time(f), '+', 2), *fn = G("CheckBotState");
		Sf_Schedule(f, t, fn, Sf_Build(f, "(O)", Sf_Attr(f, ply, "Name")), NULL);
		Sf_Release(f, mark);
	}
	return Sf_None(f);
}

/* ---- NFlagHunt and NTeamFlag (v 1): a flag at a random DM_Multiplayer_Flag_ dummy, taking it scores ---- */

/* DeathFunc(WinnerName, LooserName, inc): the dead player loses a point (and its team in NTeamFlag) */
static PyObject *Fl_DeathFunc(const char *m, int v, PyObject *args) {
	PyObject *winner, *looser, *inc;
	NM_ARGS("OOO", &winner, &looser, &inc);
	(void)m;
	Sf_Import(f, "Ffa");
	Nm_ScorerDec(f, looser);
	if (v) {
		const char *t = Nm_Total(f, looser);
		if (t && Sf_Compare(f, FFA(t), ">", Sf_Int(f, 0)) > 0) {
			Sf_SetMod(f, "Ffa", t, Sf_OpI(f, FFA(t), '-', 1));
		}
	}
	return Sf_None(f);
}

/* GetRndFlag() and GetRndZone() of the postman modes: a random key of ZoneList into the global ac, the first (or
 * the second) when it is the last one */
static void Fl_RndKey(scriptFrame_t *f, const char *m, const char *ac) {
	PyObject *k = Sf_Method(f, G("ZoneList"), "keys", "()");
	PyObject *last = G(ac);
	PyObject *n = Sf_Ask(f, "Scrap", "Rand", "(ii)", 0, Sf_Len(f, k));
	Sf_SetGlobal(f, m, ac, Sf_GetItem(f, k, Sf_ToInt(f, n)));
	if (Sf_Compare(f, G(ac), "==", last) > 0) {
		Sf_SetGlobal(f, m, ac, Sf_Item(f, k, 0));
		if (Sf_Compare(f, G(ac), "==", last) > 0) {
			Sf_SetGlobal(f, m, ac, Sf_Item(f, k, 1));
		}
	}
}

static PyObject *Fl_GetRndFlag(const char *m, int v, PyObject *args) {
	NM_ARGS("");
	(void)v;
	Fl_RndKey(f, m, "AcFlag");
	return Sf_None(f);
}

/* BrowseAreas(): ZoneList, the positions of the DM_Multiplayer_Flag_ dummies by the rest of their names (NFlagHunt
 * prints each dummy) */
static PyObject *Fl_BrowseAreas(const char *m, int v, PyObject *args) {
	NM_ARGS("");
	Sf_SetGlobal(f, m, "ZoneList", Sf_Build(f, "{}"));
	Sf_Run(f, "Scrap", "StartDummySearch", "(si)", "DM_Multiplayer_Flag_*", 1);
	PyObject *dummy = Sf_Ask(f, "Scrap", "NextDummySearch", "()");
	int mark = f->n;
	while (Sf_True(f, dummy) > 0) {
		if (!v) {
			Sf_Print(f, "O", dummy);
		}
		PyObject *pos = Sf_Item(f, dummy, 1), *zones = G("ZoneList");
		Sf_SetItem(f, zones, Sf_Slice(f, Sf_Item(f, dummy, 0), 19, INT_MAX), pos);
		dummy = Sf_Ask(f, "Scrap", "NextDummySearch", "()");
		Sf_ReleaseKeep(f, mark, &dummy, 1);
	}
	return Sf_None(f);
}

/* CreateFlag(isServer): the flag; the server puts it at a random zone and takes the joins */
static PyObject *Fl_CreateFlag(const char *m, int v, PyObject *args) {
	PyObject *server;
	NM_ARGS("O", &server);
	(void)v;
	Sf_SetGlobal(f, m, "Flag", Sf_Ask(f, "Scrap", "CreateEntity", "(siiis)", "FlagToBeHunted", 0, 0, 0, "Item"));
	Nm_FlagModel(f, m, "FlagModel");
	Nm_FlagSet(f, m, "Radius", Sf_Int(f, 2000));
	Nm_FlagSet(f, m, "RadarShow", Sf_Int(f, 1));
	int s = Sf_True(f, server);
	if (s > 0) {
		Sf_Own(f, m, "GetRndFlag", "()");
		Nm_FlagSet(f, m, "OnTake", Nm_Name(f, m, "ServerTakeFlag"));
		PyObject *zones = G("ZoneList"), *ac = G("AcFlag"), *pos = Sf_GetItem(f, zones, ac);
		Nm_FlagSet(f, m, "Pos", pos);
		Sf_Run(f, "Scrap", "Set", "(sO)", "OnClientJoin", Nm_Name(f, m, "ClientJoin"));
	} else if (s == 0) {
		Sf_Run(f, "Scrap", "Set", "(sO)", "OnUsrString", Nm_Name(f, m, "OnUsrClient"));
	}
	Nm_WayToFlag(f, m);
	return Sf_None(f);
}

/* TakeFlag(TakerName): the flag at the current zone, the sound of a taker that is the player (its team in
 * NTeamFlag) or else of an enemy */
static PyObject *Fl_TakeFlag(const char *m, int v, PyObject *args) {
	PyObject *taker;
	NM_ARGS("O", &taker);
	Sf_Import(f, "Sound");
	PyObject *zones = G("ZoneList"), *ac = G("AcFlag"), *pos = Sf_GetItem(f, zones, ac);
	Nm_FlagSet(f, m, "Pos", pos);
	if (Sf_Compare(f, taker, "!=", Sf_Str(f, "")) > 0) {
		int way = Sf_True(f, Sf_Ask(f, "Scrap", "UsrEntity", "(i)", 0));
		if (way > 0) {
			PyObject *usr = Sf_Ask(f, "Scrap", "UsrEntity", "(i)", 0);
			if (v) {
				PyObject *mine = Sf_Attr(f, usr, "TgType");
				way = Sf_Compare(f, mine, "==", Sf_Attr(f, Sf_Entity(f, taker), "TgType"));
			} else {
				way = Sf_Compare(f, Sf_Attr(f, usr, "Name"), "==", taker);
			}
		}
		if (way > 0) {
			Nm_Sound(f, "WaySound", "WayVolume");
		} else if (way == 0) {
			Nm_Sound(f, "EneWaySound", "EneWayVolume");
		}
	}
	return Sf_None(f);
}

/* OnUsrClient(id, s) of the flag and postman modes: "F'zone','taker'" a flag taken, "D(x, y, z)" the flag dropped
 * (postman modes, drop set), the rest goes to Ffa */
static PyObject *Fl_UsrClient(const char *m, const char *ac, int drop, PyObject *args) {
	PyObject *id, *s;
	NM_ARGS("OO", &id, &s);
	Sf_Import(f, "Ffa");
	int c = Sf_EqS(f, Sf_Item(f, s, 0), "F");
	if (c > 0) {
		PyObject *v[2];
		if (Sf_Unpack(f, Sf_Eval(f, Sf_Slice(f, s, 1, INT_MAX), m), 2, v)) {
			Sf_SetGlobal(f, m, ac, v[0]);
			Sf_Own(f, m, "TakeFlag", "(O)", v[1]);
		}
		return Sf_None(f);
	}
	if (c == 0 && drop) {
		c = Sf_EqS(f, Sf_Item(f, s, 0), "D");
		if (c > 0) {
			PyObject *fn = G("DropFlag");
			Sf_Call(f, fn, "(O)", Sf_Eval(f, Sf_Slice(f, s, 1, INT_MAX), m));
			return Sf_None(f);
		}
	}
	if (c == 0) {
		Sf_Run(f, "Ffa", "OnUsrClient", "(OO)", id, s);
	}
	return Sf_None(f);
}

static PyObject *Fl_OnUsrClient(const char *m, int v, PyObject *args) {
	(void)v;
	return Fl_UsrClient(m, "AcFlag", 0, args);
}

/* the 11 arguments of ClientJoin into a; Ffa.ClientJoin with them, and the client id of the entity name ("net5") */
static PyObject *Nm_Join(scriptFrame_t *f, PyObject *const *a) {
	Sf_Import(f, "Ffa");
	Sf_Run(f, "Ffa", "ClientJoin", "(OOOOOOOOOOO)", a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10]);
	return Sf_ToInt(f, Sf_Slice(f, a[0], 3, INT_MAX));
}

#define NM_JOIN_ARGS \
	PyObject *a[11]; \
	NM_ARGS("OOOOOOOOOOO", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9], &a[10]);

/* ClientJoin(entityname, ModelName, MaxLife, UsrName, PilotName, Motor0..3, WeaponBay, TgType): the join of Ffa,
 * then the current zone to the new client */
static PyObject *Fl_ClientJoin(const char *m, int v, PyObject *args) {
	NM_JOIN_ARGS;
	(void)v;
	PyObject *id = Nm_Join(f, a);
	Sf_Run(f, "SNet", "SendUsrString", "(OO)", id, Sf_Cat(f, "sOs", "F'", G("AcFlag"), "',''"));
	return Sf_None(f);
}

/* ServerTakeFlag(ItemName, VehicleName): a player took the flag after the start: it scores (its team too in
 * NTeamFlag), the flag moves on and the clients hear of it; before the start a label says to wait */
static PyObject *Fl_ServerTakeFlag(const char *m, int v, PyObject *args) {
	PyObject *item, *veh;
	NM_ARGS("OO", &item, &veh);
	Sf_Import(f, "Ffa");
	if (Sf_True(f, Sf_Method(f, FFA("Scorer"), "has_key", "(O)", veh)) <= 0) {
		return Sf_None(f);
	}
	int started = Sf_True(f, Sf_Ask(f, "Ffa", "CheckGameStart", "()"));
	if (started > 0) {
		Nm_ScorerAdd(f, veh, Sf_Int(f, 1));
		if (v) {
			Nm_TotalAdd(f, veh, 1);
		}
		Sf_Own(f, m, "GetRndFlag", "()");
		Sf_Run(f, "SNet", "SendUsrString", "(iO)", -1, Sf_Cat(f, "sOsOs", "F'", G("AcFlag"), "','", veh, "'"));
		Sf_Own(f, m, "TakeFlag", "(O)", veh);
		Sf_Run(f, "Ffa", "ActualizeGameScorer", "()");
		Sf_Run(f, "Ffa", "ServerSendPlayerStatus", "(O)", veh);
		if (Sf_True(f, Sf_Own(f, m, "CheckEndOfGame", "()")) > 0) {
			Nm_FlagSet(f, m, "OnTake", Sf_Str(f, ""));
		}
	} else if (started == 0) {
		Nm_Label(f, "SetLabelText", "Net_Label_Waiting_Challenge", 1);
	}
	return Sf_None(f);
}

/* InitServerMod(), InitClientMod() of the flag and postman modes (v 1: the server; Ffa imported in the postman
 * modes, imp) */
static PyObject *Fl_Init(const char *m, int server, int imp, PyObject *args) {
	NM_ARGS("");
	if (imp) {
		Sf_Import(f, "Ffa");
	}
	Sf_Own(f, m, "BrowseAreas", "()");
	Sf_Own(f, m, "CreateFlag", "(i)", server);
	return Sf_None(f);
}

static PyObject *Fl_InitServerMod(const char *m, int v, PyObject *args) {
	(void)v;
	return Fl_Init(m, 1, 0, args);
}

static PyObject *Fl_InitClientMod(const char *m, int v, PyObject *args) {
	(void)v;
	return Fl_Init(m, 0, 0, args);
}

/* ---- NPostman and NTeamPost (v 1): one flag at DM_Domination_Flag, its taker carries it to a random zone ---- */

static PyObject *Po_InitServerMod(const char *m, int v, PyObject *args) {
	(void)v;
	return Fl_Init(m, 1, 1, args);
}

/* DeathFunc(WinnerName, LooserName, inc): NPostman does nothing, NTeamPost scores the winner */
static PyObject *Po_DeathFunc(const char *m, int v, PyObject *args) {
	(void)m;
	return v ? Nm_DeathFunc(NULL, args) : Nm_Pass3(NULL, args);
}

/* ServerHidePlayer(VehicleName): the flag drops when its carrier leaves */
static PyObject *Po_ServerHidePlayer(const char *m, int v, PyObject *args) {
	PyObject *veh;
	NM_ARGS("O", &veh);
	(void)v;
	Sf_Import(f, "Ffa");
	if (Sf_Compare(f, FFA("MasterFlag"), "==", veh) > 0) {
		Sf_Own(f, m, "ServerDropFlag", "()");
	}
	return Sf_None(f);
}

static PyObject *Po_GetRndZone(const char *m, int v, PyObject *args) {
	NM_ARGS("");
	(void)v;
	Fl_RndKey(f, m, "AcZone");
	return Sf_None(f);
}

/* BrowseAreas(): FlagSpawnPos from DM_Domination_Flag; ZoneList, an item of radius 3000 at each DM_Domination_Zone_
 * dummy by the rest of its name */
static PyObject *Po_BrowseAreas(const char *m, int v, PyObject *args) {
	NM_ARGS("");
	(void)v;
	Sf_Run(f, "Scrap", "StartDummySearch", "(si)", "DM_Domination_Flag", 0);
	PyObject *dummy = Sf_Ask(f, "Scrap", "NextDummySearch", "()");
	Sf_SetGlobal(f, m, "FlagSpawnPos", Sf_Item(f, dummy, 1));
	Sf_SetGlobal(f, m, "ZoneList", Sf_Build(f, "{}"));
	Sf_Run(f, "Scrap", "StartDummySearch", "(si)", "DM_Domination_Zone_*", 1);
	dummy = Sf_Ask(f, "Scrap", "NextDummySearch", "()");
	int mark = f->n;
	while (Sf_True(f, dummy) > 0) {
		PyObject *ent = Sf_Ask(f, "Scrap", "CreateEntity", "(Oiiis)", Sf_Item(f, dummy, 0), 0, 0, 0, "Item");
		Sf_SetAttr(f, ent, "Pos", Sf_Item(f, dummy, 1));
		Sf_SetAttrI(f, ent, "Radius", 3000);
		PyObject *zones = G("ZoneList");
		Sf_SetItem(f, zones, Sf_Slice(f, Sf_Item(f, dummy, 0), 19, INT_MAX), ent);
		dummy = Sf_Ask(f, "Scrap", "NextDummySearch", "()");
		Sf_ReleaseKeep(f, mark, &dummy, 1);
	}
	return Sf_None(f);
}

/* CreateFlag(isServer): the flag of the module code with its models; the server puts it at FlagSpawnPos */
static PyObject *Po_CreateFlag(const char *m, int v, PyObject *args) {
	PyObject *server;
	NM_ARGS("O", &server);
	(void)v;
	Sf_Run(f, "Scrap", "Preload3DObject", "(O)", G("FlagPilotModel"));
	Sf_Run(f, "Scrap", "Preload3DObject", "(O)", G("FlagNoUsrModel"));
	Nm_FlagModel(f, m, "FlagModel");
	Nm_FlagSet(f, m, "Radius", Sf_Int(f, 2000));
	Nm_FlagSet(f, m, "RadarShow", Sf_Int(f, 1));
	int s = Sf_True(f, server);
	if (s > 0) {
		Nm_FlagSet(f, m, "OnTake", Nm_Name(f, m, "ServerTakeFlag"));
		Nm_FlagSet(f, m, "Pos", G("FlagSpawnPos"));
		Sf_Run(f, "Scrap", "Set", "(sO)", "OnClientJoin", Nm_Name(f, m, "ClientJoin"));
	} else if (s == 0) {
		Sf_Run(f, "Scrap", "Set", "(sO)", "OnUsrString", Nm_Name(f, m, "OnUsrClient"));
	}
	Nm_WayToFlag(f, m);
	return Sf_None(f);
}

/* LinkFlagTo(TakerName, Node): the flag onto the node of the taker (no older try left) */
static PyObject *Po_LinkFlagTo(const char *m, int v, PyObject *args) {
	PyObject *taker, *node;
	NM_ARGS("OO", &taker, &node);
	(void)v;
	Sf_Run(f, "Scrap", "DeleteScheduledFuncs", "(s)", "TryLinkFlagTo");
	Sf_Own(f, m, "TryLinkFlagTo", "(OO)", taker, node);
	return Sf_None(f);
}

/* TryLinkFlagTo(TakerName, Node): links the flag once the taker has a model, else tries again in 0.5 s */
static PyObject *Po_TryLinkFlagTo(const char *m, int v, PyObject *args) {
	PyObject *taker, *node;
	NM_ARGS("OO", &taker, &node);
	(void)v;
	PyObject *e = Sf_Entity(f, taker);
	int ok = Sf_True(f, e);
	if (ok > 0) {
		ok = Sf_Compare(f, Sf_Attr(f, e, "Model"), "!=", Sf_Str(f, ""));
	}
	if (ok > 0) {
		Nm_FlagSet(f, m, "LinkModel", Sf_Cat(f, "OsO", taker, ",", node));
	} else if (ok == 0) {
		PyObject *t = Sf_Op(f, Sf_Time(f), '+', Sf_Float(f, 0.5)), *fn = G("TryLinkFlagTo");
		Sf_Schedule(f, t, fn, Sf_Build(f, "(OO)", taker, node), "TryLinkFlagTo");
	}
	return Sf_None(f);
}

/* TakeFlag(TakerName): the taker carries the flag to the current zone; the sounds, waypoint and messages of the
 * player who took it, (NTeamPost) of its team, of the enemies */
static PyObject *Po_TakeFlag(const char *m, int v, PyObject *args) {
	PyObject *name;
	NM_ARGS("O", &name);
	PyObject *taker = Sf_Entity(f, name);
	int go = Sf_Compare(f, name, "!=", Sf_Str(f, ""));
	if (go > 0) {
		go = Sf_Compare(f, taker, "!=", Py_None);
	}
	if (go <= 0) {
		return Sf_None(f);
	}
	Sf_Import(f, "Sound");
	Sf_Import(f, "Ffa");
	Sf_Own(f, m, "AbortZoneKeys", "()");
	Nm_ZoneSet(f, m, "AcZone", "RadarShow", Sf_Int(f, 2));
	int take = Sf_Compare(f, FFA("MasterFlag"), "!=", name);
	if (take > 0) {
		Sf_SetMod(f, "Ffa", "MasterFlag", name);
		int usr = Sf_EqI(f, Sf_Attr(f, taker, "UsrControl"), 0);
		if (usr >= 0) {
			Nm_FlagModel(f, m, usr ? "FlagNoUsrModel" : "FlagPilotModel");
		}
		Sf_Own(f, m, "LinkFlagTo", "(Os)", name, "DC_Flag_Player");
		Nm_FlagSet(f, m, "OffModPos", Sf_Build(f, "(iii)", 0, 0, 0));
		Nm_FlagSet(f, m, "DeltaOffMul", Sf_Int(f, 4));
		Nm_FlagSet(f, m, "OffMul", Sf_Int(f, 0));
		Nm_FlagSet(f, m, "UseLinkAng", Sf_Int(f, 1));
	}
	PyObject *usr = Sf_Ask(f, "Scrap", "UsrEntity", "(i)", 0);
	int me = Sf_True(f, usr);
	if (me > 0) {
		if (!v) {
			usr = Sf_Ask(f, "Scrap", "UsrEntity", "(i)", 0);
		}
		me = Sf_Compare(f, Sf_Attr(f, usr, "Name"), "==", name);
	}
	if (me > 0) {
		if (v) {
			PyObject *zones = G("ZoneList"), *ac = G("AcZone");
			Sf_Run(f, "Scrap", "Set", "(sO)", "CurrentWaypointName", Sf_Attr(f, Sf_GetItem(f, zones, ac), "Name"));
			Nm_Sound(f, "WaySound", "WayVolume");
		} else {
			Nm_Sound(f, "WaySound", "WayVolume");
			PyObject *zones = G("ZoneList"), *ac = G("AcZone");
			Sf_Run(f, "Scrap", "Set", "(sO)", "CurrentWaypointName", Sf_Attr(f, Sf_GetItem(f, zones, ac), "Name"));
		}
		if (take > 0) {
			Nm_Label(f, "SetMsgText", "Net_Msg_You_HaveFlag", 5);
		}
	} else if (me == 0) {
		int team = v ? Sf_True(f, usr) : 0;
		if (team > 0) {
			PyObject *mine = Sf_Attr(f, usr, "TgType");
			team = Sf_Compare(f, mine, "==", Sf_Attr(f, Sf_Entity(f, name), "TgType"));
		}
		if (team > 0) {
			Nm_Sound(f, "WaySound", "WayVolume");
			if (take > 0) {
				Nm_Label(f, "SetMsgText", "Net_Msg_Your_TeamFlag", 5);
			}
		} else if (team == 0) {
			Nm_Sound(f, "EneWaySound", "EneWayVolume");
			if (v && take > 0) {
				Nm_Label(f, "SetMsgText", "Net_Msg_Enemy_TeamFlag", 5);
			}
		}
	}
	Sf_Run(f, "Ffa", "ActualizeGameScorer", "()");
	return Sf_None(f);
}

/* DropFlag(Pos): the flag alone at Pos with nobody carrying it, the zones off */
static PyObject *Po_DropFlag(const char *m, int v, PyObject *args) {
	PyObject *pos;
	NM_ARGS("O", &pos);
	Sf_Import(f, "Sound");
	Sf_Import(f, "Ffa");
	Nm_FlagSet(f, m, "LinkModel", Sf_Str(f, ","));
	if (v) {
		Nm_FlagModel(f, m, "FlagModel");
	}
	Nm_FlagSet(f, m, "Pos", pos);
	Nm_FlagSet(f, m, "rTime", Sf_Float(f, 0.2));
	Sf_SetMod(f, "Ffa", "MasterFlag", Py_None);
	if (!v) {
		Nm_FlagModel(f, m, "FlagModel");
	}
	Nm_WayToFlag(f, m);
	Nm_Sound(f, "EneWaySound", "EneWayVolume");
	Sf_Own(f, m, "AbortZoneKeys", "()");
	Sf_Run(f, "Ffa", "ActualizeGameScorer", "()");
	return Sf_None(f);
}

/* AbortZoneKeys(): no zone on the radar or taking */
static PyObject *Po_AbortZoneKeys(const char *m, int v, PyObject *args) {
	NM_ARGS("");
	(void)v;
	PyObject *keys = Sf_Method(f, G("ZoneList"), "keys", "()");
	for (int i = 0;; i++) {
		int mark = f->n;
		PyObject *idx = Sf_Next(f, keys, i);
		if (!idx) {
			break;
		}
		Sf_SetAttrI(f, Sf_GetItem(f, G("ZoneList"), idx), "RadarShow", 0);
		Sf_SetAttrS(f, Sf_GetItem(f, G("ZoneList"), idx), "OnTake", "");
		Sf_Release(f, mark);
	}
	return Sf_None(f);
}

static PyObject *Po_OnUsrClient(const char *m, int v, PyObject *args) {
	(void)v;
	return Fl_UsrClient(m, "AcZone", 1, args);
}

/* "D" + `int(Pos[0]),int(Pos[1]),int(Pos[2])` to all clients */
static void Po_SendDrop(scriptFrame_t *f, PyObject *pos) {
	Sf_Run(f, "SNet", "SendUsrString", "(iO)", -1, Sf_Cat(f, "sO", "D", Nm_PosRepr(f, pos)));
}

/* ClientJoin(...): the join of Ffa, then the carrier and zone to the new client, or the flag's place to all */
static PyObject *Po_ClientJoin(const char *m, int v, PyObject *args) {
	NM_JOIN_ARGS;
	(void)v;
	PyObject *id = Nm_Join(f, a);
	int carried = Sf_True(f, FFA("MasterFlag"));
	if (carried > 0) {
		PyObject *zone = G("AcZone");
		Sf_Run(f, "SNet", "SendUsrString", "(OO)", id, Sf_Cat(f, "sOsOs", "F'", zone, "','", FFA("MasterFlag"), "'"));
	} else if (carried == 0) {
		Po_SendDrop(f, Sf_Attr(f, G("Flag"), "Pos"));
	}
	return Sf_None(f);
}

/* ServerTakeBase(ItemName, VehicleName): the carrier reached the zone: it scores (NTeamPost: 10 and a point for
 * its team) and takes the flag again for the next zone */
static PyObject *Po_ServerTakeBase(const char *m, int v, PyObject *args) {
	PyObject *item, *veh;
	NM_ARGS("OO", &item, &veh);
	Sf_Import(f, "Ffa");
	if (Sf_Compare(f, FFA("MasterFlag"), "==", veh) > 0) {
		Nm_ScorerAdd(f, veh, Sf_Int(f, v ? 10 : 1));
		if (v) {
			Nm_TotalAdd(f, veh, 1);
		}
		Sf_Run(f, "Ffa", "ActualizeGameScorer", "()");
		Sf_Run(f, "Ffa", "ServerSendPlayerStatus", "(O)", veh);
		PyObject *fn = G("ServerTakeFlag");
		Sf_Call(f, fn, "(OO)", Sf_Attr(f, G("Flag"), "Name"), veh);
		Sf_Own(f, m, "CheckEndOfGame", "()");
	}
	return Sf_None(f);
}

/* ServerDropFlag(): the flag drops where its carrier is (back to the start in AUTO_RESPAWN_FLAG_TIME) or at
 * FlagSpawnPos, for all clients */
static PyObject *Po_ServerDropFlag(const char *m, int v, PyObject *args) {
	NM_ARGS("");
	(void)v;
	Sf_Import(f, "Ffa");
	PyObject *pos = NULL;
	int carried = Sf_True(f, FFA("MasterFlag"));
	if (carried > 0) {
		pos = Sf_Attr(f, Sf_Entity(f, FFA("MasterFlag")), "Pos");
		PyObject *t = Sf_Op(f, Sf_Time(f), '+', G("AUTO_RESPAWN_FLAG_TIME")), *fn = G("RespawnFlag");
		Sf_Schedule(f, t, fn, Sf_Build(f, "()"), "RespawnMainFlag");
	} else if (carried == 0) {
		Sf_Run(f, "Scrap", "DeleteScheduledFuncs", "(s)", "RespawnMainFlag");
		pos = G("FlagSpawnPos");
	}
	Po_SendDrop(f, pos);
	Nm_FlagSet(f, m, "OnTake", Nm_Name(f, m, "ServerTakeFlag"));
	Sf_Own(f, m, "DropFlag", "(O)", pos);
	return Sf_None(f);
}

/* ServerTakeFlag(ItemName, VehicleName): a player took the flag after the start: a new zone takes it, for all
 * clients; before the start a label says to wait */
static PyObject *Po_ServerTakeFlag(const char *m, int v, PyObject *args) {
	PyObject *item, *veh;
	NM_ARGS("OO", &item, &veh);
	(void)v;
	Sf_Import(f, "Ffa");
	if (Sf_True(f, Sf_Method(f, FFA("Scorer"), "has_key", "(O)", veh)) <= 0) {
		return Sf_None(f);
	}
	int started = Sf_True(f, Sf_Ask(f, "Ffa", "CheckGameStart", "()"));
	if (started > 0) {
		Sf_Run(f, "Scrap", "DeleteScheduledFuncs", "(s)", "RespawnMainFlag");
		Sf_Own(f, m, "GetRndZone", "()");
		Sf_Run(f, "SNet", "SendUsrString", "(iO)", -1, Sf_Cat(f, "sOsOs", "F'", G("AcZone"), "','", veh, "'"));
		Sf_Own(f, m, "TakeFlag", "(O)", veh);
		Nm_ZoneSet(f, m, "AcZone", "OnTake", Nm_Name(f, m, "ServerTakeBase"));
		Nm_FlagSet(f, m, "OnTake", Sf_Str(f, ""));
	} else if (started == 0) {
		Nm_Label(f, "SetLabelText", "Net_Label_Waiting_Challenge", 1);
	}
	return Sf_None(f);
}

/* RespawnFlag(): nobody carries the flag, it goes back to the start */
static PyObject *Po_RespawnFlag(const char *m, int v, PyObject *args) {
	NM_ARGS("");
	(void)v;
	Sf_Import(f, "Ffa");
	Sf_SetMod(f, "Ffa", "MasterFlag", Py_None);
	Sf_Own(f, m, "ServerDropFlag", "()");
	return Sf_None(f);
}

/* ---- the tables ---- */

NM_FN(NDeathMatch, GetGameDescriptor, Nm_GetGameDescriptor, 0)
NM_FN(NDeathMatch, ShowHelp, Nm_ShowHelp, 0)

PyMethodDef g_NDeathMatchMethods[] = {
	{ "DeathFunc", Nm_DeathFunc, 1, NULL },
	{ "CheckEndOfGame", Nm_CheckEndOfGame, 1, NULL },
	{ "GetGameDescriptor", NDeathMatch_GetGameDescriptor, 1, NULL },
	{ "InitServerMod", Nm_Pass, 1, NULL },
	{ "InitClientMod", Nm_Pass, 1, NULL },
	{ "ShowHelp", NDeathMatch_ShowHelp, 1, NULL },
	{ "ServerHidePlayer", Nm_Pass1, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

NM_FN(NDeathMatchBots, DeathFunc, Bots_DeathFunc, 0)
NM_FN(NDeathMatchBots, Reborn, Bots_Reborn, 0)
NM_FN(NDeathMatchBots, GetGameDescriptor, Nm_GetGameDescriptor, 0)
NM_FN(NDeathMatchBots, CheckBotState, Bots_CheckBotState, 0)
NM_FN(NDeathMatchBots, InitServerMod, Bots_InitServerMod, 0)
NM_FN(NDeathMatchBots, ShowHelp, Nm_ShowHelp, 0)

PyMethodDef g_NDeathMatchBotsMethods[] = {
	{ "DeathFunc", NDeathMatchBots_DeathFunc, 1, NULL },
	{ "Reborn", NDeathMatchBots_Reborn, 1, NULL },
	{ "CheckEndOfGame", Nm_CheckEndOfGame, 1, NULL },
	{ "GetGameDescriptor", NDeathMatchBots_GetGameDescriptor, 1, NULL },
	{ "TryGarage", Bots_TryGarage, 1, NULL },
	{ "SetBotState", Bots_SetBotState, 1, NULL },
	{ "CheckBotState", NDeathMatchBots_CheckBotState, 1, NULL },
	{ "InitServerMod", NDeathMatchBots_InitServerMod, 1, NULL },
	{ "InitClientMod", Nm_Pass, 1, NULL },
	{ "ShowHelp", NDeathMatchBots_ShowHelp, 1, NULL },
	{ "ServerHidePlayer", Nm_Pass1, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

NM_FN(NTeamDM, ShowHelp, Nm_ShowHelp, 1)

PyMethodDef g_NTeamDMMethods[] = {
	{ "DeathFunc", NTeamDM_DeathFunc, 1, NULL },
	{ "CheckEndOfGame", Nm_CheckEndOfTeamGame, 1, NULL },
	{ "GetGameDescriptor", Nm_TeamDescriptor, 1, NULL },
	{ "InitServerMod", Nm_Pass, 1, NULL },
	{ "InitClientMod", Nm_Pass, 1, NULL },
	{ "ShowHelp", NTeamDM_ShowHelp, 1, NULL },
	{ "ServerHidePlayer", Nm_Pass1, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* the functions of a flag mode (v 1 the team version) */
#define FL_MODE(mod, v) \
	NM_FN(mod, DeathFunc, Fl_DeathFunc, v) \
	NM_FN(mod, InitServerMod, Fl_InitServerMod, v) \
	NM_FN(mod, InitClientMod, Fl_InitClientMod, v) \
	NM_FN(mod, ShowHelp, Nm_ShowHelp, v) \
	NM_FN(mod, GetRndFlag, Fl_GetRndFlag, v) \
	NM_FN(mod, BrowseAreas, Fl_BrowseAreas, v) \
	NM_FN(mod, CreateFlag, Fl_CreateFlag, v) \
	NM_FN(mod, TakeFlag, Fl_TakeFlag, v) \
	NM_FN(mod, OnUsrClient, Fl_OnUsrClient, v) \
	NM_FN(mod, ClientJoin, Fl_ClientJoin, v) \
	NM_FN(mod, ServerTakeFlag, Fl_ServerTakeFlag, v)
#define FL_ENTRY(mod, name) { #name, mod##_##name, 1, NULL },
#define FL_ENTRIES(mod) \
	FL_ENTRY(mod, DeathFunc) \
	FL_ENTRY(mod, InitServerMod) \
	FL_ENTRY(mod, InitClientMod) \
	FL_ENTRY(mod, ShowHelp) \
	{ "ServerHidePlayer", Nm_Pass1, 1, NULL }, \
	FL_ENTRY(mod, GetRndFlag) \
	FL_ENTRY(mod, BrowseAreas) \
	FL_ENTRY(mod, CreateFlag) \
	FL_ENTRY(mod, TakeFlag) \
	FL_ENTRY(mod, OnUsrClient) \
	FL_ENTRY(mod, ClientJoin) \
	FL_ENTRY(mod, ServerTakeFlag)

FL_MODE(NFlagHunt, 0)
NM_FN(NFlagHunt, GetGameDescriptor, Nm_GetGameDescriptor, 0)

PyMethodDef g_NFlagHuntMethods[] = {
	FL_ENTRIES(NFlagHunt)
	{ "CheckEndOfGame", Nm_CheckEndOfGame, 1, NULL },
	{ "GetGameDescriptor", NFlagHunt_GetGameDescriptor, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

FL_MODE(NTeamFlag, 1)

PyMethodDef g_NTeamFlagMethods[] = {
	FL_ENTRIES(NTeamFlag)
	{ "CheckEndOfGame", Nm_CheckEndOfTeamGame, 1, NULL },
	{ "GetGameDescriptor", Nm_TeamDescriptor, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* the functions of a postman mode (v 1 the team version) */
#define PO_MODE(mod, v) \
	NM_FN(mod, DeathFunc, Po_DeathFunc, v) \
	NM_FN(mod, InitServerMod, Po_InitServerMod, v) \
	NM_FN(mod, InitClientMod, Fl_InitClientMod, v) \
	NM_FN(mod, ShowHelp, Nm_ShowHelp, v) \
	NM_FN(mod, ServerHidePlayer, Po_ServerHidePlayer, v) \
	NM_FN(mod, GetRndZone, Po_GetRndZone, v) \
	NM_FN(mod, BrowseAreas, Po_BrowseAreas, v) \
	NM_FN(mod, CreateFlag, Po_CreateFlag, v) \
	NM_FN(mod, LinkFlagTo, Po_LinkFlagTo, v) \
	NM_FN(mod, TryLinkFlagTo, Po_TryLinkFlagTo, v) \
	NM_FN(mod, TakeFlag, Po_TakeFlag, v) \
	NM_FN(mod, DropFlag, Po_DropFlag, v) \
	NM_FN(mod, AbortZoneKeys, Po_AbortZoneKeys, v) \
	NM_FN(mod, OnUsrClient, Po_OnUsrClient, v) \
	NM_FN(mod, ClientJoin, Po_ClientJoin, v) \
	NM_FN(mod, ServerTakeBase, Po_ServerTakeBase, v) \
	NM_FN(mod, ServerDropFlag, Po_ServerDropFlag, v) \
	NM_FN(mod, ServerTakeFlag, Po_ServerTakeFlag, v) \
	NM_FN(mod, RespawnFlag, Po_RespawnFlag, v)
#define PO_ENTRIES(mod) \
	FL_ENTRY(mod, DeathFunc) \
	FL_ENTRY(mod, InitServerMod) \
	FL_ENTRY(mod, InitClientMod) \
	FL_ENTRY(mod, ShowHelp) \
	FL_ENTRY(mod, ServerHidePlayer) \
	FL_ENTRY(mod, GetRndZone) \
	FL_ENTRY(mod, BrowseAreas) \
	FL_ENTRY(mod, CreateFlag) \
	FL_ENTRY(mod, LinkFlagTo) \
	FL_ENTRY(mod, TryLinkFlagTo) \
	FL_ENTRY(mod, TakeFlag) \
	FL_ENTRY(mod, DropFlag) \
	FL_ENTRY(mod, AbortZoneKeys) \
	FL_ENTRY(mod, OnUsrClient) \
	FL_ENTRY(mod, ClientJoin) \
	FL_ENTRY(mod, ServerTakeBase) \
	FL_ENTRY(mod, ServerDropFlag) \
	FL_ENTRY(mod, ServerTakeFlag) \
	FL_ENTRY(mod, RespawnFlag)

PO_MODE(NPostman, 0)
NM_FN(NPostman, GetGameDescriptor, Nm_GetGameDescriptor, 0)

PyMethodDef g_NPostmanMethods[] = {
	PO_ENTRIES(NPostman)
	{ "CheckEndOfGame", Nm_CheckEndOfGame, 1, NULL },
	{ "GetGameDescriptor", NPostman_GetGameDescriptor, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PO_MODE(NTeamPost, 1)

PyMethodDef g_NTeamPostMethods[] = {
	PO_ENTRIES(NTeamPost)
	{ "CheckEndOfGame", Nm_CheckEndOfTeamGame, 1, NULL },
	{ "GetGameDescriptor", Nm_TeamDescriptor, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
