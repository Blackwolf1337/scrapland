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
	if (a->weapon != b->weapon) {
		m |= NETREP_F_WEAP;
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
	if (mask & NETREP_F_WEAP) {
		NetRep_PutU8(w, s->weapon);
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
	if (mask & NETREP_F_WEAP) {
		s.weapon = (uint8_t)NetRep_GetU8(r);
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

int NetRep_WriteHello(uint8_t *buf, int cap, uint32_t rate, int flags) {
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, cap);
	NetRep_PutU8(&w, NETREP_MSG_HELLO);
	NetRep_PutU16(&w, NETREP_VERSION);
	NetRep_PutU32(&w, rate);
	NetRep_PutU8(&w, (uint32_t)flags);
	return w.bad ? 0 : w.size;
}

int NetRep_ReadHello(const uint8_t *data, int size, uint32_t *rate, int *flags) {
	NetRepReader r;
	NetRepReader_Init(&r, data, size);
	if (NetRep_GetU8(&r) != NETREP_MSG_HELLO || NetRep_GetU16(&r) != NETREP_VERSION) {
		return 0;
	}
	*rate = NetRep_GetU32(&r);
	*flags = (int)NetRep_GetU8(&r);
	return !r.bad;
}

int NetRep_WritePlayer(uint8_t *buf, int cap, int player, int slot, int kind, uint32_t tick, const float *phys,
	int nphys) {
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, cap);
	nphys = nphys < 0 ? 0 : nphys > NETREP_PHYS_MAX ? NETREP_PHYS_MAX : nphys;
	NetRep_PutU8(&w, NETREP_MSG_PLAYER);
	NetRep_PutU8(&w, (uint32_t)player);
	NetRep_PutU16(&w, (uint32_t)slot);
	NetRep_PutU8(&w, (uint32_t)kind);
	NetRep_PutU32(&w, tick);
	NetRep_PutU8(&w, (uint32_t)nphys);
	for (int i = 0; i < nphys; i++) {
		uint32_t bits;
		memcpy(&bits, &phys[i], sizeof(bits));
		NetRep_PutU32(&w, bits);
	}
	return w.bad ? 0 : w.size;
}

int NetRep_ReadPlayer(const uint8_t *data, int size, int *player, int *slot, int *kind, uint32_t *tick, float *phys,
	int *nphys) {
	NetRepReader r;
	NetRepReader_Init(&r, data, size);
	if (NetRep_GetU8(&r) != NETREP_MSG_PLAYER) {
		return 0;
	}
	*player = (int)NetRep_GetU8(&r);
	*slot = (int)NetRep_GetU16(&r);
	*kind = (int)NetRep_GetU8(&r);
	*tick = NetRep_GetU32(&r);
	int n = (int)NetRep_GetU8(&r);
	if (n > NETREP_PHYS_MAX) {
		return 0;
	}
	for (int i = 0; i < n; i++) {
		uint32_t bits = NetRep_GetU32(&r);
		memcpy(&phys[i], &bits, sizeof(bits));
	}
	*nphys = n;
	return !r.bad;
}

int NetRep_WriteStatus(uint8_t *buf, int cap, const NetRepStatus *st) {
	NetRepBuf w;
	int n = st->nammo > NETREP_AMMO_MAX ? NETREP_AMMO_MAX : st->nammo;
	NetRepBuf_Init(&w, buf, cap);
	NetRep_PutU8(&w, NETREP_MSG_STATUS);
	NetRep_PutU32(&w, st->tick);
	NetRep_PutSVar(&w, st->money);
	NetRep_PutU8(&w, st->alarm);
	NetRep_PutU8(&w, st->flags);
	NetRep_PutU8(&w, (uint32_t)n);
	for (int i = 0; i < n; i++) {
		NetRep_PutU16(&w, st->ammo[i]);
	}
	return w.bad ? 0 : w.size;
}

int NetRep_ReadStatus(const uint8_t *data, int size, NetRepStatus *st) {
	NetRepReader r;
	NetRepReader_Init(&r, data, size);
	memset(st, 0, sizeof(*st));
	if (NetRep_GetU8(&r) != NETREP_MSG_STATUS) {
		return 0;
	}
	st->tick = NetRep_GetU32(&r);
	st->money = NetRep_GetSVar(&r);
	st->alarm = (uint8_t)NetRep_GetU8(&r);
	st->flags = (uint8_t)NetRep_GetU8(&r);
	uint32_t n = NetRep_GetU8(&r);
	if (n > NETREP_AMMO_MAX) {
		return 0;
	}
	st->nammo = (uint8_t)n;
	for (uint32_t i = 0; i < n; i++) {
		st->ammo[i] = (uint16_t)NetRep_GetU16(&r);
	}
	return !r.bad && r.pos == size;
}

int NetRep_StatusDiffers(const NetRepStatus *a, const NetRepStatus *b) {
	if (a->money != b->money || a->alarm != b->alarm || a->flags != b->flags || a->nammo != b->nammo) {
		return 1;
	}
	return memcmp(a->ammo, b->ammo, a->nammo * sizeof(a->ammo[0])) != 0;
}

int NetRep_WriteShots(uint8_t *buf, int cap, uint32_t tick, const NetRepShot *shots, int n) {
	NetRepBuf w;
	if (n < 0 || n > NETREP_SHOTS_MAX) {
		return 0;
	}
	NetRepBuf_Init(&w, buf, cap);
	NetRep_PutU8(&w, NETREP_MSG_SHOTS);
	NetRep_PutU32(&w, tick);
	NetRep_PutU8(&w, (uint32_t)n);
	for (int i = 0; i < n; i++) {
		const NetRepShot *x = &shots[i];
		NetRep_PutVar(&w, x->slot);
		NetRep_PutU8(&w, x->weapon);
		NetRep_PutU8(&w, x->count);
		NetRep_PutU16(&w, x->target);
		for (int k = 0; k < 3; k++) {
			NetRep_PutSVar(&w, x->pos[k]);
		}
		for (int k = 0; k < 3; k++) {
			NetRep_PutSVar(&w, Rep_Sub(x->from[k], x->pos[k]));
		}
	}
	return w.bad ? 0 : w.size;
}

int NetRep_ReadShots(const uint8_t *data, int size, uint32_t *tick, NetRepShot *shots, int max, int *n) {
	NetRepReader r;
	NetRepReader_Init(&r, data, size);
	if (NetRep_GetU8(&r) != NETREP_MSG_SHOTS) {
		return 0;
	}
	*tick = NetRep_GetU32(&r);
	int count = (int)NetRep_GetU8(&r);
	if (count > max || count > NETREP_SHOTS_MAX) {
		return 0;
	}
	for (int i = 0; i < count; i++) {
		NetRepShot *x = &shots[i];
		uint32_t slot = NetRep_GetVar(&r);
		if (slot >= NETREP_MAX_ENTS) {
			return 0;
		}
		x->slot = (uint16_t)slot;
		x->weapon = (uint8_t)NetRep_GetU8(&r);
		x->count = (uint8_t)NetRep_GetU8(&r);
		x->target = (uint16_t)NetRep_GetU16(&r);
		for (int k = 0; k < 3; k++) {
			x->pos[k] = NetRep_GetSVar(&r);
		}
		for (int k = 0; k < 3; k++) {
			x->from[k] = Rep_Add(x->pos[k], NetRep_GetSVar(&r));
		}
	}
	*n = count;
	return !r.bad && r.pos == size;
}

int NetRep_WriteWelcome(uint8_t *buf, int cap, uint32_t tick, const float *view, const char *level, int flags) {
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, cap);
	NetRep_PutU8(&w, NETREP_MSG_WELCOME);
	NetRep_PutU16(&w, NETREP_VERSION);
	NetRep_PutU8(&w, NETREP_TICK_HZ);
	NetRep_PutU8(&w, NETREP_SNAP_HZ);
	NetRep_PutU32(&w, tick);
	Rep_PutView(&w, view);
	NetRep_PutStr(&w, level);
	NetRep_PutU8(&w, (uint8_t)flags);
	return w.bad ? 0 : w.size;
}

