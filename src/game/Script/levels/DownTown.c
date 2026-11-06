/*
 * The scripts of the level DownTown in C (levels/downtown/scripts of Data.packed, issue #2): the Map module (the
 * traffic by who controls the robots, the metro and the elevators to the townhall and the press), MapRace (the start
 * positions of the sixteen races) and the missions of the level (Mission_FirstContactPress, Mission_BertoMassacre,
 * Mission_DefeatCrazyGambler1, Mission_FaceTheMayor, Mission_Massacre, Mission_DestroyCommSats3,
 * Mission_DistractHumphrey, Mission_GetDataPart1, Mission_HumphreyRace, Mission_MultiplusRace,
 * Mission_ProtectMayorTransport, Mission_StealMayorShip). MapSnd is data (data/levels/downtown/mapsnd.def), and so
 * are the plain functions of the missions. The module variables of the missions (their counters, states, names and
 * constants) and the module code (the save variables they create, MapRace.Races) stay in the scripts; the functions
 * read and write them as their globals.
 */
#include "Levels.h"

#define FC "Mission_FirstContactPress"
#define BM "Mission_BertoMassacre"
#define CG "Mission_DefeatCrazyGambler1"
#define FM "Mission_FaceTheMayor"
#define MS "Mission_Massacre"
#define CS "Mission_DestroyCommSats3"
#define DH "Mission_DistractHumphrey"
#define GD "Mission_GetDataPart1"
#define HR "Mission_HumphreyRace"
#define MR "Mission_MultiplusRace"
#define PM "Mission_ProtectMayorTransport"
#define SM "Mission_StealMayorShip"

#define ENT(n) Sf_Ask(&f, "Scrap", "GetEntity", "(s)", n)
#define USR() Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0)
#define SAVE(n) Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", n)
#define G(m, n) Sf_Global(&f, m, n)
#define NOARGS Sf_Build(&f, "()")

/* the parameter of InitStatus(LevelPath) and Init(LevelPath) */
#define PATH_ARGS() \
	PyObject *path; \
	SF_ARGS("O", &path); \
	(void)path

/* Speech.AddRemoteCom(key, who[, then[, params]]) (then NULL: two arguments; params NULL: three) */
static byte Dt_Com(scriptFrame_t *f, const char *key, const char *who, PyObject *then, PyObject *params) {
	if (!then) {
		return Sf_Run(f, "Speech", "AddRemoteCom", "(ss)", key, who);
	}
	return params ? Sf_Run(f, "Speech", "AddRemoteCom", "(ssOO)", key, who, then, params) :
		Sf_Run(f, "Speech", "AddRemoteCom", "(ssO)", key, who, then);
}

/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, Speech.AddRemoteCom, args[, name]) */
static byte Dt_LaterCom(scriptFrame_t *f, PyObject *delay, PyObject *args, const char *name) {
	int mark = f->n;
	PyObject *t = Sf_Op(f, Sf_Time(f), '+', delay);
	Sf_Schedule(f, t, Sf_Mod(f, "Speech", "AddRemoteCom"), args, name);
	Sf_Release(f, mark);
	return !f->failed;
}

/* Menu.MissionScorerCall = v */
static byte Dt_ScorerCall(scriptFrame_t *f, PyObject *v) {
	return Sf_SetMod(f, "Menu", "MissionScorerCall", v);
}

/* the X and Y of the mission scorer text plus offsets */
static PyObject *Dt_X(scriptFrame_t *f, long d) {
	PyObject *x = Sf_Mod(f, "Scorer", "MissionScorerText_X");
	return d ? Sf_OpI(f, x, '+', d) : x;
}

static PyObject *Dt_Y(scriptFrame_t *f, long d) {
	PyObject *y = Sf_Mod(f, "Scorer", "MissionScorerText_Y");
	return d ? Sf_OpI(f, y, '+', d) : y;
}

/* Scrap.GetLangStr(key) + ":" */
static PyObject *Dt_Label(scriptFrame_t *f, const char *key) {
	return Sf_Cat(f, "Os", Sf_Lang(f, Sf_Str(f, key)), ":");
}

/* the text of a mission scorer in Horatio with a shadow at the left (visible NULL: not set) */
static byte Dt_Text(scriptFrame_t *f, PyObject *id, const char *item, PyObject *y, PyObject *text, PyObject *visible) {
	return Sf_MissionText(f, id, item, "Text", Dt_X(f, 0), y, "Horatio", "Shadow", text, "Left", visible);
}

/* the timer of a mission scorer counting TimeSpecialUse, with the function of its end (visible: Visible 1) */
static byte Dt_Timer(scriptFrame_t *f, PyObject *id, const char *item, PyObject *x, PyObject *y, const char *expired, int visible) {
	int mark = f->n;
	Sf_Run(f, "SScorer", "Add", "(Oss)", id, item, "Timer");
	Sf_Run(f, "SScorer", "Set", "(OssO)", id, item, "X", x);
	Sf_Run(f, "SScorer", "Set", "(OssO)", id, item, "Y", y);
	Sf_Run(f, "SScorer", "Set", "(Osss)", id, item, "Align", "Left");
	Sf_Run(f, "SScorer", "Set", "(Osss)", id, item, "TimerType", "TimeSpecialUse");
	Sf_Run(f, "SScorer", "Set", "(Osss)", id, item, "OnTimeExpired", expired);
	Lv_Colour(f, id, item);
	if (visible) {
		Sf_Run(f, "SScorer", "Set", "(Ossi)", id, item, "Visible", 1);
	}
	Sf_Release(f, mark);
	return !f->failed;
}

/* MissionsFuncs.SetNextMission(lib, map); SaveGame.MMDesc(desc, 1) */
static byte Dt_Next(scriptFrame_t *f, const char *lib, const char *map, const char *desc) {
	return Sf_Run(f, "MissionsFuncs", "SetNextMission", "(ss)", lib, map) && Sf_Run(f, "SaveGame", "MMDesc", "(si)", desc, 1);
}

/* `usr and usr.Name == name` */
static int Dt_UsrIs(scriptFrame_t *f, PyObject *usr, PyObject *name) {
	int t = Sf_True(f, usr);
	return t > 0 ? Sf_Compare(f, Sf_Attr(f, usr, "Name"), "==", name) : t;
}

/* `e and e.Active` */
static int Dt_Active(scriptFrame_t *f, PyObject *e) {
	int t = Sf_True(f, e);
	return t > 0 ? Sf_True(f, Sf_Attr(f, e, "Active")) : t;
}

/* Scrap.GetAlarm()[0] as a condition */
static int Dt_Alarm(scriptFrame_t *f) {
	return Sf_True(f, Sf_Item(f, Sf_Ask(f, "Scrap", "GetAlarm", "()"), 0));
}

/* pc = PhoneCab.GetFree(); MissionsFuncs.SetMissionTarget(pc.Name); pc.UseFunc = use (after import PhoneCab) */
static PyObject *Dt_PhoneCab(scriptFrame_t *f, const char *use) {
	Sf_Import(f, "PhoneCab");
	PyObject *pc = Sf_Ask(f, "PhoneCab", "GetFree", "()");
	Sf_Run(f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(f, pc, "Name"));
	Sf_SetAttrS(f, pc, "UseFunc", use);
	return pc;
}

/*
 * a mission item dropped at pos (the matrix, the data): its model and radius are globals of module, OnlyUser 1, with
 * its take sound and a mission arrow above it named arrow
 */
static byte Dt_Drop(scriptFrame_t *f, const char *module, PyObject *pos, const char *name, const char *model, const char *radius,
	const char *onTake, const char *sound, const char *arrow) {
	int mark = f->n;
	PyObject *x = Sf_Item(f, pos, 0), *y = Sf_Item(f, pos, 1), *z = Sf_Item(f, pos, 2);
	PyObject *l = Sf_Ask(f, "Scrap", "CreateEntity", "(sOOOs)", name, x, y, z, "Item");
	Sf_Method(f, l, "SetModel", "(O)", Sf_Global(f, module, model));
	Sf_SetAttr(f, l, "Radius", Sf_Global(f, module, radius));
	Sf_SetAttrI(f, l, "AppearsIfFree", 0);
	Sf_SetAttrI(f, l, "AutoFace", 0);
	Sf_SetAttrI(f, l, "OnlyUser", 1);
	Sf_SetAttr(f, l, "rTime", Sf_Float(f, 0.25));
	Sf_SetAttrS(f, l, "OnTake", onTake);
	Sf_Import(f, "Items");
	Sf_Run(f, "Items", "SetSound", "(Os)", l, sound);
	x = Sf_Item(f, pos, 0), y = Sf_Item(f, pos, 1), z = Sf_Item(f, pos, 2);
	PyObject *l2 = Sf_Ask(f, "Scrap", "CreateEntity", "(sOOOs)", arrow, x, y, z, "WithLife");
	Sf_SetAttr(f, l2, "CamTgt", Sf_Build(f, "(iii)", 0, 1000, 0));
	Sf_Run(f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(f, l2, "Name"));
	Sf_Release(f, mark);
	return !f->failed;
}

/* a mission item taken: itm.rTime = 20, deleted 5 s later, its arrow deleted now */
static byte Dt_Taken(scriptFrame_t *f, const char *item, const char *arrow) {
	int mark = f->n;
	PyObject *itm = Sf_Ask(f, "Scrap", "GetEntity", "(s)", item);
	Sf_SetAttrI(f, itm, "rTime", 20);
	Sf_Later(f, Sf_Int(f, 5), Sf_Attr(f, itm, "Del"), Sf_Build(f, "()"), NULL);
	Sf_Method(f, Sf_Ask(f, "Scrap", "GetEntity", "(s)", arrow), "Del", "()");
	Sf_Release(f, mark);
	return !f->failed;
}

/* ---- Map ---- */

/* InitStatus(LevelPath): outdoors by the engine; the elevator by the map the player comes from, the metro; the
 * traffic by who controls the robots */
static PyObject *DownTown_InitStatus(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Init", "Chars", "Elevator", "Traffic", "OutPolice", "string", "SAI", NULL };
	PATH_ARGS();
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 0));
	Sf_Ask(&f, "SAI", "GetOD", "()");
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Ask(&f, "SAI", "GetOD", "()"));
	Sf_Except(&f);
	PyObject *pm = Sf_Ask(&f, "string", "upper", "(O)", Sf_Slice(&f, SAVE("PrevMap"), 7, 0x7fffffff));
	int press = Sf_EqS(&f, pm, "PRESS");
	if (press > 0) {
		Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Elevator", "DM_Door_Elevator_Press");
	} else if (press == 0) {
		Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Elevator", "DM_Door_Elevator_TownHall");
	}
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Metro", "DM_Door_MetroExt_Downtown");
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "MetroExt", "DM_Door_MetroExt_Downtown");
	int boss = Sf_EqS(&f, SAVE("RobotsControlledByBoss"), "1");
	if (boss > 0) {
		PyObject *upper = Sf_Mod(&f, "Init", "UpperMap");
		int done = Sf_In(&f, upper, Sf_Eval(&f, SAVE("ComSatsMissionsMapsFinished"), "Map"));
		if (done > 0) {
			Sf_Run(&f, "Scrap", "Set", "(si)", "MaxChasingShips", 0);
			Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 0);
			Sf_SetMod(&f, "OutPolice", "PoliceActive", Sf_Int(&f, 0));
		} else if (done == 0) {
			Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 5);
			Sf_Run(&f, "Scrap", "Set", "(si)", "MaxChasingShips", 5);
		}
		Sf_SetMod(&f, "Traffic", "AcNormalShips", Sf_Build(&f, "[]"));
		Sf_SetMod(&f, "Traffic", "NumNormal", Sf_Int(&f, 0));
		Sf_SetMod(&f, "Traffic", "AcMediumShips", Sf_Build(&f, "[]"));
		Sf_SetMod(&f, "Traffic", "NumMedium", Sf_Int(&f, 0));
		Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[]"));
		Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 0));
	} else if (boss == 0) {
		Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 14);
		Sf_Run(&f, "Scrap", "Set", "(si)", "MaxChasingShips", -1);
		Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[]"));
		Sf_SetMod(&f, "Traffic", "NumNormal", Sf_Int(&f, 80));
		Sf_SetMod(&f, "Traffic", "NumMedium", Sf_Int(&f, 20));
		Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 0));
	}
	return Sf_None(&f);
}

/* Init(LevelPath): the elevators to the townhall and the press, the metro */
static PyObject *DownTown_Init(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Import(&f, "Elevator");
	Sf_Import(&f, "Metro");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_TownHall", "TownHall");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_Press", "Press");
	Sf_Run(&f, "Metro", "SetAsMetro", "(s)", "DM_Door_MetroExt_Downtown");
	return Sf_None(&f);
}

PyMethodDef g_DownTownMapMethods[] = {
	{ "InitStatus", DownTown_InitStatus, 1, NULL },
	{ "Init", DownTown_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapRace ---- */

#include "DownTownRaces.inc"

#define RACE(k) \
	static PyObject *MapRace_Race##k(PyObject *self, PyObject *args) { \
		(void)self; \
		return Lv_StartPos(args, races[k - 1].pos, races[k - 1].n); \
	}
RACE(1)
RACE(2)
RACE(3)
RACE(4)
RACE(5)
RACE(6)
RACE(7)
RACE(8)
RACE(9)
RACE(10)
RACE(11)
RACE(12)
RACE(13)
RACE(14)
RACE(15)
RACE(16)

PyMethodDef g_DownTownMapRaceMethods[] = {
	{ "EasyRaceDownTown1", MapRace_Race1, 1, NULL },
	{ "EasyRaceDownTown2", MapRace_Race2, 1, NULL },
	{ "EasyRaceDownTown3", MapRace_Race3, 1, NULL },
	{ "EasyRaceDownTown4", MapRace_Race4, 1, NULL },
	{ "MediumRaceDownTown1", MapRace_Race5, 1, NULL },
	{ "MediumRaceDownTown2", MapRace_Race6, 1, NULL },
	{ "MediumRaceDownTown3", MapRace_Race7, 1, NULL },
	{ "MediumRaceDownTown4", MapRace_Race8, 1, NULL },
	{ "MediumRaceDownTown5", MapRace_Race9, 1, NULL },
	{ "MediumRaceDownTown6", MapRace_Race10, 1, NULL },
	{ "MediumRaceDownTown7", MapRace_Race11, 1, NULL },
	{ "MediumRaceDownTown8", MapRace_Race12, 1, NULL },
	{ "HardRaceDownTown1", MapRace_Race13, 1, NULL },
	{ "HardRaceDownTown2", MapRace_Race14, 1, NULL },
	{ "HardRaceDownTown3", MapRace_Race15, 1, NULL },
	{ "HardRaceDownTown4", MapRace_Race16, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_FirstContactPress: Berto calls, on to the press ---- */

static PyObject *Press_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, FC,
		"[\"SPoliBoss1\", \"SMerc1\",\"SMerc2\",\"SMerc3\",\"SLMerc\",\"SMayor1\",\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBerto1\",\"SHump1\","
		"\"SBetty1\",\"SBankMaster1\",\"SArchbishop1\"]");
}

/* Init(LevelPath): Berto's call, then on to the press; C although the converter made it data, because the .def
 * reader keeps the quotes of the texts in the tuple of parameters ("Mission_MeetBoss" with its quotes) */
static PyObject *Press_Init(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Import(&f, "MissionsFuncs");
	Sf_Import(&f, "Speech");
	Dt_Com(&f, "Mission_FirstContactPress_RemoteMessage1", "Berto", Sf_Mod(&f, "MissionsFuncs", "SetNextMission"),
		Sf_Build(&f, "(ss)", "Mission_MeetBoss", "Press"));
	return Sf_None(&f);
}

PyMethodDef g_DownTownFirstContactPressMethods[] = {
	{ "InitStatus", Press_InitStatus, 1, NULL },
	{ "Init", Press_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_BertoMassacre: the evil Bertos in waves; after the last of the bar one of them carries the matrix ---- */

/* InitStatus(LevelPath): no Bertos before DeepThroat's message */
static PyObject *Berto_InitStatus(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "CreateBertos", "0");
	Lv_AcShips(&f, BM, "[\"SFunc1\",\"SHump1\",\"SBetty1\"]");
	return Sf_None(&f);
}

/* StartMission(): the scorer, the waves of Bertos and their callbacks */
static PyObject *Berto_StartMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_BertoMassacre_MissionText1", 1);
	Dt_ScorerCall(&f, G(BM, "MissionScorer"));
	Sf_DrawScorer(&f);
	Sf_Run(&f, "MissionsFuncs", "CreateBertos", "()");
	Sf_SetMod(&f, "MissionsFuncs", "OnNewWaveCallback", G(BM, "DeepThroatMessage"));
	Sf_SetMod(&f, "MissionsFuncs", "OnBertoDeathCallback", G(BM, "CheckBertoDeath"));
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallbackAux", Sf_Mod(&f, "MissionsFuncs", "ReassignBertosPlayerTarget"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallbackAux", Sf_Mod(&f, "MissionsFuncs", "ReassignBertosPlayerTarget"));
	return Sf_None(&f);
}

/* DeepThroatMessage(): a new wave, the bar full again */
static PyObject *Berto_DeepThroatMessage(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, BM, "initBar", Sf_Int(&f, 1));
	Sf_DrawScorer(&f);
	Dt_Com(&f, "Mission_BertoMassacre_RemoteMessage2", "DeepThroat", NULL, NULL);
	return Sf_None(&f);
}

/* CheckBertoDeath(LooserName, WinnerName, deathPos): the first death a message, the found Berto drops the matrix; a
 * Berto the player killed counts on the bar */
static PyObject *Berto_CheckBertoDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *pos;
	SF_ARGS("OOO", &looser, &winner, &pos);
	int first = Sf_True(&f, G(BM, "firstDeath"));
	if (first > 0) {
		Sf_SetGlobal(&f, BM, "firstDeath", Sf_Int(&f, 0));
		Dt_Com(&f, "Mission_BertoMassacre_RemoteMessage1", "DeepThroat", NULL, NULL);
	} else if (first == 0) {
		int found = Sf_True(&f, G(BM, "bertoFound"));
		if (found > 0) {
			found = Sf_Compare(&f, looser, "==", G(BM, "bertoName"));
		}
		if (found > 0) {
			Sf_Own(&f, BM, "DropMatrix", "(O)", pos);
		}
	}
	if (Dt_UsrIs(&f, USR(), winner) > 0) {
		Sf_SetGlobal(&f, BM, "bertoDeath", Sf_Int(&f, 1));
		Sf_DrawScorer(&f);
	}
	return Sf_None(&f);
}

/* OnBertoFound(): no scorer, the order to destroy that Berto */
static PyObject *Berto_OnBertoFound(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Dt_ScorerCall(&f, Py_None);
	Sf_DrawScorer(&f);
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_DestroyBerto_MissionText1", 1);
	return Sf_None(&f);
}

/* DropMatrix(pos): no more waves, the matrix of D-Tritus where the Berto died */
static PyObject *Berto_DropMatrix(PyObject *self, PyObject *args) {
	PyObject *pos;
	SF_ARGS("O", &pos);
	Sf_SetMod(&f, "MissionsFuncs", "OnNewWaveCallback", Py_None);
	Sf_SetMod(&f, "MissionsFuncs", "OnBertoDeathCallback", Py_None);
	Dt_Drop(&f, BM, pos, "DTritusMatrix", "DTritusMatrixModel", "DTritusMatrixRadius", "Mission_BertoMassacre.TakeMatrix", "TakeMatrixExt",
		"MissionCheckArrow");
	return Sf_None(&f);
}

/* TakeMatrix(ItemName, CharName) */
static PyObject *Berto_TakeMatrix(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	SF_ARGS("OO", &item, &ch);
	Dt_Taken(&f, "DTritusMatrix", "MissionCheckArrow");
	Dt_Com(&f, "Mission_DestroyBerto_RemoteMessage1", "DeepThroat", G(BM, "SetNextMission"), NULL);
	return Sf_None(&f);
}

/* GetBerto(): the first Berto of the wave carries the matrix (tried again every second until there is one) */
static PyObject *Berto_GetBerto(PyObject *self, PyObject *args) {
	SF_ARGS("");
	int any = Sf_Compare(&f, Sf_Int(&f, Sf_Len(&f, Sf_Mod(&f, "MissionsFuncs", "currentBertos"))), ">", Sf_Int(&f, 0));
	if (any > 0) {
		Sf_SetGlobal(&f, BM, "bertoName", Sf_Item(&f, Sf_Mod(&f, "MissionsFuncs", "currentBertos"), 0));
		PyObject *e = Sf_Entity(&f, G(BM, "bertoName"));
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrI(&f, e, "Money", -1);
		}
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(BM, "bertoName"));
		Dt_Com(&f, "Mission_BertoMassacre_RemoteMessage3", "DeepThroat", G(BM, "OnBertoFound"), NULL);
	} else if (any == 0) {
		Sf_Later(&f, Sf_Int(&f, 1), G(BM, "GetBerto"), NOARGS, NULL);
	}
	return Sf_None(&f);
}

/* MissionScorer(id): the bar of the Bertos left before the one with the matrix */
static PyObject *Berto_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Dt_Text(&f, id, "MassacreText", Dt_Y(&f, 0), Dt_Label(&f, "Mission_BertoMassacre_Scorer1"), Sf_Int(&f, 1));
	const char *bar = "MassacreScorer";
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, bar, "Bar");
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, bar, "MaxValue", G(BM, "MaxBarValue"));
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, bar, "IsAutoUpdatable", 0);
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, bar, "Visible", 1);
	if (Sf_True(&f, G(BM, "initBar")) > 0) {
		Sf_SetGlobal(&f, BM, "initBar", Sf_Int(&f, 0));
		Sf_Run(&f, "SScorer", "Set", "(OssO)", id, bar, "Value", G(BM, "MaxBarValue"));
	}
	if (Sf_True(&f, G(BM, "bertoFound")) == 0 && Sf_True(&f, G(BM, "bertoDeath")) > 0) {
		Sf_SetGlobal(&f, BM, "bertoDeath", Sf_Int(&f, 0));
		PyObject *v = Sf_OpI(&f, Sf_Ask(&f, "SScorer", "Get", "(Oss)", id, bar, "Value"), '-', 1);
		if (Sf_Compare(&f, v, "<=", Sf_Int(&f, 0)) > 0) {
			v = Sf_Int(&f, 0);
			Sf_SetGlobal(&f, BM, "bertoFound", Sf_Int(&f, 1));
			Sf_SetMod(&f, "MissionsFuncs", "OnNewWaveCallback", Py_None);
			Dt_ScorerCall(&f, Py_None);
			Sf_DrawScorer(&f);
			Sf_Own(&f, BM, "GetBerto", "()");
		}
		Sf_Run(&f, "SScorer", "Set", "(OssO)", id, bar, "Value", v);
	}
	Lv_BarDefaults(&f, id, bar);
	return Sf_None(&f);
}

PyMethodDef g_DownTownBertoMassacreMethods[] = {
	{ "InitStatus", Berto_InitStatus, 1, NULL },
	{ "StartMission", Berto_StartMission, 1, NULL },
	{ "DeepThroatMessage", Berto_DeepThroatMessage, 1, NULL },
	{ "CheckBertoDeath", Berto_CheckBertoDeath, 1, NULL },
	{ "OnBertoFound", Berto_OnBertoFound, 1, NULL },
	{ "DropMatrix", Berto_DropMatrix, 1, NULL },
	{ "TakeMatrix", Berto_TakeMatrix, 1, NULL },
	{ "GetBerto", Berto_GetBerto, 1, NULL },
	{ "MissionScorer", Berto_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DefeatCrazyGambler1: the crazy gambler comes back until he has lost three lives ---- */

static const char *const gambler_profile[] = {
	"Ship=SBANKMASTER1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=2", "UsePercSonic=50", "UsePercCloud=25",
	"UsePercElectro=25", "UsePercHook=15", "FactorHookMap=10", "CadenceShoot=15", "ControlAim=10", "AngAttack=3.14", "CounterAttack=1",
	"ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.45", "FactorDamageItemLife=0.35",
	"FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=20", "TimeMaxStrafe=10", "VelMaxStrafe=1.0", NULL,
};

static PyObject *Gambler_GetCrazyGamblerProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, CG, gambler_profile);
}

