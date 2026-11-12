/*
 * The CreditsText module in C (scripts/scorer/creditstext.py, issue #2): the pages of the credits. The menu level
 * preloads it when it starts with ShowCredits set (Menu.CreditsMenu), the end movie (level Fake) always. Map.py
 * calls Init, sets EndOfCredits (its next page) and calls ShowCredit(n) per page; a page fades in (ShowUp), stays
 * for a time that grows with its lines and goes (ShowDown), then EndOfCredits runs. Leaving the movie (another
 * action set than Inactive) empties the page. The item of the page is data/menus/credits.def.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions call the engine in C and work on C values; the PyMethodDef
 * entries are adapters. MAX_TEXT_LINES, TextInfo (the lines of the page), TextPos, TextFont, TextLines and ctr (the
 * corner of the next page) are variables of the module (creditstext.mod sets them, credits.def and the tests read
 * them): read from the registry where the script reads them, written there where it writes them. EndOfCredits (a
 * function), the scheduled steps and Menu.MovieScorerCall stay by name.
 */
#include <string.h>

#include "scrap/input.h"
#include "scrap/scrap.h"
#include "scrap/scrapfont.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"

static const char credits_def[] = {
#embed "../../../data/menus/credits.def"
	, 0 };

#define CREDITS_ITEM "Credits"

/* the variables of the module: MAX_TEXT_LINES (an int or a float, else 20), TextInfo (the lines: a list, tuple or
 * text), TextPos ((x, y)), TextFont, TextLines (the count of lines with a text, a number), ctr (the corner of the
 * next page, 0 to 3) */
static const char *const credits_names[] = { "MAX_TEXT_LINES", "TextInfo", "TextPos", "TextFont", "TextLines", "ctr" };
enum { CV_MAX, CV_INFO, CV_POS, CV_FONT, CV_LINES, CV_CTR };

/* the variable k from the module into *out (Native_Free it): 0 with the error of the script's read (an
 * AttributeError) */
static byte Credits_Read(int k, nativeVal_t *out) {
	Native_SetNone(out);
	return Native_VarTake("CreditsText", credits_names[k], 0, NV_ANY, out);
}

/* the variable k of the module set to v, as Script_SetVar of the script port (a failed set is no error) */
static void Credits_Put(int k, const nativeVal_t *v) {
	if (!Mode_SetModVar("CreditsText", credits_names[k], v)) {
		Mode_DropError();
	}
}

/* the number of a variable as Script_GetInt and Script_GetFloat read it (an int or a float, else def) */
static double Credits_Num(int k, double def) {
	nativeVal_t v;
	if (!Credits_Read(k, &v)) {
		Mode_DropError();
		return def;
	}
	double d = v.type == NV_INT ? (double)v.i : v.type == NV_FLOAT ? v.f : def;
	Native_Free(&v);
	return d;
}

/* range(MAX_TEXT_LINES) (Script_GetInt: an int, a float cut, else 20) */
static int Credits_Max(void) {
	nativeVal_t v;
	if (!Credits_Read(CV_MAX, &v)) {
		Mode_DropError();
		return 20;
	}
	int n = v.type == NV_INT ? (int)v.i : v.type == NV_FLOAT ? (int)v.f : 20;
	Native_Free(&v);
	return n;
}

/* the line i of the page: SScorer.Set(id, "Credits", "Text<i>", text) */
static byte Credits_SetLine(int id, int i, const nativeVal_t *text) {
	char var[0x20];
	Str_Printf(var, sizeof(var), "Text%d", i);
	return Native_ScorerSet(id, CREDITS_ITEM, var, text);
}

/* the lines 0 to MAX_TEXT_LINES - 1 of the page of player id empty */
static byte Credits_Empty(int id) {
	nativeVal_t empty;
	Native_SetStr(&empty, "");
	byte ok = 1;
	for (int i = 0, n = Credits_Max(); ok && i < n; i++) {
		ok = Credits_SetLine(id, i, &empty);
	}
	return ok;
}

/* CreditsScorer(id): the item of the page, its lines empty */
static byte Credits_Scorer(int id) {
	if (!Mode_Import("Scorer")) {
		return 0;
	}
	Script_RunFile(id, "menus/credits.def", "CreditsScorer", credits_def, NULL, NULL);
	return Credits_Empty(id);
}

