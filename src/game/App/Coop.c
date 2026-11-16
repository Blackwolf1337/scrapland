/*
 * The coop story of a world: the party's save variables on the instances, the mission level changes as travel, the
 * cutscenes and conversations on the members' clients, see include/scrap/coop.h. Not in the original.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/ai.h"
#include "scrap/aicharcontrol.h"
#include "scrap/alarm.h"
#include "scrap/aivehicle.h"
#include "scrap/camcontrol.h"
#include "scrap/chartype.h"
#include "scrap/console.h"
#include "scrap/coop.h"
#include "scrap/coord.h"
#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/faction.h"
#include "scrap/game.h"
#include "scrap/global.h"
#include "scrap/graphic2d.h"
#include "scrap/input.h"
#include "scrap/instance.h"
#include "scrap/itementity.h"
#include "scrap/missionmaker.h"
#include "scrap/netrep.h"
#include "scrap/nodesworld.h"
#include "scrap/nodesworld2d.h"
#include "scrap/places.h"
#include "scrap/platform.h"
#include "scrap/play.h"
#include "scrap/players.h"
#include "scrap/replicate.h"
#include "scrap/scorer.h"
#include "scrap/scorerhud.h"
#include "scrap/scrap.h"
#include "scrap/pyfunction.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "scrap/scriptnative.h"
#include "scrap/scrapcollision.h"
#include "scrap/sound.h"
#include "scrap/sputniks.h"
#include "scrap/text.h"
#include "scrap/texts.h"
#include "scrap/travel.h"
#include "scrap/vehicleattack.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/withlifeentity.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxweapons.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"
#include "game/Script/levels/Levels.h"

#define CP_NAME 64					/* bytes of a name of the story (CoordKv's keys) */
#define CP_MSG 16384				/* bytes of one story message to the coordinator */
#define CP_SCENE_MSG 8192			/* bytes of a scene's table for the clients */
#define CP_STYPES 64				/* items of the HUD's scorer whose class is kept */
#define CP_CDIRTY 256				/* changed variables of the story for the clients between two ticks */
#define CP_SCORER_MSG 6144			/* bytes of the mission's items of the HUD */
#define CP_SCENE_LINES 256
#define CP_GATHER 160.0f			/* units between the members placed next to the member of a scene */
#define CP_VOICES 4
#define CP_OPTS_MSG (8 + CP_NAME + COOP_OPTS * (COOP_OPT_TEXT + 2))
#define COOP_CAM "CoopCam"
#define COOP_STANDIN "CoopStandIn"
#define CP_MENU_MSG 8192			/* bytes of a menu message (the hangar's save variables) */
#define CP_MENU_TITLE 1024
#define CP_HANGAR "Hangar."			/* the save variables of the ship editor */
#define CP_ANSWER_SECS 2.0			/* test: a question's answer after this */
#define CP_CONV_SECS 3.0			/* test: the conversor's pick after this */
#define CP_CHECKPOINT "CheckPoint"	/* the checkpoint of a race of RaceMaker */
#define CP_RACE_GAP 2.5				/* seconds: test: the race's ship at the next checkpoint after this */
#define CP_FIGHT_FAR 6000.0f		/* test: a fighter further from its target is put CP_FIGHT_NEAR from it */
#define CP_FIGHT_NEAR 2500.0f
#define CP_FOLLOW_NEAR 1000.0f		/* test: a ship it stays with (no fight) is kept this near */
#define CP_FOLLOW_FAR 2000.0f		/* test: and put back that near when it is further */
#define CP_ESCORT 6000.0f			/* test: the red ones this near a ship it stays with are fought first */
#define CP_FIGHT_HIT 3000.0f		/* test: a target this near is fought with the instant-hit weapons first */
#define CP_FIGHT_LOCK 0.4			/* test: s a homing missile's lock is on the target before the release */
#define CP_FIGHT_LOCKCONE 0.5f		/* test: radians: it holds a homing missile while the target is this near ahead */
#define CP_FIGHT_SHOT 8000.0f		/* test: units a second of a shot, to lead a moving target */
#define CP_FIGHT_TURN 0.25f			/* test: radians: a ship turns at full speed to a target further aside */
#define CP_FIGHT_SIZE 250.0f		/* test: it fires when its aim is this near the target's middle */
#define CP_FIGHT_RANGE 8000.0f		/* test: it fires at a target this near and ahead */
#define CP_FIGHT_MAX 64				/* test: the targets looked at */
#define CP_AMMO_TRIED 6				/* test: ammo boxes remembered as taken */
#define CP_AMMO_GAP 4.0				/* test: s between two ammo boxes */
#define CP_FIGHT_USELESS 10.0		/* test: s a weapon fires at a target near ahead that loses no life: not taken again */
#define CP_FIGHT_RED "red"			/* test: the fight list of all the red ones (or enemies) of the level */
#define CP_POSSESS_SECS 90.0		/* test: a possession is given up after this */
#define CP_AUTO_GAP 8.0				/* test: the automatic player goes to its target again after this */
#define CP_AUTO_NEAR 250.0f			/* test: units from a target it uses */
#define CP_AUTO_OUT 900.0f			/* test: its ship flies this near a target on foot before it gets out */
#define CP_FLY_GAP 0.5				/* test: s between two looks at a flight's way */
#define CP_FLY_NEAR 400.0f			/* test: a point of its flight's path this near is passed */
#define CP_FLY_ON 500.0f			/* test: a flight comes this much nearer within CP_FLY_STUCK */
#define CP_FLY_STUCK 8.0			/* test: s, else it is placed CP_FIGHT_NEAR from its goal (as before) */
#define CP_AUTO_SHIP 8000.0f		/* test: outdoors a target on foot further than this is flown to */
#define CP_SHIP_ASK 12.0			/* test: s it asks for a ship (the switch) before it walks */
#define CP_MSG_NEAR 700.0f			/* test: a messenger this near the pack's carrier is shot first */
#define CP_MSG_SHOT 1500.0f			/* test: a messenger fires its lightning at a messenger this near (it reaches 2000) */
#define CP_AIR 1500.0f				/* test: an item this high above the floor is flown onto */
#define CP_WALK_FAR 300.0f			/* test: a walker walks to a target further than this (GoWithHuman: the switch up a step) */
#define CP_WALK_STEP 300.0f			/* test: a walk comes a quarter of this further within CP_WALK_STUCK */
#define CP_WALK_GAP 0.5				/* test: s between two looks at its way and its followers */
#define CP_WALK_NEAR 60.0f			/* test: a point of its path this near is passed */
#define CP_WALK_WAIT 900.0f			/* test: a walker waits for a character this near (one that follows it) */
#define CP_WALK_ALONE 15.0			/* test: s it waits for one at most */
#define CP_WALK_LEASH 1500.0f		/* test: followers further than this are none (BeatPickus' thugs stop there) */
#define CP_WALK_STUCK 4.0			/* test: s without coming CP_WALK_STEP / 4 further: placed at its path's point */
#define CP_WALK_END 600.0f			/* test: a walk that comes no nearer within this ends (the goto's last step) */
#define CP_MELEE 90.0f				/* test: units from a character it fights on foot (D-Tritus' punch: 50 ahead, 100 wide) */
#define CP_MELEE_FAR 600.0f			/* test: a character further is walked to (put next to it) */
#define CP_GONE 128					/* entities deleted by the level's start told to the clients */
#define CP_GONE_MSG (8 + CP_GONE * (CP_NAME + 2))
#define CP_DEAL_SECS 8.0			/* the super deal's scores before their Continue (no member's click on the instance) */
#define CP_FAIL_SECS 5.0			/* a failed mission's restart after this (the single player's after the game over) */
#define CP_LIFEBOAT "SLifeBoat<-<-<-<-<-0,0,0,0,0,0,1<-50<-0,0,0,0,0,0"	/* the hangar's spare ship (a test's ship) */

/* the variables of a player, not of the story (where it goes on, its character, money and alarm, its time) */
static const char *const cp_local[] = { "Map", "Spawn", "ComeFrom", "PrevMap", "Char", "AcBatteries", "Money", "Alarm",
	"GameplayTime", NULL };
/* the variables of the story the HUD shows: the clients get them (and their globals of the same name) */
static const char *const cp_hud[] = { "EnergyBarActive", "SpecialActionActive", NULL };
/* the variables of the story the clients' copies leave out: the mission (the client's level start would run it), the
 * auto save of a level's start, the instance's level start message */
static const char *const cp_clientNot[] = { "Mission.Map", "Mission.Library", "AutoSaveGameOnLoad", "InitialMessage",
	NULL };
/* the way point of a chapter's travel, given to the start of the target level (its scripts read them at once) */
static const char *const cp_entry[] = { "Map", "Spawn", "ComeFrom", "PrevMap", NULL };
#define CP_ENTRY "Coop.Entry."
#define CP_PURSE "Coop.Money"		/* the party's money, a counter of the story (Cp_Purse) */

#define CP_TABLES 4					/* call tables of one scene (its Load table and the actors' after it) */

typedef struct CpScene {
	char cls[CP_NAME];
	const char *module;
	int ntables;
	const char *const *lines[CP_TABLES];
	char dataModule[CP_NAME], dataSection[CP_NAME];	/* the level data section that loads it, "" */
} CpScene;

static struct {
	byte on;
	char story[COORD_PATH];			/* the file of -story */
	byte applying;					/* the coordinator's story goes in: not sent back */
	byte synced;					/* the whole story came since the coordinator connected */
	byte overflow;					/* more changes than the list holds: the whole story goes */
	byte levelSent;					/* the changes of the level's start went (COORD_MSG_STORYINIT) */
	byte hold;						/* test: the changes wait */
	byte started;					/* the level's first tick ran */
	byte waiting;					/* the world waits for its first member (its time stands) */
	cEntity *standIn;				/* the scripts' player of the level's start without a member (Coop_StandIn) */
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
	int scenePlayer;				/* the member bound when it started */
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
	int remoteWas;					/* the member bound before it */
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
} cp = { .scene = -1, .loading = -1, .conv = -1, .missionPlayer = -1, .pilot = -1 };

static void Cp_ToClient(int i, const uint8_t *buf, int size);
static void Cp_ToAll(const uint8_t *buf, int size);
static void Cp_ToNear(int s, const uint8_t *buf, int size);
static int Cp_MissionPlayer(void);
static void Cp_SetMissionPlayer(int i);
static void Cp_Reset(const char *map);
static void Cp_TellGone(int i);

static byte Cp_InList(const char *const *list, const char *s) {
	for (; *list; list++) {
		if (!crt_stricmp(*list, s)) {
			return 1;
		}
	}
	return 0;
}

byte Coop_ParseParam(const char *param, const char *ext) {
	if (!crt_stricmp("mode", param) && !crt_stricmp(ext, "coop")) {
		cp.on = 1;
		return 1;
	}
	if (!crt_stricmp("story", param)) {
		Str_Copy(cp.story, ext, sizeof(cp.story));
		return 1;
	}
	return 0;
}

byte Coop_On(void) {
	return g_Instance && cp.on;
}

byte Coop_Asked(void) {
	return cp.on;
}

/* ---- the story ---- */

static byte Cp_Story(const char *name) {
	return name[0] && (int)strlen(name) < CP_NAME && CoordKv_KeyOk(name) && !Cp_InList(cp_local, name);
}

static int Cp_DirtyAt(const char *name) {
	for (int i = 0; i < cp.ndirty; i++) {
		if (!strcmp(cp.dirty[i].name, name)) {
			return i;
		}
	}
	return -1;
}

/* a variable of the story the HUD shows to client i (-1: all here) */
static void Cp_HudVar(int i, const char *name) {
	uint8_t buf[2 * CP_NAME + 8];
	NetRepBuf w;
	const char *v = SaveVar_Get(name);
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_HUDVAR);
	NetRep_PutStr(&w, name);
	NetRep_PutStr(&w, v ? v : "0");
	if (w.bad) {
		return;
	}
	if (i < 0) {
		Cp_ToAll(buf, w.size);
	} else {
		Cp_ToClient(i, buf, w.size);
	}
}

static byte Cp_ClientStoryVar(const char *name) {
	return Cp_Story(name) && !Cp_InList(cp_clientNot, name);
}

void Coop_SaveVarChanged(const char *name, const char *old, byte inc) {
	if (cp.on && Cp_InList(cp_hud, name)) {
		Cp_HudVar(-1, name);
	}
	if (cp.on && Cp_ClientStoryVar(name) && !cp.coverflow) {
		int k = 0;
		while (k < cp.ncdirty && strcmp(cp.cdirty[k], name)) {
			k++;
		}
		if (k == CP_CDIRTY) {
			cp.coverflow = 1;
		} else if (k == cp.ncdirty) {
			Str_Copy(cp.cdirty[cp.ncdirty++], name, CP_NAME);
		}
	}
	if (!cp.on || cp.applying || !Cp_Story(name)) {
		return;
	}
	/* the member that sets the next mission is its member */
	if (g_Instance && !strcmp(name, "Mission.Library") && Players_Bound() != PLAYERS_NONE) {
		Cp_SetMissionPlayer(Players_Bound());
	}
	int i = Cp_DirtyAt(name);
	if (i < 0) {
		if (cp.ndirty == COOP_DIRTY) {
			cp.overflow = 1;
			return;
		}
		i = cp.ndirty++;
		Str_Copy(cp.dirty[i].name, name, CP_NAME);
		cp.dirty[i].base = 0;
		cp.dirty[i].hasBase = !old || CoordStory_Int(old, &cp.dirty[i].base);
		cp.dirty[i].counter = !strncmp(name, "Stats.", 6);
	}
	cp.dirty[i].counter |= inc;
}

void Coop_LevelChar(const char *level) {
	const char *s = level ? strrchr(level, '/') : NULL;
	const char *lc = level && (g_Instance || cp.world) ? MissionMaker_Char(s ? s + 1 : level) : NULL;
	if (lc && strcmp(lc, "Dtritus")) {
		SaveVar_Set("Char", lc);
		Con_Printf("coop: the level's start with its own character %s\n", lc);
	}
}

void Coop_LevelStart(const char *level) {
	static char text[COORD_STORY_MAX];
	static char key[CP_NAME], val[COORD_STORY_VALUE + 1];

	if (!Coop_On()) {
		return;
	}
	int n = cp.story[0] ? Coord_ReadFile(cp.story, text, sizeof(text)) : -1;
	if (n <= 0) {
		/* a new party: the new game of the single player (sent to the coordinator as the story's first changes) */
		SaveGame_CoopNewGame();
		Con_Printf("coop: a new story (the new game), %d changes for the coordinator\n", cp.ndirty);
		return;
	}
	cp.applying = 1;
	SaveVars_Delete("");
	int count = 0;
	for (int pos = 0; CoordKv_Next(text, n, &pos, key, sizeof(key), val, sizeof(val)); count++) {
		SaveVar_Set(key, val);
	}
	/* the level a chapter's or a mission's travel came to: its way point as the single player's level start has it */
	const char *map = SaveVar_Get(CP_ENTRY "Map");
	const char *s = map ? strrchr(map, '/') : NULL, *here = level ? strrchr(level, '/') : NULL;
	if (s && here && !crt_stricmp(s, here)) {
		for (int k = 0; cp_entry[k]; k++) {
			Str_PrintfQuiet(key, sizeof(key), CP_ENTRY "%s", cp_entry[k]);
			const char *v = SaveVar_Get(key);
			SaveVar_Set(cp_entry[k], v ? v : "");
		}
		Con_Printf("coop: the way point of the chapter: %s, spawn %s, from %s\n", map, SaveVar_Get("Spawn"),
			SaveVar_Get("ComeFrom"));
		/* taken: a later start of this level is the single player's load of a save there (Junkyard's arrival with
		 * Berto once) */
		cp.applying = 0;
		for (int k = 0; cp_entry[k]; k++) {
			Str_PrintfQuiet(key, sizeof(key), CP_ENTRY "%s", cp_entry[k]);
			SaveVar_Delete(key);
		}
	}
	cp.applying = 0;
	/* the character the members carry is theirs (cp_local) */
	if (!SaveVar_Get("Char")) {
		Coop_LevelChar(level);
	}
	Con_Printf("coop: the party's story, %d variables, mission %s in %s\n", count, SaveVar_Get("Mission.Library") ?
		SaveVar_Get("Mission.Library") : "-", SaveVar_Get("Mission.Map") ? SaveVar_Get("Mission.Map") : "-");
}

/* the message being written: its count patched in at the end */
typedef struct CpOut {
	uint8_t buf[CP_MSG + 16];
	NetRepBuf w;
	int count;
} CpOut;

static void Cp_OutStart(CpOut *o) {
	NetRepBuf_Init(&o->w, o->buf, CP_MSG);
	NetRep_PutU8(&o->w, cp.levelSent ? COORD_MSG_STORY : COORD_MSG_STORYINIT);
	NetRep_PutU16(&o->w, 0);
	o->count = 0;
}

static void Cp_OutFlush(CpOut *o) {
	if (o->count) {
		o->buf[1] = (uint8_t)o->count;
		o->buf[2] = (uint8_t)(o->count >> 8);
		Travel_SendCoord(o->buf, o->w.size);
	}
	Cp_OutStart(o);
}

static void Cp_OutPut(CpOut *o, int kind, const char *name, const char *value) {
	int vn = value ? (int)strlen(value) : 0;
	if (vn > COORD_STORY_VALUE) {
		Con_Printf("coop: the value of %s is too long for the story (%d bytes)\n", name, vn);
		return;
	}
	if (o->w.size + 4 + (int)strlen(name) + vn > CP_MSG || o->count == 0xffff) {
		Cp_OutFlush(o);
	}
	CoordStory_Put(&o->w, kind, name, value);
	o->count++;
}

/* a dirty name: its value, a counter's change as the addition since it became dirty (none when it is back) */
static void Cp_OutDirty(CpOut *o, int i) {
	char delta[24];
	long v;
	const char *value = SaveVar_Get(cp.dirty[i].name);
	if (value && cp.dirty[i].counter && cp.dirty[i].hasBase && CoordStory_Int(value, &v)) {
		if (v != cp.dirty[i].base) {
			Str_PrintfQuiet(delta, sizeof(delta), "%ld", v - cp.dirty[i].base);
			Cp_OutPut(o, COORD_OP_ADD, cp.dirty[i].name, delta);
		}
		return;
	}
	Cp_OutPut(o, value ? COORD_OP_SET : COORD_OP_DEL, cp.dirty[i].name, value);
}

static void Cp_PutAll(const char *name, const char *value, void *u) {
	if (Cp_Story(name)) {
		Cp_OutPut(u, COORD_OP_SET, name, value);
	}
}

/* the changes to the coordinator now (once the whole story came: before it the coordinator's would go over them) */
static void Cp_Send(void) {
	static CpOut o;
	if (!cp.synced || cp.hold || (!cp.ndirty && !cp.overflow)) {
		return;
	}
	Cp_OutStart(&o);
	if (cp.overflow) {
		SaveVars_Each(Cp_PutAll, &o);
	} else {
		for (int i = 0; i < cp.ndirty; i++) {
			Cp_OutDirty(&o, i);
		}
	}
	Cp_OutFlush(&o);
	cp.ndirty = 0;
	cp.overflow = 0;
	cp.levelSent = 1;
}

void Coop_SceneCamera(void) {
	if (!Coop_On() || !g_pWorld) {
		return;
	}
	const char *name = cWorld_GetCamName(g_pWorld, 0);
	if (name && *name && World_FindEntity(name)) {
		return;
	}
	if (!World_FindEntity(COOP_CAM) && !cWorld_CreateEntity(g_pWorld, COOP_CAM, 0.0f, 0.0f, 0.0f, TYEN_FCAM)) {
		return;
	}
	cWorld_SetCam(g_pWorld, 0, COOP_CAM);
}

static void Cp_TellTargets(void);
static void Cp_TellScorer(void);
static void Cp_TellStory(int to, byte all);
static void Cp_MenuFunc(int i, const char *fn);
static void Cp_ShipEdited(int i, NetRepReader *r);
static void Cp_SputnikDone(int i, NetRepReader *r);
static void Cp_ConvPicked(int i, const char *cls);

/* the entity player e would use: the one it talks to, else the one in front of it with a use arrow */
static cEntity *Cp_UseTarget(cEntity *e) {
	cEntity *t = WChar(e)->listenOn ? WChar_Listen(e) : NULL;
	if (!t) {
		t = WChar_Victim(e);
	}
	return t && Entity_IsType(t, TYEN_WITH_LIFE) && WLife(t)->arrow.canUse ? t : NULL;
}

/* the options message of the arrow of t (NULL: none) into w: their count */
static int Cp_OptionsMsg(NetRepBuf *w, cEntity *t) {
	cArrowIndicator *a = t ? &WLife(t)->arrow : NULL;
	int n = a ? (a->nOpt < COOP_OPTS ? a->nOpt : COOP_OPTS) : 0;
	NetRep_PutU8(w, COOP_MSG);
	NetRep_PutU8(w, COOP_Y_OPTS);
	NetRep_PutStr(w, n ? t->name : "");
	NetRep_PutU8(w, n ? (uint32_t)a->iOpt : 0);
	NetRep_PutU8(w, (uint32_t)n);
	for (int k = 0; k < n; k++) {
		const char *o = cArrowIndicator_GetOpt(a, k);
		char cut[COOP_OPT_TEXT];
		Str_Copy(cut, o ? o : "", sizeof(cut));
		NetRep_PutStr(w, cut);
	}
	return n;
}

/* the options of each member's target when they changed */
static void Cp_TellOptions(void) {
	uint8_t buf[CP_OPTS_MSG];
	NetRepBuf w;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = Players_Get(i);
		if (Rep_PlayerConn(i) < 0) {
			cp.pl[i].sentSize = 0;
			continue;
		}
		cEntity *t = e && NetPl_Kind(e) == NETREP_CHAR ? Cp_UseTarget(e) : NULL;
		NetRepBuf_Init(&w, buf, sizeof(buf));
		int n = Cp_OptionsMsg(&w, t);
		if (w.bad || (w.size == cp.pl[i].sentSize && !memcmp(buf, cp.pl[i].sent, (size_t)w.size))) {
			continue;
		}
		/* nothing to tell a member that was told nothing */
		if (!n && !cp.pl[i].sentSize) {
			continue;
		}
		memcpy(cp.pl[i].sent, buf, (size_t)w.size);
		cp.pl[i].sentSize = n ? w.size : 0;
		Cp_ToClient(i, buf, w.size);
		Con_Printf("coop: player %d: %d options of %s (%d)\n", i, n, n ? t->name : "-", n ? WLife(t)->arrow.iOpt : 0);
	}
}

/* the party's one purse (include/scrap/coop.h): what the members here earned or spent since the last tick goes into
 * the story's counter, every member gets the sum; a story without one takes the money of its first member */
static void Cp_Purse(void) {
	char text[16];
	const char *v = SaveVar_Get(CP_PURSE);
	int purse = v ? crt_atoi(v) : 0, add = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (Rep_PlayerConn(i) < 0) {
			continue;
		}
		int m = Players_Money(i);
		if (cp.pl[i].purseJoined == Players_Joined(i)) {
			add += m - cp.pl[i].purse;
			continue;
		}
		cp.pl[i].purseJoined = Players_Joined(i);
		cp.pl[i].purse = purse;
		if (!v) {
			Str_PrintfQuiet(text, sizeof(text), "%d", m);
			SaveVar_Set(CP_PURSE, text);
			v = SaveVar_Get(CP_PURSE);
			purse = m;
			cp.pl[i].purse = m;
		}
		Con_Printf("coop: player %d has the party's money %d (its own was %d)\n", i, purse, m);
	}
	if (add) {
		purse = SaveVar_Inc(CP_PURSE, add);
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (Rep_PlayerConn(i) >= 0) {
			Players_SetMoney(i, purse);
			cp.pl[i].purse = purse;
		}
	}
}

void Coop_Pass(void) {
	if (Coop_On() && !cp.started) {
		/* the level's start (a scene of its Init: GamblinDen's MeetGambler, Fake0) waits for the member that comes,
		 * as the single player's plays it from its start */
		cp.started = 1;
		cp.waiting = 1;
		for (int i = 0; i < PLAYERS_MAX; i++) {
			cp.waiting = cp.waiting && Rep_PlayerConn(i) < 0;
		}
		if (cp.waiting) {
			Timer_SetSpeed(&g_pWorld->timer, 0.0f);
			Con_Printf("coop: the level waits for its first member\n");
		}
	}
	if (Coop_On()) {
		/* nothing draws the talk scorer here: its time and the end of a remote communication (a talk of the level's
		 * start waits for the first member, as the level does) */
		if (!cp.waiting) {
			Scorer_TalkTick(1.0f / INSTANCE_TICK_HZ);
		}
		Scorer_Tick(0);
		if (cp.conv >= 0 && cp.convAsked && Rep_PlayerConn(cp.conv) < 0) {
			/* the member left with the conversor's interface open: as its Escape */
			Cp_ConvPicked(cp.conv, "");
		}
		Cp_Purse();
		if (cp.failAt > 0.0 && Instance_Time() >= cp.failAt) {
			cp.failAt = 0.0;
			Cp_Reset(Instance_Level());
		}
		Cp_Send();
		Cp_TellOptions();
		Cp_TellTargets();
		Cp_TellScorer();
		if (cp.ncdirty || cp.coverflow) {
			Cp_TellStory(-1, cp.coverflow);
			cp.ncdirty = 0;
			cp.coverflow = 0;
		}
	}
}

byte Coop_Revives(const nativeVal_t *name) {
	cEntity *e = Coop_On() && name->type == NV_STR ? World_FindEntity(Native_Text(name)) : NULL;
	return !Coop_On() || (e && Players_Of(e) >= 0);
}

void Coop_Boarded(int i, cEntity *ship) {
	static const char *const cb[2][2] = { { "SwitchCharMissionCallback", "SwitchCharMissionCallbackAux" },
		{ "SwitchVehicleMissionCallback", "SwitchVehicleMissionCallbackAux" } };
	byte called;
	if (!Coop_On() || !Mode_Import("Parking")) {
		Native_ErrClear();
		return;
	}
	/* single player has one pilot: the callbacks of boarding run for the first member in a ship, the ones of leaving for
	 * that member (DestroyMickus' scan: a second member's boarding started a second scan, bound to that member) */
	cEntity *p = cp.pilot >= 0 && Rep_PlayerConn(cp.pilot) >= 0 ? Players_Get(cp.pilot) : NULL;
	if (ship ? cp.pilot >= 0 && cp.pilot != i && p && NetPl_Kind(p) == NETREP_SHIP : cp.pilot != i) {
		Con_Printf("coop: player %d %s a ship: the callbacks stay with player %d\n", i, ship ? "into" : "out of", cp.pilot);
		return;
	}
	cp.pilot = ship ? i : -1;
	/* a member without an own ship: the ship it boarded stands for it while the callbacks run (single player always has
	 * its own there: a super deal's start sets Parking.PlayerShip.Invulnerable); none again after them, so that ship's
	 * death is no own ship's (Vehicles.OnDeath, the deal's respawns) */
	cEntity *stand = ship && !Players_Ship(i) ? ship : NULL;
	if (stand) {
		Players_SetShip(i, stand);
	}
	Players_Bind(i);
	for (int k = 0; k < 2; k++) {
		if (!Mode_CallIf("Parking", cb[ship != NULL][k], &called)) {
			Native_ErrPrint();
		} else if (called) {
			Con_Printf("coop: player %d %s a ship: Parking.%s\n", i, ship ? "into" : "out of", cb[ship != NULL][k]);
		}
	}
	Players_BindTurn();
	if (stand && Players_Ship(i) == stand) {
		Players_SetShip(i, NULL);
	}
}

/* ---- key names ---- */

#define CP_KEY_OPEN "{{"
#define CP_KEY_CLOSE "}}"

