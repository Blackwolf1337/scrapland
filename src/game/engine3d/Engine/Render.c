/*
 * Engine/render.cpp of the engine3d, the decompiled part: see include/scrap/render.h.
 */
#include <string.h>

#include "scrap/camara.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/escena.h"
#include "scrap/grafico2d.h"
#include "scrap/listavert.h"
#include "scrap/mallad3d.h"
#include "scrap/material.h"
#include "scrap/nodo3d.h"
#include "scrap/utild3d.h"
#include "scrap/varios.h"

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

byte Render_Clear(const box_t *rect, uint32_t flags, uint32_t color, float z, uint32_t stencil) {
	E3D_FlushBatch();
	CHECK((flags & D3DCLEAR_STENCIL) == 0);
	if (g_RenderOff) {
		return 1;
	}
	if (flags & D3DCLEAR_ZBUFFER && Render_StencilBits()) {
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

/* the time of the engine in ms as a float (unsigned through double) */
static float Time_Float(void) {
	return (float)(double)g_TimeMs;
}

static uint32_t F2U(float f) {
	uint32_t u;
	memcpy(&u, &f, sizeof(u));
	return u;
}

static void Render_SetTexTransform(uint32_t stage, const matrix_t *m) {
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_TEXTURE0 + stage, m);
}

void Render_PSConstAlpha(const CMaterial *mat, uint32_t reg) {
	static const float uno[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
	float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if ((!mat->params.transp || mat->params.atrib & MAT_1000) && !g_MatForzado) {
		memcpy(c, uno, sizeof(c));
	}
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetPixelShaderConstant_t, D3DDEV_SETPIXELSHADERCONSTANT)(dev, reg, c, 1);
}

void Mat_StageEnvBump(CMaterial *mat, uint32_t flags, uint32_t stageEnv, uint32_t stageBump) {
	float bias = 0.0f;
	if (flags & 5) {
		const CMapa *bump = mat->mapa[3];
		float s = bump->f2c * g_EnvBumpScale;
		if (bump->b55 && g_EnvBumpBias) {
			bias = s * -0.5f;
			s = (float)((double)s + (double)s);
		}
		float rot = bump->v20[2] != 0.0f ? Time_Float() * 0.001f * bump->v20[2] : 0.0f;
		Mat_StageBumpMat(mat, flags, stageBump, 0.0f, s, rot);
	}
	if (flags & 2) {
		Mat_StageBumpMat(mat, flags, stageBump, 0.0f, 0.0f, 0.0f);
	}
	Mat_StageEnvMap(mat, flags, stageEnv, bias);
}

void Mat_StageEnvMap(CMaterial *mat, uint32_t flags, uint32_t stage, float bias) {
	CHECK(g_UsarHWEnvMap);
	if (flags & 1) {
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		Render_TSS(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR);
	}
	if (flags & 5) {
		matrix_t m;
		Matrix_LoadIdentity(&m);
		const CMapa *env = mat->mapa[2];
		if (g_EnvMapViewDep == 2) {
			m = g_MatBillboard;
			m.m[12] = 0.0f;
			m.m[13] = 0.0f;
			m.m[14] = 0.0f;
			vec3_t s = { env->f18[0] * g_EnvMapScale, -env->f18[1] * g_EnvMapScale, g_EnvMapScale };
			Matrix_PostScale(&m, &s);
		} else {
			m.m[0] = env->f18[0] * g_EnvMapScale;
			m.m[5] = -(env->f18[1] * g_EnvMapScale);
		}
		m.m[12] = -g_EnvMapOffset - env->f10[0];
		m.m[13] = env->f10[0] + g_EnvMapOffset;
		if (g_EnvMapViewDep == 1) {
			matrix_t v;
			void *dev = g_pd3dDevice;
			D3D_METHOD(dev, d3dGetTransform_t, D3DDEV_GETTRANSFORM)(dev, D3DTS_VIEW, &v);
			vec3_t dir = { v.m[2], v.m[6], v.m[10] }, ang;
			Vec3_ToAngles(&ang, &dir);
			m.m[12] -= (float)((double)ang.y * M_1_PI_D);
			m.m[13] -= (float)((double)ang.x * M_1_PI_D);
		}
		m.m[12] += bias;
		m.m[13] += bias;
		Render_SetTexTransform(stage, &m);
	}
	if (flags & 2) {
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		Render_TSS(stage, D3DTSS_TEXCOORDINDEX, stage);
	}
}

void Render_Shader(uint32_t modo, void *a, void *b) {
	CHECK((byte)modo < MAX_SHADER_TAB);
	const ShaderTab *sh = modo < 0x100 ? &g_ShaderTab[modo] : &g_ShaderTab2[modo & 0xff];
	CHECK(sh->Index == (byte)modo);
	CHECK(sh->fnRender);
	sh->fnRender(a, b);
}

/* a random value in -r..r as rand() of the exe gives it */
static float Rand_Range(float r) {
	float nr = -r;
	return (float)((double)(crt_rand() & 0x7fff) * (1.0 / 32767.0) * (double)(r - nr) + (double)nr);
}

void Tex_StageJitter(void *unused, uint32_t flags, uint32_t stage, float ju, float jv, float scale) {
	(void)unused;
	if (flags & 1) {
		matrix_t m;
		Matrix_LoadIdentity(&m);
		m.m[0] = scale;
		m.m[5] = scale;
		m.m[8] = Rand_Range(ju);
		m.m[9] = Rand_Range(jv);
		Render_SetTexTransform(stage, &m);
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	}
	if (flags & 2) {
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	}
}

void Mat_StageScroll(CMaterial *mat, uint32_t idmapa, uint32_t flags, uint32_t stage, float k) {
	if (flags & 5) {
		CHECK(idmapa < MAPA_TOTAL);
		const CMapa *mapa = mat->mapa[idmapa];
		CHECK(mapa);
		if (mapa->f10[0] == 0.0f && mapa->f10[1] == 0.0f && mapa->v20[2] == 0.0f) {
			Tex_StageTransform(mat, 2, stage, 0.0f, 0.0f, 1.0f, 0.0f);
		} else {
			Tex_StageTransform(mat, flags, stage, mapa->f10[0], mapa->f10[1], mapa->f18[0], mapa->v20[2] * k);
		}
	}
	if (flags & 2) {
		Tex_StageTransform(mat, flags, stage, 0.0f, 0.0f, 1.0f, 0.0f);
	}
}

void Tex_StageTransform(void *unused, uint32_t flags, uint32_t stage, float su, float sv, float scale, float rot) {
	(void)unused;
	if (flags & 5) {
		matrix_t m;
		if (rot != 0.0f) {
			Matrix_RotationZ(&m, Time_Float() * 0.001f * rot);
			m.m[0] *= scale;
			m.m[1] *= scale;
			m.m[4] *= scale;
			m.m[5] *= scale;
		} else {
			Matrix_LoadIdentity(&m);
			m.m[0] = scale;
			m.m[5] = scale;
		}
		float t = Time_Float();
		float u = t * su * 0.001f;
		float v = t * sv * 0.001f;
		m.m[8] = u - (float)(int)u;
		m.m[9] = v - (float)(int)v;
		Render_SetTexTransform(stage, &m);
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	}
	if (flags & 2) {
		Render_TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	}
}

void Mat_StageBumpMat(CMaterial *mat, uint32_t flags, uint32_t stage, float k, float s, float rot) {
	if (!(flags & 5)) {
		return;
	}
	if (k != 0.0f) {
		s = mat->mapa[3]->f2c * k + s;
	}
	float c = 1.0f, sn = 0.0f;
	if (rot != 0.0f) {
		X87_SinCos(rot, &sn, &c);
	}
	Render_TSS(stage, D3DTSS_BUMPENVMAT00, F2U(c * s));
	Render_TSS(stage, D3DTSS_BUMPENVMAT01, F2U(sn * s));
	Render_TSS(stage, D3DTSS_BUMPENVMAT10, F2U(-(sn * s)));
	Render_TSS(stage, D3DTSS_BUMPENVMAT11, F2U(c * s));
}

void Render_D3DMaterial(const CMaterial *mat, const float *colors) {
	float m[17];
	const float *p = colors;
	if (g_MatForzado || g_DiffuseMod4x) {
		float k = !g_MatForzado || g_DiffuseMod4x ? 0.25f : 1.0f;
		for (int i = 0; i < 16; i++) {
			m[i] = i == 3 || (i >= 8 && i < 12) ? colors[i] : colors[i] * k;
		}
		m[16] = colors[16];
		if (g_MatForzado) {
			float a = mat->params.transp ? colors[3] : 1.0f;
			m[3] = (float)g_MatForzado->params.diffuse[3] / 255.0f * a;
		}
		p = m;
	}
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetMaterial_t, D3DDEV_SETMATERIAL)(dev, p);
}

