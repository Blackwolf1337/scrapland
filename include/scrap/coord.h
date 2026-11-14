/*
 * The coordinator of a multiplayer world (stage M5 of the multiplayer plan, src/coord). Not in the original.
 *
 * A world is one coordinator plus one headless instance process per level (include/scrap/instance.h). Clients connect
 * to the coordinator first (Net2, its port): it knows the players by their public keys (an identity key pair each
 * client makes on its first start: Net2_KeyPublic, Net2_Sign), starts and stops the instances on demand on loopback
 * ports after its own, keeps the levels of the city up, ends an empty other one after a grace time, keeps every
 * player's progress in a file store under the server's data folder (CoordStore), and answers LAN discovery and server
 * queries (Net2 does that for any server: the info names the world and the start level, mode COORD_MODE).
 *
 * Joining: hello (name) -> challenge (random nonce) -> join (public key, the signature of the nonce, the coordinator's
 * identity key and the name) -> the coordinator loads the player's progress and gives the client a ticket for the
 * instance of its level (started first when it is not up). A ticket (CoordTicket) is signed with the coordinator's
 * identity key; the instances get the public key on their command line, so they check a ticket without asking the
 * coordinator. It holds the player's key, the level and the entrance (the spawn dummy and the door the player comes
 * through), the times it is valid between, a nonce (a ticket is taken once) and the player's state (CoordKv text:
 * money, alarm, character, owned ship, the save variables of the player).
 *
 * Travel: a level change of a player inside an instance (an elevator, the metro) does not load a level; the instance
 * sends the player's state with the target level and entrance to the coordinator (a state message of reason
 * COORD_SR_TRAVEL), which stores it, makes sure the target instance runs and sends the client a ticket for it; the
 * client connects there with the ticket (the level loads with the loading screen of a level change). The instances
 * report each player's state on join, on leave and every COORD_REPORT_SECS; the coordinator saves a changed player at
 * once on travel and leave and the others every COORD_SAVE_SECS.
 *
 * The instances connect to the coordinator over Net2 (loopback) with the token their start gave them (the environment
 * variable COORD_TOKEN_ENV); the coordinator writes the instances it runs into its data folder, so a coordinator that
 * starts again takes the running ones over (they connect again by themselves) and players keep playing. An instance
 * that has no coordinator for COORD_ORPHAN_SECS ends.
 *
 * Wire formats (little endian; strings u8 length + bytes; the reliable channel 0 of the Net2 default layout):
 *   client <-> coordinator:
 *     'h' hello: version u16, name str
 *     'c' challenge: version u16, nonce 32, world name str
 *     'j' join: key 32, signature 64, flags u8 (COORD_JF_*), port u16 (COORD_JF_RESUME: the instance it plays in)
 *     't' ticket: port u16, level str, ticket u16 length + bytes (the instance on the coordinator's address), the
 *         world's mode str (an older coordinator's message ends before it)
 *     'd' deny: reason u8 (COORD_DENY_*), text str (the coordinator disconnects after it)
 *     'n' notice: text str
 *   instance <-> coordinator:
 *     'i' instance: version u16, token 32, id str, level str, port u16, pid u32
 *     's' state: reason u8 (COORD_SR_*), key 32, level str, entrance str, door str, state u16 length + bytes
 *     'k' kick: key 32, reason u8
 *     'r' refuse: key 32, text str (a travel that cannot go: the instance gives the player back)
 *     'x' console: text str (a line for the instance's console, as its standard input takes them)
 *     'v' story (coop, both ways): count u16, then each change: kind u8 (COORD_OP_*), name str, (set) value u16
 *         length + bytes; the coordinator applies an instance's changes to the party's story and sends them on to the
 *         other instances of the world
 *     'w' story (instance -> coordinator, coop): as 'v', the changes made while the instance's level started (its own
 *         level is not stale by them)
 *     'V' whole story (coordinator -> instance, coop): text u32 length + bytes (CoordKv text of the save variables)
 *     'g' summon (coordinator -> instance, coop): key 32, level str, entrance str, door str (the player travels
 *         there with its party)
 *     'y' party notice (instance -> coordinator, coop): text str (for the members in the other instances)
 *   coordinator -> client (coop):
 *     'o' cue: kind u8 (COORD_CUE_*), text str
 *   client <-> instance (the instance connection's reliable channel, beside the replication's messages):
 *     'T' ticket: u16 length + bytes (before the hello of the replication)
 *     'M' metro: door str, count u8, stations str each (the station map of a metro door the player used)
 *     'K' metro pick: station str ("" cancels)
 * A ticket: magic "STK1", key 32, level str, entrance str, door str, issued u64, expires u64 (ms of the Unix epoch),
 * nonce 16, state u16 length + bytes, then the EdDSA signature (64) of everything before it.
 * A player file (CoordStore): the line "scrapplayer 1", the state text, the line "sum <32 hex digits>" (BLAKE2b-128 of
 * the bytes before that line); written as <key>.tmp, the old one renamed to <key>.bak, then the new one renamed to
 * <key>.dat, so a crash leaves the old or the new file whole and a bad file falls back to the one before.
 */
