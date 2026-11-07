/*
 * The CharAct module in C (scripts/chars/charact.py, issue #2 step 7a): what every character shares. OnCreate (the
 * death callback, the morphing eyes), the photo of the player's camera action, and the death of a character: the
 * player's goes inactive, an NPC respawns and may drop a battery or coins. The hooks the missions set
 * (TakenPhoto, TakenPhotoMap, onNPCDeath) and the module code (EnergyRechargeTime) stay in the module.
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values. The script passes entity
 * handles to the scheduled calls (PlayerDeathCall, NPCDeathCall), so the characters are handle values (OBJLIB 7) and
 * the values the script reads from them (e.Pos, e.Money / 4) go on to the functions it calls. The hooks are read as
 * the module attributes they are and called as the values they hold. Scrap.GetEntity, the scheduler, SSound.Play and
 * SScorer.Fade and SetMsgText go through their seams (OBJLIB 7: the cases record or swap them); CrazyWing.CrazyPhoto,
 * CharsNPC.CharRes and the imports stay by name (OBJLIB 3, 5, 6).
 */
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

static byte CA_PlayerDeathCall(const nativeVal_t *e);
static byte CA_NPCDeathCall(const nativeVal_t *e);

/* OnCreate(EntityName): the standard initializer of a character */
static byte CA_OnCreate(const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *eyes = Mode_V(&t), *e = Mode_V(&t), *act = Mode_V(&t);
	if (!LvN_ModGet("MakeChar", "MorphEyesList", eyes) || !Mode_EntitySeam(name, e) ||
		!Mode_VSet(e, "OnDeath", Mode_S(&t, "CharAct.OnDeath")) || !Mode_VObj(e, "ActCtrl", act)) {
		return Mode_Done(&t, 0);
	}
	int morph = Mode_In(eyes, act);
	return Mode_Done(&t, morph >= 0 && (!morph || Mode_VSet(e, "Action", Mode_S(&t, "MorphEyes"))));
}

/* a hook of the module (TakenPhoto, TakenPhotoMap) on the photo: 1 when it counts, -1 with the error set */
static int CA_PhotoHook(const char *hook, const nativeVal_t *sel) {
	nativeVal_t f = { 0 }, r = { 0 };
	if (!LvN_ModGet("CharAct", hook, &f)) {
		return -1;
	}
	int c = 0;
	if (Native_True(&f)) {
		c = Mode_CallArgs(&f, &r, sel, 1) ? Native_True(&r) : -1;
	}
	Native_Free(&r);
	Native_Free(&f);
	return c;
}

/* Photograph(SelectedEntity): the missions' photo hooks and a crazy character; the sound, the flash and the end of
 * the first photo message when one counted */
static byte CA_Photograph(const nativeVal_t *sel) {
	static const int white[4] = { 255, 255, 255, 255 }, clear[4] = { 255, 255, 255, 0 };
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *act = Mode_V(&t), *head = Mode_V(&t), *r = Mode_V(&t);
	int map = CA_PhotoHook("TakenPhotoMap", sel);
	int taken = map >= 0 ? CA_PhotoHook("TakenPhoto", sel) : -1;
	if (taken < 0) {
		return Mode_Done(&t, 0);
	}
	byte photo = map || taken;
	int walk = Mode_EntitySeam(sel, e) ? Mode_VIsType(e, "WalkChar") : -1;
	if (walk < 0) {
		return Mode_Done(&t, 0);
	}
	if (walk) {
		if (!Mode_VObj(e, "Action", act) || !Native_Slice(act, 0, 5, head)) {
			return Mode_Done(&t, 0);
		}
		if (Mode_IsText(head, "Crazy")) {
			if (!Native_CallMod("CrazyWing", "CrazyPhoto", r, "v", e)) {
				return Mode_Done(&t, 0);
			}
			if (Native_True(r)) {
				photo = 1;
			}
		}
	}
	if (photo) {
		if (!Mode_Sound("PhotoSound", "PhotoVolume") || !Mode_FadeV(0, white, clear, Mode_I(&t, 1))) {
			return Mode_Done(&t, 0);
		}
		SaveVar_Set("TakePhotoMsg", "0");
		GVar_SetStr("FirstPhotoMessage", "");
	}
	return Mode_Done(&t, 1);
}

/* ActionPhoto(EntityName): the camera action of the player, on the selected entity */
static byte CA_ActionPhoto(const nativeVal_t *name) {
	nativeVal_t e = { 0 }, sel = { 0 };
	byte ok = Mode_EntitySeam(name, &e) && Mode_VObj(&e, "SelectedEntity", &sel) && CA_Photograph(&sel);
	Native_Free(&e);
	Native_Free(&sel);
	return ok;
}

/* PlayerDeathCall(e): the dead player goes inactive */
static byte CA_PlayerDeathCall(const nativeVal_t *e) {
	return !Native_True(e) || LvN_SetI(e, "Active", 0);
}

/* NPCDeathCall(e): the NPC respawns in 6 s; a battery (Gear) or coins (a quarter of its money) unless the mission's
 * onNPCDeath took care; the message of a mission character; the entity goes */
