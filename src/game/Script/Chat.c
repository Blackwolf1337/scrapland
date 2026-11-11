/*
 * The Chat module in C (scripts/scorer/chat.py, issue #2): the chat of a network game. init.py preloads it only
 * for network games and Init.Init calls SetChatProc, which names the callbacks of the engine (the chat key, the
 * received lines, the console, the six quick messages). The chat line itself is data/menus/chat.def.
 *
 * Native C (docs/objlib-plan.md, the pilot of stage 2): the state is the struct chat, the functions call the engine
 * in C and work on C texts; the PyMethodDef entries are adapters. SendID (the receiver of each player: "ALL" or the
 * ship "net<n>") and MyShip (the ship of this machine, skipped while cycling) are mirrored into the module dict
 * (OBJLIB 5): chat.mod sets them, the tests read and write them there. A line goes over the network as
 * "<receiver>\t<text>"; the server shows it and sends it on (to all, or to the one receiver).
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/input.h"
#include "scrap/net.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"

static const char chat_def[] = {
#embed "../../../data/menus/chat.def"
	, 0 };

/* the receivers of SendID, one per local player (chat.mod) */
#define CHAT_PLAYERS 4
/* a line of the chat as the C builds it (the network takes 0x80 characters of it) */
#define CHAT_LINE 0x200

static struct {
	nativeVal_t sendID[CHAT_PLAYERS];	/* texts */
	nativeVal_t myShip;			/* a text, or what SNet.GetMyClientShip gave (0 off the network) */
} chat;

/* OBJLIB 5: the mirror in the module dict, taken in by the entry points that read it */
static const nativeVar_t chat_vars[] = {
	{ "SendID", chat.sendID, CHAT_PLAYERS, 1 << NV_STR },
	{ "MyShip", &chat.myShip, 0, 1 << NV_NONE | 1 << NV_INT | 1 << NV_FLOAT | 1 << NV_STR },
	{ NULL, NULL, 0, 0 },
};
#define CHAT_SENDID (&chat_vars[0])
#define CHAT_MYSHIP (&chat_vars[1])
/* only SendID (the table from its entry on would take MyShip too) */
static const nativeVar_t chat_sendIDOnly[] = {
	{ "SendID", chat.sendID, CHAT_PLAYERS, 1 << NV_STR },
	{ NULL, NULL, 0, 0 },
};

/* a + b + c of the script on texts; c NULL is a value that is no text (the TypeError of string +) */
static byte Chat_Cat(char *out, const char *a, const char *b, const char *c) {
	if (!c) {
		return Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
	}
	Str_Printf(out, CHAT_LINE, "%s%s%s", a, b, c);
	return 1;
}

/* SendID[slot] = text */
static byte Chat_SetTo(int slot, const char *text) {
	Native_SetStr(&chat.sendID[slot], text);
	return Native_Put("Chat", CHAT_SENDID, slot);
}

/* SNet.SendChatString(to, line) (OBJLIB 7: through its ref, the seam: fc-chat records it) */
static byte Chat_SendNet(long to, const char *line) {
	return PORT_CALL(SNet, SendChatString, NULL, "is", to, line);
}

/* SScorer.AddChatMsg(text, 0, 255, 0) (OBJLIB 7: through its ref, fc-chat records it) */
static byte Chat_ChatMsg(const char *text) {
	return PORT_CALL(SScorer, AddChatMsg, NULL, "siii", text, 0L, 255L, 0L);
}

/* SNet.GetMyClientShip(): the ship name on a client, else 0 (OBJLIB 7: through its ref, fc-chat swaps it); what a
 * swapped function gives must be a value MyShip takes (None, a number, a short text without 0 bytes) */
