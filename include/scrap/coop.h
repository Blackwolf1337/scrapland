/*
 * The coop story of a world (stage M7a of the multiplayer plan, src/game/App/Coop.c): up to COORD_COOP_MAX players
 * play the single player story together as one party. Not in the original.
 *
 * Instance (ScrapC -instance:<Level> -mode:coop -story:<file>, started by a coordinator of mode coop,
 * include/scrap/coord.h): the save variables are the party's story. The file of -story is the party's story when the
 * instance starts (it goes into the save variables before the level's scripts run; a new party's empty story is the
 * single player's new game, SaveGame_CoopNewGame); the coordinator sends the whole story again when the instance
 * connects ('V') and every change another instance made ('v'). The changes the scripts make here go to the
 * coordinator once a tick (the names changed, with their values or as gone; last writer wins). A counter (Stats.*,
 * or a name SaveVar_Inc changed) goes as its addition since the last sync (COORD_OP_ADD): the coordinator adds it to
 * the story's number and sends the sum to every instance, an instance with an addition of its own not sent yet keeps
 * it on top of the sum, so two instances counting at once lose nothing. A few variables are a player's, not the
 * story's (where it is: Map, Spawn, ComeFrom, PrevMap; Char, AcBatteries, Money, Alarm, GameplayTime): they stay out.
 *
 * "The player" of a mission is the member that triggered it: the binding of the players (include/scrap/players.h)
 * runs the use of a mission entity, a scene and its callbacks with that member bound, so the items the scripts give
 * to "the player" go to it. In coop every use arrow of an entity (keypads, characters to talk to) runs on the
 * instance, not only the doors (Coop_UseAny).
 *
 * Mission level changes (SaveGame.QLoad and StartGame on an instance, Coop_QLoad) are travel through the coordinator
 * (include/scrap/travel.h): the bound player goes to the level at the spawn of its way point (the save variable
 * Spawn); a change into or out of a level of the story alone (Orbit, Fake, Junkyard, AB, ABIndoor) is a chapter of the
 * story and takes every member of the party along (COORD_SR_PARTY: the coordinator summons the others from their
 * instances). The level of the main menu is the end of the story: the single player's deletion of every save
 * variable before it is skipped, the story gets COORD_COOP_FINISHED, the members in the instance see the credits in
 * their main menu (COOP_Y_END: the client leaves the world), a member of a finished story that joins again starts in
 * the city (COORD_COOP_AFTER). The same level again (SaveGame.Restart of a super deal) starts the instance again for
 * its members: they travel to its new start (COORD_MSG_RESET).
 *
 * Cutscenes: a scene of the scripts (Scene.Play) whose table of lines (the "Scene.Load|<class>" call table of the
 * level, Mode_Calls) was seen plays on every client in the instance: the instance sends the table and the class
 * (COOP_Y_SCENE), the client runs the table and Scene.Play itself (the actors, the camera, the subtitles and voices
 * of the scene; no callback of the scripts), and ends it with the instance's end (COOP_Y_SCENEEND). The members in
 * other instances get a notice through the coordinator. Outside such a scene the speech texts of the scripts go to
 * the client of the bound player (the speaker of a conversation), their voices to it and to the members within
 * COOP_HEAR, the mission messages to all clients in the instance. After a scene the other members of the instance
 * stand next to the member of the scene. A member that comes in while a scene plays gets it too (its character
 * hidden like the others' until the end); Escape on a client ends its own copy only (its character comes back with
 * the instance's end).
 *
 * Conversation options: the instance sends each member the arrow options of the entity it would use (the character
 * it talks to, else the one in front of it) when they change (COOP_Y_OPTS); the client shows the option and its
 * next/previous action keys pick another (COOP_C_CHOICE); the member's use runs its choice (Coop_BeforeUse).
 *
 * Leader travel: a member's key (COOP_LEADER_KEY) asks the coordinator through its instance to go where the party's
 * leader plays (COORD_MSG_LEADER); the coordinator summons it there like a chapter travel of one member.
 *
 * Client: the coordinator's cue (COORD_MSG_CUE) of a new party's start plays the intro video before the first level
 * of the world; its notices show on the screen.
 */
#ifndef SCRAP_COOP_H
#define SCRAP_COOP_H

#include "scrap/mathlib.h"
#include "scrap/shared.h"

#define COOP_MSG 'Y'					/* instance -> client, reliable: kind u8 (COOP_Y_*), then its fields */
#define COOP_Y_SCENE 1					/* module str, class str, count u16, lines (u16 length + bytes each) */
#define COOP_Y_SCENEEND 2				/* class str */
#define COOP_Y_SPEECH 3					/* text u16 length + bytes, time f32 (ms u32), r g b u8 */
#define COOP_Y_MSG 4					/* text u16 length + bytes, ms u32, font str */
#define COOP_Y_VOICE 5					/* channel u8, file str */
#define COOP_Y_NOTICE 6					/* key str, arg str (a text of include/scrap/texts.h) */
#define COOP_Y_OPTS 7					/* the arrow options of the entity the player would use: entity str, current u8,
										 * count u8 (0: none), the options ("name:text" str each) */
