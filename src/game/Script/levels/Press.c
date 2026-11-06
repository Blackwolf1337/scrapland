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
 */
#include <stdlib.h>
#include <string.h>

#include "Levels.h"

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

#define ENT(n) Sf_Ask(&f, "Scrap", "GetEntity", "(s)", n)
#define USR() Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0)
#define LANG(k) Sf_Lang(&f, Sf_Str(&f, k))
#define SAVE(n) Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", n)
#define G(m, n) Sf_Global(&f, m, n)
#define NOARGS Sf_Build(&f, "()")

/* Speech.AddRemoteCom(msg, who[, func, params]) with func a global of module (NULL: none; params NULL: no fourth) */
static byte Pr_Remote(scriptFrame_t *f, const char *msg, const char *who, const char *module, const char *func, int params) {
	int mark = f->n;
	if (!func) {
		Sf_Run(f, "Speech", "AddRemoteCom", "(ss)", msg, who);
	} else if (params) {
		Sf_Run(f, "Speech", "AddRemoteCom", "(ssO())", msg, who, Sf_Global(f, module, func));
	} else {
		Sf_Run(f, "Speech", "AddRemoteCom", "(ssO)", msg, who, Sf_Global(f, module, func));
	}
	Sf_Release(f, mark);
	return !f->failed;
}

/* Scrap.GetEntity(name).var = text */
static byte Pr_EntS(scriptFrame_t *f, const char *name, const char *var, const char *text) {
	int mark = f->n;
	Sf_SetAttrS(f, Sf_Ask(f, "Scrap", "GetEntity", "(s)", name), var, text);
	Sf_Release(f, mark);
	return !f->failed;
}

/* e = Scrap.GetEntity(name); if e: e.Del() */
static byte Pr_DelIf(scriptFrame_t *f, PyObject *name) {
	int mark = f->n;
	PyObject *e = Sf_Entity(f, name);
	if (Sf_True(f, e) > 0) {
		Sf_Method(f, e, "Del", "()");
	}
	Sf_Release(f, mark);
	return !f->failed;
}

/* usr = Scrap.UsrEntity(0); usr.Pos = pos; usr.Ang = 0, ang, 0; then the fade from black of the scene's end */
static byte Pr_UsrBack(scriptFrame_t *f, int x, int y, int z, int ang, double fade) {
	int mark = f->n;
	PyObject *usr = Sf_Ask(f, "Scrap", "UsrEntity", "(i)", 0);
	Sf_SetAttr(f, usr, "Pos", Sf_Build(f, "(iii)", x, y, z));
	Sf_SetAttr(f, usr, "Ang", Sf_Build(f, "(iii)", 0, ang, 0));
	Lv_Fade(f, lv_black, lv_clear, fade);
	Sf_Release(f, mark);
	return !f->failed;
}

/* the gatekeeper of the boss room at its teleporter: SetDoor, DoorFoe(GateKeeper.<foe>), elv.UseFunc = use; elv */
static PyObject *Pr_BossDoor(scriptFrame_t *f, const char *foe, const char *use) {
	Sf_Run(f, "GateKeeper", "SetDoor", "(ss)", "GateKeeper_DTritusBossRoom", "DM_Door_Teleporter_08");
	Sf_Run(f, "GateKeeper", "DoorFoe", "(sO)", "GateKeeper_DTritusBossRoom", Sf_Mod(f, "GateKeeper", foe));
	PyObject *elv = Sf_Ask(f, "Scrap", "GetEntity", "(s)", "DM_Door_Teleporter_08");
	Sf_SetAttrS(f, elv, "UseFunc", use);
	return elv;
}

/* SScorer.SetMsgText(cad, Scrap.GetTime() + Speech.CalculateSpeechTime(cad)) */
static byte Pr_Msg(scriptFrame_t *f, PyObject *cad) {
	int mark = f->n;
	PyObject *fn = Sf_Mod(f, "SScorer", "SetMsgText");
	PyObject *t = Sf_Time(f);
	Sf_Call(f, fn, "(OO)", cad, Sf_Op(f, t, '+', Sf_Ask(f, "Speech", "CalculateSpeechTime", "(O)", cad)));
	Sf_Release(f, mark);
	return !f->failed;
}

/* Scrap.GetLangStr(key) % SInput.GetDefinedEntry(0, "Walk", action, "Mouse") */
static PyObject *Pr_KeyText(scriptFrame_t *f, const char *key, const char *action) {
	PyObject *t = Sf_Lang(f, Sf_Str(f, key));
	return Sf_Op(f, t, '%', Sf_Ask(f, "SInput", "GetDefinedEntry", "(isss)", 0, "Walk", action, "Mouse"));
}

/* a taken item: it.rTime = 20, it.Del in 5 s, the take sound Sound.ItemTake[key] at it */
static byte Pr_Taken(scriptFrame_t *f, PyObject *it, const char *key) {
	int mark = f->n;
	Sf_SetAttrI(f, it, "rTime", 20);
	Sf_Schedule(f, Sf_OpI(f, Sf_Time(f), '+', 5), Sf_Attr(f, it, "Del"), Sf_Build(f, "()"), NULL);
	PyObject *a = Sf_GetItem(f, Sf_Mod(f, "Sound", "ItemTake"), Sf_Str(f, key));
	Sf_Run(f, "SSound", "PlaySound", "(OOO)", Sf_Attr(f, it, "Pos"), Sf_Item(f, a, 0), Sf_Item(f, a, 1));
	Sf_Release(f, mark);
	return !f->failed;
}

/* a scene table of Sf_Calls lines; "!time|Func" is Scene.AddGeneric(time, 'import '+libname+';'+libname+'.Func()')
 * with libname the global of module */
static byte Pr_Scene(scriptFrame_t *f, const char *module, const char *const *lines) {
	for (; *lines && !f->failed; lines++) {
		if (**lines != '!') {
			const char *one[2] = { *lines, NULL };
			Sf_Calls(f, module, one);
			continue;
		}
		int mark = f->n;
		const char *bar = strchr(*lines, '|');
		PyObject *lib = Sf_Global(f, module, "libname");
		PyObject *code = Sf_Cat(f, "sOsOs", "import ", lib, ";", lib, ".");
		code = Sf_Cat(f, "Oss", code, bar + 1, "()");
		Sf_Run(f, "Scene", "AddGeneric", "(iO)", atoi(*lines + 1), code);
		Sf_Release(f, mark);
	}
	return !f->failed;
}

/* ---- Map ---- */

/* RemoteMsg(i): in the forbidden area: the label, the police warns up to three times, then the alarm */
static PyObject *PressMap_RemoteMsg(PyObject *self, PyObject *args) {
	PyObject *i;
	SF_ARGS("O", &i);
	Sf_Import(&f, "Speech");
	if (Sf_True(&f, G(MAP, "AlarmState")) <= 0) {
		return Sf_None(&f);
	}
	PyObject *time = Sf_Time(&f);
	Sf_Run(&f, "SScorer", "SetLabelText", "(OO)", LANG("Generics_ForbiddenArea_Label"), Sf_Op(&f, time, '+', Sf_Float(&f, 0.5)));
	int alarm = Sf_True(&f, Sf_Item(&f, Sf_Ask(&f, "Scrap", "GetAlarm", "()"), 0));
	if (alarm > 0) {
		Sf_Own(&f, MAP, "StartAlarmator", "()");
	} else if (alarm == 0) {
		PyObject *msg = Sf_Cat(&f, "sO", "Generics_Police_RemoteAlert", Sf_Repr(&f, Sf_OpI(&f, i, '+', 1)));
		if (Sf_True(&f, Sf_Ask(&f, "Speech", "RemoteComunication", "(Os)", msg, "Police")) > 0) {
			i = Sf_OpI(&f, i, '+', 1);
		}
		int last = Sf_Compare(&f, i, ">=", Sf_Int(&f, 3));
		if (last > 0) {
			Sf_Own(&f, MAP, "StartAlarmator", "()");
		} else if (last == 0) {
			PyObject *timy = Sf_Float(&f, 3.0);
			if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "RemainTalkTime"), "<", Sf_Float(&f, 2.8)) > 0) {
				timy = Sf_Op(&f, Sf_Float(&f, 3.0), '+', Sf_Ask(&f, "Scrap", "Get", "(s)", "RemainTalkTime"));
			}
			Sf_Schedule(&f, Sf_Op(&f, time, '+', timy), G(MAP, "RemoteMsg"), Sf_Build(&f, "(O)", i), NULL);
		}
	}
	return Sf_None(&f);
}

/* ForbiddenMsg(): the label of the forbidden area every second while the alarm area is on */
static PyObject *PressMap_ForbiddenMsg(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_True(&f, G(MAP, "AlarmState")) > 0) {
		PyObject *time = Sf_Time(&f);
		Sf_Run(&f, "SScorer", "SetLabelText", "(OO)", LANG("Generics_ForbiddenArea_Label"), Sf_Op(&f, time, '+', Sf_Float(&f, 0.5)));
		Sf_Schedule(&f, Sf_Op(&f, time, '+', Sf_Float(&f, 1.0)), G(MAP, "ForbiddenMsg"), NOARGS, NULL);
	}
	return Sf_None(&f);
}

/* StartAlarmator(): the label, and the alarm: Risitas guaranteed with the high alarm, else gears (not for a gear) */
static PyObject *PressMap_StartAlarmator(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Own(&f, MAP, "ForbiddenMsg", "()");
	int hi = Lv_SaveIs(&f, "MapPress_HiAlarm", "1");
	if (hi > 0) {
		Sf_Run(&f, "Scrap", "Set", "(ss)", "GuaranteedChars", ",Risitas,");
		Sf_Run(&f, "Scrap", "Set", "(si)", "AlarmFallDelta", 0);
		Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 1);
	} else if (hi == 0) {
		Sf_Run(&f, "Scrap", "Set", "(ss)", "GuaranteedChars", ",Gear,");
		PyObject *usr = USR();
		int on = Sf_True(&f, usr);
		if (on > 0) {
			on = Sf_EqS(&f, Sf_Attr(&f, usr, "ActCtrl"), "Gear");
			on = on < 0 ? -1 : !on;
		}
		if (on > 0) {
			Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 1);
		}
	}
	return Sf_None(&f);
}

/* for e in <global name>: e.OnTake = ""; e.Del(); then del <global name> */
static byte Pr_DelItems(scriptFrame_t *f, const char *name) {
	PyObject *list = Sf_Global(f, MAP, name);
	for (int i = 0; !f->failed; i++) {
		int mark = f->n;
		PyObject *e = Sf_Next(f, list, i);
		if (!e) {
			break;
		}
		Sf_SetAttrS(f, e, "OnTake", "");
		Sf_Method(f, e, "Del", "()");
		Sf_Release(f, mark);
	}
	return Sf_DelGlobal(f, MAP, name);
}

/* ActivateAlarm(ItemName, VehicleName): the player walks into the forbidden area: the activators go, the warnings or
 * the alarm start (at once in the first 2 s), the deactivators come */
static PyObject *PressMap_ActivateAlarm(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	PyObject *usr = Sf_Entity(&f, veh);
	if (Sf_EqI(&f, Sf_Attr(&f, usr, "UsrControl"), 0) <= 0) {
		return Sf_None(&f);
	}
	Sf_SetGlobal(&f, MAP, "AlarmState", Sf_Int(&f, 1));
	Pr_DelItems(&f, "ACTIVATORS");
	Sf_SetGlobal(&f, MAP, "PREVALARM", Sf_Ask(&f, "Scrap", "Get", "(s)", "AlarmFallDelta"));
	Sf_SetGlobal(&f, MAP, "PREVGUARANTEEDCHARS", Sf_Ask(&f, "Scrap", "Get", "(s)", "GuaranteedChars"));
	int zero = Sf_Compare(&f, Sf_Time(&f), "<", Sf_Float(&f, 2.0));
	int hi = Lv_SaveIs(&f, "MapPress_HiAlarm", "1");
	if (hi > 0) {
		if (zero > 0) {
			Sf_Own(&f, MAP, "StartAlarmator", "()");
		} else if (zero == 0) {
			Sf_Own(&f, MAP, "RemoteMsg", "(i)", 0);
		}
	} else if (hi == 0) {
		int gear = zero ? zero : Sf_EqS(&f, Sf_Attr(&f, usr, "ActCtrl"), "Gear");
		if (gear > 0) {
			Sf_Run(&f, "Scrap", "Set", "(ss)", "GuaranteedChars", ",Gear,");
			Sf_Own(&f, MAP, "ForbiddenMsg", "()");
		} else if (gear == 0) {
			Sf_Own(&f, MAP, "RemoteMsg", "(i)", 0);
		}
	}
	Sf_Own(&f, MAP, "CreateDeactivators", "()");
	return Sf_None(&f);
}

/* global list = []; an item named name for every (x, y, z, radius) of the global pos, taken by the player only */
static PyObject *Pr_Launchers(PyObject *args, const char *list, const char *posName, const char *name, const char *take) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *items = Sf_Build(&f, "[]");
	Sf_SetGlobal(&f, MAP, list, items);
	PyObject *pos = Sf_Global(&f, MAP, posName);
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *p = Sf_Next(&f, pos, i);
		if (!p) {
			break;
		}
		PyObject *e = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", name, Sf_Item(&f, p, 0), Sf_Item(&f, p, 1), Sf_Item(&f, p, 2), "Item");
		Sf_SetAttr(&f, e, "Radius", Sf_Item(&f, p, 3));
		Sf_SetAttrI(&f, e, "OnlyUser", 1);
		Sf_SetAttrS(&f, e, "OnTake", take);
		Sf_Method(&f, G(MAP, list), "append", "(O)", e);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* CreateActivators() */
static PyObject *PressMap_CreateActivators(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_Launchers(args, "ACTIVATORS", "ActivatorPos", "StartAlarm", "Map.ActivateAlarm");
}

/* CreateDeactivators() */
static PyObject *PressMap_CreateDeactivators(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_Launchers(args, "DEACTIVATORS", "DeactivatorPos", "EndAlarm", "Map.DeactivateAlarm");
}

/* DeactivateAlarm(ItemName, VehicleName): out of the area: the alarm values back, the activators again */
static PyObject *PressMap_DeactivateAlarm(PyObject *self, PyObject *args) {
	PyObject *item, *veh;
	SF_ARGS("OO", &item, &veh);
	if (Sf_EqI(&f, Sf_Attr(&f, Sf_Entity(&f, veh), "UsrControl"), 0) <= 0) {
		return Sf_None(&f);
	}
	Sf_SetGlobal(&f, MAP, "AlarmState", Sf_Int(&f, 0));
	Pr_DelItems(&f, "DEACTIVATORS");
	Sf_Run(&f, "Scrap", "Set", "(sO)", "AlarmFallDelta", G(MAP, "PREVALARM"));
	Sf_Run(&f, "Scrap", "Set", "(sO)", "GuaranteedChars", G(MAP, "PREVGUARANTEEDCHARS"));
	Sf_Own(&f, MAP, "CreateActivators", "()");
	return Sf_None(&f);
}

/* BrowseAlarmLaunchers(): the dummies of the alarm area (their radius in the name), then the activators */
static PyObject *PressMap_BrowseAlarmLaunchers(PyObject *self, PyObject *args) {
	static const char *const masks[] = { "DM_PressMap_ActiveAlarm_*", "DM_PressMap_DeactiveAlarm_*" };
	SF_ARGS("");
	Sf_Import(&f, "string");
	PyObject *lists[2];
	lists[0] = Sf_Build(&f, "[]");
	Sf_SetGlobal(&f, MAP, "ActivatorPos", lists[0]);
	lists[1] = Sf_Build(&f, "[]");
	Sf_SetGlobal(&f, MAP, "DeactivatorPos", lists[1]);
	for (int k = 0; k < 2 && !f.failed; k++) {
		Sf_Run(&f, "Scrap", "StartDummySearch", "(si)", masks[k], 1);
		while (!f.failed) {
			int mark = f.n;
			PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
			Sf_Print(&f, "O", dummy);
			if (Sf_True(&f, dummy) <= 0) {
				break;
			}
			PyObject *x = Sf_Item(&f, Sf_Item(&f, dummy, 1), 0), *y = Sf_Item(&f, Sf_Item(&f, dummy, 1), 1);
			PyObject *z = Sf_Item(&f, Sf_Item(&f, dummy, 1), 2);
			PyObject *r = Sf_ToInt(&f, Sf_Item(&f, Sf_Ask(&f, "string", "split", "(Os)", Sf_Item(&f, dummy, 0), "_"), 3));
			Sf_Method(&f, lists[k], "append", "(OOOO)", x, y, z, r);
			Sf_Release(&f, mark);
		}
	}
	Sf_Own(&f, MAP, "CreateActivators", "()");
	return Sf_None(&f);
}

/* InitStatus(LevelPath): an indoor map; the NPCs that come back (fewer in the demo), the elevators, the bureaucracy,
 * the boss' place, the characters the demo cannot convert into */
