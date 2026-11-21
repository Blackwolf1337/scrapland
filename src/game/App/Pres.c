/*
 * The presentation interface (stage 3.4, -mp2), see include/scrap/pres.h: the records of the instance's presentation
 * calls to their audience, the client's side that plays them through the same sinks, the HUD models of the players,
 * the audit of calls without a route and of sounds played twice. Not in the original.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/fx.h"
#include "scrap/fxmisc.h"
#include "scrap/game.h"
#include "scrap/input.h"
#include "scrap/logic.h"
#include "scrap/musicfmod.h"
#include "scrap/instance.h"
#include "scrap/net2.h"
#include "scrap/netaudit.h"
#include "scrap/netclass.h"
#include "scrap/netscene.h"
#include "scrap/netrep.h"
#include "scrap/objentity.h"
#include "scrap/platform.h"
#include "scrap/players.h"
#include "scrap/pres.h"
#include "scrap/pyfunction.h"
#include "scrap/replicate.h"
#include "scrap/repent.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/sound.h"
#include "scrap/uichan.h"
#include "scrap/viewers.h"
#include "scrap/weapons.h"
#include "scrap/world.h"

#include "../Global/testchannel.h"

#define PRES_BUF 4096				/* the records of one player before they are sent */
#define PRES_REC 1024				/* the longest record */
#define PRES_RECENT 32				/* one-shot sounds the client remembers for the double audit */
#define PRES_CALLS 16				/* the scripts' calls the instance keeps the names of */
#define PRES_HOLDS 32				/* effect records a client holds until it has their entities */
#define PRES_HOLD_REC 640			/* the longest held record */
#define PRES_HOLD_NS 1000000000ull	/* how long: then it plays and counts what it misses */
#define PRES_GVARS 64
#define PRES_CHARRES 64
#define PRES_WORLD_HUD 32			/* the items of the world's scorer kept for the players that join */
#define PRES_WORLD_VARS 16			/* the variables kept per item */
#define PRES_DOUBLE_NEAR 120.0f	/* units: a sound of the same name this near the client's own one is its double */

int g_PresIn;
int g_PresDerived;
const char *g_SndSite;
byte g_PresHud;

static const char *const pres_opNames[PRES_O_COUNT] = { "?", "soundat", "sound3d", "sound2d", "voiceopen", "voiceplay",
	"voicestop", "music", "shipexpl", "fxent", "msgtext", "labeltext", "speech", "speechcancel", "chat", "head", "fade",
	"cinema", "actionset", "hudadd", "hudclear", "hudshow", "hudset", "talk", "itemsnd", "scene", "sceneend", "campath",
	"fxcall", "gvar", "charres", "time", "camvar", "savevar" };

/* a voice of a -mp2 instance's model: nothing plays, the file's length and the time it started give what the scripts
 * read (VoiceRemain), the text what the talk voice's checks read */
typedef struct PresVoice {
	char text[0x80];
	int lenMs;
	double playAt;					/* Instance_Time when it started to play, < 0 before */
	byte open;
} PresVoice;

/* an item of the world's scorer 0 as the instance's records made it, with the last value of each variable set */
typedef struct PresWorldItem {
	char item[48], type[48];
	byte append;
	int nvars;
	struct {
		char name[32];
		byte text;
		char s[96];
		float d;
	} vars[PRES_WORLD_VARS];
} PresWorldItem;

typedef struct PresRecent {
	uint32_t hash;
	uint64_t ns;
	byte msg;						/* 0 the client's own, 1 a sound record, 2 inside another record (an effect, a talk) */
	byte hasPos;
	vec3_t pos;						/* where: a record's place, the own entity's for the client's own */
} PresRecent;

static struct {
	/* instance */
	uint8_t buf[PLAYERS_MAX][PRES_BUF];
	int size[PLAYERS_MAX];
	uint8_t rec[PRES_REC];
	NetRepBuf w;
	uint64_t to;					/* the audience of the record begun */
	int op;
	int own;						/* the player whose own entity steps, -1 */
	int skel;						/* depth of the HUD skeleton modules */
	int calls;						/* depth of the scripts' calls */
	const char *callMod[PRES_CALLS], *callFunc[PRES_CALLS];
	char site[96];
	const struct cSoundDesc *posDesc;	/* the description cSoundDesc_SetPos computed last, its place and volume */
	vec3_t pos;
	int posVolume;
	cScorer models[PLAYERS_MAX];
	byte made[PLAYERS_MAX];
	PresVoice voices[PLAYERS_MAX + 1][MUSIC_MAX_VOICES];	/* each player's, the last the world's */
	char music[64];					/* the world's music (the last play outside a player's context), "" none */
	float musicVolume;
	byte haveMusicVolume;
	char gvars[PRES_GVARS][48];		/* the engine variables of Pres_GVar the world's scripts set: their values then
									 * go to the players that join later (C code sets some of them too) */
	int numGVars;
	char charRes[PRES_CHARRES][32];	/* the character classes of Pres_CharRes */
	PresWorldItem worldHud[PRES_WORLD_HUD];	/* the items of the world's scorer and their sets: a player that joins gets them */
	int numWorldHud;
	int numCharRes;
	uint32_t sent[PRES_O_COUNT], bytes, cut;
	/* client */
	PresRecent recent[PRES_RECENT];
	int fxKind;						/* of the PRES_O_FXENT record played */
	int recOp;						/* the record being played */
	byte recHasPos;					/* the place of the sound record being played */
	vec3_t recPos;
	char lastText[64];				/* the text of the last message, label or speech record (the test verb) */
	int nextRecent;
	uint32_t played[PRES_O_COUNT];
	struct {
		uint64_t ns;				/* 0: free */
		int size, op;
		uint8_t rec[PRES_HOLD_REC];
	} hold[PRES_HOLDS];
	uint32_t held;
} pres = { .own = -1 };
static float pres_lag;				/* s the held message being played comes after its server time */
static double pres_shown = -1.0;	/* the server time drawn in this frame (Pres_ClientShown), < 0 before */

/* ---- both ---- */

/* SCRAP_PRES_TRACE (tests): every record sent (instance) and played (client) in the log */
static byte Pres_Trace(void) {
	static int trace = -1;
	if (trace < 0) {
		trace = getenv("SCRAP_PRES_TRACE") != NULL;
	}
	return (byte)trace;
}

byte Pres_Out(void) {
	return g_Mp2 && g_Instance && !g_PresIn && !g_PresDerived;
}

void Pres_Status(tcBuf *b) {
	int models = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		models += pres.made[i];
	}
	TC_Addf(b, "pres: %s, %u bytes, %u cut, %d HUD models, %u effect records held\n", !g_Mp2 ? "off" : g_Instance ?
		"instance" : "client", pres.bytes, pres.cut, models, pres.held);
	if (pres.lastText[0]) {
		TC_Addf(b, "last text\t%s\n", pres.lastText);
	}
	for (int op = 1; op < PRES_O_COUNT; op++) {
		if (pres.sent[op] || pres.played[op]) {
			TC_Addf(b, "%s\t%u\t%u\n", pres_opNames[op], pres.sent[op], pres.played[op]);
		}
	}
}

/* ---- instance: the audience and the records ---- */

void Pres_Init(void) {
	g_PresHud = g_Mp2 && g_Instance;
	if (g_PresHud) {
		Scorer_TalkModels();
	}
}

/* player i can get records: joined, with a connection */
static byte Pres_Online(int i) {
	cPlayer *p = Players_Ctx(i);
	return p && p->used && Rep_PlayerConn(i) >= 0;
}

static uint64_t Pres_Audience(presTo_t to, const vec3_t *pos) {
	uint64_t m = 0;
	int cur = Players_Current();
	if (to == PRES_PARTY && !Coop_On()) {
		to = PRES_PLAYER;
	}
	if (to == PRES_PLAYER && cur != PLAYERS_NONE) {
		return Pres_Online(cur) ? (uint64_t)1 << cur : 0;
	}
	if (to == PRES_NEAR && pos) {
		/* the players whose viewer (include/scrap/viewers.h: its camera's pose) is near the place */
		for (int k = Viewers_Count() - 1; k >= 0; k--) {
			const cViewer *v = Viewers_At(k);
			float dx = v->pos.x - pos->x, dy = v->pos.y - pos->y, dz = v->pos.z - pos->z;
			if (v->id >= 0 && v->id < PLAYERS_MAX && v->id != pres.own && Pres_Online(v->id) &&
				dx * dx + dy * dy + dz * dz <= NETREP_RADIUS * NETREP_RADIUS) {
				m |= (uint64_t)1 << v->id;
			}
		}
		return m;
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (Pres_Online(i)) {
			m |= (uint64_t)1 << i;
		}
	}
	return m;
}

