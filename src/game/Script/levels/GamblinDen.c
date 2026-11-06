/*
 * The scripts of the level GamblinDen in C (levels/gamblinden/scripts, issue #2): the Map module (the characters of
 * the den by the save variables, the drinking and dancing mercenaries, the talking NPCs, the music and the hire of
 * the mercenaries) and the twelve missions of the level. The scenes are call tables (Sf_Calls). MapSnd is data
 * (data/levels/gamblinden/mapsnd.def). The module variables (Borrachera, Cheer, the flags MayorAndBanker, Twins,
 * Bankers, Bishops, Polices, BandH, BandB, the characters the missions keep, their counters) and the module code (the
 * save variables, the alarm off) stay in the scripts.
 */
#include "scrap/script.h"

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

#define ENT(n) Sf_Ask(&f, "Scrap", "GetEntity", "(s)", n)
#define USR() Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0)
#define LANG(k) Sf_Lang(&f, Sf_Str(&f, k))
#define SAVE(n) Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", n)
#define G(m, n) Sf_Global(&f, m, n)
#define NOARGS Sf_Build(&f, "()")

/* CharsNPC.CharRes[name].CallbackFunc.append(fn) */
static byte Gd_Callback(scriptFrame_t *f, const char *name, PyObject *fn) {
	int mark = f->n;
	PyObject *res = Sf_GetItem(f, Sf_Mod(f, "CharsNPC", "CharRes"), Sf_Str(f, name));
	Sf_Method(f, Sf_Attr(f, res, "CallbackFunc"), "append", "(O)", fn);
	Sf_Release(f, mark);
	return !f->failed;
}

/* CharsNPC.CreateMissionChar(name) with the callback fn */
static byte Gd_MissionChar(scriptFrame_t *f, const char *name, PyObject *fn) {
	return Sf_Run(f, "CharsNPC", "CreateMissionChar", "(s)", name) && Gd_Callback(f, name, fn);
}

/* Scrap.SetSaveVar of pairs of name and value, ending with NULL */
static byte Gd_Saves(scriptFrame_t *f, const char *const *pairs) {
	for (; *pairs && !f->failed; pairs += 2) {
		Sf_Run(f, "Scrap", "SetSaveVar", "(ss)", pairs[0], pairs[1]);
	}
	return !f->failed;
}

/* the characters of the den the mission wants: MayorAtGambinDen, FunctionaryTwins..., Bankers..., Bishops...,
 * Polices... (NULL keeps a variable) */
static byte Gd_Den(scriptFrame_t *f, const char *mayor, const char *twins, const char *bankers, const char *bishops, const char *polices) {
	static const char *const names[] = { "MayorAtGambinDen", "FunctionaryTwinsAtGambinDen", "BankersAtGambinDen", "BishopsAtGambinDen",
		"PolicesAtGambinDen" };
	const char *vals[] = { mayor, twins, bankers, bishops, polices };
	for (int i = 0; i < 5 && !f->failed; i++) {
		if (vals[i]) {
			Sf_Run(f, "Scrap", "SetSaveVar", "(ss)", names[i], vals[i]);
		}
	}
	return !f->failed;
}

/* `if optId == "Talk": Speech.OnSpch(msg, ans)` with the names of the talk set first (names 0: not set) */
static PyObject *Gd_Talk(PyObject *args, int names, const char *msg, const char *ans) {
	PyObject *usr, *ch, *opt, *full;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	if (names) {
		Sf_Import(&f, "Speech");
		Sf_SpeechNames(&f, usr, ch);
	}
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		Sf_Run(&f, "Speech", "OnSpch", "(ss)", msg, ans);
	}
	return Sf_None(&f);
}

/* prefix + suffix into buf */
static const char *Gd_Key(char *buf, int size, const char *prefix, const char *suffix) {
	Str_Printf(buf, size, "%s%s", prefix, suffix);
	return buf;
}

/*
 * CrazyGamblerSpeech of the missions that arrange a deal with the crazy gambler: the first line and option _1, then
 * _1_1 (accept: the movie scorer, no input, the launch function of module at the end of the talk) or _1_2
 */
static PyObject *Gd_Arrange(PyObject *args, const char *module, const char *prefix, const char *launch) {
	PyObject *usr, *ch, *opt, *full;
	char k1[0x80], k2[0x80], k3[0x80];
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_SpeechNames(&f, usr, ch);
	int talk = Sf_EqS(&f, opt, "Talk");
	if (talk > 0) {
		Sf_Run(&f, "Speech", "OnSpch", "(ss)", Gd_Key(k1, sizeof(k1), prefix, "_InitMsg1"), Gd_Key(k2, sizeof(k2), prefix, "_InitAns1"));
		Sf_Run(&f, "Speech", "StdAnsLst", "([s])", Gd_Key(k1, sizeof(k1), prefix, "_1"));
	} else if (talk == 0) {
		Sf_Run(&f, "Speech", "StdOnSpch", "(O)", opt);
		if (Sf_EqS(&f, opt, Gd_Key(k1, sizeof(k1), prefix, "_1")) > 0) {
			Sf_Run(&f, "Speech", "StdAnsLst", "([ss])", Gd_Key(k2, sizeof(k2), prefix, "_1_1"), Gd_Key(k3, sizeof(k3), prefix, "_1_2"));
		} else if (!f.failed && Sf_EqS(&f, opt, Gd_Key(k2, sizeof(k2), prefix, "_1_1")) > 0) {
			Sf_Import(&f, "Menu");
			Sf_Run(&f, "Menu", "MovieScorer", "(i)", 0);
			Sf_Run(&f, "SInput", "SetActionSet", "(s)", "Inactive");
			Sf_SpeechEnd(&f, G(module, launch), NULL);
		}
	}
	return Sf_None(&f);
}

/*
 * the talk of a mercenary in the missions of the three mercenaries: in its own mission the line _1, then _1_1 and
 * _1_1_1 (with end, the end function of module, params (CharName,) when withChar), else a single line (otherMsg,
 * otherAns); convinced: the global of module that turns the first line into the second
 */
typedef struct {
	const char *library, *prefix, *otherMsg, *otherAns, *end, *convinced;
	byte withChar;
} gdMerc_t;

static PyObject *Gd_Merc(PyObject *args, const char *module, const gdMerc_t *m) {
	PyObject *usr, *ch, *opt, *full;
	char k1[0x80], k2[0x80], k3[0x80];
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOOO", &usr, &ch, &opt, &full)) {
		return NULL;
	}
	Sf_SpeechNames(&f, usr, ch);
	int own = Sf_EqS(&f, SAVE("Mission.Library"), m->library);
	if (own > 0) {
		int talk = Sf_EqS(&f, opt, "Talk");
		if (talk > 0) {
			int first = 1;
			if (m->convinced) {
				first = Sf_True(&f, G(module, m->convinced));
				first = first < 0 ? -1 : !first;
			}
			if (first > 0) {
				Sf_Run(&f, "Speech", "OnSpch", "(ss)", Gd_Key(k1, sizeof(k1), m->prefix, "_InitMsg1"), Gd_Key(k2, sizeof(k2), m->prefix, "_InitAns1"));
				Sf_Run(&f, "Speech", "StdAnsLst", "([s])", Gd_Key(k1, sizeof(k1), m->prefix, "_1"));
			} else if (first == 0) {
				Sf_Run(&f, "Speech", "OnSpch", "(ss)", Gd_Key(k1, sizeof(k1), m->prefix, "_InitMsg2"), Gd_Key(k2, sizeof(k2), m->prefix, "_InitAns2"));
			}
		} else if (talk == 0) {
			Sf_Run(&f, "Speech", "StdOnSpch", "(O)", opt);
			Gd_Key(k1, sizeof(k1), m->prefix, "_1");
			Gd_Key(k2, sizeof(k2), m->prefix, "_1_1");
			Gd_Key(k3, sizeof(k3), m->prefix, "_1_1_1");
			if (Sf_EqS(&f, opt, k1) > 0) {
				Sf_Run(&f, "Speech", "StdAnsLst", "([ss])", k2, k1);
			} else if (!f.failed && Sf_EqS(&f, opt, k2) > 0) {
				Sf_Run(&f, "Speech", "StdAnsLst", "([sss])", k3, k1, k2);
			} else if (!f.failed && Sf_EqS(&f, opt, k3) > 0) {
				Sf_Run(&f, "Speech", "StdAnsLst", "([s])", k3);
				if (m->end) {
					Sf_SpeechEnd(&f, G(module, m->end), m->withChar ? Sf_Build(&f, "(O)", ch) : NULL);
				}
			}
		}
	} else if (own == 0) {
		Sf_Run(&f, "Speech", "OnSpch", "(ss)", m->otherMsg, m->otherAns);
	}
	return Sf_None(&f);
}

/* target in front of ref, facing it (the talks that start right after a level load) */
static byte Gd_Face(scriptFrame_t *f, PyObject *target, PyObject *ref) {
	int mark = f->n;
	PyObject *dir = Sf_Ask(f, "SVec", "Rotate3D", "((ddd)O)", 0.0, 0.0, -1.0, Sf_Attr(f, ref, "Ang"));
	PyObject *pos = Sf_Attr(f, ref, "Pos");
	PyObject *d = Sf_OpI(f, Sf_Op(f, Sf_Attr(f, ref, "VRadius"), '+', Sf_Attr(f, target, "VRadius")), '+', 50);
	Sf_SetAttr(f, target, "Pos", Sf_Ask(f, "SVec", "Add", "(OO)", pos, Sf_Ask(f, "SVec", "Prod", "(OO)", dir, d)));
	PyObject *a0 = Sf_Item(f, Sf_Attr(f, ref, "Ang"), 0);
	PyObject *a1 = Sf_OpI(f, Sf_Item(f, Sf_Attr(f, ref, "Ang"), 1), '+', 180);
	PyObject *a2 = Sf_Item(f, Sf_Attr(f, ref, "Ang"), 2);
	Sf_SetAttr(f, target, "Ang", Sf_Build(f, "(OOO)", a0, a1, a2));
	Sf_Release(f, mark);
	return !f->failed;
}

