/*
 * Lines the level scripts repeat in their maps and missions (src/game/Script/levels/Levels.h): the discardable
 * characters of the Xbox, the callbacks of the mission characters, the speakers of a talk, a character that stays
 * where it is, the text of a mission scorer, the blueprint lists of the save variables, the remote coms, the phone
 * cabs and the mission items; and Mission_DestroyCommSats1, 2 and 3, one script in three levels.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/input.h"
#include "scrap/scrap.h"

#include "Levels.h"

int Lv_PcOr(scriptFrame_t *f, const char *module, const char *name) {
	int x = Sf_True(f, Sf_Ask(f, "Scrap", "Get", "(s)", "IsXbox"));
	return x > 0 ? Sf_True(f, Sf_Global(f, module, name)) : x < 0 ? -1 : 1;
}

void Lv_Callback(scriptFrame_t *f, const char *name, const char *module, const char *func) {
	PyObject *cb = Sf_Attr(f, Sf_GetItem(f, Sf_Mod(f, "CharsNPC", "CharRes"), Sf_Str(f, name)), "CallbackFunc");
	PyObject *append = Sf_Attr(f, cb, "append");
	Sf_Call(f, append, "(O)", Sf_Global(f, module, func));
}

void Lv_MissionChar(scriptFrame_t *f, const char *name, const char *module, const char *func) {
	Sf_Ask(f, "CharsNPC", "CreateMissionChar", "(s)", name);
	Lv_Callback(f, name, module, func);
}

void Lv_Speakers(scriptFrame_t *f, PyObject *usr, PyObject *ch) {
	Sf_SetMod(f, "Speech", "PlayerName", usr);
	Sf_SetMod(f, "Speech", "NPCName", ch);
}

void Lv_OnEnd(scriptFrame_t *f, const char *module, const char *func, PyObject *params) {
	Sf_SetMod(f, "Speech", "OnEndOfSpeech", Sf_Global(f, module, func));
	Sf_SetMod(f, "Speech", "OnEndOfSpeechParams", params ? params : Sf_Build(f, "()"));
}

void Lv_Stay(scriptFrame_t *f, PyObject *e, long state, PyObject *target) {
	Sf_SetAttrI(f, e, "LLogicDefaultState", state);
	Sf_SetAttr(f, e, "LLogicDefaultTarget", target);
	Sf_SetAttrI(f, e, "LLogicState", state);
	Sf_SetAttr(f, e, "LLogicTarget", target);
	Sf_SetAttrI(f, e, "LLogicIdle", 1);
	Sf_SetAttrI(f, e, "LLogicIdle", 0);
}

void Lv_ScorerSet(scriptFrame_t *f, PyObject *id, const char *item, const char *var, PyObject *v) {
	if (v) {
		Sf_Ask(f, "SScorer", "Set", "(OssO)", id, item, var, v);
	} else {
		Sf_Check(f, 0);
	}
}

void Lv_ScorerTextBegin(scriptFrame_t *f, PyObject *id, const char *item, long dy) {
	int mark = f->n;
	Sf_Ask(f, "SScorer", "Add", "(Oss)", id, item, "Text");
	Lv_ScorerSet(f, id, item, "X", Sf_Mod(f, "Scorer", "MissionScorerText_X"));
	PyObject *y = Sf_Mod(f, "Scorer", "MissionScorerText_Y");
	Lv_ScorerSet(f, id, item, "Y", dy ? Sf_OpI(f, y, '+', dy) : y);
	Lv_ScorerSet(f, id, item, "Font", Sf_Str(f, "Horatio"));
	Lv_ScorerSet(f, id, item, "Effect", Sf_Str(f, "Shadow"));
	Sf_Release(f, mark);
}

void Lv_ScorerTextEnd(scriptFrame_t *f, PyObject *id, const char *item) {
	int mark = f->n;
	Lv_ScorerSet(f, id, item, "Align", Sf_Str(f, "Left"));
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	for (int i = 0; i < 4; i++) {
		Lv_ScorerSet(f, id, item, rgba[i], Sf_Item(f, Sf_Mod(f, "Scorer", "MissionScorerText_Color"), i));
	}
	Sf_Release(f, mark);
}

PyObject *Lv_SaveList(scriptFrame_t *f, const char *var, const char *module) {
	return Sf_Eval(f, Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", var), module);
}

void Lv_SaveAppend(scriptFrame_t *f, const char *var, PyObject *lst, PyObject *item) {
	Sf_Method(f, lst, "append", "(O)", item);
	Sf_Ask(f, "Scrap", "SetSaveVar", "(sO)", var, Sf_Repr(f, lst));
}

const int lv_black[4] = { 0, 0, 0, 255 }, lv_clear[4] = { 0, 0, 0, 0 };

int Lv_SaveIs(scriptFrame_t *f, const char *name, const char *text) {
	return Sf_EqS(f, Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", name), text);
}

void Lv_Fade(scriptFrame_t *f, const int from[4], const int to[4], double time) {
	Sf_Ask(f, "SScorer", "Fade", "(i(iiii)(iiii)d)", 0, from[0], from[1], from[2], from[3], to[0], to[1], to[2], to[3], time);
}

PyObject *Lv_Dummy(scriptFrame_t *f, const char *pattern, int flag) {
	Sf_Ask(f, "Scrap", "StartDummySearch", "(si)", pattern, flag);
	return Sf_Ask(f, "Scrap", "NextDummySearch", "()");
}

byte Lv_AcShips(scriptFrame_t *f, const char *module, const char *ships) {
	int mark = f->n;
	Sf_Import(f, "Traffic");
	Sf_Run(f, "Scrap", "SetSaveVar", "(ss)", "Traffic.AcShips", ships);
	Sf_SetMod(f, "Traffic", "AcShips", Sf_Eval(f, Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", "Traffic.AcShips"), module));
	Sf_Release(f, mark);
	return !f->failed;
}

PyObject *Lv_Profile(scriptFrame_t *f, const char *module, const char *const *vars) {
	Sf_Import(f, "VehiclesNPC");
	PyObject *sp = Sf_Ask(f, "VehiclesNPC", "cShipAI", "()");
	Sf_SetAttrs(f, sp, module, vars);
	return sp;
}

int Lv_UsrIs(scriptFrame_t *f, PyObject *usr, PyObject *name) {
	int t = Sf_True(f, usr);
	return t > 0 ? Sf_Compare(f, name, "==", Sf_Attr(f, usr, "Name")) : t;
}

PyObject *Lv_SpawnCarAt(scriptFrame_t *f, const char *name, byte nearest, PyObject **spw) {
	PyObject *s = Sf_Ask(f, "OutMap", nearest ? "GetNearestFreeSpawnPos" : "GetFreeSpawnPos", "()");
	PyObject *fn = Sf_Mod(f, "Scrap", "CreateEntity");
	PyObject *p = nearest ? s : Sf_Item(f, s, 0);
	PyObject *x = Sf_Item(f, p, 0), *y = Sf_Item(f, p, 1), *z = Sf_Item(f, p, 2);
	if (spw) {
		*spw = s;
	}
	return Sf_Call(f, fn, "(sOOOs)", name, x, y, z, "Car");
}

PyObject *Lv_SpawnCar(scriptFrame_t *f, const char *name, byte nearest) {
	return Lv_SpawnCarAt(f, name, nearest, NULL);
}

byte Lv_StdAI(scriptFrame_t *f, PyObject *e, PyObject *pd) {
	Sf_Run(f, "VehiclesNPC", "SetStdProperties", "(OO)", Sf_Attr(f, e, "Name"), Sf_Attr(f, pd, "Ship"));
	return Sf_Run(f, "VehiclesNPC", "ModAIProff", "(OO)", e, pd);
}

PyObject *Lv_StdProfile(scriptFrame_t *f, const char *module, const char *profile, PyObject *e) {
	PyObject *pd = Sf_Own(f, module, profile, "()");
	Lv_StdAI(f, e, pd);
	return pd;
}

byte Lv_Life(scriptFrame_t *f, PyObject *e, const char *module, const char *life) {
	PyObject *v = Sf_Global(f, module, life);
	return Sf_SetAttr(f, e, "Life", v) && Sf_SetAttr(f, e, "MaxLife", v);
}

PyObject *Lv_PhoneCab(scriptFrame_t *f, const char *use, byte targetFirst, const char *pcModule) {
	Sf_Import(f, "PhoneCab");
	PyObject *pc = Sf_Ask(f, "PhoneCab", "GetFree", "()");
	if (pcModule) {
		Sf_SetGlobal(f, pcModule, "pc", pc);
	}
	if (!targetFirst) {
		Sf_SetAttrS(f, pc, "UseFunc", use);
	}
	Sf_Run(f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(f, pc, "Name"));
	if (targetFirst) {
		Sf_SetAttrS(f, pc, "UseFunc", use);
	}
	return pc;
}

byte Lv_DummyPosAng(scriptFrame_t *f, PyObject *dummy, PyObject **pos, PyObject **ang) {
	PyObject *p = Sf_Item(f, dummy, 1);
	PyObject *x = Sf_Item(f, p, 0), *y = Sf_Item(f, p, 1), *z = Sf_Item(f, p, 2);
	*pos = Sf_Build(f, "(OOO)", x, y, z);
	PyObject *a = Sf_Item(f, dummy, 2);
	PyObject *ax = Sf_Item(f, a, 0), *ay = Sf_Item(f, a, 1);
	*ang = Sf_Build(f, "(OOi)", ax, ay, 0);
	return !f->failed;
}

byte Lv_RaiseValue(scriptFrame_t *f, const char *module, PyObject *o) {
	Sf_Import(f, module);
	return Sf_RaiseValue(f, o);
}

PyObject *Lv_SndElements(PyObject *args, const lvSndElement_t *table) {
	PyObject *name, *x, *y, *z, *angx, *angy;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOOOOO", &name, &x, &y, &z, &angx, &angy)) {
		return NULL;
	}
	Sf_Import(&f, "Sound");
	Sf_SetGlobal(&f, "MapSnd", "cntr", Sf_OpI(&f, Sf_Global(&f, "MapSnd", "cntr"), '+', 1));
	const lvSndElement_t *el = NULL;
	for (const lvSndElement_t *t = table; t->name && !el && !f.failed; t++) {
		if (Sf_EqS(&f, name, t->name) > 0) {
			el = t;
		}
	}
	if (!el) {
		return Sf_None(&f);
	}
	if (el->loop && (!el->pc || Sf_True(&f, Sf_Global(&f, "MapSnd", "isPC")) > 0)) {
		PyObject *id = Sf_Cat(&f, "OO", name, Sf_Repr(&f, Sf_Global(&f, "MapSnd", "cntr")));
		PyObject *pos = Sf_Build(&f, "(OOO)", x, y, z);
		if (el->front) {
			PyObject *off = Sf_Ask(&f, "SVec", "Rotate3D", "((iii)(OOi))", 0, 0, -300, angx, angy, 0);
			pos = Sf_Ask(&f, "SVec", "Add", "(OO)", pos, off);
		}
		if (el->hasPitch) {
			Sf_Ask(&f, "Sound", "SetPSLoop", "(OOsdi)", id, pos, el->loop, el->vol, el->pitch);
		} else {
			Sf_Ask(&f, "Sound", "SetPSLoop", "(OOsd)", id, pos, el->loop, el->vol);
		}
	}
	if (el->beep && !f.failed) {
		const char *beep = el->beep;
		if (el->beepXbox) {
			int pc = Sf_True(&f, Sf_Global(&f, "MapSnd", "isPC"));
			beep = pc ? el->beep : el->beepXbox;
		}
		PyObject *n = Sf_Repr(&f, Sf_Global(&f, "MapSnd", "cntr"));
		PyObject *id = el->beepI ? Sf_Cat(&f, "sOO", "I", name, n) : Sf_Cat(&f, "OO", name, n);
		Sf_Ask(&f, "Sound", "SetPSInterval", "(O(OOO)siidi)", id, x, y, z, beep, el->imin, el->imax, el->ivol, 0);
	}
	return Sf_None(&f);
}

PyObject *Lv_PlanesInit(PyObject *args, const char *module) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "MissionsFuncs");
	Sf_Import(&f, "CharAct");
	Sf_SetMod(&f, "CharAct", "TakenPhotoMap", Sf_Global(&f, module, "TakenPhoto"));
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_LocalPlane_*", 1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "==", Py_None) != 0) {
			break;
		}
		PyObject *plane = Sf_Slice(&f, Sf_Item(&f, dummy, 0), 14, 0x7fffffff);
		if (Sf_In(&f, plane, Sf_Op(&f, Sf_Global(&f, module, "Naves"), '+', Sf_Global(&f, module, "Motores"))) > 0) {
			PyObject *ang = Sf_Item(&f, dummy, 2);
			PyObject *name = Sf_Cat(&f, "sO", "Plano", Sf_Slice(&f, Sf_Item(&f, dummy, 0), 14, 0x7fffffff));
			PyObject *pln = Sf_Ask(&f, "MissionsFuncs", "CreatePhotoObject", "(OOO)", name, Sf_Item(&f, dummy, 1),
				Sf_Build(&f, "(OOi)", Sf_Item(&f, ang, 0), Sf_Item(&f, ang, 1), 0));
			Sf_Ask(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", pln, 0);
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

PyObject *Lv_PlanesPhoto(PyObject *args, const char *module, const char *ships, const char *engines) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	if (Sf_EqS(&f, Sf_Slice(&f, name, 0, 5), "Plano") <= 0) {
		return Sf_None(&f);
	}
	long res = 0;
	Sf_Import(&f, "string");
	Sf_Import(&f, "RaceMaker");
	PyObject *plano = Sf_Slice(&f, name, 5, 0x7fffffff);
	int ship = Sf_In(&f, plano, Sf_Global(&f, module, "Naves"));
	int engine = ship == 0 ? Sf_In(&f, plano, Sf_Global(&f, module, "Motores")) : 0;
	if (ship > 0) {
		Sf_Method(&f, Sf_Global(&f, module, "Naves"), "remove", "(O)", plano);
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", ships, Sf_Repr(&f, Sf_Global(&f, module, "Naves")));
		Lv_SaveAppend(&f, "Hangar.shipsToEditList", Lv_SaveList(&f, "Hangar.shipsToEditList", module), plano);
		PyObject *key = Sf_Cat(&f, "sO", "SHIP_", Sf_Ask(&f, "string", "upper", "(O)", plano));
		PyObject *text = Sf_Ask(&f, "string", "replace", "(OsO)", Sf_Lang(&f, Sf_Str(&f, "OnPhotoShipBlueprints")), "<n1>", Sf_Lang(&f, key));
		Sf_Ask(&f, "RaceMaker", "ShowSystemMessage", "(O)", text);
		res = 1;
	} else if (engine > 0) {
		Sf_Method(&f, Sf_Global(&f, module, "Motores"), "remove", "(O)", plano);
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", engines, Sf_Repr(&f, Sf_Global(&f, module, "Motores")));
		Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.availableEnginesList");
		PyObject *lst = Lv_SaveList(&f, "Hangar.availableEnginesList", module);
		Lv_SaveAppend(&f, "Hangar.availableEnginesList", lst, Sf_Ask(&f, "string", "upper", "(O)", plano));
		PyObject *key = Sf_Cat(&f, "sO", "SHIP_S", Sf_Ask(&f, "string", "upper", "(O)", Sf_Slice(&f, plano, 1, 0x7fffffff)));
		PyObject *text = Sf_Ask(&f, "string", "replace", "(OsO)", Sf_Lang(&f, Sf_Str(&f, "OnPhotoEngineBlueprints")), "<n1>", Sf_Lang(&f, key));
		Sf_Ask(&f, "RaceMaker", "ShowSystemMessage", "(O)", text);
		res = 1;
	}
	if (res && !f.failed) {
		Sf_Import(&f, "SaveGame");
		Sf_SetAttrI(&f, Sf_Entity(&f, name), "ArrowCanView", 0);
		Sf_Ask(&f, "Scrap", "IncSaveVar", "(s)", "NewBluePrintAvaliable");
		Sf_Ask(&f, "SaveGame", "CheckAllBlueprints", "()");
	}
	return Sf_Return(&f, Sf_Int(&f, res));
}

PyObject *Lv_AcShipsStatus(PyObject *args, const char *module, const char *ships) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Lv_AcShips(&f, module, ships);
	return Sf_None(&f);
}

PyObject *Lv_ProfileFunc(PyObject *args, const char *module, const char *const *vars) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Sf_Return(&f, Lv_Profile(&f, module, vars));
}

void Lv_Saves(scriptFrame_t *f, const char *const *pairs) {
	for (; *pairs && !f->failed; pairs += 2) {
		int mark = f->n;
		Sf_Ask(f, "Scrap", "SetSaveVar", "(ss)", pairs[0], pairs[1]);
		Sf_Release(f, mark);
	}
}

PyObject *Lv_StartPos(PyObject *args, const double (*pos)[4], int n) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	Sf_SetMod(&f, "RaceMaker", "StartPos", Sf_Build(&f, "[]"));
	for (int i = 0; i < n && !f.failed; i++) {
		int mark = f.n;
		Sf_Method(&f, Sf_Mod(&f, "RaceMaker", "StartPos"), "append", "((ddd)d)", pos[i][0], pos[i][1], pos[i][2], pos[i][3]);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

void Lv_Colour(scriptFrame_t *f, PyObject *id, const char *item) {
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	for (int i = 0; i < 4 && !f->failed; i++) {
		int mark = f->n;
		Lv_ScorerSet(f, id, item, rgba[i], Sf_Item(f, Sf_Mod(f, "Scorer", "MissionScorerText_Color"), i));
		Sf_Release(f, mark);
	}
}

void Lv_BarDefaults(scriptFrame_t *f, PyObject *id, const char *item, PyObject *main, PyObject *black) {
	int mark = f->n;
	PyObject *fn = Sf_Mod(f, "Scorer", "MissionScorerBar_SetDefaults");
	PyObject *back = Sf_Mod(f, "Scorer", "MissionScorerBar_ColorBack");
	main = main ? main : Sf_Mod(f, "Scorer", "MissionScorerBar_ColorMain");
	black = black ? black : Sf_Mod(f, "Scorer", "MissionScorerBar_ColorBlack");
	Sf_Call(f, fn, "(OsOOO)", id, item, back, main, black);
	Sf_Release(f, mark);
}

int Lv_UsrNameIs(scriptFrame_t *f, PyObject *usr, PyObject *name) {
	int t = Sf_True(f, usr);
	return t > 0 ? Sf_Compare(f, Sf_Attr(f, usr, "Name"), "==", name) : t;
}

int Lv_And(scriptFrame_t *f, PyObject *a, PyObject *b) {
	int t = Sf_True(f, a);
	return t > 0 ? Sf_True(f, b) : t;
}

int Lv_Alarm(scriptFrame_t *f) {
	return Sf_True(f, Sf_Item(f, Sf_Ask(f, "Scrap", "GetAlarm", "()"), 0));
}

byte Lv_Com(scriptFrame_t *f, const char *key, const char *who, PyObject *then, PyObject *params) {
	if (!then) {
		return Sf_Run(f, "Speech", "AddRemoteCom", "(ss)", key, who);
	}
	return params ? Sf_Run(f, "Speech", "AddRemoteCom", "(ssOO)", key, who, then, params) :
		Sf_Run(f, "Speech", "AddRemoteCom", "(ssO)", key, who, then);
}

byte Lv_Next(scriptFrame_t *f, const char *lib, const char *map, const char *desc) {
	return Sf_Run(f, "MissionsFuncs", "SetNextMission", "(ss)", lib, map) && Sf_Run(f, "SaveGame", "MMDesc", "(si)", desc, 1);
}

byte Lv_ScorerCall(scriptFrame_t *f, PyObject *v) {
	return Sf_SetMod(f, "Menu", "MissionScorerCall", v);
}

byte Lv_Set0(scriptFrame_t *f, const char *item, const char *var, PyObject *v) {
	return Sf_Run(f, "SScorer", "Set", "(issO)", 0, item, var, v);
}

PyObject *Lv_TextX(scriptFrame_t *f, long d) {
	PyObject *x = Sf_Mod(f, "Scorer", "MissionScorerText_X");
	return d ? Sf_OpI(f, x, '+', d) : x;
}

PyObject *Lv_TextY(scriptFrame_t *f, long d) {
	PyObject *y = Sf_Mod(f, "Scorer", "MissionScorerText_Y");
	return d ? Sf_OpI(f, y, '+', d) : y;
}

PyObject *Lv_Label(scriptFrame_t *f, const char *key) {
	return Sf_Cat(f, "Os", Sf_Lang(f, Sf_Str(f, key)), ":");
}

byte Lv_Text(scriptFrame_t *f, PyObject *id, const char *item, PyObject *y, PyObject *text, PyObject *visible) {
	return Sf_MissionText(f, id, item, "Text", Lv_TextX(f, 0), y, "Horatio", "Shadow", text, "Left", visible);
}

PyObject *Lv_Count(scriptFrame_t *f, const char *module, const char *num, const char *max) {
	PyObject *n = Sf_Repr(f, Sf_Global(f, module, num));
	return Sf_Cat(f, "OsO", n, "/", Sf_Repr(f, Sf_Global(f, module, max)));
}

byte Lv_MsgText(scriptFrame_t *f, PyObject *cad) {
	int mark = f->n;
	PyObject *fn = Sf_Mod(f, "SScorer", "SetMsgText");
	PyObject *t = Sf_Time(f);
	Sf_Call(f, fn, "(OO)", cad, Sf_Op(f, t, '+', Sf_Ask(f, "Speech", "CalculateSpeechTime", "(O)", cad)));
	Sf_Release(f, mark);
	return !f->failed;
}

PyObject *Lv_CreateAt(scriptFrame_t *f, PyObject *name, PyObject *p, const char *type) {
	PyObject *fn = Sf_Mod(f, "Scrap", "CreateEntity");
	PyObject *x = Sf_Item(f, p, 0), *y = Sf_Item(f, p, 1), *z = Sf_Item(f, p, 2);
	return Sf_Call(f, fn, "(OOOOs)", name, x, y, z, type);
}

byte Lv_SaveAdd(scriptFrame_t *f, const char *module, const char *var, const char *item) {
	int mark = f->n;
	PyObject *lst = Lv_SaveList(f, var, module);
	if (Sf_In(f, Sf_Str(f, item), lst) == 0) {
		Lv_SaveAppend(f, var, lst, Sf_Str(f, item));
	}
	Sf_Release(f, mark);
	return !f->failed;
}

byte Lv_SaveCat(scriptFrame_t *f, const char *module, const char *var, const char *fmt, ...) {
	int mark = f->n;
	PyObject *lst = Lv_SaveList(f, var, module);
	if (!f->failed) {
		va_list va;
		va_start(va, fmt);
		PyObject *items = Py_VaBuildValue((char *)fmt, va);
		va_end(va);
		lst = Sf_Op(f, lst, '+', Sf_Keep(f, items));
	}
	Sf_Run(f, "Scrap", "SetSaveVar", "(sO)", var, Sf_Repr(f, lst));
	Sf_Release(f, mark);
	return !f->failed;
}

byte Lv_Blueprint(scriptFrame_t *f, const char *module, const char *var, const char *item, const char *msg, byte delay) {
	int mark = f->n;
	PyObject *lst = Lv_SaveList(f, var, module);
	int in = Sf_In(f, Sf_Str(f, item), lst);
	if (in == 0) {
		Lv_SaveAppend(f, var, lst, Sf_Str(f, item));
		if (msg && delay) {
			Sf_Run(f, "RaceMaker", "ShowSystemMessageDelay", "(Od)", Sf_Lang(f, Sf_Str(f, msg)), 0.25);
		} else if (msg) {
			Sf_Import(f, "RaceMaker");
			Sf_Run(f, "RaceMaker", "ShowSystemMessage", "(O)", Sf_Lang(f, Sf_Str(f, msg)));
		}
		Sf_Run(f, "Scrap", "IncSaveVar", "(s)", "NewBluePrintAvaliable");
	}
	Sf_Release(f, mark);
	return !f->failed;
}

byte Lv_PcDefault(scriptFrame_t *f, const char *module) {
	int mark = f->n;
	Sf_SetAttrS(f, Sf_Global(f, module, "pc"), "UseFunc", "PhoneCab.DefUseAudioCom");
	Sf_Release(f, mark);
	return !f->failed;
}

byte Lv_Drop(scriptFrame_t *f, const char *module, PyObject *pos, const char *name, const char *model, const char *radius,
	int onlyUser, const char *onTake, const char *sound, const char *arrow) {
	int mark = f->n;
	PyObject *x = Sf_Item(f, pos, 0), *y = Sf_Item(f, pos, 1), *z = Sf_Item(f, pos, 2);
	PyObject *l = Sf_Ask(f, "Scrap", "CreateEntity", "(sOOOs)", name, x, y, z, "Item");
	Sf_Method(f, l, "SetModel", "(O)", Sf_Global(f, module, model));
	Sf_SetAttr(f, l, "Radius", radius ? Sf_Global(f, module, radius) : Sf_Int(f, 3500));
	Sf_SetAttrI(f, l, "AppearsIfFree", 0);
	Sf_SetAttrI(f, l, "AutoFace", 0);
	Sf_SetAttrI(f, l, "OnlyUser", onlyUser);
	Sf_SetAttr(f, l, "rTime", Sf_Float(f, 0.25));
	Sf_SetAttrS(f, l, "OnTake", onTake);
	Sf_Import(f, "Items");
	Sf_Run(f, "Items", "SetSound", "(Os)", l, sound);
	PyObject *an = arrow ? Sf_Str(f, arrow) : Sf_Cat(f, "Os", Sf_Attr(f, l, "Name"), "CheckArrow");
	x = Sf_Item(f, pos, 0), y = Sf_Item(f, pos, 1), z = Sf_Item(f, pos, 2);
	PyObject *l2 = Sf_Ask(f, "Scrap", "CreateEntity", "(OOOOs)", an, x, y, z, "WithLife");
	Sf_SetAttr(f, l2, "CamTgt", Sf_Build(f, "(iii)", 0, 1000, 0));
	Sf_Run(f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(f, l2, "Name"));
	Sf_Release(f, mark);
	return !f->failed;
}

byte Lv_Taken(scriptFrame_t *f, PyObject *item, PyObject *arrow) {
	int mark = f->n;
	PyObject *itm = Sf_Entity(f, item);
	Sf_SetAttrI(f, itm, "rTime", 20);
	Sf_Later(f, Sf_Int(f, 5), Sf_Attr(f, itm, "Del"), Sf_Build(f, "()"), NULL);
	Sf_Method(f, Sf_Entity(f, arrow), "Del", "()");
	Sf_Release(f, mark);
	return !f->failed;
}

PyObject *Lv_KeyText(scriptFrame_t *f, const char *key, const char *action) {
	PyObject *t = Sf_Lang(f, Sf_Str(f, key));
	return Sf_Op(f, t, '%', Sf_Ask(f, "SInput", "GetDefinedEntry", "(isss)", 0, "Walk", action, "Mouse"));
}

void Lv_BossTraffic(scriptFrame_t *f, byte comSats) {
	int mark = f->n;
	int done = 0;
	if (comSats) {
		PyObject *upper = Sf_Mod(f, "Init", "UpperMap");
		done = Sf_In(f, upper, Sf_Eval(f, Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", "ComSatsMissionsMapsFinished"), "Map"));
	}
	if (done > 0) {
		Sf_Run(f, "Scrap", "Set", "(si)", "MaxChasingShips", 0);
		Sf_Run(f, "Scrap", "Set", "(si)", "iTrafficNumber", 0);
		Sf_SetMod(f, "OutPolice", "PoliceActive", Sf_Int(f, 0));
	} else if (done == 0) {
		Sf_Run(f, "Scrap", "Set", "(si)", "iTrafficNumber", 5);
		Sf_Run(f, "Scrap", "Set", "(si)", "MaxChasingShips", 5);
	}
	Sf_SetMod(f, "Traffic", "AcNormalShips", Sf_Build(f, "[]"));
	Sf_SetMod(f, "Traffic", "NumNormal", Sf_Int(f, 0));
	Sf_SetMod(f, "Traffic", "AcMediumShips", Sf_Build(f, "[]"));
	Sf_SetMod(f, "Traffic", "NumMedium", Sf_Int(f, 0));
	Sf_SetMod(f, "Traffic", "AcHeavyShips", Sf_Build(f, "[]"));
	Sf_SetMod(f, "Traffic", "NumHeavy", Sf_Int(f, 0));
	Sf_Release(f, mark);
}

/* ---- Mission_DestroyCommSats1, 2 and 3 (Outskirts, Commercial, DownTown): one script with the level's details ---- */

