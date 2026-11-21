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
# the steps' lines for pb of chainenv.sh (AUTO_PRE, AUTO_MID: a switch, a scan, a fight) go to p3.. too with their own
# index (their leader and auto lines are their own); alone they are left out
each() {
	local l m
	while IFS= read -r l; do
		if [[ $l != *'$b'* ]]; then echo "$l"; continue; fi
		[ "$np" -lt 2 ] && continue
		echo "$l"
		[[ $l == *'leader $b'* || $l == *'auto $b'* ]] && continue
		for m in $mp; do echo "${l//\$b/\$$m}"; done
	done
}
# the steps of the story's end ($endch 99: the teleporter to the AB, every member boards, the boss comes and is fought
# down by the members' weapons, the party in ABIndoor with the story at AfterFinalFight; 100: TheEndA, pa uses D-Tritus'
# ship, TheEndB, the instance ends the story, every member's client goes to the credits)
endsteps() {
	local p all="pa"
	[ "$np" -ge 2 ] && all="pa pb$mp"
	if [ "$endch" = 99 ]; then
		for p in $all; do echo "w|-|$p .* is in $lv|150|$p in $lv (any time)"; done
		echo "$lv|inst $lv goto \$a DM_Door_Teleporter_00 0 200||||"
		echo "$lv|wait 3||||"
		echo "$lv|inst $lv use \$a DM_Door_Teleporter_00|player \$a uses DM_Door_Teleporter_00|10|pa uses the teleporter to the AB"
		for p in $all; do echo "w|-|$p .* is in AB|240|$p in the AB (any time)"; done
		echo "AB|wait 20||||"
		echo "AB|inst AB switch \$a|player \$a into|30|pa gets into a ship"
		echo "AB|-|player \$a into a ship: Parking.SwitchVehicleMissionCallback|10|the boarding's mission callback: the boss comes (any time)"
		echo "AB|inst AB switch \$b||||"
		echo "AB|wait 5||||"
		echo "AB|inst AB show MapFinalFight.bossName|bossName = '|10|the boss is there"
		echo "AB|inst AB fight \$a MapFinalFight.bossName|player \$a fights MapFinalFight.bossName|5|pa fights the boss"
		echo "AB|inst AB fight \$b MapFinalFight.bossName||||"
		echo "AB|-|hit by player \$a .* on Boss|400|pa's shots hit the boss (any time)"
		for p in $all; do echo "w|-|$p .* is in ABIndoor|${CHAP_SECS:-1500}|$p in ABIndoor: the boss destroyed (any time)"; done
		echo "ABIndoor|poll inst ABIndoor story get Mission.Library|Mission.Library = Mission_AfterFinalFight|60|the next mission after $mis"
	else
		echo "ABIndoor|-|scene TheEndA plays on the clients|300|the end's first scene (any time)"
		echo "ABIndoor|-|scene TheEndA ended|600|the first scene ends (any time)"
		echo "ABIndoor|wait 5||||"
		echo "ABIndoor|inst ABIndoor auto \$a|player \$a plays the mission's targets|5|pa goes to D-Tritus' ship (the mission's target) and uses it"
		echo "ABIndoor|-|scene TheEndB plays on the clients|300|the last scene (any time)"
		echo "ABIndoor|-|the story is finished: the credits on the clients|900|the instance ends the story"
		# the last member's step is the chapter's end (the chain's pass)
		for p in $all; do
			w="$p's client goes to the credits"
			[ "$p" = "${all##* }" ] && w="the next mission after $mis: the credits on every client ($w last)"
			echo "$p|-|coop: the story is finished: the credits|60|$w (any time)"
		done
	fi
}
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
	# the story's end: BeforeFinalFight from GamblinDen through the final fight (AB) to ABIndoor, where the story names
	# AfterFinalFight; AfterFinalFight from ABIndoor (its party is there, not in the story's GamblinDen) to the credits
	endch=
	case $mis in
	Mission_BeforeFinalFight) endch=99 extra="AB ABIndoor";;
	Mission_AfterFinalFight) endch=100 lv=ABIndoor extra=GamblinDen;;
	*) extra=;;
	esac
	if [ "$CHAP_LEVELS" = auto ]; then
		ls=$(printf '%s\n' "$from" "$lv" "$arena" "$(nextlv "$mis")" $extra | grep -v '^$' | grep -vx Orbit | awk '!s[$0]++' | paste -sd,)
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
		if [ -n "$endch" ]; then
			endsteps
		else
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
		# alone (CHAP_PLAYERS=1) AUTO_PRE1_<Mission> and AUTO_MID1_<Mission> take the place of AUTO_PRE and AUTO_MID: a
		# party of one plays some missions another way (GetDataPart2's three downloads)
		v=AUTO_PRE1_$mis; [ "$np" = 1 ] && [ -n "${!v}" ] && pre=$v
		if [ -n "${!pre}" ]; then
			IFS=';' read -ra lines <<< "${!pre}"
			for l in "${lines[@]}"; do
				# "conv": pa into the conversor, on as soon as its new character is there (a player leaves the machine at
				# once: a sentinel sent to watch it finds one that stays)
				if [[ $l == conv ]]; then echo "${CONV_A//@/$lv}" | awk -v RS=';;' '{ sub(/^\n+/, ""); sub(/\n+$/, ""); if (length) print }'
				elif [[ $l == wait\ * ]]; then echo "$lv|$l||||"; else echo "$lv|inst $lv $l||||"; echo "$lv|wait 5||||"; fi
			done
		fi
		# CHAP_TOUGH=1: pa takes no damage (test line tough: a party of one dies where two members take turns, the
		# police waves and the smart mercenaries of the fights; deaths are played with 2 and 8 players)
		[ -n "$CHAP_TOUGH" ] && echo "$lv|inst $lv tough \$a 1||||"
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
		v=AUTO_MID1_$mis; [ "$np" = 1 ] && [ -n "${!v}" ] && mid=$v
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
		fi
		echo "w|wait 8||||"
		echo "w|save $tag-$n||||"
	} | each > "$steps"
	echo "== $n: $mis in $lv (party in $from)"
	envv=AUTO_ENV_$mis
	# AUTO_TRIES: a chapter whose mission failed (the party died: the instance restarts without the test lines) is played
	# again from its start, this many times at most (default 3)
	for ((try = 1; try <= ${AUTO_TRIES:-3}; try++)); do
		[ $try -gt 1 ] && echo "again: try $try"
		# a lost super deal starts its level again (a new instance without the test lines): played again too
		ab="${CHAP_ABORT-coop: the mission failed}"
		[ -n "$arena" ] && [ -z "${CHAP_ABORT+x}" ] && ab="$ab|coop: the level again"
		CHAP_ABORT="$ab" CHAP_ENV="$CHAP_ENV ${!envv}" "$T/chap.sh" "$tag$n" "$port" "$data" "$steps" > "$S/$tag$n.out" 2>&1
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
