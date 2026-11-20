/*
 * The debug menu of debug builds (not in the original): a page of the console view to check the missions, the
 * cutscenes, the videos and the levels without playing up to them. F11 opens it (or the console command debugmenu).
 * Up and Down (PgUp, PgDn, Home, End, the wheel) choose, Enter opens a page or starts the entry, Backspace goes back
 * (or takes a letter off the filter), typed letters filter the page, Escape closes it.
 *
 * Videos play at once (Video_Play, as Scrap.VideoPlayer). Missions start through the mission maker (MissionMaker_Load: the save variables of a new game). A cutscene
 * (include/scrap/scenelist.h) starts the mission it belongs to; when that mission does not play it at its start, the
 * scene is loaded by its own load function and played once the new level has run for a second (DebugMenu_Frame, a
 * check each frame).
 */
#ifdef SCRAP_DEBUG_MENU

#include <string.h>

#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/consoleview.h"
#include "scrap/crt.h"
#include "scrap/debugmenu.h"
#include "scrap/missionmaker.h"
#include "scrap/scenelist.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptports.h"

static const char *const dm_videos[][2] = {
	{ "Video/Intro.avi", "the intro of a new game" },
	{ "Video/MSE.avi", "MercurySteam logo" },
	{ "Video/MSE-Short.avi", "MercurySteam logo, short" },
	{ "Video/Bishop.avi", "the bishop" },
	{ "Video/Mayor.avi", "the mayor" },
	{ "Video/Police.avi", "the police" },
	{ "Video/Viscous.avi", "Viscous" },
};

enum { DM_ROOT, DM_LEVELS, DM_MISSIONS, DM_SCENES, DM_VIDEOS, DM_ARENAS, DM_MAPS, DM_SPLIT };

static const char *const dm_rootItems[] = { "Missions by level", "Cutscenes", "Videos", "Arenas and super deals",
	"Levels (no mission)", "Network levels in split screen" };
static const int dm_rootPages[] = { DM_LEVELS, DM_SCENES, DM_VIDEOS, DM_ARENAS, DM_MAPS, DM_SPLIT };

#define DM_ITEMS 160
#define DM_DEPTH 4
#define DM_FILTER 32

typedef struct dmItem {
	char label[0x80];
	int index;			/* into the table of the page */
} dmItem;

static struct {
	int page[DM_DEPTH];
	int sel[DM_DEPTH];
	const char *level;		/* of DM_MISSIONS */
	int depth;
	char filter[DM_FILTER];
	dmItem items[DM_ITEMS];
	int n;
	int top;			/* the first row shown */
	/* the cutscene to play after the load */
	const sceneInfo_t *pending;
	double lastTime;
	byte loaded;
} dm;

/* the levels of the missions, each once, in the order of the list (not the arenas) */
static int DM_Levels(const char **out, int max) {
	int n = 0;
	for (int i = 0; i < g_MissionMakerCount && n < max; i++) {
		const mmEntry *m = &g_MissionMakerList[i];
		if (!*m->mission || *m->dealType) {
			continue;
		}
		int k = 0;
		while (k < n && strcmp(out[k], m->level)) {
			k++;
		}
		if (k == n) {
			out[n++] = m->level;
		}
	}
	return n;
}

static byte DM_Filtered(const char *label) {
	size_t f = strlen(dm.filter);
	if (!f) {
		return 1;
	}
	for (const char *s = label; *s; s++) {
		if (!crt_strnicmp(s, dm.filter, f)) {
			return 1;
		}
	}
	return 0;
}

static void DM_Add(int index, const char *fmt, const char *a, const char *b) {
	if (dm.n >= DM_ITEMS) {
		return;
	}
	dmItem *it = &dm.items[dm.n];
	Str_PrintfQuiet(it->label, sizeof(it->label), fmt, a, b);
	if (!DM_Filtered(it->label)) {
		return;
	}
	it->index = index;
	dm.n++;
}

