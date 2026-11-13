/*
 * The watcher: a game that shows the world of a headless instance, see include/scrap/replicate.h. Not in the original.
 *
 * Connect, hello, welcome (the level, loaded when it is not the one running), then the snapshots: each record gives
 * a sample of its proxy at the server time tick / NETREP_TICK_HZ + dt. Every frame the proxies are placed at the
 * server time now minus REP_INTERP_MS, between the two samples around it (or ahead of the newest by its velocity for
 * up to WATCH_EXTRAP s). The server time is the local clock plus an offset that follows the ticks of the snapshots.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/misc.h"
#include "scrap/netrep.h"
#include "scrap/platform.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

#define WATCH_SAMPLES 8
#define WATCH_EXTRAP 0.25			/* s ahead of the newest sample at most */
#define WATCH_EXTRAP_TICKS 2			/* the sample must be from a snapshot at most this many ticks old */
#define WATCH_ACK_MS 50				/* an ack at least this often */
#define WATCH_STATUS_MS 10000
#define WATCH_CAM_NAME "WatchCam"
#define WATCH_NAME "netrep%d_%u"
#define WATCH_CONNECT_MS 15000		/* the first frames after a level load can be slow */
#define WATCH_RETRIES 5

enum { WATCH_OFF, WATCH_CONNECTING, WATCH_HELLO, WATCH_LEVEL, WATCH_RUN, WATCH_GONE };

typedef struct WatchSample {
	double t;						/* server time, s */
	uint32_t tick;					/* of its snapshot */
	NetRepState st;
} WatchSample;

typedef struct WatchProxy {
	cEntity *e;
	int kind;
	int n;
	WatchSample s[WATCH_SAMPLES];	/* by time */
} WatchProxy;

byte g_Watch;

static struct {
	char addr[64];
	uint32_t rate;
	int phase;
	Net2Host *host;
	NetRepView *view;
	char level[128];
	float start[3];					/* the view point of the welcome */
	double offset;					/* server time minus local time, s */
	byte haveOffset;
	uint64_t lastNs, lastAck, lastStatus;
	int retries;
	Net2Addr server;
	byte taken;						/* a message taken since the last ack */
	cEntity *cam;
	FILE *log;
	uint32_t msgs, notTaken, proxies;
	uint32_t made;					/* proxies made: their names stay unique while a deleted one waits */
	uint64_t bytes;
	WatchProxy px[NETREP_MAX_ENTS];
} watch;

byte Watch_ParseParam(const char *param, const char *ext) {
	if (!crt_stricmp("watch", param)) {
		Str_Copy(watch.addr, ext, sizeof(watch.addr));
		return 1;
	}
	if (!crt_stricmp("watchrate", param)) {
		watch.rate = (uint32_t)crt_atoi(ext);
		return 1;
	}
	return 0;
}

static double Watch_Now(void) {
	return (double)Plat_RealNs() / 1e9;
}

/* ---- the proxies ---- */

static void Watch_Free(int slot) {
	WatchProxy *p = &watch.px[slot];
	if (p->e) {
		cEntity_Del(p->e);
		watch.proxies--;
	}
	p->e = NULL;
	p->n = 0;
}

/* the model file or its compiled .sm3 (the loader takes either) */
static byte Watch_ModelExists(const char *path) {
	char alt[260];
	size_t n = strlen(path);

	if (File_Exists(path)) {
		return 1;
	}
	if (n < 4 || n >= sizeof(alt) || path[n - 4] != '.') {
		return 0;
	}
	memcpy(alt, path, n + 1);
	memcpy(alt + n - 3, "sm3", 3);
	return File_Exists(alt);
}