const matrix_t *Render_GetProj(void) {
	return &g_MatProj;
}

const matrix_t *Render_GetView(void) {
	return &g_MatView;
}

const matrix_t *Render_GetBillboard(void) {
	return &g_MatBillboard;
}

const vec3_t *Render_GetEyePos(void) {
	return &g_EyePos;
}

static void Render_SetTexture(uint32_t stage, void *tex) {
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetTexture_t, D3DDEV_SETTEXTURE)(dev, stage, tex);
}

static void Render_State(uint32_t state, uint32_t value) {
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, state, value);
}

void Render_DrawTexRect(const box_t *rect, uint32_t cLeft, uint32_t cRight, void *tex) {
	if (g_RenderOff) {
		return;
	}
	void *saved = g_pd3dDevice;
	g_pd3dDevice = g_MotorDev;
	Render_ResetState();
	Render_SetTexture(0, tex);
	Render_TSS(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	Render_TSS(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	uint32_t op;
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dGetTextureStageState_t, D3DDEV_GETTEXTURESTAGESTATE)(dev, 0, D3DTSS_COLOROP, &op);
	Render_TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	Render_TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	Render_TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	uint32_t vp[6];
	dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dGetViewport_t, D3DDEV_GETVIEWPORT)(dev, vp);
	int r[4];
	Render_ClipRect(r, vp, rect);
	float x0 = (float)r[0] - 0.5f, y0 = (float)r[1] - 0.5f, x1 = (float)r[2] - 0.5f, y1 = (float)r[3] - 0.5f;
	const Vert2D v[4] = {
		{ x0, y0, 0.0f, 1.0f, cLeft, 0, 0.0f, 0.0f },
		{ x1, y0, 0.0f, 1.0f, cRight, 0, 1.0f, 0.0f },
		{ x0, y1, 0.0f, 1.0f, cLeft, 0, 0.0f, 1.0f },
		{ x1, y1, 0.0f, 1.0f, cRight, 0, 1.0f, 1.0f },
	};
	dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, D3DFVF_2D);
	dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vert2D));
	Render_TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	Render_TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	Render_TSS(0, D3DTSS_COLOROP, op);
	Render_ResetState();
	Render_SetTexture(0, NULL);
	g_pd3dDevice = saved;
}

