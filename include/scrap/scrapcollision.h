/*
 * ScrapEng/ScrapEngCollision.cpp: the collision queries of the game against the floor of the world (gMundo's CGround):
 * segments, a sphere moved in steps of half its radius (slide, slide 2D, test), the box cache of a moving sphere and
 * the sector walk. Each query has a profiler section (prof.h).
 */
#ifndef SCRAP_SCRAPCOLLISION_H
#define SCRAP_SCRAPCOLLISION_H

#include "scrap/gvar.h"
#include "scrap/ground.h"

/* the profiler sections */

/* the box cache of a moving sphere: with a valid box (bit 1, of this collision cache id) the queries inside it are
 * free; a sphere that left it waits 20 queries before the box is built again */
#define SPHEREBOX_FREE 0x1		/* no triangle in the box */
#define SPHEREBOX_INSIDE 0x2		/* the last query was inside the box */
typedef struct CSphereBox {
	uint16_t flags;			/* SPHEREBOX_* */
	uint16_t wait;
	int id;				/* g_QuadTriCacheID when it was built */
	box_t box;
} CSphereBox;

/* the triangle a segment hit, for Engine_TestLine */
typedef struct CCollisionTri {
	vec3_t v[3];
	plane_t plane;
	vec3_t hit;
	int noCollisions;		/* bit 4 of the triangle flags */
} CCollisionTri;

/* the collision epsilon divided by 8 for fine queries, with restore back */
void CollisionEpsilon_Set(byte restore);
/* the first point within r of c on the segment from a to b, into out (b without one) */
byte Collision_SegSphere(const vec3_t *a, const vec3_t *b, float cx, float cy, float cz, float r, vec3_t *out);
/* 1 if a triangle is in the box from min to max moved by pos */
byte Collision_Box(const vec3_t *pos, const vec3_t *min, const vec3_t *max);
/* the segment from-to: hit the first hit point (to without one), tri the hit triangle; each may be NULL */
byte Engine_TestLine(const vec3_t *from, const vec3_t *to, vec3_t *hit, CCollisionTri *tri);
int Collision_Sector(int sector, const vec3_t *a, const vec3_t *b, byte front);
/* the sphere slid from a to b, out the end; 1 if it collided */
byte Collision_Slide2D(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache);
byte Collision_Slide(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache);
/* the sphere moved from a to b until the first contact, out the contact position (b without one) */
byte Engine_TestSphere(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache);

#endif
