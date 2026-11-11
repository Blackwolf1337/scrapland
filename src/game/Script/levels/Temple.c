/*
 * The level Temple in C (levels/temple/scripts, issue #2): the functions of Map, MapExtraLifes and the missions
 * ArchbishopDead, AskGus, MeetArchbishop and TalkToDeepThroat that are logic; their plain functions are data
 * (the files in data/levels/temple). The module variables (the discardable characters of Map, Lifes and TimeToSpawn,
 * the proofs of ArchbishopDead, FXSceneYakuzziFile) stay in the scripts.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values (Levels.h) and call the engine in C
 * (save variables, Scrap.Get and Scrap.Set, the dummy search, the time, the language texts, the action set, the
 * vector math, the textures). The variables of the modules are read where the script reads them and written through
 * at once (OBJLIB 5); MapExtraLifes.Lifes holds the handles of the lives and stays the list object of the module,
 * CharConversor.AvailableChars, Alarm.SetAlarmCallback, CharsNPC.AfterBrowseNPC and the callbacks of the characters are
 * changed in place on their objects, the gatekeeper groups are passed as the objects of GateKeeper (OBJLIB 5); a
 * variable of another module that the script set with Script_SetVar keeps dropping its error. Entities are handle
 * values (OBJLIB 7). The engine functions the cases record or swap go through their seams (OBJLIB 7):
 * Scrap.AddScheduledFunc, Scrap.UsrEntity, Scrap.CreateEntity (fr6.py), Scrap.Set, Scrap.SetAlarm, Scrap.GetAlarm,
 * SScorer.SetMsgText, SScorer.SetLabelText, SSound.PlaySound and SLogic.SetOnFloor (fc-tp-map), the SAct functions of
 * the fake speech and SSound.SetMusic (fr6b.py); SFX.BishopSellLife (fc-tp-map swaps it) through its ref (stage 7).
 *
 * Direct calls (stage 3): the functions of the other ports (MissionsFuncs, SaveGame, GateKeeper, Elevator, Metro,
 * CharsNPC, MisItems, Scene) and the own functions of the modules (their globals) go through their refs (PORT_CALL,
 * PORT_LOWN, LV_CALLBACK; the seam takes the call by name when a mission or a case has put something else there,
 * OBJLIB 5). The GateKeeper refs take Python objects and always go by name. By name: the sections of the .def files
 * (ShowDeepThroatMessage, DeepThroatCalls, LoadYakuzziScene: Tp_Do, OBJLIB 6); string.splitfields of a value that is no
 * text is the native twin. The functions given to the scheduler (Speech.AddRemoteCom of Tp_ComAt, Mode_Schedule,
 * Ma_Soon) and to the entities (the UseFunc and OnTake texts) take the entry of a port's own function (stage 4:
 * Port_Scheduled, PYF_ENTRY); the ones given to Elevator, CharAct, Alarm, CharsNPC, DTritusDesktop, Scene and Speech
 * as callbacks are values of their variables (OBJLIB 5).
 */
#include <limits.h>
#include <string.h>

#include "scrap/input.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL temple

#define GM "Map"
#define XL "MapExtraLifes"
#define AD "Mission_ArchbishopDead"
#define AG "Mission_AskGus"
#define TD "Mission_TalkToDeepThroat"
#define MA "Mission_MeetArchbishop"

/* ---- shared lines ---- */

/* the same with the global name of src (a function) */
static byte Tp_SetVarG(const char *module, const char *var, const char *src, const char *name) {
	nativeVal_t f;
	if (!LvN_GObj(src, name, &f)) {
		return 0;
	}
	LvN_SetVar(module, var, &f);
	Native_Free(&f);
	return 1;
}

/* fn() through the globals of module, the result dropped: the sections of the .def files, by name (OBJLIB 6) */
static byte Tp_Do(const char *module, const char *fn) {
	return LV_OWN(module, fn, NULL, "");
}

/* the text of an argument of an engine function ("s" of argument arg) */
static byte Tp_Str(const nativeVal_t *v, int arg, const char **s) {
	return Native_ArgStr(v, arg, s);
}

/* Scrap.AddScheduledFunc(Scrap.GetTime() + delay, Speech.AddRemoteCom, (key, who, the global then of module)) */
static byte Tp_ComAt(double delay, const char *key, const char *who, const char *module, const char *then) {
	nativeVal_t a[3];
	PyObject *add = Script_Attr("Speech", "AddRemoteCom");
	if (!add) {
		return 0;
	}
	LvN_Str(&a[0], key);
	LvN_Str(&a[1], who);
	byte ok = LvN_GObj(module, then, &a[2]) && Mode_LaterPy(delay, add, a, 3, NULL);
	for (int i = 0; i < 3; i++) {
		Native_Free(&a[i]);
	}
	Py_DECREF(add);
	return ok;
}

/* e.UseFunc = func of the entity called name */
static byte Tp_UseFunc(const char *name, const char *func) {
	nativeVal_t e;
	byte ok = LvN_Entity(name, &e) && LvN_SetS(&e, "UseFunc", func);
	Native_Free(&e);
	return ok;
}

/* a gatekeeper on its door against a foe group of GateKeeper, the door used through UseDoor */
static byte Tp_Gate(const char *keeper, const char *door, const char *group, const char *use) {
	/* OBJLIB 5: the group is the object of GateKeeper, as the script passes it */
	PyObject *g = Mode_Import("GateKeeper") ? Script_Attr("GateKeeper", group) : NULL;
	byte ok = g && (!door || PORT_CALL(GateKeeper, SetDoor, NULL, "ss", keeper, door)) &&
		PORT_CALL(GateKeeper, DoorFoe, NULL, "sP", keeper, g) && Tp_UseFunc(door ? door : "DM_Door_Teleporter_01", use);
	Py_XDECREF(g);
	return ok;
}

/* SFX.BishopSellLife(attacker, attacked) through its ref (fc-tp-map swaps it: the seam of PORT_CALL calls what it
 * puts there) */
static byte Tp_SellLife(const char *attacker, const nativeVal_t *attacked) {
	return PORT_CALL(SFX, BishopSellLife, NULL, "sv", attacker, attacked);
}

/* float(Scrap.GetSaveVar(name)) */
static byte Tp_SaveFloat(const nativeVal_t *name, nativeVal_t *out) {
	const char *s;
	nativeVal_t v;
	Native_SetNone(out);
	if (!Tp_Str(name, 1, &s)) {
		return 0;
	}
	Mode_SaveVar(s, &v);
	byte ok = Native_ToFloat(&v, out);
	Native_Free(&v);
	return ok;
}

