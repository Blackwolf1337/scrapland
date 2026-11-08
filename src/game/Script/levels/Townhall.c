/*
 * The scripts of the level Townhall in C (data02.packed levels/townhall/scripts, issue #2): the Map module (the
 * NPCs, the mayor and his gear, the elevators, the locked elevator to the orbit, the fake traffic),
 * MapSnd.SndElements (the sounds of the map elements; the rest of MapSnd is data) and the missions
 * Mission_ApologizeMayor (the staplers of the crazy mayor), Mission_CatchTrurl (the messenger and the blueprints of
 * the mayor's ship), Mission_SheriffMayor (the police boss sells his clues) and Mission_TalkToMayor (the money of
 * Corruptus and Porkins). The plain functions are data (data/levels/townhall/<module>.def):
 * Mission_SheriffMayor.Init and Mission_TalkToMayor.PreStealFunctionary.
 *
 * The module variables and the module code stay in the scripts: Map (Libraries, the discardable characters
 * _BankDirector, _Bishop, _Functionary), Mission_ApologizeMayor (MaxTargets, SMayorTaunt), Mission_SheriffMayor
 * (NextMissionData, MoneyReward, alreadyTalked), Mission_TalkToMayor (MAX_TAMAYO_MONEY, MIN_TAMAYO_MONEY,
 * functionaryTamayoName, porkinsName, porkinsMoney, tamayoMoney, porkinsHasMoney, tamayoStartOrigin,
 * tamayoHasJustReceivedMoney). Mission_CatchTrurl keeps its state in save variables.
 *
 * The functions are native C (docs/objlib-plan.md, stage 2): they work on C values, read the variables of the
 * missions where the script reads them and write them back to the module at once (OBJLIB 5), and call the engine in C:
 * save variables, texts, money, entity variables, the scorer; Scrap.UsrEntity, Scrap.Set, Scrap.SetLockAlarm,
 * Scrap.CreateEntity, Scrap.AddScheduledFunc, Scrap.DeleteScheduledFuncs, SScorer.SetMsgText, SetLabelText, Set and
 * the SAct functions through their seams (OBJLIB 7). Entities are handle values (OBJLIB 7). The calls of the other
 * ports (Speech, MissionsFuncs, SaveGame, CharsNPC, Elevator, Metro, InTraffic, Items, RaceMaker, Fx, Scorer, Menu,
 * string) and the module's own functions go by name (OBJLIB 3), CharsNPC.CharRes and Speech.SpeechRoutines are read
 * the Python way (OBJLIB 5), the scheduler keeps Python functions (OBJLIB 4). MapSnd.SndElements is LevelCommon's
 * shared Lv_SndElements, native since batch F (Lv_SndCore on values).
 */
#include "scrap/scrap.h"
#include "scrap/scorer.h"
#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL townhall

#define MAP "Map"
#define AM "Mission_ApologizeMayor"
#define CT "Mission_CatchTrurl"
#define SM "Mission_SheriffMayor"
#define TM "Mission_TalkToMayor"

/* ---- native helpers of this file ---- */

/* "TalkNPC" + `int(Scrap.Rand(0, 1.9))`: one of the two talk animations (the engine's random directly, as the port
 * did) */
static void Th_TalkAnim(nativeVal_t *out) {
	char s[16];
	Str_Printf(s, sizeof(s), "TalkNPC%ld", (long)Scrap_RandRange(0, 1.9));
	LvN_Str(out, s);
}

/* cmp(a, b) of the script: -1, 0 or 1, 2 with the error (only values of other types compared by Python can fail) */
static int Th_Cmp(const nativeVal_t *a, const nativeVal_t *b) {
	int c = Native_Compare(a, b);
	return PyErr_Occurred() ? 2 : c;
}

/* the character stays where it is (Lv_Stay with state 6 and no target) */
static byte Th_Stay6(const nativeVal_t *e) {
	nativeVal_t empty = { 0 };
	LvN_Str(&empty, "");
	return LvN_Stay(e, 6, &empty);
}

/* the optional keyword argument name of a function (def: its default text) into out */
static byte Th_KwArg(PyObject *args, PyObject *kw, char *name, const char *def, nativeVal_t *out) {
	char *names[] = { name, NULL };
	PyObject *o = NULL;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &o)) {
		return 0;
	}
	if (o) {
		Native_Arg(o, out);
	} else {
		LvN_Str(out, def);
	}
	return 1;
}

/* the adapter of a function with one keyword argument (core on the value) */
#define TH_KWFN(fn, name, def, core) \
	static PyObject *fn(PyObject *self, PyObject *args, PyObject *kw) { \
		nativeVal_t v = { 0 }; \
		(void)self; \
		if (!Th_KwArg(args, kw, name, def, &v)) { \
			return NULL; \
		} \
		byte ok = core(&v); \
		Native_Free(&v); \
		return Mode_Result(ok); \
	}

/* ---- Map ---- */

/* InitStatus(LevelPath): an indoor map; the elevators, the NPCs that come back, the mayor, the police boss */
static byte ThMap_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mods[] = { "Elevator", "Init", "Chars", "CharsNPC" };
	static const char *const defs[][2] = { { "Elevator", "DM_Door_Elevator_DownTown" }, { "ElevatorOrbit", "DM_Door_ElevatorOrbit_Orbit" },
		{ "ElevatorOrbitB", "DM_Door_ElevatorOrbit_Orbit" }, { "Metro", "DM_Door_Metro_Press" }, { "MetroExt", "DM_Door_Metro_Press" } };
	static const struct {
		const char *var, *name;
		long n;
	} repos[] = { { NULL, "Sentinel", 3 }, { NULL, "Police", 3 }, { NULL, "Nurse", 3 }, { NULL, "Desktop", 3 }, { NULL, "Messenger", 2 },
		{ "_BankDirector", "BankDirector", 3 }, { "_Bishop", "Bishop", 3 }, { "_Functionary", "Functionary", 5 } };
	nativeVal_t zero = { 0 };
	(void)a;
	(void)out;
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(mods[i])) {
			return 0;
		}
	}
	Native_SetInt(&zero, 0);
	if (!Mode_SetModVar("Init", "Outdoor", &zero)) {
		return 0;
	}
	for (int i = 0; i < 5; i++) {
		if (!LV_CALL("Elevator", "SetDef", NULL, "ss", defs[i][0], defs[i][1])) {
			return 0;
		}
	}
	/* the last three only on the PC or when the map keeps them (`not Scrap.Get("IsXbox") or _BankDirector`) */
	for (int i = 0; i < 8; i++) {
		int r = repos[i].var ? LvN_PcOr(MAP, repos[i].var) : 1;
		if (r < 0 || (r && !LV_CALL("CharsNPC", "SetReposChars", NULL, "si", repos[i].name, repos[i].n))) {
			return 0;
		}
	}
	if (LvN_SaveIs("MayorAtTownHall", "1") && (!LV_CALL("CharsNPC", "CreateMissionChar", NULL, "s", "Mayor_Default") ||
		!LvN_MissionChar("Gear_Mayor", MAP, "CreateGearMayor"))) {
		return 0;
	}
	if (LvN_SaveIs("PoliceBossAtPolice", "1") && LvN_SaveIs("PoliceBossAtTownHall", "1")) {
		if (!LV_CALL("CharsNPC", "CreateMissionChar", NULL, "s", "PoliceBoss_Default")) {
			return 0;
		}
		SaveVar_Set("PoliceBossAtPolice", "0");
	}
	return 1;
}

/* CreateGearMayor(ch): the mayor's gear stays with the mayor */
static byte ThMap_CreateGearMayor(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t t = { 0 };
	(void)out;
	LvN_Str(&t, "Mayor_Default");
	return LvN_Stay(&a[0], 4, &t);
}

/* Init(LevelPath): the elevators and teleporters, the metro, the locked elevator to the orbit, the fake traffic */
static byte ThMap_Init(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const light[] = { "TLight1B", "TLight2B", "TLight3B" };
	static const char *const med[] = { "TMed1B", "TMed2B", "TMed3B" };
	nativeVal_t e = { 0 };
	(void)a;
	(void)out;
	if (!Mode_Import("Elevator") || !Mode_Import("Metro") || !Mode_Connect("DM_Door_Elevator_DownTown", "DownTown", 0) ||
		!Mode_Connect("DM_Door_ElevatorOrbit_Orbit", "Orbit", 0) || !Mode_Connect("DM_Door_Teleporter_01", "DM_Door_Teleporter_02", 1) ||
		!Mode_Connect("DM_Door_Teleporter_04", "DM_Door_Teleporter_05", 1) ||
		!Mode_Connect("DM_Door_Teleporter_06", "DM_Door_Teleporter_07", 1) ||
		!LV_CALL("Metro", "SetAsMetro", NULL, "s", "DM_Door_Metro_Press")) {
		return 0;
	}
	Native_SetNone(&e);
	byte ok = LvN_Entity("DM_Door_ElevatorOrbit_Orbit", &e) && LvN_SetS(&e, "UseFunc", "Map.OrbitLockMsg") &&
		LvN_SetI(&e, "ArrnOpt", 0);
	Native_Free(&e);
	return ok && Mode_Import("InTraffic") && LvN_InTraffic("N_TrafficList", light, 3, "N_NumTraffic", 20) &&
		LvN_InTraffic("M_TrafficList", med, 3, "M_NumTraffic", 5) && LvN_InTraffic("H_TrafficList", NULL, 0, "H_NumTraffic", 0) &&
		LV_CALL("InTraffic", "LoadFakeTraffic", NULL, "") && LV_CALL("InTraffic", "FakeTraffic", NULL, "");
}

/* Scrap.GetTime() + Speech.CalculateSpeechTime(text) + add into *time (OBJLIB 3: the speech time by name) */
static byte Th_SpeechEnd(const nativeVal_t *text, long add, double *time) {
	nativeVal_t now = { 0 }, calc = { 0 }, t = { 0 }, n = { 0 }, r = { 0 };
	Native_SetFloat(&now, Mode_Time());
	Native_SetNone(&calc);
	Native_SetNone(&t);
	Native_SetNone(&r);
	Native_SetInt(&n, add);
	byte ok = LV_CALL("Speech", "CalculateSpeechTime", &calc, "v", text) && Native_Arith('+', &now, &calc, &t) &&
		Native_Arith('+', &t, &n, &r) && Native_ArgDouble(&r, time);
	Native_Free(&calc);
	Native_Free(&t);
	Native_Free(&r);
	return ok;
}

/* OrbitLockMsg(UsrName, ElevatorName, optId, FullOption): the elevator to the orbit cannot be used */
static byte ThMap_OrbitLockMsg(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t cad = { 0 };
	double t;
	(void)a;
	(void)out;
	if (!Mode_Import("Speech")) {
		return 0;
	}
	Mode_Lang("CantUseOrbitElevator", &cad);
	byte ok = Th_SpeechEnd(&cad, 6, &t) && Mode_MsgText(&cad, t);
	Native_Free(&cad);
	return ok;
}

LV_FN(TownhallMap, InitStatus, 1, ThMap_InitStatus)
LV_FN(TownhallMap, CreateGearMayor, 1, ThMap_CreateGearMayor)
LV_FN(TownhallMap, Init, 1, ThMap_Init)
LV_FN(TownhallMap, OrbitLockMsg, 4, ThMap_OrbitLockMsg)

