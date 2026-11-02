/*
 * ScrapEngCollision.cpp of the game: the collision queries against the floor, see include/scrap/scrapcollision.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/e3dworld.h"
#include "scrap/scrapcollision.h"
#include "scrap/groundportal.h"
#include "scrap/staticinit.h"

float g_LastCollisionEpsilon;

void CollisionEpsilon_Set(byte restore) {
	if (restore) {
		if (g_LastCollisionEpsilon == 0.0f) {
			CheckFailed("LastCollisionEpsilon", __FILE__, __TIMESTAMP__, __LINE__);
		}
		if (g_CollisionEpsilon != g_LastCollisionEpsilon * 0.125f) {
			CheckFailed("CollisionEpsilon == LastCollisionEpsilon/8.0f", __FILE__, __TIMESTAMP__, __LINE__);
		}
		g_CollisionEpsilon = g_LastCollisionEpsilon;
		g_LastCollisionEpsilon = 0.0f;
		return;
	}
	if (g_CollisionEpsilon == 0.0f) {
		CheckFailed("CollisionEpsilon", __FILE__, __TIMESTAMP__, __LINE__);
	}
	if (g_LastCollisionEpsilon != 0.0f) {
		CheckFailed("LastCollisionEpsilon == 0.0", __FILE__, __TIMESTAMP__, __LINE__);
	}
	g_LastCollisionEpsilon = g_CollisionEpsilon;
	g_CollisionEpsilon = g_CollisionEpsilon * 0.125f;
}

byte Collision_SegSphere(const vec3_t *a, const vec3_t *b, float cx, float cy, float cz, float r, vec3_t *out) {
	byte res = 0;

	if (a->x == b->x && a->y == b->y && a->z == b->z) {
		if (out) {
			*out = *b;
		}
		goto done;
	}
	float dx = b->x - a->x, dy = b->y - a->y, dz = b->z - a->z;
	float mx = a->x - cx, my = a->y - cy, mz = a->z - cz;
	float qa = dx * dx + dy * dy + dz * dz;
	float qb = mx * dx + my * dy + mz * dz;
	qb = qb + qb;
	float qc = mx * mx + my * my + mz * mz - r * r;
	if (out) {
		*out = *b;
	}
	if ((double)(g_CollisionEpsilon * 1e-4f) >= __builtin_fabs((double)qa)) {
		goto done;
	}
	float disc = qb * qb - qa * 4.0f * qc;
	if (0.0f > disc) {
		goto done;
	}
	float s = (float)__builtin_sqrt((double)disc);
	float t1 = (s - qb) / (qa + qa), t2 = (-qb - s) / (qa + qa);
	float t = Float_MinSS(t1, t2);
	if (0.0f > t) {
		if (0.0f > Float_MaxSS(t1, t2)) {
			goto done;
		}
		t = 0.0f;
	} else if (t > 1.0f) {
		goto done;
	}
	if (out) {
		out->x = a->x + dx * t;
		out->y = a->y + dy * t;
		out->z = a->z + dz * t;
	}
	res = 1;
done:
	return res;
}

/* both spheres of radius r at a and b in the box */
static byte SphereBox_Inside(const box_t *box, const vec3_t *a, const vec3_t *b, float r) {
	const vec3_t *p[2] = { a, b };

	for (int i = 0; i < 2; i++) {
		vec3_t lo = { p[i]->x - r, p[i]->y - r, p[i]->z - r };
		vec3_t hi = { p[i]->x + r, p[i]->y + r, p[i]->z + r };
		if (!(lo.x >= box->min.x) || !(box->max.x >= lo.x) || !(lo.y >= box->min.y) || !(box->max.y >= lo.y) ||
			!(lo.z >= box->min.z) || !(box->max.z >= lo.z) || !(hi.x >= box->min.x) || !(box->max.x >= hi.x) ||
			!(hi.y >= box->min.y) || !(box->max.y >= hi.y) || !(hi.z >= box->min.z) || !(box->max.z >= hi.z)) {
			return 0;
		}
	}
	return 1;
}

