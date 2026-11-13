/*
 * The instance's side of a coordinator: its connection, the tickets of the players, their states and the travel
 * between the instances, see include/scrap/travel.h and include/scrap/coord.h. Not in the original.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "scrap/alarm.h"
#include "scrap/arrowindicator.h"
#include "scrap/console.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/netrep.h"
#include "scrap/platform.h"
#include "scrap/players.h"
#include "scrap/play.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/travel.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

#define TR_MSG 512
#define TR_CHANNEL 0				/* REP_CH_RELIABLE, also the coordinator's */
#define TR_STATIONS 16
#define TR_DOOR_NEAR 3000.0f		/* units from the entrance to its door */

typedef struct TrPlayer {
	byte used;
	byte traveled;					/* handed to the coordinator: no leave report */
	byte quiet;						/* replaced by a newer connection of its key: no leave report */
	uint8_t key[NET2_KEY_SIZE];
	char hex[2 * NET2_KEY_SIZE + 1];
	char entrance[COORD_LEVEL + 1], door[COORD_LEVEL + 1];
	char metroDoor[COORD_LEVEL + 1];	/* the metro door whose map its client was sent */
	uint64_t lastReport, travelAt;	/* ms */
	int len;
	char state[COORD_STATE_MAX];
} TrPlayer;

typedef struct TrPending {
	byte used;
	int conn;
	uint64_t at;
	CoordTicket t;
} TrPending;

static struct {
	byte coordinated;
	char addr[48];
	uint16_t port;
	uint8_t key[NET2_KEY_SIZE];
	uint8_t token[32];
	byte haveToken;
	Net2Host *host;
	byte up;						/* the coordinator's connection is up */
	uint64_t lastTry, lastUp, now;
	TrPlayer pl[PLAYERS_MAX];
	TrPending pend[TRAVEL_PENDING];
	struct {
		uint8_t nonce[COORD_NONCE];
		uint64_t expires;
	} used[TRAVEL_NONCES];
	int nused;
} tr;

byte Travel_ParseParam(const char *param, const char *ext) {
	if (!crt_stricmp("coord", param)) {
		Str_Copy(tr.addr, ext, sizeof(tr.addr));
		tr.coordinated = 1;
		return 1;
	}
	if (!crt_stricmp("coordport", param)) {
		int p = crt_atoi(ext);
		tr.port = (uint16_t)(p > 0 && p < 65536 ? p : 0);
		return 1;
	}
	if (!crt_stricmp("coordkey", param)) {
		if (!Coord_Unhex(ext, tr.key, NET2_KEY_SIZE)) {
			Con_Errorf("instance: -coordkey needs %d hex digits\n", 2 * NET2_KEY_SIZE);
		}
		return 1;
	}
	return 0;
}

byte Travel_Coordinated(void) {
	return g_Instance && tr.coordinated;
}


static int Tr_PlayerOfKey(const uint8_t key[NET2_KEY_SIZE]) {
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (tr.pl[i].used && !memcmp(tr.pl[i].key, key, NET2_KEY_SIZE)) {
			return i;
		}
	}
	return -1;
}

/* the state of player i as it is now: its text with the game's values set */
static void Tr_State(int i) {
	char v[64];
	TrPlayer *p = &tr.pl[i];
	cEntity *e = Players_Get(i);

	Str_PrintfQuiet(v, sizeof(v), "%d", Players_Money(i));
	CoordKv_Set(p->state, &p->len, sizeof(p->state), "money", v);
	Str_PrintfQuiet(v, sizeof(v), "%.3f", (double)Players_Alarm(i)->value);
	CoordKv_Set(p->state, &p->len, sizeof(p->state), "alarm", v);
	if (e && NetPl_Kind(e) == NETREP_CHAR && WLife(e)->actCtrl && WLife(e)->actCtrl->cls) {
		CoordKv_Set(p->state, &p->len, sizeof(p->state), "char", WLife(e)->actCtrl->cls->name);
	}
	if (e && NetPl_Kind(e) == NETREP_SHIP) {
		Travel_Boarded(i, e);
	}
}

