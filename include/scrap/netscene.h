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
 * Tests: the test verb "scene" (list, play <name> [player], call <Module.Func|arg...> [player], state, skip
 * [player]); SCRAP_SCENE_SHOTS=<dir>:<t>,<t>,... takes a screenshot <dir>/<scene>-<k>.pcx at each scene time t (s of
 * the scene entity's action; f<x>: that part of the scene's length) of every scene that plays here, in single player
 * and on a client alike, drawn at that scene time exactly (NetScene_ShotStep).
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
 * (svarint), time (f32), the server time in ms (svarint), per point: camera, pos, ang (3 f32 each), near, far, fov, time, quake (f32), fade in, fade
 * out (svarint). Client to instance
 * (reliable): NETSCENE_MSG, kind u8 (NETSCENE_C_SKIP), name.
 */
#ifndef SCRAP_NETSCENE_H
#define SCRAP_NETSCENE_H

#include "scrap/actions.h"
#include "scrap/mathlib.h"
#include "scrap/shared.h"

#define NETSCENE_MSG 'u'
#define NETSCENE_C_SKIP 1

#define NETSCENE_PATH_POINTS 12		/* the points of a camera path a record carries */

#define NETSCENE_F_LOCALHIDE 1		/* the client hides the walking characters itself (sandbox) */
#define NETSCENE_F_OUTDOOR 2		/* an outdoor level: the ships too */

/* a camera path of the scripts (PRES_O_CAMPATH) */
typedef struct netScenePath {
	int n;
	float time;						/* "Time" of the path when it was sent */
	double sent;					/* the server time it was sent at (s) */
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
/* view 0 of the current player's context (coop: the world's, every member's) shows camera cam (cWorld_SetCam): a
 * camera path of the scripts (boarding, parking, a race's start) is sent to that player with the records of the frame
 * (NetScene_Flush), a fixed camera (a super deal's scores) as a path at its place that holds; when the follow camera
 * comes back after either, the client's own camera again; 1 when cam goes to the clients so */
byte NetScene_CamSet(struct cEntity *cam);
/* the scripts set a variable of a follow camera (FollowCamEntity.c: CamDist, CamAng, CamRot, AddTurn, Target,
 * MainTarget; s the entity name of Target and MainTarget, else d the number as the scripts gave it): when it is the
 * camera of a player's view (its own camera, also while a camera path of the player shows it) the setting goes to
 * that player's client (PRES_O_CAMVAR; coop: the world's camera to every player), which gives it its own camera:
 * the scripts' shots of the outdoor mission scenes (Pt_Shot, Im_Camera), the ship's camera when a player boards */
void NetScene_CamVar(struct cEntity *cam, const char *name, double d, const char *s);
/* before the records of the frame go out (Pres_Flush) */
void NetScene_Flush(void);
/* player i joined the instance */
void NetScene_Joined(int i);
/* player i is in the audience of the scene that plays: its records carry the server time (PRES_O_TIME) */
byte NetScene_Timed(int i);
/* a fade of the scene's lines went to its audience (Pres_Fade): a player that joins later gets it as far as it is */
void NetScene_FadeSent(uint32_t from, uint32_t to, float time);
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
/* a setting of the own camera (PRES_O_CAMVAR; s: an entity name of the server, else the number f) */
void NetScene_ClientCamVar(const char *name, const char *s, float f);
/* -mp2: a camera path the client starts itself for the eye (Play.c: the boarding at the use's press): the path entity,
 * its time 0, which the caller fills and puts on view 0; the server's path of the flow, when it comes, goes on from
 * where it is (its points, the later time of the two); Done: no path of the server is waited for any more */
struct cEntity *NetScene_ClientOwnPath(void);
void NetScene_ClientOwnPathDone(void);
/* the own camera stays off view 0 while a path plays (Play_ViewCamera) */
byte NetScene_ClientPathHolds(void);
/* each frame: the camera link once the replica is there, the end when the replica went without an end */
void NetScene_ClientFrame(void);
/* the replication deletes replica e: when it is the scene entity of the scene that plays here, the scene ends here
 * (the camera must not keep the model of a deleted entity; the end record comes after the deletion while the records
 * of a scene wait for the interpolation delay) */
void NetScene_ClientGone(struct cEntity *e);
/* a scene of the server plays here */
byte NetScene_ClientHolds(void);
/* Escape in a scene of the server: the skip asked; 1 when it was the server's scene */
byte NetScene_ClientSkip(void);
/* the events replica e fires: the presentation events (Event_IsPresent); on the replica of the scene entity of the
 * scene that plays here also the scene's own texts and streams, on that of another player's scene none */
actEventFilter_t NetScene_Filter(struct cEntity *e);
/* replica e is the scene entity of the scene that plays here or an entity linked to it (an actor): its layers are
 * set to the server's times exactly (cActCtrl_NetExact) */
byte NetScene_Exact(struct cEntity *e);
/* the connection is gone */
void NetScene_ClientReset(void);

/* the presentation halves of Scene.Play and EndOfScene on a client (src/game/Script/Scene.c) */
byte Scene_ClientBegin(void);
byte Scene_ClientEnd(void);

/* ---- both ---- */

/* the start of a level (before the module Init): SCRAP_TEST_SAVEVARS=<name>=<value>[;...] sets those save variables
 * first, for a test that plays a scene whose level state depends on the story (HumanCD needs the mental controller of
 * the press of GameAct 1stMurder to 3rdMurder) on every side alike; Char also preloads that character's library, the
 * pseudo name CharLib only preloads one (Bishop for GDB's scene, the player stays its character) */
void NetScene_TestVars(void);
/* each frame: the screenshots of SCRAP_SCENE_SHOTS */
void NetScene_ShotFrame(void);
/* the screenshots on the scene's own clock: a frame of dt s that would pass the time of the next shot of a camera
 * path or of single player's scene ends at it (the game's clock takes the rest in the next frame); the client draws
 * its frame at server time rt or earlier (not before prev, the frame before) when the next shot of the server's scene
 * falls before it. So a shot is of its scene time also when the frames are long (SCRAP_TIMESCALE) */
float NetScene_ShotStep(float dt);
double NetScene_ShotServerTime(double prev, double rt);
/* the test verb "scene" */
void NetScene_Verb(struct tcBuf *b, const char *arg);

#endif
