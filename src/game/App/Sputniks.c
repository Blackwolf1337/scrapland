/*
 * The players' Sputniks of the sandbox and freeroam worlds and their hangars, see include/scrap/sputniks.h. Not in
 * the original.
 */
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/faction.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/instance.h"
#include "scrap/net2.h"
#include "scrap/netrep.h"
#include "scrap/places.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/scrap.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/sputniks.h"
#include "scrap/travel.h"
#include "scrap/vehicleentity.h"
#include "scrap/world.h"

#define SPK_HANGARS 9				/* Hangar.HangarShip1-9; Hangar.iHangarShip 9 is the spare ship (HangarShipAux) */
#define SPK_KEYS (SPK_HANGARS + 3)	/* and iHangarShip, DestroyedShips */
#define SPK_TEXT 512				/* a ship's text (ShipEdit_ShipText) */
#define SPK_MSG 8192
#define SPK_MOVED 100.0f			/* units the parked ship moves before its Sputnik follows */
#define SPK_NAME 64
#define SPK_HOLD 10.0				/* s: the coop member that used Sputnik is hidden by its interface before the screen */
#define SPK_BESIDE 1500.0f			/* units: the coop story's Sputnik stands beside a ship this near */

typedef struct SpkPlayer {
	cEntity *spt;					/* its Sputnik (Sputnik.spt of the player), read while World_FindEntity(name) is it */
	char name[SPK_NAME];
	cEntity *ship;					/* the parked ship it stands beside, NULL: away */
	cEntity *failed;				/* the ship Sputnik.Assign failed at (not tried again there) */
	vec3_t at;
	byte open;						/* the hangar screen is on its client */
} SpkPlayer;

static struct {
	byte ready;						/* the module Sputnik imported, its class loaded */
	SpkPlayer pl[PLAYERS_MAX];
	byte clientOpen;				/* client: the instance's hangar screen is open here */
	cEntity *coopSpt;				/* the coop story's Sputnik a member used last, read while it is Sputnik.spt */
	int coopUser;					/* that member */
	uint16_t coopJoined;			/* its join */
	double coopSince;				/* the time of its use */
} spk;

/* the k-th key of the hangar (0..8 the hangars, 9 the spare ship, 10 the one flown, 11 the destroyed ones) */
static const char *Spk_Key(int k, char *buf, int cap) {
	if (k < SPK_HANGARS) {
		Str_PrintfQuiet(buf, cap, "Hangar.HangarShip%d", k + 1);
		return buf;
	}
	return k == SPK_HANGARS ? "Hangar.HangarShipAux" : k == SPK_HANGARS + 1 ? "Hangar.iHangarShip" :
		"Hangar.DestroyedShips";
}

static int Spk_KeyIndex(const char *name) {
	char key[32];
	for (int k = 0; k < SPK_KEYS; k++) {
		if (!strcmp(name, Spk_Key(k, key, sizeof(key)))) {
			return k;
		}
	}
	return -1;
}

static cEntity *Spk_Entity(const SpkPlayer *p) {
	cEntity *e = p->spt ? World_FindEntity(p->name) : NULL;
	return e == p->spt && e && !e->deadNode.target ? e : NULL;
}

/* ---- the hangar of a player (its state text) ---- */

/* entry k of the list text of Hangar.DestroyedShips ("[0,0,...]") set to v */
static void Spk_SetDestroyed(int i, int k, int v) {
	char text[64], out[64];
	int n = 0, pos = 0;
	if (!Travel_GetVar(i, "Hangar.DestroyedShips", text, sizeof(text))) {
		return;
	}
	out[pos++] = '[';
	for (const char *s = text; *s;) {
		char *end;
		long x = strtol(s + (*s == '[' || *s == ','), &end, 10);
		int len = (int)sizeof(out) - 1 - pos;
		int w = end == s + (*s == '[' || *s == ',') ? -1 :
			Str_PrintfQuiet(out + pos, len, "%s%ld", n ? "," : "", n == k ? (long)v : x);
		if (w < 0 || w >= len) {
			break;
		}
		pos += w;
		n++;
		s = end;
	}
	Str_PrintfQuiet(out + pos, (int)sizeof(out) - pos, "]");
	Travel_SetVar(i, "Hangar.DestroyedShips", out);
}

/* the first hangar of a player: its ship (the state key ship, else the own ship it called) in the first, the game's
 * spare ship */
