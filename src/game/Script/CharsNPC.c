/*
 * The CharsNPC module in C (scripts/chars/charsnpc.py, issue #2 step 7d): the NPCs of the indoor levels. The spawn
 * points of the level (DM_NPC_Spawn_*, DM_NPC_Repos_*, DM_Mission_*) become CharSpawnPos objects in CharRes, Init
 * creates the characters from them, and a dead NPC comes back at a free conversion point. The second half is the
 * generic talk of the NPCs: the option lists from the language file (ClassifyOptions) and GenericNPCSpeech.
 *
 * The module variables stay in the module (CharRes, the class CharSpawnPos, AfterBrowseNPC, ReposChars, NPCAttribs,
 * RespawnChars, Repos, optionsList, subOptionsList, ActualEntityName), because the missions and the character
 * scripts read and change them. The module code is data/modules/charsnpc.mod; the __init__ of CharSpawnPos is a C
 * method (CharsNPC_ModSpawnPosInit).
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values, read the variables of the
 * module where the script reads them (a missing one is the NameError of the script) and write them back at once,
 * the dicts and lists in place (OBJLIB 5). The spawn points are instances of the Python class CharSpawnPos and the
 * characters handle values (OBJLIB 5, 7): their attributes are read and set the Python way, and the lists of
 * functions in them (CallbackFunc, AfterBrowseNPC) are walked as the loops of the script walk them, read again on
 * each turn. Scrap.GetEntity, CreateEntity, UsrEntity, ConsoleError, GetAlarm, TestSphere, the dummy search,
 * SLogic.SetOnFloor, SScorer.SetMsgText and the scheduler go through their seams (OBJLIB 7); the save variables,
 * the language texts, Scrap.Rand and string.split are C. The other ports (Chars, InPolice, DTritusDesktop,
 * CharConversor, MissionsFuncs, Speech, Challenge), SAI.EnableAIChar and GetReposCharPos, Scrap.GetLangEntries,
 * string.find and the range of a value that is no int stay by name (OBJLIB 3, 7). The module's own functions call
 * each other in C as the port did.
 */
#include <limits.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define NPC "CharsNPC"

/* ---- the class CharSpawnPos (OBJLIB 9: a class of the Python object library with a C __init__, made by the hooks of
 * the module code) ---- */

/* CharSpawnPos.__init__(self) of charsnpc.py:20: self.CallbackFunc = [] */
static PyObject *Npc_SpawnPosInit(PyObject *self, PyObject *args) {
	PyObject *inst;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &inst)) {
		return NULL;
	}
	PyObject *list = PyList_New(0);
	int err = list ? PyObject_SetAttrString(inst, "CallbackFunc", list) : -1;
	Py_XDECREF(list);
	return err ? NULL : Py_ReturnNone();
}

static PyMethodDef npc_spawnPosInit = { "__init__", Npc_SpawnPosInit, 1, NULL };

/* the __init__ methods made since the interpreter started (a list, NULL before the first), for CharsNPC_Close */
static PyObject *npc_spawnPosInits;

/*
 * The __init__ of CharSpawnPos is a method object around the C function: instance_getattr binds it to the instance
 * as it binds a Python function (a C function alone in the class would get no self). The class does not exist yet
 * when the body of the class statement runs, so the hook in the body (where the script defines __init__, which
 * keeps the key order of the class dict) makes the method with None as its class, and the hook after the class
 * statement puts the class in (im_class) without a write to the dict. The class and the method then hold each other,
 * which the module cleanup of Py_Finalize does not break (a Python function holds the module dict, which the cleanup
 * clears): CharsNPC_Close does.
 */
int CharsNPC_ModSpawnPosInit(PyObject *module, PyObject *locals) {
	(void)module;
	PyObject *f = Script_CheckFunction(&npc_spawnPosInit, NULL, "CharsNPC");
	PyObject *m = f ? PyMethod_New(f, NULL, Py_None) : NULL;
	if (m && !npc_spawnPosInits) {
		npc_spawnPosInits = PyList_New(0);
	}
	int err = m && npc_spawnPosInits ? PyList_Append(npc_spawnPosInits, m) : -1;
	err = err ? err : PyDict_SetItemString(locals, "__init__", m);
	Py_XDECREF(m);
	Py_XDECREF(f);
	return err == 0;
}

int CharsNPC_ModSpawnPosClass(PyObject *module, PyObject *locals) {
	(void)module;
	PyObject *cls = Script_ModuleName(locals, "CharSpawnPos");
	PyObject *m = cls && PyClass_Check(cls) ? PyDict_GetItemString(((PyClassObject *)cls)->cl_dict, "__init__") : NULL;
	if (m && PyMethod_Check(m) && PyMethod_GET_CLASS(m) == Py_None) {
		Py_INCREF(cls);
		((PyMethodObject *)m)->im_class = cls;
		Py_DECREF(Py_None);
	} else if (cls) {
		PyErr_SetString(PyExc_TypeError, "CharSpawnPos: the class of the module definition without its __init__");
	}
	Py_XDECREF(cls);
	return !PyErr_Occurred();
}

/*
 * Before Py_Finalize: the __init__ methods made by CharsNPC_ModSpawnPosInit let go of their class (im_class None
 * again), so the module cleanup frees the class, its dict and the method as it frees a class of Python functions
 */
void CharsNPC_Close(void) {
	PyObject *list = npc_spawnPosInits;
	npc_spawnPosInits = NULL;
	for (int i = 0; list && i < PyList_GET_SIZE(list); i++) {
		PyMethodObject *m = (PyMethodObject *)PyList_GET_ITEM(list, i);
		PyObject *cls = m->im_class;
		Py_INCREF(Py_None);
		m->im_class = Py_None;
		Py_XDECREF(cls);
	}
	Py_XDECREF(list);
}

/*
 * CharSpawnPos(): when the class is the module's own with the __init__ of the pack (self.CallbackFunc = [], the
 * bytecode of scripts/chars/charsnpc.py:20 compared byte for byte, no base class, no __setattr__), the instance is
 * made as PyInstance_New makes it and the attribute set without running the bytecode; any other class is called
 * (the class of data/modules/charsnpc.mod has the C __init__ above, so no Python runs either)
 */
static PyObject *Npc_NewSpawnPos(PyObject *cls) {
	/* SET_LINENO 20, SET_LINENO 21, BUILD_LIST 0, LOAD_FAST 0 (self), STORE_ATTR 1 (CallbackFunc), LOAD_CONST 0 (None),
	 * RETURN_VALUE */
	static const char code[] = "\x7f\x14\x00\x7f\x15\x00\x67\x00\x00\x7c\x00\x00\x5f\x01\x00\x64\x00\x00\x53";
	PyClassObject *c = PyClass_Check(cls) ? (PyClassObject *)cls : NULL;
	PyObject *init = c && PyTuple_Size(c->cl_bases) == 0 && !PyDict_GetItemString(c->cl_dict, "__setattr__") ?
		PyDict_GetItemString(c->cl_dict, "__init__") : NULL;
	PyCodeObject *co = init && PyFunction_Check(init) && !PyFunction_GET_DEFAULTS(init) ?
		(PyCodeObject *)PyFunction_GET_CODE(init) : NULL;
	byte pack = co && co->co_argcount == 1 && PyString_Check(co->co_code) &&
		PyString_GET_SIZE(co->co_code) == (int)sizeof(code) - 1 &&
		!memcmp(PyString_AS_STRING(co->co_code), code, sizeof(code) - 1) &&
		PyTuple_Check(co->co_consts) && PyTuple_GET_SIZE(co->co_consts) >= 1 &&
		PyTuple_GET_ITEM(co->co_consts, 0) == Py_None && PyTuple_Check(co->co_names) &&
		PyTuple_GET_SIZE(co->co_names) >= 2 && PyString_Check(PyTuple_GET_ITEM(co->co_names, 1)) &&
		!strcmp(PyString_AS_STRING(PyTuple_GET_ITEM(co->co_names, 1)), "CallbackFunc");
	if (!pack) {
		return Script_Invoke(cls, PyTuple_New(0));
	}
	PyInstanceObject *inst = PyObject_NEW(PyInstanceObject, &PyInstance_Type);
	if (!inst) {
		return NULL;
	}
	Py_INCREF(cls);
	inst->in_class = c;
	inst->in_dict = PyDict_New();
	PyObject *list = inst->in_dict ? PyList_New(0) : NULL;
	int err = list ? PyDict_SetItemString(inst->in_dict, "CallbackFunc", list) : -1;
	Py_XDECREF(list);
	if (err) {
		Py_DECREF(inst);
		return NULL;
	}
	return (PyObject *)inst;
}

