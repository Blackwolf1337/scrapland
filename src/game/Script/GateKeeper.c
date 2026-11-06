/*
 * The gameplay functions of the GateKeeper character in C (scripts/chars/gatekeeper.py, issue #2; Init is
 * data/chars/gatekeeper.def, Characters.c): the doors a gatekeeper guards (SetDoor, DoorFoe, UseDoor), his talk
 * (GateKeeperSpeech, EndOfSpeech, RestoreState) and OnCreate, which gives every gatekeeper an instance of the script's
 * class GateKeeperType (FoeType, MPass, Door, CanPassFunc, ClosedFunc) in GateKeepers.
 *
 * The module variables stay in the script, the maps and missions read and set them: GateKeeperType, GateKeepers,
 * GKDoors, the foe groups (PoliceGroupGateKeeper and the others), HashGroup, LocalCallFunc, LocalParams and
 * AfterGateKeeperSpeech.
 */
#include "scrap/scrap.h"
#include "scrap/script.h"

#define GK "GateKeeper"

#define G(name) Sf_Global(&f, GK, name)

/* SScorer.SetMsgText(Scrap.GetLangStr(key), Scrap.GetTime() + 5) */
static void Gk_Msg(scriptFrame_t *f, const char *key) {
	Sf_Ask(f, "SScorer", "SetMsgText", "(OO)", Sf_Lang(f, Sf_Str(f, key)), Sf_OpI(f, Sf_Time(f), '+', 5));
}

/* UseDoor(UsrName, ElevatorName, optId, FullOption): a door a gatekeeper guards opens only without the alarm and for
 * the characters of its foe type; 1 when the elevator was used */
static PyObject *GateKeeper_UseDoor(PyObject *self, PyObject *args) {
	PyObject *usr, *elev, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &elev, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Elevator");
	PyObject *doors = G("GKDoors");
	if (Sf_True(&f, Sf_Method(&f, doors, "has_key", "(O)", elev)) > 0) {
		PyObject *gc = Sf_GetItem(&f, G("GateKeepers"), Sf_GetItem(&f, G("GKDoors"), elev));
		PyObject *foe = Sf_Attr(&f, gc, "FoeType");
		if (Sf_True(&f, Sf_Item(&f, Sf_Ask(&f, "Scrap", "GetAlarm", "()"), 0)) > 0) {
			Gk_Msg(&f, "Generics_GateKeeper_SystemMessage2");
			return Sf_Return(&f, Sf_Int(&f, 0));
		}
		int refused = Sf_Compare(&f, foe, "!=", Sf_Build(&f, "()"));
		if (refused > 0) {
			refused = Sf_In(&f, Sf_Attr(&f, Sf_Entity(&f, usr), "ActCtrl"), foe);
			refused = refused < 0 ? -1 : !refused;
		}
		if (refused > 0) {
			Gk_Msg(&f, "Generics_GateKeeper_SystemMessage1");
			return Sf_Return(&f, Sf_Int(&f, 0));
		}
	}
	Sf_Ask(&f, "Elevator", "UseElevator", "(OOOO)", usr, elev, opt, full);
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* OnCreate(EntityName): invulnerable, the morphing eyes, a new GateKeeperType in GateKeepers */
static PyObject *GateKeeper_OnCreate(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, name);
	Sf_SetAttrI(&f, e, "Invulnerable", 1);
	Sf_SetAttrS(&f, e, "Action", "MorphEyes");
	PyObject *gc = Sf_Call(&f, G("GateKeeperType"), "()");
	Sf_SetItem(&f, G("GateKeepers"), name, gc);
	return Sf_None(&f);
}

/* DoNothingFunc(Door, CharName): the default door callbacks */
static PyObject *GateKeeper_DoNothingFunc(PyObject *self, PyObject *args) {
	PyObject *door, *ch;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &door, &ch)) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* SetDoor(EntityName, DoorName, CanPassFunc=DoNothingFunc, ClosedFunc=DoNothingFunc): the gatekeeper guards the door,
 * which starts closed (ClosedFunc) */