static byte Pres_BeginTo(uint64_t to, int op) {
	pres.to = to;
	if (!pres.to) {
		return 0;
	}
	pres.op = op;
	NetRepBuf_Init(&pres.w, pres.rec, sizeof(pres.rec));
	NetRep_PutU8(&pres.w, (uint32_t)op);
	return 1;
}

byte Pres_Begin(presTo_t to, const vec3_t *pos, int op) {
	return Pres_BeginTo(Pres_Audience(to, pos), op);
}

byte Pres_BeginMask(uint64_t mask, int op) {
	uint64_t to = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if ((mask >> i & 1) && Pres_Online(i)) {
			to |= (uint64_t)1 << i;
		}
	}
	return Pres_BeginTo(to, op);
}

void Pres_PutStr(const char *s) {
	if (s && strlen(s) > 255) {
		pres.cut++;
	}
	NetRep_PutStr(&pres.w, s ? s : "");
}

void Pres_PutInt(int v) {
	NetRep_PutSVar(&pres.w, v);
}

void Pres_PutFloat(float f) {
	uint32_t u;
	memcpy(&u, &f, sizeof(u));
	NetRep_PutU32(&pres.w, u);
}

void Pres_PutVec(const vec3_t *v) {
	Pres_PutFloat(v->x);
	Pres_PutFloat(v->y);
	Pres_PutFloat(v->z);
}

static void Pres_FlushPlayer(Net2Host *host, int i) {
	int conn = Rep_PlayerConn(i);
	if (pres.size[i] > 1 && conn >= 0 && host) {
		Net2_Send(host, conn, REP_CH_RELIABLE, pres.buf[i], pres.size[i]);
		pres.bytes += (uint32_t)pres.size[i];
	}
	pres.size[i] = 0;
}

void Pres_Send(void) {
	if (pres.w.bad) {
		pres.cut++;
		return;
	}
	NetAudit_Func(NA_SOUND, "pres.sent:", pres_opNames[pres.op]);
	pres.sent[pres.op]++;
	if (Pres_Trace()) {
		Con_Printf("pres: %s to %llx at %.3f from %s\n", pres_opNames[pres.op], (unsigned long long)pres.to, Instance_Time(),
			Pres_ModSite());
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (!(pres.to >> i & 1)) {
			continue;
		}
		if (pres.size[i] + pres.w.size > PRES_BUF) {
			Pres_FlushPlayer(Instance_Host(), i);
		}
		if (!pres.size[i]) {
			pres.buf[i][0] = PRES_MSG;
			pres.size[i] = 1;
			if (NetScene_Timed(i)) {
				/* the records of a scene's audience at the scene's time on the client */
				NetRepBuf t;
				NetRepBuf_Init(&t, pres.buf[i] + 1, 8);
				NetRep_PutU8(&t, PRES_O_TIME);
				NetRep_PutSVar(&t, (int)(Instance_Time() * 1000.0));
				pres.size[i] += t.size;
			}
		}
		memcpy(pres.buf[i] + pres.size[i], pres.rec, (size_t)pres.w.size);
		pres.size[i] += pres.w.size;
	}
}

void Pres_Flush(Net2Host *host) {
	NetScene_Flush();
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (pres.size[i]) {
			Pres_FlushPlayer(host, i);
		}
	}
}

presSaved_t Pres_SimScript(void) {
	presSaved_t s = { g_PresDerived, pres.own };
	if (g_Mp2 && g_Instance) {
		g_PresDerived = 0;
		pres.own = -1;
	}
	return s;
}

void Pres_SimScriptEnd(presSaved_t saved) {
	if (g_Mp2 && g_Instance) {
		g_PresDerived = saved.derived;
		pres.own = saved.own;
	}
}

void Pres_OwnStep(int player) {
	pres.own = player;
}

static byte Pres_Skeleton(const char *module) {
	return !strcmp(module, "CharScorer") || !strcmp(module, "RacerScorer");
}

void Pres_ModCall(const char *module, const char *func) {
	if (Pres_Skeleton(module)) {
		pres.skel++;
	}
	if (pres.calls < PRES_CALLS) {
		pres.callMod[pres.calls] = module;
		pres.callFunc[pres.calls] = func;
	}
	pres.calls++;
}

int Pres_SkelPause(void) {
	int depth = pres.skel;
	pres.skel = 0;
	return depth;
}

void Pres_SkelResume(int depth) {
	pres.skel = depth;
}

void Pres_HudMode(const char *mode) {
	if (!Pres_Out() || Players_Current() == PLAYERS_NONE || (strcmp(mode, "Walk") && strcmp(mode, "Racer"))) {
		return;
	}
	/* single player's skeleton module clears the HUD first */
	if (Pres_Begin(PRES_PLAYER, NULL, PRES_O_HUDCLEAR)) {
		Pres_Send();
	}
	if (Pres_Begin(PRES_PLAYER, NULL, PRES_O_ACTIONSET)) {
		Pres_PutStr(mode);
		Pres_Send();
	}
}

void Pres_ModLeave(const char *module) {
	if (Pres_Skeleton(module) && pres.skel > 0) {
		pres.skel--;
	}
	if (pres.calls > 0) {
		pres.calls--;
	}
}

const char *Pres_ModSite(void) {
	if (!pres.calls) {
		return "-";
	}
	/* the innermost three, the caller first: the engine's own natives (Scrap.SetCam) say little alone */
	int top = pres.calls > PRES_CALLS ? PRES_CALLS : pres.calls;
	int len = 0;
	pres.site[0] = 0;
	for (int k = top > 3 ? top - 3 : 0; k < top && len < (int)sizeof(pres.site); k++) {
		len += Str_PrintfQuiet(pres.site + len, sizeof(pres.site) - (size_t)len, "%s%s.%s", len ? ">" : "",
			pres.callMod[k], pres.callFunc[k]);
	}
	return pres.site;
}

byte Pres_NativeCall(cPy_Function *f) {
	const void *data;
	if (!Pres_Out() || !(f->flags & PYF_NATIVE)) {
		return 0;
	}
	const char *name = cPy_NativeFunc_Name((nativefunc_t)f->func);
	int n = cPy_Function_NativeBytes(f, &data);
	if (!name || strncmp(name, "FX", 2) || n > PRES_REC - 300) {
		return 0;
	}
	if (Pres_Begin(PRES_PLAYER, NULL, PRES_O_FXCALL)) {
		Pres_PutStr(name);
		NetRep_PutU16(&pres.w, (uint32_t)n);
		CoordMsg_PutBytes(&pres.w, data, n);
		Pres_Send();
	}
	return 1;
}

/* ---- instance: the HUD models ---- */

cScorer *Pres_HudModel(int i) {
	if (i < 0) {
		i = Players_Current();
	}
	if (!g_PresHud || i < 0 || i >= PLAYERS_MAX) {
		return NULL;
	}
	if (!pres.made[i]) {
		Scorer_InitModel(&pres.models[i]);
		pres.made[i] = 1;
	}
	return &pres.models[i];
}

cScorer *Pres_HudMade(int i) {
	return g_PresHud && i >= 0 && i < PLAYERS_MAX && pres.made[i] ? &pres.models[i] : NULL;
}

void Pres_HudReset(int i) {
	if (i >= 0 && i < PLAYERS_MAX && pres.made[i]) {
		Scorer_FreeModel(&pres.models[i]);
		pres.made[i] = 0;
	}
	if (i >= 0 && i < PLAYERS_MAX) {
		memset(pres.voices[i], 0, sizeof(pres.voices[i]));
		Scorer_TalkReset(i);
	}
}

/* ---- instance: the voice models ---- */

/* the voice of the current context (a player's, else the world's), NULL without the models or for a bad voice */
static PresVoice *Pres_Voice(int voice) {
	if (!g_PresHud || voice < 0 || voice >= MUSIC_MAX_VOICES) {
		return NULL;
	}
	int i = Players_Current();
	return &pres.voices[i >= 0 && i < PLAYERS_MAX ? i : PLAYERS_MAX][voice];
}

/* the ms the voice has played (its length at most) */
static int Pres_VoicePlayed(const PresVoice *v) {
	if (v->playAt < 0.0) {
		return 0;
	}
	double ms = (Instance_Time() - v->playAt) * 1000.0;
	return ms >= (double)v->lenMs ? v->lenMs : ms > 0.0 ? (int)ms : 0;
}

int Pres_VoxOpen(const char *name, int voice) {
	PresVoice *v = Pres_Voice(voice);
	if (!v) {
		return -1;
	}
	memset(v, 0, sizeof(*v));
	v->lenMs = Voice_LengthMs(name);
	v->open = v->lenMs >= 0;
	v->playAt = -1.0;
	return v->open;
}

