/*
 * AI/AICharGroup.cpp of the game: the group of the character AIs (cAICharGroup, g_CharGroup, include/scrap/aichar.h).
 * Its base cAIEntityGroup lets one AI at a time take a group state; the group itself keeps the points where the
 * characters are placed again out of view (Repos). The points are the dummies "DM_NPC_Repos_<zone>_..." of the level,
 * a zone is the name of the action class of the characters that may use it. A character holds its point by the link
 * of the point to the head at 0x4bc of the character. The Global/DList.h instances of the zone lists and the
 * Global/Nto1Links.h copies of the file are here too.
 */
#include <float.h>
#include <string.h>

#include "scrap/aichar.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/fxmisc.h"
#include "scrap/nodesworld.h"
#include "scrap/scrap.h"

#define CAICHARGROUP_VFTABLE (&cAICharGroup_vftable)
static const cAIEntityGroupVtbl cAICharGroup_vftable;
#define AICG_PI2 1.5707963267948966	/* pi / 2 */
#define AICG_POINT_CTOR 0x080770
#define AICG_POINT_DTOR 0x080850

/* ---- the points and the Nto1Links.h copies ---- */

/* the constructor and GetReposPos (SAI.GetReposPos) are declared in ai.h */
static cAICGPoint *cAICGPoint_ctor(cAICGPoint *self) {
	self->link.target = NULL;
	self->link.data = NULL;
	self->link.next = &self->link;
	self->link.prev = &self->link;
	return self;
}

static void cAICGPoint_dtor(cAICGPoint *self) {
	Nto1Node_Done(&self->link);
}

/* ---- the lists of the dummies ---- */

static void cAICGPosList_Clear(cAICGPosList *list) {
	DList_Clear(list);
}

static void cAICGZoneData_dtor(cAICGZoneData *zone) {
	cAICGPosList_Clear(&zone->points);
	DList_FreePool(&zone->points);
}

static void ZoneNode_Free(cAICGZoneNode *node) {
	cAICGZoneData_dtor(&node->zone);
	operator_delete(node);
}

/* to the free list while it has room, else deleted with its points */
static void ZoneList_Release(cAICGZoneList *list, cAICGZoneNode *node) {
	if (list->numFree < list->maxFree) {
		node->next = list->freeNodes;
		list->numFree++;
		list->freeNodes = node;
	} else {
		ZoneNode_Free(node);
	}
}

/* the lists of the dummies: emptied (the free nodes kept), the free nodes deleted, both (the destructors) */
static void cAICGZoneList_Clear(cAICGZoneList *list) {
	while (list->head) {
		cAICGZoneNode *p = list->head;
		list->iter = NULL;
		if (p == p->next) {
			ZoneList_Release(list, p);
			list->head = NULL;
		} else {
			list->head = p->next;
			p->prev->next = p->next;
			p->next->prev = p->prev;
			ZoneList_Release(list, p);
		}
		list->count--;
	}
}

static void cAICGZoneList_FreePool(cAICGZoneList *list) {
	cAICGZoneNode *node;

	while ((node = list->freeNodes) != NULL) {
		list->freeNodes = node->next;
		ZoneNode_Free(node);
	}
	list->numFree = 0;
}

/* ---- the group ---- */

static void cAICharGroup_Init(cAICharGroup *self);
cAICharGroup *cAICharGroup_ctor(void *mem) {
	cAICharGroup *self = mem;

	cAIEntityGroup_ctor(&self->base);
	self->vftable = CAICHARGROUP_VFTABLE;
	self->zones = NULL;
	self->numZones = 0;
	cAICharGroup_Init(self);
	return self;
}

static void cAICharGroup_dtor(cAICharGroup *self) {
	for (int i = 0; i < self->numZones; i++) {
		cAICGPoint *p = self->zones[i].points;
		if (p) {
			Delete_Objects(p, sizeof(cAICGPoint), (void *)cAICGPoint_dtor);
		}
	}
	if (self->zones) {
		Mem_Free(self->zones);
		self->zones = NULL;
	}
	cAIEntityGroup_dtor(&self->base);
}

static void cAICharGroup_Free(cAIEntityGroup *base) {
	cAICharGroup *self = (cAICharGroup *)base;
	cAICharGroup_dtor(self);
	operator_delete(self);
}

static void AICG_NoMemory(void) {
	Con_Bluef("ERROR AI: IniCharGroup - Insuficiente memoria ListCharRepos.\n");
	CheckFailed("cAICharGroup_IniCharGroup_new_ListCharRepos_Fail", __FILE__, __TIMESTAMP__, __LINE__);
}

