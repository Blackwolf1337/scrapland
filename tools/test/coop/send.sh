#!/bin/bash
# a line to the stdin fifo of a world (never blocks when nobody reads it): send.sh <tag> <line...>
. "$(dirname "$(readlink -f "$0")")/env.sh"
tag=$1; shift
timeout 2 bash -c "echo \"$*\" > $S/$tag.in"
