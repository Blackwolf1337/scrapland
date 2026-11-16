/*
 * The input tape, see include/scrap/inputtape.h. Not in the original, test tools only.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/actions.h"
#include "scrap/alarm.h"
#include "scrap/console.h"
#include "scrap/controls.h"
#include "scrap/crt.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/input.h"
#include "scrap/inputtape.h"
#include "scrap/netrep.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/slotcontroller.h"
#include "scrap/testdet.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/world.h"

#define TAPE_VERSION 1
#define TAPE_LINE 2048				/* a D line: 4 sets of at most 40 actions, 4 players, only the non-zero values */
#define TAPE_HOLDS 32				/* the keys "tape key" holds at once */

int g_InputTape;

typedef struct tapeHold {
	int set, action;
	float value;
} tapeHold;

static struct {
	char path[0x200];
	FILE *out;						/* a recording */
	char *text;						/* a replay or the solo instance: the whole file */
	char **d, **c;					/* its D and C lines */
	int nd, nc;
	int di, ci;						/* the next D and C line */
	char *start;					/* its start line */
	uint32_t endFrame;				/* E, else the frame of the last D */
	byte haveEnd;
	byte ended;						/* a recording: "tape end" ran */
	byte started;					/* a recording: the start line is written */
	uint32_t slots;					/* the slot ticks of the player's entity so far */
	uint32_t mismatches;			/* a replay: C lines that differ from the slot's controls */
	uint32_t kindMismatches;		/* the solo instance: C lines of the other kind than its player's entity */
	uint32_t setMismatches;			/* a replay: D lines of another action set */
	uint32_t frameMismatches;		/* a replay: D lines taken in another frame than recorded */
	int base;						/* the solo instance: the first C line of the tick */
	byte soloStarted;
	tapeHold hold[TAPE_HOLDS];
	int nhold;
	char **sl;						/* the S lines (the slot phases of single player's entities) */
	byte *slUsed;
	int ns;
	char **f;						/* the F lines (the frames of single player) */
	int *fc;						/* the first C line after each F line */
	int nf, fi;						/* the solo instance: the frames and the current one */
	int count;						/* the solo instance: the C lines of the current tick or frame */
	double soloTime;				/* the solo instance: the time of the frames so far */
} tape;

static uint32_t Tape_Bits(float f) {
	uint32_t u;
	memcpy(&u, &f, sizeof(u));
	return u;
}

static float Tape_Float(const char *s, char **end) {
	uint32_t u = (uint32_t)strtoul(s, end, 16);
	float f;
	memcpy(&f, &u, sizeof(f));
	return f;
}

/* the whole file and its lines (a replay, the solo instance) */
static byte Tape_Load(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		return 0;
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	tape.text = size > 0 ? Mem_Alloc((uint32_t)size + 1) : NULL;
	size_t n = tape.text ? fread(tape.text, 1, (size_t)size, f) : 0;
	fclose(f);
	if (!tape.text) {
		return 0;
	}
	tape.text[n] = 0;
	if (strncmp(tape.text, "scrap-inputtape ", 16) || atoi(tape.text + 16) != TAPE_VERSION) {
		return 0;
	}
	int lines = 1;
	for (char *p = tape.text; *p; p++) {
		lines += *p == '\n';
	}
	tape.d = Mem_Alloc((uint32_t)(lines * sizeof(char *)));
	tape.c = Mem_Alloc((uint32_t)(lines * sizeof(char *)));
	tape.f = Mem_Alloc((uint32_t)(lines * sizeof(char *)));
	tape.sl = Mem_Alloc((uint32_t)(lines * sizeof(char *)));
	tape.slUsed = Mem_Alloc((uint32_t)lines);
	memset(tape.slUsed, 0, (size_t)lines);
	tape.fc = Mem_Alloc((uint32_t)(lines * sizeof(int)));
	for (char *p = tape.text; p && *p;) {
		char *nl = strchr(p, '\n');
		if (nl) {
			*nl = 0;
		}
		if (p[0] == 'D' && p[1] == ' ') {
			tape.d[tape.nd++] = p;
			tape.endFrame = (uint32_t)strtoul(p + 2, NULL, 10);
		} else if (p[0] == 'C' && p[1] == ' ') {
			tape.c[tape.nc++] = p;
		} else if (p[0] == 'S' && p[1] == ' ') {
			tape.sl[tape.ns++] = p;
		} else if (p[0] == 'F' && p[1] == ' ') {
			tape.fc[tape.nf] = tape.nc;
			tape.f[tape.nf++] = p;
		} else if (p[0] == 'E' && p[1] == ' ') {
			tape.endFrame = (uint32_t)strtoul(p + 2, NULL, 10);
			tape.haveEnd = 1;
		} else if (!strncmp(p, "start ", 6) && !tape.start) {
			tape.start = p;
		} else if (!strncmp(p, "rate ", 5) && TestDet_On() && atoi(p + 5) != TestDet_Fps()) {
			Con_Printf("inputtape: recorded at %d steps a second, this run has %d\n", atoi(p + 5), TestDet_Fps());
		}
		p = nl ? nl + 1 : NULL;
	}
	return 1;
}

