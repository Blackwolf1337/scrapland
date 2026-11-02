/*
 * The stencil shadows of the engine3d, original source Engine/stencil.cpp (include/scrap/stencil.h): the buffers,
 * the shadow light, the switch, and the frame pass that draws the shadow volumes into the stencil buffer and the
 * scene again where it is set, the drawing of the volumes (the segment and triangle passes) and the capture of the
 * volumes of a mesh (Stencil_Mesh).
 */
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/d3d8.h"
#include "scrap/fonts.h"
#include "scrap/light.h"
#include "scrap/meshd3d.h"
#include "scrap/material.h"
#include "scrap/model.h"
#include "scrap/prof.h"
#include "scrap/render.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/stencil.h"
#include "scrap/ground.h"
#include "scrap/utild3d.h"
#include "scrap/bsp.h"
#include "scrap/misc.h"
#include "scrap/staticinit.h"

#define g_ShowStencil (GVar_R_ShowStencil.value.i)	/* R_ShowStencil, 2: the volumes drawn */
#define g_StencilMult (GVar_R_StencilMult.value.f)	/* R_StencilMult */
#define g_StencilRefWorld (GVar_R_StencilRefWorld.value.u)	/* R_StencilRefWorld */
#define g_StencilRefModels (GVar_R_StencilRefModels.value.u)	/* R_StencilRefModels */
#define g_StencilSelf (GVar_R_StencilSelf.value.i)	/* R_StencilSelf */
#define g_StencilTog (GVar_R_StencilTog.value.i)	/* R_StencilTog: switched once */
static cGVar GVar_R_StencilTog;
static cGVar GVar_R_StencilSelf;
static cGVar GVar_R_StencilRefWorld;
static cGVar GVar_R_StencilRefModels;
static cGVar GVar_R_StencilMult;
static cGVar GVar_R_ShowStencil;

#define LIGHT_DIRECTIONAL 0x138a

/* the D3DRS_* states of the pass */
#define RS_ZWRITEENABLE 14
#define RS_SHADEMODE 9
#define RS_SRCBLEND 19
#define RS_DESTBLEND 20
#define RS_CULLMODE 22
#define RS_ALPHABLENDENABLE 27
#define RS_STENCILENABLE 52
#define RS_STENCILFAIL 53
#define RS_STENCILZFAIL 54
#define RS_STENCILPASS 55
#define RS_STENCILFUNC 56
#define RS_STENCILREF 57
#define RS_STENCILMASK 58
#define RS_STENCILWRITEMASK 59
#define RS_COLORWRITEENABLE 168

static void Stencil_RS(uint32_t state, uint32_t value) {
	void *dev = g_pd3dDevice;
	Rd_DevSetRenderState(dev, state, value);
}

static StencilBlock *g_StencilBlock;
static int g_StencilGroups;

/* the lights of all groups on or off */
static void StencilBlock_SetLights(byte on) {
	for (int i = 0; i < g_StencilGroups; i++) {
		CHECK(g_StencilBlock[i].Light);
		CLight_SetActive(g_StencilBlock[i].Light, on);
	}
}

/* a debug line, the world matrix kept */
static void Stencil_DebugLine(const vec3_t *a, const vec3_t *b, uint32_t color) {
	matrix_t world;
	void *dev = g_pd3dDevice;

	Rd_DevGetTransform(dev, D3DTS_WORLD, &world);
	Debug_DrawLine(a, b, color);
	dev = g_pd3dDevice;
	Rd_DevSetTransform(dev, D3DTS_WORLD, &world);
}

#define STENCIL_MAX_SEGS 0x2000
#define STENCIL_MAX_TRIS 0x3000
static cGVar GVar_R_StencilLDir;

#define g_StencilLDir (GVar_R_StencilLDir.value.f)	/* R_StencilLDir: the distance of a directional light */
static cGVar GVar_R_StencilFisq;

#define g_StencilFisq (GVar_R_StencilFisq.value.i)	/* R_StencilFisq: RSqrt_Estimate instead of sqrt */
static cGVar GVar_R_StencilDraw;

#define g_StencilDraw (GVar_R_StencilDraw.value.i)	/* R_StencilDraw */

