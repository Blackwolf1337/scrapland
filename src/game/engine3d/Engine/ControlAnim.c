/*
 * Engine/ControlAnim.cpp of the engine3d: CAnimCtrl, see include/scrap/controlanim.h.
 */
#include <string.h>

#include "scrap/animgrupo.h"
#include "scrap/controlanim.h"
#include "scrap/e3dutil.h"

#define ANIMCTRL_DT_MAX 0.1f		/* the longest automatic time step */
#define ANIMCTRL_FADE_MIN 0.2f

CAnimCtrl *__fastcall CAnimCtrl_ctor(CAnimCtrl *self) {
	self->move = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->ofs = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->vel = 25.0f;
	self->escalaMov = 1.0f;
	self->factor = 1.0f;
	self->speed = 0.12f;
	self->bucle = 0;
	self->saltos = 0;
	self->callback = NULL;
	self->eventos = NULL;
	self->nodo = NULL;
	self->primero = 0;
	self->frames = 0;
	self->paso = 0.0f;
	self->frame = 0.0f;
	self->state = ANIM_FIN;
	self->i58 = 0;
	self->relativo = 0;
	self->escala = 1.0f;
	self->evento = NULL;
	return self;
}

void __fastcall CAnimCtrl_Play(CAnimCtrl *self) {
	self->state = ANIM_REPR;
}

void __thiscall CAnimCtrl_SetCallback(CAnimCtrl *self, animCallback_t fn, int a, int b) {
	self->callback = fn;
	self->cbA = a;
	self->cbB = b;
}

int __thiscall CAnimCtrl_Reset(CAnimCtrl *self, float frame) {
	self->move = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->ofs = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->saltos = 0;
	self->frame = frame;
	self->evento = NULL;
	self->state = ANIM_FIN;
	return 1;
}

void __thiscall CAnimCtrl_SetFrames(CAnimCtrl *self, int primero, int frames) {
	self->primero = primero;
	self->frames = frames;
}

const char *AnimCtrl_StateName(int state) {
	switch (state) {
	case ANIM_REPR:
		return Game_Ptr(0x3d9f28);	/* "PLAY" */
	case ANIM_PAUSA:
		return Game_Ptr(0x3bc898);	/* "PAUSE" */
	case ANIM_FUND_PAUSA:
		return Game_Ptr(0x3d9f30);	/* "FADEOUT" */
	case ANIM_FIN:
		return Game_Ptr(0x3bc7ec);	/* "STOP" */
	}
	CHECK(0);
	return NULL;
}

void __fastcall CAnimCtrl_Start(CAnimCtrl *self) {
	self->move = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->ofs = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->lastPos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->saltos = 0;
	self->lastFrame = (float)self->primero;
	self->time = g_TimeMs;
	self->paso = 0.0f;
	self->frame = (float)self->primero;
	self->relativo = 0;
	self->escala = 1.0f;
	self->i58 = 0;
	self->evento = self->eventos ? *self->eventos : NULL;
	self->state = ANIM_REPR;
}

byte __thiscall CAnimCtrl_GetPosRot(CAnimCtrl *self, float frame, vec3_t *pos, quat_t *rot) {
	CHECK(pos && rot);
	*pos = (vec3_t){ 0.0f, 0.0f, 0.0f };
	*rot = (quat_t){ 0.0f, 0.0f, 0.0f, 1.0f };
	if (!self->nodo) {
		return 0;
	}
	int i = (int)frame;
	return CAnimNodo_GetPosRot(self->nodo, pos, rot, i, frame - (float)i);
}

void __fastcall CAnimCtrl_Stop(CAnimCtrl *self) {
	self->state = ANIM_FIN;
}

void __fastcall CAnimCtrl_Pause(CAnimCtrl *self) {
	self->state = ANIM_PAUSA;
}

void __thiscall CAnimCtrl_Fade(CAnimCtrl *self, float t) {
	if (self->state == ANIM_FUND_PAUSA) {
		self->state = ANIM_FUND_PAUSA;
		self->fade = 1.0f;
		self->fadeVel = (float)(1000.0 / (double)t);
	}
}

/* the frame where to go on after a jump to f: *f1 = f, *f2 = *fin = f plus what was left */
static void AnimCtrl_Jump(float *f1p, float *f2p, float *finp, float *f1, float *f2, float f) {
	*f2 = (*f2 - *f1) + f;
	*f1p = f;
	*f2p = *f2;
	*finp = *f2;
	*f1 = f;
}