static PyObject *Gambler_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, CG, "[\"SFunc1\",\"SBerto1\",\"SBetty1\"]");
}

/* Init(LevelPath): the gambler and his taunts, or the next mission when he has lost enough lives */
static PyObject *Gambler_Init(PyObject *self, PyObject *args) {
	PATH_ARGS();
	int done = Sf_Compare(&f, Sf_ToInt(&f, SAVE("NumCrazyGamblerLifes")), "==", G(CG, "CrazyGamblerLifesNextMission"));
	if (done > 0) {
		Sf_Own(&f, CG, "PreSetNextMission", "()");
	} else if (done == 0) {
		Sf_Run(&f, "SScorer", "AddModel", "(isssii)", 0, "EvilGambler", "Models/Heads/HeadEvilGambler/HeadEvilGambler", "", 4, 20);
		Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "Mission_DefeatCrazyGambler1.OnCrazyGamblerDeath");
		Sf_Own(&f, CG, "CreateCrazyGambler", "()");
		Sf_Later(&f, Sf_Int(&f, 3), G(CG, "LaunchTaunts"), NOARGS, NULL);
		Dt_ScorerCall(&f, G(CG, "MissionScorer"));
		Sf_DrawScorer(&f);
	}
	return Sf_None(&f);
}

/* LaunchTaunts(): a taunt of the five while the gambler lives, every 12 s */
static PyObject *Gambler_LaunchTaunts(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *e = Sf_Entity(&f, G(CG, "crazyGamblerName"));
	if (Sf_True(&f, e) > 0) {
		PyObject *key = Sf_Cat(&f, "sO", "Mission_DefeatCrazyGambler1_RemoteMessage", Sf_Repr(&f, Sf_Op(&f, Sf_Int(&f, 2), '+', G(CG, "numTaunt"))));
		Sf_Run(&f, "Speech", "RemoteComunication", "(Os)", key, "EvilGambler");
		Sf_SetGlobal(&f, CG, "numTaunt", Sf_OpI(&f, Sf_OpI(&f, G(CG, "numTaunt"), '+', 1), '%', 5));
	}
	Sf_Later(&f, Sf_Int(&f, 12), G(CG, "LaunchTaunts"), NOARGS, NULL);
	return Sf_None(&f);
}

/* CreateCrazyGambler(): the gambler at a free spawn point, the target of the mission */
static PyObject *Gambler_CreateCrazyGambler(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	PyObject *h = Lv_SpawnCar(&f, "CrazyGambler", 0);
	Lv_StdProfile(&f, CG, "GetCrazyGamblerProfile", h);
	Sf_SetAttrI(&f, h, "LaunchTaunts", 0);
	Sf_SetAttr(&f, h, "MPil0", G(CG, "pilotName"));
	Sf_SetAttrS(&f, h, "TgType", "red");
	Sf_SetAttrS(&f, h, "EntityList", "Enemies");
	PyObject *life = G(CG, "CRAZY_GAMBLER_LIFE");
	Sf_SetAttr(&f, h, "Life", life);
	Sf_SetAttr(&f, h, "MaxLife", life);
	Sf_SetGlobal(&f, CG, "crazyGamblerName", Sf_Attr(&f, h, "Name"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(CG, "crazyGamblerName"));
	return Sf_None(&f);
}

/* OnCrazyGamblerDeath(LooserName, WinnerName): a life fewer; he comes back or the mission ends */
static PyObject *Gambler_OnCrazyGamblerDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "SScorer", "CancelSpeech", "()");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	PyObject *lifes = Sf_OpI(&f, Sf_ToInt(&f, SAVE("NumCrazyGamblerLifes")), '-', 1);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumCrazyGamblerLifes", Sf_Repr(&f, lifes));
	int done = Sf_Compare(&f, lifes, "==", G(CG, "CrazyGamblerLifesNextMission"));
	if (done > 0) {
		Dt_ScorerCall(&f, Py_None);
		Sf_Later(&f, Sf_Int(&f, 7), G(CG, "PreSetNextMission"), NOARGS, NULL);
	} else if (done == 0) {
		Sf_Later(&f, Sf_Int(&f, 3), G(CG, "CreateCrazyGambler"), NOARGS, NULL);
	}
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* MissionScorer(id): the lives left */
static PyObject *Gambler_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Dt_Text(&f, id, "RemainingLifesText", Dt_Y(&f, 0), Dt_Label(&f, "Mission_DefeatCrazyGambler1_Scorer1"), NULL);
	Dt_Text(&f, id, "RemainingLifes", Dt_Y(&f, 20), SAVE("NumCrazyGamblerLifes"), NULL);
	return Sf_None(&f);
}

PyMethodDef g_DownTownDefeatCrazyGambler1Methods[] = {
	{ "GetCrazyGamblerProfile", Gambler_GetCrazyGamblerProfile, 1, NULL },
	{ "InitStatus", Gambler_InitStatus, 1, NULL },
	{ "Init", Gambler_Init, 1, NULL },
	{ "LaunchTaunts", Gambler_LaunchTaunts, 1, NULL },
	{ "CreateCrazyGambler", Gambler_CreateCrazyGambler, 1, NULL },
	{ "OnCrazyGamblerDeath", Gambler_OnCrazyGamblerDeath, 1, NULL },
	{ "MissionScorer", Gambler_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_FaceTheMayor: the crazy mayor in his ship, then DeepThroat at a phone cab ---- */

static const char *const mayor_profile[] = {
	"Ship=SMAYOR1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0", "GoNextItemRace=1", "ControlHookRace=0", "CadenceShootCM=3",
	"UsePercSonic=70", "UsePercCloud=20", "UsePercElectro=10", "UsePercHook=30", "FactorHookMap=6", "CadenceShoot=3", "ControlAim=8",
	"AngAttack=2.5", "CounterAttack=1", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.30",
	"FactorDamageItemLife=0.25", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=60", "TimeMaxStrafe=6",
	"VelMaxStrafe=0.75", NULL,
};

static PyObject *Mayor_GetMayorProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, FM, mayor_profile);
}

static PyObject *Mayor_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, FM, "[\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
}

/* Init(LevelPath): the mayor is not at the townhall; DeepThroat's warning and the mayor's taunt once the player is in
 * a ship, or the phone cab after the fight */
static PyObject *Mayor_Init(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Import(&f, "Speech");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MayorAtTownHall", "0");
	int begin = Sf_EqS(&f, SAVE("CrazyMayor"), "Begin");
	if (begin > 0) {
		Sf_Import(&f, "Parking");
		Dt_LaterCom(&f, Sf_Float(&f, 1.5), Sf_Build(&f, "(ss)", "Mission_FaceTheMayor_RemoteMessage1", "DeepThroat"), NULL);
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(FM, "PreLaunchTaunt"));
	} else if (begin == 0) {
		Sf_Own(&f, FM, "SetPhoneCab", "()");
	}
	return Sf_None(&f);
}

static PyObject *Mayor_PreLaunchTaunt(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Later(&f, Sf_Float(&f, 3.0), G(FM, "LaunchMayorInitialTaunt"), NOARGS, NULL);
	return Sf_None(&f);
}

/* LaunchMayorInitialTaunt(): the taunt when the player is in a ship, else a second later */
static PyObject *Mayor_LaunchMayorInitialTaunt(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	int car = Sf_True(&f, usr);
	if (car > 0) {
		car = Sf_True(&f, Sf_Method(&f, usr, "IsType", "(s)", "Car"));
	}
	if (car > 0) {
		Sf_Import(&f, "Parking");
		Dt_Com(&f, "Mission_FaceTheMayor_RemoteMessage2", "Mayor", G(FM, "CreateCrazyMayor"), NOARGS);
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
	} else if (car == 0) {
		Sf_Later(&f, Sf_Float(&f, 1.0), G(FM, "LaunchMayorInitialTaunt"), NOARGS, NULL);
	}
	return Sf_None(&f);
}

/* CreateCrazyMayor(): the mayor's ship instead of one of the traffic, an enemy and the target */
static PyObject *Mayor_CreateCrazyMayor(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "MissionsFuncs");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	Sf_Run(&f, "Scrap", "Set", "(sO)", "iTrafficNumber", Sf_OpI(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "iTrafficNumber"), '-', 1));
	Sf_SetGlobal(&f, FM, "SMayor1", Lv_SpawnCar(&f, "NormalTraffic", 0));
	PyObject *pd = Sf_Own(&f, FM, "GetMayorProfile", "()");
	Sf_Run(&f, "VehiclesNPC", "SetStdProperties", "(OO)", Sf_Attr(&f, G(FM, "SMayor1"), "Name"), Sf_Attr(&f, pd, "Ship"));
	Sf_Run(&f, "VehiclesNPC", "ModAIProff", "(OO)", G(FM, "SMayor1"), pd);
	Sf_SetAttrS(&f, G(FM, "SMayor1"), "WeapList", "2,2,2,2,2,0,1");
	Sf_SetAttrS(&f, G(FM, "SMayor1"), "EntityList", "Enemies");
	Sf_SetAttrS(&f, G(FM, "SMayor1"), "OnDeath", "Mission_FaceTheMayor.MayorOnDeath");
	PyObject *life = G(FM, "MAYOR_LIFE");
	Sf_SetAttr(&f, G(FM, "SMayor1"), "Life", life);
	Sf_SetAttr(&f, G(FM, "SMayor1"), "MaxLife", life);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(FM, "SMayor1"), "Name"));
	return Sf_None(&f);
}

/* MayorOnDeath(Looser, Winner): the alarm, the mayor killed, the phone cab */
static PyObject *Mayor_MayorOnDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "Init");
	Sf_SetGlobal(&f, FM, "SMayor1", Py_None);
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 1);
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "CrazyMayor", "Killed");
	Sf_Own(&f, FM, "SetPhoneCab", "()");
	return Sf_None(&f);
}

/* SetPhoneCab(): a free phone cab as the target; DeepThroat speaks once the alarm is over */
static PyObject *Mayor_SetPhoneCab(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Vehicles", "Speech", "Alarm", "SaveGame", "PhoneCab", "MissionsFuncs", NULL };
	SF_ARGS("");
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	PyObject *pc = Sf_Ask(&f, "PhoneCab", "GetFree", "()");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, pc, "Name"));
	Sf_SetAttrS(&f, pc, "UseFunc", "Mission_FaceTheMayor.UsePhoneCab");
	int alarm = Dt_Alarm(&f);
	if (alarm > 0) {
		Dt_Com(&f, "Mission_FaceTheMayor_RemoteMessage3", "DeepThroat", G(FM, "FleeFromPolice"), NULL);
		Sf_Method(&f, Sf_Mod(&f, "Alarm", "SetAlarmCallback"), "append", "(O)", G(FM, "SetAlarmCallback"));
	} else if (alarm == 0) {
		Sf_Own(&f, FM, "SetAlarmCallback", "(i)", 1);
	}
	return Sf_Return(&f, pc);
}

/* UsePhoneCab(UsrName, CharName, optId, FullOption): DeepThroat at the phone, the end of the mission after the talk */
static PyObject *Mayor_UsePhoneCab(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_SetMod(&f, "Speech", "NPCName", ch);
	Sf_Run(&f, "PhoneCab", "UseAudioCom", "(OOOO)", usr, ch, opt, full);
	PyObject *pc = Sf_Entity(&f, ch);
	int alarm = Dt_Alarm(&f);
	if (alarm > 0) {
		Sf_Run(&f, "Speech", "StartObjSpch", "(Oss)", pc, "Mission_TalkToDeepThroat1_RemoteMessage2", "DeepThroat");
	} else if (alarm == 0) {
		Sf_Run(&f, "Speech", "OnObjSpch", "(Osss)", pc, "DeepThroat", "Mission_TalkToDeepThroat1_ComDeepThroat_NI_1Msg",
			"Mission_TalkToDeepThroat1_ComDeepThroat_NI_1Ans");
		Sf_SpeechEnd(&f, G(FM, "EndOfMission"), Sf_Build(&f, "(O)", pc));
	}
	return Sf_None(&f);
}

/* EndOfMission(pc): on to Mission_ApologizeMayor, the places of the characters */
static PyObject *Mayor_EndOfMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "MayorAtTownHall", "1", "MayorAtGambinDen", "0", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Press", "Journalist.Betty_Defaut", "TownHall", "Journalist.Berto_Defaut", "TownHall", NULL };
	PyObject *pc;
	SF_ARGS("O", &pc);
	Sf_SetAttrS(&f, pc, "UseFunc", "PhoneCab.DefUseAudioCom");
	Sf_SetAttrS(&f, pc, "HeadModel", "");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_ApologizeMayor", "TownHall");
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", Sf_Mod(&f, "Speech", "ObjEntity"), 0);
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_ApologizeMayor_MissionText1", 1);
	Lv_Saves(&f, places);
	return Sf_None(&f);
}

PyMethodDef g_DownTownFaceTheMayorMethods[] = {
	{ "GetMayorProfile", Mayor_GetMayorProfile, 1, NULL },
	{ "InitStatus", Mayor_InitStatus, 1, NULL },
	{ "Init", Mayor_Init, 1, NULL },
	{ "PreLaunchTaunt", Mayor_PreLaunchTaunt, 1, NULL },
	{ "LaunchMayorInitialTaunt", Mayor_LaunchMayorInitialTaunt, 1, NULL },
	{ "CreateCrazyMayor", Mayor_CreateCrazyMayor, 1, NULL },
	{ "MayorOnDeath", Mayor_MayorOnDeath, 1, NULL },
	{ "SetPhoneCab", Mayor_SetPhoneCab, 1, NULL },
	{ "UsePhoneCab", Mayor_UsePhoneCab, 1, NULL },
	{ "EndOfMission", Mayor_EndOfMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_Massacre: points for every ship the player destroys before the timer runs out ---- */

static PyObject *Massacre_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, MS, "[\"SMayor1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
}

/* Init(LevelPath): the callback of the deaths, the timer between two of them, the scorer, Igor and Rusty */
static PyObject *Massacre_Init(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_SetMod(&f, "Vehicles", "OnDeathCallback", G(MS, "OnVehicleCallback"));
	Sf_Run(&f, "Scrap", "Set", "(si)", "CounterType", 2);
	Sf_Run(&f, "Scrap", "Set", "(sO)", "TimeSpecialUseTimer", G(MS, "betweenDeathsTime"));
	Dt_ScorerCall(&f, G(MS, "MissionScorer"));
	Sf_DrawScorer(&f);
	Dt_Com(&f, "Mission_Massacre_RemoteMessage4", "Igor", NULL, NULL);
	Sf_Run(&f, "SScorer", "AddModel", "(isssii)", 0, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3, 25);
	return Sf_None(&f);
}

/* `a <= lo and b > lo` of the percentages */
static int Ms_Crossed(scriptFrame_t *f, PyObject *prev, PyObject *now, double lo) {
	int t = Sf_Compare(f, prev, "<=", Sf_Float(f, lo));
	return t > 0 ? Sf_Compare(f, now, ">", Sf_Float(f, lo)) : t;
}

/* OnVehicleCallback(LooserName, WinnerName): the points of an AI ship the player destroyed (its weapon slots and its
 * kind), Igor at each quarter, the end at 100 % */
static PyObject *Massacre_OnVehicleCallback(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) == 0) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "!=", winner) > 0) {
		return Sf_None(&f);
	}
	PyObject *v = Sf_Entity(&f, looser);
	if (Sf_True(&f, Sf_Attr(&f, v, "isAI")) == 0) {
		return Sf_None(&f);
	}
	PyObject *model = Sf_Ask(&f, "string", "upper", "(O)", Sf_Attr(&f, v, "Model"));
	PyObject *slots = Sf_Attr(&f, v, "nSlot00");
	long points = 0;
	int traf = Sf_True(&f, Sf_Attr(&f, v, "Traf_Active"));
	if (traf > 0) {
		PyObject *typo = Sf_Attr(&f, v, "Traf_Type");
		int heavy = Sf_EqS(&f, typo, "Heavy");
		int medium = heavy == 0 ? Sf_EqS(&f, typo, "Medium") : 0;
		points = heavy > 0 ? 3 : medium > 0 ? 2 : 1;
	} else if (traf == 0) {
		Sf_True(&f, Sf_Ask(&f, "string", "find", "(Os)", model, "POLI"));
		points = 5;
	}
	PyObject *ac = Sf_Eval(&f, SAVE("Mission_Massacre.Points"), MS);
	PyObject *prev = Sf_Op(&f, Sf_ToFloat(&f, ac), '/', Sf_ToFloat(&f, G(MS, "MAX_POINTS")));
	PyObject *total = Sf_Op(&f, Sf_Op(&f, slots, '+', Sf_Int(&f, points)), '+', ac);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Mission_Massacre.Points", Sf_Repr(&f, total));
	Sf_Run(&f, "SScorer", "Set", "(issO)", 0, "MassacreScorer", "Value", total);
	PyObject *now = Sf_Op(&f, Sf_ToFloat(&f, total), '/', Sf_ToFloat(&f, G(MS, "MAX_POINTS")));
	Sf_Print(&f, "OsOs", prev, "<= 0.25 and", now, "> 0.25");
	int c = Ms_Crossed(&f, prev, now, 0.25);
	if (c > 0) {
		Dt_Com(&f, "Mission_Massacre_RemoteMessage1", "Igor", NULL, NULL);
	} else if (c == 0 && (c = Ms_Crossed(&f, prev, now, 0.5)) > 0) {
		Dt_Com(&f, "Mission_Massacre_RemoteMessage1", "Igor", NULL, NULL);
	} else if (c == 0 && (c = Ms_Crossed(&f, prev, now, 0.75)) > 0) {
		Dt_Com(&f, "Mission_Massacre_RemoteMessage2", "Igor", NULL, NULL);
	} else if (c == 0 && Sf_Compare(&f, now, ">=", Sf_Float(&f, 1.0)) > 0) {
		Sf_SetMod(&f, "Vehicles", "OnDeathCallback", Py_None);
		Dt_ScorerCall(&f, Py_None);
		Sf_DrawScorer(&f);
		Dt_Com(&f, "Mission_Massacre_RemoteMessage3", "Igor", G(MS, "SetNextMission"), NOARGS);
	}
	Sf_Run(&f, "Scrap", "Set", "(sO)", "TimeSpecialUseTimer", Sf_OpI(&f, G(MS, "betweenDeathsTime"), '+', 1));
	Sf_Run(&f, "Scrap", "Set", "(si)", "CounterType", 0);
	return Sf_None(&f);
}

/* MissionScorer(id): the bar of the points and the timer between two deaths */
static PyObject *Massacre_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Dt_Text(&f, id, "MassacreText", Dt_Y(&f, 0), Dt_Label(&f, "Mission_Massacre_Scorer_1"), Sf_Int(&f, 1));
	const char *bar = "MassacreScorer";
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, bar, "Bar");
	Lv_BarDefaults(&f, id, bar);
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, bar, "MaxValue", G(MS, "MAX_POINTS"));
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, bar, "Value", Sf_Eval(&f, SAVE("Mission_Massacre.Points"), MS));
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, bar, "Visible", 1);
	Dt_Timer(&f, id, "BetweenDeathsTimer", Dt_X(&f, 40), Dt_Y(&f, 60), "Mission_Massacre.TimeExpired", 1);
	return Sf_None(&f);
}

/* TimeExpired(): the points back to 0 */
static PyObject *Massacre_TimeExpired(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Dt_Com(&f, "Mission_Massacre_RemoteMessage5", "Igor", NULL, NULL);
	Sf_Run(&f, "Scrap", "Set", "(sO)", "TimeSpecialUseTimer", G(MS, "betweenDeathsTime"));
	Sf_Run(&f, "Scrap", "Set", "(si)", "CounterType", 2);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Mission_Massacre.Points", Sf_ReprI(&f, 0));
	Sf_Run(&f, "SScorer", "Set", "(issi)", 0, "MassacreScorer", "Value", 0);
	return Sf_None(&f);
}

/* bp = eval(Scrap.GetSaveVar(var)); bp = bp + [a, b]; Scrap.SetSaveVar(var, `bp`) */
static byte Dt_AddBlueprints(scriptFrame_t *f, const char *module, const char *var, const char *a, const char *b) {
	int mark = f->n;
	PyObject *bp = Sf_Op(f, Lv_SaveList(f, var, module), '+', Sf_Build(f, "[ss]", a, b));
	Sf_Run(f, "Scrap", "SetSaveVar", "(sO)", var, Sf_Repr(f, bp));
	Sf_Release(f, mark);
	return !f->failed;
}

/* SetNextMission(): the next mission of NextMissionData, the blueprints of Berto's and Betty's ships, Rusty later */
static PyObject *Massacre_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "Mission_Massacre");
	PyObject *apply = Sf_Mod(&f, "__builtin__", "apply");
	Sf_Call(&f, apply, "(OO)", Sf_Mod(&f, "MissionsFuncs", "SetNextMission"), G(MS, "NextMissionData"));
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_ArrangeIgorCombat_MissionText1", 1);
	Dt_AddBlueprints(&f, MS, "PressBlueprints.Ships", "SBerto1", "SBetty1");
	Dt_AddBlueprints(&f, MS, "PressBlueprints.Engines", "MBerto1", "MBetty1");
	Sf_Import(&f, "Scorer");
	Dt_LaterCom(&f, Sf_Float(&f, 10.0),
		Sf_Build(&f, "(ssO(ii))", "Mission_ArrangeIgorCombat_RemoteMessage1", "Rusty", Sf_Mod(&f, "Scorer", "ShowMissionScorer"), 0, 2), NULL);
	return Sf_None(&f);
}

PyMethodDef g_DownTownMassacreMethods[] = {
	{ "InitStatus", Massacre_InitStatus, 1, NULL },
	{ "Init", Massacre_Init, 1, NULL },
	{ "OnVehicleCallback", Massacre_OnVehicleCallback, 1, NULL },
	{ "MissionScorer", Massacre_MissionScorer, 1, NULL },
	{ "TimeExpired", Massacre_TimeExpired, 1, NULL },
	{ "SetNextMission", Massacre_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DestroyCommSats3: the communication probes of the level ---- */

/* InitStatus(LevelPath): the probes are the heavy traffic, none of it at the start */
static PyObject *Probes_InitStatus(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "Traffic");
	Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[s]", "TProbe"));
	Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 0));
	return Sf_None(&f);
}

