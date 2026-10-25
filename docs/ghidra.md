# Ghidra database and symbols

The Ghidra project (`ghidra/`, gitignored) is a build product. git holds the knowledge as text in `symbols/`, so any Ghidra project can be rebuilt and diffs are reviewable. Addresses in `symbols/` are RVAs (image base 0x400000 subtracted), because Scrap.exe has ASLR.

## Requirements

Ghidra 12.1 in `/opt/ghidra` (`GHIDRA_DIR` to override). Java 21 or newer: the scripts pick the newest JDK in `/usr/lib/jvm` (`GHIDRA_JAVA` to override). The system default Java 17 is too old. Ghidra 12 has no Jython, so the scripts are Java.

## Files in symbols/

| file | content |
|------|---------|
| functions.tsv | rva, size, source (DEFAULT/ANALYSIS/USER_DEFINED/IMPORTED), calling convention, full name with namespace, signature |
| globals.tsv | rva, source, name, data type path, for named non-function labels |
| comments.tsv | rva, kind (PLATE, PRE, EOL, POST, REPEATABLE), text with `\n`, `\t` and `\\` escaped |
| srcstrings.tsv | output of `tools/srcstrings.py`: string, function, source file |
| rtti_classes.txt | MSVC RTTI type names |

Tabs separate the fields, because names and signatures contain commas.

The signature is written as `rettype __cc(type name, ...)`, without the function name and without auto parameters (the `this` of `__thiscall`). Signatures are only written for named functions.

## Commands

```
tools/ghidra/rebuild.sh [dir] [name]
tools/ghidra/headless.sh <Script.java> [args]
```

`rebuild.sh` creates a new project (default `ghidra/Scrapland`, must not exist). It imports the exe and runs auto analysis (about 2 minutes). Then it runs ImportHeaders (`Python.h`), the passes NamePyBindings, NameVtables, NameFromStrings, and ImportSymbols last, so names from `symbols/` win. About 5 minutes in total.

`headless.sh` runs one script on `ghidra/Scrapland` without new analysis.

Export after working in the Ghidra GUI: close the project in the GUI first (Ghidra locks it), then

```
tools/ghidra/headless.sh ExportSymbols.java symbols
```

and commit `symbols/`.

## Headers

`ImportHeaders.java` parses C headers into the program data types with Ghidra's preprocessor and C parser:

```
tools/ghidra/headless.sh ImportHeaders.java <header> [-I<dir> ...] [-D<define> ...]
```

Ghidra files each type under a category named after its header (`/object.h/PyTypeObject`, `/methodobject.h/PyMethodDef`).

`third_party/python-1.5.2/` holds the `Include/` directory of Python 1.5.2 (plus `PC/config.h` saved as `Include/config_pc.h`, not used for parsing) and `COPYRIGHT` (Stichting Mathematisch Centrum license, which allows redistribution with the notice). It gives the exact struct layouts, now for Ghidra and later for the hook DLL.

`tools/ghidra/include/` has stand-in headers for the Ghidra parse: `config.h` (prototypes on, int/long/pointer 4 bytes, `DL_IMPORT`/`DL_EXPORT`), `stdio.h` with only `typedef struct _iobuf FILE`, `stdarg.h` with `va_list`, and empty `string.h`, `errno.h`, `stdlib.h`.

Result: 498 types, no parse errors. `PyTypeObject` is 0x6c bytes, which matches the exe (1.5.2 layout without `COUNT_ALLOCS`).

## Naming passes

All passes only rename functions whose name is still the default `FUN_...`, or a name an earlier pass gave. Names with source USER_DEFINED or IMPORTED are never changed.

### NamePyBindings

Scans initialized data for Python 1.5.2 `PyMethodDef` tables (16 bytes: `ml_name`, `ml_meth`, `ml_flags`, `ml_doc`; ended by `ml_name` 0).

The module name comes from the `Py_InitModule4(name, methods, doc, self, 1007)` call that pushes the table. For object types it comes from the `PyTypeObject` whose `tp_getattr` (+0x20) leads to the table (`tp_name` at +0x0c), one caller level deep (cPyEntity).

Functions become `<module>_<method>`. `__builtin__` gives `builtin_...`. Object types get an `obj` suffix when the name clashes with a module, like `regexobj`. The doc string becomes the plate comment. The pass also names `_PyImport_Inittab`, the module init functions (`initSAct`, `initSInput`, ...) and `Py_InitModule4`.