/* what differs between the three copies of the script */
typedef struct {
	const char *module;
	long maxChasing;		/* MaxChasingShips in DestroyEnemyShips */
	const char *map;		/* SetNextMission (NULL: data): the level added to ComSatsMissionsMapsFinished, */
	const char *const *saves;	/* the save variables set then (pairs, NULL: none) */
	const char *nextLib, *nextMap, *nextDesc;	/* and the next mission */
} lvProbes_t;

/* the mercenaries leave D-Tritus after DownTown */
static const char *const lv_probes_mercs[] = { "MercsHelpDtritus", "0", "MercFriends.MercenaryA_Smartie", "0", "MercFriends.MercenaryC_Brutus",
	"0", "MercFriends.MercenaryB_Dumber", "0", NULL };

static const lvProbes_t lv_probes[] = {
	{ "Mission_DestroyCommSats1", 0, NULL, NULL, NULL, NULL, NULL },
	{ "Mission_DestroyCommSats2", -1, "COMMERCIAL", NULL, "Mission_DestroyCommSats3", "DownTown", "Mission_DestroyCommSats3_MissionText1" },
	{ "Mission_DestroyCommSats3", 0, "DOWNTOWN", lv_probes_mercs, "Mission_GoWithHuman", "Press", "Mission_GoWithHuman_MissionText1" },
};