byte Collision_SphereBox(const vec3_t *a, const vec3_t *b, float r, CSphereBox *cache) {
	if (!cache || !g_CollisionSbox) {
		return 0;
	}
	if (cache->wait) {
		cache->wait--;
		if (g_CollisionSboxShow) {
			Debug_Point(b, 0x800000, r * 0.25f);
		}
		return 0;
	}
	uint16_t flags = cache->flags;
	byte valid = (flags & SPHEREBOX_FREE) && cache->id == g_QuadTriCacheID;
	if (valid && SphereBox_Inside(&cache->box, a, b, r)) {
		cache->flags = flags | SPHEREBOX_INSIDE;
	} else {
		vec3_t m = { (b->x - a->x) * g_CollisionSboxMult + a->x, (b->y - a->y) * g_CollisionSboxMult + a->y,
			(b->z - a->z) * g_CollisionSboxMult + a->z };
		if (valid && !(flags & SPHEREBOX_INSIDE)) {
			cache->wait = 20;
			cache->flags = flags & 0xfffc;
			if (g_CollisionSboxShow) {
				Debug_Point(b, 0xffff00, r);
			}
			return 0;
		}
		box_t box;
		box.min.x = Float_MinSS(a->x, m.x) - r;
		box.min.y = Float_MinSS(a->y, m.y) - r;
		box.min.z = Float_MinSS(a->z, m.z) - r;
		box.max.x = Float_MaxSS(a->x, m.x) + r;
		box.max.y = Float_MaxSS(a->y, m.y) + r;
		box.max.z = Float_MaxSS(a->z, m.z) + r;
		vec3_t zero = { 0.0f, 0.0f, 0.0f };
		byte hit = Collision_Box(&zero, &box.min, &box.max);
		if (g_CollisionSboxShow) {
			Debug_Point(&box.min, hit ? 0x800000 : 0xffff00, -2.0f);
			Debug_Point(&box.max, 0xffff00, 1.0f);
			Debug_Point(&m, 0xff, r);
		}
		cache->flags = (cache->flags & 0xfffc) | (hit == 0);
		cache->wait = hit ? 20 : 0;
		cache->id = g_QuadTriCacheID;
		cache->box = box;
	}
	if (g_CollisionSboxShow) {
		Debug_Point(b, (cache->flags & SPHEREBOX_FREE) ? 0x8000 : 0xff0000, r);
	}
	return cache->flags & SPHEREBOX_FREE;
}

byte Collision_Box(const vec3_t *pos, const vec3_t *min, const vec3_t *max) {
	CCollisionBBox collision;

	CGround *ground = World_Get()->ObjGround;
	CHECK(ground);
	memset(&collision, 0, sizeof(collision));
	collision.use_minmax = 1;
	collision.calc_response = 0;
	collision.pos = *pos;
	collision.min = *min;
	collision.max = *max;
	byte res = CGround_CheckBBox(ground, &collision);
	return res;
}

byte Engine_TestLine(const vec3_t *from, const vec3_t *to, vec3_t *hit, CCollisionTri *tri) {
	vec3_t pos = *to;
	byte res;

	CGround *ground = World_Get()->ObjGround;
	CHECK(ground);
	if (!hit && !tri) {
		res = CGround_TestSeg(ground, from, to, 0);
		return res;
	}
	if (tri) {
		CGroundHit h = { 0, 0, NULL };
		res = CGround_TraceSeg(ground, from, to, &pos, &h, 0);
		if (res) {
			CCollideMesh *mesh = h.quad->Mesh;
			const CCollideTri *t = &mesh->pTri[h.quad->pTri[h.tri]];
			for (int i = 0; i < 3; i++) {
				tri->v[i] = mesh->pVert[t->v[i]];
			}
			tri->plane = t->plane;
			tri->hit = pos;
			tri->noCollisions = t->flags >> 4 & 1;
		}
	} else {
		res = CGround_TraceSegPos(ground, from, to, &pos, 0);
	}
	if (hit) {
		*hit = pos;
	}
	return res;
}

