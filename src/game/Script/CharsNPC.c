/*
 * The CharsNPC module in C (scripts/chars/charsnpc.py, issue #2 step 7d): the NPCs of the indoor levels. The spawn
 * points of the level (DM_NPC_Spawn_*, DM_NPC_Repos_*, DM_Mission_*) become CharSpawnPos objects in CharRes, Init
 * creates the characters from them, and a dead NPC comes back at a free conversion point. The second half is the
 * generic talk of the NPCs: the option lists from the language file (ClassifyOptions) and GenericNPCSpeech.
 *
 * The module variables stay in the module (CharRes, the class CharSpawnPos, AfterBrowseNPC, ReposChars, NPCAttribs,
 * RespawnChars, Repos, optionsList, subOptionsList, ActualEntityName), because the missions and the character
 * scripts read and change them. The code reads them as globals of the module like the script (a missing one is a
 * NameError), works on the entity handles and the CharSpawnPos objects, and stops at the first error.
 */
#include <ctype.h>
#include <limits.h>
#include <string.h>

#include "scrap/global.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

/* the temporary references of a function, released together by Npc_Done */
#define NPC_REFS 48
typedef struct npcRefs_s {
	PyObject *o[NPC_REFS];
	int n;
} npcRefs_t;

/* o into the references of r (NULL passes) */
static PyObject *Npc_Keep(npcRefs_t *r, PyObject *o) {
	if (o) {
		CHECK(r->n < NPC_REFS);
		r->o[r->n++] = o;
	}
	return o;
}

/* releases the references of r, then result */
static PyObject *Npc_Done(npcRefs_t *r, PyObject *result) {
	while (r->n > 0) {
		r->n--;
		Py_DECREF(r->o[r->n]);
	}
	return result;
}

/* a new reference of o */
static PyObject *Npc_Ref(PyObject *o) {
	Py_XINCREF(o);
	return o;
}

/* a global of the module as the function of the script reads it (a new reference; a NameError when missing) */
static PyObject *Npc_Global(const char *name) {
	PyObject *m = PyImport_ImportModule("CharsNPC");
	PyObject *v = m ? PyDict_GetItemString(PyModule_GetDict(m), (char *)name) : NULL;
	if (m && !v) {
		PyErr_SetString(PyExc_NameError, (char *)name);
	}
	Py_XINCREF(v);
	Py_XDECREF(m);
	return v;
}

/* `global name; name = v` (v given away): 0 with the error set */
static byte Npc_SetGlobal(const char *name, PyObject *v) {
	PyObject *m = v ? PyImport_ImportModule("CharsNPC") : NULL;
	byte ok = m && PyDict_SetItemString(PyModule_GetDict(m), (char *)name, v) == 0;
	Py_XDECREF(m);
	Py_XDECREF(v);
	return ok;
}

/* the import line of a function (the names end with NULL): 0 with the error set */
static byte Npc_Import(const char *const *names) {
	for (; *names; names++) {
		PyObject *m = PyImport_ImportModule((char *)*names);
		if (!m) {
			return 0;
		}
		Py_DECREF(m);
	}
	return 1;
}

/* o[i] as a script subscripts (a new reference) */
static PyObject *Npc_Item(PyObject *o, int i) {
	PyObject *k = o ? PyInt_FromLong(i) : NULL;
	PyObject *v = k ? PyObject_GetItem(o, k) : NULL;
	Py_XDECREF(k);
	return v;
}

/* a + b as a script adds (a given away; a new reference, NULL stays NULL) */
static PyObject *Npc_Add(PyObject *a, PyObject *b) {
	PyObject *r = a && b ? PyNumber_Add(a, b) : NULL;
	Py_XDECREF(a);
	return r;
}

/* a + "text" (a given away) */
static PyObject *Npc_AddS(PyObject *a, const char *text) {
	PyObject *t = a ? PyString_FromString(text) : NULL;
	PyObject *r = Npc_Add(a, t);
	Py_XDECREF(t);
	return r;
}

/* "text" + b */
static PyObject *Npc_SAdd(const char *text, PyObject *b) {
	return Npc_Add(PyString_FromString(text), b);
}

/* o == "text" in a script (a string equals only a string) */
static byte Npc_Is(PyObject *o, const char *text) {
	return o && PyString_Check(o) && !strcmp(PyString_AsString(o), text);
}

/* o in ("text", ...) (the texts end with NULL) */
static byte Npc_In(PyObject *o, const char *const *texts) {
	for (; *texts; texts++) {
		if (Npc_Is(o, *texts)) {
			return 1;
		}
	}
	return 0;
}

/* `if o:` of a script: 1, 0, or -1 with the error set (also for NULL) */
static int Npc_True(PyObject *o) {
	return o ? PyObject_IsTrue(o) : -1;
}

/* the print statement with its items, as ceval prints them (the soft space between them): 0 with the error set */
static byte Npc_Print(PyObject *const *items, int n) {
	PyObject *w = PySys_GetObject("stdout");
	if (!w) {
		PyErr_SetString(PyExc_RuntimeError, "lost sys.stdout");
		return 0;
	}
	for (int i = 0; i < n; i++) {
		if ((PyFile_SoftSpace(w, 1) && PyFile_WriteString(" ", w) != 0) || PyFile_WriteObject(items[i], w, Py_PRINT_RAW) != 0) {
			return 0;
		}
		if (PyString_Check(items[i])) {
			const char *s = PyString_AsString(items[i]);
			int len = PyString_Size(items[i]);
			if (len > 0 && isspace((unsigned char)s[len - 1]) && s[len - 1] != ' ') {
				PyFile_SoftSpace(w, 0);
			}
		}
	}
	if (PyFile_WriteString("\n", w) != 0) {
		return 0;
	}
	PyFile_SoftSpace(w, 0);
	return 1;
}

/* a, b, ... = v of a script (UNPACK_TUPLE of ceval): n new references into out, 0 with the error set */
static byte Npc_Unpack(PyObject *v, int n, PyObject **out) {
	if (!PySequence_Check(v)) {
		PyErr_SetString(PyExc_TypeError, "unpack non-sequence");
		return 0;
	}
	if (PySequence_Length(v) != n) {
		PyErr_SetString(PyExc_ValueError, PyTuple_Check(v) ? "unpack tuple of wrong size" :
			PyList_Check(v) ? "unpack list of wrong size" : "unpack sequence of wrong size");
		return 0;
	}
	for (int i = 0; i < n; i++) {
		out[i] = PySequence_GetItem(v, i);
		if (!out[i]) {
			while (i-- > 0) {
				Py_DECREF(out[i]);
			}
			return 0;
		}
	}
	return 1;
}

/* Scrap.GetLangStr(key) (key given away) */
static PyObject *Npc_Lang(PyObject *key) {
	PyObject *s = key ? Script_Ask("Scrap", "GetLangStr", "(O)", key) : NULL;
	Py_XDECREF(key);
	return s;
}

/* Scrap.GetSaveVar(name) == "0" */
static byte Npc_SaveIs(const char *name, const char *text) {
	const char *v = SaveVar_Get(name);
	return v && !strcmp(v, text);
}

/* Scrap.ConsoleError(text) (text given away): 0 with the error set */
static byte Npc_ConsoleError(PyObject *text) {
	byte ok = text && Script_Try("Scrap", "ConsoleError", "(O)", text);
	Py_XDECREF(text);
	return ok;
}

/* ---- the spawn points ---- */

/* AddNPCPos(dummy, IsMissionChar=0): a CharSpawnPos from the dummy (name, pos, ang, flag) into CharRes; a name
 * already there gets "_R" (a new reference) */