static byte Tp_SaveFloatS(const char *name, nativeVal_t *out) {
	nativeVal_t n;
	LvN_Str(&n, name);
	byte ok = Tp_SaveFloat(&n, out);
	Native_Free(&n);
	return ok;
}

/* Scrap.SetSaveVar(name, `v`) with name a value ("ss": the TypeError of a name that is no text) */
static byte Tp_SaveReprAt(const nativeVal_t *name, const nativeVal_t *v) {
	const char *s;
	if (!Tp_Str(name, 1, &s)) {
		return 0;
	}
	LvN_SaveRepr(s, v);
	return 1;
}

/* the dummy search of the player's arrival, the player there and on the floor */
static byte Tp_Arrive(const char *dummyName) {
	nativeVal_t dummy, v, pos, name;
	Native_SetNone(&v);
	Native_SetNone(&pos);
	Native_SetNone(&name);
	LvN_Dummy(dummyName, 0, &dummy);
	byte ok = Mode_UsrHandle(&v) && LvN_Item(&dummy, 1, &pos) && Mode_VSet(&v, "Pos", &pos) && Mode_VGet(&v, "Name", &name) &&
		LvN_SetOnFloor(&name);
	Native_Free(&dummy);
	Native_Free(&v);
	Native_Free(&pos);
	Native_Free(&name);
	return ok;
}

/* ---- Map ---- */

static byte TpMap_InitStatus(void) {
	static const struct {
		const char *who, *flag;
		long n;
	} repos[] = { { "BankDirector", "_BankDirector", 3 }, { "Bishop", "_Bishop", 5 }, { "Functionary", "_Functionary", 3 } };
	nativeVal_t v;
	if (!Mode_Import("Init") || !Mode_Import("Elevator") || !Mode_Import("CharsNPC") || !Mode_Import("Messenger")) {
		return 0;
	}
	Native_SetInt(&v, 0);
	LvN_SetVar("Init", "Outdoor", &v);
	LvN_Str(&v, "Commercial");
	LvN_SetVar("Init", "OutArea", &v);
	Native_Free(&v);
	Native_SetInt(&v, 0);
	LvN_SetVar("Messenger", "MakeMDP", &v);
	if (!PORT_CALL(Elevator, SetDef, NULL, "ss", "Metro", "DM_Door_Metro_Templo00") ||
		!PORT_CALL(Elevator, SetDef, NULL, "ss", "MetroExt", "DM_Door_Metro_Templo00") ||
		!PORT_CALL(Elevator, SetDef, NULL, "ss", "Elevator", "DM_Door_Elevator_Commercial") ||
		!PORT_CALL(Elevator, SetDef, NULL, "ss", "TempleTeleporter", "DM_Door_TempleTeleporter_GDB") ||
		!PORT_CALL(CharsNPC, SetReposChars, NULL, "si", "Sentinel", 3L) || !PORT_CALL(CharsNPC, SetReposChars, NULL, "si", "Police", 3L) ||
		!PORT_CALL(CharsNPC, SetReposChars, NULL, "si", "Nurse", 3L)) {
		return 0;
	}
	/* if not Scrap.Get("IsXbox") or _<Char>: */
	for (int i = 0; i < 3; i++) {
		int on = LvN_PcOr(GM, repos[i].flag);
		if (on < 0 || (on && !PORT_CALL(CharsNPC, SetReposChars, NULL, "si", repos[i].who, repos[i].n))) {
			return 0;
		}
	}
	return 1;
}

static byte TpMap_Init(void) {
	nativeVal_t a, b;
	if (!Mode_Import("Metro") || !Mode_Import("Elevator") || !Mode_Connect("DM_Door_TempleTeleporter_GDB", "GDB", 0) ||
		!Mode_Connect("DM_Door_Elevator_Commercial", "Commercial", 0)) {
		return 0;
	}
	Native_SetEmpty(&a, NV_LIST);
	LvN_Str(Native_Append(&a), "DM_Door_Teleporter_01");
	LvN_Str(Native_Append(&a), "DM_Door_Teleporter_06");
	Native_SetEmpty(&b, NV_LIST);
	byte ok = PORT_CALL(Elevator, ConnectElevators, NULL, "vvi", &a, &b, 0L);
	Native_Free(&a);
	Native_Free(&b);
	if (!ok || !PORT_CALL(Metro, SetAsMetro, NULL, "s", "DM_Door_Metro_Templo00") ||
		!Tp_Gate("GateKeeper_Portalis", "DM_Door_Teleporter_01", "BishopGroupGateKeeper", "GateKeeper.UseDoor")) {
		return 0;
	}
	if (LvN_SaveIs("ComeFrom", "DoorTempleTeleporter") && !Tp_SetVarG("Elevator", "OnElevatorReachDown", GM, "EndOfAppears")) {
		return 0;
	}
	if (!Tp_Gate("GateKeeper_GDB", "DM_Door_TempleTeleporter_GDB", "BishopGroupGateKeeper", "GateKeeper.UseDoor")) {
		return 0;
	}
	return !LvN_SaveIs("Mission.Library", "Mission_MeetArchbishop") || Mode_GVarSetI("isActionIndoor", 0);
}

/* EndOfAppears(elv): the arrival by the teleporter from GDB, the player on the floor at its spawn */
static byte TpMap_EndOfAppears(void) {
	nativeVal_t none;
	if (!Mode_Import("Elevator")) {
		return 0;
	}
	Native_SetNone(&none);
	LvN_SetVar("Elevator", "OnElevatorReachDown", &none);
	return Tp_Arrive("DM_Player_Spawn_GDB");
}

LV_CORE0(TpMap_InitStatus)
LV_CORE0(TpMap_Init)
LV_CORE0(TpMap_EndOfAppears)

LV_FN(TpMap, InitStatusPy, 1, TpMap_InitStatus_C)
LV_FN(TpMap, InitPy, 1, TpMap_Init_C)
LV_FN(TpMap, EndOfAppearsPy, 1, TpMap_EndOfAppears_C)

