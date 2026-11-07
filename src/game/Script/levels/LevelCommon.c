/*
 * Lines the level scripts repeat in their maps and missions (src/game/Script/levels/Levels.h): the discardable
 * characters of the Xbox, the callbacks of the mission characters, the speakers of a talk, a character that stays
 * where it is, the text of a mission scorer, the blueprint lists of the save variables, the remote coms, the phone
 * cabs and the mission items; and Mission_DestroyCommSats1, 2 and 3, one script in three levels.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/input.h"
#include "scrap/sound.h"
#include "scrap/scrap.h"

#include "Levels.h"

/* the frame helpers whose lines have a native twin below run the twin (the frame fails with its error) */

int Lv_PcOr(scriptFrame_t *f, const char *module, const char *name) {
	if (f->failed) {
		return -1;
	}
	int r = LvN_PcOr(module, name);
	Sf_Check(f, r >= 0);
	return r;
}

void Lv_Callback(scriptFrame_t *f, const char *name, const char *module, const char *func) {
	if (!f->failed) {
		Sf_Check(f, LvN_Callback(name, module, func));
	}
}

void Lv_MissionChar(scriptFrame_t *f, const char *name, const char *module, const char *func) {
	if (!f->failed) {
		Sf_Check(f, LvN_MissionChar(name, module, func));
	}
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
	return f->failed ? -1 : LvN_SaveIs(name, text);
}

void Lv_Fade(scriptFrame_t *f, const int from[4], const int to[4], double time) {
	if (!f->failed) {
		Sf_Check(f, Mode_Fade(0, from, to, time));
	}
}

PyObject *Lv_Dummy(scriptFrame_t *f, const char *pattern, int flag) {
	Sf_Ask(f, "Scrap", "StartDummySearch", "(si)", pattern, flag);
	return Sf_Ask(f, "Scrap", "NextDummySearch", "()");
}

static byte LvN_AcShips(const char *module, const char *ships);

