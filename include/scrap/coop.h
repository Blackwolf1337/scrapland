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
 * runs the use of a mission entity, a scene and its callbacks in that member's context, so the items the scripts give
 * to "the player" go to it. In coop every use arrow of an entity (keypads, characters to talk to) runs on the
 * instance, not only the doors (Coop_UseAny).
 *
 * Mission level changes (SaveGame.QLoad and StartGame on an instance, Coop_QLoad) are travel through the coordinator
 * (include/scrap/travel.h): the current player goes to the level at the spawn of its way point (the save variable
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
 * the client of the current player (the speaker of a conversation), their voices to it and to the members within
 * COOP_HEAR, the mission messages to all clients in the instance. After a scene the other members of the instance
 * stand next to the member of the scene. A member that comes in while a scene plays gets it too (its character
 * hidden like the others' until the end); Escape on a client ends its own copy only (its character comes back with
 * the instance's end).
 *
 * Conversation options: the instance sends each member the arrow options of the entity it would use (the character
 * it talks to, else the one in front of it) when they change (COOP_Y_OPTS); the client shows the option in its talk
 * panel and its next/previous action keys pick another (COOP_C_CHOICE); the member's use runs its choice
 * (Coop_BeforeUse).
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
 * The character conversor's interface (at the middle of the machine's action) opens on the client of the member in
 * the machine; the machine waits for its pick, the conversion runs on the instance for that member. Sputnik's hangar
 * screen opens on the member's client like the ship editor.
 *
 * Possession and conversion: the member that possesses a character (the special action key at it, its client's
 * input) or converts at a conversor plays as the new character from then on, as the single player's player does:
 * the scripts' new main character gets the member's slot and the instance gives it to the member (Coop_Adopts), its
 * class stays the member's (its state's "char": a respawn and a travel keep it). The other members stay what they
 * are: they see the possessed character die, the member's old character go and the new one take its place, the alarm
 * of the possession is the possessor's own; the story goes on for all (the conversion back into Dtritus that
 * Mission_MeetBoss asks for counts by whichever member does it). Scripts that read a member's slot (UsrControl) see
 * the single player's 0 (Coop_UsrRead: a race's checkpoints count for any member's ship); a character or ship the
 * scripts take from a member (UsrControl -1: a ride, a race's camera travelling) is held and given back to it.
 *
 * A level's start of the missions (MissionsFuncs.Init: an outdoor level's challenge race, the mission arrow) without
 * a member yet has a stand-in player at the spawn of the way point (the mission's travel carries it in the story as
 * Coop.Entry.*, like a chapter's), where the single player's player would stand.
 *
 * A client plays only the scenes of its instance (its own scripts' Scene.Play does nothing there) and has a camera
 * of view 0 for its level's start; the coordinator tells it at its join that the world plays the coop story
 * (COORD_CUE_COOP).
 *
 * Mission targets: the instance tells its clients the targets of the mission (MissionsFuncs.currentTarget and
 * targetsList) when they change and every COOP_TARGETS_MS; a client sets them on its own copies (the level's entity
 * of that name, the proxy of a replicated one) as the single player's MissionsFuncs does: the radar marks and the
 * mission arrow.
 *
 * The variables of the story the HUD of the single player shows (EnergyBarActive, SpecialActionActive: the energy
 * bar and the special action) go to the clients at their join and when they change (COOP_Y_HUDVAR).
 *
 * Leader travel: a member's key (Walk.CoopLeader of the controls, F7) asks the coordinator through its instance to
 * go where the party's leader plays (COORD_MSG_LEADER); the coordinator summons it there like a chapter travel of one
 * member.
 *
 * Money: the party has one purse, as the single player has one (the story's counter Coop.Money). Every member's money
 * on the instances is that purse: what a member earns (a reward, a coin) or spends (the clues of the police boss, a
 * ship editor) since the last tick goes into the counter (COORD_OP_ADD, so two instances at once lose nothing) and
 * every member gets the sum. A story without the purse (a world of before) takes the money of its first member. The
 * member's own money that the coordinator keeps (the other modes' per player money) follows the purse.
 *
 * Remote communications (Speech.ForceRemoteComunication: a call with the caller's head on the monitor) go to the
 * mission's member (the current one when the story's mission changed, else the first member that came into the instance
 * while it was gone) and to the members within COOP_HEAR of it, with the caller's head (COOP_Y_HEAD); the scripts act
 * for that member during the call.
 *
 * The objects the instance's level start deletes (a mission's Init removes what the level made; the client's level
 * start runs no mission) go from the clients' levels too: their names go to a member's client at its join
 * (COOP_Y_GONE), the client deletes its own entities of those names (not the proxies of the instance's).
 *
 * Death and a failed mission: a member that dies gets a new character (the party plays on; single player's death is
 * the end of the game and the way point's reload, and the member's death costs what the instance's death costs: the
 * alarm off). When every member in the instance is dead, the single player's death runs: the mission's hook
 * (Chars.OnMainCharDeath: KillPolice's, the final fight's boss that heals), else the end of the game. The end of the
 * game (MissionsFuncs.FinishGame: such a hook, or a mission's own failure like ProtectMayorTransport's) is the
 * mission failed in coop: the members are told (MP_CoopMissionFailed) and after CP_FAIL_SECS (the single player's
 * restart after its game over) the level starts again for them at the way point the mission set (the mission's start
 * point; the same level's restart, COORD_MSG_RESET); the story stays as it is, as the single player's reload keeps
 * the save variables.
 *
 * Client: the coordinator's cue (COORD_MSG_CUE) of a new party's start plays the intro video before the first level
 * of the world; its notices show on the screen.
 *
 * The second multiplayer design (-mp2, stage 5 of plan mp2): the instance keeps the story's sync, the party and the
 * travel above (Coop.c); "the player" of the scripts in the world's context is the party by the rules of
 * include/scrap/party.h and data/coop/rules.def; the texts, voices, heads, fades, HUD items, targets' marks and
 * cutscenes go by include/scrap/pres.h and NetScene.c, the menus, the ship editor, Sputnik's screen and the conversor by
 * the UI channel (include/scrap/uichan.h), the answers of a talk are the member's input (Speech.c); the old path's
 * forwarding and menus of this comment (CoopOld.c, CoopClient.c) stay quiet there. Files: Coop.c (story, party,
 * travel), CoopOld.c and CoopClient.c (the old path, deleted with it), CoopTest.c (the automatic test player),
 * src/game/App/CoopInt.h (their shared state).
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
										 * (COOP_VARS); CONV, SPUTNIK: see there */
#define COOP_Y_FADE 10					/* a fade of the scripts' screen 0: from r g b a u8, to r g b a u8, ms u32 */
#define COOP_Y_VIDEO 11					/* the video the scripts set before a mission's level change (PlayVideo): file
										 * str; the client plays it before the next level */
#define COOP_Y_TARGETS 12				/* the mission's targets (MissionsFuncs): count u8, then name str (the
										 * instance's) and replication slot u16 (NETREP_NO_SLOT: a level entity of the
										 * same name on the client) each */
#define COOP_Y_HUDVAR 13				/* a variable of the story the client's HUD shows (the energy bar, the special
										 * action): name str, value str */
#define COOP_Y_SCORER 14				/* the mission's items of the HUD (the race's scores, the timers, the boss' bar):
										 * the four timers' times and CounterType (str each), count u8, then name str,
										 * type str, count u8 and its variables (name str, kind u8: 1 text, 2 number,
										 * 3 integer; value str) each */
#define COOP_Y_STORY 15					/* the client's copy of the party's story (its level starts: the press'
										 * blueprints): count u16, then the changes as COORD_MSG_STORY's */