/* MissionsFuncs.StartSuperDeal(map, library, mode) after the description desc */
static PyObject *Gd_Launch(PyObject *args, const char *desc, const char *map, const char *library, const char *mode) {
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "MissionsFuncs");
	Sf_Import(&f, "SaveGame");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", desc, 1);
	Sf_Run(&f, "MissionsFuncs", "StartSuperDeal", "(sss)", map, library, mode);
	return Sf_None(&f);
}

/* ---- Map ---- */

/* InitStatus(LevelPath): the characters that cannot be possessed here, indoors, the mission characters, the elevator
 * the player came by */
static PyObject *Den_InitStatus(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Init", "CharsNPC", "Elevator", "string", "CharConversor", "Chars", NULL };
	static const char *const out[] = { "Nurse", "Desktop", "Sentinel", "Gear", "Messenger", "Betty", "Mayor", "Berto", "PoliceBoss", NULL };
	PyObject *path;
	SF_ARGS("O", &path);
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	for (int i = 0; out[i] && !f.failed; i++) {
		int mark = f.n;
		PyObject *vl = Sf_Str(&f, out[i]);
		if (Sf_In(&f, vl, Sf_Mod(&f, "CharConversor", "AvailableChars")) > 0) {
			Sf_Method(&f, Sf_Mod(&f, "CharConversor", "AvailableChars"), "remove", "(O)", vl);
		}
		Sf_Release(&f, mark);
	}
	if (Sf_True(&f, Sf_Mod(&f, "Init", "isDemo")) > 0) {
		if (Sf_In(&f, Sf_Str(&f, "BankMaster"), Sf_Mod(&f, "CharConversor", "AvailableChars")) > 0) {
			Sf_Method(&f, Sf_Mod(&f, "CharConversor", "AvailableChars"), "remove", "(s)", "BankMaster");
		}
	}
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 0));
	Sf_SetMod(&f, "Init", "OutArea", Sf_Str(&f, "Outskirts"));
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "AfterBrowseNPC"), "append", "(O)", G("Map", "AfterBrowseNPC"));
	if (Sf_EqS(&f, SAVE("LonelyMercInGamblinDen"), "1") > 0) {
		Sf_Run(&f, "CharsNPC", "CreateMissionChar", "(s)", "MercenaryA_LonelyMerc");
		Sf_Run(&f, "Chars", "AddNewCharRes", "(s)", "MercenaryL");
		PyObject *res = Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "MercenaryA_LonelyMerc"));
		Sf_SetAttrS(&f, res, "ActCtrl", "MercenaryL");
		Gd_Callback(&f, "MercenaryA_LonelyMerc", G("Map", "CreateLonelyMerc"));
	}
	if (Sf_EqS(&f, SAVE("PoliceBossAtPolice"), "1") > 0) {
		Gd_MissionChar(&f, "PoliceBoss_Default", G("Map", "CreateFixedInit"));
	}
	if (Sf_EqS(&f, SAVE("CrazyGamblerAtGambinDen"), "1") > 0) {
		Gd_MissionChar(&f, "CrazyGambler_Default", G("Map", "CreateFixedInit"));
	}
	PyObject *pm = Sf_Ask(&f, "string", "upper", "(O)", Sf_Slice(&f, SAVE("PrevMap"), 7, 0x7fffffff));
	int yard = Sf_EqS(&f, pm, "SCRAPYARD");
	if (yard >= 0) {
		Sf_Run(&f, "Elevator", "SetDef", "(ss)", "Elevator", yard ? "DM_Door_Elevator_Scrapyard" : "DM_Door_Elevator_Outskirts");
	}
	return Sf_None(&f);
}

/* global name = (save var a == "1"), and when b: and save var b (== "1", or != "1" when notB) */
static int Den_Flag(scriptFrame_t *f, const char *name, const char *a, const char *b, int notB) {
	int mark = f->n;
	int v = Sf_EqS(f, Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", a), "1");
	if (v > 0 && b) {
		PyObject *s = Sf_Ask(f, "Scrap", "GetSaveVar", "(s)", b);
		v = notB ? Sf_Compare(f, s, "!=", Sf_Str(f, "1")) : Sf_EqS(f, s, "1");
	}
	if (v >= 0) {
		Sf_SetGlobal(f, "Map", name, Sf_Int(f, v));
	}
	Sf_Release(f, mark);
	return v;
}

/* global name = (a in CharsNPC.CharRes.keys()) and (b in ...) */
static int Den_Pair(scriptFrame_t *f, const char *name, const char *a, const char *b) {
	int mark = f->n;
	int v = Sf_In(f, Sf_Str(f, a), Sf_Method(f, Sf_Mod(f, "CharsNPC", "CharRes"), "keys", "()"));
	if (v > 0) {
		v = Sf_In(f, Sf_Str(f, b), Sf_Method(f, Sf_Mod(f, "CharsNPC", "CharRes"), "keys", "()"));
	}
	if (v >= 0) {
		Sf_SetGlobal(f, "Map", name, Sf_Int(f, v));
	}
	Sf_Release(f, mark);
	return v;
}

/* AfterBrowseNPC(): the mercenaries at the bar with their dance places, the friendly bishop and functionary, the pairs
 * of characters the save variables put in the den */
static PyObject *Den_AfterBrowseNPC(PyObject *self, PyObject *args) {
	static const char *const mercs[][2] = { { "MercenaryA_Smartie", "DM_DancingZone_MercenaryA" },
		{ "MercenaryB_Dumber", "DM_DancingZone_MercenaryB_Dumber" }, { "MercenaryC_Brutus", "DM_DancingZone_MercenaryC_Brutus" } };
	SF_ARGS("");
	Sf_Import(&f, "CharsNPC");
	for (int i = 0; i < 3 && !f.failed; i++) {
		int mark = f.n;
		Gd_Callback(&f, mercs[i][0], G("Map", "CreateFixedMercInit"));
		Sf_Run(&f, "Scrap", "StartDummySearch", "(si)", mercs[i][1], 0);
		PyObject *res = Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, mercs[i][0]));
		Sf_SetAttr(&f, res, "DancePos", Sf_Item(&f, Sf_Ask(&f, "Scrap", "NextDummySearch", "()"), 1));
		Sf_Release(&f, mark);
	}
	int boss = Sf_EqS(&f, SAVE("RobotsControlledByBoss"), "1");
	if (boss > 0) {
		Sf_DelItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "Bishop_Friendly35"));
		Sf_DelItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), Sf_Str(&f, "Functionary_Friendly00"));
	} else if (boss == 0) {
		Gd_Callback(&f, "Bishop_Friendly35", G("Map", "CreateFixedInit"));
		Gd_Callback(&f, "Functionary_Friendly00", G("Map", "CreateFixedInit"));
	}
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
	for (int i = 0; i < 5 && !f.failed; i++) {
		if (Den_Flag(&f, pairs[i].flag, pairs[i].a, pairs[i].b, pairs[i].notB) > 0) {
			Gd_MissionChar(&f, pairs[i].chars[0], G("Map", "CreateFixedInit"));
			Gd_MissionChar(&f, pairs[i].chars[1], G("Map", "CreateFixedInit"));
		}
	}
	if (Den_Pair(&f, "BandH", "Humphrey_Default", "Berto_Default") > 0) {
		Gd_Callback(&f, "Humphrey_Default", G("Map", "CreateFixedInit"));
		Gd_Callback(&f, "Berto_Default", G("Map", "CreateFixedInit"));
	}
	if (Den_Pair(&f, "BandB", "Betty_Default", "Berto_Default") > 0) {
		Gd_Callback(&f, "Betty_Default", G("Map", "CreateFixedInit"));
		Gd_Callback(&f, "Berto_Default", G("Map", "CreateFixedInit"));
	}
	return Sf_None(&f);
}

/* CreateFixedMercInit(e): fixed, the hire talk */
static PyObject *Den_CreateFixedMercInit(PyObject *self, PyObject *args) {
	PyObject *e;
	SF_ARGS("O", &e);
	Sf_Own(&f, "Map", "CreateFixedInit", "(O)", e);
	Sf_SetAttrS(&f, e, "UseFunc", "Map.MercenarySpeech");
	Sf_Print(&f, "s", "---------------");
	return Sf_None(&f);
}

/* CreateLonelyMerc(e): fixed, his own talk */
static PyObject *Den_CreateLonelyMerc(PyObject *self, PyObject *args) {
	PyObject *e;
	SF_ARGS("O", &e);
	Sf_Own(&f, "Map", "CreateFixedInit", "(O)", e);
	Sf_SetAttrS(&f, e, "UseFunc", "Map.LonelyMercSpeech");
	return Sf_None(&f);
}

/* `if Scrap.Rand(-1, 1) > 0: ChatNPCs(a, (a, b)) else: CharDance(a); CharDance(b)` */
static void Den_ChatOrDance(scriptFrame_t *f, const char *a, const char *b) {
	int chat = Sf_Compare(f, Sf_Ask(f, "Scrap", "Rand", "(ii)", -1, 1), ">", Sf_Int(f, 0));
	if (chat > 0) {
		Sf_Own(f, "Map", "ChatNPCs", "(s(ss))", a, a, b);
	} else if (chat == 0) {
		Sf_Own(f, "Map", "CharDance", "(s)", a);
		Sf_Own(f, "Map", "CharDance", "(s)", b);
	}
}

