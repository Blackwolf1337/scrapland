/*
 * Instances of the older HashTable.h (include/scrap/hashtable.h) of World/ and Sound/ whose values are not
 * known yet (12 bytes in World/) and the sample table of Sound/.
 */
#include "scrap/console.h"
#include "scrap/hashtable.h"
#include "scrap/cstring.h"
#include "scrap/sound.h"

typedef struct cValue12 {
	uint32_t v[3];
} cValue12;

typedef struct cWorldStrItem {
	cValue12 value;			/* 0x00 */
	char *key;			/* 0x0c cString */
	struct cWorldStrItem *next;	/* 0x10 */
} cWorldStrItem;
SD_SIZE(cWorldStrItem, 0x14);

/* cSoundStrItem: the sample table of CSoundXBox (include/scrap/sound.h) */

HASHTABLE_STR(WorldStrTable, cWorldStrItem)
HASHTABLE_STR(SoundStrTable, cSoundStrItem)

/* Get with "Sound not found" in the log when warn is set */
byte __thiscall SoundStrTable_GetWarn(HashTable *self, const char *key, cSoundTableValue *out, byte warn) {
	if (SoundStrTable_Get(self, key, out)) {
		return 1;
	}
	if (warn) {
		Con_Warnf("Sound not found: %s\n", key);
	}
	return 0;
}
