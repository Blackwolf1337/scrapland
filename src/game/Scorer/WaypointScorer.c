/*
 * WaypointScorer.cpp of the game (Menu/): cWaypointScorer, the arrow or the rectangle of the current waypoint. See
 * include/scrap/scorerhud.h.
 */
#include "scrap/objentity.h"
#include "scrap/scorerhud.h"

/* the waypoint kind of an object (1..3 the color of the mark), 0 for none */
#define Waypoint_Kind(e) (*(int *)((byte *)(e) + 0x13c))

cWaypointScorer *__fastcall cWaypointScorer_ctor(cWaypointScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = Game_Ptr(CWAYPOINTSCORER_VFTABLE);
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
	self->base.vftable = Game_Ptr(CWAYPOINTSCORER_VFTABLE);
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