#ifndef SCRAP_COORD_H
#define SCRAP_COORD_H

#include <stddef.h>
#include <stdint.h>

#include "scrap/net2.h"

#define COORD_VERSION 1
#define COORD_MODE "sandbox"			/* the mode of the server info */
#define COORD_INFO_WORLD 0x01			/* server info flag: a coordinator (join here) */
#define COORD_INFO_DEDICATED 0x02
#define COORD_NONCE 16
#define COORD_CHALLENGE 32
#define COORD_NAME 32					/* bytes of a player or world name */
#define COORD_LEVEL 48					/* bytes of a level, entrance or door name */
#define COORD_STATE_MAX 8192			/* bytes of a player's state text */
#define COORD_TICKET_MAX (COORD_STATE_MAX + 4 + NET2_KEY_SIZE + 3 * (1 + COORD_LEVEL) + 16 + COORD_NONCE + 2 + NET2_SIG_SIZE)
#define COORD_TICKET_LIFE 120000		/* ms a ticket is valid (the level load of the client comes before it is used) */
#define COORD_SKEW 10000				/* ms a ticket may come from the future of the instance's clock */
#define COORD_REPORT_SECS 60			/* an instance reports its players this often */
#define COORD_SAVE_SECS 120				/* the coordinator saves changed players this often */
#define COORD_ORPHAN_SECS 120			/* an instance without its coordinator this long ends */
#define COORD_GRACE_SECS 60				/* an empty instance of a level not kept up ends after this */
#define COORD_START_SECS 90				/* an instance that does not connect in this time is stopped */
#define COORD_STOP_SECS 10				/* an instance asked to stop is killed after this */
#define COORD_TOKEN_ENV "SCRAP_COORD_TOKEN"
#define COORD_PATH 260
#define COORD_MAX_LEVELS 32
#define COORD_CITY "DownTown,Outskirts,Police,Bank,Press,Temple,TownHall,GamblinDen,Commercial,Scrapyard,GDB"
#define COORD_LIST_PORT 28090			/* the default port of a list server (include/scrap/coordlist.h) */

/* message types */
#define COORD_MSG_HELLO 'h'
#define COORD_MSG_CHALLENGE 'c'
#define COORD_MSG_JOIN 'j'
#define COORD_MSG_TICKET 't'
#define COORD_MSG_DENY 'd'
#define COORD_MSG_NOTICE 'n'
#define COORD_MSG_INST 'i'
#define COORD_MSG_STATE 's'
#define COORD_MSG_KICK 'k'
#define COORD_MSG_REFUSE 'r'
#define COORD_MSG_CONSOLE 'x'
#define COORD_MSG_ITICKET 'T'
#define COORD_MSG_METRO 'M'
#define COORD_MSG_PICK 'K'
#define COORD_MSG_STORY 'v'
#define COORD_MSG_STORYALL 'V'
#define COORD_MSG_STORYINIT 'w'
#define COORD_MSG_SUMMON 'g'
#define COORD_MSG_PNOTICE 'y'
#define COORD_MSG_CUE 'o'
#define COORD_MSG_LEADER 'l'			/* instance -> coordinator (coop): key: the member goes to its party's leader */
#define COORD_MSG_RESET 'z'			/* instance -> coordinator (coop): its level starts again for the members in it (they
										 * travel to it; no ticket until the new start) */