static void Spk_HangarInit(int i) {
	char v[SPK_TEXT], key[32];
	if (Travel_GetVar(i, "Hangar.iHangarShip", v, sizeof(v))) {
		return;
	}
	byte ship = (Travel_GetVar(i, "ship", v, sizeof(v)) && v[0]) ||
		(Players_Ship(i) && ShipEdit_ShipText(Players_Ship(i), v, sizeof(v)) && v[0]);
	for (int k = 0; k < SPK_HANGARS; k++) {
		Travel_SetVar(i, Spk_Key(k, key, sizeof(key)), k == 0 && ship ? v : "");
	}
	const char *aux = SaveVar_Get("Hangar.HangarShipAux");
	if (aux) {
		Travel_SetVar(i, "Hangar.HangarShipAux", aux);
	}
	Travel_SetVar(i, "Hangar.DestroyedShips", "[0,0,0,0,0,0,0,0,0,0]");
	Travel_SetVar(i, "Hangar.iHangarShip", ship ? "0" : "9");
}

void Sputniks_Boarded(int player, const char *text) {
	char v[SPK_TEXT], key[32];
	int k, empty = -1;
	if (!g_Instance || !Places_On()) {
		return;
	}
	Spk_HangarInit(player);
	if (Travel_GetVar(player, "Hangar.HangarShipAux", v, sizeof(v)) && !strcmp(v, text)) {
		Travel_SetVar(player, "Hangar.iHangarShip", "9");
		return;
	}
	for (k = 0; k < SPK_HANGARS; k++) {
		Travel_GetVar(player, Spk_Key(k, key, sizeof(key)), v, sizeof(v));
		if (!strcmp(v, text)) {
			break;
		}
		if (!v[0] && empty < 0) {
			empty = k;
		}
	}
	if (k == SPK_HANGARS) {
		Travel_GetVar(player, "Hangar.iHangarShip", v, sizeof(v));
		int cur = crt_atoi(v);
		k = empty >= 0 ? empty : cur >= 0 && cur < SPK_HANGARS ? cur : 0;
		Travel_SetVar(player, Spk_Key(k, key, sizeof(key)), text);
		Spk_SetDestroyed(player, k, 0);
	}
	Str_PrintfQuiet(v, sizeof(v), "%d", k);
	Travel_SetVar(player, "Hangar.iHangarShip", v);
}

/* ---- instance ---- */

/* Sputnik.Assign(ship) with player i bound: its Sputnik (made the first time) beside the ship */
static void Spk_Assign(int i, SpkPlayer *p, cEntity *ship) {
	nativeVal_t v, s;
	Native_SetNone(&s);
	int was = Players_Enter(i);
	byte ok = Mode_EntHandle(ship, &v) && PORT_CALL(Sputnik, Assign, NULL, "v", &v);
	Native_Free(&v);
	cEntity *spt = ok && Mode_G("Sputnik", "spt", &s) && s.type == NV_ENT ? Native_Ent(&s) : NULL;
	Native_Free(&s);
	if (!ok) {
		Native_ErrPrint();
	}
	Players_Leave(was);
	p->failed = spt ? NULL : ship;
	if (!spt) {
		return;
	}
	/* its use is the instance's (Sputniks_Use), not the single player's Sputnik.ChangeShip */
	char *none = "";
	Entity_VarS(spt, "UseFunc", &none, 1);
	if (spt != p->spt) {
		Con_Printf("sputniks: player %d's Sputnik %s beside %s\n", i, spt->name, ship->name);
	}
	p->spt = spt;
	Str_Copy(p->name, spt->name, sizeof(p->name));
	p->ship = ship;
	p->at = ship->pos;
}

byte Sputniks_Outdoor(void) {
	return g_pWorld->weapons.count && Mode_ModGetI("Init", "Outdoor", 0);
}

void Sputniks_Frame(void) {
	/* with single player's flows the players' Sputniks are the scripts' (include/scrap/play.h) */
	if (!g_Instance || !Places_On() || Rep_SpFlows() || !Sputniks_Outdoor()) {
		return;
	}
	if (!spk.ready) {
		spk.ready = 1;
		Faction_EnsureClass("Sputnik");
		if (!Mode_Import("Sputnik")) {
			Native_ErrPrint();
		}
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		SpkPlayer *p = &spk.pl[i];
		cEntity *ship = Players_Get(i) ? Players_Ship(i) : NULL;
		cEntity *spt = Spk_Entity(p);
		/* the ship stands parked with no one in it (Parking.SetShipAsParked holds it with a move) */
		if (!ship || cVehicleEntity_GetOwnerSlot(ship) != -1 || WLife(ship)->life <= 0 || !cVehicleEntity_InPlace(ship)) {
			if (spt && spt->active) {
				Entity_SetActive(spt, 0);
			}
			p->ship = NULL;
			continue;
		}
		float dx = ship->pos.x - p->at.x, dy = ship->pos.y - p->at.y, dz = ship->pos.z - p->at.z;
		if ((spt && spt->active && p->ship == ship && dx * dx + dy * dy + dz * dz < SPK_MOVED * SPK_MOVED) ||
			p->failed == ship) {
			continue;
		}
		Spk_Assign(i, p, ship);
	}
}

