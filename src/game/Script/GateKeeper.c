/*
 * The gameplay functions of the GateKeeper character in C (scripts/chars/gatekeeper.py, issue #2; Init is
 * data/chars/gatekeeper.def, Characters.c): the doors a gatekeeper guards (SetDoor, DoorFoe, UseDoor), his talk
 * (GateKeeperSpeech, EndOfSpeech, RestoreState) and OnCreate, which gives every gatekeeper an instance of the script's
 * class GateKeeperType (FoeType, MPass, Door, CanPassFunc, ClosedFunc) in GateKeepers.
 *
 * The module variables stay in the script, the maps and missions read and set them: GateKeeperType, GateKeepers,
 * GKDoors, the foe groups (PoliceGroupGateKeeper and the others), HashGroup, LocalCallFunc, LocalParams and
 * AfterGateKeeperSpeech.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values and read the variables where the script
 * reads them as its globals; GateKeepers and GKDoors change in place, LocalCallFunc and LocalParams are written at
 * once. The gatekeeper objects of GateKeepers, the door functions and the callbacks are the objects themselves
 * (OBJLIB 5 (9c) and 7: they are set and called the Python way), and DoorFoe keeps the foe group it gets (the
 * group tuples of the module keep their identity). Entities are handle values; Scrap.GetEntity, GetAlarm,
 * ConsoleError and SScorer.SetMsgText go through their seams (OBJLIB 7: fc-gk-po records or swaps them).
 *
 * Direct calls (stage 3): Elevator and Speech through their refs (PORT_CALL: the port seam, OBJLIB 5 (9c)). apply of
 * LocalCallFunc and the door functions take the entry when the value is a port's own function (stage 4:
 * MODE_CBAPPLYSEQV, Mode_CallArgs), else they are called the Python way; EndOfSpeech is put into Speech as the object
 * it is (OBJLIB 5 (9c)).
 */
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define GK "GateKeeper"

/* GateKeepers[name] into gc (the object itself, OBJLIB 5 (9c: an instance)) */
static byte Gk_At(const nativeVal_t *name, nativeVal_t *gc) {
	nativeVal_t gks = { 0 };
	byte ok = Mode_G(GK, "GateKeepers", &gks) && Mode_Sub(&gks, name, gc);
	Native_Free(&gks);
	return ok;
}

/* fn(door, name) of a door function (the entry of a port's own function, else the value called the Python way) */
static byte Gk_DoorCall(const nativeVal_t *fn, const nativeVal_t *door, const nativeVal_t *name) {
	nativeVal_t args[2];
	Native_Copy(&args[0], door);
	Native_Copy(&args[1], name);
	byte ok = Mode_CallArgs(fn, NULL, args, 2);
	Native_Free(&args[0]);
	Native_Free(&args[1]);
	return ok;
}

/* UseDoor(UsrName, ElevatorName, optId, FullOption): a door a gatekeeper guards opens only without the alarm and for
 * the characters of its foe type; 1 when the elevator was used */
static byte Gk_UseDoor(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *gks = Mode_V(&t), *doors = Mode_V(&t), *key = Mode_V(&t), *gc = Mode_V(&t), *foe = Mode_V(&t);
	nativeVal_t *empty = Mode_V(&t), *e = Mode_V(&t), *ctrl = Mode_V(&t);
	if (!Mode_Import("Elevator")) {
		return Mode_Done(&t, 0);
	}
	int has = Mode_HasKey(GK, "GKDoors", 1, &a[1]);
	if (has < 0) {
		return Mode_Done(&t, 0);
	}
	if (has) {
		/* FoeType = GateKeepers[GKDoors[ElevatorName]].FoeType */
		if (!Mode_G(GK, "GateKeepers", gks) || !Mode_G(GK, "GKDoors", doors) || !Mode_Sub(doors, &a[1], key) ||
			!Mode_Sub(gks, key, gc) || !Mode_VObj(gc, "FoeType", foe)) {
			return Mode_Done(&t, 0);
		}
		int refused = Mode_AlarmOn();
		if (refused > 0) {
			Native_SetInt(out, 0);
			return Mode_Done(&t, Mode_Label(1, "Generics_GateKeeper_SystemMessage2", 5));
		}
		if (refused == 0) {
			Native_SetEmpty(empty, NV_TUPLE);
			refused = Mode_Cmp(foe, "!=", empty);
		}
		if (refused > 0) {
			int in = Mode_EntitySeam(&a[0], e) && Mode_VObj(e, "ActCtrl", ctrl) ? Mode_In(foe, ctrl) : -1;
			refused = in < 0 ? -1 : !in;
		}
		if (refused) {
			Native_SetInt(out, 0);
			return Mode_Done(&t, refused > 0 && Mode_Label(1, "Generics_GateKeeper_SystemMessage1", 5));
		}
	}
	if (!PORT_CALL(Elevator, UseElevator, NULL, "vvvv", &a[0], &a[1], &a[2], &a[3])) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(out, 1);
	return Mode_Done(&t, 1);
}