void Render_FinD3D(void) {
	E3D_DebugPrintf("\n> RenderFinD3D()");
	g_D3DOn = 0;
	CHECK(!g_ViewportW);
	void *dev = g_MotorDev;
	if (dev) {
		for (uint32_t i = 0; i < 8; i++) {
			D3D_METHOD(dev, d3dSetTexture_t, D3DDEV_SETTEXTURE)(dev, i, NULL);
		}
		D3D_METHOD(dev, d3dSetPixelShader_t, D3DDEV_SETPIXELSHADER)(dev, 0);
	}
	for (int i = 0; g_PSNames[i]; i++) {
		if (g_PSHandles[i]) {
			if (dev) {
				D3D_METHOD(dev, d3dDeletePixelShader_t, D3DDEV_DELETEPIXELSHADER)(dev, g_PSHandles[i]);
			}
			g_PSHandles[i] = 0;
		}
	}
	RenderLock_ReleaseD3D();
	ListaVert_ReleaseD3D();
	RenderEVA_ReleaseD3D();
	RenderTarget_ReleaseD3D();
	if (dev) {
		RenderGamma_ReleaseD3D();
	}
	g_pd3dDevice = NULL;
	E3D_DebugPrintf("\n< RenderFinD3D()");
}

void Render_EndView(void) {
	E3D_FlushBatch();
	Render_SetMaterial(g_MatDefault, 0);
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetViewport_t, D3DDEV_SETVIEWPORT)(dev, g_ViewportFull);
	for (int i = 0; i < 5; i++) {
		g_Viewport[i] = 0;
	}
}

void Render_ClearForced(void) {
	E3D_FlushBatch();
	g_MatFX1 = NULL;
	g_MatFX2 = NULL;
	g_MatForzado = NULL;
	g_MatFX3 = NULL;
	g_MatActual = NULL;
}

const void *Render_GetCaps(void) {
	return g_Caps;
}

void Render_BeginFrame(void) {
	g_pd3dDevice = g_MotorDev;
	CHECK(g_pd3dDevice);
	g_RenderI1F4 = 0;
	RenderGamma_Begin();
	memcpy(g_Viewport, g_ViewportFull, 6 * sizeof(uint32_t));
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetViewport_t, D3DDEV_SETVIEWPORT)(dev, g_Viewport);
	Render_ResetState();
	Render_State(D3DRS_AMBIENT, g_AmbientD3D);
	Render_State(D3DRS_FOGENABLE, 0);
	Render_State(D3DRS_CULLMODE, g_CullMode ? D3DCULL_NONE : D3DCULL_CCW);
	g_PShaderCount = 0;
	RenderGamma_LogShaderMask();
}

int Render_DrawListaIdx(const CListaVert *v, const uint16_t *idx, int ntri, byte stats) {
	void *dev = g_MotorDev;
	if (!dev) {
		return E_FAIL;
	}
	if (stats) {
		g_RenderTris += ntri;
		g_RenderVerts += v->nVert;
	}
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, CListaVert_FVF((CListaVert *)v));
	return D3D_METHOD(dev, d3dDrawIndexedPrimitiveUP_t, D3DDEV_DRAWINDEXEDPRIMITIVEUP)(dev, D3DPT_TRIANGLELIST, 0,
		(uint32_t)v->nVert, (uint32_t)ntri, idx, D3DFMT_INDEX16, v->pVert, (uint32_t)CListaVert_Size((CListaVert *)v));
}

static int Render_WorldToDevice(const matrix_t *m) {
	void *dev = g_pd3dDevice;
	return D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_WORLD, m);
}

int Render_SetWorldMatrix(const matrix_t *m, byte defer) {
	g_WorldSet = 0;
	g_MatWorld = *m;
	if (!defer && g_pd3dDevice) {
		Render_WorldToDevice(&g_MatWorld);
		g_WorldSet = 1;
	}
	return 0;
}

int Render_SetWorldD3D(const matrix_t *m) {
	g_WorldSet = 0;
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	Render_WorldToDevice(m);
	return 0;
}

void Render_WorldDirty(void) {
	g_WorldSet = 0;
}

int Render_GetWorld(matrix_t *out) {
	if (out) {
		*out = g_MatWorld;
	}
	return 0;
}

int Render_SetWorld(void) {
	if (!g_WorldSet) {
		if (!g_pd3dDevice) {
			return E_FAIL;
		}
		Render_WorldToDevice(&g_MatWorld);
		g_WorldSet = 1;
	}
	return 0;
}

