/*
 * Instances of the older HashTable.h (include/scrap/hashtable.h) of World/ and Sound/ whose values are not
 * known yet: 12 bytes in World/, 36 bytes in Sound/ (the sound descriptors by name).
 */
#include "scrap/console.h"
#include "scrap/hashtable.h"
#include "scrap/cstring.h"

typedef struct cValue12 {
	uint32_t v[3];
} cValue12;

typedef struct cWorldStrItem {
	cValue12 value;			/* 0x00 */
	char *key;			/* 0x0c cString */
	struct cWorldStrItem *next;	/* 0x10 */
} cWorldStrItem;
SD_SIZE(cWorldStrItem, 0x14);

/* an int and a cSoundDesc (its constructor is overwritten by the copy) */
typedef struct cSoundTableValue {
	uint32_t v[9];
} cSoundTableValue;

typedef struct cSoundStrItem {
	cSoundTableValue value;		/* 0x00 */
	char *key;			/* 0x24 cString */
	struct cSoundStrItem *next;	/* 0x28 */
} cSoundStrItem;
SD_SIZE(cSoundStrItem, 0x2c);

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