/* the far end of the volume edge from p (in the space of the mesh): R_StencilLDir along the light of a directional
 * light, else to the range of the light, p itself outside the range or the cone of a spot light, the light position
 * when p is on it */
static void Stencil_Extrude(const StencilBlock *b, const vec3_t *p, vec3_t *out) {
	vec3_t lp = b->lightPosObj;
	float dx = p->x - lp.x, dy = p->y - lp.y, dz = p->z - lp.z;
	float r = b->range;
	float d2 = dy * dy + dx * dx + dz * dz;
	float k, len, nx, ny, nz;

	if (g_StencilFisq) {
		if (b->directional) {
			if (d2 == 0.0f) {
				*out = b->lightPosObj;
				return;
			}
			k = RSqrt_Estimate(d2);
			out->x = g_StencilLDir * (k * dx) + p->x;
			out->y = g_StencilLDir * (k * dy) + p->y;
			out->z = g_StencilLDir * (k * dz) + p->z;
			return;
		}
		if (d2 > r * r) {
			*out = *p;
			return;
		}
		k = RSqrt_Estimate(d2);
		nx = k * dx;
		ny = k * dy;
		nz = k * dz;
		if (b->spot && b->cosCone > b->lightDirObj.y * ny + b->lightDirObj.x * nx + b->lightDirObj.z * nz) {
			*out = *p;
			return;
		}
		if (d2 == 0.0f) {
			*out = b->lightPosObj;
			return;
		}
		out->x = nx * r + lp.x;
		out->y = lp.y + ny * r;
		out->z = lp.z + nz * r;
		return;
	}
	len = (float)__builtin_sqrt((double)d2);
	if (b->directional) {
		if (len == 0.0f) {
			*out = b->lightPosObj;
			return;
		}
		k = (float)(1.0 / (double)len);
		out->x = g_StencilLDir * (k * dx) + p->x;
		out->y = g_StencilLDir * (k * dy) + p->y;
		out->z = g_StencilLDir * (k * dz) + p->z;
		return;
	}
	if (len > r) {
		*out = *p;
		return;
	}
	if (b->spot && b->cosCone * len > b->lightDirObj.y * dy + b->lightDirObj.x * dx + b->lightDirObj.z * dz) {
		*out = *p;
		return;
	}
	if (len == 0.0f) {
		*out = b->lightPosObj;
		return;
	}
	k = (float)(1.0 / (double)len);
	out->x = k * dx * r + lp.x;
	out->y = k * dy * r + lp.y;
	out->z = k * dz * r + lp.z;
}

/* the world matrix of the group and the vertex buffer of positions */
static void Stencil_SetGroup(StencilBlock *b, void *vb) {
	Rd_DevSetTransform(g_pd3dDevice, D3DTS_WORLD, &b->world);
	Rd_DevSetVertexShader(g_pd3dDevice, D3DFVF_XYZ);
	Rd_DevSetStreamSource(g_pd3dDevice, 0, vb, sizeof(vec3_t));
}

static void *g_StencilVB;
static void *g_StencilIB;
static vec3_t *g_StencilSegments;
static int g_StencilSegs;

/* the sides of the volumes: a quad from each silhouette segment to its far ends (pass 0 fills the buffer) */
static void Stencil_DrawSegs(int pass) {
	vec3_t *seg, *v;
	int group, first, last, i;

	CHECK(g_StencilVB);
	CHECK(g_StencilIB);
	CHECK(g_StencilSegs <= STENCIL_MAX_SEGS);
	first = 0;
	for (group = 0; group < g_StencilGroups; group++) {
		CHECK(g_StencilBlock[group].useg <= g_StencilSegs);
		last = g_StencilBlock[group].useg;
		if (!pass) {
			CHECK(!Rd_BufLock(g_StencilVB, first * 4 * sizeof(vec3_t),
				(last - first) * 4 * sizeof(vec3_t), &v, group ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD));
			for (i = first; i < last; i++, v += 4) {
				seg = &g_StencilSegments[i * 2];
				Stencil_Extrude(&g_StencilBlock[group], &seg[0], &v[3]);
				Stencil_Extrude(&g_StencilBlock[group], &seg[1], &v[2]);
				v[0] = seg[1];
				v[1] = seg[0];
			}
			Rd_BufUnlock(g_StencilVB);
		}
		if (g_StencilDraw && last - first) {
			Stencil_SetGroup(&g_StencilBlock[group], g_StencilVB);
			Rd_DevSetIndices(g_pd3dDevice, g_StencilIB, 0);
			Rd_DevDrawIndexedPrimitive(g_pd3dDevice,
				D3DPT_TRIANGLELIST, first * 4, (last - first) * 4, first * 6, (last - first) * 2);
		}
		first = last;
	}
	Render_WorldDirty();
}

