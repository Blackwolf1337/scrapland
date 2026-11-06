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
 */
#include "scrap/scrap.h"
#include "scrap/script.h"

#define PK "Parking"

/* a global of Parking as its functions read it */
#define G(name) Sf_Global(&f, PK, name)

/* the five motion blur variables off */
static void Pk_NoBlur(scriptFrame_t *f) {
	static const char *const vars[] = { "MotionBlurFactorBegin", "MotionBlurFactorEnd", "MotionBlurTime",
		"MotionBlurEndTime", "MotionBlurOffVar" };
	for (int i = 0; i < 5; i++) {
		Sf_Ask(f, "Scrap", "Set", "(sd)", vars[i], 0.0);
	}
}

/* Scrap.TestLine(p, (p[0], p[1] - 10000, p[2]), "") */
static PyObject *Pk_Down(scriptFrame_t *f, PyObject *p) {
	PyObject *to = Sf_Build(f, "(OOO)", Sf_Item(f, p, 0), Sf_OpI(f, Sf_Item(f, p, 1), '-', 10000), Sf_Item(f, p, 2));
	return Sf_Ask(f, "Scrap", "TestLine", "(OOs)", p, to, "");
}

/* -o */
static PyObject *Pk_Neg(scriptFrame_t *f, PyObject *o) {
	return f->failed || !o ? NULL : Sf_Keep(f, PyNumber_Negative(o));
}

/* if callback: callback() (the global read again for the call, like the script) */
static void Pk_Callback(scriptFrame_t *f, const char *name) {
	if (Sf_True(f, Sf_Global(f, PK, name)) > 0) {
		Sf_Call(f, Sf_Global(f, PK, name), "()");
	}
}

/* SSound.OpenVoice(Scrap.Get("StreamPath") + OutSound.<sound>, 1) and PlayVoiceChannel(1) 0.3 s later */
static void Pk_Voice(scriptFrame_t *f, const char *sound) {
	Sf_Import(f, "OutSound");
	PyObject *file = Sf_Op(f, Sf_Ask(f, "Scrap", "Get", "(s)", "StreamPath"), '+', Sf_Mod(f, "OutSound", sound));
	Sf_Ask(f, "SSound", "OpenVoice", "(Oi)", file, 1);
	Sf_Later(f, Sf_Float(f, 0.3), Sf_Global(f, PK, "PlayVoiceChannel"), Sf_Build(f, "(i)", 1), NULL);
}

/* SaveGame.StartGame(CleanRevenge = 1) */
static void Pk_StartGame(scriptFrame_t *f) {
	PyObject *fn = Sf_Mod(f, "SaveGame", "StartGame");
	PyObject *kw = Sf_Build(f, "{s:i}", "CleanRevenge", 1);
	Sf_CallKw(f, fn, kw ? PyTuple_New(0) : NULL, kw);
}

/* RestoreDeathPlayerShip(): the ships of the player can die again; LocalShip is deleted */
static PyObject *Parking_RestoreDeathPlayerShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	static const char *const ships[] = { "LocalShip", "PlayerShip" };
	for (int i = 0; i < 2 && !f.failed; i++) {
		Sf_SetAttrS(&f, G(ships[i]), "OnDeath", "Vehicles.OnDeath");
		if (Sf_Compare(&f, Sf_Attr(&f, G(ships[i]), "Life"), "<=", Sf_Int(&f, 0)) > 0) {
			Sf_SetAttrI(&f, G(ships[i]), "Life", 1);
		}
		if (i == 0) {
			Sf_DelGlobal(&f, PK, "LocalShip");
		}
	}
	Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Players", "OnDeath", "Chars.MainCharDeath");
	return Sf_None(&f);
}

/* SetInvunerable(v): the players and the ships do not die while the camera moves; LocalShip = v */
static PyObject *Parking_SetInvunerable(PyObject *self, PyObject *args) {
	PyObject *v;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "EntityListSet", "(sss)", "Players", "OnDeath", "");
	Sf_SetGlobal(&f, PK, "LocalShip", v);
	Sf_SetAttrS(&f, G("LocalShip"), "OnDeath", "");
	Sf_SetAttrS(&f, G("PlayerShip"), "OnDeath", "");
	return Sf_None(&f);
}

/* CharOnShip(PathCam): the end of the boarding camera: the ship under control, the ship HUD, its name */
static PyObject *Parking_CharOnShip(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "RacerScorer");
	PyObject *cp = Sf_Entity(&f, path);
	Sf_Ask(&f, "Scrap", "SetCam", "(iO)", 0, Sf_Attr(&f, cp, "TgtCam01"));
	Sf_Method(&f, cp, "Del", "()");
	PyObject *cam = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0));
	PyObject *v = Sf_Entity(&f, Sf_Attr(&f, cam, "Target"));
	Sf_SetAttrI(&f, v, "UsrControl", 0);
	Sf_SetAttrI(&f, v, "MoveTime", 0);
	Sf_SetAttr(&f, v, "Vel", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Racer");
	Sf_Ask(&f, "RacerScorer", "Create", "(i)", 0);
	PyObject *solid = Sf_Ask(&f, "SScorer", "Get", "(iss)", 0, "Mission", "SolidTime");
	int show = Sf_Compare(&f, Sf_Time(&f), ">", Sf_Float(&f, 4.0));
	if (show > 0) {
		show = Sf_Compare(&f, solid, "==", Py_None);
		if (show == 0) {
			show = Sf_Compare(&f, solid, "!=", Py_None);
			if (show > 0) {
				show = Sf_Compare(&f, solid, "<=", Sf_Int(&f, 0));
			}
		}
	}
	if (show > 0) {
		Sf_Ask(&f, "SScorer", "SetLabelText", "(OO)", Sf_Attr(&f, v, "Descriptor"), Sf_OpI(&f, Sf_Time(&f), '+', 5));
	}
	Sf_SetGlobal(&f, PK, "LastCamera", Py_None);
	Sf_Own(&f, PK, "RestoreDeathPlayerShip", "()");
	if (Sf_True(&f, G("SetWeaponOnShip")) > 0) {
		Sf_SetGlobal(&f, PK, "SetWeaponOnShip", Sf_Int(&f, 0));
		Sf_SetAttr(&f, v, "AcWeap", Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "usrShip.AcWeap")));
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "usrShip.AcWeap", "6");
	}
	return Sf_None(&f);
}

/* UseShip(UsrName, CharName, optId, FullOption, BoxName="Parking"): the character boards the parked ship CharName:
 * the camera path to the ship, the pilot, the ship rises, a parking box where it stood (none for BoxName None) */
