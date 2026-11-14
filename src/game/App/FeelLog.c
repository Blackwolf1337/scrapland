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
#include "scrap/objentity.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/replicate.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

static struct {
	byte tried;
	FILE *f;
	uint64_t last;
} feel;

static uint64_t Feel_Monotonic(void) {
#if defined(_WIN32)
	return Plat_RealNs();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

void FeelLog_Frame(void) {
	if (!feel.tried) {
		const char *path = getenv("SCRAP_FEELLOG");
		feel.tried = 1;
		feel.f = path && *path ? fopen(path, "w") : NULL;
		if (feel.f) {
			fputs("mono_ns,dt_ms,kind,x,y,z,cx,cy,cz,cyaw,input,corrections\n", feel.f);
		}
	}
	if (!feel.f || !g_pWorld) {
		return;
	}
	uint64_t now = Feel_Monotonic();
	double dt = feel.last ? (double)(now - feel.last) / 1e6 : 0.0;
	feel.last = now;
	cEntity *e = g_Play ? Play_Entity() : cWorld_GetUsrEntity(g_pWorld, 0);
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
	cEntity *cam = World_FindEntity(cWorld_GetCamName(g_pWorld, 0));
	vec3_t c = { 0.0f, 0.0f, 0.0f };
	float yaw = 0.0f;
	if (cam && Entity_IsType(cam, "Cam")) {
		c = ((cCamControl *)cam)->cam.pos;
		yaw = ((cCamControl *)cam)->cam.rot.y;
	}
	fprintf(feel.f, "%llu,%.3f,%c,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.4f,%.3f,%u\n", (unsigned long long)now, dt, kind,
		(double)p.x, (double)p.y, (double)p.z, (double)c.x, (double)c.y, (double)c.z, (double)yaw, (double)in,
		g_Play ? Play_Corrections() : 0u);
}