static void *g_StencilVB2;
static void *g_StencilVB3;
static vec3_t *g_StencilTriInv;
static int g_StencilInv;

/* the caps of the volumes: the triangles facing away from the light at their far ends and turned around in place
 * (pass 0 fills both buffers) */
static void Stencil_DrawTris(int pass) {
	vec3_t *tri, *v1, *v2;
	uint32_t flags;
	int group, first, last, n, i;

	CHECK(g_StencilInv <= STENCIL_MAX_TRIS);
	CHECK(g_StencilVB2);
	CHECK(g_StencilVB3);
	first = 0;
	for (group = 0; group < g_StencilGroups; group++) {
		CHECK(g_StencilBlock[group].utri <= g_StencilInv);
		last = g_StencilBlock[group].utri;
		n = last - first;
		if (!n) {
			continue;
		}
		if (!pass) {
			flags = group ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD;
			CHECK(!Rd_BufLock(g_StencilVB2, first * 3 * sizeof(vec3_t),
				n * 3 * sizeof(vec3_t), &v1, flags));
			CHECK(!Rd_BufLock(g_StencilVB3, first * 3 * sizeof(vec3_t),
				n * 3 * sizeof(vec3_t), &v2, flags));
			for (i = first; i < last; i++, v1 += 3, v2 += 3) {
				tri = &g_StencilTriInv[i * 3];
				Stencil_Extrude(&g_StencilBlock[group], &tri[0], &v1[0]);
				Stencil_Extrude(&g_StencilBlock[group], &tri[1], &v1[1]);
				Stencil_Extrude(&g_StencilBlock[group], &tri[2], &v1[2]);
				v2[0] = tri[0];
				v2[1] = tri[2];
				v2[2] = tri[1];
			}
			Rd_BufUnlock(g_StencilVB3);
			Rd_BufUnlock(g_StencilVB2);
		}
		if (g_StencilDraw) {
			Stencil_SetGroup(&g_StencilBlock[group], g_StencilVB2);
			Rd_DevDrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST,
				first * 3, n);
			Rd_DevSetStreamSource(g_pd3dDevice, 0, g_StencilVB3,
				sizeof(vec3_t));
			Rd_DevDrawPrimitive(g_pd3dDevice, D3DPT_TRIANGLELIST,
				first * 3, n);
		}
		first = last;
	}
	Render_WorldDirty();
}

byte g_RenderFlagNeg;
uint32_t g_RenderFlagMask;
int g_ShadowsStencil;
void *g_StencilRender;
static int g_StencilCapture;

void ShadowsStencil_Set(byte on) {
	byte ok;

	if (!Render_StencilBits()) {
		Con_Warnf("%s", "No stencil bits\n");
		ok = 0;
	} else if (Render_StencilBits() < 4) {
		Con_Warnf("%s", "Not enought stencil bits\n");
		ok = 0;
	} else {
		ok = 1;
	}
	byte v = ok ? on : 0;
	E3D_LogDebug((&g_FileLog), "Stencil shadows %s", v ? "ON" : "OFF");
	g_ShadowsStencil = v;
	g_StencilCapture = 0;
	g_StencilRender = NULL;
	g_RenderClassMask = 0;
	g_RenderFlagMask = 0;
	g_RenderFlagNeg = 0;
	g_RenderPass = 0;
	g_StencilSegs = 0;
	g_StencilInv = 0;
	g_StencilGroups = 0;
}

float g_StencilMultFrame = 1.0f;

void ShadowsStencil_Capture(byte on) {
	if (!g_RenderOff && g_ShadowsStencil) {
		g_StencilMultFrame = g_StencilMult;
		g_StencilCapture = on != 0;
	}
}