byte Lv_AcShips(scriptFrame_t *f, const char *module, const char *ships) {
	return !f->failed && Sf_Check(f, LvN_AcShips(module, ships));
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

/* ---- whole functions the level tables share (native: docs/objlib-plan.md, stage 2) ---- */

/* the adapter of a whole function: its n arguments as values, the core, the result (out, None when it stays None) */
#define LV_WHOLE(nargs, call) \
	modeVals_t t = { .n = 0 }; \
	nativeVal_t *a, *out = Mode_V(&t); \
	if (!Mode_Args(args, nargs, &t, &a)) { \
		return Mode_Done(&t, 0), NULL; \
	} \
	(void)a; \
	return Mode_Out(&t, call, out)

/* name + `MapSnd.cntr` (prefix "I" in front with beepI) */
static byte Lv_SndId(const nativeVal_t *name, byte beepI, nativeVal_t *out) {
	nativeVal_t cntr, rep, i, x;
	Native_SetNone(&rep);
	Native_SetNone(&x);
	if (!Mode_G("MapSnd", "cntr", &cntr)) {
		return 0;
	}
	Mode_Repr(&cntr, &rep);
	Native_Free(&cntr);
	byte ok;
	if (beepI) {
		LvN_Str(&i, "I");
		ok = Native_Arith('+', &i, name, &x) && Native_Arith('+', &x, &rep, out);
		Native_Free(&i);
	} else {
		ok = Native_Arith('+', name, &rep, out);
	}
	Native_Free(&rep);
	Native_Free(&x);
	return ok;
}

static byte Lv_SndCore(const nativeVal_t *a, const lvSndElement_t *table) {
	const nativeVal_t *name = &a[0];
	if (!Mode_Import("Sound") || !LvN_GAdd("MapSnd", "cntr", 1, NULL)) {
		return 0;
	}
	const lvSndElement_t *el = NULL;
	for (const lvSndElement_t *t = table; t->name && !el; t++) {
		if (LvN_EqS(name, t->name)) {
			el = t;
		}
	}
	if (!el) {
		return 1;
	}
	nativeVal_t id, pos, rot, off, sum, v, loop;
	Native_SetNone(&id);
	Native_SetNone(&off);
	Native_SetNone(&sum);
	Native_SetEmpty(&pos, NV_TUPLE);
	for (int i = 1; i <= 3; i++) {
		Native_Copy(Native_Append(&pos), &a[i]);
	}
	byte ok = 1;
	int pc = !el->loop || !el->pc ? 1 : LvN_GTrue("MapSnd", "isPC");
	if (pc < 0) {
		ok = 0;
	} else if (el->loop && pc) {
		ok = Lv_SndId(name, 0, &id);
		if (ok && el->front) {
			/* SVec.Add(pos, SVec.Rotate3D((0, 0, -300), (angx, angy, 0))) */
			Native_SetVec(&v, 0, 0, -300);
			Native_SetEmpty(&rot, NV_TUPLE);
			Native_Copy(Native_Append(&rot), &a[4]);
			Native_Copy(Native_Append(&rot), &a[5]);
			Native_SetInt(Native_Append(&rot), 0);
			ok = Mode_Rotate(&v, &rot, &off) && Mode_VecAdd(&pos, &off, &sum);
			Native_Free(&rot);
			if (ok) {
				Native_Free(&pos);
				pos = sum;
				Native_SetNone(&sum);
			}
		}
		if (ok) {
			LvN_Str(&loop, el->loop);
			ok = el->hasPitch ? LV_CALL("Sound", "SetPSLoop", NULL, "vvvdi", &id, &pos, &loop, el->vol, (long)el->pitch) :
				LV_CALL("Sound", "SetPSLoop", NULL, "vvvd", &id, &pos, &loop, el->vol);
			Native_Free(&loop);
		}
	}
	if (ok && el->beep) {
		const char *beep = el->beep;
		if (el->beepXbox) {
			int isPC = LvN_GTrue("MapSnd", "isPC");
			ok = isPC >= 0;
			beep = isPC ? el->beep : el->beepXbox;
		}
		Native_Free(&id);
		Native_Free(&pos);
		Native_SetEmpty(&pos, NV_TUPLE);
		for (int i = 1; i <= 3; i++) {
			Native_Copy(Native_Append(&pos), &a[i]);
		}
		ok = ok && Lv_SndId(name, el->beepI, &id) &&
			LV_CALL("Sound", "SetPSInterval", NULL, "vvsiidi", &id, &pos, beep, (long)el->imin, (long)el->imax, el->ivol, 0L);
	}
	Native_Free(&id);
	Native_Free(&pos);
	Native_Free(&off);
	Native_Free(&sum);
	return ok;
}

PyObject *Lv_SndElements(PyObject *args, const lvSndElement_t *table) {
	LV_WHOLE(6, Lv_SndCore(a, table));
}

static byte Lv_PlanesCore(const char *module) {
	nativeVal_t dummy, n0, plane, ships, engines, both, ang, name, pre, pln, a2, pos;
	if (!Mode_Import("MissionsFuncs") || !Mode_Import("CharAct") || !LvN_ModSetGlobal("CharAct", "TakenPhotoMap", module, "TakenPhoto")) {
		return 0;
	}
	Mode_DummyStart("DM_LocalPlane_*", 1);
	byte ok = 1;
	while (ok) {
		Mode_DummyNext(&dummy);
		if (dummy.type == NV_NONE) {
			break;
		}
		Native_SetNone(&n0);
		Native_SetNone(&plane);
		Native_SetNone(&ships);
		Native_SetNone(&engines);
		Native_SetNone(&both);
		Native_SetNone(&ang);
		Native_SetNone(&name);
		Native_SetNone(&pln);
		Native_SetNone(&a2);
		Native_SetNone(&pos);
		ok = LvN_Item(&dummy, 0, &n0) && Native_Slice(&n0, 14, 0x7fffffff, &plane) && Mode_G(module, "Naves", &ships) &&
			Mode_G(module, "Motores", &engines) && Native_Arith('+', &ships, &engines, &both);
		int in = ok ? Native_Contains(&both, &plane) : -1;
		if (in > 0) {
			/* "Plano" + dummy[0][14:], dummy[1], (ang[0], ang[1], 0) */
			LvN_Str(&pre, "Plano");
			ok = Native_Arith('+', &pre, &plane, &name) && LvN_Item(&dummy, 2, &a2);
			Native_Free(&pre);
			if (ok) {
				Native_SetEmpty(&ang, NV_TUPLE);
				ok = LvN_Item(&a2, 0, Native_Append(&ang)) && LvN_Item(&a2, 1, Native_Append(&ang));
				Native_SetInt(Native_Append(&ang), 0);
			}
			ok = ok && LvN_Item(&dummy, 1, &pos) && LV_CALL("MissionsFuncs", "CreatePhotoObject", &pln, "vvv", &name, &pos, &ang) &&
				LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &pln, 0L);
		} else {
			ok = in == 0;
		}
		Native_Free(&dummy);
		Native_Free(&n0);
		Native_Free(&plane);
		Native_Free(&ships);
		Native_Free(&engines);
		Native_Free(&both);
		Native_Free(&ang);
		Native_Free(&name);
		Native_Free(&pln);
		Native_Free(&a2);
		Native_Free(&pos);
	}
	return ok;
}

