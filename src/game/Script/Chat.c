/*
 * The Chat module in C (scripts/scorer/chat.py, issue #2): the chat of a network game. init.py preloads it only
 * for network games and Init.Init calls SetChatProc, which names the callbacks of the engine (the chat key, the
 * received lines, the console, the six quick messages). The chat line itself is data/menus/chat.def.
 *
 * SendID (the receiver of each player: "ALL" or the ship "net<n>") and MyShip (the ship of this machine, skipped
 * while cycling) stay variables of the module. A line goes over the network as "<receiver>\t<text>"; the server
 * shows it and sends it on (to all, or to the one receiver).
 */
#include <limits.h>
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"

static const char chat_def[] = {
#embed "../../../data/menus/chat.def"
	, 0 };

/* the value is the text */
static byte Chat_Is(PyObject *v, const char *text) {
	return PyString_Check(v) && !strcmp(PyString_AsString(v), text);
}

/* a + b + c with the + of Python (b a C text): a new reference, NULL with the error set */
static PyObject *Chat_Cat(PyObject *a, const char *b, PyObject *c) {
	PyObject *s = PyString_FromString((char *)b);
	PyObject *ab = s ? PyNumber_Add(a, s) : NULL;
	PyObject *r = ab && c ? PyNumber_Add(ab, c) : NULL;
	Py_XDECREF(s);
	if (!c) {
		return ab;
	}
	Py_XDECREF(ab);
	return r;
}

/* SendID[id] (a new reference) */
static PyObject *Chat_SendID(int id) {
	PyObject *ids = Script_Attr("Chat", "SendID");
	PyObject *v = ids ? PySequence_GetItem(ids, id) : NULL;
	Py_XDECREF(ids);
	return v;
}

/* SendID[id] = v (v given away): 0 with the error set */
static byte Chat_SetSendID(int id, PyObject *v) {
	PyObject *ids = v ? Script_Attr("Chat", "SendID") : NULL;
	int r = ids ? PySequence_SetItem(ids, id, v) : -1;
	Py_XDECREF(ids);
	Py_XDECREF(v);
	return r == 0;
}

/* Scrap.GetNetFlags()[i]: 1 or 0, -1 with the error set */
static int Chat_NetFlag(int i) {
	PyObject *flags = Script_Ask("Scrap", "GetNetFlags", "()");
	PyObject *f = flags ? PySequence_GetItem(flags, i) : NULL;
	Py_XDECREF(flags);
	int r = f ? PyObject_IsTrue(f) : -1;
	Py_XDECREF(f);
	return r;
}

/* GetID(id): the receiver as the chat line shows it; a receiver that is no longer a ship becomes "ALL" */
static PyObject *Chat_IdText(int id) {
	for (;;) {
		PyObject *to = Chat_SendID(id);
		if (!to) {
			return NULL;
		}
		if (Chat_Is(to, "ALL")) {
			Py_DECREF(to);
			return PyString_FromString("[To All]");
		}
		PyObject *v = Script_Ask("Scrap", "GetEntity", "(O)", to);
		Py_DECREF(to);
		if (!v) {
			return NULL;
		}
		int car = PyObject_IsTrue(v);
		if (car > 0) {
			PyObject *r = Script_Method(v, "IsType", "(s)", "Car");
			car = r ? PyObject_IsTrue(r) : -1;
			Py_XDECREF(r);
		}
		if (car) {
			PyObject *d = car > 0 ? Script_GetAttr(v, "Descriptor") : NULL;
			Py_DECREF(v);
			return d;
		}
		Py_DECREF(v);
		if (!Chat_SetSendID(id, PyString_FromString("ALL"))) {
			return NULL;
		}
	}
}

static PyObject *Chat_GetID(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	return Chat_IdText(id);
}

/* CycleUp, CycleDown: the next ship net<n> up or down that exists and is not mine, "ALL" past the ends (n 0 to
 * 257); 0 with the error set */