const char *Coop_KeyToken(const char *set, const char *action, const char *device) {
	static char token[3 * CP_NAME];
	if (!Coop_On() || !set || !action) {
		return NULL;
	}
	Str_PrintfQuiet(token, sizeof(token), CP_KEY_OPEN "%s|%s|%s" CP_KEY_CLOSE, set, action, device ? device : "");
	return token;
}

void Coop_ClientKeys(char *text, int size) {
	char out[1201], part[3][CP_NAME];
	int len = 0;
	const char *p = text;
	if (!strstr(text, CP_KEY_OPEN)) {
		return;
	}
	while (*p && len < (int)sizeof(out) - 1) {
		const char *end = !strncmp(p, CP_KEY_OPEN, 2) ? strstr(p, CP_KEY_CLOSE) : NULL;
		int n = 0;
		if (end) {
			const char *f = p + 2;
			for (n = 0; n < 3 && f <= end; n++) {
				int k = (int)strcspn(f, "|}");
				k = f + k > end ? (int)(end - f) : k;
				Str_PrintfQuiet(part[n], CP_NAME, "%.*s", k, f);
				f += k + 1;
			}
		}
		if (n == 3) {
			const char *key = Input_GetDefinedEntry(0, part[0], part[1], part[2], 0);
			len += Str_PrintfQuiet(out + len, (int)sizeof(out) - len, "%s", key ? key : "");
			len = len < (int)sizeof(out) ? len : (int)sizeof(out) - 1;
			p = end + 2;
		} else {
			out[len++] = *p++;
		}
	}
	out[len] = 0;
	Str_Copy(text, out, size);
}

/* ---- the mission's items of the HUD ---- */

/* the variables of an item a client's copy gets (the ones its class has; the functions stay the instance's) */
static const char *const cp_scorerVars[] = { "X", "Y", "W", "H", "Visible", "Text", "Font", "Align", "Effect",
	"CentralText", "Red", "Green", "Blue", "Alpha", "Value", "MaxValue", "MainBarIndex", "BackIndex", "BlackBarIndex",
	"Blink", "MainRed", "MainGreen", "MainBlue", "MainAlpha", "BackRed", "BackGreen", "BackBlue", "BackAlpha",
	"BlackRed", "BlackGreen", "BlackBlue", "BlackAlpha", "File", "SpriteIndex", "ScaleX", "ScaleY", "BlendingMode",
	NULL };
/* the classes that are copied (a talk, a radar, a menu's item are the client's own) */
static const char *const cp_scorerTypes[] = { "Text", "Label", "Timer", "Bar", "Sprite", NULL };
/* the engine variables of the four timers and their direction */
static const char *const cp_timerVars[] = { "TimeToCancelCurrentMission", "TimeToFinishCurrentMission",
	"TimeInactiveTimer", "TimeSpecialUseTimer", "CounterType", NULL };
static const char *const cp_timerTypes[] = { "TimeToCancel", "TimeToFinish", "TimeInactive", "TimeSpecialUse" };

void Coop_ScorerAdded(const char *name, const char *type) {
	if (!Coop_On() || !name || (int)strlen(name) >= CP_NAME) {
		return;
	}
	int k = 0;
	while (k < cp.nstype && strcmp(cp.stype[k].name, name)) {
		k++;
	}
	if (k == cp.nstype) {
		if (k == CP_STYPES) {
			return;
		}
		cp.nstype++;
	}
	Str_Copy(cp.stype[k].name, name, CP_NAME);
	Str_Copy(cp.stype[k].type, type, sizeof(cp.stype[k].type));
}

void Coop_ScorerBase(int id) {
	cScorer *sc = Coop_On() && !id ? Scorer_Get(0) : NULL;
	if (!sc) {
		return;
	}
	cp.nsbase = 0;
	for (cScorerItem *it = sc->items; it && cp.nsbase < CP_STYPES; it = it->next) {
		if (!it->delMe && it->name) {
			Str_Copy(cp.sbase[cp.nsbase++], it->name, CP_NAME);
		}
	}
}

static const char *Cp_ScorerType(const char *name) {
	for (int k = 0; k < cp.nstype; k++) {
		if (!strcmp(cp.stype[k].name, name)) {
			return Cp_InList(cp_scorerTypes, cp.stype[k].type) ? cp.stype[k].type : NULL;
		}
	}
	return NULL;
}

/* the variables of item it into w: their count first */
static void Cp_PutScorerVars(NetRepBuf *w, cScorerItem *it) {
	char val[256];
	int at = w->size, n = 0;
	NetRep_PutU8(w, 0);
	int tt = cTimerScorer_Type(it);
	if (tt >= 0 && tt < 4) {
		NetRep_PutStr(w, "TimerType");
		NetRep_PutU8(w, 1);
		NetRep_PutStr(w, cp_timerTypes[tt]);
		n++;
	}
	for (int k = 0; cp_scorerVars[k]; k++) {
		char *str = NULL;
		double d = 0.0;
		int i = 0, kind = ScorerItem_Var(it, cp_scorerVars[k], 0, &str, &d, &i);
		if (kind == 1) {
			Str_Copy(val, str ? str : "", sizeof(val));
		} else if (kind == 2) {
			Str_PrintfQuiet(val, sizeof(val), "%.3f", d);
		} else if (kind == 3) {
			Str_PrintfQuiet(val, sizeof(val), "%d", i);
		} else {
			continue;
		}
		NetRep_PutStr(w, cp_scorerVars[k]);
		NetRep_PutU8(w, (uint32_t)kind);
		NetRep_PutStr(w, val);
		n++;
	}
	if (!w->bad) {
		w->p[at] = (uint8_t)n;
	}
}

/* the items of the instance's HUD that are no standard ones (the mission's: the race's scores, its timers) to the
 * clients here when they changed */
static void Cp_TellScorer(void) {
	static uint8_t buf[CP_SCORER_MSG];
	uint64_t now = (uint64_t)(Instance_Time() * 1000.0);
	cScorer *sc = Scorer_Get(0);
	if (!sc || now - cp.slookAt < COOP_SCORER_MS) {
		return;
	}
	cp.slookAt = now;
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_SCORER);
	for (int k = 0; cp_timerVars[k]; k++) {
		char v[32];
		nativeVal_t t;
		Native_SetNone(&t);
		if (Native_GVarGet(cp_timerVars[k], &t) && (t.type == NV_INT || t.type == NV_FLOAT)) {
			Str_PrintfQuiet(v, sizeof(v), "%.3f", t.type == NV_INT ? (double)t.i : t.f);
		} else {
			v[0] = 0;
		}
		Native_ErrClear();
		Native_Free(&t);
		NetRep_PutStr(&w, v);
	}
	int at = w.size, n = 0;
	NetRep_PutU8(&w, 0);
	for (cScorerItem *it = sc->items; it && n < COOP_SCORER_ITEMS && !w.bad; it = it->next) {
		const char *type = it->delMe || !it->name ? NULL : Cp_ScorerType(it->name);
		byte base = 0;
		for (int k = 0; type && k < cp.nsbase && !base; k++) {
			base = !strcmp(cp.sbase[k], it->name);
		}
		if (!type || base) {
			continue;
		}
		NetRep_PutStr(&w, it->name);
		NetRep_PutStr(&w, type);
		Cp_PutScorerVars(&w, it);
		n++;
	}
	if (w.bad) {
		return;
	}
	buf[at] = (uint8_t)n;
	if (w.size == cp.ssentSize && !memcmp(buf, cp.ssent, (size_t)w.size) && now - cp.ssentAt < COOP_TARGETS_MS) {
		return;
	}
	if (n != cp.nsitems) {
		Con_Printf("coop: the mission's items of the HUD: %d\n", n);
		cp.nsitems = n;
	}
	memcpy(cp.ssent, buf, (size_t)w.size);
	cp.ssentSize = w.size;
	cp.ssentAt = now;
	Cp_ToAll(buf, w.size);
}

/* the mission's targets to the clients here when they changed (or every COOP_TARGETS_MS: a proxy of one came into a
 * client's interest since) */
static void Cp_TellTargets(void) {
	uint8_t buf[16 + COOP_TARGETS * (CP_NAME + 4)];
	char sig[sizeof(cp.targets)];
	const char *names[COOP_TARGETS];
	nativeVal_t cur, list, it[COOP_TARGETS];
	NetRepBuf w;
	int n = 0, len = 0;
	byte listed = 0;
	Native_SetNone(&cur);
	Native_SetNone(&list);
	if (Mode_ModGet("MissionsFuncs", "currentTarget", &cur) && cur.type == NV_STR && Native_Text(&cur)[0]) {
		names[n++] = Native_Text(&cur);
	} else if (Mode_ModGet("MissionsFuncs", "targetsList", &list)) {
		listed = 1;
		int k = Mode_Loop(&list);
		for (int j = 0; j < k && n < COOP_TARGETS; j++) {
			Native_SetNone(&it[n]);
			if (Native_SeqItem(&list, j, &it[n]) && it[n].type == NV_STR) {
				names[n] = Native_Text(&it[n]);
				n++;
			} else {
				Native_Free(&it[n]);
			}
		}
	}
	Native_ErrClear();
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_TARGETS);
	NetRep_PutU8(&w, (uint32_t)n);
	sig[0] = 0;
	for (int k = 0; k < n; k++) {
		int slot = Rep_EntitySlot(World_FindEntity(names[k]));
		NetRep_PutStr(&w, names[k]);
		NetRep_PutU16(&w, slot < 0 ? NETREP_NO_SLOT : (uint32_t)slot);
		len += Str_PrintfQuiet(sig + len, (int)sizeof(sig) - len, "%s:%d,", names[k], slot);
		len = len < (int)sizeof(sig) ? len : (int)sizeof(sig) - 1;
	}
	uint64_t now = (uint64_t)(Instance_Time() * 1000.0);
	if (!w.bad && (strcmp(sig, cp.targets) || now - cp.targetsAt >= COOP_TARGETS_MS)) {
		if (strcmp(sig, cp.targets)) {
			Con_Printf("coop: the mission's targets: %s\n", sig[0] ? sig : "none");
		}
		Str_Copy(cp.targets, sig, sizeof(cp.targets));
		cp.targetsAt = now;
		Cp_ToAll(buf, w.size);
	}
	for (int k = 0; listed && k < n; k++) {
		Native_Free(&it[k]);
	}
	Native_Free(&cur);
	Native_Free(&list);
}

void Coop_BeforeUse(int i, cEntity *t) {
	if (!Coop_On() || i < 0 || i >= PLAYERS_MAX || !t || !Entity_IsType(t, TYEN_WITH_LIFE)) {
		return;
	}
	cArrowIndicator *a = &WLife(t)->arrow;
	if (cp.pl[i].choiceEnt[0] && !strcmp(cp.pl[i].choiceEnt, t->name) && cp.pl[i].choice < a->nOpt) {
		a->iOpt = cp.pl[i].choice;
	}
	/* the scripts move the arrow on after a use: the next choice starts from there */
	cp.pl[i].choiceEnt[0] = 0;
}

/* player i asks to go to its party's leader (the coordinator knows where it plays) */
static void Cp_ToLeader(int i) {
	uint8_t buf[NET2_KEY_SIZE + 4];
	NetRepBuf w;
	const uint8_t *key = Travel_KeyOfPlayer(i);
	if (!key) {
		Con_Printf("coop: player %d has no key for the leader travel\n", i);
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COORD_MSG_LEADER);
	CoordMsg_PutBytes(&w, key, NET2_KEY_SIZE);
	Travel_SendCoord(buf, w.size);
	Con_Printf("coop: player %d asks to go to its leader\n", i);
}

byte Coop_Message(Net2Host *host, int conn, const uint8_t *data, int size) {
	char ent[CP_NAME];
	NetRepReader r;
	(void)host;
	if (!Coop_On() || size < 2 || data[0] != COOP_MSG) {
		return 0;
	}
	int i = Rep_ConnPlayer(conn);
	NetRepReader_Init(&r, data + 2, size - 2);
	if (i < 0 || i >= PLAYERS_MAX) {
		return 1;
	}
	if (data[1] == COOP_C_ANSWER) {
		int kind = (int)NetRep_GetU8(&r);
		if (r.bad || !kind || kind != cp.pl[i].menu) {
			return 1;
		}
		cp.pl[i].menu = 0;
		if (kind == COOP_MENU_SHIPEDIT) {
			Cp_ShipEdited(i, &r);
			return 1;
		}
		if (kind == COOP_MENU_SPUTNIK) {
			Cp_SputnikDone(i, &r);
			return 1;
		}
		if (kind == COOP_MENU_CONV) {
			NetRep_GetStr(&r, ent, sizeof(ent));
			if (!r.bad && i == cp.conv) {
				Cp_ConvPicked(i, ent);
			}
			return 1;
		}
		int yes = (int)NetRep_GetU8(&r);
		if (!r.bad) {
			Con_Printf("coop: player %d answers %s\n", i, yes ? "yes" : "no");
			Cp_MenuFunc(i, yes ? cp.pl[i].yes : cp.pl[i].no);
		}
	} else if (data[1] == COOP_C_LEADER) {
		Cp_ToLeader(i);
	} else if (data[1] == COOP_C_CHOICE) {
		NetRep_GetStr(&r, ent, sizeof(ent));
		int k = (int)NetRep_GetU8(&r);
		cEntity *t = r.bad ? NULL : World_FindEntity(ent);
		if (!t || !Entity_IsType(t, TYEN_WITH_LIFE) || k >= WLife(t)->arrow.nOpt) {
			return 1;
		}
		Str_Copy(cp.pl[i].choiceEnt, ent, CP_NAME);
		cp.pl[i].choice = k;
		/* shown at once on the arrow (the others near it see the same as in the single player) */
		WLife(t)->arrow.iOpt = k;
		Con_Printf("coop: player %d chooses option %d of %s (%s)\n", i, k, ent, cArrowIndicator_GetOpt(&WLife(t)->arrow, k));
	}
	return 1;
}

void Coop_CoordLost(void) {
	cp.synced = 0;
}

/* a change of the coordinator into the save variables; a name changed here and not sent yet keeps its value, a
 * counter its own addition on the coordinator's number */
static void Cp_Apply(int kind, const char *name, const char *value) {
	char sum[24];
	long m, v;
	if (!Cp_Story(name)) {
		return;
	}
	int i = Cp_DirtyAt(name);
	if (i >= 0) {
		const char *here = SaveVar_Get(name);
		if (kind != COORD_OP_SET || !cp.dirty[i].counter || !cp.dirty[i].hasBase || !here || !CoordStory_Int(here, &v) ||
			!CoordStory_Int(value, &m)) {
			return;
		}
		Str_PrintfQuiet(sum, sizeof(sum), "%ld", m + v - cp.dirty[i].base);
		cp.dirty[i].base = m;
		value = sum;
	}
	cp.applying = 1;
	if (kind == COORD_OP_SET) {
		SaveVar_Set(name, value);
	} else {
		SaveVar_Delete(name);
	}
	cp.applying = 0;
}

typedef struct CpNames {
	int n;
	char name[COOP_DIRTY * 2][CP_NAME];
} CpNames;

static void Cp_Collect(const char *name, const char *value, void *u) {
	CpNames *l = u;
	(void)value;
	if (Cp_Story(name) && Cp_DirtyAt(name) < 0 && l->n < COOP_DIRTY * 2) {
		Str_Copy(l->name[l->n++], name, CP_NAME);
	}
}

/* the whole story: the variables of the story not in it go */
static void Cp_ApplyAll(const char *text, int len) {
	static CpNames old;
	static char key[CP_NAME], val[COORD_STORY_VALUE + 1];
	old.n = 0;
	SaveVars_Each(Cp_Collect, &old);
	for (int i = 0; i < old.n; i++) {
		if (!CoordKv_Get(text, len, old.name[i], val, sizeof(val))) {
			Cp_Apply(COORD_OP_DEL, old.name[i], NULL);
		}
	}
	int count = 0;
	for (int pos = 0; CoordKv_Next(text, len, &pos, key, sizeof(key), val, sizeof(val)); count++) {
		Cp_Apply(COORD_OP_SET, key, val);
	}
	Con_Printf("coop: the whole story from the coordinator, %d variables (%d changes of here to send)\n", count,
		cp.ndirty);
}

/* the player given to its party's travel (the coordinator's summon) */
static void Cp_Summon(NetRepReader *r) {
	uint8_t key[NET2_KEY_SIZE];
	char level[COORD_LEVEL + 1], entrance[COORD_LEVEL + 1], door[COORD_LEVEL + 1];
	CoordMsg_GetBytes(r, key, NET2_KEY_SIZE);
	NetRep_GetStr(r, level, sizeof(level));
	NetRep_GetStr(r, entrance, sizeof(entrance));
	NetRep_GetStr(r, door, sizeof(door));
	int i = r->bad ? -1 : Travel_PlayerOfKey(key);
	if (i < 0) {
		return;
	}
	Con_Printf("coop: player %d goes along with its party to %s\n", i, level);
	Travel_Go(i, level, entrance, door, 0);
}

byte Coop_CoordMessage(const uint8_t *data, int size) {
	static char name[CP_NAME], val[COORD_STORY_VALUE + 1];
	NetRepReader r;
	int kind;

	if (!Coop_On() || size < 1) {
		return 0;
	}
	NetRepReader_Init(&r, data + 1, size - 1);
	switch (data[0]) {
	case COORD_MSG_STORY: {
		int count = (int)NetRep_GetU16(&r);
		for (int i = 0; i < count && CoordStory_Next(&r, &kind, name, sizeof(name), val, sizeof(val)) > 0; i++) {
			Cp_Apply(kind, name, val);
		}
		return 1;
	}
	case COORD_MSG_STORYALL: {
		uint32_t n = NetRep_GetU32(&r);
		if (r.bad || n > (uint32_t)(size - 5)) {
			return 1;
		}
		Cp_ApplyAll((const char *)data + 5, (int)n);
		cp.synced = 1;
		Cp_Send();
		return 1;
	}
	case COORD_MSG_SUMMON:
		Cp_Summon(&r);
		return 1;
	}
	return 0;
}

/* ---- the mission level changes ---- */

static void Cp_ToClient(int i, const uint8_t *buf, int size) {
	int conn = Rep_PlayerConn(i);
	if (conn >= 0 && Instance_Host()) {
		Net2_Send(Instance_Host(), conn, REP_CH_RELIABLE, buf, size);
	}
}

static void Cp_ToAll(const uint8_t *buf, int size) {
	for (int i = 0; i < PLAYERS_MAX; i++) {
		Cp_ToClient(i, buf, size);
	}
}

static void Cp_Notice(int i, const char *key, const char *arg) {
	uint8_t buf[256];
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_NOTICE);
	NetRep_PutStr(&w, key);
	NetRep_PutStr(&w, arg);
	if (i < 0) {
		Cp_ToAll(buf, w.size);
	} else {
		Cp_ToClient(i, buf, w.size);
	}
}

/* the member the scripts act for: the bound one, else the first one here */
static int Cp_Player(void) {
	int i = Players_Bound();
	if (i != PLAYERS_NONE && Players_Get(i)) {
		return i;
	}
	for (i = 0; i < PLAYERS_MAX; i++) {
		if (Rep_PlayerConn(i) >= 0 && Players_Get(i)) {
			return i;
		}
	}
	return -1;
}

byte Coop_KeepStory(const char *prefix) {
	if (!Coop_On() || cp.applying || prefix[0]) {
		return 0;
	}
	/* the single player's end deletes every save variable before the menu: the party keeps its story */
	Con_Printf("coop: the story is kept (all save variables deleted by the scripts)\n");
	return 1;
}

/* the end of the story (QLoad of the menu after the credits' start): kept as finished; every member here sees the
 * credits in the main menu, the members elsewhere get a notice */
static void Cp_StoryEnd(void) {
	uint8_t buf[8], nb[128];
	NetRepBuf w, nw;
	SaveVar_Set(COORD_COOP_FINISHED, "yes");
	Cp_Send();
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_END);
	Cp_ToAll(buf, w.size);
	NetRepBuf_Init(&nw, nb, sizeof(nb));
	NetRep_PutU8(&nw, COORD_MSG_PNOTICE);
	NetRep_PutStr(&nw, "MP_CoopEnd|");
	Travel_SendCoord(nb, nw.size);
	Con_Printf("coop: the story is finished: the credits on the clients\n");
}

/* the level again (a super deal's restart): the instance starts again, every member here travels to its new start */
static void Cp_Reset(const char *map) {
	uint8_t buf[4] = { COORD_MSG_RESET };
	const char *spawn = SaveVar_Get("Spawn");
	Cp_Send();
	Travel_SendCoord(buf, 1);
	Con_Printf("coop: the level again (QLoad %s): the instance starts again for its members\n", map);
	for (int i = 0; i < PLAYERS_MAX; i++) {
		if (Rep_PlayerConn(i) >= 0) {
			Travel_Go(i, Instance_Level(), spawn ? spawn : "", "", 0);
		}
	}
}

/* the video the scripts set for the level change (Scrap.Set("PlayVideo"), a mission's end) to the client of member
 * i (-1: all here): it plays it before its next level, as the single player's level load does */
static void Cp_Video(int i) {
	uint8_t buf[CP_NAME * 2 + 8];
	nativeVal_t v;
	NetRepBuf w;
	Native_SetNone(&v);
	if (Native_GVarGet("PlayVideo", &v) && v.type == NV_STR && Native_Text(&v)[0]) {
		NetRepBuf_Init(&w, buf, sizeof(buf));
		NetRep_PutU8(&w, COOP_MSG);
		NetRep_PutU8(&w, COOP_Y_VIDEO);
		NetRep_PutStr(&w, Native_Text(&v));
		if (!w.bad) {
			if (i < 0) {
				Cp_ToAll(buf, w.size);
			} else {
				Cp_ToClient(i, buf, w.size);
			}
			Con_Printf("coop: the video %s to %s\n", Native_Text(&v), i < 0 ? "the members here" : "the member");
		}
		Mode_GVarSetS("PlayVideo", "");
	}
	Native_ErrClear();
	Native_Free(&v);
}

byte Coop_QLoad(const char *map) {
	char key[CP_NAME];
	if (!Coop_On()) {
		return 0;
	}
	const char *s = strrchr(map, '/');
	const char *level = s ? s + 1 : map;
	if (!crt_stricmp(level, "Menu")) {
		Cp_StoryEnd();
		return 1;
	}
	if (!crt_stricmp(level, Instance_Level())) {
		Cp_Reset(map);
		return 1;
	}
	/* the member of the scene whose end goes on (its callback), else the bound one */
	int i = cp.inScene && cp.scenePlayer >= 0 && Players_Get(cp.scenePlayer) ? cp.scenePlayer : Cp_Player();
	if (i < 0) {
		return 0;
	}
	const char *spawn = SaveVar_Get("Spawn");
	byte party = CoordCoop_Chapter(level) || CoordCoop_Chapter(Instance_Level());
	/* the way point goes with the story: the target level's start reads it as the single player's (its race and
	 * arrow are placed at the spawn, Coop_StandIn) */
	for (int k = 0; cp_entry[k]; k++) {
		const char *v = SaveVar_Get(cp_entry[k]);
		Str_PrintfQuiet(key, sizeof(key), CP_ENTRY "%s", cp_entry[k]);
		SaveVar_Set(key, !strcmp(cp_entry[k], "Map") ? map : v ? v : "");
	}
	/* the story of now first: the target instance starts with it */
	Cp_Send();
	Cp_Video(party ? -1 : i);
	Con_Printf("coop: mission level change of player %d to %s (%s)%s\n", i, level, spawn ? spawn : "", party ?
		", the party goes along" : "");
	if (party) {
		Cp_Notice(-1, "MP_CoopTravel", level);
	}
	/* a way point from a door class (ComeFrom "DoorEditShip"): the members arrive in that door, as the single player's
	 * start puts the player in its elevator */
	const char *from = SaveVar_Get("ComeFrom");
	return Travel_Go(i, level, spawn ? spawn : "", from && !strncmp(from, "Door", 4) ? from : "", party);
}

void Coop_EntityGone(cEntity *e) {
	if (!Coop_On() || (cp.started && !cp.starting) || cp.ngone == CP_GONE || e == cp.standIn || Players_Of(e) >= 0) {
		return;
	}
	for (int k = 0; k < cp.ngone; k++) {
		if (!strcmp(cp.gone[k], e->name)) {
			return;
		}
	}
	Str_Copy(cp.gone[cp.ngone++], e->name, CP_NAME);
}

/* the entities the level's start deleted (a mission's Init: the client's level start runs none) to member i */
static void Cp_TellGone(int i) {
	static uint8_t buf[CP_GONE_MSG];
	NetRepBuf w;
	if (!cp.ngone) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_GONE);
	NetRep_PutU16(&w, (uint32_t)cp.ngone);
	for (int k = 0; k < cp.ngone; k++) {
		NetRep_PutStr(&w, cp.gone[k]);
	}
	if (!w.bad) {
		Cp_ToClient(i, buf, w.size);
	}
}

void Coop_Died(int i, cEntity *e) {
	if (!Coop_On() || cp.partyDead) {
		return;
	}
	for (int k = 0; k < PLAYERS_MAX; k++) {
		if (k != i && Rep_PlayerConn(k) >= 0 && !Rep_PlayerDead(k) && Players_Get(k)) {
			return;
		}
	}
	/* the party died: the single player's death (the mission's hook, else the end of the game: Coop_MissionFailed) */
	Con_Printf("coop: every member here died (player %d last)\n", i);
	nativeVal_t a[2];
	Native_SetStr(&a[0], e->name);
	Native_SetStr(&a[1], "");
	cp.partyDead = 1;
	Players_Bind(i);
	if (!Mode_Import("Chars") || !PORT_CALL(Chars, MainCharDeath, NULL, "vv", &a[0], &a[1])) {
		Native_ErrPrint();
	}
	cp.partyDead = 0;
	Native_Free(&a[0]);
	Native_Free(&a[1]);
}

byte Coop_PartyDeath(void) {
	return cp.partyDead;
}

byte Coop_MissionFailed(void) {
	if (!Coop_On()) {
		return 0;
	}
	if (cp.failAt <= 0.0) {
		cp.failAt = Instance_Time() + CP_FAIL_SECS;
		Cp_Notice(-1, "MP_CoopMissionFailed", "");
		Con_Printf("coop: the mission failed: the level starts again in %.0f s\n", CP_FAIL_SECS);
	}
	return 1;
}

byte Coop_PartyTravel(const char *level) {
	const char *s = strrchr(level, '/');
	level = s ? s + 1 : level;
	if (!Coop_On() || (!CoordCoop_Chapter(level) && !CoordCoop_Chapter(Instance_Level()))) {
		return 0;
	}
	Cp_Send();
	Cp_Notice(-1, "MP_CoopTravel", level);
	return 1;
}

/* a free place next to member m, the k-th around it */
static byte Cp_Beside(const cEntity *m, int k, vec3_t *out) {
	float a = (float)k * 1.3f;
	vec3_t at = { m->pos.x + CP_GATHER * cosf(a), m->pos.y + 20.0f, m->pos.z + CP_GATHER * sinf(a) };
	if (Rep_FreeSpot(&at, 0, out)) {
		return 1;
	}
	return Rep_FreeSpot(&m->pos, 1, out);
}

byte Coop_Spawn(int player, vec3_t *pos, float *yaw) {
	char found[64];
	vec3_t ang;
	if (!Coop_On()) {
		return 0;
	}
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *m = i != player ? Players_Get(i) : NULL;
		if (m && Rep_PlayerConn(i) >= 0 && NetPl_Kind(m) == NETREP_CHAR && Cp_Beside(m, player + 1, pos)) {
			*yaw = Obj(m)->ang.y;
			Con_Printf("coop: player %d starts next to player %d\n", player, i);
			return 1;
		}
	}
	/* the single player's start without a way point (Chars.CreateStartupPlayer): the first spawn but the junkyard's */
	DummySearch_Start("DM_Player_Spawn*", 1);
	while (DummySearch_NextEuler(found, sizeof(found), pos, &ang)) {
		if (strcmp(found, "DM_Player_Spawn_Junkyard")) {
			*yaw = ang.y + M_PI_F;
			Con_Printf("coop: player %d starts at %s\n", player, found);
			return 1;
		}
	}
	return 0;
}