PyMethodDef g_TownhallMapMethods[] = {
	{ "InitStatus", TownhallMap_InitStatus, METH_VARARGS, NULL },
	{ "CreateGearMayor", TownhallMap_CreateGearMayor, METH_VARARGS, NULL },
	{ "Init", TownhallMap_Init, METH_VARARGS, NULL },
	{ "OrbitLockMsg", TownhallMap_OrbitLockMsg, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};
/* ---- MapSnd ---- */

/* the sound elements of the map (the rest of MapSnd is data/levels/townhall/mapsnd.def) */
static const lvSndElement_t townhall_snd[] = {
	{ .name = "TownhallAlmLB", .beep = "beep3", .imin = 6, .imax = 10, .ivol = 0.14 },
	{ .name = "TownhallAlmLA", .beep = "beep5", .imin = 6, .imax = 10, .ivol = 0.044 },
	{ .name = "TownhallAlmLC", .beep = "beep5", .imin = 6, .imax = 10, .ivol = 0.044 },
	{ .name = "TownhallServerA", .beep = "beep2", .beepXbox = "beep6", .imin = 4, .imax = 10, .ivol = 0.129 },
	{ .name = "TownhallServerB", .beep = "beep2", .beepXbox = "beep6", .imin = 4, .imax = 10, .ivol = 0.129 },
	{ .name = "TownhallHelix", .pc = 1, .front = 1, .loop = "ventilador3", .vol = 0.30, .pitch = -8, .hasPitch = 1 },
	{ .name = "TownhallLamp", .pc = 1, .loop = "light2", .vol = 0.04, .pitch = 10, .hasPitch = 1 },
	{ .name = "TownhallLampDesk", .pc = 1, .loop = "light2", .vol = 0.04, .pitch = 10, .hasPitch = 1 },
	{ .name = "TownhallHolo00", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep6", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.08 },
	{ .name = "TownhallHolo01", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo02", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo03", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo04", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo05", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep6", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.08 },
	{ .name = "TownhallHolo06", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo07", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo08", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep6", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.08 },
	{ .name = "TownhallHolo09", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo10", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "TownhallHolo11", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep6", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.08 },
	{ .name = "TownhallHolo12", .pc = 1, .loop = "holograma2", .vol = 0.3, .beep = "beep5", .beepI = 1, .imin = 4, .imax = 10, .ivol = 0.06 },
	{ .name = "Holotree00", .pc = 1, .loop = "buble1", .vol = 0.08 },
	{ .name = NULL },
};

/* SndElements(Name, x, y, z, angx, angy) */
static PyObject *TownhallMapSnd_SndElements(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_SndElements(args, townhall_snd);
}

PyMethodDef g_TownhallMapSndMethods[] = {
	{ "SndElements", TownhallMapSnd_SndElements, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};


/* ---- Mission_ApologizeMayor ---- */

/* InitStatus(LevelPath): the mayor's AI after the NPCs are browsed, the staplers come back */
static byte ThAm_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t d = { 0 };
	(void)a;
	(void)out;
	if (!Mode_Import("CharsNPC") || !LvN_ModAppendG("CharsNPC", "AfterBrowseNPC", AM, "AfterBrowseNPC")) {
		return 0;
	}
	LvN_Str(&d, "Desktop");
	byte ok = LvN_ModAppend("CharsNPC", "RespawnChars", &d);
	Native_Free(&d);
	return ok;
}

/* AfterBrowseNPC(): the mayor stays (CreateMayorInit), no gear */
static byte ThAm_AfterBrowseNPC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t key = { 0 };
	(void)a;
	(void)out;
	if (!Mode_Import("CharsNPC") || !LvN_Callback("Mayor_Default", AM, "CreateMayorInit")) {
		return 0;
	}
	/* del CharsNPC.CharRes["Gear_Mayor"] (OBJLIB 5) */
	LvN_Str(&key, "Gear_Mayor");
	byte ok = Mode_ModDictDel("CharsNPC", "CharRes", &key);
	Native_Free(&key);
	return ok;
}

/* CreateMayorInit(e) of ApologizeMayor and TalkToMayor: the mayor stays and talks (MayorSpeech of the mission) */
static byte Th_MayorInit(const nativeVal_t *e, const char *useFunc) {
	return Th_Stay6(e) && LvN_SetS(e, "UseFunc", useFunc);
}

static byte ThAm_CreateMayorInit(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Th_MayorInit(&a[0], "Mission_ApologizeMayor.MayorSpeech");
}

/* Init(LevelPath): the state of the crazy mayor: the forbidden area, the staplers, or the transport */
static byte ThAm_Init(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (LvN_SaveIs("CrazyMayor", "Killed")) {
		return Mode_Target("Mayor_Default") && Mode_GVarSetS("GuaranteedChars",
			",Police,Nurse,BankDirector,Desktop,Sentinel,Mayor,Gear,Berto,Messenger,Bishop,Functionary,Betty,PoliceBoss,BankMaster,") &&
			LV_OWN(AM, "ForbiddenMsg", NULL, "");
	}
	if (LvN_SaveIs("CrazyMayor", "Stapler")) {
		return LV_OWN(AM, "InitStaplerMission", NULL, "");
	}
	return !LvN_SaveIs("CrazyMayor", "AskTheTransport") || Mode_Target("Mayor_Default");
}

/* ForbiddenMsg(): the label of the forbidden area, every second */
static byte ThAm_ForbiddenMsg(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t lang = { 0 }, t = { 0 }, name = { 0 };
	double now = Mode_Time();
	(void)a;
	(void)out;
	Mode_Lang("Generics_ForbiddenArea_Label", &lang);
	byte ok = Mode_LabelText(&lang, now + 0.5);
	Native_Free(&lang);
	/* OBJLIB 4: the function of the module (a mission may replace it) */
	PyObject *fn = ok ? Mode_GlobalPy(AM, "ForbiddenMsg") : NULL;
	if (!fn) {
		return 0;
	}
	Native_SetFloat(&t, now + 1.0);
	LvN_Str(&name, "ForbiddenMsg");
	ok = Mode_Schedule(&t, fn, NULL, 0, &name);
	Native_Free(&name);
	Py_DECREF(fn);
	return ok;
}

/* MayorSpeech(UsrName, CharName, optId, FullOption): the mayor talks to Dtritus for the state of the mission */
static byte ThAm_MayorSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	int dt = Mode_UsrIs("Dtritus");
	if (dt <= 0) {
		return dt == 0 && Mode_Spch("Mission_ApologizeMayor_Mayor_InitMsg2", "Mission_ApologizeMayor_Mayor_InitAns2", NULL);
	}
	if (LvN_SaveIs("CrazyMayor", "Killed")) {
		nativeVal_t g = { 0 };
		if (!Native_GVarGet("GuaranteedChars", &g)) {
			return 0;
		}
		byte guarded = !LvN_EqS(&g, "");
		Native_Free(&g);
		if (!guarded) {
			return LV_CALL("Speech", "RestoreState", NULL, "v", &a[1]) &&
				Mode_Spch("Mission_ApologizeMayor_Mayor_1Msg", "Mission_ApologizeMayor_Mayor_1Ans", NULL) &&
				LvN_ModSetGlobal("Speech", "OnEndOfSpeech", AM, "InitStaplerMission");
		}
		const char *ans = "Mission_ApologizeMayor_Mayor_1";
		return Mode_AnsLst(&ans, 1) && Mode_Spch("Mission_ApologizeMayor_Mayor_InitMsg1", "Mission_ApologizeMayor_Mayor_InitAns1", NULL) &&
			Mode_GVarSetS("GuaranteedChars", "") && Mode_DelSchedS("ForbiddenMsg");
	}
	if (LvN_SaveIs("CrazyMayor", "Stapler")) {
		return Mode_Spch("Mission_KillStaplers_Mayor_InitMsg1", "Mission_KillStaplers_Mayor_InitAns1", NULL);
	}
	if (LvN_SaveIs("CrazyMayor", "AskTheTransport")) {
		return Mode_Spch("Mission_TalkToMayor1_Mayor_InitMsg1", "Mission_TalkToMayor1_Mayor_InitAns1", NULL) &&
			LvN_ModSetGlobal("Speech", "OnEndOfSpeech", AM, "SetTransportMission");
	}
	return Mode_Spch("Mission_TalkToMayor1_Mayor_InitMsg2", "Mission_TalkToMayor1_Mayor_InitAns2", NULL);
}

/* MissionsFuncs.ActivateTarget(Scrap.GetEntity(name), on) of a value name (OBJLIB 3) */
static byte Th_Activate(const nativeVal_t *name, long on) {
	nativeVal_t e = { 0 };
	Native_SetNone(&e);
	byte ok = Mode_EntityH(name, &e) && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &e, on);
	Native_Free(&e);
	return ok;
}

/* SetTransportMission(): Mission_ProtectMayorTransport in DownTown; the mayor leaves */
static byte ThAm_SetTransportMission(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t npc = { 0 };
	(void)a;
	(void)out;
	if (!LvN_SaveIs("CrazyMayor", "AskTheTransport")) {
		return 1;
	}
	if (!LV_CALL("MissionsFuncs", "SetNextMission", NULL, "ss", "Mission_ProtectMayorTransport", "DownTown")) {
		return 0;
	}
	/* OBJLIB 5: Speech.NPCName as the value it is */
	byte ok = LvN_ModGet("Speech", "NPCName", &npc) && Th_Activate(&npc, 0);
	Native_Free(&npc);
	if (!ok || !Mode_MMDesc("Mission_ProtectMayorTransport_MissionText1", 1L)) {
		return 0;
	}
	SaveVar_Set("MayorAtTownHall", "0");
	SaveVar_Set("CrazyMayor", "Transport");
	return 1;
}

/* ChooseStapler(NotName=""): a stapler of the level at random (not NotName) is the target; none there: again in
 * 2 s */
static byte ThAm_ChooseStaplerN(const nativeVal_t *not) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *mmt = Mode_V(&t), *keys = Mode_V(&t), *stapler = Mode_V(&t), *name = Mode_V(&t);
	Native_SetEmpty(mmt, NV_LIST);
	/* for ky in CharsNPC.CharRes.keys(): CharRes read again at each key, as the script reads it (OBJLIB 5) */
	if (!Mode_Keys("CharsNPC", "CharRes", 0, keys)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(keys);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < n; i++) {
		nativeVal_t ky = { 0 }, act = { 0 };
		Native_SetNone(&ky);
		Native_SetNone(&act);
		int c = Native_SeqItem(keys, i, &ky) ? 1 : -1;
		PyObject *ch = c > 0 ? Mode_CharRes(&ky) : NULL;
		c = ch && Mode_HGet(ch, "ActCtrl", &act) ? LvN_EqS(&act, "Desktop") : -1;
		Py_XDECREF(ch);
		Native_Free(&act);
		if (c > 0) {
			c = Th_Cmp(&ky, not);
			c = c == 2 ? -1 : c != 0;
		}
		if (c > 0) {
			cEntity *e;
			c = Native_GetEntity(&ky, &e) ? e != NULL : -1;
		}
		if (c > 0) {
			int in = Native_Contains(mmt, &ky);
			c = in < 0 ? -1 : !in;
		}
		if (c > 0) {
			Native_Copy(Native_Append(mmt), &ky);
		}
		Native_Free(&ky);
		if (c < 0) {
			return Mode_Done(&t, 0);
		}
	}
	if (mmt->n > 0) {
		/* int(Scrap.Rand(0.0, len(mmt) - 0.05)): the engine's random directly, as the port did */
		long index = (long)Scrap_RandRange(0.0, (double)mmt->n - 0.05);
		if (!Mode_EntityH(&mmt->items[index], stapler)) {
			return Mode_Done(&t, 0);
		}
	}
	byte ok;
	if (Native_True(stapler)) {
		ok = LvN_SetS(stapler, "UseFunc", "Mission_ApologizeMayor.StaplerSpeech") && LvN_SetI(stapler, "AI_Repos", 0) &&
			Mode_VGet(stapler, "Name", name) && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", name);
	} else {
		ok = Mode_Later(2.0, AM, "ChooseStapler", not, 1, NULL);
	}
	return Mode_Done(&t, ok);
}