/* Init(LevelPath): the elevators, the life in the den, the music (another track each visit) */
static PyObject *Den_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Elevator");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_Outskirts", "Outskirts");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Elevator_Scrapyard", "Scrapyard");
	Sf_Own(&f, "Map", "MercDrink", "(s)", "MercenaryA_Smartie");
	Sf_Own(&f, "Map", "MercDrink", "(s)", "MercenaryB_Dumber");
	Sf_Own(&f, "Map", "MercDrink", "(s)", "MercenaryC_Brutus");
	if (Sf_EqS(&f, SAVE("LonelyMercInGamblinDen"), "1") > 0) {
		Sf_Own(&f, "Map", "MercDrink", "(s)", "MercenaryA_LonelyMerc");
	}
	Sf_Own(&f, "Map", "ChatNPCs", "(s)", "PoliceBoss_Default");
	if (Sf_EqS(&f, SAVE("RobotsControlledByBoss"), "0") > 0) {
		Sf_Own(&f, "Map", "CharDance", "(s)", "Bishop_Friendly35");
		Sf_Own(&f, "Map", "CharDance", "(s)", "Functionary_Friendly00");
	}
	Sf_EntSetS(&f, "DM_Door_GamblinDenChupito_00", "OnObjSlot", "");
	Sf_EntSetI(&f, "DM_Door_Teleporter_00", "Active", 0);
	int mark = f.n;
	if (Sf_True(&f, G("Map", "MayorAndBanker")) > 0) {
		Sf_Own(&f, "Map", "ChatNPCs", "(s(ss))", "BankDirector_Barman", "BankDirector_Barman", "Mayor_Drunk");
	}
	if (Sf_True(&f, G("Map", "Twins")) > 0) {
		Den_ChatOrDance(&f, "Functionary_Twins_01", "Functionary_Twins_02");
	}
	if (Sf_True(&f, G("Map", "Bankers")) > 0) {
		Den_ChatOrDance(&f, "BankDirector_00", "BankDirector_01");
	}
	if (Sf_True(&f, G("Map", "Bishops")) > 0) {
		Den_ChatOrDance(&f, "Bishop_00", "Bishop_01");
	}
	if (Sf_True(&f, G("Map", "Polices")) > 0) {
		Den_ChatOrDance(&f, "Police_00", "Police_01");
	}
	if (Sf_True(&f, G("Map", "BandH")) > 0) {
		Sf_Own(&f, "Map", "ChatNPCs", "(s(ss))", "Berto_Default", "Berto_Default", "Humphrey_Default");
	}
	if (Sf_True(&f, G("Map", "BandB")) > 0) {
		Sf_Own(&f, "Map", "ChatNPCs", "(s(ss))", "Berto_Default", "Berto_Default", "Betty_Default");
	}
	Sf_Release(&f, mark);
	if (Sf_EqS(&f, SAVE("Player.InfiniteLives"), "1") > 0) {
		Sf_EntSetS(&f, "DM_Door_Elevator_Scrapyard", "OnObjSlot", "!CloseDoorSlot");
	}
	if (Sf_Compare(&f, SAVE("Mission.Library"), "!=", Sf_Str(&f, "Mission_CrazyGamblerFirstContact")) > 0) {
		int gm = Sf_True(&f, Sf_ToInt(&f, SAVE("GamblinMusic")));
		if (gm >= 0) {
			gm = !gm;
			Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "GamblinMusic", Sf_ReprI(&f, gm));
			if (gm) {
				int human = Sf_Compare(&f, Sf_Ask(&f, "Scrap", "Rand", "(ii)", -1, 1), ">", Sf_Int(&f, 0));
				if (human >= 0) {
					Sf_Run(&f, "SSound", "SetMusic", "(sd)", human ? "Music/HumanGD.ogg" : "Music/GDloop1.ogg", 0.75);
				}
			} else {
				Sf_Run(&f, "SSound", "SetMusic", "(sd)", "Music/GDloop2.ogg", 0.75);
			}
		}
	}
	return Sf_None(&f);
}

/* LonelyMercSpeech(UsrName, CharName, optId, FullOption): the lonely mercenary of the matrix mission */
static PyObject *Den_LonelyMercSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_Import(&f, "Speech");
	Sf_SpeechNames(&f, usr, ch);
	int talk = Sf_EqS(&f, opt, "Talk");
	if (talk > 0) {
		PyObject *r = Sf_Ask(&f, "Speech", "OnSpch", "(ss)", "Mission_FindDtritusMatrix1_Mercenary_InitMsg1",
			"Mission_FindDtritusMatrix1_Mercenary_InitAns1");
		if (Sf_True(&f, r) > 0) {
			Sf_Run(&f, "Speech", "StdAnsLst", "([s])", "Mission_FindDtritusMatrix1_Mercenary_1");
		}
	} else if (talk == 0) {
		Sf_Run(&f, "Speech", "OnSpch", "(OO)", Sf_Cat(&f, "Os", opt, "Msg"), Sf_Cat(&f, "Os", opt, "Ans"));
	}
	return Sf_None(&f);
}

/* CharUsrAct(charname, ActName): an idle relaxing character plays the action: 1, else 0 */
static PyObject *Den_CharUsrAct(PyObject *self, PyObject *args) {
	PyObject *name, *act;
	SF_ARGS("OO", &name, &act);
	PyObject *ch = Sf_Entity(&f, name);
	int go = Sf_EqI(&f, Sf_Attr(&f, ch, "LLogicIdle"), 0);
	if (go > 0) {
		go = Sf_EqS(&f, Sf_Slice(&f, Sf_Attr(&f, ch, "Action"), 0, 5), "Relax");
	}
	if (go > 0) {
		Sf_SetAttr(&f, ch, "Action", act);
		return Sf_Return(&f, Sf_Int(&f, 1));
	}
	return Sf_Return(&f, go == 0 ? Sf_Int(&f, 0) : NULL);
}

/* CharUsrDance(charname, state): a mercenary dances for state rounds of 5 s, then back to the bar */
static PyObject *Den_CharUsrDance(PyObject *self, PyObject *args) {
	PyObject *name, *state;
	SF_ARGS("OO", &name, &state);
	Sf_Import(&f, "CharsNPC");
	PyObject *ch = Sf_Entity(&f, name);
	int good = Sf_EqI(&f, Sf_Attr(&f, ch, "LLogicIdle"), 0);
	int cheer = Sf_In(&f, Sf_Attr(&f, ch, "Name"), G("Map", "Cheer"));
	if (cheer == 0) {
		Sf_Method(&f, G("Map", "Cheer"), "append", "(O)", name);
	}
	int dance = Sf_Compare(&f, state, ">", Sf_Int(&f, 0));
	if (dance > 0) {
		int even = Sf_EqI(&f, Sf_OpI(&f, state, '%', 2), 0);
		const char *anm = even > 0 ? "Dance" : "Dance0";
		int go = even >= 0 ? good : -1;
		if (go > 0) {
			go = Sf_Compare(&f, Sf_Attr(&f, ch, "Action"), "!=", Sf_Str(&f, anm));
		}
		if (go > 0) {
			Sf_SetAttr(&f, ch, "Action", Sf_Cat(&f, "ss", anm, "Init"));
			Sf_SetAttrS(&f, ch, "DefLoop", anm);
		}
		Sf_Later(&f, Sf_Float(&f, 5.0), G("Map", "CharUsrDance"), Sf_Build(&f, "(OO)", name, Sf_OpI(&f, state, '-', 1)), NULL);
	} else if (dance == 0) {
		Sf_Own(&f, "Map", "MercDrink", "(Oi)", name, 0);
		Sf_SetAttr(&f, ch, "LLogicOrigin", Sf_Attr(&f, Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), name), "Pos"));
	}
	return Sf_None(&f);
}

/* MercDrink(merc, ctr=0): the drinkers of the bar: cheer, drink, talk, now and then one goes dancing */
static PyObject *Den_MercDrink(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "merc", "ctr", NULL };
	static const char *const xtracts[] = { "Talk1", "Talk0", "Talk1", "Talk0", "Talk1", "Talk0", "Relax0", "Relax1" };
	PyObject *merc, *ctr = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|O", names, &merc, &ctr)) {
		return NULL;
	}
	if (!ctr) {
		ctr = Sf_Int(&f, 0);
	}
	Sf_Import(&f, "CharsNPC");
	PyObject *mc = Sf_Entity(&f, merc);
	int islm = Sf_EqS(&f, Sf_Attr(&f, mc, "ActCtrl"), "MercenaryL");
	int cheered = Sf_In(&f, merc, G("Map", "Cheer"));
	if (cheered == 0) {
		Sf_Method(&f, G("Map", "Cheer"), "append", "(O)", merc);
		Sf_Own(&f, "Map", "CharUsrAct", "(Os)", merc, "Cheer");
	} else if (cheered > 0) {
		int busy = Sf_Compare(&f, Sf_Ask(&f, "Scrap", "Rand", "(di)", -1.5, 1), ">", Sf_Int(&f, 0));
		if (busy == 0) {
			busy = islm;
		}
		if (busy > 0) {
			int tired = Sf_Compare(&f, ctr, ">=", Sf_Int(&f, 3));
			if (tired > 0) {
				tired = islm < 0 ? -1 : !islm;
			}
			if (tired > 0) {
				int drunk = Sf_Compare(&f, G("Map", "Borrachera"), ">", Sf_Int(&f, 0));
				if (drunk > 0) {
					Sf_SetGlobal(&f, "Map", "Borrachera", Sf_OpI(&f, G("Map", "Borrachera"), '-', 1));
				} else if (drunk == 0) {
					PyObject *res = Sf_GetItem(&f, Sf_Mod(&f, "CharsNPC", "CharRes"), merc);
					Sf_SetAttr(&f, mc, "LLogicOrigin", Sf_Attr(&f, res, "DancePos"));
					Sf_Own(&f, "Map", "CharUsrDance", "(OO)", merc, Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "Rand", "(ii)", 10, 20)));
					Sf_SetGlobal(&f, "Map", "Borrachera", Sf_Int(&f, 5));
					return Sf_None(&f);
				}
				ctr = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "Rand", "(ii)", 0, 3));
			} else if (tired == 0) {
				Sf_Own(&f, "Map", "CharUsrAct", "(Os)", merc, "Drink");
				if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "Rand", "(ii)", -1, 4), "<", Sf_Int(&f, 0)) > 0) {
					Sf_SetGlobal(&f, "Map", "Cheer", Sf_Build(&f, "[]"));
				}
				ctr = Sf_OpI(&f, ctr, '+', 1);
			}
		} else if (busy == 0) {
			PyObject *i = Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "Rand", "(id)", 0, 7.9));
			if (i) {
				PyObject *t = Sf_Build(&f, "(ssssssss)", xtracts[0], xtracts[1], xtracts[2], xtracts[3], xtracts[4], xtracts[5], xtracts[6], xtracts[7]);
				Sf_Own(&f, "Map", "CharUsrAct", "(OO)", merc, Sf_GetItem(&f, t, i));
			}
		}
	}
	Sf_Later(&f, Sf_Ask(&f, "Scrap", "Rand", "(di)", 3.5, 5), G("Map", "MercDrink"), Sf_Build(&f, "(OO)", merc, ctr), NULL);
	return Sf_None(&f);
}