byte __thiscall CAnimCtrl_Events(CAnimCtrl *self, float *f1p, float *f2p, float *finp) {
	CHECK(*f2p == *finp);
	float f2 = *f2p;
	float f1 = *f1p;
	CHECK(self->evento);
	float t = self->evento->t;
	CHECK(t >= f1);
	if (f1 == t) {
		do {
			if (!self->callback || self->callback(self->cbA, self->cbB, self->evento) != 1) {
				CAnimEvent *ev = self->evento;
				float f;
				switch (ev->id) {
				case EVENTO_DEST_SALTO:
					break;
				case EVENTO_MOVIMIENTO:
					self->relativo = ev->arg != 0;
					break;
				case EVENTO_ESCALA:
					memcpy(&self->escala, &ev->arg, 4);
					break;
				case EVENTO_FRAME:
					ev->arg = g_FrameCount;
					break;
				case EVENTO_PAUSA:
					*f2p = f1;
					*finp = f1;
					self->state = ANIM_PAUSA;
					self->evento = self->evento->sig;
					return 0;
				case EVENTO_IR:
					memcpy(&f, &ev->arg, 4);
					AnimCtrl_Jump(f1p, f2p, finp, &f1, &f2, f);
					break;
				case EVENTO_SALTO_COND: {
					CAnimEvent *dest = (CAnimEvent *)ev->arg;
					CHECK(dest && dest->id == EVENTO_DEST_SALTO);
					int *cond = (int *)dest->arg;
					CHECK(cond);
					if (!*cond) {
						break;
					}
				}
				/* fall through */
				case EVENTO_SALTO:
					if (!self->bucle) {
						self->saltos++;
						self->evento = (CAnimEvent *)self->evento->arg;
						CHECK(self->evento && self->evento->id == EVENTO_DEST_SALTO);
						AnimCtrl_Jump(f1p, f2p, finp, &f1, &f2, self->evento->t);
					}
					break;
				case EVENTO_FIN:
					*f2p = f1;
					*finp = f1;
					self->state = ANIM_FIN;
					return 0;
				default:
					CHECK(0);
				}
			}
			self->evento = self->evento->sig;
			CHECK(self->evento);
			t = self->evento->t;
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

byte __thiscall CAnimCtrl_Update(CAnimCtrl *self, const vec3_t *rot, float t) {
	CHECK(self->evento);
	self->move = (vec3_t){ 0.0f, 0.0f, 0.0f };
	self->saltos = 0;
	if (!self->evento || self->state == ANIM_FIN) {
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
	if (state == ANIM_FUND_PAUSA) {
		float fade = self->fade - self->fadeVel * t;
		fade = fade > ANIMCTRL_FADE_MIN ? fade : ANIMCTRL_FADE_MIN;
		self->fade = fade;
		t = t * fade;
	}
	float paso = self->factor * t;
	if (state == ANIM_REPR) {
		delta = self->vel * paso;
	}
	self->paso = paso;
	float f1 = self->frame;
	float fin = f1 + delta;
	float f2;
	byte first = 1;
	for (;;) {
		f2 = fin;
		if (!CAnimCtrl_Events(self, &f1, &f2, &fin) && !first) {
			break;
		}
		first = 0;
		CHECK(f2 >= f1);
		if (self->nodo) {
			quat_t q;
			if (f1 != self->lastFrame) {
				int i = (int)f1;
				if (!CAnimNodo_GetPosRot(self->nodo, &self->lastPos, &q, i, f1 - (float)i)) {
					self->lastPos = (vec3_t){ 0.0f, 0.0f, 0.0f };
				}
			}
			vec3_t cur;
			int i = (int)f2;
			if (!CAnimNodo_GetPosRot(self->nodo, &cur, &q, i, f2 - (float)i)) {
				cur = (vec3_t){ 0.0f, 0.0f, 0.0f };
			}
			float k = self->escala * self->escalaMov;
			float dx = cur.x - self->lastPos.x, dy = cur.y - self->lastPos.y, dz = cur.z - self->lastPos.z;
			self->lastPos = cur;
			self->lastFrame = f2;
			dx = k * dx;
			dy = dy * k;
			dz = k * dz;
			if (self->relativo) {
				self->move.x = dx + self->move.x;
				self->move.y = self->move.y + dy;
				self->move.z = self->move.z + dz;
				self->ofs = (vec3_t){ 0.0f, 0.0f, 0.0f };
			} else {
				self->move.x = self->move.x + dx;
				self->move.z = self->move.z + dz;
				self->ofs = (vec3_t){ 0.0f, self->escalaMov * cur.y, 0.0f };
			}
		}
		f1 = f2;
	}
	self->frame = fin;
	if (rot && (rot->x != 0.0f || rot->y != 0.0f || rot->z != 0.0f)) {
		vec3_t tmp;
		self->move = *Vec3_Rotate(&tmp, &self->move, rot);
		self->ofs = *Vec3_Rotate(&tmp, &self->ofs, rot);
	}
	return 1;
}