#define COOP_Y_HEAD 16					/* the caller's head of a remote communication (the monitor): model str, name
										 * str, index u8 */
#define COOP_Y_GONE 17					/* the entities the instance's level start deleted (a mission's Init; the client's
										 * level start runs no mission): count u16, then name str each */
#define COOP_SCORER_ITEMS 24			/* mission items told */
#define COOP_SCORER_MS 250				/* the items looked at this often, sent when they changed */
#define COOP_TARGETS 16					/* targets told (a mission's list of targets) */
#define COOP_TARGETS_MS 2000			/* the targets again (a proxy that came into the interest since) */
#define COOP_C_CHOICE 20				/* client -> instance (COOP_MSG, reliable): entity str, option u8 */
#define COOP_C_LEADER 21				/* client -> instance: to the party's leader (COORD_MSG_LEADER) */
#define COOP_C_ANSWER 22				/* client -> instance: the menu's answer: kind u8, then YESNO: yes u8; SHIPEDIT:
										 * money u32, the hangar's save variables (COOP_VARS) */
#define COOP_MENU_YESNO 1
#define COOP_MENU_SHIPEDIT 2
#define COOP_MENU_CONV 3				/* the character conversor: Y_MENU: the member's class str, the available classes
										 * (Conversor.AvailableChars, u16 length + bytes); C_ANSWER: the class picked
										 * str ("" none: Escape) */
