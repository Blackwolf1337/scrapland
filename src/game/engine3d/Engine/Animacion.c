/*
 * Keyframe animation of the engine3d, original source Engine/animacion.cpp (include/scrap/animacion.h).
 */
#include <stddef.h>
#include <string.h>

#include "scrap/animacion.h"

#define ANIMNODO_CTOR 0x2ab6a0
#define ANIMNODO_DTOR 0x2ab8f0
/* the flags a file may have */
#define CM3_SUPPORTED (ANIM_POS | ANIM_ROT | ANIM_FOV | ANIM_COLOR | ANIM_INTENS | ANIM_VISIBLE | ANIM_EVA)
#define OPT_SUPPORTED 0x7
#define STM_SUPPORTED 0x7
#define ANIM_COLOR_SCALE 0.003921568859368563f	/* 1/255, rva 0x3d3ce4 */
#define ANIM_PACK_SCALE -2.0f			/* rva 0x3b35a0 */
/* the ratio of two quaternion components that counts as equal */
#define ANIM_RATIO_MIN 0x1.fffff8p-1		/* 1 - 2^-22 */
#define ANIM_RATIO_MAX 0x1.000004p+0		/* 1 + 2^-22 */
#define STREAM_HEADER 6
#define STREAM_MAXLEN 65535

/* the tracks in the order of the node: flag, member, size of a value */
static const struct {
	uint16_t flag;
	uint16_t off;
	int size;
} animTracks[ANIM_TRACKS] = {
	{ ANIM_POS, offsetof(CAnimNodo, Pos), sizeof(vec3_t) },
	{ ANIM_ROT, offsetof(CAnimNodo, Rot), sizeof(quat_t) },
	{ ANIM_FOV, offsetof(CAnimNodo, FOV), sizeof(float) },
	{ ANIM_COLOR, offsetof(CAnimNodo, Color), 4 },
	{ ANIM_INTENS, offsetof(CAnimNodo, Intens), sizeof(float) },
	{ ANIM_VISIBLE, offsetof(CAnimNodo, Visible), 1 },
};

static void **AnimNodo_Track(CAnimNodo *n, int i) {
	return (void **)((byte *)n + animTracks[i].off);
}

/* a stream: the header words and the keys */
static int Stream_Word(const void *s, int i) {
	uint16_t w;
	memcpy(&w, (const byte *)s + i * 2, sizeof(w));
	return w;
}

static int Stream_Start(const void *s) {
	return Stream_Word(s, 1) & 0x7fff;
}

static const byte *Stream_Key(const void *s, int k, int size) {
	return (const byte *)s + STREAM_HEADER + k * size;
}

static float Stream_Float(const void *s, int k, int size, int i) {
	float f;
	memcpy(&f, Stream_Key(s, k, size) + i * sizeof(float), sizeof(f));
	return f;
}

static int AnimBlock_Inside(const void *ptr) {
	const CAnimBlock *b = g_ActAnimBlock;

	return b && b->MemBlock && (const byte *)ptr >= b->MemBlock && (const byte *)ptr < b->MemBlock + b->SizeBlock;
}

void AnimBlock_Free(void *ptr, uint32_t flags) {
	if (flags & OPT_INBLOCK) {
		CHECK(AnimBlock_Inside(ptr));
	}
	if (!AnimBlock_Inside(ptr)) {
		Mem_Free(ptr);
	}
}

uint32_t __fastcall CAnimNodo_GetFlags(CAnimNodo *self) {
	uint32_t flags = 0;

	for (int i = 0; i < ANIM_TRACKS; i++) {
		if (*AnimNodo_Track(self, i)) {
			flags |= animTracks[i].flag;
		}
	}
	if (self->EVA) {
		flags |= ANIM_EVA;
	}
	return flags;
}

int __thiscall CAnimNodo_TrackKind(CAnimNodo *self, int frame, int unused, uint32_t flag) {
	const void *t;
	uint32_t rel = frame - self->primer_frame;

	(void)unused;
	if (flag == ANIM_POS) {
		t = self->Pos;
	} else if (flag == ANIM_ROT) {
		t = self->Rot;
	} else if (flag == ANIM_FOV) {
		t = self->FOV;
	} else {
		return 0;
	}
	if (!t || rel >= self->num_frames) {
		return 0;
	}
	if (!(flag & self->OptFlags)) {
		return 2;
	}
	if (!(flag & self->StmFlags)) {
		return 4;
	}
	int keys = Stream_Word(t, 2);
	int k = (int)rel - Stream_Start(t);
	if (keys == 1) {
		return 4;
	}
	if (k < 0) {
		return 1;
	}
	if (k + 1 < keys) {
		return 2;
	}
	return k >= keys ? 3 : 2;
}

static void AnimNodo_DeleteEVA(CAnimNodo *self) {
	CEVA *eva = self->EVA;

	if (eva) {
		CEVA_Free(eva);
		operator_delete_sized(eva, sizeof(*eva));
		self->EVA = NULL;
	}
}