/* the zone of that name in the list or NULL (the iterator of the list walks along) */
static cAICGZoneData *ZoneList_Find(cAICGZoneList *list, const char *name) {
	cAICGZoneNode *node = list->head;

	list->iter = node;
	while (node) {
		if (!strcmp(name, node->zone.name)) {
			return &node->zone;
		}
		node = node->next;
		list->iter = node;
		if (node == list->head) {
			break;
		}
	}
	return NULL;
}

/* a new zone of that name at the head of the list */
static cAICGZoneData *ZoneList_Add(cAICGZoneList *list, const char *name) {
	cAICGZoneData tmp;
	cAICGZoneNode *node;

	memset(&tmp, 0, sizeof(tmp));
	list->iter = NULL;
	if (list->freeNodes) {
		node = list->freeNodes;
		list->numFree--;
		list->freeNodes = node->next;
	} else {
		node = operator_new(sizeof(cAICGZoneNode));
		if (node) {
			memset(&node->zone.points, 0, sizeof(node->zone.points));
		}
	}
	node->zone = tmp;
	DList_LinkFirst(list, node);
	list->iter = node;
	Str_Copy(node->zone.name, name, sizeof(node->zone.name));
	cAICGZoneData_dtor(&tmp);
	return &node->zone;
}

/* the zones and their points from the dummies of the level */
static void cAICharGroup_Init(cAICharGroup *self) {
	cAICGZoneList list;
	char dummy[0x64];
	char name[100];
	vec3_t pos;
	float ang[2];
	byte flag;

	memset(&list, 0, sizeof(list));
	memcpy(dummy, "DM_NPC_Repos_*", sizeof("DM_NPC_Repos_*"));
	DummySearch_Start(dummy, 1);
	while (DummySearch_Next(dummy, sizeof(dummy), &pos, ang, &flag)) {
		crt_sscanf(dummy, "DM_NPC_Repos_%s", name);
		const char *zname = crt_strtok(name, "_");
		cAICGZoneData *zone = ZoneList_Find(&list, zname);
		if (!zone) {
			zone = ZoneList_Add(&list, zname);
		}
		cAICGPosNode *p = DList_NewNode(&zone->points, sizeof(cAICGPosNode));
		p->pos = pos;
		DList_LinkFirst(&zone->points, p);
	}
	self->numZones = list.count;
	self->zones = Mem_Alloc(list.count * sizeof(cAICGZone));
	if (!self->zones) {
		AICG_NoMemory();
	}
	cAICGZoneNode *node = list.head;
	list.iter = node;
	for (int i = 0; node; i++) {
		cAICGZone *z = &self->zones[i];
		Str_Copy(z->name, node->zone.name, sizeof(z->name));
		z->numPoints = node->zone.points.count;
		z->points = New_Objects(z->numPoints, sizeof(cAICGPoint), (void *)cAICGPoint_ctor, (void *)cAICGPoint_dtor);
		if (!z->points) {
			AICG_NoMemory();
		}
		cAICGPosList *pl = &node->zone.points;
		cAICGPosNode *p = pl->head;
		pl->iter = p;
		for (int k = 0; p; k++) {
			z->points[k].pos = p->pos;
			z->points[k].link.data = &z->points[k];
			p = p->next;
			pl->iter = p;
			if (p == pl->head) {
				break;
			}
		}
		node = node->next;
		list.iter = node;
		if (node == list.head) {
			break;
		}
	}
	cAICGZoneList_Clear(&list);
	cAICGZoneList_FreePool(&list);
}

/* ---- the repositioning ---- */

static inline cAICGPoint *AICG_Point(cAICharGroup *self, int zone, int i) {
	return &self->zones[zone].points[i];
}

/* the point taken by e (the head of e to it) and its position into out */
static void AICG_Take(cAICGPoint *p, cEntity *e, vec3_t *out) {
	Nto1Node_LinkTo(&WChar(e)->reposHead, &p->link);
	*out = p->pos;
}

static void AICG_Give(cAICGPoint *p) {
	CHECK(p->link.data);
	Nto1Node_Unlink(&p->link);
}

/* the square of the distance from the camera, as the originals sum it (y, x, z) */
static inline float AICG_DistSq(const vec3_t *cam, const vec3_t *p) {
	float dy = cam->y - p->y;
	float dx = cam->x - p->x;
	float dz = cam->z - p->z;

	return dy * dy + dx * dx + dz * dz;
}

