# Multiplayer

ScrapC has its own multiplayer (issue #1). It is not compatible with the original game's network code and has its own encrypted UDP protocol. Players meet in worlds. A world is hosted by a game or by a dedicated server, and it has a mode: Sandbox, Free roam, Coop story or one of the original's seven PvP modes.

## How it is built

A world is one coordinator plus one headless instance per level. The coordinator keeps the players, their saved progress, the list of instances and the travel between them. An instance is a headless `ScrapC -instance:<Level>` that runs one level for all players in it. The server decides everything, the clients send their input and predict their own character or ship. Elevators, metro rides and the doors that change level send the player to the instance of the target level (a loading screen, as in single player). An instance with nobody in it ends after a grace time, except the ones the coordinator keeps up.

An instance simulates around a set of viewers, one per joined player, made each frame from the player's camera entity or else from the player's entity. Where single player reads the camera or the render flag of a model, the simulation (traffic, AI placement and pause, police, sentinels) reads the viewer set, and "seen" is a frustum test against the viewers. The rail and sky traffic that single player keeps around its camera is kept around every leader viewer. Viewers within 75000 units of an earlier leader share its quota, up to 16 leaders get a whole quota and more share the counts of 16, and traffic ships farther than 37500 units from every viewer run their slots every 0.15 s instead of 0.05 s. A load probe with 64 test viewers decided these values. A script that sets the clock speed in a player's context makes only that player's entity run faster by 1 / speed, because all players share one clock.

A game that hosts runs the coordinator inside the game and starts the instances as child processes on the same machine. The host plays as a client of its own world.

## Playing

### Menus

Main menu, Multiplayer. The button Free roam (bottom left, next to Back) opens the pages of the new worlds. The old pages of the original still exist: the Create button hosts the chosen mode on the new network, and the Join tab opens the world list.

Join world has three ways to find a world.

- Local Network asks the LAN (broadcast on IPv4, a multicast group on IPv6).
- List server asks the list server set in `scrapdecomp.ini`. Without one the page says so.
- Specify IP takes an IPv4 address, `[IPv6]:port` or a bare IPv6 address. The port defaults to `worldport`.

The list has the columns name, mode, players, level and ping. Click a line to join. Refresh and the column sort work as in the old browser. The Name field at the top right is the player name of the garage.

Host world has the level the world starts in, the mode, PvP on or off, List server on or off (register at the list server), the maximum number of players (1 to 64) and the port. The modes are Sandbox, Free roam, Coop story and the seven PvP modes. A PvP mode takes its levels from the mode, the rotation is the chosen level followed by the other maps picked on the old Create page. The settings of the page last for the session.

In a world the Abort game entry of the in-game menu leaves the world and loads the main menu. The rest of that menu is the single player one, so Save and Load do nothing useful there.

Options in `scrapdecomp.ini`, section `[general]`, next to the exe.

| Option | Meaning |
|--------|---------|
| `worldport` | port of a hosted world and first port of the LAN search (default 28100) |
| `lanaddr` | ask this numeric address instead of the broadcast (for tests) |
| `listserver`, `listport` | the list server (default none, port 28090) |
| `hostbind` | address a hosted world binds (default any) |

A player has an identity key (`player.key` in the config folder, made on the first start). The coordinator saves progress per key, not per name. There are no accounts and no passwords.

### Keys

| Key | Action |
|-----|--------|
| F1 | get into the nearest parked ship (on foot), get out of the ship (in a ship) |
| F2 to F7 | the faction keys in Free roam (see below), spectate and team in the PvP modes |
| F7 | in Coop story: go to the party leader |
| Use (left mouse button) | doors, elevators, metro, faction desks, characters to talk to |

F1 is the mission info key of single player. A message tells you the key the first three times. The F2 to F7 keys are the free actions QMsg1 to QMsg6 of the controls. F7 for the party leader is the Walk action Coop leader of the controls menu. An old config without it gets F7 when the key is free.

## Sandbox

The city of single player with all its traffic, police and NPCs, without missions. Up to 64 players. The levels are DownTown, Outskirts, Commercial, Scrapyard, GDB, Police, Bank, Press, Temple, TownHall and GamblinDen, linked as in single player (metro and elevators), plus the places in the section Places below.

A new player starts on foot at a free spot of a spawn point with 2000 money. On foot, F1 gets into a parked ship near you. A ship you got into is yours: it stays as your own ship and nobody else can board it, and it is called again after a travel. Your alarm and your money are your own, so the police go after the player who did the crime.

PvP is a setting of the world. With PvP off, players cannot hurt each other. After a death you get a new character after a few seconds. A ship that died with you is not charged by default.

The server saves money, alarm, character, owned ship and the faction state of each player (see Saved progress). Each player also has a Sputnik beside the called ship in the outdoor levels. Use it for the hangar screen and change to the spare ship.

A world of the `-mp2` server gives each player single player's own ship, Sputnik and plate (the plate on the own ship). The player's own Sputnik opens its hangar screen on the player's client, rebuilds a lost ship and makes the picked ship where the old one stood. It follows the own ship when the player parks it on another parking box. Another player's Sputnik and own ship cannot be used. In Rusty's Junkyard the keypad opens the ship editor on the player's client, and one player at a time uses it. The platform rides up to Outskirts, where the built ship waits on the junker parking. A pickup (a coin, a box of a ship) goes to one player when two reach it, and comes back after its time as in single player. The lives of the GDB (`Player.NumLives`) are each player's own and show on its HUD. They count only where the story counts them (`Player.InfiniteLives` 0): a death then costs a life, and with none left a player of a world with a coordinator goes to the jail of the Police level. After a death the player comes back with its own ship and Sputnik beside it, and a lost own ship is lost in its hangar as in single player. The hangar, the lives and the money are saved with the player's progress and come back when it joins again.

Possession and the conversor work per player in a `-mp2` world as in single player. Two players never get the same body or machine: a body that another player is (a player's character) or that another player took cannot be possessed, and a conversor another player uses refuses the second player with single player's message. The new character's prediction works (the client's own character takes the class the server gives it). A level that makes the player another character makes every joining player that character: GDB makes each player the Bishop, as single player arrives there as the Bishop. The flows of the scripts start on the player's client at the key press, as single player shows them in the frame after it: the possession's white flash, the camera on the conversor or on the elevator or teleporter, the metro's map, the HUD of a movie and no controls. The server runs the flow from the same input and hands over (the new character, the conversor's pick screen, the elevator's camera, the metro's map message). A refused flow (a busy conversor) or one without a hand-over in 3 seconds goes back: camera, controls and HUD on foot. Each player's alarm sends the police at that player only; the police's bribe is paid by the player who talks to the police and turns off that player's alarm. Phone cabs and news panels are each player's: the way point of a cab, the news one reads.

