/*
 * NetRep: byte writer and reader, quantisation, the fields of a record and the small messages (include/scrap/netrep.h).
 */
#include <math.h>
#include <string.h>

#include "scrap/netrep.h"

void NetRepBuf_Init(NetRepBuf *w, uint8_t *p, int cap) {
	w->p = p;
	w->size = 0;
	w->cap = cap;
	w->bad = 0;
}

void NetRep_PutU8(NetRepBuf *w, uint32_t v) {
	if (w->size >= w->cap) {
		w->bad = 1;
		return;
	}
	w->p[w->size++] = (uint8_t)v;
}

void NetRep_PutU16(NetRepBuf *w, uint32_t v) {
	NetRep_PutU8(w, v);
	NetRep_PutU8(w, v >> 8);
}

void NetRep_PutU32(NetRepBuf *w, uint32_t v) {
	NetRep_PutU16(w, v);
	NetRep_PutU16(w, v >> 16);
}

void NetRep_PutVar(NetRepBuf *w, uint32_t v) {
	while (v >= 0x80) {
		NetRep_PutU8(w, (v & 0x7f) | 0x80);
		v >>= 7;
	}
	NetRep_PutU8(w, v);
}

void NetRep_PutSVar(NetRepBuf *w, int32_t v) {
	NetRep_PutVar(w, ((uint32_t)v << 1) ^ (uint32_t)(v >> 31));
}

void NetRep_PutStr(NetRepBuf *w, const char *s) {
	size_t n = strlen(s);
	if (n > 255) {
		n = 255;
	}
	NetRep_PutU8(w, (uint32_t)n);
	for (size_t i = 0; i < n; i++) {
		NetRep_PutU8(w, (uint8_t)s[i]);
	}
}

void NetRepReader_Init(NetRepReader *r, const uint8_t *p, int size) {
	r->p = p;
	r->size = size;
	r->pos = 0;
	r->bad = 0;
}

uint32_t NetRep_GetU8(NetRepReader *r) {
	if (r->pos >= r->size) {
		r->bad = 1;
		return 0;
	}
	return r->p[r->pos++];
}

uint32_t NetRep_GetU16(NetRepReader *r) {
	uint32_t v = NetRep_GetU8(r);
	return v | NetRep_GetU8(r) << 8;
}

uint32_t NetRep_GetU32(NetRepReader *r) {
	uint32_t v = NetRep_GetU16(r);
	return v | NetRep_GetU16(r) << 16;
}

uint32_t NetRep_GetVar(NetRepReader *r) {
	uint32_t v = 0;
	for (int shift = 0; shift < 35; shift += 7) {
		uint32_t b = NetRep_GetU8(r);
		v |= (b & 0x7f) << shift;
		if (!(b & 0x80)) {
			return v;
		}
	}
	r->bad = 1;
	return 0;
}

int32_t NetRep_GetSVar(NetRepReader *r) {
	uint32_t v = NetRep_GetVar(r);
	return (int32_t)(v >> 1) ^ -(int32_t)(v & 1);
}

void NetRep_GetStr(NetRepReader *r, char *out, int cap) {
	uint32_t n = NetRep_GetU8(r);
	int k = 0;
	for (uint32_t i = 0; i < n; i++) {
		uint32_t c = NetRep_GetU8(r);
		if (k < cap - 1) {
			out[k++] = (char)c;
		}
	}
	if (cap > 0) {
		out[k] = 0;
	}
}

/* ---- quantisation ---- */

static int32_t NetRep_Round(float v, float lo, float hi) {
	if (!(v >= lo)) {
		v = lo;		/* NaN too */
	} else if (v > hi) {
		v = hi;
	}
	return (int32_t)lrintf(v);
}

int32_t NetRep_QPos(float v) {
	return NetRep_Round(v * NETREP_POS_SCALE, -2.0e9f, 2.0e9f);
}

float NetRep_UPos(int32_t q) {
	return (float)q / NETREP_POS_SCALE;
}

uint16_t NetRep_QAng(float rad) {
	return (uint16_t)NetRep_Round(fmodf(rad, 6.2831853f) * NETREP_ANG_SCALE, -65536.0f, 65536.0f);
}

float NetRep_UAng(uint16_t q) {
	return (float)(int16_t)q / NETREP_ANG_SCALE;
}

int32_t NetRep_QVel(float v) {
	return NetRep_Round(v, -1.0e8f, 1.0e8f);
}

int8_t NetRep_QCtrl(float v) {
	return (int8_t)NetRep_Round(v * 127.0f, -127.0f, 127.0f);
}

