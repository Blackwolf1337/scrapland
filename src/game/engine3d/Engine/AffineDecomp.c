/*
 * The decomposition of an affine matrix into translation, rotation, scale with the axes of the scale and the sign of
 * the determinant, original source Engine/descomp_afin.cpp (rva 0x2d6010..0x2da770, include/scrap/readm3d.h). The
 * decomposition is Ken Shoemake's decomp.c of Graphics Gems IV ("Polar Matrix Decomposition"), compiled with its
 * mix of float and double (fabs, sqrt and the 0.5 and 2.0 constants are double), in its order of operations. Its
 * matrices are 4x4 arrays for column vectors: a matrix of the engine (row vectors) goes in transposed.
 *
 * The small functions of decomp.c (Qt_Mul, vcross, the norms, the reflections ...) are inlined in the exe; their own
 * copies there are never called.
 */
#include "scrap/game.h"
#include "scrap/readm3d.h"

typedef float HMatrix[4][4];

typedef struct {
	float x, y, z, w;
} hQuat_t;

typedef struct {
	hQuat_t t;	/* translation, w 0 */
	hQuat_t q;	/* the rotation */
	hQuat_t u;	/* the rotation of the axes of the scale */
	hQuat_t k;	/* the scale, w 1 */
	float f;	/* the sign of the determinant */
} affineParts_t;

enum { X, Y, Z, W };

#define TOL 1.0e-6
#define SQRTHALF 0.7071067811865475244

/* rva 0x432a98 */
static const HMatrix mat_id = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };

static void Mat_Pad(HMatrix a) {
	a[W][X] = a[X][W] = a[W][Y] = a[Y][W] = a[W][Z] = a[Z][W] = 0;
	a[W][W] = 1;
}

static void Mat_Copy(HMatrix c, const HMatrix a) {
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			c[i][j] = a[i][j];
		}
	}
}

static float VDot(const float *a, const float *b) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void VCross(const float *a, const float *b, float *v) {
	v[0] = a[1] * b[2] - a[2] * b[1];
	v[1] = a[2] * b[0] - a[0] * b[2];
	v[2] = a[0] * b[1] - a[1] * b[0];
}

static void AdjointTranspose(HMatrix m, HMatrix madjt) {
	VCross(m[1], m[2], madjt[0]);
	VCross(m[2], m[0], madjt[1]);
	VCross(m[0], m[1], madjt[2]);
}

static hQuat_t Qt_(float x, float y, float z, float w) {
	hQuat_t q = { x, y, z, w };
	return q;
}

static hQuat_t Qt_Conj(hQuat_t q) {
	hQuat_t r = { -q.x, -q.y, -q.z, q.w };
	return r;
}

static hQuat_t Qt_Mul(hQuat_t l, hQuat_t r) {
	hQuat_t q;
	q.w = l.w * r.w - l.x * r.x - l.y * r.y - l.z * r.z;
	q.x = l.w * r.x + l.x * r.w + l.y * r.z - l.z * r.y;
	q.y = l.w * r.y + l.y * r.w + l.z * r.x - l.x * r.z;
	q.z = l.w * r.z + l.z * r.w + l.x * r.y - l.y * r.x;
	return q;
}

static hQuat_t Qt_Scale(hQuat_t q, float w) {
	hQuat_t r = { q.x * w, q.y * w, q.z * w, q.w * w };
	return r;
}

