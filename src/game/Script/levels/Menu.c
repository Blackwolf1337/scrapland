/*
 * The Map module of the level Menu in C (levels/menu/scripts/map.py, issue #2): the main menu's scene (the demo camera
 * flying through the asteroids), the credits and the way back to the menu. StartMenu is data
 * (data/levels/menu/map.def). Libraries, InitMenu, AstList, initpos, CreditsCtr and the counters of the asteroids
 * (xctr, rrr, mmr, globctr) stay variables of the module.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions work on native values (Levels.h). The variables of Map are
 * read where the script reads them and written through at once, CreditsText.EndOfCredits gets the function object of
 * Map (OBJLIB 5). The cameras are handle values (OBJLIB 7). The engine variables, the dummy search, the camera, the
 * action set, the vector math and the random numbers are C; Scrap.Set, Scrap.CreateEntity, the SAct class and action
 * functions, SSound.SetMusic, Scrap.AddScheduledFunc and Scrap.SetDebrisSys, Scrap.SetDebrisValue, Scrap.DropDebris and
 * Scrap.MusicPlayer (fc-lvl-menu swaps the last three) go through their seams (OBJLIB 7). The loading of CreditsText
 * (Scrap.OpenPack, PreloadLibrary, ClosePack) stays by name (OBJLIB 6); CreditsText, Menu, PCMenu, SaveGame and the
 * functions of Map go by name (OBJLIB 3).
 */
#include <string.h>

#include "scrap/input.h"
#include "scrap/mathlib.h"
#include "scrap/scrap.h"
#include "scrap/world.h"
#include "Levels.h"

#define MAP "Map"

/* ---- the engine calls of the scene, through their seams (OBJLIB 7: fc-lvl-menu swaps the debris and the music
 * player, _BSCEN of fr6b.py records SetDebrisSys) ---- */

/* Scrap.SetDebrisValue(n, x, y, z): 0 position, 1 angles (degrees), 2 velocity, 3 angle speed (degrees) */
static byte Mn_DebrisValue(long n, const nativeVal_t *x, const nativeVal_t *y, const nativeVal_t *z) {
	PyObject *hook;
	int seam = Native_Seam("Scrap", "SetDebrisValue", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(hook, NULL, "ivvv", n, x, y, z);
	}
	double d[3];
	if (!Native_ArgDouble(x, &d[0]) || !Native_ArgDouble(y, &d[1]) || !Native_ArgDouble(z, &d[2])) {
		return 0;
	}
	if (n == 1 || n == 3) {
		for (int i = 0; i < 3; i++) {
			d[i] = d[i] * M_PI_D / DEG_PER_PI;
		}
	}
	g_DebrisDrop[n] = (vec3_t){ (float)d[0], (float)d[1], (float)d[2] };
	return 1;
}

/* the same with three numbers */
static byte Mn_DebrisValueD(long n, double x, double y, double z) {
	nativeVal_t v[3];
	Native_SetFloat(&v[0], x);
	Native_SetFloat(&v[1], y);
	Native_SetFloat(&v[2], z);
	return Mn_DebrisValue(n, &v[0], &v[1], &v[2]);
}

/* Scrap.DropDebris(name, speed): a debris element with the values of SetDebrisValue */
static byte Mn_DropDebris(const nativeVal_t *name, const nativeVal_t *speed) {
	PyObject *hook;
	int seam = Native_Seam("Scrap", "DropDebris", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(hook, NULL, "vv", name, speed);
	}
	const char *n;
	double s;
	if (!Native_ArgStr(name, 1, &n) || !Native_ArgDouble(speed, &s)) {
		return 0;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, n);
	cDebris *deb = cDebrisSys_NewElement(sys);
	for (int i = 0; i < 4; i++) {
		deb->drop[i] = g_DebrisDrop[i];
	}
	deb->life = (float)s;
	cDebrisSys_Drop(sys, deb);
	return 1;
}

/* Scrap.MusicPlayer(name, cmd[, volume]) (volume < 0: not given) */
static byte Mn_MusicPlayer(const char *name, const char *cmd, double volume) {
	PyObject *hook;
	int seam = Native_Seam("Scrap", "MusicPlayer", &hook);
	if (seam) {
		return seam > 0 && (volume < 0 ? Native_HookCall(hook, NULL, "ss", name, cmd) :
			Native_HookCall(hook, NULL, "ssd", name, cmd, volume));
	}
	Music_Play(name, cmd, volume < 0 ? 0.0f : (float)volume);
	return 1;
}

