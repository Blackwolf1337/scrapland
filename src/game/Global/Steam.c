/*
 * The Steam parts of the game (rva 0x134260..0x1347e0, after Global/String.cpp; no CHECK string names the source):
 * the steam_api context with the interfaces, the auth ticket and the Steam ID for Scrap.GetSteamTicket and
 * Scrap.GetSteamID. Also the Steam layer of the GOG build that does nothing (rva 0x1a64d0..0x1a66f0): the stubs the
 * game calls, and the two with arguments.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/crt.h"
#include "scrap/scrap.h"

/* the steam_api imports of the exe (cdecl) */
typedef int (*SteamHandle_t)(void);
typedef void *(*SteamCreateInterface_t)(const char *version);
#define g_SteamAPI_GetHSteamUser GAME_DATA(SteamHandle_t, 0x3a159c)
#define g_SteamInternal_CreateInterface GAME_DATA(SteamCreateInterface_t, 0x3a15a8)
#define g_SteamAPI_GetHSteamPipe GAME_DATA(SteamHandle_t, 0x3a15b0)

/* the auth ticket: requested once, then hex in g_SteamTicket when the callback (rva 0x1347a0) has it */
#define g_SteamTicketCb ((cSteamTicketCallback *)Game_Ptr(0x428a90))
#define g_SteamTicket ((char *)Game_Ptr(0x44b748))	/* [0x400] */
#define g_SteamIdStr ((char *)Game_Ptr(0x44bb48))
#define g_HexDigits ((const char *)Game_Ptr(0x3bb788))	/* "0123456789abcdef" */
#define STEAM_EMPTY ((const char *)Game_Ptr(0x3a3228))	/* "" */

#define STEAM_TICKET_SIZE 0x400

/* a C++ interface of steam_api: thiscall methods in its vtable */
typedef struct SteamIface {
	void **vftable;
} SteamIface;

/* CSteamAPIContext of the Steam SDK */
typedef struct cSteamAPIContext {
	SteamIface *client;		/* 0x00 */
	SteamIface *user;		/* 0x04 */
	SteamIface *friends;		/* 0x08 */
	SteamIface *utils;		/* 0x0c */
	SteamIface *matchmaking;	/* 0x10 */
	SteamIface *userStats;		/* 0x14 */
	SteamIface *apps;		/* 0x18 */
	SteamIface *matchmakingServers;	/* 0x1c */
	SteamIface *networking;		/* 0x20 */
	SteamIface *remoteStorage;	/* 0x24 */
	SteamIface *screenshots;	/* 0x28 */
	SteamIface *http;		/* 0x2c */
	SteamIface *unifiedMessages;	/* 0x30 */
	SteamIface *controller;		/* 0x34 */
	SteamIface *ugc;		/* 0x38 */
	SteamIface *appList;		/* 0x3c */
	SteamIface *music;		/* 0x40 */
	SteamIface *musicRemote;	/* 0x44 */
	SteamIface *htmlSurface;	/* 0x48 */
	SteamIface *inventory;		/* 0x4c */
	SteamIface *video;		/* 0x50 */
	SteamIface *parentalSettings;	/* 0x54 */
} cSteamAPIContext;
SD_SIZE(cSteamAPIContext, 0x58);

/* CCallback<cGetAuthSessionTicketResponseExt, GetAuthSessionTicketResponse_t> with the object after it */
typedef struct cSteamTicketCallback {
	const void *vftable;		/* 0x00 */
	byte flags;			/* 0x04 */
	byte pad_05[3];
	int callback;			/* 0x08 the callback id */
	void *obj;			/* 0x0c */
	void(__thiscall *func)(void *obj, void *param);	/* 0x10 */
	uint32_t ticket;		/* 0x14 HAuthTicket of the request */
	byte ready;			/* 0x18 the hex is in g_SteamTicket */
	byte pending;			/* 0x19 */
	byte pad_1a[2];
} cSteamTicketCallback;
SD_OFFSET(cSteamTicketCallback, ticket, 0x14);

/* GetAuthSessionTicketResponse_t */
typedef struct SteamTicketResponse {
	uint32_t ticket;		/* 0x0 */
	int result;			/* 0x4 EResult, 1 OK */
} SteamTicketResponse;

/* CSteamID */
typedef struct SteamID {
	uint32_t account;		/* 0x0 */
	uint32_t bits;			/* 0x4 instance 0..19, account type 20..23, universe 24..31 */
} SteamID;

