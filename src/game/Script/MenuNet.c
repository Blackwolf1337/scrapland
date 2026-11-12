/*
 * The multiplayer menus of PCMenu in C (issue #2, step 3): MultiPlayerMenu, the join page with the server browser,
 * the address and password pages, the connection to a server, the create page with the advanced options and the
 * part the create and join pages share. The layout is data/menus/net.def, one section per page.
 *
 * Native C (docs/objlib-plan.md, stage 2): the state is the struct net (the servers of the browser, the page, the
 * sort, the message of the day), the functions call the engine in C; the PyMethodDef entries are adapters (the
 * argument formats as before). The variables of other modules are read where the C read them (Init.inMainMenu,
 * isShipEdit, Path, Scorer.MarginH and MarginV, PCMenu.MultiplayerMenu, Net.Modes, ModesMap and NetMaps),
 * Menu.fromMultiPlayer is written at once. Menu, PCMenu.AskPassword and Net.PurgeServerMapList are called through their
 * refs (PORT_CALL: directly while the attribute holds the port's function, else by name), and so are Scrap.SaveConfig
 * and Scrap.Exit (stage 7), their failures printed as before; the connection status schedules itself (a function
 * object of its own, OBJLIB 7; the scheduler takes its entry, Port_Scheduled). The $names of the sections are made on
 * native values (Mode_DefRunVa, stage 6 batches 6f and 6h); the .def runner takes them as a dict (OBJLIB 6) and the
 * steps their arguments as native values. Every function of the table but BrowseFormatServerLine has a native entry
 * (batch 6h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/filepak.h"
#include "scrap/game.h"
#include "scrap/input.h"
#include "scrap/net.h"
#include "scrap/nativeobj.h"
#include "scrap/scheduler.h"
#include "scrap/scorer.h"
#include "scrap/scrapfont.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/timer.h"

static const char net_def[] = {
#embed "../../../data/menus/net.def"
	, 0 };

#define NET_FILE "menus/net.def"
#define NET_SLOTS 13			/* the lines of the browser */
#define NET_MAX_SERVERS 512
#define NET_SLIDER_Y 136
#define NET_SLIDER_H 208
#define NET_MAP_SLOTS 9			/* the lines of the map list */
#define NET_MAX_MAPS 64
#define NET_MAP_SLIDER_Y 214
#define NET_MAP_SLIDER_H 124

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
	int mapIndex;			/* the first map shown on the create page */
} net = { .domain = "Internet", .sort = "Ping" };

/* the maps of a game, as the script kept them: the text of a Python list in an engine variable */
typedef struct {
	char names[NET_MAX_MAPS][0x40];
	int num;
} netMapList_t;

static void Net_RunMenu(int id, const char *section, int n, ...);
static void Net_RunPage(int id, const char *section, const char *tab);

/* the values every page may use, as values of t: NET_PAIRS(c) are their name and value pairs for Net_RunMenu
 * (NET_COMMON of them), other $names follow */
#define NET_COMMON 6
#define NET_PAIRS(c) "tab", c[0], "domain", c[1], "motd", c[2], "motdx", c[3], "motdy", c[4], "serveraddress", c[5]
static void Net_Common(modeVals_t *t, const char *tab, const nativeVal_t **c);
static void Net_JoinMenu(int id);

/* Menu.LinkUD (ud) or Menu.LinkLR (id, a, b) through the module, a failure printed */
static void Net_Link(byte ud, int id, const char *a, const char *b) {
	if (ud) {
		MODE_PRINTED(Menu, LinkUD, "iss", (long)id, a, b);
	} else {
		MODE_PRINTED(Menu, LinkLR, "iss", (long)id, a, b);
	}
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
		Net_Link(1, id, tab, count ? "BrowserSlot1" : "Refresh");
	}
	if (count) {
		char last[0x20];
		Str_Printf(last, sizeof(last), "BrowserSlot%d", count);
		Net_Link(1, id, last, "Refresh");
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
		MODE_PRINTED(PCMenu, AskPassword, "is", 0L, "");
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
		MODE_PRINTED(Menu, DrawMenuTitleBar, "ids", 0L, bar, title);
	}
	/* a full server ends the polling (the script deleted its own next call). The port's own function as a function
	 * object of its own, made once from its ref and held as a value (not the module attribute: a case that replaces
	 * PCMenu.ShowConectionStatus does not change the polling), without arguments; the scheduler takes its entry
	 * (Port_ScheduledOf) */
	if (!full) {
		static nativeVal_t status;
		if (!Native_IsObj(&status)) {
			NObj_NewPortFunc(&status, &PORT_REF(PCMenu, ShowConectionStatus));
		}
		if (Native_IsObj(&status)) {
			Mode_SchedFn(Timer_GetTime(&g_pWorld->timer) + 0.25f, &status, NULL, 0, "ConectionStatus");
		}
	}
}

static void Net_TryJoin(int id) {
	char address[0x80], title[0x200];
	const char *addr = GVar_GetStr("DefaultServerAddress");
	int port = (int)GVar_GetNum("DefaultServerPort", 0);

	Net_InitClient(addr, port);
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	Str_Printf(address, sizeof(address), "%s:%d", addr, port);
	/* the text has a %s for the address */
	const char *fmt = Script_Lang("Net_Menu_Connecting");
	const char *at = strstr(fmt, "%s");
	if (at) {
		Str_Printf(title, sizeof(title), "%.*s%s%s", (int)(at - fmt), fmt, address, at + 2);
	} else {
		Str_Copy(title, fmt, sizeof(title));
	}
	nativeVal_t v;
	Native_SetText(&v, title, (int)strlen(title));
	Net_RunMenu(id, "connecting", 1, "title", &v);
	Native_Free(&v);
	Net_ConnectionStatus();
}

/* ---- the steps of net.def ---- */

/* the entry of the game mode (PCMenu.MultiplayerMenu, set by the mode) is disabled when it has no function */
static void Step_InGame(int id, const nativeVal_t *args, int n) {
	nativeVal_t opt, tmp = { 0 };
	const nativeVal_t *func = NULL;
	(void)args;
	(void)n;
	/* PCMenu.MultiplayerMenu[1] as PySequence_GetItem takes it (a list that holds objects item by item) */
	if (Mode_ModGet("PCMenu", "MultiplayerMenu", &opt)) {
		Mode_Elems(&opt);
		func = Native_GetItem(&opt, 1, 0, &tmp);
	}
	if (func && func->type == NV_STR && !Native_Text(func)[0]) {
		MODE_PRINTED(Menu, DisableMenuItem, "iii", (long)id, 2L, 5L);
	}
	Native_ErrClear();
	if (func == &tmp) {
		Native_Free(&tmp);
	}
	Native_Free(&opt);
	Mode_ModPutI("Menu", "fromMultiPlayer", 1);
}

/* the tabs of the join page: the focus box over the whole button */
static void Step_TabWidth(int id, const nativeVal_t *args, int n) {
	const char *item = Script_CurrentItem();
	(void)args;
	(void)n;
	Script_SetNum(id, item, "FocusPivotX", -(82 - Script_GetNum(id, item, "W") / 2));
	Script_SetNum(id, item, "W", 165);
}

