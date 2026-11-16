/*
 * The steps of a level's start (stage 1.4 of the second multiplayer design, include/scrap/initsteps.h): the gate of
 * the PLAYER_ENTER steps by the netclass table initstep, the level's list of them on the instance and its run for
 * each player that joins. Not in the original.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/initsteps.h"
#include "scrap/instance.h"
#include "scrap/netclass.h"
#include "scrap/players.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"

#define IS_STEPS 16

static struct {
	const char *steps[IS_STEPS];	/* the PLAYER_ENTER steps the start met, in its order (static texts) */
	int n;
	byte entering;
	char camName[32];
} is;

void InitSteps_Begin(void) {
	is.n = 0;
}

byte InitStep_Now(const char *step) {
	if (!g_Instance || is.entering || NetClass_Find(NCT_INITSTEP, step) != NC_PLAYER_ENTER) {
		return 1;
	}
	for (int k = 0; k < is.n; k++) {
		if (!strcmp(is.steps[k], step)) {
			return 0;
		}
	}
	if (is.n < IS_STEPS) {
		is.steps[is.n++] = step;
	}
	return 0;
}

/* the demo's lives: none left counts as none (In_InitN) */
static byte IS_Lives(void) {
	nativeVal_t lives;
	Mode_SaveVar("Player.NumLives", &lives);
	byte ok = !Native_IsStr(&lives, "-1") || Mode_SaveS("Player.NumLives", "0");
	Native_Free(&lives);
	return ok;
}

static byte IS_EnergyBar(void) {
	return Mode_GVarSetI("EnergyBarActive", 0);
}

static byte IS_CoopClient(void) {
	Coop_ClientLevelStart();
	return 1;
}

/* the steps that are no call of a module function */
static const struct {
	const char *call;
	byte (*run)(void);
} is_calls[] = {
	{ "SaveVar.Player.NumLives", IS_Lives },
	{ "GVar.EnergyBarActive", IS_EnergyBar },
	{ "Coop_ClientLevelStart", IS_CoopClient },
};

/* one step: "File:Module.Func" calls Module.Func() */
static byte IS_Run(const char *step) {
	const char *call = strchr(step, ':');
	call = call ? call + 1 : step;
	for (size_t k = 0; k < sizeof(is_calls) / sizeof(is_calls[0]); k++) {
		if (!strcmp(is_calls[k].call, call)) {
			return is_calls[k].run();
		}
	}
	char mod[64];
	const char *dot = strchr(call, '.');
	if (!dot || dot - call >= (int)sizeof(mod)) {
		return 0;
	}
	snprintf(mod, sizeof(mod), "%.*s", (int)(dot - call), call);
	return Mode_Import(mod) && Native_CallMod(mod, dot + 1, NULL, "");
}

byte InitSteps_Enter(int i) {
	if (!g_Instance || !is.n) {
		return 0;
	}
	int was = Players_Enter(i);
	snprintf(is.camName, sizeof(is.camName), "MainCam_%d", i);
	is.entering = 1;
	for (int k = 0; k < is.n; k++) {
		if (!IS_Run(is.steps[k])) {
			Con_Printf("initsteps: player %d: the step %s failed\n", i, is.steps[k]);
			Native_ErrPrint();
		}
	}
	is.entering = 0;
	Players_Leave(was);
	return 1;
}

byte InitSteps_Entering(void) {
	return is.entering;
}

int InitSteps_UsrSlot(void) {
	return is.entering && Players_Current() != PLAYERS_NONE ? PLAYERS_NET_SLOT + Players_Current() : 0;
}

const char *InitSteps_CamName(void) {
	return is.entering ? is.camName : "MainCam";
}