/* CCallbackImpl<8>::Run(param, bIOFailure, hSteamAPICall): the plain Run of vtable slot 1 */
void __thiscall CCallbackImpl_RunCall(SteamIface *self, void *param, int ioFailure, uint32_t callLo, uint32_t callHi) {
	(void)ioFailure;
	(void)callLo;
	(void)callHi;
	((void(__thiscall *)(SteamIface *, void *))self->vftable[1])(self, param);
}

/* CCallbackImpl<8>::GetCallbackSizeBytes */
int __thiscall CCallbackImpl_GetSize(SteamIface *self) {
	(void)self;
	return 8;
}

/* CCallback::Run: the member function on the object */
void __thiscall CCallback_Run(cSteamTicketCallback *self, void *param) {
	self->func(self->obj, param);
}

/* CSteamAPIContext::Clear */
void __fastcall SteamContext_Clear(cSteamAPIContext *c) {
	c->client = NULL;
	c->user = NULL;
	c->friends = NULL;
	c->utils = NULL;
	c->matchmaking = NULL;
	c->userStats = NULL;
	c->apps = NULL;
	c->matchmakingServers = NULL;
	c->networking = NULL;
	c->remoteStorage = NULL;
	c->http = NULL;
	c->screenshots = NULL;
	c->unifiedMessages = NULL;
	c->controller = NULL;
	c->ugc = NULL;
	c->appList = NULL;
	c->music = NULL;
	c->musicRemote = NULL;
	c->htmlSurface = NULL;
	c->inventory = NULL;
	c->video = NULL;
	c->parentalSettings = NULL;
}

/* the interfaces after the client, in the order the original asks for them; pipe only for the utils */
static const struct {
	uint16_t field;			/* offset in cSteamAPIContext */
	uint16_t slot;			/* byte offset in the ISteamClient vtable */
	const char *version;
} g_SteamIfaces[] = {
	{ 0x04, 0x14, "SteamUser019" },
	{ 0x08, 0x20, "SteamFriends015" },
	{ 0x0c, 0x24, "SteamUtils009" },
	{ 0x10, 0x28, "SteamMatchMaking009" },
	{ 0x1c, 0x2c, "SteamMatchMakingServers002" },
	{ 0x14, 0x34, "STEAMUSERSTATS_INTERFACE_VERSION011" },
	{ 0x18, 0x3c, "STEAMAPPS_INTERFACE_VERSION008" },
	{ 0x20, 0x40, "SteamNetworking005" },
	{ 0x24, 0x44, "STEAMREMOTESTORAGE_INTERFACE_VERSION014" },
	{ 0x28, 0x48, "STEAMSCREENSHOTS_INTERFACE_VERSION003" },
	{ 0x2c, 0x5c, "STEAMHTTP_INTERFACE_VERSION002" },
	{ 0x30, 0x60, "STEAMUNIFIEDMESSAGES_INTERFACE_VERSION001" },
	{ 0x34, 0x64, "SteamController005" },
	{ 0x38, 0x68, "STEAMUGC_INTERFACE_VERSION010" },
	{ 0x3c, 0x6c, "STEAMAPPLIST_INTERFACE_VERSION001" },
	{ 0x40, 0x70, "STEAMMUSIC_INTERFACE_VERSION001" },
	{ 0x44, 0x74, "STEAMMUSICREMOTE_INTERFACE_VERSION001" },
	{ 0x48, 0x78, "STEAMHTMLSURFACE_INTERFACE_VERSION_004" },
	{ 0x4c, 0x88, "STEAMINVENTORY_INTERFACE_V002" },
	{ 0x50, 0x8c, "STEAMVIDEO_INTERFACE_V002" },
	{ 0x54, 0x90, "STEAMPARENTALSETTINGS_INTERFACE_VERSION001" },
};

/* CSteamAPIContext::Init: 0 as soon as one interface is missing */
byte __thiscall SteamContext_Init(cSteamAPIContext *c) {
	int user = g_SteamAPI_GetHSteamUser();
	int pipe = g_SteamAPI_GetHSteamPipe();

	if (!pipe) {
		return 0;
	}
	c->client = g_SteamInternal_CreateInterface("SteamClient017");
	if (!c->client) {
		return 0;
	}
	for (size_t i = 0; i < sizeof(g_SteamIfaces) / sizeof(g_SteamIfaces[0]); i++) {
		void *f = c->client->vftable[g_SteamIfaces[i].slot / 4];
		void *p;
		if (g_SteamIfaces[i].field == 0x0c) {
			p = ((void *(__thiscall *)(SteamIface *, int, const char *))f)(c->client, pipe, g_SteamIfaces[i].version);
		} else {
			p = ((void *(__thiscall *)(SteamIface *, int, int, const char *))f)(c->client, user, pipe,
				g_SteamIfaces[i].version);
		}
		*(void **)((byte *)c + g_SteamIfaces[i].field) = p;
		if (!p) {
			return 0;
		}
	}
	return 1;
}