void Render_ResetState(void) {
	uint32_t filtro = g_TexFilter == 2 ? 3 : (uint32_t)(g_TexFilter != 0) + 1;
	g_FiltroMin = filtro;
	g_FiltroMag = filtro;
	g_FiltroMip = 2 - (uint32_t)(g_MipFilter != 1);
	uint32_t aniso = g_Anisotropy;
	if (MOTOR_MAXANISOTROPY(g_Motor) < aniso) {
		aniso = MOTOR_MAXANISOTROPY(g_Motor);
	}
	g_Anisotropia = aniso;
	for (uint32_t i = 0; i < 8; i++) {
		Render_SetTexture(i, NULL);
		Render_TSS(i, D3DTSS_TEXCOORDINDEX, i);
		Render_TSS(i, D3DTSS_MINFILTER, g_FiltroMin);
		Render_TSS(i, D3DTSS_MAGFILTER, g_FiltroMag);
		Render_TSS(i, D3DTSS_MIPFILTER, g_MipmapsOn ? g_FiltroMip : 0);
		Render_TSS(i, D3DTSS_MAXANISOTROPY, g_Anisotropia);
		Render_TSS(i, D3DTSS_MIPMAPLODBIAS, F2U(g_MipmapBias + g_MipmapBias2));
		Render_TSS(i, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		Render_TSS(i, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
		g_StageMip[i] = 1;
		g_StageTile[i] = 3;
	}
}

/* the shader mode of the material through its id (pass 0 only); the alpha test, the blending and its value from
 * the bytes 4, 5, 6 of the id */
static void Mat_Shader(const CMaterial *mat, byte frameChanged, byte *alphaTest, byte *blend, byte *alpha) {
	CMatId *id;
	CMaterial_GetId((CMaterial *)mat, &id);
	const byte *b = (const byte *)id;
	*alphaTest = b[4];
	*blend = b[5];
	*alpha = b[6];
	uint32_t mode = (uint32_t)id->id;
	if (g_LightmapMat) {
		mode += 0x100;
	}
	if (g_ShaderActual == mode && !frameChanged) {
		Render_Shader(mode, (void *)mat, (void *)4);
	} else {
		g_PShaderCount++;
		Render_Shader(g_ShaderActual, NULL, (void *)2);
		Render_Shader(mode, (void *)mat, (void *)1);
		g_ShaderActual = mode;
	}
}

/* the blending of pass 0 */
static void Mat_Blend(void *dev, const CMaterial *mat, byte shAlphaTest, byte shBlend, byte shAlpha) {
	byte alpha = shAlphaTest ? shAlpha : 0;
	byte blend = shBlend ? mat->params.transp : 0;
	int src = mat->params.srcBlend, dst = mat->params.dstBlend;
	byte at = mat->params.atrib >> 5 & 1;
	byte alphaTest = at ? 1 : alpha;
	byte blending = at ? 0 : blend;
	if (g_MatForzado) {
		src = g_MatForzado->params.srcBlend;
		if (!blending) {
			dst = g_MatForzado->params.dstBlend;
		}
		alphaTest = 0;
		blending = 1;
	}
	if (alphaTest && !(src == 2 && dst == 2)) {
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_ALPHATESTENABLE, 1);
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_ALPHAREF, at ? 0x80 : 1);
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
	} else {
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_ALPHATESTENABLE, 0);
	}
	if (mat->params.atrib & 0x400) {
		alphaTest = 0;
	}
	if (!alphaTest && !blending) {
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_ALPHABLENDENABLE, 0);
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_FOGENABLE,
			mat->params.w3e & 0xff ? (uint32_t)g_FogActive : 0);
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_COLORWRITEENABLE, 0xf);
		if (g_PSOn) {
			D3D_METHOD(dev, d3dSetTextureStageState_t, D3DDEV_SETTEXTURESTAGESTATE)(dev, 0, D3DTSS_ALPHAOP,
				D3DTOP_SELECTARG2);
		}
		return;
	}
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_ALPHABLENDENABLE, at ^ 1);
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_SRCBLEND, (uint32_t)src);
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_DESTBLEND, (uint32_t)dst);
	D3D_METHOD(dev, d3dSetTextureStageState_t, D3DDEV_SETTEXTURESTAGESTATE)(dev, 0, D3DTSS_ALPHAOP,
		!blending ? D3DTOP_SELECTARG1 : alphaTest ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
	byte fog = mat->params.w3e & 0xff && g_FogActive && g_AlphaFog && (g_AlphaFog != 1 || (src == 5 && dst == 6));
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_FOGENABLE, fog);
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_COLORWRITEENABLE,
		src == 2 && dst == 2 ? 0xf : 7);
}

/* pass 1: the second texture set added */
static void Mat_Pass1(void *dev) {
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_ALPHATESTENABLE, 0);
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_ALPHABLENDENABLE, 1);
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_SRCBLEND, 2);
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_DESTBLEND, 2);
}

