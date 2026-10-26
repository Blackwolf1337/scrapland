/*
 * Engine/ControlAnim.cpp of the engine3d: CAnimCtrl, the play state of an animation (frames, speed, fade out) and
 * the walk through its sorted event list, with the root motion of the node taken out of the animation.
 */
#ifndef SCRAP_CONTROLANIM_H
#define SCRAP_CONTROLANIM_H

#include "scrap/animacion.h"
#include "scrap/e3dutil.h"

/* CAnimCtrl.state */
enum { ANIM_REPR = 0, ANIM_PAUSA = 1, ANIM_FUND_PAUSA = 2, ANIM_FIN = 3 };

/* the events of the list (CAnimEvent.id) */
enum {
	EVENTO_DEST_SALTO = 0,		/* a jump destination, nothing to do */
	EVENTO_MOVIMIENTO = 1,		/* arg: root motion relative (b5c) */
	EVENTO_ESCALA = 4,		/* arg: the float of the root motion scale */
	EVENTO_FRAME = 6,		/* arg set to g_FrameCount */
	EVENTO_PAUSA = 9,
	EVENTO_IR = 10,			/* arg: the float of the frame to go on from */
	EVENTO_SALTO_COND = 11,		/* arg: a jump destination whose arg points to the int condition */
	EVENTO_SALTO = 12,		/* arg: the destination (an EVENTO_DEST_SALTO event); not with bucle */
	EVENTO_FIN = 13
};

/* an entry of the sorted event list of a group (constructor rva 0x2bb8e0) */
typedef struct CAnimEvent {
	int id;				/* 0x00 EVENTO_* */
	int arg;			/* 0x04 */
	struct CAnimEvent *sig;		/* 0x08 */
	float t;			/* 0x0c the frame */
	uint32_t flags;			/* 0x10 1: in a list */
} CAnimEvent;
SD_SIZE(CAnimEvent, 0x14);

/* called at each event before its default handling, which a return of 1 skips */
typedef byte (*animCallback_t)(int a, int b, CAnimEvent *ev);

typedef struct CAnimCtrl {
	float vel;			/* 0x00 frames per second, 25 */
	float escalaMov;		/* 0x04 the scale of the root motion, 1 */
	float factor;			/* 0x08 of the time, 1 */
	float speed;			/* 0x0c 0.12 */
	byte bucle;			/* 0x10 EVENTO_SALTO ignored */
	byte pad_11[3];
	vec3_t move;			/* 0x14 the root motion of the last update */
	vec3_t ofs;			/* 0x20 the root offset */
	int saltos;			/* 0x2c the jumps of the last update */
	CAnimNodo *nodo;		/* 0x30 the root node of the animation */
	int primero;			/* 0x34 */
	int frames;			/* 0x38 */
	CAnimEvent **eventos;		/* 0x3c the list head */
	animCallback_t callback;	/* 0x40 */
	int cbA;			/* 0x44 */
	int cbB;			/* 0x48 */
	float paso;			/* 0x4c factor times the time of the last update */
	float frame;			/* 0x50 */
	int state;			/* 0x54 ANIM_* */
	int i58;			/* 0x58 */
	byte relativo;			/* 0x5c EVENTO_MOVIMIENTO: the y of the root motion goes into move, not ofs */
	byte pad_5d[3];
	float escala;			/* 0x60 EVENTO_ESCALA, 1 */
	float fadeVel;			/* 0x64 the fade per second */
	float fade;			/* 0x68 from 1 down to 0.2 */
	vec3_t lastPos;			/* 0x6c the root position at lastFrame */
	uint32_t time;			/* 0x78 g_TimeMs of the last update */
	float lastFrame;		/* 0x7c */
	CAnimEvent *evento;		/* 0x80 the next event */
} CAnimCtrl;
SD_SIZE(CAnimCtrl, 0x84);
SD_OFFSET(CAnimCtrl, frame, 0x50);
SD_OFFSET(CAnimCtrl, evento, 0x80);

CAnimCtrl *__fastcall CAnimCtrl_ctor(CAnimCtrl *self);
/* rva 0x250f10, empty (1 byte, not hooked) */
void __thiscall CAnimCtrl_dtor(CAnimCtrl *self);
void __fastcall CAnimCtrl_Play(CAnimCtrl *self);
void __thiscall CAnimCtrl_SetCallback(CAnimCtrl *self, animCallback_t fn, int a, int b);
/* stopped at frame, no event; 1 */
int __thiscall CAnimCtrl_Reset(CAnimCtrl *self, float frame);
void __thiscall CAnimCtrl_SetFrames(CAnimCtrl *self, int primero, int frames);
const char *AnimCtrl_StateName(int state);
/* playing from the first frame and the first event */
void __fastcall CAnimCtrl_Start(CAnimCtrl *self);
/* the root position and rotation at frame (0 and the identity without them) */
byte __thiscall CAnimCtrl_GetPosRot(CAnimCtrl *self, float frame, vec3_t *pos, quat_t *rot);
void __fastcall CAnimCtrl_Stop(CAnimCtrl *self);
void __fastcall CAnimCtrl_Pause(CAnimCtrl *self);
/* a fade out over t seconds, only while fading */
void __thiscall CAnimCtrl_Fade(CAnimCtrl *self, float t);
/* the events from *f1 up to *f2 (== *fin); 1 while there is more to go to *f2, which may be cut at an event */
byte __thiscall CAnimCtrl_Events(CAnimCtrl *self, float *f1, float *f2, float *fin);
/* t seconds on (t < 0: the time since the last update, at most 0.1), the root motion rotated by rot; 0 when stopped */
byte __thiscall CAnimCtrl_Update(CAnimCtrl *self, const vec3_t *rot, float t);


#endif