/* CSteamID::IsValid */
byte __thiscall SteamID_IsValid(const SteamID *id) {
	uint32_t type = id->bits & 0xf00000;

	if (type == 0 || type >= 0xb00000) {
		return 0;
	}
	int universe = (int)id->bits >> 24;
	if (universe <= 0 || universe >= 5) {
		return 0;
	}
	if (type == 0x100000 && (!id->account || (id->bits & 0xfffff) > 4)) {
		return 0;
	}
	if (type == 0x700000 && (!id->account || (id->bits & 0xfffff))) {
		return 0;
	}
	if (type == 0x300000 && !id->account) {
		return 0;
	}
	return 1;
}

/* the first call asks for a ticket and returns "", a call after the callback came returns the ticket in hex */
const char *Steam_GetTicket(void) {
	byte buf[STEAM_TICKET_SIZE];
	uint32_t len;

	if (g_SteamTicketCb->pending) {
		return STEAM_EMPTY;
	}
	if (g_SteamTicketCb->ready) {
		g_SteamTicketCb->ready = 0;
		return g_SteamTicket;
	}
	memset(buf, 0, sizeof(buf));
	len = 0;
	cSteamAPIContext *c = g_SteamInternal_ContextInit(g_SteamFriendsContext);
	uint32_t t = ((uint32_t(__thiscall *)(SteamIface *, void *, int, uint32_t *))c->user->vftable[0x34 / 4])(c->user,
		buf, sizeof(buf), &len);
	if (t && len) {
		g_SteamTicketCb->ticket = t;
		g_SteamTicketCb->ready = 0;
		g_SteamTicketCb->pending = 1;
		for (uint32_t i = 0; i < len; i++) {
			g_SteamTicket[i * 2] = g_HexDigits[buf[i] >> 4];
			g_SteamTicket[i * 2 + 1] = g_HexDigits[buf[i] & 0xf];
		}
		if (len * 2 >= STEAM_TICKET_SIZE) {
			crt_report_rangecheckfailure();
		}
		g_SteamTicket[len * 2] = 0;
	}
	return STEAM_EMPTY;
}

/* the 64 bit Steam ID in decimal, "" without Steam */
const char *Steam_GetID(void) {
	if (g_SteamIdStr[0]) {
		return g_SteamIdStr;
	}
	cSteamAPIContext *c = g_SteamInternal_ContextInit(g_SteamFriendsContext);
	if (!c->user) {
		return g_SteamIdStr;
	}
	SteamID id;
	c = g_SteamInternal_ContextInit(g_SteamFriendsContext);
	((void(__thiscall *)(SteamIface *, SteamID *))c->user->vftable[0x8 / 4])(c->user, &id);
	if (SteamID_IsValid(&id)) {
		crt_ui64toa(((uint64_t)id.bits << 32) | id.account, g_SteamIdStr, 10);
		return g_SteamIdStr;
	}
	g_SteamIdStr[0] = 0;
	return g_SteamIdStr;
}

/* cGetAuthSessionTicketResponseExt: the ticket of the request is ready */
void __thiscall SteamTicket_OnResponse(cSteamTicketCallback *self, const SteamTicketResponse *r) {
	self->pending = 0;
	if (r->result == 1 && r->ticket == self->ticket) {
		self->ready = 1;
	}
}

/* the init function of the context data (SteamInternal_ContextInit calls it once) */
byte SteamContext_Setup(cSteamAPIContext *c) {
	SteamContext_Clear(c);
	if (!g_SteamAPI_GetHSteamPipe()) {
		return 0;
	}
	return SteamContext_Init(c);
}

/* ---- the stubs (rva 0x1a64d0..0x1a66f0) ---- */

void Steam_Done(void) {
}

/* NULL: the language comes from GOG */
const char *Steam_GetLanguage(void) {
	return NULL;
}

/* "started": the caller then does not quit to let Steam restart the game */
byte Steam_Init(int arg) {
	(void)arg;
	return 1;
}

byte Steam_SetAchievement(int n, int on) {
	(void)n;
	(void)on;
	return 0;
}

void Steam_RunCallbacks(void) {
}

void SteamStub_Clear3(uint32_t *a, uint32_t *b, uint32_t *c) {
	a[0] = 0;
	a[1] = 0;
	b[0] = 0;
	b[1] = 0;
	c[0] = 0;
}

int SteamStub_Clear1(uint32_t *a) {
	a[0] = 0;
	return 0;
}
