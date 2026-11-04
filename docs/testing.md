# Testing

Two tests run on the 64 bit Linux build, headless in gamescope.

```
tools/test/golden.sh
tools/test/levels.sh [level ...]
```

## golden.sh

Runs `tools/test/golden/sdtest.py` in the main menu: the script functions of the game and the Python interpreter (`sdpy.py`), 9961 checks. The output is compared with `tools/test/golden/sdtest.out`. `GOLDEN_UPDATE=1` stores a new expected output, `SCRAP_BUILD` selects the build folder (default `build-linux64`).

The stored output was checked against the frozen i686 reference before it was stored. It equals the reference except where 64 bit differs on purpose: Python ints are 64 bits on Linux x86_64 (overflows, `%x` of negative numbers, literal sizes, string hashes), the text of NaN, and there is no SDTest module.

## levels.sh

Runs every playable level (29, Nano has no map in the packs) in the ASan build (`build-linux64-asan`) for `LEVELS_TIME` seconds (default 120) and prints one line per level. A level fails on an ASan error, a fatal error (the message box text in `scrapdecomp.log`) or a Python traceback. The exit code is 0 when all levels pass. Levels can be given as arguments.

## native.sh

`tools/test/native.sh` runs the Linux build headless (`SCRAP_BUILD=build-linux64`, the default).

## Ported scripts

A ported script module is compared with the original by running the same build with `SCRAP_PACK_SCRIPTS=1`, see [scripts.md](scripts.md).

## Retired difftest

The old difftest (i686 build against a frozen reference build, with the test module SDTest) is retired. The tag `difftest-final` keeps the last state with it. The reference builds are not used by a test any more. The scrap MCP server can still start the reference exe for visual comparisons (`exe=ref`).
