# Scripts

The original game runs its menus, HUD, characters, missions and levels from Python 1.5.2 scripts in the game packs (for example `scripts/scorer/pcmenu.py`, about 4500 lines). ScrapC links a real Python 1.5.2 and runs them. Issue #2 on the project's Gitea is to move that script layer to C step by step while the game keeps working. Python goes once nothing is loaded from the packs any more.

## The loader

When the engine loads a module from the script pack (`Pack_PreloadLibrary` in `src/game/PyInterface/PythonInit.c`), `Script_ModuleLoaded` (`src/game/Script/Script.c`) puts the C functions ScrapC has for that module into it under their script names. The table `script_modules` lists them, each entry is a module name and a `PyMethodDef` table. So the code of the pack, other modules and the callbacks named in strings (for example the `OnAccept` value `PCMenu.ChangeVideoMode`) reach the C versions. Names that a module bound to a ported Python function when it loaded are replaced too (for example `onCreateMainMenu = CreateMainMenu` in `menu.py` points to the C `CreateMainMenu` after loading). Everything not ported still runs in Python as before.

`SCRAP_PACK_SCRIPTS=1` (environment variable) turns the C versions off, so the original pack scripts run. It is used to compare a port with the original.

The helpers the ports share are declared in `include/scrap/script.h`.

## Ported