static PyObject *Parking_UseShip(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "UsrName", "CharName", "optId", "FullOption", "BoxName", NULL };
	PyObject *usrName, *charName, *optId, *full, *box = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOOO|O", names, &usrName, &charName, &optId, &full, &box)) {
		return NULL;
	}
	box = box ? Sf_Ref(&f, box) : Sf_Str(&f, "Parking");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Sputnik");
	Sf_Import(&f, "Init");
	PyObject *w = Sf_Entity(&f, usrName);
	PyObject *v = Sf_Entity(&f, charName);
	if (Sf_Compare(&f, Sf_Attr(&f, w, "UsrControl"), "!=", Sf_Int(&f, 0)) != 0) {
		return Sf_None(&f);
	}
	if (Sf_Compare(&f, Sf_Attr(&f, v, "MPil0"), "!=", G("P_None")) != 0) {
		return Sf_None(&f);
	}
	Sf_Own(&f, PK, "SetShipRadius", "(Oi)", v, 0);
	Pk_NoBlur(&f);
	/* the camera from the character to the ship */
	PyObject *lastCam = Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0);
	PyObject *cp = Sf_Ask(&f, "Scrap", "CreateEntity", "(siiis)", "CamPath", 0, 0, 0, "CamPath");
	Sf_SetAttrI(&f, cp, "nWayPoint", 2);
	Sf_SetAttr(&f, cp, "TgtCam01", lastCam);
	int boxed = Sf_Compare(&f, box, "!=", Py_None);
	Sf_Own(&f, PK, "SetCameraStart", "(OOOiO)", cp, v, Sf_Attr(&f, v, "Pos"), 1, boxed >= 0 ? Sf_Int(&f, boxed) : NULL);
	Sf_SetAttrS(&f, cp, "OnEnd", "Parking.CharOnShip");
	PyObject *lcam = Sf_Entity(&f, lastCam);
	Sf_SetAttr(&f, cp, "Time00", Sf_Float(&f, 1.0));
	Sf_SetAttrI(&f, cp, "FadeIn00", 1);
	Sf_SetAttrI(&f, cp, "FadeOut00", 0);
	Sf_Ask(&f, "Scrap", "SetCam", "(iO)", 0, Sf_Attr(&f, cp, "Name"));
	/* the exchange */
	PyObject *vPos = Sf_Attr(&f, v, "MovePos");
	Sf_SetAttr(&f, v, "MPil0", Sf_Cat(&f, "sO", "P_", Sf_Attr(&f, w, "ActCtrl")));
	Sf_Ask(&f, "ShipMaker", "SetUpChars", "(O)", Sf_Attr(&f, v, "Name"));
	Sf_SetAttrI(&f, v, "ArrowCanUse", 0);
	Sf_SetAttrS(&f, v, "UseFunc", "");
	int mark = f.n;
	Sf_SetAttr(&f, v, "MovePos", Sf_Ask(&f, "SVec", "Add", "(O(iOi))", Sf_Attr(&f, v, "Pos"), 0, G("ParkingDepth"), 0));
	Sf_SetAttr(&f, v, "MoveAng", Sf_Attr(&f, v, "Ang"));
	Sf_SetAttr(&f, v, "MoveTime", Sf_Float(&f, 0.9));
	Sf_SetAttr(&f, v, "TgType", Sf_Attr(&f, w, "TgType"));
	Sf_SetAttrS(&f, v, "EntityList", "Players");
	Sf_SetAttrI(&f, v, "UsrControl", 0);
	Sf_Own(&f, PK, "SetInvunerable", "(O)", v);
	Sf_Release(&f, mark);
	if (Sf_Compare(&f, box, "!=", Py_None) > 0) {
		Sf_SetGlobal(&f, PK, "LastPark", Sf_Own(&f, PK, "CreateBox", "(OOOO)", box, Sf_Item(&f, vPos, 0),
			Sf_Item(&f, vPos, 1), Sf_Item(&f, vPos, 2)));
		Sf_SetAttr(&f, G("LastPark"), "rTime", Sf_Float(&f, 0.5));
	}
	PyObject *vName = Sf_Attr(&f, v, "Name");
	Sf_SetAttr(&f, lcam, "MainTarget", vName);
	Sf_SetAttr(&f, lcam, "Target", vName);
	Sf_SetAttr(&f, lcam, "CamDist", Sf_Ask(&f, "Scrap", "Get", "(s)", "VehicleCamDist"));
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	Sf_Ask(&f, "Menu", "MovieScorer", "(i)", 0);
	Sf_SetAttrI(&f, w, "AutoSelect", 0);
	Sf_Method(&f, w, "Del", "()");
	Sf_Schedule(&f, Sf_Time(&f), G("ExecSwitchVehicleMissionCallback"), Sf_Build(&f, "()"), NULL);
	/* Sputnik says goodbye */
	Sf_Ask(&f, "Sputnik", "SetSptAt", "(OOs)", Sf_Attr(&f, v, "MovePos"), Sf_Attr(&f, v, "Ang"), "Bye");
	PyObject *arrows = Sf_Mod(&f, "Init", "Arrows");
	Sf_Ask(&f, "Scrap", "SwitchMissionArrows", "(OO)", Sf_Item(&f, arrows, 3), Sf_Item(&f, arrows, 4));
	Pk_Voice(&f, "OnTakeOffSound");
	return Sf_None(&f);
}

/* PlayVoiceChannel(channel): plays the voice and stops it when it ends */
static PyObject *Parking_PlayVoiceChannel(PyObject *self, PyObject *args) {
	PyObject *channel;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &channel)) {
		return NULL;
	}
	Sf_Ask(&f, "SSound", "PlayVoice", "(O)", channel);
	PyObject *timy = Sf_Ask(&f, "SSound", "VoiceRemain", "(Od)", channel, 0.1);
	PyObject *at = Sf_Op(&f, Sf_Op(&f, Sf_Time(&f), '+', timy), '+', Sf_Float(&f, 0.1));
	Sf_Schedule(&f, at, Sf_Mod(&f, "SSound", "StopVoice"), Sf_Build(&f, "(O)", channel), NULL);
	return Sf_None(&f);
}

/* ExecSwitchVehicleMissionCallback(): the mission callbacks of boarding */
static PyObject *Parking_ExecSwitchVehicleMissionCallback(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Pk_Callback(&f, "SwitchVehicleMissionCallback");
	Pk_Callback(&f, "SwitchVehicleMissionCallbackAux");
	return Sf_None(&f);
}

/* EndPathCam(PathCam): the end of the parking camera: the character under control, the walk HUD */
static PyObject *Parking_EndPathCam(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "CharScorer");
	PyObject *cp = Sf_Entity(&f, path);
	Sf_Ask(&f, "Scrap", "SetCam", "(iO)", 0, Sf_Attr(&f, cp, "TgtCam03"));
	Sf_Method(&f, cp, "Del", "()");
	PyObject *lcam = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0));
	PyObject *v = Sf_Entity(&f, Sf_Attr(&f, lcam, "Target"));
	Sf_SetAttrI(&f, v, "UsrControl", 0);
	Pk_Callback(&f, "SwitchCharMissionCallback");
	Pk_Callback(&f, "SwitchCharMissionCallbackAux");
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Walk");
	Sf_Ask(&f, "CharScorer", "Create", "(i)", 0);
	Sf_SetGlobal(&f, PK, "LastCamera", Py_None);
	Sf_Own(&f, PK, "RestoreDeathPlayerShip", "()");
	return Sf_None(&f);
}

/* SetCameraStart(cp, v, iPos, split=0, testline=1): the camera point beside the ship at the height of the ground
 * (point 0 for split, else point 1) */
static PyObject *Parking_SetCameraStart(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "cp", "v", "iPos", "split", "testline", NULL };
	static const char *const vars[2][6] = { { "Pos00", "Ang00", "FOV00", "Near00", "Far00", "Quake00" },
		{ "Pos01", "Ang01", "FOV01", "Near01", "Far01", "Quake01" } };
	PyObject *cp, *v, *iPos, *split = NULL, *testline = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOO|OO", names, &cp, &v, &iPos, &split, &testline)) {
		return NULL;
	}
	split = split ? Sf_Ref(&f, split) : Sf_Int(&f, 0);
	testline = testline ? Sf_Ref(&f, testline) : Sf_Int(&f, 1);
	PyObject *ang1 = Sf_Item(&f, Sf_Attr(&f, v, "Ang"), 1);
	PyObject *rot = Sf_Ask(&f, "SVec", "Rotate3D", "((iiO)(iOi))", 0, 0, Sf_OpI(&f, Sf_Attr(&f, v, "WRadius"), '+', 50), 0,
		ang1, 0);
	PyObject *acPos = Sf_Ask(&f, "SVec", "Add", "(OO)", iPos, rot);
	PyObject *newPos = acPos;
	if (Sf_True(&f, testline) > 0) {
		PyObject *res = Pk_Down(&f, acPos);
		if (Sf_True(&f, res) > 0) {
			newPos = Sf_Item(&f, res, 0);
		}
	}
	PyObject *y = Sf_OpI(&f, Sf_Op(&f, Sf_Item(&f, newPos, 1), '+', Sf_Attr(&f, v, "WRadius")), '+', 50);
	PyObject *pos = Sf_Build(&f, "(OOO)", Sf_Item(&f, newPos, 0), y, Sf_Item(&f, newPos, 2));
	PyObject *ang = Sf_Build(&f, "(iOi)", 0, Sf_OpI(&f, Sf_Item(&f, Sf_Attr(&f, v, "Ang"), 1), '+', 180), 0);
	Sf_SetAttr(&f, cp, "Time00", Sf_Float(&f, 2.5));
	Sf_SetAttrI(&f, cp, "FadeIn00", 1);
	Sf_SetAttrI(&f, cp, "FadeOut00", 1);
	int t = Sf_True(&f, split);
	if (t >= 0) {
		const char *const *n = vars[t ? 0 : 1];
		Sf_SetAttr(&f, cp, n[0], pos);
		Sf_SetAttr(&f, cp, n[1], ang);
		Sf_SetAttrI(&f, cp, n[2], 80);
		Sf_SetAttrI(&f, cp, n[3], 50);
		Sf_SetAttrI(&f, cp, n[4], 500000);
		Sf_SetAttr(&f, cp, n[5], Sf_Float(&f, 0.5));
	}
	Sf_SetAttr(&f, cp, "camPos", Sf_Attr(&f, cp, "Pos00"));
	Sf_SetAttr(&f, cp, "camAng", Sf_Attr(&f, cp, "Ang00"));
	return Sf_None(&f);
}

