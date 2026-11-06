/*
 * The MissionsFuncs module in C (scripts/missions/missionsfuncs.py, issue #2 step 8a): the mission flow of the single
 * player game. The start of a level (the arrow to the next mission, the auto save, the initial message), the next
 * mission and its map, the mission targets on the radar, the dialog of the mission characters, the death of the
 * player (lives, the jail, the revenge of a challenge), the photo objects, the waves of Bertos, the crazy deals and
 * the super deals with their score tables.
 *
 * The tables and the state stay in the module (WorldMap, currentTarget, targetsList, the Berto waves with their hooks
 * OnNewWaveCallback and OnBertoDeathCallback, cameraData, mapsCameraData, the names and colours of the score tables)
 * with the module code (the save variables it creates), because the missions read and change them; the functions read
 * them as their globals.
 */
#include <limits.h>

#include "scrap/script.h"

#define MF "MissionsFuncs"

/* Scrap.GetSaveVar(name) */
static PyObject *Mf_Save(scriptFrame_t *f, const char *name) {
	return Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", name);
}

/* Scrap.SetSaveVar(name, value) with a value or a text */
static void Mf_SetSave(scriptFrame_t *f, const char *name, PyObject *v) {
	if (v) {
		Sf_Ask(f, "Scrap", "SetSaveVar", "(sO)", name, v);
	}
}

static void Mf_SetSaveS(scriptFrame_t *f, const char *name, const char *text) {
	Sf_Ask(f, "Scrap", "SetSaveVar", "(ss)", name, text);
}

/* Scrap.GetLangStr(key) */
static PyObject *Mf_Lang(scriptFrame_t *f, const char *key) {
	return Sf_Lang(f, Sf_Str(f, key));
}

/* a function of the module as its functions call it, with Py_BuildValue arguments */
#define Mf_Call(f, func, ...) Sf_Call(f, Sf_Global(f, MF, func), __VA_ARGS__)

/* `for x in seq` of a global list of the module, fn(f, x, arg) for every item */
static void Mf_Each(scriptFrame_t *f, PyObject *seq, void (*fn)(scriptFrame_t *f, PyObject *x, PyObject *arg), PyObject *arg) {
	for (int i = 0;; i++) {
		int mark = f->n;
		PyObject *x = Sf_Next(f, seq, i);
		if (!x) {
			break;
		}
		fn(f, x, arg);
		Sf_Release(f, mark);
	}
}

/* Init(): the start of a level: the challenges outdoors, the arrow to the mission, the auto save and the initial
 * message */
static PyObject *MissionsFuncs_Init(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_Import(&f, "SaveGame");
	if (Sf_EqI(&f, Sf_Mod(&f, "Init", "Outdoor"), 1) > 0) {
		Sf_Import(&f, "Challenge");
		Sf_Ask(&f, "Challenge", "Init", "()");
	}
	Mf_Call(&f, "SetOutMissionArrow", "()");
	if (Sf_EqS(&f, Mf_Save(&f, "AutoSaveGameOnLoad"), "1") > 0) {
		Sf_Schedule(&f, Sf_Int(&f, 0), Sf_Mod(&f, "SaveGame", "SaveGame"), Sf_Build(&f, "(i)", 0), NULL);
	}
	PyObject *msg = Mf_Save(&f, "InitialMessage");
	if (Sf_True(&f, msg) > 0) {
		Mf_Call(&f, "TryMsg", "(O)", msg);
		Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "InitialMessage");
	}
	return Sf_None(&f);
}

/* TryMsg(InitMsg): the message when the player walks or flies and no other one shows, else a second later */
static PyObject *MissionsFuncs_TryMsg(PyObject *self, PyObject *args) {
	static const char *const sets[] = { "Racer", "Walk", NULL };
	PyObject *msg;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &msg)) {
		return NULL;
	}
	int now = Sf_True(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0));
	if (now > 0) {
		now = Sf_InS(&f, Sf_Ask(&f, "SInput", "GetActionSet", "()"), sets);
	}
	if (now > 0) {
		PyObject *t = Sf_Ask(&f, "Scrap", "Get", "(s)", "TimeToMsg");
		now = Sf_Compare(&f, t, "<", Sf_Time(&f));
	}
	if (now > 0) {
		Sf_Import(&f, "Speech");
		PyObject *t = Sf_Time(&f);
		t = Sf_Op(&f, t, '+', Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)", msg));
		Sf_Ask(&f, "SScorer", "SetMsgText", "(OO)", msg, t);
	} else if (now == 0) {
		Sf_Later(&f, Sf_Float(&f, 1.0), Sf_Global(&f, MF, "TryMsg"), Sf_Build(&f, "(O)", msg), NULL);
	}
	return Sf_None(&f);
}

/* (a, b) != (Scrap.GetSaveVar("Mission.Map"), Scrap.GetSaveVar("Mission.Library")): the level is reloaded. The
 * script compares the mission with the map and the map with the mission, so this holds unless both are equal. */
static void Mf_ForceReload(scriptFrame_t *f, PyObject *a, PyObject *b) {
	PyObject *pair = Sf_Build(f, "(OO)", a, b);
	PyObject *map = Mf_Save(f, "Mission.Map");
	PyObject *saved = Sf_Build(f, "(OO)", map, Mf_Save(f, "Mission.Library"));
	if (Sf_Compare(f, pair, "!=", saved) > 0) {
		Sf_Ask(f, "Scrap", "Set", "(si)", "ForceReload", 1);
	}
}

/* SetNextMission(MissionName, AtMap): the next mission, its arrow, the way point and an auto save at the load */
static PyObject *MissionsFuncs_SetNextMission(PyObject *self, PyObject *args) {
	PyObject *mission, *map;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &mission, &map)) {
		return NULL;
	}
	Mf_SetSave(&f, "Mission.Map", map);
	Mf_SetSave(&f, "Mission.Library", mission);
	Mf_Call(&f, "SetOutMissionArrow", "()");
	Sf_Import(&f, "SaveGame");
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Mf_SetSaveS(&f, "AutoSaveGameOnLoad", "1");
	Mf_ForceReload(&f, mission, map);
	return Sf_None(&f);
}

/* SetFirstDemoMission(): the first mission of the demo */
static PyObject *MissionsFuncs_SetFirstDemoMission(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Mf_SetSaveS(&f, "Mission.Map", "GamblinDen");
	Mf_SetSaveS(&f, "Mission.Library", "Mission_CrazyGamblerFirstContact");
	Mf_Call(&f, "SetOutMissionArrow", "()");
	Mf_SetSaveS(&f, "AutoSaveGameOnLoad", "1");
	Mf_ForceReload(&f, Sf_Str(&f, "Mission_CrazyGamblerFirstContact"), Sf_Str(&f, "GamblinDen"));
	return Sf_None(&f);
}

/* SetOutMissionArrow(): the exit of this level towards the map of the mission (WorldMap) shows the mission arrow
 * (on the radar only in the junkyard) */
