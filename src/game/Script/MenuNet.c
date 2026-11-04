/*
 * The multiplayer menus of PCMenu in C (issue #2, step 3): MultiPlayerMenu, the join page with the server browser,
 * the address and password pages, the connection to a server and the part the create and join pages share. The
 * layout is data/menus/net.menu, one section per page.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/filepak.h"
#include "scrap/game.h"
#include "scrap/input.h"
#include "scrap/net.h"
#include "scrap/scheduler.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/timer.h"

static const char net_menu[] = {
#embed "../../../data/menus/net.menu"
	, 0 };

#define NET_FILE "menus/net.menu"
#define NET_SLOTS 13			/* the lines of the browser */
#define NET_MAX_SERVERS 512
#define NET_SLIDER_Y 136
#define NET_SLIDER_H 208

/* the flags of a server in the browser */
#define SIBF_DEDICATED 1
#define SIBF_FORCE_SHIP 2
#define SIBF_PASSWORD 4

typedef struct {
	int ping;
	char address[0x40];
	char name[0x80];		/* with the password and dedicated symbols in front */
	int numPlayers, maxPlayers;
	int version;
	int port;
	char type[0x40];
	char map[0x40];
	int flags;
} netServer_t;

static struct {
	netServer_t servers[NET_MAX_SERVERS];
	int numServers;
	int index;			/* the first server shown */
	char domain[0x10];		/* Internet, Local or IP */
	char sort[0x10];		/* the column the list is sorted by */
	int sortOrder;			/* 1: reversed */
	char motd[0x200];		/* the message of the day of the master server */
} net = { .domain = "Internet", .sort = "Ping" };

static void Net_RunMenu(int id, const char *section, PyObject *vars);
static void Net_JoinMenu(int id);

static void Net_Menu(const char *func, int id) {
	Script_Call("Menu", func, "(i)", id);
}

/* ---- the server list ---- */

/* the order of the list: by the column, then by the other values (the order of the script's tuples) */
static int Net_Compare(const void *pa, const void *pb) {
	const netServer_t *a = pa, *b = pb;
	int c;

	if (!strcmp(net.sort, "Name")) {
		c = strcmp(a->name, b->name);
	} else if (!strcmp(net.sort, "Type")) {
		c = strcmp(a->type, b->type);
	} else if (!strcmp(net.sort, "Players")) {
		/* the script mixed two keys here ("n/m" texts for new servers, -n after a sort); always -n */
		c = (b->numPlayers > a->numPlayers) - (b->numPlayers < a->numPlayers);
	} else if (!strcmp(net.sort, "Map")) {
		c = strcmp(a->map, b->map);
	} else {
		c = (a->ping > b->ping) - (a->ping < b->ping);
	}
	if (!c) c = (a->ping > b->ping) - (a->ping < b->ping);
	if (!c) c = strcmp(a->address, b->address);
	if (!c) c = strcmp(a->name, b->name);
	if (!c) c = (a->numPlayers > b->numPlayers) - (a->numPlayers < b->numPlayers);
	if (!c) c = (a->maxPlayers > b->maxPlayers) - (a->maxPlayers < b->maxPlayers);
	if (!c) c = (a->version > b->version) - (a->version < b->version);
	if (!c) c = (a->port > b->port) - (a->port < b->port);
	if (!c) c = strcmp(a->type, b->type);
	if (!c) c = strcmp(a->map, b->map);
	if (!c) c = (a->flags > b->flags) - (a->flags < b->flags);
	return net.sortOrder == 1 ? -c : c;
}

static void Net_Sort(void) {
	qsort(net.servers, net.numServers, sizeof(net.servers[0]), Net_Compare);
}

/* one line of the browser: name, mode, players, map, ping in fixed columns */
static void Net_ServerLine(const netServer_t *s, char *out, int size) {
	char mode[0x48], players[0x20], ping[0x10];
	Str_Printf(mode, sizeof(mode), "%c %s", s->flags & SIBF_FORCE_SHIP ? 0x0b : 0x0a, s->type);
	Str_Printf(players, sizeof(players), "(%d/%d)", s->numPlayers, s->maxPlayers);
	Str_Printf(ping, sizeof(ping), "%d", s->ping);
	Str_Printf(out, size, "%-28.28s %-14.14s %7.7s %-19.19s %4.4s", s->name, mode, players, s->map, ping);
}