/* isValidToPark(v): v and v.IsType("Car") and (v.UsrControl == 0), the value of the and */
static PyObject *Parking_isValidToPark(PyObject *self, PyObject *args) {
	PyObject *v;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	if (Sf_True(&f, v) <= 0) {
		return Sf_Return(&f, v);
	}
	PyObject *car = Sf_Method(&f, v, "IsType", "(s)", "Car");
	if (Sf_True(&f, car) <= 0) {
		return Sf_Return(&f, car);
	}
	int eq = Sf_EqI(&f, Sf_Attr(&f, v, "UsrControl"), 0);
	return Sf_Return(&f, eq >= 0 ? Sf_Int(&f, eq) : NULL);
}

/* OnPark(ItemName, VehicleName): a ship slow enough on a parking box parks there: the pilot gets out, the camera
 * path shows it; 1 when it parked */
static PyObject *Parking_OnPark(PyObject *self, PyObject *args) {
	PyObject *itemName, *vehName;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &itemName, &vehName)) {
		return NULL;
	}
	Sf_Import(&f, "Chars");
	PyObject *v = Sf_Entity(&f, vehName);
	if (Sf_True(&f, Sf_Own(&f, PK, "isValidToPark", "(O)", v)) == 0) {
		return Sf_Return(&f, Sf_Int(&f, 0));
	}
	if (Sf_Compare(&f, Sf_Attr(&f, v, "Speed"), ">", G("MinParkingVel")) > 0) {
		Sf_SetAttr(&f, v, "Vel", Sf_Ask(&f, "SVec", "Prod", "(Od)", Sf_Attr(&f, v, "Vel"), 0.6));
		return Sf_Return(&f, Sf_Int(&f, 0));
	}
	PyObject *pilot = Sf_Attr(&f, v, "MPil0");
	int bad = Sf_Compare(&f, Sf_Slice(&f, pilot, 0, 2), "!=", Sf_Str(&f, "P_"));
	if (bad == 0) {
		bad = Sf_EqS(&f, Sf_Slice(&f, pilot, 2, 0x7fffffff), "None");
	}
	if (bad > 0) {
		return Sf_Return(&f, Sf_Int(&f, 0));
	}
	Sf_Attr(&f, v, "UsrControl");
	PyObject *descriptor = Sf_Attr(&f, v, "Descriptor");
	PyObject *i = Sf_Entity(&f, itemName);
	Sf_SetAttr(&f, i, "rTime", Sf_Float(&f, 0.1));
	Sf_Method(&f, i, "Del", "()");
	PyObject *iPos = Sf_Attr(&f, i, "Pos");
	/* the ship stops and parks */
	Sf_SetAttrI(&f, v, "UsrControl", -1);
	Sf_SetAttr(&f, v, "Vel", Sf_Build(&f, "(iii)", 0, 0, 0));
	PyObject *newPos = Sf_Item(&f, Pk_Down(&f, iPos), 0);
	PyObject *vAng = Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, Sf_Attr(&f, v, "Ang"), 1), 0);
	int mark = f.n;
	Sf_SetAttr(&f, v, "MovePos", Sf_Build(&f, "(OOO)", Sf_Item(&f, newPos, 0),
		Sf_Op(&f, Sf_Item(&f, newPos, 1), '+', Sf_Attr(&f, v, "WRadius")), Sf_Item(&f, newPos, 2)));
	Sf_SetAttr(&f, v, "MoveAng", vAng);
	Sf_SetAttr(&f, v, "MoveTime", Sf_Float(&f, 2.5));
	Sf_SetAttrI(&f, v, "ArrowCanUse", 1);
	Sf_SetAttrI(&f, v, "ArrnOpt", 0);
	Sf_SetAttr(&f, v, "ArrOpt0", Sf_Cat(&f, "sO", "UseShip:", Sf_Lang(&f, Sf_Str(&f, "UseShip"))));
	Sf_SetAttrS(&f, v, "UseFunc", "Sputnik.UseShip");
	Sf_Own(&f, PK, "SetInvunerable", "(O)", v);
	Pk_NoBlur(&f);
	Sf_Release(&f, mark);
	/* the camera of the change */
	PyObject *lastCam = Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0);
	PyObject *cp = Sf_Ask(&f, "Scrap", "CreateEntity", "(siiis)", "CamPath", 0, 0, 0, "CamPath");
	Sf_SetGlobal(&f, PK, "LastCamera", cp);
	mark = f.n;
	Sf_SetAttrI(&f, cp, "nWayPoint", 4);
	Sf_SetAttrS(&f, cp, "OnEnd", "Parking.EndPathCam");
	Sf_SetAttr(&f, cp, "TgtCam00", lastCam);
	Sf_SetAttrS(&f, cp, "TgtCam00", "");
	Sf_Own(&f, PK, "SetCameraStart", "(OOO)", cp, v, iPos);
	PyObject *a90 = Sf_OpI(&f, Sf_Item(&f, vAng, 1), '+', 90);
	PyObject *rot = Sf_Ask(&f, "SVec", "Rotate3D", "((iiO)(iOi))", 0, 0, Pk_Neg(&f, G("ParkingDist")), 0, a90, 0);
	Sf_SetAttr(&f, cp, "Pos02", Sf_Ask(&f, "SVec", "Add", "(OO)", iPos, rot));
	Sf_SetAttr(&f, cp, "Ang02", Sf_Build(&f, "(iOi)", 0, Sf_OpI(&f, Sf_Item(&f, vAng, 1), '+', 90), 0));
	Sf_SetAttrI(&f, cp, "FOV02", 70);
	Sf_SetAttrI(&f, cp, "Near02", 50);
	Sf_SetAttrI(&f, cp, "Far02", 500000);
	Sf_SetAttr(&f, cp, "Time01", Sf_Float(&f, 0.0));
	Sf_SetAttr(&f, cp, "TgtCam03", lastCam);
	Sf_SetAttr(&f, cp, "Time02", Sf_Float(&f, 1.0));
	Sf_SetAttrI(&f, cp, "FadeIn02", 0);
	Sf_SetAttrI(&f, cp, "FadeOut02", 1);
	Sf_SetAttr(&f, cp, "Time", Sf_Float(&f, 0.0));
	Sf_Release(&f, mark);
	/* the character of the player */
	vAng = Sf_Build(&f, "(iOi)", 0, Sf_OpI(&f, Sf_Item(&f, vAng, 1), '+', 90), 0);
	PyObject *c = Sf_Ask(&f, "Chars", "CreateMainChar", "(sOO(iOi))", "Player0", Sf_Slice(&f, pilot, 2, 0x7fffffff),
		Sf_Attr(&f, v, "Pos"), 0, Sf_OpI(&f, Sf_Item(&f, Sf_Attr(&f, v, "Ang"), 1), '-', 90), 0);
	rot = Sf_Ask(&f, "SVec", "Rotate3D", "((iiO)O)", 0, 0, Pk_Neg(&f, G("ParkingDist")), vAng);
	iPos = Sf_Ask(&f, "SVec", "Add", "(OO)", iPos, rot);
	newPos = Sf_Item(&f, Pk_Down(&f, iPos), 0);
	mark = f.n;
	Sf_SetAttr(&f, c, "Pos", Sf_Build(&f, "(OOO)", Sf_Item(&f, newPos, 0),
		Sf_Op(&f, Sf_Item(&f, newPos, 1), '+', Sf_Attr(&f, c, "FloorDist")), Sf_Item(&f, newPos, 2)));
	Sf_SetAttr(&f, c, "Descriptor", descriptor);
	Sf_SetAttrI(&f, c, "Active", 0);
	Sf_SetAttrI(&f, c, "UsrControl", -1);
	Sf_SetAttr(&f, c, "TgType", Sf_Attr(&f, v, "TgType"));
	Sf_SetAttrS(&f, v, "EntityList", "");
	Sf_SetAttrS(&f, v, "TgType", "None");
	Sf_Entity(&f, lastCam);
	Sf_Ask(&f, "Scrap", "SetCam", "(iO)", 0, Sf_Attr(&f, cp, "Name"));
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	Sf_Import(&f, "Menu");
	Sf_Ask(&f, "Menu", "MovieScorer", "(i)", 0);
	Sf_Release(&f, mark);
	Sf_Later(&f, Sf_Float(&f, 2.5), G("ShowChar"), Sf_Build(&f, "(OO)", Sf_Attr(&f, c, "Name"), Sf_Attr(&f, v, "Name")),
		NULL);
	Sf_Own(&f, PK, "SetShipRadius", "(Oi)", v, 1);
	Sf_Import(&f, "Sputnik");
	Sf_Ask(&f, "Sputnik", "SetSptAt", "(OOs)", Sf_Attr(&f, v, "Pos"), Sf_Attr(&f, v, "Ang"), "Hello");
	Pk_Voice(&f, "OnLandSound");
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* SetDtritusPlate(v, Active): the plate shows on the player's ship (single player) */
static PyObject *Parking_SetDtritusPlate(PyObject *self, PyObject *args) {
	PyObject *v, *active;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &v, &active)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	int t = Sf_True(&f, Sf_Mod(&f, "Init", "MultiPlayer"));
	t = t < 0 ? -1 : !t;
	if (t > 0) {
		t = Sf_True(&f, G("DtritusPlate"));
	}
	if (t > 0) {
		t = Sf_True(&f, G("PlayerShip"));
	}
	if (t > 0) {
		t = Sf_True(&f, v);
	}
	if (t > 0) {
		t = Sf_Compare(&f, Sf_Attr(&f, G("PlayerShip"), "Name"), "==", Sf_Attr(&f, v, "Name"));
	}
	if (t > 0) {
		Sf_SetAttr(&f, G("DtritusPlate"), "Active", active);
	}
	return Sf_None(&f);
}

