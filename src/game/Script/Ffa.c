/*
 * The Ffa module in C (scripts/net/ffa.py, issue #2): the base of the online game modes. init.py preloads it for a
 * network game together with the mode of ServerType (NDeathMatch, NTeamDM, ...) and sets Ffa.ModLib to that mode;
 * OutMap.InitOutMap calls InitServer or InitClient. It keeps the scores, sends them from the server to the clients as
 * user strings ("S=", "S+", "S-", "T", "R", "E", "G", "!"), handles the deaths, joins, ship changes and exits,
 * spectating and the team menu, and fills the scorer of the HUD (RacerScorer). The mode supplies TeamGame, the menu
 * entries, DeathFunc, CheckEndOfGame, GetGameDescriptor, ShowHelp, ServerHidePlayer and its init functions.
 *
 * The module variables stay in the script and are read and written as its globals (the N* modes change them):
 * Scorer, isScorerVisible, TotalRed, TotalGreen, ModLib, SpectatorMode, GameFinished, USRCOLOR, STDCOLOR, REDCOLOR,
 * GREENCOLOR, MasterFlag, GameScorer, YourPosition, YourScore, Winner, ScorerFraglimit and GameStart. The functions
 * call each other through the module's globals.
 */
#include <limits.h>
#include <stdarg.h>

#include "scrap/script.h"

#define G(n) Sf_Global(&f, "Ffa", n)
#define LIB(n) Sf_Attr(&f, G("ModLib"), n)
#define ENT(n) Sf_Ask(&f, "Scrap", "GetEntity", "(s)", n)
#define GET(n) Sf_Ask(&f, "Scrap", "Get", "(s)", n)
#define STR(s) Sf_Str(&f, s)
#define LANG(k) Sf_Lang(&f, Sf_Str(&f, k))
#define LATER(d) Sf_Op(&f, Sf_Time(&f), '+', Sf_Float(&f, d))
#define SPECT() Sf_Mod(&f, "RacerScorer", "SPECTATOR_POINTS")
#define NOARGS() \
	scriptFrame_t f = { 0 }; \
	(void)self; \
	if (!PyArg_ParseTuple(args, "")) { \
		return NULL; \
	}
/* the eleven arguments of the client callbacks: entityname, ModelName, MaxLife, UsrName, PilotName, Motor0, Motor1,
 * Motor2, Motor3, WeaponBay, TgType */
#define CFGARGS(a) \
	PyObject *a[11]; \
	scriptFrame_t f = { 0 }; \
	(void)self; \
	if (!PyArg_ParseTuple(args, "OOOOOOOOOOO", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9], &a[10])) { \
		return NULL; \
	}

/* callable(Py_BuildValue args) as a statement: the result dropped */
static byte Ffa_CallV(scriptFrame_t *f, PyObject *callable, const char *fmt, va_list va) {
	int mark = f->n;
	if (callable && !f->failed) {
		PyObject *args = Py_VaBuildValue((char *)fmt, va);
		Sf_CallArgs(f, callable, Sf_Check(f, args != NULL) ? args : NULL);
	}
	Sf_Release(f, mark);
	return Sf_Check(f, callable != NULL);
}

/* func(args) of Ffa as its own functions call it (through the global), the result dropped */
static byte Ffa_Run(scriptFrame_t *f, const char *func, const char *fmt, ...) {
	int mark = f->n;
	PyObject *fn = Sf_Global(f, "Ffa", func);
	va_list va;
	va_start(va, fmt);
	Ffa_CallV(f, fn, fmt, va);
	va_end(va);
	Sf_Release(f, mark);
	return !f->failed;
}

/* ModLib.name(args), the result dropped */
static byte Ffa_Lib(scriptFrame_t *f, const char *name, const char *fmt, ...) {
	int mark = f->n;
	PyObject *fn = Sf_Attr(f, Sf_Global(f, "Ffa", "ModLib"), name);
	va_list va;
	va_start(va, fmt);
	Ffa_CallV(f, fn, fmt, va);
	va_end(va);
	Sf_Release(f, mark);
	return !f->failed;
}

/* SNet.SendUsrString(id, s) */
static byte Ffa_Send(scriptFrame_t *f, PyObject *id, PyObject *s) {
	return Sf_Run(f, "SNet", "SendUsrString", "(OO)", id, s);
}

/* SScorer.Set(id, item, var, v) through the module */
static byte Ffa_Set(scriptFrame_t *f, PyObject *id, PyObject *item, const char *var, PyObject *v) {
	return Sf_Run(f, "SScorer", "Set", "(OOsO)", id, item, var, v);
}

/* `ent.Pos = pos; ent.Ang = 0, ang, 0` */
static byte Ffa_Place(scriptFrame_t *f, PyObject *ent, PyObject *pos, PyObject *ang) {
	Sf_SetAttr(f, ent, "Pos", pos);
	return Sf_SetAttr(f, ent, "Ang", ang ? Sf_Build(f, "(iOi)", 0, ang) : NULL);
}

/* ---- game flow ---- */

/* InitFfa(): the mission scorer and the multiplayer menu entry of the mode */
static PyObject *Ffa_InitFfa(PyObject *self, PyObject *args) {
	NOARGS();
	Sf_Import(&f, "Menu");
	Sf_SetMod(&f, "Menu", "MissionScorerCall", G("FfaScorer"));
	Sf_SetMod(&f, "Menu", "MultiplayerMenu", LIB("MultiplayerMenuOnGame"));
	Sf_SetGlobal(&f, "Ffa", "SpectatorMode", Sf_Int(&f, 0));
	return Sf_None(&f);
}

/* SetupUserShip(entityname): the camera NetCam on the player's ship, the ship at a spawn point */
static PyObject *Ffa_SetupUserShip(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "OutMap");
	PyObject *cam = Sf_Ask(&f, "Scrap", "CreateEntity", "(sddds)", "NetCam", 0.0, 0.0, 0.0, "FCam");
	Sf_Run(&f, "Vehicles", "SetActiveCam", "(OOi)", name, Sf_Attr(&f, cam, "Name"), 0);
	PyObject *sp = Sf_Ask(&f, "OutMap", "GetSpawnPos", "()");
	PyObject *us = Sf_Entity(&f, name);
	Ffa_Place(&f, us, Sf_Item(&f, sp, 0), Sf_Item(&f, sp, 1));
	return Sf_None(&f);
}

/* RestartOnPress(id): fire pressed after a death, the restart a second later */
static PyObject *Ffa_RestartOnPress(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Run(&f, "SInput", "Bind", "(Osss)", id, "Racer", "Fire", "");
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "DelayedRestartOnTime");
	Sf_Later(&f, Sf_Float(&f, 1.0), G("DelayedRestartOnPress"), Sf_Build(&f, "(O)", id), "DelayedRestartOnPress");
	return Sf_None(&f);
}

/* DelayedRestartOnPress(id): the dead player's ship comes back (the client asks the server) */
static PyObject *Ffa_DelayedRestartOnPress(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "RacerScorer");
	int c = Sf_True(&f, G("GameFinished"));
	if (c == 0) {
		c = Sf_Compare(&f, G("YourScore"), "==", SPECT());
	}
	if (c != 0) {
		return Sf_None(&f);
	}
	PyObject *cam = ENT("NetCam");
	if (Sf_Compare(&f, cam, "==", Py_None) != 0) {
		return Sf_None(&f);
	}
	PyObject *tgt = Sf_Entity(&f, Sf_Attr(&f, cam, "MainTarget"));
	if (Sf_True(&f, Sf_Attr(&f, tgt, "Active")) != 0) {
		return Sf_None(&f);
	}
	Sf_SetMod(&f, "RacerScorer", "ScorerTopMessage", Sf_Method(&f, G("ModLib"), "GetGameDescriptor", "()"));
	Sf_SetMod(&f, "RacerScorer", "ScorerBotMessage", STR(""));
	Ffa_Run(&f, "ShowScorer", "(Oi)", id, 0);
	PyObject *net = Sf_Ask(&f, "Scrap", "GetNetFlags", "()");
	int client = Sf_True(&f, Sf_Item(&f, net, 0));
	if (client > 0) {
		Ffa_Send(&f, Sf_ToInt(&f, Sf_Slice(&f, Sf_Attr(&f, tgt, "Name"), 3, INT_MAX)), STR("R"));
	} else if (client == 0) {
		Ffa_Run(&f, "AbortSpectateMode", "()");
		Ffa_Run(&f, "SetSpawnPos", "(O)", Sf_Attr(&f, tgt, "Name"));
	}
	Sf_Run(&f, "SScorer", "SetMPFunc", "(is)", 0, "Ffa.ShowScorer");
	return Sf_None(&f);
}

/* OnDeath(LooserName, WinnerName): the death or kill message of the player of this machine */
static PyObject *Ffa_OnDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	PyObject *cam = ENT("NetCam");
	if (Sf_True(&f, cam) <= 0) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, looser, "==", Sf_Attr(&f, cam, "MainTarget")) > 0) {
		PyObject *bad = Sf_Entity(&f, winner);
		int b = Sf_True(&f, bad);
		if (b > 0) {
			Ffa_Run(&f, "ShowPresFireMessage", "(iO)", 0, Sf_Op(&f, LANG("Net_Title_Death_Kill"), '%', Sf_Attr(&f, bad, "Descriptor")));
		} else if (b == 0) {
			Ffa_Run(&f, "ShowPresFireMessage", "(iO)", 0, LANG("Net_Title_Death_Suicide"));
		}
	} else if (!f.failed && Sf_Compare(&f, winner, "==", Sf_Attr(&f, cam, "MainTarget")) > 0) {
		PyObject *bad = Sf_Entity(&f, looser);
		PyObject *text = Sf_Op(&f, LANG("Net_Msg_You_Kill"), '%', Sf_Attr(&f, bad, "Descriptor"));
		Sf_Run(&f, "SScorer", "SetMsgText", "(OO)", text, Sf_OpI(&f, Sf_Time(&f), '+', 5));
	}
	return Sf_None(&f);
}

/* ShowPresFireMessage(id, msg): the scorer with msg and "press fire"; fire restarts, after 10 s the restart is
 * automatic */
