/*
 * The viewers of the world: single player's camera of view 0, the instance's players' cameras and the test viewers of
 * the load probe, the frustum test of a headless process and the leaders of the quotas. See include/scrap/viewers.h.
 * Not in the original.
 */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/ai.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/game.h"
#include "scrap/nodesworld.h"
#include "scrap/nodesworld2d.h"
#include "scrap/objentity.h"
#include "scrap/players.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapworld.h"
#include "scrap/viewers.h"
#include "scrap/world.h"

#define VIEWERS_FAKE_TURN 0.26f		/* rad/s a test viewer turns (15 degrees a second) */
#define VIEWERS_FAKE_EYE 300.0f		/* a test viewer's eyes above its node */

static cViewer g_Local;				/* single player's camera, made when asked */
static cViewer g_Set[VIEWERS_MAX];	/* the instance's, made once a frame */
static int g_Count;
static int g_Leaders;
static float g_Share = VIEWERS_SHARE;	/* the test line "viewers share <units>" changes it */

static struct {
	int n;
	vec3_t pos[VIEWERS_FAKE_MAX];
	float yaw[VIEWERS_FAKE_MAX];
} g_Fake;

byte Viewers_Server(void) {
	return Players_Net();
}

/* the camera's z axis turned by its angles */
static void Viewers_CamDir(cEntity *cam, vec3_t *dir) {
	static const vec3_t z = { 0.0f, 0.0f, 1.0f };
	Vec3_Rotate(dir, &z, &((cCamControl *)cam)->cam.rot);
}

static const cViewer *Viewers_LocalView(void) {
	cWorld *w = g_pWorld;
	cViewer *v = &g_Local;

	v->pos = w->camPos[w->curCam];
	v->cam = World_FindEntity(cWorld_GetCamName(w, 0));
	v->id = VIEWERS_LOCAL_ID;
	v->leader = 0;
	v->posed = v->cam != NULL;
	if (!v->cam) {
		v->dir = (vec3_t){ 0.0f, 0.0f, 1.0f };
		v->fov = VIEWERS_NET_FOV;
		v->far = VIEWERS_NET_FAR;
		v->ent = NULL;
		return v;
	}
	Viewers_CamDir(v->cam, &v->dir);
	v->fov = ((cCamControl *)v->cam)->cam.fov;
	v->far = ((cCamControl *)v->cam)->cam.zFar;
	v->ent = Entity_IsType(v->cam, FCAM_CLASSNAME) ? cCamEntity_GetTarget(v->cam) : NULL;
	return v;
}

int Viewers_Count(void) {
	return Viewers_Server() ? g_Count : 1;
}

const cViewer *Viewers_At(int i) {
	if (!Viewers_Server()) {
		return i == 0 ? Viewers_LocalView() : NULL;
	}
	return i >= 0 && i < g_Count ? &g_Set[i] : NULL;
}

static float Viewers_DistSq(const vec3_t *a, const vec3_t *b) {
	float dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;
	return dx * dx + dy * dy + dz * dz;
}

const cViewer *Viewers_Near(const vec3_t *p) {
	if (!Viewers_Server()) {
		return Viewers_LocalView();
	}
	const cViewer *best = NULL;
	float bestD = FLT_MAX;
	for (int i = 0; i < g_Count; i++) {
		float d = Viewers_DistSq(&g_Set[i].pos, p);
		if (d < bestD) {
			bestD = d;
			best = &g_Set[i];
		}
	}
	return best;
}

int Viewers_Find(int id) {
	for (int i = 0; i < g_Count; i++) {
		if (g_Set[i].id == id) {
			return i;
		}
	}
	return -1;
}

void Viewers_CamPos(const cViewer *v, vec3_t *out) {
	if (!Viewers_Server() && v->cam) {
		CamCtl_GetCamPos(v->cam, &out->x, &out->y, &out->z);
		return;
	}
	*out = v->pos;
}

int Viewers_CamSector(const cViewer *v, const vec3_t *camPos) {
	(void)v;
	return App_Headless() ? Sector_At(camPos) : Video_GetStatus();
}

float Viewers_MinDistSq(const vec3_t *p) {
	if (!Viewers_Server()) {
		return cWorld_ViewsMinDistSq(g_pWorld, p);
	}
	float best = FLT_MAX;
	for (int i = 0; i < g_Count; i++) {
		float d = Viewers_DistSq(&g_Set[i].pos, p);
		best = d < best ? d : best;
	}
	return best;
}