/* Scrap.SetDebrisSys(name, 30, 1000000, 0, 0.02, 0, 0): the debris system of an asteroid model */
static byte Mn_DebrisSys(const nativeVal_t *name) {
	PyObject *hook;
	int seam = Native_Seam("Scrap", "SetDebrisSys", &hook);
	if (seam) {
		return seam > 0 && Native_HookCall(hook, NULL, "viiidii", name, 30L, 1000000L, 0L, 0.02, 0L, 0L);
	}
	const char *n;
	if (!Native_ArgStr(name, 1, &n)) {
		return 0;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, n);
	sys->b = 0.0f;
	sys->d = 0.0f;
	float rf = 1000000.0f;
	sys->radiusSq = rf * rf;
	sys->c = 0.02f;
	sys->flag = 0;
	cDebrisSys_Init(sys, 30, 0);
	return 1;
}

/* ---- shared lines ---- */

/* Scrap.Set(name, i), Scrap.Set(name, d), Scrap.Set(name, "text") */
static byte Mn_SetI(const char *name, long i) {
	nativeVal_t v;
	Native_SetInt(&v, i);
	return Mode_GVarSet(name, &v);
}

/* `if Scrap.Get(name):` */
static int Mn_GetTrue(const char *name) {
	nativeVal_t v;
	if (!Native_GVarGet(name, &v)) {
		return -1;
	}
	int r = Native_True(&v);
	Native_Free(&v);
	return r;
}

/* module.var = v as Script_SetVar sets it (an error is dropped, as the port did) */
static void Mn_SetVar(const char *module, const char *var, const nativeVal_t *v) {
	if (!Mode_SetModVar(module, var, v)) {
		/* OBJLIB 7 */
		PyErr_Clear();
	}
}

/* the functions of the module through its globals with no arguments */
static byte Mn_Own(const char *fn) {
	return LV_OWN(MAP, fn, NULL, "");
}

/* ---- the functions ---- */

/* InitStatus(LevelPath): in the main menu; the credits library when the credits are to be shown */
static byte Menu_InitStatus(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t one;
	(void)a;
	(void)out;
	if (!Mode_Import("Init")) {
		return 0;
	}
	Native_SetInt(&one, 1);
	Mn_SetVar("Init", "inMainMenu", &one);
	int s = Mn_GetTrue("ShowCredits");
	/* OBJLIB 6: the loading of a module stays with the module objects */
	return s == 0 || (s > 0 && LV_CALL("Scrap", "OpenPack", NULL, "s", "Scripts/Scorer") &&
		LV_CALL("Scrap", "PreloadLibrary", NULL, "ss", "CreditsText", "CreditsText.pyc") && LV_CALL("Scrap", "ClosePack", NULL, "") &&
		LvN_PutI(MAP, "InitMenu", 0));
}

/* EndCredits(id=0): the credits end: back to the menu with its music (the Xbox restarts) */
static byte Menu_EndCredits(const nativeVal_t *id) {
	if (!Mode_Import("SaveGame") || !Mode_Import("SSound") || !Mode_GVarSetS("EscapeEvent", "")) {
		return 0;
	}
	int x = Mn_GetTrue("IsXbox");
	if (x < 0) {
		return 0;
	}
	if (x) {
		return LV_CALL("SaveGame", "Restart", NULL, "");
	}
	return LV_OWN(MAP, "StartMenu", NULL, "v", id) && Mn_MusicPlayer("", "stop", -1) && LvN_SetMusic("Music/Epic.ogg", 0.8);
}

/* FadeOutCreditsEnd(): the end of the credits unless they were aborted before */
static byte Menu_FadeOutCreditsEnd(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t f, zero;
	(void)a;
	(void)out;
	if (!Native_GVarGet("MotionBlurFactorEnd", &f)) {
		return 0;
	}
	Native_SetFloat(&zero, 0.0);
	int c = Native_Compare(&f, &zero);
	Native_Free(&f);
	return c <= 0 || Mn_Own("EndCredits");
}

/* FadeOutCredits(): the music fades in 4 s, then FadeOutCreditsEnd */
static byte Menu_FadeOutCredits(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	return Mode_Import("SSound") && Mn_MusicPlayer("", "fadeout", 1 / 4.0) &&
		Mode_LaterIn(4.0 + 0.1, MAP, "FadeOutCreditsEnd", NULL, 0, NULL);
}

