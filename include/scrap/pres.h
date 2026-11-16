/*
 * The presentation interface of the second multiplayer design (stage 3.4, -mp2; src/game/App/Pres.c). Not in the
 * original.
 *
 * The engine's presentation sinks route through it: the sound system's plays (positional, 3D with a description, 2D),
 * the voices (open, play, stop), the music player, the effects of the SFX module, the HUD's texts (message, label,
 * speech, chat, the head of the monitor), the scorer's operations (items added, cleared, shown, hidden, the scripts'
 * sets), the fade, the cinema bars and the action set. Single player, the old network path and a client do what they
 * did: the sink runs, nothing else (one test of g_Mp2). On a -mp2 instance the sink runs as before (the instance's
 * own state: the HUD model, the voice state the scripts read) and the call goes to its audience as a record of a
 * PRES_MSG message; the client plays the record through the same sink.
 *
 * The audience of a call:
 *   PRES_NEAR    the players whose view is within NETREP_RADIUS of a position (a sound, an effect); not the player
 *                whose own entity's step started it (the client steps its own entity and plays it itself)
 *   PRES_PLAYER  the current player's context (include/scrap/players.h); in the world's context every player
 *   PRES_PARTY   coop: every player of the instance; else PRES_PLAYER
 *   PRES_LEVEL   every player of the instance
 *
 * What a replica derives from replicated state is not sent: the sound items of entities (cSound, their variables are in
 * the journal), the positional sets (the client's level makes them), the presentation events of the action layers
 * (Event_IsPresent: their sounds and effects play on the replica) and the Present halves of the classes. Those run
 * inside Pres_Derive / Pres_Underive.
 *
 * HUD model per player: on a -mp2 instance SScorer's scorer 0 in a player's context is that player's own cScorer
 * (Scorer_Get, src/game/Scorer/Scorer.c), so the scripts' sets and read-backs (SScorer.Get, GetItem) see the
 * player's HUD; its operations go to that player alone. The world's context keeps the scorer of today. The client
 * builds the skeleton of its HUD itself (Play.c: CharScorer, RacerScorer): the instance does not send what those
 * modules make.
 *
 * Audit (include/scrap/netaudit.h, in the sink's category): on the instance "pres.unforwarded:<sink>:<site>" for a
 * call that has no route (a 3D sound whose position is unknown, a looping sound outside an entity's items),
 * "pres.sent:<op>" per record; on the client "pres.play:<op>", "pres.double:<name>" for a one-shot sound played from a
 * message and from the client's own state within PRES_DOUBLE_MS, "pres.missing:<op>:<name>" for a record whose
 * entity or item the client does not have (skipped, the game goes on).
 *
 * Wire (reliable, channel 0, instance to client): PRES_MSG, then records until the end: op u8 and its fields
 * (strings NetRep_PutStr, floats f32 as u32, ints svarint), see Pres.c.
 */
#ifndef SCRAP_PRES_H
#define SCRAP_PRES_H

#include "scrap/mathlib.h"
#include "scrap/shared.h"

#define PRES_MSG 'O'
#define PRES_DOUBLE_MS 250

typedef enum {
	PRES_NEAR,
	PRES_PLAYER,
	PRES_PARTY,
	PRES_LEVEL
} presTo_t;

enum {
	PRES_O_SOUNDAT = 1,				/* name, pos, volume: SoundSys_PlayAt */
	PRES_O_SOUND3D,					/* name, pos, volume over the position's, freq, p1 p2 p3: SoundSys_PlaySound */
	PRES_O_SOUND2D,					/* name, volume, pan: SoundSys_Play */
	PRES_O_VOICEOPEN,				/* file, voice, arg: SoundSys_OpenVoice */
	PRES_O_VOICEPLAY,				/* voice */
	PRES_O_VOICESTOP,				/* voice */
	PRES_O_MUSIC,					/* name ("" none), command, volume: Music_Play */
	PRES_O_SHIPEXPL,				/* pos, vel, scale, time, flames, collision: FX_ShipExplosion */
	PRES_O_FXENT,					/* kind, entity, entity, f, i, a place: the effects between two entities (the SFX
									 * module's, an item's fade, the bishop's ray) */
	PRES_O_MSGTEXT,					/* text, time, font */
	PRES_O_LABELTEXT,				/* text, time */
	PRES_O_SPEECH,					/* text, time, r g b */
	PRES_O_SPEECHCANCEL,			/* time */
	PRES_O_CHAT,					/* text, color */
	PRES_O_HEAD,					/* model, name, index: the head of the monitor */
	PRES_O_FADE,					/* from, to (packed colors), time */
	PRES_O_CINEMA,					/* on, time */
	PRES_O_ACTIONSET,				/* the set's name */
	PRES_O_HUDADD,					/* item, type, append */
	PRES_O_HUDCLEAR,
	PRES_O_HUDSHOW,					/* on */
	PRES_O_HUDSET,					/* item, var, kind u8 (1 text, 2 number), value */
	PRES_O_TALK,					/* file, speaker, arg: the talk voice (Sound_OpenTalk) */
	PRES_O_COUNT
};

/* the effects of PRES_O_FXENT (the SFX module) */
enum {
	PRES_FX_CONVERSOR = 1,			/* character, conversor, phase */
	PRES_FX_CONVERSION,				/* attacker, attacked, time */
	PRES_FX_SELLLIFE,				/* attacker ("" none), attacked */
	PRES_FX_MONEY,					/* from, to, time */
	PRES_FX_EMBEDDED,				/* entity, the effect's type (the embedded effect: state, Pres sends it to all) */
	PRES_FX_ITEMFADE,				/* item, taker ("" none), scale, mode: FX_ItemFade */
	PRES_FX_BISHOPRAY,				/* bishop, target ("" none), the point hit: FX_BishopRay */
	PRES_FX_STEAL,					/* thief, victim: FX_BankerSteal */
	PRES_FX_SUCTION,				/* bank master, victim: FX_BankMasterSuction */
	PRES_FX_FLASH,					/* Berto, victim: FX_BertoFlash */
	PRES_FX_TAKE					/* messenger, data pack: FX_MessengerTake */
};