/* rva 0x2d60a0: the unit quaternion of a rotation matrix */
static hQuat_t Qt_FromMatrix(HMatrix mat) {
	hQuat_t qu;
	double tr, s;

	tr = mat[X][X] + mat[Y][Y] + mat[Z][Z];
	if (tr >= 0.0) {
		s = __builtin_sqrt(tr + mat[W][W]);
		qu.w = s * 0.5;
		s = 0.5 / s;
		qu.x = (mat[Z][Y] - mat[Y][Z]) * s;
		qu.y = (mat[X][Z] - mat[Z][X]) * s;
		qu.z = (mat[Y][X] - mat[X][Y]) * s;
	} else {
		int h = X;
		if (mat[Y][Y] > mat[X][X]) {
			h = Y;
		}
		if (mat[Z][Z] > mat[h][h]) {
			h = Z;
		}
		/* the components i, j, k for the axes I, J, K, then w */
		static const int axes[3][3] = { { X, Y, Z }, { Y, Z, X }, { Z, X, Y } };
		int I = axes[h][0], J = axes[h][1], K = axes[h][2];
		float c[3];
		s = __builtin_sqrt((mat[I][I] - (mat[J][J] + mat[K][K])) + mat[W][W]);
		c[0] = s * 0.5;
		s = 0.5 / s;
		c[1] = (mat[I][J] + mat[J][I]) * s;
		c[2] = (mat[K][I] + mat[I][K]) * s;
		qu.w = (mat[K][J] - mat[J][K]) * s;
		float *q = &qu.x;
		q[I] = c[0];
		q[J] = c[1];
		q[K] = c[2];
	}
	if (mat[W][W] != 1.0) {
		qu = Qt_Scale(qu, 1 / __builtin_sqrt(mat[W][W]));
	}
	return qu;
}

/* the 1 norm (tpose: column sums) or infinity norm (row sums) */
static float Mat_Norm(HMatrix m, int tpose) {
	float max = 0.0;

	for (int i = 0; i < 3; i++) {
		float sum;
		if (tpose) {
			sum = __builtin_fabs(m[0][i]) + __builtin_fabs(m[1][i]) + __builtin_fabs(m[2][i]);
		} else {
			sum = __builtin_fabs(m[i][0]) + __builtin_fabs(m[i][1]) + __builtin_fabs(m[i][2]);
		}
		if (max < sum) {
			max = sum;
		}
	}
	return max;
}

static float Norm_Inf(HMatrix m) {
	return Mat_Norm(m, 0);
}

static float Norm_One(HMatrix m) {
	return Mat_Norm(m, 1);
}

/* rva 0x2d7c60: the column of the largest absolute entry, -1 for the zero matrix */
static int Find_MaxCol(HMatrix m) {
	float max = 0.0;
	int col = -1;

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			float abs = m[i][j];
			if (abs < 0.0) {
				abs = -abs;
			}
			if (abs > max) {
				max = abs;
				col = j;
			}
		}
	}
	return col;
}

/* the Householder vector u that zeroes all components of v but the first */
static void Make_Reflector(const float *v, float *u) {
	float s = __builtin_sqrt(VDot(v, v));

	u[0] = v[0];
	u[1] = v[1];
	u[2] = v[2] + ((v[2] < 0.0) ? -s : s);
	s = __builtin_sqrt(2.0 / VDot(u, u));
	u[0] = u[0] * s;
	u[1] = u[1] * s;
	u[2] = u[2] * s;
}

static void Reflect_Cols(HMatrix m, const float *u) {
	for (int i = 0; i < 3; i++) {
		float s = u[0] * m[0][i] + u[1] * m[1][i] + u[2] * m[2][i];
		for (int j = 0; j < 3; j++) {
			m[j][i] -= u[j] * s;
		}
	}
}

static void Reflect_Rows(HMatrix m, const float *u) {
	for (int i = 0; i < 3; i++) {
		float s = VDot(u, m[i]);
		for (int j = 0; j < 3; j++) {
			m[i][j] -= u[j] * s;
		}
	}
}

/* rva 0x2d6dc0: the orthogonal factor q of a matrix of rank 1 or 0 */
static void Do_Rank1(HMatrix m, HMatrix q) {
	float v1[3], v2[3], s;
	int col;

	Mat_Copy(q, mat_id);
	col = Find_MaxCol(m);
	if (col < 0) {
		return;
	}
	v1[0] = m[0][col];
	v1[1] = m[1][col];
	v1[2] = m[2][col];
	Make_Reflector(v1, v1);
	Reflect_Cols(m, v1);
	v2[0] = m[2][0];
	v2[1] = m[2][1];
	v2[2] = m[2][2];
	Make_Reflector(v2, v2);
	Reflect_Rows(m, v2);
	s = m[2][2];
	if (s < 0.0) {
		q[2][2] = -1.0;
	}
	Reflect_Cols(q, v1);
	Reflect_Rows(q, v2);
}