static void Net_UpdateSlider(int id) {
	int n = net.numServers;
	if (n <= NET_SLOTS) {
		Script_SetNum(id, "BrowserSlider", "Y", NET_SLIDER_Y);
		Script_SetNum(id, "BrowserSlider", "ScaleY", NET_SLIDER_H);
		return;
	}
	double step = NET_SLIDER_H / (double)(n - NET_SLOTS + 1);
	double size = step;
	if (size < 10) {
		size = 10;
		step = (NET_SLIDER_H - (size - step)) / (double)(n - NET_SLOTS + 1);
	}
	Script_SetNum(id, "BrowserSlider", "Y", NET_SLIDER_Y + net.index * step);
	Script_SetNum(id, "BrowserSlider", "ScaleY", size);
}

static void Net_UpdateBrowser(int id) {
	int count = 0;
	for (int i = 0; i < NET_SLOTS; i++) {
		char name[0x20], link[0x20], line[0x100] = "";
		if (net.numServers > i + net.index) {
			Net_ServerLine(&net.servers[i + net.index], line, sizeof(line));
		}
		Str_Printf(name, sizeof(name), "BrowserSlot%d", i + 1);
		Script_SetStr(id, name, "Text", line);
		Script_SetNum(id, name, "W", 460);
		Str_Printf(link, sizeof(link), "BrowserSlot%d", i);
		Script_SetStr(id, name, "Up", link);
		Str_Printf(link, sizeof(link), "BrowserSlot%d", i + 2);
		Script_SetStr(id, name, "Down", link);
		Script_SetStr(id, name, "Right", "BrowserUp");
		Script_SetNum(id, name, "Visible", line[0] != 0);
		count += line[0] != 0;
	}
	const char *tab = !strcmp(net.domain, "Internet") ? "JoinInternet" : !strcmp(net.domain, "Local") ? "JoinLocal" : NULL;
	if (tab) {
		Script_Call("Menu", "LinkUD", "(iss)", id, tab, count ? "BrowserSlot1" : "Refresh");
	}
	if (count) {
		char last[0x20];
		Str_Printf(last, sizeof(last), "BrowserSlot%d", count);
		Script_Call("Menu", "LinkUD", "(iss)", id, last, "Refresh");
	}
	Net_UpdateSlider(id);
}

static void Net_ClearList(void) {
	net.index = 0;
	net.numServers = 0;
}

static void Net_BrowseLocal(void) {
	char ver[0x40], *sp;
	GVar_SetNum("DefaultServerPort", GVar_GetNum("DefaultServerPort", 1));
	Net_ClearList();
	Net_InitBrowser(0, "255.255.255.255");
	GVar_SetStr("OnNetBrowseCallback", "PCMenu.OnNetBrowse");
	Str_Printf(ver, sizeof(ver), "ver=%s", Version_Get());
	if ((sp = strchr(ver, ' '))) {
		*sp = 0;
	}
	Net_SendMasterString(ver);
	GVar_SetStr("MasterCommandFunc", "PCMenu.MasterCommand");
}

/* asks the master server for i blocks of 32 servers, the last block first; 0 when it could not be sent */
static byte Net_MasterBrowse(int i) {
	while (i) {
		char cmd[0x40];
		i--;
		Str_Printf(cmd, sizeof(cmd), "Brw=%d,%d", i * 32, (i + 1) * 32 - 1);
		if (!Net_SendMasterString(cmd)) {
			return 0;
		}
	}
	return 1;
}

static void Net_BrowseInternet(void) {
	Net_ClearList();
	Net_InitBrowser(0, GVar_GetStr("MasterServerAddress"));
	if (Net_MasterBrowse(8)) {
		GVar_SetStr("OnNetBrowseCallback", "PCMenu.OnNetBrowse");
	} else {
		Net_DoneBrowser();
	}
}

static void Net_Refresh(int id) {
	Net_DoneBrowser();
	if (!strcmp(net.domain, "Internet")) {
		Net_PingInetSvrs();
		Net_BrowseInternet();
	} else if (!strcmp(net.domain, "Local")) {
		Net_BrowseLocal();
	}
	Net_UpdateBrowser(id);
}

