/*
 * The cutscenes in every mode of the second multiplayer design (stage 4.5, -mp2; src/game/App/NetScene.c). Not in
 * the original.
 *
 * The scene tables (the Scene module, tag SCENE) load on both sides; the server starts a scene. On a -mp2 instance
 * Scene.Play runs as in single player: the scene entity (DM_Scene_<name>) and its actors are entities of the server,
 * replicated as every entity (their action classes are the scene's). Then a scene record (pres.h, PRES_O_SCENE: the
 * scene's name, its entity, the server's tick, the flags below) goes to the scene's audience. The client of each
 * player in it loads the scene's classes from its own tables (include/scrap/scenelist.h) when it lacks them, so the
 * replicas get their actions, links its camera of view 0 to the replica of the scene entity and does the presentation
 * half of Scene.Play itself (the cinema HUD, no input, no doppler, no action music, the sounds stopped). The replica
 * plays the scene's action at the server's time (the class block's action layers), so camera, actors and the
 * scene's own presentation events (its 2D sounds, texts and streams: NetScene_EventFilter) play at the times of the
 * server's scene. What the server's Scene.Play, EndOfScene and those events present is not sent again (derived).
 * EndOfScene sends PRES_O_SCENEEND: the client does the presentation half of the end (its own camera, HUD and input
 * again).
 *
 * Who sees a scene: coop: every player of the instance (the party in the level); the server hides the characters as
 * single player does, every member's input stops, the scene ends for all at once. Sandbox and free roam: only the
 * player whose context started it (the world's context: the acting player); the server hides only that player's own
 * entity, its client hides the walking characters (and outdoors the ships) for itself (NETSCENE_F_LOCALHIDE), the
 * others keep playing and see the actors move. The Scene module's entries run in the scene player's context there,
 * so what a scene's line and its callback present goes to that player. One scene at a time per instance (the module's
 * state is one, as in single player).
 *
 * Skipping: the client's Escape (Scene.AbortScene on a client) asks the server (NETSCENE_MSG). Sandbox: the scene's
 * player ends it alone. Coop: the rule SceneSkip of data/coop/rules.def: all (every member in the audience asked,
 * the default), any, host (player 0 alone) or none.
 *
 * A player that joins while a scene plays: in coop it gets the scene record and joins the scene at its current tick
 * (the replica of the scene entity comes with the server's action time), its entity hidden as the scene's start hid
 * the others; in sandbox it is not in the audience and sees the actors.
 *
 * Tests: the test verb "scene" (list, play <name> [player], state, skip [player]); SCRAP_SCENE_SHOTS=<dir>:<t>,<t>,...
 * takes a screenshot <dir>/<scene>-<k>.pcx at each scene time t (s of the scene entity's action; f<x>: that part of
 * the scene's length) of every scene that plays here, in single player and on a client alike.
 *
 * Audit (NA_HUD): "scene.start:<name>", "scene.end:<name>", "scene.skip:<name>" on the instance; on the client
 * "scene.client:<name>", "scene.load:<name>", "pres.missing:scene:<name>" (no tables of the scene here).
 *
 * Camera paths of the scripts (a CamPath entity on view 0 of a player's context: the boarding and parking of a ship,
 * a race's start) are scenes of that player: the path's points go to its client (PRES_O_CAMPATH), which plays them
 * with a camera path of its own (its own camera in place of the server's camera of the player) and goes back to its
 * own camera at the path's end. The server's paths are not replicated (cameras have no slot).
 *
 * Wire: PRES_O_SCENE name, entity, tick (svarint), flags (svarint); PRES_O_SCENEEND name; PRES_O_CAMPATH name, count
 * (svarint), time (f32), per point: camera, pos, ang (3 f32 each), near, far, fov, time, quake (f32), fade in, fade
 * out (svarint). Client to instance
 * (reliable): NETSCENE_MSG, kind u8 (NETSCENE_C_SKIP), name.
 */
#ifndef SCRAP_NETSCENE_H
#define SCRAP_NETSCENE_H

#include "scrap/actions.h"
#include "scrap/mathlib.h"
#include "scrap/shared.h"

