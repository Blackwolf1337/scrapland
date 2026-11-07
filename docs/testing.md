# Testing

Two tests run on the 64 bit Linux build, headless in gamescope.

```
tools/test/golden.sh
tools/test/levels.sh [level ...]
```

## golden.sh

Runs checks in the main menu and compares the output with `tools/test/golden/sdtest.out`. The default build has no Python interpreter and runs 1481 checks, the API part of the stored output. The reference build (see below) runs all 9961. `GOLDEN_UPDATE=1` stores a new expected output, `SCRAP_BUILD` selects the build folder (default `build-linux64`).

The builds have no Python interpreter by default (`SCRAP_NO_PYTHON` is ON since 2026-11-08). The reference build with the interpreter is `cmake -B build-py64 -DCMAKE_TOOLCHAIN_FILE=cmake/linux-x86_64.cmake -DSCRAP_NO_PYTHON=OFF`. It is kept for one round, until step B of the removal of Python. Only some test modes need it, they are named below.

`GOLDEN_MODE` selects how the checks run. `channel` is the default and starts the game without `-PythonExecute`. The 1481 checks of the script functions of the game (`tools/test/golden/sdtest.cmd`) go through the test channel in the first frame. In the reference build the 8480 checks of the Python interpreter (`sdpy.py`) also run, through the channel command `py`. They go with the interpreter at step B of the removal of Python. `python` is the old way, `sdtest.py` and `sdpy.py` run as Python in the game. `record` rebuilds `sdtest.cmd`. The total is 9961 in both ways in the reference build (`SCRAP_BUILD=build-py64`). In a build without the interpreter (read from `CMakeCache.txt`) only `channel` runs, the 8480 interpreter checks are skipped, and `GOLDEN_UPDATE`, `python` and `record` are refused. They need the reference build.

## Test channel

`src/game/Global/TestChannel.c` lets a test tool ask the game things without running Python in it. It is off unless the game starts with `-TestChannel` or `SCRAP_TEST_CHANNEL=1`. At the start of each frame (at most every 5 ms, also while the world clock stands) the game reads `test_cmd.txt` in the game folder. A new number on line 1 runs the commands below it, and the answers go to `test_reply.txt` through a temp file and a rename. It never sleeps or blocks, a missing or unchanged file does nothing. It does not answer during a level load.

The commands are expressions and assignments, `do`, `check`, `print`, `con`, `get`, `ent`, `ents`, `dump`, `menu`, `click`, `recorder`, `native` and `py`. `py` runs Python in the reference build and answers an error in the default build. It goes with the interpreter at step B. The tool side is `tools/test/channel.py`, the scrap MCP server uses it (see `tools/mcp/README.md`).

`native <literal>` answers the repr of what the native literal reader of the script ports (`src/game/Script/ScriptNative.c`) makes of the text, `native str <literal>` its str, `native check <literal>` the text the golden `check()` writes (floats with 17 digits). A text the reader does not take answers an error. `native esc [str|check] <text>` is the same with `\n` for a line end and `\\` for a backslash, because the channel reads lines (`litstore.py` sends the stored texts that have a line end this way). `channel.py` has an optional encoding (latin-1 passes the bytes of the game). `native py` puts two functions into `__main__` for Python run by the `py` command (used by the reference build of `litfuzz.sh`, goes with `py`). The command lives in `src/game/Global/TestNative.c`.

The stored output was checked against the frozen i686 reference before it was stored. It equals the reference except where 64 bit differs on purpose: Python ints are 64 bits on Linux x86_64 (overflows, `%x` of negative numbers, literal sizes, string hashes), the text of NaN, and there is no SDTest module.

In the ASan build 14 lines of `sdpy.py` (`cmp`) differ in both modes, because the ASan `strcmp` interceptor returns -1 or 1. This is known and not a regression.

## levels.sh

Runs every playable level (29, Nano has no map in the packs) in the ASan build (`build-linux64-asan`) for `LEVELS_TIME` seconds (default 120) and prints one line per level. A level fails on an ASan error, a fatal error (the message box text in `scrapdecomp.log`) or a Python traceback. The exit code is 0 when all levels pass. Levels can be given as arguments.

The levels run in parallel, `LEVELS_JOBS` at the same time (default 4). Worker n > 0 uses its own game folder `<game>-<n>` (`native.sh` makes it when it is missing). The log of a level is in the game folder of its worker, and the lines still come in the order of the levels. All 29 levels with 60 s each and 6 jobs take about 6 minutes instead of about 45.

## native.sh

`tools/test/native.sh` runs the Linux build headless (`SCRAP_BUILD=build-linux64`, the default).

## litfuzz.sh

`tools/test/litfuzz.sh [texts per seed] [seeds]` (defaults 3200 and seeds 1 to 5) checks the native values of the script ports against Python 1.5.2. The game runs in the main menu, hidden and muted, in the game folder `<build>/litfuzz` with the test channel.

In the default build (no interpreter) it checks stored texts, `tools/test/litfuzz.expected.xz`. The store has 17723 distinct texts of one full run of `litfuzz.py` with the defaults (3200 texts per seed, seeds 1 to 5, each with and without the wide atoms, plus the repr texts). The host script `tools/test/litstore.py` sends them through the channel command `native` (`litstore.py check <game folder> <store>`, `litstore.py save` writes the store). The native reader must take exactly the texts 1.5.2 took (4832) and give their stored repr, str and `check()` texts, and refuse the others. This takes about 45 s. The arguments apply only to the reference build.

In the reference build (`SCRAP_BUILD=build-py64`) `tools/test/litfuzz.py` runs in the game through the `py` command and compares live with the linked 1.5.2, as described below. `LITFUZZ_UPDATE=1` writes the store again when the run has no differences. `LITFUZZ_STORE=1` runs the stored texts in the reference build too.

For each seed it makes random literal texts twice, once with the atoms of the nopy-4 fuzz of `Script_Literal` (the same corpus) and once with more atoms (dicts that grow and collide, keys of every hashable type, long ints, nesting at the limit of 32, long texts). For every text the native reader must take it exactly when `Script_Literal` takes it. A taken text must eval to the same value and type, and the native repr, str and `check()` texts must equal repr, str and the `fmt` of `tools/test/golden/sdlib.py` of that value. The repr of every taken value is checked again as a text.

The repr cases (77 values of every type: None, ints at the limits, long ints, floats with negative zero, inf and nan, texts with every byte and both quotes, nested containers, dicts with collisions and with deleted keys) are copied into native values and back and must give the same value and the same texts. This needs Python objects, so it runs only in the reference build.

`SCRAP_BUILD=build-linux64-asan` runs it under ASan, and it then also fails on an AddressSanitizer report in `<build>/litfuzz.log`. `LITFUZZ_TIMEOUT` sets the time limit (default 1200 seconds). The exit code is 0 when there are no differences. A full live run of 5 seeds in the reference build takes about 15 minutes, under ASan about the same.

## Ported scripts

A ported script module is compared with the original by running the reference build with `SCRAP_PACK_SCRIPTS=1`, see [scripts.md](scripts.md). In a build without the interpreter `SCRAP_PACK_SCRIPTS=1` and `SCRAP_CMOD=0` load no script module, and the console says so.

## Retired difftest

The old difftest (i686 build against a frozen reference build, with the test module SDTest) is retired. The tag `difftest-final` keeps the last state with it. The reference builds are not used by a test any more. The scrap MCP server can still start the reference exe for visual comparisons (`exe=ref`).