/* the column back of the sort column lit, the list sorted by it (again: the other way round) */
static void Net_SortBy(int id, const char *column) {
	char item[0x40];
	Str_Printf(item, sizeof(item), "BrowserColumnBack_%s", net.sort);
	Script_SetNum(id, item, "Red", 255);
	Script_SetNum(id, item, "Green", 255);
	Script_SetNum(id, item, "Blue", 255);
	Script_SetNum(id, item, "Alpha", 64);
	net.sortOrder = !strcmp(net.sort, column) ? !net.sortOrder : 0;
	Str_Copy(net.sort, column, sizeof(net.sort));
	Str_Printf(item, sizeof(item), "BrowserColumnBack_%s", column);
	Script_SetNum(id, item, "Red", 255);
	Script_SetNum(id, item, "Green", 255);
	Script_SetNum(id, item, "Blue", 192);
	Script_SetNum(id, item, "Alpha", 128);
	Net_Sort();
	Net_UpdateBrowser(id);
}

/* the first maxlen characters of text; a \1 starts a character of 4 more bytes */
static void Net_CutText(const char *text, int maxlen, char *out, int size) {
	int len = (int)strlen(text);
	int n = len < maxlen ? len : maxlen;
	int j = 0;
	if (n <= 0) {
		out[0] = 0;
		return;
	}
	while (j < n && j < len) {
		if (text[j] == 1) {
			j += 4;
			n += 4;
		}
		j++;
	}
	int end = j > len ? len : j;
	Str_Copy(out, text, end + 1 < size ? end + 1 : size);
}

/* ---- the connection ---- */

static PyObject *Net_StatusFunc(void);

/* the state of the connection every quarter second: messages, the password page */
static void Net_ConnectionStatus(void) {
	char last[0x200];
	const char *status = GVar_GetStr("ClientConectionStatus");
	byte full = 0;

	Str_Copy(last, Script_GetStr(0, "Title", "Text"), sizeof(last));
	if (!strcmp(status, "server_is_not_responding")) {
		Script_SetStr(0, "Title", "Text", Script_Lang("Net_Menu_ServerDown"));
	} else if (!strcmp(status, "need_password")) {
		Net_CloseClient();
		Script_Call("PCMenu", "AskPassword", "(is)", 0, "");
		return;
	} else if (!strcmp(status, "server_is_full")) {
		static const struct {
			const char *item, *text, *func;
		} buttons[] = { { "Item1", "Menu_Multiplayer_JoinGame_Retry", "PCMenu.TryJoin" },
			{ "Item2", "Menu_Multiplayer_JoinGame_Abort", "PCMenu.AbortJoin" } };
		Script_SetStr(0, "Title", "Text", Script_Lang("Net_Menu_ServerFull"));
		Net_CloseClient();
		full = 1;
		for (int i = 0; i < 2; i++) {
			Script_SetStr(0, buttons[i].item, "Text", Script_Lang(buttons[i].text));
			Script_SetNum(0, buttons[i].item, "X", 320);
			double y = Script_GetNum(0, buttons[i].item, "Y");
			Script_SetNum(0, buttons[i].item, "CentralText", 1);
			Script_SetStr(0, buttons[i].item, "OnAccept", buttons[i].func);
			Script_SetNum(0, buttons[i].item, "Y", y);
		}
	}
	double bar = Script_GetNum(0, "TitleBarM", "Y");
	const char *title = Script_GetStr(0, "Title", "Text");
	if (strcmp(last, title)) {
		Script_Call("Menu", "DrawMenuTitleBar", "(ids)", 0, bar, title);
	}
	/* a full server ends the polling (the script deleted its own next call) */
	if (!full) {
		PyObject *f = Net_StatusFunc();
		PyObject *params = PyTuple_New(0);
		if (f && params) {
			Scheduler_Add(&g_pWorld->scheduler, Timer_GetTime(&g_pWorld->timer) + 0.25f, f, params, "ConectionStatus");
		}
		Py_XDECREF(params);
	}
}

static void Net_TryJoin(int id) {
	char address[0x80], title[0x200];
	const char *addr = GVar_GetStr("DefaultServerAddress");
	int port = (int)GVar_GetNum("DefaultServerPort", 0);

	Net_InitClient(addr, port);
	Net_Menu("StartNewMenu", id);
	Str_Printf(address, sizeof(address), "%s:%d", addr, port);
	/* the text has a %s for the address */
	const char *fmt = Script_Lang("Net_Menu_Connecting");
	const char *at = strstr(fmt, "%s");
	if (at) {
		Str_Printf(title, sizeof(title), "%.*s%s%s", (int)(at - fmt), fmt, address, at + 2);
	} else {
		Str_Copy(title, fmt, sizeof(title));
	}
	PyObject *vars = Py_BuildValue("{s:s}", "title", title);
	Net_RunMenu(id, "connecting", vars);
	Py_XDECREF(vars);
	Net_ConnectionStatus();
}

