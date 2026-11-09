/*
 * Python module SScorer (method table SScorer_methods): HUD, menus, chat and speech texts of the
 * local players. The original source file is not known (no CHECK in it). Most functions take the player number
 * first and give None when that player has no scorer.
 */
#include <string.h>

#include "scrap/input.h"
#include "scrap/scorer.h"
#include "scrap/scriptapi.h"

/* AddChatMsg(string,red,green,blue) : Agrega un mensaje a la list de chat. */
API_FN(SScorer, AddChatMsg) {
	const char *text;
	int r, g, b;
	if (!Mode_Parse(a, n, "siii", &text, &r, &g, &b)) {
		return 0;
	}
	Scorer_AddChatMsg(text, ((uint32_t)r << 8 | (uint32_t)g) << 8 | (uint32_t)b);
	return 1;
}

/* SetConsole(on) */
API_FN(SScorer, SetConsole) {
	int on;
	if (!Mode_Parse(a, n, "i", &on)) {
		return 0;
	}
	Scorer_SetConsole(on != 0);
	return 1;
}

/* SetSpeechText(text, time, a, b, c) */
API_FN(SScorer, SetSpeechText) {
	const char *text;
	double time;
	int ia, ib, ic;
	if (!Mode_Parse(a, n, "sdiii", &text, &time, &ia, &ib, &ic)) {
		return 0;
	}
	Scorer_SetSpeechText(text, (float)time, ia, ib, ic);
	return 1;
}

/* SetSpeechCallback(func[, arg]) : the list arg is kept as given (its items the callback's arguments) */
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
MODE_PYENTRY(SScorer_SetSpeechCallback)

/* CancelSpeech() */
API_FN(SScorer, CancelSpeech) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Scorer_CancelSpeech(0);
	return 1;
}

/* SetHeadMonitor(a, b, c) */
API_FN(SScorer, SetHeadMonitor) {
	const char *sa, *sb;
	int c;
	if (!Mode_Parse(a, n, "ssi", &sa, &sb, &c)) {
		return 0;
	}
	Scorer_SetHeadMonitor(sa, sb, c);
	return 1;
}

/* Fade(player, (r,g,b,a) from, (r,g,b,a) to, time) */
API_FN(SScorer, Fade) {
	int player;
	int r1, g1, b1, a1, r2, g2, b2, a2;
	float time;
	if (!Mode_Parse(a, n, "i(iiii)(iiii)f", &player, &r1, &g1, &b1, &a1, &r2, &g2, &b2, &a2, &time)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc) {
		ScorerFade_Start(&sc->fade, Scorer_PackColor((uint32_t)r1, (uint32_t)g1, (uint32_t)b1, (uint32_t)a1),
			Scorer_PackColor((uint32_t)r2, (uint32_t)g2, (uint32_t)b2, (uint32_t)a2), time);
	}
	return 1;
}

/* SetCinema(player, on, time) : cinema bars */
API_FN(SScorer, SetCinema) {
	int player, on;
	float time;
	if (!Mode_Parse(a, n, "iif", &player, &on, &time)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc) {
		ScorerCinema_Set(&sc->cinema, on != 0, time);
	}
	return 1;
}

/* SetMsgText(text, time) : in the font "Horatio" */
API_FN(SScorer, SetMsgText) {
	const char *text;
	double time;
	if (!Mode_Parse(a, n, "sd", &text, &time)) {
		return 0;
	}
	Scorer_SetMsgText(text, (float)time, "Horatio");
	return 1;
}

/* SetLabelText(text, time) */
API_FN(SScorer, SetLabelText) {
	const char *text;
	double time;
	if (!Mode_Parse(a, n, "sd", &text, &time)) {
		return 0;
	}
	Scorer_SetLabelText(text, (float)time);
	return 1;
}