/* InitStatus(LevelPath): the probes are the heavy traffic, none of it at the start */
static PyObject *Probes_InitStatus(PyObject *args, const lvProbes_t *p) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)p;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "Traffic");
	Sf_SetMod(&f, "Traffic", "AcHeavyShips", Sf_Build(&f, "[s]", "TProbe"));
	Sf_SetMod(&f, "Traffic", "NumHeavy", Sf_Int(&f, 0));
	return Sf_None(&f);
}

static PyObject *Probes_Init(PyObject *args, const lvProbes_t *p) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Own(&f, p->module, "CreateProbes", "()");
	Lv_ScorerCall(&f, Sf_Global(&f, p->module, "MissionScorer"));
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* IsValidPoint(usrPos, pos, posList): a new point far enough from the player and from the other probes */
static PyObject *Probes_IsValidPoint(PyObject *args, const lvProbes_t *p) {
	PyObject *usrPos, *pos, *list;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOO", &usrPos, &pos, &list)) {
		return NULL;
	}
	int in = Sf_In(&f, pos, list);
	if (in != 0) {
		return Sf_Return(&f, in > 0 ? Sf_Int(&f, 0) : NULL);
	}
	PyObject *d = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", pos, usrPos));
	int near = Sf_Compare(&f, d, "<", Sf_Global(&f, p->module, "minPlayerDist"));
	if (near != 0) {
		return Sf_Return(&f, near > 0 ? Sf_Int(&f, 0) : NULL);
	}
	PyObject *q;
	for (int k = 0; (q = Sf_Next(&f, list, k)) != NULL; k++) {
		d = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", q, pos));
		if (Sf_Compare(&f, d, "<", Sf_Global(&f, p->module, "minDistBetweenProbes")) > 0) {
			return Sf_Return(&f, Sf_Int(&f, 0));
		}
	}
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* CreateProbes(): the probes left at random visible points */
static PyObject *Probes_CreateProbes(PyObject *args, const lvProbes_t *p) {
	char onDeath[64];
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Str_Printf(onDeath, sizeof(onDeath), "%s.OnProbeDeath", p->module);
	Sf_Import(&f, "OutMap");
	Sf_Import(&f, "VehiclesNPC");
	Sf_Import(&f, "Traffic");
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	if (Sf_True(&f, usr) != 1) {
		return Sf_None(&f);
	}
	PyObject *list = Sf_Build(&f, "[]");
	PyObject *usrPos = Sf_Attr(&f, usr, "Pos");
	PyObject *num = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "NumProbes"));
	long i = 0;
	for (;;) {
		int mark = f.n;
		if (Sf_Compare(&f, Sf_Int(&f, i), "<", num) <= 0) {
			break;
		}
		PyObject *pos = Sf_Ask(&f, "SAI", "GetRandomVisibilityPoint", "()");
		int ok = Sf_True(&f, Sf_Own(&f, p->module, "IsValidPoint", "(OOO)", usrPos, pos, list));
		if (ok == 0) {
			Sf_Release(&f, mark);
			continue;
		}
		Sf_Run(&f, "Traffic", "SetPath", "()");
		PyObject *e = Sf_Ask(&f, "VehiclesNPC", "MakeAI", "(ssii)", "TProbe", "ProbeMos", 0, 0);
		Sf_Run(&f, "Traffic", "ResetPath", "()");
		Sf_SetAttrS(&f, e, "OnDeath", onDeath);
		Sf_SetAttr(&f, e, "MovePos", pos);
		Sf_SetAttr(&f, e, "Pos", pos);
		PyObject *ang = Sf_Build(&f, "(iii)", 0, 0, 0);
		Sf_SetAttr(&f, e, "MoveAng", ang);
		Sf_SetAttr(&f, e, "Ang", ang);
		Sf_SetAttr(&f, e, "MoveTime", Sf_Float(&f, 0.1));
		PyObject *life = Sf_Global(&f, p->module, "PROBE_LIFE");
		Sf_SetAttr(&f, e, "Life", life);
		Sf_SetAttr(&f, e, "MaxLife", life);
		Sf_SetAttr(&f, e, "MinLife", life);
		Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", e, 1);
		Sf_Method(&f, list, "append", "(O)", pos);
		Sf_Release(&f, mark);
		i++;
	}
	return Sf_None(&f);
}

