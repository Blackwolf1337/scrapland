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
#include "scrap/party.h"
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
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"

#include "game/Script/levels/Levels.h"

#include "../Global/testchannel.h"

#define NS_NAME 128
#define NS_HIDE 256					/* entities a client hides for itself in a scene */
#define NS_SHOTS 8
#define NS_GONE_NS 2000000000ull	/* the scene entity's replica gone that long without an end: the end here */
#define NS_CAMWAITS 4				/* camera targets that wait for their replica */
#define NS_CAMWAIT_NS 2000000000ull	/* that long at most */
#define NS_CAMNUMS 8

enum { NS_SKIP_ALL, NS_SKIP_ANY, NS_SKIP_HOST, NS_SKIP_NONE };

static struct {
	/* instance */
	byte on;
	char cls[NS_NAME], ent[NS_NAME];
	int player;						/* the scene's player outside coop, PLAYERS_NONE */
	uint64_t mask, skips;
	uint64_t told;					/* the players of the audience that got the scene's record */
	int flags;
	int rule;						/* NS_SKIP_, -1 before the rules were read */
	char path[PLAYERS_MAX][NS_NAME];	/* the camera path each player's view 0 shows now, to send */
	uint32_t fadeFrom, fadeTo;		/* the last fade of the scene (fadeAt < 0: none) */
	float fadeTime;
	double fadeAt;					/* Instance_Time it started */
	/* client */
	struct {
		char name[16], value[NS_NAME];
		uint64_t ns;				/* when it came, 0 free */
	} camWait[NS_CAMWAITS];
	struct {
		char name[16];
		float f;
	} camNum[NS_CAMNUMS];			/* the numbers set while a target waits, again after it */
	int ncamNum;
	byte pathOn;
	byte pathLocal;					/* the path is one the client started itself (NetScene_ClientOwnPath) */
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
	byte shotNow;					/* a shot is taken in this frame */
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

/* SceneSkip of data/coop/rules.def (include/scrap/party.h) */
static int NetScene_Rule(void) {
	static const char *const names[] = { "all", "any", "host", "none" };
	if (ns.rule >= 0) {
		return ns.rule;
	}
	const char *v = Party_Rule("SceneSkip", "all");
	ns.rule = NS_SKIP_ALL;
	for (int k = 0; k < 4; k++) {
		if (!strcmp(v, names[k])) {
			ns.rule = k;
		}
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
	ns.told = ns.mask & NetScene_Online();
	ns.fadeAt = -1.0;
	NetScene_Record(ns.told);
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

byte NetScene_Timed(int i) {
	return NetScene_On() && i >= 0 && i < PLAYERS_MAX && (ns.mask >> i & 1);
}

void NetScene_FadeSent(uint32_t from, uint32_t to, float time) {
	if (NetScene_On()) {
		ns.fadeFrom = from;
		ns.fadeTo = to;
		ns.fadeTime = time;
		ns.fadeAt = Instance_Time();
	}
}

/* the last fade of the scene as far as it is now, to the players of mask (a joiner: its screen as the others') */
static void NetScene_FadeNow(uint64_t mask) {
	if (ns.fadeAt < 0.0 || !Pres_BeginMask(mask, PRES_O_FADE)) {
		return;
	}
	float left = ns.fadeTime - (float)(Instance_Time() - ns.fadeAt);
	float f = ns.fadeTime > 0.0f && left > 0.0f ? left / ns.fadeTime : 0.0f;
	uint32_t now = 0;
	/* the colour of ScorerFade_Draw at this point: f of the start, 1 - f of the end, per byte */
	for (int sh = 0; sh < 32; sh += 8) {
		float a = (float)(ns.fadeFrom >> sh & 0xff), b = (float)(ns.fadeTo >> sh & 0xff);
		now |= (uint32_t)(int)(b * (1.0f - f) + a * f) << sh;
	}
	Pres_PutInt((int)now);
	Pres_PutInt((int)ns.fadeTo);
	Pres_PutFloat(left > 0.001f ? left : 0.001f);
	Pres_Send();
}

void NetScene_Joined(int i) {
	/* its record goes with the next records (NetScene_Flush) */
	if (NetScene_On() && i >= 0 && i < PLAYERS_MAX && (ns.mask >> i & 1)) {
		ns.told &= ~((uint64_t)1 << i);
	}
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

/* cam is the camera of player i's view 0, or one a camera path there shows */
static byte NetScene_OwnCam(const cEntity *cam, int i) {
	const char *view = i == PLAYERS_NONE ? cWorld_GetCamName(g_pWorld, 0) : Players_Ctx(i) ? Players_Ctx(i)->camName : "";
	if (!view || !*view || !cam->name) {
		return 0;
	}
	if (!strcmp(view, cam->name)) {
		return 1;
	}
	cEntity *v = World_FindEntity(view);
	if (v && Entity_IsType(v, TYEN_CAMPATH)) {
		const cCamPathEntity *p = (const cCamPathEntity *)v;
		for (int k = 0; k < p->numPoints; k++) {
			if (!strcmp(p->points[k].cam, cam->name)) {
				return 1;
			}
		}
	}
	return 0;
}

void NetScene_CamVar(cEntity *cam, const char *name, double d, const char *s) {
	int i = Players_Current();
	if (!Pres_Out() || !cam) {
		return;
	}
	/* coop: the world's camera (the party's scenes, in any context) to every player */
	if (Coop_On() && NetScene_OwnCam(cam, PLAYERS_NONE)) {
		i = PLAYERS_NONE;
	} else if (i == PLAYERS_NONE || !NetScene_OwnCam(cam, i)) {
		return;
	}
	if (!Pres_BeginMask(i == PLAYERS_NONE ? ~(uint64_t)0 : (uint64_t)1 << i, PRES_O_CAMVAR)) {
		return;
	}
	Pres_PutStr(name);
	Pres_PutInt(s != NULL);
	if (s) {
		/* the player's own entity is the client's own ("@own"): its name there may change with a new own entity */
		cEntity *e = i != PLAYERS_NONE && *s ? World_FindEntity(s) : NULL;
		Pres_PutStr(e && (e == Players_Get(i) || e == Players_Ship(i)) ? "@own" : s);
	} else {
		Pres_PutFloat((float)d);
	}
	Pres_Send();
	NetAudit_Name(NA_CAMERA, "scene.camvar:", name);
}

void NetScene_Flush(void) {
	/* a player of the audience that came while the scene plays joins it at its current time (the replica of the
	 * scene entity brings the server's action time) */
	if (NetScene_On()) {
		uint64_t online = NetScene_Online(), want = ns.mask & online & ~ns.told;
		ns.told &= online;
		if (want) {
			NetScene_Record(want);
			NetScene_FadeNow(want);
			ns.told |= want;
			for (int i = 0; i < PLAYERS_MAX; i++) {
				if (want >> i & 1) {
					Con_Printf("scene: player %d joins scene %s at its current time\n", i, ns.cls);
				}
			}
		}
	}
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
		Pres_PutInt((int)(Instance_Time() * 1000.0));
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
	/* the server's path of a flow the client started for the eye goes on from where the client's is */
	float was = p->time;
	p->numPoints = path->n;
	/* the own entity is drawn at its predicted time, ahead of the server's: the path as far as it is at that time,
	 * so the camera and the own ship it shows (boarded, parked) keep single player's timing */
	double own = Play_OwnTime();
	float ahead = own > 0.0 && path->sent > 0.0 && own > path->sent ? (float)(own - path->sent) : 0.0f;
	float at = path->time + (ahead < 0.5f ? ahead : 0.5f);
	p->time = ns.pathLocal && was > at ? was : at;
	ns.pathLocal = 0;
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

/* a camera's target of the server's name: its replica or the own entity here; 0 when it is not here (yet) */
static byte NetScene_CamTarget(cEntity *cam, const char *name, const char *s) {
	cEntity *e = !strcmp(s, "@own") ? Play_Entity() : *s ? ReplicaEnt_Find(s) : NULL;
	char *v = e ? e->name : "";
	if (*s && !e) {
		return 0;
	}
	Entity_VarS(cam, name, &v, 1);
	return 1;
}

/* the targets that wait for their replica (a character the scripts made in the same frame) */
static void NetScene_CamWaits(void) {
	cEntity *cam = World_FindEntity(NS_OWNCAM);
	uint64_t now = Plat_RealNs();
	for (int k = 0; k < NS_CAMWAITS; k++) {
		if (!ns.camWait[k].ns) {
			continue;
		}
		if (cam && NetScene_CamTarget(cam, ns.camWait[k].name, ns.camWait[k].value)) {
			ns.camWait[k].ns = 0;
			/* the numbers the scripts set with it again: the camera changed its distance for the target it had */
			for (int n = 0; n < ns.ncamNum; n++) {
				double d = ns.camNum[n].f;
				Entity_VarF(cam, ns.camNum[n].name, &d, 1);
			}
			ns.ncamNum = 0;
		} else if (now - ns.camWait[k].ns > NS_CAMWAIT_NS) {
			Pres_ClientMissing("camvar", ns.camWait[k].value);
			ns.camWait[k].ns = 0;
		}
	}
}

void NetScene_ClientCamVar(const char *name, const char *s, float f) {
	cEntity *cam = World_FindEntity(NS_OWNCAM);
	if (!cam) {
		return;
	}
	if (s) {
		/* the camera ride of an elevator the client started for the eye (include/scrap/play.h) */
		if (!strcmp(name, "EntityLink") && *s) {
			PlayFlow_Took(PLAYFLOW_ELEV, s);
		}
		if (!NetScene_CamTarget(cam, name, s)) {
			int k = 0;
			/* the same variable's wait is replaced, else a free one (the oldest given up when there is none) */
			while (k < NS_CAMWAITS - 1 && ns.camWait[k].ns && strcmp(ns.camWait[k].name, name)) {
				k++;
			}
			if (ns.camWait[k].ns && strcmp(ns.camWait[k].name, name)) {
				Pres_ClientMissing("camvar", ns.camWait[k].value);
			}
			Str_Copy(ns.camWait[k].name, name, sizeof(ns.camWait[k].name));
			Str_Copy(ns.camWait[k].value, s, sizeof(ns.camWait[k].value));
			ns.camWait[k].ns = Plat_RealNs();
			return;
		}
	} else {
		double d = f;
		Entity_VarF(cam, name, &d, 1);
		byte waits = 0;
		for (int k = 0; k < NS_CAMWAITS; k++) {
			waits |= ns.camWait[k].ns != 0;
		}
		if (waits && ns.ncamNum < NS_CAMNUMS) {
			Str_Copy(ns.camNum[ns.ncamNum].name, name, sizeof(ns.camNum[0].name));
			ns.camNum[ns.ncamNum++].f = f;
		}
	}
	NetAudit_Name(NA_CAMERA, "scene.client.camvar:", name);
}

cEntity *NetScene_ClientOwnPath(void) {
	cEntity *e = World_FindEntity(NS_PATH);
	if (!e) {
		e = cWorld_CreateEntity(g_pWorld, NS_PATH, 0.0f, 0.0f, 0.0f, TYEN_CAMPATH);
	}
	if (!e || !Entity_IsType(e, TYEN_CAMPATH)) {
		return NULL;
	}
	((cCamPathEntity *)e)->time = 0.0f;
	ns.pathOn = 1;
	ns.pathLocal = 1;
	return e;
}

void NetScene_ClientOwnPathDone(void) {
	ns.pathLocal = 0;
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
	NetScene_CamWaits();
	NetScene_PathFrame();
	if (!ns.cOn) {
		return;
	}
	cEntity *e = World_FindEntity(ns.cEnt);
	if (e && Entity_IsType(e, TYEN_OBJECT) && Obj_GetModel(e)) {
		ns.seen = 1;
		ns.goneNs = 0;
		cEntity *cam = NetScene_ViewCam();
		if (!cam) {
			/* a player that joined during the scene has no own entity nor camera yet (it gets them after the scene) */
			cam = Play_ViewCamera(e->pos.x, e->pos.y, e->pos.z);
		}
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
	memset(ns.camWait, 0, sizeof(ns.camWait));
	ns.ncamNum = 0;
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
		}
		if (eq && !strcmp(item, "CharLib")) {
			/* no save variable: only a character's library, as an outdoor level's start preloads the player's (Bishop
			 * for GDB's scene, the player stays its character) */
			SceneList_CharLib(eq + 1);
			Con_Printf("scene: test character library %s\n", eq + 1);
		} else if (eq) {
			SaveVar_Set(item, eq + 1);
			Con_Printf("scene: test save variable %s = %s\n", item, eq + 1);
			/* the character's library, as an outdoor level's start preloads the player's */
			if (!strcmp(item, "Char")) {
				SceneList_CharLib(eq + 1);
			}
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

#define NS_SHOT_EPS 0.001f
#define NS_SHOT_BACK 0.5		/* s of server time a shot of the client may go back */

/* the scene or camera path the shots are of now: its label, time and the time of the next shot (s) */
typedef struct {
	cEntity *e;						/* the scene entity, NULL for a path */
	cEntity *cam;					/* the camera path */
	char label[NS_NAME];
	float t, at;
	byte due;						/* a shot left for it */
} nsShot_t;

static cEntity *ns_lastPath;
static int ns_paths;

/* the subject of the shots now (count: a camera path seen anew counts as the next "Path<n>") */
static byte NetScene_ShotSubject(nsShot_t *s, byte count) {
	float len;
	if (!ns.shotsRead) {
		NetScene_ShotsRead();
	}
	if (!ns.nshots || !g_pWorld || g_Instance) {
		return 0;
	}
	memset(s, 0, sizeof(*s));
	/* a scene by its entity's action, else a camera path on view 0 by its time ("Path<n>": the n-th of the run) */
	cEntity *e = NetScene_Playing();
	cActCtrl *ctrl = e && Entity_IsType(e, TYEN_WITH_LIFE) ? WLife(e)->actCtrl : NULL;
	cEntity *cam = NetScene_ViewCam();
	int paths = ns_paths;
	if (ctrl && ctrl->layers[0].on) {
		s->e = e;
		Str_Copy(s->label, e->name + 9, sizeof(s->label));
		s->t = ctrl->layers[0].time;
		len = ctrl->layers[0].action ? ctrl->layers[0].action->anmTotalTime : 0.0f;
	} else if (cam && Entity_IsType(cam, TYEN_CAMPATH)) {
		cCamPathEntity *p = (cCamPathEntity *)cam;
		if ((cam != ns_lastPath || p->time < 0.05f) && (cam != ns_lastPath || ns.shotsDone)) {
			paths++;
		}
		if (count) {
			ns_lastPath = cam;
			ns_paths = paths;
		}
		s->cam = cam;
		Str_PrintfQuiet(s->label, sizeof(s->label), "Path%d", paths);
		s->t = p->time;
		len = NetScene_PathLength(p);
	} else {
		if (count) {
			ns_lastPath = NULL;
		}
		return 0;
	}
	byte fresh = strcmp(ns.shotScene, s->label) != 0;
	int done = fresh ? 0 : ns.shotsDone;
	if (count && fresh) {
		Str_Copy(ns.shotScene, s->label, sizeof(ns.shotScene));
		ns.shotsDone = 0;
	}
	s->due = done < ns.nshots;
	s->at = s->due ? ns.shotT[done] : 0.0f;
	if (s->at < 0.0f) {
		s->at = -s->at * len;
	}
	return 1;
}

void NetScene_ShotFrame(void) {
	nsShot_t s;
	char name[512];
	if (!NetScene_ShotSubject(&s, 1) || !s.due || s.t < s.at - NS_SHOT_EPS) {
		return;
	}
	Str_PrintfQuiet(name, sizeof(name), "%s/%s-%d", ns.shotDir, s.label, ns.shotsDone);
	Screenshot_Take(name);
	Con_Printf("scene: shot %s %d at %.3f\n", s.label, ns.shotsDone, (double)s.t);
	ns.shotsDone++;
	ns.shotNow = 1;
}

float NetScene_ShotStep(float dt) {
	nsShot_t s;
	/* the frame of the shot is drawn with no time gone, as the scene stands (its fades and bars too) */
	if (ns.shotNow) {
		ns.shotNow = 0;
		return dt < NS_SHOT_EPS ? dt : NS_SHOT_EPS;
	}
	/* a path and single player's scene run by the frame's time; the client's scene stands at the server's time
	 * (NetScene_ShotServerTime), its local presentation runs by this frame's time as well */
	if (!NetScene_ShotSubject(&s, 0) || !s.due) {
		return dt;
	}
	float left = s.at - s.t;
	return left > NS_SHOT_EPS && left < dt ? left : dt;
}

double NetScene_ShotServerTime(double prev, double rt) {
	nsShot_t s;
	if (!ns.cOn || !NetScene_ShotSubject(&s, 0) || !s.due || !s.e) {
		return rt;
	}
	/* not before the frame before: the models' animations do not go back (their events would be behind them) */
	return Watch_ActAt(s.e, 0, s.at, prev > rt - NS_SHOT_BACK ? prev : rt - NS_SHOT_BACK, rt);
}

/* ---- the test verb ---- */

/* the owner scan of the world (World.c) for player i when the test channel runs without its entity bound: the
 * scripts' "the player" (Mode_UsrHandle) of a call in its context */
static void NetScene_BindPlayer(int i) {
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e && !Players_Get(i); e = cEntity_Next(e)) {
		int slot = !e->active ? -1 : Entity_IsType(e, TYEN_CAR) ? (int)cVehicleEntity_GetOwnerSlot(e) :
			Entity_IsType(e, TYEN_WALKCHAR) ? (int)cWalkCharEntity_GetOwnerSlot(e) : -1;
		if (slot == PLAYERS_NET_SLOT + i) {
			Players_SetOwner(slot, e);
		}
	}
}

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
			for (int k = 0; k < NS_CAMWAITS; k++) {
				if (ns.camWait[k].ns) {
					TC_Addf(b, "camwait\t%s\t%s\n", ns.camWait[k].name, ns.camWait[k].value);
				}
			}
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
	if (!strcmp(word, "call") && n >= 2) {
		/* a function of a mission's module (imported from the level pack as SceneList_Import does) as a call table line
		 * "Module.Func|arg|...", in the context of player (the instance): the scenes of the missions that move the
		 * player's own camera (Mission_IdentifyMerc.InitIdentifyMerc) */
		char mod[64];
		const char *one[2] = { what, NULL };
		Str_PrintfQuiet(mod, sizeof(mod), "%.*s", (int)strcspn(what, ".|"), what);
		int was = Players_Current();
		if (g_Instance && player != PLAYERS_NONE && player != was) {
			NetScene_BindPlayer(player);
			Players_Enter(player);
		}
		byte ok = SceneList_Import(mod) && (strchr(what, '|') ? Mode_Calls(NULL, one) :
			Native_CallMod(mod, what + strlen(mod) + 1, NULL, ""));
		Native_ErrPrint();
		if (Players_Current() != was) {
			Players_Leave(was);
		}
		TC_Addf(b, "%s\n", ok ? "called" : "error: the call failed");
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
	TC_Add(b, "error: scene [state|list|play <name> [player]|call <Module.Func|arg...> [player]|skip [player]]\n", -1);
}
