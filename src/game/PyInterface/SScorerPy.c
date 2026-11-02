/*
 * Python module SScorer (method table SScorer_methods, rva 0x42ec68): HUD, menus, chat and speech texts of the
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
PyObject *SScorer_AddChatMsg(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetConsole(PyObject *self, PyObject *args) {
	int on;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &on)) {
		return NULL;
	}
	Scorer_SetConsole(on != 0);
	return Py_ReturnNone();
}

/* SetSpeechText(text, time, a, b, c) */
PyObject *SScorer_SetSpeechText(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetSpeechCallback(PyObject *self, PyObject *args) {
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
PyObject *SScorer_CancelSpeech(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Scorer_CancelSpeech(0);
	return Py_ReturnNone();
}

/* SetHeadMonitor(a, b, c) */
PyObject *SScorer_SetHeadMonitor(PyObject *self, PyObject *args) {
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
PyObject *SScorer_Fade(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetCinema(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetMsgText(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetLabelText(PyObject *self, PyObject *args) {
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
PyObject *SScorer_Get2DPos(PyObject *self, PyObject *args) {
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
PyObject *SScorer_GetTextArea(PyObject *self, PyObject *args) {
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
PyObject *SScorer_GetMenuAccept(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetMarkerSprite(PyObject *self, PyObject *args) {
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
PyObject *SScorer_Add(PyObject *self, PyObject *args) {
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
PyObject *SScorer_Clear(PyObject *self, PyObject *args) {
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
PyObject *SScorer_Set(PyObject *self, PyObject *args) {
	int player, i;
	char *name, *var, *s;
	double d;
	cScorer *sc;
	cScorerItem *item;

	(void)self;
	if (PyArg_ParseTuple(args, "isss", &player, &name, &var, &s)) {
		if ((sc = Scorer_Get(player)) && (item = cScorer_FindItem(sc, name))) {
			return Py_BuildValue("i", item->vftable->Var(item, var, 1, &s, NULL, NULL));
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
		return Py_BuildValue("i", item->vftable->Var(item, var, 1, NULL, &d, &i));
	}
	return Py_ReturnNone();
}

/* Get(player, item, var) : value in its type, None if not found */
PyObject *SScorer_Get(PyObject *self, PyObject *args) {
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
		switch (item->vftable->Var(item, var, 0, &s, &d, &i)) {
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
PyObject *SScorer_GetItem(PyObject *self, PyObject *args) {
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
PyObject *SScorer_New(PyObject *self, PyObject *args) {
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
PyObject *SScorer_Show(PyObject *self, PyObject *args) {
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
PyObject *SScorer_Hide(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetCursor(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetDefault(PyObject *self, PyObject *args) {
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
PyObject *SScorer_GetDefault(PyObject *self, PyObject *args) {
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
PyObject *SScorer_GetCurrent(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetOnCancel(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetOnNext(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetOnPrev(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetOnSpecialHint(PyObject *self, PyObject *args) {
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
PyObject *SScorer_AddModel(PyObject *self, PyObject *args) {
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
PyObject *SScorer_SetMPFunc(PyObject *self, PyObject *args) {
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
PyObject *SScorer_PreloadTexture(PyObject *self, PyObject *args) {
	char *name;
	int flag = 0;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|i", &name, &flag)) {
		return NULL;
	}
	Scorer_PreloadTexture(name, flag > 0);
	return Py_ReturnNone();
}

extern PyMethodDef SScorer_methods[34];
SD_PY_MODULE(SScorer, 0x42ec68)