/* ChatNPCs(Name, Vals=("PoliceBoss_Default", "CrazyGambler_Default")): two characters talk in turn every 2.5 s */
static PyObject *Den_ChatNPCs(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "Name", "Vals", NULL };
	PyObject *name, *vals = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|O", names, &name, &vals)) {
		return NULL;
	}
	if (!vals) {
		vals = Sf_Build(&f, "(ss)", "PoliceBoss_Default", "CrazyGambler_Default");
	}
	if (Sf_True(&f, Sf_Entity(&f, name)) > 0) {
		PyObject *xtracts = Sf_Build(&f, "(sss)", "Talk0", "Talk1", "Talk2");
		PyObject *act = Sf_GetItem(&f, xtracts, Sf_ToInt(&f, Sf_Ask(&f, "Scrap", "Rand", "(id)", 0, 2.9)));
		if (Sf_True(&f, Sf_Own(&f, "Map", "CharUsrAct", "(OO)", name, act)) > 0) {
			int second = Sf_Compare(&f, Sf_Item(&f, vals, 1), "==", name);
			if (second >= 0) {
				name = Sf_Item(&f, vals, second ? 0 : 1);
			}
		}
		Sf_Later(&f, Sf_Float(&f, 2.5), G("Map", "ChatNPCs"), Sf_Build(&f, "(OO)", name, vals), NULL);
	}
	return Sf_None(&f);
}

/* CharDance(Name): a dancer between its two dances, every 5 to 10 s */
static PyObject *Den_CharDance(PyObject *self, PyObject *args) {
	PyObject *name;
	SF_ARGS("O", &name);
	PyObject *ch = Sf_Entity(&f, name);
	int dance = Sf_EqS(&f, Sf_Attr(&f, ch, "Action"), "Dance");
	if (dance > 0) {
		Sf_SetAttrS(&f, ch, "Action", "Dance0");
		Sf_SetAttrS(&f, ch, "DefLoop", "Dance");
	} else if (dance == 0) {
		Sf_Own(&f, "Map", "CharUsrAct", "(Os)", name, "Dance");
	}
	Sf_Later(&f, Sf_Ask(&f, "Scrap", "Rand", "(ii)", 5, 10), G("Map", "CharDance"), Sf_Build(&f, "(O)", name), NULL);
	return Sf_None(&f);
}

/* MercenarySpeech(UsrName, CharName, optId, FullOption): hire a mercenary for three lives */
static PyObject *Den_MercenarySpeech(PyObject *self, PyObject *args) {
	static const char *const mods[] = { "Speech", "Chars", "CharsNPC", "Challenge", "Menu", "string", NULL };
	PyObject *usrName, *chName, *opt, *full;
	SF_ARGS("OOOO", &usrName, &chName, &opt, &full);
	for (int i = 0; mods[i]; i++) {
		Sf_Import(&f, mods[i]);
	}
	PyObject *ch = Sf_Entity(&f, chName);
	Sf_SpeechNames(&f, usrName, chName);
	int hire = Sf_EqS(&f, opt, "Generics_SpecialMercs_1");
	if (hire > 0) {
		int direct = 0;
		PyObject *ans = NULL;
		PyObject *usr = USR();
		if (Sf_True(&f, usr) > 0) {
			PyObject *lives = Sf_ToInt(&f, SAVE("Player.NumLives"));
			PyObject *friendVar = Sf_Cat(&f, "sO", "MercFriends.", chName);
			int friend = Sf_EqS(&f, f.failed ? NULL : Sf_Ask(&f, "Scrap", "GetSaveVar", "(O)", friendVar), "1");
			if (friend > 0) {
				ans = Sf_Str(&f, "Generics_SpecialMercs_2Ans");
				direct = 1;
			} else if (friend == 0 && Sf_Compare(&f, lives, "<", Sf_Int(&f, 3)) > 0) {
				ans = Sf_Str(&f, "Generics_SpecialMercs_3Ans");
			} else if (!f.failed) {
				ans = Sf_Str(&f, "Generics_SpecialMercs_1Ans");
				lives = Sf_OpI(&f, lives, '-', 3);
				Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Player.NumLives", Sf_Repr(&f, lives));
				Sf_Run(&f, "Scrap", "SetSaveVar", "(Os)", Sf_Cat(&f, "sO", "MercFriends.", chName), "1");
				Sf_SpeechEnd(&f, G("Map", "StartContract"), Sf_Build(&f, "(O)", ch));
			}
		}
		if (!f.failed && !ans) {
			Sf_Raise(&f, PyExc_NameError, "ans");
		}
		PyObject *msg = Sf_Cat(&f, "Os", opt, "Msg");
		if (direct) {
			Sf_Run(&f, "Speech", "OnSpch", "(OO)", msg, ans);
		} else {
			Sf_Run(&f, "CharsNPC", "PlayOption", "(OOOOO)", opt, msg, ans, usrName, ch);
		}
	} else if (hire == 0) {
		Sf_Run(&f, "CharsNPC", "GenericNPCSpeech", "(OOOO)", usrName, chName, opt, full);
	}
	if (Sf_EqS(&f, Sf_Slice(&f, Sf_Attr(&f, ch, "ArrOpt00"), 0, 25), "Generics_SpecialBishop_1:") > 0) {
		PyObject *price = Sf_Repr(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "BishopLivePrice"));
		Sf_SetAttr(&f, ch, "ArrOpt00", Sf_Ask(&f, "string", "replace", "(OsO)", Sf_Attr(&f, ch, "ArrOpt00"), "<n1>", price));
	}
	return Sf_None(&f);
}

/* StartContract(ch): the hired mercenary celebrates */
static PyObject *Den_StartContract(PyObject *self, PyObject *args) {
	PyObject *ch;
	SF_ARGS("O", &ch);
	Sf_Import(&f, "Speech");
	Sf_SetAttrS(&f, ch, "Action", "Drink");
	Sf_SetAttrS(&f, ch, "DefLoop", "Dance");
	Sf_Run(&f, "Speech", "RestoreState", "(O)", Sf_Attr(&f, ch, "Name"));
	return Sf_None(&f);
}

PyMethodDef g_GamblinDenMapMethods[] = {
	{ "InitStatus", Den_InitStatus, 1, NULL },
	{ "AfterBrowseNPC", Den_AfterBrowseNPC, 1, NULL },
	{ "CreateFixedInit", Level_FixedInit, 1, NULL },
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

static PyObject *Before_InitStatus(PyObject *self, PyObject *args) {
	static const char *const start[] = { "Spawn", "DM_Player_Spawn_Meeting_Room", "ComeFrom", "", "Char", "Dtritus", NULL };
	PyObject *path;
	SF_ARGS("O", &path);
	if (Sf_EqS(&f, SAVE("BeforeFinalFightPlayed"), "0") > 0) {
		Gd_Saves(&f, start);
	}
	Gd_Den(&f, "0", "0", "0", "0", "0");
	return Sf_None(&f);
}

/* Init(LevelPath): the scene of the plan the first time, the teleporter to the AB, the talks */
static PyObject *Before_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	int first = Sf_EqS(&f, SAVE("BeforeFinalFightPlayed"), "0");
	if (first > 0) {
		Sf_Own(&f, BF, "LoadScene", "()");
		Sf_Run(&f, "Scene", "Play", "(s)", "Plan2");
		Sf_SetMod(&f, "Scene", "EndOfSceneCallback", G(BF, "SetNextMission"));
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "BeforeFinalFightPlayed", "1");
		Sf_EntSetS(&f, "DM_Door_GamblinDenChupito_00", "OnObjSlot", "");
		Sf_Run(&f, "SSound", "SetMusicVolume", "(d)", 0.7);
		Sf_Print(&f, "s", "SSound.SetMusicVolume(0)");
	} else if (first == 0) {
		Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_FinalFight_MissionText1", 1);
		Sf_EntSetS(&f, "DM_Door_GamblinDenChupito_00", "OnObjSlot", "!AutoDoorSlot");
	}
	Sf_EntSetI(&f, "DM_Door_Teleporter_00", "Active", 1);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "DM_Door_Teleporter_00");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Teleporter_00", "AB");
	Sf_EntSetS(&f, "CrazyGambler_Default", "UseFunc", "Mission_BeforeFinalFight.CrazyGamblerSpeech");
	Sf_EntSetS(&f, "MercenaryA_Smartie", "UseFunc", "Mission_BeforeFinalFight.MercenariesSpeech");
	Sf_EntSetS(&f, "MercenaryB_Dumber", "UseFunc", "Mission_BeforeFinalFight.MercenariesSpeech");
	Sf_EntSetS(&f, "MercenaryC_Brutus", "UseFunc", "Mission_BeforeFinalFight.MercenariesSpeech");
	return Sf_None(&f);
}

static PyObject *Before_CrazyGamblerSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Talk(args, 1, "Mission_FinalFight_InitMsg1", "Mission_FinalFight_InitAns1");
}

static PyObject *Before_MercenariesSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Talk(args, 1, "Mission_FinalFight_InitMsg2", "Mission_FinalFight_InitAns2");
}

/* SetNextMission(usr=None): after the scene: the map of the mission scorer shows the AB */
static PyObject *Before_SetNextMission(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "usr", NULL };
	PyObject *usr = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) {
		return NULL;
	}
	Sf_Run(&f, "SScorer", "Fade", "(i(iiii)(iiii)d)", 0, 0, 0, 0, 255, 0, 0, 0, 0, 1.0);
	Sf_Run(&f, "SScorer", "Set", "(iOss)", 0, Sf_Ask(&f, "Scrap", "Get", "(s)", "MissionScorerName"), "ForceMap", "AB");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_FinalFight_MissionText1", 1);
	Sf_EntSetS(&f, "DM_Door_GamblinDenChupito_00", "OnObjSlot", "!AutoDoorSlot");
	Sf_Run(&f, "SSound", "SetMusicVolume", "(d)", 1.0);
	return Sf_None(&f);
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