static CEVA *AnimNodo_NewEVA(void) {
	CEVA *eva = operator_new(sizeof(*eva));

	return eva ? CEVA_ctor(eva) : NULL;
}

void __thiscall CAnimNodo_Init(CAnimNodo *self, uint32_t nframes, uint32_t first, uint32_t flags) {
	CHECK(nframes <= 0xffff);
	CHECK(first <= 0xffff);
	CHECK(flags <= 0xffff);
	for (int i = 0; i < ANIM_TRACKS; i++) {
		void **t = AnimNodo_Track(self, i);
		if (*t) {
			AnimBlock_Free(*t, self->OptFlags);
			*t = NULL;
		}
	}
	AnimNodo_DeleteEVA(self);
	self->primer_frame = first;
	self->num_frames = nframes;
	self->OptFlags = 0;
	self->StmFlags = 0;
	if (!(uint16_t)nframes || !flags) {
		return;
	}
	if (flags & ANIM_POS) {
		self->Pos = New_Array((uint16_t)nframes, sizeof(vec3_t));
	}
	if (flags & ANIM_ROT) {
		self->Rot = New_Array(self->num_frames, sizeof(quat_t));
	}
	if (flags & ANIM_FOV) {
		self->FOV = New_Array(self->num_frames, sizeof(float));
	}
	if (flags & ANIM_COLOR) {
		self->Color = New_Array(self->num_frames, 4);
	}
	if (flags & ANIM_INTENS) {
		self->Intens = New_Array(self->num_frames, sizeof(float));
	}
	if (flags & ANIM_VISIBLE) {
		self->Visible = operator_new_array(self->num_frames);
	}
	if (flags & ANIM_EVA) {
		self->EVA = AnimNodo_NewEVA();
	}
}

CAnimNodo *__thiscall CAnimacion_GetNodo(CAnimacion *self, uint32_t i) {
	CHECK(self->AnimNodo);
	if (i < (uint32_t)self->num_objetos) {
		return &self->AnimNodo[i];
	}
	return NULL;
}

CAnimNodo *__thiscall CAnimacion_GetNodoTabla(CAnimacion *self, uint32_t i) {
	uint32_t n = 0xffffffffu;

	if (i < (uint32_t)self->tabla.n) {
		n = ((byte *)self->tabla.p)[i] - 1;
	}
	if (n < (uint32_t)self->num_objetos) {
		return &self->AnimNodo[n];
	}
	return NULL;
}

void __thiscall CAnimacion_Init(CAnimacion *self, int nobjetos, int nframes, int first) {
	CHECK(nobjetos >= 0);
	self->primer_frame = first;
	self->num_frames = nframes;
	self->flag = 0;
	/* the destructors of the nodes free their tracks unless they are in the block */
	g_ActAnimBlock = &self->AnimBlock;
	if (self->AnimNodo) {
		Delete_Objects(self->AnimNodo, sizeof(CAnimNodo), ANIMNODO_DTOR);
		self->AnimNodo = NULL;
	}
	self->num_objetos = nobjetos;
	if (nobjetos) {
		self->AnimNodo = New_Objects(nobjetos, sizeof(CAnimNodo), ANIMNODO_CTOR, ANIMNODO_DTOR);
	}
	g_ActAnimBlock = NULL;
	if (self->AnimBlock.MemBlock) {
		operator_delete_sized(self->AnimBlock.MemBlock, 1);
		self->AnimBlock.MemBlock = NULL;
	}
	self->AnimBlock.UsedBlock = 0;
	self->AnimBlock.SizeBlock = 0;
}

void __thiscall CAnimNodo_ReadTrack(CAnimNodo *self, CArchIO *f, uint32_t flag, void **ptr, int size) {
	if (!flag) {
		return;
	}
	CHECK(ptr);
	CHECK(*ptr == NULL);
	int n = (flag & self->OptFlags) ? 1 : self->num_frames;
	if (flag & (self->StmFlags & self->OptFlags)) {
		size = CArchIO_ReadInt(f);
	}
	CHECK(g_ActAnimBlock);
	CAnimBlock *b = g_ActAnimBlock;
	b->UsedBlock += n * size;
	CHECK(b->UsedBlock <= b->SizeBlock);
	*ptr = b->MemBlock - n * size + b->UsedBlock;
}

