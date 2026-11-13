/*
 * The party of a coop world and its story (stage M7a of the multiplayer plan, include/scrap/coord.h): the story text,
 * the changes the instances send, the party's file and the members' copies.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/coord.h"
#include "scrap/netrep.h"

#define PARTY_KEY 64

int CoordStory_Next(NetRepReader *r, int *kind, char *name, int ncap, char *val, int vcap) {
	*kind = (int)NetRep_GetU8(r);
	NetRep_GetStr(r, name, ncap);
	val[0] = 0;
	if (*kind == COORD_OP_SET || *kind == COORD_OP_ADD) {
		int n = (int)NetRep_GetU16(r);
		if (n >= vcap) {
			r->bad = 1;
		}
		CoordMsg_GetBytes(r, val, r->bad ? 0 : n);
		val[r->bad ? 0 : n] = 0;
	} else if (*kind != COORD_OP_DEL) {
		r->bad = 1;
	}
	return r->bad ? -1 : 1;
}

int CoordStory_Int(const char *s, long *out) {
	char *end;
	if (!*s) {
		return 0;
	}
	long v = strtol(s, &end, 10);
	if (*end) {
		return 0;
	}
	*out = v;
	return 1;
}

void CoordStory_Put(NetRepBuf *w, int kind, const char *name, const char *value) {
	NetRep_PutU8(w, (uint32_t)kind);
	NetRep_PutStr(w, name);
	if (kind != COORD_OP_DEL) {
		int n = (int)strlen(value);
		NetRep_PutU16(w, (uint32_t)n);
		CoordMsg_PutBytes(w, value, n);
	}
}

int CoordStory_Apply(char *text, int *len, int cap, const uint8_t *msg, int size, int *flags, NetRepBuf *out) {
	static char val[COORD_STORY_VALUE + 1], old[COORD_STORY_VALUE + 1];
	char name[PARTY_KEY];
	NetRepReader r;
	int kind, applied = 0, at = out ? out->size : 0;
	long a, d;

	*flags = 0;
	NetRepReader_Init(&r, msg, size);
	int count = (int)NetRep_GetU16(&r);
	if (out) {
		NetRep_PutU16(out, 0);
	}
	for (int i = 0; i < count && !r.bad; i++) {
		if (CoordStory_Next(&r, &kind, name, sizeof(name), val, sizeof(val)) < 0) {
			break;
		}
		int had = CoordKv_Get(text, *len, name, old, sizeof(old));
		if (kind == COORD_OP_ADD) {
			/* a counter: added to the story's number (a value that is no number is set as it is) */
			if (CoordStory_Int(val, &d)) {
				snprintf(val, sizeof(val), "%ld", (had && CoordStory_Int(old, &a) ? a : 0) + d);
				*flags |= COORD_STORY_ADDED;
			}
			kind = COORD_OP_SET;
		}
		int done = kind == COORD_OP_SET ? CoordKv_Set(text, len, cap, name, val) : CoordKv_Del(text, len, name);
		if (done && out) {
			CoordStory_Put(out, kind, name, kind == COORD_OP_SET ? val : NULL);
		}
		if (done && (!strcmp(name, "Mission.Map") || !strcmp(name, "Mission.Library")) &&
			(kind == COORD_OP_DEL ? had : !had || strcmp(old, val))) {
			*flags |= COORD_STORY_MISSION;
		}
		applied += done;
	}
	if (out && !out->bad) {
		out->p[at] = (uint8_t)applied;
		out->p[at + 1] = (uint8_t)(applied >> 8);
	}
	return r.bad ? -1 : applied;
}

int CoordParty_Finished(const CoordParty *p) {
	char v[8];
	return CoordKv_Get(p->story, p->len, COORD_COOP_FINISHED, v, sizeof(v)) && !strcmp(v, "yes");
}

int CoordCoop_Chapter(const char *level) {
	static const char *const chapters[] = { "Orbit", "Fake", "Junkyard", "AB", "ABIndoor" };
	for (int i = 0; i < (int)(sizeof(chapters) / sizeof(chapters[0])); i++) {
		if (Coord_SameName(chapters[i], level)) {
			return 1;
		}
	}
	return 0;
}