/* ---- the values of the module ---- */

/* a global of the module as its functions read it (out None before) */
static byte Npc_G(const char *name, nativeVal_t *out) {
	return Mode_G(NPC, name, out);
}

/* Scrap.GetSaveVar(name) == text */
static byte Npc_SaveIs(const char *name, const char *text) {
	const char *v = SaveVar_Get(name);
	return v && !strcmp(v, text);
}

/* v is one of the texts (NULL ends them) */
static byte Npc_In(const nativeVal_t *v, const char *const *texts) {
	for (; *texts; texts++) {
		if (Native_IsStr(v, *texts)) {
			return 1;
		}
	}
	return 0;
}

/* the count of a for loop over v that the calls in the loop may make grow: the script reads the list again on each
 * turn (a list held as one NV_OBJ is the list itself); -1 with the TypeError of a value that is no sequence */
static int Npc_Count(const nativeVal_t *v) {
	nativeVal_t c;
	Native_Copy(&c, v);
	int n = Mode_Loop(&c);
	Native_Free(&c);
	return n;
}

/* h.var as any value into a slot that may hold one (it is freed first) */
static byte Npc_Attr(const nativeVal_t *h, const char *var, nativeVal_t *slot) {
	Native_Free(slot);
	return Mode_VObj(h, var, slot);
}

/* lst.insert(0, v) of a list of the function */
static void Npc_Insert0(nativeVal_t *lst, const nativeVal_t *v) {
	Native_Append(lst);
	/* the items move up by one, their memory with them; the first slot then gets the copy */
	memmove(&lst->items[1], &lst->items[0], (size_t)(lst->n - 1) * sizeof(*lst->items));
	Native_Copy(&lst->items[0], v);
}

/* CharRes[key] (the global) as the object it is (OBJLIB 5: the spawn points are instances of the Python class) */
static byte Npc_Res(const nativeVal_t *key, nativeVal_t *out) {
	PyObject *o = Mode_AtPy(NPC, "CharRes", key);
	if (!o) {
		return 0;
	}
	Native_Arg(o, out);
	Py_DECREF(o);
	return 1;
}

/* CharSpawnPos() of the global class (OBJLIB 5) */
static byte Npc_SpawnPos(nativeVal_t *out) {
	PyObject *cls = Mode_GlobalPy(NPC, "CharSpawnPos");
	PyObject *csp = cls ? Npc_NewSpawnPos(cls) : NULL;
	Py_XDECREF(cls);
	if (!csp) {
		return 0;
	}
	Native_Arg(csp, out);
	Py_DECREF(csp);
	return 1;
}

/* string.find(s, sub) != -1 (OBJLIB 3: string.find by name): 1, 0, -1 with the error set */
static int Npc_Find(const nativeVal_t *s, const nativeVal_t *sub) {
	nativeVal_t r = { 0 };
	int c = Native_CallMod("string", "find", &r, "vv", s, sub) ? Mode_CmpI(&r, "!=", -1) : -1;
	Native_Free(&r);
	return c;
}

/* the count of range(n): an int as it is, any other value through range by name (its errors) */
static byte Npc_Range(const nativeVal_t *n, long *count) {
	if (n->type == NV_INT) {
		*count = n->i;
		return 1;
	}
	nativeVal_t r = { 0 };
	int k = Native_CallMod("__builtin__", "range", &r, "v", n) ? Native_Size(&r) : -1;
	Native_Free(&r);
	*count = k;
	return k >= 0;
}

/* ---- the spawn points ---- */

/* AddNPCPos(dummy, IsMissionChar=0): a CharSpawnPos from the dummy (name, pos, ang, flag) into CharRes; a name
 * already there gets "_R" (mission NULL: not given; the spawn point into out) */
static byte Npc_AddNPCPos(const nativeVal_t *dummy, const nativeVal_t *mission, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *flag = Mode_V(&t), *d0 = Mode_V(&t), *nam = Mode_V(&t), *act = Mode_V(&t), *csp = Mode_V(&t);
	nativeVal_t *d1 = Mode_V(&t), *name = Mode_V(&t), *d2 = Mode_V(&t), *y = Mode_V(&t), *yt = Mode_V(&t);
	nativeVal_t *ang = Mode_V(&t), *cur = Mode_V(&t), *msg = Mode_V(&t), *dup = Mode_V(&t), *key = Mode_V(&t);
	if (!Mode_Import("string") || !Mode_Import("Chars")) {
		return Mode_Done(&t, 0);
	}
	if (mission) {
		Native_Copy(flag, mission);
	} else {
		Native_SetInt(flag, 0);
	}
	byte m = Native_True(flag);
	long starts = m ? 11 : 13, typeid = m ? 2 : 3;
	byte ok = Mode_Item(dummy, 0, d0) && Native_Split(d0, "_", nam) && Mode_Item(nam, typeid, act) &&
		Native_CallMod("Chars", "AddNewCharRes", NULL, "vv", act, flag) && Npc_SpawnPos(csp) && Mode_Item(dummy, 1, d1) &&
		Mode_VSet(csp, "Pos", d1) && Native_Slice(d0, starts, INT_MAX, name) && Mode_VSet(csp, "Name", name) &&
		Mode_Item(dummy, 2, d2) && Mode_Item(d2, 1, y) && Mode_Arith('+', y, Mode_I(&t, 180), yt);
	if (ok) {
		Mode_Tuple3(ang, Mode_I(&t, 0), yt, Mode_I(&t, 0));
		ok = Mode_VSet(csp, "Ang", ang) && Mode_VSet(csp, "ActCtrl", act) && Mode_VObj(csp, "Name", cur);
	}
	int has = ok ? Mode_HasKey(NPC, "CharRes", 1, cur) : -1;
	if (has > 0) {
		has = Mode_Cat(&t, msg, 4, cur, Mode_S(&t, "  char duplicated, using "), cur, Mode_S(&t, "_R\n")) &&
			Mode_ConsoleError(msg) && Mode_Cat(&t, dup, 2, cur, Mode_S(&t, "_R")) && Mode_VSet(csp, "Name", dup) ? 0 : -1;
	}
	ok = has >= 0 && Mode_VObj(csp, "Name", key) && Mode_DictPut(NPC, "CharRes", NULL, key, csp);
	if (ok && out) {
		Native_Copy(out, csp);
	}
	return Mode_Done(&t, ok);
}