TH_KWFN(ApologizeMayor_ChooseStapler, "NotName", "", ThAm_ChooseStaplerN)

/* InitStaplerMission(): the staplers to kill: the counter, the scorer, the NPC death hook, the first stapler */
static byte ThAm_InitStaplerMission(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Mode_MMDesc("Mission_KillStaplers_MissionText1", 1L)) {
		return 0;
	}
	SaveVar_Create("CrazyMayor.StaplerDeads", "0");
	SaveVar_Set("CrazyMayor", "Stapler");
	return LvN_ScorerCallG(AM, "MissionScorer") && LvN_Draw() && Mode_GVarSetI("RadarMMSelect", 1) &&
		LvN_ModSetGlobal("CharAct", "onNPCDeath", AM, "StaplerDead") && LV_OWN(AM, "ChooseStapler", NULL, "");
}

/* StaplerSpeech(UsrName, CharName, optId, FullOption) */
static byte ThAm_StaplerSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_Speakers(&a[0], &a[1]) && Mode_Spch("Mission_KillStaplers_Desktop_InitMsg1", "Mission_KillStaplers_Desktop_InitAns1", NULL);
}

/* `deads` + "/" + `MaxTargets` (the global read here) into out */
static byte ThAm_Ratio(const nativeVal_t *deads, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rd = Mode_V(&t), *mx = Mode_V(&t), *rm = Mode_V(&t);
	Mode_Repr(deads, rd);
	if (!Mode_G(AM, "MaxTargets", mx)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(mx, rm);
	return Mode_Done(&t, Mode_Cat(&t, out, 3, rd, Mode_S(&t, "/"), rm));
}

/* deads == MaxTargets (the global read here): 1, 0, -1 with the error */
static int ThAm_Last(const nativeVal_t *deads) {
	nativeVal_t mx = { 0 };
	if (!Mode_G(AM, "MaxTargets", &mx)) {
		Native_Free(&mx);
		return -1;
	}
	int c = Th_Cmp(deads, &mx);
	Native_Free(&mx);
	return c == 2 ? -1 : c == 0;
}

/* StaplerDead(name, pos): the death hook of the NPCs: the target stapler counts, the mayor taunts; the last one ends
 * the hunt; another stapler draws a remark. Always 0 */
static byte ThAm_StaplerDead(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *rs = Mode_V(&t), *dv = Mode_V(&t), *deads = Mode_V(&t), *txt = Mode_V(&t);
	nativeVal_t *taunt = Mode_V(&t), *t1 = Mode_V(&t), *rt = Mode_V(&t), *msg = Mode_V(&t), *act = Mode_V(&t);
	Native_SetInt(out, 0);
	if (!Mode_EntityH(&a[0], e)) {
		return Mode_Done(&t, 0);
	}
	int target = Native_True(e);
	if (target) {
		target = Mode_VGet(e, "RadarShow", rs) ? Mode_CmpI(rs, "==", 1) : -1;
	}
	if (target < 0) {
		return Mode_Done(&t, 0);
	}
	if (!target) {
		if (!Native_True(e)) {
			return Mode_Done(&t, 1);
		}
		int desk = Mode_VGet(e, "ActCtrl", act) ? LvN_EqS(act, "Desktop") : -1;
		return Mode_Done(&t, desk >= 0 && (!desk ||
			LV_CALL("Speech", "RemoteComunication", NULL, "ss", "Mission_KillStaplers_RemoteMessage7", "Mayor")));
	}
	if (!LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", e, 0L) || !LvN_SaveList("CrazyMayor.StaplerDeads", AM, dv) ||
		!Native_Arith('+', dv, Mode_I(&t, 1), deads)) {
		return Mode_Done(&t, 0);
	}
	int last = ThAm_Last(deads);
	if (last < 0 || (last && !(LvN_ScorerOff() && LvN_Draw())) ||
		(!last && !(ThAm_Ratio(deads, txt) && LvN_ScorerSet(Mode_I(&t, 0), "RemainingStaplersScorer", "Text", txt)))) {
		return Mode_Done(&t, 0);
	}
	/* msg = "Mission_KillStaplers_RemoteMessage" + `SMayorTaunt + 1`; SMayorTaunt = SMayorTaunt + 1 (0 after 4) */
	if (!Mode_G(AM, "SMayorTaunt", taunt) || !Native_Arith('+', taunt, Mode_I(&t, 1), t1)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(t1, rt);
	Native_Free(taunt);
	int over;
	if (!Mode_Cat(&t, msg, 2, Mode_S(&t, "Mission_KillStaplers_RemoteMessage"), rt) || !LvN_GAdd(AM, "SMayorTaunt", 1, NULL) ||
		!Mode_G(AM, "SMayorTaunt", taunt) || (over = Mode_CmpI(taunt, ">", 4)) < 0 || (over && !LvN_PutI(AM, "SMayorTaunt", 0))) {
		return Mode_Done(&t, 0);
	}
	last = ThAm_Last(deads);
	if (last) {
		return Mode_Done(&t, last > 0 && LvN_Com("Mission_KillStaplers_RemoteMessage6", "Mayor", AM, "MayorMessage", LvN_NoArgs()));
	}
	LvN_SaveRepr("CrazyMayor.StaplerDeads", deads);
	return Mode_Done(&t, LV_OWN(AM, "ChooseStapler", NULL, "v", &a[0]) &&
		(!Native_True(msg) || LV_CALL("Speech", "RemoteComunication", NULL, "vs", msg, "Mayor")));
}

/* MayorMessage(): the staplers done: the mayor waits to talk about the transport */
static byte ThAm_MayorMessage(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!LvN_ModNone("CharAct", "onNPCDeath")) {
		return 0;
	}
	SaveVars_Delete("CrazyMayor.StaplerDeads");
	SaveVar_Set("CrazyMayor", "AskTheTransport");
	return Mode_Target("Mayor_Default") && Mode_MMDesc("Mission_TalkToMayor1_MissionText1", 1L);
}

/* the text item of a mission scorer with its label key + ":" */
static byte Th_ScorerLabel(const nativeVal_t *id, const char *item, const char *key) {
	nativeVal_t lab = { 0 };
	LvN_Label(key, &lab);
	byte ok = LvN_ScorerTextBegin(id, item, 0) && LvN_ScorerSet(id, item, "Text", &lab) && LvN_ScorerTextEnd(id, item);
	Native_Free(&lab);
	return ok;
}

/* MissionScorer(id): the staplers killed of MaxTargets */
static byte ThAm_MissionScorer(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t deads = { 0 }, txt = { 0 };
	(void)out;
	if (!Mode_Import("Scorer") || !Th_ScorerLabel(&a[0], "RemainingStaplersText", "Mission_KillStaplers_Scorer") ||
		!LvN_ScorerTextBegin(&a[0], "RemainingStaplersScorer", 20)) {
		return 0;
	}
	Native_SetNone(&txt);
	byte ok = LvN_SaveList("CrazyMayor.StaplerDeads", AM, &deads) && ThAm_Ratio(&deads, &txt) &&
		LvN_ScorerSet(&a[0], "RemainingStaplersScorer", "Text", &txt) && LvN_ScorerTextEnd(&a[0], "RemainingStaplersScorer");
	Native_Free(&deads);
	Native_Free(&txt);
	return ok;
}

LV_FN(ApologizeMayor, InitStatus, 1, ThAm_InitStatus)
LV_FN(ApologizeMayor, AfterBrowseNPC, 0, ThAm_AfterBrowseNPC)
LV_FN(ApologizeMayor, CreateMayorInit, 1, ThAm_CreateMayorInit)
LV_FN(ApologizeMayor, Init, 1, ThAm_Init)
LV_FN(ApologizeMayor, ForbiddenMsg, 0, ThAm_ForbiddenMsg)
LV_FN(ApologizeMayor, MayorSpeech, 4, ThAm_MayorSpeech)
LV_FN(ApologizeMayor, SetTransportMission, 0, ThAm_SetTransportMission)
LV_FN(ApologizeMayor, InitStaplerMission, 0, ThAm_InitStaplerMission)
LV_FN(ApologizeMayor, StaplerSpeech, 4, ThAm_StaplerSpeech)
LV_FN(ApologizeMayor, StaplerDead, 2, ThAm_StaplerDead)
LV_FN(ApologizeMayor, MayorMessage, 0, ThAm_MayorMessage)
LV_FN(ApologizeMayor, MissionScorer, 1, ThAm_MissionScorer)

PyMethodDef g_TownhallApologizeMayorMethods[] = {
	{ "InitStatus", ApologizeMayor_InitStatus, METH_VARARGS, NULL },
	{ "AfterBrowseNPC", ApologizeMayor_AfterBrowseNPC, METH_VARARGS, NULL },
	{ "CreateMayorInit", ApologizeMayor_CreateMayorInit, METH_VARARGS, NULL },
	{ "Init", ApologizeMayor_Init, METH_VARARGS, NULL },
	{ "ForbiddenMsg", ApologizeMayor_ForbiddenMsg, METH_VARARGS, NULL },
	{ "MayorSpeech", ApologizeMayor_MayorSpeech, METH_VARARGS, NULL },
	{ "SetTransportMission", ApologizeMayor_SetTransportMission, METH_VARARGS, NULL },
	{ "ChooseStapler", (PyCFunction)(void (*)(void))ApologizeMayor_ChooseStapler, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "InitStaplerMission", ApologizeMayor_InitStaplerMission, METH_VARARGS, NULL },
	{ "StaplerSpeech", ApologizeMayor_StaplerSpeech, METH_VARARGS, NULL },
	{ "StaplerDead", ApologizeMayor_StaplerDead, METH_VARARGS, NULL },
	{ "MayorMessage", ApologizeMayor_MayorMessage, METH_VARARGS, NULL },
	{ "MissionScorer", ApologizeMayor_MissionScorer, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_CatchTrurl ---- */

/* InitStatus(LevelPath): Trurl is the target, no data packs of the messengers; three functionaries */
static byte ThCt_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const funcs[] = { "Functionary_10", "Functionary_23", "Functionary_25" };
	nativeVal_t zero = { 0 };
	(void)a;
	(void)out;
	Native_SetInt(&zero, 0);
	if (!Mode_Import("Messenger") || !Mode_SetModVar("Messenger", "MakeMDP", &zero) ||
		!LvN_MissionChar("Messenger_Trurl", CT, "CreateTrurl")) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		if (!LV_CALL("CharsNPC", "CreateMissionChar", NULL, "s", funcs[i])) {
			return 0;
		}
	}
	return 1;
}

/* Init(LevelPath): Trurl's name; the state of the mission (the first time its save variables) */
static byte ThCt_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t trurl = { 0 }, name = { 0 }, data = { 0 };
	(void)a;
	(void)out;
	Native_SetNone(&trurl);
	Mode_Lang("CharName_Trurl", &name);
	byte ok = LvN_Entity("Messenger_Trurl", &trurl) && Mode_VSet(&trurl, "Descriptor", &name);
	Native_Free(&trurl);
	Native_Free(&name);
	if (!ok) {
		return 0;
	}
	Mode_SaveVar("CatchTrurlMissionData", &data);
	if (LvN_EqS(&data, "CatchTrurl")) {
		SaveVar_Set("Mission_CatchTrurl_Data", "['Functionary_10', 'Functionary_23', 'Functionary_25']");
		SaveVar_Set("Mission_CatchTrurl_MapsPos", "[]");
		SaveVar_Set("Mission_CatchTrurl_NumMapsDropped", "0");
		SaveVar_Set("Mission_CatchTrurl_NumMapsTaken", "0");
		ok = Mode_Target("Messenger_Trurl");
	} else if (LvN_EqS(&data, "KillFunctionaries")) {
		ok = LV_OWN(CT, "KillFunctionaries", NULL, "");
	} else if (LvN_EqS(&data, "CatchTrurlAgain")) {
		ok = LV_OWN(CT, "CatchTrurlAgain", NULL, "");
	}
	Native_Free(&data);
	return ok;
}