/* columntext key maxlen: the text of the column title, cut */
static void Step_ColumnText(int id, const nativeVal_t *args, int n) {
	const char *key;
	int maxlen;
	char text[0x200];
	if (!Mode_Parse(args, n, "si", &key, &maxlen)) {
		Native_ErrPrint();
		return;
	}
	Net_CutText(Script_Lang(key), maxlen, text, sizeof(text));
	Script_SetStr(id, Script_CurrentItem(), "Text", text);
}

static void Step_Browse(int id, const nativeVal_t *args, int n) {
	char column[0x10];
	(void)args;
	(void)n;
	Net_Refresh(id);
	net.sortOrder = 1;
	Str_Copy(column, net.sort, sizeof(column));
	Net_SortBy(id, column);
}


/* ---- the create page ---- */

/* "['Bank', 'FZ']" into list (the repr of a list of map names) */
static void Net_ParseMaps(const char *text, netMapList_t *list) {
	list->num = 0;
	for (const char *p = text; *p;) {
		if (*p != '\'' && *p != '"') {
			p++;
			continue;
		}
		char quote = *p++;
		const char *end = strchr(p, quote);
		if (!end) {
			break;
		}
		if (list->num < NET_MAX_MAPS) {
			int len = (int)(end - p) + 1;
			Str_Copy(list->names[list->num++], p, len < (int)sizeof(list->names[0]) ? len : (int)sizeof(list->names[0]));
		}
		p = end + 1;
	}
}

static void Net_GetServerMaps(netMapList_t *list) {
	Net_ParseMaps(GVar_GetStr("ServerMapList"), list);
}

static void Net_SetServerMaps(const netMapList_t *list) {
	char text[NET_MAX_MAPS * 0x44 + 4];
	int n = Str_Printf(text, sizeof(text), "[");
	for (int i = 0; i < list->num; i++) {
		n += Str_Printf(text + n, sizeof(text) - n, "%s'%s'", i ? ", " : "", list->names[i]);
	}
	Str_Printf(text + n, sizeof(text) - n, "]");
	GVar_SetStr("ServerMapList", text);
}

/* a list or other variable of the Net script into *out (read as the attribute of Net): 1, else 0 with None
 * when it is not there (no error); a list that holds objects is made native item by item */
static byte Net_Script(const char *var, nativeVal_t *out) {
	if (!Mode_ModGet("Net", var, out)) {
		Native_ErrClear();
		return 0;
	}
	Mode_Elems(out);
	return 1;
}

/* the C text of seq[i] (PySequence_GetItem; a character of a text goes into tmp, Native_Free it), NULL when it is no
 * text (the error of the item left set) */
static const char *Net_Item(const nativeVal_t *seq, int i, nativeVal_t *tmp) {
	const nativeVal_t *e = Native_GetItem(seq, i, 0, tmp);
	return e && e->type == NV_STR ? Native_Text(e) : NULL;
}

/* the maps of the current mode, Net.ModesMap[ServerType] (PyDict_GetItemString: nothing when ModesMap is no dict) */
static void Net_ModeMaps(netMapList_t *list) {
	nativeVal_t modes, key, sub = { 0 }, *maps = NULL;
	Net_Script("ModesMap", &modes);
	Native_SetStr(&key, GVar_GetStr("ServerType"));
	if (modes.type == NV_DICT) {
		maps = Native_DictGet(&modes, &key);
	} else if (Native_IsObj(&modes) && Mode_Sub(&modes, &key, &sub)) {
		/* OBJLIB 7: a dict that holds objects (handles) looked up the Python way */
		maps = &sub;
	}
	Native_Free(&key);
	list->num = 0;
	if (maps) {
		Mode_Elems(maps);
	}
	for (int i = 0; maps && i < Mode_SeqLen(maps) && list->num < NET_MAX_MAPS; i++) {
		nativeVal_t tmp = { 0 };
		const char *e = Net_Item(maps, i, &tmp);
		if (e) {
			Str_Copy(list->names[list->num++], e, sizeof(list->names[0]));
		}
		Native_Free(&tmp);
	}
	Native_ErrClear();
	Native_Free(&sub);
	Native_Free(&modes);
}

static byte Net_HasMap(const netMapList_t *list, const char *name) {
	for (int i = 0; i < list->num; i++) {
		if (!strcmp(list->names[i], name)) {
			return 1;
		}
	}
	return 0;
}

static void Net_UpdateMapSlider(int id) {
	netMapList_t maps;
	Net_ModeMaps(&maps);
	if (maps.num <= NET_MAP_SLOTS) {
		Script_SetNum(id, "MapSlider", "Y", NET_MAP_SLIDER_Y);
		Script_SetNum(id, "MapSlider", "ScaleY", NET_MAP_SLIDER_H);
		return;
	}
	double step = NET_MAP_SLIDER_H / (double)(maps.num - NET_MAP_SLOTS + 1);
	double size = step;
	if (size < 10) {
		size = 10;
		step = (NET_MAP_SLIDER_H - (size - step)) / (double)(maps.num - NET_MAP_SLOTS + 1);
	}
	Script_SetNum(id, "MapSlider", "Y", NET_MAP_SLIDER_Y + net.mapIndex * step);
	Script_SetNum(id, "MapSlider", "ScaleY", size);
}

/* the maps of the mode from mapIndex on; the chosen ones are lit */
static void Net_UpdateMapList(int id) {
	netMapList_t maps, chosen;
	char item[0x20], link[0x20], key[0x80], text[0x100];
	int slot = 0;

	Net_GetServerMaps(&chosen);
	for (int i = 0; i < NET_MAP_SLOTS; i++) {
		Str_Printf(item, sizeof(item), "MapSlot%d", i + 1);
		Script_SetStr(id, item, "Hint", "");
		Script_SetStr(id, item, "Text", "");
		Script_SetStr(id, item, "OnAccept", "");
		Script_SetStr(id, item, "OnGainFocus", "");
		Script_SetStr(id, item, "OnLooseFocus", "");
		Str_Printf(item, sizeof(item), "MapSlotBack%d", i + 1);
		Script_SetNum(id, item, "Visible", 0);
	}
	Net_ModeMaps(&maps);
	for (int i = net.mapIndex; i < maps.num && i - net.mapIndex < NET_MAP_SLOTS; i++) {
		const char *t = maps.names[i];
		Str_Printf(key, sizeof(key), "Station_%s", t);
		Str_Printf(item, sizeof(item), "MapSlot%d", slot + 1);
		Script_SetStr(id, item, "Hint", t);
		Script_SetStr(id, item, "OnAccept", "PCMenu.MapChoose");
		Script_SetStr(id, item, "OnGainFocus", "PCMenu.MapPreviewOn");
		Script_SetStr(id, item, "OnLooseFocus", "PCMenu.MapPreviewOff");
		byte selected = Net_HasMap(&chosen, t);
		Str_Printf(text, sizeof(text), "%c %s", selected ? 2 : 3, Script_Lang(key));
		Script_SetStr(id, item, "Text", text);
		if (selected) {
			Script_SetNum(id, item, "Red", 255);
			Script_SetNum(id, item, "Green", 241);
			Script_SetNum(id, item, "Blue", 174);
			Str_Printf(link, sizeof(link), "MapSlotBack%d", slot + 1);
			Script_SetNum(id, link, "Visible", 1);
		} else {
			Script_SetNum(id, item, "Red", 183);
			Script_SetNum(id, item, "Green", 220);
			Script_SetNum(id, item, "Blue", 255);
		}
		Str_Printf(link, sizeof(link), "MapSlot%d", slot + 2);
		Script_SetStr(id, item, "Down", link);
		slot++;
		Str_Printf(link, sizeof(link), "MapSlot%d", slot);
		Net_Link(0, id, link, slot <= NET_MAP_SLOTS / 2 ? "MapUp" : "MapDown");
	}
	Net_Link(0, id, "MapSlot1", "MapUp");
	Str_Printf(link, sizeof(link), "MapSlot%d", slot);
	Net_Link(0, id, link, "MapDown");
	Net_Link(1, id, link, "CreateButton");
	Net_UpdateMapSlider(id);
}