/* the items of the page on top, through the filter */
static void DM_Build(void) {
	const char *levels[64];
	int page = dm.page[dm.depth];

	dm.n = 0;
	switch (page) {
	case DM_ROOT:
		for (int i = 0; i < (int)(sizeof(dm_rootItems) / sizeof(dm_rootItems[0])); i++) {
			DM_Add(i, "%s%s", dm_rootItems[i], "");
		}
		break;
	case DM_LEVELS: {
		int nl = DM_Levels(levels, 64);
		for (int i = 0; i < nl; i++) {
			DM_Add(i, "%s%s", levels[i], "");
		}
		break;
	}
	case DM_MISSIONS:
		for (int i = 0; i < g_MissionMakerCount; i++) {
			const mmEntry *m = &g_MissionMakerList[i];
			if (*m->mission && !*m->dealType && !strcmp(m->level, dm.level)) {
				DM_Add(i, "%s%s", m->mission, "");
			}
		}
		break;
	case DM_SCENES:
		for (int i = 0; i < SceneList_Count(); i++) {
			const sceneInfo_t *sc = SceneList_At(i);
			char what[0x80];
			Str_PrintfQuiet(what, sizeof(what), "%-18s %s", sc->scene, sc->actors);
			DM_Add(i, "%s  (%s)", what, sc->level);
		}
		break;
	case DM_VIDEOS:
		for (int i = 0; i < (int)(sizeof(dm_videos) / sizeof(dm_videos[0])); i++) {
			DM_Add(i, "%-22s %s", dm_videos[i][0] + 6, dm_videos[i][1]);
		}
		break;
	case DM_ARENAS:
		for (int i = 0; i < g_MissionMakerCount; i++) {
			const mmEntry *m = &g_MissionMakerList[i];
			if (*m->dealType) {
				char what[0x80];
				Str_PrintfQuiet(what, sizeof(what), "%-4s %s", m->level, m->mission);
				DM_Add(i, "%-30s %s", what, m->dealType);
			}
		}
		break;
	case DM_MAPS: {
		/* every level of the list once, and the menu */
		int n = 0;
		for (int i = 0; i < g_MissionMakerCount && n < 64; i++) {
			int k = 0;
			while (k < n && strcmp(levels[k], g_MissionMakerList[i].level)) {
				k++;
			}
			if (k == n) {
				levels[n++] = g_MissionMakerList[i].level;
				DM_Add(i, "%s%s", g_MissionMakerList[i].level, "");
			}
		}
		DM_Add(-1, "%s%s", "Menu", "");
		break;
	}
	case DM_SPLIT:
		for (int i = 0; i < g_MissionMakerSplitCount; i++) {
			DM_Add(i, "%s%s", g_MissionMakerSplitMaps[i], "");
		}
		break;
	}
	if (dm.sel[dm.depth] >= dm.n) {
		dm.sel[dm.depth] = dm.n ? dm.n - 1 : 0;
	}
}

void DebugMenu_Open(void) {
	memset(dm.page, 0, sizeof(dm.page));
	memset(dm.sel, 0, sizeof(dm.sel));
	dm.depth = 0;
	dm.filter[0] = 0;
	dm.top = 0;
	DM_Build();
}

static void DM_Push(int page) {
	if (dm.depth + 1 >= DM_DEPTH) {
		return;
	}
	dm.depth++;
	dm.page[dm.depth] = page;
	dm.sel[dm.depth] = 0;
	dm.filter[0] = 0;
	dm.top = 0;
	DM_Build();
}

static byte DM_Back(void) {
	if (!dm.depth) {
		return 0;
	}
	dm.depth--;
	dm.filter[0] = 0;
	dm.top = 0;
	DM_Build();
	return 1;
}

static void DM_Print(const char *fmt, const char *a, const char *b) {
	char line[0x100];
	Str_PrintfQuiet(line, sizeof(line), fmt, a, b);
	Con_Echo(0x80e0ff, line);
}