/* CreateTrurl(t): Trurl's talk for the state of the mission */
static byte ThCt_CreateTrurl(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t data = { 0 };
	byte ok = 1;
	(void)out;
	Mode_SaveVar("CatchTrurlMissionData", &data);
	if (LvN_EqS(&data, "CatchTrurl")) {
		ok = LvN_SetS(&a[0], "UseFunc", "Mission_CatchTrurl.OnCatchTrurl");
	} else if (LvN_EqS(&data, "CatchTrurlAgain")) {
		ok = LvN_SetS(&a[0], "UseFunc", "Mission_CatchTrurl.OnCatchTrurlAgain");
	}
	Native_Free(&data);
	return ok;
}

/* OnCatchTrurl(UsrName, CharName, optId, FullOption): Trurl caught; his talk ends in KillFunctionaries */
static byte ThCt_OnCatchTrurl(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (!LvN_EqS(&a[2], "Talk")) {
		return 1;
	}
	SaveVar_Set("CatchTrurlMissionData", "KillFunctionaries");
	return LvN_OnEnd(CT, "KillFunctionaries", NULL) && Mode_NISpeech("Messenger", "Mission_GetTrurl_Trurl_NI_1Msg");
}

/* OnCatchTrurlAgain(UsrName, CharName, optId, FullOption): Trurl again; SetNextMission at the end */
static byte ThCt_OnCatchTrurlAgain(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_Speakers(&a[0], &a[1]) && (!LvN_EqS(&a[2], "Talk") || (LvN_OnEnd(CT, "SetNextMission", NULL) &&
		Mode_Spch("Mission_GetTrurlAgain_Trurl_InitMsg1", "Mission_GetTrurlAgain_Trurl_InitAns1", NULL)));
}

/* FunctionaryMissionSpeech(UsrName, CharName, optId, FullOption): a functionary with blueprints talks */
static byte ThCt_FunctionaryMissionSpeech(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_Speakers(&a[0], &a[1]) &&
		(!LvN_EqS(&a[2], "Talk") || Mode_Spch("Mission_GetMayorOutlines_InitMsg1", "Mission_GetMayorOutlines_InitAns1", NULL));
}

/* module.var = SpawnMap when it is None (OBJLIB 5: the hook of another module as the value it is) */
static byte Ct_HookSpawn(const char *module, const char *var) {
	nativeVal_t v = { 0 };
	if (!LvN_ModGet(module, var, &v)) {
		return 0;
	}
	byte none = v.type == NV_NONE;
	Native_Free(&v);
	return !none || LvN_ModSetGlobal(module, var, CT, "SpawnMap");
}

/* SetCallbackFuncs(missionData): while functionaries live their death or possession drops a blueprint (SpawnMap) */
static byte ThCt_SetCallbackFuncs(const nativeVal_t *a, nativeVal_t *out) {
	long alive = 0;
	(void)out;
	if (!Mode_Import("string")) {
		return 0;
	}
	int n = Native_ForCount(&a[0]);
	if (n < 0) {
		return 0;
	}
	for (int i = 0; i < n; i++) {
		nativeVal_t target = { 0 }, r = { 0 };
		Native_SetNone(&target);
		Native_SetNone(&r);
		/* OBJLIB 3: string.find by name */
		int c = Native_SeqItem(&a[0], i, &target) && PORT_CALL(string, find, &r, "vs", &target, "Blueprint") ?
			Mode_CmpI(&r, "==", -1) : -1;
		Native_Free(&target);
		Native_Free(&r);
		if (c < 0) {
			return 0;
		}
		alive += c;
	}
	if (alive == 0) {
		return LvN_ModNone("CharAct", "onNPCDeath") && LvN_ModNone("CharConversor", "OnPossession");
	}
	return Ct_HookSpawn("CharAct", "onNPCDeath") && Ct_HookSpawn("CharConversor", "OnPossession");
}

/* the save variables Mission_CatchTrurl_Data and _MapsPos as the script evals them */
static byte Ct_Lists(nativeVal_t *data, nativeVal_t *maps) {
	return LvN_SaveList("Mission_CatchTrurl_Data", CT, data) && LvN_SaveList("Mission_CatchTrurl_MapsPos", CT, maps);
}

/* the save variable name = `int(name) + 1` */
static byte Ct_Inc(const char *name) {
	nativeVal_t i = { 0 }, one = { 0 }, r = { 0 };
	Native_SetInt(&one, 1);
	Native_SetNone(&r);
	byte ok = Mode_SaveInt(name, &i) && Native_Arith('+', &i, &one, &r);
	if (ok) {
		LvN_SaveRepr(name, &r);
	}
	Native_Free(&i);
	Native_Free(&r);
	return ok;
}

/* SpawnMap(name, pos): a functionary of the mission died or was possessed: he drops the next blueprint (1); else 0 */
static byte ThCt_SpawnMap(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *data = Mode_V(&t), *maps = Mode_V(&t), *dropped = Mode_V(&t), *e = Mode_V(&t), *en = Mode_V(&t);
	nativeVal_t *pair = Mode_V(&t), *pn = Mode_V(&t), *pp = Mode_V(&t);
	if (!Ct_Lists(data, maps)) {
		return Mode_Done(&t, 0);
	}
	int in = Native_Contains(data, &a[0]);
	if (in <= 0) {
		Native_SetInt(out, 0);
		return Mode_Done(&t, in == 0);
	}
	if (!Mode_SaveInt("Mission_CatchTrurl_NumMapsDropped", dropped) || !LV_OWN(CT, "CreateMap", e, "vv", &a[1], dropped) ||
		!LV_CALL("MissionsFuncs", "DelMissionTarget", NULL, "v", &a[0]) || !Th_Activate(&a[0], 0) ||
		!LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", e, 1L) || !LvN_ListRemove(data, &a[0]) ||
		!Mode_VGet(e, "Name", en) || !LvN_Append(data, en) || !Mode_VGet(e, "Name", pn) || !Mode_VGet(e, "Pos", pp)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(pair, NV_TUPLE);
	Native_Copy(Native_Append(pair), pn);
	Native_Copy(Native_Append(pair), pp);
	if (!LvN_Append(maps, pair)) {
		return Mode_Done(&t, 0);
	}
	LvN_SaveRepr("Mission_CatchTrurl_Data", data);
	LvN_SaveRepr("Mission_CatchTrurl_MapsPos", maps);
	Native_SetInt(out, 1);
	return Mode_Done(&t, Ct_Inc("Mission_CatchTrurl_NumMapsDropped") && LV_OWN(CT, "SetCallbackFuncs", NULL, "v", data));
}

/* CreateMap(pos, mapType): a blueprint on the floor to photograph: 0 the weapon, 1 the engine, 2 the ship, else the
 * name itself */
static byte ThCt_CreateMap(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const names[] = { "PlanoDevastator", "PlanoMMayor1", "PlanoSMayor1" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *cad = Mode_V(&t), *file = Mode_V(&t);
	Native_Copy(cad, &a[1]);
	for (int i = 0; i < 3; i++) {
		int c = Mode_CmpI(&a[1], "==", i);
		if (c < 0) {
			return Mode_Done(&t, 0);
		}
		if (c) {
			Native_Free(cad);
			LvN_Str(cad, names[i]);
			break;
		}
	}
	/* Scrap.CreateEntity through its seam (fr6.py swaps it) */
	byte ok = LvN_CreateAt(cad, &a[0], "WithLife", out) && LvN_SetI(out, "ArrowCanView", 1) && LvN_SetS(out, "ViewFunc", "") &&
		LvN_SetI(out, "CanPhoto", 1) && LvN_SetI(out, "VRadius", 30) &&
		Mode_Cat(&t, file, 3, Mode_S(&t, "Models/Misc/Blueprint/"), cad, Mode_S(&t, ".M3D")) && Mode_VModel(out, file) &&
		LV_CALL("Items", "SetInteractiveObjOnFloor", NULL, "v", out);
	return Mode_Done(&t, ok);
}

/* the blueprint item into the hangar list of the save variable var (once; always when item is NULL: the weapon), the
 * message, a new blueprint */
static byte Ct_Blueprint(const char *var, const char *item, const char *msg) {
	nativeVal_t l = { 0 }, it = { 0 }, text = { 0 };
	int add = 0;
	if (!LvN_SaveList(var, CT, &l)) {
		Native_Free(&l);
		return 0;
	}
	LvN_Str(&it, item ? item : "Devastator");
	if (item) {
		add = Native_Contains(&l, &it);
	}
	byte ok = add >= 0 && (add || LvN_SaveAppend(var, &l, &it));
	Native_Free(&l);
	Native_Free(&it);
	if (!ok) {
		return 0;
	}
	Mode_Lang(msg, &text);
	ok = LV_CALL("RaceMaker", "ShowSystemMessage", NULL, "v", &text);
	Native_Free(&text);
	if (ok) {
		SaveVar_Inc("NewBluePrintAvaliable", 1);
	}
	return ok;
}

/* TakeMapPhoto(mapName): the photo of a blueprint of the mission (1, else 0); after the last Trurl calls */
static byte ThCt_TakeMapPhoto(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const vars[] = { "Mission_CatchTrurl_Data", "Mission_CatchTrurl_MapsPos", "Mission_CatchTrurl_NumMapsDropped",
		"Mission_CatchTrurl_NumMapsTaken" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *data = Mode_V(&t), *maps = Mode_V(&t), *m = Mode_V(&t);
	if (!Mode_Import("RaceMaker") || !Ct_Lists(data, maps)) {
		return Mode_Done(&t, 0);
	}
	int in = Native_Contains(data, &a[0]);
	if (in <= 0) {
		Native_SetInt(out, 0);
		return Mode_Done(&t, in == 0);
	}
	if (!Mode_EntityH(&a[0], m) || !LvN_SetI(m, "RadarShow", 0) || !LvN_SetI(m, "ArrowCanView", 0) ||
		!LvN_SetI(m, "ArrowCanMission", 0)) {
		return Mode_Done(&t, 0);
	}
	byte ok;
	if (LvN_EqS(&a[0], "PlanoSMayor1")) {
		ok = Ct_Blueprint("Hangar.shipsToEditList", "SMayor1", "Mission_GetMayorOutlines_SystemMessage1");
	} else if (LvN_EqS(&a[0], "PlanoMMayor1")) {
		ok = Ct_Blueprint("Hangar.availableEnginesList", "MMAYOR1", "Mission_GetMayorOutlines_SystemMessage2");
	} else {
		ok = Ct_Blueprint("Hangar.availableWeaponsList", NULL, "Mission_GetMayorOutlines_SystemMessage3");
	}
	if (!ok || !LvN_ListRemove(data, &a[0])) {
		return Mode_Done(&t, 0);
	}
	/* for elem in maps: if elem[0] == mapName: maps.remove(elem) */
	int n = Native_ForCount(maps);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < n; i++) {
		nativeVal_t elem = { 0 }, first = { 0 };
		Native_SetNone(&elem);
		Native_SetNone(&first);
		int c = Native_SeqItem(maps, i, &elem) && LvN_Item(&elem, 0, &first) ? Th_Cmp(&first, &a[0]) : 2;
		ok = c != 2 && (c != 0 || LvN_ListRemove(maps, &elem));
		Native_Free(&elem);
		Native_Free(&first);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
		if (c == 0) {
			break;
		}
	}
	LvN_SaveRepr("Mission_CatchTrurl_Data", data);
	LvN_SaveRepr("Mission_CatchTrurl_MapsPos", maps);
	int left = Ct_Inc("Mission_CatchTrurl_NumMapsTaken") ? Native_Size(data) : -1;
	if (left < 0) {
		return Mode_Done(&t, 0);
	}
	if (left == 0) {
		for (int i = 0; i < 4; i++) {
			SaveVars_Delete(vars[i]);
		}
		if (!LvN_ModNone("CharAct", "onNPCDeath") || !LvN_ModNone("CharConversor", "OnPossession") ||
			!LvN_ModNone("CharAct", "TakenPhoto")) {
			return Mode_Done(&t, 0);
		}
		SaveVar_Set("CatchTrurlMissionData", "CatchTrurlAgain");
		ok = LvN_Com("Mission_GetMayorOutlines_RemoteMessage2", "Trurl", CT, "CatchTrurlAgain", NULL);
	} else {
		ok = LvN_Com("Mission_GetMayorOutlines_RemoteMessage1", "Trurl", NULL, NULL, NULL);
	}
	Native_SetInt(out, 1);
	return Mode_Done(&t, ok);
}

/* KillFunctionaries(): the functionaries with blueprints flee and are the targets; the blueprints already dropped
 * lie again where they fell */
static byte ThCt_KillFunctionaries(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *data = Mode_V(&t), *maps = Mode_V(&t);
	long index = 0;
	(void)a;
	(void)out;
	if (!Ct_Lists(data, maps) || Native_Size(maps) < 0) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(data);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < n; i++) {
		nativeVal_t fn = { 0 }, e = { 0 }, mi = { 0 }, pos = { 0 };
		Native_SetNone(&fn);
		Native_SetNone(&e);
		Native_SetNone(&mi);
		Native_SetNone(&pos);
		byte ok = Native_SeqItem(data, i, &fn) && Mode_EntityH(&fn, &e);
		if (ok && Native_True(&e)) {
			ok = LvN_SetF(&e, "LLogicProbTalkWhenWalking", 1.0) && LvN_SetI(&e, "LLogicEscapeWithinRadius", 1) &&
				LvN_SetI(&e, "LLogicEscapeRadiusSqr", 562500) && LvN_SetS(&e, "UseFunc", "Mission_CatchTrurl.FunctionaryMissionSpeech");
		} else if (ok) {
			ok = LvN_Item(maps, index, &mi) && LvN_Item(&mi, 1, &pos) && LV_OWN(CT, "CreateMap", NULL, "vv", &pos, &fn);
			index++;
		}
		Native_Free(&fn);
		Native_Free(&e);
		Native_Free(&mi);
		Native_Free(&pos);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Mode_WayPoint() && LV_CALL("MissionsFuncs", "SetMultipleMissionTargets", NULL, "v", data) &&
		Mode_MMDesc("Mission_GetMayorOutlines_MissionText1", 1L) && LV_OWN(CT, "SetCallbackFuncs", NULL, "v", data) &&
		LvN_ModSetGlobal("CharAct", "TakenPhoto", CT, "TakeMapPhoto"));
}

/* CatchTrurlAgain(): saved, Trurl is the target again */
static byte ThCt_CatchTrurlAgain(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_WayPoint() && Mode_Target("Messenger_Trurl") && Mode_MMDesc("Mission_GetTrurlAgain_MissionText1", 1L) &&
		Mode_EntSetS("Messenger_Trurl", "UseFunc", "Mission_CatchTrurl.OnCatchTrurlAgain");
}

/* SetNextMission(): Mission_StealMayorShip in DownTown, the parked ships, the crazy gambler calls in 2 s */
static byte ThCt_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const parked[][2] = { { "Parked.Police", "['SPOLI1', 'SPOLI2', 'SPOLI3']" }, { "Parked.Mercs", "['SMERC1']" },
		{ "Parked.TownHall", "['SBANKER1', 'SPOLI2']" }, { "Parked.Bank", "['SBANKER1']" }, { "Parked.Temple", "['SPOLI3']" } };
	(void)a;
	(void)out;
	if (!Mode_Target("")) {
		return 0;
	}
	SaveVars_Delete("CatchTrurlMissionData");
	if (!Mode_WayPoint() || !LV_CALL("MissionsFuncs", "SetNextMission", NULL, "ss", "Mission_StealMayorShip", "DownTown") ||
		!Mode_MMDesc("Mission_TestMayorShip_MissionText1", 1L)) {
		return 0;
	}
	for (int i = 0; i < 5; i++) {
		SaveVar_Set(parked[i][0], parked[i][1]);
	}
	return Mode_Later(2.0, CT, "CGCalls", NULL, 0, NULL);
}

