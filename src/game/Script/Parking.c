/*
 * The Parking and Accelerator modules in C (scripts/vehicles/parking.py and accelerator.py, issue #2). Parking: the
 * player's own ship and the Dtritus plate on it, boarding a ship (UseShip, the camera path, CharOnShip) and leaving it
 * on a parking box (OnPark, EndPathCam), the parked ships of the level and the engine's callbacks when a parking place
 * is taken or freed, the junkyard parking, the links to the next level and the start of an outdoor level (Init).
 * Accelerator: the speed rings of the DM_Accelerator dummies.
 *
 * The module variables stay in the script, the missions and other scripts read and set them: PlayerShip,
 * DtritusPlate, LocalShip (SetInvunerable sets it, RestoreDeathPlayerShip deletes it), LastCamera, LastPark,
 * SetWeaponOnShip, the callbacks SwitchCharMissionCallback, SwitchVehicleMissionCallback, ParkedOutCallback, their
 * Aux twins and OnCreatePlayerShip, ShipTypes, AllShips (CreateParkedShip changes it like the script), ParkingZones,
 * LastParkingZone, the constants (ParkingDist, MinParkingVel, ParkingDepth, P_None, PL_Junk, PR_Junk, PR_Lst,
 * PR_ComeFrom, PR_Spawn, DtritusPlateModel, DtritusPlateNode, AlarmProbOnParkedShipDeath) and
 * Accelerator.ACCELERATOR_RADIUS. The module code (the save variables, the engine callbacks) stays in the scripts.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values; the PyMethodDef entries are adapters with
 * the argument formats of the C port. The variables of the module are read where the script reads them as its globals
 * and written at once (OBJLIB 5: the missions, Doors, the levels and the cases read and set them); AllShips is changed
 * in place. The ships, cameras, boxes and the plate are handle values (OBJLIB 7). The module's own functions the script
 * calls as its globals (Parking's and Accelerator's) and the other ports (ShipMaker, Sputnik, Menu, RacerScorer,
 * CharScorer, Chars, ShipEdit, Vehicles, SaveGame, Speech, RaceMaker) go through their refs (PORT_OWN, PORT_CALL: the
 * seam, OBJLIB 5; a mission or a case may replace one), and so do Scrap.SetCam, SwitchMissionArrows, AddParkingZone,
 * CreateEntityList and EntityListGet (stage 7); string.split of other values than texts stays by name (OBJLIB 7).
 * Scrap.GetEntity, CreateEntity, UsrEntity, GetNetFlags, Set, TestLine, StartDummySearch, NextDummySearch, GetAlarm,
 * SetAlarm, SetMoney, IncSaveVar, ConsoleError, EntityListSet, AddScheduledFunc, SInput.SetActionSet,
 * SScorer.SetLabelText, SSound.OpenVoice, PlayVoice, VoiceRemain, SLogic.SetOnFloor, SAct.GetAction and SetAct go
 * through their seams (OBJLIB 7: the cases record or swap them). The evals of the save texts read literals with the
 * native reader (Mode_EvalIn); a bare name is looked up in the module (OBJLIB 6).
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define PK "Parking"

/* the slot freed, then x[i] into it */
static byte Pk_Item(const nativeVal_t *x, long i, nativeVal_t *slot) {
	Native_Free(slot);
	return Mode_Item(x, i, slot);
}

/* the slot freed, then the global name of the module into it */
static byte Pk_G(const char *name, nativeVal_t *slot) {
	return Mode_Read(PK, name, slot);
}

/* Scrap.GetEntity(name) through its seam, the handle value (None for no entity) */
static byte Pk_Ent(const nativeVal_t *name, nativeVal_t *slot) {
	Native_Free(slot);
	return Mode_EntitySeam(name, slot);
}

/* (a, b, c) of copies of the three values */
static void Pk_Tuple3(nativeVal_t *out, const nativeVal_t *a, const nativeVal_t *b, const nativeVal_t *c) {
	Native_Free(out);
	Mode_Tuple3(out, a, b, c);
}

/* (0, ang[1] op add, 0), the plain ang[1] for add NULL */
static byte Pk_Yaw(const nativeVal_t *ang, char op, const nativeVal_t *add, nativeVal_t *out) {
	nativeVal_t y = { 0 }, s = { 0 }, z;
	Native_SetInt(&z, 0);
	byte ok = Mode_Item(ang, 1, &y) && (!add || Native_Arith(op, &y, add, &s));
	if (ok) {
		Pk_Tuple3(out, &z, add ? &s : &y, &z);
	}
	Native_Free(&y);
	Native_Free(&s);
	return ok;
}

/* the x, y and z of a point (p[0], p[1], p[2]) */
static byte Pk_XYZ(const nativeVal_t *p, nativeVal_t *xyz) {
	return Pk_Item(p, 0, &xyz[0]) && Pk_Item(p, 1, &xyz[1]) && Pk_Item(p, 2, &xyz[2]);
}

/* Scrap.Set(var, 0.0) of the five motion blur variables */
static byte Pk_NoBlur(void) {
	static const char *const vars[] = { "MotionBlurFactorBegin", "MotionBlurFactorEnd", "MotionBlurTime",
		"MotionBlurEndTime", "MotionBlurOffVar" };
	for (int i = 0; i < 5; i++) {
		if (!Mode_GVarSetN(vars[i], 0.0)) {
			return 0;
		}
	}
	return 1;
}

/* Scrap.TestLine(p, (p[0], p[1] - 10000, p[2]), "") */
static byte Pk_Down(const nativeVal_t *p, nativeVal_t *out) {
	nativeVal_t xyz[3] = { { 0 }, { 0 }, { 0 } }, y = { 0 }, k, to = { 0 };
	Native_SetInt(&k, 10000);
	Native_Free(out);
	byte ok = Pk_XYZ(p, xyz) && Native_Arith('-', &xyz[1], &k, &y);
	if (ok) {
		Pk_Tuple3(&to, &xyz[0], &y, &xyz[2]);
		ok = Mode_TestLineSeam(p, &to, LvN_Empty(), out);
	}
	for (int i = 0; i < 3; i++) {
		Native_Free(&xyz[i]);
	}
	Native_Free(&y);
	Native_Free(&to);
	return ok;
}

/* Scrap.TestLine(...)[0] of Pk_Down */
static byte Pk_Ground(const nativeVal_t *p, nativeVal_t *out) {
	nativeVal_t res = { 0 };
	byte ok = Pk_Down(p, &res) && Pk_Item(&res, 0, out);
	Native_Free(&res);
	return ok;
}

/* Scrap.SetCam(0, name) */
static byte Pk_SetCam(const nativeVal_t *name) {
	return PORT_CALL(Scrap, SetCam, NULL, "iv", 0L, name);
}

/* Scrap.SwitchMissionArrows(Init.Arrows[a], Init.Arrows[b]) */
static byte Pk_Arrows(long a, long b) {
	nativeVal_t arrows = { 0 }, x = { 0 }, y = { 0 };
	byte ok = Mode_ModGet("Init", "Arrows", &arrows) && Mode_Item(&arrows, a, &x) && Mode_Item(&arrows, b, &y) &&
		PORT_CALL(Scrap, SwitchMissionArrows, NULL, "vv", &x, &y);
	Native_Free(&arrows);
	Native_Free(&x);
	Native_Free(&y);
	return ok;
}

/* Scrap.AddParkingZone(flag, pos, name) */
static byte Pk_Zone(long flag, const nativeVal_t *pos, const nativeVal_t *name) {
	return PORT_CALL(Scrap, AddParkingZone, NULL, "ivv", flag, pos, name);
}

/* CreateBox(name, pos[0], pos[1], pos[2]) through the global */
static byte Pk_BoxAt(const nativeVal_t *name, const nativeVal_t *pos, nativeVal_t *out) {
	nativeVal_t xyz[3] = { { 0 }, { 0 }, { 0 } };
	Native_Free(out);
	byte ok = Pk_XYZ(pos, xyz) && PORT_OWN(Parking, CreateBox, out, "vvvv", name, &xyz[0], &xyz[1], &xyz[2]);
	for (int i = 0; i < 3; i++) {
		Native_Free(&xyz[i]);
	}
	return ok;
}

/* the use arrow of a parked ship: ArrowCanUse 1, ArrnOpt 0, ArrOpt0 "UseShip:" + Scrap.GetLangStr("UseShip"), UseFunc */
static byte Pk_UseArrow(const nativeVal_t *v) {
	nativeVal_t lang = { 0 }, opt = { 0 };
	Mode_Lang("UseShip", &lang);
	byte ok = LvN_SetI(v, "ArrowCanUse", 1) && LvN_SetI(v, "ArrnOpt", 0) && LvN_Cat("UseShip:", &lang, &opt) &&
		Mode_VSet(v, "ArrOpt0", &opt) && LvN_SetS(v, "UseFunc", "Sputnik.UseShip");
	Native_Free(&lang);
	Native_Free(&opt);
	return ok;
}

/* `if callback: callback()` of a global of the module */
static byte Pk_Callback(const char *name) {
	byte called;
	return Mode_CallIf(PK, name, &called);
}

/* SSound.OpenVoice(Scrap.Get("StreamPath") + OutSound.<sound>, 1) and PlayVoiceChannel(1) 0.3 s later */
static byte Pk_Voice(const char *sound) {
	nativeVal_t path = { 0 }, snd = { 0 }, file = { 0 }, one;
	Native_SetInt(&one, 1);
	byte ok = Mode_Import("OutSound") && Native_GVarGet("StreamPath", &path) && Mode_ModGet("OutSound", sound, &snd) &&
		Native_Arith('+', &path, &snd, &file) && Mode_OpenVoice(&file, &one, NULL) && Mode_Later(0.3, PK, "PlayVoiceChannel",
		&one, 1, NULL);
	Native_Free(&path);
	Native_Free(&snd);
	Native_Free(&file);
	return ok;
}

/* SaveGame.StartGame(CleanRevenge = 1) */
static byte Pk_StartGame(void) {
	return PORT_CALL(SaveGame, StartGame, NULL, ";CleanRevenge=i", 1L);
}

/* RestoreDeathPlayerShip(): the ships of the player can die again; LocalShip is deleted */
static byte Pk_RestoreDeathPlayerShipN(void) {
	static const char *const ships[] = { "LocalShip", "PlayerShip" };
	nativeVal_t h = { 0 }, life = { 0 };
	byte ok = 1;
	for (int i = 0; ok && i < 2; i++) {
		int dead;
		ok = Pk_G(ships[i], &h) && LvN_SetS(&h, "OnDeath", "Vehicles.OnDeath") && Mode_VObjRead(&h, "Life", &life) &&
			(dead = Mode_CmpI(&life, "<=", 0)) >= 0 && (!dead || LvN_SetI(&h, "Life", 1)) && (i || Mode_DelGlobal(PK, ships[i]));
	}
	Native_Free(&h);
	Native_Free(&life);
	return ok && Mode_ListSetSeam("Players", "OnDeath", "Chars.MainCharDeath");
}

/* SetInvunerable(v): the players and the ships do not die while the camera moves; LocalShip = v */
static byte Pk_SetInvunerableN(const nativeVal_t *v) {
	nativeVal_t h = { 0 };
	byte ok = Mode_ListSetSeam("Players", "OnDeath", "") && Mode_Put(PK, "LocalShip", NULL, v) && Pk_G("LocalShip", &h) &&
		LvN_SetS(&h, "OnDeath", "") && Pk_G("PlayerShip", &h) && LvN_SetS(&h, "OnDeath", "");
	Native_Free(&h);
	return ok;
}