static PyObject *MissionsFuncs_SetOutMissionArrow(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "string");
	Sf_Import(&f, "Init");
	PyObject *actual = Sf_Ask(&f, "string", "upper", "(O)", Sf_Slice(&f, Sf_Mod(&f, "Init", "Path"), 7, INT_MAX));
	PyObject *target = Sf_Ask(&f, "string", "upper", "(O)", Mf_Save(&f, "Mission.Map"));
	int go = Sf_Compare(&f, actual, "!=", target);
	if (go > 0) {
		PyObject *world = Sf_Global(&f, MF, "WorldMap");
		go = Sf_True(&f, Sf_Method(&f, world, "has_key", "(O)", actual));
	}
	if (go > 0) {
		PyObject *exits = Sf_GetItem(&f, Sf_Global(&f, MF, "WorldMap"), actual);
		go = Sf_True(&f, Sf_Method(&f, exits, "has_key", "(O)", target));
	}
	if (go > 0) {
		PyObject *exits = Sf_GetItem(&f, Sf_Global(&f, MF, "WorldMap"), actual);
		PyObject *e = Sf_Entity(&f, Sf_GetItem(&f, exits, target));
		PyObject *fn = Sf_Global(&f, MF, "ActivateTarget");
		int out = Sf_Compare(&f, actual, "!=", Sf_Str(&f, "JUNKYARD"));
		if (out >= 0) {
			Sf_Call(&f, fn, "(Oii)", e, 1, out);
		}
	}
	return Sf_None(&f);
}

/* ActivateTarget(e, status, elcomoe=1): on the radar; with elcomoe a character also gets the mission arrow (1) or
 * the target arrow (2) */
static PyObject *MissionsFuncs_ActivateTarget(PyObject *self, PyObject *args) {
	PyObject *e, *status, *arrow = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO|O", &e, &status, &arrow)) {
		return NULL;
	}
	Sf_SetAttr(&f, e, "RadarShow", status);
	int life = Sf_True(&f, Sf_Method(&f, e, "IsType", "(s)", "WithLife"));
	if (life > 0 && arrow) {
		life = Sf_True(&f, arrow);
	}
	if (life > 0) {
		Sf_SetAttr(&f, e, "ArrowCanMission", status);
		int one = Sf_EqI(&f, status, 1);
		if (one > 0) {
			Sf_SetAttr(&f, e, "ArrowModMission", Sf_Ask(&f, "Scrap", "Get", "(s)", "ArrowMissionFile"));
		} else if (one == 0 && Sf_EqI(&f, status, 2) > 0) {
			Sf_SetAttr(&f, e, "ArrowModMission", Sf_Ask(&f, "Scrap", "Get", "(s)", "ArrowTargetFile"));
		}
	}
	return Sf_None(&f);
}

/* GetSecondary(val): the state of the secondary mission */
static PyObject *MissionsFuncs_GetSecondary(PyObject *self, PyObject *args) {
	PyObject *val;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &val)) {
		return NULL;
	}
	return Sf_Return(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "IsSecondMission"));
}

/* SetSecondary(val): the state of the secondary mission, saved */
static PyObject *MissionsFuncs_SetSecondary(PyObject *self, PyObject *args) {
	PyObject *val;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &val)) {
		return NULL;
	}
	Mf_SetSave(&f, "IsSecondMission", Sf_Repr(&f, val));
	Sf_Ask(&f, "Scrap", "Set", "(sO)", "IsSecondMission", val);
	return Sf_None(&f);
}

/* FinishGame(id): the end of the game */
static PyObject *MissionsFuncs_FinishGame(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Menu", "Init", "SaveGame", "Init" };
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_Ask(&f, "Menu", "MovieScorer", "(O)", id);
	Sf_Ask(&f, "Menu", "GameOver", "(O)", id);
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	return Sf_None(&f);
}

/* ShowMissionInfo(id): the mission info pages (the engine's MainMissionEvent) */
static PyObject *MissionsFuncs_ShowMissionInfo(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Ask(&f, "Menu", "EnterMissionInfo", "(i)", 0);
	return Sf_None(&f);
}

/* ActivateTarget(GetEntity(name), status) when the entity is there */
static void Mf_Target(scriptFrame_t *f, PyObject *name, PyObject *status) {
	PyObject *e = Sf_Entity(f, name);
	if (Sf_True(f, e) > 0) {
		Mf_Call(f, "ActivateTarget", "(OO)", e, status);
	}
}

/* SetMissionTarget(newTarget): the one target of the mission */
static PyObject *MissionsFuncs_SetMissionTarget(PyObject *self, PyObject *args) {
	PyObject *tgt;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &tgt)) {
		return NULL;
	}
	Mf_Call(&f, "CleanAllTargets", "()");
	Sf_SetGlobal(&f, MF, "currentTarget", tgt);
	if (Sf_True(&f, tgt) > 0) {
		PyObject *e = Sf_Entity(&f, tgt);
		Mf_Call(&f, "ActivateTarget", "(Oi)", e, 1);
	}
	return Sf_None(&f);
}

/* SetMultipleMissionTargets(tgtList): several targets */
static PyObject *MissionsFuncs_SetMultipleMissionTargets(PyObject *self, PyObject *args) {
	PyObject *list;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &list)) {
		return NULL;
	}
	Mf_Call(&f, "CleanAllTargets", "()");
	Sf_SetGlobal(&f, MF, "targetsList", list);
	Sf_SetGlobal(&f, MF, "currentTarget", Py_None);
	Mf_Each(&f, Sf_Global(&f, MF, "targetsList"), Mf_Target, Sf_Int(&f, 1));
	return Sf_None(&f);
}

/* CleanAllTargets(): no target any more */
static PyObject *MissionsFuncs_CleanAllTargets(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *cur = Sf_Global(&f, MF, "currentTarget");
	if (Sf_True(&f, cur) > 0) {
		Mf_Target(&f, Sf_Global(&f, MF, "currentTarget"), Sf_Int(&f, 0));
	}
	Mf_Each(&f, Sf_Global(&f, MF, "targetsList"), Mf_Target, Sf_Int(&f, 0));
	Sf_SetGlobal(&f, MF, "targetsList", Sf_Keep(&f, PyList_New(0)));
	return Sf_None(&f);
}

/* DelMissionTarget(target): one target less; 1 when none is left */
static PyObject *MissionsFuncs_DelMissionTarget(PyObject *self, PyObject *args) {
	PyObject *tgt;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &tgt)) {
		return NULL;
	}
	if (Sf_In(&f, tgt, Sf_Global(&f, MF, "targetsList")) > 0) {
		Sf_Method(&f, Sf_Global(&f, MF, "targetsList"), "remove", "(O)", tgt);
		Mf_Target(&f, tgt, Sf_Int(&f, 0));
	}
	int n = Sf_Len(&f, Sf_Global(&f, MF, "targetsList"));
	return Sf_Done(&f, n >= 0 ? PyInt_FromLong(n == 0) : NULL);
}

/* IsCharMissionObjective(charName): 1 when the character is a target */
static PyObject *MissionsFuncs_IsCharMissionObjective(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	int is = Sf_Compare(&f, Sf_Global(&f, MF, "currentTarget"), "==", name);
	if (is == 0) {
		is = Sf_In(&f, name, Sf_Global(&f, MF, "targetsList"));
	}
	return Sf_Done(&f, is >= 0 ? PyInt_FromLong(is) : NULL);
}

/* CharMissionDialog(UsrName, CharName, optId, FullOption): the talk of a mission character from its spawn point in
 * CharsNPC.CharRes (initMsg, initAns, speechOptionsList, afterInitSpeechFunc once) */