/* S|model|pilot|motor 0..3|class or C|class (Replicate.c) */
static cEntity *Watch_Build(int slot, int kind, const char *look, const NetRepState *st) {
	char name[32], buf[NETREP_LOOK_MAX + 1];
	char *part[8];
	int n = 0;

	Str_Copy(buf, look, sizeof(buf));
	for (char *p = buf; n < 8;) {
		part[n++] = p;
		p = strchr(p, '|');
		if (!p) {
			break;
		}
		*p++ = 0;
	}
	Str_PrintfQuiet(name, sizeof(name), WATCH_NAME, slot, ++watch.made);
	if (World_FindEntity(name)) {
		return NULL;
	}
	float x = NetRep_UPos(st->pos[0]), y = NetRep_UPos(st->pos[1]), z = NetRep_UPos(st->pos[2]);
	if (kind == NETREP_CHAR && n >= 2 && part[1][0]) {
		cEntity *e = cWorld_CreateEntity(g_pWorld, name, x, y, z, TYEN_WALKCHAR);
		if (e) {
			Obj(e)->proxy = 1;
			WithLife_SetActCtrl(e, part[1]);
			Con_Printf("watch: character %s (%s) at %.0f %.0f %.0f\n", name, part[1], (double)x, (double)y, (double)z);
		}
		return e;
	}
	if (kind != NETREP_SHIP || n < 8) {
		return NULL;
	}
	/* a model this game does not have would stop it in the loader */
	if (part[1][0] && !Watch_ModelExists(part[1])) {
		Con_Printf("watch: no model %s for slot %d\n", part[1], slot);
		return NULL;
	}
	cEntity *e = cWorld_CreateEntity(g_pWorld, name, x, y, z, TYEN_CAR);
	if (!e) {
		return NULL;
	}
	Obj(e)->proxy = 1;
	if (part[1][0]) {
		Entity_SetModel(e, part[1], 1.0f, 1.0f, 1.0f, 0);
	}
	if (part[7][0]) {
		WithLife_SetActCtrl(e, part[7]);
	}
	for (int i = 0; i < 4; i++) {
		char var[8];
		Str_PrintfQuiet(var, sizeof(var), "MEng%d", i);
		if (part[3 + i][0]) {
			Entity_VarS(e, var, &part[3 + i], 1);
		}
	}
	if (part[2][0]) {
		Entity_VarS(e, "MPil0", &part[2], 1);
	}
	return e;
}

static void Watch_Sample(WatchProxy *p, uint32_t tick, const NetRepState *st) {
	double t = (double)tick / NETREP_TICK_HZ + st->dt / 1000.0;
	int i = p->n;
	while (i > 0 && p->s[i - 1].t >= t) {
		if (p->s[i - 1].t == t) {
			p->s[i - 1].st = *st;
			p->s[i - 1].tick = tick;
			return;
		}
		i--;
	}
	if (p->n == WATCH_SAMPLES) {
		if (i == 0) {
			return;			/* older than all kept */
		}
		memmove(&p->s[0], &p->s[1], sizeof(p->s[0]) * (WATCH_SAMPLES - 1));
		p->n--;
		i--;
	}
	memmove(&p->s[i + 1], &p->s[i], sizeof(p->s[0]) * (size_t)(p->n - i));
	p->s[i].t = t;
	p->s[i].tick = tick;
	p->s[i].st = *st;
	p->n++;
}

static void Ev_Create(void *ctx, int slot, int kind, const char *look, uint32_t tick, const NetRepState *st) {
	(void)ctx;
	Watch_Free(slot);
	WatchProxy *p = &watch.px[slot];
	p->e = Watch_Build(slot, kind, look, st);
	p->kind = kind;
	if (p->e) {
		watch.proxies++;
		Watch_Sample(p, tick, st);
	}
}

static void Ev_Update(void *ctx, int slot, uint32_t tick, const NetRepState *st) {
	(void)ctx;
	if (watch.px[slot].e) {
		Watch_Sample(&watch.px[slot], tick, st);
	}
}

static void Ev_Remove(void *ctx, int slot) {
	(void)ctx;
	Watch_Free(slot);
}

static float Watch_LerpAng(uint16_t a, uint16_t b, float f) {
	return NetRep_UAng(a) + (float)(int16_t)(uint16_t)(b - a) / NETREP_ANG_SCALE * f;
}