struct cEntity;
struct cScorer;
struct cSoundDesc;
struct Net2Host;

/* >0: a sink runs for a routed call (the instance sending it, the client playing a record) */
extern int g_PresIn;
/* >0: a presentation a replica derives from replicated state runs */
extern int g_PresDerived;
/* a -mp2 instance: the scorer 0 of a player's context is that player's HUD model */
extern byte g_PresHud;

/* ---- both ---- */

/* a presentation a replica derives from state begins / ends (nested) */
static inline void Pres_Derive(void) {
	g_PresDerived++;
}
static inline void Pres_Underive(void) {
	g_PresDerived--;
}
/* the call is to be sent: a -mp2 instance, not inside a routed or derived call */
byte Pres_Out(void);
/* the test verb "pres": the counts of the records sent and played, the HUD models */
struct tcBuf;
void Pres_Status(struct tcBuf *b);

/* ---- instance ---- */

/* -mp2 on the instance: the HUD models on */
void Pres_Init(void);
/* a record to the audience: begun with the op (pos for PRES_NEAR), the fields with the Pres_Put* functions, then
 * Pres_Send; 0 when nobody is in the audience (nothing to put) */
byte Pres_Begin(presTo_t to, const vec3_t *pos, int op);
void Pres_PutStr(const char *s);
void Pres_PutInt(int v);
void Pres_PutFloat(float f);
void Pres_PutVec(const vec3_t *v);
void Pres_Send(void);
/* the records of the frame to the players' connections (Rep_Tick) */
void Pres_Flush(struct Net2Host *host);
/* player i's own entity steps (its client steps it too: what it starts is not sent to i); -1 after */
void Pres_OwnStep(int player);
/* a call of the scripts' module.func begins / ends (Port_Entry): the HUD skeleton modules the client makes itself */
void Pres_ModCall(const char *module);
void Pres_ModLeave(const char *module);
/* the HUD model of player i, made on its first use (i < 0: the current player's); NULL for none */
struct cScorer *Pres_HudModel(int i);
/* the HUD model of player i when it was made, NULL */
struct cScorer *Pres_HudMade(int i);
/* player i joined or left: its HUD model empty */
void Pres_HudReset(int i);
/* the player whose HUD model sc is, -1 for another scorer */
int Pres_HudOwner(const struct cScorer *sc);
/* a presentation sink without a route ran on the instance (the camera of view 0, the debris, the world's time
 * speed): counted as prefix site ("pres.unforwarded:<sink>:") unless inside a routed or derived call */
void Pres_NoRoute(int cat, const char *prefix, const char *site);
/* cSoundDesc_SetPos computed d at pos (the 3D sounds sent with their position) */
void Pres_SoundPos(const struct cSoundDesc *d, const vec3_t *pos);

/* the sinks (each at the head of the engine's function; returns at once unless -mp2) */
void Pres_SoundAt(const char *name, const vec3_t *pos, int volume);
void Pres_Sound3D(const char *name, const struct cSoundDesc *d);
void Pres_Sound2D(const char *name, int volume, int pan);
void Pres_VoiceOpen(const char *file, int voice, int arg);
void Pres_Talk(const char *file, const char *speaker, int arg);
void Pres_VoicePlay(int voice);
void Pres_VoiceStop(int voice);
/* the place a voice speaks from (Sound_PlayVoice: the speaker), for the voice calls that follow; NULL clears it */
void Pres_VoiceFrom(const vec3_t *pos);
void Pres_Music(const char *name, const char *cmd, float volume);
void Pres_ShipExplosion(const vec3_t *pos, const vec3_t *vel, float scale, float time, int flames, int collision);
void Pres_FxEnt(int kind, const char *a, const char *b, float f, int i, const vec3_t *v);
void Pres_MsgText(const char *text, float time, const char *font);
void Pres_LabelText(const char *text, float time);
void Pres_SpeechText(const char *text, float time, int r, int g, int b);
void Pres_SpeechCancel(float time);
void Pres_Chat(const char *text, uint32_t color);
void Pres_HeadMonitor(const char *model, const char *name, int index);
/* fade and cinema of a scorer: a HUD model's to its player, the world's scorer 0 to every player */
void Pres_Fade(const struct cScorer *sc, uint32_t from, uint32_t to, float time);
void Pres_Cinema(const struct cScorer *sc, byte on, float time);
void Pres_ActionSet(const char *name);
void Pres_HudAdd(const struct cScorer *sc, const char *item, const char *type, byte append);
void Pres_HudClear(const struct cScorer *sc);
void Pres_HudShow(const struct cScorer *sc, byte on);
/* a variable of an item of sc set by the scripts: text s, else the number d */
void Pres_HudSet(const struct cScorer *sc, const char *item, const char *var, const char *s, double d);

/* ---- client ---- */

/* a PRES_MSG message */
void Pres_Message(const uint8_t *data, int size);
/* a presentation event on a -mp2 client misses the entity name (what: the function; NULL: counted already): 1 counted
 * there ("pres.missing:<what>:<name>"; the caller returns), 0 elsewhere (the caller's CHECK stays) */
byte Pres_ClientMissing(const char *what, const char *name);
/* a one-shot sound plays on the client (the sound system's sinks): the double audit */
void Pres_ClientSound(const char *name);

#endif