static vec3_t g_ShadowLightPos;
static vec3_t g_ShadowLightDir;
/* the light of the shadows (E3D_SetShadowLight) */
static CLight *g_ShadowLight;
static float g_ShadowCos;
static int g_ShadowLightType = 5002;
static float g_ShadowRange = 1000.0f;

void E3D_SetShadowLight(CLight *l, int unused) {
	vec3_t d, n;

	(void)unused;
	g_ShadowLight = NULL;
	if (!g_ShadowsStencil || !g_StencilCapture || !l) {
		return;
	}
	g_ShadowLightType = l->type;
	g_ShadowLight = l;
	if (l->type == LIGHT_DIRECTIONAL) {
		/* far away against the direction */
		d = l->d3d.Direction;
		const vec3_t *u = Vec3_Normalize(&n, &d);
		g_ShadowLightPos = (vec3_t){ -u->x * 2000000.0f, -u->y * 2000000.0f, -u->z * 2000000.0f };
		d = l->d3d.Direction;
		u = Vec3_Normalize(&n, &d);
		g_ShadowRange = 2500000.0f;
		g_ShadowCos = 0.0f;
		g_ShadowLightDir = *u;
		return;
	}
	g_ShadowLightPos = l->d3d.Position;
	d = l->d3d.Direction;
	g_ShadowLightDir = *Vec3_Normalize(&n, &d);
	g_ShadowRange = l->range == 0.0f ? 1000.0f : l->range * 1.1f;
	g_ShadowCos = (float)cos((double)l->phi * 0.5 * 3.141592653589793 / 180.0);
}

void Stencil_Free(void) {
	g_ShadowLight = NULL;
	if (g_StencilBlock) {
		operator_delete_sized(g_StencilBlock, sizeof(StencilBlock));
		g_StencilBlock = NULL;
	}
	if (g_StencilSegments) {
		operator_delete_sized(g_StencilSegments, 0xc);
		g_StencilSegments = NULL;
	}
	if (g_StencilTriInv) {
		operator_delete_sized(g_StencilTriInv, 0xc);
		g_StencilTriInv = NULL;
	}
	if (g_StencilVB3) {
		Rd_Release(g_StencilVB3);
		g_StencilVB3 = NULL;
	}
	if (g_StencilVB2) {
		Rd_Release(g_StencilVB2);
		g_StencilVB2 = NULL;
	}
	if (g_StencilVB) {
		Rd_Release(g_StencilVB);
		g_StencilVB = NULL;
	}
	if (g_StencilIB) {
		Rd_Release(g_StencilIB);
		g_StencilIB = NULL;
	}
}

void Stencil_Init(void) {
	const char *warn = NULL;
	byte ok, on;

	if (!Render_StencilBits()) {
		warn = "No stencil bits\n";
	} else if (Render_StencilBits() < 4) {
		warn = "Not enought stencil bits\n";
	}
	if (warn) {
		Con_Warnf("%s", warn);
		g_StencilShadows = -1;
		ok = 0;
		on = 0;
	} else {
		ok = 1;
		if (g_StencilShadows < 0) {
			g_StencilShadows = 1;
			on = 1;
		} else {
			on = g_StencilShadows > 0;
		}
	}
	ShadowsStencil_Set(on);
	if (!ok) {
		return;
	}
	CHECK(!g_StencilBlock);
	g_StencilBlock = operator_new_array(STENCIL_MAX_GROUPS * sizeof(StencilBlock));
	CHECK(!g_StencilSegments);
	g_StencilSegments = operator_new_array(0x30000);
	CHECK(!g_StencilTriInv);
	g_StencilTriInv = operator_new_array(0x6c000);
	/* two triangles per quad of 4 vertices */
	uint16_t *idx = operator_new_array(0x18000);
	for (int i = 0; i < 0x2000; i++) {
		uint16_t v = (uint16_t)(i * 4);
		idx[i * 6] = v;
		idx[i * 6 + 1] = v + 1;
		idx[i * 6 + 2] = v + 2;
		idx[i * 6 + 3] = v + 1;
		idx[i * 6 + 4] = v + 3;
		idx[i * 6 + 5] = v + 2;
	}
	Render_CreateIB(&g_StencilIB, idx, 0x4000, 0);
	if (idx) {
		operator_delete_sized(idx, 2);
	}
	void *dev = g_pd3dDevice;
	Rd_DevCreateVertexBuffer(dev, 0x60000, 0x208, 2, 0, &g_StencilVB);
	dev = g_pd3dDevice;
	Rd_DevCreateVertexBuffer(dev, 0x6c000, 0x208, 2, 0, &g_StencilVB2);
	dev = g_pd3dDevice;
	Rd_DevCreateVertexBuffer(dev, 0x6c000, 0x208, 2, 0, &g_StencilVB3);
}