Challenges, revenges, bets and races work per player as in single player. A challenge taken in an indoor level (the bet of a talk with a character, when the player's lives count, `Player.InfiniteLives` 0) sends the player to the outdoor level of that area, and its race or combat starts there when the player arrives. The bet is paid from the player that took it, and the win is paid to that player. A race belongs to its player: its checkpoint and the count down lights are shown to that player alone, and the rival ships are shown to everyone. Another player cannot take a player's checkpoint, and the ships of one player's race or combat do not hurt another player and are not hurt by one. Two players can race at the same time. The race start camera plays on the racer's client. After a race or combat the revenge is set as in single player: the foe calls the player at the nearest phone cab. Crazy deals, the counters they use (`Stats.*`) and the super deal state are each player's own in a free world (the coop story keeps them as the party's). A kill counts for the player that killed. When a deal is done, that player's HUD shows the mission scorer. A super deal is played in an instance of its own: the player who starts it (at the crazy gambler) and the other players within 3000 units of it that have the same super deal open travel together to a new instance of the super deal's map (CA, QF, FZ, OM). The coordinator starts that instance for the group and ends it once it is empty. When the super deal ends, each member gets single player's result in its own progress (the rewards of a victory, the next deals) and goes back to the gambler's meeting room in GamblinDen. There is no retry: a lost super deal sends the group back too.