static byte Chat_ClientShip(nativeVal_t *out) {
	nativeVal_t r;
	if (!PORT_CALL(SNet, GetMyClientShip, &r, "")) {
		return 0;
	}
	if (!(CHAT_MYSHIP->types & 1 << r.type) || (r.type == NV_STR && (r.n >= NATIVE_TEXT || strlen(Native_Text(&r)) != (size_t)r.n))) {
		Native_Free(&r);
		return Native_Raise(NE_TYPE, "SNet.GetMyClientShip: not a value Chat.MyShip takes");
	}
	*out = r;
	return 1;
}

/* GetID(id): the receiver as the chat line shows it; a receiver that is no longer a ship becomes "ALL" */
static byte Chat_IdText(int slot, nativeVal_t *out) {
	for (;;) {
		const nativeVal_t *to = &chat.sendID[slot];
		if (Native_IsStr(to, "ALL")) {
			Native_SetStr(out, "[To All]");
			return 1;
		}
		cEntity *e = World_FindEntity(to->s);
		if (e && Entity_IsType(e, "Car")) {
			return Native_EntVar(e, to->s, "Descriptor", out);
		}
		if (!Chat_SetTo(slot, "ALL")) {
			return 0;
		}
	}
}

/* CycleUp, CycleDown: the next ship net<n> up or down that exists and is not mine, "ALL" past the ends (n 0 to
 * 257) */
static byte Chat_Cycle(int slot, int step) {
	for (;;) {
		const nativeVal_t *to = &chat.sendID[slot];
		long ctr = step > 0 ? -1 : 257;
		/* int(SendID[id][3:]) */
		if (!Native_IsStr(to, "ALL") && !Native_IntOfText(strlen(to->s) > 3 ? to->s + 3 : "", &ctr)) {
			return 0;
		}
		ctr += step;
		if (step > 0 ? ctr > 257 : ctr < 0) {
			return Chat_SetTo(slot, "ALL");
		}
		char name[0x20];
		Str_Printf(name, sizeof(name), "net%ld", ctr);
		if (!Chat_SetTo(slot, name)) {
			return 0;
		}
		if (World_FindEntity(name) && !Native_Equal(&chat.sendID[slot], &chat.myShip)) {
			return 1;
		}
	}
}

/* SScorer.Set(id, "SendID", "Text", t) with the receiver text (a number goes in as a number) */
static byte Chat_ShowTo(int id, const nativeVal_t *t) {
	switch (t->type) {
	case NV_STR:
		Script_SetStr(id, "SendID", "Text", t->s);
		return 1;
	case NV_INT:
		Script_SetNum(id, "SendID", "Text", (double)t->i);
		return 1;
	case NV_FLOAT:
		Script_SetNum(id, "SendID", "Text", t->f);
		return 1;
	}
	/* the error of the engine function for any other value (a Descriptor that is None or a vector) */
	return Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
}

/* ServerReSendChat(sendto, cad): the server sends a line on, to all or to the client of the ship net<n> (the script
 * tests the number with "or", so every number passes); cad NULL is a value that is no text */
static byte Chat_ReSend(const char *sendto, const char *cad) {
	char line[CHAT_LINE];
	if (!Mode_Import("string")) {
		return 0;
	}
	if (!strcmp(sendto, "ALL")) {
		return Chat_Cat(line, sendto, "\t", cad) && Chat_SendNet(-1, line);
	}
	if (strncmp(sendto, "net", 3)) {
		return 1;
	}
	long val;
	return Native_IntOfText(sendto + 3, &val) && Chat_Cat(line, sendto, "\t", cad) && Chat_SendNet(val, line);
}

/* PrintChatString(id, Str): a line "<receiver>\t<text>" arrives: the server sends it on; the text goes to the chat
 * and the console (a line without a tab is dropped) */
