/*
 * The cutscenes in every mode of the second multiplayer design (stage 4.5, -mp2), see include/scrap/netscene.h: the
 * scene's audience and records on the instance, the skips and the coop rule, the client's playback from its own
 * tables with the server's replicas, the scene screenshots of the tests. Not in the original.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/crt.h"
#include "scrap/game.h"
#include "scrap/instance.h"
#include "scrap/net2.h"
#include "scrap/netaudit.h"
#include "scrap/netrep.h"
#include "scrap/netscene.h"
#include "scrap/objentity.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/pres.h"
#include "scrap/pyfunction.h"
#include "scrap/replicate.h"
#include "scrap/repent.h"
#include "scrap/scenelist.h"
#include "scrap/scrap.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptports.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"

#include "game/Script/levels/Levels.h"

#include "../Global/testchannel.h"

#define NS_NAME 128
#define NS_HIDE 256					/* entities a client hides for itself in a scene */
#define NS_SHOTS 8
#define NS_GONE_NS 2000000000ull	/* the scene entity's replica gone that long without an end: the end here */

enum { NS_SKIP_ALL, NS_SKIP_ANY, NS_SKIP_HOST, NS_SKIP_NONE };

static const char ns_rules[] = {
#embed "../../../data/coop/rules.def"
	, 0 };

static struct {
	/* instance */
	byte on;
	char cls[NS_NAME], ent[NS_NAME];
	int player;						/* the scene's player outside coop, PLAYERS_NONE */
	uint64_t mask, skips;
	int flags;
	int rule;						/* NS_SKIP_, -1 before the rules were read */
	char path[PLAYERS_MAX][NS_NAME];	/* the camera path each player's view 0 shows now, to send */
	/* client */
	byte pathOn;
	byte cOn, linked, seen;
	char cCls[NS_NAME], cEnt[NS_NAME];
	int cFlags, cTick;
	uint64_t goneNs;
	int nhid;
	char hid[NS_HIDE][64];
	/* screenshots */
	byte shotsRead;
	char shotDir[256];
	float shotT[NS_SHOTS];
	int nshots, shotsDone;
	char shotScene[NS_NAME];
} ns = { .player = PLAYERS_NONE, .rule = -1 };

/* ---- instance ---- */

byte NetScene_Derives(void) {
	return g_Mp2 && g_Instance;
}

byte NetScene_On(void) {
	return NetScene_Derives() && ns.on;
}

byte NetScene_LocalHide(void) {
	return NetScene_Derives() && !Coop_On();
}

int NetScene_Player(void) {
	return NetScene_On() && !Coop_On() ? ns.player : PLAYERS_NONE;
}

/* SceneSkip of data/coop/rules.def */
static int NetScene_Rule(void) {
	static const char *const names[] = { "all", "any", "host", "none" };
	if (ns.rule >= 0) {
		return ns.rule;
	}
	ns.rule = NS_SKIP_ALL;
	for (const char *p = ns_rules; *p;) {
		char key[32], val[32];
		const char *eol = strchr(p, '\n');
		int len = eol ? (int)(eol - p) : (int)strlen(p);
		char line[128];
		Str_PrintfQuiet(line, sizeof(line), "%.*s", len < 127 ? len : 127, p);
		if (line[0] != '#' && sscanf(line, "%31s %31s", key, val) == 2 && !strcmp(key, "SceneSkip")) {
			for (int k = 0; k < 4; k++) {
				if (!strcmp(val, names[k])) {
					ns.rule = k;
				}
			}
		}
		p += len + (eol != NULL);
	}
	return ns.rule;
}

/* the players of the instance with a connection */
static uint64_t NetScene_Online(void) {
	uint64_t m = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cPlayer *p = Players_Ctx(i);
		if (p && p->used && Rep_PlayerConn(i) >= 0) {
			m |= (uint64_t)1 << i;
		}
	}
	return m;
}

