/*
 * The CharAct module in C (scripts/chars/charact.py, issue #2 step 7a): what every character shares. OnCreate (the
 * death callback, the morphing eyes), the photo of the player's camera action, and the death of a character: the
 * player's goes inactive, an NPC respawns and may drop a battery or coins. The hooks the missions set
 * (TakenPhoto, TakenPhotoMap, onNPCDeath) and the module code (EnergyRechargeTime) stay in the module.
 *
 * The script passes entity handles to the scheduled calls (PlayerDeathCall, NPCDeathCall), so this code works on the
 * handles too and keeps the Python values of the script (e.Pos, e.Money / 4) for the functions it calls.
 */
#include <string.h>

#include "scrap/pyentity.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

/* OnCreate(EntityName): the standard initializer of a character */
static PyObject *CharAct_OnCreate(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *eyes = Script_Attr("MakeChar", "MorphEyesList");
	PyObject *e = eyes ? Script_Ask("Scrap", "GetEntity", "(O)", name) : NULL;
	PyObject *act = e && Script_HSet(e, "OnDeath", PyString_FromString("CharAct.OnDeath")) ?
		PyObject_GetAttrString(e, "ActCtrl") : NULL;
	int morph = act ? PySequence_In(eyes, act) : -1;
	byte ok = morph >= 0 && (!morph || Script_HSet(e, "Action", PyString_FromString("MorphEyes")));
	Py_XDECREF(act);
	Py_XDECREF(e);
	Py_XDECREF(eyes);
	return ok ? Py_ReturnNone() : NULL;
}

/* a hook of the module (TakenPhoto, TakenPhotoMap) on the photo: 1 when it counts, -1 with the error set */
static int CharAct_PhotoHook(const char *hook, PyObject *sel) {
	PyObject *f = Script_Attr("CharAct", hook);
	if (!f) {
		return -1;
	}
	int r = 0;
	if (PyObject_IsTrue(f) > 0) {
		PyObject *v = Script_Invoke(f, Py_BuildValue("(O)", sel));
		r = v ? PyObject_IsTrue(v) > 0 : -1;
		Py_XDECREF(v);
	}
	Py_DECREF(f);
	return r;
}

/* Photograph(SelectedEntity): the missions' photo hooks and a crazy character; the sound, the flash and the end of
 * the first photo message when one counted */
static PyObject *CharAct_Photograph(PyObject *self, PyObject *args) {
	PyObject *sel;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &sel)) {
		return NULL;
	}
	int map = CharAct_PhotoHook("TakenPhotoMap", sel);
	int taken = map >= 0 ? CharAct_PhotoHook("TakenPhoto", sel) : -1;
	if (taken < 0) {
		return NULL;
	}
	byte photo = map || taken;
	PyObject *e = Script_Ask("Scrap", "GetEntity", "(O)", sel);
	PyObject *walk = e ? Script_Method(e, "IsType", "(s)", "WalkChar") : NULL;
	if (!walk) {
		Py_XDECREF(e);
		return NULL;
	}
	if (PyObject_IsTrue(walk) > 0) {
		PyObject *act = PyObject_GetAttrString(e, "Action");
		PyObject *head = act ? PySequence_GetSlice(act, 0, 5) : NULL;
		Py_XDECREF(act);
		int crazy = head ? PyString_Check(head) && !strcmp(PyString_AsString(head), "Crazy") : -1;
		Py_XDECREF(head);
		PyObject *r = crazy > 0 ? Script_Ask("CrazyWing", "CrazyPhoto", "(O)", e) : NULL;
		if (crazy < 0 || (crazy > 0 && !r)) {
			Py_DECREF(walk);
			Py_DECREF(e);
			return NULL;
		}
		if (r && PyObject_IsTrue(r) > 0) {
			photo = 1;
		}
		Py_XDECREF(r);
	}
	Py_DECREF(walk);
	Py_DECREF(e);
	if (photo) {
		PyObject *snd = Script_Attr("Sound", "PhotoSound"), *vol = snd ? Script_Attr("Sound", "PhotoVolume") : NULL;
		byte ok = vol && Script_Try("SSound", "Play", "(OO)", snd, vol) &&
			Script_Try("SScorer", "Fade", "(i(iiii)(iiii)i)", 0, 255, 255, 255, 255, 255, 255, 255, 0, 1);
		Py_XDECREF(snd);
		Py_XDECREF(vol);
		if (!ok) {
			return NULL;
		}
		SaveVar_Set("TakePhotoMsg", "0");
		GVar_SetStr("FirstPhotoMessage", "");
	}
	return Py_ReturnNone();
}

