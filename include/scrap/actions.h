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
	int node;			/* render node of the model, ACT_NODE_ */
	float ang[ACT_NODEMOD_ANGS][2];	/* "NodeN_Mx", "NodeN_My" */
} cActNodeMod;

/* the camera of a class (radians): the distance and angles and the FOV that zooms between MinFOV and MaxFOV */
typedef struct cActClassCam {
	float minFov;			/* "MinFOV" */
	float maxFov;			/* "MaxFOV" */
	float fovRange;			/* |maxFov - minFov| */
	float fovTime;			/* "FOVInterpTime" of the zoom */
	float fovLeft;			/* the zoom time left, back to the minimum */
	float fovStart;			/* the zoom time when it went back */
	float camDist;			/* "CamDist" */
	float camAng;			/* "CamAng" */
	float camRot;			/* "CamRot" */
	float fov;			/* "FOV" without the zoom */
} cActClassCam;

/* an action class (made by cActClassList_Create) */
struct cActClass {
	char *name;			/* cString */
	HashTable actions;		/* the actions by name, 457 slots */
	cAction *first;			/* the actions in a list (cAction.next) */
	cAction *last;			/* the last action of the list */
	char *modelFile;		/* "ModelFilePath", Mem_Alloc copy */
	cAction *defAction;		/* "DefAction" */
	CGameModel model;
	cStringList sounds;		/* the sound names of "AddSoundRes" */
	int users;			/* the controllers on the class; Del refuses while it has any */
	cActNodeMod nodes[ACT_NODEMODS];	/* the end is the first node -1 */
	cActClassCam cam;
	float turnAngLimit;		/* "TurnAngLimit": the Turn action plays for a bigger turn of the view */
	vec3_t soundDisp;		/* "SoundDispX".."Z" */
	int pretransformChan;		/* "PretransformChan" */
	float minTimeWalking;		/* "MinTimeWalking": the walk is kept that long after the stop */
	float maxTimeToExtraRelax;	/* the time between two relax actions */
	float minTimeToExtraRelax;
	float minFallDist;		/* "MinFallDist" */
	char *fallSound;		/* "FallSound" cString */
	float fallSoundVol;		/* "FallSoundVol" */
	float wRadius;			/* "WRadius" */
	float vRadius;			/* "VRadius" */
	float sRadius;			/* "SRadius": of the sphere of the floor test */
	float flyAccel;			/* "FlyAccel" */
	float flyFric;			/* "FlyFric" */
	float flySpeed;			/* "FlySpeed" */
	float flyFallAccel;		/* "FlyFallAccel" */
	float yModelDisp;		/* "YModelDisp" */
	byte canBePossessed;
	byte discardable;
	byte canFly;
	byte canJump;			/* the fall may go up */
	byte strafeChar;		/* the actions set the view angles (the view lock of the walk) */
	byte fireAsSelect;
	byte skipInitOnChange;		/* no init action between the walk actions */
	byte relaxAlarm;		/* the alarm versions of Relax, Turn and Run */
	float initEndWalkInterpFactor;	/* the speed of the walk end action */
	cPy_Function actionPress;
	cPy_Function actionRelease;
	cPy_Function photoPress;
	cPy_Function possessionPress;
	cPy_Function onCreate;
	cPy_Function onFreeResources;
	byte dynShip;			/* "DinaShip": the controller moves the parts of a ship */
	byte pad_209[3];
};

struct cActClassList {
	HashTable classes;		/* 457 slots */
};

/* an event of an action: Func is called with the entity name, the event name and the time */
struct cActEvent {
	float time;
	char *name;			/* cString, "Noname" */
	cPy_Function func;		/* "Func" */
	cActEvent *next;		/* by time */
	byte discardable;		/* skipped when a new action jumps over it */
	byte pad_21[3];
};

struct cAction {
	int anim;			/* the animation of the class model, -1 none */
	char *name;			/* cString */
	char *anmFile;			/* "AnmFilePath", Mem_Alloc copy */
	char *anmName;			/* its name part ("AnmName") */
	cActClass *parent;
	cAction *next;			/* in the list of the class */
	float anmTotalTime;		/* "AnmTotalTime" */
	float anmFps;			/* "AnmFPS" */
	cActEvent *events;		/* by time */
	cActEvent *lastEvent;		/* of "AddEventAtTime", the "Event..." variables */
	int nodeModifier;		/* "NodeModifier", the angle pair of the node turns */
	float angleModifierSpeed;
	byte isLoop;
	byte pad_35[3];
	float rotSpeed;			/* "RotSpeed": of the body to the view */
	float angRotLimit;		/* "AngRotLimit": between the body and the view (the view lock) */
	float maxSpeed;			/* the animation speed at the walk speed 1 */
	float minSpeed;			/* at 0 */
	byte useDisp;			/* the walk takes the motion of the root node */
	byte useFlyDisp;		/* the control moves it */
	byte useFlyFall;		/* flies without the control */
	byte pad_4b;
	int slideAsFly;			/* 0 the root motion moves it (and the strafe actions play), 1..4 the acceleration */
	float autoFaceSpeed;		/* the turn of the view to the listener per second */
	float autoFaceOffsetH;
	float autoFaceOffsetV;
	float interpTime;		/* "InterpTime" of the blend into it */
	byte anmConeView;		/* the head angle from the camera node */
	byte pad_61[3];
	int priority;			/* "PriorityLevel" */
	int chanel;			/* "Chanel": its layer */
};
/* the class name in the exe */
typedef cAction cActionMove;

/* a layer of a controller: the time in its action and its next event */
typedef struct cActLayer {
	float time;
	cAction *action;
	cActEvent *event;
	byte on;
	byte pad_d[3];
} cActLayer;

/* the blend of the angles of a turned node from the last NodeModifier to the new one */
typedef struct cActHead {
	float from[2];
	float to[2];
	float cur[2];
} cActHead;

/* the action controller of an entity */
struct cActCtrl {
	cEntity *owner;
	cActClass *cls;
	cAction *defLoop;		/* layer 0 goes back to it */
	cActLayer layers[ACT_LAYERS];
	int headMod;			/* the NodeModifier the node turns blend to, -1 at the start */
	float headBlend;		/* 0..1 */
	cActHead *heads;		/* new[] of ACT_NODEMODS, made on the first turn */
	vec3_t rootSum;			/* the root motion of layer 0 since the start */
	float rootTime;
	vec3_t rootMove;		/* rootSum per second */
	float walkSpeed;		/* 0..1 between the MinSpeed and MaxSpeed of the actions */
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
/* a watcher's proxy: the layers' actions (index + 1 in the class, 0 none) of the network at their rates (time a
 * second, 0 keeps it), the model advanced by dt; no events */
void cActCtrl_NetDrive(cActCtrl *self, const uint16_t *act, const float *rate, int layers, float dt);
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