static byte Chat_Cycle(int id, int step) {
	for (;;) {
		PyObject *to = Chat_SendID(id);
		if (!to) {
			return 0;
		}
		long ctr = step > 0 ? -1 : 257;
		if (!Chat_Is(to, "ALL")) {
			PyObject *tail = PySequence_GetSlice(to, 3, INT_MAX);
			PyObject *n = tail ? PyNumber_Int(tail) : NULL;
			Py_XDECREF(tail);
			if (!n) {
				Py_DECREF(to);
				return 0;
			}
			ctr = PyInt_AsLong(n);
			Py_DECREF(n);
		}
		Py_DECREF(to);
		ctr += step;
		if (step > 0 ? ctr > 257 : ctr < 0) {
			return Chat_SetSendID(id, PyString_FromString("ALL"));
		}
		char name[0x20];
		Str_Printf(name, sizeof(name), "net%ld", ctr);
		if (!Chat_SetSendID(id, PyString_FromString(name))) {
			return 0;
		}
		PyObject *v = Script_Ask("Scrap", "GetEntity", "(s)", name);
		if (!v) {
			return 0;
		}
		int none = v == Py_None;
		Py_DECREF(v);
		if (none) {
			continue;
		}
		PyObject *mine = Script_Attr("Chat", "MyShip");
		to = mine ? Chat_SendID(id) : NULL;
		int same = to ? PyObject_Compare(to, mine) == 0 : 0;
		Py_XDECREF(to);
		Py_XDECREF(mine);
		if (!to || PyErr_Occurred()) {
			return 0;
		}
		if (!same) {
			return 1;
		}
	}
}

/* CycleUpAndSet, CycleDownAndSet: the cycle and the receiver on the chat line */
static PyObject *Chat_CycleSet(PyObject *args, int step, byte set) {
	int id;
	if (!PyArg_ParseTuple(args, "i", &id) || !Chat_Cycle(id, step)) {
		return NULL;
	}
	if (set) {
		PyObject *t = Chat_IdText(id);
		byte ok = t && Script_Try("SScorer", "Set", "(issO)", id, "SendID", "Text", t);
		Py_XDECREF(t);
		if (!ok) {
			return NULL;
		}
	}
	return Py_ReturnNone();
}

static PyObject *Chat_CycleUp(PyObject *self, PyObject *args) {
	(void)self;
	return Chat_CycleSet(args, 1, 0);
}

static PyObject *Chat_CycleUpAndSet(PyObject *self, PyObject *args) {
	(void)self;
	return Chat_CycleSet(args, 1, 1);
}

static PyObject *Chat_CycleDown(PyObject *self, PyObject *args) {
	(void)self;
	return Chat_CycleSet(args, -1, 0);
}

static PyObject *Chat_CycleDownAndSet(PyObject *self, PyObject *args) {
	(void)self;
	return Chat_CycleSet(args, -1, 1);
}

/* ServerReSendChat(sendto, cad): the server sends a line on, to all or to the client of the ship net<n> (the
 * script tests the number with "or", so every number passes) */
static byte Chat_ReSend(PyObject *sendto, PyObject *cad) {
	PyObject *string = PyImport_ImportModule("string");
	if (!string) {
		return 0;
	}
	Py_DECREF(string);
	if (Chat_Is(sendto, "ALL")) {
		PyObject *line = Chat_Cat(sendto, "\t", cad);
		byte ok = line && Script_Try("SNet", "SendChatString", "(iO)", -1, line);
		Py_XDECREF(line);
		return ok;
	}
	PyObject *head = PySequence_GetSlice(sendto, 0, 3);
	if (!head) {
		return 0;
	}
	int net = Chat_Is(head, "net");
	Py_DECREF(head);
	if (!net) {
		return 1;
	}
	PyObject *tail = PySequence_GetSlice(sendto, 3, INT_MAX);
	PyObject *val = tail ? PyNumber_Int(tail) : NULL;
	Py_XDECREF(tail);
	PyObject *line = val ? Chat_Cat(sendto, "\t", cad) : NULL;
	byte ok = line && Script_Try("SNet", "SendChatString", "(OO)", val, line);
	Py_XDECREF(line);
	Py_XDECREF(val);
	return ok;
}

static PyObject *Chat_ServerReSendChat(PyObject *self, PyObject *args) {
	PyObject *sendto, *cad;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &sendto, &cad)) {
		return NULL;
	}
	return Chat_ReSend(sendto, cad) ? Py_ReturnNone() : NULL;
}

/* PrintChatString(id, Str): a line "<receiver>\t<text>" arrives: the server sends it on; the text goes to the chat
 * and the console (a line without a tab is dropped) */
