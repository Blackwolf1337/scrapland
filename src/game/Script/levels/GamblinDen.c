/*
 * The scripts of the level GamblinDen in C (levels/gamblinden/scripts, issue #2): the Map module (the characters of
 * the den by the save variables, the drinking and dancing mercenaries, the talking NPCs, the music and the hire of
 * the mercenaries) and the twelve missions of the level. The scenes are call tables (Mode_Calls). MapSnd is data
 * (data/levels/gamblinden/mapsnd.def). The module variables (Borrachera, Cheer, the flags MayorAndBanker, Twins,
 * Bankers, Bishops, Polices, BandH, BandB, the characters the missions keep, their counters) and the module code (the
 * save variables, the alarm off) stay in the scripts.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on C values, read the globals of their module where the
 * script reads them and write them through at once (OBJLIB 5), and call the engine in C: the save variables, the
 * language texts, the entities and their variables, the dummy search, the time. The engine functions the cases record
 * or swap go through their seams: Scrap.AddScheduledFunc, Scrap.Rand, Scrap.UsrEntity, Scrap.Set,
 * SScorer.SetMsgText, SScorer.Set, SScorer.Fade, SSound.SetMusic, SSound.SetMusicVolume. Entities the script holds are
 * handle values (OBJLIB 7). CharsNPC.CharRes and the other variables of other modules that hold objects are read as
 * objects (OBJLIB 5).
 *
 * Direct calls (stage 3): the other ports (Speech, CharsNPC, Chars, Elevator, MissionsFuncs, Scene, RaceMaker, Menu) and
 * the C functions of the module go through their refs (PORT_CALL, PORT_LOWN, LV_ONEND, LV_COM; the arrange and
 * mercenary talks take the ref of their end function; Map.CreateFixedInit has its entry on Orbit.c's
 * Level_FixedInitN). The call tables of the scenes and apply call a port's function through its entry (Mode_Calls,
 * Mode_ApplyNext), and so do the scheduled calls (Mode_Later, Mode_LaterV, the remote message of
 * LaunchDeepThroatMessage; Port_Scheduled). By name stay the functions set as callbacks (CallbackFunc, AfterBrowseNPC,
 * EndOfSceneCallback, DialogController, SpeechRoutines; OBJLIB 5).
 */
#include <string.h>

#include "scrap/input.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/sound.h"
#include "Levels.h"

/* the level of the refs of this file's modules (PORT_LCALL, PORT_LOWN) */
#define PORT_LEVEL gamblinden

#define BF "Mission_BeforeFinalFight"
#define AF "Mission_AfterFinalFight"
#define AI "Mission_ArrangeIgorCombat"
#define BC "Mission_ArrangeBankersCTF"
#define BK "Mission_ArrangeBankersCombat"
#define IF "Mission_IgorFirstContact"
#define GG "Mission_GoToGamblinDen"
#define TT "Mission_TalkToTraitoriusAfterCTF"
#define BM "Mission_BackFromMortalRace"
#define CF "Mission_CrazyGamblerFirstContact"
#define AM "Mission_ArrangeMortalRace"
#define TS "Mission_ThreeStooges"

/* ---- native helpers of this file ---- */

/* SSound.SetMusicVolume(volume) through its ref (the seam: fr6b.py records it) */
static byte Gd_MusicVolume(double volume) {
	return PORT_CALL(SSound, SetMusicVolume, NULL, "d", volume);
}

/* CharsNPC.CharRes[name].var into *out (any value: an object as NV_OBJ), and its set */
static byte Gd_ResGet(const nativeVal_t *name, const char *var, nativeVal_t *out) {
	nativeVal_t ch, v = { 0 };
	byte ok = Mode_RefTake(Mode_CharRes(name), &ch) && Mode_RefGetAttr(&ch, var, &v);
	if (ok) {
		Mode_RefPlain(&v, out);
	}
	Native_Free(&v);
	Native_Free(&ch);
	return ok;
}

static byte Gd_ResSet(const nativeVal_t *name, const char *var, const nativeVal_t *v) {
	nativeVal_t ch;
	byte ok = Mode_RefTake(Mode_CharRes(name), &ch) && Mode_VSet(&ch, var, v);
	Native_Free(&ch);
	return ok;
}

/* del CharsNPC.CharRes[name] */
static byte Gd_ResDel(const char *name) {
	nativeVal_t key = { 0 };
	LvN_Str(&key, name);
	byte ok = Mode_ModDictDel("CharsNPC", "CharRes", &key);
	Native_Free(&key);
	return ok;
}

/* a keyword function with the one optional argument kwname that the core does not read (usr=None, char=None): its
 * adapter and entry (MODE_KW) */
#define GD_KW1(pre, name, kwname, call) \
	MODE_KW(pre, name, "|O", kwname) { \
		return call; \
	}

/* ---- the lines the missions share ---- */

/* CharsNPC.CharRes[name].CallbackFunc.append(fn) with fn the function already read (a global or Map.X, its NameError or
 * AttributeError before the lookup of the character, as the script's lines were ported) */
static byte Gd_Callback(const char *name, const nativeVal_t *fn) {
	/* OBJLIB 5 and 7: the character object of CharsNPC and its list, by reference */
	nativeVal_t k = { 0 }, ch, cb = { 0 };
	LvN_Str(&k, name);
	byte ok = Mode_RefTake(Mode_CharRes(&k), &ch) && Mode_RefAttr(&ch, "CallbackFunc", &cb) &&
		Mode_RefCall(&cb, "append", fn, 1, NULL);
	Native_Free(&cb);
	Native_Free(&ch);
	Native_Free(&k);
	return ok;
}

/* the same with fn the global func of module */
static byte Gd_CallbackG(const char *name, const char *module, const char *func) {
	nativeVal_t fn = { 0 };
	byte ok = LvN_GObj(module, func, &fn) && Gd_Callback(name, &fn);
	Native_Free(&fn);
	return ok;
}

/* CharsNPC.CreateMissionChar(name) with the callback fn (read before) */
static byte Gd_MissionChar(const char *name, const nativeVal_t *fn) {
	return PORT_CALL(CharsNPC, CreateMissionChar, NULL, "s", name) && Gd_Callback(name, fn);
}

/* the same with the global func of module */
static byte Gd_MissionCharG(const char *name, const char *module, const char *func) {
	nativeVal_t fn = { 0 };
	byte ok = LvN_GObj(module, func, &fn) && Gd_MissionChar(name, &fn);
	Native_Free(&fn);
	return ok;
}

/* the characters of the den the mission wants: MayorAtGambinDen, FunctionaryTwins..., Bankers..., Bishops...,
 * Polices... (NULL keeps a variable) */
static void Gd_Den(const char *mayor, const char *twins, const char *bankers, const char *bishops, const char *polices) {
	static const char *const names[] = { "MayorAtGambinDen", "FunctionaryTwinsAtGambinDen", "BankersAtGambinDen", "BishopsAtGambinDen",
		"PolicesAtGambinDen" };
	const char *vals[] = { mayor, twins, bankers, bishops, polices };
	for (int i = 0; i < 5; i++) {
		if (vals[i]) {
			SaveVar_Set(names[i], vals[i]);
		}
	}
}

/* `if optId == "Talk": Speech.OnSpch(msg, ans)` with the names of the talk set first (names 0: not set); a = the four
 * arguments of a talk (UsrName, CharName, optId, FullOption) */
static byte Gd_Talk(const nativeVal_t *a, int names, const char *msg, const char *ans) {
	if (names && (!Mode_Import("Speech") || !LvN_Speakers(&a[0], &a[1]))) {
		return 0;
	}
	return !LvN_EqS(&a[2], "Talk") || Mode_Spch(msg, ans, NULL);
}

/* prefix + suffix into buf */
static const char *Gd_Key(char *buf, int size, const char *prefix, const char *suffix) {
	Str_Printf(buf, size, "%s%s", prefix, suffix);
	return buf;
}

/*
 * CrazyGamblerSpeech of the missions that arrange a deal with the crazy gambler: the first line and option _1, then
 * _1_1 (accept: the movie scorer, no input, the launch function of the mission, its ref, at the end of the talk) or _1_2
 */
static byte Gd_Arrange(const nativeVal_t *a, const char *prefix, const portRef_t *launch) {
	char k1[0x80], k2[0x80], k3[0x80];
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (LvN_EqS(&a[2], "Talk")) {
		const char *const ans[] = { Gd_Key(k3, sizeof(k3), prefix, "_1"), NULL };
		return Mode_Spch(Gd_Key(k1, sizeof(k1), prefix, "_InitMsg1"), Gd_Key(k2, sizeof(k2), prefix, "_InitAns1"),
			NULL) && Mode_AnsLst(ans, -1);
	}
	if (!PORT_CALL(Speech, StdOnSpch, NULL, "v", &a[2])) {
		return 0;
	}
	if (LvN_EqS(&a[2], Gd_Key(k1, sizeof(k1), prefix, "_1"))) {
		const char *const ans[] = { Gd_Key(k2, sizeof(k2), prefix, "_1_1"), Gd_Key(k3, sizeof(k3), prefix, "_1_2"), NULL };
		return Mode_AnsLst(ans, -1);
	}
	if (LvN_EqS(&a[2], Gd_Key(k2, sizeof(k2), prefix, "_1_1"))) {
		if (!Mode_Import("Menu") || !PORT_CALL(Menu, MovieScorer, NULL, "i", 0L)) {
			return 0;
		}
		Mode_SetActionSet("Inactive");
		return LvN_OnEndRef(launch, NULL);
	}
	return 1;
}

/*
 * the talk of a mercenary in the missions of the three mercenaries: in its own mission the line _1, then _1_1 and
 * _1_1_1 (with end, the ref of the mission's end function, params (CharName,) when withChar), else a single line
 * (otherMsg, otherAns); convinced: the global of module that turns the first line into the second
 */
typedef struct {
	const char *library, *prefix, *otherMsg, *otherAns;
	const portRef_t *end;
	const char *convinced;
	byte withChar;
} gdMerc_t;

static byte Gd_Merc(const nativeVal_t *a, const char *module, const gdMerc_t *m) {
	char k1[0x80], k2[0x80], k3[0x80];
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (!LvN_SaveIs("Mission.Library", m->library)) {
		return Mode_Spch(m->otherMsg, m->otherAns, NULL);
	}
	if (LvN_EqS(&a[2], "Talk")) {
		int first = 1;
		if (m->convinced) {
			first = LvN_GTrue(module, m->convinced);
			if (first < 0) {
				return 0;
			}
			first = !first;
		}
		if (first) {
			const char *const ans[] = { Gd_Key(k3, sizeof(k3), m->prefix, "_1"), NULL };
			return Mode_Spch(Gd_Key(k1, sizeof(k1), m->prefix, "_InitMsg1"),
				Gd_Key(k2, sizeof(k2), m->prefix, "_InitAns1"), NULL) && Mode_AnsLst(ans, -1);
		}
		return Mode_Spch(Gd_Key(k1, sizeof(k1), m->prefix, "_InitMsg2"),
			Gd_Key(k2, sizeof(k2), m->prefix, "_InitAns2"), NULL);
	}
	if (!PORT_CALL(Speech, StdOnSpch, NULL, "v", &a[2])) {
		return 0;
	}
	Gd_Key(k1, sizeof(k1), m->prefix, "_1");
	Gd_Key(k2, sizeof(k2), m->prefix, "_1_1");
	Gd_Key(k3, sizeof(k3), m->prefix, "_1_1_1");
	if (LvN_EqS(&a[2], k1)) {
		const char *const ans[] = { k2, k1, NULL };
		return Mode_AnsLst(ans, -1);
	}
	if (LvN_EqS(&a[2], k2)) {
		const char *const ans[] = { k3, k1, k2, NULL };
		return Mode_AnsLst(ans, -1);
	}
	if (LvN_EqS(&a[2], k3)) {
		const char *const ans[] = { k3, NULL };
		if (!Mode_AnsLst(ans, -1)) {
			return 0;
		}
		if (m->end) {
			nativeVal_t p = { 0 };
			Native_SetEmpty(&p, NV_TUPLE);
			Native_Copy(Native_Append(&p), &a[1]);
			byte ok = LvN_OnEndRef(m->end, m->withChar ? &p : NULL);
			Native_Free(&p);
			return ok;
		}
	}
	return 1;
}

/* target in front of ref, facing it (the talks that start right after a level load): target.Pos = SVec.Add(ref.Pos,
 * SVec.Prod(SVec.Rotate3D((0.0, 0.0, -1.0), ref.Ang), ref.VRadius + target.VRadius + 50)); target.Ang = (ref.Ang[0],
 * ref.Ang[1] + 180, ref.Ang[2]) */
static byte Gd_Face(const nativeVal_t *target, const nativeVal_t *ref) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ang = Mode_V(&t), *dir = Mode_V(&t), *pos = Mode_V(&t), *r1 = Mode_V(&t), *r2 = Mode_V(&t);
	nativeVal_t *sum = Mode_V(&t), *d = Mode_V(&t), *prod = Mode_V(&t), *np = Mode_V(&t), *na = Mode_V(&t);
	nativeVal_t *front = Mode_V(&t);
	Native_SetVec(front, 0.0, 0.0, -1.0);
	if (!Mode_VObj(ref, "Ang", ang) || !Mode_Rotate(front, ang, dir) || !Mode_VObj(ref, "Pos", pos) ||
		!Mode_VObj(ref, "VRadius", r1) || !Mode_VObj(target, "VRadius", r2) || !Native_Arith('+', r1, r2, sum) ||
		!Native_Arith('+', sum, Mode_I(&t, 50), d) || !Mode_VecProd(dir, d, prod) || !Mode_VecAdd(pos, prod, np) ||
		!Mode_VSet(target, "Pos", np)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(na, NV_TUPLE);
	for (int i = 0; i < 3; i++) {
		nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t);
		Native_Free(ang);
		if (!Mode_VObj(ref, "Ang", ang) || !LvN_Item(ang, i, x)) {
			return Mode_Done(&t, 0);
		}
		if (i == 1 && !Native_Arith('+', x, Mode_I(&t, 180), y)) {
			return Mode_Done(&t, 0);
		}
		Native_Copy(Native_Append(na), i == 1 ? y : x);
	}
	return Mode_Done(&t, Mode_VSet(target, "Ang", na));
}

/* Speech.PlayerName = usr.Name; Speech.NPCName = npc.Name (in the order of the script) */
static byte Gd_SpeakersOf(const nativeVal_t *usr, const nativeVal_t *npc) {
	nativeVal_t n = { 0 };
	Native_SetNone(&n);
	byte ok = Mode_VObj(usr, "Name", &n) && Mode_SetModVar("Speech", "PlayerName", &n);
	Native_Free(&n);
	ok = ok && Mode_VObj(npc, "Name", &n) && Mode_SetModVar("Speech", "NPCName", &n);
	Native_Free(&n);
	return ok;
}

/* MissionsFuncs.StartSuperDeal(map, library, mode) after the description desc */
static byte Gd_Launch(const char *desc, const char *map, const char *library, const char *mode) {
	return Mode_Import("MissionsFuncs") && Mode_Import("SaveGame") && Mode_MMDesc(desc, 1L) &&
		PORT_CALL(MissionsFuncs, StartSuperDeal, NULL, "sss", map, library, mode);
}

/* var = Scrap.GetEntity(name) of the module (the handle) */
static byte Gd_PutEnt(const char *module, const char *var, const char *name) {
	nativeVal_t h = { 0 };
	byte ok = LvN_Entity(name, &h) && Mode_Put(module, var, NULL, &h);
	Native_Free(&h);
	return ok;
}

/* var.Descriptor = Scrap.GetLangStr(key) of a global var of the module (the text first, as the script) */
static byte Gd_GDescriptor(const char *module, const char *var, const char *key) {
	nativeVal_t h = { 0 }, s = { 0 };
	Mode_Lang(key, &s);
	byte ok = Mode_G(module, var, &h) && Mode_VSet(&h, "Descriptor", &s);
	Native_Free(&h);
	Native_Free(&s);
	return ok;
}

/* MissionsFuncs.SetMissionTarget(var.Name) of a global var of the module */
static byte Gd_GTarget(const char *module, const char *var) {
	nativeVal_t h = { 0 }, n = { 0 };
	Native_SetNone(&n);
	byte ok = Mode_G(module, var, &h) && Mode_VObj(&h, "Name", &n) && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &n);
	Native_Free(&h);
	Native_Free(&n);
	return ok;
}