static PyObject *Ffa_ShowPresFireMessage(PyObject *self, PyObject *args) {
	PyObject *id, *msg;
	SF_ARGS("OO", &id, &msg);
	if (Sf_True(&f, G("GameFinished")) != 0) {
		return Sf_None(&f);
	}
	Sf_Import(&f, "RacerScorer");
	Sf_SetMod(&f, "RacerScorer", "ScorerTopMessage", msg);
	Sf_SetMod(&f, "RacerScorer", "ScorerBotMessage", LANG("Net_Bott_Press_Fire"));
	Ffa_Run(&f, "ShowScorer", "(Oi)", id, 1);
	PyObject *timy = Sf_Time(&f);
	Sf_Schedule(&f, Sf_Op(&f, timy, '+', Sf_Float(&f, 1.0)), Sf_Mod(&f, "SInput", "Bind"),
		Sf_Build(&f, "(Osss)", id, "Racer", "Fire", "Ffa.RestartOnPress"), NULL);
	Sf_Run(&f, "SScorer", "SetMPFunc", "(is)", 0, "");
	if (Sf_Compare(&f, timy, ">", Sf_Int(&f, 10)) > 0) {
		Sf_Schedule(&f, Sf_Op(&f, timy, '+', Sf_Float(&f, 10.0)), G("DelayedRestartOnTime"), Sf_Build(&f, "(O)", id),
			"DelayedRestartOnTime");
	}
	return Sf_None(&f);
}

/* DelayedRestartOnTime(id): the automatic restart */
static PyObject *Ffa_DelayedRestartOnTime(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "DelayedRestartOnPress");
	Sf_Run(&f, "SInput", "Bind", "(Osss)", id, "Racer", "Fire", "");
	Ffa_Run(&f, "DelayedRestartOnPress", "(O)", id);
	return Sf_None(&f);
}

/* EndOfGame(): the results on the scorer, the screen fades to noise */
static PyObject *Ffa_EndOfGame(PyObject *self, PyObject *args) {
	NOARGS();
	Sf_Import(&f, "RacerScorer");
	Sf_SetGlobal(&f, "Ffa", "GameFinished", Sf_Int(&f, 1));
	Sf_Run(&f, "SInput", "Bind", "(isss)", 0, "Racer", "Fire", "");
	Sf_Run(&f, "SScorer", "SetMPFunc", "(is)", 0, "");
	int team = Sf_True(&f, LIB("TeamGame"));
	if (team > 0) {
		Sf_SetMod(&f, "RacerScorer", "ScorerTopMessage", Sf_Own(&f, "Ffa", "GetGameTeamDescriptor", "()"));
	} else if (team == 0) {
		Sf_SetMod(&f, "RacerScorer", "ScorerTopMessage", LANG("Net_Title_Game_Results"));
	}
	Sf_SetMod(&f, "RacerScorer", "ScorerBotMessage", LANG("Net_Bott_Wait_Next_Match"));
	Ffa_Run(&f, "ShowScorer", "(ii)", 0, 1);
	if (Sf_Compare(&f, Sf_Ask(&f, "SInput", "GetActionSet", "()"), "!=", STR("Menu")) > 0) {
		Sf_Run(&f, "Scrap", "Set", "(si)", "NoiseFactorBegin", 0);
		Sf_Run(&f, "Scrap", "Set", "(si)", "NoiseFactor", 32);
		Sf_Run(&f, "Scrap", "Set", "(si)", "NoiseTime", 0);
		Sf_Run(&f, "Scrap", "Set", "(sd)", "NoiseEndTime", 1.0);
		Sf_Run(&f, "SScorer", "Fade", "(i(iiii)(iiii)d)", 0, 0, 0, 0, 0, 0, 0, 0, 128, 1.0);
	}
	return Sf_None(&f);
}

/* ServerSendPlayerStatus(WinnerName): the score of one player to all clients */
static PyObject *Ffa_ServerSendPlayerStatus(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	PyObject *s = Sf_Op(&f, Sf_Op(&f, STR("S+"), '+', name), '+', STR(":"));
	s = Sf_Op(&f, s, '+', Sf_Repr(&f, Sf_GetItem(&f, G("Scorer"), name)));
	Ffa_Send(&f, Sf_Int(&f, -1), s);
	Ffa_Run(&f, "ServerSendTeamData", "()");
	return Sf_None(&f);
}

/* ServerSendTeamData(): the team totals to all clients */
static PyObject *Ffa_ServerSendTeamData(PyObject *self, PyObject *args) {
	NOARGS();
	if (Sf_True(&f, LIB("TeamGame")) > 0) {
		PyObject *s = Sf_Op(&f, STR("T"), '+', Sf_Repr(&f, G("TotalRed")));
		s = Sf_Op(&f, Sf_Op(&f, s, '+', STR(",")), '+', Sf_Repr(&f, G("TotalGreen")));
		Ffa_Send(&f, Sf_Int(&f, -1), s);
	}
	return Sf_None(&f);
}

/* ServerEndOfGame(): the frag limit is reached: the ships hide, the next map of ServerMapList loads 10 s later */
static PyObject *Ffa_ServerEndOfGame(PyObject *self, PyObject *args) {
	NOARGS();
	Sf_Import(&f, "Init");
	Sf_Import(&f, "Net");
	Ffa_Send(&f, Sf_Int(&f, -1), STR("E"));
	PyObject *e1 = Sf_Ask(&f, "Scrap", "GetFirst", "()");
	int mark = f.n;
	while (Sf_True(&f, e1) > 0) {
		if (Sf_True(&f, Sf_Method(&f, e1, "IsType", "(s)", "Car")) > 0) {
			Sf_SetAttrI(&f, e1, "Active", 0);
		}
		e1 = Sf_Entity(&f, Sf_Attr(&f, e1, "NextInSlot"));
		Sf_ReleaseKeep(&f, mark, &e1, 1);
	}
	Sf_Run(&f, "Net", "PurgeServerMapList", "()");
	PyObject *maps = Sf_Eval(&f, GET("ServerMapList"), "Ffa"), *next = NULL;
	int empty = Sf_True(&f, maps);
	if (empty >= 0) {
		empty = !empty;
		if (!empty) {
			int n = Sf_Len(&f, maps);
			empty = n < 0 ? -1 : n == 0;
		}
	}
	if (empty > 0) {
		next = Sf_Mod(&f, "Init", "Path");
	} else if (empty == 0) {
		PyObject *tail = Sf_Slice(&f, Sf_Mod(&f, "Init", "Path"), 7, INT_MAX);
		int in = Sf_In(&f, tail, maps);
		if (in > 0) {
			PyObject *at = Sf_OpI(&f, Sf_Method(&f, maps, "index", "(O)", Sf_Slice(&f, Sf_Mod(&f, "Init", "Path"), 7, INT_MAX)), '+', 1);
			at = Sf_Op(&f, at, '%', Sf_Int(&f, Sf_Len(&f, maps)));
			next = Sf_Op(&f, STR("Levels/"), '+', Sf_GetItem(&f, maps, at));
		} else if (in == 0) {
			next = Sf_Op(&f, STR("Levels/"), '+', Sf_Item(&f, maps, 0));
		}
	}
	Sf_Later(&f, Sf_Float(&f, 10.0), Sf_Mod(&f, "SNet", "ServerChangeLevel"), Sf_Build(&f, "(O)", next), NULL);
	Ffa_Run(&f, "EndOfGame", "()");
	return Sf_None(&f);
}

/* ---- deaths ---- */

/* OnDeathServer(LooserName, WinnerName): the server counts the frag (a suicide or a team kill costs one), sends the
 * changed scores and checks the end of the game */
static PyObject *Ffa_OnDeathServer(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *xname, *add;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "Init");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	int w = Sf_Compare(&f, winner, "!=", STR(""));
	if (w > 0) {
		xname = winner;
		int same = Sf_True(&f, LIB("TeamGame"));
		if (same > 0) {
			same = Sf_Compare(&f, Sf_Attr(&f, Sf_Entity(&f, looser), "TgType"), "==", Sf_Attr(&f, Sf_Entity(&f, winner), "TgType"));
		}
		add = Sf_Int(&f, same > 0 ? -1 : 1);
	} else {
		xname = looser;
		add = Sf_Int(&f, -1);
	}
	if (Sf_True(&f, Sf_Method(&f, G("Scorer"), "has_key", "(O)", xname)) == 0) {
		Sf_SetItem(&f, G("Scorer"), xname, Sf_Int(&f, 0));
	}
	PyObject *xprev = Sf_GetItem(&f, G("Scorer"), xname);
	PyObject *lprev = Sf_GetItem(&f, G("Scorer"), looser);
	Ffa_Lib(&f, "DeathFunc", "(OOO)", xname, looser, add);
	Ffa_Run(&f, "ActualizeGameScorer", "()");
	if (Sf_Compare(&f, xprev, "!=", Sf_GetItem(&f, G("Scorer"), xname)) > 0) {
		Ffa_Run(&f, "ServerSendPlayerStatus", "(O)", xname);
	}
	int c = Sf_Compare(&f, xname, "!=", looser);
	if (c > 0) {
		c = Sf_Compare(&f, lprev, "!=", Sf_GetItem(&f, G("Scorer"), looser));
	}
	if (c > 0) {
		Ffa_Run(&f, "ServerSendPlayerStatus", "(O)", looser);
	}
	if (Sf_True(&f, Sf_Method(&f, G("ModLib"), "CheckEndOfGame", "()")) == 0) {
		Ffa_Run(&f, "OnDeath", "(OO)", looser, winner);
	}
	Ffa_Lib(&f, "ServerHidePlayer", "(O)", looser);
	return Sf_None(&f);
}

/* OnDeathClient(LooserName, WinnerName) */
static PyObject *Ffa_OnDeathClient(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnClientDeath", "(OO)", looser, winner);
	Ffa_Run(&f, "OnDeath", "(OO)", looser, winner);
	return Sf_None(&f);
}

/* ---- the scorer strings ---- */

/* Scorer2Str(): "<n>,<score>-..." of all players (net<n>) */
static PyObject *Ffa_Scorer2Str(PyObject *self, PyObject *args) {
	NOARGS();
	PyObject *cad = STR("");
	PyObject *keys = Sf_Method(&f, G("Scorer"), "keys", "()");
	int mark = f.n;
	for (int i = 0; !f.failed; i++) {
		PyObject *ky = Sf_Next(&f, keys, i);
		if (!ky) {
			break;
		}
		PyObject *s = Sf_Op(&f, Sf_Op(&f, cad, '+', Sf_Slice(&f, ky, 3, INT_MAX)), '+', STR(","));
		cad = Sf_Op(&f, Sf_Op(&f, s, '+', Sf_Repr(&f, Sf_GetItem(&f, G("Scorer"), ky))), '+', STR("-"));
		Sf_ReleaseKeep(&f, mark, &cad, 1);
	}
	return Sf_Return(&f, Sf_Slice(&f, cad, 0, -1));
}

