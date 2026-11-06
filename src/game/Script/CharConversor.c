/*
 * The CharConversor module in C (scripts/missions/charconversor.py, issue #2): the character conversion machines of
 * the indoor maps and the possession of a character. Init places a conversor at every DM_Conversion_Point dummy;
 * using one (EnterCharConversor) plays its Action, whose events hide the player (InitOfScene), open the interface of
 * the characters (InitInterfaceEvent, AddInterfaceContent) and give the control back (EndOfScene). Picking a
 * character (Convert, ConvertExecute) replaces the player with a new one of that class (ConvertChar). Possession
 * puts the player into the character it flashes, NewPlayer finishes it when the camera arrived.
 *
 * The module variables stay in the module, because other scripts read and set them (ConversionChars,
 * AvailableChars, ConversionPoints, CheckConversor, afterExitConversor, OnPossession, PossessionTime, PrevConv,
 * prevChar, newChar, the _usrName, _charName, _optId and _fullOption of the last use, KONTROL, Cam, AuxInterp), with
 * the module code (the save variables it creates). The class of the machine is data/missions/conversor.def, the
 * layout of the interface data/menus/charconv.def.
 */
#include <limits.h>

#include "scrap/scrap.h"
#include "scrap/script.h"

#define CC "CharConversor"

static const char conversor_def[] = {
#embed "../../../data/missions/conversor.def"
	, 0 };

static const char charconv_def[] = {
#embed "../../../data/menus/charconv.def"
	, 0 };

/* a function of the module as its functions call it, with Py_BuildValue arguments */
#define Cc_Call(f, func, ...) Sf_Call(f, Sf_Global(f, CC, func), __VA_ARGS__)

/* GetNearestConversionPoint(Pos): the conversor nearest to the place (a floor between counts 20000); the second
 * nearest is PrevConv */
static PyObject *CharConversor_GetNearestConversionPoint(PyObject *self, PyObject *args) {
	PyObject *pos;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &pos)) {
		return NULL;
	}
	Sf_SetGlobal(&f, CC, "PrevConv", Py_None);
	PyObject *res = Sf_Ref(&f, Py_None);
	PyObject *dist = Sf_Int(&f, 3200000);
	PyObject *iarea = Sf_Ask(&f, "Scrap", "GetFloor", "(O)", Sf_Item(&f, pos, 1));
	PyObject *points = Sf_Global(&f, CC, "ConversionPoints");
	int mark = f.n;
	for (int i = 0;; i++) {
		PyObject *cp = Sf_Next(&f, points, i);
		if (!cp) {
			break;
		}
		PyObject *d = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", Sf_Attr(&f, cp, "Pos"), pos));
		PyObject *floor = Sf_Ask(&f, "Scrap", "GetFloor", "(O)", Sf_Item(&f, Sf_Attr(&f, cp, "Pos"), 1));
		PyObject *diff = Sf_Op(&f, iarea, '-', floor);
		PyObject *away = f.failed ? NULL : Sf_Keep(&f, PyNumber_Absolute(diff));
		d = Sf_Op(&f, d, '+', Sf_OpI(&f, away, '*', 20000));
		if (Sf_Compare(&f, d, "<", dist) > 0) {
			Sf_SetGlobal(&f, CC, "PrevConv", res);
			res = cp;
			dist = d;
		}
		PyObject *keep[2] = { res, dist };
		Sf_ReleaseKeep(&f, mark, keep, 2);
		res = keep[0];
		dist = keep[1];
	}
	return Sf_Return(&f, res);
}

/* CreateConversorClass(): the class of the machine and its sounds */
static PyObject *CharConversor_CreateConversorClass(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Script_RunFile(0, "missions/conversor.def", "Class", conversor_def, NULL, NULL);
	Sf_Import(&f, "DoorSound");
	Sf_Ask(&f, "DoorSound", "ConversorSetSounds", "(s)", "Conversor");
	return Sf_None(&f);
}

/* Init(): a conversor at every DM_Conversion_Point dummy, into ConversionPoints */
static PyObject *CharConversor_Init(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Cc_Call(&f, "CreateConversorClass", "()");
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_Conversion_Point*", 1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "!=", Py_None) <= 0) {
			break;
		}
		PyObject *pos = Sf_Item(&f, dummy, 1);
		PyObject *kb = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", Sf_Item(&f, dummy, 0), Sf_Item(&f, pos, 0),
			Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "WithLife");
		PyObject *y = Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), '+', 180);
		Sf_SetAttr(&f, kb, "Ang", Sf_Build(&f, "(iOi)", 0, y, 0));
		Sf_SetAttrS(&f, kb, "ActCtrl", "Conversor");
		Sf_SetAttrI(&f, kb, "VRadius", 60);
		Sf_SetAttrI(&f, kb, "ArrowCanUse", 1);
		Sf_SetAttrI(&f, kb, "Life", 0);
		Sf_SetAttrI(&f, kb, "ActiveCMask", 0);
		Sf_SetAttrI(&f, kb, "MapCollision", 1);
		Sf_SetAttrI(&f, kb, "AlwaysUse", 1);
		Sf_SetAttr(&f, kb, "ArrOpt0", Sf_Cat(&f, "sO", "Convert:", Sf_Lang(&f, Sf_Str(&f, "CharConversor"))));
		Sf_SetAttrS(&f, kb, "UseFunc", "CharConversor.EnterCharConversor");
		Sf_Method(&f, Sf_Global(&f, CC, "ConversionPoints"), "append", "(O)", kb);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* ConvertChar(UsrName, CharName, optId, FullOption): the player becomes a new character of the class of the save
 * variable Char at the conversor, with the energy of the old one */