static PyObject *Npc_AddNPCPos(PyObject *dummy, PyObject *mission) {
	static const char *const mods[] = { "string", "Chars", NULL };
	npcRefs_t r = { 0 };
	if (!Npc_Import(mods)) {
		return NULL;
	}
	PyObject *flag = Npc_Keep(&r, mission ? Npc_Ref(mission) : PyInt_FromLong(0));
	int m = Npc_True(flag);
	if (m < 0) {
		return Npc_Done(&r, NULL);
	}
	int starts = m ? 11 : 13, typeid = m ? 2 : 3;
	PyObject *d0 = Npc_Keep(&r, Npc_Item(dummy, 0));
	PyObject *nam = d0 ? Npc_Keep(&r, Script_Ask("string", "split", "(Os)", d0, "_")) : NULL;
	PyObject *act = nam ? Npc_Keep(&r, Npc_Item(nam, typeid)) : NULL;
	PyObject *cls = act && Script_Try("Chars", "AddNewCharRes", "(OO)", act, flag) ? Npc_Keep(&r, Npc_Global("CharSpawnPos")) : NULL;
	PyObject *csp = cls ? Npc_Keep(&r, Script_Invoke(cls, PyTuple_New(0))) : NULL;
	byte ok = csp && Script_HSet(csp, "Pos", Npc_Item(dummy, 1)) &&
		Script_HSet(csp, "Name", PySequence_GetSlice(d0, starts, INT_MAX));
	PyObject *d2 = ok ? Npc_Keep(&r, Npc_Item(dummy, 2)) : NULL;
	PyObject *y = d2 ? Npc_Keep(&r, Npc_Item(d2, 1)) : NULL;
	PyObject *turn = y ? Npc_Keep(&r, PyInt_FromLong(180)) : NULL;
	PyObject *yt = turn ? Npc_Keep(&r, PyNumber_Add(y, turn)) : NULL;
	ok = yt && Script_HSet(csp, "Ang", Py_BuildValue("(iOi)", 0, yt, 0)) && Script_HSet(csp, "ActCtrl", Npc_Ref(act));
	PyObject *res = ok ? Npc_Keep(&r, Npc_Global("CharRes")) : NULL;
	PyObject *name = res ? Npc_Keep(&r, PyObject_GetAttrString(csp, "Name")) : NULL;
	int dup = name ? Npc_True(Npc_Keep(&r, Script_Method(res, "has_key", "(O)", name))) : -1;
	if (dup > 0) {
		dup = Npc_ConsoleError(Npc_AddS(Npc_Add(Npc_AddS(Npc_Ref(name), "  char duplicated, using "), name), "_R\n")) &&
			Script_HSet(csp, "Name", Npc_AddS(Npc_Ref(name), "_R")) ? 1 : -1;
	}
	PyObject *key = dup >= 0 ? Npc_Keep(&r, PyObject_GetAttrString(csp, "Name")) : NULL;
	if (!key || PyObject_SetItem(res, key, csp) != 0) {
		return Npc_Done(&r, NULL);
	}
	return Npc_Done(&r, Npc_Ref(csp));
}

static PyObject *CharsNPC_AddNPCPos(PyObject *self, PyObject *args) {
	PyObject *dummy, *mission = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &dummy, &mission)) {
		return NULL;
	}
	return Npc_AddNPCPos(dummy, mission);
}

/* CreateMissionChar(Name): the spawn point of a mission character from its dummy DM_Mission_<Name> */
static PyObject *CharsNPC_CreateMissionChar(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *pat = Npc_SAdd("DM_Mission_", name);
	byte ok = pat && Script_Try("Scrap", "StartDummySearch", "(Oi)", pat, 0);
	Py_XDECREF(pat);
	PyObject *dummy = ok ? Script_Ask("Scrap", "NextDummySearch", "()") : NULL;
	int found = Npc_True(dummy);
	PyObject *r = NULL;
	if (found > 0) {
		PyObject *one = PyInt_FromLong(1);
		r = one ? Npc_AddNPCPos(dummy, one) : NULL;
		Py_XDECREF(one);
	} else if (found == 0 && Npc_ConsoleError(Npc_AddS(Npc_SAdd("TWISTED ERROR : DM_Mission_", name), " Dummy not found\n"))) {
		r = Py_ReturnNone();
	}
	Py_XDECREF(dummy);
	return r;
}

/* DeleteMissionChar(Name): the entity goes, and its spawn point */
static PyObject *CharsNPC_DeleteMissionChar(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *e = Script_Ask("Scrap", "GetEntity", "(O)", name);
	int there = Npc_True(e);
	PyObject *del = there > 0 ? Script_Method(e, "Del", "()") : NULL;
	Py_XDECREF(e);
	if (there < 0 || (there > 0 && !del)) {
		return NULL;
	}
	Py_XDECREF(del);
	PyObject *res = Npc_Global("CharRes");
	byte ok = res && PyObject_DelItem(res, name) == 0;
	Py_XDECREF(res);
	return ok ? Py_ReturnNone() : NULL;
}

/* SetReposChars(CharType, Num): how many repositioned characters of a type the level has */
static PyObject *CharsNPC_SetReposChars(PyObject *self, PyObject *args) {
	PyObject *type, *num;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &type, &num)) {
		return NULL;
	}
	PyObject *repos = Npc_Global("ReposChars");
	byte ok = repos && PyObject_SetItem(repos, type, num) == 0;
	Py_XDECREF(repos);
	return ok ? Py_ReturnNone() : NULL;
}

/* the next DM_NPC_Repos_ dummy as a repositioned spawn point; a police also gets its Gear (the name with "Gear" in
 * place of characters 13 to 18): 0 with the error set */
static byte Npc_ReposOne(byte police) {
	npcRefs_t r = { 0 };
	PyObject *dummy = Npc_Keep(&r, Script_Ask("Scrap", "NextDummySearch", "()"));
	PyObject *csp = dummy ? Npc_Keep(&r, Npc_AddNPCPos(dummy, NULL)) : NULL;
	byte ok = csp && Script_HSet(csp, "repos", PyInt_FromLong(1));
	if (ok && police) {
		PyObject *d0 = Npc_Keep(&r, Npc_Item(dummy, 0));
		PyObject *gear = d0 ? Npc_Keep(&r, Npc_AddS(PySequence_GetSlice(d0, 0, 13), "Gear")) : NULL;
		PyObject *tail = gear ? Npc_Keep(&r, PySequence_GetSlice(d0, 19, INT_MAX)) : NULL;
		PyObject *name = tail ? Npc_Keep(&r, Npc_Add(Npc_Ref(gear), tail)) : NULL;
		PyObject *d1 = name ? Npc_Keep(&r, Npc_Item(dummy, 1)) : NULL;
		PyObject *d2 = d1 ? Npc_Keep(&r, Npc_Item(dummy, 2)) : NULL;
		PyObject *d3 = d2 ? Npc_Keep(&r, Npc_Item(dummy, 3)) : NULL;
		PyObject *dummy2 = d3 ? Npc_Keep(&r, Py_BuildValue("(OOOO)", name, d1, d2, d3)) : NULL;
		PyObject *csp2 = dummy2 ? Npc_Keep(&r, Npc_AddNPCPos(dummy2, NULL)) : NULL;
		ok = csp2 && Script_HSet(csp2, "repos", PyInt_FromLong(1));
	}
	Npc_Done(&r, NULL);
	return ok;
}

