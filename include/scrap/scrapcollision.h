/*
 * ScrapEng/ScrapEngCollision.cpp: the collision queries of the game against the floor of the world (gMundo's CGround):
 * segments, a sphere moved in steps of half its radius (slide, slide 2D, test), the box cache of a moving sphere and
 * the sector walk. Each query has a profiler section (prof.h).
 */
#ifndef SCRAP_SCRAPCOLLISION_H
#define SCRAP_SCRAPCOLLISION_H

#include "scrap/suelo.h"

/* the profiler sections */
#define PROF_SLIDE 0x10
#define PROF_SLIDE2D 0x11
#define PROF_TESTSPHERE 0x12
#define PROF_TESTLINE 0x13
#define PROF_BOX 0x14
#define PROF_SEGSPHERE 0x15
#define PROF_CHECK 0x16
#define PROF_SECTOR 0x17

/* the box cache of a moving sphere: with a valid box (bit 1, of this collision cache id) the queries inside it are
 * free; a sphere that left it waits 20 queries before the box is built again */
#define SPHEREBOX_FREE 0x1		/* no triangle in the box */
#define SPHEREBOX_INSIDE 0x2		/* the last query was inside the box */
typedef struct CSphereBox {
	uint16_t flags;			/* 0x00 SPHEREBOX_* */
	uint16_t wait;			/* 0x02 */
	int id;				/* 0x04 g_QuadTriCacheID when it was built */
	box_t box;			/* 0x08 */
} CSphereBox;
SD_SIZE(CSphereBox, 0x20);

/* the triangle a segment hit, for Engine_TestLine */
typedef struct CCollisionTri {
	vec3_t v[3];			/* 0x00 */
	plane_t plane;			/* 0x24 */
	vec3_t hit;			/* 0x34 */
	int noCollisions;		/* 0x40 bit 4 of the triangle flags */
} CCollisionTri;
SD_SIZE(CCollisionTri, 0x44);

/* the engine variables of the collisions (Gvars) */
#define g_CollisionSchk GAME_GLOBAL_IN(int, GVar_c_schk, 16, 0x4a810c)	/* c_schk: a sphere that does not move is a check */
#define g_CollisionCaps GAME_GLOBAL_IN(int, GVar_c_caps, 16, 0x4a81cc)	/* c_caps: a vertical test is one sweep */
#define g_CollisionIter GAME_GLOBAL_IN(int, GVar_c_iter, 16, 0x4a8154)	/* c_iter: from 2 the test steps do not sweep */
#define g_CollisionHold GAME_GLOBAL_IN(int, GVar_c_hold, 16, 0x4a81e4)	/* c_hold: the tests keep the sphere at its start */
#define g_CollisionSld2 GAME_GLOBAL_IN(int, GVar_c_sld2, 16, 0x4a816c)	/* c_sld2: the iterations of a slide 2D step */
#define g_CollisionSldi GAME_GLOBAL_IN(int, GVar_c_sldi, 16, 0x4a8184)	/* c_sldi: the iterations of a slide step */
#define g_CollisionScts GAME_GLOBAL_IN(int, GVar_c_scts, 16, 0x4a819c)	/* c_scts: the check sweeps (else contact) */
#define g_CollisionSbox GAME_GLOBAL_IN(int, GVar_c_sbox, 16, 0x4a813c)	/* c_sbox: the box cache */
#define g_CollisionSboxShow GAME_GLOBAL_IN(int, GVar_c_sboxshow, 16, 0x4a81b4)	/* c_sboxshow: debug points of the box cache */
#define g_CollisionSboxMult GAME_GLOBAL_IN(float, GVar_c_sboxmult, 16, 0x4a8124)	/* c_sboxmult: the box reaches this many moves ahead */
#define g_LastCollisionEpsilon GAME_GLOBAL(float, g_LastCollisionEpsilon, 0x4a81ec)

/* the collision epsilon divided by 8 for fine queries, with restore back */
void CollisionEpsilon_Set(byte restore);
/* the first point within r of c on the segment from a to b, into out (b without one) */
byte Collision_SegSphere(const vec3_t *a, const vec3_t *b, float cx, float cy, float cz, float r, vec3_t *out);
/* 1 if the move from a to b of a sphere of radius r is free by the box cache */
byte Collision_SphereBox(const vec3_t *a, const vec3_t *b, float r, CSphereBox *cache);
/* 1 if a triangle is in the box from min to max moved by pos */
byte Collision_Box(const vec3_t *pos, const vec3_t *min, const vec3_t *max);
/* the segment from-to: hit the first hit point (to without one), tri the hit triangle; each may be NULL */
byte Engine_TestLine(const vec3_t *from, const vec3_t *to, vec3_t *hit, CCollisionTri *tri);
int Collision_Sector(int sector, const vec3_t *a, const vec3_t *b, byte front);
/* 1 if a sphere of radius r at pos touches the floor */
byte Collision_Check(const vec3_t *pos, float r);
/* the sphere slid from a to b, out the end; 1 if it collided */
byte Collision_Slide2D(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache);
byte Collision_Slide(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache);
/* the sphere moved from a to b until the first contact, out the contact position (b without one) */
byte Engine_TestSphere(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache);

#endif