void __thiscall CAnimNodo_Read(CAnimNodo *self, CArchIO *f) {
	CArchIO_BeginBlock(f, "NAM");
	CHECK(CArchIO_ReadInt(f) == 1);
	int first = CArchIO_ReadInt(f);
	int frames = CArchIO_ReadInt(f);
	uint32_t flags = CArchIO_ReadInt(f);
	int opt = CArchIO_ReadInt(f);
	int stm = CArchIO_ReadInt(f);
	CAnimNodo_Init(self, frames, first, 0);
	self->OptFlags = opt;
	self->StmFlags = stm;
	CHECK((flags & ~CM3_SUPPORTED) == 0);
	CHECK((self->OptFlags & ~OPT_SUPPORTED) == 0);
	CHECK((self->StmFlags & ~STM_SUPPORTED) == 0);
	self->OptFlags |= OPT_INBLOCK;
	for (int i = 0; i < ANIM_TRACKS; i++) {
		CAnimNodo_ReadTrack(self, f, flags & animTracks[i].flag, AnimNodo_Track(self, i), animTracks[i].size);
	}
	EVA_Read(f, flags & ANIM_EVA, &self->EVA);
	CHECK(flags == CAnimNodo_GetFlags(self));
	CArchIO_Error(f, "NAM");
}

void __thiscall CAnimacion_Read(CAnimacion *self, CArchIO *f) {
	g_MemStatsContext = g_AnimMemStats;
	CArchIO_BeginBlock(f, "ANI");
	CHECK(CArchIO_ReadInt(f) == 2);
	CArchIO_Read4(f, &self->fps);
	int first = CArchIO_ReadInt(f);
	int frames = CArchIO_ReadInt(f);
	int nobjetos = CArchIO_ReadInt(f);
	CAnimacion_Init(self, nobjetos, frames, first);
	self->flag = CArchIO_ReadInt(f) != 0;
	int n = CArchIO_ReadInt(f);
	/* the CTabla<byte> allocation of tabla.h, inline in the original */
	CHECK(!self->tabla.p && n > 0);
	self->tabla.n = n;
	self->tabla.p = operator_new_array(n);
	CHECK(self->tabla.p);
	CArchIO_Read(f, self->tabla.p, self->tabla.n);
	int size = 0;
	CArchIO_BeginBlockSize(f, "NABK", &size);
	CAnimBlock *b = &self->AnimBlock;
	CHECK(!b->MemBlock && size >= 0);
	b->SizeBlock = size;
	b->UsedBlock = 0;
	b->MemBlock = operator_new_array(size);
	CArchIO_Read(f, b->MemBlock, self->AnimBlock.SizeBlock);
	CArchIO_Error(f, "NABK");
	g_ActAnimBlock = b;
	for (int i = 0; i < self->num_objetos; i++) {
		CAnimNodo_Read(CAnimacion_GetNodo(self, i), f);
	}
	g_ActAnimBlock = NULL;
	CHECK(self->AnimBlock.SizeBlock == self->AnimBlock.UsedBlock);
	CArchIO_Error(f, "ANI");
	g_MemStatsContext = NULL;
}

/* track t (pos, rot or fov) becomes the single value v0 */
static void AnimNodo_ToConst(CAnimNodo *self, int t, const void *v0) {
	void **track = AnimNodo_Track(self, t);

	self->OptFlags |= animTracks[t].flag;
	if (*track) {
		AnimBlock_Free(*track, self->OptFlags);
		*track = NULL;
	}
	*track = operator_new_array(animTracks[t].size);
	memcpy(*track, v0, animTracks[t].size);
}

/* track t of n values becomes a stream without the first lead - 1 and the last trail - 1 values, which equal their
 * ends; with ends the first and the last value are put at the ends of the kept range first */
static void AnimNodo_ToStream(CAnimNodo *self, int t, int n, int lead, int trail, int ends) {
	void **track = AnimNodo_Track(self, t);
	int size = animTracks[t].size;
	int len = n - (lead - 1) - (trail - 1);
	int keys = len < 2 ? 1 : len;
	int start = len >= 2 ? lead - 1 : 0;
	CAnimStream s;

	if (ends) {
		byte *v = *track;
		memcpy(v + (start + keys - 1) * size, v + (n - 1) * size, size);
		memcpy(v + start * size, v, size);
	}
	s.p = NULL;
	s.len = 0;
	AnimStream_AddHeader(&s, start, keys);
	AnimStream_AddBytes(&s, (byte *)*track + start * size, keys * size);
	uint16_t bytes = s.len;
	memcpy(s.p, &bytes, sizeof(bytes));
	self->StmFlags |= animTracks[t].flag;
	self->OptFlags |= animTracks[t].flag;
	if (*track) {
		AnimBlock_Free(*track, self->OptFlags);
		*track = NULL;
	}
	*track = s.p;
	/* the destructor of the stream, its memory moved to the node */
	Mem_Free(NULL);
}

/* maxss: b when unordered */
static float Anim_Max(float a, float b) {
	return a > b ? a : b;
}

static int Anim_PosNear(const vec3_t *a, const vec3_t *b, float eps) {
	return (double)eps > __builtin_fabs((double)(a->x - b->x)) && (double)eps > __builtin_fabs((double)(a->y - b->y)) &&
		(double)eps > __builtin_fabs((double)(a->z - b->z));
}