static void Spk_ToClient(int i, const uint8_t *buf, int size) {
	int conn = Rep_PlayerConn(i);
	if (conn >= 0 && Instance_Host()) {
		Net2_Send(Instance_Host(), conn, REP_CH_RELIABLE, buf, size);
	}
}

/* the hangar screen on player i's client: its money and its hangar */
static void Spk_Open(int i) {
	static uint8_t buf[SPK_MSG];
	char key[32], v[SPK_TEXT];
	NetRepBuf w;
	Spk_HangarInit(i);
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, SPUTNIKS_MSG);
	NetRep_PutU8(&w, SPUTNIKS_OPEN);
	NetRep_PutU32(&w, (uint32_t)Players_Money(i));
	NetRep_PutU16(&w, SPK_KEYS);
	for (int k = 0; k < SPK_KEYS; k++) {
		Travel_GetVar(i, Spk_Key(k, key, sizeof(key)), v, sizeof(v));
		int n = (int)strlen(v);
		NetRep_PutStr(&w, Spk_Key(k, key, sizeof(key)));
		NetRep_PutU16(&w, (uint32_t)n);
		CoordMsg_PutBytes(&w, v, n);
	}
	if (w.bad) {
		Con_Printf("sputniks: the hangar of player %d does not fit a message\n", i);
		return;
	}
	Spk_ToClient(i, buf, w.size);
	spk.pl[i].open = 1;
	Travel_GetVar(i, "Hangar.iHangarShip", v, sizeof(v));
	Con_Printf("sputniks: the hangar of player %d opens on its client (hangar %s, money %d)\n", i, v,
		Players_Money(i));
}

/* the coop story's one Sputnik (single player's, the scripts' Sputnik.spt): the parked ship it stands beside becomes
 * the own ship of player i, who uses it (its hangar screen comes through the scripts' Sputnik.ChangeShip, include/
 * scrap/coop.h); 1 when t is that Sputnik */
static byte Spk_CoopUse(int i, cEntity *t) {
	nativeVal_t s;
	Native_SetNone(&s);
	cEntity *spt = Mode_ModGet("Sputnik", "spt", &s) && s.type == NV_ENT ? Native_Ent(&s) : NULL;
	Native_Free(&s);
	Native_ErrClear();
	if (!t || t != spt) {
		return 0;
	}
	spk.coopSpt = t;
	spk.coopUser = i;
	spk.coopJoined = Players_Joined(i);
	spk.coopSince = Instance_Time();
	cEntity *ship = NULL;
	float best = SPK_BESIDE * SPK_BESIDE;
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		float dx = e->pos.x - t->pos.x, dy = e->pos.y - t->pos.y, dz = e->pos.z - t->pos.z, d = dx * dx + dy * dy + dz * dz;
		if (Entity_IsType(e, TYEN_CAR) && !e->deadNode.target && !Obj(e)->replica && d < best && WLife(e)->life > 0 &&
			cVehicleEntity_GetOwnerSlot(e) == -1 && cVehicleEntity_InPlace(e)) {
			ship = e;
			best = d;
		}
	}
	if (!ship) {
		Con_Printf("sputniks: the party's Sputnik stands beside no parked ship\n");
	} else if (Players_Ship(i) != ship) {
		for (int k = 0; k < PLAYERS_MAX; k++) {
			if (k != i && Players_Ship(k) == ship) {
				Players_SetShip(k, NULL);
			}
		}
		Players_SetShip(i, ship);
		Con_Printf("sputniks: the party's Sputnik: %s is player %d's ship now\n", ship->name, i);
	}
	return 1;
}

