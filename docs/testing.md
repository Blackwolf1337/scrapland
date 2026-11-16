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

The commands are expressions and assignments, `do`, `check`, `print`, `con`, `get`, `ent`, `ents`, `dump`, `menu`, `click`, `recorder`, `native` and `audit`. The tool side is `tools/test/channel.py`, the scrap MCP server uses it (see `tools/mcp/README.md`).

`native <literal>` answers the repr of what the native literal reader of the script ports (`src/game/Script/ScriptNative.c`) makes of the text, `native str <literal>` its str, `native check <literal>` the text the golden `check()` writes (floats with 17 digits). A text the reader does not take answers an error. `native esc [str|check] <text>` is the same with `\n` for a line end and `\\` for a backslash, because the channel reads lines (`litstore.py` sends the stored texts that have a line end this way). `channel.py` has an optional encoding (latin-1 passes the bytes of the game). The command lives in `src/game/Global/TestNative.c`.

The stored output was checked against the frozen i686 reference before it was stored. It equals the reference except where 64 bit differs on purpose: script ints are 64 bits on Linux x86_64 (overflows, `%x` of negative numbers, literal sizes, string hashes), the text of NaN, and there is no SDTest module.

## Deterministic mode

`SCRAP_TEST_DETERMINISTIC=1` makes every regress run repeat byte for byte, so a change is compared exactly against the build before it. It is the default of `tools/test/regress/regress.py`, `golden.sh` and `levels.sh`. `check --live` or `SCRAP_TEST_DETERMINISTIC=0` turns it off, and `check --seed N` picks another seed.

When two runs differ, set `REGRESS_DET_TRACE=<folder>` and compare the traces with `tools/test/regress/dettrace.py`. It shows where the two runs part.

The server port lock is one file for the machine (`~/.cache/scrapland/regress-port.lock`), so runs from several checkouts do not take the same port. `REGRESS_VK_DEVICE` may be a comma list of GPUs. The old switches `SCRAP_NULL_CHECK`, `SCRAP_NULL_CHECK_ENGINE`, `SCRAP_NATIVE_REF` and `SCRAP_PORT_SLOW` are gone.

## Input tape and solo instance

The input tape records the input of a single player run and plays it back in the deterministic test mode, so a replay repeats the run byte for byte (the same deterministic trace). A solo instance (the headless instance without network, one player whose controls come from the tape) can then be compared with single player. It is for test tools only, the original has nothing like it.

```
ScrapC -imap:Levels/DownTown -inputtape:rec:<file>
ScrapC -imap:Levels/DownTown -inputtape:play:<file>
ScrapC -instance:DownTown -solo:<file>
```

A tape is a text file with two layers. The D lines (device layer) hold the values of all input actions right after the devices wrote them, and a replay writes them back in place of the devices. The C lines (control layer) hold the controls of the player's character or ship per slot tick of 1/60 s. A single player replay checks them, and the solo instance writes them into its player's controls. A start line holds the place, the yaw and the money of the player, and an E line ends the tape. Floats are the hex of their bits. The line format is in `include/scrap/inputtape.h`.

While a tape is on, the trace leaves out the event and command file lines and gets a `pl` line per slot tick of the player's entity: kind, type, pos, ang, vel, view, life, the action of each of the four layers, money and alarm. Without a tape the trace is unchanged, so the regress traces stay the same.

```
tools/test/tape.py rec <level> <tape> [--secs 60]
tools/test/tape.py play <level> <tape>
tools/test/tape.py solo <level> <tape>
tools/test/tape.py cmp <trace A> <trace B>
```

`rec` drives a recording through the test channel verb `tape` (`tape key <Set>.<Action> <value>`, `tape key clear`, `tape end`, and `tape` alone for the state). It runs with `SCRAP_TEST_CHANNEL=step`, so the game clock stands while the driver thinks. The driver walks, boards the player's ship, flies and parks. `play` and `solo` run a tape, and `cmp` compares the `pl` lines of two traces.

```
tools/test/solo.sh <level> <tape> [secs]
```

`solo.sh` runs the replay and the solo instance and prints the first differing slot of the player's state, or `equal`, and the first differing slot per field. The two runs differ today on purpose. The instance steps its players before the frame and its action controllers once per 1/30 s tick, single player does it inside the world update every 1/60 s frame, and the instance lacks single player's setup, such as the boarding of the player's own ship. Stage 3.6 of the multiplayer plan makes them equal.

