#!/bin/bash
# The weapon scenario of the multiplayer rewrite (plan mp2, stage 4.1): every weapon fired by a test bot on a -mp2
# client against another player, with the mirror diff of both clients and the sound and effect census against single
# player.
#
#   weapons.sh fight <out> <weapon>...    per weapon a -mp2 instance of DownTown, client A (SCRAP_PLAYBOT=1 with the
#                                         weapon and its countermeasure) and client B (SCRAP_PLAYBOT=target), headless;
#                                         both go into their own ships, then every tenth of the fight the instance puts
#                                         A's ship level beside B's (test line goto) and sets B's life to 1, so the
#                                         next hit kills; mirror.py of A and B, the audits of the three, the hits
#                                         ("hit by player") and deaths ("player 0 died") of the instance's log after the
#                                         fight began: <out>/<weapon>.summary.txt. Three weapons at a time.
#   weapons.sh sp <out> <weapon|none>...  single player of DownTown (rendered, deterministic) per weapon: weapsp.py
#                                         boards the player's ship and fires the weapon (none: the control run)
#   weapons.sh census <fight out> <sp out> <weapon>...   weapcensus.py: the gaps of the census
#
# Weapons: Vulcan Laser Devastator Tesla ATPC Swarm Inferno. Environment: SCRAP_BUILD (default build-linux64),
# WEAP_SCRATCH (game folders, default <build>/weapons), WEAP_PORT (first port, default 30120, three are used),
# WEAP_SECS (the fight, default 60), SCRAP_VK_DEVICE (single player). No core files (killed games).
root=$(cd "$(dirname "$0")/../../.." && pwd)
build=$(realpath -m "${SCRAP_BUILD:-$root/build-linux64}")
G=$(realpath -m "${WEAP_SCRATCH:-$build/weapons}")
secs=${WEAP_SECS:-60}
level=DownTown
ulimit -c 0
ch() { timeout 60 python3 "$root/tools/test/channel.py" "$@"; }
pause() { timeout "$1" tail -f /dev/null; true; }

# a game folder with the build in it
game() {
	[ -d "$1/Bin" ] || "$root/tools/test/mkgame.sh" --native "$1" > /dev/null 2>&1 || return 1
	cp -a "$build/ScrapC" "$1/Bin/" && cp -a "$build"/ffmpeg/install/lib/lib*.so* "$1/Bin/"
	rm -f "$1"/test_*.txt "$1"/instance_*.quit
}