/* rva 0x2d74c0: the orthogonal factor q of a matrix of rank 2 or less, from its adjoint transpose */
static void Do_Rank2(HMatrix m, HMatrix madjt, HMatrix q) {
	float v1[3], v2[3];
	float w, x, y, z, c, s, d;
	int col;

	col = Find_MaxCol(madjt);
	if (col < 0) {
		Do_Rank1(m, q);
		return;
	}
	v1[0] = madjt[0][col];
	v1[1] = madjt[1][col];
	v1[2] = madjt[2][col];
	Make_Reflector(v1, v1);
	Reflect_Cols(m, v1);
	VCross(m[0], m[1], v2);
	Make_Reflector(v2, v2);
	Reflect_Rows(m, v2);
	w = m[0][0];
	x = m[0][1];
	y = m[1][0];
	z = m[1][1];
	if (w * z > x * y) {
		c = z + w;
		s = y - x;
		d = __builtin_sqrt(c * c + s * s);
		c = c / d;
		s = s / d;
		q[0][0] = q[1][1] = c;
		q[0][1] = -(q[1][0] = s);
	} else {
		c = z - w;
		s = y + x;
		d = __builtin_sqrt(c * c + s * s);
		c = c / d;
		s = s / d;
		q[0][0] = -(q[1][1] = c);
		q[0][1] = q[1][0] = s;
	}
	q[0][2] = q[2][0] = q[1][2] = q[2][1] = 0.0;
	q[2][2] = 1.0;
	Reflect_Cols(q, v1);
	Reflect_Rows(q, v2);
}

/* rva 0x2d8910: m = q s, q orthogonal, s symmetric positive semi-definite; the determinant of m */
static float Polar_Decomp(HMatrix m, HMatrix q, HMatrix s) {
	HMatrix mk, madjtk, ek;
	float det, m_one, m_inf, madjt_one, madjt_inf, e_one, gamma, g1, g2;

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			mk[i][j] = m[j][i];
		}
	}
	m_one = Norm_One(mk);
	m_inf = Norm_Inf(mk);
	do {
		AdjointTranspose(mk, madjtk);
		det = VDot(mk[0], madjtk[0]);
		if (det == 0.0) {
			Do_Rank2(mk, madjtk, mk);
			break;
		}
		madjt_one = Norm_One(madjtk);
		madjt_inf = Norm_Inf(madjtk);
		gamma = __builtin_sqrt(__builtin_sqrt((madjt_one * madjt_inf) / (m_one * m_inf)) / __builtin_fabs(det));
		g1 = gamma * 0.5;
		g2 = 0.5 / (gamma * det);
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				ek[i][j] = mk[i][j];
				mk[i][j] = g1 * mk[i][j] + g2 * madjtk[i][j];
				ek[i][j] -= mk[i][j];
			}
		}
		e_one = Norm_One(ek);
		m_one = Norm_One(mk);
		m_inf = Norm_Inf(mk);
	} while (e_one > (m_one * TOL));
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			q[i][j] = mk[j][i];
		}
	}
	Mat_Pad(q);
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			s[i][j] = mk[i][0] * m[0][j] + mk[i][1] * m[1][j] + mk[i][2] * m[2][j];
		}
	}
	Mat_Pad(s);
	for (int i = 0; i < 3; i++) {
		for (int j = i; j < 3; j++) {
			s[i][j] = s[j][i] = 0.5 * (s[i][j] + s[j][i]);
		}
	}
	return det;
}