static PyObject *Before_LoadScene(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Calls(&f, BF, plan2_scene);
	return Sf_None(&f);
}

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

static PyObject *After_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Gd_Den(&f, "0", "1", "1", "1", "1");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "PoliceBossAtPolice", "0");
	if (Sf_EqS(&f, SAVE("ComeFrom"), "DoorTeleporter") > 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Spawn", "DM_Player_Spawn_Meeting_Room");
	}
	return Sf_None(&f);
}

static PyObject *After_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_EntSetS(&f, "DM_Door_GamblinDenChupito_00", "OnObjSlot", "!AutoDoorSlot");
	Sf_EntSetI(&f, "DM_Door_Teleporter_00", "Active", 1);
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "DM_Door_Teleporter_00");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([s][s])", "DM_Door_Teleporter_00", "ABIndoor");
	return Sf_None(&f);
}

PyMethodDef g_GamblinDenAfterFinalFightMethods[] = {
	{ "InitStatus", After_InitStatus, 1, NULL },
	{ "Init", After_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ArrangeIgorCombat: the versus against the bishop Igor ---- */

static PyObject *Igor_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "PoliceBossAtPolice", "0");
	Gd_Den(&f, "1", "0", "1", "1", "0");
	return Sf_None(&f);
}

/* Igor gets his name, the crazy gambler is the target (both branches of the script do the same) */
static PyObject *Igor_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_SetGlobal(&f, AI, "igor", ENT("Bishop_Friendly35"));
	Sf_SetAttr(&f, G(AI, "igor"), "Descriptor", LANG("CharName_Igor"));
	if (Sf_True(&f, SAVE("DtritusBeatsIgor")) >= 0) {
		Sf_Own(&f, AI, "TalkToCrazyGambler", "()");
	}
	return Sf_None(&f);
}

static PyObject *Igor_TalkToIgor(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetAttrS(&f, G(AI, "igor"), "UseFunc", "Mission_ArrangeIgorCombat.IgorSpeech");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(AI, "igor"), "Name"));
	return Sf_None(&f);
}

static PyObject *Igor_IgorSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_SpeechNames(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		if (Sf_EqI(&f, Sf_Attr(&f, G(AI, "igor"), "RadarShow"), 1) > 0) {
			Sf_SpeechEnd(&f, G(AI, "TalkToCrazyGambler"), NULL);
		}
		Sf_NISpeech(&f, "Bishop", "Mission_TalkToIgor3_Igor_NI_1Msg");
	}
	return Sf_None(&f);
}

/* TalkToCrazyGambler(): the description, a way point, the crazy gambler's talk */
static PyObject *Igor_TalkToCrazyGambler(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_ArrangeIgorCombat_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	PyObject *cg = ENT("CrazyGambler_Default");
	Sf_SetAttrS(&f, cg, "UseFunc", "Mission_ArrangeIgorCombat.CrazyGamblerSpeech");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, cg, "Name"));
	return Sf_None(&f);
}

static PyObject *Igor_CrazyGamblerSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Arrange(args, AI, "Mission_ArrangeIgorCombat_CrazyGambler", "LaunchIgorCombat");
}

/* LaunchIgorCombat(): the places of the characters, the versus in the CA */
static PyObject *Igor_LaunchIgorCombat(PyObject *self, PyObject *args) {
	static const char *const places[] = { "MayorAtTownHall", "1", "PoliceBossAtPolice", "1", "BankMasterAtBank", "1",
		"Journalist.Humphrey_Defaut", "Temple", "Journalist.Betty_Defaut", "Temple", "Journalist.Berto_Defaut", "Press", NULL };
	SF_ARGS("");
	Sf_Import(&f, "SaveGame");
	Sf_Import(&f, "MissionsFuncs");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_VersusAgainstIgor_MissionText1", 1);
	Gd_Saves(&f, places);
	Sf_Run(&f, "MissionsFuncs", "StartSuperDeal", "(sss)", "CA", "Mission_IgorCombat", "Versus");
	return Sf_None(&f);
}

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
static byte Gd_Traitorius(scriptFrame_t *f, PyObject *e, const char *module) {
	Sf_SetAttrs(f, e, module, traitorius_vars);
	return Sf_SetAttr(f, e, "Descriptor", Sf_Lang(f, Sf_Str(f, "CharName_Traitorius")));
}

static PyObject *Ctf_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Gd_MissionChar(&f, "Functionary_Traitorius", G(BC, "CreateTraitoriusInit"));
	Gd_Den(&f, "1", "0", "0", "1", "1");
	return Sf_None(&f);
}

static PyObject *Ctf_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "Map");
	Sf_SetGlobal(&f, BC, "cg", ENT("CrazyGambler_Default"));
	Sf_SetGlobal(&f, BC, "traitorius", ENT("Functionary_Traitorius"));
	Sf_SetAttrS(&f, G(BC, "traitorius"), "UseFunc", "Mission_ArrangeBankersCTF.TraitoriusSpeech");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(BC, "traitorius"), "Name"));
	Sf_Run(&f, "Map", "CharDance", "(s)", "Functionary_Traitorius");
	return Sf_None(&f);
}

static PyObject *Ctf_CreateTraitoriusInit(PyObject *self, PyObject *args) {
	PyObject *e;
	SF_ARGS("O", &e);
	Gd_Traitorius(&f, e, BC);
	return Sf_None(&f);
}

/* TraitoriusSpeech(UsrName, CharName, optId, FullOption): he talks only to a functionary; the first time the crazy
 * gambler is next */
static PyObject *Ctf_TraitoriusSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_SpeechNames(&f, usr, ch);
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		int func = Sf_EqS(&f, Sf_Attr(&f, USR(), "ActCtrl"), "Functionary");
		if (func > 0) {
			if (Sf_EqI(&f, G(BC, "HasTalkedToTraitorius"), 0) > 0) {
				Sf_SetGlobal(&f, BC, "HasTalkedToTraitorius", Sf_Int(&f, 1));
				Sf_SpeechEnd(&f, G(BC, "TalkToCrazyGambler"), NULL);
			}
			Sf_Run(&f, "Speech", "OnSpch", "(ss)", "Mission_TalkToTraitorius3_Traitorius_InitMsg1", "Mission_TalkToTraitorius3_Traitorius_InitAns1");
		} else if (func == 0) {
			Sf_Run(&f, "Speech", "OnSpch", "(ss)", "Mission_TalkToTraitorius3_Traitorius_InitMsg2", "Mission_TalkToTraitorius3_Traitorius_InitAns2");
		}
	}
	return Sf_None(&f);
}

static PyObject *Ctf_TalkToCrazyGambler(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetAttrS(&f, G(BC, "cg"), "UseFunc", "Mission_ArrangeBankersCTF.CrazyGamblerSpeech");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(BC, "cg"), "Name"));
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_ArrangeTraitoriusCombat_MissionText1", 0);
	return Sf_None(&f);
}

static PyObject *Ctf_CrazyGamblerSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Arrange(args, BC, "Mission_ArrangeTraitoriusCombat_CrazyGambler", "LaunchBankersCTF");
}

static PyObject *Ctf_LaunchBankersCTF(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Launch(args, "Mission_CTFAgainstBankers_MissionText1", "OM", "Mission_BankersCTF", "CTF");
}

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

static PyObject *Bankers_InitStatus(PyObject *self, PyObject *args) {
	static const char *const start[] = { "ComeFrom", "", "Char", "Dtritus", "Spawn", "DM_Player_Spawn_Outskirts", NULL };
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "CharsNPC");
	Sf_Import(&f, "Map");
	if (Sf_EqS(&f, SAVE("Mission_ArrangeBankersCombat"), "Igor") > 0) {
		Gd_Saves(&f, start);
	}
	Gd_MissionChar(&f, "PoliceBoss_BankersCombat", Sf_Mod(&f, "Map", "CreateFixedInit"));
	Gd_Den(&f, "1", "1", "1", "0", "0");
	return Sf_None(&f);
}

/* Init(LevelPath): the characters of the mission, the step of the save state */
static PyObject *Bankers_Init(PyObject *self, PyObject *args) {
	static const char *const steps[][2] = { { "Igor", "LaunchIgorDefeatedSpeech" }, { "Deep", "LaunchDeepThroatMessage" },
		{ "Police", "TalkToPoliceBoss" }, { "Crazy", "TalkToCrazyGambler" } };
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_SetGlobal(&f, BK, "igor", ENT("Bishop_Friendly35"));
	Sf_SetAttr(&f, G(BK, "igor"), "Descriptor", LANG("CharName_Igor"));
	Sf_SetGlobal(&f, BK, "dd", ENT("DM_DTritusDesktop"));
	Sf_SetGlobal(&f, BK, "cg", ENT("CrazyGambler_Default"));
	Sf_SetGlobal(&f, BK, "policeBoss", ENT("PoliceBoss_BankersCombat"));
	PyObject *val = SAVE("Mission_ArrangeBankersCombat");
	for (int i = 0; i < 4 && !f.failed; i++) {
		if (Sf_EqS(&f, val, steps[i][0]) > 0) {
			Sf_Own(&f, BK, steps[i][1], "()");
			break;
		}
	}
	return Sf_None(&f);
}

/* LaunchIgorDefeatedSpeech(): beaten Igor in front of the player, his talk */
static PyObject *Bankers_LaunchIgorDefeatedSpeech(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_ArrangeBankersCombat", "Deep");
	PyObject *usr = USR();
	int go = Sf_True(&f, usr);
	if (go > 0) {
		go = Sf_True(&f, G(BK, "igor"));
	}
	if (go > 0) {
		Gd_Face(&f, G(BK, "igor"), usr);
		Sf_SpeechEnd(&f, G(BK, "LaunchDeepThroatMessage"), NULL);
		Sf_SpeechNames(&f, Sf_Attr(&f, usr, "Name"), Sf_Attr(&f, G(BK, "igor"), "Name"));
		Sf_NISpeech(&f, "Bishop", "Mission_VersusAgainstIgor_Igor_NI_1Msg");
	}
	return Sf_None(&f);
}

