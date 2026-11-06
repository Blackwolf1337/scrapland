/*
 * The Map module of the level Menu in C (levels/menu/scripts/map.py, issue #2): the main menu's scene (the demo camera
 * flying through the asteroids), the credits and the way back to the menu. StartMenu is data
 * (data/levels/menu/map.def). Libraries, InitMenu, AstList, initpos, CreditsCtr and the counters of the asteroids
 * (xctr, rrr, mmr, globctr) stay variables of the module.
 */
#include "scrap/script.h"
#include "scrap/scrap.h"

#define K(o) Script_Keep(&r, (o))
#define FAIL() return Script_Done(&r, NULL)

/* a global of Map (a new reference, NameError when missing) */
static PyObject *Mn_G(const char *name) {
	return Script_Global("Map", name);
}

/* fn(args) through Map's globals (args given away), the result dropped */
static byte Mn_Do(const char *fn, PyObject *args) {
	PyObject *f = Mn_G(fn), *v = f ? Script_Invoke(f, args) : NULL;
	if (!f) {
		Py_XDECREF(args);
	}
	Py_XDECREF(v);
	Py_XDECREF(f);
	return v != NULL;
}

/* Scrap.Set of several variables to 0 (the motion blur) */
static byte Mn_Zero(const char *const *vars) {
	for (; *vars; vars++) {
		if (!Script_Try("Scrap", "Set", "(si)", *vars, 0)) {
			return 0;
		}
	}
	return 1;
}

static const char *const blur_vars[] = { "MotionBlurOffVar", "MotionBlurTime", "MotionBlurEndTime", "MotionBlurFactorBegin",
	"MotionBlurFactorEnd", NULL };

/* InitStatus(LevelPath): in the main menu; the credits library when the credits are to be shown */
static PyObject *Menu_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path) || !Script_Import((const char *const[]){ "Init", NULL })) {
		return NULL;
	}
	Script_SetVar("Init", "inMainMenu", PyInt_FromLong(1));
	PyObject *show = Script_Ask("Scrap", "Get", "(s)", "ShowCredits");
	int s = show ? Script_True(show) : -1;
	Py_XDECREF(show);
	byte ok = s == 0 || (s > 0 && Script_Try("Scrap", "OpenPack", "(s)", "Scripts/Scorer") &&
		Script_Try("Scrap", "PreloadLibrary", "(ss)", "CreditsText", "CreditsText.pyc") && Script_Try("Scrap", "ClosePack", "()") &&
		Script_SetGlobal("Map", "InitMenu", PyInt_FromLong(0)));
	return ok ? Py_ReturnNone() : NULL;
}

/* EndCredits(id=0): the credits end: back to the menu with its music (the Xbox restarts) */
static PyObject *Menu_EndCredits(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", NULL };
	PyObject *id = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &id) || !Script_Import((const char *const[]){ "SaveGame", "SSound", NULL }) ||
		!Script_Try("Scrap", "Set", "(ss)", "EscapeEvent", "")) {
		return NULL;
	}
	PyObject *xbox = Script_Ask("Scrap", "Get", "(s)", "IsXbox");
	int x = xbox ? Script_True(xbox) : -1;
	Py_XDECREF(xbox);
	if (x < 0) {
		return NULL;
	}
	if (x) {
		return Script_Try("SaveGame", "Restart", "()") ? Py_ReturnNone() : NULL;
	}
	PyObject *a = id ? Py_BuildValue("(O)", id) : Py_BuildValue("(i)", 0);
	return Mn_Do("StartMenu", a) && Script_Try("Scrap", "MusicPlayer", "(ss)", "", "stop") &&
		Script_Try("SSound", "SetMusic", "(sd)", "Music/Epic.ogg", 0.8) ? Py_ReturnNone() : NULL;
}

/* FadeOutCreditsEnd(): the end of the credits unless they were aborted before */
static PyObject *Menu_FadeOutCreditsEnd(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *f = Script_Ask("Scrap", "Get", "(s)", "MotionBlurFactorEnd"), *zero = f ? PyFloat_FromDouble(0.0) : NULL;
	int c = zero ? PyObject_Compare(f, zero) : 0;
	byte bad = !zero || PyErr_Occurred();
	Py_XDECREF(zero);
	Py_XDECREF(f);
	if (bad) {
		return NULL;
	}
	return c <= 0 || Mn_Do("EndCredits", PyTuple_New(0)) ? Py_ReturnNone() : NULL;
}