/* Str2Scorer(cad): the scores from the text of Scorer2Str */
static PyObject *Ffa_Str2Scorer(PyObject *self, PyObject *args) {
	PyObject *cad;
	SF_ARGS("O", &cad);
	Sf_Import(&f, "string");
	Sf_SetGlobal(&f, "Ffa", "Scorer", Sf_Keep(&f, PyDict_New()));
	PyObject *parts = Sf_Ask(&f, "string", "split", "(Os)", cad, "-");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *ky = Sf_Next(&f, parts, i);
		if (!ky) {
			break;
		}
		PyObject *vals = Sf_Ask(&f, "string", "split", "(Os)", ky, ",");
		PyObject *v = Sf_ToInt(&f, Sf_Item(&f, vals, 1));
		Sf_SetItem(&f, G("Scorer"), Sf_Op(&f, STR("net"), '+', Sf_Item(&f, vals, 0)), v);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* ---- clients joining, changing ships, leaving ---- */

/* ClientJoin(entityname, ...): a new player on the server: its ship, its score 0 to all, the scores and the rules to
 * it */
static PyObject *Ffa_ClientJoin(PyObject *self, PyObject *args) {
	CFGARGS(a);
	Sf_Import(&f, "Net");
	Sf_CallArgs(&f, Sf_Mod(&f, "Net", "ClientJoin"), Script_Ref(args));
	PyObject *ply = Sf_CallArgs(&f, G("ClientSetConfig"), Script_Ref(args));
	Ffa_Run(&f, "SetSpawnPos", "(O)", Sf_Attr(&f, ply, "Name"));
	Sf_SetItem(&f, G("Scorer"), a[0], Sf_Int(&f, 0));
	Ffa_Run(&f, "ServerSendPlayerStatus", "(O)", a[0]);
	PyObject *cid = Sf_ToInt(&f, Sf_Slice(&f, a[0], 3, INT_MAX));
	Ffa_Send(&f, cid, Sf_Op(&f, STR("S="), '+', Sf_Own(&f, "Ffa", "Scorer2Str", "()")));
	Ffa_Run(&f, "ServerSendTeamData", "()");
	Ffa_Run(&f, "SendGameRules", "(O)", cid);
	Sf_SetAttrI(&f, ply, "Active", 0);
	Ffa_Run(&f, "ActualizeGameScorer", "()");
	Ffa_Lib(&f, "ServerHidePlayer", "(O)", Sf_Attr(&f, ply, "Name"));
	Ffa_Run(&f, "PlayerJoinGame", "(O)", Sf_Attr(&f, ply, "Name"));
	return Sf_None(&f);
}

/* BuildCfgString(): the player's ship as the "M" message sends it, one value per line */
static PyObject *Ffa_BuildCfgString(PyObject *self, PyObject *args) {
	static const char *const vars[] = { "PlayerModel", "PlayerMaxLife", "PlayerName", "PilotModel", "Motor0Model",
		"Motor1Model", "Motor2Model", "Motor3Model", "WeaponBayList", "PlayerTeamID" };
	NOARGS();
	PyObject *s = NULL;
	for (int i = 0; i < 10; i++) {
		PyObject *v = GET(vars[i]);
		if (i == 1 || i == 9) {
			v = Sf_Repr(&f, v);
		}
		s = i ? Sf_Op(&f, Sf_Op(&f, s, '+', STR("\n")), '+', v) : v;
	}
	return Sf_Return(&f, s);
}

/* ModiTry(): the player's ship changed (the ship editor): the client asks the server, the server changes its own */
static PyObject *Ffa_ModiTry(PyObject *self, PyObject *args) {
	NOARGS();
	int c = Sf_True(&f, Sf_Ask(&f, "SNet", "IsClient", "()"));
	if (c > 0) {
		Ffa_Send(&f, Sf_Int(&f, 0), Sf_Op(&f, STR("M"), '+', Sf_Own(&f, "Ffa", "BuildCfgString", "()")));
	} else if (c == 0 && Sf_True(&f, Sf_Ask(&f, "SNet", "IsServer", "()")) > 0) {
		PyObject *ename = Sf_Attr(&f, ENT("NetCam"), "MainTarget");
		Ffa_Run(&f, "ClientModify", "(OOOOOOOOOOO)", ename, GET("PlayerModel"), GET("PlayerMaxLife"), GET("PlayerName"),
			GET("PilotModel"), GET("Motor0Model"), GET("Motor1Model"), GET("Motor2Model"), GET("Motor3Model"),
			GET("WeaponBayList"), GET("PlayerTeamID"));
	}
	return Sf_None(&f);
}

/* ModifyOnReyoin(entityname, ...): the ship of a rejoining player gets its new set up */
static PyObject *Ffa_ModifyOnReyoin(PyObject *self, PyObject *args) {
	CFGARGS(a);
	PyObject *ply = Sf_Entity(&f, a[0]);
	Sf_SetAttrS(&f, ply, "ActCtrl", "");
	Sf_Method(&f, ply, "SetModel", "(s)", "");
	Sf_CallArgs(&f, G("ClientSetConfig"), Script_Ref(args));
	Sf_SetAttrI(&f, ply, "Active", 0);
	return Sf_None(&f);
}

/* ClientModify(entityname, ...): a pending change of the ship replaced by this one */
static PyObject *Ffa_ClientModify(PyObject *self, PyObject *args) {
	CFGARGS(a);
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(O)", Sf_Op(&f, a[0], '+', STR("Modify")));
	Sf_CallArgs(&f, G("TryToModify"), Script_Ref(args));
	return Sf_None(&f);
}

/* TryToModify(entityname, ...): the ship changes while it is inactive, else it tries again 0.75 s later */
static PyObject *Ffa_TryToModify(PyObject *self, PyObject *args) {
	CFGARGS(a);
	PyObject *ply = Sf_Entity(&f, a[0]);
	int c = Sf_True(&f, ply);
	if (c > 0) {
		c = Sf_True(&f, Sf_Attr(&f, ply, "Active"));
		c = c < 0 ? c : !c;
	}
	if (c > 0) {
		Sf_CallArgs(&f, G("ModifyOnReyoin"), Script_Ref(args));
		Sf_Run(&f, "SNet", "ModifyUsrData", "(O)", Sf_ToInt(&f, Sf_Slice(&f, a[0], 3, INT_MAX)));
	} else if (c == 0) {
		PyObject *t = LATER(0.75), *fn = G("TryToModify");
		Sf_Ask(&f, "Scrap", "AddScheduledFunc", "(OOOO)", t, fn, args, Sf_Op(&f, a[0], '+', STR("Modify")));
	}
	return Sf_None(&f);
}

/* PlayerMsgShow(streang, entityname): streang % the player's name for 2.5 s (not in the first 2.5 s of a level) */
static PyObject *Ffa_PlayerMsgShow(PyObject *self, PyObject *args) {
	PyObject *text, *name;
	SF_ARGS("OO", &text, &name);
	if (Sf_Compare(&f, Sf_Time(&f), ">", Sf_Float(&f, 2.5)) > 0) {
		PyObject *e = Sf_Entity(&f, name);
		int c = Sf_True(&f, e);
		if (c > 0) {
			c = Sf_Compare(&f, Sf_Attr(&f, e, "Descriptor"), "!=", STR(""));
		}
		if (c > 0) {
			PyObject *msg = Sf_Op(&f, text, '%', Sf_Attr(&f, e, "Descriptor"));
			Sf_Run(&f, "SScorer", "SetMsgText", "(OO)", msg, LATER(2.5));
		}
	}
	return Sf_None(&f);
}

/* PlayerDisconected(entityname), PlayerSpectator(entityname) */
static PyObject *Ffa_Msg(PyObject *self, PyObject *args, const char *text) {
	PyObject *name;
	SF_ARGS("O", &name);
	Ffa_Run(&f, "PlayerMsgShow", "(sO)", text, name);
	return Sf_None(&f);
}

static PyObject *Ffa_PlayerDisconected(PyObject *self, PyObject *args) {
	return Ffa_Msg(self, args, "%s leaves the match.");
}

static PyObject *Ffa_PlayerSpectator(PyObject *self, PyObject *args) {
	return Ffa_Msg(self, args, "%s becomes spectator.");
}

/* PlayerJoinGame(entityname): the join message of the player's team */
static PyObject *Ffa_PlayerJoinGame(PyObject *self, PyObject *args) {
	static const char *const types[][2] = { { "killer", "%s joins the game." }, { "red", "%s joins red team." },
		{ "green", "%s joins green team." } };
	PyObject *name;
	SF_ARGS("O", &name);
	PyObject *e = Sf_Entity(&f, name);
	if (Sf_True(&f, e) > 0) {
		for (int i = 0; i < 3; i++) {
			int c = Sf_EqS(&f, Sf_Attr(&f, e, "TgType"), types[i][0]);
			if (c != 0) {
				if (c > 0) {
					Ffa_Run(&f, "PlayerMsgShow", "(sO)", types[i][1], name);
				}
				break;
			}
		}
	}
	return Sf_None(&f);
}

/* ClientExit(entityname, ModelName, UsrName): a player leaves: its score goes */
static PyObject *Ffa_ClientExit(PyObject *self, PyObject *args) {
	PyObject *name, *model, *usr;
	SF_ARGS("OOO", &name, &model, &usr);
	Ffa_Lib(&f, "ServerHidePlayer", "(O)", name);
	if (Sf_True(&f, Sf_Method(&f, G("Scorer"), "has_key", "(O)", name)) > 0) {
		Ffa_Run(&f, "PlayerDisconected", "(O)", name);
		Sf_DelItem(&f, G("Scorer"), name);
		Ffa_Send(&f, Sf_Int(&f, -1), Sf_Op(&f, STR("S-"), '+', name));
		Ffa_Run(&f, "ActualizeGameScorer", "()");
	}
	return Sf_None(&f);
}

/* ---- spawning ---- */

/* ReSpawnClient(entityName): the ship of a client comes back */
static PyObject *Ffa_ReSpawnClient(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "RacerScorer");
	Ffa_Run(&f, "AbortSpectateMode", "()");
	if (Sf_EqS(&f, Sf_Ask(&f, "SInput", "GetActionSet", "()"), "Racer") > 0) {
		Sf_Run(&f, "RacerScorer", "Create", "(i)", 0);
	}
	Sf_Run(&f, "Vehicles", "ReSpawnShip", "(O)", name);
	Sf_SetAttrS(&f, Sf_Entity(&f, name), "OnDeath", "Ffa.OnDeathClient");
	return Sf_None(&f);
}