static PyObject *MissionsFuncs_CharMissionDialog(PyObject *self, PyObject *args) {
	PyObject *usr, *name, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &name, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	Sf_Import(&f, "CharsNPC");
	Sf_SetMod(&f, "Speech", "PlayerName", usr);
	Sf_SetMod(&f, "Speech", "NPCName", name);
	PyObject *cls = Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), name);
	int talk = Sf_EqS(&f, opt, "Talk");
	if (talk > 0) {
		PyObject *msg = Sf_Attr(&f, cls, "initMsg");
		Sf_Ask(&f, "Speech", "OnSpch", "(OOs)", msg, Sf_Attr(&f, cls, "initAns"), "TalkNPC");
		if (Sf_True(&f, Sf_Attr(&f, cls, "speechOptionsList")) > 0) {
			Sf_Ask(&f, "Speech", "StdAnsLst", "(O)", Sf_Attr(&f, cls, "speechOptionsList"));
		}
		if (Sf_True(&f, Sf_Attr(&f, cls, "afterInitSpeechFunc")) > 0) {
			Sf_SetMod(&f, "Speech", "OnEndOfSpeech", Sf_Attr(&f, cls, "afterInitSpeechFunc"));
			Sf_SetAttr(&f, cls, "afterInitSpeechFunc", Py_None);
		}
	} else if (talk == 0 && Sf_True(&f, Sf_Attr(&f, cls, "speechOptionsList")) > 0) {
		Sf_Ask(&f, "Speech", "StdOnSpch", "(O)", opt);
	}
	return Sf_None(&f);
}

/* CancelSecondaryMission(id, control): back to the game, a race or combat challenge is cancelled */
static PyObject *MissionsFuncs_CancelSecondaryMission(PyObject *self, PyObject *args) {
	static const char *const types[] = { "Race", "Combat", NULL };
	PyObject *id, *control;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, "Challenge");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "SaveGame");
	Sf_Ask(&f, "Menu", "BackToGame", "(O)", id);
	PyObject *type = Mf_Save(&f, "Challenge.Type");
	if (Sf_InS(&f, type, types) > 0) {
		Sf_Ask(&f, "Challenge", "CancelChallenge", "(O)", type);
	}
	Sf_Ask(&f, "SaveGame", "SMDesc", "(si)", "", 1);
	return Sf_None(&f);
}

/* SinglePlayerDeath(): the player died: a life less (unless infinite), the alarm off, the mercenaries no friends, a
 * character the player still has; with lives left the way point (a free phone cabin in the junkyard, the revenge of
 * a ship challenge), else the jail; the restart 5 s later */
static PyObject *MissionsFuncs_SinglePlayerDeath(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "SaveGame");
	Sf_Import(&f, "Challenge");
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	PyObject *restart = Sf_Mod(&f, "SaveGame", "StartGame");
	PyObject *params = Sf_Keep(&f, PyTuple_New(0));
	PyObject *lifes = Sf_ToInt(&f, Mf_Save(&f, "Player.NumLives"));
	if (Sf_EqS(&f, Mf_Save(&f, "Player.InfiniteLives"), "0") > 0) {
		lifes = Sf_OpI(&f, lifes, '-', 1);
		Mf_SetSave(&f, "Player.NumLives", Sf_Repr(&f, lifes));
	}
	Mf_SetSaveS(&f, "AlarmActive", "0");
	Mf_SetSaveS(&f, "AlarmStatus", "0.0");
	Sf_Ask(&f, "Scrap", "SetAlarm", "(i)", 0);
	Mf_SetSaveS(&f, "MercFriends.MercenaryA_Smartie", "0");
	Mf_SetSaveS(&f, "MercFriends.MercenaryC_Brutus", "0");
	Mf_SetSaveS(&f, "MercFriends.MercenaryB_Dumber", "0");
	PyObject *type = Mf_Save(&f, "Char");
	if (Sf_EqS(&f, type, "Gear") > 0) {
		type = Sf_Str(&f, "Police");
	}
	PyObject *avail = Sf_Eval(&f, Mf_Save(&f, "Conversor.AvailableChars"), MF);
	if (Sf_In(&f, type, avail) == 0) {
		int dtritus = Sf_In(&f, Sf_Str(&f, "Dtritus"), avail);
		if (dtritus > 0) {
			type = Sf_Str(&f, "Dtritus");
		} else if (dtritus == 0) {
			type = Sf_Str(&f, "Berto");
			Mf_SetSave(&f, "InitialMessage", Mf_Lang(&f, "Generics_NoMatrix"));
		}
	}
	Mf_SetSave(&f, "Char", type);
	Sf_Import(&f, "Init");
	int alive = Sf_True(&f, Sf_Mod(&f, "Init", "isDemo")) == 0;
	if (alive == 0) {
		alive = Sf_Compare(&f, lifes, ">=", Sf_Int(&f, 0));
	}
	if (alive > 0) {
		int left = Sf_Compare(&f, lifes, ">=", Sf_Int(&f, 0));
		if (left > 0) {
			Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
			int car = Sf_True(&f, usr);
			if (car > 0) {
				car = Sf_EqS(&f, Sf_Attr(&f, usr, "Type"), "Car");
			}
			if (car > 0) {
				PyObject *ctype = Mf_Save(&f, "Challenge.Type");
				PyObject *foe = Mf_Save(&f, "Challenge.Foe");
				int revenge = Sf_Compare(&f, ctype, "!=", Sf_Str(&f, ""));
				if (revenge > 0) {
					revenge = Sf_Compare(&f, foe, "!=", Sf_Str(&f, ""));
				}
				if (revenge > 0) {
					PyObject *fn = Sf_Mod(&f, "Challenge", "SetRevengeParams");
					PyObject *p = Sf_Build(&f, "(OOO)", ctype, foe, Mf_Save(&f, "Challenge.NumEnemies"));
					if (p) {
						restart = fn;
						params = p;
					}
				}
			}
			if (Sf_EqS(&f, Mf_Save(&f, "Spawn"), "DM_Player_Spawn_Junkyard") > 0) {
				Sf_Import(&f, "PhoneCab");
				Sf_Import(&f, "Init");
				Sf_Import(&f, "SaveGame");
				PyObject *pcb = Sf_Ask(&f, "PhoneCab", "GetFree", "()");
				Mf_SetSave(&f, "Spawn", Sf_Attr(&f, pcb, "Name"));
				Mf_SetSave(&f, "ComeFrom", Sf_Attr(&f, pcb, "Name"));
			}
		} else if (left == 0) {
			Sf_Ask(&f, "SaveGame", "MarkWayPoint", "(sss)", "Levels/Police", "DM_Player_Spawn_Jail", "");
		}
	}
	Sf_Later(&f, Sf_Float(&f, 5.0), restart, params, NULL);
	return Sf_None(&f);
}

