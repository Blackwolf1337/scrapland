# English names (stage 4)

Stage 4 translated the Spanish names of the original to English. Everything was done by the tool in `tools/rename`, so
the run can be repeated, and it was checked against the objects of the build before the rename.

## What was renamed and what stays

Every identifier of our C (`src/`, `include/`) with a Spanish word (or a Spanglish one: `Analize`, `Objetive`, `Quanto`)
is English now: classes, functions, macros, struct fields, locals, parameters, the defines of the vtables
(`CESCENA_VFTABLE`) and the include guards. These are 2625 identifiers. The full table of them, one row old and new
each, is `tools/rename/names.tsv`.

81 files of `src/` and `include/` were moved with `git mv`. The old and new paths are in `tools/rename/paths.tsv`. The
folders were English already. The uses follow the names: the `#include` lines, the comments, `CMakeLists.txt` (it globs
`src/game/*.c`, so only a comment changed), the test scripts (the names of `SDTest.Call/Peek/Poke`, `sym()`,
`testsyms.inc`, the check labels), the tools (the tables of `initpat.py`), the `USER_DEFINED` rows of
`symbols/functions.tsv` and `symbols/globals.tsv`, the users column of `tools/standalone/datamap.txt` and the docs.

Never changed:

- the string literals: the module and method names of the script API, the names of engine variables such as
  `"R_FogEnable"` or `"R_NodoR_Count"`, the file names in the packs, the texts, the CHECK texts of the original in
  `Strings*.c`, the RTTI names in the data
- the source paths of the original in comments (`Engine/Escena.cpp`, `Util/Comodin.cpp`), the sections of the ini files
  in comments (`[niebla]` of `m3d.ini`) and the comments that quote a doc string of the script API
- the names of the external headers: Win32, DirectX, the CRT, Python 1.5.2, fmod, Steam, Galaxy
  (`GalaxyListenerRegistrar` stays)

The C identifiers that come from a Python visible name are English too. `str_PyMethod_SAI_AnalyzeGraph` holds the
string `"AnalizeGraph"`, `GVar_R_RenderNode_Count` is the engine variable `"R_NodoR_Count"`, and `SScorer_GetCurrent` is
the C function behind the method `"GetActual"`. The string stays, only the C name changes.

## The tool

`tools/rename` holds:

- `glossary.tsv`: 354 Spanish words with their English (given before stage 4). `unsure.tsv`: 47 words to decide from
  the code (given).
- `overrides.tsv`: the decisions, one per line (kind, old, new, note).
- `english.py`: `english(name)` splits a name at the underscores and the case changes (`CNodo3D_SetPadre`: `C Nodo 3 D _
  Set Padre`), translates the Spanish words and joins them in the case pattern of the old name (`CEscena` -> `CScene`,
  `g_PadreNodo` -> `g_ParentNode`, `MALLAD3D` -> `MESHD3D`). It is idempotent: `english(english(x)) == english(x)`. The
  generators import it.
- `ctok.py`: a tokenizer of C and Python (identifiers, strings, comments, include paths) and the name spaces of a C file
  without preprocessing (`Scopes`: the names at file scope, each function and macro body, each struct, the members).
- `rename.py`: plans and applies.
- `objcmp.py`: the proof.

The kinds of `overrides.tsv`:

- `word`: a word of `unsure.tsv`, or a glossary word with another English (an empty new: the word stays). Abbreviations
  stay abbreviations: `sig` -> `next`, `ant` -> `prev`, `tam` -> `size`, `din` -> `dyn`, `esc` -> `scale`, `ult` ->
  `last`, `aten` -> `atten`.
- `weak`: a word that is English too, translated only next to a Spanish word of the same name: `de` (`DeActivate`
  stays, `Nodo_EsDeJuego` is translated), `media`, `radio`, `solo`, `la`, `en`, `ir`, `no`, `mayor` (the Mayor of the
  game).
- `compound`: a run of letters without a case change (`LISTAVERT`, `mallatri`, `CESCENA` in `CESCENA_VFTABLE`) and its
  words.
- `phrase`: word order. The words of a Spanish compound in English order: `lista vert` -> `vert list` (`CListaVert` ->
  `CVertList`), `malla tri` -> `tri mesh` (`CMallaTri` -> `CTriMesh`), `tri malla` -> `mesh tri` (`CTriMalla` ->
  `CMeshTri`), `sist part` -> `particle system`, `nodo r` -> `render node`, `mapa sombras` -> `shadow map`,
  `fundido ent/sal` -> `fade in/out`, `tam pantalla` -> `screen size`. The new words take the case pattern of the old
  positions.