static PyObject *Bankers_LaunchDeepThroatMessage(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Float(&f, 3.0));
	PyObject *fn = Sf_Mod(&f, "Speech", "AddRemoteCom");
	Sf_Schedule(&f, t, fn, Sf_Build(&f, "(ssO)", "Mission_VersusAgainstIgor_RemoteMessage4", "DeepThroat", G(BK, "TalkToDeepThroat")), NULL);
	return Sf_None(&f);
}

/* TalkToDeepThroat(): Dtritus' desktop for the call */
static PyObject *Bankers_TalkToDeepThroat(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "DTritusDesktop");
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(BK, "dd"), 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_CallDeepThroat2_MissionText1", 1);
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", G(BK, "DeepThroatSpeech"));
	return Sf_None(&f);
}

static PyObject *Bankers_DeepThroatSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(BK, "dd"), 0);
	Sf_SpeechNames(&f, usr, ch);
	Sf_SpeechEnd(&f, G(BK, "TalkToPoliceBoss"), NULL);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_ArrangeBankersCombat", "Police");
	Sf_NISpeech(&f, "DeepThroat", "Mission_CallDeepThroat2_ComDeepThroat_NI_1Msg");
	return Sf_None(&f);
}

static PyObject *Bankers_TalkToPoliceBoss(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "DTritusDesktop");
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", Sf_Str(&f, ""));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(BK, "policeBoss"), "Name"));
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_SpyPoliceBoss_MissionText1", 0);
	Sf_SetAttrS(&f, G(BK, "policeBoss"), "UseFunc", "Mission_ArrangeBankersCombat.PoliceBossSpeech");
	return Sf_None(&f);
}

static PyObject *Bankers_PoliceBossSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_SpeechNames(&f, usr, ch);
	int talk = Sf_EqS(&f, opt, "Talk");
	if (talk > 0) {
		Sf_SpeechEnd(&f, G(BK, "TalkToCrazyGambler"), NULL);
		Sf_NISpeech(&f, "PoliceBoss", "Mission_SpyPoliceBoss_PoliceBoss_NI_1Msg");
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Mission_ArrangeBankersCombat", "Crazy");
	} else if (talk == 0) {
		Sf_Run(&f, "Speech", "StdOnSpch", "(O)", opt);
	}
	return Sf_None(&f);
}

static PyObject *Bankers_PoliceBossSpeech2(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Talk(args, 1, "Mission_ArrangeBankersCombat_PoliceBoss_InitMsg1", "Mission_ArrangeBankersCombat_PoliceBoss_InitAns1");
}

static PyObject *Bankers_TalkToCrazyGambler(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetAttrS(&f, G(BK, "policeBoss"), "UseFunc", "Mission_ArrangeBankersCombat.PoliceBossSpeech2");
	Sf_SetAttrS(&f, G(BK, "cg"), "UseFunc", "Mission_ArrangeBankersCombat.CrazyGamblerSpeech");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(BK, "cg"), "Name"));
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_ArrangeBankersCombat_MissionText1", 0);
	return Sf_None(&f);
}

static PyObject *Bankers_CrazyGamblerSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Arrange(args, BK, "Mission_ArrangeBankersCombat_CrazyGambler", "LaunchBankersCombat");
}

static PyObject *Bankers_LaunchBankersCombat(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "MissionsFuncs");
	Sf_Import(&f, "SaveGame");
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "Mission_ArrangeBankersCombat");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetBankerShipPlans_MissionText1", 1);
	Sf_Run(&f, "MissionsFuncs", "StartSuperDeal", "(sss)", "QF", "Mission_BankersCombat", "TeamDeathMatch");
	return Sf_None(&f);
}

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

static PyObject *First_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	return Sf_None(&f);
}

static PyObject *First_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	PyObject *state = SAVE("IgorFirstContactMissionState");
	if (Sf_EqS(&f, state, "TalkToCrazyGambler") > 0) {
		Sf_Own(&f, IF, "TalkToCrazyGambler", "()");
	} else if (!f.failed && Sf_EqS(&f, state, "TalkToIgor") > 0) {
		Sf_Own(&f, IF, "TalkToIgor", "()");
	} else if (!f.failed) {
		Sf_Own(&f, IF, "TalkToMercenaries", "()");
	}
	return Sf_None(&f);
}

static PyObject *First_TalkToCrazyGambler(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_SetGlobal(&f, IF, "cg", ENT("CrazyGambler_Default"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(IF, "cg"), "Name"));
	Sf_SetItem(&f, Sf_Mod(&f, "Speech", "SpeechRoutines"), Sf_Str(&f, "CrazyGambler"), G(IF, "CrazyGamblerSpeech"));
	return Sf_None(&f);
}

/* CrazyGamblerSpeech(UsrName, CharName, optId, FullOption): his own talk again, Igor is next */
static PyObject *First_CrazyGamblerSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "CharsNPC");
	Sf_Import(&f, "CrazyGambler");
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		Sf_SetItem(&f, Sf_Mod(&f, "Speech", "SpeechRoutines"), Sf_Str(&f, "CrazyGambler"), Sf_Mod(&f, "CrazyGambler", "CrazyGamblerSpeech"));
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "IgorFirstContactMissionState", "TalkToIgor");
		Sf_SpeechEnd(&f, G(IF, "TalkToIgor"), NULL);
		Sf_NISpeech(&f, "CrazyGambler", "Mission_GetReputation_CrazyGambler_NI_1Msg");
	}
	return Sf_None(&f);
}

static PyObject *First_TalkToIgor(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToIgor_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_SetGlobal(&f, IF, "igor", ENT("Bishop_Friendly35"));
	Sf_SetAttr(&f, G(IF, "igor"), "Descriptor", LANG("CharName_Igor"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(IF, "igor"), "Name"));
	Sf_SetAttrS(&f, G(IF, "igor"), "UseFunc", "Mission_IgorFirstContact.IgorSpeech");
	return Sf_None(&f);
}

/* IgorSpeech(UsrName, CharName, optId, FullOption): Igor talks to Dtritus only; his answer sends him to the
 * mercenaries */
static PyObject *First_IgorSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_SpeechNames(&f, usr, ch);
	int talk = Sf_EqS(&f, opt, "Talk");
	if (talk > 0) {
		int dtritus = Sf_EqS(&f, Sf_Attr(&f, USR(), "ActCtrl"), "Dtritus");
		if (dtritus > 0) {
			int talked = Sf_True(&f, G(IF, "alreadyTalked"));
			if (talked == 0) {
				Sf_Run(&f, "Speech", "OnSpch", "(ss)", "Mission_TalkToIgor_Igor_InitMsg1", "Mission_TalkToIgor_Igor_InitAns1");
				Sf_Run(&f, "Speech", "StdAnsLst", "([s])", "Mission_TalkToIgor_Igor_1");
			} else if (talked > 0) {
				Sf_Run(&f, "Speech", "OnSpch", "(ss)", "Mission_GetMercShipPlans_Igor_1Msg", "Mission_GetMercShipPlans_Igor_1Ans");
			}
		} else if (dtritus == 0) {
			Sf_Run(&f, "Speech", "OnSpch", "(ss)", "Mission_TalkToIgor_Igor_InitMsg2", "Mission_TalkToIgor_Igor_InitAns2");
		}
	} else if (talk == 0) {
		Sf_Run(&f, "Speech", "StdOnSpch", "(O)", opt);
		if (Sf_EqS(&f, SAVE("IgorFirstContactMissionState"), "TalkToIgor") > 0) {
			Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "IgorFirstContactMissionState", "TalkToMercenaries");
			Sf_SpeechEnd(&f, G(IF, "TalkToMercenaries"), NULL);
		}
	}
	return Sf_None(&f);
}

/* TalkToMercenaries(): the three mercenaries as targets with their talks */
static PyObject *First_TalkToMercenaries(PyObject *self, PyObject *args) {
	static const char *const mercs[][3] = { { "mercA", "MercenaryA_Smartie", "Mission_IgorFirstContact.MercenaryASpeech" },
		{ "mercB", "MercenaryB_Dumber", "Mission_IgorFirstContact.MercenaryBSpeech" },
		{ "mercC", "MercenaryC_Brutus", "Mission_IgorFirstContact.MercenaryCSpeech" } };
	SF_ARGS("");
	Sf_SetGlobal(&f, IF, "alreadyTalked", Sf_Int(&f, 1));
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetMercShipPlans_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "MissionsFuncs", "SetMultipleMissionTargets", "([sss])", "MercenaryA_Smartie", "MercenaryB_Dumber", "MercenaryC_Brutus");
	for (int i = 0; i < 3; i++) {
		Sf_SetGlobal(&f, IF, mercs[i][0], ENT(mercs[i][1]));
	}
	for (int i = 0; i < 3; i++) {
		Sf_SetAttrS(&f, G(IF, mercs[i][0]), "UseFunc", mercs[i][2]);
	}
	return Sf_None(&f);
}

static PyObject *First_MercenaryASpeech(PyObject *self, PyObject *args) {
	static const gdMerc_t m = { "Mission_IgorFirstContact", "Mission_GetMercShipPlans_SmartMerc", "Mission_GetMercShipPlans_SmartMerc_InitMsg1",
		"Mission_GetMercShipPlans_SmartMerc_InitAns1", NULL, NULL, 0 };
	(void)self;
	return Gd_Merc(args, IF, &m);
}

static PyObject *First_MercenaryCSpeech(PyObject *self, PyObject *args) {
	static const gdMerc_t m = { "Mission_IgorFirstContact", "Mission_GetMercShipPlans_AverageMerc",
		"Mission_GetMercShipPlans_AverageMerc_InitMsg1", "Mission_GetMercShipPlans_AverageMerc_InitAns1", NULL, NULL, 0 };
	(void)self;
	return Gd_Merc(args, IF, &m);
}