/* ServerRemoveSpectatorState(entityName): a spectator plays again with 0 points */
static PyObject *Ffa_ServerRemoveSpectatorState(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	static const char *const mods[] = { "Vehicles", "Net", "OutMap", "RacerScorer" };
	for (int i = 0; i < 4; i++) {
		Sf_Import(&f, mods[i]);
	}
	int c = Sf_True(&f, Sf_Method(&f, G("Scorer"), "has_key", "(O)", name));
	if (c > 0) {
		c = Sf_Compare(&f, Sf_GetItem(&f, G("Scorer"), name), "==", SPECT());
	}
	if (c > 0) {
		Sf_SetItem(&f, G("Scorer"), name, Sf_Int(&f, 0));
		Ffa_Run(&f, "ServerSendPlayerStatus", "(O)", name);
		Ffa_Run(&f, "ActualizeGameScorer", "()");
		PyObject *cam = ENT("NetCam");
		int j = Sf_True(&f, cam);
		j = j < 0 ? j : !j;
		if (j == 0) {
			j = Sf_Compare(&f, Sf_Attr(&f, cam, "MainTarget"), "!=", name);
		}
		if (j > 0) {
			Ffa_Run(&f, "PlayerJoinGame", "(O)", name);
		}
	}
	return Sf_None(&f);
}

/* SetSpawnPos(entityName): the server puts a ship back at a free spawn point (next to the one there when taken) */
static PyObject *Ffa_SetSpawnPos(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	static const char *const mods[] = { "Vehicles", "Net", "OutMap", "RacerScorer" };
	for (int i = 0; i < 4; i++) {
		Sf_Import(&f, mods[i]);
	}
	Ffa_Run(&f, "ServerRemoveSpectatorState", "(O)", name);
	Sf_Run(&f, "Vehicles", "ReSpawnShip", "(O)", name);
	PyObject *entity = Sf_Entity(&f, name);
	PyObject *pos = Sf_Ask(&f, "OutMap", "GetFreeSpawn", "()");
	if (Sf_True(&f, Sf_Mod(&f, "OutMap", "SpawRes")) > 0) {
		PyObject *e = Sf_Entity(&f, Sf_Mod(&f, "OutMap", "SpawRes"));
		PyObject *dist = Sf_Op(&f, Sf_Attr(&f, e, "VRadius"), '+', Sf_Attr(&f, entity, "VRadius"));
		dist = Sf_OpI(&f, Sf_OpI(&f, dist, '+', 10), '/', 2);
		PyObject *at = Sf_Ask(&f, "SVec", "Add", "(O(OOO))", Sf_Attr(&f, e, "Pos"), dist, dist, dist);
		pos = Sf_Build(&f, "(OO)", at, Sf_Item(&f, pos, 1));
	}
	Ffa_Place(&f, entity, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1));
	Sf_SetAttrS(&f, entity, "OnDeath", "Ffa.OnDeathServer");
	return Sf_None(&f);
}

/* ---- server and client start ---- */

/* InitServer(dedicated): the server's callbacks, the server player's own ship unless dedicated, the mode */
static PyObject *Ffa_InitServer(PyObject *self, PyObject *args) {
	static const char *const cbs[][2] = { { "OnClientJoin", "Ffa.ClientJoin" }, { "OnClientModify", "Ffa.ModifyOnReyoin" },
		{ "OnClientExit", "Ffa.ClientExit" }, { "OnUsrString", "Ffa.OnUsrServer" } };
	PyObject *dedicated;
	SF_ARGS("O", &dedicated);
	Ffa_Run(&f, "InitFfa", "()");
	for (int i = 0; i < 4; i++) {
		Sf_Run(&f, "Scrap", "Set", "(ss)", cbs[i][0], cbs[i][1]);
	}
	if (Sf_True(&f, dedicated) == 0) {
		PyObject *ply = Sf_Ask(&f, "Scrap", "CreateEntity", "(Oddds)", Sf_Ask(&f, "SNet", "GetBotName", "()"), 0.0, 0.0, 0.0, "Car");
		PyObject *pname = Sf_Attr(&f, ply, "Name");
		Ffa_Run(&f, "ClientJoin", "(OOOOOOOOOOO)", pname, GET("PlayerModel"), GET("PlayerMaxLife"), GET("PlayerName"),
			GET("PilotModel"), GET("Motor0Model"), GET("Motor1Model"), GET("Motor2Model"), GET("Motor3Model"),
			GET("WeaponBayList"), GET("PlayerTeamID"));
		Ffa_Run(&f, "SetupUserShip", "(O)", Sf_Attr(&f, ply, "Name"));
		Sf_SetAttr(&f, ply, "Descriptor", GET("PlayerName"));
		Sf_SetAttrI(&f, ply, "Active", 0);
		Ffa_Run(&f, "StdInitFfa", "()");
	}
	Ffa_Lib(&f, "InitServerMod", "()");
	return Sf_None(&f);
}

/* ClientSetConfig(entityname, ...): the ship of a player set up (the server's own ship when ForceServerVehicle), its
 * team; a client respawns it or names the death callback */
static PyObject *Ffa_ClientSetConfig(PyObject *self, PyObject *args) {
	CFGARGS(a);
	PyObject *v = NULL;
	Sf_Import(&f, "Vehicles");
	int force = Sf_True(&f, GET("ForceServerVehicle"));
	if (force > 0) {
		v = Sf_Ask(&f, "Vehicles", "SetProperties", "(OOOOOOOOOO)", a[0], GET("PlayerModel"), GET("PlayerMaxLife"), a[4],
			GET("Motor0Model"), GET("Motor1Model"), GET("Motor2Model"), GET("Motor3Model"), GET("WeaponBayList"), a[3]);
	} else if (force == 0) {
		v = Sf_Ask(&f, "Vehicles", "SetProperties", "(OOOOOOOOOO)", a[0], a[1], a[2], a[4], a[5], a[6], a[7], a[8], a[9], a[3]);
	}
	int team = Sf_True(&f, LIB("TeamGame"));
	if (team > 0) {
		int red = Sf_EqI(&f, a[10], 0);
		if (red >= 0) {
			Sf_SetAttrS(&f, v, "TgType", red ? "red" : "green");
		}
	} else if (team == 0) {
		Sf_SetAttrS(&f, v, "TgType", "killer");
	}
	PyObject *net = Sf_Ask(&f, "Scrap", "GetNetFlags", "()");
	if (Sf_True(&f, Sf_Item(&f, net, 0)) > 0) {
		int act = Sf_True(&f, Sf_Attr(&f, v, "Active"));
		if (act > 0) {
			Ffa_Run(&f, "ReSpawnClient", "(O)", a[0]);
		} else if (act == 0 && Sf_Compare(&f, Sf_Attr(&f, ENT("NetCam"), "MainTarget"), "!=", a[0]) > 0) {
			Sf_SetAttrS(&f, Sf_Entity(&f, a[0]), "OnDeath", "Ffa.OnDeathClient");
		}
	}
	Ffa_Run(&f, "ActualizeGameScorer", "()");
	return Sf_Return(&f, v);
}

/* ClientAfterCreate(EntityName, ResourceName), ClientCreate(EntityName, ResourceName): on to Net's callbacks */
static PyObject *Ffa_NetCreate(PyObject *self, PyObject *args, const char *func) {
	PyObject *name, *res;
	SF_ARGS("OO", &name, &res);
	Sf_Import(&f, "Net");
	Sf_Run(&f, "Net", func, "(OO)", name, res);
	return Sf_None(&f);
}

static PyObject *Ffa_ClientAfterCreate(PyObject *self, PyObject *args) {
	return Ffa_NetCreate(self, args, "ClientAfterCreate");
}

static PyObject *Ffa_ClientCreate(PyObject *self, PyObject *args) {
	return Ffa_NetCreate(self, args, "ClientCreate");
}

/* ---- the user strings ---- */

/* OnUsrTest(id, str): prints them */
static PyObject *Ffa_OnUsrTest(PyObject *self, PyObject *args) {
	PyObject *id, *s;
	SF_ARGS("OO", &id, &s);
	Sf_Print(&f, "OO", id, s);
	return Sf_None(&f);
}

/* SvrShowMsg(Msg): a message on the server and all clients */
static PyObject *Ffa_SvrShowMsg(PyObject *self, PyObject *args) {
	PyObject *msg;
	SF_ARGS("O", &msg);
	Sf_Run(&f, "SScorer", "SetMsgText", "(OO)", msg, Sf_OpI(&f, Sf_Time(&f), '+', 5));
	Ffa_Send(&f, Sf_Int(&f, -1), Sf_Op(&f, STR("!"), '+', msg));
	return Sf_None(&f);
}

/* OnUsrClient(id, s): a message of the server on a client: a text, the scores, the team totals, the respawn, the end,
 * the rules */