/* the name of the mode, the title of its limit and the place of the limit, the maps */
static void Net_UpdateServerType(int id) {
	char key[0x80], title[0x100];
	const char *type = GVar_GetStr("ServerType");
	float pos[2], size[2] = { 0.0f, 0.0f };

	Str_Printf(key, sizeof(key), "Net_Type_%s", type);
	const char *text = Script_Lang(key);
	Script_SetStr(id, "ServerType", "Text", *text ? text : type);
	Str_Printf(key, sizeof(key), "Menu_MultiPlayer_%s_Limit", type);
	text = Script_Lang(key);
	Str_Printf(title, sizeof(title), "%s:", *text ? text : "Limit");
	Font_TextSize(Font_Get("Horatio"), pos, size, 0.0f, 0.0f, "%s", title);
	Script_SetStr(id, "FragLimitTitle", "Text", title);
	Script_SetNum(id, "FragLimit", "X", Script_GetNum(id, "FragLimitTitle", "X") + size[0] + 10);
	MODE_PRINTED(Net, PurgeServerMapList, "");
	Net_UpdateMapList(id);
}

/* the modes of Net.Modes: the index of ServerType (-1 when it is none of them) and the number of modes */
static int Net_ModeIndex(int *count) {
	nativeVal_t modes;
	byte there = Net_Script("Modes", &modes);
	const char *type = GVar_GetStr("ServerType");
	int found = -1;
	*count = there ? Mode_SeqLen(&modes) : 0;
	for (int i = 0; i < *count; i++) {
		nativeVal_t tmp = { 0 };
		const char *e = Net_Item(&modes, i, &tmp);
		if (e && !strcmp(e, type)) {
			found = i;
		}
		Native_Free(&tmp);
	}
	Native_ErrClear();
	Native_Free(&modes);
	return found;
}

static void Net_SetMode(int index) {
	nativeVal_t modes, tmp = { 0 };
	const char *e = Net_Script("Modes", &modes) ? Net_Item(&modes, index, &tmp) : NULL;
	if (e) {
		GVar_SetStr("ServerType", e);
	}
	Native_ErrClear();
	Native_Free(&tmp);
	Native_Free(&modes);
}

static void Net_CreateMenu(int id) {
	int count;
	char players[0x10], frags[0x10];
	net.mapIndex = 0;
	Net_DoneBrowser();
	if (Net_ModeIndex(&count) < 0 && count > 0) {
		Net_SetMode(0);
	}
	const char *advanced = Script_Lang("Menu_Multiplayer_Advanced");
	Str_Printf(players, sizeof(players), "%d", (int)GVar_GetNum("DefaultMaxPlayersOnServer", 0) + 1);
	Str_Printf(frags, sizeof(frags), "%d", (int)GVar_GetNum("FragLimit", 0));
	modeVals_t t = { .n = 0 };
	const nativeVal_t *c[NET_COMMON];
	Net_Common(&t, "Create", c);
	Net_RunMenu(id, "Create", NET_COMMON + 4, NET_PAIRS(c), "servername", Mode_S(&t, GVar_GetStr("ServerName")),
		"players", Mode_S(&t, players), "fraglimit", Mode_S(&t, frags), "advanced",
		Mode_S(&t, *advanced ? advanced : "Advanced"));
	Mode_Done(&t, 1);
}

/* a text of the language, or the English one when the language has none */
static const char *Net_Text(const char *key, const char *english) {
	const char *s = Script_Lang(key);
	return *s ? s : english;
}

static void Net_DataRateText(char *out, int size) {
	const char *rate = Net_Text("Menu_Multiplayer_Advanced_DataRate", "Connection type");
	int limit = (int)GVar_GetNum("ServerDataRateLimit", 0);
	if (limit > 40960) {
		Str_Printf(out, size, "%s : LAN", rate);
	} else {
		Str_Printf(out, size, "%s : %d kbps", rate, limit / 80);
	}
}

static void Step_MapSlots(int id, const nativeVal_t *args, int n) {
	(void)args;
	(void)n;
	for (int i = 0; i < NET_MAP_SLOTS; i++) {
		char item[0x20], link[0x20];
		Str_Printf(item, sizeof(item), "MapSlotBack%d", i + 1);
		Script_Add(id, item, "Sprite");
		Script_SetNum(id, item, "Unit", 1);
		Script_SetNum(id, item, "X", 80);
		Script_SetNum(id, item, "Y", 199 + i * 17);
		Script_SetNum(id, item, "ScaleX", 230);
		Script_SetNum(id, item, "ScaleY", 16);
		Script_SetNum(id, item, "Red", 255);
		Script_SetNum(id, item, "Green", 255);
		Script_SetNum(id, item, "Blue", 222);
		Script_SetNum(id, item, "Alpha", 32);
		Script_SetNum(id, item, "Visible", 0);
		Str_Printf(item, sizeof(item), "MapSlot%d", i + 1);
		Script_Add(id, item, "Text");
		Script_SetStr(id, item, "Font", "HoratioSmall");
		Script_SetStr(id, item, "Text", "XXXXX");
		Script_SetNum(id, item, "X", 80);
		Script_SetNum(id, item, "Y", 197 + i * 17);
		Script_SetNum(id, item, "Red", 183);
		Script_SetNum(id, item, "Green", 220);
		Script_SetNum(id, item, "Blue", 255);
		Script_SetNum(id, item, "W", 230);
		Script_SetNum(id, item, "H", 22);
		Str_Printf(link, sizeof(link), "MapSlot%d", i);
		Script_SetStr(id, item, "Up", link);
		Str_Printf(link, sizeof(link), "MapSlot%d", i + 2);
		Script_SetStr(id, item, "Down", link);
		Script_SetStr(id, item, "Right", "MapUp");
	}
}

