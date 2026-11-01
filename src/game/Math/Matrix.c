/*
 * 4x4 matrices of the engine, original source Math/Matrix.cpp (include/scrap/mathlib.h). Row vectors: a point is
 * v * M, the translation is in m[12..14]. sin and cos are the ones of the exe's runtime (src/core/libm.c).
 */
#include <string.h>

#include "scrap/game.h"

void Matrix_Identity(matrix_t *self) {
	memset(self->m, 0, sizeof(self->m));
	self->m[0] = 1.0f;
	self->m[5] = 1.0f;
	self->m[10] = 1.0f;
	self->m[15] = 1.0f;
}

matrix_t *Matrix_Copy(matrix_t *self, const matrix_t *src) {
	*self = *src;
	return self;
}

matrix_t *Matrix_FromFloats(matrix_t *self, const float *f) {
	CHECK(f);
	memcpy(self->m, f, sizeof(self->m));
	return self;
}

/* out = self * other */
matrix_t *Matrix_Mul(const matrix_t *self, matrix_t *out, const matrix_t *other) {
	const matrix_t a = *self, b = *other;

	for (int r = 0; r < 4; r++) {
		const float *row = a.m + r * 4;
		for (int c = 0; c < 4; c++) {
			out->m[r * 4 + c] = row[0] * b.m[c] + row[1] * b.m[4 + c] + row[2] * b.m[8 + c] + row[3] * b.m[12 + c];
		}
	}
	return out;
}

/* the z angle of Matrix_GetEuler and Matrix_ToEuler outside the poles (h: the length of the third row in xz) */
static float Euler_Z(const matrix_t *self, float h) {
	float m8 = self->m[8], m9 = self->m[9], m10 = self->m[10];
	float a = -1.0f / h * m8;
	float b = 1.0f / h * m10;
	float inv = 1.0f / (float)__builtin_sqrt((double)(m9 * m9 + h * h));
	float m0 = self->m[0], m2 = self->m[2];
	float y = inv * h * self->m[1] - (m2 * b - m0 * a) * (inv * m9);
	float x = m0 * b + m2 * a;

	return Game_Atan2F((double)y, (double)x);
}

/* the angles of Matrix_Rotate from the rotation part */
vec3_t *Matrix_GetEuler(const matrix_t *self, vec3_t *out) {
	float m8 = self->m[8], m9 = self->m[9], m10 = self->m[10];
	float h = (float)__builtin_sqrt((double)(m10 * m10 + m8 * m8));

	out->z = 0.0f;
	out->y = (float)Game_Atan2((double)m8, (double)m10);
	out->x = (float)Game_Atan2((double)-m9, (double)h);
	if (1e-4f > __builtin_fabs((double)h)) {
		/* looking straight up or down: z from the first row turned back by the other angles */
		vec3_t v = { self->m[0], self->m[1], self->m[2] };
		vec3_t r;
		Vec3_RotateInv(&r, &v, out);
		out->z = (float)Game_Atan2((double)r.y, (double)r.x);
		return out;
	}
	out->z = Euler_Z(self, h);
	return out;
}

/* the same in the engine3d (matriz.h, rva 0x2392d0): at the poles the first row turned back by x and y inline */
vec3_t *Matrix_ToEuler(vec3_t *out, const matrix_t *m) {
	float m8 = m->m[8], m9 = m->m[9], m10 = m->m[10];
	float h = (float)__builtin_sqrt((double)(m10 * m10 + m8 * m8));
	float x, y;

	out->z = 0.0f;
	y = Game_Atan2F((double)m8, (double)m10);
	out->y = y;
	x = Game_Atan2F((double)-m9, (double)h);
	out->x = x;
	if (1e-4f > __builtin_fabs((double)h)) {
		float m0 = m->m[0], m1 = m->m[1], m2 = m->m[2];
		float s0, c0, s1, c1;

		X87_SinCos(-x, &s0, &c0);
		X87_SinCos(-y, &s1, &c1);
		out->z = Game_Atan2F((double)(m1 * c0 - s0 * (m2 * c1 - s1 * m0)), (double)(m2 * s1 + m0 * c1));
		return out;
	}
	out->z = Euler_Z(m, h);
	return out;
}

/* the rotation angles of the z axis (third row) */
vec3_t *Matrix_GetRotAng(const matrix_t *self, vec3_t *out) {
	vec3_t dir = { self->m[8], self->m[9], self->m[10] };

	Vec3_GetRotAng(out, &dir);
	return out;
}