/* ---- the steps of net.menu ---- */

/* the entry of the game mode (PCMenu.MultiplayerMenu, set by the mode) is disabled when it has no function */
static void Step_InGame(int id, PyObject *args) {
	(void)args;
	PyObject *m = PyImport_ImportModule("PCMenu");
	PyObject *opt = m ? PyObject_GetAttrString(m, "MultiplayerMenu") : NULL;
	PyObject *func = opt ? PySequence_GetItem(opt, 1) : NULL;
	if (func && PyString_Check(func) && !PyString_AsString(func)[0]) {
		Script_Call("Menu", "DisableMenuItem", "(iii)", id, 2, 5);
	}
	PyErr_Clear();
	Py_XDECREF(func);
	Py_XDECREF(opt);
	Py_XDECREF(m);
	Script_SetInt("Menu", "fromMultiPlayer", 1);
}

/* the tabs of the join page: the focus box over the whole button */
static void Step_TabWidth(int id, PyObject *args) {
	const char *item = Script_CurrentItem();
	(void)args;
	Script_SetNum(id, item, "FocusPivotX", -(82 - Script_GetNum(id, item, "W") / 2));
	Script_SetNum(id, item, "W", 165);
}

/* columntext key maxlen: the text of the column title, cut */
static void Step_ColumnText(int id, PyObject *args) {
	const char *key;
	int maxlen;
	char text[0x200];
	if (!PyArg_ParseTuple(args, "si", &key, &maxlen)) {
		PyErr_Print();
		return;
	}
	Net_CutText(Script_Lang(key), maxlen, text, sizeof(text));
	Script_SetStr(id, Script_CurrentItem(), "Text", text);
}

static void Step_Browse(int id, PyObject *args) {
	char column[0x10];
	(void)args;
	Net_Refresh(id);
	net.sortOrder = 1;
	Str_Copy(column, net.sort, sizeof(column));
	Net_SortBy(id, column);
}

static const scriptStep_t net_steps[] = {
	{ "ingame", Step_InGame },
	{ "tabwidth", Step_TabWidth },
	{ "columntext", Step_ColumnText },
	{ "browse", Step_Browse },
	{ NULL, NULL },
};

/* the values every page may use */
static PyObject *Net_Vars(const char *tab) {
	return Py_BuildValue("{s:s,s:s,s:s,s:i,s:i,s:s}", "tab", tab, "domain", net.domain, "motd", net.motd, "motdx",
		Script_GetInt("Scorer", "MarginH", 0) + 5, "motdy", 480 - Script_GetInt("Scorer", "MarginV", 0) - 20,
		"serveraddress", GVar_GetStr("DefaultServerAddress"));
}

static void Net_RunMenu(int id, const char *section, PyObject *vars) {
	Script_RunMenu(id, NET_FILE, section, net_menu, net_steps, vars);
}

static void Net_JoinMenu(int id) {
	cScorer *sc = Scorer_Get(id);
	char current[0x80];
	Str_Copy(current, sc && sc->current ? sc->current : "Join", sizeof(current));
	PyObject *vars = Net_Vars("Join");
	if (vars) {
		PyObject *c = PyString_FromString(current);
		PyDict_SetItemString(vars, "current", c);
		Py_XDECREF(c);
	}
	Net_RunMenu(id, "Join", vars);
	Py_XDECREF(vars);
}

/* ---- the functions of PCMenu ---- */

#define NET_ARGS(args) \
	int id; \
	const char *control; \
	if (!Script_Args(args, &id, &control)) { \
		return NULL; \
	}