/* CreateDtritusPlate(): the plate of Dtritus, hidden until it is linked to the player's ship */
static PyObject *Parking_CreateDtritusPlate(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetGlobal(&f, PK, "DtritusPlate", Sf_Ask(&f, "Scrap", "CreateEntity", "(siiis)", "DtritusPlate", 0, 0, 0,
		"WithLife"));
	Sf_Method(&f, G("DtritusPlate"), "SetModel", "(O)", G("DtritusPlateModel"));
	Sf_SetAttrI(&f, G("DtritusPlate"), "Active", 0);
	Sf_SetAttr(&f, G("DtritusPlate"), "OffModPos", Sf_Build(&f, "(iii)", 0, 0, 0));
	Sf_SetAttrI(&f, G("DtritusPlate"), "DeltaOffMul", 4);
	Sf_SetAttrI(&f, G("DtritusPlate"), "OffMul", 0);
	Sf_SetAttrI(&f, G("DtritusPlate"), "UseLinkAng", 1);
	return Sf_None(&f);
}

/* CreatePlayerShip(Pos): the ship of the hangar the save game flies (the aux hangar for 9) at Pos, with the plate */
static PyObject *Parking_CreatePlayerShip(PyObject *self, PyObject *args) {
	PyObject *pos;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &pos)) {
		return NULL;
	}
	Sf_Import(&f, "ShipEdit");
	if (Sf_Compare(&f, G("PlayerShip"), "!=", Py_None) > 0) {
		Sf_Method(&f, G("PlayerShip"), "Del", "()");
		Sf_SetGlobal(&f, PK, "PlayerShip", Py_None);
	}
	PyObject *hangar = Sf_Eval(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.iHangarShip"), PK);
	PyObject *var;
	int aux = Sf_EqI(&f, hangar, 9);
	if (aux > 0) {
		var = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.HangarShipAux");
	} else {
		var = Sf_Ask(&f, "Scrap", "GetSaveVar", "(O)", Sf_Cat(&f, "sO", "Hangar.HangarShip", Sf_Repr(&f,
			Sf_OpI(&f, hangar, '+', 1))));
	}
	Sf_SetGlobal(&f, PK, "PlayerShip", Sf_Ask(&f, "ShipEdit", "CreateShipFromSaveVar", "(OOs)", var, pos, "PlayerShip"));
	Sf_SetAttrI(&f, G("PlayerShip"), "Active", 1);
	Sf_SetAttrI(&f, G("PlayerShip"), "ActiveCMask", 1);
	Sf_SetAttrI(&f, G("DtritusPlate"), "Active", 1);
	Sf_SetAttrS(&f, G("DtritusPlate"), "LinkModel", ",");
	Sf_SetAttr(&f, G("DtritusPlate"), "LinkModel", Sf_Cat(&f, "OsO", Sf_Attr(&f, G("PlayerShip"), "Name"), ",",
		G("DtritusPlateNode")));
	if (Sf_True(&f, G("OnCreatePlayerShip")) > 0) {
		Sf_Call(&f, G("OnCreatePlayerShip"), "(O)", G("PlayerShip"));
	}
	return Sf_None(&f);
}

/* ShowChar(charname, shipname): the character appears beside the parked ship, Sputnik at the player's ship (moved to
 * the nearest parking box when the player parked another ship) */
static PyObject *Parking_ShowChar(PyObject *self, PyObject *args) {
	PyObject *charName, *shipName;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &charName, &shipName)) {
		return NULL;
	}
	Sf_Import(&f, "Sputnik");
	Sf_Import(&f, "Init");
	Sf_SetAttrI(&f, Sf_Entity(&f, charName), "Active", 1);
	PyObject *v = Sf_Entity(&f, shipName);
	if (Sf_True(&f, v) > 0 && Sf_True(&f, Sf_Attr(&f, v, "Active")) > 0) {
		Sf_SetAttr(&f, v, "MPil0", G("P_None"));
	}
	PyObject *arrows = Sf_Mod(&f, "Init", "Arrows");
	Sf_Ask(&f, "Scrap", "SwitchMissionArrows", "(OO)", Sf_Item(&f, arrows, 1), Sf_Item(&f, arrows, 2));
	int own = Sf_Compare(&f, Sf_Attr(&f, v, "Name"), "==", Sf_Attr(&f, G("PlayerShip"), "Name"));
	if (own > 0) {
		Sf_Ask(&f, "Sputnik", "Assign", "(O)", v);
	} else if (own == 0) {
		PyObject *near = Sf_Ask(&f, "Scrap", "EntityListGet", "(OO)", G("PR_Lst"), Sf_Cat(&f, "sO", "Near",
			Sf_Attr(&f, v, "Name")));
		PyObject *nearest = Sf_Entity(&f, near);
		if (Sf_True(&f, nearest) > 0) {
			Sf_SetAttr(&f, G("PlayerShip"), "Pos", Sf_Attr(&f, nearest, "Pos"));
			Sf_SetAttr(&f, G("PlayerShip"), "Ang", Sf_Build(&f, "(iii)", 0, 0, 0));
			Sf_Ask(&f, "Sputnik", "Assign", "(O)", nearest);
			Sf_Method(&f, nearest, "Del", "()");
		}
	}
	return Sf_None(&f);
}

/* EndJunkerPathCam(PathCam): parked in the junkyard: the level of Rusty's junkyard starts */
static PyObject *Parking_EndJunkerPathCam(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Own(&f, PK, "EndPathCam", "(O)", path);
	Sf_Import(&f, "SaveGame");
	Sf_Import(&f, "Init");
	PyObject *spawn = Sf_Cat(&f, "sO", "DM_Player_Spawn_", Sf_Slice(&f, Sf_Mod(&f, "Init", "Path"), 7, 0x7fffffff));
	Sf_Ask(&f, "SaveGame", "MarkWayPoint", "(sOO)", "Levels/Junkyard", spawn, Sf_Attr(&f, Sf_Ask(&f, "Scrap",
		"UsrEntity", "(i)", 0), "ActCtrl"));
	Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "DoorEditShip");
	Pk_StartGame(&f);
	return Sf_None(&f);
}