byte cAICharGroup_GetReposPos(cAICharGroup *self, const vec3_t *pos, const char *name, vec3_t *out,
	cEntity *target) {
	cWorld *w = g_pWorld;
	vec3_t cam = w->camPos[w->curCam];
	float best = FLT_MAX;
	int bestZone = 0, bestPoint = 0;
	byte found = 0;

	(void)pos;
	for (int i = 0; i < self->numZones; i++) {
		cAICGZone *z = &self->zones[i];
		if (strcmp(name, z->name)) {
			continue;
		}
		for (int k = 0; k < z->numPoints; k++) {
			cAICGPoint *p = &z->points[k];
			if (p->link.target) {
				continue;
			}
			float d = AICG_DistSq(&cam, &p->pos);
			if (!(d > best)) {
				best = d;
				bestZone = i;
				found = 1;
				bestPoint = k;
			}
		}
		if (found) {
			cAICGPoint *p = AICG_Point(self, bestZone, bestPoint);
			*out = p->pos;
			if (target) {
				Nto1Node_LinkTo(&WChar(target)->reposHead, &p->link);
			}
			return 1;
		}
	}
	return 0;
}

byte cAICharGroup_FindRepos(cAICharGroup *self, vec3_t *out, cEntity *e, float minDist, float maxDist,
	byte flag) {
	byte good = 0, spare = 0, own = 0, front = 0;
	int ownZone = 0, ownPoint = 0, spareZone = 0, sparePoint = 0, goodZone = 0, goodPoint = 0;
	vec3_t fwd = {0.0f, 0.0f, 1.0f};
	vec3_t look, v;
	cEntity *hit;
	int zi;

	const char *name = ((cActCtrl *)WithLife_GetActCtrl(e))->cls->name;
	cWorld *w = g_pWorld;
	vec3_t cam = w->camPos[w->curCam];
	cEntity *camEnt = World_FindEntity(cWorld_GetCamName(w, 0));
	/* of the camera entity: its angles */
	Vec3_Rotate(&look, &fwd, &((cCamControl *)camEnt)->cam.rot);
	for (zi = 0; zi < self->numZones; zi++) {
		if (!strcmp(name, self->zones[zi].name)) {
			break;
		}
	}
	if (zi >= self->numZones) {
		return 0;
	}
	int idx = crt_rand() % self->zones[zi].numPoints;
	int up = crt_rand() % 2;
	for (int k = 0; k < self->zones[zi].numPoints; k++) {
		int n = self->zones[zi].numPoints;
		if (up) {
			idx = (idx + 1) % n;
		} else if (idx == 0) {
			idx = n - 1;
		} else {
			idx--;
		}
		cAICGPoint *p = AICG_Point(self, zi, idx);
		cEntity *t = p->link.target;
		if (t == e) {
			ownZone = zi;
			ownPoint = idx;
			own = 1;
			if (front) {
				break;
			}
			continue;
		}
		if (t || front) {
			continue;
		}
		if (cWorld_TestSphereEntities(g_pWorld, e, 0x10, &p->pos, WChar(e)->wl.prim.radius, &hit, 1, 0)) {
			continue;
		}
		if (!flag) {
			spareZone = zi;
			sparePoint = idx;
			spare = 1;
		}
		p = AICG_Point(self, zi, idx);
		float d = AICG_DistSq(&cam, &p->pos);
		if (d > maxDist * maxDist) {
			continue;
		}
		if (Floor_Get(&g_pWorld->floor, cam.y) != Floor_Get(&g_pWorld->floor, AICG_Point(self, zi, idx)->pos.y)) {
			continue;
		}
		p = AICG_Point(self, zi, idx);
		v.x = p->pos.x - cam.x;
		v.y = p->pos.y - cam.y;
		v.z = p->pos.z - cam.z;
		byte inFront = !(Vec3_Angle(&v, &look) > AICG_PI2);
		if (flag && (inFront || minDist * minDist > d) && CNodesWorld_PointSeen(g_NodesWorld, &p->pos)) {
			continue;
		}
		goodZone = zi;
		goodPoint = idx;
		good = 1;
		if (inFront) {
			front = 1;
			if (own) {
				break;
			}
		}
	}
	if (good) {
		if (own) {
			AICG_Give(AICG_Point(self, ownZone, ownPoint));
		}
		AICG_Take(AICG_Point(self, goodZone, goodPoint), e, out);
		return 1;
	}
	if (flag || !spare) {
		return 0;
	}
	if (own) {
		AICG_Give(AICG_Point(self, ownZone, ownPoint));
	}
	AICG_Take(AICG_Point(self, spareZone, sparePoint), e, out);
	return 1;
}

/* the vtable of cAICharGroup */
static const cAIEntityGroupVtbl cAICharGroup_vftable = {
	.Free = cAICharGroup_Free,
	.Clear = cAIEntityGroup_Clear,
	.Add = cAIEntityGroup_Add,
	.Get = cAIEntityGroup_Get,
	.Num = cAIEntityGroup_Num,
	.CanChange = cAIEntityGroup_CanChange,
	.ResetChange = cAIEntityGroup_ResetChange,
	.AllowChange = cAIEntityGroup_AllowChange,
	.Update = cAIEntityGroup_Update,
};
