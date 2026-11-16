/*
 * The input tape (src/game/Global/InputTape.c): records the input of a single player run and plays it back, so a
 * test can run exactly the same player input again in single player or on a solo instance (stage 0.3 of the second
 * multiplayer design). Not in the original, test tools only. Meant for the deterministic test mode
 * (include/scrap/testdet.h); without it a replay parts from the recording at once.
 *
 *   ScrapC -imap:Levels/DownTown -inputtape:rec:<file>    records
 *   ScrapC -imap:Levels/DownTown -inputtape:play:<file>   plays it back in single player
 *   ScrapC -instance:DownTown -solo:<file>                 the headless instance without network, one player whose
 *                                                          controls come from the tape
 *
 * The tape is a text file with two layers:
 *   D  the device layer: the values of all input actions (every set, every local player) right after the devices
 *      wrote them (Input_UpdateDevices, also its calls from a change of the action set), the action set and
 *      IsUsingMouse. A replay writes them in place of the devices, so the rest of the frame is single player's own
 *      code: the controls of the character and the ship, the camera, the menu and mission keys, the bound script
 *      functions, the target and weapon keys.
 *   C  the control layer: per slot tick of the player's entity (1/60 s, cObjEntity_Update), the kind of the entity
 *      (character or ship, so a switch between the two is in it), its name and the fields of its controls the slot
 *      takes (cWalkCharControl, cVehicleControls). A single player replay checks them (a difference is counted and
 *      the first one printed); the solo instance writes them into its player's controls, as NetPl_Apply does with a
 *      network input but with the exact values.
 * Floats are written as the hex of their bits (exact on every system). Lines:
 *   scrap-inputtape 1
 *   rate <steps a second of the deterministic clock> seed <SCRAP_TEST_DET_SEED or 1>
 *   D <frame> <set> <IsUsingMouse> {<set>.<action>.<player>=<bits>}       the non-zero values
 *   start <level> <class> <x> <y> <z> <yaw>                                 before the first C: the player's entity
 *   C <slot> c <name> <buttons> <walk> <strafe> <jump> <hRot> <vRot>       a character (buttons: INPUTTAPE_B_)
 *   C <slot> s <name> <buttons> <acc> <str> <vStr> <hRot> <vRot> <weapon> <fireWait> <emptyTime> <attackTime>
 *     <padBounce>                                                          a ship
 *   E <frame>                                                              the end
 * A replay ends after the frame of E (or of the last D without one): the main loop stops before the next step of the
 * clock, as the recording did.
 *
 * While a tape is on, the deterministic trace (SCRAP_TEST_DET_TRACE) leaves out the window and input events and the
 * command files of the test channel (the tape holds the input) and gets a line per slot tick of the player's entity:
 *   pl <slot> <c|s> <action class> pos <x y z> ang <x y z> vel <x y z> view <x y> life <n> act <layer 0/1/2/3> money <n>
 *     alarm <value> <on>
 * so tools/test/solo.sh can compare single player with the solo instance. Without a tape nothing here changes the
 * game or the trace.
 *
 * The test channel verb "tape" drives a recording: "tape key <Set>.<Action> <value>" holds the value of an input
 * action of player 0 (applied after the devices, as a key would be, while its set is on; "tape key clear" lets all
 * go), "tape end" writes E and the game ends before the next step, "tape" alone answers the state.
 */
#ifndef SCRAP_INPUTTAPE_H
#define SCRAP_INPUTTAPE_H

#include "scrap/shared.h"

struct cEntity;
struct NetRepInput;

enum { INPUTTAPE_OFF, INPUTTAPE_REC, INPUTTAPE_PLAY, INPUTTAPE_SOLO };

/* the buttons of a C line */
#define INPUTTAPE_B_ACT 0x001		/* character: act; ship: fire */
#define INPUTTAPE_B_ACT2 0x002		/* character: act2; ship: cm */
#define INPUTTAPE_B_ACT3 0x004		/* character: act3; ship: boostKey */
#define INPUTTAPE_B_LOCK 0x008		/* character: lock; ship: fullThrust */
#define INPUTTAPE_B_LOCKSTART 0x010	/* character: lockStart; ship: viewTarget */
#define INPUTTAPE_B_DISABLED 0x020	/* character: disabled; ship: follow */
#define INPUTTAPE_B_PREV 0x040		/* character: prevAct; ship: netFire */
#define INPUTTAPE_B_PREV2 0x080		/* character: prevAct2; ship: firePrev */
#define INPUTTAPE_B_PREV3 0x100		/* character: prevAct3; ship: cmPrev */

/* the mode, INPUTTAPE_ */
extern int g_InputTape;

/* -inputtape:rec:<file>, -inputtape:play:<file>, -solo:<file>; 1 when param is one of them */
byte InputTape_ParseParam(const char *param, const char *ext);
/* Input_UpdateDevices: 1 when a replay wrote the values of the actions (the devices are not read then) */
byte InputTape_Devices(void);
/* Input_UpdateDevices after the devices: the held keys of the test channel, the D line of a recording */
void InputTape_AfterDevices(void);
/* a slot tick of e (cObjEntity_Update) before its Obj_Tick: the C line of a recording, the check of a replay; 1 when
 * e is the player's entity of a recording or a replay */
byte InputTape_Slot(struct cEntity *e);
/* after the Obj_Tick of a slot of the player's entity (single player: InputTape_Slot gave 1; the solo instance: its
 * player's step): the "pl" line of the trace */
void InputTape_SlotDone(struct cEntity *e);
/* the main loop: a replay holds the clock while the next device line is of this frame (the recording ran that frame
 * without a step of the clock: the first frame after a level load while the test channel held the clock) */
byte InputTape_Hold(void);
/* the main loop, before the step of the clock: 1 when the tape's run is over (the game ends) */
byte InputTape_Ended(void);

/* the solo instance: the player's start (the start line of the tape), 0 without one */
byte InputTape_SoloStart(float pos[3], float *yaw);
/* once per instance tick before the players: the next PLAY_STEPS C lines become the tick's */
void InputTape_SoloTick(void);
/* the input of the tick for the instance's own rules (use, switch), quantized as a client sends it, from the tick's
 * first C line; 0 when the tape is over */
byte InputTape_SoloInput(struct cEntity *e, struct NetRepInput *in);
/* slot part of the tick: the exact controls of its C line into e (when the kinds agree) */
void InputTape_SoloApply(struct cEntity *e, int part);

/* the test channel verb "tape <arg>": the answer into out */
void InputTape_Command(const char *arg, char *out, int size);

#endif
