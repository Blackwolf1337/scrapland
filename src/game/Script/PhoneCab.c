/*
 * The phone cabs and Dtritus' desktop in C (issue #2, the mission scripts missions/phonecab.py and
 * dtritusdesktop.py). PhoneCab: the cabs outdoors (DM_Phone_* dummies) with their communicator, using one saves the
 * game, and the revenge talk after a lost or won challenge (the foe calls the player at the marked cab).
 * DTritusDesktop: Dtritus' desktop in Press (the mission sets the talk in DialogController), the desktops of the
 * journalists and the journalists themselves.
 *
 * The module variables stay in the modules: PhoneCab.audioComEntity (the communicator in use, Challenge reads it),
 * DTritusDesktop.DTritusDesktop and DialogController (the missions set the talk there). The action classes are
 * data/missions/phonecab.def.
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"

static const char phonecab_def[] = {
#embed "../../../data/missions/phonecab.def"
	, 0 };

#define PHONECAB_FILE "missions/phonecab.def"

/* ---- shared by both ---- */

/* an attribute an entity gets when it is created: a number, a text, "Talk:" + the language text, or a vector */
typedef struct {
	const char *var;
	char kind;		/* 'i', 's', 't' (talk), 'v' */
	int i;
	const char *s;
	int v[3];
} phoneAttr_t;

/* e.var = value for the attributes up to the one with a NULL var: 0 with the error set */
static byte Phone_Set(PyObject *e, const phoneAttr_t *a) {
	for (; a->var; a++) {
		PyObject *v;
		if (a->kind == 'i') {
			v = PyInt_FromLong(a->i);
		} else if (a->kind == 's') {
			v = PyString_FromString(a->s);
		} else if (a->kind == 't') {
			PyObject *lang = Script_Ask("Scrap", "GetLangStr", "(s)", a->s);
			v = lang ? Script_SPlus("Talk:", lang) : NULL;
			Py_XDECREF(lang);
		} else {
			v = Py_BuildValue("(iii)", a->v[0], a->v[1], a->v[2]);
		}
		if (!Script_HSet(e, a->var, v)) {
			return 0;
		}
	}
	return 1;
}

/* the reset of the object in the module variable var of module when the player walks away (Speech.OnOutOfRange):
 * no options but the talk (language text talk), the relax loop, no head */
static PyObject *Phone_Reset(const char *module, const char *var, const char *talk) {
	const phoneAttr_t attrs[] = {
		{ "ArrnOpt", 'i', 0, NULL, { 0 } },
		{ "ArrOpt0", 't', 0, talk, { 0 } },
		{ "ArriOpt", 'i', 0, NULL, { 0 } },
		{ "Action", 's', 0, "Relax", { 0 } },
		{ "DefLoop", 's', 0, "Relax", { 0 } },
		{ "HeadModel", 's', 0, "", { 0 } },
		{ NULL, 0, 0, NULL, { 0 } },
	};
	PyObject *e = Script_Global(module, var);
	int t = Script_True(e);
	byte ok = t >= 0 && (!t || Phone_Set(e, attrs));
	Py_XDECREF(e);
	return ok ? Py_ReturnNone() : NULL;
}

/* the use function of a communicator: without the radar mark (RadarShow 1 or 2) the message that it does not work
 * now, then use(UsrName, CharName, optId, FullOption) */
static PyObject *Phone_DefUse(PyObject *args, PyObject *(*use)(PyObject *, PyObject *)) {
	PyObject *usr, *chname, *opt, *full;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &chname, &opt, &full)) {
		return NULL;
	}
	PyObject *e = Script_Entity(chname);
	PyObject *radar = e ? Script_GetAttr(e, "RadarShow") : NULL;
	int c = radar ? Script_NumCmp(radar, 1) : -2;
	Py_XDECREF(radar);
	if (c != 0 && c != -2) {
		radar = Script_GetAttr(e, "RadarShow");
		c = radar ? Script_NumCmp(radar, 2) : -2;
		Py_XDECREF(radar);
		if (c != 0 && c != -2) {
			PyObject *msg = Script_Ask("Scrap", "GetLangStr", "(s)", "CantUsePhone");
			PyObject *now = msg ? Script_Ask("Scrap", "GetTime", "()") : NULL;
			PyObject *five = now ? PyInt_FromLong(5) : NULL;
			PyObject *until = five ? PyNumber_Add(now, five) : NULL;
			c = until && Script_Try("SScorer", "SetMsgText", "(OO)", msg, until) ? 1 : -2;
			Py_XDECREF(until);
			Py_XDECREF(five);
			Py_XDECREF(now);
			Py_XDECREF(msg);
		}
	}
	Py_XDECREF(e);
	return c == -2 ? NULL : use(NULL, args);
}

/* SaveGame.MarkWayPoint(Init.Path, CharName, CharName) and the next option of the entity: e.ArriOpt =
 * (e.ArriOpt + 1) % e.ArrnOpt. The entity (a new reference), NULL with the error set */