static PyObject *CharConversor_ConvertChar(PyObject *self, PyObject *args) {
	PyObject *uname, *cname, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &uname, &cname, &opt, &full)) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "InitLoading", "()");
	Sf_Import(&f, "Chars");
	Sf_Import(&f, "string");
	Sf_SetGlobal(&f, CC, "prevChar", Sf_Attr(&f, Sf_Entity(&f, uname), "ActCtrl"));
	PyObject *usr = Sf_Entity(&f, uname);
	PyObject *act = Sf_Attr(&f, usr, "ActCtrl");
	PyObject *energy = Sf_Attr(&f, usr, "Energy");
	Sf_SetAttrI(&f, usr, "UsrControl", -1);
	Sf_SetAttrI(&f, usr, "Active", 0);
	Sf_SetAttrS(&f, usr, "ActCtrl", "");
	Sf_Method(&f, usr, "Del", "()");
	Sf_Ask(&f, "Chars", "DiscardRes", "(O)", act);
	Sf_Ask(&f, "Chars", "LoadRes", "(O)", Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Char"));
	PyObject *kb = Sf_Entity(&f, cname);
	int z = Sf_EqS(&f, uname, "PlayerZ");
	const char *pl = z > 0 ? "Player0" : "PlayerZ";
	PyObject *fn = Sf_Mod(&f, "Chars", "CreateMainChar");
	PyObject *ch = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Char");
	PyObject *p = Cc_Call(&f, "CPPos", "(O)", kb);
	PyObject *c = Sf_Call(&f, fn, "(sOOO)", pl, ch, p, Cc_Call(&f, "CPAng", "(O)", kb));
	Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, c, "Name"));
	Sf_SetAttr(&f, c, "MovePos", Sf_Attr(&f, c, "Pos"));
	Sf_SetAttr(&f, c, "MoveAng", Sf_Attr(&f, c, "Ang"));
	Sf_SetAttr(&f, c, "MoveTime", Sf_Float(&f, 0.001));
	Sf_SetAttrI(&f, c, "AutoSelect", 0);
	Sf_SetAttr(&f, c, "Energy", energy);
	Sf_SetGlobal(&f, CC, "newChar", Sf_Attr(&f, c, "ActCtrl"));
	Sf_SetGlobal(&f, CC, "_usrName", Sf_Attr(&f, c, "Name"));
	Sf_Ask(&f, "Scrap", "EndLoading", "()");
	return Sf_None(&f);
}

/* CPPos(kb): the place in front of a conversor */
static PyObject *CharConversor_CPPos(PyObject *self, PyObject *args) {
	PyObject *kb;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &kb)) {
		return NULL;
	}
	PyObject *pos = Sf_Attr(&f, kb, "Pos");
	PyObject *ang = Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, Sf_Attr(&f, kb, "Ang"), 1), 0);
	PyObject *off = Sf_Ask(&f, "SVec", "Rotate3D", "(OO)", Sf_Build(&f, "(iii)", 0, 0, -180), ang);
	return Sf_Return(&f, Sf_Ask(&f, "SVec", "Add", "(OO)", pos, off));
}

/* CPAng(kb): the angle that looks out of a conversor */
static PyObject *CharConversor_CPAng(PyObject *self, PyObject *args) {
	PyObject *kb;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &kb)) {
		return NULL;
	}
	PyObject *y = Sf_OpI(&f, Sf_Item(&f, Sf_Attr(&f, kb, "Ang"), 1), '+', 180);
	return Sf_Return(&f, Sf_Build(&f, "(iOi)", 0, y, 0));
}

/* DrawBackground(id, elem, posX=64, posY=64, sizeX=512, sizeY=352, alpha=92, red=0, green=0, blue=32): a coloured
 * rectangle (the interface no longer calls it) */
