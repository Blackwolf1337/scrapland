/*
 * The Bureaucracy module in C (scripts/chars/bureaucracy.py, issue #2): the desks of the functionaries in the
 * interiors. Init puts an entity on every dummy DM_DesktopFunctionary_*, CreateBureaucracyClass makes the action
 * class of a kind of desk once; the class itself is data/chars/bureaucracy.def. BureaucracyTableFile (the model
 * folder, the Press map changes it) stays a variable of the module.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions call the engine in C and work on C values; the PyMethodDef
 * entries are adapters. BureaucracyTableFile is mirrored from the module dict where Init reads it (OBJLIB 5).
 * CreateBureaucracyClass is called through its ref for each desk (PORT_CALL: directly while the attribute holds the
 * port's function, else by name, as a mission may replace it).
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"

static const char bureaucracy_def[] = {
#embed "../../../data/chars/bureaucracy.def"
	, 0 };

static struct {
	nativeVal_t tableFile;		/* BureaucracyTableFile: any value, CreateBureaucracyClass gets it */
} bureaucracy;

/* OBJLIB 5: the mirror in the module dict, read where the script reads it */
static const nativeVar_t bureaucracy_vars[] = {
	{ "BureaucracyTableFile", &bureaucracy.tableFile, 0, NV_ANY },
	{ NULL, NULL, 0, 0 },
};

/* text + v of the script (the engine reads the result up to a 0 byte) into b */
static byte Bureaucracy_Cat(nativeBuf_t *b, const char *text, const nativeVal_t *v) {
	nativeVal_t t, r;
	Native_SetStr(&t, text);
	if (!Native_Arith('+', &t, v, &r)) {
		return 0;
	}
	Native_BufAdd(b, Native_Text(&r), -1);
	Native_Free(&r);
	return 1;
}

/* CreateBureaucracyClass(BureaucracyClass, FilePath): the class Bureaucracy<BureaucracyClass> unless it is there
 * (SAct.GetClass, which selects it when it is); it stops where the script stops ("Bureaucracy" + BureaucracyClass
 * and "Models/Misc/" + FilePath raise for values that are no strings, the second after the class is made) */
static byte Bureaucracy_Class(const nativeVal_t *cls, const nativeVal_t *file) {
	nativeBuf_t full;
	Native_BufInit(&full);
	if (!Bureaucracy_Cat(&full, "Bureaucracy", cls)) {
		Native_BufFree(&full);
		return 0;
	}
	CHECK(g_pActClassList);
	g_pAuxActClass = cActClassList_Get(g_pActClassList, full.p);
	byte ok = 1;
	if (!g_pAuxActClass) {
		g_pAuxActClass = cActClassList_Create(g_pActClassList, full.p);
		ok = file->type == NV_STR || Native_Raise(NE_TYPE, "illegal argument type for built-in operation");
		/* OBJLIB 6: the $names go to the .def runner as a dict (MODE_DEFRUN) */
		ok = ok && MODE_DEFRUN(1, 0, "chars/bureaucracy.def", "Class", bureaucracy_def, 2, "cls", cls, "file", file);
	}
	Native_BufFree(&full);
	return ok;
}