/* JunkerPark(ItemName, VehicleName): the junkyard parking; a stolen ship is recycled for a quarter of its price */
static PyObject *Parking_JunkerPark(PyObject *self, PyObject *args) {
	PyObject *itemName, *vehName;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &itemName, &vehName)) {
		return NULL;
	}
	Sf_Import(&f, "ShipMaker");
	PyObject *v = Sf_Entity(&f, vehName);
	if (Sf_True(&f, Sf_Own(&f, PK, "isValidToPark", "(O)", v)) == 0) {
		return Sf_None(&f);
	}
	Sf_Import(&f, "Speech");
	if (Sf_True(&f, Sf_Item(&f, Sf_Ask(&f, "Scrap", "GetAlarm", "()"), 0)) > 0) {
		Sf_Ask(&f, "Scrap", "SetAlarm", "(i)", 0);
	}
	PyObject *price = NULL;
	int stolen = Sf_Compare(&f, Sf_Attr(&f, v, "Name"), "!=", Sf_Attr(&f, G("PlayerShip"), "Name"));
	if (stolen > 0) {
		PyObject *cost = Sf_Ask(&f, "ShipMaker", "GetShipPrice", "(O)", Sf_Attr(&f, v, "Model"));
		price = Sf_OpI(&f, Sf_ToInt(&f, cost), '/', 4);
	} else if (stolen == 0) {
		price = Sf_Int(&f, 0);
	}
	PyObject *res = Sf_Own(&f, PK, "OnPark", "(OO)", itemName, vehName);
	int t = Sf_True(&f, res);
	if (t > 0) {
		t = Sf_True(&f, price);
	}
	if (t > 0) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(ss)", "Hangar.iHangarShip", "-1");
		Sf_Ask(&f, "Scrap", "SetMoney", "(O)", Sf_Op(&f, Sf_Ask(&f, "Scrap", "GetMoney", "()"), '+', price));
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "InitialMessage", Sf_Op(&f, Sf_Lang(&f, Sf_Str(&f,
			"OnRecicledAtRusty")), '%', price));
	}
	if (Sf_True(&f, G("LastCamera")) > 0) {
		Sf_SetAttrS(&f, G("LastCamera"), "OnEnd", "Parking.EndJunkerPathCam");
	}
	return Sf_None(&f);
}

/* OnParkingOccupied(EntityNane, x, y, z, name): the engine: a ship stands on a parking place; a parked one gives way
 * to a parking box */
static PyObject *Parking_OnParkingOccupied(PyObject *self, PyObject *args) {
	PyObject *ent, *x, *y, *z, *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOOO", &ent, &x, &y, &z, &name)) {
		return NULL;
	}
	PyObject *e = Sf_Entity(&f, ent);
	if (Sf_Compare(&f, Sf_Attr(&f, e, "MPil0"), "==", G("P_None")) > 0) {
		int own = Sf_Compare(&f, ent, "==", Sf_Attr(&f, G("PlayerShip"), "Name"));
		if (own > 0) {
			Sf_SetAttrI(&f, Sf_Entity(&f, ent), "Active", 0);
		} else if (own == 0) {
			Sf_Method(&f, Sf_Entity(&f, ent), "Del", "()");
		}
		PyObject *l = Sf_Own(&f, PK, "CreateBox", "(sOOO)", "Parking", x, y, z);
		Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.1));
		Sf_Print(&f, "sOsO", "Parking Occupied by", ent, " replaced by ", Sf_Attr(&f, l, "Name"));
	}
	return Sf_None(&f);
}

/* OnParkedOut(EntityNane, x, y, z, name): the engine: a parking place is free again; a new parked ship there (unless
 * ParkedOutCallback made one) */
static PyObject *Parking_OnParkedOut(PyObject *self, PyObject *args) {
	PyObject *ent, *x, *y, *z, *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOOO", &ent, &x, &y, &z, &name)) {
		return NULL;
	}
	Sf_Method(&f, Sf_Entity(&f, ent), "Del", "()");
	if (Sf_True(&f, G("ParkedOutCallback")) > 0) {
		if (Sf_True(&f, Sf_Call(&f, G("ParkedOutCallback"), "((OOO)O)", x, y, z, name)) > 0) {
			return Sf_None(&f);
		}
	}
	PyObject *angle = f.failed ? NULL : Sf_Float(&f, Scrap_RandRange(0, 360));
	PyObject *l = Sf_Own(&f, PK, "CreateParkedShip", "((OOO)OO)", x, y, z, angle, name);
	Sf_Print(&f, "sOsO", "Parking free by ", ent, " filled by ", Sf_Attr(&f, l, "Name"));
	return Sf_None(&f);
}

/* CreateBox(Name, x, y, z): a parking box ParkingDist over the ground below the point (None and an error on the
 * console when there is no ground) */
static PyObject *Parking_CreateBox(PyObject *self, PyObject *args) {
	PyObject *name, *x, *y, *z;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &name, &x, &y, &z)) {
		return NULL;
	}
	PyObject *iPos = Sf_Build(&f, "(OOO)", x, y, z);
	PyObject *res = Pk_Down(&f, iPos);
	if (Sf_True(&f, res) == 0) {
		Sf_Ask(&f, "Scrap", "ConsoleError", "(O)", Sf_Cat(&f, "sOs", "A T R O C I O U S    E R R O R : Parking box ", name,
			" was created in an invalid position.\n"));
		return Sf_None(&f);
	}
	PyObject *p = Sf_Item(&f, res, 0);
	PyObject *py = Sf_Op(&f, Sf_Item(&f, p, 1), '+', G("ParkingDist"));
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", name, Sf_Item(&f, p, 0), py, Sf_Item(&f, p, 2), "Item");
	Sf_Method(&f, l, "SetModel", "(s)", "models/Misc/Parking/Parking.m3d");
	Sf_SetAttrI(&f, l, "Radius", 1000);
	Sf_SetAttrI(&f, l, "AppearsIfFree", 1);
	Sf_SetAttrI(&f, l, "AutoFace", 1);
	Sf_SetAttrS(&f, l, "OnTake", "Parking.OnPark");
	Sf_SetAttrI(&f, l, "OnlyUser", 1);
	Sf_SetAttr(&f, l, "EntityList", G("PR_Lst"));
	return Sf_Return(&f, l);
}

/* SetShipRadius(v, parked): a parked ship takes the smaller of its two radii for the world */
static PyObject *Parking_SetShipRadius(PyObject *self, PyObject *args) {
	PyObject *v, *parked;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &v, &parked)) {
		return NULL;
	}
	PyObject *wr = Sf_Attr(&f, v, "WRadius");
	PyObject *vr = Sf_Attr(&f, v, "VRadius");
	int t = Sf_True(&f, parked);
	int swap = t > 0 ? Sf_Compare(&f, wr, ">", vr) : t;
	if (swap == 0) {
		t = Sf_True(&f, parked);
		swap = t == 0 ? Sf_Compare(&f, wr, "<", vr) : t < 0 ? -1 : 0;
	}
	if (swap > 0) {
		Sf_SetAttr(&f, v, "WRadius", vr);
		Sf_SetAttr(&f, v, "VRadius", wr);
	}
	return Sf_None(&f);
}

/* SetOnFloor(v) */
static PyObject *Parking_SetOnFloor(PyObject *self, PyObject *args) {
	PyObject *v;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, v, "Name"));
	return Sf_None(&f);
}

/* SetShipAsParked(ShipName): no pilot, the use arrow, on the ground */
static PyObject *Parking_SetShipAsParked(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *v = Sf_Entity(&f, name);
	Sf_SetAttr(&f, v, "MPil0", G("P_None"));
	Sf_SetAttrI(&f, v, "ArrowCanUse", 1);
	Sf_SetAttrI(&f, v, "ArrnOpt", 0);
	Sf_SetAttr(&f, v, "ArrOpt0", Sf_Cat(&f, "sO", "UseShip:", Sf_Lang(&f, Sf_Str(&f, "UseShip"))));
	Sf_SetAttrS(&f, v, "UseFunc", "Sputnik.UseShip");
	Sf_SetAttrI(&f, v, "UsrControl", -1);
	Sf_SetAttrS(&f, v, "TgType", "None");
	Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, v, "Name"));
	Sf_SetAttr(&f, v, "MovePos", Sf_Attr(&f, v, "Pos"));
	Sf_SetAttr(&f, v, "MoveAng", Sf_Attr(&f, v, "Ang"));
	Sf_SetAttr(&f, v, "MoveTime", Sf_Float(&f, 0.001));
	Sf_Own(&f, PK, "SetShipRadius", "(Oi)", v, 1);
	return Sf_None(&f);
}

/* ParkedDeat(v, attacker): a parked ship explodes: a parking box in its place (the player who did it may set off the
 * alarm), or Sputnik stays where the player's ship was; 1 for a parked ship */