/* Speech.SpeechRoutines["CrazyGambler"] = the global func of module */
static byte Gd_RoutineG(const char *module, const char *func) {
	nativeVal_t fn = { 0 };
	byte ok = LvN_GObj(module, func, &fn) && LvN_Routine("CrazyGambler", &fn);
	Native_Free(&fn);
	return ok;
}

/* DTritusDesktop.DialogController = the global func of module */
static byte Gd_Dialog(const char *module, const char *func) {
	return Mode_Import("DTritusDesktop") && LvN_ModSetGlobal("DTritusDesktop", "DialogController", module, func);
}

/* ---- Map ---- */

/* if vl in CharConversor.AvailableChars: CharConversor.AvailableChars.remove(vl) */
static byte Den_NotAvailable(const char *name) {
	nativeVal_t vl = { 0 }, av = { 0 };
	LvN_Str(&vl, name);
	int in = LvN_ModGet("CharConversor", "AvailableChars", &av) ? Native_Contains(&av, &vl) : -1;
	byte ok = in >= 0 && (!in || LvN_ModMethod("CharConversor", "AvailableChars", "remove", &vl));
	Native_Free(&vl);
	Native_Free(&av);
	return ok;
}

/* InitStatus(LevelPath): the characters that cannot be possessed here, indoors, the mission characters, the elevator
 * the player came by */
static byte Den_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mods[] = { "Init", "CharsNPC", "Elevator", "string", "CharConversor", "Chars", NULL };
	static const char *const outs[] = { "Nurse", "Desktop", "Sentinel", "Gear", "Messenger", "Betty", "Mayor", "Berto", "PoliceBoss", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *v = Mode_V(&t), *fn = Mode_V(&t), *pv = Mode_V(&t), *sl = Mode_V(&t), *pm = Mode_V(&t);
	(void)a;
	(void)out;
	for (int i = 0; mods[i]; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	for (int i = 0; outs[i]; i++) {
		if (!Den_NotAvailable(outs[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!LvN_ModGet("Init", "isDemo", v) || (Native_True(v) && !Den_NotAvailable("BankMaster"))) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_SetModVar("Init", "Outdoor", Mode_I(&t, 0)) || !Mode_SetModVar("Init", "OutArea", Mode_S(&t, "Outskirts")) ||
		!LvN_GObj("Map", "AfterBrowseNPC", fn) || !LvN_ModAppend("CharsNPC", "AfterBrowseNPC", fn)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_SaveIs("LonelyMercInGamblinDen", "1")) {
		if (!PORT_CALL(CharsNPC, CreateMissionChar, NULL, "s", "MercenaryA_LonelyMerc") ||
			!PORT_CALL(Chars, AddNewCharRes, NULL, "s", "MercenaryL") ||
			!Gd_ResSet(Mode_S(&t, "MercenaryA_LonelyMerc"), "ActCtrl", Mode_S(&t, "MercenaryL")) ||
			!Gd_CallbackG("MercenaryA_LonelyMerc", "Map", "CreateLonelyMerc")) {
			return Mode_Done(&t, 0);
		}
	}
	if (LvN_SaveIs("PoliceBossAtPolice", "1") && !Gd_MissionCharG("PoliceBoss_Default", "Map", "CreateFixedInit")) {
		return Mode_Done(&t, 0);
	}
	if (LvN_SaveIs("CrazyGamblerAtGambinDen", "1") && !Gd_MissionCharG("CrazyGambler_Default", "Map", "CreateFixedInit")) {
		return Mode_Done(&t, 0);
	}
	/* pm = string.upper(Scrap.GetSaveVar("PrevMap")[7:]) */
	Mode_SaveVar("PrevMap", pv);
	if (!Native_Slice(pv, 7, 0x7fffffff, sl) || !Native_Upper(sl, pm)) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, PORT_CALL(Elevator, SetDef, NULL, "ss", "Elevator",
		LvN_EqS(pm, "SCRAPYARD") ? "DM_Door_Elevator_Scrapyard" : "DM_Door_Elevator_Outskirts"));
}

/* global name = (save var a == "1"), and when b: and save var b (== "1", or != "1" when notB): the value */
static int Den_Flag(const char *name, const char *a, const char *b, int notB) {
	int v = LvN_SaveIs(a, "1");
	if (v && b) {
		v = notB ? !LvN_SaveIs(b, "1") : LvN_SaveIs(b, "1");
	}
	return LvN_PutI("Map", name, v) ? v : -1;
}

/* key in CharsNPC.CharRes.keys(): 1 or 0, -1 with the error */
static int Den_HasChar(const char *key) {
	nativeVal_t k = { 0 }, x = { 0 };
	LvN_Str(&x, key);
	Native_SetNone(&k);
	int r = Mode_Keys("CharsNPC", "CharRes", 0, &k) ? Native_Contains(&k, &x) : -1;
	Native_Free(&k);
	Native_Free(&x);
	return r;
}

/* global name = (a in CharsNPC.CharRes.keys()) and (b in ...): the value */
static int Den_Pair(const char *name, const char *a, const char *b) {
	int v = Den_HasChar(a);
	if (v > 0) {
		v = Den_HasChar(b);
	}
	return v >= 0 && LvN_PutI("Map", name, v) ? v : -1;
}

/* AfterBrowseNPC(): the mercenaries at the bar with their dance places, the friendly bishop and functionary, the pairs
 * of characters the save variables put in the den */
static byte Den_AfterBrowseNPCC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mercs[][2] = { { "MercenaryA_Smartie", "DM_DancingZone_MercenaryA" },
		{ "MercenaryB_Dumber", "DM_DancingZone_MercenaryB_Dumber" }, { "MercenaryC_Brutus", "DM_DancingZone_MercenaryC_Brutus" } };
	static const struct {
		const char *flag, *a, *b;
		int notB;
		const char *chars[2];
	} pairs[] = {
		{ "MayorAndBanker", "MayorAtGambinDen", "MayorAtTownHall", 0, { "BankDirector_Barman", "Mayor_Drunk" } },
		{ "Twins", "FunctionaryTwinsAtGambinDen", NULL, 0, { "Functionary_Twins_01", "Functionary_Twins_02" } },
		{ "Bankers", "BankersAtGambinDen", NULL, 0, { "BankDirector_00", "BankDirector_01" } },
		{ "Bishops", "BishopsAtGambinDen", NULL, 0, { "Bishop_00", "Bishop_01" } },
		{ "Polices", "PolicesAtGambinDen", "LonelyMercInGamblinDen", 1, { "Police_00", "Police_01" } },
	};
	(void)a;
	(void)out;
	if (!Mode_Import("CharsNPC")) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		nativeVal_t name = { 0 }, d = { 0 }, p = { 0 };
		if (!Gd_CallbackG(mercs[i][0], "Map", "CreateFixedMercInit")) {
			return 0;
		}
		Mode_DummyStart(mercs[i][1], 0);
		LvN_Str(&name, mercs[i][0]);
		nativeVal_t res;
		byte ok = Mode_RefTake(Mode_CharRes(&name), &res);
		Native_Free(&name);
		if (!ok) {
			return 0;
		}
		Mode_DummyNext(&d);
		ok = LvN_Item(&d, 1, &p) && Mode_VSet(&res, "DancePos", &p);
		Native_Free(&res);
		Native_Free(&d);
		Native_Free(&p);
		if (!ok) {
			return 0;
		}
	}
	if (LvN_SaveIs("RobotsControlledByBoss", "1")) {
		if (!Gd_ResDel("Bishop_Friendly35") || !Gd_ResDel("Functionary_Friendly00")) {
			return 0;
		}
	} else if (!Gd_CallbackG("Bishop_Friendly35", "Map", "CreateFixedInit") ||
		!Gd_CallbackG("Functionary_Friendly00", "Map", "CreateFixedInit")) {
		return 0;
	}
	for (int i = 0; i < 5; i++) {
		int v = Den_Flag(pairs[i].flag, pairs[i].a, pairs[i].b, pairs[i].notB);
		if (v < 0 || (v && (!Gd_MissionCharG(pairs[i].chars[0], "Map", "CreateFixedInit") ||
			!Gd_MissionCharG(pairs[i].chars[1], "Map", "CreateFixedInit")))) {
			return 0;
		}
	}
	int v = Den_Pair("BandH", "Humphrey_Default", "Berto_Default");
	if (v < 0 || (v && (!Gd_CallbackG("Humphrey_Default", "Map", "CreateFixedInit") ||
		!Gd_CallbackG("Berto_Default", "Map", "CreateFixedInit")))) {
		return 0;
	}
	v = Den_Pair("BandB", "Betty_Default", "Berto_Default");
	return v >= 0 && (!v || (Gd_CallbackG("Betty_Default", "Map", "CreateFixedInit") &&
		Gd_CallbackG("Berto_Default", "Map", "CreateFixedInit")));
}

/* CreateFixedMercInit(e): fixed, the hire talk */
static byte Den_CreateFixedMercInitC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t line = { 0 };
	(void)out;
	if (!PORT_LOWN(Map, CreateFixedInit, NULL, "v", &a[0]) || !LvN_SetS(&a[0], "UseFunc", "Map.MercenarySpeech")) {
		return 0;
	}
	LvN_Str(&line, "---------------");
	byte ok = Mode_PrintV(&line);
	Native_Free(&line);
	return ok;
}

/* CreateLonelyMerc(e): fixed, his own talk */
static byte Den_CreateLonelyMercC(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return PORT_LOWN(Map, CreateFixedInit, NULL, "v", &a[0]) && LvN_SetS(&a[0], "UseFunc", "Map.LonelyMercSpeech");
}

/* Scrap.Rand(lo, hi) > 0 on two ints: 1 or 0, -1 with the error */
static int Den_RandUp(long lo, long hi) {
	nativeVal_t a = { 0 }, b = { 0 }, r = { 0 }, z = { 0 };
	LvN_Int(&a, lo);
	LvN_Int(&b, hi);
	LvN_Int(&z, 0);
	Native_SetNone(&r);
	int up = -1;
	if (Mode_Rand(&a, &b, &r)) {
		up = Native_Compare(&r, &z) > 0;
		if (Native_Failed()) {
			up = -1;
		}
	}
	Native_Free(&r);
	return up;
}

/* ChatNPCs(a, (a, b)) */
static byte Den_Chat(const char *a, const char *b) {
	nativeVal_t vals = { 0 };
	Native_SetEmpty(&vals, NV_TUPLE);
	LvN_Str(Native_Append(&vals), a);
	LvN_Str(Native_Append(&vals), b);
	byte ok = PORT_LOWN(Map, ChatNPCs, NULL, "sv", a, &vals);
	Native_Free(&vals);
	return ok;
}

/* `if Scrap.Rand(-1, 1) > 0: ChatNPCs(a, (a, b)) else: CharDance(a); CharDance(b)` */
static byte Den_ChatOrDance(const char *a, const char *b) {
	int chat = Den_RandUp(-1, 1);
	if (chat < 0) {
		return 0;
	}
	if (chat) {
		return Den_Chat(a, b);
	}
	return PORT_LOWN(Map, CharDance, NULL, "s", a) && PORT_LOWN(Map, CharDance, NULL, "s", b);
}

/* Init(LevelPath): the elevators, the life in the den, the music (another track each visit) */
static byte Den_InitC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const flags[] = { "MayorAndBanker", "Twins", "Bankers", "Bishops", "Polices", "BandH", "BandB" };
	static const char *const two[][2] = { { "BankDirector_Barman", "Mayor_Drunk" }, { "Functionary_Twins_01", "Functionary_Twins_02" },
		{ "BankDirector_00", "BankDirector_01" }, { "Bishop_00", "Bishop_01" }, { "Police_00", "Police_01" },
		{ "Berto_Default", "Humphrey_Default" }, { "Berto_Default", "Betty_Default" } };
	(void)a;
	(void)out;
	if (!Mode_Import("Elevator") || !Mode_Connect("DM_Door_Elevator_Outskirts", "Outskirts", 0) ||
		!Mode_Connect("DM_Door_Elevator_Scrapyard", "Scrapyard", 0) || !PORT_LOWN(Map, MercDrink, NULL, "s", "MercenaryA_Smartie") ||
		!PORT_LOWN(Map, MercDrink, NULL, "s", "MercenaryB_Dumber") || !PORT_LOWN(Map, MercDrink, NULL, "s", "MercenaryC_Brutus")) {
		return 0;
	}
	if (LvN_SaveIs("LonelyMercInGamblinDen", "1") && !PORT_LOWN(Map, MercDrink, NULL, "s", "MercenaryA_LonelyMerc")) {
		return 0;
	}
	if (!PORT_LOWN(Map, ChatNPCs, NULL, "s", "PoliceBoss_Default")) {
		return 0;
	}
	if (LvN_SaveIs("RobotsControlledByBoss", "0") && (!PORT_LOWN(Map, CharDance, NULL, "s", "Bishop_Friendly35") ||
		!PORT_LOWN(Map, CharDance, NULL, "s", "Functionary_Friendly00"))) {
		return 0;
	}
	if (!Mode_EntSetS("DM_Door_GamblinDenChupito_00", "OnObjSlot", "") || !Mode_EntSetI("DM_Door_Teleporter_00", "Active", 0)) {
		return 0;
	}
	for (int i = 0; i < 7; i++) {
		int on = LvN_GTrue("Map", flags[i]);
		if (on < 0 || (on && !(i == 0 || i >= 5 ? Den_Chat(two[i][0], two[i][1]) : Den_ChatOrDance(two[i][0], two[i][1])))) {
			return 0;
		}
	}
	if (LvN_SaveIs("Player.InfiniteLives", "1") && !Mode_EntSetS("DM_Door_Elevator_Scrapyard", "OnObjSlot", "!CloseDoorSlot")) {
		return 0;
	}
	if (LvN_SaveIs("Mission.Library", "Mission_CrazyGamblerFirstContact")) {
		return 1;
	}
	/* gm = not int(Scrap.GetSaveVar("GamblinMusic")) */
	nativeVal_t s = { 0 }, i = { 0 };
	Mode_SaveVar("GamblinMusic", &s);
	Native_SetNone(&i);
	byte ok = Native_ToInt(&s, &i);
	int gm = ok && !Native_True(&i);
	Native_Free(&s);
	Native_Free(&i);
	if (!ok) {
		return 0;
	}
	SaveVar_Set("GamblinMusic", gm ? "1" : "0");
	if (!gm) {
		return LvN_SetMusic("Music/GDloop2.ogg", 0.75);
	}
	int human = Den_RandUp(-1, 1);
	return human >= 0 && LvN_SetMusic(human ? "Music/HumanGD.ogg" : "Music/GDloop1.ogg", 0.75);
}

/* LonelyMercSpeech(UsrName, CharName, optId, FullOption): the lonely mercenary of the matrix mission */
static byte Den_LonelyMercSpeechC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const ans[] = { "Mission_FindDtritusMatrix1_Mercenary_1", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *r = Mode_V(&t), *msg = Mode_V(&t), *an = Mode_V(&t);
	(void)out;
	if (!Mode_Import("Speech") || !LvN_Speakers(&a[0], &a[1])) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqS(&a[2], "Talk")) {
		return Mode_Done(&t, PORT_CALL(Speech, OnSpch, r, "ss", "Mission_FindDtritusMatrix1_Mercenary_InitMsg1",
			"Mission_FindDtritusMatrix1_Mercenary_InitAns1") && (!Native_True(r) || Mode_AnsLst(ans, -1)));
	}
	return Mode_Done(&t, Native_Arith('+', &a[2], Mode_S(&t, "Msg"), msg) && Native_Arith('+', &a[2], Mode_S(&t, "Ans"), an) &&
		PORT_CALL(Speech, OnSpch, NULL, "vv", msg, an));
}

/* CharUsrAct(charname, ActName): an idle relaxing character plays the action: 1, else 0 */
static byte Den_CharUsrActC(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ch = Mode_V(&t), *idle = Mode_V(&t), *act = Mode_V(&t), *sl = Mode_V(&t);
	if (!Mode_EntityH(&a[0], ch) || !Mode_VObj(ch, "LLogicIdle", idle)) {
		return Mode_Done(&t, 0);
	}
	int go = LvN_EqI(idle, 0);
	if (go && (!Mode_VObj(ch, "Action", act) || !Native_Slice(act, 0, 5, sl))) {
		return Mode_Done(&t, 0);
	}
	go = go && LvN_EqS(sl, "Relax");
	if (go && !Mode_VSet(ch, "Action", &a[1])) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(out, go);
	return Mode_Done(&t, 1);
}

