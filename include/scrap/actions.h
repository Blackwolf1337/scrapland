/*
 * Action classes of animated objects (original source: Actions/, src/game/Actions/). A class (cActClass) holds the
 * model, named variables and its actions (cAction, an animation of the model with timed events); SAct works on one
 * "current" class and action at a time. An entity with a class plays its actions through an action controller
 * (cActCtrl): four layers (the animation channels of the model), each with the action that runs and its next event,
 * and the default loop that layer 0 goes back to.
 */
#ifndef SCRAP_ACTIONS_H
#define SCRAP_ACTIONS_H

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

#define ACT_EMPTY GAME_STR(str_Empty, 0x3a3228)		/* "" of the exe */

/* the first slot of the vftables of the classes here: the deleting destructor */
typedef struct cActVtbl {
	void *(__thiscall *Delete)(void *self, byte flags);	/* 0x00 */
} cActVtbl;

#define CACTCLASS_VFTABLE GAME_VFTABLE(cActClass_vftable, 0x3a3224)
#define CACTCLASSLIST_VFTABLE GAME_VFTABLE(cActClassList_vftable, 0x3a3734)
#define CACTCTRL_VFTABLE GAME_VFTABLE(cActCtrl_vftable, 0x3a38a0)
#define CACTIONMOVE_VFTABLE GAME_VFTABLE(cActionMove_vftable, 0x3a3a74)

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
SD_SIZE(cActNodeMod, 0x3c);

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
SD_SIZE(cActClassCam, 0x28);

/* an action class (0x20c bytes, made by cActClassList_Create) */
struct cActClass {
	const cActVtbl *vftable;		/* 0x000 */
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
	float turnAngLimit;		/* 0x138 "TurnAngLimit" */
	vec3_t soundDisp;		/* 0x13c "SoundDispX".."Z" */
	int pretransformChan;		/* 0x148 "PretransformChan" */
	float minTimeWalking;		/* 0x14c "MinTimeWalking" */
	float maxTimeToExtraRelax;	/* 0x150 */
	float minTimeToExtraRelax;	/* 0x154 */
	float minFallDist;		/* 0x158 "MinFallDist" */
	char *fallSound;		/* 0x15c "FallSound" cString */
	float fallSoundVol;		/* 0x160 "FallSoundVol" */
	float wRadius;			/* 0x164 "WRadius" */
	float vRadius;			/* 0x168 "VRadius" */
	float sRadius;			/* 0x16c "SRadius" */
	float flyAccel;			/* 0x170 "FlyAccel" */
	float flyFric;			/* 0x174 "FlyFric" */
	float flySpeed;			/* 0x178 "FlySpeed" */
	float flyFallAccel;		/* 0x17c "FlyFallAccel" */
	float yModelDisp;		/* 0x180 "YModelDisp" */
	byte canBePossessed;		/* 0x184 */
	byte discardable;		/* 0x185 */
	byte canFly;			/* 0x186 */
	byte canJump;			/* 0x187 */
	byte strafeChar;		/* 0x188 */
	byte fireAsSelect;		/* 0x189 */
	byte skipInitOnChange;		/* 0x18a */
	byte relaxAlarm;		/* 0x18b */
	float initEndWalkInterpFactor;	/* 0x18c */
	cPy_Function actionPress;	/* 0x190 */
	cPy_Function actionRelease;	/* 0x1a4 */
	cPy_Function photoPress;	/* 0x1b8 */
	cPy_Function possessionPress;	/* 0x1cc */
	cPy_Function onCreate;		/* 0x1e0 */
	cPy_Function onFreeResources;	/* 0x1f4 */
	byte dinaShip;			/* 0x208 "DinaShip": the controller moves the parts of a ship */
	byte pad_209[3];
};
SD_OFFSET(cActClass, defAction, 0x20);
SD_OFFSET(cActClass, sounds, 0x40);
SD_OFFSET(cActClass, nodes, 0x5c);
SD_OFFSET(cActClass, cam, 0x110);
SD_OFFSET(cActClass, fallSound, 0x15c);
SD_OFFSET(cActClass, initEndWalkInterpFactor, 0x18c);
SD_OFFSET(cActClass, onFreeResources, 0x1f4);
SD_SIZE(cActClass, 0x20c);