byte InputTape_ParseParam(const char *param, const char *ext) {
	int mode;
	const char *path;

	if (!crt_stricmp("inputtape", param)) {
		if (!strncmp(ext, "rec:", 4)) {
			mode = INPUTTAPE_REC;
		} else if (!strncmp(ext, "play:", 5)) {
			mode = INPUTTAPE_PLAY;
		} else {
			Con_Printf("inputtape: -inputtape:rec:<file> or -inputtape:play:<file>\n");
			return 1;
		}
		path = strchr(ext, ':') + 1;
	} else if (!crt_stricmp("solo", param)) {
		mode = INPUTTAPE_SOLO;
		path = ext;
	} else {
		return 0;
	}
	if (g_InputTape) {
		return 1;
	}
	Str_Copy(tape.path, path, sizeof(tape.path));
	if (mode == INPUTTAPE_REC) {
		tape.out = fopen(path, "w");
		if (!tape.out) {
			Con_Printf("inputtape: cannot write %s\n", path);
			return 1;
		}
		const char *seed = getenv("SCRAP_TEST_DET_SEED");
		fprintf(tape.out, "scrap-inputtape %d\nrate %d seed %s\n", TAPE_VERSION, TestDet_Fps(),
			seed && *seed ? seed : "1");
	} else if (!Tape_Load(path)) {
		Con_Printf("inputtape: no tape in %s\n", path);
		return 1;
	}
	if (!TestDet_On()) {
		Con_Printf("inputtape: the deterministic test mode is off (SCRAP_TEST_DETERMINISTIC=1)\n");
	}
	g_InputTape = mode;
	Con_Printf("inputtape: %s %s (%d device lines, %d control lines)\n", mode == INPUTTAPE_REC ? "recording" :
		mode == INPUTTAPE_PLAY ? "playing" : "solo instance on", path, tape.nd, tape.nc);
	return 1;
}

/* ---- the device layer ---- */

byte InputTape_Devices(void) {
	if (g_InputTape != INPUTTAPE_PLAY) {
		return 0;
	}
	if (tape.di >= tape.nd) {
		return 1;			/* past the end: no input */
	}
	char *p = tape.d[tape.di++] + 2, *end;
	uint32_t frame = (uint32_t)strtoul(p, &end, 10);
	if (frame != TestDet_Frame() && tape.frameMismatches++ == 0) {
		Con_Printf("inputtape: device line %d was recorded in frame %u, this is frame %u\n", tape.di, frame,
			TestDet_Frame());
	}
	uint32_t set = (uint32_t)strtoul(end, &end, 10);
	int mouse = (int)strtol(end, &end, 10);
	if (set != Input_GetActionSet() && tape.setMismatches++ == 0) {
		Con_Printf("inputtape: device line %d is of the action set %u, the game has %u\n", tape.di, set,
			Input_GetActionSet());
	}
	for (int s = 0; s < g_ActionSets.count; s++) {
		cActionSet *as = TABLE_AT(g_ActionSets, s);
		for (int a = 0; a < as->actions.count; a++) {
			memset(TABLE_AT(as->actions, a)->value, 0, sizeof(TABLE_AT(as->actions, a)->value));
		}
	}
	g_IsUsingMouse = mouse;
	for (p = end; *p == ' ';) {
		int s = (int)strtol(p + 1, &end, 10);
		int a = *end == '.' ? (int)strtol(end + 1, &end, 10) : -1;
		int pl = *end == '.' ? (int)strtol(end + 1, &end, 10) : -1;
		if (*end != '=' || s < 0 || s >= g_ActionSets.count || pl < 0 || pl >= MAX_LOCAL_PLAYERS) {
			break;
		}
		cActionSet *as = TABLE_AT(g_ActionSets, s);
		float v = Tape_Float(end + 1, &end);
		if (a >= 0 && a < as->actions.count) {
			TABLE_AT(as->actions, a)->value[pl] = v;
		}
		p = end;
	}
	return 1;
}

