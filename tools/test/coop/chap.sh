#!/bin/bash
# The coop story further from a story point with two headless bots: chap.sh <tag> <port> <data folder> <steps file>
#   the world (ScrapCoord -mode:coop, tools/test/coop/world.sh) starts with a copy of the data folder (a saved world:
#   party.story, players/), the players pa and pb join with the identity keys of that data (COOP_KEYS/pa.key, pb.key;
#   default the folder keys next to the data folder); each line of the steps file:
#   "<log>|<line to the world, - for none>|<regex>|<secs>|<what>"
#   <log>: w (the world), pa, pb, or a level (its instance log); in the line $a and $b are the indexes of pa and pb in
#   the level of the log (the level is the line's "inst <Level>" when the log is w, pa or pb); "#" lines are comments;
#   the regex is looked for in the lines the log gets after the step starts ("(any time)" in <what>: the whole log)
# a line "poll <line>" is sent every 5 s until its pattern comes (a story variable that changes later); "%" in a
# pattern stands for the alternation "|"; each step with a pattern prints "ok: <what>" or "FAIL: <what> (...)"
# env: COOP_SCRATCH (tools/test/coop/env.sh), BUILD, CHAP_ENV (more env of both players), CHAP_ABORT (a regex of the
# polled log that fails the chapter at once), CHAP_KEEP=1 keeps the world running at the end, CHAP_RENDER=1 pa
# rendered ("shot <name>" lines take screenshots into <scratch>/shots); "save <name>" lines keep the world's data as
# <scratch>/<name>-data; CHAP_REARM, CHAP_REARM_LINES: test lines sent again after a pattern of the polled log (the
# level starts again, a death);
# CHAP_WORLD_ARGS more arguments of the coordinator; COOP_MP2=1: the second multiplayer design (-mp2 for the instances and both players)
. "$(dirname "$(readlink -f "$0")")/env.sh"
tag=$1 port=$2 data=$3 steps=$4
B=${BUILD:-$R/build-linux64}
L=$S/$tag.log D=$S/$tag-data
fail=0
v2=; [ -n "$COOP_MP2" ] && v2=-mp2
echo $$ > $S/$tag.script.pid
log() { case $1 in w) echo $L;; pa|pb|p[0-9]) echo $S/$tag-$1.log;; *) echo $D/logs/$1.log;; esac; }
# CHAP_MORE=<n>: more headless players p3..p<n> (new players of the party) after pa and pb; CHAP_PLAYERS=1: no pb
more=""; for ((k = 3; k <= ${CHAP_MORE:-0}; k++)); do more="$more p$k"; done
# a line matching pat in file after its first mark lines within secs
waitnew() {
	local end=$((SECONDS + $3))
	while [ $SECONDS -lt $end ]; do
		tail -n +$(($4 + 1)) "$1" 2>/dev/null | grep -qE "$2" && return 0
		timeout 0.5 tail -f /dev/null
	done
	return 1
}
idx() { grep -oE "player [0-9]+ is $2 " "$D/logs/$1.log" 2>/dev/null | tail -1 | awk '{print $2}'; }
player() {
	local n=$1; shift
	[ -f "$keys/$n.key" ] && set -- SCRAP_PLAYER_KEY="$keys/$n.key" "$@"
	if [ -n "$CHAP_RENDER" ] && [ $n = pa ]; then
		env PLAYER_NAME=$n WATCH_SECS=${CHAP_WATCH:-6000} SCRAP_BUILD=$B SCRAP_COOP_ANSWER=no SCRAP_COOP_SHIPEDIT=6 $CHAP_ENV "$@" "$T/join.sh" "$tag-$n" "$port" $v2 > /dev/null
	else
		env PLAYER_NAME=$n SCRAP_COOP_ANSWER=no SCRAP_COOP_SHIPEDIT=6 $CHAP_ENV "$@" "$T/hjoin.sh" "$tag-$n" "$port" "$B" $v2 > /dev/null
	fi
}
"$T/stopw.sh" $tag $tag-pa $tag-pb ${more//p/$tag-p}
# the players of the story points: their identity keys (the game reads SCRAP_PLAYER_KEY instead of its own)
keys=${COOP_KEYS:-$(dirname "$(readlink -f "$data")")/keys}
rm -f $L $S/$tag-pa.log $S/$tag-pb.log
rm -rf $D && cp -a "$data" $D && rm -f $D/logs/*.log || exit 1
WORLD_KEEP_DATA=1 "$T/world.sh" $tag $port "$B" -mode:coop -stopinstances -grace:20 ${v2:+-instargs:$v2} $CHAP_WORLD_ARGS > /dev/null
"$T/wait.sh" $L "coordinator .*listening|instance .* up" 60
player pa
"$T/wait.sh" $L "pa .* is in " 150 || echo "FAIL: pa joins"
# CHAP_PLAYERS=1: pa alone
if [ "${CHAP_PLAYERS:-2}" != 1 ]; then
	player pb
	"$T/wait.sh" $L "pb .* is in " 150 || echo "FAIL: pb joins"
fi
for n in $more; do rm -f $S/$tag-$n.log; rm -rf $S/g-$tag-$n; player $n; "$T/wait.sh" $L "$n .* is in " 150 || echo "FAIL: $n joins"; done
grep -oE "p[ab] \([0-9a-f]+\) is in [A-Za-z]+" $L | tail -2
while IFS='|' read -r lg line pat secs what; do
	[ -z "$lg" ] || [ "${lg:0:1}" = "#" ] && continue
	lv=$lg
	if [[ "$line" =~ ^inst\ ([A-Za-z]+) ]]; then
		[[ $lg = w || $lg = pa || $lg = pb ]] && lv=${BASH_REMATCH[1]}
	fi
	a=$(idx "$lv" pa) b=$(idx "$lv" pb)
	line=${line//\$a/$a} line=${line//\$b/$b} pat=${pat//\$a/$a} pat=${pat//\$b/$b}
	# "%" in a pattern is the alternation "|" (the fields are split at "|")
	pat=${pat//%/|}
	for n in $more; do x=$(idx "$lv" $n); line=${line//\$$n/$x} pat=${pat//\$$n/$x}; done
	f=$(log "$lg")
	mark=$(wc -l < "$f" 2>/dev/null || echo 0)
	[[ $what == *"(any time)"* ]] && mark=0
	if [[ $line == wait\ * ]]; then
		# seconds of game time (SCRAP_TIMESCALE, tools/test/gameclock.py)
		timeout "$(awk -v s="${line#wait }" -v n="${SCRAP_TIMESCALE:-1}" 'BEGIN { n = int(n); if (n < 1) n = 1; if (n > 16) n = 16; printf "%.2f", s / n }')" tail -f /dev/null
	elif [[ $line == save\ * ]]; then
		# a story point: the world's data now (the coordinator saves first; its logs left out) as <name>-data
		m0=$(wc -l < $L); "$T/send.sh" $tag save; waitnew $L "coordinator: saved" 10 "$m0"
		rm -rf "$S/${line#save }-data" && cp -a $D "$S/${line#save }-data" && rm -f "$S/${line#save }-data"/logs/*.log "$S/${line#save }-data/instances.txt" &&
			echo "saved ${line#save }"
	elif [[ $line == rejoin\ * ]]; then
		# a player whose client left (the story's end) joins again
		n=${line#rejoin }; "$T/waitpid.sh" "$(cat $S/$tag-$n.pid)" 30; mark=$(wc -l < "$f" 2>/dev/null || echo 0); player $n
	elif [[ $line == shot\ * ]]; then
		# CHAP_RENDER=1: pa is rendered, a screenshot to <scratch>/shots
		mkdir -p "$S/shots"
		[ -n "$CHAP_RENDER" ] && "$T/shot.sh" "g-$tag-pa" "$S/shots/${line#shot }.png" > /dev/null &&
			echo "shot ${line#shot }"
	elif [[ $line == poll\ * ]]; then
		# the line again every 5 s until the pattern comes (a story variable that changes later)
		end=$((SECONDS + ${secs:-30})) got= rearm=$mark
		while [ $SECONDS -lt $end ]; do
			"$T/send.sh" $tag "${line#poll }"
			waitnew "$f" "$pat" 5 "$mark" && { got=1; break; }
			# CHAP_REARM: a pattern of the polled log after which the instance starts again without the test lines (a
			# super deal's next round or its retry loads its level again): the lines of CHAP_REARM_LINES (";" between
			# them, $a, $b, $p3.. the members) are sent again once the members are back
			if [ -n "$CHAP_REARM" ] && hit=$(tail -n +$((rearm + 1)) "$f" | grep -E "$CHAP_REARM"); then
				rearm=$(wc -l < "$f")
				# (a level that starts again: once pa is back in it)
				if grep -q "level again" <<< "$hit"; then
					waitnew "$f" "player [0-9]+ is pa " 60 "$rearm"
				fi
				timeout "$(awk -v n="${SCRAP_TIMESCALE:-1}" 'BEGIN { printf "%.2f", 10 / (n < 1 ? 1 : n) }')" tail -f /dev/null
				a=$(idx "$lv" pa) b=$(idx "$lv" pb)
				IFS=';' read -ra rl <<< "$CHAP_REARM_LINES"
				for r in "${rl[@]}"; do
					r=${r//\$a/$a} r=${r//\$b/$b}
					for n in $more; do x=$(idx "$lv" $n); r=${r//\$$n/$x}; done
					"$T/send.sh" $tag "$r"
				done
				echo "again: the test lines after the level started again"
			fi
			# CHAP_ABORT: a pattern that ends the chapter at once (the mission failed: the instance restarted without the
			# test lines of the steps)
			if [ -n "$CHAP_ABORT" ] && tail -n +$((mark + 1)) "$f" | grep -qE "$CHAP_ABORT"; then
				echo "FAIL: $what (aborted: '$CHAP_ABORT' in $(basename "$f"))"; fail=$((fail + 1)); break 2
			fi
		done
		if [ -n "$got" ]; then echo "ok: $what"; else echo "FAIL: $what (no '$pat' in $(basename "$f"))"; fail=$((fail + 1)); fi
		continue
	elif [ "$line" != "-" ]; then
		"$T/send.sh" $tag "$line"
	fi
	[ -z "$pat" ] && continue
	if waitnew "$f" "$pat" "${secs:-30}" "$mark"; then echo "ok: $what"; else echo "FAIL: $what (no '$pat' in $(basename "$f"))"; fail=$((fail + 1)); fi
done < "$steps"
"$T/send.sh" $tag "status"; timeout 2 tail -f /dev/null
[ -n "$CHAP_KEEP" ] || "$T/stopw.sh" $tag $tag-pa $tag-pb ${more//p/$tag-p}
echo "chap $tag: $fail failed"