/* CreatePhotoObject(modelName, pos, ang, viewFunc=""): a blueprint on the floor to photograph, a mission target */
static PyObject *MissionsFuncs_CreatePhotoObject(PyObject *self, PyObject *args) {
	PyObject *model, *pos, *ang, *view = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO|O", &model, &pos, &ang, &view)) {
		return NULL;
	}
	if (!view) {
		view = Sf_Str(&f, "");
	}
	PyObject *x = Sf_Item(&f, pos, 0), *y = Sf_Item(&f, pos, 1), *z = Sf_Item(&f, pos, 2);
	PyObject *e = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", model, x, y, z, "WithLife");
	Sf_SetAttr(&f, e, "Ang", ang);
	Sf_SetAttrI(&f, e, "ArrowCanView", 1);
	Sf_SetAttrI(&f, e, "CanPhoto", 1);
	Sf_Method(&f, e, "SetModel", "(O)", Sf_Cat(&f, "sOs", "Models/Misc/Blueprint/", model, ".M3D"));
	Sf_SetAttr(&f, e, "ViewFunc", view);
	Sf_SetAttrI(&f, e, "VRadius", 30);
	Sf_Import(&f, "Items");
	Sf_Ask(&f, "Items", "SetInteractiveObjOnFloor", "(O)", e);
	Mf_Call(&f, "ActivateTarget", "(Oi)", e, 1);
	Sf_SetAttr(&f, e, "MovePos", Sf_Attr(&f, e, "Pos"));
	Sf_SetAttr(&f, e, "MoveAng", Sf_Attr(&f, e, "Ang"));
	Sf_SetAttr(&f, e, "MoveTime", Sf_Float(&f, 0.1));
	return Sf_Return(&f, e);
}

/* UnsetPhotoObject(obj): no target, no arrows */
static PyObject *MissionsFuncs_UnsetPhotoObject(PyObject *self, PyObject *args) {
	PyObject *obj;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &obj)) {
		return NULL;
	}
	Sf_SetAttrI(&f, obj, "RadarShow", 0);
	Sf_SetAttrI(&f, obj, "ArrowCanView", 0);
	Sf_SetAttrI(&f, obj, "ArrowCanMission", 0);
	return Sf_None(&f);
}

/* SAI.SetStateVehicle(2, berto, usr.Name) */
static void Mf_ChaseUsr(scriptFrame_t *f, PyObject *berto, PyObject *usr) {
	Sf_Ask(f, "SAI", "SetStateVehicle", "(iOO)", 2, berto, Sf_Attr(f, usr, "Name"));
}

/* ReassignBertosPlayerTarget(): the Bertos chase the player's new ship or character */
static PyObject *MissionsFuncs_ReassignBertosPlayerTarget(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) > 0) {
		Mf_Each(&f, Sf_Global(&f, MF, "currentBertos"), Mf_ChaseUsr, usr);
	}
	return Sf_None(&f);
}

/* CreateBerto(usrName): a Berto ship at the nearest free spawn point, with the profile and life of the save game */
static PyObject *MissionsFuncs_CreateBerto(PyObject *self, PyObject *args) {
	PyObject *usrName;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usrName)) {
		return NULL;
	}
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "OutMap");
	PyObject *spw = Sf_Ask(&f, "OutMap", "GetNearestFreeSpawnPos", "()");
	PyObject *x = Sf_Item(&f, spw, 0), *y = Sf_Item(&f, spw, 1), *z = Sf_Item(&f, spw, 2);
	PyObject *p = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "Berto", x, y, z, "Car");
	PyObject *pd = Sf_Ask(&f, "VehiclesNPC", "GetIAProf", "(s)", "SBERTO1");
	Sf_Ask(&f, "VehiclesNPC", "SetAIProfile", "(OO)", pd, Mf_Save(&f, "BertosProfile"));
	PyObject *name = Sf_Attr(&f, p, "Name");
	Sf_Ask(&f, "VehiclesNPC", "SetStdProperties", "(OO)", name, Sf_Attr(&f, pd, "Ship"));
	Sf_Ask(&f, "VehiclesNPC", "ModAIProff", "(OO)", p, pd);
	Sf_SetAttr(&f, p, "Ang", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_SetAttrS(&f, p, "OnDeath", "MissionsFuncs.OnBertoDeath");
	Sf_SetAttrS(&f, p, "TgType", "red");
	Sf_SetAttrS(&f, p, "EntityList", "Enemies");
	PyObject *life = Sf_Eval(&f, Mf_Save(&f, "BertosLife"), MF);
	Sf_SetAttr(&f, p, "Life", life);
	Sf_SetAttr(&f, p, "MaxLife", life);
	Sf_Method(&f, Sf_Global(&f, MF, "currentBertos"), "append", "(O)", Sf_Attr(&f, p, "Name"));
	return Sf_None(&f);
}

/* CreateBertos(): a new wave: the hook OnNewWaveCallback, then a Berto a second up to maxBertos */
static PyObject *MissionsFuncs_CreateBertos(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_SetGlobal(&f, MF, "launchNewWave", Sf_Int(&f, 1));
	PyObject *hook = Sf_Global(&f, MF, "OnNewWaveCallback");
	if (Sf_True(&f, hook) > 0) {
		Sf_Call(&f, Sf_Global(&f, MF, "OnNewWaveCallback"), "()");
	}
	Sf_SetMod(&f, "Parking", "SwitchVehicleMissionCallback", Py_None);
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) > 0) {
		PyObject *usrName = Sf_Attr(&f, usr, "Name");
		PyObject *now = Sf_Time(&f);
		PyObject *max = Sf_Global(&f, MF, "maxBertos");
		int n = Sf_Len(&f, Sf_Global(&f, MF, "currentBertos"));
		PyObject *count = n >= 0 ? Sf_Op(&f, max, '-', Sf_Int(&f, n)) : NULL;
		PyObject *range = Sf_Ask(&f, "__builtin__", "range", "(O)", count);
		for (int i = 0;; i++) {
			int mark = f.n;
			PyObject *k = Sf_Next(&f, range, i);
			if (!k) {
				break;
			}
			PyObject *t = Sf_Op(&f, now, '+', Sf_Op(&f, Sf_Float(&f, 1.0), '*', Sf_OpI(&f, k, '+', 1)));
			Sf_Schedule(&f, t, Sf_Global(&f, MF, "CreateBerto"), Sf_Build(&f, "(O)", usrName), NULL);
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/* OnBertoDeath(LooserName, WinnerName): the death of a ship; a dead Berto starts the next wave timeBetweenWaves
 * later (once a wave), then the hook OnBertoDeathCallback */
static PyObject *MissionsFuncs_OnBertoDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Ask(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	if (Sf_In(&f, looser, Sf_Global(&f, MF, "currentBertos")) > 0) {
		Sf_Method(&f, Sf_Global(&f, MF, "currentBertos"), "remove", "(O)", looser);
		if (Sf_True(&f, Sf_Global(&f, MF, "launchNewWave")) > 0) {
			Sf_SetGlobal(&f, MF, "launchNewWave", Sf_Int(&f, 0));
			PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Global(&f, MF, "timeBetweenWaves"));
			Sf_Schedule(&f, t, Sf_Global(&f, MF, "CreateBertos"), Sf_Keep(&f, PyTuple_New(0)), NULL);
		}
	}
	if (Sf_True(&f, Sf_Global(&f, MF, "OnBertoDeathCallback")) > 0) {
		PyObject *pos = Sf_Attr(&f, Sf_Entity(&f, looser), "Pos");
		Sf_Call(&f, Sf_Global(&f, MF, "OnBertoDeathCallback"), "(OOO)", looser, winner, pos);
	}
	return Sf_None(&f);
}

/* "CrazyDeal." + `number` + suffix */
static PyObject *Mf_DealVar(scriptFrame_t *f, PyObject *number, const char *suffix) {
	return Sf_Cat(f, "sOs", "CrazyDeal.", Sf_Repr(f, number), suffix);
}

/* CrazyDealStatus(number): (variable, value, target) of a crazy deal */
static PyObject *MissionsFuncs_CrazyDealStatus(PyObject *self, PyObject *args) {
	PyObject *number;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &number)) {
		return NULL;
	}
	PyObject *name = Sf_Ask(&f, "Scrap", "GetSaveVar", "(O)", Mf_DealVar(&f, number, ".Var"));
	PyObject *val = Sf_Ask(&f, "Scrap", "GetSaveVar", "(O)", name);
	PyObject *tgt = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(O)", Mf_DealVar(&f, number, ".Tgt")));
	int none = Sf_Compare(&f, val, "==", Py_None);
	val = none > 0 ? Sf_Int(&f, 0) : Sf_ToInt(&f, val);
	return Sf_Return(&f, Sf_Build(&f, "(OOO)", name, val, tgt));
}

