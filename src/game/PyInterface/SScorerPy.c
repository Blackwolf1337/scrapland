/*
 * Python module SScorer (method table SScorer_methods): HUD, menus, chat and speech texts of the
 * local players. The original source file is not known (no CHECK in it). Most functions take the player number
 * first and give None when that player has no scorer.
 */
#include <string.h>

#include "scrap/input.h"
#include "scrap/scorer.h"

/* 0xAARRGGBB from the (r, g, b, a) values of the script */
static uint32_t PackColor(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
	return ((a << 8 | r) << 8 | g) << 8 | b;
}

/* AddChatMsg(string,red,green,blue) : Agrega un mensaje a la list de chat. */
static PyObject *SScorer_AddChatMsg(PyObject *self, PyObject *args) {
	char *text;
	uint32_t r, g, b;

	(void)self;
	if (!PyArg_ParseTuple(args, "siii", &text, &r, &g, &b)) {
		return NULL;
	}
	Scorer_AddChatMsg(text, (r << 8 | g) << 8 | b);
	return Py_ReturnNone();
}

/* SetConsole(on) */
static PyObject *SScorer_SetConsole(PyObject *self, PyObject *args) {
	int on;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &on)) {
		return NULL;
	}
	Scorer_SetConsole(on != 0);
	return Py_ReturnNone();
}

/* SetSpeechText(text, time, a, b, c) */
static PyObject *SScorer_SetSpeechText(PyObject *self, PyObject *args) {
	char *text;
	double time;
	int a, b, c;

	(void)self;
	if (!PyArg_ParseTuple(args, "sdiii", &text, &time, &a, &b, &c)) {
		return NULL;
	}
	Scorer_SetSpeechText(text, (float)time, a, b, c);
	return Py_ReturnNone();
}

/* SetSpeechCallback(func[, arg]) */
static PyObject *SScorer_SetSpeechCallback(PyObject *self, PyObject *args) {
	char *func;
	PyObject *arg = NULL;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|O", &func, &arg)) {
		return NULL;
	}
	Scorer_SetSpeechCallback(func, arg);
	return Py_ReturnNone();
}

/* CancelSpeech() */
static PyObject *SScorer_CancelSpeech(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Scorer_CancelSpeech(0);
	return Py_ReturnNone();
}

/* SetHeadMonitor(a, b, c) */
static PyObject *SScorer_SetHeadMonitor(PyObject *self, PyObject *args) {
	char *a, *b;
	int c;

	(void)self;
	if (!PyArg_ParseTuple(args, "ssi", &a, &b, &c)) {
		return NULL;
	}
	Scorer_SetHeadMonitor(a, b, c);
	return Py_ReturnNone();
}

/* Fade(player, (r,g,b,a) from, (r,g,b,a) to, time) */
static PyObject *SScorer_Fade(PyObject *self, PyObject *args) {
	int player;
	uint32_t r1, g1, b1, a1, r2, g2, b2, a2;
	float time;

	(void)self;
	if (!PyArg_ParseTuple(args, "i(iiii)(iiii)f", &player, &r1, &g1, &b1, &a1, &r2, &g2, &b2, &a2, &time)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc) {
		ScorerFade_Start(&sc->fade, PackColor(r1, g1, b1, a1), PackColor(r2, g2, b2, a2), time);
	}
	return Py_ReturnNone();
}

/* SetCinema(player, on, time) : cinema bars */
static PyObject *SScorer_SetCinema(PyObject *self, PyObject *args) {
	int player, on;
	float time;

	(void)self;
	if (!PyArg_ParseTuple(args, "iif", &player, &on, &time)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc) {
		ScorerCinema_Set(&sc->cinema, on != 0, time);
	}
	return Py_ReturnNone();
}

/* SetMsgText(text, time) : in the font "Horatio" */
static PyObject *SScorer_SetMsgText(PyObject *self, PyObject *args) {
	char *text;
	double time;

	(void)self;
	if (!PyArg_ParseTuple(args, "sd", &text, &time)) {
		return NULL;
	}
	Scorer_SetMsgText(text, (float)time, "Horatio");
	return Py_ReturnNone();
}