static PyObject *First_MercenaryBSpeech(PyObject *self, PyObject *args) {
	static const gdMerc_t m = { "Mission_IgorFirstContact", "Mission_GetMercShipPlans_StupidMerc",
		"Mission_FirstContactMercs_StupidMerc_InitMsg2", "Mission_FirstContactMercs_StupidMerc_InitAns2", "SetNextMission", NULL, 0 };
	(void)self;
	return Gd_Merc(args, IF, &m);
}

/* SetNextMission(): the mercenaries no longer targets, on to Mission_StupidMercAgain in the scrapyard */
static PyObject *First_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(IF, "mercA"), 0);
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(IF, "mercB"), 0);
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(IF, "mercC"), 0);
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_StupidMercAgain", "Scrapyard");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_GetMercShipPlans_MissionText2", 1);
	return Sf_None(&f);
}

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

static PyObject *Go_InitStatus(PyObject *self, PyObject *args) {
	static const char *const start[] = { "Spawn", "DM_Player_Spawn_Meeting_Room", "ComeFrom", "", "Char", "Dtritus", NULL };
	PyObject *path;
	SF_ARGS("O", &path);
	Gd_Saves(&f, start);
	Sf_Run(&f, "Chars", "AddNewCharRes", "(s)", "Human");
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "AfterBrowseNPC"), "append", "(O)", G(GG, "AfterBrowseNPC"));
	return Sf_None(&f);
}

static PyObject *Go_AfterBrowseNPC(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "CharsNPC");
	Gd_Callback(&f, "MercenaryA_Smartie", G(GG, "CreateMercenaryInit"));
	Gd_Callback(&f, "MercenaryB_Dumber", G(GG, "CreateMercenaryInit"));
	Gd_Callback(&f, "MercenaryC_Brutus", G(GG, "CreateMercenaryInit"));
	Sf_Run(&f, "CharsNPC", "CreateMissionChar", "(s)", "Human_Default");
	return Sf_None(&f);
}

static PyObject *Go_CreateMercenaryInit(PyObject *self, PyObject *args) {
	PyObject *e;
	SF_ARGS("O", &e);
	Sf_SetAttrS(&f, e, "UseFunc", "Mission_GoToGamblinDen.MercenarySpeech");
	return Sf_None(&f);
}

static PyObject *Go_MercenarySpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Talk(args, 1, "Mission_DestroyCommSats1_Merc_InitMsg1", "Mission_DestroyCommSats1_Merc_InitAns1");
}

static PyObject *Go_HumanSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Talk(args, 1, "Mission_DestroyCommSats1_Human_InitMsg1", "Mission_DestroyCommSats1_Human_InitAns1");
}

static PyObject *Go_CrazyGamblerSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Talk(args, 1, "Mission_DestroyCommSats1_CrazyGambler_InitMsg1", "Mission_DestroyCommSats1_CrazyGambler_InitAns1");
}

static PyObject *Go_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Own(&f, GG, "LoadScene", "()");
	Sf_EntSetS(&f, "CrazyGambler_Default", "UseFunc", "Mission_GoToGamblinDen.CrazyGamblerSpeech");
	Sf_EntSetS(&f, "Human_Default", "UseFunc", "Mission_GoToGamblinDen.HumanSpeech");
	Sf_Run(&f, "Scene", "Play", "(s)", "Plan");
	Sf_SetMod(&f, "Scene", "EndOfSceneCallback", G(GG, "SetNextMission"));
	Sf_Run(&f, "SSound", "SetMusicVolume", "(d)", 0.5);
	return Sf_None(&f);
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

static PyObject *Go_LoadScene(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Calls(&f, GG, plan_scene);
	return Sf_None(&f);
}

/* SetNextMission(usr=None): the mercenaries help Dtritus, on to Mission_DestroyCommSats1 */
static PyObject *Go_SetNextMission(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "usr", NULL };
	PyObject *usr = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) {
		return NULL;
	}
	Sf_Import(&f, "MissionsFuncs");
	Sf_Import(&f, "SaveGame");
	Sf_Run(&f, "SScorer", "Fade", "(i(iiii)(iiii)d)", 0, 0, 0, 0, 255, 0, 0, 0, 0, 1.0);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "MercsHelpDtritus", "1");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_DestroyCommSats1", "Outskirts");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_DestroyCommSats1_MissionText1", 1);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "SSound", "SetMusicVolume", "(d)", 1.0);
	return Sf_None(&f);
}

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

static PyObject *Traitor_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Player.InfiniteLives", "1");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Char", "Functionary");
	Gd_MissionChar(&f, "Functionary_Traitorius", G(TT, "CreateTraitoriusInit"));
	return Sf_None(&f);
}

static PyObject *Traitor_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_EntSetS(&f, "DM_Door_Elevator_Scrapyard", "OnObjSlot", "!CloseDoorSlot");
	Sf_Own(&f, TT, "LaunchTraitoriusSpeech", "()");
	return Sf_None(&f);
}

static PyObject *Traitor_CreateTraitoriusInit(PyObject *self, PyObject *args) {
	PyObject *e;
	SF_ARGS("O", &e);
	Gd_Traitorius(&f, e, TT);
	Sf_SetGlobal(&f, TT, "traitorius", e);
	return Sf_None(&f);
}

/* LaunchTraitoriusSpeech(): the player in front of Traitorius, his talk */
static PyObject *Traitor_LaunchTraitoriusSpeech(PyObject *self, PyObject *args) {
	SF_ARGS("");
	PyObject *usr = USR();
	int go = Sf_True(&f, usr);
	if (go > 0) {
		go = Sf_True(&f, G(TT, "traitorius"));
	}
	if (go > 0) {
		Gd_Face(&f, usr, G(TT, "traitorius"));
		Sf_SpeechEnd(&f, G(TT, "SetNextMission"), NULL);
		Sf_SpeechNames(&f, Sf_Attr(&f, usr, "Name"), Sf_Attr(&f, G(TT, "traitorius"), "Name"));
		Sf_NISpeech(&f, "Functionary", "Mission_CTFAgainstBankers_Traitorius_NI_1Msg");
	}
	return Sf_None(&f);
}

static PyObject *Traitor_TraitoriusSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Talk(args, 1, "Mission_CTFAgainstBankers_Traitorius_InitMsg1", "Mission_CTFAgainstBankers_Traitorius_InitAns1");
}

/* SetNextMission(): on to Mission_IdentifyMerc in the outskirts, the lonely mercenary waits there */
static PyObject *Traitor_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Init");
	Sf_SetAttrS(&f, G(TT, "traitorius"), "UseFunc", "Mission_TalkToTraitoriusAfterCTF.TraitoriusSpeech");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "GamblinDen");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_IdentifyMerc", "Outskirts");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_MeetingWithMrX_MissionText1", 1);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "LonelyMercDataActive", "1");
	return Sf_None(&f);
}

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

static PyObject *Back_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Gd_Den(&f, NULL, "1", "0", "0", "1");
	return Sf_None(&f);
}

/* Init(LevelPath): the crazy gambler gives the plans of the inferno, DeepThroat after the message */
static PyObject *Back_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Import(&f, "RaceMaker");
	PyObject *l = Sf_Eval(&f, SAVE("Hangar.availableWeaponsList"), BM);
	if (Sf_In(&f, Sf_Str(&f, "Inferno"), l) == 0) {
		Sf_Run(&f, "RaceMaker", "ShowSystemMessage", "(O)", LANG("Mission_WinMortalRace_SystemMessage3"));
		Sf_Method(&f, l, "append", "(s)", "Inferno");
		Sf_Run(&f, "Scrap", "SetSaveVar", "(sO)", "Hangar.availableWeaponsList", Sf_Repr(&f, l));
		Sf_Run(&f, "Scrap", "IncSaveVar", "(s)", "NewBluePrintAvaliable");
	}
	Sf_Later(&f, Sf_Float(&f, 0.1), G(BM, "TalkToDeepThroat"), NOARGS, NULL);
	return Sf_None(&f);
}

static PyObject *Back_TalkToDeepThroat(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Run(&f, "Speech", "AddRemoteCom", "(ssO)", "Mission_WinMortalRace_RemoteMessage4", "DeepThroat", G(BM, "AfterDeepThroatMessageShown"));
	return Sf_None(&f);
}

static PyObject *Back_AfterDeepThroatMessageShown(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "DTritusDesktop");
	Sf_SetGlobal(&f, BM, "dd", ENT("DM_DTritusDesktop"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(BM, "dd"), "Name"));
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_TalkToDeepThroat10_MissionText1", 1);
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", G(BM, "DeepThroatSpeech"));
	return Sf_None(&f);
}

static PyObject *Back_DeepThroatSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_SpeechNames(&f, usr, ch);
	Sf_SpeechEnd(&f, G(BM, "SetNextMission"), NULL);
	Sf_NISpeech(&f, "DeepThroat", "Mission_TalkToDeepThroat10_ComDeepThroat_NI_1Msg");
	return Sf_None(&f);
}

static PyObject *Back_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "DTritusDesktop");
	Sf_SetMod(&f, "DTritusDesktop", "DialogController", Py_None);
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(BM, "dd"), 0);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_DestroyBadDebtors", "Outskirts");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_MeetBankMaster_MissionText1", 1);
	return Sf_None(&f);
}

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

static PyObject *Meet_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "ComeFrom", "");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "Spawn", "DM_Player_Spawn_Meeting_Room");
	return Sf_None(&f);
}

/* Init(LevelPath): the elevator unused, the crazy gambler the target, the scene the first time */
static PyObject *Meet_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_EntSetI(&f, "DM_Door_Elevator_Outskirts", "ArrowCanUse", 0);
	Sf_SetGlobal(&f, CF, "cg", ENT("CrazyGambler_Default"));
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(O)", Sf_Attr(&f, G(CF, "cg"), "Name"));
	Sf_SetItem(&f, Sf_Mod(&f, "Speech", "SpeechRoutines"), Sf_Str(&f, "CrazyGambler"), G(CF, "CrazyGamblerFirstSpeech"));
	int scene = Sf_EqS(&f, SAVE("LaunchScene"), "1");
	if (scene > 0) {
		Sf_Own(&f, CF, "LoadScene", "()");
		Sf_Run(&f, "Scene", "Play", "(s)", "MeetGambler");
		Sf_SetMod(&f, "Scene", "EndOfSceneCallback", G(CF, "OnEndOfScene"));
	} else if (scene == 0) {
		Sf_Run(&f, "SSound", "SetMusic", "(sd)", "Music/GDloop1.ogg", 1.0);
	}
	return Sf_None(&f);
}

