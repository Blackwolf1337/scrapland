/*
 * The flows of the scripts a -mp2 client starts for the eye, see include/scrap/play.h (PlayFlow_). Not in the
 * original.
 *
 * Possession, the conversor, an elevator and the metro start in scripts only the server runs. The press that starts
 * one (the possession button with a victim the own character can take, the use with the arrow on a conversor, an
 * elevator's option or a metro door) shows on the client at once what single player shows in the frame after it:
 * the possession's white flash, the camera linked to the machine or the elevator, the metro's map, the HUD of a movie
 * and no controls. The server runs the flow from the input of the same tick; its records of the same things come a
 * round trip later and change nothing visible then (the possession's fade is not started twice). Its hand-over (the
 * new own character, the interface of the conversor, the camera link of the elevator, the metro's map message) ends
 * the started flow; a refusal (a busy machine) or none in PLAYFLOW_SECS gives it up: the camera, the controls and
 * the HUD on foot again.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/console.h"
#include "scrap/input.h"
#include "scrap/netaudit.h"
#include "scrap/netscene.h"
#include "scrap/play.h"
#include "scrap/scorer.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/uichan.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

#define PLAYFLOW_SECS 3.0			/* the hand-over of the server after the start, else it is given up */
#define PLAYFLOW_FADE_SECS 2.0f		/* the possession's flash (CharConversor.Possession: white to clear in 2 s) */

static const char *const pf_names[] = { "none", "possession", "conversor", "elevator", "metro" };

static struct {
	int kind;						/* PLAYFLOW_*, 0: none */
	double at;						/* local time of the start */
	char ent[64];					/* the machine, elevator or door */
	byte fadeShown;					/* the possession's fade is on the screen: the server's equal one is skipped once */
	uint32_t fadeFrom, fadeTo;
} pf;

static void PF_Controls(const char *set) {
	if (Input_SelectActionSetNamed(set)) {
		Ui_ClientControls(set);
	}
}

/* the HUD of a movie (Menu.MovieScorer as the scripts call it: the HUD cleared, the cinema bars, the talk lines; the
 * server's records of it come later and make the same) and no controls */
static void PF_Movie(void) {
	if (!Mode_Import("Menu") || !PORT_CALL(Menu, MovieScorer, NULL, "i", 0L)) {
		Native_ErrPrint();
	}
	Ui_ClientHudCleared();
	PF_Controls("Inactive");
}

/* the arrow's option of an entity the own character would use: its id before ':' into id */
static byte PF_Option(cEntity *v, char *id, int size) {
	cArrowIndicator *a = &WLife(v)->arrow;
	if (!a->canUse || !a->nOpt || a->iOpt < 0 || a->iOpt >= a->nOpt) {
		return 0;
	}
	const char *opt = a->opts[a->iOpt];
	const char *colon = opt ? strchr(opt, ':') : NULL;
	if (!colon || colon - opt >= size) {
		return 0;
	}
	memcpy(id, opt, (size_t)(colon - opt));
	id[colon - opt] = 0;
	return 1;
}

static void PF_Start(int kind, const char *ent, double now) {
	pf.kind = kind;
	pf.at = now;
	Str_Copy(pf.ent, ent ? ent : "", sizeof(pf.ent));
	NetAudit_Name(NA_PREDICT, "pred.flow:", pf_names[kind]);
	Con_Printf("play: %s started for the eye%s%s\n", pf_names[kind], ent ? " at " : "", ent ? ent : "");
}

