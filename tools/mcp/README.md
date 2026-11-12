# scrap-mcp

An MCP server (version 0.5.0) that runs the game of this checkout, or the frozen reference build `build-ref-noexact/ScrapC.exe`, without a window, so an AI coding assistant can test the Scrapland C recreation. The game runs in gamescope (headless backend), muted, on the AMD 610M by default (`SCRAP_VK_DEVICE=1002:164e`). The default is `native64`, the Linux build in `build-linux64`. The Windows build and the reference build run under Wine.

The server talks MCP over stdio and uses only the Python standard library. Pillow is optional and does crop, zoom and compare.

## Requirements

- Linux
- Python 3.8 or newer
- gamescope with `gamescopectl`
- a built `build-linux64/ScrapC` (the default exe `native64`, a build without the Python interpreter)
- Wine with the project prefix (`tools/test/prefix.sh`), only for `c64` and `ref`
- a built `build-win64/ScrapC.exe` with its DLLs, only for `c64`
- libX11 and libXtst, for `key` and `mouse`
- the game data in `game/` of the checkout
- the Vulkan validation layer, only for `validation=true`
- Pillow for `crop`, `zoom` and `compare`. Without it screenshots are sent at full size.

## Setup

```
claude mcp add --scope user scrap -- python3 /home/xleuiv/projects/scrapland/tools/mcp/scrap_mcp.py
```

The server works on the checkout it lives in. Set `SCRAP_MCP_ROOT` to point it at another checkout. In a git worktree without `game/` the game data and the slots are those of the main checkout, the builds those of the worktree. `SCRAP_MCP_SLOTS` puts the slots in another folder (private slots). `SCRAP_MCP_DESKTOP_DISPLAY` names a desktop display that input is never sent to, besides the `DISPLAY` of the server itself.

## How it works

Every named game gets a slot under `build/mcp/slot-<name>`. `tools/test/mkgame.sh --into` makes it: the game data is linked, `Bin`, the configs and the Wine prefix are copied. `launch` copies the exe (and the DLLs for the Windows builds) into the slot and writes `scrapdecomp.ini` with `testmod=1`, so the SDTest module exists. Then `tools/test/native.sh` (`native64`, `native64asan`) or `tools/test/wine.sh` (`c64`, `ref`) starts the game.

The tools talk to the game through the test channel (`src/game/Global/TestChannel.c`, switched on with `SCRAP_TEST_CHANNEL=1`) for `native64`, `native64asan` and `c64`. A C build without the test channel is refused. No Python of the tool runs in the game. Once a frame the game reads `test_cmd.txt` in the game folder and answers in `test_reply.txt`, also while the world clock stands (a menu that stopped the game time, `/stop`), but not during a level load or a video. `tools/test/channel.py` is the tool side. `menu_dump`, `click`, `open_menu`, `wait` and `cmd` use channel commands.

`ref` (the frozen i686 reference exe) has no test channel: its harness `mcpgame.py` went with the interpreter (tag `interp-final`). For `ref` only `screenshot`, `compare`, `log`, `key`, `mouse`, `wait` with `kind=log` or `kind=time` and `quit` work. `cmd`, `menu_dump`, `open_menu`, `click` and `wait` with `kind=ready` refuse with a message.

`wait kind=ready` returns once the menu or a level runs.

## Tools

All tools take an optional `name` (default `main`), so several games can run at the same time.