static PyObject *CharConversor_DrawBackground(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "elem", "posX", "posY", "sizeX", "sizeY", "alpha", "red", "green", "blue", NULL };
	static const char *const vars[] = { "X", "Y", "ScaleX", "ScaleY", "Unit", "Alpha", "Red", "Green", "Blue" };
	static const long defs[] = { 64, 64, 512, 352, 1, 92, 0, 0, 32 };
	PyObject *id, *elem, *v[9] = { NULL };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|OOOOOOOO", names, &id, &elem, &v[0], &v[1], &v[2], &v[3], &v[5],
		&v[6], &v[7], &v[8])) {
		return NULL;
	}
	PyObject *name = Sf_Cat(&f, "sO", "BackgroundElement", elem);
	Sf_Ask(&f, "SScorer", "Add", "(OOs)", id, name, "Sprite");
	for (int i = 0; i < 9; i++) {
		PyObject *val = v[i] && i != 4 ? v[i] : Sf_Int(&f, defs[i]);
		Sf_Ask(&f, "SScorer", "Set", "(OOsO)", id, name, vars[i], val);
	}
	return Sf_None(&f);
}

/* AddInterfaceContent(id): the interface of the characters (data/menus/charconv.def); the game time stops; the
 * description of the player's character, the keyboard links of the buttons, Escape leaves */
static PyObject *CharConversor_AddInterfaceContent(PyObject *self, PyObject *args) {
	PyObject *id;
	int pid = 0;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Ask(&f, "Menu", "InitMenuSys", "(i)", 0);
	Sf_Ask(&f, "Menu", "StartNewMenu", "(O)", id);
	Sf_Check(&f, f.failed || PyArg_Parse(id, "i", &pid));
	int total = Sf_Len(&f, Sf_Global(&f, CC, "ConversionChars"));
	Sf_Len(&f, Sf_Global(&f, CC, "AvailableChars"));
	int count = 0;
	for (int i = 0; i < 3 && !f.failed; i++) {
		for (int j = 0; j < 5 && !f.failed; j++) {
			count = i * 5 + j;
			if (count >= total) {
				break;
			}
			int mark = f.n;
			PyObject *ch = Sf_Item(&f, Sf_Global(&f, CC, "ConversionChars"), count);
			int avail = Sf_In(&f, ch, Sf_Global(&f, CC, "AvailableChars"));
			/* the names of the items, as the script builds them (a TypeError for a character that is no text) */
			Sf_Cat(&f, "sO", avail ? "CharHead_" : "CharNotAvailable_", ch);
			Sf_Cat(&f, "sO", "Char_", ch);
			int sel = Sf_Compare(&f, ch, "==", Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Char"));
			PyObject *vars = Sf_Build(&f, "{s:O,s:i,s:i,s:s}", "char", ch, "x", 88 + 95 * j, "y", 76 + 92 * i, "state",
				sel ? "sel" : avail ? "normal" : "dead");
			if (vars) {
				Script_RunFile(pid, "menus/charconv.def", avail ? "head" : "na", charconv_def, NULL, vars);
				Script_RunFile(pid, "menus/charconv.def", "button", charconv_def, NULL, vars);
			}
			Sf_Release(&f, mark);
		}
		if (count >= total) {
			break;
		}
	}
	if (!f.failed) {
		PyObject *vars = Sf_Build(&f, "{}");
		Script_RunFile(pid, "menus/charconv.def", "rest", charconv_def, NULL, vars);
	}
	PyObject *control = Sf_Cat(&f, "sO", "Char_", Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Char"));
	Cc_Call(&f, "ShowCharDesc", "(OOO)", id, control, Py_None);
	PyObject *lr = Sf_From(&f, "Menu", "LinkLR");
	PyObject *ud = Sf_From(&f, "Menu", "LinkUD");
	Sf_Len(&f, Sf_Global(&f, CC, "ConversionChars"));
	for (int i = 0; i < 3 && !f.failed; i++) {
		for (int j = 0; j < 4 && !f.failed; j++) {
			int mark = f.n;
			PyObject *chars = Sf_Global(&f, CC, "ConversionChars");
			PyObject *a = Sf_Cat(&f, "sO", "Char_", Sf_Item(&f, chars, i * 5 + j));
			PyObject *b = Sf_Cat(&f, "sO", "Char_", Sf_Item(&f, chars, i * 5 + j + 1));
			Sf_Call(&f, lr, "(OOO)", id, a, b);
			Sf_Release(&f, mark);
		}
	}
	for (int j = 0; j < 5 && !f.failed; j++) {
		for (int i = 0; i < 2 && !f.failed; i++) {
			int mark = f.n;
			PyObject *chars = Sf_Global(&f, CC, "ConversionChars");
			PyObject *a = Sf_Cat(&f, "sO", "Char_", Sf_Item(&f, chars, i * 5 + j));
			PyObject *b = Sf_Cat(&f, "sO", "Char_", Sf_Item(&f, chars, i * 5 + j + 5));
			if (i * 5 + j < 10) {
				Sf_Call(&f, ud, "(OOO)", id, a, b);
			}
			Sf_Release(&f, mark);
		}
	}
	PyObject *last = Sf_Cat(&f, "sO", "Char_", Sf_Item(&f, Sf_Global(&f, CC, "ConversionChars"), 14));
	Sf_Call(&f, lr, "(OOs)", id, last, "CharExit");
	for (int i = 10; i < 15 && !f.failed; i++) {
		int mark = f.n;
		PyObject *name = Sf_Cat(&f, "sO", "Char_", Sf_Item(&f, Sf_Global(&f, CC, "ConversionChars"), i));
		Sf_Ask(&f, "SScorer", "Set", "(OOss)", id, name, "Down", "CharExit");
		Sf_Ask(&f, "SScorer", "Set", "(OssO)", id, "CharExit", "Up", name);
		Sf_Release(&f, mark);
	}
	Sf_Ask(&f, "SScorer", "SetOnCancel", "(Os)", id, "CharConversor.ExitCharConversor");
	return Sf_None(&f);
}

/* SpawnChar(c, ChrConv): a character appears out of a conversor */
static PyObject *CharConversor_SpawnChar(PyObject *self, PyObject *args) {
	PyObject *c, *conv;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &c, &conv)) {
		return NULL;
	}
	Sf_Import(&f, "Chars");
	Sf_SetAttr(&f, c, "Pos", Cc_Call(&f, "CPPos", "(O)", conv));
	PyObject *y = Sf_OpI(&f, Sf_Item(&f, Cc_Call(&f, "CPAng", "(O)", conv), 1), '+', 180);
	Sf_SetAttr(&f, c, "Ang", Sf_Build(&f, "(iOi)", 0, y, 0));
	Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, c, "Name"));
	PyObject *name = Sf_Attr(&f, c, "Name");
	Sf_Ask(&f, "SFX", "CharacterConversor", "(OOi)", name, Sf_Attr(&f, conv, "Name"), 2);
	return Sf_None(&f);
}