/* ---- the coordinator's connection ---- */

static void Tr_SendState(int i, int reason, const char *level, const char *entrance, const char *door) {
	static uint8_t buf[COORD_STATE_MAX + 256];
	NetRepBuf w;
	TrPlayer *p = &tr.pl[i];

	if (!tr.up) {
		return;
	}
	Tr_State(i);
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_STATE);
	NetRep_PutU8(&w, (uint32_t)reason);
	CoordMsg_PutBytes(&w, p->key, NET2_KEY_SIZE);
	NetRep_PutStr(&w, level);
	NetRep_PutStr(&w, entrance);
	NetRep_PutStr(&w, door);
	NetRep_PutU16(&w, (uint32_t)p->len);
	CoordMsg_PutBytes(&w, p->state, p->len);
	if (!w.bad) {
		Net2_Send(tr.host, 0, TR_CHANNEL, buf, w.size);
	}
	p->lastReport = tr.now;
}

static void Tr_Open(void) {
	Net2Config cfg;
	Net2Addr addr;
	int err = 0;

	tr.lastTry = tr.now;
	if (!tr.host) {
		Net2_DefaultConfig(&cfg, 0);
		cfg.appVersion = 1;
		cfg.ipv6 = 0;
		cfg.serverKey = tr.key;
		tr.host = Net2_Create(&cfg, &err);
		if (!tr.host) {
			Con_Errorf("instance: no Net2 client for the coordinator (error %d)\n", err);
			return;
		}
	}
	if (!Net2_ParseAddr(tr.addr, tr.port, &addr) || Net2_Connect(tr.host, &addr, tr.now) != NET2_OK) {
		Con_Errorf("instance: cannot connect to the coordinator %s port %u\n", tr.addr, tr.port);
	}
}

static void Tr_Hello(void) {
	uint8_t buf[TR_MSG];
	NetRepBuf w;

	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_INST);
	NetRep_PutU16(&w, COORD_VERSION);
	CoordMsg_PutBytes(&w, tr.token, sizeof(tr.token));
	NetRep_PutStr(&w, Instance_Level());
	NetRep_PutStr(&w, Instance_Level());
	NetRep_PutU16(&w, Instance_Port());
#ifdef _WIN32
	NetRep_PutU32(&w, (uint32_t)_getpid());
#else
	NetRep_PutU32(&w, (uint32_t)getpid());
#endif
	Net2_Send(tr.host, 0, TR_CHANNEL, buf, w.size);
	/* a coordinator that came back learns where the players are */
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (tr.pl[i].used && !tr.pl[i].traveled) {
			Tr_SendState(i, COORD_SR_REPORT, "", "", "");
		}
	}
}

static void Tr_CoordMessage(const uint8_t *data, int size) {
	uint8_t key[NET2_KEY_SIZE];
	char text[256];
	NetRepReader r;

	NetRepReader_Init(&r, data + 1, size - 1);
	if (data[0] == COORD_MSG_CONSOLE) {
		NetRep_GetStr(&r, text, sizeof(text));
		if (!r.bad) {
			Instance_Line(text);
		}
		return;
	}
	CoordMsg_GetBytes(&r, key, NET2_KEY_SIZE);
	int i = r.bad ? -1 : Tr_PlayerOfKey(key);
	if (i < 0) {
		return;
	}
	if (data[0] == COORD_MSG_KICK) {
		int reason = (int)NetRep_GetU8(&r);
		int conn = Rep_PlayerConn(i);
		Con_Printf("instance: player %d (%.8s) kicked by the coordinator\n", i, tr.pl[i].hex);
		tr.pl[i].quiet = 0;
		if (conn >= 0) {
			Net2_Disconnect(Instance_Host(), conn, NET2_DR_APP + reason);
		}
	} else if (data[0] == COORD_MSG_REFUSE) {
		NetRep_GetStr(&r, text, sizeof(text));
		Con_Printf("instance: the travel of player %d was refused (%s)\n", i, text);
		tr.pl[i].traveled = 0;
		Rep_Respawn(i);
	}
}