static PyObject *Probes_Init(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Own(&f, CS, "CreateProbes", "()");
	Dt_ScorerCall(&f, G(CS, "MissionScorer"));
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* IsValidPoint(usrPos, pos, posList): a new point far enough from the player and from the other probes */
static PyObject *Probes_IsValidPoint(PyObject *self, PyObject *args) {
	PyObject *usrPos, *pos, *list;
	SF_ARGS("OOO", &usrPos, &pos, &list);
	int in = Sf_In(&f, pos, list);
	if (in != 0) {
		return Sf_Return(&f, in > 0 ? Sf_Int(&f, 0) : NULL);
	}
	PyObject *d = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", pos, usrPos));
	int near = Sf_Compare(&f, d, "<", G(CS, "minPlayerDist"));
	if (near != 0) {
		return Sf_Return(&f, near > 0 ? Sf_Int(&f, 0) : NULL);
	}
	PyObject *p;
	for (int k = 0; (p = Sf_Next(&f, list, k)) != NULL; k++) {
		d = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", p, pos));
		if (Sf_Compare(&f, d, "<", G(CS, "minDistBetweenProbes")) > 0) {
			return Sf_Return(&f, Sf_Int(&f, 0));
		}
	}
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* CreateProbes(): the probes left at random visible points */
static PyObject *Probes_CreateProbes(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "Traffic");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *list = Sf_Build(&f, "[]");
	PyObject *usrPos = Sf_Attr(&f, usr, "Pos");
	PyObject *num = Sf_ToInt(&f, SAVE("NumProbes"));
	long i = 0;
	for (;;) {
		int mark = f.n;
		if (Sf_Compare(&f, Sf_Int(&f, i), "<", num) <= 0) {
			break;
		}
		PyObject *pos = Sf_Ask(&f, "SAI", "GetRandomVisibilityPoint", "()");
		int ok = Sf_True(&f, Sf_Own(&f, CS, "IsValidPoint", "(OOO)", usrPos, pos, list));
		if (ok == 0) {
			Sf_Release(&f, mark);
			continue;
		}
		Sf_Run(&f, "Traffic", "SetPath", "()");
		PyObject *p = Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ssii)", "TProbe", "ProbeMos", 0, 0);
		Sf_Run(&f, "Traffic", "ResetPath", "()");
		Sf_SetAttrS(&f, p, "OnDeath", "Mission_DestroyCommSats3.OnProbeDeath");
		Sf_SetAttr(&f, p, "MovePos", pos);
		Sf_SetAttr(&f, p, "Pos", pos);
		PyObject *ang = Sf_Build(&f, "(iii)", 0, 0, 0);
		Sf_SetAttr(&f, p, "MoveAng", ang);
		Sf_SetAttr(&f, p, "Ang", ang);
		Sf_SetAttr(&f, p, "MoveTime", Sf_Float(&f, 0.1));
		PyObject *life = G(CS, "PROBE_LIFE");
		Sf_SetAttr(&f, p, "Life", life);
		Sf_SetAttr(&f, p, "MaxLife", life);
		Sf_SetAttr(&f, p, "MinLife", life);
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", p, 1);
		Sf_Method(&f, list, "append", "(O)", pos);
		Sf_Release(&f, mark);
		i++;
	}
	return Sf_None(&f);
}

/* OnProbeDeath(LooserName, WinnerName): one probe fewer; the last one ends the mission */
static PyObject *Probes_OnProbeDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	PyObject *n = Sf_OpI(&f, Sf_ToInt(&f, SAVE("NumProbes")), '-', 1);
	int last = Sf_EqI(&f, n, 0);
	if (last > 0) {
		Sf_Own(&f, CS, "DestroyEnemyShips", "()");
		Dt_ScorerCall(&f, Py_None);
		Dt_LaterCom(&f, Sf_Int(&f, 2), Sf_Build(&f, "(ssOO)", "Mission_DestroyCommSats3_RemoteMessage1", "CrazyGambler", G(CS, "SetNextMission"), NOARGS),
			NULL);
	} else if (last == 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumProbes", Sf_Repr(&f, n));
		Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	}
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* DestroyShipsInList(listName): every ship of the list explodes */
static PyObject *Probes_DestroyShipsInList(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	PyObject *e = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "EntityListGet", "(Os)", name, "First"));
	int mark = f.n;
	while (Sf_True(&f, e) > 0) {
		Sf_SetAttrS(&f, e, "EntityList", "");
		Sf_SetAttrI(&f, e, "Life", 0);
		PyObject *next = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "EntityListGet", "(Os)", name, "First"));
		Sf_ReleaseKeep(&f, mark, &next, 1);
		e = next;
	}
	return Sf_None(&f);
}

/* DestroyEnemyShips(): no more police nor traffic, the alarm falls, the ships of the lists explode one after another */
static PyObject *Probes_DestroyEnemyShips(PyObject *self, PyObject *args) {
	static const char *const lists[] = { "EnemiesTraffic", "ChasingTraffic", "Traffic", "OutCop" };
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "Set", "(si)", "MaxChasingShips", 0);
	Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 0);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "PoliceWaveEliminated", "");
	Sf_Run(&f, "Scrap", "Set", "(sd)", "AlarmFallDelta", -0.05);
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	for (int i = 0; i < 4; i++) {
		Sf_Later(&f, Sf_Float(&f, 0.5 * i), G(CS, "DestroyShipsInList"), Sf_Build(&f, "(s)", lists[i]), NULL);
	}
	return Sf_None(&f);
}

/* SetNextMission(): the level counts as freed, the mercenaries leave D-Tritus, on to Mission_GoWithHuman */
static PyObject *Probes_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const mercs[] = { "MercsHelpDtritus", "0", "MercFriends.MercenaryA_Smartie", "0", "MercFriends.MercenaryC_Brutus", "0",
		"MercFriends.MercenaryB_Dumber", "0", NULL };
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "NumProbes");
	PyObject *l = Lv_SaveList(&f, "ComSatsMissionsMapsFinished", CS);
	Sf_Method(&f, l, "append", "(s)", "DOWNTOWN");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "ComSatsMissionsMapsFinished", Sf_Repr(&f, l));
	Lv_Saves(&f, mercs);
	Dt_Next(&f, "Mission_GoWithHuman", "Press", "Mission_GoWithHuman_MissionText1");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the probes left */
static PyObject *Probes_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Dt_Text(&f, id, "NumProbesText", Dt_Y(&f, 0), Dt_Label(&f, "Mission_DestroyCommSats3_Scorer1"), NULL);
	Dt_Text(&f, id, "NumProbes", Dt_Y(&f, 20), SAVE("NumProbes"), NULL);
	return Sf_None(&f);
}

PyMethodDef g_DownTownDestroyCommSats3Methods[] = {
	{ "InitStatus", Probes_InitStatus, 1, NULL },
	{ "Init", Probes_Init, 1, NULL },
	{ "IsValidPoint", Probes_IsValidPoint, 1, NULL },
	{ "CreateProbes", Probes_CreateProbes, 1, NULL },
	{ "OnProbeDeath", Probes_OnProbeDeath, 1, NULL },
	{ "DestroyShipsInList", Probes_DestroyShipsInList, 1, NULL },
	{ "DestroyEnemyShips", Probes_DestroyEnemyShips, 1, NULL },
	{ "SetNextMission", Probes_SetNextMission, 1, NULL },
	{ "MissionScorer", Probes_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GetDataPart1: the transports drop the data cubes when the player hits them ---- */

static const char *const func_profile[] = {
	"Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=4", "UsePercSonic=80", "UsePercCloud=10", "UsePercElectro=10",
	"UsePercHook=0", "FactorHookMap=10", "CadenceShoot=1", "ControlAim=3", "AngAttack=2.5", "CounterAttack=0.8", "ControlBoost=1",
	"ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.60", "FactorDamageItemLife=0.15", "FactorItemAmmo=0.1",
	"ControlStrafeEvasion=1", "UsePercHookGetAway=5", "TimeMaxStrafe=8", "VelMaxStrafe=1.0", NULL,
};

static PyObject *Data_GetFuncEnemiesProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, GD, func_profile);
}

static PyObject *Data_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, GD, "[\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
}

/* Init(LevelPath): the list of the transports and its callbacks, the first transport, the mission while the player is
 * in a ship, the boss */
static PyObject *Data_Init(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "Scrap", "Preload3DObject", "(O)", G(GD, "DataCubeModel"));
	Sf_Run(&f, "Scrap", "CreateEntityList", "(s)", "Transports");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Transports", "OnDeath", "Mission_GetDataPart1.OnTransportsDeath");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Transports", "OnDamage", "Mission_GetDataPart1.OnTransportsDamage");
	if (Sf_Compare(&f, Sf_ToInt(&f, SAVE("NumData")), ">=", G(GD, "MAX_DATA_UNITS")) > 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumData", Sf_Repr(&f, Sf_OpI(&f, G(GD, "MAX_DATA_UNITS"), '-', 1)));
	}
	Sf_Own(&f, GD, "AddTransportToList", "()");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(GD, "InitMission"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", G(GD, "StopMission"));
	int first = Sf_EqS(&f, SAVE("MissionState"), "FirstTime");
	if (first > 0) {
		Dt_Com(&f, "MissionGetDataPart1_RemoteMessage1", "Boss", NULL, NULL);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "GetData");
	} else if (first == 0) {
		Dt_Com(&f, "MissionGetDataPart1_RemoteMessage5", "Boss", NULL, NULL);
	}
	return Sf_None(&f);
}

/* `nd and int(nd) op global` of the data units taken */
static int Gd_DataCmp(scriptFrame_t *f, PyObject *nd, const char *op, const char *name) {
	int t = Sf_True(f, nd);
	return t > 0 ? Sf_Compare(f, Sf_ToInt(f, nd), op, Sf_Global(f, GD, name)) : t;
}

/* `usr and usr.IsType("Car")` */
static int Dt_UsrInCar(scriptFrame_t *f, PyObject *usr) {
	int t = Sf_True(f, usr);
	return t > 0 ? Sf_True(f, Sf_Method(f, usr, "IsType", "(s)", "Car")) : t;
}

/* InitMission(): the scorer; past half of the data the functionaries come for the transport */
static PyObject *Data_InitMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Dt_ScorerCall(&f, G(GD, "MissionScorer"));
	Sf_DrawScorer(&f);
	PyObject *nd = SAVE("NumData");
	int go = Sf_True(&f, G(GD, "funcsSpawned"));
	go = go == 0 ? Gd_DataCmp(&f, nd, ">", "NumDataFuncsAttackTransport") : go > 0 ? 0 : -1;
	if (go > 0) {
		Sf_SetGlobal(&f, GD, "funcsSpawned", Sf_Int(&f, 1));
		Dt_Com(&f, "MissionGetDataPart1_RemoteMessage8", "Functionary", NULL, NULL);
		Sf_Own(&f, GD, "SpawnFunctionaries", "(i)", 2);
		PyObject *e = Sf_Entity(&f, G(GD, "currentTransport"));
		PyObject *usr = USR();
		int t = Sf_True(&f, e);
		if (t > 0) {
			t = Dt_UsrInCar(&f, usr);
		}
		if (t > 0) {
			Sf_SetAttr(&f, usr, "Target", G(GD, "currentTransport"));
		}
	}
	return Sf_None(&f);
}

/* CreateTransport(): a transport of the light traffic at a free spawn point, the player's target */
static PyObject *Data_CreateTransport(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "Traffic");
	Sf_Run(&f, "Traffic", "SetPath", "()");
	PyObject *spw = Sf_Ask(&f, "OutMap", "GetFreeSpawnPos", "()");
	PyObject *fn = Sf_Mod(&f, "Scrap", "CreateEntity");
	PyObject *p = Sf_Item(&f, spw, 0);
	PyObject *o1 = Sf_Call(&f, fn, "(sOOOs)", "Transport", Sf_Item(&f, p, 0), Sf_Item(&f, p, 1), Sf_Item(&f, p, 2), "Car");
	Sf_SetAttrS(&f, o1, "TgType", "red");
	Sf_SetAttr(&f, o1, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, spw, 1), 0));
	PyObject *pd = Sf_Ask(&f, "VehiclesNPC", "GetIAProf", "(s)", "TLight2");
	Sf_Run(&f, "VehiclesNPC", "SetStdProperties", "(OO)", Sf_Attr(&f, o1, "Name"), Sf_Attr(&f, pd, "Ship"));
	Sf_Run(&f, "VehiclesNPC", "SetAIProfile", "(Os)", pd, "Pilot");
	Sf_Run(&f, "VehiclesNPC", "ModAIProff", "(OO)", o1, pd);
	Sf_Run(&f, "SAI", "SetInertia", "(Oi)", Sf_Attr(&f, o1, "Name"), 0);
	PyObject *life = G(GD, "TRANSPORT_LIFE");
	Sf_SetAttr(&f, o1, "Life", life);
	Sf_SetAttr(&f, o1, "MaxLife", life);
	Sf_Run(&f, "Traffic", "ResetPath", "()");
	PyObject *usr = USR();
	if (Dt_UsrInCar(&f, usr) > 0) {
		Sf_SetAttr(&f, usr, "Target", Sf_Attr(&f, o1, "Name"));
	}
	return Sf_Return(&f, o1);
}

/* CreateFunctionary(): a functionary fighter at a free spawn point */
static PyObject *Data_CreateFunctionary(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "Traffic");
	PyObject *spw = Sf_Ask(&f, "OutMap", "GetFreeSpawnPos", "()");
	PyObject *fn = Sf_Mod(&f, "Scrap", "CreateEntity");
	PyObject *p = Sf_Item(&f, spw, 0);
	PyObject *o1 = Sf_Call(&f, fn, "(sOOOs)", "FuncFighter", Sf_Item(&f, p, 0), Sf_Item(&f, p, 1), Sf_Item(&f, p, 2), "Car");
	Sf_SetAttrS(&f, o1, "TgType", "red");
	Sf_SetAttr(&f, o1, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, spw, 1), 0));
	Lv_StdProfile(&f, GD, "GetFuncEnemiesProfile", o1);
	PyObject *life = G(GD, "FUNCS_LIFE");
	Sf_SetAttr(&f, o1, "Life", life);
	Sf_SetAttr(&f, o1, "MaxLife", life);
	return Sf_Return(&f, o1);
}

/* AddTransportToList(): a new transport (the farthest one leaves a full list), the target of the mission */
static PyObject *Data_AddTransportToList(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) == 0) {
		return Sf_None(&f);
	}
	PyObject *usrPos = Sf_Attr(&f, usr, "Pos");
	PyObject *transports = G(GD, "transports");
	if (Sf_Compare(&f, Sf_Int(&f, Sf_Len(&f, transports)), "==", G(GD, "MAX_TRANSPORTS")) > 0) {
		PyObject *dist = Sf_Int(&f, 0), *remove = Sf_Str(&f, ""), *name;
		int mark = f.n;
		for (int k = 0; (name = Sf_Next(&f, transports, k)) != NULL; k++) {
			PyObject *t = Sf_Entity(&f, name);
			if (Sf_True(&f, t) > 0) {
				PyObject *d = Sf_Ask(&f, "SVec", "ModSqr", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", usrPos, Sf_Attr(&f, t, "Pos")));
				if (Sf_Compare(&f, d, ">", dist) > 0) {
					dist = d;
					remove = name;
				}
			}
			PyObject *keep[2] = { dist, remove };
			Sf_ReleaseKeep(&f, mark, keep, 2);
		}
		if (Sf_In(&f, remove, transports) > 0) {
			Sf_Method(&f, transports, "remove", "(O)", remove);
		}
	}
	PyObject *trans = Sf_Own(&f, GD, "CreateTransport", "()");
	Sf_SetGlobal(&f, GD, "currentTransport", Sf_Attr(&f, trans, "Name"));
	Sf_SetGlobal(&f, GD, "currentTransportSpawnedData", Sf_Int(&f, 0));
	Sf_Run(&f, "SAI", "SetStateVehicle", "(iOidd)", 3, G(GD, "currentTransport"), 0, 3.0, 2.0);
	Sf_SetAttrS(&f, trans, "EntityList", "Transports");
	Sf_Method(&f, G(GD, "transports"), "append", "(O)", G(GD, "currentTransport"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(GD, "currentTransport"));
	return Sf_None(&f);
}

/* SpawnFunctionaries(numFuncs): functionaries as enemies, or with only the rail after the transport once the player
 * has more than half of the data (later when the transport is not there) */
static PyObject *Data_SpawnFunctionaries(PyObject *self, PyObject *args) {
	PyObject *num;
	SF_ARGS("O", &num);
	Sf_Import(&f, "VehiclesNPC");
	int attack = 0;
	PyObject *nd = SAVE("NumData");
	if (Sf_True(&f, nd) == 0) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, Sf_ToInt(&f, nd), ">", G(GD, "NumDataFuncsAttackTransport")) > 0) {
		attack = 1;
	}
	PyObject *e = Sf_Entity(&f, G(GD, "currentTransport"));
	int noE = Sf_True(&f, e) == 0;
	if (f.failed) {
		return Sf_None(&f);
	}
	if (attack && noE) {
		Sf_Later(&f, Sf_Int(&f, 3), G(GD, "SpawnFunctionaries"), Sf_Build(&f, "(i)", 1), NULL);
		return Sf_None(&f);
	}
	long n = 0;
	Sf_Check(&f, Script_Range(num, &n));
	for (long i = 0; i < n && !f.failed; i++) {
		int mark = f.n;
		PyObject *fn = Sf_Own(&f, GD, "CreateFunctionary", "()");
		Sf_SetAttrS(&f, fn, "OnDeath", "Mission_GetDataPart1.OnFunctionaryDeath");
		if (!attack || noE) {
			Sf_SetAttrS(&f, fn, "EntityList", "Enemies");
		} else {
			Sf_SetAttrS(&f, fn, "WeapList", "0,0,0,0,1,0,1");
			Sf_Method(&f, G(GD, "funcsAttackingTransport"), "append", "(O)", Sf_Attr(&f, fn, "Name"));
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Attr(&f, fn, "Name"), G(GD, "currentTransport"));
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* OnFunctionaryDeath(LooserName, WinnerName): another one 10 s later */
static PyObject *Data_OnFunctionaryDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	if (Sf_In(&f, looser, G(GD, "funcsAttackingTransport")) > 0) {
		Sf_Method(&f, G(GD, "funcsAttackingTransport"), "remove", "(O)", looser);
	}
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Later(&f, Sf_Int(&f, 10), G(GD, "SpawnFunctionaries"), Sf_Build(&f, "(i)", 1), NULL);
	return Sf_None(&f);
}

/* OnTransportsDeath(LooserName, WinnerName): the transport explodes, its functionaries become enemies; a transport
 * that still had data gives a new one */
static PyObject *Data_OnTransportsDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *name;
	SF_ARGS("OO", &looser, &winner);
	Sf_SetAttrI(&f, Sf_Entity(&f, looser), "Ammo01", 500);
	Sf_Run(&f, "Scrap", "SetCallFunc", "(s)", "!InfernoFire");
	Sf_Run(&f, "Scrap", "AddParams", "(O)", looser);
	Sf_Run(&f, "Scrap", "Execute", "()");
	PyObject *funcs = G(GD, "funcsAttackingTransport");
	int mark = f.n;
	for (int k = 0; (name = Sf_Next(&f, funcs, k)) != NULL; k++) {
		PyObject *e = Sf_Entity(&f, name);
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrS(&f, e, "EntityList", "Enemies");
		}
		Sf_Release(&f, mark);
	}
	Sf_SetGlobal(&f, GD, "funcsAttackingTransport", Sf_Build(&f, "[]"));
	if (Sf_In(&f, looser, G(GD, "transportsTaunts")) > 0) {
		Sf_Method(&f, G(GD, "transportsTaunts"), "remove", "(O)", looser);
	}
	if (Sf_In(&f, looser, G(GD, "transports")) > 0) {
		PyObject *e = Sf_Entity(&f, looser);
		int data = Sf_True(&f, e);
		if (data > 0) {
			data = Sf_EqI(&f, Sf_Attr(&f, e, "RadarShow"), 1);
		}
		if (data > 0) {
			Sf_SetGlobal(&f, GD, "droppedData", Sf_Int(&f, 0));
			Dt_Com(&f, "MissionGetDataPart1_RemoteMessage3", "Boss", NULL, NULL);
			Sf_Later(&f, Sf_Int(&f, 2), G(GD, "AddTransportToList"), NOARGS, "AddTransport");
		}
	}
	return Sf_None(&f);
}

/* OnTransportsDamage(LooserName, WinnerName, Damage): the player hits the transport with data: the functionaries, a
 * taunt, the data cubes start to fall */
static PyObject *Data_OnTransportsDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) == 0) {
		return Sf_None(&f);
	}
	PyObject *e = Sf_Entity(&f, looser);
	int off = Sf_True(&f, e);
	if (off > 0) {
		off = Sf_Compare(&f, Sf_Attr(&f, e, "RadarShow"), "!=", Sf_Int(&f, 1));
	}
	if (off > 0 || Sf_In(&f, winner, G(GD, "funcsAttackingTransport")) > 0) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", winner) > 0) {
		if (Sf_True(&f, G(GD, "funcsSpawned")) == 0) {
			Sf_SetGlobal(&f, GD, "funcsSpawned", Sf_Int(&f, 1));
			Sf_Own(&f, GD, "SpawnFunctionaries", "(i)", 2);
		}
		PyObject *nd = SAVE("NumData");
		if (Gd_DataCmp(&f, nd, "<=", "NumDataFuncsAttackTransport") > 0 && Sf_In(&f, looser, G(GD, "transportsTaunts")) == 0) {
			Dt_Com(&f, "MissionGetDataPart1_RemoteMessage7", "Functionary", NULL, NULL);
			Sf_Method(&f, G(GD, "transportsTaunts"), "append", "(O)", looser);
		}
		if (Sf_EqI(&f, G(GD, "droppedData"), 0) > 0) {
			Sf_Later(&f, G(GD, "TimeBetweenData"), G(GD, "SpawnData"), NOARGS, NULL);
			Sf_SetGlobal(&f, GD, "droppedData", Sf_Int(&f, 1));
		}
	}
	return Sf_None(&f);
}

/* SpawnData(): the current transport drops a data cube, four in a row */
static PyObject *Data_SpawnData(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *e = Sf_Entity(&f, G(GD, "currentTransport"));
	if (Sf_True(&f, e) > 0 && Sf_Compare(&f, Sf_ToInt(&f, SAVE("NumData")), "<", G(GD, "MAX_DATA_UNITS")) > 0) {
		Sf_Own(&f, GD, "DropData", "(O)", Sf_Attr(&f, e, "Pos"));
		int more = Sf_Compare(&f, G(GD, "droppedData"), "!=", Sf_Int(&f, 3));
		if (more > 0) {
			Sf_Later(&f, G(GD, "TimeBetweenData"), G(GD, "SpawnData"), NOARGS, "SpawnData");
			Sf_SetGlobal(&f, GD, "droppedData", Sf_OpI(&f, G(GD, "droppedData"), '+', 1));
		} else if (more == 0) {
			Sf_SetGlobal(&f, GD, "droppedData", Sf_Int(&f, 0));
		}
	}
	return Sf_None(&f);
}

/* DropData(pos): a data cube that goes away after TimeDataDisappears */
static PyObject *Data_DropData(PyObject *self, PyObject *args) {
	PyObject *pos;
	SF_ARGS("O", &pos);
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "DroppedData", Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "Item");
	Sf_Method(&f, l, "SetModel", "(O)", G(GD, "DataCubeModel"));
	Sf_SetAttrI(&f, l, "Radius", 3500);
	Sf_SetAttrI(&f, l, "AppearsIfFree", 0);
	Sf_SetAttrI(&f, l, "AutoFace", 0);
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.25));
	Sf_SetAttrS(&f, l, "OnTake", "Mission_GetDataPart1.TakeData");
	Sf_Import(&f, "Items");
	Sf_Run(&f, "Items", "SetSound", "(Os)", l, "TakeDataCube");
	Sf_Later(&f, G(GD, "TimeDataDisappears"), G(GD, "DelData"), Sf_Build(&f, "(O)", Sf_Attr(&f, l, "Name")), NULL);
	return Sf_None(&f);
}