static PyObject *PressMap_InitStatus(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Init", "Elevator", "Bureaucracy", "CharsNPC", "CharConversor" };
	static const char *const demo[] = { "Nurse", "Messenger", "Mayor", "PoliceBoss", "BankMaster" };
	PyObject *path;
	SF_ARGS("O", &path);
	for (int i = 0; i < 5; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 0));
	if (Sf_True(&f, Sf_Mod(&f, "Init", "isDemo")) == 0) {
		Sf_Run(&f, "CharsNPC", "SetReposChars", "(si)", "Nurse", 3);
		int m = Lv_PcOr(&f, MAP, "_Messenger");
		if (m > 0) {
			Sf_Run(&f, "CharsNPC", "SetReposChars", "(si)", "Messenger", 2);
		} else if (m == 0) {
			Sf_Import(&f, "Messenger");
			Sf_SetMod(&f, "Messenger", "MakeMDP", Sf_Int(&f, 0));
		}
	}
	Sf_Run(&f, "CharsNPC", "SetReposChars", "(si)", "Sentinel", 3);
	Sf_Run(&f, "CharsNPC", "SetReposChars", "(si)", "Police", 5);
	Sf_Run(&f, "CharsNPC", "SetReposChars", "(si)", "Desktop", 3);
	if (Lv_PcOr(&f, MAP, "_BankDirector") > 0) {
		Sf_Run(&f, "CharsNPC", "SetReposChars", "(si)", "BankDirector", 3);
	}
	if (Lv_PcOr(&f, MAP, "_Bishop") > 0) {
		Sf_Run(&f, "CharsNPC", "SetReposChars", "(si)", "Bishop", 3);
	}
	if (Lv_PcOr(&f, MAP, "_Functionary") > 0) {
		Sf_Run(&f, "CharsNPC", "SetReposChars", "(si)", "Functionary", 3);
	}
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Metro", "DM_Door_Metro_Press");
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "MetroExt", "DM_Door_Metro_Press");
	Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Elevator", "DM_Door_Elevator_Press");
	Sf_SetMod(&f, "Bureaucracy", "BureaucracyTableFile", Sf_Str(&f, "DesktopFunctionaryPress"));
	Sf_Run(&f, "Scrap", "Set", "(ss)", "BossWorkPlace", "DM_NPC_Spawn_Boss_00");
	if (Sf_True(&f, Sf_Mod(&f, "Init", "isDemo")) > 0) {
		for (int i = 0; i < 5 && !f.failed; i++) {
			int mark = f.n;
			PyObject *vl = Sf_Str(&f, demo[i]);
			if (Sf_In(&f, vl, Sf_Mod(&f, "CharConversor", "AvailableChars")) > 0) {
				Sf_Method(&f, Sf_Mod(&f, "CharConversor", "AvailableChars"), "remove", "(O)", vl);
			}
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/* Init(LevelPath): the elevators and the metro, the boss room after the mental controller, the fake traffic, the
 * alarm area, the bureau gone when it was taken */
static PyObject *PressMap_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Metro");
	Sf_Import(&f, "Elevator");
	Sf_Import(&f, "GateKeeper");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_Press", "DownTown");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([ss][])", "DM_Door_Teleporter_07", "DM_Door_Teleporter_08");
	Sf_Run(&f, "Metro", "SetAsMetro", "(s)", "DM_Door_Metro_Press");
	if (Lv_SaveIs(&f, "Press.MentalControllerStatus", "1") > 0) {
		Pr_BossDoor(&f, "NoOneGroupGateKeeper", "GateKeeper.UseDoor");
	}
	Sf_Import(&f, "InTraffic");
	Sf_SetMod(&f, "InTraffic", "N_TrafficList", Sf_Build(&f, "[sss]", "TLight1B", "TLight2B", "TLight3B"));
	Sf_SetMod(&f, "InTraffic", "N_NumTraffic", Sf_Int(&f, 30));
	Sf_SetMod(&f, "InTraffic", "M_TrafficList", Sf_Build(&f, "[]"));
	Sf_SetMod(&f, "InTraffic", "M_NumTraffic", Sf_Int(&f, 0));
	Sf_SetMod(&f, "InTraffic", "H_TrafficList", Sf_Build(&f, "[]"));
	Sf_SetMod(&f, "InTraffic", "H_NumTraffic", Sf_Int(&f, 0));
	Sf_Run(&f, "InTraffic", "LoadFakeTraffic", "()");
	Sf_Run(&f, "InTraffic", "FakeTraffic", "()");
	Sf_Own(&f, MAP, "BrowseAlarmLaunchers", "()");
	if (Sf_Compare(&f, SAVE("MapPress_BureauExists"), "!=", Sf_Str(&f, "1")) > 0) {
		Sf_Own(&f, MAP, "HideBureau", "()");
	}
	return Sf_None(&f);
}

/* HideBureau(): the bureau hidden; its position */
static PyObject *PressMap_HideBureau(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *b = ENT("DM_Door_Bureau_00");
	Sf_SetAttrI(&f, b, "MapCollision", 0);
	Sf_SetAttrI(&f, b, "Active", 0);
	return Sf_Return(&f, Sf_Attr(&f, b, "Pos"));
}

PyMethodDef g_PressMapMethods[] = {
	{ "RemoteMsg", PressMap_RemoteMsg, METH_VARARGS, NULL },
	{ "ForbiddenMsg", PressMap_ForbiddenMsg, METH_VARARGS, NULL },
	{ "StartAlarmator", PressMap_StartAlarmator, METH_VARARGS, NULL },
	{ "ActivateAlarm", PressMap_ActivateAlarm, METH_VARARGS, NULL },
	{ "CreateActivators", PressMap_CreateActivators, METH_VARARGS, NULL },
	{ "CreateDeactivators", PressMap_CreateDeactivators, METH_VARARGS, NULL },
	{ "DeactivateAlarm", PressMap_DeactivateAlarm, METH_VARARGS, NULL },
	{ "BrowseAlarmLaunchers", PressMap_BrowseAlarmLaunchers, METH_VARARGS, NULL },
	{ "InitStatus", PressMap_InitStatus, METH_VARARGS, NULL },
	{ "Init", PressMap_Init, METH_VARARGS, NULL },
	{ "HideBureau", PressMap_HideBureau, METH_VARARGS, NULL },
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
static PyObject *DemoClose_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Pr_EntS(&f, "DM_Door_Metro_Press", "UseFunc", "MapDemoClose.OnMetroUsed");
	PyObject *lv = ENT("DM_Door_Elevator_Press");
	Sf_SetAttrS(&f, lv, "UseFunc", "");
	Sf_SetAttrS(&f, lv, "OnObjSlot", "!CloseDoorSlot");
	return Sf_None(&f);
}

/* OnMetroUsed(UsrName, MetroName, optId, FullOption): a driver takes the metro to Outskirts, others cannot */
static PyObject *DemoClose_OnMetroUsed(PyObject *self, PyObject *args) {
	PyObject *usr, *metro, *opt, *full;
	SF_ARGS("OOOO", &usr, &metro, &opt, &full);
	int drives = Sf_In(&f, Sf_Attr(&f, USR(), "ActCtrl"), Sf_Mod(&f, "Chars", "CanDriveList"));
	if (drives > 0) {
		Sf_Run(&f, "Elevator", "UseElevator", "(OOsO)", usr, metro, "MoveLevel Outskirts", full);
		Sf_Run(&f, "Metro", "SetUsrParams", "(O)", usr);
	} else if (drives == 0) {
		Sf_Run(&f, "RaceMaker", "ShowSystemMessage", "(O)", LANG("Station_CantGo"));
	}
	return Sf_None(&f);
}

PyMethodDef g_PressMapDemoCloseMethods[] = {
	{ "Init", DemoClose_Init, METH_VARARGS, NULL },
	{ "OnMetroUsed", DemoClose_OnMetroUsed, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapSnd ---- */

/* LoadSounds(): the sounds of the map (three computers only on the PC) */
static PyObject *PressSnd_LoadSounds(PyObject *self, PyObject *args) {
	static const char *const first[] = { "light1.wav", "ambience1.wav", "Atmosfera-Press1.wav", "Atmosfera-Press2.wav", "datos-good1.wav",
		"computer1.wav", NULL };
	static const char *const pc[] = { "computer2.wav", "computer3.wav", "computer4.wav", NULL };
	static const char *const last[] = { "FXChispas.wav", "gas4.wav", "gas5.wav", NULL };
	SF_ARGS("");
	for (int i = 0; first[i]; i++) {
		Sf_Run(&f, "SSound", "LoadSound", "(s)", first[i]);
	}
	if (Sf_True(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "IsXbox")) == 0) {
		for (int i = 0; pc[i]; i++) {
			Sf_Run(&f, "SSound", "LoadSound", "(s)", pc[i]);
		}
	}
	for (int i = 0; last[i]; i++) {
		Sf_Run(&f, "SSound", "LoadSound", "(s)", last[i]);
	}
	return Sf_None(&f);
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

static byte Pr_Sounds(scriptFrame_t *f, const prSnd_t *t, int xbox) {
	for (; t->name && !f->failed; t++) {
		int mark = f->n;
		const char *snd = t->comp && xbox ? "Computer1" : t->snd;
		PyObject *a = f->failed ? NULL : Py_BuildValue("(s(ddd)s)", t->name, t->pos[0], t->pos[1], t->pos[2], snd);
		PyObject *kw;
		if (t->interval) {
			kw = Sf_Build(f, "{s:d,s:d,s:d,s:d,s:d,s:d,s:d}", "MinInterval", t->min, "MaxInterval", t->max, "Volume", t->vol, "Pitch", t->pitch,
				"AttenIni", t->ini, "AttenEnd", t->end, "Doppler", 0.0);
		} else {
			kw = Sf_Build(f, "{s:d,s:d,s:d,s:d,s:d}", "Volume", t->vol, "Pitch", t->pitch, "AttenIni", t->ini, "AttenEnd", t->end, "Doppler", 0.0);
		}
		PyObject *fn = Sf_Mod(f, "Sound", t->interval ? "SetPSInterval" : "SetPSLoop");
		Sf_CallKw(f, fn, Sf_Check(f, a != NULL) ? a : NULL, kw);
		Sf_Release(f, mark);
	}
	return !f->failed;
}

/* SetSounds(): the elements, the ambience of the press, the mental controller by the act of the game: working at
 * half or full speed, or broken with smoke and sparks */
static PyObject *PressSnd_SetSounds(PyObject *self, PyObject *args) {
	static const char *const half[] = { "2ndMurder", "3rdMurder", "Epilogue", NULL };
	static const char *const full[] = { "3rdMurder", "Epilogue", NULL };
	SF_ARGS("");
	Sf_Import(&f, "Sound");
	Sf_Run(&f, "Scrap", "CallElements", "(s)", "MapSnd.SndElements");
	int xbox = Sf_True(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "IsXbox"));
	Pr_Sounds(&f, press_snd_base, xbox > 0);
	PyObject *act = SAVE("GameAct");
	PyObject *mcs = SAVE("Press.MentalControllerStatus");
	int broken = Sf_EqS(&f, act, "Epilogue");
	if (broken > 0) {
		broken = Sf_EqS(&f, mcs, "1");
	}
	if (broken == 0) {
		if (Sf_InS(&f, act, half) > 0) {
			Pr_Sounds(&f, press_snd_half, xbox > 0);
			if (Sf_InS(&f, act, full) > 0) {
				Pr_Sounds(&f, press_snd_full, xbox > 0);
			}
		}
	} else if (broken > 0) {
		Sf_Run(&f, "Scrap", "Set", "(ss)", "FXElementsMentalControllerDamageSound", "FXChispas");
		Sf_Run(&f, "Scrap", "Set", "(sd)", "FXElementsMentalControllerDamageSoundVol", 0.03);
		Pr_Sounds(&f, press_snd_broken, xbox > 0);
	}
	return Sf_None(&f);
}

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
static PyObject *PressSnd_News1(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Scene");
	Sf_Print(&f, "s", "-----------------------------");
	Sf_Run(&f, "SSound", "LoadSound", "(s)", "News1/sweep-apagado.wav");
	Sf_Run(&f, "SSound", "LoadSound", "(s)", "News1/sweep-encendido.wav");
	Sf_Calls(&f, "MapSnd", news1_sounds);
	return Sf_None(&f);
}

PyMethodDef g_PressMapSndMethods[] = {
	{ "LoadSounds", PressSnd_LoadSounds, METH_VARARGS, NULL },
	{ "SetSounds", PressSnd_SetSounds, METH_VARARGS, NULL },
	{ "News1", PressSnd_News1, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_FicusBanker ---- */

/* InitStatus(LevelPath): Ficus and Scrapus as mission characters */
static PyObject *FicusBanker_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Lv_MissionChar(&f, "Police_Ficus", FB, "CreatePoliceFicusInit");
	Lv_MissionChar(&f, "BankDirector_Scrapus", FB, "CreateBankDirectorScrapusInit");
	return Sf_None(&f);
}

/* Init(LevelPath): the check's model; the mission goes on where it was saved */
static PyObject *FicusBanker_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Elevator");
	PyObject *svv = SAVE("Mission_Police_FicusBanker");
	Sf_Run(&f, "Scrap", "Preload3DObject", "(Oiii)", G(FB, "Model_Check"), 1, 1, 1);
	if (Sf_EqS(&f, svv, "TalkFicus") > 0) {
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Police_Ficus");
	} else if (Sf_EqS(&f, svv, "KillScrapus") > 0) {
		Sf_Own(&f, FB, "InitKillScrapus", "()");
	} else if (Sf_EqS(&f, svv, "TakeCheck") > 0) {
		PyObject *fn = G(FB, "SpawnCheck");
		Sf_Call(&f, fn, "(sO)", "BankDirector_Scrapus", Sf_Eval(&f, SAVE("CheckPos"), FB));
	} else if (Sf_EqS(&f, svv, "TalkFicus2") > 0) {
		Sf_Own(&f, FB, "InitTalkFicus2", "()");
	}
	return Sf_None(&f);
}

/* CreatePoliceFicusInit(e): Ficus talks the mission and stays where he is */
static PyObject *FicusBanker_CreatePoliceFicusInit(PyObject *self, PyObject *args) {
	PyObject *e;
	SF_ARGS("O", &e);
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_FicusBanker.FicusMissionSpeech");
	Lv_Stay(&f, e, 6, Sf_Str(&f, ""));
	Sf_SetAttr(&f, e, "Descriptor", LANG("CharName_Ficus"));
	return Sf_None(&f);
}

/* Speech.SpeechRoutines[kind](UsrName, CharName, optId, FullOption) */
static byte Pr_Routine(scriptFrame_t *f, const char *kind, PyObject *usr, PyObject *ch, PyObject *opt, PyObject *full) {
	int mark = f->n;
	PyObject *fn = Sf_GetItem(f, Sf_Mod(f, "Speech", "SpeechRoutines"), Sf_Str(f, kind));
	Sf_Call(f, fn, "(OOOO)", usr, ch, opt, full);
	Sf_Release(f, mark);
	return !f->failed;
}

/* Speech.OnSpch(msg, ans) */
static byte Pr_Spch(scriptFrame_t *f, const char *msg, const char *ans) {
	return Sf_Run(f, "Speech", "OnSpch", "(ss)", msg, ans);
}

/* FicusMissionSpeech(UsrName, CharName, optId, FullOption): Ficus' talks for the state of the mission */
static PyObject *FicusBanker_FicusMissionSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Lv_Speakers(&f, usr, ch);
	int other = Sf_EqS(&f, Sf_Attr(&f, USR(), "ActCtrl"), "Dtritus");
	if (other == 0) {
		Pr_Spch(&f, "Mission_TalkToFicus_Ficus_InitMsg2", "Mission_TalkToFicus_Ficus_InitAns2");
		return Sf_None(&f);
	}
	PyObject *svv = SAVE("Mission_Police_FicusBanker");
	if (Sf_EqS(&f, svv, "TalkFicus") > 0) {
		PyObject *first = G(FB, "Firstone");
		if (Sf_EqI(&f, first, 0) > 0) {
			Sf_NISpeech(&f, NULL, "Mission_TalkToFicus_Ficus_NI_1Msg");
			Sf_Run(&f, "Speech", "StdAnsLst", "([s])", "Mission_TalkToFicus_Ficus_1");
			Sf_SetGlobal(&f, FB, "Firstone", Sf_Int(&f, 1));
		} else if (Sf_EqI(&f, first, 1) > 0) {
			Lv_OnEnd(&f, FB, "InitKillScrapus", NULL);
			Pr_Spch(&f, "Mission_TalkToFicus_Ficus_1Msg", "Mission_TalkToFicus_Ficus_1Ans");
		} else {
			Pr_Routine(&f, "Police", usr, ch, opt, full);
		}
	} else if (Sf_EqS(&f, svv, "KillScrapus") > 0) {
		static const char *const opts[] = { "Talk", "Mission_TalkToFicus_Ficus_1", NULL };
		if (Sf_InS(&f, opt, opts) > 0) {
			Pr_Spch(&f, "Mission_KillScrapus_Ficus_InitMsg1", "Mission_KillScrapus_Ficus_InitAns1");
		} else {
			Pr_Routine(&f, "Police", usr, ch, opt, full);
		}
	} else if (Sf_EqS(&f, svv, "TalkFicus2") > 0) {
		if (Sf_EqS(&f, opt, "Talk") > 0) {
			Lv_OnEnd(&f, FB, "SetNextMission", NULL);
			Pr_Spch(&f, "Mission_TalkToFicus2_Ficus_InitMsg1", "Mission_TalkToFicus2_Ficus_InitAns1");
		} else {
			Pr_Routine(&f, "Police", usr, ch, opt, full);
		}
	} else if (Sf_EqS(&f, opt, "Talk") > 0) {
		Pr_Spch(&f, "Mission_TalkToFicus2_Ficus_InitMsg2", "Mission_TalkToFicus2_Ficus_InitAns2");
	} else {
		Pr_Routine(&f, "Police", usr, ch, opt, full);
	}
	return Sf_None(&f);
}

