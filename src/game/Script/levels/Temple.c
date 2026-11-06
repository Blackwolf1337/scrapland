/*
 * The level Temple in C (levels/temple/scripts, issue #2): the functions of Map, MapExtraLifes and the missions
 * ArchbishopDead, AskGus, MeetArchbishop and TalkToDeepThroat that are logic; their plain functions are data
 * (the files in data/levels/temple). The module variables (the discardable characters of Map, Lifes and TimeToSpawn,
 * the proofs of ArchbishopDead, FXSceneYakuzziFile) stay in the scripts.
 */
#include "scrap/script.h"

#define K(o) Script_Keep(&r, (o))
#define FAIL() return Script_Done(&r, NULL)
#define BAD() return (Script_Done(&r, NULL), 0)

/* fn(args) through the globals of module (args given away), the result dropped */
static byte Tp_Do(const char *module, const char *fn, PyObject *args) {
	PyObject *f = Script_Global(module, fn), *v = f ? Script_Invoke(f, args) : NULL;
	if (!f) {
		Py_XDECREF(args);
	}
	Py_XDECREF(v);
	Py_XDECREF(f);
	return v != NULL;
}

/* e.UseFunc = func of the entity called name */
static byte Tp_UseFunc(const char *name, const char *func) {
	PyObject *e = Script_Ask("Scrap", "GetEntity", "(s)", name);
	byte ok = e && Script_HSet(e, "UseFunc", PyString_FromString((char *)func));
	Py_XDECREF(e);
	return ok;
}

/* a gatekeeper on its door against a foe group of GateKeeper, the door used through UseDoor */
static byte Tp_Gate(const char *keeper, const char *door, const char *group, const char *use) {
	PyObject *g = Script_Import((const char *const[]){ "GateKeeper", NULL }) ? Script_Attr("GateKeeper", group) : NULL;
	byte ok = g && (!door || Script_Try("GateKeeper", "SetDoor", "(ss)", keeper, door)) &&
		Script_Try("GateKeeper", "DoorFoe", "(sO)", keeper, g) && Tp_UseFunc(door ? door : "DM_Door_Teleporter_01", use);
	Py_XDECREF(g);
	return ok;
}

/* ---- Map ---- */

static PyObject *TpMap_InitStatus(PyObject *self, PyObject *args) {
	static const struct {
		const char *who, *flag;
		int n;
	} repos[] = { { "BankDirector", "_BankDirector", 3 }, { "Bishop", "_Bishop", 5 }, { "Functionary", "_Functionary", 3 } };
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path) || !Script_Import((const char *const[]){ "Init", "Elevator", "CharsNPC", "Messenger", NULL })) {
		return NULL;
	}
	Script_SetVar("Init", "Outdoor", PyInt_FromLong(0));
	Script_SetVar("Init", "OutArea", PyString_FromString("Commercial"));
	Script_SetVar("Messenger", "MakeMDP", PyInt_FromLong(0));
	if (!Script_Try("Elevator", "SetDef", "(ss)", "Metro", "DM_Door_Metro_Templo00") ||
		!Script_Try("Elevator", "SetDef", "(ss)", "MetroExt", "DM_Door_Metro_Templo00") ||
		!Script_Try("Elevator", "SetDef", "(ss)", "Elevator", "DM_Door_Elevator_Commercial") ||
		!Script_Try("Elevator", "SetDef", "(ss)", "TempleTeleporter", "DM_Door_TempleTeleporter_GDB") ||
		!Script_Try("CharsNPC", "SetReposChars", "(si)", "Sentinel", 3) || !Script_Try("CharsNPC", "SetReposChars", "(si)", "Police", 3) ||
		!Script_Try("CharsNPC", "SetReposChars", "(si)", "Nurse", 3)) {
		return NULL;
	}
	/* if not Scrap.Get("IsXbox") or _<Char>: */
	for (int i = 0; i < 3; i++) {
		PyObject *x = Script_Ask("Scrap", "Get", "(s)", "IsXbox");
		int on = x ? Script_True(x) : -1;
		Py_XDECREF(x);
		on = on < 0 ? -1 : !on;
		if (on == 0) {
			PyObject *f = Script_Global("Map", repos[i].flag);
			on = f ? Script_True(f) : -1;
			Py_XDECREF(f);
		}
		if (on < 0 || (on && !Script_Try("CharsNPC", "SetReposChars", "(si)", repos[i].who, repos[i].n))) {
			return NULL;
		}
	}
	return Py_ReturnNone();
}

static PyObject *TpMap_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path) || !Script_Import((const char *const[]){ "Metro", "Elevator", NULL }) ||
		!Script_Try("Elevator", "ConnectElevators", "([s][s])", "DM_Door_TempleTeleporter_GDB", "GDB") ||
		!Script_Try("Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_Commercial", "Commercial") ||
		!Script_Try("Elevator", "ConnectElevators", "([ss][]i)", "DM_Door_Teleporter_01", "DM_Door_Teleporter_06", 0) ||
		!Script_Try("Metro", "SetAsMetro", "(s)", "DM_Door_Metro_Templo00") ||
		!Tp_Gate("GateKeeper_Portalis", "DM_Door_Teleporter_01", "BishopGroupGateKeeper", "GateKeeper.UseDoor")) {
		return NULL;
	}
	PyObject *from = Script_SaveVar("ComeFrom");
	if (!from) {
		return NULL;
	}
	byte arrive = Script_Is(from, "DoorTempleTeleporter");
	Py_DECREF(from);
	if (arrive) {
		PyObject *f = Script_Global("Map", "EndOfAppears");
		if (!f) {
			return NULL;
		}
		Script_SetVar("Elevator", "OnElevatorReachDown", f);
	}
	if (!Tp_Gate("GateKeeper_GDB", "DM_Door_TempleTeleporter_GDB", "BishopGroupGateKeeper", "GateKeeper.UseDoor")) {
		return NULL;
	}
	PyObject *lib = Script_SaveVar("Mission.Library");
	if (!lib) {
		return NULL;
	}
	byte meet = Script_Is(lib, "Mission_MeetArchbishop");
	Py_DECREF(lib);
	return !meet || Script_Try("Scrap", "Set", "(si)", "isActionIndoor", 0) ? Py_ReturnNone() : NULL;
}