static PyObject *Data_DelData(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	PyObject *e = Sf_Entity(&f, name);
	if (Sf_True(&f, e) > 0) {
		Sf_Method(&f, e, "Del", "()");
	}
	return Sf_None(&f);
}

/* the functionaries after the transport with only the rail */
static byte Gd_FuncsAttack(scriptFrame_t *f) {
	PyObject *name;
	PyObject *fname = Sf_Ask(f, "Scrap", "EntityListGet", "(ss)", "Enemies", "First");
	PyObject *fn = Sf_Entity(f, fname);
	int mark = f->n;
	while (Sf_True(f, fn) > 0) {
		Sf_Method(f, Sf_Global(f, GD, "funcsAttackingTransport"), "append", "(O)", fname);
		fname = Sf_Attr(f, fn, "NextInList");
		fn = Sf_Entity(f, fname);
		PyObject *keep[2] = { fname, fn };
		Sf_ReleaseKeep(f, mark, keep, 2);
	}
	PyObject *funcs = Sf_Global(f, GD, "funcsAttackingTransport");
	mark = f->n;
	for (int k = 0; (name = Sf_Next(f, funcs, k)) != NULL; k++) {
		fn = Sf_Entity(f, name);
		Sf_SetAttrS(f, fn, "WeapList", "0,0,0,0,1,0,1");
		Sf_SetAttrS(f, fn, "EntityList", "");
		Sf_Run(f, "SAI", "SetStateVehicle", "(iOO)", 2, name, Sf_Global(f, GD, "currentTransport"));
		Sf_Release(f, mark);
	}
	return !f->failed;
}

/* the transport gives no more data: not a target any more */
static byte Gd_TransportDone(scriptFrame_t *f, PyObject *e) {
	if (Sf_True(f, e) > 0) {
		Sf_SetAttrS(f, e, "TgType", "none");
		Sf_Run(f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
	}
	return !f->failed;
}

/* TakeData(ItemName, CharName): a data unit more; all of them end the mission, the transport's share a new transport */
static PyObject *Data_TakeData(PyObject *self, PyObject *args) {
	PyObject *item, *ch, *name;
	SF_ARGS("OO", &item, &ch);
	if (Dt_UsrIs(&f, USR(), ch) <= 0) {
		return Sf_None(&f);
	}
	PyObject *itm = Sf_Entity(&f, item);
	Sf_SetAttrI(&f, itm, "rTime", 20);
	Sf_Later(&f, Sf_Int(&f, 5), Sf_Attr(&f, itm, "Del"), NOARGS, NULL);
	if (f.failed) {
		return Sf_None(&f);
	}
	PyObject *num = Sf_OpI(&f, Sf_ToInt(&f, SAVE("NumData")), '+', 1);
	int over = Sf_Compare(&f, num, ">", G(GD, "MAX_DATA_UNITS"));
	if (Sf_Except(&f) || over > 0) {
		return Sf_None(&f);
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumData", Sf_Repr(&f, num));
	PyObject *e = Sf_Entity(&f, G(GD, "currentTransport"));
	int all = Sf_Compare(&f, num, "==", G(GD, "MAX_DATA_UNITS"));
	if (all > 0) {
		Gd_TransportDone(&f, e);
		Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "SpawnData");
		Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "AddTransport");
		Sf_Import(&f, "Parking");
		Dt_ScorerCall(&f, Py_None);
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
		PyObject *funcs = G(GD, "funcsAttackingTransport");
		int mark = f.n;
		for (int k = 0; (name = Sf_Next(&f, funcs, k)) != NULL; k++) {
			PyObject *fn = Sf_Entity(&f, name);
			Sf_SetAttrS(&f, fn, "EntityList", "Traffic");
			Sf_SetAttrS(&f, fn, "TgType", "none");
			Sf_Release(&f, mark);
		}
		Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Transports", "OnDeath", "");
		Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Transports", "OnDamage", "");
		Dt_Com(&f, "MissionGetDataPart1_RemoteMessage6", "Boss", G(GD, "SetNextMission"), NOARGS);
	} else if (all == 0) {
		int attack = Sf_Compare(&f, num, ">", G(GD, "NumDataFuncsAttackTransport"));
		if (attack > 0) {
			attack = Sf_True(&f, e);
		}
		if (attack > 0) {
			if (Sf_True(&f, G(GD, "funcsAttackTransportMessage")) == 0) {
				Sf_SetGlobal(&f, GD, "funcsAttackTransportMessage", Sf_Int(&f, 1));
				Dt_Com(&f, "MissionGetDataPart1_RemoteMessage8", "Functionary", NULL, NULL);
			}
			Gd_FuncsAttack(&f);
		}
		Sf_SetGlobal(&f, GD, "currentTransportSpawnedData", Sf_OpI(&f, G(GD, "currentTransportSpawnedData"), '+', 1));
		if (Sf_Compare(&f, G(GD, "currentTransportSpawnedData"), ">=", G(GD, "MAX_TRANSPORTS_DATA_UNITS")) > 0) {
			Gd_TransportDone(&f, e);
			Sf_SetGlobal(&f, GD, "droppedData", Sf_Int(&f, 0));
			Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "SpawnData");
			Dt_Com(&f, "MissionGetDataPart1_RemoteMessage2", "Boss", G(GD, "AddTransportToList"), NOARGS);
		}
	}
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* SetNextMission(): on to Mission_GetDataPart2 in the bank */
static PyObject *Data_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "MissionsFuncs");
	Gd_TransportDone(&f, Sf_Entity(&f, G(GD, "currentTransport")));
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "NumData");
	Dt_Next(&f, "Mission_GetDataPart2", "Bank", "Mission_TalkToBoss2_MissionText1");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the bar of the data units */
static PyObject *Data_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Dt_Text(&f, id, "DataStatusText", Dt_Y(&f, 0), Dt_Label(&f, "Mission_GetDataPart1_Scorer_1"), NULL);
	const char *bar = "DataStatus";
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, bar, "Bar");
	Lv_BarDefaults(&f, id, bar);
	Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, bar, "IsAutoUpdatable", 0);
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, bar, "MaxValue", G(GD, "MAX_DATA_UNITS"));
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, bar, "Value", Sf_ToInt(&f, SAVE("NumData")));
	return Sf_None(&f);
}

PyMethodDef g_DownTownGetDataPart1Methods[] = {
	{ "GetFuncEnemiesProfile", Data_GetFuncEnemiesProfile, 1, NULL },
	{ "InitStatus", Data_InitStatus, 1, NULL },
	{ "Init", Data_Init, 1, NULL },
	{ "InitMission", Data_InitMission, 1, NULL },
	{ "CreateTransport", Data_CreateTransport, 1, NULL },
	{ "CreateFunctionary", Data_CreateFunctionary, 1, NULL },
	{ "AddTransportToList", Data_AddTransportToList, 1, NULL },
	{ "SpawnFunctionaries", Data_SpawnFunctionaries, 1, NULL },
	{ "OnFunctionaryDeath", Data_OnFunctionaryDeath, 1, NULL },
	{ "OnTransportsDeath", Data_OnTransportsDeath, 1, NULL },
	{ "OnTransportsDamage", Data_OnTransportsDamage, 1, NULL },
	{ "SpawnData", Data_SpawnData, 1, NULL },
	{ "DropData", Data_DropData, 1, NULL },
	{ "DelData", Data_DelData, 1, NULL },
	{ "TakeData", Data_TakeData, 1, NULL },
	{ "SetNextMission", Data_SetNextMission, 1, NULL },
	{ "MissionScorer", Data_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_HumphreyRace: the first crazy bet, a race against Humphrey with the doors of the level closed ---- */

/* InitStatus(LevelPath): the challenge of the race (only the first race of the level) or none */
static PyObject *Bet_InitStatus(PyObject *self, PyObject *args) {
	static const char *const race[] = { "Revenge.NumEnemies", "3", "Challenge.NumEnemies", "3", "Challenge.Type", "Race", "Challenge.Foe",
		"Humphrey", "Challenge.Money", "0", NULL };
	static const char *const none[] = { "Revenge.Type", "", "Revenge.Foe", "", "Revenge.NumEnemies", "", "Challenge.NumEnemies", "",
		"Challenge.Type", "", "Challenge.Foe", "", "Challenge.Money", "", "Race.Profile", "", NULL };
	PATH_ARGS();
	Lv_AcShips(&f, HR, "[\"SMayor1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
	Sf_Import(&f, "MapRace");
	int racing = Sf_EqS(&f, SAVE("MissionState"), "HumphreyRace");
	if (racing > 0) {
		Lv_Saves(&f, race);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Race.Profile", G(HR, "RACE_PROFILE"));
		Sf_SetMod(&f, "MapRace", "Races", Sf_Build(&f, "[O]", Sf_Mod(&f, "MapRace", "EasyRaceDownTown1")));
		Dt_Com(&f, "Mission_FirstCrazyBet_RemoteMessage5", "CrazyGambler", G(HR, "ShowSystemMessage"), NOARGS);
	} else if (racing == 0) {
		Lv_Saves(&f, none);
		Sf_SetMod(&f, "MapRace", "Races", Sf_Build(&f, "[]"));
	}
	return Sf_None(&f);
}

static PyObject *Bet_ShowSystemMessage(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "RaceMaker");
	Sf_Run(&f, "RaceMaker", "ShowSystemMessage", "(O)", Sf_Lang(&f, Sf_Str(&f, "RaceSystemGetShip")));
	return Sf_None(&f);
}

/* Init(LevelPath): the doors of the level closed; the race with its callbacks and the boost message, or the call of
 * the crazy gambler after it */
static PyObject *Bet_Init(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "Parking");
	Sf_Run(&f, "SScorer", "AddModel", "(isssii)", 0, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3, 25);
	Sf_EntSetS(&f, "DM_Door_CityDoorA_00", "OnObjSlot", "!CloseDoorSlot");
	Sf_EntSetI(&f, "DM_Door_MetroExt_Downtown", "ArrowCanUse", 0);
	Sf_EntSetS(&f, "DM_Door_Outmap_Commercial", "OnObjSlot", "!CloseDoorSlot");
	Sf_EntSetS(&f, "DM_Door_Outmap_Scrapyard", "OnObjSlot", "!CloseDoorSlot");
	Sf_EntSetS(&f, "DM_Door_Outmap_Outskirts", "OnObjSlot", "!CloseDoorSlot");
	Sf_EntSetS(&f, "DM_Door_Elevator_Press", "UseFunc", "Mission_HumphreyRace.OnTryingToExit");
	int racing = Sf_EqS(&f, SAVE("MissionState"), "HumphreyRace");
	if (racing > 0) {
		Sf_Run(&f, "Scrap", "Set", "(si)", "TimeToCancelCurrentMission", 0);
		Sf_SetMod(&f, "RaceMaker", "SkipTimer", Sf_Int(&f, 1));
		Sf_SetMod(&f, "RaceMaker", "OnPlayerWinsRace", G(HR, "OnPlayerWinsRace"));
		Sf_SetMod(&f, "RaceMaker", "OnPlayerLoosesRace", G(HR, "OnPlayerLoosesRace"));
		Sf_SetMod(&f, "RaceMaker", "OnPreInitRace", G(HR, "DelRacers"));
		Sf_SetMod(&f, "RaceMaker", "OnInitRace", G(HR, "SetMissionTarget"));
		Sf_SetMod(&f, "RaceMaker", "OnCheckPointCallback", G(HR, "ClosePressHangar"));
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "BadBoy0");
		Sf_SetGlobal(&f, HR, "prevSwitchVehicleMissionCallback", Sf_Mod(&f, "Parking", "SwitchVehicleMissionCallback"));
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(HR, "ShowBoostMessage"));
	} else if (racing == 0) {
		Sf_Own(&f, HR, "CallCrazyGambler", "()");
	}
	return Sf_None(&f);
}

/* ShowBoostMessage(): the callback it replaced, then the message of the boost key */
static PyObject *Bet_ShowBoostMessage(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "RaceMaker");
	PyObject *prev = G(HR, "prevSwitchVehicleMissionCallback");
	if (Sf_True(&f, prev) > 0) {
		Sf_Call(&f, prev, "()");
	}
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(HR, "prevSwitchVehicleMissionCallback"));
	PyObject *text = Sf_Lang(&f, Sf_Str(&f, "Mission_FirstCrazyBet_SystemMessage4"));
	PyObject *msg = Sf_Op(&f, text, '%', Sf_Ask(&f, "SInput", "GetDefinedEntry", "(isss)", 0, "Racer", "Boost", "Mouse"));
	Sf_Run(&f, "SScorer", "SetMsgText", "(OO)", msg, Sf_OpI(&f, Sf_Time(&f), '+', 10));
	return Sf_None(&f);
}

/* ClosePressHangar(): at the first checkpoint the door of the press hangar closes */
static PyObject *Bet_ClosePressHangar(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "RaceMaker");
	Sf_EntSetS(&f, "DM_Door_CityDoorA_01", "OnObjSlot", "!CloseDoorSlot");
	Sf_SetMod(&f, "RaceMaker", "OnCheckPointCallback", Py_None);
	return Sf_None(&f);
}

/* DelRacers(names): the other racers go, only Humphrey races */
static PyObject *Bet_DelRacers(PyObject *self, PyObject *args) {
	PyObject *names, *name;
	SF_ARGS("O", &names);
	int mark = f.n;
	for (int k = 0; (name = Sf_Next(&f, names, k)) != NULL; k++) {
		PyObject *e = Sf_Entity(&f, name);
		if (Sf_True(&f, e) > 0) {
			Sf_Method(&f, e, "Del", "()");
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* the phone cab of a mission (CallCrazyGambler, CallHumphrey, CallDeepThroat and the like) */
static PyObject *Dt_CallCab(PyObject *args, const char *use) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Dt_PhoneCab(&f, use);
	return Sf_None(&f);
}

static PyObject *Bet_CallCrazyGambler(PyObject *self, PyObject *args) {
	(void)self;
	return Dt_CallCab(args, "Mission_HumphreyRace.UsePhoneCab");
}

/*
 * UsePhoneCab of a mission (UsrName, CharName, optId, FullOption): the audio com of the phone cab into the global pc
 * of module, the police warning police of who while the alarm is on, else the NI talk key of who with the end
 * function end of module (reset: the cab's use back to the default first)
 */
static PyObject *Dt_UsePhone(PyObject *args, const char *module, const char *police, const char *who, const char *key, const char *end, int reset) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "PhoneCab");
	Sf_SetMod(&f, "Speech", "NPCName", ch);
	Sf_SetMod(&f, "Speech", "PlayerName", usr);
	Sf_Run(&f, "PhoneCab", "UseAudioCom", "(OOOO)", usr, ch, opt, full);
	Sf_SetGlobal(&f, module, "pc", Sf_Entity(&f, ch));
	int alarm = Dt_Alarm(&f);
	if (alarm > 0) {
		Sf_Run(&f, "Speech", "StartObjSpch", "(Oss)", G(module, "pc"), police, who);
	} else if (alarm == 0) {
		if (reset) {
			Sf_SetAttrS(&f, G(module, "pc"), "UseFunc", "PhoneCab.DefUseAudioCom");
		}
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(module, "pc"), 0);
		Sf_SpeechEnd(&f, G(module, end), NULL);
		Sf_NISpeech(&f, who, key);
	}
	return Sf_None(&f);
}

static PyObject *Bet_UsePhoneCab(PyObject *self, PyObject *args) {
	(void)self;
	return Dt_UsePhone(args, HR, "Generics_StillPolice1_CrazyGambler", "CrazyGambler", "Mission_FirstCrazyBet_ComCrazyGambler_NI_1Msg",
		"PreGivePlans", 1);
}

/* `if not (item in lst): lst.append(item); Scrap.SetSaveVar(var, `lst`)` with lst the list of the save variable */
static byte Dt_SaveAdd(scriptFrame_t *f, const char *module, const char *var, const char *item) {
	int mark = f->n;
	PyObject *lst = Lv_SaveList(f, var, module);
	if (Sf_In(f, Sf_Str(f, item), lst) == 0) {
		Lv_SaveAppend(f, var, lst, Sf_Str(f, item));
	}
	Sf_Release(f, mark);
	return !f->failed;
}

/* PreGivePlans2(): the blueprints of the mercenary ship and engine, the next mission, the plans 5 s later */
static PyObject *Bet_PreGivePlans2(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Dt_SaveAdd(&f, HR, "Hangar.shipsToEditList", "SMerc1");
	Dt_SaveAdd(&f, HR, "Hangar.availableEnginesList", "MMERC1");
	Sf_Own(&f, HR, "SetNextMission", "()");
	Sf_Later(&f, Sf_Int(&f, 5), G(HR, "GivePlans"), NOARGS, NULL);
	return Sf_None(&f);
}

/* GivePlans(): the message of the new blueprints and Rusty */
static PyObject *Bet_GivePlans(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *cad = Sf_Lang(&f, Sf_Str(&f, "Mission_FirstCrazyBet_SystemMessage3"));
	PyObject *t = Sf_Time(&f);
	Sf_Run(&f, "SScorer", "SetMsgText", "(OO)", cad, Sf_Op(&f, t, '+', Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)", cad)));
	Sf_Run(&f, "Scrap", "IncSaveVar", "(si)", "NewBluePrintAvaliable", 2);
	Dt_Com(&f, "Mission_FirstCrazyBet_RemoteMessage9", "Rusty", NULL, NULL);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* SetNextMission(): the doors open again, on to Mission_MeetArchbishop, the places of the characters */
static PyObject *Bet_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const doors[] = { "DM_Door_CityDoorA_00", "DM_Door_CityDoorA_01", "DM_Door_Outmap_Commercial", "DM_Door_Outmap_Scrapyard",
		"DM_Door_Outmap_Outskirts", NULL };
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Temple", "Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "Temple", NULL };
	SF_ARGS("");
	Sf_Import(&f, "PhoneCab");
	Sf_EntSetI(&f, "DM_Door_MetroExt_Downtown", "ArrowCanUse", 1);
	for (int i = 0; doors[i]; i++) {
		Sf_EntSetS(&f, doors[i], "OnObjSlot", "!AutoDoorSlot");
	}
	PyObject *pc = Sf_Ask(&f, "PhoneCab", "GetFree", "()");
	Sf_SetAttrS(&f, pc, "UseFunc", "PhoneCab.DefUseAudioCom");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "HumphreyRaceTutorial");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Dt_Next(&f, "Mission_MeetArchbishop", "Temple", "Mission_FirstContactPress_MissionText3");
	Lv_Saves(&f, places);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

PyMethodDef g_DownTownHumphreyRaceMethods[] = {
	{ "InitStatus", Bet_InitStatus, 1, NULL },
	{ "ShowSystemMessage", Bet_ShowSystemMessage, 1, NULL },
	{ "Init", Bet_Init, 1, NULL },
	{ "ShowBoostMessage", Bet_ShowBoostMessage, 1, NULL },
	{ "ClosePressHangar", Bet_ClosePressHangar, 1, NULL },
	{ "DelRacers", Bet_DelRacers, 1, NULL },
	{ "CallCrazyGambler", Bet_CallCrazyGambler, 1, NULL },
	{ "UsePhoneCab", Bet_UsePhoneCab, 1, NULL },
	{ "PreGivePlans2", Bet_PreGivePlans2, 1, NULL },
	{ "GivePlans", Bet_GivePlans, 1, NULL },
	{ "SetNextMission", Bet_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_DistractHumphrey: Humphrey and the player race to destroy ten ships, the player must keep him busy ---- */

static const char *const humphrey_profile[] = {
	"Ship=SHUMP1", "StupidPathFinding=0", "ControlStrafe=2", "CadenceShootCM=2", "UsePercSonic=50", "UsePercCloud=25", "UsePercElectro=25",
	"UsePercHook=0", "FactorHookMap=10", "CadenceShoot=0", "ControlAim=10", "AngAttack=3.1415", "CounterAttack=1", "ControlBoost=1",
	"ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=1.0", "FactorDamageItemLife=0.35", "FactorItemAmmo=0.1",
	"ControlStrafeEvasion=0", "UsePercHookGetAway=0", "TimeMaxStrafe=10", "VelMaxStrafe=1.0", NULL,
};

static PyObject *Hump_GetHumphreyProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, DH, humphrey_profile);
}

static PyObject *Hump_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, DH, "[\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
}

/* Init(LevelPath): the step of the mission by MissionState */
static PyObject *Hump_Init(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Run(&f, "Scrap", "Preload3DObject", "(O)", G(DH, "MeetingPoint_ModelFile"));
	PyObject *state = SAVE("MissionState");
	int s = Sf_EqS(&f, state, "CallHumphrey");
	if (s > 0) {
		Sf_Own(&f, DH, "CallHumphrey", "()");
	} else if (s == 0 && (s = Sf_EqS(&f, state, "DistractHumphrey")) > 0) {
		int again = Sf_EqS(&f, SAVE("DistractionStart"), "1");
		if (again > 0) {
			Dt_Com(&f, "Mission_DistractHumphrey_RemoteMessage6", "DeepThroat", G(DH, "CreateWaypoint"), NOARGS);
		} else if (again == 0) {
			Sf_Own(&f, DH, "CreateWaypoint", "()");
		}
	} else if (s == 0 && Sf_EqS(&f, state, "CallDeepThroat") > 0) {
		Sf_Own(&f, DH, "PreCallDeepThroat", "()");
	}
	return Sf_None(&f);
}

static PyObject *Hump_CallHumphrey(PyObject *self, PyObject *args) {
	(void)self;
	return Dt_CallCab(args, "Mission_DistractHumphrey.UsePhoneCab");
}

static PyObject *Hump_UsePhoneCab(PyObject *self, PyObject *args) {
	(void)self;
	return Dt_UsePhone(args, DH, "Generics_StillPolice1_Humphrey", "Humphrey", "Mission_TalkToHumphrey_ComHumphrey_NI_1Msg", "PreDistractHumphrey", 0);
}

/* CreateWaypoint(): the meeting point at the dummy of the mission */
static PyObject *Hump_CreateWaypoint(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *dummy = Lv_Dummy(&f, "DM_Mission_DistractHumphrey", 1);
	if (Sf_True(&f, dummy) == 0) {
		Sf_RaiseText(&f, "No existe el dummy para esta misi\xf3n: 'DM_Mission_DistractHumphrey'");
		return Sf_None(&f);
	}
	PyObject *p = Sf_Item(&f, dummy, 1);
	PyObject *x = Sf_Item(&f, p, 0), *y = Sf_Item(&f, p, 1), *z = Sf_Item(&f, p, 2);
	Sf_SetGlobal(&f, DH, "missionStartWaypoint", Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "MissionStart", x, y, z, "Item"));
	Sf_Method(&f, G(DH, "missionStartWaypoint"), "SetModel", "(O)", G(DH, "MeetingPoint_ModelFile"));
	Sf_SetAttrI(&f, G(DH, "missionStartWaypoint"), "RadarShow", 1);
	Sf_SetAttrI(&f, G(DH, "missionStartWaypoint"), "Radius", 3500);
	Sf_SetAttr(&f, G(DH, "missionStartWaypoint"), "Ang", Sf_Build(&f, "(iii)", 0, 180, 0));
	Sf_SetAttrS(&f, G(DH, "missionStartWaypoint"), "OnTake", "Mission_DistractHumphrey.OnStartDistraction");
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(DH, "missionStartWaypoint"), 1);
	return Sf_None(&f);
}

/* OnStartDistraction(ItemName, VehicleName): the player's ship at the meeting point starts the distraction (not with
 * the alarm on or an enemy around) */
