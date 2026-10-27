/*
 * Engine/RenderEVA.cpp of the engine3d: the meshes with a vertex animation or a skin and the particles, see the
 * RenderEVA part of include/scrap/render.h.
 */
#include "scrap/animvert.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/global.h"
#include "scrap/grafico3d.h"
#include "scrap/listavert.h"
#include "scrap/mallad3d.h"
#include "scrap/modelo.h"
#include "scrap/render.h"
#include "scrap/sistpart.h"

#define D3DFVF_TEX1 0x112		/* XYZ | NORMAL | TEX1 */
#define D3DFVF_PART 0x1c2		/* XYZ | DIFFUSE | SPECULAR | TEX1 */
#define NODO_NO_CASTSHADOWS (1u << 5)

static void Dev_SetVertexShader(void *dev, uint32_t fvf) {
	D3D_METHOD(dev, d3dSetVertexShader_t, D3DDEV_SETVERTEXSHADER)(dev, fvf);
}

static void Dev_SetStream(void *dev, void *vb, uint32_t stride) {
	D3D_METHOD(dev, d3dSetStreamSource_t, D3DDEV_SETSTREAMSOURCE)(dev, 0, vb, stride);
}

static void Dev_SetIndices(void *dev, void *ib, uint32_t base) {
	D3D_METHOD(dev, d3dSetIndices_t, D3DDEV_SETINDICES)(dev, ib, base);
}

static void Dev_DrawIndexed(void *dev, uint32_t numVerts, uint32_t ntri) {
	D3D_METHOD(dev, d3dDrawIndexedPrimitive_t, D3DDEV_DRAWINDEXEDPRIMITIVE)(dev, D3DPT_TRIANGLELIST, 0, numVerts, 0,
		ntri);
}

static void Buf_Release(void **buf) {
	if (*buf) {
		COM_RELEASE(*buf);
		*buf = NULL;
	}
}

static int Buf_Lock(void *buf, uint32_t off, uint32_t size, void **data, uint32_t flags) {
	return D3D_METHOD(buf, d3dLock_t, D3DBUF_LOCK)(buf, off, size, data, flags);
}

static void Buf_Unlock(void *buf) {
	D3D_METHOD(buf, d3dUnlock_t, D3DBUF_UNLOCK)(buf);
}

int RenderEVA_InitD3D(void) {
	CHECK(g_Motor);
	CHECK(g_pd3dDevice);
	RenderEVA_FreeParticles();
	Buf_Release(&g_EVAVB);
	void *dev = g_pd3dDevice;
	g_EVAVBUsed = 0;
	if (D3D_METHOD(dev, d3dCreateVB_t, D3DDEV_CREATEVB)(dev, MAX_VERT_VB * sizeof(CVertMalla),
		D3DUSAGE_WRITEONLY_DYNAMIC, D3DFVF_TEX1, D3DPOOL_DEFAULT, &g_EVAVB) < 0) {
		return E_FAIL;
	}
	return RenderEVA_CreateParticles() < 0 ? E_FAIL : 0;
}

int RenderEVA_CreateParticles(void) {
	CHECK(!g_PartTri);
	g_PartNumTri = VB_PARTICLES * 2;
	uint16_t *t = New_Array(VB_PARTICLES * 6, sizeof(uint16_t));
	g_PartTri = t;
	for (int i = 0; i < VB_PARTICLES; i++, t += 6) {
		uint16_t v = (uint16_t)(i * 4);
		t[0] = v;
		t[1] = v + 1;
		t[2] = v + 3;
		t[3] = v + 3;
		t[4] = v + 2;
		t[5] = v;
	}
	if (Render_CreateIB(&g_PartIB, g_PartTri, g_PartNumTri, 0) < 0) {
		return E_FAIL;
	}
	void *dev = g_pd3dDevice;
	if (D3D_METHOD(dev, d3dCreateVB_t, D3DDEV_CREATEVB)(dev, VB_PARTICLES * 4 * sizeof(Grafico3DVert),
		D3DUSAGE_WRITEONLY_DYNAMIC, D3DFVF_PART, D3DPOOL_DEFAULT, &g_PartVB) < 0) {
		return E_FAIL;
	}
	g_PartVBUsed = 0;
	return 0;
}