static byte Chat_Print(const char *str) {
	if (!Mode_Import("string")) {
		return 0;
	}
	/* cadinfo[0] and cadinfo[1] of string.split(Str, "\t") */
	const char *tab = strchr(str, '\t');
	if (!tab) {
		return 1;
	}
	const char *end = strchr(tab + 1, '\t');
	char to[CHAT_LINE], text[CHAT_LINE], line[CHAT_LINE];
	Str_Copy(to, str, (int)(tab - str) + 1 < CHAT_LINE ? (int)(tab - str) + 1 : CHAT_LINE);
	Str_Copy(text, tab + 1, end && (int)(end - tab) < CHAT_LINE ? (int)(end - tab) : CHAT_LINE);
	int server = Native_NetFlag(1);
	if (server < 0 || (server && !Chat_ReSend(to, text)) || !Chat_ChatMsg(text)) {
		return 0;
	}
	/* Scrap.Print(text + "\n") */
	Str_Printf(line, sizeof(line), "%s\n", text);
	Con_Print(0, line, 0);
	return 1;
}

/* EndChat(id): the chat line closes, back to the game */
static byte Chat_End(int id) {
	if (!Mode_Import("Menu")) {
		return 0;
	}
	/* SInput.Bind x3, SInput.SetInputFunc(id, ""), SScorer.SetOnCancel(id, "") */
	Input_Bind(id, "Menu", "Cancel", "");
	Input_Bind(id, "Menu", "PgUp", "");
	Input_Bind(id, "Menu", "PgDown", "");
	if ((unsigned)id < MAX_LOCAL_PLAYERS) {
		cPlayerInput_SetInputFunc(&g_PlayerInput[id], "");
	}
	cScorer *sc = Scorer_Get(id);
	if (sc) {
		cPy_Function_SetFunc(&sc->onCancel, "");
	}
	/* through the module (directly while it holds the port's function) */
	return PORT_CALL(Menu, BackToGame, NULL, "is", (long)id, "");
}

/* SendChatString(id, ChatStr, console=0): "<player> text" to the receiver of the player; the server shows it at
 * once, a client sends it (and shows a private line itself); the chat line closes unless it came from the console.
 * text NULL is a value that is no text. */
static byte Chat_Send(int id, const char *text, int console) {
	int slot;
	char named[CHAT_LINE], str[CHAT_LINE], full[CHAT_LINE];
	if (!Chat_Cat(named, "<", GVar_GetStr("PlayerName"), "> ") || !Chat_Cat(str, named, "", text) ||
		!Native_Index(id, CHAT_PLAYERS, &slot) || !Chat_Cat(full, chat.sendID[slot].s, "\t", str)) {
		return 0;
	}
	int server = Native_NetFlag(1);
	byte ok = server >= 0;
	if (ok && server) {
		ok = Chat_Print(full);
	} else if (ok) {
		ok = (Native_IsStr(&chat.sendID[slot], "ALL") || Chat_Print(full)) && Chat_SendNet(-1, full);
	}
	return ok && (console || Chat_End(id));
}

/* MyShip into the module, as the Script_SetVar of the script port: a failed set is no error */
static void Chat_PutShip(void) {
	if (!Native_Put("Chat", CHAT_MYSHIP, 0)) {
		Mode_DropError();
	}
}

/* GetMyShip(): the ship of this machine (a client asks the network, the server has net256 when it plays) */
static byte Chat_MyShip(void) {
	int client = Native_NetFlag(0);
	if (client < 0) {
		return 0;
	}
	if (client) {
		nativeVal_t ship;
		if (!Chat_ClientShip(&ship)) {
			return 0;
		}
		chat.myShip = ship;
	} else if (World_FindEntity("net256")) {
		Native_SetStr(&chat.myShip, "net256");
	} else {
		/* the script sets "net256" first, then "" */
		Native_SetStr(&chat.myShip, "net256");
		Chat_PutShip();
		Native_SetStr(&chat.myShip, "");
	}
	Chat_PutShip();
	return 1;
}