static PyObject *Phone_Mark(PyObject *chname) {
	static const char *const mods[] = { "SaveGame", "Init", "Speech", NULL };
	if (!Script_Import(mods)) {
		return NULL;
	}
	PyObject *path = Script_Attr("Init", "Path");
	byte ok = path && Script_Try("SaveGame", "MarkWayPoint", "(OOO)", path, chname, chname);
	Py_XDECREF(path);
	PyObject *e = ok ? Script_Entity(chname) : NULL;
	PyObject *cur = e ? Script_GetAttr(e, "ArriOpt") : NULL;
	PyObject *one = cur ? PyInt_FromLong(1) : NULL;
	PyObject *next = one ? PyNumber_Add(cur, one) : NULL;
	PyObject *n = next ? Script_GetAttr(e, "ArrnOpt") : NULL;
	ok = n && Script_HSet(e, "ArriOpt", PyNumber_Remainder(next, n));
	Py_XDECREF(n);
	Py_XDECREF(next);
	Py_XDECREF(one);
	Py_XDECREF(cur);
	if (!ok) {
		Py_XDECREF(e);
		return NULL;
	}
	return e;
}

/* c.Pos in front of an object at pos with the y angle ang (180 back), c.Ang = 0, ang + turn, 0, on the floor */
static byte Phone_Spawn(PyObject *c, PyObject *pos, PyObject *ang, int turn) {
	scriptRefs_t r = { 0 };
	PyObject *rot = Script_Keep(&r, Script_Ask("SVec", "Rotate3D", "((iii)(iOi))", 0, 0, -180, 0, ang, 0));
	PyObject *p = rot ? Script_Ask("SVec", "Add", "(OO)", pos, rot) : NULL;
	PyObject *t = p && Script_HSet(c, "Pos", p) ? Script_Keep(&r, PyInt_FromLong(turn < 0 ? -turn : turn)) : NULL;
	PyObject *a = t ? Script_Keep(&r, turn < 0 ? PyNumber_Subtract(ang, t) : PyNumber_Add(ang, t)) : NULL;
	PyObject *name = a && Script_HSet(c, "Ang", Py_BuildValue("(iOi)", 0, a, 0)) ?
		Script_Keep(&r, Script_GetAttr(c, "Name")) : NULL;
	byte ok = name && Script_Try("SLogic", "SetOnFloor", "(O)", name);
	Script_Done(&r, NULL);
	return ok;
}

/* Scrap.CreateEntity(name, pos[0], pos[1], pos[2], "WithLife") with Ang = 0, yAng + turn, 0 (a new reference) */
static PyObject *Phone_Create(PyObject *name, PyObject *pos, PyObject *yang, int turn) {
	scriptRefs_t r = { 0 };
	PyObject *x = Script_Keep(&r, Script_Index(pos, 0));
	PyObject *y = x ? Script_Keep(&r, Script_Index(pos, 1)) : NULL;
	PyObject *z = y ? Script_Keep(&r, Script_Index(pos, 2)) : NULL;
	PyObject *e = z ? Script_Ask("Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, "WithLife") : NULL;
	PyObject *t = e ? Script_Keep(&r, PyInt_FromLong(turn)) : NULL;
	PyObject *a = t ? Script_Keep(&r, turn ? PyNumber_Add(yang, t) : Script_Ref(yang)) : NULL;
	if (!a || !Script_HSet(e, "Ang", Py_BuildValue("(iOi)", 0, a, 0))) {
		Py_XDECREF(e);
		return Script_Done(&r, NULL);
	}
	return Script_Done(&r, e);
}

/* Scrap.StartDummySearch(name, 1) and the first dummy (a new reference) */
static PyObject *Phone_Dummy(PyObject *name) {
	return name && Script_Try("Scrap", "StartDummySearch", "(Oi)", name, 1) ? Script_Ask("Scrap", "NextDummySearch", "()") : NULL;
}

/* ---- PhoneCab ---- */

static PyObject *PhoneCab_ForceResetPhoneCab(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Phone_Reset("PhoneCab", "audioComEntity", "PhoneCab");
}

/* the options of the revenge on ch (save variables Revenge.Type and Revenge.Foe) */
static PyObject *PhoneCab_SetRevengeOptions(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "string", "Challenge", NULL };
	PyObject *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &ch) || !Script_Import(mods)) {
		return NULL;
	}
	PyObject *type = Script_SaveVar("Revenge.Type");
	PyObject *foe = type ? Script_SaveVar("Revenge.Foe") : NULL;
	byte ok = foe && Challenge_SetOptions(ch, type, foe);
	Py_XDECREF(foe);
	Py_XDECREF(type);
	return ok ? Py_ReturnNone() : NULL;
}

/* print "Error RevengeSpeech(", UsrName, CharName, optId, FullOption, ")": 0 with the error set */
static byte PhoneCab_PrintError(PyObject *usr, PyObject *chname, PyObject *opt, PyObject *full) {
	PyObject *head = PyString_FromString("Error RevengeSpeech(");
	PyObject *tail = head ? PyString_FromString(")") : NULL;
	PyObject *items[6] = { head, usr, chname, opt, full, tail };
	byte ok = tail && Script_Print(items, 6);
	Py_XDECREF(tail);
	Py_XDECREF(head);
	return ok;
}

