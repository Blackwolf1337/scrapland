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
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values, read the variables of the
 * module where the script reads them and write them back at once (OBJLIB 5). The save variables, the engine
 * variables, the camera name, the floor, the loading screen and the language texts are C; Scrap.Set, UsrEntity,
 * GetEntity, CreateEntity, IncSaveVar, ConsoleError, SetTimeSpeed, the dummy search, SLogic.SetOnFloor,
 * SScorer.Set, SetMsgText, Fade, SAct.GetClass, SInput.SetActionSet and the scheduler go through their seams, and the
 * characters and machines are handle values (OBJLIB 7). The other ports (Chars, CharAct, Menu, SaveGame, CharScorer,
 * DoorSound) are called through their refs (PORT_CALL: directly while the attribute holds the port's function, else
 * by name) and string.replace through its native twin; the module's own functions through its globals as the script
 * calls them (PORT_OWN, the same seam on the global). The SFX, SLogic.Flash, SendSentinelToWatch, SAct.GetCls,
 * SInput.GetDefinedEntry and SScorer.Add and SetOnCancel calls stay by name (OBJLIB 7); Menu.LinkLR and LinkUD come
 * from a from import (OBJLIB 6) and are called through their entries while they are Menu's own (PORT_VALUE); the .def
 * files take their $names as a dict (OBJLIB 6).
 */
#include <limits.h>

#include "scrap/app.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define CC "CharConversor"

static const char conversor_def[] = {
#embed "../../../data/missions/conversor.def"
	, 0 };

static const char charconv_def[] = {
#embed "../../../data/menus/charconv.def"
	, 0 };

/* a global of the module as its functions read it (out None before) */
static byte Cc_G(const char *name, nativeVal_t *out) {
	return Mode_G(CC, name, out);
}

/* GetNearestConversionPoint(Pos): the conversor nearest to the place (a floor between counts 20000); the second
 * nearest is PrevConv */
static byte Cc_GetNearestConversionPoint(const nativeVal_t *pos, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *res = Mode_V(&t), *dist = Mode_V(&t), *y = Mode_V(&t), *iarea = Mode_V(&t), *points = Mode_V(&t);
	if (!Mode_Put(CC, "PrevConv", NULL, res)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(dist, 3200000);
	int n = LvN_Item(pos, 1, y) && Mode_Floor(y, iarea) && Cc_G("ConversionPoints", points) ? Mode_Loop(points) : -1;
	for (int i = 0; i < n; i++) {
		int mark = t.n;
		nativeVal_t *cp = Mode_V(&t), *cpos = Mode_V(&t), *sub = Mode_V(&t), *d = Mode_V(&t), *cpos2 = Mode_V(&t);
		nativeVal_t *cy = Mode_V(&t), *floor = Mode_V(&t), *diff = Mode_V(&t), *away = Mode_V(&t), *k = Mode_V(&t);
		nativeVal_t *d2 = Mode_V(&t);
		int c = Native_SeqItem(points, i, cp) && Mode_VObj(cp, "Pos", cpos) && Mode_VecSub(cpos, pos, sub) &&
			Mode_VecMod(sub, d) && Mode_VObj(cp, "Pos", cpos2) && LvN_Item(cpos2, 1, cy) && Mode_Floor(cy, floor) &&
			Mode_Arith('-', iarea, floor, diff) && Mode_Abs(diff, away) &&
			Mode_Arith('*', away, Mode_I(&t, 20000), k) && Mode_Arith('+', d, k, d2) ? Mode_Cmp(d2, "<", dist) : -1;
		if (c < 0 || (c && !Mode_Put(CC, "PrevConv", NULL, res))) {
			return Mode_Done(&t, 0);
		}
		if (c) {
			Native_Free(res);
			Native_Free(dist);
			Native_Copy(res, cp);
			Native_Copy(dist, d2);
		}
		Mode_Release(&t, mark);
	}
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(out, res);
	return Mode_Done(&t, 1);
}

/* CreateConversorClass(): the class of the machine and its sounds */
static byte Cc_CreateConversorClass(void) {
	Script_RunFile(0, "missions/conversor.def", "Class", conversor_def, NULL, NULL);
	return Mode_Import("DoorSound") && PORT_CALL(DoorSound, ConversorSetSounds, NULL, "s", "Conversor");
}

/* Init(): a conversor at every DM_Conversion_Point dummy, into ConversionPoints */
static byte Cc_Init(void) {
	if (!PORT_OWN(CharConversor, CreateConversorClass, NULL, "") || !Mode_DummyStartSeam("DM_Conversion_Point*", 1)) {
		return 0;
	}
	for (;;) {
		modeVals_t t = { .n = 0 };
		nativeVal_t *dummy = Mode_V(&t), *pos = Mode_V(&t), *name = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t);
		nativeVal_t *z = Mode_V(&t), *kb = Mode_V(&t), *a = Mode_V(&t), *a1 = Mode_V(&t), *ang = Mode_V(&t);
		nativeVal_t *lang = Mode_V(&t), *opt = Mode_V(&t);
		if (!Mode_DummyNextSeam(dummy)) {
			return Mode_Done(&t, 0);
		}
		int none = Mode_Cmp(dummy, "==", Mode_V(&t));
		if (none) {
			return Mode_Done(&t, none > 0);
		}
		byte ok = LvN_Item(dummy, 1, pos) && LvN_Item(dummy, 0, name) && LvN_Item(pos, 0, x) && LvN_Item(pos, 1, y) &&
			LvN_Item(pos, 2, z) && LvN_Create(name, x, y, z, "WithLife", kb) && LvN_Item(dummy, 2, a) &&
			LvN_Item(a, 1, a1) && Mode_Arith('+', a1, Mode_I(&t, 180), ang);
		if (ok) {
			Native_Free(a);
			Mode_Ints3(a, 0, 0, 0);
			Native_Free(&a->items[1]);
			Native_Copy(&a->items[1], ang);
			Mode_Lang("CharConversor", lang);
			ok = Mode_VSet(kb, "Ang", a) && LvN_SetS(kb, "ActCtrl", "Conversor") && LvN_SetI(kb, "VRadius", 60) &&
				LvN_SetI(kb, "ArrowCanUse", 1) && LvN_SetI(kb, "Life", 0) && LvN_SetI(kb, "ActiveCMask", 0) &&
				LvN_SetI(kb, "MapCollision", 1) && LvN_SetI(kb, "AlwaysUse", 1) &&
				Mode_Cat(&t, opt, 2, Mode_S(&t, "Convert:"), lang) && Mode_VSet(kb, "ArrOpt0", opt) &&
				LvN_SetS(kb, "UseFunc", "CharConversor.EnterCharConversor") && Mode_Append(CC, "ConversionPoints", kb);
		}
		if (!Mode_Done(&t, ok)) {
			return 0;
		}
	}
}