/* CreateReposChars(): the repositioned spawn points of the types in ReposChars, then `del ReposChars` */
static byte Npc_CreateReposChars(void) {
	npcRefs_t r = { 0 };
	PyObject *repos = Npc_Keep(&r, Npc_Global("ReposChars"));
	PyObject *keys = repos ? Npc_Keep(&r, Script_Method(repos, "keys", "()")) : NULL;
	byte ok = keys != NULL;
	for (int i = 0; ok && i < PySequence_Length(keys); i++) {
		npcRefs_t t = { 0 };
		PyObject *type = Npc_Keep(&t, PySequence_GetItem(keys, i));
		PyObject *num = type ? Npc_Keep(&t, PyObject_GetItem(repos, type)) : NULL;
		byte police = Npc_Is(type, "Police");
		PyObject *pat = num ? Npc_Keep(&t, Npc_AddS(Npc_SAdd("DM_NPC_Repos_", type), "_*")) : NULL;
		PyObject *range = pat && Script_Try("Scrap", "StartDummySearch", "(Oi)", pat, 1) ?
			Npc_Keep(&t, Script_Ask("__builtin__", "range", "(O)", num)) : NULL;
		ok = range != NULL;
		for (int j = 0; ok && j < PySequence_Length(range); j++) {
			ok = Npc_ReposOne(police);
		}
		Npc_Done(&t, NULL);
	}
	if (ok) {
		PyObject *m = PyImport_ImportModule("CharsNPC");
		ok = m && PyDict_DelItemString(PyModule_GetDict(m), "ReposChars") == 0;
		if (m && !ok) {
			PyErr_SetString(PyExc_NameError, "ReposChars");
		}
		Py_XDECREF(m);
	}
	Npc_Done(&r, NULL);
	return ok;
}

static PyObject *CharsNPC_CreateReposChars(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Npc_CreateReposChars() ? Py_ReturnNone() : NULL;
}

/* BrowseNPC(): the spawn points of the level (the DM_NPC_Spawn_ dummies with their flag set), then the hooks the
 * missions put into AfterBrowseNPC */
static PyObject *CharsNPC_BrowseNPC(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	if (!Script_Try("DTritusDesktop", "InitNPCS", "()") || !Npc_CreateReposChars() ||
		!Script_Try("Scrap", "StartDummySearch", "(si)", "DM_NPC_Spawn_*", 1)) {
		return NULL;
	}
	for (;;) {
		PyObject *dummy = Script_Ask("Scrap", "NextDummySearch", "()");
		if (!dummy) {
			return NULL;
		}
		if (dummy == Py_None) {
			Py_DECREF(dummy);
			break;
		}
		PyObject *flag = Npc_Item(dummy, 3);
		int add = Npc_True(flag);
		PyObject *csp = add > 0 ? Npc_AddNPCPos(dummy, NULL) : NULL;
		Py_XDECREF(flag);
		Py_DECREF(dummy);
		if (add < 0 || (add > 0 && !csp)) {
			return NULL;
		}
		Py_XDECREF(csp);
	}
	PyObject *after = Npc_Global("AfterBrowseNPC");
	byte ok = after != NULL;
	/* the list may grow while the hooks run: the length of each turn, as the for loop of the script */
	for (int i = 0; ok && i < PySequence_Length(after); i++) {
		PyObject *f = PySequence_GetItem(after, i);
		PyObject *v = f ? Script_Invoke(f, PyTuple_New(0)) : NULL;
		ok = v != NULL;
		Py_XDECREF(v);
		Py_XDECREF(f);
	}
	Py_XDECREF(after);
	return ok ? Py_ReturnNone() : NULL;
}

/* AssignAttrib(c): money, reward and life of the type from NPCAttribs (else 50, 5, 100), PrevLife as Life */
static byte Npc_AssignAttrib(PyObject *c) {
	npcRefs_t r = { 0 };
	PyObject *act = Npc_Keep(&r, PyObject_GetAttrString(c, "ActCtrl"));
	PyObject *attribs = act ? Npc_Keep(&r, Npc_Global("NPCAttribs")) : NULL;
	int has = attribs ? Npc_True(Npc_Keep(&r, Script_Method(attribs, "has_key", "(O)", act))) : -1;
	PyObject *vals = has > 0 ? Npc_Keep(&r, PyObject_GetItem(attribs, act)) : has == 0 ? Npc_Keep(&r, Py_BuildValue("(iii)", 50, 5, 100)) : NULL;
	PyObject *v[3];
	byte ok = vals && Npc_Unpack(vals, 3, v);
	if (ok) {
		/* each set takes its value over; the values after a failed one are released */
		static const char *const vars[3] = { "Money", "MoneyReward", "Life" };
		for (int i = 0; i < 3; i++) {
			if (ok) {
				ok = Script_HSet(c, vars[i], v[i]);
			} else {
				Py_DECREF(v[i]);
			}
		}
		ok = ok && Script_HSet(c, "PrevLife", PyObject_GetAttrString(c, "Life"));
	}
	Npc_Done(&r, NULL);
	return ok;
}

static PyObject *CharsNPC_AssignAttrib(PyObject *self, PyObject *args) {
	PyObject *c;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &c)) {
		return NULL;
	}
	return Npc_AssignAttrib(c) ? Py_ReturnNone() : NULL;
}

/* CharNPCInclude(chrname): the AI and the entity list of the type */
static byte Npc_CharNPCInclude(PyObject *name) {
	static const char *const gate[] = { "GateKeeper", "Sebastian", NULL };
	static const char *const gen[] = { "Mayor", "BankDirector", "BankMaster", "Desktop", "Berto", "Betty", "Boss",
		"Humphrey", "Bishop", "Functionary", "Rusty", "SputnikJunkyard", "Maintenance", "MercenaryA", "MercenaryB",
		"MercenaryC", "MercenaryL", "CrazyGambler", "PoliceBoss", "Messenger", "Human", NULL };
	npcRefs_t r = { 0 };
	PyObject *res = Npc_Keep(&r, Npc_Global("CharRes"));
	PyObject *ctr = res ? Npc_Keep(&r, PyObject_GetItem(res, name)) : NULL;
	PyObject *act = ctr ? Npc_Keep(&r, PyObject_GetAttrString(ctr, "ActCtrl")) : NULL;
	if (!act) {
		Npc_Done(&r, NULL);
		return 0;
	}
	PyObject *list = NULL;
	byte ai = 0;
	if (Npc_Is(act, "Nurse")) {
		ai = 1;
		list = PyString_FromString("Nurse");
	} else if (Npc_In(act, gate)) {
		list = PyString_FromString("GateKeeper");
	} else if (Npc_In(act, gen)) {
		ai = 1;
		list = Npc_Is(act, "Bishop") || Npc_Is(act, "Messenger") ? Npc_Ref(act) : PyString_FromString("GenChars");
	}
	byte ok = 1;
	if (list) {
		PyObject *ch = Npc_Keep(&r, Script_Ask("Scrap", "GetEntity", "(O)", name));
		ok = ch && (!ai || Script_Try("SAI", "EnableAIChar", "(Oii)", name, 1, 0)) && Script_HSet(ch, "EntityList", Npc_Ref(list));
		Py_DECREF(list);
	}
	Npc_Done(&r, NULL);
	return ok;
}

static PyObject *CharsNPC_CharNPCInclude(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	return Npc_CharNPCInclude(name) ? Py_ReturnNone() : NULL;
}

/* CreateChar(csp, Fixed=0): the character of a spawn point, a WalkChar that talks through Speech.AutoSpeech;
 * standing on the floor, or fixed where it is; the callbacks of the spawn point get it (a new reference) */