/* CreateBankDirectorScrapusInit(e) */
static PyObject *FicusBanker_CreateBankDirectorScrapusInit(PyObject *self, PyObject *args) {
	PyObject *e;
	SF_ARGS("O", &e);
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_FicusBanker.ScrapusMissionSpeech");
	Sf_SetAttr(&f, e, "Descriptor", LANG("CharName_Scrapus"));
	return Sf_None(&f);
}

/* ScrapusMissionSpeech(UsrName, CharName, optId, FullOption) */
static PyObject *FicusBanker_ScrapusMissionSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Lv_Speakers(&f, usr, ch);
	int kill = Sf_EqS(&f, SAVE("Mission_Police_FicusBanker"), "KillScrapus");
	if (kill > 0) {
		int check = Sf_True(&f, ENT("MissionCheck"));
		kill = check < 0 ? -1 : !check;
	}
	if (kill > 0) {
		Pr_Spch(&f, "Mission_KillScrapus_Scrapus_InitMsg1", "Mission_KillScrapus_Scrapus_InitAns1");
	} else if (kill == 0) {
		Pr_Routine(&f, "BankDirector", usr, ch, opt, full);
	}
	return Sf_None(&f);
}

/* InitKillScrapus(): Scrapus is the target; his death or possession drops the check */
static PyObject *FicusBanker_InitKillScrapus(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_Police_FicusBanker", "KillScrapus");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "BankDirector_Scrapus");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_KillScrapus_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	int free = Sf_Compare(&f, Sf_Mod(&f, "CharAct", "onNPCDeath"), "==", Py_None);
	if (free > 0) {
		Sf_SetMod(&f, "CharAct", "onNPCDeath", G(FB, "SpawnCheck"));
	} else if (free == 0) {
		Sf_Run(&f, "Scrap", "ConsoleError", "(s)", "CharAct.onNPCDeath already overrrided \n");
	}
	free = Sf_Compare(&f, Sf_Mod(&f, "CharConversor", "OnPossession"), "==", Py_None);
	if (free > 0) {
		Sf_SetMod(&f, "CharConversor", "OnPossession", G(FB, "SpawnCheck"));
	} else if (free == 0) {
		Sf_Run(&f, "Scrap", "ConsoleError", "(s)", "CharConversor.OnPossession already overrrided \n");
	}
	return Sf_None(&f);
}

/* SpawnCheck(name, pos): Scrapus dies or is possessed: his check appears at pos with its arrow */
static PyObject *FicusBanker_SpawnCheck(PyObject *self, PyObject *args) {
	PyObject *name, *pos;
	SF_ARGS("OO", &name, &pos);
	if (Sf_EqS(&f, name, "BankDirector_Scrapus") <= 0) {
		return Sf_None(&f);
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_Police_FicusBanker", "TakeCheck");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "CheckPos", Sf_Repr(&f, pos));
	Sf_SetMod(&f, "CharConversor", "OnPossession", Py_None);
	Sf_SetMod(&f, "CharAct", "onNPCDeath", Py_None);
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "MissionCheck", Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "Item");
	Sf_Method(&f, l, "SetModel", "(O)", G(FB, "Model_Check"));
	Sf_SetAttrI(&f, l, "Radius", 60);
	Sf_SetAttrI(&f, l, "AppearsIfFree", 0);
	Sf_SetAttrI(&f, l, "AutoFace", 0);
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.25));
	Sf_SetAttrS(&f, l, "OnTake", "Mission_FicusBanker.TakeCheck");
	PyObject *l2 = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "MissionCheckArrow", Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
		Sf_Item(&f, pos, 2), "WithLife");
	Sf_SetAttr(&f, l2, "CamTgt", Sf_Build(&f, "(iii)", 0, 60, 0));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, l2, "Name"));
	return Sf_None(&f);
}

/* TakeCheck(ItemName, UsrName): the check taken: back to Ficus */
static PyObject *FicusBanker_TakeCheck(PyObject *self, PyObject *args) {
	PyObject *item, *usr;
	SF_ARGS("OO", &item, &usr);
	Sf_Import(&f, "Sound");
	Sf_Own(&f, FB, "InitTalkFicus2", "()");
	PyObject *it = ENT("MissionCheck");
	Sf_SetAttrI(&f, it, "rTime", 20);
	Sf_Schedule(&f, Sf_OpI(&f, Sf_Time(&f), '+', 5), Sf_Attr(&f, it, "Del"), NOARGS, NULL);
	Sf_Method(&f, ENT("MissionCheckArrow"), "Del", "()");
	PyObject *a = Sf_GetItem(&f, Sf_Mod(&f, "Sound", "ItemTake"), Sf_Str(&f, "MoneyItem"));
	Sf_Run(&f, "SSound", "PlaySound", "(OOO)", Sf_Attr(&f, it, "Pos"), Sf_Item(&f, a, 0), Sf_Item(&f, a, 1));
	return Sf_None(&f);
}

/* SetNextMission(): the race of Multiplus in DownTown is next */
static PyObject *FicusBanker_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_Run(&f, "__builtin__", "apply", "(OO)", Sf_Mod(&f, "MissionsFuncs", "SetNextMission"), G(FB, "NextMissionData"));
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_WinRaceMultiplus_MissionText1", 1);
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "Mission_Police_FicusBanker");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "CheckPos");
	return Sf_None(&f);
}

PyMethodDef g_PressFicusBankerMethods[] = {
	{ "InitStatus", FicusBanker_InitStatus, METH_VARARGS, NULL },
	{ "Init", FicusBanker_Init, METH_VARARGS, NULL },
	{ "CreatePoliceFicusInit", FicusBanker_CreatePoliceFicusInit, METH_VARARGS, NULL },
	{ "FicusMissionSpeech", FicusBanker_FicusMissionSpeech, METH_VARARGS, NULL },
	{ "CreateBankDirectorScrapusInit", FicusBanker_CreateBankDirectorScrapusInit, METH_VARARGS, NULL },
	{ "ScrapusMissionSpeech", FicusBanker_ScrapusMissionSpeech, METH_VARARGS, NULL },
	{ "InitKillScrapus", FicusBanker_InitKillScrapus, METH_VARARGS, NULL },
	{ "SpawnCheck", FicusBanker_SpawnCheck, METH_VARARGS, NULL },
	{ "TakeCheck", FicusBanker_TakeCheck, METH_VARARGS, NULL },
	{ "SetNextMission", FicusBanker_SetNextMission, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GetCrazyData ---- */

/* Init(LevelPath): the bureau hidden (its position kept); the mission goes on where it was saved */
static PyObject *CrazyData_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Map");
	Sf_SetGlobal(&f, GC, "bureauPos", Sf_Ask(&f, "Map", "HideBureau", "()"));
	PyObject *state = SAVE("MissionState");
	if (Sf_EqS(&f, state, "GetCrazyData") > 0) {
		Sf_Own(&f, GC, "GetCrazyData", "()");
	} else if (Sf_EqS(&f, state, "GoToPressRoom") > 0) {
		Pr_DelIf(&f, Sf_Str(&f, "Humphrey_Default"));
		Sf_Own(&f, GC, "GoToPressRoom", "()");
	} else if (Sf_EqS(&f, state, "FindHumphrey") > 0) {
		Sf_Own(&f, GC, "FindHumphrey", "()");
	} else if (Sf_EqS(&f, state, "FindTrurl") > 0) {
		Sf_Own(&f, GC, "FindTrurl", "()");
	}
	return Sf_None(&f);
}

/* the switch e can be used to turn it on (OnSwitchPressed) */
static byte Pr_SwitchUse(scriptFrame_t *f, PyObject *e) {
	int mark = f->n;
	Sf_SetAttrS(f, e, "UseFunc", "Mission_GetCrazyData.OnSwitchPressed");
	Sf_SetAttrI(f, e, "ArrowCanUse", 1);
	Sf_SetAttrI(f, e, "ArrnOpt", 0);
	Sf_SetAttr(f, e, "ArrOpt0", Sf_Cat(f, "sO", "Use:", Sf_Lang(f, Sf_Str(f, "USE_Switchs_Activate"))));
	Sf_Release(f, mark);
	return !f->failed;
}

/* GetCrazyData(): the four switches of the alarm (their class and entities), the alarm hook, the scorer */
static PyObject *CrazyData_GetCrazyData(PyObject *self, PyObject *args) {
	static const char *const acts[][3] = { { "ON", "/Map/Switch/Anm/ON.M3D", "0" }, { "OFF", "/Map/Switch/Anm/OFF.M3D", "0" },
		{ "StayON", "/Map/Switch/Anm/StayON.M3D", "1" }, { "StayOFF", "/Map/Switch/Anm/StayOFF.M3D", "1" } };
	SF_ARGS("");
	Sf_Import(&f, "Init");
	Sf_Import(&f, "Alarm");
	Pr_DelIf(&f, Sf_Str(&f, "Humphrey_Default"));
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MapPress_HiAlarm", "0");
	Sf_Run(&f, "SAct", "CreateClass", "(s)", "Switch");
	Sf_Run(&f, "SAct", "SetCls", "(sO)", "ModelFilePath", Sf_Cat(&f, "Os", Sf_Mod(&f, "Init", "Path"), "/Map/Switch/Switch.m3d"));
	for (int i = 0; i < 4 && !f.failed; i++) {
		int mark = f.n;
		Sf_Run(&f, "SAct", "CreateAction", "(s)", acts[i][0]);
		Sf_Run(&f, "SAct", "SetAct", "(sO)", "AnmFilePath", Sf_Cat(&f, "Os", Sf_Mod(&f, "Init", "Path"), acts[i][1]));
		Sf_Run(&f, "SAct", "SetAct", "(si)", "isLoop", acts[i][2][0] - '0');
		Sf_Release(&f, mark);
	}
	Sf_Run(&f, "SAct", "SetCls", "(ss)", "DefAction", "StayOFF");
	PyObject *switches = G(GC, "switches");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *sw = Sf_Next(&f, switches, i);
		if (!sw) {
			break;
		}
		Sf_Run(&f, "Scrap", "StartDummySearch", "(Oi)", sw, 1);
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_True(&f, dummy) == 0) {
			Sf_RaiseText(&f, "No se ha encontrado el dummy para el interruptor");
			break;
		}
		PyObject *pos = Sf_Item(&f, dummy, 1);
		PyObject *e = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", Sf_Item(&f, dummy, 0), Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
			Sf_Item(&f, pos, 2), "WithLife");
		PyObject *ang = Sf_Item(&f, dummy, 2);
		Sf_SetAttr(&f, e, "Ang", Sf_Build(&f, "(OOi)", Sf_Item(&f, ang, 0), Sf_Item(&f, ang, 1), 0));
		Sf_SetAttrS(&f, e, "ActCtrl", "Switch");
		Sf_SetAttrI(&f, e, "VRadius", 50);
		Sf_SetAttrI(&f, e, "ArrowCanUse", 1);
		Sf_SetAttrS(&f, e, "UseFunc", "Mission_GetCrazyData.OnSwitchPressed");
		Sf_SetAttrI(&f, e, "ArrnOpt", 0);
		Sf_SetAttr(&f, e, "ArrOpt0", Sf_Cat(&f, "sO", "Use:", LANG("USE_Switchs_Activate")));
		Sf_Release(&f, mark);
	}
	Sf_Run(&f, "MissionsFuncs", "SetMultipleMissionTargets", "(O)", switches);
	Sf_SetMod(&f, "Alarm", "TempAbortMissionCallback", G(GC, "OnAlarmActivated"));
	Pr_Remote(&f, "Mission_GetCrazyData_RemoteMessage1", "DeepThroat", NULL, NULL, 0);
	Sf_SetMod(&f, "Menu", "MissionScorerCall", G(GC, "MissionScorer"));
	Sf_Own(&f, GC, "MissionScorer", "(i)", 0);
	return Sf_None(&f);
}

/* OnAlarmActivated(): the alarm resets the switches, the counter and the timer */
static PyObject *CrazyData_OnAlarmActivated(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *switches = G(GC, "switches");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *sw = Sf_Next(&f, switches, i);
		if (!sw) {
			break;
		}
		PyObject *e = Sf_Entity(&f, sw);
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrS(&f, e, "DefLoop", "StayOFF");
			Sf_SetAttrS(&f, e, "Action", "OFF");
			Pr_SwitchUse(&f, e);
			Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
		}
		Sf_Release(&f, mark);
	}
	Sf_Run(&f, "MissionsFuncs", "SetMultipleMissionTargets", "(O)", G(GC, "switches"));
	Sf_SetGlobal(&f, GC, "TimerCanBeSeen", Sf_Int(&f, 0));
	Sf_SetGlobal(&f, GC, "switchesRemaining", Sf_Int(&f, 4));
	Sf_Run(&f, "SScorer", "Set", "(issi)", 0, "MissionTimer", "Visible", 0);
	Sf_Own(&f, GC, "MissionScorer", "(i)", 0);
	Pr_Remote(&f, "Mission_GetCrazyData_RemoteMessage6", "DeepThroat", NULL, NULL, 0);
	return Sf_None(&f);
}

/* OnSwitchPressed(UsrName, CharName, optId, FullOption): a switch on (not during the alarm); a sentinel comes to
 * look; after the last one the timer */
static PyObject *CrazyData_OnSwitchPressed(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	PyObject *e = Sf_Entity(&f, ch);
	if (Sf_True(&f, e) <= 0) {
		return Sf_None(&f);
	}
	int alarm = Sf_True(&f, Sf_Item(&f, Sf_Ask(&f, "Scrap", "GetAlarm", "()"), 0));
	if (alarm > 0) {
		Pr_Remote(&f, "Mission_GetCrazyData_RemoteMessage6", "DeepThroat", NULL, NULL, 0);
		return Sf_None(&f);
	}
	Sf_SetAttrS(&f, e, "DefLoop", "StayON");
	Sf_SetAttrS(&f, e, "Action", "ON");
	Sf_SetAttrS(&f, e, "UseFunc", "");
	Sf_SetAttrI(&f, e, "ArrowCanUse", 0);
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
	Sf_Run(&f, "SLogic", "SendSentinelToWatch", "(O)", Sf_Attr(&f, e, "Pos"));
	Sf_SetGlobal(&f, GC, "switchesRemaining", Sf_OpI(&f, G(GC, "switchesRemaining"), '-', 1));
	Sf_Own(&f, GC, "MissionScorer", "(i)", 0);
	PyObject *left = G(GC, "switchesRemaining");
	int done = Sf_EqI(&f, left, 0);
	if (done > 0) {
		Pr_Remote(&f, "Mission_GetCrazyData_RemoteMessage5", "DeepThroat", GC, "StartTimer", 1);
	} else if (done == 0) {
		PyObject *msg = Sf_Cat(&f, "sO", "Mission_GetCrazyData_RemoteMessage", Sf_Repr(&f, Sf_Op(&f, Sf_Int(&f, 5), '-', G(GC, "switchesRemaining"))));
		Sf_Run(&f, "Speech", "AddRemoteCom", "(Os)", msg, "DeepThroat");
	}
	return Sf_None(&f);
}

/* StartTimer(): the countdown of the data is visible */
static PyObject *CrazyData_StartTimer(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, GC, "TimerCanBeSeen", Sf_Int(&f, 1));
	Sf_Run(&f, "Scrap", "Set", "(sO)", "TimeSpecialUseTimer", G(GC, "missionTime"));
	Sf_Run(&f, "SScorer", "Set", "(issi)", 0, "MissionTimer", "Visible", 1);
	Pr_DelIf(&f, Sf_Str(&f, "Humphrey_Default"));
	Sf_Own(&f, GC, "MissionScorer", "(i)", 0);
	return Sf_None(&f);
}

/* TimeExpired(): the switches go off for good; on to the press room */
static PyObject *CrazyData_TimeExpired(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *switches = G(GC, "switches");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *sw = Sf_Next(&f, switches, i);
		if (!sw) {
			break;
		}
		PyObject *e = Sf_Entity(&f, sw);
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrS(&f, e, "DefLoop", "StayOFF");
			Sf_SetAttrS(&f, e, "Action", "OFF");
			Sf_SetAttrS(&f, e, "UseFunc", "");
			Sf_SetAttrI(&f, e, "ArrowCanUse", 0);
		}
		Sf_Release(&f, mark);
	}
	Sf_SetMod(&f, "Menu", "MissionScorerCall", Py_None);
	Pr_Remote(&f, "Mission_GetCrazyData_RemoteMessage7", "DeepThroat", GC, "PreGoToPressRoom", 1);
	Sf_SetMod(&f, "Alarm", "TempAbortMissionCallback", Py_None);
	return Sf_None(&f);
}

