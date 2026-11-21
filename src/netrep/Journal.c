/*
 * NetRep: the entity variable journal of the replication v2 (include/scrap/netrep.h, include/scrap/repent.h): a pool
 * of entries, a doubly linked list per owner in the order of the last sets, the wire form of a value.
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/netrep.h"

#define J_NONE -1
#define J_CHUNK_BITS 12				/* entries are allocated in chunks of 4096 as the pool grows */
#define J_CHUNK (1 << J_CHUNK_BITS)

typedef struct JEntry {
	int32_t prev, next;				/* in the owner's list, or next in the free list */
	uint8_t dirty;
	NetRepJVal v;
} JEntry;

struct NetRepJournal {
	int owners, entries, used;
	int top;						/* entries below it were taken once */
	uint32_t lost;
	int32_t freeHead;
	int32_t *head, *tail;
	int32_t *count;
	JEntry **chunk;					/* (entries + J_CHUNK - 1) / J_CHUNK, allocated up to top */
};

/* entry k (below top) */
static inline JEntry *J_E(const NetRepJournal *j, int k) {
	return &j->chunk[k >> J_CHUNK_BITS][k & (J_CHUNK - 1)];
}

NetRepJournal *NetRepJ_Create(int owners, int entries) {
	NetRepJournal *j = calloc(1, sizeof(*j));
	if (!j) {
		return NULL;
	}
	j->owners = owners;
	j->entries = entries;
	j->head = malloc(sizeof(*j->head) * (size_t)owners);
	j->tail = malloc(sizeof(*j->tail) * (size_t)owners);
	j->count = calloc((size_t)owners, sizeof(*j->count));
	j->chunk = calloc((size_t)(entries + J_CHUNK - 1) / J_CHUNK, sizeof(*j->chunk));
	if (!j->head || !j->tail || !j->count || !j->chunk) {
		NetRepJ_Destroy(j);
		return NULL;
	}
	for (int o = 0; o < owners; o++) {
		j->head[o] = j->tail[o] = J_NONE;
	}
	j->freeHead = J_NONE;
	return j;
}

void NetRepJ_Destroy(NetRepJournal *j) {
	if (!j) {
		return;
	}
	free(j->head);
	free(j->tail);
	free(j->count);
	if (j->chunk) {
		for (int c = 0; c < (j->entries + J_CHUNK - 1) / J_CHUNK; c++) {
			free(j->chunk[c]);
		}
	}
	free(j->chunk);
	free(j);
}

int NetRepJ_Equal(const NetRepJVal *a, const NetRepJVal *b) {
	if (a->type != b->type || strcmp(a->name, b->name)) {
		return 0;
	}
	switch (a->type) {
	case NETREP_JI:
		return a->u.i == b->u.i;
	case NETREP_JF:
		return !memcmp(&a->u.f, &b->u.f, sizeof(a->u.f));
	case NETREP_JV:
		return !memcmp(a->u.v, b->u.v, sizeof(a->u.v));
	case NETREP_JS:
		return !strcmp(a->u.s, b->u.s);
	}
	return 0;
}

static void J_Unlink(NetRepJournal *j, int owner, int k) {
	JEntry *x = J_E(j, k);
	if (x->prev != J_NONE) {
		J_E(j, x->prev)->next = x->next;
	} else {
		j->head[owner] = x->next;
	}
	if (x->next != J_NONE) {
		J_E(j, x->next)->prev = x->prev;
	} else {
		j->tail[owner] = x->prev;
	}
}

static void J_Append(NetRepJournal *j, int owner, int k) {
	JEntry *x = J_E(j, k);
	x->prev = j->tail[owner];
	x->next = J_NONE;
	if (j->tail[owner] != J_NONE) {
		J_E(j, j->tail[owner])->next = k;
	} else {
		j->head[owner] = k;
	}
	j->tail[owner] = k;
}

int NetRepJ_Find(const NetRepJournal *j, int owner, const char *name) {
	if (owner < 0 || owner >= j->owners) {
		return J_NONE;
	}
	for (int k = j->head[owner]; k != J_NONE; k = J_E(j, k)->next) {
		if (!strcmp(J_E(j, k)->v.name, name)) {
			return k;
		}
	}
	return J_NONE;
}

