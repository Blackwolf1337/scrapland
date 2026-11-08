/*
 * The gameplay functions of the Sputnik character in C (scripts/chars/sputnik.py, issue #2; Init and OnCreate are
 * data/chars/sputnik.def, Characters.c): Sputnik beside the player's ship (Assign, SetSptAt), the ship he sucks in
 * when the player takes another one (UseShip, BackCamera), the hangar screen he opens (ChangeShip, CreateShip,
 * ShowInterface, EnterInterface, EndOfSelection, BackControl) and the events of his animations.
 *
 * The module variables stay in the script, Parking, Scene and the missions read them: spt (Sputnik's entity), ply,
 * LastPos, LastAng, LastHangar, UseParams (the arguments of Parking.UseShip after the suck animation) and Angulus.
 * Parking and SputnikInterface are called through their modules.
 */
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

#define SP "Sputnik"

#define G(name) Sf_Global(&f, SP, name)
#define PS() Sf_Mod(&f, "Parking", "PlayerShip")
#define PLATE() Sf_Mod(&f, "Parking", "DtritusPlate")

/* Scrap.GetEntity(Scrap.GetCam(0)) */
static PyObject *Spt_Cam(scriptFrame_t *f) {
	return Sf_Entity(f, Sf_Ask(f, "Scrap", "GetCam", "(i)", 0));
}

/* the three arguments of an animation event */
static byte Spt_Event(PyObject *args) {
	PyObject *a, *b, *c;
	return PyArg_ParseTuple(args, "OOO", &a, &b, &c) != 0;
}

/* Assign(Entity, Player=None): Sputnik (created the first time) beside Entity, still, with the option to change the
 * ship; the player in front of him */
static PyObject *Sputnik_Assign(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "Entity", "Player", NULL };
	PyObject *ent, *player = Py_None;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|O", names, &ent, &player)) {
		return NULL;
	}
	Sf_Import(&f, "Chars");
	int has = Sf_True(&f, G("spt"));
	if (has > 0) {
		Sf_SetAttrI(&f, G("spt"), "Active", 1);
	} else if (has == 0) {
		Sf_SetGlobal(&f, SP, "spt", Sf_Ask(&f, "Scrap", "CreateEntity", "(siiis)", "Sputnik", 0, 0, 0, "WalkChar"));
		Sf_SetAttrS(&f, G("spt"), "ActCtrl", "Sputnik");
	}
	PyObject *pos = Sf_Attr(&f, ent, "Pos");
	PyObject *ang = Sf_Attr(&f, ent, "Ang");
	PyObject *spt = G("spt");
	Sf_SetAttr(&f, spt, "Ang", ang);
	int car = Sf_True(&f, Sf_Method(&f, ent, "IsType", "(s)", "Car"));
	if (car > 0) {
		PyObject *d = Sf_OpI(&f, Sf_Op(&f, Sf_Attr(&f, ent, "VRadius"), '+', Sf_Attr(&f, spt, "VRadius")), '+', 10);
		PyObject *rot = Sf_Ask(&f, "SVec", "Rotate3D", "((Oii)(iOi))", d, 0, 0, 0, Sf_Item(&f, ang, 1), 0);
		Sf_SetAttr(&f, spt, "Pos", Sf_Ask(&f, "SVec", "Add", "(OO)", pos, rot));
	} else if (car == 0) {
		Sf_SetAttr(&f, spt, "Pos", pos);
	}
	Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, spt, "Name"));
	Sf_SetAttrI(&f, spt, "ActiveCMask", 1);
	Sf_SetAttr(&f, spt, "MovePos", Sf_Attr(&f, spt, "Pos"));
	Sf_SetAttr(&f, spt, "MoveAng", Sf_Attr(&f, spt, "Ang"));
	Sf_SetAttr(&f, spt, "MoveTime", Sf_Float(&f, 0.001));
	Sf_SetAttrI(&f, spt, "ArrowCanUse", 1);
	Sf_SetAttrI(&f, spt, "ArrnOpt", 0);
	Sf_SetAttr(&f, spt, "ArrOpt0", Sf_Cat(&f, "sO", "ChangeShip:", Sf_Lang(&f, Sf_Str(&f, "ChangeShip"))));
	Sf_SetAttrS(&f, spt, "UseFunc", "Sputnik.ChangeShip");
	if (Sf_True(&f, player) > 0) {
		spt = G("spt");
		PyObject *neg = f.failed ? NULL : Sf_Keep(&f, PyNumber_Negative(Sf_Attr(&f, player, "VRadius")));
		PyObject *d = Sf_OpI(&f, Sf_Op(&f, neg, '-', Sf_Attr(&f, spt, "VRadius")), '-', 10);
		PyObject *rot = Sf_Ask(&f, "SVec", "Rotate3D", "((iiO)(iOi))", 0, 0, d, 0, Sf_Item(&f, ang, 1), 0);
		Sf_SetAttr(&f, player, "Pos", Sf_Ask(&f, "SVec", "Add", "(OO)", Sf_Attr(&f, spt, "Pos"), rot));
		Sf_SetAttr(&f, player, "Ang", Sf_Ask(&f, "SVec", "Sub", "(O(iii))", Sf_Attr(&f, ent, "Ang"), 0, 210, 0));
		Sf_Ask(&f, "SLogic", "SetOnFloor", "(O)", Sf_Attr(&f, player, "Name"));
	}
	return Sf_None(&f);
}