static void Tr_Link(void) {
	Net2Event ev;

	if (!tr.host) {
		return;
	}
	Net2_Poll(tr.host, tr.now);
	while (Net2_Receive(tr.host, &ev)) {
		switch (ev.type) {
		case NET2_EV_CONNECT:
			tr.up = 1;
			Con_Printf("instance: coordinator connected\n");
			Tr_Hello();
			break;
		case NET2_EV_DISCONNECT:
			if (tr.up) {
				Con_Printf("instance: coordinator lost (%s)\n", Net2_ReasonString(ev.reason));
			}
			tr.up = 0;
			tr.lastTry = tr.now;
			break;
		case NET2_EV_MESSAGE:
			if (ev.channel == TR_CHANNEL && ev.size > 1) {
				Tr_CoordMessage(ev.data, ev.size);
			}
			break;
		}
	}
	if (!tr.up && tr.now - tr.lastTry >= TRAVEL_RETRY_MS) {
		Tr_Open();
	}
	Net2_Flush(tr.host, tr.now);
}

void Travel_InstancePass(uint64_t ms) {
	if (!Travel_Coordinated()) {
		return;
	}
	tr.now = ms;
	if (!tr.lastUp) {
		tr.lastUp = ms;
		const char *t = getenv(COORD_TOKEN_ENV);
		tr.haveToken = t && Coord_Unhex(t, tr.token, sizeof(tr.token));
		if (!tr.haveToken) {
			Con_Errorf("instance: no token of the coordinator in %s\n", COORD_TOKEN_ENV);
		}
		Tr_Open();
	}
	Tr_Link();
	if (tr.up) {
		tr.lastUp = ms;
	} else if (ms - tr.lastUp > COORD_ORPHAN_SECS * 1000u) {
		Con_Printf("instance: no coordinator for %d s, the end\n", COORD_ORPHAN_SECS);
		Plat_Quit();
		tr.lastUp = ms;
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		TrPlayer *p = &tr.pl[i];
		if (!p->used) {
			continue;
		}
		if (p->traveled && ms - p->travelAt > TRAVEL_GIVEBACK_SECS * 1000u) {
			Con_Printf("instance: the travel of player %d got no ticket, it stays\n", i);
			p->traveled = 0;
			Rep_Respawn(i);
		}
		if (!p->traveled && ms - p->lastReport >= COORD_REPORT_SECS * 1000u) {
			Tr_SendState(i, COORD_SR_REPORT, "", "", "");
		}
	}
	for (int k = 0; k < TRAVEL_PENDING; k++) {
		if (tr.pend[k].used && (ms - tr.pend[k].at > COORD_TICKET_LIFE || !Net2_IsConnected(Instance_Host(),
			tr.pend[k].conn))) {
			tr.pend[k].used = 0;
		}
	}
}

void Travel_InstanceClose(void) {
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (tr.pl[i].used && !tr.pl[i].traveled && !tr.pl[i].quiet) {
			Tr_SendState(i, COORD_SR_LEAVE, "", "", "");
		}
	}
	if (tr.host) {
		Net2_Flush(tr.host, tr.now);
		Net2_Destroy(tr.host);
		tr.host = NULL;
	}
}

/* ---- the players' tickets ---- */

/* the ticket of conn, NULL; conn -1: a free one */
static TrPending *Tr_Pending(int conn) {
	for (int k = 0; k < TRAVEL_PENDING; k++) {
		if (conn < 0 ? !tr.pend[k].used : tr.pend[k].used && tr.pend[k].conn == conn) {
			return &tr.pend[k];
		}
	}
	return NULL;
}