/* the inverse of a rotation and translation; 0 if the last column is not 0 0 0 1 */
byte Matrix_InvertAffine(matrix_t *out, const matrix_t *m) {
	const float *a = m->m;
	float *o = out->m;

	if (__builtin_fabs((double)(a[15] - 1.0f)) > 1e-3f || __builtin_fabs((double)a[3]) > 1e-3f ||
		__builtin_fabs((double)a[7]) > 1e-3f || __builtin_fabs((double)a[11]) > 1e-3f) {
		return 0;
	}
	float c0 = a[5] * a[10] - a[6] * a[9];
	float c1 = a[4] * a[10] - a[8] * a[6];
	float c2 = a[4] * a[9] - a[8] * a[5];
	float inv = 1.0f / (c0 * a[0] - c1 * a[1] + c2 * a[2]);

	o[0] = c0 * inv;
	o[1] = -((a[10] * a[1] - a[9] * a[2]) * inv);
	o[2] = (a[6] * a[1] - a[5] * a[2]) * inv;
	o[3] = 0.0f;
	o[4] = -((a[4] * a[10] - a[8] * a[6]) * inv);
	o[5] = (a[10] * a[0] - a[8] * a[2]) * inv;
	o[6] = -((a[6] * a[0] - a[4] * a[2]) * inv);
	o[7] = 0.0f;
	o[8] = (a[4] * a[9] - a[8] * a[5]) * inv;
	o[9] = -((a[0] * a[9] - a[8] * a[1]) * inv);
	o[10] = (a[5] * a[0] - a[4] * a[1]) * inv;
	o[11] = 0.0f;
	o[12] = -(a[12] * o[0] + o[4] * a[13] + a[14] * o[8]);
	o[13] = -(a[12] * o[1] + o[5] * a[13] + a[14] * o[9]);
	o[14] = -(o[6] * a[13] + o[2] * a[12] + a[14] * o[10]);
	o[15] = 1.0f;
	return 1;
}

/* the same as an HRESULT, 0 (S_OK) or E_INVALIDARG (matriz.h, rva 0x239560) */
int Matrix_Invert(matrix_t *out, const matrix_t *m) {
	return Matrix_InvertAffine(out, m) ? 0 : (int)0x80070057;
}

/* m = m * Rz * Rx * Ry for the angles ang */
matrix_t *Matrix_Rotate(matrix_t *m, const vec3_t *ang) {
	matrix_t rx, ry, rz, t1, t2, t3;

	Matrix_RotX(&rx, ang->x);
	Matrix_RotY(&ry, ang->y);
	Matrix_RotZ(&rz, ang->z);
	Matrix_Mul(Matrix_Mul(Matrix_Mul(m, &t3, &rz), &t2, &rx), &t1, &ry);
	return Matrix_Copy(m, &t1);
}

/* the columns scaled by s */
const vec3_t *Matrix_Scale(matrix_t *self, const vec3_t *s) {
	for (int r = 0; r < 4; r++) {
		self->m[r * 4] = s->x * self->m[r * 4];
		self->m[r * 4 + 1] = s->y * self->m[r * 4 + 1];
		self->m[r * 4 + 2] = s->z * self->m[r * 4 + 2];
	}
	return s;
}

/* 0 if far and near are too close or the field of view (radians) too small */
byte Matrix_Perspective(matrix_t *out, float fov, float aspect, float zNear, float zFar) {
	float depth = zFar - zNear;
	float s, c;

	if (0.01f > __builtin_fabs((double)depth)) {
		return 0;
	}
	X87_SinCos(fov * 0.5f, &s, &c);
	if (0.01f > __builtin_fabs((double)s)) {
		return 0;
	}
	float q = zFar / depth;
	memset(out->m, 0, sizeof(out->m));
	float cot = c / s;
	out->m[11] = 1.0f;
	out->m[5] = cot;
	out->m[0] = cot * aspect;
	out->m[10] = q;
	out->m[14] = -(q * zNear);
	return 1;
}

/* the same as an HRESULT (matriz.h, rva 0x23a190) */
int Matrix_PerspectiveD3D(matrix_t *out, float fov, float aspect, float zNear, float zFar) {
	return Matrix_Perspective(out, fov, aspect, zNear, zFar) ? 0 : (int)0x80070057;
}

