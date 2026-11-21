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
 *   PRES_NEAR    the players whose viewer (include/scrap/viewers.h) is within NETREP_RADIUS of a position (a sound,
 *                an effect); not the player whose own entity's step started it (the client steps its own entity and
 *                plays it itself)
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
	PRES_O_ITEMSND,					/* entity, item: a sound item of a ship its simulation starts (Pres_ItemSound) */
	PRES_O_SCENE,					/* name, entity, tick, flags: a scene starts (include/scrap/netscene.h) */
	PRES_O_SCENEEND,				/* name: it ends */
	PRES_O_CAMPATH,					/* a camera path of the scripts for the player (include/scrap/netscene.h) */
	PRES_O_FXCALL,					/* name, the parameter bytes: a C effect function the scripts call (Scrap.Execute) */
	PRES_O_GVAR,					/* name, kind u8 (1 text, 2 number), value: an engine variable (Pres_GVar) */
	PRES_O_CHARRES,					/* name: a character class the server's scripts loaded (Pres_CharRes) */
	PRES_O_TIME,					/* ms (svarint): the server time of the message's records (first record of a message
									 * to a player a scene plays for: its client plays them at that time drawn) */
	PRES_O_CAMVAR,					/* name, kind (svarint: 0 number, 1 entity name), value (f32 or str): a setting
									 * the scripts gave the player's follow camera (include/scrap/netscene.h) */
	PRES_O_SAVEVAR,					/* name, value: a save variable of the player its client reads (Pres_SaveVar) */
	PRES_O_COUNT
};