/* the nonce was not taken before (and is taken now) */
static byte Tr_TakeNonce(const CoordTicket *t, uint64_t now) {
	int free = -1;
	for (int k = 0; k < tr.nused; k++) {
		if (tr.used[k].expires && !memcmp(tr.used[k].nonce, t->nonce, COORD_NONCE)) {
			return 0;
		}
		if (free < 0 && tr.used[k].expires < now) {
			free = k;
		}
	}
	if (free < 0) {
		free = tr.nused < TRAVEL_NONCES ? tr.nused++ : (int)(now % TRAVEL_NONCES);
	}
	memcpy(tr.used[free].nonce, t->nonce, COORD_NONCE);
	tr.used[free].expires = t->expires;
	return 1;
}

static void Tr_Ticket(Net2Host *host, int conn, const uint8_t *data, int size) {
	static CoordTicket t;
	NetRepReader r;

	NetRepReader_Init(&r, data + 1, size - 1);
	int n = (int)NetRep_GetU16(&r);
	if (r.bad || n > size - 3) {
		Net2_Disconnect(host, conn, NET2_DR_APP + COORD_DENY_TICKET);
		return;
	}
	uint64_t now = Coord_UnixMs();
	int res = Coord_TicketOpen(data + 3, n, tr.key, now, &t);
	if (res == COORD_TK_OK && crt_stricmp(t.level, Instance_Level())) {
		res = COORD_TK_BAD;
	}
	if (res == COORD_TK_OK && !Tr_TakeNonce(&t, now)) {
		res = COORD_TK_EXPIRED;
	}
	TrPending *pd = res == COORD_TK_OK ? Tr_Pending(-1) : NULL;
	if (!pd) {
		Con_Printf("instance: connection %d gave %s ticket\n", conn, res == COORD_TK_OK ? "one too many a" :
			res == COORD_TK_SIGNATURE ? "a forged" : res == COORD_TK_EXPIRED ? "an old or used" : "a bad");
		Net2_Disconnect(host, conn, NET2_DR_APP + COORD_DENY_TICKET);
		return;
	}
	pd->used = 1;
	pd->conn = conn;
	pd->at = tr.now;
	pd->t = t;
}

void Travel_Drop(int conn) {
	TrPending *pd = Tr_Pending(conn);
	if (pd) {
		pd->used = 0;
	}
}

byte Travel_HelloOk(int conn) {
	return !Travel_Coordinated() || Tr_Pending(conn) != NULL;
}

/* the position of a dummy of the level: 1 found */
static byte Tr_Dummy(const char *name, vec3_t *pos, float *yaw) {
	char found[64];
	vec3_t ang;
	DummySearch_Start(name, 1);
	if (!DummySearch_NextEuler(found, sizeof(found), pos, &ang)) {
		return 0;
	}
	*yaw = ang.y + M_PI_F;
	return 1;
}

/* the door of an action class (an elevator, a metro station) nearest to pos within TR_DOOR_NEAR: where a player at
 * the spawn dummy of its entrance rides down (the level's own ElevatorStart is the single player's door); pos NULL:
 * the first one of the class */
static cEntity *Tr_NearestDoor(const char *cls, const vec3_t *pos) {
	cEntity *best = NULL;
	float bestD = TR_DOOR_NEAR * TR_DOOR_NEAR;
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		cActCtrl *a = Entity_IsType(e, TYEN_WITH_LIFE) ? WLife(e)->actCtrl : NULL;
		if (!a || !a->cls || strcmp(a->cls->name, cls)) {
			continue;
		}
		if (!pos) {
			return e;
		}
		float dx = e->pos.x - pos->x, dy = e->pos.y - pos->y, dz = e->pos.z - pos->z;
		float d = dx * dx + dy * dy + dz * dz;
		if (d < bestD) {
			bestD = d;
			best = e;
		}
	}
	return best;
}