/* name in Map.Cheer, else appended to it: 0 with the error */
static byte Den_Cheer(const nativeVal_t *x, const nativeVal_t *name, int *was) {
	nativeVal_t cheer = { 0 };
	int in = Mode_G("Map", "Cheer", &cheer) ? Native_Contains(&cheer, x) : -1;
	Native_Free(&cheer);
	if (in < 0) {
		return 0;
	}
	*was = in;
	return in || Mode_Append("Map", "Cheer", name);
}

/* CharUsrDance(charname, state): a mercenary dances for state rounds of 5 s, then back to the bar */
static byte Den_CharUsrDanceC(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ch = Mode_V(&t), *idle = Mode_V(&t), *nm = Mode_V(&t), *rem = Mode_V(&t), *act = Mode_V(&t);
	nativeVal_t *next = Mode_V(&t), *pos = Mode_V(&t), *init = Mode_V(&t);
	int was;
	(void)out;
	if (!Mode_Import("CharsNPC") || !Mode_EntityH(&a[0], ch) || !Mode_VObj(ch, "LLogicIdle", idle)) {
		return Mode_Done(&t, 0);
	}
	int good = LvN_EqI(idle, 0);
	if (!Mode_VObj(ch, "Name", nm) || !Den_Cheer(nm, &a[0], &was)) {
		return Mode_Done(&t, 0);
	}
	int dance = Native_Compare(&a[1], Mode_I(&t, 0)) > 0;
	if (Native_Failed()) {
		return Mode_Done(&t, 0);
	}
	if (dance) {
		if (!Mode_Arith('%', &a[1], Mode_I(&t, 2), rem)) {
			return Mode_Done(&t, 0);
		}
		const char *anm = LvN_EqI(rem, 0) ? "Dance" : "Dance0";
		int go = good;
		if (go) {
			if (!Mode_VObj(ch, "Action", act)) {
				return Mode_Done(&t, 0);
			}
			go = !LvN_EqS(act, anm);
		}
		if (go && (!Mode_Cat(&t, init, 2, Mode_S(&t, anm), Mode_S(&t, "Init")) || !Mode_VSet(ch, "Action", init) ||
			!LvN_SetS(ch, "DefLoop", anm))) {
			return Mode_Done(&t, 0);
		}
		nativeVal_t args[2] = { 0 };
		if (!Native_Arith('-', &a[1], Mode_I(&t, 1), next)) {
			return Mode_Done(&t, 0);
		}
		args[0] = a[0];
		args[1] = *next;
		return Mode_Done(&t, Mode_Later(5.0, "Map", "CharUsrDance", args, 2, NULL));
	}
	return Mode_Done(&t, PORT_LOWN(Map, MercDrink, NULL, "vi", &a[0], 0L) && Gd_ResGet(&a[0], "Pos", pos) &&
		Mode_VSet(ch, "LLogicOrigin", pos));
}

/* Scrap.Rand(lo, hi) of two numbers (ints or floats as the script writes them) into *out */
static byte Den_Rand(const nativeVal_t *lo, const nativeVal_t *hi, nativeVal_t *out) {
	return Mode_Rand(lo, hi, out);
}

/* int(Scrap.Rand(lo, hi)) into *out */
static byte Den_RandInt(const nativeVal_t *lo, const nativeVal_t *hi, nativeVal_t *out) {
	nativeVal_t r = { 0 };
	Native_SetNone(&r);
	byte ok = Den_Rand(lo, hi, &r) && Native_ToInt(&r, out);
	Native_Free(&r);
	return ok;
}

/* MercDrink(merc, ctr=0): the drinkers of the bar: cheer, drink, talk, now and then one goes dancing */
static byte Den_MercDrinkC(const nativeVal_t *merc, const nativeVal_t *ctr0) {
	static const char *const xtracts[] = { "Talk1", "Talk0", "Talk1", "Talk0", "Talk1", "Talk0", "Relax0", "Relax1", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *mc = Mode_V(&t), *ac = Mode_V(&t), *ctr = Mode_V(&t), *r = Mode_V(&t), *b = Mode_V(&t), *res = Mode_V(&t);
	nativeVal_t *n = Mode_V(&t), *x = Mode_V(&t), *tbl = Mode_V(&t), *delay = Mode_V(&t);
	int was;
	Native_Copy(ctr, ctr0);
	if (!Mode_Import("CharsNPC") || !Mode_EntityH(merc, mc) || !Mode_VObj(mc, "ActCtrl", ac)) {
		return Mode_Done(&t, 0);
	}
	int islm = LvN_EqS(ac, "MercenaryL");
	if (!Den_Cheer(merc, merc, &was)) {
		return Mode_Done(&t, 0);
	}
	if (!was) {
		if (!PORT_LOWN(Map, CharUsrAct, NULL, "vs", merc, "Cheer")) {
			return Mode_Done(&t, 0);
		}
	} else {
		/* Scrap.Rand(-1.5, 1) > 0 or islm */
		Native_SetFloat(x, -1.5);
		if (!Den_Rand(x, Mode_I(&t, 1), r)) {
			return Mode_Done(&t, 0);
		}
		int busy = Native_Compare(r, Mode_I(&t, 0)) > 0;
		if (Native_Failed()) {
			return Mode_Done(&t, 0);
		}
		busy = busy || islm;
		Native_Free(r);
		if (busy) {
			int tired = Native_Compare(ctr, Mode_I(&t, 3)) >= 0;
			if (Native_Failed()) {
				return Mode_Done(&t, 0);
			}
			if (tired && !islm) {
				if (!Mode_G("Map", "Borrachera", b)) {
					return Mode_Done(&t, 0);
				}
				int drunk = Native_Compare(b, Mode_I(&t, 0)) > 0;
				if (Native_Failed()) {
					return Mode_Done(&t, 0);
				}
				Native_Free(b);
				if (drunk) {
					if (!Mode_G("Map", "Borrachera", b) || !Native_Arith('-', b, Mode_I(&t, 1), n) ||
						!Mode_Put("Map", "Borrachera", NULL, n)) {
						return Mode_Done(&t, 0);
					}
				} else {
					if (!Gd_ResGet(merc, "DancePos", res) || !Mode_VSet(mc, "LLogicOrigin", res) ||
						!Den_RandInt(Mode_I(&t, 10), Mode_I(&t, 20), n) ||
						!PORT_LOWN(Map, CharUsrDance, NULL, "vv", merc, n)) {
						return Mode_Done(&t, 0);
					}
					return Mode_Done(&t, LvN_PutI("Map", "Borrachera", 5));
				}
				Native_Free(ctr);
				if (!Den_RandInt(Mode_I(&t, 0), Mode_I(&t, 3), ctr)) {
					return Mode_Done(&t, 0);
				}
			} else {
				if (!PORT_LOWN(Map, CharUsrAct, NULL, "vs", merc, "Drink") || !Den_Rand(Mode_I(&t, -1), Mode_I(&t, 4), r)) {
					return Mode_Done(&t, 0);
				}
				int empty = Native_Compare(r, Mode_I(&t, 0)) < 0;
				if (Native_Failed()) {
					return Mode_Done(&t, 0);
				}
				if (empty) {
					nativeVal_t l = { 0 };
					Native_SetEmpty(&l, NV_LIST);
					byte ok = Mode_Put("Map", "Cheer", NULL, &l);
					Native_Free(&l);
					if (!ok) {
						return Mode_Done(&t, 0);
					}
				}
				Native_Free(n);
				if (!Native_Arith('+', ctr, Mode_I(&t, 1), n)) {
					return Mode_Done(&t, 0);
				}
				Native_Free(ctr);
				Native_Copy(ctr, n);
			}
		} else {
			/* CharUsrAct(merc, xtracts[int(Scrap.Rand(0, 7.9))]) */
			Native_SetEmpty(tbl, NV_TUPLE);
			for (int i = 0; xtracts[i]; i++) {
				LvN_Str(Native_Append(tbl), xtracts[i]);
			}
			Native_SetFloat(x, 7.9);
			if (!Den_RandInt(Mode_I(&t, 0), x, n) || !Native_Subscript(tbl, n, res) ||
				!PORT_LOWN(Map, CharUsrAct, NULL, "vv", merc, res)) {
				return Mode_Done(&t, 0);
			}
		}
	}
	nativeVal_t args[2] = { 0 };
	Native_SetFloat(x, 3.5);
	if (!Den_Rand(x, Mode_I(&t, 5), delay)) {
		return Mode_Done(&t, 0);
	}
	args[0] = *merc;
	args[1] = *ctr;
	return Mode_Done(&t, Mode_LaterV(delay, "Map", "MercDrink", args, 2, NULL));
}

/* the keyword adapter and entry of MercDrink(merc, ctr=0) */
MODE_KW(Den, MercDrink, "O|O", "merc", "ctr") {
	Mode_KwDefI(v, given, 1, 0);
	return Den_MercDrinkC(&v[0], &v[1]);
}

/* ChatNPCs(Name, Vals=("PoliceBoss_Default", "CrazyGambler_Default")): two characters talk in turn every 2.5 s */
static byte Den_ChatNPCsC(const nativeVal_t *name0, const nativeVal_t *vals) {
	static const char *const xtracts[] = { "Talk0", "Talk1", "Talk2" };
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *tbl = Mode_V(&t), *x = Mode_V(&t), *i = Mode_V(&t), *act = Mode_V(&t), *done = Mode_V(&t);
	nativeVal_t *v1 = Mode_V(&t), *name = Mode_V(&t);
	if (!Mode_EntityH(name0, e)) {
		return Mode_Done(&t, 0);
	}
	if (!Native_True(e)) {
		return Mode_Done(&t, 1);
	}
	Native_Copy(name, name0);
	Native_SetEmpty(tbl, NV_TUPLE);
	for (int k = 0; k < 3; k++) {
		LvN_Str(Native_Append(tbl), xtracts[k]);
	}
	Native_SetFloat(x, 2.9);
	if (!Den_RandInt(Mode_I(&t, 0), x, i) || !Native_Subscript(tbl, i, act) || !PORT_LOWN(Map, CharUsrAct, done, "vv", name, act)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(done)) {
		if (!LvN_Item(vals, 1, v1)) {
			return Mode_Done(&t, 0);
		}
		int second = !Native_Compare(v1, name);
		if (Native_Failed()) {
			return Mode_Done(&t, 0);
		}
		Native_Free(name);
		if (!LvN_Item(vals, second ? 0 : 1, name)) {
			return Mode_Done(&t, 0);
		}
	}
	nativeVal_t args[2] = { 0 };
	args[0] = *name;
	args[1] = *vals;
	return Mode_Done(&t, Mode_Later(2.5, "Map", "ChatNPCs", args, 2, NULL));
}

/* the keyword adapter and entry of ChatNPCs(Name, Vals=("PoliceBoss_Default", "CrazyGambler_Default")) */
MODE_KW(Den, ChatNPCs, "O|O", "Name", "Vals") {
	nativeVal_t vals = { 0 };
	if (!given[1]) {
		Native_SetEmpty(&vals, NV_TUPLE);
		LvN_Str(Native_Append(&vals), "PoliceBoss_Default");
		LvN_Str(Native_Append(&vals), "CrazyGambler_Default");
	}
	byte ok = Den_ChatNPCsC(&v[0], given[1] ? &v[1] : &vals);
	Native_Free(&vals);
	return ok;
}

/* CharDance(Name): a dancer between its two dances, every 5 to 10 s */
static byte Den_CharDanceC(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ch = Mode_V(&t), *act = Mode_V(&t), *delay = Mode_V(&t);
	(void)out;
	if (!Mode_EntityH(&a[0], ch) || !Mode_VObj(ch, "Action", act)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqS(act, "Dance")) {
		if (!LvN_SetS(ch, "Action", "Dance0") || !LvN_SetS(ch, "DefLoop", "Dance")) {
			return Mode_Done(&t, 0);
		}
	} else if (!PORT_LOWN(Map, CharUsrAct, NULL, "vs", &a[0], "Dance")) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Den_Rand(Mode_I(&t, 5), Mode_I(&t, 10), delay) && Mode_LaterV(delay, "Map", "CharDance", &a[0], 1, NULL));
}

/* MercenarySpeech(UsrName, CharName, optId, FullOption): hire a mercenary for three lives */
static byte Den_MercenarySpeechC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mods[] = { "Speech", "Chars", "CharsNPC", "Challenge", "Menu", "string", NULL };
	modeVals_t t = { .n = 0 };
	nativeVal_t *ch = Mode_V(&t), *usr = Mode_V(&t), *sv = Mode_V(&t), *lives = Mode_V(&t), *fv = Mode_V(&t), *rest = Mode_V(&t);
	nativeVal_t *msg = Mode_V(&t), *opt = Mode_V(&t), *sl = Mode_V(&t), *price = Mode_V(&t), *pr = Mode_V(&t), *rep = Mode_V(&t);
	nativeVal_t *params = Mode_V(&t);
	(void)out;
	for (int i = 0; mods[i]; i++) {
		if (!Mode_Import(mods[i])) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Mode_EntityH(&a[1], ch) || !LvN_Speakers(&a[0], &a[1])) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqS(&a[2], "Generics_SpecialMercs_1")) {
		int direct = 0;
		const char *ans = NULL;
		if (!Mode_UsrHandle(usr)) {
			return Mode_Done(&t, 0);
		}
		if (Native_True(usr)) {
			const char *s;
			Mode_SaveVar("Player.NumLives", sv);
			if (!Native_ToInt(sv, lives) || !Mode_Cat(&t, fv, 2, Mode_S(&t, "MercFriends."), &a[1]) || !Native_ArgStr(fv, 1, &s)) {
				return Mode_Done(&t, 0);
			}
			if (LvN_SaveIs(s, "1")) {
				ans = "Generics_SpecialMercs_2Ans";
				direct = 1;
			} else {
				int few = Native_Compare(lives, Mode_I(&t, 3)) < 0;
				if (Native_Failed()) {
					return Mode_Done(&t, 0);
				}
				if (few) {
					ans = "Generics_SpecialMercs_3Ans";
				} else {
					ans = "Generics_SpecialMercs_1Ans";
					Native_Free(fv);
					Native_SetEmpty(params, NV_TUPLE);
					Native_Copy(Native_Append(params), ch);
					if (!Native_Arith('-', lives, Mode_I(&t, 3), rest) || (Mode_Repr(rest, rep), !Mode_SetSaveVar("Player.NumLives", rep)) ||
						!Mode_Cat(&t, fv, 2, Mode_S(&t, "MercFriends."), &a[1]) || !Native_ArgStr(fv, 1, &s)) {
						return Mode_Done(&t, 0);
					}
					SaveVar_Set(s, "1");
					if (!LV_ONEND(Map, StartContract, params)) {
						return Mode_Done(&t, 0);
					}
				}
			}
		}
		if (!ans) {
			Native_Raise(NE_NAME, "ans");
			return Mode_Done(&t, 0);
		}
		LvN_Str(opt, ans);
		if (!Mode_Cat(&t, msg, 2, &a[2], Mode_S(&t, "Msg")) || !(direct ? PORT_CALL(Speech, OnSpch, NULL, "vv", msg, opt) :
			PORT_CALL(CharsNPC, PlayOption, NULL, "vvvvv", &a[2], msg, opt, &a[0], ch))) {
			return Mode_Done(&t, 0);
		}
	} else if (!PORT_CALL(CharsNPC, GenericNPCSpeech, NULL, "vvvv", &a[0], &a[1], &a[2], &a[3])) {
		return Mode_Done(&t, 0);
	}
	/* if ch.ArrOpt00[0:25] == "Generics_SpecialBishop_1:": the price of a life into the option */
	Native_Free(opt);
	if (!Mode_VObj(ch, "ArrOpt00", opt) || !Native_Slice(opt, 0, 25, sl)) {
		return Mode_Done(&t, 0);
	}
	if (!LvN_EqS(sl, "Generics_SpecialBishop_1:")) {
		return Mode_Done(&t, 1);
	}
	if (!Native_GVarGet("BishopLivePrice", price)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(price, pr);
	Native_Free(opt);
	if (!Mode_VObj(ch, "ArrOpt00", opt)) {
		return Mode_Done(&t, 0);
	}
	Mode_Replace(opt, "<n1>", pr, rep);
	return Mode_Done(&t, Mode_VSet(ch, "ArrOpt00", rep));
}