void InputTape_AfterDevices(void) {
	char line[TAPE_LINE];
	uint32_t cur = Input_GetActionSet();

	for (int i = 0; i < tape.nhold; i++) {
		tapeHold *h = &tape.hold[i];
		if ((uint32_t)h->set == cur) {
			float *v = &TABLE_AT(TABLE_AT(g_ActionSets, h->set)->actions, h->action)->value[0];
			if (h->value > *v) {
				*v = h->value;
			}
		}
	}
	if (g_InputTape != INPUTTAPE_REC || !tape.out || tape.ended) {
		return;
	}
	int n = Str_PrintfQuiet(line, sizeof(line), "D %u %u %d", TestDet_Frame(), cur, g_IsUsingMouse);
	for (int s = 0; s < g_ActionSets.count; s++) {
		cActionSet *as = TABLE_AT(g_ActionSets, s);
		for (int a = 0; a < as->actions.count; a++) {
			const float *v = TABLE_AT(as->actions, a)->value;
			for (int pl = 0; pl < MAX_LOCAL_PLAYERS; pl++) {
				if (v[pl] != 0.0f && n < (int)sizeof(line) - 40) {
					n += Str_PrintfQuiet(line + n, (int)sizeof(line) - n, " %d.%d.%d=%08x", s, a, pl,
						Tape_Bits(v[pl]));
				}
			}
		}
	}
	fprintf(tape.out, "%s\n", line);
	/* the test tools may end the game with a kill */
	fflush(tape.out);
}

void InputTape_Frame(float dt) {
	if (g_InputTape == INPUTTAPE_REC && tape.out && !tape.ended) {
		fprintf(tape.out, "F %u %08x\n", TestDet_Frame(), Tape_Bits(dt));
	}
}

void InputTape_ObjSlot(const char *name, cSlotController *slot) {
	if (!slot || !name || !*name || strchr(name, ' ')) {
		return;
	}
	if (g_InputTape == INPUTTAPE_REC && tape.out && !tape.ended) {
		fprintf(tape.out, "S %s %08x\n", name, Tape_Bits(slot->time));
		return;
	}
	if (g_InputTape != INPUTTAPE_SOLO) {
		return;
	}
	size_t n = strlen(name);
	for (int i = 0; i < tape.ns; i++) {
		const char *p = tape.sl[i] + 2;
		if (!tape.slUsed[i] && !strncmp(p, name, n) && p[n] == ' ') {
			tape.slUsed[i] = 1;
			slot->time = Tape_Float(p + n + 1, NULL);
			return;
		}
	}
}

/* ---- the control layer ---- */

