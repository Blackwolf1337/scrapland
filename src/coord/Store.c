/*
 * The player store of the coordinator (include/scrap/coord.h): one file per player key in a folder of the server's
 * data, written atomically, checked by a hash, with the file before it as the fall back.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/coord.h"

#define STORE_HEAD "scrapplayer 1\n"
#define STORE_SUM "sum "
#define STORE_SUM_BYTES 16
#define STORE_FILE (COORD_STATE_MAX + 128)

static int Store_Path(char *out, int cap, const char *dir, const uint8_t key[NET2_KEY_SIZE], const char *ext) {
	char hex[2 * NET2_KEY_SIZE + 1];
	Coord_Hex(key, NET2_KEY_SIZE, hex);
	return snprintf(out, (size_t)cap, "%s/%s.%s", dir, hex, ext) < cap;
}

/* a file's text when it is whole: 1 with the state in text, else 0 */
static int Store_Parse(const char *buf, int n, char *text, int *len, int cap) {
	char sum[2 * STORE_SUM_BYTES + 1];
	uint8_t hash[STORE_SUM_BYTES];
	int head = (int)sizeof(STORE_HEAD) - 1;
	int tail = (int)sizeof(STORE_SUM) - 1 + 2 * STORE_SUM_BYTES + 1;

	if (n < head + tail || memcmp(buf, STORE_HEAD, (size_t)head) || buf[n - 1] != '\n') {
		return 0;
	}
	int at = n - tail;
	if ((at > head && buf[at - 1] != '\n') || memcmp(buf + at, STORE_SUM, sizeof(STORE_SUM) - 1)) {
		return 0;
	}
	Net2_Hash(hash, STORE_SUM_BYTES, buf, (size_t)at);
	Coord_Hex(hash, STORE_SUM_BYTES, sum);
	if (memcmp(buf + at + sizeof(STORE_SUM) - 1, sum, 2 * STORE_SUM_BYTES)) {
		return 0;
	}
	int body = at - head;
	if (body > cap || !CoordKv_Valid(buf + head, body)) {
		return 0;
	}
	memcpy(text, buf + head, (size_t)body);
	*len = body;
	if (body < cap) {
		text[body] = 0;
	}
	return 1;
}

int CoordStore_Load(const char *dir, const uint8_t key[NET2_KEY_SIZE], char *text, int *len, int cap) {
	static char buf[STORE_FILE];
	char path[COORD_PATH], bak[COORD_PATH], bad[COORD_PATH];

	*len = 0;
	if (cap > 0) {
		text[0] = 0;
	}
	if (!Store_Path(path, sizeof(path), dir, key, "dat") || !Store_Path(bak, sizeof(bak), dir, key, "bak") ||
		!Store_Path(bad, sizeof(bad), dir, key, "corrupt")) {
		return COORD_LD_IO;
	}
	int n = Coord_ReadFile(path, buf, sizeof(buf));
	if (n >= 0 && Store_Parse(buf, n, text, len, cap)) {
		return COORD_LD_OK;
	}
	int main = n;
	n = Coord_ReadFile(bak, buf, sizeof(buf));
	if (n >= 0 && Store_Parse(buf, n, text, len, cap)) {
		return COORD_LD_BACKUP;
	}
	if (main == -1 && n == -1) {
		return COORD_LD_NEW;
	}
	if (main == -2 && n == -1) {
		/* a file that cannot be read at all is no reason to start the player again */
		FILE *f = fopen(path, "rb");
		if (!f) {
			return COORD_LD_IO;
		}
		fclose(f);
	}
	/* neither file is whole: the newest one kept aside for a look, the player starts new */
	remove(bad);
	if (rename(main != -1 ? path : bak, bad) != 0) {
		return COORD_LD_IO;
	}
	*len = 0;
	if (cap > 0) {
		text[0] = 0;
	}
	return COORD_LD_CORRUPT;
}

int CoordStore_Save(const char *dir, const uint8_t key[NET2_KEY_SIZE], const char *text, int len) {
	static char buf[STORE_FILE];
	char path[COORD_PATH], bak[COORD_PATH], sum[2 * STORE_SUM_BYTES + 1];
	uint8_t hash[STORE_SUM_BYTES];
	int head = (int)sizeof(STORE_HEAD) - 1;

	if (len < 0 || len > COORD_STATE_MAX || !CoordKv_Valid(text, len) || !Store_Path(path, sizeof(path), dir, key, "dat") ||
		!Store_Path(bak, sizeof(bak), dir, key, "bak")) {
		return 0;
	}
	memcpy(buf, STORE_HEAD, (size_t)head);
	memcpy(buf + head, text, (size_t)len);
	int n = head + len;
	if (len > 0 && text[len - 1] != '\n') {
		buf[n++] = '\n';
	}
	Net2_Hash(hash, STORE_SUM_BYTES, buf, (size_t)n);
	Coord_Hex(hash, STORE_SUM_BYTES, sum);
	n += snprintf(buf + n, sizeof(buf) - (size_t)n, STORE_SUM "%s\n", sum);
	return Coord_WriteAtomic(path, buf, n, bak);
}