/* OnProbeDeath(LooserName, WinnerName): one probe fewer; the last one ends the mission */
static PyObject *Probes_OnProbeDeath(PyObject *args, const lvProbes_t *p) {
	PyObject *looser, *winner;
	char msg[64];
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Run(&f, "Vehicles", "OnDeath", "(OO)", looser, winner);
	PyObject *n = Sf_OpI(&f, Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "NumProbes")), '-', 1);
	int last = Sf_EqI(&f, n, 0);
	if (last > 0) {
		Sf_Own(&f, p->module, "DestroyEnemyShips", "()");
		Lv_ScorerCall(&f, Py_None);
		PyObject *t = Sf_OpI(&f, Sf_Time(&f), '+', 2);
		PyObject *fn = Sf_Mod(&f, "Speech", "AddRemoteCom");
		Str_Printf(msg, sizeof(msg), "%s_RemoteMessage1", p->module);
		Sf_Schedule(&f, t, fn, Sf_Build(&f, "(ssOO)", msg, "CrazyGambler", Sf_Global(&f, p->module, "SetNextMission"), Sf_Build(&f, "()")),
			NULL);
	} else if (last == 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "NumProbes", Sf_Repr(&f, n));
		Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	}
	Sf_DrawScorer(&f);
	return Sf_None(&f);
}