/* CreateMissionChar(Name): the spawn point of a mission character from its dummy DM_Mission_<Name> */
static byte Npc_CreateMissionChar(const nativeVal_t *name, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pat = Mode_V(&t), *dummy = Mode_V(&t), *msg = Mode_V(&t);
	if (!LvN_Cat("DM_Mission_", name, pat) || !Mode_DummyStartV(pat, 0) || !Mode_DummyNextSeam(dummy)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(dummy)) {
		return Mode_Done(&t, Npc_AddNPCPos(dummy, Mode_I(&t, 1), out));
	}
	return Mode_Done(&t, Mode_Cat(&t, msg, 3, Mode_S(&t, "TWISTED ERROR : DM_Mission_"), name, Mode_S(&t, " Dummy not found\n")) &&
		Mode_ConsoleError(msg));
}

/* DeleteMissionChar(Name): the entity goes, and its spawn point */
static byte Npc_DeleteMissionChar(const nativeVal_t *name) {
	nativeVal_t e = { 0 };
	byte ok = Mode_EntitySeam(name, &e) && (!Native_True(&e) || Mode_VDel(&e)) && Mode_DictDel(NPC, "CharRes", NULL, name);
	Native_Free(&e);
	return ok;
}

/* SetReposChars(CharType, Num): how many repositioned characters of a type the level has */
static byte Npc_SetReposChars(const nativeVal_t *type, const nativeVal_t *num) {
	return Mode_DictPut(NPC, "ReposChars", NULL, type, num);
}

/* the next DM_NPC_Repos_ dummy as a repositioned spawn point; a police also gets its Gear (the name with "Gear" in
 * place of characters 13 to 18) */
static byte Npc_ReposOne(byte police) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *csp = Mode_V(&t), *d0 = Mode_V(&t), *head = Mode_V(&t), *gear = Mode_V(&t);
	nativeVal_t *tail = Mode_V(&t), *name = Mode_V(&t), *d1 = Mode_V(&t), *d2 = Mode_V(&t), *d3 = Mode_V(&t);
	nativeVal_t *dummy2 = Mode_V(&t), *csp2 = Mode_V(&t);
	byte ok = Mode_DummyNextSeam(dummy) && Npc_AddNPCPos(dummy, NULL, csp) && LvN_SetI(csp, "repos", 1);
	if (ok && police) {
		ok = Mode_Item(dummy, 0, d0) && Native_Slice(d0, 0, 13, head) && Mode_Cat(&t, gear, 2, head, Mode_S(&t, "Gear")) &&
			Native_Slice(d0, 19, INT_MAX, tail) && Mode_Cat(&t, name, 2, gear, tail) && Mode_Item(dummy, 1, d1) &&
			Mode_Item(dummy, 2, d2) && Mode_Item(dummy, 3, d3);
		if (ok) {
			Mode_Tuple3(dummy2, name, d1, d2);
			Native_Copy(Native_Append(dummy2), d3);
			ok = Npc_AddNPCPos(dummy2, NULL, csp2) && LvN_SetI(csp2, "repos", 1);
		}
	}
	return Mode_Done(&t, ok);
}

/* CreateReposChars(): the repositioned spawn points of the types in ReposChars, then `del ReposChars` */
static byte Npc_CreateReposChars(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *repos = Mode_V(&t), *keys = Mode_V(&t);
	int n = Npc_G("ReposChars", repos) && Mode_Keys(NPC, "ReposChars", 1, keys) ? Mode_Loop(keys) : -1;
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *type = Mode_V(&t), *num = Mode_V(&t), *pat = Mode_V(&t);
		long count = 0;
		byte ok = Native_SeqItem(keys, i, type) && Mode_Sub(repos, type, num);
		byte police = Native_IsStr(type, "Police");
		ok = ok && Mode_Cat(&t, pat, 3, Mode_S(&t, "DM_NPC_Repos_"), type, Mode_S(&t, "_*")) && Mode_DummyStartV(pat, 1) &&
			Npc_Range(num, &count);
		for (long j = 0; ok && j < count; j++) {
			ok = Npc_ReposOne(police);
		}
		if (!ok) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, n >= 0 && Mode_DelGlobal(NPC, "ReposChars"));
}

/* BrowseNPC(): the spawn points of the level (the DM_NPC_Spawn_ dummies with their flag set), then the hooks the
 * missions put into AfterBrowseNPC */
static byte Npc_BrowseNPC(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *after = Mode_V(&t);
	if (!Mode_Import("DTritusDesktop") || !Native_CallMod("DTritusDesktop", "InitNPCS", NULL, "") || !Npc_CreateReposChars() ||
		!Mode_DummyStartSeam("DM_NPC_Spawn_*", 1)) {
		return Mode_Done(&t, 0);
	}
	for (;;) {
		nativeVal_t *dummy = Mode_V(&t), *flag = Mode_V(&t);
		if (!Mode_DummyNextSeam(dummy)) {
			return Mode_Done(&t, 0);
		}
		if (dummy->type == NV_NONE) {
			break;
		}
		if (!Mode_Item(dummy, 3, flag) || (Native_True(flag) && !Npc_AddNPCPos(dummy, NULL, NULL))) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, t.n - 2);
	}
	/* the list may grow while the hooks run */
	byte ok = Npc_G("AfterBrowseNPC", after);
	for (int i = 0; ok && i < Npc_Count(after); i++) {
		nativeVal_t f = { 0 };
		ok = Mode_Item(after, i, &f) && Mode_CallArgs(&f, NULL, NULL, 0);
		Native_Free(&f);
	}
	return Mode_Done(&t, ok && Npc_Count(after) >= 0);
}