static int Anim_RotNear(const quat_t *a, const quat_t *b, float cosErr) {
	return !(cosErr > Quat_DotAbs(a, b)) || Quat_Similar(a, b);
}

static int Anim_FovNear(float a, float b, float eps) {
	return !(__builtin_fabs((double)(a - b)) > (double)eps);
}

static void AnimNodo_OptimizePos(CAnimNodo *self) {
	const vec3_t *p = self->Pos;
	int n = self->num_frames;
	vec3_t range = { 0.0f, 0.0f, 0.0f };

	for (int i = 0; i < n; i++) {
		float dz = Anim_Max(p[i].z - p[0].z, p[0].z - p[i].z);
		float dy = Anim_Max(p[i].y - p[0].y, p[0].y - p[i].y);
		float dx = Anim_Max(p[i].x - p[0].x, p[0].x - p[i].x);
		range.z = Anim_Max(range.z, dz);
		range.y = Anim_Max(range.y, dy);
		range.x = Anim_Max(range.x, dx);
	}
	vec3_t p0 = p[0];
	float eps = g_OptAnimPosErr;
	int lead = 0;
	while (lead < n && Anim_PosNear(&p[lead], &p0, eps)) {
		lead++;
	}
	if (lead == n) {
		AnimNodo_ToConst(self, 0, &p0);
	} else {
		int trail = 0;
		while (trail < n && Anim_PosNear(&p[n - 1 - trail], &p[n - 1], eps)) {
			trail++;
		}
		if (lead > 5 || trail > 5) {
			AnimNodo_ToStream(self, 0, n, lead, trail, 1);
		}
	}
	if (!(self->OptFlags & ANIM_POS)) {
		E3D_LogInfo(g_FileLog, "OptimizeAnim: ANIM_POS: [%s] %s", Vec3_Str(&range, 0, 6), g_AnimNodoName);
	}
}

static void AnimNodo_OptimizeRot(CAnimNodo *self) {
	float minDot = 1.0f;

	for (int i = 0; i < self->num_frames; i++) {
		if (!(Quat_DotAbs(&self->Rot[i], self->Rot) > minDot)) {
			minDot = Quat_DotAbs(&self->Rot[i], self->Rot);
		}
	}
	quat_t q0 = self->Rot[0];
	float cosErr = (float)Game_Cos((double)g_OptAnimRotErr * M_PI_D / DEG_PER_PI);
	int lead = 0;
	while (lead < self->num_frames && Anim_RotNear(&self->Rot[lead], &q0, cosErr)) {
		lead++;
	}
	if (lead == self->num_frames) {
		AnimNodo_ToConst(self, 1, &q0);
	} else {
		int trail = 0;
		while (trail < self->num_frames &&
			Anim_RotNear(&self->Rot[self->num_frames - 1 - trail], &self->Rot[self->num_frames - 1], cosErr)) {
			trail++;
		}
		if (lead > 5 || trail > 5) {
			AnimNodo_ToStream(self, 1, self->num_frames, lead, trail, 1);
		}
	}
	if (!(self->OptFlags & ANIM_ROT)) {
		double d = (double)minDot;
		E3D_LogInfo(g_FileLog, "OptimizeAnim: ANIM_ROT: [%f deg.] [%f cos] %s", Game_Acos(d) * 2.0 * DEG_PER_PI / M_PI_D, d,
			g_AnimNodoName);
	}
}

static void AnimNodo_OptimizeFov(CAnimNodo *self) {
	const float *v = self->FOV;
	int n = self->num_frames;
	float eps = (float)((double)g_OptAnimFovErr * M_PI_D / DEG_PER_PI);
	float f0 = v[0];
	int lead = 0;

	while (lead < n && Anim_FovNear(v[lead], f0, eps)) {
		lead++;
	}
	if (lead == n) {
		AnimNodo_ToConst(self, 2, &f0);
		return;
	}
	int trail = 0;
	while (trail < n && Anim_FovNear(v[n - 1 - trail], v[n - 1], eps)) {
		trail++;
	}
	if (lead > 5 || trail > 5) {
		AnimNodo_ToStream(self, 2, n, lead, trail, 0);
	}
}

void __fastcall CAnimNodo_Optimize(CAnimNodo *self) {
	CHECK(self->num_frames > 0);
	CHECK(!self->OptFlags);
	CHECK(!self->StmFlags);
	if (!g_OptimizeAnims) {
		return;
	}
	if (self->Pos) {
		AnimNodo_OptimizePos(self);
	}
	if (self->Rot) {
		AnimNodo_OptimizeRot(self);
	}
	if (self->FOV) {
		AnimNodo_OptimizeFov(self);
	}
}

