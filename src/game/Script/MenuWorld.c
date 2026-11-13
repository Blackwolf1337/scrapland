/*
 * The pages of the worlds of the new network, see include/scrap/worldmenu.h. Not in the original.
 *
 * The join page lists worlds (the answers of LAN discovery, or the worlds of the list server with the ping of a
 * direct query each) in the browser of the old pages, or takes an address; the host page picks the start level, the
 * mode, PvP, the list server, the number of players and the port. The name is the PlayerName of the garage. A join
 * or host shows a wait page until the level of the world loads; a failure, or a world that ended later, shows its
 * text in a box.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/coord.h"
#include "scrap/coordlist.h"
#include "scrap/filesys.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/net.h"
#include "scrap/net2.h"
#include "scrap/platform.h"
#include "scrap/pyfunction.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/texts.h"
#include "scrap/travel.h"
#include "scrap/worldmenu.h"

#define WORLD_MAX 256				/* worlds in the list */
#define WORLD_LEVELS 16
#define WORLD_MAX_PLAYERS 64
#define WORLD_CLOSE_MS 2000			/* the browser's socket closes this long after its page went */
#define WORLD_MESSAGE_MS 1000
#define WORLD_SEARCH_MS 1500		/* the answers of a search come in this time */		/* a world that ended: its text this long after the main menu is up */

enum { WP_LAN, WP_LIST, WP_ADDRESS, WP_HOST };
enum { WL_IDLE, WL_CONNECTING, WL_PAGES };
static const char *const world_pages[] = { "Lan", "List", "Address", "Host" };
static const char *const world_modes[] = { "sandbox", "freeroam" };

typedef struct {
	Net2Addr addr;
	char name[NET2_INFO_NAME + 1], map[NET2_INFO_MAP + 1], mode[NET2_INFO_MODE + 1];
	int players, maxPlayers;
	int ping;						/* ms, -1 no answer yet */
	uint8_t key[NET2_KEY_SIZE];		/* the identity key the list server gave */
	byte hasKey;
} worldEntry_t;

static struct {
	byte registered;
	int page;						/* the join page's list or address, or the host page */
	int joinPage;					/* the last page of the join tab */
	worldEntry_t e[WORLD_MAX];
	int n, index;
	char sort[16];
	int sortOrder;
	Net2Host *net;					/* the browser: LAN queries, pings, the list server */
	uint64_t gone;					/* ms since the browser's page is not shown, 0 shown */
	char motd[160];
	char address[64];
	int level, mode;
	byte pvp, listed;
	int maxPlayers, port;
	uint64_t searchEnd;				/* ms: the count shows after this when nothing answered */
	int listPhase;					/* WL_* */
	byte waiting;					/* the wait page of a join or host of these pages */
	uint64_t menuUp;				/* ms the main menu runs since (a world that ended), 0 */
} wm = { .sort = "Ping", .address = "127.0.0.1", .pvp = 1, .listed = 1, .maxPlayers = 16 };

static char world_levels[WORLD_LEVELS][COORD_LEVEL + 1];
static int world_numLevels;

static uint64_t World_Ms(void) {
	return Plat_RealNs() / 1000000;
}

static uint16_t World_Port(void) {
	int p = Game_OptionOr("worldport", NET2_DEFAULT_PORT);
	return (uint16_t)(p > 0 && p < 65536 ? p : NET2_DEFAULT_PORT);
}

/* a text of the texts table with one %s or %d filled in */
static void World_Format(char *out, int size, const char *key, const char *s, int d) {
	const char *fmt = Script_Lang(key);
	const char *at = strchr(fmt, '%');
	if (at && (at[1] == 's' || at[1] == 'd')) {
		char arg[96];
		if (at[1] == 's') {
			Str_Copy(arg, s, sizeof(arg));
		} else {
			Str_PrintfQuiet(arg, sizeof(arg), "%d", d);
		}
		Str_PrintfQuiet(out, size, "%.*s%s%s", (int)(at - fmt), fmt, arg, at + 2);
	} else {
		Str_Copy(out, fmt, size);
	}
}

