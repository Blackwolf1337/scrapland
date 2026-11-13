/*
 * The replication of the instance's world to its watchers, see include/scrap/replicate.h and include/scrap/netrep.h.
 * Not in the original.
 *
 * Every snapshot walks the world's entities: the ships and characters that are not deleted get the slot of the last
 * snapshot back (found by pointer and name, with the same look), else a free one. The sample time of an object is
 * the end of its current slot (cObjEntity.slot): its render position reaches pos there (cObjEntity_Update lerps from
 * slotPos to pos over the slot), so a watcher that draws the samples at their times draws what this world draws.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/model.h"
#include "scrap/node3d.h"
#include "scrap/netrep.h"
#include "scrap/platform.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

#define REP_CONNS 1024				/* NET2_MAX_CONNS */
#define REP_HASH 4096				/* pointer table, twice the slots */
#define REP_NAME 64

typedef struct RepEnt {
	cEntity *e;						/* NULL: the slot has no entity of this world */
	char name[REP_NAME];
	int look;
} RepEnt;

typedef struct RepConn {
	NetRepClient *c;
	NetRepClientStats last;			/* at the last status line */
} RepConn;

static struct {
	NetRepServer *srv;
	RepEnt ent[NETREP_MAX_ENTS];
	int16_t hash[REP_HASH];			/* slot by pointer, -1 empty */
	RepConn conn[REP_CONNS];
	int watchers;
	uint32_t snapTick;				/* snapshots so far: tick * SNAP_HZ / TICK_HZ */
	uint64_t gatherNs, buildNs, gatherMax, buildMax;
	uint32_t snaps;
	FILE *log;
	byte logTried;
	uint8_t msgs[NETREP_MAX_MSGS][NETREP_MSG_MAX];
} rep;

/* ---- the entities ---- */

static uint32_t Rep_PtrHash(const void *p) {
	uintptr_t v = (uintptr_t)p;
	v ^= v >> 17;
	v *= (uintptr_t)0x9e3779b97f4a7c15ull;
	return (uint32_t)(v >> 7) & (REP_HASH - 1);
}

static void Rep_HashBuild(void) {
	for (int i = 0; i < REP_HASH; i++) {
		rep.hash[i] = -1;
	}
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		if (rep.ent[s].e) {
			uint32_t h = Rep_PtrHash(rep.ent[s].e);
			while (rep.hash[h] >= 0) {
				h = (h + 1) & (REP_HASH - 1);
			}
			rep.hash[h] = (int16_t)s;
		}
	}
}

/* the slot of the entity of the last snapshot (same pointer and name), -1 */
static int Rep_Find(const cEntity *e) {
	if (!rep.srv) {
		return -1;
	}
	for (uint32_t h = Rep_PtrHash(e);; h = (h + 1) & (REP_HASH - 1)) {
		int s = rep.hash[h];
		if (s < 0) {
			return -1;
		}
		if (rep.ent[s].e == e && !strncmp(rep.ent[s].name, e->name, REP_NAME - 1)) {
			return s;
		}
	}
}

static int Rep_Kind(cEntity *e) {
	if (e->deadNode.target) {
		return 0;
	}
	if (Entity_IsType(e, TYEN_CAR)) {
		return NETREP_SHIP;
	}
	return Entity_IsType(e, TYEN_WALKCHAR) ? NETREP_CHAR : 0;
}

static const char *Rep_VarS(cEntity *e, const char *name) {
	char *s = NULL;
	return Entity_VarS(e, name, &s, 0) && s ? s : "";
}

/* how the watcher builds the proxy: S|model|pilot|motor 0..3|class or C|class */
static void Rep_Look(cEntity *e, int kind, char *out, int size) {
	cWithLifeEntity *w = WLife(e);
	const char *cls = w->actCtrl && w->actCtrl->cls ? w->actCtrl->cls->name : "";

	if (kind == NETREP_CHAR) {
		Str_PrintfQuiet(out, size, "C|%s", cls);
		return;
	}
	/* the path the model was loaded from (CGameModel_Name is only its file name) */
	CGameModel *m = &w->obj.model;
	const char *path = CGameModel_IsLoaded(m) && m->Model->scene ? m->Model->scene->name : "";
	Str_PrintfQuiet(out, size, "S|%s|%s|%s|%s|%s|%s|%s", path ? path : "",
		Rep_VarS(e, "MPil0"), Rep_VarS(e, "MEng0"), Rep_VarS(e, "MEng1"), Rep_VarS(e, "MEng2"), Rep_VarS(e, "MEng3"),
		cls);
}

