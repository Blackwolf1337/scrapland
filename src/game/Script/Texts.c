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
	{ "MP_CoopSceneSkipped", "Skipped. The others still watch the scene." },
	{ "MP_CoopToLeader", "Going to the party leader..." },
	{ "MP_CoopLeaderHere", "The party leader is here." },
	{ "MP_CoopNoLeader", "No party leader in the world." },
	{ "MP_CoopEnd", "The party finished the story!" },
	{ "MP_Controls_CoopLeader", "To the party leader (coop)" },
	{ "MP_Mode_dmbots", "DeathMatch with bots" },
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
	{ "MP_Arena_Delivered", "%s delivered the mail bag" },
	{ "MP_Arena_BagBack", "The mail bag is back at its start" },
	{ "MP_Place_Go", "To %s" },
};

const char *Texts_Get(const char *key) {
	for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
		if (!strcmp(texts[i].key, key)) {
			return texts[i].text;
		}
	}
	return NULL;
}

void Texts_Fill(char *out, int cap, const char *fmt, int num, const char *name, const char *name2) {
	const char *names[2] = { name ? name : "", name2 ? name2 : "" };
	int n = 0, k = 0;
	if (cap <= 0) {
		return;
	}
	for (const char *f = fmt ? fmt : ""; *f && n < cap - 1; f++) {
		char tmp[16];
		const char *ins = NULL;
		if (f[0] == '%' && f[1] == 's') {
			ins = names[k < 2 ? k : 1];
			k++;
		} else if (f[0] == '%' && (f[1] == 'd' || f[1] == 'i')) {
			Str_PrintfQuiet(tmp, sizeof(tmp), "%d", num);
			ins = tmp;
		} else if (f[0] == '%' && f[1] == '%') {
			ins = "%";
		}
		if (!ins) {
			out[n++] = *f;
			continue;
		}
		for (; *ins && n < cap - 1; ins++) {
			out[n++] = *ins;
		}
		f++;
	}
	out[n] = 0;
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