static void World_Status(const char *text) {
	Str_Copy(wm.motd, text, sizeof(wm.motd));
	if (Script_HasItem(0, "MOTD")) {
		Script_SetStr(0, "MOTD", "Text", wm.motd);
	}
}

/* the player and the item of a native call of the scorer */
static int World_Args(cAbstractFile *params, char *item, int size) {
	int id = 0;
	VFile_Read(params, &id, 4);
	cAbstractFile_ReadString(params, item, size - 1);
	return id;
}

/* the name typed on a page of the worlds goes to the garage's PlayerName */
static void World_KeepName(int id) {
	if (Script_HasItem(id, "WorldName")) {
		GVar_SetStr("PlayerName", Script_GetStr(id, "WorldName", "Text"));
	}
}

static const char *World_Name(void) {
	const char *n = GVar_GetStr("PlayerName");
	return n && *n ? n : "Player";
}

/* ---- the list ---- */

static int World_Compare(const void *pa, const void *pb) {
	const worldEntry_t *a = pa, *b = pb;
	int c;
	if (!strcmp(wm.sort, "Name")) {
		c = strcmp(a->name, b->name);
	} else if (!strcmp(wm.sort, "Type")) {
		c = strcmp(a->mode, b->mode);
	} else if (!strcmp(wm.sort, "Players")) {
		c = (b->players > a->players) - (b->players < a->players);
	} else if (!strcmp(wm.sort, "Map")) {
		c = strcmp(a->map, b->map);
	} else {
		/* no answer yet last */
		unsigned pa2 = (unsigned)a->ping, pb2 = (unsigned)b->ping;
		c = (pa2 > pb2) - (pa2 < pb2);
	}
	if (!c) {
		c = memcmp(&a->addr, &b->addr, sizeof(a->addr));
	}
	return wm.sortOrder ? -c : c;
}

static void World_Line(int i, char *out, int size) {
	const worldEntry_t *e = &wm.e[i];
	char ping[16], mode[0x40];
	Str_PrintfQuiet(ping, sizeof(ping), e->ping < 0 ? "?" : "%d", e->ping);
	Str_PrintfQuiet(mode, sizeof(mode), "MP_Mode_%s", e->mode);
	const char *m = Texts_Get(mode);
	Net_BrowserLine(e->name, m ? m : e->mode, 0, e->players, e->maxPlayers, e->map, ping, out, size);
}

static void World_Fill(void) {
	if (!Script_HasItem(0, "BrowserSlot1") || !Script_HasItem(0, "WorldButton_Lan")) {
		return;
	}
	qsort(wm.e, (size_t)wm.n, sizeof(wm.e[0]), World_Compare);
	Net_FillBrowser(0, wm.n, wm.index, World_Line, wm.page == WP_LIST ? "WorldList" : "WorldLan");
}

static worldEntry_t *World_Find(const Net2Addr *a) {
	for (int i = 0; i < wm.n; i++) {
		if (Net2_AddrEqual(&wm.e[i].addr, a)) {
			return &wm.e[i];
		}
	}
	return NULL;
}

/* a server's answer: a world of LAN discovery (added) or the ping of a world of the list */
static void World_Info(const Net2Event *ev) {
	char text[64];
	if (!(ev->info.flags & COORD_INFO_WORLD)) {
		return;
	}
	worldEntry_t *e = World_Find(&ev->addr);
	if (!e) {
		if (wm.page != WP_LAN || wm.n >= WORLD_MAX) {
			return;
		}
		e = &wm.e[wm.n++];
		memset(e, 0, sizeof(*e));
		e->addr = ev->addr;
	}
	e->ping = (int)ev->ping;
	e->players = ev->info.players;
	e->maxPlayers = ev->info.maxPlayers;
	Str_Copy(e->name, ev->info.name, sizeof(e->name));
	Str_Copy(e->map, ev->info.map, sizeof(e->map));
	Str_Copy(e->mode, ev->info.mode, sizeof(e->mode));
	World_Format(text, sizeof(text), "MP_Found", "", wm.n);
	World_Status(text);
	World_Fill();
}