/* the index + 1 of the action in its class, 0 none */
static uint16_t Rep_ActIndex(const cActClass *cls, const cAction *a) {
	uint16_t i = 1;
	for (const cAction *x = cls->first; x; x = x->next, i++) {
		if (x == a) {
			return i;
		}
	}
	return 0;
}

static float Rep_SampleDt(cEntity *e) {
	cSlotController *sc = Obj(e)->slot;
	float t = sc ? sc->time : 0.0f;
	return t < 0.0f ? 0.0f : t;
}

static void Rep_State(cEntity *e, int kind, NetRepState *s) {
	cWithLifeEntity *w = WLife(e);
	cObjEntity *o = &w->obj;

	memset(s, 0, sizeof(*s));
	s->pos[0] = NetRep_QPos(e->pos.x);
	s->pos[1] = NetRep_QPos(e->pos.y);
	s->pos[2] = NetRep_QPos(e->pos.z);
	s->vel[0] = NetRep_QVel(o->vel.x);
	s->vel[1] = NetRep_QVel(o->vel.y);
	s->vel[2] = NetRep_QVel(o->vel.z);
	s->ang[0] = NetRep_QAng(o->ang.x);
	s->ang[1] = NetRep_QAng(o->ang.y);
	s->ang[2] = NetRep_QAng(o->ang.z);
	float dt = Rep_SampleDt(e) * 1000.0f;
	s->dt = (uint16_t)(dt > 65535.0f ? 65535.0f : dt + 0.5f);
	s->life = (int16_t)(w->life > 32767 ? 32767 : w->life < -32768 ? -32768 : w->life);
	s->flags = e->active ? NETREP_FL_ACTIVE : 0;
	if (kind == NETREP_SHIP) {
		cVehicleEntity *v = (cVehicleEntity *)e;
		s->ctrl[0] = NetRep_QCtrl(v->controls.acc);
		s->ctrl[1] = NetRep_QCtrl(v->controls.str);
		return;
	}
	cWalkCharEntity *c = WChar(e);
	s->flags |= (c->falling ? NETREP_FL_FALLING : 0) | (c->walking ? NETREP_FL_WALKING : 0);
	s->view[0] = NetRep_QAng(c->view.x);
	s->view[1] = NetRep_QAng(c->view.y);
	cActCtrl *ctrl = w->actCtrl;
	if (ctrl && ctrl->cls) {
		for (int l = 0; l < NETREP_LAYERS; l++) {
			const cActLayer *ly = &ctrl->layers[l];
			if (ly->on && ly->action) {
				s->act[l] = Rep_ActIndex(ctrl->cls, ly->action);
				float t = ly->time * 1000.0f;
				s->actTime[l] = t > 0.0f ? (uint32_t)(t + 0.5f) : 0;
			}
		}
	}
}