/* EndOfAppears(elv): the arrival by the teleporter from GDB, the player on the floor at its spawn */
static PyObject *TpMap_EndOfAppears(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *elv;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &elv) || !Script_Import((const char *const[]){ "Elevator", NULL })) {
		return NULL;
	}
	Script_SetVar("Elevator", "OnElevatorReachDown", Script_Ref(Py_None));
	PyObject *dummy = Script_Try("Scrap", "StartDummySearch", "(si)", "DM_Player_Spawn_GDB", 0) ? K(Script_Ask("Scrap", "NextDummySearch", "()")) : NULL;
	PyObject *v = dummy ? K(Script_Usr()) : NULL, *name = v && Script_HSet(v, "Pos", Script_Index(dummy, 1)) ? K(Script_GetAttr(v, "Name")) : NULL;
	return Script_Done(&r, name && Script_Try("SLogic", "SetOnFloor", "(O)", name) ? Py_ReturnNone() : NULL);
}

PyMethodDef g_TempleMapMethods[] = {
	{ "InitStatus", TpMap_InitStatus, 1, NULL },
	{ "Init", TpMap_Init, 1, NULL },
	{ "EndOfAppears", TpMap_EndOfAppears, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapExtraLifes: the extra lives of the temple, back after TimeToSpawn; raised while the alarm is on ---- */

#define XL "MapExtraLifes"

/* float(Scrap.GetSaveVar(name)) (a new reference) */
static PyObject *Tp_SaveFloat(PyObject *name) {
	PyObject *s = PyString_Check(name) ? Script_SaveVar(PyString_AsString(name)) : Script_Ask("Scrap", "GetSaveVar", "(O)", name);
	PyObject *f = s ? PyNumber_Float(s) : NULL;
	Py_XDECREF(s);
	return f;
}

/* TakeExtraLife(ItemName, UsrName): a life (up to the most), the life away for TimeToSpawn, the alarm */
static PyObject *Xl_TakeExtraLife(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *item, *usr;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &item, &usr) || !Script_Import((const char *const[]){ "Sound", "Speech", NULL })) {
		return NULL;
	}
	PyObject *l = K(Script_Entity(item)), *nl = l && Script_Try("Scrap", "IncSaveVar", "(s)", "Stats.TempleLife") ? K(Script_SaveVar("Player.NumLives")) : NULL;
	PyObject *lives = nl ? K(Script_Int(nl)) : NULL, *gt = lives ? K(Tp_SaveFloat(K(PyString_FromString("GameplayTime")))) : NULL;
	PyObject *now = gt ? K(Script_Ask("Scrap", "GetTime", "()")) : NULL, *t = now ? K(PyNumber_Add(gt, now)) : NULL;
	PyObject *rep = t ? K(Script_Repr(t)) : NULL, *max = rep && Script_Try("Scrap", "SetSaveVar", "(OO)", item, rep) ? K(Script_Ask("Scrap", "Get", "(s)", "MaxPlayerLives")) : NULL;
	int c = max ? PyObject_Compare(lives, max) : 0;
	if (!max || PyErr_Occurred()) {
		FAIL();
	}
	if (c < 0) {
		PyObject *one = K(PyInt_FromLong(1)), *more = one ? K(PyNumber_Add(lives, one)) : NULL, *mr = more ? K(Script_Repr(more)) : NULL;
		PyObject *cad = mr && Script_Try("Scrap", "SetSaveVar", "(sO)", "Player.NumLives", mr) ? K(Script_LangOf(PyString_FromString("StealLife_SystemMessage"))) : NULL;
		PyObject *t2 = cad ? K(Script_Ask("Scrap", "GetTime", "()")) : NULL, *dur = t2 ? K(Script_Ask("Speech", "CalculateSpeechTime", "(O)", cad)) : NULL;
		PyObject *until = dur ? K(PyNumber_Add(t2, dur)) : NULL;
		if (!until || !Script_Try("SScorer", "SetMsgText", "(OO)", cad, until) || !Script_Try("SFX", "BishopSellLife", "(sO)", "", usr)) {
			FAIL();
		}
	}
	PyObject *spawn = K(Script_Global(XL, "TimeToSpawn")), *take = spawn && Script_HSet(l, "rTime", Script_Ref(spawn)) ? K(Script_Attr("Sound", "ItemTake")) : NULL;
	PyObject *a = take ? K(PyObject_GetItem(take, K(PyString_FromString("TakeExtraLife")))) : NULL;
	PyObject *a0 = a ? K(Script_Index(a, 0)) : NULL, *a1 = a0 ? K(Script_Index(a, 1)) : NULL, *pos = a1 ? K(Script_GetAttr(l, "Pos")) : NULL;
	byte ok = pos && Script_Try("SSound", "PlaySound", "(OOO)", pos, a0, a1) && Script_Try("Scrap", "SetAlarm", "(i)", 1) &&
		Script_Try("Scrap", "Set", "(si)", "BishopAttackOnAlarm", 1);
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* SetRespawnTime(l): the life comes back TimeToSpawn after it was taken (at least 0.1 s from now) */
static PyObject *Xl_SetRespawnTime(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *l;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &l)) {
		return NULL;
	}
	PyObject *name = K(Script_GetAttr(l, "Name")), *taken = name ? K(Tp_SaveFloat(name)) : NULL;
	PyObject *gt = taken ? K(Tp_SaveFloat(K(PyString_FromString("GameplayTime")))) : NULL, *a = gt ? K(PyNumber_Subtract(taken, gt)) : NULL;
	PyObject *spawn = a ? K(Script_Global(XL, "TimeToSpawn")) : NULL, *b = spawn ? K(PyNumber_Add(a, spawn)) : NULL;
	PyObject *now = b ? K(Script_Ask("Scrap", "GetTime", "()")) : NULL, *rt = now ? K(PyNumber_Subtract(b, now)) : NULL;
	PyObject *lim = rt ? K(PyFloat_FromDouble(0.1)) : NULL;
	int c = lim ? PyObject_Compare(rt, lim) : 0;
	if (!lim || PyErr_Occurred()) {
		FAIL();
	}
	return Script_Done(&r, Script_HSet(l, "rTime", Script_Ref(c < 0 ? lim : rt)) ? Py_ReturnNone() : NULL);
}

