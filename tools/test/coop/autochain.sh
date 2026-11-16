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
n=0
while [ $n -lt $max ]; do
	n=$((n + 1))
	mis=$(val "$data/party.story" Mission.Library)
	lv=$(lvname "$(val "$data/party.story" Mission.Map)")
	owner=$(cut -d' ' -f1 "$data/party.owner")
	from=$(lvname "$(val "$(ls $data/players/$owner*.dat | head -1)" level)")
	others=$(echo "$missions" | grep -vx "$mis" | paste -sd'%')
	steps=$S/$tag-$n.steps
	{
		echo "# autochain $tag $n: $from -> $lv, $mis"
		echo "$from|wait 10||||"
		if [ "$from" != "$lv" ]; then
			echo "w|inst $from travel \$a $lv|pa .* is in $lv|150|pa travels to $lv (any time)"
			echo "w|inst $from leader \$b|pb .* is in $lv|150|pb goes to its leader in $lv (any time)"
		fi
		echo "$lv|wait 15||||"
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
		echo "$lv|inst $lv auto \$a ${!va}|player \$a plays the mission's targets|5|pa plays the mission's targets"
		# AUTO_B_<Mission>="-": pb plays nothing (its helper's fights would draw the alarm: TalkToMayor's robbery)
		[ "${!vb}" = "-" ] || echo "$lv|inst $lv auto \$b ${!vb:-2}|player \$b plays the mission's targets|5|pb helps in the fights"
		echo "pa|shot ac-$mis-pa||||"
		# AUTO_MID_<Mission>: more steps after the auto lines, separated by ";;" ("@|" and " @ " stand for the level), e.g. a poll of
		# a step of the mission, then the conversor again (BeatPickus: a functionary for Traitorius, Dtritus for the thugs)
		mid=AUTO_MID_$mis
		if [ -n "${!mid}" ]; then
			v=${!mid}; v=${v//@\|/$lv\|}; v=${v// @ / $lv }
			awk -v RS=';;' '{ sub(/^\n+/, ""); sub(/\n+$/, ""); if (length) print }' <<< "$v"
		fi
		# (or the scripts take the party to another level: the next mission's level, its instance has the story now)
		echo "$lv|poll inst $lv story get Mission.Library|Mission.Library = ($others)\$%travels to .* with its party|${CHAP_SECS:-1500}|the next mission after $mis"
		echo "$lv|inst $lv auto \$a 0||||"
		echo "$lv|inst $lv auto \$b 0||||"
		echo "w|wait 8||||"
		echo "w|save $tag-$n||||"
	} > "$steps"
	echo "== $n: $mis in $lv (party in $from)"
	envv=AUTO_ENV_$mis
	# AUTO_TRIES: a chapter whose mission failed (the party died: the instance restarts without the test lines) is played
	# again from its start, this many times at most (default 3)
	for ((try = 1; try <= ${AUTO_TRIES:-3}; try++)); do
		[ $try -gt 1 ] && echo "again: try $try"
		CHAP_ABORT="${CHAP_ABORT-coop: the mission failed}" CHAP_ENV="$CHAP_ENV ${!envv}" "$T/chap.sh" "$tag$n" "$port" "$data" "$steps" > "$S/$tag$n.out" 2>&1
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