uint32_t Vec3_Pack(const vec3_t *v) {
	int z = (int)v->z + 0x200;
	int x = 0x400 - (int)(v->x * ANIM_PACK_SCALE);
	int y = 0x400 - (int)(v->y * ANIM_PACK_SCALE);

	CHECK(x >= 0 && x <= 2047);
	CHECK(y >= 0 && y <= 2047);
	CHECK(z >= 0 && z <= 1023);
	return (((uint32_t)y & 0x7ff) | (uint32_t)x << 11) << 10 | ((uint32_t)z & 0x3ff);
}

/* n more bytes at the end, their place */
static byte *AnimStream_Grow(CAnimStream *self, int n) {
	self->len += n;
	self->p = Mem_Realloc(self->p, self->len);
	return self->p + self->len - n;
}

static void AnimStream_Word(CAnimStream *self, int w) {
	uint16_t v = w;

	memcpy(AnimStream_Grow(self, sizeof(v)), &v, sizeof(v));
	CHECK(self->len <= STREAM_MAXLEN);
}

void __thiscall AnimStream_AddByte(CAnimStream *self, int data) {
	CHECK(data >= -128 && data <= 127);
	*AnimStream_Grow(self, 1) = data;
	CHECK(self->len <= STREAM_MAXLEN);
}

void __thiscall AnimStream_AddBytes(CAnimStream *self, const void *data, int n) {
	CHECK(n != 0);
	memcpy(AnimStream_Grow(self, n), data, n);
	CHECK(self->len <= STREAM_MAXLEN);
}

void __thiscall AnimStream_AddHeader(CAnimStream *self, int start, int keys) {
	CHECK(start <= 0x7fff);
	AnimStream_Word(self, start);
	AnimStream_Word(self, start);
	AnimStream_Word(self, keys);
}

void __thiscall AnimStream_AddShort(CAnimStream *self, int data) {
	CHECK(data >= -32768 && data <= 32767);
	AnimStream_Word(self, data);
}

vec3_t *Quat_ToEulerNeg(vec3_t *out, const quat_t *q) {
	matrix_t m;
	vec3_t e;

	Quat_ToMatrixD3D(&m, q);
	Matrix_ToEuler(&e, &m);
	out->x = -e.x;
	out->y = -e.y;
	out->z = -e.z;
	return out;
}

void __thiscall CAnimNodo_WriteTrack(CAnimNodo *self, CArchIO *f, uint32_t flag, const void *ptr, int size, byte header) {
	if (!flag) {
		return;
	}
	CHECK(ptr);
	int n = (flag & self->OptFlags) ? 1 : self->num_frames;
	if (flag & (self->StmFlags & self->OptFlags)) {
		size = Stream_Word(ptr, 0);
		if (header) {
			CArchIO_WriteInt(f, size);
		}
	}
	if (!header) {
		CArchIO_Write(f, ptr, n * size);
	}
}

static void AnimNodo_WriteTracks(CAnimNodo *self, CArchIO *f, uint32_t flags, byte header) {
	CHECK((self->OptFlags & ~OPT_SUPPORTED) == 0);
	CHECK((self->StmFlags & ~STM_SUPPORTED) == 0);
	if (header) {
		CArchIO_WriteInt(f, self->primer_frame);
		CArchIO_WriteInt(f, self->num_frames);
		CArchIO_WriteInt(f, flags);
		CArchIO_WriteInt(f, self->OptFlags);
		CArchIO_WriteInt(f, self->StmFlags);
	}
	for (int i = 0; i < ANIM_TRACKS; i++) {
		CAnimNodo_WriteTrack(self, f, flags & animTracks[i].flag, *AnimNodo_Track(self, i), animTracks[i].size, header);
	}
}

void __thiscall CAnimNodo_WriteData(CAnimNodo *self, CArchIO *f) {
	AnimNodo_WriteTracks(self, f, CAnimNodo_GetFlags(self), 0);
}

void __thiscall CAnimNodo_WriteHeader(CAnimNodo *self, CArchIO *f) {
	CArchIO_BeginBlockWrite(f, "NAM");
	CArchIO_WriteInt(f, 1);
	uint32_t flags = CAnimNodo_GetFlags(self);
	AnimNodo_WriteTracks(self, f, flags, 1);
	EVA_Write(f, flags & ANIM_EVA, self->EVA);
	CArchIO_EndBlockWrite(f, "NAM");
}

void __thiscall CAnimacion_Write(CAnimacion *self, CArchIO *f) {
	int32_t fps;

	CArchIO_BeginBlockWrite(f, "ANI");
	CArchIO_WriteInt(f, 2);
	memcpy(&fps, &self->fps, sizeof(fps));
	CArchIO_WriteInt(f, fps);
	CArchIO_WriteInt(f, self->primer_frame);
	CArchIO_WriteInt(f, self->num_frames);
	CArchIO_WriteInt(f, self->num_objetos);
	CArchIO_WriteInt(f, self->flag != 0);
	CArchIO_WriteInt(f, self->tabla.n);
	CArchIO_Write(f, self->tabla.p, self->tabla.n);
	CArchIO_BeginBlockWrite(f, "NABK");
	CHECK(!self->AnimBlock.MemBlock);
	for (int i = 0; i < self->num_objetos; i++) {
		CAnimNodo *n = CAnimacion_GetNodo(self, i);
		CAnimNodo_Optimize(n);
		CAnimNodo_WriteData(n, f);
	}
	CArchIO_EndBlockWrite(f, "NABK");
	for (int i = 0; i < self->num_objetos; i++) {
		CAnimNodo_WriteHeader(CAnimacion_GetNodo(self, i), f);
	}
	CArchIO_EndBlockWrite(f, "ANI");
}