/* the C line of the controls of e; 0 for an entity that is neither a character nor a ship */
static int Tape_CLine(cEntity *e, char *line, int size) {
	switch (NetPl_Kind(e)) {
	case NETREP_CHAR: {
		const cWalkCharControl *c = &WChar(e)->control;
		unsigned b = (c->act ? INPUTTAPE_B_ACT : 0) | (c->act2 ? INPUTTAPE_B_ACT2 : 0) |
			(c->act3 ? INPUTTAPE_B_ACT3 : 0) | (c->lock ? INPUTTAPE_B_LOCK : 0) |
			(c->lockStart ? INPUTTAPE_B_LOCKSTART : 0) | (c->disabled ? INPUTTAPE_B_DISABLED : 0) |
			(c->prevAct ? INPUTTAPE_B_PREV : 0) | (c->prevAct2 ? INPUTTAPE_B_PREV2 : 0) |
			(c->prevAct3 ? INPUTTAPE_B_PREV3 : 0);
		return Str_PrintfQuiet(line, size, "C %u c %s %x %08x %08x %08x %08x %08x", tape.slots, e->name, b,
			Tape_Bits(c->walk), Tape_Bits(c->strafe), Tape_Bits(c->jump), Tape_Bits(c->hRot), Tape_Bits(c->vRot));
	}
	case NETREP_SHIP: {
		const cVehicleControls *c = &Veh(e)->controls;
		unsigned b = (c->fire ? INPUTTAPE_B_ACT : 0) | (c->cm ? INPUTTAPE_B_ACT2 : 0) |
			(c->boostKey ? INPUTTAPE_B_ACT3 : 0) | (c->fullThrust ? INPUTTAPE_B_LOCK : 0) |
			(c->viewTarget ? INPUTTAPE_B_LOCKSTART : 0) | (c->follow ? INPUTTAPE_B_DISABLED : 0) |
			(c->netFire ? INPUTTAPE_B_PREV : 0) | (c->firePrev ? INPUTTAPE_B_PREV2 : 0) |
			(c->cmPrev ? INPUTTAPE_B_PREV3 : 0);
		return Str_PrintfQuiet(line, size, "C %u s %s %x %08x %08x %08x %08x %08x %d %08x %08x %08x %08x", tape.slots,
			e->name, b, Tape_Bits(c->acc), Tape_Bits(c->str), Tape_Bits(c->vStr), Tape_Bits(c->hRot),
			Tape_Bits(c->vRot), c->weapon, Tape_Bits(c->fireWait), Tape_Bits(c->emptyTime), Tape_Bits(c->attackTime),
			Tape_Bits(c->padBounce));
	}
	}
	return 0;
}

byte InputTape_Slot(cEntity *e, float iv) {
	char line[0x300], start[0x200];
	int n;

	if ((g_InputTape != INPUTTAPE_REC && g_InputTape != INPUTTAPE_PLAY) || e != Players_Local() ||
		!(n = Tape_CLine(e, line, sizeof(line)))) {
		return 0;
	}
	Str_PrintfQuiet(line + n, (int)sizeof(line) - n, " i %08x", Tape_Bits(iv));
	if (g_InputTape == INPUTTAPE_REC) {
		if (!tape.out || tape.ended) {
			return 1;
		}
		if (!tape.started) {
			/* the yaw a character's view or a ship looks to */
			float yaw = NetPl_Kind(e) == NETREP_CHAR ? WChar(e)->view.y : Obj(e)->ang.y;
			const char *lp = g_pApp->levelPath ? g_pApp->levelPath : "";
			const char *ln = strrchr(lp, '/');
			tape.started = 1;
			Str_PrintfQuiet(start, sizeof(start), "start %s %s %08x %08x %08x %08x %d", ln ? ln + 1 : lp,
				Entity_GetClassName(e), Tape_Bits(e->pos.x), Tape_Bits(e->pos.y), Tape_Bits(e->pos.z), Tape_Bits(yaw),
				Players_Money(0));
			fprintf(tape.out, "%s\n", start);
		}
		fprintf(tape.out, "%s\n", line);
		return 1;
	}
	const char *want = tape.ci < tape.nc ? tape.c[tape.ci++] : "(the end of the tape)";
	/* a tape of before the slot lengths: the controls only */
	if (!strstr(want, " i ")) {
		line[n] = 0;
	}
	if (strcmp(want, line) && tape.mismatches++ == 0) {
		Con_Printf("inputtape: the controls of slot %u differ from the tape\n  game: %s\n  tape: %s\n", tape.slots,
			line, want);
	}
	return 1;
}

/* SCRAP_TAPE_DUMP=<slot>[,<radius>]: after that slot a trace line per entity within the radius (3000) of the
 * player's entity: name, class, active, position, action class */
static void Tape_Dump(cEntity *pe) {
	const char *env = getenv("SCRAP_TAPE_DUMP");
	char *end;
	if (!env || !*env || strtoul(env, &end, 10) != tape.slots) {
		return;
	}
	float r = *end == ',' ? (float)atof(end + 1) : 3000.0f;
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		vec3_t p;
		Entity_GetPos(e, &p);
		float dx = p.x - pe->pos.x, dy = p.y - pe->pos.y, dz = p.z - pe->pos.z;
		if (!e->name || dx * dx + dy * dy + dz * dz > r * r) {
			continue;
		}
		const char *act = Entity_IsType(e, TYEN_WITH_LIFE) && WLife(e)->actCtrl && WLife(e)->actCtrl->cls ?
			WLife(e)->actCtrl->cls->name : "";
		TestDet_Trace("ent %s %s %d %.9g %.9g %.9g %s", e->name, Entity_GetClassName(e), e->active, (double)p.x,
			(double)p.y, (double)p.z, act);
	}
}

