# scrap-mcp

An MCP server that runs `ScrapC.exe` of this checkout, or the frozen reference build `build-ref-noexact/ScrapC.exe`, without a window, so an AI coding assistant can test the Scrapland C recreation. The game runs in gamescope (headless backend) under Wine, muted, on the AMD 610M by default (`SCRAP_VK_DEVICE=1002:164e`).

The server talks MCP over stdio and uses only the Python standard library. Pillow is optional and does crop, zoom and compare.

## Requirements

- Linux
- Python 3.8 or newer
- gamescope with `gamescopectl`
- Wine with the project prefix (`tools/test/prefix.sh`)
- a built `build/ScrapC.exe` (`cmake --build build`)
- the FFmpeg DLLs from `tools/ffmpeg.sh` (`build/ffmpeg/install/bin`)
- the game data in `game/` of the checkout
- the Vulkan validation layer, only for `validation=true`
- Pillow for `crop`, `zoom` and `compare`. Without it screenshots are sent at full size.

## Setup

```
claude mcp add --scope user scrap -- python3 /home/claude/projects/scrapland/tools/mcp/scrap_mcp.py
```

The server works on the checkout it lives in. Set `SCRAP_MCP_ROOT` to point it at another checkout.

## How it works

Every named game gets a slot under `build/mcp/slot-<name>`. `tools/test/mkgame.sh --into` makes it: the game data is linked, `Bin`, the configs and the Wine prefix are copied. `launch` copies the exe and the FFmpeg DLLs into the slot and writes `scrapdecomp.ini` with `testmod=1`, so the SDTest module exists. Then `tools/test/wine.sh` starts the game with `-PythonExecute` running `mcpgame.py`.

`mcpgame.py` is Python 1.5.2 inside the game. It polls `mcp_cmd.txt` every 0.1 s of game time through `Scrap.AddScheduledFunc`, runs the code and answers in `mcp_reply.txt`. The poller only runs while the world timer runs. During a video or with the game paused it does not answer. `wait kind=ready` returns once the menu or a level runs.

## Tools

All tools take an optional `name` (default `main`), so several games can run at the same time.

| Tool | What it does | Main arguments |
|------|--------------|----------------|
| `launch` | Starts a game in its slot and returns at once. | `exe` (`c` or `ref`), `map`, `args`, `env`, `validation`, `trace`, `size` (default 1280x720), `timeout` (default 3600 s) |
| `quit` | Ends a game. | `name=all` ends every game |
| `status` | Lists the games and whether they run. | |
| `wait` | Waits until the game answers Python, a log line matches, or for some seconds. | `kind` (`ready`, `log`, `time`), `pattern`, `which`, `seconds`, `timeout` (default 120), `screenshot` |
| `python` | Runs Python 1.5.2 in the game (modules `Scrap`, `SDTest`). An expression gives its repr, statements give the repr of the global `result`, or `ok`. | `code`, `timeout` (default 10), `screenshot` |
| `screenshot` | The window of the game. | `crop` (x, y, w, h), `zoom` |
| `compare` | Screenshots of several games side by side, for example `c` and `ref` on the same level. | `names`, `crop` |
| `log` | The end of `log.html` (`which=game`) or `scrapdecomp.log` (`which=engine`). | `which`, `lines` (default 40), `pattern` |
| `trace` | The traced frame of a game launched with `trace=`: a count of the draws by target and pixel shader, then the first 80 lines. | `full` |
| `vkcheck` | The Vulkan validation errors of a game launched with `validation=true`, without those of gamescope and Wine (`tools/test/vk-foreign-vuids.txt`). | |

`map` loads a level with `-imap:` (`FZ` or `Levels/FZ`). `trace` is the number of seconds after the device is created; the render calls of the first frame after that go into `scrapdecomp.log`. `wait` with `kind=log` reads `log.html` by default.

## Example

```
launch name=c exe=c map=FZ
launch name=ref exe=ref map=FZ
wait name=c kind=ready
wait name=ref kind=ready
python name=c code="Scrap.GetLevelPath()"
screenshot name=c crop=[0,0,640,360] zoom=2
compare names=[c, ref]
quit name=all
```

## Not there

Keyboard and mouse input is not implemented. Scenes are driven with Python in the game.