/* InitChat(id): the chat key opens the chat line */
static byte Chat_Init(int id) {
	int slot;
	nativeVal_t t;
	if (!Mode_Import("Menu") || !Chat_MyShip() || !Native_Index(id, CHAT_PLAYERS, &slot) || !Chat_IdText(slot, &t)) {
		return 0;
	}
	/* OBJLIB 6: the $names go to the .def runner as a dict (Mode_DefRun) */
	return Mode_DefRun(id, "menus/chat.def", "InitChat", chat_def, NULL, 0, 1, "sendid", &t);
}

/* ---- the functions of the module table: the entries parse their arguments with the formats of the port (Mode_Parse:
 * the errors of PyArg_ParseTuple), the adapters are MODE_ADAPTER ---- */

/* the text of an argument, NULL when it is no text */
static const char *Chat_Text(const nativeVal_t *v) {
	return v->type == NV_STR ? Native_Text(v) : NULL;
}

static byte Chat_GetID_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id, slot;
	return Mode_Parse(a, n, "i", &id) && Native_Import("Chat", chat_sendIDOnly) && Native_Index(id, CHAT_PLAYERS, &slot) &&
		Chat_IdText(slot, out);
}

MODE_ADAPTER(Chat, GetID)

/* CycleUp(AndSet), CycleDown(AndSet): the cycle and, with set, the receiver on the chat line */
static byte Chat_CycleSet(const nativeVal_t *a, int n, int step, byte set) {
	int id, slot;
	nativeVal_t t;
	if (!Mode_Parse(a, n, "i", &id) || !Native_Import("Chat", chat_vars) || !Native_Index(id, CHAT_PLAYERS, &slot) ||
		!Chat_Cycle(slot, step)) {
		return 0;
	}
	return !set || (Chat_IdText(slot, &t) && Chat_ShowTo(id, &t));
}

#define CHAT_CYCLE(name, step, set) \
	static byte Chat_##name##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		(void)out; \
		return Chat_CycleSet(a, n, step, set); \
	} \
	MODE_ADAPTER(Chat, name)

CHAT_CYCLE(CycleUp, 1, 0)
CHAT_CYCLE(CycleUpAndSet, 1, 1)
CHAT_CYCLE(CycleDown, -1, 0)
CHAT_CYCLE(CycleDownAndSet, -1, 1)

static byte Chat_ReSendArgs(const nativeVal_t *sendto, const nativeVal_t *cad) {
	if (!Chat_Text(sendto)) {
		/* sendto[0:3] == "net" of the script: another sequence is not "net", anything else cannot be sliced */
		return Mode_Import("string") && (Mode_Sliceable(sendto) || Native_Raise(NE_TYPE, "unsliceable object"));
	}
	return Chat_ReSend(Chat_Text(sendto), Chat_Text(cad));
}

MODE_FN(Chat, ServerReSendChat, 2, Chat_ReSendArgs(&a[0], &a[1]))

static byte Chat_PrintArg(const nativeVal_t *str) {
	if (!Chat_Text(str)) {
		/* the error of string.split on a value that is no text, after the import */
		return Mode_Import("string") && Native_Raise(NE_TYPE, "argument 1: expected read-only character buffer, %.50s found",
			Native_TypeName(str));
	}
	return Chat_Print(Chat_Text(str));
}

MODE_FN(Chat, PrintChatString, 2, Chat_PrintArg(&a[1]))

static byte Chat_SendChatString_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	const nativeVal_t *text, *console = NULL;
	(void)out;
	if (!Mode_Parse(a, n, "iO|O", &id, &text, &console)) {
		return 0;
	}
	int c = console ? Mode_Truth(console) : 0;
	return c >= 0 && Native_Import("Chat", chat_sendIDOnly) && Chat_Send(id, Chat_Text(text), c);
}

MODE_ADAPTER(Chat, SendChatString)

static byte Chat_EndChat_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	return Mode_Parse(a, n, "i", &id) && Chat_End(id);
}

