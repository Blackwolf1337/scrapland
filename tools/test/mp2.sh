#!/bin/bash
# The gates of the multiplayer rewrite (plan mp2): one table per gate, the numbers as JSON next to it, an exit code
# (0 pass, 1 fail, 2 a gate that could not run). Everything is in tools/test/parity/mp2.py (its --help).
#
#   tools/test/mp2.sh parity [--levels A,B:dm] [--points N] [--mp2]   parity per level and route (--mp2: replication
#                                                                    v2 with the mirror diff and the sound census)
#   tools/test/mp2.sh feel [--cells 0,50,150] [--sides join,host]   key to frame latency, jitter, corrections
#   tools/test/mp2.sh coop <chapter> --points-dir DIR          a coop chapter (tools/test/coop/chapters.txt) and coopparity
#   tools/test/mp2.sh sp [--levels-time 30]                    regress.py check, golden.sh, levels.sh
#   tools/test/mp2.sh classes [--levels A,B] [--bot idle]      census by class per world level (mirror, classes, the
#                                                              level start's definitions, audits, crashes)
#   tools/test/mp2.sh weapons [--weapons Vulcan,Laser]         every weapon: fight, single player, sound census
#   tools/test/mp2.sh all <chapter> --points-dir DIR           all of the above and a summary
#   tools/test/mp2.sh diff <old out> <new out>                 two runs against the run-to-run noise
# common: --out DIR (default build-linux64/mp2), --build DIR, --port BASE (40 ports from BASE), --scratch DIR, --device
# no core files of the games (killed rendered games leave NVIDIA cores of ~100 MB each)
ulimit -c 0
exec python3 "$(dirname "$(readlink -f "$0")")/parity/mp2.py" "$@"