/* MarkSpawnWaypoint(ChrConvName): a way point at the conversor */
static PyObject *CharConversor_MarkSpawnWaypoint(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	Sf_Import(&f, "SaveGame");
	Sf_Import(&f, "Init");
	PyObject *fn = Sf_Mod(&f, "SaveGame", "MarkWayPoint");
	PyObject *path = Sf_Mod(&f, "Init", "Path");
	Sf_Call(&f, fn, "(OOO)", path, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Spawn"), name);
	return Sf_None(&f);
}

/* EnterCharConversor(UsrName, CharName, optId, FullOption): the use of a conversor (the UseFunc): the machine plays,
 * the player stands in front of it with at least 2 batteries, a way point, the camera on the machine */
static PyObject *CharConversor_EnterCharConversor(PyObject *self, PyObject *args) {
	PyObject *uname, *cname, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &uname, &cname, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Chars");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Speech");
	Sf_Ask(&f, "Scrap", "Set", "(si)", "RemainTalkTime", 0);
	Sf_Ask(&f, "Scrap", "Set", "(sd)", "AbortSelectDistance", 0.0);
	if (Sf_EqS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Conversor.ActiveConversors"), "0") > 0) {
		PyObject *cad = Sf_Lang(&f, Sf_Str(&f, "NoActiveConversorsMessage"));
		PyObject *t = Sf_Time(&f);
		t = Sf_Op(&f, t, '+', Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)", cad));
		Sf_Ask(&f, "SScorer", "SetMsgText", "(OO)", cad, t);
		return Sf_None(&f);
	}
	PyObject *check = Sf_Global(&f, CC, "CheckConversor");
	if (Sf_True(&f, check) > 0) {
		if (Sf_True(&f, Sf_Call(&f, check, "(O)", cname)) == 0) {
			return Sf_None(&f);
		}
	}
	int other = Sf_Compare(&f, Sf_Attr(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "Name"), "!=", uname);
	if (other > 0) {
		Sf_Ask(&f, "Scrap", "ConsoleError", "(s)", "ERROR AT : EnterCharConversor() \n");
	} else if (other == 0) {
		PyObject *obj = Sf_Entity(&f, cname);
		Sf_SetAttrS(&f, obj, "Action", "Action");
		PyObject *usr = Sf_Entity(&f, uname);
		Sf_SetAttrS(&f, usr, "ForceAction", "Relax");
		Sf_SetAttrI(&f, usr, "AutoSelect", 0);
		Sf_SetAttrI(&f, usr, "UsrControl", -1);
		Sf_SetAttrI(&f, usr, "UsrControl", 0);
		Sf_SetAttr(&f, usr, "Pos", Cc_Call(&f, "CPPos", "(O)", obj));
		Sf_SetAttr(&f, usr, "Ang", Cc_Call(&f, "CPAng", "(O)", obj));
		Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, usr, "Name"));
		Sf_SetAttr(&f, usr, "MovePos", Sf_Attr(&f, usr, "Pos"));
		Sf_SetAttr(&f, usr, "MoveAng", Sf_Attr(&f, usr, "Ang"));
		Sf_SetAttr(&f, usr, "MoveTime", Sf_Float(&f, 0.001));
		Sf_SetAttr(&f, usr, "Life", Sf_Ask(&f, "Scrap", "Get", "(s)", "StdMaxLife"));
		PyObject *prev = Sf_ToInt(&f, Sf_Op(&f, Sf_Attr(&f, usr, "Energy"), '+', Sf_Float(&f, 0.999)));
		PyObject *full_e = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Batteries"));
		if (Sf_Compare(&f, prev, "<", Sf_Float(&f, 2.0)) > 0) {
			Sf_SetAttr(&f, usr, "Energy", Sf_Float(&f, 2.0));
		} else if (!f.failed && Sf_Compare(&f, prev, ">", full_e) > 0) {
			Sf_SetAttr(&f, usr, "Energy", full_e);
		} else {
			Sf_SetAttr(&f, usr, "Energy", prev);
		}
		Cc_Call(&f, "MarkSpawnWaypoint", "(O)", cname);
		Sf_SetAttr(&f, Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0)), "EntityLink", cname);
		Sf_SetGlobal(&f, CC, "_usrName", uname);
		Sf_SetGlobal(&f, CC, "_charName", cname);
		Sf_SetGlobal(&f, CC, "_optId", opt);
		Sf_SetGlobal(&f, CC, "_fullOption", full);
		Sf_Ask(&f, "Menu", "MovieScorer", "(i)", 0);
		Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	}
	return Sf_None(&f);
}