/* the snapshot: the states of the entities into the server, slots for new ones */
static void Rep_Gather(uint64_t tick) {
	char look[NETREP_LOOK_MAX + 1];
	static byte seen[NETREP_MAX_ENTS];
	NetRepState st;

	NetRepSrv_Begin(rep.srv, (uint32_t)tick);
	memset(seen, 0, sizeof(seen));
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		int kind = Rep_Kind(e);
		if (!kind) {
			continue;
		}
		Rep_Look(e, kind, look, sizeof(look));
		int known = NetRepSrv_NumLooks(rep.srv);
		int id = NetRepSrv_Look(rep.srv, look);
		if (id < 0) {
			continue;
		}
		if (id >= known) {
			Con_Printf("replication: look %d %s\n", id, look);
		}
		int s = Rep_Find(e);
		if (s >= 0 && (seen[s] || rep.ent[s].look != id)) {
			s = -1;			/* rebuilt with another look: a new entity for the watchers */
		}
		if (s < 0) {
			s = NetRepSrv_FreeSlot(rep.srv);
			if (s < 0) {
				continue;
			}
			rep.ent[s].e = e;
			Str_Copy(rep.ent[s].name, e->name, REP_NAME);
			rep.ent[s].look = id;
		}
		seen[s] = 1;
		Rep_State(e, kind, &st);
		NetRepSrv_Set(rep.srv, s, kind, id, &st);
	}
	NetRepSrv_End(rep.srv);
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		if (!seen[s]) {
			rep.ent[s].e = NULL;
		}
	}
	Rep_HashBuild();
}

/* SCRAP_REPLOG: the sample of each replicated entity of the world now */
static void Rep_Log(uint64_t tick) {
	if (!rep.logTried) {
		rep.logTried = 1;
		const char *path = getenv("SCRAP_REPLOG");
		if (path && *path) {
			rep.log = fopen(path, "w");
		}
		if (rep.log) {
			fputs("time,slot,x,y,z\n", rep.log);
		}
	}
	if (!rep.log) {
		return;
	}
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		int s = Rep_Kind(e) ? Rep_Find(e) : -1;
		if (s >= 0) {
			double t = (double)tick / INSTANCE_TICK_HZ + Rep_SampleDt(e);
			fprintf(rep.log, "%.4f,%d,%.2f,%.2f,%.2f\n", t, s, (double)e->pos.x, (double)e->pos.y, (double)e->pos.z);
		}
	}
}

/* ---- the watchers ---- */

void Rep_Connect(Net2Host *host, int conn) {
	(void)host;
	(void)conn;
}

void Rep_Disconnect(int conn) {
	if (conn < 0 || conn >= REP_CONNS || !rep.conn[conn].c) {
		return;
	}
	NetRepSrv_RemoveClient(rep.srv, rep.conn[conn].c);
	rep.conn[conn].c = NULL;
	rep.watchers--;
}

byte Rep_IsWatcher(int conn) {
	return conn >= 0 && conn < REP_CONNS && rep.conn[conn].c;
}

static void Rep_Hello(Net2Host *host, int conn, uint32_t rate) {
	uint8_t buf[300];
	float view[3] = { 0.0f, 0.0f, 0.0f };

	if (!rep.srv) {
		rep.srv = NetRepSrv_Create();
		Rep_HashBuild();
	}
	if (!rep.srv) {
		return;
	}
	if (!rep.conn[conn].c) {
		rep.conn[conn].c = NetRepSrv_AddClient(rep.srv, rate ? rate : NETREP_RATE_DEFAULT);
		if (!rep.conn[conn].c) {
			return;
		}
		memset(&rep.conn[conn].last, 0, sizeof(rep.conn[conn].last));
		rep.watchers++;
	}
	cEntity *p = cWorld_GetUsrEntity(g_pWorld, 0);
	if (p) {
		view[0] = p->pos.x;
		view[1] = p->pos.y;
		view[2] = p->pos.z;
	}
	NetRepSrv_SetView(rep.conn[conn].c, view[0], view[1], view[2], 0.0f);
	int n = NetRep_WriteWelcome(buf, sizeof(buf), (uint32_t)(Instance_Time() * INSTANCE_TICK_HZ + 0.5), view,
		g_pApp->levelPath ? g_pApp->levelPath : "");
	if (n > 0) {
		Net2_Send(host, conn, REP_CH_RELIABLE, buf, n);
	}
	Con_Printf("instance: watcher %d, rate %u bytes/s\n", conn, rate);
}