/* NextCredit(): the next credit page with text (a "-" page is skipped), the fade out after the last */
static byte Menu_NextCredit(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	(void)a;
	(void)out;
	if (!Mode_Import("CreditsText") || !Mode_Import("Menu")) {
		return Mode_Done(&t, 0);
	}
	for (;;) {
		int mark = t.n;
		nativeVal_t *n = Mode_V(&t), *rep = Mode_V(&t), *key = Mode_V(&t), *text = Mode_V(&t);
		if (!LvN_GAdd(MAP, "CreditsCtr", 1, n)) {
			return Mode_Done(&t, 0);
		}
		Mode_Repr(n, rep);
		if (!Mode_Cat(&t, key, 3, Mode_S(&t, "Credits_"), rep, Mode_S(&t, "_1"))) {
			return Mode_Done(&t, 0);
		}
		Mode_Lang(Native_Text(key), text);
		byte end = Native_IsStr(text, ""), skip = Native_IsStr(text, "-");
		Mode_Release(&t, mark);
		if (end) {
			return Mode_Done(&t, Mn_Own("FadeOutCredits"));
		}
		if (!skip) {
			break;
		}
	}
	nativeVal_t *ctr = Mode_V(&t);
	return Mode_Done(&t, Mode_G(MAP, "CreditsCtr", ctr) && LV_CALL("CreditsText", "ShowCredit", NULL, "v", ctr));
}

/* ShowCredits(): the credits from the first page, Escape ends them, the motion blur a moment later */
static byte Menu_ShowCredits(const nativeVal_t *a, nativeVal_t *out) {
	nativeVal_t next;
	(void)a;
	(void)out;
	if (!Mode_Import("CreditsText") || !LvN_PutI(MAP, "CreditsCtr", 0) || !LvN_GObj(MAP, "NextCredit", &next)) {
		return 0;
	}
	/* OBJLIB 5: the function object of Map */
	Mn_SetVar("CreditsText", "EndOfCredits", &next);
	Native_Free(&next);
	return Mn_Own("NextCredit") && Mode_GVarSetS("EscapeEvent", "Map.EndCredits") &&
		Mode_LaterIn(0.15, MAP, "SetMotionBlur", NULL, 0, NULL);
}

/* SetMotionBlur(): the blur of the credits while the input is inactive */
static byte Menu_SetMotionBlur(const nativeVal_t *a, nativeVal_t *out) {
	(void)a;
	(void)out;
	if (strcmp(TABLE_AT(g_ActionSets, Input_GetActionSet())->name, "Inactive")) {
		return 1;
	}
	return Mn_SetI("MotionBlurOffVar", 0) && Mn_SetI("MotionBlurTime", 0) && Mode_GVarSetN("MotionBlurEndTime", 0.45) &&
		Mode_GVarSetN("MotionBlurFactorBegin", 0.0) && Mode_GVarSetN("MotionBlurFactorEnd", 0.5);
}

/* the demo camera on its dummy with its looping flight, and the menu camera linked to it */
static byte Mn_Cameras(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *dummy = Mode_V(&t), *zero = Mode_V(&t), *c = Mode_V(&t), *p = Mode_V(&t), *ang = Mode_V(&t);
	nativeVal_t *a1 = Mode_V(&t), *y = Mode_V(&t), *an = Mode_V(&t), *cls = Mode_V(&t), *cam = Mode_V(&t);
	nativeVal_t *name = Mode_V(&t), *cn = Mode_V(&t), *act = Mode_V(&t);
	Native_SetInt(zero, 0);
	LvN_Dummy("DM_Democam", 1, dummy);
	byte ok = LvN_Create(Mode_S(&t, "DemoCam"), zero, zero, zero, "WithLife", c) && LvN_Item(dummy, 1, p) &&
		Mode_VSet(c, "Pos", p) && LvN_Item(dummy, 2, ang) && LvN_Item(ang, 1, a1) && LvN_Arith('-', a1, Mode_I(&t, 180), y);
	if (ok) {
		Native_SetEmpty(an, NV_TUPLE);
		Native_SetInt(Native_Append(an), 0);
		Native_Copy(Native_Append(an), y);
		Native_SetInt(Native_Append(an), 0);
		ok = Mode_VSet(c, "Ang", an) && LvN_CreateClass("DemoCam", cls) &&
			LvN_SetCls("ModelFilePath", Mode_S(&t, "Levels/Menu/Map/SceneCamera/SceneCamera.M3D")) &&
			LvN_CreateAction("Relax", act) && LvN_SetAct("AnmFilePath", Mode_S(&t, "Levels/Menu/Map/SceneCamera/Anm/Loop.M3D")) &&
			LvN_SetAct("isLoop", Mode_I(&t, 1)) && LvN_SetCls("DefAction", Mode_S(&t, "Relax")) && LvN_SetS(c, "ActCtrl", "DemoCam");
	}
	ok = ok && LvN_Create(Mode_S(&t, "MenuCamara"), zero, zero, zero, "Cam", cam) && LvN_SetI(cam, "Fov", 60) &&
		LvN_SetI(cam, "clipNear", 50) && LvN_SetI(cam, "clipFar", 500000) && Mode_VGet(c, "Name", name) &&
		Mode_VSet(cam, "EntityLink", name) && Mode_VGet(cam, "Name", cn);
	if (ok) {
		/* Scrap.SetCam(0, name) */
		const char *s;
		ok = Native_ArgStr(cn, 2, &s);
		if (ok) {
			cWorld_SetCam(g_pWorld, 0, s);
		}
	}
	return Mode_Done(&t, ok);
}