/* FadeOutCredits(): the music fades in 4 s, then FadeOutCreditsEnd */
static PyObject *Menu_FadeOutCredits(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (!Script_Import((const char *const[]){ "SSound", NULL }) || !Script_Try("Scrap", "MusicPlayer", "(ssd)", "", "fadeout", 1 / 4.0)) {
		return NULL;
	}
	return Script_LaterIn(4.0 + 0.1, "Map", "FadeOutCreditsEnd", PyTuple_New(0), NULL) ? Py_ReturnNone() : NULL;
}

/* NextCredit(): the next credit page with text (a "-" page is skipped), the fade out after the last */
static PyObject *Menu_NextCredit(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (!Script_Import((const char *const[]){ "CreditsText", "Menu", NULL })) {
		return NULL;
	}
	for (;;) {
		PyObject *ctr = Mn_G("CreditsCtr"), *one = ctr ? PyInt_FromLong(1) : NULL, *n = one ? PyNumber_Add(ctr, one) : NULL;
		Py_XDECREF(one);
		Py_XDECREF(ctr);
		if (!n || !Script_SetGlobal("Map", "CreditsCtr", n)) {
			return NULL;
		}
		PyObject *rep = Script_Repr(n), *key = rep ? Script_PlusS(Script_SPlus("Credits_", rep), "_1") : NULL;
		Py_XDECREF(rep);
		PyObject *text = key ? Script_LangOf(key) : NULL;
		if (!text) {
			return NULL;
		}
		byte end = Script_Is(text, ""), skip = Script_Is(text, "-");
		Py_DECREF(text);
		if (end) {
			return Mn_Do("FadeOutCredits", PyTuple_New(0)) ? Py_ReturnNone() : NULL;
		}
		if (!skip) {
			break;
		}
	}
	PyObject *ctr = Mn_G("CreditsCtr");
	byte ok = ctr && Script_Try("CreditsText", "ShowCredit", "(O)", ctr);
	Py_XDECREF(ctr);
	return ok ? Py_ReturnNone() : NULL;
}

/* ShowCredits(): the credits from the first page, Escape ends them, the motion blur a moment later */
static PyObject *Menu_ShowCredits(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (!Script_Import((const char *const[]){ "CreditsText", NULL }) || !Script_SetGlobal("Map", "CreditsCtr", PyInt_FromLong(0))) {
		return NULL;
	}
	PyObject *next = Mn_G("NextCredit");
	if (!next) {
		return NULL;
	}
	Script_SetVar("CreditsText", "EndOfCredits", next);
	return Mn_Do("NextCredit", PyTuple_New(0)) && Script_Try("Scrap", "Set", "(ss)", "EscapeEvent", "Map.EndCredits") &&
		Script_LaterIn(0.15, "Map", "SetMotionBlur", PyTuple_New(0), NULL) ? Py_ReturnNone() : NULL;
}

/* SetMotionBlur(): the blur of the credits while the input is inactive */
static PyObject *Menu_SetMotionBlur(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *set = Script_Ask("SInput", "GetActionSet", "()");
	if (!set) {
		return NULL;
	}
	byte inactive = Script_Is(set, "Inactive");
	Py_DECREF(set);
	byte ok = !inactive || (Script_Try("Scrap", "Set", "(si)", "MotionBlurOffVar", 0) && Script_Try("Scrap", "Set", "(si)", "MotionBlurTime", 0) &&
		Script_Try("Scrap", "Set", "(sd)", "MotionBlurEndTime", 0.45) && Script_Try("Scrap", "Set", "(sd)", "MotionBlurFactorBegin", 0.0) &&
		Script_Try("Scrap", "Set", "(sd)", "MotionBlurFactorEnd", 0.5));
	return ok ? Py_ReturnNone() : NULL;
}

