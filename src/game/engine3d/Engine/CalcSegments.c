/*
 * The edges of the triangles of a mesh list for the outlines, original source Engine/CalcSegmentos.cpp
 * (include/scrap/meshd3d.h): every edge of the original vertices (vertOrig) once with the one or two triangles it
 * belongs to, and the plane of each triangle.
 */
#include <string.h>

#include "scrap/meshd3d.h"
#include "scrap/material.h"
#include "scrap/misc.h"

/* an edge: the triangles on both sides (0xffff for none) and its original vertices */
typedef struct MeshSeg {
	uint16_t tri[2];		/* 0x0 */
	uint16_t v[2];			/* 0x4 */
} MeshSeg;
SD_SIZE(MeshSeg, 0x8);

/* the plane of a triangle */
typedef struct TriMeshSeg {
	float d;			/* 0x0 */
	vec3_t n;			/* 0x4 */
} TriMeshSeg;
SD_SIZE(TriMeshSeg, 0x10);

/* the triangle accessor of MeshD3D.h */
static uint16_t *MeshD3D_Tri(CMeshD3D *m, uint32_t i) {
	CHECK((uint32_t)i < (uint32_t)m->nTri);
	CHECK(m->pTri);
	return m->pTri[i];
}

void __thiscall CMeshD3D_CalcSegments(CMeshD3D *self, byte all) {
	CHECK(self->vertOrig);
	CHECK(!self->p40);
	CHECK(!self->pSeg);
	CHECK(self->pPos);
	int numTri = (int)self->nTri;
	if (all) {
		self->triBase = 0;
		for (CMeshD3D *m = self->next; m; m = m->next) {
			m->triBase = numTri;
			numTri += (int)m->nTri;
		}
	}
	MeshSeg *segs = New_Array((uint32_t)numTri * 3, sizeof(MeshSeg));
	uint16_t *segv3 = New_Array((uint32_t)numTri * 3, sizeof(uint16_t));
	TriMeshSeg *triSeg = New_Array((uint32_t)numTri, sizeof(TriMeshSeg));
	self->pSeg = triSeg;
	self->nSeg = (uint32_t)numTri;
	int numSeg = 0, group = 0;
	CMeshD3D *m = self;
	for (;;) {
		int nVert = m->vert.nVert;
		int n = (int)m->nTri;
		CHECK(group == m->triBase);
		for (int i = 0; i < n; i++) {
			uint16_t *tri = MeshD3D_Tri(m, i);
			if (self->pSeg) {
				vec3_t nrm, p;
				CMeshD3D_TriNormal(m, &nrm, i);
				CHECK(i + group < numTri);
				TriMeshSeg *ts = &((TriMeshSeg *)self->pSeg)[group + i];
				ts->n = nrm;
				const vec3_t *v = CVertList_GetPos(&m->vert, &p, tri[0]);
				((TriMeshSeg *)self->pSeg)[group + i].d = -(v->x * nrm.x + v->y * nrm.y + v->z * nrm.z);
			}
			if (tri[0] == tri[1] || tri[1] == tri[2] || tri[2] == tri[0]) {
				continue;
			}
			for (int k = 0; k < 3; k++) {
				int v1 = tri[k], v2 = tri[(k + 1) % 3], v3 = tri[(k + 2) % 3];
				CHECK(v1 < nVert && v2 < nVert);
				uint16_t a1 = m->vertOrig[v1], a2 = m->vertOrig[v2], a3 = m->vertOrig[v3];
				CHECK(a1 != a2);
				/* the same edge the other way round with another third vertex */
				int s;
				for (s = 0; s < numSeg; s++) {
					if (segs[s].v[0] == a2 && segs[s].v[1] == a1 && segv3[s] != a3) {
						break;
					}
				}
				if (s == numSeg) {
					CHECK(numSeg < numTri * 3);
					segs[numSeg].tri[0] = (uint16_t)(i + group);
					segs[numSeg].tri[1] = 0xffff;
					segs[numSeg].v[0] = a1;
					segs[numSeg].v[1] = a2;
					segv3[numSeg] = a3;
					numSeg++;
				} else if (segs[s].tri[1] == 0xffff) {
					segs[s].tri[1] = (uint16_t)(i + group);
				} else {
					E3D_LogBlue(g_FileLog, "WARN: calcular_segmentos(): lado con > 2 tris [Mat: %s]",
						m->mat->Name);
				}
			}
		}
		group += n;
		if (!all) {
			break;
		}
		m = m->next;
		if (!m) {
			break;
		}
	}
	CHECK(numSeg > 0);
	if (segv3) {
		operator_delete_sized(segv3, sizeof(uint16_t));
	}
	MeshSeg *out = New_Array((uint32_t)numSeg + 1, sizeof(MeshSeg));
	memcpy(out, segs, (uint32_t)numSeg * sizeof(MeshSeg));
	out[numSeg].tri[0] = out[numSeg].tri[1] = 0xffff;
	out[numSeg].v[0] = out[numSeg].v[1] = 0;
	if (segs) {
		operator_delete_sized(segs, sizeof(MeshSeg));
	}
	self->p40 = out;
	self->n34 = (uint32_t)numSeg + 1;
	E3D_LogWhite(g_FileLog, "INFO: Segmentos: %d [Mat: %s]", numSeg, self->mat->Name);
}