int Collision_Sector(int sector, const vec3_t *a, const vec3_t *b, byte front) {
	if (sector < 0) {
		CheckFailed("Sector >= 0", __FILE__, __TIMESTAMP__, __LINE__);
	}
	int res = Sector_Traverse(sector, a, b, front);
	return res;
}

byte Collision_Check(const vec3_t *pos, float r) {
	byte res;

	if (g_CollisionScts) {
		res = CGround_SweepSphere(World_Ground(), pos, pos, r, NULL, NULL, NULL);
	} else {
		res = CGround_SphereContact(World_Ground(), pos, pos, r, NULL, NULL, NULL);
	}
	return res;
}

/* the queries that do not walk: a sphere at rest is a check (c_schk, with schk), a move inside the box cache is
 * free; 1 with the result in *res */
static byte Collision_Shortcut(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache, byte schk,
	byte *res) {
	if (schk && g_CollisionSchk && a->x == b->x && a->y == b->y && a->z == b->z && !out) {
		*res = Collision_Check(a, r);
		return 1;
	}
	if (cache && Collision_SphereBox(a, b, r, cache)) {
		if (out) {
			*out = *b;
		}
		*res = 0;
		return 1;
	}
	return 0;
}

typedef byte(*sphereFn_t)(CGround *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out,
	vec3_t *hit, vec3_t *n);

/* the slides: steps of half the radius from a to b, each step pushed out of the triangles by fn up to maxIter
 * times; after the first push the last step ends at b */
static byte Collision_SlideSteps(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, int maxIter, sphereFn_t fn,
	const char *name) {
	vec3_t d = { b->x - a->x, b->y - a->y, b->z - a->z };
	byte moved = 0;
	int n = 0;

	CGround *ground = World_Ground();
	vec3_t cur = *b, prev;
	if (ground) {
		cur = *a;
		float stepT = r * 0.5f;
		float len = (float)__builtin_sqrt((double)(d.x * d.x + d.y * d.y + d.z * d.z));
		stepT = stepT / Float_MaxSS(len, stepT);
		float t = stepT;
		for (;;) {
			prev = cur;
			if (t >= 1.0f && !moved) {
				cur = *b;
			} else {
				if (t >= 1.0f) {
					stepT = stepT - (t - 1.0f);
				}
				cur.x = cur.x + d.x * stepT;
				cur.y = cur.y + d.y * stepT;
				cur.z = cur.z + d.z * stepT;
			}
			int k = maxIter;
			while (k > 0 && fn(ground, &prev, &cur, r, &cur, NULL, NULL)) {
				k--;
				n++;
				moved = 1;
			}
			if (k == 0 || n > 100) {
				Con_Verbosef("%s(): partially unresolved collision: %d %f\n", name, n, (double)t);
				if (out) {
					*out = prev;
				}
				moved = 1;
				goto done;
			}
			if (t >= 1.0f) {
				break;
			}
			t = t + stepT;
		}
	}
	if (out) {
		*out = cur;
	}
done:
	return moved;
}

byte Collision_Slide2D(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache) {
	byte res;

	if (Collision_Shortcut(a, b, r, out, cache, 0, &res)) {
		return res;
	}
	return Collision_SlideSteps(a, b, r, out, g_CollisionSld2, CGround_CollideSphere,
		"WorldSphereCollisionSlide2D");
}

byte Collision_Slide(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache) {
	byte res;

	if (Collision_Shortcut(a, b, r, out, cache, 1, &res)) {
		return res;
	}
	return Collision_SlideSteps(a, b, r, out, g_CollisionSldi, CGround_SphereContact,
		"WorldSphereCollisionSlide");
}