/* mappreviews x y: a hidden picture of each map of Net.NetMaps */
static void Step_MapPreviews(int id, const nativeVal_t *args, int n) {
	int x, y;
	if (!Mode_Parse(args, n, "ii", &x, &y)) {
		Native_ErrPrint();
		return;
	}
	nativeVal_t maps;
	byte there = Net_Script("NetMaps", &maps);
	for (int i = 0; there && i < Mode_SeqLen(&maps); i++) {
		nativeVal_t tmp = { 0 };
		const char *e = Net_Item(&maps, i, &tmp);
		if (e) {
			char item[0x80], file[0x100];
			Str_Printf(item, sizeof(item), "MapPreviewSlot_%s", e);
			Str_Printf(file, sizeof(file), "Levels/%s/2D/MapPreview.alpha.tga", e);
			Script_Add(id, item, "Sprite");
			Script_SetNum(id, item, "X", x);
			Script_SetNum(id, item, "Y", y);
			Script_SetStr(id, item, "File", file);
			Script_SetNum(id, item, "Visible", 0);
			Script_SetNum(id, item, "Alpha", 0);
		}
		Native_Free(&tmp);
	}
	Native_ErrClear();
	Native_Free(&maps);
}

static void Step_ServerType(int id, const nativeVal_t *args, int n) {
	(void)args;
	(void)n;
	Net_UpdateServerType(id);
}

static const scriptStep_t net_steps[] = {
	{ "ingame", Step_InGame },
	{ "tabwidth", Step_TabWidth },
	{ "columntext", Step_ColumnText },
	{ "browse", Step_Browse },
	{ "mapslots", Step_MapSlots },
	{ "mappreviews", Step_MapPreviews },
	{ "servertype", Step_ServerType },
	{ NULL, NULL },
};

/* a section of net.def with its $names: n pairs of a name and a const nativeVal_t * (Mode_DefRunVa; OBJLIB 6: the
 * runner takes them as a dict) */
static void Net_RunMenu(int id, const char *section, int n, ...) {
	va_list va;
	va_start(va, n);
	Mode_DefRunVa(id, NET_FILE, section, net_def, net_steps, 0, n, va);
	va_end(va);
}

static void Net_Common(modeVals_t *t, const char *tab, const nativeVal_t **c) {
	int marginH = (int)Mode_ModGetI("Scorer", "MarginH", 0);
	int marginV = (int)Mode_ModGetI("Scorer", "MarginV", 0);
	c[0] = Mode_S(t, tab);
	c[1] = Mode_S(t, net.domain);
	c[2] = Mode_S(t, net.motd);
	c[3] = Mode_I(t, marginH + 5);
	c[4] = Mode_I(t, 480 - marginV - 20);
	c[5] = Mode_S(t, GVar_GetStr("DefaultServerAddress"));
}

/* a section with the values every page may use and nothing else */
static void Net_RunPage(int id, const char *section, const char *tab) {
	modeVals_t t = { .n = 0 };
	const nativeVal_t *c[NET_COMMON];
	Net_Common(&t, tab, c);
	Net_RunMenu(id, section, NET_COMMON, NET_PAIRS(c));
	Mode_Done(&t, 1);
}

static void Net_JoinMenu(int id) {
	cScorer *sc = Scorer_Get(id);
	char current[0x80];
	modeVals_t t = { .n = 0 };
	const nativeVal_t *c[NET_COMMON];
	Str_Copy(current, sc && sc->current ? sc->current : "Join", sizeof(current));
	Net_Common(&t, "Join", c);
	Net_RunMenu(id, "Join", NET_COMMON + 1, NET_PAIRS(c), "current", Mode_S(&t, current));
	Mode_Done(&t, 1);
}

/* ---- the functions of PCMenu ---- */

#define NET_ARGS(a, n) \
	int id; \
	const char *control; \
	if (!Mode_IdControl(a, n, &id, &control)) { \
		return 0; \
	}

/* MultiPlayerMenu(id, control): the join page from the main menu and the editor, else the page of the game */
static void Net_MultiPlayerMenu(int id) {
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	if (Mode_ModGetI("Init", "inMainMenu", 0) || Mode_ModGetI("Init", "isShipEdit", 0)) {
		Net_JoinMenu(id);
		return;
	}
	nativeVal_t v;
	Net_RunMenu(id, "Multiplayer", 1, "server", Mode_IntVal(&v, Net_IsServer() != 0));
}

MODE_IDFN(PCMenu_MultiPlayerMenu, (Net_MultiPlayerMenu(id), 1))

static byte PCMenu_MenuRestartServer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)id;
	(void)control;
	char path[0x100];
	if (Timer_GetTime(&g_pWorld->timer) > 15 && Net_IsServer()) {
		Net_ServerChangeLevel(Mode_ModGetS("Init", "Path", path, sizeof(path)));
	}
	return 1;
}
MODE_ADAPTER(PCMenu, MenuRestartServer)

static byte PCMenu_MenuJoinGame_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
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
	return 1;
}
MODE_ADAPTER(PCMenu, MenuJoinGame)

static byte PCMenu_TryJoin_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	Net_TryJoin(id);
	return 1;
}
MODE_ADAPTER(PCMenu, TryJoin)

static byte PCMenu_JoinPassword_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	GVar_SetStr("ClientPassword", Script_GetStr(id, "Item1", "Text"));
	Net_TryJoin(id);
	return 1;
}
MODE_ADAPTER(PCMenu, JoinPassword)

/* AskPassword(id, control): the password page of the server */
static void Net_AskPassword(int id) {
	const char *title = Script_Lang("Menu_Multiplayer_Advanced_Password");
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	modeVals_t t = { .n = 0 };
	Net_RunMenu(id, "password", 2, "title", Mode_S(&t, *title ? title : "Password"), "password",
		Mode_S(&t, GVar_GetStr("ClientPassword")));
	Mode_Done(&t, 1);
}

MODE_IDFN(PCMenu_AskPassword, (Net_AskPassword(id), 1))

/* ShowConectionStatus(): any arguments */
MODE_FNANY(PCMenu, ShowConectionStatus, (Net_ConnectionStatus(), 1))

static byte PCMenu_AbortJoin_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	Scheduler_Delete(&g_pWorld->scheduler, "ConectionStatus");
	Net_CloseClient();
	Net_JoinMenu(id);
	return 1;
}
MODE_ADAPTER(PCMenu, AbortJoin)

static byte PCMenu_SpecifyIPMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	Net_RunPage(id, "ip", "Join");
	return 1;
}
MODE_ADAPTER(PCMenu, SpecifyIPMenu)

static byte PCMenu_AbortMultiplayerGame_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	MODE_PRINTED(Menu, YesNoMenu, "isss", (long)id, Script_Lang("Menu_AbortGame_Question"), "Menu.AuxAbortGame",
		"PCMenu.MultiPlayerMenu");
	return 1;
}
MODE_ADAPTER(PCMenu, AbortMultiplayerGame)

/* NetDefaultContentMenu(id, control[, tab]): Back, the garage and the Create/Join tabs, the message of the day */
static byte PCMenu_NetDefaultContentMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	const nativeVal_t *control;
	const char *tab = "Create";
	(void)out;
	if (!Mode_Parse(a, n, "iO|s", &id, &control, &tab)) {
		return 0;
	}
	Net_RunPage(id, "content", tab);
	return 1;
}
MODE_ADAPTER(PCMenu, NetDefaultContentMenu)

static byte PCMenu_NetJoinGameMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	Net_JoinMenu(id);
	return 1;
}
MODE_ADAPTER(PCMenu, NetJoinGameMenu)