/* ChangeRelax(EntityName, EventName, Time): one of the relax animations at random */
static PyObject *Sputnik_ChangeRelax(PyObject *self, PyObject *args) {
	PyObject *ent, *event, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ent, &event, &time)) {
		return NULL;
	}
	PyObject *ch = Sf_Entity(&f, ent);
	PyObject *n = f.failed ? NULL : Sf_ToInt(&f, Sf_Float(&f, Scrap_RandRange(0, 6)));
	Sf_SetAttr(&f, ch, "ForceAction", Sf_Cat(&f, "sO", "Relax", Sf_Repr(&f, n)));
	return Sf_None(&f);
}

/* ByeBye(EntityName, EventName, Time): Sputnik goes */
static PyObject *Sputnik_ByeBye(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!Spt_Event(args)) {
		return NULL;
	}
	Sf_SetAttrI(&f, G("spt"), "Active", 0);
	return Sf_None(&f);
}

/* BackCamera(EntityName, EventName, Time): the old ship is sucked in: the walk HUD, then Parking.UseShip with the
 * arguments UseShip kept */
static PyObject *Sputnik_BackCamera(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!Spt_Event(args)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "CharScorer");
	Sf_Import(&f, "Parking");
	Sf_SetAttrS(&f, Spt_Cam(&f), "EntityLink", "");
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Walk");
	Sf_Ask(&f, "CharScorer", "Create", "(i)", 0);
	Sf_SetAttrI(&f, PS(), "ActiveCMask", 1);
	Sf_SetAttrI(&f, PS(), "Active", 0);
	Sf_SetAttrI(&f, PLATE(), "Active", 0);
	Sf_Ask(&f, "Parking", "SetShipRadius", "(Oi)", PS(), 0);
	Sf_Import(&f, "Parking");
	Sf_Call(&f, Sf_Mod(&f, "__builtin__", "apply"), "(OO)", Sf_Mod(&f, "Parking", "UseShip"), G("UseParams"));
	return Sf_None(&f);
}

/* UseShip(UsrName, CharName, optId, FullOption, BoxName="Parking"): the player takes another parked ship: Sputnik
 * sucks in the player's own one first (when it is out), then Parking.UseShip */