int NetRep_ReadWelcome(const uint8_t *data, int size, uint32_t *tick, float *view, char *level, int cap, int *flags) {
	NetRepReader r;
	NetRepReader_Init(&r, data, size);
	if (NetRep_GetU8(&r) != NETREP_MSG_WELCOME || NetRep_GetU16(&r) != NETREP_VERSION ||
		NetRep_GetU8(&r) != NETREP_TICK_HZ || NetRep_GetU8(&r) != NETREP_SNAP_HZ) {
		return 0;
	}
	*tick = NetRep_GetU32(&r);
	Rep_GetView(&r, view);
	NetRep_GetStr(&r, level, cap);
	*flags = NetRep_GetU8(&r);
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
	int n;
	uint32_t shown;
	return NetRep_ReadAckInput(data, size, newest, bits, view, NULL, 0, &n, &shown);
}

int8_t NetRep_QMove(float v) {
	float q = v * NETREP_MOVE_SCALE;
	q = q > 127.0f ? 127.0f : q < -127.0f ? -127.0f : q;
	return (int8_t)lrintf(q);
}

int16_t NetRep_QRot(float v) {
	float q = v * NETREP_ROT_SCALE;
	q = q > 32767.0f ? 32767.0f : q < -32767.0f ? -32767.0f : q;
	return (int16_t)lrintf(q);
}

