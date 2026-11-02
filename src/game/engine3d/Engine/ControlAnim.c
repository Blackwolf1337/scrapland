/*
 * Engine/ControlAnim.cpp of the engine3d: CAnimCtrl, see include/scrap/controlanim.h.
 */
#include <string.h>

#include "scrap/animgroup.h"
#include "scrap/controlanim.h"
#include "scrap/e3dutil.h"

#define ANIMCTRL_DT_MAX 0.1f		/* the longest automatic time step */
#define ANIMCTRL_FADE_MIN 0.2f

CAnimCtrl *CAnimCtrl_ctor(CAnimCtrl *self) {
	self->move = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->ofs = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->vel = 25.0f;
	self->scaleMove = 1.0f;
	self->factor = 1.0f;
	self->speed = 0.12f;
	self->loop = 0;
	self->jumps = 0;
	self->callback = NULL;
	self->events = NULL;
	self->node = NULL;
	self->firstFrame = 0;
	self->frames = 0;
	self->step = 0.0f;
	self->frame = 0.0f;
	self->state = ANIM_END;
	self->unused_58 = 0;
	self->relative = 0;
	self->scale = 1.0f;
	self->event = NULL;
	return self;
}

void CAnimCtrl_dtor(CAnimCtrl *self) {
	(void)self;
}

void CAnimCtrl_Play(CAnimCtrl *self) {
	self->state = ANIM_REPR;
}

int CAnimCtrl_Reset(CAnimCtrl *self, float frame) {
	self->move = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->ofs = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->jumps = 0;
	self->frame = frame;
	self->event = NULL;
	self->state = ANIM_END;
	return 1;
}

void CAnimCtrl_SetFrames(CAnimCtrl *self, int firstFrame, int frames) {
	self->firstFrame = firstFrame;
	self->frames = frames;
}

void CAnimCtrl_Start(CAnimCtrl *self) {
	self->move = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->ofs = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->lastPos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->jumps = 0;
	self->lastFrame = (float)self->firstFrame;
	self->time = g_TimeMs;
	self->step = 0.0f;
	self->frame = (float)self->firstFrame;
	self->relative = 0;
	self->scale = 1.0f;
	self->unused_58 = 0;
	self->event = self->events ? *self->events : NULL;
	self->state = ANIM_REPR;
}

byte CAnimCtrl_GetPosRot(CAnimCtrl *self, float frame, vec3_t *pos, quat_t *rot) {
	CHECK(pos && rot);
	*pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	*rot = (quat_t){ 0.0f, 0.0f, 0.0f, 1.0f };
	if (!self->node) {
		return 0;
	}
	int i = (int)frame;
	return CAnimNode_GetPosRot(self->node, pos, rot, i, frame - (float)i);
}

void CAnimCtrl_Stop(CAnimCtrl *self) {
	self->state = ANIM_END;
}

void CAnimCtrl_Pause(CAnimCtrl *self) {
	self->state = ANIM_PAUSE;
}

void CAnimCtrl_Fade(CAnimCtrl *self, float t) {
	if (self->state == ANIM_FADE_PAUSE) {
		self->state = ANIM_FADE_PAUSE;
		self->fade = 1.0f;
		self->fadeVel = (float)(1000.0 / (double)t);
	}
}

/* the frame where to go on after a jump to f: *f1 = f, *f2 = *fin = f plus what was left */
static void AnimCtrl_Jump(float *f1p, float *f2p, float *endp, float *f1, float *f2, float f) {
	*f2 = (*f2 - *f1) + f;
	*f1p = f;
	*f2p = *f2;
	*endp = *f2;
	*f1 = f;
}

