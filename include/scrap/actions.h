/*
 * Action classes of animated objects (original source: Actions/, src/game/Actions/). A class (cActClass) holds the
 * model, named variables and its actions (cAction, an animation of the model with timed events); SAct works on one
 * "current" class and action at a time. An entity with a class plays its actions through an action controller
 * (cActCtrl): four layers (the animation channels of the model), each with the action that runs and its next event,
 * and the default loop that layer 0 goes back to.
 */
#ifndef SCRAP_ACTIONS_H
#define SCRAP_ACTIONS_H

#include "scrap/gvar.h"
#include "scrap/entity.h"
#include "scrap/filepak.h"
#include "scrap/game.h"
#include "scrap/pyfunction.h"
#include "scrap/scrapmodel.h"

typedef struct cActClass cActClass;
typedef struct cActClassList cActClassList;
typedef struct cAction cAction;
typedef struct cActEvent cActEvent;
typedef struct cActCtrl cActCtrl;

/* type of a variable as returned by the Var functions */
enum { ACTVAR_NONE = 0, ACTVAR_STRING = 1, ACTVAR_DOUBLE = 2, ACTVAR_INT = 3 };

#define ACT_LAYERS 4			/* the layers of a controller, the channels of the model */
#define ACT_NODEMODS 3			/* the turned nodes of a class ("NodeN_...") */
#define ACT_NODEMOD_ANGS 7		/* the angle pairs of a node, by the NodeModifier of the action */
#define ACT_NODE_NONE (-1)
#define ACT_NODE_MODEL (-2)		/* "_Move_The_Model_": the angles turn the whole model */

/* the turn of a node of the model with the head: the node and its angle pairs (x, y) */
typedef struct cActNodeMod {
	int node;			/* 0x00 render node of the model, ACT_NODE_ */
	float ang[ACT_NODEMOD_ANGS][2];	/* 0x04 "NodeN_Mx", "NodeN_My" */
} cActNodeMod;

/* the camera of a class (radians): the distance and angles and the FOV that zooms between MinFOV and MaxFOV */
typedef struct cActClassCam {
	float minFov;			/* 0x00 "MinFOV" */
	float maxFov;			/* 0x04 "MaxFOV" */
	float fovRange;			/* 0x08 |maxFov - minFov| */
	float fovTime;			/* 0x0c "FOVInterpTime" of the zoom */
	float fovLeft;			/* 0x10 the zoom time left, back to the minimum */
	float fovStart;			/* 0x14 the zoom time when it went back */
	float camDist;			/* 0x18 "CamDist" */
	float camAng;			/* 0x1c "CamAng" */
	float camRot;			/* 0x20 "CamRot" */
	float fov;			/* 0x24 "FOV" without the zoom */
} cActClassCam;

/* an action class (0x20c bytes, made by cActClassList_Create) */
struct cActClass {
	char *name;			/* 0x004 cString */
	HashTable actions;		/* 0x008 the actions by name, 457 slots */
	cAction *first;			/* 0x014 the actions in a list (cAction.next) */
	cAction *last;			/* 0x018 the last action of the list */
	char *modelFile;		/* 0x01c "ModelFilePath", Mem_Alloc copy */
	cAction *defAction;		/* 0x020 "DefAction" */
	CGameModel model;		/* 0x024 */
	cStringList sounds;		/* 0x040 the sound names of "AddSoundRes" */
	int users;			/* 0x058 the controllers on the class; Del refuses while it has any */
	cActNodeMod nodes[ACT_NODEMODS];	/* 0x05c the end is the first node -1 */
	cActClassCam cam;		/* 0x110 */
	float turnAngLimit;		/* 0x138 "TurnAngLimit": the Turn action plays for a bigger turn of the view */
	vec3_t soundDisp;		/* 0x13c "SoundDispX".."Z" */
	int pretransformChan;		/* 0x148 "PretransformChan" */
	float minTimeWalking;		/* 0x14c "MinTimeWalking": the walk is kept that long after the stop */
	float maxTimeToExtraRelax;	/* 0x150 the time between two relax actions */
	float minTimeToExtraRelax;	/* 0x154 */
	float minFallDist;		/* 0x158 "MinFallDist" */
	char *fallSound;		/* 0x15c "FallSound" cString */
	float fallSoundVol;		/* 0x160 "FallSoundVol" */
	float wRadius;			/* 0x164 "WRadius" */
	float vRadius;			/* 0x168 "VRadius" */
	float sRadius;			/* 0x16c "SRadius": of the sphere of the floor test */
	float flyAccel;			/* 0x170 "FlyAccel" */
	float flyFric;			/* 0x174 "FlyFric" */
	float flySpeed;			/* 0x178 "FlySpeed" */
	float flyFallAccel;		/* 0x17c "FlyFallAccel" */
	float yModelDisp;		/* 0x180 "YModelDisp" */
	byte canBePossessed;		/* 0x184 */
	byte discardable;		/* 0x185 */
	byte canFly;			/* 0x186 */
	byte canJump;			/* 0x187 the fall may go up */
	byte strafeChar;		/* 0x188 the actions set the view angles (the view lock of the walk) */
	byte fireAsSelect;		/* 0x189 */
	byte skipInitOnChange;		/* 0x18a no init action between the walk actions */
	byte relaxAlarm;		/* 0x18b the alarm versions of Relax, Turn and Run */
	float initEndWalkInterpFactor;	/* 0x18c the speed of the walk end action */
	cPy_Function actionPress;	/* 0x190 */
	cPy_Function actionRelease;	/* 0x1a4 */
	cPy_Function photoPress;	/* 0x1b8 */
	cPy_Function possessionPress;	/* 0x1cc */
	cPy_Function onCreate;		/* 0x1e0 */
	cPy_Function onFreeResources;	/* 0x1f4 */
	byte dynShip;			/* 0x208 "DinaShip": the controller moves the parts of a ship */
	byte pad_209[3];
};