/* GoToPressRoom(): an item where the bureau was takes the player to the press room */
static PyObject *CrazyData_GoToPressRoom(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MapPress_BureauExists", "0");
	PyObject *bp = G(GC, "bureauPos");
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "EmptyBureau", Sf_Item(&f, bp, 0), Sf_Item(&f, bp, 1), Sf_Item(&f, bp, 2), "Item");
	Sf_SetAttrI(&f, l, "Radius", 500);
	Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.25));
	Sf_SetAttrS(&f, l, "OnTake", "Mission_GetCrazyData.OnFakeBureauTaken");
	PyObject *name = Sf_Cat(&f, "sO", "CheckArrow", Sf_Attr(&f, l, "Name"));
	bp = G(GC, "bureauPos");
	PyObject *l2 = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, Sf_Item(&f, bp, 0), Sf_Item(&f, bp, 1), Sf_Item(&f, bp, 2), "WithLife");
	Sf_SetAttr(&f, l2, "CamTgt", Sf_Build(&f, "(iii)", 0, 100, 0));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, l2, "Name"));
	return Sf_None(&f);
}

/* OnFakeBureauTaken(ItemName, CharName): the player reaches the place: the item and its arrow go */
static PyObject *CrazyData_OnFakeBureauTaken(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	SF_ARGS("OO", &item, &ch);
	PyObject *usr = USR();
	if (Sf_True(&f, usr) <= 0) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, ch, "==", Sf_Attr(&f, usr, "Name")) > 0) {
		Pr_DelIf(&f, item);
		Pr_DelIf(&f, Sf_Cat(&f, "sO", "CheckArrow", item));
		Pr_Remote(&f, "Mission_GoToPressRoom_RemoteMessage1", "DeepThroat", GC, "PreFindHumphrey", 1);
	}
	return Sf_None(&f);
}

/* a mission character made now; when it is there it gets use and becomes the target (pos NULL: where it is) */
static byte Pr_MissionTarget(scriptFrame_t *f, const char *name, const char *use, const int *pos) {
	int mark = f->n;
	Sf_Run(f, "CharsNPC", "CreateMissionChar", "(s)", name);
	Sf_Run(f, "CharsNPC", "CreateChar", "(O)", Sf_GetItem(f, Sf_Mod(f, "CharsNPC", "CharRes"), Sf_Str(f, name)));
	PyObject *e = Sf_Ask(f, "Scrap", "GetEntity", "(s)", name);
	if (Sf_True(f, e) > 0) {
		if (pos) {
			Sf_SetAttr(f, e, "Pos", Sf_Build(f, "(iii)", pos[0], pos[1], pos[2]));
		}
		Sf_SetAttrS(f, e, "UseFunc", use);
		Sf_Run(f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(f, e, "Name"));
	}
	Sf_Release(f, mark);
	return !f->failed;
}

/* FindHumphrey(): Humphrey in the press room is the target */
static PyObject *CrazyData_FindHumphrey(PyObject *self, PyObject *args) {
	static const int pos[3] = { -3961, -1139, 3318 };
	SF_ARGS("");
	Sf_Import(&f, "CharsNPC");
	Pr_MissionTarget(&f, "Humphrey_Default", "Mission_GetCrazyData.HumphreySpeech", pos);
	return Sf_None(&f);
}

/* HumphreySpeech(UsrName, CharName, optId, FullOption) */
static PyObject *CrazyData_HumphreySpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		PyObject *e = Sf_Entity(&f, ch);
		if (Sf_True(&f, e) > 0) {
			Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
			Sf_SetAttrS(&f, e, "UseFunc", "CharsNPC.GenericNPCSpeech");
		}
		Lv_OnEnd(&f, GC, "PreFindTrurl", NULL);
		Pr_Spch(&f, "Mission_FindHumphrey_Humphrey_InitMsg1", "Mission_FindHumphrey_Humphrey_InitAns1");
	}
	return Sf_None(&f);
}

/* FindTrurl(): Trurl is the target */
static PyObject *CrazyData_FindTrurl(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Pr_MissionTarget(&f, "Messenger_Trurl", "Mission_GetCrazyData.TrurlSpeech", NULL);
	return Sf_None(&f);
}

/* TrurlSpeech(UsrName, CharName, optId, FullOption): Trurl's talk; it ends the mission while he is the target */
static PyObject *CrazyData_TrurlSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Lv_Speakers(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") <= 0) {
		return Sf_None(&f);
	}
	PyObject *e = Sf_Entity(&f, ch);
	int target = Sf_True(&f, e);
	if (target > 0) {
		target = Sf_EqI(&f, Sf_Attr(&f, e, "RadarShow"), 1);
	}
	if (target > 0) {
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 0);
		Lv_OnEnd(&f, GC, "SetNextMission", NULL);
	} else if (target == 0) {
		Sf_SetMod(&f, "Speech", "OnEndOfSpeech", Py_None);
		Sf_SetMod(&f, "Speech", "OnEndOfSpeechParams", NOARGS);
	}
	Sf_NISpeech(&f, "Trurl", "Mission_FindTrurl_Trurl_NI_1Msg");
	return Sf_None(&f);
}

/* the text of a mission scorer at y + dy as GetCrazyData sets it */
static byte Pr_ScorerText(scriptFrame_t *f, PyObject *id, const char *item, long dy, PyObject *text) {
	int mark = f->n;
	PyObject *y = Sf_Mod(f, "Scorer", "MissionScorerText_Y");
	Sf_MissionText(f, id, item, "Text", Sf_Mod(f, "Scorer", "MissionScorerText_X"), dy ? Sf_OpI(f, y, '+', dy) : y, "Horatio", "Shadow", text,
		"Left", NULL);
	Sf_Release(f, mark);
	return !f->failed;
}

/* MissionScorer(id): the switches left and the timer (hidden until it runs) */
static PyObject *CrazyData_MissionScorer(PyObject *self, PyObject *args) {
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	PyObject *id;
	SF_ARGS("O", &id);
	Pr_ScorerText(&f, id, "SwitchesRemainingText", 0, Sf_Cat(&f, "Os", LANG("Mission_GetCrazyData_Scorer1"), ":"));
	Pr_ScorerText(&f, id, "SwitchesRemaining", 20, Sf_Repr(&f, G(GC, "switchesRemaining")));
	Sf_Run(&f, "SScorer", "Add", "(Oss)", id, "MissionTimer", "Timer");
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "MissionTimer", "X", Sf_OpI(&f, Sf_Mod(&f, "Scorer", "MissionScorerText_X"), '+', 40));
	Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "MissionTimer", "Y", Sf_OpI(&f, Sf_Mod(&f, "Scorer", "MissionScorerText_Y"), '+', 60));
	Sf_Run(&f, "SScorer", "Set", "(Osss)", id, "MissionTimer", "Font", "Horatio");
	Sf_Run(&f, "SScorer", "Set", "(Osss)", id, "MissionTimer", "Effect", "Shadow");
	Sf_Run(&f, "SScorer", "Set", "(Osss)", id, "MissionTimer", "Align", "Left");
	Sf_Run(&f, "SScorer", "Set", "(Osss)", id, "MissionTimer", "TimerType", "TimeSpecialUse");
	Sf_Run(&f, "SScorer", "Set", "(Osss)", id, "MissionTimer", "OnTimeExpired", "Mission_GetCrazyData.TimeExpired");
	for (int i = 0; i < 4 && !f.failed; i++) {
		int mark = f.n;
		Sf_Run(&f, "SScorer", "Set", "(OssO)", id, "MissionTimer", rgba[i], Sf_Item(&f, Sf_Mod(&f, "Scorer", "MissionScorerText_Color"), i));
		Sf_Release(&f, mark);
	}
	if (Sf_True(&f, G(GC, "TimerCanBeSeen")) == 0) {
		Sf_Run(&f, "SScorer", "Set", "(Ossi)", id, "MissionTimer", "Visible", 0);
	}
	return Sf_None(&f);
}

PyMethodDef g_PressGetCrazyDataMethods[] = {
	{ "Init", CrazyData_Init, METH_VARARGS, NULL },
	{ "GetCrazyData", CrazyData_GetCrazyData, METH_VARARGS, NULL },
	{ "OnAlarmActivated", CrazyData_OnAlarmActivated, METH_VARARGS, NULL },
	{ "OnSwitchPressed", CrazyData_OnSwitchPressed, METH_VARARGS, NULL },
	{ "StartTimer", CrazyData_StartTimer, METH_VARARGS, NULL },
	{ "TimeExpired", CrazyData_TimeExpired, METH_VARARGS, NULL },
	{ "GoToPressRoom", CrazyData_GoToPressRoom, METH_VARARGS, NULL },
	{ "OnFakeBureauTaken", CrazyData_OnFakeBureauTaken, METH_VARARGS, NULL },
	{ "FindHumphrey", CrazyData_FindHumphrey, METH_VARARGS, NULL },
	{ "HumphreySpeech", CrazyData_HumphreySpeech, METH_VARARGS, NULL },
	{ "FindTrurl", CrazyData_FindTrurl, METH_VARARGS, NULL },
	{ "TrurlSpeech", CrazyData_TrurlSpeech, METH_VARARGS, NULL },
	{ "MissionScorer", CrazyData_MissionScorer, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GoToPress ---- */

/* Init(LevelPath): the confession scene, the gambler's head, the boss room only for Dtritus, the scene at the
 * elevator's arrival */
static PyObject *GoToPress_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Elevator");
	Sf_Own(&f, GP, "LoadScene", "()");
	Sf_Run(&f, "SScorer", "AddModel", "(isssiii)", 0, "EvilGambler", "Models/Heads/HeadEvilGambler/HeadEvilGambler", "", 4, 25, 10);
	Pr_BossDoor(&f, "DtritusGroupGateKeeper", "GateKeeper.UseDoor");
	Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", G(GP, "LaunchScene"));
	return Sf_None(&f);
}

/* CrazySads(usr): after the scene: the player in place, the gambler's message */
static PyObject *GoToPress_CrazySads(PyObject *self, PyObject *args) {
	PyObject *usr;
	SF_ARGS("O", &usr);
	Pr_UsrBack(&f, 4095, 163, 436, 30, 1.0);
	Pr_Remote(&f, "Mission_GoToPress_RemoteMessage1", "EvilGambler", GP, "SetNextMission", 1);
	return Sf_None(&f);
}

PyMethodDef g_PressGoToPressMethods[] = {
	{ "Init", GoToPress_Init, METH_VARARGS, NULL },
	{ "CrazySads", GoToPress_CrazySads, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the human who follows Dtritus (Mission_GoWithHuman and Mission_TalkToBoss) ---- */

/* CreateHuman(e): the human of the mission (the global human); TalkToBoss also takes his use */
static PyObject *Pr_CreateHuman(PyObject *args, const char *module, int nouse) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_SetGlobal(&f, module, "human", e);
	PyObject *human = G(module, "human");
	Sf_SetAttr(&f, human, "Descriptor", LANG("CharName_Human"));
	if (nouse) {
		Sf_SetAttrS(&f, G(module, "human"), "UseFunc", "");
	}
	Sf_SetAttrI(&f, G(module, "human"), "ArrowCanUse", 0);
	return Sf_None(&f);
}

/* the human stops and complains (SAI state 0, the message msg, not the target) */
static byte Pr_HumanStops(scriptFrame_t *f, const char *module, const char *msg) {
	int mark = f->n;
	Sf_Run(f, "SAI", "SetStateChar", "(iO)", 0, Sf_Attr(f, Sf_Global(f, module, "human"), "Name"));
	Sf_Run(f, "Speech", "AddRemoteCom", "(ss)", msg, "Human");
	Sf_Run(f, "MissionsFuncs", "ActivateTarget", "(Oi)", Sf_Global(f, module, "human"), 0);
	Sf_Release(f, mark);
	return !f->failed;
}

/* OnPossession(newCharName, pos): the human will not follow a possessed character */
static PyObject *Pr_HumanOnPossession(PyObject *args, const char *module, const char *msg) {
	PyObject *name, *pos;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OO", &name, &pos)) {
		return NULL;
	}
	Sf_Import(&f, "MissionsFuncs");
	Pr_HumanStops(&f, module, msg);
	return Sf_None(&f);
}

/* OnExitConversor(): out of the conversor: as Dtritus the human is a target again, else he stops */
static PyObject *Pr_HumanOnExitConversor(PyObject *args, const char *module, const char *msg) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "MissionsFuncs");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) <= 0) {
		return Sf_None(&f);
	}
	int other = Sf_EqS(&f, Sf_Attr(&f, usr, "ActCtrl"), "Dtritus");
	if (other == 0) {
		Pr_HumanStops(&f, module, msg);
	} else if (other > 0) {
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(module, "human"), 2);
	}
	return Sf_None(&f);
}

/* CheckHumanDist(): 1 when the player is within maxHumanDist of the human, else 0 */
static PyObject *Pr_CheckHumanDist(PyObject *args, const char *module) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *usr = USR();
	if (Sf_True(&f, usr) > 0) {
		PyObject *pu = Sf_Attr(&f, usr, "Pos");
		PyObject *dist = Sf_Ask(&f, "SVec", "ModSqr", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", pu, Sf_Attr(&f, G(module, "human"), "Pos")));
		PyObject *max = Sf_Op(&f, G(module, "maxHumanDist"), '*', G(module, "maxHumanDist"));
		if (Sf_Compare(&f, dist, "<=", max) > 0) {
			return Sf_Return(&f, Sf_Int(&f, 1));
		}
	}
	return Sf_Return(&f, Sf_Int(&f, 0));
}

/* ---- Mission_GoWithHuman ---- */

/* InitStatus(LevelPath): the human at the entrance (by the metro unless the player comes by the elevator), the
 * alarm on, the epilogue, the mental controller still working */
static PyObject *GoWithHuman_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Lv_MissionChar(&f, "Human_Entrance", GW, "CreateHuman");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "AlarmActive", "1");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "AlarmStatus", "1");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Dtritus");
	int elevator = Lv_SaveIs(&f, "ComeFrom", "DoorElevator");
	if (elevator > 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "DoorElevator");
	} else if (elevator == 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "DoorMetro");
		Sf_SetAttr(&f, Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "Human_Entrance")), "Pos", Sf_Build(&f, "(iii)", 3344, -1575, -3724));
		Sf_SetAttr(&f, Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "Human_Entrance")), "Ang", Sf_Build(&f, "(iii)", 0, 90, 0));
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "GameAct", "Epilogue");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Press.MentalControllerStatus", "0");
	return Sf_None(&f);
}

/* CreateHuman(e) */
static PyObject *GoWithHuman_CreateHuman(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_CreateHuman(args, GW, 0);
}

/* Init(LevelPath): the switch of the machine is the target, the human follows Dtritus, the scenes */
static PyObject *GoWithHuman_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "MissionsFuncs");
	PyObject *dummy = Lv_Dummy(&f, "DM_Machine_Switch", 1);
	if (Sf_True(&f, dummy) == 0) {
		Sf_RaiseText(&f, "ERROR: No se encuentra el dummy 'DM_Machine_Switch'");
		return Sf_None(&f);
	}
	PyObject *pos = Sf_Item(&f, dummy, 1);
	PyObject *sw = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "Machine_Switch", Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2),
		"WithLife");
	Sf_SetGlobal(&f, GW, "switch", sw);
	Sf_SetAttr(&f, sw, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), '+', 180), 0));
	Sf_SetAttrI(&f, sw, "ActiveCMask", 0);
	Sf_SetAttrI(&f, sw, "MapCollision", 1);
	Sf_SetAttr(&f, sw, "CamTgt", Sf_Build(&f, "(iii)", 0, 50, 0));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, sw, "Name"));
	PyObject *usr = USR();
	PyObject *hn = Sf_Attr(&f, G(GW, "human"), "Name");
	Sf_Run(&f, "SAI", "SetStateChar", "(iOOd)", 3, hn, Sf_Attr(&f, usr, "Name"), 1.0);
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(GW, "human"), 2);
	Sf_SetMod(&f, "CharConversor", "OnPossession", G(GW, "OnPossession"));
	Sf_SetMod(&f, "CharConversor", "afterExitConversor", G(GW, "OnExitConversor"));
	Sf_Own(&f, GW, "CheckMaxDist", "()");
	Sf_Own(&f, GW, "LoadScene", "()");
	Sf_Own(&f, GW, "LoadScene2", "()");
	return Sf_None(&f);
}

/* OnPossession(newCharName, pos) */
static PyObject *GoWithHuman_OnPossession(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_HumanOnPossession(args, GW, "Mission_GoWithHuman_RemoteMessage1");
}

/* OnExitConversor() */
static PyObject *GoWithHuman_OnExitConversor(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_HumanOnExitConversor(args, GW, "Mission_GoWithHuman_RemoteMessage1");
}