int RenderEVA_FreeParticles(void) {
	if (g_PartTri) {
		/* a scalar delete of the array in the original */
		operator_delete_sized(g_PartTri, sizeof(uint16_t));
		g_PartTri = NULL;
	}
	Buf_Release(&g_PartVB);
	Buf_Release(&g_PartIB);
	return 0;
}

int RenderEVA_ReleaseD3D(void) {
	RenderEVA_FreeParticles();
	Buf_Release(&g_EVAVB);
	return 0;
}

int RenderEVA_LockParticles(int nvert, void **data) {
	CHECK(nvert < VB_PARTICLES * 4);
	if (g_ParticlesLock) {
		g_PartVBUsed = 0;
		return Buf_Lock(g_PartVB, 0, g_ParticlesLock > 1 ? (uint32_t)nvert * sizeof(Grafico3DVert) : 0, data,
			D3DLOCK_DISCARD);
	}
	if (g_PartVBUsed + nvert > VB_PARTICLES * 4) {
		g_PartVBUsed = 0;
		return Buf_Lock(g_PartVB, 0, (uint32_t)nvert * sizeof(Grafico3DVert), data, D3DLOCK_DISCARD);
	}
	return Buf_Lock(g_PartVB, (uint32_t)g_PartVBUsed * sizeof(Grafico3DVert), (uint32_t)nvert * sizeof(Grafico3DVert),
		data, D3DLOCK_NOOVERWRITE);
}

int RenderEVA_LockSkin(int nvert, void **data) {
	CHECK(nvert < MAX_VERT_VB);
	if (g_EVAVBUsed + nvert > MAX_VERT_VB) {
		g_EVAVBUsed = 0;
		return Buf_Lock(g_EVAVB, 0, (uint32_t)nvert << 5, data, D3DLOCK_DISCARD);
	}
	return Buf_Lock(g_EVAVB, (uint32_t)g_EVAVBUsed << 5, (uint32_t)nvert << 5, data, D3DLOCK_NOOVERWRITE);
}

/* the mesh of the node of m, which holds the shadow data of the list */
static CMallaD3D *Malla_Main(CMallaD3D *m) {
	return Obj3D_Cast(m->nodo->obj3D, CLASE_OBJ3D_MALLAD3D);
}

/* drawn now: not in the shadow pass and the model seen; the shadow: in the passes 1 and 11 when the main mesh has
 * the positions */
static byte Malla_DrawNow(const CNodoR *r) {
	return g_RenderPass != 0xb && ((const CModelo *)r->owner)->visible & 1;
}

static byte Malla_Shadow(const CMallaD3D *main) {
	return (g_RenderPass == 1 || g_RenderPass == 0xb) && main->pPos;
}

/* the drawn vertices of a mesh from the vertex buffer */
static void Malla_DrawVB(const CMallaD3D *m, int n) {
	CHECK(m->pVertIB);
	Dev_SetVertexShader(g_pd3dDevice, D3DFVF_TEX1);
	Dev_SetStream(g_pd3dDevice, g_EVAVB, sizeof(CVertMalla));
	Dev_SetIndices(g_pd3dDevice, m->pVertIB, (uint32_t)g_EVAVBUsed);
	Dev_DrawIndexed(g_pd3dDevice, (uint32_t)n, m->nTri);
}

/* the CHECKs of the shadow data of the main mesh */
static void Malla_CheckShadow(const CMallaD3D *main) {
	CHECK(!(main->nodo->flags & NODO_NO_CASTSHADOWS));
	CHECK(main->pSeg);
	CHECK(main->pPos);
}