/* AssignAttrib(c): money, reward and life of the type from NPCAttribs (else 50, 5, 100), PrevLife as Life */
static byte Npc_AssignAttrib(const nativeVal_t *c) {
	static const char *const vars[3] = { "Money", "MoneyReward", "Life" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *act = Mode_V(&t), *attribs = Mode_V(&t), *vals = Mode_V(&t), *life = Mode_V(&t), *v = Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	int has = Mode_VObj(c, "ActCtrl", act) && Npc_G("NPCAttribs", attribs) ? Mode_HasKey(NPC, "NPCAttribs", 1, act) : -1;
	if (has == 0) {
		Mode_Ints3(vals, 50, 5, 100);
	}
	byte ok = has > 0 ? Mode_Sub(attribs, act, vals) : has == 0;
	if (ok) {
		Mode_Elems(vals);
		ok = Mode_Unpack(vals, 3, v);
	}
	for (int i = 0; ok && i < 3; i++) {
		ok = Mode_VSet(c, vars[i], &v[i]);
	}
	return Mode_Done(&t, ok && Mode_VObj(c, "Life", life) && Mode_VSet(c, "PrevLife", life));
}

/* CharNPCInclude(chrname): the AI and the entity list of the type */
static byte Npc_CharNPCInclude(const nativeVal_t *name) {
	static const char *const gate[] = { "GateKeeper", "Sebastian", NULL };
	static const char *const gen[] = { "Mayor", "BankDirector", "BankMaster", "Desktop", "Berto", "Betty", "Boss",
		"Humphrey", "Bishop", "Functionary", "Rusty", "SputnikJunkyard", "Maintenance", "MercenaryA", "MercenaryB",
		"MercenaryC", "MercenaryL", "CrazyGambler", "PoliceBoss", "Messenger", "Human", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *ctr = Mode_V(&t), *act = Mode_V(&t), *list = Mode_V(&t), *ch = Mode_V(&t);
	if (!Npc_Res(name, ctr) || !Mode_VObj(ctr, "ActCtrl", act)) {
		return Mode_Done(&t, 0);
	}
	byte ai = 0;
	if (Native_IsStr(act, "Nurse")) {
		ai = 1;
		Native_SetStr(list, "Nurse");
	} else if (Npc_In(act, gate)) {
		Native_SetStr(list, "GateKeeper");
	} else if (Npc_In(act, gen)) {
		ai = 1;
		if (Native_IsStr(act, "Bishop") || Native_IsStr(act, "Messenger")) {
			Native_Copy(list, act);
		} else {
			Native_SetStr(list, "GenChars");
		}
	} else {
		return Mode_Done(&t, 1);
	}
	return Mode_Done(&t, Mode_EntitySeam(name, ch) && (!ai || Native_CallMod("SAI", "EnableAIChar", NULL, "vii", name, 1L, 0L)) &&
		Mode_VSet(ch, "EntityList", list));
}

/* CreateChar(csp, Fixed=0): the character of a spawn point, a WalkChar that talks through Speech.AutoSpeech;
 * standing on the floor, or fixed where it is; the callbacks of the spawn point get it (fixed NULL: not given; the
 * character into out) */
static byte Npc_CreateChar(const nativeVal_t *csp, const nativeVal_t *fixed, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *pos = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t);
	nativeVal_t *c = Mode_V(&t), *cname = Mode_V(&t), *msg = Mode_V(&t), *act = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *key = Mode_V(&t), *desc = Mode_V(&t), *talk = Mode_V(&t), *opt = Mode_V(&t);
	nativeVal_t *repos = Mode_V(&t), *funcs = Mode_V(&t);
	if (!Mode_Import("Chars") || !Mode_Import("InPolice") || !Mode_VObj(csp, "Name", name) || !Mode_VObj(csp, "Pos", pos) ||
		!Mode_Item(pos, 0, x) || !Mode_Item(pos, 1, y) || !Mode_Item(pos, 2, z) || !LvN_Create(name, x, y, z, "WalkChar", c) ||
		!Mode_VObj(c, "Name", cname)) {
		return Mode_Done(&t, 0);
	}
	int diff = Mode_Cmp(name, "!=", cname);
	if (diff < 0 || (diff && (!LvN_Cat("Imposible crear el personaje ", name, msg) || !Mode_ConsoleError(msg) ||
		!Mode_VDel(c)))) {
		return Mode_Done(&t, 0);
	}
	nativeVal_t *act2 = Mode_V(&t), *cpos = Mode_V(&t), *cang = Mode_V(&t), *n = Mode_V(&t), *rv = Mode_V(&t);
	byte ok = Mode_VObj(csp, "ActCtrl", act) && Mode_VSet(c, "ActCtrl", act) && Mode_VObj(csp, "Ang", ang) &&
		Mode_VSet(c, "Ang", ang) && LvN_SetI(c, "ArrowCanUse", 1) && LvN_SetI(c, "ArrowCanView", 1) &&
		LvN_SetS(c, "UseFunc", "Speech.AutoSpeech") && LvN_SetS(c, "ViewFunc", "Speech.InspectChar") &&
		Mode_VObj(csp, "ActCtrl", act2) && LvN_Cat("CharName_", act2, key) && Mode_LangV(key, desc) &&
		Mode_VSet(c, "Descriptor", desc) && LvN_SetI(c, "ArrnOpt", 0);
	if (ok) {
		Mode_Lang("TalkOpt", talk);
		ok = LvN_Cat("Talk:", talk, opt) && Mode_VSet(c, "ArrOpt0", opt);
	}
	int fix = ok ? (fixed ? Native_True(fixed) : 0) : -1;
	if (fix > 0) {
		ok = Mode_VObj(c, "Pos", cpos) && Mode_VSet(c, "MovePos", cpos) && Mode_VObj(c, "Ang", cang) &&
			Mode_VSet(c, "MoveAng", cang) && LvN_SetF(c, "MoveTime", 0.001) && LvN_SetI(c, "MoveViewAng", 0);
	} else if (fix == 0) {
		ok = Mode_VObj(c, "Name", n) && LvN_SetOnFloor(n);
	}
	ok = ok && Npc_AssignAttrib(c) && Npc_Attr(c, "Name", n) && Native_CallMod("InPolice", "Include", NULL, "v", n) &&
		Npc_Attr(c, "Name", n) && Npc_CharNPCInclude(n) && Mode_VObj(csp, "repos", repos);
	ok = ok && (!Native_True(repos) || (Npc_G("Repos", rv) && Mode_VSet(c, "AI_Repos", rv))) &&
		Mode_VObj(csp, "CallbackFunc", funcs);
	for (int i = 0; ok && i < Npc_Count(funcs); i++) {
		nativeVal_t f = { 0 };
		ok = Mode_Item(funcs, i, &f) && Mode_CallArgs(&f, NULL, c, 1);
		Native_Free(&f);
	}
	ok = ok && Npc_Count(funcs) >= 0;
	if (ok && out) {
		Native_Copy(out, c);
	}
	return Mode_Done(&t, ok);
}

/* CallCreateChar(EntityName, x, y, z): the character of a spawn point at another place */
static byte Npc_CallCreateChar(const nativeVal_t *name, const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *z) {
	nativeVal_t csp = { 0 }, pos = { 0 };
	byte ok = Npc_Res(name, &csp);
	if (ok) {
		Mode_Tuple3(&pos, x, y, z);
		ok = Mode_VSet(&csp, "Pos", &pos) && Npc_CreateChar(&csp, NULL, NULL);
	}
	Native_Free(&pos);
	Native_Free(&csp);
	return ok;
}

/* Init(): the characters of all spawn points (not the Gear of a repositioned police; gatekeepers, Sputnik and
 * Sebastian fixed), then Repos = 1 */
static byte Npc_Init(void) {
	static const char *const fixedTypes[] = { "GateKeeper", "Sputnik", "Sebastian", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *res = Mode_V(&t), *keys = Mode_V(&t);
	int n = Npc_G("CharRes", res) && Mode_Keys(NPC, "CharRes", 1, keys) ? Mode_Loop(keys) : -1;
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *k = Mode_V(&t), *cls = Mode_V(&t), *repos = Mode_V(&t), *act = Mode_V(&t);
		int skip = Native_SeqItem(keys, i, k) && Mode_Sub(res, k, cls) && Mode_VObj(cls, "repos", repos) ?
			Native_True(repos) : -1;
		if (skip > 0) {
			skip = Mode_VObj(cls, "ActCtrl", act) ? Native_IsStr(act, "Gear") : -1;
		}
		if (skip == 0) {
			Native_Free(act);
			skip = Mode_VObj(cls, "ActCtrl", act) && Npc_CreateChar(cls, Mode_I(&t, Npc_In(act, fixedTypes)), NULL) ? 1 : -1;
		}
		if (skip < 0) {
			return Mode_Done(&t, 0);
		}
		Mode_Release(&t, mark);
	}
	return Mode_Done(&t, n >= 0 && Mode_Put(NPC, "Repos", NULL, Mode_I(&t, 1)));
}

/* ---- the respawn ---- */

/* `Scrap.TestSphere(CPPos[0], CPPos[1], CPPos[2], 200, "rv")` of CharPos = CharConversor.CPPos(conv): 1, 0, -1 */
static int Npc_TestConv(const nativeVal_t *conv) {
	nativeVal_t pos = { 0 }, hit = { 0 };
	int c = Native_CallMod("CharConversor", "CPPos", &pos, "v", conv) && Mode_TestSpherePos(&pos, 200, "rv", &hit) ?
		Native_True(&hit) : -1;
	Native_Free(&pos);
	Native_Free(&hit);
	return c;
}

/* Respawn(csp): after a death the character comes back at the nearest free conversion point (a repositioned one
 * near the player), else it tries again in 5 s; a Gear of a repositioned police without alarm comes back as the
 * police; a mission target is marked again */
static byte Npc_Respawn(const nativeVal_t *cspArg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *csp = Mode_V(&t), *name = Mode_V(&t), *act = Mode_V(&t), *repos = Mode_V(&t), *res = Mode_V(&t);
	nativeVal_t *tail = Mode_V(&t), *key = Mode_V(&t), *usr = Mode_V(&t), *upos = Mode_V(&t), *rpos = Mode_V(&t);
	nativeVal_t *cpos = Mode_V(&t), *conv = Mode_V(&t), *prev = Mode_V(&t), *chars = Mode_V(&t), *c = Mode_V(&t);
	nativeVal_t *v = Mode_V(&t), *msg = Mode_V(&t), *p = Mode_V(&t), *a = Mode_V(&t), *n = Mode_V(&t);
	Native_Copy(csp, cspArg);
	if (!Mode_Import("CharConversor") || !Mode_Import("Chars") || !Mode_Import("MissionsFuncs") || !Mode_Import(NPC) ||
		!Mode_VObj(csp, "Name", name) || !Mode_PrintV(name) ||
		!Mode_VObj(csp, "ActCtrl", act)) {
		return Mode_Done(&t, 0);
	}
	int swap = Native_IsStr(act, "Gear");
	if (swap) {
		swap = Mode_VObj(csp, "repos", repos) ? Mode_CmpI(repos, "==", 1) : -1;
	}
	if (swap > 0) {
		int on = Mode_AlarmOn();
		swap = on < 0 ? -1 : !on;
	}
	if (swap > 0) {
		swap = Mode_ModGet(NPC, "CharRes", res) && Npc_Attr(csp, "Name", n) && Native_Slice(n, 4, INT_MAX, tail) &&
			LvN_Cat("Police", tail, key) && Mode_Sub(res, key, v) ? 1 : -1;
		if (swap > 0) {
			Native_Free(csp);
			*csp = *v;
			Native_SetNone(v);
		}
	}
	int dorepos = swap >= 0 && Mode_UsrHandle(usr) && Npc_Attr(csp, "repos", repos) ? Native_True(repos) : -1;
	if (dorepos > 0) {
		dorepos = Native_True(usr);
	}
	if (dorepos > 0) {
		dorepos = Mode_VObj(usr, "Pos", upos) && Npc_Attr(csp, "ActCtrl", act) &&
			Native_CallMod("SAI", "GetReposCharPos", rpos, "vv", upos, act) ? Native_True(rpos) : -1;
		if (dorepos > 0 && !Mode_VSet(csp, "Pos", rpos)) {
			dorepos = -1;
		}
	}
	int hit = dorepos >= 0 && Mode_VObj(csp, "Pos", cpos) &&
		Native_CallMod("CharConversor", "GetNearestConversionPoint", conv, "v", cpos) ? Npc_TestConv(conv) : -1;
	if (hit > 0 && Native_True(usr)) {
		if (!Mode_ModGet("CharConversor", "PrevConv", prev)) {
			hit = -1;
		} else if (Native_True(prev)) {
			Native_Free(conv);
			Native_Copy(conv, prev);
		} else {
			Native_Free(conv);
			Native_Free(upos);
			if (!Mode_UsrHandle(v) || !Mode_VObj(v, "Pos", upos) ||
				!Native_CallMod("CharConversor", "GetNearestConversionPoint", conv, "v", upos)) {
				hit = -1;
			}
		}
		hit = hit < 0 ? -1 : Npc_TestConv(conv);
	}
	int later = hit;
	if (later == 0) {
		int in = Npc_Attr(csp, "ActCtrl", act) && Npc_G("RespawnChars", chars) ? Mode_In(chars, act) : -1;
		later = in < 0 ? -1 : in ? 0 : Mode_AlarmOn();
	}
	if (later) {
		return Mode_Done(&t, later > 0 && Mode_LaterIn(5.0, NPC, "Respawn", csp, 1, NULL));
	}
	Native_Free(name);
	Native_Free(n);
	byte ok = Npc_CreateChar(csp, NULL, c) && Native_CallMod("CharConversor", "SpawnChar", NULL, "vv", c, conv) &&
		LvN_SetI(c, "LLogicIdle", 1) && LvN_SetI(c, "Invulnerable", 1) && Mode_VObj(c, "Name", name) &&
		Mode_LaterIn(1.5, NPC, "ActiveSpawned", name, 1, NULL);
	if (ok && dorepos) {
		ok = Mode_VObj(c, "Pos", p) && Mode_VObj(c, "ActCtrl", a) && Mode_VObj(c, "Name", n) &&
			Native_CallMod("SAI", "GetReposCharPos", NULL, "vvv", p, a, n);
	}
	Native_Free(v);
	int target = ok && Npc_Attr(c, "Name", n) && Native_CallMod("MissionsFuncs", "IsCharMissionObjective", v, "v", n) ?
		Native_True(v) : -1;
	if (target > 0) {
		if (Native_CallMod("MissionsFuncs", "ActivateTarget", NULL, "vi", c, 1L)) {
			Mode_Lang("MainMissionCharReborn", msg);
			target = Mode_MsgText(msg, Mode_Time() + 5) ? 0 : -1;
		} else {
			target = -1;
		}
	}
	return Mode_Done(&t, target == 0);
}

/* ActiveSpawned(EntityName): 1.5 s after the respawn the character acts and can be hurt again */
static byte Npc_ActiveSpawned(const nativeVal_t *name) {
	nativeVal_t c = { 0 };
	byte ok = Mode_EntitySeam(name, &c) && LvN_SetI(&c, "LLogicIdle", 0) && LvN_SetI(&c, "Invulnerable", 0);
	Native_Free(&c);
	return ok;
}

/* ---- the generic talk ---- */

/* CheckActualEntity(Ename): the last talking character (when idle by the talk) gets its state back when another
 * one talks now */
static byte Npc_CheckActualEntity(const nativeVal_t *ename) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cur = Mode_V(&t), *ent = Mode_V(&t), *idle = Mode_V(&t);
	if (!Mode_Import("Speech") || !Npc_G("ActualEntityName", cur)) {
		return Mode_Done(&t, 0);
	}
	int diff = Mode_Cmp(ename, "!=", cur);
	if (diff > 0) {
		int i = Mode_EntitySeam(cur, ent) ? Native_True(ent) : -1;
		if (i > 0) {
			i = Mode_VObj(ent, "LLogicIdle", idle) ? Native_True(idle) : -1;
		}
		diff = i < 0 ? -1 : !i || Native_CallMod("Speech", "RestoreState", NULL, "v", cur) ? 0 : -1;
	}
	return Mode_Done(&t, diff >= 0 && Mode_Put(NPC, "ActualEntityName", NULL, ename));
}