int Render_SetMaterial(void *m, int pass) {
	if (!m) {
		CHECK(pass == -1);
		g_MatActual = NULL;
		return 0;
	}
	const CMaterial *mat = g_MatFX1 ? g_MatFX1 : m;
	if (g_UseDefMaterial && !(mat->params.atrib & 0x840)) {
		mat = g_UseDefMaterial == 1 ? g_MatBlanco : g_MatDef4;
	}
	void *dev = g_MotorDev;
	if (!dev) {
		return E_FAIL;
	}
	if (g_MatActual != mat || g_MatPass != pass || g_MatFrame != g_FrameCount || g_LightmapMat != g_MatCtx) {
		byte frameChanged = g_MatFrame != g_FrameCount;
		g_MatActual = mat;
		g_MatPass = pass;
		g_MatFrame = g_FrameCount;
		g_MatCtx = g_LightmapMat;
		if (g_MaterialLog) {
			const char *ctx = g_LightmapMat ? *(const char **)((byte *)g_LightmapMat + 4) : Game_Ptr(0x3a3228);
			E3D_LogWhite(g_FileLog, "MatLog: %d %d %d [%s] [%s]", g_FrameCount, g_MatChanges,
				CMaterial_GetId((CMaterial *)mat, NULL), mat->Nombre, ctx);
		}
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_ZWRITEENABLE, mat->params.w3e >> 8);
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_ZFUNC, (uint32_t)mat->params.i40);
		Render_State(D3DRS_ZBIAS, mat->params.atrib & 0x800 ? g_DecalZBias : 0);
		uint32_t light = 0;
		if (mat->params.b39) {
			Render_D3DMaterial(mat, g_MatFX3 ? g_MatFX3->colors : mat->colors);
			light = 1;
		}
		D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_LIGHTING, light);
		byte shAlphaTest = 1, shBlend = 1, shAlpha = 0;
		if (g_UsarShaders) {
			CHECK(!pass);
			Mat_Shader(mat, frameChanged, &shAlphaTest, &shBlend, &shAlpha);
		} else if (mat->params.b3b && pass == 1) {
			D3D_METHOD(dev, d3dSetTextureStageState_t, D3DDEV_SETTEXTURESTAGESTATE)(dev, 0, D3DTSS_TEXCOORDINDEX, 1);
		} else {
			D3D_METHOD(dev, d3dSetTextureStageState_t, D3DDEV_SETTEXTURESTAGESTATE)(dev, 0, D3DTSS_TEXCOORDINDEX, 0);
		}
		if (!pass) {
			Mat_Blend(dev, mat, shAlphaTest, shBlend, shAlpha);
		} else if (pass == 1) {
			Mat_Pass1(dev);
		}
		g_MatChanges++;
	}
	if (pass) {
		if (pass == 1) {
			Render_SetMapa(0, mat->mapa[2]);
		}
		return 0;
	}
	/* R_UseDefTexture: the white texture (1: only without one) */
	CMapa *mapa = mat->mapa[0];
	switch (g_UseDefTexture) {
	case 1:
		if (!mapa) {
			mapa = g_MatBlanco->mapa[0];
		}
		break;
	case 2:
		if (!(mat->params.atrib & 0x40)) {
			mapa = g_MatBlanco->mapa[0];
		}
		break;
	case 3:
		if (!(mat->params.atrib & 0x40)) {
			mapa = g_MatDef2->mapa[0];
		}
		break;
	case 4:
		if (!(mat->params.atrib & 0x40)) {
			mapa = g_MatDef3->mapa[0];
		}
		break;
	}
	Render_SetMapa(0, mapa);
	return 0;
}

/* the part x0..x1, y0..y1 (-0.5..0.5, y up) of the view through proj */
static void Proj_Sub(matrix_t *out, const matrix_t *proj, float x0, float x1, float y0, float y1) {
	matrix_t m = *proj;
	m.m[0] = proj->m[0] / (x1 - x0);
	m.m[5] = proj->m[5] / (y0 - y1);
	m.m[8] = (x1 + x0) / (x0 - x1);
	m.m[9] = (y1 + y0) / (y1 - y0);
	*out = m;
}

byte Render_ProjTile(const matrix_t *proj) {
	byte on = g_TileOn;
	if (g_TileState < 2) {
		if (!g_TileState && !on) {
			return 0;
		}
		g_TileState = 1;
		if (on) {
			return 0;
		}
		g_TileOn = on = 1;
	}
	int n = g_Tiles;
	g_TileState = 2;
	if (!on) {
		if (++g_TileX == n) {
			g_TileX = 0;
			if (++g_TileY == n) {
				g_TileY = 0;
				g_TileState = 0;
				return 0;
			}
		}
		g_TileOn = 1;
	}
	float fn = (float)n;
	float x0 = (float)g_TileX / fn - 0.5f;
	float x1 = (float)(g_TileX + 1) / fn - 0.5f;
	float y0 = -((float)g_TileY / fn - 0.5f);
	float y1 = -((float)(g_TileY + 1) / fn - 0.5f);
	Proj_Sub(&g_MatProj, proj, x0, x1, y0, y1);
	CHECK(g_pd3dDevice);
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_PROJECTION, &g_MatProj);
	return 1;
}

int Render_SetProj(const matrix_t *m) {
	g_MatProj = *m;
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_PROJECTION, &g_MatProj);
	return 0;
}

void Render_SetCull(uint32_t mode) {
	if (g_CullMode && mode == D3DCULL_CCW) {
		mode = D3DCULL_NONE;
	}
	Render_State(D3DRS_CULLMODE, mode);
}

void Render_SetLightmapMat(CMaterial *mat, int mode) {
	if (mat && g_MatFX2) {
		mat = g_MatFX2;
	}
	CHECK(!g_LightmapMat || !mat);
	if (mat) {
		mat->Refs++;
	}
	if (g_LightmapMat) {
		g_LightmapMat->Refs--;
	}
	g_LightmapOp = D3DTOP_MODULATE;
	g_LightmapMat = mat;
	if (mode == 2) {
		g_LightmapOp = D3DTOP_MODULATE2X;
	} else if (mode == 4) {
		g_LightmapOp = D3DTOP_MODULATE4X;
	}
}