static PyObject *Npc_CreateChar(PyObject *csp, PyObject *fixed) {
	static const char *const mods[] = { "Chars", "InPolice", NULL };
	npcRefs_t r = { 0 };
	if (!Npc_Import(mods)) {
		return NULL;
	}
	PyObject *name = Npc_Keep(&r, PyObject_GetAttrString(csp, "Name"));
	PyObject *pos = name ? Npc_Keep(&r, PyObject_GetAttrString(csp, "Pos")) : NULL;
	PyObject *x = pos ? Npc_Keep(&r, Npc_Item(pos, 0)) : NULL;
	PyObject *y = x ? Npc_Keep(&r, Npc_Item(pos, 1)) : NULL;
	PyObject *z = y ? Npc_Keep(&r, Npc_Item(pos, 2)) : NULL;
	PyObject *c = z ? Npc_Keep(&r, Script_Ask("Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, "WalkChar")) : NULL;
	PyObject *cname = c ? Npc_Keep(&r, PyObject_GetAttrString(c, "Name")) : NULL;
	if (!cname) {
		return Npc_Done(&r, NULL);
	}
	int diff = PyObject_Compare(name, cname);
	if (PyErr_Occurred()) {
		return Npc_Done(&r, NULL);
	}
	if (diff) {
		PyObject *del = Npc_ConsoleError(Npc_SAdd("Imposible crear el personaje ", name)) ? Script_Method(c, "Del", "()") : NULL;
		if (!del) {
			return Npc_Done(&r, NULL);
		}
		Py_DECREF(del);
	}
	PyObject *act = Npc_Keep(&r, PyObject_GetAttrString(csp, "ActCtrl"));
	byte ok = act && Script_HSet(c, "ActCtrl", Npc_Ref(act)) && Script_HSet(c, "Ang", PyObject_GetAttrString(csp, "Ang")) &&
		Script_HSet(c, "ArrowCanUse", PyInt_FromLong(1)) && Script_HSet(c, "ArrowCanView", PyInt_FromLong(1)) &&
		Script_HSet(c, "UseFunc", PyString_FromString("Speech.AutoSpeech")) &&
		Script_HSet(c, "ViewFunc", PyString_FromString("Speech.InspectChar"));
	PyObject *act2 = ok ? Npc_Keep(&r, PyObject_GetAttrString(csp, "ActCtrl")) : NULL;
	ok = act2 && Script_HSet(c, "Descriptor", Npc_Lang(Npc_SAdd("CharName_", act2))) &&
		Script_HSet(c, "ArrnOpt", PyInt_FromLong(0)) &&
		Script_HSet(c, "ArrOpt0", Npc_SAdd("Talk:", Npc_Keep(&r, Script_Ask("Scrap", "GetLangStr", "(s)", "TalkOpt"))));
	int fix = ok ? (fixed ? Npc_True(fixed) : 0) : -1;
	if (fix > 0) {
		ok = Script_HSet(c, "MovePos", PyObject_GetAttrString(c, "Pos")) && Script_HSet(c, "MoveAng", PyObject_GetAttrString(c, "Ang")) &&
			Script_HSet(c, "MoveTime", PyFloat_FromDouble(0.001)) && Script_HSet(c, "MoveViewAng", PyInt_FromLong(0));
	} else if (fix == 0) {
		PyObject *n = Npc_Keep(&r, PyObject_GetAttrString(c, "Name"));
		ok = n && Script_Try("SLogic", "SetOnFloor", "(O)", n);
	} else {
		ok = 0;
	}
	PyObject *n1 = ok && Npc_AssignAttrib(c) ? Npc_Keep(&r, PyObject_GetAttrString(c, "Name")) : NULL;
	PyObject *n2 = n1 && Script_Try("InPolice", "Include", "(O)", n1) ? Npc_Keep(&r, PyObject_GetAttrString(c, "Name")) : NULL;
	PyObject *repos = n2 && Npc_CharNPCInclude(n2) ? Npc_Keep(&r, PyObject_GetAttrString(csp, "repos")) : NULL;
	int rep = Npc_True(repos);
	ok = rep >= 0 && (!rep || Script_HSet(c, "AI_Repos", Npc_Global("Repos")));
	PyObject *funcs = ok ? Npc_Keep(&r, PyObject_GetAttrString(csp, "CallbackFunc")) : NULL;
	ok = funcs != NULL;
	for (int i = 0; ok && i < PySequence_Length(funcs); i++) {
		PyObject *f = PySequence_GetItem(funcs, i);
		PyObject *v = f ? Script_Invoke(f, Py_BuildValue("(O)", c)) : NULL;
		ok = v != NULL;
		Py_XDECREF(v);
		Py_XDECREF(f);
	}
	return Npc_Done(&r, ok ? Npc_Ref(c) : NULL);
}

static PyObject *CharsNPC_CreateChar(PyObject *self, PyObject *args) {
	PyObject *csp, *fixed = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "O|O", &csp, &fixed)) {
		return NULL;
	}
	return Npc_CreateChar(csp, fixed);
}

/* CallCreateChar(EntityName, x, y, z): the character of a spawn point at another place */
static PyObject *CharsNPC_CallCreateChar(PyObject *self, PyObject *args) {
	PyObject *name, *x, *y, *z;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &name, &x, &y, &z)) {
		return NULL;
	}
	PyObject *res = Npc_Global("CharRes");
	PyObject *csp = res ? PyObject_GetItem(res, name) : NULL;
	PyObject *c = csp && Script_HSet(csp, "Pos", Py_BuildValue("(OOO)", x, y, z)) ? Npc_CreateChar(csp, NULL) : NULL;
	Py_XDECREF(res);
	Py_XDECREF(csp);
	if (!c) {
		return NULL;
	}
	Py_DECREF(c);
	return Py_ReturnNone();
}

/* Init(): the characters of all spawn points (not the Gear of a repositioned police; gatekeepers, Sputnik and
 * Sebastian fixed), then Repos = 1 */
static PyObject *CharsNPC_Init(PyObject *self, PyObject *args) {
	static const char *const fixedTypes[] = { "GateKeeper", "Sputnik", "Sebastian", NULL };
	(void)self;
	(void)args;
	PyObject *res = Npc_Global("CharRes");
	PyObject *keys = res ? Script_Method(res, "keys", "()") : NULL;
	byte ok = keys != NULL;
	for (int i = 0; ok && i < PySequence_Length(keys); i++) {
		npcRefs_t t = { 0 };
		PyObject *k = Npc_Keep(&t, PySequence_GetItem(keys, i));
		PyObject *cls = k ? Npc_Keep(&t, PyObject_GetItem(res, k)) : NULL;
		int skip = cls ? Npc_True(Npc_Keep(&t, PyObject_GetAttrString(cls, "repos"))) : -1;
		if (skip > 0) {
			PyObject *act = Npc_Keep(&t, PyObject_GetAttrString(cls, "ActCtrl"));
			skip = act ? Npc_Is(act, "Gear") : -1;
		}
		if (skip == 0) {
			PyObject *act = Npc_Keep(&t, PyObject_GetAttrString(cls, "ActCtrl"));
			PyObject *fixed = act ? Npc_Keep(&t, PyInt_FromLong(Npc_In(act, fixedTypes))) : NULL;
			skip = fixed && Npc_Keep(&t, Npc_CreateChar(cls, fixed)) ? 1 : -1;
		}
		ok = skip > 0;
		Npc_Done(&t, NULL);
	}
	Py_XDECREF(keys);
	Py_XDECREF(res);
	return ok && Npc_SetGlobal("Repos", PyInt_FromLong(1)) ? Py_ReturnNone() : NULL;
}

/* ---- the respawn ---- */

