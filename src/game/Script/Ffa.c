/*
 * The Ffa module in C (scripts/net/ffa.py, issue #2): the base of the online game modes. init.py preloads it for a
 * network game together with the mode of ServerType (NDeathMatch, NTeamDM, ...) and sets Ffa.ModLib to that mode;
 * OutMap.InitOutMap calls InitServer or InitClient. It keeps the scores, sends them from the server to the clients as
 * user strings ("S=", "S+", "S-", "T", "R", "E", "G", "!"), handles the deaths, joins, ship changes and exits,
 * spectating and the team menu, and fills the scorer of the HUD (RacerScorer). The mode supplies TeamGame, the menu
 * entries, DeathFunc, CheckEndOfGame, GetGameDescriptor, ShowHelp, ServerHidePlayer and its init functions.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values and call the engine in C; the texts
 * of the network are made and read by the native repr and literal reader (byte for byte as before). The state is the
 * struct ffa, mirrored into the module dict (OBJLIB 5): the N* modes, the HUD and the tests read and write Scorer,
 * isScorerVisible, TotalRed, TotalGreen, SpectatorMode, GameFinished, the colours, MasterFlag, GameScorer,
 * YourPosition, YourScore, Winner, ScorerFraglimit and GameStart there, so the C reads each where the script reads
 * the global and writes it through at once (Scorer is changed in place, as the script does). The engine functions
 * the cases record go through their seams (OBJLIB 7, scrap/scriptmodes.h); entity handles are the entities found again
 * by name, as the handle finds them.
 *
 * Direct calls (stage 3): the functions call each other through the module globals and the other ports (Net,
 * Vehicles, OutMap, RacerScorer, Menu) through their refs (PORT_OWN, PORT_CALL; the seam takes the call by name when
 * a mode, a mission or a case has put something else there, OBJLIB 5). ModLib, the module of the mode, is read at each
 * call (its variables, OBJLIB 5; its functions through their entries, PORT_VALUE); the functions Net holds in its
 * variables ClientJoin, ClientCreate and ClientAfterCreate (the mode's or Net's defaults, OBJLIB 5) are bound by the
 * engine to the entries of the functions they hold (Port_Callback); the functions given to the scheduler take their
 * entries (Port_Scheduled; those of SInput and SNet too, stage 7), the ones set into other
 * modules are values of their variables (OBJLIB 5).
 *
 * State on values (stage 6, batch 6f): no function works on Python objects. ModLib (a module object), the functions
 * read from it, from the globals and from SInput and SNet for the scheduler, and the menu entries of the mode set
 * into Menu and PCMenu (lists that keep their identity) are held by reference (NV_OBJ) and go through the helpers of
 * scrap/scriptmodes.h (Mode_GObjAttr, Mode_CallArgs, Mode_ScheduleV, Mode_ModToV, Mode_ModSetGlobal; OBJLIB 7).
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/input.h"
#include "scrap/net.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

static struct {
	nativeVal_t scorer;		/* the score of each ship name (a dict) */
	nativeVal_t isScorerVisible, totalRed, totalGreen, spectatorMode, gameFinished, masterFlag, gameScorer;
	nativeVal_t yourPosition, yourScore, winner, scorerFraglimit, gameStart, usrColor, stdColor, redColor, greenColor;
} ffa;

/* the global of the module into its slot of ffa (as the script reads it), a write through */
#define FR(field, name) Mode_Read("Ffa", name, &ffa.field)
#define FW(field, name, v) Mode_Put("Ffa", name, &ffa.field, v)
/* Scorer[key] = v in place */
#define SCORER_PUT(key, v) Mode_DictPut("Ffa", "Scorer", &ffa.scorer, key, v)
/* the function a variable of another module holds (Net.ClientJoin, ClientCreate, ClientAfterCreate: the mode sets
 * them), by name (OBJLIB 5) */
#define VARCALL(mod, name, out, ...) Native_CallMod(mod, name, out, __VA_ARGS__)

/* ---- helpers ---- */

/* ModLib.name of the mode's module as a value (a function as NV_OBJ; OBJLIB 5: the module is a variable, read at
 * every use) */
static byte Ffa_LibGet(const char *name, nativeVal_t *out) {
	return Mode_GObjAttr("Ffa", "ModLib", name, out);
}

/* truth(ModLib.TeamGame): 1, 0, -1 with the error */
static int Ffa_Team(void) {
	nativeVal_t v;
	if (!Ffa_LibGet("TeamGame", &v)) {
		return -1;
	}
	int t = Native_True(&v);
	Native_Free(&v);
	return t;
}

/* ModLib.name(...) on the n values of args through the entry of the function there, the result into out (NULL:
 * dropped) */
static byte Ffa_Lib(const char *name, nativeVal_t *out, const nativeVal_t *args, int n) {
	nativeVal_t fn;
	byte ok = Ffa_LibGet(name, &fn) && Mode_CallArgs(&fn, out, args, n);
	Native_Free(&fn);
	return ok;
}

/* module.var = ModLib.name as the object it is (OBJLIB 5: the menu entries of the mode keep their identity) */
static byte Ffa_LibToMod(const char *module, const char *var, const char *name) {
	nativeVal_t lib;
	byte ok = Mode_G("Ffa", "ModLib", &lib) && Mode_ModToV(module, var, &lib, name);
	Native_Free(&lib);
	return ok;
}

/* module.var = the global name of Ffa as the object it is (a function of the module, OBJLIB 5) */
static byte Ffa_GlobalToMod(const char *module, const char *var, const char *name) {
	return Mode_ModSetGlobal(module, var, "Ffa", name);
}

/* RacerScorer.SPECTATOR_POINTS (OBJLIB 5) */
static byte Ffa_Spect(nativeVal_t *out) {
	return Mode_ModVar("RacerScorer", "SPECTATOR_POINTS", out);
}

/* Scrap.Get(name) */
static byte Ffa_Get(modeVals_t *t, const char *name, nativeVal_t **out) {
	*out = Mode_V(t);
	return Native_GVarGet(name, *out);
}

/* the camera of the network game, Scrap.GetEntity("NetCam") (NULL for None) */
static cEntity *Ffa_Cam(void) {
	return World_FindEntity("NetCam");
}

/* x == y, x != y */
static byte Ffa_Eq(const nativeVal_t *x, const nativeVal_t *y) {
	return !Native_Compare(x, y);
}

/* ent.Pos = pos; ent.Ang = (0, ang, 0) of the handle of name (e NULL: None) */
static byte Ffa_Place(cEntity *e, const char *name, const nativeVal_t *pos, const nativeVal_t *ang) {
	nativeVal_t a;
	Native_SetEmpty(&a, NV_TUPLE);
	Native_SetInt(Native_Append(&a), 0);
	Native_Copy(Native_Append(&a), ang);
	Native_SetInt(Native_Append(&a), 0);
	byte ok = Mode_Set(e, name, "Pos", pos) && Mode_Set(e ? World_FindEntity(name) : NULL, name, "Ang", &a);
	Native_Free(&a);
	return ok;
}

/* the client id of an entity name, int(name[3:]) */
static byte Ffa_ClientId(modeVals_t *t, const nativeVal_t *name, nativeVal_t *id) {
	nativeVal_t *num = Mode_V(t);
	return Native_Slice(name, 3, INT_MAX, num) && Native_ToInt(num, id);
}

/* Scorer[key] into *out (the global read again) */
static byte Ffa_Score(const nativeVal_t *key, nativeVal_t *out) {
	return FR(scorer, "Scorer") && Native_Subscript(&ffa.scorer, key, out);
}

/* ---- game flow ---- */

/* InitFfa(): the mission scorer and the multiplayer menu entry of the mode */
static byte Ffa_InitFfaC(void) {
	return Mode_Import("Menu") && Ffa_GlobalToMod("Menu", "MissionScorerCall", "FfaScorer") &&
		Ffa_LibToMod("Menu", "MultiplayerMenu", "MultiplayerMenuOnGame") && FW(spectatorMode, "SpectatorMode", &(nativeVal_t){ .type = NV_INT });
}

/* SetupUserShip(entityname): the camera NetCam on the player's ship, the ship at a spawn point */
static byte Ffa_SetupUserShipC(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cname = Mode_S(&t, "NetCam"), *camName = Mode_V(&t), *sp = Mode_V(&t), *pos = Mode_V(&t), *ang = Mode_V(&t);
	const nativeVal_t *zero = Mode_I(&t, 0);
	nativeVal_t *z = Mode_V(&t);
	cEntity *cam, *us;
	Native_SetFloat(z, 0.0);
	if (!Mode_Import("Vehicles") || !Mode_Import("OutMap") || !Mode_Create(cname, z, z, z, "FCam", &cam) ||
		!Mode_Get(cam, "NetCam", "Name", camName) || !PORT_CALL(Vehicles, SetActiveCam, NULL, "vvv", name, camName, zero) ||
		!PORT_CALL(OutMap, GetSpawnPos, sp, "") || !Mode_Ent(name, &us) || !Native_Subscript(sp, Mode_I(&t, 0), pos) ||
		!Native_Subscript(sp, Mode_I(&t, 1), ang)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Ffa_Place(us, Native_Text(name), pos, ang));
}

/* RestartOnPress(id): fire pressed after a death, the restart a second later */
static byte Ffa_RestartOnPressC(const nativeVal_t *id) {
	return Mode_Bind(id, "Racer", "Fire", "") && Mode_DelSchedS("DelayedRestartOnTime") &&
		Mode_Later(1.0, "Ffa", "DelayedRestartOnPress", id, 1, "DelayedRestartOnPress");
}

/* the game over or the player of this machine a spectator: GameFinished or YourScore == SPECTATOR_POINTS */
static int Ffa_Out(modeVals_t *t) {
	nativeVal_t *sp = Mode_V(t);
	if (!FR(gameFinished, "GameFinished")) {
		return -1;
	}
	if (Native_True(&ffa.gameFinished)) {
		return 1;
	}
	if (!FR(yourScore, "YourScore") || !Ffa_Spect(sp)) {
		return -1;
	}
	return Ffa_Eq(&ffa.yourScore, sp);
}

/* RacerScorer.ScorerTopMessage = ModLib.GetGameDescriptor() */
static byte Ffa_TopDescriptor(void) {
	nativeVal_t d;
	Native_SetNone(&d);
	byte ok = Ffa_Lib("GetGameDescriptor", &d, NULL, 0) && Mode_SetModVar("RacerScorer", "ScorerTopMessage", &d);
	Native_Free(&d);
	return ok;
}

/* DelayedRestartOnPress(id): the dead player's ship comes back (the client asks the server) */
static byte Ffa_DelayedRestartOnPressC(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *target = Mode_V(&t), *act = Mode_V(&t), *tn = Mode_V(&t), *cid = Mode_V(&t);
	if (!Mode_Import("OutMap") || !Mode_Import("RacerScorer")) {
		return Mode_Done(&t, 0);
	}
	int out = Ffa_Out(&t);
	if (out) {
		return Mode_Done(&t, out > 0);
	}
	cEntity *cam = Ffa_Cam(), *tgt;
	if (!cam) {
		return Mode_Done(&t, 1);
	}
	if (!Native_EntVar(cam, "NetCam", "MainTarget", target) || !Mode_Ent(target, &tgt) ||
		!Mode_Get(tgt, Native_Text(target), "Active", act)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(act)) {
		return Mode_Done(&t, 1);
	}
	const char *name = Native_Text(target);
	if (!Ffa_TopDescriptor() || !Mode_SetModVar("RacerScorer", "ScorerBotMessage", Mode_S(&t, "")) ||
		!PORT_OWN(Ffa, ShowScorer, NULL, "vi", id, 0L)) {
		return Mode_Done(&t, 0);
	}
	int client = Native_NetFlag(0);
	if (client < 0) {
		return Mode_Done(&t, 0);
	}
	byte ok;
	if (client) {
		ok = Mode_NameGet(name, "Name", tn) && Ffa_ClientId(&t, tn, cid) && Mode_Send(cid, Mode_S(&t, "R"));
	} else {
		ok = PORT_OWN(Ffa, AbortSpectateMode, NULL, "") && Mode_NameGet(name, "Name", tn) && PORT_OWN(Ffa, SetSpawnPos, NULL, "v", tn);
	}
	return Mode_Done(&t, ok && Mode_MPFunc(0, "Ffa.ShowScorer"));
}