# fight <worker> <weapon> <out>
fight() {
	local w=$1 weap=$2 out=$3 port=$((${WEAP_PORT:-30120} + $1))
	local gi=$G/i-$w ga=$G/a-$w gb=$G/b-$w fifo=$G/i-$w.in log=$out/$weap
	local env=(env -u DISPLAY -u WAYLAND_DISPLAY SCRAP_NO_MSGBOX=1 PULSE_SERVER=unix:/nonexistent
		PIPEWIRE_REMOTE=/nonexistent SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy SCRAP_TEST_CHANNEL=1)
	local pi pa pb
	for g in "$gi" "$ga" "$gb"; do
		game "$g" || { echo "$weap: no game folder $g"; return 1; }
	done
	rm -f "$fifo"
	mkfifo "$fifo"
	(cd "$gi/Bin" && exec "${env[@]}" XDG_DATA_HOME="$gi/xdg-data" timeout -k 3 $((secs + 400)) ./ScrapC \
		-instance:$level -port:$port -bind:127.0.0.1 -id:$level -mode:sandbox -mp2 0<> "$fifo" > "$log.i.log" 2>&1) &
	pi=$!
	timeout 120 bash -c "until timeout 5 python3 '$root/tools/test/channel.py' '$gi' -i $level 'audit reset' \
		> /dev/null 2>&1; do timeout 1 tail -f /dev/null; true; done" || { echo "$weap: no instance"; kill $pi; return 1; }
	(cd "$gb/Bin" && exec "${env[@]}" XDG_DATA_HOME="$gb/xdg-data" SCRAP_PLAYBOT=target timeout -k 3 $((secs + 350)) \
		./ScrapC -play:127.0.0.1 -port:$port -imap:Levels/$level -headless -name:target -mp2 < /dev/null \
		> "$log.b.log" 2>&1) &
	pb=$!
	timeout 150 bash -c "until grep -q 'level running' '$log.b.log'; do timeout 1 tail -f /dev/null; true; done"
	(cd "$ga/Bin" && exec "${env[@]}" XDG_DATA_HOME="$ga/xdg-data" SCRAP_PLAYBOT=1 SCRAP_PLAYBOT_WEAPONS=$weap \
		SCRAP_PLAYBOT_CM=1 timeout -k 3 $((secs + 300)) ./ScrapC -play:127.0.0.1 -port:$port -imap:Levels/$level \
		-headless -name:shooter -mp2 < /dev/null > "$log.a.log" 2>&1) &
	pa=$!
	timeout 150 bash -c "until grep -q 'level running' '$log.a.log'; do timeout 1 tail -f /dev/null; true; done"
	pause 5
	# B (player 0) and A (player 1) into their own ships (a player's ship has every weapon bay)
	echo "goto 0 PlayerShip 200 0" > "$fifo"
	pause 2
	echo "switch 0" > "$fifo"
	pause 4
	local aown
	aown=$(grep -o "player 1's own ship [^ ]*" "$log.i.log" | tail -1 | awk '{print $5}')
	echo "goto 1 ${aown:-PlayerShip} 200 0" > "$fifo"
	pause 2
	echo "switch 1" > "$fifo"
	timeout 40 bash -c "until grep -q 'player 1 plays as .*a ship\\|player 1 into ' '$log.i.log'; do \
		timeout 1 tail -f /dev/null; true; done" || echo "$weap: the shooter has no ship"
	ch "$gi" -i $level 'audit reset' > /dev/null
	ch "$ga" 'audit reset' > /dev/null
	ch "$gb" 'audit reset' > /dev/null
	# the fight's hits and deaths are the log lines after this mark
	echo status > "$fifo"
	pause 1
	local mark bship off k
	mark=$(wc -l < "$log.i.log")
	bship=$(grep -o 'player 0 into [^ ]*' "$log.i.log" | tail -1 | awk '{print $4}')
	for k in 1 2 3 4 5 6; do
		case $((k % 4)) in 0) off="2500 0";; 1) off="0 2500";; 2) off="-2500 0";; 3) off="0 -2500";; esac
		echo "goto 1 ${bship:-Parked} $off" > "$fifo"
		echo "life 0 1" > "$fifo"
		pause $((secs / 10))
	done
	pause $((secs - secs * 6 / 10 - 2))
	python3 "$root/tools/test/parity/mirror.py" --game "$ga" --instance "$gi" --level $level --json "$log.mirror-a.json" \
		--show 40 > "$log.mirror-a.txt" 2>&1
	python3 "$root/tools/test/parity/mirror.py" --game "$gb" --instance "$gi" --level $level --json "$log.mirror-b.json" \
		--show 40 > "$log.mirror-b.txt" 2>&1
	ch "$gi" -i $level 'audit json' > "$log.iaudit.json"
	ch "$ga" 'audit json' > "$log.aaudit.json"
	ch "$gb" 'audit json' > "$log.baudit.json"
	echo status > "$fifo"
	pause 3
	kill $pa $pb 2>/dev/null
	echo quit > "$fifo" &
	timeout 15 tail --pid=$pi -f /dev/null 2>/dev/null || kill -9 $pi 2>/dev/null
	rm -f "$fifo"
	local hits died
	hits=$(tail -n +"$mark" "$log.i.log" | grep -c "hit by player")
	died=$(tail -n +"$mark" "$log.i.log" | grep -c "player 0 died")
	{
		echo "== $weap"
		echo "instance: $hits hits by players in the fight, $died deaths of the target"
		head -1 "$log.mirror-a.txt"
		head -1 "$log.mirror-b.txt"
	} | tee "$log.summary.txt"
}

cmd=$1 out=$2
shift 2
mkdir -p "$out" "$G"
out=$(realpath "$out")
case "$cmd" in
fight)
	while [ $# -gt 0 ]; do
		pids=()
		for w in 0 1 2; do
			[ $# -gt 0 ] || break
			fight $w "$1" "$out" > "$out/$1.run.log" 2>&1 &
			pids+=($!)
			shift
		done
		wait "${pids[@]}"
	done
	cat "$out"/*.summary.txt
	;;
sp)
	for weap in "$@"; do
		g=$G/sp
		game "$g" || exit 1
		SCRAP_BUILD=$build SCRAP_GAME_DIR=$g SCRAP_TEST_DETERMINISTIC=1 SCRAP_TEST_CHANNEL=1 HEADLESS_TIMEOUT=300 \
			NATIVE_LOG=$out/$weap-sp.log setsid "$root/tools/test/native.sh" -imap:Levels/$level > /dev/null 2>&1 &
		pid=$!
		if timeout 150 bash -c "until timeout 5 python3 '$root/tools/test/channel.py' '$g' 'audit reset' > /dev/null \
			2>&1; do timeout 1 tail -f /dev/null; true; done"; then
			timeout 240 python3 "$root/tools/test/parity/weapsp.py" "$g" "$weap" "$out/$weap.json" 30
			ch "$g" "con quit" > /dev/null 2>&1
		else
			echo "$weap: single player does not answer"
		fi
		timeout 15 tail --pid=$pid -f /dev/null 2>/dev/null || kill $pid 2>/dev/null
	done
	;;
census)
	exec python3 "$root/tools/test/parity/weapcensus.py" "$out" "$@"
	;;
*)
	sed -n '2,22p' "$0"
	exit 2
	;;
esac
