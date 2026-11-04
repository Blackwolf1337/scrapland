/*
 * Python module SInput (method table SInput_methods): action sets, key definitions, text input
 * and pads of the local players. Original source: PyInterface/SInputPy.cpp. Player numbers are checked unsigned
 * (0..3); out of range they give 1 or 0 depending on the function, as the original.
 */
#include "scrap/global.h"
#include "scrap/input.h"
#include "scrap/python.h"

/* SetActionSet(name) : selects the action set of that name; looks at the first four entries */
static PyObject *SInput_SetActionSet(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	for (int i = 0; i < 4; i++) {
		if (ActionSet_IsNamed(TABLE_AT(g_ActionSets, i), name)) {
			Input_SelectActionSet(i);
			return Py_BuildValue("i", 1);
		}
	}
	return Py_BuildValue("i", 0);
}

/* GetActionSet() : name of the current action set */
static PyObject *SInput_GetActionSet(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("s", TABLE_AT(g_ActionSets, Input_GetActionSet())->name);
}

/* AssingEntry(...) (sic) */
static PyObject *SInput_AssingEntry(PyObject *self, PyObject *args) {
	char *a, *b, *d, *e;
	int c;

	(void)self;
	if (!PyArg_ParseTuple(args, "ssiss", &a, &b, &c, &d, &e)) {
		return NULL;
	}
	return Py_BuildValue("i", Input_AssignEntry(a, b, c, d, e));
}

/* GetEntry(a, b, c) : (index, name); index -1 and "" when nothing was found */
static PyObject *SInput_GetEntry(PyObject *self, PyObject *args) {
	char *a, *b, *c;
	int index = -1;
	char *name = "";

	(void)self;
	if (!PyArg_ParseTuple(args, "sss", &a, &b, &c)) {
		return NULL;
	}
	int r = Input_GetEntry(a, b, c, &index, &name);
	if (r) {
		index = r - 1;
	}
	return Py_BuildValue("is", index, name);
}

/* ListenToDefine(player, a, b, c) : waits for the next key to define it */
static PyObject *SInput_ListenToDefine(PyObject *self, PyObject *args) {
	int player;
	char *a, *b, *c;

	(void)self;
	if (!PyArg_ParseTuple(args, "isss", &player, &a, &b, &c)) {
		return NULL;
	}
	return Py_BuildValue("i", Input_ListenToDefine(player, a, b, c));
}

/* AbortListenToDefine() */
static PyObject *SInput_AbortListenToDefine(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Input_AbortListenToDefine();
	Py_INCREF(Py_None);
	return Py_None;
}

/* GetDefinedEntry(player, a, b, c[, flag]) */
static PyObject *SInput_GetDefinedEntry(PyObject *self, PyObject *args) {
	int player, flag = 0;
	char *a, *b, *c;

	(void)self;
	if (!PyArg_ParseTuple(args, "isss|i", &player, &a, &b, &c, &flag)) {
		return NULL;
	}
	return Py_BuildValue("s", Input_GetDefinedEntry(player, a, b, c, flag != 0));
}

/* GetDefinedList(player, a, b[, flag]) : the list as a string, or the error number */
static PyObject *SInput_GetDefinedList(PyObject *self, PyObject *args) {
	int player, flag = 0;
	char *a, *b;
	char buf[0x800];

	(void)self;
	if (!PyArg_ParseTuple(args, "iss|i", &player, &a, &b, &flag)) {
		return NULL;
	}
	int r = Input_GetDefinedList(player, a, b, buf, sizeof(buf), flag != 0);
	if (r) {
		return Py_BuildValue("i", r);
	}
	return Py_BuildValue("s", buf);
}

/* ClearDefinedList(player, a, b[, flag]) */
static PyObject *SInput_ClearDefinedList(PyObject *self, PyObject *args) {
	int player, flag = 0;
	char *a, *b;

	(void)self;
	if (!PyArg_ParseTuple(args, "iss|i", &player, &a, &b, &flag)) {
		return NULL;
	}
	return Py_BuildValue("i", Input_ClearDefinedList(player, a, b, flag != 0));
}

/* ResetToDefault() */
static PyObject *SInput_ResetToDefault(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Input_ResetToDefault();
	Py_INCREF(Py_None);
	return Py_None;
}

/* SetString(player, text) : 0 if set, 1 for a bad player */
static PyObject *SInput_SetString(PyObject *self, PyObject *args) {
	uint32_t player;
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &player, &text)) {
		return NULL;
	}
	if (player >= MAX_LOCAL_PLAYERS) {
		return Py_BuildValue("i", 1);
	}
	cPlayerInput_SetString(&g_PlayerInput[player], text);
	return Py_BuildValue("i", 0);
}