/* OnDeath(LooserName, WinnerName): the death or kill message of the player of this machine */
static byte Ffa_OnDeathC(const nativeVal_t *looser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *target = Mode_V(&t), *desc = Mode_V(&t), *text = Mode_V(&t);
	cEntity *bad;
	if (!Ffa_Cam()) {
		return Mode_Done(&t, 1);
	}
	if (!Native_EntVar(Ffa_Cam(), "NetCam", "MainTarget", target)) {
		return Mode_Done(&t, 0);
	}
	if (Ffa_Eq(looser, target)) {
		if (!Mode_Ent(winner, &bad)) {
			return Mode_Done(&t, 0);
		}
		if (bad) {
			return Mode_Done(&t, Native_EntVar(bad, Native_Text(winner), "Descriptor", desc) &&
				Mode_LangFmt("Net_Title_Death_Kill", desc, text) && PORT_OWN(Ffa, ShowPresFireMessage, NULL, "iv", 0L, text));
		}
		Mode_Lang("Net_Title_Death_Suicide", text);
		return Mode_Done(&t, PORT_OWN(Ffa, ShowPresFireMessage, NULL, "iv", 0L, text));
	}
	Native_Free(target);
	if (!Native_EntVar(Ffa_Cam(), "NetCam", "MainTarget", target)) {
		return Mode_Done(&t, 0);
	}
	if (Ffa_Eq(winner, target)) {
		return Mode_Done(&t, Mode_Ent(looser, &bad) && Mode_Get(bad, Native_Text(looser), "Descriptor", desc) &&
			Mode_LangFmt("Net_Msg_You_Kill", desc, text) && Mode_MsgText(text, Mode_Time() + 5.0));
	}
	return Mode_Done(&t, 1);
}

/* ShowPresFireMessage(id, msg): the scorer with msg and "press fire"; fire restarts, after 10 s the restart is
 * automatic */
static byte Ffa_ShowPresFireMessageC(const nativeVal_t *id, const nativeVal_t *msg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *bot = Mode_V(&t), *tm = Mode_V(&t), *args = Mode_V(&t), *name = Mode_V(&t);
	if (!FR(gameFinished, "GameFinished")) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(&ffa.gameFinished)) {
		return Mode_Done(&t, 1);
	}
	Mode_Lang("Net_Bott_Press_Fire", bot);
	if (!Mode_Import("RacerScorer") || !Mode_SetModVar("RacerScorer", "ScorerTopMessage", msg) ||
		!Mode_SetModVar("RacerScorer", "ScorerBotMessage", bot) || !PORT_OWN(Ffa, ShowScorer, NULL, "vi", id, 1L)) {
		return Mode_Done(&t, 0);
	}
	double timy = Mode_Time();
	/* AddScheduledFunc(timy + 1.0, SInput.Bind, (id, "Racer", "Fire", "Ffa.RestartOnPress")): the function as the
	 * module holds it (the scheduler takes its entry, Port_Scheduled) */
	nativeVal_t *bind = Mode_V(&t);
	byte ok = Mode_ModGet("SInput", "Bind", bind);
	Native_SetFloat(tm, timy + 1.0);
	Native_SetEmpty(args, NV_LIST);
	Native_Copy(Native_Append(args), id);
	Native_SetStr(Native_Append(args), "Racer");
	Native_SetStr(Native_Append(args), "Fire");
	Native_SetStr(Native_Append(args), "Ffa.RestartOnPress");
	if (!ok || !Mode_ScheduleV(tm, bind, args->items, 4, NULL) || !Mode_MPFunc(0, "")) {
		return Mode_Done(&t, 0);
	}
	if (timy > 10.0) {
		nativeVal_t *fn = Mode_V(&t);
		ok = Mode_G("Ffa", "DelayedRestartOnTime", fn);
		Native_SetFloat(tm, timy + 10.0);
		Native_SetStr(name, "DelayedRestartOnTime");
		ok = ok && Mode_ScheduleV(tm, fn, id, 1, name);
	}
	return Mode_Done(&t, ok);
}

/* DelayedRestartOnTime(id): the automatic restart */
static byte Ffa_DelayedRestartOnTimeC(const nativeVal_t *id) {
	return Mode_DelSchedS("DelayedRestartOnPress") && Mode_Bind(id, "Racer", "Fire", "") &&
		PORT_OWN(Ffa, DelayedRestartOnPress, NULL, "v", id);
}

/* EndOfGame(): the results on the scorer, the screen fades to noise */
static byte Ffa_EndOfGameC(void) {
	static const int clear[4] = { 0, 0, 0, 0 }, dark[4] = { 0, 0, 0, 128 };
	modeVals_t t = { .n = 0 };
	nativeVal_t *top = Mode_V(&t), *bot = Mode_V(&t), *one = Mode_V(&t);
	if (!Mode_Import("RacerScorer") || !FW(gameFinished, "GameFinished", Mode_I(&t, 1)) || !Mode_Bind(Mode_I(&t, 0), "Racer", "Fire", "") ||
		!Mode_MPFunc(0, "")) {
		return Mode_Done(&t, 0);
	}
	int team = Ffa_Team();
	if (team < 0 || (team ? !PORT_OWN(Ffa, GetGameTeamDescriptor, top, "") : (Mode_Lang("Net_Title_Game_Results", top), 0)) ||
		!Mode_SetModVar("RacerScorer", "ScorerTopMessage", top)) {
		return Mode_Done(&t, 0);
	}
	Mode_Lang("Net_Bott_Wait_Next_Match", bot);
	if (!Mode_SetModVar("RacerScorer", "ScorerBotMessage", bot) || !PORT_OWN(Ffa, ShowScorer, NULL, "ii", 0L, 1L)) {
		return Mode_Done(&t, 0);
	}
	if (Mode_ActionSetIs("Menu")) {
		return Mode_Done(&t, 1);
	}
	Native_SetFloat(one, 1.0);
	return Mode_Done(&t, Mode_GVarSet("NoiseFactorBegin", Mode_I(&t, 0)) && Mode_GVarSet("NoiseFactor", Mode_I(&t, 32)) &&
		Mode_GVarSet("NoiseTime", Mode_I(&t, 0)) && Mode_GVarSet("NoiseEndTime", one) && Mode_Fade(0, clear, dark, 1.0));
}

/* ServerSendPlayerStatus(WinnerName): the score of one player to all clients */
static byte Ffa_ServerSendPlayerStatusC(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *s = Mode_V(&t), *score = Mode_V(&t), *rep = Mode_V(&t);
	if (!Mode_Cat(&t, s, 3, Mode_S(&t, "S+"), name, Mode_S(&t, ":")) || !Ffa_Score(name, score)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(score, rep);
	return Mode_Done(&t, Mode_Cat(&t, s, 2, s, rep) && Mode_Send(Mode_I(&t, -1), s) && PORT_OWN(Ffa, ServerSendTeamData, NULL, ""));
}

/* ServerSendTeamData(): the team totals to all clients */
static byte Ffa_ServerSendTeamDataC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *s = Mode_V(&t), *red = Mode_V(&t), *green = Mode_V(&t);
	int team = Ffa_Team();
	if (team <= 0) {
		return Mode_Done(&t, team == 0);
	}
	if (!FR(totalRed, "TotalRed")) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(&ffa.totalRed, red);
	if (!Mode_Cat(&t, s, 3, Mode_S(&t, "T"), red, Mode_S(&t, ",")) || !FR(totalGreen, "TotalGreen")) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(&ffa.totalGreen, green);
	return Mode_Done(&t, Mode_Cat(&t, s, 2, s, green) && Mode_Send(Mode_I(&t, -1), s));
}

/* ServerEndOfGame(): the frag limit is reached: the ships hide, the next map of ServerMapList loads 10 s later */
static byte Ffa_ServerEndOfGameC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *text = Mode_V(&t), *maps = Mode_V(&t), *next = Mode_V(&t), *path = Mode_V(&t), *tail = Mode_V(&t);
	nativeVal_t *at = Mode_V(&t), *at1 = Mode_V(&t), *len = Mode_V(&t), *m = Mode_V(&t), *tm = Mode_V(&t);
	if (!Mode_Import("Init") || !Mode_Import("Net") || !Mode_SendS(-1, "E")) {
		return Mode_Done(&t, 0);
	}
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e;) {
		if (Entity_IsType(e, "Car") && !Mode_SetI(e, e->name, "Active", 0)) {
			return Mode_Done(&t, 0);
		}
		if (!Mode_NextInSlot(&e)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!PORT_CALL(Net, PurgeServerMapList, NULL, "") || !Native_GVarGet("ServerMapList", text) || !Native_EvalText("Ffa", text, maps)) {
		return Mode_Done(&t, 0);
	}
	int empty = !Native_True(maps);
	if (!empty) {
		int n = Native_Size(maps);
		if (n < 0) {
			return Mode_Done(&t, 0);
		}
		empty = n == 0;
	}
	byte ok;
	if (empty) {
		ok = Mode_ModVar("Init", "Path", next);
	} else {
		ok = Mode_ModVar("Init", "Path", path) && Native_Slice(path, 7, INT_MAX, tail);
		int in = ok ? Native_Contains(maps, tail) : -1;
		ok = in >= 0;
		if (in > 0) {
			Native_Free(path);
			Native_Free(tail);
			ok = Mode_ModVar("Init", "Path", path) && Native_Slice(path, 7, INT_MAX, tail) && Mode_Index(maps, tail, at) &&
				Native_Arith('+', at, Mode_I(&t, 1), at1);
			int n = ok ? Native_Size(maps) : -1;
			ok = n >= 0;
			if (ok) {
				Native_SetInt(len, n);
				ok = Native_Arith('%', at1, len, at) && Native_Subscript(maps, at, m) && Mode_Cat(&t, next, 2, Mode_S(&t, "Levels/"), m);
			}
		} else if (in == 0) {
			ok = Native_Subscript(maps, Mode_I(&t, 0), m) && Mode_Cat(&t, next, 2, Mode_S(&t, "Levels/"), m);
		}
	}
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	/* AddScheduledFunc(Scrap.GetTime() + 10.0, SNet.ServerChangeLevel, (next,)) (the scheduler takes its
	 * entry, Port_Scheduled) */
	nativeVal_t *fn = Mode_V(&t);
	ok = Mode_ModGet("SNet", "ServerChangeLevel", fn);
	Native_SetFloat(tm, Mode_Time() + 10.0);
	ok = ok && Mode_ScheduleV(tm, fn, next, 1, NULL);
	return Mode_Done(&t, ok && PORT_OWN(Ffa, EndOfGame, NULL, ""));
}

/* ---- deaths ---- */

/* OnDeathServer(LooserName, WinnerName): the server counts the frag (a suicide or a team kill costs one), sends the
 * changed scores and checks the end of the game */
static byte Ffa_OnDeathServerC(const nativeVal_t *looser, const nativeVal_t *winner) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lt = Mode_V(&t), *wt = Mode_V(&t), *xprev = Mode_V(&t), *lprev = Mode_V(&t), *now = Mode_V(&t);
	nativeVal_t *end = Mode_V(&t);
	const nativeVal_t *xname, *add;
	if (!Mode_Import("Vehicles") || !Mode_Import("Init") || !PORT_CALL(Vehicles, OnDeath, NULL, "vv", looser, winner)) {
		return Mode_Done(&t, 0);
	}
	if (Native_Compare(winner, Mode_S(&t, ""))) {
		xname = winner;
		int same = Ffa_Team();
		if (same < 0) {
			return Mode_Done(&t, 0);
		}
		if (same) {
			if (!Mode_EntGet(looser, "TgType", lt) || !Mode_EntGet(winner, "TgType", wt)) {
				return Mode_Done(&t, 0);
			}
			same = Ffa_Eq(lt, wt);
		}
		add = Mode_I(&t, same ? -1 : 1);
	} else {
		xname = looser;
		add = Mode_I(&t, -1);
	}
	int has = Mode_HasKey("Ffa", "Scorer", 1, xname);
	if (has < 0 || (!has && !SCORER_PUT(xname, Mode_I(&t, 0)))) {
		return Mode_Done(&t, 0);
	}
	/* ModLib.DeathFunc(xname, LooserName, add): the three values side by side (views, not freed) */
	const nativeVal_t death[3] = { *xname, *looser, *add };
	if (!Ffa_Score(xname, xprev) || !Ffa_Score(looser, lprev) || !Ffa_Lib("DeathFunc", NULL, death, 3) ||
		!PORT_OWN(Ffa, ActualizeGameScorer, NULL, "") || !Ffa_Score(xname, now)) {
		return Mode_Done(&t, 0);
	}
	if (!Ffa_Eq(xprev, now) && !PORT_OWN(Ffa, ServerSendPlayerStatus, NULL, "v", xname)) {
		return Mode_Done(&t, 0);
	}
	if (!Ffa_Eq(xname, looser)) {
		Native_Free(now);
		if (!Ffa_Score(looser, now) || (!Ffa_Eq(lprev, now) && !PORT_OWN(Ffa, ServerSendPlayerStatus, NULL, "v", looser))) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Ffa_Lib("CheckEndOfGame", end, NULL, 0) || (!Native_True(end) && !PORT_OWN(Ffa, OnDeath, NULL, "vv", looser, winner))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Ffa_Lib("ServerHidePlayer", NULL, looser, 1));
}