#define COOP_MENU_SPUTNIK 4				/* Sputnik's hangar screen: Y_MENU and C_ANSWER: money u32, COOP_VARS */
#define COOP_MENU_WARNING 5				/* a text box of the scripts (Menu.ShowWarning): Y_MENU: the text u16 length +
										 * bytes; no answer (the client closes it) */
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
struct NetRepInput;
struct NetRepBuf;
struct NetRepReader;
struct nativeVal_s;

/* ---- instance ---- */

/* -mode:coop and -story:<file>; 1 when taken (another -mode goes on to the factions) */
byte Coop_ParseParam(const char *param, const char *ext);
/* the instance plays the coop story; -mode:coop was given (a game that hosts: its world's mode) */
byte Coop_On(void);
byte Coop_Asked(void);
/* before the level's scripts start (level: its path): the story into the save variables, the way point of a
 * chapter's travel to this level (ComeFrom, PrevMap, Spawn, Map) into the player's variables its start reads */
void Coop_LevelStart(const char *level);
/* a coop level that starts with its own character (GDB: the bishop, MissionMaker_Char): Char for the level's start of
 * an instance (its members' characters are theirs) and of a member's client, which comes with its own (the leader's
 * level joined as D-Tritus): Chars.Init loads its scripts before the map's InitStatus makes the player the bishop */
void Coop_LevelChar(const char *level);
/* -mp2: the character the level's start made the player (the world's Char: GDB's bishop, ABIndoor's D-Tritus) is the
 * character of member i that enters the level (its own Char, read by its player steps), as every single player's
 * arrival there; nothing when the level's start set none */
void Coop_EnterChar(int i);
/* the scripts' SaveVar_Set("Char", cls) of "the player" (a level's start: GDB's bishop, ABIndoor's D-Tritus; a mission's
 * end): -mp2 coop: the class every member carries (its state's char) and, in the world's context, the character of
 * the members that enter later (Coop_EnterChar); single player: the save variable only */
void Coop_SetChar(const char *cls);
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
/* SaveGame.QLoad(map) on an instance: 1 when it became the travel of the current player (or its party), the end of
 * the story (the menu) or the level's restart for its members (the same level) */
byte Coop_QLoad(const char *map);
/* a call table of the scripts ran (Mode_Calls): a scene's is kept */
void Coop_SceneTable(const char *module, const char *const *lines);
/* a data section of a level module runs (module NULL: it ended); Scene.Load(cls) in it: the clients run the same
 * section for that scene (a scene made from the level's .def data has no call table) */