PyMethodDef g_TempleMapMethods[] = {
	{ "InitStatus", TpMap_InitStatusPy, 1, NULL },
	{ "Init", TpMap_InitPy, 1, NULL },
	{ "EndOfAppears", TpMap_EndOfAppearsPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- MapExtraLifes: the extra lives of the temple, back after TimeToSpawn; raised while the alarm is on ---- */

/* TakeExtraLife(ItemName, UsrName): a life (up to the most), the life away for TimeToSpawn, the alarm */
static byte Xl_TakeExtraLife(const nativeVal_t *item, const nativeVal_t *usr) {
	nativeVal_t l, nl, lives, gt, now, t, max, spawn, take, key, a, a0, a1, pos, one, alarm;
	if (!Mode_Import("Sound") || !Mode_Import("Speech")) {
		return 0;
	}
	Native_SetNone(&l);
	Native_SetNone(&lives);
	Native_SetNone(&gt);
	Native_SetNone(&t);
	Native_SetNone(&max);
	Native_SetNone(&spawn);
	Native_SetNone(&take);
	Native_SetNone(&a);
	Native_SetNone(&a0);
	Native_SetNone(&a1);
	Native_SetNone(&pos);
	Native_SetNone(&nl);
	byte ok = Mode_EntityH(item, &l);
	if (ok) {
		SaveVar_Inc("Stats.TempleLife", 1);
		Mode_SaveVar("Player.NumLives", &nl);
		ok = Native_ToInt(&nl, &lives) && Tp_SaveFloatS("GameplayTime", &gt);
	}
	if (ok) {
		Native_SetFloat(&now, Mode_Time());
		ok = Native_Arith('+', &gt, &now, &t) && Tp_SaveReprAt(item, &t) && Native_GVarGet("MaxPlayerLives", &max);
	}
	if (ok && Native_Compare(&lives, &max) < 0) {
		nativeVal_t more, cad;
		Native_SetInt(&one, 1);
		Native_SetNone(&more);
		ok = Native_Arith('+', &lives, &one, &more);
		if (ok) {
			LvN_SaveRepr("Player.NumLives", &more);
			Mode_Lang("StealLife_SystemMessage", &cad);
			ok = Mode_SpeechMsg(&cad) && Tp_SellLife("", usr);
			Native_Free(&cad);
		}
		Native_Free(&more);
	}
	ok = ok && Mode_G(XL, "TimeToSpawn", &spawn) && Mode_VSet(&l, "rTime", &spawn) && LvN_ModGet("Sound", "ItemTake", &take);
	if (ok) {
		LvN_Str(&key, "TakeExtraLife");
		ok = Native_Subscript(&take, &key, &a) && LvN_Item(&a, 0, &a0) && LvN_Item(&a, 1, &a1) && Mode_VGet(&l, "Pos", &pos) &&
			LvN_PlaySound(&pos, &a0, &a1);
		Native_Free(&key);
	}
	if (ok) {
		Native_SetInt(&alarm, 1);
		ok = Mode_SetAlarm(&alarm) && Mode_GVarSetI("BishopAttackOnAlarm", 1);
	}
	Native_Free(&l);
	Native_Free(&nl);
	Native_Free(&lives);
	Native_Free(&gt);
	Native_Free(&t);
	Native_Free(&max);
	Native_Free(&spawn);
	Native_Free(&take);
	Native_Free(&a);
	Native_Free(&a0);
	Native_Free(&a1);
	Native_Free(&pos);
	return ok;
}

/* SetRespawnTime(l): the life comes back TimeToSpawn after it was taken (at least 0.1 s from now) */
static byte Xl_SetRespawnTime(const nativeVal_t *l) {
	nativeVal_t name, taken, gt, d, spawn, b, now, rt, lim;
	Native_SetNone(&name);
	Native_SetNone(&taken);
	Native_SetNone(&gt);
	Native_SetNone(&d);
	Native_SetNone(&spawn);
	Native_SetNone(&b);
	Native_SetNone(&rt);
	byte ok = Mode_VGet(l, "Name", &name) && Tp_SaveFloat(&name, &taken) && Tp_SaveFloatS("GameplayTime", &gt) &&
		Native_Arith('-', &taken, &gt, &d) && Mode_G(XL, "TimeToSpawn", &spawn) && Native_Arith('+', &d, &spawn, &b);
	if (ok) {
		Native_SetFloat(&now, Mode_Time());
		Native_SetFloat(&lim, 0.1);
		ok = Native_Arith('-', &b, &now, &rt) && Mode_VSet(l, "rTime", Native_Compare(&rt, &lim) < 0 ? &lim : &rt);
	}
	Native_Free(&name);
	Native_Free(&taken);
	Native_Free(&gt);
	Native_Free(&d);
	Native_Free(&spawn);
	Native_Free(&b);
	Native_Free(&rt);
	return ok;
}

/* one life of MoveLifes: up or down by dist; one that is there glides, a taken one jumps */
static byte Xl_MoveLife(const nativeVal_t *l, const nativeVal_t *dist) {
	nativeVal_t mp, off, to, ang, rt, zero, p;
	Native_SetNone(&mp);
	Native_SetNone(&to);
	Native_SetNone(&ang);
	Native_SetNone(&rt);
	Native_SetNone(&p);
	Native_SetEmpty(&off, NV_TUPLE);
	Native_SetInt(Native_Append(&off), 0);
	Native_Copy(Native_Append(&off), dist);
	Native_SetInt(Native_Append(&off), 0);
	byte ok = Mode_VGet(l, "MovePos", &mp) && Mode_VecAdd(&mp, &off, &to) && Mode_VSet(l, "MovePos", &to) &&
		Mode_VGet(l, "Ang", &ang) && Mode_VSet(l, "MoveAng", &ang) && Mode_VGet(l, "rTime", &rt);
	if (ok) {
		Native_SetFloat(&zero, 0.0);
		if (!Native_Compare(&rt, &zero)) {
			ok = PORT_LOWN(MapExtraLifes, SetRespawnTime, NULL, "v", l) && LvN_SetF(l, "MoveTime", 0.3);
		} else {
			ok = LvN_SetF(l, "MoveTime", 0.0) && Mode_VGet(l, "MovePos", &p) && Mode_VSet(l, "Pos", &p);
		}
	}
	Native_Free(&mp);
	Native_Free(&off);
	Native_Free(&to);
	Native_Free(&ang);
	Native_Free(&rt);
	Native_Free(&p);
	return ok;
}

/* MoveLifes(dist): the lives of the list Lifes (the list object of the module: it holds the handles, OBJLIB 5 and 7) */
static byte Xl_MoveLifes(const nativeVal_t *dist) {
	PyObject *lifes = Mode_GlobalPy(XL, "Lifes");
	if (!lifes) {
		return 0;
	}
	byte ok = 1;
	for (int i = 0; ok; i++) {
		PyObject *o;
		int more = Script_Next(lifes, i, &o);
		if (more <= 0) {
			ok = more == 0;
			break;
		}
		nativeVal_t l;
		Native_Arg(o, &l);
		Py_DECREF(o);
		ok = Xl_MoveLife(&l, dist);
		Native_Free(&l);
	}
	Py_DECREF(lifes);
	return ok;
}

/* MoveLifes(d) of the module through its global */
static byte Xl_Move(long d) {
	return PORT_LOWN(MapExtraLifes, MoveLifes, NULL, "i", d);
}

/* SwitchAlarm(status): called again on the next alarm change; the lives up with the alarm, down after it */
static byte Xl_SwitchAlarm(const nativeVal_t *status) {
	if (!Mode_Import("Alarm") || !LvN_ModAppendG("Alarm", "SetAlarmCallback", XL, "SwitchAlarm")) {
		return 0;
	}
	if (Native_True(status)) {
		return Xl_Move(600);
	}
	return Mode_GVarSetI("BishopAttackOnAlarm", 0) && Xl_Move(-600);
}

/* -v of the script (PyNumber_Negative: -0.0 for 0.0, the OverflowError of the smallest int, others by Python) */
static byte Xl_Neg(const nativeVal_t *v, nativeVal_t *out) {
	Native_SetNone(out);
	if (v->type == NV_INT && v->i != LONG_MIN) {
		Native_SetInt(out, -v->i);
		return 1;
	}
	if (v->type == NV_FLOAT) {
		Native_SetFloat(out, -v->f);
		return 1;
	}
	PyObject *o = Native_ToPy(v), *r = o ? PyNumber_Negative(o) : NULL;
	if (r) {
		Native_Arg(r, out);
	}
	Py_XDECREF(r);
	Py_XDECREF(o);
	return r != NULL;
}

/* the life item of a dummy (name, pos, ang): its save variable, the item, its respawn time, into Lifes */
static byte Xl_Life(const nativeVal_t *dummy) {
	nativeVal_t name, spawn, neg, pos, c[3], l, model, radius, v;
	nativeBuf_t rep;
	const char *s;
	Native_SetNone(&name);
	Native_SetNone(&spawn);
	Native_SetNone(&neg);
	Native_SetNone(&pos);
	Native_SetNone(&l);
	Native_SetNone(&model);
	Native_SetNone(&radius);
	Native_SetNone(&v);
	byte ok = LvN_Item(dummy, 0, &name) && Mode_G(XL, "TimeToSpawn", &spawn) && Xl_Neg(&spawn, &neg) && Tp_Str(&name, 1, &s);
	if (ok) {
		Native_BufInit(&rep);
		Native_Repr(&rep, &neg, NR_REPR);
		SaveVar_Create(s, rep.p);
		Native_BufFree(&rep);
		ok = PORT_LOWN(MapExtraLifes, GetMoney, NULL, "v", &name);
	}
	int got = 0;
	ok = ok && LvN_Item(dummy, 1, &pos);
	for (; ok && got < 3; got++) {
		ok = LvN_Item(&pos, got, &c[got]);
	}
	if (!ok && got) {
		Native_Free(&c[--got]);
	}
	ok = ok && LvN_Create(&name, &c[0], &c[1], &c[2], "Item", &l);
	while (got-- > 0) {
		Native_Free(&c[got]);
	}
	ok = ok && PORT_CALL(MisItems, GetObjModel, &model, "s", "ExtraLife") && Mode_VModel(&l, &model) &&
		PORT_LOWN(MapExtraLifes, SetRespawnTime, NULL, "v", &l) && LvN_SetI(&l, "OnlyUser", 1) &&
		LvN_ModGet("MisItems", "COIN_RADIUS_INDOOR", &radius) && Mode_VSet(&l, "Radius", &radius) &&
		LvN_SetS(&l, "OnTake", "MapExtraLifes.TakeExtraLife") && Mode_VGet(&l, "Pos", &v) && Mode_VSet(&l, "MovePos", &v);
	Native_Free(&v);
	ok = ok && Mode_VGet(&l, "Ang", &v) && Mode_VSet(&l, "MoveAng", &v) && Mode_Append(XL, "Lifes", &l);
	Native_Free(&name);
	Native_Free(&spawn);
	Native_Free(&neg);
	Native_Free(&pos);
	Native_Free(&l);
	Native_Free(&model);
	Native_Free(&radius);
	Native_Free(&v);
	return ok;
}

/* Init(LevelPath): a life at every DM_ExtraLife dummy; up at once when the alarm is on, else when it comes */
static byte Xl_Init(void) {
	nativeVal_t gt;
	if (!Mode_Import("MisItems") || !Mode_Import("Alarm")) {
		return 0;
	}
	Mode_DummyStart("DM_ExtraLife_*", 1);
	if (!Tp_SaveFloatS("GameplayTime", &gt)) {
		return 0;
	}
	Native_Free(&gt);
	for (;;) {
		nativeVal_t d;
		Mode_DummyNext(&d);
		byte end = d.type == NV_NONE;
		byte ok = end || Xl_Life(&d);
		Native_Free(&d);
		if (!ok) {
			return 0;
		}
		if (end) {
			break;
		}
	}
	int on = Mode_AlarmOn();
	if (on < 0) {
		return 0;
	}
	if (on) {
		return PORT_LOWN(MapExtraLifes, SwitchAlarm, NULL, "i", 1L);
	}
	return LvN_ModAppendG("Alarm", "SetAlarmCallback", XL, "SwitchAlarm");
}

/* GetMoney(name): the number after the second "_" of the name */
static byte Xl_GetMoney(const nativeVal_t *name, nativeVal_t *out) {
	nativeVal_t parts, p;
	if (!Mode_Import("string")) {
		return 0;
	}
	Native_SetNone(&parts);
	Native_SetNone(&p);
	/* string.splitfields(name, "_") of the string port: a text in C, any other value by name (its error) */
	byte ok = name->type == NV_STR ? Native_Split(name, "_", &parts) : PORT_CALL(string, splitfields, &parts, "vs", name, "_");
	ok = ok && LvN_Item(&parts, 2, &p) && Native_ToFloat(&p, out);
	Native_Free(&parts);
	Native_Free(&p);
	return ok;
}

static byte Xl_TakeExtraLife_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Xl_TakeExtraLife(&a[0], &a[1]);
}

static byte Xl_SetRespawnTime_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Xl_SetRespawnTime(&a[0]);
}