/* MoveLifes(dist): the lives up or down by dist; one that is there glides, a taken one jumps */
static PyObject *Xl_MoveLifes(PyObject *self, PyObject *args) {
	PyObject *dist;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &dist)) {
		return NULL;
	}
	PyObject *lifes = Script_Global(XL, "Lifes"), *l;
	int more = lifes ? 1 : -1;
	for (int i = 0; lifes && (more = Script_Next(lifes, i, &l)) > 0; i++) {
		scriptRefs_t r = { .n = 0 };
		K(l);
		PyObject *mp = K(Script_GetAttr(l, "MovePos"));
		PyObject *to = mp ? K(Script_Ask("SVec", "Add", "(O(iOi))", mp, 0, dist, 0)) : NULL;
		PyObject *rt = to && Script_HSet(l, "MovePos", Script_Ref(to)) && Script_HSet(l, "MoveAng", Script_GetAttr(l, "Ang")) ?
			K(Script_GetAttr(l, "rTime")) : NULL;
		PyObject *zero = rt ? K(PyFloat_FromDouble(0.0)) : NULL;
		int there = zero ? PyObject_Compare(rt, zero) == 0 : -1;
		byte ok = there >= 0 && !PyErr_Occurred() && (there ? Tp_Do(XL, "SetRespawnTime", Py_BuildValue("(O)", l)) &&
			Script_HSet(l, "MoveTime", PyFloat_FromDouble(0.3)) : Script_HSet(l, "MoveTime", PyFloat_FromDouble(0.0)) &&
			Script_HSet(l, "Pos", Script_GetAttr(l, "MovePos")));
		Script_Done(&r, NULL);
		if (!ok) {
			more = -1;
			break;
		}
	}
	Py_XDECREF(lifes);
	return more == 0 ? Py_ReturnNone() : NULL;
}

/* SwitchAlarm(status): called again on the next alarm change; the lives up with the alarm, down after it */
static PyObject *Xl_SwitchAlarm(PyObject *self, PyObject *args) {
	PyObject *status;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &status) || !Script_Import((const char *const[]){ "Alarm", NULL })) {
		return NULL;
	}
	PyObject *cbs = Script_Attr("Alarm", "SetAlarmCallback"), *me = cbs ? Script_Global(XL, "SwitchAlarm") : NULL;
	PyObject *v = me ? Script_Method(cbs, "append", "(O)", me) : NULL;
	Py_XDECREF(v);
	Py_XDECREF(me);
	Py_XDECREF(cbs);
	int on = v ? Script_True(status) : -1;
	if (on < 0) {
		return NULL;
	}
	if (on) {
		return Tp_Do(XL, "MoveLifes", Py_BuildValue("(i)", 600)) ? Py_ReturnNone() : NULL;
	}
	return Script_Try("Scrap", "Set", "(si)", "BishopAttackOnAlarm", 0) && Tp_Do(XL, "MoveLifes", Py_BuildValue("(i)", -600)) ?
		Py_ReturnNone() : NULL;
}

/* the life item of a dummy (name, pos, ang): its save variable, the item, its respawn time, into Lifes */
static byte Xl_Life(PyObject *dummy) {
	scriptRefs_t r = { .n = 0 };
	PyObject *name = K(Script_Index(dummy, 0)), *spawn = name ? K(Script_Global(XL, "TimeToSpawn")) : NULL;
	PyObject *neg = spawn ? K(PyNumber_Negative(spawn)) : NULL, *rep = neg ? K(Script_Repr(neg)) : NULL;
	if (!rep || !Script_Try("Scrap", "CreateSaveVar", "(OO)", name, rep) || !Tp_Do(XL, "GetMoney", Py_BuildValue("(O)", name))) {
		BAD();
	}
	PyObject *pos = K(Script_Index(dummy, 1)), *x = pos ? K(Script_Index(pos, 0)) : NULL;
	PyObject *y = x ? K(Script_Index(pos, 1)) : NULL, *z = y ? K(Script_Index(pos, 2)) : NULL;
	PyObject *l = z ? K(Script_Ask("Scrap", "CreateEntity", "(OOOOs)", name, x, y, z, "Item")) : NULL;
	PyObject *model = l ? K(Script_Ask("MisItems", "GetObjModel", "(s)", "ExtraLife")) : NULL;
	if (!model || !K(Script_Method(l, "SetModel", "(O)", model)) || !Tp_Do(XL, "SetRespawnTime", Py_BuildValue("(O)", l)) ||
		!Script_HSet(l, "OnlyUser", PyInt_FromLong(1)) || !Script_HSet(l, "Radius", Script_Attr("MisItems", "COIN_RADIUS_INDOOR")) ||
		!Script_HSet(l, "OnTake", PyString_FromString("MapExtraLifes.TakeExtraLife")) ||
		!Script_HSet(l, "MovePos", Script_GetAttr(l, "Pos")) || !Script_HSet(l, "MoveAng", Script_GetAttr(l, "Ang"))) {
		BAD();
	}
	PyObject *lifes = K(Script_Global(XL, "Lifes"));
	byte ok = lifes && PyList_Check(lifes) ? PyList_Append(lifes, l) == 0 : lifes && K(Script_Method(lifes, "append", "(O)", l));
	Script_Done(&r, NULL);
	return ok;
}