/* one desk on the dummy name at pos with the angles ang; list the name of the entity list of the desks */
static byte Bureaucracy_Desk(const char *name, const double *pos, const double *ang, const nativeVal_t *list) {
	/* names[1] of string.splitfields(name, "_") */
	const char *a = strchr(name, '_');
	if (!a) {
		return Native_Raise(NE_INDEX, "list index out of range");
	}
	const char *b = strchr(a + 1, '_');
	nativeVal_t kind;
	Native_SetText(&kind, a + 1, b ? (int)(b - a - 1) : (int)strlen(a + 1));
	if (!Native_Import("Bureaucracy", bureaucracy_vars)) {
		Native_Free(&kind);
		return 0;
	}
	/* through the module (a mission may put another function there) */
	byte ok = PORT_CALL(Bureaucracy, CreateBureaucracyClass, NULL, "vv", &kind, &bureaucracy.tableFile);
	if (!ok) {
		Native_Free(&kind);
		return 0;
	}
	/* Scrap.CreateEntity(name, x, y, z, "WithLife"): the script's handle finds the entity by its name */
	cEntity *e = cWorld_CreateEntity(g_pWorld, name, (float)pos[0], (float)pos[1], (float)pos[2], "WithLife");
	char ref[0x80];
	Str_Copy(ref, e ? e->name : "", sizeof(ref));
	e = e ? World_FindEntity(ref) : NULL;
	if (!e) {
		/* l.Ang = ... on the None CreateEntity gives */
		Native_Free(&kind);
		return Native_Raise(NE_TYPE, "attribute-less object (assign or del)");
	}
	nativeBuf_t ctrl;
	Native_BufInit(&ctrl);
	Native_BufAdd(&ctrl, "Bureaucracy", -1);
	Native_BufAdd(&ctrl, Native_Text(&kind), kind.n);
	Native_Free(&kind);
	nativeVal_t v;
	Native_SetVec(&v, ang[0], ang[1], 0.0);
	ok = Native_EntSet(e, ref, "Ang", &v);
	if (ok) {
		Native_SetText(&v, ctrl.p, ctrl.len);
		ok = Native_EntSet(e, ref, "ActCtrl", &v);
		Native_Free(&v);
	}
	Native_BufFree(&ctrl);
	Native_SetInt(&v, 0);
	ok = ok && Native_EntSet(e, ref, "ActiveCMask", &v);
	Native_SetInt(&v, 1);
	return ok && Native_EntSet(e, ref, "MapCollision", &v) && Native_EntSet(e, ref, "EntityList", list);
}

/* Init(fromViewer=0): the entity list of the desks and a desk on every dummy DM_DesktopFunctionary_* */
static byte Bureaucracy_Init(int fromViewer) {
	nativeVal_t list;
	const char *l;
	if (fromViewer) {
		Native_SetStr(&list, "DesktopsFunctionary");
	} else if (!Native_GVarGet("DesktopsFunctionaryListName", &list)) {
		return 0;
	}
	/* Scrap.CreateEntityList(list) */
	byte ok = Native_ArgStr(&list, 1, &l);
	if (ok) {
		CHECK(cWorld_NewEntityList(g_pWorld, l));
		DummySearch_Start("DM_DesktopFunctionary_*", 1);
	}
	for (;;) {
		char name[0x40];
		vec3_t pos;
		float ang[2];
		byte flag;
		if (!ok || !DummySearch_Next(name, sizeof(name), &pos, ang, &flag)) {
			break;
		}
		/* the dummy as Scrap.NextDummySearch gives it */
		const double p[3] = { pos.x, pos.y, pos.z };
		const double a[2] = { (double)ang[0] * DEG_PER_PI / M_PI_D, (double)ang[1] * DEG_PER_PI / M_PI_D };
		ok = Bureaucracy_Desk(name, p, a, &list);
	}
	Native_Free(&list);
	return ok;
}

/* ---- the adapters of the module table (OBJLIB 7: arguments and the errors of the Python calls) ---- */

/* CreateBureaucracyClass(cls, file): an argument without a native form (an NV_OBJ of Native_Arg) is None, which the
 * function refuses with the same error (only a class instance with __radd__ could do otherwise) */
static byte Bureaucracy_CreateBureaucracyClass_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	nativeVal_t none;
	(void)out;
	if (n != 2) {
		return Port_Arity(n, 2);
	}
	Native_SetNone(&none);
	return Bureaucracy_Class(NV_REFS >> a[0].type & 1 ? &none : &a[0], NV_REFS >> a[1].type & 1 ? &none : &a[1]);
}

MODE_ADAPTER(Bureaucracy, CreateBureaucracyClass)

/* Init(fromViewer = 0): the import of the function, then the truth of the argument (not given: 0) */
MODE_KW(Bureaucracy, InitPy, "|O", "fromViewer") {
	int fromViewer = Mode_Import("string") ? given[0] ? Mode_Truth(&v[0]) : 0 : -1;
	return fromViewer >= 0 && Bureaucracy_Init(fromViewer);
}

PyMethodDef g_BureaucracyMethods[] = {
	{ "CreateBureaucracyClass", Bureaucracy_CreateBureaucracyClass, 1, NULL },
	{ "Init", (PyCFunction)(void (*)(void))Bureaucracy_InitPy, 3, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(Bureaucracy)