static PyObject *Parking_ParkedDeat(PyObject *self, PyObject *args) {
	PyObject *v, *attacker;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &v, &attacker)) {
		return NULL;
	}
	Sf_Import(&f, "OutPolice");
	if (Sf_EqI(&f, Sf_Attr(&f, v, "ArrowCanUse"), 1) <= 0) {
		return Sf_Return(&f, f.failed ? NULL : Sf_Int(&f, 0));
	}
	int other = Sf_Compare(&f, Sf_Attr(&f, G("PlayerShip"), "Name"), "!=", Sf_Attr(&f, v, "Name"));
	if (other > 0) {
		Sf_Ask(&f, "Scrap", "IncSaveVar", "(s)", "Stats.Parking");
		PyObject *vPos = Sf_Attr(&f, v, "MovePos");
		PyObject *l = Sf_Own(&f, PK, "CreateBox", "(sOOO)", "Parking", Sf_Item(&f, vPos, 0), Sf_Item(&f, vPos, 1),
			Sf_Item(&f, vPos, 2));
		Sf_SetAttr(&f, l, "rTime", Sf_Float(&f, 0.1));
		Sf_Method(&f, v, "Del", "()");
		PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
		int by = Sf_True(&f, usr);
		if (by > 0) {
			by = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", attacker);
		}
		if (by > 0) {
			int alarm = Sf_True(&f, Sf_Mod(&f, "OutPolice", "PoliceActive"));
			if (alarm > 0) {
				alarm = Sf_Compare(&f, Sf_Float(&f, Scrap_RandRange(0.0, 1.0)), "<", G("AlarmProbOnParkedShipDeath"));
			}
			if (alarm > 0) {
				Sf_Import(&f, "Speech");
				Sf_Ask(&f, "Speech", "AddRemoteCom", "(ss)", "Generics_PoliceTaunt_16", "Police");
				Sf_Ask(&f, "Scrap", "SetAlarm", "(i)", 1);
			}
		}
	} else if (other == 0) {
		Sf_Import(&f, "Sputnik");
		Sf_Import(&f, "Chars");
		Sf_SetAttr(&f, Sf_Mod(&f, "Sputnik", "spt"), "Pos", Sf_Attr(&f, v, "Pos"));
		Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, Sf_Mod(&f, "Sputnik", "spt"), "Name"));
		Sf_SetAttr(&f, Sf_Mod(&f, "Sputnik", "spt"), "MovePos", Sf_Attr(&f, Sf_Mod(&f, "Sputnik", "spt"), "Pos"));
		Sf_SetAttr(&f, Sf_Mod(&f, "Sputnik", "spt"), "MoveAng", Sf_Attr(&f, Sf_Mod(&f, "Sputnik", "spt"), "Ang"));
		Sf_SetAttr(&f, Sf_Mod(&f, "Sputnik", "spt"), "MoveTime", Sf_Float(&f, 0.0001));
	}
	return Sf_Return(&f, Sf_Int(&f, 1));
}

/* BrowseItemPos(): the parking boxes of the DM_Parking dummies; the player's ship on the one nearest to the player
 * (the junkyard's only when the player comes from there), Sputnik beside it */
static PyObject *Parking_BrowseItemPos(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_Parking*", 1);
	PyObject *myPos = Sf_Attr(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "Pos");
	PyObject *nearest = Sf_Ref(&f, Py_None);
	PyObject *nearestDist = Sf_Float(&f, 10000000.0);
	int fromJY = Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Spawn"), "==", G("PL_Junk"));
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "!=", Py_None) <= 0) {
			break;
		}
		if (Sf_True(&f, Sf_Item(&f, dummy, 3)) > 0) {
			PyObject *dist = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", myPos, Sf_Item(&f, dummy, 1)));
			int closer = Sf_Compare(&f, nearestDist, ">", dist);
			if (closer > 0) {
				closer = fromJY < 0 ? -1 : fromJY ? 1 : Sf_Compare(&f, Sf_Item(&f, dummy, 0), "!=", G("PR_Junk"));
			}
			if (closer > 0) {
				PyObject *aux = nearest;
				nearest = dummy;
				dummy = aux;
				nearestDist = dist;
			}
			if (Sf_True(&f, dummy) > 0) {
				PyObject *dname = Sf_Item(&f, dummy, 0);
				PyObject *dpos = Sf_Item(&f, dummy, 1);
				PyObject *l = Sf_Own(&f, PK, "CreateBox", "(OOOO)", Sf_Cat(&f, "sO", "Park", Sf_Slice(&f, dname, 3,
					0x7fffffff)), Sf_Item(&f, dpos, 0), Sf_Item(&f, dpos, 1), Sf_Item(&f, dpos, 2));
				Sf_Ask(&f, "Scrap", "AddParkingZone", "(iOO)", 0, Sf_Attr(&f, l, "Pos"), Sf_Slice(&f, dname, 10, 0x7fffffff));
				if (Sf_Compare(&f, dname, "==", G("PR_Junk")) > 0) {
					Sf_SetAttrS(&f, l, "OnTake", "Parking.JunkerPark");
				}
			}
		}
		PyObject *keep[2] = { nearest, nearestDist };
		Sf_ReleaseKeep(&f, mark, keep, 2);
	}
	if (Sf_True(&f, nearest) > 0) {
		Sf_Import(&f, "ShipEdit");
		Sf_SetMod(&f, "ShipEdit", "editShipPosDummy", nearest);
		Sf_Own(&f, PK, "CreatePlayerShip", "(O)", Sf_Item(&f, nearest, 1));
		PyObject *o1 = G("PlayerShip");
		Sf_SetAttr(&f, o1, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, Sf_Item(&f, nearest, 2), 1), 0));
		Sf_Own(&f, PK, "SetShipAsParked", "(O)", Sf_Attr(&f, o1, "Name"));
		Sf_Ask(&f, "Scrap", "AddParkingZone", "(iOO)", 0, Sf_Attr(&f, o1, "Pos"), Sf_Slice(&f, Sf_Item(&f, nearest, 0), 10,
			0x7fffffff));
		Sf_Import(&f, "Sputnik");
		Sf_Ask(&f, "Sputnik", "Assign", "(O)", G("PlayerShip"));
	}
	return Sf_None(&f);
}

/* CreateLonelyMercParkings(): the parking boxes of the lonely mercenary mission, their names */
static PyObject *Parking_CreateLonelyMercParkings(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *parkings = Sf_Keep(&f, PyList_New(0));
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_LonelyMerc_Parking*", 1);
	PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
	while (Sf_True(&f, dummy) > 0) {
		int mark = f.n;
		PyObject *dname = Sf_Item(&f, dummy, 0);
		PyObject *dpos = Sf_Item(&f, dummy, 1);
		PyObject *l = Sf_Own(&f, PK, "CreateBox", "(OOOO)", Sf_Cat(&f, "sO", "Park", Sf_Slice(&f, dname, 3, 0x7fffffff)),
			Sf_Item(&f, dpos, 0), Sf_Item(&f, dpos, 1), Sf_Item(&f, dpos, 2));
		Sf_Method(&f, parkings, "append", "(O)", Sf_Attr(&f, l, "Name"));
		Sf_Ask(&f, "Scrap", "AddParkingZone", "(iOO)", 0, Sf_Attr(&f, l, "Pos"), Sf_Slice(&f, dname, 10, 0x7fffffff));
		dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		Sf_ReleaseKeep(&f, mark, &dummy, 1);
	}
	return Sf_Return(&f, parkings);
}

/* CreateParkedShip(Pos, Angle, Type=None): a parked standard ship of the group Type names (ShipTypes), else any but
 * the mercenaries' (the script makes AllShips upper case and removes them from it) */
static PyObject *Parking_CreateParkedShip(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "Pos", "Angle", "Type", NULL };
	static const char *const mercs[] = { "SMERC1", "SMERC2", "SMERC3" };
	PyObject *pos, *angle, *type = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|O", names, &pos, &angle, &type)) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "ShipMaker");
	Sf_Import(&f, "string");
	PyObject *o1 = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "Parked", Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
		Sf_Item(&f, pos, 2), "Car");
	Sf_SetAttrS(&f, o1, "Descriptor", "MegaShip");
	Sf_SetAttr(&f, o1, "Ang", Sf_Build(&f, "(iOi)", 0, angle, 0));
	PyObject *keys = NULL;
	int skip = 1;
	if (Sf_True(&f, type) > 0) {
		PyObject *nam = Sf_Ask(&f, "string", "split", "(Os)", type, "_");
		PyObject *types = G("ShipTypes");
		if (Sf_True(&f, Sf_Method(&f, types, "has_key", "(O)", Sf_Item(&f, nam, 0))) > 0) {
			keys = Sf_GetItem(&f, types, Sf_Item(&f, nam, 0));
			skip = 0;
		}
	}
	if (skip) {
		keys = G("AllShips");
		long count = 0;
		int n = Sf_Len(&f, keys);
		Sf_Check(&f, n < 0 || Script_Range(Sf_Int(&f, n), &count));
		for (long i = 0; i < count && !f.failed; i++) {
			int mark = f.n;
			PyObject *k = Sf_Int(&f, i);
			Sf_SetItem(&f, keys, k, Sf_Ask(&f, "string", "upper", "(O)", Sf_GetItem(&f, keys, k)));
			Sf_Release(&f, mark);
		}
		for (int i = 0; i < 3 && !f.failed; i++) {
			PyObject *s = Sf_Str(&f, mercs[i]);
			if (Sf_In(&f, s, keys) > 0) {
				Sf_Method(&f, keys, "remove", "(O)", s);
			}
		}
	}
	int n = Sf_Len(&f, keys);
	PyObject *pick = n < 0 ? NULL : Sf_ToInt(&f, Sf_Float(&f, Scrap_RandRange(0, n - 0.1)));
	Sf_Ask(&f, "Vehicles", "SetStdProperties", "(OOs)", Sf_Attr(&f, o1, "Name"), Sf_GetItem(&f, keys, pick), "P_None");
	Sf_Own(&f, PK, "SetShipAsParked", "(O)", Sf_Attr(&f, o1, "Name"));
	Sf_Own(&f, PK, "CheckWeapons", "(O)", o1);
	return Sf_Return(&f, o1);
}