| Tool | What it does | Main arguments |
|------|--------------|----------------|
| `launch` | Starts a game in its slot and returns at once. | `exe` (`native64` default, `native64asan`, `c64` or `ref`), `map`, `args`, `env`, `validation`, `trace`, `size` (default 1280x720), `timeout` (default 3600 s) |
| `quit` | Ends a game. | `name=all` ends every game |
| `status` | Lists the games and whether they run. | |
| `wait` | Waits until the game answers, a log line matches, or for some seconds. | `kind` (`ready`, `log`, `time`), `pattern`, `which`, `seconds`, `timeout` (default 120), `screenshot` |
| `cmd` | Test channel commands, one a line (see below). | `lines`, `timeout` (default 10), `screenshot` |
| `screenshot` | The window of the game. | `crop` (x, y, w, h), `zoom` |
| `compare` | Screenshots of several games side by side, for example `c` and `ref` on the same level. | `names`, `crop` |
| `log` | The end of `log.html` (`which=game`) or `scrapdecomp.log` (`which=engine`). | `which`, `lines` (default 40), `pattern` |
| `trace` | The traced frame of a game launched with `trace=`: a count of the draws by target and pixel shader, then the first 80 lines. | `full` |
| `vkcheck` | The Vulkan validation errors of a game launched with `validation=true`, without those of gamescope and Wine (`tools/test/vk-foreign-vuids.txt`). | |
| `menu_dump` | The scorer items of a player (menus and HUD) in draw order, first drawn first: name, text, position, size, color and alpha, hidden, the OnAccept function and the cursor links (up, down, left, right). The cursor item is marked with `>`. | `player` (default 0), `filter` (regex on name or text), `screenshot` |
| `open_menu` | Opens a menu by its script function: `module.function` is called with the player and an empty control. | `menu` (e.g. `PCMenu.VideoMenu`, `PCMenu.OptionsMenu`, `Menu.MainMenu`), `player`, `screenshot` |
| `click` | Activates an item by name. It runs the OnAccept of the item with the player and the item name, as the accept key of the menu does. | `item`, `player`, `screenshot` |
| `key` | Real key presses into the game. Keys are names like `down`, `enter`, `esc`, `f1`, one character, or combos like `ctrl+a`. | `keys`, `hold` (default 0.08 s), `gap` (default 0.15 s), `screenshot` |
| `mouse` | Real mouse input into the game: relative motion in pixels, then an optional button press. | `dx`, `dy`, `button` (1 left, 2 middle, 3 right), `hold` (default 0.08 s), `screenshot` |
| `configure` | Points `native64`, `native64asan` or `c64` at another build folder for this server process only, from the next launch. An empty `build` goes back to the default. `slots` moves the slot folder (all games ended). | `exe`, `build` (absolute or relative to the repo, e.g. a worktree), `slots` |
| `server_launch` | Starts a network server and returns once it serves. Dedicated by default (`-dedicated`, no own ship); `dedicated=false` is a host that plays (`-server`). | `name` (default `server`), `mode` (ServerType, default `DeathMatch`), `map` (default `FZ`), `maps`, `port` (default the first free UDP port of 29100-29199), `dedicated`, `player`, `max_players`, `fraglimit`, `password`, `bots`, `deterministic`, `reset`, `settings`, `env`, `timeout` |
| `server_quit` | Ends a server. | `name` (default `server`) |
| `state` | What a game sees of the network game, as JSON: level, net flags, own ship, `Ffa.Scorer`, the players and bots (`net<id>`, id up to 512) with `Pos`, `Vel`, `Ang`, `Life`, `Active`, `Descriptor`, `TgType`, and the count of the other `net<id>` objects. | `name` or `names` |
| `hold` | Holds keys and mouse buttons down together for some seconds, on several games at once (XTEST, as `key`). | `name` or `names`, `keys`, `buttons`, `seconds` (default 1), `screenshot` |

`launch` also takes `connect` (`host:port`: the client starts in the menu level, waits until it answers and calls `SNet.InitClient`), `player` (PlayerName, default the game name with `connect`), `team` (PlayerTeamID), `settings` (engine variables passed as `-name:value`) and `reset` (the slot back to the template of the regression runner first: its Scrap.cfg, no saves, no intro video). `wait` has `kind=joined`: a client has its ship on the server; with `spawn=true` it then calls `Ffa.RestartOnPress(0)` (the Fire key of the help screen) and waits until the ship is active.

`map` loads a level with `-imap:` (`FZ` or `Levels/FZ`). `trace` is the number of seconds after the device is created; the render calls of the first frame after that go into `scrapdecomp.log`. `wait` with `kind=log` reads `log.html` by default.

The commands of `cmd` (the comment at the top of `TestChannel.c` has the details):

