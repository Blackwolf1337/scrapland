/*
 * Quaternions (x, y, z, w) of the engine, next to Math/Matrix.cpp in the exe (include/scrap/mathlib.h). The engine3d
 * has its own copies in Engine/cuaternio.cpp, some identical (the same C function), and a few more.
 */
#include "scrap/d3d8.h"
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

	Float_SinCos(ang * 0.5f, &s, &c);
	out->x = axis->x * s;
	out->y = axis->y * s;
	out->z = axis->z * s;
	out->w = c;
	return out;
}

float *Quat_ToAxisAngle(vec3_t *axis, float *ang, const quat_t *q) {
	double a = acos((double)q->w);
	double k = 0.0;

	if (a > 1e-20f) {
		k = 1.0 / sin(a);
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
/* a * k0 + c * k1 with the weights of the angle acos(d), linear when far (1 - d) is at most 0.001 */
static void Quat_Interp(quat_t *out, const quat_t *a, const quat_t *c, double d, double far, float t) {
	double k0, k1;

	if (far > 0.001) {
		double om = acos(d);
		double so = sin(om);
		k0 = sin((1.0 - (double)t) * om) / so;
		k1 = sin((double)t * om) / so;
	} else {
		k0 = 1.0 - (double)t;
		k1 = (double)t;
	}
	out->x = (float)((double)a->x * k0 + (double)c->x * k1);
	out->y = (float)((double)c->y * k1 + (double)a->y * k0);
	out->z = (float)((double)a->z * k0 + (double)c->z * k1);
	out->w = (float)((double)a->w * k0 + (double)c->w * k1);
}

static void Quat_Neg(quat_t *q) {
	q->x = -q->x;
	q->y = -q->y;
	q->z = -q->z;
	q->w = -q->w;
}

/* the dot product of a and b, and b turned to the side of a */
static double Quat_Side(const quat_t *a, quat_t *b) {
	double d = (double)(b->y * a->y + b->x * a->x + b->z * a->z + b->w * a->w);

	if (0.0 > d) {
		d = -d;
		Quat_Neg(b);
	}
	return d;
}

quat_t *Quat_Slerp(quat_t *out, const quat_t *a, const quat_t *b, float t) {
	quat_t c = *b;
	double d = Quat_Side(a, &c);

	Quat_Interp(out, a, &c, d, 1.0 - d, t);
	return out;
}

/* the rotation angle of w in degrees */
static double Quat_Degrees(float w) {
	return acos(__builtin_fabs((double)w)) * 180.0 / M_PI_D;
}

byte Quat_SlerpFlip(quat_t *out, const quat_t *a, const quat_t *b, float t) {
	quat_t c = *b;
	double d = Quat_Side(a, &c);
	byte flipped = 0;

	if (0.0f > a->w * c.w) {
		double da = Quat_Degrees(a->w);
		double dc = Quat_Degrees(c.w);
		if ((float)(dc + dc) + (float)(da + da) > 180.0f) {
			double dd = acos(d) * 180.0 / M_PI_D;
			if ((float)(dd + dd) > 80.0f) {
				Quat_Neg(&c);
				d = -d;
				flipped = 1;
			}
		}
	}
	Quat_Interp(out, a, &c, d, 1.0 - __builtin_fabs(d), t);
	return flipped;
}

quat_t *Quat_FromEuler(quat_t *out, float x, float y, float z) {
	float sx, cx, sy, cy, sz, cz;

	Float_SinCos(x * 0.5f, &sx, &cx);
	Float_SinCos(y * 0.5f, &sy, &cy);
	Float_SinCos(z * 0.5f, &sz, &cz);
	float a = sz * cx, b = cz * sx, c = cz * cx, e = sz * sx;
	out->x = b * cy - a * sy;
	out->z = a * cy + b * sy;
	out->y = c * sy + e * cy;
	out->w = c * cy - e * sy;
	return out;
}

/* |a . b| up to 1 */
float Quat_DotAbs(const quat_t *a, const quat_t *b) {
	float d = (float)__builtin_fabs((double)(a->y * b->y + a->x * b->x + a->z * b->z + a->w * b->w));

	CHECK((double)d <= 1.00001);
	return 1.0f > d ? d : 1.0f;
}

/* the matrix of the inverse rotation */
matrix_t *Quat_ToMatrixD3D(matrix_t *out, const quat_t *q) {
	quat_t c = { q->x, q->y, q->z, -q->w };

	return D3DX_MatrixRotationQuaternion(out, &c);
}