byte Viewers_ViewerSees(const cViewer *v, const vec3_t *p, float radius) {
	vec3_t d = { p->x - v->pos.x, p->y - v->pos.y, p->z - v->pos.z };
	float d2 = d.x * d.x + d.y * d.y + d.z * d.z;

	if (d2 <= radius * radius) {
		return 1;
	}
	float dist = sqrtf(d2);
	if (dist - radius > v->far) {
		return 0;
	}
	float half = v->fov * 0.5f * VIEWERS_CONE + asinf(radius < dist ? radius / dist : 1.0f);
	if (half < M_PI_F && (d.x * v->dir.x + d.y * v->dir.y + d.z * v->dir.z) / dist < cosf(half)) {
		return 0;
	}
	/* the indoor maps: from the point's sector through the portals to the viewer's (cAI_IsVisible) */
	int s = Sector_At(p);
	if (s >= 0) {
		int at = Sector_At(&v->pos);
		if (at >= 0 && at != s && Collision_Sector(s, p, &v->pos, 1) != at) {
			return 0;
		}
	}
	return 1;
}

byte Viewers_SeesSphere(const vec3_t *p, float radius) {
	int n = Viewers_Count();
	for (int i = 0; i < n; i++) {
		if (Viewers_ViewerSees(Viewers_At(i), p, radius)) {
			return 1;
		}
	}
	return 0;
}

byte Viewers_SeesModel(const CGameModel *m) {
	vec3_t p;

	if (!App_Headless()) {
		return CGameModel_IsVisibleAt(m, "Viewers_SeesModel");
	}
	return Viewers_SeesSphere(CGameModel_GetPos(m, &p), CGameModel_Radius(m));
}

byte Viewers_SeesSector(int sector) {
	if (!App_Headless()) {
		return Sector_Visible(sector);
	}
	int n = Viewers_Count();
	for (int i = 0; i < n; i++) {
		if (Sector_At(&Viewers_At(i)->pos) == sector) {
			return 1;
		}
	}
	return 0;
}

/* ---- the instance's set ---- */

/* the viewer of player i: its camera, else its entity's eyes */
static byte Viewers_Player(int i, cViewer *v) {
	cPlayer *pl = Players_Ctx(i);
	cEntity *e = pl ? pl->ent : NULL;

	if (!e) {
		return 0;
	}
	cEntity *cam = pl->camName[0] ? World_FindEntity(pl->camName) : NULL;
	v->id = i;
	v->posed = 1;
	if (cam && Entity_IsType(cam, TYEN_CAM)) {
		cCamControl *c = (cCamControl *)cam;
		v->cam = cam;
		v->pos = c->cam.pos;
		Viewers_CamDir(cam, &v->dir);
		v->fov = c->cam.fov;
		v->far = c->cam.zFar;
		cEntity *t = Entity_IsType(cam, FCAM_CLASSNAME) ? cCamEntity_GetTarget(cam) : NULL;
		v->ent = t ? t : e;
		return 1;
	}
	vec3_t tmp;
	const vec3_t *p = Entity_GetPos(e, &tmp);
	v->cam = NULL;
	v->ent = e;
	v->pos = (vec3_t){ p->x, p->y + PLAYERS_EYE, p->z };
	Players_Forward(e, &v->dir);
	v->fov = VIEWERS_NET_FOV;
	v->far = VIEWERS_NET_FAR;
	return 1;
}

void Viewers_Frame(float dt) {
	if (!Viewers_Server()) {
		return;
	}
	g_Count = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (Viewers_Player(i, &g_Set[g_Count])) {
			g_Count++;
		}
	}
	for (int k = 0; k < g_Fake.n && g_Count < VIEWERS_MAX; k++) {
		cViewer *v = &g_Set[g_Count++];
		g_Fake.yaw[k] = remainderf(g_Fake.yaw[k] + VIEWERS_FAKE_TURN * dt, 2.0f * M_PI_F);
		v->pos = g_Fake.pos[k];
		v->dir = (vec3_t){ -sinf(g_Fake.yaw[k]), 0.0f, -cosf(g_Fake.yaw[k]) };
		v->fov = VIEWERS_NET_FOV;
		v->far = VIEWERS_NET_FAR;
		v->cam = NULL;
		v->ent = NULL;
		v->id = VIEWERS_FAKE_ID + k;
		v->posed = 1;
	}
	/* a viewer near a leader before it shares that leader's quota */
	g_Leaders = 0;
	for (int i = 0; i < g_Count; i++) {
		g_Set[i].leader = i;
		for (int j = 0; j < i; j++) {
			if (g_Set[j].leader == j && Viewers_DistSq(&g_Set[i].pos, &g_Set[j].pos) < g_Share * g_Share) {
				g_Set[i].leader = j;
				break;
			}
		}
		g_Leaders += g_Set[i].leader == i;
	}
}