void Coop_DataSection(const char *module, const char *section);
void Coop_SceneLoad(const char *cls);
/* Scene.Play(cls) started; EndOfScene of it begins (the member of the scene current for its callback) and ended */
void Coop_ScenePlay(const char *cls);
/* the camera of view 0 a scene or a talk uses (the instance has none of its own): made when missing */
void Coop_SceneCamera(void);
void Coop_SceneEnding(void);
void Coop_SceneEnd(void);
/* a fade of the scripts' screen 0 (SScorer.Fade): on every client here (a scene's callback fades in the next scene) */
void Coop_Fade(const int *from, const int *to, double time);
/* the texts and voices of the scripts (TalkScorer.c, AbstractTextScorer.c, ScriptModes.c) */
void Coop_SpeechText(const char *text, float time, int r, int g, int b);
void Coop_MsgText(const char *text, float time, const char *font);
void Coop_OpenVoice(const char *file, int channel);
/* a remote communication of the scripts (Speech.ForceRemoteComunication) begins (on 1: the mission's member current,
 * its text and voice go to that member and the members near it) and ends (0: the binding before it again; nested
 * calls count); Coop_RemoteHead: the caller's head on their monitors */
void Coop_RemoteCom(byte on);
void Coop_RemoteHead(const char *model, const char *text, int index);
void Coop_PlayVoice(int channel);
/* a door's level change to level on a coop instance: 1 when it is a chapter of the story (into or out of a level of
 * the story alone: the whole party goes, told to the members) */
byte Coop_PartyTravel(const char *level);
/* the game deletes e (cEntity_Del): on a coop instance before its first tick (the level's start, a mission's Init)
 * its name is kept for the clients (COOP_Y_GONE at their join) */
void Coop_EntityGone(struct cEntity *e);
/* player i died (Rep_Death, entity e): when every member here is dead the single player's death runs (the mission's
 * hook Chars.OnMainCharDeath, else the end of the game); Coop_PartyDeath: it runs (Chars.MainCharDeath of a member
 * does nothing otherwise) */
void Coop_Died(int i, struct cEntity *e);
byte Coop_PartyDeath(void);
/* the scripts' end of the game (MissionsFuncs.FinishGame) on a coop instance: the mission failed, the members are
 * told and the level starts again for them at the way point after CP_FAIL_SECS; 1 there */
byte Coop_MissionFailed(void);
/* where a member without an entrance starts: next to a member here, else at the single player's first spawn of
 * the level; 1 with *pos */
byte Coop_Spawn(int player, vec3_t *pos, float *yaw);
/* a scene plays here: the characters it hid are no lost players */
byte Coop_SceneHolds(void);
/* player i's character is held: a scene plays, the ship editor or the party Sputnik's screen is open on its client (or
 * Sputnik's interface hides it, include/scrap/sputniks.h), or the scripts took the
 * character's control (a ride of the metro or an elevator): it gets no new one */
byte Coop_Holds(int i);
/* the player slot a script gives character e (UsrControl; old: its slot now): on a coop instance a member's
 * character the scripts take (-1) is held for its member, and the scripts' 0..3 is that member's slot again (else
 * the current member's: the single player's player 0); with single player's flows of the players (Rep_SpFlows) the
 * scripts' 0..3 is the acting player's slot */
int Coop_UsrSlot(struct cEntity *e, int old, int slot);
/* the slot the scripts read of a character or ship (UsrControl): on a coop instance (and with Rep_SpFlows) a
 * player's slot is the single player's 0 (the scripts' "the player": a race's checkpoints, the coins), else slot */
int Coop_UsrRead(int slot);
/* the member whose character the scripts hold (Coop_UsrSlot), -1 */
int Coop_HeldPlayer(struct cEntity *e);
/* a member's called ship in the coop story: the party's hangar ship the story flies (Hangar.iHangarShip, as single
 * player's own ship Parking.CreatePlayerShip) parked at p; for its aux slot (the junkyard's lifeboat) a standard parked
 * ship, as the single player takes a parked one, else (a level without those: GDB) the aux slot's ship; NULL when the
 * hangar has none */
struct cEntity *Coop_HangarShip(const vec3_t *p, float yaw);
/* the level has no room for a called ship (indoors): coop calls none there */
byte Coop_NoShipCall(void);
/* a player's new character: it selects what it looks at, as the single player's (the conversations need it) */
void Coop_Spawned(struct cEntity *e);
/* the name of a player's character the scripts make (Chars.CreateMainChar of a possession or a conversor) on a coop
 * instance: a new one for the current member each time (the single player's Player0 and PlayerZ would be the same
 * entity for two members) and with Rep_SpFlows, NULL elsewhere */