/* the effects of PRES_O_FXENT (the SFX module) */
enum {
	PRES_FX_CONVERSOR = 1,			/* character, conversor, phase */
	PRES_FX_CONVERSION,				/* attacker, attacked, time */
	PRES_FX_SELLLIFE,				/* attacker ("" none), attacked */
	PRES_FX_MONEY,					/* from, to, time */
	PRES_FX_EMBEDDED,				/* entity, the effect's type (the embedded effect: state, a journal entry of the
									 * entity, REPENT_J_FX, not a record) */
	PRES_FX_ITEMFADE,				/* item, taker ("" none), scale, mode: FX_ItemFade */
	PRES_FX_BISHOPRAY,				/* bishop, target ("" none), the point hit: FX_BishopRay */
	PRES_FX_STEAL,					/* thief, victim: FX_BankerSteal */
	PRES_FX_SUCTION,				/* bank master, victim: FX_BankMasterSuction */
	PRES_FX_FLASH,					/* Berto, victim: FX_BertoFlash */
	PRES_FX_TAKE,					/* messenger, data pack: FX_MessengerTake */
	PRES_FX_POLICESTEAL				/* police, victim: FX_PoliceSteal */
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
/* Pres_Begin for the players of a mask (bit i: player i; those without a connection left out) */
byte Pres_BeginMask(uint64_t mask, int op);
/* the records of the frame to the players' connections (Rep_Tick) */
void Pres_Flush(struct Net2Host *host);
/* player i's own entity steps (its client steps it too: what it starts is not sent to i); -1 after */
void Pres_OwnStep(int player);
/* a script of the simulation runs (an entity's OnDeath and OnDamage): the clients run none, so what it presents is
 * sent also inside a derived presentation (a missile's hit that kills) or a player's own step; Pres_SimScript gives
 * the state Pres_SimScriptEnd puts back */
typedef struct {
	int derived, own;
} presSaved_t;
presSaved_t Pres_SimScript(void);
void Pres_SimScriptEnd(presSaved_t saved);
/* a call of the scripts' module.func begins / ends (Port_Entry): the HUD skeleton modules the client makes itself;
 * Pres_ModSite is the innermost call running ("module.func", "-" outside the scripts) for the audit keys */
void Pres_ModCall(const char *module, const char *func);
/* the mission's part of a HUD skeleton (Menu.MissionScorerCall: a race's timer and scores) goes to the player: the
 * skeleton's suppression paused (the depth for Pres_SkelResume), resumed */
int Pres_SkelPause(void);
void Pres_SkelResume(int depth);
/* the scripts drew the HUD of mode ("Walk", "Racer") for the current player (Menu.DrawScorer): its client builds the
 * skeleton of that mode again after the instance's clear, before the mission's part comes */
void Pres_HudMode(const char *mode);
void Pres_ModLeave(const char *module);
const char *Pres_ModSite(void);
/* the HUD model of player i, made on its first use (i < 0: the current player's); NULL for none */
struct cScorer *Pres_HudModel(int i);
/* the HUD model of player i when it was made, NULL */
struct cScorer *Pres_HudMade(int i);
/* player i joined or left: its HUD model, its talk and its voice models empty */
void Pres_HudReset(int i);
/* the player whose HUD model sc is, -1 for another scorer */
int Pres_HudOwner(const struct cScorer *sc);
/* the voice models (stage 4.3): on a -mp2 instance every player's context and the world's have voices of their own
 * that play nothing; the voice functions of the sound system (Sound.c) answer from them, so a player's talk times
 * (VoiceRemain: the file's length less the time it has played) and its talk voice's checks are its own and equal single
 * player's. Each returns -1 without the models (single player, a client), else the voice function's result */
int Pres_VoxOpen(const char *name, int voice);
int Pres_VoxPlay(int voice);
int Pres_VoxStop(int voice);
int Pres_VoxRemain(int voice, float *remain);
int Pres_VoxSetString(int voice, const char *text);
int Pres_VoxGetString(int voice, char *buf, int size);
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
void Pres_Music(const char *name, const char *cmd, float volume);
/* player i joined: the world's music (its volume and the last file played outside a player's context, the level's
 * own music of its Map.Init) as records to that player */
void Pres_Joined(int i);
/* the scripts set the engine variable name (Native_GVarStore): one the gvar table tags STATE or WORLD_SETTING goes to
 * the players as a record, in the world's context to every player and kept for those that join later, in a player's
 * context to that player */
void Pres_GVar(const char *name);
/* the server's scripts loaded the character class name (Chars.LocalLoadRes: the class, its sounds and their engine
 * variables): every player and those that join later load it too (Chars.LoadRes) */
void Pres_CharRes(const char *name);
/* player i's own save variable name changed on the instance (SaveVars.c): one the client reads itself (the lives of
 * its HUD) goes to its client; also at its join */
void Pres_SaveVar(int i, const char *name, const char *value);
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

/* a -mp2 instance: the simulation of ship e starts its sound item (Vehicle_StartSnd, Weap_PlaySound: the boost, a
 * crash, a weapon's shot or launch, the weapon change), which its replica does not step: to the players near (not the
 * one whose own step it is); the client starts the item on its replica. Inside a derived presentation (the item
 * plays the replica makes itself) nothing goes. */
void Pres_ItemSound(struct cEntity *e, int item);

/* Scrap.Execute of the scripts' function f: on a -mp2 instance a C effect function (a name "FX...", as the
 * presentation events of include/scrap/actions.h) goes to the current context's players as a record (their client
 * calls it with the same parameters); 1 then, and the caller runs it inside Pres_Derive */
struct cPy_Function;
byte Pres_NativeCall(struct cPy_Function *f);

/* the census of the network tests (audit category NA_CENSUS): "<what>.name:<name>" for every sample played and
 * debris system dropped, on an instance also "<what>.near:<name>" when it is near a player's viewer (pos NULL: a play
 * without a place, near); Pres_CensusSound takes the place of a positional play from its description */
void Pres_Census(const char *what, const char *name, const vec3_t *pos);
void Pres_CensusSound(const char *name, const struct cSoundDesc *d, const vec3_t *pos);

/* a PRES_MSG message */
void Pres_Message(const uint8_t *data, int size);
/* each frame: the effect records held until the client has their entities (a replica made again under a new slot),
 * played once it has them, else after a second (counted missing then) */
void Pres_ClientFrame(void);
/* each frame after the server time drawn is known (rt, s): the held messages of that time play */
void Pres_ClientShown(double rt);
/* a presentation event on a -mp2 client misses the entity name (what: the function; NULL: counted already): 1 counted
 * there ("pres.missing:<what>:<name>"; the caller returns), 0 elsewhere (the caller's CHECK stays) */
byte Pres_ClientMissing(const char *what, const char *name);
/* the entity of a name a presentation event names (another than its own): on a -mp2 client the replica of the
 * instance's name or the own entity for the own slot's (ReplicaEnt_Find), elsewhere World_FindEntity */
struct cEntity *Pres_EventEnt(const char *name);
/* a one-shot sound plays on the client (the sound system's sinks): the double audit */
void Pres_ClientSound(const char *name);

#endif