byte Travel_Admit(int conn, int player, TravelSpawn *sp) {
	char v[64];

	memset(sp, 0, sizeof(*sp));
	sp->money = Instance_StartMoney();
	TrPlayer *p = &tr.pl[player];
	memset(p, 0, sizeof(*p));
	if (!Travel_Coordinated()) {
		return 1;
	}
	TrPending *pd = Tr_Pending(conn);
	if (!pd) {
		Net2_Disconnect(Instance_Host(), conn, NET2_DR_APP + COORD_DENY_TICKET);
		return 0;
	}
	CoordTicket *t = &pd->t;
	/* a newer connection of the same player: the old one goes without a report (the coordinator has the newer) */
	int old = Tr_PlayerOfKey(t->key);
	if (old >= 0 && old != player) {
		tr.pl[old].quiet = 1;
		if (Rep_PlayerConn(old) >= 0) {
			Net2_Disconnect(Instance_Host(), Rep_PlayerConn(old), NET2_DR_APP + COORD_DENY_PLAYING);
		}
	}
	p->used = 1;
	memcpy(p->key, t->key, NET2_KEY_SIZE);
	Coord_Hex(p->key, NET2_KEY_SIZE, p->hex);
	memcpy(p->state, t->state, (size_t)t->stateLen);
	p->len = t->stateLen;
	Str_Copy(p->entrance, t->entrance, sizeof(p->entrance));
	Str_Copy(p->door, t->door, sizeof(p->door));
	p->lastReport = tr.now;
	pd->used = 0;
	if (CoordKv_Get(p->state, p->len, "money", v, sizeof(v))) {
		sp->money = crt_atoi(v);
	}
	cEntity *door;
	if (p->entrance[0] && Tr_Dummy(p->entrance, &sp->pos, &sp->yaw)) {
		sp->have = 1;
	} else if (p->door[0] && (door = Tr_NearestDoor(p->door, NULL)) != NULL) {
		/* the level has no spawn dummy for that way in (a metro station): at the door of that kind */
		sp->pos = door->pos;
		sp->yaw = Obj(door)->ang.y;
		sp->have = 1;
	} else if (p->entrance[0]) {
		Con_Printf("instance: no entrance %s in %s, the players' spawn\n", p->entrance, Instance_Level());
	}
	CoordKv_Get(p->state, p->len, "name", v, sizeof(v));
	Con_Printf("instance: player %d is %s (%.8s), money %d%s%s\n", player, v[0] ? v : "?", p->hex, sp->money,
		p->entrance[0] ? ", entrance " : "", p->entrance);
	return 1;
}

void Travel_Joined(int player) {
	char v[64];
	TrPlayer *p = &tr.pl[player];
	cEntity *e = Players_Get(player);

	if (!p->used) {
		return;
	}
	if (CoordKv_Get(p->state, p->len, "alarm", v, sizeof(v))) {
		Players_Bind(player);
		Alarm_Set(&g_pWorld->alarm, (float)crt_atof(v));
		Players_BindTurn();
	}
	cEntity *door = e && p->door[0] ? Tr_NearestDoor(p->door, &e->pos) : NULL;
	if (door) {
		Players_Bind(player);
		byte ok = Elevator_Arrive(e->name, door->name);
		Players_BindTurn();
		if (!ok) {
			Native_ErrPrint();
		}
		Con_Printf("instance: player %d arrives in %s\n", player, door->name);
	}
	Tr_SendState(player, COORD_SR_JOIN, "", "", "");
}

void Travel_Left(int player) {
	TrPlayer *p = &tr.pl[player];
	if (p->used && !p->traveled && !p->quiet) {
		Tr_SendState(player, COORD_SR_LEAVE, "", "", "");
	}
	p->used = 0;
}

