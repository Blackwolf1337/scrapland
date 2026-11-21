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

A tape is a text file with two layers. The D lines (device layer) hold the values of all input actions right after the devices wrote them, and a replay writes them back in place of the devices. The C lines (control layer) hold the controls of the player's character or ship per slot tick of 1/60 s, ending in `i <len>`, the length of the slot. A single player replay checks the controls and the slot length, and the solo instance writes the controls into its player's controls. An F line (`F <frame> <time>`) comes before each App_Update of a frame and holds the frame's time, and the C lines after it are the slots of that frame. An S line (`S <name> <time>`) is written for each object entity made: the name it was made with and the time to its first slot, the phase of its slot controller. A start line holds the place, the yaw and the money of the player, and an E line ends the tape. Floats are the hex of their bits. Old tapes without F, S and `i` still play, and the solo instance then steps two slots per tick of 1/30 s. The line format is in `include/scrap/inputtape.h`.

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

`solo.sh` runs the replay and the solo instance and prints the first differing slot of the player's state, or `equal`, and the first differing slot per field.

On a tape with F lines the solo instance runs one world frame per F line with that frame's time. It steps its player's slots in the frames single player stepped them, with their lengths, and gives the entities single player made the same slot phases (the S lines). The S lines are needed because the random numbers are shared by the simulation and by single player's drawing, sounds and effects. The n-th number differs between single player and the headless instance, and an entity such as a parking box would test its touches in other frames.

The player's step on the server is single player's frame of the entity: its actions (`cActCtrl_Tick`), then its slot (`Obj_Tick`), 1/60 s each. It runs in the entity's own update in the world's entity loop, where single player steps its player (`Rep_OwnFrame`). A tick of the network (1/30 s) has two such parts (`NetPl_Step`). The solo player, and every player of a `-mp2` instance that is not in coop or a PvP mode, gets single player's own flows (`Rep_SpFlows`): its own ship and Sputnik at the parking nearest to it (as `Parking.BrowseItemPos` gives single player's), boarding a parked ship by its use arrow, and parking on a box. The old path keeps the instance's stand-ins, the switch into the nearest parked ship and a called ship.

`SCRAP_TAPE_DUMP=<slot>[,<radius>]` writes a trace line per entity within the radius (default 3000) of the player's entity after that slot (`ent <name> <class> <active> <x y z> <action class>`), to find which entities two runs differ in.

On the stage 3.6 tapes (recorded with `tape.py rec`, about 67 s each) DownTown and Outskirts are equal over all slots (4008 and 4010): walking, boarding, flying, firing, parking, walking. Police differs from slot 75 on. The NPCs stand elsewhere (a sentinel at the top of the stairs), because their placement and moves come from random numbers single player shares with its drawing.

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

The table `playerread` lists the reads of "the player" in the world's context of an instance, one row per C function (or `Module.variable` for a module variable tagged PLAYER). The tag is NEAREST (the player nearest to the entity that runs), ANY (some player, the next one each frame), PARTY (coop: the acting member or the nearest), MENU (a menu or dialog: the acting member), DEAD (a living member first, so dead means every member dead), ALL (a setter: the site runs once for every member through `Party_All`, `Scrap.SetAlarm` is one) or WORLD (no player). Outside coop PARTY, MENU, DEAD and ALL are NEAREST. Its rows come from the audit of instance runs. The module variable table has the tags PLAYER_SHARED and PARTY_SHARED: state of one player or party whose flow still runs in the world's context of a machine, an NPC, a race or a scene. Such a variable stays one shared value until a later stage moves it to PLAYER or PARTY.

`coord_test`, `faction_test` and `arena_test` also ran under Wine as win64 programs. `net2_test` passes with `-m32`, with ASan and UBSan and under valgrind, `netrep_test` with `-m32` and with ASan and UBSan.

The scenarios that start a world (coordinator, instances and several clients) are not in the repository. What the repository has are the tools that compare a multiplayer client with single player. They start rendered games, hidden and muted, in their own game folders under `--scratch` and talk to them through the test channel. They use loopback ports from `--port` upward.

```
python3 tools/test/parity/parity.py [--levels A,B] [--build DIR] --out DIR --scratch DIR [--port 29940]
python3 tools/test/parity/feel.py [--sides sp,host,join,menuhost] [--build DIR] --out DIR --scratch DIR
python3 tools/test/parity/coopparity.py --point NAME=DATA:LEVEL[:KEY] ... --build DIR --out DIR --scratch DIR
python3 tools/test/parity/storyedit.py <data folder> [--level L] [name=value ...]
```