/* the planes of the triangles of m from the moved positions of the main mesh, at the place of m in the list */
static void Malla_TriPlanes(const CMallaD3D *m, CMallaD3D *main) {
	int n = (m->next ? m->next->triBase : (int)main->nSeg) - m->triBase;

	for (int i = 0; i < n; i++) {
		CHECK((uint32_t)i < m->nTri);
		CHECK(m->pTri);
		const uint16_t *t = m->pTri[i];
		const vec3_t *pos = main->pPos;
		vec3_t a = pos[m->vertOrig[t[0]]], b = pos[m->vertOrig[t[1]]], c = pos[m->vertOrig[t[2]]];
		vec3_t e1 = { b.x - a.x, b.y - a.y, b.z - a.z };
		vec3_t e2 = { c.x - a.x, c.y - a.y, c.z - a.z };
		vec3_t nn = { e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x };
		MallaTriPlane *pl = (MallaTriPlane *)main->pSeg + (m->triBase + i);
		pl->n = nn;
		pl->d = -(nn.y * a.y + nn.x * a.x + nn.z * a.z);
	}
}

int RenderEVA_DrawSkinNodo(CMallaD3D *m, CNodoR *r, void *skin) {
	CEVA *eva = skin;
	CMallaD3D *main = Malla_Main(m);
	CHECK(!main->next);
	byte draw = Malla_DrawNow(r);
	byte shadow = Malla_Shadow(main);
	if (!draw && !shadow) {
		return 0;
	}
	if (!g_UseEVA) {
		Render_DrawMallaD3D(m);
		return 0;
	}
	CHECK(eva->AnimV);
	CHECK(m->vertOrig);
	int n = m->vert.nVert;
	CVertMalla *dst;
	int hr = RenderEVA_LockSkin(n, (void **)&dst);
	if (hr < 0) {
		return hr;
	}
	CHECK(dst);
	const CVertMalla *src = (const CVertMalla *)CListaVert_Tex1(&m->vert, 0);
	CAnimSlot *slot = CModelo_GetSlot((CModelo *)r->owner, r->slot);
	float bl = slot->blend;
	byte s1 = slot->subset, s2 = slot->prevSubset;
	float w = (3.0f - (bl + bl)) * (bl * bl);
	for (int i = 0; i < n; i++, src++, dst++) {
		CAnimVert *av = CEVA_Get(eva, m->vertOrig[i]);
		vec3_t pos = src->pos;
		if (av->Tr || av->Tr2) {
			CAnimVert_AddPos(av, s1, s2, w, &pos.x);
		}
		dst->pos = pos;
		dst->n = src->n;
		dst->uv = src->uv;
		if (shadow) {
			main->pPos[m->vertOrig[i]] = pos;
		}
		if (g_ShowEVA) {
			uint32_t color = av->Tr || av->Tr2 ? 0xff0000 : 0xff00;
			vec3_t e = { pos.x + src->n.x, pos.y + src->n.y, pos.z + src->n.z };
			Render_Line(&pos, &e, color);
		}
	}
	Buf_Unlock(g_EVAVB);
	if (draw) {
		Malla_DrawVB(m, n);
	}
	g_EVAVBUsed += n;
	if (shadow) {
		Malla_CheckShadow(main);
		Malla_TriPlanes(m, main);
		CHECK(!main->next);
		Stencil_Malla(&main->vert, main->pTri, main->nTri, main, r);
	}
	return 0;
}