static byte Net_Domain(const nativeVal_t *a, int n, const char *domain) {
	NET_ARGS(a, n);
	(void)control;
	Net_DoneBrowser();
	Str_Copy(net.domain, domain, sizeof(net.domain));
	Net_JoinMenu(id);
	return 1;
}

static byte PCMenu_NetJoinInternetMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Domain(a, n, "Internet");
}
MODE_ADAPTER(PCMenu, NetJoinInternetMenu)

static byte PCMenu_NetJoinLocalMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Domain(a, n, "Local");
}
MODE_ADAPTER(PCMenu, NetJoinLocalMenu)

static byte PCMenu_NetJoinIPMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Domain(a, n, "IP");
}
MODE_ADAPTER(PCMenu, NetJoinIPMenu)

/* OnNetBrowse(address, flags, name, players, maxPlayers, version, port, type, ping, map): a server answered */
static byte PCMenu_OnNetBrowse_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *address, *name, *type, *map;
	int flags, players, maxPlayers, version, port, ping;
	(void)out;
	if (!Mode_Parse(a, n, "sisiiiisis", &address, &flags, &name, &players, &maxPlayers, &version, &port, &type,
		&ping, &map)) {
		return 0;
	}
	if (net.numServers >= NET_MAX_SERVERS) {
		return 1;
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
	return 1;
}
MODE_ADAPTER(PCMenu, OnNetBrowse)

static byte PCMenu_BrowserSortExecute_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	Net_Sort();
	return 1;
}
MODE_ADAPTER(PCMenu, BrowserSortExecute)

static byte PCMenu_BrowseLocalMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	Net_BrowseLocal();
	return 1;
}
MODE_ADAPTER(PCMenu, BrowseLocalMenu)

/* MasterCommand(text): a message of the master server, "msg=" is the message of the day */
static byte PCMenu_MasterCommand_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *text;
	(void)out;
	if (!Mode_Parse(a, n, "s", &text)) {
		return 0;
	}
	if (!strncmp(text, "msg=", 4)) {
		Str_Copy(net.motd, text + 4, sizeof(net.motd));
		Script_SetStr(0, "MOTD", "Text", net.motd);
	}
	return 1;
}
MODE_ADAPTER(PCMenu, MasterCommand)

static byte PCMenu_BrowseINetMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)a;
	(void)n;
	(void)out;
	Net_BrowseInternet();
	return 1;
}
MODE_ADAPTER(PCMenu, BrowseINetMenu)

static byte PCMenu_InetMasterBrowse_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int i;
	if (!Mode_Parse(a, n, "i", &i)) {
		return 0;
	}
	Native_SetInt(out, Net_MasterBrowse(i));
	return 1;
}
MODE_ADAPTER(PCMenu, InetMasterBrowse)

/* the script called a MainMenu of its own module here, which does not exist; the main menu of Menu is meant */
static byte PCMenu_BrowserMenuBack_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cPy_Function_SetFunc(&sc->onNext, "");
		cPy_Function_SetFunc(&sc->onPrev, "");
	}
	Net_DoneBrowser();
	MODE_PRINTED(Menu, MainMenu, "is", (long)id, control);
	return 1;
}
MODE_ADAPTER(PCMenu, BrowserMenuBack)

static byte PCMenu_RefreshBrowseMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	Net_Refresh(id);
	return 1;
}
MODE_ADAPTER(PCMenu, RefreshBrowseMenu)

static byte PCMenu_RefreshBrowseMenuButton_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	Net_DoneBrowser();
	Net_Refresh(id);
	return 1;
}
MODE_ADAPTER(PCMenu, RefreshBrowseMenuButton)

/* the arrows and the page keys of the browser; the names are the script's (Previous scrolls down) */
static byte Net_Scroll(const nativeVal_t *a, int n, int how) {
	NET_ARGS(a, n);
	(void)control;
	int count = net.numServers;
	if (how == 0) {
		if (count <= net.index + NET_SLOTS) {
			return 1;
		}
		net.index++;
	} else if (how == 1) {
		if (net.index <= 0) {
			return 1;
		}
		net.index--;
	} else if (how == 2) {
		net.index = count <= NET_SLOTS ? 0 : count > net.index + NET_SLOTS * 2 ? net.index + NET_SLOTS :
			count - NET_SLOTS;
	} else {
		net.index = net.index > NET_SLOTS - 1 ? net.index - NET_SLOTS : 0;
	}
	Net_UpdateBrowser(id);
	return 1;
}

static byte PCMenu_BrowseLocalMenuPrevious_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Scroll(a, n, 0);
}
MODE_ADAPTER(PCMenu, BrowseLocalMenuPrevious)

static byte PCMenu_BrowseLocalMenuNext_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Scroll(a, n, 1);
}
MODE_ADAPTER(PCMenu, BrowseLocalMenuNext)

static byte PCMenu_BrowseLocalMenuPgUp_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Scroll(a, n, 2);
}
MODE_ADAPTER(PCMenu, BrowseLocalMenuPgUp)

static byte PCMenu_BrowseLocalMenuPgDown_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Scroll(a, n, 3);
}
MODE_ADAPTER(PCMenu, BrowseLocalMenuPgDown)

/* BrowseLocalJoin(id, "BrowserSlotN"): joins the server of that line when its mode and map are here */
static byte PCMenu_BrowseLocalJoin_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	if (strlen(control) <= 11) {
		return 1;
	}
	int i = atoi(control + 11) - 1 + net.index;
	if (i < 0 || i >= net.numServers) {
		return 1;
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
			return 1;
		}
	}
	GVar_SetNum("DefaultServerPort", s->port);
	Net_DoneBrowser();
	Net_TryJoin(id);
	return 1;
}
MODE_ADAPTER(PCMenu, BrowseLocalJoin)

/* UnicodeParse(text, maxlen) */
static byte PCMenu_UnicodeParse_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	const char *text;
	int maxlen;
	char cut[0x400];
	if (!Mode_Parse(a, n, "si", &text, &maxlen)) {
		return 0;
	}
	Net_CutText(text, maxlen, cut, sizeof(cut));
	Native_SetText(out, cut, (int)strlen(cut));
	return 1;
}
MODE_ADAPTER(PCMenu, UnicodeParse)

static byte PCMenu_CreateBrowseMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Net_RunPage(id, "browser", "Join");
	return 1;
}
MODE_ADAPTER(PCMenu, CreateBrowseMenu)

static byte PCMenu_BrowserColumnBack_Click_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	Net_SortBy(id, strlen(control) > 18 ? control + 18 : "");
	return 1;
}
MODE_ADAPTER(PCMenu, BrowserColumnBack_Click)

/* the slider area (id, control, previous) draws its handle while it has the focus */
static byte Net_SliderFocus(const nativeVal_t *a, int n, const char *render) {
	int id;
	const nativeVal_t *x, *y;
	if (!Mode_Parse(a, n, "iOO", &id, &x, &y)) {
		return 0;
	}
	Script_SetStr(id, "BrowserSliderArea", "OnRender", render);
	return 1;
}