typedef void(__thiscall *fxTexUpdate_t)(void *fx, CMapa *mapa);
typedef void *(__thiscall *fxTexGet_t)(void *fx);

int Render_SetMapa(uint32_t stage, CMapa *mapa) {
	CHECK(g_pd3dDevice);
	CHECK(stage < 8);
	void *tex = NULL;
	if (mapa) {
		if (mapa->AnimMat) {
			uint32_t f = (uint32_t)CAnimMat_GetFrame(mapa->AnimMat);
			mapa->cur = f < (uint32_t)mapa->frames ? &mapa->TextAni[f] : &mapa->est;
		}
		if (mapa->FXTextura) {
			void *fx = mapa->FXTextura;
			((fxTexUpdate_t)(*(void ***)fx)[1])(fx, mapa);
			fx = mapa->FXTextura;
			tex = ((fxTexGet_t)(*(void ***)fx)[2])(fx);
		}
		if (mapa->b8 != g_StageMip[stage]) {
			g_StageMip[stage] = mapa->b8;
			Render_TSS(stage, D3DTSS_MIPFILTER, mapa->b8 && g_MipmapsOn ? g_FiltroMip : 0);
		}
		if (mapa->bc != g_StageTile[stage]) {
			g_StageTile[stage] = mapa->bc;
			Render_TSS(stage, D3DTSS_ADDRESSU, (~((uint32_t)mapa->bc * 2) & 2) | 1);
			Render_TSS(stage, D3DTSS_ADDRESSV, (~(uint32_t)mapa->bc & 2) | 1);
		}
		Render_TSS(stage, D3DTSS_MAXANISOTROPY, mapa->lightmap ? 1 : g_Anisotropia);
		if (!tex) {
			tex = CMapa_GetTexture(mapa, 1);
		}
	}
	if (g_StageTex[stage] != tex || g_StageTexFrame[stage] != g_FrameCount) {
		g_StageTexFrame[stage] = g_FrameCount;
		g_StageTex[stage] = tex;
		Render_SetTexture(stage, tex);
		if (tex) {
			g_TexChanges++;
		}
	}
	return 0;
}

/* the vertex format, the stream and the indices of a draw */
static void Render_Stream(void *dev, void *vb, uint32_t fvf, int size, void *ib, uint32_t baseVert) {
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, fvf);
	D3D_METHOD(dev, d3dSetStreamSource_t, D3DDEV_SETSTREAMSOURCE)(dev, 0, vb, (uint32_t)size);
	if (ib) {
		D3D_METHOD(dev, d3dSetIndices_t, D3DDEV_SETINDICES)(dev, ib, baseVert);
	}
}

int Render_DrawVBIBFmt(void *vb, void *ib, int frmt, uint32_t numVerts, uint32_t ntri, uint32_t baseVert,
	uint32_t startTri) {
	void *dev = g_pd3dDevice;
	if (!dev) {
		return E_FAIL;
	}
	Render_Stream(dev, vb, ListaVert_FormatFVF(frmt), ListaVert_FormatSize(frmt), ib, baseVert);
	D3D_METHOD(dev, d3dDrawIndexedPrimitive_t, D3DDEV_DRAWINDEXEDPRIMITIVE)(dev, D3DPT_TRIANGLELIST, 0, numVerts,
		startTri * 3, ntri);
	return 0;
}

int Render_DrawMallaD3D(const CMallaD3D *m) {
	void *dev = g_pd3dDevice;
	if (!dev) {
		return E_FAIL;
	}
	if (m->StripList) {
		CHECK(m->StripList > 2);
		CHECK(m->pVertVB && !m->pVertIB);
		CHECK(!m->BaseIndex && !m->StartIndex);
		Render_Stream(dev, m->pVertVB, ListaVert_FormatFVF(m->vbFrmt), ListaVert_FormatSize(m->vbFrmt), NULL, 0);
		D3D_METHOD(dev, d3dDrawPrimitive_t, D3DDEV_DRAWPRIMITIVE)(dev, D3DPT_TRIANGLESTRIP, 0,
			(uint32_t)m->StripList - 2);
		return 0;
	}
	CHECK(m->pVertVB && m->pVertIB);
	uint32_t ntri = m->nTri;
	if (g_VBIBTriLimit < ntri) {
		ntri = g_VBIBTriLimit;
	}
	Render_Stream(dev, m->pVertVB, ListaVert_FormatFVF(m->vbFrmt), ListaVert_FormatSize(m->vbFrmt), m->pVertIB,
		(uint32_t)m->BaseIndex);
	D3D_METHOD(dev, d3dDrawIndexedPrimitive_t, D3DDEV_DRAWINDEXEDPRIMITIVE)(dev, D3DPT_TRIANGLELIST, 0,
		(uint32_t)m->vert.nVert, (uint32_t)m->StartIndex * 3, ntri);
	return 0;
}