/* Init(): the scorer of the movie gets the page item too (Menu.MovieScorerCall), and player 0 gets it now */
static byte Credits_Init(void) {
	if (!Mode_Import("Menu")) {
		return 0;
	}
	/* OBJLIB 5 (9c): the module's own function goes into a variable of Menu as the object it is (Menu calls it through
	 * its entry) */
	return Mode_ModTo("Menu", "MovieScorerCall", "CreditsText", "CreditsScorer") && Credits_Scorer(0);
}

/* PrintData(Vis): the lines of TextInfo on the page of player 0 */
static byte Credits_Print(void) {
	int n;
	nativeVal_t info;
	byte ok = Credits_Read(CV_INFO, &info) && Native_Len(&info, &n);
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t tmp;
		const nativeVal_t *line = Native_GetItem(&info, i, 0, &tmp);
		ok = line && Credits_SetLine(0, i, line);
		if (line == &tmp) {
			Native_Free(&tmp);
		}
	}
	Native_Free(&info);
	return ok;
}

/* PrintDataGuay(Vis): the lines with random characters (Vis 1 all random, 0 the text, between a mix; nothing calls
 * it). Each character draws Scrap.Rand(Vis - 1, Vis): below 0 the character, below 1 a random one, else a space;
 * spaces stay */
static byte Credits_PrintGuay(double vis) {
	static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789!$%&/=?|@#\\/*+-.";
	int n;
	nativeVal_t info;
	byte ok = Credits_Read(CV_INFO, &info) && Native_Len(&info, &n);
	for (int i = 0; ok && i < n; i++) {
		nativeVal_t tmp, txt;
		const nativeVal_t *line = Native_GetItem(&info, i, 0, &tmp);
		if (!line) {
			ok = 0;
			break;
		}
		const char *s = "";
		int len = 0;
		if (line->type == NV_STR) {
			s = Native_Text(line);
			len = line->n;
		} else {
			/* len() of the line fails first (a dict has a length too), a sequence of other things fails at
			 * txt + tinfo */
			int ln = Native_Size(line);
			ok = ln >= 0;
			if (ok && ln > 0) {
				Scrap_RandRange(vis - 1, vis);
				ok = Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
			}
		}
		if (ok) {
			Native_SetText(&txt, s, len);
			char *o = txt.text ? txt.text : txt.s;
			for (int j = 0; j < len; j++) {
				double rnd = Scrap_RandRange(vis - 1, vis);
				o[j] = s[j] == ' ' || rnd < 0 ? s[j] : rnd < 1 ? chars[(int)Scrap_RandRange(0, sizeof(chars) - 1)] : ' ';
			}
			ok = Credits_SetLine(0, i, &txt);
			Native_Free(&txt);
		}
		if (line == &tmp) {
			Native_Free(&tmp);
		}
	}
	Native_Free(&info);
	return ok;
}

/* CleanCredits(): the page goes */
static byte Credits_Clean(void) {
	Script_SetNum(0, CREDITS_ITEM, "Visible", 0);
	Script_SetNum(0, CREDITS_ITEM, "Time", 0);
	return Credits_Empty(0);
}

/* the movie still runs (SInput.GetActionSet() is "Inactive") */
static byte Credits_InMovie(void) {
	return !strcmp(TABLE_AT(g_ActionSets, Input_GetActionSet())->name, "Inactive");
}

/* cmp(v, n) of the script against an int (num 0) or a float (num 1) */
static int Credits_Cmp(const nativeVal_t *v, double n, byte num) {
	nativeVal_t x;
	if (num) {
		Native_SetFloat(&x, n);
	} else {
		Native_SetInt(&x, (long)n);
	}
	return Native_Compare(v, &x);
}

/* the next step of ShowUp or ShowDown in 0.05 s with val + d */
static byte Credits_Step(const char *func, const nativeVal_t *val, double d) {
	nativeVal_t dv, next;
	Native_SetFloat(&dv, d);
	if (!Native_Arith('+', val, &dv, &next)) {
		return 0;
	}
	/* the function by name: the scheduler takes its entry (Mode_LaterIn, Port_Scheduled) */
	byte ok = Mode_LaterIn(0.05, "CreditsText", func, &next, 1, NULL);
	Native_Free(&next);
	return ok;
}

