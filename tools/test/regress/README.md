# tools/test/regress: regression runner for the script ports

Runs the case files (cases/fc-*.txt) listed in cases.txt on the C build and compares every case line with stored pack
results made on the same engine (SCRAP_PACK_SCRIPTS=1). One command, pass or fail per case, exit code 0 when all pass.

    cd tools/test/regress
    ./regress.py check --build ../../../build-linux64          # all cases, lanes ca ca2 ca3 (default build: <repo>/build-linux64)
    ./regress.py check --build <build> fc-r6b-cm3.txt r7c      # some cases (name or substring)
    ./regress.py show <repo>/build-regress/<stamp> fc-r6b-cm3.txt   # every differing line of a case
    ./regress.py compare <repo>/build-regress/<stamp>          # judge stored C results again
    ./regress.py accept <repo>/build-regress/<stamp> <case> ...   # accept a C run's looked-at differences for the cases

The last line of a check is the summary: `regress: N/M pass, K fail, T min (failing cases)`. Results and
<repo>/build-regress/<stamp>/report.txt (every failing case with its first differing lines) go to build-regress/
(gitignored).

## Layout

- cases/: all case files. cases.txt lists 294 cases. fc-r6-pyc is left out: it compares extracted pack .py sources
  with the shipped .pyc and needs game data.
- helpers/: the Python helpers of the pack runs (fst.py, fsd.py, ...), kept because the baselines were made with them
  and their hash is part of the case hash. The channel runs load the twins in channel/.
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
- GPU: the default device of the runner is still the AMD 610M (SCRAP_VK_DEVICE=1002:164e). Since 2026-11-07 the test
  games run on the RTX 4000 (the user's choice): set REGRESS_VK_DEVICE=10de:27b0 and SCRAP_VK_DEVICE=10de:27b0 for
  every run. The stored baselines are RTX runs. ASan runs stay on the AMD (ASan crashes inside the NVIDIA driver):
  `REGRESS_VK_DEVICE=1002:164e SCRAP_VK_DEVICE=1002:164e ./regress.py check --asan --lanes cax ...`. With the other
  agents' games on the shared 610M the world clock ran at 0.3 to 0.4 of real time (slow frames, CApp caps a frame at
  0.1 s of game time); on the RTX it runs at 1.0. Needs about 300 MB of VRAM per game.
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

## Deterministic mode

By default the runner starts the games in the deterministic test mode of the game (include/scrap/testdet.h,
SCRAP_TEST_DETERMINISTIC=1) with the test channel in step mode (SCRAP_TEST_CHANNEL=step). Two runs of a case on the same
build then give the same replies, byte for byte, on any slot and under any machine load. A difference between two
builds is then a difference of the builds, no reruns needed. Without the variable the game runs exactly as before.

    ./regress.py check --live ...            # off for this run
    REGRESS_DETERMINISTIC=0 ...              # off for everything that uses the runner (also the scratch tools)

What the mode does:

- Random numbers: the generator of the game (the CRT rand behind Scrap.Rand and all C sites) gets seed 1 at the start
  and at the end of every level load.
- Clock: Plat_Ticks and Plat_Counter, the source of all time in the game, give a clock that goes on by 1/60 s per pass
  of the main loop. The real clock is not read. A level load takes no time on it. SCRAP_TEST_DETERMINISTIC=<n> with
  n > 1 gives n steps a second.
- Frames are presented without waiting for the display, so game time runs faster than real time (about 1.8 times in
  DownTown on the RTX).
- Sound: the mixer runs on the mode's clock in the main thread, not on the device thread. A voice ends after the same
  number of frames in every run.
- Addresses: on Linux the game starts itself again without address space randomization. Heap addresses in replies
  ("<X instance at 5555583f2978>") are replaced by the number of their first appearance in the replies of the run
  ("at 000000000003>"), because the heap still differs a little from run to run.
- Channel: in step mode the clock stands while no command file waits and runs only for the pace of the next one.
  The pace keeps the rules of a live run (game time since the last answer, at once after a level load or when the
  game clock stood 60 frames), counted once per clock step.
- The paces are the means of what live runs give (measured with a case of Scrap.GetTime() lines and sleeps, constants
  DET_PACE, DET_SLEEP, DET_START in regress.py), so the lines land where the baselines saw them: a case line runs 7
  steps (0.117 s) after the answer before, the first line 0.33 s into the level.
- `!sleep N` is one command file with pace N + 0.44 and `credit=20` in its header (a live wait takes about 0.45 s
  longer than N). The credit makes it a wait: the game counts game time plus the time the game clock stood (after its
  first 3 s), and after a level load since the last answer the time of the new level plus the credit (LOAD_CREDIT),
  as the runner does in a live run.
- The question at the start (Scrap.GetLevelPath()) is sent without a pace, so asking it again moves no clock.
- The two loops of the net code that wait for the goodbye of a closing client or server make a clock step per pass.
- The pass of the main loop that loads a level makes no step, whether the next command file is there already or not.
- Mouse motion from before the first step does not count (the pointer entering the new window comes as one or two
  events, before or after the focus).

A build from before the mode (no SCRAP_TEST_DETERMINISTIC string in the binary) runs live, and the result file says
`"det": false`. golden.sh and levels.sh set SCRAP_TEST_DETERMINISTIC=1 too (SCRAP_TEST_DETERMINISTIC=0 switches it
off). They do not use step mode, the game runs on. The scrap MCP launch has the option `deterministic`.