static PyObject *Hump_OnStartDistraction(PyObject *self, PyObject *args) {
	PyObject *item, *vehicle;
	SF_ARGS("OO", &item, &vehicle);
	Sf_Import(&f, "Parking");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) == 0 || Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "!=", vehicle) > 0) {
		return Sf_None(&f);
	}
	int busy = Sf_True(&f, Sf_Ask(&f, "SLogic", "IsEnemyActive", "()"));
	if (busy == 0) {
		busy = Dt_Alarm(&f);
	}
	if (busy > 0) {
		Sf_SetAttrS(&f, G(DH, "missionStartWaypoint"), "OnTake", "");
		Dt_Com(&f, "Mission_DistractHumphrey_RemoteMessage11", "Humphrey", G(DH, "EnableMissionWaypoint"), NULL);
		return Sf_None(&f);
	}
	if (Sf_EqS(&f, SAVE("DistractionStart"), "0") > 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "DistractionStart", "1");
	}
	Sf_SetAttrI(&f, G(DH, "missionStartWaypoint"), "Active", 0);
	Sf_Own(&f, DH, "CreateHumphrey", "(O)", Sf_Attr(&f, G(DH, "missionStartWaypoint"), "Pos"));
	Sf_Own(&f, DH, "StartCameraTravelling", "()");
	return Sf_None(&f);
}

static PyObject *Hump_EnableMissionWaypoint(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_True(&f, G(DH, "missionStartWaypoint")) > 0) {
		Sf_SetAttrS(&f, G(DH, "missionStartWaypoint"), "OnTake", "Mission_DistractHumphrey.OnStartDistraction");
	}
	return Sf_None(&f);
}

/* StartCameraTravelling(): the player's ship flies to the meeting point while a camera comes close to it */
static PyObject *Hump_StartCameraTravelling(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	int t = Sf_True(&f, usr);
	if (t > 0) {
		t = Sf_Compare(&f, Sf_Attr(&f, usr, "UsrControl"), "!=", Sf_Int(&f, 0));
	}
	if (t > 0) {
		return Sf_None(&f);
	}
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Menu", "MovieScorer", "(i)", 0);
	Sf_SetAttrI(&f, usr, "UsrControl", -1);
	Sf_SetAttr(&f, usr, "Vel", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_SetAttr(&f, usr, "MovePos", Sf_Attr(&f, G(DH, "missionStartWaypoint"), "Pos"));
	Sf_SetAttr(&f, usr, "MoveAng", Sf_Attr(&f, G(DH, "missionStartWaypoint"), "Ang"));
	Sf_SetAttr(&f, usr, "MoveTime", Sf_Float(&f, 1.0));
	Sf_SetAttrI(&f, usr, "MoveFade", 2);
	Sf_SetAttrI(&f, usr, "AvoidCrash", 1);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "MainMissionEvent", "");
	PyObject *prev = Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0);
	PyObject *ang = Sf_Ask(&f, "SVec", "Add", "(O(iii))", Sf_Attr(&f, G(DH, "missionStartWaypoint"), "Ang"), 15, 0, 0);
	PyObject *r0 = Sf_Ask(&f, "SVec", "Rotate3D", "((iii)O)", 0, 0, -8000, ang);
	PyObject *p0 = Sf_Ask(&f, "SVec", "Add", "(OO)", r0, Sf_Attr(&f, G(DH, "missionStartWaypoint"), "Pos"));
	PyObject *r1 = Sf_Ask(&f, "SVec", "Rotate3D", "((iii)O)", 0, 0, -2000, ang);
	PyObject *p1 = Sf_Ask(&f, "SVec", "Add", "(OO)", r1, Sf_Attr(&f, G(DH, "missionStartWaypoint"), "Pos"));
	PyObject *cam = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "RaceCamera", Sf_Item(&f, p0, 0), Sf_Item(&f, p0, 1), Sf_Item(&f, p0, 2),
		"CamPath");
	Sf_SetAttrI(&f, cam, "nWayPoint", 2);
	Sf_SetAttr(&f, cam, "TgtCam00", prev);
	Sf_SetAttrS(&f, cam, "TgtCam00", "");
	Sf_SetAttr(&f, cam, "Pos00", p0);
	Sf_SetAttr(&f, cam, "Ang00", ang);
	Sf_SetAttrI(&f, cam, "FadeIn00", 1);
	Sf_SetAttrI(&f, cam, "FadeOut00", 0);
	Sf_SetAttr(&f, cam, "Time00", Sf_Float(&f, 5.0));
	Sf_SetAttr(&f, cam, "TgtCam01", prev);
	Sf_SetAttrS(&f, cam, "TgtCam01", "");
	Sf_SetAttr(&f, cam, "Pos01", p1);
	PyObject *a0 = Sf_Op(&f, Sf_Item(&f, ang, 0), '/', Sf_Float(&f, 4.0));
	Sf_SetAttr(&f, cam, "Ang01", Sf_Build(&f, "(OOO)", a0, Sf_Item(&f, ang, 1), Sf_Item(&f, ang, 2)));
	Sf_SetAttrI(&f, cam, "FadeIn01", 0);
	Sf_SetAttrI(&f, cam, "FadeOut01", 1);
	Sf_SetAttrI(&f, cam, "Time", 0);
	Sf_Run(&f, "Scrap", "SetCam", "(iO)", 0, Sf_Attr(&f, cam, "Name"));
	PyObject *tt = Sf_Attr(&f, cam, "Time00");
	PyObject *at = Sf_Op(&f, Sf_Op(&f, Sf_Time(&f), '+', tt), '+', Sf_Float(&f, 0.2));
	Sf_Schedule(&f, at, G(DH, "AbortCameraTravelling"), Sf_Build(&f, "(OO)", Sf_Attr(&f, usr, "Name"), prev), NULL);
	Sf_Import(&f, "Scene");
	Sf_Run(&f, "Scene", "SetText", "(sO)", "Mission_DistractHumphrey_RemoteMessage12", tt);
	return Sf_None(&f);
}

/* AbortCameraTravelling(UsrEntityName, prevCamera): the player's control and camera back, the mission starts */
static PyObject *Hump_AbortCameraTravelling(PyObject *self, PyObject *args) {
	PyObject *name, *prev;
	SF_ARGS("OO", &name, &prev);
	if (Sf_True(&f, prev) > 0) {
		PyObject *usr = Sf_Entity(&f, name);
		if (Sf_True(&f, usr) > 0) {
			Sf_SetAttr(&f, usr, "MoveTime", Sf_Float(&f, 0.0));
			Sf_SetAttrI(&f, usr, "UsrControl", 0);
		}
		Sf_Run(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "Menu.Init");
		Sf_Run(&f, "Scrap", "Set", "(ss)", "MainMissionEvent", "MissionsFuncs.ShowMissionInfo");
		Sf_Method(&f, Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0)), "Del", "()");
		Sf_Run(&f, "Scrap", "SetCam", "(iO)", 0, prev);
	}
	Sf_Own(&f, DH, "LaunchMission", "()");
	return Sf_None(&f);
}

/* CreateHumphrey(targetPos): Humphrey (again), 5000 beside the meeting point flying to it, or after a ship */
static PyObject *Hump_CreateHumphrey(PyObject *self, PyObject *args) {
	PyObject *target;
	SF_ARGS("O", &target);
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "OutMap");
	PyObject *initial = target;
	if (Sf_True(&f, initial) > 0) {
		PyObject *x = Sf_OpI(&f, Sf_Item(&f, target, 0), '+', 5000);
		initial = Sf_Build(&f, "(OOO)", x, Sf_Item(&f, target, 1), Sf_Item(&f, target, 2));
	}
	PyObject *h = Sf_Entity(&f, G(DH, "humphreyName"));
	if (Sf_True(&f, h) > 0) {
		Sf_Method(&f, h, "Del", "()");
	}
	h = Lv_SpawnCar(&f, "Humphrey", 0);
	Lv_StdProfile(&f, DH, "GetHumphreyProfile", h);
	Sf_SetAttrS(&f, h, "TgType", "green");
	Sf_SetAttrS(&f, h, "OnDeath", "Mission_DistractHumphrey.OnHumphreyDeath");
	Sf_SetAttrS(&f, h, "OnDamage", "Mission_DistractHumphrey.OnHumphreyDamage");
	PyObject *life = G(DH, "HUMPHREY_LIFE");
	Sf_SetAttr(&f, h, "Life", life);
	Sf_SetAttr(&f, h, "MaxLife", life);
	Sf_SetAttrS(&f, h, "WeapList", "0,0,7,0,7,0");
	Sf_SetGlobal(&f, DH, "humphreyName", Sf_Attr(&f, h, "Name"));
	int go = Sf_True(&f, initial);
	if (go > 0) {
		Sf_SetAttr(&f, h, "Pos", initial);
		Sf_Run(&f, "RaceMaker", "PointObjectTo", "(OO)", h, target);
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOOOOiiddd)", 1, Sf_Attr(&f, h, "Name"), Sf_Item(&f, target, 0), Sf_Item(&f, target, 1),
			Sf_Item(&f, target, 2), 4500, 0, 0.1, 20.0, 20.0);
	} else if (go == 0) {
		Sf_Own(&f, DH, "AssignNewHumphreyTarget", "()");
	}
	return Sf_None(&f);
}

/* LaunchMission(): the damage callback, the timer and the scorer, Humphrey's target, DeepThroat's messages */
static PyObject *Hump_LaunchMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "RacerScorer");
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "OnGeneralDamage", "Mission_DistractHumphrey.OnGeneralDamage");
	Sf_Run(&f, "Scrap", "Set", "(sO)", "TimeSpecialUseTimer", G(DH, "missionTime"));
	Sf_Run(&f, "Scrap", "Set", "(si)", "CounterType", 0);
	Sf_Run(&f, "RacerScorer", "InitMissionScorer", "(iiO)", 0, 0, Sf_Repr(&f, G(DH, "maxShips")));
	Dt_ScorerCall(&f, G(DH, "MissionScorer"));
	Sf_DrawScorer(&f);
	Sf_Own(&f, DH, "AssignNewHumphreyTarget", "()");
	Sf_Method(&f, G(DH, "missionStartWaypoint"), "Del", "()");
	Sf_SetGlobal(&f, DH, "missionStartWaypoint", Py_None);
	PyObject *iv = Sf_OpI(&f, G(DH, "missionTime"), '/', 4);
	Dt_LaterCom(&f, Sf_Int(&f, 2), Sf_Build(&f, "(ss)", "Mission_DistractHumphrey_RemoteMessage1", "DeepThroat"), "DTMessages");
	Dt_LaterCom(&f, iv, Sf_Build(&f, "(ss)", "Mission_DistractHumphrey_RemoteMessage2", "DeepThroat"), "DTMessages");
	Dt_LaterCom(&f, Sf_Op(&f, Sf_Int(&f, 2), '*', iv), Sf_Build(&f, "(ss)", "Mission_DistractHumphrey_RemoteMessage3", "DeepThroat"), "DTMessages");
	Dt_LaterCom(&f, Sf_Op(&f, Sf_Int(&f, 3), '*', iv), Sf_Build(&f, "(ss)", "Mission_DistractHumphrey_RemoteMessage4", "DeepThroat"), "DTMessages");
	Sf_SetMod(&f, "Vehicles", "OnDeathCallback", G(DH, "OnDeath"));
	return Sf_None(&f);
}

/* Humphrey after the ship name: SAI.SetStateVehicle(2, humphreyName, name); a ship he takes stops its own moves */
static int Hp_Attack(scriptFrame_t *f, PyObject *e, PyObject *name) {
	if (Dt_Active(f, e) <= 0) {
		return f->failed ? -1 : 0;
	}
	PyObject *changed = Sf_Ask(f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Global(f, DH, "humphreyName"), name);
	int t = Sf_True(f, changed);
	if (t > 0) {
		Sf_SetAttrI(f, e, "AI_Repos", 0);
	}
	return t;
}

/* AssignNewHumphreyTarget(): Humphrey after the ship the player is shooting, or the ship nearest the player (tried
 * again every second until he has one) */
static PyObject *Hump_AssignNewHumphreyTarget(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *h = Sf_Entity(&f, G(DH, "humphreyName"));
	if (Sf_True(&f, h) == 0) {
		return Sf_None(&f);
	}
	int changed = 0;
	PyObject *e = Sf_Entity(&f, G(DH, "shipNameForTaunt"));
	int taunt = Sf_True(&f, e);
	if (taunt > 0) {
		taunt = Sf_Compare(&f, Sf_Ask(&f, "Scrap", "Rand", "(dd)", 0.0, 0.99), "<", G(DH, "probHumpreyAttacksPlayerTarget"));
	}
	if (taunt > 0) {
		changed = Hp_Attack(&f, e, G(DH, "shipNameForTaunt"));
	} else if (taunt == 0) {
		PyObject *usr = USR();
		if (Sf_True(&f, usr) > 0) {
			PyObject *name = Sf_Ask(&f, "SLogic", "GetNearestShip", "(OO)", Sf_Attr(&f, usr, "Pos"), G(DH, "shipNameForTaunt"));
			changed = Hp_Attack(&f, Sf_Entity(&f, name), name);
		}
	}
	if (changed == 0 && !f.failed) {
		Sf_Later(&f, Sf_Int(&f, 1), G(DH, "AssignNewHumphreyTarget"), NOARGS, NULL);
	}
	return Sf_None(&f);
}

/* OnHumphreyDeath(LooserName, WinnerName): his target moves again; during the mission it starts over, after it he
 * comes back */
static PyObject *Hump_OnHumphreyDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	PyObject *e = Sf_Entity(&f, Sf_Attr(&f, Sf_Entity(&f, looser), "AI_Target"));
	if (Dt_Active(&f, e) > 0) {
		Sf_SetAttrI(&f, e, "AI_Repos", 1);
	}
	int complete = Sf_True(&f, G(DH, "missionComplete"));
	if (complete == 0) {
		Sf_Own(&f, DH, "StopMission", "()");
		Dt_Com(&f, "Mission_DistractHumphrey_RemoteMessage6", "DeepThroat", G(DH, "CreateWaypoint"), NOARGS);
	} else if (complete > 0 && Sf_Compare(&f, SAVE("MissionState"), "!=", Sf_Str(&f, "CallDeepThroat")) > 0) {
		Sf_Later(&f, Sf_Int(&f, 3), G(DH, "CreateHumphrey"), Sf_Build(&f, "(O)", Py_None), NULL);
	}
	return Sf_None(&f);
}

/* OnHumphreyDamage(LooserName, WinnerName, Damage): only the player can hurt him */
static PyObject *Hump_OnHumphreyDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) == 0) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, winner, "!=", Sf_Attr(&f, usr, "Name")) > 0) {
		PyObject *h = Sf_Entity(&f, looser);
		Sf_SetAttr(&f, h, "Life", Sf_Op(&f, Sf_Attr(&f, h, "Life"), '+', damage));
	}
	return Sf_None(&f);
}

/* OnGeneralDamage(LooserName, WinnerName, Damage): the ship the player hits is the one Humphrey may snatch */
static PyObject *Hump_OnGeneralDamage(PyObject *self, PyObject *args) {
	PyObject *looser, *winner, *damage;
	SF_ARGS("OOO", &looser, &winner, &damage);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) == 0) {
		return Sf_None(&f);
	}
	int t = Sf_Compare(&f, winner, "==", Sf_Attr(&f, usr, "Name"));
	if (t > 0) {
		t = Sf_Compare(&f, looser, "!=", G(DH, "humphreyName"));
	}
	if (t > 0) {
		Sf_SetGlobal(&f, DH, "shipNameForTaunt", looser);
	}
	return Sf_None(&f);
}

/* the end of the count when one side reached maxShips: the mission stops, Humphrey leaves; done when the time ran out
 * before (DeepThroat next), else it starts over */
static byte Hp_End(scriptFrame_t *f, const char *doneKey, const char *failKey) {
	Sf_Own(f, DH, "StopMission", "()");
	if (Sf_True(f, Sf_Ask(f, "Scrap", "GetEntity", "(O)", Sf_Global(f, DH, "humphreyName"))) > 0) {
		Sf_Run(f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Global(f, DH, "humphreyName"), 0);
	}
	int done = Sf_True(f, Sf_Global(f, DH, "missionComplete"));
	if (done > 0) {
		Sf_Run(f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "CallDeepThroat");
		Sf_Run(f, "Speech", "AddRemoteCom", "(ss)", doneKey, "Humphrey");
		Sf_Run(f, "Speech", "AddRemoteCom", "(ssOO)", "Mission_DistractHumphrey_RemoteMessage17", "DeepThroat",
			Sf_Global(f, DH, "PreCallDeepThroat"), Sf_Build(f, "()"));
	} else if (done == 0) {
		Sf_Run(f, "Speech", "AddRemoteCom", "(ss)", failKey, "Humphrey");
		Sf_Run(f, "Speech", "AddRemoteCom", "(ssOO)", "Mission_DistractHumphrey_RemoteMessage6", "DeepThroat",
			Sf_Global(f, DH, "CreateWaypoint"), Sf_Build(f, "()"));
	}
	return Sf_SetGlobal(f, DH, "missionComplete", Sf_Int(f, 0));
}

/* `if e: if SAI.GetStateVehicle(humphreyName) == 0 or LooserName == e.AI_Target: AssignNewHumphreyTarget 1 s later`
 * with e Humphrey */
static byte Hp_Retarget(scriptFrame_t *f, PyObject *looser) {
	PyObject *e = Sf_Ask(f, "Scrap", "GetEntity", "(O)", Sf_Global(f, DH, "humphreyName"));
	if (Sf_True(f, e) > 0) {
		int t = Sf_EqI(f, Sf_Ask(f, "SAI", "GetStateVehicle", "(O)", Sf_Global(f, DH, "humphreyName")), 0);
		if (t == 0) {
			t = Sf_Compare(f, looser, "==", Sf_Attr(f, e, "AI_Target"));
		}
		if (t > 0) {
			Sf_Later(f, Sf_Int(f, 1), Sf_Global(f, DH, "AssignNewHumphreyTarget"), Sf_Build(f, "()"), NULL);
		}
	}
	return !f->failed;
}

/* OnDeath(LooserName, WinnerName): the count of the ships Humphrey and the player destroyed */
static PyObject *Hump_OnDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Import(&f, "RacerScorer");
	PyObject *usr = USR();
	int t = Sf_True(&f, usr);
	PyObject *usrName = t == 0 ? Sf_Str(&f, "") : Sf_Attr(&f, usr, "Name");
	PyObject *e = Sf_Entity(&f, looser);
	t = Sf_True(&f, e);
	if (t > 0) {
		t = Sf_EqI(&f, Sf_Attr(&f, e, "ArrowCanUse"), 1);
	}
	if (t > 0) {
		Sf_Run(&f, "Speech", "RemoteComunication", "(ss)", "Mission_DistractHumphrey_RemoteMessage13", "Humphrey");
		return Sf_None(&f);
	}
	int byHump = Sf_Compare(&f, winner, "==", G(DH, "humphreyName"));
	int byUsr = byHump == 0 ? Sf_Compare(&f, winner, "==", usrName) : 0;
	if (byHump > 0 || byUsr > 0) {
		const char *mine = byHump > 0 ? "shipsKilledHumphrey" : "shipsKilledPlayer";
		Sf_SetGlobal(&f, DH, "shipsKilledPlayer", Sf_OpI(&f, G(DH, "shipsKilledPlayer"), '+', byHump > 0 ? -1 : 1));
		Sf_SetGlobal(&f, DH, "shipsKilledHumphrey", Sf_OpI(&f, G(DH, "shipsKilledHumphrey"), '+', byHump > 0 ? 1 : -1));
		int end = Sf_Compare(&f, G(DH, mine), "==", G(DH, "maxShips"));
		if (end > 0) {
			if (byHump > 0) {
				Hp_End(&f, "Mission_DistractHumphrey_RemoteMessage10", "Mission_DistractHumphrey_RemoteMessage7");
			} else {
				Hp_End(&f, "Mission_DistractHumphrey_RemoteMessage9", "Mission_DistractHumphrey_RemoteMessage8");
			}
		} else if (end == 0 && byHump > 0) {
			if (Sf_Compare(&f, looser, "==", G(DH, "shipNameForTaunt")) > 0) {
				PyObject *key = Sf_Cat(&f, "sO", "Mission_DistractHumphrey_RemoteMessage", Sf_Repr(&f, Sf_OpI(&f, G(DH, "numTaunt"), '+', 14)));
				Sf_Run(&f, "Speech", "AddRemoteCom", "(Os)", key, "Humphrey");
				Sf_SetGlobal(&f, DH, "numTaunt", Sf_OpI(&f, Sf_OpI(&f, G(DH, "numTaunt"), '+', 1), '%', 3));
			}
			Sf_Run(&f, "RacerScorer", "UpdateMissionScorer", "(iOO)", 0, G(DH, "shipsKilledPlayer"), G(DH, "shipsKilledHumphrey"));
			Sf_Later(&f, Sf_Int(&f, 1), G(DH, "AssignNewHumphreyTarget"), NOARGS, NULL);
		} else if (end == 0) {
			Sf_Run(&f, "RacerScorer", "UpdateMissionScorer", "(iOO)", 0, G(DH, "shipsKilledPlayer"), G(DH, "shipsKilledHumphrey"));
			Hp_Retarget(&f, looser);
		}
	} else if (byUsr == 0) {
		Hp_Retarget(&f, looser);
	}
	if (Sf_Compare(&f, looser, "==", G(DH, "shipNameForTaunt")) > 0) {
		Sf_SetGlobal(&f, DH, "shipNameForTaunt", Sf_Str(&f, ""));
	}
	return Sf_None(&f);
}

/* StopMission(): Humphrey back to the traffic, no callbacks, the counts to 0, no scorer */
static PyObject *Hump_StopMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Vehicles");
	PyObject *h = Sf_Entity(&f, G(DH, "humphreyName"));
	if (Sf_True(&f, h) > 0) {
		Sf_SetAttrS(&f, h, "OnDeath", "");
		Sf_SetAttrS(&f, h, "OnDamage", "");
		Sf_SetAttrS(&f, h, "TgType", "none");
		Sf_SetAttrS(&f, h, "EntityList", "Traffic");
	}
	Sf_SetMod(&f, "Vehicles", "OnDeathCallback", Py_None);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "OnGeneralDamage", "");
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "DTMessages");
	Sf_SetGlobal(&f, DH, "shipsKilledPlayer", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, DH, "shipsKilledHumphrey", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, DH, "shipNameForTaunt", Sf_Str(&f, ""));
	Sf_SetGlobal(&f, DH, "numTaunt", Sf_Int(&f, 0));
	Sf_SetMod(&f, "Vehicles", "OnDeathCallback", Py_None);
	Dt_ScorerCall(&f, Py_None);
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

static PyObject *Hump_CallDeepThroat(PyObject *self, PyObject *args) {
	(void)self;
	return Dt_CallCab(args, "Mission_DistractHumphrey.UsePhoneCab2");
}

static PyObject *Hump_UsePhoneCab2(PyObject *self, PyObject *args) {
	(void)self;
	return Dt_UsePhone(args, DH, "Generics_StillPolice1_Functionary", "DeepThroat", "Mission_TalkToDeepThroat9_ComDeepThroat_NI_1Msg", "SetNextMission",
		0);
}