/* the number of crazy deals done (-1 with the error set) */
static int Mf_DealsDone(scriptFrame_t *f) {
	int done = 0;
	for (int i = 1; i <= 3 && !f->failed; i++) {
		int mark = f->n;
		PyObject *cd = Mf_Call(f, "CrazyDealStatus", "(i)", i);
		if (Sf_Compare(f, Sf_Item(f, cd, 1), ">=", Sf_Item(f, cd, 2)) > 0) {
			done++;
		}
		Sf_Release(f, mark);
	}
	return f->failed ? -1 : done;
}

/* GetNumCrazyDealCompleted(): the number of crazy deals done */
static PyObject *MissionsFuncs_GetNumCrazyDealCompleted(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	int done = Mf_DealsDone(&f);
	return Sf_Done(&f, done >= 0 ? PyInt_FromLong(done) : NULL);
}

/* OnCrazyDealFinishedMessage(): the crazy gambler calls (the engine's OnCrazyDealFinished), not for the first super
 * deal, which has its own scene */
static PyObject *MissionsFuncs_OnCrazyDealFinishedMessage(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	Sf_Import(&f, "Scorer");
	if (Sf_Compare(&f, Mf_Save(&f, "SuperDeal.Library"), "!=", Sf_Str(&f, "SuperDeal_First")) > 0) {
		Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", "SuperDeal_PayAtenttion_RemoteMessage1", "CrazyGambler");
	}
	return Sf_None(&f);
}

/* OnCrazyDealTarget(): the mission scorer (the engine's OnCrazyDealTarget); in the demo only before all three */
static PyObject *MissionsFuncs_OnCrazyDealTarget(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Scorer");
	Sf_Import(&f, "Init");
	int show = Sf_True(&f, Sf_Mod(&f, "Init", "isDemo")) == 0;
	if (show == 0) {
		PyObject *n = Mf_Call(&f, "GetNumCrazyDealCompleted", "()");
		show = Sf_Compare(&f, n, "<", Sf_Int(&f, 3));
	}
	if (show > 0) {
		Sf_Ask(&f, "Scorer", "ShowMissionScorer", "(ii)", 0, 1);
	}
	return Sf_None(&f);
}

/* NewCrazyDeal(number, varname, maxval): a new crazy deal on the save variable varname */
static PyObject *MissionsFuncs_NewCrazyDeal(PyObject *self, PyObject *args) {
	PyObject *number, *name, *max;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &number, &name, &max)) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(OO)", Mf_DealVar(&f, number, ".Var"), name);
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(Os)", name, "0");
	PyObject *var = Mf_DealVar(&f, number, ".Tgt");
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(OO)", var, Sf_Repr(&f, max));
	return Sf_None(&f);
}

/* SetNextSuperDeal(MapName, MissionName, superDealType): the next super deal */
static PyObject *MissionsFuncs_SetNextSuperDeal(PyObject *self, PyObject *args) {
	PyObject *map, *mission, *type;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &map, &mission, &type)) {
		return NULL;
	}
	Mf_SetSave(&f, "SuperDeal.Map", map);
	Mf_SetSave(&f, "SuperDeal.Library", mission);
	Mf_SetSave(&f, "SuperDeal.Type", type);
	return Sf_None(&f);
}

/* StartActualSuperDeal(): the mission is kept aside and the super deal starts (once) */
static PyObject *MissionsFuncs_StartActualSuperDeal(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	int start = Sf_True(&f, Mf_Save(&f, "SuperDeal.Mission.Map")) == 0;
	if (start > 0) {
		start = Sf_True(&f, Mf_Save(&f, "SuperDeal.Mission.Map")) == 0;
	}
	if (start > 0) {
		Mf_SetSave(&f, "SuperDeal.Mission.Map", Mf_Save(&f, "Mission.Map"));
		Mf_SetSave(&f, "SuperDeal.Mission.Library", Mf_Save(&f, "Mission.Library"));
		PyObject *fn = Sf_Global(&f, MF, "StartSuperDeal");
		PyObject *map = Mf_Save(&f, "SuperDeal.Map");
		PyObject *lib = Mf_Save(&f, "SuperDeal.Library");
		Sf_Call(&f, fn, "(OOO)", map, lib, Mf_Save(&f, "SuperDeal.Type"));
	} else if (start == 0) {
		Sf_Ask(&f, "Scrap", "ConsoleError", "(s)", "Se ha llamado dos veces a MissionsFuncs.StartActualSuperDeal\n");
	}
	return Sf_None(&f);
}

/* EndOfSuperDeal(DealerMessage): the mission back, to the crazy gambler's meeting room with his message */
static PyObject *MissionsFuncs_EndOfSuperDeal(PyObject *self, PyObject *args) {
	PyObject *msg;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &msg)) {
		return NULL;
	}
	Sf_Import(&f, "SaveGame");
	Mf_SetSave(&f, "Mission.Map", Mf_Save(&f, "SuperDeal.Mission.Map"));
	Mf_SetSave(&f, "Mission.Library", Mf_Save(&f, "SuperDeal.Mission.Library"));
	Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "SuperDeal.Mission.Map");
	Sf_Ask(&f, "Scrap", "DelSaveVars", "(s)", "SuperDeal.Mission.Library");
	Mf_SetSave(&f, "InitialMessage", Sf_Lang(&f, msg));
	Mf_SetSaveS(&f, "AutoSaveGameOnLoad", "1");
	Mf_SetSaveS(&f, "Spawn", "DM_Player_Spawn_Meeting_Room");
	Mf_SetSaveS(&f, "ComeFrom", "");
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Sf_Ask(&f, "SaveGame", "QLoad", "(s)", "Levels/GamblinDen");
	return Sf_None(&f);
}