/* DestroyShipsInList(listName): every ship of the list explodes */
static PyObject *Probes_DestroyShipsInList(PyObject *args, const lvProbes_t *p) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)p;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
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

/* DestroyEnemyShips(): no more police (MaxChasingShips of the level) nor traffic, the alarm falls, the ships of the
 * lists explode one after another */
static PyObject *Probes_DestroyEnemyShips(PyObject *args, const lvProbes_t *p) {
	static const char *const lists[] = { "EnemiesTraffic", "ChasingTraffic", "Traffic", "OutCop" };
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Run(&f, "Scrap", "Set", "(si)", "MaxChasingShips", (int)p->maxChasing);
	Sf_Run(&f, "Scrap", "Set", "(si)", "iTrafficNumber", 0);
	Sf_Run(&f, "Scrap", "Set", "(ss)", "PoliceWaveEliminated", "");
	Sf_Run(&f, "Scrap", "Set", "(sd)", "AlarmFallDelta", -0.05);
	Sf_Run(&f, "Scrap", "SetAlarm", "(i)", 0);
	for (int i = 0; i < 4; i++) {
		Sf_Later(&f, Sf_Float(&f, 0.5 * i), Sf_Global(&f, p->module, "DestroyShipsInList"), Sf_Build(&f, "(s)", lists[i]), NULL);
	}
	return Sf_None(&f);
}

/* SetNextMission() of Commercial and DownTown: the level counts as freed (the save variables of the level), on to the
 * next mission (Outskirts' is data) */
static PyObject *Probes_SetNextMission(PyObject *args, const lvProbes_t *p) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "NumProbes");
	PyObject *l = Lv_SaveList(&f, "ComSatsMissionsMapsFinished", p->module);
	Sf_Method(&f, l, "append", "(s)", p->map);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "ComSatsMissionsMapsFinished", Sf_Repr(&f, l));
	if (p->saves) {
		Lv_Saves(&f, p->saves);
	}
	Lv_Next(&f, p->nextLib, p->nextMap, p->nextDesc);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	return Sf_None(&f);
}