static void NetScene_Record(uint64_t mask) {
	if (Pres_BeginMask(mask, PRES_O_SCENE)) {
		Pres_PutStr(ns.cls);
		Pres_PutStr(ns.ent);
		Pres_PutInt((int)(Instance_Time() * NETREP_TICK_HZ));
		Pres_PutInt(ns.flags);
		Pres_Send();
	}
}

void NetScene_Started(const char *cls, const char *ent) {
	nativeVal_t out = { 0 };
	if (!NetScene_Derives()) {
		return;
	}
	byte coop = Coop_On();
	int i = Players_Current();
	if (!coop && i == PLAYERS_NONE) {
		i = Players_Acting();
	}
	ns.on = 1;
	Str_Copy(ns.cls, cls, sizeof(ns.cls));
	Str_Copy(ns.ent, ent, sizeof(ns.ent));
	ns.player = coop ? PLAYERS_NONE : i;
	ns.mask = coop || i == PLAYERS_NONE ? ~(uint64_t)0 : (uint64_t)1 << i;
	ns.skips = 0;
	Native_SetNone(&out);
	byte outdoor = Mode_Import("Init") && Mode_ModGet("Init", "Outdoor", &out) && Native_True(&out);
	Native_Free(&out);
	Native_ErrClear();
	ns.flags = (coop ? 0 : NETSCENE_F_LOCALHIDE) | (outdoor ? NETSCENE_F_OUTDOOR : 0);
	NetScene_Record(ns.mask);
	NetAudit_Name(NA_HUD, "scene.start:", cls);
	if (ns.player == PLAYERS_NONE) {
		Con_Printf("scene: %s (%s) plays for every player\n", cls, ent);
	} else {
		Con_Printf("scene: %s (%s) plays for player %d\n", cls, ent, ns.player);
	}
}

void NetScene_Ending(void) {
	if (!NetScene_On()) {
		return;
	}
	if (Pres_BeginMask(ns.mask, PRES_O_SCENEEND)) {
		Pres_PutStr(ns.cls);
		Pres_Send();
	}
	ns.on = 0;
	NetAudit_Name(NA_HUD, "scene.end:", ns.cls);
	Con_Printf("scene: %s ends\n", ns.cls);
}

void NetScene_Joined(int i) {
	if (!NetScene_On() || i < 0 || i >= PLAYERS_MAX || !(ns.mask >> i & 1)) {
		return;
	}
	NetScene_Record((uint64_t)1 << i);
	Con_Printf("scene: player %d joins scene %s at its current time\n", i, ns.cls);
}

/* the scene ends for its audience now, as Escape ends it in single player (in the context of player i outside coop) */
static void NetScene_Abort(int i) {
	int was = Players_Current();
	if (i != PLAYERS_NONE && i != was) {
		Players_Enter(i);
	}
	NetAudit_Name(NA_HUD, "scene.skip:", ns.cls);
	Con_Printf("scene: %s skipped\n", ns.cls);
	if (!Mode_Import("Scene") || !PORT_CALL(Scene, AbortScene, NULL, "i", 0L)) {
		Native_ErrPrint();
	}
	if (Players_Current() != was) {
		Players_Leave(was);
	}
}

void NetScene_Skip(int i) {
	if (!NetScene_On() || i < 0 || i >= PLAYERS_MAX || !(ns.mask >> i & 1)) {
		NetAudit_Site(NA_HUD, "scene.skip.refused");
		return;
	}
	if (!Coop_On()) {
		if (ns.player == PLAYERS_NONE || ns.player == i) {
			NetScene_Abort(i);
		}
		return;
	}
	ns.skips |= (uint64_t)1 << i;
	uint64_t want = ns.mask & NetScene_Online();
	int rule = NetScene_Rule();
	Con_Printf("scene: player %d asks to skip %s\n", i, ns.cls);
	if ((rule == NS_SKIP_ALL && (ns.skips & want) == want) || rule == NS_SKIP_ANY || (rule == NS_SKIP_HOST && i == 0)) {
		NetScene_Abort(PLAYERS_NONE);
	}
}

