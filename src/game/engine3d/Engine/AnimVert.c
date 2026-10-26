/*
 * Vertex animation of the engine3d, original source Engine/AnimVert.cpp (include/scrap/animvert.h).
 */
#include "scrap/animvert.h"

#define ANIMVERT_CTOR 0x2d33f0
#define ANIMVERT_DTOR 0x2d3440
/* the compressed offsets are -127..127 steps */
#define ANIMVERT_STEPS 127.0f
/* frames closer than this to the first one are the same */
#define ANIMVERT_EPS 1e-4

void __fastcall CAnimVert_FreeTr(CAnimVert *self) {
	if (!self->Tr) {
		return;
	}
	CHECK(self->nTr > 0);
	operator_delete_array(self->Tr);
	self->Tr = NULL;
	if (!self->Tr2) {
		self->nTr = 0;
	}
}

void __fastcall CAnimVert_FreeTr2(CAnimVert *self) {
	if (!self->Tr2) {
		return;
	}
	CHECK(self->nTr > 0);
	operator_delete_array(self->Tr2);
	self->Tr2 = NULL;
	if (!self->Tr) {
		self->nTr = 0;
	}
}

void __fastcall CAnimVert_dtor(CAnimVert *self) {
	CAnimVert_FreeTr(self);
	CAnimVert_FreeTr2(self);
}

