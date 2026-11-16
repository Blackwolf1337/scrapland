/*
 * The presentation interface (stage 3.4, -mp2), see include/scrap/pres.h: the records of the instance's presentation
 * calls to their audience, the client's side that plays them through the same sinks, the HUD models of the players,
 * the audit of calls without a route and of sounds played twice. Not in the original.
 */
#include <math.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/fx.h"
#include "scrap/fxmisc.h"
#include "scrap/game.h"
#include "scrap/input.h"
#include "scrap/logic.h"
#include "scrap/instance.h"
#include "scrap/net2.h"
#include "scrap/netaudit.h"
#include "scrap/netrep.h"
#include "scrap/objentity.h"
#include "scrap/platform.h"
#include "scrap/players.h"
#include "scrap/pres.h"
#include "scrap/replicate.h"
#include "scrap/repent.h"
#include "scrap/scorer.h"
#include "scrap/scrap.h"
#include "scrap/sound.h"
#include "scrap/world.h"

#include "../Global/testchannel.h"

#define PRES_BUF 4096				/* the records of one player before they are sent */
#define PRES_REC 1024				/* the longest record */
#define PRES_RECENT 32				/* one-shot sounds the client remembers for the double audit */

int g_PresIn;
int g_PresDerived;
const char *g_SndSite;
byte g_PresHud;

static const char *const pres_opNames[PRES_O_COUNT] = { "?", "soundat", "sound3d", "sound2d", "voiceopen", "voiceplay",
	"voicestop", "music", "shipexpl", "fxent", "msgtext", "labeltext", "speech", "speechcancel", "chat", "head", "fade",
	"cinema", "actionset", "hudadd", "hudclear", "hudshow", "hudset", "talk" };

typedef struct PresRecent {
	uint32_t hash;
	uint64_t ns;
	byte msg;						/* played from a record */
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
	vec3_t voiceFrom;
	byte haveVoiceFrom;
	const struct cSoundDesc *posDesc;	/* the description cSoundDesc_SetPos computed last, its place and volume */
	vec3_t pos;
	int posVolume;
	cScorer models[PLAYERS_MAX];
	byte made[PLAYERS_MAX];
	uint32_t sent[PRES_O_COUNT], bytes, cut;
	/* client */
	PresRecent recent[PRES_RECENT];
	int fxKind;						/* of the PRES_O_FXENT record played */
	char lastText[64];				/* the text of the last message, label or speech record (the test verb) */
	int nextRecent;
	uint32_t played[PRES_O_COUNT];
} pres = { .own = -1 };

/* ---- both ---- */

byte Pres_Out(void) {
	return g_Mp2 && g_Instance && !g_PresIn && !g_PresDerived;
}

void Pres_Status(tcBuf *b) {
	int models = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		models += pres.made[i];
	}
	TC_Addf(b, "pres: %s, %u bytes, %u cut, %d HUD models\n", !g_Mp2 ? "off" : g_Instance ? "instance" : "client",
		pres.bytes, pres.cut, models);
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
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (!Pres_Online(i)) {
			continue;
		}
		if (to == PRES_NEAR && pos) {
			const cPlayer *p = Players_Ctx(i);
			float dx = p->viewPos.x - pos->x, dy = p->viewPos.y - pos->y, dz = p->viewPos.z - pos->z;
			if (i == pres.own || dx * dx + dy * dy + dz * dz > NETREP_RADIUS * NETREP_RADIUS) {
				continue;
			}
		}
		m |= (uint64_t)1 << i;
	}
	return m;
}

byte Pres_Begin(presTo_t to, const vec3_t *pos, int op) {
	pres.to = Pres_Audience(to, pos);
	if (!pres.to) {
		return 0;
	}
	pres.op = op;
	NetRepBuf_Init(&pres.w, pres.rec, sizeof(pres.rec));
	NetRep_PutU8(&pres.w, (uint32_t)op);
	return 1;
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
		}
		memcpy(pres.buf[i] + pres.size[i], pres.rec, (size_t)pres.w.size);
		pres.size[i] += pres.w.size;
	}
}

void Pres_Flush(Net2Host *host) {
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (pres.size[i]) {
			Pres_FlushPlayer(host, i);
		}
	}
}

void Pres_OwnStep(int player) {
	pres.own = player;
}

static byte Pres_Skeleton(const char *module) {
	return !strcmp(module, "CharScorer") || !strcmp(module, "RacerScorer");
}

void Pres_ModCall(const char *module) {
	if (Pres_Skeleton(module)) {
		pres.skel++;
	}
}