static byte Chat_Print(PyObject *str) {
	PyObject *string = PyImport_ImportModule("string");
	if (!string) {
		return 0;
	}
	Py_DECREF(string);
	if (!PyString_Check(str)) {
		/* the error of string.split on a non-string */
		char msg[0x80];
		Str_Printf(msg, sizeof(msg), "argument 1: expected read-only character buffer, %.50s found", str->ob_type->tp_name);
		PyErr_SetString(PyExc_TypeError, msg);
		return 0;
	}
	/* cadinfo[0] and cadinfo[1] of string.split(Str, "\t") */
	const char *s = PyString_AsString(str), *tab = strchr(s, '\t');
	if (!tab) {
		return 1;
	}
	const char *end = strchr(tab + 1, '\t');
	PyObject *to = PyString_FromStringAndSize((char *)s, (int)(tab - s));
	PyObject *text = to ? PyString_FromStringAndSize((char *)tab + 1, end ? (int)(end - tab - 1) : (int)strlen(tab + 1)) :
		NULL;
	int server = text ? Chat_NetFlag(1) : -1;
	byte ok = server >= 0 && (!server || Chat_ReSend(to, text)) &&
		Script_Try("SScorer", "AddChatMsg", "(Oiii)", text, 0, 255, 0);
	PyObject *line = ok ? Chat_Cat(text, "\n", NULL) : NULL;
	ok = line && Script_Try("Scrap", "Print", "(O)", line);
	Py_XDECREF(line);
	Py_XDECREF(text);
	Py_XDECREF(to);
	return ok;
}

static PyObject *Chat_PrintChatString(PyObject *self, PyObject *args) {
	PyObject *id, *str;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &str)) {
		return NULL;
	}
	return Chat_Print(str) ? Py_ReturnNone() : NULL;
}

/* EndChat(id): the chat line closes, back to the game */
static byte Chat_End(int id) {
	PyObject *menu = PyImport_ImportModule("Menu");
	if (!menu) {
		return 0;
	}
	Py_DECREF(menu);
	return Script_Try("SInput", "Bind", "(isss)", id, "Menu", "Cancel", "") &&
		Script_Try("SInput", "Bind", "(isss)", id, "Menu", "PgUp", "") &&
		Script_Try("SInput", "Bind", "(isss)", id, "Menu", "PgDown", "") &&
		Script_Try("SInput", "SetInputFunc", "(is)", id, "") && Script_Try("SScorer", "SetOnCancel", "(is)", id, "") &&
		Script_Try("Menu", "BackToGame", "(is)", id, "");
}

static PyObject *Chat_EndChat(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	return Chat_End(id) ? Py_ReturnNone() : NULL;
}

/* SendChatString(id, ChatStr, console=0): "<player> text" to the receiver of the player; the server shows it at
 * once, a client sends it (and shows a private line itself); the chat line closes unless it came from the console */
static byte Chat_Send(int id, PyObject *chat, int console) {
	PyObject *player = Script_Ask("Scrap", "Get", "(s)", "PlayerName");
	PyObject *open = player ? PyString_FromString("<") : NULL;
	PyObject *named = open ? Chat_Cat(open, "", player) : NULL;
	PyObject *str = named ? Chat_Cat(named, "> ", chat) : NULL;
	Py_XDECREF(named);
	Py_XDECREF(open);
	Py_XDECREF(player);
	PyObject *to = str ? Chat_SendID(id) : NULL;
	PyObject *full = to ? Chat_Cat(to, "\t", str) : NULL;
	Py_XDECREF(to);
	Py_XDECREF(str);
	int server = full ? Chat_NetFlag(1) : -1;
	byte ok = server >= 0;
	if (ok && server) {
		ok = Chat_Print(full);
	} else if (ok) {
		to = Chat_SendID(id);
		ok = to != NULL;
		if (ok && !Chat_Is(to, "ALL")) {
			ok = Chat_Print(full);
		}
		Py_XDECREF(to);
		ok = ok && Script_Try("SNet", "SendChatString", "(iO)", -1, full);
	}
	Py_XDECREF(full);
	return ok && (console || Chat_End(id));
}

static PyObject *Chat_SendChatString(PyObject *self, PyObject *args) {
	int id;
	PyObject *chat, *console = NULL;
	(void)self;
	if (!PyArg_ParseTuple(args, "iO|O", &id, &chat, &console)) {
		return NULL;
	}
	int c = console ? PyObject_IsTrue(console) : 0;
	return c >= 0 && Chat_Send(id, chat, c) ? Py_ReturnNone() : NULL;
}

/* GetMyShip(): the ship of this machine (a client asks the network, the server has net256 when it plays) */
static byte Chat_MyShip(void) {
	int client = Chat_NetFlag(0);
	if (client < 0) {
		return 0;
	}
	if (client) {
		PyObject *ship = Script_Ask("SNet", "GetMyClientShip", "()");
		if (!ship) {
			return 0;
		}
		Script_SetVar("Chat", "MyShip", ship);
		return 1;
	}
	Script_SetVar("Chat", "MyShip", PyString_FromString("net256"));
	PyObject *v = Script_Ask("Scrap", "GetEntity", "(s)", "net256");
	if (!v) {
		return 0;
	}
	if (v == Py_None) {
		Script_SetVar("Chat", "MyShip", PyString_FromString(""));
	}
	Py_DECREF(v);
	return 1;
}