byte NetScene_Message(Net2Host *host, int conn, const uint8_t *data, int size) {
	(void)host;
	if (!NetScene_Derives() || size < 2 || data[0] != NETSCENE_MSG) {
		return 0;
	}
	if (data[1] == NETSCENE_C_SKIP) {
		NetScene_Skip(Rep_ConnPlayer(conn));
	}
	return 1;
}

void NetScene_CamSet(cEntity *cam) {
	int i = Players_Current();
	if (!NetScene_Derives() || i < 0 || i >= PLAYERS_MAX || !cam || !Entity_IsType(cam, TYEN_CAMPATH)) {
		return;
	}
	Str_Copy(ns.path[i], cam->name, sizeof(ns.path[i]));
}

void NetScene_Flush(void) {
	for (int i = 0; NetScene_Derives() && i < PLAYERS_MAX; i++) {
		cEntity *e = ns.path[i][0] ? World_FindEntity(ns.path[i]) : NULL;
		if (!ns.path[i][0]) {
			continue;
		}
		ns.path[i][0] = 0;
		if (!e || !Entity_IsType(e, TYEN_CAMPATH)) {
			continue;
		}
		cCamPathEntity *p = (cCamPathEntity *)e;
		int n = p->numPoints < NETSCENE_PATH_POINTS ? p->numPoints : NETSCENE_PATH_POINTS;
		if (n < p->numPoints) {
			NetAudit_Site(NA_CAMERA, "scene.path.cut");
		}
		if (!Pres_BeginMask((uint64_t)1 << i, PRES_O_CAMPATH)) {
			continue;
		}
		Pres_PutStr(e->name);
		Pres_PutInt(n);
		Pres_PutFloat(p->time);
		for (int k = 0; k < n; k++) {
			const cCamPathPoint *pt = &p->points[k];
			cEntity *c = pt->cam[0] ? World_FindEntity(pt->cam) : NULL;
			/* a player's own camera (the follow camera of the server's context) is the client's own */
			Pres_PutStr(c && Entity_IsType(c, TYEN_FCAM) ? "@own" : pt->cam);
			Pres_PutVec(&pt->pos);
			Pres_PutVec(&pt->ang);
			Pres_PutFloat(pt->zNear);
			Pres_PutFloat(pt->zFar);
			Pres_PutFloat(pt->fov);
			Pres_PutFloat(pt->time);
			Pres_PutFloat(pt->quake);
			Pres_PutInt(pt->fadeIn);
			Pres_PutInt(pt->fadeOut);
		}
		Pres_Send();
		NetAudit_Site(NA_CAMERA, "scene.path");
	}
}

/* ---- client ---- */

#define NS_PATH "PlayPath"
#define NS_OWNCAM "PlayCam"			/* the client's own camera (Play.c) */

/* the length of a path: the times of its points up to the last */
static float NetScene_PathLength(const cCamPathEntity *p) {
	float t = 0.0f;
	for (int k = 0; k < p->numPoints - 1; k++) {
		t += p->points[k].time > 0.0f ? p->points[k].time : 0.0f;
	}
	return t;
}

void NetScene_ClientPath(const char *name, const netScenePath_t *path) {
	cEntity *e = World_FindEntity(NS_PATH);
	if (!e) {
		e = cWorld_CreateEntity(g_pWorld, NS_PATH, 0.0f, 0.0f, 0.0f, TYEN_CAMPATH);
	}
	if (!e || !Entity_IsType(e, TYEN_CAMPATH) || path->n < 1) {
		return;
	}
	cCamPathEntity *p = (cCamPathEntity *)e;
	p->numPoints = path->n;
	p->time = path->time;
	for (int k = 0; k < path->n; k++) {
		cCamPathPoint *pt = &p->points[k];
		Str_Copy(pt->cam, !strcmp(path->p[k].cam, "@own") ? NS_OWNCAM : path->p[k].cam, sizeof(pt->cam));
		pt->pos = path->p[k].pos;
		pt->ang = path->p[k].ang;
		pt->zNear = path->p[k].zNear;
		pt->zFar = path->p[k].zFar;
		pt->fov = path->p[k].fov;
		pt->time = path->p[k].time;
		pt->quake = path->p[k].quake;
		pt->fadeIn = (byte)path->p[k].fadeIn;
		pt->fadeOut = (byte)path->p[k].fadeOut;
	}
	cWorld_SetCam(g_pWorld, 0, NS_PATH);
	ns.pathOn = 1;
	NetAudit_Name(NA_CAMERA, "scene.client.path:", name);
	Con_Printf("scene: the camera path %s of the server (%d points, %.2f s) plays here\n", name, path->n,
		(double)NetScene_PathLength(p));
}