int Render_DrawVBIB(void *vb, void *ib, const CListaVert *v, uint32_t ntri) {
	void *dev = g_MotorDev;
	if (!dev) {
		return E_FAIL;
	}
	Render_Stream(dev, vb, CListaVert_FVF((CListaVert *)v), CListaVert_Size((CListaVert *)v), ib, 0);
	D3D_METHOD(dev, d3dDrawIndexedPrimitive_t, D3DDEV_DRAWINDEXEDPRIMITIVE)(dev, D3DPT_TRIANGLELIST, 0,
		(uint32_t)v->nVert, 0, ntri);
	return 0;
}

int Render_SetView(const matrix_t *m) {
	matrix_t inv;
	g_MatView = *m;
	CHECK(Matrix_Invert(&inv, &g_MatView) == 0);
	g_MatBillboard = inv;
	g_EyePos = (vec3_t){ inv.m[12], inv.m[13], inv.m[14] };
	if (!g_pd3dDevice) {
		return E_FAIL;
	}
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_VIEW, &g_MatView);
	return 0;
}

void E3D_FlushBatch(void) {
	if (g_ViewportW) {
		RenderMallaD3D_Flush();
		RenderMallaD3D_Flush2();
		RenderObjBase3D_Flush();
	}
}

byte Box_IsVisible(const matrix_t *m, const box_t *box, const char *name, float scale) {
	box_t wb;
	Box_Transform(&wb, box, m);
	vec3_t c = { (wb.min.x + wb.max.x) * 0.5f, (wb.max.y + wb.min.y) * 0.5f, (wb.max.z + wb.min.z) * 0.5f };
	float dx = wb.max.x - wb.min.x, dy = wb.max.y - wb.min.y, dz = wb.max.z - wb.min.z;
	float r = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)) * 0.5f * scale;
	if (!(r > 0.0f)) {
		vec3_t pos = { m->m[12], m->m[13], m->m[14] };
		/* the center x where the radius is meant, as the original */
		E3D_LogBlue(g_FileLog, "ERROR: Invalid radius: %f  Model: %s  Pos: %s", (double)c.x, name, Vec3_Str(&pos, 0, 6));
		CHECK(r > 0.0f);
	}
	int out;
	Frustum_TestSpheres(&c, &r, 1, &out);
	if (out) {
		return 0;
	}
	if (g_ShowSphereVis) {
		Debug_DrawBoxM(m, &box->min, &box->max, 0xffffff);
		Debug_DrawBox(&wb.min, &wb.max, 0xffff00);
		Debug_DrawSphere(&c, r, 0xff00, 10, -1);
	}
	return 1;
}

byte Frustum_SegVisible(const vec3_t *a, const vec3_t *b) {
	return Frustum_TestSeg(a, b) == 0;
}

byte Sph_IsVisible(const vec3_t *center, float radius, int arg) {
	(void)arg;
	vec3_t c = *center;
	CHECK(!(radius <= 0.0f));
	int out;
	Frustum_TestSpheres(&c, &radius, 1, &out);
	if (out) {
		return 0;
	}
	if (g_ShowSphereVis > 1) {
		Debug_DrawSphere(&c, radius, 0xff, 10, -1);
	}
	return 1;
}

/* n bytes of src into a new buffer (a vertex or index buffer, the same methods) */
static int Buf_Fill(void *buf, const void *src, uint32_t lockSize, uint32_t n) {
	void *p;
	if (D3D_METHOD(buf, d3dLock_t, D3DBUF_LOCK)(buf, 0, lockSize, &p, 0) < 0) {
		return E_FAIL;
	}
	memcpy(p, src, n);
	D3D_METHOD(buf, d3dUnlock_t, D3DBUF_UNLOCK)(buf);
	return 0;
}

int Render_CreateIB(void **ib, const void *tris, int nTri, byte dynamic) {
	CHECK(ib);
	CHECK(tris);
	CHECK(nTri > 0);
	uint32_t n = (uint32_t)nTri * 6;
	void *dev = g_MotorDev;
	if (D3D_METHOD(dev, d3dCreateIB_t, D3DDEV_CREATEIB)(dev, n, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
		dynamic ? D3DPOOL_SYSTEMMEM : D3DPOOL_DEFAULT, ib) < 0) {
		return E_FAIL;
	}
	return Buf_Fill(*ib, tris, n, n);
}

int Render_CreateVB(void **vb, CListaVert *vert, byte dynamic) {
	CHECK(vb);
	CHECK(vert);
	CHECK(vert->nVert > 0);
	int n = vert->nVert;
	void *dev = g_MotorDev;
	uint32_t fvf = CListaVert_FVF(vert);
	if (D3D_METHOD(dev, d3dCreateVB_t, D3DDEV_CREATEVB)(dev, (uint32_t)(CListaVert_Size(vert) * n), D3DUSAGE_WRITEONLY,
		fvf, dynamic ? D3DPOOL_SYSTEMMEM : D3DPOOL_DEFAULT, vb) < 0) {
		return E_FAIL;
	}
	return Buf_Fill(*vb, vert->pVert, 0, (uint32_t)(CListaVert_Size(vert) * vert->nVert));
}

const char *Render_VersionStr(uint32_t v) {
	Bprintf(g_VersionStr, 0x40, "%d.%d", v >> 8 & 0xff, v & 0xff);
	return g_VersionStr;
}

void Render_SetCamara(CCamara *cam) {
	matrix_t m;
	Render_SetView(CCamara_GetView(cam, &m));
	g_MatProj = *CCamara_GetProj(cam, &m);
	void *dev = g_pd3dDevice;
	if (dev) {
		D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_PROJECTION, &g_MatProj);
	}
	Frustum_Calc(&g_MatView, &g_MatProj);
}