/* the events from *f1 up to *f2 (== *fin); 1 while there is more to go to *f2, which may be cut at an event */
static byte CAnimCtrl_Events(CAnimCtrl *self, float *f1p, float *f2p, float *endp) {
	CHECK(*f2p == *endp);
	float f2 = *f2p;
	float f1 = *f1p;
	CHECK(self->event);
	float t = self->event->t;
	CHECK(t >= f1);
	if (f1 == t) {
		do {
			if (!self->callback || self->callback(self->cbA, self->cbB, self->event) != 1) {
				CAnimEvent *ev = self->event;
				float f;
				switch (ev->id) {
				case EVENT_DEST_JUMP:
					break;
				case EVENT_MOVEMENT:
					self->relative = ev->arg != 0;
					break;
				case EVENT_SCALE:
					memcpy(&self->scale, &ev->arg, 4);
					break;
				case EVENT_FRAME:
					ev->arg = g_FrameCount;
					break;
				case EVENT_PAUSE:
					*f2p = f1;
					*endp = f1;
					self->state = ANIM_PAUSE;
					self->event = self->event->next;
					return 0;
				case EVENT_GO:
					memcpy(&f, &ev->arg, 4);
					AnimCtrl_Jump(f1p, f2p, endp, &f1, &f2, f);
					break;
				case EVENT_JUMP_COND: {
					CAnimEvent *dest = (CAnimEvent *)ev->arg;
					CHECK(dest && dest->id == EVENT_DEST_JUMP);
					int *cond = (int *)dest->arg;
					CHECK(cond);
					if (!*cond) {
						break;
					}
				}
				/* fall through */
				case EVENT_JUMP:
					if (!self->loop) {
						self->jumps++;
						self->event = (CAnimEvent *)self->event->arg;
						CHECK(self->event && self->event->id == EVENT_DEST_JUMP);
						AnimCtrl_Jump(f1p, f2p, endp, &f1, &f2, self->event->t);
					}
					break;
				case EVENT_END:
					*f2p = f1;
					*endp = f1;
					self->state = ANIM_END;
					return 0;
				default:
					CHECK(0);
				}
			}
			self->event = self->event->next;
			CHECK(self->event);
			t = self->event->t;
		} while (f1 == t);
	}
	if (f1 == f2) {
		return 0;
	}
	if (f2 > t) {
		*f2p = t;
	}
	return 1;
}

byte CAnimCtrl_Update(CAnimCtrl *self, const vec3_t *rot, float t) {
	CHECK(self->event);
	self->move = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->jumps = 0;
	if (!self->event || self->state == ANIM_END) {
		return 0;
	}
	int state = self->state;
	float delta = 0.0f;
	self->ofs = (vec3_t){ 0.0f, 0.0f, 0.0f };
	if (0.0f > t) {
		uint32_t now = g_TimeMs;
		uint32_t ms = now - self->time;
		self->time = now;
		float dt = (float)(double)ms * 0.001f;
		t = ANIMCTRL_DT_MAX < dt ? ANIMCTRL_DT_MAX : dt;
	}
	if (state == ANIM_FADE_PAUSE) {
		float fade = self->fade - self->fadeVel * t;
		fade = fade > ANIMCTRL_FADE_MIN ? fade : ANIMCTRL_FADE_MIN;
		self->fade = fade;
		t = t * fade;
	}
	float step = self->factor * t;
	if (state == ANIM_REPR) {
		delta = self->vel * step;
	}
	self->step = step;
	float f1 = self->frame;
	float end = f1 + delta;
	float f2;
	byte first = 1;
	for (;;) {
		f2 = end;
		if (!CAnimCtrl_Events(self, &f1, &f2, &end) && !first) {
			break;
		}
		first = 0;
		CHECK(f2 >= f1);
		if (self->node) {
			quat_t q;
			if (f1 != self->lastFrame) {
				int i = (int)f1;
				if (!CAnimNode_GetPosRot(self->node, &self->lastPos, &q, i, f1 - (float)i)) {
					self->lastPos = (vec3_t){ 0.0f, 0.0f, 0.0f };
				}
			}
			vec3_t cur;
			int i = (int)f2;
			if (!CAnimNode_GetPosRot(self->node, &cur, &q, i, f2 - (float)i)) {
				cur = (vec3_t){ 0.0f, 0.0f, 0.0f };
			}
			float k = self->scale * self->scaleMove;
			float dx = cur.x - self->lastPos.x, dy = cur.y - self->lastPos.y, dz = cur.z - self->lastPos.z;
			self->lastPos = cur;
			self->lastFrame = f2;
			dx = k * dx;
			dy = dy * k;
			dz = k * dz;
			if (self->relative) {
				self->move.x = dx + self->move.x;
				self->move.y = self->move.y + dy;
				self->move.z = self->move.z + dz;
				self->ofs = (vec3_t){ 0.0f, 0.0f, 0.0f };
			} else {
				self->move.x = self->move.x + dx;
				self->move.z = self->move.z + dz;
				self->ofs = (vec3_t){ 0.0f, self->scaleMove * cur.y, 0.0f };
			}
		}
		f1 = f2;
	}
	self->frame = end;
	if (rot && (rot->x != 0.0f || rot->y != 0.0f || rot->z != 0.0f)) {
		vec3_t tmp;
		self->move = *Vec3_Rotate(&tmp, &self->move, rot);
		self->ofs = *Vec3_Rotate(&tmp, &self->ofs, rot);
	}
	return 1;
}