static PyObject *Ffa_OnUsrClient(PyObject *self, PyObject *args) {
	PyObject *id, *s;
	SF_ARGS("OO", &id, &s);
	Sf_Import(&f, "string");
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "Net");
	PyObject *c0 = Sf_Item(&f, s, 0);
	if (Sf_EqS(&f, c0, "!") > 0) {
		Sf_Run(&f, "SScorer", "SetMsgText", "(OO)", Sf_Slice(&f, s, 1, INT_MAX), Sf_OpI(&f, Sf_Time(&f), '+', 5));
	}
	if (Sf_EqS(&f, c0, "S") > 0) {
		PyObject *c1 = Sf_Item(&f, s, 1);
		if (Sf_EqS(&f, c1, "=") > 0) {
			Ffa_Run(&f, "Str2Scorer", "(O)", Sf_Slice(&f, s, 2, INT_MAX));
		} else if (!f.failed && Sf_EqS(&f, c1, "+") > 0) {
			PyObject *lst = Sf_Ask(&f, "string", "split", "(Os)", Sf_Slice(&f, s, 2, INT_MAX), ":");
			PyObject *points = Sf_ToInt(&f, Sf_Item(&f, lst, 1));
			int mainusr = Sf_Compare(&f, Sf_Item(&f, lst, 0), "==", Sf_Attr(&f, ENT("NetCam"), "MainTarget"));
			if (mainusr == 0) {
				int has = Sf_True(&f, Sf_Method(&f, G("Scorer"), "has_key", "(O)", Sf_Item(&f, lst, 0)));
				if (has == 0) {
					Sf_Later(&f, Sf_Float(&f, 1.0), G("PlayerJoinGame"), Sf_Build(&f, "(O)", Sf_Item(&f, lst, 0)), NULL);
				} else if (has > 0) {
					int j = Sf_Compare(&f, Sf_GetItem(&f, G("Scorer"), Sf_Item(&f, lst, 0)), "==", SPECT());
					if (j > 0) {
						j = Sf_Compare(&f, points, "!=", SPECT());
					}
					if (j > 0) {
						Ffa_Run(&f, "PlayerJoinGame", "(O)", Sf_Item(&f, lst, 0));
					}
				}
			}
			Sf_SetItem(&f, G("Scorer"), Sf_Item(&f, lst, 0), points);
			if (Sf_Compare(&f, points, "==", SPECT()) > 0) {
				if (mainusr) {
					Ffa_Run(&f, "CameraSpectator", "()");
				} else {
					Ffa_Run(&f, "PlayerSpectator", "(O)", Sf_Item(&f, lst, 0));
				}
			}
		} else if (!f.failed && Sf_EqS(&f, c1, "-") > 0) {
			Ffa_Run(&f, "PlayerDisconected", "(O)", Sf_Slice(&f, s, 2, INT_MAX));
			Sf_DelItem(&f, G("Scorer"), Sf_Slice(&f, s, 2, INT_MAX));
		}
		Ffa_Run(&f, "ActualizeGameScorer", "()");
	} else if (!f.failed && Sf_EqS(&f, c0, "T") > 0) {
		PyObject *tot[2];
		if (Sf_Unpack(&f, Sf_Eval(&f, Sf_Slice(&f, s, 1, INT_MAX), "Ffa"), 2, tot)) {
			Sf_SetGlobal(&f, "Ffa", "TotalRed", tot[0]);
			Sf_SetGlobal(&f, "Ffa", "TotalGreen", tot[1]);
		}
		Ffa_Run(&f, "ActualizeGameScorer", "()");
	} else if (!f.failed && Sf_EqS(&f, c0, "R") > 0) {
		if (Sf_True(&f, G("GameFinished")) == 0) {
			Ffa_Run(&f, "ReSpawnClient", "(O)", Sf_Attr(&f, ENT("NetCam"), "MainTarget"));
		}
	} else if (!f.failed && Sf_EqS(&f, c0, "E") > 0) {
		Ffa_Run(&f, "EndOfGame", "()");
	} else if (!f.failed && Sf_EqS(&f, c0, "G") > 0) {
		Ffa_Run(&f, "SetupGameRules", "(O)", Sf_Slice(&f, s, 1, INT_MAX));
	}
	return Sf_None(&f);
}

/* OnUsrServer(id, s): a request of client id on the server: respawn, spectate, a new ship */
static PyObject *Ffa_OnUsrServer(PyObject *self, PyObject *args) {
	PyObject *id, *s;
	SF_ARGS("OO", &id, &s);
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "string");
	PyObject *e = Sf_Entity(&f, Sf_Op(&f, STR("net"), '+', Sf_Repr(&f, id)));
	PyObject *c0 = Sf_Item(&f, s, 0);
	if (Sf_EqS(&f, c0, "R") > 0) {
		if (Sf_True(&f, G("GameFinished")) == 0) {
			int c = Sf_True(&f, e);
			if (c > 0) {
				Ffa_Run(&f, "SetSpawnPos", "(O)", Sf_Attr(&f, e, "Name"));
				Ffa_Send(&f, id, STR("R"));
			} else if (c == 0) {
				PyObject *msg = Sf_Op(&f, Sf_Op(&f, STR("net"), '+', Sf_Repr(&f, id)), '+', STR(" not found for respawn.\n"));
				Sf_Run(&f, "Scrap", "ConsoleError", "(O)", msg);
			}
		}
	} else if (!f.failed && Sf_EqS(&f, c0, "V") > 0) {
		Ffa_Run(&f, "ServerSpectate", "(O)", Sf_Attr(&f, e, "Name"));
	} else if (!f.failed && Sf_EqS(&f, c0, "M") > 0) {
		PyObject *data = Sf_Ask(&f, "string", "split", "(Os)", Sf_Slice(&f, s, 1, INT_MAX), "\n"), *v[11];
		v[0] = Sf_Op(&f, STR("net"), '+', Sf_Repr(&f, id));
		for (int i = 0; i < 10; i++) {
			v[i + 1] = Sf_Item(&f, data, i);
			if (i == 1 || i == 9) {
				v[i + 1] = Sf_ToInt(&f, v[i + 1]);
			}
		}
		Ffa_Run(&f, "ClientModify", "(OOOOOOOOOOO)", v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]);
	}
	return Sf_None(&f);
}

/* ---- spectating and the team menus ---- */

/* Spectate(id=0, command=""): back to the game as a spectator, or back into the game */
static PyObject *Ffa_Spectate(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "command", NULL };
	PyObject *id = NULL, *command = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|OO", names, &id, &command)) {
		return NULL;
	}
	id = id ? id : Sf_Int(&f, 0);
	command = command ? command : STR("");
	static const char *const mods[] = { "OutMap", "RacerScorer", "Menu" };
	for (int i = 0; i < 3; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_Run(&f, "Menu", "BackToGame", "(OO)", id, command);
	PyObject *cam = ENT("NetCam");
	int on = Sf_True(&f, cam);
	if (on > 0) {
		on = Sf_EqI(&f, Sf_Attr(&f, cam, "FreeCam"), 0);
	}
	if (on < 0) {
		return Sf_None(&f);
	}
	int client = Sf_True(&f, Sf_Ask(&f, "SNet", "IsClient", "()"));
	if (client > 0) {
		Ffa_Send(&f, Sf_Int(&f, 0), STR(on ? "V" : "R"));
	} else if (client == 0 && on) {
		Ffa_Run(&f, "ServerSpectate", "(O)", Sf_Attr(&f, cam, "MainTarget"));
	} else if (client == 0) {
		Sf_SetAttrI(&f, cam, "FreeCam", 0);
		Ffa_Run(&f, "ServerRemoveSpectatorState", "(O)", Sf_Attr(&f, cam, "MainTarget"));
		Ffa_Run(&f, "DelayedRestartOnPress", "(O)", id);
	}
	return Sf_None(&f);
}

/* JoinRed(id, control), JoinGreen(id, control): into the team (team id 0 or 2), rejoining when it changes */
static PyObject *Ffa_Join(PyObject *self, PyObject *args, long team) {
	PyObject *id, *control;
	SF_ARGS("OO", &id, &control);
	PyObject *cam = ENT("NetCam");
	int c = Sf_EqI(&f, Sf_Attr(&f, cam, "FreeCam"), 1);
	if (c == 0) {
		c = Sf_Compare(&f, GET("PlayerTeamID"), "!=", Sf_Int(&f, team));
	}
	if (c > 0) {
		Sf_Run(&f, "Scrap", "Set", "(sl)", "PlayerTeamID", team);
		Ffa_Run(&f, "ReJoinGame", "(OO)", id, control);
	}
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Menu", "BackToGame", "(OO)", id, control);
	return Sf_None(&f);
}

static PyObject *Ffa_JoinRed(PyObject *self, PyObject *args) {
	return Ffa_Join(self, args, 0);
}

static PyObject *Ffa_JoinGreen(PyObject *self, PyObject *args) {
	return Ffa_Join(self, args, 2);
}

/* ReJoinGame(id, control): spectate, send the new ship, join again after a 10 s count down */
static PyObject *Ffa_ReJoinGame(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	SF_ARGS("OO", &id, &control);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "");
	PyObject *cam = ENT("NetCam");
	int c = Sf_True(&f, cam);
	if (c > 0) {
		c = Sf_EqI(&f, Sf_Attr(&f, cam, "FreeCam"), 0);
	}
	if (c > 0) {
		Ffa_Run(&f, "Spectate", "(OO)", id, control);
	}
	Ffa_Run(&f, "ModiTry", "()");
	Ffa_Run(&f, "RejoinMsg", "(iOO)", 10, id, control);
	return Sf_None(&f);
}

/* RejoinMsg(timer, id, control): the count down, one message a second, then JoinGame */
static PyObject *Ffa_RejoinMsg(PyObject *self, PyObject *args) {
	PyObject *timer, *id, *control;
	SF_ARGS("OOO", &timer, &id, &control);
	timer = Sf_OpI(&f, timer, '-', 1);
	int c = Sf_Compare(&f, timer, ">", Sf_Float(&f, 0.5));
	if (c > 0) {
		Sf_Run(&f, "SScorer", "SetMsgText", "(OO)", Sf_Op(&f, LANG("Net_Msg_Join_Time"), '%', timer), LATER(1.0));
		Sf_Later(&f, Sf_Float(&f, 1.0), G("RejoinMsg"), Sf_Build(&f, "(OOO)", timer, id, control), NULL);
	} else if (c == 0) {
		Ffa_Run(&f, "JoinGame", "(OO)", id, control);
	}
	return Sf_None(&f);
}

/* JoinGame(id, control): back into the game */
static PyObject *Ffa_JoinGame(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	SF_ARGS("OO", &id, &control);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "Menu.Init");
	int client = Sf_True(&f, Sf_Ask(&f, "SNet", "IsClient", "()"));
	if (client > 0) {
		Ffa_Send(&f, Sf_Int(&f, 0), STR("R"));
	} else if (client == 0) {
		Ffa_Run(&f, "Spectate", "(OO)", id, control);
	}
	return Sf_None(&f);
}

/* TeamMenu(id, control): the in-game menu of the team modes: red, green and (when playing) spectate */
static PyObject *Ffa_TeamMenu(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	SF_ARGS("OO", &id, &control);
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Menu", "StartNewMenu", "(O)", id);
	PyObject *red = LANG("Net_Menu_Join_Red"), *green = LANG("Net_Menu_Join_Green");
	PyObject *opts = Sf_Build(&f, "[[Os][Os]]", red, "Ffa.JoinRed", green, "Ffa.JoinGreen");
	ENT("NetCam");
	if (Sf_EqI(&f, G("SpectatorMode"), 0) > 0) {
		Sf_Method(&f, opts, "append", "(O)", Sf_Build(&f, "[Os]", LANG("Net_Menu_Spectate"), "Ffa.Spectate"));
	}
	PyObject *fn = Sf_Mod(&f, "Menu", "VerticalMenu");
	PyObject *a = Sf_Build(&f, "(OOOs)", id, LANG("Net_Menu_Game"), opts, "PCMenu.MultiPlayerMenu");
	PyObject *kw = Sf_Build(&f, "{sO}", "YStart", Sf_Mod(&f, "Menu", "OptionMenuYStart"));
	Sf_CallKw(&f, fn, Script_Ref(a), kw);
	Sf_Run(&f, "Menu", "DrawBackOptionMenu", "(O)", id);
	return Sf_None(&f);
}