/* with single player's flows (include/scrap/play.h): the player whose own Sputnik (its Sputnik.spt) e is, -1 */
static int Spk_SpOwner(cEntity *e) {
	cActCtrl *a = Entity_IsType(e, TYEN_WALKCHAR) ? WLife(e)->actCtrl : NULL;
	if (!a || !a->cls || !a->cls->name || strcmp(a->cls->name, "Sputnik")) {
		return -1;
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		nativeVal_t s;
		if (!Players_InUse(i)) {
			continue;
		}
		int was = Players_Enter(i);
		Native_SetNone(&s);
		cEntity *spt = Mode_ModGet("Sputnik", "spt", &s) && s.type == NV_ENT ? Native_Ent(&s) : NULL;
		Native_Free(&s);
		Native_ErrClear();
		Players_Leave(was);
		if (spt == e) {
			return i;
		}
	}
	return -1;
}

int Sputniks_CoopUser(cEntity *e) {
	nativeVal_t s;
	if (e && Rep_SpFlows()) {
		return Spk_SpOwner(e);
	}
	if (!e || e != spk.coopSpt || !Coop_On() || Rep_PlayerConn(spk.coopUser) < 0 || Players_Joined(spk.coopUser) != spk.coopJoined) {
		return -1;
	}
	Native_SetNone(&s);
	cEntity *spt = Mode_ModGet("Sputnik", "spt", &s) && s.type == NV_ENT ? Native_Ent(&s) : NULL;
	Native_Free(&s);
	Native_ErrClear();
	return spt == e ? spk.coopUser : -1;
}

byte Sputniks_CoopHolds(int i) {
	return spk.coopSpt && i == spk.coopUser && Players_Joined(i) == spk.coopJoined && Instance_Time() - spk.coopSince < SPK_HOLD;
}

byte Sputniks_Use(int i, cEntity *t) {
	int owner = -1;
	if (g_Instance && Coop_On()) {
		/* the use itself is single player's (its Sputnik.ChangeShip with player i bound) */
		Spk_CoopUse(i, t);
		return 0;
	}
	for (int k = 0; t && k < PLAYERS_MAX; k++) {
		if (Spk_Entity(&spk.pl[k]) == t) {
			owner = k;
		}
	}
	if (owner < 0) {
		return 0;
	}
	if (owner == i && !spk.pl[i].open) {
		Spk_Open(i);
	}
	return 1;
}

/* the hangar flown now: the parked ship of player i becomes that hangar's ship (the state key ship) */
static void Spk_Fly(int i) {
	char v[SPK_TEXT], key[32], cur[SPK_TEXT];
	Travel_GetVar(i, "Hangar.iHangarShip", v, sizeof(v));
	int k = crt_atoi(v);
	if (k < 0 || k > SPK_HANGARS || !Travel_GetVar(i, Spk_Key(k, key, sizeof(key)), v, sizeof(v)) || !v[0]) {
		return;
	}
	cEntity *ship = Players_Ship(i);
	if (!ship || cVehicleEntity_GetOwnerSlot(ship) != -1 || (ShipEdit_ShipText(ship, cur, sizeof(cur)) && !strcmp(v, cur))) {
		return;
	}
	Travel_SetVar(i, "ship", v);
	int was = Players_Enter(i);
	cEntity *now = Rep_ReplaceShip(i, ship);
	Players_Leave(was);
	Con_Printf("sputniks: player %d takes hangar %d: %s\n", i, k, now ? now->name : "(no ship)");
}

byte Sputniks_Message(Net2Host *host, int conn, const uint8_t *data, int size) {
	static char val[COORD_STORY_VALUE + 1];
	char name[64];
	NetRepReader r;
	(void)host;
	if (size < 2 || data[0] != SPUTNIKS_MSG) {
		return 0;
	}
	int i = Rep_ConnPlayer(conn);
	if (i < 0 || i >= PLAYERS_MAX || data[1] != SPUTNIKS_DONE || !spk.pl[i].open) {
		return 1;
	}
	NetRepReader_Init(&r, data + 2, size - 2);
	int money = (int)NetRep_GetU32(&r), count = (int)NetRep_GetU16(&r);
	for (int k = 0; k < count && !r.bad; k++) {
		NetRep_GetStr(&r, name, sizeof(name));
		int n = (int)NetRep_GetU16(&r);
		if (n > COORD_STORY_VALUE) {
			r.bad = 1;
			break;
		}
		CoordMsg_GetBytes(&r, val, n);
		val[n] = 0;
		/* the hangar's own keys (the client has the editor's lists too) */
		if (!r.bad && Spk_KeyIndex(name) >= 0 && n < SPK_TEXT) {
			Travel_SetVar(i, name, val);
		}
	}
	spk.pl[i].open = 0;
	if (r.bad) {
		return 1;
	}
	/* a rebuild costs money; the client never gives more than the player has */
	if (money < Players_Money(i)) {
		int was = Players_Enter(i);
		*Players_CurMoney() = money;
		Players_Leave(was);
	}
	Con_Printf("sputniks: player %d closed its hangar (money %d)\n", i, Players_Money(i));
	Spk_Fly(i);
	return 1;
}