/* Get2DPos((x,y,z)) : screen position of a world point */
API_FN(SScorer, Get2DPos) {
	vec3_t pos;
	float xy[2];
	if (!Mode_Parse(a, n, "(fff)", &pos.x, &pos.y, &pos.z)) {
		return 0;
	}
	Engine_WorldToScreen(&pos, xy, 0);
	return Api_Build(out, "ff", (double)xy[0], (double)xy[1]);
}

/* (width, height) GetTextArea(fontType, text) */
API_FN(SScorer, GetTextArea) {
	const char *fontType, *text;
	float rect[2], size[2];
	if (!Mode_Parse(a, n, "ss", &fontType, &text)) {
		return 0;
	}
	Font_TextSize(Font_Get(fontType), rect, size, 0.0f, 0.0f, "%s", text);
	return Api_Build(out, "ff", (double)size[0], (double)size[1]);
}

/* GetMenuAccept(player) : 1 while the accept action (action 10 of the first action set) is held */
API_FN(SScorer, GetMenuAccept) {
	int player;
	if (!Mode_Parse(a, n, "i", &player)) {
		return 0;
	}
	if (!Scorer_Get(player)) {
		return 1;
	}
	return Api_Build(out, "i", Input_MenuAccept(player));
}

/* SetMarkerSprite(player, sprite) : "" for none */
API_FN(SScorer, SetMarkerSprite) {
	int player;
	const char *sprite;
	if (!Mode_Parse(a, n, "is", &player, &sprite)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return 1;
	}
	cScorer_SetMarkerSprite(sc, sprite && *sprite ? sprite : NULL);
	return Api_Build(out, "i", 1);
}

/* Add(player, name, type[, flag]) : 1 if the item was made */
API_FN(SScorer, Add) {
	int player, flag = 0;
	const char *name, *type;
	if (!Mode_Parse(a, n, "iss|i", &player, &name, &type, &flag)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return 1;
	}
	return Api_Build(out, "i", cScorer_AddItem(sc, name, type, flag != 0) != NULL);
}

/* Clear(player) */
API_FN(SScorer, Clear) {
	int player;
	if (!Mode_Parse(a, n, "i", &player)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return 1;
	}
	cScorer_Clear(sc);
	return Api_Build(out, "i", 1);
}

/*
 * Set(player, item, var, value) : value as string ("isss") or number ("issd", stored as double and truncated
 * int). The number form is only tried when the string form does not parse (Native_ScorerStore).
 */
API_FN(SScorer, Set) {
	int player, r;
	const char *name, *var;
	const nativeVal_t *v;
	if (!Mode_Parse(a, n, "issO", &player, &name, &var, &v)) {
		return 0;
	}
	switch (Native_ScorerStore(player, name, var, v, &r)) {
	case 1:
		return Api_Build(out, "i", r);
	case 0:
		return 1;
	}
	return 0;
}

/* Get(player, item, var) : value in its type, None if not found */
int Scorer_GetItemVar(int player, const char *name, const char *var, char **s, double *d, int *i) {
	/* differs from the original: the values start at 0. Some variables read the int on a get too (ForceWidth sets
	 * the width of a button from it, HighRes and BlendingMode of a tab check it), which was stack garbage. */
	*s = NULL;
	*d = 0.0;
	*i = 0;
	cScorer *sc = Scorer_Get(player);
	cScorerItem *item = sc ? cScorer_FindItem(sc, name) : NULL;
	int kind = item ? ScorerItem_Var(item, var, 0, s, d, i) : 0;
	return kind >= 1 && kind <= 3 ? kind : 0;
}

API_FN(SScorer, Get) {
	int player, i;
	const char *name, *var;
	char *s;
	double d;
	if (!Mode_Parse(a, n, "iss", &player, &name, &var)) {
		return 0;
	}
	switch (Scorer_GetItemVar(player, name, var, &s, &d, &i)) {
	case 1:
		return Api_Build(out, "s", s);
	case 2:
		return Api_Build(out, "d", d);
	case 3:
		return Api_Build(out, "i", i);
	}
	return 1;
}

