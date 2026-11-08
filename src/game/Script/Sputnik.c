/*
 * The gameplay functions of the Sputnik character in C (scripts/chars/sputnik.py, issue #2; Init and OnCreate are
 * data/chars/sputnik.def, Characters.c): Sputnik beside the player's ship (Assign, SetSptAt), the ship he sucks in
 * when the player takes another one (UseShip, BackCamera), the hangar screen he opens (ChangeShip, CreateShip,
 * ShowInterface, EnterInterface, EndOfSelection, BackControl) and the events of his animations.
 *
 * The module variables stay in the script, Parking, Scene and the missions read them: spt (Sputnik's entity), ply,
 * LastPos, LastAng, LastHangar, UseParams (the arguments of Parking.UseShip after the suck animation) and Angulus.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values and read the variables where the script
 * reads them as its globals; a change is written to the module at once (OBJLIB 5). Sputnik, the player and the
 * ships of Parking (PlayerShip, DtritusPlate, read as its attributes at every use) are handle values (OBJLIB 7).
 * Scrap.GetEntity, CreateEntity, SLogic.SetOnFloor, SInput.SetActionSet, SScorer.Fade, SetCinema and Scrap.Set go
 * through their seams (OBJLIB 7: the cases record or swap them); the vector math, the language texts, the save
 * variable and the camera name are C.
 *
 * Direct calls (stage 3): Parking, Menu, CharScorer, SputnikInterface and the module's own Assign (through its
 * global, as the script called it) through their refs (PORT_CALL, PORT_OWN: the port seam, OBJLIB 5). apply of
 * Parking.UseShip on the kept UseParams stays the builtin by name (OBJLIB 4), EndOfSelection is put into
 * SputnikInterface as the object it is (OBJLIB 5).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define SP "Sputnik"

/* ---- helpers of the module ---- */

/* spt.var = v, = i, = "text", = d of the global spt (v made before, as the script makes the value first) */
static byte Spt_Set(const char *var, const nativeVal_t *v) {
	nativeVal_t spt = { 0 };
	byte ok = Mode_G(SP, "spt", &spt) && Mode_VSet(&spt, var, v);
	Native_Free(&spt);
	return ok;
}

static byte Spt_SetI(const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Spt_Set(var, &v);
}

static byte Spt_SetS(const char *var, const char *s) {
	nativeVal_t v;
	Native_SetText(&v, s, (int)strlen(s));
	byte ok = Spt_Set(var, &v);
	Native_Free(&v);
	return ok;
}

static byte Spt_SetF(const char *var, double d) {
	nativeVal_t v;
	Native_SetFloat(&v, d);
	return Spt_Set(var, &v);
}

/* Parking.<obj>.var = v, = i, = "text" (obj PlayerShip or DtritusPlate, read as the attribute of Parking) */
static byte Spt_PkSet(const char *obj, const char *var, const nativeVal_t *v) {
	nativeVal_t o = { 0 };
	byte ok = Mode_ModGet("Parking", obj, &o) && Mode_VSet(&o, var, v);
	Native_Free(&o);
	return ok;
}

static byte Spt_PkSetI(const char *obj, const char *var, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Spt_PkSet(obj, var, &v);
}

static byte Spt_PkSetS(const char *obj, const char *var, const char *s) {
	nativeVal_t v;
	Native_SetText(&v, s, (int)strlen(s));
	byte ok = Spt_PkSet(obj, var, &v);
	Native_Free(&v);
	return ok;
}

/* Parking.PlayerShip.var into slot (freed first) */
static byte Spt_PsGet(const char *var, nativeVal_t *slot) {
	nativeVal_t ps = { 0 };
	Native_Free(slot);
	byte ok = Mode_ModGet("Parking", "PlayerShip", &ps) && Mode_VObj(&ps, var, slot);
	Native_Free(&ps);
	return ok;
}