/* SetLabelText(text, time) */
static PyObject *SScorer_SetLabelText(PyObject *self, PyObject *args) {
	char *text;
	double time;

	(void)self;
	if (!PyArg_ParseTuple(args, "sd", &text, &time)) {
		return NULL;
	}
	Scorer_SetLabelText(text, (float)time);
	return Py_ReturnNone();
}

/* Get2DPos((x,y,z)) : screen position of a world point */
static PyObject *SScorer_Get2DPos(PyObject *self, PyObject *args) {
	vec3_t pos;
	float out[2];

	(void)self;
	if (!PyArg_ParseTuple(args, "(fff)", &pos.x, &pos.y, &pos.z)) {
		return NULL;
	}
	Engine_WorldToScreen(&pos, out, 0);
	return Py_BuildValue("ff", (double)out[0], (double)out[1]);
}

/* (width, height) GetTextArea(fontType, text) */
static PyObject *SScorer_GetTextArea(PyObject *self, PyObject *args) {
	char *fontType, *text;
	float rect[2], size[2];

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &fontType, &text)) {
		return NULL;
	}
	Font_TextSize(Font_Get(fontType), rect, size, 0.0f, 0.0f, "%s", text);
	return Py_BuildValue("ff", (double)size[0], (double)size[1]);
}

/* GetMenuAccept(player) : 1 while the accept action (action 10 of the first action set) is held */
static PyObject *SScorer_GetMenuAccept(PyObject *self, PyObject *args) {
	int player;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &player)) {
		return NULL;
	}
	if (!Scorer_Get(player)) {
		return Py_ReturnNone();
	}
	cInputAction *accept = TABLE_AT(TABLE_AT(g_ActionSets, 0)->actions, 10);
	return Py_BuildValue("i", accept->value[player] > 0.5f);
}

/* SetMarkerSprite(player, sprite) : "" for none */
static PyObject *SScorer_SetMarkerSprite(PyObject *self, PyObject *args) {
	int player;
	char *sprite;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &player, &sprite)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return Py_ReturnNone();
	}
	cScorer_SetMarkerSprite(sc, sprite && *sprite ? sprite : NULL);
	return Py_BuildValue("i", 1);
}

/* Add(player, name, type[, flag]) : 1 if the item was made */
static PyObject *SScorer_Add(PyObject *self, PyObject *args) {
	int player, flag = 0;
	char *name, *type;

	(void)self;
	if (!PyArg_ParseTuple(args, "iss|i", &player, &name, &type, &flag)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("i", cScorer_AddItem(sc, name, type, flag != 0) != NULL);
}

/* Clear(player) */
static PyObject *SScorer_Clear(PyObject *self, PyObject *args) {
	int player;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &player)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return Py_ReturnNone();
	}
	cScorer_Clear(sc);
	return Py_BuildValue("i", 1);
}

/*
 * Set(player, item, var, value) : value as string ("isss") or number ("issd", stored as double and truncated
 * int). The number form is only tried when the string form does not parse.
 */
static PyObject *SScorer_Set(PyObject *self, PyObject *args) {
	int player, i;
	char *name, *var, *s;
	double d;
	cScorer *sc;
	cScorerItem *item;

	(void)self;
	if (PyArg_ParseTuple(args, "isss", &player, &name, &var, &s)) {
		if ((sc = Scorer_Get(player)) && (item = cScorer_FindItem(sc, name))) {
			return Py_BuildValue("i", ScorerItem_Var(item, var, 1, &s, NULL, NULL));
		}
		return Py_ReturnNone();
	}
	PyErr_Clear();
	if (!PyArg_ParseTuple(args, "issd", &player, &name, &var, &d)) {
		return NULL;
	}
	sc = Scorer_Get(player);
	i = (int)d;
	if (sc && (item = cScorer_FindItem(sc, name))) {
		return Py_BuildValue("i", ScorerItem_Var(item, var, 1, NULL, &d, &i));
	}
	return Py_ReturnNone();
}