/* CGCalls(): the crazy gambler's call after the first crazy deals */
static byte ThCt_CGCalls(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const msgs[] = { "Mission_GetTrurl_RemoteMessage1", "Mission_GetTrurl_RemoteMessage2", "Mission_GetTrurl_RemoteMessage3" };
	nativeVal_t lib = { 0 }, num = { 0 };
	(void)a;
	(void)out;
	Mode_SaveVar("SuperDeal.Library", &lib);
	byte twelfth = LvN_EqS(&lib, "SuperDeal_Twelfth");
	Native_Free(&lib);
	if (twelfth) {
		return 1;
	}
	if (!LV_CALL("MissionsFuncs", "GetNumCrazyDealCompleted", &num, "")) {
		return 0;
	}
	byte ok = 1;
	for (int i = 0; i < 3; i++) {
		int c = Mode_CmpI(&num, "==", i);
		if (c) {
			ok = c > 0 && LvN_Com(msgs[i], "CrazyGambler", NULL, NULL, NULL);
			break;
		}
	}
	Native_Free(&num);
	return ok;
}

LV_FN(CatchTrurl, InitStatus, 1, ThCt_InitStatus)
LV_FN(CatchTrurl, Init, 1, ThCt_Init)
LV_FN(CatchTrurl, CreateTrurl, 1, ThCt_CreateTrurl)
LV_FN(CatchTrurl, OnCatchTrurl, 4, ThCt_OnCatchTrurl)
LV_FN(CatchTrurl, OnCatchTrurlAgain, 4, ThCt_OnCatchTrurlAgain)
LV_FN(CatchTrurl, FunctionaryMissionSpeech, 4, ThCt_FunctionaryMissionSpeech)
LV_FN(CatchTrurl, SetCallbackFuncs, 1, ThCt_SetCallbackFuncs)
LV_FN(CatchTrurl, SpawnMap, 2, ThCt_SpawnMap)
LV_FN(CatchTrurl, CreateMap, 2, ThCt_CreateMap)
LV_FN(CatchTrurl, TakeMapPhoto, 1, ThCt_TakeMapPhoto)
LV_FN(CatchTrurl, KillFunctionaries, 0, ThCt_KillFunctionaries)
LV_FN(CatchTrurl, CatchTrurlAgain, 0, ThCt_CatchTrurlAgain)
LV_FN(CatchTrurl, SetNextMission, 0, ThCt_SetNextMission)
LV_FN(CatchTrurl, CGCalls, 0, ThCt_CGCalls)