/* ---- the browser's network ---- */

static byte World_Open(void) {
	Net2Config cfg;
	int err = 0;
	if (wm.net) {
		return 1;
	}
	Net2_DefaultConfig(&cfg, 0);
	cfg.appVersion = LIST_VERSION;
	wm.net = Net2_Create(&cfg, &err);
	if (!wm.net) {
		cfg.ipv6 = 0;
		wm.net = Net2_Create(&cfg, &err);
	}
	if (!wm.net) {
		Con_Errorf("world menu: no Net2 socket (error %d)\n", err);
	}
	return wm.net != NULL;
}

static void World_Close(void) {
	if (wm.net) {
		Net2_Destroy(wm.net);
		wm.net = NULL;
	}
	wm.listPhase = WL_IDLE;
}

/* LAN discovery: the broadcast (IPv4) and the group (IPv6) on the ports from worldport on; lanaddr= instead asks
 * that address on the same ports (tests on loopback) */
static void World_Lan(void) {
	char lan[64];
	uint64_t now = World_Ms();
	uint16_t base = World_Port();
	if (!World_Open()) {
		return;
	}
	if (Game_OptionStr("lanaddr", lan, sizeof(lan)) && lan[0]) {
		for (int i = 0; i < NET2_LAN_PORTS; i++) {
			Net2Addr a;
			if (Net2_ParseAddr(lan, (uint16_t)(base + i), &a)) {
				a.port = (uint16_t)(base + i);
				Net2_Query(wm.net, &a, now);
			}
		}
	} else {
		Net2_LanQuery(wm.net, base, now);
	}
	Net2_Flush(wm.net, now);
	wm.searchEnd = now + WORLD_SEARCH_MS;
}

/* ---- the list server ---- */

/* the worlds of the list server of scrapdecomp.ini: a connection, the query, its pages; then a ping of each world */
static void World_ListQuery(void) {
	char addr[64];
	Net2Addr a;
	uint64_t now = World_Ms();
	if (!Game_OptionStr("listserver", addr, sizeof(addr)) || !addr[0]) {
		World_Status(Script_Lang("MP_NoListServer"));
		return;
	}
	if (!Net2_ParseAddr(addr, (uint16_t)Game_OptionOr("listport", COORD_LIST_PORT), &a) || !World_Open()) {
		World_Status(Script_Lang("MP_ListNotReached"));
		return;
	}
	if (wm.listPhase != WL_IDLE) {
		Net2_Disconnect(wm.net, 0, NET2_DR_LOCAL);
	}
	World_Status(Script_Lang("MP_Searching"));
	wm.listPhase = Net2_Connect(wm.net, &a, now) == NET2_OK ? WL_CONNECTING : WL_IDLE;
	if (wm.listPhase == WL_IDLE) {
		World_Status(Script_Lang("MP_ListNotReached"));
	}
}

static void World_ListPage(const Net2Event *ev) {
	static ListWorld got[LIST_PAGE];
	char text[64];
	int total, first;
	int k = List_ReadPage(ev->data, ev->size, got, LIST_PAGE, &total, &first);
	uint64_t now = World_Ms();
	for (int i = 0; i < k && wm.n < WORLD_MAX; i++) {
		if (World_Find(&got[i].addr)) {
			continue;
		}
		worldEntry_t *e = &wm.e[wm.n++];
		memset(e, 0, sizeof(*e));
		e->addr = got[i].addr;
		e->ping = -1;
		e->players = got[i].players;
		e->maxPlayers = got[i].maxPlayers;
		Str_Copy(e->name, got[i].name, sizeof(e->name));
		Str_Copy(e->map, got[i].map, sizeof(e->map));
		Str_Copy(e->mode, got[i].mode, sizeof(e->mode));
		memcpy(e->key, got[i].key, NET2_KEY_SIZE);
		e->hasKey = 1;
		Net2_Query(wm.net, &e->addr, now);
	}
	if (!k || first + k >= total) {
		Net2_Disconnect(wm.net, 0, NET2_DR_LOCAL);
		wm.listPhase = WL_IDLE;
		World_Format(text, sizeof(text), "MP_Found", "", wm.n);
		World_Status(text);
	}
	World_Fill();
}