/* StartContract(ch): the hired mercenary celebrates */
static byte Den_StartContractC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t n = { 0 };
	(void)out;
	Native_SetNone(&n);
	byte ok = Mode_Import("Speech") && LvN_SetS(&a[0], "Action", "Drink") && LvN_SetS(&a[0], "DefLoop", "Dance") &&
		Mode_VObj(&a[0], "Name", &n) && PORT_CALL(Speech, RestoreState, NULL, "v", &n);
	Native_Free(&n);
	return ok;
}

LV_FN(Den, InitStatus, 1, Den_InitStatusC)
MODE_FN(Den, CreateFixedInit, 1, Level_FixedInitN(&a[0]))
LV_FN(Den, AfterBrowseNPC, 0, Den_AfterBrowseNPCC)
LV_FN(Den, CreateFixedMercInit, 1, Den_CreateFixedMercInitC)
LV_FN(Den, CreateLonelyMerc, 1, Den_CreateLonelyMercC)
LV_FN(Den, Init, 1, Den_InitC)
LV_FN(Den, LonelyMercSpeech, 4, Den_LonelyMercSpeechC)
LV_FN(Den, CharUsrAct, 2, Den_CharUsrActC)
LV_FN(Den, CharUsrDance, 2, Den_CharUsrDanceC)
LV_FN(Den, CharDance, 1, Den_CharDanceC)
LV_FN(Den, MercenarySpeech, 4, Den_MercenarySpeechC)
LV_FN(Den, StartContract, 1, Den_StartContractC)

PyMethodDef g_GamblinDenMapMethods[] = {
	{ "InitStatus", Den_InitStatus, 1, NULL },
	{ "AfterBrowseNPC", Den_AfterBrowseNPC, 1, NULL },
	{ "CreateFixedInit", Den_CreateFixedInit, 1, NULL },
	{ "CreateFixedMercInit", Den_CreateFixedMercInit, 1, NULL },
	{ "CreateLonelyMerc", Den_CreateLonelyMerc, 1, NULL },
	{ "Init", Den_Init, 1, NULL },
	{ "LonelyMercSpeech", Den_LonelyMercSpeech, 1, NULL },
	{ "CharUsrAct", Den_CharUsrAct, 1, NULL },
	{ "CharUsrDance", Den_CharUsrDance, 1, NULL },
	{ "MercDrink", (PyCFunction)(void (*)(void))Den_MercDrink, 3, NULL },
	{ "ChatNPCs", (PyCFunction)(void (*)(void))Den_ChatNPCs, 3, NULL },
	{ "CharDance", Den_CharDance, 1, NULL },
	{ "MercenarySpeech", Den_MercenarySpeech, 1, NULL },
	{ "StartContract", Den_StartContract, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_BeforeFinalFight: the crazy gambler's plan for the final fight ---- */

static byte Before_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const start[] = { "Spawn", "DM_Player_Spawn_Meeting_Room", "ComeFrom", "", "Char", "Dtritus", NULL };
	(void)a;
	(void)out;
	if (LvN_SaveIs("BeforeFinalFightPlayed", "0")) {
		Mode_Saves(start);
	}
	Gd_Den("0", "0", "0", "0", "0");
	return 1;
}

/* the UseFunc of the crazy gambler and the mercenaries in the talks of the final fight */
static byte Before_Uses(void) {
	return Mode_EntSetS("CrazyGambler_Default", "UseFunc", "Mission_BeforeFinalFight.CrazyGamblerSpeech") &&
		Mode_EntSetS("MercenaryA_Smartie", "UseFunc", "Mission_BeforeFinalFight.MercenariesSpeech") &&
		Mode_EntSetS("MercenaryB_Dumber", "UseFunc", "Mission_BeforeFinalFight.MercenariesSpeech") &&
		Mode_EntSetS("MercenaryC_Brutus", "UseFunc", "Mission_BeforeFinalFight.MercenariesSpeech");
}

/* Init(LevelPath): the scene of the plan the first time, the teleporter to the AB, the talks */
static byte Before_InitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (LvN_SaveIs("BeforeFinalFightPlayed", "0")) {
		nativeVal_t line = { 0 };
		if (!PORT_LOWN(Mission_BeforeFinalFight, LoadScene, NULL, "") || !PORT_CALL(Scene, Play, NULL, "s", "Plan2") ||
			!LvN_ModSetGlobal("Scene", "EndOfSceneCallback", BF, "SetNextMission")) {
			return 0;
		}
		SaveVar_Set("BeforeFinalFightPlayed", "1");
		if (!Mode_EntSetS("DM_Door_GamblinDenChupito_00", "OnObjSlot", "") || !Gd_MusicVolume(0.7)) {
			return 0;
		}
		LvN_Str(&line, "SSound.SetMusicVolume(0)");
		byte ok = Mode_PrintV(&line);
		Native_Free(&line);
		if (!ok) {
			return 0;
		}
	} else if (!Mode_MMDesc("Mission_FinalFight_MissionText1", 1) || !Mode_EntSetS("DM_Door_GamblinDenChupito_00", "OnObjSlot", "!AutoDoorSlot")) {
		return 0;
	}
	return Mode_EntSetI("DM_Door_Teleporter_00", "Active", 1) && Mode_Target("DM_Door_Teleporter_00") &&
		Mode_Connect("DM_Door_Teleporter_00", "AB", 0) && Before_Uses();
}

/* SetNextMission(usr=None): after the scene: the map of the mission scorer shows the AB */
static byte Before_SetNextMissionC(void) {
	static const int from[] = { 0, 0, 0, 255 }, to[] = { 0, 0, 0, 0 };
	nativeVal_t id = { 0 }, item = { 0 }, v = { 0 };
	Native_SetInt(&id, 0);
	Native_SetNone(&item);
	LvN_Str(&v, "AB");
	byte ok = Mode_Fade(0, from, to, 1.0) && Native_GVarGet("MissionScorerName", &item) && Mode_ScorerSet(&id, &item, "ForceMap", &v) &&
		Mode_MMDesc("Mission_FinalFight_MissionText1", 1) && Mode_EntSetS("DM_Door_GamblinDenChupito_00", "OnObjSlot", "!AutoDoorSlot") &&
		Gd_MusicVolume(1.0);
	Native_Free(&item);
	Native_Free(&v);
	return ok;
}

static const char *const plan2_scene[] = {
	"Scene.Load|Plan2",
	"Scene.ActorAction|0|Dtritus|Relax",
	"Scene.ActorAction|70|Dtritus|Listen",
	"Scene.ActorAction|170|Dtritus|Relax",
	"Scene.ActorAction|205|Dtritus|Talk",
	"Scene.ActorAction|235|Dtritus|Relax",
	"Scene.ActorAction|475|Dtritus|Talk1",
	"Scene.ActorAction|505|Dtritus|Relax",
	"Scene.ActorAction|545|Dtritus|Walk",
	"Scene.ActorAction|600|Dtritus|WalkLook",
	"Scene.ActorAction|685|Dtritus|Walk",
	"Scene.ActorAction|705|Dtritus|Relax",
	"Scene.ActorAction|900|Dtritus|TalkEnergy",
	"Scene.ActorAction|930|Dtritus|Relax",
	"Scene.ActorAction|0|CrazyGambler|Relax",
	"Scene.ActorAction|30|CrazyGambler|Talk1",
	"Scene.ActorAction|70|CrazyGambler|Talk",
	"Scene.ActorAction|110|CrazyGambler|Talk2",
	"Scene.ActorAction|150|CrazyGambler|Talk0",
	"Scene.ActorAction|195|CrazyGambler|Relax",
	"Scene.ActorAction|245|CrazyGambler|Talk1",
	"Scene.ActorAction|285|CrazyGambler|Talk",
	"Scene.ActorAction|325|CrazyGambler|Talk0",
	"Scene.ActorAction|365|CrazyGambler|Talk2",
	"Scene.ActorAction|405|CrazyGambler|Talk1",
	"Scene.ActorAction|445|CrazyGambler|Talk",
	"Scene.ActorAction|467|CrazyGambler|Relax",
	"Scene.ActorAction|515|CrazyGambler|Talk",
	"Scene.ActorAction|555|CrazyGambler|Talk1",
	"Scene.ActorAction|595|CrazyGambler|Talk0",
	"Scene.ActorAction|635|CrazyGambler|Talk",
	"Scene.ActorAction|675|CrazyGambler|Talk1",
	"Scene.ActorAction|715|CrazyGambler|Talk",
	"Scene.ActorAction|755|CrazyGambler|Talk2",
	"Scene.ActorAction|795|CrazyGambler|Talk",
	"Scene.ActorAction|835|CrazyGambler|Talk1",
	"Scene.ActorAction|890|CrazyGambler|Relax",
	"Scene.ActorAction|940|CrazyGambler|Talk0",
	"Scene.ActorAction|980|CrazyGambler|Talk",
	"Scene.ActorAction|1020|CrazyGambler|Talk2",
	"Scene.ActorAction|1060|CrazyGambler|Talk1",
	"Scene.ActorAction|1100|CrazyGambler|Talk",
	"Scene.ActorAction|1140|CrazyGambler|Talk2",
	"Scene.ActorAction|1180|CrazyGambler|Talk0",
	"Scene.ActorAction|1220|CrazyGambler|Talk1",
	"Scene.ActorAction|1246|CrazyGambler|Relax",
	"Scene.AddGeneric|0|SScorer.Fade(0,(0,0,0,255),(0,0,0,255),3.0)",
	"Scene.AddGeneric|10|SScorer.Fade(0,(0,0,0,255),(0,0,0,0),1.5)",
	"Scene.AddGeneric|1285|SScorer.Fade(0,(0,0,0,0),(0,0,0,255),1.0)",
	"Scene.ObjAction|610|DM_Door_GamblinDenChupito_00|Open|Opened",
	"Scene.AddText|30|Mission_Scene_GoWithHuman_ThePlan1|6",
	"Scene.AddText|205|Mission_Scene_GoWithHuman_ThePlan2|1",
	"Scene.AddText|245|Mission_Scene_GoWithHuman_ThePlan3|7",
	"Scene.AddText|475|Mission_Scene_GoWithHuman_ThePlan4|1",
	"Scene.AddText|515|Mission_Scene_GoWithHuman_ThePlan5|13",
	"Scene.AddText|900|Mission_Scene_GoWithHuman_ThePlan6|1",
	"Scene.AddText|940|Mission_Scene_GoWithHuman_ThePlan7|10",
	"Scrap.Set|DefInterpTime|0.7",
	"Scene.AddCharActor|Dtritus|Dtritus",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk1|Talk1|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Walk|Walk|1",
	"Scene.CreateLocalAction|Dtritus|0|WalkLook|WalkLook|1",
	"Scene.CreateLocalAction|Dtritus|0|TalkEnergy|TalkEnergy|1",
	"Scene.CreateLocalAction|Dtritus|0|Listen|Listen|1",
	"Scene.SetDefSounds|Dtritus|Dtritus",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|CrazyGambler|CrazyGambler",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk0|Talk0|1",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk1|Talk1|1",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk2|Talk2|1",
	"Scene.SetDefSounds|CrazyGambler|CrazyGambler",
	"SAct.SetCls|DefAction|Relax",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

static byte Before_LoadSceneC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Calls(BF, plan2_scene);
}

LV_FN(Before, InitStatus, 1, Before_InitStatusC)
LV_FN(Before, Init, 1, Before_InitC)
MODE_FN(Before, CrazyGamblerSpeech, 4, Gd_Talk(a, 1, "Mission_FinalFight_InitMsg1", "Mission_FinalFight_InitAns1"))
MODE_FN(Before, MercenariesSpeech, 4, Gd_Talk(a, 1, "Mission_FinalFight_InitMsg2", "Mission_FinalFight_InitAns2"))
GD_KW1(Before, SetNextMission, "usr", Before_SetNextMissionC())
LV_FN(Before, LoadScene, 0, Before_LoadSceneC)

PyMethodDef g_GamblinDenBeforeFinalFightMethods[] = {
	{ "InitStatus", Before_InitStatus, 1, NULL },
	{ "Init", Before_Init, 1, NULL },
	{ "CrazyGamblerSpeech", Before_CrazyGamblerSpeech, 1, NULL },
	{ "MercenariesSpeech", Before_MercenariesSpeech, 1, NULL },
	{ "SetNextMission", (PyCFunction)(void (*)(void))Before_SetNextMission, 3, NULL },
	{ "LoadScene", Before_LoadScene, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_AfterFinalFight: the teleporter to the AB indoors ---- */

static byte After_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	Gd_Den("0", "1", "1", "1", "1");
	SaveVar_Set("PoliceBossAtPolice", "0");
	if (LvN_SaveIs("ComeFrom", "DoorTeleporter")) {
		SaveVar_Set("Spawn", "DM_Player_Spawn_Meeting_Room");
	}
	return 1;
}

static byte After_InitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_EntSetS("DM_Door_GamblinDenChupito_00", "OnObjSlot", "!AutoDoorSlot") && Mode_EntSetI("DM_Door_Teleporter_00", "Active", 1) &&
		Mode_Target("DM_Door_Teleporter_00") && Mode_Connect("DM_Door_Teleporter_00", "ABIndoor", 0);
}

LV_FN(After, InitStatus, 1, After_InitStatusC)
LV_FN(After, Init, 1, After_InitC)

PyMethodDef g_GamblinDenAfterFinalFightMethods[] = {
	{ "InitStatus", After_InitStatus, 1, NULL },
	{ "Init", After_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ArrangeIgorCombat: the versus against the bishop Igor ---- */

static byte Igor_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	SaveVar_Set("PoliceBossAtPolice", "0");
	Gd_Den("1", "0", "1", "1", "0");
	return 1;
}

/* Igor gets his name, the crazy gambler is the target (both branches of the script do the same) */
static byte Igor_InitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Gd_PutEnt(AI, "igor", "Bishop_Friendly35") && Gd_GDescriptor(AI, "igor", "CharName_Igor") &&
		PORT_LOWN(Mission_ArrangeIgorCombat, TalkToCrazyGambler, NULL, "");
}

static byte Igor_TalkToIgorC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_GSetS(AI, "igor", "UseFunc", "Mission_ArrangeIgorCombat.IgorSpeech") && Gd_GTarget(AI, "igor");
}

static byte Igor_IgorSpeechC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t igor = { 0 }, r = { 0 };
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (!LvN_EqS(&a[2], "Talk")) {
		return 1;
	}
	Native_SetNone(&r);
	byte ok = Mode_G(AI, "igor", &igor) && Mode_VObj(&igor, "RadarShow", &r) &&
		(!LvN_EqI(&r, 1) || LV_ONEND(Mission_ArrangeIgorCombat, TalkToCrazyGambler, NULL)) &&
		Mode_NISpeech("Bishop", "Mission_TalkToIgor3_Igor_NI_1Msg");
	Native_Free(&igor);
	Native_Free(&r);
	return ok;
}

/* TalkToCrazyGambler(): the description, a way point, the crazy gambler's talk */
static byte Igor_TalkToCrazyGamblerC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t cg = { 0 }, n = { 0 };
	(void)a;
	(void)out;
	if (!Mode_MMDesc("Mission_ArrangeIgorCombat_MissionText1", 1) || !Mode_WayPoint()) {
		return 0;
	}
	Native_SetNone(&n);
	byte ok = LvN_Entity("CrazyGambler_Default", &cg) && LvN_SetS(&cg, "UseFunc", "Mission_ArrangeIgorCombat.CrazyGamblerSpeech") &&
		Mode_VObj(&cg, "Name", &n) && PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &n);
	Native_Free(&cg);
	Native_Free(&n);
	return ok;
}

/* LaunchIgorCombat(): the places of the characters, the versus in the CA */
static byte Igor_LaunchIgorCombatC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Temple", "Journalist.Betty_Defaut", "Temple", "Journalist.Berto_Defaut", "Press", NULL };
	(void)a;
	(void)out;
	if (!Mode_Import("SaveGame") || !Mode_Import("MissionsFuncs") || !Mode_MMDesc("Mission_VersusAgainstIgor_MissionText1", 1)) {
		return 0;
	}
	Mode_Saves(places);
	return PORT_CALL(MissionsFuncs, StartSuperDeal, NULL, "sss", "CA", "Mission_IgorCombat", "Versus");
}