int Pres_VoxPlay(int voice) {
	PresVoice *v = Pres_Voice(voice);
	if (!v) {
		return -1;
	}
	if (v->open && v->playAt < 0.0) {
		v->playAt = Instance_Time();
	}
	return 1;
}

int Pres_VoxStop(int voice) {
	PresVoice *v = Pres_Voice(voice);
	if (!v) {
		return -1;
	}
	memset(v, 0, sizeof(*v));
	return 1;
}

int Pres_VoxRemain(int voice, float *remain) {
	PresVoice *v = Pres_Voice(voice);
	if (!v) {
		return -1;
	}
	*remain = v->open ? (float)(v->lenMs - Pres_VoicePlayed(v)) * 0.001f : 0.0f;
	return v->open;
}

int Pres_VoxSetString(int voice, const char *text) {
	PresVoice *v = Pres_Voice(voice);
	if (!v) {
		return -1;
	}
	if (v->open) {
		Str_Copy(v->text, text, sizeof(v->text));
	}
	return v->open;
}

int Pres_VoxGetString(int voice, char *buf, int size) {
	PresVoice *v = Pres_Voice(voice);
	if (!v) {
		return -1;
	}
	*buf = 0;
	if (v->open && v->playAt >= 0.0 && Pres_VoicePlayed(v) >= v->lenMs) {
		/* played to its end: closed, as single player's talk voice stops at its end (PosSound.c) */
		memset(v, 0, sizeof(*v));
	}
	if (v->open) {
		Str_Copy(buf, v->text, size);
	}
	return v->open;
}

int Pres_HudOwner(const cScorer *sc) {
	if (!g_PresHud || sc < pres.models || sc >= pres.models + PLAYERS_MAX) {
		return -1;
	}
	return (int)(sc - pres.models);
}

/* the audience of a scorer's operation: the model's player (not while the HUD skeleton the client makes itself is
 * built), the world's scorer 0 to all; 0 for none */
static byte Pres_BeginHud(const cScorer *sc, int op, byte skeleton) {
	if (!Pres_Out() || (skeleton && pres.skel)) {
		return 0;
	}
	int i = Pres_HudOwner(sc);
	if (i >= 0) {
		int was = Players_Enter(i);
		byte r = Pres_Begin(PRES_PLAYER, NULL, op);
		Players_Leave(was);
		return r;
	}
	return sc == Scorer_GetWorld(0) ? Pres_Begin(PRES_LEVEL, NULL, op) : 0;
}

/* ---- instance: the sinks ---- */

void Pres_SoundPos(const struct cSoundDesc *d, const vec3_t *pos) {
	pres.posDesc = d;
	pres.pos = *pos;
	pres.posVolume = d->volume;
}

void Pres_Census(const char *what, const char *name, const vec3_t *pos) {
	/* the audit keeps the prefix as it is given (no copy): literals */
	byte snd = !strcmp(what, "snd");
	NetAudit_Name(NA_CENSUS, snd ? "snd.name:" : "debris.name:", name);
	if (!g_Mp2 || !g_Instance) {
		return;
	}
	for (int k = Viewers_Count() - 1; k >= 0; k--) {
		const cViewer *v = Viewers_At(k);
		float dx = pos ? v->pos.x - pos->x : 0.0f, dy = pos ? v->pos.y - pos->y : 0.0f;
		float dz = pos ? v->pos.z - pos->z : 0.0f;
		if (v->id >= 0 && v->id < PLAYERS_MAX && dx * dx + dy * dy + dz * dz <= NETREP_RADIUS * NETREP_RADIUS) {
			NetAudit_Name(NA_CENSUS, snd ? "snd.near:" : "debris.near:", name);
			return;
		}
	}
}

void Pres_CensusSound(const char *name, const cSoundDesc *d, const vec3_t *pos) {
	/* a description cSoundDesc_SetPos made last has its place (Pres_SoundPos); another one plays without a place */
	Pres_Census("snd", name, pos ? pos : d && d == pres.posDesc ? &pres.pos : NULL);
}

static void Pres_Unforwarded(const char *prefix, const char *site) {
	NetAudit_Func(NA_SOUND, prefix, site ? site : "?");
}

void Pres_NoRoute(int cat, const char *prefix, const char *site) {
	if (Pres_Out()) {
		NetAudit_Func((netAuditCat_t)cat, prefix, site);
	}
}

void Pres_ItemSound(cEntity *e, int item) {
	vec3_t pos;
	if (!Pres_Out() || !e || !Entity_IsType(e, TYEN_VEHICLE) || !PtrArray_Get(&Obj(e)->snd, item)) {
		return;
	}
	/* the players near whose clients have the ship (one beyond a client's interest has no replica to start it on) */
	uint64_t to = Pres_Audience(PRES_NEAR, Entity_GetPos(e, &pos));
	int slot = RepEnt_Slot(e);
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if ((to >> i & 1) && !Rep_PlayerHolds(i, slot)) {
			to &= ~((uint64_t)1 << i);
		}
	}
	if (!Pres_BeginTo(to, PRES_O_ITEMSND)) {
		return;
	}
	Pres_PutStr(e->name);
	Pres_PutInt(item);
	NetAudit_Func(NA_SOUND, "pres.from:", "Pres_ItemSound");
	Pres_Send();
}

void Pres_SoundAt(const char *name, const vec3_t *pos, int volume) {
	if (!Pres_Out() || !Pres_Begin(PRES_NEAR, pos, PRES_O_SOUNDAT)) {
		return;
	}
	NetAudit_Name(NA_CENSUS, "snd.sent:", name);
	Pres_PutStr(name);
	Pres_PutVec(pos);
	Pres_PutInt(volume);
	NetAudit_Func(NA_SOUND, "pres.from:", g_SndSite ? g_SndSite : "?");
	Pres_Send();
}

void Pres_Sound3D(const char *name, const cSoundDesc *d) {
	if (!Pres_Out()) {
		return;
	}
	if (d != pres.posDesc || d->loop) {
		/* no place known (a description not made by cSoundDesc_SetPos), or a loop no item of an entity stops */
		Pres_Unforwarded(d->loop ? "pres.unforwarded:soundloop:" : "pres.unforwarded:sound:", g_SndSite);
		return;
	}
	pres.posDesc = NULL;
	if (!Pres_Begin(PRES_NEAR, &pres.pos, PRES_O_SOUND3D)) {
		return;
	}
	NetAudit_Name(NA_CENSUS, "snd.sent:", name);
	Pres_PutStr(name);
	Pres_PutVec(&pres.pos);
	Pres_PutInt(d->volume - pres.posVolume);
	Pres_PutFloat(d->freq);
	Pres_PutFloat(d->param1);
	Pres_PutFloat(d->param2);
	Pres_PutFloat(d->param3);
	NetAudit_Func(NA_SOUND, "pres.from:", g_SndSite ? g_SndSite : "?");
	Pres_Send();
}

void Pres_Sound2D(const char *name, int volume, int pan) {
	if (!Pres_Out() || !Pres_Begin(PRES_PLAYER, NULL, PRES_O_SOUND2D)) {
		return;
	}
	Pres_PutStr(name);
	Pres_PutInt(volume);
	Pres_PutInt(pan);
	NetAudit_Func(NA_SOUND, "pres.from:", g_SndSite ? g_SndSite : "?");
	Pres_Send();
}

/* a voice: in a player's context to that player (the talk voice of a speaker goes to each player that hears it in
 * its context, Sound_PlayVoice), in the world's to every player */
static byte Pres_BeginVoice(int op) {
	return Pres_Out() && Pres_Begin(PRES_PLAYER, NULL, op);
}

void Pres_VoiceOpen(const char *file, int voice, int arg) {
	if (!Pres_BeginVoice(PRES_O_VOICEOPEN)) {
		return;
	}
	Pres_PutStr(file);
	Pres_PutInt(voice);
	Pres_PutInt(arg);
	Pres_Send();
}

void Pres_Talk(const char *file, const char *speaker, int arg) {
	if (!Pres_BeginVoice(PRES_O_TALK)) {
		return;
	}
	Pres_PutStr(file);
	Pres_PutStr(speaker);
	Pres_PutInt(arg);
	Pres_Send();
}

void Pres_VoicePlay(int voice) {
	if (Pres_BeginVoice(PRES_O_VOICEPLAY)) {
		Pres_PutInt(voice);
		Pres_Send();
	}
}

void Pres_VoiceStop(int voice) {
	if (Pres_BeginVoice(PRES_O_VOICESTOP)) {
		Pres_PutInt(voice);
		Pres_Send();
	}
}