int RenderEVA_DrawSkin(CMallaD3D *m, CNodoR *r) {
	static const uint32_t colors[4] = { 0xff00ff, 0xff00, 0xff, 0xff0000 };

	CHECK(m && r);
	CHECK(m->pSkin && m->vertOrig);
	CMallaD3D *main = Malla_Main(m);
	byte draw = Malla_DrawNow(r);
	byte shadow = Malla_Shadow(main);
	if (!draw && !shadow) {
		return 0;
	}
	if (!g_UseSkin && m->pVertVB) {
		Render_SetWorldD3D(&r->mObj);
		Render_DrawMallaD3D(m);
		return 0;
	}
	CHECK(m->vert.frmt == VERT_TEX1);
	/* in node space the positions are made relative to the render node and the world matrix moves them back */
	vec3_t off = { 0.0f, 0.0f, 0.0f };
	if (g_SkinNodeSpace) {
		off = (vec3_t){ -r->m.m[12], -r->m.m[13], -r->m.m[14] };
	}
	matrix_t world;
	Matrix_LoadIdentity(&world);
	world.m[12] = -off.x;
	world.m[13] = -off.y;
	world.m[14] = -off.z;
	Render_SetWorldD3D(&world);
	m->ibc = g_FrameCount;
	m->ib8 = r;
	int n = m->vert.nVert;
	CVertMalla *dst = NULL;
	if (draw) {
		int hr = RenderEVA_LockSkin(n, (void **)&dst);
		if (hr < 0) {
			return hr;
		}
		CHECK(dst);
	}
	const CVertMalla *src = (const CVertMalla *)CListaVert_Tex1(&m->vert, 0);
	CModelo *mod = (CModelo *)r->owner;
	CHECK(mod && mod->nodosR);
	for (int i = 0; i < n; i++, src++) {
		vec3_t v = src->pos, vn = src->n;
		vec3_t p = { 0.0f, 0.0f, 0.0f }, pn = { 0.0f, 0.0f, 0.0f };
		/* the count of bones, up to 3 bones, their weights */
		const byte *sk = m->pSkin + m->vertOrig[i] * 16;
		const float *wt = (const float *)(sk + 4);
		const CNodoR *bones = mod->nodosR->p;
		for (int j = 0; j < sk[0]; j++) {
			const float *b = bones[sk[1 + j]].mObj.m;
			float wj = wt[j];
			p.x = p.x + ((b[0] * v.x + b[4] * v.y + b[8] * v.z) + (b[12] + off.x)) * wj;
			p.y = p.y + ((b[5] * v.y + b[1] * v.x + b[9] * v.z) + (b[13] + off.y)) * wj;
			p.z = p.z + ((b[6] * v.y + b[2] * v.x + b[10] * v.z) + (b[14] + off.z)) * wj;
			if (draw) {
				pn.x = (b[0] * vn.x + b[4] * vn.y + b[8] * vn.z) * wj + pn.x;
				pn.y = pn.y + (b[5] * vn.y + b[1] * vn.x + b[9] * vn.z) * wj;
				pn.z = pn.z + (b[6] * vn.y + b[2] * vn.x + b[10] * vn.z) * wj;
			}
		}
		if (draw) {
			dst->pos = p;
			dst->n = pn;
			dst->uv = src->uv;
		}
		if (shadow) {
			main->pPos[m->vertOrig[i]] = p;
		}
		if (g_ShowSkin) {
			vec3_t e = { pn.x + p.x, pn.y + p.y, pn.z + p.z };
			Render_Line(&p, &e, colors[sk[0]]);
		}
		if (draw) {
			dst++;
		}
	}
	if (draw) {
		Buf_Unlock(g_EVAVB);
		Malla_DrawVB(m, n);
	}
	g_EVAVBUsed += n;
	if (shadow) {
		Malla_CheckShadow(main);
		Malla_TriPlanes(m, main);
		/* the shadow volume once all meshes of the list are moved for r in this frame */
		for (const CMallaD3D *mm = main; mm; mm = mm->next) {
			if (mm->ibc != g_FrameCount || mm->ib8 != r) {
				return 0;
			}
		}
		Stencil_Malla(&main->vert, main->pTri, main->nTri, main, r);
	}
	return 0;
}

