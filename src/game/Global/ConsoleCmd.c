/*
 * The command table of the console (the console window, the full screen console of Scorer.c and the remote console
 * of the server): engine variables by name (":name" shows, ":name value" sets, "?prefix" lists), "help", and the
 * "/command args" commands of the dev console (the ones of QuickConsole.py that are engine calls, in C). No Python:
 * a line the table does not know prints the Unknown command error (Con_Unknown).
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/game.h"
#include "scrap/gvar.h"
#include "scrap/mathlib.h"
#include "scrap/missionmaker.h"
#include "scrap/net.h"
#include "scrap/platform.h"
#include "scrap/scrap.h"
#include "scrap/timer.h"
#include "scrap/world.h"

#define CON_COLOR_OUT 0xff0000		/* the color of Python's print (Scrap.ConsoleOut) */
#define CON_MAXARGS 9
#define CON_TEXT 0x100

static const char con_help1[] = "name shows a variable, name value sets it (also :name); cvarlist filter lists them\n";
static const char con_help2[] = "command a b c runs a command (commas, name=value and /command work too); Tab completes\n";

/* ---- values ---- */

enum { CA_NONE, CA_NUM, CA_STR, CA_VEC };

typedef struct conArg {
	int type;			/* CA_ */
	double v[3];			/* CA_NUM: v[0] */
	char s[CON_TEXT];		/* CA_STR: the text; CA_NUM, CA_VEC: as typed */
} conArg;

/* a number as Python reads its literal: decimal, 0x hex, 0 octal, float with exponent; 0 if s is none */
static byte Con_Number(const char *s, double *d) {
	const char *p = s;
	char *end;

	if (*p == '+' || *p == '-') {
		p++;
	}
	if (!((*p >= '0' && *p <= '9') || (*p == '.' && p[1] >= '0' && p[1] <= '9'))) {
		return 0;
	}
	errno = 0;
	long long i = strtoll(s, &end, 0);
	if (!*end && !errno) {
		*d = (double)i;
		return 1;
	}
	*d = strtod(s, &end);
	return *end == 0;
}

/* Python 1.5.2 str() (12 digits) or repr() (17) of a float: ".0" added when it looks like an int */
static void Con_FormatFloatP(char *buf, int size, double d, int digits) {
	Str_PrintfQuiet(buf, size, "%.*g", digits, d);
	const char *p = buf[0] == '-' ? buf + 1 : buf;
	while (*p >= '0' && *p <= '9') {
		p++;
	}
	if (!*p) {
		size_t len = strlen(buf);
		Str_Copy(buf + len, ".0", size - (int)len);
	}
}

static void Con_FormatFloat(char *buf, int size, double d) {
	Con_FormatFloatP(buf, size, d, 12);
}

/* Python 1.5.2 repr() of a string: single quotes unless it holds only single ones, octal escapes */
static void Con_ReprString(char *buf, int size, const char *s) {
	char quote = strchr(s, '\'') && !strchr(s, '"') ? '"' : '\'';
	int n = 0;

	if (size < 3) {
		return;
	}
	buf[n++] = quote;
	for (; *s && n < size - 6; s++) {
		unsigned char c = (unsigned char)*s;
		if (c == (unsigned char)quote || c == '\\') {
			buf[n++] = '\\';
			buf[n++] = (char)c;
		} else if (c < ' ' || c >= 0x7f) {
			n += Str_PrintfQuiet(buf + n, size - n, "\\%03o", c);
		} else {
			buf[n++] = (char)c;
		}
	}
	buf[n++] = quote;
	buf[n] = 0;
}

/* "(x, y, z)" as Python prints a tuple of floats */
static void Con_FormatVec(char *buf, int size, double x, double y, double z) {
	char a[0x40], b[0x40], c[0x40];

	Con_FormatFloat(a, sizeof(a), x);
	Con_FormatFloat(b, sizeof(b), y);
	Con_FormatFloat(c, sizeof(c), z);
	Str_PrintfQuiet(buf, size, "(%s, %s, %s)", a, b, c);
}

/* the text inside matching quotes (backslash escapes as in Python), else the text itself */
static void Con_Unquote(char *dst, int size, const char *s) {
	size_t len = strlen(s);
	int n = 0;

	if (len < 2 || (s[0] != '"' && s[0] != '\'') || s[len - 1] != s[0]) {
		Str_Copy(dst, s, size);
		return;
	}
	for (const char *p = s + 1; p < s + len - 1 && n < size - 1; p++) {
		if (*p == '\\' && p + 1 < s + len - 1) {
			p++;
			dst[n++] = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p;
		} else {
			dst[n++] = *p;
		}
	}
	dst[n] = 0;
}

/* ---- engine variables ---- */

/* the value of an engine variable as Python's repr of Scrap.Get / Scrap.Def gives it (floats with 17 digits as repr,
 * or 12 as the console shows them) */
static void Con_ReprGVar(char *buf, int size, const cGVar *v, byte def, int digits) {
	switch (v->base.type) {
	case GVAR_STRING:
		Con_ReprString(buf, size, (def ? v->def.s : v->value.s) ? (def ? v->def.s : v->value.s) : "");
		break;
	case GVAR_INT:
		Str_PrintfQuiet(buf, size, "%d", def ? v->def.i : v->value.i);
		break;
	case GVAR_FLOAT:
		Con_FormatFloatP(buf, size, (double)(def ? v->def.f : v->value.f), digits);
		break;
	case GVAR_FUNCTION:
		Str_Copy(buf, "'<function>'", size);
		break;
	default:
		Str_Copy(buf, "'<Script function>'", size);
		break;
	}
}

/* ":name": "name = value (default = value) ### description" */
static void Con_ShowVar(const char *name) {
	char cur[0x400], def[0x400], line[0xa00];

	cGVar *v = GVar_Find(name);
	if (!v) {
		Con_Errorf("ERROR: '%s' global variable not defined\n", name);
		return;
	}
	Con_ReprGVar(cur, sizeof(cur), v, 0, 12);
	Con_ReprGVar(def, sizeof(def), v, 1, 12);
	Str_PrintfQuiet(line, sizeof(line), "%s = %s (default = %s) ### %s\n", name, cur, def,
		v->base.description ? v->base.description : "");
	Con_Print(0, line, 0);
}