`parity.py` starts, per level, a single player game, a hosting game (`-host:<level> -mode:freeroam`) and a second game that joins it. It puts the players at the same place and compares the entities by kind, the radar icons, the HUD items, the sounds playing and the particle systems drawn, and writes `<level>.json`, side by side screenshots and `report.md` with the gaps.

`feel.py` measures on rendered games (single player, host, menu host and join) the time from a real key press (XTEST into the game's own gamescope X server, never the desktop) to the first drawn frame that moved, the frame times, the jitter of the drawn positions and the prediction corrections, walking and flying. The game writes the feel log when `SCRAP_FEELLOG` is set, each frame's line with the time the frame's state was drawn (the next frame's render: the time after the frame's work varied with the load and showed as jitter). `SCRAP_PLAYLOG` writes the corrections of a client. With `--mp2` a client boards its own ship (`PlayerShip<n>`, through the instance's `goto` and `use` lines) when the instance has it active, else the nearest parked ship; the rail traffic of the flights into it (`--bumps`) is the instance's.

`coopparity.py` is the same comparison for the coop story. A story point is the data folder of a coop world and a level. The tool gives a single player game the party's save variables and loads the level, starts a coordinator with a copy of the data and a rendered member, and compares them. `storyedit.py` edits the saved story of such a data folder (variables, level, entrance and door of the players) to make a story point.


`coopparity.py --mp2` starts the instances and the member with `-mp2`. It compares the kinds of the moment against the instance's count, and the sounds and particle systems the instance played near a player count, as `parity.py --mp2` does. `--one-level` makes the world host only the point's level: the coordinator on `--port`, its instance on `--port + 1`, two ports instead of 28.

The coop chain tools in `tools/test/coop` (`autochain.sh`, `chap.sh`, `chainenv.sh`, `chapters.txt`) run on the second multiplayer design when `COOP_MP2=1` is set. The coordinator gets `-instargs:-mp2`, so its instances start with `-mp2`, and both headless players start with `-mp2`. `CHAP_WORLD_ARGS` passes more arguments to the coordinator. For a second world in a small port block use `CHAP_WORLD_ARGS="-instport:30109 -levels:Press,Orbit"`: the instances take ports from `-instport` on, one per level of `-levels`, and Orbit is always added.

`autochain.sh` takes more variables for the coop story chain on `-mp2`. `CHAP_PLAYERS` is 1 (pa alone, `chap.sh` starts no pb), 2 (pa and pb, the default) or more: pa, pb and p3 to pN, where p3 to pN join as more members through `CHAP_MORE` of `chap.sh`. They go to their leader with the travel and help in the fights, and the arguments of their test line come from `AUTO_M_<Mission>` (default `2`). `CHAP_FIRST=<k>` makes the rounds the chapters k, k+1, ... of `story.txt`, with the mission and its level taken from there. A chapter that is no mission of the scripts (Fake) or a super deal takes the place of the story's mission. Any other difference stops the chain with "stopped, the story is at X, not at Y" (the chapter before did not end the way the story goes on). `CHAP_LEVELS=auto` makes the world of each chapter host only the levels of the chapter: where the party is, the mission's level, a super deal's arena and the next chapter's level of `story.txt`, with Orbit always added. Its instances take ports from the chain's port + 1 on, so a chain needs a block of about 10 ports. An empty data folder is a new world: the chain starts with chapter 1 in Orbit. A chapter ends when the story names another mission, when the scripts take the party on ("travels to ... with its party"), when pa takes a door whose use is the mission's ("player N travels to ..., Door...") or when the scripts send pa on alone ("mission level change of player N"). The last two let a chapter played alone end although its level's instance stops. `AUTO_A_<Mission>=-` makes pa play nothing by itself, which is right for the chapters that `AUTO_MID` steps play (MapDecont, Fake, RustyFirstContact).

The super deal chapters (the missions SuperDeal_First to SuperDeal_Twelfth, chapters 25 to 36 of `story.txt`) run like this. Pa goes to the crazy gambler in GamblinDen. The instance test line `story deals` sets the three crazy deals done: the variable that each `CrazyDeal.<n>.Var` names gets the value of `CrazyDeal.<n>.Tgt`, because the gambler offers a super deal only then. Pa talks to him and picks the options `Generics_CrazyGambler_MegaDeal_Yes_` and `Generics_CrazyGambler_MegaDeal_Start_` (the test line `use <player> <entity> <option>`). The story takes pa to the arena of `SuperDeal.Map` and the other members go to their leader. There every member boards a ship (test line `switch`) and fights the red ones (auto `1 red` for pa, `2 red` for the others), unless `AUTO_PRE_`, `AUTO_A_`, `AUTO_B_` or `AUTO_M_<deal>` say otherwise. The deal is won when `SuperDeal.Library` names the next deal (after the twelfth: `JackInTheBox.Active` is 2). Then the party is back in GamblinDen. The world keeps the instances of the arena and of GamblinDen up (`-keep`).