/* the classes by name (0x10 bytes, g_pActClassList) */
struct cActClassList {
	const cActVtbl *vftable;		/* 0x00 */
	HashTable classes;		/* 0x04 457 slots */
};
SD_SIZE(cActClassList, 0x10);

/* an event of an action: Func is called with the entity name, the event name and the time */
struct cActEvent {
	float time;			/* 0x00 */
	char *name;			/* 0x04 cString, "Noname" */
	cPy_Function func;		/* 0x08 "Func" */
	cActEvent *next;		/* 0x1c by time */
	byte discardable;		/* 0x20 skipped when a new action jumps over it */
	byte pad_21[3];
};
SD_SIZE(cActEvent, 0x24);

/* an action (cActionMove, 0x6c bytes) */
struct cAction {
	const cActVtbl *vftable;		/* 0x00 */
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
	float rotSpeed;			/* 0x38 "RotSpeed" */
	float angRotLimit;		/* 0x3c "AngRotLimit" */
	float maxSpeed;			/* 0x40 the animation speed at the walk speed 1 */
	float minSpeed;			/* 0x44 at 0 */
	byte useDisp;			/* 0x48 */
	byte useFlyDisp;		/* 0x49 */
	byte useFlyFall;		/* 0x4a */
	byte pad_4b;
	int slideAsFly;			/* 0x4c */
	float autoFaceSpeed;		/* 0x50 */
	float autoFaceOffsetH;		/* 0x54 */
	float autoFaceOffsetV;		/* 0x58 */
	float interpTime;		/* 0x5c "InterpTime" of the blend into it */
	byte anmConeView;		/* 0x60 */
	byte pad_61[3];
	int priority;			/* 0x64 "PriorityLevel" */
	int chanel;			/* 0x68 "Chanel": its layer */
};
SD_OFFSET(cAction, events, 0x24);
SD_OFFSET(cAction, isLoop, 0x34);
SD_OFFSET(cAction, slideAsFly, 0x4c);
SD_OFFSET(cAction, priority, 0x64);
SD_SIZE(cAction, 0x6c);

/* a layer of a controller: the time in its action and its next event */
typedef struct cActLayer {
	float time;			/* 0x00 */
	cAction *action;		/* 0x04 */
	cActEvent *event;		/* 0x08 */
	byte on;			/* 0x0c */
	byte pad_d[3];
} cActLayer;
SD_SIZE(cActLayer, 0x10);

/* the blend of the angles of a turned node from the last NodeModifier to the new one */
typedef struct cActHead {
	float from[2];			/* 0x00 */
	float to[2];			/* 0x08 */
	float cur[2];			/* 0x10 */
} cActHead;
SD_SIZE(cActHead, 0x18);