/* Init(LevelPath): a life at every DM_ExtraLife dummy; up at once when the alarm is on, else when it comes */
static PyObject *Xl_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path) || !Script_Import((const char *const[]){ "MisItems", "Alarm", NULL }) ||
		!Script_Try("Scrap", "StartDummySearch", "(si)", "DM_ExtraLife_*", 1)) {
		return NULL;
	}
	PyObject *key = PyString_FromString("GameplayTime"), *gt = key ? Tp_SaveFloat(key) : NULL;
	Py_XDECREF(key);
	if (!gt) {
		return NULL;
	}
	Py_DECREF(gt);
	for (;;) {
		PyObject *d = Script_Ask("Scrap", "NextDummySearch", "()");
		int end = d ? Script_Eq(d, Py_None) : -1;
		byte ok = end == 0 ? Xl_Life(d) : end > 0;
		Py_XDECREF(d);
		if (!ok) {
			return NULL;
		}
		if (end) {
			break;
		}
	}
	PyObject *alarm = Script_Ask("Scrap", "GetAlarm", "()"), *a0 = alarm ? Script_Index(alarm, 0) : NULL;
	int on = a0 ? Script_True(a0) : -1;
	Py_XDECREF(a0);
	Py_XDECREF(alarm);
	if (on < 0) {
		return NULL;
	}
	if (on) {
		return Tp_Do(XL, "SwitchAlarm", Py_BuildValue("(i)", 1)) ? Py_ReturnNone() : NULL;
	}
	PyObject *cbs = Script_Attr("Alarm", "SetAlarmCallback"), *me = cbs ? Script_Global(XL, "SwitchAlarm") : NULL;
	PyObject *v = me ? Script_Method(cbs, "append", "(O)", me) : NULL;
	Py_XDECREF(v);
	Py_XDECREF(me);
	Py_XDECREF(cbs);
	return v ? Py_ReturnNone() : NULL;
}

/* GetMoney(name): the number after the second "_" of the name */
static PyObject *Xl_GetMoney(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name) || !Script_Import((const char *const[]){ "string", NULL })) {
		return NULL;
	}
	PyObject *parts = Script_Ask("string", "splitfields", "(Os)", name, "_"), *p = parts ? Script_Index(parts, 2) : NULL;
	PyObject *f = p ? PyNumber_Float(p) : NULL;
	Py_XDECREF(p);
	Py_XDECREF(parts);
	return f;
}

PyMethodDef g_TempleXlMethods[] = {
	{ "TakeExtraLife", Xl_TakeExtraLife, 1, NULL },
	{ "SetRespawnTime", Xl_SetRespawnTime, 1, NULL },
	{ "MoveLifes", Xl_MoveLifes, 1, NULL },
	{ "SwitchAlarm", Xl_SwitchAlarm, 1, NULL },
	{ "Init", Xl_Init, 1, NULL },
	{ "GetMoney", Xl_GetMoney, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the missions ---- */

/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, fn, args[, name]) with the time of the script (args given away) */
static byte Tp_At(double delay, PyObject *fn, PyObject *args, const char *name) {
	PyObject *now = fn && args ? Script_Ask("Scrap", "GetTime", "()") : NULL, *d = now ? PyFloat_FromDouble(delay) : NULL;
	PyObject *t = d ? PyNumber_Add(now, d) : NULL;
	byte ok = t && (name ? Script_Try("Scrap", "AddScheduledFunc", "(OOOs)", t, fn, args, name) : Script_Try("Scrap", "AddScheduledFunc", "(OOO)", t, fn, args));
	Py_XDECREF(t);
	Py_XDECREF(d);
	Py_XDECREF(now);
	Py_XDECREF(args);
	return ok;
}

/* the hint text of key for its speech time */
static byte Tp_Hint(const char *key) {
	PyObject *cad = Script_LangOf(PyString_FromString((char *)key)), *now = cad ? Script_Ask("Scrap", "GetTime", "()") : NULL;
	PyObject *dur = now ? Script_Ask("Speech", "CalculateSpeechTime", "(O)", cad) : NULL, *t = dur ? PyNumber_Add(now, dur) : NULL;
	byte ok = t && Script_Try("SScorer", "SetMsgText", "(OO)", cad, t);
	Py_XDECREF(t);
	Py_XDECREF(dur);
	Py_XDECREF(now);
	Py_XDECREF(cad);
	return ok;
}

#define AD "Mission_ArchbishopDead"

/* Init(LevelPath): the forbidden area, the gatekeeper against the police; the photos of the proofs or Deep Throat */
static PyObject *Ad_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path) || !Script_Try("Scrap", "Set", "(ss)", "GuaranteedChars", ",Police,Gear,Sentinel,PoliceBoss,") ||
		!Tp_Do(AD, "ForbiddenMsg", PyTuple_New(0)) ||
		!Tp_Gate("GateKeeper_Portalis", "DM_Door_Teleporter_01", "PoliceGroupGateKeeper", "GateKeeper.UseDoor")) {
		return NULL;
	}
	PyObject *st = Script_SaveVar("Mission_ArchbishopDead.Status");
	if (!st) {
		return NULL;
	}
	byte photo = Script_Is(st, "Photo");
	Py_DECREF(st);
	if (!photo) {
		return Tp_Do(AD, "ShowDeepThroatMessage", PyTuple_New(0)) ? Py_ReturnNone() : NULL;
	}
	PyObject *f = Script_Try("Speech", "AddRemoteCom", "(ss)", "Mission_ArchbishopKilled_RemoteMessage7", "Humphrey") ? Script_Global(AD, "TakeProofPhoto") : NULL;
	if (!f) {
		return NULL;
	}
	Script_SetVar("CharAct", "TakenPhoto", f);
	return Tp_Do(AD, "CreateViscProofs", PyTuple_New(0)) ? Py_ReturnNone() : NULL;
}

