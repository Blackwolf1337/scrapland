/*
 * Engine/NodeFX.cpp of the engine3d: the effects of the NODE_FX nodes (INodeFX, in CNode3D obj58), applied to the
 * world matrix of the node by CRenderNode_ApplyFX. The type comes from the "NodeEffect" section of the user properties
 * of the node: Wheel and Roll (CNodeFX_Wheel: turned by the way the node moved, plus a turn per second and a noise),
 * Spring, Cloth, Hair and Bust (CNodeFX_Spring: the node lags behind its parent like a weight on a spring, stepped
 * every NodeFXSpringMSec and blended in between).
 */
#ifndef SCRAP_NODEFX_H
#define SCRAP_NODEFX_H

#include "scrap/node3d.h"

#define INODEFX_VFTABLE GAME_VFTABLE(INodeFX_vftable)
#define CNODEFX_WHEEL_VFTABLE GAME_VFTABLE(CNodeFX_Wheel_vftable)
#define CNODEFX_SPRING_VFTABLE GAME_VFTABLE(CNodeFX_Spring_vftable)

/* the types vftable[1] returns */
enum { NODEFX_NONE = 0, NODEFX_WHEEL = 1, NODEFX_SPRING = 2 };

/* the slots of the vftable */
enum {
	NODEFX_VF_DELETE = 0,		/* (byte flags) */
	NODEFX_VF_TYPE = 1,		/* byte () */
	NODEFX_VF_APPLY = 2		/* int (matrix_t *m, CNode3D *node, CRenderNode *r): m changed, 0 */
};

typedef struct INodeFX {
	void **vftable;			/* 0x0 NODEFX_VF_* */
} INodeFX;
SD_SIZE(INodeFX, 4);

typedef struct CNodeFX_Wheel {
	void **vftable;			/* 0x00 */
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
	void **vftable;			/* 0x00 */
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
#define g_NodeFXSpringDamp1 GAME_GLOBAL_IN(float, GVar_NodeFXSpringDamp1, 16)	/* NodeFXSpringDamp1 */
#define g_NodeFXSpringDamp2 GAME_GLOBAL_IN(float, GVar_NodeFXSpringDamp2, 16)	/* NodeFXSpringDamp2 */
#define g_NodeFXSpringDebug GAME_GLOBAL_IN(int, GVar_NodeFXSpringDebug, 16)	/* NodeFXSpringDebug: the variables instead of the members */
#define g_NodeFXSpringMSec GAME_GLOBAL_IN(uint32_t, GVar_NodeFXSpringMSec, 16)	/* NodeFXSpringMSec, the step */
#define g_NodeFXSpringMidP GAME_GLOBAL_IN(float, GVar_NodeFXSpringMidP, 16)	/* NodeFXSpringMidP */
#define g_NodeFXSpringRot GAME_GLOBAL_IN(float, GVar_NodeFXSpringRot, 16)	/* NodeFXSpringRot */
#define g_NodeFXSpringRotX GAME_GLOBAL_IN(float, GVar_NodeFXSpringRotX, 16)	/* NodeFXSpringRotX, degrees */
#define g_NodeFXSpringShow GAME_GLOBAL_IN(int, GVar_NodeFXSpringShow, 16)	/* NodeFXSpringShow */
#define g_NodeFXSpringSprng GAME_GLOBAL_IN(float, GVar_NodeFXSpringSprng, 16)	/* NodeFXSpringSprng */
#define g_NodeFXWheelRadius GAME_GLOBAL_IN(float, GVar_NodeFXWheelRadius, 16)	/* NodeFXWheelRadius */
#define g_NodeFXWheelShow GAME_GLOBAL_IN(int, GVar_NodeFXWheelShow, 16)	/* NodeFXWheelShow */
#define g_NodeFXWheelSlide GAME_GLOBAL_IN(float, GVar_NodeFXWheelSlide, 16)	/* NodeFXWheelSlide */
/* the seconds of the frame */
#define g_FrameSec GAME_GLOBAL(float, g_FrameSec)
/* the section "NodeEffect" of the user properties */
#define g_NodeEffectSect GAME_GLOBAL(const char *, g_NodeEffectSect)

/* the effect of type (NULL: "Type" of the NodeEffect section of the node), NULL for an unknown one */
void *NodeFX_New(const char *type, CNode3D *node);
/* a copy of the effect, NULL for an unknown type */
void *NodeFX_Clone(INodeFX *fx);

void __fastcall INodeFX_ctor(INodeFX *self);
INodeFX *__thiscall INodeFX_Delete(INodeFX *self, byte flags);
byte __fastcall INodeFX_Type(INodeFX *self);

/* type 0 Wheel, 1 Roll: the defaults of the properties */
CNodeFX_Wheel *__thiscall CNodeFX_Wheel_ctor(CNodeFX_Wheel *self, CNode3D *node, int type);
CNodeFX_Wheel *__thiscall CNodeFX_Wheel_ctorCopy(CNodeFX_Wheel *self, const CNodeFX_Wheel *src);
/* not to be used: CHECK(0) */
CNodeFX_Wheel *__thiscall CNodeFX_Wheel_Delete(CNodeFX_Wheel *self, byte flags);
byte __fastcall CNodeFX_Wheel_Type(CNodeFX_Wheel *self);
int __thiscall CNodeFX_Wheel_Apply(CNodeFX_Wheel *self, matrix_t *m, CNode3D *node, CRenderNode *r);
/* ang 0 and a new noiseBase; the quotient of the random number by 20000 */
int __fastcall CNodeFX_Wheel_Reset(CNodeFX_Wheel *self);

/* type 0 Spring, 1 Cloth, 2 Hair, 3 Bust */
CNodeFX_Spring *__thiscall CNodeFX_Spring_ctor(CNodeFX_Spring *self, CNode3D *node, int type);
CNodeFX_Spring *__thiscall CNodeFX_Spring_ctorCopy(CNodeFX_Spring *self, const CNodeFX_Spring *src);
CNodeFX_Spring *__thiscall CNodeFX_Spring_Delete(CNodeFX_Spring *self, byte flags);
byte __fastcall CNodeFX_Spring_Type(CNodeFX_Spring *self);
int __thiscall CNodeFX_Spring_Apply(CNodeFX_Spring *self, matrix_t *m, CNode3D *node, CRenderNode *r);
void __fastcall CNodeFX_Spring_Reset(CNodeFX_Spring *self);

/* other modules (src/imports.inc) */
float Ang_Wrap(float a);	/* rva 0x238b80: one turn off when above 2 pi or below -2 pi */
matrix_t *Matrix_Transpose(matrix_t *out, const matrix_t *m);

#endif