/* CheckWeapons(v): the parked ship carries only the weapons the player may have */
static PyObject *Parking_CheckWeapons(PyObject *self, PyObject *args) {
	static const char *const weap[] = { "Tesla", "Devastator", "ATPC", "Inferno" };
	static const char *const vars[] = { "isWpn02", "isWpn03", "isWpn04", "isWpn05" };
	PyObject *v;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	PyObject *list = Sf_Eval(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.availableWeaponsList"), PK);
	for (int i = 0; i < 4 && !f.failed; i++) {
		if (Sf_In(&f, Sf_Str(&f, weap[i]), list) == 0) {
			Sf_SetAttrI(&f, v, vars[i], 0);
		}
	}
	return Sf_None(&f);
}

/* BrowseParkedPos(): a parked ship on every active DM_Parked dummy (unless ParkedOutCallback made one) */
static PyObject *Parking_BrowseParkedPos(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "ShipMaker");
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_Parked*", 1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "!=", Py_None) <= 0) {
			break;
		}
		if (Sf_True(&f, Sf_Item(&f, dummy, 3)) > 0) {
			PyObject *name = Sf_Slice(&f, Sf_Item(&f, dummy, 0), 10, 0x7fffffff);
			PyObject *dpos = Sf_Item(&f, dummy, 1);
			int create = Sf_True(&f, G("ParkedOutCallback"));
			if (create > 0) {
				create = Sf_True(&f, Sf_Call(&f, G("ParkedOutCallback"), "(OO)", dpos, name));
				create = create < 0 ? -1 : !create;
			} else if (create == 0) {
				create = 1;
			}
			if (create > 0) {
				Sf_Own(&f, PK, "CreateParkedShip", "(OOO)", dpos, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), name);
			}
			Sf_Ask(&f, "Scrap", "AddParkingZone", "(iOO)", 1, dpos, name);
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* ChangeLevel(ItemName, VehicleName): the player flies through a link to the next level: the way point, the ship
 * (its parts when it is not the player's own) and its ammunition for the next level */
static PyObject *Parking_ChangeLevel(PyObject *self, PyObject *args) {
	static const char *const ammo[][2] = { { "usrShip.Ammo00", "Ammo00" }, { "usrShip.Ammo01", "Ammo01" },
		{ "usrShip.Ammo02", "Ammo02" }, { "usrShip.AcWeap", "AcWeap" } };
	PyObject *itemName, *vehName;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &itemName, &vehName)) {
		return NULL;
	}
	Sf_Import(&f, "SaveGame");
	Sf_Import(&f, "Init");
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	PyObject *level = Sf_Cat(&f, "sO", "Levels/", Sf_Slice(&f, itemName, 11, 0x7fffffff));
	PyObject *spawn = Sf_Op(&f, G("PR_Spawn"), '+', Sf_Slice(&f, Sf_Mod(&f, "Init", "Path"), 7, 0x7fffffff));
	Sf_Ask(&f, "SaveGame", "MarkWayPoint", "(OOO)", level, spawn, Sf_Slice(&f, Sf_Attr(&f, usr, "MPil0"), 2, 0x7fffffff));
	int own = Sf_True(&f, G("PlayerShip"));
	if (own > 0) {
		own = Sf_Compare(&f, Sf_Attr(&f, usr, "Name"), "==", Sf_Attr(&f, G("PlayerShip"), "Name"));
	}
	if (own > 0) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "ComeFrom", G("PR_ComeFrom"));
	} else if (own == 0) {
		PyObject *s = Sf_Cat(&f, "OsOsOsOsOsOsOs", G("PR_ComeFrom"), "\"", Sf_Attr(&f, usr, "Model"), "\",\"",
			Sf_Attr(&f, usr, "MEng0"), "\",\"", Sf_Attr(&f, usr, "MEng1"), "\",\"", Sf_Attr(&f, usr, "MEng2"), "\",\"",
			Sf_Attr(&f, usr, "MEng3"), "\",\"", Sf_Attr(&f, usr, "WeapList"), "\",");
		s = Sf_Cat(&f, "OOsOs", s, Sf_Repr(&f, Sf_Attr(&f, usr, "MaxLife")), ",\"", Sf_Attr(&f, usr, "Descriptor"), "\"");
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", "ComeFrom", s);
	}
	for (int i = 0; i < 4 && !f.failed; i++) {
		Sf_Ask(&f, "Scrap", "SetSaveVar", "(sO)", ammo[i][0], Sf_Repr(&f, Sf_Attr(&f, usr, ammo[i][1])));
	}
	Pk_StartGame(&f);
	return Sf_None(&f);
}

/* BrowseParkingAreas(): an object for every DM_AreaToPark_ dummy */
static PyObject *Parking_BrowseParkingAreas(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_AreaToPark_*", 1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "!=", Py_None) <= 0) {
			break;
		}
		PyObject *p = Sf_Item(&f, dummy, 1);
		Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", Sf_Item(&f, dummy, 0), Sf_Item(&f, p, 0), Sf_Item(&f, p, 1),
			Sf_Item(&f, p, 2), "Obj");
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* BrowseSwitchLevel(): the links to the next levels (DM_OutLink_ dummies) */
static PyObject *Parking_BrowseSwitchLevel(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Import(&f, "ShipMaker");
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_OutLink_*", 1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "!=", Py_None) <= 0) {
			break;
		}
		PyObject *p = Sf_Item(&f, dummy, 1);
		PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", Sf_Item(&f, dummy, 0), Sf_Item(&f, p, 0),
			Sf_Item(&f, p, 1), Sf_Item(&f, p, 2), "Item");
		Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), '+', 180), 0));
		Sf_SetAttrI(&f, l, "Radius", 20000);
		Sf_SetAttrS(&f, l, "OnTake", "Parking.ChangeLevel");
		Sf_SetAttrI(&f, l, "OnlyUser", 1);
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* ShowNextMap(EntityName, EventName, Time): the event of a link door: the name of the next station */
static PyObject *Parking_ShowNextMap(PyObject *self, PyObject *args) {
	PyObject *ent, *event, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ent, &event, &time)) {
		return NULL;
	}
	Sf_Import(&f, "RaceMaker");
	Sf_Import(&f, "string");
	PyObject *msg = Sf_Lang(&f, Sf_Str(&f, "Generics_Outlink_SystemMessage"));
	PyObject *station = Sf_Lang(&f, Sf_Cat(&f, "sO", "Station_", Sf_Slice(&f, ent, 15, 0x7fffffff)));
	Sf_Ask(&f, "RaceMaker", "ShowSystemMessage", "(O)", Sf_Ask(&f, "string", "replace", "(OsO)", msg, "<n1>", station));
	return Sf_None(&f);
}

/* DoorInitializator(Class): the event of the Open action of a link door */
static PyObject *Parking_DoorInitializator(PyObject *self, PyObject *args) {
	PyObject *cls;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cls)) {
		return NULL;
	}
	Sf_Ask(&f, "SAct", "GetAction", "(s)", "Open");
	Sf_Ask(&f, "SAct", "SetAct", "(si)", "AddEventAtTime", 0);
	Sf_Ask(&f, "SAct", "SetAct", "(ss)", "EventName", "Open");
	Sf_Ask(&f, "SAct", "SetAct", "(ss)", "EventFunc", "Parking.ShowNextMap");
	return Sf_None(&f);
}