LV_FN(Igor, InitStatus, 1, Igor_InitStatusC)
LV_FN(Igor, Init, 1, Igor_InitC)
LV_FN(Igor, TalkToIgor, 0, Igor_TalkToIgorC)
LV_FN(Igor, IgorSpeech, 4, Igor_IgorSpeechC)
LV_FN(Igor, TalkToCrazyGambler, 0, Igor_TalkToCrazyGamblerC)
MODE_FN(Igor, CrazyGamblerSpeech, 4, Gd_Arrange(a, "Mission_ArrangeIgorCombat_CrazyGambler",
	&PORT_LREF(Mission_ArrangeIgorCombat, LaunchIgorCombat)))
LV_FN(Igor, LaunchIgorCombat, 0, Igor_LaunchIgorCombatC)

PyMethodDef g_GamblinDenArrangeIgorCombatMethods[] = {
	{ "InitStatus", Igor_InitStatus, 1, NULL },
	{ "Init", Igor_Init, 1, NULL },
	{ "TalkToIgor", Igor_TalkToIgor, 1, NULL },
	{ "IgorSpeech", Igor_IgorSpeech, 1, NULL },
	{ "TalkToCrazyGambler", Igor_TalkToCrazyGambler, 1, NULL },
	{ "CrazyGamblerSpeech", Igor_CrazyGamblerSpeech, 1, NULL },
	{ "LaunchIgorCombat", Igor_LaunchIgorCombat, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ArrangeBankersCTF: Traitorius, then the CTF against the bankers ---- */

static const char *const traitorius_vars[] = { "LLogicDefaultState=6", "LLogicDefaultTarget='", "LLogicState=6", "LLogicTarget='",
	"LLogicProbTalkWhenWalking=1.0", "LLogicIdle=1", "LLogicIdle=0", NULL };

/* Traitorius stands in the den with his name */
static byte Gd_Traitorius(const nativeVal_t *e) {
	nativeVal_t s = { 0 };
	if (!Mode_SetAttrs(e, traitorius_vars)) {
		return 0;
	}
	Mode_Lang("CharName_Traitorius", &s);
	byte ok = Mode_VSet(e, "Descriptor", &s);
	Native_Free(&s);
	return ok;
}

static byte Ctf_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Gd_MissionCharG("Functionary_Traitorius", BC, "CreateTraitoriusInit")) {
		return 0;
	}
	Gd_Den("1", "0", "0", "1", "1");
	return 1;
}

static byte Ctf_InitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("Map") && Gd_PutEnt(BC, "cg", "CrazyGambler_Default") && Gd_PutEnt(BC, "traitorius", "Functionary_Traitorius") &&
		Mode_GSetS(BC, "traitorius", "UseFunc", "Mission_ArrangeBankersCTF.TraitoriusSpeech") && Gd_GTarget(BC, "traitorius") &&
		PORT_LCALL(Map, CharDance, NULL, "s", "Functionary_Traitorius");
}

static byte Ctf_CreateTraitoriusInitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Gd_Traitorius(&a[0]);
}

/* TraitoriusSpeech(UsrName, CharName, optId, FullOption): he talks only to a functionary; the first time the crazy
 * gambler is next */
static byte Ctf_TraitoriusSpeechC(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (!LvN_EqS(&a[2], "Talk")) {
		return 1;
	}
	int func = Mode_UsrIs("Functionary");
	if (func < 0) {
		return 0;
	}
	if (!func) {
		return Mode_Spch("Mission_TalkToTraitorius3_Traitorius_InitMsg2", "Mission_TalkToTraitorius3_Traitorius_InitAns2",
			NULL);
	}
	int first = LvN_GEqI(BC, "HasTalkedToTraitorius", 0);
	if (first < 0 || (first && (!LvN_PutI(BC, "HasTalkedToTraitorius", 1) || !LV_ONEND(Mission_ArrangeBankersCTF, TalkToCrazyGambler, NULL)))) {
		return 0;
	}
	return Mode_Spch("Mission_TalkToTraitorius3_Traitorius_InitMsg1", "Mission_TalkToTraitorius3_Traitorius_InitAns1",
		NULL);
}

static byte Ctf_TalkToCrazyGamblerC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_GSetS(BC, "cg", "UseFunc", "Mission_ArrangeBankersCTF.CrazyGamblerSpeech") && Gd_GTarget(BC, "cg") &&
		Mode_MMDesc("Mission_ArrangeTraitoriusCombat_MissionText1", 0);
}

LV_FN(Ctf, InitStatus, 1, Ctf_InitStatusC)
LV_FN(Ctf, Init, 1, Ctf_InitC)
LV_FN(Ctf, CreateTraitoriusInit, 1, Ctf_CreateTraitoriusInitC)
LV_FN(Ctf, TraitoriusSpeech, 4, Ctf_TraitoriusSpeechC)
LV_FN(Ctf, TalkToCrazyGambler, 0, Ctf_TalkToCrazyGamblerC)
MODE_FN(Ctf, CrazyGamblerSpeech, 4, Gd_Arrange(a, "Mission_ArrangeTraitoriusCombat_CrazyGambler",
	&PORT_LREF(Mission_ArrangeBankersCTF, LaunchBankersCTF)))
MODE_FN(Ctf, LaunchBankersCTF, 0, Gd_Launch("Mission_CTFAgainstBankers_MissionText1", "OM", "Mission_BankersCTF", "CTF"))

PyMethodDef g_GamblinDenArrangeBankersCTFMethods[] = {
	{ "InitStatus", Ctf_InitStatus, 1, NULL },
	{ "Init", Ctf_Init, 1, NULL },
	{ "CreateTraitoriusInit", Ctf_CreateTraitoriusInit, 1, NULL },
	{ "TraitoriusSpeech", Ctf_TraitoriusSpeech, 1, NULL },
	{ "TalkToCrazyGambler", Ctf_TalkToCrazyGambler, 1, NULL },
	{ "CrazyGamblerSpeech", Ctf_CrazyGamblerSpeech, 1, NULL },
	{ "LaunchBankersCTF", Ctf_LaunchBankersCTF, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ArrangeBankersCombat: after the versus: Igor, DeepThroat, the police boss, the crazy gambler ---- */

static byte Bankers_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const start[] = { "ComeFrom", "", "Char", "Dtritus", "Spawn", "DM_Player_Spawn_Outskirts", NULL };
	nativeVal_t fn = { 0 };
	(void)a;
	(void)out;
	if (!Mode_Import("CharsNPC") || !Mode_Import("Map")) {
		return 0;
	}
	if (LvN_SaveIs("Mission_ArrangeBankersCombat", "Igor")) {
		Mode_Saves(start);
	}
	/* Map.CreateFixedInit: the attribute of Map (its AttributeError) */
	byte ok = LvN_ModGet("Map", "CreateFixedInit", &fn) && Gd_MissionChar("PoliceBoss_BankersCombat", &fn);
	Native_Free(&fn);
	if (ok) {
		Gd_Den("1", "1", "1", "0", "0");
	}
	return ok;
}

/* Init(LevelPath): the characters of the mission, the step of the save state (one call site per step) */
static byte Bankers_InitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Gd_PutEnt(BK, "igor", "Bishop_Friendly35") || !Gd_GDescriptor(BK, "igor", "CharName_Igor") || !Gd_PutEnt(BK, "dd", "DM_DTritusDesktop") ||
		!Gd_PutEnt(BK, "cg", "CrazyGambler_Default") || !Gd_PutEnt(BK, "policeBoss", "PoliceBoss_BankersCombat")) {
		return 0;
	}
	if (LvN_SaveIs("Mission_ArrangeBankersCombat", "Igor")) {
		return PORT_LOWN(Mission_ArrangeBankersCombat, LaunchIgorDefeatedSpeech, NULL, "");
	}
	if (LvN_SaveIs("Mission_ArrangeBankersCombat", "Deep")) {
		return PORT_LOWN(Mission_ArrangeBankersCombat, LaunchDeepThroatMessage, NULL, "");
	}
	if (LvN_SaveIs("Mission_ArrangeBankersCombat", "Police")) {
		return PORT_LOWN(Mission_ArrangeBankersCombat, TalkToPoliceBoss, NULL, "");
	}
	if (LvN_SaveIs("Mission_ArrangeBankersCombat", "Crazy")) {
		return PORT_LOWN(Mission_ArrangeBankersCombat, TalkToCrazyGambler, NULL, "");
	}
	return 1;
}

/* LaunchIgorDefeatedSpeech(): beaten Igor in front of the player, his talk */
static byte Bankers_LaunchIgorDefeatedSpeechC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr = { 0 }, igor = { 0 };
	(void)a;
	(void)out;
	SaveVar_Set("Mission_ArrangeBankersCombat", "Deep");
	Native_SetNone(&usr);
	Native_SetNone(&igor);
	int go = Mode_UsrHandle(&usr) ? Native_True(&usr) : -1;
	if (go > 0) {
		go = LvN_GTrue(BK, "igor");
	}
	byte ok = go >= 0 && (!go || (Mode_G(BK, "igor", &igor) && Gd_Face(&igor, &usr) &&
		LV_ONEND(Mission_ArrangeBankersCombat, LaunchDeepThroatMessage, NULL) &&
		Gd_SpeakersOf(&usr, &igor) && Mode_NISpeech("Bishop", "Mission_VersusAgainstIgor_Igor_NI_1Msg")));
	Native_Free(&usr);
	Native_Free(&igor);
	return ok;
}

/* LaunchDeepThroatMessage(): Speech.AddRemoteCom in 3 s with TalkToDeepThroat after it */
static byte Bankers_LaunchDeepThroatMessageC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t t = { 0 }, args[3] = { 0 };
	(void)a;
	(void)out;
	/* Scrap.GetTime() + 3.0, Speech.AddRemoteCom, (key, who, TalkToDeepThroat) in the order of the script */
	Native_SetFloat(&t, Mode_Time() + 3.0);
	nativeVal_t fn;
	if (!Mode_RefMod("Speech", "AddRemoteCom", &fn)) {
		return 0;
	}
	LvN_Str(&args[0], "Mission_VersusAgainstIgor_RemoteMessage4");
	LvN_Str(&args[1], "DeepThroat");
	byte ok = LvN_GObj(BK, "TalkToDeepThroat", &args[2]) && Mode_ScheduleV(&t, &fn, args, 3, NULL);
	Native_Free(&fn);
	for (int i = 0; i < 3; i++) {
		Native_Free(&args[i]);
	}
	return ok;
}

/* TalkToDeepThroat(): Dtritus' desktop for the call */
static byte Bankers_TalkToDeepThroatC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("DTritusDesktop") && LvN_GActivate(BK, "dd", 1) && Mode_WayPoint() && Mode_MMDesc("Mission_CallDeepThroat2_MissionText1", 1) &&
		Gd_Dialog(BK, "DeepThroatSpeech");
}

static byte Bankers_DeepThroatSpeechC(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	if (!LvN_GActivate(BK, "dd", 0) || !LvN_Speakers(&a[0], &a[1]) || !LV_ONEND(Mission_ArrangeBankersCombat, TalkToPoliceBoss, NULL)) {
		return 0;
	}
	SaveVar_Set("Mission_ArrangeBankersCombat", "Police");
	return Mode_NISpeech("DeepThroat", "Mission_CallDeepThroat2_ComDeepThroat_NI_1Msg");
}

static byte Bankers_TalkToPoliceBossC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t e = { 0 };
	(void)a;
	(void)out;
	LvN_Str(&e, "");
	byte ok = Mode_Import("DTritusDesktop") && Mode_SetModVar("DTritusDesktop", "DialogController", &e) && Gd_GTarget(BK, "policeBoss") &&
		Mode_WayPoint() && Mode_MMDesc("Mission_SpyPoliceBoss_MissionText1", 0) &&
		Mode_GSetS(BK, "policeBoss", "UseFunc", "Mission_ArrangeBankersCombat.PoliceBossSpeech");
	Native_Free(&e);
	return ok;
}

static byte Bankers_PoliceBossSpeechC(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (!LvN_EqS(&a[2], "Talk")) {
		return PORT_CALL(Speech, StdOnSpch, NULL, "v", &a[2]);
	}
	if (!LV_ONEND(Mission_ArrangeBankersCombat, TalkToCrazyGambler, NULL) ||
		!Mode_NISpeech("PoliceBoss", "Mission_SpyPoliceBoss_PoliceBoss_NI_1Msg")) {
		return 0;
	}
	SaveVar_Set("Mission_ArrangeBankersCombat", "Crazy");
	return 1;
}

static byte Bankers_TalkToCrazyGamblerC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_GSetS(BK, "policeBoss", "UseFunc", "Mission_ArrangeBankersCombat.PoliceBossSpeech2") &&
		Mode_GSetS(BK, "cg", "UseFunc", "Mission_ArrangeBankersCombat.CrazyGamblerSpeech") && Gd_GTarget(BK, "cg") && Mode_WayPoint() &&
		Mode_MMDesc("Mission_ArrangeBankersCombat_MissionText1", 0);
}

static byte Bankers_LaunchBankersCombatC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Mode_Import("MissionsFuncs") || !Mode_Import("SaveGame")) {
		return 0;
	}
	SaveVars_Delete("Mission_ArrangeBankersCombat");
	return Mode_MMDesc("Mission_GetBankerShipPlans_MissionText1", 1) &&
		PORT_CALL(MissionsFuncs, StartSuperDeal, NULL, "sss", "QF", "Mission_BankersCombat", "TeamDeathMatch");
}

LV_FN(Bankers, InitStatus, 1, Bankers_InitStatusC)
LV_FN(Bankers, Init, 1, Bankers_InitC)
LV_FN(Bankers, LaunchIgorDefeatedSpeech, 0, Bankers_LaunchIgorDefeatedSpeechC)
LV_FN(Bankers, LaunchDeepThroatMessage, 0, Bankers_LaunchDeepThroatMessageC)
LV_FN(Bankers, TalkToDeepThroat, 0, Bankers_TalkToDeepThroatC)
LV_FN(Bankers, DeepThroatSpeech, 4, Bankers_DeepThroatSpeechC)
LV_FN(Bankers, TalkToPoliceBoss, 0, Bankers_TalkToPoliceBossC)
LV_FN(Bankers, PoliceBossSpeech, 4, Bankers_PoliceBossSpeechC)
MODE_FN(Bankers, PoliceBossSpeech2, 4, Gd_Talk(a, 1, "Mission_ArrangeBankersCombat_PoliceBoss_InitMsg1",
	"Mission_ArrangeBankersCombat_PoliceBoss_InitAns1"))
LV_FN(Bankers, TalkToCrazyGambler, 0, Bankers_TalkToCrazyGamblerC)
MODE_FN(Bankers, CrazyGamblerSpeech, 4, Gd_Arrange(a, "Mission_ArrangeBankersCombat_CrazyGambler",
	&PORT_LREF(Mission_ArrangeBankersCombat, LaunchBankersCombat)))
LV_FN(Bankers, LaunchBankersCombat, 0, Bankers_LaunchBankersCombatC)

PyMethodDef g_GamblinDenArrangeBankersCombatMethods[] = {
	{ "InitStatus", Bankers_InitStatus, 1, NULL },
	{ "Init", Bankers_Init, 1, NULL },
	{ "LaunchIgorDefeatedSpeech", Bankers_LaunchIgorDefeatedSpeech, 1, NULL },
	{ "LaunchDeepThroatMessage", Bankers_LaunchDeepThroatMessage, 1, NULL },
	{ "TalkToDeepThroat", Bankers_TalkToDeepThroat, 1, NULL },
	{ "DeepThroatSpeech", Bankers_DeepThroatSpeech, 1, NULL },
	{ "TalkToPoliceBoss", Bankers_TalkToPoliceBoss, 1, NULL },
	{ "PoliceBossSpeech", Bankers_PoliceBossSpeech, 1, NULL },
	{ "PoliceBossSpeech2", Bankers_PoliceBossSpeech2, 1, NULL },
	{ "TalkToCrazyGambler", Bankers_TalkToCrazyGambler, 1, NULL },
	{ "CrazyGamblerSpeech", Bankers_CrazyGamblerSpeech, 1, NULL },
	{ "LaunchBankersCombat", Bankers_LaunchBankersCombat, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_IgorFirstContact: the crazy gambler, Igor, then the three mercenaries ---- */

static byte First_InitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (LvN_SaveIs("IgorFirstContactMissionState", "TalkToCrazyGambler")) {
		return PORT_LOWN(Mission_IgorFirstContact, TalkToCrazyGambler, NULL, "");
	}
	if (LvN_SaveIs("IgorFirstContactMissionState", "TalkToIgor")) {
		return PORT_LOWN(Mission_IgorFirstContact, TalkToIgor, NULL, "");
	}
	return PORT_LOWN(Mission_IgorFirstContact, TalkToMercenaries, NULL, "");
}

static byte First_TalkToCrazyGamblerC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Gd_PutEnt(IF, "cg", "CrazyGambler_Default") && Gd_GTarget(IF, "cg") && Gd_RoutineG(IF, "CrazyGamblerSpeech");
}