## Free roam

Sandbox plus six factions. The factions are the six groups of the game (`data/modules/challenge.mod`) and they are defined in `data/factions/factions.def`, which also holds the numbers (cooldowns, prices, ranges). The actions are not in the original.

To join, go on foot to the office of the faction and use the desk (or press F2 within reach of the desk). Use it again to leave. Wanted players cannot join. A member plays as the class of its rank (Police, Functionary, Bishop and so on), flies the ships of its faction and has ranks that rise with its deeds. The rank and the deeds are saved.

| Faction | Office (level) | F2 | F3 | F4 | F5 | F6 | F7 |
|---------|----------------|----|----|----|----|----|----|
| Police | Police (front desk, from Outskirts) | join, leave | siren | armour | arrest | backup | ship |
| Bishops | Temple | join, leave | heal | ray | revive | ship | bounty |
| Bankers | Bank | join, leave | steal | suck | loan | bribe | ship |
| Bureaucracy | TownHall | join, leave | bullet time | bla-bla | permit | fine | ship |
| Press | Press | join, leave | photo | courier | ship | bounty | none |
| Mercs | GamblinDen | join, leave | contracts | hire | heavy | ship | bounty |
| no faction | | join | bounty | none | none | none | none |

What the actions do, with the values of `factions.def`.

Police. Siren: traffic within 6000 units moves away from your police ship. Armour: you turn into the armoured Gear body with 300 life (on foot, toggle). Arrest: the nearest wanted player within reach pays a fine of 300 per wanted level and goes to the jail, you get 150 per level and a step toward the next rank. Backup: two police ships outdoors, or the gears of the level indoors, go after the nearest wanted player. Ship: your police ship of your rank (SPoli1 to SPoli5, SPoliBoss1) is called beside you. Ranks: Rookie, Officer, Sergeant, Lieutenant, Captain.

Bishops. Heal: the nearest wounded player within 900 gets up to 60 life and pays 120. Ray: 30 damage to what is in front of you within 2500. Revive: a dead player waits 15 seconds for a reviver instead of the normal respawn, and a Bishop within 900 of the body revives it for a price. The Temple and 3000 units around the Temple elevator in Commercial are a sanctuary, where nobody can hurt anybody. Ranks: Novice, Brother, Priest, Bishop, Archbishop.

Bankers. Steal takes 150 from a player in front of you. Suck takes 60 from each player in front of you that may be robbed. Loan gives 1000 to the nearest player without debt, who owes 1250 and pays half of each later income back, and the banker gets 100. Bribe lowers the wanted level of a wanted player (you) by one for 400, paid to the nearest police officer. In the Bank the vault opens every 600 seconds for 60 seconds. A banker at it secures it for a bonus, anyone else robs it for 1500 and it counts as theft. Ranks: Clerk, Teller, Broker, Director, Bank Master.

Bureaucracy. Bullet time slows characters and ships within 2500 units to 0.3 time speed for 8 seconds. Bla-bla puts the players and NPCs within 1200 to sleep for 4 seconds. Permit sells a ship permit for 300 to the nearest player. Ships of faction ranks from 3 up need a permit (not for the Bureaucracy). Fine fines a wanted player or a player in such a ship without a permit. Ranks: Intern, Clerk, Officer, Head Officer, Deputy Mayor.

Press. Photo of a player in front of you within 4000 units who did a crime in the last 15 seconds gives 6 wanted points to the photographed and 100 to you. Courier takes a job at the Press desk to a desk in another level, 300 seconds, 400 reward and 200 more within half the time. Headlines of arrests, bounties, rescues, robberies and deliveries show for 9 seconds above the radar. Ranks: Runner, Reporter, Editor, Anchor, Chief Editor.