/* Get(player, item, var) : value in its type, None if not found */
static PyObject *SScorer_Get(PyObject *self, PyObject *args) {
	int player, i;
	char *name, *var, *s;
	double d;

	(void)self;
	if (!PyArg_ParseTuple(args, "iss", &player, &name, &var)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	cScorerItem *item = sc ? cScorer_FindItem(sc, name) : NULL;
	if (item) {
		switch (ScorerItem_Var(item, var, 0, &s, &d, &i)) {
		case 1:
			return Py_BuildValue("s", s);
		case 2:
			return Py_BuildValue("d", d);
		case 3:
			return Py_BuildValue("i", i);
		}
	}
	return Py_ReturnNone();
}

/* GetItem(player, item) : the Python object of the item, None if not found */
static PyObject *SScorer_GetItem(PyObject *self, PyObject *args) {
	int player;
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &player, &name)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc && cScorer_FindItem(sc, name)) {
		return ScorerItem_PyNew(player, name);
	}
	return Py_ReturnNone();
}

/* New(player, name, type[, flag]) : makes the item and returns its Python object; NULL if that fails */
static PyObject *SScorer_New(PyObject *self, PyObject *args) {
	int player, flag = 0;
	char *name, *type;

	(void)self;
	if (!PyArg_ParseTuple(args, "iss|i", &player, &name, &type, &flag)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc && cScorer_AddItem(sc, name, type, flag != 0)) {
		return ScorerItem_PyNew(player, name);
	}
	return NULL;
}

/* Show(playernumber) : Activa el scorer. */
static PyObject *SScorer_Show(PyObject *self, PyObject *args) {
	int player;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &player)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return Py_ReturnNone();
	}
	sc->visible = 1;
	return Py_BuildValue("i", 1);
}

/* Hide(playernumber) */
static PyObject *SScorer_Hide(PyObject *self, PyObject *args) {
	int player;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &player)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return Py_ReturnNone();
	}
	sc->visible = 0;
	return Py_BuildValue("i", 1);
}

/* SetCursor(playernumber,CursorName) : pone un item del scorer como cursor (NULL if not found) */
static PyObject *SScorer_SetCursor(PyObject *self, PyObject *args) {
	int player;
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &player, &name)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return Py_ReturnNone();
	}
	sc->cursor = cScorer_FindItem(sc, name);
	return Py_BuildValue("i", 1);
}

/* SetDefault(playernumber, ItemName) */
static PyObject *SScorer_SetDefault(PyObject *self, PyObject *args) {
	int player;
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &player, &name)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return Py_ReturnNone();
	}
	cScorer_SetDefault(sc, name);
	return Py_BuildValue("i", 1);
}

/* DefaultItemName GetDefault(playernumber) */
static PyObject *SScorer_GetDefault(PyObject *self, PyObject *args) {
	int player;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &player)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc && sc->defaultItem) {
		return Py_BuildValue("s", sc->defaultItem);
	}
	return Py_ReturnNone();
}

/* GetActual(playernumber) */
static PyObject *SScorer_GetCurrent(PyObject *self, PyObject *args) {
	int player;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &player)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc && sc->current) {
		return Py_BuildValue("s", sc->current);
	}
	return Py_ReturnNone();
}

/* SetOnCancel(playernumber, CancelEvent) : returns 1 even without a scorer */
static PyObject *SScorer_SetOnCancel(PyObject *self, PyObject *args) {
	int player;
	char *func;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &player, &func)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc) {
		cPy_Function_SetFunc(&sc->onCancel, func);
	}
	return Py_BuildValue("i", 1);
}

/* SetOnNext(playernumber, NextEvent) */
static PyObject *SScorer_SetOnNext(PyObject *self, PyObject *args) {
	int player;
	char *func;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &player, &func)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return Py_ReturnNone();
	}
	cPy_Function_SetFunc(&sc->onNext, func);
	return Py_BuildValue("i", 1);
}

/* SetOnPrev(playernumber, PrevEvent) */
static PyObject *SScorer_SetOnPrev(PyObject *self, PyObject *args) {
	int player;
	char *func;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &player, &func)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return Py_ReturnNone();
	}
	cPy_Function_SetFunc(&sc->onPrev, func);
	return Py_BuildValue("i", 1);
}

/* SetOnSpecialHint(playernumber, SpecialHintTestFunc, SpecialHint) */
static PyObject *SScorer_SetOnSpecialHint(PyObject *self, PyObject *args) {
	int player;
	char *func, *hint;

	(void)self;
	if (!PyArg_ParseTuple(args, "iss", &player, &func, &hint)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return Py_ReturnNone();
	}
	cPy_Function_SetFunc(&sc->onSpecialHint, func);
	cString_Set(&sc->specialHint, hint);
	return Py_BuildValue("i", 1);
}