/* ":name value": a number for int and float variables, the text (quotes taken off) for the others */
static void Con_SetVar(const char *name, const char *val) {
	char s[0x400];
	double d;
	int r;

	cGVar *v = GVar_Find(name);
	if (v && (v->base.type == GVAR_INT || v->base.type == GVAR_FLOAT)) {
		if (!Con_Number(val, &d)) {
			Con_Errorf("ERROR: '%s' global variable must be integer or float\n", name);
			return;
		}
		r = GVar_SetNum(name, d);
	} else {
		Con_Unquote(s, sizeof(s), val);
		r = GVar_SetStr(name, s);
	}
	if (r == 1) {
		Str_PrintfQuiet(s, sizeof(s), "%s = %s\n", name, val);
		Con_Echo(CON_COLOR_OUT, s);
	}
}

/* ---- commands ---- */

typedef struct conParam {
	const char *name;
	char type;			/* n number, s text (a number is taken as its text), v vector, a number or text */
	const char *def;		/* NULL: required; "None": may be left out */
} conParam;

typedef struct conCmd {
	const char *name;
	void (*fn)(const char *cmd, const conArg *a);
	const char *desc;
	conParam p[4];
	char complete;			/* what Tab offers for the first argument (Con_ArgMatches): 0 nothing */
} conCmd;

static const conCmd *Con_FindCmd(const char *name);
static void Con_Help(void);

/* one value at *pp up to the next top level comma: "text", 'text', (x,y,z), None, a number or bare text */
static byte Con_ParseValue(const char **pp, conArg *a) {
	const char *p = *pp;
	int n = 0;

	memset(a, 0, sizeof(*a));
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (*p == '"' || *p == '\'') {
		char q = *p++;
		for (; *p && *p != q; p++) {
			if (*p == '\\' && p[1]) {
				p++;
				if (n < CON_TEXT - 1) {
					a->s[n++] = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p;
				}
			} else if (n < CON_TEXT - 1) {
				a->s[n++] = *p;
			}
		}
		if (*p != q) {
			return 0;
		}
		p++;
		a->type = CA_STR;
	} else if (*p == '(') {
		const char *end = strchr(p, ')');
		char piece[0x40];
		int k = 0;
		if (!end) {
			return 0;
		}
		Str_Copy(a->s, p, (int)(end - p + 2) < CON_TEXT ? (int)(end - p + 2) : CON_TEXT);
		for (p++; p < end; k++) {
			const char *c = memchr(p, ',', (size_t)(end - p));
			if (!c) {
				c = end;
			}
			if (k >= 3 || c - p >= (int)sizeof(piece)) {
				return 0;
			}
			memcpy(piece, p, (size_t)(c - p));
			piece[c - p] = 0;
			Str_TrimBlanks(piece);
			if (!Con_Number(piece, &a->v[k])) {
				return 0;
			}
			p = c < end ? c + 1 : end;
		}
		if (k != 3) {
			return 0;
		}
		p = end + 1;
		a->type = CA_VEC;
	} else {
		const char *c = strchr(p, ',');
		size_t len = c ? (size_t)(c - p) : strlen(p);
		if (len >= CON_TEXT) {
			len = CON_TEXT - 1;
		}
		memcpy(a->s, p, len);
		a->s[len] = 0;
		Str_TrimBlanks(a->s);
		p += c ? (size_t)(c - p) : strlen(p);
		if (!a->s[0]) {
			return 0;
		}
		a->type = Con_Number(a->s, &a->v[0]) ? CA_NUM : strcmp(a->s, "None") ? CA_STR : CA_NONE;
	}
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (*p && *p != ',') {
		return 0;
	}
	*pp = p;
	return 1;
}