const char *Coop_MainCharName(void);
/* player i's slot is on e, a live character or ship other than the one the instance gave it: 1 when it is the
 * member's from now on (a possession or a conversor of the scripts: the member plays as that character, its class
 * kept in its state; a ship the scripts put it in), and with Rep_SpFlows (single player's boarding and parking); 0
 * elsewhere (the instance gives the player a new character) */
byte Coop_Adopts(int i, struct cEntity *e);
/* the scripts set the class (ActCtrl) of e: a member's character keeps it as the member's class (InfiltrateBank's
 * scene makes the chief of bankers D-Tritus again: the next level's character) */
void Coop_ActCtrlSet(struct cEntity *e);
/* test: the buttons of a test press into player i's input of the tick (the line "press"), the race and the fight
 * helpers; back: how far back its client shows the other entities (s, the shots' lag compensation) */
void Coop_TestInput(int i, struct NetRepInput *in, double back);
/* the level's start of the missions (MissionsFuncs.Init: the challenge's race and the mission arrow are placed
 * around the player; the Init of the map and of the mission libraries: a mission's race) on a coop instance without
 * a member yet: a stand-in character at the level's first spawn is the scripts' player meanwhile (on 1, gone with
 * 0); Coop_StandInEntity: it, NULL */
void Coop_StandIn(byte on);
struct cEntity *Coop_StandInEntity(void);
/* e is a stand-in of a level's start (the world numbers their names: CoopStandIn0, ...; it stays in the world for the
 * names the scripts kept): no player sees it, the -mp2 replication leaves it out */
byte Coop_IsStandIn(const struct cEntity *e);
/* where the single player would start in the level on a coop instance (the way point's spawn, else the level's start),
 * its heading; 0 elsewhere */
byte Coop_StartPos(vec3_t *p, float *yaw);
/* player i came into the instance: a scene that plays is sent to its client, its character hidden until the end */
void Coop_Joined(int i);
/* the use arrow of t runs on the instance for a player's character (not only a door's) */
byte Coop_UseAny(struct cEntity *t);
/* the scripts' Menu.YesNoMenu (1: it went to the member the scripts act for, its answer runs yes or no with id 0
 * and control "" here) and ShipEdit.EnterEditShipMenu (1: the editor opens on that member's client, the hangar it
 * built comes back into the story and the ship editor's callback runs here); the menus of a game without a member
 * stay as they are */
byte Coop_MenuYesNo(const char *title, const char *yes, const char *no);
/* the scores of a super deal (RacerScorer.ShowScorer, deal 1: Yes or No, 2: Continue) on a coop instance: the question
 * goes to the member as Coop_MenuYesNo, Continue's action runs CP_DEAL_SECS later (the table is on every member's
 * HUD); 1 when taken */
byte Coop_DealScores(int deal, const char *title, const char *yes, const char *no);
byte Coop_ShipEditOpen(void);
/* the character conversor's interface (CharConversor.AddInterfaceContent at the middle of the machine's action, 1:
 * it went to the client of the member that entered the machine, Coop_ConvEnter; the machine waits until the member
 * picked a class: the conversion runs here for it) and the end of the machine's action (Coop_ConvEnd: that member
 * in its context for the end's callbacks) */
void Coop_ConvEnter(void);
byte Coop_ConvMenu(void);
void Coop_ConvEnd(void);
/* a text box of the scripts (Menu.ShowWarning: the first possession, a conversion): 1 on a coop instance, where it
 * went to the client of the member the scripts act for (the instance opens no menu nobody closes) */
byte Coop_Warning(const char *text);
/* Sputnik's hangar screen (SputnikInterface.EnterSputnikInterface): 1 when it opened on the client of the member the
 * scripts act for (its money and hangar come back into the story, then EndSputnikCallBack runs here) */