void PlayFlow_Press(cEntity *own, uint16_t pressed, double now) {
	char id[64];
	if (pf.kind || !own || !Entity_IsType(own, TYEN_WALKCHAR)) {
		return;
	}
	if ((pressed & NETREP_IN_ACT2) && !WChar(own)->control.act && cWalkCharEntity_CanPossess(own, 0)) {
		/* CharConversor.Possession: the flash on the screen, the HUD of a movie, no controls (the slow motion and the
		 * camera's flight are the server's, they come with its records) */
		cScorer *sc = Scorer_Get(0);
		if (sc) {
			pf.fadeFrom = Scorer_PackColor(255, 255, 255, 255);
			pf.fadeTo = Scorer_PackColor(255, 255, 255, 0);
			ScorerFade_Start(&sc->fade, pf.fadeFrom, pf.fadeTo, PLAYFLOW_FADE_SECS);
			pf.fadeShown = 1;
		}
		PF_Movie();
		PF_Start(PLAYFLOW_POSS, WChar_Victim(own) ? WChar_Victim(own)->name : NULL, now);
		return;
	}
	cEntity *v = (pressed & NETREP_IN_ACT3) ? WChar_Victim(own) : NULL;
	if (!v || !Entity_IsType(v, TYEN_WITH_LIFE) || !PF_Option(v, id, sizeof(id))) {
		return;
	}
	if (!strcmp(id, "Convert")) {
		/* CharConversor.EnterCharConversor: the camera on the machine, the HUD of a movie, no controls */
		NetScene_ClientCamVar("EntityLink", v->name, 0.0f);
		PF_Movie();
		PF_Start(PLAYFLOW_CONV, v->name, now);
	} else if (!strncmp(id, "MoveUp ", 7) || !strncmp(id, "MoveDown ", 9) || !strncmp(id, "MoveTele ", 9)) {
		/* Elevator.UseElevator to another elevator: ElevatorCamera (the camera rides with it), no controls */
		NetScene_ClientCamVar("EntityLink", v->name, 0.0f);
		PF_Movie();
		PF_Start(PLAYFLOW_ELEV, v->name, now);
	} else if (!strcmp(id, "Travel")) {
		/* Metro.OnUse: the map of the stations (the instance's map message hands over) */
		if (!Mode_Import("Metro") || !PORT_CALL(Metro, OnUse, NULL, "ssss", own->name, v->name, "Travel", "")) {
			Native_ErrPrint();
			return;
		}
		PF_Start(PLAYFLOW_METRO, v->name, now);
	}
}

/* the camera, the controls and the HUD on foot again */
static void PF_Back(void) {
	if (pf.kind == PLAYFLOW_CONV || pf.kind == PLAYFLOW_ELEV) {
		NetScene_ClientCamVar("EntityLink", "", 0.0f);
	}
	if (pf.kind == PLAYFLOW_METRO) {
		if (Mode_Import("Menu") && !PORT_CALL(Menu, BackToGame, NULL, "i", 0L)) {
			Native_ErrPrint();
		}
	} else {
		PF_Controls("Walk");
	}
}

static void PF_End(const char *why) {
	if (pf.kind) {
		Con_Printf("play: the %s started for the eye %s\n", pf_names[pf.kind], why);
	}
	memset(&pf, 0, sizeof(pf));
}

void PlayFlow_Frame(double now) {
	if (pf.kind && now - pf.at > PLAYFLOW_SECS) {
		NetAudit_Name(NA_PREDICT, "pred.flow.cancel:", pf_names[pf.kind]);
		PF_Back();
		PF_End("given up (no hand-over)");
	}
}

int PlayFlow_Pending(void) {
	return pf.kind;
}

const char *PlayFlow_Ent(void) {
	return pf.ent;
}

byte PlayFlow_FadeShown(uint32_t from, uint32_t to) {
	if (pf.kind != PLAYFLOW_POSS || !pf.fadeShown || from != pf.fadeFrom || to != pf.fadeTo) {
		return 0;
	}
	/* the server's flash of the possession: the hand-over, the one on the screen goes on */
	pf.fadeShown = 0;
	PlayFlow_Took(PLAYFLOW_POSS, NULL);
	return 1;
}

void PlayFlow_Took(int kind, const char *ent) {
	if (!pf.kind || pf.kind != kind || (ent && pf.ent[0] && strcmp(ent, pf.ent))) {
		return;
	}
	NetAudit_Name(NA_PREDICT, "pred.flow.took:", pf_names[kind]);
	PF_End("handed over");
}

void PlayFlow_Refused(void) {
	if (!pf.kind) {
		return;
	}
	NetAudit_Name(NA_PREDICT, "pred.flow.cancel:", pf_names[pf.kind]);
	PF_Back();
	PF_End("refused");
}

void PlayFlow_Reset(void) {
	memset(&pf, 0, sizeof(pf));
}