/* Scrap.TestSphere at CharConversor.CPPos(conv), radius 200, "rv" (a new reference) */
static PyObject *Npc_TestConv(PyObject *conv) {
	npcRefs_t r = { 0 };
	PyObject *pos = Npc_Keep(&r, Script_Ask("CharConversor", "CPPos", "(O)", conv));
	PyObject *x = pos ? Npc_Keep(&r, Npc_Item(pos, 0)) : NULL;
	PyObject *y = x ? Npc_Keep(&r, Npc_Item(pos, 1)) : NULL;
	PyObject *z = y ? Npc_Keep(&r, Npc_Item(pos, 2)) : NULL;
	return Npc_Done(&r, z ? Script_Ask("Scrap", "TestSphere", "(OOOis)", x, y, z, 200, "rv") : NULL);
}

/* Scrap.GetAlarm()[0] (a new reference) */
static PyObject *Npc_Alarm(void) {
	PyObject *a = Script_Ask("Scrap", "GetAlarm", "()");
	PyObject *v = a ? Npc_Item(a, 0) : NULL;
	Py_XDECREF(a);
	return v;
}

/* Respawn(csp): after a death the character comes back at the nearest free conversion point (a repositioned one
 * near the player), else it tries again in 5 s; a Gear of a repositioned police without alarm comes back as the
 * police; a mission target is marked again */
static PyObject *CharsNPC_Respawn(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "CharConversor", "Chars", "MissionsFuncs", "CharsNPC", NULL };
	PyObject *csp;
	npcRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &csp) || !Npc_Import(mods)) {
		return NULL;
	}
	PyObject *name = Npc_Keep(&r, PyObject_GetAttrString(csp, "Name"));
	PyObject *act = name && Npc_Print(&name, 1) ? Npc_Keep(&r, PyObject_GetAttrString(csp, "ActCtrl")) : NULL;
	if (!act) {
		return Npc_Done(&r, NULL);
	}
	int swap = Npc_Is(act, "Gear");
	if (swap) {
		PyObject *repos = Npc_Keep(&r, PyObject_GetAttrString(csp, "repos"));
		swap = repos ? Script_NumCmp(repos, 1) : -2;
		swap = swap == -2 ? -1 : swap == 0;
	}
	if (swap > 0) {
		PyObject *alarm = Npc_Keep(&r, Npc_Alarm());
		swap = alarm ? Script_NumCmp(alarm, 0) : -2;
		swap = swap == -2 ? -1 : swap == 0;
	}
	if (swap > 0) {
		PyObject *res = Npc_Keep(&r, Script_Attr("CharsNPC", "CharRes"));
		PyObject *n = res ? Npc_Keep(&r, PyObject_GetAttrString(csp, "Name")) : NULL;
		PyObject *key = n ? Npc_Keep(&r, Npc_SAdd("Police", Npc_Keep(&r, PySequence_GetSlice(n, 4, INT_MAX)))) : NULL;
		csp = key ? Npc_Keep(&r, PyObject_GetItem(res, key)) : NULL;
	}
	PyObject *usr = swap >= 0 && csp ? Npc_Keep(&r, Script_Ask("Scrap", "UsrEntity", "(i)", 0)) : NULL;
	int dorepos = usr ? Npc_True(Npc_Keep(&r, PyObject_GetAttrString(csp, "repos"))) : -1;
	if (dorepos > 0) {
		dorepos = Npc_True(usr);
	}
	if (dorepos > 0) {
		PyObject *upos = Npc_Keep(&r, PyObject_GetAttrString(usr, "Pos"));
		PyObject *ract = upos ? Npc_Keep(&r, PyObject_GetAttrString(csp, "ActCtrl")) : NULL;
		PyObject *rpos = ract ? Npc_Keep(&r, Script_Ask("SAI", "GetReposCharPos", "(OO)", upos, ract)) : NULL;
		dorepos = Npc_True(rpos);
		if (dorepos > 0 && !Script_HSet(csp, "Pos", Npc_Ref(rpos))) {
			dorepos = -1;
		}
	}
	PyObject *cpos = dorepos >= 0 ? Npc_Keep(&r, PyObject_GetAttrString(csp, "Pos")) : NULL;
	PyObject *conv = cpos ? Npc_Keep(&r, Script_Ask("CharConversor", "GetNearestConversionPoint", "(O)", cpos)) : NULL;
	int hit = conv ? Npc_True(Npc_Keep(&r, Npc_TestConv(conv))) : -1;
	if (hit > 0) {
		int u = Npc_True(usr);
		if (u > 0) {
			PyObject *prev = Npc_Keep(&r, Script_Attr("CharConversor", "PrevConv"));
			int p = Npc_True(prev);
			if (p > 0) {
				conv = prev;
			} else if (p == 0) {
				PyObject *u0 = Npc_Keep(&r, Script_Ask("Scrap", "UsrEntity", "(i)", 0));
				PyObject *upos = u0 ? Npc_Keep(&r, PyObject_GetAttrString(u0, "Pos")) : NULL;
				conv = upos ? Npc_Keep(&r, Script_Ask("CharConversor", "GetNearestConversionPoint", "(O)", upos)) : NULL;
			} else {
				conv = NULL;
			}
			hit = conv ? Npc_True(Npc_Keep(&r, Npc_TestConv(conv))) : -1;
		} else if (u < 0) {
			hit = -1;
		}
	}
	int later = hit;
	if (later == 0) {
		PyObject *ract = Npc_Keep(&r, PyObject_GetAttrString(csp, "ActCtrl"));
		PyObject *chars = ract ? Npc_Keep(&r, Npc_Global("RespawnChars")) : NULL;
		int in = chars ? PySequence_In(chars, ract) : -1;
		later = in < 0 ? -1 : in ? 0 : Npc_True(Npc_Keep(&r, Npc_Alarm()));
	}
	if (later < 0) {
		return Npc_Done(&r, NULL);
	}
	if (later) {
		return Npc_Done(&r, Script_LaterIn(5.0, "CharsNPC", "Respawn", Py_BuildValue("(O)", csp), NULL) ? Py_ReturnNone() : NULL);
	}
	PyObject *c = Npc_Keep(&r, Npc_CreateChar(csp, NULL));
	byte ok = c && Script_Try("CharConversor", "SpawnChar", "(OO)", c, conv) &&
		Script_HSet(c, "LLogicIdle", PyInt_FromLong(1)) && Script_HSet(c, "Invulnerable", PyInt_FromLong(1));
	PyObject *cn = ok ? Npc_Keep(&r, PyObject_GetAttrString(c, "Name")) : NULL;
	ok = cn && Script_LaterIn(1.5, "CharsNPC", "ActiveSpawned", Py_BuildValue("(O)", cn), NULL);
	if (ok && dorepos) {
		PyObject *p = Npc_Keep(&r, PyObject_GetAttrString(c, "Pos"));
		PyObject *a = p ? Npc_Keep(&r, PyObject_GetAttrString(c, "ActCtrl")) : NULL;
		PyObject *n = a ? Npc_Keep(&r, PyObject_GetAttrString(c, "Name")) : NULL;
		ok = n && Npc_Keep(&r, Script_Ask("SAI", "GetReposCharPos", "(OOO)", p, a, n));
	}
	PyObject *n = ok ? Npc_Keep(&r, PyObject_GetAttrString(c, "Name")) : NULL;
	int target = n ? Npc_True(Npc_Keep(&r, Script_Ask("MissionsFuncs", "IsCharMissionObjective", "(O)", n))) : -1;
	if (target > 0) {
		PyObject *msg = Script_Try("MissionsFuncs", "ActivateTarget", "(Oi)", c, 1) ?
			Npc_Keep(&r, Script_Ask("Scrap", "GetLangStr", "(s)", "MainMissionCharReborn")) : NULL;
		target = msg && Script_Try("SScorer", "SetMsgText", "(Od)", msg, (double)Timer_GetTime(&g_pWorld->timer) + 5) ? 0 : -1;
	}
	return Npc_Done(&r, target == 0 ? Py_ReturnNone() : NULL);
}