/* the proxy at server time rt */
static void Watch_Place(int slot, WatchProxy *p, double rt, float dt) {
	cEntity *e = p->e;
	cWithLifeEntity *w = WLife(e);
	cObjEntity *o = &w->obj;
	vec3_t pos, ang, vel, rot = { 0.0f, 0.0f, 0.0f };
	float rates[NETREP_LAYERS], f = 0.0f, ext = 0.0f;
	uint16_t act[NETREP_LAYERS];

	int i = p->n - 1;
	while (i > 0 && p->s[i].t > rt) {
		i--;
	}
	const WatchSample *a = &p->s[i];
	const WatchSample *b = i + 1 < p->n ? &p->s[i + 1] : a;
	if (b != a) {
		f = (float)((rt - a->t) / (b->t - a->t));
		f = f < 0.0f ? 0.0f : f;
	} else if (rt > a->t && a->tick + WATCH_EXTRAP_TICKS >= NetRepCli_Tick(watch.view)) {
		/* ahead of the newest sample only while it is from the newest snapshots: an entity without a record since
		 * then has not changed (or waits for the budget) */
		ext = (float)(rt - a->t > WATCH_EXTRAP ? WATCH_EXTRAP : rt - a->t);
	}
	const NetRepState *sa = &a->st, *sb = &b->st;
	float *pv = &pos.x, *av = &ang.x, *vv = &vel.x, *rv = &rot.x;
	for (int k = 0; k < 3; k++) {
		float pa = NetRep_UPos(sa->pos[k]);
		vv[k] = (float)sa->vel[k] + (float)(sb->vel[k] - sa->vel[k]) * f;
		pv[k] = pa + (NetRep_UPos(sb->pos[k]) - pa) * f + vv[k] * ext;
		av[k] = Watch_LerpAng(sa->ang[k], sb->ang[k], f);
		if (b != a) {
			rv[k] = (float)(int16_t)(uint16_t)(sb->ang[k] - sa->ang[k]) / NETREP_ANG_SCALE / (float)(b->t - a->t);
		}
	}
	Entity_SetPos(e, &pos);
	o->slotPos = pos;
	o->renderPos = pos;
	o->ang = ang;
	o->slotAng = ang;
	o->renderAng = ang;
	o->vel = vel;
	o->rotVel = rot;
	w->life = sa->life;
	byte on = (sa->flags & NETREP_FL_ACTIVE) != 0;
	if (on != e->active) {
		Entity_SetActive(e, on);
	}
	if (p->kind == NETREP_SHIP) {
		cVehicleEntity *v = (cVehicleEntity *)e;
		/* the proxy knows no maximum: the highest life seen */
		if (v->maxLife < w->life) {
			v->maxLife = w->life;
		}
		v->controls.acc = sa->ctrl[0] / 127.0f;
		v->controls.str = sa->ctrl[1] / 127.0f;
		v->speed = sqrtf(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
	} else {
		cWalkCharEntity *c = WChar(e);
		c->view.x = Watch_LerpAng(sa->view[0], sb->view[0], f);
		c->view.y = Watch_LerpAng(sa->view[1], sb->view[1], f);
		c->view.z = 0.0f;
		c->slotView = c->view;
		c->renderView = c->view;
		c->viewVel = (vec3_t){ 0.0f, 0.0f, 0.0f };
		c->falling = (sa->flags & NETREP_FL_FALLING) != 0;
		c->walking = (sa->flags & NETREP_FL_WALKING) != 0;
		/* the action of the sample before; its rate from the time it went on between the two samples */
		for (int l = 0; l < NETREP_LAYERS; l++) {
			act[l] = sa->act[l];
			float ta = sa->actTime[l] / 1000.0f, tb = sb->actTime[l] / 1000.0f;
			rates[l] = b != a && sb->act[l] == sa->act[l] && tb > ta ? (tb - ta) / (float)(b->t - a->t) : 0.0f;
			rates[l] = rates[l] > 4.0f ? 4.0f : rates[l];
		}
		if (w->actCtrl && w->actCtrl->cls) {
			cActCtrl_NetDrive(w->actCtrl, act, rates, NETREP_LAYERS, dt);
		}
	}
	if (watch.log && rt >= p->s[0].t) {
		fprintf(watch.log, "%.4f,%d,%.2f,%.2f,%.2f\n", rt, slot, (double)pos.x, (double)pos.y, (double)pos.z);
	}
}

/* characters and ships the level scripts made: the instance has the real ones */
static void Watch_Purge(void) {
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (e->deadNode.target || (!Entity_IsType(e, TYEN_CAR) && !Entity_IsType(e, TYEN_WALKCHAR))) {
			continue;
		}
		if (!Obj(e)->proxy) {
			cEntity_Del(e);
		}
	}
}

/* ---- the camera ---- */

