# Testing

Two tests run on the 64 bit Linux build, headless in gamescope.

```
tools/test/golden.sh
tools/test/levels.sh [level ...]
```

## golden.sh

Runs 9961 checks in the main menu and compares the output with `tools/test/golden/sdtest.out`. `GOLDEN_UPDATE=1` stores a new expected output, `SCRAP_BUILD` selects the build folder (default `build-linux64`).

`GOLDEN_MODE` selects how the checks run. `channel` is the default and starts the game without `-PythonExecute`. The 1481 checks of the script functions of the game (`tools/test/golden/sdtest.cmd`) go through the test channel in the first frame. The 8480 checks of the Python interpreter (`sdpy.py`) still run Python through the channel command `py`. They go with the interpreter at stage 7 of the removal of Python. `python` is the old way, `sdtest.py` and `sdpy.py` run as Python in the game. `record` rebuilds `sdtest.cmd`. The total is 9961 in both ways. In a build with `SCRAP_NO_PYTHON` (read from `CMakeCache.txt`) only `channel` runs, the 8480 interpreter checks are skipped, and `GOLDEN_UPDATE` is refused.

## Test channel

`src/game/Global/TestChannel.c` lets a test tool ask the game things without running Python in it. It is off unless the game starts with `-TestChannel` or `SCRAP_TEST_CHANNEL=1`. At the start of each frame (at most every 5 ms, also while the world clock stands) the game reads `test_cmd.txt` in the game folder. A new number on line 1 runs the commands below it, and the answers go to `test_reply.txt` through a temp file and a rename. It never sleeps or blocks, a missing or unchanged file does nothing. It does not answer during a level load.

The commands are expressions and assignments, `do`, `check`, `print`, `con`, `get`, `ent`, `ents`, `dump`, `menu`, `click`, `recorder`, `native` and `py`. `py` still runs Python and goes with the interpreter at stage 7. The tool side is `tools/test/channel.py`, the scrap MCP server uses it (see `tools/mcp/README.md`).

`native <literal>` answers the repr of what the native literal reader of the script ports (`src/game/Script/ScriptNative.c`) makes of the text, `native str <literal>` its str, `native check <literal>` the text the golden `check()` writes (floats with 17 digits). A text the reader does not take answers an error. `native py` puts two functions into `__main__` for Python run by the `py` command (used by `litfuzz.sh`, STAGE 7 like `py`). The command lives in `src/game/Global/TestNative.c`.

The stored output was checked against the frozen i686 reference before it was stored. It equals the reference except where 64 bit differs on purpose: Python ints are 64 bits on Linux x86_64 (overflows, `%x` of negative numbers, literal sizes, string hashes), the text of NaN, and there is no SDTest module.

In the ASan build 14 lines of `sdpy.py` (`cmp`) differ in both modes, because the ASan `strcmp` interceptor returns -1 or 1. This is known and not a regression.

## levels.sh

Runs every playable level (29, Nano has no map in the packs) in the ASan build (`build-linux64-asan`) for `LEVELS_TIME` seconds (default 120) and prints one line per level. A level fails on an ASan error, a fatal error (the message box text in `scrapdecomp.log`) or a Python traceback. The exit code is 0 when all levels pass. Levels can be given as arguments.

The levels run in parallel, `LEVELS_JOBS` at the same time (default 4). Worker n > 0 uses its own game folder `<game>-<n>` (`native.sh` makes it when it is missing). The log of a level is in the game folder of its worker, and the lines still come in the order of the levels. All 29 levels with 60 s each and 6 jobs take about 6 minutes instead of about 45.

## native.sh

`tools/test/native.sh` runs the Linux build headless (`SCRAP_BUILD=build-linux64`, the default).

## litfuzz.sh

`tools/test/litfuzz.sh [texts per seed] [seeds]` (defaults 3200 and seeds 1 to 5) checks the native values of the script ports against the linked Python 1.5.2. The game runs in the main menu, hidden and muted, in the game folder `<build>/litfuzz` with the test channel. `tools/test/litfuzz.py` runs in the game through the `py` command of the channel. It needs a build with the interpreter.

For each seed it makes random literal texts twice, once with the atoms of the nopy-4 fuzz of `Script_Literal` (the same corpus) and once with more atoms (dicts that grow and collide, keys of every hashable type, long ints, nesting at the limit of 32, long texts). For every text the native reader must take it exactly when `Script_Literal` takes it. A taken text must eval to the same value and type, and the native repr, str and `check()` texts must equal repr, str and the `fmt` of `tools/test/golden/sdlib.py` of that value. The repr of every taken value is checked again as a text.

The repr cases (77 values of every type: None, ints at the limits, long ints, floats with negative zero, inf and nan, texts with every byte and both quotes, nested containers, dicts with collisions and with deleted keys) are copied into native values and back and must give the same value and the same texts.

`SCRAP_BUILD=build-linux64-asan` runs it under ASan, and it then also fails on an AddressSanitizer report in `<build>/litfuzz.log`. `LITFUZZ_TIMEOUT` sets the time limit (default 1200 seconds). The exit code is 0 when there are no differences. A full run of 5 seeds takes about 15 minutes, under ASan about the same.

## Ported scripts

A ported script module is compared with the original by running the same build with `SCRAP_PACK_SCRIPTS=1`, see [scripts.md](scripts.md).

## Retired difftest

The old difftest (i686 build against a frozen reference build, with the test module SDTest) is retired. The tag `difftest-final` keeps the last state with it. The reference builds are not used by a test any more. The scrap MCP server can still start the reference exe for visual comparisons (`exe=ref`).