byte NetScene_ClientPathHolds(void) {
	return ns.pathOn;
}

/* each frame: the end of the path, the own camera again */
static void NetScene_PathFrame(void) {
	cEntity *e = ns.pathOn ? World_FindEntity(NS_PATH) : NULL;
	if (!ns.pathOn) {
		return;
	}
	if (!e || ((cCamPathEntity *)e)->time >= NetScene_PathLength((cCamPathEntity *)e)) {
		ns.pathOn = 0;
		if (World_FindEntity(NS_OWNCAM)) {
			cWorld_SetCam(g_pWorld, 0, NS_OWNCAM);
		}
		/* the HUD and input of the own entity it may have become during the path (a ship boarded) */
		if (!ns.cOn) {
			Play_SceneEnd();
		}
		Con_Printf("scene: the camera path ends here\n");
	}
}

static cEntity *NetScene_ViewCam(void) {
	const char *name = g_pWorld ? cWorld_GetCamName(g_pWorld, 0) : NULL;
	cEntity *cam = name && *name ? World_FindEntity(name) : NULL;
	return cam && Entity_IsType(cam, TYEN_CAM) ? cam : NULL;
}

/* the walking characters (and outdoors the ships) hidden here for the scene, the own entity among them */
static void NetScene_Hide(void) {
	ns.nhid = 0;
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e && ns.nhid < NS_HIDE; e = cEntity_Next(e)) {
		int active = 0;
		if (e->deadNode.target || !e->name || !(Entity_IsType(e, "WalkChar") ||
			((ns.cFlags & NETSCENE_F_OUTDOOR) && Entity_IsType(e, "Car")))) {
			continue;
		}
		if (Entity_VarI(e, "Active", &active, 0) && active) {
			int zero = 0;
			Entity_VarI(e, "Active", &zero, 1);
			Str_Copy(ns.hid[ns.nhid++], e->name, sizeof(ns.hid[0]));
		}
	}
}

static void NetScene_Unhide(void) {
	for (int k = 0; k < ns.nhid; k++) {
		cEntity *e = World_FindEntity(ns.hid[k]);
		int one = 1;
		if (e) {
			Entity_VarI(e, "Active", &one, 1);
		}
	}
	ns.nhid = 0;
}

void NetScene_ClientStart(const char *cls, const char *ent, int tick, int flags) {
	if (ns.cOn && !strcmp(ns.cEnt, ent)) {
		return;
	}
	if (ns.cOn) {
		NetScene_ClientEnd(ns.cCls);
	}
	if (!SceneList_Loaded(cls)) {
		const sceneInfo_t *s = SceneList_Find(cls);
		if (s && SceneList_Load(s)) {
			NetAudit_Name(NA_HUD, "scene.load:", cls);
		} else {
			NetAudit_Name(NA_HUD, "pres.missing:scene:", cls);
			Con_Printf("scene: no tables of scene %s here\n", cls);
		}
	}
	ns.cOn = 1;
	ns.linked = ns.seen = 0;
	ns.goneNs = 0;
	ns.cFlags = flags;
	ns.cTick = tick;
	Str_Copy(ns.cCls, cls, sizeof(ns.cCls));
	Str_Copy(ns.cEnt, ent, sizeof(ns.cEnt));
	if (!Scene_ClientBegin()) {
		Native_ErrPrint();
	}
	if (flags & NETSCENE_F_LOCALHIDE) {
		NetScene_Hide();
	}
	NetAudit_Name(NA_HUD, "scene.client:", cls);
	Con_Printf("scene: %s of the server (%s, tick %d) plays here\n", cls, ent, tick);
}

