/*
 * Engine/NodeFX.cpp of the engine3d: the effects of the NODE_FX nodes (INodeFX, in CNode3D obj58), applied to the
 * world matrix of the node by CRenderNode_ApplyFX. The type comes from the "NodeEffect" section of the user properties
 * of the node: Wheel and Roll (CNodeFX_Wheel: turned by the way the node moved, plus a turn per second and a noise),
 * Spring, Cloth, Hair and Bust (CNodeFX_Spring: the node lags behind its parent like a weight on a spring, stepped
 * every NodeFXSpringMSec and blended in between).
 */
#ifndef SCRAP_NODEFX_H
#define SCRAP_NODEFX_H

#include "scrap/gvar.h"
#include "scrap/node3d.h"

/* the types vftable[1] returns */
enum { NODEFX_NONE = 0, NODEFX_WHEEL = 1, NODEFX_SPRING = 2 };

typedef struct INodeFX INodeFX;

typedef struct INodeFXVtbl {
	INodeFX *(*Delete)(INodeFX *self, byte flags);				/* 0x0 */
	byte(*Type)(INodeFX *self);							/* 0x4 NODEFX_ */
	/* m changed by the effect; 0 */
	int(*Apply)(INodeFX *self, matrix_t *m, CNode3D *node, CRenderNode *r);	/* 0x8 */
} INodeFXVtbl;

/* calls through the slots of INodeFXVtbl */
static inline byte NodeFX_Type(void *self) {
	return VT(INodeFXVtbl, self)->Type(self);
}

static inline int NodeFX_Apply(void *self, matrix_t *m, CNode3D *node, CRenderNode *r) {
	return VT(INodeFXVtbl, self)->Apply(self, m, node, r);
}

static inline INodeFX *NodeFX_Delete(void *self, byte flags) {
	return VT(INodeFXVtbl, self)->Delete(self, flags);
}

struct INodeFX {
	const INodeFXVtbl *vftable;	/* 0x0 */
};

typedef struct CNodeFX_Wheel {
	union {
		INodeFX base;
		const INodeFXVtbl *vftable;	/* 0x00 */
	};
	float radius;			/* 0x04 0: no turn from the movement */
	vec3_t rot;			/* 0x08 the axes turned by the distance moved */
	vec3_t trans;			/* 0x14 the part of the movement along each axis that rolls, the rest slides */
	vec3_t roll;			/* 0x20 radians per second */
	vec3_t noiseVel;		/* 0x2c */
	vec3_t noiseAmt;		/* 0x38 */
	vec3_t ang;			/* 0x44 the angles given to look r1 */
	uint32_t noiseBase;		/* 0x50 ms, random */
} CNodeFX_Wheel;

typedef struct CNodeFX_Spring {
	union {
		INodeFX base;
		const INodeFXVtbl *vftable;	/* 0x00 */
	};
	vec3_t prev;			/* 0x04 the position of the last step */
	vec3_t next;			/* 0x10 the position of the next one */
	uint32_t prevMSec;		/* 0x1c */
	uint32_t nextMSec;		/* 0x20 */
	vec3_t vel;			/* 0x24 */
	float damping;			/* 0x30 */
	float spring;			/* 0x34 */
	float torsion;			/* 0x38 */
	float damp2;			/* 0x3c */
} CNodeFX_Spring;

/* the seconds of the frame */
extern float g_FrameSec;

/* the effect of type (NULL: "Type" of the NodeEffect section of the node), NULL for an unknown one */
void *NodeFX_New(const char *type, CNode3D *node);
/* a copy of the effect, NULL for an unknown type */
void *NodeFX_Clone(INodeFX *fx);

/* other modules (src/imports.inc) */
float Ang_Wrap(float a);	/* rva 0x238b80: one turn off when above 2 pi or below -2 pi */
matrix_t *Matrix_Transpose(matrix_t *out, const matrix_t *m);

#endif