When two runs of one build still differ, start both with SCRAP_TEST_DET_TRACE=<file> (an absolute path, one file per
game) and compare the files with `./dettrace.py <A> <B> <build>/ScrapC`. The trace has a line per clock step, per
crt_rand call (with its caller), per window or input event, per command file and per level load. The first differing
line names the frame and the code where the runs part. This is how the level load race and the mouse motion were
found.

The stored pack baselines are live runs. Lines that depend on chance or on the exact moment (an NPC action, an AI
state, a menu animation) fail in a live run now and then. In a deterministic run a fixed set of them fails, the same
in every run, and a rerun does not clear it. To tell such a line from a port difference run the case with `--live` a
few times, or compare the raw replies with a deterministic run of the base build: equal replies mean the builds
behave the same. Any change that draws one random number more or less moves all later draws, so this set changes
with such a batch.

## Notes

The case fc-r8j-snd-DownTown passes `Scrap.Set` on. A recorder that swallowed it made the rail traffic load a model that is in no pack. `native ports` also prints the lines `modules pack/none/absent/other/added/made/loaded` and `def calls registry/byname/other/errors`. `rd/msw-run.py` is gone with the interpreter, and mission starts are compared through the test channel.

## Harness

The game runs with the test channel and no Python code at all. tochannel.py turns each case line into the channel
commands of one command file (run in one frame, as the Python line was):

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

Timing-sensitive lines (NPC actions, AI states, door and meteor timers, the menu cursor) can still differ between two
runs. A full run shows a few such flaky cases, more under heavy machine load.
Rerun failing cases before treating them as port differences.

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
  Script_EvalValue (the C eval of the game's texts): a text that is a bare name or a name chain evaluates as eval does
  (its value, or NameError). Two names or numbers with blanks between them raise the 1.5.2 parser's SyntaxError with
  its message and position, e.g. "('unexpected EOF while parsing', (None, 1, 8, 'bad list'))". Native_EvalText, the
  eval of the ports on native C values, gives the same errors (both call Script_EvalOther). Other texts the reader
  cannot take raise the plain 'invalid syntax', where the pack's parser gives "('unexpected EOF while parsing', (None,
  1, col, text))": eval('('), eval(''), eval('(1,') and exec('res.zz = 1 +') in fc-misth2, fc-nm-po, fc-nm-fl and
  fc-r8r-sg. The game never evals such texts and the parser goes away at stage 7, so these four cases have accepted C
  runs, with the reason in a comment above the line in the case file.
- baselines/<case>.accepted.xz (regress.py accept): a C run whose differences were looked at and accepted; a line
  equal to it (same case line, same normalised reply) passes. Used for the deleted-handle probes fc-nc-* (the C
  raises at other points than the script, the open items of round 8n, e.g. OutMap.GetNearestFreeSpawnPosReal) and
  fc-r8r-mi / fc-r8r-video (explained in round 8r: MenuMissionInfo with an unknown page, the new Display entry) and
  fc-misth2, fc-nm-po, fc-nm-fl and fc-r8r-sg (the parser messages above). Remake after a fix that changes such a
  line. A comment added to a case file changes its hash, so the stored pack runs of these four cases got the new hash
  without a rerun (only comment lines changed).
- masks/<case>: `ignore <text of the case line>` and `sub <regex> => <text>`, each with a # reason. They cover lines
  that depend on randomness the case cannot control in C (the recorders replace Scrap.Rand, C ports call
  Scrap_RandRange directly), NPC and AI timing, and accepted differences named in the round reports.
- Triage helpers: diffsum.py RESULTDIR (each differing line cut around its difference), classify.py RESULTDIR ...
  (per failing line: C varies between C runs, pack varies, or STABLE DIFF = a candidate port difference),
  probe.py <slot> <Level> <case> [build] (one run, replies printed).
- A baseline is stale when the case file, a helper it execfiles or fst.py changed after the pack runs (shown in the
  report): the baselines cannot be remade any more, so a stale note means the case or a helper changed after the pack
  runs. Revert the change, or check the C run and accept it (`regress.py accept`).

## Baselines

baselines/<case>.xz holds the three pack runs of a case as one xz-compressed JSON list (game log dropped, about 1 MB
for all cases). Accepted C runs are baselines/<case>.accepted.xz.

There is no baseline command. The baselines (three pack runs per case, made on 2026-11-07 on the RTX 4000 at
script-loader 50259f8a) are frozen reference data since step B of the Python removal. The tag `interp-final` has the
runner that made them. pack_runs still reads a loose baselines/<case>.p<k>.json if one is there. With the world clock
at real time the three runs agree on more lines than the 610M runs did, so lines that vary with game time or chance
(NPC turns, AI states, respawns, random draws) fail in C more often and need a mask in masks/.

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
- RTX run times of 2026-11-07: with both harnesses side by side (7 games), a channel run of all 294 cases took 51 min
  on 4 lanes and 56 min on 3 lanes, a python run 43 min on 4 lanes, and pack run 3 of 245 cases 52 min on 3 lanes.
- Three times on 2026-11-07 a DownTown game on the RTX just ended with no ASan report (pack run 3 of fc-r7c-dt1 during
  the DownTown load, pack run 3 and one C channel run of fc-r8j-snd-DownTown at SebastianSound.SetSounds). 12 reruns
  of fc-r8j-snd-DownTown passed. Rerun a case that ends this way before treating it as a port difference.
- Ctrl-C or SIGTERM to regress.py also ends the lane games. A game left over (a killed runner) is found with
  `pgrep -x ScrapC` and readlink /proc/<pid>/exe (the slot path); kill it by pid.
- Slots: C lanes ca ca2 ca3 (default for check), ASan cax (`check --asan --lanes cax`).