/* CheckMaxDist(): every second: the human stops when Dtritus is too far, else follows him; at the switch the scene */
static PyObject *GoWithHuman_CheckMaxDist(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Later(&f, Sf_Float(&f, 1.0), G(GW, "CheckMaxDist"), NOARGS, "CheckMaxDist");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) <= 0) {
		return Sf_None(&f);
	}
	PyObject *hn = Sf_Attr(&f, G(GW, "human"), "Name");
	int near = Sf_True(&f, Sf_Own(&f, GW, "CheckHumanDist", "()"));
	if (near == 0) {
		if (Sf_Compare(&f, Sf_Ask(&f, "SAI", "GetStateChar", "(O)", hn), "!=", Sf_Int(&f, 0)) > 0) {
			Sf_Run(&f, "SAI", "SetStateChar", "(iO)", 0, hn);
			Pr_Remote(&f, "Mission_GoWithHuman_RemoteMessage1", "Human", NULL, NULL, 0);
		}
	} else if (near > 0 && Sf_EqS(&f, Sf_Attr(&f, usr, "ActCtrl"), "Dtritus") > 0) {
		int follow = Sf_Compare(&f, Sf_Ask(&f, "SAI", "GetStateChar", "(O)", Sf_Attr(&f, G(GW, "human"), "Name")), "!=", Sf_Int(&f, 3));
		if (follow > 0) {
			Sf_Run(&f, "SAI", "SetStateChar", "(iOOd)", 3, Sf_Attr(&f, G(GW, "human"), "Name"), Sf_Attr(&f, usr, "Name"), 1.0);
		} else if (follow == 0) {
			PyObject *ph = Sf_Attr(&f, G(GW, "human"), "Pos");
			PyObject *dist = Sf_Ask(&f, "SVec", "ModSqr", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", ph, Sf_Attr(&f, G(GW, "switch"), "Pos")));
			PyObject *min = Sf_Op(&f, G(GW, "minDistFromSwitch"), '*', G(GW, "minDistFromSwitch"));
			if (Sf_Compare(&f, dist, "<=", min) > 0) {
				Sf_Own(&f, GW, "LaunchScene", "()");
			}
		}
	}
	return Sf_None(&f);
}

/* CheckHumanDist() */
static PyObject *GoWithHuman_CheckHumanDist(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_CheckHumanDist(args, GW);
}

/* LoadNextMission(name=None): the machine destroyed, the reputation and the ship profile back, on to the final
 * fight in the GamblinDen */
static PyObject *GoWithHuman_LoadNextMission(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "name", NULL };
	static const char *const prev[][2] = { { "PrevBadReputation", "BadReputation" }, { "PrevStdShipAIProfile", "StdShipAIProfile" } };
	PyObject *name = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &name)) {
		return NULL;
	}
	Sf_Run(&f, "Scrap", "Set", "(si)", "isActionIndoor", 1);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Press.MentalControllerStatus", "1");
	for (int i = 0; i < 2 && !f.failed; i++) {
		if (Sf_Compare(&f, SAVE(prev[i][0]), "!=", Py_None) > 0) {
			Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", prev[i][1], SAVE(prev[i][0]));
			Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", prev[i][0]);
		}
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "WindowsError", "1");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "RobotsControlledByBoss", "0");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_BeforeFinalFight", "GamblinDen");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Map", "Levels/GamblinDen");
	Sf_Run(&f, "SaveGame", "QLoad", "(s)", "Levels/GamblinDen");
	return Sf_None(&f);
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
static PyObject *GoWithHuman_LoadScene2(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Pr_Scene(&f, GW, gowithhuman_scene2);
	return Sf_None(&f);
}

/* the boss on the news panel (FakeSpeech, made by CreateFakeSpeech) */
static PyObject *Pr_NormCamera(PyObject *self, PyObject *args) {
	static const char *const vars[] = { "NewsPanelDistance", "NewsPanelDisplacement", "NewsPanelMove", "NewsPanelX", "NewsPanelY", "NewsPanelW",
		"NewsPanelH" };
	static const int vals[] = { 200, 110, 0, 0, 0, 27, 18 };
	SF_ARGS("");
	for (int i = 0; i < 7; i++) {
		Sf_Run(&f, "Scrap", "Set", "(si)", vars[i], vals[i]);
	}
	Sf_Run(&f, "Scrap", "Set", "(ss)", "NewsHead", "FakeSpeech");
	PyObject *fs = ENT("FakeSpeech");
	Sf_SetAttrS(&f, fs, "ForceAction", "TalkAngry");
	Sf_SetAttrS(&f, fs, "ForceDefLoop", "TalkAngry");
	return Sf_None(&f);
}

/* SetTalk() */
static PyObject *Pr_SetTalk(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *fs = ENT("FakeSpeech");
	Sf_SetAttrS(&f, fs, "ForceAction", "Talk");
	Sf_SetAttrS(&f, fs, "ForceDefLoop", "Talk");
	return Sf_None(&f);
}

/* the boss' head for the news panel: an entity FakeSpeech (and Betty's FakeSpeech2 with talk2) */
static byte Pr_FakeSpeech(scriptFrame_t *f, const char *name, const char *ctrl, const char *talk) {
	int mark = f->n;
	PyObject *kb = Sf_Ask(f, "Scrap", "CreateEntity", "(siiis)", name, 2760, -1573, -1949, "WithLife");
	Sf_SetAttr(f, kb, "Ang", Sf_Build(f, "(iii)", 0, 0, 0));
	Sf_SetAttrS(f, kb, "ActCtrl", ctrl);
	if (talk) {
		Sf_SetAttrS(f, kb, "Action", talk);
		Sf_SetAttrS(f, kb, "DefLoop", talk);
	}
	Sf_Release(f, mark);
	return !f->failed;
}

/* Scrap.Set("NewsHead", "FakeSpeech"); Scrap.Set("NewsPanelShowHead", 1); NormCamera() */
static byte Pr_FakeSpeechShow(scriptFrame_t *f, const char *module) {
	Sf_Run(f, "Scrap", "Set", "(ss)", "NewsHead", "FakeSpeech");
	Sf_Run(f, "Scrap", "Set", "(si)", "NewsPanelShowHead", 1);
	int mark = f->n;
	Sf_Own(f, module, "NormCamera", "()");
	Sf_Release(f, mark);
	return !f->failed;
}

/* CreateFakeSpeech(): the boss on the news panel */
static PyObject *GoWithHuman_CreateFakeSpeech(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Pr_FakeSpeech(&f, "FakeSpeech", "Boss", NULL);
	Pr_FakeSpeechShow(&f, GW);
	return Sf_None(&f);
}

PyMethodDef g_PressGoWithHumanMethods[] = {
	{ "InitStatus", GoWithHuman_InitStatus, METH_VARARGS, NULL },
	{ "CreateHuman", GoWithHuman_CreateHuman, METH_VARARGS, NULL },
	{ "Init", GoWithHuman_Init, METH_VARARGS, NULL },
	{ "OnPossession", GoWithHuman_OnPossession, METH_VARARGS, NULL },
	{ "OnExitConversor", GoWithHuman_OnExitConversor, METH_VARARGS, NULL },
	{ "CheckMaxDist", GoWithHuman_CheckMaxDist, METH_VARARGS, NULL },
	{ "CheckHumanDist", GoWithHuman_CheckHumanDist, METH_VARARGS, NULL },
	{ "LoadNextMission", (PyCFunction)(void (*)(void))GoWithHuman_LoadNextMission, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "LoadScene2", GoWithHuman_LoadScene2, METH_VARARGS, NULL },
	{ "CreateFakeSpeech", GoWithHuman_CreateFakeSpeech, METH_VARARGS, NULL },
	{ "NormCamera", Pr_NormCamera, METH_VARARGS, NULL },
	{ "SetTalk", Pr_SetTalk, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_HumphreyArchbishopKilled ---- */

/* SetNextMission(usr): after Betty's news: the player in place, the archbishop's death in the temple next, the
 * characters back at home, the gambler's call in 2 s */
static PyObject *ArchbishopKilled_SetNextMission(PyObject *self, PyObject *args) {
	static const char *const vars[][2] = { { "MayorAtTownHall", "1" }, { "PoliceBossAtPolice", "1" }, { "BankMasterAtBank", "1" },
		{ "Journalist.Humphrey_Defaut", "Press" }, { "Journalist.Betty_Defaut", "Press" }, { "Journalist.Berto_Defaut", "Temple" } };
	PyObject *usr;
	SF_ARGS("O", &usr);
	Pr_UsrBack(&f, 5670, 163, -283, -110, 1.0);
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_ArchbishopDead", "Temple");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_ArchBishopKilled_MissionText1", 1);
	for (int i = 0; i < 6; i++) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", vars[i][0], vars[i][1]);
	}
	Sf_Run(&f, "SSound", "UsePS", "(i)", 1);
	Sf_Later(&f, Sf_Int(&f, 2), G(HK, "CGCalls"), NOARGS, NULL);
	Sf_Run(&f, "Scrap", "Set", "(si)", "isActionIndoor", 1);
	return Sf_None(&f);
}

/* CGCalls(): the crazy gambler's call for the number of crazy deals done (not in the twelfth deal) */
static PyObject *ArchbishopKilled_CGCalls(PyObject *self, PyObject *args) {
	static const char *const msgs[] = { "Mission_ArchbishopKilled_RemoteMessage3", "Mission_ArchbishopKilled_RemoteMessage4",
		"Mission_ArchbishopKilled_RemoteMessage5", "Mission_ArchbishopKilled_RemoteMessage6" };
	SF_ARGS("");
	if (Sf_Compare(&f, SAVE("SuperDeal.Library"), "!=", Sf_Str(&f, "SuperDeal_Twelfth")) > 0) {
		PyObject *num = Sf_Ask(&f, "MissionsFuncs", "GetNumCrazyDealCompleted", "()");
		for (int i = 0; i < 4 && !f.failed; i++) {
			if (Sf_EqI(&f, num, i) > 0) {
				Pr_Remote(&f, msgs[i], "CrazyGambler", NULL, NULL, 0);
				break;
			}
		}
	}
	return Sf_None(&f);
}

PyMethodDef g_PressHumphreyArchbishopKilledMethods[] = {
	{ "SetNextMission", ArchbishopKilled_SetNextMission, METH_VARARGS, NULL },
	{ "CGCalls", ArchbishopKilled_CGCalls, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_IntroHuman2 ---- */

/* LaunchHumphreySpeech(usr): after the news: Humphrey in front of the player tells about the human */
static PyObject *IntroHuman2_LaunchHumphreySpeech(PyObject *self, PyObject *args) {
	PyObject *usr;
	SF_ARGS("O", &usr);
	Sf_Run(&f, "Scrap", "Set", "(si)", "isActionIndoor", 1);
	Lv_Fade(&f, lv_black, lv_clear, 1.0);
	PyObject *h = ENT("Humphrey_Default");
	int both = Sf_True(&f, usr);
	if (both > 0) {
		both = Sf_True(&f, h);
	}
	if (both > 0) {
		PyObject *dir = Sf_Ask(&f, "SVec", "Rotate3D", "((ddd)O)", 0.0, 0.0, -1.0, Sf_Attr(&f, usr, "Ang"));
		PyObject *pu = Sf_Attr(&f, usr, "Pos");
		PyObject *d = Sf_OpI(&f, Sf_Op(&f, Sf_Attr(&f, usr, "VRadius"), '+', Sf_Attr(&f, h, "VRadius")), '+', 50);
		Sf_SetAttr(&f, h, "Pos", Sf_Ask(&f, "SVec", "Add", "(OO)", pu, Sf_Ask(&f, "SVec", "Prod", "(OO)", dir, d)));
		PyObject *a0 = Sf_Item(&f, Sf_Attr(&f, usr, "Ang"), 0);
		PyObject *a1 = Sf_OpI(&f, Sf_Item(&f, Sf_Attr(&f, usr, "Ang"), 1), '+', 180);
		PyObject *a2 = Sf_Item(&f, Sf_Attr(&f, usr, "Ang"), 2);
		Sf_SetAttr(&f, h, "Ang", Sf_Build(&f, "(OOO)", a0, a1, a2));
		Lv_OnEnd(&f, IH, "SetNextMission", NULL);
		Lv_Speakers(&f, Sf_Attr(&f, usr, "Name"), Sf_Attr(&f, h, "Name"));
		Sf_Run(&f, "Speech", "NISpeech", "(s)", "Humphrey");
		Sf_Run(&f, "Speech", "AddNI", "(ss)", "Mission_IntroHuman_Humphrey_InitMsg1", "Mission_IntroHuman_Humphrey_InitAns1");
		Sf_Run(&f, "Speech", "RunNI", "()");
	}
	return Sf_None(&f);
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
static PyObject *IntroHuman2_LoadScene(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Pr_Scene(&f, IH, introhuman2_scene);
	return Sf_None(&f);
}

PyMethodDef g_PressIntroHuman2Methods[] = {
	{ "LaunchHumphreySpeech", IntroHuman2_LaunchHumphreySpeech, METH_VARARGS, NULL },
	{ "LoadScene", IntroHuman2_LoadScene, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_PressMeeting ---- */

/* SetNextMission(usr): after the meeting: saved, the mayor next in DownTown, Betty's ship parked at the press */
static PyObject *PressMeeting_SetNextMission(PyObject *self, PyObject *args) {
	PyObject *usr;
	SF_ARGS("O", &usr);
	Sf_Run(&f, "Scrap", "Set", "(si)", "isActionIndoor", 1);
	Lv_Fade(&f, lv_black, lv_clear, 1.0);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_FaceTheMayor", "DownTown");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_FaceTheMayor_MissionText1", 1);
	Sf_Run(&f, "SSound", "UsePS", "(i)", 1);
	Lv_SaveAppend(&f, "Parked.Press", Lv_SaveList(&f, "Parked.Press", PM), Sf_Str(&f, "SBETTY1"));
	return Sf_None(&f);
}

PyMethodDef g_PressPressMeetingMethods[] = {
	{ "SetNextMission", PressMeeting_SetNextMission, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_MeetBoss ---- */

/* InitStatus(LevelPath): the callbacks after the NPCs, Betty's resources, Berto waits for Dtritus */
static PyObject *MeetBoss_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "CharsNPC");
	Sf_Import(&f, "Chars");
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "AfterBrowseNPC"), "append", "(O)", G(MB, "AfterBrowseNPC"));
	Sf_Run(&f, "Chars", "AddNewCharRes", "(s)", "Betty");
	Lv_MissionChar(&f, "Berto_MeetBos", MB, "CreateBertoInit");
	Sf_Run(&f, "Scrap", "Set", "(si)", "DefActionRadius", 1000);
	return Sf_None(&f);
}

/* AfterBrowseNPC(): Humphrey's callback; no messengers out of the demo */
static PyObject *MeetBoss_AfterBrowseNPC(PyObject *self, PyObject *args) {
	static const char *const msgs[] = { "Messenger_01", "Messenger_02" };
	SF_ARGS("");
	Sf_Import(&f, "Init");
	Lv_Callback(&f, "Humphrey_Default", MB, "CreateHumphreyInit");
	if (Sf_True(&f, Sf_Mod(&f, "Init", "isDemo")) == 0) {
		for (int i = 0; i < 2 && !f.failed; i++) {
			int mark = f.n;
			if (Sf_True(&f, Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), "has_key", "(s)", msgs[i])) > 0) {
				Sf_DelItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, msgs[i]));
			}
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/* CreateHumphreyInit(h) */
static PyObject *MeetBoss_CreateHumphreyInit(PyObject *self, PyObject *args) {
	PyObject *h;
	SF_ARGS("O", &h);
	if (Lv_SaveIs(&f, "MissionState", "ChallengeHumphrey") > 0) {
		Sf_SetAttrS(&f, h, "UseFunc", "Mission_MeetBoss.HumphreySpeech");
	}
	return Sf_None(&f);
}

/* CreateBertoInit(e): Berto talks and stays where he is */
static PyObject *MeetBoss_CreateBertoInit(PyObject *self, PyObject *args) {
	PyObject *e;
	SF_ARGS("O", &e);
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_MeetBoss.BertoSpeech");
	Lv_Stay(&f, e, 6, Sf_Str(&f, ""));
	return Sf_None(&f);
}

/* BertoSpeech(UsrName, CharName, optId, FullOption): the first talk starts the prolog (Berto goes, the camera of
 * Sebastian), later the usual talk */
static PyObject *MeetBoss_BertoSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	int first = Sf_True(&f, G(MB, "FirstBertoSpeech"));
	if (first > 0) {
		Sf_SetGlobal(&f, MB, "FirstBertoSpeech", Sf_Int(&f, 0));
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
		Pr_EntS(&f, "DM_Door_Metro_Press", "UseFunc", "Mission_MeetBoss.OnTryingToExit2");
		Pr_EntS(&f, "DM_Door_Elevator_Press", "OnObjSlot", "!CloseDoorSlot");
		Pr_DelIf(&f, Sf_Str(&f, "Berto_MeetBos"));
		Sf_SetAttrI(&f, G(MB, "sebastian"), "Active", 1);
		Sf_Own(&f, MB, "PlayProlog0", "()");
	} else if (first == 0) {
		Sf_Run(&f, "Speech", "AutoSpeech", "(OOOO)", usr, ch, opt, full);
	}
	return Sf_None(&f);
}

