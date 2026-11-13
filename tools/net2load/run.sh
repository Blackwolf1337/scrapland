#!/usr/bin/env bash
# The standard net2load matrix: a fresh reference server per run, the fake clients against it, one CSV line per
# run (results.csv) and the logs of both sides in OUT.
#   tools/net2load/run.sh BUILD_DIR OUT_DIR [SECONDS] ["64 128 256"] ["0 2 10"] [extra client options...]
# Loopback only: server 127.0.0.1:29300, clients 127.10.0.1.. port 29301.
set -euo pipefail

bin=$1/net2load
out=$2
secs=${3:-30}
counts=${4:-"64 128 256"}
losses=${5:-"0 2 10"}
shift $(( $# < 5 ? $# : 5 ))
mkdir -p "$out"

for n in $counts; do
	for loss in $losses; do
		label="c${n}-l${loss}"
		"$bin" server --exit-empty --seconds $((secs + 120)) --interval 5 >"$out/$label.srv.log" 2>&1 &
		srv=$!
		# the server is up when it printed its address
		for _ in $(seq 50); do
			grep -q listening "$out/$label.srv.log" && break
			sleep 0.1
		done
		"$bin" client --clients "$n" --loss "$loss" --seconds "$secs" --interval 5 --csv "$out/results.csv" \
			--label "$label" "$@" >"$out/$label.cli.log" 2>&1 || echo "$label: client failures (see log)"
		# exit-empty ends the server once every client is gone; a hung one is stopped
		for _ in $(seq 100); do
			kill -0 "$srv" 2>/dev/null || break
			sleep 0.1
		done
		kill "$srv" 2>/dev/null || true
		wait "$srv" || true
		grep -E '^(==|connect|rtt|lost|per client|reliable|server)' "$out/$label.cli.log" || true
	done
done