/* MercsHired(): one of the three mercenaries is a friend of the player */
static byte Npc_MercsHired(void) {
	return !(Npc_SaveIs("MercFriends.MercenaryA_Smartie", "0") && Npc_SaveIs("MercFriends.MercenaryB_Dumber", "0") &&
		Npc_SaveIs("MercFriends.MercenaryC_Brutus", "0"));
}

/* IncludeSubOptions(option): the sub options that contain option move from subOptionsList to optionsList */
static byte Npc_IncludeSubOptions(const nativeVal_t *opt) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *subs = Mode_V(&t), *aux = Mode_V(&t);
	Native_SetEmpty(aux, NV_LIST);
	byte ok = Mode_Import("string") && Npc_G("subOptionsList", subs);
	for (int i = 0; ok && i < Npc_Count(subs); i++) {
		nativeVal_t s = { 0 };
		int in = Mode_Item(subs, i, &s) ? Npc_Find(&s, opt) : -1;
		ok = in >= 0 && (!in || (Mode_Append(NPC, "optionsList", &s) && LvN_Append(aux, &s)));
		Native_Free(&s);
	}
	ok = ok && Npc_Count(subs) >= 0;
	for (int i = 0; ok && i < aux->n; i++) {
		ok = Mode_Remove(NPC, "subOptionsList", &aux->items[i]);
	}
	return Mode_Done(&t, ok);
}