/* Parking.SetShipRadius(Parking.PlayerShip, 0) */
static byte Spt_ShipRadius(void) {
	nativeVal_t ps = { 0 };
	byte ok = Mode_ModGet("Parking", "PlayerShip", &ps) && PORT_CALL(Parking, SetShipRadius, NULL, "vi", &ps, 0L);
	Native_Free(&ps);
	return ok;
}

/* Parking.SetShipAsParked(Parking.PlayerShip.Name) */
static byte Spt_Parked(void) {
	nativeVal_t name = { 0 };
	byte ok = Spt_PsGet("Name", &name) && PORT_CALL(Parking, SetShipAsParked, NULL, "v", &name);
	Native_Free(&name);
	return ok;
}

/* Assign(Parking.PlayerShip, ply) through the module's global Assign */
static byte Spt_AssignPly(void) {
	nativeVal_t ps = { 0 }, ply = { 0 };
	byte ok = Mode_ModGet("Parking", "PlayerShip", &ps) && Mode_G(SP, "ply", &ply) &&
		PORT_OWN(Sputnik, Assign, NULL, "vv", &ps, &ply);
	Native_Free(&ps);
	Native_Free(&ply);
	return ok;
}

/* Scrap.GetEntity(Scrap.GetCam(0)).EntityLink = spt.Name */
static byte Spt_CamToSpt(void) {
	nativeVal_t spt = { 0 }, name = { 0 };
	byte ok = Mode_G(SP, "spt", &spt) && Mode_VObj(&spt, "Name", &name) && Mode_CamLink(&name);
	Native_Free(&spt);
	Native_Free(&name);
	return ok;
}

/* Parking.CreateBox("Parkingson", vPos[0], vPos[1], vPos[2]) (the box into out, NULL: dropped) */
static byte Spt_Box(const nativeVal_t *pos, nativeVal_t *out) {
	nativeVal_t c[3] = { { 0 }, { 0 }, { 0 } };
	byte ok = Mode_Item(pos, 0, &c[0]) && Mode_Item(pos, 1, &c[1]) && Mode_Item(pos, 2, &c[2]) &&
		PORT_CALL(Parking, CreateBox, out, "svvv", "Parkingson", &c[0], &c[1], &c[2]);
	for (int i = 0; i < 3; i++) {
		Native_Free(&c[i]);
	}
	return ok;
}

/* SVec.Rotate3D(v, (0, Ang[1], 0)) into out, the rotation of the script around the y axis */
static byte Spt_RotY(const nativeVal_t *v, const nativeVal_t *ang, nativeVal_t *out) {
	nativeVal_t a1 = { 0 }, zero, rot = { 0 };
	Native_SetInt(&zero, 0);
	byte ok = Mode_Item(ang, 1, &a1);
	if (ok) {
		Mode_Tuple3(&rot, &zero, &a1, &zero);
		ok = Mode_Rotate(v, &rot, out);
	}
	Native_Free(&a1);
	Native_Free(&rot);
	return ok;
}

/* ---- the functions ---- */

/* Assign(Entity, Player=None): Sputnik (created the first time) beside Entity, still, with the option to change the
 * ship; the player in front of him */
