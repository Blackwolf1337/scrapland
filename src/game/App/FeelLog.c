/*
 * The feel log, see include/scrap/feellog.h. Not in the original.
 */
#include <stdio.h>
#include <stdlib.h>
#if !defined(_WIN32)
#include <time.h>
#endif

#include "scrap/camcontrol.h"
#include "scrap/entity.h"
#include "scrap/feellog.h"
#include "scrap/input.h"
#include "scrap/objentity.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/replicate.h"
#include "scrap/scorer.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

#define FEEL_SEEN 64
#define FEEL_NEAR 5000.0f			/* units from the own entity a new missile counts */

static struct {
	byte tried;
	FILE *f;
	uint64_t last;
	uint32_t fires;
	uint64_t drawn;					/* the time the frame before was drawn (FeelLog_Render), 0: not yet */
	uint32_t missiles;				/* the missile entities that came near the own entity so far */
	const cEntity *seen[FEEL_SEEN];	/* the newest of them (a countermeasure's or a missile weapon's on the screen) */
	int nextSeen;
	char line[256];					/* the values of the frame before, written with that time */
} feel;

/* the own entity: the client's, single player's of slot 0 */
static cEntity *Feel_Own(void) {
	return g_Play ? Play_Entity() : cWorld_GetUsrEntity(g_pWorld, 0);
}

void FeelLog_Fire(cEntity *e) {
	if (feel.f && e && e == Feel_Own()) {
		feel.fires++;
	}
}

static uint64_t Feel_Monotonic(void) {
#if defined(_WIN32)
	return Plat_RealNs();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

/* the missiles near the own entity not seen before (a countermeasure's cloud, mine or EMI, a missile weapon's
 * missiles: single player's own, a client's replicas or its own launch) */
static void Feel_Missiles(const cEntity *own) {
	for (cEntity *m = own ? cWorld_GetFirstEntity(g_pWorld) : NULL; m; m = cEntity_Next(m)) {
		if (!Entity_IsType(m, "Missile")) {
			continue;
		}
		float dx = m->pos.x - own->pos.x, dy = m->pos.y - own->pos.y, dz = m->pos.z - own->pos.z;
		if (dx * dx + dy * dy + dz * dz > FEEL_NEAR * FEEL_NEAR) {
			continue;
		}
		int k = 0;
		while (k < FEEL_SEEN && feel.seen[k] != m) {
			k++;
		}
		if (k == FEEL_SEEN) {
			feel.seen[feel.nextSeen] = m;
			feel.nextSeen = (feel.nextSeen + 1) % FEEL_SEEN;
			feel.missiles++;
		}
	}
}

void FeelLog_Frame(void) {
	if (!feel.tried) {
		const char *path = getenv("SCRAP_FEELLOG");
		feel.tried = 1;
		feel.f = path && *path ? fopen(path, "w") : NULL;
		if (feel.f) {
			fputs("mono_ns,dt_ms,kind,x,y,z,cx,cy,cz,cyaw,input,corrections,fires,view,missiles\n", feel.f);
		}
	}
	if (!feel.f || !g_pWorld) {
		return;
	}
	/* the line of the frame before, at the time it was drawn in this frame (or now, when it was not drawn) */
	if (feel.line[0]) {
		uint64_t at = feel.drawn ? feel.drawn : Feel_Monotonic();
		double dt = feel.last ? (double)(at - feel.last) / 1e6 : 0.0;
		feel.last = at;
		fprintf(feel.f, "%llu,%.3f,%s\n", (unsigned long long)at, dt, feel.line);
	}
	feel.drawn = 0;
	cEntity *e = Feel_Own();
	char kind = '-';
	float in = 0.0f;
	vec3_t p = { 0.0f, 0.0f, 0.0f };
	if (e && Entity_IsType(e, TYEN_CAR)) {
		kind = 'S';
		in = Veh(e)->controls.acc;
	} else if (e && Entity_IsType(e, TYEN_WALKCHAR)) {
		kind = 'C';
		in = WChar(e)->control.walk;
	}
	if (e) {
		p = Obj(e)->renderPos;
	}
	Feel_Missiles(e);
	cEntity *cam = World_FindEntity(cWorld_GetCamName(g_pWorld, 0));
	vec3_t c = { 0.0f, 0.0f, 0.0f };
	float yaw = 0.0f;
	if (cam && Entity_IsType(cam, "Cam")) {
		c = ((cCamControl *)cam)->cam.pos;
		yaw = ((cCamControl *)cam)->cam.rot.y;
	}
	/* what a flow of the scripts changes on the screen (stage 6b, include/scrap/play.h PlayFlow_): the camera's link,
	 * a fade, the HUD's or a menu's items, the action set */
	char *link = NULL;
	if (cam && Entity_IsType(cam, "Cam")) {
		Entity_VarS(cam, "EntityLink", &link, 0);
	}
	uint32_t h = 5381;
	for (const char *q = link ? link : ""; *q; q++) {
		h = h * 33u + (byte)*q;
	}
	cScorer *sc = Scorer_Get(0);
	int items = 0, fade = 0;
	for (cScorerItem *it = sc ? sc->items : NULL; it; it = it->next) {
		items++;
	}
	if (sc) {
		fade = sc->fade.time > 0.0f || sc->fade.to[3] > 0.004f;
	}
	snprintf(feel.line, sizeof(feel.line), "%c,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.4f,%.3f,%u,%u,L%08xF%dH%dA%u,%u", kind,
		(double)p.x, (double)p.y, (double)p.z, (double)c.x, (double)c.y, (double)c.z, (double)yaw, (double)in,
		g_Play ? Play_Corrections() : 0u, feel.fires, h, fade, items, Input_GetActionSet(), feel.missiles);
}

void FeelLog_Render(void) {
	if (feel.f) {
		feel.drawn = Feel_Monotonic();
	}
}