Mercs. Bounty puts 500 (at most 5000 on a player, for an hour) on the last player that hurt you, else the nearest one. A Merc that kills the target gets the bounty and no crime counts. Contracts shows the three highest bounties of the level. Hire gives two SMERC1 ships for 500 for 120 seconds that follow you and attack who hurt you. Heavy gives your ship 1.5 times its life and full ammo. Ranks: Thug, Gun, Hunter, Veteran, Warlord.

Bounty for the other players (F7 Bishops, F6 Press, F3 no faction) is the same as the Mercs' bounty.

Crimes against players and NPCs raise your wanted points. The wanted level goes from 1 to 5 (from 1, 6, 14, 26 and 45 points). It falls by itself, faster when the police alarm is off. The police of the level hunt a wanted player. A hurt or killed player, police NPC or civilian counts as a crime. Only a police member against a wanted player and a Merc against a bounty target are lawful. A member that dies in the level of its office comes back at the jail dummy of that level.

Who may hurt whom. PvP off in the world: nobody, except police against wanted players (option `-policepvp`, default on). Hostile factions (Police and Mercs, Bishops and Bankers) may hurt each other if `-factionpvp` is on (default). Friends (Police with Bureaucracy, Bankers with Bureaucracy) and members of one faction never do.

In a world of the `-mp2` server the factions work as in the other worlds. The faction, rank, deeds, wanted level, bounty, debt, permit and courier job of a player are its own and are saved with its progress, so leaving the world and joining again keeps them. Other players see the character class of a member (also the armour), the model and pilot of its faction ship and its siren, also players who join later. The effects of the abilities (heal, ray, revive, steal, suck, money, photo) show on the clients of the players near, as single player shows those effects. The police that the crimes of a wanted player call go after that player only, and they can kill a wanted player as they kill a wanted player in single player.

## Coop story

Up to 8 players play the single player campaign together. There is one party per world. The story (the save variables of single player) belongs to the party and is saved with it. The first member that joins owns the story. A new world starts with the intro video and the start of a new game in Orbit.

Everyone plays the missions with the mission scripts of single player. The member that uses a mission entity is "the player" of the mission, the others are helpers. They can fight, fly, carry and talk. Scenes play on the clients of all members in the level (Escape skips only your own copy), and members in other levels get a notice that a scene plays. In conversations the speaker's client gets the text and the options, members within 2500 units hear the voices. Menus of a mission (the Junkyard question and ship editor, the character conversor, Sputnik's hangar screen) open on the client of the member the mission acts for.

The party has one purse. What each member earns or spends goes into it and every member sees the sum. The ship you call is the party's hangar ship. The party has one Sputnik at the level's start, and a member that uses it takes the ship beside it.

A level change of the story into or out of Orbit, Fake, Junkyard, AB and ABIndoor takes the whole party along. Other level changes (elevators, metro) take the member that used them. F7 takes a member to the party leader. A member that joins later starts in the level of the leader. A member that leaves keeps its own copy of the story, and when it joins again and the party's story differs it takes the party's story (its own copy is kept as `.own` in the data folder).

Possession and the conversor work as in single player and change only the member that did it. The class stays with the member for respawns and travel. Mission counters (`Stats.*` and counted names) add up over all members, the other story variables are last writer wins.

When the scripts set the save variable `Char` of "the player" (`Coop_SetChar`: the start of a level such as the bishop of GDB or D-Tritus of ABIndoor, the end of a mission) on a `-mp2` coop instance, it becomes the class that every member carries (the `char` of its state), and a member that enters the level later gets the character that the scripts of the level made the player (`Coop_EnterChar`). Single player only sets the save variable. In the final fight (AB) the death hook of the mission (`Chars.MainCharDeath`, which coop runs only when the whole party died) runs in the context of the member who died last, so the boss goes back and the ship of that member comes back as in single player. A `-mp2` client preloads no mission libraries, the code of the mission is the instance's.

