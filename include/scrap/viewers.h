/*
 * The viewers of the world (stage 2.1 of the second multiplayer design, src/game/World/Viewers.c). Not in the original.
 *
 * The simulation of the original places and keeps things around the camera of view 0 and asks whether a model was
 * drawn in the last frame: the traffic and the rail traffic around the camera, the characters' placement and pause,
 * the police and the sentinels, the slot rate of the characters. Those reads go through the viewer set:
 *
 *   Single player (and every process that is not the instance) has one viewer, the camera of view 0: its place is
 *   the world's view position (cWorld.camPos[curCam]), its direction and field of view are those of the camera
 *   entity, read when asked, so the calls compute what the original computed. "Seen" is the render flag of the model
 *   (CGameModel_IsVisible) on a process that draws; a headless process draws nothing and tests the viewer's frustum.
 *
 *   The instance (Players_Net) has one viewer per joined player, made once a frame after the owner scan: the pose of
 *   the player's camera (the camera of view 0 its context set, cPlayer.camName: the follow camera MainCam_<i> its
 *   startup made, a scene's camera), else its entity's eyes and heading with the field of view of a camera; a player
 *   without an entity has the pose of its client's camera (Viewers_SetFree), when it sent one. "Seen" is
 *   a frustum test against all viewers (the field of view widened as the AI does, the far clip of the camera, the
 *   sector of the point through the portals as cAI_IsVisible). The test command `viewers fake <n> [seed]` adds n test
 *   viewers on the nodes of the level's ground graph, spread as far apart as the graph allows, each turning slowly
 *   (the load probe of stage 2.0).
 *
 * Quotas (stage 2.2): the traffic that single player keeps around its camera (rail traffic, sky traffic) is kept around
 * every viewer. Viewers nearer than VIEWERS_SHARE to a viewer before them in the set share its quota (a leader and its
 * followers); every leader gets single player's count, up to 16 leaders (more share the counts of 16), and a ship
 * far from every viewer runs its slots slower (src/game/Logic/Traffic.c, the fallback of the load probe of stage 2.0).
 * A ship belongs to a leader (cAIVehicle.viewer) and is placed around it as single player places it around the
 * camera; the repositionings per MinTimeReposAI are single player's for each leader.
 */
#ifndef SCRAP_VIEWERS_H
#define SCRAP_VIEWERS_H

#include "scrap/mathlib.h"
#include "scrap/shared.h"

#define VIEWERS_FAKE_MAX 128
#define VIEWERS_MAX (64 + VIEWERS_FAKE_MAX)
#define VIEWERS_FAKE_ID 64			/* id of the first test viewer (the players' ids are their indices) */
#define VIEWERS_LOCAL_ID -1			/* the camera of view 0 */
#define VIEWERS_NET_FOV 0x1.921fb6p+0f	/* pi / 2, the field of view of a viewer without a camera */
#define VIEWERS_NET_FAR 400000.0f	/* the far clip of a viewer without a camera (a camera's default) */
#define VIEWERS_SHARE 75000.0f		/* a viewer this near to a leader shares its quota: inside the leader's traffic
									   (MinDistReposAI; the load probe of stage 2.0) */
#define VIEWERS_CONE 1.5f			/* the half field of view of the frustum test widened as the AI does */

struct cEntity;
struct CGameModel;

typedef struct cViewer {
	vec3_t pos;					/* the camera's place (single player: cWorld.camPos[curCam]) */
	vec3_t dir;					/* the camera's z axis, length 1 */
	float fov;					/* radians */
	float far;					/* the far clip */
	struct cEntity *cam;		/* the camera entity, NULL (single player: a script deleted it) */
	struct cEntity *ent;		/* the entity the camera follows (an FCam's target, the player's entity), NULL */
	int id;						/* the player, VIEWERS_FAKE_ID + n for a test viewer, VIEWERS_LOCAL_ID */
	int leader;					/* the index of the viewer whose quota it shares (itself for a leader) */
	byte posed;					/* the pose is a camera's: single player without the camera entity has only the
								   place (the sites that need the camera skip their work, as the original did) */
} cViewer;

/* the set is the instance's (one viewer per player) */
byte Viewers_Server(void);
/* the viewers; Viewers_At(i) is valid until the next call (single player: built from the camera when asked) */
int Viewers_Count(void);
const cViewer *Viewers_At(int i);
/* the viewer nearest to p (single player: the camera), NULL when the instance has none */
const cViewer *Viewers_Near(const vec3_t *p);
/* the index of the viewer whose id is id, -1 */
int Viewers_Find(int id);
/* the camera entity's own place (single player: CamCtl_GetCamPos of the camera of view 0, which the sector tests of
 * the original read), else the viewer's place */
void Viewers_CamPos(const cViewer *v, vec3_t *out);
/* the sector the viewer's camera is in: single player's renderer's (Video_GetStatus, the sector it drew the camera in),
 * on a headless process the sector of its place */
int Viewers_CamSector(const cViewer *v, const vec3_t *camPos);
/* the square of the distance of the nearest viewer to p: single player cWorld_GetMinCamDistSq (the four views of the
 * world), the instance FLT_MAX without a viewer */
float Viewers_MinDistSq(const vec3_t *p);
/* a viewer sees the model: the render flag where the process draws, else the frustum test */
byte Viewers_SeesModel(const struct CGameModel *m);
/* a viewer sees the sphere: the frustum test of every viewer */
byte Viewers_SeesSphere(const vec3_t *p, float radius);
/* the viewer sees the sphere (frustum, far clip, sector) */
byte Viewers_ViewerSees(const cViewer *v, const vec3_t *p, float radius);
/* the sector of the indoor maps is in view: the renderer's sectors where the process draws, else a viewer in it */
byte Viewers_SeesSector(int sector);

/* the instance: the set made again (after the owner scan of the frame) */
void Viewers_Frame(float dt);
/* the instance: the pose of player id's camera its client sent with its input (a spectator's or a dead player's free
 * camera, a player before its first entity); its viewer while it has no entity; NULL clears it */
void Viewers_SetFree(int id, const vec3_t *pos, const vec3_t *dir);
/* the leaders (quotas): their count, and whether viewer i is one */
int Viewers_Leaders(void);
/* the index of the leader that the quota owner id (cAIVehicle.viewer - 1) names, -1 when it is no leader now */
int Viewers_LeaderIndex(int id);
/* the leader with the fewest of count(leader index, ctx) under quota: its index, -1 when all have quota or more */
int Viewers_NeediestLeader(int quota, int (*count)(int leader, void *ctx), void *ctx);

/* the instance's test lines: "viewers" (the set), "viewers fake <n> [seed]", "viewers clear"; 1 if it was one */
byte Viewers_Command(const char *line);

#endif