/* ActionPhoto(EntityName): the camera action of the player, on the selected entity */
static PyObject *CharAct_ActionPhoto(PyObject *self, PyObject *args) {
	PyObject *name;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	PyObject *e = Script_Ask("Scrap", "GetEntity", "(O)", name);
	PyObject *sel = e ? PyObject_GetAttrString(e, "SelectedEntity") : NULL;
	Py_XDECREF(e);
	if (!sel) {
		return NULL;
	}
	PyObject *a = Py_BuildValue("(O)", sel);
	Py_DECREF(sel);
	PyObject *r = a ? CharAct_Photograph(NULL, a) : NULL;
	Py_XDECREF(a);
	return r;
}

/* PlayerDeathCall(e): the dead player goes inactive */
static PyObject *CharAct_PlayerDeathCall(PyObject *self, PyObject *args) {
	PyObject *e;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	if (PyObject_IsTrue(e) > 0 && !Script_HSet(e, "Active", PyInt_FromLong(0))) {
		return NULL;
	}
	return Py_ReturnNone();
}

/* NPCDeathCall(e): the NPC respawns in 6 s; a battery (Gear) or coins (a quarter of its money) unless the mission's
 * onNPCDeath took care; the message of a mission character; the entity goes */
static PyObject *CharAct_NPCDeathCall(PyObject *self, PyObject *args) {
	PyObject *e;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	PyObject *npc = PyImport_ImportModule("CharsNPC"), *items = npc ? PyImport_ImportModule("MisItems") : NULL;
	Py_XDECREF(npc);
	if (!items) {
		return NULL;
	}
	Py_DECREF(items);
	PyObject *res = Script_Attr("CharsNPC", "CharRes");
	PyObject *name = res ? PyObject_GetAttrString(e, "Name") : NULL;
	PyObject *entry = name ? PyObject_GetItem(res, name) : NULL;
	Py_XDECREF(res);
	if (!entry || !Script_LaterIn(6.0, "CharsNPC", "Respawn", Py_BuildValue("(O)", entry), NULL)) {
		Py_XDECREF(entry);
		Py_XDECREF(name);
		return NULL;
	}
	Py_DECREF(entry);
	PyObject *hook = Script_Attr("CharAct", "onNPCDeath");
	PyObject *r = hook ? PyInt_FromLong(0) : NULL;
	if (hook && PyObject_IsTrue(hook) > 0) {
		PyObject *pos = PyObject_GetAttrString(e, "Pos");
		Py_XDECREF(r);
		r = pos ? Script_Invoke(hook, Py_BuildValue("(OO)", name, pos)) : NULL;
		Py_XDECREF(pos);
	}
	Py_XDECREF(hook);
	Py_DECREF(name);
	PyObject *money = r ? PyObject_GetAttrString(e, "Money") : NULL;
	int ok = money != NULL;
	if (ok && Script_NumCmp(money, 0) != 0) {
		int gear = Script_HIs(e, "ActCtrl", "Gear");
		PyObject *pos = gear >= 0 ? PyObject_GetAttrString(e, "Pos") : NULL;
		ok = pos != NULL;
		if (ok && gear) {
			ok = Script_LaterIn(0.5, "MisItems", "CreateBattery", Py_BuildValue("(O)", pos), NULL);
		} else if (ok && Script_NumCmp(r, 0) == 0 && Script_NumCmp(money, 2) > 0) {
			PyObject *four = PyInt_FromLong(4);
			PyObject *coins = four ? PyNumber_Divide(money, four) : NULL;
			Py_XDECREF(four);
			ok = coins && Script_LaterIn(0.5, "MisItems", "CreateCoin", Py_BuildValue("(OO)", pos, coins), NULL);
			Py_XDECREF(coins);
		}
		Py_XDECREF(pos);
	}
	Py_XDECREF(money);
	Py_XDECREF(r);
	PyObject *radar = ok && !PyErr_Occurred() ? PyObject_GetAttrString(e, "RadarShow") : NULL;
	if (!radar) {
		return NULL;
	}
	int shown = Script_NumCmp(radar, 1) == 0;
	Py_DECREF(radar);
	if (shown && !Script_Try("SScorer", "SetMsgText", "(sd)", Script_Lang("MainMissionCharDeath"),
			(double)Timer_GetTime(&g_pWorld->timer) + 5)) {
		return NULL;
	}
	PyObject *del = Script_Method(e, "Del", "()");
	if (!del) {
		return NULL;
	}
	Py_DECREF(del);
	return Py_ReturnNone();
}