static byte PCMenu_BrowserSliderGainFocus_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_SliderFocus(a, n, "PCMenu.BrowserSliderOnRender");
}
MODE_ADAPTER(PCMenu, BrowserSliderGainFocus)

static byte PCMenu_BrowserSliderLooseFocus_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_SliderFocus(a, n, "");
}
MODE_ADAPTER(PCMenu, BrowserSliderLooseFocus)

/* while the slider has the focus and accept is held, the cursor height picks the first server shown */
static byte PCMenu_BrowserSliderOnRender_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	int count = net.numServers;
	if (!Input_MenuAccept(id) || count <= NET_SLOTS) {
		return 1;
	}
	double y = Script_GetNum(id, "Cursor", "Y") - NET_SLIDER_Y - 5;
	if (y < 0) {
		y = 0;
	}
	if (y > NET_SLIDER_H - 10) {
		y = NET_SLIDER_H - 10;
	}
	int old = net.index;
	net.index = (int)((double)(count - NET_SLOTS + 1) * (y / (NET_SLIDER_H - 10)));
	if (net.index >= count - NET_SLOTS) {
		net.index = count - NET_SLOTS;
	}
	if (old != net.index) {
		Net_UpdateBrowser(id);
	}
	return 1;
}
MODE_ADAPTER(PCMenu, BrowserSliderOnRender)

static byte PCMenu_UpdateBrowseSlider_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Net_UpdateSlider(id);
	return 1;
}
MODE_ADAPTER(PCMenu, UpdateBrowseSlider)

static byte PCMenu_UpdateBrowseMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Net_UpdateBrowser(id);
	return 1;
}
MODE_ADAPTER(PCMenu, UpdateBrowseMenu)

/* BrowseFormatServerLine(info): info as the script kept it, (ping, address, name, players, max, version, port,
 * type, map, flags). OBJLIB 7: the adapter parses with PyArg (a tuple format with texts in it, "(issiiiissi)":
 * Mode_Parse takes numbers only inside a tuple, and the errors of the nested format are those of getargs.c), so the
 * function has no entry */
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
	nativeVal_t r;
	Native_SetText(&r, line, (int)strlen(line));
	return Mode_Return(1, &r);
}

static byte PCMenu_NetCreateGameMenu_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	Net_CreateMenu(id);
	return 1;
}
MODE_ADAPTER(PCMenu, NetCreateGameMenu)

/* MenuInitServer: starts a server on the first chosen map (all maps of the mode when none is chosen) */
static byte PCMenu_MenuInitServer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	char level[0x100];
	netMapList_t maps;
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	Net_GetServerMaps(&maps);
	if (maps.num <= 0) {
		Net_ModeMaps(&maps);
		Net_SetServerMaps(&maps);
		Net_UpdateMapList(id);
	}
	if (maps.num <= 0) {
		Native_Raise(NE_INDEX, "MenuInitServer: no maps");
		return 0;
	}
	Str_Printf(level, sizeof(level), "Levels/%s", maps.names[0]);
	Net_InitServer(level, (int)GVar_GetNum("DefaultMaxPlayersOnServer", 0), 0, 0);
	return 1;
}
MODE_ADAPTER(PCMenu, MenuInitServer)

static byte PCMenu_AdvancedServer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	char pass[0x100], force[0x100], rate[0x100];
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	MODE_PRINTED(Menu, StartNewMenu, "i", (long)id);
	byte usePass = GVar_GetNum("UseServerPassword", 0) != 0;
	const char *password = Net_Text("Menu_Multiplayer_Advanced_Password", "Password");
	Str_Printf(pass, sizeof(pass), usePass ? "\x02 %s:" : "\x03 %s", password);
	Str_Printf(force, sizeof(force), "%c %s", GVar_GetNum("ForceServerVehicle", 0) ? 2 : 3,
		Net_Text("Menu_Multiplayer_Advanced_Force", "Force server vehicle"));
	Net_DataRateText(rate, sizeof(rate));
	modeVals_t t = { .n = 0 };
	Net_RunMenu(id, "Advanced", 6,
		"title", Mode_S(&t, Net_Text("Menu_Multiplayer_Advanced_Options", "Advanced Options")),
		"pass", Mode_S(&t, pass), "password", Mode_S(&t, GVar_GetStr("ServerPassword")), "force", Mode_S(&t, force),
		"datarate", Mode_S(&t, rate), "usepass", Mode_I(&t, usePass));
	Mode_Done(&t, 1);
	return 1;
}
MODE_ADAPTER(PCMenu, AdvancedServer)

static byte PCMenu_BackToCreateServer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	GVar_SetStr("ServerPassword", Script_GetStr(id, "Item2", "Text"));
	Net_CreateMenu(id);
	return 1;
}
MODE_ADAPTER(PCMenu, BackToCreateServer)

static byte PCMenu_SwitchForceVehicle_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	char text[0x100];
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	byte on = GVar_GetNum("ForceServerVehicle", 0) == 0;
	Str_Printf(text, sizeof(text), "%c %s", on ? 2 : 3, Net_Text("Menu_Multiplayer_Advanced_Force", "Force server vehicle"));
	Script_SetStr(id, "Item3", "Text", text);
	GVar_SetNum("ForceServerVehicle", on);
	return 1;
}
MODE_ADAPTER(PCMenu, SwitchForceVehicle)

static byte PCMenu_SwitchUsePass_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	char text[0x100];
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	const char *password = Net_Text("Menu_Multiplayer_Advanced_Password", "Password");
	if (GVar_GetNum("UseServerPassword", 0)) {
		GVar_SetNum("UseServerPassword", 0);
		Str_Printf(text, sizeof(text), "\x03 %s", password);
		Script_SetStr(id, "Item1", "Text", text);
		Script_SetNum(id, "Item2", "Visible", 0);
		Script_SetStr(id, "Item1", "Right", "");
		Script_SetStr(id, "Item1", "Left", "");
	} else {
		GVar_SetNum("UseServerPassword", 1);
		Str_Printf(text, sizeof(text), "\x02 %s:", password);
		Script_SetStr(id, "Item1", "Text", text);
		Net_Link(0, id, "Item1", "Item2");
		Net_Link(0, id, "Item2", "Item1");
		Script_SetNum(id, "Item2", "Visible", 1);
	}
	return 1;
}
MODE_ADAPTER(PCMenu, SwitchUsePass)

/* RunDedicatedServer: not in a menu of the game; the game ends and starts again as a dedicated server */
static byte PCMenu_RunDedicatedServer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	char params[0x100];
	netMapList_t maps;
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	Net_GetServerMaps(&maps);
	if (maps.num <= 0) {
		Net_ModeMaps(&maps);
		Net_SetServerMaps(&maps);
	}
	GVar_SetStr("ServerPassword", Script_GetStr(id, "Item2", "Text"));
	MODE_PRINTED(Scrap, SaveConfig, "");
	Str_Printf(params, sizeof(params), "-console -dedicated -imap:\"Levels/%s\"", maps.num ? maps.names[0] : "");
	GVar_SetStr("ReRunAtExitParams", params);
	MODE_PRINTED(Scrap, Exit, "");
	return 1;
}
MODE_ADAPTER(PCMenu, RunDedicatedServer)