/* "name=" at *pp (not "=="): the name, *pp after the '=' */
static byte Con_ParseKeyword(const char **pp, char *name, int size) {
	const char *p = *pp;
	int n = 0;

	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_')) {
		return 0;
	}
	while (((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) {
		if (n < size - 1) {
			name[n++] = *p;
		}
		p++;
	}
	name[n] = 0;
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (*p != '=' || p[1] == '=') {
		return 0;
	}
	*pp = p + 1;
	return 1;
}

static void Con_Usage(const conCmd *c) {
	char args[0x100];
	int n = 0;

	args[0] = 0;
	for (const conParam *p = c->p; p->name; p++) {
		n += Str_PrintfQuiet(args + n, (int)sizeof(args) - n, "%s%s%s%s", p == c->p ? "" : ",", p->name,
			p->def ? "=" : "", p->def ? p->def : "");
	}
	Con_Errorf("Usage: /%s %s\n", c->name, args);
}

/* the arguments of a command line on the parameters of c (positional, then name=value; a vector parameter also
 * takes three numbers), the defaults filled in and the types checked; 0 with the message printed */
static byte Con_BindArgs(const conCmd *c, const char *line, conArg *out) {
	conArg in[CON_MAXARGS];
	char kw[CON_MAXARGS][0x20];
	byte set[4] = { 0 };
	int n = 0, np = 0;
	const char *p = line;

	while (c->p[np].name) {
		np++;
	}
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	while (*p) {
		const char *save = p;
		if (n >= CON_MAXARGS) {
			Con_Usage(c);
			return 0;
		}
		if (!Con_ParseKeyword(&p, kw[n], sizeof(kw[n]))) {
			kw[n][0] = 0;
			p = save;
		}
		if (!Con_ParseValue(&p, &in[n])) {
			Con_Errorf("/%s: cannot read the arguments (open quote?)\n", c->name);
			return 0;
		}
		n++;
		if (*p == ',') {
			p++;
		}
		while (*p == ' ' || *p == '\t') {
			p++;
		}
	}
	for (int i = 0, k = 0; i < n; i++) {
		int at = k;
		if (kw[i][0]) {
			for (at = 0; at < np && strcmp(c->p[at].name, kw[i]); at++) {
			}
			if (at == np) {
				Con_Errorf("/%s: unknown argument name %s\n", c->name, kw[i]);
				return 0;
			}
		} else {
			if (k >= np) {
				Con_Usage(c);
				return 0;
			}
			k++;
		}
		if (set[at]) {
			Con_Usage(c);
			return 0;
		}
		out[at] = in[i];
		if (c->p[at].type == 'v' && in[i].type == CA_NUM && !kw[i][0] && i + 2 < n && !kw[i + 1][0] && !kw[i + 2][0]
			&& in[i + 1].type == CA_NUM && in[i + 2].type == CA_NUM) {
			out[at].type = CA_VEC;
			out[at].v[1] = in[i + 1].v[0];
			out[at].v[2] = in[i + 2].v[0];
			i += 2;
		}
		set[at] = 1;
	}
	for (int k = 0; k < np; k++) {
		const conParam *prm = &c->p[k];
		if (!set[k]) {
			const char *d = prm->def;
			if (!d || !Con_ParseValue(&d, &out[k])) {
				Con_Usage(c);
				return 0;
			}
		}
		conArg *a = &out[k];
		if (a->type == CA_NONE && prm->def && !strcmp(prm->def, "None")) {
			continue;
		}
		if (prm->type == 's' && a->type == CA_NUM) {
			a->type = CA_STR;
		}
		if ((prm->type == 'n' && a->type != CA_NUM) || (prm->type == 's' && a->type != CA_STR)
			|| (prm->type == 'v' && a->type != CA_VEC) || (prm->type == 'a' && a->type != CA_NUM && a->type != CA_STR)) {
			Con_Errorf(prm->type == 'n' ? "/%s: argument %s is not a number\n" : prm->type == 'v'
				? "/%s: argument %s is not a vector\n" : "/%s: argument %s is not text\n", c->name, prm->name);
			return 0;
		}
	}
	return 1;
}

/* ---- entity variables as the attribute get and set of an entity handle (PyEntity.c) ---- */

static cEntity *Con_Entity(const char *name) {
	return name ? World_FindEntity(name) : NULL;
}

static cEntity *Con_Cam(void) {
	return Con_Entity(cWorld_GetCamName(g_pWorld, 0));
}

/* the entity or the message of a missing one */
static cEntity *Con_Need(const char *cmd, cEntity *e, const char *name) {
	if (!e) {
		Con_Errorf("/%s: no such entity: %s\n", cmd, name ? name : "");
	}
	return e;
}

/* a number: the int variable, else the float one */
static void Con_EntSetNum(cEntity *e, const char *var, double d) {
	int i = (int)d;

	if (!Entity_VarI(e, var, &i, 1) && !Entity_VarF(e, var, &d, 1)) {
		Con_Errorf("'%s'->%s : Member invalid.\n", e->name, var);
	}
}

static void Con_EntSetVec(cEntity *e, const char *var, const double *v) {
	double x = v[0], y = v[1], z = v[2];

	if (!Entity_VarV(e, var, &x, &y, &z, 1)) {
		Con_Errorf("'%s'->%s : Member invalid.\n", e->name, var);
	}
}

static void Con_EntSetStr(cEntity *e, const char *var, const char *s) {
	char *t = (char *)s;

	if (!Entity_VarS(e, var, &t, 1)) {
		Con_Errorf("'%s'->%s : Member invalid.\n", e->name, var);
	}
}

static byte Con_EntVec(cEntity *e, const char *var, double *v) {
	if (!Entity_VarV(e, var, &v[0], &v[1], &v[2], 0)) {
		Con_Errorf("'%s'->%s : Member invalid.\n", e->name, var);
		return 0;
	}
	return 1;
}

/* a variable as Python's str() of the attribute prints it; 0 with the message when there is none */
static byte Con_EntText(cEntity *e, const char *var, char *buf, int size) {
	int i = 0;
	double x = 0.0, y = 0.0, z = 0.0;
	char *s = NULL;

	if (Entity_VarI(e, var, &i, 0)) {
		Str_PrintfQuiet(buf, size, "%d", i);
	} else if (Entity_VarF(e, var, &x, 0)) {
		Con_FormatFloat(buf, size, x);
	} else if (Entity_VarV(e, var, &x, &y, &z, 0)) {
		Con_FormatVec(buf, size, x, y, z);
	} else if (Entity_VarS(e, var, &s, 0)) {
		Str_Copy(buf, s ? s : "None", size);
	} else {
		Con_Errorf("'%s'->%s : Member invalid.\n", e->name, var);
		return 0;
	}
	return 1;
}

/* ---- the commands (QuickConsole.py) ---- */

static double con_lastTimeSpeed = 1.0;		/* QuickConsole.LastTimeSpeed */

static void Cmd_Speed(const char *cmd, const conArg *a) {
	(void)cmd;
	Timer_SetSpeed(&g_pWorld->timer, (float)a[0].v[0]);
}

static void Cmd_Stop(const char *cmd, const conArg *a) {
	(void)cmd;
	(void)a;
	if (con_lastTimeSpeed > 0.0) {
		con_lastTimeSpeed = (double)g_pWorld->timer.speed;
		Timer_SetSpeed(&g_pWorld->timer, 0.0f);
	}
}

static void Cmd_Go(const char *cmd, const conArg *a) {
	(void)cmd;
	(void)a;
	Timer_SetSpeed(&g_pWorld->timer, (float)con_lastTimeSpeed);
}

static void Cmd_CShow(const char *cmd, const conArg *a) {
	(void)cmd;
	if (a[0].type == CA_NUM) {
		GVar_SetNum("cshow", a[0].v[0]);
	} else {
		GVar_SetStr("cshow", a[0].s);
	}
}

static void Cmd_ListModels(const char *cmd, const conArg *a) {
	(void)cmd;
	(void)a;
	Models_List();
}

static void Cmd_ModelInfo(const char *cmd, const conArg *a) {
	(void)cmd;
	Model_Info(a[0].s, (int)a[1].v[0]);
}

static void Cmd_Screenshot(const char *cmd, const conArg *a) {
	char name[CON_TEXT + 2];

	(void)cmd;
	Str_PrintfQuiet(name, sizeof(name), "%s*", a[0].s);
	Screenshot_Take(name);
}

static void Cmd_Reset3D(const char *cmd, const conArg *a) {
	(void)cmd;
	(void)a;
	Video_SetCurrentMode(Video_GetCurrentMode());
}

static void Con_ShowMap(int on) {
	GVar_SetNum("RenderSky", on);
	GVar_SetNum("RenderMap", on);
	GVar_SetNum("R_SkipUpdateVis", !on);
}

static void Cmd_HideMap(const char *cmd, const conArg *a) {
	(void)cmd;
	(void)a;
	Con_ShowMap(0);
}

static void Cmd_ShowMap(const char *cmd, const conArg *a) {
	(void)cmd;
	(void)a;
	Con_ShowMap(1);
}

static void Cmd_CamRot(const char *cmd, const conArg *a) {
	cEntity *cam = Con_Need(cmd, Con_Cam(), cWorld_GetCamName(g_pWorld, 0));
	if (cam) {
		Con_EntSetNum(cam, "AddTurn", a[0].v[0]);
	}
}

static void Cmd_MyPos(const char *cmd, const conArg *a) {
	char line[0x100], vec[0xc0];
	double v[3];

	cEntity *e = Con_Need(cmd, cWorld_GetUsrEntity(g_pWorld, 0), "Player1");
	if (!e) {
		return;
	}
	if (a[0].type == CA_NUM && a[1].type == CA_NUM && a[2].type == CA_NUM) {
		v[0] = a[0].v[0];
		v[1] = a[1].v[0];
		v[2] = a[2].v[0];
		Con_EntSetVec(e, "Pos", v);
		return;
	}
	if (!Con_EntVec(e, "Pos", v)) {
		return;
	}
	Con_FormatVec(vec, sizeof(vec), v[0], v[1], v[2]);
	Str_PrintfQuiet(line, sizeof(line), "e.Pos =  %s\n", vec);
	Con_Echo(CON_COLOR_OUT, line);
	if (!Con_EntVec(e, "Ang", v)) {
		return;
	}
	Con_FormatVec(vec, sizeof(vec), v[0], v[1], v[2]);
	Str_PrintfQuiet(line, sizeof(line), "e.Ang =  %s\n", vec);
	Con_Echo(CON_COLOR_OUT, line);
}

static void Cmd_SetCam(const char *cmd, const conArg *a) {
	cEntity *cam = Con_Need(cmd, Con_Cam(), cWorld_GetCamName(g_pWorld, 0));
	if (cam) {
		Con_EntSetVec(cam, "camPos", a[0].v);
		Con_EntSetVec(cam, "camAng", a[1].v);
		Con_EntSetNum(cam, "Fov", a[2].v[0]);
	}
}

static void Cmd_GetCam(const char *cmd, const conArg *a) {
	char pos[0xc0], ang[0xc0], fov[0x40], line[0x200];

	(void)a;
	cEntity *cam = Con_Need(cmd, Con_Cam(), cWorld_GetCamName(g_pWorld, 0));
	if (cam && Con_EntText(cam, "camPos", pos, sizeof(pos)) && Con_EntText(cam, "camAng", ang, sizeof(ang))
		&& Con_EntText(cam, "Fov", fov, sizeof(fov))) {
		Str_PrintfQuiet(line, sizeof(line), "/setcam %s , %s , %s\n", pos, ang, fov);
		Con_Echo(CON_COLOR_OUT, line);
	}
}

static void Cmd_CamEnt(const char *cmd, const conArg *a) {
	char target[CON_TEXT], name[CON_TEXT], line[0x240];

	(void)a;
	cEntity *cam = Con_Need(cmd, Con_Cam(), cWorld_GetCamName(g_pWorld, 0));
	if (cam && Con_EntText(cam, "Target", target, sizeof(target)) && Con_EntText(cam, "Name", name, sizeof(name))) {
		Str_PrintfQuiet(line, sizeof(line), "Entity '%s' at camera '%s'\n", target, name);
		Con_Echo(CON_COLOR_OUT, line);
	}
}

static void Cmd_Act(const char *cmd, const conArg *a) {
	char target[CON_TEXT];
	const char *name = a[0].s;

	if (a[0].type == CA_NONE || !a[0].s[0]) {
		cEntity *cam = Con_Need(cmd, Con_Cam(), cWorld_GetCamName(g_pWorld, 0));
		if (!cam || !Con_EntText(cam, "Target", target, sizeof(target))) {
			return;
		}
		name = target;
	}
	cEntity *e = Con_Need(cmd, Con_Entity(name), name);
	if (e) {
		Con_EntSetStr(e, "ForceAction", a[1].s);
	}
}

typedef struct conDummy {
	char name[0x40];
	double pos[3];
	double ang[2];
	int flag;
} conDummy;

/* the order of Python's sort of the tuples (name, pos, ang, flag) */
static int Con_DummyCmp(const void *pa, const void *pb) {
	const conDummy *a = pa, *b = pb;
	int r = strcmp(a->name, b->name);

	for (int k = 0; !r && k < 3; k++) {
		r = (a->pos[k] > b->pos[k]) - (a->pos[k] < b->pos[k]);
	}
	for (int k = 0; !r && k < 2; k++) {
		r = (a->ang[k] > b->ang[k]) - (a->ang[k] < b->ang[k]);
	}
	return r ? r : (a->flag > b->flag) - (a->flag < b->flag);
}

static void Cmd_Dms(const char *cmd, const conArg *a) {
	char line[0x200], pattern[CON_TEXT + 2], repr[0x100], x[0x40], y[0x40], z[0x40], p[0x40], q[0x40];
	conDummy *list = NULL;
	int n = 0, cap = 0;
	vec3_t pos;
	float ang[2];
	byte flag;

	(void)cmd;
	Str_PrintfQuiet(line, sizeof(line), "Searching '%s*'...\n", a[0].s);
	Con_Echo(CON_COLOR_OUT, line);
	Str_PrintfQuiet(pattern, sizeof(pattern), "%s*", a[0].s);
	DummySearch_Start(pattern, 1);
	for (;;) {
		conDummy d;
		if (!DummySearch_Next(d.name, sizeof(d.name), &pos, ang, &flag)) {
			break;
		}
		d.pos[0] = (double)pos.x;
		d.pos[1] = (double)pos.y;
		d.pos[2] = (double)pos.z;
		d.ang[0] = (double)ang[0] * DEG_PER_PI / M_PI_D;
		d.ang[1] = (double)ang[1] * DEG_PER_PI / M_PI_D;
		d.flag = flag != 0;
		if (n == cap) {
			cap = cap ? cap * 2 : 64;
			list = Mem_Realloc(list, (uint32_t)cap * sizeof(*list));
		}
		list[n++] = d;
	}
	if (n) {
		qsort(list, (size_t)n, sizeof(*list), Con_DummyCmp);
	}
	for (int i = 0; i < n; i++) {
		Con_ReprString(repr, sizeof(repr), list[i].name);
		Con_FormatFloat(x, sizeof(x), list[i].pos[0]);
		Con_FormatFloat(y, sizeof(y), list[i].pos[1]);
		Con_FormatFloat(z, sizeof(z), list[i].pos[2]);
		Con_FormatFloat(p, sizeof(p), list[i].ang[0]);
		Con_FormatFloat(q, sizeof(q), list[i].ang[1]);
		Str_PrintfQuiet(line, sizeof(line), "(%s, (%s, %s, %s), (%s, %s), %d)\n", repr, x, y, z, p, q, list[i].flag);
		Con_Echo(CON_COLOR_OUT, line);
	}
	if (list) {
		Mem_Free(list);
	}
}

static void Cmd_Dmm(const char *cmd, const conArg *a) {
	char line[0x200];
	double pos[3], rot[3];

	cEntity *e = cWorld_GetUsrEntity(g_pWorld, 0);
	Str_PrintfQuiet(line, sizeof(line), "\n\n[%s]\n", a[0].s);
	Con_Print(CON_COLOR_ERROR, line, 0);
	if (e) {
		if (!Con_EntVec(e, "Pos", pos) || !Con_EntVec(e, "Ang", rot)) {
			return;
		}
	} else {
		e = Con_Need(cmd, Con_Cam(), cWorld_GetCamName(g_pWorld, 0));
		if (!e || !Con_EntVec(e, "camPos", pos) || !Con_EntVec(e, "camAng", rot)) {
			return;
		}
	}
	Str_PrintfQuiet(line, sizeof(line), "Pos = \t%d\t%d\t%d\n", (int)pos[0], (int)pos[1], (int)pos[2]);
	Con_Print(CON_COLOR_ERROR, line, 0);
	Str_PrintfQuiet(line, sizeof(line), "Rot = \t%d\t%d\t%d\n\n\n", (int)-rot[0], (int)(rot[1] + 180.0), (int)rot[2]);
	Con_Print(CON_COLOR_ERROR, line, 0);
}

static void Cmd_Kick(const char *cmd, const conArg *a) {
	char line[CON_TEXT + 0x20];

	(void)cmd;
	if (g_pApp->flags & APPF_CLIENT) {
		Str_PrintfQuiet(line, sizeof(line), "/kick '%s'", a[0].s);
		Net_SendCtrlString(line);
		return;
	}
	Str_PrintfQuiet(line, sizeof(line), Net_Kick(a[0].s) ? "%s is going to be kicked...\n" : "%s cannot be kicked\n",
		a[0].s);
	Con_Print(0, line, 0);
}

static void Cmd_Join(const char *cmd, const conArg *a) {
	(void)cmd;
	Net_InitClient(a[0].s, (int)a[1].v[0]);
}

static void Cmd_Help(const char *cmd, const conArg *a) {
	(void)cmd;
	(void)a;
	Con_Help();
}

/* ---- the commands of the console view (not in the original) ---- */

#define CON_COLOR_HINT 0x909090		/* grey (0xBBGGRR) */
#define CON_COLOR_DESC 0x60c060		/* green */
#define CON_COLOR_NAME 0x80e0ff		/* yellow */

static int Con_NumCmds(void);
static const conCmd *Con_CmdAt(int i);

/* text in s, case ignored */
static byte Con_HasText(const char *s, const char *text) {
	size_t n = strlen(text);
	for (; *s; s++) {
		if (!crt_strnicmp(s, text, n)) {
			return 1;
		}
	}
	return !n;
}

/* filter: a prefix of the name, or "" for all */
static byte Con_Matches(const char *name, const char *filter) {
	return !filter || !*filter || !crt_strnicmp(name, filter, strlen(filter));
}

static const char *Con_Filter(const conArg *a) {
	return a->type == CA_STR ? a->s : "";
}

static void Cmd_CmdList(const char *cmd, const conArg *a) {
	char line[0x100];
	int n = 0;
	(void)cmd;
	for (int i = 0; i < Con_NumCmds(); i++) {
		const conCmd *c = Con_CmdAt(i);
		if (Con_Matches(c->name, Con_Filter(&a[0]))) {
			Str_PrintfQuiet(line, sizeof(line), "  %-14s %s\n", c->name, c->desc);
			Con_Echo(0, line);
			n++;
		}
	}
	Str_PrintfQuiet(line, sizeof(line), "%d commands\n", n);
	Con_Echo(CON_COLOR_HINT, line);
}

static void Cmd_CvarList(const char *cmd, const conArg *a) {
	char line[0x200], val[0x100];
	int n = 0;
	(void)cmd;
	for (cGVar *v = GVar_First(); v; v = v->base.next) {
		if (Con_Matches(v->base.name, Con_Filter(&a[0]))) {
			Con_ReprGVar(val, sizeof(val), v, 0, 6);
			Str_PrintfQuiet(line, sizeof(line), "  %-32s %s\n", v->base.name, val);
			Con_Echo(0, line);
			n++;
		}
	}
	Str_PrintfQuiet(line, sizeof(line), "%d variables\n", n);
	Con_Echo(CON_COLOR_HINT, line);
}

static void Cmd_Clear(const char *cmd, const conArg *a) {
	(void)cmd;
	(void)a;
	Con_BackClear();
}

static void Cmd_Echo(const char *cmd, const conArg *a) {
	(void)cmd;
	Con_Echo(0, a[0].type == CA_STR ? a[0].s : "");
	Con_Echo(0, "\n");
}

static void Cmd_Quit(const char *cmd, const conArg *a) {
	(void)cmd;
	(void)a;
	Plat_Quit();
}

static void Cmd_Map(const char *cmd, const conArg *a) {
	const char *level = MissionMaker_Level(a[0].s);
	if (!level) {
		Con_Errorf("/%s: no level %s (Tab lists them)\n", cmd, a[0].s);
		return;
	}
	if (!MissionMaker_Load(level, "", "", "Dtritus")) {
		Con_Errorf("/%s: %s did not load\n", cmd, level);
	}
}

static void Cmd_Mission(const char *cmd, const conArg *a) {
	const mmEntry *m = MissionMaker_Find(a[0].s);
	const char *level = a[1].type == CA_STR ? MissionMaker_Level(a[1].s) : m ? m->level : NULL;
	if (!level) {
		Con_Errorf(m || a[1].type == CA_STR ? "/%s: no level %s\n" : "/%s: no mission %s (missions lists them)\n", cmd,
			a[1].type == CA_STR ? a[1].s : a[0].s);
		return;
	}
	if (!MissionMaker_Load(level, a[0].s, m ? m->dealType : "", "Dtritus")) {
		Con_Errorf("/%s: %s did not start\n", cmd, a[0].s);
	}
}

static void Cmd_Missions(const char *cmd, const conArg *a) {
	char line[0x100];
	const char *f = Con_Filter(&a[0]);
	(void)cmd;
	for (int i = 0; i < g_MissionMakerCount; i++) {
		const mmEntry *m = &g_MissionMakerList[i];
		if (*m->mission && (!*f || Con_HasText(m->mission, f) || !crt_stricmp(m->level, f))) {
			Str_PrintfQuiet(line, sizeof(line), "  %-12s %s%s%s\n", m->level, m->mission, *m->dealType ? "  " : "", m->dealType);
			Con_Echo(0, line);
		}
	}
}

static void Cmd_Split(const char *cmd, const conArg *a) {
	const char *level = MissionMaker_Level(a[0].s);
	if (!level || !MissionMaker_Split(level)) {
		Con_Errorf("/%s: %s did not load\n", cmd, a[0].s);
	}
}

static const conCmd con_cmds[] = {
	{ "help", Cmd_Help, "show this help", { { NULL } }, 0 },
	{ "cmdlist", Cmd_CmdList, "list the commands (that start with filter)", { { "filter", 's', "None" }, { NULL } }, 0 },
	{ "cvarlist", Cmd_CvarList, "list the variables and their values", { { "filter", 's', "None" }, { NULL } }, 'v' },
	{ "clear", Cmd_Clear, "clear the console", { { NULL } }, 0 },
	{ "echo", Cmd_Echo, "print a text", { { "text", 's', "None" }, { NULL } }, 0 },
	{ "quit", Cmd_Quit, "end the game", { { NULL } }, 0 },
	{ "map", Cmd_Map, "load a level with the save variables of a new game", { { "level", 's', NULL }, { NULL } }, 'l' },
	{ "mission", Cmd_Mission, "start a mission (in its level) with the save variables of a new game",
		{ { "mission", 's', NULL }, { "level", 's', "None" }, { NULL } }, 'm' },
	{ "missions", Cmd_Missions, "list the missions (of a level, or with a text in the name)",
		{ { "filter", 's', "None" }, { NULL } }, 'l' },
	{ "split", Cmd_Split, "load a network level in split screen", { { "level", 's', NULL }, { NULL } }, 's' },
	{ "speed", Cmd_Speed, "set the game speed (1 normal, 0 paused)", { { "ts", 'n', NULL }, { NULL } }, 0 },
	{ "stop", Cmd_Stop, "pause the game and remember the speed", { { NULL } }, 0 },
	{ "go", Cmd_Go, "continue at the remembered speed", { { NULL } }, 0 },
	{ "cshow", Cmd_CShow, "set the engine variable cshow", { { "f", 'a', NULL }, { NULL } }, 0 },
	{ "listmodels", Cmd_ListModels, "list the loaded models", { { NULL } }, 0 },
	{ "modelinfo", Cmd_ModelInfo, "print information about a model",
		{ { "Ename", 's', "\"\"" }, { "Nmodel", 'n', "1" }, { NULL } }, 'e' },
	{ "screenshot", Cmd_Screenshot, "save a screenshot", { { "name", 's', "\"Shot\"" }, { NULL } }, 0 },
	{ "reset3d", Cmd_Reset3D, "restart the video mode", { { NULL } }, 0 },
	{ "hidemap", Cmd_HideMap, "hide the map and the sky", { { NULL } }, 0 },
	{ "showmap", Cmd_ShowMap, "show the map and the sky", { { NULL } }, 0 },
	{ "camrot", Cmd_CamRot, "turn the camera by an angle in degrees", { { "angulus", 'n', "180" }, { NULL } }, 0 },
	{ "mypos", Cmd_MyPos, "move the player, or print position and angles",
		{ { "x", 'n', "None" }, { "y", 'n', "None" }, { "z", 'n', "None" }, { NULL } }, 0 },
	{ "setcam", Cmd_SetCam, "place the camera (position, angles, fov)",
		{ { "pos", 'v', NULL }, { "ang", 'v', NULL }, { "fov", 'n', NULL }, { NULL } }, 0 },
	{ "getcam", Cmd_GetCam, "print the /setcam line for the camera", { { NULL } }, 0 },
	{ "CamEnt", Cmd_CamEnt, "print the entity the camera follows", { { NULL } }, 0 },
	{ "act", Cmd_Act, "force an action on an entity",
		{ { "EntityName", 's', "None" }, { "ActName", 's', "\"Relax\"" }, { NULL } }, 'e' },
	{ "dms", Cmd_Dms, "list dummies whose names start with name", { { "name", 's', NULL }, { NULL } }, 0 },
	{ "dmm", Cmd_Dmm, "print a dummy line for the player position", { { "DummyName", 's', "\"DummyName\"" }, { NULL } }, 0 },
	{ "kick", Cmd_Kick, "disconnect a player (server only)", { { "name", 's', NULL }, { NULL } }, 0 },
	{ "join", Cmd_Join, "connect to a server", { { "address", 's', "\"Localhost\"" }, { "port", 'n', "0" }, { NULL } }, 0 },
};

static int Con_NumCmds(void) {
	return (int)(sizeof(con_cmds) / sizeof(con_cmds[0]));
}

static const conCmd *Con_CmdAt(int i) {
	return &con_cmds[i];
}

/* by the exact name, as Python finds QuickConsole's functions */
static const conCmd *Con_FindCmd(const char *name) {
	for (size_t i = 0; i < sizeof(con_cmds) / sizeof(con_cmds[0]); i++) {
		if (!strcmp(con_cmds[i].name, name)) {
			return &con_cmds[i];
		}
	}
	return NULL;
}

/* the help lines, with all the commands and their arguments */
static void Con_Help(void) {
	char line[0x100], usage[0x80];

	Con_Print(0, con_help1, 0);
	Con_Print(0, con_help2, 0);
	Con_Print(0, "Commands:\n", 0);
	for (size_t i = 0; i < sizeof(con_cmds) / sizeof(con_cmds[0]); i++) {
		const conCmd *c = &con_cmds[i];
		int n = Str_PrintfQuiet(usage, sizeof(usage), "%s", c->name);
		for (const conParam *p = c->p; p->name; p++) {
			n += Str_PrintfQuiet(usage + n, (int)sizeof(usage) - n, "%s%s%s%s", p == c->p ? " " : ",", p->name,
				p->def ? "=" : "", p->def ? p->def : "");
		}
		Str_PrintfQuiet(line, sizeof(line), "  %-32s %s\n", usage, c->desc);
		Con_Print(0, line, 0);
	}
}

/* ---- the fallback and the parser ---- */

/* a line the table does not know (before the table it ran as Python: a "/command" as QuickConsole.command(args), any
 * other line as a statement) */
static void Con_Unknown(const char *line) {
	Con_Errorf("Unknown command or line: %s (type help for the list)\n", line);
}

/* "/name args": a command of the table, else the fallback */
static void Console_Command(const char *line) {
	char name[0x40];
	conArg args[4];
	const char *p = line + 1;
	int n = 0;

	while (*p == ' ') {
		p++;
	}
	while (*p && *p != ' ' && n < (int)sizeof(name) - 1) {
		name[n++] = *p++;
	}
	name[n] = 0;
	const conCmd *c = Con_FindCmd(n ? name : "help");
	if (!c) {
		Con_Unknown(line);
		return;
	}
	memset(args, 0, sizeof(args));
	if (Con_BindArgs(c, p, args)) {
		c->fn(c->name, args);
	}
}

static void Console_Plain(const char *line);

/* a line of the console: "/command args", ":name" and ":name value", "?prefix", and the plain "name args" (a command,
 * else a variable shown or set) */
void Console_Execute(const char *cmd) {
	char name[0x400];

	switch (cmd[0]) {
	case '/':
		Console_Command(cmd);
		return;
	case ':': {
		Str_Copy(name, cmd + 1, sizeof(name));
		char *val = strchr(name, ' ');
		if (val) {
			*val++ = 0;
			Str_TrimBlanks(val);
			Str_TrimBlanks(name);
			Con_SetVar(name, val);
		} else {
			Str_TrimBlanks(name);
			Con_ShowVar(name);
		}
		return;
	}
	case '?':
		GVars_Show(cmd + 1);
		return;
	default:
		Console_Plain(cmd);
		return;
	}
}

/* ---- the plain lines and Tab of the console view (not in the original) ---- */

/* a command by its name, case ignored */
static const conCmd *Con_FindCmdI(const char *name) {
	for (int i = 0; i < Con_NumCmds(); i++) {
		if (!crt_stricmp(con_cmds[i].name, name)) {
			return &con_cmds[i];
		}
	}
	return NULL;
}

/* the arguments of "name a b c" as a /command takes them: without a comma in the line the blanks between the words
 * become commas (not those in quotes or parentheses) */
static void Con_PlainArgs(const char *in, char *out, int size) {
	char quote = 0;
	int depth = 0, n = 0;
	byte comma = strchr(in, ',') != NULL, blank = 0;

	for (; *in && n < size - 1; in++) {
		char c = *in;
		if (quote) {
			quote = c == quote ? 0 : quote;
		} else if (c == '"' || c == '\'') {
			quote = c;
		} else if (c == '(') {
			depth++;
		} else if (c == ')') {
			depth--;
		} else if ((c == ' ' || c == '\t') && !comma && !depth) {
			blank = 1;
			continue;
		}
		if (blank && n) {
			out[n++] = ',';
		}
		blank = 0;
		out[n++] = c;
	}
	out[n] = 0;
}

/* "name args": a command (case ignored, blanks or commas between the arguments), else an engine variable shown or
 * set, else the unknown line */
static void Console_Plain(const char *line) {
	char word[0x40], args[0x400];
	conArg a[4];
	const char *p = line;
	int n = 0;

	while (*p == ' ' || *p == '\t') {
		p++;
	}
	while (*p && *p != ' ' && *p != '\t' && n < (int)sizeof(word) - 1) {
		word[n++] = *p++;
	}
	word[n] = 0;
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (!n) {
		return;
	}
	const conCmd *c = Con_FindCmdI(word);
	if (c) {
		Con_PlainArgs(p, args, sizeof(args));
		memset(a, 0, sizeof(a));
		if (Con_BindArgs(c, args, a)) {
			c->fn(c->name, a);
		}
		return;
	}
	if (GVar_Find(word)) {
		Str_Copy(args, p, sizeof(args));
		Str_TrimBlanks(args);
		if (*args) {
			Con_SetVar(word, args);
		} else {
			Con_ShowVar(word);
		}
		return;
	}
	Con_Unknown(line);
}

#define CON_MATCHES 4096
#define CON_LIST_MAX 400		/* more matches than this are only counted */
#define CON_DESC_MAX 40			/* the descriptions of the variables only for fewer matches */

typedef struct conMatch {
	const char *name;
	const char *desc;		/* of a command */
	const cGVar *var;
} conMatch;

static conMatch con_match[CON_MATCHES];
static int con_nmatch;

static void Con_AddMatch(const char *name, const char *prefix, const char *desc, const cGVar *var) {
	if (con_nmatch >= CON_MATCHES || crt_strnicmp(name, prefix, strlen(prefix))) {
		return;
	}
	for (int i = 0; i < con_nmatch; i++) {
		if (!crt_stricmp(con_match[i].name, name)) {
			return;
		}
	}
	con_match[con_nmatch++] = (conMatch){ name, desc, var };
}

static int Con_MatchCmp(const void *a, const void *b) {
	const conMatch *x = a, *y = b;
	if ((x->var != NULL) != (y->var != NULL)) {
		return x->var ? 1 : -1;
	}
	return crt_stricmp(x->name, y->name);
}

/* the matches of an argument of the kind of Con_ArgMatches: l levels, s network levels, m missions, v variables, e
 * entities */
static void Con_ArgMatches(char kind, const char *prefix) {
	switch (kind) {
	case 'l':
		for (int i = 0; i < g_MissionMakerCount; i++) {
			Con_AddMatch(g_MissionMakerList[i].level, prefix, NULL, NULL);
		}
		Con_AddMatch("Menu", prefix, NULL, NULL);
		/* fallthrough */
	case 's':
		for (int i = 0; i < g_MissionMakerSplitCount; i++) {
			Con_AddMatch(g_MissionMakerSplitMaps[i], prefix, NULL, NULL);
		}
		break;
	case 'm':
		for (int i = 0; i < g_MissionMakerCount; i++) {
			if (*g_MissionMakerList[i].mission) {
				Con_AddMatch(g_MissionMakerList[i].mission, prefix, g_MissionMakerList[i].level, NULL);
			}
		}
		break;
	case 'v':
		for (cGVar *v = GVar_First(); v; v = v->base.next) {
			Con_AddMatch(v->base.name, prefix, NULL, v);
		}
		break;
	case 'e':
		for (cEntity *e = g_pWorld ? cWorld_GetFirstEntity(g_pWorld) : NULL; e; e = cEntity_Next(e)) {
			Con_AddMatch(e->name, prefix, NULL, NULL);
		}
		break;
	}
}

/* the list of the matches: commands with their text, variables with their value (and description) */
static void Con_PrintMatches(void) {
	char line[0x200], val[0x100];

	for (int i = 0; i < con_nmatch && i < CON_LIST_MAX; i++) {
		const conMatch *m = &con_match[i];
		if (m->var) {
			Con_ReprGVar(val, sizeof(val), m->var, 0, 6);
			Str_PrintfQuiet(line, sizeof(line), "  %-32s = %s\n", m->name, val);
			Con_Echo(0, line);
			if (con_nmatch <= CON_DESC_MAX && m->var->base.description && *m->var->base.description) {
				Str_PrintfQuiet(line, sizeof(line), "      %s\n", m->var->base.description);
				Con_Echo(CON_COLOR_DESC, line);
			}
		} else {
			Str_PrintfQuiet(line, sizeof(line), m->desc ? "  %-32s %s\n" : "  %s\n", m->name, m->desc);
			Con_Echo(m->desc ? 0 : CON_COLOR_NAME, line);
		}
	}
	if (con_nmatch > CON_LIST_MAX) {
		Str_PrintfQuiet(line, sizeof(line), "... %d more, type more letters\n", con_nmatch - CON_LIST_MAX);
		Con_Echo(CON_COLOR_HINT, line);
	}
}

void Console_Complete(char *line, int size, int *cursor) {
	char tail[0x400], word[0x100], echo[0x420];
	int pre = line[0] == '/' || line[0] == ':' ? 1 : 0;
	int cur = *cursor;

	Str_Copy(tail, line + cur, sizeof(tail));
	line[cur] = 0;
	con_nmatch = 0;
	/* the word before the cursor: the name, or the argument after the last blank or comma */
	int start = cur;
	while (start > pre && line[start - 1] != ' ' && line[start - 1] != ',' && line[start - 1] != '\t') {
		start--;
	}
	Str_Copy(word, line + start, sizeof(word));
	byte isName = 1;
	for (int i = pre; i < start; i++) {
		if (line[i] != ' ' && line[i] != '\t') {
			isName = 0;
		}
	}
	if (isName) {
		if (!word[0]) {
			Con_Echo(CON_COLOR_HINT, "Type the start of a command or variable, then Tab (cmdlist, cvarlist list all)\n");
			Str_Copy(line + cur, tail, size - cur);
			return;
		}
		if (line[0] != ':') {
			for (int i = 0; i < Con_NumCmds(); i++) {
				Con_AddMatch(con_cmds[i].name, word, con_cmds[i].desc, NULL);
			}
		}
		if (line[0] != '/') {
			Con_ArgMatches('v', word);
		}
	} else {
		/* the name and which argument the word is */
		char name[0x40];
		int n = 0, argc = 0;
		const char *p = line + pre;
		while (*p == ' ') {
			p++;
		}
		while (*p && *p != ' ' && *p != ',' && n < (int)sizeof(name) - 1) {
			name[n++] = *p++;
		}
		name[n] = 0;
		for (; p < line + start; p++) {
			if ((*p == ' ' || *p == ',') && p[1] != ' ' && p[1] != ',' && p + 1 < line + start) {
				argc++;
			}
		}
		const conCmd *c = Con_FindCmdI(name);
		if (c) {
			char kind = argc == 0 ? c->complete : c->fn == Cmd_Mission && argc == 1 ? 'l' : 0;
			Con_ArgMatches(kind, word);
		} else if (GVar_Find(name) && argc == 0 && !word[0]) {
			Con_ShowVar(name);
		}
	}
	if (con_nmatch) {
		qsort(con_match, (size_t)con_nmatch, sizeof(con_match[0]), Con_MatchCmp);
		/* the common start of all the matches, in the spelling of the first */
		int len = (int)strlen(con_match[0].name);
		for (int i = 1; i < con_nmatch; i++) {
			int k = 0;
			while (k < len && con_match[i].name[k] && crt_strnicmp(con_match[i].name + k, con_match[0].name + k, 1) == 0) {
				k++;
			}
			len = k;
		}
		if (con_nmatch > 1) {
			Str_PrintfQuiet(echo, sizeof(echo), "]%s\n", line);
			Con_Echo(CON_COLOR_HINT, echo);
			Con_PrintMatches();
		}
		if (len >= (int)strlen(word)) {
			int n = Str_PrintfQuiet(line + start, size - start, "%.*s%s", len, con_match[0].name,
				con_nmatch == 1 && !tail[0] ? " " : "");
			cur = start + n;
		}
	}
	*cursor = cur;
	Str_Copy(line + cur, tail, size - cur);
}

/* ---- for the test channel (TestChannel.c) ---- */

void Console_ReprFloat(char *buf, int size, double d) {
	Con_FormatFloatP(buf, size, d, 17);
}

void Console_ReprString(char *buf, int size, const char *s) {
	Con_ReprString(buf, size, s);
}

byte Console_ReprVar(const char *name, char *buf, int size) {
	const cGVar *v = GVar_Find(name);

	if (!v) {
		return 0;
	}
	Con_ReprGVar(buf, size, v, 0, 17);
	return 1;
}