/* NoFXDeath(e, WinnerName, Time=2.0): the death without the effect, the call Time seconds later (now for 0) */
static PyObject *CharAct_NoFXDeath(PyObject *self, PyObject *args) {
	PyObject *e, *winner, *time = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO|O", &e, &winner, &time)) {
		return NULL;
	}
	PyObject *two = time ? NULL : PyFloat_FromDouble(2.0);
	if (!time) {
		time = two;
	}
	PyObject *mf = PyImport_ImportModule("MissionsFuncs");
	Py_XDECREF(mf);
	int player = mf ? Script_HIs(e, "EntityList", "Players") : -1;
	int now = player >= 0 ? Script_NumCmp(time, 0) == 0 : 0;
	PyObject *secs = player >= 0 ? PyNumber_Float(time) : NULL;
	double delay = secs ? PyFloat_AsDouble(secs) : 0.0;
	Py_XDECREF(secs);
	PyObject *r = NULL;
	if (player > 0) {
		PyObject *a = Py_BuildValue("(O)", e);
		if (now) {
			r = a ? CharAct_PlayerDeathCall(NULL, a) : NULL;
			Py_XDECREF(a);
		} else {
			PyObject *f = Script_Attr("CharAct", "PlayerDeathCall");
			r = f && Script_Later(delay, f, a, NULL) ? Py_ReturnNone() : NULL;
			if (!f) {
				Py_XDECREF(a);
			}
			Py_XDECREF(f);
		}
	} else if (player == 0 && Script_HSet(e, "UseFunc", PyString_FromString(""))) {
		PyObject *a = Py_BuildValue("(O)", e);
		if (now) {
			r = a ? CharAct_NPCDeathCall(NULL, a) : NULL;
			Py_XDECREF(a);
		} else if (Script_HSet(e, "ActiveCMask", PyInt_FromLong(0)) && Script_HSet(e, "VRadius", PyInt_FromLong(0))) {
			PyObject *f = Script_Attr("CharAct", "NPCDeathCall");
			r = f && Script_Later(delay, f, a, NULL) ? Py_ReturnNone() : NULL;
			if (!f) {
				Py_XDECREF(a);
			}
			Py_XDECREF(f);
		} else {
			Py_XDECREF(a);
		}
	}
	Py_XDECREF(two);
	return r;
}

/* OnDeath(LooserName, WinnerName): the death of a character with its effect */
static PyObject *CharAct_OnDeath(PyObject *self, PyObject *args) {
	PyObject *looser, *winner;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &looser, &winner)) {
		return NULL;
	}
	PyObject *e = Script_Ask("Scrap", "GetEntity", "(O)", looser);
	PyObject *fx = e && Script_HSet(e, "ArrowCanUse", PyInt_FromLong(0)) && Script_HSet(e, "AutoSelect", PyInt_FromLong(0)) ?
		Script_Method(e, "SetFX", "(s)", "CharacterDeath") : NULL;
	PyObject *a = fx ? Py_BuildValue("(OO)", e, winner) : NULL;
	PyObject *r = a ? CharAct_NoFXDeath(NULL, a) : NULL;
	Py_XDECREF(a);
	Py_XDECREF(fx);
	Py_XDECREF(e);
	return r;
}

PyMethodDef g_CharActMethods[] = {
	{ "ActionPhoto", CharAct_ActionPhoto, 1, NULL },
	{ "Photograph", CharAct_Photograph, 1, NULL },
	{ "OnDeath", CharAct_OnDeath, 1, NULL },
	{ "NoFXDeath", CharAct_NoFXDeath, 1, NULL },
	{ "PlayerDeathCall", CharAct_PlayerDeathCall, 1, NULL },
	{ "NPCDeathCall", CharAct_NPCDeathCall, 1, NULL },
	{ "OnCreate", CharAct_OnCreate, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