cEntity *Coop_HangarShip(const vec3_t *p, float yaw) {
	char var[CP_NAME];
	nativeVal_t c[3], pos, cad, out;
	const char *n = SaveVar_Get("Hangar.iHangarShip");
	int k = n ? crt_atoi(n) : -1;
	/* the hangar's aux slot (9) is the junkyard's lifeboat of the story's start: a standard ship then, as the single
	 * player takes a parked one; the lifeboat where the level has none of those (GDB: the bishops' ships only) */
	Native_SetFloat(&c[0], p->x);
	Native_SetFloat(&c[1], p->y);
	Native_SetFloat(&c[2], p->z);
	Mode_Tuple3(&pos, &c[0], &c[1], &c[2]);
	if (k == 9) {
		nativeVal_t ang;
		Native_SetFloat(&ang, yaw);
		Native_SetNone(&out);
		cEntity *std = PORT_CALL(Parking, CreateParkedShip, &out, "vv", &pos, &ang) && out.type == NV_ENT ? Native_Ent(&out) : NULL;
		Native_ErrClear();
		Native_Free(&out);
		if (std) {
			Native_Free(&pos);
			return std;
		}
		Str_Copy(var, "Hangar.HangarShipAux", sizeof(var));
	} else {
		Str_PrintfQuiet(var, sizeof(var), "Hangar.HangarShip%d", k + 1);
	}
	const char *text = k >= 0 && k <= 9 ? SaveVar_Get(var) : NULL;
	if (!text || !text[0] || text[0] == '<') {
		Native_Free(&pos);
		return NULL;
	}
	/* Parking.CreatePlayerShip's ship of the hangar the story flies, a parked one for the member */
	Native_SetStr(&cad, text);
	Native_SetNone(&out);
	cEntity *ship = NULL;
	if (Mode_Import("ShipEdit") && PORT_CALL(ShipEdit, CreateShipFromSaveVar, &out, "vvs", &cad, &pos, "Parked") &&
		out.type == NV_ENT && (ship = Native_Ent(&out)) != NULL) {
		int one = 1;
		vec3_t ang = { 0.0f, yaw, 0.0f };
		Entity_VarI(ship, "Active", &one, 1);
		Entity_VarI(ship, "ActiveCMask", &one, 1);
		Obj_SetAng(ship, &ang);
		if (!PORT_CALL(Parking, SetShipAsParked, NULL, "s", ship->name)) {
			Native_ErrPrint();
		}
		Con_Printf("coop: the party's hangar ship %s (%s)\n", ship->name, var);
	} else {
		Native_ErrPrint();
	}
	Native_Free(&out);
	Native_Free(&cad);
	Native_Free(&pos);
	return ship;
}

byte Coop_NoShipCall(void) {
	return Coop_On() && !g_IsMapOutDoor;
}

byte Coop_UseAny(cEntity *t) {
	return Coop_On() && t && !Entity_IsType(t, TYEN_CAR);
}

/* ---- the cutscenes ---- */

/* a table of the scene's classes and actors (the lines of the modules a scene's load uses) */
static byte Cp_SceneLines(const char *const *lines) {
	static const char *const mods[] = { "Scene.", "SAct.", "MakeChar.", NULL };
	for (; *lines; lines++) {
		for (int k = 0; mods[k]; k++) {
			if (!strncmp(*lines, mods[k], strlen(mods[k]))) {
				return 1;
			}
		}
	}
	return 0;
}

/* the entry of scene cls (a new one when missing), NULL when the list is full */
static CpScene *Cp_SceneSlot(const char *cls) {
	int k = 0;
	while (k < cp.nscenes && strcmp(cp.scenes[k].cls, cls)) {
		k++;
	}
	if (k == COOP_SCENES) {
		return NULL;
	}
	cp.nscenes += k == cp.nscenes;
	Str_Copy(cp.scenes[k].cls, cls, CP_NAME);
	return &cp.scenes[k];
}

void Coop_SceneTable(const char *module, const char *const *lines) {
	static const char head[] = "Scene.Load|";
	char cls[CP_NAME];
	if (!Coop_On() || !lines || !lines[0]) {
		return;
	}
	if (strncmp(lines[0], head, sizeof(head) - 1)) {
		CpScene *sc = cp.loading >= 0 ? &cp.scenes[cp.loading] : NULL;
		if (sc && sc->module == module && sc->ntables < CP_TABLES && Cp_SceneLines(lines)) {
			sc->lines[sc->ntables++] = lines;
		} else {
			cp.loading = -1;
		}
		return;
	}
	const char *c = lines[0] + sizeof(head) - 1;
	int n = 0;
	while (c[n] && c[n] != '|' && n < CP_NAME - 1) {
		n++;
	}
	memcpy(cls, c, (size_t)n);
	cls[n] = 0;
	CpScene *sc = Cp_SceneSlot(cls);
	if (!sc) {
		return;
	}
	sc->module = module;
	sc->ntables = 1;
	sc->lines[0] = lines;
	sc->dataModule[0] = sc->dataSection[0] = 0;
	cp.loading = (int)(sc - cp.scenes);
}

void Coop_DataSection(const char *module, const char *section) {
	if (!Coop_On()) {
		return;
	}
	Str_Copy(cp.dataModule, module ? module : "", CP_NAME);
	Str_Copy(cp.dataSection, section ? section : "", CP_NAME);
}

void Coop_SceneLoad(const char *cls) {
	CpScene *sc;
	/* only the load of a data section (a call table's load is kept by Coop_SceneTable) */
	if (!Coop_On() || !cp.dataSection[0] || !(sc = Cp_SceneSlot(cls))) {
		return;
	}
	sc->module = "";
	sc->ntables = 0;
	Str_Copy(sc->dataModule, cp.dataModule, CP_NAME);
	Str_Copy(sc->dataSection, cp.dataSection, CP_NAME);
	Con_Printf("coop: scene %s loads from the data section %s of %s\n", cls, cp.dataSection, cp.dataModule);
}

/* the message of the scene that plays (its table, or the class alone of a scene of the level's own data); 0 when it
 * does not fit */
static int Cp_SceneMsg(uint8_t *buf, int size) {
	NetRepBuf w;
	NetRepBuf_Init(&w, buf, size);
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_SCENE);
	if (cp.scene < 0) {
		NetRep_PutStr(&w, "");
		NetRep_PutStr(&w, cp.sceneCls);
		NetRep_PutU16(&w, 0);
		NetRep_PutStr(&w, "");
		NetRep_PutStr(&w, "");
		return w.size;
	}
	const CpScene *sc = &cp.scenes[cp.scene];
	int n = 0;
	for (int t = 0; t < sc->ntables; t++) {
		for (const char *const *l = sc->lines[t]; *l && n < CP_SCENE_LINES; l++) {
			n++;
		}
	}
	NetRep_PutStr(&w, sc->module);
	NetRep_PutStr(&w, cp.sceneCls);
	NetRep_PutU16(&w, (uint32_t)n);
	int m = 0;
	for (int t = 0; t < sc->ntables; t++) {
		for (const char *const *l = sc->lines[t]; *l && m < n; l++, m++) {
			int len = (int)strlen(*l);
			NetRep_PutU16(&w, (uint32_t)len);
			CoordMsg_PutBytes(&w, *l, len);
		}
	}
	NetRep_PutStr(&w, sc->dataModule);
	NetRep_PutStr(&w, sc->dataSection);
	return w.bad ? 0 : w.size;
}

void Coop_ScenePlay(const char *cls) {
	static uint8_t buf[CP_SCENE_MSG];
	uint8_t nb[256];
	NetRepBuf nw;

	if (!Coop_On()) {
		return;
	}
	cp.inScene = 1;
	Input_SelectActionSetNamed("");
	cp.serial++;
	cp.scenePlayer = Cp_Player();
	cp.scene = -1;
	Str_Copy(cp.sceneCls, cls, sizeof(cp.sceneCls));
	for (int k = 0; k < cp.nscenes; k++) {
		if (!strcmp(cp.scenes[k].cls, cls)) {
			cp.scene = k;
		}
	}
	/* the members elsewhere: a notice through the coordinator */
	NetRepBuf_Init(&nw, nb, sizeof(nb));
	NetRep_PutU8(&nw, COORD_MSG_PNOTICE);
	char text[96];
	Str_PrintfQuiet(text, sizeof(text), "MP_CoopScene|%s", Instance_Level());
	NetRep_PutStr(&nw, text);
	Travel_SendCoord(nb, nw.size);
	int size = Cp_SceneMsg(buf, sizeof(buf));
	if (!size) {
		Con_Printf("coop: the table of scene %s does not fit a message\n", cls);
		cp.scene = -1;
		size = Cp_SceneMsg(buf, sizeof(buf));
	}
	Cp_ToAll(buf, size);
	if (cp.scene < 0) {
		/* a scene of the level's own data (map.def): the clients have its class from their level's start */
		Con_Printf("coop: scene %s plays (no table: the clients play the class they have)\n", cls);
	} else {
		Con_Printf("coop: scene %s plays on the clients (player %d)\n", cls, cp.scenePlayer);
	}
}

void Coop_Spawned(cEntity *e) {
	if (Coop_On() && NetPl_Kind(e) == NETREP_CHAR) {
		WChar(e)->listenOn = 1;
		/* a talk turns the camera of view 0 too */
		Coop_SceneCamera();
	}
}

byte Coop_StartPos(vec3_t *p, float *yaw) {
	/* where the single player would stand: the way point's spawn, else the level's start */
	const char *spawn = SaveVar_Get("Spawn");
	char found[64];
	vec3_t ang;
	if (!Coop_On()) {
		return 0;
	}
	if (spawn && spawn[0]) {
		DummySearch_Start(spawn, 1);
	}
	if (spawn && spawn[0] && DummySearch_NextEuler(found, sizeof(found), p, &ang)) {
		*yaw = ang.y + M_PI_F;
		return 1;
	}
	return Coop_Spawn(-1, p, yaw);
}

void Coop_StandIn(byte on) {
	vec3_t p;
	float yaw = 0.0f;
	/* the Init of the map and of the mission libraries runs (Coop_EntityGone keeps what it deletes) */
	cp.starting += on ? 1 : cp.starting > 0 ? -1 : 0;
	if (!on && !cp.starting && Coop_On()) {
		Con_Printf("coop: the level's start deleted %d entities so far (told to the clients)\n", cp.ngone);
	}
	if (!on) {
		if (cp.standIn && World_FindEntity(COOP_STANDIN) == cp.standIn) {
			cEntity_Del(cp.standIn);
		}
		cp.standIn = NULL;
		return;
	}
	if (!Coop_On() || Players_Any() || !Coop_StartPos(&p, &yaw)) {
		return;
	}
	cp.standIn = cWorld_CreateEntity(g_pWorld, COOP_STANDIN, p.x, p.y, p.z, TYEN_WALKCHAR);
	if (cp.standIn) {
		vec3_t ang = { 0.0f, yaw, 0.0f };
		Faction_EnsureClass("Dtritus");
		WithLife_SetActCtrl(cp.standIn, "Dtritus");
		Obj_SetAng(cp.standIn, &ang);
		Con_Printf("coop: a stand-in player for the level's start at %.0f %.0f %.0f\n", (double)p.x, (double)p.y,
			(double)p.z);
	}
}

cEntity *Coop_StandInEntity(void) {
	return cp.standIn;
}

const char *Coop_MainCharName(void) {
	int i = Coop_On() ? Players_Bound() : PLAYERS_NONE;
	if (i == PLAYERS_NONE) {
		return NULL;
	}
	Str_PrintfQuiet(cp.mainChar, sizeof(cp.mainChar), REP_PLAYER_PREFIX "%d_c%u", i, ++cp.mainChars);
	return cp.mainChar;
}

byte Coop_Adopts(int i, cEntity *e) {
	int kind = Coop_On() ? NetPl_Kind(e) : 0;
	if ((kind != NETREP_CHAR && kind != NETREP_SHIP) || WLife(e)->life <= 0) {
		return 0;
	}
	cActCtrl *a = WLife(e)->actCtrl;
	const char *cls = a && a->cls ? a->cls->name : "";
	if (kind == NETREP_CHAR) {
		Faction_SetChar(i, cls);
		Coop_Spawned(e);
	}
	Con_Printf("coop: player %d plays as %s (%s), a character of the scripts\n", i, e->name, cls);
	return 1;
}

void Coop_ActCtrlSet(cEntity *e) {
	int i = Coop_On() ? Players_Of(e) : -1;
	cActCtrl *a = i >= 0 && NetPl_Kind(e) == NETREP_CHAR ? WLife(e)->actCtrl : NULL;
	if (a && a->cls) {
		Faction_SetChar(i, a->cls->name);
		Con_Printf("coop: player %d is a %s now (its class set by the scripts)\n", i, a->cls->name);
	}
}

/* the story to the clients' copies: the changed variables (all: the whole story) to client i (-1: all here) */
typedef struct CpStoryOut {
	int to;
	byte all;
	NetRepBuf w;
	int at, n;
} CpStoryOut;

static void Cp_StoryFlush(CpStoryOut *o) {
	if (o->n && !o->w.bad) {
		o->w.p[o->at] = (uint8_t)(o->n & 0xff);
		o->w.p[o->at + 1] = (uint8_t)(o->n >> 8);
		if (o->to < 0) {
			Cp_ToAll(o->w.p, o->w.size);
		} else {
			Cp_ToClient(o->to, o->w.p, o->w.size);
		}
	}
	NetRepBuf_Init(&o->w, o->w.p, NET2_MAX_MESSAGE);
	NetRep_PutU8(&o->w, COOP_MSG);
	NetRep_PutU8(&o->w, COOP_Y_STORY);
	o->at = o->w.size;
	NetRep_PutU16(&o->w, 0);
	o->n = 0;
}

static void Cp_StoryPut(CpStoryOut *o, const char *name, const char *value) {
	int len = (int)strlen(name) + (value ? (int)strlen(value) : 0) + 8;
	if (o->w.size + len > NET2_MAX_MESSAGE) {
		Cp_StoryFlush(o);
	}
	CoordStory_Put(&o->w, value ? COORD_OP_SET : COORD_OP_DEL, name, value);
	o->n++;
}

static void Cp_StoryEach(const char *name, const char *value, void *u) {
	if (Cp_ClientStoryVar(name)) {
		Cp_StoryPut(u, name, value);
	}
}

static void Cp_TellStory(int to, byte all) {
	static uint8_t buf[NET2_MAX_MESSAGE];
	CpStoryOut o = { .to = to, .all = all };
	o.w.p = buf;
	Cp_StoryFlush(&o);
	if (all) {
		SaveVars_Each(Cp_StoryEach, &o);
	} else {
		for (int k = 0; k < cp.ncdirty; k++) {
			Cp_StoryPut(&o, cp.cdirty[k], SaveVar_Get(cp.cdirty[k]));
		}
	}
	Cp_StoryFlush(&o);
}

/* a talk the level's Init started with the stand-in (TalkToTraitoriusAfterCTF: Traitorius' talk in front of the
 * player) goes on with member i as its player: Speech.PlayerName (the stand-in is gone after the Init), its line again */
static void Cp_TalkPlayer(int i) {
	nativeVal_t pn, v;
	cEntity *e = Players_Get(i);
	Native_SetNone(&pn);
	if (e && Mode_ModGet("Speech", "PlayerName", &pn) && pn.type == NV_STR &&
		!strncmp(Native_Text(&pn), COOP_STANDIN, sizeof(COOP_STANDIN) - 1)) {
		Native_SetStr(&v, e->name);
		/* its line again for the member (the stand-in's had no client and no end) */
		nativeVal_t cur;
		Native_SetNone(&cur);
		Players_Bind(i);
		if (Mode_SetModVar("Speech", "PlayerName", &v) && Mode_ModGet("Speech", "CurrNI", &cur) && cur.type == NV_INT &&
			PORT_CALL(Speech, RunNI, NULL, "v", &cur)) {
			Con_Printf("coop: the talk of the level's start goes on with player %d\n", i);
		} else {
			Native_ErrPrint();
		}
		Players_BindTurn();
		Native_Free(&cur);
		Native_Free(&v);
	}
	Native_ErrClear();
	Native_Free(&pn);
}

/* a scene of the level's start that the stand-in started (IntroHuman2's news: its end's callback gets the scene's
 * player, Scene.usr, the stand-in that is gone) is the first member's: it is the scene's player now */
static void Cp_ScenePlayer(int i) {
	nativeVal_t usr, name, h;
	cEntity *e = Players_Get(i);
	if (!cp.inScene || cp.scenePlayer >= 0 || !e) {
		return;
	}
	Native_SetNone(&usr);
	Native_SetNone(&name);
	Native_SetNone(&h);
	byte gone = !Mode_ModGet("Scene", "usr", &usr) || !Native_True(&usr) || !Mode_VGet(&usr, "Name", &name) ||
		name.type != NV_STR || !strncmp(Native_Text(&name), COOP_STANDIN, sizeof(COOP_STANDIN) - 1);
	Native_ErrClear();
	if (gone && Mode_EntHandle(e, &h) && Mode_SetModVar("Scene", "usr", &h)) {
		cp.scenePlayer = i;
		Con_Printf("coop: the scene %s of the level's start is player %d's\n", cp.sceneCls, i);
	}
	Native_ErrPrint();
	Native_Free(&usr);
	Native_Free(&name);
	Native_Free(&h);
}

void Coop_Joined(int i) {
	static uint8_t buf[CP_SCENE_MSG];
	nativeVal_t h;
	if (!Coop_On()) {
		return;
	}
	/* the client's copy of the story for its next level starts */
	Cp_TellStory(i, 1);
	Cp_TellGone(i);
	cp.pl[i].menu = 0;
	cp.pl[i].held[0] = 0;
	for (int k = 0; cp_hud[k]; k++) {
		Cp_HudVar(i, cp_hud[k]);
	}
	if (cp.waiting) {
		cp.waiting = 0;
		Timer_SetSpeed(&g_pWorld->timer, 1.0f);
		Con_Printf("coop: player %d came: the level goes on\n", i);
		Cp_TalkPlayer(i);
		Cp_ScenePlayer(i);
		Elevator_FirstMember();
	}
	/* an empty list of options: the client knows it plays the coop story (its keys) */
	uint8_t none[16];
	NetRepBuf nw;
	NetRepBuf_Init(&nw, none, sizeof(none));
	Cp_OptionsMsg(&nw, NULL);
	cp.pl[i].sentSize = 0;
	Cp_ToClient(i, none, nw.size);
	if (!cp.inScene) {
		return;
	}
	/* hidden and stopped as the scene's start did to the members there (its end shows it again) */
	cEntity *e = Players_Get(i);
	Native_SetNone(&h);
	if (e && (!Mode_EntHandle(e, &h) || !Mode_Import("Scene") || !PORT_CALL(Scene, HideThis, NULL, "v", &h))) {
		Native_ErrPrint();
	}
	Native_Free(&h);
	int size = Cp_SceneMsg(buf, sizeof(buf));
	if (size) {
		Cp_ToClient(i, buf, size);
	}
	Con_Printf("coop: player %d came in during scene %s: it plays on its client too\n", i, cp.sceneCls);
}

byte Coop_SceneHolds(void) {
	return Coop_On() && cp.inScene;
}

byte Coop_Holds(int i) {
	return Coop_On() && (cp.inScene || (i >= 0 && i < PLAYERS_MAX && (cp.pl[i].menu == COOP_MENU_SHIPEDIT ||
		cp.pl[i].menu == COOP_MENU_SPUTNIK || Sputniks_CoopHolds(i) || cp.pl[i].held[0])));
}

int Coop_HeldPlayer(cEntity *e) {
	for (int i = 0; Coop_On() && e && i < PLAYERS_MAX; i++) {
		if (cp.pl[i].held[0] && !strcmp(cp.pl[i].held, e->name)) {
			return i;
		}
	}
	return -1;
}

int Coop_UsrRead(int slot) {
	return Coop_On() && slot >= PLAYERS_NET_SLOT && slot < PLAYERS_NET_SLOT + PLAYERS_MAX ? 0 : slot;
}

int Coop_UsrSlot(cEntity *e, int old, int slot) {
	if (!Coop_On()) {
		return slot;
	}
	if (slot == -1 && old >= PLAYERS_NET_SLOT && old < PLAYERS_NET_SLOT + PLAYERS_MAX) {
		/* a dead one is no ride of the scripts: its member gets a new character */
		if (!Entity_IsType(e, TYEN_WITH_LIFE) || WLife(e)->life > 0) {
			Str_Copy(cp.pl[old - PLAYERS_NET_SLOT].held, e->name, CP_NAME);
		}
		return slot;
	}
	if (slot >= PLAYERS_NET_SLOT && slot < PLAYERS_NET_SLOT + PLAYERS_MAX) {
		cp.pl[slot - PLAYERS_NET_SLOT].held[0] = 0;
	}
	if (slot < 0 || slot >= MAX_LOCAL_PLAYERS) {
		return slot;
	}
	if (old >= PLAYERS_NET_SLOT && old < PLAYERS_NET_SLOT + PLAYERS_MAX) {
		return old;		/* a member's own (the scripts read its slot as 0, Coop_UsrRead) */
	}
	int i = Coop_HeldPlayer(e);
	if (i < 0) {
		i = Players_Bound();
	}
	if (i == PLAYERS_NONE) {
		return slot;
	}
	cp.pl[i].held[0] = 0;
	return PLAYERS_NET_SLOT + i;
}

/* the other members on foot next to the member of the scene */
static void Cp_Gather(void) {
	cEntity *m = cp.scenePlayer >= 0 ? Players_Get(cp.scenePlayer) : NULL;
	if (!m) {
		return;
	}
	int k = 0;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = Players_Get(i);
		if (i == cp.scenePlayer || !e || Rep_PlayerConn(i) < 0 || NetPl_Kind(e) != NETREP_CHAR) {
			continue;
		}
		vec3_t p;
		if (!Cp_Beside(m, ++k, &p)) {
			continue;
		}
		e->pos = p;
		Obj(e)->ang.y = Obj(m)->ang.y;
		WChar(e)->view.y = Obj(m)->ang.y;
		Con_Printf("coop: player %d stands next to player %d after the scene\n", i, cp.scenePlayer);
	}
}

void Coop_SceneEnding(void) {
	if (!Coop_On() || !cp.inScene) {
		return;
	}
	cp.endingSerial = cp.serial;
	cp.endingTable = cp.scene;
	Str_Copy(cp.endingCls, cp.sceneCls, sizeof(cp.endingCls));
	/* the scene's member (hidden by the scene its entity is no player found: its connection counts) */
	if (cp.scenePlayer >= 0 && Rep_PlayerConn(cp.scenePlayer) >= 0) {
		Players_Bind(cp.scenePlayer);
	}
}

void Coop_SceneEnd(void) {
	uint8_t buf[128];
	NetRepBuf w;
	if (!Coop_On() || !cp.inScene || cp.endingSerial == 0) {
		return;
	}
	if (cp.endingTable >= 0) {
		NetRepBuf_Init(&w, buf, sizeof(buf));
		NetRep_PutU8(&w, COOP_MSG);
		NetRep_PutU8(&w, COOP_Y_SCENEEND);
		NetRep_PutStr(&w, cp.endingCls);
		Cp_ToAll(buf, w.size);
	}
	Con_Printf("coop: scene %s ended\n", cp.endingCls);
	/* the callback started the next scene: that one plays on */
	if (cp.serial == cp.endingSerial) {
		Cp_Gather();
		cp.inScene = 0;
		cp.scene = -1;
		Input_SelectActionSetNamed("");
	}
	cp.endingSerial = 0;
	Players_BindTurn();
}

/* ---- the texts and voices of the scripts ---- */

/* to member s and the members within COOP_HEAR of it (s none: to all) */
static void Cp_ToNear(int s, const uint8_t *buf, int size) {
	cEntity *se = s >= 0 && s < PLAYERS_MAX ? Players_Get(s) : NULL;
	for (int i = 0; i < PLAYERS_MAX; i++) {
		cEntity *e = Players_Get(i);
		if (Rep_PlayerConn(i) < 0 || (!e && i != s)) {
			continue;
		}
		float dx = se && e ? e->pos.x - se->pos.x : 0.0f, dy = se && e ? e->pos.y - se->pos.y : 0.0f;
		float dz = se && e ? e->pos.z - se->pos.z : 0.0f;
		if (i == s || dx * dx + dy * dy + dz * dz <= COOP_HEAR * COOP_HEAR) {
			Cp_ToClient(i, buf, size);
		}
	}
}

/* the mission's member here: the one bound when the story's mission changed last, else the first member that came
 * while it was gone (the one that traveled to the mission's level); -1 */
static int Cp_MissionPlayer(void) {
	int i = cp.missionPlayer;
	if (i >= 0 && i < PLAYERS_MAX && Rep_PlayerConn(i) >= 0 && Players_Joined(i) == cp.missionJoined) {
		return i;
	}
	cp.missionPlayer = -1;
	for (i = 0; i < PLAYERS_MAX; i++) {
		if (Rep_PlayerConn(i) >= 0) {
			Cp_SetMissionPlayer(i);
			return i;
		}
	}
	return -1;
}

byte Coop_BindMission(void) {
	int i = Coop_On() ? Cp_MissionPlayer() : -1;
	if (i < 0) {
		return 0;
	}
	Players_Bind(i);
	return 1;
}

static void Cp_SetMissionPlayer(int i) {
	if (i >= 0 && i < PLAYERS_MAX && Rep_PlayerConn(i) >= 0 && i != cp.missionPlayer) {
		cp.missionPlayer = i;
		cp.missionJoined = Players_Joined(i);
		Con_Printf("coop: player %d is the mission's member here\n", i);
	}
}

/* a scene the clients play themselves makes its own texts and voices */
static byte Cp_Quiet(void) {
	return !Coop_On() || cp.inScene;
}

void Coop_SpeechText(const char *text, float time, int r, int g, int b) {
	uint8_t buf[1024];
	NetRepBuf w;
	if (Cp_Quiet()) {
		return;
	}
	int n = (int)strlen(text);
	n = n > 900 ? 900 : n;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_SPEECH);
	NetRep_PutU16(&w, (uint32_t)n);
	CoordMsg_PutBytes(&w, text, n);
	NetRep_PutU32(&w, (uint32_t)(time > 0.0f ? time * 1000.0f : 0.0f));
	NetRep_PutU8(&w, (uint32_t)(r & 255));
	NetRep_PutU8(&w, (uint32_t)(g & 255));
	NetRep_PutU8(&w, (uint32_t)(b & 255));
	/* the speaker's client shows the conversation; a remote communication the mission's member and the members near it
	 * */
	int i = Players_Bound();
	Con_Printf("coop: %s for player %d: %.160s\n", cp.remote ? "remote communication" : "speech",
		i == PLAYERS_NONE ? -1 : i, text);
	if (!cp.remote && i != PLAYERS_NONE) {
		cp.pl[i].talkUntil = Instance_Time() + time;
	}
	if (cp.remote && i != PLAYERS_NONE) {
		Cp_ToNear(i, buf, w.size);
	} else if (i != PLAYERS_NONE && Rep_PlayerConn(i) >= 0) {
		Cp_ToClient(i, buf, w.size);
	} else {
		Cp_ToAll(buf, w.size);
	}
}

void Coop_MsgText(const char *text, float time, const char *font) {
	uint8_t buf[1200];
	NetRepBuf w;
	if (Cp_Quiet()) {
		return;
	}
	int n = (int)strlen(text);
	n = n > 1000 ? 1000 : n;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MSG);
	NetRep_PutU16(&w, (uint32_t)n);
	CoordMsg_PutBytes(&w, text, n);
	NetRep_PutU32(&w, (uint32_t)(time > 0.0f ? time * 1000.0f : 0.0f));
	NetRep_PutStr(&w, font ? font : "");
	Cp_ToAll(buf, w.size);
}