int NetRepJ_Set(NetRepJournal *j, int owner, const NetRepJVal *v) {
	if (owner < 0 || owner >= j->owners) {
		return -1;
	}
	int k = NetRepJ_Find(j, owner, v->name);
	if (k != J_NONE) {
		if (k == j->tail[owner] && NetRepJ_Equal(&J_E(j, k)->v, v)) {
			return 0;
		}
		J_Unlink(j, owner, k);
	} else {
		if (j->freeHead != J_NONE) {
			k = j->freeHead;
			j->freeHead = J_E(j, k)->next;
		} else if (j->top < j->entries && (j->chunk[j->top >> J_CHUNK_BITS] ||
			(j->chunk[j->top >> J_CHUNK_BITS] = malloc(sizeof(JEntry) * J_CHUNK)) != NULL)) {
			k = j->top++;
		} else {
			j->lost++;
			return -1;
		}
		j->used++;
		j->count[owner]++;
	}
	J_E(j, k)->v = *v;
	J_E(j, k)->v.name[NETREP_JNAME - 1] = 0;
	if (v->type == NETREP_JS) {
		J_E(j, k)->v.u.s[NETREP_JSTR - 1] = 0;
	}
	J_E(j, k)->dirty = 1;
	J_Append(j, owner, k);
	return 1;
}

void NetRepJ_Clear(NetRepJournal *j, int owner) {
	if (owner < 0 || owner >= j->owners) {
		return;
	}
	for (int k = j->head[owner]; k != J_NONE;) {
		int next = J_E(j, k)->next;
		J_E(j, k)->next = j->freeHead;
		j->freeHead = k;
		j->used--;
		k = next;
	}
	j->head[owner] = j->tail[owner] = J_NONE;
	j->count[owner] = 0;
}

int NetRepJ_Count(const NetRepJournal *j, int owner) {
	return owner >= 0 && owner < j->owners ? j->count[owner] : 0;
}

int NetRepJ_First(const NetRepJournal *j, int owner) {
	return owner >= 0 && owner < j->owners ? j->head[owner] : J_NONE;
}

int NetRepJ_Next(const NetRepJournal *j, int k) {
	return J_E(j, k)->next;
}

const NetRepJVal *NetRepJ_Get(const NetRepJournal *j, int k) {
	return &J_E(j, k)->v;
}

int NetRepJ_IsDirty(const NetRepJournal *j, int k) {
	return J_E(j, k)->dirty;
}

void NetRepJ_Clean(NetRepJournal *j, int owner) {
	for (int k = NetRepJ_First(j, owner); k != J_NONE; k = J_E(j, k)->next) {
		J_E(j, k)->dirty = 0;
	}
}

int NetRepJ_Used(const NetRepJournal *j) {
	return j->used;
}

uint32_t NetRepJ_Lost(const NetRepJournal *j) {
	return j->lost;
}

static void J_PutDouble(NetRepBuf *w, double d) {
	uint64_t b;
	memcpy(&b, &d, sizeof(b));
	NetRep_PutU32(w, (uint32_t)b);
	NetRep_PutU32(w, (uint32_t)(b >> 32));
}

static double J_GetDouble(NetRepReader *r) {
	uint64_t b = NetRep_GetU32(r);
	b |= (uint64_t)NetRep_GetU32(r) << 32;
	double d;
	memcpy(&d, &b, sizeof(d));
	return d;
}

void NetRep_PutJVal(NetRepBuf *w, const NetRepJVal *v) {
	NetRep_PutStr(w, v->name);
	NetRep_PutU8(w, v->type);
	switch (v->type) {
	case NETREP_JI:
		NetRep_PutSVar(w, v->u.i);
		break;
	case NETREP_JF:
		J_PutDouble(w, v->u.f);
		break;
	case NETREP_JV:
		for (int i = 0; i < 3; i++) {
			J_PutDouble(w, v->u.v[i]);
		}
		break;
	case NETREP_JS:
		NetRep_PutStr(w, v->u.s);
		break;
	}
}

int NetRep_GetJVal(NetRepReader *r, NetRepJVal *v) {
	memset(v, 0, sizeof(*v));
	NetRep_GetStr(r, v->name, NETREP_JNAME);
	v->type = (uint8_t)NetRep_GetU8(r);
	switch (v->type) {
	case NETREP_JI:
		v->u.i = NetRep_GetSVar(r);
		break;
	case NETREP_JF:
		v->u.f = J_GetDouble(r);
		break;
	case NETREP_JV:
		for (int i = 0; i < 3; i++) {
			v->u.v[i] = J_GetDouble(r);
		}
		break;
	case NETREP_JS:
		NetRep_GetStr(r, v->u.s, NETREP_JSTR);
		break;
	default:
		return 0;
	}
	return !r->bad && v->name[0];
}