/* the arrival from another outdoor level (Init): on the player's own ship or on the stolen ship the save variable
 * ComeFrom describes, at the spawn dummy, with its ammunition */
static void Pk_Arrive(scriptFrame_t *f, PyObject *comeFrom, PyObject *spawn) {
	PyObject *usrPlay = Sf_Ask(f, "Scrap", "UsrEntity", "(i)", 0);
	Sf_SetAttrI(f, usrPlay, "Active", 0);
	Sf_Ask(f, "Scrap", "StartDummySearch", "(Oi)", spawn, 0);
	PyObject *dummy = Sf_Ask(f, "Scrap", "NextDummySearch", "()");
	int found = Sf_True(f, dummy);
	if (found == 0) {
		Sf_Ask(f, "Scrap", "ConsoleError", "(O)", Sf_Cat(f, "sOs", "E R R O R : ", spawn, " not found.\n"));
		return;
	}
	if (found < 0) {
		return;
	}
	PyObject *usrShip = NULL;
	int own = Sf_Compare(f, comeFrom, "==", Sf_Global(f, PK, "PR_ComeFrom"));
	if (own > 0) {
		usrShip = Sf_Global(f, PK, "PlayerShip");
		PyObject *pos = Sf_Attr(f, Sf_Global(f, PK, "PlayerShip"), "Pos");
		PyObject *l = Sf_Own(f, PK, "CreateBox", "(sOOO)", "ParkingPlayerShip", Sf_Item(f, pos, 0), Sf_Item(f, pos, 1),
			Sf_Item(f, pos, 2));
		Sf_SetAttr(f, l, "rTime", Sf_Float(f, 0.1));
	} else if (own == 0) {
		PyObject *p = Sf_Item(f, dummy, 1);
		PyObject *o1 = Sf_Ask(f, "Scrap", "CreateEntity", "(sOOOs)", "StolenShip", Sf_Item(f, p, 0), Sf_Item(f, p, 1),
			Sf_Item(f, p, 2), "Car");
		PyObject *a = Sf_Eval(f, Sf_Slice(f, comeFrom, 8, 0x7fffffff), PK);
		Sf_Ask(f, "Vehicles", "SetProperties", "(OOOsOOOOOO)", Sf_Attr(f, o1, "Name"), Sf_Item(f, a, 0), Sf_Item(f, a, 6),
			"P_None", Sf_Item(f, a, 1), Sf_Item(f, a, 2), Sf_Item(f, a, 3), Sf_Item(f, a, 4), Sf_Item(f, a, 5),
			Sf_Item(f, a, 7));
		usrShip = o1;
		PyObject *pos = Sf_Attr(f, Sf_Global(f, PK, "PlayerShip"), "Pos");
		Sf_Own(f, PK, "OnParkingOccupied", "(sOOOs)", "PlayerShip", Sf_Item(f, pos, 0), Sf_Item(f, pos, 1),
			Sf_Item(f, pos, 2), "_05");
		Sf_SetAttrI(f, Sf_Global(f, PK, "DtritusPlate"), "Active", 0);
	}
	PyObject *at = Sf_Item(f, dummy, 1);
	Sf_SetAttr(f, usrShip, "MovePos", at);
	Sf_SetAttr(f, usrShip, "Pos", at);
	PyObject *ang = Sf_Build(f, "(iOi)", 0, Sf_OpI(f, Sf_Item(f, Sf_Item(f, dummy, 2), 1), '+', 180), 0);
	Sf_SetAttr(f, usrShip, "MoveAng", ang);
	Sf_SetAttr(f, usrShip, "Ang", ang);
	static const char *const ammo[][2] = { { "Ammo00", "usrShip.Ammo00" }, { "Ammo01", "usrShip.Ammo01" },
		{ "Ammo02", "usrShip.Ammo02" } };
	for (int i = 0; i < 3 && !f->failed; i++) {
		PyObject *s = Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", ammo[i][1]);
		Sf_SetAttr(f, usrShip, ammo[i][0], f->failed || !s ? NULL : Sf_Keep(f, PyNumber_Float(s)));
	}
	Sf_SetGlobal(f, PK, "SetWeaponOnShip", Sf_Int(f, 1));
	Sf_Own(f, PK, "UseShip", "(OOssO)", Sf_Attr(f, usrPlay, "Name"), Sf_Attr(f, usrShip, "Name"), "", "", Py_None);
}

/* Init(): offline: the plate, the parking boxes and the player's ship, the parked ships (unless the save game says
 * the level is desolate), the links and parking areas; the player starts in the junkyard parking or arrives from
 * another outdoor level on a ship */
static PyObject *Parking_Init(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "GetNetFlags", "()"), "==", Sf_Build(&f, "(iii)", 0, 0, 0)) <= 0) {
		return Sf_None(&f);
	}
	Sf_Own(&f, PK, "CreateDtritusPlate", "()");
	Sf_Ask(&f, "Scrap", "CreateEntityList", "(O)", G("PR_Lst"));
	Sf_Own(&f, PK, "BrowseItemPos", "()");
	PyObject *comeFrom = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "ComeFrom");
	PyObject *spawn = Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Spawn");
	if (Sf_True(&f, Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Parking.Desolate"))) == 0) {
		Sf_Own(&f, PK, "BrowseParkedPos", "()");
	}
	Sf_Own(&f, PK, "BrowseSwitchLevel", "()");
	Sf_Own(&f, PK, "BrowseParkingAreas", "()");
	int junk = Sf_Compare(&f, spawn, "==", G("PL_Junk"));
	if (junk > 0) {
		Sf_Own(&f, PK, "UseShip", "(OOsss)", Sf_Attr(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "Name"),
			Sf_Attr(&f, G("PlayerShip"), "Name"), "", "", "ParkParking_Junkyard");
		Sf_SetAttrS(&f, G("LastPark"), "OnTake", "Parking.JunkerPark");
	} else if (junk == 0) {
		int arrive = Sf_Compare(&f, Sf_Slice(&f, comeFrom, 0, 8), "==", G("PR_ComeFrom"));
		if (arrive > 0) {
			arrive = Sf_Compare(&f, Sf_Slice(&f, spawn, 0, 14), "==", G("PR_Spawn"));
		}
		if (arrive > 0) {
			Pk_Arrive(&f, comeFrom, spawn);
		}
	}
	return Sf_None(&f);
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

/* CreateAccelerator(Pos, Angx, Angy, Radius=ACCELERATOR_RADIUS): the item of a speed ring */
static PyObject *Accelerator_CreateAccelerator(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "Pos", "Angx", "Angy", "Radius", NULL };
	PyObject *pos, *ax, *ay, *radius = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOO|O", names, &pos, &ax, &ay, &radius)) {
		return NULL;
	}
	/* the default of the def line, bound when the script loaded */
	radius = radius ? Sf_Ref(&f, radius) : Sf_Int(&f, 3500);
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "Accelerator", Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
		Sf_Item(&f, pos, 2), "Item");
	Sf_SetAttr(&f, l, "Radius", radius);
	Sf_SetAttrS(&f, l, "OnTake", "!AcceleratorOnEnter");
	Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(OOi)", ax, ay, 0));
	return Sf_Return(&f, l);
}

/* Init(): a speed ring on every active DM_Accelerator dummy ("=<radius>" in the name gives its radius) */
static PyObject *Accelerator_Init(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "string");
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_Accelerator*", 1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "!=", Py_None) <= 0) {
			break;
		}
		if (Sf_True(&f, Sf_Item(&f, dummy, 3)) > 0) {
			PyObject *s = Sf_Ask(&f, "string", "split", "(Os)", Sf_Item(&f, dummy, 0), "=");
			PyObject *radius;
			int n = Sf_Len(&f, s);
			if (n > 1) {
				radius = Sf_ToInt(&f, Sf_Item(&f, s, 1));
			} else {
				radius = Sf_Global(&f, "Accelerator", "ACCELERATOR_RADIUS");
			}
			PyObject *ang = Sf_Item(&f, dummy, 2);
			Sf_Own(&f, "Accelerator", "CreateAccelerator", "(OOOO)", Sf_Item(&f, dummy, 1), Sf_Item(&f, ang, 0),
				Sf_Item(&f, ang, 1), radius);
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

PyMethodDef g_AcceleratorMethods[] = {
	{ "CreateAccelerator", (PyCFunction)(void (*)(void))Accelerator_CreateAccelerator, 3, NULL },
	{ "Init", Accelerator_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