/* the demo camera on its dummy with its looping flight, and the menu camera linked to it */
static byte Mn_Cameras(void) {
	scriptRefs_t r = { .n = 0 };
	PyObject *dummy = Script_Try("Scrap", "StartDummySearch", "(si)", "DM_Democam", 1) ? K(Script_Ask("Scrap", "NextDummySearch", "()")) : NULL;
	PyObject *c = dummy ? K(Script_Ask("Scrap", "CreateEntity", "(siiis)", "DemoCam", 0, 0, 0, "WithLife")) : NULL;
	PyObject *ang = c && Script_HSet(c, "Pos", Script_Index(dummy, 1)) ? K(Script_Index(dummy, 2)) : NULL;
	PyObject *a1 = ang ? K(Script_Index(ang, 1)) : NULL, *k = a1 ? K(PyInt_FromLong(180)) : NULL;
	PyObject *y = k ? K(PyNumber_Subtract(a1, k)) : NULL;
	if (!y || !Script_HSet(c, "Ang", Py_BuildValue("(iOi)", 0, y, 0)) || !Script_Try("SAct", "CreateClass", "(s)", "DemoCam") ||
		!Script_Try("SAct", "SetCls", "(ss)", "ModelFilePath", "Levels/Menu/Map/SceneCamera/SceneCamera.M3D") ||
		!Script_Try("SAct", "CreateAction", "(s)", "Relax") ||
		!Script_Try("SAct", "SetAct", "(ss)", "AnmFilePath", "Levels/Menu/Map/SceneCamera/Anm/Loop.M3D") ||
		!Script_Try("SAct", "SetAct", "(si)", "isLoop", 1) || !Script_Try("SAct", "SetCls", "(ss)", "DefAction", "Relax") ||
		!Script_HSet(c, "ActCtrl", PyString_FromString("DemoCam"))) {
		return Script_Done(&r, NULL), 0;
	}
	PyObject *cam = K(Script_Ask("Scrap", "CreateEntity", "(siiis)", "MenuCamara", 0, 0, 0, "Cam"));
	PyObject *cn = cam && Script_HSet(cam, "Fov", PyInt_FromLong(60)) && Script_HSet(cam, "clipNear", PyInt_FromLong(50)) &&
		Script_HSet(cam, "clipFar", PyInt_FromLong(500000)) && Script_HSet(cam, "EntityLink", Script_GetAttr(c, "Name")) ?
		K(Script_GetAttr(cam, "Name")) : NULL;
	byte ok = cn && Script_Try("Scrap", "SetCam", "(iO)", 0, cn);
	Script_Done(&r, NULL);
	return ok;
}

/* the asteroids of the start: one each quarter second from 14 s, then the flight goes on (drotorito) */
static byte Mn_Asteroids(void) {
	PyObject *list = Mn_G("AstList"), *a;
	int more;
	for (int i = 0; list && (more = Script_Next(list, i, &a)) > 0; i++) {
		byte ok = Script_Try("Scrap", "SetDebrisSys", "(Oiiidii)", a, 30, 1000000, 0, 0.02, 0, 0);
		Py_DECREF(a);
		if (!ok) {
			more = -1;
			break;
		}
	}
	Py_XDECREF(list);
	if (!list || more < 0) {
		return 0;
	}
	PyObject *join = Script_Ask("Scrap", "Get", "(s)", "JoinServerOnStartup");
	int j = join ? Script_True(join) : -1;
	Py_XDECREF(join);
	if (j < 0 || (j && (!Script_Import((const char *const[]){ "PCMenu", NULL }) || !Script_Try("Scrap", "Set", "(si)", "JoinServerOnStartup", 0) ||
		!Script_Try("PCMenu", "TryJoin", "(is)", 0, "")))) {
		return 0;
	}
	PyObject *tim = PyInt_FromLong(14), *quarter = PyFloat_FromDouble(0.25), *pos = Mn_G("initpos"), *vec;
	byte ok = tim && quarter && pos;
	for (int i = 0; ok && (more = Script_Next(pos, i, &vec)) > 0; i++) {
		ok = Mn_Do("cremeteoro", Py_BuildValue("(OO)", vec, tim));
		PyObject *t = ok ? PyNumber_Add(tim, quarter) : NULL;
		Py_DECREF(vec);
		Py_DECREF(tim);
		tim = t;
		ok = tim != NULL;
	}
	ok = ok && more == 0;
	Py_XDECREF(pos);
	Py_XDECREF(quarter);
	Py_XDECREF(tim);
	return ok && Mn_Do("drotorito", PyTuple_New(0)) && Script_Try("Scrap", "Set", "(si)", "IsMapOutDoor", 0);
}

/* Init(LevelPath): the menu scene, the menu (unless the credits show), the music and the asteroids */
static PyObject *Menu_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path) || !Script_Import((const char *const[]){ "Menu", "SAct", NULL }) ||
		!Script_Try("Scrap", "Set", "(si)", "DrawNextFrame", 0) || !Script_Try("Scrap", "Set", "(si)", "CameraCloudAlpha", 0) ||
		!Mn_Zero(blur_vars) || !Mn_Cameras()) {
		return NULL;
	}
	PyObject *init = Mn_G("InitMenu");
	int m = init ? Script_True(init) : -1;
	Py_XDECREF(init);
	if (m < 0 || (m && !Mn_Do("StartMenu", PyTuple_New(0))) || !Script_Import((const char *const[]){ "SSound", NULL })) {
		return NULL;
	}
	PyObject *show = Script_Ask("Scrap", "Get", "(s)", "ShowCredits");
	int s = show ? Script_True(show) : -1;
	Py_XDECREF(show);
	if (s < 0) {
		return NULL;
	}
	if (s) {
		if (!Script_Import((const char *const[]){ "Init", "CreditsText", NULL }) || !Script_Try("CreditsText", "Init", "()") ||
			!Script_Try("SInput", "SetActionSet", "(s)", "Inactive") || !Script_Try("Menu", "MovieScorer", "(i)", 0) ||
			!Mn_Do("ShowCredits", PyTuple_New(0)) || !Script_Try("Scrap", "Set", "(si)", "ShowCredits", 0) ||
			!Script_Try("SSound", "SetMusic", "(sd)", "Music/Creditos.ogg", 1.0)) {
			return NULL;
		}
	} else if (!Script_Try("SSound", "SetMusic", "(sd)", "Music/Epic.ogg", 1.0)) {
		return NULL;
	}
	return Mn_Asteroids() ? Py_ReturnNone() : NULL;
}

