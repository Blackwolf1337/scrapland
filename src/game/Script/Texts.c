/*
 * The texts ScrapC adds, see include/scrap/texts.h. Keys start with MP_; a text with %s or %d is a format.
 */
#include <string.h>

#include "scrap/global.h"
#include "scrap/script.h"
#include "scrap/texts.h"

static const struct {
	const char *key, *text;
} texts[] = {
	{ "MP_FreeRoam", "Free roam" },
	{ "MP_JoinWorld", "Join world" },
	{ "MP_HostWorld", "Host world" },
	{ "MP_ListServer", "List server" },
	{ "MP_Name", "Name" },
	{ "MP_Level", "Level" },
	{ "MP_Mode", "Mode" },
	{ "MP_Mode_sandbox", "Sandbox" },
	{ "MP_Mode_freeroam", "Free roam" },
	{ "MP_Mode_coop", "Coop story" },
	{ "MP_CoopTravel", "The story goes on in %s" },
	{ "MP_CoopScene", "A cutscene plays in %s" },
	{ "MP_PvP", "PvP" },
	{ "MP_Public", "List server" },
	{ "MP_On", "On" },
	{ "MP_Off", "Off" },
	{ "MP_MaxPlayers", "Max players" },
	{ "MP_Port", "Port" },
	{ "MP_HostButton", "Host" },
	{ "MP_Searching", "Searching..." },
	{ "MP_Found", "Worlds found: %d" },
	{ "MP_NoListServer", "No list server set" },
	{ "MP_ListNotReached", "List server not reached" },
	{ "MP_Connecting", "Connecting to %s" },
	{ "MP_Starting", "Starting the world" },
	{ "MP_BadAddress", "Not a numeric address: %s" },
	{ "MP_NotReached", "World not reached" },
	{ "MP_Refused", "Refused: %s" },
	{ "MP_WorldGone", "The world is gone" },
	{ "MP_HostFailed", "The world could not be started" },
	{ "MP_Lost", "Connection lost: %s" },
};

const char *Texts_Get(const char *key) {
	for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
		if (!strcmp(texts[i].key, key)) {
			return texts[i].text;
		}
	}
	return NULL;
}

void Texts_Format(char *out, int size, const char *key, const char *arg) {
	const char *fmt = Script_Lang(key);
	const char *at = strchr(fmt, '%');
	if (at && (at[1] == 's' || at[1] == 'd')) {
		Str_PrintfQuiet(out, size, "%.*s%s%s", (int)(at - fmt), fmt, arg, at + 2);
	} else {
		Str_Copy(out, fmt, size);
	}
}