/* BertoSpeech2(UsrName, CharName, optId, FullOption): Berto after his possession, or not yet */
static PyObject *MeetBoss_BertoSpeech2(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Lv_Speakers(&f, usr, ch);
	int done = Lv_SaveIs(&f, "Stats.PossessBerto", "1");
	if (done > 0) {
		Sf_Run(&f, "Speech", "AutoSpeech", "(OOOO)", usr, ch, opt, full);
	} else if (done == 0) {
		Sf_Run(&f, "Speech", "RestoreState", "(O)", ch);
		Pr_Spch(&f, "Mission_FirstCrazyBet_Humphrey_InitMsg2", "Mission_FirstCrazyBet_Humphrey_InitAns2");
	}
	return Sf_None(&f);
}

/* the emergency door and its warning lights: closed or automatic */
static PyObject *Pr_Emero(PyObject *args, const char *slot) {
	static const char *const names[] = { "DM_Door_PressDoorEmero_00", "DM_Door_PressWarningUpR_Press", "DM_Door_PressWarningDown_Press",
		"DM_Door_PressWarningUpL_Press" };
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	for (int i = 0; i < 4; i++) {
		Pr_EntS(&f, names[i], "OnObjSlot", slot);
	}
	return Sf_None(&f);
}

/* DeactivateEmero() */
static PyObject *MeetBoss_DeactivateEmero(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_Emero(args, "!CloseDoorSlot");
}

/* ActivateEmero() */
static PyObject *MeetBoss_ActivateEmero(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_Emero(args, "!AutoDoorSlot");
}

/* the exits of the press (metro and elevator) lead to the function OnTryingToExit<n> */
static byte Pr_Exits(scriptFrame_t *f, const char *use) {
	Pr_EntS(f, "DM_Door_Metro_Press", "UseFunc", use);
	return Pr_EntS(f, "DM_Door_Elevator_Press", "UseFunc", use);
}

/* Init(LevelPath): Sebastian's head, the emergency door closed, the mission where it was saved: the prolog with
 * Berto, the battery, the conversion into Dtritus or the challenge to Humphrey */
static PyObject *MeetBoss_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "SScorer", "AddModel", "(issssiiii)", 0, "Sebastian", "Models/Chars/Sebastian/Sebastian", "Models/Chars/Sebastian/Anm/Sebastian",
		3, 25, -10, 1);
	PyObject *state = SAVE("MissionState");
	Sf_Own(&f, MB, "DeactivateEmero", "()");
	if (Sf_Compare(&f, state, "!=", Sf_Str(&f, "MeetBoss")) > 0) {
		Pr_DelIf(&f, Sf_Str(&f, "Berto_MeetBos"));
		Sf_Run(&f, "CharsNPC", "CreateChar", "(O)", Sf_Ask(&f, "CharsNPC", "CreateMissionChar", "(s)", "Berto_Default"));
	}
	Sf_SetGlobal(&f, MB, "sebastian", ENT("Sebastian_Camera"));
	if (Sf_EqS(&f, state, "MeetBoss") > 0) {
		Sf_Import(&f, "Init");
		Sf_Import(&f, "Elevator");
		Pr_Exits(&f, "Mission_MeetBoss.OnTryingToExit");
		Sf_Own(&f, MB, "LoadProlog0Scene", "()");
		Sf_Own(&f, MB, "LoadProlog1Scene", "()");
		Sf_Own(&f, MB, "LoadProlog2Scene", "()");
		Pr_EntS(&f, "DM_Door_PressSlidingDoor_01", "OnObjSlot", "");
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Berto_MeetBos");
		Sf_SetAttrI(&f, G(MB, "sebastian"), "Active", 0);
		Sf_Run(&f, "Scrap", "Set", "(si)", "GenCharAttackActive", 0);
		int demo = Sf_True(&f, Sf_Mod(&f, "Init", "isDemo"));
		if (demo > 0) {
			Sf_Schedule(&f, Sf_Int(&f, 5), Sf_Mod(&f, "SScorer", "Fade"), Sf_Build(&f, "(i(iiii)(iiii)d)", 0, 0, 0, 0, 0, 0, 0, 0, 255, 0.5), NULL);
			Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", G(MB, "EndOfMetroAnim"));
		} else if (demo == 0) {
			Pr_Remote(&f, "Mission_FirstContactPress_RemoteMessage2", "Berto", NULL, NULL, 0);
		}
	} else if (Sf_EqS(&f, state, "GetBattery") > 0) {
		Sf_Import(&f, "Chars");
		Sf_SetMod(&f, "Chars", "OnMainCharDeath", G(MB, "OnMainCharDeath"));
		Pr_Exits(&f, "Mission_MeetBoss.OnTryingToExit2");
		Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetBatteries_MissionText1", 1);
		if (Lv_SaveIs(&f, "PlayerHasBeenKilled", "1") > 0) {
			Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "PlayerHasBeenKilled", "0");
			Pr_Remote(&f, "Mission_DestroyPolice_RemoteMessage5", "Bishop", NULL, NULL, 0);
		}
		Sf_Own(&f, MB, "CreateBattery", "()");
		Sf_Run(&f, "Scrap", "Set", "(si)", "GenCharAttackActive", 0);
	} else if (Sf_EqS(&f, state, "ConvertIntoDtritus") > 0) {
		Pr_Exits(&f, "Mission_MeetBoss.OnTryingToExit3");
		Pr_Remote(&f, "Mission_FirstCrazyBet_RemoteMessage1", "CrazyGambler", MB, "UseConversor", 1);
	} else if (Sf_EqS(&f, state, "ChallengeHumphrey") > 0) {
		Pr_Exits(&f, "Mission_MeetBoss.OnTryingToExit3");
		Sf_Own(&f, MB, "ChallengeHumphrey", "()");
	}
	return Sf_None(&f);
}

/* the optional usr of the prolog functions */
static int Pr_UsrArg(PyObject *args, PyObject *kw) {
	static char *names[] = { "usr", NULL };
	PyObject *usr = NULL;
	return PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr);
}

/* PlayProlog0(usr=None): the player at the start of the prolog; the gatekeeper of the boss room comes alive */
static PyObject *MeetBoss_PlayProlog0(PyObject *self, PyObject *args, PyObject *kw) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!Pr_UsrArg(args, kw)) {
		return NULL;
	}
	PyObject *ch = USR();
	Sf_SetAttr(&f, ch, "Pos", Sf_Build(&f, "(iii)", -1114, -312, 7562));
	Sf_SetAttr(&f, ch, "Ang", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_Run(&f, "Scene", "Play", "(s)", "Prolog0");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", G(MB, "PlayProlog1"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	PyObject *gc = ENT("GateKeeper_DTritusBossRoom");
	Sf_SetAttr(&f, gc, "TimeSpeed", Sf_Float(&f, 1.0));
	Sf_SetAttrI(&f, gc, "Active", 1);
	return Sf_None(&f);
}

/* PlayProlog1(usr=None) */
static PyObject *MeetBoss_PlayProlog1(PyObject *self, PyObject *args, PyObject *kw) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!Pr_UsrArg(args, kw)) {
		return NULL;
	}
	Pr_EntS(&f, "DM_Door_PressSlidingDoor_01", "OnObjSlot", "!AutoDoorUsrSlot");
	Pr_EntS(&f, "DM_Door_PressSlidingDoor_00", "OnObjSlot", "");
	Lv_Fade(&f, lv_black, lv_clear, 1.0);
	Sf_Run(&f, "Scene", "Play", "(s)", "Prolog1");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", G(MB, "PlayProlog2"));
	return Sf_None(&f);
}

/* PlayProlog2(usr=None) */
static PyObject *MeetBoss_PlayProlog2(PyObject *self, PyObject *args, PyObject *kw) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!Pr_UsrArg(args, kw)) {
		return NULL;
	}
	Sf_Run(&f, "SSound", "SetMusic", "(sd)", "Music/Track1.ogg", 1.0);
	Pr_EntS(&f, "DM_Door_PressSlidingDoor_00", "OnObjSlot", "!AutoDoorUsrSlot");
	Lv_Fade(&f, lv_black, lv_clear, 1.0);
	Sf_Run(&f, "Scene", "Play", "(s)", "Prolog2");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", G(MB, "AssignFirstMission"));
	return Sf_None(&f);
}

/* the action and possession keys of Dtritus' class */
static byte Pr_DtritusKeys(scriptFrame_t *f, const char *action, const char *possession) {
	Sf_Run(f, "SAct", "GetClass", "(s)", "Dtritus");
	Sf_Run(f, "SAct", "SetCls", "(ss)", "ActionPress", action);
	return Sf_Run(f, "SAct", "SetCls", "(ss)", "PossessionPress", possession);
}

/* the energy bar and the special action on (1) or off (0), now and in the save game */
static byte Pr_EnergyBar(scriptFrame_t *f, int on) {
	Sf_Run(f, "Scrap", "Set", "(si)", "EnergyBarActive", on);
	Sf_Run(f, "Scrap", "SetSaveVar", "(ss)", "EnergyBarActive", on ? "1" : "0");
	Sf_Run(f, "Scrap", "Set", "(si)", "SpecialActionActive", on);
	return Sf_Run(f, "Scrap", "SetSaveVar", "(ss)", "SpecialActionActive", on ? "1" : "0");
}

/* CreateBattery(): the battery to take is the target; no keys, no energy bar meanwhile */
static PyObject *MeetBoss_CreateBattery(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "MisItems");
	Sf_Import(&f, "MissionsFuncs");
	PyObject *b = Sf_Ask(&f, "MisItems", "CreateBattery", "((iii)O)", 2619, 106, 3165, G(MB, "AssingOnTakeBattery"));
	PyObject *pos = Sf_Attr(&f, b, "Pos");
	PyObject *name = Sf_Cat(&f, "Os", Sf_Attr(&f, b, "Name"), "CheckArrow");
	PyObject *b2 = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "WithLife");
	Sf_SetAttr(&f, b2, "CamTgt", Sf_Build(&f, "(iii)", 0, 100, 0));
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "DeleteCoin");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, b2, "Name"));
	Pr_DtritusKeys(&f, "", "");
	Pr_EnergyBar(&f, 0);
	return Sf_None(&f);
}

/* AssingOnTakeBattery(l) */
static PyObject *MeetBoss_AssingOnTakeBattery(PyObject *self, PyObject *args) {
	PyObject *l;
	SF_ARGS("O", &l);
	Sf_SetAttrS(&f, l, "OnTake", "Mission_MeetBoss.OnTakeBattery");
	return Sf_None(&f);
}

/* OnTakeBattery(ItemName, CharName): the battery taken: the action key and the energy bar on, Berto back, the
 * messages of Sebastian and the gambler */
static PyObject *MeetBoss_OnTakeBattery(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	SF_ARGS("OO", &item, &ch);
	Sf_Import(&f, "Scorer");
	Sf_Import(&f, "Sound");
	Pr_Exits(&f, "Mission_MeetBoss.OnTryingToExit3");
	Pr_Taken(&f, Sf_Entity(&f, item), "TakeBattery");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_SetAttrI(&f, USR(), "Energy", 0);
	Pr_DtritusKeys(&f, "Mission_MeetBoss.OnActionPressed", "");
	Pr_EnergyBar(&f, 1);
	Sf_Run(&f, "Scorer", "Blink", "(is)", 5, "SpecialAction");
	if (Sf_True(&f, Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), "has_key", "(s)", "Berto_Default")) == 0) {
		Sf_Run(&f, "CharsNPC", "CreateChar", "(O)", Sf_Ask(&f, "CharsNPC", "CreateMissionChar", "(s)", "Berto_Default"));
	}
	Pr_Remote(&f, "Mission_GetBatteries_RemoteMessage1", "Sebastian", NULL, NULL, 0);
	Pr_Remote(&f, "Mission_GetBatteries_RemoteMessage4", "CrazyGambler", MB, "PreFirstCrazyDeal", 0);
	return Sf_None(&f);
}

/* PreBertoPossession(): the special action level 2, Berto's own talk, then the possession task */
static PyObject *MeetBoss_PreBertoPossession(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "DTritusDesktop");
	Sf_Run(&f, "Scrap", "Set", "(si)", "SpecialActionActive", 2);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "SpecialActionActive", "2");
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", Py_None);
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_FirstCrazyBet_MissionText1", 1);
	PyObject *e = ENT("Berto_Default");
	if (Sf_True(&f, e) > 0) {
		Sf_SetAttrS(&f, e, "UseFunc", "Mission_MeetBoss.BertoSpeech2");
	}
	Sf_Own(&f, MB, "BertoPossession", "()");
	return Sf_None(&f);
}

/* BertoPossession(): the three crazy deals, the first super deal, the possession key, the warnings every 20 s */
static PyObject *MeetBoss_BertoPossession(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "JackInTheBox.Active", "1");
	Sf_Run(&f, "MissionsFuncs", "NewCrazyDeal", "(isi)", 1, "Stats.PossessBerto", 1);
	Sf_Run(&f, "MissionsFuncs", "NewCrazyDeal", "(isi)", 2, "Stats.ConvertIntoDtritus", 1);
	Sf_Run(&f, "MissionsFuncs", "NewCrazyDeal", "(isi)", 3, "Stats.WinHumphreyRace", 1);
	Sf_Run(&f, "MissionsFuncs", "SetNextSuperDeal", "(sss)", "CA", "SuperDeal_First", "Versus");
	Pr_DtritusKeys(&f, "", "Mission_MeetBoss.OnPossession");
	Pr_Msg(&f, Pr_KeyText(&f, "Mission_FirstCrazyBet_SystemMessage2", "Possession"));
	Sf_Later(&f, Sf_Int(&f, 20), G(MB, "OnWarning"), NOARGS, "Warnings");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Berto_Default");
	return Sf_None(&f);
}

/* OnWarning() */
static PyObject *MeetBoss_OnWarning(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Pr_Remote(&f, "Mission_FirstCrazyBet_RemoteMessage2", "CrazyGambler", NULL, NULL, 0);
	Sf_Later(&f, Sf_Int(&f, 20), G(MB, "OnWarning"), NOARGS, "Warnings");
	return Sf_None(&f);
}

/* OnActionPressed(CharName) */
static PyObject *MeetBoss_OnActionPressed(PyObject *self, PyObject *args) {
	PyObject *ch;
	SF_ARGS("O", &ch);
	Pr_Msg(&f, LANG("Mission_TalkToCrazy_SystemMessage1"));
	return Sf_None(&f);
}

/* OnPossession(UserName, PossessedName): only Berto counts: the alarm, the possession, the warnings stop (the queued
 * one too), the alarm's end hook */
static PyObject *MeetBoss_OnPossession(PyObject *self, PyObject *args) {
	PyObject *user, *name;
	SF_ARGS("OO", &user, &name);
	Sf_Import(&f, "Alarm");
	Sf_Import(&f, "CharConversor");
	Sf_Import(&f, "Speech");
	PyObject *b = Sf_Entity(&f, name);
	int conv = Sf_In(&f, Sf_Attr(&f, b, "ActCtrl"), Sf_Mod(&f, "CharConversor", "ConversionChars"));
	if (conv == 0) {
		Pr_Msg(&f, LANG("Generics_ForbiddenPossession_SystemMessage"));
		return Sf_None(&f);
	}
	int berto = Sf_EqS(&f, Sf_Attr(&f, b, "ActCtrl"), "Berto");
	if (berto == 0) {
		Pr_Remote(&f, "Mission_FirstCrazyBet_RemoteMessage2", "CrazyGambler", NULL, NULL, 0);
	}
	if (berto <= 0) {
		return Sf_None(&f);
	}
	Sf_SetGlobal(&f, MB, "alarmFallDelta", Sf_Ask(&f, "Scrap", "Get", "(s)", "AlarmFallDelta"));
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 1);
	Sf_Run(&f, "Scrap", "Set", "(si)", "AlarmFallDelta", 0);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Pr_DtritusKeys(&f, "!DtritusActionPress", "CharConversor.Possession");
	Sf_Run(&f, "SAct", "GetClass", "(s)", "Berto");
	Sf_Run(&f, "SAct", "SetCls", "(ss)", "ActionPress", "");
	Sf_Run(&f, "CharConversor", "Possession", "(OO)", user, name);
	Pr_Remote(&f, "Mission_FirstCrazyBet_RemoteMessage3", "CrazyGambler", MB, "ShowMessage", 1);
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "Warnings");
	PyObject *key = Py_None;
	int n = Sf_Len(&f, Sf_Mod(&f, "Speech", "RemoteCom"));
	for (int i = 0; i < n && !f.failed; i++) {
		int mark = f.n;
		PyObject *rc = Sf_Item(&f, Sf_Mod(&f, "Speech", "RemoteCom"), i);
		if (Sf_EqS(&f, Sf_Item(&f, rc, 0), "Mission_FirstCrazyBet_RemoteMessage2") > 0) {
			key = Sf_Item(&f, Sf_Mod(&f, "Speech", "RemoteCom"), i);
			Sf_ReleaseKeep(&f, mark, &key, 1);
		} else {
			Sf_Release(&f, mark);
		}
	}
	if (Sf_True(&f, key) > 0) {
		Sf_Method(&f, Sf_Mod(&f, "Speech", "RemoteCom"), "remove", "(O)", key);
	}
	Sf_Method(&f, Sf_Mod(&f, "Alarm", "SetAlarmCallback"), "append", "(O)", G(MB, "OnAlarmEnd"));
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MissionState", "ConvertIntoDtritus");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Stats.PossessBerto", "1");
	return Sf_None(&f);
}

