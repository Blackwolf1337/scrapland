/*
 * The places of the sandbox and freeroam modes that single player keeps closed, see include/scrap/places.h and
 * include/scrap/placedef.h. Not in the original.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/arena.h"
#include "scrap/chartype.h"
#include "scrap/collisiongrid.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/entity.h"
#include "scrap/filepak.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/netrep.h"
#include "scrap/objentity.h"
#include "scrap/placedef.h"
#include "scrap/places.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapworld.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/texts.h"
#include "scrap/travel.h"
#include "scrap/walkcharentity.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"
#include "game/Script/levels/Levels.h"

#define PL_TRACK 1024				/* entities the census follows between two counts */
#define PL_MOVED 50.0f				/* units an entity went between two counts to count as moving */
#define PL_TEXT 96

static struct {
	char mode[16];					/* -mode of an instance, the world's mode of a client ("" sandbox) */
	byte worldMode;					/* a client knows its world's mode */
	byte useHeld[PLAYERS_MAX];
	struct {
		const cEntity *e;
		vec3_t pos;
	} track[PL_TRACK];
	int ntrack;
} pl;

static byte Pl_ModeOk(const char *mode) {
	return !mode[0] || !crt_stricmp(mode, "sandbox") || !crt_stricmp(mode, "freeroam");
}

byte Places_ParseParam(const char *param, const char *ext) {
	if (!crt_stricmp("mode", param)) {
		Str_Copy(pl.mode, ext, sizeof(pl.mode));
	}
	return 0;
}

void Places_WorldMode(const char *mode) {
	Str_Copy(pl.mode, mode ? mode : "", sizeof(pl.mode));
	pl.worldMode = 1;
}

byte Places_On(void) {
	if (g_Instance) {
		return !Arena_On() && Pl_ModeOk(pl.mode);
	}
	return g_Watch && pl.worldMode && Pl_ModeOk(pl.mode);
}

/* the level being played ("Levels/Orbit" -> "Orbit") */
static const char *Pl_Level(void) {
	const char *p = g_pApp && g_pApp->levelPath ? g_pApp->levelPath : "";
	const char *s = strrchr(p, '/');
	return s ? s + 1 : p;
}

static const PlaceLevel *Pl_Here(void) {
	return Places_On() ? PlaceDef_Level(PlaceDef_Table(), Pl_Level()) : NULL;
}

/* a position of the table: 1 with pos and yaw (radians) */
static byte Pl_Pos(const PlacePos *p, vec3_t *pos, float *yaw) {
	vec3_t at = { 0.0f, 0.0f, 0.0f }, ang = { 0.0f, 0.0f, 0.0f };
	char found[64];
	if (p->dummy[0]) {
		DummySearch_Start(p->dummy, 1);
		if (!DummySearch_NextEuler(found, sizeof(found), &at, &ang)) {
			Con_Printf("places: no dummy %s in %s\n", p->dummy, Pl_Level());
			return 0;
		}
	}
	pos->x = at.x + p->x;
	pos->y = at.y + p->y;
	pos->z = at.z + p->z;
	*yaw = p->dummyYaw ? ang.y : p->yaw * (M_PI_F / 180.0f);
	return 1;
}

/* a comma list as a list value of texts */
static void Pl_List(nativeVal_t *out, const char *list) {
	char name[PLACE_NAME];
	Native_SetEmpty(out, NV_LIST);
	while (*list) {
		int n = (int)strcspn(list, ",");
		Str_PrintfQuiet(name, sizeof(name), "%.*s", n, list);
		if (name[0]) {
			Native_SetStr(Native_Append(out), name);
		}
		list += n + (list[n] == ',');
	}
}

/* ---- the AI graph of a level without one (the instance) ---- */

#define PL_AI_NODES 4096			/* points of a graph at most */
#define PL_AI_CLEAR 0.3f			/* the sphere around a point that must be free, of the step */
#define PL_AI_DROP 60000.0f			/* a point must have the ground at most this far below it */

static uint8_t *pl_graph;			/* the graph in the format of PathAI.pth (src/game/AI/NodesWorldSup.c) */

/* the point of a grid cell is in the free space of the level: no collision around it, the ground below it, under
 * the top of the map */