/* MissionScorer(id): the probes left */
static PyObject *Probes_MissionScorer(PyObject *args, const lvProbes_t *p) {
	PyObject *id;
	char key[64];
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Str_Printf(key, sizeof(key), "%s_Scorer1", p->module);
	Sf_Import(&f, "Scorer");
	Sf_MissionText(&f, id, "NumProbesText", "Text", Lv_TextX(&f, 0), Lv_TextY(&f, 0), "Horatio", "Shadow", Lv_Label(&f, key), "Left", NULL);
	Sf_MissionText(&f, id, "NumProbes", "Text", Lv_TextX(&f, 0), Lv_TextY(&f, 20), "Horatio", "Shadow",
		Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "NumProbes"), "Left", NULL);
	return Sf_None(&f);
}

#define PROBES_FN(n, fn) \
	static PyObject *Probes##n##_##fn(PyObject *self, PyObject *args) { \
		(void)self; \
		return Probes_##fn(args, &lv_probes[n]); \
	}
#define PROBES(n) \
	PROBES_FN(n, InitStatus) PROBES_FN(n, Init) PROBES_FN(n, IsValidPoint) PROBES_FN(n, CreateProbes) PROBES_FN(n, OnProbeDeath) \
	PROBES_FN(n, DestroyShipsInList) PROBES_FN(n, DestroyEnemyShips) PROBES_FN(n, MissionScorer)
#define PROBES_DEF(n, fn) { #fn, Probes##n##_##fn, 1, NULL },
#define PROBES_DEFS(n) \
	PROBES_DEF(n, InitStatus) PROBES_DEF(n, Init) PROBES_DEF(n, IsValidPoint) PROBES_DEF(n, CreateProbes) PROBES_DEF(n, OnProbeDeath) \
	PROBES_DEF(n, DestroyShipsInList) PROBES_DEF(n, DestroyEnemyShips) PROBES_DEF(n, MissionScorer)

PROBES(0)
PROBES(1)
PROBES(2)
PROBES_FN(1, SetNextMission)
PROBES_FN(2, SetNextMission)

PyMethodDef g_OutskirtsDestroyCommSats1Methods[] = {
	PROBES_DEFS(0)
	{ NULL, NULL, 0, NULL },
};

PyMethodDef g_CommercialDestroyCommSats2Methods[] = {
	PROBES_DEFS(1)
	PROBES_DEF(1, SetNextMission)
	{ NULL, NULL, 0, NULL },
};

PyMethodDef g_DownTownDestroyCommSats3Methods[] = {
	PROBES_DEFS(2)
	PROBES_DEF(2, SetNextMission)
	{ NULL, NULL, 0, NULL },
};

/* ---- native lines of the level scripts (Levels.h; docs/objlib-plan.md, stage 2) ---- */

void LvN_Int(nativeVal_t *v, long i) {
	Native_SetInt(v, i);
}

void LvN_Str(nativeVal_t *v, const char *s) {
	Native_SetText(v, s, (int)strlen(s));
}

byte LvN_ModGet(const char *module, const char *var, nativeVal_t *out) {
	PyObject *o = Script_Attr(module, var);
	Native_SetNone(out);
	if (!o) {
		return 0;
	}
	Native_Arg(o, out);
	Py_DECREF(o);
	return 1;
}

byte LvN_ModSetGlobal(const char *module, const char *var, const char *src, const char *name) {
	PyObject *o = Mode_GlobalPy(src, name);
	byte ok = o && Mode_SetModPy(module, var, o);
	Py_XDECREF(o);
	return ok;
}

byte LvN_ModNone(const char *module, const char *var) {
	return Mode_SetModPy(module, var, Py_None);
}

byte LvN_ModAppend(const char *module, const char *var, const nativeVal_t *v) {
	PyObject *o = Script_Attr(module, var);
	PyObject *x = o ? Native_ToPy(v) : NULL;
	PyObject *r = x ? Script_Method(o, "append", "(O)", x) : NULL;
	Py_XDECREF(r);
	Py_XDECREF(x);
	Py_XDECREF(o);
	return r != NULL;
}

byte LvN_GObj(const char *module, const char *name, nativeVal_t *out) {
	PyObject *o = Mode_GlobalPy(module, name);
	Native_SetNone(out);
	if (!o) {
		return 0;
	}
	Native_Arg(o, out);
	Py_DECREF(o);
	return 1;
}

byte LvN_GAdd(const char *module, const char *name, long add, nativeVal_t *out) {
	nativeVal_t v, n, r;
	Native_SetInt(&n, add);
	Native_SetNone(&r);
	byte ok = Mode_G(module, name, &v) && Native_Arith('+', &v, &n, &r) && Mode_Put(module, name, NULL, &r);
	Native_Free(&v);
	if (ok && out) {
		*out = r;
	} else {
		Native_Free(&r);
	}
	return ok;
}

byte LvN_PutI(const char *module, const char *name, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_Put(module, name, NULL, &v);
}

byte LvN_PutS(const char *module, const char *name, const char *s) {
	nativeVal_t v;
	LvN_Str(&v, s);
	byte ok = Mode_Put(module, name, NULL, &v);
	Native_Free(&v);
	return ok;
}

int LvN_GTrue(const char *module, const char *name) {
	nativeVal_t v;
	int r = Mode_G(module, name, &v) ? Native_True(&v) : -1;
	Native_Free(&v);
	return r;
}

int LvN_GEqI(const char *module, const char *name, long i) {
	nativeVal_t v;
	int r = Mode_G(module, name, &v) ? LvN_EqI(&v, i) : -1;
	Native_Free(&v);
	return r;
}

byte LvN_EqI(const nativeVal_t *a, long i) {
	nativeVal_t b;
	Native_SetInt(&b, i);
	return !Native_Compare(a, &b);
}

byte LvN_EqS(const nativeVal_t *a, const char *s) {
	nativeVal_t b;
	LvN_Str(&b, s);
	byte eq = !Native_Compare(a, &b);
	Native_Free(&b);
	return eq;
}

byte LvN_SetI(const nativeVal_t *h, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_VSet(h, var, &v);
}

byte LvN_SetS(const nativeVal_t *h, const char *var, const char *s) {
	nativeVal_t v;
	LvN_Str(&v, s);
	byte ok = Mode_VSet(h, var, &v);
	Native_Free(&v);
	return ok;
}

byte LvN_SetF(const nativeVal_t *h, const char *var, double d) {
	nativeVal_t v;
	Native_SetFloat(&v, d);
	return Mode_VSet(h, var, &v);
}

byte LvN_Item(const nativeVal_t *x, long i, nativeVal_t *out) {
	nativeVal_t k;
	Native_SetInt(&k, i);
	Native_SetNone(out);
	return Native_Subscript(x, &k, out);
}

byte LvN_RaiseText(const char *text) {
	/* OBJLIB 7 */
	PyObject *s = PyString_FromString((char *)text);
	if (s) {
		PyErr_SetObject(s, Py_None);
		Py_DECREF(s);
	}
	return 0;
}

void LvN_Dummy(const char *pattern, int flag, nativeVal_t *dummy) {
	Mode_DummyStart(pattern, flag);
	Mode_DummyNext(dummy);
}