void InputTape_SlotDone(cEntity *e) {
	Tape_Dump(e);
	if (g_TestDetTrace) {
		cWithLifeEntity *w = WLife(e);
		cObjEntity *o = Obj(e);
		const char *act[ACT_LAYERS];
		int kind = NetPl_Kind(e);
		byte ch = kind == NETREP_CHAR;
		int pi = Players_Of(e);
		const cAlarm *al = Players_Alarm(pi < 0 ? 0 : pi);
		for (int i = 0; i < ACT_LAYERS; i++) {
			const cActLayer *l = w->actCtrl ? &w->actCtrl->layers[i] : NULL;
			act[i] = l && l->on && l->action && l->action->name ? l->action->name : "-";
		}
		/* the action class (the character or ship type), else the entity class */
		const char *cls = w->actCtrl && w->actCtrl->cls && w->actCtrl->cls->name ? w->actCtrl->cls->name :
			Entity_GetClassName(e);
		TestDet_Trace("pl %u %c %s pos %.9g %.9g %.9g ang %.9g %.9g %.9g vel %.9g %.9g %.9g view %.9g %.9g life %d "
			"act %s/%s/%s/%s money %d alarm %.9g %d", tape.slots, ch ? 'c' : kind == NETREP_SHIP ? 's' : '-', cls,
			(double)e->pos.x, (double)e->pos.y, (double)e->pos.z, (double)o->ang.x, (double)o->ang.y,
			(double)o->ang.z, (double)o->vel.x, (double)o->vel.y, (double)o->vel.z,
			ch ? (double)WChar(e)->view.x : 0.0, ch ? (double)WChar(e)->view.y : 0.0, w->life, act[0], act[1], act[2],
			act[3], Players_Money(pi < 0 ? 0 : pi), (double)al->value, al->on);
	}
	tape.slots++;
}

/* the frame of the next D line, 0xffffffff after the last */
static uint32_t Tape_NextFrame(void) {
	return tape.di < tape.nd ? (uint32_t)strtoul(tape.d[tape.di] + 2, NULL, 10) : 0xffffffffu;
}

byte InputTape_Hold(void) {
	return g_InputTape == INPUTTAPE_PLAY && Tape_NextFrame() <= TestDet_Frame();
}

byte InputTape_Ended(void) {
	switch (g_InputTape) {
	case INPUTTAPE_REC:
		return tape.ended;
	case INPUTTAPE_PLAY:
		return tape.di >= tape.nd && TestDet_Frame() >= tape.endFrame;
	case INPUTTAPE_SOLO:
		return tape.soloStarted && (tape.nf ? tape.fi >= tape.nf : tape.base >= tape.nc);
	}
	return 0;
}

/* ---- the solo instance ---- */

byte InputTape_SoloStart(float pos[3], float *yaw, int *money) {
	char *p = tape.start ? strchr(tape.start + 6, ' ') : NULL;
	p = p ? strchr(p + 1, ' ') : NULL;
	if (g_InputTape != INPUTTAPE_SOLO || !p) {
		return 0;
	}
	for (int i = 0; i < 3; i++) {
		pos[i] = Tape_Float(p, &p);
	}
	*yaw = Tape_Float(p, &p);
	char *end;
	long m = strtol(p, &end, 10);
	if (end > p) {
		*money = (int)m;
	}
	return 1;
}

void InputTape_SoloTick(void) {
	if (g_InputTape != INPUTTAPE_SOLO) {
		return;
	}
	if (!tape.nf) {
		/* a tape without frames: PLAY_STEPS C lines a tick */
		tape.base += tape.soloStarted ? PLAY_STEPS : 0;
		tape.count = PLAY_STEPS;
	} else {
		/* single player's frame: its time and the C lines of its slots */
		tape.fi += tape.soloStarted;
		if (tape.fi < tape.nf) {
			tape.base = tape.fc[tape.fi];
			tape.count = (tape.fi + 1 < tape.nf ? tape.fc[tape.fi + 1] : tape.nc) - tape.base;
			char *end;
			strtoul(tape.f[tape.fi] + 2, &end, 10);
			tape.soloTime += (double)Tape_Float(end, NULL);
		} else {
			tape.base = tape.nc;
			tape.count = 0;
		}
	}
	tape.soloStarted = 1;
}