/* the revenge talk: the foe tells the player the result (Talk), the bets of a new challenge, or the options again */
static PyObject *PhoneCab_RevengeSpeech(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Speech", "Challenge", NULL };
	PyObject *usr, *chname, *opt, *full;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &chname, &opt, &full) || !Script_Import(mods)) {
		return NULL;
	}
	PyObject *ch = Script_Keep(&r, Script_Entity(chname));
	if (!ch || !Script_SetGlobal("PhoneCab", "audioComEntity", Script_Ref(ch))) {
		return Script_Done(&r, NULL);
	}
	Script_SetVar("Speech", "PlayerName", Script_Ref(usr));
	Script_SetVar("Speech", "NPCName", Script_Ref(chname));
	byte ok = 1;
	if (Script_Is(opt, "Talk")) {
		PyObject *player = Script_Keep(&r, Script_Usr());
		int t = Script_True(player);
		if (t <= 0) {
			return Script_Done(&r, t < 0 ? NULL : Py_ReturnNone());
		}
		PyObject *wins = Script_Keep(&r, Script_Attr("Challenge", "playerWinsRevenge"));
		t = Script_True(wins);
		PyObject *foe = t >= 0 ? Script_Keep(&r, Script_SaveVar("Revenge.Foe")) : NULL;
		PyObject *head = foe ? Script_Keep(&r, PySequence_GetSlice(foe, 0, 4)) : NULL;
		if (!head) {
			return Script_Done(&r, NULL);
		}
		if (Script_Is(head, "Merc")) {
			foe = Script_Keep(&r, PyString_FromString("Mercs"));
		}
		const char *prefix = t ? "Win" : "Loose";
		PyObject *type = Script_Keep(&r, Script_SaveVar("Revenge.Type"));
		PyObject *msg = NULL, *ans = NULL;
		char key[64];
		if (!type) {
			return Script_Done(&r, NULL);
		}
		if (Script_Is(type, "Race") || Script_Is(type, "Combat")) {
			const char *kind = Script_Is(type, "Race") ? "Race" : "Combat";
			Str_Printf(key, sizeof(key), "%sRevenge%sInitMsg", prefix, kind);
			msg = Script_Keep(&r, PyString_FromString(key));
			Str_Printf(key, sizeof(key), "%sRevenge%sInitAns", prefix, kind);
			ans = Script_Keep(&r, Script_Plus(PyString_FromString(key), foe));
			if (!ans) {
				return Script_Done(&r, NULL);
			}
		} else if (!PhoneCab_PrintError(usr, chname, opt, full)) {
			return Script_Done(&r, NULL);
		}
		Script_SetVar("Speech", "OnOutOfRange", Script_Global("PhoneCab", "ForceResetPhoneCab"));
		wins = Script_Keep(&r, Script_Attr("Challenge", "playerWinsRevenge"));
		t = Script_True(wins);
		if (t == 0) {
			PyObject *num = Script_Keep(&r, Script_Int(Script_Keep(&r, Script_SaveVar("Revenge.NumEnemies"))));
			PyObject *launch = num ? Script_Attr("Challenge", "LaunchRevenge") : NULL;
			if (!launch) {
				return Script_Done(&r, NULL);
			}
			Script_SetVar("Challenge", "numChallengeEnemies", Script_Ref(num));
			Script_SetVar("Speech", "OnEndOfSpeech", launch);
			Script_SetVar("Speech", "OnEndOfSpeechParams", PyTuple_New(0));
		} else if (t < 0) {
			return Script_Done(&r, NULL);
		}
		PyObject *ac = Script_Keep(&r, Script_Global("PhoneCab", "audioComEntity"));
		PyObject *rfoe = ac ? Script_Keep(&r, Script_SaveVar("Revenge.Foe")) : NULL;
		if (rfoe && !msg) {
			PyErr_SetString(PyExc_NameError, "initMsg");
			return Script_Done(&r, NULL);
		}
		ok = rfoe && Script_Keep(&r, Script_Ask("Speech", "OnObjSpch", "(OOOO)", ac, rfoe, msg, ans));
		wins = ok ? Script_Keep(&r, Script_Attr("Challenge", "playerWinsRevenge")) : NULL;
		t = Script_True(wins);
		ok = t >= 0;
		if (t > 0) {
			PyObject *a = Script_Keep(&r, Py_BuildValue("(O)", ch));
			ok = a && Script_Keep(&r, PhoneCab_SetRevengeOptions(NULL, a));
		}
	} else if (Script_Is(opt, "ChallengeRaceStdOpt1") || Script_Is(opt, "ChallengeRaceStdOpt2") ||
		Script_Is(opt, "ChallengeRaceStdOpt3")) {
		ok = Script_Try("Challenge", "OnChallengeOpt", "(iOi)", PyString_AsString(opt)[19] - '0', ch, 0);
	} else if (Script_Is(opt, "ChallengeRaceStdOpt4")) {
		PyObject *foe = Script_Keep(&r, Script_SaveVar("Revenge.Foe"));
		PyObject *groups = foe ? Script_Keep(&r, Script_Attr("Challenge", "groups")) : NULL;
		PyObject *group = groups ? Script_Keep(&r, PyObject_GetItem(groups, foe)) : NULL;
		if (!group) {
			return Script_Done(&r, NULL);
		}
		if (Script_Is(group, "Mercs")) {
			foe = Script_Keep(&r, PyString_FromString("Mercs"));
		}
		PyObject *ac = Script_Keep(&r, Script_Global("PhoneCab", "audioComEntity"));
		PyObject *rfoe = ac ? Script_Keep(&r, Script_SaveVar("Revenge.Foe")) : NULL;
		PyObject *ans = rfoe ? Script_Keep(&r, Script_PlusS(Script_SPlus("ChallengeRace", foe), "Msg3_Filter")) : NULL;
		PyObject *v = ans ? Script_Keep(&r, Script_Ask("Speech", "OnObjSpch", "(OOsO)", ac, rfoe, "ChallengeRaceStdOpt2", ans)) : NULL;
		int t = Script_True(v);
		ok = t >= 0;
		if (t > 0) {
			PyObject *a = Script_Keep(&r, Py_BuildValue("(O)", ch));
			ok = a && Script_Keep(&r, PhoneCab_SetRevengeOptions(NULL, a));
		}
	} else {
		ok = PhoneCab_PrintError(usr, chname, opt, full);
	}
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* GetNearestPhoneCab(usrPos): the communicator of PhoneCabs nearest to the position (None when there is none) */
static PyObject *PhoneCab_GetNearestPhoneCab(PyObject *self, PyObject *args) {
	PyObject *usrPos;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usrPos)) {
		return NULL;
	}
	PyObject *res = Script_Ref(Py_None), *dist = Script_Ref(Py_None);
	PyObject *name = Script_Ask("Scrap", "EntityListGet", "(ss)", "PhoneCabs", "First");
	PyObject *pc = Script_Entity(name);
	Py_XDECREF(name);
	int t;
	while ((t = Script_True(pc)) > 0) {
		PyObject *pos = Script_GetAttr(pc, "Pos");
		PyObject *d = pos ? Script_Ask("SVec", "Sub", "(OO)", pos, usrPos) : NULL;
		PyObject *newdist = d ? Script_Ask("SVec", "ModSqr", "(O)", d) : NULL;
		Py_XDECREF(d);
		Py_XDECREF(pos);
		int nearer = newdist ? Script_Eq(dist, Py_None) : -1;
		if (nearer == 0) {
			nearer = PyObject_Compare(newdist, dist) < 0;
			if (PyErr_Occurred()) {
				nearer = -1;
			}
		}
		if (nearer > 0) {
			Py_DECREF(res);
			Py_DECREF(dist);
			res = Script_Ref(pc);
			dist = Script_Ref(newdist);
		}
		Py_XDECREF(newdist);
		PyObject *next = nearer >= 0 ? Script_GetAttr(pc, "NextInList") : NULL;
		Py_DECREF(pc);
		pc = Script_Entity(next);
		Py_XDECREF(next);
	}
	Py_XDECREF(pc);
	Py_DECREF(dist);
	if (t < 0) {
		Py_DECREF(res);
		return NULL;
	}
	return res;
}