#define NETSCENE_MSG 'R'
#define NETSCENE_C_SKIP 1

#define NETSCENE_PATH_POINTS 12		/* the points of a camera path a record carries */

#define NETSCENE_F_LOCALHIDE 1		/* the client hides the walking characters itself (sandbox) */
#define NETSCENE_F_OUTDOOR 2		/* an outdoor level: the ships too */

/* a camera path of the scripts (PRES_O_CAMPATH) */
typedef struct netScenePath {
	int n;
	float time;						/* "Time" of the path when it was sent */
	struct {
		char cam[64];				/* the camera the point copies: "" none, "@own" the player's own camera */
		vec3_t pos, ang;
		float zNear, zFar, fov, time, quake;
		int fadeIn, fadeOut;
	} p[NETSCENE_PATH_POINTS];
} netScenePath_t;

struct Net2Host;
struct tcBuf;
struct cEntity;

/* ---- instance ---- */

/* a -mp2 instance: the scene's presentation is the client's (Scene.c runs it inside Pres_Derive) */
byte NetScene_Derives(void);
/* a scene plays on this instance with an audience */
byte NetScene_On(void);
/* sandbox and free roam: the server hides only the scene player's own entities */
byte NetScene_LocalHide(void);
/* the player whose context the scene's entries run in (sandbox; PLAYERS_NONE in coop or without a scene) */
int NetScene_Player(void);
/* Scene.Play of cls made the scene entity ent: the audience and the record */
void NetScene_Started(const char *cls, const char *ent);
/* EndOfScene of the scene begins (before its callback): the end record */
void NetScene_Ending(void);
/* view 0 of the current player's context shows camera cam (cWorld_SetCam): a camera path of the scripts (boarding,
 * parking, a race's start) is sent to that player with the records of the frame (NetScene_Flush) */
void NetScene_CamSet(struct cEntity *cam);
/* before the records of the frame go out (Pres_Flush) */
void NetScene_Flush(void);
/* player i joined the instance */
void NetScene_Joined(int i);
/* a client's message (NETSCENE_MSG): 1 when taken */
byte NetScene_Message(struct Net2Host *host, int conn, const uint8_t *data, int size);
/* a skip of player i (the test verb, the message) */
void NetScene_Skip(int i);

/* ---- client ---- */

/* the records */
void NetScene_ClientStart(const char *cls, const char *ent, int tick, int flags);
void NetScene_ClientEnd(const char *cls);
/* a camera path of the server for the own player: played by a camera path of the client's own, its own camera
 * after its end */
void NetScene_ClientPath(const char *name, const netScenePath_t *path);
/* the own camera stays off view 0 while a path plays (Play_ViewCamera) */
byte NetScene_ClientPathHolds(void);
/* each frame: the camera link once the replica is there, the end when the replica went without an end */
void NetScene_ClientFrame(void);
/* a scene of the server plays here */
byte NetScene_ClientHolds(void);
/* Escape in a scene of the server: the skip asked; 1 when it was the server's scene */
byte NetScene_ClientSkip(void);
/* the events replica e fires: the presentation events (Event_IsPresent); on the replica of the scene entity of the
 * scene that plays here also the scene's own texts and streams, on that of another player's scene none */
actEventFilter_t NetScene_Filter(struct cEntity *e);
/* the connection is gone */
void NetScene_ClientReset(void);

/* the presentation halves of Scene.Play and EndOfScene on a client (src/game/Script/Scene.c) */
byte Scene_ClientBegin(void);
byte Scene_ClientEnd(void);

/* ---- both ---- */

/* the start of a level (Init.Init): SCRAP_TEST_SAVEVARS=<name>=<value>[;...] sets those save variables first, for a
 * test that plays a scene whose level state depends on the story (HumanCD needs the mental controller of the press of
 * GameAct 1stMurder to 3rdMurder) on every side alike */
void NetScene_TestVars(void);
/* each frame: the screenshots of SCRAP_SCENE_SHOTS */
void NetScene_ShotFrame(void);
/* the test verb "scene" */
void NetScene_Verb(struct tcBuf *b, const char *arg);

#endif