void Matrix_RotX(matrix_t *self, float ang) {
	Matrix_Identity(self);
	float c = (float)Game_Cos((double)ang);
	self->m[5] = c;
	float s = (float)Game_Sin((double)ang);
	self->m[6] = s;
	self->m[9] = -s;
	self->m[10] = c;
}

void Matrix_RotY(matrix_t *self, float ang) {
	Matrix_Identity(self);
	float c = (float)Game_Cos((double)ang);
	self->m[0] = c;
	float s = (float)Game_Sin((double)ang);
	self->m[8] = s;
	self->m[2] = -s;
	self->m[10] = c;
}

void Matrix_RotZ(matrix_t *self, float ang) {
	Matrix_Identity(self);
	float c = (float)Game_Cos((double)ang);
	self->m[0] = c;
	float s = (float)Game_Sin((double)ang);
	self->m[1] = s;
	self->m[4] = -s;
	self->m[5] = c;
}

/* the rotation by ang (radians) around axis, no translation */
void Matrix_RotAxis(matrix_t *self, const vec3_t *axis, float ang) {
	float c = (float)Game_Cos((double)ang);
	float s = (float)Game_Sin((double)ang);
	float len = (float)__builtin_sqrt((double)(axis->x * axis->x + axis->y * axis->y + axis->z * axis->z));
	float x = axis->x, y = axis->y, z = axis->z;
	float *m = self->m;

	if (len != 0.0f) {
		float inv = 1.0f / len;
		x = axis->x * inv;
		y = axis->y * inv;
		z = axis->z * inv;
	}
	float omc = 1.0f - c;
	m[11] = 0.0f;
	m[7] = 0.0f;
	m[3] = 0.0f;
	m[14] = 0.0f;
	m[13] = 0.0f;
	m[12] = 0.0f;
	m[0] = x * x * omc + c;
	m[1] = y * x * omc - z * s;
	m[15] = 1.0f;
	m[4] = y * x * omc + z * s;
	m[2] = z * x * omc + y * s;
	m[8] = z * x * omc - y * s;
	m[10] = z * z * omc + c;
	m[5] = y * y * omc + c;
	m[6] = z * y * omc - x * s;
	m[9] = z * y * omc + x * s;
}

const vec3_t *Matrix_Translate(matrix_t *self, const vec3_t *v) {
	self->m[12] = v->x + self->m[12];
	self->m[13] = v->y + self->m[13];
	self->m[14] = v->z + self->m[14];
	return v;
}

/* v * m divided by w; 0 if w is about 0 */
byte Matrix_TransformProject(vec3_t *out, const vec3_t *v, const matrix_t *m) {
	const float *a = m->m;
	float vx = v->x, vy = v->y, vz = v->z;
	float y = a[1] * vx + a[5] * vy + a[9] * vz + a[13];
	float z = a[2] * vx + a[6] * vy + a[10] * vz + a[14];
	float w = a[3] * vx + a[7] * vy + a[11] * vz + a[15];

	if (1e-5f > __builtin_fabs((double)w)) {
		return 0;
	}
	float x = a[4] * vy + vx * a[0] + a[8] * vz + a[12];
	out->y = y / w;
	out->x = x / w;
	out->z = z / w;
	return 1;
}

/* v * m without the translation */
vec3_t *Matrix_TransformDir(vec3_t *out, const vec3_t *v, const matrix_t *m) {
	const float *a = m->m;
	float vx = v->x, vy = v->y, vz = v->z;
	float y = a[1] * vx + a[5] * vy + a[9] * vz;
	float z = a[2] * vx + a[6] * vy + a[10] * vz;
	float x = a[4] * vy + vx * a[0] + a[8] * vz;

	out->y = y;
	out->z = z;
	out->x = x;
	return out;
}

/* the components of (v, 1) * m that are wanted (non-NULL) */
const matrix_t *Matrix_TransformParts(float *x, float *y, float *z, float *w, const vec3_t *v, const matrix_t *m) {
	const float *a = m->m;

	if (x) {
		*x = v->y * a[4] + v->x * a[0] + a[8] * v->z + a[12];
	}
	if (y) {
		*y = v->y * a[5] + a[1] * v->x + a[9] * v->z + a[13];
	}
	if (z) {
		*z = a[6] * v->y + a[2] * v->x + a[10] * v->z + a[14];
	}
	if (w) {
		*w = a[7] * v->y + a[3] * v->x + a[11] * v->z + a[15];
	}
	return m;
}
