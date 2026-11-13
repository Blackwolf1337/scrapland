/*
 * The coop story of a world (stage M7a of the multiplayer plan, src/game/App/Coop.c): up to COORD_COOP_MAX players
 * play the single player story together as one party. Not in the original.
 *
 * Instance (ScrapC -instance:<Level> -mode:coop -story:<file>, started by a coordinator of mode coop,
 * include/scrap/coord.h): the save variables are the party's story. The file of -story is the party's story when the
 * instance starts (it goes into the save variables before the level's scripts run; a new party's empty story is the
 * single player's new game, SaveGame_CoopNewGame); the coordinator sends the whole story again when the instance
 * connects ('V') and every change another instance made ('v'). The changes the scripts make here go to the
 * coordinator once a tick (the names changed, with their values or as gone). A few variables are a player's, not the
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
 * instances). The level of the main menu (the end of the story) and the same level again are refused for now.
 *
 * Cutscenes: a scene of the scripts (Scene.Play) whose table of lines (the "Scene.Load|<class>" call table of the
 * level, Mode_Calls) was seen plays on every client in the instance: the instance sends the table and the class
 * (COOP_Y_SCENE), the client runs the table and Scene.Play itself (the actors, the camera, the subtitles and voices
 * of the scene; no callback of the scripts), and ends it with the instance's end (COOP_Y_SCENEEND). The members in
 * other instances get a notice through the coordinator. Outside such a scene the speech texts of the scripts go to
 * the client of the bound player (the speaker of a conversation), their voices to it and to the members within
 * COOP_HEAR, the mission messages to all clients in the instance. After a scene the other members of the instance
 * stand next to the member of the scene.
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
#define COOP_HEAR 2500.0f				/* units: a member this near the speaker hears its conversation */
#define COOP_DIRTY 1024					/* changed names kept between two sends */
#define COOP_SCENES 64					/* scene tables kept */
#define COOP_NOTICE_MS 4000

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
/* a save variable was made, changed or deleted by the game (SaveVars.c) */
void Coop_SaveVarChanged(const char *name);
/* each tick: the changes to the coordinator */
void Coop_Pass(void);
/* a message of the coordinator ('v', 'V', 'g'): 1 when taken */
byte Coop_CoordMessage(const uint8_t *data, int size);
/* the coordinator's connection is gone (the whole story comes again when it is back) */
void Coop_CoordLost(void);
/* SaveGame.QLoad(map) on an instance: 1 when it became the travel of the bound player (or its party) */
byte Coop_QLoad(const char *map);
/* a call table of the scripts ran (Mode_Calls): a scene's is kept */
void Coop_SceneTable(const char *module, const char *const *lines);
/* Scene.Play(cls) started; EndOfScene of it begins (the member of the scene bound for its callback) and ended */
void Coop_ScenePlay(const char *cls);
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
/* the use arrow of t runs on the instance for a player's character (not only a door's) */
byte Coop_UseAny(struct cEntity *t);
/* test lines: "story" (the count, the mission), "story get <name>", "story set <name> <value>" */
byte Coop_Command(const char *line);

/* ---- client ---- */

/* a cue of the coordinator (COORD_CUE_*) */
void Coop_Cue(int kind, const char *text);
/* an instance's COOP_MSG: 1 when taken */
byte Coop_ClientMessage(const uint8_t *data, int size);
/* the instance connection is gone */
void Coop_ClientReset(void);

#endif