static byte PCMenu_SetDataRate_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	static const int rates[] = { 2560, 5120, 10240, 15360, 20480, 30720, 40960, 409600000 };
	char text[0x100];
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	int limit = (int)GVar_GetNum("ServerDataRateLimit", 0);
	int next = rates[0];
	for (int i = 0; i < 7; i++) {
		if (limit == rates[i]) {
			next = rates[i + 1];
		}
	}
	GVar_SetNum("ServerDataRateLimit", next);
	Net_DataRateText(text, sizeof(text));
	Script_SetStr(id, "Item4", "Text", text);
	return 1;
}
MODE_ADAPTER(PCMenu, SetDataRate)

static byte PCMenu_GetDataRateText_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	char text[0x100];
	(void)a;
	(void)n;
	Net_DataRateText(text, sizeof(text));
	Native_SetText(out, text, (int)strlen(text));
	return 1;
}
MODE_ADAPTER(PCMenu, GetDataRateText)

/* the previous and next mode of Net.Modes, round */
static byte Net_StepMode(const nativeVal_t *a, int n, int dir) {
	int count;
	NET_ARGS(a, n);
	(void)control;
	int i = Net_ModeIndex(&count);
	if (count > 0) {
		Net_SetMode(i < 0 ? 0 : (i + dir + count) % count);
	}
	net.mapIndex = 0;
	Net_UpdateServerType(id);
	return 1;
}

static byte PCMenu_ServerTypePrev_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_StepMode(a, n, -1);
}
MODE_ADAPTER(PCMenu, ServerTypePrev)

static byte PCMenu_ServerTypeNext_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_StepMode(a, n, 1);
}
MODE_ADAPTER(PCMenu, ServerTypeNext)

static byte PCMenu_UpdateServerType_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Net_UpdateServerType(id);
	return 1;
}
MODE_ADAPTER(PCMenu, UpdateServerType)

/* the help text of the mode in a box, its name above */
static byte PCMenu_ServerTypeInfo_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	char key[0x80], nameKey[0x80];
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	const char *type = GVar_GetStr("ServerType");
	Str_Printf(nameKey, sizeof(nameKey), "Net_Type_%s", type);
	Str_Printf(key, sizeof(key), "Net_Help_%s", type);
	const char *name = Script_Lang(nameKey);
	const char *help = Script_Lang(key);
	if (!*help) {
		name = type;
		help = "Unofficial modification!";
	}
	MODE_PRINTED(Menu, ShowTextBox, "isss", (long)id, help, "PCMenu.NetCreateGameMenu", "PCMenu.NetCreateGameMenu");
	double y = Script_GetNum(id, "MissionDesc", "Y");
	Script_SetNum(id, "MissionDesc", "Y", y + 45);
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cScorer_AddItem(sc, "ServerTypeInfo", "Text", 1);
	}
	Script_SetStr(id, "ServerTypeInfo", "Font", "ScrapMedium");
	Script_SetStr(id, "ServerTypeInfo", "Text", name);
	Script_SetNum(id, "ServerTypeInfo", "X", 320);
	Script_SetNum(id, "ServerTypeInfo", "Y", y + 22);
	Script_SetStr(id, "ServerTypeInfo", "Align", "Center");
	Script_SetNum(id, "ServerTypeInfo", "Visible", 1);
	Script_SetStr(id, "ServerTypeInfo", "Effect", "Outline");
	Script_SetNum(id, "ServerTypeInfo", "Red", 160);
	Script_SetNum(id, "ServerTypeInfo", "Green", 180);
	Script_SetNum(id, "ServerTypeInfo", "Blue", 255);
	return 1;
}
MODE_ADAPTER(PCMenu, ServerTypeInfo)

/* the map arrows; the names are the script's (MapUp scrolls down) */
static byte PCMenu_MapUp_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	netMapList_t maps;
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	Net_ModeMaps(&maps);
	if (maps.num > net.mapIndex + NET_MAP_SLOTS) {
		net.mapIndex++;
		Net_UpdateMapList(id);
	}
	return 1;
}
MODE_ADAPTER(PCMenu, MapUp)

static byte PCMenu_MapDown_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	if (net.mapIndex > 0) {
		net.mapIndex--;
		Net_UpdateMapList(id);
	}
	return 1;
}
MODE_ADAPTER(PCMenu, MapDown)

/* OnGainFocus/OnLooseFocus (id, a, b) of an item: its OnRender on or off */
static byte Net_Render(const nativeVal_t *a, int n, const char *item, const char *func) {
	int id;
	const char *x, *y;
	if (!Mode_Parse(a, n, "iss", &id, &x, &y)) {
		return 0;
	}
	/* the map slider names its item, the edit fields get it as the first argument */
	Script_SetStr(id, item ? item : x, "OnRender", func);
	return 1;
}

static byte PCMenu_MapSliderGainFocus_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Render(a, n, "MapSliderArea", "PCMenu.MapSliderOnRender");
}
MODE_ADAPTER(PCMenu, MapSliderGainFocus)

static byte PCMenu_MapSliderLooseFocus_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Render(a, n, "MapSliderArea", "");
}
MODE_ADAPTER(PCMenu, MapSliderLooseFocus)

static byte PCMenu_MapSliderOnRender_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	netMapList_t maps;
	(void)out;
	NET_ARGS(a, n);
	(void)control;
	Net_ModeMaps(&maps);
	if (!Input_MenuAccept(id) || maps.num <= NET_MAP_SLOTS) {
		return 1;
	}
	double y = Script_GetNum(id, "Cursor", "Y") - NET_MAP_SLIDER_Y - 5;
	if (y < 0) {
		y = 0;
	}
	if (y > NET_MAP_SLIDER_H - 10) {
		y = NET_MAP_SLIDER_H - 10;
	}
	int old = net.mapIndex;
	net.mapIndex = (int)((double)(maps.num - NET_MAP_SLOTS + 1) * (y / (NET_MAP_SLIDER_H - 10)));
	if (net.mapIndex >= maps.num - NET_MAP_SLOTS) {
		net.mapIndex = maps.num - NET_MAP_SLOTS;
	}
	if (old != net.mapIndex) {
		Net_UpdateMapList(id);
	}
	return 1;
}
MODE_ADAPTER(PCMenu, MapSliderOnRender)

static byte PCMenu_UpdateMapSlider_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Net_UpdateMapSlider(id);
	return 1;
}
MODE_ADAPTER(PCMenu, UpdateMapSlider)

static byte PCMenu_UpdateMapList_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Net_UpdateMapList(id);
	return 1;
}
MODE_ADAPTER(PCMenu, UpdateMapList)

/* MapChoose(id, "MapSlotN"): the map of the line in or out of the maps of the server */
static byte PCMenu_MapChoose_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	netMapList_t maps, chosen = { .num = 0 };
	(void)out;
	char name[0x40];
	NET_ARGS(a, n);
	Str_Copy(name, Script_GetStr(id, control, "Hint"), sizeof(name));
	Net_GetServerMaps(&maps);
	byte selected = 0;
	for (int i = 0; i < maps.num; i++) {
		if (!strcmp(maps.names[i], name)) {
			selected = 1;
		} else {
			Str_Copy(chosen.names[chosen.num++], maps.names[i], sizeof(chosen.names[0]));
		}
	}
	if (!selected && chosen.num < NET_MAX_MAPS) {
		Str_Copy(chosen.names[chosen.num++], name, sizeof(chosen.names[0]));
	}
	Net_SetServerMaps(&chosen);
	Net_UpdateMapList(id);
	return 1;
}
MODE_ADAPTER(PCMenu, MapChoose)