/* ConvertChar(UsrName, CharName, optId, FullOption): the player becomes a new character of the class of the save
 * variable Char at the conversor, with the energy of the old one */
static byte Cc_ConvertChar(const nativeVal_t *uname, const nativeVal_t *cname) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *u0 = Mode_V(&t), *prev = Mode_V(&t), *usr = Mode_V(&t), *act = Mode_V(&t), *energy = Mode_V(&t);
	nativeVal_t *sv = Mode_V(&t), *kb = Mode_V(&t), *ch = Mode_V(&t), *p = Mode_V(&t), *a = Mode_V(&t), *c = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t), *v = Mode_V(&t), *t001 = Mode_V(&t);
	Loading_Init();
	if (!Mode_Import("Chars") || !Mode_Import("string") || !Mode_EntitySeam(uname, u0) || !Mode_VObj(u0, "ActCtrl", prev) ||
		!Mode_Put(CC, "prevChar", NULL, prev) || !Mode_EntitySeam(uname, usr) || !Mode_VObj(usr, "ActCtrl", act) ||
		!Mode_VObj(usr, "Energy", energy) || !LvN_SetI(usr, "UsrControl", -1) || !LvN_SetI(usr, "Active", 0) ||
		!LvN_SetS(usr, "ActCtrl", "") || !Mode_VDel(usr) || !PORT_CALL(Chars, DiscardRes, NULL, "v", act)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Char", sv);
	if (!PORT_CALL(Chars, LoadRes, NULL, "v", sv) || !Mode_EntitySeam(cname, kb)) {
		return Mode_Done(&t, 0);
	}
	const char *pl = LvN_EqS(uname, "PlayerZ") ? "Player0" : "PlayerZ";
	Mode_SaveVar("Char", ch);
	Native_SetFloat(t001, 0.001);
	byte ok = PORT_OWN(CharConversor, CPPos, p, "v", kb) && PORT_OWN(CharConversor, CPAng, a, "v", kb) &&
		PORT_CALL(Chars, CreateMainChar, c, "svvv", pl, ch, p, a) && Mode_VGet(c, "Name", name) &&
		LvN_SetOnFloor(name) && Mode_VObj(c, "Pos", v) && Mode_VSet(c, "MovePos", v) && (Native_Free(v), 1) &&
		Mode_VObj(c, "Ang", v) && Mode_VSet(c, "MoveAng", v) && Mode_VSet(c, "MoveTime", t001) &&
		LvN_SetI(c, "AutoSelect", 0) && Mode_VSet(c, "Energy", energy) && (Native_Free(v), 1) &&
		Mode_VObj(c, "ActCtrl", v) && Mode_Put(CC, "newChar", NULL, v) && (Native_Free(v), 1) &&
		Mode_VObj(c, "Name", v) && Mode_Put(CC, "_usrName", NULL, v);
	if (ok) {
		Loading_End();
	}
	return Mode_Done(&t, ok);
}

/* CPPos(kb): the place in front of a conversor */
static byte Cc_CPPos(const nativeVal_t *kb, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *pos = Mode_V(&t), *ka = Mode_V(&t), *y = Mode_V(&t), *ang = Mode_V(&t), *off = Mode_V(&t);
	nativeVal_t *r = Mode_V(&t);
	if (!Mode_VObj(kb, "Pos", pos) || !Mode_VObj(kb, "Ang", ka) || !LvN_Item(ka, 1, y)) {
		return Mode_Done(&t, 0);
	}
	Mode_Ints3(ang, 0, 0, 0);
	Native_Free(&ang->items[1]);
	Native_Copy(&ang->items[1], y);
	Mode_Ints3(off, 0, 0, -180);
	return Mode_Done(&t, Mode_Rotate(off, ang, r) && Mode_VecAdd(pos, r, out));
}

/* CPAng(kb): the angle that looks out of a conversor */
static byte Cc_CPAng(const nativeVal_t *kb, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ka = Mode_V(&t), *y = Mode_V(&t), *y2 = Mode_V(&t);
	if (!Mode_VObj(kb, "Ang", ka) || !LvN_Item(ka, 1, y) || !Mode_Arith('+', y, Mode_I(&t, 180), y2)) {
		return Mode_Done(&t, 0);
	}
	Mode_Ints3(out, 0, 0, 0);
	Native_Free(&out->items[1]);
	Native_Copy(&out->items[1], y2);
	return Mode_Done(&t, 1);
}

/* DrawBackground(id, elem, posX=64, posY=64, sizeX=512, sizeY=352, alpha=92, red=0, green=0, blue=32): a coloured
 * rectangle (the interface no longer calls it) */