void NetScene_ClientEnd(const char *cls) {
	char *none = "";
	if (!ns.cOn || strcmp(cls, ns.cCls)) {
		return;
	}
	ns.cOn = 0;
	cEntity *cam = NetScene_ViewCam();
	if (cam) {
		Entity_VarS(cam, "EntityLink", &none, 1);
	}
	NetScene_Unhide();
	if (!Scene_ClientEnd()) {
		Native_ErrPrint();
	}
	Play_SceneEnd();
	Con_Printf("scene: %s ends here\n", cls);
}

void NetScene_ClientFrame(void) {
	NetScene_PathFrame();
	if (!ns.cOn) {
		return;
	}
	cEntity *e = World_FindEntity(ns.cEnt);
	if (e && Entity_IsType(e, TYEN_OBJECT) && Obj_GetModel(e)) {
		ns.seen = 1;
		ns.goneNs = 0;
		cEntity *cam = NetScene_ViewCam();
		char *link = NULL;
		if (cam && (!Entity_VarS(cam, "EntityLink", &link, 0) || !link || strcmp(link, ns.cEnt))) {
			char *name = ns.cEnt;
			Entity_VarS(cam, "EntityLink", &name, 1);
			if (!ns.linked) {
				Con_Printf("scene: the camera on %s\n", ns.cEnt);
			}
			ns.linked = 1;
		}
		return;
	}
	/* the replica went (its slot taken back) and no end came: the end here */
	uint64_t now = Plat_RealNs();
	if (ns.seen && !ns.goneNs) {
		ns.goneNs = now;
	} else if (ns.seen && now - ns.goneNs > NS_GONE_NS) {
		NetScene_ClientEnd(ns.cCls);
	}
}

void NetScene_ClientGone(cEntity *e) {
	if (ns.cOn && e && e->name && !strcmp(e->name, ns.cEnt)) {
		NetScene_ClientEnd(ns.cCls);
	}
}

byte NetScene_ClientHolds(void) {
	return ns.cOn;
}

byte NetScene_ClientSkip(void) {
	uint8_t buf[2 + NS_NAME + 2];
	NetRepBuf w;
	if (!ns.cOn) {
		return 0;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, NETSCENE_MSG);
	NetRep_PutU8(&w, NETSCENE_C_SKIP);
	NetRep_PutStr(&w, ns.cCls);
	if (Watch_SendReliable(buf, w.size)) {
		Con_Printf("scene: skip of %s asked\n", ns.cCls);
	}
	return 1;
}

/* the scene's own events the client plays: its 2D sounds (Event_IsPresent), its texts and streams */
static byte NetScene_SceneEvent(const cActEvent *ev) {
	if (Event_IsPresent(ev)) {
		return 1;
	}
	if (!(ev->func.flags & (PYF_ENTRY | PYF_VALUE))) {
		return 0;
	}
	const char *name = cPy_Function_Name((cPy_Function *)&ev->func);
	return !strcmp(name, "PlayText") || !strcmp(name, "PlayStream");
}

static byte NetScene_NoEvent(const cActEvent *ev) {
	(void)ev;
	return 0;
}

actEventFilter_t NetScene_Filter(cEntity *e) {
	if (!e->name || strncmp(e->name, "DM_Scene_", 9)) {
		return Event_IsPresent;
	}
	return ns.cOn && !strcmp(e->name, ns.cEnt) ? NetScene_SceneEvent : NetScene_NoEvent;
}