`chainenv.sh` holds the steps of the chapters of part 1 that the automatic player does not play by itself. MapDecont: the decontamination, the check-in and the fall to Fake. Fake: its scenes alone. RustyFirstContact: Rusty's talk, the keypad's question, the ship editor on pa's client and the platform. MeetBoss: pa possesses Berto, hides by the metro station until the alarm falls, then plays on as Berto. PoliceBossCheats: a policeman by the conversor and the photo of the boss cheating at poker. FunctionaryHunting: a stolen police ship to the meeting point. SheriffMayor: money for the police boss, as the earlier chains did. CatchTrurl: the three functionaries with Trurl's maps possessed, then D-Tritus again by the conversor. StealMayorShip: pa fights the police of the level in the mayor's ship, and the other members stay out of the fight because only the mayor's ship's kills count.

```
tools/test/mp2.sh coop <chapter> --points-dir DIR --mp2
```

This runs a chapter that way and runs `coopparity.py --mp2` at its start and at its end. With `--mp2` the gate also reads the audit lines of every game of the chapter (the logs of the instances and of the players) and fails on any of them.

`SCRAP_AUDIT_LOG=<key start>[,<key start>...]` in the environment of any game writes the first count of each audit key that starts with one of the given strings to the console as `audit: <key>`. The coop gate uses `player.world.norule,pres.unforwarded,pres.double`.

`tools/test/coop/story.txt` lists the chapters of the coop story from the new game to the credits, in the order of the campaign chain (100 chapters). A line has the mission, the level, the part (1 to 3), the story point of the points folder if one exists and the chapter name of `chapters.txt` if it is one of them. The three parts are the shares of the parallel agents of stage 5b: part 1 is chapters 1 to 36 (the new game to the end of the twelve super deals), part 2 is 37 to 68 (StupidMerc to GetAuthorization), part 3 is 69 to 100 (TalkToBoss to the credits).

```
python3 tools/test/parity/spchain.py --points-dir DIR [--parts 1,2,3] --out DIR --scratch DIR
```