/* the counter global name + 1 (a new reference of the new value) */
static PyObject *Mn_Inc(const char *name) {
	PyObject *v = Mn_G(name), *one = v ? PyInt_FromLong(1) : NULL, *n = one ? PyNumber_Add(v, one) : NULL;
	Py_XDECREF(one);
	Py_XDECREF(v);
	if (n && !Script_SetGlobal("Map", name, Script_Ref(n))) {
		Py_DECREF(n);
		return NULL;
	}
	return n;
}

/* the centre and the impulse of the next asteroid: every fourth one alternates around the camera, the others go
 * through the cycles xctr (centre) and rrr (impulse) */
static byte Mn_Throw(PyObject *mmr, double *centre, double *impulse) {
	PyObject *four = PyInt_FromLong(4), *m = four ? PyNumber_Remainder(mmr, four) : NULL;
	int every = m ? Script_NumCmp(m, 0) : -2;
	Py_XDECREF(m);
	if (every == -2) {
		Py_XDECREF(four);
		return 0;
	}
	if (every == 0) {
		*centre = Scrap_RandRange(500, -500);
		PyObject *q = PyNumber_Divide(mmr, four), *qi = q ? Script_Int(q) : NULL, *two = qi ? PyInt_FromLong(2) : NULL;
		PyObject *e = two ? PyNumber_Remainder(qi, two) : NULL;
		int even = e ? Script_NumCmp(e, 0) : -2;
		Py_XDECREF(e);
		Py_XDECREF(two);
		Py_XDECREF(qi);
		Py_XDECREF(q);
		Py_DECREF(four);
		if (even == -2) {
			return 0;
		}
		*impulse = even == 0 ? Scrap_RandRange(-1000, -500) : Scrap_RandRange(1000, 500);
		return 1;
	}
	Py_DECREF(four);
	static const double centres[4][2] = { { -1500, -1000 }, { 1000, 2500 }, { -1000, -500 }, { 500, 1000 } };
	static const double impulses[3][2] = { { -200, 200 }, { 400, 1000 }, { -1000, -400 } };
	PyObject *x = Mn_G("xctr");
	int xc = -1;
	for (int i = 0; x && i < 3 && xc < 0; i++) {
		int c = Script_NumCmp(x, i);
		if (c == -2) {
			Py_DECREF(x);
			return 0;
		}
		if (c == 0) {
			xc = i;
		}
	}
	Py_XDECREF(x);
	if (!x) {
		return 0;
	}
	int slot = xc < 0 ? 3 : xc;
	/* the else branch sets xctr before it draws, the others after: the draw itself is the same */
	*centre = Scrap_RandRange(centres[slot][0], centres[slot][1]);
	if (!Script_SetGlobal("Map", "xctr", PyInt_FromLong(xc < 0 ? 0 : xc + 1))) {
		return 0;
	}
	PyObject *rv = Mn_G("rrr");
	int rr = -1;
	for (int i = 0; rv && i < 2 && rr < 0; i++) {
		int c = Script_NumCmp(rv, i);
		if (c == -2) {
			Py_DECREF(rv);
			return 0;
		}
		if (c == 0) {
			rr = i;
		}
	}
	Py_XDECREF(rv);
	if (!rv) {
		return 0;
	}
	int rs = rr < 0 ? 2 : rr;
	*impulse = Scrap_RandRange(impulses[rs][0], impulses[rs][1]);
	return Script_SetGlobal("Map", "rrr", PyInt_FromLong(rr < 0 ? 0 : rr + 1));
}