static byte Spt_Assign(const nativeVal_t *ent, const nativeVal_t *player) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *spt = Mode_V(&t), *e = Mode_V(&t), *pos = Mode_V(&t), *ang = Mode_V(&t), *r1 = Mode_V(&t);
	nativeVal_t *r2 = Mode_V(&t), *s = Mode_V(&t), *d = Mode_V(&t), *vec = Mode_V(&t), *rv = Mode_V(&t), *np = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t), *sp = Mode_V(&t), *sa = Mode_V(&t), *pr = Mode_V(&t), *neg = Mode_V(&t);
	nativeVal_t *r3 = Mode_V(&t), *x = Mode_V(&t), *d2 = Mode_V(&t), *vec2 = Mode_V(&t), *rv2 = Mode_V(&t), *np2 = Mode_V(&t);
	nativeVal_t *ea = Mode_V(&t), *sub = Mode_V(&t), *off = Mode_V(&t), *pname = Mode_V(&t);
	const nativeVal_t *zero = Mode_I(&t, 0), *ten = Mode_I(&t, 10);
	if (!Mode_Import("Chars") || !Mode_G(SP, "spt", spt)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(spt)) {
		if (!Mode_VSetI(spt, "Active", 1)) {
			return Mode_Done(&t, 0);
		}
	} else if (!LvN_Create(Mode_S(&t, "Sputnik"), zero, zero, zero, "WalkChar", e) || !Mode_Put(SP, "spt", NULL, e) ||
		!Mode_Read(SP, "spt", spt) || !Mode_VSetS(spt, "ActCtrl", "Sputnik")) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_VObj(ent, "Pos", pos) || !Mode_VObj(ent, "Ang", ang) || !Mode_Read(SP, "spt", spt) ||
		!Mode_VSet(spt, "Ang", ang)) {
		return Mode_Done(&t, 0);
	}
	int car = Mode_VIsType(ent, "Car");
	if (car < 0) {
		return Mode_Done(&t, 0);
	}
	if (car) {
		/* SVec.Add(Pos, SVec.Rotate3D((Entity.VRadius + spt.VRadius + 10, 0, 0), (0, Ang[1], 0))) */
		if (!Mode_VObj(ent, "VRadius", r1) || !Mode_VObj(spt, "VRadius", r2) || !Mode_Arith('+', r1, r2, s) ||
			!Mode_Arith('+', s, ten, d)) {
			return Mode_Done(&t, 0);
		}
		Mode_Tuple3(vec, d, zero, zero);
		if (!Spt_RotY(vec, ang, rv) || !Mode_VecAdd(pos, rv, np) || !Mode_VSet(spt, "Pos", np)) {
			return Mode_Done(&t, 0);
		}
	} else if (!Mode_VSet(spt, "Pos", pos)) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_VObj(spt, "Name", name) || !LvN_SetOnFloor(name) || !Mode_VSetI(spt, "ActiveCMask", 1) ||
		!Mode_VObj(spt, "Pos", sp) || !Mode_VSet(spt, "MovePos", sp) || !Mode_VObj(spt, "Ang", sa) ||
		!Mode_VSet(spt, "MoveAng", sa) || !Mode_VSetF(spt, "MoveTime", 0.001) || !Mode_VSetI(spt, "ArrowCanUse", 1) ||
		!Mode_VSetI(spt, "ArrnOpt", 0) || !LvN_SetLangOpt(spt, "ArrOpt0", "ChangeShip:", "ChangeShip") ||
		!Mode_VSetS(spt, "UseFunc", "Sputnik.ChangeShip")) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(player)) {
		return Mode_Done(&t, 1);
	}
	/* Player.Pos = SVec.Add(spt.Pos, SVec.Rotate3D((0, 0, -Player.VRadius - spt.VRadius - 10), (0, Ang[1], 0))) */
	if (!Mode_Read(SP, "spt", spt) || !Mode_VObj(spt, "Pos", sp) || !Mode_VObj(player, "VRadius", pr) || !Mode_Neg(pr, neg) ||
		!Mode_VObj(spt, "VRadius", r3) || !Mode_Arith('-', neg, r3, x) || !Mode_Arith('-', x, ten, d2)) {
		return Mode_Done(&t, 0);
	}
	Mode_Tuple3(vec2, zero, zero, d2);
	if (!Spt_RotY(vec2, ang, rv2) || !Mode_VecAdd(sp, rv2, np2) || !Mode_VSet(player, "Pos", np2)) {
		return Mode_Done(&t, 0);
	}
	/* Player.Ang = SVec.Sub(Entity.Ang, (0, 210, 0)) */
	Mode_Ints3(off, 0, 210, 0);
	return Mode_Done(&t, Mode_VObj(ent, "Ang", ea) && Mode_VecSub(ea, off, sub) && Mode_VSet(player, "Ang", sub) &&
		Mode_VObj(player, "Name", pname) && LvN_SetOnFloor(pname));
}