/* rva 0x2da2f0: the eigenvalues of the symmetric s (Jacobi), u the eigenvectors in its columns */
static hQuat_t Spect_Decomp(HMatrix s, HMatrix u) {
	static const char nxt[] = { Y, Z, X };	/* rva 0x432ad8 */
	hQuat_t kv;
	double diag[3], offd[3];
	double g, h, fabsh, fabsOffDi, t, theta, c, sn, tau, ta, offdq, a, b;

	Mat_Copy(u, mat_id);
	diag[X] = s[X][X];
	diag[Y] = s[Y][Y];
	diag[Z] = s[Z][Z];
	offd[X] = s[Y][Z];
	offd[Y] = s[Z][X];
	offd[Z] = s[X][Y];
	for (int sweep = 20; sweep > 0; sweep--) {
		float sm = __builtin_fabs(offd[X]) + __builtin_fabs(offd[Y]) + __builtin_fabs(offd[Z]);
		if (sm == 0.0) {
			break;
		}
		for (int i = Z; i >= X; i--) {
			int p = nxt[i];
			int q = nxt[p];
			fabsOffDi = __builtin_fabs(offd[i]);
			g = 100.0 * fabsOffDi;
			if (fabsOffDi > 0.0) {
				h = diag[q] - diag[p];
				fabsh = __builtin_fabs(h);
				if (fabsh + g == fabsh) {
					t = offd[i] / h;
				} else {
					theta = 0.5 * h / offd[i];
					t = 1.0 / (__builtin_fabs(theta) + __builtin_sqrt(theta * theta + 1.0));
					if (theta < 0.0) {
						t = -t;
					}
				}
				c = 1.0 / __builtin_sqrt(t * t + 1.0);
				sn = t * c;
				tau = sn / (c + 1.0);
				ta = t * offd[i];
				offd[i] = 0.0;
				diag[p] -= ta;
				diag[q] += ta;
				offdq = offd[q];
				offd[q] -= sn * (offd[p] + tau * offd[q]);
				offd[p] += sn * (offdq - tau * offd[p]);
				for (int j = Z; j >= X; j--) {
					a = u[j][p];
					b = u[j][q];
					u[j][p] -= sn * (b + tau * a);
					u[j][q] += sn * (a - tau * b);
				}
			}
		}
	}
	kv.x = diag[X];
	kv.y = diag[Y];
	kv.z = diag[Z];
	kv.w = 1.0;
	return kv;
}

#define SGN(n, v) ((n) ? -(v) : (v))
#define SWAP(a, i, j) \
	{ \
		a[3] = a[i]; \
		a[i] = a[j]; \
		a[j] = a[3]; \
	}
#define CYCLE(a, p) \
	if (p) { \
		a[3] = a[0]; \
		a[0] = a[1]; \
		a[1] = a[2]; \
		a[2] = a[3]; \
	} else { \
		a[3] = a[2]; \
		a[2] = a[1]; \
		a[1] = a[0]; \
		a[0] = a[3]; \
	}