static byte Pl_AiFree(const vec3_t *c, float step, float top) {
	vec3_t down = { c->x, c->y - PL_AI_DROP, c->z };
	return c->y < top && !Engine_TestSphere(c, c, step * PL_AI_CLEAR, NULL, NULL) &&
		Engine_TestLine(c, &down, NULL, NULL);
}

/* the middle of grid cell x y z */
static void Pl_AiCentre(const box_t *box, float step, vec3_t *c, int x, int y, int z) {
	c->x = box->min.x + ((float)x + 0.5f) * step;
	c->y = box->min.y + ((float)y + 0.5f) * step;
	c->z = box->min.z + ((float)z + 0.5f) * step;
}

/* the graph of the free space reached from the DM_NodeIniAI dummies over a grid of step units: the points in the
 * order the search found them, each linked to its neighbours (the 26 around it) it sees; into pl_graph as the
 * graph file of the AI of the level (path/AI/PathAI.pth, served from memory); the number of points */
static int Pl_AiGraph(const char *path, float step) {
	char name[0x104], found[64];
	vec3_t seed, ang, top3;
	box_t box;
	World_GetCollideBox(&box);
	float top = box.max.y;
	DummySearch_Start("DM_MapTop", 1);
	if (DummySearch_NextEuler(found, sizeof(found), &top3, &ang)) {
		top = top3.y;
	}
	int nx = (int)((box.max.x - box.min.x) / step) + 1, ny = (int)((box.max.y - box.min.y) / step) + 1;
	int nz = (int)((box.max.z - box.min.z) / step) + 1;
	if (nx <= 0 || ny <= 0 || nz <= 0 || (int64_t)nx * ny * nz > 4000000) {
		Con_Printf("places: no AI graph for %s (a grid of %d x %d x %d)\n", path, nx, ny, nz);
		return 0;
	}
	/* cell -> point index + 1 (0 not reached, -1 not free); the search queue of cells */
	int cells = nx * ny * nz;
	int *mark = calloc((size_t)cells, sizeof(int));
	int *queue = malloc(PL_AI_NODES * sizeof(int));
	if (!mark || !queue) {
		free(mark);
		free(queue);
		return 0;
	}
#define PL_CELL(x, y, z) (((z) * ny + (y)) * nx + (x))
	int n = 0;
	DummySearch_Start("DM_NodeIniAI*", 1);
	while (n < PL_AI_NODES && DummySearch_NextEuler(found, sizeof(found), &seed, &ang)) {
		/* the free cell nearest to the dummy within two cells */
		int sx = (int)((seed.x - box.min.x) / step), sy = (int)((seed.y - box.min.y) / step);
		int sz = (int)((seed.z - box.min.z) / step);
		for (int k = 0; k < 125; k++) {
			int r = k == 0 ? 0 : k < 27 ? 1 : 2, x = sx + k % 5 - 2, y = sy + k / 5 % 5 - 2, z = sz + k / 25 - 2;
			vec3_t c;
			if (k == 0) {
				x = sx, y = sy, z = sz;
			} else if ((abs(x - sx) > 1 || abs(y - sy) > 1 || abs(z - sz) > 1) != (r == 2)) {
				continue;
			}
			if (x < 0 || y < 0 || z < 0 || x >= nx || y >= ny || z >= nz || mark[PL_CELL(x, y, z)]) {
				continue;
			}
			Pl_AiCentre(&box, step, &c, x, y, z);
			mark[PL_CELL(x, y, z)] = Pl_AiFree(&c, step, top) ? (queue[n] = PL_CELL(x, y, z), ++n) : -1;
			if (mark[PL_CELL(x, y, z)] > 0) {
				break;
			}
		}
	}
	for (int head = 0; head < n && n < PL_AI_NODES; head++) {
		int cell = queue[head], x = cell % nx, y = cell / nx % ny, z = cell / (nx * ny);
		static const int d[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		for (int k = 0; k < 6 && n < PL_AI_NODES; k++) {
			int a = x + d[k][0], b = y + d[k][1], e = z + d[k][2];
			vec3_t c, from;
			if (a < 0 || b < 0 || e < 0 || a >= nx || b >= ny || e >= nz || mark[PL_CELL(a, b, e)]) {
				continue;
			}
			Pl_AiCentre(&box, step, &c, a, b, e);
			Pl_AiCentre(&box, step, &from, x, y, z);
			byte ok = Pl_AiFree(&c, step, top) && !Engine_TestLine(&from, &c, NULL, NULL);
			mark[PL_CELL(a, b, e)] = ok ? (queue[n] = PL_CELL(a, b, e), ++n) : -1;
		}
	}
	/* the file: the count, the points, then per point its links as the positions of the points it links to */
	size_t size = 4 + (size_t)n * (sizeof(vec3_t) + 4 + 26 * sizeof(vec3_t));
	free(pl_graph);
	pl_graph = malloc(size);
	if (!pl_graph || !n) {
		Con_Printf("places: no AI graph for %s (no free space at its DM_NodeIniAI dummies)\n", path);
		free(mark);
		free(queue);
		return 0;
	}
	uint8_t *w = pl_graph;
	int links = 0;
	memcpy(w, &n, 4);
	w += 4;
	for (int i = 0; i < n; i++) {
		int cell = queue[i];
		vec3_t c;
		Pl_AiCentre(&box, step, &c, cell % nx, cell / nx % ny, cell / (nx * ny));
		memcpy(w, &c, sizeof(c));
		w += sizeof(c);
	}
	for (int i = 0; i < n; i++) {
		int cell = queue[i], x = cell % nx, y = cell / nx % ny, z = cell / (nx * ny), count = 0;
		uint8_t *cw = w;
		vec3_t c;
		Pl_AiCentre(&box, step, &c, x, y, z);
		w += 4;
		for (int k = 0; k < 27; k++) {
			int a = x + k % 3 - 1, b = y + k / 3 % 3 - 1, e = z + k / 9 - 1;
			vec3_t o;
			if (k == 13 || a < 0 || b < 0 || e < 0 || a >= nx || b >= ny || e >= nz || mark[PL_CELL(a, b, e)] <= 0) {
				continue;
			}
			Pl_AiCentre(&box, step, &o, a, b, e);
			if (!Engine_TestLine(&c, &o, NULL, NULL)) {
				memcpy(w, &o, sizeof(o));
				w += sizeof(o);
				count++;
			}
		}
		memcpy(cw, &count, 4);
		links += count;
	}
#undef PL_CELL
	free(mark);
	free(queue);
	Str_PrintfQuiet(name, sizeof(name), "%s/AI/PathAI.pth", path);
	FilePack_SetMemory(name, pl_graph, (int)(w - pl_graph));
	Con_Printf("places: AI graph of %s from its geometry: %d points, %d links (step %.0f)\n", path, n, links / 2,
		(double)step);
	return n;
}

/* ---- the level start (the instance): before the characters and the traffic are made ---- */

/* a character at a position: the spawn point CharsNPC makes of a DM_NPC_Spawn_<Class>_<id> dummy there */
static byte Pl_Npc(const PlaceNpc *c) {
	char name[PLACE_NAME + 16];
	vec3_t p;
	float yaw;
	nativeVal_t d, pos, ang, x[3], a[2];
	if (c->mission) {
		return PORT_CALL(CharsNPC, CreateMissionChar, NULL, "s", c->name);
	}
	if (!Pl_Pos(&c->pos, &p, &yaw)) {
		return 1;
	}
	Str_PrintfQuiet(name, sizeof(name), "DM_NPC_Spawn_%s", c->name);
	Native_SetFloat(&x[0], p.x);
	Native_SetFloat(&x[1], p.y);
	Native_SetFloat(&x[2], p.z);
	Mode_Tuple3(&pos, &x[0], &x[1], &x[2]);
	/* the angles of a dummy as the dummy search gives them: degrees, the yaw second */
	Native_SetFloat(&a[0], 0.0);
	Native_SetFloat(&a[1], (double)(yaw * (180.0f / M_PI_F)));
	Native_SetEmpty(&ang, NV_TUPLE);
	*Native_Append(&ang) = a[0];
	*Native_Append(&ang) = a[1];
	Native_SetEmpty(&d, NV_TUPLE);
	Native_SetStr(Native_Append(&d), name);
	*Native_Append(&d) = pos;
	*Native_Append(&d) = ang;
	Native_SetInt(Native_Append(&d), 1);
	byte ok = PORT_CALL(CharsNPC, AddNPCPos, NULL, "v", &d);
	Native_Free(&d);
	return ok;
}

byte Places_LevelStatus(void) {
	static const char *const rails[3] = { "NumNormal", "NumMedium", "NumHeavy" };
	const PlaceLevel *l = g_Instance ? Pl_Here() : NULL;
	byte ok = 1;
	if (!l) {
		return 1;
	}
	Con_Printf("places: %s, %d characters, traffic %d, police %d\n", l->level, l->nnpcs, l->traffic, l->police);
	if (l->aiStep > 0.0f) {
		/* before SAI.IniAI, which reads the graph */
		char file[0x104];
		cFilePack f;
		Str_PrintfQuiet(file, sizeof(file), "%s/AI/PathAI.pth", g_pApp->levelPath);
		cFilePack_ctor(&f);
		if (!cFilePack_Exists(&f.file.base, file)) {
			Pl_AiGraph(g_pApp->levelPath, l->aiStep);
		}
		cFilePack_dtor(&f);
	}
	if ((l->nnpcs || l->nrepos) && !Mode_Import("CharsNPC")) {
		return 0;
	}
	for (int i = 0; ok && i < l->nnpcs; i++) {
		ok = l->npc[i].fixed || Pl_Npc(&l->npc[i]);
	}
	for (int i = 0; ok && i < l->nrepos; i++) {
		ok = PORT_CALL(CharsNPC, SetReposChars, NULL, "si", l->repos[i].cls, (long)l->repos[i].n);
	}
	if (ok && l->traffic >= 0) {
		ok = Mode_GVarSetI("iTrafficNumber", l->traffic);
	}
	if (ok && l->ships[0]) {
		nativeVal_t ships;
		Pl_List(&ships, l->ships);
		ok = Mode_Import("Traffic") && Mode_SetModVar("Traffic", "AcShips", &ships);
		Native_Free(&ships);
	}
	for (int i = 0; ok && i < 3; i++) {
		ok = l->rail[i] < 0 || (Mode_Import("Traffic") && LvN_ModSetI("Traffic", rails[i], l->rail[i]));
	}
	if (ok && l->police >= 0) {
		ok = Mode_Import("OutPolice") && LvN_ModSetI("OutPolice", "PoliceActive", l->police);
	}
	return ok;
}

/* a fixed character: a character of the class standing at its place, without the AI and the logic of an NPC (a
 * level without the graphs of the AI has no way for them to walk) and not brought back after a death */
static void Pl_FixedNpc(const PlaceNpc *c) {
	char cls[PLACE_NAME], key[PLACE_NAME + 16];
	vec3_t p, ang = { 0.0f, 0.0f, 0.0f };
	nativeVal_t name;
	if (World_FindEntity(c->name) || !Pl_Pos(&c->pos, &p, &ang.y)) {
		return;
	}
	Str_PrintfQuiet(cls, sizeof(cls), "%.*s", (int)strcspn(c->name, "_"), c->name);
	cEntity *e = cWorld_CreateEntity(g_pWorld, c->name, p.x, p.y, p.z, TYEN_WALKCHAR);
	if (!e) {
		return;
	}
	WithLife_SetActCtrl(e, cls);
	Obj_SetAng(e, &ang);
	Str_PrintfQuiet(key, sizeof(key), "CharName_%s", cls);
	char *desc = (char *)Script_Lang(key);
	int one = 1;
	if (desc && *desc) {
		Entity_VarS(e, "Descriptor", &desc, 1);
	}
	Entity_VarI(e, "ArrowCanView", &one, 1);
	Native_SetStr(&name, c->name);
	if (!LvN_SetOnFloor(&name)) {
		Native_ErrPrint();
	}
	Native_Free(&name);
}

/* ---- the level start (both sides): the doors ---- */

#define PL_GATE_PREFIX "PlaceGate_"

/* the language's name of a level (its metro station's), else the level */
static const char *Pl_LevelText(const char *level) {
	char key[PLACE_NAME + 16];
	Str_PrintfQuiet(key, sizeof(key), "Station_%s", level);
	const char *t = Script_Lang(key);
	return t && *t ? t : level;
}

/* the use point of a gate: its options are the levels it leads to, on the radar */
static void Pl_MakeGate(const PlaceGate *g) {
	char name[PLACE_NAME + 16], opt[PL_TEXT + 16], text[PL_TEXT], var[16];
	vec3_t p;
	float yaw;
	Str_PrintfQuiet(name, sizeof(name), PL_GATE_PREFIX "%s", g->name);
	if (World_FindEntity(name) || !Pl_Pos(&g->pos, &p, &yaw)) {
		return;
	}
	cEntity *e = cWorld_CreateEntity(g_pWorld, name, p.x, p.y + 100.0f, p.z, TYEN_WITH_LIFE);
	if (!e) {
		return;
	}
	int one = 1, zero = 0, radar = 2, rad = 150;
	char *s;
	Entity_VarI(e, "VRadius", &rad, 1);
	Entity_VarI(e, "ArrowCanUse", &one, 1);
	Entity_VarI(e, "ArrowCanView", &one, 1);
	Entity_VarI(e, "AlwaysUse", &one, 1);
	Entity_VarI(e, "ActiveCMask", &zero, 1);
	Entity_VarI(e, "RadarShow", &radar, 1);
	/* the options: the list emptied, then each one in turn */
	Entity_VarI(e, "ArrnOpt", &zero, 1);
	for (int k = 0; k < g->nopt; k++) {
		Texts_Format(text, sizeof(text), "MP_Place_Go", Pl_LevelText(g->opt[k].level));
		Str_PrintfQuiet(opt, sizeof(opt), "Go%d:%s", k, text);
		Str_PrintfQuiet(var, sizeof(var), "ArrOpt%d", k);
		s = opt;
		Entity_VarS(e, var, &s, 1);
	}
	Con_Printf("places: gate %s at %.0f %.0f %.0f, %d ways\n", name, (double)p.x, (double)p.y, (double)p.z, g->nopt);
}

byte Places_LevelInit(void) {
	const PlaceLevel *l = Pl_Here();
	byte ok = 1;
	if (!l) {
		return 1;
	}
	for (int i = 0; ok && i < l->nlinks; i++) {
		/* the door's options: the levels (Elevator.ConnectElevators([door], levels)) */
		nativeVal_t doors, maps;
		Native_SetEmpty(&doors, NV_LIST);
		Native_SetStr(Native_Append(&doors), l->link[i].door);
		Pl_List(&maps, l->link[i].levels);
		ok = Mode_Import("Elevator") && PORT_CALL(Elevator, ConnectElevators, NULL, "vv", &doors, &maps);
		Native_Free(&doors);
		Native_Free(&maps);
	}
	for (int i = 0; ok && i < l->ngates; i++) {
		Pl_MakeGate(&l->gate[i]);
	}
	for (int i = 0; g_Instance && i < l->nnpcs; i++) {
		if (l->npc[i].fixed) {
			Pl_FixedNpc(&l->npc[i]);
		}
	}
	for (int i = 0; i < l->nnouse; i++) {
		cEntity *e = World_FindEntity(l->nouse[i]);
		int zero = 0;
		if (e && Entity_IsType(e, TYEN_WITH_LIFE)) {
			Entity_VarI(e, "ArrowCanUse", &zero, 1);
		}
	}
	if (ok && g_Instance && l->intraffic[0] >= 0) {
		static const char *const nums[3] = { "N_NumTraffic", "M_NumTraffic", "H_NumTraffic" };
		ok = Mode_Import("InTraffic");
		for (int k = 0; ok && k < 3; k++) {
			ok = LvN_ModSetI("InTraffic", nums[k], l->intraffic[k]);
		}
		ok = ok && PORT_CALL(InTraffic, FakeTraffic, NULL, "");
	}
	return ok;
}

/* ---- the gates: the use (the instance) and the arrival ---- */

static const PlaceGate *Pl_GateOf(const cEntity *e) {
	const PlaceLevel *l = Pl_Here();
	if (!l || !e || strncmp(e->name, PL_GATE_PREFIX, sizeof(PL_GATE_PREFIX) - 1)) {
		return NULL;
	}
	return PlaceDef_Gate(l, e->name + sizeof(PL_GATE_PREFIX) - 1);
}

/* player i takes option k of the gate g: to that level, at its gate */
static byte Pl_Take(int i, const PlaceGate *g, int k) {
	char entrance[PLACE_NAME + 16];
	if (k < 0 || k >= g->nopt) {
		k = 0;
	}
	entrance[0] = 0;
	if (!strncmp(g->opt[k].gate, "DM_", 3)) {
		/* a dummy of the level */
		Str_Copy(entrance, g->opt[k].gate, sizeof(entrance));
	} else if (g->opt[k].gate[0]) {
		Str_PrintfQuiet(entrance, sizeof(entrance), PL_GATE_PREFIX "%s", g->opt[k].gate);
	}
	Con_Printf("places: player %d takes the gate %s to %s\n", i, g->name, g->opt[k].level);
	return Travel_SendAt(i, g->opt[k].level, entrance);
}

void Places_Input(int i, cEntity *e, NetRepInput *in) {
	if (i < 0 || i >= PLAYERS_MAX || !e || NetPl_Kind(e) != NETREP_CHAR) {
		return;
	}
	cEntity *t = WChar_Victim(e);
	const PlaceGate *g = Pl_GateOf(t);
	byte use = (in->buttons & (NETREP_IN_SWITCH | NETREP_IN_ACT3)) != 0;
	if (g && use) {
		if (!pl.useHeld[i]) {
			Pl_Take(i, g, WLife(t)->arrow.iOpt);
		}
		in->buttons &= (uint16_t)~(NETREP_IN_SWITCH | NETREP_IN_ACT3);
	}
	pl.useHeld[i] = g && use;
}

byte Places_TestUse(int player, cEntity *t, int option) {
	const PlaceGate *g = Pl_GateOf(t);
	return g && Pl_Take(player, g, option);
}

byte Places_Entrance(const char *entrance, vec3_t *pos, float *yaw) {
	const PlaceLevel *l = Pl_Here();
	if (!l || strncmp(entrance, PL_GATE_PREFIX, sizeof(PL_GATE_PREFIX) - 1)) {
		return 0;
	}
	const PlaceGate *g = PlaceDef_Gate(l, entrance + sizeof(PL_GATE_PREFIX) - 1);
	return g && Pl_Pos(&g->pos, pos, yaw);
}

int Places_Spawns(vec3_t *pos, float *yaw, int max) {
	const PlaceLevel *l = g_Instance ? Pl_Here() : NULL;
	int n = 0;
	for (int i = 0; l && i < l->nspawns && n < max; i++) {
		n += Pl_Pos(&l->spawn[i], &pos[n], &yaw[n]);
	}
	return n;
}

/* ---- tests ---- */

/* the entity moved more than PL_MOVED since the last census (a new one counts as not moving); its position into the
 * next census's table at *n */
static byte Pl_Moved(const cEntity *e, int *n, const cEntity **nextE, vec3_t *next) {
	byte moved = 0;
	for (int k = 0; k < pl.ntrack; k++) {
		if (pl.track[k].e == e) {
			float dx = e->pos.x - pl.track[k].pos.x, dy = e->pos.y - pl.track[k].pos.y;
			float dz = e->pos.z - pl.track[k].pos.z;
			moved = dx * dx + dy * dy + dz * dz > PL_MOVED * PL_MOVED;
			break;
		}
	}
	if (*n < PL_TRACK) {
		nextE[*n] = e;
		next[*n] = e->pos;
		(*n)++;
	}
	return moved;
}

/* "census": the characters and the ships of the level that are no player's, how many of them moved since the last
 * census, their classes */
static void Pl_Census(void) {
	static vec3_t next[PL_TRACK];
	static const cEntity *nextE[PL_TRACK];
	char classes[512];
	const char *cls[32];
	int ncls[32], nc = 0, n = 0;
	int chars = 0, charsMoved = 0, cars = 0, carsAI = 0, carsMoved = 0;

	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (Players_Of(e) >= 0 || e->deadNode.target) {
			continue;
		}
		byte isChar = Entity_IsType(e, TYEN_WALKCHAR), isCar = Entity_IsType(e, TYEN_CAR);
		if (!isChar && !isCar) {
			continue;
		}
		byte moved = Pl_Moved(e, &n, nextE, next);
		if (isChar) {
			chars++;
			charsMoved += moved;
			cActCtrl *a = WLife(e)->actCtrl;
			const char *c = a && a->cls ? a->cls->name : "-";
			int k = 0;
			while (k < nc && strcmp(cls[k], c)) {
				k++;
			}
			if (k == nc && nc < 32) {
				cls[nc] = c;
				ncls[nc++] = 0;
			}
			if (k < nc) {
				ncls[k]++;
			}
		} else {
			cars++;
			carsAI += Entity_GetAI(e) != NULL;
			carsMoved += moved;
		}
	}
	for (int k = 0; k < n; k++) {
		pl.track[k].e = nextE[k];
		pl.track[k].pos = next[k];
	}
	pl.ntrack = n;
	int len = 0;
	classes[0] = 0;
	for (int k = 0; k < nc && len < (int)sizeof(classes) - 48; k++) {
		len += Str_PrintfQuiet(classes + len, (int)sizeof(classes) - len, "%s%s %d", k ? ", " : "", cls[k], ncls[k]);
	}
	Con_Printf("places: census %s: characters %d (moved %d), ships %d (with AI %d, moved %d); %s\n", Pl_Level(), chars,
		charsMoved, cars, carsAI, carsMoved, classes);
}