static byte CA_NPCDeathCall(const nativeVal_t *e) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *res = Mode_V(&t), *name = Mode_V(&t), *entry = Mode_V(&t), *hook = Mode_V(&t), *r = Mode_V(&t);
	nativeVal_t *money = Mode_V(&t), *ctrl = Mode_V(&t), *pos = Mode_V(&t), *msg = Mode_V(&t), *radar = Mode_V(&t);
	nativeVal_t *args = Mode_V(&t);
	Mode_V(&t);
	if (!Mode_Import("CharsNPC") || !Mode_Import("MisItems") || !LvN_ModGet("CharsNPC", "CharRes", res) ||
		!Mode_VObj(e, "Name", name) || !Mode_Sub(res, name, entry) ||
		!Mode_LaterIn(6.0, "CharsNPC", "Respawn", entry, 1, NULL) || !LvN_ModGet("CharAct", "onNPCDeath", hook)) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(r, 0);
	if (Native_True(hook)) {
		Native_Copy(&args[0], name);
		if (!Mode_VObj(e, "Pos", &args[1]) || !Mode_CallArgs(hook, r, args, 2)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_VObj(e, "Money", money)) {
		return Mode_Done(&t, 0);
	}
	if (Mode_CmpI(money, "!=", 0) > 0) {
		if (!Mode_VObj(e, "ActCtrl", ctrl) || !Mode_VObj(e, "Pos", pos)) {
			return Mode_Done(&t, 0);
		}
		if (Mode_IsText(ctrl, "Gear")) {
			if (!Mode_LaterIn(0.5, "MisItems", "CreateBattery", pos, 1, NULL)) {
				return Mode_Done(&t, 0);
			}
		} else if (Mode_CmpI(r, "==", 0) > 0 && Mode_CmpI(money, ">", 2) > 0) {
			Native_Free(&args[0]);
			Native_Free(&args[1]);
			Native_Copy(&args[0], pos);
			if (!Mode_Arith('/', money, Mode_I(&t, 4), &args[1]) ||
				!Mode_LaterIn(0.5, "MisItems", "CreateCoin", args, 2, NULL)) {
				return Mode_Done(&t, 0);
			}
		}
	}
	if (!Mode_VObj(e, "RadarShow", radar)) {
		return Mode_Done(&t, 0);
	}
	if (Mode_CmpI(radar, "==", 1) > 0) {
		Mode_Lang("MainMissionCharDeath", msg);
		if (!Mode_MsgText(msg, Mode_Time() + 5)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, Mode_VDel(e));
}

/* NoFXDeath(e, WinnerName, Time=2.0): the death without the effect, the call Time seconds later (now for 0) */
static byte CA_NoFXDeath(const nativeVal_t *e, const nativeVal_t *time) {
	nativeVal_t list = { 0 }, secs = { 0 };
	if (!Mode_Import("MissionsFuncs") || !Mode_VObj(e, "EntityList", &list)) {
		return 0;
	}
	byte player = Mode_IsText(&list, "Players");
	Native_Free(&list);
	byte now = Mode_CmpI(time, "==", 0) > 0;
	if (!Native_ToFloat(time, &secs)) {
		return 0;
	}
	double delay = secs.f;
	if (player) {
		return now ? CA_PlayerDeathCall(e) : Mode_LaterIn(delay, "CharAct", "PlayerDeathCall", e, 1, NULL);
	}
	if (!LvN_SetS(e, "UseFunc", "")) {
		return 0;
	}
	if (now) {
		return CA_NPCDeathCall(e);
	}
	return LvN_SetI(e, "ActiveCMask", 0) && LvN_SetI(e, "VRadius", 0) &&
		Mode_LaterIn(delay, "CharAct", "NPCDeathCall", e, 1, NULL);
}

/* OnDeath(LooserName, WinnerName): the death of a character with its effect */
static byte CA_OnDeath(const nativeVal_t *looser) {
	nativeVal_t e = { 0 }, two;
	Native_SetFloat(&two, 2.0);
	byte ok = Mode_EntitySeam(looser, &e) && LvN_SetI(&e, "ArrowCanUse", 0) && LvN_SetI(&e, "AutoSelect", 0) &&
		Mode_VSetFX(&e, "CharacterDeath") && CA_NoFXDeath(&e, &two);
	Native_Free(&e);
	return ok;
}

MODE_FN(CharAct, ActionPhoto, 1, CA_ActionPhoto(&a[0]))
MODE_FN(CharAct, Photograph, 1, CA_Photograph(&a[0]))
MODE_FN(CharAct, OnDeath, 2, CA_OnDeath(&a[0]))
MODE_FN(CharAct, PlayerDeathCall, 1, CA_PlayerDeathCall(&a[0]))
MODE_FN(CharAct, NPCDeathCall, 1, CA_NPCDeathCall(&a[0]))
MODE_FN(CharAct, OnCreate, 1, CA_OnCreate(&a[0]))

static PyObject *CharAct_NoFXDeath(PyObject *self, PyObject *args) {
	PyObject *e, *winner, *time = NULL;
	nativeVal_t ev, tv;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO|O", &e, &winner, &time)) {
		return NULL;
	}
	Native_Arg(e, &ev);
	if (time) {
		Native_Arg(time, &tv);
	} else {
		Native_SetFloat(&tv, 2.0);
	}
	byte ok = CA_NoFXDeath(&ev, &tv);
	Native_Free(&ev);
	Native_Free(&tv);
	return Mode_Result(ok);
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
