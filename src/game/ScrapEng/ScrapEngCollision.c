/*
 * ScrapEngCollision.cpp of the game: the collision queries against the floor, see include/scrap/scrapcollision.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/mundo.h"
#include "scrap/prof.h"
#include "scrap/scrapcollision.h"
#include "scrap/sueloportal.h"

void __fastcall ProfScope_End(const int *section) {
	Prof_End(*section);
}

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

	Prof_Begin(PROF_SEGSPHERE);
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
	Prof_End(PROF_SEGSPHERE);
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
			Debug_Punto(b, 0x800000, r * 0.25f);
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
				Debug_Punto(b, 0xffff00, r);
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
			Debug_Punto(&box.min, hit ? 0x800000 : 0xffff00, -2.0f);
			Debug_Punto(&box.max, 0xffff00, 1.0f);
			Debug_Punto(&m, 0xff, r);
		}
		cache->flags = (cache->flags & 0xfffc) | (hit == 0);
		cache->wait = hit ? 20 : 0;
		cache->id = g_QuadTriCacheID;
		cache->box = box;
	}
	if (g_CollisionSboxShow) {
		Debug_Punto(b, (cache->flags & SPHEREBOX_FREE) ? 0x8000 : 0xff0000, r);
	}
	return cache->flags & SPHEREBOX_FREE;
}

byte Collision_Box(const vec3_t *pos, const vec3_t *min, const vec3_t *max) {
	CColisBBox colis;

	Prof_Begin(PROF_BOX);
	CSuelo *suelo = Mundo_Get()->ObjSuelo;
	CHECK(suelo);
	memset(&colis, 0, sizeof(colis));
	colis.usar_minmax = 1;
	colis.calc_respuesta = 0;
	colis.pos = *pos;
	colis.min = *min;
	colis.max = *max;
	byte res = CSuelo_CheckBBox(suelo, &colis);
	Prof_End(PROF_BOX);
	return res;
}

byte Engine_TestLine(const vec3_t *from, const vec3_t *to, vec3_t *hit, CColisTri *tri) {
	vec3_t pos = *to;
	byte res;

	Prof_Begin(PROF_TESTLINE);
	CSuelo *suelo = Mundo_Get()->ObjSuelo;
	CHECK(suelo);
	if (!hit && !tri) {
		res = CSuelo_TestSeg(suelo, from, to, 0);
		Prof_End(PROF_TESTLINE);
		return res;
	}
	if (tri) {
		CSueloHit h = { 0, 0, NULL };
		res = CSuelo_TraceSeg(suelo, from, to, &pos, &h, 0);
		if (res) {
			CCollideMesh *mesh = h.quad->Mesh;
			const CCollideTri *t = &mesh->pTri[h.quad->pTri[h.tri]];
			for (int i = 0; i < 3; i++) {
				tri->v[i] = mesh->pVert[t->v[i]];
			}
			tri->plane = t->plane;
			tri->hit = pos;
			tri->noColisiones = t->flags >> 4 & 1;
		}
	} else {
		res = CSuelo_TraceSegPos(suelo, from, to, &pos, 0);
	}
	if (hit) {
		*hit = pos;
	}
	Prof_End(PROF_TESTLINE);
	return res;
}

int Collision_Sector(int sector, const vec3_t *a, const vec3_t *b, byte front) {
	if (sector < 0) {
		CheckFailed("Sector >= 0", __FILE__, __TIMESTAMP__, __LINE__);
	}
	Prof_Begin(PROF_SECTOR);
	int res = Sector_Recorrer(sector, a, b, front);
	Prof_End(PROF_SECTOR);
	return res;
}

byte Collision_Check(const vec3_t *pos, float r) {
	byte res;

	Prof_Begin(PROF_CHECK);
	if (g_CollisionScts) {
		res = CSuelo_SweepSphere(Mundo_Suelo(), pos, pos, r, NULL, NULL, NULL);
	} else {
		res = CSuelo_SphereContact(Mundo_Suelo(), pos, pos, r, NULL, NULL, NULL);
	}
	Prof_End(PROF_CHECK);
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

typedef byte(__thiscall *sphereFn_t)(CSuelo *self, const vec3_t *a, const vec3_t *b, float r, vec3_t *out,
	vec3_t *hit, vec3_t *n);

/* the slides: steps of half the radius from a to b, each step pushed out of the triangles by fn up to maxIter
 * times; after the first push the last step ends at b */
static byte Collision_SlideSteps(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, int section, int maxIter,
	sphereFn_t fn, const char *name) {
	vec3_t d = { b->x - a->x, b->y - a->y, b->z - a->z };
	byte moved = 0;
	int n = 0;

	Prof_Begin(section);
	CSuelo *suelo = Mundo_Suelo();
	vec3_t cur = *b, prev;
	if (suelo) {
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
			while (k > 0 && fn(suelo, &prev, &cur, r, &cur, NULL, NULL)) {
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
	Prof_End(section);
	return moved;
}

byte Collision_Slide2D(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache) {
	byte res;

	if (Collision_Shortcut(a, b, r, out, cache, 0, &res)) {
		return res;
	}
	return Collision_SlideSteps(a, b, r, out, PROF_SLIDE2D, g_CollisionSld2, CSuelo_CollideSphere,
		"WorldSphereCollisionSlide2D");
}

byte Collision_Slide(const vec3_t *a, const vec3_t *b, float r, vec3_t *out, CSphereBox *cache) {
	byte res;

	if (Collision_Shortcut(a, b, r, out, cache, 1, &res)) {
		return res;
	}
	return Collision_SlideSteps(a, b, r, out, PROF_SLIDE, g_CollisionSldi, CSuelo_SphereContact,
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
	Prof_Begin(PROF_TESTSPHERE);
	CSuelo *suelo = Mundo_Suelo();
	vec3_t cur = end;
	if (g_CollisionCaps && a->x == b->x && a->z == b->z) {
		if (out) {
			*out = end;
		}
		res = CSuelo_SweepSphere(suelo, &start, &end, r, out, NULL, NULL);
		goto done;
	}
	if (suelo) {
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
				if (CSuelo_SweepSphere(suelo, &prev, &cur, r, out ? &cur : NULL, NULL, NULL)) {
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
	Prof_End(PROF_TESTSPHERE);
	return res;
}
