# Testing

Two tests run on the 64 bit Linux build, headless in gamescope.

```
tools/test/golden.sh
tools/test/levels.sh [level ...]
```

## golden.sh

Runs checks in the main menu and compares the output with `tools/test/golden/sdtest.out`. It holds only the 1481 checks of the script functions of the game, plus the closing `done` line and the `Menu item:` print line. `GOLDEN_UPDATE=1` stores a new expected output after you have reviewed it, `SCRAP_BUILD` selects the build folder (default `build-linux64`).

The game starts with the test channel, and `golden/run.py` sends the checks of `tools/test/golden/sdtest.cmd` in the first frame. `sdtest.cmd` is kept by hand: `sdtest.py`, `sdpy.py`, `sdlib.py`, `sdrec.py` and `mkcmd.py` went when the interpreter was removed, as did the python and record modes (the tag `interp-final` has them).

## Test channel

`src/game/Global/TestChannel.c` lets a test tool ask the game things without a script engine. It is off unless the game starts with `-TestChannel` or `SCRAP_TEST_CHANNEL=1`. At the start of each frame (at most every 5 ms, also while the world clock stands) the game reads `test_cmd.txt` in the game folder. A new number on line 1 runs the commands below it, and the answers go to `test_reply.txt` through a temp file and a rename. It never sleeps or blocks, a missing or unchanged file does nothing. It does not answer during a level load.

The commands are expressions and assignments, `do`, `check`, `print`, `con`, `get`, `ent`, `ents`, `dump`, `menu`, `click`, `recorder` and `native`. The tool side is `tools/test/channel.py`, the scrap MCP server uses it (see `tools/mcp/README.md`).

`native <literal>` answers the repr of what the native literal reader of the script ports (`src/game/Script/ScriptNative.c`) makes of the text, `native str <literal>` its str, `native check <literal>` the text the golden `check()` writes (floats with 17 digits). A text the reader does not take answers an error. `native esc [str|check] <text>` is the same with `\n` for a line end and `\\` for a backslash, because the channel reads lines (`litstore.py` sends the stored texts that have a line end this way). `channel.py` has an optional encoding (latin-1 passes the bytes of the game). The command lives in `src/game/Global/TestNative.c`.

The stored output was checked against the frozen i686 reference before it was stored. It equals the reference except where 64 bit differs on purpose: script ints are 64 bits on Linux x86_64 (overflows, `%x` of negative numbers, literal sizes, string hashes), the text of NaN, and there is no SDTest module.

## Deterministic mode

`SCRAP_TEST_DETERMINISTIC=1` makes every regress run repeat byte for byte, so a change is compared exactly against the build before it. It is the default of `tools/test/regress/regress.py`, `golden.sh` and `levels.sh`. `check --live` or `SCRAP_TEST_DETERMINISTIC=0` turns it off, and `check --seed N` picks another seed.

When two runs differ, set `REGRESS_DET_TRACE=<folder>` and compare the traces with `tools/test/regress/dettrace.py`. It shows where the two runs part.

The server port lock is one file for the machine (`~/.cache/scrapland/regress-port.lock`), so runs from several checkouts do not take the same port. `REGRESS_VK_DEVICE` may be a comma list of GPUs. The old switches `SCRAP_NULL_CHECK`, `SCRAP_NULL_CHECK_ENGINE`, `SCRAP_NATIVE_REF` and `SCRAP_PORT_SLOW` are gone.

## Regress cases and the channel

The regress case fc-r8j-snd-DownTown passes `Scrap.Set` on. A recorder that swallowed it made the rail traffic of DownTown load a model that is in no pack (`TLight1.m3d`) and the game ended in about 1 of 6 runs. `native ports` also prints the lines `modules pack/none/absent/other/added/made/loaded` (how the module lookups went) and `def calls registry/byname/other/errors` (how the .def call lines went). `rd/msw-run.py` is gone, and mission starts are compared through the test channel.

## levels.sh

Runs every playable level (29, Nano has no map in the packs) in the ASan build (`build-linux64-asan`) for `LEVELS_TIME` seconds (default 120) and prints one line per level. A level fails on an ASan error, a fatal error (the message box text in `scrapdecomp.log`) or a traceback in the log. The exit code is 0 when all levels pass. Levels can be given as arguments.

The levels run in parallel, `LEVELS_JOBS` at the same time (default 4). Worker n > 0 uses its own game folder `<game>-<n>` (`native.sh` makes it when it is missing). The log of a level is in the game folder of its worker, and the lines still come in the order of the levels. All 29 levels with 60 s each and 6 jobs take about 6 minutes instead of about 45.

## native.sh

`tools/test/native.sh` runs the Linux build headless (`SCRAP_BUILD=build-linux64`, the default).

## litfuzz.sh

`tools/test/litfuzz.sh` takes no arguments. It checks the native values of the script ports against the stored texts of the old Python 1.5.2 (the library is gone, the store stays as the reference). The game runs in the main menu, hidden and muted, in the game folder `<build>/litfuzz` with the test channel.

The store is `tools/test/litfuzz.expected.xz`, 17723 texts of one full run of the old `litfuzz.py` in the reference build (random literal texts of every type, plus 77 repr cases). `tools/test/litstore.py check <game folder> <store>` sends them through the channel command `native`. The native reader must take exactly the 4832 texts 1.5.2 took, give their stored repr, str and `check()` texts, and refuse the others. The run takes about 40 s. The store is the final reference and nothing writes it any more.