/* ForbiddenMsg(): the forbidden area label, again each second */
static PyObject *Ad_ForbiddenMsg(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *time = K(Script_Ask("Scrap", "GetTime", "()")), *label = time ? K(Script_LangOf(PyString_FromString("Generics_ForbiddenArea_Label"))) : NULL;
	PyObject *half = label ? K(PyFloat_FromDouble(0.5)) : NULL, *t1 = half ? K(PyNumber_Add(time, half)) : NULL;
	PyObject *one = t1 && Script_Try("SScorer", "SetLabelText", "(OO)", label, t1) ? K(PyFloat_FromDouble(1.0)) : NULL;
	PyObject *t2 = one ? K(PyNumber_Add(time, one)) : NULL, *f = t2 ? K(Script_Global(AD, "ForbiddenMsg")) : NULL;
	return Script_Done(&r, f && Script_Try("Scrap", "AddScheduledFunc", "(OO()s)", t2, f, "ForbiddenMsg") ? Py_ReturnNone() : NULL);
}

/* CreateViscProofs(): the three proofs to photograph at the DM_ViscProof dummies */
static PyObject *Ad_CreateViscProofs(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "") || !Script_Try("Scrap", "StartDummySearch", "(si)", "DM_ViscProof*", 1)) {
		return NULL;
	}
	for (int i = 1; i < 4; i++) {
		scriptRefs_t r = { .n = 0 };
		PyObject *d = K(Script_Ask("Scrap", "NextDummySearch", "()"));
		int has = d ? Script_True(d) : -1;
		if (has == 0) {
			PyObject *s = K(PyString_FromString("Uno de los dummies de las pruebas del viscoson no existe"));
			if (s) {
				PyErr_SetObject(s, Py_None);
			}
			FAIL();
		}
		PyObject *p = has > 0 ? K(Script_Index(d, 1)) : NULL, *a = p ? K(Script_Index(d, 2)) : NULL, *c[5] = { NULL };
		for (int k = 0; a && k < 5; k++) {
			c[k] = K(Script_Index(k < 3 ? p : a, k < 3 ? k : k - 3));
			if (!c[k]) {
				FAIL();
			}
		}
		char name[0x20];
		Str_Printf(name, sizeof(name), "ViscProof%d", i);
		PyObject *prf = c[4] ? K(Script_Ask("MissionsFuncs", "CreatePhotoObject", "(s(OOO)(OOi)s)", name, c[0], c[1], c[2], c[3], c[4], 0,
			"Mission_ArchbishopDead.ViewProof")) : NULL;
		PyObject *pos = prf ? K(Script_GetAttr(prf, "Pos")) : NULL, *v = pos ? K(Script_Ask("SVec", "Add", "(O(iii))", pos, 0, -25, 0)) : NULL;
		PyObject *names = v && Script_HSet(prf, "MovePos", Script_Ref(v)) && Script_HSet(prf, "Pos", Script_Ref(v)) ? K(Script_Global(AD, "ProofsNames")) : NULL;
		PyObject *pn = names ? K(Script_GetAttr(prf, "Name")) : NULL;
		if (!pn || !K(Script_Method(names, "append", "(O)", pn))) {
			FAIL();
		}
		Script_Done(&r, NULL);
	}
	return Py_ReturnNone();
}

static PyObject *Ad_ViewProof(PyObject *self, PyObject *args) {
	PyObject *a, *b, *c, *d;
	(void)self;
	return PyArg_ParseTuple(args, "OOOO", &a, &b, &c, &d) && Tp_Hint("Mission_ArchbishopDead_ViewProofHint") ? Py_ReturnNone() : NULL;
}

/* TakeProofPhoto(proofName): a photo of a proof (1), the third ends the photos; 0 for another object */
static PyObject *Ad_TakeProofPhoto(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *proof;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &proof)) {
		return NULL;
	}
	PyObject *names = K(Script_Global(AD, "ProofsNames"));
	int in = names ? PySequence_Contains(names, proof) : -1;
	if (in <= 0) {
		return Script_Done(&r, in < 0 ? NULL : PyInt_FromLong(0));
	}
	PyObject *e = K(Script_Entity(proof)), *n = e && Script_Try("MissionsFuncs", "UnsetPhotoObject", "(O)", e) ? K(Script_Global(AD, "NumProofsWithPhoto")) : NULL;
	PyObject *one = n ? K(PyInt_FromLong(1)) : NULL, *m = one ? PyNumber_Add(n, one) : NULL;
	if (!m || !Script_SetGlobal(AD, "NumProofsWithPhoto", Script_Ref(m))) {
		Py_XDECREF(m);
		FAIL();
	}
	int three = Script_NumCmp(K(m), 3);
	if (three == -2) {
		FAIL();
	}
	if (three == 0) {
		Script_SetVar("CharAct", "TakenPhoto", Script_Ref(Py_None));
		PyObject *add = K(Script_Attr("Speech", "AddRemoteCom")), *show = add ? K(Script_Global(AD, "ShowDeepThroatMessage")) : NULL;
		if (!show || !Tp_At(1.0, add, Py_BuildValue("(ssO)", "Mission_ArchbishopKilled_RemoteMessage2", "Humphrey", show), NULL) ||
			!Script_Try("Scrap", "SetSaveVar", "(ss)", "Mission_ArchbishopDead.Status", "DeepThroat")) {
			FAIL();
		}
	}
	return Script_Done(&r, PyInt_FromLong(1));
}