byte Travel_Request(const char *rider, const char *level, const char *entrance, const char *door) {
	cEntity *e = World_FindEntity(rider);
	int i = e ? Players_Of(e) : -1;
	if (i < 0 || !tr.pl[i].used) {
		Con_Printf("instance: level change of %s to %s refused (the coordinator owns the level changes)\n", rider,
			level);
		return 0;
	}
	TrPlayer *p = &tr.pl[i];
	if (p->traveled) {
		return 1;
	}
	if (!tr.up) {
		Con_Printf("instance: player %d cannot travel to %s now (no coordinator)\n", i, level);
		Rep_Respawn(i);
		return 1;
	}
	const char *s = strrchr(level, '/');
	Con_Printf("instance: player %d travels to %s (%s, %s)\n", i, s ? s + 1 : level, entrance, door);
	Tr_SendState(i, COORD_SR_TRAVEL, s ? s + 1 : level, entrance, door);
	p->traveled = 1;
	p->travelAt = tr.now;
	return 1;
}

/* ---- the metro ---- */

byte Travel_MetroUse(const char *usr, const char *door) {
	const char *names[TR_STATIONS];
	uint8_t buf[TR_MSG];
	NetRepBuf w;
	cEntity *e = World_FindEntity(usr);
	int i = e ? Players_Of(e) : -1;
	int conn = i >= 0 ? Rep_PlayerConn(i) : -1;
	if (conn < 0) {
		return 0;
	}
	int n = Metro_Stations(names, TR_STATIONS), m = 0;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_METRO);
	NetRep_PutStr(&w, door);
	for (int k = 0; k < n; k++) {
		m += crt_stricmp(names[k], Instance_Level()) != 0;
	}
	NetRep_PutU8(&w, (uint32_t)m);
	for (int k = 0; k < n; k++) {
		if (crt_stricmp(names[k], Instance_Level())) {
			NetRep_PutStr(&w, names[k]);
		}
	}
	Str_Copy(tr.pl[i].metroDoor, door, sizeof(tr.pl[i].metroDoor));
	Net2_Send(Instance_Host(), conn, TR_CHANNEL, buf, w.size);
	Con_Printf("instance: player %d at the metro door %s, map sent\n", i, door);
	return 1;
}

/* the station a player picked: the ride of the metro (Metro.UseMetro on the instance: the elevator with MoveLevel) */
static void Tr_Pick(int conn, const uint8_t *data, int size) {
	const char *names[TR_STATIONS];
	char station[COORD_LEVEL + 1], opt[COORD_LEVEL + 16];
	NetRepReader r;

	int i = Rep_ConnPlayer(conn);
	NetRepReader_Init(&r, data + 1, size - 1);
	NetRep_GetStr(&r, station, sizeof(station));
	cEntity *e = i >= 0 ? Players_Get(i) : NULL;
	if (r.bad || !e || !tr.pl[i].metroDoor[0] || !station[0]) {
		return;
	}
	int n = Metro_Stations(names, TR_STATIONS), ok = 0;
	for (int k = 0; k < n; k++) {
		ok |= !crt_stricmp(names[k], station) && crt_stricmp(station, Instance_Level());
	}
	if (!ok) {
		return;
	}
	Str_PrintfQuiet(opt, sizeof(opt), "MoveLevel %s", station);
	Con_Printf("instance: player %d takes the metro to %s\n", i, station);
	Players_Bind(i);
	if (!PORT_CALL(Elevator, UseElevator, NULL, "ssss", e->name, tr.pl[i].metroDoor, opt, "")) {
		Native_ErrPrint();
	}
	Players_BindTurn();
	tr.pl[i].metroDoor[0] = 0;
}

byte Travel_Message(Net2Host *host, int conn, const uint8_t *data, int size) {
	if (size < 1) {
		return 0;
	}
	if (data[0] == COORD_MSG_ITICKET) {
		if (Travel_Coordinated()) {
			Tr_Ticket(host, conn, data, size);
		}
		return 1;
	}
	if (data[0] == COORD_MSG_PICK) {
		Tr_Pick(conn, data, size);
		return 1;
	}
	return 0;
}

