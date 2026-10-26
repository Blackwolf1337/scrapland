/*
 * Engine/render.cpp of the engine3d, the decompiled part: see include/scrap/render.h.
 */
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"

/* the off bit of pixel shader i in g_PSOff */
#define PS_OFF(i) (0x200000u << (i))

static void Render_TSS(uint32_t stage, uint32_t type, uint32_t value) {
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetTextureStageState_t, D3DDEV_SETTEXTURESTAGESTATE)(dev, stage, type, value);
}

static void Render_PS(int i, void *arg, byte flags) {
	if (!g_PSOn || !g_PS[i] || g_PSOff & PS_OFF(i)) {
		Render_PSFixed(arg, flags);
		return;
	}
	void *dev = g_pd3dDevice;
	if (flags & 1) {
		D3D_METHOD(dev, d3dSetPixelShader_t, D3DDEV_SETPIXELSHADER)(dev, g_PS[i]);
	}
	if (flags & 2) {
		dev = g_pd3dDevice;
		D3D_METHOD(dev, d3dSetPixelShader_t, D3DDEV_SETPIXELSHADER)(dev, 0);
	}
}

void Render_PS0(void *arg, byte flags) {
	Render_PS(0, arg, flags);
}

void Render_PS1(void *arg, byte flags) {
	Render_PS(1, arg, flags);
}

void Render_PS2(void *arg, byte flags) {
	Render_PS(2, arg, flags);
}

void Render_PS3(void *arg, byte flags) {
	Render_PS(3, arg, flags);
}

void Render_PS4(void *arg, byte flags) {
	Render_PS(4, arg, flags);
}

void Render_PS5(void *arg, byte flags) {
	Render_PS(5, arg, flags);
}

void Render_PS6(void *arg, byte flags) {
	Render_PS(6, arg, flags);
}

void Render_PS7(void *arg, byte flags) {
	Render_PS(7, arg, flags);
}

void Render_PS8(void *arg, byte flags) {
	Render_PS(8, arg, flags);
}

void Render_PSFixed(void *arg, byte flags) {
	(void)arg;
	if (flags & 1) {
		byte mod4x = g_DiffuseMod4x != 0;
		Render_TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		Render_TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		Render_TSS(0, D3DTSS_COLOROP, mod4x ? D3DTOP_MODULATE4X : D3DTOP_MODULATE);
	}
	if (flags & 2) {
		Render_TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		Render_TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		Render_TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	}
}

/* the distance of p to frustum plane i */
static float Frustum_Dist(int i, const vec3_t *p) {
	const plane_t *pl = &g_Frustum[i];
	return pl->n.x * p->x + pl->n.y * p->y + pl->n.z * p->z + pl->d;
}

int Frustum_TestSeg(const vec3_t *a, const vec3_t *b) {
	float da[MAX_PLANOS_FRUSTUM], db[MAX_PLANOS_FRUSTUM];
	uint32_t outA = 0, outB = 0;

	for (int i = 0; i < MAX_PLANOS_FRUSTUM; i++) {
		uint32_t bit = 1u << i;
		da[i] = Frustum_Dist(i, a);
		db[i] = Frustum_Dist(i, b);
		if (da[i] > 0.0f) {
			outA |= bit;
		}
		if (db[i] > 0.0f) {
			outB |= bit;
		}
		if (outA & bit & outB) {
			return i + 1;
		}
	}
	if (!outA || !outB) {
		return 0;
	}
	/* the crossings of the planes only one end is outside of */
	uint32_t cross = outA ^ outB;
	for (int i = 0; i < MAX_PLANOS_FRUSTUM; i++) {
		uint32_t bit = 1u << i;
		if (!(bit & cross)) {
			continue;
		}
		float t = da[i] / (da[i] - db[i]);
		vec3_t p = { a->x + (b->x - a->x) * t, (b->y - a->y) * t + a->y, (b->z - a->z) * t + a->z };
		if (!Frustum_PlaneOutside(&p, bit)) {
			return 0;
		}
	}
	return 10;
}

int Frustum_PlaneOutside(const vec3_t *p, uint32_t skip) {
	for (int i = 0; i < MAX_PLANOS_FRUSTUM; i++) {
		if (!(skip & 1u << i) && Frustum_Dist(i, p) > 0.0f) {
			return i + 1;
		}
	}
	return 0;
}

void Frustum_TestSpheres(const vec3_t *centers, const float *radius, int n, int *out) {
	for (int k = 0; k < n; k++) {
		out[k] = 0;
		for (int i = 0; i < MAX_PLANOS_FRUSTUM; i++) {
			if (Frustum_Dist(i, &centers[k]) > radius[k]) {
				out[k] = i + 1;
				break;
			}
		}
	}
}

