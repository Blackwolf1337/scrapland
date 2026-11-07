# Testing

Two tests run on the 64 bit Linux build, headless in gamescope.

```
tools/test/golden.sh
tools/test/levels.sh [level ...]
```

## golden.sh

Runs 9961 checks in the main menu and compares the output with `tools/test/golden/sdtest.out`. `GOLDEN_UPDATE=1` stores a new expected output, `SCRAP_BUILD` selects the build folder (default `build-linux64`).

`GOLDEN_MODE` selects how the checks run. `channel` is the default and starts the game without `-PythonExecute`. The 1481 checks of the script functions of the game (`tools/test/golden/sdtest.cmd`) go through the test channel in the first frame. The 8480 checks of the Python interpreter (`sdpy.py`) still run Python through the channel command `py`. They go with the interpreter at stage 7 of the removal of Python. `python` is the old way, `sdtest.py` and `sdpy.py` run as Python in the game. `record` rebuilds `sdtest.cmd`. The total is 9961 in both ways.

## Test channel

`src/game/Global/TestChannel.c` lets a test tool ask the game things without running Python in it. It is off unless the game starts with `-TestChannel` or `SCRAP_TEST_CHANNEL=1`. At the start of each frame (at most every 5 ms, also while the world clock stands) the game reads `test_cmd.txt` in the game folder. A new number on line 1 runs the commands below it, and the answers go to `test_reply.txt` through a temp file and a rename. It never sleeps or blocks, a missing or unchanged file does nothing. It does not answer during a level load.

The commands are expressions and assignments, `do`, `check`, `print`, `con`, `get`, `ent`, `ents`, `dump`, `menu`, `click`, `recorder` and `py`. `py` still runs Python and goes with the interpreter at stage 7. The tool side is `tools/test/channel.py`, the scrap MCP server uses it (see `tools/mcp/README.md`).

The stored output was checked against the frozen i686 reference before it was stored. It equals the reference except where 64 bit differs on purpose: Python ints are 64 bits on Linux x86_64 (overflows, `%x` of negative numbers, literal sizes, string hashes), the text of NaN, and there is no SDTest module.

In the ASan build 14 lines of `sdpy.py` (`cmp`) differ in both modes, because the ASan `strcmp` interceptor returns -1 or 1. This is known and not a regression.

## levels.sh

Runs every playable level (29, Nano has no map in the packs) in the ASan build (`build-linux64-asan`) for `LEVELS_TIME` seconds (default 120) and prints one line per level. A level fails on an ASan error, a fatal error (the message box text in `scrapdecomp.log`) or a Python traceback. The exit code is 0 when all levels pass. Levels can be given as arguments.

The levels run in parallel, `LEVELS_JOBS` at the same time (default 4). Worker n > 0 uses its own game folder `<game>-<n>` (`native.sh` makes it when it is missing). The log of a level is in the game folder of its worker, and the lines still come in the order of the levels. All 29 levels with 60 s each and 6 jobs take about 6 minutes instead of about 45.

## native.sh

`tools/test/native.sh` runs the Linux build headless (`SCRAP_BUILD=build-linux64`, the default).

## Ported scripts

A ported script module is compared with the original by running the same build with `SCRAP_PACK_SCRIPTS=1`, see [scripts.md](scripts.md).

## Retired difftest

The old difftest (i686 build against a frozen reference build, with the test module SDTest) is retired. The tag `difftest-final` keeps the last state with it. The reference builds are not used by a test any more. The scrap MCP server can still start the reference exe for visual comparisons (`exe=ref`).