/* OnCreate(EntityName): invulnerable, the morphing eyes, a new GateKeeperType in GateKeepers */
static byte Gk_OnCreate(const nativeVal_t *name) {
	nativeVal_t e = { 0 }, cls = { 0 }, gc = { 0 };
	byte ok = Mode_EntitySeam(name, &e) && Mode_VSetI(&e, "Invulnerable", 1) && Mode_VSetS(&e, "Action", "MorphEyes") &&
		Mode_G(GK, "GateKeeperType", &cls) && Mode_CallArgs(&cls, &gc, NULL, 0) &&
		Mode_DictPut(GK, "GateKeepers", NULL, name, &gc);
	Native_Free(&e);
	Native_Free(&cls);
	Native_Free(&gc);
	return ok;
}

/* SetDoor(EntityName, DoorName, CanPassFunc=DoNothingFunc, ClosedFunc=DoNothingFunc): the gatekeeper guards the door,
 * which starts closed (ClosedFunc); pass and closed NULL when not given */
static byte Gk_SetDoor(const nativeVal_t *name, const nativeVal_t *door, const nativeVal_t *pass, const nativeVal_t *closed) {
	nativeVal_t defPass = { 0 }, defClosed = { 0 }, d = { 0 };
	/* the defaults: the attribute DoNothingFunc of the module when the call comes, as the C port read it */
	byte ok = (pass || Mode_ModGet(GK, "DoNothingFunc", &defPass)) && (closed || Mode_ModGet(GK, "DoNothingFunc", &defClosed));
	pass = pass ? pass : &defPass;
	closed = closed ? closed : &defClosed;
	ok = ok && Mode_AtSet(GK, "GateKeepers", name, "Door", door) && Mode_AtSet(GK, "GateKeepers", name, "CanPassFunc", pass) &&
		Mode_AtSet(GK, "GateKeepers", name, "ClosedFunc", closed) && Mode_DictPut(GK, "GKDoors", NULL, door, name) &&
		Mode_EntitySeam(door, &d) && Gk_DoorCall(closed, &d, name);
	Native_Free(&defPass);
	Native_Free(&defClosed);
	Native_Free(&d);
	return ok;
}

/* GetGateKeeperId(EntityName): Init.Path[7:] + "." + EntityName. The script does not import Init, so the name is
 * the module's own function Init, which has no Path: it raises AttributeError like the script */
static byte Gk_GetGateKeeperId(const nativeVal_t *name, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *init = Mode_V(&t), *path = Mode_V(&t), *rest = Mode_V(&t);
	return Mode_Done(&t, Mode_G(GK, "Init", init) && Mode_VObj(init, "Path", path) && Native_Slice(path, 7, 0x7fffffff, rest) &&
		Mode_Cat(&t, out, 3, rest, Mode_S(&t, "."), name));
}

/* RestoreState(UsrName): the speech aborted: the door closed again */
static byte Gk_RestoreState(const nativeVal_t *usr) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *gc = Mode_V(&t), *name = Mode_V(&t), *door = Mode_V(&t), *fn = Mode_V(&t);
	return Mode_Done(&t, Mode_Import("Speech") && PORT_CALL(Speech, RestoreState, NULL, "v", usr) && Gk_At(usr, gc) &&
		Mode_VObj(gc, "Door", name) && Mode_EntitySeam(name, door) && Mode_VObj(gc, "ClosedFunc", fn) &&
		Gk_DoorCall(fn, door, usr));
}

