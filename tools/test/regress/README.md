# tools/test/regress: regression runner for the script ports

Runs the case files (cases/fc-*.txt) listed in cases.txt on the C build and compares every case line with stored pack
results made on the same engine (SCRAP_PACK_SCRIPTS=1). One command, pass or fail per case, exit code 0 when all pass.

    cd tools/test/regress
    ./regress.py check --build ../../../build-linux64          # all cases, lanes ca ca2 ca3 (default build: <repo>/build-linux64)
    ./regress.py check --build <build> fc-r6b-cm3.txt r7c      # some cases (name or substring)
    ./regress.py show <repo>/build-regress/<stamp> fc-r6b-cm3.txt   # every differing line of a case
    ./regress.py compare <repo>/build-regress/<stamp>          # judge stored C results again
    ./regress.py baseline --build <build> [cases]          # pack runs 1 and 2 into baselines/ (lanes pa pa2 pa3; default build: <repo>/build-py64, the reference build with the interpreter)
    ./regress.py baseline --index 2 --lanes ca,ca2,ca3 ... # a run of its own number on other lanes (in parallel)
    ./regress.py baseline --add [cases]                    # one more pack run (p3, ...) for cases that vary rarely
    ./regress.py accept <repo>/build-regress/<stamp> <case> ...   # accept a C run's looked-at differences for the cases

The last line of a check is the summary: `regress: N/M pass, K fail, T min (failing cases)`. Results and
<repo>/build-regress/<stamp>/report.txt (every failing case with its first differing lines) go to build-regress/
(gitignored).

## Layout

- cases/: all case files. cases.txt lists 294 cases. fc-r6-pyc is left out: it compares extracted pack .py sources
  with the shipped .pyc and needs game data.
- helpers/: the Python helpers (fst.py, fsd.py, ...) that the python harness copies into the game folder. Case lines
  load them by name, e.g. execfile('fsd.py').
- channel/: the channel twins <helper>.tc: the same helper functions written as test channel statements (lambdas over
  the tc_ functions of src/game/Global/TestExpr.c). One statement per line, indented lines continue the line before,
  `include <name>` pulls another twin in.
- masks/, template/Scrap.cfg, baselines/.
- tochannel.py, normalise.py, compare.py, slot.py, and the triage helpers classify.py, diffsum.py, probe.py.

Game data and slots come from SCRAP_MCP_ROOT. Default is this checkout, or the main checkout of a git worktree when
the worktree has no game/ folder (found with `git rev-parse --git-common-dir`). scrap_mcp.py is the one of this
checkout (tools/mcp); REGRESS_MCP_DIR takes the tools/mcp of another checkout.

## What makes it stable

- Slot reset (slot.py) before every case, pack and C alike: the engine configuration xdg-data/.../Scrap.cfg comes from
  template/Scrap.cfg (9 hangar ships, iHangarShip 3, PlayerName Tester, ServerType DeathMatch, ServerMapList ['FZ'],
  Fraglimit 20), no save files, every file a case or tool left in the game folder removed (fst.py, nmbots.pyc, logs,
  cores, ...), Bin, commonappdata and the game folder Scrap.cfg restored. The comment at the top of slot.py lists all
  state a slot can carry.
- The intro video MSE.vds is left out of the slot's Video folder (the engine skips a missing intro video): a game is
  ready 6 s after its start instead of 38 s.
