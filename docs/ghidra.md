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

`rebuild.sh` creates a new project (default `ghidra/Scrapland`, must not exist). It imports the exe and runs auto analysis (about 2 minutes). Then it runs ImportHeaders twice (`Python.h`, then the engine headers), CheckLayouts, the passes NamePyBindings, NameVtables, NameFromStrings, then ImportSymbols, so names from `symbols/` win, and ReplacePlaceholders last. About 5 minutes in total.

`headless.sh` runs one script on `ghidra/Scrapland` without new analysis.

Export after working in the Ghidra GUI: close the project in the GUI first (Ghidra locks it), then

```
tools/ghidra/headless.sh ExportSymbols.java symbols
```

and commit `symbols/`.

## Headers

`ImportHeaders.java` parses C headers into the program data types with Ghidra's preprocessor and C parser:

```
tools/ghidra/headless.sh ImportHeaders.java <header> [-I<dir> ...] [-D<define> ...] [-C<from>=<to> ...]
```

Ghidra files each type under a category named after its header (`/object.h/PyTypeObject`, `/game.h/cWorld`). A type that a header uses before its own header defines it stays in the category of the first header (`cEntity` is in `/game.h`, which points to it first).

ImportHeaders runs the steps of Ghidra's `CParserUtils.parseHeaderFiles`, with two passes on the preprocessed text before the parser. The first cuts the body of every function definition to a `;`, so the `static inline` functions of the engine headers become prototypes. Ghidra's parser is C89 (no declaration in a `for`, no statement expressions) and its preprocessor does not stringify (`#expr` of `CHECK`), and only the types are wanted. The second puts the value of an enum constant into an array size (`weaps[AIVS_NUM]`), which Ghidra's parser takes as 0: a first parse into a scratch manager gives the values. `-C<from>=<to>` sets a calling convention afterwards, see below. The parser input is saved as `ImportHeaders.c` in Ghidra's user temp directory (`/tmp/<user>-ghidra`), the line numbers of parse errors refer to it.

### Python 1.5.2

`third_party/python-1.5.2/` holds the `Include/` directory of Python 1.5.2 (plus `PC/config.h` saved as `Include/config_pc.h`, not used for parsing) and `COPYRIGHT` (Stichting Mathematisch Centrum license, which allows redistribution with the notice). It gives the exact struct layouts, for Ghidra and for the C code (the hook DLL that once used it is gone since 2026-11-01, see tag `hooked-final`).

`tools/ghidra/include/` has stand-in headers for the Ghidra parse: `stdio.h` with only `typedef struct _iobuf FILE`, `stdarg.h` with `va_list`, `stdint.h` and `stddef.h` with the i686 MSVC sizes (long 4 bytes), and empty `string.h`, `errno.h`, `stdlib.h`. `third_party/python-1.5.2/config/config.h` sets prototypes on, int/long/pointer 4 bytes, `DL_IMPORT`/`DL_EXPORT`.

Result: 498 types, no parse errors. `PyTypeObject` is 0x6c bytes, which matches the exe (1.5.2 layout without `COUNT_ALLOCS`).

### Engine headers

`tools/ghidra/include/scrap_all.h` includes all 143 headers of `include/scrap`. Most come in alphabetical order (`prof.h` needs `game.h` before it, `readdata.h` needs `e3dutil.h`). As in the sources, `FXMESSENGER_TRAILS` of `fxcharactions.h` is undefined before `embeddedfxchar.h` defines its own; Ghidra's preprocessor would keep the first definition. gcc compiles `scrap_all.h` too, without warnings.

```
tools/ghidra/headless.sh ImportHeaders.java tools/ghidra/include/scrap_all.h -Itools/ghidra/include -Iinclude \
	-Ithird_party/python-1.5.2/config -Ithird_party/python-1.5.2/Include -DSD_GHIDRA -C__vectorcall=__thiscall
```

The GCC and C23 parts the parser does not know are handled for the Ghidra parse only, by `tools/ghidra/include/gccext.h` (included by `scrap_all.h` with `SD_GHIDRA`):

| construct | in the parse |
|-----------|--------------|
| `static_assert`, `SD_OFFSET`, `SD_SIZE` | gone (CheckLayouts checks them instead) |
| `typeof` | its argument, only used in macros of function bodies |
| `bool`, `true`, `false` | `_Bool`, 1, 0 |
| `__thiscall` | `__vectorcall`, set back by `-C__vectorcall=__thiscall`: Ghidra's parser has no `__thiscall`, and no header uses `__vectorcall` |
| `__attribute__((...))`, `[[noreturn]]` | Ghidra's parser takes them (and ignores `packed`) |
| `__asm__`, `__builtin_*`, statement expressions, `#expr`, C99 statements | only in function bodies, which ImportHeaders cuts |

The headers are written for both compilers in two points, as Ghidra's parser gets these wrong:

- Packed structs use `#pragma pack(push, 1)` and `#pragma pack(pop)` instead of `__attribute__((packed))`, which Ghidra ignores (`bmp.h`, `meshd3d.h`).
- A member that points to an array uses a typedef of the array (`tri16_t *pTri`, `ammoNodes_t *nodes`). Ghidra drops the pointer of `uint16_t (*pTri)[3]` and makes the member the array itself.