/* AfterDeepThroatMessageShown(): the first time the next mission, then Deep Throat calls again each minute */
static PyObject *Ad_AfterDeepThroatMessageShown(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *marked = Script_Global(AD, "NextMissionMarked");
	int first = marked ? Script_NumCmp(marked, 0) : -2;
	Py_XDECREF(marked);
	if (first == -2) {
		return NULL;
	}
	const char *msg = "Mission_FirstMeetDeepThroat_RemoteMessage5";
	if (first == 0) {
		msg = "Mission_FirstMeetDeepThroat_RemoteMessage4";
		if (!Script_SetGlobal(AD, "NextMissionMarked", PyInt_FromLong(1)) || !Script_Import((const char *const[]){ "Init", NULL }) ||
			!Script_Try("SaveGame", "QWayPoint", "()") ||
			!Script_Try("MissionsFuncs", "SetNextMission", "(ss)", "Mission_FirstMeetDeepThroat", "Commercial") ||
			!Script_Try("SaveGame", "MMDesc", "(si)", "Mission_FirstMeetDeepthroat_MissionText1", 1) ||
			!Script_Try("Scrap", "DelSaveVars", "(s)", "Mission_ArchbishopDead")) {
			return NULL;
		}
	}
	PyObject *add = Script_Attr("Speech", "AddRemoteCom"), *me = add ? Script_Global(AD, "AfterDeepThroatMessageShown") : NULL;
	byte ok = me && Tp_At(60.0, add, Py_BuildValue("(ssO)", msg, "DeepThroat", me), NULL);
	Py_XDECREF(me);
	Py_XDECREF(add);
	return ok ? Py_ReturnNone() : NULL;
}

PyMethodDef g_TempleArchbishopDeadMethods[] = {
	{ "Init", Ad_Init, 1, NULL },
	{ "ForbiddenMsg", Ad_ForbiddenMsg, 1, NULL },
	{ "CreateViscProofs", Ad_CreateViscProofs, 1, NULL },
	{ "ViewProof", Ad_ViewProof, 1, NULL },
	{ "TakeProofPhoto", Ad_TakeProofPhoto, 1, NULL },
	{ "AfterDeepThroatMessageShown", Ad_AfterDeepThroatMessageShown, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* Deep Throat's desktop as the target, its dialog by module.DeepThroatSpeech (CallDeepThroat of AskGus, Init of
 * TalkToDeepThroat) */
static byte Tp_CallDeepThroat(const char *module) {
	PyObject *dd = Script_Import((const char *const[]){ "DTritusDesktop", NULL }) ? Script_Ask("Scrap", "GetEntity", "(s)", "DM_DTritusDesktop") : NULL;
	PyObject *name = dd ? Script_GetAttr(dd, "Name") : NULL;
	PyObject *f = name && Script_Try("MissionsFuncs", "SetMissionTarget", "(O)", name) ? Script_Global(module, "DeepThroatSpeech") : NULL;
	Py_XDECREF(name);
	Py_XDECREF(dd);
	if (f) {
		Script_SetVar("DTritusDesktop", "DialogController", f);
	}
	return f != NULL;
}

/* DeepThroatSpeech(UsrName, CharName, optId, FullOption) of both missions: the talk at the desktop, then
 * module.SetNextMission */
static PyObject *Tp_DeepThroatSpeech(const char *module, PyObject *args, const char *ni) {
	PyObject *usr, *ch, *opt, *full;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	PyObject *dd = Script_Ask("Scrap", "GetEntity", "(s)", "DM_DTritusDesktop");
	PyObject *next = dd && Script_Try("MissionsFuncs", "ActivateTarget", "(Oi)", dd, 0) ? Script_Global(module, "SetNextMission") : NULL;
	Py_XDECREF(dd);
	if (!next) {
		return NULL;
	}
	Script_SetVar("Speech", "PlayerName", Script_Ref(usr));
	Script_SetVar("Speech", "NPCName", Script_Ref(ch));
	Script_SetVar("Speech", "OnEndOfSpeech", next);
	Script_SetVar("Speech", "OnEndOfSpeechParams", PyTuple_New(0));
	return Script_Try("Speech", "NISpeech", "(s)", "DeepThroat") && Script_Try("Speech", "AutoAddNI", "(s)", ni) &&
		Script_Try("Speech", "RunNI", "()") ? Py_ReturnNone() : NULL;
}

#define AG "Mission_AskGus"

/* l = eval(Scrap.GetSaveVar(var)); item added when missing, the list saved back */
static byte Ag_AddToList(const char *var, const char *item) {
	scriptRefs_t r = { .n = 0 };
	PyObject *text = K(Script_SaveVar(var)), *l = text ? K(Script_EvalIn(AG, text)) : NULL, *it = l ? K(PyString_FromString((char *)item)) : NULL;
	int in = it ? PySequence_Contains(l, it) : -1;
	byte ok = in > 0 || (in == 0 && K(Script_Method(l, "append", "(O)", it)) && Script_Try("Scrap", "SetSaveVar", "(sO)", var, K(Script_Repr(l))));
	Script_Done(&r, NULL);
	return ok;
}

static PyObject *Ag_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	PyObject *list = Script_Attr("CharsNPC", "AfterBrowseNPC"), *f = list ? Script_Global(AG, "AfterBrowseNPC") : NULL;
	PyObject *v = f ? Script_Method(list, "append", "(O)", f) : NULL;
	Py_XDECREF(v);
	Py_XDECREF(f);
	Py_XDECREF(list);
	return v ? Py_ReturnNone() : NULL;
}

static PyObject *Ag_AfterBrowseNPC(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *res = K(Script_Attr("CharsNPC", "CharRes")), *gus = res ? K(PyObject_GetItem(res, K(PyString_FromString("Bishop_Gus")))) : NULL;
	PyObject *cbs = gus ? K(Script_GetAttr(gus, "CallbackFunc")) : NULL, *f = cbs ? K(Script_Global(AG, "CreateGusInit")) : NULL;
	return Script_Done(&r, f && K(Script_Method(cbs, "append", "(O)", f)) ? Py_ReturnNone() : NULL);
}

/* CreateGusInit(e): Gus talks about the mission while it is on */
static PyObject *Ag_CreateGusInit(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *e;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	PyObject *st = K(Script_SaveVar("MissionState"));
	if (!st) {
		FAIL();
	}
	if (!Script_Is(st, "AskGus")) {
		return Script_Done(&r, Py_ReturnNone());
	}
	PyObject *name = Script_HSet(e, "UseFunc", PyString_FromString("Mission_AskGus.GusSpeech")) &&
		Script_HSet(e, "Descriptor", Script_LangOf(PyString_FromString("CharName_Gus"))) ? K(Script_GetAttr(e, "Name")) : NULL;
	return Script_Done(&r, name && Script_Try("MissionsFuncs", "SetMissionTarget", "(O)", name) ? Py_ReturnNone() : NULL);
}

