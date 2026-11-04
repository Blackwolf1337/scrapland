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
| `PCMenu` | the options and controls: `OptionsMenu`, `ControlsMenu`, `MouseControlsMenu`, the key menus of the four tabs (`RacerMoveControlsMenu`, `RacerAttackControlsMenu`, `WalkCharControlsMenu`, `UseCharControlsMenu`, all through `CreateControlMenu`), the pad menu (`ControllerConfig`, `ControllerConfigCharMenu`, `ControllerConfigShipMenu`, `SwitchOption`) and their handlers | `src/game/Script/MenuControls.c`, `data/menus/options.menu` |
| `PCMenu` | the multiplayer menus: `MultiPlayerMenu` (the in-game version and the main menu path), the join page `NetJoinGameMenu` with the server browser (Internet, local network, address), the connecting page, the password page, `NetDefaultContentMenu` (the part the create and join pages share), `NetCreateGameMenu` with the advanced options, and all their handlers | `src/game/Script/MenuNet.c`, `data/menus/net.menu` |

With the multiplayer menus every function of `PCMenu` that a menu or another script calls is C. The helpers still in `pcmenu.py` are only called by the old Python functions.

Two things differ from the script on purpose. The server browser sorted by players with two different keys, depending on when a server came in (a text "n/m" for new servers, -n after a sort). It now always sorts by the number of players. `BrowserMenuBack` called a `MainMenu` that `PCMenu` does not have, it now calls `Menu.MainMenu`.

The engine variable `ServerMapList` keeps the text of a Python list (the script wrote it with backquotes and read it with `eval`). The C code reads and writes that text directly.

The video options gained two entries in this step. Display (Windowed, Borderless, Fullscreen, engine variable `FullScreen` 0, 2, 1) was added before by a Python snippet (`PyMenuDisplay.c`, removed). Desktop is a new entry at the front of the video modes. It sets the engine variable `VideoDesktop`, and with it `VideoWidth` and `VideoHeight` follow the size of the desktop at the start and at every engine reset. The texts of both are in the extra texts of `src/game/Global/Language.c` (10 languages).

## The .menu format

A layout is a .menu file, built into ScrapC with the C23 `#embed` directive (`data/menus/`). A pack in the `mods/` folder of the game that contains `scrapc/menus/video.menu` (or `scrapc/menus/options.menu`, `scrapc/menus/net.menu`) replaces the built-in file (the engine mounts every `.packed` in `mods/`, later packs win). `Script_RunMenu` reads `scrapc/<name>` from the packs first, else the built-in copy.

The file is a list of lines. `#` starts a comment, indentation is only for reading.

| Line | Does |
|------|------|
| `style <name>` | the attribute lines that follow belong to the style, until the next command |
| `item <name> <type> [style ...]` | `SScorer.Add` (not when the item is already there), then the attributes of the styles. The item becomes the current item |
| `<Attr> <value>` | `SScorer.Set` of the last item (or of the style being defined) |
| `use <style>` | the attributes of a style onto the last item, at this point |
| `set <item> <Attr> <value>` | `SScorer.Set` of another item |
| `lr <a> <b>`, `ud <a> <b>` | `Menu.LinkLR`, `Menu.LinkUD` (keyboard navigation) |
| `default <item>` | `SScorer.SetDefault` |
| `list <name>` | the lines that follow are the entries of a list, until the next command |
| `call <Module.Func> [arg ...]` | a function of a script, `Name=value` for a keyword argument |
| `run <step> [arg ...]` | a step the C code of the menu does (`MenuVideo.c` has `prepare`, `values`, `display`) |
| `menu <name>` | a section. The C function of a menu picks one, the lines before the first section always run |
| `do <name>` | the lines of another section, in place |
| `if <cond>`, `elif <cond>`, `else`, `end` | the lines between run only when the condition holds |
| `let <name> <value> [value ...]` | `$name` is the sum of the values (or the one value, when it is no number) |
| `measure <name> <font> <text>` | `$name` is the width of the text in that font (what `SScorer.GetTextArea` gave the script) |

A value is a number, a word or a "quoted text". `{Key}` in a text is the language text `Key`. Unquoted values:

| Value | Is |
|-------|----|
| `@Item.Var` | the number of a variable of another item |
| `%Module.var` | a variable of a script module, for example `%Menu.OptionMenuYStart` |
| `$id` | the player |
| `$section` | the name of the section that runs |
| `$name` | a value the C menu function gives to the file, or that `let` or `measure` made, for example `$tab` and `$first` in the keys section |
| `[name]` | the list `name`, passed as a tuple of lists, one per entry line. An entry line that holds only one value, which is itself a list or tuple, is used as that entry (for example `%PCMenu.MultiplayerMenu`, the entry a game mode sets) |
| `None` | `None` |

`@Item.Var`, `%Module.var` and `$name` may end in +N or -N, for example `@Item1.Y+35` or `$x+140`.

A condition is a value (true when it is not 0 and not empty), or `<value> == <value>`, or `<value> != <value>`.

The C function of a menu picks the section and can hand values to the file as `$names`. A `call` can also reach a C function of a script, as the four key sections of `options.menu` do with `call PCMenu.CreateControlMenu`. The key lists are in the file, so a mod pack can change which actions appear.

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

The title of the video menu is a `list` and a `call` in `data/menus/video.menu`:

```
list title
	""		""
	"{Menu_Back}"	PCMenu.VideoMenuBack
call Menu.VerticalMenu $id "{Menu_Options_Video}" [title] PCMenu.VideoMenuBack XStart=250 YStart=%Menu.OptionMenuYStart
```

From `data/menus/options.menu` (the controls menu, shortened):

```
menu Controls
call Menu.StartNewMenu $id
list entries
	"{Menu_Controls_Mouse}"		PCMenu.MouseControlsMenu
	"{Menu_Back}"			Menu.OptionsMenu
call Menu.VerticalMenu $id "{Menu_Options_Controls}" [entries] Menu.OptionsMenu YStart=%Menu.OptionMenuYStart
```

From `data/menus/net.menu` (the server name on the create page, shortened):

```
measure w Horatio "{Menu_MultiPlayer_ServerName}:"
let x 74 $w 10
item ServerName Text
	Font Horatio
	Text $servername
```

## Checking a port

The scrap MCP server (`tools/mcp/scrap_mcp.py`) opens the menu with `open_menu PCMenu.VideoMenu` and lists every item with `menu_dump` (name, text, position, size, colour, callback, links). Dump the C version, then dump the same build started with `SCRAP_PACK_SCRIPTS=1`. The two must be equal item by item and in draw order. For the video options the only difference is the intended Display entry. Then click the handlers through with the `click` tool and read the engine variables with the `python` tool. Step 2 also compared a click sequence: the same list of opens, clicks, key presses and python calls on both versions, with `menu_dump` and the engine variables after each step, compared as text. Two differences are expected and normalized first: the accept and called names of C functions show without the `PCMenu.` prefix, and the alpha of animated items (the cursor) differs. `tools/test/golden.sh` must still match (see [testing.md](testing.md)).

Step 3 used the same method. Two lessons from it. When a step starts a server, run the C game and the pack game one after the other, because two games on one machine compete for the server port. Saved state matters: the slot's `Scrap.cfg` keeps engine variables and key and pad bindings between runs, so compare runs that start from the same config.

## Porting the next script

- Find the functions of the module and the state only they use. The original Python sources are in the game packs as `.py` next to the `.pyc` (`scripts/scorer/pcmenu.py`, `menu.py` and so on), the port follows them.
- Write the C functions in `src/game/Script/` with a `PyMethodDef` table and add the module to `script_modules` in `Script.c`.
- Use the shared C helpers instead of going through Python: `GVar_GetNum`, `GVar_SetNum`, `GVar_SetStr` (engine variables, `include/scrap/scrap.h`), `Input_SelectActionSetNamed` (`include/scrap/input.h`), and in `include/scrap/script.h` `Script_SetCursor`, `Script_GetString` and `Script_SetInt` besides the older ones.
- Put the layout in `data/menus/` when it is a menu.
- Compare with `SCRAP_PACK_SCRIPTS=1` as above.

Next in the issue: the other menus, then HUD, characters and missions.