/* Classify(l, cad): the entries of l that contain cad, None when there are none */
static byte Npc_Classify(const nativeVal_t *l, const nativeVal_t *cad, nativeVal_t *out) {
	nativeVal_t seq, aux;
	Native_Copy(&seq, l);
	Native_SetEmpty(&aux, NV_LIST);
	int n = Mode_Import("string") ? Mode_Loop(&seq) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t c = { 0 };
		int in = Native_SeqItem(&seq, i, &c) ? Npc_Find(&c, cad) : -1;
		ok = in >= 0 && (!in || LvN_Append(&aux, &c));
		Native_Free(&c);
	}
	if (ok && aux.n > 0) {
		Native_Free(out);
		*out = aux;
	} else {
		Native_Free(&aux);
	}
	Native_Free(&seq);
	return ok;
}

/* "Generics_" + act + charType + "_" */
static byte Npc_Generics(modeVals_t *t, const nativeVal_t *act, const nativeVal_t *type, nativeVal_t *out) {
	Native_Free(out);
	return Mode_Cat(t, out, 4, Mode_S(t, "Generics_"), act, type, Mode_S(t, "_"));
}

/* Scrap.GetLangEntries(key) (OBJLIB 3: by name) != []: 1, 0, -1 */
static int Npc_Entries(const nativeVal_t *key, nativeVal_t *out) {
	nativeVal_t empty;
	Native_SetEmpty(&empty, NV_LIST);
	Native_Free(out);
	return Native_CallMod("Scrap", "GetLangEntries", out, "v", key) ? Mode_Cmp(out, "!=", &empty) : -1;
}

/* ClassifyOptions(charType): the options of a type in the language file (Generics_<GameAct><type>_<n>..., earlier
 * acts when the act has none), the longest ones as sub options; the disguise option of Rusty and the crazy gambler
 * and the Special option go first. (optsList, subOptsList) into out */