byte Places_Command(const char *line) {
	char pat[64], found[64];
	vec3_t pos, ang;

	if (!strcmp(line, "census")) {
		Pl_Census();
		return 1;
	}
	int player;
	if (sscanf(line, "nearest %d %63s", &player, pat) == 2) {
		/* the player next to the nearest character or AI ship of the level (screenshots) */
		cEntity *me = Players_Get(player), *best = NULL;
		float bestD = 1e30f;
		byte cars = !strcmp(pat, "car");
		for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); me && e; e = cEntity_Next(e)) {
			if (Players_Of(e) >= 0 || e->deadNode.target || (cars ? !Entity_IsType(e, TYEN_CAR) || !Entity_GetAI(e) :
				!Entity_IsType(e, TYEN_WALKCHAR))) {
				continue;
			}
			float dx = e->pos.x - me->pos.x, dy = e->pos.y - me->pos.y, dz = e->pos.z - me->pos.z;
			float d = dx * dx + dy * dy + dz * dz;
			if (d < bestD) {
				bestD = d;
				best = e;
			}
		}
		Con_Printf("places: nearest %s of player %d: %s\n", pat, player, best ? best->name : "none");
		if (best) {
			Travel_TestGoto(player, best->name, cars ? 1500.0f : 400.0f, 0.0f);
		}
		return 1;
	}
	pat[0] = 0;
	if (!strncmp(line, "dummies", 7)) {
		sscanf(line, "dummies %63s", pat);
		/* "*" would go on to the nodes after the dummies (a CHECK of the engine) */
		DummySearch_Start(pat[0] ? pat : "DM_*", 1);
		while (DummySearch_NextEuler(found, sizeof(found), &pos, &ang)) {
			Con_Printf("places: dummy %s at %.0f %.0f %.0f yaw %.2f\n", found, (double)pos.x, (double)pos.y,
				(double)pos.z, (double)ang.y);
		}
		return 1;
	}
	if (!strncmp(line, "ents", 4)) {
		sscanf(line, "ents %63s", pat);
		for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
			if (pat[0] && !Wild_Match(pat, e->name)) {
				continue;
			}
			cActCtrl *a = Entity_IsType(e, TYEN_WITH_LIFE) ? WLife(e)->actCtrl : NULL;
			Con_Printf("places: entity %s (%s, %s) at %.0f %.0f %.0f\n", e->name, e->vftable->GetClassName(e),
				a && a->cls ? a->cls->name : "-", (double)e->pos.x, (double)e->pos.y, (double)e->pos.z);
		}
		return 1;
	}
	return 0;
}