/* ChangeRelax(EntityName, EventName, Time): one of the relax animations at random */
static byte Spt_ChangeRelax(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ch = Mode_V(&t), *r = Mode_V(&t), *n = Mode_V(&t), *rep = Mode_V(&t), *act = Mode_V(&t);
	if (!Mode_EntitySeam(name, ch)) {
		return Mode_Done(&t, 0);
	}
	/* Scrap.Rand(0, 6) as the C port drew it (the C directly) */
	Mode_RandF(0, 6, r);
	if (!Native_ToInt(r, n)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(n, rep);
	return Mode_Done(&t, Mode_Cat(&t, act, 2, Mode_S(&t, "Relax"), rep) && Mode_VSet(ch, "ForceAction", act));
}

/* BackCamera(EntityName, EventName, Time): the old ship is sucked in: the walk HUD, then Parking.UseShip with the
 * arguments UseShip kept */
static byte Spt_BackCamera(void) {
	nativeVal_t fn = { 0 }, params = { 0 };
	if (!Mode_Import("Menu") || !Mode_Import("CharScorer") || !Mode_Import("Parking") || !Mode_CamLink(LvN_Empty()) ||
		!Mode_ActionSet("Walk") || !PORT_CALL(CharScorer, Create, NULL, "i", 0L) ||
		!Spt_PkSetI("PlayerShip", "ActiveCMask", 1) || !Spt_PkSetI("PlayerShip", "Active", 0) ||
		!Spt_PkSetI("DtritusPlate", "Active", 0) || !Spt_ShipRadius() || !Mode_Import("Parking")) {
		return 0;
	}
	/* apply(Parking.UseShip, UseParams): the function as a value, the builtin by name (OBJLIB 4) */
	byte ok = Mode_ModGet("Parking", "UseShip", &fn) && Mode_G(SP, "UseParams", &params) &&
		Native_CallMod("__builtin__", "apply", NULL, "vv", &fn, &params);
	Native_Free(&fn);
	Native_Free(&params);
	return ok;
}

/* UseShip(UsrName, CharName, optId, FullOption, BoxName="Parking"): the player takes another parked ship: Sputnik
 * sucks in the player's own one first (when it is out), then Parking.UseShip */
static byte Spt_UseShip(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *n = Mode_V(&t), *act = Mode_V(&t), *pos = Mode_V(&t), *life = Mode_V(&t), *params = Mode_V(&t);
	nativeVal_t *box = Mode_V(&t), *e = Mode_V(&t);
	if (!Mode_Import("Menu") || !Mode_Import("Parking") || !Spt_PsGet("Name", n)) {
		return Mode_Done(&t, 0);
	}
	int other = Mode_Cmp(n, "!=", &a[1]);
	if (other < 0) {
		return Mode_Done(&t, 0);
	}
	if (other && !Spt_PsGet("Active", act)) {
		return Mode_Done(&t, 0);
	}
	if (!other || !Native_True(act)) {
		if (other && (!Spt_PsGet("Pos", pos) || !Spt_Box(pos, NULL))) {
			return Mode_Done(&t, 0);
		}
		return Mode_Done(&t, Mode_Import("Parking") &&
			PORT_CALL(Parking, UseShip, NULL, "vvvvv", &a[0], &a[1], &a[2], &a[3], &a[4]));
	}
	if (!Mode_ActionSet("Inactive") || !PORT_CALL(Menu, MovieScorer, NULL, "i", 0L) ||
		!Spt_PkSetI("PlayerShip", "ActiveCMask", 0) || !Spt_PkSetI("DtritusPlate", "Active", 0) ||
		!Spt_PsGet("Pos", pos) || !Spt_PsGet("MaxLife", life) || !Spt_PkSet("PlayerShip", "Life", life) ||
		!Spt_CamToSpt() || !Spt_Set("Pos", pos) || !Spt_Set("MovePos", pos) || !Spt_SetF("MoveTime", 0.01) ||
		!Spt_SetS("Action", "Suck")) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(params, NV_TUPLE);
	for (int i = 0; i < 5; i++) {
		Native_Copy(Native_Append(params), &a[i]);
	}
	return Mode_Done(&t, Mode_Put(SP, "UseParams", NULL, params) && Spt_Box(pos, box) && Mode_VSetI(box, "rTime", 5) &&
		Mode_EntitySeam(&a[1], e) && PORT_CALL(Parking, SetInvunerable, NULL, "v", e));
}

/* ChangeShip(UsrName, CharName, optId, FullOption): the player asks Sputnik for another ship: his interface
 * animation, the player hidden */
static byte Spt_ChangeShip(const nativeVal_t *usr) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ply = Mode_V(&t), *last = Mode_V(&t);
	return Mode_Done(&t, Mode_Import("Menu") && Mode_Import("Parking") && Mode_Import("SputnikInterface") &&
		Mode_EntitySeam(usr, ply) && Mode_Put(SP, "ply", NULL, ply) && Spt_AssignPly() && Mode_Read(SP, "ply", ply) &&
		Mode_VSetI(ply, "Active", 0) && Spt_SetS("Action", "Interface") && Spt_CamToSpt() && Spt_PsGet("Pos", last) &&
		Mode_Put(SP, "LastPos", NULL, last) && Spt_PsGet("Ang", last) && Mode_Put(SP, "LastAng", NULL, last) &&
		Spt_PkSetS("PlayerShip", "OnDeath", "") && Mode_ActionSet("Inactive") &&
		PORT_CALL(Menu, MovieScorer, NULL, "i", 0L) && Mode_Read(SP, "LastPos", last) && Spt_Set("MovePos", last) &&
		Spt_Set("Pos", last));
}