The stand-in headers need no Windows types: the engine headers use their own (`exe_rect`, `void *` for handles, `int` for `HRESULT`).

Result: 11203 data types (with the pointers and arrays Ghidra makes), 148 function bodies cut, 1 enum array size, 3440 function types set to `__thiscall`, no parse errors. The update in place on 2026-10-30 added 164 data types.

### Layout check

`CheckLayouts.java` reads the `SD_SIZE` and `SD_OFFSET` lines of `include/scrap` and compares them with the imported types (sizes, member offsets, also in anonymous unions). `rebuild.sh` runs it after the import.

```
tools/ghidra/headless.sh CheckLayouts.java include/scrap
```

Result: 542 sizes and 1054 offsets checked, 0 wrong. For example `cWorld` is 0x21cc bytes with `money` at 0x2090 and `items` at 0x20e0, `CWinApp` 0x90, `cVehicleEntity` 0x814, `PyTypeObject` still 0x6c. A one-time check against gcc (a C file of `_Static_assert` lines from all imported structs, compiled with i686-w64-mingw32-gcc) agreed on all 710 struct and union sizes and 6894 member offsets.

### Placeholders

The analysis makes a 1 byte structure for each RTTI class at the root (`/CEscena`, `/cEntity`) and the demangler some in `/Demangler` (`/Demangler/PyObject`). The `this` of the `__thiscall` methods of a class gets that placeholder as long as it exists, not the header type.

`ReplacePlaceholders.java` replaces each placeholder that has a header type of the same name, or of its English name from `tools/rename/names.tsv` (`CEscena` -> `CScene`). It also maps template class names: a placeholder like `/cGridElements<InfPointVis*>` goes to the header type `cGridElements_InfPointVisPtr` (`*` becomes `Ptr`, `<` and `,` become `_`, `>` and spaces go). Ghidra then points every use at the header type. Only structures without components go, and only for a header type that is no placeholder itself. `rebuild.sh` runs it last.

```
tools/ghidra/headless.sh ReplacePlaceholders.java tools/rename/names.tsv include/scrap third_party/python-1.5.2/Include
```

Result: the new C types for the RTTI classes (commit 13d8691 for the AI and pathfinding classes, bb7b1d1 for the others) replaced 53 more placeholders: 47 by the type of the same name, 6 by the English or template name (`/CEntradaNula` by `/e3dinput.h/CNullInput`, `/CMotorApp` by `/scrapmain.h/CEngineApp`, `/cObjElementFXSistPartDuDv` by `/fxmisc.h/cObjElementFXParticleSystemDuDv`, `/cBinaryHeap<ElemAStar*>` by `/astar.h/cBinaryHeap_ElemAStarPtr`, and the two cGridElements templates). Before that, `/cEntity` was replaced by `/game.h/cEntity`, `/Demangler/PyObject` by `/object.h/PyObject`, `/CEscena` by `/node3d.h/CScene`, `/CNodo3D` by `/node3d.h/CNode3D` and `/CMallaD3D` by `/render.h/CMeshD3D`. 200 stay, all library classes: std exceptions, `type_info`, the CRT demangler nodes (`charNode`, `pcharNode`, `pDNameNode`, `DNameStatusNode`, `pairNode`), the Steam `CCallback` templates, the GOG/Galaxy listeners, and in `/Demangler` the CRT, STL, lambda and Galaxy types. Library code is not decompiled, so they stay.

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

Types come from C headers imported with ImportHeaders: Python 1.5.2 and the engine headers of `include/scrap/` (Phase 3, see Headers). The headers are the source of truth, so types are not exported from Ghidra: a change goes into the header, and a rebuilt or updated project imports it.

## Round trip check

How the pipeline was verified: rebuild into a scratch project, export, diff against `symbols/`.

Result on 2026-10-25: globals and comments identical. Functions differed by one extra function found in the clean pass order. The project database was then rebuilt with `rebuild.sh`, so both match.

After adding the header import, the project database was rebuilt with `rebuild.sh` (5 minutes) and exported. Compared with the previous `symbols/`, the function list is identical (15726 functions). The differences are only the new signatures (464 binding functions) and the header data types on tables and type objects.

After adding the engine headers on 2026-10-30, the project was updated in place (ImportHeaders, CheckLayouts, ReplacePlaceholders) and exported: the export is the same as before the update. A scratch project from `rebuild.sh` (3.5 minutes) exports the same globals and comments and differs in two functions only (`MemPools_PrintAll` named by ImportSymbols, `PyTuple_SetItem` without its signature), from the order of the passes, not from the types.

## ScrapHacks

ScrapHacks (`ref/ScrapHacks`, gitdab.com/ReScrap/ScrapHacks) targets the 2004 exe. Its rizin database `Scrap.rzdb` names 406 `py.*` functions and vtable slots (`fcn.vmt.<Class>.<n>`), both already covered by the passes, so nothing was ported automatically.

Its notes (`notes/src/Classes.md`, `World.md`, `Variables.md`, `Python_Modules.md` with the Spanish doc strings) are reference for struct layouts. Porting by Version Tracking would need the 2004 Scrap.exe.

## Next steps

- Name engine functions from log and assert strings (e.g. `"cPy_Function::SetFunc: ..."`).
- The hook DLL work of phase 3 is history: `docs/hookdll.md` is gone since 2026-11-01, see tag `hooked-final`.
