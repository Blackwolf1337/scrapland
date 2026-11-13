/*
 * The pages of the worlds of the new network in the multiplayer menu (src/game/Script/MenuWorld.c, sections of
 * data/menus/net.def), not in the original. The old multiplayer pages get a "Free roam" button; its pages find worlds
 * (Net2 LAN discovery, the list server of include/scrap/coordlist.h, an address typed in), join one and host one
 * (include/scrap/travel.h: Join_Start, Join_StartHost). Their buttons call native functions ("!WorldJoinMenu", ...)
 * registered at the first multiplayer page, so the PCMenu module keeps the names of the original.
 *
 * Options of scrapdecomp.ini ([general]): worldport= the port a world is hosted on and the first port LAN discovery
 * asks (default NET2_DEFAULT_PORT), lanaddr= a numeric address the LAN queries go to instead of the broadcast (tests
 * on loopback), listserver= and listport= the list server (default none: nothing is asked or registered),
 * hostbind= the address a hosted world binds (default any).
 */
#ifndef SCRAP_WORLDMENU_H
#define SCRAP_WORLDMENU_H

#include "scrap/shared.h"

/* ---- the parts of MenuNet.c the pages share ---- */

/* a line of the server browser in its fixed columns (ping a text: "?" before an answer) */
void Net_BrowserLine(const char *name, const char *type, byte forceShip, int players, int maxPlayers, const char *map,
	const char *ping, char *out, int size);
/* the browser's lines BrowserSlot1.. from entry first of total (line(i) the text of entry i), the slider; tab the item
 * above the list ("" or NULL none) */
void Net_FillBrowser(int id, int total, int first, void (*line)(int i, char *out, int size), const char *tab);
/* the first entry shown after an arrow (how 0 down, 1 up) or a page key (2, 3); -1 when it stays */
int Net_ScrollIndex(int count, int index, int how);
/* a section of net.def with the values every page may use (tab, world: the page, motd: the status line) and n more */
void Net_RunWorld(int id, const char *section, const char *tab, const char *world, const char *motd, int n,
	const char *const *names, const struct nativeVal_s *const *vals);

/* ---- MenuWorld.c ---- */

/* the native functions of the pages, once */
void WorldMenu_Register(void);
/* each frame: the browser's network (answers of LAN discovery, pings, the list server) and the state of a join or
 * host started here (a failure shows its text; a world that ended shows why once the main menu is up again) */
void WorldMenu_Frame(void);

#endif