static PyObject *Chat_GetMyShip(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Chat_MyShip() ? Py_ReturnNone() : NULL;
}

/* InitChat(id): the chat key opens the chat line */
static PyObject *Chat_InitChat(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	PyObject *menu = PyImport_ImportModule("Menu");
	if (!menu) {
		return NULL;
	}
	Py_DECREF(menu);
	PyObject *t = Chat_MyShip() ? Chat_IdText(id) : NULL;
	PyObject *vars = t ? Py_BuildValue("{s:O}", "sendid", t) : NULL;
	Py_XDECREF(t);
	if (!vars) {
		return NULL;
	}
	Script_RunFile(id, "menus/chat.def", "InitChat", chat_def, NULL, vars);
	Py_DECREF(vars);
	return Py_ReturnNone();
}

/* ConsoleChatString(estrin): a line of the full screen console goes to the receiver of player 0 */
static PyObject *Chat_ConsoleChatString(PyObject *self, PyObject *args) {
	PyObject *str;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &str)) {
		return NULL;
	}
	return Chat_Send(0, str, 1) ? Py_ReturnNone() : NULL;
}

/* qchat1 .. qchat6(id): the quick message QuickMessageString<n> */
static PyObject *Chat_Quick(PyObject *args, int n) {
	int id;
	char var[0x20];
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Str_Printf(var, sizeof(var), "QuickMessageString%d", n);
	PyObject *str = Script_Ask("Scrap", "Get", "(s)", var);
	byte ok = str && Chat_Send(id, str, 1);
	Py_XDECREF(str);
	return ok ? Py_ReturnNone() : NULL;
}

#define CHAT_QUICK(n) \
	static PyObject *Chat_qchat##n(PyObject *self, PyObject *args) { \
		(void)self; \
		return Chat_Quick(args, n); \
	}

CHAT_QUICK(1)
CHAT_QUICK(2)
CHAT_QUICK(3)
CHAT_QUICK(4)
CHAT_QUICK(5)
CHAT_QUICK(6)

/* SetChatProc(): the callbacks of the engine */
static PyObject *Chat_SetChatProc(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	GVar_SetStr("ChatEvent", "Chat.InitChat");
	GVar_SetStr("OnChatString", "Chat.PrintChatString");
	GVar_SetStr("FullScreenConsoleCallback", "Chat.ConsoleChatString");
	GVar_SetStr("QChatEvent1", "Chat.qchat1");
	GVar_SetStr("QChatEvent2", "Chat.qchat2");
	GVar_SetStr("QChatEvent3", "Chat.qchat3");
	GVar_SetStr("QChatEvent4", "Chat.qchat4");
	GVar_SetStr("QChatEvent5", "Chat.qchat5");
	GVar_SetStr("QChatEvent6", "Chat.qchat6");
	return Py_ReturnNone();
}

PyMethodDef g_ChatMethods[] = {
	{ "GetID", Chat_GetID, 1, NULL },
	{ "CycleUp", Chat_CycleUp, 1, NULL },
	{ "CycleUpAndSet", Chat_CycleUpAndSet, 1, NULL },
	{ "CycleDown", Chat_CycleDown, 1, NULL },
	{ "CycleDownAndSet", Chat_CycleDownAndSet, 1, NULL },
	{ "ServerReSendChat", Chat_ServerReSendChat, 1, NULL },
	{ "PrintChatString", Chat_PrintChatString, 1, NULL },
	{ "SendChatString", Chat_SendChatString, 1, NULL },
	{ "EndChat", Chat_EndChat, 1, NULL },
	{ "GetMyShip", Chat_GetMyShip, 1, NULL },
	{ "InitChat", Chat_InitChat, 1, NULL },
	{ "ConsoleChatString", Chat_ConsoleChatString, 1, NULL },
	{ "qchat1", Chat_qchat1, 1, NULL },
	{ "qchat2", Chat_qchat2, 1, NULL },
	{ "qchat3", Chat_qchat3, 1, NULL },
	{ "qchat4", Chat_qchat4, 1, NULL },
	{ "qchat5", Chat_qchat5, 1, NULL },
	{ "qchat6", Chat_qchat6, 1, NULL },
	{ "SetChatProc", Chat_SetChatProc, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