static PyObject *Ag_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	PyObject *st = Script_SaveVar("MissionState");
	if (!st) {
		return NULL;
	}
	const char *fn = Script_Is(st, "DeepThroatCalls") ? "DeepThroatCalls" : Script_Is(st, "AfterDeepThroatCalls") ? "CallDeepThroat" : NULL;
	Py_DECREF(st);
	return !fn || Tp_Do(AG, fn, PyTuple_New(0)) ? Py_ReturnNone() : NULL;
}

/* DeepThroatAppears(CharName): Gus as before, the bishop's ship and engine as blueprints, Deep Throat calls */
static PyObject *Ag_DeepThroatAppears(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *e = Script_Entity(name);
	int live = e ? Script_True(e) : -1;
	byte ok = live >= 0 && (!live || Script_HSet(e, "UseFunc", PyString_FromString("Bishop.BishopSpeech")));
	Py_XDECREF(e);
	return ok && Script_Try("MissionsFuncs", "SetMissionTarget", "(O)", Py_None) && Ag_AddToList("Hangar.shipsToEditList", "SBishop1") &&
		Ag_AddToList("Hangar.availableEnginesList", "MBISHOP1") && Script_Try("Scrap", "IncSaveVar", "(si)", "NewBluePrintAvaliable", 2) &&
		Tp_Hint("Mission_TalkToGus_SystemMessage1") && Script_Try("Scrap", "SetSaveVar", "(ss)", "MissionState", "DeepThroatCalls") &&
		Tp_Do(AG, "DeepThroatCalls", PyTuple_New(0)) ? Py_ReturnNone() : NULL;
}

/* PreCallDeepThroat(): Dtritus can no longer be converted to; the call to Deep Throat */
static PyObject *Ag_PreCallDeepThroat(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "") || !Script_Import((const char *const[]){ "CharConversor", NULL })) {
		return NULL;
	}
	PyObject *text = K(Script_SaveVar("Conversor.AvailableChars")), *chars = text ? K(Script_EvalIn(AG, text)) : NULL;
	PyObject *dt = chars ? K(PyString_FromString("Dtritus")) : NULL;
	int in = dt ? PySequence_Contains(chars, dt) : -1;
	if (in < 0 || (in && !K(Script_Method(chars, "remove", "(O)", dt))) ||
		!Script_Try("Scrap", "SetSaveVar", "(sO)", "Conversor.AvailableChars", K(Script_Repr(chars)))) {
		FAIL();
	}
	PyObject *av = K(Script_Attr("CharConversor", "AvailableChars"));
	in = av ? PySequence_Contains(av, dt) : -1;
	if (in < 0 || (in && !K(Script_Method(av, "remove", "(O)", dt))) ||
		!Script_Try("SaveGame", "MMDesc", "(si)", "Mission_TalkToDeepThroat4_MissionText1", 1) ||
		!Script_Try("Scrap", "SetSaveVar", "(ss)", "MissionState", "AfterDeepThroatCalls") || !Tp_Do(AG, "CallDeepThroat", PyTuple_New(0))) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

static PyObject *Ag_CallDeepThroat(PyObject *self, PyObject *args) {
	(void)self;
	return PyArg_ParseTuple(args, "") && Tp_CallDeepThroat(AG) ? Py_ReturnNone() : NULL;
}

static PyObject *Ag_DeepThroatSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Tp_DeepThroatSpeech(AG, args, "Mission_TalkToDeepThroat4__ComDeepThroat_NI_1Msg");
}

PyMethodDef g_TempleAskGusMethods[] = {
	{ "InitStatus", Ag_InitStatus, 1, NULL },
	{ "AfterBrowseNPC", Ag_AfterBrowseNPC, 1, NULL },
	{ "CreateGusInit", Ag_CreateGusInit, 1, NULL },
	{ "Init", Ag_Init, 1, NULL },
	{ "DeepThroatAppears", Ag_DeepThroatAppears, 1, NULL },
	{ "PreCallDeepThroat", Ag_PreCallDeepThroat, 1, NULL },
	{ "CallDeepThroat", Ag_CallDeepThroat, 1, NULL },
	{ "DeepThroatSpeech", Ag_DeepThroatSpeech, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

#define TD "Mission_TalkToDeepThroat"

static PyObject *Td_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	return PyArg_ParseTuple(args, "O", &path) && Tp_CallDeepThroat(TD) ? Py_ReturnNone() : NULL;
}

static PyObject *Td_DeepThroatSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Tp_DeepThroatSpeech(TD, args, "Mission_TalkToDeepThroat5_DeepThroat_NI_1Msg");
}

PyMethodDef g_TempleTalkToDeepThroatMethods[] = {
	{ "Init", Td_Init, 1, NULL },
	{ "DeepThroatSpeech", Td_DeepThroatSpeech, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

#define MA "Mission_MeetArchbishop"

/* UseDoor(...): through the gatekeeper's door; the scene starts when the player walks again */
static PyObject *Ma_UseDoor(PyObject *self, PyObject *args) {
	PyObject *usr, *elv, *opt, *full;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &elv, &opt, &full)) {
		return NULL;
	}
	PyObject *ok = Script_Ask("GateKeeper", "UseDoor", "(OOOO)", usr, elv, opt, full);
	int used = ok ? Script_True(ok) : -1;
	Py_XDECREF(ok);
	if (used <= 0) {
		return used < 0 ? NULL : Py_ReturnNone();
	}
	PyObject *f = Script_Try("MissionsFuncs", "SetMissionTarget", "(s)", "") ? Script_Global(MA, "TestEndOfElevator") : NULL;
	byte done = f && Tp_At(0.1, f, PyTuple_New(0), NULL);
	Py_XDECREF(f);
	return done ? Py_ReturnNone() : NULL;
}

