# shellcheck shell=bash
# pa into the conversor and on when its new character is there (AUTO_PRE "conv", in AUTO_MID as $CONV_A); -mp2: the
# conversor's interface goes by the UI channel
export CONV_A='@|inst @ usenear $a DM_Conversion_Point|coop: player $a enters the conversor%ui: the conv opens on the client of player $a|30|pa uses the conversor;;@|-|coop: player $a plays as .*a character of the scripts|90|pa has its new character'
export AUTO_PRE_Mission_IgorCombat='switch $a;switch $b' AUTO_A_Mission_IgorCombat='1 red' AUTO_B_Mission_IgorCombat='2 red'
export AUTO_A_Mission_Massacre="1 @Traffic" AUTO_B_Mission_Massacre="2 @Traffic"
export AUTO_PRE_Mission_BankersCombat='switch $a;switch $b' AUTO_B_Mission_BankersCombat='2 red'
export AUTO_PRE_Mission_BankersCTF='switch $a;switch $b;waypoint $a 1' AUTO_B_Mission_BankersCTF='2 red'
# MortalRace: no member takes damage (the racers kill the baton's carrier again and again, and a member's death drops
# the baton: the race's checkpoints stopped for the party), as a party of one plays it with CHAP_TOUGH
export AUTO_PRE_Mission_MortalRace='switch $a;switch $b'
# (the other members play nothing: with eight a helper took the baton and pa's checkpoints did not count)
export CHAP_SECS=1800 CHAP_ENV="SCRAP_COOP_CONV=Dtritus SCRAP_COOP_WARNING=3 SCRAP_COOP_ANSWER=yes"
export AUTO_PRE_Mission_FaceTheMayor='switch $a;switch $b'
export AUTO_PRE_Mission_AttackTraitorius='spare $a 30;spare $b 30'
export AUTO_ENV_Mission_BeatPickus="SCRAP_COOP_CONV=Functionary" AUTO_PRE_Mission_BeatPickus='conv'
export AUTO_ENV_Mission_BeatPickus="SCRAP_COOP_CONV=Functionary,Dtritus,Functionary" AUTO_MID_Mission_BeatPickus='@|poll inst @ story get MissionState|MissionState = HireThugs|900|Traitorius sends the functionary to the thugs;;@|inst @ auto $a 0||||;;@|wait 5||||;;'"$CONV_A"';;@|inst @ walk $a 1||||;;@|inst @ auto $a|player $a plays|5|pa plays on as Dtritus (it walks: the thugs follow it);;@|poll inst @ story get MissionState|MissionState = TalkToTraitoriusAgain|1200|the plans photographed;;@|inst @ auto $a 0||||;;@|inst @ walk $a 0||||;;@|wait 5||||;;'"$CONV_A"';;@|inst @ auto $a|player $a plays|5|pa plays on as a functionary'
export AUTO_ENV_Mission_ArrangeBankersCTF="SCRAP_COOP_CONV=Functionary" AUTO_PRE_Mission_ArrangeBankersCTF='conv'
# BeatPickus from a story point at GetFuncShipPlans (bp2-data): BPTAIL
export BP_TAIL=';;@|poll inst @ story get MissionState|MissionState = TalkToTraitoriusAgain|1200|the plans photographed;;@|inst @ auto $a 0||||;;@|inst @ walk $a 0||||;;@|wait 5||||;;'"$CONV_A"';;@|inst @ auto $a|player $a plays|5|pa plays on as a functionary'
export AUTO_PRE_Mission_DestroyMickus='switch $a;switch $b;scan $a 1;scan $b 1'
export AUTO_MID_Mission_IdentifyMerc='@|poll inst @ story get MissionState|MissionState = IdentifyMerc2|600|the scene of the mercenary;;@|inst @ ship $a||||;;@|wait 3||||;;@|inst @ ship $b||||;;@|inst @ scan $a 1||||;;@|inst @ scan $b 1||||'
export AUTO_PRE_Mission_ProtectGus='switch $a;switch $b;scan $a 1;scan $b 1'
export AUTO_PRE_Mission_IntoTheGDB='switch $a;switch $b;scan $a 1;scan $b 1'
export AUTO_PRE_Mission_TalkToMayor='conv'
# TalkToMayor: Dtritus for the mayor, a banker robs Corruptus (the banker's slaps), Dtritus again for the mayor; pb idle;
# the end of the slaps is the state's own log line, not a poll (in a poll's gap the banker robbed the mayor: no more talk)
export AUTO_ENV_Mission_TalkToMayor="SCRAP_COOP_CONV=Dtritus,BankDirector,Dtritus" AUTO_B_Mission_TalkToMayor='-'
export AUTO_MID_Mission_TalkToMayor='@|poll inst @ story get MissionState|MissionState = StealFunctionary|900|the mayor asks for the functionary'"'"'s money;;@|inst @ auto $a 0||||;;@|wait 5||||;;'"$CONV_A"';;@|inst @ auto $a|player $a plays|5|pa plays on as a banker;;@|-|coop: MissionState = TalkToMayor2|900|Corruptus emptied by the slaps (any time);;@|inst @ auto $a 0||||;;@|wait 5||||;;'"$CONV_A"';;@|inst @ auto $a|player $a plays|5|pa plays on as Dtritus'
# BeatPickus: pb plays nothing (its helper's punches at the police and sentinels of the station draw the alarm)
export AUTO_B_Mission_BeatPickus='-'
# IntoTheGDB: once the scan found the traitor ("Destroy him"), he is shot down (he stays a bishop that is no foe)
export AUTO_MID_Mission_IntoTheGDB='@|poll inst @ show Mission_IntoTheGDB.traitorFound|traitorFound = 1|1200|the traitor scanned;;@|inst @ scan $a 0||||;;@|inst @ scan $b 0||||'
# DestroyPorkinsTransports: the transports and the bishops come when the player boards (Parking.SwitchVehicleMissionCallback)
export AUTO_PRE_Mission_DestroyPorkinsTransports='switch $a;switch $b'
# GetAuthorization: the conversor (the target) to a messenger, the messengers' tube game (Cp_MessengerPack), the
# conversor back to D-Tritus for the gatekeeper and the human, the walk with the human to the metro; pb idle (Police)
export AUTO_ENV_Mission_GetAuthorization="SCRAP_COOP_CONV=Messenger,Dtritus" AUTO_B_Mission_GetAuthorization='1'
# pb a messenger too (it escorts pa's tube, shoots the messengers that come for it)
export AUTO_PRE_Mission_GetAuthorization='usenear $b DM_Conversion_Point;wait 40'
export AUTO_MID_Mission_GetAuthorization='@|poll inst @ story get MissionState|MissionState = FreeHuman|1500|the tube filled and dropped (FreeHuman);;@|inst @ auto $a 0||||;;@|inst @ auto $b 0||||;;@|wait 5||||;;'"$CONV_A"';;@|inst @ walk $a 1||||;;@|inst @ auto $a|player $a plays|5|pa plays on as D-Tritus (it walks: the human follows it)'
# TalkToBoss (FreeHuman2): to the human first ("I'm lost. Come get me": he follows a near D-Tritus), then the walk with
# him to the bureau; pb idle (the alarm is on in the press)
export AUTO_PRE_Mission_TalkToBoss='usenear $a Human_Default;wait 5;walk $a 1' AUTO_B_Mission_TalkToBoss='-'
# GetDataPart1 and GetDataPart3: the mission runs while the player is in a ship (Parking.SwitchVehicleMissionCallback)
export AUTO_PRE_Mission_GetDataPart1='switch $a;switch $b' AUTO_PRE_Mission_GetDataPart3='switch $a;switch $b'
# GetDataPart1: pb plays nothing (its helper shot the transports down before they dropped their data)
export AUTO_B_Mission_GetDataPart1='-'
# GetDataPart2: pb plays the mission too (two members keep the three downloads running while the functionaries go to
# stop them)
export AUTO_B_Mission_GetDataPart2='1'
# M7j
# DistractHumphrey: the call and the meeting point by the plain auto player, then pa fights the traffic and pb the police
# the shots bring, with the fight helper (Humphrey is green: left out); both hold their fire at 6 and 8 of the player's
# count (ten before the time ends the round too soon, as Humphrey's ten); after the time (MissionState CallDeepThroat)
# both shoot the traffic to finish the round, then the plain auto player again (Deep Throat's phone cab)
export AUTO_MID_Mission_DistractHumphrey='@|poll inst @ story get DistractionStart|DistractionStart = 1|900|the distraction starts at the meeting point;;@|inst @ upto $a Mission_DistractHumphrey.shipsKilledPlayer 6||||;;@|inst @ upto $b Mission_DistractHumphrey.shipsKilledPlayer 8||||;;@|inst @ fight $a @Traffic|player $a fights|5|pa fights the traffic;;@|inst @ fight $b red|player $b fights|5|pb fights the police the shots bring;;@|poll inst @ story get MissionState|MissionState = CallDeepThroat|1200|Humphrey kept busy until Deep Throat has the code;;@|inst @ upto $a 0 0||||;;@|inst @ upto $b 0 0||||;;@|inst @ fight $b @Traffic||||;;@|poll inst @ show Mission_DistractHumphrey.missionComplete|missionComplete = 0|900|the round finished (ten ships);;@|inst @ fight $a 0||||;;@|inst @ fight $b 0||||;;@|inst @ auto $b 0||||'
# with eight members only pa and pb play it (their counts and the police the shots bring; six more helpers kept the
# police and the alarms up): the more members stay idle
# (the helpers stop after the round: with eight members their fights kept the police and pa's alarm up, and the call
# to Deep Throat waits for the alarm to fall)
# DestroyBadDebtors: pa shoots the money transports (the mission's targets), pb keeps the mercenaries off the treasures
# the bankers carry home (the money counts only when a banker takes it)
export AUTO_MID_Mission_DestroyBadDebtors='@|wait 20||||;;@|inst @ fight $b Mission_DestroyBadDebtors.mercsActive|player $b fights|5|pb fights the mercenaries'
# InfiltrateBank: after the gatekeeper's no the chief of bankers is the target: pa possesses him (the possession key),
# then the auto player takes the gatekeeper's teleporter as the chief
export AUTO_MID_Mission_InfiltrateBank='@|poll inst @ show MissionsFuncs.currentTarget|currentTarget = .BankMaster_Default|600|the chief of bankers is the target;;@|inst @ auto $a 0||||;;@|wait 5||||;;@|inst @ possess $a BankMaster_Default|player $a goes to possess|5|pa goes to possess the chief;;@|-|possession of BankMaster_Default done|120|pa possessed the chief (any time);;@|wait 5||||;;@|inst @ auto $a 1|player $a plays|5|pa plays on as the chief'
# ReinsertOwnMatrix2: the bishops (red) attack whoever boards a ship; as single player pa walks to the restore point on
# foot and uses it, pb stays out of ships (no auto line)
export AUTO_PRE_Mission_ReinsertOwnMatrix2='usenear $a DM_Mission_RestoreMatrix;wait 30' AUTO_B_Mission_ReinsertOwnMatrix2='0'
# DestroyHumphrey: Humphrey attacks once the player boards a ship (Parking.SwitchVehicleMissionCallback StartMission)
# (part 3: with two members Humphrey killed the party again and again: no member takes damage, as the 8 player chain needs none)
export AUTO_PRE_Mission_DestroyHumphrey='switch $a;switch $b'
# GoWithHuman: the human waits at the entrance and follows D-Tritus once he is near (CheckMaxDist), to the machine's
# switch; he stops when D-Tritus is too far: pa goes to him first, then walks (test line walk)
export AUTO_PRE_Mission_GoWithHuman='goto $a Human_Entrance 0 300;wait 5;walk $a 1 Human_Entrance'
# Part 1 of the story (stage 5b), the intro's levels by their steps, as single player plays them (no mission targets):
# Orbit: the gatekeeper's talk, the decontamination, the check-in, the fall to Fake
export AUTO_A_MapDecont='-' AUTO_B_MapDecont='-'
export AUTO_MID_MapDecont='@|inst @ use $a Decontamination_keyboard|scene Decontamination ended|120|the decontamination;;@|inst @ use $a Functionary_00|scene Functionary ended|200|the check-in;;@|inst @ use $a DownPanel_keyboard||||'
# Fake: its scene chain plays by itself and ends in the Junkyard
export AUTO_A_Fake='-' AUTO_B_Fake='-'
# RustyFirstContact: Berto's scene, Rusty's talk, the keypad: the tutorial question and the ship editor on pa's client
# (SCRAP_COOP_ANSWER, SCRAP_COOP_SHIPEDIT), the platform: Rusty's farewell, the elevator to Outskirts
export AUTO_A_Mission_RustyFirstContact='-' AUTO_B_Mission_RustyFirstContact='-'
export AUTO_MID_Mission_RustyFirstContact='@|-|scene Mecanic ended|300|Berto'"'"'s scene (any time);;@|inst @ goto $a Rusty_0 0 250||||;;@|wait 2||||;;@|inst @ use $a Rusty_0|player $a uses Rusty_0|10|pa talks to Rusty;;@|wait 25||||;;@|inst @ goto $a DM_EditKeyboard 0 250||||;;@|wait 3||||;;@|inst @ use $a DM_EditKeyboard|ui: the yesno opens on the client of player $a|20|the keypad asks pa;;@|-|ui: player $a closed the ship editor|120|pa'"'"'s editor closes;;@|inst @ goto $a DM_Door_EditShip_00 0 150||||;;@|wait 3||||;;@|inst @ use $a DM_Door_EditShip_00|scene MecanicFinal plays|30|Rusty'"'"'s farewell'
# MeetBoss: after the gambler's talk on the desktop the possession of Berto is the task (SpecialActionActive 2): pa
# possesses him (the possession key), hides by the metro station until the alarm the possession raised falls (as the
# single player does), then the auto player goes on as Berto (the conversor back to D-Tritus, Humphrey)
export AUTO_MID_Mission_MeetBoss='@|poll inst @ story get SpecialActionActive|SpecialActionActive = 2|1200|the possession task;;@|inst @ auto $a 0||||;;@|wait 3||||;;@|inst @ possess $a Berto_Default|player $a goes to possess|5|pa goes to possess Berto;;@|-|possession of Berto_Default done|120|pa possessed Berto (any time);;@|wait 5||||;;@|inst @ goto $a DM_Door_Metro_Press 0 400||||;;@|poll inst @ status|player $a \(NetPlayer[0-9_c]+\):.*alarm 0.00|400|the alarm of pa (Berto) fell by the metro station;;@|inst @ auto $a 1|player $a plays|5|pa plays on as Berto'
# PoliceBossCheats: pa a policeman by the conversor (Ficus' friend wants it), the photo of the boss cheating at poker
# (the use key when he cheats), the fight scene, then the auto player again
export AUTO_ENV_Mission_PoliceBossCheats="SCRAP_COOP_CONV=Police" AUTO_PRE_Mission_PoliceBossCheats='conv'
export AUTO_MID_Mission_PoliceBossCheats='@|poll inst @ story get Mission_PoliceBossCheats|Mission_PoliceBossCheats = BossCheats|900|Ficus'"'"' friend: the boss cheats at poker (a photo);;@|inst @ auto $a 0||||;;@|wait 10||||;;@|inst @ goto $a DM_Mission_PoliceBoss_BossCheater 0 200|player $a next to|20|pa at the poker table;;@|wait 3||||;;@|poll inst @ press $a act3 -150|scene PoliFight|600|pa photographs the boss cheating: the fight scene;;@|-|scene PoliFight ended|600|the scene ends (any time);;@|wait 10||||;;@|inst @ auto $a|player $a plays|5|pa plays the mission'"'"'s targets again'
# FunctionaryHunting: in a stolen police ship (Ficus' disguise) to the meeting point, where the police friends come
export AUTO_PRE_Mission_FunctionaryHunting='board $a POLI;wait 5;goto $a DM_Mission_FunctionaryHunting 0 0;wait 30'
# SheriffMayor: the police boss is paid (the money of the party's member, as the earlier chains)
export AUTO_PRE_Mission_SheriffMayor='money $a 50000'
# CatchTrurl: after Trurl's talk the functionaries with his maps are possessed one after the other (their maps drop),
# then D-Tritus again by the conversor, the auto player photographs the maps and talks to Trurl
export AUTO_MID_Mission_CatchTrurl='@|poll inst @ story get CatchTrurlMissionData|CatchTrurlMissionData = KillFunctionaries|900|Trurl'"'"'s talk: the functionaries with his maps;;@|inst @ auto $a 0||||;;@|wait 5||||;;@|inst @ possess $a Functionary_10|the possession of Functionary_10 done|150|pa possesses Functionary_10;;@|inst @ possess $a Functionary_23|the possession of Functionary_23 done|150|pa possesses Functionary_23;;@|inst @ possess $a Functionary_25|the possession of Functionary_25 done|150|pa possesses Functionary_25;;@|wait 5||||;;'"$CONV_A"';;@|inst @ auto $a|player $a plays|5|pa photographs the maps and talks to Trurl again'
# StealMayorShip: in the mayor's ship twenty police ships of the level to destroy, then the call once the alarm fell
export AUTO_MID_Mission_StealMayorShip='@|wait 60||||;;@|inst @ auto $a 1 @OutCop|player $a plays|10|pa fights the police ships of the level;;@|poll inst @ show Mission_StealMayorShip.numCopsKilled|numCopsKilled = 20|2400|twenty police ships destroyed;;@|inst @ auto $a 0||||;;@|inst @ auto $b 0||||;;@|poll inst @ status|player $a \(.*alarm 0\.00|900|pa'"'"'s alarm falls before the call;;@|inst @ auto $a 1||||;;@|inst @ auto $b 2||||'
# StealMayorShip: only the kills of the mayor's ship count: the other members stay out of the fight (their kills of
# the police would not count; alive they keep the party alive while pa fights)
export AUTO_B_Mission_StealMayorShip='-'
# ApologizeMayor: pb plays nothing (its helper's punches at the sentinels and gears of the town hall draw a fight the mayor
# joins: he stays on a gear and does not talk again)
export AUTO_B_Mission_ApologizeMayor='-'
# DestroyPorkinsTransports: the bishops kill the whole party now and then (the mission starts again): pb takes no damage
export AUTO_PRE_Mission_DestroyPorkinsTransports='switch $a;switch $b'
# the CTF super deals (FZ): pa flies onto the waypoint's item (the flag) as a player follows its arrow, the others fight
export AUTO_PRE_SuperDeal_Fifth='switch $a;switch $b;waypoint $a 1' AUTO_PRE_SuperDeal_Ninth='switch $a;switch $b;waypoint $a 1'
# the Domination super deals (OM): pa flies onto the waypoint's item (the baton, then the zones), the others fight
export AUTO_PRE_SuperDeal_Sixth='switch $a;switch $b;waypoint $a 1' AUTO_PRE_SuperDeal_Tenth='switch $a;switch $b;waypoint $a 1'
# the automatic player does not win a Domination deal (it does not hunt the enemy's baton carrier): after its try the
# deal's victory is called as the campaign chain of round 8o does (the story goes on to the next deal)
export AUTO_MID_SuperDeal_Sixth='@|wait 150||||;;@|inst @ call SuperDeal_Sixth.Victory|coop: call SuperDeal_Sixth.Victory: done|10|the deal'"'"'s victory (test line)'
export AUTO_MID_SuperDeal_Tenth='@|wait 150||||;;@|inst @ call SuperDeal_Tenth.Victory|coop: call SuperDeal_Tenth.Victory: done|10|the deal'"'"'s victory (test line)'
# ApologizeMayor: pb plays nothing (its helper's punches at the sentinels and gears of the town hall draw a fight the mayor
# joins: he stays on a gear and does not talk again)
export AUTO_B_Mission_ApologizeMayor='-'
# DestroyPorkinsTransports: the bishops kill the whole party now and then (the mission starts again): pb takes no damage
export AUTO_PRE_Mission_DestroyPorkinsTransports='switch $a;switch $b'
# Part 3 of the story (stage 5b), chapters 69 to 100
# GetDataPart2 alone: the three downloads run only together and each functionary goes back to stop his; the single
# member starts them, knocks the functionaries down on foot, then starts the downloads again
export AUTO_MID1_Mission_GetDataPart2='@|wait 40||||;;@|inst @ auto $a 0||||;;@|inst @ fight $a Mission_GetDataPart2.funcNames|player $a fights|5|pa fights the functionaries;;@|wait 150||||;;@|inst @ fight $a 0||||;;@|inst @ auto $a|player $a plays|5|pa starts the downloads again'
# GetDataPart3 alone: the police the data cubes' fight brought keep the alarm up while the member fights them (the
# call waits for the alarm to fall), and the fighters left of the last wave draw it back into a ship, whose boarding
# callback starts the data's waves again: after the boss' call it stops, waits for its alarm to fall, then goes to the
# phone cab of the call on foot and uses it (test lines switch, target and usetarget)
# DefeatCrazyGambler1 to 3: the crazy gambler's fights kill the party of two again and again (the mission starts
# again): no member takes damage
# WhoIsDeepThroat: the gatekeeper of the boss room's teleporter lets only D-Tritus' group pass; the members are still
# the GDB's bishops (single player's player too, until the conversor): pa into D-Tritus by the conversor first
# BertoMassacre: the Berto waves killed the party of two three times in a row in a run (passed in others): no damage
