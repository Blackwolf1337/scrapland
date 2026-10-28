/*
 * Where the Python 1.5.2 of Scrap.exe is (rvas): its code, between the M3D library (rva 0x2d6010) and the MSVC
 * runtime (operator delete at 0x313634), and its data. ScrapC.exe runs the real library of src/python instead
 * (docs/standalone.md, stage 2a); tools/standalone/pymap.py names the symbols in these ranges.
 */
#ifndef SCRAP_ORIGPYTHON_H
#define SCRAP_ORIGPYTHON_H

#define ORIG_PY_CODE_START 0x2da770
#define ORIG_PY_CODE_END 0x313634

/* .data (type objects, method tables, the grammar), .bss, and the common symbols at the end of .bss (PyExc_*) */
#define ORIG_PY_DATA \
	{ { 0x432b50, 0x43cfd8 }, { 0x4e73c0, 0x4e8aa8 }, { 0x4ed3a0, 0x4ed540 } }

#endif