/* EndOfSpeech(): the gatekeeper idles again; the door callback of the talk (LocalCallFunc) runs once */
static byte Gk_EndOfSpeech(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *npc = Mode_V(&t), *ch = Mode_V(&t), *fn = Mode_V(&t), *params = Mode_V(&t), *none = Mode_V(&t);
	if (!Mode_Import("Speech") || !Mode_ModGet("Speech", "NPCName", npc) || !Mode_EntitySeam(npc, ch) ||
		(Native_True(ch) && !Mode_VSetI(ch, "LLogicIdle", 0)) || !Mode_G(GK, "LocalCallFunc", fn)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(fn)) {
		return Mode_Done(&t, 1);
	}
	/* apply(LocalCallFunc, LocalParams): the entry while the function is a port's own, the arguments as the builtin
	 * takes them (MODE_CBAPPLYSEQV) */
	return Mode_Done(&t, Mode_Read(GK, "LocalCallFunc", fn) && Mode_G(GK, "LocalParams", params) &&
		MODE_CBAPPLYSEQV(fn, params, NULL) && Mode_Put(GK, "LocalCallFunc", NULL, none) &&
		Mode_Put(GK, "LocalParams", NULL, none));
}

/* the talk that opens the door: the answer, EndOfSpeech at its end with the door callback, the abort restores the
 * door */
static byte Gk_Open(const nativeVal_t *ch, const nativeVal_t *gc, const nativeVal_t *door, const nativeVal_t *chName,
	const char *ans) {
	nativeVal_t fn = { 0 }, params;
	Native_SetEmpty(&params, NV_TUPLE);
	Native_Copy(Native_Append(&params), door);
	Native_Copy(Native_Append(&params), chName);
	byte ok = Mode_Spch("Generics_GateKeeper_InitMsg1", ans, "TalkNPC") &&
		Mode_ModSetGlobal("Speech", "OnEndOfSpeech", GK, "EndOfSpeech") && Mode_VObj(gc, "CanPassFunc", &fn) &&
		Mode_Put(GK, "LocalCallFunc", NULL, &fn) && Mode_Put(GK, "LocalParams", NULL, &params) &&
		Mode_VSetS(ch, "AbortSelectFunc", "GateKeeper.RestoreState");
	Native_Free(&fn);
	Native_Free(&params);
	return ok;
}

/* if AfterGateKeeperSpeech: AfterGateKeeperSpeech() */
static byte Gk_After(void) {
	byte called;
	return Mode_CallIf(GK, "AfterGateKeeperSpeech", &called);
}

/* GateKeeperSpeech(UsrName, CharName, optId, FullOption): the speech routine: the gatekeeper opens for his foe type
 * (then AllreadyOpen), else names the group he waits for */