byte LvN_DummyPosAng(const nativeVal_t *dummy, nativeVal_t *pos, nativeVal_t *ang) {
	nativeVal_t p, a;
	Native_SetNone(&p);
	Native_SetNone(&a);
	Native_SetEmpty(pos, NV_TUPLE);
	Native_SetEmpty(ang, NV_TUPLE);
	byte ok = LvN_Item(dummy, 1, &p);
	for (int i = 0; i < 3 && ok; i++) {
		ok = LvN_Item(&p, i, Native_Append(pos));
	}
	ok = ok && LvN_Item(dummy, 2, &a);
	for (int i = 0; i < 2 && ok; i++) {
		ok = LvN_Item(&a, i, Native_Append(ang));
	}
	if (ok) {
		Native_SetInt(Native_Append(ang), 0);
	}
	Native_Free(&p);
	Native_Free(&a);
	return ok;
}

byte LvN_SaveList(const char *var, const char *module, nativeVal_t *out) {
	nativeVal_t s;
	Mode_SaveVar(var, &s);
	Native_SetNone(out);
	byte ok = Native_EvalText(module, &s, out);
	Native_Free(&s);
	return ok;
}

void LvN_SaveRepr(const char *var, const nativeVal_t *v) {
	nativeBuf_t b;
	Native_BufInit(&b);
	Native_Repr(&b, v, NR_REPR);
	SaveVar_Set(var, b.p);
	Native_BufFree(&b);
}

byte LvN_SaveAppend(const char *var, nativeVal_t *lst, const nativeVal_t *item) {
	if (lst->type != NV_LIST) {
		/* the append method of another value: its error, as the script meets it */
		PyObject *o = Native_ToPy(lst), *x = o ? Native_ToPy(item) : NULL;
		PyObject *r = x ? Script_Method(o, "append", "(O)", x) : NULL;
		Py_XDECREF(r);
		Py_XDECREF(x);
		Py_XDECREF(o);
		if (!r) {
			return 0;
		}
		return Native_Raise(NE_SYSTEM, "append of a value that is no list");
	}
	Native_Copy(Native_Append(lst), item);
	LvN_SaveRepr(var, lst);
	return 1;
}

byte LvN_ListRemove(nativeVal_t *lst, const nativeVal_t *x) {
	if (lst->type != NV_LIST) {
		PyObject *o = Native_ToPy(lst), *v = o ? Native_ToPy(x) : NULL;
		PyObject *r = v ? Script_Method(o, "remove", "(O)", v) : NULL;
		Py_XDECREF(r);
		Py_XDECREF(v);
		Py_XDECREF(o);
		return r ? Native_Raise(NE_SYSTEM, "remove of a value that is no list") : 0;
	}
	for (int i = 0; i < lst->n; i++) {
		if (!Native_Compare(&lst->items[i], x)) {
			Native_Free(&lst->items[i]);
			memmove(&lst->items[i], &lst->items[i + 1], (size_t)(lst->n - i - 1) * sizeof(*lst->items));
			lst->n--;
			return 1;
		}
	}
	return Native_Raise(NE_VALUE, "list.remove(x): x not in list");
}

byte LvN_SaveIs(const char *name, const char *text) {
	const char *s = SaveVar_Get(name);
	return s && !strcmp(s, text);
}

const nativeVal_t *LvN_NoArgs(void) {
	static nativeVal_t empty = { .type = NV_TUPLE };
	return &empty;
}

byte LvN_Com(const char *key, const char *who, const char *module, const char *func, const nativeVal_t *params) {
	PyObject *add = Script_Attr("Speech", "AddRemoteCom");
	if (!add) {
		return 0;
	}
	if (!func) {
		return Native_HookCall(add, NULL, "ss", key, who);
	}
	PyObject *then = Mode_GlobalPy(module, func);
	if (!then) {
		Py_DECREF(add);
		return 0;
	}
	byte ok = params ? Native_HookCall(add, NULL, "ssOv", key, who, then, params) :
		Native_HookCall(add, NULL, "ssO", key, who, then);
	Py_DECREF(then);
	return ok;
}

byte LvN_Speakers(const nativeVal_t *usr, const nativeVal_t *ch) {
	return Mode_SetModVar("Speech", "PlayerName", usr) && Mode_SetModVar("Speech", "NPCName", ch);
}

byte LvN_OnEnd(const char *module, const char *func, const nativeVal_t *params) {
	return LvN_ModSetGlobal("Speech", "OnEndOfSpeech", module, func) &&
		Mode_SetModVar("Speech", "OnEndOfSpeechParams", params ? params : LvN_NoArgs());
}

byte LvN_Stay(const nativeVal_t *e, long state, const nativeVal_t *target) {
	return LvN_SetI(e, "LLogicDefaultState", state) && Mode_VSet(e, "LLogicDefaultTarget", target) &&
		LvN_SetI(e, "LLogicState", state) && Mode_VSet(e, "LLogicTarget", target) && LvN_SetI(e, "LLogicIdle", 1) &&
		LvN_SetI(e, "LLogicIdle", 0);
}

byte LvN_Callback(const char *name, const char *module, const char *func) {
	/* OBJLIB 5: CharRes holds the character objects of CharsNPC */
	PyObject *res = Script_Attr("CharsNPC", "CharRes");
	PyObject *key = res ? PyString_FromString((char *)name) : NULL;
	PyObject *ch = key ? PyObject_GetItem(res, key) : NULL;
	PyObject *cb = ch ? PyObject_GetAttrString(ch, "CallbackFunc") : NULL;
	PyObject *append = cb ? PyObject_GetAttrString(cb, "append") : NULL;
	PyObject *fn = append ? Mode_GlobalPy(module, func) : NULL;
	PyObject *a = fn ? Py_BuildValue("(O)", fn) : NULL;
	PyObject *r = a ? Script_CallObject(append, a, NULL) : NULL;
	Py_XDECREF(r);
	Py_XDECREF(a);
	Py_XDECREF(fn);
	Py_XDECREF(append);
	Py_XDECREF(cb);
	Py_XDECREF(ch);
	Py_XDECREF(key);
	Py_XDECREF(res);
	return r != NULL;
}

byte LvN_MissionChar(const char *name, const char *module, const char *func) {
	return LV_CALL("CharsNPC", "CreateMissionChar", NULL, "s", name) && LvN_Callback(name, module, func);
}

int LvN_PcOr(const char *module, const char *name) {
	nativeVal_t x;
	if (!Native_GVarGet("IsXbox", &x)) {
		return -1;
	}
	int r = Native_True(&x) ? LvN_GTrue(module, name) : 1;
	Native_Free(&x);
	return r;
}

byte LvN_ScorerSet(const nativeVal_t *id, const char *item, const char *var, const nativeVal_t *v) {
	nativeVal_t it;
	LvN_Str(&it, item);
	byte ok = Mode_ScorerSet(id, &it, var, v);
	Native_Free(&it);
	return ok;
}

byte LvN_ScorerTextBegin(const nativeVal_t *id, const char *item, long dy) {
	nativeVal_t it, x, y, d, v;
	LvN_Str(&it, item);
	Native_SetNone(&x);
	Native_SetNone(&y);
	Native_SetNone(&v);
	Native_SetInt(&d, dy);
	byte ok = Mode_ScorerAdd(id, &it, "Text") && LvN_ModGet("Scorer", "MissionScorerText_X", &x) &&
		LvN_ScorerSet(id, item, "X", &x) && LvN_ModGet("Scorer", "MissionScorerText_Y", &y) &&
		(!dy || Native_Arith('+', &y, &d, &v)) && LvN_ScorerSet(id, item, "Y", dy ? &v : &y);
	if (ok) {
		LvN_Str(&v, "Horatio");
		ok = LvN_ScorerSet(id, item, "Font", &v);
		Native_Free(&v);
	}
	if (ok) {
		LvN_Str(&v, "Shadow");
		ok = LvN_ScorerSet(id, item, "Effect", &v);
	}
	Native_Free(&it);
	Native_Free(&x);
	Native_Free(&y);
	Native_Free(&v);
	return ok;
}

