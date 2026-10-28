/*
 * The built-in modules of the Python library of ScrapC.exe, the table of Scrap.exe (rva 0x428d58) in its order: the
 * engine modules (src/game/PyInterface), then the standard ones of the 1.5.2 Windows build that the game kept.
 */
#include "Python.h"

extern void initScrap Py_PROTO((void));
extern void initSInput Py_PROTO((void));
extern void initSNet Py_PROTO((void));
extern void initSWeap Py_PROTO((void));
extern void initSScorer Py_PROTO((void));
extern void initSVec Py_PROTO((void));
extern void initSSound Py_PROTO((void));
extern void initSAI Py_PROTO((void));
extern void initSAct Py_PROTO((void));
extern void initSLogic Py_PROTO((void));
extern void initSFX Py_PROTO((void));
extern void initerrno Py_PROTO((void));
extern void initmath Py_PROTO((void));
extern void initnew Py_PROTO((void));
extern void initregex Py_PROTO((void));
extern void initsignal Py_PROTO((void));
extern void initstrop Py_PROTO((void));
extern void init_locale Py_PROTO((void));
extern void PyMarshal_Init Py_PROTO((void));
extern void initimp Py_PROTO((void));

struct _inittab _PyImport_Inittab[] = {
	{ "Scrap", initScrap },
	{ "SInput", initSInput },
	{ "SNet", initSNet },
	{ "SWeap", initSWeap },
	{ "SScorer", initSScorer },
	{ "SVec", initSVec },
	{ "SSound", initSSound },
	{ "SAI", initSAI },
	{ "SAct", initSAct },
	{ "SLogic", initSLogic },
	{ "SFX", initSFX },
	{ "errno", initerrno },
	{ "math", initmath },
	{ "new", initnew },
	{ "regex", initregex },
	{ "signal", initsignal },
	{ "strop", initstrop },
	{ "_locale", init_locale },
	{ "marshal", PyMarshal_Init },
	{ "imp", initimp },
	/* the next three are set up by Py_Initialize, they have no init function */
	{ "__main__", NULL },
	{ "__builtin__", NULL },
	{ "sys", NULL },
	{ 0, 0 }
};
