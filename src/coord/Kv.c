/*
 * The state text of a player (include/scrap/coord.h): "key=value" lines, the values escaped.
 */
#include <string.h>

#include "scrap/coord.h"

#define KV_KEY 64

int CoordKv_KeyOk(const char *key) {
	int n = 0;
	for (const char *p = key; *p; p++, n++) {
		char c = *p;
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
			c == '-')) {
			return 0;
		}
	}
	return n > 0 && n < KV_KEY;
}

/* the line at pos: key [ks, ke), value [vs, ve), the next line at *next; 0 at the end, -1 a malformed line */
static int Kv_Line(const char *text, int len, int pos, int *ks, int *ke, int *vs, int *ve, int *next) {
	if (pos >= len) {
		return 0;
	}
	int end = pos;
	while (end < len && text[end] != '\n') {
		end++;
	}
	*next = end < len ? end + 1 : end;
	int eq = pos;
	while (eq < end && text[eq] != '=') {
		eq++;
	}
	if (eq == end || eq == pos || eq - pos >= KV_KEY) {
		return -1;
	}
	for (int i = pos; i < eq; i++) {
		char c = text[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
			c == '-')) {
			return -1;
		}
	}
	*ks = pos;
	*ke = eq;
	*vs = eq + 1;
	*ve = end;
	return 1;
}

static void Kv_Unescape(const char *v, int n, char *out, int cap) {
	int o = 0;
	for (int i = 0; i < n && o < cap - 1; i++) {
		char c = v[i];
		if (c == '\\' && i + 1 < n) {
			c = v[++i];
			c = c == 'n' ? '\n' : c == 'r' ? '\r' : c;
		}
		out[o++] = c;
	}
	if (cap > 0) {
		out[o] = 0;
	}
}

/* the escaped value: its length, -1 when it does not fit cap */
static int Kv_Escape(const char *v, char *out, int cap) {
	int o = 0;
	for (const char *p = v; *p; p++) {
		char c = *p;
		int two = c == '\\' || c == '\n' || c == '\r';
		if (o + 1 + two > cap) {
			return -1;
		}
		if (two) {
			out[o++] = '\\';
			c = c == '\n' ? 'n' : c == '\r' ? 'r' : c;
		}
		out[o++] = c;
	}
	return o;
}

static int Kv_Find(const char *text, int len, const char *key, int *ls, int *vs, int *ve, int *next) {
	int n = (int)strlen(key), ks, ke;
	for (int pos = 0, r; (r = Kv_Line(text, len, pos, &ks, &ke, vs, ve, next)) != 0; pos = *next) {
		if (r > 0 && ke - ks == n && !memcmp(text + ks, key, (size_t)n)) {
			*ls = ks;
			return 1;
		}
	}
	return 0;
}

int CoordKv_Get(const char *text, int len, const char *key, char *out, int cap) {
	int ls, vs, ve, next;
	if (!Kv_Find(text, len, key, &ls, &vs, &ve, &next)) {
		if (cap > 0) {
			out[0] = 0;
		}
		return 0;
	}
	Kv_Unescape(text + vs, ve - vs, out, cap);
	return 1;
}

int CoordKv_Set(char *text, int *len, int cap, const char *key, const char *value) {
	char esc[COORD_STATE_MAX];
	int ls, vs, ve, next;

	if (!CoordKv_KeyOk(key)) {
		return 0;
	}
	int n = Kv_Escape(value, esc, sizeof(esc));
	if (n < 0) {
		return 0;
	}
	if (Kv_Find(text, *len, key, &ls, &vs, &ve, &next)) {
		int grow = n - (ve - vs);
		if (*len + grow > cap) {
			return 0;
		}
		memmove(text + vs + n, text + ve, (size_t)(*len - ve));
		memcpy(text + vs, esc, (size_t)n);
		*len += grow;
		return 1;
	}
	int kn = (int)strlen(key);
	int lead = *len > 0 && text[*len - 1] != '\n';
	if (*len + lead + kn + 1 + n + 1 > cap) {
		return 0;
	}
	if (lead) {
		text[(*len)++] = '\n';
	}
	memcpy(text + *len, key, (size_t)kn);
	*len += kn;
	text[(*len)++] = '=';
	memcpy(text + *len, esc, (size_t)n);
	*len += n;
	text[(*len)++] = '\n';
	return 1;
}

int CoordKv_Next(const char *text, int len, int *pos, char *key, int kcap, char *val, int vcap) {
	int ks, ke, vs, ve, next;
	for (int r; (r = Kv_Line(text, len, *pos, &ks, &ke, &vs, &ve, &next)) != 0;) {
		*pos = next;
		if (r < 0) {
			continue;
		}
		int kn = ke - ks < kcap - 1 ? ke - ks : kcap - 1;
		memcpy(key, text + ks, (size_t)kn);
		key[kn] = 0;
		Kv_Unescape(text + vs, ve - vs, val, vcap);
		return 1;
	}
	return 0;
}

int CoordKv_Merge(char *text, int *len, int cap, const char *from, int fromLen) {
	char key[KV_KEY], val[COORD_STATE_MAX];
	int ok = 1;
	for (int pos = 0; CoordKv_Next(from, fromLen, &pos, key, sizeof(key), val, sizeof(val));) {
		ok = CoordKv_Set(text, len, cap, key, val) && ok;
	}
	return ok;
}

int CoordKv_Valid(const char *text, int len) {
	int ks, ke, vs, ve, next;
	for (int pos = 0, r; (r = Kv_Line(text, len, pos, &ks, &ke, &vs, &ve, &next)) != 0; pos = next) {
		if (r < 0) {
			return 0;
		}
		for (int i = vs; i < ve; i++) {
			if (text[i] == 0 || text[i] == '\r') {
				return 0;
			}
		}
	}
	return 1;
}

int CoordKv_Del(char *text, int *len, const char *key) {
	int ls, vs, ve, next;
	if (!Kv_Find(text, *len, key, &ls, &vs, &ve, &next)) {
		return 0;
	}
	memmove(text + ls, text + next, (size_t)(*len - next));
	*len -= next - ls;
	return 1;
}