/* ActiveSpawned(EntityName): 1.5 s after the respawn the character acts and can be hurt again */
static PyObject *CharsNPC_ActiveSpawned(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *c = Script_Ask("Scrap", "GetEntity", "(O)", name);
	byte ok = c && Script_HSet(c, "LLogicIdle", PyInt_FromLong(0)) && Script_HSet(c, "Invulnerable", PyInt_FromLong(0));
	Py_XDECREF(c);
	return ok ? Py_ReturnNone() : NULL;
}

/* ---- the generic talk ---- */

/* CheckActualEntity(Ename): the last talking character (when idle by the talk) gets its state back when another
 * one talks now */
static byte Npc_CheckActualEntity(PyObject *ename) {
	static const char *const mods[] = { "Speech", NULL };
	if (!Npc_Import(mods)) {
		return 0;
	}
	PyObject *cur = Npc_Global("ActualEntityName");
	if (!cur) {
		return 0;
	}
	int diff = PyObject_Compare(ename, cur);
	byte ok = !PyErr_Occurred();
	if (ok && diff) {
		PyObject *ent = Script_Ask("Scrap", "GetEntity", "(O)", cur);
		int idle = Npc_True(ent);
		PyObject *v = idle > 0 ? PyObject_GetAttrString(ent, "LLogicIdle") : NULL;
		if (idle > 0) {
			idle = Npc_True(v);
		}
		ok = idle >= 0 && (!idle || Script_Try("Speech", "RestoreState", "(O)", cur));
		Py_XDECREF(v);
		Py_XDECREF(ent);
	}
	Py_DECREF(cur);
	return ok && Npc_SetGlobal("ActualEntityName", Npc_Ref(ename));
}

static PyObject *CharsNPC_CheckActualEntity(PyObject *self, PyObject *args) {
	PyObject *ename;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &ename)) {
		return NULL;
	}
	return Npc_CheckActualEntity(ename) ? Py_ReturnNone() : NULL;
}

/* MercsHired(): one of the three mercenaries is a friend of the player */
static byte Npc_MercsHired(void) {
	return !(Npc_SaveIs("MercFriends.MercenaryA_Smartie", "0") && Npc_SaveIs("MercFriends.MercenaryB_Dumber", "0") &&
		Npc_SaveIs("MercFriends.MercenaryC_Brutus", "0"));
}

static PyObject *CharsNPC_MercsHired(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return PyInt_FromLong(Npc_MercsHired());
}

/* string.find(s, sub) != -1: 1, 0, -1 with the error set */
static int Npc_Find(PyObject *s, PyObject *sub) {
	PyObject *f = Script_Ask("string", "find", "(OO)", s, sub);
	int c = f ? Script_NumCmp(f, -1) : -2;
	Py_XDECREF(f);
	return c == -2 ? -1 : c != 0;
}

/* IncludeSubOptions(option): the sub options that contain option move from subOptionsList to optionsList */
static byte Npc_IncludeSubOptions(PyObject *opt) {
	static const char *const mods[] = { "string", NULL };
	if (!Npc_Import(mods)) {
		return 0;
	}
	PyObject *aux = PyList_New(0);
	PyObject *subs = aux ? Npc_Global("subOptionsList") : NULL;
	byte ok = subs != NULL;
	for (int i = 0; ok && i < PySequence_Length(subs); i++) {
		PyObject *s = PySequence_GetItem(subs, i);
		int in = s ? Npc_Find(s, opt) : -1;
		if (in > 0) {
			PyObject *opts = Npc_Global("optionsList");
			PyObject *v = opts ? Script_Method(opts, "append", "(O)", s) : NULL;
			in = v && PyList_Append(aux, s) == 0 ? 1 : -1;
			Py_XDECREF(v);
			Py_XDECREF(opts);
		}
		ok = in >= 0;
		Py_XDECREF(s);
	}
	for (int i = 0; ok && i < PyList_Size(aux); i++) {
		PyObject *list = Npc_Global("subOptionsList");
		PyObject *v = list ? Script_Method(list, "remove", "(O)", PyList_GetItem(aux, i)) : NULL;
		ok = v != NULL;
		Py_XDECREF(v);
		Py_XDECREF(list);
	}
	Py_XDECREF(subs);
	Py_XDECREF(aux);
	return ok;
}

static PyObject *CharsNPC_IncludeSubOptions(PyObject *self, PyObject *args) {
	PyObject *opt;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &opt)) {
		return NULL;
	}
	return Npc_IncludeSubOptions(opt) ? Py_ReturnNone() : NULL;
}

/* Classify(l, cad): the entries of l that contain cad, None when there are none (a new reference) */
static PyObject *Npc_Classify(PyObject *l, PyObject *cad) {
	static const char *const mods[] = { "string", NULL };
	PyObject *aux = Npc_Import(mods) ? PyList_New(0) : NULL;
	byte ok = aux != NULL;
	for (int i = 0; ok && i < PySequence_Length(l); i++) {
		PyObject *c = PySequence_GetItem(l, i);
		int in = c ? Npc_Find(c, cad) : -1;
		ok = in >= 0 && (!in || PyList_Append(aux, c) == 0);
		Py_XDECREF(c);
	}
	if (!ok) {
		Py_XDECREF(aux);
		return NULL;
	}
	if (PyList_Size(aux) > 0) {
		return aux;
	}
	Py_DECREF(aux);
	return Py_ReturnNone();
}

static PyObject *CharsNPC_Classify(PyObject *self, PyObject *args) {
	PyObject *l, *cad;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &l, &cad)) {
		return NULL;
	}
	return Npc_Classify(l, cad);
}

/* "Generics_" + act + charType + "_" */
static PyObject *Npc_Generics(PyObject *act, PyObject *type) {
	return Npc_AddS(Npc_Add(Npc_SAdd("Generics_", act), type), "_");
}

/* ClassifyOptions(charType): the options of a type in the language file (Generics_<GameAct><type>_<n>..., earlier
 * acts when the act has none), the longest ones as sub options; the disguise option of Rusty and the crazy gambler
 * and the Special option go first. (optsList, subOptsList) (a new reference) */