void Sputniks_Left(int i) {
	if (i < 0 || i >= PLAYERS_MAX) {
		return;
	}
	cEntity *spt = Spk_Entity(&spk.pl[i]);
	if (spt) {
		cEntity_Del(spt);
	}
	memset(&spk.pl[i], 0, sizeof(spk.pl[i]));
}

/* ---- client ---- */

/* the hangar's ships are made (hidden) at ShipEdit.editShipPosDummy, which single player's Parking sets at the level's
 * start to the parking of the player's ship; a client's level start has none: the own entity's place */
void Sputniks_ClientDummy(void) {
	nativeVal_t d, c[3], pos, ang, name, zero;
	cEntity *e = Players_Local();
	Native_SetNone(&d);
	byte have = Mode_ModGet("ShipEdit", "editShipPosDummy", &d) && d.type != NV_NONE;
	Native_Free(&d);
	Native_ErrClear();
	if (have || !g_pWorld) {
		return;
	}
	/* the player is hidden while Sputnik's interface animation plays (no own entity): the camera's place */
	const vec3_t *at = e ? &e->pos : &g_pWorld->camPos[0];
	Native_SetFloat(&c[0], at->x);
	Native_SetFloat(&c[1], at->y);
	Native_SetFloat(&c[2], at->z);
	Mode_Tuple3(&pos, &c[0], &c[1], &c[2]);
	Native_SetFloat(&zero, 0.0);
	Mode_Tuple3(&ang, &zero, &zero, &zero);
	Native_SetStr(&name, "Sputnik");
	Mode_Tuple3(&d, &name, &pos, &ang);
	if (!Mode_SetModVar("ShipEdit", "editShipPosDummy", &d)) {
		Native_ErrPrint();
	}
	Native_Free(&d);
	Native_Free(&name);
	Native_Free(&ang);
	Native_Free(&pos);
}

byte Sputniks_ClientMessage(const uint8_t *data, int size) {
	NetRepReader r;
	if (size < 2 || data[0] != SPUTNIKS_MSG || data[1] != SPUTNIKS_OPEN) {
		return 0;
	}
	NetRepReader_Init(&r, data + 2, size - 2);
	int money = (int)NetRep_GetU32(&r);
	if (r.bad) {
		return 1;
	}
	Coop_GetHangar(&r);
	*Players_CurMoney() = money;
	spk.clientOpen = 1;
	if (Mode_Import("ShipEdit")) {
		Sputniks_ClientDummy();
	}
	if (!Mode_Import("SputnikInterface") || !PORT_CALL(SputnikInterface, EnterSputnikInterface, NULL, "i", 0L)) {
		Native_ErrPrint();
	}
	Con_Printf("sputniks: the hangar of the instance, money %d\n", money);
	/* test (SCRAP_PLAYBOT_SPUTNIK=<control>: Exit, HangarAux, Hangar1-9): that control of the screen at once */
	const char *pick = getenv("SCRAP_PLAYBOT_SPUTNIK");
	if (pick && *pick && spk.clientOpen && (!strcmp(pick, "Exit") ?
		!PORT_CALL(SputnikInterface, ExitSputnikInterface, NULL, "is", 0L, pick) :
		!PORT_CALL(SputnikInterface, SelectShip, NULL, "is", 0L, pick))) {
		Native_ErrPrint();
	}
	return 1;
}

void Sputniks_ClientDone(void) {
	static uint8_t buf[SPK_MSG];
	NetRepBuf w;
	if (!g_Play || !spk.clientOpen) {
		return;
	}
	spk.clientOpen = 0;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, SPUTNIKS_MSG);
	NetRep_PutU8(&w, SPUTNIKS_DONE);
	NetRep_PutU32(&w, (uint32_t)*Players_CurMoney());
	Coop_PutHangar(&w);
	if (!w.bad && Watch_SendReliable(buf, w.size)) {
		const char *cur = SaveVar_Get("Hangar.iHangarShip");
		Con_Printf("sputniks: the hangar to the instance (hangar %s, money %d)\n", cur ? cur : "-", *Players_CurMoney());
	}
}