/* drotorito(): the next asteroid in front of the flying camera, again a quarter second later */
static PyObject *Menu_drotorito(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *cam = K(Script_Ask("Scrap", "GetEntity", "(s)", "MenuCamara"));
	PyObject *dmc = cam ? K(Script_Ask("Scrap", "GetEntity", "(s)", "DemoCam")) : NULL;
	PyObject *vec = dmc ? K(Script_GetAttr(dmc, "NodePos_DM_Camera_01")) : NULL;
	PyObject *pos = vec ? K(Script_GetAttr(dmc, "Pos")) : NULL, *sub = pos ? K(Script_Ask("SVec", "Sub", "(OO)", vec, pos)) : NULL;
	PyObject *dis = sub ? K(Script_Ask("SVec", "Norm", "(O)", sub)) : NULL, *mmr = dis ? K(Mn_Inc("mmr")) : NULL;
	double centre, impulse;
	if (!mmr || !Mn_Throw(mmr, &centre, &impulse)) {
		FAIL();
	}
	PyObject *d0 = K(Script_Index(dis, 0)), *d2 = d0 ? K(Script_Index(dis, 2)) : NULL, *imp = d2 ? K(PyFloat_FromDouble(impulse)) : NULL;
	PyObject *x = imp ? K(PyNumber_Multiply(imp, d0)) : NULL, *z = x ? K(PyNumber_Multiply(imp, d2)) : NULL;
	PyObject *to = z ? K(Script_Ask("SVec", "Add", "(O(OdO))", vec, x, centre, z)) : NULL;
	if (!to || !Mn_Do("cremeteoro", Py_BuildValue("(O)", to)) || !Script_LaterIn(0.25, "Map", "drotorito", PyTuple_New(0), NULL)) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

/* cremeteoro(vec, tim=35): an asteroid at vec with a random push and spin, the models in turn */
static PyObject *Menu_cremeteoro(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "vec", "tim", NULL };
	scriptRefs_t r = { .n = 0 };
	PyObject *vec, *tim = NULL;
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|O", names, &vec, &tim)) {
		return NULL;
	}
	if (!tim) {
		tim = K(PyInt_FromLong(35));
	}
	PyObject *x = K(Script_Index(vec, 0)), *y = x ? K(Script_Index(vec, 1)) : NULL, *z = y ? K(Script_Index(vec, 2)) : NULL;
	if (!z || !Script_Try("Scrap", "SetDebrisValue", "(iOOO)", 0, x, y, z)) {
		FAIL();
	}
	double a = Scrap_RandRange(-180, 180), b = Scrap_RandRange(-180, 180), c = Scrap_RandRange(-180, 180);
	if (!Script_Try("Scrap", "SetDebrisValue", "(iddd)", 1, a, b, c) || !Script_Try("Scrap", "SetDebrisValue", "(iiii)", 2, 0, 0, 0)) {
		FAIL();
	}
	a = Scrap_RandRange(-30, 30), b = Scrap_RandRange(-30, 30), c = Scrap_RandRange(-30, 30);
	PyObject *list = Script_Try("Scrap", "SetDebrisValue", "(iddd)", 3, a, b, c) ? K(Mn_G("AstList")) : NULL;
	PyObject *ctr = list ? K(Mn_G("globctr")) : NULL, *model = ctr ? K(PyObject_GetItem(list, ctr)) : NULL;
	PyObject *n = model && Script_Try("Scrap", "DropDebris", "(OO)", model, tim) ? K(Mn_Inc("globctr")) : NULL;
	int len = n ? PyObject_Length(K(Mn_G("AstList"))) : -1;
	int wrap = len >= 0 ? Script_NumCmp(n, len) : -2;
	if (wrap == -2 || (wrap >= 0 && !Script_SetGlobal("Map", "globctr", PyInt_FromLong(0)))) {
		FAIL();
	}
	return Script_Done(&r, Py_ReturnNone());
}

PyMethodDef g_MenuMapMethods[] = {
	{ "InitStatus", Menu_InitStatus, 1, NULL },
	{ "EndCredits", (PyCFunction)(void (*)(void))Menu_EndCredits, 3, NULL },
	{ "FadeOutCreditsEnd", Menu_FadeOutCreditsEnd, 1, NULL },
	{ "FadeOutCredits", Menu_FadeOutCredits, 1, NULL },
	{ "NextCredit", Menu_NextCredit, 1, NULL },
	{ "ShowCredits", Menu_ShowCredits, 1, NULL },
	{ "SetMotionBlur", Menu_SetMotionBlur, 1, NULL },
	{ "Init", Menu_Init, 1, NULL },
	{ "drotorito", Menu_drotorito, 1, NULL },
	{ "cremeteoro", (PyCFunction)(void (*)(void))Menu_cremeteoro, 3, NULL },
	{ NULL, NULL, 0, NULL },
};