static PyObject *Npc_ClassifyOptions(PyObject *type) {
	npcRefs_t r = { 0 };
	PyObject *opts = Npc_Keep(&r, PyList_New(0)), *subs = Npc_Keep(&r, PyList_New(0));
	PyObject *empty = Npc_Keep(&r, PyList_New(0));
	PyObject *act = empty ? Npc_Keep(&r, Script_Ask("Scrap", "GetSaveVar", "(s)", "GameAct")) : NULL;
	PyObject *cad = act ? Npc_Keep(&r, Npc_Generics(act, type)) : NULL;
	PyObject *all = NULL;
	long i = 1, minLen = 1000, maxLen = -1;
	char num[16];
	while (cad) {
		Str_Printf(num, sizeof(num), "%ld", i);
		PyObject *key = Npc_AddS(Npc_AddS(Npc_Ref(cad), num), "Opt");
		all = key ? Npc_Keep(&r, Script_Ask("Scrap", "GetLangEntries", "(O)", key)) : NULL;
		Py_XDECREF(key);
		int c = all ? PyObject_Compare(all, empty) : 0;
		if (!all || PyErr_Occurred()) {
			return Npc_Done(&r, NULL);
		}
		const char *next = Npc_Is(act, "Prologue") ? "1stMurder" : Npc_Is(act, "1stMurder") ? "2ndMurder" :
			Npc_Is(act, "2ndMurder") ? "3rdMurder" : Npc_Is(act, "3rdMurder") || Npc_Is(act, "Epilogue") ? "" : NULL;
		if (c != 0 || !next) {
			break;
		}
		act = Npc_Keep(&r, PyString_FromString(next));
		cad = act ? Npc_Keep(&r, Npc_Generics(act, type)) : NULL;
	}
	if (!cad || !all || !Npc_Print(&all, 1)) {
		return Npc_Done(&r, NULL);
	}
	for (;;) {
		Str_Printf(num, sizeof(num), "%ld", i);
		PyObject *pre = Npc_AddS(Npc_Ref(cad), num);
		PyObject *aux = pre ? Npc_Classify(all, pre) : NULL;
		Py_XDECREF(pre);
		int t = Npc_True(aux);
		for (int j = 0; t > 0 && j < PySequence_Length(aux); j++) {
			PyObject *opt = PySequence_GetItem(aux, j);
			int l = opt && PyList_Append(opts, opt) == 0 ? PyObject_Length(opt) : -1;
			Py_XDECREF(opt);
			if (l < 0) {
				t = -1;
			} else if (l <= minLen) {
				minLen = l;
			} else if (l >= maxLen) {
				maxLen = l;
			}
		}
		Py_XDECREF(aux);
		if (t < 0) {
			return Npc_Done(&r, NULL);
		}
		if (!t) {
			break;
		}
		i++;
	}
	if (minLen != 1000 && maxLen != -1) {
		PyObject *aux = Npc_Keep(&r, PyList_New(0));
		byte ok = aux != NULL;
		for (int j = 0; ok && j < PyList_Size(opts); j++) {
			PyObject *opt = PyList_GetItem(opts, j);
			int l = PyObject_Length(opt);
			ok = l >= 0 && (l != maxLen || (PyList_Append(aux, opt) == 0 && PyList_Append(subs, opt) == 0));
		}
		for (int j = 0; ok && j < PyList_Size(aux); j++) {
			PyObject *v = Script_Method(opts, "remove", "(O)", PyList_GetItem(aux, j));
			ok = v != NULL;
			Py_XDECREF(v);
		}
		if (!ok) {
			return Npc_Done(&r, NULL);
		}
	}
	static const char *const disguised[] = { "Rusty", "CrazyGambler", NULL };
	PyObject *usr = Npc_Keep(&r, Script_Ask("Scrap", "UsrEntity", "(i)", 0));
	int t = Npc_True(usr);
	if (t > 0) {
		PyObject *ua = Npc_Keep(&r, PyObject_GetAttrString(usr, "ActCtrl"));
		t = ua ? !Npc_Is(ua, "Dtritus") && Npc_In(type, disguised) : -1;
	}
	if (t > 0) {
		PyObject *s = Npc_Keep(&r, Npc_AddS(Npc_SAdd("Generics_Disguise", type), "_1"));
		t = s && PyList_Insert(opts, 0, s) == 0 ? 0 : -1;
	}
	if (t < 0) {
		return Npc_Done(&r, NULL);
	}
	if (!Npc_Is(type, "Bishop") || Npc_SaveIs("Player.InfiniteLives", "0")) {
		PyObject *key = Npc_Keep(&r, Npc_AddS(Npc_AddS(Npc_SAdd("Generics_Special", type), "_"), "1Opt"));
		PyObject *special = key ? Npc_Keep(&r, Script_Ask("Scrap", "GetLangEntries", "(O)", key)) : NULL;
		int c = special ? PyObject_Compare(special, empty) : 0;
		if (!special || PyErr_Occurred()) {
			return Npc_Done(&r, NULL);
		}
		if (c != 0) {
			PyObject *first = Npc_Keep(&r, Npc_Item(special, 0));
			if (!first || PyList_Insert(opts, 0, first) != 0) {
				return Npc_Done(&r, NULL);
			}
		}
	}
	return Npc_Done(&r, Py_BuildValue("(OO)", opts, subs));
}

static PyObject *CharsNPC_ClassifyOptions(PyObject *self, PyObject *args) {
	PyObject *type;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &type)) {
		return NULL;
	}
	return Npc_ClassifyOptions(type);
}

/* Speech.StdAnsLst(optionsList); len(optionsList), -1 with the error set */
static int Npc_ShowOptions(void) {
	PyObject *opts = Npc_Global("optionsList");
	byte ok = opts && Script_Try("Speech", "StdAnsLst", "(O)", opts);
	Py_XDECREF(opts);
	opts = ok ? Npc_Global("optionsList") : NULL;
	int n = opts ? PyObject_Length(opts) : -1;
	Py_XDECREF(opts);
	return n;
}

/* PlayOption(optId, msg, ans, UsrName, ch): the talk of an option, its sub options join the list, the arrow on the
 * next option; the challenge options (without infinite lives) */
static byte Npc_PlayOption(PyObject *opt, PyObject *msg, PyObject *ans, PyObject *usr, PyObject *ch) {
	static const char *const mods[] = { "Speech", "Challenge", NULL };
	npcRefs_t r = { 0 };
	if (!Npc_Import(mods)) {
		return 0;
	}
	int said = Npc_True(Npc_Keep(&r, Script_Ask("Speech", "OnSpch", "(OO)", msg, ans)));
	int n = -1;
	if (said > 0) {
		PyObject *opts = Npc_Keep(&r, Npc_Global("optionsList"));
		PyObject *at = opts ? Npc_Keep(&r, Script_Method(opts, "index", "(O)", opt)) : NULL;
		PyObject *one = at ? Npc_Keep(&r, PyInt_FromLong(1)) : NULL;
		PyObject *index = one ? Npc_Keep(&r, PyNumber_Add(at, one)) : NULL;
		n = index && Npc_IncludeSubOptions(opt) ? Npc_ShowOptions() : -1;
		int last = n >= 0 ? Script_NumCmp(index, n) : -2;
		if (last == -2 || !Script_HSet(ch, "ArriOpt", last == 0 ? PyInt_FromLong(0) : Npc_Ref(index))) {
			said = -1;
		}
	}
	if (said < 0) {
		Npc_Done(&r, NULL);
		return 0;
	}
	byte ok = 1;
	if (Npc_SaveIs("Player.InfiniteLives", "0")) {
		if (n < 0) {
			/* numOpts is only bound when the speech started */
			PyErr_SetString(PyExc_NameError, "numOpts");
			ok = 0;
		} else {
			ok = Script_Try("Challenge", "AddChallenge", "(OOi)", ch, usr, n);
		}
	}
	Npc_Done(&r, NULL);
	return ok;
}

static PyObject *CharsNPC_PlayOption(PyObject *self, PyObject *args) {
	PyObject *opt, *msg, *ans, *usr, *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOOO", &opt, &msg, &ans, &usr, &ch)) {
		return NULL;
	}
	return Npc_PlayOption(opt, msg, ans, usr, ch) ? Py_ReturnNone() : NULL;
}

/* Speech.OnSpch(prefix + type + suffix, ...): the talk of a challenge; 1, 0, -1 with the error set */
static int Npc_ChallengeSpch(const char *msg, PyObject *type, const char *msgEnd, const char *ans, const char *ansEnd) {
	PyObject *m = msgEnd ? Npc_AddS(Npc_SAdd(msg, type), msgEnd) : PyString_FromString(msg);
	PyObject *a = m ? Npc_AddS(Npc_SAdd(ans, type), ansEnd) : NULL;
	PyObject *v = a ? Script_Ask("Speech", "OnSpch", "(OO)", m, a) : NULL;
	int t = Npc_True(v);
	Py_XDECREF(v);
	Py_XDECREF(a);
	Py_XDECREF(m);
	return t;
}