byte Render_ClipRect(int *out, const uint32_t *vp, const box_t *rect) {
	CHECK(out);
	int w = (int)vp[2], h = (int)vp[3];
	float fw = (float)(double)vp[2], fx = (float)(double)vp[0];
	float fh = (float)(double)vp[3], fy = (float)(double)vp[1];
	int x0 = (int)((double)(fw * rect->min.x + fx) + 0.5);
	int y0 = (int)((double)(rect->min.y * fh + fy) + 0.5);
	int x1 = (int)((double)(rect->max.x * fw + fx) + 0.5);
	int y1 = (int)((double)(rect->max.y * fh + fy) + 0.5);
	x0 = x0 > 0 ? (x0 >= w ? w : x0) : 0;
	y0 = y0 > 0 ? (y0 >= h ? h : y0) : 0;
	x1 = x1 > 0 ? (x1 >= w ? w : x1) : 0;
	y1 = y1 > 0 ? (y1 >= h ? h : y1) : 0;
	if (x1 > x0 && y1 > y0) {
		out[0] = x0;
		out[1] = y0;
		out[2] = x1;
		out[3] = y1;
		return 1;
	}
	out[2] = x0;
	out[0] = x0;
	out[1] = y0;
	out[3] = y0;
	return 0;
}

void Render_NoIndexedBlend(void *dev) {
	D3D_METHOD(dev, d3dSetRenderState_t, D3DDEV_SETRENDERSTATE)(dev, D3DRS_INDEXEDVERTEXBLENDENABLE, 0);
}

byte Render_SetViewRect(const box_t *rect, byte proj) {
	E3D_FlushBatch();
	void *dev = g_pd3dDevice;
	if (!rect) {
		matrix_t m = g_MatProj;
		D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_PROJECTION, &m);
		dev = g_pd3dDevice;
		D3D_METHOD(dev, d3dSetViewport_t, D3DDEV_SETVIEWPORT)(dev, g_Viewport);
		return 1;
	}
	uint32_t vp[6];
	memcpy(vp, g_Viewport, sizeof(vp));
	int w = (int)vp[2], h = (int)vp[3];
	int r[4];
	Render_ClipRect(r, vp, rect);
	vp[2] = (uint32_t)(r[2] - r[0]);
	vp[3] = (uint32_t)(r[3] - r[1]);
	vp[0] = (uint32_t)r[0];
	vp[1] = (uint32_t)r[1];
	dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetViewport_t, D3DDEV_SETVIEWPORT)(dev, vp);
	if (proj) {
		float fw = (float)w, fh = (float)h;
		matrix_t m;
		Proj_Sub(&m, &g_MatProj, (float)r[0] / fw - 0.5f, (float)r[2] / fw - 0.5f, -((float)r[1] / fh - 0.5f),
			-((float)r[3] / fh - 0.5f));
		dev = g_pd3dDevice;
		D3D_METHOD(dev, d3dSetTransform_t, D3DDEV_SETTRANSFORM)(dev, D3DTS_PROJECTION, &m);
	}
	return 1;
}

int Render_GetViewportH(void) {
	CHECK(g_ViewportH);
	return g_ViewportH;
}

int Render_GetViewportW(void) {
	CHECK(g_ViewportW);
	return g_ViewportW;
}

int Render_StencilBits(void) {
	switch (MOTOR_ZFORMAT(g_Motor)) {
	case D3DFMT_D15S1:
		return 1;
	case D3DFMT_D24S8:
		return 8;
	case D3DFMT_D24X4S4:
		return 4;
	default:
		return 0;
	}
}

int Render_Line(const vec3_t *a, const vec3_t *b, uint32_t color) {
	if (!g_MotorDev) {
		return E_FAIL;
	}
	if (g_DiffuseMod4x) {
		color = (color >> 2 & 0x3f3f3f) | (color & 0xff000000);
	}
	const struct {
		vec3_t p;
		uint32_t color;
	} v[2] = { { *a, color }, { *b, color } };
	Render_SetMaterial(g_MatLinea, 0);
	void *dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, D3DFVF_XYZ | D3DFVF_DIFFUSE);
	dev = g_pd3dDevice;
	D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_LINELIST, 1, v, sizeof(v[0]));
	return 0;
}

int Render_ToScreen(const vec3_t *pos, float *xy) {
	CHECK(g_pd3dDevice);
	CHECK(xy);
	matrix_t vp, m;
	m = *Matrix_MulD3D(&vp, &g_MatView, &g_MatProj);
	vec3_t p;
	if (Vec3_TransformH(&p, pos, &m)) {
		return -1;
	}
	if (0.02 > (double)p.z || p.z > 1.0f || __builtin_fabs((double)p.x) > 1.0 || __builtin_fabs((double)p.y) > 1.0) {
		return -1;
	}
	float w = (float)(double)g_Viewport[2], x = (float)(double)g_Viewport[0];
	xy[0] = (w * p.x + x) * 0.5f + w * 0.5f;
	float h = (float)(double)g_Viewport[3], y = (float)(double)g_Viewport[1];
	xy[1] = h * 0.5f - (h * p.y + y) * 0.5f;
	return 0;
}