static byte Xl_MoveLifes_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Xl_MoveLifes(&a[0]);
}

static byte Xl_SwitchAlarm_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Xl_SwitchAlarm(&a[0]);
}

static byte Xl_GetMoney_C(const nativeVal_t *a, nativeVal_t *out) {
	return Xl_GetMoney(&a[0], out);
}

LV_CORE0(Xl_Init)

LV_FN(Xl, TakeExtraLifePy, 2, Xl_TakeExtraLife_C)
LV_FN(Xl, SetRespawnTimePy, 1, Xl_SetRespawnTime_C)
LV_FN(Xl, MoveLifesPy, 1, Xl_MoveLifes_C)
LV_FN(Xl, SwitchAlarmPy, 1, Xl_SwitchAlarm_C)
LV_FN(Xl, InitPy, 1, Xl_Init_C)
LV_FN(Xl, GetMoneyPy, 1, Xl_GetMoney_C)

PyMethodDef g_TempleXlMethods[] = {
	{ "TakeExtraLife", Xl_TakeExtraLifePy, 1, NULL },
	{ "SetRespawnTime", Xl_SetRespawnTimePy, 1, NULL },
	{ "MoveLifes", Xl_MoveLifesPy, 1, NULL },
	{ "SwitchAlarm", Xl_SwitchAlarmPy, 1, NULL },
	{ "Init", Xl_InitPy, 1, NULL },
	{ "GetMoney", Xl_GetMoneyPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- the missions ---- */

/* Init(LevelPath): the forbidden area, the gatekeeper against the police; the photos of the proofs or Deep Throat */
static byte Ad_Init(void) {
	if (!Mode_GVarSetS("GuaranteedChars", ",Police,Gear,Sentinel,PoliceBoss,") || !PORT_LOWN(Mission_ArchbishopDead, ForbiddenMsg, NULL, "") ||
		!Tp_Gate("GateKeeper_Portalis", "DM_Door_Teleporter_01", "PoliceGroupGateKeeper", "GateKeeper.UseDoor")) {
		return 0;
	}
	if (!LvN_SaveIs("Mission_ArchbishopDead.Status", "Photo")) {
		return Tp_Do(AD, "ShowDeepThroatMessage");
	}
	return LvN_Com("Mission_ArchbishopKilled_RemoteMessage7", "Humphrey", NULL, NULL, NULL) &&
		Tp_SetVarG("CharAct", "TakenPhoto", AD, "TakeProofPhoto") && PORT_LOWN(Mission_ArchbishopDead, CreateViscProofs, NULL, "");
}

/* ForbiddenMsg(): the forbidden area label, again each second */
static byte Ad_ForbiddenMsg(void) {
	nativeVal_t label;
	double time = Mode_Time();
	Mode_Lang("Generics_ForbiddenArea_Label", &label);
	byte ok = Mode_LabelText(&label, time + 0.5);
	Native_Free(&label);
	if (!ok) {
		return 0;
	}
	nativeVal_t t, nm;
	PyObject *f = Mode_GlobalPy(AD, "ForbiddenMsg");
	Native_SetFloat(&t, time + 1.0);
	LvN_Str(&nm, "ForbiddenMsg");
	ok = f && Mode_Schedule(&t, f, NULL, 0, &nm);
	Native_Free(&nm);
	Py_XDECREF(f);
	return ok;
}

/* CreateViscProofs(): the three proofs to photograph at the DM_ViscProof dummies */
static byte Ad_CreateViscProofs(void) {
	char name[0x20];
	Mode_DummyStart("DM_ViscProof*", 1);
	byte ok = 1;
	for (int i = 1; i < 4 && ok; i++) {
		nativeVal_t d, pos, ang, nm, use, prf, p, off, v, pn;
		Native_SetNone(&pos);
		Native_SetNone(&ang);
		Native_SetNone(&prf);
		Native_SetNone(&p);
		Native_SetNone(&v);
		Native_SetNone(&pn);
		Mode_DummyNext(&d);
		if (!Native_True(&d)) {
			Native_Free(&d);
			return LvN_RaiseText("Uno de los dummies de las pruebas del viscoson no existe");
		}
		Str_Printf(name, sizeof(name), "ViscProof%d", i);
		LvN_Str(&nm, name);
		LvN_Str(&use, "Mission_ArchbishopDead.ViewProof");
		Native_SetEmpty(&off, NV_TUPLE);
		Native_SetInt(Native_Append(&off), 0);
		Native_SetInt(Native_Append(&off), -25);
		Native_SetInt(Native_Append(&off), 0);
		ok = LvN_DummyPosAng(&d, &pos, &ang) && PORT_CALL(MissionsFuncs, CreatePhotoObject, &prf, "vvvv", &nm, &pos, &ang, &use) &&
			Mode_VGet(&prf, "Pos", &p) && Mode_VecAdd(&p, &off, &v) && Mode_VSet(&prf, "MovePos", &v) && Mode_VSet(&prf, "Pos", &v);
		if (ok) {
			/* ProofsNames.append(prf.Name): the list read before the name, as the script reads them */
			PyObject *names = Mode_GlobalPy(AD, "ProofsNames"), *x = NULL, *r = NULL;
			ok = names && Mode_VGet(&prf, "Name", &pn) && (x = Native_ToPy(&pn)) != NULL &&
				(r = Script_Method(names, "append", "(O)", x)) != NULL;
			Py_XDECREF(r);
			Py_XDECREF(x);
			Py_XDECREF(names);
		}
		Native_Free(&d);
		Native_Free(&pos);
		Native_Free(&ang);
		Native_Free(&nm);
		Native_Free(&use);
		Native_Free(&prf);
		Native_Free(&p);
		Native_Free(&off);
		Native_Free(&v);
		Native_Free(&pn);
	}
	return ok;
}

static byte Ad_ViewProof(void) {
	return Mode_LangMsg("Mission_ArchbishopDead_ViewProofHint");
}

/* TakeProofPhoto(proofName): a photo of a proof (1), the third ends the photos; 0 for another object */
static byte Ad_TakeProofPhoto(const nativeVal_t *proof, nativeVal_t *out) {
	nativeVal_t names, e, n, one, m, none;
	if (!Mode_G(AD, "ProofsNames", &names)) {
		return 0;
	}
	int in = Native_Contains(&names, proof);
	Native_Free(&names);
	if (in <= 0) {
		Native_SetInt(out, 0);
		return in == 0;
	}
	Native_SetNone(&e);
	Native_SetNone(&n);
	Native_SetNone(&m);
	Native_SetInt(&one, 1);
	byte ok = Mode_EntityH(proof, &e) && PORT_CALL(MissionsFuncs, UnsetPhotoObject, NULL, "v", &e) &&
		Mode_G(AD, "NumProofsWithPhoto", &n) && Native_Arith('+', &n, &one, &m) && Mode_Put(AD, "NumProofsWithPhoto", NULL, &m);
	if (ok && LvN_EqI(&m, 3)) {
		Native_SetNone(&none);
		LvN_SetVar("CharAct", "TakenPhoto", &none);
		ok = Tp_ComAt(1.0, "Mission_ArchbishopKilled_RemoteMessage2", "Humphrey", AD, "ShowDeepThroatMessage");
		if (ok) {
			SaveVar_Set("Mission_ArchbishopDead.Status", "DeepThroat");
		}
	}
	Native_Free(&e);
	Native_Free(&n);
	Native_Free(&m);
	Native_SetInt(out, 1);
	return ok;
}

/* AfterDeepThroatMessageShown(): the first time the next mission, then Deep Throat calls again each minute */
static byte Ad_AfterDeepThroatMessageShown(void) {
	nativeVal_t marked;
	if (!Mode_G(AD, "NextMissionMarked", &marked)) {
		return 0;
	}
	byte first = LvN_EqI(&marked, 0);
	Native_Free(&marked);
	const char *msg = "Mission_FirstMeetDeepThroat_RemoteMessage5";
	if (first) {
		msg = "Mission_FirstMeetDeepThroat_RemoteMessage4";
		if (!LvN_PutI(AD, "NextMissionMarked", 1) || !Mode_Import("Init") || !PORT_CALL(SaveGame, QWayPoint, NULL, "") ||
			!PORT_CALL(MissionsFuncs, SetNextMission, NULL, "ss", "Mission_FirstMeetDeepThroat", "Commercial") ||
			!PORT_CALL(SaveGame, MMDesc, NULL, "si", "Mission_FirstMeetDeepthroat_MissionText1", 1L)) {
			return 0;
		}
		SaveVars_Delete("Mission_ArchbishopDead");
	}
	return Tp_ComAt(60.0, msg, "DeepThroat", AD, "AfterDeepThroatMessageShown");
}

LV_CORE0(Ad_Init)
LV_CORE0(Ad_ForbiddenMsg)
LV_CORE0(Ad_CreateViscProofs)
LV_CORE0(Ad_ViewProof)
LV_CORE0(Ad_AfterDeepThroatMessageShown)

static byte Ad_TakeProofPhoto_C(const nativeVal_t *a, nativeVal_t *out) {
	return Ad_TakeProofPhoto(&a[0], out);
}

LV_FN(Ad, InitPy, 1, Ad_Init_C)
LV_FN(Ad, ForbiddenMsgPy, 0, Ad_ForbiddenMsg_C)
LV_FN(Ad, CreateViscProofsPy, 0, Ad_CreateViscProofs_C)
LV_FN(Ad, ViewProofPy, 4, Ad_ViewProof_C)
LV_FN(Ad, TakeProofPhotoPy, 1, Ad_TakeProofPhoto_C)
LV_FN(Ad, AfterDeepThroatMessageShownPy, 0, Ad_AfterDeepThroatMessageShown_C)

PyMethodDef g_TempleArchbishopDeadMethods[] = {
	{ "Init", Ad_InitPy, 1, NULL },
	{ "ForbiddenMsg", Ad_ForbiddenMsgPy, 1, NULL },
	{ "CreateViscProofs", Ad_CreateViscProofsPy, 1, NULL },
	{ "ViewProof", Ad_ViewProofPy, 1, NULL },
	{ "TakeProofPhoto", Ad_TakeProofPhotoPy, 1, NULL },
	{ "AfterDeepThroatMessageShown", Ad_AfterDeepThroatMessageShownPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* Deep Throat's desktop as the target, its dialog by module.DeepThroatSpeech (CallDeepThroat of AskGus, Init of
 * TalkToDeepThroat) */
static byte Tp_CallDeepThroat(const char *module) {
	nativeVal_t dd, name;
	if (!Mode_Import("DTritusDesktop")) {
		return 0;
	}
	Native_SetNone(&name);
	byte ok = LvN_Entity("DM_DTritusDesktop", &dd) && Mode_VGet(&dd, "Name", &name) &&
		PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &name) &&
		Tp_SetVarG("DTritusDesktop", "DialogController", module, "DeepThroatSpeech");
	Native_Free(&dd);
	Native_Free(&name);
	return ok;
}

/* DeepThroatSpeech(UsrName, CharName, optId, FullOption) of both missions: the talk at the desktop, then
 * module.SetNextMission */
static byte Tp_DeepThroatSpeech(const char *module, const nativeVal_t *usr, const nativeVal_t *ch, const char *ni) {
	nativeVal_t dd, next;
	Native_SetNone(&next);
	byte ok = LvN_Entity("DM_DTritusDesktop", &dd) && PORT_CALL(MissionsFuncs, ActivateTarget, NULL, "vi", &dd, 0L) &&
		LvN_GObj(module, "SetNextMission", &next);
	Native_Free(&dd);
	if (!ok) {
		Native_Free(&next);
		return 0;
	}
	LvN_SetVar("Speech", "PlayerName", usr);
	LvN_SetVar("Speech", "NPCName", ch);
	LvN_SetVar("Speech", "OnEndOfSpeech", &next);
	LvN_SetVar("Speech", "OnEndOfSpeechParams", LvN_NoArgs());
	Native_Free(&next);
	return Mode_NISpeech("DeepThroat", ni);
}

/* l = eval(Scrap.GetSaveVar(var)); item added when missing, the list saved back */
static byte Ag_AddToList(const char *var, const char *item) {
	nativeVal_t l, it;
	if (!LvN_SaveList(var, AG, &l)) {
		return 0;
	}
	LvN_Str(&it, item);
	int in = Native_Contains(&l, &it);
	byte ok = in > 0 || (in == 0 && LvN_SaveAppend(var, &l, &it));
	Native_Free(&l);
	Native_Free(&it);
	return ok;
}

static byte Ag_InitStatus(void) {
	return LvN_ModAppendG("CharsNPC", "AfterBrowseNPC", AG, "AfterBrowseNPC");
}

static byte Ag_AfterBrowseNPC(void) {
	return LV_CALLBACK("Bishop_Gus", Mission_AskGus, CreateGusInit);
}

/* CreateGusInit(e): Gus talks about the mission while it is on */
static byte Ag_CreateGusInit(const nativeVal_t *e) {
	nativeVal_t d, name;
	if (!LvN_SaveIs("MissionState", "AskGus")) {
		return 1;
	}
	Native_SetNone(&name);
	Mode_Lang("CharName_Gus", &d);
	byte ok = LvN_SetS(e, "UseFunc", "Mission_AskGus.GusSpeech") && Mode_VSet(e, "Descriptor", &d) && Mode_VGet(e, "Name", &name) &&
		PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &name);
	Native_Free(&d);
	Native_Free(&name);
	return ok;
}

static byte Ag_Init(void) {
	if (LvN_SaveIs("MissionState", "DeepThroatCalls")) {
		return Tp_Do(AG, "DeepThroatCalls");
	}
	return !LvN_SaveIs("MissionState", "AfterDeepThroatCalls") || PORT_LOWN(Mission_AskGus, CallDeepThroat, NULL, "");
}

/* DeepThroatAppears(CharName): Gus as before, the bishop's ship and engine as blueprints, Deep Throat calls */
static byte Ag_DeepThroatAppears(const nativeVal_t *name) {
	nativeVal_t e;
	if (!Mode_EntityH(name, &e)) {
		return 0;
	}
	byte ok = !Native_True(&e) || LvN_SetS(&e, "UseFunc", "Bishop.BishopSpeech");
	Native_Free(&e);
	ok = ok && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "O", Py_None) && Ag_AddToList("Hangar.shipsToEditList", "SBishop1") &&
		Ag_AddToList("Hangar.availableEnginesList", "MBISHOP1");
	if (ok) {
		SaveVar_Inc("NewBluePrintAvaliable", 2);
		ok = Mode_LangMsg("Mission_TalkToGus_SystemMessage1");
	}
	if (ok) {
		SaveVar_Set("MissionState", "DeepThroatCalls");
		ok = Tp_Do(AG, "DeepThroatCalls");
	}
	return ok;
}