/* GetGameTeamDescriptor(): the title of a team game: tied, red or green ahead */
static PyObject *Ffa_GetGameTeamDescriptor(PyObject *self, PyObject *args) {
	NOARGS();
	Sf_Import(&f, "string");
	PyObject *v = NULL;
	if (Sf_Compare(&f, G("TotalRed"), "==", G("TotalGreen")) > 0) {
		v = Sf_Op(&f, LANG("Net_Title_Teams_Tied"), '%', G("TotalGreen"));
	} else if (!f.failed && Sf_Compare(&f, G("TotalRed"), ">=", G("TotalGreen")) > 0) {
		v = Sf_Op(&f, LANG("Net_Title_Red_Wins"), '%', Sf_Build(&f, "(OO)", G("TotalRed"), G("TotalGreen")));
	} else if (!f.failed) {
		v = Sf_Op(&f, LANG("Net_Title_Green_Wins"), '%', Sf_Build(&f, "(OO)", G("TotalGreen"), G("TotalRed")));
	}
	return Sf_Return(&f, v);
}

/* CameraSpectator(): the camera of this machine flies free from the first start point */
static PyObject *Ffa_CameraSpectator(PyObject *self, PyObject *args) {
	NOARGS();
	static const char *const mods[] = { "OutMap", "RacerScorer", "PCMenu" };
	for (int i = 0; i < 3; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetMod(&f, "PCMenu", "MultiplayerMenu", LIB("MultiplayerMenuSpectator"));
	Sf_SetGlobal(&f, "Ffa", "SpectatorMode", Sf_Int(&f, 1));
	PyObject *cam = ENT("NetCam");
	Sf_SetAttrI(&f, cam, "FreeCam", 1);
	Sf_SetAttr(&f, cam, "camPos", Sf_Item(&f, Sf_Item(&f, Sf_Mod(&f, "OutMap", "StartPos"), 0), 0));
	PyObject *ang = Sf_Item(&f, Sf_Item(&f, Sf_Mod(&f, "OutMap", "StartPos"), 0), 1);
	Sf_SetAttr(&f, cam, "camAng", ang ? Sf_Build(&f, "(iOi)", 0, ang) : NULL);
	Sf_SetMod(&f, "RacerScorer", "ScorerTopMessage", Sf_Method(&f, G("ModLib"), "GetGameDescriptor", "()"));
	Sf_SetMod(&f, "RacerScorer", "ScorerBotMessage", STR("Spectator mode"));
	Sf_Run(&f, "SScorer", "SetMPFunc", "(is)", 0, "Ffa.ShowScorer");
	Ffa_Run(&f, "ShowScorer", "(iO)", 0, G("isScorerVisible"));
	Ffa_Run(&f, "ActualizeGameScorer", "()");
	return Sf_None(&f);
}

/* AbortSpectateMode(): the camera follows the ship again */
static PyObject *Ffa_AbortSpectateMode(PyObject *self, PyObject *args) {
	NOARGS();
	Sf_Import(&f, "PCMenu");
	Sf_SetAttrI(&f, ENT("NetCam"), "FreeCam", 0);
	Sf_SetMod(&f, "PCMenu", "MultiplayerMenu", LIB("MultiplayerMenuOnGame"));
	Sf_SetGlobal(&f, "Ffa", "SpectatorMode", Sf_Int(&f, 0));
	return Sf_None(&f);
}

/* ServerSpectate(EntityName): the server makes a player a spectator */
static PyObject *Ffa_ServerSpectate(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "RacerScorer");
	if (Sf_Compare(&f, Sf_GetItem(&f, G("Scorer"), name), "!=", SPECT()) > 0) {
		PyObject *cam = ENT("NetCam");
		int c = Sf_True(&f, cam);
		if (c > 0) {
			c = Sf_Compare(&f, Sf_Attr(&f, cam, "MainTarget"), "==", name);
		}
		if (c > 0) {
			Ffa_Run(&f, "CameraSpectator", "()");
		}
		PyObject *e = Sf_Entity(&f, name);
		Sf_SetAttrI(&f, e, "Active", 0);
		Sf_SetItem(&f, G("Scorer"), Sf_Attr(&f, e, "Name"), SPECT());
		Ffa_Run(&f, "ServerSendPlayerStatus", "(O)", Sf_Attr(&f, e, "Name"));
		Ffa_Run(&f, "ActualizeGameScorer", "()");
		Ffa_Lib(&f, "ServerHidePlayer", "(O)", Sf_Attr(&f, e, "Name"));
		Ffa_Run(&f, "PlayerSpectator", "(O)", Sf_Attr(&f, e, "Name"));
	}
	return Sf_None(&f);
}

/* InitClient(): the client's callbacks, the HUD, the mode */
static PyObject *Ffa_InitClient(PyObject *self, PyObject *args) {
	static const char *const cbs[][2] = { { "ClientSetPlayer", "Ffa.SetupUserShip" }, { "ClientSetConfig", "Ffa.ClientSetConfig" },
		{ "ClientCreate", "Ffa.ClientCreate" }, { "ClientAfterCreate", "Ffa.ClientAfterCreate" }, { "OnUsrString", "Ffa.OnUsrClient" } };
	NOARGS();
	Ffa_Run(&f, "InitFfa", "()");
	for (int i = 0; i < 5; i++) {
		Sf_Run(&f, "Scrap", "Set", "(ss)", cbs[i][0], cbs[i][1]);
	}
	Ffa_Run(&f, "StdInitFfa", "()");
	Ffa_Lib(&f, "InitClientMod", "()");
	return Sf_None(&f);
}

/* StdInitFfa(): the ship controls, the "press fire" scorer, the scorer back from a menu */
static PyObject *Ffa_StdInitFfa(PyObject *self, PyObject *args) {
	NOARGS();
	Sf_Import(&f, "RacerScorer");
	Sf_Run(&f, "SInput", "SetActionSet", "(s)", "Racer");
	Ffa_Run(&f, "ShowPresFireMessage", "(iO)", 0, Sf_Method(&f, G("ModLib"), "GetGameDescriptor", "()"));
	Sf_SetMod(&f, "RacerScorer", "BackMenuFunc", G("BackMenuFunc"));
	return Sf_None(&f);
}

/* ---- the scorer ---- */

/* GetScorerInfo(id=0): the rows of the scorer (score, name with the flag mark, text colour, back colour, entity),
 * by team and score; the player of id sorts above an equal score */
static PyObject *Ffa_GetScorerInfo(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", NULL };
	PyObject *id = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &id)) {
		return NULL;
	}
	id = id ? id : Sf_Int(&f, 0);
	Sf_Import(&f, "RacerScorer");
	PyObject *red = Sf_Keep(&f, PyList_New(0)), *green = Sf_Keep(&f, PyList_New(0));
	PyObject *spect = Sf_Keep(&f, PyList_New(0)), *killer = Sf_Keep(&f, PyList_New(0));
	PyObject *userscore = Sf_Float(&f, 0.1);
	PyObject *keys = Sf_Method(&f, G("Scorer"), "keys", "()");
	int mark = f.n;
	for (int i = 0; !f.failed; i++) {
		PyObject *key = Sf_Next(&f, keys, i);
		if (!key) {
			break;
		}
		int mf = Sf_Compare(&f, G("MasterFlag"), "==", key);
		PyObject *flag = mf < 0 ? NULL : STR(mf ? "*" : " ");
		PyObject *ent = Sf_Entity(&f, key);
		int isuser = Sf_Compare(&f, Sf_Attr(&f, ent, "UsrControl"), "==", id);
		PyObject *color = isuser < 0 ? NULL : G(isuser ? "USRCOLOR" : "STDCOLOR");
		int sp = Sf_Compare(&f, Sf_GetItem(&f, G("Scorer"), key), "==", SPECT());
		PyObject *row = NULL, *list = NULL;
		if (sp > 0) {
			PyObject *score = Sf_GetItem(&f, G("Scorer"), key);
			row = Sf_Build(&f, "(OOO(iiii)O)", score, Sf_Op(&f, flag, '+', Sf_Attr(&f, ent, "Descriptor")), color, 0, 0, 0, 0, key);
			list = spect;
		} else if (sp == 0) {
			PyObject *typo = Sf_Attr(&f, ent, "TgType");
			PyObject *score = Sf_ToFloat(&f, Sf_GetItem(&f, G("Scorer"), key));
			if (isuser) {
				score = Sf_Op(&f, score, '+', Sf_Float(&f, 0.1));
				userscore = score;
			}
			int r = Sf_EqS(&f, typo, "red"), g = r == 0 ? Sf_EqS(&f, typo, "green") : 0;
			PyObject *desc = r >= 0 && g >= 0 ? Sf_Op(&f, flag, '+', Sf_Attr(&f, ent, "Descriptor")) : NULL;
			if (r > 0 || g > 0) {
				row = Sf_Build(&f, "(OOOOO)", score, desc, color, G(r > 0 ? "REDCOLOR" : "GREENCOLOR"), key);
				list = r > 0 ? red : green;
			} else if (r == 0 && g == 0) {
				row = Sf_Build(&f, "(OOO(iiii)O)", score, desc, color, 0, 0, 0, 0, key);
				list = killer;
			}
		}
		if (row) {
			Sf_Check(&f, PyList_Append(list, row) == 0);
		}
		Sf_ReleaseKeep(&f, mark, &userscore, 1);
	}
	PyObject *lists[4] = { killer, red, green, spect };
	for (int i = 0; i < 4; i++) {
		int mark = f.n;
		Sf_Method(&f, lists[i], "sort", "()");
		Sf_Method(&f, lists[i], "reverse", "()");
		Sf_Release(&f, mark);
	}
	int gw = Sf_Compare(&f, G("TotalGreen"), ">", G("TotalRed"));
	PyObject *info = NULL;
	if (gw >= 0) {
		info = Sf_Op(&f, Sf_Op(&f, Sf_Op(&f, gw ? green : red, '+', gw ? red : green), '+', killer), '+', spect);
	}
	PyObject *tginfo = Sf_Keep(&f, PyList_New(0));
	int n = Sf_Len(&f, info);
	for (int i = 0; i < n && !f.failed; i++) {
		int mark = f.n;
		PyObject *item = Sf_Item(&f, info, i), *score = Sf_Item(&f, item, 0);
		if (Sf_Compare(&f, score, "==", userscore) > 0) {
			score = Sf_Op(&f, userscore, '-', Sf_Float(&f, 0.1));
		}
		PyObject *iscore = Sf_ToInt(&f, score);
		PyObject *row = Sf_Build(&f, "(OOOOO)", iscore, Sf_Item(&f, item, 1), Sf_Item(&f, item, 2), Sf_Item(&f, item, 3),
			Sf_Item(&f, item, 4));
		if (row) {
			Sf_Check(&f, PyList_Append(tginfo, row) == 0);
		}
		Sf_Release(&f, mark);
	}
	return Sf_Return(&f, tginfo);
}