/* AddModel(playernumber, Name, ModelFile, anim, numanim, Radius[, c[, flag]]) : anim "" for none */
static PyObject *SScorer_AddModel(PyObject *self, PyObject *args) {
	int player, numAnim, flag = 0;
	char *name, *model, *anim;
	float radius, c = 0.0f;

	(void)self;
	if (!PyArg_ParseTuple(args, "isssif|fi", &player, &name, &model, &anim, &numAnim, &radius, &c, &flag)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc && cScorer_AddModel(sc, name, model, *anim ? anim : NULL, numAnim, radius, c, flag > 0)) {
		return Py_BuildValue("i", 1);
	}
	return Py_ReturnNone();
}

/* SetMPFunc(playernumber, func) : always None */
static PyObject *SScorer_SetMPFunc(PyObject *self, PyObject *args) {
	int player;
	char *func;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &player, &func)) {
		return NULL;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc) {
		cPy_Function_SetFunc(&sc->mpFunc, func);
	}
	return Py_ReturnNone();
}

/* PreloadTexture(name[, flag]) */
static PyObject *SScorer_PreloadTexture(PyObject *self, PyObject *args) {
	char *name;
	int flag = 0;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|i", &name, &flag)) {
		return NULL;
	}
	Scorer_PreloadTexture(name, flag > 0);
	return Py_ReturnNone();
}