/* CrazyGamblerSpeech(UsrName, CharName, optId, FullOption): his own talk again, Igor is next */
static byte First_CrazyGamblerSpeechC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t fn = { 0 };
	(void)out;
	if (!Mode_Import("Menu") || !Mode_Import("CharsNPC") || !Mode_Import("CrazyGambler")) {
		return 0;
	}
	if (!LvN_EqS(&a[2], "Talk")) {
		return 1;
	}
	byte ok = LvN_ModGet("CrazyGambler", "CrazyGamblerSpeech", &fn) && LvN_Routine("CrazyGambler", &fn);
	Native_Free(&fn);
	if (!ok) {
		return 0;
	}
	SaveVar_Set("IgorFirstContactMissionState", "TalkToIgor");
	return LV_ONEND(Mission_IgorFirstContact, TalkToIgor, NULL) && Mode_NISpeech("CrazyGambler", "Mission_GetReputation_CrazyGambler_NI_1Msg");
}

static byte First_TalkToIgorC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_MMDesc("Mission_TalkToIgor_MissionText1", 1) && Mode_WayPoint() && Gd_PutEnt(IF, "igor", "Bishop_Friendly35") &&
		Gd_GDescriptor(IF, "igor", "CharName_Igor") && Gd_GTarget(IF, "igor") &&
		Mode_GSetS(IF, "igor", "UseFunc", "Mission_IgorFirstContact.IgorSpeech");
}

/* IgorSpeech(UsrName, CharName, optId, FullOption): Igor talks to Dtritus only; his answer sends him to the
 * mercenaries */
static byte First_IgorSpeechC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const ans[] = { "Mission_TalkToIgor_Igor_1", NULL };
	(void)out;
	if (!LvN_Speakers(&a[0], &a[1])) {
		return 0;
	}
	if (LvN_EqS(&a[2], "Talk")) {
		int dtritus = Mode_UsrIs("Dtritus");
		if (dtritus < 0) {
			return 0;
		}
		if (!dtritus) {
			return Mode_Spch("Mission_TalkToIgor_Igor_InitMsg2", "Mission_TalkToIgor_Igor_InitAns2", NULL);
		}
		int talked = LvN_GTrue(IF, "alreadyTalked");
		if (talked < 0) {
			return 0;
		}
		if (!talked) {
			return Mode_Spch("Mission_TalkToIgor_Igor_InitMsg1", "Mission_TalkToIgor_Igor_InitAns1", NULL) &&
				Mode_AnsLst(ans, -1);
		}
		return Mode_Spch("Mission_GetMercShipPlans_Igor_1Msg", "Mission_GetMercShipPlans_Igor_1Ans", NULL);
	}
	if (!PORT_CALL(Speech, StdOnSpch, NULL, "v", &a[2])) {
		return 0;
	}
	if (!LvN_SaveIs("IgorFirstContactMissionState", "TalkToIgor")) {
		return 1;
	}
	SaveVar_Set("IgorFirstContactMissionState", "TalkToMercenaries");
	return LV_ONEND(Mission_IgorFirstContact, TalkToMercenaries, NULL);
}

/* TalkToMercenaries(): the three mercenaries as targets with their talks */
static byte First_TalkToMercenariesC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mercs[][3] = { { "mercA", "MercenaryA_Smartie", "Mission_IgorFirstContact.MercenaryASpeech" },
		{ "mercB", "MercenaryB_Dumber", "Mission_IgorFirstContact.MercenaryBSpeech" },
		{ "mercC", "MercenaryC_Brutus", "Mission_IgorFirstContact.MercenaryCSpeech" } };
	static const char *const names[] = { "MercenaryA_Smartie", "MercenaryB_Dumber", "MercenaryC_Brutus", NULL };
	nativeVal_t l = { 0 };
	(void)a;
	(void)out;
	Mode_Texts(&l, names, -1);
	byte ok = LvN_PutI(IF, "alreadyTalked", 1) && Mode_MMDesc("Mission_GetMercShipPlans_MissionText1", 1) && Mode_WayPoint() &&
		PORT_CALL(MissionsFuncs, SetMultipleMissionTargets, NULL, "v", &l);
	Native_Free(&l);
	for (int i = 0; i < 3 && ok; i++) {
		ok = Gd_PutEnt(IF, mercs[i][0], mercs[i][1]);
	}
	for (int i = 0; i < 3 && ok; i++) {
		ok = Mode_GSetS(IF, mercs[i][0], "UseFunc", mercs[i][2]);
	}
	return ok;
}

static const gdMerc_t first_mercA = { "Mission_IgorFirstContact", "Mission_GetMercShipPlans_SmartMerc",
	"Mission_GetMercShipPlans_SmartMerc_InitMsg1", "Mission_GetMercShipPlans_SmartMerc_InitAns1", NULL, NULL, 0 };
static const gdMerc_t first_mercC = { "Mission_IgorFirstContact", "Mission_GetMercShipPlans_AverageMerc",
	"Mission_GetMercShipPlans_AverageMerc_InitMsg1", "Mission_GetMercShipPlans_AverageMerc_InitAns1", NULL, NULL, 0 };
static const gdMerc_t first_mercB = { "Mission_IgorFirstContact", "Mission_GetMercShipPlans_StupidMerc",
	"Mission_FirstContactMercs_StupidMerc_InitMsg2", "Mission_FirstContactMercs_StupidMerc_InitAns2",
	&PORT_LREF(Mission_IgorFirstContact, SetNextMission), NULL, 0 };

/* SetNextMission(): the mercenaries no longer targets, on to Mission_StupidMercAgain in the scrapyard */
static byte First_SetNextMissionC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_WayPoint() && LvN_GActivate(IF, "mercA", 0) && LvN_GActivate(IF, "mercB", 0) && LvN_GActivate(IF, "mercC", 0) &&
		LvN_Next("Mission_StupidMercAgain", "Scrapyard", "Mission_GetMercShipPlans_MissionText2");
}

LV_FN(First, InitStatus, 1, LvN_Pass)
LV_FN(First, Init, 1, First_InitC)
LV_FN(First, TalkToCrazyGambler, 0, First_TalkToCrazyGamblerC)
LV_FN(First, CrazyGamblerSpeech, 4, First_CrazyGamblerSpeechC)
LV_FN(First, TalkToIgor, 0, First_TalkToIgorC)
LV_FN(First, IgorSpeech, 4, First_IgorSpeechC)
LV_FN(First, TalkToMercenaries, 0, First_TalkToMercenariesC)
MODE_FN(First, MercenaryASpeech, 4, Gd_Merc(a, IF, &first_mercA))
MODE_FN(First, MercenaryCSpeech, 4, Gd_Merc(a, IF, &first_mercC))
MODE_FN(First, MercenaryBSpeech, 4, Gd_Merc(a, IF, &first_mercB))
LV_FN(First, SetNextMission, 0, First_SetNextMissionC)

PyMethodDef g_GamblinDenIgorFirstContactMethods[] = {
	{ "InitStatus", First_InitStatus, 1, NULL },
	{ "Init", First_Init, 1, NULL },
	{ "TalkToCrazyGambler", First_TalkToCrazyGambler, 1, NULL },
	{ "CrazyGamblerSpeech", First_CrazyGamblerSpeech, 1, NULL },
	{ "TalkToIgor", First_TalkToIgor, 1, NULL },
	{ "IgorSpeech", First_IgorSpeech, 1, NULL },
	{ "TalkToMercenaries", First_TalkToMercenaries, 1, NULL },
	{ "MercenaryASpeech", First_MercenaryASpeech, 1, NULL },
	{ "MercenaryCSpeech", First_MercenaryCSpeech, 1, NULL },
	{ "MercenaryBSpeech", First_MercenaryBSpeech, 1, NULL },
	{ "SetNextMission", First_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_GoToGamblinDen: the scene of Bill's plan in the den ---- */

static byte Go_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const start[] = { "Spawn", "DM_Player_Spawn_Meeting_Room", "ComeFrom", "", "Char", "Dtritus", NULL };
	nativeVal_t fn = { 0 };
	(void)a;
	(void)out;
	Mode_Saves(start);
	if (!PORT_CALL(Chars, AddNewCharRes, NULL, "s", "Human")) {
		return 0;
	}
	byte ok = LvN_GObj(GG, "AfterBrowseNPC", &fn) && LvN_ModAppend("CharsNPC", "AfterBrowseNPC", &fn);
	Native_Free(&fn);
	return ok;
}

static byte Go_AfterBrowseNPCC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("CharsNPC") && Gd_CallbackG("MercenaryA_Smartie", GG, "CreateMercenaryInit") &&
		Gd_CallbackG("MercenaryB_Dumber", GG, "CreateMercenaryInit") && Gd_CallbackG("MercenaryC_Brutus", GG, "CreateMercenaryInit") &&
		PORT_CALL(CharsNPC, CreateMissionChar, NULL, "s", "Human_Default");
}

static byte Go_CreateMercenaryInitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_SetS(&a[0], "UseFunc", "Mission_GoToGamblinDen.MercenarySpeech");
}

static byte Go_InitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return PORT_LOWN(Mission_GoToGamblinDen, LoadScene, NULL, "") &&
		Mode_EntSetS("CrazyGambler_Default", "UseFunc", "Mission_GoToGamblinDen.CrazyGamblerSpeech") &&
		Mode_EntSetS("Human_Default", "UseFunc", "Mission_GoToGamblinDen.HumanSpeech") && PORT_CALL(Scene, Play, NULL, "s", "Plan") &&
		LvN_ModSetGlobal("Scene", "EndOfSceneCallback", GG, "SetNextMission") && Gd_MusicVolume(0.5);
}

static const char *const plan_scene[] = {
	"Scene.Load|Plan",
	"Scene.ActorAction|20|Dtritus|Walk",
	"Scene.ActorAction|290|Dtritus|WalkHi",
	"Scene.ActorAction|330|Dtritus|Talk",
	"Scene.ActorAction|392|Dtritus|Relax",
	"Scene.ActorAction|485|Dtritus|Turn",
	"Scene.ActorAction|500|Dtritus|Relax",
	"Scene.ActorAction|522|Dtritus|Turn",
	"Scene.ActorAction|537|Dtritus|Relax",
	"Scene.ActorAction|800|Dtritus|Turn",
	"Scene.ActorAction|815|Dtritus|Relax",
	"Scene.ActorAction|1070|Dtritus|Turn",
	"Scene.ActorAction|1085|Dtritus|Relax",
	"Scene.ActorAction|1230|Dtritus|Listen",
	"Scene.ActorAction|1340|Dtritus|Relax",
	"Scene.ActorAction|1492|Dtritus|Turn",
	"Scene.ActorAction|1507|Dtritus|Relax",
	"Scene.ActorAction|1530|Dtritus|OK",
	"Scene.ActorAction|1575|Dtritus|Relax",
	"Scene.ActorAction|0|Human|Relax",
	"Scene.ActorAction|200|Human|TalkFunny",
	"Scene.ActorAction|278|Human|Relax",
	"Scene.ActorAction|292|Human|Turn",
	"Scene.ActorAction|307|Human|Relax",
	"Scene.ActorAction|490|Human|Talk",
	"Scene.ActorAction|555|Human|Talk0",
	"Scene.ActorAction|600|Human|Talk",
	"Scene.ActorAction|665|Human|Talk0",
	"Scene.ActorAction|710|Human|Relax",
	"Scene.ActorAction|720|Human|Turn",
	"Scene.ActorAction|735|Human|Relax",
	"Scene.ActorAction|800|Human|Talk",
	"Scene.ActorAction|815|Human|Turn",
	"Scene.ActorAction|830|Human|Talk",
	"Scene.ActorAction|900|Human|Talk1",
	"Scene.ActorAction|950|Human|TalkCase",
	"Scene.ActorAction|1058|Human|Relax",
	"Scene.ActorAction|1072|Human|Turn",
	"Scene.ActorAction|1087|Human|Relax",
	"Scene.ActorAction|1320|Human|Relax1",
	"Scene.ActorAction|1420|Human|Relax",
	"Scene.ActorAction|1490|Human|Turn",
	"Scene.ActorAction|1505|Human|Relax",
	"Scene.ActorAction|0|CrazyGambler|Relax",
	"Scene.ActorAction|400|CrazyGambler|Talk1",
	"Scene.ActorAction|420|CrazyGambler|TalkSign",
	"Scene.ActorAction|460|CrazyGambler|Talk1",
	"Scene.ActorAction|481|CrazyGambler|Relax",
	"Scene.ActorAction|1070|CrazyGambler|Talk1",
	"Scene.ActorAction|1110|CrazyGambler|Talk0",
	"Scene.ActorAction|1150|CrazyGambler|Talk2",
	"Scene.ActorAction|1190|CrazyGambler|Talk",
	"Scene.ActorAction|1230|CrazyGambler|Talk0",
	"Scene.ActorAction|1270|CrazyGambler|TalkSign",
	"Scene.ActorAction|1310|CrazyGambler|Talk2",
	"Scene.ActorAction|1350|CrazyGambler|Talk",
	"Scene.ActorAction|1390|CrazyGambler|Talk1",
	"Scene.ActorAction|1430|CrazyGambler|Talk0",
	"Scene.ActorAction|1484|CrazyGambler|Relax",
	"Scene.ActorAction|0|MercenaryA|RelaxRight",
	"Scene.ActorAction|208|MercenaryA|Turn",
	"Scene.ActorAction|223|MercenaryA|Relax",
	"Scene.ActorAction|290|MercenaryA|Turn",
	"Scene.ActorAction|305|MercenaryA|Relax",
	"Scene.ActorAction|406|MercenaryA|Turn",
	"Scene.ActorAction|421|MercenaryA|Relax",
	"Scene.ActorAction|492|MercenaryA|Turn",
	"Scene.ActorAction|507|MercenaryA|Relax",
	"Scene.ActorAction|720|MercenaryA|Talk",
	"Scene.ActorAction|792|MercenaryA|Relax",
	"Scene.ActorAction|1073|MercenaryA|Turn",
	"Scene.ActorAction|1088|MercenaryA|Relax",
	"Scene.ActorAction|1190|MercenaryA|Drink",
	"Scene.ActorAction|1245|MercenaryA|Relax",
	"Scene.ActorAction|1500|MercenaryA|TalkHappy",
	"Scene.ActorAction|1542|MercenaryA|Relax",
	"Scene.ActorAction|0|MercenaryB|Relax",
	"Scene.ActorAction|110|MercenaryB|Talk0",
	"Scene.ActorAction|170|MercenaryB|Talk",
	"Scene.ActorAction|191|MercenaryB|Relax",
	"Scene.ActorAction|288|MercenaryB|Turn",
	"Scene.ActorAction|303|MercenaryB|Relax",
	"Scene.ActorAction|400|MercenaryB|Turn",
	"Scene.ActorAction|415|MercenaryB|Relax",
	"Scene.ActorAction|490|MercenaryB|Turn",
	"Scene.ActorAction|505|MercenaryB|Relax",
	"Scene.ActorAction|722|MercenaryB|RelaxLeft",
	"Scene.ActorAction|805|MercenaryB|Relax",
	"Scene.ActorAction|1070|MercenaryB|Turn",
	"Scene.ActorAction|1085|MercenaryB|Relax",
	"Scene.ActorAction|1295|MercenaryB|Relax1",
	"Scene.ActorAction|1380|MercenaryB|Relax",
	"Scene.ActorAction|1495|MercenaryB|TalkHappy",
	"Scene.ActorAction|1537|MercenaryB|Relax",
	"Scene.ActorAction|0|MercenaryC|RelaxRight",
	"Scene.ActorAction|200|MercenaryC|Turn",
	"Scene.ActorAction|220|MercenaryC|Relax",
	"Scene.ActorAction|285|MercenaryC|Turn",
	"Scene.ActorAction|300|MercenaryC|Relax",
	"Scene.ActorAction|410|MercenaryC|Turn",
	"Scene.ActorAction|425|MercenaryC|Relax",
	"Scene.ActorAction|488|MercenaryC|Turn",
	"Scene.ActorAction|503|MercenaryC|Relax",
	"Scene.ActorAction|725|MercenaryC|RelaxRight",
	"Scene.ActorAction|808|MercenaryC|Relax",
	"Scene.ActorAction|1069|MercenaryC|Turn",
	"Scene.ActorAction|1084|MercenaryC|Relax",
	"Scene.ActorAction|1490|MercenaryC|TalkHappy",
	"Scene.ActorAction|1532|MercenaryC|Relax",
	"Scene.AddGeneric|0|SScorer.Fade(0,(0,0,0,255),(0,0,0,255),3.0)",
	"Scene.AddGeneric|10|SScorer.Fade(0,(0,0,0,255),(0,0,0,0),2.0)",
	"Scene.AddGeneric|1550|SScorer.Fade(0,(0,0,0,0),(0,0,0,255),1.5)",
	"Scene.AddText|110|Mission_Scene_GoToGamblingDen_1|3",
	"Scene.AddText|200|Mission_Scene_GoToGamblingDen_2|3",
	"Scene.AddText|290|Mission_Scene_GoToGamblingDen_3|3",
	"Scene.AddText|400|Mission_Scene_GoToGamblingDen_4|3",
	"Scene.AddText|490|Mission_Scene_GoToGamblingDen_5|7",
	"Scene.AddText|720|Mission_Scene_GoToGamblingDen_6|2",
	"Scene.AddText|800|Mission_Scene_GoToGamblingDen_7|1",
	"Scene.AddText|830|Mission_Scene_GoToGamblingDen_8|8",
	"Scene.AddText|1070|Mission_Scene_GoToGamblingDen_9|14",
	"Scene.AddText|1494|Mission_Scene_GoToGamblingDen_10|1",
	"Scrap.Set|DefInterpTime|0.7",
	"Scene.AddCharActor|Dtritus|Dtritus",
	"MakeChar.CreateRelaxAction|Dtritus|0|Turn|Turn|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Walk|Walk|1",
	"MakeChar.CreateRelaxAction|Dtritus|0|Talk|Talk|1",
	"Scene.CreateLocalAction|Dtritus|0|WalkHi|WalkHi|1",
	"Scene.CreateLocalAction|Dtritus|0|Listen|Listen|1",
	"Scene.CreateLocalAction|Dtritus|0|OK|OK|1",
	"Scene.SetDefSounds|Dtritus|Dtritus",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|Human|Human",
	"MakeChar.CreateRelaxAction|Human|0|Turn|Turn|1",
	"MakeChar.CreateRelaxAction|Human|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|Human|0|Talk0|Talk0|1",
	"MakeChar.CreateRelaxAction|Human|0|Relax1|Relax1|1",
	"Scene.CreateLocalAction|Human|0|TalkFunny|TalkFunny|1",
	"Scene.CreateLocalAction|Human|0|TalkCase|TalkCase|1",
	"Scene.SetDefSounds|Human|Human",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|CrazyGambler|CrazyGambler",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk0|Talk0|1",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk1|Talk1|1",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk2|Talk2|1",
	"Scene.CreateLocalAction|CrazyGambler|0|TalkSign|TalkSign|1",
	"Scene.SetDefSounds|CrazyGambler|CrazyGambler",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|MercenaryA|MercenaryA",
	"MakeChar.CreateRelaxAction|MercenaryA|0|Turn|Turn|1",
	"MakeChar.CreateRelaxAction|MercenaryA|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|MercenaryA|0|Drink|Drink|1",
	"SAct.SetAct|AddEventAtTime|^SAct.GetAct:TimeOfFrame5",
	"SAct.SetAct|EventName|FXMercenaryDrink",
	"SAct.SetAct|EventFunc|!FXMercenaryDrink",
	"Scene.CreateLocalAction|MercenaryA|0|RelaxRight|RelaxRight|1",
	"Scene.CreateLocalAction|MercenaryA|0|TalkHappy|TalkHappy|1",
	"Scene.SetDefSounds|MercenaryA|Mercenary",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|MercenaryB|MercenaryB",
	"MakeChar.CreateRelaxAction|MercenaryB|0|Turn|Turn|1",
	"MakeChar.CreateRelaxAction|MercenaryB|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|MercenaryB|0|Talk0|Talk0|1",
	"MakeChar.CreateRelaxAction|MercenaryB|0|Talk1|Talk1|1",
	"MakeChar.CreateRelaxAction|MercenaryB|0|Relax1|Relax1|1",
	"Scene.CreateLocalAction|MercenaryB|0|RelaxLeft|RelaxLeft|1",
	"Scene.CreateLocalAction|MercenaryB|0|TalkHappy|TalkHappy|1",
	"Scene.SetDefSounds|MercenaryB|Mercenary",
	"SAct.SetCls|DefAction|Relax",
	"Scene.AddCharActor|MercenaryC|MercenaryC",
	"MakeChar.CreateRelaxAction|MercenaryC|0|Turn|Turn|1",
	"MakeChar.CreateRelaxAction|MercenaryC|0|Talk|Talk|1",
	"Scene.CreateLocalAction|MercenaryC|0|RelaxRight|RelaxRight|1",
	"Scene.CreateLocalAction|MercenaryC|0|TalkHappy|TalkHappy|1",
	"Scene.SetDefSounds|MercenaryC|Mercenary",
	"SAct.SetCls|DefAction|Relax",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

static byte Go_LoadSceneC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Calls(GG, plan_scene);
}

