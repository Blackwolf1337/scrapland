/*
 * One UI transaction channel of the second multiplayer design (stage 4.4, -mp2; src/game/App/UiChan.c). Not in the
 * original.
 *
 * A UI module (tag UI of tools/script/netclass/module.tsv: the menus, the questions and text boxes, the conversor,
 * the metro map, Sputnik's hangar screen, the ship editor, the mission pages) runs on the client of the player who
 * opened it, with the C ports single player uses. The instance never builds one on its own process: where single
 * player opens the interface, the instance sends an open to the client of the player in whose context it runs
 * (include/scrap/players.h), with the values the interface reads (the player's save variables, its money). What the
 * interface changes in the world (an answer, a class picked in the conversor, a station, the ship flown, the
 * hangar and money of the ship editor) comes back as one transaction; the instance checks it against its rules (the
 * open it sent, the machine still the player's, the class available, the money there, the parts priced as single
 * player prices them) and applies it in that player's context with single player's own functions, or refuses it with
 * single player's message on the player's HUD and changes nothing.
 *
 * Machines: a conversor in use is the player's (Players_SetMachine): its action's events run in that player's
 * context, a second player's use of it is refused. The world's clock never stops for one player: the menus' time
 * stop is per player on the instance (Players_SetTimeSpeed) and none on a client, a conversor waiting for its pick
 * stands alone.
 *
 * The phone cabs and the news panels have no interface of their own: their use is the player's use input, which
 * runs in its context on the instance already; the news head of the monitor goes to the player's HUD (UI_S_NEWS).
 *
 * Wire (reliable, REP_CH_RELIABLE), both ways UI_MSG then a kind u8:
 *   instance -> client  UI_S_OPEN    ui u8, seq u16, then the fields of the ui (see UiChan.c)
 *                       UI_S_REFUSE  ui u8, seq u16, text u16 length + bytes (shown as single player shows it)
 *                       UI_S_CLOSE   ui u8, seq u16 (the instance took the interface back: its machine went)
 *                       UI_S_NEWS    on u8 (the news head of the monitor)
 *   client -> instance  UI_C_DONE    ui u8, seq u16, then the fields of the transaction
 *                       UI_C_ASK     ui u8 (an interface the client opens itself and the instance fills: the mission
 *                                    pages)
 *
 * Audit (include/scrap/netaudit.h, category NA_HUD): "ui.open:<ui>", "ui.done:<ui>", "ui.refuse:<ui>:<why>",
 * "ui.world:<ui>" (an interface opened in the world's context: its player resolved), on the client
 * "ui.client.open:<ui>", "ui.client.done:<ui>". Test verb "ui" (TestChannel.c): the open interfaces, the counts, the
 * player's values ("ui state [player]"), a call of the scripts in a player's context ("ui call <player>
 * Module.Func|arg|...", the call table line format of include/scrap/scriptmodes.h).
 */
#ifndef SCRAP_UICHAN_H
#define SCRAP_UICHAN_H

#include "scrap/shared.h"

#define UI_MSG 'I'

enum {
	UI_S_OPEN = 1,
	UI_S_REFUSE,
	UI_S_CLOSE,
	UI_S_NEWS
};

enum {
	UI_C_DONE = 1,
	UI_C_ASK
};

/* the interfaces */
enum {
	UI_K_YESNO = 1,					/* Menu.YesNoMenu: title, the yes and no functions (instance side only) */
	UI_K_TEXTBOX,					/* Menu.ShowWarning: the text */
	UI_K_CONV,						/* the conversor: the player's class, the classes it can pick */
	UI_K_METRO,						/* the metro map: the door, the stations, the station of the mission */
	UI_K_SPUTNIK,					/* Sputnik's hangar screen: money, the hangar */
	UI_K_SHIPEDIT,					/* the ship editor: the function that opened it, money, the hangar */
	UI_K_MISSION,					/* the mission pages of the menu: the story's variables they show */
	UI_K_COUNT
};

struct cEntity;
struct Net2Host;
struct tcBuf;

/* the name of an interface ("conv") */
const char *Ui_Name(int ui);

/* ---- instance ---- */

/* a -mp2 instance: its interfaces open on the clients, the menu system's own state (Menu.InitMenuSys, BackToGame:
 * the action set, the scorer, the time) is the client's */
byte Ui_Instance(void);

/* the interface opens on the client of the current player instead (each returns 1 when it went there, 0 when the
 * caller builds it as single player does: not a -mp2 instance) */
byte Ui_YesNo(const char *title, const char *yes, const char *no);
byte Ui_TextBox(const char *text);
byte Ui_Conv(void);
byte Ui_Metro(const char *usr, const char *door);
byte Ui_Sputnik(void);
byte Ui_ShipEdit(void);
/* the news head of the monitor on the current player's HUD (NewsPanel.UseNewsPanel, EndOfNews) */
void Ui_NewsHead(int on);
/* a use of the machine named by the current player on a -mp2 instance: 1 when another player holds it (the use is
 * refused with single player's message for a machine that cannot be used, msgKey a language text), else the
 * machine is the player's from now (released when its interface ends) */
byte Ui_MachineBusy(const char *machine, const char *msgKey);
/* the machine of the current player is free again (its flow ended) */
void Ui_MachineFree(void);
/* a transaction of a client */
byte Ui_Message(struct Net2Host *host, int conn, const uint8_t *data, int size);
/* player i joined or left: its interface and machine go */
void Ui_PlayerGone(int i);

/* ---- client ---- */

/* a message of the instance */
void Ui_ClientMessage(const uint8_t *data, int size);
/* the client shows an interface of kind ui of the instance (0: any) */
byte Ui_ClientIn(int ui);
/* the money of the client is the one of the interface shown (Sputnik's screen, the ship editor spend it): the
 * instance's status of the player does not overwrite it until the transaction */
byte Ui_ClientMoney(void);
/* the replica e is the client's own while its interface shows: the ship editor's platform plays the editor's
 * animations here, not the instance's state (Watch.c leaves it alone) */
byte Ui_ClientKeeps(const struct cEntity *e);
/* the conversor's pick on the client (cls "": Escape): to the instance, the interface closes; 0 when no interface of
 * the instance is open (single player goes on) */
byte Ui_ClientConv(const char *cls);
/* Sputnik's screen or the ship editor closed on the client: the hangar and money to the instance; 0 when no such
 * interface of the instance is open */
byte Ui_ClientHangar(int ui);
/* the instance gave the player the action set (include/scrap/pres.h): after the conversor of the instance closed here,
 * the walk or racer controls back also build the client's HUD skeleton (the end of the machine's scene) */
void Ui_ClientControls(const char *set);
/* the mission pages open on the client: the story's values asked of the instance (1 when asked) */
byte Ui_ClientAsk(int ui);

/* ---- both ---- */

void Ui_Status(struct tcBuf *b, const char *arg);

#endif