/* GetString(player) : the text input of the player, 1 for a bad player */
static PyObject *SInput_GetString(PyObject *self, PyObject *args) {
	uint32_t player;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &player)) {
		return NULL;
	}
	if (player >= MAX_LOCAL_PLAYERS) {
		return Py_BuildValue("i", 1);
	}
	return Py_BuildValue("s", g_PlayerInput[player].text);
}

/* GetCursorChar(player) : (x, y, char) of the text cursor, 1 for a bad player */
static PyObject *SInput_GetCursorChar(PyObject *self, PyObject *args) {
	uint32_t player;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &player)) {
		return NULL;
	}
	if (player >= MAX_LOCAL_PLAYERS) {
		return Py_BuildValue("i", 1);
	}
	char c[2] = { cPlayerInput_GetCursorChar(&g_PlayerInput[player]), 0 };
	return Py_BuildValue("ffs", (double)g_PlayerInput[player].cursorX, (double)g_PlayerInput[player].cursorY, c);
}

/* GetVirtualKeyboard() : (a, b, text) */
static PyObject *SInput_GetVirtualKeyboard(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("iis", g_VirtualKeyboard.a, g_VirtualKeyboard.b, g_VirtualKeyboard.text);
}

/* SetVirtualKeyboard(text) */
static PyObject *SInput_SetVirtualKeyboard(PyObject *self, PyObject *args) {
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	return Py_BuildValue("i", cVirtualKeyboard_Set(&g_VirtualKeyboard, text));
}

/* SetInputFunc(player, func) : 0 if set, 1 for a bad player */
static PyObject *SInput_SetInputFunc(PyObject *self, PyObject *args) {
	uint32_t player;
	char *func;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &player, &func)) {
		return NULL;
	}
	if (player >= MAX_LOCAL_PLAYERS) {
		return Py_BuildValue("i", 1);
	}
	cPlayerInput_SetInputFunc(&g_PlayerInput[player], func);
	return Py_BuildValue("i", 0);
}

/* Bind(player, a, b, c) */
static PyObject *SInput_Bind(PyObject *self, PyObject *args) {
	int player;
	char *a, *b, *c;

	(void)self;
	if (!PyArg_ParseTuple(args, "isss", &player, &a, &b, &c)) {
		return NULL;
	}
	return Py_BuildValue("i", Input_Bind(player, a, b, c));
}

/* Rumble(pad, a, b, c) : 1 if done, 0 for a bad pad */
static PyObject *SInput_Rumble(PyObject *self, PyObject *args) {
	uint32_t pad;
	float a, b, c;

	(void)self;
	if (!PyArg_ParseTuple(args, "ifff", &pad, &a, &b, &c)) {
		return NULL;
	}
	if (pad >= MAX_LOCAL_PLAYERS) {
		return Py_BuildValue("i", 0);
	}
	cPad_Rumble(&g_Pads[pad], a, b, c);
	return Py_BuildValue("i", 1);
}