MODE_ADAPTER(Chat, EndChat)
MODE_FN(Chat, GetMyShip, 0, Chat_MyShip())

static byte Chat_InitChat_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	return Mode_Parse(a, n, "i", &id) && Native_Import("Chat", chat_sendIDOnly) && Chat_Init(id);
}

MODE_ADAPTER(Chat, InitChat)

/* ConsoleChatString(estrin): a line of the full screen console goes to the receiver of player 0 */
MODE_FN(Chat, ConsoleChatString, 1, Native_Import("Chat", chat_sendIDOnly) && Chat_Send(0, Chat_Text(&a[0]), 1))

/* qchat1 .. qchat6(id): the quick message QuickMessageString<n> */
static byte Chat_Quick(const nativeVal_t *a, int n, int num) {
	int id;
	char var[0x20];
	if (!Mode_Parse(a, n, "i", &id)) {
		return 0;
	}
	Str_Printf(var, sizeof(var), "QuickMessageString%d", num);
	return Native_Import("Chat", chat_sendIDOnly) && Chat_Send(id, GVar_GetStr(var), 1);
}

#define CHAT_QUICK(num) \
	static byte Chat_qchat##num##_E(const nativeVal_t *a, int n, nativeVal_t *out) { \
		(void)out; \
		return Chat_Quick(a, n, num); \
	} \
	MODE_ADAPTER(Chat, qchat##num)

CHAT_QUICK(1)
CHAT_QUICK(2)
CHAT_QUICK(3)
CHAT_QUICK(4)
CHAT_QUICK(5)
CHAT_QUICK(6)

/* SetChatProc(): the callbacks of the engine */
static void Chat_SetProcs(void) {
	GVar_SetStr("ChatEvent", "Chat.InitChat");
	GVar_SetStr("OnChatString", "Chat.PrintChatString");
	GVar_SetStr("FullScreenConsoleCallback", "Chat.ConsoleChatString");
	GVar_SetStr("QChatEvent1", "Chat.qchat1");
	GVar_SetStr("QChatEvent2", "Chat.qchat2");
	GVar_SetStr("QChatEvent3", "Chat.qchat3");
	GVar_SetStr("QChatEvent4", "Chat.qchat4");
	GVar_SetStr("QChatEvent5", "Chat.qchat5");
	GVar_SetStr("QChatEvent6", "Chat.qchat6");
}

MODE_FN(Chat, SetChatProc, 0, (Chat_SetProcs(), 1))

PyMethodDef g_ChatMethods[] = {
	{ "GetID", Chat_GetID, 1, NULL },
	{ "CycleUp", Chat_CycleUp, 1, NULL },
	{ "CycleUpAndSet", Chat_CycleUpAndSet, 1, NULL },
	{ "CycleDown", Chat_CycleDown, 1, NULL },
	{ "CycleDownAndSet", Chat_CycleDownAndSet, 1, NULL },
	{ "ServerReSendChat", Chat_ServerReSendChat, 1, NULL },
	{ "PrintChatString", Chat_PrintChatString, 1, NULL },
	{ "SendChatString", Chat_SendChatString, 1, NULL },
	{ "EndChat", Chat_EndChat, 1, NULL },
	{ "GetMyShip", Chat_GetMyShip, 1, NULL },
	{ "InitChat", Chat_InitChat, 1, NULL },
	{ "ConsoleChatString", Chat_ConsoleChatString, 1, NULL },
	{ "qchat1", Chat_qchat1, 1, NULL },
	{ "qchat2", Chat_qchat2, 1, NULL },
	{ "qchat3", Chat_qchat3, 1, NULL },
	{ "qchat4", Chat_qchat4, 1, NULL },
	{ "qchat5", Chat_qchat5, 1, NULL },
	{ "qchat6", Chat_qchat6, 1, NULL },
	{ "SetChatProc", Chat_SetChatProc, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(Chat)