/* the scene drawn: unlit where the stencil is set, then lit where it is not, the floor through g_StencilRender */
static void Stencil_DrawScene(void) {
	if (g_StencilRender) {
		g_RenderPass = 2;
		StencilBlock_SetLights(0);
		((void (*)(void))g_StencilRender)();
		if (g_StencilSelf) {
			E3D_FlushBatch();
			Stencil_RS(RS_STENCILREF, g_StencilRefModels);
			Models_Render();
		}
		E3D_FlushBatch();
		StencilBlock_SetLights(1);
		Stencil_RS(RS_STENCILENABLE, 0);
		g_RenderPass = 3;
		g_RenderFlagMask = 0x40;
		g_RenderFlagNeg = 1;
		((void (*)(void))g_StencilRender)();
		Models_Render();
		E3D_FlushBatch();
		g_RenderFlagMask = 0x40;
		g_RenderFlagNeg = 0;
		g_RenderPass = 1;
		g_StencilRender = NULL;
		return;
	}
	StencilBlock_SetLights(0);
	Models_Render();
	E3D_FlushBatch();
	StencilBlock_SetLights(1);
	Stencil_RS(RS_STENCILENABLE, 0);
	g_RenderFlagMask = 0x40;
	g_RenderFlagNeg = 1;
	Models_Render();
	E3D_FlushBatch();
	g_RenderFlagMask = 0x40;
	g_RenderFlagNeg = 0;
	g_RenderClassMask = 0x58ffff1f;
	Models_Render();
	E3D_FlushBatch();
}

static CMaterial g_StencilMat;

void ShadowsStencil_Render(void) {
	if (g_RenderOff) {
		return;
	}
	if (g_ShadowsStencil) {
		g_StencilCapture = 0;
		g_StencilMultFrame = g_StencilMult;
	}
	if (g_KeyV && g_KeyX) {
		g_KeyV = 0;
		ShadowsStencil_Set(g_ShadowsStencil == 0);
	}
	if (g_StencilTog) {
		g_StencilTog = 0;
		ShadowsStencil_Set(g_ShadowsStencil == 0);
	}
	if (!g_ShadowsStencil) {
		return;
	}
	Render_SetWorldMatrix((&g_MatIdentity), 0);
	g_StencilMat.params.lighting = 0;
	g_StencilMat.params.zFunc = (g_ShowStencil != 2) + 4;
	Render_SetMaterial((&g_StencilMat), 0);
	/* the volumes: front faces count up, back faces down */
	Stencil_RS(RS_ZWRITEENABLE, 0);
	Stencil_RS(RS_STENCILENABLE, g_ShowStencil != 2);
	Stencil_RS(RS_SHADEMODE, 1);
	Stencil_RS(RS_STENCILFUNC, 8);
	Stencil_RS(RS_STENCILZFAIL, 1);
	Stencil_RS(RS_STENCILFAIL, 1);
	Stencil_RS(RS_STENCILREF, 1);
	Stencil_RS(RS_STENCILMASK, 0xffffffff);
	Stencil_RS(RS_STENCILWRITEMASK, 0xffffffff);
	Stencil_RS(RS_STENCILPASS, 8);
	Stencil_RS(RS_COLORWRITEENABLE, g_ShowStencil == 2 ? 0xf : 0);
	Stencil_RS(RS_ALPHABLENDENABLE, 1);
	Stencil_RS(RS_SRCBLEND, (g_ShowStencil == 2) + 1);
	Stencil_RS(RS_DESTBLEND, 2);
	Stencil_RS(RS_CULLMODE, 3);
	Stencil_DrawSegs(0);
	Stencil_DrawTris(0);
	Stencil_RS(RS_CULLMODE, 2);
	Stencil_RS(RS_STENCILPASS, 7);
	Stencil_DrawSegs(1);
	Stencil_DrawTris(1);
	Stencil_RS(RS_SHADEMODE, 2);
	Stencil_RS(RS_CULLMODE, 3);
	Stencil_RS(RS_ZWRITEENABLE, 1);
	Stencil_RS(RS_STENCILENABLE, 0);
	Stencil_RS(RS_ALPHABLENDENABLE, 0);
	Stencil_RS(RS_COLORWRITEENABLE, 0xf);
	Render_SetCull(3);
	Stencil_RS(RS_STENCILENABLE, 1);
	Stencil_RS(RS_STENCILREF, g_StencilRefWorld);
	Stencil_RS(RS_STENCILFUNC, 4);
	Stencil_RS(RS_STENCILPASS, 2);
	CHECK(!g_StencilCapture);
	Stencil_DrawScene();
	g_RenderClassMask = 0xa70000e0;
	Stencil_RS(RS_STENCILENABLE, 0);
	CHECK(!g_StencilCapture);
	if (g_ProfMode == 2) {
		Font_PrintfXY(g_FntComic14, 10.0f, 120.0f, "grupos: %d", g_StencilGroups);
		Font_PrintfXY(g_FntComic14, 10.0f, 140.0f, "segs: %d", g_StencilSegs);
		Font_PrintfXY(g_FntComic14, 10.0f, 160.0f, "inv: %d", g_StencilInv);
	}
	g_StencilSegs = 0;
	g_StencilInv = 0;
	g_StencilGroups = 0;
	g_ShadowLight = NULL;
}