/* ShowUp(val=0.5): the lines go on the page; at val 0 it fades in for a time by its lines, then ShowDown */
static byte Credits_ShowUp(const nativeVal_t *val) {
	if (!Credits_InMovie()) {
		return Credits_Clean();
	}
	if (!Credits_Print()) {
		return 0;
	}
	if (Credits_Cmp(val, 0, 0) > 0) {
		return Credits_Step("ShowUp", val, -0.05);
	}
	Script_SetNum(0, CREDITS_ITEM, "Visible", 1);
	Script_SetNum(0, CREDITS_ITEM, "Time", 0);
	double time = 3.8 + Credits_Num(CV_LINES, 0) * 0.6;
	if (time > 6.5) {
		time = 6.75;
	}
	Script_SetNum(0, CREDITS_ITEM, "Time", 0);
	Script_SetNum(0, CREDITS_ITEM, "Life", time - 1.2 - 0.5);
	Script_SetNum(0, CREDITS_ITEM, "FadeIn", 0.5);
	Script_SetNum(0, CREDITS_ITEM, "FadeOut", 1.2);
	return Mode_LaterIn(time, "CreditsText", "ShowDown", NULL, 0, NULL);
}

/* ShowDown(val=0.0): ten more steps of 0.05 s while the page fades out, then EndOfCredits() when it is set; out
 * what EndOfCredits() gave */
static byte Credits_ShowDown(const nativeVal_t *val, nativeVal_t *out) {
	if (!Credits_InMovie()) {
		return Credits_Clean();
	}
	if (Credits_Cmp(val, 0.5, 1) < 0) {
		return Credits_Step("ShowDown", val, 0.05);
	}
	/* EndOfCredits is a function the level puts into the module (OBJLIB 5 (9c), held by reference: OBJLIB 7), called
	 * through its entry (ShowDown gives what it gives) */
	nativeVal_t end;
	int t = Mode_ModGet("CreditsText", "EndOfCredits", &end) ? Mode_Truth(&end) : -1;
	byte ok = t == 0 || (t > 0 && Mode_CallArgs(&end, out, NULL, 0));
	Native_Free(&end);
	return ok;
}

/* 590 - x of the script (the right corner) */
static byte Credits_Right(const nativeVal_t *x, nativeVal_t *out) {
	nativeVal_t c;
	Native_SetInt(&c, 590);
	return Native_Arith('-', &c, x, out);
}

/* ShowCredit(num): the page num (rep its repr: the language texts Credits_<num>_1 to _20, "-" or missing for an
 * empty line) in the next corner of the screen (ctr goes round: top left, bottom right, top right, bottom left) */
static byte Credits_Show(const char *rep) {
	char cad[0x80];
	nativeVal_t lines, info, font, ctr, pos;
	Str_Printf(cad, sizeof(cad), "Credits_%s_", rep);
	Native_SetInt(&lines, 0);
	Credits_Put(CV_LINES, &lines);
	Native_SetEmpty(&info, NV_LIST);
	Credits_Put(CV_INFO, &info);
	int count = 0;
	byte ok = Credits_Read(CV_FONT, &font);
	for (int i = 0, n = ok ? Credits_Max() : 0; ok && i < n; i++) {
		char key[0xa0];
		Str_Printf(key, sizeof(key), "%s%d", cad, i + 1);
		/* Scrap.GetLangStr(key) */
		const char *txt = Script_Lang(key);
		if (*txt && strcmp(txt, "-")) {
			/* SScorer.GetTextArea(TextFont, txt): the script measures the line for a width it never uses */
			const char *f;
			ok = Native_ArgStr(&font, 1, &f);
			if (ok) {
				float rect[2], size[2];
				Font_TextSize(Font_Get(f), rect, size, 0.0f, 0.0f, "%s", txt);
				Native_SetText(Native_Append(&info), txt, (int)strlen(txt));
				Native_SetInt(&lines, ++count);
				Credits_Put(CV_LINES, &lines);
			}
		} else {
			Native_SetStr(Native_Append(&info), "");
		}
	}
	Native_Free(&font);
	/* the lines so far, as the script's list has them */
	Credits_Put(CV_INFO, &info);
	Native_Free(&info);
	nativeVal_t sizex = { 0 };
	if (!ok || !Native_GVarGet("R_CreditsSizeX", &sizex) || !Credits_Read(CV_CTR, &ctr)) {
		Native_Free(&sizex);
		return 0;
	}
	int corner = -1;
	for (int k = 0; k < 4 && corner < 0; k++) {
		if (Credits_Cmp(&ctr, k, 0) == 0) {
			corner = k;
		}
	}
	Native_Free(&ctr);
	nativeVal_t right = { 0 };
	ok = !(corner == 1 || corner == 2) || Credits_Right(&sizex, &right);
	Native_Free(&sizex);
	if (corner >= 0) {
		if (!ok) {
			return 0;
		}
		static const int y0[4] = { 80, 300, 80, 300 }, dy[4] = { 0, 25, 0, 23 };
		Native_SetEmpty(&pos, NV_TUPLE);
		nativeVal_t *x = Native_Append(&pos);
		if (corner == 1 || corner == 2) {
			*x = right;
		} else {
			Native_SetInt(x, 50);
		}
		Native_SetInt(Native_Append(&pos), y0[corner] - count * dy[corner]);
		Credits_Put(CV_POS, &pos);
		Native_Free(&pos);
		Native_SetInt(&ctr, (corner + 1) % 4);
		Credits_Put(CV_CTR, &ctr);
	}
	/* Scrap.Set("R_CreditsX", TextPos[0]), Scrap.Set("R_CreditsY", TextPos[1]) */
	if (!Credits_Read(CV_POS, &pos)) {
		return 0;
	}
	static const char *const gvars[2] = { "R_CreditsX", "R_CreditsY" };
	for (int i = 0; ok && i < 2; i++) {
		nativeVal_t tmp;
		const nativeVal_t *v = Native_GetItem(&pos, i, 0, &tmp);
		ok = v && Native_GVarSet(gvars[i], v);
		if (v == &tmp) {
			Native_Free(&tmp);
		}
	}
	Native_Free(&pos);
	if (!ok) {
		return 0;
	}
	nativeVal_t half;
	Native_SetFloat(&half, 0.5);
	return Credits_ShowUp(&half);
}

