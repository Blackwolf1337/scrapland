/*
 * The scripts of the level Press in C (data01.packed levels/press/scripts, issue #2): the Map module (the NPCs, the
 * elevators, the alarm of the forbidden area, the bureau), MapPlanes (the blueprints of Berto, Betty and Humphrey to
 * photograph), MapDemoClose (the demo's closed exits), MapSnd (LoadSounds, SetSounds with the sounds of the mental
 * controller, News1; the rest of MapSnd is data) and the missions Mission_FicusBanker, Mission_GetCrazyData,
 * Mission_GoToPress, Mission_GoWithHuman, Mission_HumphreyArchbishopKilled, Mission_IntroHuman2, Mission_MeetBoss,
 * Mission_PressMeeting, Mission_TalkToBoss, Mission_WhoIsDeepThroat and Mission_WhoIsTheBaddie. The plain functions
 * are data (data/levels/press/<module>.def): the scene loaders and the functions that only call others.
 *
 * The module variables and the module code stay in the scripts: Map (Libraries, AlarmState, the alarm activators
 * ACTIVATORS, DEACTIVATORS, ActivatorPos, DeactivatorPos, PREVALARM, PREVGUARANTEEDCHARS, the discardable characters),
 * MapPlanes (Naves, Motores), MapSnd (isPC, cntr), Mission_FicusBanker (NextMissionData, Model_Check, Firstone),
 * Mission_GetCrazyData (switches, switchesRemaining, missionTime, TimerCanBeSeen, bureauPos), Mission_GoWithHuman
 * (libname, maxHumanDist, minDistFromSwitch, switch, human, FXSceneHumanCDFile), Mission_MeetBoss (alarmFallDelta,
 * sebastian, FirstBertoSpeech), Mission_TalkToBoss (human, bossName, alarmFallDelta, maxHumanDist, minHumanDist),
 * Mission_WhoIsTheBaddie (BAD_REPUTATION, STD_SHIPS_PROFILE, libname).
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values, read the globals of their module where the
 * script reads them and write them back at once (OBJLIB 5; the lists of the modules are changed in place), and call
 * the engine in C: save variables, engine variables, the dummy search, the language texts, the time speed and the
 * action set. The engine functions the cases record or swap go through their seams (AddScheduledFunc,
 * DeleteScheduledFuncs, CreateEntity, UsrEntity, Set, SetAlarm, GetAlarm, Rand, DelSaveVars, SScorer.SetLabelText,
 * SetMsgText and Fade, SSound.PlaySound and LoadSound, the SAct functions; OBJLIB 7). The entities the script holds are
 * handle values (OBJLIB 7). The calls of other ports (the keyword calls of Sound among them) go through their refs
 * (PORT_CALL: directly while the attribute holds the port's function, else by name) and the module's own functions
 * through their globals (PORT_LOWN, the same seam on the global: a mission may replace one). The callbacks of Speech
 * and of the CharsNPC characters are named by their refs (LV_ONEND, LV_COM, LV_CALLBACK, LV_MISSIONCHAR); they and the
 * hooks of PR_HOOK are the values of the variables (OBJLIB 5). SAI.SetStateChar and GetStateChar,
 * SLogic.SendSentinelToWatch, Scrap.CallElements, SScorer.AddModel and SSound.UsePS stay by name (OBJLIB 7); the plain
 * functions of the .def files are data, called and given as callbacks by name (OBJLIB 6); the call lines of the scene
 * tables and of News1 are data in C that the stage 4 registry resolves (OBJLIB 4); CharsNPC.CharRes and the GateKeeper
 * groups are the objects they are (OBJLIB 5); the scheduler keeps Python callables (OBJLIB 4). MapPlanes is
 * LevelCommon's.
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/scrap.h"
#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL press

#define MAP "Map"
#define MPL "MapPlanes"
#define MDC "MapDemoClose"
#define FB "Mission_FicusBanker"
#define GC "Mission_GetCrazyData"
#define GP "Mission_GoToPress"
#define GW "Mission_GoWithHuman"
#define HK "Mission_HumphreyArchbishopKilled"
#define IH "Mission_IntroHuman2"
#define MB "Mission_MeetBoss"
#define PM "Mission_PressMeeting"
#define TB "Mission_TalkToBoss"
#define WD "Mission_WhoIsDeepThroat"
#define WB "Mission_WhoIsTheBaddie"

/* SAI.SetStateChar(...) by name (OBJLIB 7: the cases record it) */
#define CHAR_STATE(...) LV_CALL("SAI", "SetStateChar", NULL, __VA_ARGS__)
/* module.var = the global name of src (a function) (OBJLIB 5) */
#define PR_HOOK(module, var, src, name) LvN_ModSetGlobal(module, var, src, name)
/* the scheduled call of a global function of module after delay s without arguments, and with one */
#define PR_LATER(s, module, func) Mode_Later(s, module, func, NULL, 0, NULL)
#define PR_LATER1(s, module, func, v) Mode_Later(s, module, func, v, 1, NULL)

/* ---- native helpers of this file ---- */

/* Speech.AddRemoteCom(msg, who[, func, params]) with func a global of module (NULL: none; params: a fourth ()); a
 * function of the module named by its ref goes through LV_COM, the functions of the .def files are data and go by
 * their names (OBJLIB 6) */
static byte Pr_Remote(const char *msg, const char *who, const char *module, const char *func, int params) {
	return LvN_Com(msg, who, module, func, params ? LvN_NoArgs() : NULL);
}

/* e = Scrap.GetEntity(name); if e: e.Del() */
static byte Pr_DelIf(const nativeVal_t *name) {
	nativeVal_t e;
	if (!Mode_EntityH(name, &e)) {
		return 0;
	}
	byte ok = !Native_True(&e) || Mode_VDel(&e);
	Native_Free(&e);
	return ok;
}

static byte Pr_DelIfS(const char *name) {
	nativeVal_t n;
	LvN_Str(&n, name);
	byte ok = Pr_DelIf(&n);
	Native_Free(&n);
	return ok;
}

/* usr = Scrap.UsrEntity(0); usr.Pos = pos; usr.Ang = 0, ang, 0; then the fade from black of the scene's end */
static byte Pr_UsrBack(long x, long y, long z, long ang, double fade) {
	nativeVal_t usr;
	Native_SetNone(&usr);
	byte ok = Mode_UsrHandle(&usr) && LvN_SetInts3(&usr, "Pos", x, y, z) && LvN_SetInts3(&usr, "Ang", 0, ang, 0) &&
		Mode_Fade(0, lv_black, lv_clear, fade);
	Native_Free(&usr);
	return ok;
}

/* the gatekeeper of the boss room at its teleporter: SetDoor, DoorFoe(GateKeeper.<foe>), elv.UseFunc = use; elv into out
 * (NULL: dropped) */
static byte Pr_BossDoor(const char *foe, const char *use, nativeVal_t *out) {
	nativeVal_t elv;
	Native_SetNone(&elv);
	if (!PORT_CALL(GateKeeper, SetDoor, NULL, "ss", "GateKeeper_DTritusBossRoom", "DM_Door_Teleporter_08") ||
		!Mode_DoorFoe("GateKeeper_DTritusBossRoom", foe) || !LvN_Entity("DM_Door_Teleporter_08", &elv)) {
		return 0;
	}
	byte ok = LvN_SetS(&elv, "UseFunc", use);
	if (ok && out) {
		*out = elv;
	} else {
		Native_Free(&elv);
	}
	return ok;
}

/* a scene table of call lines (Mode_Calls: by name, OBJLIB 4 until the stage 4 registry resolves them); "!time|Func"
 * is Scene.AddGeneric(time, 'import '+libname+';'+libname+'.Func()') with libname the global of module (the code text
 * the scene runs, OBJLIB 6) */
static byte Pr_Scene(const char *module, const char *const *lines) {
	for (; *lines; lines++) {
		if (**lines != '!') {
			const char *one[2] = { *lines, NULL };
			if (!Mode_Calls(module, one)) {
				return 0;
			}
			continue;
		}
		modeVals_t t = { .n = 0 };
		nativeVal_t *lib = Mode_V(&t), *code = Mode_V(&t);
		const char *bar = strchr(*lines, '|');
		byte ok = Mode_G(module, "libname", lib) &&
			Mode_Cat(&t, code, 7, Mode_S(&t, "import "), lib, Mode_S(&t, ";"), lib, Mode_S(&t, "."), Mode_S(&t, bar + 1), Mode_S(&t, "()")) &&
			PORT_CALL(Scene, AddGeneric, NULL, "iv", (long)atoi(*lines + 1), code);
		if (!Mode_Done(&t, ok)) {
			return 0;
		}
	}
	return 1;
}

/* SScorer.SetLabelText(Scrap.GetLangStr("Generics_ForbiddenArea_Label"), time + 0.5) through its seam */
static byte Pr_Forbidden(double time) {
	nativeVal_t t;
	Mode_Lang("Generics_ForbiddenArea_Label", &t);
	byte ok = Mode_LabelText(&t, time + 0.5);
	Native_Free(&t);
	return ok;
}

/* ---- Map ---- */

/* Scrap.AddScheduledFunc(time, func, args) with func the global of module and time a double */
static byte Pr_At(double time, const char *module, const char *func, const nativeVal_t *args, int n) {
	nativeVal_t t;
	Native_SetFloat(&t, time);
	return Mode_AtG(&t, module, func, args, n);
}

/* RemoteMsg(i): in the forbidden area: the label, the police warns up to three times, then the alarm */
static byte PressMap_RemoteMsg(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *i = Mode_V(&t), *i1 = Mode_V(&t), *rep = Mode_V(&t), *msg = Mode_V(&t), *r = Mode_V(&t), *rt = Mode_V(&t);
	nativeVal_t *timy = Mode_V(&t), *at = Mode_V(&t), *now = Mode_V(&t);
	(void)out;
	if (!Mode_Import("Speech")) {
		return Mode_Done(&t, 0);
	}
	int on = LvN_GTrue(MAP, "AlarmState");
	if (on <= 0) {
		return Mode_Done(&t, on == 0);
	}
	double time = Mode_Time();
	if (!Pr_Forbidden(time)) {
		return Mode_Done(&t, 0);
	}
	int alarm = Mode_AlarmOn();
	if (alarm) {
		return Mode_Done(&t, alarm > 0 && PORT_LOWN(Map, StartAlarmator, NULL, ""));
	}
	Native_Copy(i, &a[0]);
	if (!Native_Arith('+', i, Mode_I(&t, 1), i1)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(i1, rep);
	if (!Native_Arith('+', Mode_S(&t, "Generics_Police_RemoteAlert"), rep, msg) ||
		!PORT_CALL(Speech, RemoteComunication, r, "vs", msg, "Police")) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(r)) {
		Native_Free(i);
		Native_Copy(i, i1);
	}
	int last = Mode_CmpI(i, ">=", 3);
	if (last) {
		return Mode_Done(&t, last > 0 && PORT_LOWN(Map, StartAlarmator, NULL, ""));
	}
	Native_SetFloat(timy, 3.0);
	/* RemainTalkTime < 2.8 */
	Native_SetFloat(at, 2.8);
	int more = Native_GVarGet("RemainTalkTime", rt) ? Mode_Cmp(rt, "<", at) : -1;
	if (more < 0) {
		return Mode_Done(&t, 0);
	}
	if (more) {
		Native_Free(rt);
		if (!Native_GVarGet("RemainTalkTime", rt) || !Native_Arith('+', timy, rt, at)) {
			return Mode_Done(&t, 0);
		}
		Native_Copy(timy, at);
	}
	Native_SetFloat(now, time);
	Native_Free(at);
	return Mode_Done(&t, Native_Arith('+', now, timy, at) && Mode_AtG(at, MAP, "RemoteMsg", i, 1));
}

/* ForbiddenMsg(): the label of the forbidden area every second while the alarm area is on */
static byte PressMap_ForbiddenMsg(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	int on = LvN_GTrue(MAP, "AlarmState");
	if (on <= 0) {
		return on == 0;
	}
	double time = Mode_Time();
	return Pr_Forbidden(time) && Pr_At(time + 1.0, MAP, "ForbiddenMsg", NULL, 0);
}

/* StartAlarmator(): the label, and the alarm: Risitas guaranteed with the high alarm, else gears (not for a gear) */
static byte PressMap_StartAlarmator(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t one, usr;
	(void)a;
	(void)out;
	if (!PORT_LOWN(Map, ForbiddenMsg, NULL, "")) {
		return 0;
	}
	Native_SetInt(&one, 1);
	if (LvN_SaveIs("MapPress_HiAlarm", "1")) {
		return Mode_GVarSetS("GuaranteedChars", ",Risitas,") && Mode_GVarSetI("AlarmFallDelta", 0) && Mode_SetAlarm(&one);
	}
	if (!Mode_GVarSetS("GuaranteedChars", ",Gear,")) {
		return 0;
	}
	Native_SetNone(&usr);
	int on = LvN_Usr(&usr);
	if (on > 0) {
		on = LvN_CtrlIs(&usr, "Gear");
		on = on < 0 ? -1 : !on;
	}
	Native_Free(&usr);
	return on >= 0 && (!on || Mode_SetAlarm(&one));
}

/* for e in <global name>: e.OnTake = ""; e.Del(); then del <global name> */
static byte Pr_DelItems(const char *name) {
	nativeVal_t list;
	if (!Mode_G(MAP, name, &list)) {
		return 0;
	}
	int n = Mode_Loop(&list);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t e;
		Native_SetNone(&e);
		ok = Native_SeqItem(&list, i, &e) && LvN_SetS(&e, "OnTake", "") && Mode_VDel(&e);
		Native_Free(&e);
	}
	Native_Free(&list);
	return ok && Mode_DelGlobal(MAP, name);
}

/* `Scrap.GetEntity(VehicleName).UsrControl == 0`: 1, 0, -1 */
static int Pr_NoUsrControl(const nativeVal_t *veh) {
	nativeVal_t e, c;
	Native_SetNone(&e);
	Native_SetNone(&c);
	int r = Mode_EntityH(veh, &e) && Mode_VGet(&e, "UsrControl", &c) ? Mode_CmpI(&c, "==", 0) : -1;
	Native_Free(&e);
	Native_Free(&c);
	return r;
}

/* the global name of Map = Scrap.Get(var) */
static byte Pr_KeepGVar(const char *name, const char *var) {
	nativeVal_t v;
	byte ok = Native_GVarGet(var, &v) && Mode_Put(MAP, name, NULL, &v);
	Native_Free(&v);
	return ok;
}

/* ActivateAlarm(ItemName, VehicleName): the player walks into the forbidden area: the activators go, the warnings or
 * the alarm start (at once in the first 2 s), the deactivators come */
static byte PressMap_ActivateAlarm(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr;
	(void)out;
	int go = Pr_NoUsrControl(&a[1]);
	if (go <= 0) {
		return go == 0;
	}
	if (!LvN_PutI(MAP, "AlarmState", 1) || !Pr_DelItems("ACTIVATORS") || !Pr_KeepGVar("PREVALARM", "AlarmFallDelta") ||
		!Pr_KeepGVar("PREVGUARANTEEDCHARS", "GuaranteedChars")) {
		return 0;
	}
	int zero = Mode_Time() < 2.0;
	byte ok;
	if (LvN_SaveIs("MapPress_HiAlarm", "1")) {
		ok = zero ? PORT_LOWN(Map, StartAlarmator, NULL, "") : PORT_LOWN(Map, RemoteMsg, NULL, "i", 0L);
	} else {
		Native_SetNone(&usr);
		int gear = zero ? zero : Mode_EntityH(&a[1], &usr) ? LvN_CtrlIs(&usr, "Gear") : -1;
		Native_Free(&usr);
		ok = gear > 0 ? Mode_GVarSetS("GuaranteedChars", ",Gear,") && PORT_LOWN(Map, ForbiddenMsg, NULL, "") :
			gear == 0 && PORT_LOWN(Map, RemoteMsg, NULL, "i", 0L);
	}
	return ok && PORT_LOWN(Map, CreateDeactivators, NULL, "");
}

/* global list = []; an item named name for every (x, y, z, radius) of the global pos, taken by the player only */
static byte Pr_Launchers(const char *list, const char *posName, const char *name, const char *take) {
	nativeVal_t items, pos, n;
	Native_SetNone(&pos);
	Native_SetEmpty(&items, NV_LIST);
	byte ok = Mode_Put(MAP, list, NULL, &items);
	Native_Free(&items);
	if (!ok || !Mode_G(MAP, posName, &pos)) {
		return 0;
	}
	LvN_Str(&n, name);
	int count = Mode_Loop(&pos);
	ok = count >= 0;
	for (int i = 0; ok && i < count; i++) {
		nativeVal_t p, e, r;
		Native_SetNone(&p);
		Native_SetNone(&e);
		Native_SetNone(&r);
		ok = Native_SeqItem(&pos, i, &p) && LvN_CreateAt(&n, &p, "Item", &e) && LvN_Item(&p, 3, &r) && Mode_VSet(&e, "Radius", &r) &&
			LvN_SetI(&e, "OnlyUser", 1) && LvN_SetS(&e, "OnTake", take) && Mode_Append(MAP, list, &e);
		Native_Free(&p);
		Native_Free(&e);
		Native_Free(&r);
	}
	Native_Free(&pos);
	Native_Free(&n);
	return ok;
}

/* CreateActivators(), CreateDeactivators() */
static byte PressMap_CreateActivators(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_Launchers("ACTIVATORS", "ActivatorPos", "StartAlarm", "Map.ActivateAlarm");
}

static byte PressMap_CreateDeactivators(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_Launchers("DEACTIVATORS", "DeactivatorPos", "EndAlarm", "Map.DeactivateAlarm");
}

/* DeactivateAlarm(ItemName, VehicleName): out of the area: the alarm values back, the activators again */
static byte PressMap_DeactivateAlarm(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t v;
	Native_SetNone(&v);
	(void)out;
	int go = Pr_NoUsrControl(&a[1]);
	if (go <= 0) {
		return go == 0;
	}
	if (!LvN_PutI(MAP, "AlarmState", 0) || !Pr_DelItems("DEACTIVATORS") || !Mode_G(MAP, "PREVALARM", &v)) {
		return 0;
	}
	byte ok = Mode_GVarSet("AlarmFallDelta", &v);
	Native_Free(&v);
	ok = ok && Mode_G(MAP, "PREVGUARANTEEDCHARS", &v) && Mode_GVarSet("GuaranteedChars", &v);
	Native_Free(&v);
	return ok && PORT_LOWN(Map, CreateActivators, NULL, "");
}

/* BrowseAlarmLaunchers(): the dummies of the alarm area (their radius in the name), then the activators */
static byte PressMap_BrowseAlarmLaunchers(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const masks[] = { "DM_PressMap_ActiveAlarm_*", "DM_PressMap_DeactiveAlarm_*" };
	static const char *const lists[] = { "ActivatorPos", "DeactivatorPos" };
	nativeVal_t empty;
	(void)a;
	(void)out;
	if (!Mode_Import("string")) {
		return 0;
	}
	for (int k = 0; k < 2; k++) {
		Native_SetEmpty(&empty, NV_LIST);
		byte ok = Mode_Put(MAP, lists[k], NULL, &empty);
		Native_Free(&empty);
		if (!ok) {
			return 0;
		}
	}
	for (int k = 0; k < 2; k++) {
		Mode_DummyStart(masks[k], 1);
		for (;;) {
			modeVals_t t = { .n = 0 };
			nativeVal_t *dummy = Mode_V(&t), *p = Mode_V(&t), *name = Mode_V(&t), *parts = Mode_V(&t), *rs = Mode_V(&t), *r = Mode_V(&t);
			nativeVal_t *item = Mode_V(&t);
			Mode_DummyNext(dummy);
			if (!Mode_PrintV(dummy)) {
				return Mode_Done(&t, 0);
			}
			if (!Native_True(dummy)) {
				Mode_Done(&t, 1);
				break;
			}
			/* lst.append(x, y, z, r): the old append of several arguments makes the tuple */
			Native_SetEmpty(item, NV_TUPLE);
			byte ok = LvN_Item(dummy, 1, p) && LvN_Item(p, 0, Native_Append(item)) && LvN_Item(p, 1, Native_Append(item)) &&
				LvN_Item(p, 2, Native_Append(item)) && LvN_Item(dummy, 0, name) && Native_Split(name, "_", parts) && LvN_Item(parts, 3, rs) &&
				Native_ToInt(rs, r);
			if (ok) {
				Native_Copy(Native_Append(item), r);
				ok = Mode_Append(MAP, lists[k], item);
			}
			if (!Mode_Done(&t, ok)) {
				return 0;
			}
		}
	}
	return PORT_LOWN(Map, CreateActivators, NULL, "");
}

/* CharConversor.AvailableChars.remove(v) of the list of CharConversor in place (OBJLIB 5) */
static byte Pr_NotAvailable(const char *name) {
	nativeVal_t v, l;
	LvN_Str(&v, name);
	int in = LvN_ModGet("CharConversor", "AvailableChars", &l) ? Native_Contains(&l, &v) : -1;
	Native_Free(&l);
	byte ok = in >= 0;
	if (in > 0) {
		PyObject *o = Script_Attr("CharConversor", "AvailableChars");
		PyObject *x = o ? Native_ToPy(&v) : NULL;
		PyObject *r = x ? Script_Method(o, "remove", "(O)", x) : NULL;
		ok = r != NULL;
		Py_XDECREF(r);
		Py_XDECREF(x);
		Py_XDECREF(o);
	}
	Native_Free(&v);
	return ok;
}

/* InitStatus(LevelPath): an indoor map; the NPCs that come back (fewer in the demo), the elevators, the bureaucracy,
 * the boss' place, the characters the demo cannot convert into */
static byte PressMap_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mods[] = { "Init", "Elevator", "Bureaucracy", "CharsNPC", "CharConversor" };
	static const char *const demo[] = { "Nurse", "Messenger", "Mayor", "PoliceBoss", "BankMaster" };
	static const char *const pcs[][2] = { { "_BankDirector", "BankDirector" }, { "_Bishop", "Bishop" }, { "_Functionary", "Functionary" } };
	static const char *const defs[][2] = { { "Metro", "DM_Door_Metro_Press" }, { "MetroExt", "DM_Door_Metro_Press" },
		{ "Elevator", "DM_Door_Elevator_Press" } };
	nativeVal_t v;
	(void)a;
	(void)out;
	for (int i = 0; i < 5; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	Native_SetInt(&v, 0);
	if (!Mode_SetModVar("Init", "Outdoor", &v) || !LvN_ModGet("Init", "isDemo", &v)) {
		return 0;
	}
	byte isDemo = Native_True(&v);
	Native_Free(&v);
	if (!isDemo) {
		if (!LvN_Repos("Nurse", 3)) {
			return 0;
		}
		int m = LvN_PcOr(MAP, "_Messenger");
		Native_SetInt(&v, 0);
		if (m < 0 || (m ? !LvN_Repos("Messenger", 2) : !Mode_Import("Messenger") || !Mode_SetModVar("Messenger", "MakeMDP", &v))) {
			return 0;
		}
	}
	if (!LvN_Repos("Sentinel", 3) || !LvN_Repos("Police", 5) || !LvN_Repos("Desktop", 3)) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		int go = LvN_PcOr(MAP, pcs[i][0]);
		if (go < 0 || (go && !LvN_Repos(pcs[i][1], 3))) {
			return 0;
		}
	}
	for (int i = 0; i < 3; i++) {
		if (!PORT_CALL(Elevator, SetDef, NULL, "ss", defs[i][0], defs[i][1])) {
			return 0;
		}
	}
	LvN_Str(&v, "DesktopFunctionaryPress");
	byte ok = Mode_SetModVar("Bureaucracy", "BureaucracyTableFile", &v);
	Native_Free(&v);
	if (!ok || !Mode_GVarSetS("BossWorkPlace", "DM_NPC_Spawn_Boss_00") || !LvN_ModGet("Init", "isDemo", &v)) {
		return 0;
	}
	isDemo = Native_True(&v);
	Native_Free(&v);
	for (int i = 0; isDemo && i < 5; i++) {
		if (!Pr_NotAvailable(demo[i])) {
			return 0;
		}
	}
	return 1;
}