static char str_PyDoc_SScorer_Add[];
static char str_PyDoc_SScorer_AddChatMsg[80];
static char str_PyDoc_SScorer_AddModel[];
static char str_PyDoc_SScorer_CancelSpeech[];
static char str_PyDoc_SScorer_Clear[];
static char str_PyDoc_SScorer_Fade[];
static char str_PyDoc_SScorer_Get[];
static char str_PyDoc_SScorer_Get2DPos[];
static char str_PyDoc_SScorer_GetCurrent[64];
static char str_PyDoc_SScorer_GetDefault[];
static char str_PyDoc_SScorer_GetItem[];
static char str_PyDoc_SScorer_GetMenuAccept[];
static char str_PyDoc_SScorer_GetTextArea[128];
static char str_PyDoc_SScorer_Hide[];
static char str_PyDoc_SScorer_New[];
static char str_PyDoc_SScorer_PreloadTexture[];
static char str_PyDoc_SScorer_Set[96];
static char str_PyDoc_SScorer_SetCinema[];
static char str_PyDoc_SScorer_SetConsole[72];
static char str_PyDoc_SScorer_SetCursor[80];
static char str_PyDoc_SScorer_SetDefault[88];
static char str_PyDoc_SScorer_SetHeadMonitor[];
static char str_PyDoc_SScorer_SetLabelText[104];
static char str_PyDoc_SScorer_SetMPFunc[104];
static char str_PyDoc_SScorer_SetMarkerSprite[];
static char str_PyDoc_SScorer_SetMsgText[];
static char str_PyDoc_SScorer_SetOnCancel[104];
static char str_PyDoc_SScorer_SetOnNext[];
static char str_PyDoc_SScorer_SetOnPrev[];
static char str_PyDoc_SScorer_SetOnSpecialHint[];
static char str_PyDoc_SScorer_SetSpeechCallback[];
static char str_PyDoc_SScorer_SetSpeechText[];
static char str_PyDoc_SScorer_Show[];
extern const char str_PyMethod_SScorer_Add[];
static const char str_PyMethod_SScorer_AddChatMsg[];
static const char str_PyMethod_SScorer_AddModel[];
static const char str_PyMethod_SScorer_CancelSpeech[];
static const char str_PyMethod_SScorer_Clear[];
static const char str_PyMethod_SScorer_Fade[];
extern const char str_PyMethod_SScorer_Get[];
static const char str_PyMethod_SScorer_Get2DPos[];
static const char str_PyMethod_SScorer_GetCurrent[];
static const char str_PyMethod_SScorer_GetDefault[];
static const char str_PyMethod_SScorer_GetItem[];
static const char str_PyMethod_SScorer_GetMenuAccept[];
static const char str_PyMethod_SScorer_GetTextArea[];
static const char str_PyMethod_SScorer_Hide[];
static const char str_PyMethod_SScorer_New[];
static const char str_PyMethod_SScorer_PreloadTexture[];
extern const char str_PyMethod_SScorer_Set[];
static const char str_PyMethod_SScorer_SetCinema[];
static const char str_PyMethod_SScorer_SetConsole[];
static const char str_PyMethod_SScorer_SetCursor[];
static const char str_PyMethod_SScorer_SetDefault[];
static const char str_PyMethod_SScorer_SetHeadMonitor[];
static const char str_PyMethod_SScorer_SetLabelText[];
static const char str_PyMethod_SScorer_SetMPFunc[];
static const char str_PyMethod_SScorer_SetMarkerSprite[];
static const char str_PyMethod_SScorer_SetMsgText[];
static const char str_PyMethod_SScorer_SetOnCancel[];
static const char str_PyMethod_SScorer_SetOnNext[];
static const char str_PyMethod_SScorer_SetOnPrev[];
static const char str_PyMethod_SScorer_SetOnSpecialHint[];
static const char str_PyMethod_SScorer_SetSpeechCallback[];
static const char str_PyMethod_SScorer_SetSpeechText[];
static const char str_PyMethod_SScorer_Show[];
static PyMethodDef SScorer_methods[34] = {
	{
		.ml_name = (void *)str_PyMethod_SScorer_Add,
		.ml_meth = (void *)SScorer_Add,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_Add,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_Clear,
		.ml_meth = (void *)SScorer_Clear,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_Clear,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_Set,
		.ml_meth = (void *)SScorer_Set,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_Set,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_Get,
		.ml_meth = (void *)SScorer_Get,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_Get,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_Show,
		.ml_meth = (void *)SScorer_Show,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_Show,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_Hide,
		.ml_meth = (void *)SScorer_Hide,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_Hide,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetCursor,
		.ml_meth = (void *)SScorer_SetCursor,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetCursor,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetDefault,
		.ml_meth = (void *)SScorer_SetDefault,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetDefault,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_GetDefault,
		.ml_meth = (void *)SScorer_GetDefault,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_GetDefault,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_GetCurrent,
		.ml_meth = (void *)SScorer_GetCurrent,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_GetCurrent,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetOnCancel,
		.ml_meth = (void *)SScorer_SetOnCancel,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetOnCancel,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetOnNext,
		.ml_meth = (void *)SScorer_SetOnNext,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetOnNext,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetOnPrev,
		.ml_meth = (void *)SScorer_SetOnPrev,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetOnPrev,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetOnSpecialHint,
		.ml_meth = (void *)SScorer_SetOnSpecialHint,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetOnSpecialHint,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_AddModel,
		.ml_meth = (void *)SScorer_AddModel,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_AddModel,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetMPFunc,
		.ml_meth = (void *)SScorer_SetMPFunc,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetMPFunc,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_PreloadTexture,
		.ml_meth = (void *)SScorer_PreloadTexture,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_PreloadTexture,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_AddChatMsg,
		.ml_meth = (void *)SScorer_AddChatMsg,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_AddChatMsg,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetConsole,
		.ml_meth = (void *)SScorer_SetConsole,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetConsole,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetSpeechText,
		.ml_meth = (void *)SScorer_SetSpeechText,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetSpeechText,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetSpeechCallback,
		.ml_meth = (void *)SScorer_SetSpeechCallback,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetSpeechCallback,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_CancelSpeech,
		.ml_meth = (void *)SScorer_CancelSpeech,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_CancelSpeech,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetHeadMonitor,
		.ml_meth = (void *)SScorer_SetHeadMonitor,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetHeadMonitor,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_Fade,
		.ml_meth = (void *)SScorer_Fade,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_Fade,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetCinema,
		.ml_meth = (void *)SScorer_SetCinema,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetCinema,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetMsgText,
		.ml_meth = (void *)SScorer_SetMsgText,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetMsgText,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetLabelText,
		.ml_meth = (void *)SScorer_SetLabelText,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetLabelText,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_Get2DPos,
		.ml_meth = (void *)SScorer_Get2DPos,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_Get2DPos,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_GetTextArea,
		.ml_meth = (void *)SScorer_GetTextArea,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_GetTextArea,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_GetMenuAccept,
		.ml_meth = (void *)SScorer_GetMenuAccept,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_GetMenuAccept,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_SetMarkerSprite,
		.ml_meth = (void *)SScorer_SetMarkerSprite,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_SetMarkerSprite,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_GetItem,
		.ml_meth = (void *)SScorer_GetItem,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_GetItem,
	},
	{
		.ml_name = (void *)str_PyMethod_SScorer_New,
		.ml_meth = (void *)SScorer_New,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SScorer_New,
	},
};