/* MissionScorer(id): the time left (hidden once it ran out) and the counts of RacerScorer */
static PyObject *Hump_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id, *wh[2] = { NULL, NULL };
	SF_ARGS("O", &id);
	Sf_Import(&f, "RacerScorer");
	Dt_Text(&f, id, "RemainingTime", Dt_Y(&f, 0), Dt_Label(&f, "Mission_DistractHumphrey_Scorer1"), NULL);
	if (Sf_True(&f, G(DH, "missionComplete")) > 0) {
		Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "RemainingTime", "Visible", 0);
	}
	PyObject *area = Sf_Ask(&f, "SScorer", "GetTextArea", "(sO)", "Horatio", Dt_Label(&f, "Mission_DistractHumphrey_Scorer1"));
	Sf_Unpack(&f, area, 2, wh);
	Dt_Timer(&f, id, "MissionTimer", Sf_OpI(&f, Sf_Op(&f, Dt_X(&f, 0), '+', wh[0]), '+', 30), Dt_Y(&f, 12),
		"Mission_DistractHumphrey.TimeExpired", 0);
	Sf_Run(&f, "RacerScorer", "MissionScorer", "(i)", 0);
	return Sf_None(&f);
}

PyMethodDef g_DownTownDistractHumphreyMethods[] = {
	{ "GetHumphreyProfile", Hump_GetHumphreyProfile, 1, NULL },
	{ "InitStatus", Hump_InitStatus, 1, NULL },
	{ "Init", Hump_Init, 1, NULL },
	{ "CallHumphrey", Hump_CallHumphrey, 1, NULL },
	{ "UsePhoneCab", Hump_UsePhoneCab, 1, NULL },
	{ "CreateWaypoint", Hump_CreateWaypoint, 1, NULL },
	{ "OnStartDistraction", Hump_OnStartDistraction, 1, NULL },
	{ "EnableMissionWaypoint", Hump_EnableMissionWaypoint, 1, NULL },
	{ "StartCameraTravelling", Hump_StartCameraTravelling, 1, NULL },
	{ "AbortCameraTravelling", Hump_AbortCameraTravelling, 1, NULL },
	{ "CreateHumphrey", Hump_CreateHumphrey, 1, NULL },
	{ "LaunchMission", Hump_LaunchMission, 1, NULL },
	{ "AssignNewHumphreyTarget", Hump_AssignNewHumphreyTarget, 1, NULL },
	{ "OnHumphreyDeath", Hump_OnHumphreyDeath, 1, NULL },
	{ "OnHumphreyDamage", Hump_OnHumphreyDamage, 1, NULL },
	{ "OnGeneralDamage", Hump_OnGeneralDamage, 1, NULL },
	{ "OnDeath", Hump_OnDeath, 1, NULL },
	{ "StopMission", Hump_StopMission, 1, NULL },
	{ "CallDeepThroat", Hump_CallDeepThroat, 1, NULL },
	{ "UsePhoneCab2", Hump_UsePhoneCab2, 1, NULL },
	{ "MissionScorer", Hump_MissionScorer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_MultiplusRace: Ficus' race against the police, then Stupidus and his friends ---- */

static const char *const multiplus_profile[] = {
	"Ship=SFUNC1", "StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=1", "GoNextItemRace=0", "ControlHookRace=0", "CadenceShootCM=6",
	"UsePercSonic=90", "UsePercCloud=10", "UsePercElectro=0", "UsePercHook=0", "FactorHookMap=2", "CadenceShoot=6", "ControlAim=2",
	"AngAttack=1.5", "CounterAttack=0.5", "ControlBoost=0", "ControlBoostItemLife=1", "ControlBoostItemAmmo=0", "FactorDamageGetAway=0.25",
	"FactorDamageItemLife=0.4", "FactorItemAmmo=0.1", "ControlStrafeEvasion=0", "UsePercHookGetAway=0", "TimeMaxStrafe=4", "VelMaxStrafe=0.5",
	NULL,
};

static PyObject *Plus_GetFunctionaryProfile(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_ProfileFunc(args, MR, multiplus_profile);
}

/* InitStatus(LevelPath): the race of the mission is the only one of the level, no alarm */
static PyObject *Plus_InitStatus(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Import(&f, "MapRace");
	Sf_SetMod(&f, "MapRace", "Races", Sf_Build(&f, "[O]", G(MR, "RaceList")));
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "AlarmActive", "0");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "AlarmStatus", "0");
	Lv_AcShips(&f, MR, "[\"SMayor1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
	return Sf_None(&f);
}

/* Init(LevelPath): the race, the phone cab of Ficus or the hunt of Stupidus by the step of the mission */
static PyObject *Plus_Init(PyObject *self, PyObject *args) {
	static const char *const cab[] = { "Alarm", "Left", "Loose", "KillStupidus", NULL };
	PATH_ARGS();
	int s = Sf_EqS(&f, SAVE("MultiplusRace"), "Begin");
	if (s > 0) {
		Sf_Own(&f, MR, "InitRace", "()");
	} else if (s == 0 && Sf_InS(&f, SAVE("MultiplusRace"), cab) > 0) {
		Sf_Own(&f, MR, "SetPhoneCab", "()");
	}
	if (Sf_EqS(&f, SAVE("MultiplusRace"), "Killing") > 0) {
		Sf_Own(&f, MR, "InitKill", "()");
	}
	return Sf_None(&f);
}

/* RaceList(): the checkpoints of the race (those of EasyRaceDownTown2) */
static PyObject *Plus_RaceList(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_StartPos(args, races[1].pos, races[1].n);
}

static PyObject *Plus_GetDealType(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return PyString_FromString("Race");
}

/* EndOfRace(ItemName, VehicleName): Ficus on the end of the race (the police, a win, a defeat), his phone cab */
static PyObject *Plus_EndOfRace(PyObject *self, PyObject *args) {
	static const char *const police[] = { "Alarm", "Killed", "CounterMeasure", NULL };
	PyObject *item, *vehicle;
	SF_ARGS("OO", &item, &vehicle);
	int stop = Sf_InS(&f, vehicle, police);
	if (stop > 0) {
		Dt_Com(&f, "Mission_WinRaceMultiplus_RemoteMessage1", "Ficus", NULL, NULL);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MultiplusRace", "Alarm");
	} else if (stop == 0) {
		int won = Sf_Compare(&f, vehicle, "==", Sf_Attr(&f, USR(), "Name"));
		if (won > 0) {
			Dt_Com(&f, "Mission_WinRaceMultiplus_RemoteMessage3", "Ficus", G(MR, "PreKillStupidus"), NOARGS);
		} else if (won == 0) {
			Dt_Com(&f, "Mission_WinRaceMultiplus_RemoteMessage2", "Ficus", NULL, NULL);
			int left = Sf_EqS(&f, vehicle, "Left");
			if (left >= 0) {
				Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MultiplusRace", left ? "Left" : "Loose");
			}
		}
	}
	Sf_Later(&f, Sf_Float(&f, 0.5), G(MR, "SetPhoneCab"), NOARGS, NULL);
	return Sf_None(&f);
}

static PyObject *Plus_SetPhoneCab(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *pc = Sf_Ask(&f, "PhoneCab", "GetFree", "()");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, pc, "Name"));
	Sf_SetAttrS(&f, pc, "UseFunc", "Mission_MultiplusRace.UsePhoneCab");
	return Sf_None(&f);
}

/* UsePhoneCab(UsrName, CharName, optId, FullOption): Ficus by the end of the race: the race again or the hunt of
 * Stupidus */
static PyObject *Plus_UsePhoneCab(PyObject *self, PyObject *args) {
	static const char *const steps[][3] = {
		{ "Alarm", "Mission_WinRaceMultiplus_ComFicus_InitMsg1", "Mission_WinRaceMultiplus_ComFicus_InitAns1" },
		{ "Left", "Mission_WinRaceMultiplus_ComFicus_InitMsg2", "Mission_WinRaceMultiplus_ComFicus_InitAns2" },
		{ "Loose", "Mission_WinRaceMultiplus_ComFicus_InitMsg3", "Mission_WinRaceMultiplus_ComFicus_InitAns3" },
		{ "KillStupidus", "Mission_WinRaceMultiplus_ComFicus_InitMsg4", "Mission_WinRaceMultiplus_ComFicus_InitAns4" },
	};
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	int again = 0;
	Sf_SetMod(&f, "Speech", "NPCName", ch);
	Sf_Run(&f, "PhoneCab", "UseAudioCom", "(OOOO)", usr, ch, opt, full);
	PyObject *pc = Sf_Entity(&f, ch);
	int alarm = Dt_Alarm(&f);
	if (alarm > 0) {
		Sf_Run(&f, "Speech", "OnObjSpch", "(Osss)", pc, "Ficus", "Mission_WinRaceMultiplus_ComFicus_InitMsg1", "Mission_WinRaceMultiplus_RemoteMessage1");
	} else if (alarm == 0) {
		int i = 0, s = 0;
		for (; i < 4 && (s = Sf_EqS(&f, SAVE("MultiplusRace"), steps[i][0])) == 0; i++) {
		}
		if (s > 0) {
			Sf_Run(&f, "Speech", "OnObjSpch", "(Osss)", pc, "Ficus", steps[i][1], steps[i][2]);
			if (i < 3) {
				again = 1;
			} else {
				Sf_SpeechEnd(&f, G(MR, "PreInitKill"), NULL);
			}
		} else if (s == 0) {
			Sf_Run(&f, "PhoneCab", "DefUseAudioCom", "(OOOO)", usr, ch, opt, full);
		}
		if (again && !f.failed) {
			Sf_SpeechEnd(&f, G(MR, "InitRace"), NULL);
			Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MultiplusRace", "Begin");
		}
	}
	return Sf_None(&f);
}

/* InitRace(): the race against the police with the callbacks of the mission */
static PyObject *Plus_InitRace(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Challenge.Money", "0");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Race.Profile", G(MR, "RACE_PROFILE"));
	Sf_SetMod(&f, "RaceMaker", "GetDealType", G(MR, "GetDealType"));
	Sf_SetMod(&f, "RaceMaker", "EndOfRaceCallback", G(MR, "EndOfRace"));
	Sf_SetMod(&f, "RaceMaker", "OnInitialTaunt", G(MR, "AfterInitRaceTaunt"));
	PyObject *fn = Sf_Mod(&f, "RaceMaker", "InitRace");
	PyObject *onShip = Sf_Method(&f, usr, "IsType", "(s)", "Car");
	PyObject *kw = Sf_Build(&f, "{sisisO}", "numply", 5, "waypointlimit", 15, "userOnShip", onShip);
	Sf_CallKw(&f, fn, f.failed ? NULL : Py_BuildValue("(s)", "Police"), kw);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Mod(&f, "RaceMaker", "ChallengerName"));
	Sf_SetGlobal(&f, MR, "outOfRange", Sf_Mod(&f, "Speech", "OnOutOfRange"));
	Sf_SetMod(&f, "Speech", "OnOutOfRange", Py_None);
	return Sf_None(&f);
}

static PyObject *Plus_AfterInitRaceTaunt(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *oor = G(MR, "outOfRange");
	if (Sf_True(&f, oor) > 0) {
		Sf_Call(&f, oor, "()");
		Sf_Run(&f, "Scrap", "Set", "(sd)", "RemainTalkTime", 0.0);
	}
	return Sf_None(&f);
}

/* InitKill(): the scorer, Stupidus, the warning when the player leaves his ship */
static PyObject *Plus_InitKill(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Dt_ScorerCall(&f, G(MR, "MissionScorer"));
	Sf_DrawScorer(&f);
	Sf_Own(&f, MR, "SpawnStupidus", "()");
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", G(MR, "HeyYouFight"));
	return Sf_None(&f);
}

/* a functionary of Stupidus' gang: NormalTraffic at a free spawn point with the profile of the mission */
static PyObject *Mr_Functionary(scriptFrame_t *f, const char *global) {
	PyObject *e = Lv_SpawnCar(f, "NormalTraffic", 0);
	if (global) {
		Sf_SetGlobal(f, MR, global, e);
		e = Sf_Global(f, MR, global);
	}
	PyObject *pd = Sf_Own(f, MR, "GetFunctionaryProfile", "()");
	Sf_Run(f, "VehiclesNPC", "SetStdProperties", "(OO)", Sf_Attr(f, e, "Name"), Sf_Attr(f, pd, "Ship"));
	Sf_Run(f, "VehiclesNPC", "ModAIProff", "(OOi)", e, pd, 1);
	PyObject *life = Sf_Global(f, MR, "FUNC_LIFE");
	Sf_SetAttr(f, e, "Life", life);
	Sf_SetAttr(f, e, "MaxLife", life);
	return e;
}

/* SpawnStupidus(): Stupidus again; after his first life he brings messages and, from the fourth, friends */
static PyObject *Plus_SpawnStupidus(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Speech");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	PyObject *st = Mr_Functionary(&f, "Stupidus");
	Sf_SetAttrS(&f, st, "OnDeath", "Mission_MultiplusRace.StupidusOnDeath");
	Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, st, "Name"), 0);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, st, "Name"));
	PyObject *lives = Sf_ToInt(&f, SAVE("MultiplusRace.Lives"));
	PyObject *total = G(MR, "TOTAL_STUPIDUS_LIVES");
	int first = Sf_Compare(&f, lives, "==", total);
	if (first > 0) {
		Sf_SetAttrS(&f, st, "OnDamage", "Mission_MultiplusRace.StupidusDamage");
		Sf_SetAttrS(&f, st, "TgType", "red");
	} else if (first == 0) {
		Sf_SetAttrS(&f, st, "EntityList", "Enemies");
		int s = Sf_Compare(&f, lives, "==", Sf_OpI(&f, G(MR, "TOTAL_STUPIDUS_LIVES"), '-', 1));
		if (s > 0) {
			Dt_Com(&f, "Mission_KillStupidus_RemoteMessage2", "Stupidus", NULL, NULL);
		} else if (s == 0 && (s = Sf_Compare(&f, lives, "==", Sf_OpI(&f, G(MR, "TOTAL_STUPIDUS_LIVES"), '-', 2))) > 0) {
			Dt_Com(&f, "Mission_KillStupidus_RemoteMessage3", "Stupidus", NULL, NULL);
		} else if (s == 0 && Sf_Compare(&f, lives, "<=", Sf_OpI(&f, G(MR, "TOTAL_STUPIDUS_LIVES"), '-', 3)) > 0) {
			if (Sf_Compare(&f, lives, "==", Sf_OpI(&f, G(MR, "TOTAL_STUPIDUS_LIVES"), '-', 3)) > 0) {
				Dt_Com(&f, "Mission_KillStupidus_RemoteMessage4", "Stupidus", NULL, NULL);
			}
			if (Sf_EqI(&f, Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Enemies", "Num"), 1) > 0) {
				Sf_Own(&f, MR, "SpawnStupidusFriends", "(i)", 2);
			}
		}
	}
	return Sf_None(&f);
}

/* SpawnStupidusFriends(numFriends): enemy functionaries beside Stupidus */
static PyObject *Plus_SpawnStupidusFriends(PyObject *self, PyObject *args) {
	PyObject *num;
	SF_ARGS("O", &num);
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	long n = 0;
	Sf_Check(&f, Script_Range(num, &n));
	for (long i = 0; i < n && !f.failed; i++) {
		int mark = f.n;
		PyObject *e = Mr_Functionary(&f, NULL);
		Sf_SetAttrS(&f, e, "OnDeath", "Mission_MultiplusRace.StupidusFriendOnDeath");
		Sf_SetAttrS(&f, e, "EntityList", "Enemies");
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

static PyObject *Plus_StupidusFriendOnDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Later(&f, Sf_Float(&f, 5.0), G(MR, "SpawnStupidusFriends"), Sf_Build(&f, "(i)", 1), NULL);
	return Sf_None(&f);
}

/* RestoreEnemies(): the enemies back to the traffic, Ficus ends the mission */
static PyObject *Plus_RestoreEnemies(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *name = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "Enemies", "First");
	PyObject *e = Sf_Entity(&f, name);
	int mark = f.n;
	while (Sf_True(&f, e) > 0) {
		Sf_SetAttrS(&f, e, "OnDeath", "Vehicles.OnDeath");
		name = Sf_Attr(&f, e, "NextInList");
		Sf_SetAttrS(&f, e, "EntityList", "Traffic");
		Sf_SetAttrS(&f, e, "TgType", "none");
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, e, "Name"), 1);
		e = Sf_Entity(&f, name);
		PyObject *keep[2] = { name, e };
		Sf_ReleaseKeep(&f, mark, keep, 2);
	}
	Dt_Com(&f, "Mission_KillStupidus_RemoteMessage6", "Ficus", G(MR, "SetNextMission"), NOARGS);
	return Sf_None(&f);
}

/* `int(Scrap.GetSaveVar("MultiplusRace.Lives")) + 1`: the lives shown */
static PyObject *Mr_Lives(scriptFrame_t *f) {
	return Sf_Repr(f, Sf_OpI(f, Sf_ToInt(f, Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", "MultiplusRace.Lives")), '+', 1));
}

/* StupidusOnDeath(Looser, Winner): a life fewer and Stupidus again 5 s later, or the end of the gang */
static PyObject *Plus_StupidusOnDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	PyObject *lives = Sf_OpI(&f, Sf_ToInt(&f, SAVE("MultiplusRace.Lives")), '-', 1);
	int over = Sf_Compare(&f, lives, "<", Sf_Int(&f, 0));
	if (over > 0) {
		Sf_Later(&f, Sf_Float(&f, 0.5), G(MR, "RestoreEnemies"), NOARGS, NULL);
		Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
		Dt_ScorerCall(&f, Py_None);
		Sf_DrawScorer(&f);
	} else if (over == 0) {
		Sf_Later(&f, Sf_Float(&f, 5.0), G(MR, "SpawnStupidus"), NOARGS, NULL);
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "MultiplusRace.Lives", Sf_Repr(&f, lives));
		Sf_Run(&f, "SScorer", "Set", "(issO)", 0, "RemainingLivesScorer", "Text", Mr_Lives(&f));
	}
	return Sf_None(&f);
}

/* MissionScorer(id): Stupidus' lives left */
static PyObject *Plus_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Dt_Text(&f, id, "RemainingLives", Dt_Y(&f, 0), Dt_Label(&f, "Mission_KillStupidus_Scorer_1"), NULL);
	Dt_Text(&f, id, "RemainingLivesScorer", Dt_Y(&f, 20), Mr_Lives(&f), NULL);
	return Sf_None(&f);
}

/* SetNextMission(): the places of the characters, the next mission of NextMissionData, the crazy gambler 2 s later */
static PyObject *Plus_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Police", "Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "Police", NULL };
	SF_ARGS("");
	Sf_Import(&f, "Init");
	Lv_Saves(&f, places);
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "MultiplusRace");
	PyObject *apply = Sf_Mod(&f, "__builtin__", "apply");
	Sf_Call(&f, apply, "(OO)", Sf_Mod(&f, "MissionsFuncs", "SetNextMission"), G(MR, "NextMissionData"));
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToFicus3_MissionText1", 1);
	Sf_Later(&f, Sf_Int(&f, 2), G(MR, "CGCalls"), NOARGS, NULL);
	return Sf_None(&f);
}

/* CGCalls() of the missions: the crazy gambler's message by the number of crazy deals done (none during the twelfth) */
static PyObject *Dt_CGCalls(PyObject *args, const char *const *keys, int n) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (Sf_Compare(&f, SAVE("SuperDeal.Library"), "!=", Sf_Str(&f, "SuperDeal_Twelfth")) > 0) {
		PyObject *num = Sf_Ask(&f, "MissionsFuncs", "GetNumCrazyDealCompleted", "()");
		for (int i = 0; i < n; i++) {
			int s = Sf_EqI(&f, num, i);
			if (s != 0) {
				if (s > 0) {
					Dt_Com(&f, keys[i], "CrazyGambler", NULL, NULL);
				}
				break;
			}
		}
	}
	return Sf_None(&f);
}

static PyObject *Plus_CGCalls(PyObject *self, PyObject *args) {
	static const char *const keys[] = { "Mission_TalkToFicus3_RemoteMessage4", "Mission_TalkToFicus3_RemoteMessage5",
		"Mission_TalkToFicus3_RemoteMessage6", "Mission_TalkToFicus3_RemoteMessage7" };
	(void)self;
	return Dt_CGCalls(args, keys, 4);
}