static void Watch_SetView(const float *pos, float yaw, float pitch) {
	CGameCamera *c = &((cCamControl *)watch.cam)->cam;
	c->pos = (vec3_t){ pos[0], pos[1], pos[2] };
	c->rot = (vec3_t){ pitch * M_PI_F / 180.0f, yaw * M_PI_F / 180.0f, 0.0f };
}

static void Watch_MakeCam(void) {
	int one = 1;
	float p[3] = { watch.start[0], watch.start[1] + 800.0f, watch.start[2] };

	watch.cam = World_FindEntity(WATCH_CAM_NAME);
	if (!watch.cam) {
		watch.cam = cWorld_CreateEntity(g_pWorld, WATCH_CAM_NAME, p[0], p[1], p[2], TYEN_FCAM);
	}
	if (!watch.cam) {
		return;
	}
	cWorld_SetCam(g_pWorld, 0, WATCH_CAM_NAME);
	Entity_VarI(watch.cam, "FreeCam", &one, 1);
	Watch_SetView(p, 0.0f, -10.0f);
}

/* ---- the connection ---- */

static void Watch_Start(void) {
	Net2Config cfg;
	Net2Addr *addr = &watch.server;
	int err = 0;

	watch.phase = WATCH_GONE;
	if (!Net2_ParseAddr(watch.addr, Instance_Port(), addr)) {
		Con_Errorf("watch: '%s' is not a numeric address\n", watch.addr);
		return;
	}
	Net2_DefaultConfig(&cfg, 0);
	cfg.appVersion = INSTANCE_APP_VERSION;
	cfg.ipv4 = addr->family == NET2_AF_IPV4;
	cfg.ipv6 = addr->family == NET2_AF_IPV6;
	cfg.connectTimeout = WATCH_CONNECT_MS;
	watch.host = Net2_Create(&cfg, &err);
	watch.view = NetRepCli_Create();
	if (!watch.host || !watch.view || Net2_Connect(watch.host, addr, Plat_RealNs() / 1000000) != NET2_OK) {
		Con_Errorf("watch: no Net2 client (error %d)\n", err);
		return;
	}
	const char *path = getenv("SCRAP_REPLOG");
	if (path && *path) {
		watch.log = fopen(path, "w");
		if (watch.log) {
			fputs("time,slot,x,y,z\n", watch.log);
		}
	}
	watch.phase = WATCH_CONNECTING;
	Con_Printf("watch: connecting to %s\n", watch.addr);
}

static void Watch_Welcome(const uint8_t *data, int size) {
	uint32_t tick;

	if (!NetRep_ReadWelcome(data, size, &tick, watch.start, watch.level, sizeof(watch.level))) {
		Con_Errorf("watch: bad welcome\n");
		return;
	}
	watch.offset = (double)tick / NETREP_TICK_HZ - Watch_Now();
	watch.haveOffset = 1;
	watch.phase = WATCH_LEVEL;
	Con_Printf("watch: level %s, tick %u\n", watch.level, tick);
	if (!g_pApp->levelPath || crt_stricmp(g_pApp->levelPath, watch.level)) {
		CWinApp_LoadLevel(g_pApp, watch.level);
	}
}

static void Watch_Snapshot(const uint8_t *data, int size) {
	NetRepEvents ev = { NULL, Ev_Create, Ev_Update, Ev_Remove };

	watch.msgs++;
	watch.bytes += (uint64_t)size;
	if (!NetRepCli_Message(watch.view, data, size, &ev)) {
		watch.notTaken++;
		return;
	}
	watch.taken = 1;
	/* the newest tick against the local clock: jumps when far off, else follows slowly */
	double target = (double)NetRepCli_Tick(watch.view) / NETREP_TICK_HZ - Watch_Now();
	double d = target - watch.offset;
	if (!watch.haveOffset || d > 0.3 || d < -0.3) {
		watch.offset = target;
		watch.haveOffset = 1;
	} else {
		watch.offset += d * 0.05;
	}
}

