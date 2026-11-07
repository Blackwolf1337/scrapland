# rd/regress: regression runner for the script ports

Runs the case files (rd/fc-*.txt) listed in cases.txt on the C build and compares every case line with stored pack
results made on the same engine (SCRAP_PACK_SCRIPTS=1). One command, pass or fail per case, exit code 0 when all pass.

    cd rd/regress
    ./regress.py check --build /home/claude/projects/scrapland/build-linux64          # all cases, lanes ca ca2 ca3
    ./regress.py check --build <worktree>/build-linux64 fc-r6b-cm3.txt r7c            # some cases (name or substring)
    ./regress.py show results/<stamp> fc-r6b-cm3.txt                                   # every differing line of a case
    ./regress.py compare results/<stamp>                                               # judge stored C results again
    ./regress.py baseline --build <build> [cases]          # pack runs 1 and 2 into baselines/ (lanes pa pa2 pa3)
    ./regress.py baseline --index 2 --lanes ca,ca2,ca3 ... # a run of its own number on other lanes (in parallel)
    ./regress.py baseline --add [cases]                    # one more pack run (p3, ...) for cases that vary rarely
    ./regress.py accept results/<stamp> <case> ...         # accept a C run's looked-at differences for the cases

The last line of a check is the summary: `regress: N/M pass, K fail, T min (failing cases)`. Results and
results/<stamp>/report.txt (every failing case with its first differing lines) stay in results/.

## What makes it stable

- Slot reset (slot.py) before every case, pack and C alike: the engine configuration xdg-data/.../Scrap.cfg comes from
  template/Scrap.cfg (9 hangar ships, iHangarShip 3, PlayerName Tester, ServerType DeathMatch, ServerMapList ['FZ'],
  Fraglimit 20), no save files, every file a case or tool left in the game folder removed (fst.py, nmbots.pyc, logs,
  cores, ...), Bin, commonappdata and the game folder Scrap.cfg restored. The comment at the top of slot.py lists all
  state a slot can carry.
- The intro video MSE.vds is left out of the slot's Video folder (the engine skips a missing intro video): a game is
  ready 6 s after its start instead of 38 s.
- GPU: the games render on the AMD 610M by default (SCRAP_VK_DEVICE=1002:164e); REGRESS_VK_DEVICE=10de:27b0 puts them on the RTX 4000 (shared with the ollama LXC, only with the user's OK).
  With the other agents' games on the 610M the world clock ran at 0.3 to 0.4 of real time there (slow frames, CApp
  caps a frame at 0.1 s of game time); on the RTX it runs at 1.0. Needs about 300 MB of VRAM per game.
- `!sleep N` waits N seconds of game time, not real time (so machine load does not change what a case sees). A level
  load during the wait is seen by a mark in the game's globals (a load starts Python over); the load counts as 20 game
  seconds (LOAD_CREDIT; the real-time sleeps of the cases covered the load), the new level's time counts from its
  start. While the world clock stands (a menu stopped it) real seconds count.
- Case lines time out after 30 s; two timeouts in a row end the case (status hang); a game that ends is status crash.
- Cases marked `port` in cases.txt start a real server: only one of them runs at a time.

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
- baselines/<case>.accepted.json (regress.py accept): a C run whose differences were looked at and accepted; a line
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

baselines/<case>.p<k>.json (and .txt in the func6.py format): pack runs on the build named in the file. Remake them
after an engine change that the pack side sees too (they are pack results on the C engine): `baseline` and
`baseline --index 2 --lanes ca,ca2,ca3` at the same time, then `baseline --index 3` (three runs catch most of the
pack's own variation; masks/ covers the rest). Old baselines go to old-<commit>/ (their times still order the
lanes, longest case first).

## Cases

cases.txt: `<Level> <case> [port]`; the cases left out and why are listed at its end. A case that must be cut to be
stable gets a copy in cases/ (fc-weap-rg.txt: fc-weap.txt without the real weapon re-inits that hit Model.c:48 at
random).

## Notes

- One run of all 295 cases with 3 lanes took 73 min (2026-11-06, load average about 13 from the other agents' games;
  about 214 lane minutes, mostly the game-time waits of the mission cases). Four lanes (`--lanes ca,ca2,ca3,cax`) bring
  it to about 55 min. The waits are game time, so a slow machine makes the run longer, not the results different.
- Ctrl-C or SIGTERM to regress.py also ends the lane games. A game left over (a killed runner) is found with
  `pgrep -x ScrapC` and readlink /proc/<pid>/exe (the slot path); kill it by pid.
- Slots: pack pa pa2 pa3, C ca ca2 ca3, ASan cax (`check --asan --lanes cax`).

## Test channel (2026-11-07, branch nopy-6)

`REGRESS_HARNESS=channel` runs the games with the test channel of the game (scrap_mcp 0.3.0 or newer, the case lines
through its `py` command, answers also while the world clock stands); `REGRESS_MCP_DIR=<checkout>/tools/mcp` takes
scrap_mcp.py from another checkout (game data and slots stay those of the main checkout). Default: the old python
harness (mcpgame.py) and the main checkout's scrap_mcp.py, as before.