/* InitOfScene(EntityName, EventName, Time): the player vanishes into the machine */
static PyObject *CharConversor_InitOfScene(PyObject *self, PyObject *args) {
	PyObject *ename, *event, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ename, &event, &time)) {
		return NULL;
	}
	PyObject *usr = Sf_Global(&f, CC, "_usrName");
	Sf_Ask(&f, "SFX", "CharacterConversor", "(OOi)", usr, Sf_Global(&f, CC, "_charName"), 1);
	return Sf_None(&f);
}

/* EndOfScene(EntityName, EventName, Time): the end of the machine's action: the camera and the control back to the
 * player, afterExitConversor or the first conversion warning, the sentinels look */
static PyObject *CharConversor_EndOfScene(PyObject *self, PyObject *args) {
	PyObject *ename, *event, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ename, &event, &time)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "CharScorer");
	Sf_SetAttrS(&f, Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0)), "EntityLink", "");
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	Sf_SetAttrI(&f, usr, "AutoSelect", 1);
	Sf_SetAttrI(&f, usr, "MoveTime", 0);
	Sf_Entity(&f, ename);
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Walk");
	Sf_Ask(&f, "CharScorer", "Create", "(i)", 0);
	PyObject *after = Sf_Global(&f, CC, "afterExitConversor");
	int t = Sf_True(&f, after);
	if (t > 0) {
		Sf_Call(&f, after, "()");
	} else if (t == 0) {
		int first = Sf_EqS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Conversor.FirstConversion"), "1");
		if (first > 0) {
			first = Sf_Compare(&f, Sf_Attr(&f, usr, "ActCtrl"), "!=", Sf_Str(&f, "Dtritus"));
		}
		if (first > 0) {
			Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Conversor.FirstConversion", "0");
			Sf_Ask(&f, "Menu", "ShowWarning", "(iO)", 0, Sf_Lang(&f, Sf_Str(&f, "FirstConversionWarning")));
		}
	}
	Sf_Ask(&f, "SLogic", "SendSentinelToWatch", "(O)", Sf_Attr(&f, usr, "Pos"));
	return Sf_None(&f);
}

/* InitInterfaceEvent(EntityName, EventName, Time): the middle of the machine's action opens the interface */
static PyObject *CharConversor_InitInterfaceEvent(PyObject *self, PyObject *args) {
	PyObject *ename, *event, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ename, &event, &time)) {
		return NULL;
	}
	Cc_Call(&f, "AddInterfaceContent", "(i)", 0);
	return Sf_None(&f);
}

/* ExitCharConversor(id, control): the player appears again, the interface closes */
static PyObject *CharConversor_ExitCharConversor(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &control)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	PyObject *usr = Sf_Global(&f, CC, "_usrName");
	Sf_Ask(&f, "SFX", "CharacterConversor", "(OOi)", usr, Sf_Global(&f, CC, "_charName"), 2);
	Sf_Ask(&f, "Scrap", "Set", "(sO)", "AbortSelectDistance", Sf_Ask(&f, "Scrap", "Def", "(s)", "AbortSelectDistance"));
	Sf_Ask(&f, "Menu", "BackToGame", "(O)", id);
	return Sf_None(&f);
}

/* ShowCharDesc(id, control, lostfocus): the description of the character with the key of the special action; its
 * head talks, the one that lost the focus stops */