struct cActClassList {
	HashTable classes;		/* 0x04 457 slots */
};

/* an event of an action: Func is called with the entity name, the event name and the time */
struct cActEvent {
	float time;			/* 0x00 */
	char *name;			/* 0x04 cString, "Noname" */
	cPy_Function func;		/* 0x08 "Func" */
	cActEvent *next;		/* 0x1c by time */
	byte discardable;		/* 0x20 skipped when a new action jumps over it */
	byte pad_21[3];
};

struct cAction {
	int anim;			/* 0x04 the animation of the class model, -1 none */
	char *name;			/* 0x08 cString */
	char *anmFile;			/* 0x0c "AnmFilePath", Mem_Alloc copy */
	char *anmName;			/* 0x10 its name part ("AnmName") */
	cActClass *parent;		/* 0x14 */
	cAction *next;			/* 0x18 in the list of the class */
	float anmTotalTime;		/* 0x1c "AnmTotalTime" */
	float anmFps;			/* 0x20 "AnmFPS" */
	cActEvent *events;		/* 0x24 by time */
	cActEvent *lastEvent;		/* 0x28 of "AddEventAtTime", the "Event..." variables */
	int nodeModifier;		/* 0x2c "NodeModifier", the angle pair of the node turns */
	float angleModifierSpeed;	/* 0x30 */
	byte isLoop;			/* 0x34 */
	byte pad_35[3];
	float rotSpeed;			/* 0x38 "RotSpeed": of the body to the view */
	float angRotLimit;		/* 0x3c "AngRotLimit": between the body and the view (the view lock) */
	float maxSpeed;			/* 0x40 the animation speed at the walk speed 1 */
	float minSpeed;			/* 0x44 at 0 */
	byte useDisp;			/* 0x48 the walk takes the motion of the root node */
	byte useFlyDisp;		/* 0x49 the control moves it */
	byte useFlyFall;		/* 0x4a flies without the control */
	byte pad_4b;
	int slideAsFly;			/* 0x4c 0 the root motion moves it (and the strafe actions play), 1..4 the acceleration */
	float autoFaceSpeed;		/* 0x50 the turn of the view to the listener per second */
	float autoFaceOffsetH;		/* 0x54 */
	float autoFaceOffsetV;		/* 0x58 */
	float interpTime;		/* 0x5c "InterpTime" of the blend into it */
	byte anmConeView;		/* 0x60 the head angle from the camera node */
	byte pad_61[3];
	int priority;			/* 0x64 "PriorityLevel" */
	int chanel;			/* 0x68 "Chanel": its layer */
};
/* the class name in the exe */
typedef cAction cActionMove;

/* a layer of a controller: the time in its action and its next event */
typedef struct cActLayer {
	float time;			/* 0x00 */
	cAction *action;		/* 0x04 */
	cActEvent *event;		/* 0x08 */
	byte on;			/* 0x0c */
	byte pad_d[3];
} cActLayer;

/* the blend of the angles of a turned node from the last NodeModifier to the new one */
typedef struct cActHead {
	float from[2];			/* 0x00 */
	float to[2];			/* 0x08 */
	float cur[2];			/* 0x10 */
} cActHead;