/* d folded once into -pi..pi */
static void AnimAng_Fold(float *out, float d) {
	if ((double)d > M_PI_D) {
		*out = d - M_2PI_F;
	}
	float e = *out;
	if ((double)e < -M_PI_D) {
		*out = e + M_2PI_F;
	}
}

vec3_t *Vec3_AngDiff(vec3_t *out, const vec3_t *a, const vec3_t *b) {
	float x = b->x - a->x;
	float y = b->y - a->y;
	float z = b->z - a->z;

	out->x = x;
	out->y = y;
	out->z = z;
	AnimAng_Fold(&out->x, x);
	AnimAng_Fold(&out->y, y);
	AnimAng_Fold(&out->z, z);
	return out;
}

int __thiscall CAnimNodo_FrameIndex(CAnimNodo *self, int frame, const void *ptr, uint32_t flag) {
	int rel = frame - self->primer_frame;

	CHECK((uint32_t)rel < self->num_frames);
	CHECK(ptr);
	CHECK((flag & self->OptFlags) == 0);
	CHECK((flag & self->StmFlags) == 0);
	return rel;
}

void __fastcall CAnimacion_Trim(CAnimacion *self) {
	CHECK(!self->AnimBlock.MemBlock);
	int n = self->num_objetos;
	while (n > 0 && !CAnimNodo_GetFlags(CAnimacion_GetNodo(self, n - 1))) {
		n--;
	}
	if (self->num_objetos == n) {
		return;
	}
	self->num_objetos = n;
	CHECK(self->num_objetos);
	CAnimNodo *nodos = New_Objects(self->num_objetos, sizeof(CAnimNodo), ANIMNODO_CTOR, ANIMNODO_DTOR);
	/* an empty temporary node, its destructor inline */
	g_AnimNodoCount++;
	for (int i = 0; i < self->num_objetos; i++) {
		nodos[i] = self->AnimNodo[i];
		memset(&self->AnimNodo[i], 0, sizeof(CAnimNodo));
	}
	if (self->AnimNodo) {
		Delete_Objects(self->AnimNodo, sizeof(CAnimNodo), ANIMNODO_DTOR);
		self->AnimNodo = NULL;
	}
	self->AnimNodo = nodos;
	g_AnimNodoCount--;
}

/* the values of a position track, one or num_frames */
static int AnimNodo_PosCount(CAnimNodo *self) {
	CHECK(!(self->StmFlags & ANIM_POS));
	return (self->OptFlags & ANIM_POS) ? 1 : self->num_frames;
}

byte __thiscall CAnimNodo_Translate(CAnimNodo *self, const vec3_t *d) {
	if (!self->Pos) {
		return 0;
	}
	int n = AnimNodo_PosCount(self);
	for (int i = 0; i < n; i++) {
		vec3_t *p = &self->Pos[i];
		p->x = p->x + d->x;
		p->y = p->y + d->y;
		p->z = d->z + p->z;
	}
	return 1;
}

void __thiscall CAnimNodo_FreeTracks(CAnimNodo *self, uint32_t flags) {
	CHECK(0);
	CHECK((flags & ~(ANIM_POS | ANIM_ROT)) == 0);
	self->OptFlags &= ~flags;
	self->StmFlags &= ~flags;
	if ((flags & ANIM_POS) && self->Pos) {
		AnimBlock_Free(self->Pos, self->OptFlags);
		self->Pos = NULL;
	}
	if ((flags & ANIM_ROT) && self->Rot) {
		AnimBlock_Free(self->Rot, self->OptFlags);
		self->Rot = NULL;
	}
}

static int Anim_SimilarFloat(float a, float b) {
	if (a == b) {
		return 1;
	}
	if (a == 0.0f || b == 0.0f) {
		return 0;
	}
	if (a > 0.0f && 0.0f > b) {
		return 0;
	}
	if (0.0f > a && b > 0.0f) {
		return 0;
	}
	double r = (double)(a / b);
	return !(r > ANIM_RATIO_MAX) && !(ANIM_RATIO_MIN > r);
}

byte Quat_Similar(const quat_t *a, const quat_t *b) {
	return Anim_SimilarFloat(a->x, b->x) && Anim_SimilarFloat(a->y, b->y) && Anim_SimilarFloat(a->z, b->z) &&
		Anim_SimilarFloat(a->w, b->w);
}

