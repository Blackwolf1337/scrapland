# shellcheck shell=bash
# Sourced by the coop chain tools of this folder: T this folder, R the repository, S the scratch (COOP_SCRATCH:
# worlds, players, logs and the saved story points go there).
T=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
R=$(cd "$T/../../.." && pwd)
S=${COOP_SCRATCH:?set COOP_SCRATCH to the scratch folder of the coop tools}
mkdir -p "$S"
# no core files of the games (killed rendered games leave NVIDIA cores of ~100 MB each)
ulimit -c 0