/* PreCallDeepThroat(): Dtritus can no longer be converted to; the call to Deep Throat */
static byte Ag_PreCallDeepThroat(void) {
	nativeVal_t chars, dt;
	if (!Mode_Import("CharConversor") || !LvN_SaveList("Conversor.AvailableChars", AG, &chars)) {
		return 0;
	}
	LvN_Str(&dt, "Dtritus");
	int in = Native_Contains(&chars, &dt);
	byte ok = in >= 0 && (!in || LvN_ListRemove(&chars, &dt));
	if (ok) {
		LvN_SaveRepr("Conversor.AvailableChars", &chars);
	}
	Native_Free(&chars);
	if (ok) {
		/* OBJLIB 5: the list object of CharConversor, changed in place */
		PyObject *av = Script_Attr("CharConversor", "AvailableChars"), *x = av ? Native_ToPy(&dt) : NULL;
		in = x ? PySequence_Contains(av, x) : -1;
		PyObject *r = in > 0 ? Script_Method(av, "remove", "(O)", x) : NULL;
		ok = in == 0 || r != NULL;
		Py_XDECREF(r);
		Py_XDECREF(x);
		Py_XDECREF(av);
	}
	Native_Free(&dt);
	ok = ok && PORT_CALL(SaveGame, MMDesc, NULL, "si", "Mission_TalkToDeepThroat4_MissionText1", 1L);
	if (ok) {
		SaveVar_Set("MissionState", "AfterDeepThroatCalls");
		ok = PORT_LOWN(Mission_AskGus, CallDeepThroat, NULL, "");
	}
	return ok;
}