PyMethodDef g_DownTownMultiplusRaceMethods[] = {
	{ "GetFunctionaryProfile", Plus_GetFunctionaryProfile, 1, NULL },
	{ "InitStatus", Plus_InitStatus, 1, NULL },
	{ "Init", Plus_Init, 1, NULL },
	{ "RaceList", Plus_RaceList, 1, NULL },
	{ "GetDealType", Plus_GetDealType, 1, NULL },
	{ "EndOfRace", Plus_EndOfRace, 1, NULL },
	{ "SetPhoneCab", Plus_SetPhoneCab, 1, NULL },
	{ "UsePhoneCab", Plus_UsePhoneCab, 1, NULL },
	{ "InitRace", Plus_InitRace, 1, NULL },
	{ "AfterInitRaceTaunt", Plus_AfterInitRaceTaunt, 1, NULL },
	{ "InitKill", Plus_InitKill, 1, NULL },
	{ "SpawnStupidus", Plus_SpawnStupidus, 1, NULL },
	{ "SpawnStupidusFriends", Plus_SpawnStupidusFriends, 1, NULL },
	{ "StupidusFriendOnDeath", Plus_StupidusFriendOnDeath, 1, NULL },
	{ "RestoreEnemies", Plus_RestoreEnemies, 1, NULL },
	{ "StupidusOnDeath", Plus_StupidusOnDeath, 1, NULL },
	{ "MissionScorer", Plus_MissionScorer, 1, NULL },
	{ "SetNextMission", Plus_SetNextMission, 1, NULL },
	{ "CGCalls", Plus_CGCalls, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ProtectMayorTransport: the mayor's transport through the mercenaries, then the mercenaries ---- */

#define KW(fn) (PyCFunction)(void (*)(void))fn, METH_VARARGS | METH_KEYWORDS

static const char *const merc_profile[] = {
	"StupidPathFinding=0", "ControlStrafe=2", "ControlBrake=0", "GoNextItemRace=1", "ControlHookRace=0", "CadenceShootCM=5", "UsePercSonic=80",
	"UsePercCloud=10", "UsePercElectro=10", "UsePercHook=15", "FactorHookMap=6", "CadenceShoot=12", "ControlAim=7", "AngAttack=1.2",
	"CounterAttack=0.75", "ControlBoost=1", "ControlBoostItemLife=1", "ControlBoostItemAmmo=1", "FactorDamageGetAway=0.50",
	"FactorDamageItemLife=0.25", "FactorItemAmmo=0.1", "ControlStrafeEvasion=1", "UsePercHookGetAway=10", "TimeMaxStrafe=6", "VelMaxStrafe=0.75",
	NULL,
};

/* GetMercEnemiesProfile(TypeShip) */
static PyObject *Escort_GetMercEnemiesProfile(PyObject *self, PyObject *args) {
	PyObject *ship;
	SF_ARGS("O", &ship);
	Sf_Import(&f, "VehiclesNPC");
	PyObject *sp = Sf_Ask(&f, "VehiclesNPC", "cShipAI", "()");
	Sf_SetAttr(&f, sp, "Ship", ship);
	Sf_SetAttrs(&f, sp, PM, merc_profile);
	return Sf_Return(&f, sp);
}

/* InitStatus(LevelPath): no hired mercenaries, the traffic of the mission */
static PyObject *Escort_InitStatus(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Import(&f, "VehiclesNPC");
	Sf_SetMod(&f, "VehiclesNPC", "UseMercs", Sf_Int(&f, 0));
	Lv_AcShips(&f, PM, "[\"SBanker1\",\"SBishop1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\",\"SBankMaster1\",\"SArchbishop1\"]");
	return Sf_None(&f);
}

/* Init(LevelPath): the taunts of the mercenaries, Rusty; the escort or the hunt by the step of the mission */
static PyObject *Escort_Init(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Own(&f, PM, "MercTauntMessager", "()");
	Sf_Run(&f, "SScorer", "AddModel", "(isssii)", 0, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3, 25);
	int s = Sf_EqS(&f, SAVE("CrazyMayor"), "Transport");
	if (s > 0) {
		Sf_Own(&f, PM, "MissionInit", "()");
	} else if (s == 0 && Sf_EqS(&f, SAVE("CrazyMayor"), "KillThem") > 0) {
		Sf_Own(&f, PM, "KillThem", "()");
	}
	return Sf_None(&f);
}

/* Scrap.CreateEntity(name, pos[0], pos[1], pos[2], "Item") with pos a global of the mission */
static PyObject *Pt_Item(scriptFrame_t *f, const char *name, const char *pos) {
	PyObject *fn = Sf_Mod(f, "Scrap", "CreateEntity");
	PyObject *x = Sf_Item(f, Sf_Global(f, PM, pos), 0), *y = Sf_Item(f, Sf_Global(f, PM, pos), 1), *z = Sf_Item(f, Sf_Global(f, PM, pos), 2);
	return Sf_Call(f, fn, "(sOOOs)", name, x, y, z, "Item");
}

/* MissionInit(): the meeting point where the escort starts, the mayor's call */
static PyObject *Escort_MissionInit(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *l = Pt_Item(&f, "Winit", "MissionBegin");
	Sf_Method(&f, l, "SetModel", "(s)", "models/Misc/MeetingPoint/MeetingPoint.m3d");
	Sf_SetAttrI(&f, l, "RadarShow", 1);
	Sf_SetAttrI(&f, l, "Radius", 3500);
	Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(iii)", 0, 180, 0));
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	Sf_SetAttrS(&f, l, "OnTake", "Mission_ProtectMayorTransport.OnMissionStart");
	PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Float(&f, 4.0));
	Sf_Schedule(&f, t, Sf_Mod(&f, "Speech", "RemoteComunication"), Sf_Build(&f, "(ss)", "Mission_ProtectMayorTransport_RemoteMessage1", "Mayor"),
		NULL);
	return Sf_None(&f);
}

/* OnMissionStart(ItemName, VehicleName): the player at the meeting point (not with the alarm on): the transport and
 * the scene of the start */
static PyObject *Escort_OnMissionStart(PyObject *self, PyObject *args) {
	PyObject *item, *vehicle;
	SF_ARGS("OO", &item, &vehicle);
	Sf_Import(&f, "Scene");
	Sf_Import(&f, "Chars");
	PyObject *usr = USR();
	if (Sf_Compare(&f, vehicle, "!=", Sf_Attr(&f, usr, "Name")) > 0) {
		return Sf_None(&f);
	}
	PyObject *itm = Sf_Entity(&f, item);
	Sf_SetAttr(&f, usr, "Pos", Sf_Attr(&f, itm, "Pos"));
	Sf_SetAttr(&f, usr, "Ang", Sf_Attr(&f, itm, "Ang"));
	Sf_SetAttr(&f, usr, "Vel", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_SetAttrI(&f, itm, "rTime", 15);
	if (Dt_Alarm(&f) > 0) {
		Sf_Run(&f, "Speech", "RemoteComunication", "(ss)", "Mission_ProtectMayorTransport_RemoteMessage2", "Mayor");
		return Sf_None(&f);
	}
	Sf_Method(&f, itm, "Del", "()");
	Sf_Run(&f, "Scene", "InitOutDoorScene", "()");
	Sf_Run(&f, "Traffic", "SetPath", "()");
	Sf_SetGlobal(&f, PM, "trans", Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ssii)", "TLight2", "Transporter", 0, 0));
	Sf_Run(&f, "Traffic", "ResetPath", "()");
	Sf_SetAttr(&f, G(PM, "trans"), "Pos", G(PM, "RouteBegin"));
	Sf_SetAttr(&f, G(PM, "trans"), "Ang", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_SetAttrS(&f, G(PM, "trans"), "TgType", "green");
	PyObject *life = G(PM, "TRANSPORT_LIFE");
	Sf_SetAttr(&f, G(PM, "trans"), "Life", life);
	Sf_SetAttr(&f, G(PM, "trans"), "MaxLife", life);
	Sf_SetAttrS(&f, G(PM, "trans"), "OnDeath", "Mission_ProtectMayorTransport.FailMission");
	Sf_SetAttrS(&f, G(PM, "trans"), "OnDamage", "Mission_ProtectMayorTransport.ActualizeScorer");
	Sf_SetAttr(&f, G(PM, "trans"), "Descriptor", Sf_Lang(&f, Sf_Str(&f, "Mission_ProtectMayorTransport_Scorer1")));
	Sf_Run(&f, "Scrap", "Set", "(sO)", "CurrentWaypointName", Sf_Attr(&f, G(PM, "trans"), "Name"));
	Sf_Own(&f, PM, "TransGoto", "(O)", G(PM, "RouteEnd"));
	Sf_Own(&f, PM, "OnMissionStart2", "()");
	return Sf_None(&f);
}

/* a shot of the scene: the camera at dist, rot (and ang unless noAng) turning by turn around the global target, the
 * voices stopped, the text of the shot for secs, the function of the next shot */
static byte Pt_Shot(scriptFrame_t *f, long dist, long rot, long ang, int noAng, long turn, const char *target, const char *text, long secs,
	const char *next) {
	int mark = f->n;
	PyObject *cam = Sf_Entity(f, Sf_Ask(f, "Scrap", "GetCam", "(i)", 0));
	Sf_SetAttrI(f, cam, "CamDist", dist);
	Sf_SetAttrI(f, cam, "CamRot", rot);
	if (!noAng) {
		Sf_SetAttrI(f, cam, "CamAng", ang);
	}
	Sf_SetAttrI(f, cam, "AddTurn", turn);
	Sf_SetAttr(f, cam, "Target", Sf_Attr(f, Sf_Global(f, PM, target), "Name"));
	for (int i = 0; i < 3; i++) {
		Sf_Run(f, "SSound", "StopVoice", "(i)", i);
	}
	Sf_Run(f, "Scene", "SetText", "(sl)", text, secs);
	Sf_Run(f, "SScorer", "SetSpeechCallback", "(s)", next);
	Sf_Release(f, mark);
	return !f->failed;
}

/* OnMissionStart2(): the camera on the transport, the player's death ends the game during the escort */
static PyObject *Escort_OnMissionStart2(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Scene");
	Sf_Import(&f, "Chars");
	Pt_Shot(&f, 4000, 0, 0, 0, 90 + 45, "trans", "Mission_Scene_ProtectMayorTransport_1", 8, "Mission_ProtectMayorTransport.AppearsBadGuys");
	Sf_SetMod(&f, "Chars", "OnMainCharDeath", G(PM, "OnPlayerDeathScorting"));
	Sf_Run(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "Mission_ProtectMayorTransport.AbortConversation");
	return Sf_None(&f);
}

/* TransGoto(Pos): the transport flies to Pos */
static PyObject *Escort_TransGoto(PyObject *self, PyObject *args) {
	PyObject *pos;
	SF_ARGS("O", &pos);
	PyObject *fn = Sf_Mod(&f, "SAI", "SetStateVehicle");
	Sf_Call(&f, fn, "(iOOOOiiddd)", 1, Sf_Attr(&f, G(PM, "trans"), "Name"), Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), 3000, 0,
		1.0, 2.0, 0.025);
	Sf_SetAttrI(&f, G(PM, "trans"), "Traf_Active", 1);
	Sf_SetAttr(&f, G(PM, "trans"), "Traf_Begin", G(PM, "RouteBegin"));
	Sf_SetAttr(&f, G(PM, "trans"), "Traf_End", G(PM, "RouteBegin"));
	Sf_SetAttrI(&f, G(PM, "trans"), "Traf_Speed", 100 * 28);
	return Sf_None(&f);
}

/* CreateBadGuy(idnum): a mercenary (Bad1 or Bad2) after the transport, or an enemy of the player without it */
static PyObject *Escort_CreateBadGuy(PyObject *self, PyObject *args) {
	PyObject *idnum;
	SF_ARGS("O", &idnum);
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "OutMap");
	Sf_SetGlobal(&f, PM, "BornMercTaunt", Sf_OpI(&f, G(PM, "BornMercTaunt"), '+', 1));
	if (Sf_Compare(&f, G(PM, "BornMercTaunt"), ">", Sf_Int(&f, 2)) > 0) {
		Sf_SetGlobal(&f, PM, "BornMercTaunt", Sf_Int(&f, 0));
	}
	int first = Sf_EqI(&f, idnum, 0);
	if (first < 0) {
		return Sf_None(&f);
	}
	const char *bad = first ? "Bad1" : "Bad2";
	Sf_SetGlobal(&f, PM, bad, Lv_SpawnCar(&f, first ? "Bad1_Fighter" : "Bad2_Fighter", 0));
	PyObject *pd = Sf_Own(&f, PM, "GetMercEnemiesProfile", "(s)", first ? "SMerc2" : "SMerc1");
	Sf_Run(&f, "VehiclesNPC", "SetStdProperties", "(OO)", Sf_Attr(&f, G(PM, bad), "Name"), Sf_Attr(&f, pd, "Ship"));
	Sf_Run(&f, "VehiclesNPC", "ModAIProff", "(OO)", G(PM, bad), pd);
	if (Sf_Compare(&f, G(PM, "BornMercTaunt"), "<", Sf_Int(&f, 0)) > 0) {
		PyObject *at = first ? Sf_Build(&f, "(ddd)", -383020.28125, -63158.125, 337138.46875) :
			Sf_Build(&f, "(ddd)", -388646.28125, -64070.9414063, 342182.59375);
		Sf_SetAttr(&f, G(PM, bad), "Pos", at);
	}
	PyObject *who = G(PM, bad);
	Sf_SetAttr(&f, who, "Ang", Sf_Build(&f, "(iii)", 0, -75, 0));
	Sf_SetAttrS(&f, who, "TgType", "red");
	Sf_SetAttrI(&f, who, "InCtrl_Acc", 1);
	PyObject *life = G(PM, "MERCS_LIFE");
	Sf_SetAttr(&f, who, "Life", life);
	Sf_SetAttr(&f, who, "MaxLife", life);
	int t = Sf_True(&f, G(PM, "trans"));
	if (t > 0) {
		Sf_SetAttrS(&f, who, "EntityList", "MadGuys");
		Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "MadGuys", "OnDamage", "Mission_ProtectMayorTransport.MadGuyDamage");
		Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Attr(&f, who, "Name"), Sf_Attr(&f, G(PM, "trans"), "Name"));
	} else if (t == 0) {
		Sf_SetAttrS(&f, who, "EntityList", "Enemies");
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", who, 1);
	}
	return Sf_None(&f);
}

/* AppearsBadGuys(): the lists of the mercenaries, the two of them standing still for the scene */
static PyObject *Escort_AppearsBadGuys(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Scene");
	Sf_Run(&f, "Scrap", "CreateEntityList", "(s)", "MadGuys");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "MadGuys", "OnDeath", "Mission_ProtectMayorTransport.MadGuyDeath");
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "Mission_ProtectMayorTransport.MadGuyDeath");
	Sf_Own(&f, PM, "CreateBadGuy", "(i)", 0);
	Sf_Own(&f, PM, "CreateBadGuy", "(i)", 1);
	Sf_Run(&f, "SAI", "SetRotStaticObj", "(Odd)", Sf_Attr(&f, G(PM, "Bad1"), "Name"), 3.0, 2.0);
	Sf_Run(&f, "SAI", "SetRotStaticObj", "(Odd)", Sf_Attr(&f, G(PM, "Bad2"), "Name"), 3.0, 2.0);
	Sf_Later(&f, Sf_Float(&f, 0.25), G(PM, "AppearsBadGuys2"), NOARGS, NULL);
	return Sf_None(&f);
}

static PyObject *Escort_AppearsBadGuys2(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Scene");
	Pt_Shot(&f, 1600, 0, 0, 1, 30, "Bad2", "Mission_Scene_ProtectMayorTransport_2", 4, "Mission_ProtectMayorTransport.NextBadGuy");
	return Sf_None(&f);
}

static PyObject *Escort_NextBadGuy(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Scene");
	Pt_Shot(&f, 3500, 0, 0, 0, 200, "Bad1", "Mission_Scene_ProtectMayorTransport_3", 5, "Mission_ProtectMayorTransport.BackToTheTransport");
	return Sf_None(&f);
}

static PyObject *Escort_BackToTheTransport(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Scene");
	Pt_Shot(&f, 3000, +90, -90, 0, 0, "trans", "Mission_Scene_ProtectMayorTransport_4", 7, "Mission_ProtectMayorTransport.BackToPlayer");
	return Sf_None(&f);
}

/* BackToPlayer(): the end of the scene: the scorer, the mercenaries turn fast again, the transport to its target */
static PyObject *Escort_BackToPlayer(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Scene");
	for (int i = 0; i < 3; i++) {
		Sf_Run(&f, "SSound", "StopVoice", "(i)", i);
	}
	Dt_ScorerCall(&f, G(PM, "MissionScorer"));
	Sf_Run(&f, "Scene", "EndOutDoorScene", "()");
	Sf_Run(&f, "Scrap", "Set", "(ss)", "EscapeEvent", "Menu.Init");
	Sf_Run(&f, "SAI", "SetRotStaticObj", "(Odd)", Sf_Attr(&f, G(PM, "Bad1"), "Name"), 20.0, 20.0);
	Sf_Run(&f, "SAI", "SetRotStaticObj", "(Odd)", Sf_Attr(&f, G(PM, "Bad2"), "Name"), 20.0, 20.0);
	Sf_Own(&f, PM, "TransGoto", "(O)", G(PM, "RouteTarget"));
	PyObject *l = Pt_Item(&f, "Wend", "RouteTarget");
	Sf_SetAttrI(&f, l, "Radius", 4000);
	Sf_SetAttrS(&f, l, "OnTake", "Mission_ProtectMayorTransport.OnTravelComplete");
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(PM, "trans"), 1);
	Sf_Run(&f, "Speech", "RemoteComunication", "(ss)", "Mission_ProtectMayorTransport_RemoteMessage3", "Mayor");
	return Sf_None(&f);
}

/* `NumTargets` + "/" + `MaxTargets` */
static PyObject *Pt_Count(scriptFrame_t *f) {
	return Sf_Cat(f, "OsO", Sf_Repr(f, Sf_Global(f, PM, "NumTargets")), "/", Sf_Repr(f, Sf_Global(f, PM, "MaxTargets")));
}

/* MadGuyDeath(MainCharnName, Param2): without the transport a mercenary more counts; a dead mercenary comes back */
static PyObject *Escort_MadGuyDeath(PyObject *self, PyObject *args) {
	PyObject *name, *p2;
	SF_ARGS("OO", &name, &p2);
	Sf_Import(&f, "Vehicles");
	if (Sf_Compare(&f, G(PM, "trans"), "==", Py_None) > 0) {
		Sf_SetGlobal(&f, PM, "NumTargets", Sf_OpI(&f, G(PM, "NumTargets"), '+', 1));
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "CrazyMayor.KillThem", Sf_Repr(&f, G(PM, "NumTargets")));
		Sf_Run(&f, "SScorer", "Set", "(issO)", 0, "RemainingCharsScorer", "Text", Pt_Count(&f));
		if (Sf_Compare(&f, G(PM, "NumTargets"), ">=", G(PM, "MaxTargets")) > 0) {
			Dt_ScorerCall(&f, Py_None);
			Sf_DrawScorer(&f);
			Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "CrazyMayor.KillThem", Sf_Repr(&f, Sf_OpI(&f, G(PM, "MaxTargets"), '-', 1)));
			Dt_Com(&f, "Mission_KillMercs_RemoteMessage1", "Mayor", G(PM, "PreLaunchTraitorius"), NULL);
		}
		if (Sf_Compare(&f, Sf_OpI(&f, G(PM, "NumTargets"), '+', 1), ">=", G(PM, "MaxTargets")) > 0) {
			return Sf_None(&f);
		}
	}
	int one = Sf_True(&f, G(PM, "Bad1"));
	if (one > 0) {
		one = Sf_Compare(&f, name, "==", Sf_Attr(&f, G(PM, "Bad1"), "Name"));
	}
	if (one < 0) {
		return Sf_None(&f);
	}
	int t = Sf_True(&f, G(PM, "trans"));
	long delay = one ? (t > 0 ? 10 : 5) : (t > 0 ? 15 : 10);
	if (t >= 0) {
		Sf_Later(&f, Sf_Int(&f, delay), G(PM, "CreateBadGuy"), Sf_Build(&f, "(i)", one ? 0 : 1), NULL);
	}
	Sf_SetGlobal(&f, PM, one ? "Bad1" : "Bad2", Py_None);
	return Sf_None(&f);
}

static PyObject *Escort_PreLaunchTraitorius(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Later(&f, Sf_Int(&f, 3), G(PM, "TraitoriusCom"), NOARGS, NULL);
	return Sf_None(&f);
}

/* SetNextMission(): on to Mission_Ambush, the blueprints of Humphrey's ship, Rusty 10 s later */
static PyObject *Escort_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Menu");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "CrazyMayor");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "TransportState");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Dt_Next(&f, "Mission_Ambush", "Scrapyard", "Mission_Ambush1_MissionText1");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MayorAtTownHall", "1");
	Lv_SaveAppend(&f, "PressBlueprints.Ships", Lv_SaveList(&f, "PressBlueprints.Ships", PM), Sf_Str(&f, "SHump1"));
	Lv_SaveAppend(&f, "PressBlueprints.Engines", Lv_SaveList(&f, "PressBlueprints.Engines", PM), Sf_Str(&f, "MHump1"));
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MayorAtGambinDen", "1");
	Sf_Import(&f, "Scorer");
	Dt_LaterCom(&f, Sf_Float(&f, 10.0),
		Sf_Build(&f, "(ssO(ii))", "Mission_ProtectMayorTransport_RemoteMessage6", "Rusty", Sf_Mod(&f, "Scorer", "ShowMissionScorer"), 0, 2), NULL);
	return Sf_None(&f);
}

/* MadGuyDamage(Name, LUser, DamageNum): the mercenary the player hits turns on him */
static PyObject *Escort_MadGuyDamage(PyObject *self, PyObject *args) {
	PyObject *name, *luser, *damage;
	SF_ARGS("OOO", &name, &luser, &damage);
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "MadGuys", "OnDamage", "");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) > 0) {
		int one = Sf_True(&f, G(PM, "Bad1"));
		if (one > 0) {
			one = Sf_Compare(&f, Sf_Attr(&f, G(PM, "Bad1"), "Name"), "==", name);
		}
		const char *bad = one > 0 ? "Bad1" : one == 0 && Sf_True(&f, G(PM, "Bad2")) > 0 ? "Bad2" : NULL;
		if (bad) {
			Sf_SetAttrS(&f, G(PM, bad), "EntityList", "Enemies");
			Sf_Run(&f, "SAI", "SetStateVehicle", "(iOO)", 2, Sf_Attr(&f, G(PM, bad), "Name"), Sf_Attr(&f, usr, "Name"));
		}
	}
	return Sf_None(&f);
}

/* MercTauntMessager(): again 10 to 20 s later while the mission lasts */
static PyObject *Escort_MercTauntMessager(PyObject *self, PyObject *args) {
	static const char *const steps[] = { "Transport", "KillThem", NULL };
	SF_ARGS("");
	Sf_Import(&f, "Vehicles");
	if (Sf_InS(&f, SAVE("CrazyMayor"), steps) > 0) {
		PyObject *t = Sf_Time(&f);
		t = Sf_Op(&f, t, '+', Sf_Ask(&f, "Scrap", "Rand", "(ii)", 10, 20));
		Sf_Schedule(&f, t, G(PM, "MercTauntMessager"), NOARGS, NULL);
	}
	return Sf_None(&f);
}

/* OnTravelComplete(ItemName, VehicleName): the transport at its target: it lands, the reward, the hunt of the
 * mercenaries */
static PyObject *Escort_OnTravelComplete(PyObject *self, PyObject *args) {
	PyObject *item, *vehicle;
	SF_ARGS("OO", &item, &vehicle);
	int none = Sf_Compare(&f, G(PM, "trans"), "==", Py_None);
	if (none > 0) {
		PyObject *itm = Sf_Entity(&f, item);
		Sf_SetAttrI(&f, itm, "rTime", 15);
		Sf_Method(&f, itm, "Del", "()");
	} else if (none == 0 && Sf_Compare(&f, vehicle, "==", Sf_Attr(&f, G(PM, "trans"), "Name")) > 0) {
		PyObject *itm = Sf_Entity(&f, item);
		Sf_SetAttrI(&f, itm, "rTime", 15);
		Sf_SetAttr(&f, G(PM, "trans"), "MovePos", Sf_Attr(&f, itm, "Pos"));
		Sf_SetAttr(&f, G(PM, "trans"), "MoveAng", Sf_Attr(&f, itm, "Ang"));
		Sf_SetAttr(&f, G(PM, "trans"), "MoveTime", Sf_Float(&f, 2.5));
		Sf_SetAttr(&f, G(PM, "trans"), "Life", Sf_Attr(&f, G(PM, "trans"), "MaxLife"));
		Sf_SetAttrS(&f, G(PM, "trans"), "OnDeath", "Vehicles.OnDeath");
		Sf_SetAttrS(&f, G(PM, "trans"), "OnDamage", "");
		Sf_SetAttrS(&f, G(PM, "trans"), "TgType", "None");
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(PM, "trans"), 0);
		Sf_Run(&f, "Scrap", "Set", "(ss)", "CurrentWaypointName", "");
		Sf_Later(&f, Sf_Int(&f, 30), G(PM, "RestoreTransport"), Sf_Build(&f, "(O)", Sf_Attr(&f, G(PM, "trans"), "Name")), NULL);
		Sf_Method(&f, itm, "Del", "()");
		Sf_Run(&f, "Scrap", "SetMoney", "(O)", Sf_Op(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), '+', G(PM, "ScortReward")));
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "TransportState", "Saved");
		Dt_Com(&f, "Mission_ProtectMayorTransport_RemoteMessage5", "Mayor", G(PM, "InitKillBadGuys"), NULL);
	}
	return Sf_None(&f);
}

/* ActualizeScorer(Name, LUser, DamageNum): the life of the transport on its bar */
static PyObject *Escort_ActualizeScorer(PyObject *self, PyObject *args) {
	PyObject *name, *luser, *damage;
	SF_ARGS("OOO", &name, &luser, &damage);
	Sf_Run(&f, "SScorer", "Set", "(issO)", 0, "RemainingLifeScorer", "MaxValue", Sf_Attr(&f, G(PM, "trans"), "MaxLife"));
	Sf_Run(&f, "SScorer", "Set", "(issO)", 0, "RemainingLifeScorer", "Value", Sf_Attr(&f, G(PM, "trans"), "Life"));
	return Sf_None(&f);
}