#define COORD_JF_RESUME 0x01			/* the client plays in an instance already (the coordinator came back) */

/* deny reasons, also the Net2 disconnect reason NET2_DR_APP + reason */
#define COORD_DENY_VERSION 1
#define COORD_DENY_SIGNATURE 2
#define COORD_DENY_PLAYING 3			/* the key is online already */
#define COORD_DENY_FULL 4
#define COORD_DENY_STORE 5				/* the progress could not be read */
#define COORD_DENY_LEVEL 6				/* no instance for the level */
#define COORD_DENY_TICKET 7				/* instance: no valid ticket */
#define COORD_DENY_KICK 8

/* state reasons */
#define COORD_SR_JOIN 1					/* the player arrived (its ticket taken) */
#define COORD_SR_REPORT 2				/* every COORD_REPORT_SECS, and for all players when the coordinator came back */
#define COORD_SR_LEAVE 3
#define COORD_SR_TRAVEL 4
#define COORD_SR_PARTY 5				/* coop: a travel of the story that takes every member of the party along */

/* Coord_TicketOpen */
#define COORD_TK_OK 0
#define COORD_TK_BAD 1					/* malformed */
#define COORD_TK_SIGNATURE 2
#define COORD_TK_EXPIRED 3
#define COORD_TK_EARLY 4				/* issued too far in the future */

/* CoordStore_Load */
#define COORD_LD_OK 0
#define COORD_LD_NEW 1					/* no file: a new player */
#define COORD_LD_BACKUP 2				/* the file was bad, the one before it was read */
#define COORD_LD_CORRUPT 3				/* both bad: moved aside as <key>.corrupt, a new player */
#define COORD_LD_IO 4					/* could not be read (permissions, ...) */

/* ---- the state text: one "key=value" a line; keys of letters, digits and "._-"; values with "\\" and "\n" escaped ---- */

int CoordKv_KeyOk(const char *key);
/* the value of key (unescaped) into out: 1 found, 0 not (out ""); a value longer than cap is cut */
int CoordKv_Get(const char *text, int len, const char *key, char *out, int cap);
/* key set to value (replaced or added at the end): 0 for a bad key or no room (text unchanged) */
int CoordKv_Set(char *text, int *len, int cap, const char *key, const char *value);
/* the pair at *pos and *pos after it: 1 with a pair, 0 at the end; malformed lines are skipped */
int CoordKv_Next(const char *text, int len, int *pos, char *key, int kcap, char *val, int vcap);
/* the pairs of from set into text: 0 when one did not fit */
int CoordKv_Merge(char *text, int *len, int cap, const char *from, int fromLen);
/* every line is a pair with a good key */
int CoordKv_Valid(const char *text, int len);

typedef void (*coordlogfn_t)(const char *fmt, ...);

/* ---- tickets ---- */

typedef struct CoordTicket {
	uint8_t key[NET2_KEY_SIZE];		/* the player's public key */
	char level[COORD_LEVEL + 1];	/* "DownTown" */
	char entrance[COORD_LEVEL + 1];	/* the spawn dummy ("DM_Player_Spawn_TownHall"), "" the players' spawn */
	char door[COORD_LEVEL + 1];		/* the class of the door it came through ("DoorElevatorTownHall"), "" */
	uint64_t issued, expires;		/* ms of the Unix epoch */
	uint8_t nonce[COORD_NONCE];
	int stateLen;
	char state[COORD_STATE_MAX + 1];
} CoordTicket;