void __thiscall CAnimNodo_Scale(CAnimNodo *self, const vec3_t *s) {
	if (self->EVA) {
		CEVA_Scale(self->EVA, s->x, s->y, s->z);
	}
	if (!self->Pos) {
		return;
	}
	int n = AnimNodo_PosCount(self);
	for (int i = 0; i < n; i++) {
		vec3_t *p = &self->Pos[i];
		p->x = p->x * s->x;
		p->y = s->y * p->y;
		p->z = p->z * s->z;
	}
}

void __thiscall CAnimacion_Scale(CAnimacion *self, const vec3_t *s) {
	for (int i = 0; i < self->num_objetos; i++) {
		CAnimNodo_Scale(CAnimacion_GetNodo(self, i), s);
	}
}

void __thiscall CAnimNodo_SetEVA(CAnimNodo *self, int verts) {
	AnimNodo_DeleteEVA(self);
	self->EVA = AnimNodo_NewEVA();
	CEVA_Alloc(self->EVA, verts);
}

CAnimVert *__thiscall CAnimNodo_GetVert(CAnimNodo *self, uint32_t i) {
	if (self->EVA) {
		return CEVA_Get(self->EVA, i);
	}
	return NULL;
}

static void Anim_CheckSubframe(float subframe) {
	CHECK(subframe >= 0.0f && subframe < 1.0f);
}

byte __thiscall CAnimNodo_GetColor(CAnimNodo *self, vec3_t *color, float *intens, int frame, float subframe) {
	uint32_t rel = frame - self->primer_frame;

	CHECK(color && intens);
	Anim_CheckSubframe(subframe);
	CHECK(!(self->OptFlags & (ANIM_COLOR | ANIM_INTENS)));
	if (rel >= self->num_frames) {
		return 0;
	}
	CHECK(self->Color && self->Intens);
	const byte *c = self->Color + rel * 4;
	vec3_t v;
	v.x = (float)c[0] * ANIM_COLOR_SCALE;
	v.y = (float)c[1] * ANIM_COLOR_SCALE;
	v.z = (float)c[2] * ANIM_COLOR_SCALE;
	float i = self->Intens[rel];
	if (rel + 1 < self->num_frames) {
		v.x = ((float)c[4] * ANIM_COLOR_SCALE - v.x) * subframe + v.x;
		v.y = ((float)c[5] * ANIM_COLOR_SCALE - v.y) * subframe + v.y;
		v.z = ((float)c[6] * ANIM_COLOR_SCALE - v.z) * subframe + v.z;
		i = i + (self->Intens[rel + 1] - i) * subframe;
	}
	*color = v;
	*intens = i;
	return 1;
}

byte __thiscall CAnimNodo_GetFOV(CAnimNodo *self, float *fov, int frame, float subframe) {
	uint32_t rel = frame - self->primer_frame;

	CHECK(fov);
	Anim_CheckSubframe(subframe);
	if (rel >= self->num_frames) {
		return 0;
	}
	CHECK(self->FOV);
	if (!(self->OptFlags & ANIM_FOV)) {
		float v = self->FOV[rel];
		if (rel + 1 < self->num_frames) {
			v = v + (self->FOV[rel + 1] - v) * subframe;
		}
		*fov = v;
		return 1;
	}
	if (!(self->StmFlags & ANIM_FOV)) {
		*fov = *self->FOV;
		return 1;
	}
	const void *s = self->FOV;
	int keys = Stream_Word(s, 2);
	int k = (int)rel - Stream_Start(s);
	if (k < 0) {
		*fov = Stream_Float(s, 0, sizeof(float), 0);
	} else if (k + 1 < keys) {
		*fov = Stream_Float(s, k + 1, sizeof(float), 0) * subframe + (1.0f - subframe) * Stream_Float(s, k, sizeof(float), 0);
	} else {
		*fov = Stream_Float(s, keys - 1, sizeof(float), 0);
	}
	return 1;
}

byte __thiscall CAnimNodo_GetPosRotFrame(CAnimNodo *self, vec3_t *pos, quat_t *rot, int frame) {
	uint32_t rel = frame - self->primer_frame;

	CHECK(pos && rot);
	CHECK(!(self->OptFlags & (ANIM_POS | ANIM_ROT)));
	if (rel >= self->num_frames) {
		return 0;
	}
	CHECK(self->Pos && self->Rot);
	*pos = self->Pos[rel];
	*rot = self->Rot[rel];
	return 1;
}