static byte Cc_DrawBackground(const nativeVal_t *id, const nativeVal_t *elem, nativeVal_t *const *v) {
	static const char *const vars[] = { "X", "Y", "ScaleX", "ScaleY", "Unit", "Alpha", "Red", "Green", "Blue" };
	static const long defs[] = { 64, 64, 512, 352, 1, 92, 0, 0, 32 };
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t);
	if (!Mode_Cat(&t, name, 2, Mode_S(&t, "BackgroundElement"), elem) ||
		!Native_CallMod("SScorer", "Add", NULL, "vvs", id, name, "Sprite")) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 9; i++) {
		const nativeVal_t *val = v[i] && i != 4 ? v[i] : Mode_I(&t, defs[i]);
		if (!Mode_ScorerSet(id, name, vars[i], val)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* the $names of a charconv.def section as the dict Script_RunFile takes (OBJLIB 6): char, x, y, state */
static PyObject *Cc_Vars(const nativeVal_t *ch, long x, long y, const char *state) {
	PyObject *c = Native_ToPy(ch);
	PyObject *d = c ? Py_BuildValue("{s:O,s:i,s:i,s:s}", "char", c, "x", x, "y", y, "state", state) : NULL;
	Py_XDECREF(c);
	return d;
}

/* "Char_" + ConversionChars[i] */
static byte Cc_CharItem(long i, nativeVal_t *out) {
	nativeVal_t chars = { 0 }, ch = { 0 }, pre;
	Native_SetStr(&pre, "Char_");
	byte ok = Cc_G("ConversionChars", &chars) && LvN_Item(&chars, i, &ch) && Native_Arith('+', &pre, &ch, out);
	Native_Free(&chars);
	Native_Free(&ch);
	return ok;
}

/* f(id, "Char_" + ConversionChars[a], "Char_" + ConversionChars[b]) of a link function of Menu (b < 0: "CharExit") */
static byte Cc_Link(PyObject *f, const nativeVal_t *id, long a, long b) {
	nativeVal_t x = { 0 }, y = { 0 };
	byte ok = Cc_CharItem(a, &x) && (b < 0 ? (Native_SetStr(&y, "CharExit"), 1) : Cc_CharItem(b, &y)) &&
		PORT_VALUE(f, NULL, "vvv", id, &x, &y);
	Native_Free(&x);
	Native_Free(&y);
	return ok;
}

/* the heads and buttons of the characters: count the last index made */
static byte Cc_Heads(int pid, int total, int *count) {
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 5; j++) {
			*count = i * 5 + j;
			if (*count >= total) {
				return 1;
			}
			modeVals_t t = { .n = 0 };
			nativeVal_t *chars = Mode_V(&t), *ch = Mode_V(&t), *avl = Mode_V(&t), *n1 = Mode_V(&t), *n2 = Mode_V(&t);
			nativeVal_t *sv = Mode_V(&t);
			int avail = Cc_G("ConversionChars", chars) && LvN_Item(chars, *count, ch) && Cc_G("AvailableChars", avl) ?
				Native_Contains(avl, ch) : -1;
			/* the names of the items, as the script builds them (a TypeError for a character that is no text) */
			int sel = avail >= 0 && Mode_Cat(&t, n1, 2, Mode_S(&t, avail ? "CharHead_" : "CharNotAvailable_"), ch) &&
				Mode_Cat(&t, n2, 2, Mode_S(&t, "Char_"), ch) ? (Mode_SaveVar("Char", sv), Mode_Cmp(ch, "==", sv)) : -1;
			PyObject *vars = sel >= 0 ? Cc_Vars(ch, 88 + 95 * j, 76 + 92 * i, sel ? "sel" : avail ? "normal" : "dead") : NULL;
			if (vars) {
				Script_RunFile(pid, "menus/charconv.def", avail ? "head" : "na", charconv_def, NULL, vars);
				Script_RunFile(pid, "menus/charconv.def", "button", charconv_def, NULL, vars);
				Py_DECREF(vars);
			}
			if (!Mode_Done(&t, vars != NULL)) {
				return 0;
			}
		}
	}
	return 1;
}

/* AddInterfaceContent(id): the interface of the characters (data/menus/charconv.def); the game time stops; the
 * description of the player's character, the keyboard links of the buttons, Escape leaves (from Menu import
 * LinkLR, LinkUD: OBJLIB 6, the calls through PORT_VALUE) */
static byte Cc_AddInterfaceContent(const nativeVal_t *id) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *chars = Mode_V(&t), *avl = Mode_V(&t), *sv = Mode_V(&t), *control = Mode_V(&t), *none = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t);
	int pid = 0, total = -1, count = 0;
	if (!Mode_Import("Menu") || !PORT_CALL(Menu, InitMenuSys, NULL, "i", 0L) ||
		!PORT_CALL(Menu, StartNewMenu, NULL, "v", id) || !Native_ArgInt(id, &pid) ||
		!Cc_G("ConversionChars", chars) || (total = Mode_Len(chars)) < 0 || !Cc_G("AvailableChars", avl) ||
		Mode_Len(avl) < 0 || !Cc_Heads(pid, total, &count)) {
		return Mode_Done(&t, 0);
	}
	PyObject *rest = PyDict_New();
	if (!rest) {
		return Mode_Done(&t, 0);
	}
	Script_RunFile(pid, "menus/charconv.def", "rest", charconv_def, NULL, rest);
	Py_DECREF(rest);
	Mode_SaveVar("Char", sv);
	if (!Mode_Cat(&t, control, 2, Mode_S(&t, "Char_"), sv) ||
		!PORT_OWN(CharConversor, ShowCharDesc, NULL, "vvv", id, control, none)) {
		return Mode_Done(&t, 0);
	}
	PyObject *lr = Mode_From("Menu", "LinkLR"), *ud = lr ? Mode_From("Menu", "LinkUD") : NULL;
	Native_Free(chars);
	byte ok = ud && Cc_G("ConversionChars", chars) && Mode_Len(chars) >= 0;
	for (int i = 0; ok && i < 3; i++) {
		for (int j = 0; ok && j < 4; j++) {
			ok = Cc_Link(lr, id, i * 5 + j, i * 5 + j + 1);
		}
	}
	for (int j = 0; ok && j < 5; j++) {
		for (int i = 0; ok && i < 2; i++) {
			nativeVal_t a = { 0 }, b = { 0 };
			ok = Cc_CharItem(i * 5 + j, &a) && Cc_CharItem(i * 5 + j + 5, &b) &&
				(i * 5 + j >= 10 || PORT_VALUE(ud, NULL, "vvv", id, &a, &b));
			Native_Free(&a);
			Native_Free(&b);
		}
	}
	ok = ok && Cc_Link(lr, id, 14, -1);
	for (int i = 10; ok && i < 15; i++) {
		Native_Free(name);
		ok = Cc_CharItem(i, name) && Mode_ScorerSet(id, name, "Down", Mode_S(&t, "CharExit")) &&
			Mode_ScorerSet(id, Mode_S(&t, "CharExit"), "Up", name);
	}
	Py_XDECREF(lr);
	Py_XDECREF(ud);
	return Mode_Done(&t, ok && Native_CallMod("SScorer", "SetOnCancel", NULL, "vs", id, "CharConversor.ExitCharConversor"));
}

