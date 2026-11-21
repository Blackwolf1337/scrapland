/*
 * The coop story's state shared by Coop.c (story sync, the party, travel), CoopOld.c (the old path's forwarding
 * and menus), CoopTest.c (the automatic test player) and CoopClient.c (the old path's client). Private to these
 * files. Not in the original.
 */
#ifndef SCRAP_APP_COOPINT_H
#define SCRAP_APP_COOPINT_H

#include "scrap/coop.h"
#include "scrap/netrep.h"
#include "scrap/players.h"

#define CP_AMMO_TRIED 6				/* test: ammo boxes remembered as taken */
#define CP_FLY_ON 500.0f			/* test: a flight comes this much nearer within CP_FLY_STUCK */

#define CP_STYPES 64				/* items of the HUD's scorer whose class is kept */
#define CP_SCORER_MSG 6144			/* bytes of the mission's items of the HUD */
#define CP_VOICES 4
#define CP_OPTS_MSG (8 + CP_NAME + COOP_OPTS * (COOP_OPT_TEXT + 2))
#define CP_TABLES 4					/* call tables of one scene (its Load table and the actors' after it) */

#define CP_CDIRTY 256				/* changed variables of the story for the clients between two ticks */
#define CP_GONE 128					/* entities deleted by the level's start told to the clients */
#define CP_GONE_MSG (8 + CP_GONE * (CP_NAME + 2))

#define CP_NAME 64					/* bytes of a name of the story (CoordKv's keys) */
#define CP_SCENE_MSG 8192			/* bytes of a scene's table for the clients */
#define CP_SCENE_LINES 256
#define CP_MENU_MSG 8192			/* bytes of a menu message (the hangar's save variables) */
#define CP_MENU_TITLE 1024
#define CP_HANGAR "Hangar."			/* the save variables of the ship editor */
/* rules of data/coop/rules.def (include/scrap/party.h) */

/* the lists of rules.def: local (the variables of a player, not of the story: where it goes on, its character, money
 * and alarm, its time), hud (the variables of the story the HUD shows: the clients get them and their globals of the
 * same name), clientnot (the variables of the story the clients' copies leave out: the mission, the auto save of a
 * level's start, the instance's level start message), entry (the way point of a chapter's travel, given to the start
 * of the target level) */

typedef struct CpScene {
	char cls[CP_NAME];
	const char *module;
	int ntables;
	const char *const *lines[CP_TABLES];
	char dataModule[CP_NAME], dataSection[CP_NAME];	/* the level data section that loads it, "" */
} CpScene;