/* StartSuperDeal(MapName, MissionName, superDealType): Dtritus at the beginning of the super deal's level */
static PyObject *MissionsFuncs_StartSuperDeal(PyObject *self, PyObject *args) {
	PyObject *map, *mission, *type;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &map, &mission, &type)) {
		return NULL;
	}
	Sf_Import(&f, "SaveGame");
	Mf_SetSaveS(&f, "Spawn", "DM_Player_Spawn_Beginning");
	Mf_SetSaveS(&f, "Char", "Dtritus");
	Mf_SetSaveS(&f, "ComeFrom", "");
	Mf_SetSave(&f, "SuperDealType", type);
	Sf_Ask(&f, "SaveGame", "QWayPoint", "()");
	Mf_Call(&f, "SetNextMission", "(OO)", mission, map);
	Sf_Ask(&f, "SaveGame", "QLoad", "(O)", Sf_Cat(&f, "sO", "Levels/", map));
	return Sf_None(&f);
}

/* the first entity of the entity list listName */
static PyObject *Mf_First(scriptFrame_t *f, PyObject *list) {
	return Sf_Entity(f, Sf_Ask(f, "Scrap", "EntityListGet", "(Os)", list, "First"));
}

/* RelaxShipsInList(listName): the ships of the list leave it and relax */
static PyObject *MissionsFuncs_RelaxShipsInList(PyObject *self, PyObject *args) {
	PyObject *list;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &list)) {
		return NULL;
	}
	PyObject *e = Mf_First(&f, list);
	while (Sf_True(&f, e) > 0) {
		int mark = f.n;
		Sf_SetAttrS(&f, e, "EntityList", "");
		if (Sf_True(&f, Sf_Attr(&f, e, "Active")) > 0) {
			Sf_Ask(&f, "SAI", "SetStateVehicle", "(iOi)", 3, Sf_Attr(&f, e, "Name"), 1);
		}
		PyObject *next = Mf_First(&f, list);
		/* e lives below the mark of the turn before: keep the next one, drop the rest of this turn */
		Py_XINCREF(next);
		Sf_Release(&f, mark);
		e = Sf_Keep(&f, next);
	}
	return Sf_None(&f);
}

/* SetInvulnerableShipsInList(listName, invulState=1): every ship of the list */
static PyObject *MissionsFuncs_SetInvulnerableShipsInList(PyObject *self, PyObject *args) {
	PyObject *list, *state = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &list, &state)) {
		return NULL;
	}
	if (!state) {
		state = Sf_Int(&f, 1);
	}
	PyObject *e = Mf_First(&f, list);
	while (Sf_True(&f, e) > 0) {
		int mark = f.n;
		Sf_SetAttr(&f, e, "Invulnerable", state);
		PyObject *next = Sf_Entity(&f, Sf_Attr(&f, e, "NextInList"));
		Py_XINCREF(next);
		Sf_Release(&f, mark);
		e = Sf_Keep(&f, next);
	}
	return Sf_None(&f);
}

/* (n, name, textColor, colour) of a score table, with a colour global of the module */
static PyObject *Mf_Row(scriptFrame_t *f, PyObject *n, PyObject *name, const char *colour) {
	PyObject *text = Sf_Global(f, MF, "textColor");
	return Sf_Build(f, "(OOOO)", n, name, text, Sf_Global(f, MF, colour));
}

/* RacerScorer.var + add */
static PyObject *Mf_Score(scriptFrame_t *f, const char *var, long add) {
	PyObject *v = Sf_Mod(f, "RacerScorer", var);
	return add ? Sf_OpI(f, v, '+', add) : v;
}

/* OnSuperDealFinish(victory, superDealType="", retryFunc=""): the camera of the level, the messages and the score
 * table of the super deal (0 lost with a retry, 1 won, 2 lost) */
static PyObject *MissionsFuncs_OnSuperDealFinish(PyObject *self, PyObject *args) {
	static const char *const tops[] = { "SuperDeal_YouLoose", "SuperDeal_YouWin", "SuperDeal_YouLoose" };
	PyObject *victory, *type = NULL, *retry = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O|OO", &victory, &type, &retry)) {
		return NULL;
	}
	if (!type) {
		type = Sf_Str(&f, "");
	}
	if (!retry) {
		retry = Sf_Str(&f, "");
	}
	Sf_Import(&f, "RacerScorer");
	PyObject *cam = Sf_Global(&f, MF, "cameraData");
	if (Sf_True(&f, cam) > 0) {
		PyObject *c = Sf_Ask(&f, "Scrap", "CreateEntity", "(siiis)", "Camara", 0, 0, 0, "Cam");
		Sf_SetAttr(&f, c, "camPos", Sf_Item(&f, Sf_Global(&f, MF, "cameraData"), 0));
		Sf_SetAttr(&f, c, "camAng", Sf_Item(&f, Sf_Global(&f, MF, "cameraData"), 1));
		Sf_Ask(&f, "Scrap", "SetCam", "(iO)", 0, Sf_Attr(&f, c, "Name"));
		Sf_SetGlobal(&f, MF, "cameraData", Py_None);
	}
	int v = -1;
	for (int i = 0; i < 3 && v < 0 && !f.failed; i++) {
		if (Sf_EqI(&f, victory, i) > 0) {
			v = i;
		}
	}
	if (v >= 0) {
		Sf_SetMod(&f, "RacerScorer", "ScorerTopMessage", Mf_Lang(&f, tops[v]));
		Sf_SetMod(&f, "RacerScorer", "ScorerBotMessage", v == 0 ? Mf_Lang(&f, "SuperDeal_TryAgain") : Sf_Str(&f, ""));
	}
	PyObject *data = NULL;
	if (Sf_EqS(&f, type, "DeathMatch") > 0) {
		Sf_Import(&f, "DeathMatch");
		PyObject *fn = Sf_Global(&f, MF, "BuildMultipleEnemiesData");
		data = Sf_Call(&f, fn, "(OO)", victory, Sf_Mod(&f, "DeathMatch", "enemiesFrags"));
	} else if (Sf_EqS(&f, type, "TeamDeathMatch") > 0) {
		data = Mf_Call(&f, "BuildTeamDeathMatchData", "(O)", victory);
	} else if (Sf_EqS(&f, type, "Versus") > 0) {
		data = Mf_Call(&f, "BuildVersusData", "(O)", victory);
	} else if (Sf_EqS(&f, type, "MortalRace") > 0) {
		Sf_Import(&f, "MortalRace");
		PyObject *fn = Sf_Global(&f, MF, "BuildMultipleEnemiesData");
		data = Sf_Call(&f, fn, "(OO)", victory, Sf_Mod(&f, "MortalRace", "BayScorer"));
	} else if (Sf_EqS(&f, type, "Domination") > 0) {
		data = Mf_Call(&f, "BuildDominationData", "(O)", victory);
	} else if (Sf_EqS(&f, type, "CTF") > 0) {
		data = Mf_Call(&f, "BuildCTFData", "(O)", victory);
	}
	if (v >= 0 && !data) {
		/* the unbound local of the script for another type */
		Sf_Raise(&f, PyExc_NameError, "data");
	}
	if (v == 0) {
		Sf_Ask(&f, "RacerScorer", "ShowScorer", "(iOiOs)", 0, data, 1, retry, "MissionsFuncs.SuperDealOnDefeat");
	} else if (v == 1) {
		Sf_Ask(&f, "RacerScorer", "ShowScorer", "(iOis)", 0, data, 2, "MissionsFuncs.SuperDealOnVictory");
	} else if (v == 2) {
		Sf_Ask(&f, "RacerScorer", "ShowScorer", "(iOiO)", 0, data, 2, retry);
	}
	return Sf_None(&f);
}