static void AnimNodo_GetPos(CAnimNodo *self, vec3_t *pos, uint32_t rel, float subframe) {
	if (!(self->OptFlags & ANIM_POS)) {
		vec3_t p = self->Pos[rel];
		if (rel + 1 < self->num_frames) {
			const vec3_t *b = &self->Pos[rel + 1];
			p.x = (b->x - p.x) * subframe + p.x;
			p.y = (b->y - p.y) * subframe + p.y;
			p.z = (b->z - p.z) * subframe + p.z;
		}
		*pos = p;
		return;
	}
	if (!(self->StmFlags & ANIM_POS)) {
		*pos = *self->Pos;
		return;
	}
	const void *s = self->Pos;
	int k = (int)rel - Stream_Start(s);
	if (k < 0) {
		memcpy(pos, Stream_Key(s, 0, sizeof(vec3_t)), sizeof(vec3_t));
		return;
	}
	int keys = Stream_Word(s, 2);
	if (k + 1 < keys) {
		float t = 1.0f - subframe;
		pos->x = Stream_Float(s, k, sizeof(vec3_t), 0) * t + Stream_Float(s, k + 1, sizeof(vec3_t), 0) * subframe;
		pos->y = Stream_Float(s, k, sizeof(vec3_t), 1) * t + Stream_Float(s, k + 1, sizeof(vec3_t), 1) * subframe;
		pos->z = Stream_Float(s, k, sizeof(vec3_t), 2) * t + Stream_Float(s, k + 1, sizeof(vec3_t), 2) * subframe;
		return;
	}
	memcpy(pos, Stream_Key(s, keys - 1, sizeof(vec3_t)), sizeof(vec3_t));
}

static void AnimNodo_GetRot(CAnimNodo *self, quat_t *rot, uint32_t rel, float subframe) {
	if (!(self->OptFlags & ANIM_ROT)) {
		quat_t q = self->Rot[rel];
		if (rel + 1 < self->num_frames) {
			Quat_Slerp(&q, &q, &self->Rot[rel + 1], subframe);
		}
		*rot = q;
		return;
	}
	if (!(self->StmFlags & ANIM_ROT)) {
		*rot = *self->Rot;
		return;
	}
	const void *s = self->Rot;
	int keys = Stream_Word(s, 2);
	int k = (int)rel - Stream_Start(s);
	if (k < 0) {
		memcpy(rot, Stream_Key(s, 0, sizeof(quat_t)), sizeof(quat_t));
	} else if (k + 1 < keys) {
		const quat_t *a = (const quat_t *)Stream_Key(s, k, sizeof(quat_t));
		Quat_Slerp(rot, a, a + 1, subframe);
	} else {
		memcpy(rot, Stream_Key(s, keys - 1, sizeof(quat_t)), sizeof(quat_t));
	}
}

byte __thiscall CAnimNodo_GetPosRot(CAnimNodo *self, vec3_t *pos, quat_t *rot, int frame, float subframe) {
	uint32_t rel = frame - self->primer_frame;

	CHECK(pos && rot);
	Anim_CheckSubframe(subframe);
	if ((!self->Pos && !self->Rot) || rel >= self->num_frames) {
		return 0;
	}
	CHECK(self->Pos && self->Rot);
	AnimNodo_GetPos(self, pos, rel, subframe);
	AnimNodo_GetRot(self, rot, rel, subframe);
	return 1;
}

byte __thiscall CAnimNodo_GetVisible(CAnimNodo *self, byte *visible, int frame, float subframe) {
	uint32_t rel = frame - self->primer_frame;

	CHECK(visible);
	Anim_CheckSubframe(subframe);
	CHECK(!(self->OptFlags & ANIM_VISIBLE));
	if (!self->Visible || rel >= self->num_frames) {
		return 0;
	}
	int v = self->Visible[rel];
	if (rel + 1 < self->num_frames && self->Visible[rel + 1] != v) {
		int b = self->Visible[rel + 1];
		v = (int)X87_Fistp((double)((float)(b - v) * subframe + (float)v));
		CHECK((v & ~0xff) == 0);
	}
	*visible = v;
	return 1;
}

void __fastcall CAnimNodo_OptimizeEVA(CAnimNodo *self) {
	CHECK(self->EVA);
	CEVA_Optimize(self->EVA);
	if (!CEVA_NumAnimated(self->EVA)) {
		AnimNodo_DeleteEVA(self);
	}
}

void __thiscall CAnimacion_SwapNodos(CAnimacion *self, uint32_t i, uint32_t j) {
	CAnimNodo *a = CAnimacion_GetNodo(self, i);
	CAnimNodo *b = CAnimacion_GetNodo(self, j);
	CAnimNodo tmp;

	/* a temporary node, constructor and destructor inline */
	g_AnimNodoCount++;
	memset(&tmp.Pos, 0, sizeof(tmp) - offsetof(CAnimNodo, Pos));
	CAnimNodo_Init(&tmp, 0, 0, ANIM_POS | ANIM_ROT);
	CHECK(!CAnimNodo_GetFlags(&tmp));
	CHECK(!CAnimNodo_GetFlags(a));
	tmp = *a;
	*a = *b;
	*b = tmp;
	g_AnimNodoCount--;
	CAnimNodo_Init(&tmp, 0, 0, ANIM_POS | ANIM_ROT);
}