void __fastcall CRenderIB_Reset(CRenderIB *self) {
	CHECK(!self->pVertIB);
	self->Total = 0;
	self->Used = 0;
}

void __fastcall CRenderVB_Reset(CRenderVB *self) {
	CHECK(!self->pVertVB);
	self->FVF = 0;
	self->Size = 0;
	self->Used = 0;
	self->Total = 0;
}

/* n bytes of src at off of the buffer */
static void Buf_Write(void *buf, uint32_t off, const void *src, uint32_t n) {
	void *p;
	CHECK(D3D_METHOD(buf, d3dLock_t, D3DBUF_LOCK)(buf, off, n, &p, 0) >= 0);
	memcpy(p, src, n);
	D3D_METHOD(buf, d3dUnlock_t, D3DBUF_UNLOCK)(buf);
}

void __thiscall CRenderIB_Add(CRenderIB *self, int numtri, const uint16_t *idx) {
	CHECK(self->pVertIB);
	CHECK(numtri > 0);
	CHECK(self->Used + numtri <= self->Total);
	Buf_Write(self->pVertIB, (uint32_t)self->Used * 6, idx, (uint32_t)numtri * 6);
	self->Used += numtri;
}

void __thiscall CRenderVB_Add(CRenderVB *self, int numvert, const void *verts) {
	CHECK(self->pVertVB);
	CHECK(numvert > 0);
	CHECK(self->Used + numvert <= self->Total);
	Buf_Write(self->pVertVB, (uint32_t)(self->Used * self->Size), verts, (uint32_t)(self->Size * numvert));
	self->Used += numvert;
}

/* the buffer (AddRef) at 0 and its used count at 8 of both kinds */
static void *Buf_Get(void *const *buf, const int *used, int *out) {
	*out = *used;
	COM_ADDREF(*buf);
	return *buf;
}

void *__thiscall CRenderIB_Get(CRenderIB *self, int *used) {
	return Buf_Get(&self->pVertIB, &self->Used, used);
}

void *__thiscall CRenderVB_Get(CRenderVB *self, int *used) {
	return Buf_Get(&self->pVertVB, &self->Used, used);
}

void __thiscall CRenderIB_Create(CRenderIB *self, uint32_t numtri) {
	CHECK(!self->pVertIB);
	CHECK(numtri < 64000);
	self->Total = (int)numtri;
	self->Used = 0;
	if (numtri) {
		void *dev = g_pd3dDevice;
		CHECK(D3D_METHOD(dev, d3dCreateIB_t, D3DDEV_CREATEIB)(dev, numtri * 6, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
			D3DPOOL_DEFAULT, &self->pVertIB) >= 0);
	}
}

void __thiscall CRenderVB_Create(CRenderVB *self, uint32_t numvert, int size, uint32_t fvf) {
	CHECK(!self->pVertVB);
	CHECK(numvert < 64000);
	self->Total = (int)numvert;
	self->Used = 0;
	self->Size = size;
	self->FVF = fvf;
	if (numvert) {
		void *dev = g_pd3dDevice;
		CHECK(D3D_METHOD(dev, d3dCreateVB_t, D3DDEV_CREATEVB)(dev, numvert * (uint32_t)size, D3DUSAGE_WRITEONLY, fvf,
			D3DPOOL_DEFAULT, &self->pVertVB) >= 0);
	}
}

byte Render_Clear(const int *rect, uint32_t flags, uint32_t color, float z, uint32_t stencil) {
	if (g_ViewportW) {
		RenderMallaD3D_Flush();
		RenderMallaD3D_Flush2();
		RenderObjBase3D_Flush();
	}
	CHECK((flags & D3DCLEAR_STENCIL) == 0);
	if (g_RenderOff) {
		return 1;
	}
	if (flags & D3DCLEAR_ZBUFFER && Render_HasStencil()) {
		flags |= D3DCLEAR_STENCIL;
	}
	int r[4];
	if (rect) {
		Render_ClipRect(r, g_Viewport, rect);
	}
	void *dev = g_pd3dDevice;
	return D3D_METHOD(dev, d3dClear_t, D3DDEV_CLEAR)(dev, rect != NULL, rect ? r : NULL, flags, color, z, stencil) == 0;
}

void Render_GetViewportSize(int *ancho, int *alto) {
	CHECK(g_ViewportW && g_ViewportH);
	CHECK(ancho && alto);
	*ancho = g_ViewportW;
	*alto = g_ViewportH;
}