PyMethodDef g_TownhallCatchTrurlMethods[] = {
	{ "InitStatus", CatchTrurl_InitStatus, METH_VARARGS, NULL },
	{ "Init", CatchTrurl_Init, METH_VARARGS, NULL },
	{ "CreateTrurl", CatchTrurl_CreateTrurl, METH_VARARGS, NULL },
	{ "OnCatchTrurl", CatchTrurl_OnCatchTrurl, METH_VARARGS, NULL },
	{ "OnCatchTrurlAgain", CatchTrurl_OnCatchTrurlAgain, METH_VARARGS, NULL },
	{ "FunctionaryMissionSpeech", CatchTrurl_FunctionaryMissionSpeech, METH_VARARGS, NULL },
	{ "SetCallbackFuncs", CatchTrurl_SetCallbackFuncs, METH_VARARGS, NULL },
	{ "SpawnMap", CatchTrurl_SpawnMap, METH_VARARGS, NULL },
	{ "CreateMap", CatchTrurl_CreateMap, METH_VARARGS, NULL },
	{ "TakeMapPhoto", CatchTrurl_TakeMapPhoto, METH_VARARGS, NULL },
	{ "KillFunctionaries", CatchTrurl_KillFunctionaries, METH_VARARGS, NULL },
	{ "CatchTrurlAgain", CatchTrurl_CatchTrurlAgain, METH_VARARGS, NULL },
	{ "SetNextMission", CatchTrurl_SetNextMission, METH_VARARGS, NULL },
	{ "CGCalls", CatchTrurl_CGCalls, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_SheriffMayor ---- */

/* InitStatus(LevelPath): the police boss and the mayor of the mission */
static byte ThSm_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LvN_MissionChar("PoliceBoss_SheriffMayor", SM, "CreatePoliceBoss") && LvN_MissionChar("Mayor_SheriffMayor", SM, "CreateMayor");
}

/* CreateMayor(e): the mayor stays */
static byte ThSm_CreateMayor(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Th_Stay6(&a[0]);
}

/* CreatePoliceBoss(e): the police boss stays and talks */
static byte ThSm_CreatePoliceBoss(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_SetS(&a[0], "UseFunc", "Mission_SheriffMayor.PoliceBossMissionSpeech") && Th_Stay6(&a[0]);
}

/* e.var == i: 1, 0, -1 with the error */
static int Th_VarIs(const nativeVal_t *e, const char *var, long i) {
	nativeVal_t v = { 0 };
	Native_SetNone(&v);
	int r = Mode_VObj(e, var, &v) ? Mode_CmpI(&v, "==", i) : -1;
	Native_Free(&v);
	return r;
}

/* ReadyToTalk(e): `e and e.LLogicIdle == 0 and e.LLogicState == 6`, the value of the and */
static byte ThSm_ReadyToTalk(const nativeVal_t *a, nativeVal_t *out) {
	if (!Native_True(&a[0])) {
		Native_Copy(out, &a[0]);
		return 1;
	}
	int r = Th_VarIs(&a[0], "LLogicIdle", 0);
	if (r > 0) {
		r = Th_VarIs(&a[0], "LLogicState", 6);
	}
	if (r < 0) {
		return 0;
	}
	Native_SetInt(out, r);
	return 1;
}

/* Blablibla(EntityName="Mayor_SheriffMayor"): the police boss and the mayor talk in turns while both idle */
static byte ThSm_BlabliblaN(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sheriff = Mode_V(&t), *mayor = Mode_V(&t), *me = Mode_V(&t), *doit = Mode_V(&t), *act = Mode_V(&t);
	nativeVal_t *sl = Mode_V(&t), *anim = Mode_V(&t), *next = Mode_V(&t);
	if (!LvN_Entity("PoliceBoss_SheriffMayor", sheriff) || !LvN_Entity("Mayor_SheriffMayor", mayor) || !Mode_EntityH(name, me) ||
		!LV_OWN(SM, "ReadyToTalk", doit, "v", sheriff)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(doit)) {
		Native_Free(doit);
		if (!LV_OWN(SM, "ReadyToTalk", doit, "v", mayor)) {
			return Mode_Done(&t, 0);
		}
	}
	if (Native_True(doit)) {
		/* doit = me.Action[:5] == "Relax" */
		if (!Mode_VGet(me, "Action", act) || !Native_Slice(act, 0, 5, sl)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(doit);
		Native_SetInt(doit, LvN_EqS(sl, "Relax"));
	}
	if (Native_True(doit)) {
		Th_TalkAnim(anim);
		if (!Mode_VSet(me, "Action", anim)) {
			return Mode_Done(&t, 0);
		}
	}
	/* the other one talks next */
	int notMayor = Th_Cmp(name, Mode_S(&t, "Mayor_SheriffMayor"));
	int other = notMayor == 2 ? 2 : Th_Cmp(doit, Mode_I(&t, notMayor != 0));
	if (other == 2) {
		return Mode_Done(&t, 0);
	}
	LvN_Str(next, other ? "PoliceBoss_SheriffMayor" : "Mayor_SheriffMayor");
	return Mode_Done(&t, Mode_Later(2.0, SM, "Blablibla", next, 1, NULL));
}

TH_KWFN(SheriffMayor_Blablibla, "EntityName", "Mayor_SheriffMayor", ThSm_BlabliblaN)

/* PoliceBossMissionSpeech(UsrName, CharName, optId, FullOption): the police boss tells his clues to the mayor, or
 * sells them to Dtritus (OnPay); after that his usual talk */
static byte ThSm_PoliceBossMissionSpeech(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t pb = { 0 };
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	Native_SetNone(&pb);
	int done = LvN_Entity("PoliceBoss_SheriffMayor", &pb) ? Th_VarIs(&pb, "ArrowCanMission", 0) : -1;
	Native_Free(&pb);
	if (done < 0) {
		return 0;
	}
	if (done) {
		int talked = LvN_GTrue(SM, "alreadyTalked");
		if (talked <= 0) {
			return talked == 0 && LvN_RoutineCall("PoliceBoss", a);
		}
		int mayor = Mode_UsrIs("Mayor");
		if (mayor < 0) {
			return 0;
		}
		return mayor ? Mode_Spch("Mission_Get1stClue_PoliceBoss_InitMsg1", "Mission_Get1stClue_PoliceBoss_InitAns1", NULL) :
			Mode_Spch("Mission_GetInfoClues_PoliceBoss_InitMsg3", "Mission_GetInfoClues_PoliceBoss_InitAns3", NULL);
	}
	int mayor = Mode_UsrIs("Mayor");
	if (mayor < 0) {
		return 0;
	}
	if (mayor) {
		return Mode_SetLockAlarmI(1) && LvN_OnEnd(SM, "SetNextMission", NULL) &&
			Mode_Spch("Mission_GetInfoClues_PoliceBoss_InitMsg2", "Mission_GetInfoClues_PoliceBoss_InitAns2", NULL);
	}
	nativeVal_t money = { 0 }, reward = { 0 }, params = { 0 };
	Native_SetInt(&money, Mode_GetMoney());
	if (!Mode_G(SM, "MoneyReward", &reward)) {
		Native_Free(&reward);
		return 0;
	}
	int rich = Th_Cmp(&money, &reward);
	Native_Free(&reward);
	if (rich == 2) {
		return 0;
	}
	if (rich < 0) {
		return Mode_Spch("Mission_GetInfoClues_PoliceBoss_InitMsg3", "Mission_GetInfoClues_PoliceBoss_InitAns3", NULL);
	}
	if (!Mode_SetLockAlarmI(1)) {
		return 0;
	}
	Native_SetEmpty(&params, NV_TUPLE);
	Native_Copy(Native_Append(&params), &a[1]);
	byte ok = LvN_OnEnd(SM, "OnPay", &params);
	Native_Free(&params);
	return ok && Mode_Spch("Mission_GetInfoClues_PoliceBoss_InitMsg1", "Mission_GetInfoClues_PoliceBoss_InitAns1", NULL);
}

/* OnPay(CharName): the clues paid; the police boss carries the money */
static byte ThSm_OnPay(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *money = Mode_V(&t), *reward = Mode_V(&t), *rest = Mode_V(&t), *pb = Mode_V(&t);
	(void)out;
	Native_SetInt(money, Mode_GetMoney());
	if (!Mode_G(SM, "MoneyReward", reward) || !Native_Arith('-', money, reward, rest) || !Mode_SetMoney(rest) ||
		!LV_CALL("SaveGame", "SaveMoney", NULL, "") || !Mode_EntityH(&a[0], pb)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(reward);
	return Mode_Done(&t, Mode_G(SM, "MoneyReward", reward) && Mode_VSet(pb, "Money", reward) && LvN_SetI(pb, "MoneyReward", 1000) &&
		LV_OWN(SM, "SetNextMission", NULL, ""));
}

/* SetNextMission(): the police boss goes, the mayor comes back, the mission of NextMissionData */
static byte ThSm_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t data = { 0 };
	(void)a;
	(void)out;
	if (!LvN_PutI(SM, "alreadyTalked", 1) || !Mode_SetLockAlarmI(0)) {
		return 0;
	}
	SaveVar_Set("PoliceBossAtPolice", "0");
	SaveVar_Set("MayorAtTownHall", "1");
	if (!Mode_Target(NULL)) {
		return 0;
	}
	/* apply(MissionsFuncs.SetNextMission, NextMissionData) through __builtin__.apply (OBJLIB 3) */
	PyObject *fn = Script_Attr("MissionsFuncs", "SetNextMission");
	byte ok = fn && Mode_G(SM, "NextMissionData", &data) && Native_CallMod("__builtin__", "apply", NULL, "Ov", fn, &data);
	Py_XDECREF(fn);
	if (fn) {
		Native_Free(&data);
	}
	return ok && Mode_MMDesc("Mission_Get1stClue_MissionText1", 1L);
}

LV_FN(SheriffMayor, InitStatus, 1, ThSm_InitStatus)
LV_FN(SheriffMayor, CreateMayor, 1, ThSm_CreateMayor)
LV_FN(SheriffMayor, CreatePoliceBoss, 1, ThSm_CreatePoliceBoss)
LV_FN(SheriffMayor, ReadyToTalk, 1, ThSm_ReadyToTalk)
LV_FN(SheriffMayor, PoliceBossMissionSpeech, 4, ThSm_PoliceBossMissionSpeech)
LV_FN(SheriffMayor, OnPay, 1, ThSm_OnPay)
LV_FN(SheriffMayor, SetNextMission, 0, ThSm_SetNextMission)

PyMethodDef g_TownhallSheriffMayorMethods[] = {
	{ "InitStatus", SheriffMayor_InitStatus, METH_VARARGS, NULL },
	{ "CreateMayor", SheriffMayor_CreateMayor, METH_VARARGS, NULL },
	{ "CreatePoliceBoss", SheriffMayor_CreatePoliceBoss, METH_VARARGS, NULL },
	{ "ReadyToTalk", SheriffMayor_ReadyToTalk, METH_VARARGS, NULL },
	{ "Blablibla", (PyCFunction)(void (*)(void))SheriffMayor_Blablibla, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "PoliceBossMissionSpeech", SheriffMayor_PoliceBossMissionSpeech, METH_VARARGS, NULL },
	{ "OnPay", SheriffMayor_OnPay, METH_VARARGS, NULL },
	{ "SetNextMission", SheriffMayor_SetNextMission, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_TalkToMayor ---- */

/* InitStatus(LevelPath): the mayor's AI after the NPCs are browsed, Porkins the bishop and Corruptus */
static byte ThTm_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LvN_ModAppendG("CharsNPC", "AfterBrowseNPC", TM, "AfterBrowseNPC") && LvN_MissionChar("Bishop_Porkins", TM, "CreateBishopPorkins") &&
		LvN_MissionChar("Functionary_Tamayo", TM, "CreateFunctionaryTamayo");
}

/* AfterBrowseNPC(): the mayor stays (CreateMayorInit), his gear stays too */
static byte ThTm_AfterBrowseNPC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t key = { 0 }, stay = { 0 };
	(void)a;
	(void)out;
	if (!Mode_Import("CharsNPC") || !LvN_Callback("Mayor_Default", TM, "CreateMayorInit")) {
		return 0;
	}
	/* CharsNPC.CharRes["Gear_Mayor"].ExtraInfo = "STAY" (OBJLIB 5) */
	LvN_Str(&key, "Gear_Mayor");
	PyObject *gear = Mode_CharRes(&key);
	Native_Free(&key);
	LvN_Str(&stay, "STAY");
	byte ok = gear && Mode_HSet(gear, "ExtraInfo", &stay);
	Native_Free(&stay);
	Py_XDECREF(gear);
	return ok;
}

/* CreateMayorInit(e): the mayor stays and talks (MayorSpeech) */
static byte ThTm_CreateMayorInit(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Th_MayorInit(&a[0], "Mission_TalkToMayor.MayorSpeech");
}

/* e.Descriptor = the name text; e.Money = money; e.MoneyReward = money / slaps (the global money read for each); the
 * global nameVar = e.Name */
static byte Tm_Rich(const nativeVal_t *e, const char *key, const char *money, long slaps, const char *nameVar) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *desc = Mode_V(&t), *m = Mode_V(&t), *m2 = Mode_V(&t), *r = Mode_V(&t);
	Mode_Lang(key, desc);
	if (!Mode_VSet(e, "Descriptor", desc) || !Mode_G(TM, money, m) || !Mode_VSet(e, "Money", m) || !Mode_G(TM, money, m2) ||
		!Mode_Arith('/', m2, Mode_I(&t, slaps), r) || !Mode_VSet(e, "MoneyReward", r)) {
		return Mode_Done(&t, 0);
	}
	if (!nameVar) {
		return Mode_Done(&t, 1);
	}
	Native_Free(desc);
	return Mode_Done(&t, Mode_VGet(e, "Name", desc) && Mode_Put(TM, nameVar, NULL, desc));
}

/* CreateBishopPorkins(e): Porkins stays with his money (40 slaps empty him) */
static byte ThTm_CreateBishopPorkins(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Th_Stay6(&a[0]) && Tm_Rich(&a[0], "CharName_Porkins", "porkinsMoney", 40, "porkinsName");
}

/* the global var of the mission = e.attr */
static byte Tm_Keep(const nativeVal_t *e, const char *attr, const char *var) {
	nativeVal_t v = { 0 };
	Native_SetNone(&v);
	byte ok = Mode_VGet(e, attr, &v) && Mode_Put(TM, var, NULL, &v);
	Native_Free(&v);
	return ok;
}

/* CreateFunctionaryTamayo(e): Corruptus walks around with his money (10 slaps empty him) */
static byte ThTm_CreateFunctionaryTamayo(const nativeVal_t *a, nativeVal_t *out) {
	const nativeVal_t *e = &a[0];
	(void)out;
	return LvN_SetF(e, "LLogicProbTalkWhenWalking", 1.0) && LvN_SetI(e, "LLogicFastEscape", 0) && LvN_SetI(e, "LLogicStopInRelax", 0) &&
		LvN_SetI(e, "LLogicDefaultState", 0) && LvN_SetS(e, "LLogicDefaultTarget", "") && LvN_SetI(e, "LLogicState", 0) &&
		LvN_SetS(e, "LLogicTarget", "") && Tm_Rich(e, "CharName_Corruptus", "tamayoMoney", 10, NULL) &&
		Tm_Keep(e, "LLogicOrigin", "tamayoStartOrigin") && Tm_Keep(e, "Name", "functionaryTamayoName") &&
		(!LvN_SaveIs("MissionState", "StealFunctionary") || LvN_SetS(e, "UseFunc", "Mission_TalkToMayor.FunctionarySpeech"));
}

/* Init(LevelPath): Rusty's head; the mayor is the target, or the stealing goes on */
static byte ThTm_Init(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t state = { 0 }, r = { 0 };
	(void)a;
	(void)out;
	if (!Mode_Import("string")) {
		return 0;
	}
	/* SScorer.AddModel(0, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3, 25) */
	cScorer *sc = Scorer_Get(0);
	if (sc) {
		cScorer_AddModel(sc, "Rusty", "Models/Heads/HeadRusty/HeadRusty", NULL, 3, 25.0f, 0.0f, 0);
	}
	Mode_SaveVar("MissionState", &state);
	Native_SetNone(&r);
	/* OBJLIB 3: string.find by name */
	int talk = PORT_CALL(string, find, &r, "vs", &state, "TalkToMayor") ? Mode_CmpI(&r, "!=", -1) : -1;
	Native_Free(&r);
	byte ok = talk >= 0;
	if (talk > 0) {
		ok = Mode_Target("Mayor_Default");
	} else if (ok && LvN_EqS(&state, "StealFunctionary")) {
		ok = LV_OWN(TM, "StealFunctionary", NULL, "");
	}
	Native_Free(&state);
	return ok;
}

/* MayorSpeech(UsrName, CharName, optId, FullOption): the mayor's talk to Dtritus for the state of the mission */
static byte ThTm_MayorSpeech(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t state = { 0 };
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	int dt = Mode_UsrIs("Dtritus");
	if (dt <= 0) {
		return dt == 0 && Mode_Spch("Mission_TalkToMayor_Mayor_InitMsg2", "Mission_TalkToMayor_Mayor_InitAns2", NULL);
	}
	Mode_SaveVar("MissionState", &state);
	byte ok = 1;
	if (!Native_True(&state)) {
		ok = Mode_Spch("Mission_Campaign2_Mayor_InitMsg1", "Mission_Campaign2_Mayor_InitAns1", NULL);
	} else if (LvN_EqS(&state, "TalkToMayor1")) {
		if (LvN_EqS(&a[2], "Talk")) {
			const char *ans = "Mission_TalkToMayor_Mayor_1";
			ok = Mode_Spch("Mission_TalkToMayor_Mayor_InitMsg1", "Mission_TalkToMayor_Mayor_InitAns1", NULL);
			ok = ok && Mode_AnsLst(&ans, 1);
		} else {
			ok = LV_CALL("Speech", "RestoreState", NULL, "v", &a[1]) &&
				Mode_Spch("Mission_TalkToMayor_Mayor_1Msg", "Mission_TalkToMayor_Mayor_1Ans", NULL) &&
				LvN_OnEnd(TM, "PreStealFunctionary", NULL);
		}
	} else if (LvN_EqS(&state, "StealFunctionary")) {
		ok = Mode_Spch("Mission_Campaign1_Mayor_InitMsg1", "Mission_Campaign1_Mayor_InitAns1", NULL);
	} else if (LvN_EqS(&state, "TalkToMayor2")) {
		ok = Mode_Spch("Mission_TalkToMayor2_Mayor_InitMsg1", "Mission_TalkToMayor2_Mayor_InitAns1", NULL) &&
			LvN_OnEnd(TM, "SetNextMission", NULL);
	}
	Native_Free(&state);
	return ok;
}

/* BishopSpeech(...), FunctionarySpeech(...): the talks of Porkins and Corruptus */
static byte Tm_Speech(const nativeVal_t *a, const char *msg, const char *ans) {
	return LvN_Speakers(&a[0], &a[1]) && Mode_Spch(msg, ans, NULL);
}

MODE_FN(TalkToMayor, BishopSpeech, 4, Tm_Speech(a, "Mission_Campaign1_Porkins_InitMsg1", "Mission_Campaign1_Porkins_InitAns1"))
MODE_FN(TalkToMayor, FunctionarySpeech, 4, Tm_Speech(a, "Mission_Campaign_Functionary_InitMsg1", "Mission_Campaign_Functionary_InitAns1"))

/* StealFunctionary(): Porkins and Corruptus are the targets, the money bar, the money check */
static byte ThTm_StealFunctionary(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *m = Mode_V(&t), *list = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_GEntity(TM, "porkinsName", e) || (Native_True(e) && !LvN_SetS(e, "UseFunc", "Mission_TalkToMayor.BishopSpeech"))) {
		return Mode_Done(&t, 0);
	}
	Native_Free(e);
	if (!Mode_GEntity(TM, "functionaryTamayoName", e) || (Native_True(e) && !(LvN_SetS(e, "UseFunc", "Mission_TalkToMayor.FunctionarySpeech") &&
		Mode_G(TM, "tamayoMoney", m) && Mode_VSet(e, "Money", m)))) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(list, NV_LIST);
	if (!Mode_G(TM, "functionaryTamayoName", Native_Append(list)) || !Mode_G(TM, "porkinsName", Native_Append(list))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, LV_CALL("MissionsFuncs", "SetMultipleMissionTargets", NULL, "v", list) && LV_OWN(TM, "CheckTamayoMoney", NULL, "") &&
		LvN_ScorerCallG(TM, "MissionScorer") && LvN_Draw());
}