static byte Npc_ClassifyOptions(const nativeVal_t *type, nativeVal_t *out) {
	static const char *const disguised[] = { "Rusty", "CrazyGambler", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *opts = Mode_V(&t), *subs = Mode_V(&t), *act = Mode_V(&t), *cad = Mode_V(&t), *key = Mode_V(&t);
	nativeVal_t *all = Mode_V(&t), *usr = Mode_V(&t), *ua = Mode_V(&t), *s = Mode_V(&t), *special = Mode_V(&t);
	nativeVal_t *first = Mode_V(&t);
	long i = 1, minLen = 1000, maxLen = -1;
	char num[24];
	Native_SetEmpty(opts, NV_LIST);
	Native_SetEmpty(subs, NV_LIST);
	Mode_SaveVar("GameAct", act);
	if (!Npc_Generics(&t, act, type, cad)) {
		return Mode_Done(&t, 0);
	}
	for (;;) {
		Str_Printf(num, sizeof(num), "%ld", i);
		Native_Free(key);
		int c = Mode_Cat(&t, key, 3, cad, Mode_S(&t, num), Mode_S(&t, "Opt")) ? Npc_Entries(key, all) : -1;
		if (c < 0) {
			return Mode_Done(&t, 0);
		}
		const char *next = Native_IsStr(act, "Prologue") ? "1stMurder" : Native_IsStr(act, "1stMurder") ? "2ndMurder" :
			Native_IsStr(act, "2ndMurder") ? "3rdMurder" : Native_IsStr(act, "3rdMurder") || Native_IsStr(act, "Epilogue") ? "" :
			NULL;
		if (c || !next) {
			break;
		}
		Native_Free(act);
		Native_SetStr(act, next);
		if (!Npc_Generics(&t, act, type, cad)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_PrintV(all)) {
		return Mode_Done(&t, 0);
	}
	for (;;) {
		int mark = t.n;
		nativeVal_t *pre = Mode_V(&t), *aux = Mode_V(&t);
		Str_Printf(num, sizeof(num), "%ld", i);
		if (!Mode_Cat(&t, pre, 2, cad, Mode_S(&t, num)) || !Npc_Classify(all, pre, aux)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(aux)) {
			break;
		}
		for (int j = 0; j < aux->n; j++) {
			const nativeVal_t *opt = &aux->items[j];
			int l = Native_Size(opt);
			if (l < 0) {
				return Mode_Done(&t, 0);
			}
			Native_Copy(Native_Append(opts), opt);
			if (l <= minLen) {
				minLen = l;
			} else if (l >= maxLen) {
				maxLen = l;
			}
		}
		Mode_Release(&t, mark);
		i++;
	}
	if (minLen != 1000 && maxLen != -1) {
		nativeVal_t *aux = Mode_V(&t);
		Native_SetEmpty(aux, NV_LIST);
		for (int j = 0; j < opts->n; j++) {
			if (Native_Size(&opts->items[j]) == maxLen) {
				Native_Copy(Native_Append(aux), &opts->items[j]);
				Native_Copy(Native_Append(subs), &opts->items[j]);
			}
		}
		for (int j = 0; j < aux->n; j++) {
			LvN_ListRemove(opts, &aux->items[j]);
		}
	}
	int d = Mode_UsrHandle(usr) ? Native_True(usr) : -1;
	if (d > 0) {
		d = Mode_VObj(usr, "ActCtrl", ua) ? !Native_IsStr(ua, "Dtritus") && Npc_In(type, disguised) : -1;
	}
	if (d > 0) {
		d = Mode_Cat(&t, s, 3, Mode_S(&t, "Generics_Disguise"), type, Mode_S(&t, "_1")) ? 0 : -1;
		if (!d) {
			Npc_Insert0(opts, s);
		}
	}
	if (d < 0) {
		return Mode_Done(&t, 0);
	}
	if (!Native_IsStr(type, "Bishop") || Npc_SaveIs("Player.InfiniteLives", "0")) {
		Native_Free(key);
		int c = Mode_Cat(&t, key, 4, Mode_S(&t, "Generics_Special"), type, Mode_S(&t, "_"), Mode_S(&t, "1Opt")) ?
			Npc_Entries(key, special) : -1;
		if (c < 0 || (c && !Mode_Item(special, 0, first))) {
			return Mode_Done(&t, 0);
		}
		if (c) {
			Npc_Insert0(opts, first);
		}
	}
	Native_SetEmpty(out, NV_TUPLE);
	Native_Copy(Native_Append(out), opts);
	Native_Copy(Native_Append(out), subs);
	return Mode_Done(&t, 1);
}

/* Speech.StdAnsLst(optionsList); len(optionsList), -1 with the error set */
static int Npc_ShowOptions(void) {
	nativeVal_t opts = { 0 };
	int n = Npc_G("optionsList", &opts) && Native_CallMod("Speech", "StdAnsLst", NULL, "v", &opts) &&
		Mode_Read(NPC, "optionsList", &opts) ? Mode_Len(&opts) : -1;
	Native_Free(&opts);
	return n;
}

/* Challenge.AddChallenge(ch, UsrName, numOpts) */
static byte Npc_AddChallenge(const nativeVal_t *ch, const nativeVal_t *usr, long n) {
	return Native_CallMod("Challenge", "AddChallenge", NULL, "vvi", ch, usr, n);
}

/* PlayOption(optId, msg, ans, UsrName, ch): the talk of an option, its sub options join the list, the arrow on the
 * next option; the challenge options (without infinite lives) */
static byte Npc_PlayOption(const nativeVal_t *opt, const nativeVal_t *msg, const nativeVal_t *ans, const nativeVal_t *usr,
	const nativeVal_t *ch) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *said = Mode_V(&t), *opts = Mode_V(&t), *at = Mode_V(&t), *index = Mode_V(&t);
	if (!Mode_Import("Speech") || !Mode_Import("Challenge") || !Native_CallMod("Speech", "OnSpch", said, "vv", msg, ans)) {
		return Mode_Done(&t, 0);
	}
	int n = -1;
	if (Native_True(said)) {
		byte ok = Npc_G("optionsList", opts) && (Mode_Elems(opts), Mode_Index(opts, opt, at)) && Mode_Arith('+', at, Mode_I(&t, 1), index) &&
			Npc_IncludeSubOptions(opt) && (n = Npc_ShowOptions()) >= 0;
		int last = ok ? Mode_CmpI(index, "==", n) : -1;
		if (last < 0 || !Mode_VSet(ch, "ArriOpt", last ? Mode_I(&t, 0) : index)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Npc_SaveIs("Player.InfiniteLives", "0")) {
		return Mode_Done(&t, 1);
	}
	if (n < 0) {
		/* numOpts is only bound when the speech started */
		return Mode_Done(&t, Native_Raise(NE_NAME, "numOpts"));
	}
	return Mode_Done(&t, Npc_AddChallenge(ch, usr, n));
}

/* Speech.OnSpch(msg + type + msgEnd, ans + type + ansEnd) (msgEnd NULL: msg alone), the talk of a challenge: 1, 0, -1 */
static int Npc_ChallengeSpch(const char *msg, const nativeVal_t *type, const char *msgEnd, const char *ans, const char *ansEnd) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *m = Mode_V(&t), *a = Mode_V(&t), *r = Mode_V(&t);
	byte ok = msgEnd ? Mode_Cat(&t, m, 3, Mode_S(&t, msg), type, Mode_S(&t, msgEnd)) : (Native_SetStr(m, msg), 1);
	int said = ok && Mode_Cat(&t, a, 3, Mode_S(&t, ans), type, Mode_S(&t, ansEnd)) &&
		Native_CallMod("Speech", "OnSpch", r, "vv", m, a) ? Native_True(r) : -1;
	Mode_Done(&t, 1);
	return said;
}

/* Challenge.challengeType, foeType (Script_SetVar of the port: an error is dropped) and SetChallengeOptions(ch) */
static byte Npc_StartChallenge(const char *kind, const nativeVal_t *foe, const nativeVal_t *ch) {
	nativeVal_t k;
	Native_SetStr(&k, kind);
	LvN_SetVar("Challenge", "challengeType", &k);
	LvN_SetVar("Challenge", "foeType", foe);
	return Native_CallMod("Challenge", "SetChallengeOptions", NULL, "v", ch);
}

/* GenericNPCSpeech(UsrName, CharName, optId, FullOption): the talk with an NPC. Talk: the greeting (one of three
 * answers at random) and the options of the type; an option of the list; the challenge options; anything else
 * prints an error and starts the talk again */
static byte Npc_GenericNPCSpeech(const nativeVal_t *usr, const nativeVal_t *chname, const nativeVal_t *opt,
	const nativeVal_t *full) {
	static const char *const mods[] = { "Speech", "Chars", "Challenge", "Menu", "Challenge", "string", NULL };
	static const char *const mercs[] = { "MercenaryA", "MercenaryB", "MercenaryC", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *ch = Mode_V(&t), *e = Mode_V(&t), *type0 = Mode_V(&t), *u = Mode_V(&t), *ua = Mode_V(&t);
	nativeVal_t *m = Mode_V(&t), *a = Mode_V(&t), *r = Mode_V(&t), *key = Mode_V(&t), *special = Mode_V(&t);
	nativeVal_t *first = Mode_V(&t), *list = Mode_V(&t), *lists = Mode_V(&t), *v = Mode_V(&t), *opts = Mode_V(&t);
	Mode_V(&t);
	for (int i = 0; mods[i]; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_EntitySeam(chname, ch)) {
		return Mode_Done(&t, 0);
	}
	LvN_SetVar("Speech", "PlayerName", usr);
	LvN_SetVar("Speech", "NPCName", chname);
	if (!Mode_EntitySeam(chname, e) || !Mode_VObj(e, "ActCtrl", type0)) {
		return Mode_Done(&t, 0);
	}
	const nativeVal_t *type = Npc_In(type0, mercs) ? Mode_S(&t, "Mercs") : type0;
	if (Native_IsStr(opt, "Talk")) {
		int betty = Npc_CheckActualEntity(chname) && Mode_UsrHandle(u) ? Native_IsStr(type, "Betty") : -1;
		if (betty > 0) {
			betty = Native_True(u);
		}
		if (betty > 0) {
			betty = Mode_VObj(u, "ActCtrl", ua) ? !Native_IsStr(ua, "Dtritus") : -1;
		}
		if (betty > 0) {
			return Mode_Done(&t, Mode_Spch("Generics_Betty_InitMsg2", "Generics_Betty_InitAns4", NULL));
		}
		if (betty < 0) {
			return Mode_Done(&t, 0);
		}
		char num[16];
		Str_Printf(num, sizeof(num), "%d", (int)Scrap_RandRange(1.0, 3.9));
		int said = Mode_Cat(&t, m, 3, Mode_S(&t, "Generics_"), type, Mode_S(&t, "_InitMsg1")) &&
			Mode_Cat(&t, a, 4, Mode_S(&t, "Generics_"), type, Mode_S(&t, "_InitAns"), Mode_S(&t, num)) &&
			Native_CallMod("Speech", "OnSpch", r, "vv", m, a) ? Native_True(r) : -1;
		if (said <= 0) {
			return Mode_Done(&t, said == 0);
		}
		int n = 0;
		if (Npc_SaveIs("GameAct", "Epilogue")) {
			if (Native_IsStr(type, "Bishop") || Native_IsStr(type, "Mercs")) {
				n = Mode_Cat(&t, key, 4, Mode_S(&t, "Generics_Special"), type, Mode_S(&t, "_"), Mode_S(&t, "1Opt")) &&
					Native_CallMod("Scrap", "GetLangEntries", special, "v", key) && Mode_Item(special, 0, first) ? 0 : -1;
				if (!n) {
					Native_SetEmpty(list, NV_LIST);
					Native_Copy(Native_Append(list), first);
					n = Mode_Put(NPC, "optionsList", NULL, list) && Npc_ShowOptions() >= 0 ? 1 : -1;
				}
			}
		} else {
			n = Npc_ClassifyOptions(type, lists) && Mode_Unpack(lists, 2, v) && Mode_Put(NPC, "optionsList", NULL, &v[0]) &&
				Mode_Put(NPC, "subOptionsList", NULL, &v[1]) ? Npc_ShowOptions() : -1;
		}
		if (n < 0) {
			return Mode_Done(&t, 0);
		}
		if (Npc_SaveIs("Player.InfiniteLives", "0") && (!Native_IsStr(type, "Mercs") || !Npc_MercsHired())) {
			return Mode_Done(&t, Npc_AddChallenge(ch, usr, n));
		}
		return Mode_Done(&t, 1);
	}
	int listed = Npc_G("optionsList", opts) ? Mode_In(opts, opt) : -1;
	if (listed < 0) {
		return Mode_Done(&t, 0);
	}
	if (listed) {
		return Mode_Done(&t, Mode_Cat(&t, m, 2, opt, Mode_S(&t, "Msg")) && Mode_Cat(&t, a, 2, opt, Mode_S(&t, "Ans")) &&
			Npc_PlayOption(opt, m, a, usr, ch));
	}
	if (Native_IsStr(opt, "ChallengeRaceStdOpt0") || Native_IsStr(opt, "ChallengeCombatStdOpt0")) {
		byte race = Native_IsStr(opt, "ChallengeRaceStdOpt0");
		const char *pre = race ? "ChallengeRace" : "ChallengeCombat";
		int said = Npc_ChallengeSpch(pre, type, "Opt1", pre, "Msg1");
		return Mode_Done(&t, said >= 0 && (!said || Npc_StartChallenge(race ? "Race" : "Combat", type0, ch)));
	}
	if (Native_IsStr(opt, "ChallengeRaceStdOpt1") || Native_IsStr(opt, "ChallengeRaceStdOpt2") ||
		Native_IsStr(opt, "ChallengeRaceStdOpt3")) {
		return Mode_Done(&t, Native_CallMod("Challenge", "OnChallengeOpt", NULL, "iv", (long)(Native_Text(opt)[19] - '0'), ch));
	}
	if (Native_IsStr(opt, "ChallengeRaceStdOpt4")) {
		int said = Npc_ChallengeSpch("ChallengeRaceStdOpt2", type, NULL, "ChallengeRace", "Msg3");
		int n = said > 0 ? Npc_ShowOptions() : 0;
		return Mode_Done(&t, said >= 0 && n >= 0 && (!said || Npc_AddChallenge(ch, usr, n)));
	}
	const nativeVal_t *items[6] = { Mode_S(&t, "Error en los argumentos:  ("), usr, chname, opt, full, Mode_S(&t, ")") };
	return Mode_Done(&t, Mode_Print(items, 6) && Npc_GenericNPCSpeech(usr, chname, Mode_S(&t, "Talk"), full));
}

/* ---- the functions of the module ---- */

/* the functions with an optional second argument (the formats of the port: no keywords) */
#define NPC_FN2(name, call) \
	static PyObject *CharsNPC_##name(PyObject *self, PyObject *args) { \
		PyObject *o, *o2 = NULL; \
		modeVals_t t = { .n = 0 }; \
		nativeVal_t *a = Mode_V(&t), *a2 = Mode_V(&t), *out = Mode_V(&t); \
		(void)self; \
		if (!PyArg_ParseTuple(args, "O|O", &o, &o2)) { \
			return Mode_Done(&t, 0), NULL; \
		} \
		Native_Arg(o, a); \
		if (o2) { \
			Native_Arg(o2, a2); \
		} \
		return Mode_Out(&t, call(a, o2 ? a2 : NULL, out), out); \
	}

/* the functions the port ran without parsing their arguments (any arguments) */
#define NPC_ANY(name, call) \
	static PyObject *CharsNPC_##name(PyObject *self, PyObject *args) { \
		modeVals_t t = { .n = 0 }; \
		nativeVal_t *out = Mode_V(&t); \
		(void)self; \
		(void)args; \
		return Mode_Out(&t, call, out); \
	}