void Watch_PreFrame(void) {
	Net2Event ev;
	uint64_t ns = Plat_RealNs();

	if (watch.phase == WATCH_OFF) {
		Watch_Start();
	}
	if (!watch.host) {
		return;
	}
	Net2_Poll(watch.host, ns / 1000000);
	while (Net2_Receive(watch.host, &ev)) {
		switch (ev.type) {
		case NET2_EV_CONNECT: {
			uint8_t buf[16];
			int n = NetRep_WriteHello(buf, sizeof(buf), watch.rate ? watch.rate : NETREP_RATE_DEFAULT);
			Net2_Send(watch.host, 0, REP_CH_RELIABLE, buf, n);
			watch.phase = WATCH_HELLO;
			Con_Printf("watch: connected\n");
			break;
		}
		case NET2_EV_DISCONNECT:
			Con_Printf("watch: disconnected (%s)\n", Net2_ReasonString(ev.reason));
			if (watch.phase == WATCH_CONNECTING && watch.retries++ < WATCH_RETRIES &&
				Net2_Connect(watch.host, &watch.server, ns / 1000000) == NET2_OK) {
				Con_Printf("watch: connecting again\n");
				break;
			}
			watch.phase = WATCH_GONE;
			break;
		case NET2_EV_MESSAGE:
			if (ev.size < 1) {
				break;
			}
			if (ev.channel == REP_CH_RELIABLE && ev.data[0] == NETREP_MSG_WELCOME) {
				Watch_Welcome(ev.data, ev.size);
			} else if (ev.channel == REP_CH_UNRELIABLE && ev.data[0] == NETREP_MSG_SNAP && watch.phase == WATCH_RUN) {
				Watch_Snapshot(ev.data, ev.size);
			}
			break;
		}
	}
	if (watch.phase == WATCH_LEVEL && !g_pApp->loadPending && g_pApp->levelPath &&
		!crt_stricmp(g_pApp->levelPath, watch.level)) {
		watch.phase = WATCH_RUN;
		Watch_MakeCam();
		Con_Printf("watch: level running\n");
	}
	if (watch.phase != WATCH_RUN) {
		watch.lastNs = ns;
		return;
	}
	Watch_Purge();
	float dt = watch.lastNs ? (float)((double)(ns - watch.lastNs) / 1e9) : 0.0f;
	watch.lastNs = ns;
	double rt = (double)ns / 1e9 + watch.offset - REP_INTERP_MS / 1000.0;
	for (int s = 0; s < NETREP_MAX_ENTS; s++) {
		if (watch.px[s].e && watch.px[s].n) {
			Watch_Place(s, &watch.px[s], rt, dt);
		}
	}
	if (ns / 1000000 - watch.lastStatus >= WATCH_STATUS_MS) {
		const vec3_t *c = watch.cam ? &((cCamControl *)watch.cam)->cam.pos : NULL;
		Con_Printf("watch: %u proxies, %u messages %llu bytes, %u not taken, server time %.2f, view %.0f %.0f %.0f\n",
			watch.proxies, watch.msgs, (unsigned long long)watch.bytes, watch.notTaken, rt, c ? (double)c->x : 0.0,
			c ? (double)c->y : 0.0, c ? (double)c->z : 0.0);
		watch.lastStatus = ns / 1000000;
	}
}

void Watch_PostFrame(void) {
	uint64_t ms = Plat_RealNs() / 1000000;
	uint16_t newest;
	uint32_t bits;
	uint8_t buf[32];

	if (!watch.host) {
		return;
	}
	if (watch.phase == WATCH_RUN && (watch.taken || ms - watch.lastAck >= WATCH_ACK_MS) &&
		NetRepCli_Ack(watch.view, &newest, &bits)) {
		float v[3] = { 0.0f, 0.0f, 0.0f };
		if (watch.cam) {
			const vec3_t *p = &((cCamControl *)watch.cam)->cam.pos;
			v[0] = p->x;
			v[1] = p->y;
			v[2] = p->z;
		}
		int n = NetRep_WriteAck(buf, sizeof(buf), newest, bits, v);
		Net2_Send(watch.host, 0, REP_CH_UNRELIABLE, buf, n);
		watch.taken = 0;
		watch.lastAck = ms;
	}
	Net2_Flush(watch.host, ms);
}

void Watch_Close(void) {
	if (watch.host) {
		Net2_Destroy(watch.host);
		watch.host = NULL;
	}
	NetRepCli_Destroy(watch.view);
	watch.view = NULL;
	if (watch.log) {
		fclose(watch.log);
		watch.log = NULL;
	}
}