A replay whose controls differ from the tape prints `inputtape: the controls of slot ... differ from the tape` in the console.

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
| `netclass_test` | the lookup of the network classification tables (`src/game/Script/NetClass.c`, `NetClassTable.c`): sorted tables, every row found, the families of the entity variables |
| `modvarover_test` | the per player overlay of the module variables (`src/game/Script/ModVarOver.c`): which netclass tags get an overlay (PLAYER always, PARTY outside coop, not PLAYER_SHARED, PARTY_SHARED, WORLD, CLIENT), values and deletes per player, a player that has not written a variable reads the module's value, clearing by module and by player |
| `netclass_check` | `tools/script/netclass.py --check`: the generated tables are up to date, every engine API function has a tag, and the untagged rows of the other tables |

`tools/script/netclass.py` makes the tables from `tools/script/netclass/*.tsv` (engine API functions, entity variables, engine variables, script modules, module variables, save variable prefixes, level init steps, `playerread`; the tags are explained at the top of each file). The build runs `netclass.py --check --quiet` before the game code: an engine API function (a row of an engine module's function table) without a tag fails the build. `--scan` adds the names the code and data have that a table lacks.

The table `playerread` lists the reads of "the player" in the world's context of an instance, one row per C function (or `Module.variable` for a module variable tagged PLAYER). The tag is NEAREST (the player nearest to the entity that runs), ANY (some player, the next one each frame), PARTY (coop: the party's acting member, its mission's) or WORLD (no player). Its rows come from the audit of instance runs. The module variable table has the tags PLAYER_SHARED and PARTY_SHARED: state of one player or party whose flow still runs in the world's context of a machine, an NPC, a race or a scene. Such a variable stays one shared value until a later stage moves it to PLAYER or PARTY.

`coord_test`, `faction_test` and `arena_test` also ran under Wine as win64 programs. `net2_test` passes with `-m32`, with ASan and UBSan and under valgrind, `netrep_test` with `-m32` and with ASan and UBSan.

The scenarios that start a world (coordinator, instances and several clients) are not in the repository. What the repository has are the tools that compare a multiplayer client with single player. They start rendered games, hidden and muted, in their own game folders under `--scratch` and talk to them through the test channel. They use loopback ports from `--port` upward.

```
python3 tools/test/parity/parity.py [--levels A,B] [--build DIR] --out DIR --scratch DIR [--port 29940]
python3 tools/test/parity/feel.py [--sides sp,host,join,menuhost] [--build DIR] --out DIR --scratch DIR
python3 tools/test/parity/coopparity.py --point NAME=DATA:LEVEL[:KEY] ... --build DIR --out DIR --scratch DIR
python3 tools/test/parity/storyedit.py <data folder> [--level L] [name=value ...]
```

`parity.py` starts, per level, a single player game, a hosting game (`-host:<level> -mode:freeroam`) and a second game that joins it. It puts the players at the same place and compares the entities by kind, the radar icons, the HUD items, the sounds playing and the particle systems drawn, and writes `<level>.json`, side by side screenshots and `report.md` with the gaps.

`feel.py` measures on rendered games (single player, host, menu host and join) the time from a real key press (XTEST into the game's own gamescope X server, never the desktop) to the first drawn frame that moved, the frame times, the jitter of the drawn positions and the prediction corrections, walking and flying. The game writes the feel log when `SCRAP_FEELLOG` is set. `SCRAP_PLAYLOG` writes the corrections of a client.

`coopparity.py` is the same comparison for the coop story. A story point is the data folder of a coop world and a level. The tool gives a single player game the party's save variables and loads the level, starts a coordinator with a copy of the data and a rendered member, and compares them. `storyedit.py` edits the saved story of such a data folder (variables, level, entrance and door of the players) to make a story point.

The test channel has verbs for this (`src/game/Global/TestChannel.c`). `entinfo` lists the entities around a place (class, list, model, action class, position, radar flag, and for a proxy the entity it stands for), `radar`, `fx` and `sounds` list what the last frame drew and played, `entsnd <object>` the sounds of one object, `sounds src <x> <y> <z> <radius>` the positional sound sources within the radius of a point (lines `ps <name> <sound> <loop> <x> <y> <z> <playing>` for the positional sets, which are the level's sound loops and interval sounds made by `MapSnd`, and `es <entity> <item> <sound> <loop> <x> <y> <z> <playing>` for the sound items of objects), `ents`, `menu` and `get` are the old ones. `iline <line>` sends a line to the standard input of the instance, and `channel.py <game folder> -i <level> ...` talks to an instance's own channel (it answers `test_cmd_<level>.txt` in the game folder), so a hosting game's children can be reached.

`audit` answers the counters of the network audit (`src/game/Global/NetAudit.c`, `include/scrap/netaudit.h`) since the last `audit reset`: the sound, voice and music sinks, effects, debris, animation events, HUD operations and texts, fade, cinema, camera, action set, time speed, the entity variables the scripts set, the reads of "the player" on an instance (a read in a player's context counts as `player.read.bound:<function>` or `player.any.bound:<function>`, a read in the world's context as `player.world.<rule>:<site>` with the rule of its row in the `playerread` table: nearest, any, party or world, and as `player.world.norule:<site>` for a site without a row, stage 1.3 gate: none on the 11 city levels with 8 bots) and the reads of the render flag and the camera distance on a headless process. One line per key, by count down. `audit json` gives the same as one line of JSON. The counters are always on and change no output.

The instance test lines for the viewer set (`src/game/World/Viewers.c`, `include/scrap/viewers.h`) are `viewers`, which lists the viewers (id, leader, pose, camera), `viewers fake <n> [seed]`, which adds n test viewers on nodes of the level's ground graph spread as far apart as the graph allows, each turning slowly (the load probe), `viewers clear` and `viewers share <units>`, which sets the share distance. `ticks` prints the tick count, p50, p99, max and mean tick time in ms since `ticks reset`, the dropped ticks, the viewers and the leaders. `traffic` prints the entity count and, per traffic list, the ships, the active ones and the quota, and `traffic cap <leaders> cheap <units> <slot>` sets the quota cap (0 none), the cheap distance (0 none) and the slot interval of cheap ships.

The audit of a `-mp2` client has more keys in the category `create`. `client.sim.call:<Module>.<func>` counts every call of a script module function tagged SIM in the module table, in `Port_Entry`, because the client runs no simulation script. `client.rail.call:` and `client.rail.create:` count the calls and creations of the rail traffic's modules (`Traffic`, `VehiclesNPC` and `InTraffic` and what they call) on the client. Since stage 2.2 that traffic runs only on the server, so both stay 0. `client.script.create:` counts an entity the client made itself. `mirror json` of the client also gives `simcalls`.

The module table of the netclass tables may have rows `Module.func` that tag one function of a mixed module (looked up before the module row, `NetClass_ModuleTag`). The initstep table has one more tag: GROUP (a group of steps whose own steps have rows). The rail traffic steps are tagged WORLD, so they are the server's. A `-mp2` client on a level that has a client part (DownTown and Police so far, `Lv_Client` in `src/game/Script/levels/Levels.h`) runs only the DEF and GROUP steps of the level start. On other levels it runs every step and deletes what that made, as stage 3.2 did (`client.purge:`).

```
python3 tools/test/parity/census.py --client <game> --instance <game> --level <Level> --sp <game> [--radius 15000] [--json FILE]
```

`census.py` needs three running games with the test channel: a joined client started with `-mp2`, its instance, and a single player game of the same level. It takes the client's view point from `mirror json` and asks all three for `sounds src`. It compares the positional sets of the client against single player (around the player and in the whole level), and the object sound items of the client against the instance (entities both have) and against single player (only for entities where the instance and single player agree, the others are entities of the moment such as a parked ship of another type, NPCs and traffic). Playing states are listed but not counted, because they depend on where each game's listener is. Rail traffic and the players' own entities are left out. The exit code is 0 without gaps. Stage 3.3 gate on DownTown and Police with one joined `-mp2` client: `client.sim.call`, `client.script.create` and `client.purge` are 0 and the census has 0 gaps.

`SCRAP_PLAYER_TRACE=1` in the environment of an instance prints `pltrace` lines to its log. Each change of a player's money or alarm is printed with the context it happened in (`in player N` or `in the world`) and the dispatch point that entered that context. Each callback dispatched to a player prints `pltrace callback <function> for player N`. `pltrace WRONG` means a player's context changed another player's money or alarm. The frame loops (the entity updates, the input step, the alarm updates) print no callback lines.

Instances read test lines on their standard input (or through `iline`): `status`, `money`, `use`, `goto`, `switch`, `call`, `alarm`, `life`, `faction ...`, `arena ...`, `story ...`, and a few more for the coop story. A client plays by itself with `SCRAP_PLAYBOT` (modes such as `idle`, `use`, `face`, or `1` to fight) and sends faction requests with `SCRAP_PLAYBOT_FACTION=<s>:<ability>,...`. `SCRAP_NET2_LOSS`, `SCRAP_NET2_DELAY` and `SCRAP_NET2_JITTER` add packet loss, delay and jitter to the sends of the process.

What the multiplayer tests did not cover is listed in [multiplayer.md](multiplayer.md).

## Retired difftest

The old difftest (i686 build against a frozen reference build, with the test module SDTest) is retired. The tag `difftest-final` keeps the last state with it. No test uses a reference build any more. The scrap MCP server can still start the reference exe for visual comparisons (`exe=ref`, screenshots only, no test channel).