/* CreatePhoneCab(name, pos, yAng): the cab */
static PyObject *PhoneCab_CreatePhoneCab(PyObject *self, PyObject *args) {
	static const phoneAttr_t attrs[] = {
		{ "ActCtrl", 's', 0, "PhoneCab", { 0 } },
		{ "ArrowCanUse", 'i', 0, NULL, { 0 } },
		{ "Life", 'i', 1, NULL, { 0 } },
		{ "ActiveCMask", 'i', 0, NULL, { 0 } },
		{ "MapCollision", 'i', 1, NULL, { 0 } },
		{ NULL, 0, 0, NULL, { 0 } },
	};
	PyObject *name, *pos, *yang;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &pos, &yang)) {
		return NULL;
	}
	PyObject *pc = Phone_Create(name, pos, yang, 180);
	if (pc && !Phone_Set(pc, attrs)) {
		Py_DECREF(pc);
		return NULL;
	}
	return pc;
}

/* CreateAudioCom(name, pos, yAng): the communicator the player uses */
static PyObject *PhoneCab_CreateAudioCom(PyObject *self, PyObject *args) {
	static const phoneAttr_t attrs[] = {
		{ "ActCtrl", 's', 0, "AudioCom", { 0 } },
		{ "VRadius", 'i', 60, NULL, { 0 } },
		{ "UseAngSel", 'i', 0, NULL, { 0 } },
		{ "UseMapCheck", 'i', 0, NULL, { 0 } },
		{ "ArrowCanUse", 'i', 1, NULL, { 0 } },
		{ "Life", 'i', 1, NULL, { 0 } },
		{ "ActiveCMask", 'i', 0, NULL, { 0 } },
		{ "MapCollision", 'i', 1, NULL, { 0 } },
		{ "ArrnOpt", 'i', 0, NULL, { 0 } },
		{ "ArrOpt0", 't', 0, "PhoneCab", { 0 } },
		{ "UseFunc", 's', 0, "PhoneCab.DefUseAudioCom", { 0 } },
		{ NULL, 0, 0, NULL, { 0 } },
	};
	PyObject *name, *pos, *yang;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &pos, &yang)) {
		return NULL;
	}
	PyObject *ac = Phone_Create(name, pos, yang, 180);
	if (ac && !Phone_Set(ac, attrs)) {
		Py_DECREF(ac);
		return NULL;
	}
	return ac;
}

/* UseAudioCom(UsrName, CharName, optId, FullOption): using a communicator saves the game; with a revenge pending
 * the foe answers at the marked cab, or tells the player the police is still after him */