/* Challenge.challengeType, foeType and SetChallengeOptions(ch): 0 with the error set */
static byte Npc_StartChallenge(const char *kind, PyObject *foe, PyObject *ch) {
	Script_SetVar("Challenge", "challengeType", PyString_FromString(kind));
	Script_SetVar("Challenge", "foeType", Npc_Ref(foe));
	return Script_Try("Challenge", "SetChallengeOptions", "(O)", ch);
}

/* GenericNPCSpeech(UsrName, CharName, optId, FullOption): the talk with an NPC. Talk: the greeting (one of three
 * answers at random) and the options of the type; an option of the list; the challenge options; anything else
 * prints an error and starts the talk again */
static PyObject *CharsNPC_GenericNPCSpeech(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Speech", "Chars", "Challenge", "Menu", "Challenge", "string", NULL };
	static const char *const mercs[] = { "MercenaryA", "MercenaryB", "MercenaryC", NULL };
	PyObject *usr, *chname, *opt, *full;
	npcRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &chname, &opt, &full) || !Npc_Import(mods)) {
		return NULL;
	}
	PyObject *ch = Npc_Keep(&r, Script_Ask("Scrap", "GetEntity", "(O)", chname));
	if (!ch) {
		return Npc_Done(&r, NULL);
	}
	Script_SetVar("Speech", "PlayerName", Npc_Ref(usr));
	Script_SetVar("Speech", "NPCName", Npc_Ref(chname));
	PyObject *e = Npc_Keep(&r, Script_Ask("Scrap", "GetEntity", "(O)", chname));
	PyObject *type0 = e ? Npc_Keep(&r, PyObject_GetAttrString(e, "ActCtrl")) : NULL;
	if (!type0) {
		return Npc_Done(&r, NULL);
	}
	PyObject *type = Npc_In(type0, mercs) ? Npc_Keep(&r, PyString_FromString("Mercs")) : type0;
	if (!type) {
		return Npc_Done(&r, NULL);
	}
	byte ok = 1;
	int t, n;
	if (Npc_Is(opt, "Talk")) {
		PyObject *u = Npc_CheckActualEntity(chname) ? Npc_Keep(&r, Script_Ask("Scrap", "UsrEntity", "(i)", 0)) : NULL;
		int betty = u ? Npc_Is(type, "Betty") : -1;
		if (betty > 0) {
			betty = Npc_True(u);
		}
		if (betty > 0) {
			PyObject *ua = Npc_Keep(&r, PyObject_GetAttrString(u, "ActCtrl"));
			betty = ua ? !Npc_Is(ua, "Dtritus") : -1;
		}
		if (betty > 0) {
			ok = Script_Try("Speech", "OnSpch", "(ss)", "Generics_Betty_InitMsg2", "Generics_Betty_InitAns4");
		} else if (betty == 0) {
			char num[16];
			double rnd = Scrap_RandRange(1.0, 3.9);
			Str_Printf(num, sizeof(num), "%d", (int)rnd);
			PyObject *m = Npc_Keep(&r, Npc_AddS(Npc_SAdd("Generics_", type), "_InitMsg1"));
			PyObject *a = m ? Npc_Keep(&r, Npc_AddS(Npc_AddS(Npc_SAdd("Generics_", type), "_InitAns"), num)) : NULL;
			t = a ? Npc_True(Npc_Keep(&r, Script_Ask("Speech", "OnSpch", "(OO)", m, a))) : -1;
			if (t > 0) {
				if (Npc_SaveIs("GameAct", "Epilogue")) {
					n = 0;
					if (Npc_Is(type, "Bishop") || Npc_Is(type, "Mercs")) {
						PyObject *key = Npc_Keep(&r, Npc_AddS(Npc_AddS(Npc_SAdd("Generics_Special", type), "_"), "1Opt"));
						PyObject *special = key ? Npc_Keep(&r, Script_Ask("Scrap", "GetLangEntries", "(O)", key)) : NULL;
						PyObject *first = special ? Npc_Keep(&r, Npc_Item(special, 0)) : NULL;
						n = first && Npc_SetGlobal("optionsList", Py_BuildValue("[O]", first)) && Npc_ShowOptions() >= 0 ? 1 : -1;
					}
				} else {
					PyObject *lists = Npc_Keep(&r, Npc_ClassifyOptions(type));
					PyObject *v[2];
					byte set = lists && Npc_Unpack(lists, 2, v);
					if (set && !(set = Npc_SetGlobal("optionsList", v[0]))) {
						Py_DECREF(v[1]);
					}
					n = set && Npc_SetGlobal("subOptionsList", v[1]) ? Npc_ShowOptions() : -1;
				}
				ok = n >= 0;
				if (ok && Npc_SaveIs("Player.InfiniteLives", "0") && (!Npc_Is(type, "Mercs") || !Npc_MercsHired())) {
					ok = Script_Try("Challenge", "AddChallenge", "(OOi)", ch, usr, n);
				}
			} else if (t < 0) {
				ok = 0;
			}
		} else {
			ok = 0;
		}
		return Npc_Done(&r, ok ? Py_ReturnNone() : NULL);
	}
	PyObject *opts = Npc_Keep(&r, Npc_Global("optionsList"));
	int listed = opts ? PySequence_In(opts, opt) : -1;
	if (listed > 0) {
		PyObject *msg = Npc_Keep(&r, Npc_AddS(Npc_Ref(opt), "Msg"));
		PyObject *ans = msg ? Npc_Keep(&r, Npc_AddS(Npc_Ref(opt), "Ans")) : NULL;
		ok = ans && Npc_PlayOption(opt, msg, ans, usr, ch);
	} else if (listed < 0) {
		ok = 0;
	} else if (Npc_Is(opt, "ChallengeRaceStdOpt0") || Npc_Is(opt, "ChallengeCombatStdOpt0")) {
		const char *kind = Npc_Is(opt, "ChallengeRaceStdOpt0") ? "Race" : "Combat";
		const char *pre = kind[0] == 'R' ? "ChallengeRace" : "ChallengeCombat";
		t = Npc_ChallengeSpch(pre, type, "Opt1", pre, "Msg1");
		ok = t >= 0 && (!t || Npc_StartChallenge(kind, type0, ch));
	} else if (Npc_Is(opt, "ChallengeRaceStdOpt1") || Npc_Is(opt, "ChallengeRaceStdOpt2") || Npc_Is(opt, "ChallengeRaceStdOpt3")) {
		ok = Script_Try("Challenge", "OnChallengeOpt", "(iO)", PyString_AsString(opt)[19] - '0', ch);
	} else if (Npc_Is(opt, "ChallengeRaceStdOpt4")) {
		t = Npc_ChallengeSpch("ChallengeRaceStdOpt2", type, NULL, "ChallengeRace", "Msg3");
		n = t > 0 ? Npc_ShowOptions() : 0;
		ok = t >= 0 && n >= 0 && (!t || Script_Try("Challenge", "AddChallenge", "(OOi)", ch, usr, n));
	} else {
		PyObject *head = Npc_Keep(&r, PyString_FromString("Error en los argumentos:  ("));
		PyObject *tail = Npc_Keep(&r, PyString_FromString(")"));
		PyObject *items[6] = { head, usr, chname, opt, full, tail };
		PyObject *a = head && tail && Npc_Print(items, 6) ? Npc_Keep(&r, Py_BuildValue("(OOsO)", usr, chname, "Talk", full)) : NULL;
		ok = a && Npc_Keep(&r, CharsNPC_GenericNPCSpeech(NULL, a));
	}
	return Npc_Done(&r, ok ? Py_ReturnNone() : NULL);
}

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
