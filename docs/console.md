# Console and debug menu

ScrapC has a console for commands and engine variables. Builds with the debug menu option also have a menu to start missions, cutscenes and videos. The code is in `src/game/Scorer/ConsoleView.c` and `src/game/Global/ConsoleCmd.c` (console) and `src/game/Script/DebugMenu.c` (debug menu).

## Console

The key next to 1 (backquote or grave, `^` on a German keyboard) opens and closes the console. Escape closes it too. The console slides over the top half of the screen and the game HUD stays visible below. While it is open the game gets no keyboard input.

### Syntax

| Input | Effect |
|-------|--------|
| `name` | shows an engine variable |
| `name value` | sets an engine variable |
| `command a b c` | runs a command |

Command arguments are separated by blanks or commas. `name=value` works too. The old forms still work: `:name`, `:name value`, `?prefix` (lists variables) and `/command a,b,c`.

### Editing

| Key | Effect |
|-----|--------|
| Left, Right | move the cursor |
| Home, End | start and end of the line |
| Ctrl+Left, Ctrl+Right | move by words |
| Backspace, Delete | delete a letter (with Ctrl: a word) |
| Ctrl+U | clear to the start of the line |
| Ctrl+K | clear to the end of the line |
| Ctrl+W | delete a word |
| Ctrl+V | paste |
| Ctrl+C | copy the input line |

### History and scrollback

Up and Down (also Ctrl+P and Ctrl+N) go through the history. It holds 64 lines and is kept in `ConsoleHistory.txt` next to `Scrap.cfg`.

The scrollback holds 2048 lines. PgUp and PgDn scroll (faster with Ctrl or Shift), and so does the mouse wheel. Ctrl+Home and Ctrl+End jump to the start and the end. Ctrl+L clears it.

### Completion

Tab completes commands and engine variables to the common start of all matches and lists the matches. Commands are listed with their description, variables with their value and description. Tab after a complete variable name shows its value, default and description.

Tab also completes arguments:

| Command | Completes |
|---------|-----------|
| `map`, `missions`, `split` | level names |
| `mission` | mission names, then the level as second argument |
| `cvarlist` | variable names |
| `scene` | cutscene names |
| `act`, `modelinfo` | entity names |

### Commands

| Command | Effect |
|---------|--------|
| `help` | short help |
| `cmdlist [filter]` | lists the commands |
| `cvarlist [filter]` | lists the engine variables |
| `clear` | clears the scrollback |
| `echo text` | prints the text |
| `quit` | quits the game |
| `map Level` | loads a level as a new game |
| `mission Mission [Level]` | starts a mission |
| `missions [filter or level]` | lists the missions |
| `split Level` | starts a network level in split screen |
| `scene Name` | plays a cutscene (debug builds) |
| `debugmenu` | opens the debug menu (debug builds) |

The older commands stay: `speed`, `stop`, `go`, `cshow`, `listmodels`, `modelinfo`, `screenshot`, `reset3d`, `hidemap`, `showmap`, `camrot`, `mypos`, `setcam`, `getcam`, `CamEnt`, `act`, `dms`, `dmm`, `kick` and `join`.

### map and mission

`map` and `mission` use the mission maker of the developers (`src/game/Script/MissionMaker.c`, a port of `missionmaker.py` from the game packs). They clear the save variables of the running session, set those of a new game and load the level. They do not write the auto save (slot 0). The GDB level starts with the bishop as player character.

## Debug menu

The debug menu is only in builds with the CMake option `SCRAP_DEBUG_MENU`. It is on by default when `CMAKE_BUILD_TYPE` is Debug. Turn it on in an existing build folder with:

```
cmake -B build-linux64 -DSCRAP_DEBUG_MENU=ON
```

F11 or the console command `debugmenu` opens it. F11 again goes back to the console text.

### Pages

- Missions by level.
- Cutscenes (35, each listed with its actors and level).
- Videos: Intro, MSE, MSE-Short, Bishop, Mayor, Police, Viscous.
- Arenas and super deals.
- Levels (no mission).
- Network levels in split screen.

### Keys

| Key | Effect |
|-----|--------|
| Up, Down | choose an entry (also PgUp, PgDn, Home, End and the mouse wheel) |
| Enter, Right | start or open the entry |
| Backspace, Left | go back (Backspace first removes a letter of the filter) |
| letters | filter the page |
| Escape | close the menu |

### Cutscenes

A cutscene entry starts the mission it belongs to. When the mission does not play that scene at its start, the scene is loaded and played once the level has run for a second and no other scene is playing. The console command `scene Name` does the same.