/* ---- the functions of the module table: MODE_FN, and entries that parse with the formats of the port (Mode_Parse) ---- */

static byte CreditsText_CreditsScorer_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	int id;
	(void)out;
	return Mode_Parse(a, n, "i", &id) && Credits_Scorer(id);
}

MODE_ADAPTER(CreditsText, CreditsScorer)
MODE_FN(CreditsText, Init, 0, Credits_Init())
MODE_FN(CreditsText, PrintData, 1, Credits_Print())

static byte CreditsText_PrintDataGuay_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	double vis;
	(void)out;
	return Mode_Parse(a, n, "d", &vis) && Credits_PrintGuay(vis);
}

MODE_ADAPTER(CreditsText, PrintDataGuay)
MODE_FN(CreditsText, CleanCredits, 0, Credits_Clean())

/* ShowUp(val=0.5), ShowDown(val=0.0) */
static byte Credits_ShowArgs(const nativeVal_t *a, int n, byte up, nativeVal_t *out) {
	nativeVal_t def;
	Native_SetFloat(&def, up ? 0.5 : 0.0);
	return up ? Credits_ShowUp(n ? &a[0] : &def) : Credits_ShowDown(n ? &a[0] : &def, out);
}

MODE_FNOPT(CreditsText, ShowUp, 0, 1, Credits_ShowArgs(a, n, 1, out))
MODE_FNOPT(CreditsText, ShowDown, 0, 1, Credits_ShowArgs(a, n, 0, out))

/* ShowCredit(num): the page of `num` */
static byte Credits_ShowNum(const nativeVal_t *num) {
	if (!Mode_Import("Scorer")) {
		return 0;
	}
	nativeBuf_t rep;
	Native_BufInit(&rep);
	Native_Repr(&rep, num, NR_REPR);
	byte ok = Credits_Show(rep.p);
	Native_BufFree(&rep);
	return ok;
}

MODE_FN(CreditsText, ShowCredit, 1, Credits_ShowNum(&a[0]))

PyMethodDef g_CreditsTextMethods[] = {
	{ "Init", CreditsText_Init, 1, NULL },
	{ "CreditsScorer", CreditsText_CreditsScorer, 1, NULL },
	{ "PrintDataGuay", CreditsText_PrintDataGuay, 1, NULL },
	{ "PrintData", CreditsText_PrintData, 1, NULL },
	{ "ShowUp", CreditsText_ShowUp, 1, NULL },
	{ "ShowDown", CreditsText_ShowDown, 1, NULL },
	{ "CleanCredits", CreditsText_CleanCredits, 1, NULL },
	{ "ShowCredit", CreditsText_ShowCredit, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(CreditsText)