/* TestEndOfElevator(): the Yakuzzi scene once the player walks out of the teleporter */
static PyObject *Ma_TestEndOfElevator(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *set = Script_Ask("SInput", "GetActionSet", "()");
	if (!set) {
		return NULL;
	}
	byte walk = Script_Is(set, "Walk");
	Py_DECREF(set);
	if (!walk) {
		PyObject *f = Script_Global(MA, "TestEndOfElevator");
		byte ok = f && Tp_At(0.1, f, PyTuple_New(0), NULL);
		Py_XDECREF(f);
		return ok ? Py_ReturnNone() : NULL;
	}
	PyObject *next = Script_Global(MA, "SetNextMission");
	if (!next) {
		return NULL;
	}
	Script_SetVar("Scene", "EndOfSceneCallback", next);
	PyObject *head = Script_Try("Scene", "Play", "(s)", "Yakuzzi") ? Script_Ask("Scrap", "Get", "(s)", "NewsHead") : NULL;
	PyObject *e = head ? Script_Entity(head) : NULL;
	byte ok = e && Script_HSet(e, "Active", PyInt_FromLong(0));
	Py_XDECREF(e);
	Py_XDECREF(head);
	return ok ? Py_ReturnNone() : NULL;
}

/* CreateFakeSpeech(): the archbishop of the news panel in his yakuzzi, and the panel itself */
static PyObject *Ma_CreateFakeSpeech(PyObject *self, PyObject *args) {
	static const struct {
		const char *var;
		int v;
	} panel[] = { { "NewsPanelShowHead", 1 }, { "NewsPanelDistance", 300 }, { "NewsPanelDisplacement", 35 }, { "NewsPanelMove", -30 },
		{ "NewsPanelX", 20 }, { "NewsPanelY", 20 }, { "NewsPanelW", 270 }, { "NewsPanelH", 180 } };
	scriptRefs_t r = { .n = 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "") || !Script_Try("SAct", "CreateClass", "(s)", "FakeSpeech") ||
		!Script_Try("SAct", "SetCls", "(ss)", "ModelFilePath", "Levels/Temple/Map/Yakuzzi/ArchBishop.M3D") ||
		!Script_Try("SAct", "CreateAction", "(s)", "Relax") ||
		!Script_Try("SAct", "SetAct", "(ss)", "AnmFilePath", "Levels/Temple/Map/Yakuzzi/YakuzziAnm/ArchBishop_Speech.M3D") ||
		!Script_Try("SAct", "SetAct", "(si)", "isLoop", 1) || !Script_Try("SAct", "SetCls", "(ss)", "DefAction", "Relax") ||
		!Script_Try("Scrap", "StartDummySearch", "(si)", "DM_Scene_Yakuzzi", 0)) {
		return NULL;
	}
	PyObject *d = K(Script_Ask("Scrap", "NextDummySearch", "()")), *kb = NULL;
	int none = d ? Script_Eq(d, Py_None) : -1;
	if (none < 0) {
		FAIL();
	}
	if (!none) {
		PyObject *p = K(Script_Index(d, 1)), *x = p ? K(Script_Index(p, 0)) : NULL, *y = x ? K(Script_Index(p, 1)) : NULL;
		PyObject *z = y ? K(Script_Index(p, 2)) : NULL;
		kb = z ? K(Script_Ask("Scrap", "CreateEntity", "(sOOOs)", "FakeSpeech", x, y, z, "WithLife")) : NULL;
		if (!kb || !Script_HSet(kb, "Ang", Py_BuildValue("(iii)", 0, 0, 0)) || !Script_HSet(kb, "ActCtrl", PyString_FromString("FakeSpeech"))) {
			FAIL();
		}
	}
	if (!Script_Try("Scrap", "Set", "(ss)", "NewsLogoBmp", "2D/News/LogoTemple.bmp") ||
		!Script_Try("Scrap", "Set", "(ss)", "BackNewsBmp", "2D/News/BackTemple.bmp")) {
		FAIL();
	}
	if (!kb) {
		PyErr_SetString(PyExc_NameError, "kb");
		FAIL();
	}
	PyObject *kn = K(Script_GetAttr(kb, "Name"));
	if (!kn || !Script_Try("Scrap", "Set", "(sO)", "NewsHead", kn)) {
		FAIL();
	}
	for (size_t i = 0; i < sizeof(panel) / sizeof(panel[0]); i++) {
		if (!Script_Try("Scrap", "Set", "(si)", panel[i].var, panel[i].v)) {
			FAIL();
		}
	}
	PyObject *back = K(Script_Ask("Scrap", "Get", "(s)", "BackNewsBmp"));
	PyObject *logo = back && Script_Try("SScorer", "PreloadTexture", "(O)", back) ? K(Script_Ask("Scrap", "Get", "(s)", "NewsLogoBmp")) : NULL;
	byte ok = logo && Script_Try("SScorer", "PreloadTexture", "(O)", logo) && Script_Try("SSound", "SetMusic", "(sd)", "Music/ArchBishop.ogg", 1.0);
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

/* Init(LevelPath): the gatekeeper against Dtritus, the door through UseDoor, the scene and the news panel */
static PyObject *Ma_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	return PyArg_ParseTuple(args, "O", &path) && Script_Try("MissionsFuncs", "SetMissionTarget", "(s)", "DM_Door_Teleporter_01") &&
		Tp_Gate("GateKeeper_Portalis", NULL, "DtritusGroupGateKeeper", "Mission_MeetArchbishop.UseDoor") &&
		Tp_Do(MA, "LoadYakuzziScene", PyTuple_New(0)) && Tp_Do(MA, "CreateFakeSpeech", PyTuple_New(0)) ? Py_ReturnNone() : NULL;
}

PyMethodDef g_TempleMeetArchbishopMethods[] = {
	{ "UseDoor", Ma_UseDoor, 1, NULL },
	{ "TestEndOfElevator", Ma_TestEndOfElevator, 1, NULL },
	{ "CreateFakeSpeech", Ma_CreateFakeSpeech, 1, NULL },
	{ "Init", Ma_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