With imported headers the pass types the tables as `/methodobject.h/PyMethodDef[n]` and the type objects (labels `<name>_Type`) as `/object.h/PyTypeObject`. Every binding function gets the `PyCFunction` signature `PyObject * __cdecl(PyObject *self, PyObject *args)`, plus `PyObject *kw` when `ml_flags` has `METH_KEYWORDS` (2). Header types are only taken from categories ending in `.h`, because the demangler also creates a 1 byte placeholder `/Demangler/PyObject`. Without imported headers the pass falls back to its own `/Scrap/Python/PyMethodDef` and skips the signatures.

Result: 29 tables, 472 functions, among them 290 engine script functions (Scrap, SAct, SAI, SInput, SLogic, SNet, SScorer, SSound, SVec, SWeap, SFX, cPyEntity).

### NameVtables

Walks the 316 RTTI vftables. Unnamed entries become `<Class>::vfNN` (NN = slot number) with `__thiscall`. Classes are handled in order of their RTTI base class count, so an inherited function goes to the most basic class. 1990 functions.

### NameFromStrings

Names functions of the statically linked libraries Python 1.5.2, zlib 1.1.3, libpng 1.0.5 and libjpeg 6a through strings that only one source function contains. Each exe function using such a string votes for that name. A name is taken when it has at least two thirds of the function's votes and no other function has as many votes for it (MSVC inlines small functions). Plate comment: `src: <file>`. 229 functions.

Library sources go to `ref/src` (not in git):

| library | source |
|---------|--------|
| Python 1.5.2 | py152.tgz from python.org |
| zlib 1.1.3 | zlib.net/fossils |
| libpng 1.0.5 | download.sourceforge.net/libpng |
| libjpeg 6a | jpegsrc.v6a from ijg.org |

Regenerate `srcstrings.tsv` with:

```
python3 -I tools/srcstrings.py ref/src/Python-1.5.2/{Objects,Python,Parser} ref/src/Python-1.5.2/PC/{getpathp,import_nt,config}.c ref/src/Python-1.5.2/Modules/{mathmodule,newmodule,regexmodule,regexpr,signalmodule,stropmodule,_localemodule,errnomodule,main,getbuildinfo}.c ref/src/jpeg-6a ref/src/libpng-1.0.5 ref/src/zlib-1.1.3 > symbols/srcstrings.tsv
```

Only the Python modules listed in `_PyImport_Inittab` of the exe are given, so strings shared with unused modules do not lose their uniqueness.

### Function ID

Part of the Ghidra analysis, not a script of this repo. It names the statically linked MSVC CRT and STL.

## State

After the first passes: 15726 functions, 7374 with a name (about 47 percent, most of it CRT/STL from Function ID plus the passes above).

## Data types

Types come from C headers imported with ImportHeaders (Python 1.5.2 so far). The project's own engine headers (`include/scrap/`, Phase 3) will be imported the same way. Types are still not exported from Ghidra.

## Round trip check

How the pipeline was verified: rebuild into a scratch project, export, diff against `symbols/`.

Result on 2026-10-25: globals and comments identical. Functions differed by one extra function found in the clean pass order. The project database was then rebuilt with `rebuild.sh`, so both match.

After adding the header import, the project database was rebuilt with `rebuild.sh` (5 minutes) and exported. Compared with the previous `symbols/`, the function list is identical (15726 functions). The differences are only the new signatures (464 binding functions) and the header data types on tables and type objects.

## ScrapHacks

ScrapHacks (`ref/ScrapHacks`, gitdab.com/ReScrap/ScrapHacks) targets the 2004 exe. Its rizin database `Scrap.rzdb` names 406 `py.*` functions and vtable slots (`fcn.vmt.<Class>.<n>`), both already covered by the passes, so nothing was ported automatically.

Its notes (`notes/src/Classes.md`, `World.md`, `Variables.md`, `Python_Modules.md` with the Spanish doc strings) are reference for struct layouts. Porting by Version Tracking would need the 2004 Scrap.exe.

## Next steps

- Name engine functions from log and assert strings (e.g. `"cPy_Function::SetFunc: ..."`).
- Struct layouts for the RTTI classes.
- Phase 3: hook DLL.