/* the action controller of an entity (0x7c bytes) */
struct cActCtrl {
	cEntity *owner;			/* 0x04 */
	cActClass *cls;			/* 0x08 */
	cAction *defLoop;		/* 0x0c layer 0 goes back to it */
	cActLayer layers[ACT_LAYERS];	/* 0x10 */
	int headMod;			/* 0x50 the NodeModifier the node turns blend to, -1 at the start */
	float headBlend;		/* 0x54 0..1 */
	cActHead *heads;		/* 0x58 new[] of ACT_NODEMODS, made on the first turn */
	vec3_t rootSum;			/* 0x5c the root motion of layer 0 since the start */
	float rootTime;			/* 0x68 */
	vec3_t rootMove;		/* 0x6c rootSum per second */
	float walkSpeed;		/* 0x78 0..1 between the MinSpeed and MaxSpeed of the actions */
};

/* Actions/ActClass.cpp (src/game/Actions/ActClass.c) */
cActClass *cActClass_ctor(cActClass *self, const char *name);
cAction *cActClass_CreateAction(cActClass *cls, const char *name);
cAction *cActClass_GetAction(cActClass *cls, const char *name);
/* set != 0: writes the string *s, or the number *d / *i when s is NULL; set == 0: reads into all three.
   Returns the ACTVAR_ type (0 if not found). */
int cActClass_Var(cActClass *cls, const char *name, int set, char **s, double *d, int *i);
/* the camera of the class into the follow camera of e, the FOV zoom of the class "Functionary" with it */
void cActClassCam_Update(void *cam, cEntity *followCam, cEntity *e);

/* the instance of Global/DList.h with a cString (cStringList of filepak.h) */
cStringNode *cStringList_NewNode(cStringList *list);
void cStringList_Clear(cStringList *list);
void cStringList_FreePool(cStringList *list);

cActClass *cActClassList_Create(cActClassList *list, const char *name);
cActClass *cActClassList_Get(cActClassList *list, const char *name);
/* 0 if there is no such class or a controller uses it */
byte cActClassList_Del(cActClassList *list, const char *name);
void ActClassList_Done(void);
void ActClassList_Init(void);

/* Actions/ActCtrl.cpp (src/game/Actions/ActCtrl.c). The functions the entities call take a void * like their
 * callers declared them first. */
cActCtrl *cActCtrl_ctor(cActCtrl *self, cEntity *owner, cActClass *cls);
/* the model command (CGameModel_AnimCmd) on the model of the owner */
void cActCtrl_AnimCmd(cActCtrl *self, const char *cmd);
/* the name of the action of layer i, "" when it does not run */
const char *cActCtrl_GetActionName(void *ctrl, int i);
/* the layer i of the actions (16 bytes, the time of its action first) */
cActLayer *cActCtrl_GetLayer(void *ctrl, int i);
/* the name of the default loop action of the controller, "" without one */
const char *cActCtrl_GetDefLoopName(void *ctrl);
/* the turned nodes of the class by ang (x, y) blended to the NodeModifier of the action of layer 0 */
void cActCtrl_TurnHead(void *ctrl, const float *ang, float dt);
/* the action of the controller (not when it runs already in its layer and one of priority runs); 0 if not */
byte cActCtrl_PlayAction(void *ctrl, void *action, int arg, int set, float speed);
byte cActCtrl_Play(void *ctrl, const char *action, int arg, float speed);
/* the action, unless it runs already in its layer; 0 then or without it */
byte cActCtrl_PlayNew(void *ctrl, const char *action, int arg, float speed);
/* the action that runs when nothing else does; a warning if the class has no such action */
void cActCtrl_SetDefLoop(void *ctrl, const char *action, byte force);
/* the update of the slot: the ship parts with DinaShip, else cActCtrl_Update */
void cActCtrl_Tick(cActCtrl *self, float dt);
/* layer i (> 0) stopped */
void cActCtrl_StopChan(cActCtrl *self, int i);
byte cActCtrl_IsActive(void *ctrl, const char *action);

/* Actions/Actions.cpp (src/game/Actions/Actions.c) */
cAction *cAction_ctor(cAction *self, cActClass *parent, const char *name);
void cActEvent_Fire(cActEvent *self, const char *entity);
int cAction_Var(cAction *act, const char *name, int set, char **s, double *d, int *i);

extern cActClassList *g_pActClassList;
extern cActClass *g_pAuxActClass;	/* SAct's current class */
extern cAction *g_pAuxAction;	/* SAct's current action */

void cActCtrl_Free(cActCtrl *self);
byte cActCtrl_VarI(cActCtrl *self, const char *name, int *i, int set);
byte cActCtrl_VarS(cActCtrl *self, const char *name, char **s, int set);
void cActClass_Free(cActClass *self);
void cAction_Free(cAction *self);

#endif