static cGVar GVar_R_StencilBBox;

/* the capture of the volumes of one mesh list */
#define g_StencilBBox (GVar_R_StencilBBox.value.i)	/* R_StencilBBox: the segment to the light tested */
static cGVar GVar_R_StencilViewDisp;

#define g_StencilViewDisp (GVar_R_StencilViewDisp.value.f)	/* R_StencilViewDisp: the volumes moved along the view */
#define LIGHT_SPOT 0x1389

static byte g_TriFacing[65536];
static int g_StencilGroupsMax;

void Stencil_Mesh(struct CVertList *v, const void *tris, uint32_t nTri, struct CMeshD3D *m, struct CRenderNode *r) {
	(void)v;
	(void)tris;
	(void)nTri;
	if (!g_ShadowsStencil || !g_StencilCapture || !g_ShadowLight) {
		return;
	}
	CHECK(m);
	if (m->node->flags >> 5 & 1) {
		return;
	}
	const uint16_t *edges = m->segs;
	const TriMeshPlane *planes = m->pSeg;
	int nTriSeg = (int)m->nSeg;
	const vec3_t *pos = m->pPos;
	if (!edges || !planes || !pos) {
		return;
	}
	/* the center of the list in the world, near enough to the eye */
	vec3_t c = m->center;
	Vec3_TransformH(&c, &c, &m->node->m);
	const vec3_t *eye = Render_GetEyePos();
	float dist = r ? ((CModel *)r->owner)->stencilDist : 4000.0f;
	float k = dist * g_StencilMultFrame;
	if ((c.x - eye->x) * (c.x - eye->x) + (c.y - eye->y) * (c.y - eye->y) + (c.z - eye->z) * (c.z - eye->z) >
		k * k) {
		return;
	}
	if (r && ((CModel *)r->owner)->visible == 2 && g_StencilBBox) {
		/* a model seen only by its shadow: the segment from it away from the light must be seen */
		vec3_t d, far;
		float dx = c.x - g_ShadowLightPos.x;
		float dy = c.y - g_ShadowLightPos.y;
		float dz = c.z - g_ShadowLightPos.z;
		float len = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
		if (len == 0.0f) {
			d.x = 0.0f;
			d.y = 0.0f;
			d.z = 0.0f;
		} else {
			float inv = (float)(1.0 / (double)len);
			d.x = dx * inv;
			d.y = dy * inv;
			d.z = dz * inv;
		}
		if (g_ShadowLightType == LIGHT_DIRECTIONAL) {
			far.x = c.x + d.x * g_StencilLDir;
			far.y = c.y + d.y * g_StencilLDir;
			far.z = c.z + d.z * g_StencilLDir;
		} else {
			far.x = d.x * g_ShadowRange + g_ShadowLightPos.x;
			far.y = d.y * g_ShadowRange + g_ShadowLightPos.y;
			far.z = d.z * g_ShadowRange + g_ShadowLightPos.z;
		}
		if (g_ShowStencil == 3) {
			Stencil_DebugLine(&c, &far, 0xffff00);
		}
		if (!Frustum_SegVisible(&c, &far)) {
			return;
		}
	}
	if (g_ShowStencil == 3) {
		Stencil_DebugLine(&c, &g_ShadowLightPos, 0xff);
	}
	g_TriFacing[0xffff] = 1;
	CHECK(nTriSeg <= 0x10000);
	CHECK(nTriSeg <= 65535);
	matrix_t world, inv;
	vec3_t lpos, ldir;
	void *dev = g_pd3dDevice;
	Rd_DevGetTransform(dev, D3DTS_WORLD, &world);
	CHECK(D3DX_MatrixInverse(&inv, NULL, &world));
	Vec3_TransformH(&lpos, &g_ShadowLightPos, &inv);
	Vec3_TransformDir0(&ldir, &g_ShadowLightDir, &inv);
	for (int i = 0; i < nTriSeg; i++) {
		const TriMeshPlane *p = &planes[i];
		float s = p->n.x * lpos.x + p->n.y * lpos.y + lpos.z * p->n.z + p->d;
		uint32_t bits;
		memcpy(&bits, &s, sizeof(bits));
		g_TriFacing[i] = ~(byte)(bits >> 31) & 1;
	}
	int segs0 = g_StencilSegs;
	int tris0 = g_StencilInv;
	vec3_t *segs = g_StencilSegments;
	/* the silhouette: the edges between a triangle that faces the light and one that does not */
	for (const uint16_t *e = edges; e[0] != 0xffff; e += 4) {
		CHECK(e[0] < 0x10000);
		CHECK(e[1] < 0x10000);
		byte f0 = g_TriFacing[e[0]];
		if (g_TriFacing[e[1]] == f0) {
			continue;
		}
		vec3_t a = pos[f0 ? e[2] : e[3]];
		vec3_t b = pos[f0 ? e[3] : e[2]];
		if (g_ShowStencil == 1) {
			vec3_t mid;
			mid.x = (a.x + b.x) * 0.5f;
			mid.y = (a.y + b.y) * 0.5f;
			mid.z = (a.z + b.z) * 0.5f;
			Debug_DrawLineM(&world, &a, &mid, 0xff00);
			mid.x = (a.x + b.x) * 0.5f;
			mid.y = (a.y + b.y) * 0.5f;
			mid.z = (a.z + b.z) * 0.5f;
			Debug_DrawLineM(&world, &b, &mid, 0xff0000);
		}
		if (g_StencilSegs >= STENCIL_MAX_SEGS) {
			g_StencilSegs = segs0;
			goto done;
		}
		segs[g_StencilSegs * 2] = a;
		segs[g_StencilSegs * 2 + 1] = b;
		g_StencilSegs++;
	}
	if (g_StencilSegs != segs0) {
		/* the caps: the triangles that do not face the light */
		int t = 0;
		int ntri = g_StencilInv;
		vec3_t *out = (vec3_t *)g_StencilTriInv + ntri * 3;
		for (CMeshD3D *sub = m; sub; sub = sub->next) {
			CHECK(sub->vertOrig);
			CHECK(sub->triBase == t);
			for (uint32_t j = 0; j < sub->nTri; j++, t++) {
				const uint16_t *tri = sub->pTri[j];
				CHECK(t < nTriSeg);
				if (g_TriFacing[t]) {
					continue;
				}
				if (ntri >= STENCIL_MAX_TRIS) {
					g_StencilSegs = segs0;
					g_StencilInv = tris0;
					return;
				}
				out[0] = m->pPos[sub->vertOrig[tri[0]]];
				out[1] = m->pPos[sub->vertOrig[tri[1]]];
				out[2] = m->pPos[sub->vertOrig[tri[2]]];
				out += 3;
				g_StencilInv = ++ntri;
			}
		}
		CHECK(t == nTriSeg);
	}
done:
	CHECK(g_StencilGroups < STENCIL_MAX_GROUPS);
	if (g_StencilSegs == segs0) {
		return;
	}
	/* a new group: the light, its position in the object and the world matrix moved along the view */
	matrix_t view = *Render_GetBillboard();
	vec3_t disp = { 0.0f, 0.0f, g_StencilViewDisp };
	Vec3_TransformDir0(&disp, &disp, &view);
	Matrix_AddPos(&world, &disp);
	StencilBlock *bl = &g_StencilBlock[g_StencilGroups];
	bl->Light = g_ShadowLight;
	bl->lightPos = g_ShadowLightPos;
	bl->range = g_ShadowRange;
	bl->cosCone = g_ShadowCos;
	bl->directional = g_ShadowLightType == LIGHT_DIRECTIONAL;
	bl->spot = g_ShadowLightType == LIGHT_SPOT;
	bl->lightPosObj = lpos;
	bl->lightDirObj = ldir;
	bl->world = world;
	bl->useg = g_StencilSegs;
	bl->utri = g_StencilInv;
	g_StencilGroups++;
	if (g_StencilGroupsMax <= g_StencilGroups) {
		g_StencilGroupsMax = g_StencilGroups;
	}
}