static PyObject *CharConversor_ShowCharDesc(PyObject *self, PyObject *args) {
	PyObject *id, *control, *lost;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &id, &control, &lost)) {
		return NULL;
	}
	PyObject *entry = Sf_Ask(&f, "SInput", "GetDefinedEntry", "(isss)", 0, "Walk", "Action", "Mouse");
	if (Sf_EqS(&f, control, "Char_Desktop") > 0) {
		entry = Sf_Build(&f, "(OO)", entry, entry);
	}
	PyObject *text = Sf_Op(&f, Sf_Lang(&f, control), '%', entry);
	Sf_Ask(&f, "SScorer", "Set", "(OssO)", id, "CharDesc", "Text", text);
	Sf_Ask(&f, "SScorer", "Set", "(Ossi)", id, "CharDesc", "XAlignCenter", 300);
	Sf_Ask(&f, "SScorer", "Set", "(Ossi)", id, "CharDesc", "Visible", 1);
	if (Sf_True(&f, lost) > 0) {
		PyObject *name = Sf_Cat(&f, "sO", "CharHead_", Sf_Slice(&f, lost, 5, INT_MAX));
		Sf_Ask(&f, "SScorer", "Set", "(OOsi)", id, name, "HeadAnimIndex", 0);
	}
	PyObject *name = Sf_Cat(&f, "sO", "CharHead_", Sf_Slice(&f, control, 5, INT_MAX));
	if (!f.failed) {
		PyObject *anim = Sf_Op(&f, Sf_Int(&f, 1), '+', Sf_ToInt(&f, Sf_Float(&f, Scrap_RandRange(0, 1.99))));
		Sf_Ask(&f, "SScorer", "Set", "(OOsO)", id, name, "HeadAnimIndex", anim);
	}
	return Sf_None(&f);
}

/* HideCharDesc(id, control, gainfocus): no description, the head stops */
static PyObject *CharConversor_HideCharDesc(PyObject *self, PyObject *args) {
	PyObject *id, *control, *gain;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &id, &control, &gain)) {
		return NULL;
	}
	Sf_Ask(&f, "SScorer", "Set", "(Osss)", id, "CharDesc", "Text", "");
	Sf_Ask(&f, "SScorer", "Set", "(Ossi)", id, "CharDesc", "Visible", 0);
	PyObject *name = Sf_Cat(&f, "sO", "CharHead_", Sf_Slice(&f, control, 5, INT_MAX));
	Sf_Ask(&f, "SScorer", "Set", "(OOsi)", id, name, "HeadAnimIndex", 0);
	return Sf_None(&f);
}

/* Convert(id, control): the pick of a character; the conversion runs after the frame is drawn (ConvertExecute) */
static PyObject *CharConversor_Convert(PyObject *self, PyObject *args) {
	PyObject *id, *control;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &control)) {
		return NULL;
	}
	Sf_SetGlobal(&f, CC, "KONTROL", control);
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "AfterDrawCallback", "CharConversor.ConvertExecute");
	return Sf_None(&f);
}

/* ConvertExecute(): an available character becomes the player's, the interface closes */
static PyObject *CharConversor_ConvertExecute(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "string");
	PyObject *ch = Sf_Ask(&f, "string", "replace", "(Oss)", Sf_Global(&f, CC, "KONTROL"), "Char_", "");
	if (Sf_In(&f, ch, Sf_Global(&f, CC, "AvailableChars")) == 0) {
		return Sf_None(&f);
	}
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Char", ch);
	PyObject *fn = Sf_Global(&f, CC, "ConvertChar");
	PyObject *u = Sf_Global(&f, CC, "_usrName");
	PyObject *c = Sf_Global(&f, CC, "_charName");
	PyObject *o = Sf_Global(&f, CC, "_optId");
	Sf_Call(&f, fn, "(OOOO)", u, c, o, Sf_Global(&f, CC, "_fullOption"));
	fn = Sf_Global(&f, CC, "ExitCharConversor");
	Sf_Call(&f, fn, "(iO)", 0, Sf_Global(&f, CC, "KONTROL"));
	return Sf_None(&f);
}

/* Possession(UserName, PossessedName): the player takes the character it flashed (one of ConversionChars, alive,
 * not invulnerable): the character dies without effects, a new player of its class stands there, the camera flies
 * over in slow motion and NewPlayer finishes */
