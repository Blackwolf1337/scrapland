/*
 * Geometry helpers, original source Math/MathUtils.cpp (include/scrap/mathlib.h). The float expressions keep the
 * order of the original operations, so the results match it bit for bit; sqrt is the correctly rounded one of the
 * exe (__libm_sse2_sqrt_precise), done in double.
 */
#include "scrap/game.h"

/* x87: dist * dist in the precision the FPU is set to, like the original's fmul st, st */
static long double X87_Square(float x) {
	long double r;

	__asm__("fmul %%st(0), %%st(0)" : "=t"(r) : "0"((long double)x));
	return r;
}

static float Vec3_Dist2To(const vec3_t *p, float x, float y, float z) {
	float dx = p->x - x, dy = p->y - y, dz = p->z - z;
	return dy * dy + dx * dx + dz * dz;
}

/* i: bit 0 max x, bit 1 max y, bit 2 max z */
vec3_t *__thiscall Box_GetCorner(const box_t *self, vec3_t *out, int i) {
	if ((unsigned)i > 7) {
		out->z = 0.0f;
		out->y = 0.0f;
		out->x = 0.0f;
		return out;
	}
	out->x = i & 1 ? self->max.x : self->min.x;
	out->y = i & 2 ? self->max.y : self->min.y;
	out->z = i & 4 ? self->max.z : self->min.z;
	return out;
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

vec3_t *Vec3_Approach(vec3_t *v, const vec3_t *target, float step) {
	Float_Approach(&v->x, target->x, step);
	Float_Approach(&v->y, target->y, step);
	Float_Approach(&v->z, target->z, step);
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

/* q inside the prism of the triangle t0 t1 t2 (on the inner side of its edges) */
byte Tri_HasPoint(const vec3_t *q, const vec3_t *t0, const vec3_t *t1, const vec3_t *t2) {
	float ax = q->x - t0->x, ay = q->y - t0->y, az = q->z - t0->z;
	float bx = t1->x - t0->x, by = t1->y - t0->y, bz = t1->z - t0->z;
	float nx = ay * bz - az * by;
	float ny = az * bx - ax * bz;
	float nz = ax * by - ay * bx;
	float cx = q->x - t1->x, cy = q->y - t1->y, cz = q->z - t1->z;
	float px = t2->x - t1->x, py = t2->y - t1->y, pz = t2->z - t1->z;

	float s1 = (cz * px - cx * pz) * ny + (cy * pz - cz * py) * nx + (cx * py - cy * px) * nz;
	if (0.0f > s1) {
		return 0;
	}
	float ex = q->x - t2->x, ey = q->y - t2->y, ez = q->z - t2->z;
	float fx = t0->x - t2->x, fy = t0->y - t2->y, fz = t0->z - t2->z;
	float s2 = (ez * fx - ex * fz) * ny + (ey * fz - ez * fy) * nx + (ex * fy - ey * fx) * nz;
	return !(0.0f > s2);
}

/* _fpclass: a normal number or a zero, no NaN, infinity or denormal */
byte Float_IsValid(float f) {
	return (crt_fpclass(f) & ~(FPCLASS_NN | FPCLASS_NZ | FPCLASS_PZ | FPCLASS_PN)) == 0;
}

byte Vec3_IsValid(const vec3_t *v) {
	return Float_IsValid(v->x) && Float_IsValid(v->y) && Float_IsValid(v->z);
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

/* the plane through v1 v2 v3; 0 for a degenerate triangle, the normal is then zero */
byte __thiscall Plane_SafeSet(plane_t *self, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3) {
	float ax = v3->x - v1->x, ay = v3->y - v1->y, az = v3->z - v1->z;
	float bx = v2->x - v1->x, by = v2->y - v1->y, bz = v2->z - v1->z;
	float nx = by * az - bz * ay;
	float ny = bz * ax - bx * az;
	float nz = bx * ay - by * ax;

	self->n.x = nx;
	self->n.y = ny;
	self->n.z = nz;
	if (nx == 0.0f && ny == 0.0f && nz == 0.0f) {
		return 0;
	}
	float len = (float)__builtin_sqrt((double)(ny * ny + nx * nx + nz * nz));
	if (len != 0.0f) {
		float inv = 1.0f / len;
		nx = nx * inv;
		ny = ny * inv;
		nz = nz * inv;
	}
	self->n.y = ny;
	self->n.x = nx;
	self->n.z = nz;
	self->d = -(v1->y * ny + v1->x * nx + v1->z * nz);
	return 1;
}

/* a t^2 + b t + c = 0, t1 <= t2; 0 without a real solution */
byte Quad_Solve(float a, float b, float c, float *t1, float *t2) {
	double disc = (double)(b * b - a * 4.0f * c);
	if (0.0 > disc) {
		return 0;
	}
	double s = __builtin_sqrt(disc);
	double a2 = (double)(a + a);
	*t1 = (float)((s - (double)b) / a2);
	*t2 = (float)(((double)-b - s) / a2);
	float f1 = *t1;
	if (f1 > *t2) {
		*t1 = *t2;
		*t2 = f1;
	}
	return 1;
}

float Dist_PointTriangle(const vec3_t *p, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3, vec3_t *out) {
	return (float)__builtin_sqrt((double)Dist2_PointTriangle(p, v1, v2, v3, out));
}

/* in the FPU's precision like the original, the caller may use more than a float of it */
long double Dist2_PointTriangle(const vec3_t *p, const vec3_t *v1, const vec3_t *v2, const vec3_t *v3, vec3_t *out) {
	plane_t plane;
	float d[3];
	vec3_t c[3];

	byte ok = Plane_SafeSet(&plane, v1, v2, v3);
	CHECK(ok);
	float dist = p->x * plane.n.x + p->y * plane.n.y + p->z * plane.n.z + plane.d;
	vec3_t q = { p->x - plane.n.x * dist, p->y - plane.n.y * dist, p->z - plane.n.z * dist };
	if (Tri_HasPoint(&q, v1, v2, v3)) {
		if (out) {
			*out = q;
		}
		return X87_Square(dist);
	}
	d[0] = Dist2_PointSegment(v1, v2, p, &c[0]);
	d[1] = Dist2_PointSegment(v2, v3, p, &c[1]);
	d[2] = Dist2_PointSegment(v3, v1, p, &c[2]);
	int i = d[1] > d[0] ? 0 : 1;
	int best = d[2] > d[i] ? i : 2;
	if (out) {
		*out = c[best];
	}
	return d[best];
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
	out->y = (float)Game_Atan2((double)dir->x, (double)dir->z);
	out->x = (float)Game_Atan2((double)-dir->y, __builtin_sqrt((double)(dir->z * dir->z + dir->x * dir->x)));
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

	X87_SinCos(-rot->x, &s0, &c0);
	X87_SinCos(-rot->y, &s1, &c1);
	X87_SinCos(-rot->z, &s2, &c2);
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
	ang = ang - (float)(crt_floor((double)ang / M_2PI_D) * M_2PI_D);
	if ((double)ang > M_PI_D) {
		ang = ang - M_2PI_F;
	}
	return ang;
}

float *Vec2_Rotate(float *out, const float *in, float ang) {
	float s, c;

	X87_SinCos(ang, &s, &c);
	out[0] = in[0] * c - in[1] * s;
	out[1] = in[1] * c + in[0] * s;
	return out;
}

/* around z, then x, then y */
vec3_t *Vec3_Rotate(vec3_t *out, const vec3_t *in, const vec3_t *rot) {
	float s0, c0, s1, c1, s2, c2;

	X87_SinCos(rot->x, &s0, &c0);
	X87_SinCos(rot->y, &s1, &c1);
	X87_SinCos(rot->z, &s2, &c2);
	float a = c2 * in->y + s2 * in->x;
	float b = c2 * in->x - s2 * in->y;
	float d = c0 * in->z + s0 * a;
	out->y = c0 * a - s0 * in->z;
	out->x = c1 * b + s1 * d;
	out->z = c1 * d - s1 * b;
	return out;
}

/* the 3x3 rotation (row major, m * from = to) that turns the unit vector from into the unit vector to (Moller and
 * Hughes): the identity for nearly equal vectors, a half turn around an axis normal to from for nearly opposite ones */
float *Matrix3_RotFromTo(const vec3_t *from, const vec3_t *to, float *m) {
	float a0 = from->x, a1 = from->y, a2 = from->z;
	float b0 = to->x, b1 = to->y, b2 = to->z;
	float v0 = b2 * a1 - b1 * a2;
	float v1 = b0 * a2 - a0 * b2;
	float v2 = a0 * b1 - b0 * a1;
	float e = a0 * b0 + b1 * a1 + a2 * b2;

	if ((double)e > 0.999999) {
		m[0] = 1.0f;
		m[1] = 0.0f;
		m[2] = 0.0f;
		m[3] = 0.0f;
		m[4] = 1.0f;
		m[5] = 0.0f;
		m[6] = 0.0f;
		m[7] = 0.0f;
		m[8] = 1.0f;
		return m;
	}
	if (-0.999999 > (double)e) {
		/* the axis: (0, z, -y) of from, or (-z, 0, x) when that is too short */
		float px = 0.0f, py = a2, pz = -a1;

		if (1e-06 > (double)(a2 * a2 + 0.0f + pz * pz)) {
			px = -a2;
			py = 0.0f;
			pz = a0;
		}
		float k = (float)(1.0 / __builtin_sqrt((double)(py * py + px * px + pz * pz)));
		float q0 = px * k, q1 = py * k, q2 = pz * k;
		float c0 = a2 * q1 - a1 * q2;
		float c1 = a0 * q2 - a2 * q0;
		float c2 = a1 * q0 - a0 * q1;
		float m01 = c1 * c0 + -(a0 * a1) + -(q1 * q0);
		float m02 = c2 * c0 + -(a0 * a2) + -(q2 * q0);
		float m12 = c2 * c1 + -(a2 * a1) + -(q2 * q1);

		m[0] = -(a0 * a0) + c0 * c0 + -(q0 * q0);
		m[1] = m01;
		m[3] = m01;
		m[2] = m02;
		m[6] = m02;
		m[4] = c1 * c1 + -(a1 * a1) + -(q1 * q1);
		m[5] = m12;
		m[7] = m12;
		m[8] = c2 * c2 + -(a2 * a2) + -(q2 * q2);
		return m;
	}
	float h = (float)((1.0 - (double)e) / (double)(v1 * v1 + v0 * v0 + v2 * v2));
	float hv0 = h * v0, hv2 = v2 * h;
	float p01 = v1 * hv0, p02 = v2 * hv0, p12 = v1 * hv2;

	m[0] = hv0 * v0 + e;
	m[1] = p01 - v2;
	m[2] = v1 + p02;
	m[3] = v2 + p01;
	m[6] = p02 - v1;
	m[8] = v2 * hv2 + e;
	m[4] = v1 * h * v1 + e;
	m[5] = p12 - v0;
	m[7] = p12 + v0;
	return m;
}

/* around x by ax, then around y by ay */
vec3_t *Vec3_RotateXY(vec3_t *out, const vec3_t *in, float ax, float ay) {
	float s0, c0, s1, c1;

	X87_SinCos(ax, &s0, &c0);
	X87_SinCos(ay, &s1, &c1);
	float a = c0 * in->z + s0 * in->y;
	out->y = c0 * in->y - s0 * in->z;
	out->z = c1 * a - s1 * in->x;
	out->x = s1 * a + c1 * in->x;
	return out;
}