static PyObject *GateKeeper_SetDoor(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "EntityName", "DoorName", "CanPassFunc", "ClosedFunc", NULL };
	PyObject *name, *door, *pass = NULL, *closed = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|OO", names, &name, &door, &pass, &closed)) {
		return NULL;
	}
	/* the defaults: DoNothingFunc of the module, as the def line bound it when the script loaded */
	pass = pass ? Sf_Ref(&f, pass) : Sf_Mod(&f, GK, "DoNothingFunc");
	closed = closed ? Sf_Ref(&f, closed) : Sf_Mod(&f, GK, "DoNothingFunc");
	Sf_SetAttr(&f, Sf_GetItem(&f, G("GateKeepers"), name), "Door", door);
	Sf_SetAttr(&f, Sf_GetItem(&f, G("GateKeepers"), name), "CanPassFunc", pass);
	Sf_SetAttr(&f, Sf_GetItem(&f, G("GateKeepers"), name), "ClosedFunc", closed);
	Sf_SetItem(&f, G("GKDoors"), door, name);
	Sf_Call(&f, closed, "(OO)", Sf_Entity(&f, door), name);
	return Sf_None(&f);
}

/* DoorFoe(EntityName, FoeType): the characters the gatekeeper lets pass */
static PyObject *GateKeeper_DoorFoe(PyObject *self, PyObject *args) {
	PyObject *name, *foe;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &foe)) {
		return NULL;
	}
	Sf_SetAttr(&f, Sf_GetItem(&f, G("GateKeepers"), name), "FoeType", foe);
	Sf_SetAttrI(&f, Sf_GetItem(&f, G("GateKeepers"), name), "MPass", 0);
	return Sf_None(&f);
}

/* GetGateKeeperId(EntityName): Init.Path[7:] + "." + EntityName. The script does not import Init, so the name is
 * the module's own function Init, which has no Path: it raises AttributeError like the script */
static PyObject *GateKeeper_GetGateKeeperId(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *path = Sf_Slice(&f, Sf_Attr(&f, G("Init"), "Path"), 7, 0x7fffffff);
	return Sf_Return(&f, Sf_Cat(&f, "OsO", path, ".", name));
}

/* RestoreState(UsrName): the speech aborted: the door closed again */
static PyObject *GateKeeper_RestoreState(PyObject *self, PyObject *args) {
	PyObject *usr;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &usr)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	Sf_Ask(&f, "Speech", "RestoreState", "(O)", usr);
	PyObject *gc = Sf_GetItem(&f, G("GateKeepers"), usr);
	PyObject *door = Sf_Entity(&f, Sf_Attr(&f, gc, "Door"));
	Sf_Call(&f, Sf_Attr(&f, gc, "ClosedFunc"), "(OO)", door, usr);
	return Sf_None(&f);
}

/* EndOfSpeech(): the gatekeeper idles again; the door callback of the talk (LocalCallFunc) runs once */
static PyObject *GateKeeper_EndOfSpeech(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	PyObject *ch = Sf_Entity(&f, Sf_Mod(&f, "Speech", "NPCName"));
	if (Sf_True(&f, ch) > 0) {
		Sf_SetAttrI(&f, ch, "LLogicIdle", 0);
	}
	if (Sf_True(&f, G("LocalCallFunc")) > 0) {
		Sf_Call(&f, Sf_Mod(&f, "__builtin__", "apply"), "(OO)", G("LocalCallFunc"), G("LocalParams"));
		Sf_SetGlobal(&f, GK, "LocalCallFunc", Py_None);
		Sf_SetGlobal(&f, GK, "LocalParams", Py_None);
	}
	return Sf_None(&f);
}

/* the talk that opens the door: the answer, EndOfSpeech at its end with the door callback, the abort restores the
 * door */
static void Gk_Open(scriptFrame_t *f, PyObject *ch, PyObject *gc, PyObject *door, PyObject *chName, const char *ans) {
	Sf_Ask(f, "Speech", "OnSpch", "(sss)", "Generics_GateKeeper_InitMsg1", ans, "TalkNPC");
	Sf_SetMod(f, "Speech", "OnEndOfSpeech", Sf_Global(f, GK, "EndOfSpeech"));
	Sf_SetGlobal(f, GK, "LocalCallFunc", Sf_Attr(f, gc, "CanPassFunc"));
	Sf_SetGlobal(f, GK, "LocalParams", Sf_Build(f, "(OO)", door, chName));
	Sf_SetAttrS(f, ch, "AbortSelectFunc", "GateKeeper.RestoreState");
}

/* if AfterGateKeeperSpeech: AfterGateKeeperSpeech() */
static void Gk_After(scriptFrame_t *f) {
	if (Sf_True(f, Sf_Global(f, GK, "AfterGateKeeperSpeech")) > 0) {
		Sf_Call(f, Sf_Global(f, GK, "AfterGateKeeperSpeech"), "()");
	}
}