static void World_ListEvent(const Net2Event *ev) {
	if (ev->type == NET2_EV_CONNECT && wm.listPhase == WL_CONNECTING) {
		uint8_t q[3] = { LIST_MSG_QUERY, LIST_VERSION & 0xff, LIST_VERSION >> 8 };
		Net2_Send(wm.net, 0, 0, q, sizeof(q));
		wm.listPhase = WL_PAGES;
	} else if (ev->type == NET2_EV_MESSAGE && ev->size > 0 && ev->data[0] == LIST_MSG_PAGE && wm.listPhase == WL_PAGES) {
		World_ListPage(ev);
	} else if (ev->type == NET2_EV_DISCONNECT && wm.listPhase != WL_IDLE) {
		wm.listPhase = WL_IDLE;
		World_Status(Script_Lang("MP_ListNotReached"));
	}
}

static void World_Refresh(void) {
	wm.n = 0;
	wm.index = 0;
	if (wm.page == WP_LAN) {
		World_Status(Script_Lang("MP_Searching"));
		World_Lan();
	} else if (wm.page == WP_LIST) {
		World_ListQuery();
	}
	World_Fill();
}

/* ---- the pages ---- */

/* the levels a host may start in: the city of the coordinator */
static void World_Levels(void) {
	const char *p = COORD_CITY;
	if (world_numLevels) {
		return;
	}
	while (*p && world_numLevels < WORLD_LEVELS) {
		int n = (int)strcspn(p, ",");
		Str_PrintfQuiet(world_levels[world_numLevels++], COORD_LEVEL + 1, "%.*s", n, p);
		p += n + (p[n] == ',');
	}
}

/* the name of a level: its metro station's of the language, else the level's */
static const char *World_LevelText(int i) {
	char key[COORD_LEVEL + 16];
	Str_PrintfQuiet(key, sizeof(key), "Station_%s", world_levels[i]);
	const char *t = Lang_GetStr(key);
	return t && *t ? t : world_levels[i];
}

static void World_Show(int id, int page) {
	char players[16], port[16], key[32], title[0x80];
	cScorer *sc = Scorer_Get(id);
	modeVals_t t = { .n = 0 };
	const char *names[10];
	const nativeVal_t *vals[10];
	int n = 0;

	World_KeepName(id);
	wm.page = page;
	if (page != WP_HOST) {
		wm.joinPage = page;
	}
	Str_Copy(title, sc && sc->current ? sc->current : "WorldJoin", sizeof(title));
	names[n] = "name", vals[n++] = Mode_S(&t, World_Name());
	if (page == WP_HOST) {
		World_Levels();
		Str_PrintfQuiet(players, sizeof(players), "%d", wm.maxPlayers);
		Str_PrintfQuiet(port, sizeof(port), "%d", wm.port ? wm.port : World_Port());
		Str_PrintfQuiet(key, sizeof(key), "MP_Mode_%s", world_modes[wm.mode]);
		names[n] = "level", vals[n++] = Mode_S(&t, World_LevelText(wm.level));
		names[n] = "mode", vals[n++] = Mode_S(&t, Script_Lang(key));
		names[n] = "pvp", vals[n++] = Mode_S(&t, Script_Lang(wm.pvp ? "MP_On" : "MP_Off"));
		names[n] = "public", vals[n++] = Mode_S(&t, Script_Lang(wm.listed ? "MP_On" : "MP_Off"));
		names[n] = "players", vals[n++] = Mode_S(&t, players);
		names[n] = "port", vals[n++] = Mode_S(&t, port);
		Net_RunWorld(id, "WorldHost", "WorldHost", world_pages[page], "", n, names, vals);
	} else {
		names[n] = "worldaddress", vals[n++] = Mode_S(&t, wm.address);
		names[n] = "current", vals[n++] = Mode_S(&t, title);
		Net_RunWorld(id, "World", "WorldJoin", world_pages[page], wm.motd, n, names, vals);
		World_Fill();
	}
	Mode_Done(&t, 1);
}

