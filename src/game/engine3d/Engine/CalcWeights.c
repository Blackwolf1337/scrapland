/*
 * Engine/CalcPesos.cpp of the engine3d: the weights of the vertices of a mesh with a weight (VERT_TEX1_B1, b50 or
 * b51): a vertex near (closer than the weld threshold of the scene) a vertex of a mesh of the parent node takes the
 * position and normal of the nearest one, the best facing one among equals, in the space of its node with weight 0;
 * the others keep their place with weight 1.
 */
#include "scrap/scene.h"
#include "scrap/vertlist.h"
#include "scrap/meshd3d.h"
#include "scrap/trimesh.h"
#include "scrap/node3d.h"

/* a vertex of VERT_TEX1_B1 */
typedef struct VertTex1B1 {
	vec3_t pos;			/* 0x00 */
	float weight;			/* 0x0c */
	vec3_t n;			/* 0x10 */
	uv_t uv;			/* 0x1c */
} VertTex1B1;
SD_SIZE(VertTex1B1, 0x24);

#define WEIGHTS_FLT_MAX 3.40282347e+38f

byte CMeshD3D_CalcWeights(CMeshD3D *self, CNode3D *node) {
	if (!self->weight && !self->weight2) {
		return 0;
	}
	CHECK(node && node->scene);
	CScene *scn = node->scene;
	float maxd2 = scn->vertWeldThreshold * scn->vertWeldThreshold;
	for (int i = 0; i < self->vert.nVert; i++) {
		VertTex1B1 *v = (VertTex1B1 *)CVertList_Tex1B1(&self->vert, (uint32_t)i);
		vec3_t p = v->pos;
		Vec3_TransformH(&p, &p, &node->m);
		matrix_t inv;
		CHECK(Matrix_Invert(&inv, &node->m) == 0);
		v->weight = 1.0f;
		float best = WEIGHTS_FLT_MAX, bestDot = -2.0f;
		VertTex1B1 found = *v;
		for (CNode3D *n = scn->Node3D; n; n = n->next) {
			if (n == node || CNode3D_Class(n) != CLASS_OBJ3D_MESHD3D || node->parent != n) {
				continue;
			}
			for (CMeshD3D *m = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_MESHD3D); m; m = m->next) {
				for (int j = 0; j < m->vert.nVert; j++) {
					vec3_t q;
					CVertList_GetPos(&m->vert, &q, (uint32_t)j);
					Vec3_TransformH(&q, &q, &n->m);
					float dx = p.x - q.x, dy = p.y - q.y, dz = p.z - q.z;
					float d2 = dy * dy + dx * dx + dz * dz;
					if (!(maxd2 >= d2 && best >= d2)) {
						continue;
					}
					vec3_t u;
					CVertList_GetNormal(&m->vert, &u, (uint32_t)j);
					Vec3_TransformDir0(&u, &u, &n->m);
					Vec3_TransformDir0(&u, &u, &inv);
					Vec3_Unit(&u);
					float dot = v->n.y * u.y + v->n.x * u.x + v->n.z * u.z;
					if (best > d2 || dot > bestDot) {
						best = d2;
						bestDot = dot;
						found.weight = 0.0f;
						found.n = u;
						Vec3_TransformH(&found.pos, &q, &inv);
					}
				}
			}
		}
		if (best != WEIGHTS_FLT_MAX) {
			*v = found;
		}
	}
	if (self->next) {
		CMeshD3D_CalcWeights(self->next, node);
	}
	return 1;
}