byte InputTape_SoloFrames(void) {
	return g_InputTape == INPUTTAPE_SOLO && tape.nf > 0;
}

double InputTape_SoloTime(void) {
	return tape.soloTime;
}

int InputTape_SoloSlots(void) {
	return g_InputTape == INPUTTAPE_SOLO ? tape.count : 0;
}

/* the C line i: its kind (NETREP_CHAR, NETREP_SHIP, 0 past the end), buttons, five floats and for a ship the weapon;
 * the rest of the line in *rest */
static int Tape_CParse(int i, unsigned *b, float f[5], int *weapon, char **rest) {
	if (i < 0 || i >= tape.nc) {
		return 0;
	}
	char *p = tape.c[i] + 2, *end;
	strtoul(p, &end, 10);
	int kind = end[1] == 'c' ? NETREP_CHAR : end[1] == 's' ? NETREP_SHIP : 0;
	p = strchr(end + 3, ' ');
	if (!kind || !p) {
		return 0;
	}
	*b = (unsigned)strtoul(p, &p, 16);
	for (int k = 0; k < 5; k++) {
		f[k] = Tape_Float(p, &p);
	}
	*weapon = kind == NETREP_SHIP ? (int)strtol(p, &p, 10) : -1;
	*rest = p;
	return kind;
}

byte InputTape_SoloInput(cEntity *e, NetRepInput *in) {
	unsigned b;
	float f[5];
	int w;
	char *rest;

	(void)e;
	memset(in, 0, sizeof(*in));
	int kind = Tape_CParse(tape.base, &b, f, &w, &rest);
	if (!kind) {
		return 0;
	}
	for (int k = 0; k < 3; k++) {
		in->move[k] = NetRep_QMove(f[k]);
		in->move2[k] = in->move[k];
	}
	in->rot[0] = NetRep_QRot(f[3]);
	in->rot[1] = NetRep_QRot(f[4]);
	if (kind == NETREP_CHAR) {
		in->buttons = (b & INPUTTAPE_B_ACT ? NETREP_IN_ACT : 0) | (b & INPUTTAPE_B_ACT2 ? NETREP_IN_ACT2 : 0) |
			(b & INPUTTAPE_B_ACT3 ? NETREP_IN_ACT3 : 0) | (b & INPUTTAPE_B_LOCK ? NETREP_IN_LOCK : 0);
	} else {
		in->buttons = (b & INPUTTAPE_B_ACT ? NETREP_IN_ACT : 0) | (b & INPUTTAPE_B_ACT2 ? NETREP_IN_ACT2 : 0) |
			(b & INPUTTAPE_B_ACT3 ? NETREP_IN_BOOST : 0) | (b & INPUTTAPE_B_LOCK ? NETREP_IN_FULL : 0);
		in->weapon = (uint8_t)w;
	}
	return 1;
}

float InputTape_SoloInterval(int part, float iv) {
	const char *p = g_InputTape == INPUTTAPE_SOLO && part < tape.count && tape.base + part < tape.nc ?
		strstr(tape.c[tape.base + part], " i ") : NULL;
	return p ? Tape_Float(p + 3, NULL) : iv;
}