/* in the demo: how to use, for secs */
static void Cf_DemoHint(scriptFrame_t *f, const char *key, long secs) {
	if (Sf_True(f, Sf_Mod(f, "Init", "isDemo")) > 0) {
		int mark = f->n;
		PyObject *fn = Sf_Mod(f, "SScorer", "SetMsgText");
		PyObject *t = Sf_Lang(f, Sf_Str(f, key));
		t = Sf_Op(f, t, '%', Sf_Ask(f, "SInput", "GetDefinedEntry", "(isss)", 0, "Walk", "Use", "Mouse"));
		Sf_Call(f, fn, "(OO)", t, Sf_OpI(f, Sf_Time(f), '+', secs));
		Sf_Release(f, mark);
	}
}

static PyObject *Meet_OnEndOfScene(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "usr", NULL };
	PyObject *usr = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &usr)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "LaunchScene", "0");
	Sf_Run(&f, "SSound", "SetMusic", "(sd)", "Music/GDloop1.ogg", 1.0);
	Cf_DemoHint(&f, "Mission_Decontamination_SystemMessage9", 10);
	return Sf_None(&f);
}

static PyObject *Meet_CrazyGamblerFirstSpeech(PyObject *self, PyObject *args) {
	PyObject *usr, *ch, *opt, *full;
	SF_ARGS("OOOO", &usr, &ch, &opt, &full);
	Sf_Import(&f, "Init");
	if (Sf_EqS(&f, opt, "Talk") > 0) {
		Sf_SpeechEnd(&f, G(CF, "EndOfTalk"), NULL);
		Sf_NISpeech(&f, NULL, "Mission_FirstMeetCrazyGambler_CrazyGambler_NI_1Msg");
		Cf_DemoHint(&f, "Mission_Decontamination_SystemMessage8", 7);
	}
	return Sf_None(&f);
}

static PyObject *Meet_CrazyGamblerSecondSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Talk(args, 0, "Mission_DestroyPolice_CrazyGambler_InitMsg1", "Mission_DestroyPolice_CrazyGambler_InitAns1");
}

/* EndOfTalk(char=None): his second talk from now on, the elevator back, the next mission */
static PyObject *Meet_EndOfTalk(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "char", NULL };
	PyObject *ch = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &ch)) {
		return NULL;
	}
	Sf_Import(&f, "CrazyGambler");
	Sf_SetItem(&f, Sf_Mod(&f, "Speech", "SpeechRoutines"), Sf_Str(&f, "CrazyGambler"), G(CF, "CrazyGamblerSecondSpeech"));
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(CF, "cg"), 0);
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_EntSetI(&f, "DM_Door_Elevator_Outskirts", "ArrowCanUse", 1);
	Sf_Own(&f, CF, "SetNextMission", "()");
	return Sf_None(&f);
}

static PyObject *Meet_SetNextMission(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "Init");
	if (Sf_True(&f, Sf_Mod(&f, "Init", "isDemo")) == 0) {
		Sf_Run(&f, "Scrap", "SetSaveVar", "(ss)", "PoliceBossAtPolice", "1");
	}
	Sf_Run(&f, "Scrap", "DelSaveVars", "(s)", "LaunchScene");
	Sf_Run(&f, "MissionsFuncs", "SetMissionTarget", "(s)", "");
	Sf_Run(&f, "SaveGame", "QWayPoint", "()");
	Sf_Run(&f, "MissionsFuncs", "SetNextMission", "(ss)", "Mission_KillPolice", "Outskirts");
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_DestroyPolice_MissionText1", 1);
	return Sf_None(&f);
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

static PyObject *Meet_LoadScene(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Calls(&f, CF, meet_scene);
	return Sf_None(&f);
}

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

static PyObject *Mortal_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Gd_Den(&f, NULL, "0", "0", "1", "1");
	return Sf_None(&f);
}

static PyObject *Mortal_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_SetGlobal(&f, AM, "cg", ENT("CrazyGambler_Default"));
	Sf_SetAttrS(&f, G(AM, "cg"), "UseFunc", "Mission_ArrangeMortalRace.CrazyGamblerSpeech");
	Sf_Run(&f, "MissionsFuncs", "ActivateTarget", "(Oi)", G(AM, "cg"), 1);
	return Sf_None(&f);
}

static PyObject *Mortal_CrazyGamblerSpeech(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Arrange(args, AM, "Mission_ArrangeMortalRace_CrazyGambler", "LaunchMortalRace");
}

static PyObject *Mortal_LaunchMortalRace(PyObject *self, PyObject *args) {
	(void)self;
	return Gd_Launch(args, "Mission_WinMortalRace_MissionText1", "FZ", "Mission_MortalRace", "MortalRace");
}

PyMethodDef g_GamblinDenArrangeMortalRaceMethods[] = {
	{ "InitStatus", Mortal_InitStatus, 1, NULL },
	{ "Init", Mortal_Init, 1, NULL },
	{ "CrazyGamblerSpeech", Mortal_CrazyGamblerSpeech, 1, NULL },
	{ "LaunchMortalRace", Mortal_LaunchMortalRace, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Mission_ThreeStooges: the first contact with the three mercenaries ---- */

static PyObject *Stooges_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "AfterBrowseNPC"), "append", "(O)", G(TS, "AfterBrowseNPC"));
	return Sf_None(&f);
}

static PyObject *Stooges_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	SF_ARGS("O", &path);
	Sf_Run(&f, "MissionsFuncs", "SetMultipleMissionTargets", "([sss])", "MercenaryA_Smartie", "MercenaryB_Dumber", "MercenaryC_Brutus");
	return Sf_None(&f);
}

static PyObject *Stooges_AfterBrowseNPC(PyObject *self, PyObject *args) {
	SF_ARGS("");
	Sf_Import(&f, "CharsNPC");
	Gd_Callback(&f, "MercenaryA_Smartie", G(TS, "CreateMercenaryAInit"));
	Gd_Callback(&f, "MercenaryB_Dumber", G(TS, "CreateMercenaryBInit"));
	Gd_Callback(&f, "MercenaryC_Brutus", G(TS, "CreateMercenaryCInit"));
	return Sf_None(&f);
}

static PyObject *Stooges_UseFunc(PyObject *args, const char *use) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	Sf_SetAttrS(&f, e, "UseFunc", use);
	return Sf_None(&f);
}

static PyObject *Stooges_CreateMercenaryAInit(PyObject *self, PyObject *args) {
	(void)self;
	return Stooges_UseFunc(args, "Mission_ThreeStooges.MercenaryASpeech");
}

static PyObject *Stooges_CreateMercenaryBInit(PyObject *self, PyObject *args) {
	(void)self;
	return Stooges_UseFunc(args, "Mission_ThreeStooges.MercenaryBSpeech");
}

static PyObject *Stooges_CreateMercenaryCInit(PyObject *self, PyObject *args) {
	(void)self;
	return Stooges_UseFunc(args, "Mission_ThreeStooges.MercenaryCSpeech");
}

static PyObject *Stooges_MercenaryASpeech(PyObject *self, PyObject *args) {
	static const gdMerc_t m = { "Mission_ThreeStooges", "Mission_FirstContactMercs_SmartMerc", "Mission_FirstContactMercs_SmartMerc_InitMsg1",
		"Mission_FirstContactMercs_SmartMerc_InitAns1", NULL, NULL, 0 };
	(void)self;
	return Gd_Merc(args, TS, &m);
}

static PyObject *Stooges_MercenaryCSpeech(PyObject *self, PyObject *args) {
	static const gdMerc_t m = { "Mission_ThreeStooges", "Mission_FirstContactMercs_AverageMerc",
		"Mission_FirstContactMercs_AverageMerc_InitMsg1", "Mission_FirstContactMercs_AverageMerc_InitAns1", NULL, NULL, 0 };
	(void)self;
	return Gd_Merc(args, TS, &m);
}

static PyObject *Stooges_MercenaryBSpeech(PyObject *self, PyObject *args) {
	static const gdMerc_t m = { "Mission_ThreeStooges", "Mission_FirstContactMercs_StupidMerc", "Mission_FirstContactMercs_StupidMerc_InitMsg2",
		"Mission_FirstContactMercs_StupidMerc_InitAns2", "SetNextMission", "alreadyConvinced", 1 };
	(void)self;
	return Gd_Merc(args, TS, &m);
}

/* SetNextMission(CharName): convinced; the mercenaries no targets, the next mission of NextMissionData */
static PyObject *Stooges_SetNextMission(PyObject *self, PyObject *args) {
	PyObject *ch;
	SF_ARGS("O", &ch);
	Sf_Import(&f, "Speech");
	Sf_SetGlobal(&f, TS, "alreadyConvinced", Sf_Int(&f, 1));
	Sf_Run(&f, "Speech", "RestoreState", "(O)", ch);
	Sf_Run(&f, "MissionsFuncs", "DelMissionTarget", "(s)", "MercenaryA_Smartie");
	Sf_Run(&f, "MissionsFuncs", "DelMissionTarget", "(s)", "MercenaryB_Dumber");
	Sf_Run(&f, "MissionsFuncs", "DelMissionTarget", "(s)", "MercenaryC_Brutus");
	PyObject *fn = Sf_Mod(&f, "MissionsFuncs", "SetNextMission");
	Sf_Run(&f, "__builtin__", "apply", "(OO)", fn, G(TS, "NextMissionData"));
	Sf_Run(&f, "SaveGame", "MMDesc", "(si)", "Mission_FollowStupidMerc_MissionText1", 1);
	return Sf_None(&f);
}

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