/* SetNextMission(usr=None): the mercenaries help Dtritus, on to Mission_DestroyCommSats1 */
static byte Go_SetNextMissionC(void) {
	static const int from[] = { 0, 0, 0, 255 }, to[] = { 0, 0, 0, 0 };
	if (!Mode_Import("MissionsFuncs") || !Mode_Import("SaveGame") || !Mode_Fade(0, from, to, 1.0)) {
		return 0;
	}
	SaveVar_Set("MercsHelpDtritus", "1");
	return LvN_Next("Mission_DestroyCommSats1", "Outskirts", "Mission_DestroyCommSats1_MissionText1") && Mode_WayPoint() && Gd_MusicVolume(1.0);
}

LV_FN(Go, InitStatus, 1, Go_InitStatusC)
LV_FN(Go, AfterBrowseNPC, 0, Go_AfterBrowseNPCC)
LV_FN(Go, CreateMercenaryInit, 1, Go_CreateMercenaryInitC)
MODE_FN(Go, MercenarySpeech, 4, Gd_Talk(a, 1, "Mission_DestroyCommSats1_Merc_InitMsg1", "Mission_DestroyCommSats1_Merc_InitAns1"))
MODE_FN(Go, HumanSpeech, 4, Gd_Talk(a, 1, "Mission_DestroyCommSats1_Human_InitMsg1", "Mission_DestroyCommSats1_Human_InitAns1"))
MODE_FN(Go, CrazyGamblerSpeech, 4, Gd_Talk(a, 1, "Mission_DestroyCommSats1_CrazyGambler_InitMsg1",
	"Mission_DestroyCommSats1_CrazyGambler_InitAns1"))
LV_FN(Go, Init, 1, Go_InitC)
LV_FN(Go, LoadScene, 0, Go_LoadSceneC)
GD_KW1(Go, SetNextMission, "usr", Go_SetNextMissionC())

PyMethodDef g_GamblinDenGoToGamblinDenMethods[] = {
	{ "InitStatus", Go_InitStatus, 1, NULL },
	{ "AfterBrowseNPC", Go_AfterBrowseNPC, 1, NULL },
	{ "CreateMercenaryInit", Go_CreateMercenaryInit, 1, NULL },
	{ "MercenarySpeech", Go_MercenarySpeech, 1, NULL },
	{ "HumanSpeech", Go_HumanSpeech, 1, NULL },
	{ "CrazyGamblerSpeech", Go_CrazyGamblerSpeech, 1, NULL },
	{ "Init", Go_Init, 1, NULL },
	{ "LoadScene", Go_LoadScene, 1, NULL },
	{ "SetNextMission", (PyCFunction)(void (*)(void))Go_SetNextMission, 3, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_TalkToTraitoriusAfterCTF: Traitorius sends Dtritus to the meeting with Mr X ---- */

static byte Traitor_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	SaveVar_Set("Player.InfiniteLives", "1");
	SaveVar_Set("Char", "Functionary");
	return Gd_MissionCharG("Functionary_Traitorius", TT, "CreateTraitoriusInit");
}

static byte Traitor_InitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_EntSetS("DM_Door_Elevator_Scrapyard", "OnObjSlot", "!CloseDoorSlot") &&
		PORT_LOWN(Mission_TalkToTraitoriusAfterCTF, LaunchTraitoriusSpeech, NULL, "");
}

static byte Traitor_CreateTraitoriusInitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return Gd_Traitorius(&a[0]) && Mode_Put(TT, "traitorius", NULL, &a[0]);
}

/* LaunchTraitoriusSpeech(): the player in front of Traitorius, his talk */
static byte Traitor_LaunchTraitoriusSpeechC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t usr = { 0 }, tr = { 0 };
	(void)a;
	(void)out;
	Native_SetNone(&usr);
	Native_SetNone(&tr);
	int go = Mode_UsrHandle(&usr) ? Native_True(&usr) : -1;
	if (go > 0) {
		go = LvN_GTrue(TT, "traitorius");
	}
	byte ok = go >= 0 && (!go || (Mode_G(TT, "traitorius", &tr) && Gd_Face(&usr, &tr) &&
		LV_ONEND(Mission_TalkToTraitoriusAfterCTF, SetNextMission, NULL) &&
		Gd_SpeakersOf(&usr, &tr) && Mode_NISpeech("Functionary", "Mission_CTFAgainstBankers_Traitorius_NI_1Msg")));
	Native_Free(&usr);
	Native_Free(&tr);
	return ok;
}

/* SetNextMission(): on to Mission_IdentifyMerc in the outskirts, the lonely mercenary waits there */
static byte Traitor_SetNextMissionC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Mode_Import("Init") || !Mode_GSetS(TT, "traitorius", "UseFunc", "Mission_TalkToTraitoriusAfterCTF.TraitoriusSpeech")) {
		return 0;
	}
	SaveVar_Set("ComeFrom", "GamblinDen");
	if (!Mode_WayPoint() || !LvN_Next("Mission_IdentifyMerc", "Outskirts", "Mission_MeetingWithMrX_MissionText1")) {
		return 0;
	}
	SaveVar_Set("LonelyMercDataActive", "1");
	return 1;
}

LV_FN(Traitor, InitStatus, 1, Traitor_InitStatusC)
LV_FN(Traitor, Init, 1, Traitor_InitC)
LV_FN(Traitor, CreateTraitoriusInit, 1, Traitor_CreateTraitoriusInitC)
LV_FN(Traitor, LaunchTraitoriusSpeech, 0, Traitor_LaunchTraitoriusSpeechC)
MODE_FN(Traitor, TraitoriusSpeech, 4, Gd_Talk(a, 1, "Mission_CTFAgainstBankers_Traitorius_InitMsg1",
	"Mission_CTFAgainstBankers_Traitorius_InitAns1"))
LV_FN(Traitor, SetNextMission, 0, Traitor_SetNextMissionC)

PyMethodDef g_GamblinDenTalkToTraitoriusAfterCTFMethods[] = {
	{ "InitStatus", Traitor_InitStatus, 1, NULL },
	{ "Init", Traitor_Init, 1, NULL },
	{ "CreateTraitoriusInit", Traitor_CreateTraitoriusInit, 1, NULL },
	{ "LaunchTraitoriusSpeech", Traitor_LaunchTraitoriusSpeech, 1, NULL },
	{ "TraitoriusSpeech", Traitor_TraitoriusSpeech, 1, NULL },
	{ "SetNextMission", Traitor_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_BackFromMortalRace: the inferno plans, DeepThroat on the desktop ---- */

static byte Back_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	Gd_Den(NULL, "1", "0", "0", "1");
	return 1;
}

/* Init(LevelPath): the crazy gambler gives the plans of the inferno, DeepThroat after the message */
static byte Back_InitC(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *l = Mode_V(&t), *msg = Mode_V(&t);
	(void)a;
	(void)out;
	if (!Mode_Import("RaceMaker") || !LvN_SaveList("Hangar.availableWeaponsList", BM, l)) {
		return Mode_Done(&t, 0);
	}
	int in = Native_Contains(l, Mode_S(&t, "Inferno"));
	if (in < 0) {
		return Mode_Done(&t, 0);
	}
	if (!in) {
		Mode_Lang("Mission_WinMortalRace_SystemMessage3", msg);
		if (!PORT_CALL(RaceMaker, ShowSystemMessage, NULL, "v", msg) || !LvN_SaveAppend("Hangar.availableWeaponsList", l, Mode_S(&t, "Inferno"))) {
			return Mode_Done(&t, 0);
		}
		SaveVar_Inc("NewBluePrintAvaliable", 1);
	}
	return Mode_Done(&t, Mode_Later(0.1, BM, "TalkToDeepThroat", NULL, 0, NULL));
}

static byte Back_TalkToDeepThroatC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return LV_COM("Mission_WinMortalRace_RemoteMessage4", "DeepThroat", Mission_BackFromMortalRace, AfterDeepThroatMessageShown, NULL);
}

static byte Back_AfterDeepThroatMessageShownC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("DTritusDesktop") && Gd_PutEnt(BM, "dd", "DM_DTritusDesktop") && Gd_GTarget(BM, "dd") &&
		Mode_MMDesc("Mission_TalkToDeepThroat10_MissionText1", 1) && Gd_Dialog(BM, "DeepThroatSpeech");
}

static byte Back_DeepThroatSpeechC(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	return LvN_Speakers(&a[0], &a[1]) && LV_ONEND(Mission_BackFromMortalRace, SetNextMission, NULL) &&
		Mode_NISpeech("DeepThroat", "Mission_TalkToDeepThroat10_ComDeepThroat_NI_1Msg");
}

static byte Back_SetNextMissionC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("DTritusDesktop") && LvN_ModNone("DTritusDesktop", "DialogController") && LvN_GActivate(BM, "dd", 0) && Mode_WayPoint() &&
		LvN_Next("Mission_DestroyBadDebtors", "Outskirts", "Mission_MeetBankMaster_MissionText1");
}

LV_FN(Back, InitStatus, 1, Back_InitStatusC)
LV_FN(Back, Init, 1, Back_InitC)
LV_FN(Back, TalkToDeepThroat, 0, Back_TalkToDeepThroatC)
LV_FN(Back, AfterDeepThroatMessageShown, 0, Back_AfterDeepThroatMessageShownC)
LV_FN(Back, DeepThroatSpeech, 4, Back_DeepThroatSpeechC)
LV_FN(Back, SetNextMission, 0, Back_SetNextMissionC)

PyMethodDef g_GamblinDenBackFromMortalRaceMethods[] = {
	{ "InitStatus", Back_InitStatus, 1, NULL },
	{ "Init", Back_Init, 1, NULL },
	{ "TalkToDeepThroat", Back_TalkToDeepThroat, 1, NULL },
	{ "AfterDeepThroatMessageShown", Back_AfterDeepThroatMessageShown, 1, NULL },
	{ "DeepThroatSpeech", Back_DeepThroatSpeech, 1, NULL },
	{ "SetNextMission", Back_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_CrazyGamblerFirstContact: the scene of the crazy gambler and his first talk ---- */

static byte Meet_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	SaveVar_Set("ComeFrom", "");
	SaveVar_Set("Spawn", "DM_Player_Spawn_Meeting_Room");
	return 1;
}

/* Init(LevelPath): the elevator unused, the crazy gambler the target, the scene the first time */
static byte Meet_InitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (!Mode_EntSetI("DM_Door_Elevator_Outskirts", "ArrowCanUse", 0) || !Gd_PutEnt(CF, "cg", "CrazyGambler_Default") || !Gd_GTarget(CF, "cg") ||
		!Gd_RoutineG(CF, "CrazyGamblerFirstSpeech")) {
		return 0;
	}
	if (LvN_SaveIs("LaunchScene", "1")) {
		return PORT_LOWN(Mission_CrazyGamblerFirstContact, LoadScene, NULL, "") && PORT_CALL(Scene, Play, NULL, "s", "MeetGambler") &&
			LvN_ModSetGlobal("Scene", "EndOfSceneCallback", CF, "OnEndOfScene");
	}
	return LvN_SetMusic("Music/GDloop1.ogg", 1.0);
}