/* rva 0x2d9780: the rotation p with q p closest to the identity that keeps k a scale, k permuted to match */
static hQuat_t Snuggle(hQuat_t q, hQuat_t *k) {
	hQuat_t p;
	float ka[4];
	int turn = -1;

	ka[X] = k->x;
	ka[Y] = k->y;
	ka[Z] = k->z;
	if (ka[X] == ka[Y]) {
		turn = ka[X] == ka[Z] ? W : Z;
	} else if (ka[X] == ka[Z]) {
		turn = Y;
	} else if (ka[Y] == ka[Z]) {
		turn = X;
	}
	if (turn >= 0) {
		/* rva 0x432adc.. */
		static const hQuat_t qxtoz = { 0, SQRTHALF, 0, SQRTHALF };
		static const hQuat_t qytoz = { SQRTHALF, 0, 0, SQRTHALF };
		static const hQuat_t qppmm = { 0.5, 0.5, -0.5, -0.5 };
		static const hQuat_t qpppp = { 0.5, 0.5, 0.5, 0.5 };
		static const hQuat_t qmpmm = { -0.5, 0.5, -0.5, -0.5 };
		static const hQuat_t qpppm = { 0.5, 0.5, 0.5, -0.5 };
		static const hQuat_t q0001 = { 0.0, 0.0, 0.0, 1.0 };
		static const hQuat_t q1000 = { 1.0, 0.0, 0.0, 0.0 };
		hQuat_t qtoz, qp;
		unsigned neg[3], win;
		double mag[3], t;

		switch (turn) {
		default:
			return Qt_Conj(q);
		case X:
			q = Qt_Mul(q, qtoz = qxtoz);
			SWAP(ka, X, Z)
			break;
		case Y:
			q = Qt_Mul(q, qtoz = qytoz);
			SWAP(ka, Y, Z)
			break;
		case Z:
			qtoz = q0001;
			break;
		}
		q = Qt_Conj(q);
		mag[0] = (double)q.z * q.z + (double)q.w * q.w - 0.5;
		mag[1] = (double)q.x * q.z - (double)q.y * q.w;
		mag[2] = (double)q.y * q.z + (double)q.x * q.w;
		for (int i = 0; i < 3; i++) {
			if ((neg[i] = (mag[i] < 0.0))) {
				mag[i] = -mag[i];
			}
		}
		if (mag[0] > mag[1]) {
			win = mag[0] > mag[2] ? 0 : 2;
		} else {
			win = mag[1] > mag[2] ? 1 : 2;
		}
		switch (win) {
		case 0:
			p = neg[0] ? q1000 : q0001;
			break;
		case 1:
			p = neg[1] ? qppmm : qpppp;
			CYCLE(ka, 0)
			break;
		default:
			p = neg[2] ? qmpmm : qpppm;
			CYCLE(ka, 1)
			break;
		}
		qp = Qt_Mul(q, p);
		t = __builtin_sqrt(mag[win] + 0.5);
		p = Qt_Mul(p, Qt_(0.0, 0.0, -qp.z / t, qp.w / t));
		p = Qt_Mul(qtoz, Qt_Conj(p));
	} else {
		float qa[4], pa[4];
		unsigned lo, hi, neg[4], par = 0;
		double all, big, two;

		qa[0] = q.x;
		qa[1] = q.y;
		qa[2] = q.z;
		qa[3] = q.w;
		for (int i = 0; i < 4; i++) {
			pa[i] = 0.0;
			if ((neg[i] = (qa[i] < 0.0))) {
				qa[i] = -qa[i];
			}
			par ^= neg[i];
		}
		lo = qa[0] > qa[1] ? 0 : 1;
		hi = qa[2] > qa[3] ? 2 : 3;
		if (qa[lo] > qa[hi]) {
			if (qa[lo ^ 1] > qa[hi]) {
				hi = lo;
				lo ^= 1;
			} else {
				hi ^= lo;
				lo ^= hi;
				hi ^= lo;
			}
		} else if (qa[hi ^ 1] > qa[lo]) {
			lo = hi ^ 1;
		}
		all = (qa[0] + qa[1] + qa[2] + qa[3]) * 0.5;
		two = (qa[hi] + qa[lo]) * SQRTHALF;
		big = qa[hi];
		if (all > two) {
			if (all > big) {
				for (int i = 0; i < 4; i++) {
					pa[i] = SGN(neg[i], 0.5);
				}
				CYCLE(ka, par)
			} else {
				pa[hi] = SGN(neg[hi], 1.0);
			}
		} else if (two > big) {
			pa[hi] = SGN(neg[hi], SQRTHALF);
			pa[lo] = SGN(neg[lo], SQRTHALF);
			if (lo > hi) {
				hi ^= lo;
				lo ^= hi;
				hi ^= lo;
			}
			if (hi == W) {
				hi = "\001\002\000"[lo];	/* rva 0x3e7e84 */
				lo = 3 - hi - lo;
			}
			SWAP(ka, hi, lo)
		} else {
			pa[hi] = SGN(neg[hi], 1.0);
		}
		p.x = -pa[0];
		p.y = -pa[1];
		p.z = -pa[2];
		p.w = pa[3];
	}
	k->x = ka[X];
	k->y = ka[Y];
	k->z = ka[Z];
	return p;
}