/* ShowMessage(): the action key of Berto explained; the alarm falls again */
static PyObject *MeetBoss_ShowMessage(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Pr_Msg(&f, Pr_KeyText(&f, "Mission_FirstCrazyBet_SystemMessage1", "Action"));
	Sf_Run(&f, "SAct", "GetClass", "(s)", "Berto");
	Sf_Run(&f, "SAct", "SetCls", "(ss)", "ActionPress", "!BertoActionPress");
	Sf_Run(&f, "Scrap", "Set", "(sO)", "AlarmFallDelta", G(MB, "alarmFallDelta"));
	return Sf_None(&f);
}

/* OnAlarmEnd(status): when the alarm ends the gambler sends Dtritus to the conversor */
static PyObject *MeetBoss_OnAlarmEnd(PyObject *self, PyObject *args) {
	PyObject *status;
	SF_ARGS("O", &status);
	Sf_Import(&f, "Alarm");
	int on = Sf_EqI(&f, status, 1);
	if (on > 0) {
		Sf_Method(&f, Sf_Mod(&f, "Alarm", "SetAlarmCallback"), "append", "(O)", G(MB, "OnAlarmEnd"));
	} else if (on == 0) {
		PyObject *a = Sf_Build(&f, "(ssO())", "Mission_FirstCrazyBet_RemoteMessage1", "CrazyGambler", G(MB, "UseConversor"));
		Sf_Later(&f, Sf_Int(&f, 1), Sf_Mod(&f, "Speech", "AddRemoteCom"), a, NULL);
	}
	return Sf_None(&f);
}

/* set the use of every conversion point */
static byte Pr_Conversors(scriptFrame_t *f, const char *use) {
	PyObject *points = Sf_Mod(f, "CharConversor", "ConversionPoints");
	for (int i = 0; !f->failed; i++) {
		int mark = f->n;
		PyObject *cc = Sf_Next(f, points, i);
		if (!cc) {
			break;
		}
		Sf_SetAttrS(f, cc, "UseFunc", use);
		Sf_Release(f, mark);
	}
	return !f->failed;
}

/* UseConversor(): the conversors on, the fourth one the target */
static PyObject *MeetBoss_UseConversor(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_True(&f, USR()) > 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Conversor.ActiveConversors", "1");
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "DM_Conversion_Point_04");
		Pr_Conversors(&f, "Mission_MeetBoss.OnConversorUsed");
		Sf_SetMod(&f, "CharConversor", "afterExitConversor", G(MB, "CheckConversion"));
	}
	return Sf_None(&f);
}

/* OnConversorUsed(UsrName, CharName, optId, FullOption): only the target conversor works */
static PyObject *MeetBoss_OnConversorUsed(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	int target = Sf_EqI(&f, Sf_Attr(&f, Sf_Entity(&f, ch), "RadarShow"), 1);
	if (target > 0) {
		Sf_Run(&f, "CharConversor", "EnterCharConversor", "(OOOO)", usr, ch, opt, full);
	} else if (target == 0) {
		Pr_Remote(&f, "Mission_FirstCrazyBet_RemoteMessage1", "CrazyGambler", NULL, NULL, 0);
	}
	return Sf_None(&f);
}

/* CheckConversion(): out of the conversor as Dtritus: the conversors as usual, on to Humphrey */
static PyObject *MeetBoss_CheckConversion(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_EqS(&f, Sf_Attr(&f, USR(), "ActCtrl"), "Dtritus") > 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Stats.ConvertIntoDtritus", "1");
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
		Pr_Conversors(&f, "CharConversor.EnterCharConversor");
		Sf_SetMod(&f, "CharConversor", "afterExitConversor", Py_None);
		Pr_Remote(&f, "Mission_FirstCrazyBet_RemoteMessage4", "CrazyGambler", MB, "PreChallengeHumphrey", 0);
	}
	return Sf_None(&f);
}

/* ChallengeHumphrey(): Humphrey is the target */
static PyObject *MeetBoss_ChallengeHumphrey(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "Humphrey_Default");
	PyObject *h = ENT("Humphrey_Default");
	if (Sf_True(&f, h) > 0) {
		Sf_SetAttrS(&f, h, "UseFunc", "Mission_MeetBoss.HumphreySpeech");
	}
	return Sf_None(&f);
}

/* HumphreySpeech(UsrName, CharName, optId, FullOption): Humphrey takes the challenge from a driver */
static PyObject *MeetBoss_HumphreySpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_Import(&f, "Chars");
	int drives = Sf_In(&f, Sf_Attr(&f, Sf_Entity(&f, usr), "ActCtrl"), Sf_Mod(&f, "Chars", "CanDriveList"));
	if (drives > 0) {
		Lv_Speakers(&f, usr, ch);
		Lv_OnEnd(&f, MB, "SetNextMission", NULL);
		Sf_Run(&f, "Speech", "NISpeech", "(s)", "Humphrey");
		Sf_Run(&f, "Speech", "AddNI", "(ss)", "Mission_FirstCrazyBet_Humphrey_InitMsg1", "Mission_FirstCrazyBet_Humphrey_InitAns1");
		Sf_Run(&f, "Speech", "RunNI", "()");
	} else if (drives == 0) {
		Pr_Remote(&f, "Mission_FirstCrazyBet_RemoteMessage10", "CrazyGambler", NULL, NULL, 0);
	}
	return Sf_None(&f);
}

/* SetNextMission(): the race against Humphrey next (in Outskirts in the demo), the crazy wind on; a new game from
 * the way point with a character that can drive */
static PyObject *MeetBoss_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Init");
	Sf_Own(&f, MB, "ActivateEmero", "()");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "CrazyWing.Status", "1");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "PlayerHasBeenKilled");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	int demo = Sf_True(&f, Sf_Mod(&f, "Init", "isDemo"));
	if (demo > 0) {
		Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_Demo_HumphreyRace", "Outskirts");
		Sf_Run(&f, "SaveGame", "MarkWayPoint", "(ss)", "Levels/Outskirts", "DM_Player_Spawn_GamblinDen");
	} else if (demo == 0) {
		Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_HumphreyRace", "DownTown");
		PyObject *spawn = Sf_Cat(&f, "sO", "DM_Player_Spawn_", Sf_Slice(&f, Sf_Mod(&f, "Init", "Path"), 7, 0x7fffffff));
		Sf_Run(&f, "SaveGame", "MarkWayPoint", "(sO)", "Levels/DownTown", spawn);
	}
	Sf_Import(&f, "Chars");
	int drives = Sf_In(&f, SAVE("Char"), Sf_Mod(&f, "Chars", "CanDriveList"));
	if (drives == 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Dtritus");
	}
	Sf_Run(&f, "SaveGame", "StartGame", "()");
	return Sf_None(&f);
}

PyMethodDef g_PressMeetBossMethods[] = {
	{ "InitStatus", MeetBoss_InitStatus, METH_VARARGS, NULL },
	{ "AfterBrowseNPC", MeetBoss_AfterBrowseNPC, METH_VARARGS, NULL },
	{ "CreateHumphreyInit", MeetBoss_CreateHumphreyInit, METH_VARARGS, NULL },
	{ "CreateBertoInit", MeetBoss_CreateBertoInit, METH_VARARGS, NULL },
	{ "BertoSpeech", MeetBoss_BertoSpeech, METH_VARARGS, NULL },
	{ "BertoSpeech2", MeetBoss_BertoSpeech2, METH_VARARGS, NULL },
	{ "DeactivateEmero", MeetBoss_DeactivateEmero, METH_VARARGS, NULL },
	{ "ActivateEmero", MeetBoss_ActivateEmero, METH_VARARGS, NULL },
	{ "Init", MeetBoss_Init, METH_VARARGS, NULL },
	{ "PlayProlog0", (PyCFunction)(void (*)(void))MeetBoss_PlayProlog0, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "PlayProlog1", (PyCFunction)(void (*)(void))MeetBoss_PlayProlog1, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "PlayProlog2", (PyCFunction)(void (*)(void))MeetBoss_PlayProlog2, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CreateBattery", MeetBoss_CreateBattery, METH_VARARGS, NULL },
	{ "AssingOnTakeBattery", MeetBoss_AssingOnTakeBattery, METH_VARARGS, NULL },
	{ "OnTakeBattery", MeetBoss_OnTakeBattery, METH_VARARGS, NULL },
	{ "PreBertoPossession", MeetBoss_PreBertoPossession, METH_VARARGS, NULL },
	{ "BertoPossession", MeetBoss_BertoPossession, METH_VARARGS, NULL },
	{ "OnWarning", MeetBoss_OnWarning, METH_VARARGS, NULL },
	{ "OnActionPressed", MeetBoss_OnActionPressed, METH_VARARGS, NULL },
	{ "OnPossession", MeetBoss_OnPossession, METH_VARARGS, NULL },
	{ "ShowMessage", MeetBoss_ShowMessage, METH_VARARGS, NULL },
	{ "OnAlarmEnd", MeetBoss_OnAlarmEnd, METH_VARARGS, NULL },
	{ "UseConversor", MeetBoss_UseConversor, METH_VARARGS, NULL },
	{ "OnConversorUsed", MeetBoss_OnConversorUsed, METH_VARARGS, NULL },
	{ "CheckConversion", MeetBoss_CheckConversion, METH_VARARGS, NULL },
	{ "ChallengeHumphrey", MeetBoss_ChallengeHumphrey, METH_VARARGS, NULL },
	{ "HumphreySpeech", MeetBoss_HumphreySpeech, METH_VARARGS, NULL },
	{ "SetNextMission", MeetBoss_SetNextMission, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_TalkToBoss ---- */

/* InitStatus(LevelPath): freeing the human: the human at the bureau (by the elevator or the metro), the alarm on;
 * the boss' callback */
static PyObject *TalkToBoss_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	if (Lv_SaveIs(&f, "MissionState", "FreeHuman2") > 0) {
		Lv_MissionChar(&f, "Human_Default", TB, "CreateHuman");
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "AlarmActive", "1");
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "AlarmStatus", "1");
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Dtritus");
		int elevator = Lv_SaveIs(&f, "ComeFrom", "DoorElevator");
		if (elevator > 0) {
			Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "DoorElevator");
			Sf_SetAttr(&f, Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "Human_Default")), "Pos", Sf_Build(&f, "(iii)", -1361, -1285, -1130));
			Sf_SetAttr(&f, Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "Human_Default")), "Ang", Sf_Build(&f, "(iii)", 0, 0, 0));
		} else if (elevator == 0) {
			Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "DoorMetro");
		}
	}
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "AfterBrowseNPC"), "append", "(O)", G(TB, "AfterBrowseNPC"));
	return Sf_None(&f);
}

/* AfterBrowseNPC() */
static PyObject *TalkToBoss_AfterBrowseNPC(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "CharsNPC");
	Lv_Callback(&f, "Boss_00", TB, "SetBossSpeech");
	return Sf_None(&f);
}

/* SetBossSpeech(e) */
static PyObject *TalkToBoss_SetBossSpeech(PyObject *self, PyObject *args) {
	PyObject *e;
	SF_ARGS("O", &e);
	if (Lv_SaveIs(&f, "MissionState", "TalkToBoss") > 0) {
		Sf_SetAttrS(&f, e, "UseFunc", "Mission_TalkToBoss.BossSpeech");
	}
	return Sf_None(&f);
}

/* CreateHuman(e) */
static PyObject *TalkToBoss_CreateHuman(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_CreateHuman(args, TB, 1);
}

/* Init(LevelPath): Rusty's head; the human to hide in the bureau, or the talk with the boss */
static PyObject *TalkToBoss_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "SScorer", "AddModel", "(isssii)", 0, "Rusty", "Models/Heads/HeadRusty/HeadRusty", "", 3, 25);
	int free = Lv_SaveIs(&f, "MissionState", "FreeHuman2");
	if (free > 0) {
		Sf_Import(&f, "MissionsFuncs");
		Sf_Own(&f, TB, "LoadScene", "()");
		Sf_Own(&f, TB, "HideHumanIntoBureau", "()");
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(TB, "human"), 2);
	} else if (free == 0) {
		Sf_Own(&f, TB, "GoTalkToBoss", "()");
	}
	return Sf_None(&f);
}

/* HideHumanIntoBureau(): the bureau is the target and closed; the human follows Dtritus, the alarm does not fall */
static PyObject *TalkToBoss_HideHumanIntoBureau(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "DM_Door_Bureau_00");
	PyObject *bure = ENT("DM_Door_Bureau_00");
	Sf_SetAttrS(&f, bure, "OnObjSlot", "");
	Sf_SetAttrS(&f, bure, "Action", "Closed");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) > 0) {
		PyObject *hn = Sf_Attr(&f, G(TB, "human"), "Name");
		Sf_Run(&f, "SAI", "SetStateChar", "(iOOd)", 3, hn, Sf_Attr(&f, usr, "Name"), 1.0);
		Sf_SetMod(&f, "CharConversor", "OnPossession", G(TB, "OnPossession"));
		Sf_SetMod(&f, "CharConversor", "afterExitConversor", G(TB, "OnExitConversor"));
		Sf_Own(&f, TB, "CreateSceneLaunchers", "()");
		Sf_Own(&f, TB, "CheckMaxDist", "()");
		Sf_SetGlobal(&f, TB, "alarmFallDelta", Sf_Ask(&f, "Scrap", "Get", "(s)", "AlarmFallDelta"));
		Sf_Run(&f, "Scrap", "Set", "(si)", "AlarmFallDelta", 0);
	}
	return Sf_None(&f);
}

/* CreateSceneLaunchers(): an item at each door of the bureau room starts the scene when the human gets there */
static PyObject *TalkToBoss_CreateSceneLaunchers(PyObject *self, PyObject *args) {
	SF_ARGS("");
	for (int i = 0; i < 2 && !f.failed; i++) {
		int mark = f.n;
		Sf_Run(&f, "Scrap", "StartDummySearch", "(Oi)", Sf_Cat(&f, "sO", "DM_Bureau_Room_Door_", Sf_ReprI(&f, i + 1)), 1);
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_True(&f, dummy) == 0) {
			PyObject *msg = Sf_Cat(&f, "sO", "No se encuentra el dummy: DM_Bureau_Room_Door_", Sf_ReprI(&f, i + 1));
			Sf_RaiseText(&f, Sf_Text(&f, msg));
			break;
		}
		PyObject *pos = Sf_Item(&f, dummy, 1);
		PyObject *name = Sf_Cat(&f, "sO", "SceneLauncher", Sf_ReprI(&f, i + 1));
		PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "Item");
		Sf_SetAttrI(&f, l, "OnlyUser", 0);
		Sf_SetAttrI(&f, l, "Radius", 250);
		Sf_SetAttrS(&f, l, "OnTake", "Mission_TalkToBoss.LaunchScene");
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* LaunchScene(ItemName, CharName): the human reaches the room: the alarm off, the scene of the human hiding */
static PyObject *TalkToBoss_LaunchScene(PyObject *self, PyObject *args) {
	PyObject *item, *ch;
	SF_ARGS("OO", &item, &ch);
	if (Sf_Compare(&f, ch, "!=", Sf_Attr(&f, G(TB, "human"), "Name")) != 0) {
		return Sf_None(&f);
	}
	if (Sf_True(&f, USR()) <= 0) {
		return Sf_None(&f);
	}
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	Sf_Run(&f, "Scrap", "Set", "(sO)", "AlarmFallDelta", G(TB, "alarmFallDelta"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_Run(&f, "Scrap", "DeleteScheduledFuncs", "(s)", "CheckMaxDist");
	Sf_Method(&f, ENT("SceneLauncher1"), "Del", "()");
	Sf_Method(&f, ENT("SceneLauncher2"), "Del", "()");
	Sf_Method(&f, G(TB, "human"), "Del", "()");
	Sf_SetGlobal(&f, TB, "human", Py_None);
	PyObject *boss = Sf_Entity(&f, G(TB, "bossName"));
	if (Sf_True(&f, boss) > 0) {
		Sf_SetAttrS(&f, boss, "UseFunc", "Mission_TalkToBoss.BossSpeech");
	}
	Sf_Run(&f, "Scene", "Play", "(s)", "HideHuman");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", G(TB, "EndOfScene"));
	return Sf_None(&f);
}

/* EndOfScene(usr=None): the bureau closed; the boss calls */
static PyObject *TalkToBoss_EndOfScene(PyObject *self, PyObject *args, PyObject *kw) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!Pr_UsrArg(args, kw)) {
		return NULL;
	}
	PyObject *e = ENT("DM_Door_Bureau_00");
	Sf_SetAttrS(&f, e, "ForceAction", "Closed");
	Sf_SetAttrS(&f, e, "ForceDefLoop", "Closed");
	Sf_SetMod(&f, "CharConversor", "OnPossession", Py_None);
	Sf_SetMod(&f, "CharConversor", "afterExitConversor", Py_None);
	Pr_Remote(&f, "Mission_SetHumanFree_RemoteMessage1", "Boss", TB, "PreGoTalkToBoss", 1);
	return Sf_None(&f);
}