static void World_Wait(int id, const char *title) {
	modeVals_t t = { .n = 0 };
	const char *names[1] = { "title" };
	const nativeVal_t *vals[1] = { Mode_S(&t, title) };
	Net_RunWorld(id, "worldwait", "WorldJoin", "Wait", "", 1, names, vals);
	Mode_Done(&t, 1);
	wm.waiting = 1;
}

/* a box with the text, its button back to the page of the worlds the player was on */
static void World_Box(const char *text) {
	MODE_PRINTED(Menu, ShowTextBox, "isss", 0L, text, wm.page == WP_HOST ? "!WorldHostMenu" : "!WorldJoinMenu",
		wm.page == WP_HOST ? "!WorldHostMenu" : "!WorldJoinMenu");
}

/* the host part of an address of the address page or of a list entry (no port, no brackets) */
static void World_Host(const Net2Addr *a, char *out, int size) {
	char text[64];
	Net2_AddrString(a, text, sizeof(text));
	char *colon = strrchr(text, ':');
	if (colon) {
		*colon = 0;
	}
	if (text[0] == '[') {
		size_t n = strlen(text);
		if (n >= 2 && text[n - 1] == ']') {
			text[n - 1] = 0;
		}
		Str_Copy(out, text + 1, size);
	} else {
		Str_Copy(out, text, size);
	}
}

static void World_Join(int id, const Net2Addr *a, const uint8_t *key) {
	char host[64], shown[80], title[0x100];
	World_KeepName(id);
	World_Host(a, host, sizeof(host));
	Net2_AddrString(a, shown, sizeof(shown));
	World_Close();
	Join_Start(host, a->port, World_Name(), key);
	World_Format(title, sizeof(title), "MP_Connecting", shown, 0);
	World_Wait(id, title);
}

/* ---- the native functions ---- */

#define WORLD_FN(name) static void World_##name(cAbstractFile *params)
#define WORLD_ARGS \
	char item[0x80]; \
	int id = World_Args(params, item, sizeof(item)); \
	(void)item

WORLD_FN(MenuFn) {
	WORLD_ARGS;
	Net_DoneBrowser();
	World_Show(id, wm.joinPage);
	if (wm.joinPage != WP_ADDRESS) {
		World_Refresh();
	}
}

WORLD_FN(JoinMenu) {
	WORLD_ARGS;
	World_Show(id, wm.joinPage);
	if (wm.joinPage != WP_ADDRESS && !wm.n) {
		World_Refresh();
	}
}

WORLD_FN(HostMenu) {
	WORLD_ARGS;
	World_Show(id, WP_HOST);
}

static void World_Domain(cAbstractFile *params, int page) {
	WORLD_ARGS;
	wm.motd[0] = 0;
	World_Show(id, page);
	if (page != WP_ADDRESS) {
		World_Refresh();
	}
}

WORLD_FN(LanMenu) {
	World_Domain(params, WP_LAN);
}

WORLD_FN(ListMenu) {
	World_Domain(params, WP_LIST);
}

WORLD_FN(AddressMenu) {
	World_Domain(params, WP_ADDRESS);
}

WORLD_FN(RefreshFn) {
	WORLD_ARGS;
	(void)id;
	World_Refresh();
}

/* BrowserColumnBack_<column>: sorted by it, again the other way round */
WORLD_FN(Sort) {
	WORLD_ARGS;
	(void)id;
	const char *column = strlen(item) > 18 ? item + 18 : "";
	wm.sortOrder = !strcmp(wm.sort, column) ? !wm.sortOrder : 0;
	Str_Copy(wm.sort, column, sizeof(wm.sort));
	World_Fill();
}