`spchain.py` is the single player reference of the coop story. One rendered single player game loads the story point of each chapter of `story.txt` in order (the party's save variables, then the level, as `coopparity.py` does for its single player side). It records the level, `Mission.Library`, `MissionState`, the entity count and the script errors of each load, and ends with `Map.Credits` on the menu level. It writes `spchain.json` and `spchain.md` to `--out` and exits with 0 when every chapter with a story point was reached.

In coop the world's context of an instance is the party (`src/game/App/Party.c`, `include/scrap/party.h`). `data/coop/rules.def` can give a site another playerread rule in coop with a line `read <site> <rule> [<mission>]`. With a mission the line holds only while the story's `Mission.Library` is that mission. The file also holds the other rules of the coop story in one place: `SceneSkip`, the lists `local`, `hud`, `clientnot` and `entry` of the story's save variables, `pilot` (`first` or `each`: who gets the boarding callbacks), `failsecs`, `dealsecs` and `gather`.

In coop on `-mp2` every save variable is the party's story except those of the list `local` of `rules.def` (Map, Spawn, ComeFrom, PrevMap, Char, AcBatteries, Money, Alarm, AlarmActive, AlarmStatus, GameplayTime). The prefixes that a sandbox keeps per player (the PLAYER rows of `tools/script/netclass/savevar.tsv`: the hangar, the lives, the bets) are the party's in coop, so a member's ship editor changes the party's hangar.

A fixed camera that the scripts show in view 0 (a `Cam` entity that is no follow camera and no camera path, such as the arena camera of a super deal's score table) goes to the player as a camera path of two points at the camera's place that holds. When the scripts give view 0 its follow camera back after such a camera or after a camera path, the client's own camera comes back at once. In coop a fixed camera set in the world's context goes to every member. Before, such a camera counted as `pres.unforwarded:camera`.

The items of the world's scorer (made by the start of a level before any player is there, such as the `MissionTimer` of a race or the `Credits` of the intro) go to a player that joins, each with the last value of every variable that the scripts set on it. A client's own clear of its HUD (its `CharScorer.Create` or `RacerScorer.Create`) keeps them, the server's clear removes them. The coop stand-in of a level's start is made once per level and is inactive after the start (its fall at the spawn had been a member's second fall sound).

The test channel has verbs for this (`src/game/Global/TestChannel.c`). `entinfo` lists the entities around a place (class, list, model, action class, position, radar flag, and for a proxy the entity it stands for), `radar`, `fx` and `sounds` list what the last frame drew and played, `entsnd <object>` the sounds of one object, `sounds src <x> <y> <z> <radius>` the positional sound sources within the radius of a point (lines `ps <name> <sound> <loop> <x> <y> <z> <playing>` for the positional sets, which are the level's sound loops and interval sounds made by `MapSnd`, and `es <entity> <item> <sound> <loop> <x> <y> <z> <playing>` for the sound items of objects), `ents`, `menu` and `get` are the old ones. `iline <line>` sends a line to the standard input of the instance, and `channel.py <game folder> -i <level> ...` talks to an instance's own channel (it answers `test_cmd_<level>.txt` in the game folder), so a hosting game's children can be reached.

`audit` answers the counters of the network audit (`src/game/Global/NetAudit.c`, `include/scrap/netaudit.h`) since the last `audit reset`: the sound, voice and music sinks, effects, debris, animation events, HUD operations and texts, fade, cinema, camera, action set, time speed, the entity variables the scripts set, the reads of "the player" on an instance (a read in a player's context counts as `player.read.bound:<function>` or `player.any.bound:<function>`, a read in the world's context as `player.world.<rule>:<site>` with the rule of its row in the `playerread` table: nearest, any, party or world, and as `player.world.norule:<site>` for a site without a row, stage 1.3 gate: none on the 11 city levels with 8 bots) and the reads of the render flag and the camera distance on a headless process. One line per key, by count down. `audit json` gives the same as one line of JSON. The counters are always on and change no output.

The instance test lines for the viewer set (`src/game/World/Viewers.c`, `include/scrap/viewers.h`) are `viewers`, which lists the viewers (id, leader, pose, camera), `viewers fake <n> [seed]`, which adds n test viewers on nodes of the level's ground graph spread as far apart as the graph allows, each turning slowly (the load probe), `viewers clear` and `viewers share <units>`, which sets the share distance. `ticks` prints the tick count, p50, p99, max and mean tick time in ms since `ticks reset`, the dropped ticks, the viewers and the leaders. `traffic` prints the entity count and, per traffic list, the ships, the active ones and the quota, and `traffic cap <leaders> cheap <units> <slot>` sets the quota cap (0 none), the cheap distance (0 none) and the slot interval of cheap ships.

The audit of a `-mp2` client has more keys in the category `create`. `client.sim.call:<Module>.<func>` counts every call of a script module function tagged SIM in the module table, in `Port_Entry`, because the client runs no simulation script. `client.rail.call:` and `client.rail.create:` count the calls and creations of the rail traffic's modules (`Traffic`, `VehiclesNPC` and `InTraffic` and what they call) on the client. Since stage 2.2 that traffic runs only on the server, so both stay 0. `client.script.create:` counts an entity the client made itself. `mirror json` of the client also gives `simcalls`.

The module table of the netclass tables may have rows `Module.func` that tag one function of a mixed module (looked up before the module row, `NetClass_ModuleTag`). The initstep table has one more tag: GROUP (a group of steps whose own steps have rows). The rail traffic steps are tagged WORLD, so they are the server's. A `-mp2` client runs only the DEF and GROUP steps of the level start. Since stage 4.2 every one of the 28 world levels has a client part (`Lv_Client` in `src/game/Script/levels/Levels.h`), so nothing is purged any more (`client.purge:` stays 0). The levels whose Map is data (the arenas and the InitStatus of Fake) run their `map.def` sections with the definitions only (`Script_DefClient` in `include/scrap/script.h`): the `setvar` and `do` lines and the calls the netclass tables give the client run, and every other line is counted as `client.def.skip:`. The client log has the line `initsteps: Levels/<L>: the client's steps`.

```
python3 tools/test/parity/census.py --client <game> --instance <game> --level <Level> --sp <game> [--radius 15000] [--json FILE]
```

`census.py` needs three running games with the test channel: a joined client started with `-mp2`, its instance, and a single player game of the same level. It takes the client's view point from `mirror json` and asks all three for `sounds src`. It compares the positional sets of the client against single player (around the player and in the whole level), and the object sound items of the client against the instance (entities both have) and against single player (only for entities where the instance and single player agree, the others are entities of the moment such as a parked ship of another type, NPCs and traffic). Playing states are listed but not counted, because they depend on where each game's listener is. Rail traffic and the players' own entities are left out. The exit code is 0 without gaps. Stage 3.3 gate on DownTown and Police with one joined `-mp2` client: `client.sim.call`, `client.script.create` and `client.purge` are 0 and the census has 0 gaps.

```
python3 tools/test/parity/census.py --classes --client <game> --instance <game> --level <Level> [--sp <game>] [--interest 54000] [--json FILE] [--samples 3]
```

`census.py --classes` is the census by class (stage 4.1). It asks the instance and the client (and single player with `--sp`) for the test verbs `entinfo` and `mirror json`, `--samples` times, one second apart. It groups every entity by class and kind (the action class of a thing with life, else the model, else the name without digits). A gap is found in every sample. `missing` is an entity of the instance in the client's interest that the client has no entity of the same name and class of. The interest is every object, item, thing with life and plain entity of the level, and the ships, characters and missiles within `--interest` of the client's own entity. `state` is a replica that differs from the instance's entity in active, model, action class, team or radar. `extra` is an entity of the client that is no replica, but for its own entity and the cameras. The report has a table per class and kind with the counts on the instance (whole level), the instance inside the interest, the client and single player. The exit code is 0 without gaps. `mirror.py` allows the life of a burning entity (class block `burn` above 0) on the client to be as much as the server's or up to 5 points more (`BURN_LAG`), also for `prevlife`, because the client draws it about 100 ms behind.

The audit category `census` (all builds) has `snd.name:<sample>` for every sample played (the four play functions of the sound system) and `debris.name:<model>` for every debris system dropped. A `-mp2` instance also counts `snd.near:<sample>` and `debris.near:<model>` when the play or the drop is within the interest radius (60000 units) of a player's viewer. A play without a place counts as near. The weapon census compares these near keys of the instance with the name keys of the clients.

The instance has audit keys for cameras. `pres.unforwarded:camera:<calls>` names the innermost three calls of script functions that run when a camera is set, the caller first, joined with `>` (for example `Parking.UseShip>Scrap.SetCam`). A camera path of the scripts (a `CamPath`, the boarding and parking paths) counts as `pres.scene:camera:<calls>` instead, because those are scenes of stage 4.5. The camera that follows the player again at the end of the boarding and parking paths (`Parking.CharOnShip` and `EndPathCam`, like `Vehicles.SetActiveCam`) is not counted, because a `-mp2` client's camera follows its own entity itself. A `-mp2` client has two more keys in the category `create`. `client.damage:<weapon>` counts a hit of a weapon on the client, which does no damage there (the damage is the server's, only the own character's punch is the client's). `client.skip:<file>` counts a pilot animation the client did not load because its replica shows a stand-in model for a missing pilot.

The instance test line `goto <player> <entity> [<dx> <dz>]` levels a ship it places and stops it (no velocity, no rotation), so a weapon test can put a shooter's ship beside a target facing it. The weapon scenario of stage 4.1 is `tools/test/parity/weapons.sh fight <out> <weapon>...` (three weapons at a time, ports from `WEAP_PORT`, game folders in `WEAP_SCRATCH`). It runs a `-mp2` instance of DownTown, client A with `SCRAP_PLAYBOT=1 SCRAP_PLAYBOT_WEAPONS=<weapon> SCRAP_PLAYBOT_CM=1` (it fires the weapon and its countermeasure at the nearest other player) and client B with `SCRAP_PLAYBOT=target`. Both go into their own ships. Every tenth of the fight the instance puts A's ship 2500 units beside B's ship, facing it, and sets B's life to 1 (`life 0 1`), so the next hit kills. Then the scenario takes the mirror diff of both clients, the instance log lines `hit by player` and `player 0 died`, and the census against single player firing the same weapon. A gap is a name of the weapon that the instance played near a player and neither client played. Single player has the name when it fires, not in a control run without firing. `weapons.sh sp <out> <weapon|none>...` makes the single player side (`weapsp.py`, the control run is `none`) and `weapons.sh census <fight out> <sp out> <weapon>...` prints the gaps (`weapcensus.py`).

The UI transaction channel of a `-mp2` instance (stage 4.4, `include/scrap/uichan.h`) has the test verb `ui`: `ui` lists the counts of each interface (opens, transactions, refusals on the instance, opens and transactions on a client) and the interfaces open per player, `ui state [player]` the values the interfaces change as that player's context reads them (money, class, the hangar, the conversor and mission save variables, NewsPanel.ShowingNews), `ui call <player> Module.Func|arg|...` a call of the scripts in a player's context (an interface the scripts open for that player). Single player has the same verb, and the test verb `use <entity> [option]` makes player 0 use an entity through its arrow as the instance line `use` does. The stage's scenario scripts (the same steps on a single player game and on a `-mp2` instance with headless clients, then a compare of `ui state` and the screen) are not in the repository.

In stage 6a (the free activities: the hangar, Sputnik, ship building, pickups and lives), `ui state [player]` also prints, as the player's context reads them: `entity` (ship, char, other or none: the kind of the player's entity), `life` (its life), `pos` (its place), `Player.NumLives`, every other save variable of the prefixes `Hangar.` and `Parked.` that the earlier lines do not show, `Parking.PlayerShip` (the player's own ship: name, active flag and its ship text), `Sputnik.spt` (the player's own Sputnik: name and active flag) and `Sputnik.beside` (1 when both are shown and within 2000 units of each other). Single player answers the same lines. A `-mp2` client has two more audit keys in the category `create`: `client.ui.create:<type>:<name>` counts an entity the client made while an interface of the instance is shown on it (the hidden ships of the hangar that Sputnik's hangar screen and the ship editor make, as single player's do), and `client.ui.call:<Module>.<func>` counts a call of a function tagged SIM in that time (helpers of the simulation modules the interface calls for its own screen, such as `Parking.SetDtritusPlate` and `SaveGame.SaveMoney`). They are counted apart from `client.script.create:` and `client.sim.call:`. `mirror.py` does not count a readback of `OffMul` (the blend of a linked replica onto its link) when the replica's `LinkModel` names an entity the client has no replica of, such as the plate on the ship of another player outside the client's interest: the replica waits for its link. The stage's scenario scripts (the same steps on a single player game and on a `-mp2` pair, two-player scripts, world scripts with a coordinator) are not in the repository.

`SCRAP_PLAYER_TRACE=1` in the environment of an instance prints `pltrace` lines to its log. Each change of a player's money or alarm is printed with the context it happened in (`in player N` or `in the world`) and the dispatch point that entered that context. Each callback dispatched to a player prints `pltrace callback <function> for player N`. `pltrace WRONG` means a player's context changed another player's money or alarm. The frame loops (the entity updates, the input step, the alarm updates) print no callback lines.

Instances read test lines on their standard input (or through `iline`): `status`, `money`, `use`, `goto`, `switch`, `call`, `alarm`, `life`, `faction ...`, `arena ...`, `story ...`, and a few more for the coop story. A client plays by itself with `SCRAP_PLAYBOT` (modes such as `idle`, `use`, `face`, or `1` to fight) and sends faction requests with `SCRAP_PLAYBOT_FACTION=<s>:<ability>,...`. `SCRAP_NET2_LOSS`, `SCRAP_NET2_DELAY` and `SCRAP_NET2_JITTER` add packet loss, delay and jitter to the sends of the process.

`arrow <entity>` is another test line of the instances. It prints what the automatic player sees of an entity: its mission arrow, use arrow, life, radar mark, active flag, whether it is deleted, and the module of its use function. The automatic player takes a door that the mission made its target when the door's use function is the mission's (not a port of the doors' modules such as Elevator or Metro: GoToOutskirts' elevator to GamblinDen). It puts its ship at the nearest life box when the ship is under 35 percent of its most life, and it keeps a fighter that it places next to its target inside the box of the level's grid of points. A helper (auto 2) does not race. The test variables of the `-mp2` client work with the UI channel too: `SCRAP_COOP_ANSWER` answers a question, `SCRAP_COOP_SHIPEDIT` closes the ship editor with the hangar's spare ship (hangar 9, as the test of the old path did) and `SCRAP_COOP_SPUTNIK` closes Sputnik's hangar screen.

The audit key `pres.double:<name>` counts a sound record only when its place is within 120 units of the client's own entity, so two characters that fall side by side at the same moment are no double.

`SCRAP_TIMESCALE=<n>` (1 to 16) makes every clock of a game and of ScrapCoord run n times as fast as the real one: the game time, the instance's ticks, the frames of a headless client, the net timers (the delay of `SCRAP_NET2_DELAY` too), the waits between passes and the mixer (the sound device stays stopped). Rendered games present without waiting for the display. Give all processes of a network test the same n and they play their game time in 1/n of the real time, as long as each keeps up: the instance's status line counts dropped ticks, which must stay 0. DownTown with one fight bot keeps up at 16. The tools wait game time through `tools/test/gameclock.py` (parity.py, coopparity.py, the `wait` lines of coop/chap.sh). feel.py and the single player gates of mp2.py (regress, golden, levels) always run on the real clock, and the deterministic test mode ignores the variable.

The instance sends the engine variables its scripts set that `gvar.tsv` tags STATE or WORLD_SETTING, the character classes its scripts load (`Chars.LocalLoadRes`) and the world's music to its players, and again to each player that joins (the records `gvar`, `charres` and `music` of `include/scrap/pres.h`).

`tools/test/parity/classes.py` is the census by class of stage 4.2 on every world level. Per level it starts a headless `-mp2` instance (sandbox) and one joined headless `-mp2` client (`SCRAP_PLAYBOT`, idle by default) and fills one row of columns: the mirror differences (`mirror.py`), the class gaps (`census.py --classes`), the definitions (`defcmp.py`), the client's audit at its level start and after it (`client.sim.call`, `client.script.create`, `client.purge`, `client.noclass`), `pres.unforwarded` and `pres.double` of the instance, `pres.double` and `pres.missing` of the client, and the crashes (`FATAL ERROR`, AddressSanitizer, a signal; the known exit check `g_CountSprite2D == 0` is not counted). The exit code is 0 when every column is 0. It stops with an error when the instance has no server on its port.

```
python3 tools/test/parity/classes.py --out <dir> [--levels A,B] [--bot idle] [--jobs 4] [--port <base>] [--build build-linux64] [--scratch <dir>] [--settle 40] [--report-only]
```

Worker n of `--jobs` uses `--port` + n. `--settle` is game time (`SCRAP_TIMESCALE`). `--report-only` rebuilds the report from the files of an earlier run. For a run with AddressSanitizer pass `--build build-linux64-asan` and set `ASAN_OPTIONS=detect_leaks=0`.

`tools/test/parity/defcmp.py --client <game> --instance <game> --level <L>` reads the engine variables of `tools/script/netclass/gvar.tsv` tagged WORLD_SETTING, STATE or CLIENT_ONLY and the module variables of `Init` on a joined client and on its instance (test verbs `get` and `dump`). A difference is a definition the level's client part lacks. The `SKIP` pattern of the script lists what differs by design: the camera, the screen, the energy bar and the talk times of the own HUD, the autoplay volume the original resets on every channel it plays, the dynamic music, the talk line width and the map libraries a client imports without running them.

`tools/test/mp2.sh` has two gates for this. `mp2.sh classes [--levels A,B] [--bot idle] [--classes-jobs 4]` runs `classes.py` into `<out>/classes`. `mp2.sh weapons [--weapons Vulcan,Laser]` runs `weapons.sh fight`, `sp` (with the control run `none`) and `census` for each weapon (default all seven) on the real clock and prints one row per weapon: hits, deaths, the mirror differences of shooter and target and the census gaps. `mp2.sh all` runs both after parity.

`parity.py --mp2` takes the moment of a point from the instance, because single player runs its own simulation. The kinds of the moment (LOOSE: traffic, police, NPCs and so on) are compared against the instance's count at the point (within four fifths of the radius, since a ship at the edge moves out by the time a client is asked), the other kinds against the smaller of single player's and the instance's count (the free world leaves the story's things of a place out on purpose). A sound single player plays is a gap only when the instance played it near a player (census key `snd.near`); its sources near the point are the census column's, since an entity's loop starts by the listener's distance. A particle system is a gap only when the instance dropped it near a player (`debris.near`). A HUD item is a gap only when the instance's world scorer or the other client has it. A positional set is left to the census column, a radar icon to the mirror column (RadarShow, the arrow and its view cone of every replica). What this leaves out is listed per point in `report.md` as single player's moment and not counted.

The test verb `scene` plays the cutscenes (single player, instance and client). `scene list` prints the 35 cutscenes of the scene table: name, level, mission, the function that loads its classes and the actors. `scene play <name> [player]` clears the screen fade, loads the scene's classes with its load function when they are not there yet and plays the scene (`Scene.Play`). On an instance the optional player is the context the scene starts in, and in sandbox the scene then belongs to that player. `scene state` prints the scene that plays and the time of its scene entity's action. `scene skip [player]` asks to skip the scene. On a client it sends the skip to the server, on an instance it is the skip of player N (default 0), and in single player it ends the scene as Escape does.

`scene call <Module.Func|arg|...> [player]` imports a mission's module from the current level's pack (as the scene table loads a mission module) and calls the function. On an instance the call runs in the context of the given player (the player's entity is bound for the call). A name without `|` arguments is called directly. Example on Outskirts: `scene call Mission_IdentifyMerc.CreateLonelyMercProfiles`, then `scene call Mission_IdentifyMerc.InitIdentifyMerc 0`.

`SCRAP_SCENE_SHOTS=<dir>:<t>,<t>,...` in the environment of a single player game or a client takes an in-engine screenshot `<dir>/<scene>-<k>.pcx` when the action time of the scene entity reaches each t (in seconds). `f<x>` means that part of the scene's length, so `f0.5` is the middle. A camera path on view 0 (boarding or parking a ship) is shot the same way as `Path<n>-<k>.pcx`, with n counting the paths of the run. The time is the scene's own, so single player and a client are compared at the same scene time whatever their clocks do. The shots are taken at the exact scene time also on the fast clock (`SCRAP_TIMESCALE=4`). In single player and for camera paths the frame before the shot ends at the shot's time (the game clock takes the rest in the next frame). On a client the frame of the shot draws the server's scene at that time (found from the replica's samples), and the frame that takes the shot runs with no game time gone. So the scene gates run with `SCRAP_TIMESCALE=4`.

`SCRAP_TEST_SAVEVARS=<name>=<value>[;...]` sets save variables at the start of every level, before the module Init runs. It is for scenes whose level state depends on the story. HumanCD, for example, needs `GameAct=2ndMurder` (the mental controller of the press). When it sets `Char`, that character's library (`Chars/<Char>.pyc` and its sound library) is preloaded too, as an outdoor level preloads only the player's character. GDB's scene EnterMatrix needs `Char=Bishop`. The scene table also preloads the character libraries a scene's loader needs (EnterMatrix: Bishop) on every side. Give the variable to every game of a test alike.

```
python3 tools/test/parity/scenes.py sp <game> <scene>...
python3 tools/test/parity/scenes.py net <instance game> <level> <player|all> --clients <game>,<game> [--out DIR] <scene>...
python3 tools/test/parity/scenecmp.py <single player shots> <client shots>... [--mad M] [--bad B] [--sheet out.png]
```

`scenes.py sp` plays the scenes one after the other in single player and waits for each end. `scenes.py net` does the same on a `-mp2` instance and writes, per scene, a JSON file with the instance's audit keys `pres.unforwarded` and `pres.double` and the scene keys, each client's `pres.double`, `pres.missing`, `client.fail` and scene keys, and each client's mirror diff (`mirror.py`) after the scene. Both wait until no scene plays before they start the next one (a level that plays its own scene at the start, such as ABIndoor's TheEndA, first). `scenecmp.py` compares the screenshots of the same names. It takes the mean absolute difference of the RGB values at 320x180 (0 to 255) and the share of pixels that differ by more than 48 in a channel. A shot passes when mad is at most M (default 12) and bad is at most B (default 0.12), and a missing shot fails.

The audit keys of the scenes are in the category `hud`. An instance has `scene.start:<name>`, `scene.end:<name>`, `scene.skip:<name>` and `scene.skip.refused`. A client has `scene.client:<name>`, `scene.load:<name>` (the client loaded the scene's classes itself) and `pres.missing:scene:<name>` (the client has no tables of that scene). In the category `camera`, `scene.path` on the instance counts a camera path sent to a player and `scene.client.path:<name>` counts it on the client. Also in the category `camera`, `scene.camvar:<var>` on the instance counts a setting of a player's follow camera sent to its client, and `scene.client.camvar:<var>` counts it on the client. A camera target the client lacks for 2 s counts `pres.missing:camvar:<name>` (category sound, as the other `pres.missing` keys).

Scene records to a scene's audience carry the server time (the record `PRES_O_TIME` comes first in the message). The client plays them when its drawn server time reaches that time and advances their fades and cinema bars by the lag. A player that joins during a scene gets the scene's last fade as far as it is.

`SCRAP_PRES_TRACE=1` makes the instance print every presentation record it sends (`pres: <op> to <mask> at <server time> from <calls>`) and a client every record it plays (`pres: <op> (server time <drawn time>, lag <s>)`, and the values of camera records).

The test channel verb `ent <entity> <var>...` takes these pseudo variables besides the entity's own: `@render` (the model's drawn place), `@model` (the model's position), `@link` (the entity its model is linked to), `@shown` (1 when the model is loaded and drawn) and `@addr` (the entity's address, for a debugger).

In coop the skipping follows the rule `SceneSkip` of `data/coop/rules.def`: `all` (every member who sees the scene asked, the default), `any`, `host` (player 0 alone) or `none`. In sandbox and free roam the scene's own player ends it alone.

What the multiplayer tests did not cover is listed in [multiplayer.md](multiplayer.md).

## Retired difftest

The old difftest (i686 build against a frozen reference build, with the test module SDTest) is retired. The tag `difftest-final` keeps the last state with it. No test uses a reference build any more. The scrap MCP server can still start the reference exe for visual comparisons (`exe=ref`, screenshots only, no test channel).
