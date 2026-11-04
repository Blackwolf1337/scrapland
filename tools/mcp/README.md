# scrap-mcp

An MCP server (version 0.2.0) that runs the game of this checkout, or the frozen reference build `build-ref-noexact/ScrapC.exe`, without a window, so an AI coding assistant can test the Scrapland C recreation. The game runs in gamescope (headless backend), muted, on the AMD 610M by default (`SCRAP_VK_DEVICE=1002:164e`). The default is `native64`, the Linux build in `build-linux64`. The Windows build and the reference build run under Wine.

The server talks MCP over stdio and uses only the Python standard library. Pillow is optional and does crop, zoom and compare.

## Requirements

- Linux
- Python 3.8 or newer
- gamescope with `gamescopectl`
- a built `build-linux64/ScrapC` (the default exe `native64`)
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

The server works on the checkout it lives in. Set `SCRAP_MCP_ROOT` to point it at another checkout. `SCRAP_MCP_DESKTOP_DISPLAY` names a desktop display that input is never sent to, besides the `DISPLAY` of the server itself.

## How it works

Every named game gets a slot under `build/mcp/slot-<name>`. `tools/test/mkgame.sh --into` makes it: the game data is linked, `Bin`, the configs and the Wine prefix are copied. `launch` copies the exe (and the DLLs for the Windows builds) into the slot and writes `scrapdecomp.ini` with `testmod=1`, so the SDTest module exists. Then `tools/test/native.sh` (`native64`, `native64asan`) or `tools/test/wine.sh` (`c64`, `ref`) starts the game with `-PythonExecute` running `mcpgame.py`.

`mcpgame.py` is Python 1.5.2 inside the game. It polls `mcp_cmd.txt` every 0.1 s of game time through `Scrap.AddScheduledFunc`, runs the code and answers in `mcp_reply.txt`. The poller only runs while the world timer runs. During a video or with the game paused it does not answer. `wait kind=ready` returns once the menu or a level runs.

## Tools

All tools take an optional `name` (default `main`), so several games can run at the same time.

| Tool | What it does | Main arguments |
|------|--------------|----------------|
| `launch` | Starts a game in its slot and returns at once. | `exe` (`native64` default, `native64asan`, `c64` or `ref`), `map`, `args`, `env`, `validation`, `trace`, `size` (default 1280x720), `timeout` (default 3600 s) |
| `quit` | Ends a game. | `name=all` ends every game |
| `status` | Lists the games and whether they run. | |
| `wait` | Waits until the game answers Python, a log line matches, or for some seconds. | `kind` (`ready`, `log`, `time`), `pattern`, `which`, `seconds`, `timeout` (default 120), `screenshot` |
| `python` | Runs Python 1.5.2 in the game (modules `Scrap`, `SDTest`). An expression gives its repr, statements give the repr of the global `result`, or `ok`. | `code`, `timeout` (default 10), `screenshot` |
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
| `configure` | Points `native64` or `native64asan` at another build folder for this server process only, from the next launch. An empty `build` goes back to the default. | `exe`, `build` (absolute or relative to the repo, e.g. a worktree) |

`map` loads a level with `-imap:` (`FZ` or `Levels/FZ`). `trace` is the number of seconds after the device is created; the render calls of the first frame after that go into `scrapdecomp.log`. `wait` with `kind=log` reads `log.html` by default.

`menu_dump` uses `SScorer.Items(player)`, an addition of ScrapC to the script API (the original has no such call), and `SScorer.Get`. OnAccept is readable in ScrapC, the original only allows setting it. It shows `module.function`, or `!native` for a C function. `click` uses `SScorer.Accept(player, item)`, also only in ScrapC.

`key` and `mouse` use XTEST (ctypes with libX11 and libXtst) on the X display of the game's own gamescope. The display is taken from the game process itself: the game binary, or a Wine process running `ScrapC.exe` or `ScrapRef.exe`, with the `SCRAP_GAME_DIR` of that game. Input is refused if that display is the desktop's own (the `DISPLAY` of the server, or `SCRAP_MCP_DESKTOP_DISPLAY`).

Menus are a good place to start a check: `open_menu`, then `menu_dump` before and after a change.

## Example

```
launch name=c exe=native64 map=FZ
launch name=ref exe=ref map=FZ
wait name=c kind=ready
wait name=ref kind=ready
python name=c code="Scrap.GetLevelPath()"
open_menu name=c menu=PCMenu.VideoMenu
menu_dump name=c filter="Video"
key name=c keys=[down, enter]
screenshot name=c crop=[0,0,640,360] zoom=2
compare names=[c, ref]
quit name=all
```