void Pres_ModLeave(const char *module) {
	if (Pres_Skeleton(module) && pres.skel > 0) {
		pres.skel--;
	}
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

static void Pres_Unforwarded(const char *prefix, const char *site) {
	NetAudit_Func(NA_SOUND, prefix, site ? site : "?");
}

void Pres_SoundAt(const char *name, const vec3_t *pos, int volume) {
	if (!Pres_Out() || !Pres_Begin(PRES_NEAR, pos, PRES_O_SOUNDAT)) {
		return;
	}
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

void Pres_VoiceFrom(const vec3_t *pos) {
	pres.haveVoiceFrom = pos != NULL;
	if (pos) {
		pres.voiceFrom = *pos;
	}
}

/* a voice: to the players near its speaker, in a player's context to that player */
static byte Pres_BeginVoice(int op) {
	if (!Pres_Out()) {
		return 0;
	}
	if (Players_Current() == PLAYERS_NONE && pres.haveVoiceFrom) {
		return Pres_Begin(PRES_NEAR, &pres.voiceFrom, op);
	}
	return Pres_Begin(PRES_PLAYER, NULL, op);
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
	if (!Pres_Out() || !Pres_Begin(PRES_PARTY, NULL, PRES_O_MUSIC)) {
		return;
	}
	Pres_PutStr(name);
	Pres_PutStr(cmd);
	Pres_PutFloat(volume);
	Pres_Send();
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

void Pres_FxEnt(int kind, const char *a, const char *b, float f, int i) {
	cEntity *e = World_FindEntity(b && *b ? b : a);
	vec3_t pos;
	if (!Pres_Out() || !e) {
		return;
	}
	/* the embedded effect is the entity's state from now on: every player (one that comes into the interest later
	 * gets the entity's journal, not this) */
	if (!Pres_Begin(kind == PRES_FX_EMBEDDED ? PRES_LEVEL : PRES_NEAR, Entity_GetPos(e, &pos), PRES_O_FXENT)) {
		return;
	}
	Pres_PutInt(kind);
	Pres_PutStr(a);
	Pres_PutStr(b);
	Pres_PutFloat(f);
	Pres_PutInt(i);
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

void Pres_HudAdd(const cScorer *sc, const char *item, const char *type, byte append) {
	if (Pres_BeginHud(sc, PRES_O_HUDADD, 1)) {
		Pres_PutStr(item);
		Pres_PutStr(type);
		Pres_PutInt(append);
		Pres_Send();
	}
}

void Pres_HudClear(const cScorer *sc) {
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
	byte msg = g_PresIn > 0;
	if (!msg) {
		NetAudit_Func(NA_SOUND, "pres.local:", g_SndSite ? g_SndSite : "?");
	}
	uint32_t h = Pres_Hash(name);
	uint64_t now = Plat_RealNs();
	for (int k = 0; k < PRES_RECENT; k++) {
		const PresRecent *r = &pres.recent[k];
		if (r->hash == h && r->msg != msg && r->ns && now - r->ns < (uint64_t)PRES_DOUBLE_MS * 1000000u) {
			NetAudit_Name(NA_SOUND, "pres.double:", name);
			break;
		}
	}
	PresRecent *r = &pres.recent[pres.nextRecent];
	pres.nextRecent = (pres.nextRecent + 1) % PRES_RECENT;
	r->hash = h;
	r->ns = now;
	r->msg = msg;
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
	cEntity *e = *name ? World_FindEntity(name) : NULL;
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
		if (!r->bad) {
			cScorer_AddItem(sc, item, type, append != 0);
		}
		break;
	}
	case PRES_O_HUDCLEAR:
		cScorer_Clear(sc);
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
			SoundSys_PlayAt(g_pSound, name, &pos, vol);
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
		SoundSys_PlaySound(g_pSound, name, &d, NULL);
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
		}
		break;
	}
	case PRES_O_CINEMA: {
		int on = NetRep_GetSVar(r);
		float time = Pres_GetFloat(r);
		cScorer *sc = Scorer_Get(0);
		if (!r->bad && sc && time >= 0.0f) {
			ScorerCinema_Set(&sc->cinema, on != 0, time);
		}
		break;
	}
	case PRES_O_ACTIONSET:
		NetRep_GetStr(r, name, sizeof(name));
		if (!r->bad && !Input_SelectActionSetNamed(name)) {
			Pres_Missing(op, name);
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
	default:
		r->bad = 1;
		break;
	}
}

void Pres_Message(const uint8_t *data, int size) {
	NetRepReader r;
	NetRepReader_Init(&r, data + 1, size - 1);
	g_PresIn++;
	while (!r.bad && r.pos < r.size) {
		int op = (int)NetRep_GetU8(&r);
		if (op <= 0 || op >= PRES_O_COUNT) {
			Con_Warnf("pres: unknown record %d\n", op);
			break;
		}
		pres.played[op]++;
		if (op == PRES_O_MSGTEXT || op == PRES_O_LABELTEXT || op == PRES_O_SPEECH) {
			NetRepReader t = r;
			NetRep_GetStr(&t, pres.lastText, sizeof(pres.lastText));
		}
		NetAudit_Func(NA_SOUND, "pres.play:", pres_opNames[op]);
		Pres_Play(op, &r);
	}
	g_PresIn--;
}