/* OnDeathClient(LooserName, WinnerName) */
static byte Ffa_OnDeathClientC(const nativeVal_t *looser, const nativeVal_t *winner) {
	return Mode_Import("Vehicles") && PORT_CALL(Vehicles, OnClientDeath, NULL, "vv", looser, winner) &&
		PORT_OWN(Ffa, OnDeath, NULL, "vv", looser, winner);
}

/* ---- the scorer strings ---- */

/* Scorer2Str(): "<n>,<score>-..." of all players (net<n>) */
static byte Ffa_Scorer2StrC(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cad = Mode_S(&t, ""), *keys = Mode_V(&t);
	if (!Mode_Keys("Ffa", "Scorer", 1, keys)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(keys);
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *ky = Mode_V(&t), *num = Mode_V(&t), *score = Mode_V(&t), *rep = Mode_V(&t);
		if (!Native_SeqItem(keys, i, ky) || !Native_Slice(ky, 3, INT_MAX, num) || !Mode_Cat(&t, cad, 3, cad, num, Mode_S(&t, ",")) ||
			!Ffa_Score(ky, score)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(score, rep);
		if (!Mode_Cat(&t, cad, 3, cad, rep, Mode_S(&t, "-"))) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, n >= 0 && Native_Slice(cad, 0, -1, out));
}

/* Str2Scorer(cad): the scores from the text of Scorer2Str */
static byte Ffa_Str2ScorerC(const nativeVal_t *cad) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *empty = Mode_V(&t), *parts = Mode_V(&t);
	Native_SetEmpty(empty, NV_DICT);
	if (!Mode_Import("string") || !FW(scorer, "Scorer", empty) || !Native_Split(cad, "-", parts)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < parts->n; i++) {
		int mark = t.n;
		nativeVal_t *vals = Mode_V(&t), *v1 = Mode_V(&t), *v = Mode_V(&t), *v0 = Mode_V(&t), *key = Mode_V(&t);
		if (!Native_Split(&parts->items[i], ",", vals) || !Native_SeqItem(vals, 1, v1) || !Native_ToInt(v1, v) ||
			!Native_SeqItem(vals, 0, v0) || !Mode_Cat(&t, key, 2, Mode_S(&t, "net"), v0) || !SCORER_PUT(key, v)) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, 1);
}

/* ---- clients joining, changing ships, leaving ---- */

#define FFA_A11 "vvvvvvvvvvv"
#define FFA_ARGS(a) &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9], &a[10]

/* ClientJoin(entityname, ...): a new player on the server: its ship, its score 0 to all, the scores and the rules to
 * it */
static byte Ffa_ClientJoinC(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ply = Mode_V(&t), *pn = Mode_V(&t), *cid = Mode_V(&t), *str = Mode_V(&t), *s = Mode_V(&t);
	if (!Mode_Import("Net") || !VARCALL("Net", "ClientJoin", NULL, FFA_A11, FFA_ARGS(a)) ||
		!PORT_OWN(Ffa, ClientSetConfig, ply, FFA_A11, FFA_ARGS(a)) || !Mode_VGet(ply, "Name", pn) || !PORT_OWN(Ffa, SetSpawnPos, NULL, "v", pn) ||
		!SCORER_PUT(&a[0], Mode_I(&t, 0)) || !PORT_OWN(Ffa, ServerSendPlayerStatus, NULL, "v", &a[0]) || !Ffa_ClientId(&t, &a[0], cid) ||
		!PORT_OWN(Ffa, Scorer2Str, str, "") || !Mode_Cat(&t, s, 2, Mode_S(&t, "S="), str) || !Mode_Send(cid, s) ||
		!PORT_OWN(Ffa, ServerSendTeamData, NULL, "") || !PORT_OWN(Ffa, SendGameRules, NULL, "v", cid) || !Mode_VSet(ply, "Active", Mode_I(&t, 0)) ||
		!PORT_OWN(Ffa, ActualizeGameScorer, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	Native_Free(pn);
	if (!Mode_VGet(ply, "Name", pn) || !Ffa_Lib("ServerHidePlayer", NULL, pn, 1)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(pn);
	return Mode_Done(&t, Mode_VGet(ply, "Name", pn) && PORT_OWN(Ffa, PlayerJoinGame, NULL, "v", pn));
}

/* the ship of the player as the engine variables give it, in the order of the "M" message */
static const char *const ffa_cfg[10] = { "PlayerModel", "PlayerMaxLife", "PlayerName", "PilotModel", "Motor0Model",
	"Motor1Model", "Motor2Model", "Motor3Model", "WeaponBayList", "PlayerTeamID" };

/* BuildCfgString(): the player's ship as the "M" message sends it, one value per line */
static byte Ffa_BuildCfgStringC(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *s = Mode_V(&t);
	for (int i = 0; i < 10; i++) {
		nativeVal_t *v, *r = Mode_V(&t);
		if (!Ffa_Get(&t, ffa_cfg[i], &v)) {
			return Mode_Done(&t, 0);
		}
		if (i == 1 || i == 9) {
			Mode_Repr(v, r);
			v = r;
		}
		if (i ? !Mode_Cat(&t, s, 3, s, Mode_S(&t, "\n"), v) : (Native_Copy(s, v), 0)) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Copy(out, s);
	return Mode_Done(&t, 1);
}

/* the ten engine variables of the player's ship into v[0..9] */
static byte Ffa_Cfg(modeVals_t *t, nativeVal_t **v) {
	for (int i = 0; i < 10; i++) {
		if (!Ffa_Get(t, ffa_cfg[i], &v[i])) {
			return 0;
		}
	}
	return 1;
}

/* ModiTry(): the player's ship changed (the ship editor): the client asks the server, the server changes its own */
static byte Ffa_ModiTryC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cfg = Mode_V(&t), *s = Mode_V(&t), *ename = Mode_V(&t), *v[10];
	int c = Mode_IsClient();
	if (c > 0) {
		return Mode_Done(&t, PORT_OWN(Ffa, BuildCfgString, cfg, "") && Mode_Cat(&t, s, 2, Mode_S(&t, "M"), cfg) && Mode_Send(Mode_I(&t, 0), s));
	}
	if (c < 0) {
		return Mode_Done(&t, 0);
	}
	int srv = Mode_IsServer();
	if (srv <= 0) {
		return Mode_Done(&t, srv == 0);
	}
	return Mode_Done(&t, Mode_Get(Ffa_Cam(), "NetCam", "MainTarget", ename) && Ffa_Cfg(&t, v) &&
		PORT_OWN(Ffa, ClientModify, NULL, FFA_A11, ename, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]));
}

/* ModifyOnReyoin(entityname, ...): the ship of a rejoining player gets its new set up */
static byte Ffa_ModifyOnReyoinC(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	cEntity *ply;
	if (!Mode_Ent(&a[0], &ply) || !Mode_SetS(ply, Native_Text(&a[0]), "ActCtrl", "")) {
		return Mode_Done(&t, 0);
	}
	/* ply.SetModel(""): the method of the handle */
	cEntity *e = World_FindEntity(Native_Text(&a[0]));
	if (!e) {
		nativeVal_t x;
		return Mode_Done(&t, Native_EntVar(NULL, Native_Text(&a[0]), "SetModel", &x));
	}
	Entity_SetModel(e, "", 1.0f, 1.0f, 1.0f, 1);
	return Mode_Done(&t, PORT_OWN(Ffa, ClientSetConfig, NULL, FFA_A11, FFA_ARGS(a)) && Mode_NameSet(Native_Text(&a[0]), "Active", Mode_I(&t, 0)));
}

/* ClientModify(entityname, ...): a pending change of the ship replaced by this one */
static byte Ffa_ClientModifyC(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t);
	return Mode_Done(&t, Mode_Cat(&t, name, 2, &a[0], Mode_S(&t, "Modify")) && Mode_DelSched(name) &&
		PORT_OWN(Ffa, TryToModify, NULL, FFA_A11, FFA_ARGS(a)));
}

/* TryToModify(entityname, ...): the ship changes while it is inactive, else it tries again 0.75 s later */
static byte Ffa_TryToModifyC(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *act = Mode_V(&t), *cid = Mode_V(&t), *tm = Mode_V(&t), *name = Mode_V(&t);
	cEntity *ply;
	if (!Mode_Ent(&a[0], &ply)) {
		return Mode_Done(&t, 0);
	}
	int c = ply != NULL;
	if (c) {
		if (!Native_EntVar(ply, Native_Text(&a[0]), "Active", act)) {
			return Mode_Done(&t, 0);
		}
		c = !Native_True(act);
	}
	if (c) {
		return Mode_Done(&t, PORT_OWN(Ffa, ModifyOnReyoin, NULL, FFA_A11, FFA_ARGS(a)) && Ffa_ClientId(&t, &a[0], cid) && Mode_ModifyUsrData(cid));
	}
	/* AddScheduledFunc(Scrap.GetTime() + 0.75, TryToModify, args, entityname + "Modify"): the scheduler takes the entry
	 * of the global while it is the port's own function */
	nativeVal_t *fn = Mode_V(&t);
	byte ok = Mode_G("Ffa", "TryToModify", fn);
	Native_SetFloat(tm, Mode_Time() + 0.75);
	return Mode_Done(&t, ok && Mode_Cat(&t, name, 2, &a[0], Mode_S(&t, "Modify")) && Mode_ScheduleV(tm, fn, a, 11, name));
}

/* PlayerMsgShow(streang, entityname): streang % the player's name for 2.5 s (not in the first 2.5 s of a level) */
static byte Ffa_PlayerMsgShowC(const nativeVal_t *text, const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *desc = Mode_V(&t), *msg = Mode_V(&t);
	cEntity *e;
	if (Mode_Time() <= 2.5) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_Ent(name, &e)) {
		return Mode_Done(&t, 0);
	}
	if (!e) {
		return Mode_Done(&t, 1);
	}
	if (!Native_EntVar(e, Native_Text(name), "Descriptor", desc)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_Compare(desc, Mode_S(&t, ""))) {
		return Mode_Done(&t, 1);
	}
	Native_Free(desc);
	return Mode_Done(&t, Native_EntVar(e, Native_Text(name), "Descriptor", desc) && Native_Arith('%', text, desc, msg) &&
		Mode_MsgText(msg, Mode_Time() + 2.5));
}

/* PlayerJoinGame(entityname): the join message of the player's team */
static byte Ffa_PlayerJoinGameC(const nativeVal_t *name) {
	static const char *const types[][2] = { { "killer", "%s joins the game." }, { "red", "%s joins red team." },
		{ "green", "%s joins green team." } };
	modeVals_t t = { .n = 0 };
	cEntity *e;
	if (!Mode_Ent(name, &e)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; e && i < 3; i++) {
		nativeVal_t *tg = Mode_V(&t);
		if (!Native_EntVar(e, Native_Text(name), "TgType", tg)) {
			return Mode_Done(&t, 0);
		}
		if (Native_IsStr(tg, types[i][0])) {
			return Mode_Done(&t, PORT_OWN(Ffa, PlayerMsgShow, NULL, "sv", types[i][1], name));
		}
	}
	return Mode_Done(&t, 1);
}

/* ClientExit(entityname, ModelName, UsrName): a player leaves: its score goes */
static byte Ffa_ClientExitC(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *s = Mode_V(&t);
	if (!Ffa_Lib("ServerHidePlayer", NULL, name, 1)) {
		return Mode_Done(&t, 0);
	}
	int has = Mode_HasKey("Ffa", "Scorer", 1, name);
	if (has <= 0) {
		return Mode_Done(&t, has == 0);
	}
	return Mode_Done(&t, PORT_OWN(Ffa, PlayerDisconected, NULL, "v", name) && Mode_DictDel("Ffa", "Scorer", &ffa.scorer, name) &&
		Mode_Cat(&t, s, 2, Mode_S(&t, "S-"), name) && Mode_Send(Mode_I(&t, -1), s) && PORT_OWN(Ffa, ActualizeGameScorer, NULL, ""));
}

/* ---- spawning ---- */