/* <players>/<hex key> */
static void Party_MemberBase(const CoordParty *p, const uint8_t key[NET2_KEY_SIZE], char *out, int cap) {
	char hex[2 * NET2_KEY_SIZE + 1];
	Coord_Hex(key, NET2_KEY_SIZE, hex);
	snprintf(out, (size_t)cap, "%s/%s", p->playersDir, hex);
}

int CoordParty_Load(CoordParty *p, const char *dataDir, const char *playersDir, coordlogfn_t log) {
	char path[COORD_PATH + 16];

	memset(p, 0, sizeof(*p));
	snprintf(p->base, sizeof(p->base), "%s/party", dataDir);
	snprintf(p->playersDir, sizeof(p->playersDir), "%s", playersDir);
	int r = CoordStore_LoadText(p->base, "story", p->story, &p->len, sizeof(p->story));
	if (r == COORD_LD_IO) {
		log("coordinator: cannot read the party's story %s.story\n", p->base);
		return 0;
	}
	if (r == COORD_LD_BACKUP || r == COORD_LD_CORRUPT) {
		log("coordinator: the party's story file was bad, %s\n", r == COORD_LD_BACKUP ? "the one before it was taken" :
			"kept as .storycorrupt, the story starts again");
		p->dirty = 1;
	}
	snprintf(path, sizeof(path), "%s.owner", p->base);
	if (Coord_ReadFile(path, p->owner, sizeof(p->owner)) != 2 * NET2_KEY_SIZE) {
		p->owner[0] = 0;
	}
	log("coordinator: coop party, story %d bytes%s%.8s\n", p->len, p->owner[0] ? ", owner " : "", p->owner);
	return 1;
}

void CoordParty_Join(CoordParty *p, const uint8_t key[NET2_KEY_SIZE], coordlogfn_t log) {
	static char copy[COORD_STORY_MAX];
	char base[COORD_PATH + 80], path[COORD_PATH + 16];
	int len = 0;

	Party_MemberBase(p, key, base, sizeof(base));
	int r = CoordStore_LoadText(base, "story", copy, &len, sizeof(copy));
	if (r != COORD_LD_OK && r != COORD_LD_BACKUP) {
		len = 0;
	}
	if (!p->len && len) {
		memcpy(p->story, copy, (size_t)len);
		p->len = len;
		p->dirty = 1;
		p->seq++;
		log("coordinator: the party takes the story of %.8s (%d bytes)\n", base + strlen(p->playersDir) + 1, len);
	} else if (len && (len != p->len || memcmp(copy, p->story, (size_t)len))) {
		/* the member's own copy stays beside the party's */
		if (!CoordStore_SaveText(base, "own", copy, len)) {
			log("coordinator: cannot keep the own story of %.8s\n", base + strlen(p->playersDir) + 1);
		} else {
			log("coordinator: %.8s takes the party's story, its own kept as .own\n", base + strlen(p->playersDir) + 1);
		}
	}
	if (!p->owner[0]) {
		Coord_Hex(key, NET2_KEY_SIZE, p->owner);
		snprintf(path, sizeof(path), "%s.owner", p->base);
		Coord_WriteAtomic(path, p->owner, 2 * NET2_KEY_SIZE, NULL);
		log("coordinator: %.8s owns the story of this world\n", p->owner);
	}
}

int CoordParty_SaveMember(CoordParty *p, const uint8_t key[NET2_KEY_SIZE]) {
	char base[COORD_PATH + 80];
	if (!p->len) {
		return 1;
	}
	Party_MemberBase(p, key, base, sizeof(base));
	return CoordParty_Save(p) && CoordStore_SaveText(base, "story", p->story, p->len);
}

int CoordParty_Save(CoordParty *p) {
	if (!p->dirty) {
		return 1;
	}
	if (!CoordStore_SaveText(p->base, "story", p->story, p->len)) {
		return 0;
	}
	p->dirty = 0;
	return 1;
}

int CoordParty_StoryFile(const CoordParty *p, const char *dataDir, char *out, int cap) {
	char path[COORD_PATH + 16];
	snprintf(path, sizeof(path), "%s/story.txt", dataDir);
	return Coord_WriteAtomic(path, p->story, p->len, NULL) && Coord_AbsPath(path, out, cap);
}