/* SpawnChar(c, ChrConv): a character appears out of a conversor (OBJLIB 7: SFX by name) */
static byte Cc_SpawnChar(const nativeVal_t *c, const nativeVal_t *conv) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *p = Mode_V(&t), *a = Mode_V(&t), *y = Mode_V(&t), *y2 = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t), *name2 = Mode_V(&t), *cn = Mode_V(&t);
	if (!Mode_Import("Chars") || !PORT_OWN(CharConversor, CPPos, p, "v", conv) || !Mode_VSet(c, "Pos", p) ||
		!PORT_OWN(CharConversor, CPAng, a, "v", conv) || !LvN_Item(a, 1, y) ||
		!Mode_Arith('+', y, Mode_I(&t, 180), y2)) {
		return Mode_Done(&t, 0);
	}
	Mode_Ints3(ang, 0, 0, 0);
	Native_Free(&ang->items[1]);
	Native_Copy(&ang->items[1], y2);
	return Mode_Done(&t, Mode_VSet(c, "Ang", ang) && Mode_VObj(c, "Name", name) && LvN_SetOnFloor(name) &&
		Mode_VObj(c, "Name", name2) && Mode_VObj(conv, "Name", cn) &&
		Native_CallMod("SFX", "CharacterConversor", NULL, "vvi", name2, cn, 2L));
}

/* MarkSpawnWaypoint(ChrConvName): a way point at the conversor */
static byte Cc_MarkSpawnWaypoint(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *path = Mode_V(&t), *spawn = Mode_V(&t);
	if (!Mode_Import("SaveGame") || !Mode_Import("Init") || !LvN_ModGet("Init", "Path", path)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Spawn", spawn);
	return Mode_Done(&t, PORT_CALL(SaveGame, MarkWayPoint, NULL, "vvv", path, spawn, name));
}

/* the energy of the player at a conversor: at least 2 batteries, at most the save variable Batteries */
static byte Cc_Energy(const nativeVal_t *usr) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *f = Mode_V(&t), *s = Mode_V(&t), *prev = Mode_V(&t), *sv = Mode_V(&t);
	nativeVal_t *full = Mode_V(&t), *two = Mode_V(&t);
	Native_SetFloat(f, 0.999);
	Native_SetFloat(two, 2.0);
	if (!Mode_VObj(usr, "Energy", e) || !Mode_Arith('+', e, f, s) || !Native_ToInt(s, prev)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Batteries", sv);
	int c = Native_ToInt(sv, full) ? Mode_Cmp(prev, "<", two) : -1;
	if (c > 0) {
		return Mode_Done(&t, Mode_VSet(usr, "Energy", two));
	}
	c = c < 0 ? -1 : Mode_Cmp(prev, ">", full);
	return Mode_Done(&t, c >= 0 && Mode_VSet(usr, "Energy", c ? full : prev));
}

/* EnterCharConversor(UsrName, CharName, optId, FullOption): the use of a conversor (the UseFunc): the machine plays,
 * the player stands in front of it with at least 2 batteries, a way point, the camera on the machine */
static byte Cc_EnterCharConversor(const nativeVal_t *uname, const nativeVal_t *cname, const nativeVal_t *opt,
	const nativeVal_t *full) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *sv = Mode_V(&t), *cad = Mode_V(&t), *check = Mode_V(&t), *r = Mode_V(&t), *u = Mode_V(&t);
	nativeVal_t *un = Mode_V(&t), *obj = Mode_V(&t), *usr = Mode_V(&t), *p = Mode_V(&t), *a = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t), *v = Mode_V(&t), *life = Mode_V(&t), *t001 = Mode_V(&t), *cam = Mode_V(&t);
	nativeVal_t *ce = Mode_V(&t);
	if (!Mode_Import("Chars") || !Mode_Import("Menu") || !Mode_Import("Speech") || !Mode_GVarSetI("RemainTalkTime", 0) ||
		!Mode_GVarSetN("AbortSelectDistance", 0.0)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Conversor.ActiveConversors", sv);
	if (LvN_EqS(sv, "0")) {
		Mode_Lang("NoActiveConversorsMessage", cad);
		return Mode_Done(&t, Mode_SpeechMsg(cad));
	}
	if (!Cc_G("CheckConversor", check)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(check)) {
		if (!Mode_CallArgs(check, r, cname, 1)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(r)) {
			return Mode_Done(&t, 1);
		}
	}
	int other = Mode_UsrHandle(u) && Mode_VObj(u, "Name", un) ? Mode_Cmp(un, "!=", uname) : -1;
	if (other) {
		return Mode_Done(&t, other > 0 && Mode_ConsoleError(Mode_S(&t, "ERROR AT : EnterCharConversor() \n")));
	}
	Native_SetFloat(t001, 0.001);
	byte ok = Mode_EntitySeam(cname, obj) && LvN_SetS(obj, "Action", "Action") && Mode_EntitySeam(uname, usr) &&
		LvN_SetS(usr, "ForceAction", "Relax") && LvN_SetI(usr, "AutoSelect", 0) && LvN_SetI(usr, "UsrControl", -1) &&
		LvN_SetI(usr, "UsrControl", 0) && PORT_OWN(CharConversor, CPPos, p, "v", obj) && Mode_VSet(usr, "Pos", p) &&
		PORT_OWN(CharConversor, CPAng, a, "v", obj) && Mode_VSet(usr, "Ang", a) && Mode_VObj(usr, "Name", name) &&
		LvN_SetOnFloor(name) && Mode_VObj(usr, "Pos", v) && Mode_VSet(usr, "MovePos", v) && (Native_Free(v), 1) &&
		Mode_VObj(usr, "Ang", v) && Mode_VSet(usr, "MoveAng", v) && Mode_VSet(usr, "MoveTime", t001) &&
		Native_GVarGet("StdMaxLife", life) && Mode_VSet(usr, "Life", life) && Cc_Energy(usr) &&
		PORT_OWN(CharConversor, MarkSpawnWaypoint, NULL, "v", cname) &&
		(Mode_CamName(0, cam), Mode_EntitySeam(cam, ce)) && Mode_VSet(ce, "EntityLink", cname) &&
		Mode_Put(CC, "_usrName", NULL, uname) && Mode_Put(CC, "_charName", NULL, cname) && Mode_Put(CC, "_optId", NULL, opt) &&
		Mode_Put(CC, "_fullOption", NULL, full) && PORT_CALL(Menu, MovieScorer, NULL, "i", 0L) &&
		Mode_ActionSet("Inactive");
	return Mode_Done(&t, ok);
}