/* OnPositionChange(newpos, prevpos, value, prevValue, newwinner, winner): the label of a change of the lead or of the
 * place (free for all only) */
static PyObject *Ffa_OnPositionChange(PyObject *self, PyObject *args) {
	PyObject *newpos, *prevpos, *value, *prevvalue, *newwinner, *winner, *txpos = NULL;
	SF_ARGS("OOOOOO", &newpos, &prevpos, &value, &prevvalue, &newwinner, &winner);
	if (Sf_EqI(&f, newpos, 1) > 0) {
		txpos = STR("1st");
	} else if (!f.failed && Sf_EqI(&f, newpos, 2) > 0) {
		txpos = STR("2nd");
	} else if (!f.failed) {
		txpos = Sf_Op(&f, Sf_Repr(&f, newpos), '+', STR("th"));
	}
	int lead = Sf_Compare(&f, newwinner, "!=", winner);
	if (lead == 0) {
		lead = Sf_Compare(&f, value, "!=", prevvalue);
		if (lead > 0) {
			lead = Sf_Compare(&f, winner, "!=", Sf_Int(&f, -1));
		}
	}
	if (lead > 0) {
		if (Sf_True(&f, LIB("TeamGame")) == 0) {
			const char *key = NULL;
			if (Sf_EqI(&f, newwinner, -1) > 0) {
				key = "Net_Label_Lost_Lead";
			} else if (!f.failed && Sf_EqI(&f, newwinner, 0) > 0) {
				key = "Net_Label_Tied_Lead";
			} else if (!f.failed) {
				key = "Net_Label_Tied_Leading";
			}
			if (key) {
				Sf_Run(&f, "SScorer", "SetLabelText", "(OO)", LANG(key), Sf_OpI(&f, Sf_Time(&f), '+', 5));
			}
		}
		return Sf_None(&f);
	}
	int place = lead < 0 ? -1 : Sf_Compare(&f, value, "!=", prevvalue);
	if (place == 0) {
		place = Sf_Compare(&f, newpos, "!=", prevpos);
	}
	if (place > 0 && Sf_True(&f, LIB("TeamGame")) == 0) {
		const char *key = NULL;
		if (Sf_Compare(&f, newpos, "<", prevpos) > 0) {
			key = "Net_Label_Up_to";
		} else if (!f.failed && Sf_Compare(&f, newpos, ">", prevpos) > 0) {
			key = "Net_Label_Down_to";
		} else if (!f.failed) {
			key = "Net_Label_Still_to";
		}
		if (key) {
			Sf_Run(&f, "SScorer", "SetLabelText", "(OO)", Sf_Op(&f, LANG(key), '%', txpos), Sf_OpI(&f, Sf_Time(&f), '+', 5));
		}
	}
	return Sf_None(&f);
}

/* isTied(value): 1 when value leads GameScorer alone, 0 when tied at the top, -1 when not at the top */
static PyObject *Ffa_isTied(PyObject *self, PyObject *args) {
	PyObject *value;
	SF_ARGS("O", &value);
	long pos = -1, ctr = 0;
	PyObject *list = G("GameScorer");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *key = Sf_Next(&f, list, i);
		if (!key) {
			break;
		}
		if (Sf_Compare(&f, Sf_Item(&f, key, 0), "==", value) > 0) {
			ctr++;
			if (pos == -1) {
				pos = i + 1;
			}
		}
		Sf_Release(&f, mark);
	}
	return Sf_Return(&f, Sf_Int(&f, pos != 1 ? -1 : ctr == 1 ? 1 : 0));
}

/* CheckGameStart(): the game counts once two active players (one per team in a team game) are in */
static PyObject *Ffa_CheckGameStart(PyObject *self, PyObject *args) {
	NOARGS();
	int c = Sf_True(&f, G("GameStart"));
	if (c != 0) {
		return Sf_Return(&f, Sf_Int(&f, 1));
	}
	int n = Sf_Len(&f, Sf_Method(&f, G("Scorer"), "keys", "()"));
	if (n >= 0 && n <= 1) {
		return Sf_Return(&f, Sf_Int(&f, 0));
	}
	int team = Sf_True(&f, LIB("TeamGame"));
	long red = 0, green = 0;
	PyObject *keys = team < 0 ? NULL : Sf_Method(&f, G("Scorer"), "keys", "()");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *name = Sf_Next(&f, keys, i);
		if (!name) {
			break;
		}
		PyObject *ply = Sf_Entity(&f, name);
		int on = Sf_True(&f, ply);
		if (on > 0) {
			on = Sf_True(&f, Sf_Attr(&f, ply, "Active"));
		}
		if (on > 0 && team) {
			int r = Sf_EqS(&f, Sf_Attr(&f, ply, "TgType"), "red");
			if (r > 0) {
				red++;
			} else if (r == 0) {
				green++;
			}
		} else if (on > 0) {
			red++;
		}
		Sf_Release(&f, mark);
	}
	if (!f.failed && (team ? green > 0 && red > 0 : red > 1)) {
		Sf_SetGlobal(&f, "Ffa", "GameStart", Sf_Int(&f, 1));
	}
	return Sf_Return(&f, G("GameStart"));
}

/* UsrPos(info): the place of the player's row (USRCOLOR), 1 when none */
static PyObject *Ffa_UsrPos(PyObject *self, PyObject *args) {
	PyObject *info;
	SF_ARGS("O", &info);
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *key = Sf_Next(&f, info, i);
		if (!key) {
			break;
		}
		if (Sf_Compare(&f, Sf_Item(&f, key, 2), "==", G("USRCOLOR")) > 0) {
			return Sf_Return(&f, Sf_Int(&f, i + 1));
		}
		Sf_Release(&f, mark);
	}
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* ActualizeGameScorer(NowShowScorer=1): GameScorer again, the place labels, the scorer shown or its values set */
static PyObject *Ffa_ActualizeGameScorer(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "NowShowScorer", NULL };
	PyObject *now = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &now)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	Sf_SetGlobal(&f, "Ffa", "GameScorer", Sf_Own(&f, "Ffa", "GetScorerInfo", "(i)", 0));
	int n = Sf_Len(&f, G("GameScorer"));
	if (n > 0) {
		PyObject *yp = Sf_Own(&f, "Ffa", "UsrPos", "(O)", G("GameScorer"));
		PyObject *value = Sf_Item(&f, Sf_GetItem(&f, G("GameScorer"), Sf_OpI(&f, yp, '-', 1)), 0);
		PyObject *nw = Sf_Own(&f, "Ffa", "isTied", "(O)", value);
		int c = Sf_Compare(&f, yp, "!=", G("YourPosition"));
		if (c == 0) {
			c = Sf_Compare(&f, value, "!=", G("YourScore"));
		}
		if (c == 0) {
			c = Sf_Compare(&f, nw, "!=", G("Winner"));
		}
		if (c > 0) {
			Ffa_Run(&f, "OnPositionChange", "(OOOOOO)", yp, G("YourPosition"), value, G("YourScore"), nw, G("Winner"));
			Sf_SetGlobal(&f, "Ffa", "YourPosition", yp);
			Sf_SetGlobal(&f, "Ffa", "YourScore", value);
			Sf_SetGlobal(&f, "Ffa", "Winner", nw);
		}
	}
	if (Sf_True(&f, LIB("TeamGame")) > 0) {
		Sf_SetMod(&f, "RacerScorer", "ScorerTopMessage", Sf_Method(&f, G("ModLib"), "GetGameDescriptor", "()"));
	}
	int vis = Sf_True(&f, G("isScorerVisible"));
	if (vis > 0) {
		if ((now ? Sf_True(&f, now) : 1) > 0) {
			Ffa_Run(&f, "ShowScorer", "(ii)", 0, 1);
		}
	} else if (vis == 0) {
		Ffa_Run(&f, "SetScorerValues", "(i)", 0);
	}
	return Sf_None(&f);
}

/* ShowScorer(id, doit): the scores on screen or the plain HUD (not while a menu is up) */
static PyObject *Ffa_ShowScorer(PyObject *self, PyObject *args) {
	PyObject *id, *doit;
	SF_ARGS("OO", &id, &doit);
	Sf_SetGlobal(&f, "Ffa", "isScorerVisible", doit);
	Sf_Import(&f, "RacerScorer");
	if (Sf_EqS(&f, Sf_Ask(&f, "SInput", "GetActionSet", "()"), "Menu") != 0) {
		return Sf_None(&f);
	}
	Ffa_Run(&f, "ActualizeGameScorer", "(i)", 0);
	int d = Sf_True(&f, doit);
	if (d > 0) {
		Sf_Run(&f, "RacerScorer", "ShowScorer", "(OO)", id, G("GameScorer"));
	} else if (d == 0) {
		Sf_Run(&f, "RacerScorer", "Create", "(O)", id);
	}
	return Sf_None(&f);
}

/* BackMenuFunc(id): back from a menu, the scorer as it was */
static PyObject *Ffa_BackMenuFunc(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Ffa_Run(&f, "ShowScorer", "(OO)", id, G("isScorerVisible"));
	return Sf_None(&f);
}

/* ShowHelp(id): the help of the mode */
static PyObject *Ffa_ShowHelp(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Ffa_Lib(&f, "ShowHelp", "(O)", id);
	return Sf_None(&f);
}

/* SetupGameRules(s): the client gets the frag limit */
static PyObject *Ffa_SetupGameRules(PyObject *self, PyObject *args) {
	PyObject *s;
	SF_ARGS("O", &s);
	Sf_SetGlobal(&f, "Ffa", "ScorerFraglimit", Sf_ToInt(&f, s));
	Ffa_Run(&f, "ActualizeGameScorer", "()");
	return Sf_None(&f);
}

/* SendGameRules(clientid): the server sends the frag limit */
static PyObject *Ffa_SendGameRules(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Ffa_Send(&f, id, Sf_Op(&f, STR("G"), '+', Sf_Repr(&f, G("ScorerFraglimit"))));
	return Sf_None(&f);
}