byte Rep_Message(Net2Host *host, int conn, int channel, const uint8_t *data, int size) {
	uint32_t rate, bits;
	uint16_t newest;
	float view[3];

	if (conn < 0 || conn >= REP_CONNS || size < 1) {
		return 0;
	}
	if (channel == REP_CH_RELIABLE && data[0] == NETREP_MSG_HELLO) {
		if (NetRep_ReadHello(data, size, &rate)) {
			Rep_Hello(host, conn, rate);
		} else {
			Net2_Disconnect(host, conn, NET2_DR_VERSION);
		}
		return 1;
	}
	if (!rep.conn[conn].c) {
		return 0;
	}
	if (data[0] == NETREP_MSG_ACK && NetRep_ReadAck(data, size, &newest, &bits, view)) {
		NetRepSrv_Ack(rep.srv, rep.conn[conn].c, newest, bits);
		NetRepSrv_SetView(rep.conn[conn].c, view[0], view[1], view[2], 0.0f);
	}
	return 1;
}

void Rep_Tick(Net2Host *host, uint64_t tick) {
	int sizes[NETREP_MAX_MSGS];

	Rep_Log(tick);
	uint32_t due = (uint32_t)(tick * NETREP_SNAP_HZ / NETREP_TICK_HZ);
	if (!rep.watchers || due == rep.snapTick) {
		rep.snapTick = due;
		return;
	}
	rep.snapTick = due;
	uint64_t t0 = Plat_RealNs();
	Rep_Gather(tick);
	uint64_t t1 = Plat_RealNs();
	for (int i = 0; i < REP_CONNS; i++) {
		if (!rep.conn[i].c) {
			continue;
		}
		int n = NetRepSrv_Build(rep.srv, rep.conn[i].c, rep.msgs, sizes, NETREP_MAX_MSGS);
		for (int m = 0; m < n; m++) {
			Net2_Send(host, i, REP_CH_UNRELIABLE, rep.msgs[m], sizes[m]);
		}
	}
	uint64_t t2 = Plat_RealNs();
	rep.gatherNs += t1 - t0;
	rep.buildNs += t2 - t1;
	rep.gatherMax = t1 - t0 > rep.gatherMax ? t1 - t0 : rep.gatherMax;
	rep.buildMax = t2 - t1 > rep.buildMax ? t2 - t1 : rep.buildMax;
	rep.snaps++;
}

void Rep_Status(double secs) {
	if (!rep.srv || !rep.snaps) {
		return;
	}
	Con_Printf("replication: %d watchers, %d entities, %u snapshots, gather ms avg %.3f max %.3f, build ms avg %.3f max "
		"%.3f\n", rep.watchers, NetRepSrv_Live(rep.srv), rep.snaps, (double)rep.gatherNs / rep.snaps / 1e6,
		(double)rep.gatherMax / 1e6, (double)rep.buildNs / rep.snaps / 1e6, (double)rep.buildMax / 1e6);
	rep.gatherNs = rep.buildNs = rep.gatherMax = rep.buildMax = 0;
	rep.snaps = 0;
	for (int i = 0; i < REP_CONNS; i++) {
		if (!rep.conn[i].c) {
			continue;
		}
		const NetRepClientStats *st = NetRepSrv_Stats(rep.conn[i].c);
		NetRepClientStats *l = &rep.conn[i].last;
		uint32_t snaps = st->snaps - l->snaps;
		uint32_t msgs = st->msgs - l->msgs;
		double bytes = (double)(st->bytes - l->bytes);
		Con_Printf("  watcher %d: %.0f bytes/s, %u snapshots, %.1f bytes and %.1f records a snapshot (max %u bytes, %u "
			"records), %u messages, %u creates %u removes %u fulls, %u waited, %u acked, interest %u\n", i,
			secs > 0.0 ? bytes / secs : 0.0, snaps, snaps ? bytes / snaps : 0.0,
			snaps ? (double)(st->records - l->records) / snaps : 0.0, st->maxSnapBytes, st->maxRecords, msgs,
			st->creates - l->creates, st->removes - l->removes, st->fulls - l->fulls, st->skipped - l->skipped,
			st->acked - l->acked, st->interest);
		*l = *st;
	}
}

void Rep_Close(void) {
	NetRepSrv_Destroy(rep.srv);
	rep.srv = NULL;
	if (rep.log) {
		fclose(rep.log);
		rep.log = NULL;
	}
}