void Pres_Music(const char *name, const char *cmd, float volume) {
	if (!Pres_Out()) {
		return;
	}
	/* the world's music, for the players that join later (Pres_Joined) */
	if (Players_Current() == PLAYERS_NONE || Coop_On()) {
		if (!crt_stricmp(cmd, "play")) {
			Str_Copy(pres.music, name ? name : "", sizeof(pres.music));
		} else if (!crt_stricmp(cmd, "stop")) {
			pres.music[0] = 0;
		} else if (!crt_stricmp(cmd, "volume")) {
			pres.musicVolume = volume;
			pres.haveMusicVolume = 1;
		}
	}
	if (!Pres_Begin(PRES_PARTY, NULL, PRES_O_MUSIC)) {
		return;
	}
	Pres_PutStr(name);
	Pres_PutStr(cmd);
	Pres_PutFloat(volume);
	Pres_Send();
}

/* the fields of a PRES_O_GVAR record: name, kind (1 text, 2 number), value */
static void Pres_PutGVar(const char *name, byte text, const char *s, float f) {
	Pres_PutStr(name);
	NetRep_PutU8(&pres.w, text ? 1u : 2u);
	if (text) {
		Pres_PutStr(s);
	} else {
		Pres_PutFloat(f);
	}
}

/* the engine variable name when the gvar table tags it STATE or WORLD_SETTING and it holds a text or a number, NULL
 * else */
static const cGVar *Pres_GVarOf(const char *name) {
	netTag_t tag = NetClass_Find(NCT_GVAR, name);
	const cGVar *v = GVar_Find(name);
	if ((tag != NC_STATE && tag != NC_WORLD_SETTING) || !v ||
		(v->base.type != GVAR_STRING && v->base.type != GVAR_INT && v->base.type != GVAR_FLOAT)) {
		return NULL;
	}
	return v;
}

/* the fields of v's value now into the record begun, sent */
static void Pres_SendGVar(const char *name, const cGVar *v) {
	byte text = v->base.type == GVAR_STRING;
	Pres_PutGVar(name, text, text && v->value.s ? v->value.s : "",
		v->base.type == GVAR_INT ? (float)v->value.i : v->base.type == GVAR_FLOAT ? v->value.f : 0.0f);
	Pres_Send();
}

void Pres_GVar(const char *name) {
	const cGVar *v = Pres_Out() ? Pres_GVarOf(name) : NULL;
	if (!v) {
		return;
	}
	if (Players_Current() == PLAYERS_NONE) {
		int k;
		for (k = 0; k < pres.numGVars && strcmp(pres.gvars[k], name); k++) {
		}
		if (k == pres.numGVars && k < PRES_GVARS) {
			Str_Copy(pres.gvars[k], name, sizeof(pres.gvars[k]));
			pres.numGVars++;
		}
	}
	if (Pres_Begin(PRES_PLAYER, NULL, PRES_O_GVAR)) {
		Pres_SendGVar(name, v);
	}
}

void Pres_CharRes(const char *name) {
	if (!Pres_Out() || !name || !*name) {
		return;
	}
	int k;
	for (k = 0; k < pres.numCharRes && crt_stricmp(pres.charRes[k], name); k++) {
	}
	if (k < pres.numCharRes) {
		return;
	}
	if (k < PRES_CHARRES) {
		Str_Copy(pres.charRes[k], name, sizeof(pres.charRes[k]));
		pres.numCharRes++;
	}
	if (Pres_Begin(PRES_LEVEL, NULL, PRES_O_CHARRES)) {
		Pres_PutStr(name);
		Pres_Send();
	}
}

/* the save variables of a player its client reads itself: the lives of the GDB on its HUD (StatusScorer) */
static const char *const pres_saveVars[] = { "Player.NumLives", NULL };

void Pres_SaveVar(int i, const char *name, const char *value) {
	int k;
	for (k = 0; pres_saveVars[k] && strcmp(pres_saveVars[k], name); k++) {
	}
	if (!pres_saveVars[k] || !Pres_Out() || i < 0 || i >= PLAYERS_MAX || !Pres_Online(i)) {
		return;
	}
	if (Pres_BeginTo((uint64_t)1 << i, PRES_O_SAVEVAR)) {
		Pres_PutStr(name);
		Pres_PutStr(value);
		Pres_Send();
	}
}

static void Pres_PutHudAdd(const char *item, const char *type, byte append, byte level);

void Pres_Joined(int i) {
	if (!Pres_Out() || i < 0 || i >= PLAYERS_MAX || !Pres_Online(i)) {
		return;
	}
	int was = Players_Enter(i);
	for (int k = 0; pres_saveVars[k]; k++) {
		const char *v = SaveVar_Get(pres_saveVars[k]);
		if (v) {
			Pres_SaveVar(i, pres_saveVars[k], v);
		}
	}
	Players_Leave(was);
	for (int k = 0; k < pres.numCharRes; k++) {
		if (Pres_BeginTo((uint64_t)1 << i, PRES_O_CHARRES)) {
			Pres_PutStr(pres.charRes[k]);
			Pres_Send();
		}
	}
	for (int k = 0; k < pres.numGVars; k++) {
		const cGVar *v = Pres_GVarOf(pres.gvars[k]);
		if (v && Pres_BeginTo((uint64_t)1 << i, PRES_O_GVAR)) {
			Pres_SendGVar(pres.gvars[k], v);
		}
	}
	if (pres.haveMusicVolume && Pres_BeginTo((uint64_t)1 << i, PRES_O_MUSIC)) {
		Pres_PutStr(NULL);
		Pres_PutStr("volume");
		Pres_PutFloat(pres.musicVolume);
		Pres_Send();
	}
	for (int k = 0; k < pres.numWorldHud; k++) {
		const PresWorldItem *w = &pres.worldHud[k];
		if (Pres_BeginTo((uint64_t)1 << i, PRES_O_HUDADD)) {
			Pres_PutHudAdd(w->item, w->type, w->append, 1);
			Pres_Send();
		}
		for (int v = 0; v < w->nvars; v++) {
			if (Pres_BeginTo((uint64_t)1 << i, PRES_O_HUDSET)) {
				Pres_PutStr(w->item);
				Pres_PutStr(w->vars[v].name);
				Pres_PutInt(w->vars[v].text ? 1 : 2);
				if (w->vars[v].text) {
					Pres_PutStr(w->vars[v].s);
				} else {
					Pres_PutFloat(w->vars[v].d);
				}
				Pres_Send();
			}
		}
	}
	if (pres.music[0] && Pres_BeginTo((uint64_t)1 << i, PRES_O_MUSIC)) {
		Pres_PutStr(pres.music);
		Pres_PutStr("play");
		Pres_PutFloat(0.0f);
		Pres_Send();
	}
}

void Pres_ShipExplosion(const vec3_t *pos, const vec3_t *vel, float scale, float time, int flames, int collision) {
	if (!Pres_Out() || !Pres_Begin(PRES_NEAR, pos, PRES_O_SHIPEXPL)) {
		return;
	}
	Pres_PutVec(pos);
	Pres_PutVec(vel);
	Pres_PutFloat(scale);
	Pres_PutFloat(time);
	Pres_PutInt(flames);
	Pres_PutInt(collision);
	Pres_Send();
}

void Pres_FxEnt(int kind, const char *a, const char *b, float f, int i, const vec3_t *v) {
	const vec3_t zero = { 0.0f, 0.0f, 0.0f };
	cEntity *e = World_FindEntity(b && *b && kind != PRES_FX_EMBEDDED ? b : a);
	vec3_t pos;
	if (!Pres_Out() || !e) {
		return;
	}
	/* the embedded effect is the entity's state from now on: a journal entry (a player that gets the entity later
	 * gets it with the entity) */
	if (kind == PRES_FX_EMBEDDED) {
		RepEnt_Note(e, REPENT_J_FX, b ? b : "");
		return;
	}
	/* D-Tritus' kill goes to his own player too (its client's punch hurts nobody) */
	int own = pres.own;
	if (kind == PRES_FX_KILL) {
		pres.own = -1;
	}
	byte begun = Pres_Begin(PRES_NEAR, Entity_GetPos(e, &pos), PRES_O_FXENT);
	pres.own = own;
	if (!begun) {
		return;
	}
	Pres_PutInt(kind);
	Pres_PutStr(a);
	Pres_PutStr(b);
	Pres_PutFloat(f);
	Pres_PutInt(i);
	Pres_PutVec(v ? v : &zero);
	Pres_Send();
}

void Pres_MsgText(const char *text, float time, const char *font) {
	if (!Pres_Out() || !Pres_Begin(PRES_PLAYER, NULL, PRES_O_MSGTEXT)) {
		return;
	}
	Pres_PutStr(text);
	Pres_PutFloat(time);
	Pres_PutStr(font);
	Pres_Send();
}