- `name`: a whole identifier, where word by word gives bad English or a collision.
- `piece`: a part between underscores (`CArchINI` in `Fini_CArchINI_X` is `CIniFile`).
- `prefix`: the start of identifiers (`Arch_` -> `DataFile_`, `Motor_` -> `Engine_`, `MOTOR_` -> `ENGINE_`, `motorIni`
  -> `engineIni`).
- `keep`: a whole identifier that stays.
- `path`: a file with a new name chosen by hand.
- `local`: a local or parameter of one function or prototype of one file.
- `member`: a name where it is a member (`x.m`, `x->m`, the field, `SD_OFFSET`).
- `prose`: an English word that stays in comments and docs (`actual`, `fin`, `ant`, `esc`, `todo`, `motor`, `sig`).

The commands of `rename.py`:

- `python3 tools/rename/rename.py`: dry run. It prints the counts and the problems and exits 1 on a problem. `-v` lists
  every rename.
- `--table` writes `names.tsv` and `paths.tsv` from the plan.
- `-w` applies: it rewrites the files, moves the files with `git mv` and writes the tables. `--only names|paths|docs`
  does a part.
- `--check` exits 1 when a file would still change or move or a problem is found. It passes now.

`rename.py` finds our identifiers as the identifier tokens of `src/` and `include/` that no external header of the
build declares (the headers come from the dependency files `build/**/*.d`; their names at file scope, macros and
members). It stops on these problems:

- two old names that become one
- two names of one name space (all names at file scope and of `symbols/*.tsv`, each function or macro body, each
  struct) that become one
- a new name that is a macro, ours or of the external headers (`near` of `windef.h` for `cerca`, `floor` of the CRT for
  `suelo`, `free` for `liberar`)
- a name at file scope that becomes a name of an external header
- a C keyword (`static` for `estatica` and `est`)
- a new name that `english()` would change again
- two paths that become one, or a new path that exists (`input.h`, `world.h`, `hashtable.h`)

`objcmp.py <objects before> <objects after>` compares the object files of two builds modulo `names.tsv` and
`paths.tsv`: the disassembly with relocations (the old symbol names mapped), the immediates of instructions relocated
against `.rdata` masked (the CHECK texts stringize names, so their lengths change), and the relocations of `.data` and
`.rdata` by symbol. It is the proof that the rename changed only names.

## The decisions

The unsure words were decided from the code (`overrides.tsv`, kind `word`):

- `NodoR` is the render node (`Engine/NodoR.cpp`, `CRenderNode`). `part` is particle in `SistPart` and English
  elsewhere.
- `est` is static (`CTextEst`, the static texture of a material next to `TextAni`: `CTextStatic`).
- `cab` is `cabecera` (header) in `TXF_cab`, `g_TexCab` and `g_BmpFileCab`, and a taxi cab in the phone cab warnings.
- `ver` is show in `g_FlareVerInfo` (`ver_info` of `m3d.ini`) and `g_VerUniones` (`g_ShowJoins`), and version in
  `MST_VER`.
- `pri` is the first glyph of a font (`pri..ult` -> `first..last`) and `g_M3DPriFrame`. `tramas` is dithering
  (`g_Tramas` -> `g_Dither`). `sec` is sector in `g_BspSecOk` and `g_BspSecVisto`, seconds in `g_FrameSec`. `escr` is
  write (`g_MapaSombrasEscrTemp`). `reparto` is share, `finp` is `endp`, `nEcu` is `nEquator`, `mesc` is `mscn`.
- These stay: `desc`, `repos`, `sup` (`CAStarSup`, `CNodesWorldSup`: the base class, unclear), `txf`, `ric`, `mst`,
  `amor` (`AmorPieceWeight`: a typo of armor in the engine variable, it stays like the variable), `od` (`SAI_GetOD`),
  `expo`, `lum`, `cm`, `ab`, `nw`, `rad`, `fisq`, `uni`, `hn`, `xtr`, `inf`, `mi`, `tg`, `bip`, `ase`, `lm`, `sai`,
  `vm`, `mm`, `ve`, `wsm`, `dev`.

