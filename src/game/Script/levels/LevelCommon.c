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

void Lv_AcShips(scriptFrame_t *f, const char *module, const char *ships) {
	int mark = f->n;
	Sf_Import(f, "Traffic");
	Sf_Ask(f, "Scrap", "SetSaveVar", "(ss)", "Traffic.AcShips", ships);
	Sf_SetMod(f, "Traffic", "AcShips", Lv_SaveList(f, "Traffic.AcShips", module));
	Sf_Release(f, mark);
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

PyObject *Lv_Profile(scriptFrame_t *f, const char *module, const char *const *vars) {
	Sf_Import(f, "VehiclesNPC");
	PyObject *sp = Sf_Ask(f, "VehiclesNPC", "cShipAI", "()");
	Sf_SetAttrs(f, sp, module, vars);
	return sp;
}

PyObject *Lv_ProfileFunc(PyObject *args, const char *module, const char *const *vars) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Sf_Return(&f, Lv_Profile(&f, module, vars));
}

PyObject *Lv_SpawnCar(scriptFrame_t *f, PyObject *name) {
	PyObject *s = Sf_Ask(f, "OutMap", "GetFreeSpawnPos", "()");
	PyObject *fn = Sf_Mod(f, "Scrap", "CreateEntity");
	PyObject *p = Sf_Item(f, s, 0);
	PyObject *x = Sf_Item(f, p, 0), *y = Sf_Item(f, p, 1), *z = Sf_Item(f, p, 2);
	return Sf_Call(f, fn, "(OOOOs)", name, x, y, z, "Car");
}

PyObject *Lv_SetProfile(scriptFrame_t *f, const char *module, const char *profile, PyObject *e) {
	PyObject *pd = Sf_Own(f, module, profile, "()");
	Sf_Ask(f, "VehiclesNPC", "SetStdProperties", "(OO)", Sf_Attr(f, e, "Name"), Sf_Attr(f, pd, "Ship"));
	Sf_Ask(f, "VehiclesNPC", "ModAIProff", "(OO)", e, pd);
	return pd;
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

void Lv_BarDefaults(scriptFrame_t *f, PyObject *id, const char *item) {
	int mark = f->n;
	PyObject *fn = Sf_Mod(f, "Scorer", "MissionScorerBar_SetDefaults");
	PyObject *back = Sf_Mod(f, "Scorer", "MissionScorerBar_ColorBack");
	PyObject *main = Sf_Mod(f, "Scorer", "MissionScorerBar_ColorMain");
	PyObject *black = Sf_Mod(f, "Scorer", "MissionScorerBar_ColorBlack");
	Sf_Call(f, fn, "(OsOOO)", id, item, back, main, black);
	Sf_Release(f, mark);
}