SD_PY_MODULE(SScorer, 0x42ec68)

static const char str_PyMethod_SScorer_Hide[] = "Hide";
const char str_PyMethod_SScorer_Add[] = "Add";
static const char str_PyMethod_SScorer_Clear[] = "Clear";
static const char str_PyMethod_SScorer_Show[] = "Show";
static const char str_PyMethod_SScorer_SetCursor[] = "SetCursor";
static const char str_PyMethod_SScorer_SetDefault[] = "SetDefault";
static const char str_PyMethod_SScorer_GetDefault[] = "GetDefault";
static const char str_PyMethod_SScorer_GetCurrent[] = "GetActual";
static const char str_PyMethod_SScorer_SetOnCancel[] = "SetOnCancel";
static const char str_PyMethod_SScorer_SetOnNext[] = "SetOnNext";
static const char str_PyMethod_SScorer_SetOnPrev[] = "SetOnPrev";
static const char str_PyMethod_SScorer_SetOnSpecialHint[] = "SetOnSpecialHint";
static const char str_PyMethod_SScorer_AddModel[] = "AddModel";
static const char str_PyMethod_SScorer_SetMPFunc[] = "SetMPFunc";
static const char str_PyMethod_SScorer_PreloadTexture[] = "PreloadTexture";
static const char str_PyMethod_SScorer_AddChatMsg[] = "AddChatMsg";
static const char str_PyMethod_SScorer_SetConsole[] = "SetConsole";
static const char str_PyMethod_SScorer_SetSpeechText[] = "SetSpeechText";
static const char str_PyMethod_SScorer_SetSpeechCallback[] = "SetSpeechCallback";
static const char str_PyMethod_SScorer_CancelSpeech[] = "CancelSpeech";
static const char str_PyMethod_SScorer_SetHeadMonitor[] = "SetHeadMonitor";
static const char str_PyMethod_SScorer_Fade[] = "Fade";
static const char str_PyMethod_SScorer_SetCinema[] = "SetCinema";
static const char str_PyMethod_SScorer_SetMsgText[] = "SetMsgText";
static const char str_PyMethod_SScorer_SetLabelText[] = "SetLabelText";
static const char str_PyMethod_SScorer_Get2DPos[] = "Get2DPos";
static const char str_PyMethod_SScorer_GetTextArea[] = "GetTextArea";
static const char str_PyMethod_SScorer_GetMenuAccept[] = "GetMenuAccept";
static const char str_PyMethod_SScorer_SetMarkerSprite[] = "SetMarkerSprite";
static const char str_PyMethod_SScorer_GetItem[] = "GetItem";
static const char str_PyMethod_SScorer_New[] = "New";

static char str_PyDoc_SScorer_PreloadTexture[] = "PreloadTexture(filename) : Precarga una textura";

static char str_PyDoc_SScorer_AddChatMsg[80] =
	"AddChatMsg(string,red,gree,blue) : Agrega un mensaje a la lista de chat.";

static char str_PyDoc_SScorer_SetConsole[72] =
	"SetConsole(show) : oculta/ muestra la consola de pantalla completa";

static char str_PyDoc_SScorer_SetSpeechText[] =
	"SetSpeechText(text,time,r,g,b) : activa el texto de una conversacion que durará un tiempo espeficico";

static char str_PyDoc_SScorer_SetSpeechCallback[] =
	"SetSpeechCallback(Callback) : Especifica la funcion callback que será llamada";

static char str_PyDoc_SScorer_CancelSpeech[] = "CancelSpeech(time) : cancela un mensaje remoto";

static char str_PyDoc_SScorer_SetHeadMonitor[] =
	"SetHeadMonitor(Head,Msg,anm) : Especifica la cabeza que será usada en el monitor y el mensaje.";