More decisions:

- `motor`: the 3D engine (`Engine/Motor.cpp`) is the engine (`Motor_`, `MOTOR_`, `CMotorApp` -> `CEngineApp`, `g_Motor`
  -> `g_Engine`, `Motor.c` -> `Engine.c`). A vehicle has motors (`cVehicleMotors`, `cVehicleMotor`, the engine
  variables `Motor0Model`, `MotorTrustRotation`): there motor is English and stays.
- `suelo` is ground (`CSuelo` -> `CGround`, `Suelo.c` -> `Ground.c`). `floor` is the math function of the CRT and the
  floors of the buildings (`World_NumFloors`).
- `cad` (`cadena`) is `String_` (`Cad_Copy` -> `String_Copy`). `Str_` is the string module of the game (`StrUtils.c`,
  `Str_Copy` is another copy of the exe).
- `arch` is File (`CArchIO` -> `CFileIO`), `CArchINI` is `CIniFile`, `cArchDatos` is `cDataFile` and its functions
  `Arch_*` are `DataFile_*` (`Arch_dat.c` -> `DataFile.c`), `cArchBlq` is `cBlockFile`.
- `actual` is current, a false friend (`g_CamActual` -> `g_CamCurrent`, `Camera_SetActual` -> `Camera_SetCurrent`).
- `esc`: as a local it is an `escena` (`CEscena *esc` -> `scn`), as a member a scale (`CListaVert.esc` -> `scale`).
  Five locals that are scales and the Escape key of `Splash_Show` have local overrides.
- The `m3d.ini` values next to an engine variable of the same English name keep `Ini`: `g_CieloPosScale` ->
  `g_SkyPosScaleIni` (`g_SkyPosScale` is the engine variable), `g_NieblaActiva` -> `g_FogActiveIni`, and so on.
- Names that would meet an existing one got another English: `Ruta_Split` -> `Path_SplitDrive` (`Path_Split` of
  `StrUtils.c` splits without the drive), `Escena_Release` -> `SceneList_Release`, `Motor_LoadIni` ->
  `Engine_LoadVarsIni` (`Engine_LoadIni` reads the `m3d.ini` of a level), `Sectores_Free` -> `Sectors_FreeAll`,
  `CBmpFlt_Rellenar` -> `CBmpFlt_FillGaps`, `MAX_RUTA` -> `MAX_FILEPATH` (`MAX_PATH` of `windows.h`), `g_Anisotropia` ->
  `g_AnisotropyCur`.