static PyObject *PhoneCab_UseAudioCom(PyObject *self, PyObject *args) {
	PyObject *usr, *chname, *opt, *full;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &chname, &opt, &full)) {
		return NULL;
	}
	PyObject *e = Script_Keep(&r, Phone_Mark(chname));
	if (!e || !Script_SetGlobal("PhoneCab", "audioComEntity", Script_Ref(e))) {
		return Script_Done(&r, NULL);
	}
	Script_SetVar("Speech", "OnOutOfRange", Script_Global("PhoneCab", "ForceResetPhoneCab"));
	PyObject *foe = Script_Keep(&r, Script_SaveVar("Revenge.Foe"));
	PyObject *type = foe ? Script_Keep(&r, Script_SaveVar("Revenge.Type")) : NULL;
	if (!type) {
		return Script_Done(&r, NULL);
	}
	if (Script_Is(type, "") || Script_Is(foe, "")) {
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *alarm = Script_Keep(&r, Script_Ask("Scrap", "GetAlarm", "()"));
	int t = Script_True(alarm ? Script_Keep(&r, Script_Index(alarm, 0)) : NULL);
	byte ok = t >= 0;
	if (t > 0) {
		PyObject *text = Script_Keep(&r, Script_SPlus("Generics_StillPolice1_", foe));
		ok = text && Script_Keep(&r, Script_Ask("Speech", "StartObjSpch", "(OOO)", e, text, foe));
	} else if (t == 0) {
		PyObject *radar = Script_Keep(&r, Script_GetAttr(e, "RadarShow"));
		int c = radar ? Script_NumCmp(radar, 1) : -2;
		if (c != 0 && c != -2) {
			radar = Script_Keep(&r, Script_GetAttr(e, "RadarShow"));
			c = radar ? Script_NumCmp(radar, 2) : -2;
		}
		ok = c != -2 && (c != 0 || Script_Keep(&r, PhoneCab_RevengeSpeech(NULL, args)));
	}
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

static PyObject *PhoneCab_DefUseAudioCom(PyObject *self, PyObject *args) {
	(void)self;
	return Phone_DefUse(args, PhoneCab_UseAudioCom);
}

/* SpawnChar(c, Phone): the character in front of the cab */
static PyObject *PhoneCab_SpawnChar(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Chars", NULL };
	PyObject *c, *phone;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &c, &phone) || !Script_Import(mods)) {
		return NULL;
	}
	scriptRefs_t r = { 0 };
	PyObject *pos = Script_Keep(&r, Script_GetAttr(phone, "Pos"));
	PyObject *ang = pos ? Script_Keep(&r, Script_GetAttr(phone, "Ang")) : NULL;
	PyObject *y = ang ? Script_Keep(&r, Script_Index(ang, 1)) : NULL;
	/* c.Ang reads Phone.Ang again */
	byte ok = y && Phone_Spawn(c, pos, y, 180);
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* the action classes (phonecab.def), $name for a press desktop */
static PyObject *Phone_Class(const char *section, PyObject *name) {
	PyObject *vars = name ? Py_BuildValue("{s:O}", "name", name) : NULL;
	Script_RunFile(0, PHONECAB_FILE, section, phonecab_def, NULL, vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

static PyObject *PhoneCab_CreatePhoneCabClass(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Phone_Class("PhoneCab", NULL);
}

static PyObject *PhoneCab_CreateAudioComClass(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Phone_Class("AudioCom", NULL);
}

/* Init(): a cab and its communicator (in PhoneCabs) at each DM_Phone_* dummy that is on */
static PyObject *PhoneCab_Init(PyObject *self, PyObject *args) {
	static const phoneAttr_t link[] = {
		{ "OffModPos", 'v', 0, NULL, { 0, 0, 0 } },
		{ "DeltaOffMul", 'i', 1, NULL, { 0 } },
		{ "OffMul", 'i', 1, NULL, { 0 } },
		{ "UseLinkAng", 'i', 1, NULL, { 0 } },
		{ "EntityList", 's', 0, "PhoneCabs", { 0 } },
		{ NULL, 0, 0, NULL, { 0 } },
	};
	(void)self;
	(void)args;
	if (!Script_Try("Scrap", "CreateEntityList", "(s)", "PhoneCabs") ||
		!Script_Try("Scrap", "StartDummySearch", "(si)", "DM_Phone_*", 1)) {
		return NULL;
	}
	for (;;) {
		scriptRefs_t r = { 0 };
		PyObject *dummy = Script_Keep(&r, Script_Ask("Scrap", "NextDummySearch", "()"));
		int none = dummy ? Script_Eq(dummy, Py_None) : -1;
		if (none != 0) {
			return Script_Done(&r, none > 0 ? Py_ReturnNone() : NULL);
		}
		int on = Script_True(Script_Keep(&r, Script_Index(dummy, 3)));
		if (on < 0) {
			return Script_Done(&r, NULL);
		}
		if (!on) {
			Script_Done(&r, NULL);
			continue;
		}
		PyObject *d0 = Script_Keep(&r, Script_Index(dummy, 0));
		PyObject *name = d0 ? Script_Keep(&r, Script_PlusS(Script_Ref(d0), "_PhoneCab")) : NULL;
		PyObject *d1 = name ? Script_Keep(&r, Script_Index(dummy, 1)) : NULL;
		PyObject *d2 = d1 ? Script_Keep(&r, Script_Index(dummy, 2)) : NULL;
		PyObject *yang = d2 ? Script_Keep(&r, Script_Index(d2, 1)) : NULL;
		PyObject *a = yang ? Script_Keep(&r, Py_BuildValue("(OOO)", name, d1, yang)) : NULL;
		PyObject *pc = a ? Script_Keep(&r, PhoneCab_CreatePhoneCab(NULL, a)) : NULL;
		/* the communicator reads the dummy again */
		d2 = pc ? Script_Keep(&r, Script_Index(dummy, 2)) : NULL;
		yang = d2 ? Script_Keep(&r, Script_Index(d2, 1)) : NULL;
		a = yang ? Script_Keep(&r, Py_BuildValue("(OOO)", d0, d1, yang)) : NULL;
		PyObject *ac = a ? Script_Keep(&r, PhoneCab_CreateAudioCom(NULL, a)) : NULL;
		PyObject *pcname = ac ? Script_Keep(&r, Script_GetAttr(pc, "Name")) : NULL;
		byte ok = pcname && Script_HSet(ac, "LinkModel", Script_PlusS(Script_Ref(pcname), ",DC_JackCom")) && Phone_Set(ac, link);
		Script_Done(&r, NULL);
		if (!ok) {
			return NULL;
		}
	}
}

/* GetFree(): the cab nearest to the player; without a player a random free one (no radar mark) */
static PyObject *PhoneCab_GetFree(PyObject *self, PyObject *args) {
	scriptRefs_t r = { 0 };
	(void)self;
	(void)args;
	PyObject *usr = Script_Keep(&r, Script_Usr());
	int t = Script_True(usr);
	if (t < 0) {
		return Script_Done(&r, NULL);
	}
	if (t) {
		PyObject *pos = Script_Keep(&r, Script_GetAttr(usr, "Pos"));
		PyObject *a = pos ? Script_Keep(&r, Py_BuildValue("(O)", pos)) : NULL;
		return Script_Done(&r, a ? PhoneCab_GetNearestPhoneCab(NULL, a) : NULL);
	}
	PyObject *len = Script_Keep(&r, Script_Ask("Scrap", "EntityListGet", "(ss)", "PhoneCabs", "Num"));
	PyObject *tenth = len ? Script_Keep(&r, PyFloat_FromDouble(0.1)) : NULL;
	PyObject *top = tenth ? Script_Keep(&r, PyNumber_Subtract(len, tenth)) : NULL;
	PyObject *topf = top ? Script_Keep(&r, PyNumber_Float(top)) : NULL;
	PyObject *rnd = topf ? Script_Keep(&r, PyFloat_FromDouble(Scrap_RandRange(0.0, PyFloat_AsDouble(topf)))) : NULL;
	PyObject *index = rnd ? Script_Keep(&r, Script_Int(rnd)) : NULL;
	if (!index) {
		return Script_Done(&r, NULL);
	}
	for (int pass = 0; pass < 2; pass++) {
		PyObject *name = Script_Ask("Scrap", "EntityListGet", "(ss)", "PhoneCabs", "First");
		PyObject *res = Script_Keep(&r, Script_Entity(name));
		Py_XDECREF(name);
		long i = 0;
		while ((t = Script_True(res)) > 0) {
			if (!pass) {
				PyObject *n = PyInt_FromLong(i);
				int same = Script_Eq(n, index);
				Py_XDECREF(n);
				if (same < 0) {
					return Script_Done(&r, NULL);
				}
				if (same) {
					break;
				}
				i++;
			} else {
				PyObject *radar = Script_Keep(&r, Script_GetAttr(res, "RadarShow"));
				int c = radar ? Script_NumCmp(radar, 0) : -2;
				if (c == -2) {
					return Script_Done(&r, NULL);
				}
				if (c == 0) {
					return Script_Done(&r, Script_Ref(res));
				}
			}
			PyObject *next = Script_Keep(&r, Script_GetAttr(res, "NextInList"));
			res = next ? Script_Keep(&r, Script_Entity(next)) : NULL;
		}
		if (t < 0) {
			return Script_Done(&r, NULL);
		}
		if (!pass) {
			if (!t) {
				break;
			}
			PyObject *radar = Script_Keep(&r, Script_GetAttr(res, "RadarShow"));
			int c = radar ? Script_NumCmp(radar, 0) : -2;
			if (c == -2) {
				return Script_Done(&r, NULL);
			}
			if (c == 0) {
				return Script_Done(&r, Script_Ref(res));
			}
		}
	}
	PyObject *text = PyString_FromString("Aqui no se debe de llegar nunca, siempre se ha de devolver una cabina (res != None)");
	if (text) {
		PyErr_SetObject(text, Py_None);
		Py_DECREF(text);
	}
	return Script_Done(&r, NULL);
}

PyMethodDef g_PhoneCabMethods[] = {
	{ "ForceResetPhoneCab", PhoneCab_ForceResetPhoneCab, 1, NULL },
	{ "RevengeSpeech", PhoneCab_RevengeSpeech, 1, NULL },
	{ "SetRevengeOptions", PhoneCab_SetRevengeOptions, 1, NULL },
	{ "GetNearestPhoneCab", PhoneCab_GetNearestPhoneCab, 1, NULL },
	{ "CreatePhoneCab", PhoneCab_CreatePhoneCab, 1, NULL },
	{ "CreateAudioCom", PhoneCab_CreateAudioCom, 1, NULL },
	{ "DefUseAudioCom", PhoneCab_DefUseAudioCom, 1, NULL },
	{ "UseAudioCom", PhoneCab_UseAudioCom, 1, NULL },
	{ "SpawnChar", PhoneCab_SpawnChar, 1, NULL },
	{ "CreatePhoneCabClass", PhoneCab_CreatePhoneCabClass, 1, NULL },
	{ "CreateAudioComClass", PhoneCab_CreateAudioComClass, 1, NULL },
	{ "Init", PhoneCab_Init, 1, NULL },
	{ "GetFree", PhoneCab_GetFree, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- DTritusDesktop ---- */

static PyObject *DTritus_ForceResetDTritusDesktop(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Phone_Reset("DTritusDesktop", "DTritusDesktop", "DtritusDesktop");
}

/* SpawnChar(c): the character in front of the desktop (DM_DTritusDesktop) */
static PyObject *DTritus_SpawnChar(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Chars", NULL };
	PyObject *c;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &c) || !Script_Import(mods)) {
		return NULL;
	}
	scriptRefs_t r = { 0 };
	PyObject *name = Script_Keep(&r, PyString_FromString("DM_DTritusDesktop"));
	PyObject *dummy = Script_Keep(&r, Phone_Dummy(name));
	PyObject *pos = dummy ? Script_Keep(&r, Script_Index(dummy, 1)) : NULL;
	PyObject *d2 = pos ? Script_Keep(&r, Script_Index(dummy, 2)) : NULL;
	PyObject *y = d2 ? Script_Keep(&r, Script_Index(d2, 1)) : NULL;
	byte ok = y && Phone_Spawn(c, pos, y, -180);
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

static PyObject *DTritus_CreateDTritusDesktopClass(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Phone_Class("DTritusDesktop", NULL);
}

/* CreateDTritusDesktop(name, pos, yAng): the desktop the player talks to */
static PyObject *DTritus_CreateDTritusDesktop(PyObject *self, PyObject *args) {
	static const phoneAttr_t attrs[] = {
		{ "ActCtrl", 's', 0, "DTritusDesktop", { 0 } },
		{ "VRadius", 'i', 60, NULL, { 0 } },
		{ "ArrowCanUse", 'i', 1, NULL, { 0 } },
		{ "Life", 'i', 1, NULL, { 0 } },
		{ "ActiveCMask", 'i', 1, NULL, { 0 } },
		{ "MapCollision", 'i', 1, NULL, { 0 } },
		{ "ArrnOpt", 'i', 0, NULL, { 0 } },
		{ "CamTgt", 'v', 0, NULL, { 0, 225, -50 } },
		{ "ArrOpt0", 't', 0, "DtritusDesktop", { 0 } },
		{ "UseFunc", 's', 0, "DTritusDesktop.DefUseDTritusDesktop", { 0 } },
		{ NULL, 0, 0, NULL, { 0 } },
	};
	PyObject *name, *pos, *yang;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &pos, &yang)) {
		return NULL;
	}
	PyObject *ac = Phone_Create(name, pos, yang, 0);
	if (ac && !Phone_Set(ac, attrs)) {
		Py_DECREF(ac);
		return NULL;
	}
	return ac;
}

/* UseDTritusDesktop(UsrName, CharName, optId, FullOption): using it saves the game; the talk of the mission
 * (DialogController), or Deep Throat's taunt while the alarm is on */
static PyObject *DTritus_UseDTritusDesktop(PyObject *self, PyObject *args) {
	PyObject *usr, *chname, *opt, *full;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &chname, &opt, &full)) {
		return NULL;
	}
	PyObject *e = Script_Keep(&r, Phone_Mark(chname));
	if (!e || !Script_SetGlobal("DTritusDesktop", "DTritusDesktop", Script_Ref(e))) {
		return Script_Done(&r, NULL);
	}
	Script_SetVar("Speech", "OnOutOfRange", Script_Global("DTritusDesktop", "ForceResetDTritusDesktop"));
	PyObject *dc = Script_Keep(&r, Script_Global("DTritusDesktop", "DialogController"));
	int t = Script_True(dc);
	if (t <= 0) {
		return Script_Done(&r, t < 0 ? NULL : Py_ReturnNone());
	}
	PyObject *alarm = Script_Keep(&r, Script_Ask("Scrap", "GetAlarm", "()"));
	t = Script_True(alarm ? Script_Keep(&r, Script_Index(alarm, 0)) : NULL);
	byte ok = t >= 0;
	if (t > 0) {
		ok = Script_Try("Speech", "AddRemoteCom", "(ss)", "Generics_DeepThroatTaunt_1", "DeepThroat");
	} else if (t == 0) {
		dc = Script_Keep(&r, Script_Global("DTritusDesktop", "DialogController"));
		ok = dc && Script_Keep(&r, Script_Invoke(dc, Py_BuildValue("(OOOO)", usr, chname, opt, full)));
	}
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

