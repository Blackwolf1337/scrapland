/*
 * Entities/More/Primitive.cpp of the game (src/game/Entities/More/Primitive.c): cPrimitive, the collision sphere of
 * an entity with life (at 0x39c, "VRadius"). The sphere is on when its radius is above 0; the tests take the position
 * of the owner.
 */
#ifndef SCRAP_PRIMITIVE_H
#define SCRAP_PRIMITIVE_H

#include "scrap/entity.h"

typedef struct cPrimitive {
	cEntity *owner;			/* 0x0 */
	float radius;			/* 0x4 "VRadius" */
	byte on;			/* 0x8 */
	byte pad_9[3];
} cPrimitive;


cPrimitive *cPrimitive_ctor(cPrimitive *self);
byte cPrimitive_VarF(cPrimitive *self, const char *name, double *d, int set);
/* the segment from a to *p into the sphere: *p the first point in it; 1 then */
byte cPrimitive_TestSegment(cPrimitive *self, const vec3_t *a, vec3_t *p);
byte cPrimitive_SegHit(cPrimitive *self, const vec3_t *a, vec3_t *p);
/* on only with a radius */
void cPrimitive_SetOn(cPrimitive *self, byte on);
void cPrimitive_SetOwner(cPrimitive *self, cEntity *owner);
/* the radius (CHECK the owner and >= 0); on unless 0 */
void cPrimitive_SetSize(cPrimitive *self, float size);
/* the sphere of other at c (moving from prev, NULL for none) touches this one: 1, and other is told where (its
 * vtable slot 0x2c with the owner, the point on this sphere and the direction) unless g_pWorld->testing */
byte cPrimitive_TestSphere(cPrimitive *self, cEntity *other, const vec3_t *c, float r, const vec3_t *prev);
byte cPrimitive_SphereHit(cPrimitive *self, cEntity *other, const vec3_t *c, float r, const vec3_t *prev);
/* out (a point near from) moved to the distance of both radii plus 1 from from, xz: only in x and z */
void cPrimitive_UnelasticDisp(cPrimitive *self, vec3_t *out, const vec3_t *from, const cPrimitive *other,
	byte xz);

#endif