static void World_Scroll(int how) {
	int i = Net_ScrollIndex(wm.n, wm.index, how);
	if (i >= 0) {
		wm.index = i;
		World_Fill();
	}
}

WORLD_FN(Next) {
	(void)params;
	World_Scroll(0);
}

WORLD_FN(Prev) {
	(void)params;
	World_Scroll(1);
}

/* BrowserSlotN */
WORLD_FN(JoinSlot) {
	WORLD_ARGS;
	int i = strlen(item) > 11 ? atoi(item + 11) - 1 + wm.index : -1;
	if (i < 0 || i >= wm.n) {
		return;
	}
	worldEntry_t e = wm.e[i];
	World_Join(id, &e.addr, e.hasKey ? e.key : NULL);
}

WORLD_FN(JoinAddress) {
	char text[0x100];
	Net2Addr a;
	WORLD_ARGS;
	Str_Copy(wm.address, Script_GetStr(id, "Item1", "Text"), sizeof(wm.address));
	if (!Net2_ParseAddr(wm.address, World_Port(), &a)) {
		World_Format(text, sizeof(text), "MP_BadAddress", wm.address, 0);
		World_Status(text);
		return;
	}
	World_Join(id, &a, NULL);
}

static void World_SetChoice(int id, const char *item, const char *text) {
	Script_SetStr(id, item, "Text", text);
}

static void World_StepLevel(cAbstractFile *params, int dir) {
	WORLD_ARGS;
	World_Levels();
	wm.level = (wm.level + dir + world_numLevels) % world_numLevels;
	World_SetChoice(id, "HostLevel", World_LevelText(wm.level));
}

WORLD_FN(HostLevelPrev) {
	World_StepLevel(params, -1);
}

WORLD_FN(HostLevelNext) {
	World_StepLevel(params, 1);
}

WORLD_FN(HostMode) {
	char key[32];
	WORLD_ARGS;
	wm.mode = !wm.mode;
	Str_PrintfQuiet(key, sizeof(key), "MP_Mode_%s", world_modes[wm.mode]);
	World_SetChoice(id, "HostMode", Script_Lang(key));
}

WORLD_FN(HostPvP) {
	WORLD_ARGS;
	wm.pvp = !wm.pvp;
	World_SetChoice(id, "HostPvP", Script_Lang(wm.pvp ? "MP_On" : "MP_Off"));
}

WORLD_FN(HostPublic) {
	WORLD_ARGS;
	wm.listed = !wm.listed;
	World_SetChoice(id, "HostPublic", Script_Lang(wm.listed ? "MP_On" : "MP_Off"));
}

/* the number of a field kept in min..max (and written back) */
static int World_Clamp(int id, const char *item, int min, int max) {
	char text[16];
	int v = atoi(Script_GetStr(id, item, "Text"));
	v = v < min ? min : v > max ? max : v;
	Str_PrintfQuiet(text, sizeof(text), "%d", v);
	Script_SetStr(id, item, "Text", text);
	return v;
}

WORLD_FN(HostFn) {
	JoinHostOpts o;
	WORLD_ARGS;
	World_KeepName(id);
	World_Levels();
	wm.maxPlayers = World_Clamp(id, "HostPlayers", 1, WORLD_MAX_PLAYERS);
	wm.port = World_Clamp(id, "HostPort", 1, 65535 - NET2_LAN_PORTS - COORD_MAX_LEVELS);
	memset(&o, 0, sizeof(o));
	Str_Copy(o.level, world_levels[wm.level], sizeof(o.level));
	Str_Copy(o.mode, world_modes[wm.mode], sizeof(o.mode));
	o.pvp = wm.pvp;
	o.maxPlayers = wm.maxPlayers;
	o.port = (uint16_t)wm.port;
	o.listed = wm.listed;
	World_Close();
	Join_StartHost(&o, World_Name());
	World_Wait(id, Script_Lang("MP_Starting"));
}