void Pres_LabelText(const char *text, float time) {
	if (!Pres_Out() || !Pres_Begin(PRES_PLAYER, NULL, PRES_O_LABELTEXT)) {
		return;
	}
	Pres_PutStr(text);
	Pres_PutFloat(time);
	Pres_Send();
}

void Pres_SpeechText(const char *text, float time, int r, int g, int b) {
	if (!Pres_Out() || !Pres_Begin(PRES_PLAYER, NULL, PRES_O_SPEECH)) {
		return;
	}
	Pres_PutStr(text);
	Pres_PutFloat(time);
	Pres_PutInt(r);
	Pres_PutInt(g);
	Pres_PutInt(b);
	Pres_Send();
}

void Pres_SpeechCancel(float time) {
	if (Pres_Out() && Pres_Begin(PRES_PLAYER, NULL, PRES_O_SPEECHCANCEL)) {
		Pres_PutFloat(time);
		Pres_Send();
	}
}

void Pres_Chat(const char *text, uint32_t color) {
	if (Pres_Out() && Pres_Begin(PRES_PLAYER, NULL, PRES_O_CHAT)) {
		Pres_PutStr(text);
		Pres_PutInt((int)color);
		Pres_Send();
	}
}

void Pres_HeadMonitor(const char *model, const char *name, int index) {
	if (Pres_Out() && Pres_Begin(PRES_PLAYER, NULL, PRES_O_HEAD)) {
		Pres_PutStr(model);
		Pres_PutStr(name);
		Pres_PutInt(index);
		Pres_Send();
	}
}

void Pres_Fade(const cScorer *sc, uint32_t from, uint32_t to, float time) {
	if (Pres_BeginHud(sc, PRES_O_FADE, 0)) {
		Pres_PutInt((int)from);
		Pres_PutInt((int)to);
		Pres_PutFloat(time);
		Pres_Send();
		NetScene_FadeSent(from, to, time);
	}
}

void Pres_Cinema(const cScorer *sc, byte on, float time) {
	if (Pres_BeginHud(sc, PRES_O_CINEMA, 0)) {
		Pres_PutInt(on);
		Pres_PutFloat(time);
		Pres_Send();
	}
}

void Pres_ActionSet(const char *name) {
	/* the world's context has no player whose input it is */
	if (Pres_Out() && Players_Current() != PLAYERS_NONE && !pres.skel && Pres_Begin(PRES_PLAYER, NULL,
			PRES_O_ACTIONSET)) {
		Pres_PutStr(name);
		Pres_Send();
	}
}

/* the world scorer's item of the list, NULL */
static PresWorldItem *Pres_WorldItem(const char *item) {
	for (int k = 0; item && k < pres.numWorldHud; k++) {
		if (!strcmp(pres.worldHud[k].item, item)) {
			return &pres.worldHud[k];
		}
	}
	return NULL;
}

static void Pres_WorldItemDrop(PresWorldItem *w) {
	int k = (int)(w - pres.worldHud);
	pres.numWorldHud--;
	memmove(w, w + 1, (size_t)(pres.numWorldHud - k) * sizeof(*w));
}

/* the records of an add and its sets: level marks an item of the world's scorer (the client's own clears keep it) */
static void Pres_PutHudAdd(const char *item, const char *type, byte append, byte level) {
	Pres_PutStr(item);
	Pres_PutStr(type);
	Pres_PutInt(append);
	NetRep_PutU8(&pres.w, level);
}

void Pres_HudAdd(const cScorer *sc, const char *item, const char *type, byte append) {
	byte world = sc == Scorer_GetWorld(0);
	if (Pres_Out() && !pres.skel && world && item && type) {
		PresWorldItem *w = Pres_WorldItem(item);
		if (w) {
			Pres_WorldItemDrop(w);
		}
		if (pres.numWorldHud < PRES_WORLD_HUD) {
			w = &pres.worldHud[pres.numWorldHud++];
			memset(w, 0, sizeof(*w));
			Str_Copy(w->item, item, sizeof(w->item));
			Str_Copy(w->type, type, sizeof(w->type));
			w->append = append;
		}
	}
	if (Pres_BeginHud(sc, PRES_O_HUDADD, 1)) {
		Pres_PutHudAdd(item, type, append, world);
		Pres_Send();
	}
}

void Pres_HudClear(const cScorer *sc) {
	if (Pres_Out() && sc == Scorer_GetWorld(0)) {
		pres.numWorldHud = 0;
	}
	if (Pres_BeginHud(sc, PRES_O_HUDCLEAR, 1)) {
		Pres_Send();
	}
}

void Pres_HudShow(const cScorer *sc, byte on) {
	if (Pres_BeginHud(sc, PRES_O_HUDSHOW, 1)) {
		Pres_PutInt(on);
		Pres_Send();
	}
}

void Pres_HudSet(const cScorer *sc, const char *item, const char *var, const char *s, double d) {
	PresWorldItem *w = Pres_Out() && sc == Scorer_GetWorld(0) && var ? Pres_WorldItem(item) : NULL;
	if (w && !crt_stricmp(var, "DelMe") && (s ? crt_atoi(s) : d) != 0) {
		Pres_WorldItemDrop(w);
	} else if (w) {
		int k = 0;
		while (k < w->nvars && strcmp(w->vars[k].name, var)) {
			k++;
		}
		if (k < PRES_WORLD_VARS) {
			w->nvars += k == w->nvars;
			Str_Copy(w->vars[k].name, var, sizeof(w->vars[k].name));
			w->vars[k].text = s != NULL;
			Str_Copy(w->vars[k].s, s ? s : "", sizeof(w->vars[k].s));
			w->vars[k].d = (float)d;
		}
	}
	if (Pres_BeginHud(sc, PRES_O_HUDSET, 1)) {
		Pres_PutStr(item);
		Pres_PutStr(var);
		Pres_PutInt(s ? 1 : 2);
		if (s) {
			Pres_PutStr(s);
		} else {
			Pres_PutFloat((float)d);
		}
		Pres_Send();
	}
}

/* ---- client ---- */

static uint32_t Pres_Hash(const char *s) {
	uint32_t h = 2166136261u;
	for (; *s; s++) {
		h = (h ^ (uint8_t)(*s | 0x20)) * 16777619u;
	}
	return h;
}

void Pres_ClientSound(const char *name) {
	if (!g_Mp2 || g_Instance || !g_Watch || !name || g_PresDerived) {
		return;
	}
	byte msg = !g_PresIn ? 0 : pres.recOp == PRES_O_SOUNDAT || pres.recOp == PRES_O_SOUND3D ||
		pres.recOp == PRES_O_SOUND2D ? 1 : 2;
	if (!msg) {
		NetAudit_Func(NA_SOUND, "pres.local:", g_SndSite ? g_SndSite : "?");
	}
	uint32_t h = Pres_Hash(name);
	uint64_t now = Plat_RealNs();
	/* the place: a record's own, the client's own entity's for its own sound (the same name from another character a
	 * few steps away, two falls at once, is no double) */
	vec3_t at = { 0.0f, 0.0f, 0.0f };
	byte hasPos = 0;
	cEntity *own = msg == 0 ? Players_Local() : NULL;
	if (msg == 1 && pres.recHasPos) {
		at = pres.recPos;
		hasPos = 1;
	} else if (own) {
		at = own->pos;
		hasPos = 1;
	}
	for (int k = 0; k < PRES_RECENT; k++) {
		const PresRecent *r = &pres.recent[k];
		float dx = r->pos.x - at.x, dy = r->pos.y - at.y, dz = r->pos.z - at.z;
		if (r->hash == h && r->msg != msg && r->ns && now - r->ns < (uint64_t)PRES_DOUBLE_MS * 1000000u &&
			(!hasPos || !r->hasPos || dx * dx + dy * dy + dz * dz <= PRES_DOUBLE_NEAR * PRES_DOUBLE_NEAR)) {
			NetAudit_Name(NA_SOUND, "pres.double:", name);
			break;
		}
	}
	PresRecent *r = &pres.recent[pres.nextRecent];
	pres.nextRecent = (pres.nextRecent + 1) % PRES_RECENT;
	r->hash = h;
	r->ns = now;
	r->msg = msg;
	r->hasPos = hasPos;
	r->pos = at;
}

cEntity *Pres_EventEnt(const char *name) {
	return g_Mp2 && !g_Instance && g_Watch ? ReplicaEnt_Find(name) : World_FindEntity(name);
}

byte Pres_ClientMissing(const char *what, const char *name) {
	char key[128];
	if (!g_Mp2 || g_Instance || !g_Watch) {
		return 0;
	}
	if (what) {
		Str_PrintfQuiet(key, sizeof(key), "%s:%s", what, name ? name : "");
		NetAudit_Name(NA_SOUND, "pres.missing:", key);
	}
	return 1;
}