byte NetScene_Exact(cEntity *e) {
	if (!ns.cOn || !e->name) {
		return 0;
	}
	const cEntity *l = Entity_IsType(e, TYEN_OBJECT) ? Obj(e)->mod.link.target : NULL;
	return !strcmp(e->name, ns.cEnt) || (l && l->name && !strcmp(l->name, ns.cEnt));
}

void NetScene_ClientReset(void) {
	ns.cOn = 0;
	ns.pathOn = 0;
	ns.nhid = 0;
}

/* ---- both: the tests ---- */

void NetScene_TestVars(void) {
	const char *v = getenv("SCRAP_TEST_SAVEVARS");
	char item[256];
	for (const char *p = v; p && *p;) {
		const char *end = strchr(p, ';');
		int len = end ? (int)(end - p) : (int)strlen(p);
		Str_PrintfQuiet(item, sizeof(item), "%.*s", len < 255 ? len : 255, p);
		char *eq = strchr(item, '=');
		if (eq) {
			*eq = 0;
			SaveVar_Set(item, eq + 1);
			Con_Printf("scene: test save variable %s = %s\n", item, eq + 1);
		}
		p = end ? end + 1 : NULL;
	}
}


static void NetScene_ShotsRead(void) {
	const char *v = getenv("SCRAP_SCENE_SHOTS");
	ns.shotsRead = 1;
	const char *colon = v ? strrchr(v, ':') : NULL;
	if (!colon) {
		return;
	}
	Str_PrintfQuiet(ns.shotDir, sizeof(ns.shotDir), "%.*s", (int)(colon - v), v);
	for (const char *p = colon + 1; *p && ns.nshots < NS_SHOTS;) {
		/* f<x>: that part of the scene's length */
		ns.shotT[ns.nshots++] = *p == 'f' ? -(float)atof(p + 1) : (float)atof(p);
		p = strchr(p, ',');
		if (!p) {
			break;
		}
		p++;
	}
}

/* the scene entity of the scene that plays here: the server's (client) or the Scene module's, NULL */
static cEntity *NetScene_Playing(void) {
	if (g_Instance) {
		return NULL;
	}
	if (g_Mp2 && g_Play) {
		return ns.cOn ? World_FindEntity(ns.cEnt) : NULL;
	}
	nativeVal_t cur = { 0 };
	cEntity *e = NULL;
	Native_SetNone(&cur);
	if (Mode_Import("Scene") && Mode_ModGet("Scene", "SceneName", &cur) && cur.type == NV_STR &&
		!strncmp(Native_Text(&cur), "DM_Scene_", 9)) {
		e = World_FindEntity(Native_Text(&cur));
	}
	Native_Free(&cur);
	Native_ErrClear();
	return e;
}

void NetScene_ShotFrame(void) {
	static cEntity *lastPath;
	static int paths;
	char name[512], label[NS_NAME];
	float t, len;
	if (!ns.shotsRead) {
		NetScene_ShotsRead();
	}
	if (!ns.nshots || !g_pWorld || g_Instance) {
		return;
	}
	/* a scene by its entity's action, else a camera path on view 0 by its time ("Path<n>": the n-th of the run) */
	cEntity *e = NetScene_Playing();
	cActCtrl *ctrl = e && Entity_IsType(e, TYEN_WITH_LIFE) ? WLife(e)->actCtrl : NULL;
	cEntity *cam = NetScene_ViewCam();
	if (ctrl && ctrl->layers[0].on) {
		Str_Copy(label, e->name + 9, sizeof(label));
		t = ctrl->layers[0].time;
		len = ctrl->layers[0].action ? ctrl->layers[0].action->anmTotalTime : 0.0f;
	} else if (cam && Entity_IsType(cam, TYEN_CAMPATH)) {
		cCamPathEntity *p = (cCamPathEntity *)cam;
		if (cam != lastPath || p->time < 0.05f) {
			if (cam != lastPath || ns.shotsDone) {
				paths++;
			}
			lastPath = cam;
		}
		Str_PrintfQuiet(label, sizeof(label), "Path%d", paths);
		t = p->time;
		len = NetScene_PathLength(p);
	} else {
		lastPath = NULL;
		return;
	}
	if (strcmp(ns.shotScene, label)) {
		Str_Copy(ns.shotScene, label, sizeof(ns.shotScene));
		ns.shotsDone = 0;
	}
	float at = ns.shotsDone < ns.nshots ? ns.shotT[ns.shotsDone] : 0.0f;
	if (at < 0.0f) {
		at = -at * len;
	}
	if (ns.shotsDone < ns.nshots && t >= at) {
		Str_PrintfQuiet(name, sizeof(name), "%s/%s-%d", ns.shotDir, label, ns.shotsDone);
		Screenshot_Take(name);
		Con_Printf("scene: shot %s %d at %.3f\n", label, ns.shotsDone, (double)t);
		ns.shotsDone++;
	}
}