static byte Gk_GateKeeperSpeech(const nativeVal_t *usrName, const nativeVal_t *chName, const nativeVal_t *opt) {
	static const char *const mods[] = { "Speech", "Chars", "Challenge", "Menu", "string" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *none = Mode_V(&t), *ch = Mode_V(&t), *usr = Mode_V(&t), *gc = Mode_V(&t), *name = Mode_V(&t);
	nativeVal_t *door = Mode_V(&t), *foe = Mode_V(&t), *empty = Mode_V(&t), *ctrl = Mode_V(&t), *keys = Mode_V(&t);
	nativeVal_t *hash = Mode_V(&t), *grp = Mode_V(&t), *ans = Mode_V(&t), *rep = Mode_V(&t), *msg = Mode_V(&t);
	for (int i = 0; i < 5; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_Put(GK, "LocalCallFunc", NULL, none) || !Mode_Put(GK, "LocalParams", NULL, none) ||
		!Mode_EntitySeam(chName, ch) || !Mode_EntitySeam(usrName, usr) || !LvN_Speakers(usrName, chName) ||
		!Gk_At(chName, gc) || !Mode_VObj(gc, "Door", name) || !Mode_EntitySeam(name, door)) {
		return Mode_Done(&t, 0);
	}
	int talk = Mode_Cmp(opt, "==", Mode_S(&t, "Talk"));
	if (talk < 0) {
		return Mode_Done(&t, 0);
	}
	if (!talk) {
		int again = Mode_Cmp(opt, "==", Mode_S(&t, "AllreadyOpen"));
		return Mode_Done(&t, again >= 0 && (!again || Gk_Open(ch, gc, door, chName, "Generics_GateKeeper_InitAns2")));
	}
	/* (gc.FoeType == ()) or (usr.ActCtrl in gc.FoeType) */
	Native_SetEmpty(empty, NV_TUPLE);
	int open = Mode_VObj(gc, "FoeType", foe) ? Mode_Cmp(foe, "==", empty) : -1;
	if (open == 0) {
		Native_Free(foe);
		open = Mode_VObj(usr, "ActCtrl", ctrl) && Mode_VObj(gc, "FoeType", foe) ? Mode_In(foe, ctrl) : -1;
	}
	if (open < 0) {
		return Mode_Done(&t, 0);
	}
	if (open) {
		return Mode_Done(&t, Mode_VSetI(ch, "ArrnOpt", 0) && LvN_SetLangOpt(ch, "ArrOpt0", "AllreadyOpen:", "TalkOpt") &&
			Gk_Open(ch, gc, door, chName, "Generics_GateKeeper_InitAns1") && Gk_After());
	}
	/* elif gc.FoeType in HashGroup.keys() */
	Native_Free(foe);
	int known = Mode_VObj(gc, "FoeType", foe) && Mode_Keys(GK, "HashGroup", 1, keys) ? Mode_In(keys, foe) : -1;
	if (known < 0) {
		return Mode_Done(&t, 0);
	}
	Native_Free(foe);
	if (known) {
		return Mode_Done(&t, Mode_G(GK, "HashGroup", hash) && Mode_VObj(gc, "FoeType", foe) && Mode_Sub(hash, foe, grp) &&
			Mode_Cat(&t, ans, 2, Mode_S(&t, "Generics_GateKeeper_InitAns"), grp) &&
			PORT_CALL(Speech, OnSpch, NULL, "svs", "Generics_GateKeeper_InitMsg1", ans, "TalkNoNPC") &&
			Mode_ModSetGlobal("Speech", "OnEndOfSpeech", GK, "EndOfSpeech") && Gk_After());
	}
	if (!Mode_VObj(gc, "FoeType", foe)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(foe, rep);
	return Mode_Done(&t, Mode_Cat(&t, msg, 3, Mode_S(&t, "G A T E K E E P E R    E R R O R : "), rep,
		Mode_S(&t, " is an invalid foe type.\n")) && Mode_ConsoleError(msg));
}

/* DoorFoe(EntityName, FoeType): the characters the gatekeeper lets pass. FoeType is stored as the value it is: an
 * NV_OBJ is the caller's object (the letter P, the adapter), a native value becomes a new object */
static byte GateKeeper_DoorFoe_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t zero;
	(void)out;
	if (n != 2) {
		return Port_Arity(n, 2);
	}
	Native_SetInt(&zero, 0);
	return Mode_AtSet(GK, "GateKeepers", &a[0], "FoeType", &a[1]) && Mode_AtSet(GK, "GateKeepers", &a[0], "MPass", &zero);
}

/* DoorFoe through Python: FoeType (argument 1) kept as the object it is (OBJLIB 5 (7e: Mode_EntryObj): the
 * group list GateKeeper.PoliceGroupGateKeeper itself) */
static PyObject *GateKeeper_DoorFoe(PyObject *self, PyObject *args) {
	(void)self;
	return Mode_EntryObj(args, GateKeeper_DoorFoe_E, 1 << 1);
}

MODE_KW(GateKeeper, SetDoor, "OO|OO", "EntityName", "DoorName", "CanPassFunc", "ClosedFunc") {
	return Gk_SetDoor(&v[0], &v[1], given[2] ? &v[2] : NULL, given[3] ? &v[3] : NULL);
}

MODE_FN(GateKeeper, UseDoor, 4, Gk_UseDoor(a, out))
MODE_FN(GateKeeper, OnCreate, 1, Gk_OnCreate(&a[0]))
MODE_FN(GateKeeper, GetGateKeeperId, 1, Gk_GetGateKeeperId(&a[0], out))
MODE_FN(GateKeeper, RestoreState, 1, Gk_RestoreState(&a[0]))
MODE_FN(GateKeeper, EndOfSpeech, 0, Gk_EndOfSpeech())
MODE_FN(GateKeeper, GateKeeperSpeech, 4, Gk_GateKeeperSpeech(&a[0], &a[1], &a[2]))

PyMethodDef g_GateKeeperFuncs[] = {
	{ "UseDoor", GateKeeper_UseDoor, 1, NULL },
	{ "OnCreate", GateKeeper_OnCreate, 1, NULL },
	{ "DoNothingFunc", Mode_PyNone2, 1, NULL },
	{ "SetDoor", (PyCFunction)(void (*)(void))GateKeeper_SetDoor, 3, NULL },
	{ "DoorFoe", GateKeeper_DoorFoe, 1, NULL },
	{ "GetGateKeeperId", GateKeeper_GetGateKeeperId, 1, NULL },
	{ "RestoreState", GateKeeper_RestoreState, 1, NULL },
	{ "EndOfSpeech", GateKeeper_EndOfSpeech, 1, NULL },
	{ "GateKeeperSpeech", GateKeeper_GateKeeperSpeech, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(GateKeeper)