- Three headers would have met the game's: `entrada.h` -> `e3dinput.h`, `mundo.h` -> `e3dworld.h`, `tablahash.h` ->
  `e3dhashtable.h` (`input.h`, `world.h` and `hashtable.h` are the game's; `e3d` as in `e3dutil.h`). `CMundo` is
  `CWorld` (`cWorld`, lower c, is the game's world).
- File names chosen by hand: `Arch_dat.c` -> `DataFile.c`, `CaptPant.c` -> `ScreenCapture.c`, `EventoAnim.c` ->
  `AnimEvent.c`, `Motor.c` -> `Engine.c`.
- By hand before the run: `NODO_BONE` of `LeeM3D.c` was `NODO_HUESO` of `escena.h` (the same bit 20), it is one define
  now (`NODE_BONE`). `CModelo` had `scale` and `escala` (the scale the scene was made with), `escala` is `escalaBase`
  -> `scaleBase`.
- English abbreviations of the game code stay (local overrides): `sig` of `SDTest_Call` (the signature) and of
  `crt_signal` (the signal number), `pri` of fmod (the priority), `es` of `ConsoleWin_SaveOutput` (the `EDITSTREAM`),
  `ind` of `App_PickLoadingTip` (`LoadingTips_Ind`).
- `initpat.py` names an object alone when its name starts with the class: `ZonaPintar_ZonasPintar` became `PaintZones`.

## The generators

`vtables.py`, `strings.py` and `initpat.py` map the names they derive from the exe through `english()` (`sdexe.py`
imports it): the RTTI classes (`CEscena` -> `CScene_vftable`), the method names of the script API
(`str_PyMethod_*`), the engine variables and natives (`Init_GVar_*`, `Init_NativeFunc_*`). `globals.py`, `xctable.py`
and `testsyms.py` take their names from the C, the map and `hooks.inc`, which are renamed.

Two of them were not idempotent before stage 4 and are fixed. `strings.py` read the rvas of `src/core/testaddrs.inc`
(stage 3c2, written from the C objects) as uses of the C and cut the strings anew at the objects of `globals.py`
(`D3DFMT_UNKNOWN` at 0x3de7fc instead of `MT_UNKNOWN` at 0x3de800). `initpat.py` knew only the defines before stage 3b5
(`Game_Ptr`, `GAME_DATA`) for the names of the objects and wrote `Init_CGameModel_447478` for
`Init_CGameModel_SirenModel`.

`strings.py`, `globals.py`, `vtables.py`, `initpat.py`, `xctable.py` and `testsyms.py` with `-w` change nothing now.
Run them in the order of `docs/standalone.md`: `strings.py -w`, build `scrapdata_check`, `globals.py -w`, build, then
the others.

## The proof

The results of 2026-10-29:

- Both builds are clean (`cmake --build build --clean-first`: no warning but the two of the vendored Python, as before).
- `objcmp.py`: all 882 objects of the build before the rename (`scrapcode`, `scrapcode_sa`, `scrapdata_check`,
  `python152`, `ScrapC`, `dinput8`) are the same after it but for the names. The only difference is the order of the
  table of `testmod.c` (`testsyms.inc` is sorted by name).
- `sigcheck.py`, `testsyms.py` and `rvacheck.py` pass. `testsyms.py`: 12145 uses of 6151 names as before, 0 scripts to
  rewrite. `rvacheck.py`: 554 `GAME_FUNC` and script functions name their hooks. `hookcheck.py` gives the same 120
  findings, mapped through `names.tsv` line by line.
- The hook DLL difftest and `DIFFTEST_STANDALONE=1` match in all three scenarios with the counts of before: sdtest 9965
  checks (116 raise an exception in both), sdcall 29078 (0), sdlevel 19095 (89). 9957 of 9957 hooks are installed.
- `cmake --build build --target datacheck` passes: `datagen.py: 14378 C objects match Scrap.exe 1.6.1, 2 pointers into
  the Python lead to the real one` and `rsrccheck.py: 23 resources match`.

After a merge of a branch with Spanish names: run `rename.py` (dry run), add overrides where it reports problems, run
`rename.py -w`, then the generators. The Ghidra project takes the new names of `symbols/functions.tsv` and
`symbols/globals.tsv` with `tools/ghidra/headless.sh ImportSymbols.java symbols`.

## Classes

The classes with a Spanish name, old and new. The third column marks the classes that have RTTI in `Scrap.exe`. Every
other identifier is in `tools/rename/names.tsv`.

| Old | New | |
|-----|-----|-|
| `CAlturas` | `CHeights` |  |
| `CAnimacion` | `CAnimation` |  |
| `CAnimGrupo` | `CAnimGroup` |  |
| `CAnimGrupos` | `CAnimGroups` |  |
| `CAnimNodo` | `CAnimNode` |  |
| `cArchBlq` | `cBlockFile` |  |
| `cArchDatos` | `cDataFile` |  |
| `CArchINI` | `CIniFile` |  |
| `CArchIO` | `CFileIO` |  |
| `CCamara` | `CCamera` | RTTI |
| `CColisBBox` | `CCollisionBBox` |  |
| `CColisTri` | `CCollisionTri` |  |
| `CDigitos` | `CDigits` |  |
| `cEmbeddedFXMoto` | `cEmbeddedFXBike` | RTTI |
| `CEntradaDInput` | `CInputDInput` | RTTI |
| `CEntradaNula` | `CNullInput` | RTTI |
| `CEntradaWindows` | `CInputWindows` | RTTI |
| `CEscena` | `CScene` | RTTI |
| `CFondo2D` | `CBackground2D` |  |
| `CFuente3` | `CFont3` |  |
| `CFuenteGrid` | `CFontGrid` |  |
| `cFXElementsOrbitBoya` | `cFXElementsOrbitBuoy` | RTTI |
| `cFXElementsOrbitBoyaController` | `cFXElementsOrbitBuoyController` | RTTI |
| `CGrafico2D` | `CGraphic2D` |  |
| `CGrafico3D` | `CGraphic3D` | RTTI |
| `CImagen` | `CImage` |  |
| `cLeePila` | `cReadStack` |  |
| `cLineaDat` | `cDatLine` |  |
| `cListaDin` | `cDynList` |  |
| `CListaEscena` | `CSceneList` |  |
| `CListaGrf2D` | `CGraphic2DList` |  |
| `CListaMat` | `CMatList` | RTTI |
| `CListaModelo` | `CModelList` |  |
| `CListaVert` | `CVertList` |  |
| `CLuz` | `CLight` | RTTI |
| `CMallaD3D` | `CMeshD3D` | RTTI |
| `CMallaTri` | `CTriMesh` | RTTI |
| `CMapa` | `CMap` |  |
| `CModelo` | `CModel` | RTTI |
| `CMotorApp` | `CEngineApp` | RTTI |
| `CMundo` | `CWorld` |  |
| `CNiebla` | `CFog` |  |
| `CNodo3D` | `CNode3D` | RTTI |
| `CNodoR` | `CRenderNode` |  |
| `CNodoRef` | `CNodeRef` |  |
| `cObjElementFXSistPart` | `cObjElementFXParticleSystem` | RTTI |
| `cObjElementFXSistPartDuDv` | `cObjElementFXParticleSystemDuDv` | RTTI |
| `CObjNombre` | `CObjName` | RTTI |
| `CParticula` | `CParticle` |  |
| `CParticulaActive` | `CParticleActive` | RTTI |
| `CParticulaBase` | `CParticleBase` | RTTI |
| `CParticulaStatic` | `CParticleStatic` | RTTI |
| `CRadiosidad` | `CRadiosity` |  |
| `CRotacion` | `CRotation` |  |
| `CSistPart` | `CParticleSystem` | RTTI |
| `CSuelo` | `CGround` | RTTI |
| `CSueloHit` | `CGroundHit` |  |
| `CTabla` | `CTable` |  |
| `CTablaHash` | `CHashTable` |  |
| `CTablaHashNodo` | `CHashTableNode` |  |
| `CTextEst` | `CTextStatic` |  |
| `CTextura` | `CTexture` |  |
| `CTriMalla` | `CMeshTri` |  |
| `CVertMalla` | `CMeshVert` |  |
| `CZona` | `CZone` |  |
| `IEntrada` | `IInput` | RTTI |
| `IFXTextura` | `IFXTexture` | RTTI |
| `IObjeto3D` | `IObject3D` | RTTI |

## Files

The moved files, old and new path. The same list is `tools/rename/paths.tsv`.

| Old path | New path |
|----------|----------|
| `include/scrap/animacion.h` | `include/scrap/animation.h` |
| `include/scrap/animgrupo.h` | `include/scrap/animgroup.h` |
| `include/scrap/camara.h` | `include/scrap/camera.h` |
| `include/scrap/entrada.h` | `include/scrap/e3dinput.h` |
| `include/scrap/escena.h` | `include/scrap/scene.h` |
| `include/scrap/fondo2d.h` | `include/scrap/background2d.h` |
| `include/scrap/fuentes.h` | `include/scrap/fonts.h` |
| `include/scrap/fxtextura.h` | `include/scrap/fxtexture.h` |
| `include/scrap/grafico2d.h` | `include/scrap/graphic2d.h` |
| `include/scrap/grafico3d.h` | `include/scrap/graphic3d.h` |
| `include/scrap/leedatos.h` | `include/scrap/readdata.h` |
| `include/scrap/leem3d.h` | `include/scrap/readm3d.h` |
| `include/scrap/listavert.h` | `include/scrap/vertlist.h` |
| `include/scrap/luz.h` | `include/scrap/light.h` |
| `include/scrap/mallad3d.h` | `include/scrap/meshd3d.h` |
| `include/scrap/mallatri.h` | `include/scrap/trimesh.h` |
| `include/scrap/modelo.h` | `include/scrap/model.h` |
| `include/scrap/mundo.h` | `include/scrap/e3dworld.h` |
| `include/scrap/niebla.h` | `include/scrap/fog.h` |
| `include/scrap/nodo3d.h` | `include/scrap/node3d.h` |
| `include/scrap/rendersuelo.h` | `include/scrap/renderground.h` |
| `include/scrap/sistpart.h` | `include/scrap/particlesystem.h` |
| `include/scrap/suelo.h` | `include/scrap/ground.h` |
| `include/scrap/sueloportal.h` | `include/scrap/groundportal.h` |
| `include/scrap/tabla.h` | `include/scrap/table.h` |
| `include/scrap/tablahash.h` | `include/scrap/e3dhashtable.h` |
| `include/scrap/texto.h` | `include/scrap/text.h` |
| `include/scrap/textura.h` | `include/scrap/texture.h` |
| `include/scrap/varios.h` | `include/scrap/misc.h` |
| `src/game/engine3d/Engine/AnimGrupo.c` | `src/game/engine3d/Engine/AnimGroup.c` |
| `src/game/engine3d/Engine/Animacion.c` | `src/game/engine3d/Engine/Animation.c` |
| `src/game/engine3d/Engine/ArchINI.c` | `src/game/engine3d/Engine/IniFile.c` |
| `src/game/engine3d/Engine/ArchIO.c` | `src/game/engine3d/Engine/FileIO.c` |
| `src/game/engine3d/Engine/CalcIlum.c` | `src/game/engine3d/Engine/CalcIllum.c` |
| `src/game/engine3d/Engine/CalcPesos.c` | `src/game/engine3d/Engine/CalcWeights.c` |
| `src/game/engine3d/Engine/CalcSegmentos.c` | `src/game/engine3d/Engine/CalcSegments.c` |
| `src/game/engine3d/Engine/Calculo3d.c` | `src/game/engine3d/Engine/Calc3d.c` |
| `src/game/engine3d/Engine/Camara.c` | `src/game/engine3d/Engine/Camera.c` |
| `src/game/engine3d/Engine/CaptPant.c` | `src/game/engine3d/Engine/ScreenCapture.c` |
| `src/game/engine3d/Engine/EfecMalla.c` | `src/game/engine3d/Engine/MeshEffect.c` |
| `src/game/engine3d/Engine/Entrada.c` | `src/game/engine3d/Engine/Input.c` |
| `src/game/engine3d/Engine/Escena.c` | `src/game/engine3d/Engine/Scene.c` |
| `src/game/engine3d/Engine/EventoAnim.c` | `src/game/engine3d/Engine/AnimEvent.c` |
| `src/game/engine3d/Engine/FXTextura.c` | `src/game/engine3d/Engine/FXTexture.c` |
| `src/game/engine3d/Engine/Fondo2D.c` | `src/game/engine3d/Engine/Background2D.c` |
| `src/game/engine3d/Engine/Fuentes.c` | `src/game/engine3d/Engine/Fonts.c` |
| `src/game/engine3d/Engine/Grafico2D.c` | `src/game/engine3d/Engine/Graphic2D.c` |
| `src/game/engine3d/Engine/Grafico3D.c` | `src/game/engine3d/Engine/Graphic3D.c` |
| `src/game/engine3d/Engine/LeeDatos.c` | `src/game/engine3d/Engine/ReadData.c` |
| `src/game/engine3d/Engine/LeeM3D.c` | `src/game/engine3d/Engine/ReadM3D.c` |
| `src/game/engine3d/Engine/ListaGrf2D.c` | `src/game/engine3d/Engine/Graphic2DList.c` |
| `src/game/engine3d/Engine/ListaVert.c` | `src/game/engine3d/Engine/VertList.c` |
| `src/game/engine3d/Engine/Luz.c` | `src/game/engine3d/Engine/Light.c` |
| `src/game/engine3d/Engine/MallaD3D.c` | `src/game/engine3d/Engine/MeshD3D.c` |
| `src/game/engine3d/Engine/MallaTri.c` | `src/game/engine3d/Engine/TriMesh.c` |
| `src/game/engine3d/Engine/Modelo.c` | `src/game/engine3d/Engine/Model.c` |
| `src/game/engine3d/Engine/Motor.c` | `src/game/engine3d/Engine/Engine.c` |
| `src/game/engine3d/Engine/Mundo.c` | `src/game/engine3d/Engine/World.c` |
| `src/game/engine3d/Engine/Niebla.c` | `src/game/engine3d/Engine/Fog.c` |
| `src/game/engine3d/Engine/Nodo3D.c` | `src/game/engine3d/Engine/Node3D.c` |
| `src/game/engine3d/Engine/NodoR.c` | `src/game/engine3d/Engine/RenderNode.c` |
| `src/game/engine3d/Engine/Radiosidad.c` | `src/game/engine3d/Engine/Radiosity.c` |
| `src/game/engine3d/Engine/RenderCalcIlum.c` | `src/game/engine3d/Engine/RenderCalcIllum.c` |
| `src/game/engine3d/Engine/RenderMallaD3D.c` | `src/game/engine3d/Engine/RenderMeshD3D.c` |
| `src/game/engine3d/Engine/RenderSuelo.c` | `src/game/engine3d/Engine/RenderGround.c` |
| `src/game/engine3d/Engine/SistPart.c` | `src/game/engine3d/Engine/ParticleSystem.c` |
| `src/game/engine3d/Engine/Suelo.c` | `src/game/engine3d/Engine/Ground.c` |
| `src/game/engine3d/Engine/SueloPortal.c` | `src/game/engine3d/Engine/GroundPortal.c` |
| `src/game/engine3d/Engine/Tabla.c` | `src/game/engine3d/Engine/Table.c` |
| `src/game/engine3d/Engine/TablaHash.c` | `src/game/engine3d/Engine/HashTable.c` |
| `src/game/engine3d/Engine/Teclado.c` | `src/game/engine3d/Engine/Keyboard.c` |
| `src/game/engine3d/Engine/Texto.c` | `src/game/engine3d/Engine/Text.c` |
| `src/game/engine3d/Engine/Textura.c` | `src/game/engine3d/Engine/Texture.c` |
| `src/game/engine3d/Engine/UtilEscena.c` | `src/game/engine3d/Engine/UtilScene.c` |
| `src/game/engine3d/Engine/Varios.c` | `src/game/engine3d/Engine/Misc.c` |
| `src/game/engine3d/Util/Arch_dat.c` | `src/game/engine3d/Util/DataFile.c` |
| `src/game/engine3d/Util/Cadenas.c` | `src/game/engine3d/Util/Strings.c` |
| `src/game/engine3d/Util/Comodin.c` | `src/game/engine3d/Util/Wildcard.c` |
| `src/game/engine3d/Util/Errores.c` | `src/game/engine3d/Util/Errors.c` |
| `src/game/engine3d/Util/Listadin.c` | `src/game/engine3d/Util/DynList.c` |
| `src/game/engine3d/Util/Paletas.c` | `src/game/engine3d/Util/Palettes.c` |
| `src/game/engine3d/Engine/DescompAfin.c` | `src/game/engine3d/Engine/AffineDecomp.c` |
| `tools/test/py/sdcall_animacion.py` | `tools/test/py/sdcall_animation.py` |
| `tools/test/py/sdcall_animgrupo.py` | `tools/test/py/sdcall_animgroup.py` |
| `tools/test/py/sdcall_escena.py` | `tools/test/py/sdcall_scene.py` |
| `tools/test/py/sdcall_fuentes.py` | `tools/test/py/sdcall_fonts.py` |
| `tools/test/py/sdcall_grafico2d.py` | `tools/test/py/sdcall_graphic2d.py` |
| `tools/test/py/sdcall_grafico3d.py` | `tools/test/py/sdcall_graphic3d.py` |
| `tools/test/py/sdcall_leem3d.py` | `tools/test/py/sdcall_readm3d.py` |
| `tools/test/py/sdcall_listavert.py` | `tools/test/py/sdcall_vertlist.py` |
| `tools/test/py/sdcall_luz.py` | `tools/test/py/sdcall_light.py` |
| `tools/test/py/sdcall_mallad3d.py` | `tools/test/py/sdcall_meshd3d.py` |
| `tools/test/py/sdcall_mallatri.py` | `tools/test/py/sdcall_trimesh.py` |
| `tools/test/py/sdcall_modelo.py` | `tools/test/py/sdcall_model.py` |
| `tools/test/py/sdcall_nodo3d.py` | `tools/test/py/sdcall_node3d.py` |
| `tools/test/py/sdcall_sistpart.py` | `tools/test/py/sdcall_particlesystem.py` |
| `tools/test/py/sdcall_suelo.py` | `tools/test/py/sdcall_ground.py` |
| `tools/test/py/sdcall_texto.py` | `tools/test/py/sdcall_text.py` |
| `tools/test/py/sdcall_textura.py` | `tools/test/py/sdcall_texture.py` |
| `tools/test/py/sdcall_descomp.py` | `tools/test/py/sdcall_affinedecomp.py` |