/* SFX.CharacterConversor(_usrName, _charName, step) of the module's globals (OBJLIB 7: SFX by name) */
static byte Cc_Fx(long step) {
	nativeVal_t u = { 0 }, c = { 0 };
	byte ok = Cc_G("_usrName", &u) && Cc_G("_charName", &c) && Native_CallMod("SFX", "CharacterConversor", NULL, "vvi", &u, &c, step);
	Native_Free(&u);
	Native_Free(&c);
	return ok;
}

/* EndOfScene(EntityName, EventName, Time): the end of the machine's action: the camera and the control back to the
 * player, afterExitConversor or the first conversion warning, the sentinels look (OBJLIB 7:
 * SLogic.SendSentinelToWatch by name) */
static byte Cc_EndOfScene(const nativeVal_t *ename) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *e = Mode_V(&t), *after = Mode_V(&t), *sv = Mode_V(&t), *act = Mode_V(&t);
	nativeVal_t *msg = Mode_V(&t), *pos = Mode_V(&t);
	if (!Mode_Import("Menu") || !Mode_Import("CharScorer") || !Mode_CamLink(Mode_S(&t, "")) || !Mode_UsrHandle(usr) ||
		!LvN_SetI(usr, "AutoSelect", 1) || !LvN_SetI(usr, "MoveTime", 0) || !Mode_EntitySeam(ename, e) ||
		!Mode_ActionSet("Walk") || !PORT_CALL(CharScorer, Create, NULL, "i", 0L) ||
		!LvN_GObj(CC, "afterExitConversor", after)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(after)) {
		if (!Mode_CallArgs(after, NULL, NULL, 0)) {
			return Mode_Done(&t, 0);
		}
	} else {
		Mode_SaveVar("Conversor.FirstConversion", sv);
		int first = LvN_EqS(sv, "1");
		if (first) {
			first = Mode_VObj(usr, "ActCtrl", act) ? Mode_Cmp(act, "!=", Mode_S(&t, "Dtritus")) : -1;
		}
		if (first < 0) {
			return Mode_Done(&t, 0);
		}
		if (first) {
			Mode_Lang("FirstConversionWarning", msg);
			if (!Mode_SetSaveVar("Conversor.FirstConversion", Mode_S(&t, "0")) ||
				!PORT_CALL(Menu, ShowWarning, NULL, "iv", 0L, msg)) {
				return Mode_Done(&t, 0);
			}
		}
	}
	return Mode_Done(&t, Mode_VObj(usr, "Pos", pos) && Native_CallMod("SLogic", "SendSentinelToWatch", NULL, "v", pos));
}

/* ExitCharConversor(id, control): the player appears again, the interface closes */
static byte Cc_ExitCharConversor(const nativeVal_t *id) {
	return Mode_Import("Menu") && Cc_Fx(2) && Mode_GVarSetDef("AbortSelectDistance") &&
		PORT_CALL(Menu, BackToGame, NULL, "v", id);
}

/* SScorer.Set(id, "CharHead_" + control[5:], "HeadAnimIndex", anim) */
static byte Cc_HeadAnim(const nativeVal_t *id, const nativeVal_t *control, const nativeVal_t *anim) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *tail = Mode_V(&t), *name = Mode_V(&t);
	return Mode_Done(&t, Native_Slice(control, 5, INT_MAX, tail) && Mode_Cat(&t, name, 2, Mode_S(&t, "CharHead_"), tail) &&
		Mode_ScorerSet(id, name, "HeadAnimIndex", anim));
}

/* ShowCharDesc(id, control, lostfocus): the description of the character with the key of the special action; its
 * head talks, the one that lost the focus stops (OBJLIB 7: SInput.GetDefinedEntry by name) */