int NetRep_WriteAckInput(uint8_t *buf, int cap, uint16_t newest, uint32_t bits, const float *view,
	const NetRepInput *in, int n, uint32_t shownMs) {
	int size = NetRep_WriteAck(buf, cap, newest, bits, view);
	NetRepBuf w;
	if (!size) {
		return 0;
	}
	NetRepBuf_Init(&w, buf + size, cap - size);
	n = n < 0 ? 0 : n > NETREP_INPUT_REDUNDANCY ? NETREP_INPUT_REDUNDANCY : n;
	NetRep_PutU8(&w, (uint32_t)n);
	if (n) {
		NetRep_PutU32(&w, in[0].tick);
		NetRep_PutU32(&w, shownMs);
	}
	for (int i = 0; i < n; i++) {
		uint32_t back = i ? in[i - 1].tick - in[i].tick : 0;
		if (back > 255 || (i && !back)) {
			return 0;
		}
		NetRep_PutU8(&w, back);
		for (int k = 0; k < 3; k++) {
			NetRep_PutU8(&w, (uint8_t)in[i].move[k]);
		}
		NetRep_PutU16(&w, (uint16_t)in[i].rot[0]);
		NetRep_PutU16(&w, (uint16_t)in[i].rot[1]);
		NetRep_PutU16(&w, in[i].buttons);
		NetRep_PutU8(&w, in[i].weapon);
		NetRep_PutU16(&w, in[i].slot);
	}
	return w.bad ? 0 : size + w.size;
}

int NetRep_ReadAckInput(const uint8_t *data, int size, uint16_t *newest, uint32_t *bits, float *view,
	NetRepInput *in, int max, int *n, uint32_t *shownMs) {
	NetRepReader r;
	NetRepReader_Init(&r, data, size);
	*n = 0;
	*shownMs = 0;
	if (NetRep_GetU8(&r) != NETREP_MSG_ACK) {
		return 0;
	}
	*newest = (uint16_t)NetRep_GetU16(&r);
	*bits = NetRep_GetU32(&r);
	Rep_GetView(&r, view);
	if (r.bad || r.pos == r.size) {
		return !r.bad;
	}
	int count = (int)NetRep_GetU8(&r);
	if (count > NETREP_INPUT_REDUNDANCY) {
		return 0;
	}
	uint32_t tick = count ? NetRep_GetU32(&r) : 0;
	*shownMs = count ? NetRep_GetU32(&r) : 0;
	for (int i = 0; i < count; i++) {
		NetRepInput x;
		uint32_t back = NetRep_GetU8(&r);
		if (i && !back) {
			return 0;
		}
		tick -= back;
		x.tick = tick;
		for (int k = 0; k < 3; k++) {
			x.move[k] = (int8_t)NetRep_GetU8(&r);
		}
		x.rot[0] = (int16_t)NetRep_GetU16(&r);
		x.rot[1] = (int16_t)NetRep_GetU16(&r);
		x.buttons = (uint16_t)NetRep_GetU16(&r);
		x.weapon = (uint8_t)NetRep_GetU8(&r);
		x.slot = (uint16_t)NetRep_GetU16(&r);
		if (r.bad) {
			return 0;
		}
		if (*n < max) {
			in[(*n)++] = x;
		}
	}
	return !r.bad;
}