/* rva 0x2d69a0: a = t f r u k u^-1 with the rotation r of q */
static void Decomp_Affine(HMatrix a, affineParts_t *parts) {
	HMatrix q, s, u;
	hQuat_t p;
	float det;

	parts->t = Qt_(a[X][W], a[Y][W], a[Z][W], 0);
	det = Polar_Decomp(a, q, s);
	if (det < 0.0) {
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				q[i][j] = -q[i][j];
			}
		}
		parts->f = -1;
	} else {
		parts->f = 1;
	}
	parts->q = Qt_FromMatrix(q);
	parts->k = Spect_Decomp(s, u);
	parts->u = Qt_FromMatrix(u);
	p = Snuggle(parts->u, &parts->k);
	parts->u = Qt_Mul(parts->u, p);
}

/* rva 0x2d6c50: the transform of an engine matrix; the rotations conjugated (the engine's quaternion convention) */
void Matrix_ToTransform(const matrix_t *m, m3dTransform_t *out) {
	HMatrix a;
	affineParts_t parts;

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			a[i][j] = m->m[j * 4 + i];
		}
	}
	Decomp_Affine(a, &parts);
	out->pos.x = parts.t.x;
	out->pos.y = parts.t.y;
	out->pos.z = parts.t.z;
	out->rot.x = -parts.q.x;
	out->rot.y = -parts.q.y;
	out->rot.z = -parts.q.z;
	out->rot.w = parts.q.w;
	out->rotScale.x = -parts.u.x;
	out->rotScale.y = -parts.u.y;
	out->rotScale.z = -parts.u.z;
	out->rotScale.w = parts.u.w;
	out->scale.x = parts.k.x;
	out->scale.y = parts.k.y;
	out->scale.z = parts.k.z;
	out->sign = parts.f;
}

/* the scale along the axes of rotScale: (u^-1) diag(esc) u, then the sign */
static void M3DTransform_Scale(const m3dTransform_t *self, matrix_t *mu, matrix_t *mui, matrix_t *ms, matrix_t *mf) {
	Quat_ToMatrixD3D(mu, &self->rotScale);
	CHECK(Matrix_Invert(mui, mu) == 0);
	Matrix_LoadIdentity(ms);
	ms->m[0] = self->scale.x;
	ms->m[5] = self->scale.y;
	ms->m[10] = self->scale.z;
	Matrix_LoadIdentity(mf);
	mf->m[0] = self->sign;
	mf->m[5] = self->sign;
	mf->m[10] = self->sign;
}

/* rva 0x2d65d0: the matrix of a transform without its rotation and translation (the frames of an animation) */
matrix_t *__thiscall M3DTransform_ToMatrixAnim(m3dTransform_t *self, matrix_t *out) {
	matrix_t mu, mui, ms, mf, t, a, b;

	M3DTransform_Scale(self, &mu, &mui, &ms, &mf);
	a = *Matrix_MulD3D(&t, &mui, &ms);
	b = *Matrix_MulD3D(&t, &a, &mu);
	*out = *Matrix_MulD3D(&t, &b, &mf);
	return out;
}

/* rva 0x2d6750: the whole matrix of a transform (the pivots of the meshes) */
void __thiscall M3DTransform_ToMatrix(m3dTransform_t *self, matrix_t *out) {
	matrix_t mu, mui, ms, mf, mq, mt, t, a, b;

	Quat_ToMatrixD3D(&mq, &self->rot);
	M3DTransform_Scale(self, &mu, &mui, &ms, &mf);
	Matrix_LoadIdentity(&mt);
	mt.m[12] = self->pos.x;
	mt.m[13] = self->pos.y;
	mt.m[14] = self->pos.z;
	a = *Matrix_MulD3D(&t, &mui, &ms);
	b = *Matrix_MulD3D(&t, &a, &mu);
	a = *Matrix_MulD3D(&t, &b, &mq);
	b = *Matrix_MulD3D(&t, &a, &mf);
	*out = *Matrix_MulD3D(&t, &b, &mt);
}