byte LvN_ScorerTextEnd(const nativeVal_t *id, const char *item) {
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	nativeVal_t v, c, k;
	LvN_Str(&v, "Left");
	byte ok = LvN_ScorerSet(id, item, "Align", &v);
	Native_Free(&v);
	for (int i = 0; i < 4 && ok; i++) {
		Native_SetNone(&k);
		ok = LvN_ModGet("Scorer", "MissionScorerText_Color", &c) && LvN_Item(&c, i, &k) &&
			LvN_ScorerSet(id, item, rgba[i], &k);
		Native_Free(&c);
		Native_Free(&k);
	}
	return ok;
}

byte LvN_Life(const nativeVal_t *e, const char *module, const char *life) {
	nativeVal_t v;
	byte ok = Mode_G(module, life, &v) && Mode_VSet(e, "Life", &v) && Mode_VSet(e, "MaxLife", &v);
	Native_Free(&v);
	return ok;
}

byte LvN_Value(const char *text, int len, nativeVal_t *out) {
	char buf[0x100], *end;
	Native_SetNone(out);
	if (len < 0) {
		len = (int)strlen(text);
	}
	if (len <= 0 || len >= (int)sizeof(buf)) {
		return Native_Raise(NE_SYSTEM, "bad value in a call table");
	}
	memcpy(buf, text, len);
	buf[len] = 0;
	if (buf[0] == '\'') {
		LvN_Str(out, buf + 1);
		return 1;
	}
	if (buf[0] == '%' || buf[0] == '^') {
		char *dot = strchr(buf, '.'), *colon = buf[0] == '^' && dot ? strchr(dot, ':') : NULL;
		if (!dot || (buf[0] == '^' && !colon)) {
			return Native_Raise(NE_SYSTEM, "bad value in a call table");
		}
		*dot = 0;
		if (colon) {
			*colon = 0;
			return LV_CALL(buf + 1, dot + 1, out, "s", colon + 1);
		}
		return LvN_ModGet(buf + 1, dot + 1, out);
	}
	if (buf[0] == '-' || (buf[0] >= '0' && buf[0] <= '9')) {
		if (!strchr(buf, '.')) {
			long i = strtol(buf, &end, 10);
			if (*end == 0) {
				Native_SetInt(out, i);
				return 1;
			}
		} else {
			double d = strtod(buf, &end);
			if (*end == 0) {
				Native_SetFloat(out, d);
				return 1;
			}
		}
	}
	LvN_Str(out, buf);
	return 1;
}

byte LvN_Profile(const char *const *vars, nativeVal_t *out) {
	if (!Mode_Import("VehiclesNPC") || !LV_CALL("VehiclesNPC", "cShipAI", out, "")) {
		return 0;
	}
	for (; *vars; vars++) {
		const char *eq = strchr(*vars, '=');
		char name[0x40];
		int len = eq ? (int)(eq - *vars) : 0;
		if (!eq || len >= (int)sizeof(name)) {
			return Native_Raise(NE_SYSTEM, "bad attribute in a table");
		}
		memcpy(name, *vars, len);
		name[len] = 0;
		nativeVal_t v;
		byte ok = LvN_Value(eq + 1, -1, &v) && Mode_VSet(out, name, &v);
		Native_Free(&v);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

byte LvN_StdAI(const nativeVal_t *e, const nativeVal_t *pd) {
	nativeVal_t name, ship;
	Native_SetNone(&name);
	Native_SetNone(&ship);
	byte ok = Mode_VGet(e, "Name", &name) && Mode_VGet(pd, "Ship", &ship) &&
		LV_CALL("VehiclesNPC", "SetStdProperties", NULL, "vv", &name, &ship) &&
		LV_CALL("VehiclesNPC", "ModAIProff", NULL, "vv", e, pd);
	Native_Free(&name);
	Native_Free(&ship);
	return ok;
}

byte LvN_StdProfile(const char *module, const char *profile, const nativeVal_t *e, nativeVal_t *pd) {
	return LV_OWN(module, profile, pd, "") && LvN_StdAI(e, pd);
}

int LvN_UsrNameIs(const nativeVal_t *usr, const nativeVal_t *name) {
	nativeVal_t n;
	if (!Native_True(usr)) {
		return 0;
	}
	Native_SetNone(&n);
	int r = Mode_VGet(usr, "Name", &n) ? !Native_Compare(&n, name) : -1;
	Native_Free(&n);
	return r;
}

byte LvN_Draw(void) {
	nativeVal_t set;
	PyObject *fn = Script_Attr("Menu", "DrawScorer");
	if (!fn) {
		return 0;
	}
	Native_SetStr(&set, TABLE_AT(g_ActionSets, Input_GetActionSet())->name);
	return Native_HookCall(fn, NULL, "iv", 0L, &set);
}

byte LvN_ScorerOff(void) {
	return LvN_ModNone("Menu", "MissionScorerCall");
}

byte LvN_ScorerCallG(const char *module, const char *func) {
	return LvN_ModSetGlobal("Menu", "MissionScorerCall", module, func);
}

byte LvN_Next(const char *lib, const char *map, const char *desc) {
	return LV_CALL("MissionsFuncs", "SetNextMission", NULL, "ss", lib, map) && LV_CALL("SaveGame", "MMDesc", NULL, "si", desc, 1L);
}

byte LvN_Entity(const char *name, nativeVal_t *out) {
	nativeVal_t n;
	LvN_Str(&n, name);
	byte ok = Mode_EntityH(&n, out);
	Native_Free(&n);
	return ok;
}

byte LvN_LaterObj(double delay, const nativeVal_t *func, const nativeVal_t *args, int n) {
	nativeVal_t t;
	PyObject *fn = Native_ToPy(func);
	if (!fn) {
		return 0;
	}
	Native_SetFloat(&t, Mode_Time() + delay);
	byte ok = Mode_Schedule(&t, fn, args, n, NULL);
	Py_DECREF(fn);
	return ok;
}

byte LvN_Create(const nativeVal_t *name, const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *z, const char *cls,
	nativeVal_t *out) {
	PyObject *hook;
	Native_SetNone(out);
	int seam = Native_Seam("Scrap", "CreateEntity", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(hook, out, "vvvvs", name, x, y, z, cls);
	}
	cEntity *e;
	return Mode_Create(name, x, y, z, cls, &e) && Mode_EntHandle(e, out);
}

byte LvN_CreateAt(const nativeVal_t *name, const nativeVal_t *pos, const char *cls, nativeVal_t *out) {
	nativeVal_t c[3];
	int n = 0;
	byte ok = 1;
	Native_SetNone(out);
	for (; ok && n < 3; n++) {
		ok = LvN_Item(pos, n, &c[n]);
	}
	if (!ok) {
		Native_Free(&c[--n]);
	}
	ok = ok && LvN_Create(name, &c[0], &c[1], &c[2], cls, out);
	while (n-- > 0) {
		Native_Free(&c[n]);
	}
	return ok;
}

void LvN_Label(const char *key, nativeVal_t *out) {
	nativeVal_t t, colon;
	Mode_Lang(key, &t);
	LvN_Str(&colon, ":");
	Native_SetNone(out);
	Native_Arith('+', &t, &colon, out);
	Native_Free(&t);
	Native_Free(&colon);
}