PyObject *Lv_PlanesInit(PyObject *args, const char *module) {
	LV_WHOLE(1, Lv_PlanesCore(module));
}

/* the photo of a blueprint into its hangar list: the list of the module (Naves, Motores) loses it and is saved, the
 * hangar list gets item, the message of key with the name of the ship */
static byte Lv_PhotoTo(const char *module, const char *var, const char *saveVar, const nativeVal_t *plano, const char *hangar,
	const nativeVal_t *item, const char *msg, const char *prefix, const nativeVal_t *shipName) {
	nativeVal_t l, lst, up, pre, key, text, name, r;
	Native_SetNone(&lst);
	Native_SetNone(&up);
	Native_SetNone(&key);
	Native_SetNone(&r);
	if (!Mode_Remove(module, var, plano) || !Mode_G(module, var, &l)) {
		return 0;
	}
	LvN_SaveRepr(saveVar, &l);
	Native_Free(&l);
	byte ok = LvN_SaveList(hangar, module, &lst) && LvN_SaveAppend(hangar, &lst, item) && Native_Upper(shipName, &up);
	if (ok) {
		LvN_Str(&pre, prefix);
		ok = Native_Arith('+', &pre, &up, &key);
		Native_Free(&pre);
	}
	const char *k = NULL;
	ok = ok && Native_ArgStr(&key, 1, &k);
	if (ok) {
		/* string.replace(Scrap.GetLangStr(msg), "<n1>", Scrap.GetLangStr(key)) */
		Mode_Lang(msg, &text);
		Mode_Lang(k, &name);
		Mode_Replace(&text, "<n1>", &name, &r);
		ok = LV_CALL("RaceMaker", "ShowSystemMessage", NULL, "v", &r);
		Native_Free(&text);
		Native_Free(&name);
	}
	Native_Free(&lst);
	Native_Free(&up);
	Native_Free(&key);
	Native_Free(&r);
	return ok;
}

static byte Lv_PhotoCore(const nativeVal_t *name, const char *module, const char *ships, const char *engines, nativeVal_t *out) {
	nativeVal_t head, plano, list, up, rest, h, zero;
	Native_SetNone(&head);
	if (!Native_Slice(name, 0, 5, &head)) {
		return 0;
	}
	int is = LvN_EqS(&head, "Plano");
	Native_Free(&head);
	if (!is) {
		return 1;
	}
	Native_SetNone(&plano);
	Native_SetNone(&up);
	Native_SetNone(&rest);
	if (!Mode_Import("string") || !Mode_Import("RaceMaker") || !Native_Slice(name, 5, 0x7fffffff, &plano)) {
		return 0;
	}
	byte ok = Mode_G(module, "Naves", &list);
	int ship = ok ? Native_Contains(&list, &plano) : -1;
	Native_Free(&list);
	int engine = 0;
	if (ship == 0) {
		ok = Mode_G(module, "Motores", &list);
		engine = ok ? Native_Contains(&list, &plano) : -1;
		Native_Free(&list);
	}
	long res = 0;
	if (ship > 0) {
		ok = Lv_PhotoTo(module, "Naves", ships, &plano, "Hangar.shipsToEditList", &plano, "OnPhotoShipBlueprints", "SHIP_", &plano);
		res = 1;
	} else if (engine > 0) {
		/* the engine: its name in capitals into the list, the message of SHIP_S + plano[1:] in capitals */
		ok = Native_Upper(&plano, &up) && Native_Slice(&plano, 1, 0x7fffffff, &rest) &&
			Lv_PhotoTo(module, "Motores", engines, &plano, "Hangar.availableEnginesList", &up, "OnPhotoEngineBlueprints", "SHIP_S", &rest);
		res = 1;
	} else {
		ok = ship >= 0 && engine >= 0;
	}
	Native_Free(&plano);
	Native_Free(&up);
	Native_Free(&rest);
	if (ok && res) {
		Native_SetNone(&h);
		Native_SetInt(&zero, 0);
		ok = Mode_Import("SaveGame") && Mode_EntityH(name, &h) && Mode_VSet(&h, "ArrowCanView", &zero);
		Native_Free(&h);
		if (ok) {
			SaveVar_Inc("NewBluePrintAvaliable", 1);
			ok = LV_CALL("SaveGame", "CheckAllBlueprints", NULL, "");
		}
	}
	Native_SetInt(out, res);
	return ok;
}

PyObject *Lv_PlanesPhoto(PyObject *args, const char *module, const char *ships, const char *engines) {
	LV_WHOLE(1, Lv_PhotoCore(&a[0], module, ships, engines, out));
}