/* differs from the original (ScrapC only): Items(player) : the names of the items of the scorer in draw order, for
 * tools that inspect a menu (tools/mcp); None without a scorer */
API_FN(SScorer, Items) {
	int player;
	if (!Mode_Parse(a, n, "i", &player)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return 1;
	}
	Native_SetEmpty(out, NV_LIST);
	for (cScorerItem *it = sc->items; it; it = it->next) {
		if (!it->delMe) {
			Native_SetStr(Native_Append(out), it->name ? it->name : "");
		}
	}
	return 1;
}

byte Scorer_AcceptItem(int player, const char *name) {
	cScorer *sc = Scorer_Get(player);
	cScorerItem *it = sc ? cScorer_FindItem(sc, name) : NULL;
	if (!it || !cPy_Function_IsValid(&it->onAccept)) {
		return 0;
	}
	cPy_Function_AddParamI(&it->onAccept, player);
	cPy_Function_AddParamS(&it->onAccept, name);
	cPy_Function_Execute(&it->onAccept, 0);
	return 1;
}

/* differs from the original (ScrapC only): Accept(player, item) : runs the OnAccept of the item with the player and
 * the item name, as the accept key of the menu does; 1 if it has one */
API_FN(SScorer, Accept) {
	int player;
	const char *name;
	return Mode_Parse(a, n, "is", &player, &name) && Api_Build(out, "i", Scorer_AcceptItem(player, name));
}

/* GetItem(player, item) : the Python object of the item (an NV_OBJ, OBJLIB 7), None if not found */
API_FN(SScorer, GetItem) {
	int player;
	const char *name;
	if (!Mode_Parse(a, n, "is", &player, &name)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc && cScorer_FindItem(sc, name)) {
		return Api_Object(ScorerItem_PyNew(player, name), out);
	}
	return 1;
}

/*
 * New(player, name, type[, flag]) : makes the item and returns its Python object. The original returns NULL without
 * an exception when that fails (SystemError in Python). An unknown type still does so: tools/test/golden records it.
 * differs from the original: a player without a scorer and a duplicated name raise a ValueError.
 */
API_FN(SScorer, New) {
	int player, flag = 0;
	const char *name, *type;
	if (!Mode_Parse(a, n, "iss|i", &player, &name, &type, &flag)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return Native_Raise(NE_VALUE, "SScorer.New: no scorer for player %d", player);
	}
	if (cScorer_AddItem(sc, name, type, flag != 0)) {
		return Api_Object(ScorerItem_PyNew(player, name), out);
	}
	if (cScorer_FindItem(sc, name)) {
		return Native_Raise(NE_VALUE, "SScorer.New: item '%.128s' already exists", name);
	}
	return 0;
}

/* Show(playernumber) : Activa el scorer. */
API_FN(SScorer, Show) {
	int player;
	if (!Mode_Parse(a, n, "i", &player)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return 1;
	}
	sc->visible = 1;
	return Api_Build(out, "i", 1);
}

/* Hide(playernumber) */
API_FN(SScorer, Hide) {
	int player;
	if (!Mode_Parse(a, n, "i", &player)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return 1;
	}
	sc->visible = 0;
	return Api_Build(out, "i", 1);
}

/* SetCursor(playernumber,CursorName) : pone un item del scorer como cursor (NULL if not found) */
API_FN(SScorer, SetCursor) {
	int player;
	const char *name;
	if (!Mode_Parse(a, n, "is", &player, &name)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return 1;
	}
	sc->cursor = cScorer_FindItem(sc, name);
	return Api_Build(out, "i", 1);
}

/* SetDefault(playernumber, ItemName) */
API_FN(SScorer, SetDefault) {
	int player;
	const char *name;
	if (!Mode_Parse(a, n, "is", &player, &name)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return 1;
	}
	cScorer_SetDefault(sc, name);
	return Api_Build(out, "i", 1);
}