/* ---- the owned ship ---- */

void Travel_Boarded(int player, cEntity *ship) {
	char text[512];
	TrPlayer *p = &tr.pl[player];
	if (!p->used || !ShipEdit_ShipText(ship, text, sizeof(text)) || !text[0]) {
		return;
	}
	CoordKv_Set(p->state, &p->len, sizeof(p->state), "ship", text);
}

cEntity *Travel_OwnShip(int player, const vec3_t *pos, float yaw) {
	char text[512];
	nativeVal_t c[3], tp, out, ang[3], at;
	TrPlayer *p = &tr.pl[player];
	cEntity *ship = NULL;

	if (!p->used || !CoordKv_Get(p->state, p->len, "ship", text, sizeof(text)) || !text[0]) {
		return NULL;
	}
	Native_SetFloat(&c[0], pos->x);
	Native_SetFloat(&c[1], pos->y);
	Native_SetFloat(&c[2], pos->z);
	Mode_Tuple3(&tp, &c[0], &c[1], &c[2]);
	Native_SetNone(&out);
	if (PORT_CALL(ShipEdit, CreateShipFromSaveVar, &out, "svs", text, &tp, "Parked") && out.type == NV_ENT &&
		(ship = Native_Ent(&out)) != NULL) {
		Native_SetFloat(&ang[0], 0.0);
		Native_SetFloat(&ang[1], yaw);
		Native_SetFloat(&ang[2], 0.0);
		Mode_Tuple3(&at, &ang[0], &ang[1], &ang[2]);
		nativeVal_t one;
		Native_SetInt(&one, 1);
		if (!Native_EntSet(ship, ship->name, "Ang", &at) || !Native_EntSet(ship, ship->name, "Active", &one) ||
			!PORT_CALL(Parking, SetShipAsParked, NULL, "s", ship->name)) {
			Native_ErrPrint();
		}
		Native_Free(&at);
		Con_Printf("instance: player %d called its own ship %s (%s)\n", player, ship->name, text);
	} else {
		ship = NULL;
		Native_ErrPrint();
	}
	Native_Free(&out);
	Native_Free(&tp);
	return ship;
}

/* ---- tests ---- */

void Travel_TestUse(int player, const char *entity, const char *option) {
	cEntity *e = Players_Get(player);
	cEntity *t = World_FindEntity(entity);
	if (!e || !t || !Entity_IsType(t, TYEN_WITH_LIFE)) {
		Con_Printf("instance: use: no player %d or no entity %s\n", player, entity);
		return;
	}
	cArrowIndicator *a = &WLife(t)->arrow;
	if (option && *option) {
		for (int k = 0; k < a->nOpt; k++) {
			if (!strncmp(cArrowIndicator_GetOpt(a, k), option, strlen(option))) {
				a->iOpt = k;
			}
		}
	}
	Con_Printf("instance: player %d uses %s (%s)\n", player, entity, a->nOpt ? cArrowIndicator_GetOpt(a, a->iOpt) : "");
	Players_Bind(player);
	cArrowIndicator_Execute(a, e);
	Players_BindTurn();
}

void Travel_TestGoto(int player, const char *entity) {
	cEntity *e = Players_Get(player);
	cEntity *t = World_FindEntity(entity);
	if (!e || !t) {
		Con_Printf("instance: goto: no player %d or no entity %s\n", player, entity);
		return;
	}
	vec3_t p = { t->pos.x + 300.0f, t->pos.y + 100.0f, t->pos.z };
	e->pos = p;
	Con_Printf("instance: player %d next to %s (%.0f %.0f %.0f)\n", player, entity, (double)p.x, (double)p.y,
		(double)p.z);
}