- GPU: the games render on the AMD 610M by default (SCRAP_VK_DEVICE=1002:164e); REGRESS_VK_DEVICE=10de:27b0 puts them
  on the RTX 4000 (shared with the ollama LXC, only with the user's OK). With the other agents' games on the 610M the
  world clock ran at 0.3 to 0.4 of real time there (slow frames, CApp caps a frame at 0.1 s of game time); on the RTX
  it runs at 1.0. Needs about 300 MB of VRAM per game.
- `!sleep N` waits N seconds of game time, not real time (so machine load does not change what a case sees). A level
  load is seen in the game output (wine.log of the slot: the engine prints "Loading level at" and "Level loaded in",
  App.c). When a load started during the case line before the sleep or during the sleep, the wait ends when the new
  level's own time (Scrap.GetTime(), it starts at 0 with the level) reaches N minus LOAD_CREDIT (20 s), however long
  the load took in real time. Real seconds count only while the new level answers with its clock standing. Before
  2026-11-07 the real seconds of a slow load counted too: on the 610M shared with other agents' games a load took 40 s
  instead of 5, so the read after `!sleep 45` came at the new level's start. The mark in the game's globals remains
  for a load the output does not show. While the world clock stands (a menu stopped it) real seconds count, as before.
- Case lines time out after 30 s; two timeouts in a row end the case (status hang); a game that ends is status crash.
  A command waits longer than the timeout while a level load runs or ended less than 15 s ago (LOAD_START), at most
  240 s more (LOAD_WAIT): Scrap.LoadLevel loads at once and the answer comes after the load.
- Cases marked `port` in cases.txt start a real server: only one of them runs at a time. The lock file is
  build-mcp/.regress-port.lock next to the slots (of SCRAP_MCP_ROOT), so the runs of all checkouts that share the game
  data take turns. A lock per checkout once let another worktree's run start its server at the same time
  (SNet.InitServer answered 0).

## Harness

With REGRESS_HARNESS unset, the runner takes the channel harness for a build without the interpreter (the default
build) and the python harness for a build with it. REGRESS_HARNESS=python runs the old game side, mcpgame.py, with
-PythonExecute. On a build without the interpreter it is refused with a message. REGRESS_HARNESS=channel runs
the game with the test channel and no Python code at all. tochannel.py turns each case line into the channel commands
of one command file (run in one frame, as the Python line was):

- the channel twins of the helpers the line loads (execfile, or exec open ... in module),
- channel twins of the exec strings and of two def lines of the cases (tables EXEC and LINES in tochannel.py),
- then the line itself, which the channel reads as it is (its expression language covers Python's expression syntax,
  lambdas and simple statements).

`./tochannel.py --stats` lists lines that would not convert (currently none). The reply of a converted line is the
first error of the helper commands, else the answer of the line.

Pacing: on the channel every command file is sent with pace=0.1 in its header. The game runs it only after 0.1 s of
game time since its previous answer, as mcpgame.py polled every 0.1 s of game time. The pack baselines saw that
spacing; without it NPC talks were seen a step early. While the world clock runs, the game answers the channel in
cWorld_Update right after the scheduled functions of the frame (where the old harness's script function ran as a
scheduled function). It answers at the start of the frame only when the clock stands (menus) or before the first world
update.

Timing-sensitive lines (NPC actions, AI states, door and meteor timers, the menu cursor) can still differ between a
channel run and a python run. Both harnesses show a few such flaky cases per full run, more under heavy machine load.
Rerun failing cases before treating them as port differences.

Other variables:

- REGRESS_PY_PROFILE=1: the games run with the Python profile (SCRAP_PY_PROFILE). Each result JSON keeps a
  "pyprofile" text of the Python code objects that ran in that game.
- REGRESS_PACK_SRC=<root of the extracted packs>: for pack runs only, the python harness copies
  Data.packed/scripts/net/ndeathmatchbots.py from that folder into the game folder, and the helper _nbots of
  fc-nm-bots compiles it (the packs have no .pyc of NDeathMatchBots). C runs make the module from
  data/modules/ndeathmatchbots.mod (written by hand from that source), so fc-nm-bots runs on both harnesses without
  it.

## Comparison (compare.py, normalise.py)

- normalise.py: one function, norm(), with numbered and documented rules (R1 to R15): function type 'py'/'C',
  heap addresses, arity/keyword/argument type error texts, level path case, C function reprs and type names,
  calls a C port makes directly that a case recorder cannot see (alarm and LaunchTaunt RemoteComunication,
  ScheduledVoiceCallback, PlayVoiceScheduled, Scrap.Rand records), keyword dict order, entity numbers ('Parked190'),
  the random coin/box drop of a dying ship, idle animation names, 'Module.Func' callback names, NPC answer lines.
- compare.py, per case line: all pack runs equal after norm(): the C reply must be equal (floats within 0.1 + 1 %).
  Pack runs differ but have the same token structure: the differing places are wildcards (number or any), the rest
  must match. Pack runs differ in structure (random list lengths): the line is masked and only has to agree on
  being an error or not.
  Script_EvalValue (the C eval of the game's texts) raises SyntaxError('invalid syntax') for texts it cannot read; the
  pack's parser gives the 1.5.2 message with position, e.g. "('unexpected EOF while parsing', (None, 1, 8, 'bad
  list'))". Two names or numbers with blanks between them get the parser's message in C too, other forms differ
  (fc-misth2, fc-nm-po, fc-nm-fl, fc-r8r-sg).
- baselines/<case>.accepted.xz (regress.py accept): a C run whose differences were looked at and accepted; a line
  equal to it (same case line, same normalised reply) passes. Used for the deleted-handle probes fc-nc-* (the C
  raises at other points than the script, the open items of round 8n, e.g. OutMap.GetNearestFreeSpawnPosReal) and
  fc-r8r-mi / fc-r8r-video (explained in round 8r: MenuMissionInfo with an unknown page, the new Display entry).
  Remake after a fix that changes such a line.
- masks/<case>: `ignore <text of the case line>` and `sub <regex> => <text>`, each with a # reason. They cover lines
  that depend on randomness the case cannot control in C (the recorders replace Scrap.Rand, C ports call
  Scrap_RandRange directly), NPC and AI timing, and accepted differences named in the round reports.
- Triage helpers: diffsum.py RESULTDIR (each differing line cut around its difference), classify.py RESULTDIR ...
  (per failing line: C varies between C runs, pack varies, or STABLE DIFF = a candidate port difference),
  probe.py <slot> <Level> <case> <pack 0/1> (one run, replies printed).
- A baseline is stale when the case file, a helper it execfiles or fst.py changed after the pack runs (shown in the
  report): run `baseline` again for that case.

## Baselines

baselines/<case>.xz holds the three pack runs of a case as one xz-compressed JSON list (game log dropped, about 1 MB
for all cases). Accepted C runs are baselines/<case>.accepted.xz. The baseline command writes loose
baselines/<case>.p<k>.json during its run and folds them into the .xz at its end (file lock, so a parallel
`baseline --index 2` is fine).

The runs are pack runs (SCRAP_PACK_SCRIPTS=1, Python) on the build named in the file, so they can only be remade with
the reference build (build-py64, configured with -DSCRAP_NO_PYTHON=OFF). `baseline` is refused on a build without the
interpreter. After step B of the Python removal they are fixed reference data. Remake them after an
engine change that the pack side sees too (they are pack results on the C engine): `baseline` and
`baseline --index 2 --lanes ca,ca2,ca3` at the same time, then `baseline --index 3` (three runs catch most of the
pack's own variation; masks/ covers the rest). Old baselines are kept in git history only. Set REGRESS_PACK_SRC for
baseline runs (fc-nm-bots needs the source on the pack side).

The baselines were remade on 2026-11-07 at script-loader 250d6e41 (three pack runs per case; fc-r6-am has a fourth run
because Mayor_Default was missing in run 3).

To check whether a difference between old and new pack runs is an engine change, run the old build (its commit built
in a scratch folder) with the current runner on a slot and compare. If it gives the old values the engine changed. If
it gives the new ones the cause is the runner or timing.

## Cases

cases.txt: `<Level> <case> [port]`; the cases left out and why are listed at its end. A case that must be cut to be
stable gets a copy in cases/ (fc-weap-rg.txt: fc-weap.txt without the real weapon re-inits that hit Model.c:48 at
random).

## Notes

- One run of all 295 cases with 3 lanes took 73 min (2026-11-06, load average about 13 from the other agents' games;
  about 214 lane minutes, mostly the game-time waits of the mission cases). Four lanes (`--lanes ca,ca2,ca3,cax`) bring
  it to about 55 min. The waits are game time, so a slow machine makes the run longer, not the results different.
  On 2026-11-07, with the 610M shared with the other agents' games (world clock about 0.3 of real time), one pack run
  of all cases took about 3 h with 3 lanes, and the C runs of both harnesses side by side (3 lanes each) took 213 and
  215 min.
- Ctrl-C or SIGTERM to regress.py also ends the lane games. A game left over (a killed runner) is found with
  `pgrep -x ScrapC` and readlink /proc/<pid>/exe (the slot path); kill it by pid.
- Slots: C lanes ca ca2 ca3 (default for check), pack lanes pa pa2 pa3 (default for baseline), ASan cax
  (`check --asan --lanes cax`).