#define COOP_Y_END 8					/* the story is finished: the credits (the client leaves the world for the menu) */
#define COOP_C_CHOICE 20				/* client -> instance (COOP_MSG, reliable): entity str, option u8 */
#define COOP_C_LEADER 21				/* client -> instance: to the party's leader (COORD_MSG_LEADER) */
#define COOP_LEADER_KEY 0x41			/* the client's key for it (DirectInput code: F7) */
#define COOP_HEAR 2500.0f				/* units: a member this near the speaker hears its conversation */
#define COOP_DIRTY 1024					/* changed names kept between two sends */
#define COOP_SCENES 64					/* scene tables kept */
#define COOP_NOTICE_MS 4000
#define COOP_OPTS 8						/* options of one arrow (ARROW_MAX_OPTS) */
#define COOP_OPT_TEXT 160

struct Net2Host;
struct cEntity;

/* ---- instance ---- */

/* -mode:coop and -story:<file>; 1 when taken (another -mode goes on to the factions) */
byte Coop_ParseParam(const char *param, const char *ext);
/* the instance plays the coop story; -mode:coop was given (a game that hosts: its world's mode) */
byte Coop_On(void);
byte Coop_Asked(void);
/* before the level's scripts start: the story into the save variables */
void Coop_LevelStart(void);
/* the scripts delete the save variables with prefix: 1 when they stay (all of them: the single player's end) */
byte Coop_KeepStory(const char *prefix);
/* a save variable was made (old NULL), changed or deleted by the game (SaveVars.c); inc: by SaveVar_Inc (a counter) */
void Coop_SaveVarChanged(const char *name, const char *old, byte inc);
/* each tick: the changes to the coordinator */
void Coop_Pass(void);
/* a message of the coordinator ('v', 'V', 'g'): 1 when taken */
byte Coop_CoordMessage(const uint8_t *data, int size);
/* the coordinator's connection is gone (the whole story comes again when it is back) */
void Coop_CoordLost(void);
/* SaveGame.QLoad(map) on an instance: 1 when it became the travel of the bound player (or its party), the end of
 * the story (the menu) or the level's restart for its members (the same level) */
byte Coop_QLoad(const char *map);
/* a call table of the scripts ran (Mode_Calls): a scene's is kept */
void Coop_SceneTable(const char *module, const char *const *lines);
/* Scene.Play(cls) started; EndOfScene of it begins (the member of the scene bound for its callback) and ended */
void Coop_ScenePlay(const char *cls);
/* the camera of view 0 a scene or a talk uses (the instance has none of its own): made when missing */
void Coop_SceneCamera(void);
void Coop_SceneEnding(void);
void Coop_SceneEnd(void);
/* the texts and voices of the scripts (TalkScorer.c, AbstractTextScorer.c, ScriptModes.c) */
void Coop_SpeechText(const char *text, float time, int r, int g, int b);
void Coop_MsgText(const char *text, float time, const char *font);
void Coop_OpenVoice(const char *file, int channel);
void Coop_PlayVoice(int channel);
/* where a member without an entrance starts: next to a member here, else at the single player's first spawn of
 * the level; 1 with *pos */
byte Coop_Spawn(int player, vec3_t *pos, float *yaw);
/* a scene plays here: the characters it hid are no lost players */
byte Coop_SceneHolds(void);
/* the level has no room for a called ship (indoors): coop calls none there */
byte Coop_NoShipCall(void);
/* a player's new character: it selects what it looks at, as the single player's (the conversations need it) */
void Coop_Spawned(struct cEntity *e);
/* player i came into the instance: a scene that plays is sent to its client, its character hidden until the end */
void Coop_Joined(int i);
/* the use arrow of t runs on the instance for a player's character (not only a door's) */
byte Coop_UseAny(struct cEntity *t);
/* a client's message (COOP_MSG): 1 when taken */
byte Coop_Message(struct Net2Host *host, int conn, const uint8_t *data, int size);
/* player i uses the arrow of t: the option it chose on its client is the one that runs */
void Coop_BeforeUse(int i, struct cEntity *t);
/* test lines: "story" (the count, the mission), "story get <name>", "story set <name> <value>", "story inc <name>
 * <n>", "story hold 1|0" (the changes wait, go), "leader <player>"
 * (as the client's key), "travel <player> <level>" (alone, at the level's start), "qload <map>" and "restart" (as
 * the scripts' SaveGame.QLoad and SaveGame.Restart), "theend" (as the end's credits: all variables deleted, the menu) */
byte Coop_Command(const char *line);

/* ---- client ---- */

/* a cue of the coordinator (COORD_CUE_*) */
void Coop_Cue(int kind, const char *text);
/* an instance's COOP_MSG: 1 when taken */
byte Coop_ClientMessage(const uint8_t *data, int size);
/* the instance connection is gone */
void Coop_ClientReset(void);
/* the scene ended here (EndOfScene of the client): an Escape before the instance's end skips it for this member alone
 * (its character comes back with the instance's end, so it does not walk off where the instance has none) */
void Coop_ClientSceneEnded(void);
/* each frame of a client: the options; tests: SCRAP_COOP_SKIP=<s> (Escape that many seconds into a scene),
 * SCRAP_COOP_CHOOSE=<k>[:<entity prefix>] (option k of a list) */
void Coop_ClientFrame(void);

#endif