/* the mission of a cutscene, and the scene after the load when the mission does not play it at its start */
static void DM_PlayScene(const sceneInfo_t *s) {
	DM_Print("debug menu: cutscene %s (%s)\n", s->scene, s->level);
	dm.pending = s->atStart ? NULL : s;
	dm.loaded = 0;
	dm.lastTime = Mode_Time();
	MissionMaker_Load(s->level, s->mission, "", NULL);
}

byte DebugMenu_Scene(const char *name) {
	const sceneInfo_t *s = SceneList_Find(name);
	if (s) {
		DM_PlayScene(s);
	}
	return s != NULL;
}

const char *DebugMenu_SceneName(int i) {
	const sceneInfo_t *s = SceneList_At(i);
	return s ? s->scene : NULL;
}

/* Enter on the chosen item; 0 when the menu should close (something was started) */
static byte DM_Activate(void) {
	const char *levels[64];

	if (!dm.n) {
		return 1;
	}
	int i = dm.items[dm.sel[dm.depth]].index;
	switch (dm.page[dm.depth]) {
	case DM_ROOT:
		DM_Push(dm_rootPages[i]);
		return 1;
	case DM_LEVELS:
		DM_Levels(levels, 64);
		dm.level = levels[i];
		DM_Push(DM_MISSIONS);
		return 1;
	case DM_MISSIONS:
	case DM_ARENAS: {
		const mmEntry *m = &g_MissionMakerList[i];
		DM_Print("debug menu: mission %s in %s\n", m->mission, m->level);
		MissionMaker_Load(m->level, m->mission, m->dealType, NULL);
		return 0;
	}
	case DM_SCENES:
		DM_PlayScene(SceneList_At(i));
		return 0;
	case DM_VIDEOS:
		DM_Print("debug menu: video %s%s\n", dm_videos[i][0], "");
		Video_Play(dm_videos[i][0], "play", 0.0f);
		return 0;
	case DM_MAPS: {
		const char *level = i < 0 ? "Menu" : g_MissionMakerList[i].level;
		DM_Print("debug menu: level %s%s\n", level, "");
		MissionMaker_Load(level, "", "", NULL);
		return 0;
	}
	case DM_SPLIT:
		DM_Print("debug menu: split screen %s%s\n", g_MissionMakerSplitMaps[i], "");
		MissionMaker_Split(g_MissionMakerSplitMaps[i]);
		return 0;
	}
	return 1;
}

byte DebugMenu_Key(int key, int mods) {
	int *sel = &dm.sel[dm.depth];
	(void)mods;
	switch (key) {
	case CVK_UP:
		*sel = *sel > 0 ? *sel - 1 : dm.n ? dm.n - 1 : 0;
		return 1;
	case CVK_DOWN:
		*sel = *sel + 1 < dm.n ? *sel + 1 : 0;
		return 1;
	case CVK_PGUP:
		*sel = *sel > 10 ? *sel - 10 : 0;
		return 1;
	case CVK_PGDN:
		*sel = *sel + 10 < dm.n ? *sel + 10 : dm.n ? dm.n - 1 : 0;
		return 1;
	case CVK_HOME:
		*sel = 0;
		return 1;
	case CVK_END:
		*sel = dm.n ? dm.n - 1 : 0;
		return 1;
	case CVK_ENTER:
	case CVK_RIGHT:
		return DM_Activate();
	case CVK_LEFT:
		DM_Back();
		return 1;
	case CVK_BACKSPACE: {
		size_t f = strlen(dm.filter);
		if (f) {
			dm.filter[f - 1] = 0;
			DM_Build();
			return 1;
		}
		return DM_Back();
	}
	}
	return 1;
}

void DebugMenu_Char(int c) {
	size_t f = strlen(dm.filter);
	if ((byte)c <= ' ' || f + 1 >= sizeof(dm.filter)) {
		return;
	}
	dm.filter[f] = (char)c;
	dm.filter[f + 1] = 0;
	dm.sel[dm.depth] = 0;
	dm.top = 0;
	DM_Build();
}