static float Pres_GetFloat(NetRepReader *r) {
	uint32_t u = NetRep_GetU32(r);
	float f;
	memcpy(&f, &u, sizeof(f));
	return f;
}

static void Pres_GetVec(NetRepReader *r, vec3_t *v) {
	v->x = Pres_GetFloat(r);
	v->y = Pres_GetFloat(r);
	v->z = Pres_GetFloat(r);
}

static void Pres_Missing(int op, const char *name) {
	char key[96];
	if (op == PRES_O_FXENT) {
		Str_PrintfQuiet(key, sizeof(key), "%s%d:%s", pres_opNames[op], pres.fxKind, name);
	} else {
		Str_PrintfQuiet(key, sizeof(key), "%s:%s", pres_opNames[op], name);
	}
	NetAudit_Name(NA_SOUND, "pres.missing:", key);
}

/* an entity of a record: the replica of the instance's name; NULL counted */
static cEntity *Pres_Ent(int op, const char *name, const char *type) {
	cEntity *e = *name ? ReplicaEnt_Find(name) : NULL;
	if (!e || (type && !Entity_IsType(e, type))) {
		Pres_Missing(op, name);
		return NULL;
	}
	return e;
}

static void Pres_FxPlay(NetRepReader *r) {
	char a[256], b[256];
	int kind = NetRep_GetSVar(r);
	NetRep_GetStr(r, a, sizeof(a));
	NetRep_GetStr(r, b, sizeof(b));
	float f = Pres_GetFloat(r);
	int i = NetRep_GetSVar(r);
	vec3_t v;
	Pres_GetVec(r, &v);
	if (r->bad) {
		return;
	}
	cEntity *ea, *eb;
	pres.fxKind = kind;
	switch (kind) {
	case PRES_FX_CONVERSOR:
		if ((ea = Pres_Ent(PRES_O_FXENT, a, TYEN_WALKCHAR)) && (eb = Pres_Ent(PRES_O_FXENT, b, TYEN_WITH_LIFE))) {
			FX_CharacterConversor(ea, eb, i, 1.7f, 0.2f);
		}
		break;
	case PRES_FX_CONVERSION:
		if ((ea = Pres_Ent(PRES_O_FXENT, a, TYEN_WITH_LIFE)) && (eb = Pres_Ent(PRES_O_FXENT, b, TYEN_WITH_LIFE))) {
			FX_CharacterConversion(ea, eb, f);
		}
		break;
	case PRES_FX_SELLLIFE:
		ea = *a ? Pres_Ent(PRES_O_FXENT, a, TYEN_WITH_LIFE) : NULL;
		if ((ea || !*a) && (eb = Pres_Ent(PRES_O_FXENT, b, TYEN_WITH_LIFE))) {
			FX_BishopSellLife(ea, eb);
		}
		break;
	case PRES_FX_MONEY:
		if ((ea = Pres_Ent(PRES_O_FXENT, a, TYEN_WITH_LIFE)) && (eb = Pres_Ent(PRES_O_FXENT, b, TYEN_WITH_LIFE))) {
			FX_MoneyTransfer(ea, eb, f);
		}
		break;
	case PRES_FX_EMBEDDED:
		if ((ea = Pres_Ent(PRES_O_FXENT, a, NULL))) {
			Entity_SetEmbeddedFX(ea, b);
		}
		break;
	case PRES_FX_BISHOPRAY:
		eb = *b ? Pres_Ent(PRES_O_FXENT, b, TYEN_OBJECT) : NULL;
		if ((ea = Pres_Ent(PRES_O_FXENT, a, TYEN_WALKCHAR)) && (eb || !*b)) {
			FX_BishopRay(ea, eb, &v);
		}
		break;
	case PRES_FX_STEAL:
	case PRES_FX_SUCTION:
	case PRES_FX_FLASH:
	case PRES_FX_KILL:
		if ((ea = Pres_Ent(PRES_O_FXENT, a, TYEN_WITH_LIFE)) && (eb = Pres_Ent(PRES_O_FXENT, b, TYEN_WITH_LIFE))) {
			if (kind == PRES_FX_KILL) {
				FX_DtritusKill(ea, eb);
			} else if (kind == PRES_FX_STEAL) {
				FX_BankerSteal(ea, eb);
			} else if (kind == PRES_FX_SUCTION) {
				FX_BankMasterSuction(ea, eb);
			} else {
				FX_BertoFlash(ea, eb);
			}
		}
		break;
	case PRES_FX_POLICESTEAL:
		if ((ea = Pres_Ent(PRES_O_FXENT, a, TYEN_WALKCHAR)) && (eb = Pres_Ent(PRES_O_FXENT, b, TYEN_WALKCHAR))) {
			FX_PoliceSteal(ea, eb);
		}
		break;
	case PRES_FX_TAKE:
		if ((ea = Pres_Ent(PRES_O_FXENT, a, TYEN_WALKCHAR)) && (eb = Pres_Ent(PRES_O_FXENT, b, NULL))) {
			FX_MessengerTake(ea, eb);
		}
		break;
	case PRES_FX_ITEMFADE:
		eb = *b ? Pres_Ent(PRES_O_FXENT, b, TYEN_OBJECT) : NULL;
		if ((ea = Pres_Ent(PRES_O_FXENT, a, NULL)) && (eb || !*b)) {
			FX_ItemFade(ea, i, f, eb);
		}
		break;
	}
}

/* the HUD of the client's view 0 */
static void Pres_HudPlay(int op, NetRepReader *r) {
	char item[256], s[256];
	cScorer *sc = Scorer_Get(0);
	if (!sc) {
		return;
	}
	switch (op) {
	case PRES_O_HUDADD: {
		char type[64];
		NetRep_GetStr(r, item, sizeof(item));
		NetRep_GetStr(r, type, sizeof(type));
		int append = NetRep_GetSVar(r);
		byte level = (byte)NetRep_GetU8(r);
		cScorerItem *it = r->bad ? NULL : cScorer_AddItem(sc, item, type, append != 0);
		if (it) {
			it->level = level;
		}
		break;
	}
	case PRES_O_HUDCLEAR:
		cScorer_Clear(sc);
		Ui_ClientHudCleared();
		break;
	case PRES_O_HUDSHOW:
		sc->visible = NetRep_GetSVar(r) != 0;
		break;
	case PRES_O_HUDSET: {
		char var[64];
		NetRep_GetStr(r, item, sizeof(item));
		NetRep_GetStr(r, var, sizeof(var));
		int kind = NetRep_GetSVar(r);
		double d = 0.0;
		int i = 0;
		char *ps = s;
		if (kind == 1) {
			NetRep_GetStr(r, s, sizeof(s));
		} else {
			d = Pres_GetFloat(r);
			i = (int)d;
		}
		if (r->bad) {
			break;
		}
		cScorerItem *it = cScorer_FindItem(sc, item);
		if (!it) {
			Pres_Missing(op, item);
			break;
		}
		if (kind == 1) {
			ScorerItem_Var(it, var, 1, &ps, NULL, NULL);
		} else {
			ScorerItem_Var(it, var, 1, NULL, &d, &i);
		}
		break;
	}
	}
}