void Coop_Fade(const int *from, const int *to, double time) {
	uint8_t buf[16];
	NetRepBuf w;
	if (!Coop_On()) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_FADE);
	for (int k = 0; k < 4; k++) {
		NetRep_PutU8(&w, (uint32_t)(from[k] & 255));
	}
	for (int k = 0; k < 4; k++) {
		NetRep_PutU8(&w, (uint32_t)(to[k] & 255));
	}
	NetRep_PutU32(&w, (uint32_t)(time > 0.0 ? time * 1000.0 : 0.0));
	Cp_ToAll(buf, w.size);
}

void Coop_OpenVoice(const char *file, int channel) {
	if (Coop_On() && channel >= 0 && channel < CP_VOICES) {
		Str_Copy(cp.voice[channel], file, sizeof(cp.voice[channel]));
	}
}

void Coop_PlayVoice(int channel) {
	uint8_t buf[192];
	NetRepBuf w;
	if (Cp_Quiet() || channel < 0 || channel >= CP_VOICES || !cp.voice[channel][0]) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_VOICE);
	NetRep_PutU8(&w, (uint32_t)channel);
	NetRep_PutStr(&w, cp.voice[channel]);
	/* the speaker and the members near it hear it */
	Cp_ToNear(Players_Bound(), buf, w.size);
}

void Coop_RemoteCom(byte on) {
	if (!Coop_On() || (!on && !cp.remote)) {
		return;
	}
	/* nested (the voice's call around the text's): the outer one binds and gives the binding back */
	if (on && cp.remote++) {
		return;
	}
	if (on) {
		int i = Cp_MissionPlayer();
		cp.remoteWas = Players_Bound();
		if (i >= 0) {
			Players_Bind(i);
		}
	} else if (!--cp.remote) {
		Players_Bind(cp.remoteWas);
	}
}

void Coop_RemoteHead(const char *model, const char *text, int index) {
	uint8_t buf[512];
	NetRepBuf w;
	if (Cp_Quiet() || !cp.remote) {
		return;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_HEAD);
	NetRep_PutStr(&w, model);
	NetRep_PutStr(&w, text);
	NetRep_PutU8(&w, (uint32_t)(index & 255));
	if (!w.bad) {
		Cp_ToNear(Players_Bound(), buf, w.size);
	}
}

/* ---- the menus of the scripts ---- */

/* the save variables of the ship editor (and the act of the game for the editor's choice of parts) as COOP_VARS */
typedef struct CpVars {
	NetRepBuf *w;
	byte act;
	int count;
} CpVars;

static void Cp_PutVar(const char *name, const char *value, void *u) {
	CpVars *v = u;
	int n = (int)strlen(value);
	byte hangar = !strncmp(name, CP_HANGAR, sizeof(CP_HANGAR) - 1);
	if ((!hangar && (!v->act || strcmp(name, "GameAct"))) || strlen(name) > 255 || n > COORD_STORY_VALUE) {
		return;
	}
	NetRep_PutStr(v->w, name);
	NetRep_PutU16(v->w, (uint32_t)n);
	CoordMsg_PutBytes(v->w, value, n);
	v->count++;
}

static void Cp_PutVars(NetRepBuf *w, byte act) {
	CpVars v = { w, act, 0 };
	int at = w->size;
	NetRep_PutU16(w, 0);
	SaveVars_Each(Cp_PutVar, &v);
	if (!w->bad) {
		w->p[at] = (uint8_t)v.count;
		w->p[at + 1] = (uint8_t)(v.count >> 8);
	}
}

/* COOP_VARS into the save variables (only the ship editor's, GameAct too with act) */
static void Cp_GetVars(NetRepReader *r, byte act) {
	static char val[COORD_STORY_VALUE + 1];
	char name[256];
	int count = (int)NetRep_GetU16(r);
	for (int k = 0; k < count && !r->bad; k++) {
		NetRep_GetStr(r, name, sizeof(name));
		int n = (int)NetRep_GetU16(r);
		if (n > COORD_STORY_VALUE) {
			r->bad = 1;
			break;
		}
		CoordMsg_GetBytes(r, val, n);
		val[n] = 0;
		if (!r->bad && (!strncmp(name, CP_HANGAR, sizeof(CP_HANGAR) - 1) || (act && !strcmp(name, "GameAct")))) {
			SaveVar_Set(name, val);
		}
	}
}

void Coop_PutHangar(NetRepBuf *w) {
	Cp_PutVars(w, 0);
}

void Coop_GetHangar(NetRepReader *r) {
	Cp_GetVars(r, 0);
}

byte Coop_ShipEditAway(void) {
	int i = Coop_On() ? Cp_Player() : -1;
	return i >= 0 && cp.pl[i].menu == COOP_MENU_SHIPEDIT;
}

const char *Coop_ActionSet(const char *name) {
	return !Coop_On() ? name : cp.inScene ? "Inactive" : "Walk";
}

byte Coop_KeepTime(void) {
	return Coop_On();
}

byte Coop_MenuYesNo(const char *title, const char *yes, const char *no) {
	uint8_t buf[CP_MENU_TITLE + 16];
	NetRepBuf w;
	if (!Coop_On()) {
		return 0;
	}
	int i = Cp_Player();
	if (i < 0 || Rep_PlayerConn(i) < 0) {
		return 0;
	}
	int n = (int)strlen(title);
	n = n > CP_MENU_TITLE ? CP_MENU_TITLE : n;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MENU);
	NetRep_PutU8(&w, COOP_MENU_YESNO);
	NetRep_PutU16(&w, (uint32_t)n);
	CoordMsg_PutBytes(&w, title, n);
	cp.pl[i].menu = COOP_MENU_YESNO;
	Str_Copy(cp.pl[i].yes, yes, CP_NAME);
	Str_Copy(cp.pl[i].no, no, CP_NAME);
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: player %d is asked (%s or %s)\n", i, yes, no);
	return 1;
}

byte Coop_DealScores(int deal, const char *title, const char *yes, const char *no) {
	char mod[CP_NAME];
	if (!Coop_On()) {
		return 0;
	}
	if (deal == 1) {
		return Coop_MenuYesNo(title, yes, no);
	}
	/* Continue: its action after the members read the table on their HUDs */
	Str_Copy(mod, yes, sizeof(mod));
	char *dot = strchr(mod, '.');
	if (!dot) {
		return 0;
	}
	*dot = 0;
	/* the arguments of a menu's action (the player, the control), as Cp_MenuFunc gives them */
	nativeVal_t args[2];
	Native_SetInt(&args[0], 0);
	Native_SetText(&args[1], "", 0);
	byte ok = Mode_SchedAt(Mode_Time() + CP_DEAL_SECS, mod, dot + 1, args, 2, NULL);
	Native_Free(&args[1]);
	if (!ok) {
		Native_ErrPrint();
		return 0;
	}
	Con_Printf("coop: the super deal's scores, then %s in %.0f s\n", yes, CP_DEAL_SECS);
	return 1;
}

byte Coop_ShipEditOpen(void) {
	static uint8_t buf[CP_MENU_MSG];
	NetRepBuf w;
	if (!Coop_On()) {
		return 0;
	}
	int i = Cp_Player();
	if (i < 0 || Rep_PlayerConn(i) < 0) {
		return 0;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MENU);
	NetRep_PutU8(&w, COOP_MENU_SHIPEDIT);
	NetRep_PutStr(&w, cp.via);
	NetRep_PutU32(&w, (uint32_t)Players_Money(i));
	Cp_PutVars(&w, 1);
	if (w.bad) {
		Con_Printf("coop: the hangar of player %d does not fit a message\n", i);
		return 0;
	}
	cp.pl[i].menu = COOP_MENU_SHIPEDIT;
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: the ship editor opens on the client of player %d%s%s\n", i, cp.via[0] ? " with " : "", cp.via);
	return 1;
}

/* "Module.Func"(0, "") for player i: the function its answer chose */
static void Cp_MenuFunc(int i, const char *fn) {
	char mod[CP_NAME];
	const char *dot = strchr(fn, '.');
	if (!dot || dot - fn >= CP_NAME) {
		return;
	}
	Str_PrintfQuiet(mod, sizeof(mod), "%.*s", (int)(dot - fn), fn);
	Players_Bind(i);
	Str_Copy(cp.via, fn, sizeof(cp.via));
	if (!Mode_Import(mod) || !Native_CallMod(mod, dot + 1, NULL, "is", 0L, "")) {
		Native_ErrPrint();
	}
	cp.via[0] = 0;
	Players_BindTurn();
}

/* player i's money and hangar from its client into the story (a menu of the hangar closed there): its money, -1 for a
 * bad message; player i stays bound */
static int Cp_HangarBack(int i, NetRepReader *r) {
	int money = (int)NetRep_GetU32(r);
	if (r->bad) {
		return -1;
	}
	Players_Bind(i);
	Cp_GetVars(r, 0);
	g_pWorld->money = money;
	return money;
}

/* the function a module's variable holds (a menu's callback of the scripts) with the menu's id 0 and control "" when
 * args; then None in it with clear */
static void Cp_MenuCallback(const char *mod, const char *var, byte args, byte clear) {
	nativeVal_t cb, a[2];
	Native_SetNone(&cb);
	if (Mode_ModGet(mod, var, &cb) && cb.type != NV_NONE) {
		Native_SetInt(&a[0], 0);
		Native_SetStr(&a[1], "");
		if (clear) {
			nativeVal_t none;
			Native_SetNone(&none);
			Mode_SetModVar(mod, var, &none);
		}
		if (!Mode_CallArgs(&cb, NULL, args ? a : NULL, args ? 2 : 0)) {
			Native_ErrPrint();
		}
		Native_Free(&a[1]);
	}
	Native_ErrClear();
	Native_Free(&cb);
}

/* player i closed the ship editor: its hangar into the story, its money, the ship on the platform as the editor's end
 * leaves it, then the editor's callback of the scripts */
static void Cp_ShipEdited(int i, NetRepReader *r) {
	int money = Cp_HangarBack(i, r);
	if (money < 0) {
		return;
	}
	if (!Junkyard_PlatformShip()) {
		Native_ErrPrint();
	}
	const char *cur = SaveVar_Get(CP_HANGAR "iHangarShip");
	Con_Printf("coop: player %d closed the ship editor (hangar %s, money %d)\n", i, cur ? cur : "-", money);
	Cp_MenuCallback("ShipEdit", "EndShipEditCallBack", 1, 0);
	Players_BindTurn();
}

byte Coop_Warning(const char *text) {
	uint8_t buf[CP_MENU_TITLE + 16];
	NetRepBuf w;
	if (!Coop_On()) {
		return 0;
	}
	int i = Cp_Player(), n = (int)strlen(text);
	if (i < 0 || Rep_PlayerConn(i) < 0) {
		return 1;
	}
	n = n > CP_MENU_TITLE ? CP_MENU_TITLE : n;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MENU);
	NetRep_PutU8(&w, COOP_MENU_WARNING);
	NetRep_PutU16(&w, (uint32_t)n);
	CoordMsg_PutBytes(&w, text, n);
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: a text box for player %d: %.60s\n", i, text);
	return 1;
}

byte Coop_SputnikOpen(void) {
	static uint8_t buf[CP_MENU_MSG];
	NetRepBuf w;
	nativeVal_t spt;
	Native_SetNone(&spt);
	/* the member that used the party's Sputnik (its interface's events may run while another one is bound) */
	int user = Coop_On() && Mode_ModGet("Sputnik", "spt", &spt) && spt.type == NV_ENT ? Sputniks_CoopUser(Native_Ent(&spt)) : -1;
	Native_Free(&spt);
	Native_ErrClear();
	int i = user >= 0 ? user : Coop_On() ? Cp_Player() : -1;
	if (i < 0 || Rep_PlayerConn(i) < 0) {
		return 0;
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MENU);
	NetRep_PutU8(&w, COOP_MENU_SPUTNIK);
	NetRep_PutU32(&w, (uint32_t)Players_Money(i));
	Cp_PutVars(&w, 1);
	if (w.bad) {
		return 0;
	}
	cp.pl[i].menu = COOP_MENU_SPUTNIK;
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: Sputnik's hangar screen opens on the client of player %d\n", i);
	return 1;
}

/* player i closed Sputnik's screen: its hangar and money, then the callback of the scripts (once) */
static void Cp_SputnikDone(int i, NetRepReader *r) {
	int money = Cp_HangarBack(i, r);
	if (money < 0) {
		return;
	}
	const char *cur = SaveVar_Get(CP_HANGAR "iHangarShip");
	Con_Printf("coop: player %d closed Sputnik's screen (hangar %s, money %d)\n", i, cur ? cur : "-", money);
	Cp_MenuCallback("SputnikInterface", "EndSputnikCallBack", 0, 1);
	Players_BindTurn();
}

/* the machine of the member's conversor runs (1) or waits for its pick (0) */
static void Cp_ConvMachine(byte run) {
	cEntity *m = World_FindEntity(cp.convMachine);
	double speed = run ? 1.0 : 0.0;
	if (m) {
		Entity_VarF(m, "TimeSpeed", &speed, 1);
	}
}

void Coop_ConvEnter(void) {
	nativeVal_t c;
	int i = Coop_On() ? Players_Bound() : PLAYERS_NONE;
	if (i == PLAYERS_NONE) {
		return;
	}
	Native_SetNone(&c);
	if (Mode_ModGet("CharConversor", "_charName", &c) && c.type == NV_STR) {
		cp.conv = i;
		cp.convAsked = 0;
		Str_Copy(cp.convMachine, Native_Text(&c), sizeof(cp.convMachine));
		Con_Printf("coop: player %d enters the conversor %s\n", i, cp.convMachine);
	}
	Native_ErrClear();
	Native_Free(&c);
}

byte Coop_ConvMenu(void) {
	static uint8_t buf[CP_MENU_MSG];
	NetRepBuf w;
	int i = Coop_On() ? cp.conv : -1;
	if (i < 0) {
		return 0;
	}
	if (Rep_PlayerConn(i) < 0) {
		return 1;
	}
	const char *avl = SaveVar_Get("Conversor.AvailableChars");
	cEntity *e = Players_Get(i);
	cActCtrl *a = e && Entity_IsType(e, TYEN_WITH_LIFE) ? WLife(e)->actCtrl : NULL;
	int n = avl ? (int)strlen(avl) : 0;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_Y_MENU);
	NetRep_PutU8(&w, COOP_MENU_CONV);
	NetRep_PutStr(&w, a && a->cls ? a->cls->name : "Dtritus");
	NetRep_PutU16(&w, (uint32_t)n);
	CoordMsg_PutBytes(&w, avl ? avl : "", n);
	if (w.bad) {
		return 1;
	}
	/* the menu's bookkeeping of the single player (the mode and time to go back to), the machine waits */
	Players_Bind(i);
	if (!Mode_Import("Menu") || !PORT_CALL(Menu, InitMenuSys, NULL, "i", 0L)) {
		Native_ErrPrint();
	}
	cp.pl[i].menu = COOP_MENU_CONV;
	cp.convAsked = 1;
	Cp_ConvMachine(0);
	Cp_ToClient(i, buf, w.size);
	Con_Printf("coop: the conversor's interface opens on the client of player %d\n", i);
	return 1;
}

/* player i picked a class in the conversor (cls "": none), as the single player's Convert and Escape run */
static void Cp_ConvPicked(int i, const char *cls) {
	char k[CP_NAME + 8];
	nativeVal_t v;
	Players_Bind(i);
	cp.convAsked = 0;
	Cp_ConvMachine(1);
	Con_Printf("coop: player %d picks %s in the conversor\n", i, cls[0] ? cls : "nothing");
	Str_PrintfQuiet(k, sizeof(k), "Char_%s", cls);
	Native_SetStr(&v, k);
	byte ok = Mode_Import("CharConversor") && (cls[0] ? Mode_SetModVar("CharConversor", "KONTROL", &v) &&
		PORT_CALL(CharConversor, ConvertExecute, NULL, "") : PORT_CALL(CharConversor, ExitCharConversor, NULL, "is", 0L, ""));
	if (!ok) {
		Native_ErrPrint();
	}
	Native_Free(&v);
	Players_BindTurn();
}

void Coop_ConvEnd(void) {
	if (Coop_On() && cp.conv >= 0) {
		Players_Bind(cp.conv);
		cp.conv = -1;
	}
}

/* ---- tests ---- */

/* test: player i's ship along the race route, checkpoint by checkpoint: CP_RACE_GAP seconds after RaceMaker's
 * checkpoint came to a place (it moves on when taken) the ship is put there, as a pilot that knows the route (the
 * streets of a level have no straight line between two checkpoints); the race's own take of the checkpoint, its
 * count down and its score do the rest */
static void Cp_TestRace(int i) {
	cEntity *e = Players_Get(i), *c = World_FindEntity(CP_CHECKPOINT);
	if (!e || !c || NetPl_Kind(e) != NETREP_SHIP) {
		return;
	}
	vec3_t *at = &cp.pl[i].raceAt;
	double now = Instance_Time();
	if (at->x != c->pos.x || at->y != c->pos.y || at->z != c->pos.z) {
		*at = c->pos;
		cp.pl[i].raceSince = now;
		Con_Printf("coop: player %d: race checkpoint at %.0f %.0f %.0f\n", i, (double)at->x, (double)at->y, (double)at->z);
	}
	if (now - cp.pl[i].raceSince < CP_RACE_GAP) {
		return;
	}
	cp.pl[i].raceSince = now;
	e->pos = c->pos;
	Obj(e)->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
}

/* test: a target of the mission to fight (its radar mark red, or in the scripts' list of enemies) */
static int Cp_Foe(cEntity *e) {
	char *tg = NULL, *list = NULL;
	return (Entity_VarS(e, "TgType", &tg, 0) && tg && (!crt_stricmp(tg, "red") || !crt_stricmp(tg, "killer"))) ||
		(Entity_VarS(e, "EntityList", &list, 0) && list && !strcmp(list, "Enemies"));
}

/* test: a character of the mission while the mission's death hook of the characters waits (CharAct.onNPCDeath:
 * ApologizeMayor's staplers, CatchTrurl's functionaries, FicusBanker's Scrapus): the mission wants it dead, as its text
 * tells the single player */
static byte Cp_DeathWanted(cEntity *e) {
	nativeVal_t v;
	if (!Entity_IsType(e, TYEN_WALKCHAR)) {
		return 0;
	}
	byte hook = Mode_ModGet("CharAct", "onNPCDeath", &v) && v.type != NV_NONE;
	Native_Free(&v);
	Native_ErrClear();
	return hook;
}

/* test: an entity of the scripts with a life that is no character and no ship (IntoTheGDB's restore point: life 0) */
static byte Cp_Thing(cEntity *e) {
	return !Entity_IsType(e, TYEN_WALKCHAR) && !Entity_IsType(e, TYEN_CAR);
}

/* test: the living entities of src (see fightList) that are no player's (foes 1: only targets to fight, 0: only the
 * others, -1: all), into out; their count; of the mission's targets only the ones on the radar (a player does not see
 * the ones the scripts took off it: IntoTheGDB's scanned bishops); things of the scripts with a life 0 count */
static int Cp_FightTargets(const char *src, cEntity **out, int max, int foes) {
	char mod[CP_NAME];
	nativeVal_t v, it;
	int n = 0;
	if (!strcmp(src, CP_FIGHT_RED) || src[0] == '@') {
		/* the red ones, or the entities of an entity list of the scripts ("@OutCop": the police ships) but a green one of
		 * the list (DistractHumphrey's Humphrey among the traffic) */
		for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e && n < max; e = cEntity_Next(e)) {
			char *list = NULL, *tg = NULL;
			if (Entity_IsType(e, TYEN_WITH_LIFE) && WLife(e)->life > 0.0f && !e->deadNode.target && !Obj(e)->proxy &&
				Players_Of(e) < 0 && (src[0] == '@' ? Entity_VarS(e, "EntityList", &list, 0) && list && !strcmp(list, src + 1) &&
				!(Entity_VarS(e, "TgType", &tg, 0) && tg && !crt_stricmp(tg, "green")) : Cp_Foe(e) && foes != 0)) {
				out[n++] = e;
			}
		}
		return n;
	}
	Native_SetNone(&v);
	Str_Copy(mod, src[0] ? src : "MissionsFuncs.currentTarget", sizeof(mod));
	char *dot = strchr(mod, '.');
	if (!dot) {
		Native_SetText(&v, src, (int)strlen(src));
	} else {
		*dot = 0;
		if (!Mode_ModGet(mod, dot + 1, &v) || (!src[0] && (v.type != NV_STR || !Native_Text(&v)[0]))) {
			Native_Free(&v);
			Native_SetNone(&v);
			if (!src[0] && !Mode_ModGet("MissionsFuncs", "targetsList", &v)) {
				Native_SetNone(&v);
			}
		}
	}
	int k = v.type == NV_STR ? 1 : v.type == NV_NONE ? 0 : Mode_Loop(&v);
	for (int j = 0; j < k && n < max; j++) {
		Native_SetNone(&it);
		cEntity *e = v.type == NV_STR ? World_FindEntity(Native_Text(&v)) : Native_SeqItem(&v, j, &it) &&
			it.type == NV_STR ? World_FindEntity(Native_Text(&it)) : NULL;
		/* the mission's target whose radar mark and arrow the scripts took away (ActivateTarget 0: DistractHumphrey's phone
		 * cab after the call, IntoTheGDB's scanned bishops) is none for a player */
		if (e && Entity_IsType(e, TYEN_WITH_LIFE) && (WLife(e)->life > 0.0f || Cp_Thing(e)) && !e->deadNode.target &&
			Players_Of(e) < 0 && (foes < 0 || (Cp_Foe(e) || Cp_DeathWanted(e)) == foes) &&
			(src[0] || Obj(e)->radarShow || WLife(e)->arrow.canMission)) {
			out[n++] = e;
		}
		Native_Free(&it);
	}
	Native_ErrClear();
	Native_Free(&v);
	return n;
}

/* test: the weapon of ship e to fight a target d away with: the first it has with ammo of the homing missiles, the
 * heavy and the instant-hit weapons (near: the instant-hit ones first) but the useless ones (bits), else its own */
static int Cp_FightWeapon(cEntity *e, float d, uint32_t useless) {
	cWeaponManager *m = (cWeaponManager *)&Veh(e)->weapons;
	const int far[] = { g_SwarmIDWeap, g_InfernoIDWeap, g_ATPCIDWeap, g_DevastatorIDWeap, g_TeslaIDWeap, g_VulcanIDWeap,
		g_LaserIDWeap };
	const int near[] = { g_ATPCIDWeap, g_TeslaIDWeap, g_VulcanIDWeap, g_DevastatorIDWeap, g_SwarmIDWeap, g_InfernoIDWeap,
		g_LaserIDWeap };
	const int *pick = d < CP_FIGHT_HIT ? near : far;
	for (int k = 0; k < (int)(sizeof(far) / sizeof(far[0])); k++) {
		if (pick[k] >= 0 && pick[k] < g_pWorld->weapons.count && m->weaps[pick[k]].slots &&
			cWeaponManager_HasMinAmmo(m, (uint32_t)pick[k]) && !(pick[k] < 32 && (useless >> pick[k] & 1))) {
			return pick[k];
		}
	}
	return Veh(e)->controls.weapon;
}

/* test: the weakest weapon of ship e that has ammo (the laser, else the Vulcan), else its own */
static int Cp_GentleWeapon(cEntity *e) {
	cWeaponManager *m = (cWeaponManager *)&Veh(e)->weapons;
	const int weak[] = { g_LaserIDWeap, g_VulcanIDWeap };
	for (int k = 0; k < 2; k++) {
		if (weak[k] >= 0 && weak[k] < g_pWorld->weapons.count && m->weaps[weak[k]].slots &&
			cWeaponManager_HasMinAmmo(m, (uint32_t)weak[k])) {
			return weak[k];
		}
	}
	return Veh(e)->controls.weapon;
}

/* test: a weapon that fired at t for CP_FIGHT_USELESS s (near enough) while t lost no life is useless against it, as a
 * player sees (the mercury freight that only the ATPC hurts); a life that changes at all (a shield that regenerates)
 * starts the watch again */
static void Cp_FightUseless(int i, cEntity *t, int w, byte firing, double now) {
	float life = WLife(t)->life;
	if (firing && w == cp.pl[i].lifeWeap && life < cp.pl[i].lifeMark - 0.5f && w >= 0 && w < 32) {
		cp.pl[i].useful |= 1u << w;
	}
	if (!firing || w != cp.pl[i].lifeWeap || fabsf(life - cp.pl[i].lifeMark) >= 0.5f) {
		cp.pl[i].lifeWeap = firing ? w : -1;
		cp.pl[i].lifeMark = life;
		cp.pl[i].lifeSince = now;
		return;
	}
	if (now - cp.pl[i].lifeSince < CP_FIGHT_USELESS || w < 0 || w >= 32) {
		return;
	}
	cp.pl[i].useless |= 1u << w;
	cp.pl[i].lifeWeap = -1;
	Con_Printf("coop: player %d: weapon %d does not hurt %s\n", i, w, t->name);
}

/* test: the weapons that hurt the target are all out of ammo: player i's ship is put at the nearest ammo box of the
 * level it did not take lately (its pickup radius takes it, as a player flies through it); 1 while it goes for ammo */
static byte Cp_FightAmmo(int i, cEntity *e, double now) {
	cWeaponManager *m = (cWeaponManager *)&Veh(e)->weapons;
	uint32_t want = cp.pl[i].useful;
	for (int w = 0; want && w < 32 && w < g_pWorld->weapons.count; w++) {
		if ((want >> w & 1) && m->weaps[w].slots && cWeaponManager_HasMinAmmo(m, (uint32_t)w)) {
			return 0;
		}
	}
	if (!want) {
		return 0;
	}
	if (now - cp.pl[i].ammoAt < CP_AMMO_GAP) {
		return 1;
	}
	cEntity *box = NULL;
	float best = 0.0f;
	for (cEntity *b = cWorld_GetFirstEntity(g_pWorld); b; b = cEntity_Next(b)) {
		char *res = NULL;
		int k = 0;
		if (!b->active || b->deadNode.target || !Entity_IsType(b, "item") || !Entity_VarS(b, "NetRes", &res, 0) || !res ||
			strncmp(res, "Box:", 4) || !strcmp(res + 4, "Life")) {
			continue;
		}
		while (k < CP_AMMO_TRIED && strcmp(cp.pl[i].ammoTried[k], b->name)) {
			k++;
		}
		float dx = b->pos.x - e->pos.x, dy = b->pos.y - e->pos.y, dz = b->pos.z - e->pos.z, d = dx * dx + dy * dy + dz * dz;
		if (k == CP_AMMO_TRIED && (!box || d < best)) {
			box = b;
			best = d;
		}
	}
	if (!box) {
		/* every one taken lately: the oldest again next time */
		cp.pl[i].ammoTried[cp.pl[i].ammoNext][0] = 0;
		cp.pl[i].ammoNext = (cp.pl[i].ammoNext + 1) % CP_AMMO_TRIED;
		return 0;
	}
	Str_Copy(cp.pl[i].ammoTried[cp.pl[i].ammoNext], box->name, CP_NAME);
	cp.pl[i].ammoNext = (cp.pl[i].ammoNext + 1) % CP_AMMO_TRIED;
	cp.pl[i].ammoAt = now;
	e->pos = (vec3_t){ box->pos.x, box->pos.y + 200.0f, box->pos.z };
	Obj(e)->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
	Rep_Placed(i);
	Con_Printf("coop: player %d: no ammo for the weapons that hurt its target: ammo box %s\n", i, box->name);
	return 1;
}

static void Cp_Approach(cEntity *e, cEntity *t, float dist, float *dx, float *dz);

/* test: player i on foot at character t (d away): next to it on a side with a free line to it, facing it, the action
 * key when it is near ahead (D-Tritus' punch, the attacks of the other characters) */
