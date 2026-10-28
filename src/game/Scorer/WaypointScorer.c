/*
 * WaypointScorer.cpp of the game (Menu/): cWaypointScorer, the arrow or the rectangle of the current waypoint. See
 * include/scrap/scorerhud.h.
 */
#include "scrap/objentity.h"
#include "scrap/scorerhud.h"
#include "scrap/camcontrol.h"
#include "scrap/crt.h"
#include "scrap/mathlib.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/timer.h"
#include "scrap/world.h"

/* the waypoint kind of an object (1..3 the color of the mark), 0 for none */
#define Waypoint_Kind(e) (*(int *)((byte *)(e) + 0x13c))

cWaypointScorer *__fastcall cWaypointScorer_ctor(cWaypointScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CWAYPOINTSCORER_VFTABLE;
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x20) : NULL;
	self->img = img;
	img->color = 0xffffffff;
	self->dotIndex = 1;
	self->testTime = 0.0f;
	self->visible = 1;
	return self;
}

void __fastcall cWaypointScorer_dtor(cWaypointScorer *self) {
	self->base.vftable = CWAYPOINTSCORER_VFTABLE;
	ScorerImg_Free(self->img);
	cScorerItem_dtor(&self->base);
}

cWaypointScorer *__thiscall cWaypointScorer_Delete(cWaypointScorer *self, byte flags) {
	cWaypointScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cWaypointScorer));
	}
	return self;
}

