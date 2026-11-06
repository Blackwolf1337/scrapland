/*
 * Lines the level scripts repeat in their maps and missions (src/game/Script/levels/Levels.h): the discardable
 * characters of the Xbox, the callbacks of the mission characters, the speakers of a talk, a character that stays
 * where it is, the text of a mission scorer and the blueprint lists of the save variables.
 */
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