A member that dies gets a new character, the party plays on. In a CTF, Domination or MortalRace super deal every member's death is the player's death for the deal, and a member that carried the flag or the baton drops it. In the mission MortalRace (as in the CTF, Domination and MortalRace super deals) every member's death runs the mission's death hook for that member, so a member that carried the baton drops it where it died and the party races on; the instance brings the member back as after any other death (the hook's own respawn of the player is left out). Any member can take the baton up; only the checkpoints of the ship that carries it count; a member that leaves its ship drops the baton only when that ship carried it. When a mission failed and the level starts again, each member comes back with the character class it entered the level with (as single player's restart loads the level's start), not the class it had at its death. A ship of the scripts that a member died in (not a ship it called, not the party's own ship) is removed with the death, so a mission that makes its ship again when the ship is gone does so (StealMayorShip's mayor's ship); on an instance an empty parking zone gives its own place to the scripts (the farthest parking zone search no longer gives the origin). Two members that come in at the same entrance get free places next to each other instead of standing on each other. In the other deals and missions only the death of every member in the level counts: when every member in the level is dead, the mission fails if it has a failure hook or the end of the game: the members get a notice and the level starts again 5 seconds later at the start of the mission, with the story as it was. Members cannot hurt each other. The super deals (CA, QF, OM, FZ) play as in single player, their score tables go on after 8 seconds.

At the end of the story the members in the last level see the credits and go back to the main menu, members elsewhere get a notice. A finished story keeps its variables. A member that joins again after that starts in DownTown. A new story needs a new data folder.

Start it with the mode Coop story in the Host world page, `ScrapC -host -mode:coop` or `ScrapCoord -mode:coop`. Its data folder is separate from the free worlds (`hostcoop` in the config folder for a hosting game). The levels are the city, Orbit, Fake, Junkyard, AB, ABIndoor, QF, OM, FZ and CA, the start level is Orbit.

## The original's PvP modes

The seven modes of the original run on the new protocol: DeathMatch, DeathMatch with bots, TeamDM, FlagHunt, TeamFlag, Postman and TeamPost. The rules are as the original counts them. The scores, the HUD, the end screen and the texts are the original's.

- A kill gives 1 point, a suicide or a team kill takes 1. In TeamDM the team gets it too.
- FlagHunt and TeamFlag: the one that touches the flag gets 1 (and its team), the flag moves to another place. A death costs a point.
- Postman and TeamPost: the carrier brings the mail bag from its start to a zone. That gives 1 point (TeamPost 10 and 1 for the team). A dead, leaving or spectating carrier drops the bag. A lone bag goes home after 60 seconds. Postman counts no kills, TeamPost counts the killer's.
- A round counts when two members with ships are there. It ends at the frag limit or the time limit. The end screen shows for 10 seconds, then the world goes to the next level of the map list for all players.
- A player has no ship at the connect. The join request makes the member with the garage ship (the default is SBetty1). The ship comes after Fire, or by itself after 10 seconds. Join in the smaller team. A late joiner gets the full state at once. A joiner during the end screen waits for the next round.
- In these modes the keys are Fire after a death (a new ship), the Scorer key held (the scores), F2 spectate on or off, F3 red team and F4 green team. F1 is the help of the mode.
- Players may hurt each other whatever the PvP setting says.
- The bots of DeathMatch with bots (the profiles of the original) only exist in levels with AI nodes. The small arenas have none, so the mode has no bots there.

Host it from the old Create page (mode, maps, players, frag limit), from the Host world page, with `ScrapC -host:FZ -mode:tdm -maps:FZ,OM`, or with ScrapCoord (below). The mode names for the command line are `dm`, `dmbots`, `tdm`, `flaghunt`, `teamflag`, `postman` and `teampost`.