/* CreateShip(EntityName, EventName, Time): before the hangar screen: the player's ship back when it was away
 * (LastHangar None), else LastHangar the hangar it came from */
static byte Spt_CreateShip(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *act = Mode_V(&t), *pos = Mode_V(&t), *hangar = Mode_V(&t);
	if (!Mode_Import("Menu") || !Mode_Import("Parking") || !Mode_Import("SputnikInterface") || !Spt_PsGet("Active", act)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(act)) {
		Mode_SaveVar("Hangar.iHangarShip", hangar);
		return Mode_Done(&t, Mode_Put(SP, "LastHangar", NULL, hangar));
	}
	return Mode_Done(&t, Spt_PkSetI("PlayerShip", "Active", 1) && Spt_PkSetI("DtritusPlate", "Active", 1) && Spt_Parked() &&
		Spt_PsGet("Pos", pos) && Spt_Set("Pos", pos) && Mode_Put(SP, "LastHangar", NULL, hangar));
}

/* ShowInterface(EntityName, EventName, Time): the fade to black */
static byte Spt_ShowInterface(void) {
	return Mode_Fade(0, lv_clear, lv_black, 0.25) && Mode_SetCinema(0, 0, 0.25);
}

/* EnterInterface(EntityName, EventName, Time): the hangar screen (SputnikInterface) with EndOfSelection as its end */
static byte Spt_EnterInterface(void) {
	return Mode_Import("Menu") && Mode_Import("Parking") && Mode_Import("SputnikInterface") && Mode_ActionSet("Walk") &&
		Mode_ModSetGlobal("SputnikInterface", "EndSputnikCallBack", SP, "EndOfSelection") &&
		PORT_CALL(SputnikInterface, EnterSputnikInterface, NULL, "i", 0L) && Spt_SetI("Active", 0) &&
		Mode_CamLink(LvN_Empty()) && Spt_PkSetS("PlayerShip", "OnDeath", "Vehicles.OnDeath") &&
		Mode_Fade(0, lv_black, lv_clear, 0.25) && Mode_GVarSetI("NoiseFactorBegin", 0) && Mode_GVarSetI("NoiseFactor", 24) &&
		Mode_GVarSetI("NoiseTime", 0) && Mode_GVarSetN("NoiseEndTime", 0.25);
}