byte Coop_SputnikOpen(void);
/* the editor of the member the scripts act for is on its client (its tutorial runs there; -mp2: of the current
 * player, any mode) */
byte Coop_ShipEditAway(void);
/* the save variables of the hangar (Hangar.*) into a message and back, as the menus of the hangar carry them (also the
 * Sputniks of the sandbox and freeroam, include/scrap/sputniks.h) */
void Coop_PutHangar(struct NetRepBuf *w);
void Coop_GetHangar(struct NetRepReader *r);
/* the action set the scripts select (name): on a coop instance, where no input reads it, the one of its members'
 * play: Inactive while a scene plays, else Walk (a menu, a ride or a talk of one member is on its own client; the
 * scripts that wait for Walk, a remote communication, a walk out of a teleporter, go on) */
const char *Coop_ActionSet(const char *name);
/* the instance's time keeps running for the other members (a menu or a tutorial of the scripts stops it in the single
 * player) */
byte Coop_KeepTime(void);
/* a client's message (COOP_MSG): 1 when taken */
byte Coop_Message(struct Net2Host *host, int conn, const uint8_t *data, int size);
/* player i uses the arrow of t: the option it chose on its client is the one that runs */
void Coop_BeforeUse(int i, struct cEntity *t);
/* test lines: "story" (the count, the mission), "story get <name>", "story set <name> <value>", "story inc <name>
 * <n>", "story hold 1|0" (the changes wait, go), "leader <player>" (as the client's key), "travel <player> <level>"
 * (alone, at the level's start), "qload <map>" and "restart" (as the scripts' SaveGame.QLoad and SaveGame.Restart),
 * "theend" (as the end's credits: all variables deleted, the menu), "call <Module.Func|arg|...>" (a function of the
 * scripts, as a line of a call table), "kill <Module.var>" (the entities of that list of the scripts, or the one it
 * names, die: Life 0), "show <Module.var>" (its value), "press <player> <act|act2|act3|lock,...> [<ticks>]" (the
 * buttons of its client's input for some ticks, 3 by default; a negative count: pressed once in four ticks), "target
 * <player> [<dx> <dz>]" (onto the mission's target), "usetarget <player>" (it uses the target), "race <player>
 * [0|1]" (its ship goes along the race's checkpoints, one every 2.5 s; 0 stops), "fight <player>
 * [<Module.var>|<entity>|red|@<list>|0]" (it turns to the nearest living target of that list of the scripts, that
 * entity, the red ones of the level, the entities of that entity list of the scripts or the mission's targets and fires through its input; one further than 6000 units: put
 * 2500 from it; 0 stops), "auto <player> [0|1|2] [<list>]" (it plays the mission's targets: fights the red ones and a
 * character the mission's death hook waits for (on foot: next to it, the action key), else the red ones of the level,
 * flies a race, else goes to the target and uses it, else onto an item of the mission on the radar, else fights list
 * as "fight" does; 2 only fights), "board <player> <MODEL>"
 * (next to the nearest parked ship whose model has the text and into it), "usenear <player> <prefix>" (next to the
 * nearest entity whose name starts with it, and it uses its arrow), "possess <player> <character>" (next to it until
 * its arrow is on it, then the possession button), "aiv <entity>" (a vehicle's AI: state, sub state, place, the point it
 * goes to, life), "spare <player> <percent>" (its fights fire no more at a ship whose life is at or under that percent
 * of its most: AttackTraitorius' scare; 0 off), "walk <player> <0|1> [<character>]" (the automatic player on foot walks
 * to a target far away along the level's AI graph, its followers keep up: BeatPickus' thugs; that one alone:
 * GoWithHuman's human), "where <prefix>" (place, life and logic target of the entities of that name prefix), "scan
 * <player> <0|1>" (the mission's ships that are no foes are followed, not shot: DestroyMickus' transports until the
 * scan finds Mickus), "waypoint <player> <0|1>" (the item of the scripts' waypoint is taken before the fights:
 * BankersCTF's flags), "upto <player> <Module.var> <n>" (its fights hold fire while that variable of the scripts is n
 * or more: DistractHumphrey's count; 0 off) */