static PyObject *CharConversor_Possession(PyObject *self, PyObject *args) {
	PyObject *uname, *pname;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &uname, &pname)) {
		return NULL;
	}
	Sf_Import(&f, "CharAct");
	Sf_Import(&f, "Chars");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Fx");
	Sf_Import(&f, "Speech");
	PyObject *ent = Sf_Entity(&f, pname);
	if (Sf_In(&f, Sf_Attr(&f, ent, "ActCtrl"), Sf_Global(&f, CC, "ConversionChars")) == 0) {
		int human = Sf_EqS(&f, Sf_Attr(&f, ent, "ActCtrl"), "Human");
		PyObject *cad = Sf_Lang(&f, Sf_Str(&f, human ? "Generics_ForbiddenPossession2_SystemMessage" :
			"Generics_ForbiddenPossession_SystemMessage"));
		PyObject *t = Sf_Time(&f);
		t = Sf_Op(&f, t, '+', Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)", cad));
		Sf_Ask(&f, "SScorer", "SetMsgText", "(OO)", cad, t);
		return Sf_None(&f);
	}
	int go = Sf_Compare(&f, Sf_Attr(&f, ent, "Life"), ">", Sf_Int(&f, 1));
	if (go > 0) {
		int inv = Sf_True(&f, Sf_Attr(&f, ent, "Invulnerable"));
		go = inv < 0 ? -1 : !inv;
	}
	if (go <= 0) {
		return Sf_None(&f);
	}
	PyObject *me = Sf_Entity(&f, uname);
	PyObject *onp = Sf_Global(&f, CC, "OnPossession");
	if (Sf_True(&f, onp) > 0) {
		PyObject *name = Sf_Attr(&f, ent, "Name");
		Sf_Call(&f, onp, "(OO)", name, Sf_Attr(&f, me, "Pos"));
	}
	Sf_Ask(&f, "Scrap", "IncSaveVar", "(s)", "Stats.Possession");
	Sf_Ask(&f, "SLogic", "Flash", "(Oi)", Sf_Attr(&f, me, "Pos"), 1500);
	Sf_Ask(&f, "SScorer", "Fade", "(i(iiii)(iiii)i)", 0, 255, 255, 255, 255, 255, 255, 255, 0, 2);
	Sf_SetGlobal(&f, CC, "Cam", Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0)));
	PyObject *x = Sf_Item(&f, Sf_Attr(&f, me, "Pos"), 0);
	PyObject *y = Sf_Item(&f, Sf_Attr(&f, me, "Pos"), 1);
	PyObject *z = Sf_Item(&f, Sf_Attr(&f, me, "Pos"), 2);
	Sf_SetGlobal(&f, CC, "AuxInterp", Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "AuxInterp", x, y, z, "Obj"));
	PyObject *va = Sf_Attr(&f, me, "ViewAng");
	Sf_SetAttr(&f, Sf_Global(&f, CC, "AuxInterp"), "Ang", va);
	Sf_SetAttrI(&f, ent, "Money", 0);
	Sf_Ask(&f, "CharAct", "NoFXDeath", "(OOi)", ent, uname, 0);
	Sf_Method(&f, ent, "Del", "()");
	Sf_SetAttrI(&f, me, "UsrControl", -1);
	int z1 = Sf_EqS(&f, Sf_Attr(&f, me, "Name"), "PlayerZ");
	const char *pl = z1 > 0 ? "Player0" : "PlayerZ";
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "Char", Sf_Attr(&f, ent, "ActCtrl"));
	PyObject *fn = Sf_Mod(&f, "Chars", "CreateMainChar");
	PyObject *ch = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Char");
	PyObject *epos = Sf_Attr(&f, ent, "Pos");
	PyObject *newme = Sf_Call(&f, fn, "(sOOO)", pl, ch, epos, Sf_Attr(&f, ent, "Ang"));
	Sf_SetAttrI(&f, newme, "UsrControl", -1);
	Sf_SetAttrS(&f, newme, "ForceAction", "Flashed");
	PyObject *mn = Sf_Attr(&f, me, "Name");
	PyObject *nn = Sf_Attr(&f, newme, "Name");
	Sf_Ask(&f, "SFX", "CharacterConversion", "(OOO)", mn, nn, Sf_Global(&f, CC, "PossessionTime"));
	Sf_SetAttr(&f, Sf_Global(&f, CC, "AuxInterp"), "CamTgt", Sf_Attr(&f, newme, "CamTgt"));
	Sf_SetAttr(&f, Sf_Global(&f, CC, "AuxInterp"), "MovePos", Sf_Attr(&f, newme, "Pos"));
	Sf_SetAttr(&f, Sf_Global(&f, CC, "AuxInterp"), "MoveAng", Sf_Attr(&f, newme, "ViewAng"));
	Sf_SetAttrI(&f, Sf_Global(&f, CC, "AuxInterp"), "MoveFade", 3);
	Sf_SetAttr(&f, Sf_Global(&f, CC, "AuxInterp"), "MoveTime", Sf_Global(&f, CC, "PossessionTime"));
	PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Global(&f, CC, "PossessionTime"));
	t = Sf_Op(&f, t, '+', Sf_Float(&f, 0.1));
	Sf_Schedule(&f, t, Sf_Global(&f, CC, "NewPlayer"), Sf_Build(&f, "(OO)", newme, me), NULL);
	Sf_Ask(&f, "SAct", "GetClass", "(O)", Sf_Attr(&f, ent, "ActCtrl"));
	static const char *const cams[][2] = { { "Fov", "FOV" }, { "CamDist", "CamDist" }, { "CamAng", "CamAng" },
		{ "CamRot", "CamRot" } };
	for (int i = 0; i < 4; i++) {
		PyObject *v = Sf_Ask(&f, "SAct", "GetCls", "(s)", cams[i][1]);
		Sf_SetAttr(&f, Sf_Global(&f, CC, "Cam"), cams[i][0], v);
	}
	PyObject *aux = Sf_Attr(&f, Sf_Global(&f, CC, "AuxInterp"), "Name");
	Sf_SetAttr(&f, Sf_Global(&f, CC, "Cam"), "MainTarget", aux);
	Sf_SetAttr(&f, Sf_Global(&f, CC, "Cam"), "Target", aux);
	Sf_Ask(&f, "Scrap", "Set", "(sd)", "S_PitchScale", 0.2);
	Sf_Ask(&f, "Scrap", "SetTimeSpeed", "(d)", 0.2);
	Sf_Ask(&f, "Menu", "MovieScorer", "(i)", 0);
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	return Sf_None(&f);
}

