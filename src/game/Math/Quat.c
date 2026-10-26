/*
 * Quaternions (x, y, z, w) of the engine, next to Math/Matrix.cpp in the exe (include/scrap/mathlib.h).
 */
#include "scrap/game.h"

quat_t *Quat_Mul(quat_t *out, const quat_t *a, const quat_t *b) {
	out->x = a->x * b->w + b->z * a->y - b->y * a->z + b->x * a->w;
	out->y = b->w * a->y - a->x * b->z + b->x * a->z + a->w * b->y;
	out->z = b->y * a->x - b->x * a->y + a->z * b->w + a->w * b->z;
	out->w = a->w * b->w - (b->x * a->x + b->y * a->y + a->z * b->z);
	return out;
}

/* ang in radians */
quat_t *Quat_FromAxisAngle(quat_t *out, const vec3_t *axis, float ang) {
	float s, c;

	X87_SinCos(ang * 0.5f, &s, &c);
	out->x = axis->x * s;
	out->y = axis->y * s;
	out->z = axis->z * s;
	out->w = c;
	return out;
}

float *Quat_ToAxisAngle(vec3_t *axis, float *ang, const quat_t *q) {
	double a = Game_Acos((double)q->w);
	double k = 0.0;

	if (a > 1e-20f) {
		k = 1.0 / Game_Sin(a);
	}
	axis->x = (float)((double)q->x * k);
	axis->y = (float)((double)q->y * k);
	axis->z = (float)((double)q->z * k);
	*ang = (float)(a * 2.0);
	return ang;
}

/* the rotation matrix of a unit quaternion */
matrix_t *Quat_ToMatrix(matrix_t *out, const quat_t *q) {
	float x = q->x, y = q->y, z = q->z, w = q->w;
	float xx = x * x, zx = z * x, yy = y * y, wx = w * x, zz = z * z, yx = y * x, zy = z * y, wy = w * y;
	float wz = w * z;
	float *m = out->m;
	float t;

	m[11] = 0.0f;
	m[7] = 0.0f;
	m[3] = 0.0f;
	m[14] = 0.0f;
	m[13] = 0.0f;
	m[12] = 0.0f;
	m[15] = 1.0f;
	t = zz + yy;
	m[0] = 1.0f - (t + t);
	t = yx - wz;
	m[1] = t + t;
	t = wz + yx;
	m[4] = t + t;
	t = wy + zx;
	m[2] = t + t;
	t = zx - wy;
	m[8] = t + t;
	t = zz + xx;
	m[5] = 1.0f - (t + t);
	t = zy - wx;
	m[6] = t + t;
	t = wx + zy;
	m[9] = t + t;
	t = yy + xx;
	m[10] = 1.0f - (t + t);
	return out;
}

/* spherical interpolation from a (t = 0) to b (t = 1) on the shorter way, in double */
quat_t *Quat_Slerp(quat_t *out, const quat_t *a, const quat_t *b, float t) {
	quat_t c = *b;
	double d = (double)(c.y * a->y + c.x * a->x + c.z * a->z + c.w * a->w);
	double k0, k1;

	if (0.0 > d) {
		d = -d;
		c.x = -b->x;
		c.y = -b->y;
		c.z = -b->z;
		c.w = -b->w;
	}
	if (1.0 - d > 0.001) {
		double om = Game_Acos(d);
		double so = Game_Sin(om);
		k0 = Game_Sin((1.0 - (double)t) * om) / so;
		k1 = Game_Sin((double)t * om) / so;
	} else {
		k0 = 1.0 - (double)t;
		k1 = (double)t;
	}
	out->x = (float)((double)a->x * k0 + (double)c.x * k1);
	out->y = (float)((double)c.y * k1 + (double)a->y * k0);
	out->z = (float)((double)a->z * k0 + (double)c.z * k1);
	out->w = (float)((double)a->w * k0 + (double)c.w * k1);
	return out;
}
