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

extern cGVar GVar_NodeFXSpringMSec;


/* the types vftable[1] returns */
enum { NODEFX_NONE = 0, NODEFX_WHEEL = 1, NODEFX_SPRING = 2 };

typedef struct INodeFX INodeFX;

typedef struct INodeFXVtbl {
	INodeFX *(*Delete)(INodeFX *self, byte flags);				/* 0x0 */
	byte(*Type)(INodeFX *self);							/* 0x4 NODEFX_ */
	/* m changed by the effect; 0 */
	int(*Apply)(INodeFX *self, matrix_t *m, CNode3D *node, CRenderNode *r);	/* 0x8 */
} INodeFXVtbl;
extern const INodeFXVtbl CNodeFX_Spring_vftable;
#define CNODEFX_SPRING_VFTABLE (&CNodeFX_Spring_vftable)
extern const INodeFXVtbl CNodeFX_Wheel_vftable;
#define CNODEFX_WHEEL_VFTABLE (&CNodeFX_Wheel_vftable)
extern const INodeFXVtbl INodeFX_vftable;
#define INODEFX_VFTABLE (&INodeFX_vftable)

struct INodeFX {
	const INodeFXVtbl *vftable;	/* 0x0 */
};
SD_SIZE(INodeFX, 4);

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
SD_OFFSET(CNodeFX_Wheel, ang, 0x44);
SD_SIZE(CNodeFX_Wheel, 0x54);

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
SD_OFFSET(CNodeFX_Spring, vel, 0x24);
SD_SIZE(CNodeFX_Spring, 0x40);

/* the engine variables of the effects */
extern cGVar GVar_NodeFXSpringDamp1;
#define g_NodeFXSpringDamp1 (GVar_NodeFXSpringDamp1.value.f)	/* NodeFXSpringDamp1 */
extern cGVar GVar_NodeFXSpringDamp2;
#define g_NodeFXSpringDamp2 (GVar_NodeFXSpringDamp2.value.f)	/* NodeFXSpringDamp2 */
extern cGVar GVar_NodeFXSpringDebug;
#define g_NodeFXSpringDebug (GVar_NodeFXSpringDebug.value.i)	/* NodeFXSpringDebug: the variables instead of the members */
#define g_NodeFXSpringMSec (GVar_NodeFXSpringMSec.value.u)	/* NodeFXSpringMSec, the step */
extern cGVar GVar_NodeFXSpringMidP;
#define g_NodeFXSpringMidP (GVar_NodeFXSpringMidP.value.f)	/* NodeFXSpringMidP */
extern cGVar GVar_NodeFXSpringRot;
#define g_NodeFXSpringRot (GVar_NodeFXSpringRot.value.f)	/* NodeFXSpringRot */
extern cGVar GVar_NodeFXSpringRotX;
#define g_NodeFXSpringRotX (GVar_NodeFXSpringRotX.value.f)	/* NodeFXSpringRotX, degrees */
extern cGVar GVar_NodeFXSpringShow;
#define g_NodeFXSpringShow (GVar_NodeFXSpringShow.value.i)	/* NodeFXSpringShow */
extern cGVar GVar_NodeFXSpringSprng;
#define g_NodeFXSpringSprng (GVar_NodeFXSpringSprng.value.f)	/* NodeFXSpringSprng */
extern cGVar GVar_NodeFXWheelRadius;
#define g_NodeFXWheelRadius (GVar_NodeFXWheelRadius.value.f)	/* NodeFXWheelRadius */
extern cGVar GVar_NodeFXWheelShow;
#define g_NodeFXWheelShow (GVar_NodeFXWheelShow.value.i)	/* NodeFXWheelShow */
extern cGVar GVar_NodeFXWheelSlide;
#define g_NodeFXWheelSlide (GVar_NodeFXWheelSlide.value.f)	/* NodeFXWheelSlide */
/* the seconds of the frame */
extern float g_FrameSec;
/* the section "NodeEffect" of the user properties */
extern const char *g_NodeEffectSect;

/* the effect of type (NULL: "Type" of the NodeEffect section of the node), NULL for an unknown one */
void *NodeFX_New(const char *type, CNode3D *node);
/* a copy of the effect, NULL for an unknown type */
void *NodeFX_Clone(INodeFX *fx);

void INodeFX_ctor(INodeFX *self);
INodeFX *INodeFX_Delete(INodeFX *self, byte flags);
byte INodeFX_Type(INodeFX *self);

/* type 0 Wheel, 1 Roll: the defaults of the properties */
CNodeFX_Wheel *CNodeFX_Wheel_ctor(CNodeFX_Wheel *self, CNode3D *node, int type);
CNodeFX_Wheel *CNodeFX_Wheel_ctorCopy(CNodeFX_Wheel *self, const CNodeFX_Wheel *src);
/* not to be used: CHECK(0) */
INodeFX *CNodeFX_Wheel_Delete(INodeFX *base, byte flags);
byte CNodeFX_Wheel_Type(INodeFX *base);
int CNodeFX_Wheel_Apply(INodeFX *base, matrix_t *m, CNode3D *node, CRenderNode *r);
/* ang 0 and a new noiseBase; the quotient of the random number by 20000 */
int CNodeFX_Wheel_Reset(CNodeFX_Wheel *self);

/* type 0 Spring, 1 Cloth, 2 Hair, 3 Bust */
CNodeFX_Spring *CNodeFX_Spring_ctor(CNodeFX_Spring *self, CNode3D *node, int type);
CNodeFX_Spring *CNodeFX_Spring_ctorCopy(CNodeFX_Spring *self, const CNodeFX_Spring *src);
INodeFX *CNodeFX_Spring_Delete(INodeFX *base, byte flags);
byte CNodeFX_Spring_Type(INodeFX *base);
int CNodeFX_Spring_Apply(INodeFX *base, matrix_t *m, CNode3D *node, CRenderNode *r);
void CNodeFX_Spring_Reset(CNodeFX_Spring *self);

/* other modules (src/imports.inc) */
float Ang_Wrap(float a);	/* rva 0x238b80: one turn off when above 2 pi or below -2 pi */
matrix_t *Matrix_Transpose(matrix_t *out, const matrix_t *m);

#endif