typedef struct CoopState {
	byte on;
	char story[COORD_PATH];			/* the file of -story */
	char levelChar[64];				/* the character the level's scripts made the player (Coop_SetChar), "" none */
	byte applying;					/* the coordinator's story goes in: not sent back */
	byte synced;					/* the whole story came since the coordinator connected */
	byte overflow;					/* more changes than the list holds: the whole story goes */
	byte levelSent;					/* the changes of the level's start went (COORD_MSG_STORYINIT) */
	byte hold;						/* test: the changes wait */
	byte started;					/* the level's first tick ran */
	byte waiting;					/* the world waits for its first member (its time stands) */
	cEntity *standIn;				/* the scripts' player of the level's start without a member (Coop_StandIn) */
	cEntity *standInKept;			/* that entity, kept in the world for the next start (its name in standInName) */
	char standInName[CP_NAME];
	int ndirty;
	struct {
		char name[CP_NAME];
		byte counter;				/* a counter (Stats.*, or SaveVar_Inc): its change goes as an addition */
		byte hasBase;
		long base;					/* its number when it became dirty (0 for a new one) */
	} dirty[COOP_DIRTY];
	int nscenes;
	CpScene scenes[COOP_SCENES];
	int loading;					/* the scene whose Load table ran last (its later tables join it), -1 */
	char dataModule[CP_NAME], dataSection[CP_NAME];	/* the level data section that runs, "" */
	byte inScene;					/* a scene of the scripts plays */
	int scene;						/* its table (the clients play it), -1 none */
	int scenePlayer;				/* the member current when it started */
	uint32_t serial;				/* scenes started so far */
	uint32_t endingSerial;			/* the one whose end runs (its callback may start the next) */
	int endingTable;
	char endingCls[CP_NAME], sceneCls[CP_NAME];
	char voice[CP_VOICES][128];		/* the file opened on each voice channel */
	struct {
		uint8_t sent[CP_OPTS_MSG];	/* the options told to the member last */
		int sentSize;
		char choiceEnt[CP_NAME];	/* the entity whose option the member chose, "" */
		int choice;
		byte menu;					/* the menu it answers (COOP_MENU_*), 0 */
		char held[CP_NAME];			/* its character the scripts took (UsrControl -1), "" */
		char yes[CP_NAME], no[CP_NAME];
		uint16_t press;				/* test: buttons pressed (NETREP_IN_*) for pressTicks more ticks */
		int pressTicks;
		byte pressPulse;			/* the press comes once in four ticks */
		float race;					/* test: its ship flies to the race's checkpoint (0 off) */
		vec3_t raceAt;				/* where that checkpoint was */
		double raceSince;			/* since when (instance time) */
		vec3_t flyTo;				/* test: the point of the level's AI graph its ship flies to next */
		double flyAt;				/* when it looked at its way last */
		float flyBest;				/* the nearest it came to its goal since flyMoved */
		double flyMoved;			/* when it came CP_FLY_ON nearer last (a stuck flight is placed on) */
		char flyGoal[CP_NAME];		/* the entity it flies to ("" none) */
		double shipAsk;				/* since when it asks for a ship for a far goal outdoors (0 not) */
		byte fight;					/* test: it fights the targets of fightList */
		byte follow;				/* test: the fight only stays with its target (no shot: an escort, a ship to follow) */
		int spare;					/* test: no shot at a ship whose life is at or under this percent of its most (0 off) */
		double carryAt;				/* test: when the messenger last put itself with its pack at the pack's goal */
		byte tough;					/* test: its entity takes no damage (a party of one against a police wave: TransportKiller) */
		byte scan;					/* test: the mission's ships that are no foes are followed, not shot (a scan) */
		byte waypoint;				/* test: the item of the scripts' waypoint is taken before the fights (CTF's flags) */
		byte walk;					/* test: the automatic player on foot steps to its target (followers keep up) */
		char follower[CP_NAME];		/* the follower it saw last ("" none) */
		vec3_t walkTo;				/* the point of the level's AI graph it walks to next */
		vec3_t walkFrom;			/* where it was at walkMoved */
		double walkMoved;			/* when it had come further last (a stuck walk is placed on) */
		double walkWait;			/* since when it waits for its follower (0 not) */
		double walkTold;			/* when its walk was told last */
		double walkBack;			/* since when it is next to its last follower that follows no more (0 not) */
		float walkNear;				/* the nearest it came to its target on its walk */
		double walkNearAt;			/* when (0 none yet) */
		double usedAt;				/* when its auto play last used its target (a talk that starts after it) */
		char walkShort[64];			/* the target its walk ended short of (the goto and the use come next) */
		char fightList[CP_NAME];	/* "Module.var" (a list of the scripts), an entity, "" the mission's targets */
		char fightAt[CP_NAME];		/* the target it turned to last */
		double fightSince;			/* when it was put near a target last (instance time) */
		double fightTold;			/* when its state was told last */
		double lockAt;				/* since when its homing missiles' lock is on the target */
		uint32_t useless;			/* the weapons that did not hurt its target (bits by weapon id) */
		uint32_t useful;			/* the ones that did */
		char ammoTried[CP_AMMO_TRIED][CP_NAME];	/* the ammo boxes it took last (the oldest replaced) */
		int ammoNext;
		double ammoAt;				/* when it was put at a box last */
		double lifeBoxAt;			/* when it was put at a life box last */
		int lifeWeap;				/* the weapon of the watch of the target's life (-1 none) */
		float lifeMark;				/* the target's life when that watch began */
		double lifeSince;			/* when */
		char possess[CP_NAME];		/* test: the character it possesses ("" none) */
		double possessSince, possessAt;	/* since when, when it was put next to it last */
		byte autoOn;				/* test: it plays the mission's targets on its own (fights, races, uses) */
		char autoAt[CP_NAME];		/* the target it went to last */
		char autoList[CP_NAME];		/* what it fights when the mission shows nothing to do ("" nothing; as fightList) */
		double autoSince, autoUse;	/* when, when it uses it (0 done) */
		double autoTold;			/* when its new target was told last */
		int autoTurn;				/* its uses so far: the next of several targets */
		uint16_t purseJoined;		/* the join whose money is the party's purse (Cp_Purse) */
		int purse;					/* the purse it got last */
		char upTo[CP_NAME];			/* test: its fights hold fire while this variable of the scripts is at upToMax ("" off) */
		int upToMax;
		double talkUntil;			/* a conversation of its own plays until then (instance time) */
		vec3_t autoItemAt;			/* where the item it went onto last was */
		char walkWho[CP_NAME];		/* test: the one follower its walk waits for ("" any) */
	} pl[PLAYERS_MAX];
	struct {						/* the items of the HUD's scorer made, by name (their class for the clients) */
		char name[CP_NAME], type[16];
	} stype[CP_STYPES];
	int nstype, nsbase, nsitems;
	char cdirty[CP_CDIRTY][CP_NAME];	/* changed variables of the story for the clients' copies */
	int ncdirty;
	byte coverflow;					/* more than that: the whole story goes */
	char sbase[CP_STYPES][CP_NAME];	/* the standard items of the HUD (before the mission's call) */
	uint8_t ssent[CP_SCORER_MSG];	/* the mission's items told last */
	int ssentSize;
	uint64_t ssentAt, slookAt;		/* when they were told, looked at (instance ms) */
	uint8_t cscore[CP_SCORER_MSG];	/* client: the instance's mission items (again when the HUD lost them) */
	int cscoreSize;
	char cnames[COOP_SCORER_ITEMS][CP_NAME];	/* client: the items it made of them */
	int ncnames;
	char targets[COOP_TARGETS * (CP_NAME + 4)];	/* the targets told last (names and slots), "" */
	uint64_t targetsAt;				/* when (instance ms) */
	char ctargets[COOP_TARGETS * CP_NAME];	/* client: the names of its copies it set as the targets */
	int missionPlayer;				/* the mission's member here (Cp_MissionPlayer), -1 */
	int pilot;						/* the member the parking's mission callbacks of boarding ran for last, -1 */
	uint16_t missionJoined;			/* its join */
	int remote;						/* a remote communication of the scripts is told (Coop_RemoteCom, nested) */
	int sceneWas;					/* the context before the scene's member entered for the scene's end */
	int remoteWas;					/* the context before it */
	byte partyDead;					/* every member here died: the single player's death runs (Coop_Died) */
	char gone[CP_GONE][CP_NAME];	/* the entities the level's start deleted (the mission's removals), for the clients */
	int ngone;
	int starting;					/* the level's Init runs (Coop_StandIn, nested) */
	uint8_t cgone[CP_GONE_MSG];		/* client: the instance's list, to delete from its level once it runs */
	int cgoneSize;
	double failAt;					/* a failed mission: the level starts again then (instance time), 0 */
	int conv;						/* the member in a character conversor, -1 */
	char convMachine[CP_NAME];		/* that conversor */
	byte convAsked;					/* its interface is on the member's client (the machine waits) */
	uint32_t mainChars;				/* the main characters of the scripts made so far (their names) */
	char mainChar[CP_NAME];
	char via[CP_NAME];				/* the function of a menu's answer that runs ("" none) */
	struct {						/* client: the options of the instance */
		char ent[CP_NAME];
		int n, cur;
		char opt[COOP_OPTS][COOP_OPT_TEXT];
	} copt;
	char clientScene[CP_NAME];		/* client: the scene it plays for its instance */
	int pendingSize;				/* client: a scene message that waits for the client's own level to run */
	uint8_t pending[CP_SCENE_MSG];
	byte skipped;					/* client: it ended here (Escape, or its last frame before the instance's) */
	byte aborting;					/* client: Escape ends it */
	byte ended;						/* client: the story is finished, the world is left */
	byte coop;						/* client: its instance plays the coop story */
	byte world;						/* client: its world plays the coop story (the cue of the coordinator) */
	byte replay;					/* client: the scene of the instance starts */
	byte cmenu;						/* client: the instance's menu it shows (COOP_MENU_*), 0 */
	byte testEdit;					/* client test: SCRAP_COOP_SHIPEDIT */
	double cmenuAt;
	double sceneAt;					/* client: when it began */
	char notice[256];
} CoopState;

extern CoopState g_Coop;

byte Cp_ClientStoryVar(const char *name);
byte Cp_InList(const char *const *list, const char *s);
int Cp_MissionPlayer(void);
int Cp_OptionsMsg(NetRepBuf *w, cEntity *t);
int Cp_Player(void);
int Cp_SceneMsg(uint8_t *buf, int size);
void Cp_ConvPicked(int i, const char *cls);
void Cp_GetVars(NetRepReader *r, byte act);
void Cp_MenuFunc(int i, const char *fn);
void Cp_PutVars(NetRepBuf *w, byte act);
void Cp_ShipEdited(int i, NetRepReader *r);
void Cp_SputnikDone(int i, NetRepReader *r);
void Cp_TellOptions(void);
void Cp_TellScorer(void);
void Cp_TellStory(int to, byte all);
void Cp_TellTargets(void);
void Cp_ToAll(const uint8_t *buf, int size);
void Cp_ToClient(int i, const uint8_t *buf, int size);
void Cp_ToLeader(int i);
void Cp_ToNear(int s, const uint8_t *buf, int size);

/* the engine variables of the four timers of the HUD and their direction (CoopOld.c, CoopClient.c) */
extern const char *const g_CoopTimerVars[];

#endif
