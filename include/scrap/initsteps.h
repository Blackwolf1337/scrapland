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
 */
#ifndef SCRAP_INITSTEPS_H
#define SCRAP_INITSTEPS_H

#include "scrap/shared.h"

/* a level's start begins: the list of its player steps empty */
void InitSteps_Begin(void);
/* the PLAYER_ENTER step "File:Module.Func" (its row of initstep.tsv) is called: 1 when it runs now; on the instance
 * it goes to the level's list (once) and gives 0 */
byte InitStep_Now(const char *step);
/* the player steps of the level for player i in its context, in the start's order: 1 when the level has any */
byte InitSteps_Enter(int i);
/* the player steps run for a joining player now */
byte InitSteps_Entering(void);
/* the startup player of the steps: the owner slot of its character (single player 0, the instance: the slot of the
 * current player), the name of its camera ("MainCam", the instance one per player) */
int InitSteps_UsrSlot(void);
const char *InitSteps_CamName(void);

#endif
