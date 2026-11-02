/*
 * Geometry helpers, original source Math/MathUtils.cpp (include/scrap/mathlib.h). The float expressions keep the
 * order of the original operations, so the results match it bit for bit; sqrt is the correctly rounded one of the
 * exe (__libm_sse2_sqrt_precise), done in double.
 */
#include "scrap/game.h"

static float Vec3_Dist2To(const vec3_t *p, float x, float y, float z) {
	float dx = p->x - x, dy = p->y - y, dz = p->z - z;
	return dy * dy + dx * dx + dz * dz;
}

void Box_Merge(box_t *box, const box_t *other, byte first) {
	if (!box) {
		return;
	}
	if (first) {
		*box = *other;
		return;
	}
	box->min.x = box->min.x < other->min.x ? box->min.x : other->min.x;
	box->min.y = box->min.y < other->min.y ? box->min.y : other->min.y;
	box->min.z = box->min.z < other->min.z ? box->min.z : other->min.z;
	box->max.x = box->max.x > other->max.x ? box->max.x : other->max.x;
	box->max.y = box->max.y > other->max.y ? box->max.y : other->max.y;
	box->max.z = box->max.z > other->max.z ? box->max.z : other->max.z;
}

/* *v at most step nearer to target */
float *Float_Approach(float *v, float target, float step) {
	float down = *v - step;
	if (down > target) {
		*v = down;
		return v;
	}
	float up = *v + step;
	*v = up < target ? up : target;
	return v;
}

/* the velocities of two masses after an elastic hit along the normal n */
vec3_t *Vec3_Bounce(const vec3_t *n, float m1, float m2, vec3_t *v1, vec3_t *v2) {
	float nx = n->x, ny = n->y, nz = n->z;
	float v1x = v1->x, v1y = v1->y, v1z = v1->z;
	float u1 = v1x * nx + v1y * ny + v1z * nz;
	float u2 = v2->y * ny + nx * v2->x + v2->z * nz;
	float m = m1 + m2;

	float d1 = ((u2 + u2 - u1) * m2 + u1 * m1) / m - u1;
	v1->x = v1x + nx * d1;
	v1->y = v1y + ny * d1;
	v1->z = v1z + nz * d1;
	float d2 = ((u1 + u1 - u2) * m1 + u2 * m2) / m - u2;
	v2->x = v2->x + n->x * d2;
	v2->y = v2->y + d2 * n->y;
	v2->z = v2->z + n->z * d2;
	return v1;
}

/* distance of p to the segment a b, the nearest point into out */
float Dist_PointSegment(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out) {
	float ax = a->x, ay = a->y, az = a->z;
	float bx = b->x;
	float dx = bx - ax, dy = b->y - ay, dz = b->z - az;
	float len = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
	vec3_t dir = { dx, dy, dz };

	if (len != 0.0f) {
		float inv = 1.0f / len;
		dir.x = inv * dx;
		dir.y = inv * dy;
		dir.z = dz * inv;
	}
	float pd = -(p->y * dir.y + p->x * dir.x + p->z * dir.z);
	float t = -(ay * dir.y + ax * dir.x + az * dir.z + pd);
	if (0.0f >= t) {
		if (out) {
			out->x = ax;
			out->y = a->y;
			out->z = a->z;
		}
		return (float)__builtin_sqrt((double)Vec3_Dist2To(p, a->x, a->y, a->z));
	}
	if (t * t > dy * dy + dx * dx + dz * dz) {
		if (out) {
			out->x = bx;
			out->y = b->y;
			out->z = b->z;
		}
		return (float)__builtin_sqrt((double)Vec3_Dist2To(p, b->x, b->y, b->z));
	}
	float cx = ax + dir.x * t, cy = ay + dir.y * t, cz = az + dir.z * t;
	if (out) {
		out->x = cx;
		out->y = cy;
		out->z = cz;
	}
	return (float)__builtin_sqrt((double)Vec3_Dist2To(p, cx, cy, cz));
}

float Dist2_PointSegment(const vec3_t *a, const vec3_t *b, const vec3_t *p, vec3_t *out) {
	float ax = a->x, ay = a->y;
	float bx = b->x;
	float dx = bx - ax, dy = b->y - ay, dz = b->z - a->z;
	float len2 = dy * dy + dx * dx + dz * dz;

	if (len2 == 0.0f) {
		if (out) {
			out->x = ax;
			out->y = a->y;
			out->z = a->z;
		}
		return Vec3_Dist2To(p, a->x, a->y, a->z);
	}
	float pd = -(p->y * dy + p->x * dx + p->z * dz);
	float t = -(ay * dy + ax * dx + a->z * dz + pd);
	if (0.0f >= t) {
		if (out) {
			out->x = ax;
			out->y = a->y;
			out->z = a->z;
		}
		return Vec3_Dist2To(p, a->x, a->y, a->z);
	}
	if (t > len2) {
		if (out) {
			out->x = bx;
			out->y = b->y;
			out->z = b->z;
		}
		return Vec3_Dist2To(p, b->x, b->y, b->z);
	}
	float inv = 1.0f / len2;
	float cx = dx * t * inv + ax, cy = dy * t * inv + a->y, cz = dz * t * inv + a->z;
	if (out) {
		out->x = cx;
		out->y = cy;
		out->z = cz;
	}
	return Vec3_Dist2To(p, cx, cy, cz);
}