/* GateKeeperSpeech(UsrName, CharName, optId, FullOption): the speech routine: the gatekeeper opens for his foe type
 * (then AllreadyOpen), else names the group he waits for */
static PyObject *GateKeeper_GateKeeperSpeech(PyObject *self, PyObject *args) {
	PyObject *usrName, *chName, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usrName, &chName, &opt, &full)) {
		return NULL;
	}
	static const char *const mods[] = { "Speech", "Chars", "Challenge", "Menu", "string" };
	for (int i = 0; i < 5; i++) {
		Sf_Import(&f, mods[i]);
	}
	Sf_SetGlobal(&f, GK, "LocalCallFunc", Py_None);
	Sf_SetGlobal(&f, GK, "LocalParams", Py_None);
	PyObject *ch = Sf_Entity(&f, chName);
	PyObject *usr = Sf_Entity(&f, usrName);
	Sf_SetMod(&f, "Speech", "PlayerName", usrName);
	Sf_SetMod(&f, "Speech", "NPCName", chName);
	PyObject *gc = Sf_GetItem(&f, G("GateKeepers"), chName);
	PyObject *door = Sf_Entity(&f, Sf_Attr(&f, gc, "Door"));
	int talk = Sf_EqS(&f, opt, "Talk");
	if (talk > 0) {
		int open = Sf_Compare(&f, Sf_Attr(&f, gc, "FoeType"), "==", Sf_Build(&f, "()"));
		if (open == 0) {
			open = Sf_In(&f, Sf_Attr(&f, usr, "ActCtrl"), Sf_Attr(&f, gc, "FoeType"));
		}
		if (open > 0) {
			Sf_SetAttrI(&f, ch, "ArrnOpt", 0);
			Sf_SetAttr(&f, ch, "ArrOpt0", Sf_Cat(&f, "sO", "AllreadyOpen:", Sf_Lang(&f, Sf_Str(&f, "TalkOpt"))));
			Gk_Open(&f, ch, gc, door, chName, "Generics_GateKeeper_InitAns1");
			Gk_After(&f);
			return Sf_None(&f);
		}
		if (open < 0) {
			return Sf_None(&f);
		}
		PyObject *hash = G("HashGroup");
		int known = Sf_In(&f, Sf_Attr(&f, gc, "FoeType"), Sf_Method(&f, hash, "keys", "()"));
		if (known > 0) {
			PyObject *ans = Sf_Cat(&f, "sO", "Generics_GateKeeper_InitAns", Sf_GetItem(&f, G("HashGroup"),
				Sf_Attr(&f, gc, "FoeType")));
			Sf_Ask(&f, "Speech", "OnSpch", "(sOs)", "Generics_GateKeeper_InitMsg1", ans, "TalkNoNPC");
			Sf_SetMod(&f, "Speech", "OnEndOfSpeech", G("EndOfSpeech"));
			Gk_After(&f);
		} else if (known == 0) {
			Sf_Ask(&f, "Scrap", "ConsoleError", "(O)", Sf_Cat(&f, "sOs", "G A T E K E E P E R    E R R O R : ",
				Sf_Repr(&f, Sf_Attr(&f, gc, "FoeType")), " is an invalid foe type.\n"));
		}
	} else if (talk == 0 && Sf_EqS(&f, opt, "AllreadyOpen") > 0) {
		Gk_Open(&f, ch, gc, door, chName, "Generics_GateKeeper_InitAns2");
	}
	return Sf_None(&f);
}

PyMethodDef g_GateKeeperFuncs[] = {
	{ "UseDoor", GateKeeper_UseDoor, 1, NULL },
	{ "OnCreate", GateKeeper_OnCreate, 1, NULL },
	{ "DoNothingFunc", GateKeeper_DoNothingFunc, 1, NULL },
	{ "SetDoor", (PyCFunction)(void (*)(void))GateKeeper_SetDoor, 3, NULL },
	{ "DoorFoe", GateKeeper_DoorFoe, 1, NULL },
	{ "GetGateKeeperId", GateKeeper_GetGateKeeperId, 1, NULL },
	{ "RestoreState", GateKeeper_RestoreState, 1, NULL },
	{ "EndOfSpeech", GateKeeper_EndOfSpeech, 1, NULL },
	{ "GateKeeperSpeech", GateKeeper_GateKeeperSpeech, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