/* ResetToSplit() : does nothing on PC */
static PyObject *SInput_ResetToSplit(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

/* CheckPadButton(pad, button) : always 0 on PC, the pad number is checked */
static PyObject *SInput_CheckPadButton(PyObject *self, PyObject *args) {
	int pad = 0, button = 0;

	(void)self;
	if (!PyArg_ParseTuple(args, "ii", &pad, &button)) {
		return NULL;
	}
	CHECK(pad >= 0 && pad < 4);
	return Py_BuildValue("i", 0);
}

static char str_PyDoc_SInput_AbortListenToDefine[];
static char str_PyDoc_SInput_AssingEntry[];
static char str_PyDoc_SInput_Bind[88];
static char str_PyDoc_SInput_CheckPadButton[];
static char str_PyDoc_SInput_ClearDefinedList[];
static char str_PyDoc_SInput_GetActionSet[68];
static char str_PyDoc_SInput_GetCursorChar[];
static char str_PyDoc_SInput_GetDefinedEntry[];
static char str_PyDoc_SInput_GetDefinedList[];
static char str_PyDoc_SInput_GetEntry[];
static char str_PyDoc_SInput_GetString[];
static char str_PyDoc_SInput_GetVirtualKeyboard[88];
static char str_PyDoc_SInput_ListenToDefine[];
static char str_PyDoc_SInput_ResetToDefault[112];
static char str_PyDoc_SInput_ResetToSplit[96];
static char str_PyDoc_SInput_Rumble[72];
static char str_PyDoc_SInput_SetActionSet[];
static char str_PyDoc_SInput_SetInputFunc[];
static char str_PyDoc_SInput_SetString[];
static char str_PyDoc_SInput_SetVirtualKeyboard[];
static const char str_PyMethod_SInput_AbortListenToDefine[];
static const char str_PyMethod_SInput_AssingEntry[];
static const char str_PyMethod_SInput_Bind[];
static const char str_PyMethod_SInput_CheckPadButton[];
static const char str_PyMethod_SInput_ClearDefinedList[];
static const char str_PyMethod_SInput_GetActionSet[];
static const char str_PyMethod_SInput_GetCursorChar[];
static const char str_PyMethod_SInput_GetDefinedEntry[];
static const char str_PyMethod_SInput_GetDefinedList[];
static const char str_PyMethod_SInput_GetEntry[];
static const char str_PyMethod_SInput_GetString[];
static const char str_PyMethod_SInput_GetVirtualKeyboard[];
static const char str_PyMethod_SInput_ListenToDefine[];
static const char str_PyMethod_SInput_ResetToDefault[];
static const char str_PyMethod_SInput_ResetToSplit[];
static const char str_PyMethod_SInput_Rumble[];
static const char str_PyMethod_SInput_SetActionSet[];
static const char str_PyMethod_SInput_SetInputFunc[];
static const char str_PyMethod_SInput_SetString[];
static const char str_PyMethod_SInput_SetVirtualKeyboard[];
static PyMethodDef SInput_methods[21] = {
	{
		.ml_name = (void *)str_PyMethod_SInput_SetActionSet,
		.ml_meth = (void *)SInput_SetActionSet,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_SetActionSet,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_GetActionSet,
		.ml_meth = (void *)SInput_GetActionSet,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_GetActionSet,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_AssingEntry,
		.ml_meth = (void *)SInput_AssingEntry,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_AssingEntry,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_GetEntry,
		.ml_meth = (void *)SInput_GetEntry,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_GetEntry,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_ListenToDefine,
		.ml_meth = (void *)SInput_ListenToDefine,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_ListenToDefine,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_AbortListenToDefine,
		.ml_meth = (void *)SInput_AbortListenToDefine,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_AbortListenToDefine,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_GetDefinedList,
		.ml_meth = (void *)SInput_GetDefinedList,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_GetDefinedList,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_GetDefinedEntry,
		.ml_meth = (void *)SInput_GetDefinedEntry,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_GetDefinedEntry,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_ClearDefinedList,
		.ml_meth = (void *)SInput_ClearDefinedList,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_ClearDefinedList,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_ResetToDefault,
		.ml_meth = (void *)SInput_ResetToDefault,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_ResetToDefault,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_SetString,
		.ml_meth = (void *)SInput_SetString,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_SetString,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_GetString,
		.ml_meth = (void *)SInput_GetString,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_GetString,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_GetCursorChar,
		.ml_meth = (void *)SInput_GetCursorChar,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_GetCursorChar,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_GetVirtualKeyboard,
		.ml_meth = (void *)SInput_GetVirtualKeyboard,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_GetVirtualKeyboard,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_SetVirtualKeyboard,
		.ml_meth = (void *)SInput_SetVirtualKeyboard,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_SetVirtualKeyboard,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_SetInputFunc,
		.ml_meth = (void *)SInput_SetInputFunc,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_SetInputFunc,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_Bind,
		.ml_meth = (void *)SInput_Bind,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_Bind,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_Rumble,
		.ml_meth = (void *)SInput_Rumble,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_Rumble,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_ResetToSplit,
		.ml_meth = (void *)SInput_ResetToSplit,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_ResetToSplit,
	},
	{
		.ml_name = (void *)str_PyMethod_SInput_CheckPadButton,
		.ml_meth = (void *)SInput_CheckPadButton,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SInput_CheckPadButton,
	},
};

SD_PY_MODULE(SInput, 0x42d790)

static const char str_PyMethod_SInput_SetActionSet[] = "SetActionSet";
static const char str_PyMethod_SInput_GetActionSet[] = "GetActionSet";
static const char str_PyMethod_SInput_AssingEntry[] = "AssingEntry";
static const char str_PyMethod_SInput_GetEntry[] = "GetEntry";
static const char str_PyMethod_SInput_ListenToDefine[] = "ListenToDefine";
static const char str_PyMethod_SInput_AbortListenToDefine[] = "AbortListenToDefine";
static const char str_PyMethod_SInput_GetDefinedList[] = "GetDefinedList";
static const char str_PyMethod_SInput_GetDefinedEntry[] = "GetDefinedEntry";
static const char str_PyMethod_SInput_ClearDefinedList[] = "ClearDefinedList";
static const char str_PyMethod_SInput_ResetToDefault[] = "ResetToDefault";
static const char str_PyMethod_SInput_SetString[] = "SetString";
static const char str_PyMethod_SInput_GetString[] = "GetString";
static const char str_PyMethod_SInput_GetCursorChar[] = "GetCursorChar";
static const char str_PyMethod_SInput_GetVirtualKeyboard[] = "GetVirtualKeyboard";
static const char str_PyMethod_SInput_SetVirtualKeyboard[] = "SetVirtualKeyboard";
static const char str_PyMethod_SInput_SetInputFunc[] = "SetInputFunc";
static const char str_PyMethod_SInput_Bind[] = "Bind";
static const char str_PyMethod_SInput_Rumble[] = "Rumble";
static const char str_PyMethod_SInput_ResetToSplit[] = "ResetToSplit";
static const char str_PyMethod_SInput_CheckPadButton[] = "CheckPadButton";

static char str_PyDoc_SInput_SetActionSet[] =
	"SetActionSet(string name) :  Pone el set de acciones requerido...";

static char str_PyDoc_SInput_GetActionSet[68] =
	"string GetActionSet() :  Obtiene el set de acciones Actual...";

static char str_PyDoc_SInput_AssingEntry[] =
	"int AssingEntry(Device,Entry,Player,ActionSet,Action) :  Asigna una entrada... retorna 0 o el Nro de"
	" parametro erroneo";

static char str_PyDoc_SInput_GetEntry[] =
	"(Player,Action) GetEntry(Device,Entry,ActionSet) :  Obtiene una entrada, (0,) si vacia";

static char str_PyDoc_SInput_ListenToDefine[] =
	"ListenToDefine(iPlayer,ActionSet,Action,LaFunction) : Espera a que el usuario mueva un control y lo "
	"redefine";

static char str_PyDoc_SInput_AbortListenToDefine[] = "AbortListenToDefine() : Aborta la redefinicion en curso";

static char str_PyDoc_SInput_GetDefinedEntry[] =
	"value GetDefinedEntry(iPlayer,ActionSet,Action,Device) : obtiene una cadena con la primera definicio"
	"n del control que encuentre";

static char str_PyDoc_SInput_GetDefinedList[] =
	"GetDefinedList(iPlayer,ActionSet,Action) : obtiene una cadena con la definicion de controles";

static char str_PyDoc_SInput_ClearDefinedList[] =
	"ClearDefinedList(iPlayer,ActionSet,Action) : Elimina todas las entradas de un control.";

static char str_PyDoc_SInput_ResetToDefault[112] =
	"ResetToDefault(iPlayer,ActionSet,Action) : Pone todas las entradas de los controles a valores por de"
	"fecto.";

static char str_PyDoc_SInput_SetString[] =
	"SetString(iPlayer,String) : Modifica la cadena de entrada de texto.";

static char str_PyDoc_SInput_GetString[] = "GetString(iPlayer) : Obtiene la cadena de entrada de texto.";

static char str_PyDoc_SInput_GetCursorChar[] =
	"GetCursorChar(iPlayer) : Obtiene el (x,y,caracter) que indican el estado del cursor.";

static char str_PyDoc_SInput_GetVirtualKeyboard[88] =
	"GetVirtualKeyboard() : Obtiene el (W,H,Board) que son datos del keyboard virtual.";

static char str_PyDoc_SInput_SetVirtualKeyboard[] =
	"SetVirtualKeyboard(tipo de teclado) : Cambia el teclado virtual";

static char str_PyDoc_SInput_SetInputFunc[] =
	"SetInputFunc(iPlayer,modfunc) : agrega la funcion callback de entrada modfunc(iPlayer,string)";

static char str_PyDoc_SInput_Bind[88] =
	"Bind(iPlayer,ActionSet,Action) : obtiene una cadena con la definicion de controles";

static char str_PyDoc_SInput_Rumble[72] = "Rumble(iPlayer,Left,Right,Time) : Inicializa el rumble de un pad.";

static char str_PyDoc_SInput_ResetToSplit[96] =
	"ResetToSplit() : Resetea el sistema de entrada de datos para iniciar el modo split screen.";

static char str_PyDoc_SInput_CheckPadButton[] =
	"CheckPadButton() : Chequea el estado de un determinado botón del pad.";

