/*
 * Engine/ControlAnim.cpp of the engine3d: CAnimCtrl, the play state of an animation (frames, speed, fade out) and
 * the walk through its sorted event list, with the root motion of the node taken out of the animation.
 */
#ifndef SCRAP_CONTROLANIM_H
#define SCRAP_CONTROLANIM_H

#include "scrap/animation.h"
#include "scrap/e3dutil.h"

/* CAnimCtrl.state */
enum { ANIM_REPR = 0, ANIM_PAUSE = 1, ANIM_FADE_PAUSE = 2, ANIM_END = 3 };

/* the events of the list (CAnimEvent.id) */
enum {
	EVENT_DEST_JUMP = 0,		/* a jump destination, nothing to do */
	EVENT_MOVEMENT = 1,		/* arg: root motion relative (b5c) */
	EVENT_SCALE = 4,		/* arg: the float of the root motion scale */
	EVENT_FRAME = 6,		/* arg set to g_FrameCount */
	EVENT_PAUSE = 9,
	EVENT_GO = 10,			/* arg: the float of the frame to go on from */
	EVENT_JUMP_COND = 11,		/* arg: a jump destination whose arg points to the int condition */
	EVENT_JUMP = 12,		/* arg: the destination (an EVENT_DEST_JUMP event); not with loop */
	EVENT_END = 13
};

/* an entry of the sorted event list of a group */
typedef struct CAnimEvent {
	int id;				/* EVENTO_* */
	intptr_t arg;			/* a number or an event or condition */
	struct CAnimEvent *next;
	float t;			/* the frame */
	uint32_t flags;			/* 1: in a list */
} CAnimEvent;

/* Engine/EventoAnim.cpp: an event of the time t; ctorAdd puts it into the list */
CAnimEvent *CAnimEvent_ctor(CAnimEvent *self, float t, int id, intptr_t arg);
/* the rest of the chain deleted */
void CAnimEvent_dtor(CAnimEvent *self);
CAnimEvent *AnimEvents_Add(CAnimEvent **list, CAnimEvent *ev);

/* called at each event before its default handling, which a return of 1 skips */
typedef byte (*animCallback_t)(int a, int b, CAnimEvent *ev);

typedef struct CAnimCtrl {
	float vel;			/* frames per second, 25 */
	float scaleMove;		/* the scale of the root motion, 1 */
	float factor;			/* of the time, 1 */
	float speed;			/* 0.12 */
	byte loop;			/* EVENT_JUMP ignored */
	byte pad_11[3];
	vec3_t move;			/* the root motion of the last update */
	vec3_t ofs;			/* the root offset */
	int jumps;			/* the jumps of the last update */
	CAnimNode *node;		/* the root node of the animation */
	int firstFrame;
	int frames;
	CAnimEvent **events;		/* the list head */
	animCallback_t callback;
	int cbA;
	int cbB;
	float step;			/* factor times the time of the last update */
	float frame;
	int state;			/* ANIM_* */
	int unused_58;
	byte relative;			/* EVENT_MOVEMENT: the y of the root motion goes into move, not ofs */
	byte pad_5d[3];
	float scale;			/* EVENT_SCALE, 1 */
	float fadeVel;			/* the fade per second */
	float fade;			/* from 1 down to 0.2 */
	vec3_t lastPos;			/* the root position at lastFrame */
	uint32_t time;			/* g_TimeMs of the last update */
	float lastFrame;
	CAnimEvent *event;		/* the next event */
} CAnimCtrl;

CAnimCtrl *CAnimCtrl_ctor(CAnimCtrl *self);
/* empty */
void CAnimCtrl_dtor(CAnimCtrl *self);
void CAnimCtrl_Play(CAnimCtrl *self);
/* stopped at frame, no event; 1 */
int CAnimCtrl_Reset(CAnimCtrl *self, float frame);
void CAnimCtrl_SetFrames(CAnimCtrl *self, int firstFrame, int frames);
/* playing from the first frame and the first event */
void CAnimCtrl_Start(CAnimCtrl *self);
/* the root position and rotation at frame (0 and the identity without them) */
byte CAnimCtrl_GetPosRot(CAnimCtrl *self, float frame, vec3_t *pos, quat_t *rot);
void CAnimCtrl_Stop(CAnimCtrl *self);
void CAnimCtrl_Pause(CAnimCtrl *self);
/* a fade out over t seconds, only while fading */
void CAnimCtrl_Fade(CAnimCtrl *self, float t);
/* t seconds on (t < 0: the time since the last update, at most 0.1), the root motion rotated by rot; 0 when stopped */
byte CAnimCtrl_Update(CAnimCtrl *self, const vec3_t *rot, float t);

#endif