static void Net_Fade(int id, const char *map, const char *how) {
	char item[0x80];
	Str_Printf(item, sizeof(item), "MapPreviewSlot_%s", map);
	Script_SetNum(id, item, "Visible", 1);
	Script_SetStr(id, item, "OnRender", how);
}

/* MapPreviewOn(id, control, prev): the picture of the map fades in, the one of the previous line out */
static byte PCMenu_MapPreviewOn_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	char *control, *prev, last[0x40], map[0x40];
	(void)out;
	if (!Mode_Parse(a, n, "iss", &id, &control, &prev)) {
		return 0;
	}
	Str_Copy(last, Script_GetStr(id, prev, "Hint"), sizeof(last));
	Str_Copy(map, Script_GetStr(id, control, "Hint"), sizeof(map));
	if (last[0]) {
		Net_Fade(id, last, "!ScorerSpriteFadeOut");
	}
	Net_Fade(id, map, "!ScorerSpriteFadeIn");
	return 1;
}
MODE_ADAPTER(PCMenu, MapPreviewOn)

static byte PCMenu_MapPreviewOff_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	char *control, *prev, map[0x40];
	(void)out;
	if (!Mode_Parse(a, n, "iss", &id, &control, &prev)) {
		return 0;
	}
	Str_Copy(map, Script_GetStr(id, control, "Hint"), sizeof(map));
	if (map[0]) {
		Net_Fade(id, map, "!ScorerSpriteFadeOut");
	}
	return 1;
}
MODE_ADAPTER(PCMenu, MapPreviewOff)

static byte PCMenu_ServerNameGainFocus_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Render(a, n, NULL, "PCMenu.ServerNameOnRender");
}
MODE_ADAPTER(PCMenu, ServerNameGainFocus)

/* the LooseFocus of the edit fields (ServerName, PlayersNumber, FragLimit): no OnRender */
static byte PCMenu_EditLooseFocus_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Render(a, n, NULL, "");
}
MODE_ADAPTER(PCMenu, EditLooseFocus)

static byte PCMenu_PlayersNumberGainFocus_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Render(a, n, NULL, "PCMenu.PlayersNumberOnRender");
}
MODE_ADAPTER(PCMenu, PlayersNumberGainFocus)

static byte PCMenu_FragLimitGainFocus_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	return Net_Render(a, n, NULL, "PCMenu.FragLimitOnRender");
}
MODE_ADAPTER(PCMenu, FragLimitGainFocus)

/* while an edit field has the focus its text goes to the engine variable each frame */
static byte PCMenu_ServerNameOnRender_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	GVar_SetStr("ServerName", Script_GetStr(id, control, "Text"));
	return 1;
}
MODE_ADAPTER(PCMenu, ServerNameOnRender)

/* the number in the field kept in min..max ("" counts as min) */
static int Net_Clamp(int id, const char *control, int min, int max) {
	char text[0x20];
	int v = atoi(Script_GetStr(id, control, "Text"));
	if (!Script_GetStr(id, control, "Text")[0] || v < min || v > max) {
		v = v > max ? max : min;
		Str_Printf(text, sizeof(text), "%d", v);
		Script_SetStr(id, control, "Text", text);
	}
	return v;
}

static byte PCMenu_PlayersNumberOnRender_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	GVar_SetNum("DefaultMaxPlayersOnServer", Net_Clamp(id, control, 2, 16) - 1);
	return 1;
}
MODE_ADAPTER(PCMenu, PlayersNumberOnRender)

static byte PCMenu_FragLimitOnRender_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	NET_ARGS(a, n);
	GVar_SetNum("FragLimit", Net_Clamp(id, control, 1, 99));
	return 1;
}
MODE_ADAPTER(PCMenu, FragLimitOnRender)

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
	{ "NetCreateGameMenu", PCMenu_NetCreateGameMenu, 1, NULL },
	{ "MenuInitServer", PCMenu_MenuInitServer, 1, NULL },
	{ "AdvancedServer", PCMenu_AdvancedServer, 1, NULL },
	{ "BackToCreateServer", PCMenu_BackToCreateServer, 1, NULL },
	{ "SwitchForceVehicle", PCMenu_SwitchForceVehicle, 1, NULL },
	{ "SwitchUsePass", PCMenu_SwitchUsePass, 1, NULL },
	{ "RunDedicatedServer", PCMenu_RunDedicatedServer, 1, NULL },
	{ "SetDataRate", PCMenu_SetDataRate, 1, NULL },
	{ "GetDataRateText", PCMenu_GetDataRateText, 1, NULL },
	{ "ServerTypePrev", PCMenu_ServerTypePrev, 1, NULL },
	{ "ServerTypeNext", PCMenu_ServerTypeNext, 1, NULL },
	{ "UpdateServerType", PCMenu_UpdateServerType, 1, NULL },
	{ "ServerTypeInfo", PCMenu_ServerTypeInfo, 1, NULL },
	{ "MapUp", PCMenu_MapUp, 1, NULL },
	{ "MapDown", PCMenu_MapDown, 1, NULL },
	{ "MapSliderGainFocus", PCMenu_MapSliderGainFocus, 1, NULL },
	{ "MapSliderLooseFocus", PCMenu_MapSliderLooseFocus, 1, NULL },
	{ "MapSliderOnRender", PCMenu_MapSliderOnRender, 1, NULL },
	{ "UpdateMapSlider", PCMenu_UpdateMapSlider, 1, NULL },
	{ "UpdateMapList", PCMenu_UpdateMapList, 1, NULL },
	{ "MapChoose", PCMenu_MapChoose, 1, NULL },
	{ "MapPreviewOn", PCMenu_MapPreviewOn, 1, NULL },
	{ "MapPreviewOff", PCMenu_MapPreviewOff, 1, NULL },
	{ "ServerNameGainFocus", PCMenu_ServerNameGainFocus, 1, NULL },
	{ "ServerNameLooseFocus", PCMenu_EditLooseFocus, 1, NULL },
	{ "ServerNameOnRender", PCMenu_ServerNameOnRender, 1, NULL },
	{ "PlayersNumberGainFocus", PCMenu_PlayersNumberGainFocus, 1, NULL },
	{ "PlayersNumberLooseFocus", PCMenu_EditLooseFocus, 1, NULL },
	{ "PlayersNumberOnRender", PCMenu_PlayersNumberOnRender, 1, NULL },
	{ "FragLimitGainFocus", PCMenu_FragLimitGainFocus, 1, NULL },
	{ "FragLimitLooseFocus", PCMenu_EditLooseFocus, 1, NULL },
	{ "FragLimitOnRender", PCMenu_FragLimitOnRender, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(MenuNet)