/* an int result into out */
static byte Npc_IntOut(long i, nativeVal_t *out) {
	Native_SetInt(out, i);
	return 1;
}

NPC_FN2(AddNPCPos, Npc_AddNPCPos)
NPC_FN2(CreateChar, Npc_CreateChar)
MODE_FN(CharsNPC, CreateMissionChar, 1, Npc_CreateMissionChar(&a[0], out))
MODE_FN(CharsNPC, DeleteMissionChar, 1, Npc_DeleteMissionChar(&a[0]))
MODE_FN(CharsNPC, SetReposChars, 2, Npc_SetReposChars(&a[0], &a[1]))
NPC_ANY(CreateReposChars, Npc_CreateReposChars())
NPC_ANY(BrowseNPC, Npc_BrowseNPC())
MODE_FN(CharsNPC, AssignAttrib, 1, Npc_AssignAttrib(&a[0]))
MODE_FN(CharsNPC, CallCreateChar, 4, Npc_CallCreateChar(&a[0], &a[1], &a[2], &a[3]))
MODE_FN(CharsNPC, CharNPCInclude, 1, Npc_CharNPCInclude(&a[0]))
MODE_FN(CharsNPC, Respawn, 1, Npc_Respawn(&a[0]))
MODE_FN(CharsNPC, ActiveSpawned, 1, Npc_ActiveSpawned(&a[0]))
NPC_ANY(Init, Npc_Init())
MODE_FN(CharsNPC, CheckActualEntity, 1, Npc_CheckActualEntity(&a[0]))
NPC_ANY(MercsHired, Npc_IntOut(Npc_MercsHired(), out))
MODE_FN(CharsNPC, IncludeSubOptions, 1, Npc_IncludeSubOptions(&a[0]))
MODE_FN(CharsNPC, Classify, 2, Npc_Classify(&a[0], &a[1], out))
MODE_FN(CharsNPC, ClassifyOptions, 1, Npc_ClassifyOptions(&a[0], out))
MODE_FN(CharsNPC, PlayOption, 5, Npc_PlayOption(&a[0], &a[1], &a[2], &a[3], &a[4]))
MODE_FN(CharsNPC, GenericNPCSpeech, 4, Npc_GenericNPCSpeech(&a[0], &a[1], &a[2], &a[3]))

PyMethodDef g_CharsNPCMethods[] = {
	{ "AddNPCPos", CharsNPC_AddNPCPos, 1, NULL },
	{ "CreateMissionChar", CharsNPC_CreateMissionChar, 1, NULL },
	{ "DeleteMissionChar", CharsNPC_DeleteMissionChar, 1, NULL },
	{ "SetReposChars", CharsNPC_SetReposChars, 1, NULL },
	{ "CreateReposChars", CharsNPC_CreateReposChars, 1, NULL },
	{ "BrowseNPC", CharsNPC_BrowseNPC, 1, NULL },
	{ "AssignAttrib", CharsNPC_AssignAttrib, 1, NULL },
	{ "CallCreateChar", CharsNPC_CallCreateChar, 1, NULL },
	{ "CreateChar", CharsNPC_CreateChar, 1, NULL },
	{ "CharNPCInclude", CharsNPC_CharNPCInclude, 1, NULL },
	{ "Respawn", CharsNPC_Respawn, 1, NULL },
	{ "ActiveSpawned", CharsNPC_ActiveSpawned, 1, NULL },
	{ "Init", CharsNPC_Init, 1, NULL },
	{ "CheckActualEntity", CharsNPC_CheckActualEntity, 1, NULL },
	{ "MercsHired", CharsNPC_MercsHired, 1, NULL },
	{ "IncludeSubOptions", CharsNPC_IncludeSubOptions, 1, NULL },
	{ "Classify", CharsNPC_Classify, 1, NULL },
	{ "ClassifyOptions", CharsNPC_ClassifyOptions, 1, NULL },
	{ "PlayOption", CharsNPC_PlayOption, 1, NULL },
	{ "GenericNPCSpeech", CharsNPC_GenericNPCSpeech, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