/* the signed ticket into out: its size, 0 when it does not fit */
int Coord_TicketSeal(const CoordTicket *t, const uint8_t seed[NET2_SEED_SIZE], uint8_t *out, int cap);
/* a ticket checked against the coordinator's public key at the time now (ms of the Unix epoch): COORD_TK_* */
int Coord_TicketOpen(const uint8_t *data, int size, const uint8_t pub[NET2_KEY_SIZE], uint64_t now, CoordTicket *out);

/* the text of a deny reason (also of the disconnect reason NET2_DR_APP + reason) */
const char *Coord_DenyText(int reason);

/* the hash a client signs to join: the coordinator's challenge, its identity key and the player's name */
void Coord_JoinHash(uint8_t hash[64], const uint8_t nonce[COORD_CHALLENGE], const uint8_t serverKey[NET2_KEY_SIZE],
	const char *name);

/* ---- the player store ---- */

/* the progress of the player of key from <dir>/<hex key>.dat (or its .bak): COORD_LD_*; text and *len "" for a new
 * player */
int CoordStore_Load(const char *dir, const uint8_t key[NET2_KEY_SIZE], char *text, int *len, int cap);
/* written atomically (see above): 1 ok */
int CoordStore_Save(const char *dir, const uint8_t key[NET2_KEY_SIZE], const char *text, int len);

/* ---- any text in the store's file format (the coop story): <base>.<ext>, the one before <base>.<ext>bak, a bad
 * one kept as <base>.<ext>corrupt; COORD_LD_*, and 1 / 0 for the save ---- */

int CoordStore_LoadText(const char *base, const char *ext, char *text, int *len, int cap);
int CoordStore_SaveText(const char *base, const char *ext, const char *text, int len);

/* ---- the coop story (stage M7a, src/coord/Party.c) ----
 * Coop (-mode:coop): one party per world, at most COORD_COOP_MAX players. The story (the save variables of the single
 * player game: missions, flags, plans, hangar) belongs to the party: one CoordKv text kept by the coordinator
 * (<data>/party.story), changed by the instances (their changes come as story messages), sent on to every other
 * instance of the world, given to an instance that starts (a file named by -story) and saved into each member's
 * store as a copy (<players>/<key>.story). A member that joins with a copy unlike the party's takes the party's
 * story; its copy stays as <key>.own (the one before as <key>.ownbak). A party without a story takes the copy of
 * its first member; a new party starts the single player's new game (the instance of the start level makes it). The
 * story owner is the first member (<data>/party.owner). */

struct NetRepBuf;
struct NetRepReader;

#define COORD_STORY_MAX 32768			/* bytes of the story text */
#define COORD_STORY_VALUE 4096			/* bytes of one value (the save game's limit) */
#define COORD_COOP_MAX 8
#define COORD_COOP_START "Orbit"
#define COORD_COOP_LEVELS COORD_CITY ",Orbit,Fake,Junkyard,AB,ABIndoor,QF,OM,FZ,CA"
#define COORD_COOP_AFTER "DownTown"		/* where a member of a finished story starts that left in a chapter level */
#define COORD_COOP_FINISHED "Coop.Finished"	/* the story's variable of its end ("yes") */
#define COORD_OP_SET 0
#define COORD_OP_DEL 1
#define COORD_OP_ADD 2					/* a counter: the value is a whole number added to the story's (0 when missing) */
#define COORD_CUE_INTRO 1				/* a new party's start: the intro video (text: its file) */
#define COORD_CUE_NOTICE 2				/* a line on the screen (text) */
#define COORD_CUE_COOP 3				/* the world plays the coop story (at each join, before the ticket) */
#define COORD_STORY_MISSION 0x01		/* CoordStory_Apply: Mission.Map or Mission.Library changed */
#define COORD_STORY_ADDED 0x02			/* a counter was added to (the sender gets the sum too) */

int CoordKv_Del(char *text, int *len, const char *key);
/* one change of a story message: 1 read (kind, name, value), 0 at the end of count, -1 malformed */
int CoordStory_Next(struct NetRepReader *r, int *kind, char *name, int ncap, char *val, int vcap);
/* the changes of a story message (from its count on) applied to text: the number applied, -1 malformed; *flags
 * COORD_STORY_*; out (NULL: none) gets the count and the changes applied as the instances take them (a counter's
 * addition as the value of the sum) */
