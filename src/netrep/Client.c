/*
 * NetRep: the client side (include/scrap/netrep.h). Per slot the states of the last NETREP_HIST snapshots taken (the
 * bases the server may name), the snapshot of the newest record applied (older ones only fill the history), the
 * kind and look; the look strings; the messages taken for the ack.
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/netrep.h"

typedef struct CliHist {
	uint32_t snap;
	NetRepState st;
} CliHist;

typedef struct CliEnt {
	uint8_t exists;
	uint8_t kind;
	uint16_t look;
	uint32_t applied;				/* snapshot of the newest record applied (also after a remove) */
	NetRepState cur;
	CliHist hist[NETREP_HIST];
} CliEnt;

struct NetRepView {
	int haveAck;
	uint16_t newest;
	uint32_t bits;
	uint32_t tick;
	uint8_t haveLook[NETREP_MAX_LOOKS];
	char looks[NETREP_MAX_LOOKS][NETREP_LOOK_MAX + 1];
	CliEnt ent[NETREP_MAX_ENTS];
};

NetRepView *NetRepCli_Create(void) {
	return calloc(1, sizeof(NetRepView));
}

void NetRepCli_Destroy(NetRepView *v) {
	free(v);
}

static void Cli_Taken(NetRepView *v, uint16_t seq) {
	if (!v->haveAck) {
		v->haveAck = 1;
		v->newest = seq;
		v->bits = 0;
		return;
	}
	int16_t d = (int16_t)(uint16_t)(seq - v->newest);
	if (d > 0) {
		v->bits = d >= 33 ? 0 : d == 32 ? 1u << 31 : (v->bits << d) | (1u << (d - 1));
		v->newest = seq;
	} else if (d < 0 && d >= -32) {
		v->bits |= 1u << (-d - 1);
	}
}

static const NetRepState *Cli_Base(const CliEnt *e, uint32_t snap) {
	const CliHist *h = &e->hist[snap % NETREP_HIST];
	return e->exists && h->snap == snap ? &h->st : NULL;
}

int NetRepCli_Message(NetRepView *v, const uint8_t *data, int size, const NetRepEvents *ev) {
	static const NetRepState zero;
	char look[NETREP_LOOK_MAX + 1];
	NetRepReader r;

	NetRepReader_Init(&r, data, size);
	if (NetRep_GetU8(&r) != NETREP_MSG_SNAP) {
		return 0;
	}
	uint16_t seq = (uint16_t)NetRep_GetU16(&r);
	uint32_t snap = NetRep_GetU32(&r);
	uint32_t tick = NetRep_GetU32(&r);
	uint32_t count = NetRep_GetU16(&r);
	if (r.bad || snap == 0) {
		return 0;
	}
	int ok = 1;
	for (uint32_t i = 0; i < count; i++) {
		uint32_t slot = NetRep_GetVar(&r);
		uint32_t op = NetRep_GetU8(&r);
		if (r.bad || slot >= NETREP_MAX_ENTS || (op & ~7u) || ((op & NETREP_REC_LOOK) && (op & 3) != NETREP_OP_CREATE)) {
			return 0;
		}
		CliEnt *e = &v->ent[slot];
		int newer = snap > e->applied;
		if ((op & 3) == NETREP_OP_REMOVE) {
			if (newer) {
				if (e->exists && ev && ev->remove) {
					ev->remove(ev->ctx, (int)slot);
				}
				e->exists = 0;
				e->applied = snap;
			}
			continue;
		}
		const NetRepState *base = &zero;
		uint32_t kind = 0, lookId = 0;
		int have = 1;
		switch (op & 3) {
		case NETREP_OP_DELTA: {
			uint32_t age = NetRep_GetVar(&r);
			base = age && age < snap ? Cli_Base(e, snap - age) : NULL;
			if (!base) {
				base = &zero;
				have = 0;
			}
			break;
		}
		case NETREP_OP_FULL:
			have = e->exists;
			break;
		default:
			kind = NetRep_GetU8(&r);
			lookId = NetRep_GetVar(&r);
			if (lookId >= NETREP_MAX_LOOKS) {
				return 0;
			}
			if (op & NETREP_REC_LOOK) {
				NetRep_GetStr(&r, look, sizeof(look));
				memcpy(v->looks[lookId], look, sizeof(look));
				v->haveLook[lookId] = 1;
			}
			have = v->haveLook[lookId];
			break;
		}
		uint32_t mask = NetRep_GetVar(&r);
		NetRepState st;
		if (!NetRep_ReadFields(&r, base, &st, mask)) {
			return 0;
		}
		if (!have) {
			ok = 0;
			continue;
		}
		if ((op & 3) == NETREP_OP_CREATE) {
			int same = e->exists && e->kind == kind && e->look == lookId;
			if (!newer) {
				/* older than the newest record: history of the same entity, else an entity that is gone */
				CliHist *h = &e->hist[snap % NETREP_HIST];
				if (same && h->snap < snap) {
					h->snap = snap;
					h->st = st;
				}
				continue;
			}
			if (!same) {
				if (e->exists && ev && ev->remove) {
					ev->remove(ev->ctx, (int)slot);
				}
				memset(e->hist, 0, sizeof(e->hist));
			}
			e->exists = 1;
			e->kind = (uint8_t)kind;
			e->look = (uint16_t)lookId;
			e->hist[snap % NETREP_HIST].snap = snap;
			e->hist[snap % NETREP_HIST].st = st;
			e->applied = snap;
			e->cur = st;
			if (ev) {
				if (!same && ev->create) {
					ev->create(ev->ctx, (int)slot, (int)kind, v->looks[lookId], tick, &st);
				} else if (same && ev->update) {
					ev->update(ev->ctx, (int)slot, tick, &st);
				}
			}
			continue;
		}
		/* delta or full of an entity the client has: an older one only fills the history */
		CliHist *h = &e->hist[snap % NETREP_HIST];
		if (h->snap < snap) {
			h->snap = snap;
			h->st = st;
		}
		if (newer) {
			e->applied = snap;
			e->cur = st;
			if (ev && ev->update) {
				ev->update(ev->ctx, (int)slot, tick, &st);
			}
		}
	}
	if (r.bad || r.pos != r.size) {
		return 0;
	}
	if (tick > v->tick) {
		v->tick = tick;
	}
	if (ok) {
		Cli_Taken(v, seq);
	}
	return ok;
}

int NetRepCli_Ack(const NetRepView *v, uint16_t *newest, uint32_t *bits) {
	if (!v->haveAck) {
		return 0;
	}
	*newest = v->newest;
	*bits = v->bits;
	return 1;
}

const NetRepState *NetRepCli_State(const NetRepView *v, int slot, int *kind, int *look) {
	if (slot < 0 || slot >= NETREP_MAX_ENTS || !v->ent[slot].exists) {
		return NULL;
	}
	if (kind) {
		*kind = v->ent[slot].kind;
	}
	if (look) {
		*look = v->ent[slot].look;
	}
	return &v->ent[slot].cur;
}

const char *NetRepCli_LookString(const NetRepView *v, int look) {
	return look >= 0 && look < NETREP_MAX_LOOKS && v->haveLook[look] ? v->looks[look] : NULL;
}

uint32_t NetRepCli_Tick(const NetRepView *v) {
	return v->tick;
}