int __thiscall cWaypointScorer_Var(cWaypointScorer *self, const char *name, byte set, char **s, double *d, int *i) {
	if (s && !crt_stricmp(name, "Sprite") && set) {
		self->img = ScorerImg_Replace(self->img, *s, 2, 0x10060);
		return 1;
	}
	if (i) {
		if (!crt_stricmp(name, "WaypointRectIndex")) {
			return ScorerVar_Int(&self->rectIndex, set, i);
		}
		if (!crt_stricmp(name, "WaypointArrowIndex")) {
			return ScorerVar_Int(&self->arrowIndex, set, i);
		}
		if (!crt_stricmp(name, "WaypointDotIndex")) {
			return ScorerVar_Int(&self->dotIndex, set, i);
		}
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

/* the entity of the slot of the world player 0 (World.h: UsrEntities, CHECKed to be with life) */
static cEntity *Waypoint_User(void) {
	cEntity *e = g_pWorld->owners[0];
	if (e) {
		CHECK(Entity_IsType(e, TYEN_WITH_LIFE));
	}
	return g_pWorld->owners[0];
}

#define WAYPOINT_RECT_SIZE 200.0f	/* the rectangle per radius and depth, half of it in split screen */
#define WAYPOINT_RECT_MIN 12.0f
#define WAYPOINT_TEST_TIME 0.25f	/* the line of sight is tested 4 times a second */
#define WAYPOINT_DOT_SCALE 0.6f
#define WAYPOINT_DOT_ALPHA 223.0f	/* the dot blinks once a second */
#define WAYPOINT_HALF_W 320.0f
#define WAYPOINT_HALF_H 240.0f
#define WAYPOINT_ARROW_EDGE 0.8f	/* the arrow on this part of the way to the edge, 0.6 in split screen */
#define WAYPOINT_ARROW_SPLIT 0.6f	/* also the scale of an arrow that is not the current waypoint */

/* the y of a screen point in the view of the player in split screen */
static float Waypoint_SplitY(int player, float y) {
	if (g_SplitScreenGame) {
		y = y * 0.5f;
		if (player == 1) {
			y = y + WAYPOINT_HALF_H;
		}
	}
	return y;
}

/* the four corners of the rectangle around a waypoint in sight, each a turned copy of the corner frame */
static void Waypoint_DrawRect(cWaypointScorer *self, cEntity *e, const float *scr, float depth) {
	float d = X87_MulMulF(((const cObjEntityVtbl *)e->vftable)->GetRadius(e),
		g_SplitScreenGame ? WAYPOINT_RECT_SIZE * 0.5f : WAYPOINT_RECT_SIZE, depth);
	if (WAYPOINT_RECT_MIN > d) {
		d = WAYPOINT_RECT_MIN;
	}
	ScorerHud_DrawCorners(self->img, self->rectIndex, scr, d);
}

/* the blinking dot on a waypoint in sight that is not the current one, while the camera sees it */
static void Waypoint_DrawDot(cWaypointScorer *self, cEntity *e, cEntity *cam, const float *scr) {
	vec3_t c, t;

	c = g_pWorld->camPos[g_pWorld->curCam];
	((const cCamControlVtbl *)cam->vftable)->GetCamPos(cam, &c.x, &c.y, &c.z);
	float tt = self->testTime - Scorer_Get(self->base.player)->frameTime;
	self->testTime = tt;
	if (!(0.0f < tt)) {
		self->visible = Engine_TestLine(((const cObjEntityVtbl *)e->vftable)->GetViewPos(e, &t), &c, NULL, NULL);
		self->testTime = WAYPOINT_TEST_TIME;
	}
	if (!self->visible) {
		return;
	}
	float now = Timer_GetTime(&g_pWorld->timer);
	float whole = (float)crt_floor((double)Timer_GetTime(&g_pWorld->timer));
	int a = (int)((now - whole - 0.5f) * WAYPOINT_DOT_ALPHA);
	CPsaImage *img = self->img;
	img->color = (img->color & 0xffffff) | (uint32_t)((a < 0 ? -a : a) + 0x20) << 24;
	img->pivotX = 0.5f;
	img->pivotY = 0.5f;
	img->x = scr[0];
	img->y = scr[1];
	img->scaleX = WAYPOINT_DOT_SCALE;
	img->scaleY = WAYPOINT_DOT_SCALE;
	CPsaImage_DrawFrame(img, self->dotIndex, scr[0], scr[1]);
}

/* the arrow from the middle of the screen towards a waypoint out of sight */
static void Waypoint_DrawArrow(cWaypointScorer *self, const vec3_t *p, byte current) {
	float scr[2];
	CPsaImage *img = self->img;

	((byte *)&img->color)[3] = 0xff;
	Engine_WorldToScreen(p, scr, NULL);
	float sx = scr[0], sy = scr[1];
	float a = ScorerHud_ArrowAngle(sx, sy);
	float ex = sx - WAYPOINT_HALF_W, ey = sy - WAYPOINT_HALF_H;
	if (g_SplitScreenGame) {
		ex = ex * WAYPOINT_ARROW_SPLIT;
		ey = ey * WAYPOINT_ARROW_SPLIT;
	} else {
		ex = ex * WAYPOINT_ARROW_EDGE;
		ey = ey * WAYPOINT_ARROW_EDGE;
	}
	float x = ex + WAYPOINT_HALF_W;
	float y = Waypoint_SplitY(self->base.player, ey + WAYPOINT_HALF_H);
	float sc = current ? 1.0f : WAYPOINT_ARROW_SPLIT;
	img->scaleX = sc;
	img->scaleY = sc;
	ScorerHud_DrawArrow(img, self->arrowIndex, a, x, y, 0);
}

void __thiscall cWaypointScorer_DrawTarget(cWaypointScorer *self, cEntity *e, byte current) {
	float scr[2], depth, cx, cy, cz;
	vec3_t p;

	cEntity *cam = World_FindEntity(cWorld_GetCamName(g_pWorld, self->base.player));
	if (!cam || !Entity_IsType(cam, "FCam")) {
		return;
	}
	((const cObjEntityVtbl *)e->vftable)->GetViewPos(e, &p);
	((const cCamControlVtbl *)cam->vftable)->GetCamPos(cam, &cx, &cy, &cz);
	byte vis = Engine_WorldToScreen(&p, scr, &depth);
	scr[1] = Waypoint_SplitY(self->base.player, scr[1]);
	switch (Waypoint_Kind(e)) {
	case 1:
		self->img->color = 0x80ffff00;
		break;
	case 2:
		self->img->color = 0x8000ffff;
		break;
	case 3:
		self->img->color = 0x80ff0000;
		break;
	}
	if (!vis) {
		Waypoint_DrawArrow(self, &p, current);
	} else if (current) {
		Waypoint_DrawRect(self, e, scr, depth);
	} else {
		Waypoint_DrawDot(self, e, cam, scr);
	}
}

void __thiscall cWaypointScorer_Draw(cWaypointScorer *self, int player) {
	static const vec3_t zero = {0.0f, 0.0f, 0.0f};
	const vec3_t *p;
	const char *key;
	vec3_t tmp, from;
	cEntity *e, *best;
	float bestDist;

	if (Waypoint_User()) {
		cEntity *u = Waypoint_User();
		p = u->vftable->GetPos(u, &tmp);
	} else {
		tmp = zero;
		p = &tmp;
	}
	from = *p;
	/* a lookup of the waypoint the original does not use */
	if (*g_CurrentWaypointName) {
		World_FindEntity(g_CurrentWaypointName);
	}
	if (player == 1 && g_CurrentWaypointName2 && *g_CurrentWaypointName2) {
		key = g_CurrentWaypointName2;
	} else {
		key = g_CurrentWaypointName;
	}
	if (*key) {
		e = World_FindEntity(key);
		if (e && Entity_IsType(e, TYEN_OBJECT)) {
			cWaypointScorer_DrawTarget(self, e, 1);
			return;
		}
	}
	/* no current waypoint: the nearest object that is one */
	best = NULL;
	bestDist = 99999997952.0f;
	for (e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (Entity_IsType(e, TYEN_OBJECT) && Waypoint_Kind(e)) {
			const vec3_t *v = e->vftable->GetPos(e, &tmp);
			float dy = v->y - from.y;
			float dx = v->x - from.x;
			float dz = v->z - from.z;
			float d = dy * dy + dx * dx + dz * dz;
			if (bestDist > d) {
				bestDist = d;
				best = e;
			}
		}
	}
	if (best) {
		cWaypointScorer_DrawTarget(self, best, 0);
	}
}