static void Pres_Play(int op, NetRepReader *r) {
	char name[256], s2[256];
	vec3_t pos, vel;
	switch (op) {
	case PRES_O_SOUNDAT: {
		NetRep_GetStr(r, name, sizeof(name));
		Pres_GetVec(r, &pos);
		int vol = NetRep_GetSVar(r);
		if (!r->bad) {
			pres.recPos = pos;
			pres.recHasPos = 1;
			SoundSys_PlayAt(g_pSound, name, &pos, vol);
			pres.recHasPos = 0;
		}
		break;
	}
	case PRES_O_SOUND3D: {
		cSoundDesc d;
		NetRep_GetStr(r, name, sizeof(name));
		Pres_GetVec(r, &pos);
		int vol = NetRep_GetSVar(r);
		float freq = Pres_GetFloat(r);
		cSoundDesc_Init(&d);
		d.param1 = Pres_GetFloat(r);
		d.param2 = Pres_GetFloat(r);
		d.param3 = Pres_GetFloat(r);
		if (r->bad) {
			break;
		}
		cSoundDesc_SetPos(&d, &pos);
		d.volume += vol;
		d.freq = freq;
		pres.recPos = pos;
		pres.recHasPos = 1;
		SoundSys_PlaySound(g_pSound, name, &d, NULL);
		pres.recHasPos = 0;
		break;
	}
	case PRES_O_ITEMSND: {
		NetRep_GetStr(r, name, sizeof(name));
		int item = NetRep_GetSVar(r);
		cEntity *e = r->bad ? NULL : Pres_EventEnt(name);
		if (e && Entity_IsType(e, TYEN_VEHICLE)) {
			Vehicle_StartSnd(e, item);
		} else if (!r->bad) {
			Pres_ClientMissing("itemsnd", name);
		}
		break;
	}
	case PRES_O_SOUND2D: {
		NetRep_GetStr(r, name, sizeof(name));
		int vol = NetRep_GetSVar(r), pan = NetRep_GetSVar(r);
		if (!r->bad) {
			SoundSys_Play(g_pSound, name, vol, pan);
		}
		break;
	}
	case PRES_O_VOICEOPEN: {
		NetRep_GetStr(r, name, sizeof(name));
		int voice = NetRep_GetSVar(r), arg = NetRep_GetSVar(r);
		if (!r->bad) {
			SoundSys_OpenVoice(g_pSound, name, voice, arg);
		}
		break;
	}
	case PRES_O_VOICEPLAY:
	case PRES_O_VOICESTOP: {
		int voice = NetRep_GetSVar(r);
		if (!r->bad) {
			if (op == PRES_O_VOICEPLAY) {
				SoundSys_PlayVoice(g_pSound, voice);
			} else {
				SoundSys_StopVoice(g_pSound, voice);
			}
		}
		break;
	}
	case PRES_O_MUSIC: {
		NetRep_GetStr(r, name, sizeof(name));
		NetRep_GetStr(r, s2, sizeof(s2));
		float vol = Pres_GetFloat(r);
		if (!r->bad && s2[0]) {
			Music_Play(name[0] ? name : NULL, s2, vol);
		}
		break;
	}
	case PRES_O_SHIPEXPL: {
		Pres_GetVec(r, &pos);
		Pres_GetVec(r, &vel);
		float scale = Pres_GetFloat(r), time = Pres_GetFloat(r);
		int flames = NetRep_GetSVar(r), coll = NetRep_GetSVar(r);
		if (!r->bad) {
			FX_ShipExplosion(pos, vel, scale, time, flames, coll);
		}
		break;
	}
	case PRES_O_FXENT:
		Pres_FxPlay(r);
		break;
	case PRES_O_MSGTEXT: {
		NetRep_GetStr(r, name, sizeof(name));
		float time = Pres_GetFloat(r);
		NetRep_GetStr(r, s2, sizeof(s2));
		if (!r->bad) {
			Scorer_SetMsgText(name, time, s2);
		}
		break;
	}
	case PRES_O_LABELTEXT: {
		NetRep_GetStr(r, name, sizeof(name));
		float time = Pres_GetFloat(r);
		if (!r->bad) {
			Scorer_SetLabelText(name, time);
		}
		break;
	}
	case PRES_O_SPEECH: {
		NetRep_GetStr(r, name, sizeof(name));
		float time = Pres_GetFloat(r);
		int cr = NetRep_GetSVar(r), cg = NetRep_GetSVar(r), cb = NetRep_GetSVar(r);
		if (!r->bad) {
			Scorer_SetSpeechText(name, time, cr, cg, cb);
		}
		break;
	}
	case PRES_O_SPEECHCANCEL: {
		float time = Pres_GetFloat(r);
		if (!r->bad) {
			Scorer_CancelSpeech(time);
		}
		break;
	}
	case PRES_O_CHAT: {
		NetRep_GetStr(r, name, sizeof(name));
		int color = NetRep_GetSVar(r);
		if (!r->bad) {
			Scorer_AddChatMsg(name, (uint32_t)color);
		}
		break;
	}
	case PRES_O_HEAD: {
		NetRep_GetStr(r, name, sizeof(name));
		NetRep_GetStr(r, s2, sizeof(s2));
		int index = NetRep_GetSVar(r);
		if (!r->bad) {
			Scorer_SetHeadMonitor(name, s2, index);
		}
		break;
	}
	case PRES_O_FADE: {
		uint32_t from = (uint32_t)NetRep_GetSVar(r), to = (uint32_t)NetRep_GetSVar(r);
		float time = Pres_GetFloat(r);
		cScorer *sc = Scorer_Get(0);
		if (!r->bad && sc) {
			ScorerFade_Start(&sc->fade, from, to, time);
			/* a held record played after its time: the fade as far as it is at the time drawn */
			sc->fade.time = sc->fade.time > pres_lag ? sc->fade.time - pres_lag : 0.0f;
		}
		break;
	}
	case PRES_O_CINEMA: {
		int on = NetRep_GetSVar(r);
		float time = Pres_GetFloat(r);
		cScorer *sc = Scorer_Get(0);
		if (!r->bad && sc && time >= 0.0f) {
			ScorerCinema_Set(&sc->cinema, on != 0, time);
			if (time > 0.0f) {
				sc->cinema.time = sc->cinema.time + pres_lag < sc->cinema.life ? sc->cinema.time + pres_lag : sc->cinema.life;
			}
		}
		break;
	}
	case PRES_O_GVAR: {
		NetRep_GetStr(r, name, sizeof(name));
		uint32_t kind = NetRep_GetU8(r);
		if (kind == 1) {
			NetRep_GetStr(r, s2, sizeof(s2));
		}
		float f = kind == 2 ? Pres_GetFloat(r) : 0.0f;
		if (!r->bad && (kind == 1 ? GVar_SetStr(name, s2) : GVar_SetNum(name, (double)f)) < 0) {
			Pres_Missing(op, name);
		}
		break;
	}
	case PRES_O_SAVEVAR:
		NetRep_GetStr(r, name, sizeof(name));
		NetRep_GetStr(r, s2, sizeof(s2));
		if (!r->bad) {
			SaveVar_Set(name, s2);
		}
		break;
	case PRES_O_CHARRES:
		NetRep_GetStr(r, name, sizeof(name));
		/* the class with its sounds as single player loads a class it lacks (Chars.LoadRes: nothing when the
		 * level's start has it) */
		if (!r->bad && (!Mode_Import("Chars") || !PORT_CALL(Chars, LoadRes, NULL, "s", name))) {
			Native_ErrPrint();
			Pres_Missing(op, name);
		}
		break;
	case PRES_O_ACTIONSET:
		NetRep_GetStr(r, name, sizeof(name));
		if (!r->bad && !Input_SelectActionSetNamed(name)) {
			Pres_Missing(op, name);
		} else if (!r->bad) {
			Ui_ClientControls(name);
		}
		break;
	case PRES_O_HUDADD:
	case PRES_O_HUDCLEAR:
	case PRES_O_HUDSHOW:
	case PRES_O_HUDSET:
		Pres_HudPlay(op, r);
		break;
	case PRES_O_TALK: {
		NetRep_GetStr(r, name, sizeof(name));
		NetRep_GetStr(r, s2, sizeof(s2));
		int arg = NetRep_GetSVar(r);
		if (!r->bad) {
			Sound_OpenTalk(name, s2, arg);
		}
		break;
	}
	case PRES_O_SCENE: {
		NetRep_GetStr(r, name, sizeof(name));
		NetRep_GetStr(r, s2, sizeof(s2));
		int tick = NetRep_GetSVar(r), flags = NetRep_GetSVar(r);
		if (!r->bad) {
			NetScene_ClientStart(name, s2, tick, flags);
		}
		break;
	}
	case PRES_O_SCENEEND:
		NetRep_GetStr(r, name, sizeof(name));
		if (!r->bad) {
			NetScene_ClientEnd(name);
		}
		break;
	case PRES_O_TIME:
		NetRep_GetSVar(r);
		break;
	case PRES_O_CAMVAR: {
		int kind;
		NetRep_GetStr(r, name, sizeof(name));
		kind = NetRep_GetSVar(r);
		if (kind) {
			NetRep_GetStr(r, s2, sizeof(s2));
		}
		float f = kind ? 0.0f : Pres_GetFloat(r);
		if (!r->bad) {
			if (Pres_Trace()) {
				Con_Printf("pres: camvar %s %s %.2f\n", name, kind ? s2 : "-", (double)f);
			}
			NetScene_ClientCamVar(name, kind ? s2 : NULL, f);
		}
		break;
	}
	case PRES_O_FXCALL: {
		static cPy_Function call;
		static uint8_t bytes[PRES_REC];
		NetRep_GetStr(r, name, sizeof(name));
		int n = (int)NetRep_GetU16(r);
		if (n > (int)sizeof(bytes)) {
			r->bad = 1;
			break;
		}
		CoordMsg_GetBytes(r, bytes, n);
		Str_PrintfQuiet(s2, sizeof(s2), "!%s", name);
		if (!r->bad && cPy_Function_SetFunc(&call, s2)) {
			cPy_Function_SetNativeBytes(&call, bytes, n);
			cPy_Function_Execute(&call, 0);
		} else if (!r->bad) {
			Pres_Missing(op, name);
		}
		break;
	}
	case PRES_O_CAMPATH: {
		static netScenePath_t path;
		NetRep_GetStr(r, name, sizeof(name));
		int n = NetRep_GetSVar(r);
		path.time = Pres_GetFloat(r);
		path.sent = NetRep_GetSVar(r) / 1000.0;
		path.n = n < 0 ? 0 : n > NETSCENE_PATH_POINTS ? NETSCENE_PATH_POINTS : n;
		for (int k = 0; k < path.n && !r->bad; k++) {
			NetRep_GetStr(r, path.p[k].cam, sizeof(path.p[k].cam));
			Pres_GetVec(r, &path.p[k].pos);
			Pres_GetVec(r, &path.p[k].ang);
			path.p[k].zNear = Pres_GetFloat(r);
			path.p[k].zFar = Pres_GetFloat(r);
			path.p[k].fov = Pres_GetFloat(r);
			path.p[k].time = Pres_GetFloat(r);
			path.p[k].quake = Pres_GetFloat(r);
			path.p[k].fadeIn = NetRep_GetSVar(r);
			path.p[k].fadeOut = NetRep_GetSVar(r);
		}
		if (!r->bad && n == path.n) {
			NetScene_ClientPath(name, &path);
		}
		break;
	}
	default:
		r->bad = 1;
		break;
	}
}