static PyObject *DTritus_DefUseDTritusDesktop(PyObject *self, PyObject *args) {
	(void)self;
	return Phone_DefUse(args, DTritus_UseDTritusDesktop);
}

/* CreatePressDesktopClass(DesktopName): the class of a journalist's desktop */
static PyObject *DTritus_CreatePressDesktopClass(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	if (!PyString_Check(name)) {
		/* the script fails at its first string concatenation, after SAct.CreateClass */
		PyObject *v = Script_SPlus("Models/Misc/DesktopPress/", name);
		Py_XDECREF(v);
		return NULL;
	}
	return Phone_Class("PressDesktop", name);
}

/* CreatePressDesktop(DesktopName): the desktop at its dummy DM_<DesktopName>, when there is one */
static PyObject *DTritus_CreatePressDesktop(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptRefs_t r = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *dummy = Script_Keep(&r, Phone_Dummy(Script_Keep(&r, Script_SPlus("DM_", name))));
	int t = Script_True(dummy);
	if (t <= 0) {
		return Script_Done(&r, t < 0 ? NULL : Py_ReturnNone());
	}
	PyObject *a = Script_Keep(&r, Py_BuildValue("(O)", name));
	PyObject *pos = a && Script_Keep(&r, DTritus_CreatePressDesktopClass(NULL, a)) ? Script_Keep(&r, Script_Index(dummy, 1)) : NULL;
	PyObject *d2 = pos ? Script_Keep(&r, Script_Index(dummy, 2)) : NULL;
	PyObject *y = d2 ? Script_Keep(&r, Script_Index(d2, 1)) : NULL;
	PyObject *ac = y ? Script_Keep(&r, Phone_Create(name, pos, y, 0)) : NULL;
	byte ok = ac && Script_HSet(ac, "ActCtrl", Script_Ref(name)) && Script_HSet(ac, "MapCollision", PyInt_FromLong(1)) &&
		Script_HSet(ac, "VRadius", PyInt_FromLong(60));
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* Init(): Dtritus' desktop at DM_DTritusDesktop (when the dummy is on) and the three desktops of the journalists */
static PyObject *DTritus_Init(PyObject *self, PyObject *args) {
	static const char *const desks[] = { "BertoDesktop", "BettyDesktop", "HumphreyDesktop" };
	scriptRefs_t r = { 0 };
	(void)self;
	(void)args;
	PyObject *dummy = Script_Keep(&r, Phone_Dummy(Script_Keep(&r, PyString_FromString("DM_DTritusDesktop"))));
	int t = Script_True(dummy);
	if (t > 0) {
		t = Script_True(Script_Keep(&r, Script_Index(dummy, 3)));
	}
	if (t > 0) {
		PyObject *d0 = Script_Keep(&r, Script_Index(dummy, 0));
		PyObject *d1 = d0 ? Script_Keep(&r, Script_Index(dummy, 1)) : NULL;
		PyObject *d2 = d1 ? Script_Keep(&r, Script_Index(dummy, 2)) : NULL;
		PyObject *y = d2 ? Script_Keep(&r, Script_Index(d2, 1)) : NULL;
		PyObject *a = y && Script_Keep(&r, DTritus_CreateDTritusDesktopClass(NULL, NULL)) ?
			Script_Keep(&r, Py_BuildValue("(OOO)", d0, d1, y)) : NULL;
		PyObject *desk = a ? DTritus_CreateDTritusDesktop(NULL, a) : NULL;
		t = desk && Script_SetGlobal("DTritusDesktop", "DTritusDesktop", desk) ? 1 : -1;
	}
	for (int i = 0; t >= 0 && i < 3; i++) {
		PyObject *a = Script_Keep(&r, Py_BuildValue("(s)", desks[i]));
		t = a && Script_Keep(&r, DTritus_CreatePressDesktop(NULL, a)) ? t : -1;
	}
	return Script_Done(&r, t >= 0 ? Py_ReturnNone() : NULL);
}

/* InitNPCS(): the journalists whose default map (save variable Journalist.<name>_Defaut) is this one */
static PyObject *DTritus_InitNPCS(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "CharsNPC", "string", "Init", NULL };
	static const char *const names[] = { "Humphrey", "Betty", "Berto" };
	(void)self;
	(void)args;
	if (!Script_Import(mods)) {
		return NULL;
	}
	for (int i = 0; i < 3; i++) {
		scriptRefs_t r = { 0 };
		char var[64];
		Str_Printf(var, sizeof(var), "Journalist.%s_Defaut", names[i]);
		PyObject *sv = Script_Keep(&r, Script_SaveVar(var));
		PyObject *map = sv ? Script_Keep(&r, Script_Ask("string", "upper", "(O)", sv)) : NULL;
		PyObject *path = map ? Script_Keep(&r, Script_Attr("Init", "Path")) : NULL;
		PyObject *tail = path ? Script_Keep(&r, PySequence_GetSlice(path, 7, 0x7fffffff)) : NULL;
		PyObject *here = tail ? Script_Keep(&r, Script_Ask("string", "upper", "(O)", tail)) : NULL;
		int t = here ? Script_Eq(map, here) : -1;
		if (t > 0) {
			Str_Printf(var, sizeof(var), "%s_Default", names[i]);
			t = Script_Try("CharsNPC", "CreateMissionChar", "(s)", var) ? 1 : -1;
		}
		Script_Done(&r, NULL);
		if (t < 0) {
			return NULL;
		}
	}
	return Py_ReturnNone();
}

PyMethodDef g_DTritusDesktopMethods[] = {
	{ "ForceResetDTritusDesktop", DTritus_ForceResetDTritusDesktop, 1, NULL },
	{ "SpawnChar", DTritus_SpawnChar, 1, NULL },
	{ "CreateDTritusDesktopClass", DTritus_CreateDTritusDesktopClass, 1, NULL },
	{ "CreateDTritusDesktop", DTritus_CreateDTritusDesktop, 1, NULL },
	{ "DefUseDTritusDesktop", DTritus_DefUseDTritusDesktop, 1, NULL },
	{ "UseDTritusDesktop", DTritus_UseDTritusDesktop, 1, NULL },
	{ "CreatePressDesktopClass", DTritus_CreatePressDesktopClass, 1, NULL },
	{ "CreatePressDesktop", DTritus_CreatePressDesktop, 1, NULL },
	{ "Init", DTritus_Init, 1, NULL },
	{ "InitNPCS", DTritus_InitNPCS, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