static char str_PyDoc_SScorer_Fade[] = "Fade(id,(r,g,b,a),(r,g,b,a),time) : Realiza un fade de rgba a rgba.";

static char str_PyDoc_SScorer_SetCinema[] = "SetCinema(id,status,time) : Activa el modo escena de cine.";

static char str_PyDoc_SScorer_SetMsgText[] =
	"SetMsgText(text,time) : Muestra un mensaje de sistema y lo desactiva en el tiempo de mundo time";

static char str_PyDoc_SScorer_SetLabelText[104] =
	"SetLabelText(text,time) : Muestra un rotulo de sistema y lo desactiva en el tiempo de mundo time";

static char str_PyDoc_SScorer_Get2DPos[] =
	"(x,y) Get2DPos(vector3d) : obtiene en cordenadas de pantalla una posicion del escenario";

static char str_PyDoc_SScorer_GetTextArea[128] =
	"(width, height) GetTextArea(fontType, text) : obtiene la anchura y la altura de un texto dado el tex"
	"to y el tipo de fuente";

static char str_PyDoc_SScorer_GetMenuAccept[] =
	"GetMenuAccept(id) : Devuelve el estado de la acción de menu aceptar.";

static char str_PyDoc_SScorer_SetMarkerSprite[] =
	"SetMarkerSprite(id,SpriteName) : pone un sprite como marcador";

static char str_PyDoc_SScorer_Add[] = "Add(playernumber,itemname,itemtype,AtEnd) : Agrega un item al scorer.";

static char str_PyDoc_SScorer_Clear[] = "Clear(playernumber) : Limpia el scorer completamente.";

static char str_PyDoc_SScorer_Set[96] =
	"Set(playernumber,itemname,varname,value) :  Modifica el valor de una variable de un item";

static char str_PyDoc_SScorer_Get[] =
	"Get(playernumber,itemname,varname) :  Obtiene el valor de una variable de un item";

static char str_PyDoc_SScorer_GetItem[] = "GetItem(playernumber,itemname) :  Obtiene un objeto item";

static char str_PyDoc_SScorer_New[] =
	"New(playernumber,itemname,itemtype,AtEnd) : Agrega un item al scorer y obtiene la entidad.";

static char str_PyDoc_SScorer_Show[] = "Show(playernumber) : Activa el scorer.";

static char str_PyDoc_SScorer_Hide[] = "Hide(playernumber) : Desactiva el scorer.";

static char str_PyDoc_SScorer_SetCursor[80] =
	"SetCursor(playernumber,CursorName) : pone un item del scorer como cursor";

static char str_PyDoc_SScorer_SetDefault[88] =
	"SetDefault(playernumber,DefaultItemName) : pone un item del scorer 'por defecto'";

static char str_PyDoc_SScorer_GetDefault[] =
	"DefaultItemName GetDefault(playernumber) Obtiene el nombre del item por defecto";

static char str_PyDoc_SScorer_GetCurrent[64] = "ActualItem GetActual(playernumber) Obtiene el item actual";

static char str_PyDoc_SScorer_SetOnCancel[104] =
	"SetOnCancel(playernumber,CancelEvent) : Define la funcion que se ejecutara en el caso del escape.";

static char str_PyDoc_SScorer_SetOnNext[] =
	"SetOnNext(playernumber,CancelEvent) : Define la funcion que se ejecutara en el caso del pagina abajo"
	".";

static char str_PyDoc_SScorer_SetOnPrev[] =
	"SetOnPrev(playernumber,CancelEvent) : Define la funcion que se ejecutara en el caso del pagina abajo"
	".";

static char str_PyDoc_SScorer_SetOnSpecialHint[] =
	"SetOnSpecialHint(playernumber,SpecialHintTestFunc,SpecialHint) : Define la funcion que decide si se "
	"muestra un hint especial";

static char str_PyDoc_SScorer_AddModel[] =
	"AddModel(playernumber,Name,ModelFile,numanim,Radius) : Pre carga un modelo para usarse en el scorer.";

static char str_PyDoc_SScorer_SetMPFunc[104] =
	"SetMPFunc(playernumber,Callback) : Callback(id,showmpscorer) (muestra oculta el scorer multiplayer)";