/* Corruptus low on money goes to Porkins and waits in front of him */
static byte Tm_GoToPorkins(const nativeVal_t *e, const nativeVal_t *p) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *z = Mode_V(&t), *oa = Mode_V(&t), *dir = Mode_V(&t), *pr = Mode_V(&t), *er = Mode_V(&t), *s = Mode_V(&t);
	nativeVal_t *dist = Mode_V(&t), *po = Mode_V(&t), *prod = Mode_V(&t), *sum = Mode_V(&t), *mz = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *pn = Mode_V(&t);
	Native_SetVec(z, 0.0, 0.0, 1.0);
	Native_SetVec(mz, 0.0, 0.0, -1.0);
	return Mode_Done(&t, Mode_VGet(p, "LLogicOriginAng", oa) && Mode_Rotate(z, oa, dir) && Mode_VGet(p, "VRadius", pr) &&
		Mode_VGet(e, "VRadius", er) && Native_Arith('+', pr, er, s) && Native_Arith('+', s, Mode_I(&t, 50), dist) &&
		Mode_VGet(p, "LLogicOrigin", po) && Mode_VecProd(dir, dist, prod) && Mode_VecAdd(po, prod, sum) &&
		Mode_VSet(e, "LLogicOrigin", sum) && Mode_Rotate(mz, dir, ang) && Mode_VSet(e, "LLogicOriginAng", ang) &&
		Mode_G(TM, "porkinsName", pn) && LvN_Stay(e, 6, pn) && LvN_PutI(TM, "tamayoHasJustReceivedMoney", 0));
}

/* Corruptus in front of Porkins: they talk and Porkins fills Corruptus' money up to MAX_TAMAYO_MONEY */
static byte Tm_Refill(const nativeVal_t *e, const nativeVal_t *p) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ep = Mode_V(&t), *pp = Mode_V(&t), *d = Mode_V(&t), *sq = Mode_V(&t), *mz = Mode_V(&t), *pa = Mode_V(&t);
	nativeVal_t *bdir = Mode_V(&t), *tof = Mode_V(&t), *ang = Mode_V(&t), *anim = Mode_V(&t), *pm = Mode_V(&t);
	if (!Mode_VGet(e, "Pos", ep) || !Mode_VGet(p, "Pos", pp) || !Mode_VecSub(ep, pp, d) || !Mode_VecModSqr(d, sq)) {
		return Mode_Done(&t, 0);
	}
	int close = Mode_CmpI(sq, "<=", 400 * 400);
	if (close <= 0) {
		return Mode_Done(&t, close == 0);
	}
	Native_SetVec(mz, 0.0, 0.0, -1.0);
	ep = Mode_V(&t);
	pp = Mode_V(&t);
	/* OBJLIB 7: SVec.GetAngle by name */
	if (!Mode_VGet(p, "Ang", pa) || !Mode_Rotate(mz, pa, bdir) || !Mode_VGet(e, "Pos", ep) || !Mode_VGet(p, "Pos", pp) ||
		!Mode_VecSub(ep, pp, tof) || !Native_CallMod("SVec", "GetAngle", ang, "vv", bdir, tof)) {
		return Mode_Done(&t, 0);
	}
	int facing = Mode_CmpI(ang, "<=", 30);
	if (facing <= 0) {
		return Mode_Done(&t, facing == 0);
	}
	Th_TalkAnim(anim);
	if (!Mode_VSet(e, "Action", anim)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(anim);
	Th_TalkAnim(anim);
	int rich = Mode_VSet(p, "Action", anim) && Mode_VGet(p, "Money", pm) ? Mode_CmpI(pm, ">", 0) : -1;
	if (rich < 0) {
		return Mode_Done(&t, 0);
	}
	if (rich) {
		nativeVal_t *pn = Mode_V(&t), *fn = Mode_V(&t), *mx = Mode_V(&t), *em = Mode_V(&t), *gives = Mode_V(&t), *pm2 = Mode_V(&t);
		nativeVal_t *em2 = Mode_V(&t), *sum = Mode_V(&t), *pm3 = Mode_V(&t), *rest = Mode_V(&t);
		int more;
		/* OBJLIB 3: Fx.MoneyTransfer by name */
		if (!Mode_Import("Fx") || !Mode_G(TM, "porkinsName", pn) || !Mode_G(TM, "functionaryTamayoName", fn) ||
			!LV_CALL("Fx", "MoneyTransfer", NULL, "vvi", pn, fn, 1L) || !Mode_G(TM, "MAX_TAMAYO_MONEY", mx) ||
			!Mode_VGet(e, "Money", em) || !Native_Arith('-', mx, em, gives) || !Mode_VGet(p, "Money", pm2) ||
			(more = Th_Cmp(gives, pm2)) == 2) {
			return Mode_Done(&t, 0);
		}
		if (more > 0) {
			Native_Free(gives);
			if (!Mode_VGet(p, "Money", gives)) {
				return Mode_Done(&t, 0);
			}
		}
		if (!Mode_VGet(e, "Money", em2) || !Native_Arith('+', em2, gives, sum) || !Mode_VSet(e, "Money", sum) ||
			!Mode_VGet(p, "Money", pm3) || !Native_Arith('-', pm3, gives, rest) || !Mode_VSet(p, "Money", rest)) {
			return Mode_Done(&t, 0);
		}
	}
	nativeVal_t *r = Mode_V(&t), *act = Mode_V(&t), *anm = Mode_V(&t), *now = Mode_V(&t), *s = Mode_V(&t), *at = Mode_V(&t);
	const char *action;
	/* SAct.GetClass, GetAction and GetAct through their seams */
	if (!LvN_PutI(TM, "tamayoHasJustReceivedMoney", 1) || !LvN_GetClass("Functionary", r) || !Mode_VGet(e, "Action", act) ||
		!Native_ArgStr(act, 1, &action) || !LvN_GetAction(action, r) || !LvN_GetAct("AnmTotalTime", anm)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(now, Mode_Time());
	nativeVal_t *dt = Mode_V(&t);
	Native_SetFloat(dt, 0.1);
	if (!Native_Arith('+', now, anm, s) || !Native_Arith('-', s, dt, at)) {
		return Mode_Done(&t, 0);
	}
	/* OBJLIB 4: the function of the module (a mission may replace it) */
	PyObject *f = Mode_GlobalPy(TM, "RestoreTamayoState");
	byte ok = f && Mode_Schedule(at, f, NULL, 0, NULL);
	Py_XDECREF(f);
	return Mode_Done(&t, ok);
}

/* CheckTamayoMoney(): every second: Corruptus' money on the bar; low on money he goes to Porkins for more */
static byte ThTm_CheckTamayoMoney(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *m = Mode_V(&t), *tm = Mode_V(&t), *fm = Mode_V(&t), *p = Mode_V(&t), *min = Mode_V(&t);
	nativeVal_t *st = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_GEntity(TM, "functionaryTamayoName", e)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(e)) {
		if (!Mode_VGet(e, "Money", m) || !Mode_Put(TM, "tamayoMoney", NULL, m) || !Mode_G(TM, "tamayoMoney", tm) ||
			!Native_ToFloat(tm, fm) || !LvN_ScorerSet(Mode_I(&t, 0), "TamayoMoneyBar", "Value", fm) || !Mode_GEntity(TM, "porkinsName", p)) {
			return Mode_Done(&t, 0);
		}
		if (Native_True(p)) {
			int low, c;
			Native_Free(tm);
			if (!Mode_G(TM, "tamayoMoney", tm) || !Mode_G(TM, "MIN_TAMAYO_MONEY", min) || (c = Th_Cmp(tm, min)) == 2) {
				return Mode_Done(&t, 0);
			}
			low = c <= 0;
			if (low) {
				low = Mode_VGet(e, "LLogicDefaultState", st) ? Mode_CmpI(st, "!=", 6) : -1;
			}
			if (low > 0) {
				if (!Tm_GoToPorkins(e, p)) {
					return Mode_Done(&t, 0);
				}
			} else if (low == 0) {
				int waits = Th_VarIs(e, "LLogicDefaultState", 6);
				if (waits > 0) {
					waits = Th_VarIs(p, "LLogicDefaultState", 6);
				}
				if (waits > 0) {
					waits = Th_VarIs(p, "LLogicIdle", 0);
				}
				if (waits > 0) {
					waits = LvN_GEqI(TM, "tamayoHasJustReceivedMoney", 0);
				}
				if (waits < 0 || (waits && !Tm_Refill(e, p))) {
					return Mode_Done(&t, 0);
				}
			} else {
				return Mode_Done(&t, 0);
			}
		}
	}
	return Mode_Done(&t, Mode_Later(1.0, TM, "CheckTamayoMoney", NULL, 0, "CheckTamayoMoney") && LV_OWN(TM, "OnBankerSteal", NULL, ""));
}