byte Engine_TestSphere(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache) {
	vec3_t start = *a, end = *b;
	vec3_t d = { end.x - start.x, end.y - start.y, end.z - start.z };
	byte res = 0;
	int iter = 0;

	if (Collision_Shortcut(a, b, r, out, cache, 1, &res)) {
		return res;
	}
	CGround *ground = World_Ground();
	vec3_t cur = end;
	if (g_CollisionCaps && a->x == b->x && a->z == b->z) {
		if (out) {
			*out = end;
		}
		res = CGround_SweepSphere(ground, &start, &end, r, out, NULL, NULL);
		goto done;
	}
	if (ground) {
		cur = start;
		float stepT = r * 0.5f;
		stepT = stepT + stepT;
		float len = (float)__builtin_sqrt((double)(d.x * d.x + d.y * d.y + d.z * d.z));
		stepT = stepT / Float_MaxSS(len, stepT);
		float t = stepT;
		for (;;) {
			vec3_t prev = cur;
			if (t >= 1.0f) {
				cur = end;
			} else {
				cur.x = cur.x + d.x * stepT;
				cur.y = cur.y + d.y * stepT;
				cur.z = cur.z + d.z * stepT;
			}
			if (g_CollisionIter < 2) {
				iter++;
				if (CGround_SweepSphere(ground, &prev, &cur, r, out ? &cur : NULL, NULL, NULL)) {
					if (out) {
						if (cur.x == a->x && cur.y == a->y && cur.z == a->z) {
							Con_Verbosef("WorldSphereCollisionTest(): initially colliding: %d %f %ph [%s]\n", iter,
								(double)t, (void *)out, Vec3_Str(a, 0, 6));
						}
						*out = cur;
					}
					res = 1;
					goto done;
				}
			}
			if (t >= 1.0f) {
				break;
			}
			t = t + stepT;
		}
	}
	if (g_CollisionHold) {
		cur = *a;
		res = 1;
	}
	if (out) {
		*out = cur;
	}
done:
	return res;
}

cGVar GVar_c_schk;
cGVar GVar_c_sboxmult;
cGVar GVar_c_sbox;
cGVar GVar_c_iter;
cGVar GVar_c_sld2;
cGVar GVar_c_sldi;
cGVar GVar_c_scts;
cGVar GVar_c_sboxshow;
cGVar GVar_c_caps;
cGVar GVar_c_hold;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_c_iter, &GVar_c_iter, "c_iter", 0, "", 0)
INIT_GVAR_INT(Init_GVar_c_hold, &GVar_c_hold, "c_hold", 0, "Freeze WorldSphereCollisionTest calls", 0)
INIT_GVAR_INT(Init_GVar_c_caps, &GVar_c_caps, "c_caps", 0, "Detect vertical WorldSphereCollisionTest cases", 1)
INIT_GVAR_INT(Init_GVar_c_schk, &GVar_c_schk, "c_schk", 0, "Detect WorldSphereCollisionCheck cases", 1)
INIT_GVAR_INT(Init_GVar_c_scts, &GVar_c_scts, "c_scts", 0, "WorldSphereCollisionCheck use: 0=slide, 1=test", 0)
INIT_GVAR_INT(Init_GVar_c_sldi, &GVar_c_sldi, "c_sldi", 0, "WorldSphereCollisionSlide max. iter.", 10)
INIT_GVAR_INT(Init_GVar_c_sld2, &GVar_c_sld2, "c_sld2", 0, "WorldSphereCollisionSlide2D max. iter.", 10)
INIT_GVAR_INT(Init_GVar_c_sbox, &GVar_c_sbox, "c_sbox", 0, "WorldSphereCollision box cache enable", 1)
INIT_GVAR_INT(Init_GVar_c_sboxshow, &GVar_c_sboxshow, "c_sboxshow", 0, "WorldSphereCollision box info", 0)
INIT_GVAR_FLOAT(Init_GVar_c_sboxmult, &GVar_c_sboxmult, "c_sboxmult", 0, "WorldSphereCollision box mult", 20.0f)