static byte Ag_CallDeepThroat(void) {
	return Tp_CallDeepThroat(AG);
}

LV_CORE0(Ag_InitStatus)
LV_CORE0(Ag_AfterBrowseNPC)
LV_CORE0(Ag_Init)
LV_CORE0(Ag_PreCallDeepThroat)
LV_CORE0(Ag_CallDeepThroat)

static byte Ag_CreateGusInit_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Ag_CreateGusInit(&a[0]);
}

static byte Ag_DeepThroatAppears_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Ag_DeepThroatAppears(&a[0]);
}

static byte Ag_DeepThroatSpeech_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Tp_DeepThroatSpeech(AG, &a[0], &a[1], "Mission_TalkToDeepThroat4__ComDeepThroat_NI_1Msg");
}

LV_FN(Ag, InitStatusPy, 1, Ag_InitStatus_C)
LV_FN(Ag, AfterBrowseNPCPy, 0, Ag_AfterBrowseNPC_C)
LV_FN(Ag, CreateGusInitPy, 1, Ag_CreateGusInit_C)
LV_FN(Ag, InitPy, 1, Ag_Init_C)
LV_FN(Ag, DeepThroatAppearsPy, 1, Ag_DeepThroatAppears_C)
LV_FN(Ag, PreCallDeepThroatPy, 0, Ag_PreCallDeepThroat_C)
LV_FN(Ag, CallDeepThroatPy, 0, Ag_CallDeepThroat_C)
LV_FN(Ag, DeepThroatSpeechPy, 4, Ag_DeepThroatSpeech_C)