/* ---- the fields ---- */

static int32_t Rep_Sub(int32_t a, int32_t b) {
	return (int32_t)((uint32_t)a - (uint32_t)b);
}

static int32_t Rep_Add(int32_t a, int32_t b) {
	return (int32_t)((uint32_t)a + (uint32_t)b);
}

uint32_t NetRep_Diff(const NetRepState *a, const NetRepState *b) {
	uint32_t m = 0;
	if (memcmp(a->pos, b->pos, sizeof(a->pos))) {
		m |= NETREP_F_POS;
	}
	if (memcmp(a->ang, b->ang, sizeof(a->ang))) {
		m |= NETREP_F_ANG;
	}
	if (memcmp(a->vel, b->vel, sizeof(a->vel))) {
		m |= NETREP_F_VEL;
	}
	if (a->dt != b->dt) {
		m |= NETREP_F_DT;
	}
	if (a->life != b->life) {
		m |= NETREP_F_LIFE;
	}
	if (a->flags != b->flags) {
		m |= NETREP_F_FLAGS;
	}
	if (a->ctrl[0] != b->ctrl[0] || a->ctrl[1] != b->ctrl[1]) {
		m |= NETREP_F_CTRL;
	}
	for (int l = 0; l < NETREP_LAYERS; l++) {
		if (a->act[l] != b->act[l] || a->actTime[l] != b->actTime[l]) {
			m |= NETREP_F_ACT0 << l;
		}
	}
	if (a->view[0] != b->view[0] || a->view[1] != b->view[1]) {
		m |= NETREP_F_VIEW;
	}
	return m;
}

void NetRep_WriteFields(NetRepBuf *w, const NetRepState *base, const NetRepState *s, uint32_t mask) {
	if (mask & NETREP_F_POS) {
		for (int i = 0; i < 3; i++) {
			NetRep_PutSVar(w, Rep_Sub(s->pos[i], base->pos[i]));
		}
	}
	if (mask & NETREP_F_ANG) {
		for (int i = 0; i < 3; i++) {
			NetRep_PutSVar(w, (int16_t)(uint16_t)(s->ang[i] - base->ang[i]));
		}
	}
	if (mask & NETREP_F_VEL) {
		for (int i = 0; i < 3; i++) {
			NetRep_PutSVar(w, Rep_Sub(s->vel[i], base->vel[i]));
		}
	}
	if (mask & NETREP_F_DT) {
		NetRep_PutVar(w, s->dt);
	}
	if (mask & NETREP_F_LIFE) {
		NetRep_PutSVar(w, (int32_t)s->life - base->life);
	}
	if (mask & NETREP_F_FLAGS) {
		NetRep_PutU8(w, s->flags);
	}
	if (mask & NETREP_F_CTRL) {
		NetRep_PutU8(w, (uint8_t)s->ctrl[0]);
		NetRep_PutU8(w, (uint8_t)s->ctrl[1]);
	}
	for (int l = 0; l < NETREP_LAYERS; l++) {
		if (mask & (NETREP_F_ACT0 << l)) {
			NetRep_PutVar(w, s->act[l]);
			NetRep_PutVar(w, s->actTime[l]);
		}
	}
	if (mask & NETREP_F_VIEW) {
		for (int i = 0; i < 2; i++) {
			NetRep_PutSVar(w, (int16_t)(uint16_t)(s->view[i] - base->view[i]));
		}
	}
}