void InputTape_SoloApply(cEntity *e, int part) {
	unsigned b;
	float f[5];
	int w;
	char *rest;

	if (g_InputTape != INPUTTAPE_SOLO) {
		return;
	}
	int kind = Tape_CParse(tape.base + part, &b, f, &w, &rest);
	if (!kind) {
		return;
	}
	if (kind != NetPl_Kind(e)) {
		if (tape.kindMismatches++ == 0) {
			Con_Printf("inputtape: control line %d is of a %s, the player has a %s\n", tape.base + part,
				kind == NETREP_CHAR ? "character" : "ship", NetPl_Kind(e) == NETREP_CHAR ? "character" : "ship");
		}
		return;
	}
	if (kind == NETREP_CHAR) {
		cWalkCharControl *c = &WChar(e)->control;
		c->walk = f[0];
		c->strafe = f[1];
		c->jump = f[2];
		c->hRot = f[3];
		c->vRot = f[4];
		c->act = (b & INPUTTAPE_B_ACT) != 0;
		c->act2 = (b & INPUTTAPE_B_ACT2) != 0;
		c->act3 = (b & INPUTTAPE_B_ACT3) != 0;
		c->lock = (b & INPUTTAPE_B_LOCK) != 0;
		return;
	}
	cVehicleControls *c = &Veh(e)->controls;
	c->acc = f[0];
	c->str = f[1];
	c->vStr = f[2];
	c->hRot = f[3];
	c->vRot = f[4];
	c->fire = (b & INPUTTAPE_B_ACT) != 0;
	c->cm = (b & INPUTTAPE_B_ACT2) != 0;
	c->boostKey = (b & INPUTTAPE_B_ACT3) != 0;
	c->fullThrust = (b & INPUTTAPE_B_LOCK) != 0;
	c->viewTarget = (b & INPUTTAPE_B_LOCKSTART) != 0;
	c->follow = (b & INPUTTAPE_B_DISABLED) != 0;
	if (w >= 0 && w < g_pWorld->weapons.count) {
		c->weapon = w;
	}
}

/* ---- the test channel ---- */

/* "<Set>.<Action>": the indexes; 0 for an unknown name */
static byte Tape_Action(const char *name, int *set, int *action) {
	const char *dot = strchr(name, '.');
	if (!dot) {
		return 0;
	}
	for (int s = 0; s < g_ActionSets.count; s++) {
		cActionSet *as = TABLE_AT(g_ActionSets, s);
		if (strlen(as->name) != (size_t)(dot - name) || crt_strnicmp(as->name, name, (size_t)(dot - name))) {
			continue;
		}
		for (int a = 0; a < as->actions.count; a++) {
			if (!crt_stricmp(TABLE_AT(as->actions, a)->name, dot + 1)) {
				*set = s;
				*action = a;
				return 1;
			}
		}
	}
	return 0;
}

void InputTape_Command(const char *arg, char *out, int size) {
	char name[0x80];
	float v = 0.0f;
	int set, action;

	if (!strcmp(arg, "end")) {
		if (g_InputTape == INPUTTAPE_REC && tape.out && !tape.ended) {
			fprintf(tape.out, "E %u\n", TestDet_Frame());
			fclose(tape.out);
			tape.out = NULL;
			tape.ended = 1;
			Str_PrintfQuiet(out, size, "ok: the tape ends at frame %u\n", TestDet_Frame());
		} else {
			Str_Copy(out, "error: no recording\n", size);
		}
		return;
	}
	if (!strcmp(arg, "key clear")) {
		tape.nhold = 0;
		Str_Copy(out, "ok\n", size);
		return;
	}
	if (sscanf(arg, "key %127s %f", name, &v) == 2) {
		if (!Tape_Action(name, &set, &action)) {
			Str_PrintfQuiet(out, size, "error: no input action %s\n", name);
			return;
		}
		int i = 0;
		while (i < tape.nhold && (tape.hold[i].set != set || tape.hold[i].action != action)) {
			i++;
		}
		if (v == 0.0f) {
			if (i < tape.nhold) {
				tape.hold[i] = tape.hold[--tape.nhold];
			}
		} else if (i < TAPE_HOLDS) {
			tape.hold[i] = (tapeHold){ set, action, v };
			tape.nhold += i == tape.nhold;
		}
		Str_Copy(out, "ok\n", size);
		return;
	}
	if (arg[0]) {
		Str_Copy(out, "error: tape [key <Set>.<Action> <value> | key clear | end]\n", size);
		return;
	}
	static const char *const modes[] = { "off", "rec", "play", "solo" };
	cEntity *e = Players_Local();
	Str_PrintfQuiet(out, size, "%s frame %u set %u slots %u device %d/%d control %d/%d mismatches %u kinds %u sets %u "
		"frames %u player %s\n", modes[g_InputTape], TestDet_Frame(), Input_GetActionSet(), tape.slots, tape.di,
		tape.nd, tape.ci, tape.nc, tape.mismatches, tape.kindMismatches, tape.setMismatches, tape.frameMismatches,
		e && e->name ? e->name : "-");
}
