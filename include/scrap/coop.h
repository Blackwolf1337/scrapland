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
 * instances). A chapter's way point (Map, Spawn, ComeFrom, PrevMap of the travel) goes into the story as
 * Coop.Entry.*; the target level's start takes them as the single player's start would have them, and a ComeFrom of
 * a door class ("DoorEditShip") is the door the members arrive in (its elevator ride). The level of the main menu is the end of the story: the single player's deletion of every save
 * variable before it is skipped, the story gets COORD_COOP_FINISHED, the members in the instance see the credits in
 * their main menu (COOP_Y_END: the client leaves the world), a member of a finished story that joins again starts in
 * the city (COORD_COOP_AFTER). The same level again (SaveGame.Restart of a super deal) starts the instance again for
 * its members: they travel to its new start (COORD_MSG_RESET).
 *
 * Cutscenes: a scene of the scripts (Scene.Play) whose table of lines (the "Scene.Load|<class>" call table of the
 * level, Mode_Calls, or the level data section that loaded it) was seen plays on every client in the instance: the instance sends the table and the class
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
 * Menus of the scripts: a question (Menu.YesNoMenu) goes to the member the scripts act for (COOP_Y_MENU); its client
 * shows it and sends the answer (COOP_C_ANSWER), the instance runs the function of the answer for that member. The
 * ship editor (ShipEdit.EnterEditShipMenu) opens on the member's client with the hangar of the story (its save
 * variables Hangar.* and the member's money): the client builds the ship on its platform and runs the function of
 * the answer that opened the editor (the Junkyard's tutorial), else the keypad's MapShipEdit.EnterEditShip; when it
 * closes, the hangar and the money come back, go into the story, the instance puts the ship on its platform and runs
 * the editor's callback of the scripts. Meanwhile the member's character is held (the scripts took its control:
 * no new character, Coop_Holds), the scripts' UsrControl 0 gives it back (Coop_UsrSlot), and the instance's time
 * never stops for a menu or a tutorial (Coop_KeepTime).
 *
 * A client plays only the scenes of its instance (its own scripts' Scene.Play does nothing there) and has a camera
 * of view 0 for its level's start; the coordinator tells it at its join that the world plays the coop story
 * (COORD_CUE_COOP).
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
#define COOP_Y_SCENE 1					/* module str, class str, count u16, lines (u16 length + bytes each), the data
										 * section that loads it (module str, section str; "" none) */
#define COOP_Y_SCENEEND 2				/* class str */
#define COOP_Y_SPEECH 3					/* text u16 length + bytes, time f32 (ms u32), r g b u8 */
#define COOP_Y_MSG 4					/* text u16 length + bytes, ms u32, font str */
#define COOP_Y_VOICE 5					/* channel u8, file str */
#define COOP_Y_NOTICE 6					/* key str, arg str (a text of include/scrap/texts.h) */
#define COOP_Y_OPTS 7					/* the arrow options of the entity the player would use: entity str, current u8,
										 * count u8 (0: none), the options ("name:text" str each) */
#define COOP_Y_END 8					/* the story is finished: the credits (the client leaves the world for the menu) */
#define COOP_Y_MENU 9					/* a menu of the scripts for this member: kind u8 (COOP_MENU_*), then
										 * YESNO: title u16 length + bytes; SHIPEDIT: the function of the answer
										 * that opened it str ("" none), money u32, the hangar's save variables
										 * (COOP_VARS) */
#define COOP_C_CHOICE 20				/* client -> instance (COOP_MSG, reliable): entity str, option u8 */
#define COOP_C_LEADER 21				/* client -> instance: to the party's leader (COORD_MSG_LEADER) */
#define COOP_LEADER_KEY 0x41			/* the client's key for it (DirectInput code: F7) */
#define COOP_C_ANSWER 22				/* client -> instance: the menu's answer: kind u8, then YESNO: yes u8; SHIPEDIT:
										 * money u32, the hangar's save variables (COOP_VARS) */
#define COOP_MENU_YESNO 1
#define COOP_MENU_SHIPEDIT 2
/* COOP_VARS: count u16, then name str and value (u16 length + bytes) each: the save variables Hangar.* (the instance
 * sends GameAct too) */
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
/* before the level's scripts start (level: its path): the story into the save variables, the way point of a
 * chapter's travel to this level (ComeFrom, PrevMap, Spawn, Map) into the player's variables its start reads */
void Coop_LevelStart(const char *level);
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
/* a data section of a level module runs (module NULL: it ended); Scene.Load(cls) in it: the clients run the same
 * section for that scene (a scene made from the level's .def data has no call table) */
void Coop_DataSection(const char *module, const char *section);
void Coop_SceneLoad(const char *cls);
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
/* a door's level change to level on a coop instance: 1 when it is a chapter of the story (into or out of a level of
 * the story alone: the whole party goes, told to the members) */
byte Coop_PartyTravel(const char *level);
/* where a member without an entrance starts: next to a member here, else at the single player's first spawn of
 * the level; 1 with *pos */
byte Coop_Spawn(int player, vec3_t *pos, float *yaw);
/* a scene plays here: the characters it hid are no lost players */
byte Coop_SceneHolds(void);
/* player i's character is held: a scene plays, or the ship editor is open on its client (the scripts took the
 * character's control, it gets no new one) */
byte Coop_Holds(int i);
/* the player slot a script gives a character (UsrControl): the scripts' 0..3 on a coop instance is the bound
 * member's (the single player's player 0) */
int Coop_UsrSlot(int slot);
/* the level has no room for a called ship (indoors): coop calls none there */
byte Coop_NoShipCall(void);
/* a player's new character: it selects what it looks at, as the single player's (the conversations need it) */
void Coop_Spawned(struct cEntity *e);
/* player i came into the instance: a scene that plays is sent to its client, its character hidden until the end */
void Coop_Joined(int i);
/* the use arrow of t runs on the instance for a player's character (not only a door's) */
byte Coop_UseAny(struct cEntity *t);
/* the scripts' Menu.YesNoMenu (1: it went to the member the scripts act for, its answer runs yes or no with id 0
 * and control "" here) and ShipEdit.EnterEditShipMenu (1: the editor opens on that member's client, the hangar it
 * built comes back into the story and the ship editor's callback runs here); the menus of a game without a member
 * stay as they are */
byte Coop_MenuYesNo(const char *title, const char *yes, const char *no);
byte Coop_ShipEditOpen(void);
/* the editor of the member the scripts act for is on its client (its tutorial runs there) */
byte Coop_ShipEditAway(void);
/* the instance's time keeps running for the other members (a menu or a tutorial of the scripts stops it in the single
 * player) */
byte Coop_KeepTime(void);
/* a client's message (COOP_MSG): 1 when taken */
byte Coop_Message(struct Net2Host *host, int conn, const uint8_t *data, int size);
/* player i uses the arrow of t: the option it chose on its client is the one that runs */
void Coop_BeforeUse(int i, struct cEntity *t);
/* test lines: "story" (the count, the mission), "story get <name>", "story set <name> <value>", "story inc <name>
 * <n>", "story hold 1|0" (the changes wait, go), "leader <player>"
 * (as the client's key), "travel <player> <level>" (alone, at the level's start), "qload <map>" and "restart" (as
 * the scripts' SaveGame.QLoad and SaveGame.Restart), "theend" (as the end's credits: all variables deleted, the menu),
 * "call <Module.Func|arg|...>" (a function of the scripts, as a line of a call table) */
byte Coop_Command(const char *line);

/* ---- client ---- */

/* a cue of the coordinator (COORD_CUE_*) */
void Coop_Cue(int kind, const char *text);
/* an instance's COOP_MSG: 1 when taken */
byte Coop_ClientMessage(const uint8_t *data, int size);
/* the instance connection is gone; left: the world too */
void Coop_ClientReset(byte left);
/* a level's scripts start on a client (before the level module's Init) */
void Coop_ClientLevelStart(void);
/* 1 when a Scene.Play of the client's own scripts does not play (a coop client plays the scenes of its instance) */
byte Coop_ClientNoScene(void);
/* the story is finished (COOP_Y_END came): 1 once, the join ends (Join_Stop: the credits of the main menu) */
byte Coop_ClientEnded(void);
/* the client keeps e, a ship its level scripts made (the ships of the instance's editor while it is open here) */
byte Coop_ClientKeeps(struct cEntity *e);
/* the ship editor of the instance closes here (before its callback): the hangar goes to the instance */
void Coop_ClientShipEdited(void);
/* Escape ends the scene here (before its EndOfScene) */
void Coop_ClientSceneAbort(void);
/* the scene ended here (EndOfScene of the client): an Escape before the instance's end skips it for this member alone
 * (its character comes back with the instance's end, so it does not walk off where the instance has none) */
void Coop_ClientSceneEnded(void);
/* each frame of a client: the options; tests: SCRAP_COOP_SKIP=<s> (Escape that many seconds into a scene),
 * SCRAP_COOP_CHOOSE=<k>[:<entity prefix>] (option k of a list), SCRAP_COOP_ANSWER=yes|no[:<s>] (a question's answer
 * after 2 s or s), SCRAP_COOP_SHIPEDIT=<s> (the ship editor opens with the lifeboat in the hangar and closes after
 * that many seconds) */
void Coop_ClientFrame(void);

#endif