static byte Cc_ShowCharDesc(const nativeVal_t *id, const nativeVal_t *control, const nativeVal_t *lost) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *entry = Mode_V(&t), *pair = Mode_V(&t), *lang = Mode_V(&t), *text = Mode_V(&t), *item = Mode_V(&t);
	nativeVal_t *f = Mode_V(&t), *k = Mode_V(&t), *anim = Mode_V(&t);
	if (!Native_CallMod("SInput", "GetDefinedEntry", entry, "isss", 0L, "Walk", "Action", "Mouse")) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqS(control, "Char_Desktop")) {
		Native_SetEmpty(pair, NV_TUPLE);
		Native_Copy(Native_Append(pair), entry);
		Native_Copy(Native_Append(pair), entry);
		Native_Free(entry);
		Native_Copy(entry, pair);
	}
	Native_SetStr(item, "CharDesc");
	if (!Mode_LangV(control, lang) || !Mode_Arith('%', lang, entry, text) || !Mode_ScorerSet(id, item, "Text", text) ||
		!Mode_ScorerSet(id, item, "XAlignCenter", Mode_I(&t, 300)) || !Mode_ScorerSet(id, item, "Visible", Mode_I(&t, 1)) ||
		(Native_True(lost) && !Cc_HeadAnim(id, lost, Mode_I(&t, 0)))) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(f, Scrap_RandRange(0, 1.99));
	return Mode_Done(&t, Native_ToInt(f, k) && Mode_Arith('+', Mode_I(&t, 1), k, anim) && Cc_HeadAnim(id, control, anim));
}

/* HideCharDesc(id, control, gainfocus): no description, the head stops */
static byte Cc_HideCharDesc(const nativeVal_t *id, const nativeVal_t *control) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *item = Mode_S(&t, "CharDesc");
	return Mode_Done(&t, Mode_ScorerSet(id, item, "Text", Mode_S(&t, "")) && Mode_ScorerSet(id, item, "Visible", Mode_I(&t, 0)) &&
		Cc_HeadAnim(id, control, Mode_I(&t, 0)));
}

/* Convert(id, control): the pick of a character; the conversion runs after the frame is drawn (ConvertExecute) */
static byte Cc_Convert(const nativeVal_t *control) {
	return Mode_Put(CC, "KONTROL", NULL, control) && Mode_GVarSetS("AfterDrawCallback", "CharConversor.ConvertExecute");
}

/* ConvertExecute(): an available character becomes the player's, the interface closes (string.replace through its
 * native twin) */
static byte Cc_ConvertExecute(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *k = Mode_V(&t), *ch = Mode_V(&t), *avl = Mode_V(&t), *u = Mode_V(&t), *c = Mode_V(&t), *o = Mode_V(&t);
	nativeVal_t *full = Mode_V(&t), *k2 = Mode_V(&t);
	int in = Mode_Import("string") && Cc_G("KONTROL", k) && PORT_CALL(string, replace, ch, "vss", k, "Char_", "") &&
		Cc_G("AvailableChars", avl) ? Native_Contains(avl, ch) : -1;
	if (in <= 0) {
		return Mode_Done(&t, in == 0);
	}
	if (!Mode_SetSaveVar("Char", ch)) {
		return Mode_Done(&t, 0);
	}
	/* ConvertChar(_usrName, _charName, _optId, _fullOption); ExitCharConversor(0, KONTROL): the globals are read before
	 * the function now (the script looked the function up first; another error only when both are missing) */
	return Mode_Done(&t, Cc_G("_usrName", u) && Cc_G("_charName", c) && Cc_G("_optId", o) && Cc_G("_fullOption", full) &&
		PORT_OWN(CharConversor, ConvertChar, NULL, "vvvv", u, c, o, full) && Cc_G("KONTROL", k2) &&
		PORT_OWN(CharConversor, ExitCharConversor, NULL, "iv", 0L, k2));
}

/* the message of a possession that is not allowed */
static byte Cc_Forbidden(const nativeVal_t *act) {
	return Mode_LangMsg(LvN_EqS(act, "Human") ? "Generics_ForbiddenPossession2_SystemMessage" :
		"Generics_ForbiddenPossession_SystemMessage");
}

/* Cam.<var> = v and AuxInterp.<var> = v of the module's globals (read at each set) */
static byte Cc_GSet(const char *g, const char *var, const nativeVal_t *v) {
	nativeVal_t o = { 0 };
	byte ok = Cc_G(g, &o) && Mode_VSet(&o, var, v);
	Native_Free(&o);
	return ok;
}