/* the action controller of an entity (0x7c bytes) */
struct cActCtrl {
	const cActVtbl *vftable;		/* 0x00 */
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
SD_OFFSET(cActCtrl, layers, 0x10);
SD_OFFSET(cActCtrl, headMod, 0x50);
SD_OFFSET(cActCtrl, rootSum, 0x5c);
SD_SIZE(cActCtrl, 0x7c);

/* Actions/ActClass.cpp (src/game/Actions/ActClass.c) */
cActClass *__thiscall cActClass_ctor(cActClass *self, const char *name);
void __fastcall cActClass_dtor(cActClass *self);
cActClass *__thiscall cActClass_Delete(cActClass *self, byte flags);
cActNodeMod *__fastcall cActNodeMod_ctor(cActNodeMod *self);
cAction *__thiscall cActClass_CreateAction(cActClass *cls, const char *name);
cAction *__thiscall cActClass_GetAction(cActClass *cls, const char *name);
/* set != 0: writes the string *s, or the number *d / *i when s is NULL; set == 0: reads into all three.
   Returns the ACTVAR_ type (0 if not found). */
int __thiscall cActClass_Var(cActClass *cls, const char *name, int set, char **s, double *d, int *i);
/* the sound of the file loaded and its name kept for FreeResources */
void __thiscall cActClass_AddSoundRes(cActClass *self, const char *file);
/* OnFreeResources called, the sounds and the model released */
void __fastcall cActClass_FreeResources(cActClass *self);
/* the camera variables of the class (only d is used) */
int __thiscall cActClassCam_Var(cActClassCam *self, const char *name, int set, char **s, double *d, int *i);
/* the camera of the class into the follow camera of e, the FOV zoom of the class "Functionary" with it */
void __thiscall cActClassCam_Update(void *cam, cEntity *followCam, cEntity *e);
/* NULL when there is no action of that name */
cAction *__thiscall cActClass_SetDefAction(cActClass *self, const char *name);
/* a warning if the class has a model already */
void __thiscall cActClass_SetModelFile(cActClass *self, const char *file);
/* "_Move_The_Model_" or a render node of the model; a message if it has no such node */
void __thiscall cActNodeMod_SetNode(int *node, CGameModel *model, const char *name);

/* the instance of Global/DList.h with a cString (cStringList of filepak.h) */
cStringNode *__thiscall cStringList_NewNode(cStringList *list);
void __fastcall cStringList_Clear(cStringList *list);
void __fastcall cStringList_FreePool(cStringList *list);
void __fastcall cStringList_RemoveHead(cStringList *list);
void __fastcall cStringList_dtor(cStringList *list);

/* Actions/ActClassList.cpp (src/game/Actions/ActClassList.c) */
cActClassList *__fastcall cActClassList_ctor(cActClassList *self);
void __fastcall cActClassList_dtor(cActClassList *self);
cActClassList *__thiscall cActClassList_Delete(cActClassList *self, byte flags);
cActClass *__thiscall cActClassList_Create(cActClassList *list, const char *name);
cActClass *__thiscall cActClassList_Get(cActClassList *list, const char *name);
/* 0 if there is no such class or a controller uses it */
byte __thiscall cActClassList_Del(cActClassList *list, const char *name);
void ActClassList_Done(void);
void ActClassList_Init(void);

/* Actions/ActCtrl.cpp (src/game/Actions/ActCtrl.c). The functions the entities call take a void * like their
 * callers declared them first. */
cActCtrl *__thiscall cActCtrl_ctor(cActCtrl *self, cEntity *owner, cActClass *cls);
void __fastcall cActCtrl_dtor(cActCtrl *self);
cActCtrl *__thiscall cActCtrl_Delete(cActCtrl *self, byte flags);
/* the model command (CGameModel_AnimCmd) on the model of the owner */
void __thiscall cActCtrl_AnimCmd(cActCtrl *self, const char *cmd);
/* the model of the class on the owner with the animations of the actions */
void __fastcall cActCtrl_InitModel(cActCtrl *self);
/* the layers dt on: speeds, events, the end of the actions, the root motion of layer 0 */
void __thiscall cActCtrl_Update(cActCtrl *self, float dt);
/* the events of the layer chan up to t (FLT_MAX from the end of the action on); silent skips the discardable ones
 * and logs with Con_Verbosef */
void __thiscall cActCtrl_FireEvents(cActCtrl *self, int chan, float t, byte silent);
byte __thiscall cActCtrl_VarI(cActCtrl *self, const char *name, int *i, byte set);
byte __thiscall cActCtrl_VarF(cActCtrl *self, const char *name, double *d, byte set);
byte __thiscall cActCtrl_VarS(cActCtrl *self, const char *name, char **s, byte set);
byte __thiscall cActCtrl_VarV(cActCtrl *self, const char *name, double *x, double *y, double *z, byte set);
/* the name of the action of layer i, "" when it does not run (rva 0x62c40) */
const char *__thiscall cActCtrl_GetActionName(void *ctrl, int i);
/* the layer i of the actions (16 bytes, the time of its action first) */
float *__thiscall cActCtrl_GetLayer(void *ctrl, int i);
/* the name of the default loop action of the controller, "" without one (rva 0x62c80) */
const char *__thiscall cActCtrl_GetDefLoopName(void *ctrl);
void __fastcall cActCtrl_InitHeads(cActCtrl *self);
/* the turned nodes of the class by ang (x, y) blended to the NodeModifier of the action of layer 0 */
void __thiscall cActCtrl_TurnHead(void *ctrl, const float *ang, float dt);
/* the action of the controller (not when it runs already in its layer and one of priority runs); 0 if not */
byte __thiscall cActCtrl_PlayAction(void *ctrl, void *action, int arg, int set, float speed);
byte __thiscall cActCtrl_Play(void *ctrl, const char *action, int arg, float speed);
/* PlayAction unless it runs already in its layer; 0 then or without it */
byte __thiscall cActCtrl_PlayActionNew(cActCtrl *self, cAction *action, int arg, float speed);
/* the action, unless it runs already in its layer; 0 then or without it (rva 0x631c0) */
byte __thiscall cActCtrl_PlayNew(void *ctrl, const char *action, int arg, float speed);
/* the action that runs when nothing else does; a warning if the class has no such action */
void __thiscall cActCtrl_SetDefLoop(void *ctrl, const char *action, byte force);
/* the update of the slot: the ship parts with DinaShip, else cActCtrl_Update */
void __thiscall cActCtrl_Tick(cActCtrl *self, float dt);
/* layer i (> 0) stopped */
void __thiscall cActCtrl_StopChan(cActCtrl *self, int i);
byte __thiscall cActCtrl_IsActive(void *ctrl, const char *action);
byte __thiscall cActCtrl_IsDefLoop(cActCtrl *self, const char *action);
/* the target of the blend set (the old one becomes the start) */
void __thiscall cActHead_Set(cActHead *self, const float *to);
void __thiscall cActHead_Blend(cActHead *self, float t);
/* the update of DinaShip classes: the parts of the ship (Motor, Rudder, Flaps, ...) by its state */
void __thiscall cActCtrl_UpdateShip(cActCtrl *self, float dt);

/* Actions/Actions.cpp (src/game/Actions/Actions.c) */
cAction *__thiscall cAction_ctor(cAction *self, cActClass *parent, const char *name);
void __fastcall cAction_dtor(cAction *self);
cAction *__thiscall cAction_Delete(cAction *self, byte flags);
cActEvent *__thiscall cActEvent_ctor(cActEvent *self, float time, cActEvent *next);
/* a new event at time (after the ones before it), as the lastEvent */
void __thiscall cAction_AddEvent(cAction *self, float time);
void __thiscall cActEvent_Fire(cActEvent *self, const char *entity);
int __thiscall cAction_Var(cAction *act, const char *name, int set, char **s, double *d, int *i);
/* the variables after "Event": of the last event */
int __thiscall cActEvent_Var(cActEvent *self, const char *name, int set, char **s, double *d, int *i);
/* the animation of the file added to the model of the class; a warning if the action has one */
void __thiscall cAction_SetAnmFile(cAction *self, const char *file);

/* Actions/Events/CharEvents.cpp and SoundEvents.cpp (src/game/Actions/Events/): the C functions ("!name") the
 * events call, named like the registry; the parameters (entity name, value string, time) come in a memory file */
void Event_AnmChangeRandomAnm(cAbstractFile *params);	/* "base:max": base and a random number up to max */
void Event_AnmCommand(cAbstractFile *params);
void Event_AnmSetAction(cAbstractFile *params);
void Event_AnmSetDefaultLoop(cAbstractFile *params);
void Event_AnmSetDisplAng(cAbstractFile *params);
void Event_AnmStopChan(cAbstractFile *params);
void Event_AnmSound2DEvent(cAbstractFile *params);	/* "sound:volume[:pitch[:pan]]" */
void Event_AnmSoundEvent(cAbstractFile *params);	/* "sound:volume[:pitch]" at the entity */
void Event_AnmSoundLoopEvent(cAbstractFile *params);	/* "index:volume:pitch" of a sound of the entity */
void Event_StopStreamingVoice(cAbstractFile *params);
void Event_StreamingVoice(cAbstractFile *params);	/* "text:volume[:dist]" */
void Event_WarningPolice(cAbstractFile *params);

#define g_pActClassList GAME_GLOBAL(cActClassList *, g_pActClassList, 0x444080)
#define g_pAuxActClass GAME_GLOBAL(cActClass *, g_pAuxActClass, 0x444084)	/* SAct's current class */
#define g_pAuxAction GAME_GLOBAL(cAction *, g_pAuxAction, 0x444088)	/* SAct's current action */
#define g_LogActEvents GAME_GLOBAL_IN(int, GVar_LogActEvents, 16, 0x44409c)		/* LogActEvents: the events into the console */
#define g_DefInterpTime GAME_GLOBAL_IN(float, GVar_DefInterpTime, 16, 0x4440b4)	/* DefInterpTime of new actions */

#endif