/* the asteroids of the start: one each quarter second from 14 s, then the flight goes on (drotorito) */
static byte Mn_Asteroids(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *list = Mode_V(&t), *item = Mode_V(&t), *pos = Mode_V(&t), *tim = Mode_V(&t), *next = Mode_V(&t);
	nativeVal_t *quarter = Mode_V(&t);
	if (!LvN_GElems(MAP, "AstList", list)) {
		return Mode_Done(&t, 0);
	}
	int n = Native_ForCount(list);
	for (int i = 0; i < n; i++) {
		Native_Free(item);
		if (!Native_SeqItem(list, i, item) || !Mn_DebrisSys(item)) {
			return Mode_Done(&t, 0);
		}
	}
	int j = n < 0 ? -1 : Mn_GetTrue("JoinServerOnStartup");
	if (j < 0 || (j && (!Mode_Import("PCMenu") || !Mn_SetI("JoinServerOnStartup", 0) ||
		!LV_CALL("PCMenu", "TryJoin", NULL, "is", 0L, "")))) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(tim, 14);
	Native_SetFloat(quarter, 0.25);
	if (!LvN_GElems(MAP, "initpos", pos)) {
		return Mode_Done(&t, 0);
	}
	n = Native_ForCount(pos);
	for (int i = 0; i < n; i++) {
		Native_Free(item);
		Native_Free(next);
		if (!Native_SeqItem(pos, i, item) || !LV_OWN(MAP, "cremeteoro", NULL, "vv", item, tim) ||
			!LvN_Arith('+', tim, quarter, next)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(tim);
		Native_Copy(tim, next);
	}
	return Mode_Done(&t, n >= 0 && Mn_Own("drotorito") && Mn_SetI("IsMapOutDoor", 0));
}

/* Init(LevelPath): the menu scene, the menu (unless the credits show), the music and the asteroids */
static byte Menu_Init(const nativeVal_t *a, nativeVal_t *out) {
	static const char *const blur[] = { "MotionBlurOffVar", "MotionBlurTime", "MotionBlurEndTime", "MotionBlurFactorBegin",
		"MotionBlurFactorEnd" };
	(void)a;
	(void)out;
	if (!Mode_Import("Menu") || !Mode_Import("SAct") || !Mn_SetI("DrawNextFrame", 0) || !Mn_SetI("CameraCloudAlpha", 0)) {
		return 0;
	}
	for (int i = 0; i < 5; i++) {
		if (!Mn_SetI(blur[i], 0)) {
			return 0;
		}
	}
	if (!Mn_Cameras()) {
		return 0;
	}
	int m = LvN_GTrue(MAP, "InitMenu");
	if (m < 0 || (m && !Mn_Own("StartMenu")) || !Mode_Import("SSound")) {
		return 0;
	}
	int s = Mn_GetTrue("ShowCredits");
	if (s < 0) {
		return 0;
	}
	if (s) {
		if (!Mode_Import("Init") || !Mode_Import("CreditsText") || !LV_CALL("CreditsText", "Init", NULL, "")) {
			return 0;
		}
		Mode_SetActionSet("Inactive");
		if (!LV_CALL("Menu", "MovieScorer", NULL, "i", 0L) || !Mn_Own("ShowCredits") || !Mn_SetI("ShowCredits", 0) ||
			!LvN_SetMusic("Music/Creditos.ogg", 1.0)) {
			return 0;
		}
	} else if (!LvN_SetMusic("Music/Epic.ogg", 1.0)) {
		return 0;
	}
	return Mn_Asteroids();
}

/* the centre and the impulse of the next asteroid: every fourth one alternates around the camera, the others go
 * through the cycles xctr (centre) and rrr (impulse) */
static byte Mn_Throw(const nativeVal_t *mmr, double *centre, double *impulse) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *m = Mode_V(&t), *q = Mode_V(&t), *qi = Mode_V(&t), *e = Mode_V(&t), *x = Mode_V(&t);
	if (!LvN_Arith('%', mmr, Mode_I(&t, 4), m)) {
		return Mode_Done(&t, 0);
	}
	if (LvN_EqI(m, 0)) {
		*centre = Scrap_RandRange(500, -500);
		if (!LvN_Arith('/', mmr, Mode_I(&t, 4), q) || !Native_ToInt(q, qi) || !LvN_Arith('%', qi, Mode_I(&t, 2), e)) {
			return Mode_Done(&t, 0);
		}
		*impulse = LvN_EqI(e, 0) ? Scrap_RandRange(-1000, -500) : Scrap_RandRange(1000, 500);
		return Mode_Done(&t, 1);
	}
	static const double centres[4][2] = { { -1500, -1000 }, { 1000, 2500 }, { -1000, -500 }, { 500, 1000 } };
	static const double impulses[3][2] = { { -200, 200 }, { 400, 1000 }, { -1000, -400 } };
	if (!Mode_G(MAP, "xctr", x)) {
		return Mode_Done(&t, 0);
	}
	int xc = -1;
	for (int i = 0; i < 3 && xc < 0; i++) {
		if (LvN_EqI(x, i)) {
			xc = i;
		}
	}
	int slot = xc < 0 ? 3 : xc;
	/* the else branch sets xctr before it draws, the others after: the draw itself is the same */
	*centre = Scrap_RandRange(centres[slot][0], centres[slot][1]);
	if (!LvN_PutI(MAP, "xctr", xc < 0 ? 0 : xc + 1)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(x);
	if (!Mode_G(MAP, "rrr", x)) {
		return Mode_Done(&t, 0);
	}
	int rr = -1;
	for (int i = 0; i < 2 && rr < 0; i++) {
		if (LvN_EqI(x, i)) {
			rr = i;
		}
	}
	int rs = rr < 0 ? 2 : rr;
	*impulse = Scrap_RandRange(impulses[rs][0], impulses[rs][1]);
	return Mode_Done(&t, LvN_PutI(MAP, "rrr", rr < 0 ? 0 : rr + 1));
}

/* drotorito(): the next asteroid in front of the flying camera, again a quarter second later */
static byte Menu_drotorito(const nativeVal_t *a, nativeVal_t *out) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cam = Mode_V(&t), *dmc = Mode_V(&t), *vec = Mode_V(&t), *pos = Mode_V(&t), *sub = Mode_V(&t);
	nativeVal_t *dis = Mode_V(&t), *mmr = Mode_V(&t), *d0 = Mode_V(&t), *d2 = Mode_V(&t), *imp = Mode_V(&t);
	nativeVal_t *x = Mode_V(&t), *z = Mode_V(&t), *off = Mode_V(&t), *to = Mode_V(&t);
	double centre = 0, impulse = 0;
	(void)a;
	(void)out;
	if (!LvN_Entity("MenuCamara", cam) || !LvN_Entity("DemoCam", dmc) || !Mode_VGet(dmc, "NodePos_DM_Camera_01", vec) ||
		!Mode_VGet(dmc, "Pos", pos) || !Mode_VecSub(vec, pos, sub) || !Mode_VecNorm(sub, dis) || !LvN_GAdd(MAP, "mmr", 1, mmr) ||
		!Mn_Throw(mmr, &centre, &impulse) || !LvN_Item(dis, 0, d0) || !LvN_Item(dis, 2, d2)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(imp, impulse);
	if (!LvN_Arith('*', imp, d0, x) || !LvN_Arith('*', imp, d2, z)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(off, NV_TUPLE);
	Native_Copy(Native_Append(off), x);
	Native_SetFloat(Native_Append(off), centre);
	Native_Copy(Native_Append(off), z);
	return Mode_Done(&t, Mode_VecAdd(vec, off, to) && LV_OWN(MAP, "cremeteoro", NULL, "v", to) &&
		Mode_LaterIn(0.25, MAP, "drotorito", NULL, 0, NULL));
}

/* cremeteoro(vec, tim=35): an asteroid at vec with a random push and spin, the models in turn */
static byte Menu_cremeteoro(const nativeVal_t *vec, const nativeVal_t *tim) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *x = Mode_V(&t), *y = Mode_V(&t), *z = Mode_V(&t), *list = Mode_V(&t), *ctr = Mode_V(&t);
	nativeVal_t *model = Mode_V(&t), *n = Mode_V(&t), *list2 = Mode_V(&t);
	if (!LvN_Item(vec, 0, x) || !LvN_Item(vec, 1, y) || !LvN_Item(vec, 2, z) || !Mn_DebrisValue(0, x, y, z)) {
		return Mode_Done(&t, 0);
	}
	double ra = Scrap_RandRange(-180, 180), rb = Scrap_RandRange(-180, 180), rc = Scrap_RandRange(-180, 180);
	if (!Mn_DebrisValueD(1, ra, rb, rc) || !Mn_DebrisValue(2, Mode_I(&t, 0), Mode_I(&t, 0), Mode_I(&t, 0))) {
		return Mode_Done(&t, 0);
	}
	ra = Scrap_RandRange(-30, 30), rb = Scrap_RandRange(-30, 30), rc = Scrap_RandRange(-30, 30);
	if (!Mn_DebrisValueD(3, ra, rb, rc) || !Mode_G(MAP, "AstList", list) || !Mode_G(MAP, "globctr", ctr) ||
		!Native_Subscript(list, ctr, model) || !Mn_DropDebris(model, tim) || !LvN_GAdd(MAP, "globctr", 1, n) ||
		!Mode_G(MAP, "AstList", list2)) {
		return Mode_Done(&t, 0);
	}
	int len = Native_Size(list2);
	if (len < 0) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Native_Compare(n, Mode_I(&t, len)) < 0 || LvN_PutI(MAP, "globctr", 0));
}