/* ---- the test verb ---- */

void NetScene_Verb(tcBuf *b, const char *arg) {
	char word[32] = "", what[64] = "";
	int player = PLAYERS_NONE;
	int n = sscanf(arg ? arg : "", "%31s %63s %d", word, what, &player);
	if (n < 1 || !strcmp(word, "state")) {
		TC_Addf(b, "scene: %s\n", !g_Mp2 ? "single player" : g_Instance ? "instance" : "client");
		if (!g_Mp2) {
			/* single player: the time line below */
		} else if (g_Instance) {
			TC_Addf(b, "server\t%s\t%s\tplayer %d\tmask %llx\tskips %llx\n", ns.on ? ns.cls : "-", ns.on ? ns.ent : "-",
				ns.player, (unsigned long long)ns.mask, (unsigned long long)ns.skips);
		} else {
			TC_Addf(b, "client\t%s\t%s\tlinked %d\thidden %d\n", ns.cOn ? ns.cCls : "-", ns.cOn ? ns.cEnt : "-", ns.linked,
				ns.nhid);
		}
		cEntity *e = NetScene_Playing();
		if (!e && g_Instance && ns.on) {
			e = World_FindEntity(ns.ent);
		}
		cActCtrl *ctrl = e && Entity_IsType(e, TYEN_WITH_LIFE) ? WLife(e)->actCtrl : NULL;
		if (ctrl) {
			TC_Addf(b, "time\t%s\t%.3f\n", e->name, (double)ctrl->layers[0].time);
		}
		return;
	}
	if (!strcmp(word, "list")) {
		for (int i = 0; i < SceneList_Count(); i++) {
			const sceneInfo_t *s = SceneList_At(i);
			TC_Addf(b, "%s\t%s\t%s\t%s.%s\t%s\n", s->scene, s->level, *s->mission ? s->mission : "-", s->module, s->load,
				s->actors);
		}
		return;
	}
	if (!strcmp(word, "play") && n >= 2) {
		int was = Players_Current();
		if (g_Instance && player != PLAYERS_NONE && player != was) {
			Players_Enter(player);
		}
		/* the screen clear first, as the missions fade in before a scene: a scene that ended black (its last line's fade
		 * out) leaves no black for the next one on either side */
		byte ok = Mode_Fade(0, lv_clear, lv_clear, 0.0) && SceneList_Play(what);
		if (Players_Current() != was) {
			Players_Leave(was);
		}
		TC_Addf(b, "%s\n", ok ? "playing" : "error: no such scene or its load failed");
		return;
	}
	if (!strcmp(word, "skip")) {
		if (g_Instance) {
			NetScene_Skip(n >= 2 ? atoi(what) : 0);
		} else if (!NetScene_ClientSkip() && Mode_Import("Scene") && !PORT_CALL(Scene, AbortScene, NULL, "i", 0L)) {
			Native_ErrPrint();
		}
		TC_Add(b, "ok\n", -1);
		return;
	}
	TC_Add(b, "error: scene [state|list|play <name> [player]|skip [player]]\n", -1);
}