/* CharOnShip(PathCam): the end of the boarding camera: the ship under control, the ship HUD, its name */
static byte Pk_CharOnShipN(const nativeVal_t *path) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cp = Mode_V(&t), *x = Mode_V(&t), *cam = Mode_V(&t), *v = Mode_V(&t), *solid = Mode_V(&t);
	nativeVal_t *now = Mode_V(&t), *at = Mode_V(&t);
	if (!Mode_Import("RacerScorer") || !Pk_Ent(path, cp) || !Mode_VObjRead(cp, "TgtCam01", x) || !Pk_SetCam(x) || !Mode_VDel(cp)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(x);
	Mode_CamName(0, x);
	if (!Pk_Ent(x, cam) || !Mode_VObjRead(cam, "Target", x) || !Pk_Ent(x, v) || !LvN_SetI(v, "UsrControl", 0) ||
		!LvN_SetI(v, "MoveTime", 0) || !LvN_SetInts3(v, "Vel", 0, 0, 0) || !Mode_ActionSet("Racer") ||
		!PORT_CALL(RacerScorer, Create, NULL, "i", 0L)) {
		return Mode_Done(&t, 0);
	}
	Mode_ScorerGet(0, "Mission", "SolidTime", solid);
	int show = Mode_Time() > 4.0;
	if (show) {
		show = solid->type == NV_NONE || Mode_CmpI(solid, "<=", 0) > 0;
	}
	if (show) {
		Native_SetFloat(now, Mode_Time());
		if (!Mode_VObjRead(v, "Descriptor", x) || !Native_Arith('+', now, Mode_I(&t, 5), at) || !Mode_LabelTextV(x, at)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!LvN_PutNone(PK, "LastCamera") || !PORT_OWN(Parking, RestoreDeathPlayerShip, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	int set = LvN_GTrue(PK, "SetWeaponOnShip");
	if (set > 0) {
		Native_Free(x);
		Mode_SaveVar("usrShip.AcWeap", x);
		set = LvN_PutI(PK, "SetWeaponOnShip", 0) && Native_ToInt(x, at) && Mode_VSet(v, "AcWeap", at) &&
			Mode_SaveS("usrShip.AcWeap", "6") ? 0 : -1;
	}
	return Mode_Done(&t, set >= 0);
}

/* UseShip(UsrName, CharName, optId, FullOption, BoxName="Parking"): the character boards the parked ship CharName:
 * the camera path to the ship, the pilot, the ship rises, a parking box where it stood (none for BoxName None) */
static byte Pk_UseShipN(const nativeVal_t *const *a, nativeVal_t *out) {
	static const char *const imports[] = { "ShipMaker", "Menu", "Sputnik", "Init" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *w = Mode_V(&t), *v = Mode_V(&t), *x = Mode_V(&t), *lastCam = Mode_V(&t), *cp = Mode_V(&t);
	nativeVal_t *lcam = Mode_V(&t), *vPos = Mode_V(&t), *y = Mode_V(&t), *box = Mode_V(&t), *now = Mode_V(&t);
	nativeVal_t parking;
	(void)out;
	/* BoxName="Parking" */
	Native_SetStr(&parking, "Parking");
	const nativeVal_t *boxName = a[4] ? a[4] : &parking;
	for (int i = 0; i < 4; i++) {
		if (!Mode_Import(imports[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Pk_Ent(a[0], w) || !Pk_Ent(a[1], v) || !Mode_VObjRead(w, "UsrControl", x)) {
		return Mode_Done(&t, 0);
	}
	int c = Mode_CmpI(x, "!=", 0);
	if (c) {
		return Mode_Done(&t, c > 0);
	}
	if (!Mode_VObjRead(v, "MPil0", x) || (c = Mode_CmpG(x, "!=", PK, "P_None")) < 0) {
		return Mode_Done(&t, 0);
	}
	if (c) {
		return Mode_Done(&t, 1);
	}
	if (!PORT_OWN(Parking, SetShipRadius, NULL, "vi", v, 0L) || !Pk_NoBlur()) {
		return Mode_Done(&t, 0);
	}
	/* the camera from the character to the ship */
	Mode_CamName(0, lastCam);
	int boxed = Mode_Cmp(boxName, "!=", Mode_V(&t));
	if (!LvN_Create(Mode_S(&t, "CamPath"), Mode_I(&t, 0), Mode_I(&t, 0), Mode_I(&t, 0), "CamPath", cp) ||
		!LvN_SetI(cp, "nWayPoint", 2) || !Mode_VSet(cp, "TgtCam01", lastCam) || !Mode_VObjRead(v, "Pos", x) || boxed < 0 ||
		!PORT_OWN(Parking, SetCameraStart, NULL, "vvvii", cp, v, x, 1L, (long)boxed) || !LvN_SetS(cp, "OnEnd", "Parking.CharOnShip") ||
		!Pk_Ent(lastCam, lcam) || !LvN_SetF(cp, "Time00", 1.0) || !LvN_SetI(cp, "FadeIn00", 1) || !LvN_SetI(cp, "FadeOut00", 0) ||
		!Mode_VObjRead(cp, "Name", x) || !Pk_SetCam(x)) {
		return Mode_Done(&t, 0);
	}
	/* the exchange */
	if (!Mode_VObjRead(v, "MovePos", vPos) || !Mode_VObjRead(w, "ActCtrl", x) || !LvN_Cat("P_", x, y) || !Mode_VSet(v, "MPil0", y) ||
		!Mode_VObjRead(v, "Name", x) || !PORT_CALL(ShipMaker, SetUpChars, NULL, "v", x) || !LvN_SetI(v, "ArrowCanUse", 0) ||
		!LvN_SetS(v, "UseFunc", "")) {
		return Mode_Done(&t, 0);
	}
	int mark = t.n;
	nativeVal_t *pos = Mode_V(&t), *depth = Mode_V(&t), *off = Mode_V(&t);
	if (!Mode_VObjRead(v, "Pos", pos) || !Pk_G("ParkingDepth", depth)) {
		return Mode_Done(&t, 0);
	}
	Pk_Tuple3(off, Mode_I(&t, 0), depth, Mode_I(&t, 0));
	if (!Mode_VecAdd(pos, off, x) || !Mode_VSet(v, "MovePos", x) || !Mode_VObjRead(v, "Ang", x) || !Mode_VSet(v, "MoveAng", x) ||
		!LvN_SetF(v, "MoveTime", 0.9) || !Mode_VObjRead(w, "TgType", x) || !Mode_VSet(v, "TgType", x) ||
		!LvN_SetS(v, "EntityList", "Players") || !LvN_SetI(v, "UsrControl", 0) || !PORT_OWN(Parking, SetInvunerable, NULL, "v", v)) {
		return Mode_Done(&t, 0);
	}
	Mode_Release(&t, mark);
	if (boxed) {
		if (!Pk_BoxAt(boxName, vPos, box) || !Mode_Put(PK, "LastPark", NULL, box) || !Pk_G("LastPark", box) ||
			!LvN_SetF(box, "rTime", 0.5)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_VObjRead(v, "Name", x) || !Mode_VSet(lcam, "MainTarget", x) || !Mode_VSet(lcam, "Target", x) ||
		!Native_GVarGet("VehicleCamDist", y) || !Mode_VSet(lcam, "CamDist", y) || !Mode_ActionSet("Inactive") ||
		!PORT_CALL(Menu, MovieScorer, NULL, "i", 0L) || !LvN_SetI(w, "AutoSelect", 0) || !Mode_VDel(w)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(now, Mode_Time());
	if (!Mode_AtG(now, PK, "ExecSwitchVehicleMissionCallback", NULL, 0)) {
		return Mode_Done(&t, 0);
	}
	/* Sputnik says goodbye */
	return Mode_Done(&t, Mode_VObjRead(v, "MovePos", x) && Mode_VObjRead(v, "Ang", y) &&
		PORT_CALL(Sputnik, SetSptAt, NULL, "vvs", x, y, "Bye") && Pk_Arrows(3, 4) && Pk_Voice("OnTakeOffSound"));
}

/* PlayVoiceChannel(channel): plays the voice and stops it when it ends */
static byte Pk_PlayVoiceChannelN(const nativeVal_t *channel) {
	nativeVal_t timy = { 0 }, now, def, sum = { 0 }, at = { 0 };
	Native_SetFloat(&def, 0.1);
	if (!Mode_PlayVoice(channel, NULL) || !Mode_VoiceRemain(channel, &def, &timy)) {
		Native_Free(&timy);
		return 0;
	}
	Native_SetFloat(&now, Mode_Time());
	/* SSound.StopVoice as the module holds it (the scheduler takes its entry, Port_Scheduled) */
	nativeVal_t stop = { 0 };
	byte ok = Native_Arith('+', &now, &timy, &sum) && Native_Arith('+', &sum, &def, &at) &&
		Mode_RefMod("SSound", "StopVoice", &stop) && Mode_ScheduleV(&at, &stop, channel, 1, NULL);
	Native_Free(&stop);
	Native_Free(&timy);
	Native_Free(&sum);
	Native_Free(&at);
	return ok;
}

/* ExecSwitchVehicleMissionCallback(): the mission callbacks of boarding */
static byte Pk_ExecSwitchVehicleMissionCallbackN(void) {
	return Pk_Callback("SwitchVehicleMissionCallback") && Pk_Callback("SwitchVehicleMissionCallbackAux");
}

/* EndPathCam(PathCam): the end of the parking camera: the character under control, the walk HUD */
static byte Pk_EndPathCamN(const nativeVal_t *path) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cp = Mode_V(&t), *x = Mode_V(&t), *lcam = Mode_V(&t), *v = Mode_V(&t);
	if (!Mode_Import("CharScorer") || !Pk_Ent(path, cp) || !Mode_VObjRead(cp, "TgtCam03", x) || !Pk_SetCam(x) || !Mode_VDel(cp)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(x);
	Mode_CamName(0, x);
	return Mode_Done(&t, Pk_Ent(x, lcam) && Mode_VObjRead(lcam, "Target", x) && Pk_Ent(x, v) && LvN_SetI(v, "UsrControl", 0) &&
		Pk_Callback("SwitchCharMissionCallback") && Pk_Callback("SwitchCharMissionCallbackAux") && Mode_ActionSet("Walk") &&
		PORT_CALL(CharScorer, Create, NULL, "i", 0L) && LvN_PutNone(PK, "LastCamera") &&
		PORT_OWN(Parking, RestoreDeathPlayerShip, NULL, ""));
}

/* SetCameraStart(cp, v, iPos, split=0, testline=1): the camera point beside the ship at the height of the ground
 * (point 0 for split, else point 1) */
static byte Pk_SetCameraStartN(const nativeVal_t *a) {
	static const char *const vars[2][6] = { { "Pos00", "Ang00", "FOV00", "Near00", "Far00", "Quake00" },
		{ "Pos01", "Ang01", "FOV01", "Near01", "Far01", "Quake01" } };
	const nativeVal_t *cp = &a[0], *v = &a[1], *split = &a[3], *testline = &a[4];
	modeVals_t t = { .n = 0 };
	nativeVal_t *ang = Mode_V(&t), *ang1 = Mode_V(&t), *wr = Mode_V(&t), *r = Mode_V(&t), *off = Mode_V(&t), *rv = Mode_V(&t);
	nativeVal_t *rot = Mode_V(&t), *acPos = Mode_V(&t), *res = Mode_V(&t), *newPos = Mode_V(&t), *y = Mode_V(&t);
	nativeVal_t *y2 = Mode_V(&t), *pos = Mode_V(&t), *a180 = Mode_V(&t), *x0 = Mode_V(&t), *x2 = Mode_V(&t);
	if (!Mode_VObjRead(v, "Ang", ang) || !Pk_Item(ang, 1, ang1) || !Mode_VObjRead(v, "WRadius", wr) || !Native_Arith('+', wr, Mode_I(&t, 50), r)) {
		return Mode_Done(&t, 0);
	}
	Pk_Tuple3(off, Mode_I(&t, 0), Mode_I(&t, 0), r);
	Pk_Tuple3(rv, Mode_I(&t, 0), ang1, Mode_I(&t, 0));
	if (!Mode_Rotate(off, rv, rot) || !Mode_VecAdd(&a[2], rot, acPos)) {
		return Mode_Done(&t, 0);
	}
	const nativeVal_t *np = acPos;
	if (Native_True(testline)) {
		if (!Pk_Down(acPos, res)) {
			return Mode_Done(&t, 0);
		}
		if (Native_True(res)) {
			if (!Pk_Item(res, 0, newPos)) {
				return Mode_Done(&t, 0);
			}
			np = newPos;
		}
	}
	if (!Pk_Item(np, 1, y) || !Mode_VObjRead(v, "WRadius", wr) || !Native_Arith('+', y, wr, y2) ||
		!Native_Arith('+', y2, Mode_I(&t, 50), r) || !Pk_Item(np, 0, x0) || !Pk_Item(np, 2, x2)) {
		return Mode_Done(&t, 0);
	}
	Pk_Tuple3(pos, x0, r, x2);
	if (!Mode_VObjRead(v, "Ang", ang) || !Pk_Yaw(ang, '+', Mode_I(&t, 180), a180) || !LvN_SetF(cp, "Time00", 2.5) ||
		!LvN_SetI(cp, "FadeIn00", 1) || !LvN_SetI(cp, "FadeOut00", 1)) {
		return Mode_Done(&t, 0);
	}
	const char *const *n = vars[Native_True(split) ? 0 : 1];
	return Mode_Done(&t, Mode_VSet(cp, n[0], pos) && Mode_VSet(cp, n[1], a180) && LvN_SetI(cp, n[2], 80) &&
		LvN_SetI(cp, n[3], 50) && LvN_SetI(cp, n[4], 500000) && LvN_SetF(cp, n[5], 0.5) && Mode_VObjRead(cp, "Pos00", y) &&
		Mode_VSet(cp, "camPos", y) && Mode_VObjRead(cp, "Ang00", y) && Mode_VSet(cp, "camAng", y));
}

/* v.IsType("Car") as the script calls it (the value of the method of another object) */
static byte Pk_IsCar(const nativeVal_t *v, nativeVal_t *out) {
	if (v->type == NV_ENT) {
		int k = Mode_VIsType(v, "Car");
		if (k >= 0) {
			Native_SetInt(out, k);
		}
		return k >= 0;
	}
	PyObject *m = Mode_VBound(v, "IsType");
	byte ok = m && Native_Call(m, out, "s", "Car");
	Py_XDECREF(m);
	return ok;
}

/* isValidToPark(v): v and v.IsType("Car") and (v.UsrControl == 0), the value of the and */
static byte Pk_isValidToParkN(const nativeVal_t *v, nativeVal_t *out) {
	if (!Native_True(v)) {
		Native_Copy(out, v);
		return 1;
	}
	nativeVal_t uc = { 0 };
	if (!Pk_IsCar(v, out)) {
		return 0;
	}
	if (!Native_True(out)) {
		return 1;
	}
	byte ok = Mode_VObjRead(v, "UsrControl", &uc);
	if (ok) {
		Native_Free(out);
		Native_SetInt(out, Mode_EqI(&uc, 0));
	}
	Native_Free(&uc);
	return ok;
}

/* isValidToPark(v) through the global: 1, 0, -1 with the error */
static int Pk_Valid(const nativeVal_t *v) {
	nativeVal_t r = { 0 };
	int ok = PORT_OWN(Parking, isValidToPark, &r, "v", v) ? Native_True(&r) : -1;
	Native_Free(&r);
	return ok;
}

/* OnPark(ItemName, VehicleName): a ship slow enough on a parking box parks there: the pilot gets out, the camera
 * path shows it; 1 when it parked */
static byte Pk_OnParkN(const nativeVal_t *itemName, const nativeVal_t *vehName, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *pilot = Mode_V(&t), *descriptor = Mode_V(&t);
	nativeVal_t *i = Mode_V(&t), *iPos = Mode_V(&t), *newPos = Mode_V(&t), *ang = Mode_V(&t), *vAng = Mode_V(&t);
	nativeVal_t *lastCam = Mode_V(&t), *cp = Mode_V(&t), *c = Mode_V(&t), *rot = Mode_V(&t), *off = Mode_V(&t);
	nativeVal_t *dist = Mode_V(&t), *neg = Mode_V(&t), *rv = Mode_V(&t), *a90 = Mode_V(&t), *x0 = Mode_V(&t), *x2 = Mode_V(&t);
	nativeVal_t *p3 = Mode_V(&t), *args = Mode_V(&t);
	Native_SetInt(out, 0);
	if (!Mode_Import("Chars") || !Pk_Ent(vehName, v)) {
		return Mode_Done(&t, 0);
	}
	int valid = Pk_Valid(v);
	if (valid <= 0) {
		return Mode_Done(&t, valid == 0);
	}
	if (!Mode_VObjRead(v, "Speed", x)) {
		return Mode_Done(&t, 0);
	}
	int fast = Mode_CmpG(x, ">", PK, "MinParkingVel");
	if (fast) {
		Native_SetFloat(rv, 0.6);
		return Mode_Done(&t, fast > 0 && Mode_VObjRead(v, "Vel", x) && Mode_VecProd(x, rv, y) && Mode_VSet(v, "Vel", y));
	}
	if (!Mode_VObjRead(v, "MPil0", pilot) || !Native_Slice(pilot, 0, 2, x)) {
		return Mode_Done(&t, 0);
	}
	int bad = !Native_IsStr(x, "P_");
	if (!bad) {
		Native_Free(x);
		if (!Native_Slice(pilot, 2, 0x7fffffff, x)) {
			return Mode_Done(&t, 0);
		}
		bad = LvN_EqS(x, "None");
	}
	if (bad) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_VObjRead(v, "UsrControl", x) || !Mode_VObjRead(v, "Descriptor", descriptor) || !Pk_Ent(itemName, i) ||
		!LvN_SetF(i, "rTime", 0.1) || !Mode_VDel(i) || !Mode_VObjRead(i, "Pos", iPos)) {
		return Mode_Done(&t, 0);
	}
	/* the ship stops and parks */
	if (!LvN_SetI(v, "UsrControl", -1) || !LvN_SetInts3(v, "Vel", 0, 0, 0) || !Pk_Ground(iPos, newPos) ||
		!Mode_VObjRead(v, "Ang", ang) || !Pk_Yaw(ang, '+', NULL, vAng) || !Pk_Item(newPos, 1, x) || !Mode_VObjRead(v, "WRadius", y) ||
		!Native_Arith('+', x, y, rot) || !Pk_Item(newPos, 0, x0) || !Pk_Item(newPos, 2, x2)) {
		return Mode_Done(&t, 0);
	}
	Pk_Tuple3(p3, x0, rot, x2);
	if (!Mode_VSet(v, "MovePos", p3) || !Mode_VSet(v, "MoveAng", vAng) || !LvN_SetF(v, "MoveTime", 2.5) || !Pk_UseArrow(v) ||
		!PORT_OWN(Parking, SetInvunerable, NULL, "v", v) || !Pk_NoBlur()) {
		return Mode_Done(&t, 0);
	}
	/* the camera of the change */
	Mode_CamName(0, lastCam);
	if (!LvN_Create(Mode_S(&t, "CamPath"), Mode_I(&t, 0), Mode_I(&t, 0), Mode_I(&t, 0), "CamPath", cp) ||
		!Mode_Put(PK, "LastCamera", NULL, cp) || !LvN_SetI(cp, "nWayPoint", 4) || !LvN_SetS(cp, "OnEnd", "Parking.EndPathCam") ||
		!Mode_VSet(cp, "TgtCam00", lastCam) || !LvN_SetS(cp, "TgtCam00", "") || !PORT_OWN(Parking, SetCameraStart, NULL, "vvv", cp, v, iPos)) {
		return Mode_Done(&t, 0);
	}
	if (!Pk_Item(vAng, 1, x) || !Native_Arith('+', x, Mode_I(&t, 90), a90) || !Pk_G("ParkingDist", dist) || !Mode_Neg(dist, neg)) {
		return Mode_Done(&t, 0);
	}
	Pk_Tuple3(off, Mode_I(&t, 0), Mode_I(&t, 0), neg);
	Pk_Tuple3(rv, Mode_I(&t, 0), a90, Mode_I(&t, 0));
	if (!Mode_Rotate(off, rv, rot) || !Mode_VecAdd(iPos, rot, x) || !Mode_VSet(cp, "Pos02", x) ||
		!Pk_Yaw(vAng, '+', Mode_I(&t, 90), y) || !Mode_VSet(cp, "Ang02", y) || !LvN_SetI(cp, "FOV02", 70) ||
		!LvN_SetI(cp, "Near02", 50) || !LvN_SetI(cp, "Far02", 500000) || !LvN_SetF(cp, "Time01", 0.0) ||
		!Mode_VSet(cp, "TgtCam03", lastCam) || !LvN_SetF(cp, "Time02", 1.0) || !LvN_SetI(cp, "FadeIn02", 0) ||
		!LvN_SetI(cp, "FadeOut02", 1) || !LvN_SetF(cp, "Time", 0.0)) {
		return Mode_Done(&t, 0);
	}
	/* the character of the player */
	if (!Pk_Yaw(vAng, '+', Mode_I(&t, 90), y)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(vAng);
	*vAng = *y;
	Native_SetNone(y);
	if (!Native_Slice(pilot, 2, 0x7fffffff, x) || !Mode_VObjRead(v, "Pos", y) || !Mode_VObjRead(v, "Ang", ang) ||
		!Pk_Yaw(ang, '-', Mode_I(&t, 90), rv) || !PORT_CALL(Chars, CreateMainChar, c, "svvv", "Player0", x, y, rv)) {
		return Mode_Done(&t, 0);
	}
	if (!Pk_G("ParkingDist", dist) || !Mode_Neg(dist, neg)) {
		return Mode_Done(&t, 0);
	}
	Pk_Tuple3(off, Mode_I(&t, 0), Mode_I(&t, 0), neg);
	if (!Mode_Rotate(off, vAng, rot) || !Mode_VecAdd(iPos, rot, x)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(iPos);
	*iPos = *x;
	Native_SetNone(x);
	if (!Pk_Ground(iPos, newPos) || !Pk_Item(newPos, 1, x) || !Mode_VObjRead(c, "FloorDist", y) || !Native_Arith('+', x, y, rot) ||
		!Pk_Item(newPos, 0, x0) || !Pk_Item(newPos, 2, x2)) {
		return Mode_Done(&t, 0);
	}
	Pk_Tuple3(p3, x0, rot, x2);
	if (!Mode_VSet(c, "Pos", p3) || !Mode_VSet(c, "Descriptor", descriptor) || !LvN_SetI(c, "Active", 0) ||
		!LvN_SetI(c, "UsrControl", -1) || !Mode_VObjRead(v, "TgType", x) || !Mode_VSet(c, "TgType", x) ||
		!LvN_SetS(v, "EntityList", "") || !LvN_SetS(v, "TgType", "None") || !Pk_Ent(lastCam, x) || !Mode_VObjRead(cp, "Name", x) ||
		!Pk_SetCam(x) || !Mode_ActionSet("Inactive") || !Mode_Import("Menu") ||
		!PORT_CALL(Menu, MovieScorer, NULL, "i", 0L)) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_VObjRead(c, "Name", x) || !Mode_VObjRead(v, "Name", y)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(args, NV_TUPLE);
	Native_Copy(Native_Append(args), x);
	Native_Copy(Native_Append(args), y);
	if (!Mode_Later(2.5, PK, "ShowChar", args->items, 2, NULL) || !PORT_OWN(Parking, SetShipRadius, NULL, "vi", v, 1L) ||
		!Mode_Import("Sputnik") || !Mode_VObjRead(v, "Pos", x) || !Mode_VObjRead(v, "Ang", y) ||
		!PORT_CALL(Sputnik, SetSptAt, NULL, "vvs", x, y, "Hello") || !Pk_Voice("OnLandSound")) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(out, 1);
	return Mode_Done(&t, 1);
}

/* SetDtritusPlate(v, Active): the plate shows on the player's ship (single player) */
static byte Pk_SetDtritusPlateN(const nativeVal_t *v, const nativeVal_t *active) {
	nativeVal_t a = { 0 }, b = { 0 }, plate = { 0 };
	if (!Mode_Import("Init")) {
		return 0;
	}
	int t = Mode_ModTrue("Init", "MultiPlayer");
	t = t < 0 ? -1 : !t;
	if (t > 0) {
		t = LvN_GTrue(PK, "DtritusPlate");
	}
	if (t > 0) {
		t = LvN_GTrue(PK, "PlayerShip");
	}
	if (t > 0) {
		t = Native_True(v);
	}
	if (t > 0) {
		t = Pk_G("PlayerShip", &plate) && Mode_VObjRead(&plate, "Name", &a) && Mode_VObjRead(v, "Name", &b) ? Mode_Cmp(&a, "==", &b) : -1;
	}
	if (t > 0) {
		t = Pk_G("DtritusPlate", &plate) && Mode_VSet(&plate, "Active", active) ? 1 : -1;
	}
	Native_Free(&a);
	Native_Free(&b);
	Native_Free(&plate);
	return t >= 0;
}

/* CreateDtritusPlate(): the plate of Dtritus, hidden until it is linked to the player's ship */
static byte Pk_CreateDtritusPlateN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *plate = Mode_V(&t), *model = Mode_V(&t);
	return Mode_Done(&t, LvN_Create(Mode_S(&t, "DtritusPlate"), Mode_I(&t, 0), Mode_I(&t, 0), Mode_I(&t, 0), "WithLife", plate) &&
		Mode_Put(PK, "DtritusPlate", NULL, plate) && Pk_G("DtritusPlate", plate) && Pk_G("DtritusPlateModel", model) &&
		Mode_VModel(plate, model) && LvN_SetI(plate, "Active", 0) && LvN_SetInts3(plate, "OffModPos", 0, 0, 0) &&
		LvN_SetI(plate, "DeltaOffMul", 4) && LvN_SetI(plate, "OffMul", 0) && LvN_SetI(plate, "UseLinkAng", 1));
}

/* CreatePlayerShip(Pos): the ship of the hangar the save game flies (the aux hangar for 9) at Pos, with the plate */
static byte Pk_CreatePlayerShipN(const nativeVal_t *pos) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ship = Mode_V(&t), *x = Mode_V(&t), *hangar = Mode_V(&t), *var = Mode_V(&t), *plate = Mode_V(&t);
	nativeVal_t *y = Mode_V(&t), *hook = Mode_V(&t);
	if (!Mode_Import("ShipEdit") || !Pk_G("PlayerShip", ship)) {
		return Mode_Done(&t, 0);
	}
	if (ship->type != NV_NONE && (!Mode_VDel(ship) || !LvN_PutNone(PK, "PlayerShip"))) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("Hangar.iHangarShip", x);
	if (!Mode_EvalIn(PK, x, hangar)) {
		return Mode_Done(&t, 0);
	}
	if (Mode_EqI(hangar, 9)) {
		Mode_SaveVar("Hangar.HangarShipAux", var);
	} else {
		if (!Native_Arith('+', hangar, Mode_I(&t, 1), x)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(x, y);
		if (!LvN_Cat("Hangar.HangarShip", y, x)) {
			return Mode_Done(&t, 0);
		}
		Mode_SaveVar(Native_Text(x), var);
	}
	if (!PORT_CALL(ShipEdit, CreateShipFromSaveVar, ship, "vvs", var, pos, "PlayerShip") ||
		!Mode_Put(PK, "PlayerShip", NULL, ship) || !Pk_G("PlayerShip", ship) || !LvN_SetI(ship, "Active", 1) ||
		!LvN_SetI(ship, "ActiveCMask", 1) || !Pk_G("DtritusPlate", plate) || !LvN_SetI(plate, "Active", 1) ||
		!LvN_SetS(plate, "LinkModel", ",") || !Pk_G("PlayerShip", ship) || !Mode_VObjRead(ship, "Name", x) ||
		!Native_Arith('+', x, Mode_S(&t, ","), y) || !Pk_G("DtritusPlateNode", x) || !Native_Arith('+', y, x, hook) ||
		!Mode_VSet(plate, "LinkModel", hook)) {
		return Mode_Done(&t, 0);
	}
	if (!Pk_G("OnCreatePlayerShip", hook)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !Native_True(hook) || (Pk_G("OnCreatePlayerShip", hook) && Pk_G("PlayerShip", ship) &&
		Mode_CallArgs(hook, NULL, ship, 1)));
}

/* ShowChar(charname, shipname): the character appears beside the parked ship, Sputnik at the player's ship (moved to
 * the nearest parking box when the player parked another ship) */
static byte Pk_ShowCharN(const nativeVal_t *charName, const nativeVal_t *shipName) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *x = Mode_V(&t), *v = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *ship = Mode_V(&t), *near = Mode_V(&t);
	nativeVal_t *nearest = Mode_V(&t);
	if (!Mode_Import("Sputnik") || !Mode_Import("Init") || !Pk_Ent(charName, x) || !LvN_SetI(x, "Active", 1) ||
		!Pk_Ent(shipName, v)) {
		return Mode_Done(&t, 0);
	}
	int act = Mode_Active(v);
	if (act < 0 || (act && !LvN_SetG(v, "MPil0", PK, "P_None")) || !Pk_Arrows(1, 2) || !Mode_VObjRead(v, "Name", a) ||
		!Pk_G("PlayerShip", ship) || !Mode_VObjRead(ship, "Name", b)) {
		return Mode_Done(&t, 0);
	}
	int own = Mode_Cmp(a, "==", b);
	if (own) {
		return Mode_Done(&t, own > 0 && PORT_CALL(Sputnik, Assign, NULL, "v", v));
	}
	if (!Pk_G("PR_Lst", x) || !Mode_VObjRead(v, "Name", a) || !LvN_Cat("Near", a, b) ||
		!PORT_CALL(Scrap, EntityListGet, near, "vv", x, b) || !Pk_Ent(near, nearest)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !Native_True(nearest) || (Pk_G("PlayerShip", ship) && Mode_VObjRead(nearest, "Pos", x) &&
		Mode_VSet(ship, "Pos", x) && Pk_G("PlayerShip", ship) && LvN_SetInts3(ship, "Ang", 0, 0, 0) &&
		PORT_CALL(Sputnik, Assign, NULL, "v", nearest) && Mode_VDel(nearest)));
}

/* EndJunkerPathCam(PathCam): parked in the junkyard: the level of Rusty's junkyard starts */
static byte Pk_EndJunkerPathCamN(const nativeVal_t *path) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *p = Mode_V(&t), *x = Mode_V(&t), *spawn = Mode_V(&t), *usr = Mode_V(&t);
	return Mode_Done(&t, PORT_OWN(Parking, EndPathCam, NULL, "v", path) && Mode_Import("SaveGame") && Mode_Import("Init") &&
		Mode_ModGet("Init", "Path", p) && Native_Slice(p, 7, 0x7fffffff, x) && LvN_Cat("DM_Player_Spawn_", x, spawn) &&
		Mode_UsrHandle(usr) && Mode_VObjRead(usr, "ActCtrl", x) &&
		PORT_CALL(SaveGame, MarkWayPoint, NULL, "svv", "Levels/Junkyard", spawn, x) &&
		Mode_SaveS("ComeFrom", "DoorEditShip") && Pk_StartGame());
}

/* JunkerPark(ItemName, VehicleName): the junkyard parking; a stolen ship is recycled for a quarter of its price */
static byte Pk_JunkerParkN(const nativeVal_t *itemName, const nativeVal_t *vehName) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *a = Mode_V(&t), *b = Mode_V(&t), *ship = Mode_V(&t), *x = Mode_V(&t), *price = Mode_V(&t);
	nativeVal_t *res = Mode_V(&t), *money = Mode_V(&t), *lang = Mode_V(&t);
	if (!Mode_Import("ShipMaker") || !Pk_Ent(vehName, v)) {
		return Mode_Done(&t, 0);
	}
	int valid = Pk_Valid(v);
	if (valid <= 0) {
		return Mode_Done(&t, valid == 0);
	}
	if (!Mode_Import("Speech")) {
		return Mode_Done(&t, 0);
	}
	int alarm = Mode_AlarmOn();
	if (alarm < 0 || (alarm && !Mode_SetAlarm(Mode_I(&t, 0))) || !Mode_VObjRead(v, "Name", a) || !Pk_G("PlayerShip", ship) ||
		!Mode_VObjRead(ship, "Name", b)) {
		return Mode_Done(&t, 0);
	}
	int stolen = Mode_Cmp(a, "!=", b);
	if (stolen < 0) {
		return Mode_Done(&t, 0);
	}
	if (stolen) {
		if (!Mode_VObjRead(v, "Model", a) || !PORT_CALL(ShipMaker, GetShipPrice, x, "v", a) || !Native_ToInt(x, b) ||
			!Mode_Arith('/', b, Mode_I(&t, 4), price)) {
			return Mode_Done(&t, 0);
		}
	} else {
		Native_SetInt(price, 0);
	}
	if (!PORT_OWN(Parking, OnPark, res, "vv", itemName, vehName)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(res) && Native_True(price)) {
		Mode_SaveS("Hangar.iHangarShip", "-1");
		Native_SetInt(x, Mode_GetMoney());
		if (!Native_Arith('+', x, price, money) || !Mode_SetMoneySeam(money)) {
			return Mode_Done(&t, 0);
		}
		Mode_Lang("OnRecicledAtRusty", lang);
		if (!Native_Arith('%', lang, price, x) || !Mode_SetSaveVar("InitialMessage", x)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Pk_G("LastCamera", x)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, !Native_True(x) || (Pk_G("LastCamera", x) && LvN_SetS(x, "OnEnd", "Parking.EndJunkerPathCam")));
}

/* print a, text, b, text as the two engine callbacks print their line */
static byte Pk_Line(const char *a, const nativeVal_t *ent, const char *b, const nativeVal_t *box) {
	nativeVal_t sa, sb, name = { 0 };
	LvN_Str(&sa, a);
	LvN_Str(&sb, b);
	const nativeVal_t *items[4] = { &sa, ent, &sb, &name };
	byte ok = Mode_VObj(box, "Name", &name) && Mode_Print(items, 4);
	Native_Free(&name);
	return ok;
}

/* OnParkingOccupied(EntityNane, x, y, z, name): the engine: a ship stands on a parking place; a parked one gives way
 * to a parking box */
static byte Pk_OnParkingOccupiedN(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *x = Mode_V(&t), *ship = Mode_V(&t), *l = Mode_V(&t);
	if (!Pk_Ent(&a[0], e) || !Mode_VObjRead(e, "MPil0", x)) {
		return Mode_Done(&t, 0);
	}
	int park = Mode_CmpG(x, "==", PK, "P_None");
	if (park <= 0) {
		return Mode_Done(&t, park == 0);
	}
	if (!Pk_G("PlayerShip", ship) || !Mode_VObjRead(ship, "Name", x) || !Pk_Ent(&a[0], e)) {
		return Mode_Done(&t, 0);
	}
	int own = Mode_Cmp(&a[0], "==", x);
	if (own < 0 || (own ? !LvN_SetI(e, "Active", 0) : !Mode_VDel(e))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, PORT_OWN(Parking, CreateBox, l, "svvv", "Parking", &a[1], &a[2], &a[3]) && LvN_SetF(l, "rTime", 0.1) &&
		Pk_Line("Parking Occupied by", &a[0], " replaced by ", l));
}

/* OnParkedOut(EntityNane, x, y, z, name): the engine: a parking place is free again; a new parked ship there (unless
 * ParkedOutCallback made one) */
static byte Pk_OnParkedOutN(const nativeVal_t *a) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *hook = Mode_V(&t), *pos = Mode_V(&t), *res = Mode_V(&t), *angle = Mode_V(&t), *l = Mode_V(&t);
	if (!Pk_Ent(&a[0], e) || !Mode_VDel(e) || !Pk_G("ParkedOutCallback", hook)) {
		return Mode_Done(&t, 0);
	}
	Pk_Tuple3(pos, &a[1], &a[2], &a[3]);
	if (Native_True(hook)) {
		nativeVal_t *args = Mode_V(&t);
		Native_SetEmpty(args, NV_TUPLE);
		Native_Copy(Native_Append(args), pos);
		Native_Copy(Native_Append(args), &a[4]);
		if (!Pk_G("ParkedOutCallback", hook) || !Mode_CallArgs(hook, res, args->items, 2)) {
			return Mode_Done(&t, 0);
		}
		if (Native_True(res)) {
			return Mode_Done(&t, 1);
		}
	}
	Native_SetFloat(angle, Scrap_RandRange(0, 360));
	return Mode_Done(&t, PORT_OWN(Parking, CreateParkedShip, l, "vvv", pos, angle, &a[4]) &&
		Pk_Line("Parking free by ", &a[0], " filled by ", l));
}

/* CreateBox(Name, x, y, z): a parking box ParkingDist over the ground below the point (None and an error on the
 * console when there is no ground) */
static byte Pk_CreateBoxN(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *iPos = Mode_V(&t), *res = Mode_V(&t), *p = Mode_V(&t), *x = Mode_V(&t), *y = Mode_V(&t), *dist = Mode_V(&t);
	nativeVal_t *z = Mode_V(&t);
	Pk_Tuple3(iPos, &a[1], &a[2], &a[3]);
	if (!Pk_Down(iPos, res)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(res)) {
		return Mode_Done(&t, LvN_Cat("A T R O C I O U S    E R R O R : Parking box ", &a[0], x) &&
			Native_Arith('+', x, Mode_S(&t, " was created in an invalid position.\n"), y) && Mode_ConsoleError(y));
	}
	if (!Pk_Item(res, 0, p) || !Pk_Item(p, 1, x) || !Pk_G("ParkingDist", dist) || !Native_Arith('+', x, dist, y) ||
		!Pk_Item(p, 0, x) || !Pk_Item(p, 2, z) || !LvN_Create(&a[0], x, y, z, "Item", out) ||
		!Mode_VModel(out, Mode_S(&t, "models/Misc/Parking/Parking.m3d")) || !LvN_SetI(out, "Radius", 1000) ||
		!LvN_SetI(out, "AppearsIfFree", 1) || !LvN_SetI(out, "AutoFace", 1) || !LvN_SetS(out, "OnTake", "Parking.OnPark") ||
		!LvN_SetI(out, "OnlyUser", 1) || !LvN_SetG(out, "EntityList", PK, "PR_Lst")) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, 1);
}

/* SetShipRadius(v, parked): a parked ship takes the smaller of its two radii for the world */
static byte Pk_SetShipRadiusN(const nativeVal_t *v, const nativeVal_t *parked) {
	nativeVal_t wr = { 0 }, vr = { 0 };
	byte ok = Mode_VObjRead(v, "WRadius", &wr) && Mode_VObjRead(v, "VRadius", &vr);
	int swap = !ok ? -1 : Native_True(parked) ? Mode_Cmp(&wr, ">", &vr) : Mode_Cmp(&wr, "<", &vr);
	ok = swap >= 0 && (!swap || (Mode_VSet(v, "WRadius", &vr) && Mode_VSet(v, "VRadius", &wr)));
	Native_Free(&wr);
	Native_Free(&vr);
	return ok;
}

/* SetShipAsParked(ShipName): no pilot, the use arrow, on the ground */
static byte Pk_SetShipAsParkedN(const nativeVal_t *name) {
	nativeVal_t v = { 0 }, x = { 0 };
	byte ok = Pk_Ent(name, &v) && LvN_SetG(&v, "MPil0", PK, "P_None") && Pk_UseArrow(&v) && LvN_SetI(&v, "UsrControl", -1) &&
		LvN_SetS(&v, "TgType", "None") && LvN_SetOnFloorOf(&v) && Mode_VObjRead(&v, "Pos", &x) && Mode_VSet(&v, "MovePos", &x) &&
		Mode_VObjRead(&v, "Ang", &x) && Mode_VSet(&v, "MoveAng", &x) && LvN_SetF(&v, "MoveTime", 0.001) &&
		PORT_OWN(Parking, SetShipRadius, NULL, "vi", &v, 1L);
	Native_Free(&v);
	Native_Free(&x);
	return ok;
}

/* ParkedDeat(v, attacker): a parked ship explodes: a parking box in its place (the player who did it may set off the
 * alarm), or Sputnik stays where the player's ship was; 1 for a parked ship */
static byte Pk_ParkedDeatN(const nativeVal_t *v, const nativeVal_t *attacker, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *x = Mode_V(&t), *ship = Mode_V(&t), *a = Mode_V(&t), *vPos = Mode_V(&t), *l = Mode_V(&t), *usr = Mode_V(&t);
	nativeVal_t *spt = Mode_V(&t), *rnd = Mode_V(&t);
	Native_SetInt(out, 0);
	if (!Mode_Import("OutPolice") || !Mode_VObjRead(v, "ArrowCanUse", x)) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_EqI(x, 1)) {
		return Mode_Done(&t, 1);
	}
	if (!Pk_G("PlayerShip", ship) || !Mode_VObjRead(ship, "Name", a) || !Mode_VObjRead(v, "Name", x)) {
		return Mode_Done(&t, 0);
	}
	int other = Mode_Cmp(a, "!=", x);
	if (other < 0) {
		return Mode_Done(&t, 0);
	}
	if (other) {
		if (!Mode_IncSaveVar("Stats.Parking") || !Mode_VObjRead(v, "MovePos", vPos) || !Pk_BoxAt(Mode_S(&t, "Parking"), vPos, l) ||
			!LvN_SetF(l, "rTime", 0.1) || !Mode_VDel(v) || !Mode_UsrHandle(usr)) {
			return Mode_Done(&t, 0);
		}
		int by = Native_True(usr);
		if (by) {
			by = Mode_VObjRead(usr, "Name", x) ? Mode_Cmp(x, "==", attacker) : -1;
		}
		if (by > 0) {
			by = Mode_ModTrue("OutPolice", "PoliceActive");
			if (by > 0) {
				Native_SetFloat(rnd, Scrap_RandRange(0.0, 1.0));
				by = Mode_CmpG(rnd, "<", PK, "AlarmProbOnParkedShipDeath");
			}
			if (by > 0) {
				by = Mode_Import("Speech") && PORT_CALL(Speech, AddRemoteCom, NULL, "ss", "Generics_PoliceTaunt_16",
					"Police") && Mode_SetAlarm(Mode_I(&t, 1)) ? 1 : -1;
			}
		}
		if (by < 0) {
			return Mode_Done(&t, 0);
		}
	} else if (!Mode_Import("Sputnik") || !Mode_Import("Chars") || !Mode_ModGet("Sputnik", "spt", spt) || !Mode_VObjRead(v, "Pos", x) ||
		!Mode_VSet(spt, "Pos", x) || !Mode_VObjRead(spt, "Name", x) || !LvN_SetOnFloor(x) || !Mode_VObjRead(spt, "Pos", x) ||
		!Mode_VSet(spt, "MovePos", x) || !Mode_VObjRead(spt, "Ang", x) || !Mode_VSet(spt, "MoveAng", x) ||
		!LvN_SetF(spt, "MoveTime", 0.0001)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(out, 1);
	return Mode_Done(&t, 1);
}

/* "Park" + dname[3:] and dname[10:] of a parking dummy's name */
static byte Pk_ParkNames(const nativeVal_t *dname, nativeVal_t *box, nativeVal_t *zone) {
	nativeVal_t s = { 0 };
	Native_Free(box);
	Native_Free(zone);
	byte ok = Native_Slice(dname, 3, 0x7fffffff, &s) && LvN_Cat("Park", &s, box) && Native_Slice(dname, 10, 0x7fffffff, zone);
	Native_Free(&s);
	return ok;
}

/* BrowseItemPos(): the parking boxes of the DM_Parking dummies; the player's ship on the one nearest to the player
 * (the junkyard's only when the player comes from there), Sputnik beside it */
static byte Pk_BrowseItemPosN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *myPos = Mode_V(&t), *nearest = Mode_V(&t), *nearestDist = Mode_V(&t), *x = Mode_V(&t);
	nativeVal_t *dummy = Mode_V(&t), *dist = Mode_V(&t), *dname = Mode_V(&t), *dpos = Mode_V(&t), *l = Mode_V(&t);
	nativeVal_t *box = Mode_V(&t), *zone = Mode_V(&t), *ship = Mode_V(&t), *ang = Mode_V(&t);
	if (!Mode_DummyStartSeam("DM_Parking*", 1) || !Mode_UsrHandle(usr) || !Mode_VObjRead(usr, "Pos", myPos)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(nearestDist, 10000000.0);
	Mode_SaveVar("Spawn", x);
	int fromJY = Mode_CmpG(x, "==", PK, "PL_Junk");
	if (fromJY < 0) {
		return Mode_Done(&t, 0);
	}
	for (;;) {
		Native_Free(dummy);
		if (!Mode_DummyNextSeam(dummy)) {
			return Mode_Done(&t, 0);
		}
		if (dummy->type == NV_NONE) {
			break;
		}
		if (!Pk_Item(dummy, 3, x)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(x)) {
			continue;
		}
		Native_Free(dist);
		Native_Free(l);
		if (!Pk_Item(dummy, 1, x) || !Mode_VecSub(myPos, x, l) || !Mode_VecMod(l, dist)) {
			return Mode_Done(&t, 0);
		}
		int closer = Mode_Cmp(nearestDist, ">", dist);
		if (closer && !fromJY) {
			closer = Pk_Item(dummy, 0, x) ? Mode_CmpG(x, "!=", PK, "PR_Junk") : -1;
		}
		if (closer < 0) {
			return Mode_Done(&t, 0);
		}
		if (closer) {
			nativeVal_t aux = *nearest;
			*nearest = *dummy;
			*dummy = aux;
			Native_Free(nearestDist);
			*nearestDist = *dist;
			Native_SetNone(dist);
		}
		if (Native_True(dummy)) {
			if (!Pk_Item(dummy, 0, dname) || !Pk_Item(dummy, 1, dpos) || !Pk_ParkNames(dname, box, zone) ||
				!Pk_BoxAt(box, dpos, l) || !Mode_VObjRead(l, "Pos", x) || !Pk_Zone(0, x, zone)) {
				return Mode_Done(&t, 0);
			}
			int junk = Mode_CmpG(dname, "==", PK, "PR_Junk");
			if (junk < 0 || (junk && !LvN_SetS(l, "OnTake", "Parking.JunkerPark"))) {
				return Mode_Done(&t, 0);
			}
		}
	}
	if (!Native_True(nearest)) {
		return Mode_Done(&t, 1);
	}
	if (!Mode_Import("ShipEdit") || !Mode_SetModVar("ShipEdit", "editShipPosDummy", nearest) || !Pk_Item(nearest, 1, x) ||
		!PORT_OWN(Parking, CreatePlayerShip, NULL, "v", x) || !Pk_G("PlayerShip", ship) || !Pk_Item(nearest, 2, x) ||
		!Pk_Yaw(x, '+', NULL, ang) || !Mode_VSet(ship, "Ang", ang) || !Mode_VObjRead(ship, "Name", x) ||
		!PORT_OWN(Parking, SetShipAsParked, NULL, "v", x) || !Mode_VObjRead(ship, "Pos", dpos) || !Pk_Item(nearest, 0, dname) ||
		!Native_Slice(dname, 10, 0x7fffffff, (Native_Free(zone), zone)) || !Pk_Zone(0, dpos, zone) || !Mode_Import("Sputnik") ||
		!Pk_G("PlayerShip", ship)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, PORT_CALL(Sputnik, Assign, NULL, "v", ship));
}

/* CreateLonelyMercParkings(): the parking boxes of the lonely mercenary mission, their names */
static byte Pk_CreateLonelyMercParkingsN(nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *dname = Mode_V(&t), *dpos = Mode_V(&t), *box = Mode_V(&t), *zone = Mode_V(&t);
	nativeVal_t *l = Mode_V(&t), *x = Mode_V(&t);
	Native_SetEmpty(out, NV_LIST);
	if (!Mode_DummyStartSeam("DM_LonelyMerc_Parking*", 1) || !Mode_DummyNextSeam(dummy)) {
		return Mode_Done(&t, 0);
	}
	while (Native_True(dummy)) {
		if (!Pk_Item(dummy, 0, dname) || !Pk_Item(dummy, 1, dpos) || !Pk_ParkNames(dname, box, zone) || !Pk_BoxAt(box, dpos, l) ||
			!Mode_VObjRead(l, "Name", x)) {
			return Mode_Done(&t, 0);
		}
		Native_Copy(Native_Append(out), x);
		Native_Free(dummy);
		if (!Mode_VObjRead(l, "Pos", x) || !Pk_Zone(0, x, zone) || !Mode_DummyNextSeam(dummy)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, 1);
}

/* string.split(s, sep) (strop's split in C for a text, other values by name for their error; OBJLIB 7) */
static byte Pk_Split(const nativeVal_t *s, const char *sep, nativeVal_t *out) {
	Native_Free(out);
	return s->type == NV_STR ? Native_Split(s, sep, out) : PORT_CALL(string, split, out, "vs", s, sep);
}

/* AllShips in upper case without the mercenaries' ships, the list changed in place (OBJLIB 5: other modules and the
 * cases hold it) */
static byte Pk_AllShips(void) {
	static const char *const mercs[] = { "SMERC1", "SMERC2", "SMERC3" };
	nativeVal_t keys = { 0 }, it = { 0 }, up = { 0 }, k, s;
	if (!Pk_G("AllShips", &keys)) {
		return 0;
	}
	int n = Mode_Len(&keys);
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		Native_SetInt(&k, i);
		Native_Free(&up);
		ok = Pk_Item(&keys, i, &it) && Native_Upper(&it, &up) && Mode_DictPut(PK, "AllShips", NULL, &k, &up);
	}
	for (int i = 0; ok && i < 3; i++) {
		LvN_Str(&s, mercs[i]);
		ok = Mode_RemoveIn(&s, PK, "AllShips");
	}
	Native_Free(&keys);
	Native_Free(&it);
	Native_Free(&up);
	return ok;
}

/* CreateParkedShip(Pos, Angle, Type=None): a parked standard ship of the group Type names (ShipTypes), else any but
 * the mercenaries' (the script makes AllShips upper case and removes them from it) */
static byte Pk_CreateParkedShipN(const nativeVal_t *const *a, nativeVal_t *out) {
	static const char *const imports[] = { "Vehicles", "ShipMaker", "string" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *xyz = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *ang = Mode_V(&t), *nam = Mode_V(&t), *types = Mode_V(&t);
	nativeVal_t *key = Mode_V(&t), *keys = Mode_V(&t), *pick = Mode_V(&t), *x = Mode_V(&t), *name = Mode_V(&t);
	for (int i = 0; i < 3; i++) {
		if (!Mode_Import(imports[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Pk_Item(a[0], 0, xyz) || !Pk_Item(a[0], 1, y) || !Pk_Item(a[0], 2, z) ||
		!LvN_Create(Mode_S(&t, "Parked"), xyz, y, z, "Car", out) || !LvN_SetS(out, "Descriptor", "MegaShip")) {
		return Mode_Done(&t, 0);
	}
	Pk_Tuple3(ang, Mode_I(&t, 0), a[1], Mode_I(&t, 0));
	if (!Mode_VSet(out, "Ang", ang)) {
		return Mode_Done(&t, 0);
	}
	byte skip = 1;
	/* Type=None */
	if (a[2] && Native_True(a[2])) {
		int has;
		if (!Pk_Split(a[2], "_", nam) || !Pk_G("ShipTypes", types) || !Pk_Item(nam, 0, key) || (has = Mode_RefHasKey(types, key)) < 0) {
			return Mode_Done(&t, 0);
		}
		if (has) {
			if (!Mode_Sub(types, key, keys)) {
				return Mode_Done(&t, 0);
			}
			skip = 0;
		}
	}
	if (skip && (!Pk_AllShips() || !Pk_G("AllShips", keys))) {
		return Mode_Done(&t, 0);
	}
	int n = Mode_Len(keys);
	if (n < 0) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(x, Scrap_RandRange(0, n - 0.1));
	return Mode_Done(&t, Native_ToInt(x, pick) && Mode_VObjRead(out, "Name", name) && Mode_Sub(keys, pick, x) &&
		PORT_CALL(Vehicles, SetStdProperties, NULL, "vvs", name, x, "P_None") &&
		PORT_OWN(Parking, SetShipAsParked, NULL, "v", name) && PORT_OWN(Parking, CheckWeapons, NULL, "v", out));
}

/* CheckWeapons(v): the parked ship carries only the weapons the player may have */
static byte Pk_CheckWeaponsN(const nativeVal_t *v) {
	static const char *const weap[] = { "Tesla", "Devastator", "ATPC", "Inferno" };
	static const char *const vars[] = { "isWpn02", "isWpn03", "isWpn04", "isWpn05" };
	nativeVal_t text = { 0 }, list = { 0 }, w;
	Mode_SaveVar("Hangar.availableWeaponsList", &text);
	byte ok = Mode_EvalIn(PK, &text, &list);
	for (int i = 0; ok && i < 4; i++) {
		LvN_Str(&w, weap[i]);
		int in = Mode_In(&list, &w);
		ok = in >= 0 && (in || LvN_SetI(v, vars[i], 0));
	}
	Native_Free(&text);
	Native_Free(&list);
	return ok;
}

/* BrowseParkedPos(): a parked ship on every active DM_Parked dummy (unless ParkedOutCallback made one) */
static byte Pk_BrowseParkedPosN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *x = Mode_V(&t), *name = Mode_V(&t), *dpos = Mode_V(&t), *hook = Mode_V(&t);
	nativeVal_t *res = Mode_V(&t), *args = Mode_V(&t);
	if (!Mode_Import("Vehicles") || !Mode_Import("ShipMaker") || !Mode_DummyStartSeam("DM_Parked*", 1)) {
		return Mode_Done(&t, 0);
	}
	for (;;) {
		Native_Free(dummy);
		if (!Mode_DummyNextSeam(dummy)) {
			return Mode_Done(&t, 0);
		}
		if (dummy->type == NV_NONE) {
			return Mode_Done(&t, 1);
		}
		if (!Pk_Item(dummy, 3, x)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(x)) {
			continue;
		}
		Native_Free(name);
		if (!Pk_Item(dummy, 0, x) || !Native_Slice(x, 10, 0x7fffffff, name) || !Pk_Item(dummy, 1, dpos) ||
			!Pk_G("ParkedOutCallback", hook)) {
			return Mode_Done(&t, 0);
		}
		byte create = 1;
		if (Native_True(hook)) {
			Native_Free(args);
			Native_SetEmpty(args, NV_TUPLE);
			Native_Copy(Native_Append(args), dpos);
			Native_Copy(Native_Append(args), name);
			Native_Free(res);
			if (!Pk_G("ParkedOutCallback", hook) || !Mode_CallArgs(hook, res, args->items, 2)) {
				return Mode_Done(&t, 0);
			}
			create = !Native_True(res);
		}
		if (create && (!Pk_Item(dummy, 2, x) || !Pk_Item(x, 1, res) || !PORT_OWN(Parking, CreateParkedShip, NULL, "vvv", dpos, res, name))) {
			return Mode_Done(&t, 0);
		}
		if (!Pk_Zone(1, dpos, name)) {
			return Mode_Done(&t, 0);
		}
	}
}

/* ChangeLevel(ItemName, VehicleName): the player flies through a link to the next level: the way point, the ship
 * (its parts when it is not the player's own) and its ammunition for the next level */
static byte Pk_ChangeLevelN(const nativeVal_t *itemName) {
	static const char *const ammo[][2] = { { "usrShip.Ammo00", "Ammo00" }, { "usrShip.Ammo01", "Ammo01" },
		{ "usrShip.Ammo02", "Ammo02" }, { "usrShip.AcWeap", "AcWeap" } };
	static const char *const parts[] = { "\"", "Model", "\",\"", "MEng0", "\",\"", "MEng1", "\",\"", "MEng2", "\",\"", "MEng3",
		"\",\"", "WeapList", "\"," };
	modeVals_t t = { .n = 0 };
	nativeVal_t *usr = Mode_V(&t), *x = Mode_V(&t), *level = Mode_V(&t), *spawn = Mode_V(&t), *y = Mode_V(&t);
	nativeVal_t *ship = Mode_V(&t), *s = Mode_V(&t), *r = Mode_V(&t);
	if (!Mode_Import("SaveGame") || !Mode_Import("Init") || !Mode_UsrHandle(usr) || !Native_Slice(itemName, 11, 0x7fffffff, x) ||
		!LvN_Cat("Levels/", x, level) || !Pk_G("PR_Spawn", x) || !Mode_ModGet("Init", "Path", y) ||
		!Native_Slice(y, 7, 0x7fffffff, r) || !Native_Arith('+', x, r, spawn) || !Mode_VObjRead(usr, "MPil0", x) ||
		!Native_Slice(x, 2, 0x7fffffff, (Native_Free(y), y)) ||
		!PORT_CALL(SaveGame, MarkWayPoint, NULL, "vvv", level, spawn, y)) {
		return Mode_Done(&t, 0);
	}
	int own = LvN_GTrue(PK, "PlayerShip");
	if (own > 0) {
		own = Mode_VObjRead(usr, "Name", x) && Pk_G("PlayerShip", ship) && Mode_VObjRead(ship, "Name", y) ? Mode_Cmp(x, "==", y) : -1;
	}
	if (own < 0) {
		return Mode_Done(&t, 0);
	}
	if (own) {
		if (!Pk_G("PR_ComeFrom", x) || !Mode_SetSaveVar("ComeFrom", x)) {
			return Mode_Done(&t, 0);
		}
	} else {
		if (!Pk_G("PR_ComeFrom", s)) {
			return Mode_Done(&t, 0);
		}
		for (int i = 0; i < 13; i++) {
			if (i & 1) {
				if (!Mode_VObjRead(usr, parts[i], x)) {
					return Mode_Done(&t, 0);
				}
			} else {
				Native_Free(x);
				LvN_Str(x, parts[i]);
			}
			Native_Free(r);
			if (!Native_Arith('+', s, x, r)) {
				return Mode_Done(&t, 0);
			}
			Native_Free(s);
			*s = *r;
			Native_SetNone(r);
		}
		if (!Mode_VObjRead(usr, "MaxLife", x)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(y);
		Mode_Repr(x, y);
		if (!Native_Arith('+', s, y, r) || !Native_Arith('+', r, Mode_S(&t, ",\""), (Native_Free(s), s)) ||
			!Mode_VObjRead(usr, "Descriptor", x) || !Native_Arith('+', s, x, (Native_Free(r), r)) ||
			!Native_Arith('+', r, Mode_S(&t, "\""), (Native_Free(s), s)) || !Mode_SetSaveVar("ComeFrom", s)) {
			return Mode_Done(&t, 0);
		}
	}
	for (int i = 0; i < 4; i++) {
		if (!Mode_VObjRead(usr, ammo[i][1], x)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(y);
		Mode_Repr(x, y);
		if (!Mode_SetSaveVar(ammo[i][0], y)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Pk_StartGame());
}

/* the loops over the dummies of a pattern: an entity of the class for each (DM_AreaToPark_ and DM_OutLink_) */
static byte Pk_Browse(const char *pattern, const char *cls, byte link) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *name = Mode_V(&t), *p = Mode_V(&t), *xyz = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t);
	nativeVal_t *l = Mode_V(&t), *ang = Mode_V(&t), *a = Mode_V(&t);
	if (!Mode_DummyStartSeam(pattern, 1)) {
		return Mode_Done(&t, 0);
	}
	for (;;) {
		Native_Free(dummy);
		if (!Mode_DummyNextSeam(dummy)) {
			return Mode_Done(&t, 0);
		}
		if (dummy->type == NV_NONE) {
			return Mode_Done(&t, 1);
		}
		Native_Free(l);
		if (!Pk_Item(dummy, 1, p) || !Pk_Item(dummy, 0, name) || !Pk_Item(p, 0, xyz) || !Pk_Item(p, 1, y) || !Pk_Item(p, 2, z) ||
			!LvN_Create(name, xyz, y, z, cls, l)) {
			return Mode_Done(&t, 0);
		}
		if (link && (!Pk_Item(dummy, 2, a) || !Pk_Yaw(a, '+', Mode_I(&t, 180), (Native_Free(ang), ang)) || !Mode_VSet(l, "Ang", ang) ||
			!LvN_SetI(l, "Radius", 20000) || !LvN_SetS(l, "OnTake", "Parking.ChangeLevel") || !LvN_SetI(l, "OnlyUser", 1))) {
			return Mode_Done(&t, 0);
		}
	}
}

/* BrowseParkingAreas(): an object for every DM_AreaToPark_ dummy */
static byte Pk_BrowseParkingAreasN(void) {
	return Pk_Browse("DM_AreaToPark_*", "Obj", 0);
}

/* BrowseSwitchLevel(): the links to the next levels (DM_OutLink_ dummies) */
static byte Pk_BrowseSwitchLevelN(void) {
	return Mode_Import("Vehicles") && Mode_Import("ShipMaker") && Pk_Browse("DM_OutLink_*", "Item", 1);
}

/* ShowNextMap(EntityName, EventName, Time): the event of a link door: the name of the next station */
static byte Pk_ShowNextMapN(const nativeVal_t *ent) {
	nativeVal_t msg = { 0 }, s = { 0 }, key = { 0 }, station = { 0 }, text = { 0 };
	byte ok = Mode_Import("RaceMaker") && Mode_Import("string");
	if (ok) {
		Mode_Lang("Generics_Outlink_SystemMessage", &msg);
		ok = Native_Slice(ent, 15, 0x7fffffff, &s) && LvN_Cat("Station_", &s, &key) && Mode_LangV(&key, &station);
	}
	if (ok) {
		Mode_Replace(&msg, "<n1>", &station, &text);
		ok = PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", &text);
	}
	Native_Free(&msg);
	Native_Free(&s);
	Native_Free(&key);
	Native_Free(&station);
	Native_Free(&text);
	return ok;
}

/* DoorInitializator(Class): the event of the Open action of a link door */
static byte Pk_DoorInitializatorN(void) {
	nativeVal_t r = { 0 }, v;
	byte ok = LvN_GetAction("Open", &r);
	Native_Free(&r);
	Native_SetInt(&v, 0);
	if (!ok || !LvN_SetAct("AddEventAtTime", &v)) {
		return 0;
	}
	LvN_Str(&v, "Open");
	ok = LvN_SetAct("EventName", &v);
	Native_Free(&v);
	LvN_Str(&v, "Parking.ShowNextMap");
	ok = ok && LvN_SetAct("EventFunc", &v);
	Native_Free(&v);
	return ok;
}

/* the arrival from another outdoor level (Init): on the player's own ship or on the stolen ship the save variable
 * ComeFrom describes, at the spawn dummy, with its ammunition */
static byte Pk_Arrive(const nativeVal_t *comeFrom, const nativeVal_t *spawn) {
	static const char *const ammo[][2] = { { "Ammo00", "usrShip.Ammo00" }, { "Ammo01", "usrShip.Ammo01" },
		{ "Ammo02", "usrShip.Ammo02" } };
	modeVals_t t = { .n = 0 };
	nativeVal_t *usrPlay = Mode_V(&t), *dummy = Mode_V(&t), *x = Mode_V(&t), *usrShip = Mode_V(&t), *ship = Mode_V(&t);
	nativeVal_t *pos = Mode_V(&t), *l = Mode_V(&t), *p = Mode_V(&t), *xyz = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t);
	nativeVal_t *ca = Mode_V(&t), *at = Mode_V(&t), *ang = Mode_V(&t), *name = Mode_V(&t);
	const char *s;
	if (!Mode_UsrHandle(usrPlay) || !LvN_SetI(usrPlay, "Active", 0) || !Native_ArgStr(spawn, 1, &s) ||
		!Mode_DummyStartSeam(s, 0) || !Mode_DummyNextSeam(dummy)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(dummy)) {
		return Mode_Done(&t, LvN_Cat("E R R O R : ", spawn, x) && Native_Arith('+', x, Mode_S(&t, " not found.\n"), y) &&
			Mode_ConsoleError(y));
	}
	int own = Mode_CmpG(comeFrom, "==", PK, "PR_ComeFrom");
	if (own < 0) {
		return Mode_Done(&t, 0);
	}
	if (own) {
		if (!Pk_G("PlayerShip", usrShip) || !Pk_G("PlayerShip", ship) || !Mode_VObjRead(ship, "Pos", pos) ||
			!Pk_BoxAt(Mode_S(&t, "ParkingPlayerShip"), pos, l) || !LvN_SetF(l, "rTime", 0.1)) {
			return Mode_Done(&t, 0);
		}
	} else {
		if (!Pk_Item(dummy, 1, p) || !Pk_Item(p, 0, xyz) || !Pk_Item(p, 1, y) || !Pk_Item(p, 2, z) ||
			!LvN_Create(Mode_S(&t, "StolenShip"), xyz, y, z, "Car", usrShip) || !Native_Slice(comeFrom, 8, 0x7fffffff, x) ||
			!Mode_EvalIn(PK, x, ca) || !Mode_VObjRead(usrShip, "Name", name)) {
			return Mode_Done(&t, 0);
		}
		/* Vehicles.SetProperties(o1.Name, a[0], a[6], "P_None", a[1], a[2], a[3], a[4], a[5], a[7]) */
		static const signed char order[] = { 0, 6, -1, 1, 2, 3, 4, 5, 7 };
		nativeVal_t *items = Mode_V(&t);
		Native_SetEmpty(items, NV_TUPLE);
		Native_Copy(Native_Append(items), name);
		for (int i = 0; i < 9; i++) {
			nativeVal_t *it = Native_Append(items);
			if (order[i] < 0) {
				LvN_Str(it, "P_None");
			} else if (!Mode_Item(ca, order[i], it)) {
				return Mode_Done(&t, 0);
			}
		}
		if (!PORT_CALL(Vehicles, SetProperties, NULL, "vvvvvvvvvv", &items->items[0], &items->items[1], &items->items[2],
			&items->items[3], &items->items[4], &items->items[5], &items->items[6], &items->items[7], &items->items[8],
			&items->items[9]) || !Pk_G("PlayerShip", ship) || !Mode_VObjRead(ship, "Pos", pos) || !Pk_Item(pos, 0, xyz) ||
			!Pk_Item(pos, 1, y) || !Pk_Item(pos, 2, z) || !PORT_OWN(Parking, OnParkingOccupied, NULL, "svvvs", "PlayerShip", xyz,
			y, z, "_05") || !Pk_G("DtritusPlate", ship) || !LvN_SetI(ship, "Active", 0)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Pk_Item(dummy, 1, at) || !Mode_VSet(usrShip, "MovePos", at) || !Mode_VSet(usrShip, "Pos", at) ||
		!Pk_Item(dummy, 2, x) || !Pk_Yaw(x, '+', Mode_I(&t, 180), ang) || !Mode_VSet(usrShip, "MoveAng", ang) ||
		!Mode_VSet(usrShip, "Ang", ang)) {
		return Mode_Done(&t, 0);
	}
	for (int i = 0; i < 3; i++) {
		Native_Free(x);
		Native_Free(y);
		Mode_SaveVar(ammo[i][1], x);
		if (!Native_ToFloat(x, y) || !Mode_VSet(usrShip, ammo[i][0], y)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, LvN_PutI(PK, "SetWeaponOnShip", 1) && Mode_VObjRead(usrPlay, "Name", x) && Mode_VObjRead(usrShip, "Name", y) &&
		PORT_OWN(Parking, UseShip, NULL, "vvssv", x, y, "", "", Mode_V(&t)));
}

/* Init(): offline: the plate, the parking boxes and the player's ship, the parked ships (unless the save game says
 * the level is desolate), the links and parking areas; the player starts in the junkyard parking or arrives from
 * another outdoor level on a ship */
static byte Pk_InitN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *x = Mode_V(&t), *comeFrom = Mode_V(&t), *spawn = Mode_V(&t), *n = Mode_V(&t), *usr = Mode_V(&t);
	nativeVal_t *ship = Mode_V(&t), *y = Mode_V(&t);
	if (!Mode_Import("Vehicles")) {
		return Mode_Done(&t, 0);
	}
	int off = Native_NetOff();
	if (off <= 0) {
		return Mode_Done(&t, off == 0);
	}
	if (!PORT_OWN(Parking, CreateDtritusPlate, NULL, "") || !Pk_G("PR_Lst", x) ||
		!PORT_CALL(Scrap, CreateEntityList, NULL, "v", x) || !PORT_OWN(Parking, BrowseItemPos, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	Mode_SaveVar("ComeFrom", comeFrom);
	Mode_SaveVar("Spawn", spawn);
	Native_Free(x);
	Mode_SaveVar("Parking.Desolate", x);
	if (!Native_ToInt(x, n) || (!Native_True(n) && !PORT_OWN(Parking, BrowseParkedPos, NULL, "")) ||
		!PORT_OWN(Parking, BrowseSwitchLevel, NULL, "") || !PORT_OWN(Parking, BrowseParkingAreas, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	int junk = Mode_CmpG(spawn, "==", PK, "PL_Junk");
	if (junk < 0) {
		return Mode_Done(&t, 0);
	}
	if (junk) {
		return Mode_Done(&t, Mode_UsrHandle(usr) && Mode_VObjRead(usr, "Name", x) && Pk_G("PlayerShip", ship) &&
			Mode_VObjRead(ship, "Name", y) && PORT_OWN(Parking, UseShip, NULL, "vvsss", x, y, "", "", "ParkParking_Junkyard") &&
			Pk_G("LastPark", ship) && LvN_SetS(ship, "OnTake", "Parking.JunkerPark"));
	}
	Native_Free(x);
	if (!Native_Slice(comeFrom, 0, 8, x)) {
		return Mode_Done(&t, 0);
	}
	int arrive = Mode_CmpG(x, "==", PK, "PR_ComeFrom");
	if (arrive > 0) {
		Native_Free(x);
		arrive = Native_Slice(spawn, 0, 14, x) ? Mode_CmpG(x, "==", PK, "PR_Spawn") : -1;
	}
	return Mode_Done(&t, arrive >= 0 && (!arrive || Pk_Arrive(comeFrom, spawn)));
}

/* the adapters (the argument formats of the C port: the arity errors of PyArg_ParseTuple, the keyword functions with
 * the names of the script) */
MODE_FN(Parking, RestoreDeathPlayerShip, 0, Pk_RestoreDeathPlayerShipN())
MODE_FN(Parking, SetInvunerable, 1, Pk_SetInvunerableN(&a[0]))
MODE_FN(Parking, CharOnShip, 1, Pk_CharOnShipN(&a[0]))
MODE_FN(Parking, PlayVoiceChannel, 1, Pk_PlayVoiceChannelN(&a[0]))
MODE_FN(Parking, ExecSwitchVehicleMissionCallback, 0, Pk_ExecSwitchVehicleMissionCallbackN())
MODE_FN(Parking, EndPathCam, 1, Pk_EndPathCamN(&a[0]))
MODE_FN(Parking, isValidToPark, 1, Pk_isValidToParkN(&a[0], out))
MODE_FN(Parking, OnPark, 2, Pk_OnParkN(&a[0], &a[1], out))
MODE_FN(Parking, SetDtritusPlate, 2, Pk_SetDtritusPlateN(&a[0], &a[1]))
MODE_FN(Parking, CreateDtritusPlate, 0, Pk_CreateDtritusPlateN())
MODE_FN(Parking, CreatePlayerShip, 1, Pk_CreatePlayerShipN(&a[0]))
MODE_FN(Parking, ShowChar, 2, Pk_ShowCharN(&a[0], &a[1]))
MODE_FN(Parking, EndJunkerPathCam, 1, Pk_EndJunkerPathCamN(&a[0]))
MODE_FN(Parking, JunkerPark, 2, Pk_JunkerParkN(&a[0], &a[1]))
MODE_FN(Parking, OnParkingOccupied, 5, Pk_OnParkingOccupiedN(a))
MODE_FN(Parking, OnParkedOut, 5, Pk_OnParkedOutN(a))
MODE_FN(Parking, CreateBox, 4, Pk_CreateBoxN(a, out))
MODE_FN(Parking, SetShipRadius, 2, Pk_SetShipRadiusN(&a[0], &a[1]))
MODE_FN(Parking, SetOnFloor, 1, LvN_SetOnFloorOf(&a[0]))
MODE_FN(Parking, SetShipAsParked, 1, Pk_SetShipAsParkedN(&a[0]))
MODE_FN(Parking, ParkedDeat, 2, Pk_ParkedDeatN(&a[0], &a[1], out))
MODE_FN(Parking, BrowseItemPos, 0, Pk_BrowseItemPosN())
MODE_FN(Parking, CreateLonelyMercParkings, 0, Pk_CreateLonelyMercParkingsN(out))
MODE_FN(Parking, CheckWeapons, 1, Pk_CheckWeaponsN(&a[0]))
MODE_FN(Parking, BrowseParkedPos, 0, Pk_BrowseParkedPosN())
MODE_FN(Parking, ChangeLevel, 2, Pk_ChangeLevelN(&a[0]))
MODE_FN(Parking, BrowseParkingAreas, 0, Pk_BrowseParkingAreasN())
MODE_FN(Parking, BrowseSwitchLevel, 0, Pk_BrowseSwitchLevelN())
MODE_FN(Parking, ShowNextMap, 3, Pk_ShowNextMapN(&a[0]))
MODE_FN(Parking, DoorInitializator, 1, Pk_DoorInitializatorN())
MODE_FN(Parking, Init, 0, Pk_InitN())

/* the slots of a keyword function for a core that takes NULL for an argument not given */
static void Pk_Given(const nativeVal_t *v, const byte *given, int n, const nativeVal_t **p) {
	for (int i = 0; i < n; i++) {
		p[i] = given[i] ? &v[i] : NULL;
	}
}

MODE_KW(Parking, UseShip, "OOOO|O", "UsrName", "CharName", "optId", "FullOption", "BoxName") {
	const nativeVal_t *p[5];
	Pk_Given(v, given, 5, p);
	return Pk_UseShipN(p, out);
}

MODE_KW(Parking, SetCameraStart, "OOO|OO", "cp", "v", "iPos", "split", "testline") {
	Mode_KwDefI(v, given, 3, 0);
	Mode_KwDefI(v, given, 4, 1);
	return Pk_SetCameraStartN(v);
}

MODE_KW(Parking, CreateParkedShip, "OO|O", "Pos", "Angle", "Type") {
	const nativeVal_t *p[3];
	Pk_Given(v, given, 3, p);
	return Pk_CreateParkedShipN(p, out);
}

PyMethodDef g_ParkingMethods[] = {
	{ "RestoreDeathPlayerShip", Parking_RestoreDeathPlayerShip, 1, NULL },
	{ "SetInvunerable", Parking_SetInvunerable, 1, NULL },
	{ "CharOnShip", Parking_CharOnShip, 1, NULL },
	{ "UseShip", (PyCFunction)(void (*)(void))Parking_UseShip, 3, NULL },
	{ "PlayVoiceChannel", Parking_PlayVoiceChannel, 1, NULL },
	{ "ExecSwitchVehicleMissionCallback", Parking_ExecSwitchVehicleMissionCallback, 1, NULL },
	{ "EndPathCam", Parking_EndPathCam, 1, NULL },
	{ "SetCameraStart", (PyCFunction)(void (*)(void))Parking_SetCameraStart, 3, NULL },
	{ "isValidToPark", Parking_isValidToPark, 1, NULL },
	{ "OnPark", Parking_OnPark, 1, NULL },
	{ "SetDtritusPlate", Parking_SetDtritusPlate, 1, NULL },
	{ "CreateDtritusPlate", Parking_CreateDtritusPlate, 1, NULL },
	{ "CreatePlayerShip", Parking_CreatePlayerShip, 1, NULL },
	{ "ShowChar", Parking_ShowChar, 1, NULL },
	{ "EndJunkerPathCam", Parking_EndJunkerPathCam, 1, NULL },
	{ "JunkerPark", Parking_JunkerPark, 1, NULL },
	{ "OnParkingOccupied", Parking_OnParkingOccupied, 1, NULL },
	{ "OnParkedOut", Parking_OnParkedOut, 1, NULL },
	{ "CreateBox", Parking_CreateBox, 1, NULL },
	{ "SetShipRadius", Parking_SetShipRadius, 1, NULL },
	{ "SetOnFloor", Parking_SetOnFloor, 1, NULL },
	{ "SetShipAsParked", Parking_SetShipAsParked, 1, NULL },
	{ "ParkedDeat", Parking_ParkedDeat, 1, NULL },
	{ "BrowseItemPos", Parking_BrowseItemPos, 1, NULL },
	{ "CreateLonelyMercParkings", Parking_CreateLonelyMercParkings, 1, NULL },
	{ "CreateParkedShip", (PyCFunction)(void (*)(void))Parking_CreateParkedShip, 3, NULL },
	{ "CheckWeapons", Parking_CheckWeapons, 1, NULL },
	{ "BrowseParkedPos", Parking_BrowseParkedPos, 1, NULL },
	{ "ChangeLevel", Parking_ChangeLevel, 1, NULL },
	{ "BrowseParkingAreas", Parking_BrowseParkingAreas, 1, NULL },
	{ "BrowseSwitchLevel", Parking_BrowseSwitchLevel, 1, NULL },
	{ "ShowNextMap", Parking_ShowNextMap, 1, NULL },
	{ "DoorInitializator", Parking_DoorInitializator, 1, NULL },
	{ "Init", Parking_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Accelerator ---- */

#define ACC "Accelerator"

/* CreateAccelerator(Pos, Angx, Angy, Radius=ACCELERATOR_RADIUS): the item of a speed ring */
static byte Acc_CreateAcceleratorN(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t xyz[3] = { { 0 }, { 0 }, { 0 } }, name, ang = { 0 }, z;
	LvN_Str(&name, "Accelerator");
	Native_SetInt(&z, 0);
	byte ok = Pk_XYZ(&a[0], xyz) && LvN_Create(&name, &xyz[0], &xyz[1], &xyz[2], "Item", out) && Mode_VSet(out, "Radius", &a[3]) &&
		LvN_SetS(out, "OnTake", "!AcceleratorOnEnter");
	if (ok) {
		Pk_Tuple3(&ang, &a[1], &a[2], &z);
		ok = Mode_VSet(out, "Ang", &ang);
	}
	for (int i = 0; i < 3; i++) {
		Native_Free(&xyz[i]);
	}
	Native_Free(&name);
	Native_Free(&ang);
	return ok;
}

/* Init(): a speed ring on every active DM_Accelerator dummy ("=<radius>" in the name gives its radius) */
static byte Acc_InitN(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *x = Mode_V(&t), *s = Mode_V(&t), *radius = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *pos = Mode_V(&t), *ax = Mode_V(&t), *ay = Mode_V(&t);
	if (!Mode_Import("string") || !Mode_DummyStartSeam("DM_Accelerator*", 1)) {
		return Mode_Done(&t, 0);
	}
	for (;;) {
		Native_Free(dummy);
		if (!Mode_DummyNextSeam(dummy)) {
			return Mode_Done(&t, 0);
		}
		if (dummy->type == NV_NONE) {
			return Mode_Done(&t, 1);
		}
		if (!Pk_Item(dummy, 3, x)) {
			return Mode_Done(&t, 0);
		}
		if (!Native_True(x)) {
			continue;
		}
		if (!Pk_Item(dummy, 0, x) || !Pk_Split(x, "=", s)) {
			return Mode_Done(&t, 0);
		}
		int n = Mode_Len(s);
		Native_Free(radius);
		if (n < 0 || !(n > 1 ? Pk_Item(s, 1, x) && Native_ToInt(x, radius) : Mode_G(ACC, "ACCELERATOR_RADIUS", radius)) ||
			!Pk_Item(dummy, 2, ang) || !Pk_Item(dummy, 1, pos) || !Pk_Item(ang, 0, ax) || !Pk_Item(ang, 1, ay) ||
			!PORT_OWN(Accelerator, CreateAccelerator, NULL, "vvvv", pos, ax, ay, radius)) {
			return Mode_Done(&t, 0);
		}
	}
}

MODE_KW(Accelerator, CreateAccelerator, "OOO|O", "Pos", "Angx", "Angy", "Radius") {
	/* the default of the def line, bound when the script loaded */
	Mode_KwDefI(v, given, 3, 3500);
	return Acc_CreateAcceleratorN(v, out);
}

MODE_FN(Accelerator, Init, 0, Acc_InitN())

PyMethodDef g_AcceleratorMethods[] = {
	{ "CreateAccelerator", (PyCFunction)(void (*)(void))Accelerator_CreateAccelerator, 3, NULL },
	{ "Init", Accelerator_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(Parking)