/* the effect record at r: 1 when the client has the entities it names; r moves past it */
static byte Pres_FxReady(NetRepReader *r) {
	char a[256], b[256];
	vec3_t v;
	NetRep_GetSVar(r);
	NetRep_GetStr(r, a, sizeof(a));
	NetRep_GetStr(r, b, sizeof(b));
	Pres_GetFloat(r);
	NetRep_GetSVar(r);
	Pres_GetVec(r, &v);
	return !r->bad && (!*a || ReplicaEnt_Find(a)) && (!*b || ReplicaEnt_Find(b));
}

/* the entity of a sound item record is here (a replica made a moment later than the server started the item) */
static byte Pres_ItemReady(NetRepReader *r) {
	char a[256];
	NetRep_GetStr(r, a, sizeof(a));
	NetRep_GetSVar(r);
	return !r->bad && ReplicaEnt_Find(a);
}

/* a record of op that waits for its entities: the effects and the sound items (0 for the other records) */
static byte Pres_Waits(int op, NetRepReader *r) {
	return op == PRES_O_FXENT ? !Pres_FxReady(r) : op == PRES_O_ITEMSND ? !Pres_ItemReady(r) : 0;
}

/* an effect or sound item record whose entity the client does not have yet (a replica made again under a new slot,
 * or made a moment later): held, 1; 0 when no place is free */
static byte Pres_Hold(int op, const uint8_t *rec, int size) {
	if (size > PRES_HOLD_REC) {
		return 0;
	}
	for (int k = 0; k < PRES_HOLDS; k++) {
		if (!pres.hold[k].ns) {
			memcpy(pres.hold[k].rec, rec, (size_t)size);
			pres.hold[k].size = size;
			pres.hold[k].op = op;
			pres.hold[k].ns = Plat_RealNs();
			pres.held++;
			return 1;
		}
	}
	return 0;
}

static void Pres_PlayRecord(int op, NetRepReader *r) {
	if (Pres_Trace()) {
		Con_Printf("pres: %s (server time %.3f, lag %.3f)\n", pres_opNames[op], pres_shown, (double)pres_lag);
	}
	pres.played[op]++;
	pres.recOp = op;
	if (op == PRES_O_MSGTEXT || op == PRES_O_LABELTEXT || op == PRES_O_SPEECH) {
		NetRepReader t = *r;
		NetRep_GetStr(&t, pres.lastText, sizeof(pres.lastText));
	}
	NetAudit_Func(NA_SOUND, "pres.play:", pres_opNames[op]);
	Pres_Play(op, r);
}

/* the messages that came while a scene of the server plays here, held for the interpolation delay: the scene's
 * entities are drawn that long behind the server, so what its lines present (a fade, an effect) plays at the scene's
 * time it played at on the server (include/scrap/netscene.h). A message with its server time (PRES_O_TIME first)
 * plays once that time is drawn, and its fades and cinema bars are as far as they are then (pres_lag); one that
 * starts a scene waits for its time as well */
#define PRES_LATE 32
#define PRES_LATE_MAX_NS 1000000000ull	/* a message with a time plays at most that long after its real due time */
static struct {
	uint64_t due;					/* Plat_RealNs when it plays (without a time) */
	double at;						/* the server time it plays at, < 0 none */
	int size;
	uint8_t data[PRES_BUF];
} pres_late[PRES_LATE];
static int pres_lateFirst, pres_lateN;

static void Pres_Play2(const uint8_t *data, int size);

/* the server time of a message (its PRES_O_TIME), < 0 none */
static double Pres_MessageTime(const uint8_t *data, int size) {
	NetRepReader r;
	if (size < 3 || data[1] != PRES_O_TIME) {
		return -1.0;
	}
	NetRepReader_Init(&r, data + 2, size - 2);
	int ms = NetRep_GetSVar(&r);
	return r.bad ? -1.0 : ms / 1000.0;
}

/* the held messages that are due (all with force) */
static void Pres_LateFlush(byte force) {
	uint64_t now = Plat_RealNs();
	while (pres_lateN) {
		int k = pres_lateFirst;
		double at = pres_late[k].at;
		byte due = at >= 0.0 && pres_shown >= 0.0 ? pres_shown >= at || now >= pres_late[k].due + PRES_LATE_MAX_NS :
			now >= pres_late[k].due;
		if (!force && !due) {
			break;
		}
		pres_lateFirst = (pres_lateFirst + 1) % PRES_LATE;
		pres_lateN--;
		pres_lag = at >= 0.0 && pres_shown > at ? (float)(pres_shown - at) : 0.0f;
		pres_lag = pres_lag > 0.5f ? 0.5f : pres_lag;
		Pres_Play2(pres_late[k].data, pres_late[k].size);
		pres_lag = 0.0f;
	}
}

void Pres_ClientShown(double rt) {
	pres_shown = Watch_ClockReady() ? rt : -1.0;
	Pres_LateFlush(0);
}

void Pres_Message(const uint8_t *data, int size) {
	double at = Pres_MessageTime(data, size);
	/* while some are held every message waits behind them (the order of the records stays) */
	if ((NetScene_ClientHolds() || pres_lateN || (at >= 0.0 && pres_shown >= 0.0 && at > pres_shown)) &&
		size <= PRES_BUF) {
		if (pres_lateN == PRES_LATE) {
			Pres_LateFlush(1);
		}
		int k = (pres_lateFirst + pres_lateN) % PRES_LATE;
		double d = NetScene_ClientHolds() ? Watch_Delay() : 0.0;
		pres_late[k].due = Plat_RealNs() + (uint64_t)(d > 0.0 ? d * 1e9 : 0.0);
		pres_late[k].at = at;
		pres_late[k].size = size;
		memcpy(pres_late[k].data, data, (size_t)size);
		pres_lateN++;
		return;
	}
	Pres_Play2(data, size);
}

static void Pres_Play2(const uint8_t *data, int size) {
	NetRepReader r;
	NetRepReader_Init(&r, data + 1, size - 1);
	g_PresIn++;
	while (!r.bad && r.pos < r.size) {
		int op = (int)NetRep_GetU8(&r);
		if (op <= 0 || op >= PRES_O_COUNT) {
			Con_Warnf("pres: unknown record %d\n", op);
			break;
		}
		if (op == PRES_O_FXENT || op == PRES_O_ITEMSND) {
			NetRepReader t = r;
			if (Pres_Waits(op, &t) && !t.bad && Pres_Hold(op, r.p + r.pos, t.pos - r.pos)) {
				r = t;
				continue;
			}
		}
		Pres_PlayRecord(op, &r);
	}
	g_PresIn--;
}

void Pres_ClientFrame(void) {
	uint64_t now = Plat_RealNs();
	Pres_LateFlush(0);
	NetScene_ClientFrame();
	for (int k = 0; k < PRES_HOLDS; k++) {
		if (!pres.hold[k].ns) {
			continue;
		}
		NetRepReader r, t;
		NetRepReader_Init(&r, pres.hold[k].rec, pres.hold[k].size);
		t = r;
		if (Pres_Waits(pres.hold[k].op, &t) && now - pres.hold[k].ns < PRES_HOLD_NS) {
			continue;
		}
		pres.hold[k].ns = 0;
		g_PresIn++;
		Pres_PlayRecord(pres.hold[k].op, &r);
		g_PresIn--;
	}
}