static cGVar GVar_R_StencilRefModels;
static cGVar GVar_R_StencilMult;
/* engine variables */
static cGVar GVar_R_ShowStencil;
static cGVar GVar_R_StencilTog;
static cGVar GVar_R_StencilSelf;
static cGVar GVar_R_StencilRefWorld;
cGVar GVar_R_StencilShadows;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_R_StencilShadows, &GVar_R_StencilShadows, "R_StencilShadows", 6, "Enable stencil shadows (req. reset3d) (-1/0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_ShowStencil, &GVar_R_ShowStencil, "R_ShowStencil", 0, "Show stencil shadows (0/1/2/3/4)", 0)
INIT_GVAR_INT(Init_GVar_R_StencilSelf, &GVar_R_StencilSelf, "R_StencilSelf", 2, "Self-proyected stencil shadows (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_StencilTog, &GVar_R_StencilTog, "R_StencilTog", 0, "Toggle stencil shadows", 0)
INIT_GVAR_FLOAT(Init_GVar_R_StencilMult, &GVar_R_StencilMult, "R_StencilMult", 2, "Stencil shadows distance mult", 1.0f)
INIT_GVAR_INT(Init_GVar_R_StencilBBox, &GVar_R_StencilBBox, "R_StencilBBox", 0, "Discard by approx. object bbox (0/1)", 1)
INIT_GVAR_INT(Init_GVar_R_StencilFisq, &GVar_R_StencilFisq, "R_StencilFisq", 0, "Use fast inverse sqrt (0/1)", 0)
INIT_GVAR_FLOAT(Init_GVar_R_StencilLDir, &GVar_R_StencilLDir, "R_StencilLDir", 0, "Dir light shadow distance", 2000.0f)
INIT_GVAR_INT(Init_GVar_R_StencilDraw, &GVar_R_StencilDraw, "R_StencilDraw", 0, "Draw Stencil Shadows", 1)
INIT_GVAR_INT(Init_GVar_R_StencilRefWorld, &GVar_R_StencilRefWorld, "R_StencilRefWorld", 0, "", 2)
INIT_GVAR_INT(Init_GVar_R_StencilRefModels, &GVar_R_StencilRefModels, "R_StencilRefModels", 0, "", 2)
INIT_GVAR_FLOAT(Init_GVar_R_StencilViewDisp, &GVar_R_StencilViewDisp, "R_StencilViewDisp", 0, "", 0.0f)
INIT_VECTOR3(Init_ShadowLightPos, &g_ShadowLightPos, 200.0f, 200.0f, 0.0f)
INIT_VECTOR3(Init_ShadowLightDir, &g_ShadowLightDir, 0.0f, -1.0f, 0.0f)
FINI_CALL(Fini_CMaterial_StencilMat, &g_StencilMat, CMaterial_dtor)
INIT_OBJECT(Init_CMaterial_StencilMat, Fini_CMaterial_StencilMat, &g_StencilMat, CMaterial_ctor)