| Module | Content | C file |
|--------|---------|--------|
| `PCMenu` | the video options (`VideoMenu` and its handlers, about 1150 lines of `pcmenu.py`) | `src/game/Script/MenuVideo.c`, `data/menus/video.def` |
| `PCMenu` | the options and controls: `OptionsMenu`, `ControlsMenu`, `MouseControlsMenu`, the key menus of the four tabs (`RacerMoveControlsMenu`, `RacerAttackControlsMenu`, `WalkCharControlsMenu`, `UseCharControlsMenu`, all through `CreateControlMenu`), the pad menu (`ControllerConfig`, `ControllerConfigCharMenu`, `ControllerConfigShipMenu`, `SwitchOption`) and their handlers | `src/game/Script/MenuControls.c`, `data/menus/options.def` |
| `PCMenu` | the multiplayer menus: `MultiPlayerMenu` (the in-game version and the main menu path), the join page `NetJoinGameMenu` with the server browser (Internet, local network, address), the connecting page, the password page, `NetDefaultContentMenu` (the part the create and join pages share), `NetCreateGameMenu` with the advanced options, and all their handlers | `src/game/Script/MenuNet.c`, `data/menus/net.def` |
| `Menu` | the building blocks of all menus: `LinkLR`, `LinkUD`, `UnLinkLR`, `UnLinkUD`, the logos (`DrawBackMainMenu`, `DrawBackSubMenu`, `DrawBackOptionMenu`), `DrawCircuitMenu`, `DrawMenuTitleBar`, `StartNewMenu`, `AddTabs`, `VerticalMenu`, `YesNoMenu`, `DisableMenuItem`, `ShowTextBox`, `ShowWarning`, `SetTabDefaults`, `SliderMenu`, `PreviousNextMenu`, `PreviousNextMenuSimple`, `DummyFunc` (step 4a). `VerticalMenu` and `AddTabs` take keyword arguments. The C functions compute the places with the integer and float division of the script (Python 1.5.2 divides integers as integers) and hand them to the file as `$names` | `src/game/Script/MenuBase.c`, `data/menus/base.def` |
| `Menu` | the main menu (through `Menu.onCreateMainMenu`, the hook a mod may replace), the single player menu in all its variants, the audio options, the skill menus, new game, abort, the load and save menus with the names of saved games, the death screen, `Init`, `InitMenuSys`, `BackToGame`, `DrawScorer`, `MovieScorer` (step 4b) | `src/game/Script/MenuMain.c`, `data/menus/main.def` |
| `Menu` | the rest of `menu.py` (step 4c): the mission info pages (`MenuMissionInfo`, the arrows `MenuMissionInfo_Left` and `MenuMissionInfo_Right`, `GoPrimaryMission`, `GoCrazyDeal`, `GoNotes`, `EnterMissionInfo`, `ExitMissionInfo`, `MenuObjetive`, `CrazyDealObjetive`, `MissionNotesText`) in `MenuMission.c` and `data/menus/mission.def`. In `MenuMain.c` and `main.def`: `Initialize` (the textures and talking heads a level preloads, written as `call SScorer.PreloadTexture` and `call SScorer.AddModel` lines with conditions), `CreditsMenu`, `GameOver`, the television that switches off when the player dies (`TVOff`, `SetCam`, `TVOffEnd`, `BlackTV`) and the debug menu of the developers (`DebugMenu` and the `DebugSet_` functions) | `src/game/Script/MenuMission.c`, `data/menus/mission.def`, `src/game/Script/MenuMain.c`, `data/menus/main.def` |
| `Scorer` | the HUD (step 5): status, police meter, monitor, messages, label, talk, mission scorer, radar, `Blink`, `ShowMissionScorer`, `MissionScorerBar_SetDefaults` | `src/game/Script/MenuHud.c`, `data/menus/hud.def` |
| `CharScorer` | the HUD on foot, with the special action of the character the player controls | `src/game/Script/MenuHud.c`, `data/menus/hud.def` |
| `RacerScorer` | the HUD in a ship: weapons, cannon, target, waypoint, radar. `CreateRacer` (also for a spectator), `ShowScorer` (the scores of a game, with the super deal variants, and the ping refresh `MultiplayerUpdatePing`) and the scorer of race missions | `src/game/Script/MenuHud.c`, `data/menus/racer.def` |
| `Chars` | step 6a, the port of `scripts/chars/chars.py`: the sound events of the animations (`ActSound`, `ActSoundLoop`, `StrSound`, `StrSoundStop`; the character scripts call them about 3400 times while a level loads) go straight to the action classes of the engine, with the same variable sets the script made through `SAct`. Also the resources of the characters (`Chars.CharRes` stays the Python list other scripts fill), `CanDrive`, `SetOnFloor`, `IntSetChar`, `MainCharDeath` with its hook `OnMainCharDeath`, `Init`, `CreateMainChar` and `CreateStartupPlayer`. This module has no .def file, it holds no layout | `src/game/Script/Chars.c` |
| `MakeChar` | step 6b, the port of `scripts/chars/makechar.py`: the helpers the character scripts build the action classes with. `InitChar` with the class defaults, the relax, walk, fall, talk, morph, discover, listen, sit and turret actions, the break loop for flashed, crazy and dazed, `EventAnimCommand` and `SetNodeRotation`. They work on the current class and action of `SAct` with the same variable sets and event times the script computed. `MaxSpeed`, `MinSpeed`, `RelaxRotLimit`, `DazedChars` and the morph lists stay variables of the module, because the character scripts change them. Helpers with defaults take keyword arguments. This module has no .def file, the event times come from animation lengths in loops, which the format does not express | `src/game/Script/MakeChar.c` |
| `Betty` | step 6c: `Init` from the file. Step 6d: `OnCreate` and `OnFreeResources` are sections of the file too. `EnterSlowSpeed` and `ExitSlowSpeed` stay C | `src/game/Script/Characters.c`, `data/chars/betty.def` |
| `BankDirector`, `Boss`, `CrazyGambler`, `Humphrey`, `Maintenance`, `Mayor`, `Rusty`, `Sebastian`, `Sentinel`, `SputnikJunkyard`, `Bishop`, `Functionary`, `Gear`, `BankMaster`, `Berto`, `Nurse`, `Desktop`, `Police`, `GateKeeper`, `Dtritus`, `Sputnik`, `PoliceBoss`, `Messenger`, `Human` | step 6c: the `Init` of each module is a data file `data/chars/<name>.def` that a small C `Init` runs (the macro `CHAR_INIT` in `Characters.c`). Step 6d: their `OnCreate` and `OnFreeResources` callbacks are sections of the same file (see below). Their other functions stay in the scripts for now, except those of step 6e below | `src/game/Script/Characters.c`, `data/chars/` |
| `MercenaryA`, `MercenaryB`, `MercenaryC`, `MercenaryL` | step 6c: one file `data/chars/mercenarya.def` for all four. The script's `MercenaryA.Init(MercName="MercenaryA", doDance=1)` becomes the values `$merc` and `$dance` (`MercenaryL` passes 0). Its `OnCreate` section serves all four mercenaries. The rest stays in the scripts like above | `src/game/Script/Characters.c`, `data/chars/mercenarya.def` |
| `Functionary`, `Rusty`, `Boss`, `Mayor`, `Maintenance`, `SputnikJunkyard`, `CrazyGambler` | step 6e: the gameplay functions are C, because they are logic on entity variables and module state, not data. Functionary: `EnterFastEffect`, `ExitFastEffect`, `ActionPress`, `ActionRelease` (the slow motion of his special action while the energy bar empties). Rusty: `ActionPress`, `ActionRelease` (they do nothing). Boss: `ActionPress`, `BossSpeech`. Mayor: `MayorActionPress`, `MayorReCheckAction`. Maintenance: `MaintenanceTalk`, `MaintenanceWorkFX`. SputnikJunkyard: `SputnikJunkyardTalk`. CrazyGambler: `CalculateStatus`, `CrazyGamblerSpeech`, `StartSuperDeal`. Each module gets a second method table `g_<Module>Funcs` next to its `Init` table from `Characters.c` (the registry allows several tables per module). Module variables other scripts may read or change (`Functionary.PressOk`, `speedFactor`, `charSpeed`, `worldSpeed`, `CrazyGambler.CDcompl`) stay variables of the module | `src/game/Script/CharFuncs.c` |