/* BuildMultipleEnemiesData(playerWins, enemiesDict): the score table of all against all, sorted by the frags
 * (the player's one more when he won), Dtritus and the enemies' names */
static PyObject *MissionsFuncs_BuildMultipleEnemiesData(PyObject *self, PyObject *args) {
	PyObject *wins, *enemies;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &wins, &enemies)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	PyObject *frags = Sf_Keep(&f, PyList_New(0));
	PyObject *keys = Sf_Method(&f, enemies, "keys", "()");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *k = Sf_Next(&f, keys, i);
		if (!k) {
			break;
		}
		Sf_Method(&f, frags, "append", "(O)", Sf_GetItem(&f, enemies, k));
		Sf_Release(&f, mark);
	}
	int lost = Sf_EqI(&f, wins, 0);
	if (lost == 0) {
		lost = Sf_EqI(&f, wins, 2);
	}
	PyObject *player = lost > 0 ? Mf_Score(&f, "missionYou", 0) : Mf_Score(&f, "missionYou", 1);
	Sf_Method(&f, frags, "append", "(O)", player);
	Sf_Method(&f, frags, "sort", "()");
	Sf_Method(&f, frags, "reverse", "()");
	int n = Sf_Len(&f, frags), index = 0;
	for (int i = 0; i < n && !f.failed; i++) {
		if (Sf_Compare(&f, Sf_Item(&f, frags, i), "==", player) > 0) {
			index = i;
			break;
		}
	}
	PyObject *data = Sf_Keep(&f, PyList_New(0));
	n = Sf_Len(&f, frags);
	long enemy = 0;
	for (int i = 0; i < n && !f.failed; i++) {
		int mark = f.n;
		PyObject *row;
		if (i == index) {
			PyObject *fr = Sf_Item(&f, frags, i);
			row = Mf_Row(&f, fr, Mf_Lang(&f, "CharName_Dtritus"), "bgColorGreen");
		} else {
			PyObject *fr = Sf_Item(&f, frags, i);
			row = Mf_Row(&f, fr, Sf_Item(&f, Sf_Global(&f, MF, "enemiesNames"), enemy), "bgColorRed");
			enemy++;
		}
		Sf_Method(&f, data, "append", "(O)", row);
		Sf_Release(&f, mark);
	}
	return Sf_Return(&f, data);
}

/* BuildTeamDeathMatchData(playerWins): the two teams */
static PyObject *MissionsFuncs_BuildTeamDeathMatchData(PyObject *self, PyObject *args) {
	PyObject *wins;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &wins)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	int won = Sf_True(&f, wins);
	PyObject *data = NULL;
	if (won == 0) {
		PyObject *a = Mf_Score(&f, "missionBad", 1);
		PyObject *ra = Mf_Row(&f, a, Mf_Lang(&f, "SuperDeal_EnemyTeam"), "bgColorRed");
		PyObject *b = Mf_Score(&f, "missionYou", 0);
		PyObject *rb = Mf_Row(&f, b, Mf_Lang(&f, "SuperDeal_DtritusTeam"), "bgColorGreen");
		data = Sf_Build(&f, "[OO]", ra, rb);
	} else if (won > 0) {
		PyObject *a = Mf_Score(&f, "missionYou", 1);
		PyObject *ra = Mf_Row(&f, a, Mf_Lang(&f, "SuperDeal_DtritusTeam"), "bgColorGreen");
		PyObject *b = Mf_Score(&f, "missionBad", 0);
		PyObject *rb = Mf_Row(&f, b, Mf_Lang(&f, "SuperDeal_EnemyTeam"), "bgColorRed");
		data = Sf_Build(&f, "[OO]", ra, rb);
	}
	return Sf_Return(&f, data);
}

/* BuildVersusData(playerWins): Dtritus and the one foe of the mission */
static PyObject *MissionsFuncs_BuildVersusData(PyObject *self, PyObject *args) {
	PyObject *wins;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &wins)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	PyObject *foe = NULL;
	if (Sf_EqS(&f, Mf_Save(&f, "Mission.Library"), "SuperDeal_First") > 0) {
		foe = Sf_Str(&f, "Scrapus");
	} else if (Sf_EqS(&f, Mf_Save(&f, "Mission.Library"), "Mission_IgorCombat") > 0) {
		foe = Mf_Lang(&f, "CharName_Igor");
	} else {
		foe = Sf_Str(&f, "Multiplus");
	}
	int won = Sf_True(&f, wins);
	PyObject *data = NULL;
	if (won == 0) {
		PyObject *a = Mf_Score(&f, "missionBad", 1);
		PyObject *ra = Mf_Row(&f, a, foe, "bgColorRed");
		PyObject *b = Mf_Score(&f, "missionYou", 0);
		PyObject *rb = Mf_Row(&f, b, Mf_Lang(&f, "CharName_Dtritus"), "bgColorGreen");
		data = Sf_Build(&f, "[OO]", ra, rb);
	} else if (won > 0) {
		PyObject *a = Mf_Score(&f, "missionYou", 1);
		PyObject *ra = Mf_Row(&f, a, Mf_Lang(&f, "CharName_Dtritus"), "bgColorGreen");
		PyObject *b = Mf_Score(&f, "missionBad", 0);
		PyObject *rb = Mf_Row(&f, b, foe, "bgColorRed");
		data = Sf_Build(&f, "[OO]", ra, rb);
	}
	return Sf_Return(&f, data);
}

/* int(Scrap.GetSaveVar(name)) */
static PyObject *Mf_SaveInt(scriptFrame_t *f, const char *name) {
	return Sf_ToInt(f, Mf_Save(f, name));
}

/* BuildDominationData(playerWins): the rounds of the two teams */
static PyObject *MissionsFuncs_BuildDominationData(PyObject *self, PyObject *args) {
	PyObject *wins;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &wins)) {
		return NULL;
	}
	int won = Sf_True(&f, wins);
	PyObject *data = NULL;
	if (won == 0) {
		PyObject *a = Mf_SaveInt(&f, "DominationRounds");
		PyObject *ra = Mf_Row(&f, a, Mf_Lang(&f, "SuperDeal_EnemyTeam"), "bgColorRed");
		PyObject *b = Mf_SaveInt(&f, "PlayerRounds");
		PyObject *rb = Mf_Row(&f, b, Mf_Lang(&f, "SuperDeal_DtritusTeam"), "bgColorGreen");
		data = Sf_Build(&f, "[OO]", ra, rb);
	} else if (won > 0) {
		PyObject *a = Mf_SaveInt(&f, "DominationRounds");
		PyObject *ra = Mf_Row(&f, a, Mf_Lang(&f, "SuperDeal_DtritusTeam"), "bgColorGreen");
		PyObject *b = Mf_SaveInt(&f, "EnemyRounds");
		PyObject *rb = Mf_Row(&f, b, Mf_Lang(&f, "SuperDeal_EnemyTeam"), "bgColorRed");
		data = Sf_Build(&f, "[OO]", ra, rb);
	}
	return Sf_Return(&f, data);
}