## Places that single player keeps closed

In Sandbox and Free roam (not in the PvP modes and not in Coop story, where they are story levels) these places are open, with the level's own geometry but without the story. They are in `data/places/places.def`.

| Place | Way in | Life |
|-------|--------|------|
| Orbit | the Orbit elevator in TownHall (locked in single player), the Fake gate | five characters, standing |
| Fake | the Orbit gate "Lift", the Junkyard gate | 6 characters standing, 25 indoor traffic ships that move while a player is there |
| Junkyard | the gate "Junker" in Outskirts (the junker parking), the Fake gate | Rusty, maintenance, 3 Sputniks |
| AB | the "Teleporter" gate in GamblinDen | 12 traffic ships, police and the asteroids |
| ABIndoor | the same gate in GamblinDen | Human, Crazy Gambler, Berto and Betty, standing |
| QF, FZ, OM, CA | the "Deals" gate in GamblinDen | traffic and police ships |
| MCA, MCB, MDA, MDB, MOA, MOB, MSA, MSB | the "Deals2" gate in GamblinDen | traffic and police ships |

A gate is a use point with one option per target ("To <station name>"). Each place has a gate back. The small arenas have no AI graph of their own, so the server makes one from the level geometry when the level starts. Orbit's own elevator and the teleporters of GamblinDen, AB and ABIndoor do not work outside the story, so gates stand at them. The ship editor keypad of the Junkyard does not work in a world, but its platform rides to Outskirts. In a world of the `-mp2` server the keypad works: the editor opens on the client of the player who used it, and the server checks the hangar and money it sends back.

## Dedicated server

Build `ScrapCoord` with the game (cmake target `ScrapCoord`, next to `ScrapC`) and run it in a game folder that has `Bin/ScrapC`. The coordinator starts the instances there.

```
ScrapCoord -port:28100 -name:<world> -data:<folder> -gamedir:<game folder>
```

| Option | Meaning |
|--------|---------|
| `-port:<n>` | the port of the world (default 28100) |
| `-bind:<addr>` | numeric IPv4 or IPv6 address to bind (default any) |
| `-name:<text>` | the name in the lists (default Scrapland) |
| `-data:<folder>` | where it saves the players and the party (default `server`) |
| `-gamedir:<folder>` | the game folder of the instances (default `.`) |
| `-exe:<file>` | the ScrapC the instances run |
| `-mode:<mode>` | `sandbox` (default), `freeroam`, `coop` or a PvP mode |
| `-levels:<list>` | levels the world has (default: the city and the places, in coop its own list, in a PvP mode give the rotation) |
| `-keep:<list>` | levels that stay up (default the city, in coop the start level, `None` for none) |
| `-start:<level>` | the level new players start in (default DownTown) |
| `-maxplayers:<n>` | at most this many players (default 64, coop 8) |
| `-pvp:0\|1` | PvP on or off in Sandbox and Free roam (default 1) |
| `-money:<n>` | start money (default 2000) |
| `-grace:<s>` | seconds an empty level not kept up lives (default 60) |
| `-instport:<n>` | first port of the instances (default the world port plus 8) |
| `-instargs:"..."` | extra options for every instance (`-lives`, `-fraglimit` and so on) |
| `-list:<addr>`, `-listport:<n>` | register at that list server |
| `-listpublic:<addr>` | the address to list when it differs from the one the list server sees |
| `-stopinstances` | stop the instances at the end |

Commands on its standard input: `status`, `save`, `kick <name or key prefix>`, `start <level>`, `stop <level>`, `inst <level> <line>` and `quit`. A hosting game has the same commands in its console as `/coord <line>`.

Instances use the ports from the instance port on, and the LAN search asks the 8 ports from the world port on, so keep the instance ports outside that range (the defaults do). The log of each instance is in `<data>/logs/<Level>.log`. If an instance fails to start the coordinator tries again after 1, 2, 4, 8 and 16 seconds and then gives up until you run `start <level>`. If the coordinator restarts, it adopts the instances that still run. An instance that has no coordinator for 120 seconds ends.

