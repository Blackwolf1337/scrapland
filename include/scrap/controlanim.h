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

/* an entry of the sorted event list of a group (constructor rva 0x2bb8e0) */
typedef struct CAnimEvent {
	int id;				/* 0x00 EVENTO_* */
	int arg;			/* 0x04 */
	struct CAnimEvent *next;		/* 0x08 */
	float t;			/* 0x0c the frame */
	uint32_t flags;			/* 0x10 1: in a list */
} CAnimEvent;
SD_SIZE(CAnimEvent, 0x14);

/* Engine/EventoAnim.cpp: an event of the time t; ctorAdd puts it into the list */
CAnimEvent *CAnimEvent_ctor(CAnimEvent *self, float t, int id, int arg);
/* the rest of the chain deleted */
void CAnimEvent_dtor(CAnimEvent *self);
CAnimEvent *CAnimEvent_Delete(CAnimEvent *self, byte flags);
/* into the list sorted by t, then id; CHECK it is in none yet */
void CAnimEvent_Insert(CAnimEvent *self, CAnimEvent **list);
CAnimEvent *AnimEvents_Add(CAnimEvent **list, CAnimEvent *ev);

/* called at each event before its default handling, which a return of 1 skips */
typedef byte (*animCallback_t)(int a, int b, CAnimEvent *ev);

typedef struct CAnimCtrl {
	float vel;			/* 0x00 frames per second, 25 */
	float scaleMove;		/* 0x04 the scale of the root motion, 1 */
	float factor;			/* 0x08 of the time, 1 */
	float speed;			/* 0x0c 0.12 */
	byte loop;			/* 0x10 EVENT_JUMP ignored */
	byte pad_11[3];
	vec3_t move;			/* 0x14 the root motion of the last update */
	vec3_t ofs;			/* 0x20 the root offset */
	int jumps;			/* 0x2c the jumps of the last update */
	CAnimNode *node;		/* 0x30 the root node of the animation */
	int firstFrame;			/* 0x34 */
	int frames;			/* 0x38 */
	CAnimEvent **events;		/* 0x3c the list head */
	animCallback_t callback;	/* 0x40 */
	int cbA;			/* 0x44 */
	int cbB;			/* 0x48 */
	float step;			/* 0x4c factor times the time of the last update */
	float frame;			/* 0x50 */
	int state;			/* 0x54 ANIM_* */
	int unused_58;			/* 0x58 */
	byte relative;			/* 0x5c EVENT_MOVEMENT: the y of the root motion goes into move, not ofs */
	byte pad_5d[3];
	float scale;			/* 0x60 EVENT_SCALE, 1 */
	float fadeVel;			/* 0x64 the fade per second */
	float fade;			/* 0x68 from 1 down to 0.2 */
	vec3_t lastPos;			/* 0x6c the root position at lastFrame */
	uint32_t time;			/* 0x78 g_TimeMs of the last update */
	float lastFrame;		/* 0x7c */
	CAnimEvent *event;		/* 0x80 the next event */
} CAnimCtrl;
SD_SIZE(CAnimCtrl, 0x84);
SD_OFFSET(CAnimCtrl, frame, 0x50);
SD_OFFSET(CAnimCtrl, event, 0x80);

CAnimCtrl *CAnimCtrl_ctor(CAnimCtrl *self);
/* rva 0x250f10, empty */
void CAnimCtrl_dtor(CAnimCtrl *self);
void CAnimCtrl_Play(CAnimCtrl *self);
void CAnimCtrl_SetCallback(CAnimCtrl *self, animCallback_t fn, int a, int b);
/* stopped at frame, no event; 1 */
int CAnimCtrl_Reset(CAnimCtrl *self, float frame);
void CAnimCtrl_SetFrames(CAnimCtrl *self, int firstFrame, int frames);
const char *AnimCtrl_StateName(int state);
/* playing from the first frame and the first event */
void CAnimCtrl_Start(CAnimCtrl *self);
/* the root position and rotation at frame (0 and the identity without them) */
byte CAnimCtrl_GetPosRot(CAnimCtrl *self, float frame, vec3_t *pos, quat_t *rot);
void CAnimCtrl_Stop(CAnimCtrl *self);
void CAnimCtrl_Pause(CAnimCtrl *self);
/* a fade out over t seconds, only while fading */
void CAnimCtrl_Fade(CAnimCtrl *self, float t);
/* the events from *f1 up to *f2 (== *fin); 1 while there is more to go to *f2, which may be cut at an event */
byte CAnimCtrl_Events(CAnimCtrl *self, float *f1, float *f2, float *end);
/* t seconds on (t < 0: the time since the last update, at most 0.1), the root motion rotated by rot; 0 when stopped */
byte CAnimCtrl_Update(CAnimCtrl *self, const vec3_t *rot, float t);


#endif
