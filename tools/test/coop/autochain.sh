#!/bin/bash
# The coop story chapter after chapter from a story point: autochain.sh <tag> <port> <first data dir> [chapters]
# each chapter: the mission and its level from the data's party.story, the party's level from pa's player data; pa
# travels there (test line travel, none when it is there), pb goes to its leader, both play the mission's targets
# (auto, auto 2) until Mission.Library is another mission (any of the scripts' missions), then the world's data is
# saved as <tag>-<n>-data and the next chapter starts from it. Env as chap.sh (BUILD, CHAP_ENV, CHAP_SECS: the
# limit of a chapter, default 1500; AUTO_ENV_<Mission>: more env of the players for that mission, e.g. the conversor;
# AUTO_A_<Mission>, AUTO_B_<Mission>: the arguments of pa's and pb's test line auto, default "" and "2", e.g. "1 @Traffic"
# where the mission's targets are any ships, Massacre; AUTO_PRE_<Mission>: instance lines before the auto lines,
# separated by ";", e.g. "switch \$a;switch \$b": the super deal starts when the player boards a ship; "wait <secs>"
# waits there). The tools of this folder come from the M7k scenario tools (scrapland-notes mp-tools/m7k), with the
# scratch from COOP_SCRATCH (env.sh) and the identity keys from COOP_KEYS (chap.sh).
. "$(dirname "$(readlink -f "$0")")/env.sh"
tag=$1 port=$2 data=$3 max=${4:-99}
echo $$ > $S/$tag.chain.pid
levels="Orbit Fake Junkyard AB ABIndoor QF OM FZ CA DownTown Outskirts Police Bank Press Temple TownHall GamblinDen Commercial Scrapyard GDB"
missions=$(grep -ohE '"Mission_[A-Za-z0-9]+"' $R/src/game/Script/ModuleFiles.inc | tr -d '"' | sort -u)
lvname() { for l in $levels; do [ "${l,,}" = "${1,,}" ] && { echo $l; return; }; done; echo "$1"; }
val() { grep -a "^$2=" "$1" | head -1 | cut -d= -f2-; }
# CHAP_PLAYERS: the members: 1 (pa alone), 2 (pa and pb, the default) or more (pa, pb and p3.. as chap.sh's CHAP_MORE:
# they go to their leader and help in the fights as pb does). CHAP_LEVELS=auto: each chapter's world hosts only the
# levels of the chapter (where the party is, the mission's level, the next chapter's level of story.txt; Orbit is always
# added) with its instances from <port>+1 on
np=${CHAP_PLAYERS:-2}
[ "$np" -gt 2 ] && export CHAP_MORE=$np
export CHAP_PLAYERS=$np
mp=""; for ((k = 3; k <= np; k++)); do mp="$mp p$k"; done
story=$T/story.txt
nextlv() { awk -F'|' -v m="$1" '!/^#/ && f { print $3; exit } !/^#/ && $2 == m { f = 1 }' "$story"; }
nextmis() { awk -F'|' -v m="$1" '!/^#/ && f { print $2; exit } !/^#/ && $2 == m { f = 1 }' "$story"; }
worldargs=$CHAP_WORLD_ARGS
n=0
while [ $n -lt $max ]; do
	n=$((n + 1))
	if [ -f "$data/party.story" ]; then
		mis=$(val "$data/party.story" Mission.Library)
		lv=$(lvname "$(val "$data/party.story" Mission.Map)")
		owner=$(cut -d' ' -f1 "$data/party.owner")
		from=$(lvname "$(val "$(ls $data/players/$owner*.dat | head -1)" level)")
	else
		# a new world (an empty data folder): the new game in Orbit, the story's first chapter
		mis=$(awk -F'|' '!/^#/ { print $2; exit }' "$story") lv=Orbit from=Orbit
	fi
	# CHAP_FIRST=<k>: the rounds are the chapters k, k+1, ... of story.txt: the mission and its level from there (a
	# chapter that is no mission of the scripts: Fake; a super deal, while the story's mission waits for its end)
	if [ -n "$CHAP_FIRST" ]; then
		ch=$(awk -F'|' -v k=$((CHAP_FIRST + n - 1)) '!/^#/ && $1 == k { print $2 "|" $3 }' "$story")
		if [ -n "$ch" ] && [ "${ch%%|*}" != "$mis" ]; then
			echo "chapter $((CHAP_FIRST + n - 1)): ${ch%%|*} in ${ch#*|} (the story's mission: ${mis:-none})"
			# only a chapter that is no mission of the story (Fake, a super deal) or the story's first one differs:
			# else the chapter before did not end the way the story goes on
			if [[ ${ch%%|*} == Mission_* ]] && [[ $mis == Mission_* ]]; then
				echo "autochain $tag: stopped, the story is at $mis, not at ${ch%%|*} (chapter $((CHAP_FIRST + n - 1)))"
				exit 1
			fi
			mis=${ch%%|*} lv=${ch#*|}
		fi
	fi
	others=$(echo "$missions" | grep -vx "$mis" | paste -sd'%')
	# a super deal (SuperDeal_*): pa asks the crazy gambler for it in his level (the crazy deals it needs are set done by
	# the test line "story deals"), the party fights in the arena of SuperDeal.Map, the deal is won when the story names
	# the next one (the last one: JackInTheBox.Active 2), then the party is back with the gambler
	arena= endpoll=
	if [[ $mis == SuperDeal_* ]]; then
		arena=$(lvname "$(val "$data/party.story" SuperDeal.Map)")
		nd=$(nextmis "$mis")
		if [[ $nd == SuperDeal_* ]]; then endpoll="story get SuperDeal.Library|SuperDeal.Library = $nd"
		else endpoll="story get JackInTheBox.Active|JackInTheBox.Active = 2"; fi
	fi
	if [ "$CHAP_LEVELS" = auto ]; then
		ls=$(printf '%s\n' "$from" "$lv" "$arena" "$(nextlv "$mis")" | grep -v '^$' | grep -vx Orbit | awk '!s[$0]++' | paste -sd,)
		export CHAP_WORLD_ARGS="$worldargs -instport:$((port + 1)) -levels:${ls:-Orbit}${arena:+ -keep:$arena,$lv}"
	fi
	steps=$S/$tag-$n.steps
	{
		echo "# autochain $tag $n: $from -> $lv, $mis"
		echo "$from|wait 10||||"
		if [ "$from" != "$lv" ]; then
			echo "w|inst $from travel \$a $lv|pa .* is in $lv|150|pa travels to $lv (any time)"
			[ "$np" -ge 2 ] && echo "w|inst $from leader \$b|pb .* is in $lv|150|pb goes to its leader in $lv (any time)"
			for m in $mp; do echo "w|inst $from leader \$$m|$m .* is in $lv|150|$m goes to its leader in $lv (any time)"; done
		fi
		echo "$lv|wait 15||||"
		if [ -n "$arena" ]; then
			g=CrazyGambler_Default
			echo "$lv|inst $lv story deals|crazy deal 3|10|the crazy deals done (test)"
			echo "$lv|inst $lv goto \$a $g 0 250||||"
			echo "$lv|wait 2||||"
			echo "$lv|inst $lv use \$a $g Talk|player \$a uses $g|10|pa talks to the crazy gambler"
			echo "$lv|wait 15||||"
			echo "$lv|inst $lv use \$a $g Generics_CrazyGambler_MegaDeal_Yes_|player \$a uses $g .Generics_CrazyGambler_MegaDeal_Yes_|10|pa asks for the super deal"
			echo "$lv|wait 15||||"
			echo "$lv|inst $lv use \$a $g Generics_CrazyGambler_MegaDeal_Start_|player \$a uses $g .Generics_CrazyGambler_MegaDeal_Start_|10|pa starts the super deal"
			echo "w|-|pa .* is in $arena|300|the super deal takes pa to $arena (any time)"
			# the members go to their leader (the story takes the mission's member alone: the arena is no chapter
			# level of the coordinator)
			[ "$np" -ge 2 ] && echo "w|inst $lv leader \$b|pb .* is in $arena|150|pb goes to its leader in $arena (any time)"
			for m in $mp; do echo "w|inst $lv leader \$$m|$m .* is in $arena|150|$m goes to its leader in $arena (any time)"; done
			back=$lv lv=$arena
			echo "$lv|wait 10||||"
			# the deal's fight starts when the player boards a ship (as the crazy gambler's combats), the members fight
			# the red ones: AUTO_PRE/A/B/M_<deal> of chainenv.sh replace these
			v=AUTO_PRE_$mis; [ -n "${!v}" ] || { l='switch $a'; [ "$np" -ge 2 ] && l="$l;switch \$b"; for m in $mp; do l="$l;switch \$$m"; done; printf -v "$v" '%s' "$l"; }
			v=AUTO_A_$mis; [ -n "${!v}" ] || printf -v "$v" '%s' "1 red"
			v=AUTO_B_$mis; [ -n "${!v}" ] || printf -v "$v" '%s' "2 red"
			v=AUTO_M_$mis; [ -n "${!v}" ] || printf -v "$v" '%s' "2 red"
		fi
		pre=AUTO_PRE_$mis
		if [ -n "${!pre}" ]; then
			IFS=';' read -ra lines <<< "${!pre}"
			for l in "${lines[@]}"; do
				# "conv": pa into the conversor, on as soon as its new character is there (a player leaves the machine at
				# once: a sentinel sent to watch it finds one that stays)
				if [[ $l == conv ]]; then echo "${CONV_A//@/$lv}" | awk -v RS=';;' '{ sub(/^\n+/, ""); sub(/\n+$/, ""); if (length) print }'
				elif [[ $l == wait\ * ]]; then echo "$lv|$l||||"; else echo "$lv|inst $lv $l||||"; echo "$lv|wait 5||||"; fi
			done
		fi
		va=AUTO_A_$mis vb=AUTO_B_$mis
		# AUTO_A_<Mission>="-": pa plays nothing by itself (the steps of AUTO_MID play the chapter: the intro's levels)
		[ "${!va}" = "-" ] || echo "$lv|inst $lv auto \$a ${!va}|player \$a plays the mission's targets|5|pa plays the mission's targets"
		# AUTO_B_<Mission>="-": pb plays nothing (its helper's fights would draw the alarm: TalkToMayor's robbery)
		if [ "$np" -ge 2 ] && [ "${!vb}" != "-" ]; then
			echo "$lv|inst $lv auto \$b ${!vb:-2}|player \$b plays the mission's targets|5|pb helps in the fights"
			# the more members help in the fights (AUTO_M_<Mission>: their arguments of auto, default "2")
			vm=AUTO_M_$mis
			for m in $mp; do echo "$lv|inst $lv auto \$$m ${!vm:-2}|player \$$m plays the mission's targets|5|$m helps in the fights"; done
		fi
		echo "pa|shot ac-$mis-pa||||"
		# AUTO_MID_<Mission>: more steps after the auto lines, separated by ";;" ("@|" and " @ " stand for the level), e.g. a poll of
		# a step of the mission, then the conversor again (BeatPickus: a functionary for Traitorius, Dtritus for the thugs)
		mid=AUTO_MID_$mis
		if [ -n "${!mid}" ]; then
			v=${!mid}; v=${v//@\|/$lv\|}; v=${v// @ / $lv }
			awk -v RS=';;' '{ sub(/^\n+/, ""); sub(/\n+$/, ""); if (length) print }' <<< "$v"
		fi
		# (or the scripts take the party to another level: the next mission's level, its instance has the story now)
		if [ -n "$arena" ]; then
			echo "$lv|poll inst $lv ${endpoll%%|*}|${endpoll#*|}|${CHAP_SECS:-1500}|the next mission after $mis (won)"
			echo "w|-|pa .* is in $back|300|the party is back in $back (any time)"
		else
			# (or pa takes the party's way on by a door of the mission or the scripts send it on: alone its instance
			# stops then)
			echo "$lv|poll inst $lv story get Mission.Library|Mission.Library = ($others)\$%travels to .* with its party%player \$a travels to .*, Door%mission level change of player \$a |${CHAP_SECS:-1500}|the next mission after $mis"
		fi
		echo "$lv|inst $lv auto \$a 0||||"
		[ "$np" -ge 2 ] && echo "$lv|inst $lv auto \$b 0||||"
		for m in $mp; do echo "$lv|inst $lv auto \$$m 0||||"; done
		echo "w|wait 8||||"
		echo "w|save $tag-$n||||"
	} > "$steps"
	echo "== $n: $mis in $lv (party in $from)"
	envv=AUTO_ENV_$mis
	# AUTO_TRIES: a chapter whose mission failed (the party died: the instance restarts without the test lines) is played
	# again from its start, this many times at most (default 3)
	for ((try = 1; try <= ${AUTO_TRIES:-3}; try++)); do
		[ $try -gt 1 ] && echo "again: try $try"
		# a super deal's next round and its retry start its level again (a new instance without the test lines): the
		# deal's lines again (chap.sh CHAP_REARM)
		rearm= rlines=
		if [ -n "$arena" ]; then
			rearm="coop: the level again"
			v=AUTO_PRE_$mis; IFS=';' read -ra pl <<< "${!v}"
			for l in "${pl[@]}"; do [[ $l == wait\ * || $l == conv ]] || rlines="$rlines;inst $lv $l"; done
			v=AUTO_A_$mis; rlines="$rlines;inst $lv auto \$a ${!v}"
			v=AUTO_B_$mis; [ "$np" -ge 2 ] && [ "${!v}" != "-" ] && rlines="$rlines;inst $lv auto \$b ${!v:-2}"
			v=AUTO_M_$mis; for m in $mp; do rlines="$rlines;inst $lv auto \$$m ${!v:-2}"; done
			rlines=${rlines#;}
		fi
		# AUTO_REARM_<Mission>: a pattern of the level's log after which the lines of AUTO_REARM_LINES_<Mission> are sent
		# again (StealMayorShip: pa back in the mayor's ship after a death)
		v=AUTO_REARM_$mis
		if [ -n "${!v}" ]; then
			rearm=${!v} rlines=
			v=AUTO_REARM_LINES_$mis; IFS=';' read -ra pl <<< "${!v}"
			for l in "${pl[@]}"; do rlines="$rlines;inst $lv $l"; done
			rlines=${rlines#;}
		fi
		CHAP_REARM="$rearm" CHAP_REARM_LINES="$rlines" CHAP_ABORT="${CHAP_ABORT-coop: the mission failed}" CHAP_ENV="$CHAP_ENV ${!envv}" "$T/chap.sh" "$tag$n" "$port" "$data" "$steps" > "$S/$tag$n.out" 2>&1
		cp "$S/$tag$n.out" "$S/$tag$n-try$try.out"
		grep -E "^(FAIL|chap )" "$S/$tag$n.out"
		grep -q "ok: the next mission after" "$S/$tag$n.out" && break
		grep -q "aborted:" "$S/$tag$n.out" || break
	done
	if ! grep -q "ok: the next mission after" "$S/$tag$n.out"; then
		echo "autochain $tag: stopped in $mis (chapter $n)"
		exit 1
	fi
	data=$S/$tag-$n-data
	echo "next: $(val "$data/party.story" Mission.Library) in $(val "$data/party.story" Mission.Map)"
done
echo "autochain $tag: $n chapters"