/* import Traffic; Scrap.SetSaveVar("Traffic.AcShips", ships); Traffic.AcShips = eval(Scrap.GetSaveVar(...)) */
static byte LvN_AcShips(const char *module, const char *ships) {
	nativeVal_t v;
	if (!Mode_Import("Traffic")) {
		return 0;
	}
	SaveVar_Set("Traffic.AcShips", ships);
	byte ok = LvN_SaveList("Traffic.AcShips", module, &v) && Mode_SetModVar("Traffic", "AcShips", &v);
	Native_Free(&v);
	return ok;
}

PyObject *Lv_AcShipsStatus(PyObject *args, const char *module, const char *ships) {
	LV_WHOLE(1, LvN_AcShips(module, ships));
}

PyObject *Lv_ProfileFunc(PyObject *args, const char *module, const char *const *vars) {
	(void)module;
	LV_WHOLE(0, LvN_Profile(vars, out));
}

void Lv_Saves(scriptFrame_t *f, const char *const *pairs) {
	for (; *pairs && !f->failed; pairs += 2) {
		SaveVar_Set(pairs[0], pairs[1]);
	}
}

static byte Lv_StartPosCore(const double (*pos)[4], int n) {
	nativeVal_t empty;
	Native_SetEmpty(&empty, NV_LIST);
	if (!Mode_Import("RaceMaker") || !Mode_SetModVar("RaceMaker", "StartPos", &empty)) {
		return 0;
	}
	for (int i = 0; i < n; i++) {
		nativeVal_t p;
		Native_SetEmpty(&p, NV_TUPLE);
		Native_SetVec(Native_Append(&p), pos[i][0], pos[i][1], pos[i][2]);
		Native_SetFloat(Native_Append(&p), pos[i][3]);
		byte ok = LvN_ModAppend("RaceMaker", "StartPos", &p);
		Native_Free(&p);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

PyObject *Lv_StartPos(PyObject *args, const double (*pos)[4], int n) {
	LV_WHOLE(0, Lv_StartPosCore(pos, n));
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
	return !f->failed && Sf_Check(f, LvN_Next(lib, map, desc));
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
static byte Probes_InitStatus(const nativeVal_t *a, const lvProbes_t *p, nativeVal_t *out) {
	nativeVal_t l, zero;
	(void)a;
	(void)p;
	(void)out;
	if (!Mode_Import("VehiclesNPC") || !Mode_Import("Traffic")) {
		return 0;
	}
	Native_SetEmpty(&l, NV_LIST);
	LvN_Str(Native_Append(&l), "TProbe");
	Native_SetInt(&zero, 0);
	byte ok = Mode_SetModVar("Traffic", "AcHeavyShips", &l) && Mode_SetModVar("Traffic", "NumHeavy", &zero);
	Native_Free(&l);
	return ok;
}

static byte Probes_Init(const nativeVal_t *a, const lvProbes_t *p, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LV_OWN(p->module, "CreateProbes", NULL, "") && LvN_ScorerCallG(p->module, "MissionScorer") && LvN_Draw();
}

/* SVec.Mod(SVec.Sub(a, b)) < the global limit of the module: 1, 0, -1 with the error */
static int Probes_Near(const nativeVal_t *a, const nativeVal_t *b, const lvProbes_t *p, const char *limit) {
	nativeVal_t d, m, l;
	Native_SetNone(&d);
	Native_SetNone(&m);
	int r = Mode_VecSub(a, b, &d) && Mode_VecMod(&d, &m) && Mode_G(p->module, limit, &l) ? Native_Compare(&m, &l) < 0 : -1;
	if (r >= 0) {
		Native_Free(&l);
	}
	Native_Free(&d);
	Native_Free(&m);
	return r;
}

/* IsValidPoint(usrPos, pos, posList): a new point far enough from the player and from the other probes */
static byte Probes_IsValidPoint(const nativeVal_t *a, const lvProbes_t *p, nativeVal_t *out) {
	const nativeVal_t *usrPos = &a[0], *pos = &a[1], *list = &a[2];
	int in = Native_Contains(list, pos);
	if (in != 0) {
		Native_SetInt(out, 0);
		return in > 0;
	}
	int near = Probes_Near(pos, usrPos, p, "minPlayerDist");
	if (near != 0) {
		Native_SetInt(out, 0);
		return near > 0;
	}
	int n = Native_ForCount(list);
	if (n < 0) {
		return 0;
	}
	for (int k = 0; k < n; k++) {
		nativeVal_t q;
		Native_SetNone(&q);
		near = Native_SeqItem(list, k, &q) ? Probes_Near(&q, pos, p, "minDistBetweenProbes") : -1;
		Native_Free(&q);
		if (near != 0) {
			Native_SetInt(out, 0);
			return near > 0;
		}
	}
	Native_SetInt(out, 1);
	return 1;
}

/* one probe at pos: an AI ship of the traffic path with the life of the module, as a mission target */
static byte Probes_Make(const lvProbes_t *p, const char *onDeath, const nativeVal_t *pos) {
	nativeVal_t e, ang, life;
	Native_SetNone(&e);
	Native_SetNone(&life);
	byte ok = LV_CALL("Traffic", "SetPath", NULL, "") && LV_CALL("VehiclesNPC", "MakeAI", &e, "ssii", "TProbe", "ProbeMos", 0L, 0L) &&
		LV_CALL("Traffic", "ResetPath", NULL, "") && LvN_SetS(&e, "OnDeath", onDeath) && Mode_VSet(&e, "MovePos", pos) &&
		Mode_VSet(&e, "Pos", pos);
	if (ok) {
		Native_SetEmpty(&ang, NV_TUPLE);
		for (int i = 0; i < 3; i++) {
			Native_SetInt(Native_Append(&ang), 0);
		}
		ok = Mode_VSet(&e, "MoveAng", &ang) && Mode_VSet(&e, "Ang", &ang) && LvN_SetF(&e, "MoveTime", 0.1) &&
			Mode_G(p->module, "PROBE_LIFE", &life) && Mode_VSet(&e, "Life", &life) && Mode_VSet(&e, "MaxLife", &life) &&
			Mode_VSet(&e, "MinLife", &life) && LV_CALL("MissionsFuncs", "ActivateTarget", NULL, "vi", &e, 1L);
		Native_Free(&ang);
	}
	Native_Free(&e);
	Native_Free(&life);
	return ok;
}

/* CreateProbes(): the probes left at random visible points */
static byte Probes_CreateProbes(const nativeVal_t *a, const lvProbes_t *p, nativeVal_t *out) {
	char onDeath[64];
	nativeVal_t usr, list, usrPos, s, num, i, pos, valid;
	(void)a;
	(void)out;
	Str_Printf(onDeath, sizeof(onDeath), "%s.OnProbeDeath", p->module);
	if (!Mode_Import("OutMap") || !Mode_Import("VehiclesNPC") || !Mode_Import("Traffic") || !Mode_UsrHandle(&usr)) {
		return 0;
	}
	if (!Native_True(&usr)) {
		Native_Free(&usr);
		return 1;
	}
	Native_SetEmpty(&list, NV_LIST);
	Native_SetNone(&usrPos);
	Native_SetNone(&num);
	byte ok = Mode_VGet(&usr, "Pos", &usrPos);
	if (ok) {
		Mode_SaveVar("NumProbes", &s);
		ok = Native_ToInt(&s, &num);
		Native_Free(&s);
	}
	long k = 0;
	while (ok) {
		Native_SetInt(&i, k);
		if (Native_Compare(&i, &num) >= 0) {
			break;
		}
		Mode_RandomVisPoint(&pos);
		Native_SetNone(&valid);
		ok = LV_OWN(p->module, "IsValidPoint", &valid, "vvv", &usrPos, &pos, &list);
		if (ok && Native_True(&valid)) {
			ok = Probes_Make(p, onDeath, &pos);
			if (ok) {
				Native_Copy(Native_Append(&list), &pos);
				k++;
			}
		}
		Native_Free(&pos);
		Native_Free(&valid);
	}
	Native_Free(&usr);
	Native_Free(&list);
	Native_Free(&usrPos);
	Native_Free(&num);
	return ok;
}

/* OnProbeDeath(LooserName, WinnerName): one probe fewer; the last one ends the mission */
static byte Probes_OnProbeDeath(const nativeVal_t *a, const lvProbes_t *p, nativeVal_t *out) {
	char msg[64];
	nativeVal_t s, num, one, n, args[4], t;
	(void)out;
	if (!Mode_Import("Vehicles") || !LV_CALL("Vehicles", "OnDeath", NULL, "vv", &a[0], &a[1])) {
		return 0;
	}
	Mode_SaveVar("NumProbes", &s);
	Native_SetNone(&num);
	Native_SetNone(&n);
	Native_SetInt(&one, 1);
	byte ok = Native_ToInt(&s, &num) && Native_Arith('-', &num, &one, &n);
	Native_Free(&s);
	Native_Free(&num);
	if (ok && LvN_EqI(&n, 0)) {
		ok = LV_OWN(p->module, "DestroyEnemyShips", NULL, "") && LvN_ScorerOff();
		if (ok) {
			/* Scrap.AddScheduledFunc(Scrap.GetTime() + 2, Speech.AddRemoteCom, (msg, "CrazyGambler", SetNextMission, ())) */
			Native_SetFloat(&t, Mode_Time() + 2);
			PyObject *fn = Script_Attr("Speech", "AddRemoteCom");
			Str_Printf(msg, sizeof(msg), "%s_RemoteMessage1", p->module);
			LvN_Str(&args[0], msg);
			LvN_Str(&args[1], "CrazyGambler");
			Native_SetEmpty(&args[3], NV_TUPLE);
			ok = fn && LvN_GObj(p->module, "SetNextMission", &args[2]) && Mode_Schedule(&t, fn, args, 4, NULL);
			if (fn) {
				Native_Free(&args[2]);
			}
			Py_XDECREF(fn);
			Native_Free(&args[0]);
			Native_Free(&args[1]);
			Native_Free(&args[3]);
		}
	} else if (ok) {
		LvN_SaveRepr("NumProbes", &n);
		ok = LV_CALL("SaveGame", "QWayPoint", NULL, "");
	}
	Native_Free(&n);
	return ok && LvN_Draw();
}

/* DestroyShipsInList(listName): every ship of the list explodes */
static byte Probes_DestroyShipsInList(const nativeVal_t *a, const lvProbes_t *p, nativeVal_t *out) {
	const char *list;
	nativeVal_t name, e;
	(void)p;
	(void)out;
	/* Scrap.EntityListGet(listName, "First") ("ss") */
	if (!Native_ArgStr(&a[0], 1, &list)) {
		return 0;
	}
	Native_SetNone(&name);
	Native_SetNone(&e);
	byte ok = Mode_ListGet(list, "First", &name) && Mode_EntityH(&name, &e);
	while (ok && Native_True(&e)) {
		ok = LvN_SetS(&e, "EntityList", "") && LvN_SetI(&e, "Life", 0);
		Native_Free(&name);
		Native_Free(&e);
		ok = ok && Mode_ListGet(list, "First", &name) && Mode_EntityH(&name, &e);
	}
	Native_Free(&name);
	Native_Free(&e);
	return ok;
}

/* DestroyEnemyShips(): no more police (MaxChasingShips of the level) nor traffic, the alarm falls, the ships of the
 * lists explode one after another */
static byte Probes_DestroyEnemyShips(const nativeVal_t *a, const lvProbes_t *p, nativeVal_t *out) {
	static const char *const lists[] = { "EnemiesTraffic", "ChasingTraffic", "Traffic", "OutCop" };
	nativeVal_t v, zero;
	(void)a;
	(void)out;
	Native_SetInt(&v, p->maxChasing);
	Native_SetInt(&zero, 0);
	if (!Mode_GVarSet("MaxChasingShips", &v) || !Mode_GVarSet("iTrafficNumber", &zero) || !Mode_GVarSetS("PoliceWaveEliminated", "") ||
		!Mode_GVarSetN("AlarmFallDelta", -0.05) || !Mode_SetAlarm(&zero)) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		LvN_Str(&v, lists[i]);
		byte ok = Mode_Later(0.5 * i, p->module, "DestroyShipsInList", &v, 1, NULL);
		Native_Free(&v);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* SetNextMission() of Commercial and DownTown: the level counts as freed (the save variables of the level), on to the
 * next mission (Outskirts' is data) */
static byte Probes_SetNextMission(const nativeVal_t *a, const lvProbes_t *p, nativeVal_t *out) {
	nativeVal_t l, map;
	(void)a;
	(void)out;
	SaveVars_Delete("NumProbes");
	LvN_Str(&map, p->map);
	byte ok = LvN_SaveList("ComSatsMissionsMapsFinished", p->module, &l) && LvN_SaveAppend("ComSatsMissionsMapsFinished", &l, &map);
	Native_Free(&l);
	Native_Free(&map);
	if (ok && p->saves) {
		for (const char *const *s = p->saves; *s; s += 2) {
			SaveVar_Set(s[0], s[1]);
		}
	}
	return ok && LvN_Next(p->nextLib, p->nextMap, p->nextDesc) && LV_CALL("SaveGame", "QWayPoint", NULL, "");
}

/* MissionScorer(id): the probes left */
static byte Probes_MissionScorer(const nativeVal_t *a, const lvProbes_t *p, nativeVal_t *out) {
	char key[64];
	nativeVal_t label, num;
	(void)out;
	Str_Printf(key, sizeof(key), "%s_Scorer1", p->module);
	if (!Mode_Import("Scorer")) {
		return 0;
	}
	nativeVal_t x, y;
	LvN_Label(key, &label);
	Native_SetNone(&y);
	byte ok = LvN_TextX(0, &x) && LvN_TextY(0, &y) &&
		LvN_MissionText(&a[0], "NumProbesText", "Text", &x, &y, "Horatio", "Shadow", &label, "Left", NULL);
	Native_Free(&label);
	Native_Free(&x);
	Native_Free(&y);
	if (ok) {
		Mode_SaveVar("NumProbes", &num);
		ok = LvN_TextX(0, &x) && LvN_TextY(20, &y) &&
			LvN_MissionText(&a[0], "NumProbes", "Text", &x, &y, "Horatio", "Shadow", &num, "Left", NULL);
		Native_Free(&num);
		Native_Free(&x);
		Native_Free(&y);
	}
	return ok;
}

#define PROBES_FN(n, fn) \
	static byte Probes##n##_##fn##_C(const nativeVal_t *a, nativeVal_t *out) { \
		return Probes_##fn(a, &lv_probes[n], out); \
	} \
	LV_FN(Probes##n, fn, PROBES_ARGS_##fn, Probes##n##_##fn##_C)
#define PROBES_ARGS_InitStatus 1
#define PROBES_ARGS_Init 1
#define PROBES_ARGS_IsValidPoint 3
#define PROBES_ARGS_CreateProbes 0
#define PROBES_ARGS_OnProbeDeath 2
#define PROBES_ARGS_DestroyShipsInList 1
#define PROBES_ARGS_DestroyEnemyShips 0
#define PROBES_ARGS_SetNextMission 0
#define PROBES_ARGS_MissionScorer 1
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
	Native_SetNone(out);
	if (x->type == NV_OBJ) {
		/* OBJLIB 7 */
		PyObject *n = PyInt_FromLong(i), *r = n ? PyObject_GetItem(x->obj, n) : NULL;
		Py_XDECREF(n);
		if (!r) {
			return 0;
		}
		Native_Arg(r, out);
		Py_DECREF(r);
		return 1;
	}
	Native_SetInt(&k, i);
	return Native_Subscript(x, &k, out);
}

void LvN_Elems(const nativeVal_t *v, nativeVal_t *out) {
	PyObject *o = v->type == NV_OBJ ? v->obj : NULL;
	if (!o || (!PyList_Check(o) && !PyTuple_Check(o))) {
		Native_Copy(out, v);
		return;
	}
	/* OBJLIB 7 */
	int n = PyObject_Length(o);
	Native_SetEmpty(out, PyList_Check(o) ? NV_LIST : NV_TUPLE);
	for (int i = 0; i < n; i++) {
		PyObject *x = PySequence_GetItem(o, i);
		if (!x) {
			PyErr_Clear();
			break;
		}
		Native_Arg(x, Native_Append(out));
		Py_DECREF(x);
	}
}

byte LvN_GElems(const char *module, const char *name, nativeVal_t *out) {
	nativeVal_t v;
	if (!LvN_GObj(module, name, &v)) {
		Native_SetNone(out);
		return 0;
	}
	LvN_Elems(&v, out);
	Native_Free(&v);
	return 1;
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

/* the hook of module.name when a case replaced the engine function (1), 0 for the engine's own, -1 with the error */
#define LV_SEAM(module, name) \
	PyObject *hook; \
	int seam = Native_Seam(module, name, &hook); \
	if (seam < 0) { \
		return 0; \
	}

byte LvN_GetClass(const char *name, nativeVal_t *out) {
	LV_SEAM("SAct", "GetClass");
	if (seam) {
		return Native_HookCall(hook, out, "s", name);
	}
	Native_SetInt(out, Native_ActClass(name));
	return 1;
}

byte LvN_CreateClass(const char *name, nativeVal_t *out) {
	LV_SEAM("SAct", "CreateClass");
	if (seam) {
		return Native_HookCall(hook, out, "s", name);
	}
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Create(g_pActClassList, name);
	Native_SetInt(out, g_pAuxActClass != NULL);
	return 1;
}

byte LvN_GetAction(const char *name, nativeVal_t *out) {
	LV_SEAM("SAct", "GetAction");
	if (seam) {
		return Native_HookCall(hook, out, "s", name);
	}
	Native_SetNone(out);
	Native_ActAction(name);
	if (g_pAuxActClass) {
		Native_SetInt(out, g_pAuxAction != NULL);
	}
	return 1;
}

byte LvN_CreateAction(const char *name, nativeVal_t *out) {
	LV_SEAM("SAct", "CreateAction");
	if (seam) {
		return Native_HookCall(hook, out, "s", name);
	}
	Native_SetNone(out);
	if (!g_pAuxActClass) {
		Con_Debugf("Caution: SAct.CreateAction(): AuxActClass not defined.\n");
		return 1;
	}
	g_pAuxAction = cActClass_CreateAction(g_pAuxActClass, name);
	Native_SetInt(out, g_pAuxAction != NULL);
	return 1;
}

byte LvN_GetAct(const char *name, nativeVal_t *out) {
	LV_SEAM("SAct", "GetAct");
	if (seam) {
		return Native_HookCall(hook, out, "s", name);
	}
	Native_ActVar(name, out);
	return 1;
}

byte LvN_SetAct(const char *name, const nativeVal_t *v) {
	LV_SEAM("SAct", "SetAct");
	if (seam) {
		return Native_HookCall(hook, NULL, "sv", name, v);
	}
	Native_ActSet(name, v);
	return 1;
}

byte LvN_SetCls(const char *name, const nativeVal_t *v) {
	LV_SEAM("SAct", "SetCls");
	if (seam) {
		return Native_HookCall(hook, NULL, "sv", name, v);
	}
	return Native_ClsSet(name, v);
}

byte LvN_SetMusic(const char *name, double volume) {
	LV_SEAM("SSound", "SetMusic");
	if (seam) {
		return Native_HookCall(hook, NULL, "sd", name, volume);
	}
	SoundSys_SetMusicVolume(g_pSound, (float)volume);
	SoundSys_SetMusic(g_pSound, name);
	return 1;
}

/* Scorer.<var> + d */
static byte LvN_TextAt(const char *var, long d, nativeVal_t *out) {
	nativeVal_t v, n;
	Native_SetNone(out);
	if (!LvN_ModGet("Scorer", var, &v)) {
		return 0;
	}
	if (!d) {
		*out = v;
		return 1;
	}
	Native_SetInt(&n, d);
	byte ok = Native_Arith('+', &v, &n, out);
	Native_Free(&v);
	return ok;
}

byte LvN_TextX(long d, nativeVal_t *out) {
	return LvN_TextAt("MissionScorerText_X", d, out);
}

byte LvN_TextY(long d, nativeVal_t *out) {
	return LvN_TextAt("MissionScorerText_Y", d, out);
}

byte LvN_ScorerSetS(const nativeVal_t *id, const char *item, const char *var, const char *s) {
	nativeVal_t v;
	LvN_Str(&v, s);
	byte ok = LvN_ScorerSet(id, item, var, &v);
	Native_Free(&v);
	return ok;
}

byte LvN_MissionText(const nativeVal_t *id, const char *item, const char *type, const nativeVal_t *x, const nativeVal_t *y,
	const char *font, const char *effect, const nativeVal_t *text, const char *align, const nativeVal_t *visible) {
	static const char *const rgba[] = { "Red", "Green", "Blue", "Alpha" };
	nativeVal_t it;
	if (type) {
		LvN_Str(&it, item);
		byte ok = Mode_ScorerAdd(id, &it, type);
		Native_Free(&it);
		if (!ok) {
			return 0;
		}
	}
	if (!LvN_ScorerSet(id, item, "X", x) || !LvN_ScorerSet(id, item, "Y", y) || !LvN_ScorerSetS(id, item, "Font", font) ||
		(effect && !LvN_ScorerSetS(id, item, "Effect", effect)) || !LvN_ScorerSet(id, item, "Text", text) ||
		!LvN_ScorerSetS(id, item, "Align", align)) {
		return 0;
	}
	for (int i = 0; i < 4; i++) {
		nativeVal_t c, k;
		Native_SetNone(&k);
		byte ok = LvN_ModGet("Scorer", "MissionScorerText_Color", &c) && LvN_Item(&c, i, &k) && LvN_ScorerSet(id, item, rgba[i], &k);
		Native_Free(&c);
		Native_Free(&k);
		if (!ok) {
			return 0;
		}
	}
	return !visible || LvN_ScorerSet(id, item, "Visible", visible);
}



byte LvN_SetOnFloor(const nativeVal_t *name) {
	LV_SEAM("SLogic", "SetOnFloor");
	if (seam) {
		return Native_HookCall(hook, NULL, "v", name);
	}
	return Mode_SetOnFloor(name);
}

byte LvN_PlaySound(const nativeVal_t *pos, const nativeVal_t *name, const nativeVal_t *volume) {
	LV_SEAM("SSound", "PlaySound");
	if (seam) {
		return Native_HookCall(hook, NULL, "vvv", pos, name, volume);
	}
	return Mode_PlaySound(pos, name, volume);
}

byte LvN_ModSetI(const char *module, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_SetModVar(module, var, &v);
}

byte LvN_Set0(const char *item, const char *var, const nativeVal_t *v) {
	nativeVal_t id;
	Native_SetInt(&id, 0);
	return LvN_ScorerSet(&id, item, var, v);
}

byte LvN_Set0I(const char *item, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return LvN_Set0(item, var, &v);
}