static PyObject *Sputnik_UseShip(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "UsrName", "CharName", "optId", "FullOption", "BoxName", NULL };
	PyObject *usr, *ch, *opt, *full, *box = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOOO|O", names, &usr, &ch, &opt, &full, &box)) {
		return NULL;
	}
	box = box ? Sf_Ref(&f, box) : Sf_Str(&f, "Parking");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Parking");
	int other = Sf_Compare(&f, Sf_Attr(&f, PS(), "Name"), "!=", ch);
	int out = other > 0 ? Sf_True(&f, Sf_Attr(&f, PS(), "Active")) : -1;
	if (other > 0 && out > 0) {
		Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
		Sf_Ask(&f, "Menu", "MovieScorer", "(i)", 0);
		Sf_SetAttrI(&f, PS(), "ActiveCMask", 0);
		Sf_SetAttrI(&f, PLATE(), "Active", 0);
		PyObject *vPos = Sf_Attr(&f, PS(), "Pos");
		Sf_SetAttr(&f, PS(), "Life", Sf_Attr(&f, PS(), "MaxLife"));
		Sf_SetAttr(&f, Spt_Cam(&f), "EntityLink", Sf_Attr(&f, G("spt"), "Name"));
		Sf_SetAttr(&f, G("spt"), "Pos", vPos);
		Sf_SetAttr(&f, G("spt"), "MovePos", vPos);
		Sf_SetAttr(&f, G("spt"), "MoveTime", Sf_Float(&f, 0.01));
		Sf_SetAttrS(&f, G("spt"), "Action", "Suck");
		Sf_SetGlobal(&f, SP, "UseParams", Sf_Build(&f, "(OOOOO)", usr, ch, opt, full, box));
		PyObject *b = Sf_Ask(&f, "Parking", "CreateBox", "(sOOO)", "Parkingson", Sf_Item(&f, vPos, 0), Sf_Item(&f, vPos, 1),
			Sf_Item(&f, vPos, 2));
		Sf_SetAttrI(&f, b, "rTime", 5);
		Sf_Ask(&f, "Parking", "SetInvunerable", "(O)", Sf_Entity(&f, ch));
	} else if (other > 0 && out == 0) {
		PyObject *vPos = Sf_Attr(&f, PS(), "Pos");
		Sf_Ask(&f, "Parking", "CreateBox", "(sOOO)", "Parkingson", Sf_Item(&f, vPos, 0), Sf_Item(&f, vPos, 1),
			Sf_Item(&f, vPos, 2));
		Sf_Import(&f, "Parking");
		Sf_Ask(&f, "Parking", "UseShip", "(OOOOO)", usr, ch, opt, full, box);
	} else if (other == 0) {
		Sf_Import(&f, "Parking");
		Sf_Ask(&f, "Parking", "UseShip", "(OOOOO)", usr, ch, opt, full, box);
	}
	return Sf_None(&f);
}

/* ChangeShip(UsrName, CharName, optId, FullOption): the player asks Sputnik for another ship: his interface
 * animation, the player hidden */
static PyObject *Sputnik_ChangeShip(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "SputnikInterface");
	Sf_SetGlobal(&f, SP, "ply", Sf_Entity(&f, usr));
	Sf_Own(&f, SP, "Assign", "(OO)", PS(), G("ply"));
	Sf_SetAttrI(&f, G("ply"), "Active", 0);
	Sf_SetAttrS(&f, G("spt"), "Action", "Interface");
	Sf_SetAttr(&f, Spt_Cam(&f), "EntityLink", Sf_Attr(&f, G("spt"), "Name"));
	Sf_SetGlobal(&f, SP, "LastPos", Sf_Attr(&f, PS(), "Pos"));
	Sf_SetGlobal(&f, SP, "LastAng", Sf_Attr(&f, PS(), "Ang"));
	Sf_SetAttrS(&f, PS(), "OnDeath", "");
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	Sf_Ask(&f, "Menu", "MovieScorer", "(i)", 0);
	PyObject *last = G("LastPos");
	Sf_SetAttr(&f, G("spt"), "MovePos", last);
	Sf_SetAttr(&f, G("spt"), "Pos", last);
	return Sf_None(&f);
}