byte Coop_Command(const char *line);
/* the mission's member of a coop instance (the party's acting member: the rule PARTY of the player resolver,
 * include/scrap/players.h; single player's player: BeatPickus' check of the thugs near "the player" every second); -1
 * elsewhere or without a member */
int Coop_MissionPlayer(void);

/* player i got into ship or out of its ship (NULL): the mission's callbacks of single player's parking run for it
 * (Parking.SwitchVehicleMissionCallback, SwitchCharMissionCallback and their Aux: the final fight's boss comes) */
void Coop_Boarded(int i, struct cEntity *ship);
/* a mission's revival of the player's dead ship (Vehicles.ReSpawnShip of the final fight's and the super deals'
 * RespawnPlayer) runs: always but on a coop instance, where the dead member has a new character already (Death and a
 * failed mission) and its old ship (name) is gone or no member's */
byte Coop_Revives(const struct nativeVal_s *name);
/* the name of the key bound to action of set on device: on the coop instance (no keys) a token the member's client
 * puts its own key's name in (Coop_ClientKeys), else NULL */
const char *Coop_KeyToken(const char *set, const char *action, const char *device);
/* an item of the HUD's scorer was made (its class name): the coop instance keeps the type for the clients' copies */
void Coop_ScorerAdded(const char *name, const char *type);
/* the HUD's scorer id has its standard items, the mission's call (Menu.MissionScorerCall) adds its own next: the coop
 * instance sends the clients the items that are not the standard ones */
void Coop_ScorerBase(int id);

/* ---- client ---- */

/* the key tokens of the instance (Coop_KeyToken) in text replaced by this client's key names */
void Coop_ClientKeys(char *text, int size);
/* this game is a client in a world of the coop story */
byte Coop_Client(void);
/* the party's whole story from the coordinator at the join (CoordKv text): the client's copy of it, without the
 * mission's level and library (the client's level starts run no mission) and the auto save */
void Coop_ClientStory(const char *text, int len);
/* the entity's model has no animation for the action (a scene's event of the instance): told once a run per action */
void Coop_ClientNoAnim(const char *entity, const char *action);
/* the option of the instance's arrow list this client chose ("name:text"), NULL without one (or in a scene); *count
 * its options (the talk panel draws it as the single player's use box) */
const char *Coop_ClientOption(int *count);
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
/* the conversor's interface of the instance here: a class picked (ConvertExecute) or Escape (ExitCharConversor) goes
 * to the instance and the menu closes; 1 when it was the instance's */
byte Coop_ClientConverted(const char *cls);
/* the instance's Sputnik screen closes here (before its callback): the money and the hangar go to the instance */
void Coop_ClientSputnikDone(void);
/* Escape ends the scene here (before its EndOfScene) */
void Coop_ClientSceneAbort(void);
/* the scene ended here (EndOfScene of the client): an Escape before the instance's end skips it for this member alone
 * (its character comes back with the instance's end, so it does not walk off where the instance has none) */
void Coop_ClientSceneEnded(void);
/* each frame of a client: the options; tests: SCRAP_COOP_SKIP=<s> (Escape that many seconds into a scene),
 * SCRAP_COOP_CHOOSE=<k>[:<entity prefix>] (option k of a list), SCRAP_COOP_ANSWER=yes|no[:<s>] (a question's answer
 * after 2 s or s), SCRAP_COOP_SHIPEDIT=<s> (the ship editor opens with the lifeboat in the hangar and closes after
 * that many seconds), SCRAP_COOP_CONV=<class>[,<class>...][:<s>] (the conversor's interface picks that class after
 * 3 s or s, a list the next class at each visit and its last one after its end; "none": Escape), SCRAP_COOP_SPUTNIK=<s> (Sputnik's screen closes after s), SCRAP_COOP_WARNING=<s> (a text box
 * of the instance closes after s) */
void Coop_ClientFrame(void);

#endif