int CoordStory_Apply(char *text, int *len, int cap, const uint8_t *msg, int size, int *flags, struct NetRepBuf *out);
/* one change written (value NULL for COORD_OP_DEL) */
void CoordStory_Put(struct NetRepBuf *w, int kind, const char *name, const char *value);
/* s is a whole number (an optional sign and digits, nothing else): 1 with *out */
int CoordStory_Int(const char *s, long *out);

typedef struct CoordParty {
	char base[COORD_PATH];			/* <data>/party */
	char playersDir[COORD_PATH];
	char owner[2 * NET2_KEY_SIZE + 1];
	uint32_t seq;					/* changes so far (the members' copies follow it) */
	int dirty;
	int len;
	char story[COORD_STORY_MAX];
} CoordParty;

/* the party of the data folder loaded (1), 0 when its file could not be read */
int CoordParty_Load(CoordParty *p, const char *dataDir, const char *playersDir, coordlogfn_t log);
/* a member joins: the party's story, its copy kept (see above); the first member becomes the owner */
void CoordParty_Join(CoordParty *p, const uint8_t key[NET2_KEY_SIZE], coordlogfn_t log);
/* the party's story into the member's copy, the party's file when dirty: 1 ok */
int CoordParty_SaveMember(CoordParty *p, const uint8_t key[NET2_KEY_SIZE]);
int CoordParty_Save(CoordParty *p);
/* the story as a plain CoordKv file for an instance's -story (absolute path into out): 1 ok */
int CoordParty_StoryFile(const CoordParty *p, const char *dataDir, char *out, int cap);
/* the story came to its end (COORD_COOP_FINISHED) */
int CoordParty_Finished(const CoordParty *p);
/* a level of the story alone (Orbit, Fake, Junkyard, AB, ABIndoor): a level change into or out of one is a chapter
 * that takes the whole party along */
int CoordCoop_Chapter(const char *level);

/* ---- system helpers (src/coord/Sys.c) ---- */

uint64_t Coord_UnixMs(void);
void Coord_Hex(const uint8_t *b, int n, char *out);			/* 2n digits and a zero */
int Coord_SameName(const char *a, const char *b);			/* equal but for the case of ASCII letters */
int Coord_Unhex(const char *s, uint8_t *b, int n);			/* exactly 2n hex digits: 1 */
int Coord_MkDir(const char *path);							/* 1 when it exists afterwards */
/* the whole file into buf (zero added): its size, -1 missing, -2 other error or larger than cap - 1 */
int Coord_ReadFile(const char *path, char *buf, int cap);
/* path written through a temporary file and renamed over it (keep: the old file renamed to bak first): 1 ok */
int Coord_WriteAtomic(const char *path, const void *data, int size, const char *bak);
/* the 32 byte seed in a hex file, made (random, mode 0600) when it is missing: 1 ok */
int Coord_LoadSeed(const char *path, uint8_t seed[NET2_SEED_SIZE], int *made);
/* the absolute path of an existing path: 1 ok */
int Coord_AbsPath(const char *path, char *out, int cap);

/* ---- the console of the dedicated programs ScrapCoord and ScrapList (src/coord/Cli.c) ---- */

uint64_t CoordCli_Ms(void);							/* a monotonic clock */
/* a line of the standard input when one is complete, waiting up to ms for it: 1 with a line, 0 none yet, -1 at its
 * end */
int CoordCli_Line(char *line, int cap, int ms);
/* the value of the argument a when it is -name:value, else NULL */
const char *CoordCli_Arg(const char *a, const char *name);
void CoordCli_Wait(int ms);							/* the pause between the passes of a program without input */

/* ---- child processes (src/coord/Proc.c): never waits ---- */

typedef struct CoordProc {
	intptr_t handle;				/* POSIX: the pid; Windows: the process handle; 0 none */
	uint32_t pid;
	int adopted;					/* not our child (a coordinator before this one started it) */
} CoordProc;