### List server

```
ScrapList [-port:28090] [-bind:<addr>] [-ttl:<s>] [-peraddr:<n>] [-conns:<n>] [-key:<file>]
```

A coordinator with `-list` sends a signed heartbeat every 30 seconds (and when the players change). A world stays listed 90 seconds (`-ttl`) after its last heartbeat, and only after the list server could query the world at the address it listed. At most 16 worlds per address (`-peraddr`) and 1024 in all. Clients with `listserver=` in `scrapdecomp.ini` ask it. `-key` keeps the identity key of the list server in a file. Commands: `status` and `quit`.

### Saved progress

Each player is one file `<data>/players/<64 hex>.dat` named by the player key. It has the level, entrance, money, alarm, character, owned ship, faction, rank and deeds, wanted points, bounty, debt, permit and courier job. The coordinator writes it at travel and leave, every 120 seconds for changed players, and at the end. The write goes to a temp file, then the old file becomes `.bak`, then the rename. A bad file falls back to `.bak`, and when both are bad it is kept as `.corrupt` and the player starts new (logged). In Coop story each member also has `<key>.story`, and the party has `party.story` and `party.owner`.

A hosting game keeps its data in the config folder (`host`, and `hostcoop` for coop). On Linux that is under `~/.local/share/MercurySteam/Scrapland`. Instance tokens are in `<data>/instances.txt`, so keep the folder private.

## Command line

Players and hosts (`ScrapC`):

| Option | Meaning |
|--------|---------|
| `-join:<address>` | join a world at that coordinator address |
| `-port:<n>` | the coordinator port to join, or the port to host on |
| `-name:<text>` | the player name |
| `-headless` | no window, for bots and tests |
| `-host[:<level>]` | host a world, starting in the level (default DownTown); stays on loopback unless `-hostbind` |
| `-hostbind:<addr>` | the address a hosted world binds |
| `-hostkeep:<list>` | the levels that stay up |
| `-mode:<mode>` | the mode of a hosted world |
| `-maps:<list>` | the map rotation of a PvP mode |

The instance (`ScrapC -instance:<Level>`) takes the options below. A hosting game and ScrapCoord pass them on by themselves.

| Option | Meaning |
|--------|---------|
| `-id:<text>`, `-port:<n>`, `-bind:<addr>` | name, UDP port and address of the instance |
| `-maxplayers:<n>` | at most this many players (default 64) |
| `-mode:<mode>` | `sandbox`, `freeroam`, `coop` or a PvP mode |
| `-pvp:0\|1` | PvP (default 1) |
| `-money:<n>` | start money (default 2000) |
| `-ships:<n>` | called ships a player owns at once (default 1, at most 8) |
| `-respawn:<s>` | seconds from a death to the new character (default 3, 1 to 60) |
| `-lives:<n>` | lives (default 0, endless; with none left the player starts again with the start money) |
| `-rebuild:0\|1` | a ship that died with its owner costs its price at the next call (default 0) |
| `-spawn:<n>` | all players at that spawn point (tests; default spread) |
| `-policepvp:0\|1` | police and wanted players may hurt each other whatever PvP says (default 1) |
| `-factionpvp:0\|1` | hostile factions may hurt each other (default 1) |
| `-fraglimit:<n>`, `-timelimit:<minutes>` | end of a PvP round (time limit off by default) |
| `-maps:<list>`, `-endsecs:<s>` | rotation and length of the end screen (default 10) |
| `-story:<file>` | the party's story (coop, the coordinator passes it) |

A value with a blank, a dash or a colon is quoted for the game's parser by the launchers. An instance also reads `quit` on its standard input, a file `instance_<id>.quit` in the game folder, or SIGTERM. Its standard input takes the test lines of the scenario tools (`status`, `money`, `goto`, `faction`, `arena`, `story` and more, see [testing.md](testing.md)).

