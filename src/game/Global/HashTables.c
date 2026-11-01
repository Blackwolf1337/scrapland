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