void *__thiscall CAnimVert_Delete(CAnimVert *self, uint32_t flags) {
	if (flags & 2) {
		uint32_t *mem = (uint32_t *)self - 1;
		crt_eh_vector_dtor(self, sizeof(*self), *mem, Game_Ptr(ANIMVERT_DTOR));
		if (flags & 1) {
			operator_delete_array(mem);
		}
		return mem;
	}
	CAnimVert_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void __thiscall CAnimVert_AllocTr(CAnimVert *self, int frames) {
	CHECK(!self->Tr);
	CHECK(frames > 0);
	self->nTr = frames;
	self->Tr = New_Array(frames, sizeof(vec3_t));
}

void __fastcall CAnimVert_AllocTr2(CAnimVert *self) {
	CHECK(!self->Tr2);
	CHECK(self->nTr > 0);
	self->Tr2 = New_Array(self->nTr, 3);
}

vec3_t *__thiscall CAnimVert_Frame(CAnimVert *self, uint32_t frame) {
	CHECK(self->Tr);
	CHECK(self->nTr > 0);
	CHECK(frame < self->nTr);
	return &self->Tr[frame];
}

/* a + (b - a) * t of compressed steps, times esc */
static float AnimVert_Step(int a, int b, float t, float esc) {
	return ((float)(b - a) * t + (float)a) * esc;
}

void __thiscall CAnimVert_GetPos(CAnimVert *self, uint32_t frame, float subframe, vec3_t *pos) {
	CHECK(self->Tr || self->Tr2);
	CHECK(self->nTr > 0);
	CHECK(pos);
	CHECK((int)frame >= 0);
	CHECK(subframe >= 0.0f && subframe < 1.0f);
	if (frame >= self->nTr) {
		return;
	}
	if (!self->Tr) {
		const int8_t *c = self->Tr2 + frame * 3;
		float e = self->esc;
		if (frame < self->nTr - 1) {
			pos->x = AnimVert_Step(c[0], c[3], subframe, e) + pos->x;
			pos->y = AnimVert_Step(c[1], c[4], subframe, e) + pos->y;
			pos->z = AnimVert_Step(c[2], c[5], subframe, e) + pos->z;
		} else {
			pos->x = (float)c[0] * e + pos->x;
			pos->y = (float)c[1] * e + pos->y;
			pos->z = (float)c[2] * e + pos->z;
		}
		return;
	}
	vec3_t a = self->Tr[frame];
	if (frame < self->nTr - 1) {
		const vec3_t *b = &self->Tr[frame + 1];
		a.x = (b->x - a.x) * subframe + a.x;
		a.y = (b->y - a.y) * subframe + a.y;
		a.z = (b->z - a.z) * subframe + a.z;
	}
	*pos = a;
}

void __thiscall CAnimVert_AddPos(CAnimVert *self, uint32_t frame1, uint32_t frame2, float t, float *pos) {
	CHECK(self->Tr2);
	if (frame1 != frame2) {
		CHECK(frame1 < self->nTr);
		CHECK(frame2 < self->nTr);
		const int8_t *a = self->Tr2 + frame1 * 3;
		const int8_t *b = self->Tr2 + frame2 * 3;
		for (int i = 0; i < 3; i++) {
			pos[i] = AnimVert_Step(a[i], b[i], t, self->esc) + pos[i];
		}
		return;
	}
	CHECK(frame1 < self->nTr);
	const int8_t *a = self->Tr2 + frame1 * 3;
	for (int i = 0; i < 3; i++) {
		pos[i] = (float)a[i] * self->esc + pos[i];
	}
}

void __thiscall CAnimVert_Scale(CAnimVert *self, float x, float y, float z) {
	if (self->Tr) {
		for (uint32_t f = 0; f < self->nTr; f++) {
			vec3_t *v = CAnimVert_Frame(self, f);
			v->x = v->x * x;
			v->y = v->y * y;
			v->z = v->z * z;
		}
	}
	if (self->Tr2) {
		CHECK(x == y && x == z);
		self->esc = self->esc * x;
	}
}

static float AnimVert_Dist(float a, float b) {
	return (float)__builtin_fabs((double)(a - b));
}

void __fastcall CAnimVert_Compress(CAnimVert *self) {
	float m = 0.0f;
	vec3_t base = *CAnimVert_Frame(self, 0);

	for (uint32_t f = 1; f < self->nTr; f++) {
		const vec3_t *v = CAnimVert_Frame(self, f);
		float dx = AnimVert_Dist(v->x, base.x);
		float dy = AnimVert_Dist(v->y, base.y);
		float dz = AnimVert_Dist(v->z, base.z);
		/* maxss: the second operand when unordered */
		float d = dy > dx ? dy : dx;
		d = dz > d ? dz : d;
		if (!(m > d)) {
			m = d;
		}
	}
	self->esc = m / ANIMVERT_STEPS;
	CAnimVert_AllocTr2(self);
	float k = ANIMVERT_STEPS / m;
	for (uint32_t f = 0; f < self->nTr; f++) {
		const vec3_t *v = CAnimVert_Frame(self, f);
		int8_t *c = self->Tr2 + f * 3;
		c[0] = (int8_t)(int)((v->x - base.x) * k);
		c[1] = (int8_t)(int)((v->y - base.y) * k);
		c[2] = (int8_t)(int)((v->z - base.z) * k);
	}
	CAnimVert_FreeTr(self);
}

void __fastcall CAnimVert_Optimize(CAnimVert *self) {
	const vec3_t *base = CAnimVert_Frame(self, 0);
	uint32_t f;

	for (f = 1; f < self->nTr; f++) {
		const vec3_t *v = CAnimVert_Frame(self, f);
		if (ANIMVERT_EPS < __builtin_fabs((double)(v->x - base->x)) ||
			ANIMVERT_EPS < __builtin_fabs((double)(v->y - base->y)) ||
			ANIMVERT_EPS < __builtin_fabs((double)(v->z - base->z))) {
			break;
		}
	}
	if (f == self->nTr) {
		CAnimVert_FreeTr(self);
	} else {
		CAnimVert_Compress(self);
	}
}

void AnimVert_Read(CArchIO *f, CAnimVert **pAV) {
	CHECK(*pAV);
	CHECK(!(*pAV)->Tr && !(*pAV)->Tr2);
	if (!CArchIO_ReadInt(f)) {
		return;
	}
	CAnimVert *av = *pAV;
	CHECK(!av->Tr);
	CHECK(!av->Tr2);
	av->nTr = CArchIO_ReadInt(f);
	CAnimVert_AllocTr2(av);
	CArchIO_Read(f, &av->esc, sizeof(av->esc));
	CArchIO_Read(f, av->Tr2, av->nTr * 3);
}

void AnimVert_Write(CArchIO *f, CAnimVert *av) {
	if (!av->Tr && !av->Tr2) {
		CArchIO_WriteInt(f, 0);
		return;
	}
	CArchIO_WriteInt(f, 1);
	CHECK(!av->Tr);
	CHECK(av->Tr2);
	CArchIO_Write(f, &av->nTr, sizeof(av->nTr));
	CArchIO_Write(f, &av->esc, sizeof(av->esc));
	CArchIO_Write(f, av->Tr2, av->nTr * 3);
}

void __fastcall CEVA_Free(CEVA *self) {
	if (!self->AnimV) {
		return;
	}
	CHECK(self->nV > 0);
	if (self->AnimV) {
		Delete_Objects(self->AnimV, sizeof(CAnimVert), ANIMVERT_DTOR);
		self->AnimV = NULL;
	}
	self->nV = 0;
}

void __thiscall CEVA_Alloc(CEVA *self, int verts) {
	CHECK(!self->AnimV);
	CHECK(verts > 0);
	self->nV = verts;
	self->AnimV = New_Objects(verts, sizeof(CAnimVert), ANIMVERT_CTOR, ANIMVERT_DTOR);
}

CAnimVert *__thiscall CEVA_Get(CEVA *self, uint32_t i) {
	CHECK(self->AnimV);
	CHECK(i < self->nV);
	return &self->AnimV[i];
}

void __thiscall CEVA_Scale(CEVA *self, float x, float y, float z) {
	for (uint32_t i = 0; i < self->nV; i++) {
		CAnimVert_Scale(CEVA_Get(self, i), x, y, z);
	}
}

int __fastcall CEVA_MemSize(CEVA *self) {
	int size = sizeof(*self);

	if (!self->AnimV) {
		return size;
	}
	for (uint32_t i = 0; i < self->nV; i++) {
		const CAnimVert *av = CEVA_Get(self, i);
		int n = sizeof(*av);
		if (av->Tr) {
			n = av->nTr * sizeof(vec3_t) + sizeof(*av);
		}
		if (av->Tr2) {
			n += av->nTr * 3;
		}
		size += n;
	}
	return size;
}

int __fastcall CEVA_NumAnimated(CEVA *self) {
	int n = 0;

	if (!self->AnimV) {
		return 0;
	}
	for (uint32_t i = 0; i < self->nV; i++) {
		const CAnimVert *av = CEVA_Get(self, i);
		if (av->Tr || av->Tr2) {
			n++;
		}
	}
	return n;
}

void __fastcall CEVA_Optimize(CEVA *self) {
	int animated = 0;

	CHECK(self->AnimV);
	for (uint32_t i = 0; i < self->nV; i++) {
		CAnimVert_Optimize(CEVA_Get(self, i));
		const CAnimVert *av = CEVA_Get(self, i);
		if (av->Tr || av->Tr2) {
			animated = 1;
		}
	}
	if (!animated) {
		CEVA_Free(self);
	}
}

void EVA_Read(CArchIO *f, int flag, CEVA **pEVA) {
	CHECK(pEVA);
	CHECK(*pEVA == NULL);
	if (!flag) {
		return;
	}
	CArchIO_BeginBlock(f, "EVA");
	CHECK(CArchIO_ReadInt(f) == 1);
	CEVA *eva = operator_new(sizeof(*eva));
	if (eva) {
		eva->nV = 0;
		eva->AnimV = NULL;
	}
	CEVA_Alloc(eva, CArchIO_ReadInt(f));
	for (uint32_t i = 0; i < eva->nV; i++) {
		CAnimVert *av = CEVA_Get(eva, i);
		AnimVert_Read(f, &av);
	}
	CHECK(eva->nV);
	CHECK(CEVA_NumAnimated(eva));
	*pEVA = eva;
	CArchIO_Error(f, "EVA");
}

void EVA_Write(CArchIO *f, int flag, CEVA *eva) {
	CHECK(!flag == !eva);
	if (!flag) {
		return;
	}
	CArchIO_BeginBlockWrite(f, "EVA");
	CArchIO_WriteInt(f, 1);
	CArchIO_WriteInt(f, eva->nV);
	for (uint32_t i = 0; i < eva->nV; i++) {
		AnimVert_Write(f, CEVA_Get(eva, i));
	}
	CHECK(eva->nV);
	CHECK(CEVA_NumAnimated(eva));
	CArchIO_EndBlockWrite(f, "EVA");
}
