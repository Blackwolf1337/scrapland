/*
 * The steps of a level's start (stage 1.4 of the second multiplayer design, src/game/Script/InitSteps.c). Not in the
 * original.
 *
 * The start (In_InitN and In_Game of src/game/Script/InitLevel.c, Om_InitOutMap of OutMap.c, InMap_Init of InMap.c)
 * calls its steps in single player's order; each step has a row "File:call" in the netclass table initstep
 * (tools/script/netclass/initstep.tsv) tagged WORLD (once for the world), DEF (definitions) or PLAYER_ENTER (for each
 * player that enters: the startup player, its arrival, the HUD). The start asks InitStep_Now at the call of a
 * PLAYER_ENTER step. Single player and every process that is no instance run it there. The instance keeps it in the
 * level's list and runs the list for each player that joins (InitSteps_Enter, in the player's context), in place of
 * a startup player of its own: the startup player made there is that player's character (the instance adopts it,
 * src/game/App/Replicate.c), its camera is the camera of view 0 of the player's context.
 *
 * A client of the replication v2 (-mp2, include/scrap/repent.h) on a level with a client part (Lv_Client of
 * src/game/Script/levels/Levels.h, stage 3.3) runs only the DEF steps, the GROUP steps (a group whose own steps have
 * rows); WORLD and PLAYER_ENTER steps are the server's (the traffic and the rail traffic too, stage 2.2) and come by
 * replication. In place of the level's Map.InitStatus and Map.Init (SIM) it runs the level's client part: the
 * definitions those make (the flags of Init, classes). Every
 * other process, and a -mp2 client of a level without a client part, runs every step (the stage 3.2 client then
 * deletes what it made, ReplicaEnt_Purge).
 */
#ifndef SCRAP_INITSTEPS_H
#define SCRAP_INITSTEPS_H

#include "scrap/shared.h"
#include "scrap/scriptnative.h"

/* a level's start (Init(LevelPath), path "Levels/<Level>") begins: the list of its player steps empty, a -mp2
 * client's start split or not */
void InitSteps_Begin(const char *path);
/* the step "File:Module.Func" (its row of initstep.tsv) is called: 1 when it runs now. A -mp2 client with a split
 * start runs only DEF and GROUP steps; the instance keeps a PLAYER_ENTER step in the level's list (once, 0) */
byte InitStep_Now(const char *step);
/* the start of this level runs the client's steps only (a -mp2 client of a level with a client part) */
byte InitSteps_Client(void);
/* the client part of the level in place of Map.InitStatus and of Map.Init (their definitions) */
byte InitSteps_LevelStatus(const nativeVal_t *path);
byte InitSteps_LevelInit(const nativeVal_t *path);
/* the player steps of the level for player i in its context, in the start's order: 1 when the level has any */
byte InitSteps_Enter(int i);
/* the player steps run for a joining player now */
byte InitSteps_Entering(void);
/* the startup player of the steps: the owner slot of its character (single player 0, the instance: the slot of the
 * current player), the name of its camera ("MainCam", the instance one per player) */
int InitSteps_UsrSlot(void);
const char *InitSteps_CamName(void);

#endif