| Command | Answer |
|---------|--------|
| `<expr>` | the repr of the value. Expressions are literals (as the scripts write constants), `inf`, `nan`, tuples and lists, names (the globals of `__main__`, the builtins, then modules by name, imported when needed), `x.attr`, calls with positional and keyword arguments (`Speech.ForceRemoteCom('a', AfterRemoteComFunc='x')`, `CharAct.NoFXDeath(Scrap.GetEntity('Nurse_01'), 'Player0', 0)`), `x[i]`, `x[a:b]`, unary minus and `is None` or `is not None` at the end. No operators, no statements. |
| `<target> = <expr>` | sets an attribute or item (`CharAct.onNPCDeath = None`, `Scrap.GetEntity('Bank_01').Money = 20`): `ok` |
| `do <expr>` or `do <target> = <expr>` | the same without an answer (only an error answers) |
| `get <var> ...` | engine variables: `[('name', value), ...]` |
| `ent <entity> <var> ...` | entity variables: `[('var', value), ...]`, `'?'` for one the entity does not have |
| `ents <list> [x y z r]` | the sorted names of the entities of a `Scrap.SphereCall` list (`r`, `i`, `v`, ...) in the sphere, by default the whole map |
| `dump [module ...]` | the module dicts (all pack modules without names), one `key<tab><type value>` line per key |
| `menu [player]` | the scorer items as `menu_dump` reads them |
| `click <item> [player]` | runs the OnAccept of the item |
| `con <line>` | a console line (`:name`, `:name value`, `?prefix`, `/command`): what the console shows |
| `print <expr>` | what the print statement writes for the value |
| `recorder <Module.attr> <list> [<literal> \| raise <Exc> <text>]` | puts a C function at the attribute that appends its argument tuples to `__main__.<list>` and returns the literal or raises |
| `check <name><tab><expr>` | the golden form: `<name> = <value>` |

An error answers `error: <type>: <value>`.

`menu_dump` uses `SScorer.Items(player)`, an addition of ScrapC to the script API (the original has no such call), and `SScorer.Get`. OnAccept is readable in ScrapC, the original only allows setting it. It shows `module.function`, or `!native` for a C function. `click` uses `SScorer.Accept(player, item)`, also only in ScrapC.

`key` and `mouse` use XTEST (ctypes with libX11 and libXtst) on the X display of the game's own gamescope. The display is taken from the game process itself: the game binary, or a Wine process running `ScrapC.exe` or `ScrapRef.exe`, with the `SCRAP_GAME_DIR` of that game. Input is refused if that display is the desktop's own (the `DISPLAY` of the server, or `SCRAP_MCP_DESKTOP_DISPLAY`).

Menus are a good place to start a check: `open_menu`, then `menu_dump` before and after a change.

## Network games

The old network code of the game runs as it is (UDP, the original protocol). A server and its clients each get a slot and a gamescope of their own; all traffic stays on 127.0.0.1.

- The dedicated server reads a Scrap.cfg of its own (config folder "") and came up asking for a password, so `server_launch` passes `-UseServerPassword:0` unless `password` is given.
- The command line reader of the engine starts a parameter at every `-`, starts the value again at every `:` and drops blanks: names and values are letters, digits and `_ . [ ] , '`. There is no connect option on the command line, hence the `SNet.InitClient` call.
- A client's ship is inactive after the join until Fire is pressed on the help screen (`wait kind=joined spawn=true`).
- Ship keys: `w` forward, `s` back, `a` `d` strafe, arrows turn, `ctrl` or button 1 fire, button 2 boost, button 3 or `space` countermeasure, `shift` blocks strafing (moves nothing).
- Positions come from the client that flies the ship: a `Pos` set on the server is overwritten, set it on the owning client (the server has it after a key or about 2 s).
- Ids: players `net0` and up, server bots `net256` and up, other objects above `net512`.
- `bots` (3 or more) runs DeathMatchBots: the mode makes the 3 bots of its BotsList, the tool adds the rest through `NDeathMatchBots.BotsList` and `InitServerMod`.

`tools/test/mpgroup.py` runs a server and N clients through this server (stdio, as an assistant does) and checks that they join, that all sides see the ships move to the same places, that a shot kills and that all sides have the same scores (`--help`).

```
server_launch reset=true
launch name=c1 connect=127.0.0.1:29100 player=Alice reset=true
launch name=c2 connect=127.0.0.1:29100 player=Bob reset=true
wait name=c1 kind=joined spawn=true
wait name=c2 kind=joined spawn=true
hold names=[c1, c2] keys=[w] seconds=2
state names=[server, c1, c2]
quit name=all
```

## Example

```
launch name=c exe=native64 map=FZ
launch name=ref exe=ref map=FZ
wait name=c kind=ready
wait name=ref kind=time seconds=60
cmd name=c lines="Scrap.GetLevelPath()"
open_menu name=c menu=PCMenu.VideoMenu
menu_dump name=c filter="Video"
key name=c keys=[down, enter]
screenshot name=c crop=[0,0,640,360] zoom=2
compare names=[c, ref]
quit name=all
```