WORLD_FN(Abort) {
	WORLD_ARGS;
	wm.waiting = 0;
	Join_Stop(NULL);
	Join_UiClear();
	World_Show(id, wm.page);
}

static cPy_NativeFunc world_natives[22];

void WorldMenu_Register(void) {
	static const struct {
		const char *name;
		nativefunc_t fn;
	} fns[] = {
		{ "WorldMenu", World_MenuFn },
		{ "WorldJoinMenu", World_JoinMenu },
		{ "WorldHostMenu", World_HostMenu },
		{ "WorldLanMenu", World_LanMenu },
		{ "WorldListMenu", World_ListMenu },
		{ "WorldAddressMenu", World_AddressMenu },
		{ "WorldRefresh", World_RefreshFn },
		{ "WorldSort", World_Sort },
		{ "WorldNext", World_Next },
		{ "WorldPrev", World_Prev },
		{ "WorldJoinSlot", World_JoinSlot },
		{ "WorldJoinAddress", World_JoinAddress },
		{ "WorldHostLevelPrev", World_HostLevelPrev },
		{ "WorldHostLevelNext", World_HostLevelNext },
		{ "WorldHostMode", World_HostMode },
		{ "WorldHostPvP", World_HostPvP },
		{ "WorldHostPublic", World_HostPublic },
		{ "WorldHost", World_HostFn },
		{ "WorldAbort", World_Abort },
	};
	_Static_assert(sizeof(fns) / sizeof(fns[0]) <= sizeof(world_natives) / sizeof(world_natives[0]), "natives");
	if (wm.registered) {
		return;
	}
	wm.registered = 1;
	for (size_t i = 0; i < sizeof(fns) / sizeof(fns[0]); i++) {
		cPy_NativeFunc_ctor(&world_natives[i], fns[i].name, fns[i].fn);
	}
}

/* ---- each frame ---- */

static void World_Net(void) {
	Net2Event ev;
	uint64_t now = World_Ms();
	if (!wm.net) {
		return;
	}
	if (!Script_HasItem(0, "WorldButton_Lan")) {
		if (!wm.gone) {
			wm.gone = now;
		} else if (now - wm.gone >= WORLD_CLOSE_MS) {
			World_Close();
			return;
		}
	} else {
		wm.gone = 0;
	}
	if (wm.searchEnd && now >= wm.searchEnd) {
		char text[64];
		wm.searchEnd = 0;
		World_Format(text, sizeof(text), "MP_Found", "", wm.n);
		World_Status(text);
	}
	Net2_Poll(wm.net, now);
	while (Net2_Receive(wm.net, &ev)) {
		if (ev.type == NET2_EV_SERVERINFO) {
			World_Info(&ev);
		} else {
			World_ListEvent(&ev);
		}
	}
	if (wm.net) {
		Net2_Flush(wm.net, now);
	}
}

void WorldMenu_Frame(void) {
	const char *why;
	if (!wm.registered) {
		return;
	}
	World_Net();
	int ui = Join_UiState(&why);
	if (wm.waiting) {
		if (ui == JOIN_UI_FAILED) {
			wm.waiting = 0;
			World_Box(why);
			Join_UiClear();
		} else if (ui != JOIN_UI_WAIT) {
			wm.waiting = 0;
		}
		return;
	}
	/* a world that ended while its level ran: the text once the main menu runs again */
	if (ui != JOIN_UI_FAILED || g_pApp->loadPending || !Mode_ModGetI("Init", "inMainMenu", 0)) {
		wm.menuUp = 0;
		return;
	}
	uint64_t now = World_Ms();
	if (!wm.menuUp) {
		wm.menuUp = now;
	} else if (now - wm.menuUp >= WORLD_MESSAGE_MS) {
		wm.menuUp = 0;
		World_Box(why);
		Join_UiClear();
	}
}
