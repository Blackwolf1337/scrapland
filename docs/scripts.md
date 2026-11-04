# Scripts

The original game runs its menus, HUD, characters, missions and levels from Python 1.5.2 scripts in the game packs (for example `scripts/scorer/pcmenu.py`, about 4500 lines). ScrapC links a real Python 1.5.2 and runs them. Issue #2 on the project's Gitea is to move that script layer to C step by step while the game keeps working. Python goes once nothing is loaded from the packs any more.

## The loader

When the engine loads a module from the script pack (`Pack_PreloadLibrary` in `src/game/PyInterface/PythonInit.c`), `Script_ModuleLoaded` (`src/game/Script/Script.c`) puts the C functions ScrapC has for that module into it under their script names. The table `script_modules` lists them, each entry is a module name and a `PyMethodDef` table. So the code of the pack, other modules and the callbacks named in strings (for example the `OnAccept` value `PCMenu.ChangeVideoMode`) reach the C versions. Everything not ported still runs in Python as before.

`SCRAP_PACK_SCRIPTS=1` (environment variable) turns the C versions off, so the original pack scripts run. It is used to compare a port with the original.

The helpers the ports share are declared in `include/scrap/script.h`.

## Ported

| Module | Content | C file |
|--------|---------|--------|
| `PCMenu` | the video options (`VideoMenu` and its handlers, about 1150 lines of `pcmenu.py`) | `src/game/Script/MenuVideo.c`, `data/menus/video.menu` |

The video options gained two entries in this step. Display (Windowed, Borderless, Fullscreen, engine variable `FullScreen` 0, 2, 1) was added before by a Python snippet (`PyMenuDisplay.c`, removed). Desktop is a new entry at the front of the video modes. It sets the engine variable `VideoDesktop`, and with it `VideoWidth` and `VideoHeight` follow the size of the desktop at the start and at every engine reset. The texts of both are in the extra texts of `src/game/Global/Language.c` (10 languages).

## The .menu format

A layout is a .menu file, built into ScrapC with the C23 `#embed` directive (`data/menus/`). A pack in the `mods/` folder of the game that contains `scrapc/menus/video.menu` replaces the built-in file (the engine mounts every `.packed` in `mods/`, later packs win). `Script_RunMenu` reads `scrapc/<name>` from the packs first, else the built-in copy.

The file is a list of lines. `#` starts a comment, indentation is only for reading.

| Line | Does |
|------|------|
| `style <name>` | the attribute lines that follow belong to the style, until the next command |
| `item <name> <type> [style ...]` | `SScorer.Add`, then the attributes of the styles |
| `<Attr> <value>` | `SScorer.Set` of the last item (or of the style being defined) |
| `use <style>` | the attributes of a style onto the last item, at this point |
| `set <item> <Attr> <value>` | `SScorer.Set` of another item |
| `lr <a> <b>`, `ud <a> <b>` | `Menu.LinkLR`, `Menu.LinkUD` (keyboard navigation) |
| `default <item>` | `SScorer.SetDefault` |
| `call <Module.Func> [arg ...]` | a function of a script, `$id` is the player |
| `run <step>` | a step the C code of the menu does (`MenuVideo.c` has `prepare`, `title`, `values`, `display`) |

A value is a number, a word or a "quoted text". `{Key}` in a text is the language text `Key`, `@Item.Var` is the number of a variable of another item.

The order of the attribute lines matters. `CentralText` and `GetExtens` measure the text, so they come after `Text`, `X` and `Y` (also when they come from a style through `use`).

From `data/menus/video.menu`:

```
style titleend
	Align Right
	GetExtens 1
	Align Left
	Red 143
	Green 180
	Blue 215
	Effect Shadow

item TitleTextureFilter Text font
	Text "{Menu_Video_TextureFilter}:"
	CentralText 1
	X 260
	Y 200
	use titleend
	OnAccept PCMenu.ChangeTextureFilter
```

## Checking a port

The scrap MCP server (`tools/mcp/scrap_mcp.py`) opens the menu with `open_menu PCMenu.VideoMenu` and lists every item with `menu_dump` (name, text, position, size, colour, callback, links). Dump the C version, then dump the same build started with `SCRAP_PACK_SCRIPTS=1`. The two must be equal item by item and in draw order. For the video options the only difference is the intended Display entry. Then click the handlers through with the `click` tool and read the engine variables with the `python` tool. `tools/test/golden.sh` must still match (see [testing.md](testing.md)).

## Porting the next script

- Find the functions of the module and the state only they use.
- Write the C functions in `src/game/Script/` with a `PyMethodDef` table and add the module to `script_modules` in `Script.c`.
- Put the layout in `data/menus/` when it is a menu.
- Compare with `SCRAP_PACK_SCRIPTS=1` as above.

Next in the issue: the other menus, then HUD, characters and missions.