void Particula_Quad(CParticula *p, const matrix_t *bill, const matrix_t *view, Grafico3DVert *out, float alpha,
	float zbias) {
	float hw = p->size * 0.5f * p->escala;
	float hl = hw;
	float rot = p->rot;
	vec3_t pos = p->pos;

	if (PARTICULA_TYPE(p) == PARTICULA_ACTIVE && ((CParticulaActive *)p)->sparks) {
		/* a spark: stretched along its motion on the screen, turned to it */
		CParticulaActive *a = (CParticulaActive *)p;
		vec3_t d0, d1, v;
		Vec3_TransformDir0(&d0, &p->pos, view);
		v = (vec3_t){ a->vel.x * 0.01f, a->vel.y * 0.01f, a->vel.z * 0.01f };
		Vec3_TransformDir0(&d1, &v, view);
		float x1 = view->m[12] + d0.x, y1 = view->m[13] + d0.y, z1 = view->m[14] + d0.z;
		float x2 = x1 + d1.x, y2 = y1 + d1.y, z2 = d1.z + z1;
		if (z1 > g_MinSparkDist && z2 > g_MinSparkDist) {
			float sx = (x2 / z2 - x1 / z1) * a->sparkVelScale;
			float sy = -((y2 / z2 - y1 / z1) * a->sparkVelScale);
			float ang = Game_Atan2F((double)sx, (double)sy);
			float len = (float)__builtin_sqrt((double)(sy * sy + sx * sx));
			float mx = Float_MinSS(g_MaxSparkSize, a->sparkMaxSize);
			float s = len * 500.0f + 1.0f;
			if (!(mx > s)) {
				s = mx;
			}
			hl = s * hw;
			rot = ang + rot;
		}
	}
	vec3_t c = pos;
	if (zbias > 0.0f && g_ParticlesZBias) {
		/* moved to the camera by up to zbias times its size */
		vec3_t d = { bill->m[12] - pos.x, bill->m[13] - pos.y, bill->m[14] - pos.z };
		float zb = zbias * p->size * p->escala;
		float l2 = d.y * d.y + d.x * d.x + d.z * d.z;
		float k = 1.0f;
		float zz = (zb + zb) * (zb + zb);
		if (zz > l2) {
			k = l2 / zz;
		}
		Vec3_Unit(&d);
		float s = k * zb;
		c = (vec3_t){ d.x * s + pos.x, d.y * s + pos.y, d.z * s + pos.z };
	}
	vec3_t u, v;
	if (rot != 0.0f) {
		float sn, cs;
		X87_SinCos(rot, &sn, &cs);
		float chw = cs * hw, shw = sn * hw, chl = cs * hl, shl = sn * hl;
		u = (vec3_t){ bill->m[0] * chw + bill->m[4] * shw, bill->m[1] * chw + bill->m[5] * shw,
			bill->m[2] * chw + bill->m[6] * shw };
		v = (vec3_t){ bill->m[4] * chl - bill->m[0] * shl, bill->m[5] * chl - bill->m[1] * shl,
			bill->m[6] * chl - bill->m[2] * shl };
	} else {
		u = (vec3_t){ bill->m[0] * hw, bill->m[1] * hw, bill->m[2] * hw };
		v = (vec3_t){ bill->m[4] * hl, bill->m[5] * hl, bill->m[6] * hl };
	}
	uint32_t col;
	if (1.0f > alpha) {
		col = (uint32_t)(int)((float)(double)(~p->color >> 24) * alpha) << 24 | (p->color & 0xffffff);
	} else {
		col = p->color ^ 0xff000000;
	}
	if (g_DiffuseMod4x) {
		col = (col & 0xff000000) | (col >> 2 & 0x3f3f3f);
	}
	out[0] = (Grafico3DVert){ { (v.x - u.x) + c.x, (v.y - u.y) + c.y, (v.z - u.z) + c.z }, col, 0, p->uv0[0],
		p->uv0[1] };
	out[1] = (Grafico3DVert){ { c.x + (v.x + u.x), c.y + (v.y + u.y), c.z + (v.z + u.z) }, col, 0, p->uv1[0],
		p->uv0[1] };
	out[2] = (Grafico3DVert){ { (-u.x - v.x) + c.x, (-u.y - v.y) + c.y, (-u.z - v.z) + c.z }, col, 0, p->uv0[0],
		p->uv1[1] };
	out[3] = (Grafico3DVert){ { c.x + (u.x - v.x), c.y + (u.y - v.y), c.z + (u.z - v.z) }, col, 0, p->uv1[0],
		p->uv1[1] };
}

/* a particle is drawn when static, or active and born */
static byte Particula_Drawn(CParticula *p) {
	return PARTICULA_TYPE(p) == PARTICULA_STATIC ||
		(PARTICULA_TYPE(p) == PARTICULA_ACTIVE && 0.0f >= ((CParticulaActive *)p)->delay);
}

/* the alpha of a particle: faded out near the eye (in fade, d2 the square of the distance), times the alpha of
 * the system */