/* the end of Possession: the camera of the possessed class, the slow motion (OBJLIB 7: SAct.GetCls by name) */
static byte Cc_PossessionCam(const nativeVal_t *ent) {
	static const char *const cams[][2] = { { "Fov", "FOV" }, { "CamDist", "CamDist" }, { "CamAng", "CamAng" },
		{ "CamRot", "CamRot" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *act = Mode_V(&t), *aux = Mode_V(&t), *name = Mode_V(&t), *cls = Mode_V(&t);
	const char *a;
	if (!Mode_VObj(ent, "ActCtrl", act) || !Native_ArgStr(act, 1, &a) || !LvN_GetClass(a, cls)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 4; i++) {
		nativeVal_t v = { 0 };
		byte ok = Native_CallMod("SAct", "GetCls", &v, "s", cams[i][1]) && Cc_GSet("Cam", cams[i][0], &v);
		Native_Free(&v);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Cc_G("AuxInterp", aux) && Mode_VObj(aux, "Name", name) && Cc_GSet("Cam", "MainTarget", name) &&
		Cc_GSet("Cam", "Target", name) && Mode_GVarSetN("S_PitchScale", 0.2) && Mode_TimeSpeed(0.2) &&
		PORT_CALL(Menu, MovieScorer, NULL, "i", 0L) && Mode_ActionSet("Inactive"));
}

/* Possession(UserName, PossessedName): the player takes the character it flashed (one of ConversionChars, alive,
 * not invulnerable): the character dies without effects, a new player of its class stands there, the camera flies
 * over in slow motion and NewPlayer finishes (OBJLIB 7: SLogic.Flash and SFX by name) */
static byte Cc_Possession(const nativeVal_t *uname, const nativeVal_t *pname) {
	static const int white[4] = { 255, 255, 255, 255 }, clear[4] = { 255, 255, 255, 0 };
	modeVals_t t = { .n = 0 };
	nativeVal_t *ent = Mode_V(&t), *act = Mode_V(&t), *chars = Mode_V(&t), *life = Mode_V(&t), *inv = Mode_V(&t);
	nativeVal_t *me = Mode_V(&t), *onp = Mode_V(&t), *name = Mode_V(&t), *pos = Mode_V(&t), *cam = Mode_V(&t);
	nativeVal_t *ce = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *aux = Mode_V(&t), *v = Mode_V(&t);
	nativeVal_t *ch = Mode_V(&t), *epos = Mode_V(&t), *eang = Mode_V(&t), *newme = Mode_V(&t), *mn = Mode_V(&t);
	nativeVal_t *nn = Mode_V(&t), *pt = Mode_V(&t), *now = Mode_V(&t), *t1 = Mode_V(&t), *t2 = Mode_V(&t);
	static const char *const mods[] = { "CharAct", "Chars", "Menu", "Fx", "Speech" };
	for (int i = 0; i < 5; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	int in = Mode_EntitySeam(pname, ent) && Mode_VObj(ent, "ActCtrl", act) && Cc_G("ConversionChars", chars) ?
		Native_Contains(chars, act) : -1;
	if (in <= 0) {
		Native_Free(act);
		return Mode_Done(&t, in == 0 && Mode_VObj(ent, "ActCtrl", act) && Cc_Forbidden(act));
	}
	int go = Mode_VObj(ent, "Life", life) ? Mode_CmpI(life, ">", 1) : -1;
	if (go > 0) {
		go = Mode_VObj(ent, "Invulnerable", inv) ? !Native_True(inv) : -1;
	}
	if (go <= 0) {
		return Mode_Done(&t, go == 0);
	}
	if (!Mode_EntitySeam(uname, me) || !LvN_GObj(CC, "OnPossession", onp)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(onp)) {
		nativeVal_t a[2] = { { 0 }, { 0 } };
		byte ok = Mode_VObj(ent, "Name", &a[0]) && Mode_VObj(me, "Pos", &a[1]) && Mode_CallArgs(onp, NULL, a, 2);
		Native_Free(&a[0]);
		Native_Free(&a[1]);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	Native_Free(pos);
	Mode_CamName(0, cam);
	if (!Mode_IncSaveVar("Stats.Possession") || !Mode_VObj(me, "Pos", pos) ||
		!Native_CallMod("SLogic", "Flash", NULL, "vi", pos, 1500L) || !Mode_FadeV(0, white, clear, Mode_I(&t, 2)) ||
		!Mode_EntitySeam(cam, ce) || !Mode_Put(CC, "Cam", NULL, ce)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(pos);
	if (!Mode_VObj(me, "Pos", pos) || !LvN_Item(pos, 0, x) || (Native_Free(pos), !Mode_VObj(me, "Pos", pos)) ||
		!LvN_Item(pos, 1, y) || (Native_Free(pos), !Mode_VObj(me, "Pos", pos)) || !LvN_Item(pos, 2, z) ||
		!LvN_Create(Mode_S(&t, "AuxInterp"), x, y, z, "Obj", aux) || !Mode_Put(CC, "AuxInterp", NULL, aux) ||
		!Mode_VObj(me, "ViewAng", v) || !Cc_GSet("AuxInterp", "Ang", v) || !LvN_SetI(ent, "Money", 0) ||
		!PORT_CALL(CharAct, NoFXDeath, NULL, "vvi", ent, uname, 0L) || !Mode_VDel(ent) ||
		!LvN_SetI(me, "UsrControl", -1) || (Native_Free(name), !Mode_VObj(me, "Name", name))) {
		return Mode_Done(&t, 0);
	}
	const char *pl = LvN_EqS(name, "PlayerZ") ? "Player0" : "PlayerZ";
	Native_Free(act);
	if (!Mode_VObj(ent, "ActCtrl", act) || !Mode_SetSaveVar("Char", act)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Char", ch);
	Native_Free(v);
	if (!Mode_VObj(ent, "Pos", epos) || !Mode_VObj(ent, "Ang", eang) ||
		!PORT_CALL(Chars, CreateMainChar, newme, "svvv", pl, ch, epos, eang) || !LvN_SetI(newme, "UsrControl", -1) ||
		!LvN_SetS(newme, "ForceAction", "Flashed") || !Mode_VObj(me, "Name", mn) || !Mode_VObj(newme, "Name", nn) ||
		!Cc_G("PossessionTime", pt) || !Native_CallMod("SFX", "CharacterConversion", NULL, "vvv", mn, nn, pt) ||
		!Mode_VObj(newme, "CamTgt", v) || !Cc_GSet("AuxInterp", "CamTgt", v) || (Native_Free(v), !Mode_VObj(newme, "Pos", v)) ||
		!Cc_GSet("AuxInterp", "MovePos", v) || (Native_Free(v), !Mode_VObj(newme, "ViewAng", v)) ||
		!Cc_GSet("AuxInterp", "MoveAng", v) || !Cc_GSet("AuxInterp", "MoveFade", Mode_I(&t, 3)) ||
		(Native_Free(pt), !Cc_G("PossessionTime", pt)) || !Cc_GSet("AuxInterp", "MoveTime", pt)) {
		return Mode_Done(&t, 0);
	}
	nativeVal_t pa[2], tenth;
	Native_SetFloat(now, Mode_Time());
	Native_SetFloat(&tenth, 0.1);
	Native_Free(pt);
	if (!Cc_G("PossessionTime", pt) || !Mode_Arith('+', now, pt, t1) || !Mode_Arith('+', t1, &tenth, t2)) {
		return Mode_Done(&t, 0);
	}
	/* OBJLIB 4: the scheduler keeps the global NewPlayer */
	PyObject *fn = Mode_GlobalPy(CC, "NewPlayer");
	Native_Copy(&pa[0], newme);
	Native_Copy(&pa[1], me);
	byte ok = fn && Mode_Schedule(t2, fn, pa, 2, NULL);
	Native_Free(&pa[0]);
	Native_Free(&pa[1]);
	Py_XDECREF(fn);
	return Mode_Done(&t, ok && Cc_PossessionCam(ent));
}

/* NewPlayer(newme, me): the end of a possession: the new character takes the camera, the energy (one battery less)
 * and the control, the old one goes, the time runs again and the alarm starts (OBJLIB 4: the scheduler keeps
 * Scrap.SetAlarm) */
static byte Cc_NewPlayer(const nativeVal_t *newme, const nativeVal_t *me) {
	static const char *const mods[] = { "CharAct", "Chars", "CharScorer", "Menu" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *e = Mode_V(&t), *one = Mode_V(&t), *e1 = Mode_V(&t), *act = Mode_V(&t);
	nativeVal_t *aux = Mode_V(&t), *when = Mode_V(&t), *sv = Mode_V(&t), *msg = Mode_V(&t);
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	Native_SetFloat(one, 1.0);
	if (!Mode_ActionSet("Walk") || !Mode_VObj(newme, "Name", name) || !Cc_GSet("Cam", "MainTarget", name) ||
		!Cc_GSet("Cam", "Target", name) || !Mode_VObj(me, "Energy", e) || !Mode_Arith('-', e, one, e1) ||
		!Mode_VSet(newme, "Energy", e1) || !LvN_SetI(newme, "Life", 100) || !LvN_SetI(newme, "UsrControl", 0) ||
		!Mode_VObj(me, "ActCtrl", act) || !LvN_SetI(me, "Active", 0) || !LvN_SetS(me, "ActCtrl", "") || !Mode_VDel(me) ||
		!PORT_CALL(Chars, DiscardRes, NULL, "v", act) || !Cc_G("AuxInterp", aux) || !Mode_VDel(aux) ||
		!Mode_DelGlobal(CC, "Cam") || !Mode_DelGlobal(CC, "AuxInterp") || !Mode_GVarSetI("S_PitchScale", 1) ||
		!Mode_TimeSpeed(1.0) || !PORT_CALL(CharScorer, Create, NULL, "i", 0L)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(when, Mode_Time() + 0.1);
	PyObject *alarm = Script_Attr("Scrap", "SetAlarm");
	byte ok = alarm && Mode_Schedule(when, alarm, Mode_I(&t, 1), 1, NULL) && LvN_SetS(newme, "ForceAction", "FlashedEnd");
	Py_XDECREF(alarm);
	if (!ok) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Conversor.FirstPossession", sv);
	if (!LvN_EqS(sv, "1")) {
		return Mode_Done(&t, 1);
	}
	Mode_Lang("FirstPossessionWarning", msg);
	return Mode_Done(&t, Mode_SetSaveVar("Conversor.FirstPossession", Mode_S(&t, "0")) &&
		PORT_CALL(Menu, ShowWarning, NULL, "iv", 0L, msg));
}

/* the adapters (the arity errors of the script's functions) */
MODE_FN(CharConversor, GetNearestConversionPoint, 1, Cc_GetNearestConversionPoint(&a[0], out))
MODE_FN(CharConversor, CreateConversorClass, 0, Cc_CreateConversorClass())
MODE_FN(CharConversor, Init, 0, Cc_Init())
MODE_FN(CharConversor, ConvertChar, 4, Cc_ConvertChar(&a[0], &a[1]))
MODE_FN(CharConversor, CPPos, 1, Cc_CPPos(&a[0], out))
MODE_FN(CharConversor, CPAng, 1, Cc_CPAng(&a[0], out))
MODE_FN(CharConversor, AddInterfaceContent, 1, Cc_AddInterfaceContent(&a[0]))
MODE_FN(CharConversor, SpawnChar, 2, Cc_SpawnChar(&a[0], &a[1]))
MODE_FN(CharConversor, MarkSpawnWaypoint, 1, Cc_MarkSpawnWaypoint(&a[0]))
MODE_FN(CharConversor, EnterCharConversor, 4, Cc_EnterCharConversor(&a[0], &a[1], &a[2], &a[3]))
MODE_FN(CharConversor, InitOfScene, 3, Cc_Fx(1))
MODE_FN(CharConversor, EndOfScene, 3, Cc_EndOfScene(&a[0]))
MODE_FN(CharConversor, InitInterfaceEvent, 3, PORT_OWN(CharConversor, AddInterfaceContent, NULL, "i", 0L))
MODE_FN(CharConversor, ExitCharConversor, 2, Cc_ExitCharConversor(&a[0]))
MODE_FN(CharConversor, ShowCharDesc, 3, Cc_ShowCharDesc(&a[0], &a[1], &a[2]))
MODE_FN(CharConversor, HideCharDesc, 3, Cc_HideCharDesc(&a[0], &a[1]))
MODE_FN(CharConversor, Convert, 2, Cc_Convert(&a[1]))
MODE_FN(CharConversor, ConvertExecute, 0, Cc_ConvertExecute())
MODE_FN(CharConversor, Possession, 2, Cc_Possession(&a[0], &a[1]))
MODE_FN(CharConversor, NewPlayer, 2, Cc_NewPlayer(&a[0], &a[1]))

static PyObject *CharConversor_DrawBackground(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", "elem", "posX", "posY", "sizeX", "sizeY", "alpha", "red", "green", "blue", NULL };
	PyObject *id, *elem, *o[9] = { NULL };
	nativeVal_t a[2], v[9], *pv[9] = { NULL };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|OOOOOOOO", names, &id, &elem, &o[0], &o[1], &o[2], &o[3], &o[5],
		&o[6], &o[7], &o[8])) {
		return NULL;
	}
	Native_Arg(id, &a[0]);
	Native_Arg(elem, &a[1]);
	for (int i = 0; i < 9; i++) {
		Native_SetNone(&v[i]);
		if (o[i]) {
			Native_Arg(o[i], &v[i]);
			pv[i] = &v[i];
		}
	}
	byte ok = Cc_DrawBackground(&a[0], &a[1], pv);
	for (int i = 0; i < 9; i++) {
		Native_Free(&v[i]);
	}
	Native_Free(&a[0]);
	Native_Free(&a[1]);
	return Mode_Result(ok);
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

PORTS(CharConversor)