/* ---- the adapters ---- */

LV_FN(Menu, InitStatusPy, 1, Menu_InitStatus)
LV_FN(Menu, FadeOutCreditsEndPy, 0, Menu_FadeOutCreditsEnd)
LV_FN(Menu, FadeOutCreditsPy, 0, Menu_FadeOutCredits)
LV_FN(Menu, NextCreditPy, 0, Menu_NextCredit)
LV_FN(Menu, ShowCreditsPy, 0, Menu_ShowCredits)
LV_FN(Menu, SetMotionBlurPy, 0, Menu_SetMotionBlur)
LV_FN(Menu, InitPy, 1, Menu_Init)
LV_FN(Menu, drotoritoPy, 0, Menu_drotorito)

/* EndCredits(id=0) */
static PyObject *Menu_EndCreditsPy(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", NULL };
	PyObject *id = NULL;
	nativeVal_t v;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &id)) {
		return NULL;
	}
	if (id) {
		Native_Arg(id, &v);
	} else {
		Native_SetInt(&v, 0);
	}
	byte ok = Menu_EndCredits(&v);
	Native_Free(&v);
	return Mode_Result(ok);
}

/* cremeteoro(vec, tim=35) */
static PyObject *Menu_cremeteoroPy(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "vec", "tim", NULL };
	PyObject *vec, *tim = NULL;
	nativeVal_t v, t;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|O", names, &vec, &tim)) {
		return NULL;
	}
	Native_Arg(vec, &v);
	if (tim) {
		Native_Arg(tim, &t);
	} else {
		Native_SetInt(&t, 35);
	}
	byte ok = Menu_cremeteoro(&v, &t);
	Native_Free(&v);
	Native_Free(&t);
	return Mode_Result(ok);
}

PyMethodDef g_MenuMapMethods[] = {
	{ "InitStatus", Menu_InitStatusPy, 1, NULL },
	{ "EndCredits", (PyCFunction)(void (*)(void))Menu_EndCreditsPy, 3, NULL },
	{ "FadeOutCreditsEnd", Menu_FadeOutCreditsEndPy, 1, NULL },
	{ "FadeOutCredits", Menu_FadeOutCreditsPy, 1, NULL },
	{ "NextCredit", Menu_NextCreditPy, 1, NULL },
	{ "ShowCredits", Menu_ShowCreditsPy, 1, NULL },
	{ "SetMotionBlur", Menu_SetMotionBlurPy, 1, NULL },
	{ "Init", Menu_InitPy, 1, NULL },
	{ "drotorito", Menu_drotoritoPy, 1, NULL },
	{ "cremeteoro", (PyCFunction)(void (*)(void))Menu_cremeteoroPy, 3, NULL },
	{ NULL, NULL, 0, NULL },
};