/* CreateShip(EntityName, EventName, Time): before the hangar screen: the player's ship back when it was away
 * (LastHangar None), else LastHangar the hangar it came from */
static PyObject *Sputnik_CreateShip(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!Spt_Event(args)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "SputnikInterface");
	int active = Sf_True(&f, Sf_Attr(&f, PS(), "Active"));
	if (active == 0) {
		Sf_SetAttrI(&f, PS(), "Active", 1);
		Sf_SetAttrI(&f, PLATE(), "Active", 1);
		Sf_Ask(&f, "Parking", "SetShipAsParked", "(O)", Sf_Attr(&f, PS(), "Name"));
		Sf_SetAttr(&f, G("spt"), "Pos", Sf_Attr(&f, PS(), "Pos"));
		Sf_SetGlobal(&f, SP, "LastHangar", Py_None);
	} else if (active > 0) {
		Sf_SetGlobal(&f, SP, "LastHangar", Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.iHangarShip"));
	}
	return Sf_None(&f);
}

/* ShowInterface(EntityName, EventName, Time): the fade to black */
static PyObject *Sputnik_ShowInterface(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!Spt_Event(args)) {
		return NULL;
	}
	Sf_Ask(&f, "SScorer", "Fade", "(i(iiii)(iiii)d)", 0, 0, 0, 0, 0, 0, 0, 0, 255, 0.25);
	Sf_Ask(&f, "SScorer", "SetCinema", "(iid)", 0, 0, 0.25);
	return Sf_None(&f);
}

/* EnterInterface(EntityName, EventName, Time): the hangar screen (SputnikInterface) with EndOfSelection as its end */
static PyObject *Sputnik_EnterInterface(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!Spt_Event(args)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "SputnikInterface");
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Walk");
	Sf_SetMod(&f, "SputnikInterface", "EndSputnikCallBack", G("EndOfSelection"));
	Sf_Ask(&f, "SputnikInterface", "EnterSputnikInterface", "(i)", 0);
	Sf_SetAttrI(&f, G("spt"), "Active", 0);
	Sf_SetAttrS(&f, Spt_Cam(&f), "EntityLink", "");
	Sf_SetAttrS(&f, PS(), "OnDeath", "Vehicles.OnDeath");
	Sf_Ask(&f, "SScorer", "Fade", "(i(iiii)(iiii)d)", 0, 0, 0, 0, 255, 0, 0, 0, 0, 0.25);
	Sf_Ask(&f, "Scrap", "Set", "(si)", "NoiseFactorBegin", 0);
	Sf_Ask(&f, "Scrap", "Set", "(si)", "NoiseFactor", 24);
	Sf_Ask(&f, "Scrap", "Set", "(si)", "NoiseTime", 0);
	Sf_Ask(&f, "Scrap", "Set", "(sd)", "NoiseEndTime", 0.25);
	return Sf_None(&f);
}

/* EndOfSelection(): back from the hangar screen: the ship of the chosen hangar where the old one stood; a new one is
 * made in front of the player (Sputnik's Make animation, the ship transparent) */
static PyObject *Sputnik_EndOfSelection(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "SputnikInterface");
	Sf_SetAttrI(&f, G("ply"), "Active", 1);
	Sf_SetAttrI(&f, G("spt"), "Active", 1);
	Sf_Ask(&f, "Parking", "CreatePlayerShip", "(O)", G("LastPos"));
	Sf_Ask(&f, "Parking", "SetShipRadius", "(Oi)", PS(), 0);
	Sf_SetAttr(&f, PS(), "Ang", G("LastAng"));
	Sf_SetAttrI(&f, PS(), "Active", 1);
	Sf_SetAttrI(&f, PLATE(), "Active", 1);
	Sf_Ask(&f, "Parking", "SetShipAsParked", "(O)", Sf_Attr(&f, PS(), "Name"));
	Sf_Own(&f, SP, "Assign", "(OO)", PS(), G("ply"));
	Sf_Ask(&f, "Scrap", "Set", "(si)", "NoiseFactor", 0);
	if (Sf_Compare(&f, G("LastHangar"), "!=", Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "Hangar.iHangarShip")) > 0) {
		Sf_SetAttrS(&f, G("spt"), "Action", "Make");
		Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
		Sf_SetAttrI(&f, PLATE(), "Active", 0);
		Sf_Ask(&f, "Menu", "MovieScorer", "(i)", 0);
		Sf_SetAttrI(&f, PS(), "RenderEffect", 2);
		Sf_SetAttrI(&f, PS(), "RenderEffectParam0", 1);
	}
	return Sf_None(&f);
}