/* exe with argv (argv[0] the program, NULL ended) in the folder cwd, stdout and stderr appended to logPath, the
 * environment plus envName=envValue; 1 started */
int CoordProc_Start(CoordProc *p, const char *exe, const char *const *argv, const char *cwd, const char *logPath,
	const char *envName, const char *envValue);
/* a running process of an earlier coordinator: 1 when it runs */
int CoordProc_Adopt(CoordProc *p, uint32_t pid);
/* 1 while it runs (an ended child is reaped) */
int CoordProc_Alive(CoordProc *p);
void CoordProc_Kill(CoordProc *p);
/* the path of this program */
void CoordProc_Self(char *buf, int size);

/* ---- messages: raw bytes on the byte writer and reader of include/scrap/netrep.h (src/coord/Ticket.c) ---- */

struct NetRepBuf;
struct NetRepReader;
void CoordMsg_PutBytes(struct NetRepBuf *w, const void *b, int n);
/* n bytes into b (zeros when the message is short: the reader is bad then) */
void CoordMsg_GetBytes(struct NetRepReader *r, void *b, int n);

/* ---- the coordinator (src/coord/Coord.c) ---- */

typedef void (*coordlog_t)(const char *text);

typedef struct CoordConfig {
	uint16_t port;					/* of the coordinator; the instances from instPort on */
	uint16_t instPort;				/* 0: port + NET2_LAN_PORTS (outside the ports of LAN discovery) */
	char bind[48];					/* numeric IPv4 or IPv6 address of the coordinator, "" any */
	char name[COORD_NAME + 1];		/* of the world (server info) */
	char dataDir[COORD_PATH];		/* players/, logs/, coordinator.key, instances.txt */
	char gameDir[COORD_PATH];		/* the game folder (Bin/ScrapC in it), the instances run in its Bin */
	char exe[COORD_PATH];			/* the ScrapC the instances run, "" gameDir/Bin/ScrapC */
	char levels[512];				/* the levels a player may go to, comma separated, "" the city and the places */
	char keep[512];					/* the levels kept up, "" the city */
	char start[COORD_LEVEL + 1];	/* where a new player starts, "" DownTown */
	char instArgs[256];				/* more arguments for the instances (space separated) */
	char mode[16];					/* of the server info, "" COORD_MODE; another one goes to the instances as -mode */
	char listAddr[64];				/* numeric address of the list server to register with, "" none */
	uint16_t listPort;				/* 0 COORD_LIST_PORT */
	char listPublic[48];			/* the address the list server lists (only a list server on loopback takes it), "" the
									   one it sees */
	int maxPlayers;
	int pvp, money;					/* -pvp and -money of the instances */
	int grace;						/* s, 0 COORD_GRACE_SECS */
	int saveSecs;					/* s, 0 COORD_SAVE_SECS */
	int dedicated;
	int stopInstances;				/* Coord_Destroy stops them (in-game hosting) or leaves them to the next one */
	coordlog_t log;					/* NULL: stdout */
} CoordConfig;

typedef struct Coord Coord;

void Coord_DefaultConfig(CoordConfig *cfg);
Coord *Coord_Create(const CoordConfig *cfg);	/* NULL: the reason went to the log */
/* one pass: the network, the instances, the saves; never waits. now: ms of a monotonic clock */
void Coord_Pass(Coord *c, uint64_t now);
/* a console line: status, save, kick <name|hex>, stop <level>, start <level>, inst <level> <line> (a console line
 * for that instance); 0 for an unknown one */
int Coord_Command(Coord *c, const char *line);
void Coord_Status(Coord *c);
/* all players saved, the instances stopped (cfg.stopInstances) or left running, the server closed */
void Coord_Destroy(Coord *c);
/* the coordinator's identity key (the instances check tickets with it) */
void Coord_PublicKey(const Coord *c, uint8_t pub[NET2_KEY_SIZE]);
uint16_t Coord_Port(const Coord *c);
/* the address a program on this machine reaches a server bound to bind at: bind itself, or the loopback of its family
 * for "" and the any addresses */
void Coord_Loopback(const char *bind, char *out, int size);

#endif