/* ReSpawnClient(entityName): the ship of a client comes back */
static byte Ffa_ReSpawnClientC(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	return Mode_Done(&t, Mode_Import("Vehicles") && Mode_Import("RacerScorer") && PORT_OWN(Ffa, AbortSpectateMode, NULL, "") &&
		(!Mode_ActionSetIs("Racer") || PORT_CALL(RacerScorer, Create, NULL, "i", 0L)) && PORT_CALL(Vehicles, ReSpawnShip, NULL, "v", name) &&
		Mode_EntSet(name, "OnDeath", Mode_S(&t, "Ffa.OnDeathClient")));
}

static byte Ffa_ImportSpawn(void) {
	static const char *const mods[] = { "Vehicles", "Net", "OutMap", "RacerScorer" };
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	return 1;
}

/* ServerRemoveSpectatorState(entityName): a spectator plays again with 0 points */
static byte Ffa_ServerRemoveSpectatorStateC(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *score = Mode_V(&t), *sp = Mode_V(&t), *target = Mode_V(&t);
	if (!Ffa_ImportSpawn()) {
		return Mode_Done(&t, 0);
	}
	int c = Mode_HasKey("Ffa", "Scorer", 1, name);
	if (c > 0) {
		if (!Ffa_Score(name, score) || !Ffa_Spect(sp)) {
			return Mode_Done(&t, 0);
		}
		c = Ffa_Eq(score, sp);
	}
	if (c <= 0) {
		return Mode_Done(&t, c == 0);
	}
	if (!SCORER_PUT(name, Mode_I(&t, 0)) || !PORT_OWN(Ffa, ServerSendPlayerStatus, NULL, "v", name) || !PORT_OWN(Ffa, ActualizeGameScorer, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	cEntity *cam = Ffa_Cam();
	int j = !cam;
	if (!j) {
		if (!Native_EntVar(cam, "NetCam", "MainTarget", target)) {
			return Mode_Done(&t, 0);
		}
		j = !Ffa_Eq(target, name);
	}
	return Mode_Done(&t, !j || PORT_OWN(Ffa, PlayerJoinGame, NULL, "v", name));
}

/* SetSpawnPos(entityName): the server puts a ship back at a free spawn point (next to the one there when taken) */
static byte Ffa_SetSpawnPosC(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pos = Mode_V(&t), *res = Mode_V(&t), *vr1 = Mode_V(&t), *vr2 = Mode_V(&t), *dist = Mode_V(&t);
	nativeVal_t *p0 = Mode_V(&t), *p1 = Mode_V(&t), *epos = Mode_V(&t), *at = Mode_V(&t), *sp = Mode_V(&t);
	cEntity *entity, *e;
	if (!Ffa_ImportSpawn() || !PORT_OWN(Ffa, ServerRemoveSpectatorState, NULL, "v", name) || !PORT_CALL(Vehicles, ReSpawnShip, NULL, "v", name) ||
		!Mode_Ent(name, &entity) || !PORT_CALL(OutMap, GetFreeSpawn, pos, "") || !Mode_ModVar("OutMap", "SpawRes", res)) {
		return Mode_Done(&t, 0);
	}
	const char *en = Native_Text(name);
	byte live = entity != NULL;
	if (Native_True(res)) {
		Native_Free(res);
		if (!Mode_ModVar("OutMap", "SpawRes", res) || !Mode_Ent(res, &e) || !Mode_Get(e, Native_Text(res), "VRadius", vr1) ||
			!(live ? Mode_NameGet(en, "VRadius", vr2) : Mode_Get(NULL, en, "VRadius", vr2)) || !Native_Arith('+', vr1, vr2, dist) ||
			!Native_Arith('+', dist, Mode_I(&t, 10), vr1) || !Native_Arith('/', vr1, Mode_I(&t, 2), dist) ||
			!Native_EntVar(e, Native_Text(res), "Pos", epos)) {
			return Mode_Done(&t, 0);
		}
		/* SVec.Add(e.Pos, (dist, dist, dist)) */
		nativeVal_t *d3 = Mode_V(&t);
		Native_SetEmpty(d3, NV_TUPLE);
		for (int k = 0; k < 3; k++) {
			Native_Copy(Native_Append(d3), dist);
		}
		if (!Mode_VecAdd(epos, d3, at)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_Subscript(pos, Mode_I(&t, 1), p1)) {
			return Mode_Done(&t, 0);
		}
		Native_SetEmpty(sp, NV_TUPLE);
		Native_Copy(Native_Append(sp), at);
		Native_Copy(Native_Append(sp), p1);
		Native_Free(pos);
		Native_Copy(pos, sp);
		Native_Free(p1);
	}
	if (!Native_Subscript(pos, Mode_I(&t, 0), p0) || !Native_Subscript(pos, Mode_I(&t, 1), p1) ||
		!Ffa_Place(live ? World_FindEntity(en) : NULL, en, p0, p1)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, live ? Mode_NameSet(en, "OnDeath", Mode_S(&t, "Ffa.OnDeathServer")) : Mode_SetS(NULL, en, "OnDeath", ""));
}

/* ---- server and client start ---- */

/* InitServer(dedicated): the server's callbacks, the server player's own ship unless dedicated, the mode */
static byte Ffa_InitServerC(const nativeVal_t *dedicated) {
	static const char *const cbs[][2] = { { "OnClientJoin", "Ffa.ClientJoin" }, { "OnClientModify", "Ffa.ModifyOnReyoin" },
		{ "OnClientExit", "Ffa.ClientExit" }, { "OnUsrString", "Ffa.OnUsrServer" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *bot = Mode_V(&t), *z = Mode_V(&t), *pname = Mode_V(&t), *v[10], *pn = Mode_V(&t), *player = Mode_V(&t);
	char nb[128];
	cEntity *ply;
	if (!PORT_OWN(Ffa, InitFfa, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 4; i++) {
		if (!Mode_GVarSetS(cbs[i][0], cbs[i][1])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Native_True(dedicated)) {
		Net_GetBotName(nb);
		Native_SetStr(bot, nb);
		Native_SetFloat(z, 0.0);
		if (!Mode_Create(bot, z, z, z, "Car", &ply) || !Mode_Get(ply, NULL, "Name", pname) || !Ffa_Cfg(&t, v) ||
			!PORT_OWN(Ffa, ClientJoin, NULL, FFA_A11, pname, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9])) {
			return Mode_Done(&t, 0);
		}
		const char *n = Native_Text(pname);
		if (!Mode_NameGet(n, "Name", pn) || !PORT_OWN(Ffa, SetupUserShip, NULL, "v", pn) || !Native_GVarGet("PlayerName", player) ||
			!Mode_NameSet(n, "Descriptor", player) || !Mode_NameSet(n, "Active", Mode_I(&t, 0)) || !PORT_OWN(Ffa, StdInitFfa, NULL, "")) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Ffa_Lib("InitServerMod", NULL, NULL, 0));
}

/* ClientSetConfig(entityname, ...): the ship of a player set up (the server's own ship when ForceServerVehicle), its
 * team; a client respawns it or names the death callback; the ship's handle back (OBJLIB 7: as Vehicles gave it) */
static byte Ffa_ClientSetConfigC(const nativeVal_t *a, nativeVal_t *v) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *force, *g[10], *act = Mode_V(&t), *target = Mode_V(&t);
	if (!Mode_Import("Vehicles") || !Ffa_Get(&t, "ForceServerVehicle", &force)) {
		return Mode_Done(&t, 0);
	}
	byte ok;
	if (Native_True(force)) {
		ok = Ffa_Cfg(&t, g) && PORT_CALL(Vehicles, SetProperties, v, "vvvvvvvvvv", &a[0], g[0], g[1], &a[4], g[4], g[5], g[6], g[7], g[8], &a[3]);
	} else {
		ok = PORT_CALL(Vehicles, SetProperties, v, "vvvvvvvvvv", &a[0], &a[1], &a[2], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9], &a[3]);
	}
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	int team = Ffa_Team();
	if (team < 0 || !Mode_VSet(v, "TgType", Mode_S(&t, team ? (Ffa_Eq(&a[10], Mode_I(&t, 0)) ? "red" : "green") : "killer"))) {
		return Mode_Done(&t, 0);
	}
	int client = Native_NetFlag(0);
	if (client < 0) {
		return Mode_Done(&t, 0);
	}
	if (client) {
		if (!Mode_VGet(v, "Active", act)) {
			return Mode_Done(&t, 0);
		}
		if (Native_True(act)) {
			ok = PORT_OWN(Ffa, ReSpawnClient, NULL, "v", &a[0]);
		} else {
			ok = Mode_Get(Ffa_Cam(), "NetCam", "MainTarget", target) &&
				(Ffa_Eq(target, &a[0]) || Mode_EntSet(&a[0], "OnDeath", Mode_S(&t, "Ffa.OnDeathClient")));
		}
	}
	return Mode_Done(&t, ok && PORT_OWN(Ffa, ActualizeGameScorer, NULL, ""));
}

/* ---- the user strings ---- */

/* SvrShowMsg(Msg): a message on the server and all clients */
static byte Ffa_SvrShowMsgC(const nativeVal_t *msg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *s = Mode_V(&t);
	return Mode_Done(&t, Mode_MsgText(msg, Mode_Time() + 5.0) && Mode_Cat(&t, s, 2, Mode_S(&t, "!"), msg) && Mode_Send(Mode_I(&t, -1), s));
}

/* the "S+name:points" message of OnUsrClient */
static byte Ffa_ScorePlus(modeVals_t *t, const nativeVal_t *s) {
	nativeVal_t *rest = Mode_V(t), *lst = Mode_V(t), *l0 = Mode_V(t), *l1 = Mode_V(t), *points = Mode_V(t), *target = Mode_V(t);
	nativeVal_t *score = Mode_V(t), *sp = Mode_V(t);
	if (!Native_Slice(s, 2, INT_MAX, rest) || !Native_Split(rest, ":", lst) || !Native_Subscript(lst, Mode_I(t, 1), l1) ||
		!Native_ToInt(l1, points) || !Native_Subscript(lst, Mode_I(t, 0), l0) || !Mode_Get(Ffa_Cam(), "NetCam", "MainTarget", target)) {
		return 0;
	}
	int mainusr = Ffa_Eq(l0, target);
	if (!mainusr) {
		int has = Mode_HasKey("Ffa", "Scorer", 1, l0);
		if (has < 0) {
			return 0;
		}
		if (!has) {
			if (!Mode_Later(1.0, "Ffa", "PlayerJoinGame", l0, 1, NULL)) {
				return 0;
			}
		} else {
			if (!Ffa_Score(l0, score) || !Ffa_Spect(sp)) {
				return 0;
			}
			int j = Ffa_Eq(score, sp);
			if (j) {
				Native_Free(sp);
				if (!Ffa_Spect(sp)) {
					return 0;
				}
				j = !Ffa_Eq(points, sp);
			}
			if (j && !PORT_OWN(Ffa, PlayerJoinGame, NULL, "v", l0)) {
				return 0;
			}
		}
	}
	Native_Free(sp);
	if (!SCORER_PUT(l0, points) || !Ffa_Spect(sp)) {
		return 0;
	}
	if (Ffa_Eq(points, sp)) {
		return mainusr ? PORT_OWN(Ffa, CameraSpectator, NULL, "") : PORT_OWN(Ffa, PlayerSpectator, NULL, "v", l0);
	}
	return 1;
}

/* OnUsrClient(id, s): a message of the server on a client: a text, the scores, the team totals, the respawn, the end,
 * the rules */
static byte Ffa_OnUsrClientC(const nativeVal_t *s) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *c0 = Mode_V(&t), *c1 = Mode_V(&t), *rest = Mode_V(&t), *tot = Mode_V(&t), *two = Mode_V(&t);
	Mode_V(&t);
	nativeVal_t *target = Mode_V(&t);
	if (!Mode_Import("string") || !Mode_Import("RacerScorer") || !Mode_Import("Net") || !Native_Subscript(s, Mode_I(&t, 0), c0)) {
		return Mode_Done(&t, 0);
	}
	if (Native_IsStr(c0, "!") && (!Native_Slice(s, 1, INT_MAX, rest) || !Mode_MsgText(rest, Mode_Time() + 5.0))) {
		return Mode_Done(&t, 0);
	}
	if (Native_IsStr(c0, "S")) {
		if (!Native_Subscript(s, Mode_I(&t, 1), c1)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(rest);
		byte ok = 1;
		if (Native_IsStr(c1, "=")) {
			ok = Native_Slice(s, 2, INT_MAX, rest) && PORT_OWN(Ffa, Str2Scorer, NULL, "v", rest);
		} else if (Native_IsStr(c1, "+")) {
			ok = Ffa_ScorePlus(&t, s);
		} else if (Native_IsStr(c1, "-")) {
			ok = Native_Slice(s, 2, INT_MAX, rest) && PORT_OWN(Ffa, PlayerDisconected, NULL, "v", rest) &&
				Mode_DictDel("Ffa", "Scorer", &ffa.scorer, rest);
		}
		return Mode_Done(&t, ok && PORT_OWN(Ffa, ActualizeGameScorer, NULL, ""));
	}
	if (Native_IsStr(c0, "T")) {
		Native_Free(rest);
		return Mode_Done(&t, Native_Slice(s, 1, INT_MAX, rest) && Native_EvalText("Ffa", rest, tot) && Mode_Unpack(tot, 2, two) &&
			FW(totalRed, "TotalRed", &two[0]) && FW(totalGreen, "TotalGreen", &two[1]) && PORT_OWN(Ffa, ActualizeGameScorer, NULL, ""));
	}
	if (Native_IsStr(c0, "R")) {
		if (!FR(gameFinished, "GameFinished")) {
			return Mode_Done(&t, 0);
		}
		return Mode_Done(&t, Native_True(&ffa.gameFinished) ||
			(Mode_Get(Ffa_Cam(), "NetCam", "MainTarget", target) && PORT_OWN(Ffa, ReSpawnClient, NULL, "v", target)));
	}
	if (Native_IsStr(c0, "E")) {
		return Mode_Done(&t, PORT_OWN(Ffa, EndOfGame, NULL, ""));
	}
	if (Native_IsStr(c0, "G")) {
		Native_Free(rest);
		return Mode_Done(&t, Native_Slice(s, 1, INT_MAX, rest) && PORT_OWN(Ffa, SetupGameRules, NULL, "v", rest));
	}
	return Mode_Done(&t, 1);
}

/* OnUsrServer(id, s): a request of client id on the server: respawn, spectate, a new ship */
static byte Ffa_OnUsrServerC(const nativeVal_t *id, const nativeVal_t *s) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rep = Mode_V(&t), *name = Mode_V(&t), *c0 = Mode_V(&t), *en = Mode_V(&t), *msg = Mode_V(&t), *rest = Mode_V(&t);
	nativeVal_t *data = Mode_V(&t), *v[11];
	cEntity *e;
	Mode_Repr(id, rep);
	if (!Mode_Import("RacerScorer") || !Mode_Import("string") || !Mode_Cat(&t, name, 2, Mode_S(&t, "net"), rep) || !Mode_Ent(name, &e) ||
		!Native_Subscript(s, Mode_I(&t, 0), c0)) {
		return Mode_Done(&t, 0);
	}
	if (Native_IsStr(c0, "R")) {
		if (!FR(gameFinished, "GameFinished")) {
			return Mode_Done(&t, 0);
		}
		if (Native_True(&ffa.gameFinished)) {
			return Mode_Done(&t, 1);
		}
		if (e) {
			return Mode_Done(&t, Native_EntVar(e, Native_Text(name), "Name", en) && PORT_OWN(Ffa, SetSpawnPos, NULL, "v", en) &&
				Mode_Send(id, Mode_S(&t, "R")));
		}
		return Mode_Done(&t, Mode_Cat(&t, msg, 3, Mode_S(&t, "net"), rep, Mode_S(&t, " not found for respawn.\n")) &&
			Mode_ConsoleError(msg));
	}
	if (Native_IsStr(c0, "V")) {
		return Mode_Done(&t, Mode_Get(e, Native_Text(name), "Name", en) && PORT_OWN(Ffa, ServerSpectate, NULL, "v", en));
	}
	if (Native_IsStr(c0, "M")) {
		if (!Native_Slice(s, 1, INT_MAX, rest) || !Native_Split(rest, "\n", data)) {
			return Mode_Done(&t, 0);
		}
		v[0] = Mode_V(&t);
		if (!Mode_Cat(&t, v[0], 2, Mode_S(&t, "net"), rep)) {
			return Mode_Done(&t, 0);
		}
		for (int i = 0; i < 10; i++) {
			v[i + 1] = Mode_V(&t);
			if (!Native_Subscript(data, Mode_I(&t, i), v[i + 1])) {
				return Mode_Done(&t, 0);
			}
			if (i == 1 || i == 9) {
				nativeVal_t *n = Mode_V(&t);
				if (!Native_ToInt(v[i + 1], n)) {
					return Mode_Done(&t, 0);
				}
				v[i + 1] = n;
			}
		}
		return Mode_Done(&t, PORT_OWN(Ffa, ClientModify, NULL, FFA_A11, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]));
	}
	return Mode_Done(&t, 1);
}

/* ---- spectating and the team menus ---- */

/* Spectate(id=0, command=""): back to the game as a spectator, or back into the game */
static byte Ffa_SpectateC(const nativeVal_t *id, const nativeVal_t *command) {
	static const char *const mods[] = { "OutMap", "RacerScorer", "Menu" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *fc = Mode_V(&t), *target = Mode_V(&t);
	for (int i = 0; i < 3; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!PORT_CALL(Menu, BackToGame, NULL, "vv", id, command)) {
		return Mode_Done(&t, 0);
	}
	cEntity *cam = Ffa_Cam();
	int on = cam != NULL;
	if (on) {
		if (!Native_EntVar(cam, "NetCam", "FreeCam", fc)) {
			return Mode_Done(&t, 0);
		}
		on = Ffa_Eq(fc, Mode_I(&t, 0));
	}
	int client = Mode_IsClient();
	if (client < 0) {
		return Mode_Done(&t, 0);
	}
	if (client) {
		return Mode_Done(&t, Mode_SendS(0, on ? "V" : "R"));
	}
	if (on) {
		return Mode_Done(&t, Native_EntVar(cam, "NetCam", "MainTarget", target) && PORT_OWN(Ffa, ServerSpectate, NULL, "v", target));
	}
	return Mode_Done(&t, Mode_SetI(cam, "NetCam", "FreeCam", 0) && Native_EntVar(cam, "NetCam", "MainTarget", target) &&
		PORT_OWN(Ffa, ServerRemoveSpectatorState, NULL, "v", target) && PORT_OWN(Ffa, DelayedRestartOnPress, NULL, "v", id));
}

/* JoinRed(id, control), JoinGreen(id, control): into the team (team id 0 or 2), rejoining when it changes */
static byte Ffa_JoinC(const nativeVal_t *a, long team) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *fc = Mode_V(&t), *tid;
	if (!Mode_Get(Ffa_Cam(), "NetCam", "FreeCam", fc)) {
		return Mode_Done(&t, 0);
	}
	int c = Ffa_Eq(fc, Mode_I(&t, 1));
	if (!c) {
		if (!Ffa_Get(&t, "PlayerTeamID", &tid)) {
			return Mode_Done(&t, 0);
		}
		c = !Ffa_Eq(tid, Mode_I(&t, team));
	}
	if (c && (!Mode_GVarSet("PlayerTeamID", Mode_I(&t, team)) || !PORT_OWN(Ffa, ReJoinGame, NULL, "vv", &a[0], &a[1]))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_Import("Menu") && PORT_CALL(Menu, BackToGame, NULL, "vv", &a[0], &a[1]));
}

/* ReJoinGame(id, control): spectate, send the new ship, join again after a 10 s count down */
static byte Ffa_ReJoinGameC(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *fc = Mode_V(&t);
	if (!Mode_GVarSetS("EscapeEvent", "")) {
		return Mode_Done(&t, 0);
	}
	cEntity *cam = Ffa_Cam();
	int c = cam != NULL;
	if (c) {
		if (!Native_EntVar(cam, "NetCam", "FreeCam", fc)) {
			return Mode_Done(&t, 0);
		}
		c = Ffa_Eq(fc, Mode_I(&t, 0));
	}
	return Mode_Done(&t, (!c || PORT_OWN(Ffa, Spectate, NULL, "vv", &a[0], &a[1])) && PORT_OWN(Ffa, ModiTry, NULL, "") &&
		PORT_OWN(Ffa, RejoinMsg, NULL, "ivv", 10L, &a[0], &a[1]));
}

/* RejoinMsg(timer, id, control): the count down, one message a second, then JoinGame */
static byte Ffa_RejoinMsgC(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *timer = Mode_V(&t), *half = Mode_V(&t), *text = Mode_V(&t), *args = Mode_V(&t);
	if (!Native_Arith('-', &a[0], Mode_I(&t, 1), timer)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(half, 0.5);
	if (Native_Compare(timer, half) <= 0) {
		return Mode_Done(&t, PORT_OWN(Ffa, JoinGame, NULL, "vv", &a[1], &a[2]));
	}
	Native_SetEmpty(args, NV_LIST);
	Native_Copy(Native_Append(args), timer);
	Native_Copy(Native_Append(args), &a[1]);
	Native_Copy(Native_Append(args), &a[2]);
	return Mode_Done(&t, Mode_LangFmt("Net_Msg_Join_Time", timer, text) && Mode_MsgText(text, Mode_Time() + 1.0) &&
		Mode_Later(1.0, "Ffa", "RejoinMsg", args->items, 3, NULL));
}

/* JoinGame(id, control): back into the game */
static byte Ffa_JoinGameC(const nativeVal_t *a) {
	if (!Mode_GVarSetS("EscapeEvent", "Menu.Init")) {
		return 0;
	}
	int client = Mode_IsClient();
	return client > 0 ? Mode_SendS(0, "R") : client == 0 && PORT_OWN(Ffa, Spectate, NULL, "vv", &a[0], &a[1]);
}

/* TeamMenu(id, control): the in-game menu of the team modes: red, green and (when playing) spectate */
static byte Ffa_TeamMenuC(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *opts = Mode_V(&t), *title = Mode_V(&t), *ys = Mode_V(&t), *row;
	if (!Mode_Import("Menu") || !PORT_CALL(Menu, StartNewMenu, NULL, "v", id)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(opts, NV_LIST);
	row = Native_Append(opts);
	Native_SetEmpty(row, NV_LIST);
	Mode_Lang("Net_Menu_Join_Red", Native_Append(row));
	Native_SetStr(Native_Append(row), "Ffa.JoinRed");
	row = Native_Append(opts);
	Native_SetEmpty(row, NV_LIST);
	Mode_Lang("Net_Menu_Join_Green", Native_Append(row));
	Native_SetStr(Native_Append(row), "Ffa.JoinGreen");
	if (!FR(spectatorMode, "SpectatorMode")) {
		return Mode_Done(&t, 0);
	}
	if (Ffa_Eq(&ffa.spectatorMode, Mode_I(&t, 0))) {
		row = Native_Append(opts);
		Native_SetEmpty(row, NV_LIST);
		Mode_Lang("Net_Menu_Spectate", Native_Append(row));
		Native_SetStr(Native_Append(row), "Ffa.Spectate");
	}
	/* Menu.VerticalMenu(id, title, opts, "PCMenu.MultiPlayerMenu", YStart = Menu.OptionMenuYStart) through its ref; the
	 * lookup of VerticalMenu now comes after the title and the YStart read, which differs only when both fail */
	Mode_Lang("Net_Menu_Game", title);
	byte ok = Mode_ModVar("Menu", "OptionMenuYStart", ys) &&
		PORT_CALL(Menu, VerticalMenu, NULL, "vvvs;YStart=v", id, title, opts, "PCMenu.MultiPlayerMenu", ys);
	return Mode_Done(&t, ok && PORT_CALL(Menu, DrawBackOptionMenu, NULL, "v", id));
}

/* GetGameTeamDescriptor(): the title of a team game: tied, red or green ahead */
static byte Ffa_GetGameTeamDescriptorC(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pair = Mode_V(&t);
	if (!Mode_Import("string") || !FR(totalRed, "TotalRed") || !FR(totalGreen, "TotalGreen")) {
		return Mode_Done(&t, 0);
	}
	if (Ffa_Eq(&ffa.totalRed, &ffa.totalGreen)) {
		return Mode_Done(&t, FR(totalGreen, "TotalGreen") && Mode_LangFmt("Net_Title_Teams_Tied", &ffa.totalGreen, out));
	}
	if (!FR(totalRed, "TotalRed") || !FR(totalGreen, "TotalGreen")) {
		return Mode_Done(&t, 0);
	}
	byte red = Native_Compare(&ffa.totalRed, &ffa.totalGreen) >= 0;
	if (!FR(totalRed, "TotalRed") || !FR(totalGreen, "TotalGreen")) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(pair, NV_TUPLE);
	Native_Copy(Native_Append(pair), red ? &ffa.totalRed : &ffa.totalGreen);
	Native_Copy(Native_Append(pair), red ? &ffa.totalGreen : &ffa.totalRed);
	return Mode_Done(&t, Mode_LangFmt(red ? "Net_Title_Red_Wins" : "Net_Title_Green_Wins", pair, out));
}

/* CameraSpectator(): the camera of this machine flies free from the first start point */
static byte Ffa_CameraSpectatorC(void) {
	static const char *const mods[] = { "OutMap", "RacerScorer", "PCMenu" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *sp = Mode_V(&t), *s0 = Mode_V(&t), *pos = Mode_V(&t), *ang = Mode_V(&t), *a3 = Mode_V(&t);
	for (int i = 0; i < 3; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Ffa_LibToMod("PCMenu", "MultiplayerMenu", "MultiplayerMenuSpectator") || !FW(spectatorMode, "SpectatorMode", Mode_I(&t, 1))) {
		return Mode_Done(&t, 0);
	}
	cEntity *cam = Ffa_Cam();
	if (!Mode_SetI(cam, "NetCam", "FreeCam", 1) || !Mode_ModVar("OutMap", "StartPos", sp) || !Native_Subscript(sp, Mode_I(&t, 0), s0) ||
		!Native_Subscript(s0, Mode_I(&t, 0), pos) || !Mode_Set(cam, "NetCam", "camPos", pos)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(sp);
	Native_Free(s0);
	if (!Mode_ModVar("OutMap", "StartPos", sp) || !Native_Subscript(sp, Mode_I(&t, 0), s0) || !Native_Subscript(s0, Mode_I(&t, 1), ang)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(a3, NV_TUPLE);
	Native_SetInt(Native_Append(a3), 0);
	Native_Copy(Native_Append(a3), ang);
	Native_SetInt(Native_Append(a3), 0);
	return Mode_Done(&t, Mode_Set(cam, "NetCam", "camAng", a3) && Ffa_TopDescriptor() &&
		Mode_SetModVar("RacerScorer", "ScorerBotMessage", Mode_S(&t, "Spectator mode")) && Mode_MPFunc(0, "Ffa.ShowScorer") &&
		FR(isScorerVisible, "isScorerVisible") && PORT_OWN(Ffa, ShowScorer, NULL, "iv", 0L, &ffa.isScorerVisible) &&
		PORT_OWN(Ffa, ActualizeGameScorer, NULL, ""));
}

/* AbortSpectateMode(): the camera follows the ship again */
static byte Ffa_AbortSpectateModeC(void) {
	modeVals_t t = { .n = 0 };
	return Mode_Done(&t, Mode_Import("PCMenu") && Mode_SetI(Ffa_Cam(), "NetCam", "FreeCam", 0) &&
		Ffa_LibToMod("PCMenu", "MultiplayerMenu", "MultiplayerMenuOnGame") && FW(spectatorMode, "SpectatorMode", Mode_I(&t, 0)));
}

/* ServerSpectate(EntityName): the server makes a player a spectator */
static byte Ffa_ServerSpectateC(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *score = Mode_V(&t), *sp = Mode_V(&t), *target = Mode_V(&t), *en = Mode_V(&t);
	cEntity *e;
	if (!Mode_Import("OutMap") || !Mode_Import("RacerScorer") || !Ffa_Score(name, score) || !Ffa_Spect(sp)) {
		return Mode_Done(&t, 0);
	}
	if (Ffa_Eq(score, sp)) {
		return Mode_Done(&t, 1);
	}
	cEntity *cam = Ffa_Cam();
	if (cam) {
		if (!Native_EntVar(cam, "NetCam", "MainTarget", target)) {
			return Mode_Done(&t, 0);
		}
		if (Ffa_Eq(target, name) && !PORT_OWN(Ffa, CameraSpectator, NULL, "")) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_Ent(name, &e) || !Mode_SetI(e, Native_Text(name), "Active", 0) || !Native_EntVar(e, Native_Text(name), "Name", en) ||
		!Ffa_Spect(sp)) {
		return Mode_Done(&t, 0);
	}
	/* the handle of the player, found again by its name after each call */
	const char *n = Native_Text(name);
	byte ok = SCORER_PUT(en, sp) && PORT_OWN(Ffa, ServerSendPlayerStatus, NULL, "v", en) && PORT_OWN(Ffa, ActualizeGameScorer, NULL, "");
	Native_Free(en);
	ok = ok && Mode_NameGet(n, "Name", en) && Ffa_Lib("ServerHidePlayer", NULL, en, 1);
	Native_Free(en);
	return Mode_Done(&t, ok && Mode_NameGet(n, "Name", en) && PORT_OWN(Ffa, PlayerSpectator, NULL, "v", en));
}

/* InitClient(): the client's callbacks, the HUD, the mode */
static byte Ffa_InitClientC(void) {
	static const char *const cbs[][2] = { { "ClientSetPlayer", "Ffa.SetupUserShip" }, { "ClientSetConfig", "Ffa.ClientSetConfig" },
		{ "ClientCreate", "Ffa.ClientCreate" }, { "ClientAfterCreate", "Ffa.ClientAfterCreate" }, { "OnUsrString", "Ffa.OnUsrClient" } };
	if (!PORT_OWN(Ffa, InitFfa, NULL, "")) {
		return 0;
	}
	for (int i = 0; i < 5; i++) {
		if (!Mode_GVarSetS(cbs[i][0], cbs[i][1])) {
			return 0;
		}
	}
	return PORT_OWN(Ffa, StdInitFfa, NULL, "") && Ffa_Lib("InitClientMod", NULL, NULL, 0);
}

/* StdInitFfa(): the ship controls, the "press fire" scorer, the scorer back from a menu */
static byte Ffa_StdInitFfaC(void) {
	nativeVal_t d;
	Native_SetNone(&d);
	/* SInput.SetActionSet("Racer") */
	if (!Mode_Import("RacerScorer")) {
		return 0;
	}
	Input_SelectActionSetNamed("Racer");
	byte ok = Ffa_Lib("GetGameDescriptor", &d, NULL, 0) && PORT_OWN(Ffa, ShowPresFireMessage, NULL, "iv", 0L, &d) &&
		Ffa_GlobalToMod("RacerScorer", "BackMenuFunc", "BackMenuFunc");
	Native_Free(&d);
	return ok;
}

/* ---- the scorer ---- */

static int Ffa_RowCompare(const void *a, const void *b) {
	return Native_Compare(a, b);
}

/* list.sort(); list.reverse() of a list of rows (1.5.2 sorts with cmp: the rows differ in their keys) */
static void Ffa_SortDown(nativeVal_t *list) {
	if (list->n > 1) {
		qsort(list->items, (size_t)list->n, sizeof(nativeVal_t), Ffa_RowCompare);
	}
	for (int i = 0, j = list->n - 1; i < j; i++, j--) {
		nativeVal_t x = list->items[i];
		list->items[i] = list->items[j];
		list->items[j] = x;
	}
}

/* (score, desc, color, back, key) as a new row at the end of list */
static void Ffa_Row(nativeVal_t *list, const nativeVal_t *score, const nativeVal_t *desc, const nativeVal_t *color,
	const nativeVal_t *back, const nativeVal_t *key) {
	nativeVal_t *row = Native_Append(list);
	const nativeVal_t *items[5] = { score, desc, color, back, key };
	Native_SetEmpty(row, NV_TUPLE);
	for (int i = 0; i < 5; i++) {
		Native_Copy(Native_Append(row), items[i]);
	}
}

/* GetScorerInfo(id=0): the rows of the scorer (score, name with the flag mark, text colour, back colour, entity),
 * by team and score; the player of id sorts above an equal score */
static byte Ffa_GetScorerInfoC(const nativeVal_t *id, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *lists = Mode_V(&t), *userscore = Mode_V(&t), *keys = Mode_V(&t), *none4 = Mode_V(&t), *info = Mode_V(&t);
	nativeVal_t *tginfo = Mode_V(&t), *tenth = Mode_V(&t);
	/* killer, red, green, spect */
	Native_SetEmpty(lists, NV_LIST);
	for (int i = 0; i < 4; i++) {
		Native_SetEmpty(Native_Append(lists), NV_LIST);
	}
	nativeVal_t *killer = &lists->items[0], *red = &lists->items[1], *green = &lists->items[2], *spect = &lists->items[3];
	Native_SetFloat(userscore, 0.1);
	Native_SetFloat(tenth, 0.1);
	Native_SetEmpty(none4, NV_TUPLE);
	for (int i = 0; i < 4; i++) {
		Native_SetInt(Native_Append(none4), 0);
	}
	if (!Mode_Import("RacerScorer") || !Mode_Keys("Ffa", "Scorer", 1, keys)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(keys);
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *key = Mode_V(&t), *flag = Mode_V(&t), *uc = Mode_V(&t), *score = Mode_V(&t), *sp = Mode_V(&t);
		nativeVal_t *desc = Mode_V(&t), *d = Mode_V(&t), *typo = Mode_V(&t), *f = Mode_V(&t), *back = Mode_V(&t);
		cEntity *ent;
		if (!Native_SeqItem(keys, i, key) || !FR(masterFlag, "MasterFlag")) {
			return Mode_Done(&t, 0);
		}
		Native_SetStr(flag, Ffa_Eq(&ffa.masterFlag, key) ? "*" : " ");
		if (!Mode_Ent(key, &ent) || !Mode_Get(ent, Native_Text(key), "UsrControl", uc)) {
			return Mode_Done(&t, 0);
		}
		int isuser = Ffa_Eq(uc, id);
		const nativeVal_t *color;
		if (isuser ? !FR(usrColor, "USRCOLOR") : !FR(stdColor, "STDCOLOR")) {
			return Mode_Done(&t, 0);
		}
		color = isuser ? &ffa.usrColor : &ffa.stdColor;
		if (!Ffa_Score(key, score) || !Ffa_Spect(sp)) {
			return Mode_Done(&t, 0);
		}
		if (Ffa_Eq(score, sp)) {
			Native_Free(score);
			if (!Ffa_Score(key, score) || !Native_EntVar(ent, Native_Text(key), "Descriptor", d) || !Native_Arith('+', flag, d, desc)) {
				return Mode_Done(&t, 0);
			}
			Ffa_Row(spect, score, desc, color, none4, key);
		} else {
			if (!Native_EntVar(ent, Native_Text(key), "TgType", typo)) {
				return Mode_Done(&t, 0);
			}
			Native_Free(score);
			if (!Ffa_Score(key, score) || !Native_ToFloat(score, f)) {
				return Mode_Done(&t, 0);
			}
			if (isuser) {
				Native_Free(score);
				if (!Native_Arith('+', f, tenth, score)) {
					return Mode_Done(&t, 0);
				}
				Native_Free(f);
				Native_Copy(f, score);
				Native_Free(userscore);
				Native_Copy(userscore, score);
			}
			int r = Native_IsStr(typo, "red"), g = !r && Native_IsStr(typo, "green");
			if (!Native_EntVar(ent, Native_Text(key), "Descriptor", d) || !Native_Arith('+', flag, d, desc)) {
				return Mode_Done(&t, 0);
			}
			if (r || g) {
				if (r ? !FR(redColor, "REDCOLOR") : !FR(greenColor, "GREENCOLOR")) {
					return Mode_Done(&t, 0);
				}
				Ffa_Row(r ? red : green, f, desc, color, r ? &ffa.redColor : &ffa.greenColor, key);
			} else {
				Ffa_Row(killer, f, desc, color, none4, key);
			}
			(void)back;
		}
		Mode_Release(&t, mark);
	}
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 4; i++) {
		Ffa_SortDown(&lists->items[i]);
	}
	if (!FR(totalGreen, "TotalGreen") || !FR(totalRed, "TotalRed")) {
		return Mode_Done(&t, 0);
	}
	int gw = Native_Compare(&ffa.totalGreen, &ffa.totalRed) > 0;
	if (!Mode_Cat(&t, info, 4, gw ? green : red, gw ? red : green, killer, spect)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(tginfo, NV_LIST);
	for (int i = 0; i < info->n; i++) {
		int mark = t.n;
		nativeVal_t *item = &info->items[i], *score = Mode_V(&t), *iscore = Mode_V(&t);
		nativeVal_t *it[5];
		for (int k = 0; k < 5; k++) {
			it[k] = Mode_V(&t);
			if (!Native_SeqItem(item, k, it[k])) {
				return Mode_Done(&t, 0);
			}
		}
		Native_Copy(score, it[0]);
		if (Ffa_Eq(score, userscore)) {
			Native_Free(score);
			if (!Native_Arith('-', userscore, tenth, score)) {
				return Mode_Done(&t, 0);
			}
		}
		if (!Native_ToInt(score, iscore)) {
			return Mode_Done(&t, 0);
		}
		Ffa_Row(tginfo, iscore, it[1], it[2], it[3], it[4]);
		Mode_Release(&t, mark);
	}
	Native_Copy(out, tginfo);
	return Mode_Done(&t, 1);
}

/* OnPositionChange(newpos, prevpos, value, prevValue, newwinner, winner): the label of a change of the lead or of the
 * place (free for all only) */
static byte Ffa_OnPositionChangeC(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *txpos = Mode_V(&t), *rep = Mode_V(&t), *text = Mode_V(&t);
	const nativeVal_t *newpos = &a[0], *prevpos = &a[1], *value = &a[2], *prevvalue = &a[3], *newwinner = &a[4], *winner = &a[5];
	if (Ffa_Eq(newpos, Mode_I(&t, 1))) {
		Native_SetStr(txpos, "1st");
	} else if (Ffa_Eq(newpos, Mode_I(&t, 2))) {
		Native_SetStr(txpos, "2nd");
	} else {
		Mode_Repr(newpos, rep);
		if (!Mode_Cat(&t, txpos, 2, rep, Mode_S(&t, "th"))) {
			return Mode_Done(&t, 0);
		}
	}
	int lead = !Ffa_Eq(newwinner, winner);
	if (!lead) {
		lead = !Ffa_Eq(value, prevvalue) && !Ffa_Eq(winner, Mode_I(&t, -1));
	}
	if (lead) {
		int team = Ffa_Team();
		if (team != 0) {
			return Mode_Done(&t, team > 0);
		}
		const char *key = Ffa_Eq(newwinner, Mode_I(&t, -1)) ? "Net_Label_Lost_Lead" : Ffa_Eq(newwinner, Mode_I(&t, 0)) ?
			"Net_Label_Tied_Lead" : "Net_Label_Tied_Leading";
		return Mode_Done(&t, Mode_Label(0, key, 5));
	}
	int place = !Ffa_Eq(value, prevvalue) || !Ffa_Eq(newpos, prevpos);
	if (!place) {
		return Mode_Done(&t, 1);
	}
	int team = Ffa_Team();
	if (team != 0) {
		return Mode_Done(&t, team > 0);
	}
	int c = Native_Compare(newpos, prevpos);
	return Mode_Done(&t, Mode_LangFmt(c < 0 ? "Net_Label_Up_to" : c > 0 ? "Net_Label_Down_to" : "Net_Label_Still_to", txpos, text) &&
		Mode_LabelText(text, Mode_Time() + 5.0));
}

/* isTied(value): 1 when value leads GameScorer alone, 0 when tied at the top, -1 when not at the top */
static byte Ffa_isTiedC(const nativeVal_t *value, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	long pos = -1, ctr = 0;
	if (!FR(gameScorer, "GameScorer")) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(&ffa.gameScorer);
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *key = Mode_V(&t), *k0 = Mode_V(&t);
		if (!Native_SeqItem(&ffa.gameScorer, i, key) || !Native_Subscript(key, Mode_I(&t, 0), k0)) {
			return Mode_Done(&t, 0);
		}
		if (Ffa_Eq(k0, value)) {
			ctr++;
			if (pos == -1) {
				pos = i + 1;
			}
		}
		Mode_Release(&t, mark);
	}
	Native_SetInt(out, pos != 1 ? -1 : ctr == 1 ? 1 : 0);
	return Mode_Done(&t, n >= 0);
}

/* CheckGameStart(): the game counts once two active players (one per team in a team game) are in */
static byte Ffa_CheckGameStartC(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *keys = Mode_V(&t);
	if (!FR(gameStart, "GameStart")) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(&ffa.gameStart)) {
		Native_SetInt(out, 1);
		return Mode_Done(&t, 1);
	}
	if (!Mode_Keys("Ffa", "Scorer", 1, keys)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(keys);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	if (n <= 1) {
		Native_SetInt(out, 0);
		return Mode_Done(&t, 1);
	}
	int team = Ffa_Team();
	long red = 0, green = 0;
	Native_Free(keys);
	if (team < 0 || !Mode_Keys("Ffa", "Scorer", 1, keys)) {
		return Mode_Done(&t, 0);
	}
	n = Native_ForCount(keys);
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *name = Mode_V(&t), *act = Mode_V(&t), *tg = Mode_V(&t);
		cEntity *ply;
		if (!Native_SeqItem(keys, i, name) || !Mode_Ent(name, &ply)) {
			return Mode_Done(&t, 0);
		}
		int on = ply != NULL;
		if (on) {
			if (!Native_EntVar(ply, Native_Text(name), "Active", act)) {
				return Mode_Done(&t, 0);
			}
			on = Native_True(act);
		}
		if (on && team) {
			if (!Native_EntVar(ply, Native_Text(name), "TgType", tg)) {
				return Mode_Done(&t, 0);
			}
			if (Native_IsStr(tg, "red")) {
				red++;
			} else {
				green++;
			}
		} else if (on) {
			red++;
		}
		Mode_Release(&t, mark);
	}
	if (n < 0 || ((team ? green > 0 && red > 0 : red > 1) && !FW(gameStart, "GameStart", Mode_I(&t, 1))) || !FR(gameStart, "GameStart")) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(out, &ffa.gameStart);
	return Mode_Done(&t, 1);
}

/* UsrPos(info): the place of the player's row (USRCOLOR), 1 when none */
static byte Ffa_UsrPosC(const nativeVal_t *info, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	int n = Native_ForCount(info);
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *key = Mode_V(&t), *k2 = Mode_V(&t);
		if (!Native_SeqItem(info, i, key) || !Native_Subscript(key, Mode_I(&t, 2), k2) || !FR(usrColor, "USRCOLOR")) {
			return Mode_Done(&t, 0);
		}
		if (Ffa_Eq(k2, &ffa.usrColor)) {
			Native_SetInt(out, i + 1);
			return Mode_Done(&t, 1);
		}
		Mode_Release(&t, mark);
	}
	Native_SetInt(out, 1);
	return Mode_Done(&t, n >= 0);
}

/* ActualizeGameScorer(NowShowScorer=1): GameScorer again, the place labels, the scorer shown or its values set (now
 * NULL: the default) */
static byte Ffa_ActualizeGameScorerC(const nativeVal_t *now) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *gs = Mode_V(&t), *yp = Mode_V(&t), *yp1 = Mode_V(&t), *row = Mode_V(&t), *value = Mode_V(&t), *nw = Mode_V(&t);
	if (!Mode_Import("RacerScorer") || !PORT_OWN(Ffa, GetScorerInfo, gs, "i", 0L) || !FW(gameScorer, "GameScorer", gs) ||
		!FR(gameScorer, "GameScorer")) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(&ffa.gameScorer);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	if (n > 0) {
		if (!FR(gameScorer, "GameScorer") || !PORT_OWN(Ffa, UsrPos, yp, "v", &ffa.gameScorer) || !FR(gameScorer, "GameScorer") ||
			!Native_Arith('-', yp, Mode_I(&t, 1), yp1) || !Native_Subscript(&ffa.gameScorer, yp1, row) ||
			!Native_Subscript(row, Mode_I(&t, 0), value) || !PORT_OWN(Ffa, isTied, nw, "v", value) || !FR(yourPosition, "YourPosition")) {
			return Mode_Done(&t, 0);
		}
		int c = !Ffa_Eq(yp, &ffa.yourPosition);
		if (!c) {
			if (!FR(yourScore, "YourScore")) {
				return Mode_Done(&t, 0);
			}
			c = !Ffa_Eq(value, &ffa.yourScore);
		}
		if (!c) {
			if (!FR(winner, "Winner")) {
				return Mode_Done(&t, 0);
			}
			c = !Ffa_Eq(nw, &ffa.winner);
		}
		if (c && (!FR(yourPosition, "YourPosition") || !FR(yourScore, "YourScore") || !FR(winner, "Winner") ||
			!PORT_OWN(Ffa, OnPositionChange, NULL, "vvvvvv", yp, &ffa.yourPosition, value, &ffa.yourScore, nw, &ffa.winner) ||
			!FW(yourPosition, "YourPosition", yp) || !FW(yourScore, "YourScore", value) || !FW(winner, "Winner", nw))) {
			return Mode_Done(&t, 0);
		}
	}
	int team = Ffa_Team();
	if (team < 0 || (team > 0 && !Ffa_TopDescriptor()) || !FR(isScorerVisible, "isScorerVisible")) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(&ffa.isScorerVisible)) {
		return Mode_Done(&t, (now && !Native_True(now)) || PORT_OWN(Ffa, ShowScorer, NULL, "ii", 0L, 1L));
	}
	return Mode_Done(&t, PORT_OWN(Ffa, SetScorerValues, NULL, "i", 0L));
}

/* ShowScorer(id, doit): the scores on screen or the plain HUD (not while a menu is up) */
static byte Ffa_ShowScorerC(const nativeVal_t *id, const nativeVal_t *doit) {
	if (!FW(isScorerVisible, "isScorerVisible", doit) || !Mode_Import("RacerScorer")) {
		return 0;
	}
	if (Mode_ActionSetIs("Menu")) {
		return 1;
	}
	if (!PORT_OWN(Ffa, ActualizeGameScorer, NULL, "i", 0L)) {
		return 0;
	}
	if (Native_True(doit)) {
		return FR(gameScorer, "GameScorer") && PORT_CALL(RacerScorer, ShowScorer, NULL, "vv", id, &ffa.gameScorer);
	}
	return PORT_CALL(RacerScorer, Create, NULL, "v", id);
}

/* SetupGameRules(s): the client gets the frag limit */
static byte Ffa_SetupGameRulesC(const nativeVal_t *s) {
	nativeVal_t v;
	if (!Native_ToInt(s, &v)) {
		return 0;
	}
	return FW(scorerFraglimit, "ScorerFraglimit", &v) && PORT_OWN(Ffa, ActualizeGameScorer, NULL, "");
}

/* SendGameRules(clientid): the server sends the frag limit */
static byte Ffa_SendGameRulesC(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rep = Mode_V(&t), *s = Mode_V(&t);
	if (!FR(scorerFraglimit, "ScorerFraglimit")) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(&ffa.scorerFraglimit, rep);
	return Mode_Done(&t, Mode_Cat(&t, s, 2, Mode_S(&t, "G"), rep) && Mode_Send(id, s));
}

/* SScorer.Set(id, item, var, v) */
static byte Ffa_Set(modeVals_t *t, const nativeVal_t *id, const char *item, const char *var, const nativeVal_t *v) {
	return Mode_ScorerSet(id, Mode_S(t, item), var, v);
}

/* SScorer.Set(id, item, "Text", `v`) */
static byte Ffa_SetRepr(modeVals_t *t, const nativeVal_t *id, const char *item, const nativeVal_t *v) {
	nativeVal_t *rep = Mode_V(t);
	Mode_Repr(v, rep);
	return Ffa_Set(t, id, item, "Text", rep);
}

/* GameScorer[i][0] into *out */
static byte Ffa_ScorerAt(modeVals_t *t, int i, nativeVal_t *out) {
	nativeVal_t *row = Mode_V(t);
	return FR(gameScorer, "GameScorer") && Native_Subscript(&ffa.gameScorer, Mode_I(t, i), row) && Native_Subscript(row, Mode_I(t, 0), out);
}

/* SetScorerValues(id): the numbers of the mission scorer: the limit, the team totals or the best and own score */
static byte Ffa_SetScorerValuesC(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *fc = Mode_V(&t), *top = Mode_V(&t), *sp = Mode_V(&t), *pts = Mode_V(&t);
	if (!Mode_Import("RacerScorer")) {
		return Mode_Done(&t, 0);
	}
	cEntity *cam = Ffa_Cam();
	if (cam) {
		if (!Native_EntVar(cam, "NetCam", "FreeCam", fc)) {
			return Mode_Done(&t, 0);
		}
		if (Ffa_Eq(fc, Mode_I(&t, 1))) {
			return Mode_Done(&t, 1);
		}
	}
	if (!FR(scorerFraglimit, "ScorerFraglimit") || !Ffa_SetRepr(&t, id, "MissionLimit", &ffa.scorerFraglimit)) {
		return Mode_Done(&t, 0);
	}
	int team = Ffa_Team();
	if (team < 0) {
		return Mode_Done(&t, 0);
	}
	if (team) {
		if (!FR(totalRed, "TotalRed") || !FR(totalGreen, "TotalGreen")) {
			return Mode_Done(&t, 0);
		}
		int r = Native_Compare(&ffa.totalRed, &ffa.totalGreen) > 0;
		const char *rc[2] = { r ? "MissionSLimit" : "MissionYou", r ? "MissionBest" : "MissionBad" };
		const char *gc[2] = { r ? "MissionYou" : "MissionSLimit", r ? "MissionBad" : "MissionBest" };
		return Mode_Done(&t, Ffa_Set(&t, id, rc[0], "Green", Mode_I(&t, 0)) && Ffa_Set(&t, id, rc[0], "Red", Mode_I(&t, 255)) &&
			FR(totalRed, "TotalRed") && Ffa_SetRepr(&t, id, rc[1], &ffa.totalRed) && Ffa_Set(&t, id, gc[0], "Green", Mode_I(&t, 255)) &&
			Ffa_Set(&t, id, gc[0], "Red", Mode_I(&t, 0)) && FR(totalGreen, "TotalGreen") && Ffa_SetRepr(&t, id, gc[1], &ffa.totalGreen));
	}
	if (!FR(gameScorer, "GameScorer")) {
		return Mode_Done(&t, 0);
	}
	int n = Native_Size(&ffa.gameScorer);
	if (n == 0) {
		return Mode_Done(&t, Ffa_Set(&t, id, "MissionBest", "Text", Mode_S(&t, "-")) && Ffa_Set(&t, id, "MissionBad", "Text", Mode_S(&t, "-")) &&
			Ffa_Set(&t, id, "MissionYou", "Blue", Mode_I(&t, 0)) && Ffa_Set(&t, id, "MissionYou", "Red", Mode_I(&t, 0)));
	}
	if (n < 0 || !FR(yourScore, "YourScore") || !Ffa_ScorerAt(&t, 0, top)) {
		return Mode_Done(&t, 0);
	}
	if (Ffa_Eq(&ffa.yourScore, top)) {
		if (!FR(yourScore, "YourScore") || !Ffa_SetRepr(&t, id, "MissionBest", &ffa.yourScore) || !FR(gameScorer, "GameScorer")) {
			return Mode_Done(&t, 0);
		}
		int len = Native_Size(&ffa.gameScorer);
		int indx = len > 1 ? 1 : 0;
		Native_Free(top);
		if (len < 0 || !Ffa_Spect(sp) || !Ffa_ScorerAt(&t, indx, top)) {
			return Mode_Done(&t, 0);
		}
		if (Ffa_Eq(sp, top)) {
			Native_SetInt(pts, 0);
		} else if (!Ffa_ScorerAt(&t, indx, pts)) {
			return Mode_Done(&t, 0);
		}
		return Mode_Done(&t, Ffa_SetRepr(&t, id, "MissionBad", pts) && Ffa_Set(&t, id, "MissionYou", "Y", Mode_I(&t, 180)) &&
			Ffa_Set(&t, id, "MissionYou", "Blue", Mode_I(&t, 255)) && Ffa_Set(&t, id, "MissionYou", "Red", Mode_I(&t, 0)));
	}
	/* pts is computed and not used, as in the script */
	Native_Free(top);
	if (!Ffa_Spect(sp) || !Ffa_ScorerAt(&t, 0, top) || (!Ffa_Eq(sp, top) && !Ffa_ScorerAt(&t, 0, pts))) {
		return Mode_Done(&t, 0);
	}
	Native_Free(top);
	return Mode_Done(&t, Ffa_ScorerAt(&t, 0, top) && Ffa_SetRepr(&t, id, "MissionBest", top) && FR(yourScore, "YourScore") &&
		Ffa_SetRepr(&t, id, "MissionBad", &ffa.yourScore) && Ffa_Set(&t, id, "MissionYou", "Y", Mode_I(&t, 200)) &&
		Ffa_Set(&t, id, "MissionYou", "Blue", Mode_I(&t, 0)) && Ffa_Set(&t, id, "MissionYou", "Red", Mode_I(&t, 255)));
}

/* FfaScorer(id): Menu.MissionScorerCall: the mission scorer of the HUD with the values (not for a spectator) */
static byte Ffa_FfaScorerC(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *fc = Mode_V(&t);
	if (!Mode_Import("RacerScorer")) {
		return Mode_Done(&t, 0);
	}
	cEntity *cam = Ffa_Cam();
	if (!cam) {
		return Mode_Done(&t, 1);
	}
	if (!Native_EntVar(cam, "NetCam", "FreeCam", fc)) {
		return Mode_Done(&t, 0);
	}
	if (Ffa_Eq(fc, Mode_I(&t, 1))) {
		return Mode_Done(&t, 1);
	}
	if (!PORT_CALL(RacerScorer, MissionScorer, NULL, "v", id)) {
		return Mode_Done(&t, 0);
	}
	int team = Ffa_Team();
	return Mode_Done(&t, team >= 0 && (!team || PORT_CALL(RacerScorer, MissionScorerTeam, NULL, "v", id)) &&
		PORT_OWN(Ffa, SetScorerValues, NULL, "v", id));
}

/* ---- the adapters of the module table (OBJLIB 7: arguments, results and the errors of the Python calls) ---- */

/* the entry of a function of nargs arguments with the core call (a the arguments, out the result) */
#define FFA_FN(name, nargs, call) MODE_FN(Ffa, name, nargs, call)

FFA_FN(InitFfa, 0, Ffa_InitFfaC())
FFA_FN(SetupUserShip, 1, Ffa_SetupUserShipC(&a[0]))
FFA_FN(RestartOnPress, 1, Ffa_RestartOnPressC(&a[0]))
FFA_FN(DelayedRestartOnPress, 1, Ffa_DelayedRestartOnPressC(&a[0]))
FFA_FN(OnDeath, 2, Ffa_OnDeathC(&a[0], &a[1]))
FFA_FN(ShowPresFireMessage, 2, Ffa_ShowPresFireMessageC(&a[0], &a[1]))
FFA_FN(DelayedRestartOnTime, 1, Ffa_DelayedRestartOnTimeC(&a[0]))
FFA_FN(EndOfGame, 0, Ffa_EndOfGameC())
FFA_FN(ServerSendPlayerStatus, 1, Ffa_ServerSendPlayerStatusC(&a[0]))
FFA_FN(ServerSendTeamData, 0, Ffa_ServerSendTeamDataC())
FFA_FN(ServerEndOfGame, 0, Ffa_ServerEndOfGameC())
FFA_FN(OnDeathServer, 2, Ffa_OnDeathServerC(&a[0], &a[1]))
FFA_FN(OnDeathClient, 2, Ffa_OnDeathClientC(&a[0], &a[1]))
FFA_FN(Scorer2Str, 0, Ffa_Scorer2StrC(out))
FFA_FN(Str2Scorer, 1, Ffa_Str2ScorerC(&a[0]))
FFA_FN(ClientJoin, 11, Ffa_ClientJoinC(a))
FFA_FN(BuildCfgString, 0, Ffa_BuildCfgStringC(out))
FFA_FN(ModiTry, 0, Ffa_ModiTryC())
FFA_FN(ModifyOnReyoin, 11, Ffa_ModifyOnReyoinC(a))
FFA_FN(ClientModify, 11, Ffa_ClientModifyC(a))
FFA_FN(TryToModify, 11, Ffa_TryToModifyC(a))
FFA_FN(PlayerMsgShow, 2, Ffa_PlayerMsgShowC(&a[0], &a[1]))
FFA_FN(PlayerDisconected, 1, PORT_OWN(Ffa, PlayerMsgShow, NULL, "sv", "%s leaves the match.", &a[0]))
FFA_FN(PlayerSpectator, 1, PORT_OWN(Ffa, PlayerMsgShow, NULL, "sv", "%s becomes spectator.", &a[0]))
FFA_FN(PlayerJoinGame, 1, Ffa_PlayerJoinGameC(&a[0]))
FFA_FN(ClientExit, 3, Ffa_ClientExitC(&a[0]))
FFA_FN(ReSpawnClient, 1, Ffa_ReSpawnClientC(&a[0]))
FFA_FN(ServerRemoveSpectatorState, 1, Ffa_ServerRemoveSpectatorStateC(&a[0]))
FFA_FN(SetSpawnPos, 1, Ffa_SetSpawnPosC(&a[0]))
FFA_FN(InitServer, 1, Ffa_InitServerC(&a[0]))
FFA_FN(ClientSetConfig, 11, Ffa_ClientSetConfigC(a, out))
FFA_FN(ClientAfterCreate, 2, Mode_Import("Net") && VARCALL("Net", "ClientAfterCreate", NULL, "vv", &a[0], &a[1]))
FFA_FN(ClientCreate, 2, Mode_Import("Net") && VARCALL("Net", "ClientCreate", NULL, "vv", &a[0], &a[1]))
FFA_FN(OnUsrTest, 2, Mode_Print((const nativeVal_t *const[]){ &a[0], &a[1] }, 2))
FFA_FN(SvrShowMsg, 1, Ffa_SvrShowMsgC(&a[0]))
FFA_FN(OnUsrClient, 2, Ffa_OnUsrClientC(&a[1]))
FFA_FN(OnUsrServer, 2, Ffa_OnUsrServerC(&a[0], &a[1]))
FFA_FN(JoinRed, 2, Ffa_JoinC(a, 0))
FFA_FN(JoinGreen, 2, Ffa_JoinC(a, 2))
FFA_FN(ReJoinGame, 2, Ffa_ReJoinGameC(a))
FFA_FN(RejoinMsg, 3, Ffa_RejoinMsgC(a))
FFA_FN(JoinGame, 2, Ffa_JoinGameC(a))
FFA_FN(TeamMenu, 2, Ffa_TeamMenuC(&a[0]))
FFA_FN(GetGameTeamDescriptor, 0, Ffa_GetGameTeamDescriptorC(out))
FFA_FN(CameraSpectator, 0, Ffa_CameraSpectatorC())
FFA_FN(AbortSpectateMode, 0, Ffa_AbortSpectateModeC())
FFA_FN(ServerSpectate, 1, Ffa_ServerSpectateC(&a[0]))
FFA_FN(InitClient, 0, Ffa_InitClientC())
FFA_FN(StdInitFfa, 0, Ffa_StdInitFfaC())
FFA_FN(OnPositionChange, 6, Ffa_OnPositionChangeC(a))
FFA_FN(isTied, 1, Ffa_isTiedC(&a[0], out))
FFA_FN(CheckGameStart, 0, Ffa_CheckGameStartC(out))
FFA_FN(UsrPos, 1, Ffa_UsrPosC(&a[0], out))
FFA_FN(ShowScorer, 2, Ffa_ShowScorerC(&a[0], &a[1]))
FFA_FN(BackMenuFunc, 1, FR(isScorerVisible, "isScorerVisible") && PORT_OWN(Ffa, ShowScorer, NULL, "vv", &a[0], &ffa.isScorerVisible))
FFA_FN(ShowHelp, 1, Ffa_Lib("ShowHelp", NULL, &a[0], 1))
FFA_FN(SetupGameRules, 1, Ffa_SetupGameRulesC(&a[0]))
FFA_FN(SendGameRules, 1, Ffa_SendGameRulesC(&a[0]))
FFA_FN(SetScorerValues, 1, Ffa_SetScorerValuesC(&a[0]))
FFA_FN(FfaScorer, 1, Ffa_FfaScorerC(&a[0]))

/* the keyword functions: their parameters by name and their defaults */
MODE_KW(Ffa, Spectate, "|OO", "id", "command") {
	Mode_KwDefI(v, given, 0, 0);
	Mode_KwDefS(v, given, 1, "");
	return Ffa_SpectateC(&v[0], &v[1]);
}

MODE_KW(Ffa, GetScorerInfo, "|O", "id") {
	Mode_KwDefI(v, given, 0, 0);
	return Ffa_GetScorerInfoC(&v[0], out);
}

/* ActualizeGameScorer(NowShowScorer), NowShowScorer optional */
static byte Ffa_ActualizeGameScorerK(const nativeVal_t *const *a, nativeVal_t *out) {
	(void)out;
	return Ffa_ActualizeGameScorerC(a[0]);
}

static byte Ffa_ActualizeGameScorer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	return Mode_KwEntry(a, n, "|O", Ffa_ActualizeGameScorerK, out);
}

static char *Ffa_ActualizeGameScorer_N[] = { "NowShowScorer", NULL };

static PyObject *Ffa_ActualizeGameScorer(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return Mode_KwCall(args, kw, "|O", Ffa_ActualizeGameScorer_N, Ffa_ActualizeGameScorerK);
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

PORTS(Ffa)