static void Cp_TestMelee(int i, NetRepInput *in, cEntity *t, float d, double now) {
	cEntity *e = Players_Get(i);
	if (d > CP_MELEE_FAR) {
		if (now - cp.pl[i].fightSince >= 1.0) {
			float dx, dz;
			Cp_Approach(e, t, CP_MELEE, &dx, &dz);
			Travel_TestGoto(i, t->name, dx, dz);
			cp.pl[i].fightSince = now;
		}
		return;
	}
	float rot[2], dyaw = NetPl_Aim(e, &t->pos, rot, 1);
	in->rot[0] = NetRep_QRot(rot[0]);
	in->move[0] = NetRep_QMove(d > CP_MELEE && fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
	if (d < CP_MELEE * 2.0f && fabsf(dyaw) < 0.3f && fmod(now, 0.5) < 1.0 / INSTANCE_TICK_HZ) {
		in->buttons |= NETREP_IN_ACT;
	}
}

/* test: player i fights the nearest of its targets through its input, as a player at the keys: turns to it and fires
 * when it is ahead and near (the weapon's own shots and damage, the target's OnDeath with the member as the winner);
 * a target further than CP_FIGHT_FAR: the member is put CP_FIGHT_NEAR from it (once a second at most, as a pilot that
 * knows the way through the streets) */
static void Cp_Fight(int i, NetRepInput *in, double back) {
	cEntity *e = Players_Get(i), *all[CP_FIGHT_MAX], *t = NULL;
	int kind = NetPl_Kind(e), n = kind ? Cp_FightTargets(cp.pl[i].fightList, all, CP_FIGHT_MAX, -1) : 0;
	float best = 0.0f;
	/* the one it fights until it dies, else the nearest */
	for (int k = 0; k < n; k++) {
		float dx = all[k]->pos.x - e->pos.x, dy = all[k]->pos.y - e->pos.y, dz = all[k]->pos.z - e->pos.z;
		float d = dx * dx + dy * dy + dz * dz;
		if (!strcmp(all[k]->name, cp.pl[i].fightAt)) {
			t = all[k];
			best = d;
			break;
		}
		if (!t || d < best) {
			t = all[k];
			best = d;
		}
	}
	if (!t) {
		if (kind && cp.pl[i].fightAt[0]) {
			Con_Printf("coop: player %d: no target left to fight\n", i);
			cp.pl[i].fightAt[0] = 0;
		}
		return;
	}
	/* a ship the mission wants hit but alive (AttackTraitorius: scared under 30% of its life for a minute) is spared
	 * there, as a player watches its life bar */
	byte gentle = 0;
	if (cp.pl[i].spare > 0 && Entity_IsType(t, TYEN_CAR)) {
		float pct = WLife(t)->life * 100.0f / (float)(Veh(t)->maxLife > 0 ? Veh(t)->maxLife : 1);
		cp.pl[i].follow |= pct <= (float)cp.pl[i].spare;
		/* near it the weakest weapon, as a player does not to kill it */
		gentle = pct <= (float)cp.pl[i].spare * 2.0f;
	}
	float d = sqrtf(best), near = cp.pl[i].follow ? CP_FOLLOW_NEAR : CP_FIGHT_NEAR;
	double now = Instance_Time();
	if (kind == NETREP_CHAR && Entity_IsType(t, TYEN_CAR)) {
		/* a ship to fight a ship: the switch (into the nearest parked ship, else a called one) */
		in->buttons |= fmod(now, 2.0) < 1.0 / INSTANCE_TICK_HZ ? NETREP_IN_SWITCH : 0;
		return;
	}
	if (strcmp(cp.pl[i].fightAt, t->name)) {
		Str_Copy(cp.pl[i].fightAt, t->name, sizeof(cp.pl[i].fightAt));
		Con_Printf("coop: player %d fights %s (%.0f away)\n", i, t->name, (double)d);
		cp.pl[i].useless = cp.pl[i].useful = 0;
		cp.pl[i].lifeWeap = -1;
	}
	if (kind == NETREP_CHAR) {
		Cp_TestMelee(i, in, t, d, now);
		return;
	}
	if (d > (cp.pl[i].follow ? CP_FOLLOW_FAR : CP_FIGHT_FAR) && now - cp.pl[i].fightSince >= (cp.pl[i].follow ? 0.5 : 1.0)) {
		float k = near / d;
		e->pos = (vec3_t){ t->pos.x + (e->pos.x - t->pos.x) * k, t->pos.y + (kind == NETREP_SHIP ? 300.0f : 100.0f),
			t->pos.z + (e->pos.z - t->pos.z) * k };
		/* a ship it stays with: at its speed, as a pilot behind it (a scan needs it near for seconds) */
		Obj(e)->vel = cp.pl[i].follow ? Obj(t)->vel : (vec3_t){ 0.0f, 0.0f, 0.0f };
		/* facing it (yaw 0 looks to -z) */
		Obj(e)->ang.y = atan2f(-(t->pos.x - e->pos.x), -(t->pos.z - e->pos.z));
		if (kind == NETREP_CHAR) {
			WChar(e)->view.y = Obj(e)->ang.y;
		}
		Rep_Placed(i);
		cp.pl[i].fightSince = now;
		d = near;
	}
	if (kind == NETREP_SHIP && Cp_FightAmmo(i, e, now)) {
		return;
	}
	/* where its client shows it (the server's shot looks there) */
	back = back < 0.0 ? 0.0 : back > 0.5 ? 0.5 : back;
	/* and ahead of it by the flight of a shot (CP_FIGHT_SHOT units a second), as a player leads a moving target */
	float lead = kind == NETREP_SHIP ? d / CP_FIGHT_SHOT - (float)back : -(float)back;
	vec3_t *v = &Obj(t)->vel, at = { t->pos.x + v->x * lead, t->pos.y + v->y * lead, t->pos.z + v->z * lead };
	float rot[2], dy = at.y - e->pos.y, cone = atan2f(CP_FIGHT_SIZE, d);
	float dyaw = NetPl_Aim(e, &at, rot, 1);
	if (kind == NETREP_SHIP) {
		/* a pilot's quick turn: the full turn until it is this near (NetPl_Aim's own is the bots' slow one) */
		float hz = sqrtf((at.x - e->pos.x) * (at.x - e->pos.x) + (at.z - e->pos.z) * (at.z - e->pos.z));
		float pitch = atan2f(dy, hz) - Obj(e)->ang.x;
		rot[0] = dyaw > CP_FIGHT_TURN ? 1.0f : dyaw < -CP_FIGHT_TURN ? -1.0f : dyaw / CP_FIGHT_TURN;
		rot[1] = pitch > CP_FIGHT_TURN ? 1.0f : pitch < -CP_FIGHT_TURN ? -1.0f : pitch / CP_FIGHT_TURN;
	}
	byte ahead = fabsf(dyaw) < cone && (kind != NETREP_SHIP || fabsf(rot[1]) < cone * 2.0f) && d < CP_FIGHT_RANGE;
	in->rot[0] = NetRep_QRot(rot[0]);
	if (kind == NETREP_SHIP) {
		in->rot[1] = NetRep_QRot(rot[1]);
		in->weapon = (uint8_t)(gentle ? Cp_GentleWeapon(e) : Cp_FightWeapon(e, d, cp.pl[i].useless));
		Cp_FightUseless(i, t, in->weapon, d < CP_FIGHT_RANGE && !cp.pl[i].follow, now);
		/* closer while far, else its speed (behind it, turning with it) */
		float tv = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z), want = d > near * 1.5f ? tv + 1500.0f : tv;
		in->move[0] = NetRep_QMove(Veh(e)->speed > want + 200.0f ? -1.0f : Veh(e)->speed < want ? 1.0f : 0.0f);
		in->move[2] = NetRep_QMove(dy > 500.0f ? 0.8f : dy < -500.0f ? -0.8f : dy / 625.0f);
	} else {
		in->move[0] = NetRep_QMove(d > near * 0.5f && fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
	}
	/* the weapons that fire at the release (the homing missiles lock on while held, the Devastator charges) in
	 * bursts, the others held while the target is ahead */
	int w = kind == NETREP_SHIP ? (int)in->weapon : -1;
	if (cp.pl[i].follow) {
		/* it stays with the target and shoots nothing */
	} else if (w >= 0 && (w == g_SwarmIDWeap || w == g_InfernoIDWeap)) {
		/* a homing missile: held while the target is near ahead until its lock is on it, released then */
		const char *lk = cTargetLock_GetEntity((cTargetLock *)&Veh(e)->weapons);
		if (!lk || strcmp(lk, t->name)) {
			cp.pl[i].lockAt = now;
		}
		if (now - cp.pl[i].lockAt >= CP_FIGHT_LOCK) {
			cp.pl[i].lockAt = now + 0.2;
		} else if (fabsf(dyaw) < CP_FIGHT_LOCKCONE && d < CP_FIGHT_RANGE && now >= cp.pl[i].lockAt - 0.3) {
			in->buttons |= NETREP_IN_ACT;
		}
	} else if (ahead && (kind != NETREP_SHIP || w != g_DevastatorIDWeap || fmod(now, 3.0) < 2.0)) {
		/* the Devastator charges while held and fires at the release, the others are held */
		in->buttons |= NETREP_IN_ACT;
	}
	if (now - cp.pl[i].fightTold >= 5.0) {
		cp.pl[i].fightTold = now;
		Con_Printf("coop: player %d at %s: %.0f away, yaw %.2f, its life %.0f, weapon %d, ammo %.0f\n", i, t->name,
			(double)d, (double)dyaw, (double)WLife(t)->life, kind == NETREP_SHIP ? Veh(e)->controls.weapon : -1,
			kind == NETREP_SHIP ? (double)cWeaponManager_GetWeapAmmo(&Veh(e)->weapons, (uint32_t)Veh(e)->controls.weapon) : 0.0);
	}
}

/* test: player i holds its fire: the variable of the scripts it watches is at its most (DistractHumphrey: ten ships
 * shot down before the time ends the round too soon, a player watches the count on the HUD) */
static byte Cp_UpTo(int i) {
	char mod[CP_NAME];
	nativeVal_t v;
	Str_Copy(mod, cp.pl[i].upTo, sizeof(mod));
	char *dot = strchr(mod, '.');
	if (!dot) {
		return 0;
	}
	*dot = 0;
	Native_SetNone(&v);
	byte at = Mode_ModGet(mod, dot + 1, &v) && v.type == NV_INT && v.i >= cp.pl[i].upToMax;
	Native_ErrClear();
	Native_Free(&v);
	return at;
}

static void Cp_TestFight(int i, NetRepInput *in, double back) {
	byte follow = cp.pl[i].follow;
	cp.pl[i].follow |= Cp_UpTo(i);
	Cp_Fight(i, in, back);
	cp.pl[i].follow = follow;
}

/* test: where player e stands to use t (its arrow selects only what it sees: a line of sight from it): dist from t on
 * the side e comes from, else the next of eight sides with floor under it and nothing between it and t */
static void Cp_Approach(cEntity *e, cEntity *t, float dist, float *dx, float *dz) {
	float a = atan2f(e->pos.x - t->pos.x, e->pos.z - t->pos.z);
	for (int k = 0; k < 8; k++) {
		float b = a + (float)((k + 1) / 2) * (k & 1 ? 1.0f : -1.0f) * (M_PI_F / 4.0f);
		vec3_t at = t->pos, p = { t->pos.x + sinf(b) * dist, t->pos.y + 100.0f, t->pos.z + cosf(b) * dist };
		vec3_t down = { p.x, p.y - 400.0f, p.z };
		at.y += 20.0f;
		if (!Engine_TestLine(&p, &at, NULL, NULL) && Engine_TestLine(&p, &down, NULL, NULL)) {
			*dx = p.x - t->pos.x;
			*dz = p.z - t->pos.z;
			return;
		}
	}
	*dx = 0.0f;
	*dz = dist;
}

/* test: a flying ship to shoot down (not a parked one, not a green one to stay with) */
static byte Cp_ShipFoe(cEntity *v) {
	char *tg = NULL;
	return Entity_IsType(v, TYEN_CAR) && strncmp(v->name, "Parked", 6) && Entity_VarS(v, "TgType", &tg, 0) &&
		!(tg && !crt_stricmp(tg, "green"));
}

/* test: the nearest entity to e the scripts marked with the mission arrow (MissionsFuncs.ActivateTarget: a clue to take,
 * a blueprint; things with life 0 too: GetDataPart2's desks), not a door of the level's exits; a ship to shoot down before the others (Ambush's probes while the
 * phone cab of the mission's start keeps its arrow); NULL */
static cEntity *Cp_ArrowTarget(const cEntity *e) {
	cEntity *t = NULL;
	float best = 0.0f;
	byte ship = 0;
	for (cEntity *v = cWorld_GetFirstEntity(g_pWorld); v; v = cEntity_Next(v)) {
		if (!Entity_IsType(v, TYEN_WITH_LIFE) || !WLife(v)->arrow.canMission || (WLife(v)->life <= 0.0f && !Cp_Thing(v)) ||
			v->deadNode.target ||
			Obj(v)->proxy || Players_Of(v) >= 0 || !strncmp(v->name, "DM_Door", 7)) {
			continue;
		}
		float dx = v->pos.x - e->pos.x, dz = v->pos.z - e->pos.z, d = dx * dx + dz * dz;
		byte s = Cp_ShipFoe(v);
		if (!t || s > ship || (s == ship && d < best)) {
			t = v;
			best = d;
			ship = s;
		}
	}
	return t;
}

/* test: the nearest item to e the scripts show on the radar with a function of the mission for its taker
 * (ProtectMayorTransport's meeting point: "a yellow arrow on your radar"; not the level's parkings and links), NULL */
static cEntity *Cp_RadarItem(const cEntity *e) {
	const char *mission = SaveVar_Get("Mission.Library");
	cEntity *t = NULL;
	float best = 0.0f;
	for (cEntity *v = cWorld_GetFirstEntity(g_pWorld); v; v = cEntity_Next(v)) {
		const cItemEntity *it = (const cItemEntity *)v;
		if (!v->active || v->deadNode.target || !Entity_IsType(v, "item") || !Obj(v)->radarShow || Obj(v)->proxy ||
			it->respawn > 0.0f || !mission || strcmp(cPy_Function_Module(&it->onTake), mission)) {
			continue;
		}
		float dx = v->pos.x - e->pos.x, dz = v->pos.z - e->pos.z, d = dx * dx + dz * dz;
		if (!t || d < best) {
			t = v;
			best = d;
		}
	}
	return t;
}

/* test: no floor within CP_AIR under entity v (DistractHumphrey's meeting point: a ship takes it, the scripts' camera
 * travel moves a ship) */
static byte Cp_InAir(const cEntity *v) {
	vec3_t p = v->pos, down = { p.x, p.y - CP_AIR, p.z };
	return !Engine_TestLine(&p, &down, NULL, NULL);
}

/* test: the mission's green ship (to follow or escort: StupidMerc's mercenary), NULL */
static cEntity *Cp_Escorted(void) {
	cEntity *all[CP_FIGHT_MAX];
	int n = Cp_FightTargets("", all, CP_FIGHT_MAX, 0);
	for (int k = 0; k < n; k++) {
		char *tg = NULL;
		if (Entity_IsType(all[k], TYEN_CAR) && strncmp(all[k]->name, "Parked", 6) && Entity_VarS(all[k], "TgType", &tg, 0) &&
			tg && !crt_stricmp(tg, "green")) {
			return all[k];
		}
	}
	return NULL;
}

/* test: a red one within CP_ESCORT of e */
static byte Cp_RedNear(const cEntity *e) {
	cEntity *all[CP_FIGHT_MAX];
	int n = Cp_FightTargets(CP_FIGHT_RED, all, CP_FIGHT_MAX, 1);
	for (int k = 0; k < n; k++) {
		float dx = all[k]->pos.x - e->pos.x, dy = all[k]->pos.y - e->pos.y, dz = all[k]->pos.z - e->pos.z;
		if (dx * dx + dy * dy + dz * dz < CP_ESCORT * CP_ESCORT) {
			return 1;
		}
	}
	return 0;
}

/* test: the level has the walkers' AI graph (the indoor levels) */
static byte Cp_WalkGraph(void) {
	return g_AStar2D && g_NodesWorld2D && NodesWorld_IsCreated(NW2D_SUP(g_NodesWorld2D));
}

/* test: walking character v follows e within the scripts' leash: its logic chases e (LLogicTarget with LLogicState
 * GENCHAR_CHASE: BeatPickus' thugs; not the police whose logic attacks it) or the target of its AI's chase without
 * action is e (SAI.SetStateChar 3: GoWithHuman's and TalkToBoss' human; not one that attacks it) */
static byte Cp_Follows(cEntity *v, const cEntity *e) {
	char *tgt = NULL;
	int st = -1;
	float vx = v->pos.x - e->pos.x, vy = v->pos.y - e->pos.y, vz = v->pos.z - e->pos.z;
	if (!v->active || !Entity_IsType(v, TYEN_WALKCHAR) || Players_Of(v) >= 0 || Obj(v)->proxy ||
		vx * vx + vy * vy + vz * vz >= CP_WALK_LEASH * CP_WALK_LEASH) {
		return 0;
	}
	cAIChar *ai = Entity_GetAI(v);
	return (Entity_VarS(v, "LLogicTarget", &tgt, 0) && tgt && !strcmp(tgt, e->name) && Entity_VarI(v, "LLogicState", &st, 0) &&
		st == GENCHAR_CHASE) || (ai && ai->state == AIC_CHASE_WITHOUT_ACTION && ai->base.target.target == e);
}

/* test: the point player e walks to next on its way to `to`: the walking NPCs' A* on the level's AI graph (CAStar2D,
 * the graph of the indoor levels), the furthest point of the path its body reaches on a straight line; 0 without a
 * graph or a path */
static byte Cp_PathPoint(cEntity *e, const vec3_t *to, vec3_t *next) {
	static cNWVecList path;
	if (!Cp_WalkGraph()) {
		return 0;
	}
	vec3_t from = e->pos, dest = *to;
	float r = Obj_GetRadius(e), h = WChar(e)->floorDist, fr = WithLife_GetActCtrl(e)->cls->sRadius;
	byte noPath = 0, reach = 0, got = 0;
	int res = 0;
	if (CAStar2D_FindPath(g_AStar2D, &path, &dest, &from, r, 0.0f, h, fr, 0, &noPath, &reach, &res) && path.head) {
		/* from its end where the walker is */
		cNWVecNode *n = path.head, *last = path.head->prev;
		float hx = n->v.x - from.x, hz = n->v.z - from.z, lx = last->v.x - from.x, lz = last->v.z - from.z;
		byte fwd = hx * hx + hz * hz <= lx * lx + lz * lz;
		n = fwd ? n : last;
		for (int k = 0; k < path.count && n; k++, n = fwd ? n->next : n->prev) {
			float dx = n->v.x - from.x, dz = n->v.z - from.z;
			if (got && !CNodesWorld2D_TestPath(g_NodesWorld2D, &from, &n->v, r, r, h, fr, NULL)) {
				break;
			}
			if (got || dx * dx + dz * dz > CP_WALK_NEAR * CP_WALK_NEAR) {
				*next = n->v;
				got = 1;
			}
		}
	}
	NWVecList_Free(&path);
	return got;
}

/* test: the point ship e flies to next on its way to `to`: `to` on a free line, else the AI ships' A* on the level's
 * graph of visibility (CAStar, the outdoor levels), the furthest point of the path it reaches on a straight line; 0
 * without a graph or a path */
static byte Cp_FlyPoint(cEntity *e, const vec3_t *to, vec3_t *next) {
	static cNWVecList path;
	if (!g_AStar || !g_NodesWorld || !NodesWorld_IsCreated(g_NodesWorld)) {
		return 0;
	}
	vec3_t from = e->pos, dest = *to;
	float r = Obj_GetRadius(e);
	byte startOk = 0, goalOk = 0, got = 0;
	int state = 0;
	/* a free line: straight there */
	if (CNodesWorld_TestPath(g_NodesWorld, &from, &dest, r, r, NULL, 0)) {
		*next = dest;
		return 1;
	}
	if (CAStar_FindPath(g_AStar, &path, &dest, &from, r, 0.0f, 0, &startOk, &goalOk, &state) && path.head) {
		/* from its end where the ship is */
		cNWVecNode *n = path.head, *last = path.head->prev;
		float hx = n->v.x - from.x, hz = n->v.z - from.z, lx = last->v.x - from.x, lz = last->v.z - from.z;
		byte fwd = hx * hx + hz * hz <= lx * lx + lz * lz;
		n = fwd ? n : last;
		for (int k = 0; k < path.count && n; k++, n = fwd ? n->next : n->prev) {
			float dx = n->v.x - from.x, dy = n->v.y - from.y, dz = n->v.z - from.z;
			if (got && !CNodesWorld_TestPath(g_NodesWorld, &from, &n->v, r, r, NULL, 0)) {
				break;
			}
			if (got || dx * dx + dy * dy + dz * dz > CP_FLY_NEAR * CP_FLY_NEAR) {
				*next = n->v;
				got = 1;
			}
		}
	}
	NWVecList_Free(&path);
	return got;
}

/* test: player i's ship (e) flies to g through its input along the level's graph of visibility, as a pilot steers
 * round the buildings; when it comes no nearer for CP_FLY_STUCK s it is placed CP_FIGHT_NEAR from g (the placement of
 * before, kept where the graph gives no way); 1 while it flies, 0 within `near` of g (on the ground) or stuck that
 * near */
static byte Cp_FlyTo(int i, NetRepInput *in, cEntity *e, cEntity *g, float near, double now) {
	float dx = g->pos.x - e->pos.x, dy = g->pos.y - e->pos.y, dz = g->pos.z - e->pos.z;
	float d = sqrtf(dx * dx + dy * dy + dz * dz);
	/* over it (a goal on a platform the ship cannot come down to: out of the ship there, the fall takes it down) */
	if (dx * dx + dz * dz <= near * near) {
		cp.pl[i].flyGoal[0] = 0;
		return 0;
	}
	/* a new goal, or a flight a fight broke off: its watch starts again */
	if (strcmp(cp.pl[i].flyGoal, g->name) || now - cp.pl[i].flyAt > CP_FLY_GAP * 4.0) {
		Str_Copy(cp.pl[i].flyGoal, g->name, sizeof(cp.pl[i].flyGoal));
		cp.pl[i].flyBest = d;
		cp.pl[i].flyMoved = now;
		cp.pl[i].flyAt = 0.0;
		Con_Printf("coop: player %d (auto) flies to %s (%.0f away)\n", i, g->name, (double)d);
	}
	if (d < cp.pl[i].flyBest - CP_FLY_ON) {
		cp.pl[i].flyBest = d;
		cp.pl[i].flyMoved = now;
	} else if (now - cp.pl[i].flyMoved >= CP_FLY_STUCK) {
		/* as near as the placement puts it: the way ends here (the goal on a platform it cannot fly onto) */
		if (dx * dx + dz * dz <= CP_FIGHT_NEAR * CP_FIGHT_NEAR * 2.25f) {
			return 0;
		}
		float k = CP_FIGHT_NEAR / d;
		if (k < 1.0f) {
			e->pos = (vec3_t){ g->pos.x - dx * k, g->pos.y + 300.0f, g->pos.z - dz * k };
			Obj(e)->vel = (vec3_t){ 0.0f, 0.0f, 0.0f };
			Obj(e)->ang.y = atan2f(-dx, -dz);
			Rep_Placed(i);
			Con_Printf("coop: player %d (auto) is stuck on its flight to %s: placed %.0f from it\n", i, g->name,
				(double)CP_FIGHT_NEAR);
		}
		cp.pl[i].flyBest = d < CP_FIGHT_NEAR ? d : CP_FIGHT_NEAR;
		cp.pl[i].flyMoved = now;
		cp.pl[i].flyAt = now - CP_FLY_GAP;
		return 1;
	}
	if (now - cp.pl[i].flyAt >= CP_FLY_GAP) {
		cp.pl[i].flyAt = now;
		vec3_t up = { g->pos.x, g->pos.y + 300.0f, g->pos.z };
		if (!Cp_FlyPoint(e, &up, &cp.pl[i].flyTo)) {
			cp.pl[i].flyTo = up;
		}
	}
	/* a pilot's quick turn to its point, full speed while it faces it, slow in the turn */
	vec3_t *p = &cp.pl[i].flyTo;
	float rot[2], dyaw = NetPl_Aim(e, p, rot, 1);
	float hz = sqrtf((p->x - e->pos.x) * (p->x - e->pos.x) + (p->z - e->pos.z) * (p->z - e->pos.z));
	float py = p->y - e->pos.y, pitch = atan2f(py, hz) - Obj(e)->ang.x;
	rot[0] = dyaw > CP_FIGHT_TURN ? 1.0f : dyaw < -CP_FIGHT_TURN ? -1.0f : dyaw / CP_FIGHT_TURN;
	rot[1] = pitch > CP_FIGHT_TURN ? 1.0f : pitch < -CP_FIGHT_TURN ? -1.0f : pitch / CP_FIGHT_TURN;
	in->rot[0] = NetRep_QRot(rot[0]);
	in->rot[1] = NetRep_QRot(rot[1]);
	in->move[0] = NetRep_QMove(fabsf(dyaw) < 0.5f ? 1.0f : fabsf(dyaw) < 1.2f ? 0.3f : 0.0f);
	in->move[2] = NetRep_QMove(py > 500.0f ? 0.8f : py < -500.0f ? -0.8f : py / 625.0f);
	return 1;
}

/* test: player i on foot (e) further than CP_WALK_FAR from t walks there through its input along the level's AI graph
 * while a follower is near it, waits for one that fell behind and goes back to it after CP_WALK_ALONE s, as a player
 * walks with characters that follow him (BeatPickus' thugs, or the one of the test line: GoWithHuman's human;
 * followers only with the test line walk); a walk that does not come on is placed at its path's point, one within
 * CP_WALK_END that comes no nearer ends; 1 while it walks */
static byte Cp_WalkStep(int i, NetRepInput *in, cEntity *e, cEntity *t, double now) {
	float dx = t->pos.x - e->pos.x, dz = t->pos.z - e->pos.z, d = sqrtf(dx * dx + dz * dz);
	if (d <= CP_WALK_FAR) {
		cp.pl[i].walkWait = 0.0;
		cp.pl[i].walkNearAt = 0.0;
		return 0;
	}
	if (now - cp.pl[i].autoSince >= CP_WALK_GAP) {
		/* its followers (test line walk): the characters that follow it (or the one named by the test line) */
		cEntity *back = NULL;
		float best = 0.0f;
		for (cEntity *v = cp.pl[i].walk ? cWorld_GetFirstEntity(g_pWorld) : NULL; v; v = cEntity_Next(v)) {
			float vx = v->pos.x - e->pos.x, vy = v->pos.y - e->pos.y, vz = v->pos.z - e->pos.z, vd = vx * vx + vy * vy + vz * vz;
			if (v != e && (!cp.pl[i].walkWho[0] || !strcmp(v->name, cp.pl[i].walkWho)) && Cp_Follows(v, e) &&
				(!back || vd < best)) {
				back = v;
				best = vd;
			}
		}
		if (back) {
			Str_Copy(cp.pl[i].follower, back->name, sizeof(cp.pl[i].follower));
			cp.pl[i].walkBack = 0.0;
		} else if (cp.pl[i].walk && cp.pl[i].follower[0] && (back = World_FindEntity(cp.pl[i].follower)) != NULL) {
			/* one that stopped follows another now (BeatPickus' thugs behind their leader): back to it; next to it for
			 * CP_WALK_ALONE s without it following again it is no follower */
			float vx = back->pos.x - e->pos.x, vz = back->pos.z - e->pos.z;
			best = CP_WALK_WAIT * CP_WALK_WAIT;
			if (vx * vx + vz * vz >= best) {
				cp.pl[i].walkBack = 0.0;
			} else if (cp.pl[i].walkBack <= 0.0) {
				cp.pl[i].walkBack = now;
			} else if (now - cp.pl[i].walkBack >= CP_WALK_ALONE) {
				cp.pl[i].follower[0] = 0;
				back = NULL;
			}
		}
		byte near = !back || best < CP_WALK_WAIT * CP_WALK_WAIT;
		cp.pl[i].autoSince = now;
		cp.pl[i].autoUse = 0.0;
		cp.pl[i].autoAt[0] = 0;
		if (near) {
			cp.pl[i].walkWait = 0.0;
		} else if (cp.pl[i].walkWait <= 0.0) {
			cp.pl[i].walkWait = now;
		}
		/* none kept up for a while: back to the nearest one (they stopped where it left them) */
		cEntity *to = !near && now - cp.pl[i].walkWait >= CP_WALK_ALONE ? back : t;
		if (!near || to != t) {
			cp.pl[i].walkNearAt = 0.0;
		} else if (cp.pl[i].walkNearAt <= 0.0 || d < cp.pl[i].walkNear - CP_WALK_STEP * 0.25f) {
			cp.pl[i].walkNear = d;
			cp.pl[i].walkNearAt = now;
		} else if (d <= CP_WALK_END && now - cp.pl[i].walkNearAt >= CP_WALK_STUCK) {
			/* its way ends short of the target (BeatPickus' blueprints on a wall, the graph's last node 500 away): the
			 * goto's last step, as at the end of a walk */
			cp.pl[i].walkNearAt = 0.0;
			return 0;
		}
		if (!Cp_PathPoint(e, &to->pos, &cp.pl[i].walkTo)) {
			cp.pl[i].walkTo = to->pos;
		}
		float mx = e->pos.x - cp.pl[i].walkFrom.x, mz = e->pos.z - cp.pl[i].walkFrom.z;
		if (!near && to == t) {
			cp.pl[i].walkMoved = now;
		} else if (mx * mx + mz * mz > CP_WALK_STEP * CP_WALK_STEP * 0.0625f) {
			cp.pl[i].walkFrom = e->pos;
			cp.pl[i].walkMoved = now;
		} else if (now - cp.pl[i].walkMoved >= CP_WALK_STUCK) {
			/* stuck at a corner or a door the graph's line misses: a step onto its path's point */
			vec3_t p = { cp.pl[i].walkTo.x, cp.pl[i].walkTo.y + 150.0f, cp.pl[i].walkTo.z };
			vec3_t down = { p.x, p.y - 2000.0f, p.z }, floor;
			if (Engine_TestLine(&p, &down, &floor, NULL)) {
				p.y = floor.y + 100.0f;
			}
			e->pos = cp.pl[i].walkFrom = p;
			cp.pl[i].walkMoved = now;
			Rep_Placed(i);
			Con_Printf("coop: player %d (auto) is stuck on its walk: placed at %.0f %.0f %.0f\n", i, (double)p.x,
				(double)p.y, (double)p.z);
		}
		if (now - cp.pl[i].walkTold >= 5.0) {
			cp.pl[i].walkTold = now;
			Con_Printf("coop: player %d (auto) walks to %s (%.0f away) by %.0f %.0f, its follower %s %.0f away%s\n", i,
				to->name, (double)d, (double)cp.pl[i].walkTo.x, (double)cp.pl[i].walkTo.z, back ? back->name : "-",
				(double)sqrtf(best), !near && to == t ? ", waits for it" : "");
		}
	}
	if (cp.pl[i].walkWait > 0.0 && now - cp.pl[i].walkWait < CP_WALK_ALONE) {
		return 1;
	}
	float rot[2], dyaw = NetPl_Aim(e, &cp.pl[i].walkTo, rot, 1);
	in->rot[0] = NetRep_QRot(rot[0]);
	in->move[0] = NetRep_QMove(fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
	return 1;
}

/* test: a ship of the mission player i stays near without a shot (test line scan: DestroyMickus' transports until
 * the scan finds Mickus, who turns red; IntoTheGDB's bishops) */
static byte Cp_Scanned(int i, cEntity *v) {
	return cp.pl[i].scan && !Cp_Foe(v);
}

/* test: the item of the scripts' waypoint (CurrentWaypointName: BankersCTF's flag), NULL */
static cEntity *Cp_WaypointItem(void) {
	const char *n = g_CurrentWaypointName;
	cEntity *v = n && n[0] ? World_FindEntity(n) : NULL;
	return v && v->active && !v->deadNode.target && Entity_IsType(v, "item") ? v : NULL;
}

/* test: player i as a banker (on foot) robs the first of the mission's characters n in all that has money, as a
 * player does with the banker's slap (TalkToMayor: Corruptus emptied): it walks there, steps behind it (a victim that
 * looks away notices fewer robberies) and holds the action key while its arrow is on it; 0 when it is no banker or
 * no target has money */
static byte Cp_BankerSlaps(int i, NetRepInput *in, cEntity *e, cEntity **all, int n, double now) {
	cActCtrl *a = NetPl_Kind(e) == NETREP_CHAR ? WithLife_GetActCtrl(e) : NULL;
	cEntity *t = NULL;
	if (!a || !a->cls || strcmp(a->cls->name, "BankDirector")) {
		return 0;
	}
	for (int k = 0; k < n && !t; k++) {
		if (Entity_IsType(all[k], TYEN_WALKCHAR) && WChar(all[k])->money > 0) {
			t = all[k];
		}
	}
	if (!t) {
		return 0;
	}
	if (now - cp.pl[i].autoTold >= 5.0) {
		cp.pl[i].autoTold = now;
		Con_Printf("coop: player %d (auto) robs %s (money %d, %.0f away, its arrow on %s)\n", i, t->name, WChar(t)->money,
			(double)sqrtf((t->pos.x - e->pos.x) * (t->pos.x - e->pos.x) + (t->pos.z - e->pos.z) * (t->pos.z - e->pos.z)),
			WChar_Victim(e) ? WChar_Victim(e)->name : "nothing");
	}
	if (Cp_WalkStep(i, in, e, t, now)) {
		return 1;
	}
	/* yaw 0 looks to -z: behind it is along (sin yaw, cos yaw); from in front or aside it steps round to its back
	 * unless its arrow is on it already near enough */
	float yaw = WChar(t)->view.y, rot[2], bx = sinf(yaw), bz = cosf(yaw);
	float dx = e->pos.x - t->pos.x, dz = e->pos.z - t->pos.z, d = sqrtf(dx * dx + dz * dz);
	byte reach = WChar_Victim(e) == t && d < CP_MELEE * 2.5f;
	if (!reach && dx * bx + dz * bz < d * 0.7f) {
		vec3_t back = { t->pos.x + bx * CP_MELEE, t->pos.y, t->pos.z + bz * CP_MELEE };
		float dyaw = NetPl_Aim(e, &back, rot, 1);
		in->rot[0] = NetRep_QRot(rot[0]);
		in->move[0] = NetRep_QMove(fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
		return 1;
	}
	float dyaw = NetPl_Aim(e, &t->pos, rot, 1);
	in->rot[0] = NetRep_QRot(rot[0]);
	in->move[0] = NetRep_QMove(d > CP_MELEE && fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
	/* the banker's action loops while the key is held, each loop robs once */
	if (reach && fabsf(dyaw) < 0.3f) {
		in->buttons |= NETREP_IN_ACT;
	}
	return 1;
}

/* test: a messenger of the level (not flashed) within CP_MSG_NEAR of c, NULL */
static cEntity *Cp_MessengerNear(const cEntity *c) {
	for (cEntity *m = cWorld_GetFirstEntity(g_pWorld); m; m = cEntity_Next(m)) {
		float mx = m->pos.x - c->pos.x, mz = m->pos.z - c->pos.z;
		if (m->active && Entity_IsType(m, TYEN_WALKCHAR) && Players_Of(m) < 0 && mx * mx + mz * mz < CP_MSG_NEAR * CP_MSG_NEAR &&
			!strcmp(ChType_ActClass(ChType_ActCtrl(m))->name, "Messenger") &&
			crt_strncmp(cActCtrl_GetActionName(ChType_ActCtrl(m), 0), "Flashed", 7)) {
			return m;
		}
	}
	return NULL;
}

/* test: player i as a messenger plays the messengers' game of the data pack where the mission marks it
 * (MessengerDataPackMissionArrow: GetAuthorization's tube): another messenger's pack is shot off it (the action key
 * held at it: the lightning flashes it, it drops the pack), a free pack or one on an open desk is walked onto (the
 * touch takes it), its own pack goes to the desk with the mission arrow (the messengers' logic drops it there; the
 * messengers that come for it are shot on the way), another member's pack is escorted; 0
 * when it is no messenger or no pack is marked */
static byte Cp_MessengerPack(int i, NetRepInput *in, cEntity *e, double now) {
	cActCtrl *a = NetPl_Kind(e) == NETREP_CHAR ? WithLife_GetActCtrl(e) : NULL;
	cEntity *pack = g_MessengerDataPack ? World_FindEntity(g_MessengerDataPack) : NULL;
	nativeVal_t v;
	if (!a || !a->cls || strcmp(a->cls->name, "Messenger") || !pack || !Entity_IsType(pack, "item")) {
		return 0;
	}
	byte marked = Native_GVarGet("MessengerDataPackMissionArrow", &v) && Native_True(&v);
	Native_Free(&v);
	Native_ErrClear();
	if (!marked) {
		return 0;
	}
	cEntity *carrier = Obj(pack)->mod.link.target, *to = pack;
	byte shoot = carrier && carrier != e && Entity_IsType(carrier, TYEN_WALKCHAR) && Players_Of(carrier) < 0 &&
		!strcmp(ChType_ActClass(ChType_ActCtrl(carrier))->name, "Messenger");
	if (carrier == e || (carrier && Players_Of(carrier) >= 0)) {
		/* the messengers go for the one that carries it: one near it (not flashed) is shot first, as a player keeps them
		 * off its own pack or a member's; else its own pack goes to the desk, a member's is escorted */
		cEntity *m = Cp_MessengerNear(carrier);
		to = m ? m : carrier == e ? Cp_ArrowTarget(e) : carrier;
		shoot = m != NULL;
	} else if (shoot) {
		to = carrier;
	}
	if (!to) {
		return 1;
	}
	if (now - cp.pl[i].autoTold >= 5.0) {
		cp.pl[i].autoTold = now;
		Con_Printf("coop: player %d (auto) %s %s (the pack %s)\n", i, shoot ? carrier == e ? "keeps off" : "shoots the pack off" :
			carrier == e ? "carries the pack to" : carrier && to == carrier ? "escorts" : "takes the pack at", to->name,
			carrier ? carrier->name : "free");
	}
	/* one that comes for its own pack is shot where it stands (no chase away from the desk) */
	byte keep = shoot && carrier == e;
	float dx = to->pos.x - e->pos.x, dz = to->pos.z - e->pos.z, d = sqrtf(dx * dx + dz * dz);
	if (!keep && (!shoot || d > CP_MSG_SHOT) && Cp_WalkStep(i, in, e, to, now)) {
		return 1;
	}
	float rot[2], dyaw = NetPl_Aim(e, &to->pos, rot, 1);
	in->rot[0] = NetRep_QRot(rot[0]);
	in->move[0] = NetRep_QMove(!keep && d > (shoot ? CP_MSG_SHOT * 0.25f : CP_MELEE * 0.5f) && fabsf(dyaw) < 0.6f ? 1.0f : 0.0f);
	/* the messenger's lightning fires while the key is held */
	vec3_t from = { e->pos.x, e->pos.y + 100.0f, e->pos.z }, at = { to->pos.x, to->pos.y + 100.0f, to->pos.z };
	if (shoot && fabsf(dyaw) < 0.15f && !Engine_TestLine(&from, &at, NULL, NULL)) {
		in->buttons |= NETREP_IN_ACT;
	}
	return 1;
}

/* test: the mission waits for the player to board (Parking.SwitchVehicleMissionCallback: GetDataPart1, the super
 * deals) */
static byte Cp_WaitsForShip(void) {
	nativeVal_t v;
	byte set = Mode_ModGet("Parking", "SwitchVehicleMissionCallback", &v) && v.type != NV_NONE;
	Native_Free(&v);
	Native_ErrClear();
	return set;
}

/* test: player i plays the mission's targets on its own: fights the ones to fight, flies a race of RaceMaker, else
 * goes to the target (out of its ship) and uses its arrow, again every CP_AUTO_GAP s while no talk plays; a green ship
 * of the mission is escorted as a player does: the red ones near it fought first, else it stays with it (the helper
 * too) */
static void Cp_TestAuto(int i, NetRepInput *in, double back) {
	cEntity *e = Players_Get(i), *all[CP_FIGHT_MAX];
	int kind = NetPl_Kind(e);
	if (!kind || Coop_Holds(i)) {
		return;
	}
	double now = Instance_Time();
	if (now < cp.pl[i].talkUntil) {
		/* a conversation of its own plays: it listens (single player's input stops there), its next use CP_AUTO_GAP after
		 * it (a use between two lines started BeatPickus' talk with Traitorius again) */
		cp.pl[i].autoSince = now;
		return;
	}
	byte pulse = fmod(now, 2.0) < 1.0 / INSTANCE_TICK_HZ;
	/* the mission's targets to fight, else any red one of the level (a mission's enemies it marks without targets; not
	 * while the mission has a target of another kind: a ship to follow, a character to talk to; the helper fights them) */
	cEntity *escort = Cp_Escorted();
	/* the waypoint's item (test line waypoint: a flag of the CTF) is flown onto first, as a player follows its arrow */
	cEntity *wp = cp.pl[i].waypoint ? Cp_WaypointItem() : NULL;
	if (wp) {
		if (strcmp(cp.pl[i].autoAt, wp->name) || now - cp.pl[i].autoSince >= 1.0) {
			Str_Copy(cp.pl[i].autoAt, wp->name, sizeof(cp.pl[i].autoAt));
			cp.pl[i].autoSince = now;
			Con_Printf("coop: player %d (auto) goes onto %s (the waypoint)\n", i, wp->name);
			Travel_TestGoto(i, wp->name, 0.0f, 0.0f);
		}
		return;
	}
	/* a messenger plays the pack's game before the fights (the messengers attack the one that carries the pack) */
	if (cp.pl[i].autoOn != 2 && Cp_MessengerPack(i, in, e, now)) {
		return;
	}
	/* a mission arrow to take (BankersCombat's blueprints while the bankers attack) before the level's red ones; one on
	 * a ship to shoot down before the mission's targets to use (Ambush's probes, its phone cab still the target) */
	cEntity *arrow = Cp_ArrowTarget(e);
	byte other = Cp_FightTargets("", all, 1, 0) || arrow;
	const char *foes = Cp_FightTargets("", all, CP_FIGHT_MAX, 1) ? "" : arrow && Cp_ShipFoe(arrow) && !Cp_Scanned(i, arrow) ? arrow->name : (escort ? Cp_RedNear(escort) :
		(cp.pl[i].autoOn == 2 || !other) && Cp_FightTargets(CP_FIGHT_RED, all, CP_FIGHT_MAX, 1)) ? CP_FIGHT_RED : NULL;
	if (foes) {
		Str_Copy(cp.pl[i].fightList, foes, sizeof(cp.pl[i].fightList));
		Cp_TestFight(i, in, back);
		return;
	}
	if (cp.pl[i].autoOn == 2 && !escort) {
		/* a helper: it only fights (what it was told to when the mission shows nothing) */
		if (cp.pl[i].autoList[0]) {
			Str_Copy(cp.pl[i].fightList, cp.pl[i].autoList, sizeof(cp.pl[i].fightList));
			Cp_TestFight(i, in, back);
		}
		return;
	}
	if (World_FindEntity(CP_CHECKPOINT)) {
		if (kind == NETREP_SHIP) {
			Cp_TestRace(i);
		} else {
			in->buttons |= pulse ? NETREP_IN_SWITCH : 0;
		}
		return;
	}
	/* the mission's targets one after the other (the right one of three to talk to: ThreeStooges' mercenaries) */
	int nt = Cp_FightTargets("", all, CP_FIGHT_MAX, 0);
	if (cp.pl[i].autoOn != 2 && Cp_BankerSlaps(i, in, e, all, nt, now)) {
		return;
	}
	cEntity *t = cp.pl[i].autoOn == 2 ? escort : nt ? all[cp.pl[i].autoTurn % nt] : NULL;
	/* else the nearest one the scripts marked with the mission arrow */
	if (!t) {
		t = Cp_ArrowTarget(e);
	}
	/* else, and before a target to use, an item on the radar: flown or walked onto, as a player takes it (DistractHumphrey's
	 * meeting point while the phone cab of the call stays the mission's target) */
	cEntity *item = t && Entity_IsType(t, TYEN_CAR) ? NULL : Cp_RadarItem(e);
	if (item && kind == NETREP_CHAR && Cp_InAir(item)) {
		/* one in the air is flown onto: into a ship first (the switch) */
		in->buttons |= pulse ? NETREP_IN_SWITCH : 0;
		return;
	}
	if (item) {
		/* again at once when it moved on (Win1stRace's buoy, taken at each checkpoint) */
		vec3_t *at = &cp.pl[i].autoItemAt;
		float mx = item->pos.x - at->x, my = item->pos.y - at->y, mz = item->pos.z - at->z;
		if (strcmp(cp.pl[i].autoAt, item->name) || now - cp.pl[i].autoSince >= CP_AUTO_GAP ||
			mx * mx + my * my + mz * mz > CP_AUTO_NEAR * CP_AUTO_NEAR) {
			*at = item->pos;
			Str_Copy(cp.pl[i].autoAt, item->name, sizeof(cp.pl[i].autoAt));
			cp.pl[i].autoSince = now;
			Con_Printf("coop: player %d (auto) goes onto %s (on the radar)\n", i, item->name);
			Travel_TestGoto(i, item->name, 0.0f, 0.0f);
		}
		return;
	}
	if (!t) {
		if (cp.pl[i].autoList[0]) {
			Str_Copy(cp.pl[i].fightList, cp.pl[i].autoList, sizeof(cp.pl[i].fightList));
			Cp_TestFight(i, in, back);
		} else if (kind == NETREP_CHAR && Cp_WaitsForShip()) {
			/* the mission starts when the player boards (its level again after a failure): into a ship */
			in->buttons |= pulse ? NETREP_IN_SWITCH : 0;
		}
		return;
	}
	/* what it was told to fight comes before the rest (not before a parked ship of the mission to board) */
	if (cp.pl[i].autoList[0] && !(Entity_IsType(t, TYEN_CAR) && !strncmp(t->name, "Parked", 6)) &&
		Cp_FightTargets(cp.pl[i].autoList, all, 1, -1)) {
		Str_Copy(cp.pl[i].fightList, cp.pl[i].autoList, sizeof(cp.pl[i].fightList));
		Cp_TestFight(i, in, back);
		return;
	}
	if (strcmp(cp.pl[i].autoAt, t->name) && now - cp.pl[i].autoTold >= 10.0) {
		cp.pl[i].autoTold = now;
		Con_Printf("coop: player %d (auto) at the target %s\n", i, t->name);
	}
	/* a flying ship of the mission is to shoot down, a green one (to escort or follow) to stay with */
	char *tg = NULL;
	if (Entity_IsType(t, TYEN_CAR) && strncmp(t->name, "Parked", 6) && Entity_VarS(t, "TgType", &tg, 0)) {
		Str_Copy(cp.pl[i].fightList, t->name, sizeof(cp.pl[i].fightList));
		cp.pl[i].follow = !Cp_ShipFoe(t) || Cp_Scanned(i, t);
		Cp_TestFight(i, in, back);
		cp.pl[i].follow = 0;
		return;
	}
	if (kind == NETREP_SHIP) {
		/* its ship flies to the target first, then out of it near the place it goes to (a roof pushes a ship off: no
		 * time to wait for a calm one) */
		if (!Cp_FlyTo(i, in, e, t, CP_AUTO_OUT, now)) {
			in->buttons |= fmod(now, 0.5) < 1.0 / INSTANCE_TICK_HZ ? NETREP_IN_SWITCH : 0;
		}
		return;
	}
	/* outdoors a target far away: into a ship first (the switch: a parked one near, else its called ship), as a player
	 * flies there; it walks when it gets none */
	float fx = t->pos.x - e->pos.x, fz = t->pos.z - e->pos.z;
	if (Cp_WalkGraph() || Entity_IsType(t, TYEN_CAR) || fx * fx + fz * fz <= CP_AUTO_SHIP * CP_AUTO_SHIP) {
		cp.pl[i].shipAsk = 0.0;
	} else if (cp.pl[i].shipAsk <= 0.0) {
		cp.pl[i].shipAsk = now;
		Con_Printf("coop: player %d (auto) asks for a ship to %s\n", i, t->name);
	} else if (now - cp.pl[i].shipAsk < CP_SHIP_ASK) {
		in->buttons |= fmod(now, 2.0) < 1.0 / INSTANCE_TICK_HZ ? NETREP_IN_SWITCH : 0;
		return;
	}
	/* a parked ship of the mission (the one to steal): next to it and into it with the switch */
	if (Entity_IsType(t, TYEN_CAR) && !strncmp(t->name, "Parked", 6)) {
		if (strcmp(cp.pl[i].autoAt, t->name) || now - cp.pl[i].autoSince >= CP_AUTO_GAP) {
			Str_Copy(cp.pl[i].autoAt, t->name, sizeof(cp.pl[i].autoAt));
			cp.pl[i].autoSince = now;
			Con_Printf("coop: player %d (auto) boards %s\n", i, t->name);
			Travel_TestGoto(i, t->name, 0.0f, 300.0f);
			Rep_TestSwitch(i, 0);
		}
		return;
	}
	/* a talk plays: it listens */
	if (g_RemainTalkTime > 0.01f) {
		cp.pl[i].autoSince = now;
		return;
	}
	/* a phone cab or a desktop while the police is after it: it waits for the alarm to fall, as the single player
	 * does after the call's "the police is still after you" (PhoneCab.UseAudioCom, the missions' UsePhoneCab) */
	char *fn = NULL;
	const cAlarm *al = Players_Alarm(i);
	if (al && al->on && Entity_VarS(t, "UseFunc", &fn, 0) && fn && (strstr(fn, "Phone") || strstr(fn, "AudioCom") ||
		strstr(fn, "Desktop"))) {
		if (now - cp.pl[i].autoTold >= 10.0) {
			cp.pl[i].autoTold = now;
			Con_Printf("coop: player %d (auto) waits for its alarm to fall before %s\n", i, t->name);
		}
		cp.pl[i].autoUse = 0.0;
		return;
	}
	if (cp.pl[i].autoUse > 0.0 && now >= cp.pl[i].autoUse) {
		/* the use of its arrow, as the test line use (the option its client chose) */
		cp.pl[i].autoUse = 0.0;
		cp.pl[i].autoTurn++;
		cEntity *v = WChar_Victim(e);
		Con_Printf("coop: player %d (auto) %s %s (its arrow on %s)\n", i, WLife(t)->arrow.canPhoto ? "photographs" : "uses",
			t->name, v ? v->name : "nothing");
		if (WLife(t)->arrow.canPhoto) {
			/* a photo is the use button with the arrow on it */
			cp.pl[i].press = NETREP_IN_ACT3;
			cp.pl[i].pressTicks = 12;
			cp.pl[i].pressPulse = 1;
		} else {
			Travel_TestUse(i, t->name, "");
		}
	}
	/* not while its photo's press runs (the walk to the next target would turn it away); outdoors (no graph of the
	 * walkers) it walks straight to a target far away */
	if ((cp.pl[i].walk || !Cp_WalkGraph()) && cp.pl[i].pressTicks <= 0 && Cp_WalkStep(i, in, e, t, now)) {
		return;
	}
	if (!strcmp(cp.pl[i].autoAt, t->name) && now - cp.pl[i].autoSince < CP_AUTO_GAP) {
		return;
	}
	Str_Copy(cp.pl[i].autoAt, t->name, sizeof(cp.pl[i].autoAt));
	cp.pl[i].autoSince = now;
	/* a target without a use or a photo (the arrow over an item to take) is walked onto */
	byte use = WLife(t)->arrow.canUse || WLife(t)->arrow.canPhoto;
	cp.pl[i].autoUse = use ? now + 2.0 : 0.0;
	float dx = 0.0f, dz = 0.0f;
	if (use) {
		Cp_Approach(e, t, CP_AUTO_NEAR, &dx, &dz);
	}
	Travel_TestGoto(i, t->name, dx, dz);
}

/* test: player i possesses the character: next to it, facing it, until its arrow is on it (a walking one moves on),
 * then the possession button; given up after CP_POSSESS_SECS */
static void Cp_TestPossess(int i, NetRepInput *in) {
	cEntity *e = Players_Get(i), *c = World_FindEntity(cp.pl[i].possess);
	double now = Instance_Time();
	/* the possessed one goes (the member's new character of its class) */
	if (!c || e == c || now - cp.pl[i].possessSince > CP_POSSESS_SECS) {
		Con_Printf("coop: player %d: the possession of %s %s\n", i, cp.pl[i].possess, !c || e == c ? "done" : "given up");
		cp.pl[i].possess[0] = 0;
		return;
	}
	if (NetPl_Kind(e) != NETREP_CHAR) {
		return;		/* between its old and its new character */
	}
	if (WChar_Victim(e) == c) {
		in->buttons |= fmod(now, 0.5) < 1.0 / INSTANCE_TICK_HZ ? NETREP_IN_ACT2 : 0;
	} else if (now - cp.pl[i].possessAt >= 0.5) {
		cp.pl[i].possessAt = now;
		Travel_TestGoto(i, c->name, 0.0f, 150.0f);
	}
}

void Coop_TestInput(int i, NetRepInput *in, double back) {
	if (i < 0 || i >= PLAYERS_MAX) {
		return;
	}
	if (cp.pl[i].possess[0]) {
		Cp_TestPossess(i, in);
	}
	if (cp.pl[i].race > 0.0f) {
		Cp_TestRace(i);
	}
	if (cp.pl[i].fight) {
		Cp_TestFight(i, in, back);
	} else if (cp.pl[i].autoOn) {
		Cp_TestAuto(i, in, back);
	}
	if (cp.pl[i].pressTicks <= 0) {
		return;
	}
	/* a pulsed press (a negative count of ticks): pressed one tick in every four */
	in->buttons |= !cp.pl[i].pressPulse || cp.pl[i].pressTicks % 4 == 0 ? cp.pl[i].press : 0;
	cp.pl[i].pressTicks--;
}

/* the buttons of a test press: act (the action), act2 (the possession), act3 (the use), lock */
static uint16_t Cp_Buttons(const char *names) {
	static const struct {
		const char *name;
		uint16_t bit;
	} bits[] = { { "act", NETREP_IN_ACT }, { "act2", NETREP_IN_ACT2 }, { "act3", NETREP_IN_ACT3 },
		{ "lock", NETREP_IN_LOCK }, { "boost", NETREP_IN_BOOST }, { "switch", NETREP_IN_SWITCH } };
	uint16_t b = 0;
	for (int k = 0; k < (int)(sizeof(bits) / sizeof(bits[0])); k++) {
		const char *at = strstr(names, bits[k].name);
		size_t n = strlen(bits[k].name);
		while (at && ((at > names && at[-1] != ',') || (at[n] && at[n] != ','))) {
			at = strstr(at + 1, bits[k].name);
		}
		b |= at ? bits[k].bit : 0;
	}
	return b;
}

/* test line "where <prefix>": the place, life and logic target of the entities whose names start with it (the
 * followers of a walk, a mission's characters) */
static byte Cp_TestWhere(const char *line) {
	char name[CP_NAME];
	if (sscanf(line, "where %63s", name) != 1) {
		return 0;
	}
	size_t n = strlen(name);
	for (cEntity *v = cWorld_GetFirstEntity(g_pWorld); v; v = cEntity_Next(v)) {
		char *tgt = NULL;
		if (strncmp(v->name, name, n)) {
			continue;
		}
		Entity_VarS(v, "LLogicTarget", &tgt, 0);
		Con_Printf("coop: %s at %.0f %.0f %.0f, %s, life %.0f, player %d, logic target %s\n", v->name, (double)v->pos.x,
			(double)v->pos.y, (double)v->pos.z, v->active ? "active" : "inactive",
			Entity_IsType(v, TYEN_WALKCHAR) || Entity_IsType(v, TYEN_CAR) ? (double)WLife(v)->life : 0.0, Players_Of(v),
			tgt ? tgt : "-");
	}
	return 1;
}

byte Coop_Command(const char *line) {
	char name[CP_NAME], val[256];
	int i;
	if (!Coop_On()) {
		return 0;
	}
	if (Cp_TestWhere(line)) {
		return 1;
	}
	if (sscanf(line, "leader %d", &i) == 1) {
		Cp_ToLeader(i);
		return 1;
	}
	if (sscanf(line, "qload %63s", name) == 1 || !strcmp(line, "restart") || !strcmp(line, "theend")) {
		/* as the scripts do: SaveGame.QLoad, SaveGame.Restart (a super deal), the end's credits section */
		Players_Bind(Cp_Player());
		if (!strcmp(line, "theend")) {
			SaveVars_ScriptDelete("");
			Str_Copy(name, "Levels/Menu", sizeof(name));
		}
		byte ok = Mode_Import("SaveGame") && (!strcmp(line, "restart") ? PORT_CALL(SaveGame, Restart, NULL, "") :
			PORT_CALL(SaveGame, QLoad, NULL, "s", name));
		if (!ok) {
			Native_ErrPrint();
		}
		Players_BindTurn();
		return 1;
	}
	if (!strncmp(line, "call ", 5) && line[5] != '@') {
		/* a call of the scripts as a call table line ("Module.Func|arg|..."), for the bound player */
		const char *one[2] = { line + 5, NULL };
		Players_Bind(Cp_Player());
		Con_Printf("coop: call %s: %s\n", line + 5, Mode_Calls(NULL, one) ? "done" : "failed");
		Native_ErrPrint();
		Players_BindTurn();
		return 1;
	}
	if (sscanf(line, "show %63s", name) == 1 && strchr(name, '.')) {
		/* a variable of a module of the scripts */
		nativeVal_t v, r;
		char *dot = strchr(name, '.');
		*dot = 0;
		Native_SetNone(&v);
		Native_SetNone(&r);
		if (Mode_ModGet(name, dot + 1, &v)) {
			Mode_Repr(&v, &r);
			Con_Printf("coop: %s.%s = %.200s\n", name, dot + 1, r.type == NV_STR ? Native_Text(&r) : "?");
		} else {
			Con_Printf("coop: no %s.%s\n", name, dot + 1);
		}
		Native_ErrClear();
		Native_Free(&v);
		Native_Free(&r);
		return 1;
	}
	if (sscanf(line, "aiv %63s", name) == 1) {
		/* a vehicle's AI: its state and sub state, the point it goes to, its life */
		cEntity *e = World_FindEntity(name);
		cAIVehicle *ai = e && Entity_IsType(e, TYEN_CAR) ? Entity_GetAI(e) : NULL;
		char st[256] = "", in[256] = "";
		vec3_t d, p;
		cAIChar *ac = e && Entity_IsType(e, TYEN_WALKCHAR) ? Entity_GetAI(e) : NULL;
		if (ac) {
			/* a walking character's AI: its state and target */
			Con_Printf("coop: %s char AI state %d intern %d, target %s, at %.0f %.0f %.0f\n", name, ac->state, ac->internState,
				Nto1Node_TargetName(&ac->base.target), (double)e->pos.x, (double)e->pos.y, (double)e->pos.z);
			return 1;
		}
		if (!ai) {
			Con_Printf("coop: %s has no vehicle AI\n", name);
			return 1;
		}
		cAIVehicle_GetStateName(ai, st);
		cAIVehicle_GetInternStateName(ai, in);
		cAIVehicleControl_GetDest(ai->control, &d);
		Entity_GetPos(e, &p);
		double away = sqrt((double)((d.x - p.x) * (d.x - p.x) + (d.y - p.y) * (d.y - p.y) + (d.z - p.z) * (d.z - p.z)));
		Con_Printf("coop: %s AI %s (%d) %s at %.0f %.0f %.0f, to %.0f %.0f %.0f (%.0f away), life %.0f\n", name, st,
			ai->state, in, (double)p.x, (double)p.y, (double)p.z, (double)d.x, (double)d.y, (double)d.z, away,
			(double)WLife(e)->life);
		return 1;
	}
	if (sscanf(line, "kill %63s", name) == 1 && strchr(name, '.')) {
		/* the entities of a list of the scripts (the targets of a mission) die as by a weapon of the member the scripts
		 * act for: their OnDeath runs with it as the winner */
		nativeVal_t list, it;
		char *dot = strchr(name, '.');
		*dot = 0;
		int p = Cp_Player(), n = Mode_ModGet(name, dot + 1, &list) ? list.type == NV_STR ? 1 : Mode_Loop(&list) : -1;
		cEntity *by = p >= 0 ? Players_Get(p) : NULL;
		for (int k = 0; k < n; k++) {
			Native_SetNone(&it);
			cEntity *v = list.type == NV_STR ? World_FindEntity(Native_Text(&list)) : Native_SeqItem(&list, k, &it) &&
				it.type == NV_STR ? World_FindEntity(Native_Text(&it)) : NULL;
			if (v && Entity_IsType(v, TYEN_WITH_LIFE)) {
				if (by) {
					cVehicleAttack_HitBy(&WLife(v)->attack, by->name);
				}
				WLife(v)->life = 0;
				Con_Printf("coop: %s of %s.%s killed by %s\n", v->name, name, dot + 1, by ? by->name : "-");
			}
			Native_Free(&it);
		}
		Native_ErrPrint();
		if (n >= 0) {
			Native_Free(&list);
		}
		return 1;
	}
	int ticks = 3;
	if (sscanf(line, "press %d %63s %d", &i, name, &ticks) >= 2 && i >= 0 && i < PLAYERS_MAX) {
		/* the buttons as the member's client sends them, for some ticks (the game's own input path) */
		cp.pl[i].press = Cp_Buttons(name);
		cp.pl[i].pressTicks = ticks < 0 ? -ticks : ticks;
		cp.pl[i].pressPulse = ticks < 0;
		cEntity *e = Players_Get(i), *v = NetPl_Kind(e) == NETREP_CHAR ? WChar_Victim(e) : NULL;
		Con_Printf("coop: player %d presses %s (%04x) for %d ticks (arrow on %s)\n", i, name, cp.pl[i].press, ticks,
			v ? v->name : "-");
		return 1;
	}
	int on = 1;
	if (sscanf(line, "race %d %d", &i, &on) >= 1 && i >= 0 && i < PLAYERS_MAX) {
		cp.pl[i].race = on ? 1.0f : 0.0f;
		cp.pl[i].raceAt = (vec3_t){ 0.0f, 0.0f, 0.0f };
		Con_Printf("coop: player %d flies the race%s\n", i, on ? "" : ": off");
		return 1;
	}
	name[0] = 0;
	if (sscanf(line, "auto %d %d %63s", &i, &on, name) >= 1 && i >= 0 && i < PLAYERS_MAX) {
		cp.pl[i].autoOn = (byte)(on < 0 ? 0 : on > 2 ? 2 : on);
		Str_Copy(cp.pl[i].autoList, name, sizeof(cp.pl[i].autoList));
		cp.pl[i].autoAt[0] = 0;
		cp.pl[i].autoUse = 0.0;
		cp.pl[i].fightAt[0] = 0;
		Con_Printf("coop: player %d plays the mission's targets%s\n", i, !on ? ": off" : on == 2 ? ": fights only" : "");
		return 1;
	}
	int pct = 0;
	name[0] = 0;
	if (sscanf(line, "walk %d %d %63s", &i, &pct, name) >= 2 && i >= 0 && i < PLAYERS_MAX) {
		cp.pl[i].walk = pct != 0;
		cp.pl[i].follower[0] = 0;
		Str_Copy(cp.pl[i].walkWho, name, sizeof(cp.pl[i].walkWho));
		Con_Printf("coop: player %d %s to its targets\n", i, pct ? "walks" : "goes");
		return 1;
	}
	if (sscanf(line, "waypoint %d %d", &i, &pct) == 2 && i >= 0 && i < PLAYERS_MAX) {
		cp.pl[i].waypoint = pct != 0;
		Con_Printf("coop: player %d %s the waypoint's item first\n", i, pct ? "takes" : "leaves");
		return 1;
	}
	if (sscanf(line, "scan %d %d", &i, &pct) == 2 && i >= 0 && i < PLAYERS_MAX) {
		cp.pl[i].scan = pct != 0;
		Con_Printf("coop: player %d %s the mission's ships that are no foes\n", i, pct ? "scans" : "fights");
		return 1;
	}
	if (sscanf(line, "upto %d %63s %d", &i, name, &pct) == 3 && i >= 0 && i < PLAYERS_MAX) {
		Str_Copy(cp.pl[i].upTo, strcmp(name, "0") ? name : "", sizeof(cp.pl[i].upTo));
		cp.pl[i].upToMax = pct;
		Con_Printf("coop: player %d holds its fire while %s is at %d\n", i, name, pct);
		return 1;
	}
	if (sscanf(line, "spare %d %d", &i, &pct) == 2 && i >= 0 && i < PLAYERS_MAX) {
		cp.pl[i].spare = pct;
		Con_Printf("coop: player %d spares a ship under %d%% of its life\n", i, pct);
		return 1;
	}
	if (sscanf(line, "possess %d %63s", &i, name) == 2 && i >= 0 && i < PLAYERS_MAX) {
		Str_Copy(cp.pl[i].possess, name, sizeof(cp.pl[i].possess));
		cp.pl[i].possessSince = Instance_Time();
		cp.pl[i].possessAt = 0.0;
		Con_Printf("coop: player %d goes to possess %s\n", i, name);
		return 1;
	}
	if (sscanf(line, "usenear %d %63s", &i, name) == 2 && i >= 0 && i < PLAYERS_MAX) {
		/* the member next to the nearest entity whose name starts with the text (a conversor, a phone cab), and it uses
		 * its arrow */
		cEntity *e = Players_Get(i), *best = NULL;
		float bestD = 0.0f;
		size_t n = strlen(name);
		for (cEntity *v = e ? cWorld_GetFirstEntity(g_pWorld) : NULL; v; v = cEntity_Next(v)) {
			float dx = v->pos.x - e->pos.x, dz = v->pos.z - e->pos.z, d = dx * dx + dz * dz;
			if (!strncmp(v->name, name, n) && Entity_IsType(v, TYEN_WITH_LIFE) && (!best || d < bestD)) {
				best = v;
				bestD = d;
			}
		}
		if (best) {
			Travel_TestGoto(i, best->name, 0.0f, 250.0f);
			Travel_TestUse(i, best->name, "");
		} else {
			Con_Printf("coop: no %s for player %d\n", name, i);
		}
		return 1;
	}
	if (sscanf(line, "board %d %63s", &i, name) == 2 && i >= 0 && i < PLAYERS_MAX) {
		/* the member next to the nearest parked ship whose model has the text, and into it with its switch */
		cEntity *e = Players_Get(i), *best = NULL;
		float bestD = 0.0f;
		for (cEntity *v = e ? cWorld_GetFirstEntity(g_pWorld) : NULL; v; v = cEntity_Next(v)) {
			char *model = NULL, up[256];
			if (!Entity_IsType(v, TYEN_CAR) || strncmp(v->name, "Parked", 6) || v->deadNode.target ||
				WLife(v)->life <= 0.0f || Players_Of(v) >= 0 || !Entity_VarS(v, "Model", &model, 0) || !model) {
				continue;
			}
			Str_Copy(up, model, sizeof(up));
			crt_strupr(up);
			float dx = v->pos.x - e->pos.x, dz = v->pos.z - e->pos.z, d = dx * dx + dz * dz;
			if (strstr(up, name) && (!best || d < bestD)) {
				best = v;
				bestD = d;
			}
		}
		if (best) {
			Travel_TestGoto(i, best->name, 0.0f, 300.0f);
			Rep_TestSwitch(i, 0);
		} else {
			Con_Printf("coop: no parked ship of %s for player %d\n", name, i);
		}
		return 1;
	}
	name[0] = 0;
	if (sscanf(line, "fight %d %63s", &i, name) >= 1 && i >= 0 && i < PLAYERS_MAX) {
		cp.pl[i].fight = strcmp(name, "0") != 0;
		Str_Copy(cp.pl[i].fightList, cp.pl[i].fight ? name : "", sizeof(cp.pl[i].fightList));
		cp.pl[i].fightAt[0] = 0;
		cp.pl[i].fightSince = 0.0;
		Con_Printf("coop: player %d fights %s\n", i, !cp.pl[i].fight ? "no more" : name[0] ? name : "the mission's targets");
		return 1;
	}
	float dx = 0.0f, dz = 0.0f;
	if (sscanf(line, "target %d %f %f", &i, &dx, &dz) >= 1 || sscanf(line, "usetarget %d", &i) == 1) {
		/* the member onto the mission's target (MissionsFuncs.currentTarget), or it uses the target */
		nativeVal_t t;
		Native_SetNone(&t);
		if (Mode_ModGet("MissionsFuncs", "currentTarget", &t) && t.type == NV_STR) {
			if (line[0] == 'u') {
				Travel_TestUse(i, Native_Text(&t), "");
			} else {
				Travel_TestGoto(i, Native_Text(&t), dx, dz);
			}
		} else {
			Con_Printf("coop: no mission target\n");
		}
		Native_ErrClear();
		Native_Free(&t);
		return 1;
	}
	if (sscanf(line, "travel %d %63s", &i, name) == 2) {
		Con_Printf("coop: player %d travels alone to %s\n", i, name);
		Travel_Go(i, name, "", "", 0);
		return 1;
	}
	if (strncmp(line, "story", 5)) {
		return 0;
	}
	if (sscanf(line, "story set %63s %255[^\n]", name, val) == 2) {
		SaveVar_Set(name, val);
		Con_Printf("coop: %s = %s\n", name, val);
	} else if (sscanf(line, "story inc %63s %d", name, &i) == 2) {
		Con_Printf("coop: %s = %d\n", name, SaveVar_Inc(name, i));
	} else if (sscanf(line, "story hold %d", &i) == 1) {
		cp.hold = (byte)(i != 0);
		Con_Printf("coop: the story changes %s\n", cp.hold ? "wait" : "go");
	} else if (sscanf(line, "story get %63s", name) == 1) {
		const char *v = SaveVar_Get(name);
		Con_Printf("coop: %s %s%s\n", name, v ? "= " : "is not set", v ? v : "");
	} else {
		const char *lib = SaveVar_Get("Mission.Library"), *map = SaveVar_Get("Mission.Map");
		Con_Printf("coop: story %s, %d changes to send, mission %s in %s, scene %s, action set %s\n", cp.synced ?
			"in sync" : "waiting for the coordinator", cp.ndirty, lib ? lib : "-", map ? map : "-", cp.inScene ? "playing" :
			"none", TABLE_AT(g_ActionSets, Input_GetActionSet())->name);
	}
	return 1;
}

/* ================================ client ================================ */

/* the video the next level load plays (a headless client plays none) */
static void Cp_ClientVideo(const char *file) {
	if (!g_HeadlessPlay && !Mode_GVarSetS("PlayVideo", file)) {
		Native_ErrPrint();
	}
}

/* the notices that name a level get its station's name in the client's language */
static const char *Cp_NoticeArg(const char *key, const char *arg) {
	return !strcmp(key, "MP_CoopTravel") || !strcmp(key, "MP_CoopScene") ? Places_LevelText(arg) : arg;
}

void Coop_Cue(int kind, const char *text) {
	char key[64], line[256];
	if (kind == COORD_CUE_COOP) {
		cp.world = 1;
	} else if (kind == COORD_CUE_INTRO) {
		Con_Printf("coop: a new story: the intro %s\n", text);
		Cp_ClientVideo(text);
	} else if (kind == COORD_CUE_NOTICE) {
		const char *bar = strchr(text, '|');
		int n = bar ? (int)(bar - text) : (int)strlen(text);
		Str_PrintfQuiet(key, sizeof(key), "%.*s", n, text);
		Texts_Format(line, sizeof(line), key, Cp_NoticeArg(key, bar ? bar + 1 : ""));
		Con_Printf("coop: %s\n", line);
		Text_Message(COOP_NOTICE_MS, "%s", line);
	}
}

byte Coop_Client(void) {
	return g_Play && cp.world;
}

void Coop_ClientNoAnim(const char *entity, const char *action) {
	static char told[16][CP_NAME * 2];
	static int ntold;
	char key[CP_NAME * 2];
	Str_PrintfQuiet(key, sizeof(key), "%s.%s", entity, action);
	for (int k = 0; k < ntold; k++) {
		if (!strcmp(told[k], key)) {
			return;
		}
	}
	if (ntold < 16) {
		Str_Copy(told[ntold++], key, sizeof(told[0]));
	}
	Con_Printf("coop: %s has no animation for %s here: the scene's event is left out\n", entity, action);
}

void Coop_ClientLevelStart(void) {
	/* the level's start sets up the camera of view 0 as the single player's (Fake's clip planes, its credits); a
	 * client has none before its own character came */
	if (g_Play && cp.world && Play_ViewCamera(0.0f, 0.0f, 0.0f)) {
		Con_Printf("coop: the camera of view 0 for the level's start\n");
	}
}

byte Coop_ClientNoScene(void) {
	return g_Play && cp.world && !cp.replay;
}

/* the scene's callback of the scripts is the instance's (Fake's chain would load the next level here) */
static void Cp_ClientNoCallback(void) {
	nativeVal_t none;
	Native_SetNone(&none);
	if (!Mode_SetModVar("Scene", "EndOfSceneCallback", &none)) {
		Native_ErrPrint();
	}
}

static byte Cp_ClientImport(const char *mod);

/* the scene's table run and the scene played here */
static void Cp_ClientScene(NetRepReader *r) {
	static char text[CP_SCENE_MSG];
	static const char *lines[CP_SCENE_LINES + 1];
	char module[CP_NAME], cls[CP_NAME];
	NetRep_GetStr(r, module, sizeof(module));
	NetRep_GetStr(r, cls, sizeof(cls));
	int n = (int)NetRep_GetU16(r), at = 0;
	for (int k = 0; k < n && k < CP_SCENE_LINES && !r->bad; k++) {
		int len = (int)NetRep_GetU16(r);
		if (at + len + 1 > (int)sizeof(text)) {
			r->bad = 1;
			break;
		}
		CoordMsg_GetBytes(r, text + at, len);
		text[at + len] = 0;
		lines[k] = text + at;
		at += len + 1;
		lines[k + 1] = NULL;
	}
	char dmod[CP_NAME], dsec[CP_NAME];
	NetRep_GetStr(r, dmod, sizeof(dmod));
	NetRep_GetStr(r, dsec, sizeof(dsec));
	if (r->bad || !strcmp(cls, cp.clientScene)) {
		return;
	}
	/* a class the client has already (made by its level's start, or a scene of the level's own data) plays as it
	 * is: its table again would add its actions a second time (a fatal check) */
	nativeVal_t sname, have;
	char text2[CP_NAME + 8];
	Str_PrintfQuiet(text2, sizeof(text2), "Scene%s", cls);
	Native_SetText(&sname, text2, (int)strlen(text2));
	Native_SetNone(&have);
	byte got = Mode_SActV("GetClass", &sname, &have) && Native_True(&have);
	Native_Free(&have);
	Native_ErrClear();
	/* the events of the scene call the functions of its module (a mission's the client's level start did not load) */
	if ((dmod[0] && !Cp_ClientImport(dmod)) || (module[0] && !Cp_ClientImport(module))) {
		Native_ErrPrint();
	}
	/* a scene of a level data section: the client runs that section */
	if (!got && dsec[0]) {
		if (!Script_LevelSection(dmod, dsec)) {
			Native_ErrPrint();
		}
		Native_SetNone(&have);
		got = Mode_SActV("GetClass", &sname, &have) && Native_True(&have);
		Native_Free(&have);
		Native_ErrClear();
		Con_Printf("coop: scene %s from the data section %s of %s: %s\n", cls, dsec, dmod, got ? "loaded" : "no class");
	}
	Native_Free(&sname);
	if (!n && !got) {
		Con_Printf("coop: scene %s of the instance: no class here\n", cls);
		return;
	}
	if (got) {
		n = 0;
	}
	Con_Printf("coop: scene %s of the instance plays here\n", cls);
	/* line by line, a failing one does not stop the scene. The "@" calls of the level's module run too: an event of
	 * theirs that forces an action whose animation the client's model lacks (the instance's mission gave it, Orbit's
	 * desk functionary) is left out (cActCtrl_PlayAction) */
	for (int k = 0; k < n && lines[k]; k++) {
		const char *one[2] = { lines[k], NULL };
		if (!Mode_Calls(module, one)) {
			Native_ErrPrint();
		}
	}
	cp.replay = 1;
	byte ok = Mode_Import("Scene") && PORT_CALL(Scene, Play, NULL, "s", cls);
	cp.replay = 0;
	if (!ok) {
		Native_ErrPrint();
		return;
	}
	Cp_ClientNoCallback();
	Str_Copy(cp.clientScene, cls, sizeof(cp.clientScene));
	cp.skipped = 0;
	cp.sceneAt = Watch_ShownTime();
}

/* the answer to the instance's question: the menu closes here, the instance runs the function of the answer */
static void Cp_ClientAnswer(byte yes) {
	uint8_t buf[4] = { COOP_MSG, COOP_C_ANSWER, COOP_MENU_YESNO, yes };
	if (cp.cmenu != COOP_MENU_YESNO) {
		return;
	}
	cp.cmenu = 0;
	if (!PORT_CALL(Menu, BackToGame, NULL, "is", 0L, "")) {
		Native_ErrPrint();
	}
	if (Watch_SendReliable(buf, sizeof(buf))) {
		Con_Printf("coop: the answer %s to the instance\n", yes ? "yes" : "no");
	}
}

static void Cp_MenuYes(cAbstractFile *params) {
	(void)params;
	Cp_ClientAnswer(1);
}

static void Cp_MenuNo(cAbstractFile *params) {
	(void)params;
	Cp_ClientAnswer(0);
}

/* a library of the level's scripts imported here: a mission's the client's own level start did not load (its story
 * is the instance's), as the level start preloads it */
static byte Cp_ClientImport(const char *mod) {
	char dir[COORD_PATH], pyc[CP_NAME + 8];
	nativeVal_t path;
	if (Mode_Import(mod)) {
		return 1;
	}
	Native_ErrClear();
	if (!g_pApp->levelPath) {
		return 0;
	}
	Str_PrintfQuiet(dir, sizeof(dir), "%s/Scripts", g_pApp->levelPath);
	Str_PrintfQuiet(pyc, sizeof(pyc), "%s.pyc", mod);
	Native_SetStr(&path, dir);
	byte ok = Mode_Pack(&path) && PORT_CALL(Scrap, PreloadLibrary, NULL, "ss", mod, pyc);
	Native_Free(&path);
	Mode_Pack(NULL);
	return ok && Mode_Import(mod);
}

/* the conversor's interface of the instance: the member's class and the classes the story has, as the single
 * player's machine opens it */
static void Cp_ClientConvOpen(NetRepReader *r) {
	static char avl[COORD_STORY_VALUE + 1];
	char cls[CP_NAME];
	nativeVal_t text, list;
	NetRep_GetStr(r, cls, sizeof(cls));
	int n = (int)NetRep_GetU16(r);
	if (r->bad || n > COORD_STORY_VALUE) {
		return;
	}
	CoordMsg_GetBytes(r, avl, n);
	avl[n] = 0;
	if (r->bad) {
		return;
	}
	SaveVar_Set("Char", cls);
	SaveVar_Set("Conversor.AvailableChars", avl);
	Native_SetStr(&text, avl);
	Native_SetNone(&list);
	if (!Mode_Import("CharConversor") || !Native_EvalText("CharConversor", &text, &list) ||
		!Mode_SetModVar("CharConversor", "AvailableChars", &list)) {
		Native_ErrPrint();
	}
	Native_Free(&text);
	Native_Free(&list);
	cp.cmenu = COOP_MENU_CONV;
	cp.cmenuAt = Watch_ShownTime();
	if (!PORT_CALL(CharConversor, AddInterfaceContent, NULL, "i", 0L)) {
		Native_ErrPrint();
	}
	Con_Printf("coop: the conversor's interface of the instance (as %s)\n", cls);
}

byte Coop_ClientConverted(const char *cls) {
	uint8_t buf[8 + CP_NAME];
	NetRepBuf w;
	if (!g_Play || cp.cmenu != COOP_MENU_CONV) {
		return 0;
	}
	cp.cmenu = 0;
	if (!PORT_CALL(Menu, BackToGame, NULL, "is", 0L, "")) {
		Native_ErrPrint();
	}
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_C_ANSWER);
	NetRep_PutU8(&w, COOP_MENU_CONV);
	NetRep_PutStr(&w, cls);
	if (!w.bad && Watch_SendReliable(buf, w.size)) {
		Con_Printf("coop: the conversor's pick %s to the instance\n", cls[0] ? cls : "(none)");
	}
	return 1;
}

/* a menu of the instance's scripts for this member */
static void Cp_ClientMenu(NetRepReader *r) {
	static cPy_NativeFunc natives[2];
	static char title[CP_MENU_TITLE + 1];
	char via[CP_NAME], mod[CP_NAME];
	int kind = (int)NetRep_GetU8(r);
	if (!natives[0].name) {
		cPy_NativeFunc_ctor(&natives[0], "CoopMenuYes", Cp_MenuYes);
		cPy_NativeFunc_ctor(&natives[1], "CoopMenuNo", Cp_MenuNo);
	}
	if (kind == COOP_MENU_YESNO) {
		int n = (int)NetRep_GetU16(r);
		if (n > CP_MENU_TITLE) {
			return;
		}
		CoordMsg_GetBytes(r, title, n);
		title[n] = 0;
		if (r->bad || !Mode_Import("Menu")) {
			return;
		}
		/* as the scripts open it in the single player, the answers to the instance */
		if (!PORT_CALL(Menu, InitMenuSys, NULL, "i", 0L) ||
			!PORT_CALL(Menu, YesNoMenu, NULL, "isss", 0L, title, "!CoopMenuYes", "!CoopMenuNo")) {
			Native_ErrPrint();
			return;
		}
		Script_SetNum(0, "Cursor", "Visible", 0);
		Script_SetDefault(0, "Yes");
		cp.cmenu = COOP_MENU_YESNO;
		cp.cmenuAt = Watch_ShownTime();
		Con_Printf("coop: the instance asks: %s\n", title);
		return;
	}
	if (kind == COOP_MENU_CONV) {
		Cp_ClientConvOpen(r);
		return;
	}
	if (kind == COOP_MENU_WARNING) {
		int n = (int)NetRep_GetU16(r);
		if (n > CP_MENU_TITLE) {
			return;
		}
		CoordMsg_GetBytes(r, title, n);
		title[n] = 0;
		Coop_ClientKeys(title, (int)sizeof(title));
		/* the box of the single player (it waits until the member plays: its entry calls it again) */
		if (!r->bad && (!Mode_Import("Menu") || !PORT_CALL(Menu, ShowWarning, NULL, "is", 0L, title))) {
			Native_ErrPrint();
		}
		cp.cmenu = COOP_MENU_WARNING;
		cp.cmenuAt = Watch_ShownTime();
		Con_Printf("coop: a text box of the instance: %.60s\n", title);
		return;
	}
	if (kind == COOP_MENU_SPUTNIK) {
		int m = (int)NetRep_GetU32(r);
		if (r->bad) {
			return;
		}
		Cp_GetVars(r, 1);
		g_pWorld->money = m;
		cp.cmenu = COOP_MENU_SPUTNIK;
		cp.cmenuAt = Watch_ShownTime();
		if (!Mode_Import("SputnikInterface") || !PORT_CALL(SputnikInterface, EnterSputnikInterface, NULL, "i", 0L)) {
			Native_ErrPrint();
		}
		Con_Printf("coop: Sputnik's hangar screen of the instance, money %d\n", m);
		return;
	}
	if (kind != COOP_MENU_SHIPEDIT) {
		return;
	}
	NetRep_GetStr(r, via, sizeof(via));
	int money = (int)NetRep_GetU32(r);
	if (r->bad) {
		return;
	}
	Cp_GetVars(r, 1);
	g_pWorld->money = money;
	if (cp.testEdit) {
		/* test: the hangar's spare ship is the member's (SCRAP_COOP_SHIPEDIT) */
		SaveVar_Set(CP_HANGAR "HangarShipAux", CP_LIFEBOAT);
		SaveVar_Set(CP_HANGAR "iHangarShip", "9");
	}
	/* the ship of the client's own level start goes (CreatePlayerShip keeps the old one on the platform) */
	nativeVal_t none;
	Native_SetNone(&none);
	if (!Mode_Import("HangarTab") || !Mode_SetModVar("HangarTab", "prevShip", &none)) {
		Native_ErrPrint();
	}
	/* the editor of the hangar the instance has: the ship on the platform again, then the function of the answer
	 * that opened it there (its tutorial), else the keypad's */
	const char *dot = strchr(via, '.');
	byte ok = Junkyard_PlatformShip(), done = 0;
	if (ok && dot && dot - via < CP_NAME) {
		Str_PrintfQuiet(mod, sizeof(mod), "%.*s", (int)(dot - via), via);
		done = Cp_ClientImport(mod) && Native_CallMod(mod, dot + 1, NULL, "is", 0L, "");
		/* else the plain editor at least */
		Native_ErrPrint();
	}
	if (ok && !done) {
		ok = Mode_Import("MapShipEdit") && Native_CallMod("MapShipEdit", "EnterEditShip", NULL, "ssss", "", "", "", "");
	}
	if (!ok) {
		Native_ErrPrint();
	}
	cp.cmenu = COOP_MENU_SHIPEDIT;
	cp.cmenuAt = Watch_ShownTime();
	Con_Printf("coop: the ship editor of the instance%s%s, money %d\n", via[0] ? " with " : "", via, money);
}

byte Coop_ClientKeeps(cEntity *e) {
	return cp.cmenu == COOP_MENU_SHIPEDIT && Entity_IsType(e, TYEN_CAR);
}

/* the money and the hangar to the instance: the menu of kind closed here */
static void Cp_ClientHangarBack(int kind) {
	static uint8_t buf[CP_MENU_MSG];
	NetRepBuf w;
	if (!g_Play || cp.cmenu != kind) {
		return;
	}
	cp.cmenu = 0;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_C_ANSWER);
	NetRep_PutU8(&w, (uint32_t)kind);
	NetRep_PutU32(&w, (uint32_t)g_pWorld->money);
	Cp_PutVars(&w, 0);
	if (!w.bad && Watch_SendReliable(buf, w.size)) {
		Con_Printf("coop: the hangar to the instance (ship %s, money %d)\n", SaveVar_Get(CP_HANGAR "iHangarShip") ?
			SaveVar_Get(CP_HANGAR "iHangarShip") : "-", g_pWorld->money);
	}
}

void Coop_ClientShipEdited(void) {
	Cp_ClientHangarBack(COOP_MENU_SHIPEDIT);
}

void Coop_ClientSputnikDone(void) {
	Cp_ClientHangarBack(COOP_MENU_SPUTNIK);
}

static void Cp_ClientOptions(NetRepReader *r) {
	char ent[CP_NAME];
	NetRep_GetStr(r, ent, sizeof(ent));
	int cur = (int)NetRep_GetU8(r), n = (int)NetRep_GetU8(r);
	if (n > COOP_OPTS) {
		return;
	}
	for (int k = 0; k < n; k++) {
		NetRep_GetStr(r, cp.copt.opt[k], COOP_OPT_TEXT);
	}
	if (r->bad) {
		return;
	}
	Str_Copy(cp.copt.ent, ent, sizeof(cp.copt.ent));
	cp.coop = 1;
	cp.copt.n = n;
	cp.copt.cur = cur < n ? cur : 0;
	if (n) {
		Con_Printf("coop: %d options of %s, %d: %s\n", n, ent, cp.copt.cur, cp.copt.opt[cp.copt.cur]);
	}
}

/* another option of the instance's list: shown here and told to the instance */
static void Cp_ClientChoose(int k) {
	uint8_t buf[CP_NAME + 8];
	NetRepBuf w;
	cp.copt.cur = k;
	NetRepBuf_Init(&w, buf, sizeof(buf));
	NetRep_PutU8(&w, COOP_MSG);
	NetRep_PutU8(&w, COOP_C_CHOICE);
	NetRep_PutStr(&w, cp.copt.ent);
	NetRep_PutU8(&w, (uint32_t)k);
	if (!w.bad && Watch_SendReliable(buf, w.size)) {
		Con_Printf("coop: option %d of %s chosen: %s\n", k, cp.copt.ent, cp.copt.opt[k]);
	}
}

const char *Coop_ClientOption(int *count) {
	if (!Coop_Client() || !cp.copt.n || cp.clientScene[0]) {
		return NULL;
	}
	*count = cp.copt.n;
	return cp.copt.opt[cp.copt.cur];
}

/* the keys that pick another option of the instance's list (the talk panel shows it) */
static void Cp_ClientOptionsFrame(void) {
	static float prevNext, prevPrev;
	int n = cp.copt.n;
	if (!n || cp.clientScene[0]) {
		return;
	}
	float next = Input_BoundValue(ACTSET_WALK, WALK_NEXT_ACTION, 0), prev = Input_BoundValue(ACTSET_WALK, WALK_PREV_ACTION, 0);
	if (n > 1 && next > 0.5f && prevNext <= 0.5f) {
		Cp_ClientChoose((cp.copt.cur + 1) % n);
	} else if (n > 1 && prev > 0.5f && prevPrev <= 0.5f) {
		Cp_ClientChoose((cp.copt.cur + n - 1) % n);
	}
	prevNext = next;
	prevPrev = prev;
}

/* the key that takes this member to its party's leader */
static void Cp_ClientLeaderKey(void) {
	static byte was;
	byte down = Input_BoundValue(ACTSET_WALK, WALK_COOP_LEADER, 0) > 0.5f;
	if (down && !was && cp.coop) {
		uint8_t buf[2] = { COOP_MSG, COOP_C_LEADER };
		if (Watch_SendReliable(buf, sizeof(buf))) {
			Con_Printf("coop: to the party's leader\n");
			Texts_Format(cp.notice, sizeof(cp.notice), "MP_CoopToLeader", "");
			Text_Message(COOP_NOTICE_MS, "%s", cp.notice);
		}
	}
	was = down;
}

void Coop_ClientSceneAbort(void) {
	cp.aborting = 1;
}

void Coop_ClientSceneEnded(void) {
	byte esc = cp.aborting;
	cp.aborting = 0;
	if (!cp.clientScene[0] || cp.skipped) {
		return;
	}
	/* the scene plays on for the others; this member gets its character back with the instance's end */
	cp.skipped = 1;
	if (!esc) {
		Con_Printf("coop: scene %s ended here before the instance's end\n", cp.clientScene);
		return;
	}
	Con_Printf("coop: scene %s skipped here, it plays on in the instance\n", cp.clientScene);
	Texts_Format(cp.notice, sizeof(cp.notice), "MP_CoopSceneSkipped", "");
	Text_Message(COOP_NOTICE_MS, "%s", cp.notice);
}

/* the instance's mission items of the HUD on this client's HUD (made again when its HUD was drawn anew); the items
 * it made that the instance no longer has go */
static void Cp_ClientScorer(void) {
	char names[COOP_SCORER_ITEMS][CP_NAME], name[CP_NAME], type[16], var[CP_NAME], val[256];
	cScorer *sc = Scorer_Get(0);
	if (!sc || cp.cscoreSize < 2) {
		return;
	}
	NetRepReader r;
	NetRepReader_Init(&r, cp.cscore + 2, cp.cscoreSize - 2);
	for (int k = 0; cp_timerVars[k]; k++) {
		nativeVal_t t;
		NetRep_GetStr(&r, val, sizeof(val));
		if (!r.bad && val[0]) {
			if (k == 4) {
				Native_SetInt(&t, atol(val));
			} else {
				Native_SetFloat(&t, atof(val));
			}
			if (!Native_GVarSet(cp_timerVars[k], &t)) {
				Native_ErrPrint();
			}
		}
	}
	int n = (int)NetRep_GetU8(&r), m = 0;
	for (int j = 0; j < n && j < COOP_SCORER_ITEMS && !r.bad; j++) {
		NetRep_GetStr(&r, name, sizeof(name));
		NetRep_GetStr(&r, type, sizeof(type));
		int nv = (int)NetRep_GetU8(&r);
		cScorerItem *it = r.bad ? NULL : cScorer_FindItem(sc, name);
		if (!r.bad && !it) {
			it = cScorer_AddItem(sc, name, type, 1);
		}
		for (int k = 0; k < nv && !r.bad; k++) {
			NetRep_GetStr(&r, var, sizeof(var));
			int kind = (int)NetRep_GetU8(&r);
			NetRep_GetStr(&r, val, sizeof(val));
			char *str = val;
			double d = atof(val);
			int i = atoi(val);
			if (it && !r.bad) {
				ScorerItem_Var(it, var, 1, kind == 1 ? &str : NULL, kind == 2 ? &d : NULL, kind == 3 ? &i : NULL);
			}
		}
		if (it) {
			Str_Copy(names[m++], name, CP_NAME);
		}
	}
	for (int k = 0; k < cp.ncnames; k++) {
		byte keep = 0;
		for (int j = 0; j < m && !keep; j++) {
			keep = !strcmp(names[j], cp.cnames[k]);
		}
		cScorerItem *it = keep ? NULL : cScorer_FindItem(sc, cp.cnames[k]);
		if (it) {
			it->delMe = 1;
		}
	}
	if (m != cp.ncnames) {
		Con_Printf("coop: the instance's mission items on the HUD here: %d%s%s\n", m, m ? ", " : "", m ? names[0] : "");
	}
	memcpy(cp.cnames, names, sizeof(names[0]) * (size_t)m);
	cp.ncnames = m;
}

/* a variable of the client's copy of the story (value NULL: deleted) */
static void Cp_ClientStorySet(const char *name, const char *value) {
	if (!Cp_ClientStoryVar(name)) {
		return;
	}
	if (value) {
		SaveVar_Set(name, value);
	} else {
		SaveVar_Delete(name);
	}
}

void Coop_ClientStory(const char *text, int len) {
	static char key[CP_NAME], val[COORD_STORY_VALUE + 1];
	int count = 0;
	/* the client's own variables (a single player game before) are no part of it */
	SaveVars_Delete("");
	for (int pos = 0; CoordKv_Next(text, len, &pos, key, sizeof(key), val, sizeof(val)); count++) {
		Cp_ClientStorySet(key, val);
	}
	Con_Printf("coop: the party's story here, %d variables\n", count);
}

/* the client's HUD was drawn anew (a level, a ship): the instance's items again */
static void Cp_ClientScorerFrame(void) {
	cScorer *sc = Scorer_Get(0);
	for (int k = 0; sc && k < cp.ncnames; k++) {
		if (!cScorer_FindItem(sc, cp.cnames[k])) {
			Cp_ClientScorer();
			return;
		}
	}
}

/* the entities the instance's level start deleted go from the client's level too (its own ones, not the proxies of
 * the instance's) */
static void Cp_ClientGone(void) {
	char name[CP_NAME];
	NetRepReader r;
	NetRepReader_Init(&r, cp.cgone + 2, cp.cgoneSize - 2);
	cp.cgoneSize = 0;
	int n = (int)NetRep_GetU16(&r), del = 0;
	for (int k = 0; k < n && !r.bad; k++) {
		NetRep_GetStr(&r, name, sizeof(name));
		cEntity *e = r.bad ? NULL : World_FindEntity(name);
		if (e && e != Players_Local() && !(Entity_IsType(e, TYEN_OBJECT) && Obj(e)->proxy) && cEntity_Del(e)) {
			del++;
		}
	}
	Con_Printf("coop: %d of the %d entities the instance's level start deleted were here\n", del, n);
}

void Coop_ClientFrame(void) {
	static double skipAfter = -1.0, editSecs, answerSecs, convSecs, sputnikSecs, warnSecs;
	static int choose = -2, answer;
	static const char *chooseOf = "";
	static char conv[CP_NAME];
	static int convVisits;
	if (skipAfter < 0.0) {
		const char *s = getenv("SCRAP_COOP_SKIP"), *c = getenv("SCRAP_COOP_CHOOSE"), *a = getenv("SCRAP_COOP_ANSWER");
		const char *e = getenv("SCRAP_COOP_SHIPEDIT");
		answer = !a ? -1 : !crt_strncmp(a, "yes", 3) ? 1 : !crt_strncmp(a, "no", 2) ? 0 : -1;
		answerSecs = a && strchr(a, ':') ? atof(strchr(a, ':') + 1) : CP_ANSWER_SECS;
		editSecs = e ? atof(e) : 0.0;
		skipAfter = s ? atof(s) : 0.0;
		choose = c ? atoi(c) : -1;
		chooseOf = c && strchr(c, ':') ? strchr(c, ':') + 1 : "";
		const char *cv = getenv("SCRAP_COOP_CONV"), *sp = getenv("SCRAP_COOP_SPUTNIK");
		Str_PrintfQuiet(conv, sizeof(conv), "%.*s", cv ? (int)strcspn(cv, ":") : 0, cv ? cv : "");
		convSecs = cv && strchr(cv, ':') ? atof(strchr(cv, ':') + 1) : CP_CONV_SECS;
		sputnikSecs = sp ? atof(sp) : 0.0;
		const char *wn = getenv("SCRAP_COOP_WARNING");
		warnSecs = wn ? atof(wn) : 0.0;
	}
	if (cp.pendingSize) {
		NetRepReader r;
		NetRepReader_Init(&r, cp.pending + 2, cp.pendingSize - 2);
		cp.pendingSize = 0;
		Cp_ClientScene(&r);
	}
	if (cp.cgoneSize) {
		Cp_ClientGone();
	}
	Cp_ClientOptionsFrame();
	Cp_ClientScorerFrame();
	Cp_ClientLeaderKey();
	/* test (SCRAP_COOP_CHOOSE=<k>[:<entity prefix>]): option k of each list of more options than k */
	if (choose >= 0 && cp.copt.n > choose && cp.copt.cur != choose && !cp.clientScene[0] &&
		!strncmp(cp.copt.ent, chooseOf, strlen(chooseOf))) {
		Cp_ClientChoose(choose);
	}
	/* tests: a question's answer, the ship editor closed with the hangar's spare ship */
	if (answer >= 0 && cp.cmenu == COOP_MENU_YESNO && Watch_ShownTime() - cp.cmenuAt >= answerSecs) {
		Cp_ClientAnswer((byte)answer);
	}
	cp.testEdit = editSecs > 0.0;
	if (editSecs > 0.0 && cp.cmenu == COOP_MENU_SHIPEDIT && Watch_ShownTime() - cp.cmenuAt >= editSecs) {
		Con_Printf("coop: test: the ship editor closes\n");
		if (!PORT_CALL(ShipEdit, ExitEditShipMenu, NULL, "is", 0L, "Exit")) {
			Native_ErrPrint();
		}
		if (cp.cmenu == COOP_MENU_SHIPEDIT) {
			Con_Printf("coop: test: the ship editor did not close\n");
			cp.cmenu = 0;
		}
	}
	/* tests: the conversor's pick (as its button's Convert and ConvertExecute), Sputnik's screen closed */
	if (conv[0] && cp.cmenu == COOP_MENU_CONV && Watch_ShownTime() - cp.cmenuAt >= convSecs) {
		/* a list "A,B,...": each visit the next class of it, the last one again after its end */
		char k[CP_NAME + 8], pick[CP_NAME];
		const char *at = conv;
		for (int n = 0; n < convVisits && strchr(at, ','); n++) {
			at = strchr(at, ',') + 1;
		}
		Str_PrintfQuiet(pick, sizeof(pick), "%.*s", (int)strcspn(at, ","), at);
		convVisits++;
		nativeVal_t v;
		Con_Printf("coop: test: the conversor's pick %s\n", pick);
		Str_PrintfQuiet(k, sizeof(k), "Char_%s", pick);
		Native_SetStr(&v, k);
		byte ok = strcmp(pick, "none") ? Mode_SetModVar("CharConversor", "KONTROL", &v) &&
			PORT_CALL(CharConversor, ConvertExecute, NULL, "") : PORT_CALL(CharConversor, ExitCharConversor, NULL, "is", 0L, "");
		if (!ok) {
			Native_ErrPrint();
		}
		Native_Free(&v);
		if (cp.cmenu == COOP_MENU_CONV) {
			Con_Printf("coop: test: %s is no class of the conversor here\n", pick);
			Coop_ClientConverted("");
		}
	}
	if (sputnikSecs > 0.0 && cp.cmenu == COOP_MENU_SPUTNIK && Watch_ShownTime() - cp.cmenuAt >= sputnikSecs) {
		Con_Printf("coop: test: Sputnik's screen closes\n");
		if (!PORT_CALL(SputnikInterface, ExitSputnikInterface, NULL, "is", 0L, "Exit")) {
			Native_ErrPrint();
		}
		if (cp.cmenu == COOP_MENU_SPUTNIK) {
			Coop_ClientSputnikDone();
		}
	}
	if (warnSecs > 0.0 && cp.cmenu == COOP_MENU_WARNING && Watch_ShownTime() - cp.cmenuAt >= warnSecs) {
		Con_Printf("coop: test: the text box closes\n");
		cp.cmenu = 0;
		if (!PORT_CALL(Menu, BackToGame, NULL, "is", 0L, "")) {
			Native_ErrPrint();
		}
	}
	/* test: Escape the given seconds into each scene of the instance */
	if (skipAfter > 0.0 && cp.clientScene[0] && !cp.skipped && Watch_ShownTime() - cp.sceneAt >= skipAfter &&
		(!Mode_Import("Scene") || !PORT_CALL(Scene, AbortScene, NULL, "i", 0L))) {
		Native_ErrPrint();
		cp.skipped = 1;
	}
}

/* the mission's targets of the instance on the client's copies (the level's entity of the name, the proxy of a
 * replicated one), as its MissionsFuncs sets them: the radar marks, the mission arrow */
static void Cp_ClientTargets(NetRepReader *r) {
	char name[CP_NAME], sig[sizeof(cp.ctargets)];
	nativeVal_t list;
	int n = (int)NetRep_GetU8(r), len = 0, got = 0;
	sig[0] = 0;
	Native_SetEmpty(&list, NV_LIST);
	for (int k = 0; k < n && k < COOP_TARGETS && !r->bad; k++) {
		NetRep_GetStr(r, name, sizeof(name));
		int slot = (int)NetRep_GetU16(r);
		cEntity *e = r->bad ? NULL : slot != NETREP_NO_SLOT ? Watch_SlotEntity(slot) : World_FindEntity(name);
		if (!e) {
			continue;
		}
		Native_SetStr(Native_Append(&list), e->name);
		len += Str_PrintfQuiet(sig + len, (int)sizeof(sig) - len, "%s,", e->name);
		len = len < (int)sizeof(sig) ? len : (int)sizeof(sig) - 1;
		got++;
	}
	if (r->bad || !strcmp(sig, cp.ctargets) || !Mode_Import("MissionsFuncs")) {
		Native_Free(&list);
		return;
	}
	Str_Copy(cp.ctargets, sig, sizeof(cp.ctargets));
	byte ok;
	if (got == 1) {
		ok = PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &list.items[0]);
	} else if (got) {
		ok = PORT_CALL(MissionsFuncs, SetMultipleMissionTargets, NULL, "v", &list);
	} else {
		nativeVal_t none;
		Native_SetNone(&none);
		ok = PORT_CALL(MissionsFuncs, SetMissionTarget, NULL, "v", &none);
	}
	if (!ok) {
		Native_ErrPrint();
	}
	Con_Printf("coop: the mission's targets here: %s\n", sig[0] ? sig : "none");
	Native_Free(&list);
}

static void Cp_ClientSceneEnd(NetRepReader *r) {
	char cls[CP_NAME], name[CP_NAME + 16];
	NetRep_GetStr(r, cls, sizeof(cls));
	if (r->bad || strcmp(cls, cp.clientScene)) {
		return;
	}
	cp.clientScene[0] = 0;
	if (cp.skipped) {
		Con_Printf("coop: scene %s ended in the instance\n", cls);
		return;
	}
	Str_PrintfQuiet(name, sizeof(name), "DM_Scene_%s", cls);
	Cp_ClientNoCallback();
	if (World_FindEntity(name) && !PORT_CALL(Scene, EndOfScene, NULL, "ssi", name, "", 0L)) {
		Native_ErrPrint();
	}
}

byte Coop_ClientMessage(const uint8_t *data, int size) {
	static char text[1201];
	char font[64], file[160], key[64], arg[96], line[256];
	NetRepReader r;

	if (size < 2 || data[0] != COOP_MSG) {
		return 0;
	}
	cp.world = 1;
	NetRepReader_Init(&r, data + 2, size - 2);
	switch (data[1]) {
	case COOP_Y_SCENE:
		/* played in the next frame of the running level (a member that arrives gets it before its level runs: its
		 * camera would be taken over by the level's start) */
		if (size <= (int)sizeof(cp.pending)) {
			memcpy(cp.pending, data, (size_t)size);
			cp.pendingSize = size;
		}
		break;
	case COOP_Y_SCENEEND: {
		/* a scene that waits is dropped by its own end only (a callback starts the next scene before the end of the
		 * last one goes out) */
		char cls[CP_NAME], pcls[CP_NAME];
		NetRepReader pr, er = r;
		NetRep_GetStr(&er, cls, sizeof(cls));
		NetRepReader_Init(&pr, cp.pending + 2, cp.pendingSize > 2 ? cp.pendingSize - 2 : 0);
		NetRep_GetStr(&pr, pcls, sizeof(pcls));
		NetRep_GetStr(&pr, pcls, sizeof(pcls));
		if (cp.pendingSize && !pr.bad && !er.bad && !strcmp(cls, pcls)) {
			cp.pendingSize = 0;
		}
		Cp_ClientSceneEnd(&r);
		break;
	}
	case COOP_Y_GONE:
		/* deleted in the next frame of the running level (the level the client loads makes them first) */
		if (size <= (int)sizeof(cp.cgone)) {
			memcpy(cp.cgone, data, (size_t)size);
			cp.cgoneSize = size;
		}
		break;
	case COOP_Y_HEAD: {
		char model[128];
		NetRep_GetStr(&r, model, sizeof(model));
		NetRep_GetStr(&r, text, sizeof(text));
		int index = (int)NetRep_GetU8(&r);
		if (!r.bad) {
			/* the client's head of that name may have fewer animations than the instance's (a mission's AddModel there:
			 * ProtectMayorTransport's Rusty): its talk then */
			const cScorerModel *m = cScorer_FindModel(Scorer_Get(0), model);
			if (m && (index >= m->numAnims || -index >= m->numAnims)) {
				index = index < 0 ? -1 : 1;
			}
			Scorer_SetHeadMonitor(model, text, index);
			Con_Printf("coop: a remote communication of %s\n", text);
		}
		break;
	}
	case COOP_Y_SPEECH:
	case COOP_Y_MSG: {
		int n = (int)NetRep_GetU16(&r);
		if (n >= (int)sizeof(text)) {
			break;
		}
		CoordMsg_GetBytes(&r, text, n);
		text[n] = 0;
		Coop_ClientKeys(text, (int)sizeof(text));
		float t = (float)NetRep_GetU32(&r) / 1000.0f;
		if (data[1] == COOP_Y_MSG) {
			NetRep_GetStr(&r, font, sizeof(font));
			if (!r.bad) {
				Scorer_SetMsgText(text, t, font[0] ? font : NULL);
			}
			break;
		}
		int cr = (int)NetRep_GetU8(&r), cg = (int)NetRep_GetU8(&r), cb = (int)NetRep_GetU8(&r);
		if (!r.bad) {
			Scorer_SetSpeechText(text, t, cr, cg, cb);
		}
		break;
	}
	case COOP_Y_VOICE: {
		int ch = (int)NetRep_GetU8(&r);
		NetRep_GetStr(&r, file, sizeof(file));
		if (!r.bad && g_pSound) {
			SoundSys_OpenVoice(g_pSound, file, ch, 1);
			SoundSys_PlayVoice(g_pSound, ch);
		}
		break;
	}
	case COOP_Y_OPTS:
		Cp_ClientOptions(&r);
		break;
	case COOP_Y_MENU:
		Cp_ClientMenu(&r);
		break;
	case COOP_Y_FADE: {
		uint32_t c[8];
		for (int k = 0; k < 8; k++) {
			c[k] = NetRep_GetU8(&r);
		}
		float t = (float)NetRep_GetU32(&r) / 1000.0f;
		cScorer *sc = Scorer_Get(0);
		if (!r.bad && sc) {
			ScorerFade_Start(&sc->fade, Scorer_PackColor(c[0], c[1], c[2], c[3]), Scorer_PackColor(c[4], c[5], c[6], c[7]), t);
		}
		break;
	}
	case COOP_Y_VIDEO:
		NetRep_GetStr(&r, file, sizeof(file));
		if (!r.bad) {
			Con_Printf("coop: the video %s before the next level\n", file);
			Cp_ClientVideo(file);
		}
		break;
	case COOP_Y_STORY: {
		static char name[CP_NAME], val[COORD_STORY_VALUE + 1];
		int count = (int)NetRep_GetU16(&r), kind;
		for (int i = 0; i < count && CoordStory_Next(&r, &kind, name, sizeof(name), val, sizeof(val)) > 0; i++) {
			Cp_ClientStorySet(name, kind == COORD_OP_DEL ? NULL : val);
		}
		break;
	}
	case COOP_Y_SCORER:
		if (size <= (int)sizeof(cp.cscore)) {
			memcpy(cp.cscore, data, (size_t)size);
			cp.cscoreSize = size;
			Cp_ClientScorer();
		}
		break;
	case COOP_Y_HUDVAR:
		NetRep_GetStr(&r, key, sizeof(key));
		NetRep_GetStr(&r, arg, sizeof(arg));
		if (!r.bad && Cp_InList(cp_hud, key)) {
			SaveVar_Set(key, arg);
			if (!Mode_GVarSetI(key, atoi(arg))) {
				Native_ErrPrint();
			}
		}
		break;
	case COOP_Y_TARGETS:
		Cp_ClientTargets(&r);
		break;
	case COOP_Y_END:
		/* the credits of the main menu, as the single player's end; the world is left */
		Con_Printf("coop: the story is finished: the credits\n");
		if (!Mode_GVarSetI("ShowCredits", 1)) {
			Native_ErrPrint();
		}
		/* not here in the middle of the instance's messages: the join ends in its next pass */
		cp.ended = 1;
		break;
	case COOP_Y_NOTICE:
		NetRep_GetStr(&r, key, sizeof(key));
		NetRep_GetStr(&r, arg, sizeof(arg));
		if (!r.bad) {
			Texts_Format(line, sizeof(line), key, Cp_NoticeArg(key, arg));
			Con_Printf("coop: %s\n", line);
			Text_Message(COOP_NOTICE_MS, "%s", line);
		}
		break;
	}
	return 1;
}

byte Coop_ClientEnded(void) {
	byte e = cp.ended;
	cp.ended = 0;
	return e;
}

void Coop_ClientReset(byte left) {
	cp.world = cp.world && !left;
	cp.cmenu = 0;
	cp.clientScene[0] = 0;
	cp.pendingSize = 0;
	cp.coop = 0;
	cp.copt.n = 0;
	cp.skipped = 0;
	cp.ctargets[0] = 0;
	cp.cscoreSize = 0;
	cp.ncnames = 0;
}