`menu.py` is done, every function of it has a C version. The HUD is done too (`Scorer`, `CharScorer`, `RacerScorer`). The characters are in progress: `Chars`, `MakeChar` and the `Init` of 29 character modules are done, and so are the `OnCreate` callbacks (all but `GateKeeper`'s; the one of `MercenaryA` serves the four mercenaries) and 15 `OnFreeResources`. Step 6e added the gameplay functions of seven character modules (`Functionary`, `Rusty`, `Boss`, `Mayor`, `Maintenance`, `SputnikJunkyard`, `CrazyGambler`). The other character functions are not done. `Bureaucracy` (it builds the desktop classes from tables in loops) and `TestC` (a test character no level loads) stay Python. `chat.py` and `creditstext.py` in the same folder are still Python.

The code of the scripts that sets the margins of the screen (`Scorer.MarginH` and `Scorer.MarginV`) stays in the scripts. The C reads them and gives them to the HUD files as `$mh`, `$mv`, `$right` and `$bottom`. The game modes keep setting `RacerScorer.ScorerTopMessage`, `ScorerBotMessage` and the mission variables.

With the multiplayer menus every function of `PCMenu` that a menu or another script calls is C. The helpers still in `pcmenu.py` are only called by the old Python functions.

Two things differ from the script on purpose. The server browser sorted by players with two different keys, depending on when a server came in (a text "n/m" for new servers, -n after a sort). It now always sorts by the number of players. `BrowserMenuBack` called a `MainMenu` that `PCMenu` does not have, it now calls `Menu.MainMenu`.

The engine variable `ServerMapList` keeps the text of a Python list (the script wrote it with backquotes and read it with `eval`). The C code reads and writes that text directly.

The video options gained two entries in this step. Display (Windowed, Borderless, Fullscreen, engine variable `FullScreen` 0, 2, 1) was added before by a Python snippet (`PyMenuDisplay.c`, removed). Desktop is a new entry at the front of the video modes. It sets the engine variable `VideoDesktop`, and with it `VideoWidth` and `VideoHeight` follow the size of the desktop at the start and at every engine reset. The texts of both are in the extra texts of `src/game/Global/Language.c` (10 languages).

## The .def format

The data of a port (a menu layout, the HUD, a character) is a .def file, built into ScrapC with the C23 `#embed` directive (`data/menus/`, `data/chars/`). A pack in the `mods/` folder of the game that contains `scrapc/menus/video.def` (or another file of `data/menus/`, or `scrapc/chars/<name>.def`) replaces the built-in file (the engine mounts every `.packed` in `mods/`, later packs win). `Script_RunFile` reads `scrapc/<name>` from the packs first, else the built-in copy.

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
| `run <step> [arg ...]` | a step the C code of the menu does (`MenuVideo.c` has `prepare`, `values`, `display`). The steps of the character files are listed below |
| `section <name>` | a section. The C function that runs the file picks one, the lines before the first section always run |
| `do <name>` | the lines of another section, in place |
| `if <cond>`, `elif <cond>`, `else`, `end` | the lines between run only when the condition holds |
| `let <name> <value> [value ...]` | `$name` is the sum of the values (or the one value, when it is no number) |
| `measure <name> <font> <text>` | `$name` is the width of the text in that font (what `SScorer.GetTextArea` gave the script) |

The character files (step 6c) use more lines:

| Line | Does |
|------|------|
| `action <name>`, `getaction <name>` | `SAct.CreateAction`, `SAct.GetAction`. The attribute lines that follow set the action, like `SAct.SetAct` |
| `action` (no name) | the current action, the last one a `MakeChar` helper made |
| `class <name>` | `SAct.GetClass`. The attribute lines that follow set the class, like `SAct.SetCls` |
| `event <time> <name> <func> [discardable]` | an event of the current action |
| `setvar <Module.var> <value>` | a variable of a script module |
| `entity <name>` | the attribute lines that follow set script variables of the entity, like `entity.var = value` in a script (step 6d) |

A value is a number, a word or a "quoted text". `{Key}` in a text is the language text `Key`. Unquoted values:

| Value | Is |
|-------|----|
| `@Item.Var` | the number of a variable of another item |
| `%Module.var` | a variable of a script module, for example `%Menu.OptionMenuYStart` |
| `$id` | the player |
| `$section` | the name of the section that runs |
| `$name` | a value the C function gives to the file, or that `let` or `measure` made, for example `$tab` and `$first` in the keys section |
| `[name]` | the list `name`, passed as a tuple of lists, one per entry line. An entry line that holds only one value, which is itself a list or tuple, is used as that entry (for example `%PCMenu.MultiplayerMenu`, the entry a game mode sets) |
| `None` | `None` |
| `&Var` | a variable of the current action of `SAct`, for example `&TimeOfFrame43` or `&AnmTotalTime` |
| `(a,b,)` | a tuple of values |

`@Item.Var`, `%Module.var`, `$name` and `&Var` may end in +N or -N, for example `@Item1.Y+35` or `$x+140`. For these values a chain of operations on numbers is done left to right with the number rules of Python (`+`, `-`, `*`, `/`), for example `&AnmTotalTime/4*2`.

A quoted argument of `call` and `run` stays a text: `call Gear.DropMissileEvent "22" "35"` passes two strings.

A condition is a value (true when it is not 0 and not empty), or `<value> == <value>`, `<value> != <value>`, or two numbers compared with `<`, `>`, `<=` or `>=`.

`$(name)` inside a word or a text is the value `name` as text, for example `item $(name)Circuit1 Sprite circuit` in the slider section of `base.def`.

Up to four menus may run inside each other (a .def calls a function that runs another .def). Each has its own buffer for an override from a mod pack.

The character files work the same way. A pack in `mods/` with `scrapc/chars/<name>.def` replaces the built-in `data/chars/<name>.def`. The C gives every character file `$xbox` and `$level` (the level path in capitals).

The C function that runs a file picks the section and can hand values to the file as `$names`. A `call` can also reach a C function of a script, as the four key sections of `options.def` do with `call PCMenu.CreateControlMenu`. The key lists are in the file, so a mod pack can change which actions appear.

The callbacks of the characters (step 6d) are sections of the character file. `section OnCreate` and `section OnFreeResources` replace the script functions `OnCreate(EntityName)` and `OnFreeResources()`. Small C module functions run them (the macros `CHAR_ONCREATE`, `CHAR_ONFREE` and `CHAR_MODULE0`, `CHAR_MODULE1`, `CHAR_MODULE2` in `Characters.c`), and `$name` is the entity. 25 files have an `OnCreate` section (all ported characters except `GateKeeper`, whose `OnCreate` makes a Python object per gatekeeper and stays in the script). 15 files also have `OnFreeResources`: Betty, BankDirector, BankMaster, Berto, Bishop, Desktop, Dtritus, Functionary, Gear, Messenger, Nurse, PoliceBoss, Police, Rusty and Sebastian. The `OnCreate` of `MercenaryA` serves all four mercenaries. The `OnCreate` and `OnFreeResources` of Betty are in `betty.def` too, her `EnterSlowSpeed` and `ExitSlowSpeed` stay C.

The `entity` line uses `PyEntity_SetVar` (`PyEntity.c`), the code the attribute set of the Python entity handle shares. The value is tried as an int, a float, a vector and a string, in this order. A word with `{Key}` gives a language text. From `data/chars/maintenance.def`:

```
menu OnCreate
call CharAct.OnCreate $name
entity $name
	Action Loop
	OnDeath ""
	ArrowCanUse 0
	Descriptor {CharName_Maintenance}
call MaintenanceSound.InitLoops $name
```

The steps of the character files (`Characters.c`):

| Step | Does |
|------|------|
| `run speech <Name> <Module> <Func>` | sets `Speech.SpeechRoutines[Name]` |
| `run spread <first> <end> <count> <name> <func>` | adds `count` events at `first + (end - first) * i / count`, the loop the scripts write (the hit events of Dtritus; the 16 hit events of Betty use the same code with numbered names) |
| `run timedname <name> <func>` | adds an event at time 0 named `name` plus the repr of the `AnmTotalTime` of the action (Sputnik) |
| `run bettyhit` | the camera turns and hits of Betty |

The order of the attribute lines matters. `CentralText` and `GetExtens` measure the text, so they come after `Text`, `X` and `Y` (also when they come from a style through `use`).

From `data/menus/video.def`:

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

The title of the video menu is a `list` and a `call` in `data/menus/video.def`:

```
list title
	""		""
	"{Menu_Back}"	PCMenu.VideoMenuBack
call Menu.VerticalMenu $id "{Menu_Options_Video}" [title] PCMenu.VideoMenuBack XStart=250 YStart=%Menu.OptionMenuYStart
```

From `data/menus/options.def` (the controls menu, shortened):

```
menu Controls
call Menu.StartNewMenu $id
list entries
	"{Menu_Controls_Mouse}"		PCMenu.MouseControlsMenu
	"{Menu_Back}"			Menu.OptionsMenu
call Menu.VerticalMenu $id "{Menu_Options_Controls}" [entries] Menu.OptionsMenu YStart=%Menu.OptionMenuYStart
```

From `data/menus/net.def` (the server name on the create page, shortened):

```
measure w Horatio "{Menu_MultiPlayer_ServerName}:"
let x 74 $w 10
item ServerName Text
	Font Horatio
	Text $servername
```

Where the script failed with a Python exception that changes what happens next, the C function raises the same exception. For example the volume dummies of the audio menu without the sliders raise `TypeError` before moving the focus.

The long attribute lists of the HUD were converted from the scripts with a small converter. It turns literal `SScorer.Add` and `SScorer.Set` lines into .def lines and marks the rest for hand editing.

## Checking a port

The scrap MCP server (`tools/mcp/scrap_mcp.py`) opens the menu with `open_menu PCMenu.VideoMenu` and lists every item with `menu_dump` (name, text, position, size, colour, callback, links). Dump the C version, then dump the same build started with `SCRAP_PACK_SCRIPTS=1`. The two must be equal item by item and in draw order. For the video options the only difference is the intended Display entry. Then click the handlers through with the `click` tool and read the engine variables with the `python` tool. Step 2 also compared a click sequence: the same list of opens, clicks, key presses and python calls on both versions, with `menu_dump` and the engine variables after each step, compared as text. Two differences are expected and normalized first: the accept and called names of C functions show without the `PCMenu.` prefix, and the alpha of animated items (the cursor) differs. `tools/test/golden.sh` must still match (see [testing.md](testing.md)).

Step 3 used the same method. Two lessons from it. When a step starts a server, run the C game and the pack game one after the other, because two games on one machine compete for the server port. Saved state matters: the slot's `Scrap.cfg` keeps engine variables and key and pad bindings between runs, so compare runs that start from the same config.

`Initialize` was checked by replacing `SScorer.PreloadTexture` and `SScorer.AddModel` in the game with functions that record their arguments (the script and the C `call` lines both look them up in the `SScorer` module), calling `Menu.Initialize()` once, and comparing the recorded lists of both versions: 49 calls in a level, 20 in the main menu.

Step 4 added one more lesson. `Menu.Init` and `InitMenuSys(0)` stop the game time in the main menu, and the MCP commands run on scheduled functions of the game time. A test must set the time speed back in the same python call (`Menu.Init(0); Scrap.SetTimeSpeed(1)`). A sequence that clicks New Game starts a game and stops the comparison.

The menu dump shows only places, texts and colours, so the HUD ports were also checked by reading back every attribute the .def files set with `SScorer.Get` in both versions and comparing the lists. Some attributes cannot be read back. Getters that check their input value (`BlendingMode`, `HighRes`, `SpriteIndex`, `TabBlendingMode`, `ScaleY`, `ScaleSmoothWidth`, `ScaleSmoothHeight`) end the game with a failed check, and the indexed icon pairs (`WeaponIconAc` and `WeaponIconIndex` and the like) read out of bounds when the index was never set (the original crashed on that). Attributes a HUD never sets read back as uninitialised memory in both versions and differ from run to run.

Gameplay scripts need a way to see their effect on the engine, a menu dump does not show it. For the characters that is the event table: `SAct.DumpEvents(file)` (ScrapC only, like `SScorer.Items`) writes every action class with its actions and their events (time, name, function, discardable) to the file, and since step 6b also the readable variables of every class and action. After a level load the files of the script and the C version were equal: 2794 lines in Bank, 571 in FZ. The whole table was also compared after a direct call of every `MakeChar` helper (683 lines).

A lesson for direct calls of action helpers: creating a class or an action that already exists, or an action whose animation file is missing, ends the game with a failed check in both versions. A direct test needs new action names, existing animations, and `InitChar` of a character that the level does not load.

For step 6d the variables of every character entity (`ActCtrl`, `Action`, `OnDeath`, `Descriptor`, `Invulnerable`, `TgType`, `Energy`, `EnergyBarDecr`, `ArrowCanUse`, `UsrControl`, `EntityList`) were dumped after the start of 10 levels (Bank, GamblinDen, Police, Townhall, Press, Temple, Junkyard, FZ, ABIndoor, Orbit) and compared with the pack scripts. They were equal, except the `Action` of the NPCs in GamblinDen. It differs between two runs of the pack scripts as well, because they pick dance, talk and relax animations at random. Every C `OnFreeResources` was also called directly without an error.

For step 6c the action tables (`SAct.DumpEvents`) after the start of all 29 levels were equal between the pack scripts and the C version, compared level by level. GDB, Bank and Police were compared again after the two fixes below.

A quoted value must stay a text. The first version passed "22" to `Gear.DropMissileEvent` as a number, its string concatenation failed and two Gear events were missing.

For step 6e a harness called the functions of the seven modules directly in a level (GamblinDen, Press, Junkyard) in both versions. After each call it read back the entity variables, the engine variables, the time speed, the Functionary class `FOVInterpTime`, the module variables, and the state of `Speech` and `CharsNPC`. The results were equal. Three differences are left: the type of the builtin function object, the current animation of the Mayor (the NPCs in GamblinDen pick talk loops at random, as before), and the next `Scrap.Rand` value, which cannot be compared because the random sequence of the game differs between runs. To reach the pressed branch of `Functionary.ActionPress` the test sets `UsrControl` to 0 and lowers `FunctionarySpecialActionLow`, because setting `Energy` did not stick in either version.

Errors must stop where the script stops. In GDB the character Bishop of the save game is not preloaded, so `Chars.Init` raises an ImportError in the script and the rest of `Init.Init` does not run, and no player is created. The first C version printed the error and went on, and the game crashed in `Alarm_Update` on a player without an action controller. `Chars.Init` and `Chars.CreateMainChar` now stop at the first error like the script.

## Porting the next script

- Find the functions of the module and the state only they use. The original Python sources are in the game packs as `.py` next to the `.pyc` (`scripts/scorer/pcmenu.py`, `menu.py` and so on), the port follows them.
- Write the C functions in `src/game/Script/` with a `PyMethodDef` table and add the module to `script_modules` in `Script.c`.
- Use the shared C helpers instead of going through Python: `GVar_GetNum`, `GVar_SetNum`, `GVar_SetStr` (engine variables, `include/scrap/scrap.h`), `Input_SelectActionSetNamed` (`include/scrap/input.h`), and in `include/scrap/script.h` `Script_SetCursor`, `Script_GetString` and `Script_SetInt` besides the older ones. `Script_SetValue` gives a value from a step of a .def back to the file that runs it, as `$name`. `Script_Eval` returns the value of a Python expression, used for save variables that keep Python lists. `Script_Invoke(callable, args)` calls a Python function and releases the argument tuple, because `PyEval_CallObject` does not take it over (earlier ports leaked temporary tuples in ten places).
- Ports of gameplay functions (step 6e) use more helpers in `include/scrap/script.h`. `Script_Attr` gives an attribute of a module (a new reference), `Script_GetFloat` a number variable of a module. `Script_EntNum`, `Script_EntStr` and `Script_EntSet` read and write the script variables of an entity by name, exactly as the Python entity handle does: when reading, an int is tried before a float, and `Script_EntSet` uses the int, float, vector, string order of `PyEntity_SetVar`, so a float value given to a variable with an int setter is truncated like in the script. `Scrap_RandRange` (`include/scrap/scrap.h`) is the body of `Scrap.Rand`, shared so a C port draws from the random sequence the same way.
- Entities have a C interface for their script variables (`cEntity_VarI`, `cEntity_VarF`, `cEntity_VarS`, `cEntity_VarV` in `include/scrap/entity.h`, which the Python entity object also uses). Gameplay ports should use it instead of the Python entity objects. `Chars.c` still uses the Python entity objects for the player creation.
- Put the layout in `data/menus/` when it is a menu.
- Compare with `SCRAP_PACK_SCRIPTS=1` as above.

Next are Bishop, Gear, Police and Messenger (sirens, selling lives, the data pack), then Sputnik (the ship interface) and `GateKeeper`, then `CharAct`, `CharsNPC`, `Speech`, `InMap`, `Bureaucracy` and the missions. `chat.py` and `creditstext.py` are still Python.