/* Init(LevelPath): the elevators and the metro, the boss room after the mental controller, the fake traffic, the
 * alarm area, the bureau gone when it was taken */
static byte PressMap_Init(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const light[] = { "TLight1B", "TLight2B", "TLight3B", NULL }, *const none[] = { NULL };
	static const char *const tel[] = { "DM_Door_Teleporter_07", "DM_Door_Teleporter_08" };
	nativeVal_t l1, l2, b, one;
	(void)a;
	(void)out;
	if (!Mode_Import("Metro") || !Mode_Import("Elevator") || !Mode_Import("GateKeeper") || !Mode_Connect("DM_Door_Elevator_Press", "DownTown", 0)) {
		return 0;
	}
	Mode_Texts(&l1, tel, 2);
	Native_SetEmpty(&l2, NV_LIST);
	byte ok = PORT_CALL(Elevator, ConnectElevators, NULL, "vv", &l1, &l2);
	Native_Free(&l1);
	Native_Free(&l2);
	if (!ok || !PORT_CALL(Metro, SetAsMetro, NULL, "s", "DM_Door_Metro_Press") ||
		(LvN_SaveIs("Press.MentalControllerStatus", "1") && !Pr_BossDoor("NoOneGroupGateKeeper", "GateKeeper.UseDoor", NULL)) ||
		!Mode_Import("InTraffic") || !LvN_InTraffic("N_TrafficList", light, -1, "N_NumTraffic", 30) || !LvN_InTraffic("M_TrafficList", none, -1, "M_NumTraffic", 0) ||
		!LvN_InTraffic("H_TrafficList", none, -1, "H_NumTraffic", 0) || !PORT_CALL(InTraffic, LoadFakeTraffic, NULL, "") ||
		!PORT_CALL(InTraffic, FakeTraffic, NULL, "") || !PORT_LOWN(Map, BrowseAlarmLaunchers, NULL, "")) {
		return 0;
	}
	Mode_SaveVar("MapPress_BureauExists", &b);
	LvN_Str(&one, "1");
	int hide = Mode_Cmp(&b, "!=", &one);
	Native_Free(&b);
	Native_Free(&one);
	return hide >= 0 && (!hide || PORT_LOWN(Map, HideBureau, NULL, ""));
}

/* HideBureau(): the bureau hidden; its position */
static byte PressMap_HideBureau(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t b;
	(void)a;
	if (!LvN_Entity("DM_Door_Bureau_00", &b)) {
		return 0;
	}
	byte ok = LvN_SetI(&b, "MapCollision", 0) && LvN_SetI(&b, "Active", 0) && Mode_VGet(&b, "Pos", out);
	Native_Free(&b);
	return ok;
}

LV_FN(PressMap, RemoteMsgA, 1, PressMap_RemoteMsg)
LV_FN(PressMap, ForbiddenMsgA, 0, PressMap_ForbiddenMsg)
LV_FN(PressMap, StartAlarmatorA, 0, PressMap_StartAlarmator)
LV_FN(PressMap, ActivateAlarmA, 2, PressMap_ActivateAlarm)
LV_FN(PressMap, CreateActivatorsA, 0, PressMap_CreateActivators)
LV_FN(PressMap, CreateDeactivatorsA, 0, PressMap_CreateDeactivators)
LV_FN(PressMap, DeactivateAlarmA, 2, PressMap_DeactivateAlarm)
LV_FN(PressMap, BrowseAlarmLaunchersA, 0, PressMap_BrowseAlarmLaunchers)
LV_FN(PressMap, InitStatusA, 1, PressMap_InitStatus)
LV_FN(PressMap, InitA, 1, PressMap_Init)
LV_FN(PressMap, HideBureauA, 0, PressMap_HideBureau)