static PyObject *PCMenu_MultiPlayerMenu(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	Net_Menu("StartNewMenu", id);
	if (Script_GetInt("Init", "inMainMenu", 0) || Script_GetInt("Init", "isShipEdit", 0)) {
		Net_JoinMenu(id);
		return Py_ReturnNone();
	}
	PyObject *vars = Py_BuildValue("{s:i}", "server", Net_IsServer() != 0);
	Net_RunMenu(id, "Multiplayer", vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

static PyObject *PCMenu_MenuRestartServer(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)id;
	(void)control;
	char path[0x100];
	if (Timer_GetTime(&g_pWorld->timer) > 15 && Net_IsServer()) {
		Net_ServerChangeLevel(Script_GetString("Init", "Path", path, sizeof(path)));
	}
	return Py_ReturnNone();
}

static PyObject *PCMenu_MenuJoinGame(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	char text[0x100], *colon;
	Net_CloseClient();
	Str_Copy(text, Script_GetStr(id, "Item1", "Text"), sizeof(text));
	colon = strchr(text, ':');
	if (colon) {
		*colon = 0;
	}
	GVar_SetStr("DefaultServerAddress", text);
	GVar_SetNum("DefaultServerPort", colon ? atoi(colon + 1) : GVar_GetNum("DefaultServerPort", 1));
	Net_TryJoin(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_TryJoin(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	Net_TryJoin(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_JoinPassword(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	GVar_SetStr("ClientPassword", Script_GetStr(id, "Item1", "Text"));
	Net_TryJoin(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_AskPassword(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	const char *title = Script_Lang("Menu_Multiplayer_Advanced_Password");
	Net_Menu("StartNewMenu", id);
	PyObject *vars = Py_BuildValue("{s:s,s:s}", "title", *title ? title : "Password", "password",
		GVar_GetStr("ClientPassword"));
	Net_RunMenu(id, "password", vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

static PyObject *PCMenu_ShowConectionStatus(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Net_ConnectionStatus();
	return Py_ReturnNone();
}

static PyMethodDef net_status_def = { "ShowConectionStatus", PCMenu_ShowConectionStatus, 1, NULL };

static PyObject *Net_StatusFunc(void) {
	static PyObject *f;
	if (!f) {
		f = PyCFunction_New(&net_status_def, NULL);
	}
	return f;
}

static PyObject *PCMenu_AbortJoin(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	Scheduler_Delete(&g_pWorld->scheduler, "ConectionStatus");
	Net_CloseClient();
	Net_JoinMenu(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_SpecifyIPMenu(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	PyObject *vars = Net_Vars("Join");
	Net_RunMenu(id, "ip", vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

static PyObject *PCMenu_AbortMultiplayerGame(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	Script_Call("Menu", "YesNoMenu", "(isss)", id, Script_Lang("Menu_AbortGame_Question"), "Menu.AuxAbortGame",
		"PCMenu.MultiPlayerMenu");
	return Py_ReturnNone();
}

/* NetDefaultContentMenu(id, control[, tab]): Back, the garage and the Create/Join tabs, the message of the day */
static PyObject *PCMenu_NetDefaultContentMenu(PyObject *self, PyObject *args) {
	int id;
	PyObject *control;
	char *tab = "Create";
	(void)self;
	if (!PyArg_ParseTuple(args, "iO|s", &id, &control, &tab)) {
		return NULL;
	}
	PyObject *vars = Net_Vars(tab);
	Net_RunMenu(id, "content", vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

static PyObject *PCMenu_NetJoinGameMenu(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	Net_JoinMenu(id);
	return Py_ReturnNone();
}

static PyObject *Net_Domain(PyObject *args, const char *domain) {
	NET_ARGS(args);
	(void)control;
	Net_DoneBrowser();
	Str_Copy(net.domain, domain, sizeof(net.domain));
	Net_JoinMenu(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_NetJoinInternetMenu(PyObject *self, PyObject *args) {
	(void)self;
	return Net_Domain(args, "Internet");
}

static PyObject *PCMenu_NetJoinLocalMenu(PyObject *self, PyObject *args) {
	(void)self;
	return Net_Domain(args, "Local");
}

static PyObject *PCMenu_NetJoinIPMenu(PyObject *self, PyObject *args) {
	(void)self;
	return Net_Domain(args, "IP");
}

/* OnNetBrowse(address, flags, name, players, maxPlayers, version, port, type, ping, map): a server answered */
static PyObject *PCMenu_OnNetBrowse(PyObject *self, PyObject *args) {
	char *address, *name, *type, *map;
	int flags, players, maxPlayers, version, port, ping;
	(void)self;
	if (!PyArg_ParseTuple(args, "sisiiiisis", &address, &flags, &name, &players, &maxPlayers, &version, &port, &type,
		&ping, &map)) {
		return NULL;
	}
	if (net.numServers >= NET_MAX_SERVERS) {
		return Py_ReturnNone();
	}
	netServer_t *s = &net.servers[net.numServers++];
	s->ping = ping;
	Str_Copy(s->address, address, sizeof(s->address));
	Str_Printf(s->name, sizeof(s->name), "%s%s%s", flags & SIBF_PASSWORD ? "\x08 " : "  ",
		flags & SIBF_DEDICATED ? "\x09 " : "  ", name);
	s->numPlayers = players;
	s->maxPlayers = maxPlayers;
	s->version = version;
	s->port = port;
	Str_Copy(s->type, type, sizeof(s->type));
	Str_Copy(s->map, map, sizeof(s->map));
	s->flags = flags;
	Net_Sort();
	Net_UpdateBrowser(0);
	return Py_ReturnNone();
}

static PyObject *PCMenu_BrowserSortExecute(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Net_Sort();
	return Py_ReturnNone();
}

static PyObject *PCMenu_BrowseLocalMenu(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Net_BrowseLocal();
	return Py_ReturnNone();
}

/* MasterCommand(text): a message of the master server, "msg=" is the message of the day */
static PyObject *PCMenu_MasterCommand(PyObject *self, PyObject *args) {
	char *text;
	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	if (!strncmp(text, "msg=", 4)) {
		Str_Copy(net.motd, text + 4, sizeof(net.motd));
		Script_SetStr(0, "MOTD", "Text", net.motd);
	}
	return Py_ReturnNone();
}

static PyObject *PCMenu_BrowseINetMenu(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	Net_BrowseInternet();
	return Py_ReturnNone();
}

static PyObject *PCMenu_InetMasterBrowse(PyObject *self, PyObject *args) {
	int i;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &i)) {
		return NULL;
	}
	return PyInt_FromLong(Net_MasterBrowse(i));
}

/* the script called a MainMenu of its own module here, which does not exist; the main menu of Menu is meant */
static PyObject *PCMenu_BrowserMenuBack(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cPy_Function_SetFunc(&sc->onNext, "");
		cPy_Function_SetFunc(&sc->onPrev, "");
	}
	Net_DoneBrowser();
	Script_Call("Menu", "MainMenu", "(is)", id, control);
	return Py_ReturnNone();
}

static PyObject *PCMenu_RefreshBrowseMenu(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	Net_Refresh(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_RefreshBrowseMenuButton(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	Net_DoneBrowser();
	Net_Refresh(id);
	return Py_ReturnNone();
}

/* the arrows and the page keys of the browser; the names are the script's (Previous scrolls down) */
static PyObject *Net_Scroll(PyObject *args, int how) {
	NET_ARGS(args);
	(void)control;
	int n = net.numServers;
	if (how == 0) {
		if (n <= net.index + NET_SLOTS) {
			return Py_ReturnNone();
		}
		net.index++;
	} else if (how == 1) {
		if (net.index <= 0) {
			return Py_ReturnNone();
		}
		net.index--;
	} else if (how == 2) {
		net.index = n <= NET_SLOTS ? 0 : n > net.index + NET_SLOTS * 2 ? net.index + NET_SLOTS : n - NET_SLOTS;
	} else {
		net.index = net.index > NET_SLOTS - 1 ? net.index - NET_SLOTS : 0;
	}
	Net_UpdateBrowser(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_BrowseLocalMenuPrevious(PyObject *self, PyObject *args) {
	(void)self;
	return Net_Scroll(args, 0);
}

static PyObject *PCMenu_BrowseLocalMenuNext(PyObject *self, PyObject *args) {
	(void)self;
	return Net_Scroll(args, 1);
}

static PyObject *PCMenu_BrowseLocalMenuPgUp(PyObject *self, PyObject *args) {
	(void)self;
	return Net_Scroll(args, 2);
}

static PyObject *PCMenu_BrowseLocalMenuPgDown(PyObject *self, PyObject *args) {
	(void)self;
	return Net_Scroll(args, 3);
}

/* BrowseLocalJoin(id, "BrowserSlotN"): joins the server of that line when its mode and map are here */
static PyObject *PCMenu_BrowseLocalJoin(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	if (strlen(control) <= 11) {
		return Py_ReturnNone();
	}
	int i = atoi(control + 11) - 1 + net.index;
	if (i < 0 || i >= net.numServers) {
		return Py_ReturnNone();
	}
	const netServer_t *s = &net.servers[i];
	char file[0x100], msg[0x100];
	cFilePack pack;
	GVar_SetStr("DefaultServerAddress", s->address);
	for (int check = 0; check < 2; check++) {
		Str_Printf(file, sizeof(file), check ? "Levels/%s/Scripts/Map.py" : "scripts/net/n%s.py", check ? s->map : s->type);
		cFilePack_ctor(&pack);
		byte found = cFilePack_Exists(&pack.file.base, file);
		if (!found) {
			Str_Printf(file + strlen(file), sizeof(file) - (int)strlen(file), "c");
			found = cFilePack_Exists(&pack.file.base, file);
		}
		cFilePack_dtor(&pack);
		if (!found) {
			Str_Printf(msg, sizeof(msg), check ? "%s map not found." : "%s modification not supported.",
				check ? s->map : s->type);
			Script_SetStr(0, "MOTD", "Text", msg);
			return Py_ReturnNone();
		}
	}
	GVar_SetNum("DefaultServerPort", s->port);
	Net_DoneBrowser();
	Net_TryJoin(id);
	return Py_ReturnNone();
}

/* UnicodeParse(text, maxlen) */
static PyObject *PCMenu_UnicodeParse(PyObject *self, PyObject *args) {
	char *text;
	int maxlen;
	char out[0x400];
	(void)self;
	if (!PyArg_ParseTuple(args, "si", &text, &maxlen)) {
		return NULL;
	}
	Net_CutText(text, maxlen, out, sizeof(out));
	return PyString_FromString(out);
}

static PyObject *PCMenu_CreateBrowseMenu(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	PyObject *vars = Net_Vars("Join");
	Net_RunMenu(id, "browser", vars);
	Py_XDECREF(vars);
	return Py_ReturnNone();
}

static PyObject *PCMenu_BrowserColumnBack_Click(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	Net_SortBy(id, strlen(control) > 18 ? control + 18 : "");
	return Py_ReturnNone();
}

static PyObject *PCMenu_BrowserSliderGainFocus(PyObject *self, PyObject *args) {
	int id;
	PyObject *a, *b;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &a, &b)) {
		return NULL;
	}
	Script_SetStr(id, "BrowserSliderArea", "OnRender", "PCMenu.BrowserSliderOnRender");
	return Py_ReturnNone();
}

static PyObject *PCMenu_BrowserSliderLooseFocus(PyObject *self, PyObject *args) {
	int id;
	PyObject *a, *b;
	(void)self;
	if (!PyArg_ParseTuple(args, "iOO", &id, &a, &b)) {
		return NULL;
	}
	Script_SetStr(id, "BrowserSliderArea", "OnRender", "");
	return Py_ReturnNone();
}

/* while the slider has the focus and accept is held, the cursor height picks the first server shown */
static PyObject *PCMenu_BrowserSliderOnRender(PyObject *self, PyObject *args) {
	(void)self;
	NET_ARGS(args);
	(void)control;
	int n = net.numServers;
	if (!Input_MenuAccept(id) || n <= NET_SLOTS) {
		return Py_ReturnNone();
	}
	double y = Script_GetNum(id, "Cursor", "Y") - NET_SLIDER_Y - 5;
	if (y < 0) {
		y = 0;
	}
	if (y > NET_SLIDER_H - 10) {
		y = NET_SLIDER_H - 10;
	}
	int old = net.index;
	net.index = (int)((double)(n - NET_SLOTS + 1) * (y / (NET_SLIDER_H - 10)));
	if (net.index >= n - NET_SLOTS) {
		net.index = n - NET_SLOTS;
	}
	if (old != net.index) {
		Net_UpdateBrowser(id);
	}
	return Py_ReturnNone();
}

static PyObject *PCMenu_UpdateBrowseSlider(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Net_UpdateSlider(id);
	return Py_ReturnNone();
}

static PyObject *PCMenu_UpdateBrowseMenu(PyObject *self, PyObject *args) {
	int id;
	(void)self;
	if (!PyArg_ParseTuple(args, "i", &id)) {
		return NULL;
	}
	Net_UpdateBrowser(id);
	return Py_ReturnNone();
}

/* BrowseFormatServerLine(info): info as the script kept it, (ping, address, name, players, max, version, port,
 * type, map, flags) */
static PyObject *PCMenu_BrowseFormatServerLine(PyObject *self, PyObject *args) {
	netServer_t s;
	char *address, *name, *type, *map, line[0x100];
	(void)self;
	if (!PyArg_ParseTuple(args, "(issiiiissi)", &s.ping, &address, &name, &s.numPlayers, &s.maxPlayers, &s.version,
		&s.port, &type, &map, &s.flags)) {
		return NULL;
	}
	Str_Copy(s.name, name, sizeof(s.name));
	Str_Copy(s.type, type, sizeof(s.type));
	Str_Copy(s.map, map, sizeof(s.map));
	Net_ServerLine(&s, line, sizeof(line));
	return PyString_FromString(line);
}

PyMethodDef g_MenuNetMethods[] = {
	{ "MultiPlayerMenu", PCMenu_MultiPlayerMenu, 1, NULL },
	{ "MenuRestartServer", PCMenu_MenuRestartServer, 1, NULL },
	{ "MenuJoinGame", PCMenu_MenuJoinGame, 1, NULL },
	{ "TryJoin", PCMenu_TryJoin, 1, NULL },
	{ "JoinPassword", PCMenu_JoinPassword, 1, NULL },
	{ "AskPassword", PCMenu_AskPassword, 1, NULL },
	{ "ShowConectionStatus", PCMenu_ShowConectionStatus, 1, NULL },
	{ "AbortJoin", PCMenu_AbortJoin, 1, NULL },
	{ "SpecifyIPMenu", PCMenu_SpecifyIPMenu, 1, NULL },
	{ "AbortMultiplayerGame", PCMenu_AbortMultiplayerGame, 1, NULL },
	{ "NetDefaultContentMenu", PCMenu_NetDefaultContentMenu, 1, NULL },
	{ "NetJoinGameMenu", PCMenu_NetJoinGameMenu, 1, NULL },
	{ "NetJoinInternetMenu", PCMenu_NetJoinInternetMenu, 1, NULL },
	{ "NetJoinLocalMenu", PCMenu_NetJoinLocalMenu, 1, NULL },
	{ "NetJoinIPMenu", PCMenu_NetJoinIPMenu, 1, NULL },
	{ "OnNetBrowse", PCMenu_OnNetBrowse, 1, NULL },
	{ "BrowserSortExecute", PCMenu_BrowserSortExecute, 1, NULL },
	{ "BrowseLocalMenu", PCMenu_BrowseLocalMenu, 1, NULL },
	{ "MasterCommand", PCMenu_MasterCommand, 1, NULL },
	{ "BrowseINetMenu", PCMenu_BrowseINetMenu, 1, NULL },
	{ "InetMasterBrowse", PCMenu_InetMasterBrowse, 1, NULL },
	{ "BrowserMenuBack", PCMenu_BrowserMenuBack, 1, NULL },
	{ "RefreshBrowseMenu", PCMenu_RefreshBrowseMenu, 1, NULL },
	{ "RefreshBrowseMenuButton", PCMenu_RefreshBrowseMenuButton, 1, NULL },
	{ "BrowseLocalMenuPrevious", PCMenu_BrowseLocalMenuPrevious, 1, NULL },
	{ "BrowseLocalMenuNext", PCMenu_BrowseLocalMenuNext, 1, NULL },
	{ "BrowseLocalMenuPgUp", PCMenu_BrowseLocalMenuPgUp, 1, NULL },
	{ "BrowseLocalMenuPgDown", PCMenu_BrowseLocalMenuPgDown, 1, NULL },
	{ "BrowseLocalJoin", PCMenu_BrowseLocalJoin, 1, NULL },
	{ "UnicodeParse", PCMenu_UnicodeParse, 1, NULL },
	{ "CreateBrowseMenu", PCMenu_CreateBrowseMenu, 1, NULL },
	{ "BrowserColumnBack_Click", PCMenu_BrowserColumnBack_Click, 1, NULL },
	{ "BrowserSliderGainFocus", PCMenu_BrowserSliderGainFocus, 1, NULL },
	{ "BrowserSliderLooseFocus", PCMenu_BrowserSliderLooseFocus, 1, NULL },
	{ "BrowserSliderOnRender", PCMenu_BrowserSliderOnRender, 1, NULL },
	{ "UpdateBrowseSlider", PCMenu_UpdateBrowseSlider, 1, NULL },
	{ "UpdateBrowseMenu", PCMenu_UpdateBrowseMenu, 1, NULL },
	{ "BrowseFormatServerLine", PCMenu_BrowseFormatServerLine, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