`SCRAP_BUILD=build-linux64-asan` runs it under ASan, and it then also fails on an AddressSanitizer report in `<build>/litfuzz.log`. `LITFUZZ_TIMEOUT` sets the time limit (default 1200 seconds). The exit code is 0 when there are no differences.

## Ported scripts

The pack scripts no longer run, because Python is gone from the game. `SCRAP_PACK_SCRIPTS=1` and `SCRAP_CMOD=0` load no script module, and the console says so. The ports were compared with the pack scripts through the regress pack baselines (`tools/test/regress`), which are frozen now. See [scripts.md](scripts.md).

## Multiplayer tests

The unit tests of the multiplayer are ctest targets. They need no game files and no display.

```
ctest --test-dir build-linux64
```

| Test | What it checks |
|------|----------------|
| `net2_test` | the transport (`src/net2`): handshake, crypto, acks, reliable messages, fragments, LAN query, on an in-memory link with loss, duplicates and reordering, plus a fuzz of the decoders |
| `net2_loopback` | the same library over real UDP on loopback with several clients and packet loss |
| `netrep_test` | the replication (`src/netrep`): the codec, snapshots and deltas, the input buffer, prediction and the lag compensation history |
| `coord_test` | the coordinator library (`src/coord`): the key and ticket formats, the player store, the story and party, the places table, the list server registry, the quoting of command lines |
| `faction_test` | the faction table `data/factions/factions.def` (parser, errors, ranks, key order, PvP rules, loan, bounty, courier) |
| `arena_test` | the rules, scores, scorer order and wire format of the seven PvP modes (`src/arena`) |

`coord_test`, `faction_test` and `arena_test` also ran under Wine as win64 programs. `net2_test` passes with `-m32`, with ASan and UBSan and under valgrind, `netrep_test` with `-m32` and with ASan and UBSan.

The scenarios that start a world (coordinator, instances and several clients) are not in the repository. What the repository has are the tools that compare a multiplayer client with single player. They start rendered games, hidden and muted, in their own game folders under `--scratch` and talk to them through the test channel. They use loopback ports from `--port` upward.

```
python3 tools/test/parity/parity.py [--levels A,B] [--build DIR] --out DIR --scratch DIR [--port 29940]
python3 tools/test/parity/feel.py [--sides sp,host,join,menuhost] [--build DIR] --out DIR --scratch DIR
python3 tools/test/parity/coopparity.py --point NAME=DATA:LEVEL[:KEY] ... --build DIR --out DIR --scratch DIR
python3 tools/test/parity/storyedit.py <data folder> [--level L] [name=value ...]
```

`parity.py` starts, per level, a single player game, a hosting game (`-host:<level> -mode:freeroam`) and a second game that joins it. It puts the players at the same place and compares the entities by kind, the radar icons, the HUD items, the sounds playing and the particle systems drawn, and writes `<level>.json`, side by side screenshots and `report.md` with the gaps.

`feel.py` measures on rendered games (single player, host, menu host and join) the time from a real key press (XTEST into the game's own gamescope X server, never the desktop) to the first drawn frame that moved, the frame times, the jitter of the drawn positions and the prediction corrections, walking and flying, and bumps against the client's own rail traffic. The game writes the feel log when `SCRAP_FEELLOG` is set. `SCRAP_PLAYLOG` writes the corrections of a client.

`coopparity.py` is the same comparison for the coop story. A story point is the data folder of a coop world and a level. The tool gives a single player game the party's save variables and loads the level, starts a coordinator with a copy of the data and a rendered member, and compares them. `storyedit.py` edits the saved story of such a data folder (variables, level, entrance and door of the players) to make a story point.

The test channel has verbs for this (`src/game/Global/TestChannel.c`). `entinfo` lists the entities around a place (class, list, model, action class, position, radar flag, and for a proxy the entity it stands for), `radar`, `fx` and `sounds` list what the last frame drew and played, `entsnd <object>` the sounds of one object, `ents`, `menu` and `get` are the old ones. `iline <line>` sends a line to the standard input of the instance, and `channel.py <game folder> -i <level> ...` talks to an instance's own channel (it answers `test_cmd_<level>.txt` in the game folder), so a hosting game's children can be reached.

Instances read test lines on their standard input (or through `iline`): `status`, `money`, `use`, `goto`, `switch`, `call`, `alarm`, `life`, `faction ...`, `arena ...`, `story ...`, and a few more for the coop story. A client plays by itself with `SCRAP_PLAYBOT` (modes such as `idle`, `use`, `face`, or `1` to fight) and sends faction requests with `SCRAP_PLAYBOT_FACTION=<s>:<ability>,...`. `SCRAP_NET2_LOSS`, `SCRAP_NET2_DELAY` and `SCRAP_NET2_JITTER` add packet loss, delay and jitter to the sends of the process.

What the multiplayer tests did not cover is listed in [multiplayer.md](multiplayer.md).

## Retired difftest

The old difftest (i686 build against a frozen reference build, with the test module SDTest) is retired. The tag `difftest-final` keeps the last state with it. No test uses a reference build any more. The scrap MCP server can still start the reference exe for visual comparisons (`exe=ref`, screenshots only, no test channel).