/* v mirrored at the normal n */
vec3_t *Vec3_Reflect(const vec3_t *n, const vec3_t *v, vec3_t *out) {
	float nx = n->x, ny = n->y, nz = n->z;
	float dot = v->x * nx + v->y * ny + v->z * nz;
	dot = dot + dot;
	out->x = nx * dot - v->x;
	out->y = ny * dot - v->y;
	out->z = nz * dot - v->z;
	return out;
}

/* angles in radians: x around the x axis, y around y, z around z */

/* the angles (x, y, z = 0) that turn the z axis into dir */
vec3_t *Vec3_GetRotAng(vec3_t *out, const vec3_t *dir) {
	out->z = 0.0f;
	out->y = (float)atan2((double)dir->x, (double)dir->z);
	out->x = (float)atan2((double)-dir->y, __builtin_sqrt((double)(dir->z * dir->z + dir->x * dir->x)));
	return out;
}

/* Vec3_Rotate with the cosines and sines of the angles given */
vec3_t *Vec3_RotateSC(vec3_t *out, const vec3_t *in, const vec3_t *c, const vec3_t *s) {
	float a = c->y * in->x + s->y * in->z;
	float b = c->y * in->z - s->y * in->x;
	float d = c->x * in->y - s->x * b;

	out->z = s->x * in->y + c->x * b;
	out->x = c->z * a - s->z * d;
	out->y = s->z * a + c->z * d;
	return out;
}

/* the inverse of Vec3_Rotate: by -rot, y first */
vec3_t *Vec3_RotateInv(vec3_t *out, const vec3_t *in, const vec3_t *rot) {
	float s0, c0, s1, c1, s2, c2;

	Float_SinCos(-rot->x, &s0, &c0);
	Float_SinCos(-rot->y, &s1, &c1);
	Float_SinCos(-rot->z, &s2, &c2);
	float t1 = c1 * in->z - s1 * in->x;
	float t2 = s1 * in->z + c1 * in->x;
	float t3 = c0 * in->y - s0 * t1;
	out->z = c0 * t1 + s0 * in->y;
	out->x = c2 * t2 - s2 * t3;
	out->y = c2 * t3 + s2 * t2;
	return out;
}

vec3_t *Vec3_NormAng(vec3_t *out, const vec3_t *in) {
	float z = NormAng(in->z);
	float y = NormAng(in->y);
	float x = NormAng(in->x);

	out->y = y;
	out->x = x;
	out->z = z;
	return out;
}

/* ang folded into (-pi, pi] */
float NormAng(float ang) {
	ang = ang - (float)(floor((double)ang / M_2PI_D) * M_2PI_D);
	if ((double)ang > M_PI_D) {
		ang = ang - M_2PI_F;
	}
	return ang;
}

float *Vec2_Rotate(float *out, const float *in, float ang) {
	float s, c;

	Float_SinCos(ang, &s, &c);
	out[0] = in[0] * c - in[1] * s;
	out[1] = in[1] * c + in[0] * s;
	return out;
}

/* around z, then x, then y */
vec3_t *Vec3_Rotate(vec3_t *out, const vec3_t *in, const vec3_t *rot) {
	float s0, c0, s1, c1, s2, c2;

	Float_SinCos(rot->x, &s0, &c0);
	Float_SinCos(rot->y, &s1, &c1);
	Float_SinCos(rot->z, &s2, &c2);
	float a = c2 * in->y + s2 * in->x;
	float b = c2 * in->x - s2 * in->y;
	float d = c0 * in->z + s0 * a;
	out->y = c0 * a - s0 * in->z;
	out->x = c1 * b + s1 * d;
	out->z = c1 * d - s1 * b;
	return out;
}

/* around x by ax, then around y by ay */
vec3_t *Vec3_RotateXY(vec3_t *out, const vec3_t *in, float ax, float ay) {
	float s0, c0, s1, c1;

	Float_SinCos(ax, &s0, &c0);
	Float_SinCos(ay, &s1, &c1);
	float a = c0 * in->z + s0 * in->y;
	out->y = c0 * in->y - s0 * in->z;
	out->z = c1 * a - s1 * in->x;
	out->x = s1 * a + c1 * in->x;
	return out;
}