/* BuildCTFData(playerWins): the flags of the two teams */
static PyObject *MissionsFuncs_BuildCTFData(PyObject *self, PyObject *args) {
	PyObject *wins;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &wins)) {
		return NULL;
	}
	Sf_Import(&f, "CTF");
	int won = Sf_True(&f, wins);
	PyObject *data = NULL;
	if (won == 0) {
		PyObject *a = Sf_Mod(&f, "CTF", "flags");
		PyObject *ra = Mf_Row(&f, a, Mf_Lang(&f, "SuperDeal_EnemyTeam"), "bgColorRed");
		PyObject *b = Sf_Mod(&f, "CTF", "flagsFriends");
		PyObject *rb = Mf_Row(&f, b, Mf_Lang(&f, "SuperDeal_DtritusTeam"), "bgColorGreen");
		data = Sf_Build(&f, "[OO]", ra, rb);
	} else if (won > 0) {
		PyObject *a = Sf_Mod(&f, "CTF", "flags");
		PyObject *ra = Mf_Row(&f, a, Mf_Lang(&f, "SuperDeal_DtritusTeam"), "bgColorGreen");
		PyObject *b = Sf_Mod(&f, "CTF", "flagsEnemies");
		PyObject *rb = Mf_Row(&f, b, Mf_Lang(&f, "SuperDeal_EnemyTeam"), "bgColorRed");
		data = Sf_Build(&f, "[OO]", ra, rb);
	}
	return Sf_Return(&f, data);
}

/* exec(Scrap.GetSaveVar(var)) when it holds code; on victory the super deal type is cleared first */
static PyObject *Mf_SuperDealEnd(PyObject *args, const char *var, byte victory) {
	PyObject *id, *control;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OO", &id, &control)) {
		return NULL;
	}
	PyObject *cad = Mf_Save(&f, var);
	int run = Sf_True(&f, cad);
	if (run > 0) {
		run = Sf_Compare(&f, cad, "!=", Sf_Str(&f, ""));
	}
	if (run > 0) {
		if (victory) {
			Mf_SetSaveS(&f, "SuperDealType", "");
		}
		Sf_Exec(&f, cad, MF, Sf_Build(&f, "{sOsOsO}", "id", id, "control", control, "cad", cad));
	}
	return Sf_None(&f);
}

/* SuperDealOnDefeat(id, control), SuperDealOnVictory(id, control): the code the super deal left in OnAbort or
 * OnVictory */
static PyObject *MissionsFuncs_SuperDealOnDefeat(PyObject *self, PyObject *args) {
	(void)self;
	return Mf_SuperDealEnd(args, "OnAbort", 0);
}

static PyObject *MissionsFuncs_SuperDealOnVictory(PyObject *self, PyObject *args) {
	(void)self;
	return Mf_SuperDealEnd(args, "OnVictory", 1);
}

PyMethodDef g_MissionsFuncsMethods[] = {
	{ "Init", MissionsFuncs_Init, 1, NULL },
	{ "TryMsg", MissionsFuncs_TryMsg, 1, NULL },
	{ "SetNextMission", MissionsFuncs_SetNextMission, 1, NULL },
	{ "SetFirstDemoMission", MissionsFuncs_SetFirstDemoMission, 1, NULL },
	{ "SetOutMissionArrow", MissionsFuncs_SetOutMissionArrow, 1, NULL },
	{ "ActivateTarget", MissionsFuncs_ActivateTarget, 1, NULL },
	{ "GetSecondary", MissionsFuncs_GetSecondary, 1, NULL },
	{ "SetSecondary", MissionsFuncs_SetSecondary, 1, NULL },
	{ "FinishGame", MissionsFuncs_FinishGame, 1, NULL },
	{ "ShowMissionInfo", MissionsFuncs_ShowMissionInfo, 1, NULL },
	{ "SetMissionTarget", MissionsFuncs_SetMissionTarget, 1, NULL },
	{ "SetMultipleMissionTargets", MissionsFuncs_SetMultipleMissionTargets, 1, NULL },
	{ "CleanAllTargets", MissionsFuncs_CleanAllTargets, 1, NULL },
	{ "DelMissionTarget", MissionsFuncs_DelMissionTarget, 1, NULL },
	{ "IsCharMissionObjective", MissionsFuncs_IsCharMissionObjective, 1, NULL },
	{ "CharMissionDialog", MissionsFuncs_CharMissionDialog, 1, NULL },
	{ "CancelSecondaryMission", MissionsFuncs_CancelSecondaryMission, 1, NULL },
	{ "SinglePlayerDeath", MissionsFuncs_SinglePlayerDeath, 1, NULL },
	{ "CreatePhotoObject", MissionsFuncs_CreatePhotoObject, 1, NULL },
	{ "UnsetPhotoObject", MissionsFuncs_UnsetPhotoObject, 1, NULL },
	{ "ReassignBertosPlayerTarget", MissionsFuncs_ReassignBertosPlayerTarget, 1, NULL },
	{ "CreateBerto", MissionsFuncs_CreateBerto, 1, NULL },
	{ "CreateBertos", MissionsFuncs_CreateBertos, 1, NULL },
	{ "OnBertoDeath", MissionsFuncs_OnBertoDeath, 1, NULL },
	{ "CrazyDealStatus", MissionsFuncs_CrazyDealStatus, 1, NULL },
	{ "GetNumCrazyDealCompleted", MissionsFuncs_GetNumCrazyDealCompleted, 1, NULL },
	{ "OnCrazyDealFinishedMessage", MissionsFuncs_OnCrazyDealFinishedMessage, 1, NULL },
	{ "OnCrazyDealTarget", MissionsFuncs_OnCrazyDealTarget, 1, NULL },
	{ "NewCrazyDeal", MissionsFuncs_NewCrazyDeal, 1, NULL },
	{ "SetNextSuperDeal", MissionsFuncs_SetNextSuperDeal, 1, NULL },
	{ "StartActualSuperDeal", MissionsFuncs_StartActualSuperDeal, 1, NULL },
	{ "EndOfSuperDeal", MissionsFuncs_EndOfSuperDeal, 1, NULL },
	{ "StartSuperDeal", MissionsFuncs_StartSuperDeal, 1, NULL },
	{ "RelaxShipsInList", MissionsFuncs_RelaxShipsInList, 1, NULL },
	{ "SetInvulnerableShipsInList", MissionsFuncs_SetInvulnerableShipsInList, 1, NULL },
	{ "OnSuperDealFinish", MissionsFuncs_OnSuperDealFinish, 1, NULL },
	{ "BuildMultipleEnemiesData", MissionsFuncs_BuildMultipleEnemiesData, 1, NULL },
	{ "BuildTeamDeathMatchData", MissionsFuncs_BuildTeamDeathMatchData, 1, NULL },
	{ "BuildVersusData", MissionsFuncs_BuildVersusData, 1, NULL },
	{ "BuildDominationData", MissionsFuncs_BuildDominationData, 1, NULL },
	{ "BuildCTFData", MissionsFuncs_BuildCTFData, 1, NULL },
	{ "SuperDealOnDefeat", MissionsFuncs_SuperDealOnDefeat, 1, NULL },
	{ "SuperDealOnVictory", MissionsFuncs_SuperDealOnVictory, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