## Limits and gaps

Tested. The tests ran on one Linux machine, on loopback, with scripted test players (bots), and with up to four rendered clients at a time. 34 players in one level (32 bots and 2 rendered) and 16 players in an arena were run. 64 players in one level were not. The Windows build was tested under Wine as host and as dedicated server, with a Linux client.

Not tested.
- LAN discovery by the real broadcast and the IPv6 group. Every test sent the queries to loopback.
- Play over the internet, with real latency, NAT or a firewall.
- macOS, and Windows without Wine.
- The list server with many worlds and under attack (no limit of queries per address yet).
- A longer soak with many players travelling at once.

Coop story.
- Played through in coop with test players up to the mission Face the Mayor and ApologizeMayor only in part. The end of the game (the final fight, the credits) was run from a prepared story point with 2 and 8 players. About 50 missions between ApologizeMayor and the final fight were not played in coop, among them the super deals of OM and FZ (the ones of CA and QF were).
- The test players only press and aim. Real players, their fights and the races were not tested as much. The last blow on the final boss was done by a test line in the first runs. In the later ones the members' own weapons were enough, with a ship prepared for the test.
- The party's Sputnik is missing at an arrival in Outskirts from the Bank. A super deal that starts with a member in a ship that is not its own raised a script error once. The mission texts of the instance get the key names from the client, so a text that the instance sends without the client has an empty key name.
- The intro video adds about 70 seconds to the first level load of a rendered client.
- A client once stopped at a check when it connected to a level that the failed mission had restarted. It was not seen again.
- The remote calls of the story show only on the client of the member the mission acts for and on the ones near it.
- Chat and the original's server messages are not ported, there is no text chat in any mode.

Free roam and Sandbox.
- Without `-mp2`: no inventory, shop or price in the sandbox. One owned ship per player by default, no hangar of several ships. The player's character is Dtritus unless a faction changes it.
- Relations of factions have an effect only on who may hurt whom. Hits between hostile factions count as crimes.
- The news panels of the levels are not filled, the headlines are a HUD line. Contracts list only the players of the same level. The Press elevator took no rider in one test world and was not looked into.
- The desk arrow shows no text on the clients (the use works, the text is missing). The Sputnik has no use arrow text either.
- Not covered in the free activities: a member that dies during a super deal, the counter measure key during a race (it is bound for all players at once), and the AI race group of the rival ships, which is shared when two races run at the same time.
- The hook of a ship's laser is not replicated, so other players do not see the beam.
- Rail traffic runs on the server around every player and is replicated like other traffic. The client makes none of its own, and the bump handling is gone.
- Parked ships play their idle motor with a model chosen on each client, so the sound can differ between players.
- The places have only the life listed above (no missions), the teleporters of GamblinDen, AB and ABIndoor are replaced by gates, and the generated AI graphs of the small arenas hold at most 4096 points.
- The old PvP modes: the in-game menu has no Spectate and Team entries (use the keys). A spectator has a free camera at the first spawn, not one that follows players. The arena items (life, missiles) are the server's, and the client's own copies do not vanish when someone takes one. The Advanced options of the old Create page (password, forced ship, data rate) do nothing on the new network. Bots work only in levels with AI nodes.
- The coordinator restarts an instance that fails at once with a backoff (up to 5 tries), and a mode that crashes on a level needs a look at its log.
- A ticket is bound to the player key and valid for 120 seconds, but not to the connection of the client. The list server's identity key is not pinned by clients and coordinators.
- A join from the LAN page or by address does not pin the world's key. A join from the list pins it.
- A world's Abort game goes to the main menu, not to the page of the worlds.

Where a world differs from single player on purpose: a world has one level per process and a travel is a new connection, the missions of single player do not run in Sandbox and Free roam, and a player is not the only human, so scripts that read "the player" see the member bound at that moment.