/* EndOfSelection(): back from the hangar screen: the ship of the chosen hangar where the old one stood; a new one is
 * made in front of the player (Sputnik's Make animation, the ship transparent) */
static byte Spt_EndOfSelection(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ply = Mode_V(&t), *last = Mode_V(&t), *hangar = Mode_V(&t), *save = Mode_V(&t);
	if (!Mode_Import("Menu") || !Mode_Import("Parking") || !Mode_Import("SputnikInterface") || !Mode_G(SP, "ply", ply) ||
		!Mode_VSetI(ply, "Active", 1) || !Spt_SetI("Active", 1) || !Mode_G(SP, "LastPos", last) ||
		!PORT_CALL(Parking, CreatePlayerShip, NULL, "v", last) || !Spt_ShipRadius() || !Mode_Read(SP, "LastAng", last) ||
		!Spt_PkSet("PlayerShip", "Ang", last) || !Spt_PkSetI("PlayerShip", "Active", 1) ||
		!Spt_PkSetI("DtritusPlate", "Active", 1) || !Spt_Parked() || !Spt_AssignPly() || !Mode_GVarSetI("NoiseFactor", 0) ||
		!Mode_G(SP, "LastHangar", hangar)) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Hangar.iHangarShip", save);
	int other = Mode_Cmp(hangar, "!=", save);
	if (other <= 0) {
		return Mode_Done(&t, other == 0);
	}
	return Mode_Done(&t, Spt_SetS("Action", "Make") && Mode_ActionSet("Inactive") && Spt_PkSetI("DtritusPlate", "Active", 0) &&
		PORT_CALL(Menu, MovieScorer, NULL, "i", 0L) && Spt_PkSetI("PlayerShip", "RenderEffect", 2) &&
		Spt_PkSetI("PlayerShip", "RenderEffectParam0", 1));
}

/* BackControl(EntityName, EventName, Time): the end of the Make animation: the player walks again */
static byte Spt_BackControl(void) {
	nativeVal_t act = { 0 };
	byte ok = Mode_Import("CharScorer") && Mode_Import("Parking") && Mode_ActionSet("Walk") &&
		PORT_CALL(CharScorer, Create, NULL, "i", 0L) && Spt_PsGet("Active", &act) && Spt_PkSet("DtritusPlate", "Active", &act);
	Native_Free(&act);
	return ok;
}

/* spt.var = (0, Ang[1], 0) */
static byte Spt_SetAngY(const char *var, const nativeVal_t *ang) {
	nativeVal_t a1 = { 0 }, zero, v = { 0 };
	Native_SetInt(&zero, 0);
	byte ok = Mode_Item(ang, 1, &a1);
	if (ok) {
		Mode_Tuple3(&v, &zero, &a1, &zero);
		ok = Spt_Set(var, &v);
	}
	Native_Free(&a1);
	Native_Free(&v);
	return ok;
}

/* SetSptAt(Pos, Ang, Anm): Sputnik still at Pos playing Anm, without the use arrow */
static byte Spt_SetSptAt(const nativeVal_t *pos, const nativeVal_t *ang, const nativeVal_t *anm) {
	return Spt_SetI("Active", 1) && Spt_Set("Pos", pos) && Spt_SetAngY("Ang", ang) && Spt_Set("MovePos", pos) &&
		Spt_SetAngY("MoveAng", ang) && Spt_SetF("MoveTime", 0.001) && Spt_Set("Action", anm) && Spt_SetI("ActiveCMask", 0) &&
		Spt_SetI("ArrowCanUse", 0);
}