/* BackControl(EntityName, EventName, Time): the end of the Make animation: the player walks again */
static PyObject *Sputnik_BackControl(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!Spt_Event(args)) {
		return NULL;
	}
	Sf_Import(&f, "CharScorer");
	Sf_Import(&f, "Parking");
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Walk");
	Sf_Ask(&f, "CharScorer", "Create", "(i)", 0);
	Sf_SetAttr(&f, PLATE(), "Active", Sf_Attr(&f, PS(), "Active"));
	return Sf_None(&f);
}

/* SetSptAt(Pos, Ang, Anm): Sputnik still at Pos playing Anm, without the use arrow */
static PyObject *Sputnik_SetSptAt(PyObject *self, PyObject *args) {
	PyObject *pos, *ang, *anm;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &pos, &ang, &anm)) {
		return NULL;
	}
	Sf_SetAttrI(&f, G("spt"), "Active", 1);
	Sf_SetAttr(&f, G("spt"), "Pos", pos);
	Sf_SetAttr(&f, G("spt"), "Ang", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, ang, 1), 0));
	Sf_SetAttr(&f, G("spt"), "MovePos", pos);
	Sf_SetAttr(&f, G("spt"), "MoveAng", Sf_Build(&f, "(iOi)", 0, Sf_Item(&f, ang, 1), 0));
	Sf_SetAttr(&f, G("spt"), "MoveTime", Sf_Float(&f, 0.001));
	Sf_SetAttr(&f, G("spt"), "Action", anm);
	Sf_SetAttrI(&f, G("spt"), "ActiveCMask", 0);
	Sf_SetAttrI(&f, G("spt"), "ArrowCanUse", 0);
	return Sf_None(&f);
}

/* the effect events: on the player's ship (InitSuck, InitMake) or on Sputnik */
static PyObject *Spt_FX(PyObject *args, byte ship, const char *fx) {
	scriptFrame_t f = { 0 };
	if (!Spt_Event(args)) {
		return NULL;
	}
	Sf_Import(&f, "Parking");
	Sf_Method(&f, ship ? PS() : Sf_Entity(&f, Sf_Str(&f, "Sputnik")), "SetFX", "(s)", fx);
	return Sf_None(&f);
}

static PyObject *Sputnik_InitSuck(PyObject *self, PyObject *args) {
	(void)self;
	return Spt_FX(args, 1, "VehicleSputnikSuck");
}

static PyObject *Sputnik_InitMake(PyObject *self, PyObject *args) {
	(void)self;
	return Spt_FX(args, 1, "VehicleSputnikMake");
}

static PyObject *Sputnik_InitHello(PyObject *self, PyObject *args) {
	(void)self;
	return Spt_FX(args, 0, "CharacterSputnikHello");
}

static PyObject *Sputnik_InitBye(PyObject *self, PyObject *args) {
	(void)self;
	return Spt_FX(args, 0, "CharacterSputnikBye");
}

static PyObject *Sputnik_InitInterface(PyObject *self, PyObject *args) {
	(void)self;
	return Spt_FX(args, 0, "CharacterSputnikInterface");
}

/* the entries of the functions other ports call (OBJLIB 7: on Python values) */
MODE_PYENTRYKW(Sputnik_Assign)
MODE_PYENTRY(Sputnik_SetSptAt)

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