int Viewers_Leaders(void) {
	return Viewers_Server() ? g_Leaders : 1;
}

int Viewers_LeaderIndex(int id) {
	int i = Viewers_Find(id);
	return i >= 0 && g_Set[i].leader == i ? i : -1;
}

int Viewers_NeediestLeader(int quota, int (*count)(int leader, void *ctx), void *ctx) {
	int best = -1, bestN = quota;
	for (int i = 0; i < g_Count; i++) {
		if (g_Set[i].leader != i) {
			continue;
		}
		int n = count(i, ctx);
		if (n < bestN) {
			bestN = n;
			best = i;
		}
	}
	return best;
}

/* ---- the test viewers ---- */

/* n points of the ground graph (else the flying graph), each the node farthest from those taken (the first a random
 * one), so the viewers spread over the level's streets as far apart as they can; yaw (or NULL) a random heading each */
int Viewers_Spread(int n, unsigned seed, vec3_t *pos, float *yaw) {
	CNodesWorld *nw = g_NodesWorld2D && NW2D_SUP(g_NodesWorld2D)->sup.numNodes > 0 ? NW2D_SUP(g_NodesWorld2D) :
		g_NodesWorld;
	int nodes = nw ? nw->sup.numNodes : 0;
	static float near[1 << 16];

	if (nodes <= 0) {
		return 0;
	}
	if (nodes > (int)(sizeof(near) / sizeof(near[0]))) {
		nodes = (int)(sizeof(near) / sizeof(near[0]));
	}
	uint32_t r = seed * 2654435761u + 1u;
	r = r * 1664525u + 1013904223u;
	int k = (int)((r >> 8) % (uint32_t)nodes);
	for (int j = 0; j < nodes; j++) {
		near[j] = FLT_MAX;
	}
	for (int i = 0; i < n; i++) {
		const vec3_t *p = &nw->sup.nodes[k].pos;
		pos[i] = *p;
		r = r * 1664525u + 1013904223u;
		if (yaw) {
			yaw[i] = (float)((r >> 8) % 6283u) / 1000.0f;
		}
		int far = -1;
		float farD = -1.0f;
		for (int j = 0; j < nodes; j++) {
			float d = Viewers_DistSq(&nw->sup.nodes[j].pos, p);
			near[j] = d < near[j] ? d : near[j];
			if (near[j] > farD) {
				farD = near[j];
				far = j;
			}
		}
		k = far;
	}
	return n;
}

byte Viewers_Command(const char *line) {
	int n;
	unsigned seed = 1;

	if (strncmp(line, "viewers", 7) || (line[7] && line[7] != ' ')) {
		return 0;
	}
	if (sscanf(line, "viewers fake %d %u", &n, &seed) >= 1) {
		n = n < 0 ? 0 : (n > VIEWERS_FAKE_MAX ? VIEWERS_FAKE_MAX : n);
		g_Fake.n = Viewers_Spread(n, seed, g_Fake.pos, g_Fake.yaw);
		for (int i = 0; i < g_Fake.n; i++) {
			g_Fake.pos[i].y += VIEWERS_FAKE_EYE;
		}
		Con_Printf("viewers: %d test viewers\n", g_Fake.n);
		return 1;
	}
	float share;
	if (sscanf(line, "viewers share %f", &share) == 1) {
		g_Share = share;
		Con_Printf("viewers: share %.0f\n", (double)g_Share);
		return 1;
	}
	if (!strcmp(line, "viewers clear")) {
		g_Fake.n = 0;
		Con_Printf("viewers: test viewers cleared\n");
		return 1;
	}
	Con_Printf("viewers: %d, leaders %d\n", Viewers_Count(), Viewers_Leaders());
	for (int i = 0; i < Viewers_Count(); i++) {
		const cViewer *v = Viewers_At(i);
		Con_Printf("viewer %d id %d leader %d pos %.0f %.0f %.0f dir %.2f %.2f %.2f fov %.2f cam %s\n", i, v->id, v->leader,
			(double)v->pos.x, (double)v->pos.y, (double)v->pos.z, (double)v->dir.x, (double)v->dir.y, (double)v->dir.z,
			(double)v->fov, v->cam ? v->cam->name : "-");
	}
	return 1;
}