PyMethodDef g_PressMapMethods[] = {
	{ "RemoteMsg", PressMap_RemoteMsgA, METH_VARARGS, NULL },
	{ "ForbiddenMsg", PressMap_ForbiddenMsgA, METH_VARARGS, NULL },
	{ "StartAlarmator", PressMap_StartAlarmatorA, METH_VARARGS, NULL },
	{ "ActivateAlarm", PressMap_ActivateAlarmA, METH_VARARGS, NULL },
	{ "CreateActivators", PressMap_CreateActivatorsA, METH_VARARGS, NULL },
	{ "CreateDeactivators", PressMap_CreateDeactivatorsA, METH_VARARGS, NULL },
	{ "DeactivateAlarm", PressMap_DeactivateAlarmA, METH_VARARGS, NULL },
	{ "BrowseAlarmLaunchers", PressMap_BrowseAlarmLaunchersA, METH_VARARGS, NULL },
	{ "InitStatus", PressMap_InitStatusA, METH_VARARGS, NULL },
	{ "Init", PressMap_InitA, METH_VARARGS, NULL },
	{ "HideBureau", PressMap_HideBureauA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapPlanes ---- */

/* Init(LevelPath) */
static PyObject *PressPlanes_Init(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_PlanesInit(args, MPL);
}

/* TakenPhoto(Name) */
static PyObject *PressPlanes_TakenPhoto(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_PlanesPhoto(args, MPL, "PressBlueprints.Ships", "PressBlueprints.Engines");
}

PyMethodDef g_PressMapPlanesMethods[] = {
	{ "Init", PressPlanes_Init, METH_VARARGS, NULL },
	{ "TakenPhoto", PressPlanes_TakenPhoto, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapDemoClose ---- */

/* Init(LevelPath): the demo: the metro only to Outskirts, the elevator closed */
static byte DemoClose_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t lv;
	Native_SetNone(&lv);
	(void)a;
	(void)out;
	if (!Mode_EntSetS("DM_Door_Metro_Press", "UseFunc", "MapDemoClose.OnMetroUsed") || !LvN_Entity("DM_Door_Elevator_Press", &lv)) {
		return 0;
	}
	byte ok = LvN_SetS(&lv, "UseFunc", "") && LvN_SetS(&lv, "OnObjSlot", "!CloseDoorSlot");
	Native_Free(&lv);
	return ok;
}

/* OnMetroUsed(UsrName, MetroName, optId, FullOption): a driver takes the metro to Outskirts, others cannot */
static byte DemoClose_OnMetroUsed(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, ctrl, list, text;
	(void)out;
	Native_SetNone(&usr);
	Native_SetNone(&ctrl);
	Native_SetNone(&list);
	int drives = Mode_UsrHandle(&usr) && Mode_VGet(&usr, "ActCtrl", &ctrl) && LvN_ModGet("Chars", "CanDriveList", &list) ?
		Native_Contains(&list, &ctrl) : -1;
	Native_Free(&usr);
	Native_Free(&ctrl);
	Native_Free(&list);
	if (drives > 0) {
		return PORT_CALL(Elevator, UseElevator, NULL, "vvsv", &a[0], &a[1], "MoveLevel Outskirts", &a[3]) &&
			PORT_CALL(Metro, SetUsrParams, NULL, "v", &a[0]);
	}
	if (drives < 0) {
		return 0;
	}
	Mode_Lang("Station_CantGo", &text);
	byte ok = PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", &text);
	Native_Free(&text);
	return ok;
}

LV_FN(DemoClose, InitA, 1, DemoClose_Init)
LV_FN(DemoClose, OnMetroUsedA, 4, DemoClose_OnMetroUsed)

PyMethodDef g_PressMapDemoCloseMethods[] = {
	{ "Init", DemoClose_InitA, METH_VARARGS, NULL },
	{ "OnMetroUsed", DemoClose_OnMetroUsedA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapSnd ---- */

/* SSound.LoadSound(file) through its seam (the cases record it) */
static byte Pr_LoadSound(const char *file) {
	nativeVal_t v;
	LvN_Str(&v, file);
	byte ok = Mode_LoadSoundSeam(&v);
	Native_Free(&v);
	return ok;
}

/* Scrap.Get("IsXbox") as a condition: 1, 0, -1 */
static int Pr_Xbox(void) {
	nativeVal_t x;
	int r = Native_GVarGet("IsXbox", &x) ? Native_True(&x) : -1;
	Native_Free(&x);
	return r;
}

/* LoadSounds(): the sounds of the map (three computers only on the PC) */
static byte PressSnd_LoadSounds(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const first[] = { "light1.wav", "ambience1.wav", "Atmosfera-Press1.wav", "Atmosfera-Press2.wav", "datos-good1.wav",
		"computer1.wav", NULL };
	static const char *const pc[] = { "computer2.wav", "computer3.wav", "computer4.wav", NULL };
	static const char *const last[] = { "FXChispas.wav", "gas4.wav", "gas5.wav", NULL };
	(void)a;
	(void)out;
	for (int i = 0; first[i]; i++) {
		if (!Pr_LoadSound(first[i])) {
			return 0;
		}
	}
	int x = Pr_Xbox();
	for (int i = 0; x == 0 && pc[i]; i++) {
		if (!Pr_LoadSound(pc[i])) {
			return 0;
		}
	}
	for (int i = 0; x >= 0 && last[i]; i++) {
		if (!Pr_LoadSound(last[i])) {
			return 0;
		}
	}
	return x >= 0;
}

/* a point sound of SetSounds: Sound.SetPSLoop(name, pos, snd, Volume, Pitch, AttenIni, AttenEnd, Doppler=0.0) or
 * Sound.SetPSInterval(name, pos, snd, MinInterval, MaxInterval, Volume, ...); comp: snd is one of the computer
 * sounds, all "Computer1" on the Xbox */
typedef struct {
	const char *name;
	double pos[3];
	const char *snd;
	byte comp;
	byte interval;
	double min, max, vol, pitch, ini, end;
} prSnd_t;

static const prSnd_t press_snd_base[] = {
	{ "Press1", { -1724.50708008, -4612.3984375, -318.820251465 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 4356.0, 6996.0 },
	{ "Press2", { 2813.55834961, -1771.67333984, -1232.49645996 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 772.0, 3412.0 },
	{ "Press3", { -5641.73779297, -858.944274902, -685.887329102 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 772.0, 3412.0 },
	{ "Press4", { -4932.39892578, -1194.26245117, 2200.28955078 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 772.0, 2388.0 },
	{ "Press5", { -4640.32275391, -921.339904785, 3386.58959961 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 772.0, 2388.0 },
	{ "Press6", { -2001.66064453, -1162.64160156, 4314.89257813 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 708.0, 2388.0 },
	{ "Press7", { 26.5417785645, -1191.02685547, 4224.77392578 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 820.0, 2388.0 },
	{ "Press8", { 2177.04760742, -630.110534668, 5628.82763672 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 3076.0, 6676.0 },
	{ "Press9", { 5365.44287109, -307.341186523, 5406.49707031 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 1284.0, 4308.0 },
	{ "Press10", { 6985.42529297, 688.624694824, 7049.59179688 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 772.0, 4884.0 },
	{ "Press11", { 1755.46154785, 424.879577637, 923.08190918 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 928.0, 2764.0 },
	{ "Press12", { 2014.75830078, 491.506713867, -727.861328125 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 512.0, 2508.0 },
	{ "Press13", { -1838.99169922, 448.17590332, 3384.20214844 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 772.0, 2388.0 },
	{ "Press14", { -1747.12927246, -9.12878417969, 8004.51416016 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 1412.0, 3092.0 },
	{ "Press15", { -2620.16870117, 183.615692139, 5290.62109375 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 1284.0, 4372.0 },
	{ "Press16", { 4877.22216797, 490.537506104, 3099.77783203 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 1412.0, 2772.0 },
	{ "Press17", { -4003.81396484, 343.673370361, 2491.84350586 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 1476.0, 2772.0 },
	{ "Press18", { -1606.63562012, 950.017578125, -415.267150879 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 2052.0, 5204.0 },
	{ "Press19", { 4866.41552734, -6.58361816406, 113.616073608 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 1284.0, 3108.0 },
	{ "Press20", { 1892.41833496, 221.046508789, 10275.6386719 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 772.0, 2388.0 },
	{ "Press21", { 7966.37890625, -1976.3392334, 8224.44042969 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 1540.0, 2388.0 },
	{ "Press22", { -3916.1640625, -250.411422729, 10314.0117188 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.55, -2.0, 1540.0, 3476.0 },
	{ "Press23", { -3916.1640625, -250.411422729, 10314.0117188 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.75, -4.0, 1540.0, 3476.0 },
	{ "Press24", { -5357.41943359, -523.968444824, 7982.35986328 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.75, -4.0, 1540.0, 3476.0 },
	{ "Press25", { -5357.41943359, -523.968444824, 7982.35986328 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.55, -2.0, 1540.0, 3476.0 },
	{ "Press26", { -5448.22314453, -973.464111328, 5425.13232422 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.75, -4.0, 1540.0, 3000.0 },
	{ "Press27", { -5448.22314453, -973.464111328, 5425.13232422 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.55, -2.0, 1540.0, 3000.0 },
	{ "Press28", { 2167.46166992, -1384.32763672, 14632.1953125 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.55, -2.0, 3076.0, 6676.0 },
	{ "Press29", { 2167.46166992, -1384.32763672, 14632.1953125 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.75, -4.0, 3076.0, 6676.0 },
	{ "Press30", { 7281.4453125, -1455.22497559, 12845.0859375 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.55, -2.0, 4356.0, 6676.0 },
	{ "Press31", { 7281.4453125, -1455.22497559, 12845.0859375 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.75, -4.0, 4356.0, 6676.0 },
	{ "Press32", { -759.339355469, 23863.0390625, 7228.90283203 }, "ambience1", 0, 0, 0, 0, 0.19, -40.0, 3588.0, 5316.0 },
	{ "Press33", { -2657.54589844, 19646.6015625, 6079.54980469 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.55, -2.0, 1924.0, 3796.0 },
	{ "Press34", { -2657.54589844, 19646.6015625, 6079.54980469 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.75, -4.0, 1924.0, 3796.0 },
	{ "Press35", { -8.59783935547, 19828.8144531, 3956.32373047 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.55, -2.0, 1924.0, 4436.0 },
	{ "Press36", { -8.59783935547, 19828.8144531, 3956.32373047 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.75, -4.0, 1924.0, 4436.0 },
	{ "Press37", { 3123.93408203, 19849.5390625, 4951.98095703 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.55, -2.0, 1924.0, 4436.0 },
	{ "Press38", { 3123.93408203, 19849.5390625, 4951.98095703 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.75, -2.0, 1924.0, 4436.0 },
	{ "Press39", { 3743.77197266, 19818.7617188, 8476.9609375 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.75, -2.0, 1924.0, 4436.0 },
	{ "Press40", { 3743.77197266, 19818.7617188, 8476.9609375 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.55, -4.0, 1924.0, 4436.0 },
	{ "Press41", { 2237.00170898, 129.727722168, -2396.54736328 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.75, -2.0, 964.0, 2452.0 },
	{ "Press42", { 2237.00170898, 129.727722168, -2396.54736328 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.55, -4.0, 964.0, 2452.0 },
	{ "Press43", { 3446.05322266, 277.288116455, -2387.71118164 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.75, -2.0, 964.0, 2452.0 },
	{ "Press44", { 3446.05322266, 277.288116455, -2387.71118164 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.55, -4.0, 964.0, 2452.0 },
	{ "Press45", { 4996.72558594, 329.111419678, -2400.4987793 }, "Atmosfera-Press1", 0, 0, 0, 0, 0.75, -2.0, 1028.0, 2516.0 },
	{ "Press46", { 4996.72558594, 329.111419678, -2400.4987793 }, "Atmosfera-Press2", 0, 0, 0, 0, 0.55, -4.0, 1028.0, 2516.0 },
	{ NULL },
};

static const prSnd_t press_snd_half[] = {
	{ "Datos1", { 8003.546875, -925.107299805, 11001.7568359 }, "datos-good1", 0, 0, 0, 0, 0.08, 0.0, 900.0, 4724.0 },
	{ "Datos2", { 9553.35058594, -816.703125, 12890.1943359 }, "datos-good1", 0, 0, 0, 0, 0.08, 0.0, 900.0, 4724.0 },
	{ "Computer1", { 6703.66894531, -937.297668457, 10949.5878906 }, "Computer1", 1, 1, 1.0, 15.0, 0.1, 0.0, 0.0, 0.0 },
	{ "Computer2", { 8032.22265625, -1091.76660156, 10923.9111328 }, "Computer2", 1, 1, 1.0, 18.0, 0.10000000149, 0.0, 0.0, 0.0 },
	{ "Computer3", { 8948.77734375, -1090.01342773, 11492.5488281 }, "Computer3", 1, 1, 1.0, 12.0, 0.10000000149, 0.0, 0.0, 0.0 },
	{ "Computer4", { 9146.15234375, -1128.93469238, 12799.1591797 }, "Computer4", 1, 1, 1.0, 10.0, 0.10000000149, 0.0, 0.0, 0.0 },
	{ "Computer5", { 9103.68457031, -1144.51220703, 11329.7851563 }, "Computer1", 1, 1, 1.0, 16.0, 0.10000000149, 0.0, 0.0, 0.0 },
	{ "Computer6", { 9379.03613281, -1245.40515137, 12820.5996094 }, "Computer2", 1, 1, 1.0, 17.0, 0.10000000149, 0.0, 0.0, 0.0 },
	{ NULL },
};

static const prSnd_t press_snd_full[] = {
	{ "Datos3", { 8274.88769531, -1105.56201172, 14544.3408203 }, "datos-good1", 0, 0, 0, 0, 0.1, 0.0, 900.0, 4724.0 },
	{ "Datos4", { 8274.88769531, -1105.56201172, 14544.3408203 }, "datos-good1", 0, 0, 0, 0, 0.1, 0.0, 900.0, 4724.0 },
	{ "Datos5", { 2208.61474609, -956.398742676, 15136.1455078 }, "datos-good1", 0, 0, 0, 0, 0.1, 0.0, 900.0, 4724.0 },
	{ "Computer7", { 8956.21679688, -1152.52087402, 13966.1582031 }, "Computer3", 1, 1, 1.0, 15.0, 0.10000000149, 0.0, 0.0, 0.0 },
	{ "Computer8", { 7365.1796875, -1216.16259766, 15078.2626953 }, "Computer4", 1, 1, 1.0, 20.0, 0.10000000149, 0.0, 0.0, 0.0 },
	{ "Computer9", { 4142.25390625, -1274.63818359, 15452.1757813 }, "Computer1", 1, 1, 1.0, 12.0, 0.10000000149, 0.0, 0.0, 0.0 },
	{ "Computer10", { 2919.67114258, -1211.56018066, 15364.8652344 }, "Computer2", 1, 1, 1.0, 14.0, 0.10000000149, 0.0, 0.0, 0.0 },
	{ "Computer11", { 1232.42492676, -1172.45019531, 15280.9521484 }, "Computer3", 1, 1, 1.0, 16.0, 0.10000000149, 0.0, 0.0, 0.0 },
	{ NULL },
};

static const prSnd_t press_snd_broken[] = {
	{ "Humo1", { 8956.21679688, -1152.52087402, 13966.1582031 }, "gas4", 0, 1, 1.0, 10.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo2", { 8956.21679688, -1152.52087402, 13966.1582031 }, "gas5", 0, 1, 1.0, 5.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo3", { 7365.1796875, -1216.16259766, 15078.2626953 }, "gas4", 0, 1, 1.0, 10.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo4", { 7365.1796875, -1216.16259766, 15078.2626953 }, "gas5", 0, 1, 1.0, 5.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo5", { 4142.25390625, -1274.63818359, 15452.1757813 }, "gas4", 0, 1, 1.0, 5.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo6", { 4142.25390625, -1274.63818359, 15452.1757813 }, "gas5", 0, 1, 1.0, 10.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo7", { 2919.67114258, -1211.56018066, 15364.8652344 }, "gas4", 0, 1, 1.0, 9.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo8", { 2919.67114258, -1211.56018066, 15364.8652344 }, "gas5", 0, 1, 1.0, 4.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo9", { 1232.42492676, -1172.45019531, 15280.9521484 }, "gas4", 0, 1, 1.0, 11.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo10", { 1232.42492676, -1172.45019531, 15280.9521484 }, "gas5", 0, 1, 1.0, 5.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo11", { 6703.66894531, -937.297668457, 10949.5878906 }, "gas4", 0, 1, 1.0, 10.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo12", { 6703.66894531, -937.297668457, 10949.5878906 }, "gas5", 0, 1, 1.0, 5.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo13", { 8032.22265625, -1091.76660156, 10923.9111328 }, "gas5", 0, 1, 1.0, 14.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo14", { 8032.22265625, -1091.76660156, 10923.9111328 }, "gas4", 0, 1, 1.0, 9.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo15", { 8948.77734375, -1090.01342773, 11492.5488281 }, "gas5", 0, 1, 1.0, 7.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo16", { 8948.77734375, -1090.01342773, 11492.5488281 }, "gas4", 0, 1, 1.0, 3.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo17", { 9146.15234375, -1128.93469238, 12799.1591797 }, "gas5", 0, 1, 1.0, 10.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo18", { 9146.15234375, -1128.93469238, 12799.1591797 }, "gas4", 0, 1, 1.0, 5.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo19", { 9103.68457031, -1144.51220703, 11329.7851563 }, "gas5", 0, 1, 1.0, 11.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo20", { 9103.68457031, -1144.51220703, 11329.7851563 }, "gas4", 0, 1, 1.0, 5.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo21", { 9379.03613281, -1245.40515137, 12820.5996094 }, "gas5", 0, 1, 1.0, 10.0, 0.03, 0.0, 0.0, 0.0 },
	{ "Humo22", { 9379.03613281, -1245.40515137, 12820.5996094 }, "gas4", 0, 1, 1.0, 5.0, 0.03, 0.0, 0.0, 0.0 },
	{ NULL },
};

/* the keyword calls of Sound.SetPSInterval and SetPSLoop (their entries take no keyword slots, so they go the slow
 * way with the keyword dict) */
static byte Pr_Sounds(const prSnd_t *t, int xbox) {
	for (; t->name; t++) {
		nativeVal_t args[3];
		LvN_Str(&args[0], t->name);
		Native_SetVec(&args[1], t->pos[0], t->pos[1], t->pos[2]);
		LvN_Str(&args[2], t->comp && xbox ? "Computer1" : t->snd);
		byte ok = t->interval ? PORT_CALL(Sound, SetPSInterval, NULL,
			"vvv;MinInterval=d,MaxInterval=d,Volume=d,Pitch=d,AttenIni=d,AttenEnd=d,Doppler=d", &args[0], &args[1], &args[2], t->min,
			t->max, t->vol, t->pitch, t->ini, t->end, 0.0) :
			PORT_CALL(Sound, SetPSLoop, NULL, "vvv;Volume=d,Pitch=d,AttenIni=d,AttenEnd=d,Doppler=d", &args[0], &args[1], &args[2],
			t->vol, t->pitch, t->ini, t->end, 0.0);
		Native_Free(&args[0]);
		Native_Free(&args[2]);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* the save variable is one of the texts (they end with NULL) */
static byte Pr_SaveIn(const nativeVal_t *v, const char *const *texts) {
	for (; *texts; texts++) {
		if (LvN_EqS(v, *texts)) {
			return 1;
		}
	}
	return 0;
}

/* SetSounds(): the elements, the ambience of the press, the mental controller by the act of the game: working at
 * half or full speed, or broken with smoke and sparks */
static byte PressSnd_SetSounds(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const half[] = { "2ndMurder", "3rdMurder", "Epilogue", NULL };
	static const char *const full[] = { "3rdMurder", "Epilogue", NULL };
	nativeVal_t act, mcs;
	(void)a;
	(void)out;
	/* Scrap.CallElements by name: fc-r7b-pr1 records it (OBJLIB 7) */
	if (!Mode_Import("Sound") || !LV_CALL("Scrap", "CallElements", NULL, "s", "MapSnd.SndElements")) {
		return 0;
	}
	int xbox = Pr_Xbox();
	if (xbox < 0 || !Pr_Sounds(press_snd_base, xbox)) {
		return 0;
	}
	Mode_SaveVar("GameAct", &act);
	Mode_SaveVar("Press.MentalControllerStatus", &mcs);
	byte broken = LvN_EqS(&act, "Epilogue") && LvN_EqS(&mcs, "1"), ok;
	if (!broken) {
		ok = !Pr_SaveIn(&act, half) || (Pr_Sounds(press_snd_half, xbox) && (!Pr_SaveIn(&act, full) || Pr_Sounds(press_snd_full, xbox)));
	} else {
		ok = Mode_GVarSetS("FXElementsMentalControllerDamageSound", "FXChispas") && Mode_GVarSetN("FXElementsMentalControllerDamageSoundVol", 0.03) &&
			Pr_Sounds(press_snd_broken, xbox);
	}
	Native_Free(&act);
	Native_Free(&mcs);
	return ok;
}

/* the call lines of News1 (Mode_Calls: by name, OBJLIB 4) */
static const char *const news1_sounds[] = {
	"Scene.AddStream|0|musica-betty2|0.6",
	"Scene.AddSound|233|sweep-encendido|0.3|0|0",
	"Scene.AddStream|365|mov-cam|0.3",
	"Scene.AddSound|709|sweep-encendido|0.3|0|0",
	"Scene.AddSound|744|sweep-apagado|0.3|0|0",
	"Scene.AddStream|860|golpe-betty2|0.6",
	NULL,
};

/* News1(): the sounds of Betty's news of the archbishop's murder */
static byte PressSnd_News1(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t line;
	(void)a;
	(void)out;
	LvN_Str(&line, "-----------------------------");
	byte ok = Mode_Import("Scene") && Mode_PrintV(&line);
	Native_Free(&line);
	return ok && Pr_LoadSound("News1/sweep-apagado.wav") && Pr_LoadSound("News1/sweep-encendido.wav") && Mode_Calls("MapSnd", news1_sounds);
}

LV_FN(PressSnd, LoadSoundsA, 0, PressSnd_LoadSounds)
LV_FN(PressSnd, SetSoundsA, 0, PressSnd_SetSounds)
LV_FN(PressSnd, News1A, 0, PressSnd_News1)

PyMethodDef g_PressMapSndMethods[] = {
	{ "LoadSounds", PressSnd_LoadSoundsA, METH_VARARGS, NULL },
	{ "SetSounds", PressSnd_SetSoundsA, METH_VARARGS, NULL },
	{ "News1", PressSnd_News1A, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_FicusBanker ---- */

/* InitStatus(LevelPath): Ficus and Scrapus as mission characters */
static byte FicusBanker_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LV_MISSIONCHAR("Police_Ficus", Mission_FicusBanker, CreatePoliceFicusInit) &&
		LV_MISSIONCHAR("BankDirector_Scrapus", Mission_FicusBanker, CreateBankDirectorScrapusInit);
}

/* Init(LevelPath): the check's model; the mission goes on where it was saved */
static byte FicusBanker_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t svv, m, pos;
	byte ok = 1;
	(void)a;
	(void)out;
	if (!Mode_Import("Elevator")) {
		return 0;
	}
	Mode_SaveVar("Mission_Police_FicusBanker", &svv);
	if (!Mode_G(FB, "Model_Check", &m) || !Mode_PreloadOnes(&m)) {
		Native_Free(&svv);
		Native_Free(&m);
		return 0;
	}
	Native_Free(&m);
	if (LvN_EqS(&svv, "TalkFicus")) {
		ok = Mode_Target("Police_Ficus");
	} else if (LvN_EqS(&svv, "KillScrapus")) {
		ok = PORT_LOWN(Mission_FicusBanker, InitKillScrapus, NULL, "");
	} else if (LvN_EqS(&svv, "TakeCheck")) {
		/* SpawnCheck("BankDirector_Scrapus", eval(Scrap.GetSaveVar("CheckPos"))); the global is looked up after the
		 * eval (the script looks it up first: the same unless both fail) */
		Native_SetNone(&pos);
		ok = LvN_SaveList("CheckPos", FB, &pos) && PORT_LOWN(Mission_FicusBanker, SpawnCheck, NULL, "sv", "BankDirector_Scrapus", &pos);
		Native_Free(&pos);
	} else if (LvN_EqS(&svv, "TalkFicus2")) {
		ok = LV_OWN(FB, "InitTalkFicus2", NULL, "");
	}
	Native_Free(&svv);
	return ok;
}

/* CreatePoliceFicusInit(e): Ficus talks the mission and stays where he is */
static byte FicusBanker_CreatePoliceFicusInit(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_SetS(&a[0], "UseFunc", "Mission_FicusBanker.FicusMissionSpeech") && LvN_Stay(&a[0], 6, LvN_Empty()) &&
		LvN_SetLang(&a[0], "Descriptor", "CharName_Ficus");
}

/* FicusMissionSpeech(UsrName, CharName, optId, FullOption): Ficus' talks for the state of the mission */
static byte FicusBanker_FicusMissionSpeech(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const ans[] = { "Mission_TalkToFicus_Ficus_1" };
	nativeVal_t usr, svv, first;
	byte ok;
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	Native_SetNone(&usr);
	int dt = Mode_UsrHandle(&usr) ? LvN_CtrlIs(&usr, "Dtritus") : -1;
	Native_Free(&usr);
	if (dt <= 0) {
		return dt == 0 && Mode_Spch("Mission_TalkToFicus_Ficus_InitMsg2", "Mission_TalkToFicus_Ficus_InitAns2", NULL);
	}
	Mode_SaveVar("Mission_Police_FicusBanker", &svv);
	byte talk = LvN_EqS(&a[2], "Talk");
	if (LvN_EqS(&svv, "TalkFicus")) {
		if (!Mode_G(FB, "Firstone", &first)) {
			ok = 0;
		} else if (LvN_EqI(&first, 0)) {
			ok = Mode_NISpeech(NULL, "Mission_TalkToFicus_Ficus_NI_1Msg") && Mode_AnsLst(ans, 1) && LvN_PutI(FB, "Firstone", 1);
		} else if (LvN_EqI(&first, 1)) {
			ok = LV_ONEND(Mission_FicusBanker, InitKillScrapus, NULL) &&
				Mode_Spch("Mission_TalkToFicus_Ficus_1Msg", "Mission_TalkToFicus_Ficus_1Ans", NULL);
		} else {
			ok = LvN_RoutineCall("Police", a);
		}
		Native_Free(&first);
	} else if (LvN_EqS(&svv, "KillScrapus")) {
		ok = talk || LvN_EqS(&a[2], "Mission_TalkToFicus_Ficus_1") ?
			Mode_Spch("Mission_KillScrapus_Ficus_InitMsg1", "Mission_KillScrapus_Ficus_InitAns1", NULL) : LvN_RoutineCall("Police", a);
	} else if (LvN_EqS(&svv, "TalkFicus2")) {
		ok = talk ? LV_ONEND(Mission_FicusBanker, SetNextMission, NULL) &&
			Mode_Spch("Mission_TalkToFicus2_Ficus_InitMsg1", "Mission_TalkToFicus2_Ficus_InitAns1", NULL) :
			LvN_RoutineCall("Police", a);
	} else {
		ok = talk ? Mode_Spch("Mission_TalkToFicus2_Ficus_InitMsg2", "Mission_TalkToFicus2_Ficus_InitAns2", NULL) : LvN_RoutineCall("Police", a);
	}
	Native_Free(&svv);
	return ok;
}

/* CreateBankDirectorScrapusInit(e) */
static byte FicusBanker_CreateBankDirectorScrapusInit(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_SetS(&a[0], "UseFunc", "Mission_FicusBanker.ScrapusMissionSpeech") && LvN_SetLang(&a[0], "Descriptor", "CharName_Scrapus");
}

/* ScrapusMissionSpeech(UsrName, CharName, optId, FullOption) */
static byte FicusBanker_ScrapusMissionSpeech(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t check;
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	int kill = LvN_SaveIs("Mission_Police_FicusBanker", "KillScrapus");
	if (kill) {
		kill = LvN_Entity("MissionCheck", &check) ? !Native_True(&check) : -1;
		Native_Free(&check);
	}
	if (kill > 0) {
		return Mode_Spch("Mission_KillScrapus_Scrapus_InitMsg1", "Mission_KillScrapus_Scrapus_InitAns1", NULL);
	}
	return kill == 0 && LvN_RoutineCall("BankDirector", a);
}

/* module.var = SpawnCheck when it is None, else the console line of its override */
static byte Fb_Hook(const char *module, const char *var, const char *msg) {
	nativeVal_t v, none, m;
	Native_SetNone(&none);
	int free = LvN_ModGet(module, var, &v) ? Mode_Cmp(&v, "==", &none) : -1;
	Native_Free(&v);
	if (free > 0) {
		return PR_HOOK(module, var, FB, "SpawnCheck");
	}
	if (free < 0) {
		return 0;
	}
	LvN_Str(&m, msg);
	byte ok = Mode_ConsoleError(&m);
	Native_Free(&m);
	return ok;
}

/* InitKillScrapus(): Scrapus is the target; his death or possession drops the check */
static byte FicusBanker_InitKillScrapus(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	SaveVar_Set("Mission_Police_FicusBanker", "KillScrapus");
	return Mode_Target("BankDirector_Scrapus") && Mode_MMDesc("Mission_KillScrapus_MissionText1", 1) && Mode_WayPoint() &&
		Fb_Hook("CharAct", "onNPCDeath", "CharAct.onNPCDeath already overrrided \n") &&
		Fb_Hook("CharConversor", "OnPossession", "CharConversor.OnPossession already overrrided \n");
}

/* SpawnCheck(name, pos): Scrapus dies or is possessed: his check appears at pos with its arrow */
static byte FicusBanker_SpawnCheck(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = Mode_V(&t), *m = Mode_V(&t), *l2 = Mode_V(&t), *cam = Mode_V(&t);
	(void)out;
	if (!LvN_EqS(&a[0], "BankDirector_Scrapus")) {
		return Mode_Done(&t, 1);
	}
	SaveVar_Set("Mission_Police_FicusBanker", "TakeCheck");
	LvN_SaveRepr("CheckPos", &a[1]);
	Mode_Ints3(cam, 0, 60, 0);
	byte ok = LvN_ModNone("CharConversor", "OnPossession") && LvN_ModNone("CharAct", "onNPCDeath") &&
		LvN_CreateAt(Mode_S(&t, "MissionCheck"), &a[1], "Item", l) && Mode_G(FB, "Model_Check", m) && Mode_VModel(l, m) &&
		LvN_SetI(l, "Radius", 60) && LvN_SetI(l, "AppearsIfFree", 0) && LvN_SetI(l, "AutoFace", 0) && LvN_SetI(l, "OnlyUser", 1) &&
		LvN_SetF(l, "rTime", 0.25) && LvN_SetS(l, "OnTake", "Mission_FicusBanker.TakeCheck") &&
		LvN_CreateAt(Mode_S(&t, "MissionCheckArrow"), &a[1], "WithLife", l2) && Mode_VSet(l2, "CamTgt", cam) && LvN_TargetName(l2);
	return Mode_Done(&t, ok);
}

/* a taken item: it.rTime = 20 and it.Del in 5 s */
static byte Pr_TakenDel(const nativeVal_t *it) {
	nativeVal_t time;
	if (!LvN_SetI(it, "rTime", 20)) {
		return 0;
	}
	Native_SetFloat(&time, Mode_Time() + 5);
	/* OBJLIB 4: the scheduler keeps the bound method */
	PyObject *del = Mode_VBound(it, "Del");
	byte ok = del && Mode_Schedule(&time, del, NULL, 0, NULL);
	Py_XDECREF(del);
	return ok;
}

/* the take sound Sound.ItemTake[key] at the item */
static byte Pr_TakenSound(const nativeVal_t *it, const char *key) {
	nativeVal_t tab, k, a, s, v, pos;
	Native_SetNone(&a);
	Native_SetNone(&s);
	Native_SetNone(&v);
	Native_SetNone(&pos);
	LvN_Str(&k, key);
	byte ok = LvN_ModGet("Sound", "ItemTake", &tab) && Native_Subscript(&tab, &k, &a) && Mode_VGet(it, "Pos", &pos) && LvN_Item(&a, 0, &s) &&
		LvN_Item(&a, 1, &v) && LvN_PlaySound(&pos, &s, &v);
	Native_Free(&tab);
	Native_Free(&k);
	Native_Free(&a);
	Native_Free(&s);
	Native_Free(&v);
	Native_Free(&pos);
	return ok;
}

/* TakeCheck(ItemName, UsrName): the check taken: back to Ficus */
static byte FicusBanker_TakeCheck(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t it, arrow;
	Native_SetNone(&it);
	(void)a;
	(void)out;
	if (!Mode_Import("Sound") || !LV_OWN(FB, "InitTalkFicus2", NULL, "") || !LvN_Entity("MissionCheck", &it)) {
		return 0;
	}
	Native_SetNone(&arrow);
	byte ok = Pr_TakenDel(&it) && LvN_Entity("MissionCheckArrow", &arrow) && Mode_VDel(&arrow) && Pr_TakenSound(&it, "MoneyItem");
	Native_Free(&it);
	Native_Free(&arrow);
	return ok;
}

/* SetNextMission(): the race of Multiplus in DownTown is next */
static byte FicusBanker_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Target(NULL) && Mode_ApplyNext(FB) && Mode_MMDesc("Mission_WinRaceMultiplus_MissionText1", 1) &&
		Mode_DelSaveVars("Mission_Police_FicusBanker") && Mode_DelSaveVars("CheckPos");
}

LV_FN(FicusBanker, InitStatusA, 1, FicusBanker_InitStatus)
LV_FN(FicusBanker, InitA, 1, FicusBanker_Init)
LV_FN(FicusBanker, CreatePoliceFicusInitA, 1, FicusBanker_CreatePoliceFicusInit)
LV_FN(FicusBanker, FicusMissionSpeechA, 4, FicusBanker_FicusMissionSpeech)
LV_FN(FicusBanker, CreateBankDirectorScrapusInitA, 1, FicusBanker_CreateBankDirectorScrapusInit)
LV_FN(FicusBanker, ScrapusMissionSpeechA, 4, FicusBanker_ScrapusMissionSpeech)
LV_FN(FicusBanker, InitKillScrapusA, 0, FicusBanker_InitKillScrapus)
LV_FN(FicusBanker, SpawnCheckA, 2, FicusBanker_SpawnCheck)
LV_FN(FicusBanker, TakeCheckA, 2, FicusBanker_TakeCheck)
LV_FN(FicusBanker, SetNextMissionA, 0, FicusBanker_SetNextMission)

PyMethodDef g_PressFicusBankerMethods[] = {
	{ "InitStatus", FicusBanker_InitStatusA, METH_VARARGS, NULL },
	{ "Init", FicusBanker_InitA, METH_VARARGS, NULL },
	{ "CreatePoliceFicusInit", FicusBanker_CreatePoliceFicusInitA, METH_VARARGS, NULL },
	{ "FicusMissionSpeech", FicusBanker_FicusMissionSpeechA, METH_VARARGS, NULL },
	{ "CreateBankDirectorScrapusInit", FicusBanker_CreateBankDirectorScrapusInitA, METH_VARARGS, NULL },
	{ "ScrapusMissionSpeech", FicusBanker_ScrapusMissionSpeechA, METH_VARARGS, NULL },
	{ "InitKillScrapus", FicusBanker_InitKillScrapusA, METH_VARARGS, NULL },
	{ "SpawnCheck", FicusBanker_SpawnCheckA, METH_VARARGS, NULL },
	{ "TakeCheck", FicusBanker_TakeCheckA, METH_VARARGS, NULL },
	{ "SetNextMission", FicusBanker_SetNextMissionA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GetCrazyData ---- */

/* Init(LevelPath): the bureau hidden (its position kept); the mission goes on where it was saved */
static byte CrazyData_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t bp, state;
	byte ok = 1;
	(void)a;
	(void)out;
	Native_SetNone(&bp);
	if (!Mode_Import("Map") || !PORT_LCALL(Map, HideBureau, &bp, "") || !Mode_Put(GC, "bureauPos", NULL, &bp)) {
		Native_Free(&bp);
		return 0;
	}
	Native_Free(&bp);
	Mode_SaveVar("MissionState", &state);
	if (LvN_EqS(&state, "GetCrazyData")) {
		ok = PORT_LOWN(Mission_GetCrazyData, GetCrazyData, NULL, "");
	} else if (LvN_EqS(&state, "GoToPressRoom")) {
		ok = Pr_DelIfS("Humphrey_Default") && PORT_LOWN(Mission_GetCrazyData, GoToPressRoom, NULL, "");
	} else if (LvN_EqS(&state, "FindHumphrey")) {
		ok = PORT_LOWN(Mission_GetCrazyData, FindHumphrey, NULL, "");
	} else if (LvN_EqS(&state, "FindTrurl")) {
		ok = PORT_LOWN(Mission_GetCrazyData, FindTrurl, NULL, "");
	}
	Native_Free(&state);
	return ok;
}

/* h.ArrOpt0 = "Use:" + Scrap.GetLangStr("USE_Switchs_Activate") */
static byte Gc_UseOpt(const nativeVal_t *e) {
	nativeVal_t a, b, r;
	LvN_Str(&a, "Use:");
	Mode_Lang("USE_Switchs_Activate", &b);
	Native_SetNone(&r);
	byte ok = Native_Arith('+', &a, &b, &r) && Mode_VSet(e, "ArrOpt0", &r);
	Native_Free(&a);
	Native_Free(&b);
	Native_Free(&r);
	return ok;
}

/* the switch e can be used to turn it on (OnSwitchPressed) */
static byte Pr_SwitchUse(const nativeVal_t *e) {
	return LvN_SetS(e, "UseFunc", "Mission_GetCrazyData.OnSwitchPressed") && LvN_SetI(e, "ArrowCanUse", 1) && LvN_SetI(e, "ArrnOpt", 0) &&
		Gc_UseOpt(e);
}

/* GetCrazyData(): the four switches of the alarm (their class and entities), the alarm hook, the scorer */
static byte CrazyData_GetCrazyData(const nativeVal_t *a, nativeVal_t *out) {
	static const struct {
		const char *name, *anm;
		long loop;
	} acts[] = { { "ON", "/Map/Switch/Anm/ON.M3D", 0 }, { "OFF", "/Map/Switch/Anm/OFF.M3D", 0 }, { "StayON", "/Map/Switch/Anm/StayON.M3D", 1 },
		{ "StayOFF", "/Map/Switch/Anm/StayOFF.M3D", 1 } };
	nativeVal_t switches;
	Native_SetNone(&switches);
	(void)a;
	(void)out;
	if (!Mode_Import("Init") || !Mode_Import("Alarm") || !Pr_DelIfS("Humphrey_Default")) {
		return 0;
	}
	SaveVar_Set("MapPress_HiAlarm", "0");
	if (!Mode_SAct("CreateClass", "Switch") || !Mode_PathSet(1, "ModelFilePath", "/Map/Switch/Switch.m3d")) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		if (!Mode_SAct("CreateAction", acts[i].name) || !Mode_PathSet(0, "AnmFilePath", acts[i].anm) || !Mode_SActSet(0, "isLoop", NULL, acts[i].loop)) {
			return 0;
		}
	}
	if (!Mode_SActSet(1, "DefAction", "StayOFF", 0) || !Mode_G(GC, "switches", &switches)) {
		return 0;
	}
	int n = Mode_Loop(&switches);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t sw, dummy, e;
		const char *s;
		Native_SetNone(&sw);
		Native_SetNone(&dummy);
		Native_SetNone(&e);
		ok = Native_SeqItem(&switches, i, &sw) && Native_ArgStr(&sw, 1, &s);
		if (ok) {
			Mode_DummyStart(s, 1);
			Mode_DummyNext(&dummy);
			if (!Native_True(&dummy)) {
				ok = LvN_RaiseText("No se ha encontrado el dummy para el interruptor");
			} else {
				ok = Mode_FromDummy(&dummy, "WithLife", &e) && LvN_SetS(&e, "ActCtrl", "Switch") && LvN_SetI(&e, "VRadius", 50) &&
					LvN_SetI(&e, "ArrowCanUse", 1) && LvN_SetS(&e, "UseFunc", "Mission_GetCrazyData.OnSwitchPressed") && LvN_SetI(&e, "ArrnOpt", 0) &&
					Gc_UseOpt(&e);
			}
		}
		Native_Free(&sw);
		Native_Free(&dummy);
		Native_Free(&e);
	}
	ok = ok && PORT_CALL(MissionsFuncs, SetMultipleMissionTargets, NULL, "v", &switches);
	Native_Free(&switches);
	return ok && PR_HOOK("Alarm", "TempAbortMissionCallback", GC, "OnAlarmActivated") && Pr_Remote("Mission_GetCrazyData_RemoteMessage1", "DeepThroat",
		NULL, NULL, 0) && LV_SCORERG(Mission_GetCrazyData, MissionScorer) && PORT_LOWN(Mission_GetCrazyData, MissionScorer, NULL, "i", 0L);
}

/* for sw in switches: e = Scrap.GetEntity(sw); if e: the switch off (and usable again with use, else not) */
static byte Gc_SwitchesOff(byte use) {
	nativeVal_t switches;
	if (!Mode_G(GC, "switches", &switches)) {
		return 0;
	}
	int n = Mode_Loop(&switches);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t sw, e;
		Native_SetNone(&sw);
		Native_SetNone(&e);
		ok = Native_SeqItem(&switches, i, &sw) && Mode_EntityH(&sw, &e) && (!Native_True(&e) || (LvN_SetS(&e, "DefLoop", "StayOFF") &&
			LvN_SetS(&e, "Action", "OFF") && (use ? Pr_SwitchUse(&e) && LvN_Activate(&e, 0) : LvN_SetS(&e, "UseFunc", "") &&
			LvN_SetI(&e, "ArrowCanUse", 0))));
		Native_Free(&sw);
		Native_Free(&e);
	}
	Native_Free(&switches);
	return ok;
}

/* OnAlarmActivated(): the alarm resets the switches, the counter and the timer */
static byte CrazyData_OnAlarmActivated(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t switches;
	Native_SetNone(&switches);
	(void)a;
	(void)out;
	if (!Gc_SwitchesOff(1) || !Mode_G(GC, "switches", &switches)) {
		return 0;
	}
	byte ok = PORT_CALL(MissionsFuncs, SetMultipleMissionTargets, NULL, "v", &switches);
	Native_Free(&switches);
	return ok && LvN_PutI(GC, "TimerCanBeSeen", 0) && LvN_PutI(GC, "switchesRemaining", 4) && LvN_Set0I("MissionTimer", "Visible", 0) &&
		PORT_LOWN(Mission_GetCrazyData, MissionScorer, NULL, "i", 0L) &&
		Pr_Remote("Mission_GetCrazyData_RemoteMessage6", "DeepThroat", NULL, NULL, 0);
}

/* OnSwitchPressed(UsrName, CharName, optId, FullOption): a switch on (not during the alarm); a sentinel comes to
 * look; after the last one the timer */
static byte CrazyData_OnSwitchPressed(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *pos = Mode_V(&t), *left = Mode_V(&t), *d = Mode_V(&t), *rep = Mode_V(&t), *msg = Mode_V(&t);
	(void)out;
	if (!Mode_EntityH(&a[1], e)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(e)) {
		return Mode_Done(&t, 1);
	}
	int alarm = Mode_AlarmOn();
	if (alarm) {
		return Mode_Done(&t, alarm > 0 && Pr_Remote("Mission_GetCrazyData_RemoteMessage6", "DeepThroat", NULL, NULL, 0));
	}
	/* SLogic.SendSentinelToWatch by name: fc-r7b-gc records it (OBJLIB 7) */
	if (!LvN_SetS(e, "DefLoop", "StayON") || !LvN_SetS(e, "Action", "ON") || !LvN_SetS(e, "UseFunc", "") || !LvN_SetI(e, "ArrowCanUse", 0) ||
		!LvN_Activate(e, 0) || !Mode_VGet(e, "Pos", pos) || !LV_CALL("SLogic", "SendSentinelToWatch", NULL, "v", pos) ||
		!Mode_GOpI(GC, "switchesRemaining", '-', 1) || !PORT_LOWN(Mission_GetCrazyData, MissionScorer, NULL, "i", 0L) ||
		!Mode_G(GC, "switchesRemaining", left)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqI(left, 0)) {
		return Mode_Done(&t, LV_COM("Mission_GetCrazyData_RemoteMessage5", "DeepThroat", Mission_GetCrazyData, StartTimer, LvN_NoArgs()));
	}
	Native_Free(left);
	if (!Mode_G(GC, "switchesRemaining", left) || !Native_Arith('-', Mode_I(&t, 5), left, d)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(d, rep);
	return Mode_Done(&t, Native_Arith('+', Mode_S(&t, "Mission_GetCrazyData_RemoteMessage"), rep, msg) &&
		PORT_CALL(Speech, AddRemoteCom, NULL, "vs", msg, "DeepThroat"));
}

/* StartTimer(): the countdown of the data is visible */
static byte CrazyData_StartTimer(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t mt;
	Native_SetNone(&mt);
	(void)a;
	(void)out;
	if (!LvN_PutI(GC, "TimerCanBeSeen", 1) || !Mode_G(GC, "missionTime", &mt)) {
		return 0;
	}
	byte ok = Mode_GVarSet("TimeSpecialUseTimer", &mt);
	Native_Free(&mt);
	return ok && LvN_Set0I("MissionTimer", "Visible", 1) && Pr_DelIfS("Humphrey_Default") &&
		PORT_LOWN(Mission_GetCrazyData, MissionScorer, NULL, "i", 0L);
}

/* TimeExpired(): the switches go off for good; on to the press room */
static byte CrazyData_TimeExpired(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Gc_SwitchesOff(0) && LvN_ScorerOff() && Pr_Remote("Mission_GetCrazyData_RemoteMessage7", "DeepThroat", GC, "PreGoToPressRoom", 1) &&
		LvN_ModNone("Alarm", "TempAbortMissionCallback");
}

/* GoToPressRoom(): an item where the bureau was takes the player to the press room */
static byte CrazyData_GoToPressRoom(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *bp = Mode_V(&t), *l = Mode_V(&t), *ln = Mode_V(&t), *name = Mode_V(&t), *l2 = Mode_V(&t), *cam = Mode_V(&t);
	(void)a;
	(void)out;
	SaveVar_Set("MapPress_BureauExists", "0");
	Mode_Ints3(cam, 0, 100, 0);
	byte ok = Mode_G(GC, "bureauPos", bp) && LvN_CreateAt(Mode_S(&t, "EmptyBureau"), bp, "Item", l) && LvN_SetI(l, "Radius", 500) &&
		LvN_SetF(l, "rTime", 0.25) && LvN_SetS(l, "OnTake", "Mission_GetCrazyData.OnFakeBureauTaken") && Mode_VGet(l, "Name", ln) &&
		Native_Arith('+', Mode_S(&t, "CheckArrow"), ln, name);
	Native_Free(bp);
	ok = ok && Mode_G(GC, "bureauPos", bp) && LvN_CreateAt(name, bp, "WithLife", l2) && Mode_VSet(l2, "CamTgt", cam) && LvN_TargetName(l2);
	return Mode_Done(&t, ok);
}

/* Pr_DelIf("CheckArrow" + item) */
static byte Gc_ArrowDel(const nativeVal_t *item) {
	nativeVal_t a, n;
	LvN_Str(&a, "CheckArrow");
	Native_SetNone(&n);
	byte ok = Native_Arith('+', &a, item, &n) && Pr_DelIf(&n);
	Native_Free(&a);
	Native_Free(&n);
	return ok;
}

/* OnFakeBureauTaken(ItemName, CharName): the player reaches the place: the item and its arrow go */
static byte CrazyData_OnFakeBureauTaken(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, un;
	(void)out;
	Native_SetNone(&usr);
	Native_SetNone(&un);
	int u = LvN_Usr(&usr);
	int me = u > 0 ? (Mode_VGet(&usr, "Name", &un) ? Mode_Cmp(&a[1], "==", &un) : -1) : u;
	byte ok = me >= 0 && (!me || (Pr_DelIf(&a[0]) && Gc_ArrowDel(&a[0]) &&
		Pr_Remote("Mission_GoToPressRoom_RemoteMessage1", "DeepThroat", GC, "PreFindHumphrey", 1)));
	Native_Free(&usr);
	Native_Free(&un);
	return ok;
}

/* a mission character made now; when it is there it gets use and becomes the target (pos NULL: where it is) */
static byte Pr_MissionTarget(const char *name, const char *use, const long *pos) {
	nativeVal_t k, e;
	Native_SetNone(&e);
	if (!PORT_CALL(CharsNPC, CreateMissionChar, NULL, "s", name)) {
		return 0;
	}
	/* CharsNPC.CreateChar(CharsNPC.CharRes[name]) (OBJLIB 5: the character object of CharsNPC) */
	LvN_Str(&k, name);
	PyObject *ch = Mode_CharRes(&k);
	Native_Free(&k);
	byte ok = ch && PORT_CALL(CharsNPC, CreateChar, NULL, "O", ch);
	Py_XDECREF(ch);
	if (!ok || !LvN_Entity(name, &e)) {
		return 0;
	}
	ok = !Native_True(&e) || ((!pos || LvN_SetInts3(&e, "Pos", pos[0], pos[1], pos[2])) && LvN_SetS(&e, "UseFunc", use) && LvN_TargetName(&e));
	Native_Free(&e);
	return ok;
}

/* FindHumphrey(): Humphrey in the press room is the target */
static byte CrazyData_FindHumphrey(const nativeVal_t *a, nativeVal_t *out) {
	static const long pos[3] = { -3961, -1139, 3318 };
	(void)a;
	(void)out;
	return Mode_Import("CharsNPC") && Pr_MissionTarget("Humphrey_Default", "Mission_GetCrazyData.HumphreySpeech", pos);
}

/* HumphreySpeech(UsrName, CharName, optId, FullOption) */
static byte CrazyData_HumphreySpeech(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e;
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (!LvN_EqS(&a[2], "Talk")) {
		return 1;
	}
	if (!Mode_EntityH(&a[1], &e)) {
		return 0;
	}
	byte ok = !Native_True(&e) || (LvN_Activate(&e, 0) && LvN_SetS(&e, "UseFunc", "CharsNPC.GenericNPCSpeech"));
	Native_Free(&e);
	return ok && LvN_OnEnd(GC, "PreFindTrurl", NULL) && Mode_Spch("Mission_FindHumphrey_Humphrey_InitMsg1", "Mission_FindHumphrey_Humphrey_InitAns1", NULL);
}

/* FindTrurl(): Trurl is the target */
static byte CrazyData_FindTrurl(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_MissionTarget("Messenger_Trurl", "Mission_GetCrazyData.TrurlSpeech", NULL);
}

/* TrurlSpeech(UsrName, CharName, optId, FullOption): Trurl's talk; it ends the mission while he is the target */
static byte CrazyData_TrurlSpeech(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e;
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (!LvN_EqS(&a[2], "Talk")) {
		return 1;
	}
	if (!Mode_EntityH(&a[1], &e)) {
		return 0;
	}
	int target = Native_True(&e);
	if (target > 0) {
		nativeVal_t r;
		Native_SetNone(&r);
		target = Mode_VGet(&e, "RadarShow", &r) ? Mode_CmpI(&r, "==", 1) : -1;
		Native_Free(&r);
	}
	byte ok;
	if (target > 0) {
		ok = LvN_Activate(&e, 0) && LvN_OnEnd(GC, "SetNextMission", NULL);
	} else if (target == 0) {
		ok = LvN_ModNone("Speech", "OnEndOfSpeech") && Mode_SetModVar("Speech", "OnEndOfSpeechParams", LvN_NoArgs());
	} else {
		ok = 0;
	}
	Native_Free(&e);
	return ok && Mode_NISpeech("Trurl", "Mission_FindTrurl_Trurl_NI_1Msg");
}

/* the text of a mission scorer at y + dy as GetCrazyData sets it (Lv_Text) */
static byte Pr_ScorerText(const nativeVal_t *id, const char *item, long dy, const nativeVal_t *text) {
	nativeVal_t x, y;
	Native_SetNone(&x);
	byte ok = LvN_TextY(dy, &y) && LvN_TextX(0, &x) && LvN_MissionText(id, item, "Text", &x, &y, "Horatio", "Shadow", text, "Left", NULL);
	Native_Free(&x);
	Native_Free(&y);
	return ok;
}

/* MissionScorer(id): the switches left and the timer (hidden until it runs) */
static byte CrazyData_MissionScorer(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *label = Mode_V(&t), *left = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t);
	const nativeVal_t *id = &a[0];
	(void)out;
	LvN_Label("Mission_GetCrazyData_Scorer1", label);
	if (!Pr_ScorerText(id, "SwitchesRemainingText", 0, label) || !Mode_G(GC, "switchesRemaining", y)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(y, left);
	Native_Free(y);
	if (!Pr_ScorerText(id, "SwitchesRemaining", 20, left) || !Mode_ScorerAdd(id, Mode_S(&t, "MissionTimer"), "Timer") || !LvN_TextX(40, x) ||
		!LvN_ScorerSet(id, "MissionTimer", "X", x) || !LvN_TextY(60, y) || !LvN_ScorerSet(id, "MissionTimer", "Y", y) ||
		!LvN_ScorerSet(id, "MissionTimer", "Font", Mode_S(&t, "Horatio")) || !LvN_ScorerSet(id, "MissionTimer", "Effect", Mode_S(&t, "Shadow")) ||
		!LvN_ScorerSet(id, "MissionTimer", "Align", Mode_S(&t, "Left")) || !LvN_ScorerSet(id, "MissionTimer", "TimerType", Mode_S(&t, "TimeSpecialUse")) ||
		!LvN_ScorerSet(id, "MissionTimer", "OnTimeExpired", Mode_S(&t, "Mission_GetCrazyData.TimeExpired")) || !Mode_TextColour(id, "MissionTimer")) {
		return Mode_Done(&t, 0);
	}
	int seen = LvN_GTrue(GC, "TimerCanBeSeen");
	return Mode_Done(&t, seen >= 0 && (seen || LvN_ScorerSet(id, "MissionTimer", "Visible", Mode_I(&t, 0))));
}

LV_FN(CrazyData, InitA, 1, CrazyData_Init)
LV_FN(CrazyData, GetCrazyDataA, 0, CrazyData_GetCrazyData)
LV_FN(CrazyData, OnAlarmActivatedA, 0, CrazyData_OnAlarmActivated)
LV_FN(CrazyData, OnSwitchPressedA, 4, CrazyData_OnSwitchPressed)
LV_FN(CrazyData, StartTimerA, 0, CrazyData_StartTimer)
LV_FN(CrazyData, TimeExpiredA, 0, CrazyData_TimeExpired)
LV_FN(CrazyData, GoToPressRoomA, 0, CrazyData_GoToPressRoom)
LV_FN(CrazyData, OnFakeBureauTakenA, 2, CrazyData_OnFakeBureauTaken)
LV_FN(CrazyData, FindHumphreyA, 0, CrazyData_FindHumphrey)
LV_FN(CrazyData, HumphreySpeechA, 4, CrazyData_HumphreySpeech)
LV_FN(CrazyData, FindTrurlA, 0, CrazyData_FindTrurl)
LV_FN(CrazyData, TrurlSpeechA, 4, CrazyData_TrurlSpeech)
LV_FN(CrazyData, MissionScorerA, 1, CrazyData_MissionScorer)

PyMethodDef g_PressGetCrazyDataMethods[] = {
	{ "Init", CrazyData_InitA, METH_VARARGS, NULL },
	{ "GetCrazyData", CrazyData_GetCrazyDataA, METH_VARARGS, NULL },
	{ "OnAlarmActivated", CrazyData_OnAlarmActivatedA, METH_VARARGS, NULL },
	{ "OnSwitchPressed", CrazyData_OnSwitchPressedA, METH_VARARGS, NULL },
	{ "StartTimer", CrazyData_StartTimerA, METH_VARARGS, NULL },
	{ "TimeExpired", CrazyData_TimeExpiredA, METH_VARARGS, NULL },
	{ "GoToPressRoom", CrazyData_GoToPressRoomA, METH_VARARGS, NULL },
	{ "OnFakeBureauTaken", CrazyData_OnFakeBureauTakenA, METH_VARARGS, NULL },
	{ "FindHumphrey", CrazyData_FindHumphreyA, METH_VARARGS, NULL },
	{ "HumphreySpeech", CrazyData_HumphreySpeechA, METH_VARARGS, NULL },
	{ "FindTrurl", CrazyData_FindTrurlA, METH_VARARGS, NULL },
	{ "TrurlSpeech", CrazyData_TrurlSpeechA, METH_VARARGS, NULL },
	{ "MissionScorer", CrazyData_MissionScorerA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GoToPress ---- */

/* Init(LevelPath): the confession scene, the gambler's head, the boss room only for Dtritus, the scene at the
 * elevator's arrival */
static byte GoToPress_Init(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	/* SScorer.AddModel by name: the cases record it (OBJLIB 7) */
	return Mode_Import("Elevator") && LV_OWN(GP, "LoadScene", NULL, "") &&
		LV_CALL("SScorer", "AddModel", NULL, "isssiii", 0L, "EvilGambler", "Models/Heads/HeadEvilGambler/HeadEvilGambler", "", 4L, 25L, 10L) &&
		Pr_BossDoor("DtritusGroupGateKeeper", "GateKeeper.UseDoor", NULL) && PR_HOOK("Elevator", "OnElevatorReachDown", GP, "LaunchScene");
}

/* CrazySads(usr): after the scene: the player in place, the gambler's message */
static byte GoToPress_CrazySads(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_UsrBack(4095, 163, 436, 30, 1.0) && Pr_Remote("Mission_GoToPress_RemoteMessage1", "EvilGambler", GP, "SetNextMission", 1);
}

LV_FN(GoToPress, InitA, 1, GoToPress_Init)
LV_FN(GoToPress, CrazySadsA, 1, GoToPress_CrazySads)

PyMethodDef g_PressGoToPressMethods[] = {
	{ "Init", GoToPress_InitA, METH_VARARGS, NULL },
	{ "CrazySads", GoToPress_CrazySadsA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the human who follows Dtritus (Mission_GoWithHuman and Mission_TalkToBoss) ---- */

/* CreateHuman(e): the human of the mission (the global human); TalkToBoss also takes his use */
static byte Pr_CreateHuman(const nativeVal_t *e, const char *module, int nouse) {
	nativeVal_t h;
	Native_SetNone(&h);
	if (!Mode_Put(module, "human", NULL, e) || !Mode_G(module, "human", &h)) {
		return 0;
	}
	byte ok = LvN_SetLang(&h, "Descriptor", "CharName_Human") && (!nouse || LvN_SetS(&h, "UseFunc", "")) && LvN_SetI(&h, "ArrowCanUse", 0);
	Native_Free(&h);
	return ok;
}

/* the human stops and complains (SAI state 0, the message msg, not the target) */
static byte Pr_HumanStops(const char *module, const char *msg) {
	nativeVal_t hn;
	byte ok = LvN_GAttr(module, "human", "Name", &hn) && CHAR_STATE("iv", 0L, &hn);
	Native_Free(&hn);
	return ok && Pr_Remote(msg, "Human", NULL, NULL, 0) && LvN_GActivate(module, "human", 0);
}

/* OnPossession(newCharName, pos): the human will not follow a possessed character */
static byte Pr_HumanOnPossession(const char *module, const char *msg) {
	return Mode_Import("MissionsFuncs") && Pr_HumanStops(module, msg);
}

/* OnExitConversor(): out of the conversor: as Dtritus the human is a target again, else he stops */
static byte Pr_HumanOnExitConversor(const char *module, const char *msg) {
	nativeVal_t usr;
	if (!Mode_Import("MissionsFuncs")) {
		return 0;
	}
	Native_SetNone(&usr);
	int u = LvN_Usr(&usr);
	int dt = u > 0 ? LvN_CtrlIs(&usr, "Dtritus") : u;
	Native_Free(&usr);
	if (u <= 0) {
		return u == 0;
	}
	if (dt == 0) {
		return Pr_HumanStops(module, msg);
	}
	return dt > 0 && LvN_GActivate(module, "human", 2);
}

/* `SVec.ModSqr(SVec.Sub(a.Pos, b.Pos)) <= d * d` with d the global dist of module: 1, 0, -1 */
static int Pr_Within(const nativeVal_t *a, const nativeVal_t *b, const char *module, const char *dist) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pa = Mode_V(&t), *pb = Mode_V(&t), *sub = Mode_V(&t), *d = Mode_V(&t), *m1 = Mode_V(&t), *m2 = Mode_V(&t), *mm = Mode_V(&t);
	int r = Mode_VGet(a, "Pos", pa) && Mode_VGet(b, "Pos", pb) && Mode_VecSub(pa, pb, sub) && Mode_VecModSqr(sub, d) && Mode_G(module, dist, m1) &&
		Mode_G(module, dist, m2) && Mode_Arith('*', m1, m2, mm) ? Mode_Cmp(d, "<=", mm) : -1;
	Mode_Done(&t, 1);
	return r;
}

/* CheckHumanDist(): 1 when the player is within maxHumanDist of the human, else 0 */
static byte Pr_CheckHumanDist(const char *module, nativeVal_t *out) {
	nativeVal_t usr, h;
	Native_SetNone(&usr);
	Native_SetNone(&h);
	int near = LvN_Usr(&usr);
	if (near > 0) {
		near = Mode_G(module, "human", &h) ? Pr_Within(&usr, &h, module, "maxHumanDist") : -1;
	}
	Native_Free(&usr);
	Native_Free(&h);
	Native_SetInt(out, near > 0);
	return near >= 0;
}

/* CharsNPC.CharRes[key].var = (x, y, z) (OBJLIB 5: the character objects of CharsNPC) */
static byte Pr_ResSet(const char *key, const char *var, long x, long y, long z) {
	nativeVal_t k, v;
	LvN_Str(&k, key);
	PyObject *ch = Mode_CharRes(&k);
	Native_Free(&k);
	Mode_Ints3(&v, x, y, z);
	byte ok = ch && Mode_HSet(ch, var, &v);
	Native_Free(&v);
	Py_XDECREF(ch);
	return ok;
}

/* ---- Mission_GoWithHuman ---- */

/* InitStatus(LevelPath): the human at the entrance (by the metro unless the player comes by the elevator), the
 * alarm on, the epilogue, the mental controller still working */
static byte GoWithHuman_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const first[] = { "AlarmActive", "1", "AlarmStatus", "1", "Char", "Dtritus", NULL };
	static const char *const last[] = { "GameAct", "Epilogue", "Press.MentalControllerStatus", "0", NULL };
	(void)a;
	(void)out;
	if (!LV_MISSIONCHAR("Human_Entrance", Mission_GoWithHuman, CreateHuman)) {
		return 0;
	}
	Mode_Saves(first);
	if (LvN_SaveIs("ComeFrom", "DoorElevator")) {
		SaveVar_Set("ComeFrom", "DoorElevator");
	} else {
		SaveVar_Set("ComeFrom", "DoorMetro");
		if (!Pr_ResSet("Human_Entrance", "Pos", 3344, -1575, -3724) || !Pr_ResSet("Human_Entrance", "Ang", 0, 90, 0)) {
			return 0;
		}
	}
	Mode_Saves(last);
	return 1;
}

/* CreateHuman(e) */
static byte GoWithHuman_CreateHuman(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Pr_CreateHuman(&a[0], GW, 0);
}

/* Init(LevelPath): the switch of the machine is the target, the human follows Dtritus, the scenes */
static byte GoWithHuman_Init(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *pos = Mode_V(&t), *sw = Mode_V(&t), *a2 = Mode_V(&t), *a21 = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *cam = Mode_V(&t), *usr = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_Import("MissionsFuncs")) {
		return Mode_Done(&t, 0);
	}
	LvN_Dummy("DM_Machine_Switch", 1, dummy);
	if (!Native_True(dummy)) {
		return Mode_Done(&t, LvN_RaiseText("ERROR: No se encuentra el dummy 'DM_Machine_Switch'"));
	}
	Native_SetEmpty(ang, NV_TUPLE);
	Native_SetInt(Native_Append(ang), 0);
	Mode_Ints3(cam, 0, 50, 0);
	byte ok = LvN_Item(dummy, 1, pos) && LvN_CreateAt(Mode_S(&t, "Machine_Switch"), pos, "WithLife", sw) && Mode_Put(GW, "switch", NULL, sw) &&
		LvN_Item(dummy, 2, a2) && LvN_Item(a2, 1, a21) && Native_Arith('+', a21, Mode_I(&t, 180), Native_Append(ang));
	if (ok) {
		Native_SetInt(Native_Append(ang), 0);
	}
	ok = ok && Mode_VSet(sw, "Ang", ang) && LvN_SetI(sw, "ActiveCMask", 0) && LvN_SetI(sw, "MapCollision", 1) && Mode_VSet(sw, "CamTgt", cam) &&
		LvN_TargetName(sw) && Mode_UsrHandle(usr) && LvN_HumanFollow(GW, usr) && LvN_GActivate(GW, "human", 2) &&
		PR_HOOK("CharConversor", "OnPossession", GW, "OnPossession") && PR_HOOK("CharConversor", "afterExitConversor", GW, "OnExitConversor") &&
		PORT_LOWN(Mission_GoWithHuman, CheckMaxDist, NULL, "") && LV_OWN(GW, "LoadScene", NULL, "") &&
		PORT_LOWN(Mission_GoWithHuman, LoadScene2, NULL, "");
	return Mode_Done(&t, ok);
}

/* OnPossession(newCharName, pos), OnExitConversor() */
static byte GoWithHuman_OnPossession(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_HumanOnPossession(GW, "Mission_GoWithHuman_RemoteMessage1");
}

static byte GoWithHuman_OnExitConversor(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_HumanOnExitConversor(GW, "Mission_GoWithHuman_RemoteMessage1");
}

/* SAI.GetStateChar(human.Name) op i: 1, 0, -1 (OBJLIB 7: by name, the cases record it) */
static int Pr_HumanState(const char *module, const char *op, long i) {
	nativeVal_t hn, st;
	Native_SetNone(&st);
	int r = LvN_GAttr(module, "human", "Name", &hn) && LV_CALL("SAI", "GetStateChar", &st, "v", &hn) ? Mode_CmpI(&st, op, i) : -1;
	Native_Free(&hn);
	Native_Free(&st);
	return r;
}

/* CheckMaxDist(): every second: the human stops when Dtritus is too far, else follows him; at the switch the scene */
static byte GoWithHuman_CheckMaxDist(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *hn = Mode_V(&t), *r = Mode_V(&t), *st = Mode_V(&t), *h = Mode_V(&t), *sw = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_Later(1.0, GW, "CheckMaxDist", NULL, 0, "CheckMaxDist")) {
		return Mode_Done(&t, 0);
	}
	int u = LvN_Usr(usr);
	if (u <= 0) {
		return Mode_Done(&t, u == 0);
	}
	int near = LvN_GAttr(GW, "human", "Name", hn) && PORT_LOWN(Mission_GoWithHuman, CheckHumanDist, r, "") ? Native_True(r) : -1;
	if (near == 0) {
		int moving = LV_CALL("SAI", "GetStateChar", st, "v", hn) ? Mode_CmpI(st, "!=", 0) : -1;
		return Mode_Done(&t, moving >= 0 && (!moving || (CHAR_STATE("iv", 0L, hn) && Pr_Remote("Mission_GoWithHuman_RemoteMessage1", "Human", NULL,
			NULL, 0))));
	}
	int dt = near > 0 ? LvN_CtrlIs(usr, "Dtritus") : -1;
	if (dt <= 0) {
		return Mode_Done(&t, dt == 0);
	}
	int follow = Pr_HumanState(GW, "!=", 3);
	if (follow > 0) {
		return Mode_Done(&t, LvN_HumanFollow(GW, usr));
	}
	if (follow < 0 || !Mode_G(GW, "human", h) || !Mode_G(GW, "switch", sw)) {
		return Mode_Done(&t, 0);
	}
	int there = Pr_Within(h, sw, GW, "minDistFromSwitch");
	return Mode_Done(&t, there >= 0 && (!there || LV_OWN(GW, "LaunchScene", NULL, "")));
}

/* CheckHumanDist() */
static byte GoWithHuman_CheckHumanDist(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	return Pr_CheckHumanDist(GW, out);
}

/* LoadNextMission(name=None): the machine destroyed, the reputation and the ship profile back, on to the final
 * fight in the GamblinDen */
static byte Gw_LoadNextMission(void) {
	static const char *const prev[][2] = { { "PrevBadReputation", "BadReputation" }, { "PrevStdShipAIProfile", "StdShipAIProfile" } };
	static const char *const saves[] = { "WindowsError", "1", "RobotsControlledByBoss", "0", NULL };
	if (!Mode_GVarSetI("isActionIndoor", 1)) {
		return 0;
	}
	SaveVar_Set("Press.MentalControllerStatus", "1");
	for (int i = 0; i < 2; i++) {
		nativeVal_t v;
		Mode_SaveVar(prev[i][0], &v);
		byte there = v.type != NV_NONE;
		Native_Free(&v);
		if (there) {
			Mode_SaveVar(prev[i][0], &v);
			byte ok = Mode_SetSaveVar(prev[i][1], &v) && Mode_DelSaveVars(prev[i][0]);
			Native_Free(&v);
			if (!ok) {
				return 0;
			}
		}
	}
	Mode_Saves(saves);
	if (!PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_BeforeFinalFight", "GamblinDen")) {
		return 0;
	}
	SaveVar_Set("Map", "Levels/GamblinDen");
	return PORT_CALL(SaveGame, QLoad, NULL, "s", "Levels/GamblinDen");
}

static PyObject *GoWithHuman_LoadNextMission(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "name", NULL };
	PyObject *name = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &name)) {
		return NULL;
	}
	return Mode_Result(Gw_LoadNextMission());
}

static const char *const gowithhuman_scene2[] = {
	"SAct.GetClass|Boss",
	"MakeChar.CreateRelaxAction|Boss|0|TalkAngry|TalkAngry|1",
	"Scene.Load|HumanCD2",
	"Scene.ActorAction|0|Human|Relax",
	"Scene.ActorAction|150|Human|Relax0",
	"Scene.ActorAction|360|Human|Relax",
	"Scene.ActorAction|405|Human|Walk",
	"Scene.ActorAction|420|Human|Relax",
	"Scene.ActorAction|550|Human|Relax1",
	"Scene.ActorAction|670|Human|Relax",
	"Scene.ActorAction|1220|Human|Sad",
	"Scene.ActorAction|0|Dtritus|Relax",
	"Scene.ActorAction|330|Dtritus|Talk",
	"Scene.ActorAction|381|Dtritus|Relax",
	"Scene.ActorAction|410|Dtritus|RelaxLeft",
	"Scene.ActorAction|500|Dtritus|Relax",
	"Scene.ActorAction|1030|Dtritus|TalkPoint",
	"Scene.ActorAction|1075|Dtritus|Relax",
	"Scene.ActorAction|1100|Dtritus|Listen",
	"Scene.ActorAction|0|MonitorCharacter|Relax",
	"Scene.ActorAction|0|MonitorNoise|Relax",
	"Scene.AddGeneric|0|SScorer.Fade(0,(255,255,255,255),(255,255,255,0),3.0)",
	"Scene.AddGeneric|1390|SScorer.Fade(0,(0,0,0,0),(0,0,0,255),2.0)",
	"Scene.AddGeneric|1390|Scrap.MusicPlayer(\"\", \"fadeout\", 1/2.0)",
	"!180|NearCamera",
	"!450|EyeCamera",
	"!600|NearCamera",
	"!700|NormCamera",
	"!1050|NearCamera",
	"!1200|EyeCamera",
	"!1300|NormCamera",
	"Scene.AddText|60|Mission_Scene_GoWithHuman_1|9",
	"Scene.AddText|330|Mission_Scene_GoWithHuman_2|2",
	"Scene.AddText|390|Mission_Scene_GoWithHuman_3|21",
	"Scene.AddText|1030|Mission_Scene_GoWithHuman_4|2",
	"Scene.AddText|1080|Mission_Scene_GoWithHuman_5|10",
	"Scrap.Set|DefInterpTime|0.7",
	"Scene.AddCharActor|Human|Human",
	"MakeChar.CreateRelaxAction|Human|0|Walk|Walk|1",
	"MakeChar.CreateRelaxAction|Human|0|Relax1|Relax1|1",
	"MakeChar.CreateRelaxAction|Human|0|Relax0|Relax0|1",
	"Scene.CreateLocalAction|Human|0|Sad|Sad|0",
	"Scene.SetDefSounds|Human|Human",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|Dtritus|Dtritus",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk|Talk|1",
	"Scene.CreateLocalAction|Dtritus|0|RelaxLeft|RelaxLeft|1",
	"Scene.CreateLocalAction|Dtritus|0|Listen|Listen|0",
	"Scene.CreateLocalAction|Dtritus|0|TalkPoint|TalkPoint|1",
	"Scene.SetDefSounds|Dtritus|Dtritus",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddLocalActor|MonitorNoise|MonitorNoise",
	"Scene.CreateLocalAction|MonitorNoise|0|Relax|Relax|1",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddLocalActor|MonitorCharacter|MonitorCharacter",
	"Scene.CreateLocalAction|MonitorCharacter|0|Relax|Relax|1",
	"SAct.SetCls|DefAction|Relax",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

/* LoadScene2(): the boss' class gets its angry talk; the scene HumanCD2 */
static byte GoWithHuman_LoadScene2(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_Scene(GW, gowithhuman_scene2);
}

/* the action of the boss on the news panel: FakeSpeech.ForceAction = FakeSpeech.ForceDefLoop = action */
static byte Pr_FakeAction(const char *action) {
	nativeVal_t fs;
	if (!LvN_Entity("FakeSpeech", &fs)) {
		return 0;
	}
	byte ok = LvN_SetS(&fs, "ForceAction", action) && LvN_SetS(&fs, "ForceDefLoop", action);
	Native_Free(&fs);
	return ok;
}

/* NormCamera(): the boss on the news panel (FakeSpeech, made by CreateFakeSpeech) */
static byte Pr_NormCamera(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const vars[] = { "NewsPanelDistance", "NewsPanelDisplacement", "NewsPanelMove", "NewsPanelX", "NewsPanelY", "NewsPanelW",
		"NewsPanelH" };
	static const long vals[] = { 200, 110, 0, 0, 0, 27, 18 };
	(void)a;
	(void)out;
	for (int i = 0; i < 7; i++) {
		if (!Mode_GVarSetI(vars[i], vals[i])) {
			return 0;
		}
	}
	return Mode_GVarSetS("NewsHead", "FakeSpeech") && Pr_FakeAction("TalkAngry");
}

/* SetTalk() */
static byte Pr_SetTalk(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_FakeAction("Talk");
}

/* the boss' head for the news panel: an entity FakeSpeech (and Betty's FakeSpeech2 with talk2) */
static byte Pr_FakeSpeech(const char *name, const char *ctrl, const char *talk) {
	nativeVal_t n, x, y, z, kb;
	LvN_Str(&n, name);
	Native_SetInt(&x, 2760);
	Native_SetInt(&y, -1573);
	Native_SetInt(&z, -1949);
	Native_SetNone(&kb);
	byte ok = LvN_Create(&n, &x, &y, &z, "WithLife", &kb) && LvN_SetInts3(&kb, "Ang", 0, 0, 0) && LvN_SetS(&kb, "ActCtrl", ctrl) &&
		(!talk || (LvN_SetS(&kb, "Action", talk) && LvN_SetS(&kb, "DefLoop", talk)));
	Native_Free(&n);
	Native_Free(&kb);
	return ok;
}

/* Scrap.Set("NewsHead", "FakeSpeech"); Scrap.Set("NewsPanelShowHead", 1) (the caller then calls its NormCamera) */
static byte Pr_FakeSpeechShow(void) {
	return Mode_GVarSetS("NewsHead", "FakeSpeech") && Mode_GVarSetI("NewsPanelShowHead", 1);
}

/* CreateFakeSpeech(): the boss on the news panel */
static byte GoWithHuman_CreateFakeSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_FakeSpeech("FakeSpeech", "Boss", NULL) && Pr_FakeSpeechShow() && PORT_LOWN(Mission_GoWithHuman, NormCamera, NULL, "");
}

LV_FN(GoWithHuman, InitStatusA, 1, GoWithHuman_InitStatus)
LV_FN(GoWithHuman, CreateHumanA, 1, GoWithHuman_CreateHuman)
LV_FN(GoWithHuman, InitA, 1, GoWithHuman_Init)
LV_FN(GoWithHuman, OnPossessionA, 2, GoWithHuman_OnPossession)
LV_FN(GoWithHuman, OnExitConversorA, 0, GoWithHuman_OnExitConversor)
LV_FN(GoWithHuman, CheckMaxDistA, 0, GoWithHuman_CheckMaxDist)
LV_FN(GoWithHuman, CheckHumanDistA, 0, GoWithHuman_CheckHumanDist)
LV_FN(GoWithHuman, LoadScene2A, 0, GoWithHuman_LoadScene2)
LV_FN(GoWithHuman, CreateFakeSpeechA, 0, GoWithHuman_CreateFakeSpeech)
LV_FN(Pr, NormCameraA, 0, Pr_NormCamera)
LV_FN(Pr, SetTalkA, 0, Pr_SetTalk)

PyMethodDef g_PressGoWithHumanMethods[] = {
	{ "InitStatus", GoWithHuman_InitStatusA, METH_VARARGS, NULL },
	{ "CreateHuman", GoWithHuman_CreateHumanA, METH_VARARGS, NULL },
	{ "Init", GoWithHuman_InitA, METH_VARARGS, NULL },
	{ "OnPossession", GoWithHuman_OnPossessionA, METH_VARARGS, NULL },
	{ "OnExitConversor", GoWithHuman_OnExitConversorA, METH_VARARGS, NULL },
	{ "CheckMaxDist", GoWithHuman_CheckMaxDistA, METH_VARARGS, NULL },
	{ "CheckHumanDist", GoWithHuman_CheckHumanDistA, METH_VARARGS, NULL },
	{ "LoadNextMission", (PyCFunction)(void (*)(void))GoWithHuman_LoadNextMission, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "LoadScene2", GoWithHuman_LoadScene2A, METH_VARARGS, NULL },
	{ "CreateFakeSpeech", GoWithHuman_CreateFakeSpeechA, METH_VARARGS, NULL },
	{ "NormCamera", Pr_NormCameraA, METH_VARARGS, NULL },
	{ "SetTalk", Pr_SetTalkA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_HumphreyArchbishopKilled ---- */

/* SetNextMission(usr): after Betty's news: the player in place, the archbishop's death in the temple next, the
 * characters back at home, the gambler's call in 2 s */
static byte ArchbishopKilled_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const saves[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1", "Journalist.Humphrey_Defaut",
		"Press", "Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "Temple", NULL };
	(void)a;
	(void)out;
	if (!Pr_UsrBack(5670, 163, -283, -110, 1.0) || !PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_ArchbishopDead", "Temple") ||
		!Mode_MMDesc("Mission_ArchBishopKilled_MissionText1", 1)) {
		return 0;
	}
	Mode_Saves(saves);
	/* SSound.UsePS by name: the cases record it (OBJLIB 7) */
	return LV_CALL("SSound", "UsePS", NULL, "i", 1L) && PR_LATER(2, HK, "CGCalls") && Mode_GVarSetI("isActionIndoor", 1);
}

/* CGCalls(): the crazy gambler's call for the number of crazy deals done (not in the twelfth deal) */
static byte ArchbishopKilled_CGCalls(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const msgs[] = { "Mission_ArchbishopKilled_RemoteMessage3", "Mission_ArchbishopKilled_RemoteMessage4",
		"Mission_ArchbishopKilled_RemoteMessage5", "Mission_ArchbishopKilled_RemoteMessage6" };
	(void)a;
	(void)out;
	return Mode_CGCalls(msgs, 4);
}

LV_FN(ArchbishopKilled, SetNextMissionA, 1, ArchbishopKilled_SetNextMission)
LV_FN(ArchbishopKilled, CGCallsA, 0, ArchbishopKilled_CGCalls)

PyMethodDef g_PressHumphreyArchbishopKilledMethods[] = {
	{ "SetNextMission", ArchbishopKilled_SetNextMissionA, METH_VARARGS, NULL },
	{ "CGCalls", ArchbishopKilled_CGCallsA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_IntroHuman2 ---- */

/* LaunchHumphreySpeech(usr): after the news: Humphrey in front of the player talks (SetNextMission at the end) */
static byte IntroHuman2_LaunchHumphreySpeech(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *h = Mode_V(&t), *front = Mode_V(&t), *ua = Mode_V(&t), *dir = Mode_V(&t), *pu = Mode_V(&t), *vu = Mode_V(&t);
	nativeVal_t *vh = Mode_V(&t), *s = Mode_V(&t), *d = Mode_V(&t), *p = Mode_V(&t), *np = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *x = Mode_V(&t), *un = Mode_V(&t), *hn = Mode_V(&t);
	const nativeVal_t *usr = &a[0];
	(void)out;
	if (!Mode_GVarSetI("isActionIndoor", 1) || !Mode_Fade(0, lv_black, lv_clear, 1.0) || !LvN_Entity("Humphrey_Default", h)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(usr) || !Native_True(h)) {
		return Mode_Done(&t, 1);
	}
	Native_SetVec(front, 0.0, 0.0, -1.0);
	Native_SetEmpty(ang, NV_TUPLE);
	byte ok = Mode_VGet(usr, "Ang", ua) && Mode_Rotate(front, ua, dir) && Mode_VGet(usr, "Pos", pu) && Mode_VGet(usr, "VRadius", vu) &&
		Mode_VGet(h, "VRadius", vh) && Native_Arith('+', vu, vh, s) && Native_Arith('+', s, Mode_I(&t, 50), d) && Mode_VecProd(dir, d, p) &&
		Mode_VecAdd(pu, p, np) && Mode_VSet(h, "Pos", np);
	for (int i = 0; ok && i < 3; i++) {
		Native_Free(ua);
		Native_Free(x);
		ok = Mode_VGet(usr, "Ang", ua) && LvN_Item(ua, i, x) && (i != 1 ? (Native_Copy(Native_Append(ang), x), 1) :
			Native_Arith('+', x, Mode_I(&t, 180), Native_Append(ang)));
	}
	ok = ok && Mode_VSet(h, "Ang", ang) && LvN_OnEnd(IH, "SetNextMission", NULL) && Mode_VGet(usr, "Name", un) && Mode_VGet(h, "Name", hn) &&
		LvN_Speakers(un, hn) && PORT_CALL(Speech, NISpeech, NULL, "s", "Humphrey") &&
		PORT_CALL(Speech, AddNI, NULL, "ss", "Mission_IntroHuman_Humphrey_InitMsg1", "Mission_IntroHuman_Humphrey_InitAns1") &&
		PORT_CALL(Speech, RunNI, NULL, "");
	return Mode_Done(&t, ok);
}

static const char *const introhuman2_scene[] = {
	"Scene.Load|News3",
	"Scene.ActorAction|0|Betty|TV-Relax",
	"Scene.ActorAction|60|Betty|TV-Look",
	"Scene.ActorAction|100|Betty|TV-Relax",
	"Scene.ActorAction|140|Betty|MorphLips",
	"Scene.ActorAction|140|Betty|TV-Talk",
	"Scene.ActorAction|200|Betty|TV-Look",
	"Scene.ActorAction|260|Betty|TV-Talk",
	"Scene.ActorAction|290|Betty|TV-Talk1",
	"Scene.ActorAction|340|Betty|TV-Talk0",
	"Scene.ActorAction|380|Betty|TV-Boton",
	"Scene.ActorAction|420|Betty|TV-Talk",
	"Scene.ActorAction|660|Betty|TV-Talk1",
	"Scene.ActorAction|720|Betty|TV-Talk",
	"Scene.ActorAction|785|Betty|TV-Talk0",
	"Scene.ActorAction|845|Betty|TV-Relax",
	"Scene.ActorBreakChannel|860|Betty|3",
	"Scene.ActorAction|0|Cabecera|Relax",
	"Scene.AddGeneric|90|Scrap.GetEntity(\"News3Cabecera\").Active=0",
	"Scene.ActorAction|0|Screenshot1|Relax",
	"Scene.ActorAction|0|Final|Relax",
	"Scene.ActorAction|0|Portatil|Relax",
	"Scene.AddText|140|News_3rdMurder_RemoteMessage1|24",
	"Scrap.Set|DefInterpTime|1.0",
	"Scene.AddCharActor|Betty|Betty",
	"Scene.CreateLocalAction|Betty|0|TV-Relax|TV-Relax|1",
	"Scene.CreateLocalAction|Betty|0|TV-Talk|TV-Talk|1",
	"Scene.CreateLocalAction|Betty|0|TV-Talk0|TV-Talk0|1",
	"Scene.CreateLocalAction|Betty|0|TV-Talk1|TV-Talk1|1",
	"Scene.CreateLocalAction|Betty|0|TV-Boton|TV-Boton|1",
	"Scene.CreateLocalAction|Betty|0|TV-Look|TV-Look|1",
	"Scene.SetDefSounds|Betty|Betty",
	"SAct.SetCls|DefAction|TV-Relax",
	"Scene.AddLocalActor|Cabecera|Cabecera",
	"Scene.CreateLocalAction|Cabecera|0|Relax|Relax|1",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddLocalActor|Screenshot1|Screenshot1",
	"Scene.CreateLocalAction|Screenshot1|0|Relax|Relax|1",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddLocalActor|Final|Final",
	"Scene.CreateLocalAction|Final|0|Relax|Relax|1",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddLocalActor|Portatil|Portatil",
	"Scene.CreateLocalAction|Portatil|0|Relax|Relax|1",
	"SAct.SetCls|DefAction|Relax",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

/* LoadScene(): Betty's news of the third murder (News3) */
static byte IntroHuman2_LoadScene(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_Scene(IH, introhuman2_scene);
}

LV_FN(IntroHuman2, LaunchHumphreySpeechA, 1, IntroHuman2_LaunchHumphreySpeech)
LV_FN(IntroHuman2, LoadSceneA, 0, IntroHuman2_LoadScene)

PyMethodDef g_PressIntroHuman2Methods[] = {
	{ "LaunchHumphreySpeech", IntroHuman2_LaunchHumphreySpeechA, METH_VARARGS, NULL },
	{ "LoadScene", IntroHuman2_LoadSceneA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_PressMeeting ---- */

/* SetNextMission(usr): after the meeting: saved, the mayor next in DownTown, Betty's ship parked at the press */
static byte PressMeeting_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t lst, it;
	(void)a;
	(void)out;
	/* SSound.UsePS by name: the cases record it (OBJLIB 7) */
	if (!Mode_GVarSetI("isActionIndoor", 1) || !Mode_Fade(0, lv_black, lv_clear, 1.0) || !Mode_WayPoint() ||
		!PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_FaceTheMayor", "DownTown") ||
		!Mode_MMDesc("Mission_FaceTheMayor_MissionText1", 1) || !LV_CALL("SSound", "UsePS", NULL, "i", 1L)) {
		return 0;
	}
	LvN_Str(&it, "SBETTY1");
	byte ok = LvN_SaveList("Parked.Press", PM, &lst) && LvN_SaveAppend("Parked.Press", &lst, &it);
	Native_Free(&lst);
	Native_Free(&it);
	return ok;
}

LV_FN(PressMeeting, SetNextMissionA, 1, PressMeeting_SetNextMission)

PyMethodDef g_PressPressMeetingMethods[] = {
	{ "SetNextMission", PressMeeting_SetNextMissionA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_MeetBoss ---- */

/* InitStatus(LevelPath): the callbacks after the NPCs, Betty's resources, Berto waits for Dtritus */
static byte MeetBoss_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("CharsNPC") && Mode_Import("Chars") && LvN_ModAppendG("CharsNPC", "AfterBrowseNPC", MB, "AfterBrowseNPC") &&
		PORT_CALL(Chars, AddNewCharRes, NULL, "s", "Betty") && LV_MISSIONCHAR("Berto_MeetBos", Mission_MeetBoss, CreateBertoInit) &&
		Mode_GVarSetI("DefActionRadius", 1000);
}

/* `if CharsNPC.CharRes.has_key(name): del CharsNPC.CharRes[name]` (OBJLIB 5) */
static byte Pr_ResDel(const char *name) {
	nativeVal_t k;
	LvN_Str(&k, name);
	int has = Mode_HasKey("CharsNPC", "CharRes", 0, &k);
	byte ok = has >= 0 && (!has || Mode_ModDictDel("CharsNPC", "CharRes", &k));
	Native_Free(&k);
	return ok;
}

/* Init.isDemo as a condition: 1, 0, -1 */
static int Pr_Demo(void) {
	nativeVal_t v;
	int r = LvN_ModGet("Init", "isDemo", &v) ? Native_True(&v) : -1;
	Native_Free(&v);
	return r;
}

/* AfterBrowseNPC(): Humphrey's callback; no messengers out of the demo */
static byte MeetBoss_AfterBrowseNPC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Mode_Import("Init") || !LV_CALLBACK("Humphrey_Default", Mission_MeetBoss, CreateHumphreyInit)) {
		return 0;
	}
	int demo = Pr_Demo();
	return demo >= 0 && (demo || (Pr_ResDel("Messenger_01") && Pr_ResDel("Messenger_02")));
}

/* CreateHumphreyInit(h) */
static byte MeetBoss_CreateHumphreyInit(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return !LvN_SaveIs("MissionState", "ChallengeHumphrey") || LvN_SetS(&a[0], "UseFunc", "Mission_MeetBoss.HumphreySpeech");
}

/* CreateBertoInit(e): Berto talks and stays where he is */
static byte MeetBoss_CreateBertoInit(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_SetS(&a[0], "UseFunc", "Mission_MeetBoss.BertoSpeech") && LvN_Stay(&a[0], 6, LvN_Empty());
}

/* Speech.AutoSpeech(UsrName, CharName, optId, FullOption) */
static byte Pr_AutoSpeech(const nativeVal_t *a) {
	return PORT_CALL(Speech, AutoSpeech, NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]);
}

/* BertoSpeech(UsrName, CharName, optId, FullOption): the first talk starts the prolog (Berto goes, the camera of
 * Sebastian), later the usual talk */
static byte MeetBoss_BertoSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	int first = LvN_GTrue(MB, "FirstBertoSpeech");
	if (first > 0) {
		return LvN_PutI(MB, "FirstBertoSpeech", 0) && Mode_Target(NULL) &&
			Mode_EntSetS("DM_Door_Metro_Press", "UseFunc", "Mission_MeetBoss.OnTryingToExit2") &&
			Mode_EntSetS("DM_Door_Elevator_Press", "OnObjSlot", "!CloseDoorSlot") && Pr_DelIfS("Berto_MeetBos") &&
			Mode_GSetI(MB, "sebastian", "Active", 1) && PORT_LOWN(Mission_MeetBoss, PlayProlog0, NULL, "");
	}
	return first == 0 && Pr_AutoSpeech(a);
}

/* BertoSpeech2(UsrName, CharName, optId, FullOption): Berto after his possession, or not yet */
static byte MeetBoss_BertoSpeech2(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (LvN_SaveIs("Stats.PossessBerto", "1")) {
		return Pr_AutoSpeech(a);
	}
	return PORT_CALL(Speech, RestoreState, NULL, "v", &a[1]) &&
		Mode_Spch("Mission_FirstCrazyBet_Humphrey_InitMsg2", "Mission_FirstCrazyBet_Humphrey_InitAns2", NULL);
}

/* the emergency door and its warning lights: closed or automatic */
static byte Pr_Emero(const char *slot) {
	static const char *const names[] = { "DM_Door_PressDoorEmero_00", "DM_Door_PressWarningUpR_Press", "DM_Door_PressWarningDown_Press",
		"DM_Door_PressWarningUpL_Press" };
	for (int i = 0; i < 4; i++) {
		if (!Mode_EntSetS(names[i], "OnObjSlot", slot)) {
			return 0;
		}
	}
	return 1;
}

/* DeactivateEmero(), ActivateEmero() */
static byte MeetBoss_DeactivateEmero(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_Emero("!CloseDoorSlot");
}

static byte MeetBoss_ActivateEmero(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_Emero("!AutoDoorSlot");
}

/* the exits of the press (metro and elevator) lead to the function OnTryingToExit<n> */
static byte Pr_Exits(const char *use) {
	return Mode_EntSetS("DM_Door_Metro_Press", "UseFunc", use) && Mode_EntSetS("DM_Door_Elevator_Press", "UseFunc", use);
}

/* CharsNPC.CreateChar(CharsNPC.CreateMissionChar(name)) */
static byte Pr_CreateChar(const char *name) {
	nativeVal_t ch;
	Native_SetNone(&ch);
	byte ok = PORT_CALL(CharsNPC, CreateMissionChar, &ch, "s", name) && PORT_CALL(CharsNPC, CreateChar, NULL, "v", &ch);
	Native_Free(&ch);
	return ok;
}

/* Scrap.AddScheduledFunc(time, SScorer.Fade, (0, from, to, secs)) at the time value time (OBJLIB 4: the function of
 * SScorer the scheduler keeps) */
static byte Pr_FadeAt(const nativeVal_t *time, const int *from, const int *to, double secs) {
	nativeVal_t args[4];
	PyObject *fn = Script_Attr("SScorer", "Fade");
	if (!fn) {
		return 0;
	}
	Native_SetInt(&args[0], 0);
	Native_SetEmpty(&args[1], NV_TUPLE);
	Native_SetEmpty(&args[2], NV_TUPLE);
	for (int i = 0; i < 4; i++) {
		Native_SetInt(Native_Append(&args[1]), from[i]);
		Native_SetInt(Native_Append(&args[2]), to[i]);
	}
	Native_SetFloat(&args[3], secs);
	byte ok = Mode_Schedule(time, fn, args, 4, NULL);
	Native_Free(&args[1]);
	Native_Free(&args[2]);
	Py_DECREF(fn);
	return ok;
}

/* Init(LevelPath): Sebastian's head, the emergency door closed, the mission where it was saved: the prolog with
 * Berto, the battery, the conversion into Dtritus or the challenge to Humphrey */
static byte MeetBoss_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t state, meet, seb, five;
	byte ok;
	Native_SetNone(&seb);
	(void)a;
	(void)out;
	/* SScorer.AddModel by name: the cases record it (OBJLIB 7) */
	if (!LV_CALL("SScorer", "AddModel", NULL, "isssiiii", 0L, "Sebastian", "Models/Chars/Sebastian/Sebastian", "Models/Chars/Sebastian/Anm/Sebastian",
		3L, 25L, -10L, 1L)) {
		return 0;
	}
	Mode_SaveVar("MissionState", &state);
	LvN_Str(&meet, "MeetBoss");
	int other = PORT_LOWN(Mission_MeetBoss, DeactivateEmero, NULL, "") ? Mode_Cmp(&state, "!=", &meet) : -1;
	Native_Free(&meet);
	if (other < 0 || (other && (!Pr_DelIfS("Berto_MeetBos") || !Pr_CreateChar("Berto_Default"))) || !LvN_Entity("Sebastian_Camera", &seb)) {
		Native_Free(&state);
		return 0;
	}
	ok = Mode_Put(MB, "sebastian", NULL, &seb);
	Native_Free(&seb);
	if (!ok) {
		Native_Free(&state);
		return 0;
	}
	if (LvN_EqS(&state, "MeetBoss")) {
		ok = Mode_Import("Init") && Mode_Import("Elevator") && Pr_Exits("Mission_MeetBoss.OnTryingToExit") && LV_OWN(MB, "LoadProlog0Scene", NULL, "") &&
			LV_OWN(MB, "LoadProlog1Scene", NULL, "") && LV_OWN(MB, "LoadProlog2Scene", NULL, "") &&
			Mode_EntSetS("DM_Door_PressSlidingDoor_01", "OnObjSlot", "") && Mode_Target("Berto_MeetBos") && Mode_GSetI(MB, "sebastian", "Active", 0) &&
			Mode_GVarSetI("GenCharAttackActive", 0);
		int demo = ok ? Pr_Demo() : -1;
		Native_SetInt(&five, 5);
		ok = demo > 0 ? Pr_FadeAt(&five, lv_clear, lv_black, 0.5) && PR_HOOK("Elevator", "OnElevatorReachDown", MB, "EndOfMetroAnim") :
			demo == 0 && Pr_Remote("Mission_FirstContactPress_RemoteMessage2", "Berto", NULL, NULL, 0);
	} else if (LvN_EqS(&state, "GetBattery")) {
		ok = Mode_Import("Chars") && PR_HOOK("Chars", "OnMainCharDeath", MB, "OnMainCharDeath") && Pr_Exits("Mission_MeetBoss.OnTryingToExit2") &&
			Mode_MMDesc("Mission_GetBatteries_MissionText1", 1);
		if (ok && LvN_SaveIs("PlayerHasBeenKilled", "1")) {
			SaveVar_Set("PlayerHasBeenKilled", "0");
			ok = Pr_Remote("Mission_DestroyPolice_RemoteMessage5", "Bishop", NULL, NULL, 0);
		}
		ok = ok && PORT_LOWN(Mission_MeetBoss, CreateBattery, NULL, "") && Mode_GVarSetI("GenCharAttackActive", 0);
	} else if (LvN_EqS(&state, "ConvertIntoDtritus")) {
		ok = Pr_Exits("Mission_MeetBoss.OnTryingToExit3") &&
			LV_COM("Mission_FirstCrazyBet_RemoteMessage1", "CrazyGambler", Mission_MeetBoss, UseConversor, LvN_NoArgs());
	} else if (LvN_EqS(&state, "ChallengeHumphrey")) {
		ok = Pr_Exits("Mission_MeetBoss.OnTryingToExit3") && PORT_LOWN(Mission_MeetBoss, ChallengeHumphrey, NULL, "");
	}
	Native_Free(&state);
	return ok;
}

/* the optional usr of the prolog functions: the entry and the adapter of a core without arguments (the adapter keeps
 * the keyword usr and the errors of PyArg_ParseTupleAndKeywords) */
#define PR_USRFN(fn, core) \
	static byte fn##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		const nativeVal_t *usr = NULL; \
		(void)out; \
		return Mode_Parse(a, n, "|O", &usr) && core(); \
	} \
	static PyObject *fn(PyObject *self, PyObject *args, PyObject *kw) { \
		static char *names[] = { "usr", NULL }; \
		PyObject *usr = NULL; \
		(void)self; \
		if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) { \
			return NULL; \
		} \
		return Mode_Result(core()); \
	}

/* PlayProlog0(usr=None): the player at the start of the prolog; the gatekeeper of the boss room comes alive */
static byte Mb_PlayProlog0(void) {
	nativeVal_t ch, gc;
	Native_SetNone(&gc);
	Native_SetNone(&ch);
	byte ok = Mode_UsrHandle(&ch) && LvN_SetInts3(&ch, "Pos", -1114, -312, 7562) && LvN_SetInts3(&ch, "Ang", 0, 0, 0) &&
		PORT_CALL(Scene, Play, NULL, "s", "Prolog0") && PR_HOOK("Scene", "EndOfSceneCallback", MB, "PlayProlog1") && Mode_Target("");
	Native_Free(&ch);
	if (!ok || !LvN_Entity("GateKeeper_DTritusBossRoom", &gc)) {
		return 0;
	}
	ok = LvN_SetF(&gc, "TimeSpeed", 1.0) && LvN_SetI(&gc, "Active", 1);
	Native_Free(&gc);
	return ok;
}

/* PlayProlog1(usr=None), PlayProlog2(usr=None) */
static byte Mb_PlayProlog1(void) {
	return Mode_EntSetS("DM_Door_PressSlidingDoor_01", "OnObjSlot", "!AutoDoorUsrSlot") && Mode_EntSetS("DM_Door_PressSlidingDoor_00", "OnObjSlot", "") &&
		Mode_Fade(0, lv_black, lv_clear, 1.0) && PORT_CALL(Scene, Play, NULL, "s", "Prolog1") &&
		PR_HOOK("Scene", "EndOfSceneCallback", MB, "PlayProlog2");
}

static byte Mb_PlayProlog2(void) {
	return LvN_SetMusic("Music/Track1.ogg", 1.0) && Mode_EntSetS("DM_Door_PressSlidingDoor_00", "OnObjSlot", "!AutoDoorUsrSlot") &&
		Mode_Fade(0, lv_black, lv_clear, 1.0) && PORT_CALL(Scene, Play, NULL, "s", "Prolog2") &&
		PR_HOOK("Scene", "EndOfSceneCallback", MB, "AssignFirstMission");
}

PR_USRFN(MeetBoss_PlayProlog0, Mb_PlayProlog0)
PR_USRFN(MeetBoss_PlayProlog1, Mb_PlayProlog1)
PR_USRFN(MeetBoss_PlayProlog2, Mb_PlayProlog2)

/* the action and possession keys of Dtritus' class */
static byte Pr_DtritusKeys(const char *action, const char *possession) {
	return Mode_SAct("GetClass", "Dtritus") && Mode_SActSet(1, "ActionPress", action, 0) && Mode_SActSet(1, "PossessionPress", possession, 0);
}

/* the energy bar and the special action on (1) or off (0), now and in the save game */
static byte Pr_EnergyBar(int on) {
	if (!Mode_GVarSetI("EnergyBarActive", on)) {
		return 0;
	}
	SaveVar_Set("EnergyBarActive", on ? "1" : "0");
	if (!Mode_GVarSetI("SpecialActionActive", on)) {
		return 0;
	}
	SaveVar_Set("SpecialActionActive", on ? "1" : "0");
	return 1;
}

/* CreateBattery(): the battery to take is the target; no keys, no energy bar meanwhile */
static byte MeetBoss_CreateBattery(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *at = Mode_V(&t), *fn = Mode_V(&t), *b = Mode_V(&t), *pos = Mode_V(&t), *bn = Mode_V(&t), *name = Mode_V(&t), *b2 = Mode_V(&t);
	nativeVal_t *cam = Mode_V(&t);
	(void)a;
	(void)out;
	Mode_Ints3(at, 2619, 106, 3165);
	Mode_Ints3(cam, 0, 100, 0);
	byte ok = Mode_Import("MisItems") && Mode_Import("MissionsFuncs") && LvN_GObj(MB, "AssingOnTakeBattery", fn) &&
		PORT_CALL(MisItems, CreateBattery, b, "vv", at, fn) && Mode_VGet(b, "Pos", pos) && Mode_VGet(b, "Name", bn) &&
		Native_Arith('+', bn, Mode_S(&t, "CheckArrow"), name) && LvN_CreateAt(name, pos, "WithLife", b2) && Mode_VSet(b2, "CamTgt", cam) &&
		Mode_DelSchedS("DeleteCoin") && LvN_TargetName(b2) && Pr_DtritusKeys("", "") && Pr_EnergyBar(0);
	return Mode_Done(&t, ok);
}

/* AssingOnTakeBattery(l) */
static byte MeetBoss_AssingOnTakeBattery(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_SetS(&a[0], "OnTake", "Mission_MeetBoss.OnTakeBattery");
}

/* OnTakeBattery(ItemName, CharName): the battery taken: the action key and the energy bar on, Berto back, the
 * messages of Sebastian and the gambler */
static byte MeetBoss_OnTakeBattery(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t it, usr, k;
	Native_SetNone(&it);
	(void)out;
	if (!Mode_Import("Scorer") || !Mode_Import("Sound") || !Pr_Exits("Mission_MeetBoss.OnTryingToExit3") || !Mode_EntityH(&a[0], &it)) {
		return 0;
	}
	byte ok = Pr_TakenDel(&it) && Pr_TakenSound(&it, "TakeBattery");
	Native_Free(&it);
	Native_SetNone(&usr);
	ok = ok && Mode_Target(NULL) && Mode_UsrHandle(&usr) && LvN_SetI(&usr, "Energy", 0) && Pr_DtritusKeys("Mission_MeetBoss.OnActionPressed", "") &&
		Pr_EnergyBar(1) && PORT_CALL(Scorer, Blink, NULL, "is", 5L, "SpecialAction");
	Native_Free(&usr);
	if (!ok) {
		return 0;
	}
	LvN_Str(&k, "Berto_Default");
	int has = Mode_HasKey("CharsNPC", "CharRes", 0, &k);
	Native_Free(&k);
	return has >= 0 && (has || Pr_CreateChar("Berto_Default")) && Pr_Remote("Mission_GetBatteries_RemoteMessage1", "Sebastian", NULL, NULL, 0) &&
		Pr_Remote("Mission_GetBatteries_RemoteMessage4", "CrazyGambler", MB, "PreFirstCrazyDeal", 0);
}

/* PreBertoPossession(): the special action level 2, Berto's own talk, then the possession task */
static byte MeetBoss_PreBertoPossession(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e;
	Native_SetNone(&e);
	(void)a;
	(void)out;
	if (!Mode_Import("DTritusDesktop") || !Mode_GVarSetI("SpecialActionActive", 2)) {
		return 0;
	}
	SaveVar_Set("SpecialActionActive", "2");
	if (!LvN_ModNone("DTritusDesktop", "DialogController") || !Mode_MMDesc("Mission_FirstCrazyBet_MissionText1", 1) || !LvN_Entity("Berto_Default", &e)) {
		return 0;
	}
	byte ok = !Native_True(&e) || LvN_SetS(&e, "UseFunc", "Mission_MeetBoss.BertoSpeech2");
	Native_Free(&e);
	return ok && PORT_LOWN(Mission_MeetBoss, BertoPossession, NULL, "");
}

/* the system message of the text of key with the key of action (Lv_MsgText of Lv_KeyText) */
static byte Pr_KeyMsg(const char *key, const char *action) {
	nativeVal_t t;
	Native_SetNone(&t);
	byte ok = Mode_KeyText(key, action, &t) && Mode_SpeechMsg(&t);
	Native_Free(&t);
	return ok;
}

/* BertoPossession(): the three crazy deals, the first super deal, the possession key, the warnings every 20 s */
static byte MeetBoss_BertoPossession(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	SaveVar_Set("JackInTheBox.Active", "1");
	return PORT_CALL(MissionsFuncs, NewCrazyDeal, NULL, "isi", 1L, "Stats.PossessBerto", 1L) &&
		PORT_CALL(MissionsFuncs, NewCrazyDeal, NULL, "isi", 2L, "Stats.ConvertIntoDtritus", 1L) &&
		PORT_CALL(MissionsFuncs, NewCrazyDeal, NULL, "isi", 3L, "Stats.WinHumphreyRace", 1L) &&
		PORT_CALL(MissionsFuncs, SetNextSuperDeal, NULL, "sss", "CA", "SuperDeal_First", "Versus") &&
		Pr_DtritusKeys("", "Mission_MeetBoss.OnPossession") && Pr_KeyMsg("Mission_FirstCrazyBet_SystemMessage2", "Possession") &&
		Mode_Later(20, MB, "OnWarning", NULL, 0, "Warnings") && Mode_Target("Berto_Default");
}

/* OnWarning() */
static byte MeetBoss_OnWarning(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_Remote("Mission_FirstCrazyBet_RemoteMessage2", "CrazyGambler", NULL, NULL, 0) && Mode_Later(20, MB, "OnWarning", NULL, 0, "Warnings");
}

/* OnActionPressed(CharName) */
static byte MeetBoss_OnActionPressed(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_LangMsg("Mission_TalkToCrazy_SystemMessage1");
}

/* Speech.RemoteCom.remove(the last queued com of key) (OBJLIB 5: the list of Speech in place) */
static byte Mb_UnqueueCom(const char *key) {
	PyObject *list = Script_Attr("Speech", "RemoteCom");
	if (!list) {
		return 0;
	}
	int n = PyObject_Length(list);
	PyObject *found = NULL;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t k;
		Native_SetNone(&k);
		PyObject *rc = PySequence_GetItem(list, i), *k0 = rc ? PySequence_GetItem(rc, 0) : NULL;
		ok = k0 != NULL;
		if (ok) {
			Native_Arg(k0, &k);
			if (LvN_EqS(&k, key)) {
				Py_XDECREF(found);
				found = rc;
				Py_INCREF(found);
			}
		}
		Native_Free(&k);
		Py_XDECREF(k0);
		Py_XDECREF(rc);
	}
	if (ok && found) {
		PyObject *r = Script_Method(list, "remove", "(O)", found);
		ok = r != NULL;
		Py_XDECREF(r);
	}
	Py_XDECREF(found);
	Py_DECREF(list);
	return ok;
}

/* OnPossession(UserName, PossessedName): only Berto counts: the alarm, the possession, the warnings stop (the queued
 * one too), the alarm's end hook */
static byte MeetBoss_OnPossession(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t b, act, chars, fall, one;
	(void)out;
	if (!Mode_Import("Alarm") || !Mode_Import("CharConversor") || !Mode_Import("Speech")) {
		return 0;
	}
	Native_SetNone(&b);
	Native_SetNone(&act);
	Native_SetNone(&chars);
	int conv = Mode_EntityH(&a[1], &b) && Mode_VGet(&b, "ActCtrl", &act) && LvN_ModGet("CharConversor", "ConversionChars", &chars) ?
		Native_Contains(&chars, &act) : -1;
	Native_Free(&chars);
	if (conv <= 0) {
		Native_Free(&b);
		Native_Free(&act);
		return conv == 0 && Mode_LangMsg("Generics_ForbiddenPossession_SystemMessage");
	}
	Native_Free(&act);
	int berto = Mode_VGet(&b, "ActCtrl", &act) ? LvN_EqS(&act, "Berto") : -1;
	Native_Free(&b);
	Native_Free(&act);
	if (berto <= 0) {
		return berto == 0 && Pr_Remote("Mission_FirstCrazyBet_RemoteMessage2", "CrazyGambler", NULL, NULL, 0);
	}
	Native_SetInt(&one, 1);
	byte ok = Native_GVarGet("AlarmFallDelta", &fall) && Mode_Put(MB, "alarmFallDelta", NULL, &fall);
	Native_Free(&fall);
	if (!ok || !Mode_SetAlarm(&one) || !Mode_GVarSetI("AlarmFallDelta", 0) || !Mode_Target(NULL) ||
		!Pr_DtritusKeys("!DtritusActionPress", "CharConversor.Possession") || !Mode_SAct("GetClass", "Berto") ||
		!Mode_SActSet(1, "ActionPress", "", 0) || !PORT_CALL(CharConversor, Possession, NULL, "vv", &a[0], &a[1]) ||
		!LV_COM("Mission_FirstCrazyBet_RemoteMessage3", "CrazyGambler", Mission_MeetBoss, ShowMessage, LvN_NoArgs()) ||
		!Mode_DelSchedS("Warnings") ||
		!Mb_UnqueueCom("Mission_FirstCrazyBet_RemoteMessage2") || !LvN_ModAppendG("Alarm", "SetAlarmCallback", MB, "OnAlarmEnd")) {
		return 0;
	}
	SaveVar_Set("MissionState", "ConvertIntoDtritus");
	SaveVar_Set("Stats.PossessBerto", "1");
	return 1;
}

/* ShowMessage(): the action key of Berto explained; the alarm falls again */
static byte MeetBoss_ShowMessage(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t fall;
	Native_SetNone(&fall);
	(void)a;
	(void)out;
	if (!Pr_KeyMsg("Mission_FirstCrazyBet_SystemMessage1", "Action") || !Mode_SAct("GetClass", "Berto") ||
		!Mode_SActSet(1, "ActionPress", "!BertoActionPress", 0) || !Mode_G(MB, "alarmFallDelta", &fall)) {
		return 0;
	}
	byte ok = Mode_GVarSet("AlarmFallDelta", &fall);
	Native_Free(&fall);
	return ok;
}

/* OnAlarmEnd(status): when the alarm ends the gambler sends Dtritus to the conversor */
static byte MeetBoss_OnAlarmEnd(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t args[4];
	(void)out;
	if (!Mode_Import("Alarm")) {
		return 0;
	}
	if (LvN_EqI(&a[0], 1)) {
		return LvN_ModAppendG("Alarm", "SetAlarmCallback", MB, "OnAlarmEnd");
	}
	if (!LvN_GObj(MB, "UseConversor", &args[2])) {
		return 0;
	}
	LvN_Str(&args[0], "Mission_FirstCrazyBet_RemoteMessage1");
	LvN_Str(&args[1], "CrazyGambler");
	Native_SetEmpty(&args[3], NV_TUPLE);
	byte ok = Mode_LaterIn(1, "Speech", "AddRemoteCom", args, 4, NULL);
	for (int i = 0; i < 4; i++) {
		Native_Free(&args[i]);
	}
	return ok;
}

/* set the use of every conversion point (OBJLIB 7: the handles in the list of CharConversor) */
static byte Pr_Conversors(const char *use) {
	nativeVal_t pts;
	if (!LvN_ModGet("CharConversor", "ConversionPoints", &pts)) {
		return 0;
	}
	int n = Mode_Loop(&pts);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t cc;
		Native_SetNone(&cc);
		ok = Native_SeqItem(&pts, i, &cc) && LvN_SetS(&cc, "UseFunc", use);
		Native_Free(&cc);
	}
	Native_Free(&pts);
	return ok;
}

/* UseConversor(): the conversors on, the fourth one the target */
static byte MeetBoss_UseConversor(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr;
	(void)a;
	(void)out;
	Native_SetNone(&usr);
	int u = LvN_Usr(&usr);
	Native_Free(&usr);
	if (u <= 0) {
		return u == 0;
	}
	SaveVar_Set("Conversor.ActiveConversors", "1");
	return Mode_Target("DM_Conversion_Point_04") && Pr_Conversors("Mission_MeetBoss.OnConversorUsed") &&
		PR_HOOK("CharConversor", "afterExitConversor", MB, "CheckConversion");
}

/* OnConversorUsed(UsrName, CharName, optId, FullOption): only the target conversor works */
static byte MeetBoss_OnConversorUsed(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e, r;
	(void)out;
	Native_SetNone(&e);
	Native_SetNone(&r);
	int target = Mode_EntityH(&a[1], &e) && Mode_VGet(&e, "RadarShow", &r) ? Mode_CmpI(&r, "==", 1) : -1;
	Native_Free(&e);
	Native_Free(&r);
	if (target > 0) {
		return PORT_CALL(CharConversor, EnterCharConversor, NULL, "vvvv", &a[0], &a[1], &a[2], &a[3]);
	}
	return target == 0 && Pr_Remote("Mission_FirstCrazyBet_RemoteMessage1", "CrazyGambler", NULL, NULL, 0);
}

/* CheckConversion(): out of the conversor as Dtritus: the conversors as usual, on to Humphrey */
static byte MeetBoss_CheckConversion(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr;
	(void)a;
	(void)out;
	Native_SetNone(&usr);
	int dt = Mode_UsrHandle(&usr) ? LvN_CtrlIs(&usr, "Dtritus") : -1;
	Native_Free(&usr);
	if (dt <= 0) {
		return dt == 0;
	}
	SaveVar_Set("Stats.ConvertIntoDtritus", "1");
	return Mode_Target(NULL) && Pr_Conversors("CharConversor.EnterCharConversor") && LvN_ModNone("CharConversor", "afterExitConversor") &&
		Pr_Remote("Mission_FirstCrazyBet_RemoteMessage4", "CrazyGambler", MB, "PreChallengeHumphrey", 0);
}

/* ChallengeHumphrey(): Humphrey is the target */
static byte MeetBoss_ChallengeHumphrey(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t h;
	Native_SetNone(&h);
	(void)a;
	(void)out;
	if (!Mode_Target("Humphrey_Default") || !LvN_Entity("Humphrey_Default", &h)) {
		return 0;
	}
	byte ok = !Native_True(&h) || LvN_SetS(&h, "UseFunc", "Mission_MeetBoss.HumphreySpeech");
	Native_Free(&h);
	return ok;
}

/* v in Chars.CanDriveList: 1, 0, -1 */
static int Pr_Drives(const nativeVal_t *v) {
	nativeVal_t l;
	int r = LvN_ModGet("Chars", "CanDriveList", &l) ? Native_Contains(&l, v) : -1;
	Native_Free(&l);
	return r;
}

/* HumphreySpeech(UsrName, CharName, optId, FullOption): Humphrey takes the challenge from a driver */
static byte MeetBoss_HumphreySpeech(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e, act;
	(void)out;
	if (!Mode_Import("Chars")) {
		return 0;
	}
	Native_SetNone(&e);
	Native_SetNone(&act);
	int drives = Mode_EntityH(&a[0], &e) && Mode_VGet(&e, "ActCtrl", &act) ? Pr_Drives(&act) : -1;
	Native_Free(&e);
	Native_Free(&act);
	if (drives > 0) {
		return LvN_Speakers(&a[0], &a[1]) && LV_ONEND(Mission_MeetBoss, SetNextMission, NULL) &&
			PORT_CALL(Speech, NISpeech, NULL, "s", "Humphrey") &&
			PORT_CALL(Speech, AddNI, NULL, "ss", "Mission_FirstCrazyBet_Humphrey_InitMsg1", "Mission_FirstCrazyBet_Humphrey_InitAns1") &&
			PORT_CALL(Speech, RunNI, NULL, "");
	}
	return drives == 0 && Pr_Remote("Mission_FirstCrazyBet_RemoteMessage10", "CrazyGambler", NULL, NULL, 0);
}

/* SetNextMission(): the race against Humphrey next (in Outskirts in the demo), the crazy wind on; a new game from
 * the way point with a character that can drive */
static byte MeetBoss_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t path, tail, spawn, ch;
	Native_SetNone(&path);
	(void)a;
	(void)out;
	if (!Mode_Import("Init") || !PORT_LOWN(Mission_MeetBoss, ActivateEmero, NULL, "")) {
		return 0;
	}
	SaveVar_Set("CrazyWing.Status", "1");
	if (!Mode_DelSaveVars("MissionState") || !Mode_DelSaveVars("PlayerHasBeenKilled") || !Mode_WayPoint()) {
		return 0;
	}
	int demo = Pr_Demo();
	byte ok;
	if (demo > 0) {
		ok = PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_Demo_HumphreyRace", "Outskirts") &&
			PORT_CALL(SaveGame, MarkWayPoint, NULL, "ss", "Levels/Outskirts", "DM_Player_Spawn_GamblinDen");
	} else if (demo == 0) {
		Native_SetNone(&tail);
		Native_SetNone(&spawn);
		ok = PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_HumphreyRace", "DownTown") && LvN_ModGet("Init", "Path", &path) &&
			Native_Slice(&path, 7, 0x7fffffff, &tail) && LvN_Cat("DM_Player_Spawn_", &tail, &spawn) &&
			PORT_CALL(SaveGame, MarkWayPoint, NULL, "sv", "Levels/DownTown", &spawn);
		Native_Free(&path);
		Native_Free(&tail);
		Native_Free(&spawn);
	} else {
		ok = 0;
	}
	if (!ok || !Mode_Import("Chars")) {
		return 0;
	}
	Mode_SaveVar("Char", &ch);
	int drives = Pr_Drives(&ch);
	Native_Free(&ch);
	if (drives < 0) {
		return 0;
	}
	if (!drives) {
		SaveVar_Set("Char", "Dtritus");
	}
	return PORT_CALL(SaveGame, StartGame, NULL, "");
}

LV_FN(MeetBoss, InitStatusA, 1, MeetBoss_InitStatus)
LV_FN(MeetBoss, AfterBrowseNPCA, 0, MeetBoss_AfterBrowseNPC)
LV_FN(MeetBoss, CreateHumphreyInitA, 1, MeetBoss_CreateHumphreyInit)
LV_FN(MeetBoss, CreateBertoInitA, 1, MeetBoss_CreateBertoInit)
LV_FN(MeetBoss, BertoSpeechA, 4, MeetBoss_BertoSpeech)
LV_FN(MeetBoss, BertoSpeech2A, 4, MeetBoss_BertoSpeech2)
LV_FN(MeetBoss, DeactivateEmeroA, 0, MeetBoss_DeactivateEmero)
LV_FN(MeetBoss, ActivateEmeroA, 0, MeetBoss_ActivateEmero)
LV_FN(MeetBoss, InitA, 1, MeetBoss_Init)
LV_FN(MeetBoss, CreateBatteryA, 0, MeetBoss_CreateBattery)
LV_FN(MeetBoss, AssingOnTakeBatteryA, 1, MeetBoss_AssingOnTakeBattery)
LV_FN(MeetBoss, OnTakeBatteryA, 2, MeetBoss_OnTakeBattery)
LV_FN(MeetBoss, PreBertoPossessionA, 0, MeetBoss_PreBertoPossession)
LV_FN(MeetBoss, BertoPossessionA, 0, MeetBoss_BertoPossession)
LV_FN(MeetBoss, OnWarningA, 0, MeetBoss_OnWarning)
LV_FN(MeetBoss, OnActionPressedA, 1, MeetBoss_OnActionPressed)
LV_FN(MeetBoss, OnPossessionA, 2, MeetBoss_OnPossession)
LV_FN(MeetBoss, ShowMessageA, 0, MeetBoss_ShowMessage)
LV_FN(MeetBoss, OnAlarmEndA, 1, MeetBoss_OnAlarmEnd)
LV_FN(MeetBoss, UseConversorA, 0, MeetBoss_UseConversor)
LV_FN(MeetBoss, OnConversorUsedA, 4, MeetBoss_OnConversorUsed)
LV_FN(MeetBoss, CheckConversionA, 0, MeetBoss_CheckConversion)
LV_FN(MeetBoss, ChallengeHumphreyA, 0, MeetBoss_ChallengeHumphrey)
LV_FN(MeetBoss, HumphreySpeechA, 4, MeetBoss_HumphreySpeech)
LV_FN(MeetBoss, SetNextMissionA, 0, MeetBoss_SetNextMission)

PyMethodDef g_PressMeetBossMethods[] = {
	{ "InitStatus", MeetBoss_InitStatusA, METH_VARARGS, NULL },
	{ "AfterBrowseNPC", MeetBoss_AfterBrowseNPCA, METH_VARARGS, NULL },
	{ "CreateHumphreyInit", MeetBoss_CreateHumphreyInitA, METH_VARARGS, NULL },
	{ "CreateBertoInit", MeetBoss_CreateBertoInitA, METH_VARARGS, NULL },
	{ "BertoSpeech", MeetBoss_BertoSpeechA, METH_VARARGS, NULL },
	{ "BertoSpeech2", MeetBoss_BertoSpeech2A, METH_VARARGS, NULL },
	{ "DeactivateEmero", MeetBoss_DeactivateEmeroA, METH_VARARGS, NULL },
	{ "ActivateEmero", MeetBoss_ActivateEmeroA, METH_VARARGS, NULL },
	{ "Init", MeetBoss_InitA, METH_VARARGS, NULL },
	{ "PlayProlog0", (PyCFunction)(void (*)(void))MeetBoss_PlayProlog0, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "PlayProlog1", (PyCFunction)(void (*)(void))MeetBoss_PlayProlog1, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "PlayProlog2", (PyCFunction)(void (*)(void))MeetBoss_PlayProlog2, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CreateBattery", MeetBoss_CreateBatteryA, METH_VARARGS, NULL },
	{ "AssingOnTakeBattery", MeetBoss_AssingOnTakeBatteryA, METH_VARARGS, NULL },
	{ "OnTakeBattery", MeetBoss_OnTakeBatteryA, METH_VARARGS, NULL },
	{ "PreBertoPossession", MeetBoss_PreBertoPossessionA, METH_VARARGS, NULL },
	{ "BertoPossession", MeetBoss_BertoPossessionA, METH_VARARGS, NULL },
	{ "OnWarning", MeetBoss_OnWarningA, METH_VARARGS, NULL },
	{ "OnActionPressed", MeetBoss_OnActionPressedA, METH_VARARGS, NULL },
	{ "OnPossession", MeetBoss_OnPossessionA, METH_VARARGS, NULL },
	{ "ShowMessage", MeetBoss_ShowMessageA, METH_VARARGS, NULL },
	{ "OnAlarmEnd", MeetBoss_OnAlarmEndA, METH_VARARGS, NULL },
	{ "UseConversor", MeetBoss_UseConversorA, METH_VARARGS, NULL },
	{ "OnConversorUsed", MeetBoss_OnConversorUsedA, METH_VARARGS, NULL },
	{ "CheckConversion", MeetBoss_CheckConversionA, METH_VARARGS, NULL },
	{ "ChallengeHumphrey", MeetBoss_ChallengeHumphreyA, METH_VARARGS, NULL },
	{ "HumphreySpeech", MeetBoss_HumphreySpeechA, METH_VARARGS, NULL },
	{ "SetNextMission", MeetBoss_SetNextMissionA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_TalkToBoss ---- */

/* InitStatus(LevelPath): freeing the human: the human at the bureau (by the elevator or the metro), the alarm on;
 * the boss' callback */
static byte TalkToBoss_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const saves[] = { "AlarmActive", "1", "AlarmStatus", "1", "AlarmStatus", "1", "Char", "Dtritus", NULL };
	(void)a;
	(void)out;
	if (LvN_SaveIs("MissionState", "FreeHuman2")) {
		if (!LV_MISSIONCHAR("Human_Default", Mission_TalkToBoss, CreateHuman)) {
			return 0;
		}
		Mode_Saves(saves);
		if (LvN_SaveIs("ComeFrom", "DoorElevator")) {
			SaveVar_Set("ComeFrom", "DoorElevator");
			if (!Pr_ResSet("Human_Default", "Pos", -1361, -1285, -1130) || !Pr_ResSet("Human_Default", "Ang", 0, 0, 0)) {
				return 0;
			}
		} else {
			SaveVar_Set("ComeFrom", "DoorMetro");
		}
	}
	return LvN_ModAppendG("CharsNPC", "AfterBrowseNPC", TB, "AfterBrowseNPC");
}

/* AfterBrowseNPC() */
static byte TalkToBoss_AfterBrowseNPC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("CharsNPC") && LV_CALLBACK("Boss_00", Mission_TalkToBoss, SetBossSpeech);
}

/* SetBossSpeech(e) */
static byte TalkToBoss_SetBossSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return !LvN_SaveIs("MissionState", "TalkToBoss") || LvN_SetS(&a[0], "UseFunc", "Mission_TalkToBoss.BossSpeech");
}

/* CreateHuman(e) */
static byte TalkToBoss_CreateHuman(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Pr_CreateHuman(&a[0], TB, 1);
}

/* Init(LevelPath): Rusty's head; the human to hide in the bureau, or the talk with the boss */
static byte TalkToBoss_Init(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	/* SScorer.AddModel by name: the cases record it (OBJLIB 7) */
	if (!LV_CALL("SScorer", "AddModel", NULL, "isssii", 0L, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3L, 25L)) {
		return 0;
	}
	if (LvN_SaveIs("MissionState", "FreeHuman2")) {
		return Mode_Import("MissionsFuncs") && LV_OWN(TB, "LoadScene", NULL, "") && PORT_LOWN(Mission_TalkToBoss, HideHumanIntoBureau, NULL, "") &&
			LvN_GActivate(TB, "human", 2);
	}
	return PORT_LOWN(Mission_TalkToBoss, GoTalkToBoss, NULL, "");
}

/* HideHumanIntoBureau(): the bureau is the target and closed; the human follows Dtritus, the alarm does not fall */
static byte TalkToBoss_HideHumanIntoBureau(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t bure, usr, fall;
	Native_SetNone(&bure);
	Native_SetNone(&fall);
	(void)a;
	(void)out;
	if (!Mode_Target("DM_Door_Bureau_00") || !LvN_Entity("DM_Door_Bureau_00", &bure)) {
		return 0;
	}
	byte ok = LvN_SetS(&bure, "OnObjSlot", "") && LvN_SetS(&bure, "Action", "Closed");
	Native_Free(&bure);
	Native_SetNone(&usr);
	int u = ok ? LvN_Usr(&usr) : -1;
	if (u > 0) {
		ok = LvN_HumanFollow(TB, &usr) && PR_HOOK("CharConversor", "OnPossession", TB, "OnPossession") &&
			PR_HOOK("CharConversor", "afterExitConversor", TB, "OnExitConversor") && PORT_LOWN(Mission_TalkToBoss, CreateSceneLaunchers, NULL, "") &&
			PORT_LOWN(Mission_TalkToBoss, CheckMaxDist, NULL, "") && Native_GVarGet("AlarmFallDelta", &fall) &&
			Mode_Put(TB, "alarmFallDelta", NULL, &fall) &&
			Mode_GVarSetI("AlarmFallDelta", 0);
		Native_Free(&fall);
	}
	Native_Free(&usr);
	return ok && u >= 0;
}

/* CreateSceneLaunchers(): an item at each door of the bureau room starts the scene when the human gets there */
static byte TalkToBoss_CreateSceneLaunchers(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	for (int i = 1; i <= 2; i++) {
		char name[0x40];
		nativeVal_t dummy, pos, n, l;
		Str_Printf(name, sizeof(name), "DM_Bureau_Room_Door_%d", i);
		LvN_Dummy(name, 1, &dummy);
		if (!Native_True(&dummy)) {
			Native_Free(&dummy);
			Str_Printf(name, sizeof(name), "No se encuentra el dummy: DM_Bureau_Room_Door_%d", i);
			return LvN_RaiseText(name);
		}
		Str_Printf(name, sizeof(name), "SceneLauncher%d", i);
		LvN_Str(&n, name);
		Native_SetNone(&pos);
		Native_SetNone(&l);
		byte ok = LvN_Item(&dummy, 1, &pos) && LvN_CreateAt(&n, &pos, "Item", &l) && LvN_SetI(&l, "OnlyUser", 0) && LvN_SetI(&l, "Radius", 250) &&
			LvN_SetS(&l, "OnTake", "Mission_TalkToBoss.LaunchScene");
		Native_Free(&dummy);
		Native_Free(&pos);
		Native_Free(&n);
		Native_Free(&l);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* Scrap.GetEntity(name).Del() of a C name */
static byte Pr_DelS(const char *name) {
	nativeVal_t e;
	if (!LvN_Entity(name, &e)) {
		return 0;
	}
	byte ok = Mode_VDel(&e);
	Native_Free(&e);
	return ok;
}

/* the boss of the global bossName gets the boss' talk when he is there */
static byte Tb_BossTalk(void) {
	nativeVal_t n, boss;
	Native_SetNone(&boss);
	byte ok = Mode_G(TB, "bossName", &n) && Mode_EntityH(&n, &boss) && (!Native_True(&boss) || LvN_SetS(&boss, "UseFunc", "Mission_TalkToBoss.BossSpeech"));
	Native_Free(&n);
	Native_Free(&boss);
	return ok;
}

/* LaunchScene(ItemName, CharName): the human reaches the room: the alarm off, the scene of the human hiding */
static byte TalkToBoss_LaunchScene(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t hn, usr, zero, fall, h;
	Native_SetNone(&fall);
	Native_SetNone(&h);
	(void)out;
	Native_SetNone(&usr);
	int other = LvN_GAttr(TB, "human", "Name", &hn) ? Mode_Cmp(&a[1], "!=", &hn) : -1;
	Native_Free(&hn);
	if (other) {
		return other > 0;
	}
	int u = LvN_Usr(&usr);
	Native_Free(&usr);
	if (u <= 0) {
		return u == 0;
	}
	Native_SetInt(&zero, 0);
	if (!Mode_SetAlarm(&zero) || !Mode_G(TB, "alarmFallDelta", &fall)) {
		return 0;
	}
	byte ok = Mode_GVarSet("AlarmFallDelta", &fall);
	Native_Free(&fall);
	if (!ok || !Mode_Target(NULL) || !Mode_DelSchedS("CheckMaxDist") || !Pr_DelS("SceneLauncher1") || !Pr_DelS("SceneLauncher2") ||
		!Mode_G(TB, "human", &h)) {
		return 0;
	}
	ok = Mode_VDel(&h);
	Native_Free(&h);
	return ok && LvN_PutNone(TB, "human") && Tb_BossTalk() && PORT_CALL(Scene, Play, NULL, "s", "HideHuman") &&
		PR_HOOK("Scene", "EndOfSceneCallback", TB, "EndOfScene");
}

/* EndOfScene(usr=None): the bureau closed; the boss calls */
static byte Tb_EndOfScene(void) {
	nativeVal_t e;
	if (!LvN_Entity("DM_Door_Bureau_00", &e)) {
		return 0;
	}
	byte ok = LvN_SetS(&e, "ForceAction", "Closed") && LvN_SetS(&e, "ForceDefLoop", "Closed");
	Native_Free(&e);
	return ok && LvN_ModNone("CharConversor", "OnPossession") && LvN_ModNone("CharConversor", "afterExitConversor") &&
		Pr_Remote("Mission_SetHumanFree_RemoteMessage1", "Boss", TB, "PreGoTalkToBoss", 1);
}

PR_USRFN(TalkToBoss_EndOfScene, Tb_EndOfScene)

/* OnPossession(newCharName, pos), OnExitConversor() */
static byte TalkToBoss_OnPossession(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_HumanOnPossession(TB, "Mission_SetHumanFree_RemoteMessage2");
}

static byte TalkToBoss_OnExitConversor(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_HumanOnExitConversor(TB, "Mission_SetHumanFree_RemoteMessage2");
}

/* CheckMaxDist(): every second: the human stops when Dtritus is too far, else follows him */
static byte TalkToBoss_CheckMaxDist(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr, r, hn;
	Native_SetNone(&hn);
	(void)a;
	(void)out;
	if (!Mode_Later(1.0, TB, "CheckMaxDist", NULL, 0, "CheckMaxDist")) {
		return 0;
	}
	Native_SetNone(&usr);
	Native_SetNone(&r);
	int u = LvN_Usr(&usr);
	byte ok = u >= 0;
	if (u > 0) {
		int near = PORT_LOWN(Mission_TalkToBoss, CheckHumanDist, &r, "") ? Native_True(&r) : -1;
		if (near == 0) {
			int moving = Pr_HumanState(TB, "!=", 0);
			ok = moving >= 0 && (!moving || (LvN_GAttr(TB, "human", "Name", &hn) && CHAR_STATE("iv", 0L, &hn) &&
				Pr_Remote("Mission_SetHumanFree_RemoteMessage2", "Human", NULL, NULL, 0)));
			Native_Free(&hn);
		} else if (near > 0) {
			int dt = LvN_CtrlIs(&usr, "Dtritus");
			ok = dt >= 0 && (!dt || LvN_HumanFollow(TB, &usr));
		} else {
			ok = 0;
		}
	}
	Native_Free(&usr);
	Native_Free(&r);
	return ok;
}

/* CheckHumanDist() */
static byte TalkToBoss_CheckHumanDist(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	return Pr_CheckHumanDist(TB, out);
}

/* GoTalkToBoss(): the boss is the target */
static byte TalkToBoss_GoTalkToBoss(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t n, boss;
	(void)a;
	(void)out;
	Native_SetNone(&boss);
	int there = Mode_G(TB, "bossName", &n) && Mode_EntityH(&n, &boss) ? Native_True(&boss) : -1;
	Native_Free(&n);
	Native_Free(&boss);
	if (there <= 0) {
		return there == 0;
	}
	byte ok = Mode_G(TB, "bossName", &n) && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &n);
	Native_Free(&n);
	return ok;
}

/* BossSpeech(UsrName, CharName, optId, FullOption): the boss' talk; its answer ends the mission while he is the
 * target */
static byte TalkToBoss_BossSpeech(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const ans[] = { "Mission_TalkToBoss_Boss_1" };
	nativeVal_t usr, n, boss, r, params;
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	Native_SetNone(&usr);
	int dt = Mode_UsrHandle(&usr) ? LvN_CtrlIs(&usr, "Dtritus") : -1;
	Native_Free(&usr);
	if (dt <= 0) {
		return dt == 0 && Mode_Spch("Mission_TalkToBoss_Boss_InitMsg2", "Mission_TalkToBoss_Boss_InitAns2", NULL);
	}
	if (LvN_EqS(&a[2], "Talk")) {
		return Mode_AnsLst(ans, 1) && Mode_Spch("Mission_TalkToBoss_Boss_InitMsg1", "Mission_TalkToBoss_Boss_InitAns1", NULL);
	}
	if (!Mode_Spch("Mission_TalkToBoss_Boss_1Msg", "Mission_TalkToBoss_Boss_1Ans", NULL)) {
		return 0;
	}
	Native_SetNone(&boss);
	Native_SetNone(&r);
	int target = Mode_G(TB, "bossName", &n) && Mode_EntityH(&n, &boss) ? Native_True(&boss) : -1;
	if (target > 0) {
		target = Mode_VGet(&boss, "RadarShow", &r) ? Mode_CmpI(&r, "==", 1) : -1;
	}
	Native_Free(&n);
	Native_Free(&boss);
	Native_Free(&r);
	if (target <= 0) {
		return target == 0;
	}
	Native_SetEmpty(&params, NV_TUPLE);
	Native_Copy(Native_Append(&params), &a[1]);
	byte ok = LV_ONEND(Mission_TalkToBoss, SetNextMission, &params);
	Native_Free(&params);
	return ok;
}

/* the blueprint item appended to the save list var (the eval in the globals of module) */
static byte Pr_SaveListAdd(const char *module, const char *var, const char *item) {
	nativeVal_t lst, it;
	LvN_Str(&it, item);
	byte ok = LvN_SaveList(var, module, &lst) && LvN_SaveAppend(var, &lst, &it);
	Native_Free(&lst);
	Native_Free(&it);
	return ok;
}

/* SetNextMission(CharName): the first data part in DownTown next; the police boss' blueprints; Rusty's message in
 * 10 s */
static byte TalkToBoss_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t args[4];
	(void)out;
	if (!Mode_Import("MissionsFuncs") || !Mode_Import("SaveGame") || !PORT_CALL(Speech, RestoreState, NULL, "v", &a[0]) || !Mode_Target(NULL) ||
		!Mode_DelSaveVars("MissionState") || !Mode_WayPoint() ||
		!PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_GetDataPart1", "DownTown") ||
		!Mode_MMDesc("Mission_GetDataPart1_MissionText1", 1) || !Pr_SaveListAdd(TB, "PoliceBlueprints.Ships", "SPoliBoss1") ||
		!Pr_SaveListAdd(TB, "PoliceBlueprints.Engines", "MPoliBoss1") || !Mode_Import("Scorer") ||
		!LvN_ModGet("Scorer", "ShowMissionScorer", &args[2])) {
		return 0;
	}
	LvN_Str(&args[0], "Mission_GetDataPart1_RemoteMessage9");
	LvN_Str(&args[1], "Rusty");
	Native_SetEmpty(&args[3], NV_TUPLE);
	Native_SetInt(Native_Append(&args[3]), 0);
	Native_SetInt(Native_Append(&args[3]), 2);
	byte ok = Mode_LaterIn(10.0, "Speech", "AddRemoteCom", args, 4, NULL);
	for (int i = 0; i < 4; i++) {
		Native_Free(&args[i]);
	}
	return ok;
}


LV_FN(TalkToBoss, InitStatusA, 1, TalkToBoss_InitStatus)
LV_FN(TalkToBoss, AfterBrowseNPCA, 0, TalkToBoss_AfterBrowseNPC)
LV_FN(TalkToBoss, SetBossSpeechA, 1, TalkToBoss_SetBossSpeech)
LV_FN(TalkToBoss, CreateHumanA, 1, TalkToBoss_CreateHuman)
LV_FN(TalkToBoss, InitA, 1, TalkToBoss_Init)
LV_FN(TalkToBoss, HideHumanIntoBureauA, 0, TalkToBoss_HideHumanIntoBureau)
LV_FN(TalkToBoss, CreateSceneLaunchersA, 0, TalkToBoss_CreateSceneLaunchers)
LV_FN(TalkToBoss, LaunchSceneA, 2, TalkToBoss_LaunchScene)
LV_FN(TalkToBoss, OnPossessionA, 2, TalkToBoss_OnPossession)
LV_FN(TalkToBoss, OnExitConversorA, 0, TalkToBoss_OnExitConversor)
LV_FN(TalkToBoss, CheckMaxDistA, 0, TalkToBoss_CheckMaxDist)
LV_FN(TalkToBoss, CheckHumanDistA, 0, TalkToBoss_CheckHumanDist)
LV_FN(TalkToBoss, GoTalkToBossA, 0, TalkToBoss_GoTalkToBoss)
LV_FN(TalkToBoss, BossSpeechA, 4, TalkToBoss_BossSpeech)
LV_FN(TalkToBoss, SetNextMissionA, 1, TalkToBoss_SetNextMission)

PyMethodDef g_PressTalkToBossMethods[] = {
	{ "InitStatus", TalkToBoss_InitStatusA, METH_VARARGS, NULL },
	{ "AfterBrowseNPC", TalkToBoss_AfterBrowseNPCA, METH_VARARGS, NULL },
	{ "SetBossSpeech", TalkToBoss_SetBossSpeechA, METH_VARARGS, NULL },
	{ "CreateHuman", TalkToBoss_CreateHumanA, METH_VARARGS, NULL },
	{ "Init", TalkToBoss_InitA, METH_VARARGS, NULL },
	{ "HideHumanIntoBureau", TalkToBoss_HideHumanIntoBureauA, METH_VARARGS, NULL },
	{ "CreateSceneLaunchers", TalkToBoss_CreateSceneLaunchersA, METH_VARARGS, NULL },
	{ "LaunchScene", TalkToBoss_LaunchSceneA, METH_VARARGS, NULL },
	{ "EndOfScene", (PyCFunction)(void (*)(void))TalkToBoss_EndOfScene, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "OnPossession", TalkToBoss_OnPossessionA, METH_VARARGS, NULL },
	{ "OnExitConversor", TalkToBoss_OnExitConversorA, METH_VARARGS, NULL },
	{ "CheckMaxDist", TalkToBoss_CheckMaxDistA, METH_VARARGS, NULL },
	{ "CheckHumanDist", TalkToBoss_CheckHumanDistA, METH_VARARGS, NULL },
	{ "GoTalkToBoss", TalkToBoss_GoTalkToBossA, METH_VARARGS, NULL },
	{ "BossSpeech", TalkToBoss_BossSpeechA, METH_VARARGS, NULL },
	{ "SetNextMission", TalkToBoss_SetNextMissionA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_WhoIsDeepThroat ---- */

/* Init(LevelPath): the teleporter to the boss room is the target; its use starts the scene */
static byte DeepThroat_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t elv;
	(void)a;
	(void)out;
	Native_SetNone(&elv);
	byte ok = Pr_BossDoor("DtritusGroupGateKeeper", "Mission_WhoIsDeepThroat.UseTeleporter", &elv) && LvN_TargetName(&elv) &&
		LV_OWN(WD, "LoadScene", NULL, "");
	Native_Free(&elv);
	return ok;
}

/* UseTeleporter(UsrName, ElevatorName, optId, FullOption) */
static byte DeepThroat_UseTeleporter(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t r;
	(void)out;
	Native_SetNone(&r);
	int used = PORT_CALL(GateKeeper, UseDoor, &r, "vvvv", &a[0], &a[1], &a[2], &a[3]) ? Native_True(&r) : -1;
	Native_Free(&r);
	return used >= 0 && (!used || (PR_HOOK("Elevator", "OnElevatorReachDown", WD, "LaunchScene") && Mode_Target("")));
}

/* LaunchScene(charName=""): the teleporter arrived: the scene in 0.1 s */
static byte Wd_LaunchScene(void) {
	return LvN_ModNone("Elevator", "OnElevatorReachDown") && Mode_Target("") && PR_LATER(0.1, WD, "PlayScene");
}

static PyObject *DeepThroat_LaunchScene(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "charName", NULL };
	PyObject *name = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &name)) {
		return NULL;
	}
	return Mode_Result(Wd_LaunchScene());
}

/* PlayScene(): Deep Throat discovered */
static byte DeepThroat_PlayScene(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return PORT_CALL(Scene, Play, NULL, "s", "DiscoverDeep") && Mode_EntSetS("DM_Door_PressSlidingDoor_00", "OnObjSlot", "") &&
		PR_HOOK("Scene", "EndOfSceneCallback", WD, "SetNextMission");
}


LV_FN(DeepThroat, InitA, 1, DeepThroat_Init)
LV_FN(DeepThroat, UseTeleporterA, 4, DeepThroat_UseTeleporter)
LV_FN(DeepThroat, PlaySceneA, 0, DeepThroat_PlayScene)

PyMethodDef g_PressWhoIsDeepThroatMethods[] = {
	{ "Init", DeepThroat_InitA, METH_VARARGS, NULL },
	{ "UseTeleporter", DeepThroat_UseTeleporterA, METH_VARARGS, NULL },
	{ "LaunchScene", (PyCFunction)(void (*)(void))DeepThroat_LaunchScene, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "PlayScene", DeepThroat_PlaySceneA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_WhoIsTheBaddie ---- */

/* InitStatus(LevelPath): the bad reputation and the pilots' profile while the boss rules the robots (the old values
 * kept once), nobody at the GamblinDen; the start in the boss room before the scene */
static byte Baddie_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const prev[][3] = { { "PrevBadReputation", "BadReputation", "BAD_REPUTATION" },
		{ "PrevStdShipAIProfile", "StdShipAIProfile", "STD_SHIPS_PROFILE" } };
	static const char *const saves[] = { "RobotsControlledByBoss", "1", "BankMasterAtBank", "0", "MayorAtGambinDen", "0",
		"FunctionaryTwinsAtGambinDen", "0", "BankersAtGambinDen", "0", "BishopsAtGambinDen", "0", "PolicesAtGambinDen", "0", "Traffic.AcShips",
		"[\"SBanker1\",\"SBishop1\",\"SFunc1\"]", NULL };
	static const char *const scene[] = { "ComeFrom", "", "Char", "Dtritus", "Spawn", "DM_Mission_Berto_MeetBos", NULL };
	for (int i = 0; i < 2; i++) {
		nativeVal_t v;
		Mode_SaveVar(prev[i][0], &v);
		byte none = v.type == NV_NONE;
		Native_Free(&v);
		if (none) {
			Mode_SaveVar(prev[i][1], &v);
			byte ok = Mode_SetSaveVar(prev[i][0], &v);
			Native_Free(&v);
			ok = ok && Mode_G(WB, prev[i][2], &v) && Mode_SetSaveVar(prev[i][1], &v);
			Native_Free(&v);
			if (!ok) {
				return 0;
			}
		}
	}
	Mode_Saves(saves);
	(void)out;
	if (!LvN_SaveIs("MissionState", "TalkToBoss")) {
		return 1;
	}
	Mode_Saves(scene);
	return LV_OWN(WB, "InitStatusScene", NULL, "v", &a[0]);
}

/* Init(LevelPath): the boss room open to all; the scene of the boss, or the way to the human in the bureau */
static byte Baddie_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t elv;
	(void)out;
	Native_SetNone(&elv);
	byte ok = Pr_BossDoor("NoOneGroupGateKeeper", "GateKeeper.UseDoor", &elv);
	if (ok && LvN_SaveIs("MissionState", "TalkToBoss")) {
		ok = LvN_TargetName(&elv) && LV_OWN(WB, "InitScene", NULL, "v", &a[0]) && PORT_LOWN(Mission_WhoIsTheBaddie, PlayScene, NULL, "");
	} else if (ok) {
		ok = PORT_LOWN(Mission_WhoIsTheBaddie, GoForHuman, NULL, "");
	}
	Native_Free(&elv);
	return ok;
}

/* GoForHuman(): a usable arrow at the bureau is the target */
static byte Baddie_GoForHuman(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *pos = Mode_V(&t), *en = Mode_V(&t), *name = Mode_V(&t), *e2 = Mode_V(&t), *cam = Mode_V(&t), *opt = Mode_V(&t);
	nativeVal_t *use = Mode_V(&t);
	(void)a;
	(void)out;
	Mode_Ints3(cam, 0, 100, 0);
	Mode_Lang("USE_Bureau", use);
	byte ok = LvN_Entity("DM_Door_Bureau_00", e) && Mode_VGet(e, "Pos", pos) && Mode_VGet(e, "Name", en) &&
		Native_Arith('+', en, Mode_S(&t, "CheckArrow"), name) && LvN_CreateAt(name, pos, "WithLife", e2) && LvN_SetI(e2, "ArrowCanUse", 1) &&
		LvN_SetI(e2, "UseAngSel", 1) && LvN_SetI(e2, "UseMapCheck", 0) && LvN_SetI(e2, "VRadius", 60) && LvN_SetI(e2, "ArrnOpt", 0) &&
		Native_Arith('+', Mode_S(&t, "Use:"), use, opt) && Mode_VSet(e2, "ArrOpt0", opt) && Mode_VSet(e2, "CamTgt", cam) &&
		LvN_SetS(e2, "UseFunc", "Mission_WhoIsTheBaddie.OnBureauUsed") && LvN_TargetName(e2);
	return Mode_Done(&t, ok);
}

/* OnBureauUsed(UsrName, CharName, optId, FullOption): the bureau opens, the gambler's message */
static byte Baddie_OnBureauUsed(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t bureau, b;
	Native_SetNone(&b);
	(void)out;
	if (!Mode_EntityH(&a[1], &bureau)) {
		return 0;
	}
	byte ok = LvN_SetI(&bureau, "ArrowCanUse", 0) && LvN_SetS(&bureau, "UseFunc", "");
	Native_Free(&bureau);
	if (!ok || !Mode_Target(NULL) || !Pr_Remote("Mission_TalkToHuman_RemoteMessage1", "CrazyGambler", WB, "SetNextMission", 1) ||
		!LvN_Entity("DM_Door_Bureau_00", &b)) {
		return 0;
	}
	ok = LvN_SetS(&b, "OnObjSlot", "") && LvN_SetS(&b, "Action", "Open") && LvN_SetS(&b, "DefLoop", "Opened");
	Native_Free(&b);
	return ok;
}

/* PlayScene(): the boss discovered, with his and Betty's heads on the news panel */
static byte Baddie_PlayScene(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t zero;
	(void)a;
	(void)out;
	Native_SetInt(&zero, 0);
	return Mode_SetLockAlarm(&zero) && Mode_SetAlarm(&zero) && PORT_LOWN(Mission_WhoIsTheBaddie, CreateFakeSpeech, NULL, "") &&
		PORT_CALL(Scene, Play, NULL, "s", "BossDiscovered") && Mode_EntSetS("DM_Door_PressSlidingDoor_00", "OnObjSlot", "") &&
		PR_HOOK("Scene", "EndOfSceneCallback", WB, "EndOfScene") && Mode_GVarSetI("useDynamicMusic", 0) && LvN_SetMusic("Music/HumanCD1.ogg", 1.0);
}

/* EndOfScene(usr): the heads go, the alarm */
static byte Baddie_EndOfScene(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t one;
	(void)a;
	(void)out;
	Native_SetInt(&one, 1);
	return Mode_Fade(0, lv_black, lv_clear, 3.0) && Mode_GVarSetS("NewsHead", "Betty") && Pr_DelS("FakeSpeech") && Pr_DelS("FakeSpeech2") &&
		Mode_GVarSetI("NewsPanelShowHead", 0) && LV_OWN(WB, "AfterScene", NULL, "") && Mode_GVarSetI("useDynamicMusic", 1) && Mode_SetAlarm(&one);
}

/* CreateFakeSpeech(): the angry boss and the alarmed Betty for the news panel */
static byte Baddie_CreateFakeSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_FakeSpeech("FakeSpeech", "Boss", "TalkAngry") && Pr_FakeSpeech("FakeSpeech2", "Betty", "TalkAlarmed") &&
		Pr_FakeSpeechShow() && PORT_LOWN(Mission_WhoIsTheBaddie, NormCamera, NULL, "");
}

static const char *const baddie_scene[] = {
	"Scene.Load|BossDiscovered",
	"Scene.ObjAction|0|DM_Door_PressSlidingDoor_00|Opened|Opened",
	"Scene.ObjAction|0|DM_Door_Teleporter_07|Close|Closed",
	"Scene.ActorAction|0|Dtritus|Run",
	"Scene.ActorAction|90|Dtritus|Run",
	"Scene.ActorAction|195|Dtritus|Relax",
	"Scene.ActorAction|230|Dtritus|LookUp",
	"Scene.ActorAction|312|Dtritus|Relax",
	"Scene.ActorAction|500|Dtritus|Talk",
	"Scene.ActorAction|510|Dtritus|TalkEnergy",
	"Scene.ActorAction|570|Dtritus|Talk",
	"Scene.ActorAction|596|Dtritus|Relax",
	"Scene.ActorAction|792|Dtritus|Talk1",
	"Scene.ActorAction|810|Dtritus|Relax",
	"Scene.ActorAction|825|Dtritus|RelaxLeft",
	"Scene.ActorAction|845|Dtritus|Walk",
	"Scene.ActorAction|925|Dtritus|Relax",
	"Scene.ActorAction|1215|Dtritus|Talk",
	"Scene.ActorAction|1300|Dtritus|Talk1",
	"Scene.ActorAction|1350|Dtritus|Talk",
	"Scene.ActorAction|1410|Dtritus|Relax",
	"Scene.ActorAction|1430|Dtritus|RelaxRight",
	"Scene.ActorAction|1445|Dtritus|Walk",
	"Scene.ActorAction|1500|Dtritus|RelaxUp",
	"Scene.ActorAction|1660|Dtritus|RelaxLeft",
	"Scene.ActorAction|1680|Dtritus|Alarm",
	"Scene.ActorAction|1960|Dtritus|RelaxUp",
	"Scene.ActorAction|0|MonitorCharacter|Relax",
	"Scene.ActorAction|0|MonitorNoise|Relax",
	"Scene.AddGeneric|75|SScorer.Fade(0,(0,0,0,0),(0,0,0,255),0.5)",
	"Scene.AddGeneric|90|SScorer.Fade(0,(0,0,0,255),(0,0,0,0),0.5)",
	"Scene.AddGeneric|2230|SScorer.Fade(0,(0,0,0,0),(0,0,0,255),1.5)",
	"Scene.AddGeneric|310|Scrap.Set(\"NewsPanelShowHead\",1)",
	"!500|NearCamera",
	"!800|NormCamera",
	"!1000|NearCamera",
	"!1400|EyeCamera",
	"!1600|EyeCamera2",
	"!1800|BettyCamera",
	"!1960|NearCamera",
	"Scene.AddText|310|Mission_Scene_MyBossIsTheBaddie_1|6",
	"Scene.AddText|500|Mission_Scene_MyBossIsTheBaddie_2|3",
	"Scene.AddText|605|Mission_Scene_MyBossIsTheBaddie_3|6",
	"Scene.AddText|792|Mission_Scene_MyBossIsTheBaddie_4|1",
	"Scene.AddText|820|Mission_Scene_MyBossIsTheBaddie_5|13",
	"Scene.AddText|1215|Mission_Scene_MyBossIsTheBaddie_6|7",
	"Scene.AddText|1425|Mission_Scene_MyBossIsTheBaddie_7|8",
	"Scene.AddText|1850|Mission_Scene_MyBossIsTheBaddie_8|4",
	"Scene.AddText|1960|Mission_Scene_MyBossIsTheBaddie_9|9",
	"Scrap.Set|DefInterpTime|0.7",
	"Scene.AddCharActor|Dtritus|Dtritus",
	"MakeChar.CreateRelaxAction|Dtritus|0|Walk|Walk|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk1|Talk1|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Run|Run|1",
	"Scene.CreateLocalAction|Dtritus|0|LookUp|LookUp|1",
	"Scene.CreateLocalAction|Dtritus|0|RelaxUp|RelaxUp|1",
	"Scene.CreateLocalAction|Dtritus|0|RelaxLeft|RelaxLeft|1",
	"Scene.CreateLocalAction|Dtritus|0|RelaxRight|RelaxRight|1",
	"Scene.CreateLocalAction|Dtritus|0|Alarm|Alarm|1",
	"Scene.CreateLocalAction|Dtritus|0|TalkEnergy|TalkEnergy|1",
	"Scrap.Set|DefInterpTime|0.4",
	"Scene.SetDefSounds|Dtritus|Dtritus",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddLocalActor|MonitorNoise|MonitorNoise",
	"Scene.CreateLocalAction|MonitorNoise|0|Relax|Relax|1",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddLocalActor|MonitorCharacter|MonitorCharacter",
	"Scene.CreateLocalAction|MonitorCharacter|0|Relax|Relax|1",
	"SAct.SetCls|DefAction|Relax",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

/* LoadScene(): the scene BossDiscovered */
static byte Baddie_LoadScene(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Pr_Scene(WB, baddie_scene);
}


LV_FN(Baddie, InitStatusA, 1, Baddie_InitStatus)
LV_FN(Baddie, InitA, 1, Baddie_Init)
LV_FN(Baddie, GoForHumanA, 0, Baddie_GoForHuman)
LV_FN(Baddie, OnBureauUsedA, 4, Baddie_OnBureauUsed)
LV_FN(Baddie, PlaySceneA, 0, Baddie_PlayScene)
LV_FN(Baddie, EndOfSceneA, 1, Baddie_EndOfScene)
LV_FN(Baddie, CreateFakeSpeechA, 0, Baddie_CreateFakeSpeech)
LV_FN(Baddie, LoadSceneA, 0, Baddie_LoadScene)

PyMethodDef g_PressWhoIsTheBaddieMethods[] = {
	{ "InitStatus", Baddie_InitStatusA, METH_VARARGS, NULL },
	{ "Init", Baddie_InitA, METH_VARARGS, NULL },
	{ "GoForHuman", Baddie_GoForHumanA, METH_VARARGS, NULL },
	{ "OnBureauUsed", Baddie_OnBureauUsedA, METH_VARARGS, NULL },
	{ "PlayScene", Baddie_PlaySceneA, METH_VARARGS, NULL },
	{ "EndOfScene", Baddie_EndOfSceneA, METH_VARARGS, NULL },
	{ "CreateFakeSpeech", Baddie_CreateFakeSpeechA, METH_VARARGS, NULL },
	{ "NormCamera", Pr_NormCameraA, METH_VARARGS, NULL },
	{ "SetTalk", Pr_SetTalkA, METH_VARARGS, NULL },
	{ "LoadScene", Baddie_LoadSceneA, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_Press)