/* the effect events: on the player's ship (InitSuck, InitMake) or on Sputnik */
static byte Spt_FX(byte ship, const char *fx) {
	nativeVal_t name, e = { 0 };
	if (!Mode_Import("Parking")) {
		return 0;
	}
	byte ok;
	if (ship) {
		ok = Mode_ModGet("Parking", "PlayerShip", &e);
	} else {
		Native_SetStr(&name, "Sputnik");
		ok = Mode_EntitySeam(&name, &e);
	}
	ok = ok && Mode_VSetFX(&e, fx);
	Native_Free(&e);
	return ok;
}

MODE_KW(Sputnik, Assign, "O|O", "Entity", "Player") {
	return Spt_Assign(&v[0], &v[1]);
}

MODE_KW(Sputnik, UseShip, "OOOO|O", "UsrName", "CharName", "optId", "FullOption", "BoxName") {
	Mode_KwDefS(v, given, 4, "Parking");
	return Spt_UseShip(v);
}

MODE_FN(Sputnik, ChangeRelax, 3, Spt_ChangeRelax(&a[0]))
MODE_FN(Sputnik, ByeBye, 3, Spt_SetI("Active", 0))
MODE_FN(Sputnik, BackCamera, 3, Spt_BackCamera())
MODE_FN(Sputnik, ChangeShip, 4, Spt_ChangeShip(&a[0]))
MODE_FN(Sputnik, CreateShip, 3, Spt_CreateShip())
MODE_FN(Sputnik, ShowInterface, 3, Spt_ShowInterface())
MODE_FN(Sputnik, EnterInterface, 3, Spt_EnterInterface())
MODE_FN(Sputnik, EndOfSelection, 0, Spt_EndOfSelection())
MODE_FN(Sputnik, BackControl, 3, Spt_BackControl())
MODE_FN(Sputnik, SetSptAt, 3, Spt_SetSptAt(&a[0], &a[1], &a[2]))
MODE_FN(Sputnik, InitSuck, 3, Spt_FX(1, "VehicleSputnikSuck"))
MODE_FN(Sputnik, InitMake, 3, Spt_FX(1, "VehicleSputnikMake"))
MODE_FN(Sputnik, InitHello, 3, Spt_FX(0, "CharacterSputnikHello"))
MODE_FN(Sputnik, InitBye, 3, Spt_FX(0, "CharacterSputnikBye"))
MODE_FN(Sputnik, InitInterface, 3, Spt_FX(0, "CharacterSputnikInterface"))

PyMethodDef g_SputnikFuncs[] = {
	{ "Assign", (PyCFunction)(void (*)(void))Sputnik_Assign, 3, NULL },
	{ "ChangeRelax", Sputnik_ChangeRelax, 1, NULL },
	{ "ByeBye", Sputnik_ByeBye, 1, NULL },
	{ "BackCamera", Sputnik_BackCamera, 1, NULL },
	{ "UseShip", (PyCFunction)(void (*)(void))Sputnik_UseShip, 3, NULL },
	{ "ChangeShip", Sputnik_ChangeShip, 1, NULL },
	{ "CreateShip", Sputnik_CreateShip, 1, NULL },
	{ "ShowInterface", Sputnik_ShowInterface, 1, NULL },
	{ "EnterInterface", Sputnik_EnterInterface, 1, NULL },
	{ "EndOfSelection", Sputnik_EndOfSelection, 1, NULL },
	{ "BackControl", Sputnik_BackControl, 1, NULL },
	{ "SetSptAt", Sputnik_SetSptAt, 1, NULL },
	{ "InitSuck", Sputnik_InitSuck, 1, NULL },
	{ "InitMake", Sputnik_InitMake, 1, NULL },
	{ "InitHello", Sputnik_InitHello, 1, NULL },
	{ "InitBye", Sputnik_InitBye, 1, NULL },
	{ "InitInterface", Sputnik_InitInterface, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(Sputnik)