/* OnPossession(newCharName, pos) */
static PyObject *TalkToBoss_OnPossession(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_HumanOnPossession(args, TB, "Mission_SetHumanFree_RemoteMessage2");
}

/* OnExitConversor() */
static PyObject *TalkToBoss_OnExitConversor(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_HumanOnExitConversor(args, TB, "Mission_SetHumanFree_RemoteMessage2");
}

/* CheckMaxDist(): every second: the human stops when Dtritus is too far, else follows him */
static PyObject *TalkToBoss_CheckMaxDist(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Later(&f, Sf_Float(&f, 1.0), G(TB, "CheckMaxDist"), NOARGS, "CheckMaxDist");
	PyObject *usr = USR();
	if (Sf_True(&f, usr) <= 0) {
		return Sf_None(&f);
	}
	int near = Sf_True(&f, Sf_Own(&f, TB, "CheckHumanDist", "()"));
	if (near == 0) {
		PyObject *state = Sf_Ask(&f, "SAI", "GetStateChar", "(O)", Sf_Attr(&f, G(TB, "human"), "Name"));
		if (Sf_Compare(&f, state, "!=", Sf_Int(&f, 0)) > 0) {
			Sf_Run(&f, "SAI", "SetStateChar", "(iO)", 0, Sf_Attr(&f, G(TB, "human"), "Name"));
			Pr_Remote(&f, "Mission_SetHumanFree_RemoteMessage2", "Human", NULL, NULL, 0);
		}
	} else if (near > 0 && Sf_EqS(&f, Sf_Attr(&f, usr, "ActCtrl"), "Dtritus") > 0) {
		Sf_Run(&f, "SAI", "SetStateChar", "(iOOd)", 3, Sf_Attr(&f, G(TB, "human"), "Name"), Sf_Attr(&f, usr, "Name"), 1.0);
	}
	return Sf_None(&f);
}

/* CheckHumanDist() */
static PyObject *TalkToBoss_CheckHumanDist(PyObject *self, PyObject *args) {
	(void)self;
	return Pr_CheckHumanDist(args, TB);
}

/* GoTalkToBoss(): the boss is the target */
static PyObject *TalkToBoss_GoTalkToBoss(PyObject *self, PyObject *args) {
	SF_ARGS("");
	if (Sf_True(&f, Sf_Entity(&f, G(TB, "bossName"))) > 0) {
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", G(TB, "bossName"));
	}
	return Sf_None(&f);
}

/* BossSpeech(UsrName, CharName, optId, FullOption): the boss' talk; its answer ends the mission while he is the
 * target */
static PyObject *TalkToBoss_BossSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Lv_Speakers(&f, usr, ch);
	int dt = Sf_EqS(&f, Sf_Attr(&f, USR(), "ActCtrl"), "Dtritus");
	if (dt == 0) {
		Pr_Spch(&f, "Mission_TalkToBoss_Boss_InitMsg2", "Mission_TalkToBoss_Boss_InitAns2");
	}
	if (dt <= 0) {
		return Sf_None(&f);
	}
	int talk = Sf_EqS(&f, opt, "Talk");
	if (talk > 0) {
		Sf_Run(&f, "Speech", "StdAnsLst", "([s])", "Mission_TalkToBoss_Boss_1");
		Pr_Spch(&f, "Mission_TalkToBoss_Boss_InitMsg1", "Mission_TalkToBoss_Boss_InitAns1");
	} else if (talk == 0) {
		Pr_Spch(&f, "Mission_TalkToBoss_Boss_1Msg", "Mission_TalkToBoss_Boss_1Ans");
		PyObject *boss = Sf_Entity(&f, G(TB, "bossName"));
		int target = Sf_True(&f, boss);
		if (target > 0) {
			target = Sf_EqI(&f, Sf_Attr(&f, boss, "RadarShow"), 1);
		}
		if (target > 0) {
			Lv_OnEnd(&f, TB, "SetNextMission", Sf_Build(&f, "(O)", ch));
		}
	}
	return Sf_None(&f);
}

/* SetNextMission(CharName): the first data part in DownTown next; the police boss' blueprints; Rusty's message in
 * 10 s */
static PyObject *TalkToBoss_SetNextMission(PyObject *self, PyObject *args) {
	PyObject *ch;
	SF_ARGS("O", &ch);
	Sf_Import(&f, "MissionsFuncs");
	Sf_Import(&f, "SaveGame");
	Sf_Run(&f, "Speech", "RestoreState", "(O)", ch);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "MissionState");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_GetDataPart1", "DownTown");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetDataPart1_MissionText1", 1);
	Lv_SaveAppend(&f, "PoliceBlueprints.Ships", Lv_SaveList(&f, "PoliceBlueprints.Ships", TB), Sf_Str(&f, "SPoliBoss1"));
	Lv_SaveAppend(&f, "PoliceBlueprints.Engines", Lv_SaveList(&f, "PoliceBlueprints.Engines", TB), Sf_Str(&f, "MPoliBoss1"));
	Sf_Import(&f, "Scorer");
	PyObject *a = Sf_Build(&f, "(ssO(ii))", "Mission_GetDataPart1_RemoteMessage9", "Rusty", Sf_Mod(&f, "Scorer", "ShowMissionScorer"), 0, 2);
	Sf_Later(&f, Sf_Float(&f, 10.0), Sf_Mod(&f, "Speech", "AddRemoteCom"), a, NULL);
	return Sf_None(&f);
}

PyMethodDef g_PressTalkToBossMethods[] = {
	{ "InitStatus", TalkToBoss_InitStatus, METH_VARARGS, NULL },
	{ "AfterBrowseNPC", TalkToBoss_AfterBrowseNPC, METH_VARARGS, NULL },
	{ "SetBossSpeech", TalkToBoss_SetBossSpeech, METH_VARARGS, NULL },
	{ "CreateHuman", TalkToBoss_CreateHuman, METH_VARARGS, NULL },
	{ "Init", TalkToBoss_Init, METH_VARARGS, NULL },
	{ "HideHumanIntoBureau", TalkToBoss_HideHumanIntoBureau, METH_VARARGS, NULL },
	{ "CreateSceneLaunchers", TalkToBoss_CreateSceneLaunchers, METH_VARARGS, NULL },
	{ "LaunchScene", TalkToBoss_LaunchScene, METH_VARARGS, NULL },
	{ "EndOfScene", (PyCFunction)(void (*)(void))TalkToBoss_EndOfScene, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "OnPossession", TalkToBoss_OnPossession, METH_VARARGS, NULL },
	{ "OnExitConversor", TalkToBoss_OnExitConversor, METH_VARARGS, NULL },
	{ "CheckMaxDist", TalkToBoss_CheckMaxDist, METH_VARARGS, NULL },
	{ "CheckHumanDist", TalkToBoss_CheckHumanDist, METH_VARARGS, NULL },
	{ "GoTalkToBoss", TalkToBoss_GoTalkToBoss, METH_VARARGS, NULL },
	{ "BossSpeech", TalkToBoss_BossSpeech, METH_VARARGS, NULL },
	{ "SetNextMission", TalkToBoss_SetNextMission, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_WhoIsDeepThroat ---- */

/* Init(LevelPath): the teleporter to the boss room is the target; its use starts the scene */
static PyObject *DeepThroat_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	PyObject *elv = Pr_BossDoor(&f, "DtritusGroupGateKeeper", "Mission_WhoIsDeepThroat.UseTeleporter");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, elv, "Name"));
	Sf_Own(&f, WD, "LoadScene", "()");
	return Sf_None(&f);
}

/* UseTeleporter(UsrName, ElevatorName, optId, FullOption) */
static PyObject *DeepThroat_UseTeleporter(PyObject *self, PyObject *args) {
	PyObject *usr, *el, *opt, *full;
	SF_ARGS("OOOO", &usr, &el, &opt, &full);
	if (Sf_True(&f, Sf_Ask(&f, "GateKeeper", "UseDoor", "(OOOO)", usr, el, opt, full)) > 0) {
		Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", G(WD, "LaunchScene"));
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	}
	return Sf_None(&f);
}

/* LaunchScene(charName=""): the teleporter arrived: the scene in 0.1 s */
static PyObject *DeepThroat_LaunchScene(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "charName", NULL };
	PyObject *name = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &name)) {
		return NULL;
	}
	Sf_SetMod(&f, "Elevator", "OnElevatorReachDown", Py_None);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Sf_Later(&f, Sf_Float(&f, 0.1), G(WD, "PlayScene"), NOARGS, NULL);
	return Sf_None(&f);
}

/* PlayScene(): Deep Throat discovered */
static PyObject *DeepThroat_PlayScene(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scene", "Play", "(s)", "DiscoverDeep");
	Pr_EntS(&f, "DM_Door_PressSlidingDoor_00", "OnObjSlot", "");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", G(WD, "SetNextMission"));
	return Sf_None(&f);
}

PyMethodDef g_PressWhoIsDeepThroatMethods[] = {
	{ "Init", DeepThroat_Init, METH_VARARGS, NULL },
	{ "UseTeleporter", DeepThroat_UseTeleporter, METH_VARARGS, NULL },
	{ "LaunchScene", (PyCFunction)(void (*)(void))DeepThroat_LaunchScene, METH_VARARGS | METH_KEYWORDS, NULL },
	{ "PlayScene", DeepThroat_PlayScene, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_WhoIsTheBaddie ---- */

/* InitStatus(LevelPath): the bad reputation and the pilots' profile while the boss rules the robots (the old values
 * kept once), nobody at the GamblinDen; the start in the boss room before the scene */
static PyObject *Baddie_InitStatus(PyObject *self, PyObject *args) {
	static const char *const prev[][3] = { { "PrevBadReputation", "BadReputation", "BAD_REPUTATION" },
		{ "PrevStdShipAIProfile", "StdShipAIProfile", "STD_SHIPS_PROFILE" } };
	static const char *const zero[] = { "BankMasterAtBank", "MayorAtGambinDen", "FunctionaryTwinsAtGambinDen", "BankersAtGambinDen",
		"BishopsAtGambinDen", "PolicesAtGambinDen" };
	PyObject *path;
	SF_ARGS("O", &path);
	for (int i = 0; i < 2 && !f.failed; i++) {
		if (Sf_Compare(&f, SAVE(prev[i][0]), "==", Py_None) > 0) {
			Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", prev[i][0], SAVE(prev[i][1]));
			Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", prev[i][1], G(WB, prev[i][2]));
		}
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "RobotsControlledByBoss", "1");
	for (int i = 0; i < 6; i++) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", zero[i], "0");
	}
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Traffic.AcShips", "[\"SBanker1\",\"SBishop1\",\"SFunc1\"]");
	if (Lv_SaveIs(&f, "MissionState", "TalkToBoss") > 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "");
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Dtritus");
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Spawn", "DM_Mission_Berto_MeetBos");
		Sf_Own(&f, WB, "InitStatusScene", "(O)", path);
	}
	return Sf_None(&f);
}

/* Init(LevelPath): the boss room open to all; the scene of the boss, or the way to the human in the bureau */
static PyObject *Baddie_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	PyObject *elv = Pr_BossDoor(&f, "NoOneGroupGateKeeper", "GateKeeper.UseDoor");
	int scene = Lv_SaveIs(&f, "MissionState", "TalkToBoss");
	if (scene > 0) {
		Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, elv, "Name"));
		Sf_Own(&f, WB, "InitScene", "(O)", path);
		Sf_Own(&f, WB, "PlayScene", "()");
	} else if (scene == 0) {
		Sf_Own(&f, WB, "GoForHuman", "()");
	}
	return Sf_None(&f);
}

/* GoForHuman(): a usable arrow at the bureau is the target */
static PyObject *Baddie_GoForHuman(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *e = ENT("DM_Door_Bureau_00");
	PyObject *pos = Sf_Attr(&f, e, "Pos");
	PyObject *name = Sf_Cat(&f, "Os", Sf_Attr(&f, e, "Name"), "CheckArrow");
	PyObject *e2 = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "WithLife");
	Sf_SetAttrI(&f, e2, "ArrowCanUse", 1);
	Sf_SetAttrI(&f, e2, "UseAngSel", 1);
	Sf_SetAttrI(&f, e2, "UseMapCheck", 0);
	Sf_SetAttrI(&f, e2, "VRadius", 60);
	Sf_SetAttrI(&f, e2, "ArrnOpt", 0);
	Sf_SetAttr(&f, e2, "ArrOpt0", Sf_Cat(&f, "sO", "Use:", LANG("USE_Bureau")));
	Sf_SetAttr(&f, e2, "CamTgt", Sf_Build(&f, "(iii)", 0, 100, 0));
	Sf_SetAttrS(&f, e2, "UseFunc", "Mission_WhoIsTheBaddie.OnBureauUsed");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, e2, "Name"));
	return Sf_None(&f);
}

/* OnBureauUsed(UsrName, CharName, optId, FullOption): the bureau opens, the gambler's message */
static PyObject *Baddie_OnBureauUsed(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	PyObject *bureau = Sf_Entity(&f, ch);
	Sf_SetAttrI(&f, bureau, "ArrowCanUse", 0);
	Sf_SetAttrS(&f, bureau, "UseFunc", "");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Py_None);
	Pr_Remote(&f, "Mission_TalkToHuman_RemoteMessage1", "CrazyGambler", WB, "SetNextMission", 1);
	PyObject *b = ENT("DM_Door_Bureau_00");
	Sf_SetAttrS(&f, b, "OnObjSlot", "");
	Sf_SetAttrS(&f, b, "Action", "Open");
	Sf_SetAttrS(&f, b, "DefLoop", "Opened");
	return Sf_None(&f);
}

/* PlayScene(): the boss discovered, with his and Betty's heads on the news panel */
static PyObject *Baddie_PlayScene(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "SetLockAlarm", "(i)", 0);
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	Sf_Own(&f, WB, "CreateFakeSpeech", "()");
	Sf_Run(&f, "Scene", "Play", "(s)", "BossDiscovered");
	Pr_EntS(&f, "DM_Door_PressSlidingDoor_00", "OnObjSlot", "");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", G(WB, "EndOfScene"));
	Sf_Run(&f, "Scrap", "Set", "(si)", "useDynamicMusic", 0);
	Sf_Run(&f, "SSound", "SetMusic", "(sd)", "Music/HumanCD1.ogg", 1.0);
	return Sf_None(&f);
}

/* EndOfScene(usr): the heads go, the alarm */
static PyObject *Baddie_EndOfScene(PyObject *self, PyObject *args) {
	PyObject *usr;
	SF_ARGS("O", &usr);
	Lv_Fade(&f, lv_black, lv_clear, 3.0);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "NewsHead", "Betty");
	Sf_Method(&f, ENT("FakeSpeech"), "Del", "()");
	Sf_Method(&f, ENT("FakeSpeech2"), "Del", "()");
	Sf_Run(&f, "Scrap", "Set", "(si)", "NewsPanelShowHead", 0);
	Sf_Own(&f, WB, "AfterScene", "()");
	Sf_Run(&f, "Scrap", "Set", "(si)", "useDynamicMusic", 1);
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 1);
	return Sf_None(&f);
}

/* CreateFakeSpeech(): the angry boss and the alarmed Betty for the news panel */
static PyObject *Baddie_CreateFakeSpeech(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Pr_FakeSpeech(&f, "FakeSpeech", "Boss", "TalkAngry");
	Pr_FakeSpeech(&f, "FakeSpeech2", "Betty", "TalkAlarmed");
	Pr_FakeSpeechShow(&f, WB);
	return Sf_None(&f);
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
static PyObject *Baddie_LoadScene(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Pr_Scene(&f, WB, baddie_scene);
	return Sf_None(&f);
}

PyMethodDef g_PressWhoIsTheBaddieMethods[] = {
	{ "InitStatus", Baddie_InitStatus, METH_VARARGS, NULL },
	{ "Init", Baddie_Init, METH_VARARGS, NULL },
	{ "GoForHuman", Baddie_GoForHuman, METH_VARARGS, NULL },
	{ "OnBureauUsed", Baddie_OnBureauUsed, METH_VARARGS, NULL },
	{ "PlayScene", Baddie_PlayScene, METH_VARARGS, NULL },
	{ "EndOfScene", Baddie_EndOfScene, METH_VARARGS, NULL },
	{ "CreateFakeSpeech", Baddie_CreateFakeSpeech, METH_VARARGS, NULL },
	{ "NormCamera", Pr_NormCamera, METH_VARARGS, NULL },
	{ "SetTalk", Pr_SetTalk, METH_VARARGS, NULL },
	{ "LoadScene", Baddie_LoadScene, METH_VARARGS, NULL },
	{ NULL, NULL, 0, NULL },
};