PyMethodDef g_TempleAskGusMethods[] = {
	{ "InitStatus", Ag_InitStatusPy, 1, NULL },
	{ "AfterBrowseNPC", Ag_AfterBrowseNPCPy, 1, NULL },
	{ "CreateGusInit", Ag_CreateGusInitPy, 1, NULL },
	{ "Init", Ag_InitPy, 1, NULL },
	{ "DeepThroatAppears", Ag_DeepThroatAppearsPy, 1, NULL },
	{ "PreCallDeepThroat", Ag_PreCallDeepThroatPy, 1, NULL },
	{ "CallDeepThroat", Ag_CallDeepThroatPy, 1, NULL },
	{ "DeepThroatSpeech", Ag_DeepThroatSpeechPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

static byte Td_Init_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Tp_CallDeepThroat(TD);
}

static byte Td_DeepThroatSpeech_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Tp_DeepThroatSpeech(TD, &a[0], &a[1], "Mission_TalkToDeepThroat5_DeepThroat_NI_1Msg");
}

LV_FN(Td, InitPy, 1, Td_Init_C)
LV_FN(Td, DeepThroatSpeechPy, 4, Td_DeepThroatSpeech_C)

PyMethodDef g_TempleTalkToDeepThroatMethods[] = {
	{ "Init", Td_InitPy, 1, NULL },
	{ "DeepThroatSpeech", Td_DeepThroatSpeechPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* the global fn of the mission at Scrap.GetTime() + 0.1 */
static byte Ma_Soon(const char *fn) {
	PyObject *f = Mode_GlobalPy(MA, fn);
	byte ok = f && Mode_LaterPy(0.1, f, NULL, 0, NULL);
	Py_XDECREF(f);
	return ok;
}

/* UseDoor(...): through the gatekeeper's door; the scene starts when the player walks again */
static byte Ma_UseDoor(const nativeVal_t *a) {
	nativeVal_t used;
	Native_SetNone(&used);
	if (!PORT_CALL(GateKeeper, UseDoor, &used, "vvvv", &a[0], &a[1], &a[2], &a[3])) {
		return 0;
	}
	int t = Native_True(&used);
	Native_Free(&used);
	return !t || (PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "s", "") && Ma_Soon("TestEndOfElevator"));
}

/* TestEndOfElevator(): the Yakuzzi scene once the player walks out of the teleporter */
static byte Ma_TestEndOfElevator(void) {
	if (strcmp(TABLE_AT(g_ActionSets, Input_GetActionSet())->name, "Walk")) {
		return Ma_Soon("TestEndOfElevator");
	}
	nativeVal_t head, e;
	if (!Tp_SetVarG("Scene", "EndOfSceneCallback", MA, "SetNextMission") || !PORT_CALL(Scene, Play, NULL, "s", "Yakuzzi") ||
		!Native_GVarGet("NewsHead", &head)) {
		return 0;
	}
	Native_SetNone(&e);
	byte ok = Mode_EntityH(&head, &e) && LvN_SetI(&e, "Active", 0);
	Native_Free(&head);
	Native_Free(&e);
	return ok;
}

/* SScorer.PreloadTexture(Scrap.Get(var)) ("s|i": the TypeError of a value that is no text) */
static byte Ma_Preload(const char *var) {
	nativeVal_t v;
	const char *s;
	if (!Native_GVarGet(var, &v)) {
		return 0;
	}
	byte ok = Tp_Str(&v, 1, &s);
	if (ok) {
		Scorer_PreloadTexture(s, 0);
	}
	Native_Free(&v);
	return ok;
}

/* CreateFakeSpeech(): the archbishop of the news panel in his yakuzzi, and the panel itself */
static byte Ma_CreateFakeSpeech(void) {
	static const struct {
		const char *var;
		long v;
	} panel[] = { { "NewsPanelShowHead", 1 }, { "NewsPanelDistance", 300 }, { "NewsPanelDisplacement", 35 }, { "NewsPanelMove", -30 },
		{ "NewsPanelX", 20 }, { "NewsPanelY", 20 }, { "NewsPanelW", 270 }, { "NewsPanelH", 180 } };
	nativeVal_t r, v, d, kb, kn;
	Native_SetNone(&r);
	byte ok = LvN_CreateClass("FakeSpeech", &r);
	Native_Free(&r);
	if (ok) {
		LvN_Str(&v, "Levels/Temple/Map/Yakuzzi/ArchBishop.M3D");
		ok = LvN_SetCls("ModelFilePath", &v) && LvN_CreateAction("Relax", &r);
		Native_Free(&v);
		Native_Free(&r);
	}
	if (ok) {
		LvN_Str(&v, "Levels/Temple/Map/Yakuzzi/YakuzziAnm/ArchBishop_Speech.M3D");
		ok = LvN_SetAct("AnmFilePath", &v);
		Native_Free(&v);
	}
	if (ok) {
		Native_SetInt(&v, 1);
		ok = LvN_SetAct("isLoop", &v);
	}
	if (ok) {
		LvN_Str(&v, "Relax");
		ok = LvN_SetCls("DefAction", &v);
		Native_Free(&v);
	}
	if (!ok) {
		return 0;
	}
	LvN_Dummy("DM_Scene_Yakuzzi", 0, &d);
	Native_SetNone(&kb);
	byte have = d.type != NV_NONE;
	if (have) {
		nativeVal_t name, c[3], zero;
		int got = 0;
		LvN_Str(&name, "FakeSpeech");
		ok = LvN_Item(&d, 1, &v);
		for (; ok && got < 3; got++) {
			ok = LvN_Item(&v, got, &c[got]);
		}
		if (!ok && got) {
			Native_Free(&c[--got]);
		}
		ok = ok && LvN_Create(&name, &c[0], &c[1], &c[2], "WithLife", &kb);
		while (got-- > 0) {
			Native_Free(&c[got]);
		}
		Native_Free(&v);
		Native_Free(&name);
		if (ok) {
			Native_SetEmpty(&zero, NV_TUPLE);
			for (int i = 0; i < 3; i++) {
				Native_SetInt(Native_Append(&zero), 0);
			}
			ok = Mode_VSet(&kb, "Ang", &zero) && LvN_SetS(&kb, "ActCtrl", "FakeSpeech");
			Native_Free(&zero);
		}
	}
	Native_Free(&d);
	ok = ok && Mode_GVarSetS("NewsLogoBmp", "2D/News/LogoTemple.bmp") && Mode_GVarSetS("BackNewsBmp", "2D/News/BackTemple.bmp");
	if (ok && !have) {
		ok = Native_Raise(NE_NAME, "kb");
	}
	Native_SetNone(&kn);
	ok = ok && Mode_VGet(&kb, "Name", &kn) && Mode_GVarSet("NewsHead", &kn);
	for (size_t i = 0; ok && i < sizeof(panel) / sizeof(panel[0]); i++) {
		ok = Mode_GVarSetI(panel[i].var, panel[i].v);
	}
	Native_Free(&kb);
	Native_Free(&kn);
	return ok && Ma_Preload("BackNewsBmp") && Ma_Preload("NewsLogoBmp") && LvN_SetMusic("Music/ArchBishop.ogg", 1.0);
}

/* Init(LevelPath): the gatekeeper against Dtritus, the door through UseDoor, the scene and the news panel */
static byte Ma_Init(void) {
	return PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "s", "DM_Door_Teleporter_01") &&
		Tp_Gate("GateKeeper_Portalis", NULL, "DtritusGroupGateKeeper", "Mission_MeetArchbishop.UseDoor") &&
		Tp_Do(MA, "LoadYakuzziScene") && PORT_LOWN(Mission_MeetArchbishop, CreateFakeSpeech, NULL, "");
}

static byte Ma_UseDoor_C(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Ma_UseDoor(a);
}

LV_CORE0(Ma_TestEndOfElevator)
LV_CORE0(Ma_CreateFakeSpeech)
LV_CORE0(Ma_Init)

LV_FN(Ma, UseDoorPy, 4, Ma_UseDoor_C)
LV_FN(Ma, TestEndOfElevatorPy, 0, Ma_TestEndOfElevator_C)
LV_FN(Ma, CreateFakeSpeechPy, 0, Ma_CreateFakeSpeech_C)
LV_FN(Ma, InitPy, 1, Ma_Init_C)

PyMethodDef g_TempleMeetArchbishopMethods[] = {
	{ "UseDoor", Ma_UseDoorPy, 1, NULL },
	{ "TestEndOfElevator", Ma_TestEndOfElevatorPy, 1, NULL },
	{ "CreateFakeSpeech", Ma_CreateFakeSpeechPy, 1, NULL },
	{ "Init", Ma_InitPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_Temple)