/* RestoreTamayoState(): after the talk Corruptus walks again from Porkins (or his start) */
static byte ThTm_RestoreTamayoState(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *p = Mode_V(&t), *o = Mode_V(&t), *empty = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_GEntity(TM, "functionaryTamayoName", e)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(e)) {
		return Mode_Done(&t, 1);
	}
	LvN_Str(empty, "");
	if (!Mode_GEntity(TM, "porkinsName", p) || !(Native_True(p) ? Mode_VGet(p, "Pos", o) : Mode_G(TM, "tamayoStartOrigin", o))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_VSet(e, "LLogicOrigin", o) && LvN_Stay(e, 0, empty) && LvN_SetF(e, "LLogicProbTalkWhenWalking", 1.0) &&
		LvN_SetI(e, "LLogicIgnoreBankerAttack", 0));
}

/* e.var <= the global name of the mission: 1, 0, -1 with the error */
static int Tm_AtMost(const nativeVal_t *e, const char *var, const char *name) {
	nativeVal_t v = { 0 }, g = { 0 };
	Native_SetNone(&v);
	Native_SetNone(&g);
	int c = Mode_VGet(e, var, &v) && Mode_G(TM, name, &g) ? Th_Cmp(&v, &g) : 2;
	Native_Free(&v);
	Native_Free(&g);
	return c == 2 ? -1 : c <= 0;
}

/* OnBankerSteal(): Corruptus without money ends the stealing (the mayor calls); low on money the bankers leave him;
 * Porkins without money once draws the mayor's call */
static byte ThTm_OnBankerSteal(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *m = Mode_V(&t), *p = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_GEntity(TM, "functionaryTamayoName", e)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(e)) {
		int empty = Mode_VGet(e, "Money", m) ? Mode_CmpI(m, "<=", 0) : -1;
		if (empty < 0) {
			return Mode_Done(&t, 0);
		}
		if (empty) {
			if (!Mode_Target(NULL) || !LvN_SetS(e, "UseFunc", "CharsNPC.GenericNPCSpeech") || !Mode_GEntity(TM, "porkinsName", p) ||
				(Native_True(p) && !LvN_SetS(p, "UseFunc", "CharsNPC.GenericNPCSpeech"))) {
				return Mode_Done(&t, 0);
			}
			return Mode_Done(&t, LvN_Com("Mission_Campaign1_RemoteMessage1", "Mayor", TM, "EndOfStealFunctionary", LvN_NoArgs()) &&
				Mode_DelSchedS("CheckTamayoMoney"));
		}
		int low = Tm_AtMost(e, "Money", "MIN_TAMAYO_MONEY");
		if (low < 0 || (low && !LvN_SetI(e, "LLogicIgnoreBankerAttack", 1))) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Free(m);
	if (!Mode_GEntity(TM, "porkinsName", p)) {
		return Mode_Done(&t, 0);
	}
	int poor = Native_True(p);
	if (poor) {
		poor = Mode_VGet(p, "Money", m) ? Mode_CmpI(m, "<=", 0) : -1;
	}
	if (poor > 0) {
		poor = LvN_GTrue(TM, "porkinsHasMoney");
	}
	if (poor < 0) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !poor || (LvN_PutI(TM, "porkinsHasMoney", 0) &&
		LvN_Com("Mission_Campaign1_RemoteMessage2", "Mayor", NULL, NULL, NULL)));
}

/* EndOfStealFunctionary(): the mayor is the target again */
static byte ThTm_EndOfStealFunctionary(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	SaveVar_Set("MissionState", "TalkToMayor2");
	return LvN_ScorerOff() && LvN_Draw() && Mode_MMDesc("Mission_TalkToMayor2_MissionText1", 1L) && Mode_WayPoint() && Mode_Target("Mayor_Default");
}

/* lst = eval(Scrap.GetSaveVar(var)); lst.append(item); Scrap.SetSaveVar(var, `lst`) */
static byte Tm_SaveAdd(const char *var, const char *item) {
	nativeVal_t l = { 0 }, it = { 0 };
	LvN_Str(&it, item);
	byte ok = LvN_SaveList(var, TM, &l) && LvN_SaveAppend(var, &l, &it);
	Native_Free(&l);
	Native_Free(&it);
	return ok;
}

/* SetNextMission(): Mission_DestroyPorkinsTransports in Commercial, two police blueprints more, Rusty calls in 10 s */
static byte ThTm_SetNextMission(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t call[4] = { 0 };
	(void)a;
	(void)out;
	SaveVars_Delete("MissionState");
	if (!Mode_Target(NULL) || !LV_CALL("MissionsFuncs", "SetNextMission", NULL, "ss", "Mission_DestroyPorkinsTransports", "Commercial") ||
		!Mode_MMDesc("Mission_Campaign2_MissionText1", 1L) || !Mode_WayPoint() || !Tm_SaveAdd("PoliceBlueprints.Ships", "SPoli5") ||
		!Tm_SaveAdd("PoliceBlueprints.Engines", "MPoli5") || !Mode_Import("Scorer")) {
		return 0;
	}
	/* Speech.AddRemoteCom in 10 s with ("Mission_Campaign2_RemoteMessage5", "Rusty", Scorer.ShowMissionScorer, (0, 2)) */
	LvN_Str(&call[0], "Mission_Campaign2_RemoteMessage5");
	LvN_Str(&call[1], "Rusty");
	Native_SetEmpty(&call[3], NV_TUPLE);
	Native_SetInt(Native_Append(&call[3]), 0);
	Native_SetInt(Native_Append(&call[3]), 2);
	byte ok = LvN_ModGet("Scorer", "ShowMissionScorer", &call[2]) && Mode_LaterIn(10.0, "Speech", "AddRemoteCom", call, 4, NULL);
	for (int i = 0; i < 4; i++) {
		Native_Free(&call[i]);
	}
	return ok;
}

/* MissionScorer(id): Corruptus' money as a bar */
static byte ThTm_MissionScorer(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *item = Mode_V(&t), *back = Mode_V(&t), *mainc = Mode_V(&t), *black = Mode_V(&t), *mx = Mode_V(&t), *v = Mode_V(&t);
	(void)out;
	LvN_Str(item, "TamayoMoneyBar");
	/* OBJLIB 3, 5: Scorer.MissionScorerBar_SetDefaults with the colours of Scorer by name */
	return Mode_Done(&t, Th_ScorerLabel(&a[0], "TamayoMoney", "Mission_Campaign1_Scorer1") && Mode_ScorerAdd(&a[0], item, "Bar") &&
		LvN_ModGet("Scorer", "MissionScorerBar_ColorBack", back) && LvN_ModGet("Scorer", "MissionScorerBar_ColorMain", mainc) &&
		LvN_ModGet("Scorer", "MissionScorerBar_ColorBlack", black) &&
		LV_CALL("Scorer", "MissionScorerBar_SetDefaults", NULL, "vvvvv", &a[0], item, back, mainc, black) &&
		Mode_ScorerSet(&a[0], item, "IsAutoUpdatable", Mode_I(&t, 0)) && Mode_G(TM, "MAX_TAMAYO_MONEY", mx) &&
		Mode_ScorerSet(&a[0], item, "MaxValue", mx) && Mode_G(TM, "tamayoMoney", v) && Mode_ScorerSet(&a[0], item, "Value", v));
}

LV_FN(TalkToMayor, InitStatus, 1, ThTm_InitStatus)
LV_FN(TalkToMayor, AfterBrowseNPC, 0, ThTm_AfterBrowseNPC)
LV_FN(TalkToMayor, CreateMayorInit, 1, ThTm_CreateMayorInit)
LV_FN(TalkToMayor, CreateBishopPorkins, 1, ThTm_CreateBishopPorkins)
LV_FN(TalkToMayor, CreateFunctionaryTamayo, 1, ThTm_CreateFunctionaryTamayo)
LV_FN(TalkToMayor, Init, 1, ThTm_Init)
LV_FN(TalkToMayor, MayorSpeech, 4, ThTm_MayorSpeech)
LV_FN(TalkToMayor, StealFunctionary, 0, ThTm_StealFunctionary)
LV_FN(TalkToMayor, CheckTamayoMoney, 0, ThTm_CheckTamayoMoney)
LV_FN(TalkToMayor, RestoreTamayoState, 0, ThTm_RestoreTamayoState)
LV_FN(TalkToMayor, OnBankerSteal, 0, ThTm_OnBankerSteal)
LV_FN(TalkToMayor, EndOfStealFunctionary, 0, ThTm_EndOfStealFunctionary)
LV_FN(TalkToMayor, SetNextMission, 0, ThTm_SetNextMission)
LV_FN(TalkToMayor, MissionScorer, 1, ThTm_MissionScorer)

PyMethodDef g_TownhallTalkToMayorMethods[] = {
	{ "InitStatus", TalkToMayor_InitStatus, METH_VARARGS, NULL },
	{ "AfterBrowseNPC", TalkToMayor_AfterBrowseNPC, METH_VARARGS, NULL },
	{ "CreateMayorInit", TalkToMayor_CreateMayorInit, METH_VARARGS, NULL },
	{ "CreateBishopPorkins", TalkToMayor_CreateBishopPorkins, METH_VARARGS, NULL },
	{ "CreateFunctionaryTamayo", TalkToMayor_CreateFunctionaryTamayo, METH_VARARGS, NULL },
	{ "Init", TalkToMayor_Init, METH_VARARGS, NULL },
	{ "MayorSpeech", TalkToMayor_MayorSpeech, METH_VARARGS, NULL },
	{ "BishopSpeech", TalkToMayor_BishopSpeech, METH_VARARGS, NULL },
	{ "FunctionarySpeech", TalkToMayor_FunctionarySpeech, METH_VARARGS, NULL },
	{ "StealFunctionary", TalkToMayor_StealFunctionary, METH_VARARGS, NULL },
	{ "CheckTamayoMoney", TalkToMayor_CheckTamayoMoney, METH_VARARGS, NULL },
	{ "RestoreTamayoState", TalkToMayor_RestoreTamayoState, METH_VARARGS, NULL },
	{ "OnBankerSteal", TalkToMayor_OnBankerSteal, METH_VARARGS, NULL },
	{ "EndOfStealFunctionary", TalkToMayor_EndOfStealFunctionary, METH_VARARGS, NULL },
	{ "SetNextMission", TalkToMayor_SetNextMission, METH_VARARGS, NULL },
	{ "MissionScorer", TalkToMayor_MissionScorer, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_Townhall)