/* NewPlayer(newme, me): the end of a possession: the new character takes the camera, the energy (one battery less)
 * and the control, the old one goes, the time runs again and the alarm starts */
static PyObject *CharConversor_NewPlayer(PyObject *self, PyObject *args) {
	PyObject *newme, *me;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &newme, &me)) {
		return NULL;
	}
	Sf_Import(&f, "CharAct");
	Sf_Import(&f, "Chars");
	Sf_Import(&f, "CharScorer");
	Sf_Import(&f, "Menu");
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Walk");
	PyObject *name = Sf_Attr(&f, newme, "Name");
	Sf_SetAttr(&f, Sf_Global(&f, CC, "Cam"), "MainTarget", name);
	Sf_SetAttr(&f, Sf_Global(&f, CC, "Cam"), "Target", name);
	Sf_SetAttr(&f, newme, "Energy", Sf_Op(&f, Sf_Attr(&f, me, "Energy"), '-', Sf_Float(&f, 1.0)));
	Sf_SetAttrI(&f, newme, "Life", 100);
	Sf_SetAttrI(&f, newme, "UsrControl", 0);
	PyObject *act = Sf_Attr(&f, me, "ActCtrl");
	Sf_SetAttrI(&f, me, "Active", 0);
	Sf_SetAttrS(&f, me, "ActCtrl", "");
	Sf_Method(&f, me, "Del", "()");
	Sf_Ask(&f, "Chars", "DiscardRes", "(O)", act);
	Sf_Method(&f, Sf_Global(&f, CC, "AuxInterp"), "Del", "()");
	Sf_DelGlobal(&f, CC, "Cam");
	Sf_DelGlobal(&f, CC, "AuxInterp");
	Sf_Ask(&f, "Scrap", "Set", "(si)", "S_PitchScale", 1);
	Sf_Ask(&f, "Scrap", "SetTimeSpeed", "(d)", 1.0);
	Sf_Ask(&f, "CharScorer", "Create", "(i)", 0);
	PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Float(&f, 0.1));
	Sf_Schedule(&f, t, Sf_Mod(&f, "Scrap", "SetAlarm"), Sf_Build(&f, "(i)", 1), NULL);
	Sf_SetAttrS(&f, newme, "ForceAction", "FlashedEnd");
	if (Sf_EqS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Conversor.FirstPossession"), "1") > 0) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Conversor.FirstPossession", "0");
		Sf_Ask(&f, "Menu", "ShowWarning", "(iO)", 0, Sf_Lang(&f, Sf_Str(&f, "FirstPossessionWarning")));
	}
	return Sf_None(&f);
}

PyMethodDef g_CharConversorMethods[] = {
	{ "GetNearestConversionPoint", CharConversor_GetNearestConversionPoint, 1, NULL },
	{ "CreateConversorClass", CharConversor_CreateConversorClass, 1, NULL },
	{ "Init", CharConversor_Init, 1, NULL },
	{ "ConvertChar", CharConversor_ConvertChar, 1, NULL },
	{ "CPPos", CharConversor_CPPos, 1, NULL },
	{ "CPAng", CharConversor_CPAng, 1, NULL },
	{ "DrawBackground", (PyCFunction)(void (*)(void))CharConversor_DrawBackground, 3, NULL },
	{ "AddInterfaceContent", CharConversor_AddInterfaceContent, 1, NULL },
	{ "SpawnChar", CharConversor_SpawnChar, 1, NULL },
	{ "MarkSpawnWaypoint", CharConversor_MarkSpawnWaypoint, 1, NULL },
	{ "EnterCharConversor", CharConversor_EnterCharConversor, 1, NULL },
	{ "InitOfScene", CharConversor_InitOfScene, 1, NULL },
	{ "EndOfScene", CharConversor_EndOfScene, 1, NULL },
	{ "InitInterfaceEvent", CharConversor_InitInterfaceEvent, 1, NULL },
	{ "ExitCharConversor", CharConversor_ExitCharConversor, 1, NULL },
	{ "ShowCharDesc", CharConversor_ShowCharDesc, 1, NULL },
	{ "HideCharDesc", CharConversor_HideCharDesc, 1, NULL },
	{ "Convert", CharConversor_Convert, 1, NULL },
	{ "ConvertExecute", CharConversor_ConvertExecute, 1, NULL },
	{ "Possession", CharConversor_Possession, 1, NULL },
	{ "NewPlayer", CharConversor_NewPlayer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
