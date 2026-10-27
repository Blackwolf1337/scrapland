/*
 * Instances of the older HashTable.h (include/scrap/hashtable.h) of World/ and Sound/: the 12 byte values of
 * World/ (the lairs of the ship groups of Logic/Scrapyard.c) and the sample table of Sound/.
 */
#include "scrap/console.h"
#include "scrap/hashtable.h"
#include "scrap/cstring.h"
#include "scrap/sound.h"

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