void DebugMenu_Wheel(int notches) {
	int *sel = &dm.sel[dm.depth];
	*sel -= notches;
	if (*sel < 0) {
		*sel = 0;
	}
	if (*sel >= dm.n) {
		*sel = dm.n ? dm.n - 1 : 0;
	}
}

void DebugMenu_Draw(float top, float bottom) {
	static const char *const titles[] = { "Debug menu", "Missions by level", "", "Cutscenes", "Videos",
		"Arenas and super deals", "Levels", "Network levels in split screen" };
	char title[0x80];
	const float row = CONSOLEVIEW_ROW, x = 6.0f;
	int page = dm.page[dm.depth];

	Str_PrintfQuiet(title, sizeof(title), "%s%s%s", page == DM_MISSIONS ? dm.level : titles[page],
		dm.filter[0] ? "   filter: " : "", dm.filter);
	ConsoleView_Text(x, top + 4.0f, 0xffd040, title);
	float y0 = top + 4.0f + row * 1.5f;
	int rows = (int)((bottom - y0 - row * 2.0f) / row);
	int sel = dm.sel[dm.depth];
	if (sel < dm.top) {
		dm.top = sel;
	}
	if (rows > 0 && sel >= dm.top + rows) {
		dm.top = sel - rows + 1;
	}
	for (int r = 0; r < rows && dm.top + r < dm.n; r++) {
		int i = dm.top + r;
		float y = y0 + (float)r * row;
		if (i == sel) {
			ConsoleView_Rect(0.0f, y - 1.0f, 640.0f, row, 0xa0d07818);
		}
		ConsoleView_Text(x + 6.0f, y, i == sel ? 0xffffff : 0xc8c8c8, dm.items[i].label);
	}
	if (!dm.n) {
		ConsoleView_Text(x + 6.0f, y0, 0x909090, "nothing (Backspace takes a letter off the filter)");
	}
	ConsoleView_Text(x, bottom - row - 2.0f, 0x909090,
		"Up/Down choose  Enter start  Backspace back  type to filter  Esc close  F11 console");
}

void DebugMenu_Frame(void) {
	nativeVal_t path = { 0 };
	char want[0x60];

	if (!dm.pending || !g_pWorld) {
		return;
	}
	/* the level started when its clock went back; then a second of it */
	double t = Mode_Time();
	if (t < dm.lastTime) {
		dm.loaded = 0;
		Native_SetNone(&path);
		Str_PrintfQuiet(want, sizeof(want), "Levels/%s", dm.pending->level);
		if (Mode_Import("Init") && Mode_ModGet("Init", "Path", &path) && path.type == NV_STR &&
			!crt_stricmp(Native_Text(&path), want)) {
			dm.loaded = 1;
		}
		Native_Free(&path);
	}
	dm.lastTime = t;
	if (!dm.loaded || t < 1.0) {
		return;
	}
	/* a scene the mission played at its start goes first (SceneName is the name of the scene entity, DM_Scene_...,
	 * while one plays; the class name after a Load alone) */
	nativeVal_t cur = { 0 };
	Native_SetNone(&cur);
	byte busy = Mode_Import("Scene") && Mode_ModGet("Scene", "SceneName", &cur) && cur.type == NV_STR &&
		!crt_strnicmp(Native_Text(&cur), "DM_Scene_", 9);
	Native_Free(&cur);
	Native_ErrClear();
	if (busy) {
		return;
	}
	const sceneInfo_t *s = dm.pending;
	dm.pending = NULL;
	if (!SceneList_Load(s)) {
		DM_Print("debug menu: %s.%s did not load the scene\n", s->module, s->load);
		return;
	}
	if (!Mode_Import("Scene") || !PORT_CALL(Scene, Play, NULL, "s", s->scene)) {
		Native_ErrClear();
		DM_Print("debug menu: Scene.Play(\"%s\") failed%s\n", s->scene, "");
	}
}

#endif