/* SetScorerValues(id): the numbers of the mission scorer: the limit, the team totals or the best and own score */
static PyObject *Ffa_SetScorerValues(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "RacerScorer");
	PyObject *cam = ENT("NetCam");
	int c = Sf_True(&f, cam);
	if (c > 0) {
		c = Sf_EqI(&f, Sf_Attr(&f, cam, "FreeCam"), 1);
	}
	if (c != 0) {
		return Sf_None(&f);
	}
	Ffa_Set(&f, id, STR("MissionLimit"), "Text", Sf_Repr(&f, G("ScorerFraglimit")));
	int team = Sf_True(&f, LIB("TeamGame"));
	if (team > 0) {
		int r = Sf_Compare(&f, G("TotalRed"), ">", G("TotalGreen"));
		if (r < 0) {
			return Sf_None(&f);
		}
		const char *rc[2] = { r ? "MissionSLimit" : "MissionYou", r ? "MissionBest" : "MissionBad" };
		const char *gc[2] = { r ? "MissionYou" : "MissionSLimit", r ? "MissionBad" : "MissionBest" };
		Ffa_Set(&f, id, STR(rc[0]), "Green", Sf_Int(&f, 0));
		Ffa_Set(&f, id, STR(rc[0]), "Red", Sf_Int(&f, 255));
		Ffa_Set(&f, id, STR(rc[1]), "Text", Sf_Repr(&f, G("TotalRed")));
		Ffa_Set(&f, id, STR(gc[0]), "Green", Sf_Int(&f, 255));
		Ffa_Set(&f, id, STR(gc[0]), "Red", Sf_Int(&f, 0));
		Ffa_Set(&f, id, STR(gc[1]), "Text", Sf_Repr(&f, G("TotalGreen")));
		return Sf_None(&f);
	}
	if (team < 0) {
		return Sf_None(&f);
	}
	int n = Sf_Len(&f, G("GameScorer"));
	if (n == 0) {
		Ffa_Set(&f, id, STR("MissionBest"), "Text", STR("-"));
		Ffa_Set(&f, id, STR("MissionBad"), "Text", STR("-"));
		Ffa_Set(&f, id, STR("MissionYou"), "Blue", Sf_Int(&f, 0));
		Ffa_Set(&f, id, STR("MissionYou"), "Red", Sf_Int(&f, 0));
		return Sf_None(&f);
	}
	if (n < 0) {
		return Sf_None(&f);
	}
	int best = Sf_Compare(&f, G("YourScore"), "==", Sf_Item(&f, Sf_Item(&f, G("GameScorer"), 0), 0));
	PyObject *pts;
	if (best > 0) {
		Ffa_Set(&f, id, STR("MissionBest"), "Text", Sf_Repr(&f, G("YourScore")));
		int indx = Sf_Len(&f, G("GameScorer")) > 1 ? 1 : 0;
		if (Sf_Compare(&f, SPECT(), "==", Sf_Item(&f, Sf_Item(&f, G("GameScorer"), indx), 0)) > 0) {
			pts = Sf_Int(&f, 0);
		} else {
			pts = Sf_Item(&f, Sf_Item(&f, G("GameScorer"), indx), 0);
		}
		Ffa_Set(&f, id, STR("MissionBad"), "Text", Sf_Repr(&f, pts));
		Ffa_Set(&f, id, STR("MissionYou"), "Y", Sf_Int(&f, 180));
		Ffa_Set(&f, id, STR("MissionYou"), "Blue", Sf_Int(&f, 255));
		Ffa_Set(&f, id, STR("MissionYou"), "Red", Sf_Int(&f, 0));
	} else if (best == 0) {
		/* pts is computed and not used, as in the script */
		if (Sf_Compare(&f, SPECT(), "==", Sf_Item(&f, Sf_Item(&f, G("GameScorer"), 0), 0)) == 0) {
			Sf_Item(&f, Sf_Item(&f, G("GameScorer"), 0), 0);
		}
		Ffa_Set(&f, id, STR("MissionBest"), "Text", Sf_Repr(&f, Sf_Item(&f, Sf_Item(&f, G("GameScorer"), 0), 0)));
		Ffa_Set(&f, id, STR("MissionBad"), "Text", Sf_Repr(&f, G("YourScore")));
		Ffa_Set(&f, id, STR("MissionYou"), "Y", Sf_Int(&f, 200));
		Ffa_Set(&f, id, STR("MissionYou"), "Blue", Sf_Int(&f, 0));
		Ffa_Set(&f, id, STR("MissionYou"), "Red", Sf_Int(&f, 255));
	}
	return Sf_None(&f);
}

/* FfaScorer(id): Menu.MissionScorerCall: the mission scorer of the HUD with the values (not for a spectator) */
static PyObject *Ffa_FfaScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "RacerScorer");
	PyObject *cam = ENT("NetCam");
	int c = Sf_True(&f, cam);
	if (c > 0) {
		c = Sf_Compare(&f, Sf_Attr(&f, cam, "FreeCam"), "!=", Sf_Int(&f, 1));
	}
	if (c > 0) {
		Sf_Run(&f, "RacerScorer", "MissionScorer", "(O)", id);
		if (Sf_True(&f, LIB("TeamGame")) > 0) {
			Sf_Run(&f, "RacerScorer", "MissionScorerTeam", "(O)", id);
		}
		Ffa_Run(&f, "SetScorerValues", "(O)", id);
	}
	return Sf_None(&f);
}

#define KW(fn) (PyCFunction)(void (*)(void))fn, 3

PyMethodDef g_FfaMethods[] = {
	{ "InitFfa", Ffa_InitFfa, 1, NULL },
	{ "SetupUserShip", Ffa_SetupUserShip, 1, NULL },
	{ "RestartOnPress", Ffa_RestartOnPress, 1, NULL },
	{ "DelayedRestartOnPress", Ffa_DelayedRestartOnPress, 1, NULL },
	{ "OnDeath", Ffa_OnDeath, 1, NULL },
	{ "ShowPresFireMessage", Ffa_ShowPresFireMessage, 1, NULL },
	{ "DelayedRestartOnTime", Ffa_DelayedRestartOnTime, 1, NULL },
	{ "EndOfGame", Ffa_EndOfGame, 1, NULL },
	{ "ServerSendPlayerStatus", Ffa_ServerSendPlayerStatus, 1, NULL },
	{ "ServerSendTeamData", Ffa_ServerSendTeamData, 1, NULL },
	{ "ServerEndOfGame", Ffa_ServerEndOfGame, 1, NULL },
	{ "OnDeathServer", Ffa_OnDeathServer, 1, NULL },
	{ "OnDeathClient", Ffa_OnDeathClient, 1, NULL },
	{ "Scorer2Str", Ffa_Scorer2Str, 1, NULL },
	{ "Str2Scorer", Ffa_Str2Scorer, 1, NULL },
	{ "ClientJoin", Ffa_ClientJoin, 1, NULL },
	{ "BuildCfgString", Ffa_BuildCfgString, 1, NULL },
	{ "ModiTry", Ffa_ModiTry, 1, NULL },
	{ "ModifyOnReyoin", Ffa_ModifyOnReyoin, 1, NULL },
	{ "ClientModify", Ffa_ClientModify, 1, NULL },
	{ "TryToModify", Ffa_TryToModify, 1, NULL },
	{ "PlayerMsgShow", Ffa_PlayerMsgShow, 1, NULL },
	{ "PlayerDisconected", Ffa_PlayerDisconected, 1, NULL },
	{ "PlayerSpectator", Ffa_PlayerSpectator, 1, NULL },
	{ "PlayerJoinGame", Ffa_PlayerJoinGame, 1, NULL },
	{ "ClientExit", Ffa_ClientExit, 1, NULL },
	{ "ReSpawnClient", Ffa_ReSpawnClient, 1, NULL },
	{ "ServerRemoveSpectatorState", Ffa_ServerRemoveSpectatorState, 1, NULL },
	{ "SetSpawnPos", Ffa_SetSpawnPos, 1, NULL },
	{ "InitServer", Ffa_InitServer, 1, NULL },
	{ "ClientSetConfig", Ffa_ClientSetConfig, 1, NULL },
	{ "ClientAfterCreate", Ffa_ClientAfterCreate, 1, NULL },
	{ "ClientCreate", Ffa_ClientCreate, 1, NULL },
	{ "OnUsrTest", Ffa_OnUsrTest, 1, NULL },
	{ "SvrShowMsg", Ffa_SvrShowMsg, 1, NULL },
	{ "OnUsrClient", Ffa_OnUsrClient, 1, NULL },
	{ "OnUsrServer", Ffa_OnUsrServer, 1, NULL },
	{ "Spectate", KW(Ffa_Spectate), NULL },
	{ "JoinRed", Ffa_JoinRed, 1, NULL },
	{ "JoinGreen", Ffa_JoinGreen, 1, NULL },
	{ "ReJoinGame", Ffa_ReJoinGame, 1, NULL },
	{ "RejoinMsg", Ffa_RejoinMsg, 1, NULL },
	{ "JoinGame", Ffa_JoinGame, 1, NULL },
	{ "TeamMenu", Ffa_TeamMenu, 1, NULL },
	{ "GetGameTeamDescriptor", Ffa_GetGameTeamDescriptor, 1, NULL },
	{ "CameraSpectator", Ffa_CameraSpectator, 1, NULL },
	{ "AbortSpectateMode", Ffa_AbortSpectateMode, 1, NULL },
	{ "ServerSpectate", Ffa_ServerSpectate, 1, NULL },
	{ "InitClient", Ffa_InitClient, 1, NULL },
	{ "StdInitFfa", Ffa_StdInitFfa, 1, NULL },
	{ "GetScorerInfo", KW(Ffa_GetScorerInfo), NULL },
	{ "OnPositionChange", Ffa_OnPositionChange, 1, NULL },
	{ "isTied", Ffa_isTied, 1, NULL },
	{ "CheckGameStart", Ffa_CheckGameStart, 1, NULL },
	{ "UsrPos", Ffa_UsrPos, 1, NULL },
	{ "ActualizeGameScorer", KW(Ffa_ActualizeGameScorer), NULL },
	{ "ShowScorer", Ffa_ShowScorer, 1, NULL },
	{ "BackMenuFunc", Ffa_BackMenuFunc, 1, NULL },
	{ "ShowHelp", Ffa_ShowHelp, 1, NULL },
	{ "SetupGameRules", Ffa_SetupGameRules, 1, NULL },
	{ "SendGameRules", Ffa_SendGameRules, 1, NULL },
	{ "SetScorerValues", Ffa_SetScorerValues, 1, NULL },
	{ "FfaScorer", Ffa_FfaScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