/* DefaultItemName GetDefault(playernumber) */
API_FN(SScorer, GetDefault) {
	int player;
	if (!Mode_Parse(a, n, "i", &player)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc && sc->defaultItem) {
		return Api_Build(out, "s", sc->defaultItem);
	}
	return 1;
}

/* GetActual(playernumber) */
API_FN(SScorer, GetCurrent) {
	int player;
	if (!Mode_Parse(a, n, "i", &player)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc && sc->current) {
		return Api_Build(out, "s", sc->current);
	}
	return 1;
}

/* SetOnCancel(playernumber, CancelEvent) : returns 1 even without a scorer */
API_FN(SScorer, SetOnCancel) {
	int player;
	const char *func;
	if (!Mode_Parse(a, n, "is", &player, &func)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc) {
		cPy_Function_SetFunc(&sc->onCancel, func);
	}
	return Api_Build(out, "i", 1);
}

/* SetOnNext(playernumber, NextEvent) */
API_FN(SScorer, SetOnNext) {
	int player;
	const char *func;
	if (!Mode_Parse(a, n, "is", &player, &func)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return 1;
	}
	cPy_Function_SetFunc(&sc->onNext, func);
	return Api_Build(out, "i", 1);
}

/* SetOnPrev(playernumber, PrevEvent) */
API_FN(SScorer, SetOnPrev) {
	int player;
	const char *func;
	if (!Mode_Parse(a, n, "is", &player, &func)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return 1;
	}
	cPy_Function_SetFunc(&sc->onPrev, func);
	return Api_Build(out, "i", 1);
}

/* SetOnSpecialHint(playernumber, SpecialHintTestFunc, SpecialHint) */
API_FN(SScorer, SetOnSpecialHint) {
	int player;
	const char *func, *hint;
	if (!Mode_Parse(a, n, "iss", &player, &func, &hint)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (!sc) {
		return 1;
	}
	cPy_Function_SetFunc(&sc->onSpecialHint, func);
	cString_Set(&sc->specialHint, hint);
	return Api_Build(out, "i", 1);
}

/* AddModel(playernumber, Name, ModelFile, anim, numanim, Radius[, c[, flag]]) : anim "" for none */
API_FN(SScorer, AddModel) {
	int player, numAnim, flag = 0;
	const char *name, *model, *anim;
	float radius, c = 0.0f;
	if (!Mode_Parse(a, n, "isssif|fi", &player, &name, &model, &anim, &numAnim, &radius, &c, &flag)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc && cScorer_AddModel(sc, name, model, *anim ? anim : NULL, numAnim, radius, c, flag > 0)) {
		return Api_Build(out, "i", 1);
	}
	return 1;
}

/* SetMPFunc(playernumber, func) : always None */
API_FN(SScorer, SetMPFunc) {
	int player;
	const char *func;
	if (!Mode_Parse(a, n, "is", &player, &func)) {
		return 0;
	}
	cScorer *sc = Scorer_Get(player);
	if (sc) {
		cPy_Function_SetFunc(&sc->mpFunc, func);
	}
	return 1;
}

/* PreloadTexture(name[, flag]) */
API_FN(SScorer, PreloadTexture) {
	const char *name;
	int flag = 0;
	if (!Mode_Parse(a, n, "s|i", &name, &flag)) {
		return 0;
	}
	Scorer_PreloadTexture(name, flag > 0);
	return 1;
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
static PyMethodDef SScorer_methods[36] = {
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
	{
		.ml_name = "Items",
		.ml_meth = (void *)SScorer_Items,
		.ml_flags = 1,
		.ml_doc = "Items(playernumber) : the item names in draw order (ScrapC)",
	},
	{
		.ml_name = "Accept",
		.ml_meth = (void *)SScorer_Accept,
		.ml_flags = 1,
		.ml_doc = "Accept(playernumber,ItemName) : runs the OnAccept of the item (ScrapC)",
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

PORTS(PyInterface_SScorerPy)