static float SistPart_PartAlpha(const CSistPart *sp, const CParticula *p, byte fade, const vec3_t *eye, float d2) {
	float a = 1.0f;
	if (fade) {
		float dx = eye->x - p->pos.x, dy = eye->y - p->pos.y, dz = eye->z - p->pos.z;
		float dd = dy * dy + dx * dx + dz * dz;
		if (d2 > dd) {
			float f = (1.0f / (dd / d2 + 1.0f) - 0.5f) * 2.5f;
			a = f > 1.0f ? 0.0f : 1.0f - f;
		}
	}
	if (1.0f > sp->alpha) {
		a = a * sp->alpha;
	}
	return a;
}

int SistPart_Draw2(CSistPart *sp, CNodoR *r) {
	void *dev = g_pd3dDevice;
	int n = sp->params.nParticulas;
	int numActive = sp->numActive;
	matrix_t inv, tmp;

	CHECK(sp->mat);
	Render_SetMaterial(sp->mat, 0);
	const matrix_t *m = r ? &r->m : &sp->nodo->m;
	matrix_t world = *g_MatIdentity;
	matrix_t bill = *Render_GetBillboard();
	matrix_t view = *Render_GetView();
	uint32_t flags = sp->params.flags;
	if (flags & (SISTPART_LOCAL | SISTPART_LOCALPOS)) {
		/* drawn in the space of the node: the camera matrices brought into it */
		if (flags & SISTPART_LOCAL) {
			world = *m;
		} else {
			world.m[12] = m->m[12];
			world.m[13] = m->m[13];
			world.m[14] = m->m[14];
		}
		CHECK(Matrix_Invert(&inv, &world) == 0);
		bill = *Matrix_MulD3D(&tmp, &bill, &inv);
		view = *Matrix_MulD3D(&tmp, &world, &view);
	}
	byte fade = 0;
	vec3_t eye = { 0.0f, 0.0f, 0.0f };
	float d2 = 0.0f;
	if (g_ParticlesAlphaDist && sp->params.alphaDist > 0.0f) {
		fade = 1;
		CHECK(Matrix_Invert(&inv, m) == 0);
		vec3_t e = *Render_GetEyePos();
		Vec3_TransformH(&eye, &e, &inv);
		d2 = sp->params.alphaDist * sp->params.alphaDist;
	}
	Render_SetWorldMatrix(&world, 0);
	if (g_PartVB && g_UsarVB) {
		Grafico3DVert *out;
		if (RenderEVA_LockParticles(numActive * 4, (void **)&out) < 0) {
			return 0;
		}
		int np = 0, left = sp->numActive;
		for (int i = 0; i < n; i++) {
			CParticula *p = sp->Particles[i];
			if (!p->active) {
				continue;
			}
			if (Particula_Drawn(p)) {
				Particula_Quad(p, &bill, &view, out, SistPart_PartAlpha(sp, p, fade, &eye, d2), sp->params.zBias);
				out += 4;
				np++;
			}
			if (--left <= 0) {
				break;
			}
		}
		CHECK(np <= numActive);
		Buf_Unlock(g_PartVB);
		if (np > 0) {
			Dev_SetVertexShader(dev, D3DFVF_PART);
			Dev_SetStream(dev, g_PartVB, sizeof(Grafico3DVert));
			Dev_SetIndices(dev, g_PartIB, (uint32_t)g_PartVBUsed);
			if (g_ParticlesOne) {
				np = 1;
			}
			if (g_ParticlesDraw) {
				Dev_DrawIndexed(dev, (uint32_t)np * 4, (uint32_t)np * 2);
			}
		}
		g_RenderParts += np;
		g_PartVBUsed += np * 4;
		return 0;
	}
	for (int i = 0; i < n; i++) {
		CParticula *p = sp->Particles[i];
		if (!p->active || !Particula_Drawn(p)) {
			continue;
		}
		Grafico3DVert q[4];
		Particula_Quad(p, &bill, &view, q, SistPart_PartAlpha(sp, p, fade, &eye, d2), sp->params.zBias);
		Dev_SetVertexShader(dev, D3DFVF_PART);
		D3D_METHOD(dev, d3dDrawPrimitiveUP_t, D3DDEV_DRAWPRIMITIVEUP)(dev, D3DPT_TRIANGLESTRIP, 2, q, sizeof(q[0]));
		g_RenderParts++;
	}
	return 0;
}