/* MissionScorer(id): the life of the transport, or the count of the mercenaries */
static PyObject *Escort_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	int t = Sf_True(&f, G(PM, "trans"));
	if (t > 0) {
		Dt_Text(&f, id, "RemainingLifeText", Dt_Y(&f, 0), Dt_Label(&f, "Mission_ProtectMayorTransport_Scorer1"), Sf_Int(&f, 1));
		const char *bar = "RemainingLifeScorer";
		Sf_Run(&f, "SScorer", "Add", "(Oss)", id, bar, "Bar");
		Lv_BarDefaults(&f, id, bar);
		Sf_Run(&f, "SScorer", "Set", "(OssO)", id, bar, "MaxValue", Sf_Attr(&f, G(PM, "trans"), "MaxLife"));
		Sf_Run(&f, "SScorer", "Set", "(OssO)", id, bar, "Value", Sf_Attr(&f, G(PM, "trans"), "Life"));
		Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, bar, "Visible", 1);
	} else if (t == 0) {
		Dt_Text(&f, id, "RemainingCharsText", Dt_Y(&f, 0), Dt_Label(&f, "Mission_KillMercs_Scorer1"), Sf_Int(&f, 1));
		Dt_Text(&f, id, "RemainingCharsScorer", Dt_Y(&f, 26), Pt_Count(&f), Sf_Int(&f, 1));
	}
	return Sf_None(&f);
}

/* KillThem(): the mayor calls for the hunt of the mercenaries */
static PyObject *Escort_KillThem(PyObject *self, PyObject *args) {
	SF_ARGS("");
	int killed = Sf_EqS(&f, SAVE("TransportState"), "Killed");
	if (killed >= 0) {
		Dt_Com(&f, killed ? "Mission_KillMercs_RemoteMessage3" : "Mission_KillMercs_RemoteMessage4", "Mayor", G(PM, "InitKillThemMission"), NULL);
	}
	return Sf_None(&f);
}

/* InitKillThemMission(id=None): the mercenaries (the second only while more than one is left), the hunt */
static PyObject *Escort_InitKillThemMission(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", NULL };
	PyObject *id = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &id)) {
		return NULL;
	}
	Sf_Own(&f, PM, "CreateBadGuy", "(i)", 0);
	PyObject *n = Sf_OpI(&f, Sf_ToInt(&f, SAVE("CrazyMayor.KillThem")), '+', 1);
	if (Sf_Compare(&f, n, "<", G(PM, "MaxTargets")) > 0) {
		Sf_Own(&f, PM, "CreateBadGuy", "(i)", 1);
	}
	Sf_Own(&f, PM, "InitKillBadGuys", "()");
	return Sf_None(&f);
}

/* InitKillBadGuys(itm=None): the hunt of the mercenaries: no transport, the count from the save variable */
static PyObject *Escort_InitKillBadGuys(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "itm", NULL };
	static const char *const bads[] = { "Bad1", "Bad2" };
	PyObject *itm = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &itm)) {
		return NULL;
	}
	Sf_Import(&f, "Chars");
	Sf_SetMod(&f, "Chars", "OnMainCharDeath", Py_None);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "CrazyMayor", "KillThem");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_KillMercs_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_SetGlobal(&f, PM, "NumTargets", Sf_ToInt(&f, SAVE("CrazyMayor.KillThem")));
	Sf_SetGlobal(&f, PM, "trans", Py_None);
	Dt_ScorerCall(&f, G(PM, "MissionScorer"));
	Sf_DrawScorer(&f);
	Sf_Run(&f, "Scrap", "EntityListSet", "(sss)", "Enemies", "OnDeath", "Mission_ProtectMayorTransport.MadGuyDeath");
	for (int i = 0; i < 2; i++) {
		if (Sf_True(&f, G(PM, bads[i])) > 0) {
			Sf_SetAttrS(&f, G(PM, bads[i]), "EntityList", "Enemies");
			Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(PM, bads[i]), 1);
		}
	}
	return Sf_None(&f);
}

static PyObject *Escort_RestoreTransport(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	PyObject *v = Sf_Entity(&f, name);
	if (Sf_True(&f, v) > 0) {
		Sf_SetAttrI(&f, v, "MoveTime", 0);
	}
	return Sf_None(&f);
}

PyMethodDef g_DownTownProtectMayorTransportMethods[] = {
	{ "GetMercEnemiesProfile", Escort_GetMercEnemiesProfile, 1, NULL },
	{ "InitStatus", Escort_InitStatus, 1, NULL },
	{ "Init", Escort_Init, 1, NULL },
	{ "MissionInit", Escort_MissionInit, 1, NULL },
	{ "OnMissionStart", Escort_OnMissionStart, 1, NULL },
	{ "OnMissionStart2", Escort_OnMissionStart2, 1, NULL },
	{ "TransGoto", Escort_TransGoto, 1, NULL },
	{ "CreateBadGuy", Escort_CreateBadGuy, 1, NULL },
	{ "AppearsBadGuys", Escort_AppearsBadGuys, 1, NULL },
	{ "AppearsBadGuys2", Escort_AppearsBadGuys2, 1, NULL },
	{ "NextBadGuy", Escort_NextBadGuy, 1, NULL },
	{ "BackToTheTransport", Escort_BackToTheTransport, 1, NULL },
	{ "BackToPlayer", Escort_BackToPlayer, 1, NULL },
	{ "MadGuyDeath", Escort_MadGuyDeath, 1, NULL },
	{ "PreLaunchTraitorius", Escort_PreLaunchTraitorius, 1, NULL },
	{ "SetNextMission", Escort_SetNextMission, 1, NULL },
	{ "MadGuyDamage", Escort_MadGuyDamage, 1, NULL },
	{ "MercTauntMessager", Escort_MercTauntMessager, 1, NULL },
	{ "OnTravelComplete", Escort_OnTravelComplete, 1, NULL },
	{ "ActualizeScorer", Escort_ActualizeScorer, 1, NULL },
	{ "MissionScorer", Escort_MissionScorer, 1, NULL },
	{ "KillThem", Escort_KillThem, 1, NULL },
	{ "InitKillThemMission", KW(Escort_InitKillThemMission), NULL },
	{ "InitKillBadGuys", KW(Escort_InitKillBadGuys), NULL },
	{ "RestoreTransport", Escort_RestoreTransport, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_StealMayorShip: in the mayor's ship the player destroys twenty police ships, then parks it ---- */

static PyObject *Steal_InitStatus(PyObject *self, PyObject *args) {
	(void)self;
	return Lv_AcShipsStatus(args, SM, "[\"SMayor1\",\"SFunc1\",\"SBerto1\",\"SHump1\",\"SBetty1\"]");
}

/* `numCopsKilled < COPS_TO_KILL` */
static int Sm_Hunting(scriptFrame_t *f) {
	return Sf_Compare(f, Sf_Global(f, SM, "numCopsKilled"), "<", Sf_Global(f, SM, "COPS_TO_KILL"));
}

/* Init(LevelPath): the hunt of the police or the parking by the count, the mayor's ship half a second later and
 * every 5 s */
static PyObject *Steal_Init(PyObject *self, PyObject *args) {
	PATH_ARGS();
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Vehicles");
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(SM, "StartMission"));
	Sf_SetGlobal(&f, SM, "numCopsKilled", Sf_ToInt(&f, SAVE("NumCopsKilled")));
	int hunt = Sm_Hunting(&f);
	if (hunt > 0) {
		Sf_SetMod(&f, "Vehicles", "OnDeathCallback", G(SM, "OnPoliceDeath"));
		Dt_Com(&f, "Mission_TestMayorShip_RemoteMessage1", "Trurl", NULL, NULL);
	} else if (hunt == 0) {
		Dt_Com(&f, "Mission_ParkMayorShip_RemoteMessage2", "Trurl", NULL, NULL);
	}
	Sf_Later(&f, Sf_Float(&f, 0.5), G(SM, "CreateMissionShip"), NOARGS, NULL);
	Sf_Later(&f, Sf_Float(&f, 5.0), G(SM, "TestForMissionShip"), NOARGS, "TestForMissionShip");
	return Sf_None(&f);
}

/* `string.find(string.upper(v.Model), text) != -1` */
static int Sm_ModelHas(scriptFrame_t *f, PyObject *v, const char *text) {
	PyObject *model = Sf_Ask(f, "string", "upper", "(O)", Sf_Attr(f, v, "Model"));
	return Sf_Compare(f, Sf_Ask(f, "string", "find", "(Os)", model, text), "!=", Sf_Int(f, -1));
}

/* CreateMissionShip(): the player's mayor ship (not his own) is the mission ship, else a parked mayor ship instead of
 * the farthest parked one */
static PyObject *Steal_CreateMissionShip(PyObject *self, PyObject *args) {
	PyObject *ep[2] = { NULL, NULL };
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	PyObject *usr = USR();
	int t = Dt_UsrInCar(&f, usr);
	if (t > 0) {
		t = Sf_True(&f, Sf_Mod(&f, "Parking", "PlayerShip"));
	}
	if (t > 0) {
		int mayor = Sm_ModelHas(&f, usr, "MAYOR");
		if (mayor > 0) {
			mayor = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "!=", Sf_Attr(&f, Sf_Mod(&f, "Parking", "PlayerShip"), "Name"));
		}
		if (mayor > 0) {
			Sf_SetGlobal(&f, SM, "missionShipName", Sf_Attr(&f, usr, "Name"));
			int hunt = Sm_Hunting(&f);
			if (hunt > 0) {
				Sf_Own(&f, SM, "SetPoliceTargets", "()");
			} else if (hunt == 0) {
				Sf_Own(&f, SM, "SetPlatformTarget", "()");
			}
			return Sf_None(&f);
		}
	}
	Sf_Unpack(&f, Sf_Ask(&f, "Scrap", "GetFarestParked", "()"), 2, ep);
	if (Sf_Compare(&f, ep[0], "!=", Sf_Str(&f, "")) > 0) {
		PyObject *e = Sf_Entity(&f, ep[0]);
		if (Sf_True(&f, e) > 0) {
			Sf_Method(&f, e, "Del", "()");
		}
	}
	PyObject *ship = Sf_Ask(&f, "Parking", "CreateParkedShip", "(Ois)", ep[1], 0, "Mayor");
	Sf_SetGlobal(&f, SM, "missionShipName", Sf_Attr(&f, ship, "Name"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(SM, "missionShipName"));
	return Sf_None(&f);
}

/* TestForMissionShip(): the mayor's ship again when it is gone, every 5 s */
static PyObject *Steal_TestForMissionShip(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_True(&f, Sf_Entity(&f, G(SM, "missionShipName"))) == 0) {
		Sf_Own(&f, SM, "CreateMissionShip", "()");
		Dt_Com(&f, "Mission_ParkMayorShip_RemoteMessage2", "Trurl", NULL, NULL);
	}
	Sf_Later(&f, Sf_Float(&f, 5.0), G(SM, "TestForMissionShip"), NOARGS, "TestForMissionShip");
	return Sf_None(&f);
}

/* SetPlatformTarget(): the phone cab is the platform where the ship is parked */
static PyObject *Steal_SetPlatformTarget(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "PhoneCab");
	Sf_Import(&f, "Parking");
	Sf_SetGlobal(&f, SM, "pc", Sf_Ask(&f, "PhoneCab", "GetFree", "()"));
	Sf_SetAttrS(&f, G(SM, "pc"), "UseFunc", "Mission_StealMayorShip.UsePhoneCab");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(SM, "pc"), "Name"));
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", G(SM, "TestForCorrectPlatform"));
	return Sf_None(&f);
}

/* TestForCorrectPlatform(): the player leaves the ship: at the platform (70 m across) the ship stays, else Trurl
 * complains */
static PyObject *Steal_TestForCorrectPlatform(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	int t = Sf_True(&f, usr);
	if (t > 0) {
		t = Sf_True(&f, G(SM, "pc"));
	}
	if (t > 0) {
		PyObject *d = Sf_Ask(&f, "SVec", "ModSqr", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", Sf_Attr(&f, usr, "Pos"), Sf_Attr(&f, G(SM, "pc"), "Pos")));
		int far = Sf_Compare(&f, d, ">", Sf_Int(&f, 49000000));
		if (far > 0) {
			Dt_Com(&f, "Mission_ParkMayorShip_RemoteMessage1", "Trurl", NULL, NULL);
		} else if (far == 0) {
			PyObject *e = Sf_Entity(&f, G(SM, "missionShipName"));
			if (Sf_True(&f, e) > 0) {
				Sf_Import(&f, "Parking");
				Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "TestForMissionShip");
				Sf_SetAttrS(&f, e, "UseFunc", "Mission_StealMayorShip.UseParkedMayorShip");
				Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
				Dt_Com(&f, "Mission_ParkMayorShip_RemoteMessage3", "Trurl", NULL, NULL);
				Sf_Run(&f, "SaveGame", "QWayPoint", "()");
				Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_ByeByeTrurl_MissionText1", 1);
			}
		}
	}
	return Sf_None(&f);
}

/* UseParkedMayorShip(UsrName, CharName, optId, FullOption): the parked ship is not to be used any more */
static PyObject *Steal_UseParkedMayorShip(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_Import(&f, "RaceMaker");
	Sf_Run(&f, "RaceMaker", "ShowSystemMessage", "(O)", Sf_Lang(&f, Sf_Str(&f, "Mission_ParkMayorShip_SystemMessage1")));
	return Sf_None(&f);
}

/* UsePhoneCab(UsrName, CharName, optId, FullOption): Trurl's farewell and DeepThroat at the phone of the platform */
static PyObject *Steal_UsePhoneCab(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_Import(&f, "PhoneCab");
	Sf_SetMod(&f, "Speech", "NPCName", ch);
	Sf_SetMod(&f, "Speech", "PlayerName", usr);
	Sf_Run(&f, "PhoneCab", "UseAudioCom", "(OOOO)", usr, ch, opt, full);
	int alarm = Dt_Alarm(&f);
	if (alarm > 0) {
		Sf_Run(&f, "Speech", "StartObjSpch", "(Oss)", G(SM, "pc"), "Generics_StillPolice1_Messenger", "Trurl");
	} else if (alarm == 0) {
		Sf_SpeechEnd(&f, G(SM, "SetNextMission"), NULL);
		Sf_Run(&f, "Speech", "NISpeech", "(s)", "Messenger");
		Sf_Run(&f, "Speech", "AutoAddNI", "(s)", "Mission_ByeByeTrurl_ComTrurl_NI_1Msg");
		PyObject *fn = Sf_Mod(&f, "Speech", "AutoAddNI");
		Sf_CallKw(&f, fn, f.failed ? NULL : Py_BuildValue("(s)", "Mission_ByeByeTrurl_ComDeepThroat_NI_1Msg"),
			Sf_Build(&f, "{ss}", "HeadName", "DeepThroat"));
		Sf_Run(&f, "Speech", "RunNI", "()");
	}
	return Sf_None(&f);
}

/* SetNextMission(): on to Mission_ThreeStooges, the places of the characters, the crazy gambler 2 s later */
static PyObject *Steal_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "PoliceBossAtTownHall", "0", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "GamblinDen", "Journalist.Betty_Defaut", "Press", "Journalist.Berto_Defaut", "GamblinDen", NULL };
	SF_ARGS("");
	Sf_SetAttrS(&f, G(SM, "pc"), "UseFunc", "PhoneCab.DefUseAudioCom");
	Sf_SetMod(&f, "Speech", "OnOutOfRange", Py_None);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "NumCopsKilled");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Dt_Next(&f, "Mission_ThreeStooges", "GamblinDen", "Mission_FirstContactMercs_MissionText1");
	Lv_Saves(&f, places);
	Sf_Later(&f, Sf_Int(&f, 2), G(SM, "CGCalls"), NOARGS, NULL);
	return Sf_None(&f);
}

static PyObject *Steal_CGCalls(PyObject *self, PyObject *args) {
	static const char *const keys[] = { "Mission_FirstContactMercs_RemoteMessage1", "Mission_FirstContactMercs_RemoteMessage2",
		"Mission_FirstContactMercs_RemoteMessage3" };
	(void)self;
	return Dt_CGCalls(args, keys, 3);
}

/* `usr and usr.Name != missionShipName`: the player is not in the mayor's ship */
static int Sm_NotInShip(scriptFrame_t *f, PyObject *usr) {
	int t = Sf_True(f, usr);
	return t > 0 ? Sf_Compare(f, Sf_Attr(f, usr, "Name"), "!=", Sf_Global(f, SM, "missionShipName")) : t;
}

/* StartMission(): the player in the mayor's ship: the hunt of the police or the platform */
static PyObject *Steal_StartMission(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Menu", "Parking", "OutPolice", "Speech", NULL };
	SF_ARGS("");
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	if (Sm_NotInShip(&f, USR()) > 0) {
		return Sf_None(&f);
	}
	PyObject *e = Sf_Entity(&f, G(SM, "missionShipName"));
	if (Sf_True(&f, e) == 0) {
		return Sf_None(&f);
	}
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
	int hunt = Sm_Hunting(&f);
	if (hunt > 0) {
		Dt_Com(&f, "Mission_TestMayorShip_RemoteMessage5", "Trurl", NULL, NULL);
		Sf_Own(&f, SM, "SetPoliceTargets", "()");
	} else if (hunt == 0) {
		Sf_Own(&f, SM, "SetPlatformTarget", "()");
	}
	return Sf_None(&f);
}

/* ContinueMission(): back in a ship: the mission goes on in the mayor's, else Trurl complains */
static PyObject *Steal_ContinueMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	int t = Sm_NotInShip(&f, USR());
	if (t > 0) {
		Sf_Import(&f, "Menu");
		Dt_ScorerCall(&f, Py_None);
		Sf_DrawScorer(&f);
		Dt_Com(&f, "Mission_TestMayorShip_RemoteMessage3", "Trurl", NULL, NULL);
		return Sf_None(&f);
	}
	Sf_Own(&f, SM, "StartMission", "()");
	return Sf_None(&f);
}

/* SetPoliceListTargetStatus(status): the police ships of the list OutCop as targets or not */
static PyObject *Steal_SetPoliceListTargetStatus(PyObject *self, PyObject *args) {
	PyObject *status;
	SF_ARGS("O", &status);
	PyObject *name = Sf_Ask(&f, "Scrap", "EntityListGet", "(ss)", "OutCop", "First");
	int mark = f.n;
	while (Sf_True(&f, name) > 0) {
		PyObject *e = Sf_Entity(&f, name);
		if (Sf_True(&f, e) <= 0) {
			break;
		}
		Sf_Own(&f, SM, "SetPoliceShipTargetStatus", "(OO)", e, status);
		name = Sf_Attr(&f, e, "NextInList");
		Sf_ReleaseKeep(&f, mark, &name, 1);
	}
	return Sf_None(&f);
}

/* ComeBack(): the player left the mayor's ship: the ship is the target again, not the police */
static PyObject *Steal_ComeBack(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "OutPolice");
	PyObject *e = Sf_Entity(&f, G(SM, "missionShipName"));
	if (Sf_True(&f, e) > 0) {
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 1);
	}
	Sf_SetMod(&f, "OutPolice", "OnPoliceCreatedCallback", Py_None);
	Sf_Own(&f, SM, "SetPoliceListTargetStatus", "(i)", 0);
	Dt_Com(&f, "Mission_TestMayorShip_RemoteMessage2", "Trurl", NULL, NULL);
	Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", G(SM, "ContinueMission"));
	return Sf_None(&f);
}

/* `numCopsKilled` + "/" + `COPS_TO_KILL` */
static PyObject *Sm_Count(scriptFrame_t *f) {
	return Sf_Cat(f, "OsO", Sf_Repr(f, Sf_Global(f, SM, "numCopsKilled")), "/", Sf_Repr(f, Sf_Global(f, SM, "COPS_TO_KILL")));
}

/* MissionScorer(id): the police ships destroyed */
static PyObject *Steal_MissionScorer(PyObject *self, PyObject *args) {
	PyObject *id;
	SF_ARGS("O", &id);
	Sf_Import(&f, "Scorer");
	Dt_Text(&f, id, "RemainingCharsText", Dt_Y(&f, 0), Dt_Label(&f, "Mission_TestMayorShip_Scorer_1"), NULL);
	Dt_Text(&f, id, "RemainingCharsScorer", Dt_Y(&f, 20), Sm_Count(&f), NULL);
	return Sf_None(&f);
}

/* OnPoliceDeath(LooserName, WinnerName): a police ship the player destroyed in the mayor's ship counts; at twenty the
 * ship is to be parked */
static PyObject *Steal_OnPoliceDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	SF_ARGS("OO", &looser, &winner);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) == 0 || Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "!=", winner) > 0) {
		return Sf_None(&f);
	}
	if (Sm_ModelHas(&f, Sf_Entity(&f, looser), "POLI") == 0) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "!=", G(SM, "missionShipName")) > 0) {
		Dt_Com(&f, "Mission_TestMayorShip_RemoteMessage3", "Trurl", NULL, NULL);
		return Sf_None(&f);
	}
	Sf_SetGlobal(&f, SM, "numCopsKilled", Sf_OpI(&f, G(SM, "numCopsKilled"), '+', 1));
	int done = Sf_Compare(&f, G(SM, "numCopsKilled"), "==", G(SM, "COPS_TO_KILL"));
	if (done > 0) {
		static const char *const mods[] = { "Menu", "Parking", "Vehicles", "OutPolice", NULL };
		for (int i = 0; mods[i]; i++) {
			Sf_Import(&f, mods[i]);
		}
		Sf_Own(&f, SM, "SetPoliceListTargetStatus", "(i)", 0);
		Sf_SetMod(&f, "OutPolice", "OnPoliceCreatedCallback", Py_None);
		Sf_SetMod(&f, "Vehicles", "OnDeathCallback", Py_None);
		Sf_SetMod(&f, "Parking", "SwitchCharMissionCallback", Py_None);
		Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
		Dt_ScorerCall(&f, Py_None);
		Sf_DrawScorer(&f);
		Dt_Com(&f, "Mission_TestMayorShip_RemoteMessage4", "Trurl", G(SM, "ParkMayorShip"), NULL);
	} else if (done == 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumCopsKilled", Sf_Repr(&f, G(SM, "numCopsKilled")));
		Sf_Run(&f, "SScorer", "Set", "(issO)", 0, "RemainingCharsScorer", "Text", Sm_Count(&f));
	}
	return Sf_None(&f);
}

/* ParkMayorShip(): to the platform with the ship; Berto's ship parked at the press */
static PyObject *Steal_ParkMayorShip(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumCopsKilled", Sf_Repr(&f, G(SM, "COPS_TO_KILL")));
	Sf_Own(&f, SM, "SetPlatformTarget", "()");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_ParkMayorShip_MissionText1", 1);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Parked.Press", "['SBERTO1']");
	return Sf_None(&f);
}

PyMethodDef g_DownTownStealMayorShipMethods[] = {
	{ "InitStatus", Steal_InitStatus, 1, NULL },
	{ "Init", Steal_Init, 1, NULL },
	{ "CreateMissionShip", Steal_CreateMissionShip, 1, NULL },
	{ "TestForMissionShip", Steal_TestForMissionShip, 1, NULL },
	{ "SetPlatformTarget", Steal_SetPlatformTarget, 1, NULL },
	{ "TestForCorrectPlatform", Steal_TestForCorrectPlatform, 1, NULL },
	{ "UseParkedMayorShip", Steal_UseParkedMayorShip, 1, NULL },
	{ "UsePhoneCab", Steal_UsePhoneCab, 1, NULL },
	{ "SetNextMission", Steal_SetNextMission, 1, NULL },
	{ "CGCalls", Steal_CGCalls, 1, NULL },
	{ "StartMission", Steal_StartMission, 1, NULL },
	{ "ContinueMission", Steal_ContinueMission, 1, NULL },
	{ "SetPoliceListTargetStatus", Steal_SetPoliceListTargetStatus, 1, NULL },
	{ "ComeBack", Steal_ComeBack, 1, NULL },
	{ "MissionScorer", Steal_MissionScorer, 1, NULL },
	{ "OnPoliceDeath", Steal_OnPoliceDeath, 1, NULL },
	{ "ParkMayorShip", Steal_ParkMayorShip, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