int NetRep_ReadFields(NetRepReader *r, const NetRepState *base, NetRepState *out, uint32_t mask) {
	NetRepState s = *base;

	if (mask & ~(uint32_t)NETREP_F_ALL) {
		return 0;
	}
	if (mask & NETREP_F_POS) {
		for (int i = 0; i < 3; i++) {
			s.pos[i] = Rep_Add(base->pos[i], NetRep_GetSVar(r));
		}
	}
	if (mask & NETREP_F_ANG) {
		for (int i = 0; i < 3; i++) {
			s.ang[i] = (uint16_t)(base->ang[i] + (uint16_t)NetRep_GetSVar(r));
		}
	}
	if (mask & NETREP_F_VEL) {
		for (int i = 0; i < 3; i++) {
			s.vel[i] = Rep_Add(base->vel[i], NetRep_GetSVar(r));
		}
	}
	if (mask & NETREP_F_DT) {
		uint32_t dt = NetRep_GetVar(r);
		if (dt > 0xffff) {
			return 0;
		}
		s.dt = (uint16_t)dt;
	}
	if (mask & NETREP_F_LIFE) {
		s.life = (int16_t)(uint16_t)((uint32_t)base->life + (uint32_t)NetRep_GetSVar(r));
	}
	if (mask & NETREP_F_FLAGS) {
		s.flags = (uint8_t)NetRep_GetU8(r);
	}
	if (mask & NETREP_F_CTRL) {
		s.ctrl[0] = (int8_t)NetRep_GetU8(r);
		s.ctrl[1] = (int8_t)NetRep_GetU8(r);
	}
	for (int l = 0; l < NETREP_LAYERS; l++) {
		if (mask & (NETREP_F_ACT0 << l)) {
			uint32_t a = NetRep_GetVar(r);
			if (a > 0xffff) {
				return 0;
			}
			s.act[l] = (uint16_t)a;
			s.actTime[l] = NetRep_GetVar(r);
		}
	}
	if (mask & NETREP_F_VIEW) {
		for (int i = 0; i < 2; i++) {
			s.view[i] = (uint16_t)(base->view[i] + (uint16_t)NetRep_GetSVar(r));
		}
	}
	if (r->bad) {
		return 0;
	}
	*out = s;
	return 1;
}

/* ---- the small messages ---- */

static void Rep_PutView(NetRepBuf *w, const float *view) {
	for (int i = 0; i < 3; i++) {
		NetRep_PutU32(w, (uint32_t)NetRep_Round(view[i], -2.0e9f, 2.0e9f));
	}
}

static void Rep_GetView(NetRepReader *r, float *view) {
	for (int i = 0; i < 3; i++) {
		view[i] = (float)(int32_t)NetRep_GetU32(r);
	}
}

int NetRep_WriteHello(uint8_t *buf, int cap, uint32_t rate) {
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, cap);
	NetRep_PutU8(&w, NETREP_MSG_HELLO);
	NetRep_PutU16(&w, NETREP_VERSION);
	NetRep_PutU32(&w, rate);
	return w.bad ? 0 : w.size;
}

int NetRep_ReadHello(const uint8_t *data, int size, uint32_t *rate) {
	NetRepReader r;
	NetRepReader_Init(&r, data, size);
	if (NetRep_GetU8(&r) != NETREP_MSG_HELLO || NetRep_GetU16(&r) != NETREP_VERSION) {
		return 0;
	}
	*rate = NetRep_GetU32(&r);
	return !r.bad;
}

int NetRep_WriteWelcome(uint8_t *buf, int cap, uint32_t tick, const float *view, const char *level) {
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, cap);
	NetRep_PutU8(&w, NETREP_MSG_WELCOME);
	NetRep_PutU16(&w, NETREP_VERSION);
	NetRep_PutU8(&w, NETREP_TICK_HZ);
	NetRep_PutU8(&w, NETREP_SNAP_HZ);
	NetRep_PutU32(&w, tick);
	Rep_PutView(&w, view);
	NetRep_PutStr(&w, level);
	return w.bad ? 0 : w.size;
}

int NetRep_ReadWelcome(const uint8_t *data, int size, uint32_t *tick, float *view, char *level, int cap) {
	NetRepReader r;
	NetRepReader_Init(&r, data, size);
	if (NetRep_GetU8(&r) != NETREP_MSG_WELCOME || NetRep_GetU16(&r) != NETREP_VERSION ||
		NetRep_GetU8(&r) != NETREP_TICK_HZ || NetRep_GetU8(&r) != NETREP_SNAP_HZ) {
		return 0;
	}
	*tick = NetRep_GetU32(&r);
	Rep_GetView(&r, view);
	NetRep_GetStr(&r, level, cap);
	return !r.bad;
}

int NetRep_WriteAck(uint8_t *buf, int cap, uint16_t newest, uint32_t bits, const float *view) {
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, cap);
	NetRep_PutU8(&w, NETREP_MSG_ACK);
	NetRep_PutU16(&w, newest);
	NetRep_PutU32(&w, bits);
	Rep_PutView(&w, view);
	return w.bad ? 0 : w.size;
}

int NetRep_ReadAck(const uint8_t *data, int size, uint16_t *newest, uint32_t *bits, float *view) {
	NetRepReader r;
	NetRepReader_Init(&r, data, size);
	if (NetRep_GetU8(&r) != NETREP_MSG_ACK) {
		return 0;
	}
	*newest = (uint16_t)NetRep_GetU16(&r);
	*bits = NetRep_GetU32(&r);
	Rep_GetView(&r, view);
	return !r.bad;
}