/* in the demo: how to use, for secs */
static byte Cf_DemoHint(const char *key, long secs) {
	nativeVal_t demo = { 0 }, text = { 0 };
	Native_SetNone(&text);
	if (!LvN_ModGet("Init", "isDemo", &demo)) {
		return 0;
	}
	byte ok = !Native_True(&demo) || (Mode_KeyText(key, "Use", &text) && Mode_MsgText(&text, Mode_Time() + (double)secs));
	Native_Free(&demo);
	Native_Free(&text);
	return ok;
}

static byte Meet_OnEndOfSceneC(void) {
	if (!Mode_Import("Init")) {
		return 0;
	}
	SaveVar_Set("LaunchScene", "0");
	return LvN_SetMusic("Music/GDloop1.ogg", 1.0) && Cf_DemoHint("Mission_Decontamination_SystemMessage9", 10);
}

static byte Meet_CrazyGamblerFirstSpeechC(const nativeVal_t *a, nativeVal_t *out) {
	(void)out;
	if (!Mode_Import("Init")) {
		return 0;
	}
	return !LvN_EqS(&a[2], "Talk") || (LV_ONEND(Mission_CrazyGamblerFirstContact, EndOfTalk, NULL) &&
		Mode_NISpeech(NULL, "Mission_FirstMeetCrazyGambler_CrazyGambler_NI_1Msg") && Cf_DemoHint("Mission_Decontamination_SystemMessage8", 7));
}

/* EndOfTalk(char=None): his second talk from now on, the elevator back, the next mission */
static byte Meet_EndOfTalkC(void) {
	return Mode_Import("CrazyGambler") && Gd_RoutineG(CF, "CrazyGamblerSecondSpeech") && LvN_GActivate(CF, "cg", 0) && Mode_WayPoint() &&
		Mode_EntSetI("DM_Door_Elevator_Outskirts", "ArrowCanUse", 1) && PORT_LOWN(Mission_CrazyGamblerFirstContact, SetNextMission, NULL, "");
}

static byte Meet_SetNextMissionC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t demo = { 0 };
	(void)a;
	(void)out;
	if (!Mode_Import("Init") || !LvN_ModGet("Init", "isDemo", &demo)) {
		return 0;
	}
	if (!Native_True(&demo)) {
		SaveVar_Set("PoliceBossAtPolice", "1");
	}
	Native_Free(&demo);
	SaveVars_Delete("LaunchScene");
	return Mode_Target("") && Mode_WayPoint() &&
		LvN_Next("Mission_KillPolice", "Outskirts", "Mission_DestroyPolice_MissionText1");
}

static const char *const meet_scene[] = {
	"Scene.Load|MeetGambler",
	"Scene.ActorAction|0|CrazyGambler|RelaxHand",
	"Scene.ActorAction|75|CrazyGambler|TalkHand",
	"Scene.ActorAction|145|CrazyGambler|TalkHand1",
	"Scene.ActorAction|175|CrazyGambler|TalkHand",
	"Scene.ActorAction|205|CrazyGambler|TalkHand2",
	"Scene.ActorAction|243|CrazyGambler|TalkHand",
	"Scene.ActorAction|295|CrazyGambler|TalkHand1",
	"Scene.ActorAction|325|CrazyGambler|TalkHand",
	"Scene.ActorAction|345|CrazyGambler|TalkHand2",
	"Scene.ActorAction|390|CrazyGambler|TurnHand",
	"Scene.ActorAction|410|CrazyGambler|TalkHand",
	"Scene.ActorAction|475|CrazyGambler|TalkHand1",
	"Scene.ActorAction|505|CrazyGambler|TalkHand",
	"Scene.ActorAction|575|CrazyGambler|PutHead",
	"Scene.ActorAction|685|CrazyGambler|TalkHi",
	"Scene.ActorAction|775|CrazyGambler|Talk1",
	"Scene.ActorAction|805|CrazyGambler|Talk",
	"Scene.ActorAction|850|CrazyGambler|Relax",
	"Scene.ActorAction|0|CrazyHead|RelaxHand",
	"Scene.ActorAction|75|CrazyHead|TalkHand",
	"Scene.ActorAction|145|CrazyHead|TalkHand1",
	"Scene.ActorAction|175|CrazyHead|TalkHand",
	"Scene.ActorAction|205|CrazyHead|TalkHand2",
	"Scene.ActorAction|243|CrazyHead|TalkHand",
	"Scene.ActorAction|295|CrazyHead|TalkHand1",
	"Scene.ActorAction|325|CrazyHead|TalkHand",
	"Scene.ActorAction|345|CrazyHead|TalkHand2",
	"Scene.ActorAction|390|CrazyHead|TurnHand",
	"Scene.ActorAction|410|CrazyHead|TalkHand",
	"Scene.ActorAction|475|CrazyHead|TalkHand1",
	"Scene.ActorAction|505|CrazyHead|TalkHand",
	"Scene.ActorAction|575|CrazyHead|PutHead",
	"Scene.ActorAction|685|CrazyHead|Invisible",
	"SAct.SetAct|AddEventAtTime|^SAct.GetAct:TimeOfFrame0",
	"SAct.SetAct|EventName|MeetGamblerCrazyHead",
	"SAct.SetAct|EventFunc|!FXCrazyGamblerSetHeadModel",
	"SAct.SetAct|AddEventAtTime|^SAct.GetAct:TimeOfFrame665",
	"SAct.SetAct|EventName|MeetGamblerCrazyGambler",
	"SAct.SetAct|EventFunc|!FXCrazyGamblerSetHeadModel",
	"Scene.AddText|75|Mission_Scene_FirstMeetCrazyGambler_1|17",
	"Scene.AddText|685|Mission_Scene_FirstMeetCrazyGambler_2|10",
	"Scrap.Set|DefInterpTime|1.0",
	"Scene.AddCharActor|CrazyGambler|CrazyGambler",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk|Talk|1",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk0|Talk0|1",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk1|Talk1|1",
	"MakeChar.CreateRelaxAction|CrazyGambler|0|Talk2|Talk2|1",
	"Scene.CreateLocalAction|CrazyGambler|0|TalkHi|TalkHi|1",
	"Scene.CreateLocalAction|CrazyGambler|0|RelaxHand|RelaxHand|1",
	"Scene.CreateLocalAction|CrazyGambler|0|TalkHand|TalkHand|1",
	"Scene.CreateLocalAction|CrazyGambler|0|TalkHand1|TalkHand1|1",
	"Scene.CreateLocalAction|CrazyGambler|0|TalkHand2|TalkHand2|1",
	"Scene.CreateLocalAction|CrazyGambler|0|TurnHand|TurnHand|1",
	"Scene.CreateLocalAction|CrazyGambler|0|PutHead|PutHead|1",
	"Scrap.Set|DefInterpTime|0.5",
	"Scene.SetDefSounds|CrazyGambler|CrazyGambler",
	"SAct.SetCls|DefAction|RelaxHand",
	"Scene.AddLocalActor|CrazyHead|CrazyHead",
	"Scene.CreateLocalAction|CrazyHead|0|RelaxHand|RelaxHand|1",
	"Scene.CreateLocalAction|CrazyHead|0|Invisible|Invisible|1",
	"Scene.CreateLocalAction|CrazyHead|0|TalkHand|TalkHand|1",
	"Scene.CreateLocalAction|CrazyHead|0|TalkHand1|TalkHand1|1",
	"Scene.CreateLocalAction|CrazyHead|0|TalkHand2|TalkHand2|1",
	"Scene.CreateLocalAction|CrazyHead|0|TurnHand|TurnHand|1",
	"Scene.CreateLocalAction|CrazyHead|0|PutHead|PutHead|1",
	"Scrap.Set|DefInterpTime|0.5",
	"SAct.SetCls|DefAction|RelaxHand",
	"Scrap.Set|DefInterpTime|^Scrap.Def:DefInterpTime",
	NULL,
};

static byte Meet_LoadSceneC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Calls(CF, meet_scene);
}

LV_FN(Meet, InitStatus, 1, Meet_InitStatusC)
LV_FN(Meet, Init, 1, Meet_InitC)
GD_KW1(Meet, OnEndOfScene, "usr", Meet_OnEndOfSceneC())
LV_FN(Meet, CrazyGamblerFirstSpeech, 4, Meet_CrazyGamblerFirstSpeechC)
MODE_FN(Meet, CrazyGamblerSecondSpeech, 4, Gd_Talk(a, 0, "Mission_DestroyPolice_CrazyGambler_InitMsg1",
	"Mission_DestroyPolice_CrazyGambler_InitAns1"))
GD_KW1(Meet, EndOfTalk, "char", Meet_EndOfTalkC())
LV_FN(Meet, SetNextMission, 0, Meet_SetNextMissionC)
LV_FN(Meet, LoadScene, 0, Meet_LoadSceneC)

PyMethodDef g_GamblinDenCrazyGamblerFirstContactMethods[] = {
	{ "InitStatus", Meet_InitStatus, 1, NULL },
	{ "Init", Meet_Init, 1, NULL },
	{ "OnEndOfScene", (PyCFunction)(void (*)(void))Meet_OnEndOfScene, 3, NULL },
	{ "CrazyGamblerFirstSpeech", Meet_CrazyGamblerFirstSpeech, 1, NULL },
	{ "CrazyGamblerSecondSpeech", Meet_CrazyGamblerSecondSpeech, 1, NULL },
	{ "EndOfTalk", (PyCFunction)(void (*)(void))Meet_EndOfTalk, 3, NULL },
	{ "SetNextMission", Meet_SetNextMission, 1, NULL },
	{ "LoadScene", Meet_LoadScene, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ArrangeMortalRace: the mortal race in the FZ ---- */

static byte Mortal_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	Gd_Den(NULL, "0", "0", "1", "1");
	return 1;
}

static byte Mortal_InitC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Gd_PutEnt(AM, "cg", "CrazyGambler_Default") && Mode_GSetS(AM, "cg", "UseFunc", "Mission_ArrangeMortalRace.CrazyGamblerSpeech") &&
		LvN_GActivate(AM, "cg", 1);
}

LV_FN(Mortal, InitStatus, 1, Mortal_InitStatusC)
LV_FN(Mortal, Init, 1, Mortal_InitC)
MODE_FN(Mortal, CrazyGamblerSpeech, 4, Gd_Arrange(a, "Mission_ArrangeMortalRace_CrazyGambler",
	&PORT_LREF(Mission_ArrangeMortalRace, LaunchMortalRace)))
MODE_FN(Mortal, LaunchMortalRace, 0, Gd_Launch("Mission_WinMortalRace_MissionText1", "FZ", "Mission_MortalRace", "MortalRace"))

PyMethodDef g_GamblinDenArrangeMortalRaceMethods[] = {
	{ "InitStatus", Mortal_InitStatus, 1, NULL },
	{ "Init", Mortal_Init, 1, NULL },
	{ "CrazyGamblerSpeech", Mortal_CrazyGamblerSpeech, 1, NULL },
	{ "LaunchMortalRace", Mortal_LaunchMortalRace, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ThreeStooges: the first contact with the three mercenaries ---- */

static byte Stooges_InitStatusC(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t fn = { 0 };
	(void)a;
	(void)out;
	byte ok = LvN_GObj(TS, "AfterBrowseNPC", &fn) && LvN_ModAppend("CharsNPC", "AfterBrowseNPC", &fn);
	Native_Free(&fn);
	return ok;
}

static byte Stooges_InitC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const names[] = { "MercenaryA_Smartie", "MercenaryB_Dumber", "MercenaryC_Brutus", NULL };
	nativeVal_t l = { 0 };
	(void)a;
	(void)out;
	Mode_Texts(&l, names, -1);
	byte ok = PORT_CALL(MissionsFuncs, SetMultipleMissionTargets, NULL, "v", &l);
	Native_Free(&l);
	return ok;
}

static byte Stooges_AfterBrowseNPCC(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("CharsNPC") && Gd_CallbackG("MercenaryA_Smartie", TS, "CreateMercenaryAInit") &&
		Gd_CallbackG("MercenaryB_Dumber", TS, "CreateMercenaryBInit") && Gd_CallbackG("MercenaryC_Brutus", TS, "CreateMercenaryCInit");
}

static const gdMerc_t stooges_mercA = { "Mission_ThreeStooges", "Mission_FirstContactMercs_SmartMerc",
	"Mission_FirstContactMercs_SmartMerc_InitMsg1", "Mission_FirstContactMercs_SmartMerc_InitAns1", NULL, NULL, 0 };
static const gdMerc_t stooges_mercC = { "Mission_ThreeStooges", "Mission_FirstContactMercs_AverageMerc",
	"Mission_FirstContactMercs_AverageMerc_InitMsg1", "Mission_FirstContactMercs_AverageMerc_InitAns1", NULL, NULL, 0 };
static const gdMerc_t stooges_mercB = { "Mission_ThreeStooges", "Mission_FirstContactMercs_StupidMerc",
	"Mission_FirstContactMercs_StupidMerc_InitMsg2", "Mission_FirstContactMercs_StupidMerc_InitAns2",
	&PORT_LREF(Mission_ThreeStooges, SetNextMission), "alreadyConvinced", 1 };

/* SetNextMission(CharName): convinced; the mercenaries no targets, the next mission of NextMissionData */
static byte Stooges_SetNextMissionC(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const mercs[] = { "MercenaryA_Smartie", "MercenaryB_Dumber", "MercenaryC_Brutus" };
	(void)out;
	if (!Mode_Import("Speech") || !LvN_PutI(TS, "alreadyConvinced", 1) || !PORT_CALL(Speech, RestoreState, NULL, "v", &a[0])) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		if (!PORT_CALL(MissionsFuncs, DelMissionTarget, NULL, "s", mercs[i])) {
			return 0;
		}
	}
	return Mode_ApplyNext(TS) && Mode_MMDesc("Mission_FollowStupidMerc_MissionText1", 1);
}

/* CreateMercenary?Init(e): the talk of the mercenary in this mission */
static byte Stooges_UseFunc(const nativeVal_t *a, const char *use) {
	return LvN_SetS(&a[0], "UseFunc", use);
}

LV_FN(Stooges, InitStatus, 1, Stooges_InitStatusC)
LV_FN(Stooges, Init, 1, Stooges_InitC)
LV_FN(Stooges, AfterBrowseNPC, 0, Stooges_AfterBrowseNPCC)
MODE_FN(Stooges, CreateMercenaryAInit, 1, Stooges_UseFunc(a, "Mission_ThreeStooges.MercenaryASpeech"))
MODE_FN(Stooges, CreateMercenaryBInit, 1, Stooges_UseFunc(a, "Mission_ThreeStooges.MercenaryBSpeech"))
MODE_FN(Stooges, CreateMercenaryCInit, 1, Stooges_UseFunc(a, "Mission_ThreeStooges.MercenaryCSpeech"))
MODE_FN(Stooges, MercenaryASpeech, 4, Gd_Merc(a, TS, &stooges_mercA))
MODE_FN(Stooges, MercenaryCSpeech, 4, Gd_Merc(a, TS, &stooges_mercC))
MODE_FN(Stooges, MercenaryBSpeech, 4, Gd_Merc(a, TS, &stooges_mercB))
LV_FN(Stooges, SetNextMission, 1, Stooges_SetNextMissionC)

PyMethodDef g_GamblinDenThreeStoogesMethods[] = {
	{ "InitStatus", Stooges_InitStatus, 1, NULL },
	{ "Init", Stooges_Init, 1, NULL },
	{ "AfterBrowseNPC", Stooges_AfterBrowseNPC, 1, NULL },
	{ "CreateMercenaryAInit", Stooges_CreateMercenaryAInit, 1, NULL },
	{ "CreateMercenaryBInit", Stooges_CreateMercenaryBInit, 1, NULL },
	{ "CreateMercenaryCInit", Stooges_CreateMercenaryCInit, 1, NULL },
	{ "MercenaryASpeech", Stooges_MercenaryASpeech, 1, NULL },
	{ "MercenaryCSpeech", Stooges_MercenaryCSpeech, 1, NULL },
	{ "MercenaryBSpeech", Stooges_MercenaryBSpeech, 1, NULL },
	{ "SetNextMission", Stooges_SetNextMission, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(levels_GamblinDen)
